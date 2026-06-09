# Android Binder flat_binder_object 对象转换机制

## 目录

1. [概述](#一概述)
2. [flat_binder_object 数据结构](#二flat_binder_object-数据结构)
3. [Binder 对象类型](#三binder-对象类型)
4. [对象转换流程](#四对象转换流程)
5. [BINDER_TYPE_BINDER 转换](#五binder_type_binder-转换)
6. [BINDER_TYPE_HANDLE 转换](#六binder_type_handle-转换)
7. [BINDER_TYPE_FD 转换](#七binder_type_fd-转换)
8. [完整示例](#八完整示例)
9. [总结](#九总结)

---

## 一、概述

当 Binder 事务穿越进程边界时，用户空间传递的 Binder 对象（或句柄、文件描述符）不能直接使用 —— 因为不同进程的虚拟地址空间相互隔离。

内核驱动中的 `binder_transaction()` 函数会遍历事务数据中的**偏移数组**，定位每一个 `flat_binder_object`，并根据其类型执行**句柄↔节点** 或 **文件描述符** 的转换。

```
进程 A（Client）                    进程 B（Server）
   │                                    │
   │  flat_binder_object.type           │
   │  = BINDER_TYPE_BINDER              │
   │  （本地 Binder 对象）               │
   │         │                          │
   │         ▼                          │
   │   内核转换：                        │
   │   binder_node → binder_ref         │
   │   （节点 → 远程句柄）               │
   │                                    │
   │         │                          │
   │         ▼                          │
   │  flat_binder_object.type           │
   │  = BINDER_TYPE_HANDLE              │
   │  （远程句柄）                       │
```

---

## 二、flat_binder_object 数据结构

这是用户空间与内核交换 Binder 对象的**基本单元**，出现在 Parcel 数据中。

```c
struct flat_binder_object {
    struct binder_object_header {
        __u32 type;     // 对象类型
    } hdr;

    __u32 flags;        // 标志位（0x80 = 文件描述符，其他 = 引用计数优先级）

    union {
        binder_uintptr_t binder;    // BINDER_TYPE_BINDER: 本地 Binder 对象地址
        __u32 handle;               // BINDER_TYPE_HANDLE: 远程句柄
    };

    binder_uintptr_t cookie;        // 附加数据（通常指向 BBinder 对象）
};
```

---

## 三、Binder 对象类型

| 类型常量 | 值 | 说明 |
|---------|-----|------|
| `BINDER_TYPE_BINDER` | `1` | **本地 Binder 对象**。发送方传的是自己进程内的 `BBinder*` 地址 |
| `BINDER_TYPE_WEAK_BINDER` | `2` | 弱引用本地 Binder 对象（同 BINDER，但只增弱引用） |
| `BINDER_TYPE_HANDLE` | `3` | **远程句柄**。发送方传的是从 ServiceManager 或其他途径获得的 handle |
| `BINDER_TYPE_WEAK_HANDLE` | `4` | 弱引用远程句柄 |
| `BINDER_TYPE_FD` | `5` | **文件描述符**。在目标进程中复制 FD |
| `BINDER_TYPE_FDA` | `6` | 文件描述符数组 |

---

## 四、对象转换流程

### 4.1 总体流程

```
binder_transaction() 中：
    │
    ├── 1. 拷贝数据到目标进程 mmap 缓冲区
    │
    ├── 2. 遍历偏移数组 offsets[]
    │      │
    │      ├── 对每个 offset:
    │      │    ├── fbo = buffer->data + offset
    │      │    │
    │      │    ├── switch (fbo->hdr.type):
    │      │    │    │
    │      │    │    ├── BINDER_TYPE_BINDER
    │      │    │    │    ├── 查找发送方的 binder_node（按 ptr 查找）
    │      │    │    │    ├── 在目标进程创建/查找 binder_ref
    │      │    │    │    ├── 增加引用计数
    │      │    │    │    └── fbo->hdr.type = BINDER_TYPE_HANDLE
    │      │    │    │        fbo->handle = ref->desc
    │      │    │    │
    │      │    │    ├── BINDER_TYPE_HANDLE
    │      │    │    │    ├── 查找发送方的 binder_ref（按 handle 查找）
    │      │    │    │    ├── 在目标进程创建/查找新的 binder_ref
    │      │    │    │    ├── 增加引用计数
    │      │    │    │    └── fbo->handle = new_ref->desc
    │      │    │    │
    │      │    │    └── BINDER_TYPE_FD
    │      │    │         ├── 在目标进程分配新的 FD
    │      │    │         ├── 执行 get_unused_fd_flags()
    │      │    │         └── fbo->handle = new_fd
    │      │    │
    │      │    └── 继续下一个 offset
    │      │
    │      └── 所有对象转换完成
    │
    └── 继续事务处理
```

---

## 五、BINDER_TYPE_BINDER 转换

这是**最常用的场景**：进程 A 将自己的一个本地 Binder 对象传给进程 B。

### 5.1 场景

```
进程 A（Server）                内核                       进程 B（Client）
   │                            │                            │
   │  创建 BBinder 对象         │                            │
   │  (位于用户空间地址 X)       │                            │
   │                            │                            │
   │── BC_TRANSACTION ─────────►│                            │
   │  包含 fbo:                 │                            │
   │  type=BINDER_TYPE_BINDER   │                            │
   │  binder=addr_X             │                            │
   │  cookie=cookie_X            │                            │
   │                            │                            │
   │                            │  1. proc->nodes 红黑树查找 │
   │                            │     是否存在 ptr=addr_X   │
   │                            │     的 binder_node         │
   │                            │     未找到 → 创建新 node   │
   │                            │                            │
   │                            │  2. target_proc 中查找     │
   │                            │     是否已有引用此 node    │
   │                            │     的 binder_ref          │
   │                            │     未找到 → 创建新 ref    │
   │                            │     desc = 分配新句柄号    │
   │                            │                            │
   │                            │  3. 增加弱引用计数          │
   │                            │                            │
   │                            │  4. 原地修改 fbo:          │
   │                            │     type → BINDER_TYPE_HANDLE │
   │                            │     handle → ref->desc     │
   │                            │     cookie → 不变          │
   │                            │                            │
   │                            ├───────────────────────────►│
   │                            │                            │
   │                            │                            │── BR_TRANSACTION
   │                            │                            │   fbo:
   │                            │                            │   type=BINDER_TYPE_HANDLE
   │                            │                            │   handle=ref->desc
   │                            │                            │   cookie=cookie_X
```

### 5.2 内核源码逻辑

```c
// binder_transaction() 中的 BINDER_TYPE_BINDER 处理
case BINDER_TYPE_BINDER:
case BINDER_TYPE_WEAK_BINDER:
{
    struct binder_ref *ref;
    struct binder_node *node;

    // ── 1. 在发送方进程中查找 binder_node ──
    //     按 ptr（用户空间 Binder 对象地址）在 proc->nodes 红黑树中查找
    node = binder_get_node(proc, fbo->binder);

    if (!node) {
        // ── 2. 未找到 → 创建新的 binder_node ──
        node = binder_new_node(proc, fbo->binder, fbo->cookie);
        if (IS_ERR(node)) {
            return_error = BR_FAILED_REPLY;
            goto err;
        }
    }

    // ── 3. 在目标进程中查找或创建 binder_ref ──
    //     确保目标进程已有一个引用指向这个 node
    if (fbo->hdr.type == BINDER_TYPE_BINDER)
        ref = binder_get_ref_for_node(target_proc, node, &ref_off);
    else
        ref = binder_get_ref_for_node_weak(target_proc, node, &ref_off);

    if (!ref) {
        return_error = BR_FAILED_REPLY;
        goto err;
    }

    // ── 4. 原地修改 fbo：类型改为 HANDLE，handle 替换为句柄号 ──
    if (fbo->hdr.type == BINDER_TYPE_BINDER)
        fbo->hdr.type = BINDER_TYPE_HANDLE;
    else
        fbo->hdr.type = BINDER_TYPE_WEAK_HANDLE;

    fbo->flags = ref->data.desc | (fbo->flags & ~FLAT_BINDER_FLAG_HANDLE_MASK);
    fbo->binder = 0;  // 清空本地地址（在目标进程中无效）

    // ── 5. 增加引用计数 ──
    //     防止 node 在目标进程使用期间被销毁
    if (fbo->hdr.type == BINDER_TYPE_HANDLE)
        ref->data.strong++;
    else
        ref->data.weak++;

    break;
}
```

---

## 六、BINDER_TYPE_HANDLE 转换

场景：进程 B 持有一个指向进程 C 的句柄，把它传给进程 A。

### 6.1 场景

```
进程 A                        内核                       进程 B
   │                            │                            │
   │                            │                            │── 持有 handle=5
   │                            │                            │   指向 proc C 的 node
   │                            │                            │
   │                            │◄── BC_TRANSACTION ─────────│
   │                            │   fbo:                     │
   │                            │   type=BINDER_TYPE_HANDLE  │
   │                            │   handle=5                 │
   │                            │                            │
   │                            │  1. proc->refs_by_desc 查找│
   │                            │     ref（handle=5）         │
   │                            │     → 找到 node（指向 proc C）│
   │                            │                            │
   │                            │  2. 目标进程（proc A）中   │
   │                            │     创建/查找新的 ref       │
   │                            │     指向同一个 node         │
   │                            │     desc = 新句柄号        │
   │                            │                            │
   │                            │  3. 原地修改 fbo:          │
   │                            │     handle → 新句柄号      │
   │                            │                            │
   │                            ├──────────────────────────►│
   │                            │                            │
   │  BR_TRANSACTION            │                            │
   │  fbo:                      │                            │
   │  type=BINDER_TYPE_HANDLE   │                            │
   │  handle=新句柄号            │                            │
```

### 6.2 内核源码逻辑

```c
// binder_transaction() 中的 BINDER_TYPE_HANDLE 处理
case BINDER_TYPE_HANDLE:
case BINDER_TYPE_WEAK_HANDLE:
{
    struct binder_ref *ref;

    // ── 1. 在发送方进程的 refs_by_desc 红黑树中 ──
    //     按 handle 查找 binder_ref
    ref = binder_get_ref(proc, fbo->handle);

    if (!ref) {
        // 句柄无效
        return_error = BR_FAILED_REPLY;
        goto err;
    }

    // ── 2. 不允许进程将句柄传回给自己（形成环）
    if (ref->node->proc == target_proc) {
        // 目标进程就是这个 node 的所有者
        // → 将 HANDLE 转换回 BINDER
        if (fbo->hdr.type == BINDER_TYPE_HANDLE)
            fbo->hdr.type = BINDER_TYPE_BINDER;
        else
            fbo->hdr.type = BINDER_TYPE_WEAK_BINDER;

        fbo->binder = ref->node->ptr;
        fbo->cookie = ref->node->cookie;

        // 增加 node 引用计数
        if (fbo->hdr.type == BINDER_TYPE_BINDER)
            ref->node->local_strong_refs++;
        else
            ref->node->local_weak_refs++;

    } else {
        // ── 3. 在目标进程中创建新的引用 ──
        struct binder_ref *new_ref;

        new_ref = binder_get_ref_for_node(target_proc, ref->node, &ref_off);
        if (!new_ref) {
            return_error = BR_FAILED_REPLY;
            goto err;
        }

        // ── 4. 修改 fbo：句柄号替换为目标的句柄 ──
        fbo->binder = 0;
        fbo->handle = new_ref->data.desc;

        // 增加引用计数
        if (fbo->hdr.type == BINDER_TYPE_HANDLE)
            new_ref->data.strong++;
        else
            new_ref->data.weak++;
    }

    break;
}
```

### 6.3 环回转换的典型场景

```
进程 A                       进程 B                        进程 C
   │                            │                            │
   │── 持有 handle=1 ──────────►│                            │
   │   (指向 proc B 的 service) │                            │
   │                            │── 持有 handle=5 ──────────►│
   │                            │   (指向 proc C 的 service) │
   │                            │                            │
   │◄── 调用 B 的方法 ──────────│                            │
   │                            │                            │
   │   B 把 handle=5 传给 A     │                            │
   │   A 得到 handle=7          │                            │
   │   (指向 proc C 的 node)    │                            │
   │                            │                            │
   ─────────────────────────────────────────────────────────
   如果 B 把 proc C 的 node 传回给 C 自己：
   fbo->type = BINDER_TYPE_HANDLE → BINDER_TYPE_BINDER
   因为 target_proc == node->proc
```

---

## 七、BINDER_TYPE_FD 转换

### 7.1 场景

进程 A 将自己的一个文件描述符（如打开的文件、Socket）传给进程 B。

```
进程 A                        内核                       进程 B
   │                            │                            │
   │  打开文件 fd=7             │                            │
   │                            │                            │
   │── BC_TRANSACTION ─────────►│                            │
   │  fbo:                      │                            │
   │  type=BINDER_TYPE_FD       │                            │
   │  handle=7                  │                            │
   │                            │                            │
   │                            │  1. 获取 proc A 的         │
   │                            │     files_struct           │
   │                            │  2. 找到 fd=7 对应的       │
   │                            │     struct file*           │
   │                            │  3. get_unused_fd() 在     │
   │                            │     目标进程分配新 fd      │
   │                            │  4. fd_install() 安装      │
   │                            │     (增加 struct file 引用)│
   │                            │  5. fbo->handle = 新 fd    │
   │                            │                            │
   │                            ├───────────────────────────►│
   │                            │                            │
   │                            │                            │── BR_TRANSACTION
   │                            │                            │   fbo.handle = 新 fd
   │                            │                            │   （现在进程 B 可使用此 fd）
```

### 7.2 内核源码逻辑

```c
// binder_transaction() 中的 BINDER_TYPE_FD 处理
case BINDER_TYPE_FD:
{
    int target_fd;
    struct file *file;

    // ── 1. 从发送方进程获取 struct file ──
    file = fget(proc->files, fbo->handle);
    if (!file) {
        return_error = BR_FAILED_REPLY;
        goto err;
    }

    // ── 2. 在目标进程中分配新的文件描述符编号 ──
    target_fd = task_get_unused_fd_flags(target_proc, O_CLOEXEC);
    if (target_fd < 0) {
        fput(file);
        return_error = BR_FAILED_REPLY;
        goto err;
    }

    // ── 3. 在目标进程的 FD 表中安装 ──
    task_fd_install(target_proc, target_fd, file);

    // ── 4. 修改 fbo：handle 替换为目标进程的 fd 号 ──
    fbo->handle = target_fd;
    fbo->flags = 0;

    break;
}
```

### 7.3 FD 传递的特性

| 特性 | 说明 |
|------|------|
| **自动复制** | 内核在目标进程创建新的 FD，指向同一个内核 `struct file` |
| **引用计数** | `struct file` 引用计数 +1 |
| **O_CLOEXEC** | 自动设置 close-on-exec，防止子进程泄漏 FD |
| **FD 号可能不同** | 发送方 fd=7，接收方可能变成 fd=32 |
| **生命周期** | 两者关闭后，`struct file` 才会被释放 |

---

## 八、完整示例

### 8.1 Binder 对象从创建到跨进程传递全过程

```
1. 进程 A 创建 Binder 服务
   ┌────────────────────────────┐
   │ BBinder* obj = new MyBinder; │
   │ obj 位于用户空间地址 0x7f... │
   └────────────────────────────┘
          │
          ▼
2. 内核记录 node（一次 Binder 创建）
   ┌────────────────────────────┐
   │ binder_node {              │
   │   ptr = 0x7f...,           │
   │   cookie = obj,            │
   │   proc = proc_A,           │
   │   refs = [ref_1, ...]      │
   │ }                          │
   └────────────────────────────┘
          │
          ▼
3. 其他进程通过 ServiceManager 获得句柄 handle=3
   ┌────────────────────────────┐
   │ binder_ref {               │
   │   desc = 3,                │
   │   node → binder_node,      │
   │   strong = 1,              │
   │   proc = proc_B            │
   │ }                          │
   └────────────────────────────┘
          │
          ▼
4. 进程 B 把 handle=3 传给进程 C
   ┌───────────────────────────────────────────────┐
   │ 事务数据中：                                   │
   │ flat_binder_object {                          │
   │   type = BINDER_TYPE_HANDLE,                  │
   │   handle = 3                                  │
   │ }                                             │
   │                                               │
   │ 内核转换后：                                   │
   │ flat_binder_object {                          │
   │   type = BINDER_TYPE_HANDLE,                  │
   │   handle = 5       ← 在 proc_C 中的新句柄     │
   │ }                                             │
   └───────────────────────────────────────────────┘
          │
          ▼
5. 进程 C 调用 handle=5 → 最终到达 proc_A 的 MyBinder
   进程 C              内核                   进程 A
    │                   │                       │
    │ BC_TRANSACTION    │                       │
    │ target.handle=5   │                       │
    │──────────────────►│                       │
    │                   │ refs_by_desc 查找     │
    │                   │ → ref(desc=5)         │
    │                   │ → ref->node           │
    │                   │ → node->proc (proc_A) │
    │                   │──────────────────────►│
    │                   │                       │── BR_TRANSACTION
    │                   │                       │── cookie → BBinder*
    │                   │                       │── onTransact()
    │                   │◄──────────────────────│
    │◄──────────────────│                       │
    │ BR_REPLY          │                       │
```

### 8.2 引用计数变化

```
初始：
  node->local_strong_refs = 1    (node 所在进程的本地引用)
  node->refs = []

进程 B 通过 ServiceManager 获得句柄：
  binder_ref(proc=B, desc=3, strong=1)
  node->local_strong_refs = 1    (不变，这是 proc A 的)
  node->refs = [ref_B]
  ref_B->data.strong = 1          (进程 B 持有 1 个强引用)

进程 B 将句柄传给进程 C：
  binder_ref(proc=C, desc=5, strong=1)
  node->refs = [ref_B, ref_C]
  ref_C->data.strong = 1

进程 B 释放句柄：
  ref_B->data.strong = 0
  如果弱引用也为 0 → 删除 ref_B

最后一个引用释放：
  node->local_strong_refs = 0 且 node->refs 为空
  → 发送 BR_DEAD_BINDER 通知
  → 可以销毁 node
```

---

## 九、总结

```
flat_binder_object 转换 = Binder 跨进程传递的核心
           │
           ├── BINDER_TYPE_BINDER ← 发送方传本地对象
           │      ↓ 内核转换
           │   BINDER_TYPE_HANDLE ← 接收方拿到远程句柄
           │
           ├── BINDER_TYPE_HANDLE ← 发送方传远程句柄
           │      ├── 目标进程 == node 所有者?
           │      │    └── 是 → 转回 BINDER_TYPE_BINDER（环回）
           │      └── 否 → 创建新 ref，新句柄号
           │
           └── BINDER_TYPE_FD ← 发送方传文件描述符
                  ↓
              在目标进程分配新 FD，指向同一个 struct file
```

| 转换方向 | 关键函数 | 结果 |
|----------|---------|------|
| BINDER → HANDLE | `binder_get_node()` + `binder_get_ref_for_node()` | 本地对象 → 远程句柄 |
| HANDLE → HANDLE | `binder_get_ref()` + `binder_get_ref_for_node()` | 句柄号重新映射 |
| HANDLE → BINDER | 环回检测 `target_proc == node->proc` | 远程句柄 → 本地对象 |
| FD → FD | `fget()` + `get_unused_fd()` + `fd_install()` | FD 跨进程复制 |

**文件**：`drivers/android/binder.c` — `binder_transaction()`
