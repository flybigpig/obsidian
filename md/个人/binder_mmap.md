### 一 内存映射函数的实现 `binder_mmap`(`kernel/drivers/android/binder.c`)

```cpp
static int binder_mmap(struct file *filp, struct vm_area_struct *vma/*用户态虚拟地址空间描述，地址空间在0～3G*/)
{
    int ret;
    /* 一块连续的内核虚拟地址空间描述，32位体系架构中地址空间在 3G+896M + 8M ～ 4G之间*/
    struct vm_struct *area;
    struct binder_proc *proc = filp->private_data;
    const char *failure_string;
    struct binder_buffer *buffer;

    if (proc->tsk != current)
        return -EINVAL;

    //申请空间不能大于4M，如果大于4M就改为4M大小。
    if ((vma->vm_end - vma->vm_start) > SZ_4M)
          vma->vm_end = vma->vm_start + SZ_4M;

     binder_debug(BINDER_DEBUG_OPEN_CLOSE,
             "binder_mmap: %d %lx-%lx (%ld K) vma %lx pagep %lx\n",
               proc->pid, vma->vm_start, vma->vm_end,
             (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,
             (unsigned long)pgprot_val(vma->vm_page_prot));

    //检查vma是否被forbidden，vma是一块连续的用户态虚拟内存地址空间的描述

    if (vma->vm_flags & FORBIDDEN_MMAP_FLAGS) {
          ret = -EPERM;
          failure_string = "bad vm_flags";
        goto err_bad_arg;
    }

    //打开VM_DONTCOPY,关闭VM_MAYWRITE
     vma->vm_flags = (vma->vm_flags | VM_DONTCOPY) & ~VM_MAYWRITE;

    //加上binder_mmap_lock互斥锁，因为接下来要操作proc结构体，可能发生多线程竞争
     mutex_lock(&binder_mmap_lock);

    //一个进程已经有一次mmap，如要执行新的map，需先将之前的unmap。
    if (proc->buffer) {
          ret = -EBUSY;
          failure_string = "already mapped";
          goto err_already_mapped;
    }

    /* 获取一块与用户态空间大小一致的内核的连续虚拟地址空间，
     * 注意虚拟地址空间是在此一次性分配的，物理页面却是需要时才去申请和映射
    */
    area = get_vm_area(vma->vm_end - vma->vm_start, VM_IOREMAP);
    if (area == NULL) {
          ret = -ENOMEM;
          failure_string = "get_vm_area";
        goto err_get_vm_area_failed;
    }

    //将内核虚拟地址记录在proc的buffer中 
     proc->buffer = area->addr;

    /* 记录用户态虚拟地址空间与内核态虚拟地址空间的偏移量，
     * 这样通过buffer和user_buffer_offset就可以计算出用户态的虚拟地址。
    */
     proc->user_buffer_offset = vma->vm_start - (uintptr_t)proc->buffer;

    /*释放互斥锁*/
     mutex_unlock(&binder_mmap_lock);
#ifdef CONFIG_CPU_CACHE_VIPT
    /* CPU的缓存方式是否为： VIPT（Virtual Index Physical Tag）：使用虚拟地址的索引域和物理地址的标记域。
     * 这里先不管，有兴趣的可参照：[https://blog.csdn.net/Q_AN1314/article/details/78980191](https://blog.csdn.net/Q_AN1314/article/details/78980191)
    */

    if (cache_is_vipt_aliasing()) {
        while (CACHE_COLOUR((vma->vm_start ^ (uint32_t)proc->buffer))) {
               pr_info("binder_mmap: %d %lx-%lx maps %p bad alignment\n", proc->pid, vma->vm_start, vma->vm_end, proc->buffer);
               vma->vm_start += PAGE_SIZE;
        }
    }
#endif

    /*分配存放物理页地址的数组*/
    proc->pages = kzalloc(sizeof(proc->pages[0]) * ((vma->vm_end - vma->vm_start) / PAGE_SIZE), GFP_KERNEL);
    if (proc->pages == NULL) {
          ret = -ENOMEM;
          failure_string = "alloc page array";
        goto err_alloc_pages_failed;
    }

    /*将虚拟地址空间的大小记录在proc的buffer_size中*/
     proc->buffer_size = vma->vm_end - vma->vm_start;

    /* 安装vma线性空间操作函数：open，close，fault
    * open-> binder_vma_open: 简单的输出日志，pid，虚拟地址的起止、大小、标志位（vm_flags和vm_page_prot)
    * close -> binder_vma_close: 将proc的vma，vma_vm_mm设为NULL，并将proc加入到binder_deferred_workqueue队列，
    *                            binder驱动有一个单独的线程处理这个队列。
    * fault -> binder_vam_fault: 直接返回VM_FAULT_SIGBUS, 
    */
     vma->vm_ops = &binder_vm_ops;

    /*在vma的vm_private_data字段里存入proc的引指针*/
     vma->vm_private_data = proc;

    /* 先分配1个物理页，并将其分别映射到内核线性地址和用户态虚拟地址上，具体详见2
    */
    if (binder_update_page_range(proc, 1, proc->buffer, proc->buffer **+ PAGE_SIZE**, vma)) {
          ret = -ENOMEM;
          failure_string = "alloc small buf";
        goto err_alloc_small_buf_failed;
    }

    /*成功分配了物理页并建立好的映射关系后，内核起始虚地址做为第一个binder_buffer的地址*/
     buffer = proc->buffer;
    /*接着将内核虚拟内存链入proc的buffers和free_buffers链表中，free标志位设为1
     INIT_LIST_HEAD(&proc->buffers);
     list_add(&buffer->entry, &proc->buffers);
     buffer->free = 1;
     binder_insert_free_buffer(proc, buffer);
     /*异步只能使用整个地址空间的一半*/
     proc->free_async_space = proc->buffer_size / 2;
     barrier();
     proc->files = get_files_struct(current);
     proc->vma = vma;
     proc->vma_vm_mm = vma->vm_mm;/*vma->vm_mm: vma对应的mm_struct,描述一个进程的虚拟地址空间，一个进程只有一个*/

    /*pr_info("binder_mmap: %d %lx-%lx maps %p\n",
           proc->pid, vma->vm_start, vma->vm_end, proc->buffer);*/

    return 0;

    /*出错处理*/
err_alloc_small_buf_failed:

     kfree(proc->pages);
     proc->pages = NULL;
err_alloc_pages_failed:
     mutex_lock(&binder_mmap_lock);
     vfree(proc->buffer);
     proc->buffer = NULL;
err_get_vm_area_failed:
err_already_mapped:
     mutex_unlock(&binder_mmap_lock);

err_bad_arg:
     pr_err("binder_mmap: %d %lx-%lx %s failed %d\n",
            proc->pid, vma->vm_start, vma->vm_end, failure_string, ret);
    return ret;
}
```

