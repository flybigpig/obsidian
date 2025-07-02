---
created: 2025-07-02T13:33:25 (UTC +08:00)
tags: [操作系统原理]
source: https://www.cnblogs.com/theseventhson/p/15928203.html
author: 第七子007
---

# linux源码解读（二十六）：android binder原理和实现方式 - 第七子007 - 博客园

> ## Excerpt
> 1、linux提供了好几种IPC的机制：共享内存、管道、消息队列、信号量等，所有IPC机制的核心或本质就是在内核开辟一块空间，通信双方都从这块空间读写数据，整个流程图示如下： 这种通信方式天生的缺陷看出来了么? A进程把数据拷贝到内核，B进程从内核再拷贝走，同一份数据可能在内存存放了3份，同时还复制

---
　　1、linux提供了好几种IPC的机制：共享内存、管道、消息队列、信号量等，所有IPC机制的核心或本质就是在内核开辟一块空间，通信双方都从这块空间读写数据，整个流程图示如下：

          ![](https://img2022.cnblogs.com/blog/2052730/202202/2052730-20220223173811918-1508177042.png)

      这种通信方式天生的缺陷看出来了么? A进程把数据拷贝到内核，B进程从内核再拷贝走，同一份数据可能在内存存放了3份，同时还复制了2次，感觉和app通过read、write等方法从磁盘读数据的效率一样低啊！既然app读写文件数据能通过mmap提升效率，IPC是不是也能通过mmap提升效率了？答案也是肯定的，这就是android中binder诞生的背景！相比传统的IPC，binder只需要拷贝1次，整个原理和流程如下图所示：

　　![](https://img2022.cnblogs.com/blog/2052730/202202/2052730-20220223180854847-984964398.png)

       A进程还是把数据从用户空间写到内核缓存区，也就是生产者的流程或方式没变！改动最大的就是消费端了：先是内核建立数据接受缓存区，但这个缓存区和内核缓存区建立了映射，换句话说用的是同一块物理地址！接着是接收端进程空间和内核的数据接受缓存区页也建立映射，换句话说用的也是同一块物理地址，这样一来**内核实际只用了1块物理地址，但是这块物理地址映射了2个虚拟地址（感觉好鸡贼.....）**！

　　2、原理和本质其实也很简单：通信双方不就是共用一块物理内存么？既然双方都要依靠这块内存，那么对这块内存的管理就尤为重要了！围绕这块内存的各种操作，也就是binder的各种操作，都定义在了binder\_fops 结构体中，如下：

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

```
static const struct file_operations binder_fops = {
    .owner = THIS_MODULE,
    .poll = binder_poll,
    .unlocked_ioctl = binder_ioctl,
    .compat_ioctl = binder_ioctl,
    .mmap = binder_mmap,
    .open = binder_open,
    .flush = binder_flush,
    .release = binder_release,
};
```

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

　　从名字就能看出来，分别是poll、io控制、mmap、打开、刷新和释放，通过这些接口完全能够操作共用的物理内存了！老规矩，在分析具体的实现方法前，先看看有哪些重要的结构体，从这些结构体的属性字段就能管中窥豹，看出具体怎么落地实现，结构体主要字段和关联关系如下：

![](https://img2022.cnblogs.com/blog/2052730/202202/2052730-20220224143853804-134590380.png)

 　　（2）要想实现IPC，第一个就要在内核开辟内存空间，否则交换个毛的数据啊！看上面的结构体，交换数据的空间在binder\_buffer中，和binder\_proc中的buffers的字段是关联的，所以binder驱动先生成binder\_proc实例（本质是用来管理交换数据的公共内存），初始化后加入链表队列，整个过程在binder\_open方法内部实现的，如下：

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

```
/*1、新生成、初始化binder_proc实例，并加入binder_procs全局队列
  2、初始化todo队列和等待队列
*/
static int binder_open(struct inode *nodp, struct file *filp)
{
    struct binder_proc *proc;

    binder_debug(BINDER_DEBUG_OPEN_CLOSE, "binder_open: %d:%d\n",
             current->group_leader->pid, current->pid);
    // 分配 binder_proc 数据结构内存
    proc = kzalloc(sizeof(*proc), GFP_KERNEL);
    if (proc == NULL)
        return -ENOMEM;
    //增加task结构体的引用计数
    get_task_struct(current);
    proc->tsk = current;
    proc->vma_vm_mm = current->mm;
    INIT_LIST_HEAD(&proc->todo);//初始化待处理事件队列头
    init_waitqueue_head(&proc->wait);//初始化等待队列头
    proc->default_priority = task_nice(current);
    //锁定临界区
    binder_lock(__func__);
    // 增加BINDER_STAT_PROC的对象计数
    binder_stats_created(BINDER_STAT_PROC);
    /*添加新生成的proc_node到 binder_procs全局队列中，
    这样任何进程就可以访问到其他进程的 binder_proc 对象了*/
    hlist_add_head(&proc->proc_node, &binder_procs);
    proc->pid = current->group_leader->pid;
    INIT_LIST_HEAD(&proc->delivered_death);
    filp->private_data = proc;
    //释放临界区的锁
    binder_unlock(__func__);

    if (binder_debugfs_dir_entry_proc) {
        char strbuf[11];

        snprintf(strbuf, sizeof(strbuf), "%u", proc->pid);
        proc->debugfs_entry = debugfs_create_file(strbuf, S_IRUGO,
            binder_debugfs_dir_entry_proc, proc, &binder_proc_fops);
    }

    return 0;
}
```

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

　　既然binder\_open是生成binder\_proc实例，用完后也需要释放和回收，避免内存泄漏，binder驱动提供了binder\_release方法，如下:

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

```
static int binder_release(struct inode *nodp, struct file *filp)
{
    struct binder_proc *proc = filp->private_data;

    debugfs_remove(proc->debugfs_entry);
    binder_defer_work(proc, BINDER_DEFERRED_RELEASE);

    return 0;
}
static void
binder_defer_work(struct binder_proc *proc, enum binder_deferred_state defer)
{
    mutex_lock(&binder_deferred_lock);
    proc->deferred_work |= defer;
    if (hlist_unhashed(&proc->deferred_work_node)) {
        //binder_proc实例添加到释放队列
        hlist_add_head(&proc->deferred_work_node,
                &binder_deferred_list);
        schedule_work(&binder_deferred_work);
    }
    mutex_unlock(&binder_deferred_lock);
}
```

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

　　(3) binder\_proc本质上是用来管理共用内存的结构体，这个实例化后就需要开始最重要的一步了：**在进程虚拟地址申请内存，然后映射到内核的物理地址**，这个过程是在binder\_map中实现的，代码如下：

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

```
/*把内核的物理内存映射到用户进程地址空间中，这样就可以像操作用户内存那样操作内核内存*/
static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret;
    struct vm_struct *area;
    //获取proc实例，这里有通信双方的共用内存
    struct binder_proc *proc = filp->private_data;
    const char *failure_string;
    //通信双方的共用内存
    struct binder_buffer *buffer;

    if (proc->tsk != current)
        return -EINVAL;
    //共用内存最多4m，不能再多了
    if ((vma->vm_end - vma->vm_start) > SZ_4M)
        vma->vm_end = vma->vm_start + SZ_4M;

    binder_debug(BINDER_DEBUG_OPEN_CLOSE,
             "binder_mmap: %d %lx-%lx (%ld K) vma %lx pagep %lx\n",
             proc->pid, vma->vm_start, vma->vm_end,
             (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,
             (unsigned long)pgprot_val(vma->vm_page_prot));
    //看看flags是否合法
    if (vma->vm_flags & FORBIDDEN_MMAP_FLAGS) {
        ret = -EPERM;
        failure_string = "bad vm_flags";
        goto err_bad_arg;
    }
    vma->vm_flags = (vma->vm_flags | VM_DONTCOPY) & ~VM_MAYWRITE;
    //锁定临界区，便于在进程之间互斥，避免不同的进程同时申请虚拟内存
    mutex_lock(&binder_mmap_lock);
    if (proc->buffer) {//这块内存已经映射了
        ret = -EBUSY;
        failure_string = "already mapped";
        goto err_already_mapped;
    }
    /*从/proc/self/maps查找未使用的虚拟内存，并申请内核虚拟内存空间
    注意：这里是进程的虚拟地址空间
    */
    area = get_vm_area(vma->vm_end - vma->vm_start, VM_IOREMAP);
    if (area == NULL) {
        ret = -ENOMEM;
        failure_string = "get_vm_area";
        goto err_get_vm_area_failed;
    }
    // 将申请到的内存地址保存到 binder_proc 对象中
    proc->buffer = area->addr;
    proc->user_buffer_offset = vma->vm_start - (uintptr_t)proc->buffer;
    mutex_unlock(&binder_mmap_lock);

#ifdef CONFIG_CPU_CACHE_VIPT
    if (cache_is_vipt_aliasing()) {
        while (CACHE_COLOUR((vma->vm_start ^ (uint32_t)proc->buffer))) {
            pr_info("binder_mmap: %d %lx-%lx maps %p bad alignment\n", proc->pid, vma->vm_start, vma->vm_end, proc->buffer);
            vma->vm_start += PAGE_SIZE;
        }
    }
#endif
    //根据请求到的内存空间大小，分配给binder_proc对象的pages， 用于保存指向物理页的指针
    proc->pages = kzalloc(sizeof(proc->pages[0]) * ((vma->vm_end - vma->vm_start) / PAGE_SIZE), GFP_KERNEL);
    if (proc->pages == NULL) {
        ret = -ENOMEM;
        failure_string = "alloc page array";
        goto err_alloc_pages_failed;
    }
    //共用内存大小，不超过4M
    proc->buffer_size = vma->vm_end - vma->vm_start;

    vma->vm_ops = &binder_vm_ops;
    vma->vm_private_data = proc;
    //分配一个物理页
    if (binder_update_page_range(proc, 1, proc->buffer, proc->buffer + PAGE_SIZE, vma)) {
        ret = -ENOMEM;
        failure_string = "alloc small buf";
        goto err_alloc_small_buf_failed;
    }
    buffer = proc->buffer;
    INIT_LIST_HEAD(&proc->buffers);
    //将binder_buffer对象放入到proc->buffers链表中，便于统一管理
    list_add(&buffer->entry, &proc->buffers);
    buffer->free = 1;
    /*新生成的buffer加入红黑树管理*/
    binder_insert_free_buffer(proc, buffer);
    proc->free_async_space = proc->buffer_size / 2;
    //内存屏障，防止乱序
    barrier();
    proc->files = get_files_struct(current);
    proc->vma = vma;
    proc->vma_vm_mm = vma->vm_mm;

    /*pr_info("binder_mmap: %d %lx-%lx maps %p\n",
         proc->pid, vma->vm_start, vma->vm_end, proc->buffer);*/
    return 0;

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

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

　　这里面最核心的函数就是binder\_update\_page\_range了，这个函数要么释放物理页，要么分配物理页；如果分配物理页，还会建立和进程虚拟地址空间的映射关系！其中真正建立虚拟内存和物理页映射关系的当属map\_kernel\_range\_noflush和vm\_insert\_page函数了：前者是内核虚拟内存和物理页建立映射，后者是进程虚拟内存和物理页建立映射。整个代码如下：

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

```
/*分配和释放物理页；如果是分配，同时建立和进程虚拟地址空间的映射*/
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
        mm = NULL;
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
    //如果传入的allocate是0，就是释放物理页
    if (allocate == 0)
        goto free_range;

    if (vma == NULL) {
        pr_err("%d: binder_alloc_buf failed to map pages in userspace, no vma\n",
            proc->pid);
        goto err_no_vma;
    }
    /* 开始循环分配物理页，并建立映射，每次循环分配1个页*/
    for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
        int ret;
        /* 确定页所存放的数组的位置，按内核虚拟地址由小到大排列*/
        page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];

        BUG_ON(*page);
        //最核心的地方；分配物理页
        *page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO);
        if (*page == NULL) {
            pr_err("%d: binder_alloc_buf failed for page at %p\n",
                proc->pid, page_addr);
            goto err_alloc_page_failed;
        }
        /*将内核虚拟地址与该物理页建立映射关系,最终调用的是
        mm\vmalloc.c的vmap_page_range_noflush，然后依次递进调用
        vmap_pud_range、vmap_pmd_range、vmap_pte_range、set_pte_at设置页表的4级映射关系；
        注意：这里映射的是内核虚拟地址空间，用户进程的虚拟地址空间映射在后面，
        调用的是vm_insert_page方法*/
        ret = map_kernel_range_noflush((unsigned long)page_addr,
                    PAGE_SIZE, PAGE_KERNEL, page);
        //页表更新后刷新cpu的TLB缓存
        flush_cache_vmap((unsigned long)page_addr,
                (unsigned long)page_addr + PAGE_SIZE);
        if (ret != 1) {
            pr_err("%d: binder_alloc_buf failed to map page at %p in kernel\n",
                   proc->pid, page_addr);
            goto err_map_kernel_failed;
        }
        /*计算用户态虚地址*/
        user_page_addr =
            (uintptr_t)page_addr + proc->user_buffer_offset;
        /*将用户虚拟地址与该物理页建立映射关系*/
        ret = vm_insert_page(vma, user_page_addr, page[0]);
        if (ret) {
            pr_err("%d: binder_alloc_buf failed to map page at %lx in userspace\n",
                   proc->pid, user_page_addr);
            goto err_vm_insert_page_failed;
        }
        /* vm_insert_page does not seem to increment the refcount */
    }
    if (mm) {
        //释放写锁
        up_write(&mm->mmap_sem);
        /*减少内存描述符(mm_struct)中的mm_users用户计数*/
        mmput(mm);
    }
    return 0;

