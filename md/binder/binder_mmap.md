
在 Linux 内核中，Binder 是一种进程间通信（IPC）机制，广泛应用于 Android 系统。Binder 驱动通过映射物理内存和虚拟内存实现高效的跨进程数据传输。下面详细解释 Binder 如何进行内存映射：



### **Binder 内存映射原理**

Binder 采用 **内存映射（mmap）** 技术实现高效的 IPC，主要分为两个层面：

  

1. **用户空间到内核空间的映射**  
    Binder 驱动允许进程将内核缓冲区直接映射到自己的用户空间，避免了传统 IPC 中的数据拷贝。
    
2. **物理内存的分配与管理**  
    Binder 驱动负责分配物理页，并通过页表将物理内存映射到不同进程的虚拟地址空间。
    

### **映射流程详解**

#### **1. 用户空间初始化映射（`binder_alloc_mmap_handler`）**

当进程调用 `mmap()` 系统调用时，Binder 驱动通过 `binder_alloc_mmap_handler` 函数处理映射请求：

  

c

运行

```c
// binder_alloc_mmap_handler 关键代码
alloc->buffer = (void __user *)vma->vm_start;  // 用户空间虚拟地址
alloc->pages = kzalloc(...);                   // 分配页表数组
alloc->buffer_size = vma->vm_end - vma->vm_start;  // 映射大小

// 创建并初始化缓冲区
struct binder_buffer *buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
buffer->user_data = alloc->buffer;  // 指向用户空间地址
list_add(&buffer->entry, &alloc->buffers);
```

  

- **关键点**：
    - `vma->vm_start` 是用户空间的虚拟地址。
    - `alloc->pages` 数组用于跟踪物理页。
    - `binder_buffer` 结构将用户空间地址与内核管理的缓冲区关联。

#### **2. 物理内存分配**

当数据需要传输时，Binder 驱动按需分配物理页：

  

c

运行

```c
// 简化的物理页分配逻辑
struct page *page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM);
if (page) {
    // 将物理页添加到 alloc->pages 数组
    alloc->pages[index] = page;
}
```

  

- **关键点**：
    - 通过 `alloc_page()` 分配物理页。
    - 物理页与虚拟地址的映射关系由内核页表维护。

#### **3. 虚拟地址与物理地址的关联**

Binder 驱动通过 **内核辅助映射（Kernel-assisted Mapping）** 实现：

  

1. **用户空间映射**：  
    进程 A 通过 `mmap()` 将内核缓冲区映射到用户空间地址 `VA_A`。
    
2. **内核空间映射**：  
    Binder 驱动将同一物理页映射到内核空间地址 `KA`。
    
3. **跨进程映射**：  
    进程 B 通过 `mmap()` 将同一内核缓冲区映射到自己的用户空间地址 `VA_B`。
    

  

plaintext

```plaintext
┌───────────────────────────────────────────────────┐
│                    物理内存                        │
│  ┌───────────┐                                     │
│  │  物理页   │                                     │
│  └───────────┘                                     │
└───────────┬───────────────────────────────────────┘
            │
            ▼
┌───────────────────────────────────────────────────┐
│                   内核空间                         │
│  ┌───────────┐                                     │
│  │   KA      │ ←── 内核页表映射                     │
│  └───────────┘                                     │
└───────────┬───────────────────────────────────────┘
            │
            ▼
┌───────────────────────────────────────────────────┐
│                  用户空间                          │
│  进程 A                进程 B                      │
│  ┌───────────┐        ┌───────────┐                │
│  │   VA_A    │        │   VA_B    │ ←── 用户页表映射 │
│  └───────────┘        └───────────┘                │
└───────────────────────────────────────────────────┘
```

  

- **优势**：数据只需从发送进程的用户空间（`VA_A`）写入内核缓冲区（`KA`），接收进程直接从自己的用户空间（`VA_B`）读取，避免了第二次拷贝。

### **关键数据结构**

1. **`struct binder_alloc`**  
    管理进程的内存分配信息：
    
    c
    
    运行
    
    ```c
    struct binder_alloc {
        struct mm_struct *vma_vm_mm;      // 虚拟内存管理器
        void __user *buffer;              // 用户空间基址
        struct page **pages;              // 物理页数组
        size_t buffer_size;               // 缓冲区大小
        // ...
    };
    ```
    
      
    
2. **`struct binder_buffer`**  
    表示一个分配的缓冲区：
    
    c
    
    运行
    
    ```c
    struct binder_buffer {
        struct list_head entry;           // 链表节点
        void __user *user_data;           // 用户空间地址
        size_t size;                      // 缓冲区大小
        int free;                         // 是否空闲
        // ...
    };
    ```
    
      
    
      
    

### **映射过程中的同步与安全**

- **锁机制**：使用 `binder_alloc_mmap_lock` 保护对 `binder_alloc` 的并发访问。
- **错误处理**：在内存分配失败时，通过 `goto` 标签链释放已分配的资源（如 `alloc->pages`）。
- **内存屏障**：使用 `barrier()` 确保内存操作的顺序性，避免竞态条件。

