# Android binder_alloc 内存管理

## 目录

1. [概述](#一概述)
2. [binder_alloc 数据结构](#二binder_alloc-数据结构)
3. [缓冲区分配策略](#三缓冲区分配策略)
4. [缓冲区回收流程](#四缓冲区回收流程)
5. [物理页按需分配](#五物理页按需分配)
6. [异步事务空间管理](#六异步事务空间管理)
7. [调试与观测](#七调试与观测)
8. [总结](#八总结)

---

## 一、概述

`binder_alloc` 是 Binder 驱动中负责 **mmap 缓冲区管理** 的子系统。它管理着每个 `binder_proc` 通过 `binder_mmap` 分配的内存区域，提供缓冲区的分配、释放、物理页按需映射等核心功能。

```
binder_proc
   │
   └── alloc (struct binder_alloc)
        │
        ├── buffer     → mmap 映射的内核虚拟地址起点
        ├── buffers    → 所有 binder_buffer 链表
        ├── free_buffers → 空闲 binder_buffer 红黑树
        ├── pages[]    → 物理页指针数组
        └── free_async_space → 剩余异步事务空间
```

---

## 二、binder_alloc 数据结构

### 2.1 binder_alloc

```c
// drivers/android/binder_alloc.h
struct binder_alloc {
    // mmap 缓冲区信息
    void *buffer;                    // 内核空间虚拟地址（mmap 基址）
    ptrdiff_t buffer_size;           // 缓冲区大小（默认 1MB - 2×page）
    struct list_head buffers;        // 所有 binder_buffer 的链表
    struct rb_root free_buffers;     // 空闲 buffer 的红黑树（按地址排序）
    struct list_head allocated_buffers; // 已分配 buffer 的链表

    // 物理页管理
    struct page **pages;             // 物理页指针数组
    size_t page_count;               // 已分配的物理页数

    // 异步事务空间
    size_t free_async_space;         // 剩余异步事务空间（初始为 buffer_size / 2）
    size_t allocated_async_space;    // 已分配的异步事务空间

    // 统计
    struct dentry *vma_vmfile;       // debugfs 文件
    unsigned long vma_addr;          // 用户空间 mmap 地址

    // 互斥锁
    struct mutex mutex;              // 保护 alloc 的并发访问

    // 所属进程
    struct binder_proc *vma_proc;    // 对应的 binder_proc
};
```

### 2.2 binder_buffer

```c
// drivers/android/binder_types.h
struct binder_buffer {
    struct rb_node rb_node;          // 红黑树节点（挂在 free_buffers 或 allocated_buffers）
    struct list_head entry;          // 链表节点（挂在 proc->buffers）

    unsigned free:1;                 // 1 = 空闲, 0 = 已分配
    unsigned allow_user_free:1;      // 是否允许用户空间通过 BC_FREE_BUFFER 释放
    unsigned async:1;                // 1 = 异步事务缓冲区

    struct binder_transaction *transaction;  // 关联的事务（如果正在使用）
    struct binder_node *target_node; // 目标 Binder 节点

    size_t data_size;                // 数据大小
    size_t offsets_size;             // 偏移数组大小
    size_t extra_buffers_size;       // 额外缓冲区大小

    void *data;                      // 数据起始地址（buffer + header_size）
};
```

### 2.3 内存布局

```
mmap 基址 (proc->alloc.buffer)
   │
   ├── [header]  struct binder_buffer（内核管理用）
   ├── [data]    事务数据（Parcel 内容）
   ├── [offsets] flat_binder_object 偏移数组
   ├── [extra]   额外数据区
   │
   ├── [header]  struct binder_buffer
   ├── [data]
   ├── [offsets]
   │
   └── ... (多个 buffer 动态分配和释放)

缓冲区总大小: 默认 1MB - 2×page
```

---

## 三、缓冲区分配策略

### 3.1 分配总览

`binder_alloc_new_buf()` 是分配缓冲区的入口：

```
binder_alloc_new_buf(alloc, data_size, offsets_size, extra_size)
    │
    ├── 1. 校验大小（不超过 buffer_size / 异步限制）
    │
    ├── 2. 选择最佳空闲块（best-fit）
    │    └── 遍历 free_buffers 红黑树
    │
    ├── 3. 找到合适块后:
    │    ├── 从空闲块中切出所需大小
    │    ├── 剩余部分作为新空闲块重新插入红黑树
    │    └── 标记为已分配
    │
    └── 4. 按需分配物理页
         └── binder_alloc_alloc_buf()
              └── 如果所需页尚未分配 → 分配新页并映射
```

### 3.2 最佳适应（Best-Fit）算法

```c
struct binder_buffer *binder_alloc_new_buf(struct binder_alloc *alloc,
                                           size_t data_size,
                                           size_t offsets_size,
                                           size_t extra_buffers_size)
{
    struct rb_node *p = alloc->free_buffers.rb_node;
    struct binder_buffer *best_fit = NULL;
    struct binder_buffer *buffer;
    size_t size;

    // ── 1. 计算总需求大小 ──
    //     对齐到 sizeof(void*) 边界
    size = ALIGN(data_size, sizeof(void *))
         + ALIGN(offsets_size, sizeof(void *))
         + ALIGN(extra_buffers_size, sizeof(void *));

    // ── 2. 异步事务空间检查 ──
    if (async) {
        if (alloc->free_async_space < size) {
            // 异步空间不足 → 返回 -ENOSPC
            return ERR_PTR(-ENOSPC);
        }
    }

    // ── 3. 在空闲红黑树中查找最佳适配块 ──
    //     红黑树按 buffer 地址排序
    //     遍历找到 size >= 所需大小的最小空闲块
    while (p) {
        buffer = rb_entry(p, struct binder_buffer, rb_node);

        if (buffer->free_space >= size) {
            // 这个块够大
            best_fit = buffer;
            // 尝试找更小的适配块（左子树）
            p = p->rb_left;
        } else {
            // 这个块不够大，找更大的（右子树）
            p = p->rb_right;
        }
    }

    if (!best_fit) {
        // ── 4. 没有找到合适的空闲块 ──
        return ERR_PTR(-ENOSPC);
    }

    // ── 5. 从空闲块中切出一块 ──
    if (best_fit->free_space == size) {
        // 完美匹配：直接移除整个块
        rb_erase(&best_fit->rb_node, &alloc->free_buffers);
        best_fit->free = 0;
        buffer = best_fit;
    } else {
        // 切割：从空闲块头部切出需要的大小
        buffer = (struct binder_buffer *)
                 ((void *)best_fit + best_fit->free_space - size);

        // 初始化新 buffer
        memset(buffer, 0, sizeof(*buffer));
        buffer->free = 0;
        buffer->async = async;
        buffer->data = (void *)buffer + sizeof(*buffer);
        buffer->free_space = 0;

        // 更新原空闲块的大小
        best_fit->free_space -= size;

        // 新 buffer 加入 allocated_buffers
        list_add_tail(&buffer->entry, &alloc->allocated_buffers);
    }

    // ── 6. 更新异步空间统计 ──
    if (async) {
        alloc->free_async_space -= size;
        alloc->allocated_async_space += size;
    }

    return buffer;
}
```

### 3.3 分配示例

```
初始: 一个空闲块, 1MB
┌────────────────────────────────────────────────────────┐
│  free_space = 1MB                                      │
│  [free_buffers]                                        │
└────────────────────────────────────────────────────────┘

分配 100KB:
┌──────────────┬─────────────────────────────────────────┐
│  allocated   │  free_space = 1MB - 100KB               │
│  100KB       │  [free_buffers]                          │
└──────────────┴─────────────────────────────────────────┘

再分配 200KB:
┌──────────────┬──────────────────┬──────────────────────┐
│  allocated   │  allocated       │  free_space = 724KB  │
│  100KB       │  200KB           │  [free_buffers]       │
└──────────────┴──────────────────┴──────────────────────┘

释放中间的 200KB（产生碎片）:
┌──────────────┬──────────────────┬──────────────────────┐
│  allocated   │  FREE 200KB     │  free_space = 724KB  │
│  100KB       │  [free_buffers]  │  [free_buffers]       │
└──────────────┴──────────────────┴──────────────────────┘
              ↑           ↑
          两个独立的空闲块（未能合并）
```

---

## 四、缓冲区回收流程

### 4.1 BC_FREE_BUFFER — 用户空间主动释放

```c
// frameworks/native/libs/binder/Parcel.cpp
void Parcel::freeDataNoInit()
{
    if (mOwner) {
        // 调用 BC_FREE_BUFFER 通知内核释放
        IPCThreadState::self()->freeBuffer(mData, mDataSize, mObjects, mObjectsSize);
    }
    // ...
}
```

```c
// binder_thread_write() 中的处理
case BC_FREE_BUFFER:
{
    binder_uintptr_t data_ptr;

    // ── 1. 读取用户空间传过来的 buffer 地址 ──
    data_ptr = *(binder_uintptr_t *)(ptr); ptr += sizeof(binder_uintptr_t);

    // ── 2. 根据地址找到对应的 binder_buffer ──
    buffer = binder_alloc_prepare_to_free(&proc->alloc, data_ptr);

    if (buffer == NULL) {
        // 无效地址
        break;
    }

    // ── 3. 释放缓冲区 ──
    binder_alloc_free_buf(&proc->alloc, buffer);

    break;
}
```

### 4.2 binder_alloc_free_buf

```c
void binder_alloc_free_buf(struct binder_alloc *alloc,
                           struct binder_buffer *buffer)
{
    // ── 1. 如果是异步事务，归还异步空间 ──
    if (buffer->async) {
        alloc->free_async_space += buffer->free_space +
                                   sizeof(struct binder_buffer);
        alloc->allocated_async_space -= buffer->free_space +
                                        sizeof(struct binder_buffer);
    }

    // ── 2. 标记为空闲 ──
    buffer->free = 1;
    buffer->transaction = NULL;
    buffer->target_node = NULL;

    // ── 3. 从 allocated_buffers 移到 free_buffers ──
    list_del_init(&buffer->entry);

    // ── 4. 尝试合并相邻的空闲块 ──
    //     检查地址相邻的空闲块，合并以消除碎片
    buffer_add_to_free_tree(alloc, buffer);

    // ── 5. 合并 ──
    //     将地址连续的相邻空闲块合并为一个大的空闲块
    buffer_merge_free(alloc, buffer);
}
```

### 4.3 空闲块合并

```c
static void buffer_merge_free(struct binder_alloc *alloc,
                              struct binder_buffer *buffer)
{
    struct rb_node *node;
    struct binder_buffer *prev, *next;

    // ── 查找相邻的空闲块 ──
    node = rb_prev(&buffer->rb_node);
    if (node) {
        prev = rb_entry(node, struct binder_buffer, rb_node);
        if (prev->free &&
            (void *)prev + prev->free_space == (void *)buffer) {
            // ← prev 和 buffer 地址连续 → 合并
            prev->free_space += buffer->free_space;
            rb_erase(&buffer->rb_node, &alloc->free_buffers);
            list_del(&buffer->entry);
            buffer = prev;  // buffer 指针前移
        }
    }

    node = rb_next(&buffer->rb_node);
    if (node) {
        next = rb_entry(node, struct binder_buffer, rb_node);
        if (next->free &&
            (void *)buffer + buffer->free_space == (void *)next) {
            // buffer 和 next 地址连续 → 合并
            buffer->free_space += next->free_space;
            rb_erase(&next->rb_node, &alloc->free_buffers);
            list_del(&next->entry);
        }
    }
}
```

### 4.4 碎片化示意

```
最佳情况（连续释放 → 合并）:
┌─────────────┬─────────────┬─────────────┐
│  allocated  │   FREE      │  allocated  │
│  100KB      │  200KB      │  300KB      │
└─────────────┴─────────────┴─────────────┘
       ↓ 释放右边块后合并
┌─────────────┬───────────────────────────┐
│  allocated  │   FREE (200KB+300KB)      │
│  100KB      │   合并为 500KB            │
└─────────────┴───────────────────────────┘

最差情况（交替分配释放 → 无法合并）:
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ A  │ F  │ A  │ F  │ A  │ F  │ A  │ F  │
│10K │10K │10K │10K │10K │10K │10K │10K │
└────┴────┴────┴────┴────┴────┴────┴────┘
  释放所有 A 后:
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ F  │ F  │ F  │ F  │ F  │ F  │ F  │ F  │  ← 相邻块自动合并
│10K │10K │10K │10K │10K │10K │10K │10K │     为 80KB 整块
└────┴────┴────┴────┴────┴────┴────┴────┘
```

---

## 五、物理页按需分配

### 5.1 物理页惰性分配

Binder 的 mmap 在建立映射时**只分配虚拟地址空间，不分配物理页**。物理页在**实际使用时才按需分配**：

```c
int binder_alloc_alloc_buf(struct binder_alloc *alloc, size_t data_size,
                           size_t offsets_size, size_t extra_buffers_size)
{
    struct binder_buffer *buffer;
    size_t size, total_size, page_needed;
    int ret;

    size = ALIGN(data_size, sizeof(void *))
         + ALIGN(offsets_size, sizeof(void *))
         + ALIGN(extra_buffers_size, sizeof(void *));

    // ── 计算总共需要的内核管理头 + 数据大小 ──
    total_size = size + sizeof(struct binder_buffer);

    // ── 计算需要的物理页数 ──
    page_needed = ALIGN(total_size, PAGE_SIZE) / PAGE_SIZE;

    // ── 如果已有页数不够，分配新页 ──
    if (alloc->page_count < page_needed) {
        ret = binder_alloc_alloc_pages(alloc, page_needed - alloc->page_count);
        if (ret)
            return ret;
    }

    // ... 继续 buffer 分配 ...
}
```

### 5.2 物理页分配

```c
int binder_alloc_alloc_pages(struct binder_alloc *alloc, size_t count)
{
    struct page *page;
    size_t i;

    for (i = 0; i < count; i++) {
        // ── 1. 分配物理页 ──
        page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO);
        if (!page)
            return -ENOMEM;

        // ── 2. 将物理页映射到 mmap 区域 ──
        //     通过 vm_insert_page() 将页插入用户空间页表
        ret = vm_insert_page(alloc->vma,
                            (unsigned long)alloc->buffer +
                            (alloc->page_count + i) * PAGE_SIZE,
                            page);
        if (ret) {
            __free_page(page);
            return ret;
        }

        // ── 3. 记录物理页指针 ──
        alloc->pages[alloc->page_count + i] = page;
    }

    alloc->page_count += count;
    return 0;
}
```

### 5.3 流程示意

```
binder_mmap 时:
虚拟地址空间:  [XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX]  1MB
物理内存:      空（未分配页面）

第一次事务（分配 4KB 数据）:
虚拟地址空间:  [PPPPXXXXXXX...]
物理内存:      [Pg0]

第二次事务（分配 8KB 数据）:
虚拟地址空间:  [PPPPPPPPPPXXXX...]
物理内存:      [Pg0][Pg1][Pg2]

第 N 次事务:
虚拟地址空间:  [PPPPPPPPPPPPPPPPPPP...]
物理内存:      [Pg0][Pg1][Pg2][Pg3][Pg4]...
```

---

## 六、异步事务空间管理

### 6.1 空间限制

异步事务（oneway）有**独立的空间限制**：

- 初始值：`free_async_space = buffer_size / 2`
- 每次分配异步 buffer：`free_async_space -= size`
- 释放时：`free_async_space += size`

```c
// binder_transaction() 中
if (tr->oneway) {
    // 同步事务不受 async 空间限制
    // 但异步事务受 free_async_space 限制
    if (proc->alloc.free_async_space < required_size) {
        // 返回 BR_FAILED_REPLY
        return_error = BR_FAILED_REPLY;
        goto err;
    }
}
```

### 6.2 溢出时序攻击保护

```c
// binder_thread_read() 中
if (proc->outstanding_txns > 0) {
    // 等待异步事务完成，防止异步事务无限堆积
    wait_event(proc->freeze_wait,
               !atomic_read(&proc->outstanding_txns));
}
```

---

## 七、调试与观测

### 7.1 debugfs 查看

```bash
# 查看特定进程的 alloc 状态
adb shell cat /d/binder/proc/<pid>

# proc 1234
# buffer: 0x7f8b4000 - 0x7f8b8000  ← mmap 地址范围
# buffer size: 1020KB               ← 映射大小（1MB - 4KB）
# page count: 256                   ← 已分配的物理页数
# free async space: 524288          ← 剩余异步空间（bytes）

# 每个 buffer 详情
adb shell cat /d/binder/proc/<pid>/buffer
# buffer 0x7f8b4010: size 256     free 0  async 0  transaction 1234
# buffer 0x7f8b4110: size 1024    free 0  async 0  transaction 5678
# buffer 0x7f8b4510: size 4096    free 1  async 0
# buffer 0x7f8b5510: size 2048    free 0  async 1  transaction 9012
```

### 7.2 TransactionTooLargeException

```bash
# 常见错误：事务数据超过 1MB
# binder: 1234: transaction failed for 5678,
#          size 1048576 > 1040384 (buffer_size - header)

# 查看 buffer_size
adb shell cat /d/binder/proc/<pid> | grep "buffer size"
```

---

## 八、总结

```
binder_alloc 内存管理
      │
      ├── 分配策略: 最佳适应（Best-Fit）
      │     └── free_buffers 红黑树 → 找 >= 需求的最小空闲块
      │           ├── 完全匹配 → 直接取用
      │           └── 部分匹配 → 切割，剩余部分重新插入
      │
      ├── 回收策略: 释放后合并相邻空闲块
      │     └── buffer_merge_free()
      │           ├── 和前一个空闲块合并
      │           └── 和后一个空闲块合并
      │
      ├── 物理页: 惰性分配（按需分配）
      │     └── vm_insert_page() → 使用时才分配物理页
      │
      └── 异步空间: 独立配额（buffer_size / 2）
            └── 防止 oneway 调用淹没目标进程
```

| 特性 | 值 |
|------|-----|
| 默认 mmap 大小 | 1MB - 2×page size |
| 最大映射大小 | 4MB（内核 SVE 限制） |
| 异步空间上限 | mmap 大小的一半 |
| 分配算法 | 最佳适应（Best-Fit） |
| 物理页分配 | 惰性分配（使用时才分配） |
| 碎片处理 | 释放时自动合并相邻空闲块 |
| 并发保护 | mutex 互斥锁 |

**文件**：`drivers/android/binder_alloc.c`、`drivers/android/binder_alloc.h`
