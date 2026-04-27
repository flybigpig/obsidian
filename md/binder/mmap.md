我来查找 Binder 内存映射（mmap）的核心代码。
让我搜索 `binder_mmap` 函数，这是内存映射的核心入口：
现在让我读取 `binder_update_page_range` 函数，这是实际分配/释放物理页面的核心：
现在让我再查看 `binder_alloc_new_buf` 函数，这是从 mmap 区域分配 buffer 的核心：
让我再读取 `binder_alloc_new_buf_locked` 的核心分配逻辑：
现在我已经掌握了 Binder 内存映射的完整核心代码。以下是详细解析：

---

## Binder 内存映射（mmap）核心代码解析

### 一、整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                    用户空间 (User Space)                       │
│  ┌───────────────────────────────────────────────────────┐  │
│  │           mmap 映射区域 (最大 4MB)                        │  │
│  │  ┌─────────┐ ┌──────────┐ ┌──────────┐ ┌─────────┐   │  │
│  │  │ buffer1 │ │ buffer2  │ │ buffer3  │ │  free   │   │  │
│  │  └─────────┘ └──────────┘ └──────────┘ └─────────┘   │  │
│  └───────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                    内核空间 (Kernel Space)                     │
│  binder_alloc {                                              │
│    vma ──────────────► vm_area_struct  (虚拟内存区域)          │
│    buffer ───────────► 用户空间起始地址                         │
│    pages[] ──────────► 物理页表数组                            │
│    free_buffers ─────► 红黑树(按大小, 空闲buffer)              │
│    allocated_buffers ► 红黑树(按地址, 已分配buffer)            │
│  }                                                           │
└─────────────────────────────────────────────────────────────┘
                              │
                     ┌────────▼────────┐
                     │   物理内存页面     │
                     │  (按需分配/释放)   │
                     └─────────────────┘
```

### 二、入口函数：`binder_mmap()` — `binder.c:5465`

```cpp
static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct binder_proc *proc = filp->private_data;

    // ① 限制最大映射大小为 4MB（防止恶意进程占用过多内存）
    if ((vma->vm_end - vma->vm_start) > SZ_4M)
        vma->vm_end = vma->vm_start + SZ_4M;

    // ② 设置 VMA 标志：
    //    VM_DONTCOPY - fork时不复制（子进程不能继承）
    //    VM_MIXEDMAP - 混合映射（支持非连续物理页）
    //    去掉 VM_MAYWRITE - 用户态不可直接写
    vma->vm_flags |= VM_DONTCOPY | VM_MIXEDMAP;
    vma->vm_flags &= ~VM_MAYWRITE;

    // ③ 注册 VMA 操作回调（open/close/fault）
    vma->vm_ops = &binder_vm_ops;
    vma->vm_private_data = proc;

    // ④ 调用分配器处理 mmap
    ret = binder_alloc_mmap_handler(&proc->alloc, vma);
}
```

**关键点：mmap 本身并不分配物理页，只是建立虚拟地址空间的映射关系！**

### 三、核心初始化：`binder_alloc_mmap_handler()` — `binder_alloc.c:653`

```cpp
int binder_alloc_mmap_handler(struct binder_alloc *alloc,
                              struct vm_area_struct *vma)
{
    // ① 记录用户空间缓冲区起始地址
    alloc->buffer = (void __user *)vma->vm_start;  // 虚拟地址起点

    // ② 分配页表数组（每页一个 entry，用于跟踪物理页）
    alloc->pages = kzalloc(sizeof(*alloc->pages) *
                  ((vma->vm_end - vma->vm_start) / PAGE_SIZE), GFP_KERNEL);

    // ③ 记录映射大小
    alloc->buffer_size = vma->vm_end - vma->vm_start;  // 通常为 4MB

    // ④ 创建一个覆盖整个区域的初始空闲 buffer
    buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
    buffer->user_data = alloc->buffer;          // 指向用户空间起始
    list_add(&buffer->entry, &alloc->buffers);  // 加入链表
    buffer->free = 1;
    binder_insert_free_buffer(alloc, buffer);   // 插入空闲红黑树

    // ⑤ 异步事务最多使用一半空间（防止单个异步事务耗尽所有内存）
    alloc->free_async_space = alloc->buffer_size / 2;  // 2MB

    // ⑥ 保存 VMA 引用和 mm_struct（内存描述符）
    alloc->vma = vma;
    alloc->vma_vm_mm = vma->vm_mm;
    atomic_inc(&alloc->vma_vm_mm->mm_count);  // 增加 mm 引用计数

