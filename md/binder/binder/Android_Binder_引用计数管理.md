# Android Binder 引用计数管理

## 目录

1. [概述](#一概述)
2. [引用计数类型](#二引用计数类型)
3. [用户空间引用管理](#三用户空间引用管理)
4. [内核引用管理](#四内核引用管理)
5. [BC/BR 引用命令](#五bcbr-引用命令)
6. [Binder 对象的生命周期](#六binder-对象的生命周期)
7. [总结](#七总结)

---

## 一、概述

Binder 使用两套引用计数系统来管理 Binder 对象的生命周期：

1. **强引用（Strong Ref）**：决定对象是否存活。强引用 > 0 时对象保持活跃。
2. **弱引用（Weak Ref）**：防止对象被提前回收，允许在强引用归零后通过弱引用重建强引用。

除了用户空间通过 `sp<>` / `wp<>` 智能指针管理外，内核驱动也维护自己的引用计数，以防止在跨进程传递期间对象被错误释放。

---

## 二、引用计数类型

### 2.1 数据结构中的引用计数字段

```c
// binder_node — 服务端导出的 Binder 对象
struct binder_node {
    // ...
    int local_strong_refs;      // 本进程的强引用计数（进程内引用）
    int local_weak_refs;        // 本进程的弱引用计数
    int internal_strong_refs;   // 内核内部强引用计数（跨进程引用合计）
    int internal_weak_refs;     // 内核内部弱引用计数

    // 标记位
    bool has_strong_ref;        // 是否有强引用持有者
    bool has_weak_ref;          // 是否有弱引用持有者
    // ...
};

// binder_ref — 远程引用（句柄）
struct binder_ref_data {
    int debug_id;
    u32 desc;                   // 句柄号
    int strong;                 // 强引用计数（本 ref 的）
    int weak;                   // 弱引用计数（本 ref 的）
};
```

### 2.2 引用类型关系

```
binder_node
   │
   ├── local_strong_refs   ← 本进程（Binder 对象所在进程）内部强引用
   ├── local_weak_refs     ← 本进程内部弱引用
   ├── internal_strong_refs ← 所有远程进程的强引用之和
   ├── internal_weak_refs  ← 所有远程进程的弱引用之和
   │
   └── refs → binder_ref(proc=B)
                ├── data.strong  ← 进程 B 持有的强引用计数
                ├── data.weak    ← 进程 B 持有的弱引用计数
                └── death        ← 死亡通知

                binder_ref(proc=C)
                ├── data.strong
                ├── data.weak
                └── death
```

---

## 三、用户空间引用管理

### 3.1 Android 智能指针

```cpp
// frameworks/native/libs/utils/RefBase.h

template <typename T>
class sp {       // 强指针 → 递增递减 strong ref
    // ...
    void incStrong() { refs->incStrong(); }
    void decStrong() { refs->decStrong(); }
};

template <typename T>
class wp {       // 弱指针 → 递增递减 weak ref
    // ...
    void incWeak() { refs->incWeak(); }
    void decWeak() { refs->decWeak(); }
    sp<T> promote();  // 弱引用提升为强引用（如果对象还活着）
};
```

### 3.2 BpBinder 的引用管理

```cpp
// frameworks/native/libs/binder/BpBinder.cpp

void BpBinder::incStrong(int32_t)
{
    // 第一次增加强引用时，通知内核
    if (atomic_fetch_add_unless(&mAlive, 1, 0) == 0) {
        // 已有引用计数，不需要通知内核
    } else {
        // 第一次增加强引用，通知内核增加强引用计数
        IPCThreadState::self()->incStrongHandle(mHandle);
    }
}

void BpBinder::decStrong(int32_t)
{
    // 强引用归零时通知内核
    if (atomic_fetch_sub(&mAlive, 1) == 1) {
        IPCThreadState::self()->decStrongHandle(mHandle);
    }
}

void BpBinder::incWeak(int32_t)
{
    // 增加弱引用 → 发送 BC_INCREFS
    IPCThreadState::self()->incWeakHandle(mHandle);
}

void BpBinder::decWeak(int32_t)
{
    // 减少弱引用 → 发送 BC_DECREFS
    IPCThreadState::self()->decWeakHandle(mHandle);
}
```

### 3.3 IPCThreadState 的引用操作

```cpp
// IPCThreadState.cpp — 引用计数命令的发送

void IPCThreadState::incStrongHandle(int32_t handle)
{
    mOut.writeInt32(BC_ACQUIRE);       // 增加强引用
    mOut.writeInt32(handle);
}

void IPCThreadState::decStrongHandle(int32_t handle)
{
    mOut.writeInt32(BC_RELEASE);       // 减少强引用
    mOut.writeInt32(handle);
}

void IPCThreadState::incWeakHandle(int32_t handle)
{
    mOut.writeInt32(BC_INCREFS);       // 增加弱引用
    mOut.writeInt32(handle);
}

void IPCThreadState::decWeakHandle(int32_t handle)
{
    mOut.writeInt32(BC_DECREFS);       // 减少弱引用
    mOut.writeInt32(handle);
}
```

---

## 四、内核引用管理

### 4.1 BC_ACQUIRE / BC_RELEASE — 强引用操作

```c
// drivers/android/binder.c — binder_thread_write()

case BC_ACQUIRE:
{
    struct binder_ref *ref;
    u32 target = *(u32 *)(ptr); ptr += sizeof(u32);

    ref = binder_get_ref(proc, target);
    if (!ref) break;

    // ── 增加本 ref 的强引用计数 ──
    ref->data.strong++;

    // ── 更新 node 的 internal_strong_refs ──
    ref->node->internal_strong_refs++;

    // ── 检查是否需要通知 node 进程 ──
    if (ref->node->internal_strong_refs == 1 &&
        ref->node->local_strong_refs == 0) {
        // node 进程需要知道有一个新的强引用
        // 发送 BR_ACQUIRE 给 node 所在进程
    }

    break;
}

case BC_RELEASE:
{
    struct binder_ref *ref;
    u32 target = *(u32 *)(ptr); ptr += sizeof(u32);

    ref = binder_get_ref(proc, target);
    if (!ref) break;

    // ── 减少本 ref 的强引用计数 ──
    ref->data.strong--;

    // ── 更新 node 的 internal_strong_refs ──
    ref->node->internal_strong_refs--;

    // ── 如果 ref 的两个计数都为 0，检查是否需要释放 ──
    if (ref->data.strong == 0 && ref->data.weak == 0) {
        // 如果没有死亡通知，可以删除此 ref
        if (!ref->death) {
            binder_cleanup_ref(ref);
        }
    }

    break;
}
```

### 4.2 BC_INCREFS / BC_DECREFS — 弱引用操作

```c
case BC_INCREFS:
{
    struct binder_ref *ref;
    u32 target = *(u32 *)(ptr); ptr += sizeof(u32);

    ref = binder_get_ref(proc, target);
    if (!ref) break;

    // ── 增加弱引用 ──
    ref->data.weak++;
    ref->node->internal_weak_refs++;

    break;
}

case BC_DECREFS:
{
    struct binder_ref *ref;
    u32 target = *(u32 *)(ptr); ptr += sizeof(u32);

    ref = binder_get_ref(proc, target);
    if (!ref) break;

    // ── 减少弱引用 ──
    ref->data.weak--;
    ref->node->internal_weak_refs--;

    // ── 如果两个计数都为 0，清理 ref ──
    if (ref->data.strong == 0 && ref->data.weak == 0) {
        if (!ref->death) {
            binder_cleanup_ref(ref);
        }
    }

    break;
}
```

### 4.3 node 的引用计数变化逻辑

```c
// 内核根据 node 的引用计数变化决定是否需要通知 node 进程

static void binder_node_inner_lock(struct binder_node *node)
{
    // 当 internal_strong_refs 从 0 → 1 时：
    // 说明有第一个远程强引用
    // 需要确保 node 进程知道有人持有强引用
    if (node->internal_strong_refs == 1 &&
        node->local_strong_refs == 0) {
        node->has_strong_ref = 1;
        // 生成 BR_ACQUIRE 通知 node 进程
    }

    // 当 internal_strong_refs 从 1 → 0 时：
    // 最后一个远程强引用被释放
    if (node->internal_strong_refs == 0) {
        node->has_strong_ref = 0;
        // 生成 BR_RELEASE 通知 node 进程
    }

    // 类似的逻辑用于弱引用
    if (node->internal_weak_refs == 1 &&
        node->local_weak_refs == 0) {
        node->has_weak_ref = 1;
        // 生成 BR_INCREFS
    }

    if (node->internal_weak_refs == 0) {
        node->has_weak_ref = 0;
        // 生成 BR_DECREFS
    }
}
```

### 4.4 ref 的删除条件

```c
static void binder_cleanup_ref(struct binder_ref *ref)
{
    struct binder_node *node = ref->node;

    // ── 1. 从 node 的 refs 链表中移除 ──
    list_del(&ref->node_entry);

    // ── 2. 从 proc 的两个红黑树中移除 ──
    rb_erase(&ref->rb_node_desc, &ref->proc->refs_by_desc);
    rb_erase(&ref->rb_node_node, &ref->proc->refs_by_node);

    // ── 3. 释放死亡通知（如果有） ──
    if (ref->death) {
        // 如果死亡通知已发送但未确认，需要处理
        kfree(ref->death);
    }

    // ── 4. 释放 ref 本身 ──
    kfree(ref);

    // ── 5. 检查 node 是否需要释放 ──
    //     如果 node->refs 为空且没有本地引用，释放 node
}
```

---

## 五、BC/BR 引用命令

### 5.1 命令一览

| 命令 | 方向 | 作用 |
|------|------|------|
| `BC_ACQUIRE` | 用户 → 内核 | 增加远程强引用计数 |
| `BC_RELEASE` | 用户 → 内核 | 减少远程强引用计数 |
| `BC_INCREFS` | 用户 → 内核 | 增加远程弱引用计数 |
| `BC_DECREFS` | 用户 → 内核 | 减少远程弱引用计数 |
| `BC_ACQUIRE_NODE` | 用户 → 内核 | 通过 node 指针增加强引用 |
| `BC_RELEASE_NODE` | 用户 → 内核 | 通过 node 指针减少强引用 |
| `BR_ACQUIRE` | 内核 → 用户 | 通知 node 进程：有远程强引用 |
| `BR_RELEASE` | 内核 → 用户 | 通知 node 进程：远程强引用已释放 |
| `BR_INCREFS` | 内核 → 用户 | 通知 node 进程：有远程弱引用 |
| `BR_DECREFS` | 内核 → 用户 | 通知 node 进程：远程弱引用已释放 |

### 5.2 引用计数变化时序

```
场景：进程 B 获取一个指向进程 A 服务的句柄

进程 A（Server）              内核                    进程 B（Client）
   │                           │                          │
   │                           │── ref(desc=1, strong=0, weak=1)
   │                           │   （ServiceManager 返回句柄时
   │                           │     自动创建 ref，加弱引用）
   │                           │                          │
   │                           │◄── BC_INCREFS ───────────│
   │                           │    (第一次增加弱引用)     │
   │                           │    ref.data.weak++        │
   │                           │    node.internal_weak_refs++ │
   │                           │                          │
   │                           │◄── BC_ACQUIRE ───────────│
   │                           │    (增加强引用)           │
   │                           │    ref.data.strong++      │
   │                           │    node.internal_strong_refs++│
   │                           │                          │
   │◄── BR_ACQUIRE ────────────│                          │
   │   (通知 A：有远程强引用)    │                          │
   │   A 增加本地强引用计数      │                          │
   │                           │                          │
   │                           │                          │── 使用句柄进行 IPC
   │                           │                          │
   │                           │◄── BC_RELEASE ──────────│
   │                           │    (减少强引用)           │
   │                           │    ref.data.strong--     │
   │                           │    node.internal_strong_refs--│
   │                           │                          │
   │◄── BR_RELEASE ────────────│                          │
   │   (通知 A：远程强引用释放)  │                          │
   │   A 减少本地强引用计数      │                          │
   │                           │                          │
   │                           │◄── BC_DECREFS ──────────│
   │                           │    (减少弱引用)           │
   │                           │    ref.data.weak--       │
   │                           │    node.internal_weak_refs--│
   │                           │                          │
   │                           │   strong=0, weak=0 → 删除 ref
```

### 5.3 智能指针与内核计数的对应关系

```
用户空间                       内核
sp<BpBinder> 构造
   │
   ├── incStrong()
   │    └── BC_ACQUIRE     →  ref->data.strong++
   │                          node->internal_strong_refs++
   │
   └── incWeak()
        └── BC_INCREFS     →  ref->data.weak++
                             node->internal_weak_refs++

sp<BpBinder> 析构
   │
   ├── decStrong()
   │    └── BC_RELEASE     →  ref->data.strong--
   │                          node->internal_strong_refs--
   │
   └── decWeak()
        └── BC_DECREFS     →  ref->data.weak--
                             node->internal_weak_refs--
```

---

## 六、Binder 对象的生命周期

### 6.1 完整生命周期

```
1. 进程 A 创建 BBinder 对象
   ┌───────────────────────────────────┐
   │ BBinder* obj = new BBinder();      │
   │ obj 的 sp 强引用计数初始为 1       │
   └───────────────────────────────────┘
          │
2. addService("service", obj)
   │    内核创建 binder_node
   │    node->local_strong_refs = 1     ← obj 所在进程持有
   │    node->local_weak_refs = 1
   │    ServiceManager 获得 handle=1
   │
3. 进程 B getService("service")
   │    内核创建 binder_ref
   │    ref(proc=B, desc=5, strong=1, weak=1)
   │    node->internal_strong_refs++ (=1)
   │    node->internal_weak_refs++ (=1)
   │    内核通知 A: BR_ACQUIRE / BR_INCREFS
   │
4. 进程 B 使用 sp<IBinder> 持有句柄
   │    sp 构造: BC_ACQUIRE + BC_INCREFS
   │    sp 析构: BC_RELEASE + BC_DECREFS
   │    ref 计数在 0~1 之间变化
   │
5. 进程 B 不再使用，sp 全部析构
   │    ref(proc=B, desc=5, strong=0, weak=0)
   │    删除 ref
   │    node->internal_strong_refs-- (=0)
   │    node->internal_weak_refs-- (=0)
   │    内核通知 A: BR_RELEASE / BR_DECREFS
   │
6. 进程 A 销毁 BBinder
   │    node->local_strong_refs = 0
   │    node->local_weak_refs = 0
   │    node 没有 refs 了
   │    释放 node
   │    (如果有死亡通知，发送 BR_DEAD_BINDER)
```

### 6.2 生命周期的保护机制

```
情况 1: 进程 B 持有强引用，进程 A 崩溃
   │
   ├── 进程 A 死亡 → binder_deferred_release()
   ├── node 所在进程已死
   ├── node 仍然存活（因为 ref 还有引用计数）
   ├── 进程 B 仍然可以调用 handle（虽然会失败）
   ├── 进程 B 释放最后一个 ref
   ├── node->internal_strong_refs = 0
   └── 释放 node

情况 2: 进程 B 只有弱引用，进程 A 崩溃
   │
   ├── 进程 A 死亡 → node->proc = NULL
   ├── 进程 B 尝试 wp::promote() → BC_ACQUIRE_NODE
   ├── 内核检查 node->proc == NULL
   ├── 返回 BR_FAILED_REPLY
   └── promote 失败 → BpBinder 已死
```

---

## 七、总结

```
Binder 引用计数 = 双重引用（强引用 + 弱引用）
      │
      ├── 强引用 (Strong Ref)
      │     ├── BC_ACQUIRE / BC_RELEASE
      │     ├── sp<> 管理
      │     ├── ref->data.strong
      │     └── node->internal_strong_refs
      │
      ├── 弱引用 (Weak Ref)
      │     ├── BC_INCREFS / BC_DECREFS
      │     ├── wp<> 管理
      │     ├── ref->data.weak
      │     └── node->internal_weak_refs
      │
      └── 计数规则
            ├── strong > 0: 对象存活，可调用
            ├── strong = 0, weak > 0: 对象可能已死，但结构保留
            ├── strong = 0, weak = 0: 删除 ref
            └── internal_strong 从 0→1: BR_ACQUIRE 通知 node 进程
                internal_strong 从 1→0: BR_RELEASE 通知 node 进程
```

| 场景 | 强引用行为 | 弱引用行为 |
|------|-----------|-----------|
| sp<BpBinder> 构造 | BC_ACQUIRE (+1) | BC_INCREFS (+1) |
| sp<BpBinder> 析构 | BC_RELEASE (-1) | BC_DECREFS (-1) |
| wp<BpBinder> 构造 | 不变 | BC_INCREFS (+1) |
| wp<BpBinder> 析构 | 不变 | BC_DECREFS (-1) |
| wp::promote() 成功 | BC_ACQUIRE (+1) | 不变 |
| wp::promote() 失败 | 不变 | 不变 |
| Binder 对象传给别人 | 不变（内核自动管理） | 不变 |

**文件**：
- `drivers/android/binder.c` — BC_ACQUIRE/BC_RELEASE/BC_INCREFS/BC_DECREFS 处理
- `frameworks/native/libs/binder/BpBinder.cpp`
- `frameworks/native/libs/binder/IPCThreadState.cpp`
- `frameworks/native/libs/utils/RefBase.cpp`