free_range:
    for (page_addr = end - PAGE_SIZE; page_addr >= start;
         page_addr -= PAGE_SIZE) {
        page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];
        if (vma)
            zap_page_range(vma, (uintptr_t)page_addr +
                proc->user_buffer_offset, PAGE_SIZE, NULL);
err_vm_insert_page_failed:
        unmap_kernel_range((unsigned long)page_addr, PAGE_SIZE);
err_map_kernel_failed:
        __free_page(*page);
        *page = NULL;
err_alloc_page_failed:
        ;
    }
err_no_vma:
    if (mm) {
        up_write(&mm->mmap_sem);
        mmput(mm);
    }
    return -ENOMEM;
}
```

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

　　(4)内存映射完毕后，万事俱备，只欠数据了！通信双方都可以从内核缓存区读写数据，调用链条比较长，是这样的：binder\_ioctl() -> binder\_get\_thread() -> binder\_ioctl\_write\_read() -> binder\_thread\_write()/binder\_thread\_read()！最终执行的就是binder\_thread\_write和binder\_thread\_read方法了，这两个方法的核心思路也很简单：循环读取cmd，根据不同的cmd才取不同的动作；具体读写数据时，因为涉及到内核和用户进程之间的拷贝，最终调用的还是copy\_from\_user和copy\_to\_user。因为代码太长，我这里只截取少量核心代码展示：

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

```
/*循环读取cmd，根据不同的cmd才取不同的动作；具体读写数据时，因为涉及到内核和用户进程之间
的拷贝，最终调用的还是copy_from_user和copy_to_user*/
static int binder_thread_write(struct binder_proc *proc,
            struct binder_thread *thread,
            binder_uintptr_t binder_buffer, size_t size,
            binder_size_t *consumed)
{
    uint32_t cmd;
    void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
    //数据起始地址
    void __user *ptr = buffer + *consumed;
    //数据结束地址
    void __user *end = buffer + size;
    //可能有多个命令或数据要处理，需要循环
    while (ptr < end && thread->return_error == BR_OK) {
        //读取一个cmd命令
        if (get_user(cmd, (uint32_t __user *)ptr))
            return -EFAULT;
        //起始地址条过cmd命令占用的空间
        ptr += sizeof(uint32_t);
                 .................
                switch (cmd) {
        case BC_INCREFS:
        case BC_ACQUIRE:
        case BC_RELEASE:
                case BC_REPLY: {
            struct binder_transaction_data tr;
            //从3环用户态写数据到内核态，离不开copy_from_user和copy_to_user，任何情况都不例外
            if (copy_from_user(&tr, ptr, sizeof(tr)))
                return -EFAULT;
            ptr += sizeof(tr);
            binder_transaction(proc, thread, &tr, cmd == BC_REPLY);
            break;
        }
}    
```

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

　　binder\_thread\_read核心思路: **检查当前线程是否被唤醒。如果是，核心功能就是执行copy\_to\_user让3环的进程把数据拷贝走**！如果不是就继续retry循环，核心代码如下：**有个while死循环，在循环中探查是否有数据；如果没数据就回到retry继续等待唤醒！**

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

```
retry:
    // 获取将要处理的任务
    wait_for_proc_work = thread->transaction_stack == NULL &&
                list_empty(&thread->todo);
    if (wait_for_proc_work) {
        if (!(thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
                    BINDER_LOOPER_STATE_ENTERED))) {
            binder_user_error("binder: %d:%d ERROR: Thread waiting "
                "for process work before calling BC_REGISTER_"
                "LOOPER or BC_ENTER_LOOPER (state %x)\n",
                proc->pid, thread->pid, thread->looper);
            wait_event_interruptible(binder_user_error_wait,
                         binder_stop_on_user_error < 2);
        }
        binder_set_nice(proc->default_priority);
        if (non_block) {
            // 非阻塞且没有数据则返回 EAGAIN
            if (!binder_has_proc_work(proc, thread))
                ret = -EAGAIN;
        } else
            // 阻塞则进入睡眠状态，等待可操作的任务
            ret = wait_event_freezable_exclusive(proc->wait, binder_has_proc_work(proc, thread));
    } else {
        if (non_block) {
            if (!binder_has_thread_work(thread))
                ret = -EAGAIN;
        } else
            ret = wait_event_freezable(thread->wait, binder_has_thread_work(thread));
    }

    binder_lock(__func__);

    if (wait_for_proc_work)
        proc->ready_threads--;
    thread->looper &= ~BINDER_LOOPER_STATE_WAITING;

    if (ret)
        return ret;

    while (1) {
        uint32_t cmd;
        struct binder_transaction_data tr;
        struct binder_work *w;
        struct binder_transaction *t = NULL;

        // 获取 binder_work 对象
        if (!list_empty(&thread->todo))
            w = list_first_entry(&thread->todo, struct binder_work, entry);
        else if (!list_empty(&proc->todo) && wait_for_proc_work)
            w = list_first_entry(&proc->todo, struct binder_work, entry);
        else {
            if (ptr - buffer == 4 && !(thread->looper & BINDER_LOOPER_STATE_NEED_RETURN)) /* no data added没有数据就回到retry继续等 */
                goto retry;
            break;
        }     ..................    }
```

![复制代码](https://assets.cnblogs.com/images/copycode.gif)

    （5）上述通过读写公共内存通信的方式原理很简单，实现的时候还有点需要注意：记得介绍linux IPC时的poll么？ binder面临同样的问题：读取数据的进程是怎么知道公共内存有数据的了？这个问题是在binder\_transaction解决的：binder\_thread\_write函数执行完后会调用binder\_transaction，**最后两行代码就是唤醒目标线程了，刚好和binder\_thread\_read中的循环探查数据完美闭环**！

```
if (target_wait)
        // 唤醒目标线程
        wake_up_interruptible(target_wait);
```

总结:

1、整个过程**本质就是更改页表，让不同的虚拟地址映射到同一个物理地址**，和windows下用shadow  walker过PG保护的原理一模一样！

2、3环进程和内核之间拷贝数据用的还是copy\_from\_user和copy\_to\_user，一万年都不变的！

参考：

1、[https://www.bilibili.com/video/BV1Kf4y1z7kT?p=3&spm\_id\_from=pageDriver](https://www.bilibili.com/video/BV1Kf4y1z7kT?p=3&spm_id_from=pageDriver)  android为什么选binder

2、https://zhuanlan.zhihu.com/p/35519585  binder原理剖析

3、https://github.com/xdtianyu/SourceAnalysis/blob/master/Binder%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90.md binder源码分析

4、https://www.bilibili.com/video/BV1Ef4y127nU   手写binder进程通信框架

     https://www.bilibili.com/video/BV1Zp4y1Q7tZ/?spm\_id\_from=333.788.recommend\_more\_video.1

5、https://wangkuiwu.github.io/2014/09/02/Binder-Datastruct/  binder中的数据结构

6、https://blog.csdn.net/zhanshenwu/article/details/106458188  binder地址映射全解

7、https://blog.csdn.net/ljt326053002/article/details/105328384  binder源码分析
