# Android Binder binder_ioctl_set_ctx_mgr 与 binder_vma 数据结构

## 目录

1. [binder_ioctl_set_ctx_mgr](#一binder_ioctl_set_ctx_mgr)
2. [binder_vma 数据结构](#二binder_vma-虚拟内存区域数据结构)

---

# 一、binder_ioctl_set_ctx_mgr

## 1.1 概述

`binder_ioctl_set_ctx_mgr` 是 Binder 驱动中处理 `BINDER_SET_CONTEXT_MGR` ioctl 命令的函数。它将当前进程注册为 **Context Manager**（即 ServiceManager），在内核中创建一个特殊的 `binder_node`，其句柄固定为 **0**。

## 1.2 函数源码

```c
// drivers/android/binder.c

static int binder_ioctl_set_ctx_mgr(struct file *filp,
                                    struct binder_proc *proc)
{
    struct binder_context *context = proc->context;
    struct binder_node *node;
    int ret = 0;

    // ── 1. 安全检查：是否为特权进程 ──
    //     只有 CAP_SYS_ADMIN 或有 root 权限的进程可以成为 Context Manager
    //     在 Android 中，init 启动 ServiceManager 时自动满足
    if (!capable(CAP_SYS_ADMIN))
        return -EPERM;

    // ── 2. 互斥访问 ──
    mutex_lock(&context->context_mgr_node_lock);

    // ── 3. 唯一性检查：只能有一个 Context Manager ──
    if (context->binder_context_mgr_node) {
        pr_err("BINDER_SET_CONTEXT_MGR already set\n");
        ret = -EBUSY;
        goto out;
    }

    // ── 4. 创建特殊的 binder_node ──
    //     ptr = 0：表示这是 Context Manager 节点
    //     cookie = 0：无额外数据
    node = binder_new_node(proc, 0, 0);
    if (!node) {
        ret = -ENOMEM;
        goto out;
    }

    // ── 5. 设置引用计数（确保永不释放） ──
    node->local_strong_refs++;
    node->local_weak_refs++;
    node->has_strong_ref = 1;
    node->has_weak_ref = 1;

    // ── 6. 保存为 context_mgr_node ──
    context->binder_context_mgr_node = node;

    // ── 7. 增加节点所在进程的引用 ──
    //     防止 ServiceManager 关闭 Binder fd 时 node 被清理
    node->proc = proc;
    node->debug_id = atomic_inc_return(&binder_last_id);

out:
    mutex_unlock(&context->context_mgr_node_lock);
    return ret;
}
```

## 1.3 调用路径

```
ServiceManager 进程启动
   │
   ├── binder_open("/dev/binder")
   │    └── 创建 binder_proc
   │
   ├── mmap() → binder_mmap()
   │    └── 分配缓冲区
   │
   ├── ioctl(fd, BINDER_SET_CONTEXT_MGR, 0)
   │    └── binder_ioctl()
   │         └── binder_ioctl_set_ctx_mgr()
   │              ├── 检查 CAP_SYS_ADMIN
   │              ├── 创建 binder_node(ptr=0, cookie=0)
   │              ├── 设置 local_strong_refs = 1
   │              ├── 设置 local_weak_refs = 1
   │              └── context->binder_context_mgr_node = node
   │
   └── binder_loop() ← 进入事件循环
```

## 1.4 多 Binder 上下文

Android 8.0+（Treble）每个 Binder 设备都有自己的 Context Manager：

```c
// 每个 binder_context 有自己的 context_mgr_node
struct binder_context {
    // ...
    struct binder_node *binder_context_mgr_node;
    struct mutex context_mgr_node_lock;

    bool binder_context_mgr_uevent;  // 是否发送 uevent
};

// 不同设备的上下文独立
// /dev/binder   → context = "binder"
// /dev/hwbinder → context = "hwbinder"
// /dev/vndbinder → context = "vndbinder"
```

## 1.5 handle=0 的查找机制

```c
// 当其他进程用 handle=0 调用时，内核的查找路径

static struct binder_ref *binder_get_ref_olocked(struct binder_proc *proc,
                                                  u32 desc)
{
    struct rb_node *p = proc->refs_by_desc.rb_node;

    // ── 在 refs_by_desc 红黑树中查找 ──
    while (p) {
        struct binder_ref *ref;
        ref = rb_entry(p, struct binder_ref, rb_node_desc);

        if (desc < ref->data.desc)
            p = p->rb_left;
        else if (desc > ref->data.desc)
            p = p->rb_right;
        else
            return ref;
    }

    // ── 未找到 → 特殊处理 desc=0 ──
    if (desc == 0) {
        // 自动为 handle=0 创建引用
        // 指向 context_mgr_node
        struct binder_node *node = proc->context->binder_context_mgr_node;
        if (node) {
            return binder_get_ref_for_node(proc, node, NULL);
        }
    }

    return NULL;
}
```

---

# 二、binder_vma（Virtual Memory Area）数据结构

## 2.1 概述

`binder_vma` 不是独立的 Binder 数据结构，而是在 `binder_mmap` 中使用的内核 `struct vm_area_struct`。它描述了用户空间 mmap 映射的**虚拟内存区域**，是 Binder 一次拷贝优化的基础。

## 2.2 struct vm_area_struct

```c
// include/linux/mm_types.h

struct vm_area_struct {
    // ── VMA 地址范围 ──
    unsigned long vm_start;       // 用户空间起始地址（含）
    unsigned long vm_end;         // 用户空间结束地址（不含）

    // ── 所属进程的地址空间 ──
    struct mm_struct *vm_mm;      // 所属进程的内存描述符

    // ── 红黑树节点（用于在进程地址空间中快速查找） ──
    struct rb_node vm_rb;

    // ── 链表节点（进程所有 VMA 按地址排序） ──
    struct list_head anon_vma_chain;

    // ── 页表操作函数集（关键！） ──
    const struct vm_operations_struct *vm_ops;

    // ── 映射偏移（用于文件映射） ──
    unsigned long vm_pgoff;       // 文件偏移（页为单位）

    // ── 标志位 ──
    unsigned long vm_flags;       // VM_READ, VM_WRITE, VM_SHARED 等

    // ── 文件信息（如果是文件映射） ──
    struct file *vm_file;         // 对应的文件（Binder 中是 /dev/binder）

    // ── 匿名映射信息 ──
    struct anon_vma *anon_vma;

    // ── NUMA 信息 ──
    struct mempolicy *vm_policy;
};
```

## 2.3 Binder 中的 VMA 使用

### binder_mmap 中的 VMA 操作

```c
static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct binder_proc *proc = filp->private_data;

    // ── 1. 保存 VMA 信息到 binder_proc ──
    proc->vma_addr = vma->vm_start;  // 记录用户空间起始地址

    // ── 2. 限制映射大小（最大 4MB） ──
    if ((vma->vm_end - vma->vm_start) > SZ_4M)
        vma->vm_end = vma->vm_start + SZ_4M;

    // ── 3. 只能使用 MAP_SHARED ──
    //     保证所有进程可以看到同一个映射的更新
    if (!(vma->vm_flags & VM_SHARED))
        return -EINVAL;

    // ── 4. 保存 VMA 大小 ──
    proc->buffer_size = vma->vm_end - vma->vm_start;

    // ── 5. 分配内核缓冲区 ──
    proc->buffer = kzalloc(proc->buffer_size, GFP_KERNEL);
    if (!proc->buffer)
        return -ENOMEM;

    // ── 6. 关键：使用 remap_vmalloc_range 建立映射 ──
    //     将内核分配的物理内存映射到用户空间 VMA
    ret = remap_vmalloc_range(vma, proc->buffer, vma->vm_pgoff);

    // ── 7. 设置 VMA 操作函数 ──
    //     用于自定义页面错误处理
    vma->vm_ops = &binder_vm_ops;
    vma->vm_private_data = proc;

    // ── 8. 初始化缓冲区管理 ──
    // ...
}
```

### binder_vm_ops — VMA 操作函数集

```c
// drivers/android/binder_alloc.c

// Binder 自定义的 VMA 操作
static const struct vm_operations_struct binder_vm_ops = {
    // ── 关闭 VMA 时调用 ──
    .close = binder_vma_close,

    // ── 页错误处理 ──
    // （Binder 通常不注册 fault 处理，
    //   因为物理页通过 vm_insert_page 预先插入了）
};
```

## 2.4 binder_vma_close

```c
// drivers/android/binder_alloc.c

static void binder_vma_close(struct vm_area_struct *vma)
{
    struct binder_proc *proc = vma->vm_private_data;

    // ── VMA 关闭时清理 ──
    // 当进程退出或 munmap 时调用

    if (proc) {
        proc->vma_addr = 0;  // 清零 VMA 地址
        // 注意：不在这里释放 buffer
        // buffer 的释放在 binder_release() 中
    }
}
```

## 2.5 VMA 标志位

在 Binder 的 `ProcessState::self()` 中，mmap 使用的标志：

```cpp
// frameworks/native/libs/binder/ProcessState.cpp
mVMStart = mmap(nullptr, BINDER_VM_SIZE,
                PROT_READ,                          // ← 只读
                MAP_PRIVATE | MAP_NORESERVE,         // ← 私有 + 不预分配
                mDriverFD, 0);
```

对应内核 VMA 中的标志：

| VMA 标志 | 值 | 说明 |
|---------|-----|------|
| `VM_READ` | 0x01 | 页面可读 |
| `VM_SHARED` | 0x08 | 多进程共享映射（由内核 `remap_vmalloc_range` 设置） |
| `VM_IO` | 0x10 | 内存映射 I/O 区域 |
| `VM_DONTEXPAND` | 0x4000 | 不可通过 mremap 扩展 |
| `VM_DONTDUMP` | 0x400000 | 不包含在 core dump 中 |
| `VM_PFNMAP` | 0x800 | 物理页号直接映射（非正常页缓存） |

## 2.6 VMA 与数据拷贝的关系

```
用户空间 VMA（进程 B 的地址空间）
┌───────────────────────────────────────┐
│ vm_start = 0x7f8b400000              │
│                                       │
│   mmap 映射的缓冲区                    │
│   ┌─────────────────────────────┐    │
│   │   binder_buffer             │    │
│   │   (直接读取，无需拷贝)       │    │
│   └─────────────────────────────┘    │
│                                       │
│ vm_end = 0x7f8b401000                │
└───────────────────────────────────────┘
        ↑  remap_vmalloc_range 建立映射
        │
内核虚拟地址 (vmalloc 区)
┌───────────────────────────────────────┐
│ proc->buffer                          │
│                                       │
│   ┌─────────────────────────────┐    │
│   │   实际的物理内存             │    │
│   │   (被进程 A 写入数据)       │    │
│   └─────────────────────────────┘    │
└───────────────────────────────────────┘
        ↑
进程 A 写入: copy_from_user(proc_A_data, proc->buffer)
```

## 2.7 VMA 查看方式

```bash
# 查看进程的 VMA 信息
adb shell cat /proc/<pid>/maps | grep binder

# 输出示例：
# 7f8b400000-7f8b800000 r--p 00000000 00:0c 1234    /dev/binder
# │              │      │           │                │
# │              │      │           │                └── 映射文件
# │              │      │           └── 设备号:主:次
# │              │      └── 权限: r-- = 只读
# │              └── 结束地址
# └── 起始地址

# 查看所有 VMA 详情
adb shell cat /proc/<pid>/smaps | grep -A 10 binder
# 7f8b400000-7f8b800000 r--p 00000000 00:0c 1234    /dev/binder
# Size:               4096 kB
# Rss:                 256 kB    ← 实际驻留的物理页
# Pss:                  32 kB    ← 按共享比例算的驻留
# Shared_Clean:        256 kB
# Shared_Dirty:          0 kB
# Private_Clean:         0 kB
# Private_Dirty:         0 kB
# Swap:                  0 kB
```

## 2.8 binder_proc 中与 VMA 相关的字段

```c
// drivers/android/binder_internal.h
struct binder_proc {
    // ...

    // ── VMA 相关信息（在 binder_alloc 中） ──
    struct binder_alloc alloc;

    // ...
};

// drivers/android/binder_alloc.h
struct binder_alloc {
    // ...

    unsigned long vma_addr;       // 用户空间 mmap 起始地址
    struct vm_area_struct *vma;   // 内核 VMA 结构体指针

    // 注意：在新版内核中，vma 指针被移除
    // 只保留 vma_addr，因为 VMA 可能在进程退出时先于 binder 被销毁
    // 保存地址而非指针可以防止悬空引用
};
```

## 2.9 安全设计

```c
// 为防止 VMA 在 binder 使用期间被其它线程修改或销毁，
// binder 使用以下保护机制：

// 1. VMA 关闭时清空地址
static void binder_vma_close(struct vm_area_struct *vma)
{
    // 防止 binder 后续操作访问已释放的 VMA
    ((struct binder_proc *)vma->vm_private_data)->vma_addr = 0;
}

// 2. 使用 VMA 前检查有效性
static int binder_alloc_check_vma(struct binder_alloc *alloc)
{
    // 检查 VMA 地址是否有效
    if (!alloc->vma_addr)
        return -ESRCH;  // 进程已退出

    // 检查 VMA 是否仍然存在
    struct mm_struct *mm = get_task_mm(alloc->vma_proc->tsk);
    if (!mm)
        return -ESRCH;

    // 查找 VMA 并验证地址匹配
    struct vm_area_struct *vma = find_vma(mm, alloc->vma_addr);
    if (!vma || vma->vm_start != alloc->vma_addr) {
        mmput(mm);
        return -ESRCH;
    }

    mmput(mm);
    return 0;
}
```