### **总结**

Binder 通过 **虚拟内存映射** 和 **物理页共享** 实现高效 IPC：

  

1. 用户空间通过 `mmap()` 将内核缓冲区映射到自己的地址空间。
2. 内核负责分配物理页，并维护物理页与虚拟地址的映射关系。
3. 数据传输时，只需一次拷贝（从发送进程的用户空间到内核缓冲区），接收进程直接从自己的用户空间读取数据。

  

这种机制显著减少了 IPC 开销，提高了系统性能。


```
/**  
 * 地址映射  
 * @param filp * @param vma * @return */static int binder_mmap(struct file *filp, struct vm_area_struct *vma /*用户空间*/) {  
    int ret;  
    struct binder_proc *proc = filp->private_data;  // 存储data  
    const char *failure_string;  
  
    if (proc->tsk != current->group_leader)  
        return -EINVAL;  
  
    if ((vma->vm_end - vma->vm_start) > SZ_4M)  
        vma->vm_end = vma->vm_start + SZ_4M;  
  
    binder_debug(BINDER_DEBUG_OPEN_CLOSE,  
                 "%s: %d %lx-%lx (%ld K) vma %lx pagep %lx\n",  
                 __func__, proc->pid, vma->vm_start, vma->vm_end,  
                 (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,  
                 (unsigned long) pgprot_val(vma->vm_page_prot));  
  
    if (vma->vm_flags & FORBIDDEN_MMAP_FLAGS) {  
        ret = -EPERM;  
        failure_string = "bad vm_flags";  
        goto err_bad_arg;  
    }  
    vma->vm_flags |= VM_DONTCOPY | VM_MIXEDMAP;  
    vma->vm_flags &= ~VM_MAYWRITE;  
  
    vma->vm_ops = &binder_vm_ops;  
    vma->vm_private_data = proc;  
  
    ret = binder_alloc_mmap_handler(&proc->alloc, vma);  
    if (ret)  
        return ret;  
    mutex_lock(&proc->files_lock);  
    proc->files = get_files_struct(current);  
    mutex_unlock(&proc->files_lock);  
    return 0;  
  
    err_bad_arg:  
    pr_err("%s: %d %lx-%lx %s failed %d\n", __func__,  
           proc->pid, vma->vm_start, vma->vm_end, failure_string, ret);  
    return ret;  
}
```

```
/**  
 * binder_alloc_mmap_handler() - map virtual address space for proc * @alloc: alloc structure for this proc * @vma:   vma passed to mmap() * * Called by binder_mmap() to initialize the space specified in * vma for allocating binder buffers * * Return: *      0 = success *      -EBUSY = address space already mapped *      -ENOMEM = failed to map memory to given address space */int binder_alloc_mmap_handler(struct binder_alloc *alloc,  
               struct vm_area_struct *vma)  
{  
   int ret;  
   const char *failure_string;  
   struct binder_buffer *buffer;  
  
   mutex_lock(&binder_alloc_mmap_lock);  
   if (alloc->buffer) {  
      ret = -EBUSY;  
      failure_string = "already mapped";  
      goto err_already_mapped;  
   }  
  
   alloc->buffer = (void __user *)vma->vm_start;  // 映射存储用户空间的地址  
   mutex_unlock(&binder_alloc_mmap_lock);  
  
   alloc->pages = kzalloc(sizeof(alloc->pages[0]) *  
               ((vma->vm_end - vma->vm_start) / PAGE_SIZE),  
                GFP_KERNEL);  
   if (alloc->pages == NULL) {  
      ret = -ENOMEM;  
      failure_string = "alloc page array";  
      goto err_alloc_pages_failed;  
   }  
   alloc->buffer_size = vma->vm_end - vma->vm_start;  
  
   buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);  
   if (!buffer) {  
      ret = -ENOMEM;  
      failure_string = "alloc buffer struct";  
      goto err_alloc_buf_struct_failed;  
   }  
  
   buffer->user_data = alloc->buffer;  
   list_add(&buffer->entry, &alloc->buffers);  
   buffer->free = 1;  
   binder_insert_free_buffer(alloc, buffer);  
   alloc->free_async_space = alloc->buffer_size / 2;  
   barrier();  
   alloc->vma = vma;  
   alloc->vma_vm_mm = vma->vm_mm;  
   /* Same as mmgrab() in later kernel versions */  
   atomic_inc(&alloc->vma_vm_mm->mm_count);  
  
   return 0;  
  
err_alloc_buf_struct_failed:  
   kfree(alloc->pages);  
   alloc->pages = NULL;  
err_alloc_pages_failed:  
   mutex_lock(&binder_alloc_mmap_lock);  
   alloc->buffer = NULL;  
err_already_mapped:  
   mutex_unlock(&binder_alloc_mmap_lock);  
   pr_err("%s: %d %lx-%lx %s failed %d\n", __func__,  
          alloc->pid, vma->vm_start, vma->vm_end, failure_string, ret);  
   return ret;  
}
```