打开binder后，需要调用`mmap`进行内存映射，该函数经过系统调用，会调用到binder驱动的`binder_mmap`函数。基本过程在上面的代码基本已经都注释了。几个需要注意的地方这里再说明一下：

+   调用`get_vm_area`获取内核态虚拟地址，地址是在32体系架构地址空间在： 3G+896M + 8M ～ 4G之间。`proc`中没有直接记录用户态的虚地址，而是存放一个用户态地址与内核态地址偏移量：`proc->user_buffer_offset`。
    
+   `vma`操作函数 —— `vma->vm_ops`, Binder实现了`open`, `close`和`fault`三个接口。
    
    +   `open`(`binder_vma_open`)的实现只是简单的输出一条进程id及`vma`地址及标志位相关信息的debug日志。
    +   `close`(`binder_vma_close`)除了输出一条类似`open`的日志信息外，还会将`proc->vma`和`proc->vma_vm_mm`置空，接着调用`binder_deferred_work`在`binder_deferred_workqueue`队列中放入一个`BINDER_DEFERRED_PUT_FILES`状态的work，在之后binder线程执行到该work时会将`proc->files`置空, 接着调用`put_files_struct`来**释放进程所属的文件资源**。
    +   `fault`(`binder_vm_fault`):简单的返回`VM_FAULT_SIGBUS`, 这个钩子是在访问的虚地址没有映射的物理页时（缺页）时，该函数被缺页处理程序调用，该函数负责返回物理页描述符，但因在binder中物理页框与虚拟地址的映射，在调用`binder_alloc_buf`分配`binder_buffer`时就已经建立好了，所以一般来说是不会发生缺页中断的。
+   需要调用`binder_update_page_range`分配一个页框的原因是：用于存放第一个binder\_buffer，此时整个地址空间都在这个binder\_buffer中管理， 但是随着地址空间被不断的分配和回收，会分裂成一系列的binder\_buffer节点。
    

### 二 分配物理页框，建立地址映射 —— `binder_update_page_range`