    return 0;
}
```

### 四、Buffer 分配：`binder_alloc_new_buf_locked()` — `binder_alloc.c:321`

这是 **实际分配 buffer 并映射物理页** 的核心：

```cpp
static struct binder_buffer *binder_alloc_new_buf_locked(
        struct binder_alloc *alloc, size_t data_size,
        size_t offsets_size, size_t extra_buffers_size, int is_async)
{
    size = max(size, sizeof(void *));  // 最小对齐到指针大小

    // ══════════════════════════════════════
    // 第一步：最佳适应算法 (Best Fit) 在空闲红黑树中查找
    // ══════════════════════════════════════
    while (n) {
        buffer = rb_entry(n, struct binder_buffer, rb_node);
        buffer_size = binder_alloc_buffer_size(alloc, buffer);
        
        if (size < buffer_size) {
            best_fit = n;       // 记录候选
            n = n->rb_left;     // 继续找更小的（更合适的）
        } else if (size > buffer_size) {
            n = n->rb_right;    // 当前太小，找更大的
        } else {
            best_fit = n; break;// 完美匹配！
        }
    }

    // ══════════════════════════════════════
    // 第二步：按需映射物理页面 (Lazy Allocation!)
    // ══════════════════════════════════════
    has_page_addr = (void __user *)
        (((uintptr_t)buffer->user_data + buffer_size) & PAGE_MASK);
    end_page_addr = (void __user *)PAGE_ALIGN(
        (uintptr_t)buffer->user_data + size);
    
    // ★ 关键：只为新需要的部分分配物理页
    ret = binder_update_page_range(alloc, 1,
        PAGE_ALIGN((uintptr_t)buffer->user_data),
        end_page_addr);

    // ══════════════════════════════════════
    // 第三步：如果 buffer 有剩余，拆分成两个
    // ══════════════════════════════════════
    if (buffer_size != size) {
        new_buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
        new_buffer->user_data = (u8 __user *)buffer->user_data + size;
        list_add(&new_buffer->entry, &buffer->entry);  // 插入链表
        new_buffer->free = 1;
        binder_insert_free_buffer(alloc, new_buffer);  // 剩余部分放回空闲树
    }

    // ══════════════════════════════════════
    // 第四步：标记为已分配
    // ══════════════════════════════════════
    rb_erase(best_fit, &alloc->free_buffers);          // 从空闲树移除
    buffer->free = 0;
    binder_insert_allocated_buffer_locked(alloc, buffer);// 加入已分配树
    
    return buffer;
}
```

### 五、物理页管理：`binder_update_page_range()` — `binder_alloc.c:187`

```cpp
static int binder_update_page_range(struct binder_alloc *alloc, int allocate,
                                    void __user *start, void __user *end)
{
    if (allocate == 0) goto free_range;  // 释放路径

    // 获取 mm_sem 锁，保护进程的内存布局
    down_read(&mm->mmap_sem);
    vma = alloc->vma;

    for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
        page = &alloc->pages[(page_addr - alloc->buffer) / PAGE_SIZE];

        if (!page->page_ptr) {
            // ★ 分配物理页（零填充）
            page->page_ptr = alloc_page(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO);
            
            // ★ 将物理页映射到用户空间虚拟地址
            ret = vm_insert_page(vma, user_page_addr, page->page_ptr);
        } else {
            // 页面已存在于 LRU 中，从 LRU 移除（重新激活）
            list_lru_del(&binder_alloc_lru, &page->lru);
        }
    }
    up_read(&mm->mmap_sem);
    return 0;

free_range:  // 释放路径：不立即释放物理页，而是放入 LRU
    for (...) {
        list_lru_add(&binder_alloc_lru, &page->lru);  // 延迟回收!
    }
}
```

### 六、内存释放与 LRU 回收机制

```
释放流程:
  binder_free_buf()
      → 检查相邻 buffer 是否共享物理页
         ├─ 共享页 → 不释放（其他 buffer 还在用）
         └─ 独占页 → binder_update_page_range(alloc, 0, ...) 
                      → 不直接 free_page! 而是加入 LRU 链表
                      
LRU 回收:
  内存压力时 → binder_alloc_free_page() → __free_page() 真正释放
```

### 七、完整调用链总结

```
用户态: ProcessState::open_driver() → ioctl(BINDER_SET_CONTEXT_MGR)
                                        ↓
内核态: binder_mmap()                          ← 只建立 VMA，不分配物理页
         ↓
       binder_alloc_mmap_handler()             ← 初始化 alloc 结构，创建初始空闲 buffer
         ↓
       [等待 IPC 请求...]
         ↓
       binder_transaction()                    ← 收到 BC_TRANSACTION
         ↓
       binder_alloc_new_buf()                  ← 分配 buffer
         ↓
       binder_alloc_new_buf_locked()           ← Best Fit 查找空闲块
         ↓
       binder_update_page_range(allocate=1)    ← ★ 按需分配并映射物理页
         ↓
       copy_from_user() / memcpy()             ← 数据写入共享内存
         ↓
       [Server 处理完成...]
         ↓
       binder_alloc_free_buf()                 ← 释放 buffer
         ↓
       binder_update_page_range(allocate=0)    ← 放入 LRU（延迟回收）
```

### 八、核心设计要点

| 特性 | 实现方式 |
|------|---------|
| **懒式分配** | mmap 时只建虚拟映射，物理页在首次分配 buffer 时才申请 |
| **最佳适应算法** | 红黑树组织空闲 buffer，按大小查找最接近的空闲块 |
| **延迟回收** | 释放 buffer 时不立即释放物理页，放入 LRU 等待内存压力时统一回收 |
| **异步限额** | 异步事务最多占用一半空间 (2MB)，防止异步通知淹没同步通信 |
| **安全防护** | `VM_DONTCOPY`(fork不复制)、`~VM_MAYWRITE`(用户不可直接写)、最大4MB限制 |
| **零拷贝** | 通过 mmap 共享内存实现，数据只需一次复制到共享区即可被对方读取 |

这就是 Binder 内存映射的完整内核级实现。其精髓在于 **"先建虚映射 + 按需分物理页 + LRU延迟回收"** 三级策略，既保证了 IPC 效率，又避免了内存浪费。