```cpp
static int binder_update_page_range(struct binder_proc *proc, int allocate,
                    void *start, void *end,
                    struct vm_area_struct *vma)
{
    void *page_addr;
    unsigned long user_page_addr;
    struct page **page;
    struct mm_struct *mm;

    binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
             "%d: %s pages %p-%p\n", proc->pid,
             allocate ? "allocate" : "free", start, end);

    if (end <= start)
        return 0;

    trace_binder_update_page_range(proc, allocate, start, end);
    if (vma)
        mm = NULL; /*binder_mmap的调用走这里*/
    else
        /* 读取进程的内存描述符（mm_struct）, 
         * 并增加内存描述符(mm_struct)中的mm_users用户计数，防止mm_struct被释放*/
        mm = get_task_mm(proc->tsk); 

    if (mm) {
        /*获取写锁*/
        down_write(&mm->mmap_sem);
        vma = proc->vma;
        if (vma && mm != proc->vma_vm_mm) {
            pr_err("%d: vma mm and task mm mismatch\n",
                proc->pid);
            vma = NULL;
        }
    }

    /*本次调用是释放物理页，直接进入释放物理页框流程*/

    if (allocate == 0) 
        goto free_range;

    if (vma == NULL) {
        pr_err("%d: binder_alloc_buf failed to map pages in userspace, no vma\n",
            proc->pid);
        goto err_no_vma;
    }

    /* 开始循环分配物理页，并建立映射，每次循环分配1个页框*/
    for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
        int ret;
        /* 确定页框所存放的数组的位置，按内核虚拟地址由小到大排列*/
        page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];
        BUG_ON(*page);
         /*分配页框*/
        *page = **alloc_page**(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO);
        if (*page == NULL) {
            pr_err("%d: binder_alloc_buf failed for page at %p\n",
                proc->pid, page_addr);
            goto err_alloc_page_failed;
        }

        /*将内核虚拟地址与该页框建立映射关系*/
        ret = **map_kernel_range_noflush**((unsigned long)page_addr,
                    PAGE_SIZE, PAGE_KERNEL, page);

        /* 将刚刚修改的内核页表项刷新到CPU高速缓存*/
        **flush_cache_vmap**((unsigned long)page_addr,
                (unsigned long)page_addr + PAGE_SIZE);

        if (ret != 1) {
            pr_err("%d: binder_alloc_buf failed to map page at %p in kernel\n",
                   proc->pid, page_addr);
            goto err_map_kernel_failed;
        }

        /*计算用户态虚地址*/
        user_page_addr =
            (uintptr_t)page_addr + proc->user_buffer_offset;

        /*将用户虚拟地址与该页框建立映射关系*/
        ret = vm_insert_page(vma, user_page_addr, page[0]);
        if (ret) {
            pr_err("%d: binder_alloc_buf failed to map page at %lx in userspace\n",
                   proc->pid, user_page_addr);
            goto err_vm_insert_page_failed;
        }
        /* vm_insert_page does not seem to increment the refcount */
    }

    if (mm) {
        /*释放写锁*/
        up_write(&mm->mmap_sem);
        mmput(mm);/*减少内存描述符(mm_struct)中的mm_users用户计数*/
    }

    /*分配物理页框流程到这里结束，接下去是释放物理页流程*/
    return 0;

/*释放物理页，解除地址映射*/
free_range:/*由后往前解除用户态及内核态的物理页框映射，并释放物理页框*/
    for (page_addr = end - PAGE_SIZE; page_addr >= start;
         page_addr -= PAGE_SIZE) {
        page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];
        if (vma)
            /*解除用户态虚拟地址和物理页框的映射*/
            zap_page_range(vma, (uintptr_t)page_addr +
                proc->user_buffer_offset, PAGE_SIZE, NULL);
err_vm_insert_page_failed:
        /*解除内核态虚拟地址和物理页框的映射*/
        unmap_kernel_range((unsigned long)page_addr, PAGE_SIZE);
err_map_kernel_failed:
        __free_page(*page);/*释放物理页框*/
        *page = NULL;
err_alloc_page_failed:
        ;
    }

err_no_vma:
    if (mm) {
        /*释放写锁*/
        up_write(&mm->mmap_sem);
        mmput(mm);/*减少内存描述符中的mm_users用户计数*/
    }
    return -ENOMEM;
}
```

+   `binder_update_page_range`同时负责分配和释放物理页框，具体是分配还是释放通过参数`allocate`控制，如果该参数为`0`，则表示要解除内核态和用户态对物理页框的地址映射，释放物理页框；否则就是申请物理页框，并建立内核态和用户态的地址映射。整个流程相对直观，关键代码都已经注释，理解起来应该不难。
    
+   参数`vma`的类型是`struct vm_area_struct`是用户态虚拟地址空间描述。该参数为`NULL`表示`binder_update_page_range`在内核调用路径中，此时需尝试获取`mm_struct`并增加其引用计数，以防止进程的内存描述被释放，然后再在操作完成后减少它的引用计数(`mmput`)。