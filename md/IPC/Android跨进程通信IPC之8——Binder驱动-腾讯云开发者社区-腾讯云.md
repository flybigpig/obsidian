原本是没有这篇文章的，因为原来写Binder的时候没打算写Binder驱动，不过我发现后面大量的代码都涉及到了Binder驱动，如果不讲解Binder驱动，可能会对大家理解Binder造成一些折扣，我后面还是加上了这篇文章。主要内容如下：

-   1、Binder驱动简述
-   2、Binder驱动的核心函数
-   3、Binder驱动的结构体
-   4、Binder驱动通信协议
-   5、Binder驱动内存
-   6、附录:关于misc

驱动层的原路径(这部分代码不在AOSP中，而是位于Linux内核代码中)

```
/kernel/drivers/android/binder.c
/kernel/include/uapi/linux/android/binder.h
```

> PS:我主要上面的源代码来分析。

或者

```
/kernel/drivers/staging/android/binder.c
/kernel/drivers/staging/android/uapi/binder.h
```

### 一、Binder驱动简述

##### (一)、 简述

Binder驱动是Android专用的，但底层的驱动架构与Linux驱动一样。Binder驱动在misc设备上进行注册，作为虚拟字符设备，没有直接操作硬件，只对设备内存做处理。主要工作是：

-   1、驱动设备的初始化(binder\_init)
-   2、打开(binder\_open)
-   3、映射(binder\_mmap)
-   4、数据操作(binder\_ioctl)。

如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/puwzzkslpq.png)

Binder驱动简述.png

##### (二)、系统调用

用户态的程序调用Kernel层驱动是需要陷入内核态，进行系统调用(system call，后面简写syscall)，比如打开Binder驱动方法的调用链为：open——> \_open()——> binder\_open() 。open() 为用户态的函数，\_open()便是系统调用(syscall)中的响应的处理函数，通过查找，调用内核态中对应的驱动binder\_open()函数，至于其他的从用户态陷入内核态的流程也基本一致。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/obhigbin2a.png)

image.png

简单的说，当用户空间调用open()函数，最终会调用binder驱动的binder\_open()函数；mmap()/ioctl()函数也是同理，Binder的系统中的用户态进入内核态都依赖系统调用过程。

##### (三) Binder驱动的四个核心方法

###### 3、binder\_init()函数

代码在[/kernel/drivers/android/binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199102&objectType=1&isNewArticle=undefined)

代码如下：

```
//kernel/drivers/android/binder.c      4216行
static int __init binder_init(void)
{
    int ret;
    //创建名为binder的工作队列
    binder_deferred_workqueue = create_singlethread_workqueue("binder");
    //   ****  省略部分代码  ****
    binder_debugfs_dir_entry_root = debugfs_create_dir("binder", NULL);
    if (binder_debugfs_dir_entry_root)
        binder_debugfs_dir_entry_proc = debugfs_create_dir("proc",
                         binder_debugfs_dir_entry_root);

    if (binder_debugfs_dir_entry_root) {
        //在debugfs文件系统中创建一系列的问题件
        //   ****  省略部分代码  ****
    }
     //   ****  省略部分代码  ****
    while ((device_name = strsep(&device_names, ","))) {
       //binder设备初始化
        ret = init_binder_device(device_name);
        if (ret)
            //binder设备初始化失败
            goto err_init_binder_device_failed;
    }
    return ret;
}
```

debugfs\_create\_dir是指在debugfs文件系统中创建一个目录，返回的是指向dentry的指针。当kernel中禁用debugfs的话，返回值是 -%ENODEV。默认是禁用的。如果需要打开，在目录/kernel/arch/arm64/configs/下找到目标defconfig文件中添加一行CONFIG\_DEBUG\_FS=y，再重新编译版本，即可打开debug\_fs。

###### 3.1 init\_binder\_device()函数解析

```
//kernel/drivers/android/binder.c      4189行
static int __init init_binder_device(const char *name)
{
    int ret;
    struct binder_device *binder_device;

    binder_device = kzalloc(sizeof(*binder_device), GFP_KERNEL);
    if (!binder_device)
        return -ENOMEM;

    binder_device->miscdev.fops = &binder_fops;
    binder_device->miscdev.minor = MISC_DYNAMIC_MINOR;
    binder_device->miscdev.name = name;

    binder_device->context.binder_context_mgr_uid = INVALID_UID;
    binder_device->context.name = name;
        //注册 misc设备
    ret = misc_register(&binder_device->miscdev);
    if (ret < 0) {
        kfree(binder_device);
        return ret;
    }
    hlist_add_head(&binder_device->hlist, &binder_devices);
    return ret;
}
```

这里面主要就是通过调用misc\_register()函数来注册misc设备，miscdevice结构体，便是前面注册misc设备时传递进去的参数

###### 3.1.1 binder\_device的结构体

这里要说一下binder\_device的结构体

```
//kernel/drivers/android/binder.c      234行
struct binder_device {
    struct hlist_node hlist;
    struct miscdevice miscdev;
    struct binder_context context;
};
```

在binder\_device里面有一个miscdevice

然后看下

```
//设备文件操作结构，这是file_operation结构
binder_device->miscdev.fops = &binder_fops;   
// 次设备号 动态分配
binder_device->miscdev.minor = MISC_DYNAMIC_MINOR;
// 设备名
binder_device->miscdev.name = name;
// uid
binder_device->context.binder_context_mgr_uid = INVALID_UID;
//上下文的名字
binder_device->context.name = name;
```

如果有人对[miscdevice](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Flinux%2Fmiscdevice.h&objectId=1199102&objectType=1&isNewArticle=undefined)结构体有兴趣 ,可以自行研究。

###### 3.1.2 file\_operations的结构体

file\_operations 结构体，指定相应文件操作的方法

```
/kernel/drivers/android/binder.c       4173行
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

###### 3.2 binder\_open()函数解析

开打binder驱动设备

```
/kernel/drivers/android/binder.c     3456行
static int binder_open(struct inode *nodp, struct file *filp)
{
        // binder进程
        struct binder_proc *proc;
        struct binder_device *binder_dev;
        binder_debug(BINDER_DEBUG_OPEN_CLOSE, "binder_open: %d:%d\n",
             current->group_leader->pid, current->pid);
        //为binder_proc结构体在再分配kernel内存空间
        proc = kzalloc(sizeof(*proc), GFP_KERNEL);
        if (proc == NULL)
             return -ENOMEM;
        get_task_struct(current);
        //当前线程的task保存在binder进程的tsk
        proc->tsk = current;
        proc->vma_vm_mm = current->mm;
        //初始化todo列表
        INIT_LIST_HEAD(&proc->todo);
         //初始化wait队列
        init_waitqueue_head(&proc->wait);
        // 当前进程的nice值转化为进程优先级
        proc->default_priority = task_nice(current);
        binder_dev = container_of(filp->private_data, struct binder_device,miscdev);
        proc->context = &binder_dev->context;
        //同步锁,因为binder支持多线程访问
        binder_lock(__func__);
        //BINDER_PROC对象创建+1
        binder_stats_created(BINDER_STAT_PROC);
        //将proc_node节点添加到binder_procs为表头的队列
        hlist_add_head(&proc->proc_node, &binder_procs);
        proc->pid = current->group_leader->pid;
        INIT_LIST_HEAD(&proc->delivered_death);
        //file文件指针的private_data变量指向binder_proc数据
        filp->private_data = proc;
        //释放同步锁
        binder_unlock(__func__);
        if (binder_debugfs_dir_entry_proc) {
        char strbuf[11];
        snprintf(strbuf, sizeof(strbuf), "%u", proc->pid);
/*
* proc debug entries are shared between contexts, so
* this will fail if the process tries to open the driver
* again with a different context. The priting code will
* anyway print all contexts that a given PID has, so this
* is not a problem.
*/
        proc->debugfs_entry =debugfs_create_file(strbuf,S_IRUGO,binder_debugfs_dir_entry_proc,(void *)(unsigned long)proc->pid,&binder_proc_fops);
    }
    return 0;
}
```

创建binder\_proc对象，并把当前进程等信息保存到binder\_proc对象，该对象管理IPC所需的各种新并有用其他结构体的跟结构体；再把binder\_proc对象保存到文件指针filp，以及binder\_proc加入到全局链表 **binder\_proc**。如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/ir4j3ywcm4.png)

binder\_proc.png

Binder驱动通过static HIST\_HEAD(binder\_procs)；，创建了全局的哈希链表binder\_procs，用于保存所有的binder\_procs队列，每次创建的binder\_proc对象都会加入binder\_procs链表中。

###### 3.3 binder\_mmap()函数解析

binder\_mmap(文件描述符，用户虚拟内存空间)

主要功能：首先在内核虚拟地址空间，申请一块与用户虚拟内存相同的大小的内存；然后申请1个page大小的物理内存，再讲同一块物理内存分别映射到内核虚拟内存空间和用户虚拟内存空间，从而实现了用户空间的buffer与内核空间的buffer同步操作的功能。

```
/kernel/drivers/android/binder.c      3357行
static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret;
    //内核虚拟空间
    struct vm_struct *area;
    struct binder_proc *proc = filp->private_data;
    const char *failure_string;
    struct binder_buffer *buffer;

    if (proc->tsk != current)
        return -EINVAL;
    //保证映射内存大小不超过4M
    if ((vma->vm_end - vma->vm_start) > SZ_4M)
        vma->vm_end = vma->vm_start + SZ_4M;

    binder_debug(BINDER_DEBUG_OPEN_CLOSE,
             "binder_mmap: %d %lx-%lx (%ld K) vma %lx pagep %lx\n",
             proc->pid, vma->vm_start, vma->vm_end,
             (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,
             (unsigned long)pgprot_val(vma->vm_page_prot));

    if (vma->vm_flags & FORBIDDEN_MMAP_FLAGS) {
        ret = -EPERM;
        failure_string = "bad vm_flags";
        goto err_bad_arg;
    }
    vma->vm_flags = (vma->vm_flags | VM_DONTCOPY) & ~VM_MAYWRITE;

    mutex_lock(&binder_mmap_lock);
    if (proc->buffer) {
        ret = -EBUSY;
        failure_string = "already mapped";
        goto err_already_mapped;
    }
     //分配一个连续的内核虚拟空间，与进程虚拟空间大小一致
    area = get_vm_area(vma->vm_end - vma->vm_start, VM_IOREMAP);
    if (area == NULL) {
        ret = -ENOMEM;
        failure_string = "get_vm_area";
        goto err_get_vm_area_failed;
    }
     //指向内核虚拟空间的地址
    proc->buffer = area->addr;
      //地址便宜量=用户空间地址-内核空间地址
    proc->user_buffer_offset = vma->vm_start - (uintptr_t)proc->buffer;
      // 释放锁
    mutex_unlock(&binder_mmap_lock);

#ifdef CONFIG_CPU_CACHE_VIPT
    if (cache_is_vipt_aliasing()) {
        while (CACHE_COLOUR((vma->vm_start ^ (uint32_t)proc->buffer))) {
            pr_info("binder_mmap: %d %lx-%lx maps %p bad alignment\n", proc->pid, vma->vm_start, vma->vm_end, proc->buffer);
            vma->vm_start += PAGE_SIZE;
        }
    }
#endif
        //分配物理页的指针数组，大小等于用户虚拟内存/4K
    proc->pages = kzalloc(sizeof(proc->pages[0]) * ((vma->vm_end - vma->vm_start) / PAGE_SIZE), GFP_KERNEL);
    if (proc->pages == NULL) {
        ret = -ENOMEM;
        failure_string = "alloc page array";
        goto err_alloc_pages_failed;
    }
    proc->buffer_size = vma->vm_end - vma->vm_start;

    vma->vm_ops = &binder_vm_ops;
    vma->vm_private_data = proc;
    // 分配物理页面，同时映射到内核空间和进程空间，目前只分配1个page的物理页
    if (binder_update_page_range(proc, 1, proc->buffer, proc->buffer + PAGE_SIZE, vma)) {
        ret = -ENOMEM;
        failure_string = "alloc small buf";
        goto err_alloc_small_buf_failed;
    }
     // binder_buffer对象，指向proc的buffer地址
    buffer = proc->buffer;
     //创建进程的buffers链表头
    INIT_LIST_HEAD(&proc->buffers);
     //将binder_buffer地址  加入到所属进程的buffer队列
    list_add(&buffer->entry, &proc->buffers);
    buffer->free = 1;
     //将空闲的buffer放入proc->free_buffer中
    binder_insert_free_buffer(proc, buffer);
      // 异步可用空间大小为buffer总体大小的一半
    proc->free_async_space = proc->buffer_size / 2;
    barrier();
    proc->files = get_files_struct(current);
    proc->vma = vma;
    proc->vma_vm_mm = vma->vm_mm;

    /*pr_info("binder_mmap: %d %lx-%lx maps %p\n",
         proc->pid, vma->vm_start, vma->vm_end, proc->buffer);*/
    return 0;
//错误跳转
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

binder\_mmap通过加锁，保证一次只有一个进程分享内存，保证多进程间的并发访问。其中user\_buffer\_offset是虚拟进程地址与虚拟内核地址的差值，也就是说同一物理地址，当内核地址为kernel\_addr，则进程地址为proc\_addr=kernel\_addr+user\_buffer\_offset。

这里面重点说下binder\_update\_page\_range()函数

###### 3.3.1 binder\_update\_page\_range()函数解析

```
/kernel/drivers/android/binder.c      567行
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
        mm = get_task_mm(proc->tsk);

    if (mm) {
        down_write(&mm->mmap_sem);
        vma = proc->vma;
        if (vma && mm != proc->vma_vm_mm) {
            pr_err("%d: vma mm and task mm mismatch\n",
                proc->pid);
            vma = NULL;
        }
    }

    if (allocate == 0)
        goto free_range;

    if (vma == NULL) {
        pr_err("%d: binder_alloc_buf failed to map pages in userspace, no vma\n",
            proc->pid);
        goto err_no_vma;
    }
    //  **************** 这里是重点********************
    for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
        int ret;

        page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];

        BUG_ON(*page);
         // 分配物理内存
        *page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO);
        if (*page == NULL) {
            pr_err("%d: binder_alloc_buf failed for page at %p\n",
                proc->pid, page_addr);
            goto err_alloc_page_failed;
        }
        // 物理空间映射到内核空间
        ret = map_kernel_range_noflush((unsigned long)page_addr,
                    PAGE_SIZE, PAGE_KERNEL, page);
        flush_cache_vmap((unsigned long)page_addr,
                (unsigned long)page_addr + PAGE_SIZE);
        if (ret != 1) {
            pr_err("%d: binder_alloc_buf failed to map page at %p in kernel\n",
                   proc->pid, page_addr);
            goto err_map_kernel_failed;
        }
        user_page_addr =
            (uintptr_t)page_addr + proc->user_buffer_offset;
         // 物理空间映射到虚拟进程空间
        ret = vm_insert_page(vma, user_page_addr, page[0]);
        if (ret) {
            pr_err("%d: binder_alloc_buf failed to map page at %lx in userspace\n",
                   proc->pid, user_page_addr);
            goto err_vm_insert_page_failed;
        }
        /* vm_insert_page does not seem to increment the refcount */
    }
    if (mm) {
        up_write(&mm->mmap_sem);
        mmput(mm);
    }
    return 0;

free_range:
     
    for (page_addr = end - PAGE_SIZE; page_addr >= start;
         page_addr -= PAGE_SIZE) {
        page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];
        if (vma)
            zap_page_range(vma, (uintptr_t)page_addr +
                proc->user_buffer_offset, PAGE_SIZE);
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

主要工作可用下面的图来表达：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/0bf46w9i6j.png)

binder\_update\_page\_range.png

binder\_update\_page\_range 主要完成工作：分配物理内存空间，将物理空间映射到内核空间，将物理空间映射到进程空间。当然binder\_update\_page\_range 既可以分配物理页面，也可以释放物理页面

###### 3.4 binder\_ioctl()函数解析

在分析binder\_ioctl()函数之前，建议大家看下我的上篇文章[Android跨进程通信IPC之7——Binder相关结构体简介](https://cloud.tencent.com/developer/article/1199100?from_column=20421&from=20421)了解相关的结构体，为了便于查找，这些结构体之间都留有字段的存储关联的结构，下面的这幅图描述了这里说到的内容

![](https://ask.qcloudimg.com/http-save/yehe-2957818/vt08j1kze5.png)

相关结构体.png

现在让我们详细分析下binder\_ioctl()函数

-   binder\_ioctl()函数负责在两个进程间收发IPC数据和IPC reply数据
-   ioctl(文件描述符，ioctl命令，数据类型)
-   ioctl文件描述符，是通过open()方法打开Binder Driver后返回值。
-   ioctl命令和数据类型是一体，不同的命令对应不同的数据类型

下面这些命令中BINDER\_WRITE\_READ使用最为频繁，也是ioctl的最为核心的命令。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/7q4aoy1vnj.png)

ioctl.png

代码如下：

```
//kernel/drivers/android/binder.c       3239行
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret;
    struct binder_proc *proc = filp->private_data;
        //binder线程
    struct binder_thread *thread;
    unsigned int size = _IOC_SIZE(cmd);
    void __user *ubuf = (void __user *)arg;

    /*pr_info("binder_ioctl: %d:%d %x %lx\n",
            proc->pid, current->pid, cmd, arg);*/

    if (unlikely(current->mm != proc->vma_vm_mm)) {
        pr_err("current mm mismatch proc mm\n");
        return -EINVAL;
    }
    trace_binder_ioctl(cmd, arg);
        //进入休眠状态，直到中断唤醒
    ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
    if (ret)
        goto err_unlocked;

    binder_lock(__func__);
        //获取binder_thread
    thread = binder_get_thread(proc);
    if (thread == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    switch (cmd) {
        //进行binder的读写操作
    case BINDER_WRITE_READ:
        ret = binder_ioctl_write_read(filp, cmd, arg, thread);
        if (ret)
            goto err;
        break;
        //设置binder的最大支持线程数
    case BINDER_SET_MAX_THREADS:
        if (copy_from_user(&proc->max_threads, ubuf, sizeof(proc->max_threads))) {
            ret = -EINVAL;
            goto err;
        }
        break;
        //设置binder的context管理者，也就是servicemanager称为守护进程
    case BINDER_SET_CONTEXT_MGR:
        ret = binder_ioctl_set_ctx_mgr(filp);
        if (ret)
            goto err;
        break;
        //当binder线程退出，释放binder线程
    case BINDER_THREAD_EXIT:
        binder_debug(BINDER_DEBUG_THREADS, "%d:%d exit\n",
                 proc->pid, thread->pid);
        binder_free_thread(proc, thread);
        thread = NULL;
        break;
         //获取binder的版本号
    case BINDER_VERSION: {
        struct binder_version __user *ver = ubuf;

        if (size != sizeof(struct binder_version)) {
            ret = -EINVAL;
            goto err;
        }
        if (put_user(BINDER_CURRENT_PROTOCOL_VERSION,
                 &ver->protocol_version)) {
            ret = -EINVAL;
            goto err;
        }
        break;
    }
    default:
        ret = -EINVAL;
        goto err;
    }
    ret = 0;
err:
    if (thread)
        thread->looper &= ~BINDER_LOOPER_STATE_NEED_RETURN;
    binder_unlock(__func__);
    wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
    if (ret && ret != -ERESTARTSYS)
        pr_info("%d:%d ioctl %x %lx returned %d\n", proc->pid, current->pid, cmd, arg, ret);
err_unlocked:
    trace_binder_ioctl_done(ret);
    return ret;
}
```

这里面重点说两个函数binder\_get\_thread()函数和binder\_ioctl\_write\_read()函数

###### 3.4.1 binder\_get\_thread()函数解析

> 从binder\_proc中查找binder\_thread，如果当前线程已经加入到proc的线程队列则直接返回，如果不存在则创建binder\_thread，并将当前线程添加到当前的proc

```
//kernel/drivers/android/binder.c      3026行
static struct binder_thread *binder_get_thread(struct binder_proc *proc)
{
    struct binder_thread *thread = NULL;
    struct rb_node *parent = NULL;
    struct rb_node **p = &proc->threads.rb_node;
        //根据当前进程的pid，从binder_proc中查找相应的binder_thread
    while (*p) {
        parent = *p;
        thread = rb_entry(parent, struct binder_thread, rb_node);

        if (current->pid < thread->pid)
            p = &(*p)->rb_left;
        else if (current->pid > thread->pid)
            p = &(*p)->rb_right;
        else
            break;
    }
    if (*p == NULL) {
                // 新建binder_thread结构体
        thread = kzalloc(sizeof(*thread), GFP_KERNEL);
        if (thread == NULL)
            return NULL;
        binder_stats_created(BINDER_STAT_THREAD);
        thread->proc = proc;
                //保持当前进程(线程)的pid
        thread->pid = current->pid;
        init_waitqueue_head(&thread->wait);
        INIT_LIST_HEAD(&thread->todo);
        rb_link_node(&thread->rb_node, parent, p);
        rb_insert_color(&thread->rb_node, &proc->threads);
        thread->looper |= BINDER_LOOPER_STATE_NEED_RETURN;
        thread->return_error = BR_OK;
        thread->return_error2 = BR_OK;
    }
      return thread;
}
```

###### 3.4.2 binder\_ioctl\_write\_read()函数解析

> 对于ioctl()方法中，传递进来的命令是cmd = BINDER\_WRITE\_READ时执行该方法，arg是一个binder\_write\_read结构体

```
/kernel/drivers/android/binder.c      3134行
static int binder_ioctl_write_read(struct file *filp,
                unsigned int cmd, unsigned long arg,
                struct binder_thread *thread)
{
    int ret = 0;
    struct binder_proc *proc = filp->private_data;
    unsigned int size = _IOC_SIZE(cmd);
    void __user *ubuf = (void __user *)arg;
    struct binder_write_read bwr;

    if (size != sizeof(struct binder_write_read)) {
        ret = -EINVAL;
        goto out;
    }
         //把用户控件数据ubuf拷贝到bwr
    if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {
        ret = -EFAULT;
        goto out;
    }
    binder_debug(BINDER_DEBUG_READ_WRITE,
             "%d:%d write %lld at %016llx, read %lld at %016llx\n",
             proc->pid, thread->pid,
             (u64)bwr.write_size, (u64)bwr.write_buffer,
             (u64)bwr.read_size, (u64)bwr.read_buffer);
        //如果写缓存中有数据
    if (bwr.write_size > 0) {
                //执行binder写操作
        ret = binder_thread_write(proc, thread,
                      bwr.write_buffer,
                      bwr.write_size,
                      &bwr.write_consumed);
        trace_binder_write_done(ret);
                //如果写失败，再将bwr数据写回用户空间，并返回
        if (ret < 0) {
            bwr.read_consumed = 0;
            if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
                ret = -EFAULT;
            goto out;
        }
    }
        //当读缓存有数据
    if (bwr.read_size > 0) {
                 //执行binder读操作
        ret = binder_thread_read(proc, thread, bwr.read_buffer,
                     bwr.read_size,
                     &bwr.read_consumed,
                     filp->f_flags & O_NONBLOCK);
        trace_binder_read_done(ret);
        if (!list_empty(&proc->todo))
            wake_up_interruptible(&proc->wait);
                 //如果读失败
        if (ret < 0) {
                        //当读失败，再将bwr数据写回用户空间，并返回
            if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
                ret = -EFAULT;
            goto out;
        }
    }
    binder_debug(BINDER_DEBUG_READ_WRITE,
             "%d:%d wrote %lld of %lld, read return %lld of %lld\n",
             proc->pid, thread->pid,
             (u64)bwr.write_consumed, (u64)bwr.write_size,
             (u64)bwr.read_consumed, (u64)bwr.read_size);
         //将内核数据bwr拷贝到用户空间ubuf
    if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {
        ret = -EFAULT;
        goto out;
    }
out:
    return ret;
}
```

对于binder\_ioctl\_write\_read流程图如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/vksxtuemnn.png)

binder\_ioctl\_write\_read流程图.png

流程：

-   1 首先把用户空间的数据拷贝到内核空间bwr
-   2 其次当bwr写缓存中有数据，则执行binder写操作。如果写失败，则再将bwr数据写回用户控件，并退出。
-   3 再次当bwr读缓存中有数据，则执行binder读缓存；当读失败，再将bwr数据写会用户空间，并退出。
-   4 最后把内核数据拷贝到用户空间。

##### 3.5 总结

本章主要讲解了binder驱动的的四大功能点

-   1 binder\_init ：初始化字符设备
-   2 binder\_open：打开驱动设备，过程需要持有binder\_main\_lock同步锁
-   3 binder\_mmap: 申请内存空间，该过程需要持有binder\_mmap\_lock同步锁；
-   4 binder\_ioctl：执行相应的io操作，该过程需要持有binder\_main\_lock同步锁；当处于binder\_thread\_read过程，却读缓存无数据则会先释放该同步锁，并处于wait\_event\_freezable过程，等有数据到来则唤醒并尝试持有同步锁。

下面我们重点介绍下binder\_thread\_write()函数和binder\_thread\_read()函数

### 四 Binder驱动通信

##### (一) Binder驱动通信简述

> Client进程通过RPC(Remote Procedure Call Protocol) 与Server通信，可以简单地划分为三层: 1、驱动层 2、IPC层 3、业务层。**下图的doAction()**便是Client与Server共同协商好的统一方法；其中handle、PRC数据、代码、协议、这4项组成了IPC层的数据，通过IPC层进行数据传输；而真正在Client和Server两端建立通信的基础是Binder Driver

模型如下图:

![](https://ask.qcloudimg.com/http-save/yehe-2957818/64hrnt997g.png)

binder通信模型.png

##### (二) Binder驱动通信协议

先来一发完整的Binder通信过程，如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/17r7xz229x.png)

binder流程.png

Binder协议包含在IPC数据中，分为两类：

-   BINDER\_COMMAND\_PROTOCOL:binder请求码，以"BC\_" 开头，简称"BC码"，从IPC层传递到 Binder Driver层；
-   BINDER\_RETURN\_PROTOCOL: binder响应码，以"BR\_"开头，简称"BR码"，用于从BinderDirver层传递到IPC层

Binder IPC 通信至少是两个进程的交互:

-   client进程执行binder\_thread\_write，根据BC\_XXX 命令，生成相应的binder\_work；
-   server进程执行binder\_thread\_read，根据binder\_work.type类型，生成BR\_XXX，发送用户处理。

如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/vl5revtlqz.png)

进程.png

其中binder\_work.type共有6种类型

```
//kernel/drivers/android/binder.c      240行
struct binder_work {
    struct list_head entry;
    enum {
        BINDER_WORK_TRANSACTION = 1
        BINDER_WORK_TRANSACTION_COMPLETE,
        BINDER_WORK_NODE,
        BINDER_WORK_DEAD_BINDER,
        BINDER_WORK_DEAD_BINDER_AND_CLEAR,
        BINDER_WORK_CLEAR_DEATH_NOTIFICATION,
    } type;
};
```

这里用到了上面提到两个函数一个是binder\_thread\_write()函数，另一个是binder\_thread\_read函数骂我们就来详细研究下：

##### (三)、通信函数

###### 1、binder\_thread\_write() 函数详解

> 请求处理过程是通过binder\_thread\_write()函数，该方法用于处理Binder协议的请求码。当binder\_buffer存在数据，binder线程的写操作循环执行 代码在 kernel/drivers/android/binder.c 2248行。

代码太多了就不全部粘贴了，只粘贴重点部分，代码如下：

```
static int binder_thread_write(){
    while (ptr < end && thread->return_error == BR_OK) {
        //获取IPC数据中的Binder协议的BC码
        if (get_user(cmd, (uint32_t __user *)ptr))
            return -EFAULT;
        switch (cmd) {
            case BC_INCREFS: ...
            case BC_ACQUIRE: ...
            case BC_RELEASE: ...
            case BC_DECREFS: ...
            case BC_INCREFS_DONE: ...
            case BC_ACQUIRE_DONE: ...
            case BC_FREE_BUFFER: ...
            
            case BC_TRANSACTION:
            case BC_REPLY: {
                struct binder_transaction_data tr;
                //拷贝用户控件tr到内核
                copy_from_user(&tr, ptr, sizeof(tr))；
                binder_transaction(proc, thread, &tr, cmd == BC_REPLY);
                break;
            case BC_REGISTER_LOOPER: ...
            case BC_ENTER_LOOPER: ...
            case BC_EXIT_LOOPER: ...
            case BC_REQUEST_DEATH_NOTIFICATION: ...
            case BC_CLEAR_DEATH_NOTIFICATION:  ...
            case BC_DEAD_BINDER_DONE: ...
            }
        }
    }
}
```

对于 请求码为BC\_TRANSCATION或BC\_REPLY时，会执行binder\_transaction()方法，这是最频繁的操作。那我们一起来看下binder\_transaction()函数。

###### 1.1、binder\_transaction() 函数详解

这块的代码依旧很多，我就只粘贴重点了，全部代码在/kernel/drivers/android/binder.c 1827行。

```
static void binder_transaction(struct binder_proc *proc,
               struct binder_thread *thread,
               struct binder_transaction_data *tr, int reply){
    //根绝各种判断，获取如下信息：
    //目标进程
    struct binder_proc *target_proc；  
    // 目标线程
    struct binder_thread *target_thread； 
     // 目标binder节点
    struct binder_node *target_node；    
    //目标TODO队列
    struct list_head *target_list；
    // 目标等待队列     
    wait_queue_head_t *target_wait；    
   //*** 省略部分代码 ***
   
    //分配两个结构体内存
    t = kzalloc(sizeof(*t), GFP_KERNEL);
    if (t == NULL) {
        return_error = BR_FAILED_REPLY;
        goto err_alloc_t_failed;
    }
    binder_stats_created(BINDER_STAT_TRANSACTION);
    *tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);
    //*** 省略部分代码 ***
    //从target_proc分配一块buffer
    t->buffer = binder_alloc_buf(target_proc, tr->data_size,
     //*** 省略部分代码 ***
    for (; offp < off_end; offp++) {
     //*** 省略部分代码 ***
        switch (fp->type) {
        case BINDER_TYPE_BINDER: ...
        case BINDER_TYPE_WEAK_BINDER: ...
        case BINDER_TYPE_HANDLE: ...
        case BINDER_TYPE_WEAK_HANDLE: ...
        case BINDER_TYPE_FD: ...
        }
    }
    //分别给target_list和当前线程TODO队列插入事务
    t->work.type = BINDER_WORK_TRANSACTION;
    list_add_tail(&t->work.entry, target_list);
    tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
    list_add_tail(&tcomplete->entry, &thread->todo);
    if (target_wait)
        wake_up_interruptible(target_wait);
    return;
}
```

replay的过程会找到 target\_thread，非reply则一般找到target\_proc，对于特殊的嵌套binder call会根据transaction\_stack来决定是否插入事物到目标进程。

###### 1.2、BC\_PROTOCOL 请求码

> Binder请求码，在[binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199102&objectType=1&isNewArticle=undefined)里面的366行。是用**binder\_driver\_command\_protocol** 来定义的，是用于应用程序向Binder驱动设备发送请求消息，应用程序包含Client端和Server端，以BC\_开头，供给17条 ( - 表示目前不支持请求码)

![](https://ask.qcloudimg.com/http-save/yehe-2957818/76j6doo33x.png)

BC请求码.png

重点说几个：

-   BC\_FREE\_BUFFER:通过mmap()映射内存，其中ServiceMananger映射的空间大小为128K，其他Binder应用的进程映射的内存大小为8K-1M，Binder驱动基于这块映射的内存采用最佳匹配算法来动态分配和释放，通过**binder\_buffer**结构体中**free**字段来表示相应的buffer是空闲还是已分配状态。对于已分配的buffer加入binder\_proc中的allocated\_buffers红黑树；对于空闲的buffers加入到binder\_proch中的free\_buffer红黑树。当应用程序需要内存时，根据所需内存大小从free\_buffers中找到最合适的内存，并放入allocated\_buffers树；当应用程序处理完后，必须尽快用BC\_FREE\_BUFFER命令来释放该buffer，从而添加回free\_buffers树中。
-   BC\_INCREFS、BC\_ACQUIRE、BC\_RELEASE、BC\_DECREFS等请求码的作用是对binder的 强弱引用的技术操作，用于实现强/弱 指针的功能。
-   对于参数类型 binder\_ptr\_cookie是由binder指针和cookie组成
-   Binder线程创建与退出：
-   BC\_ENTER\_LOOPER: binder主线程(由应用层发起)的创建会向驱动发送该消息；joinThreadPool()过程创建binder主线程；
-   BC\_REGISTER\_LOOPER: Binder用于驱动层决策而创建新的binder线程；joinThreadPool()过程，创建非binder主线程；
-   BC\_EXIT\_LOOPER:退出Binder线程，对于binder主线程是不能退出的；joinThreadPool的过程出现timeout，并且非binder主线程，则会退出该binder线程。

###### 1.3、binder\_thread\_read()函数详解

响应处理过程是通过binder\_thread\_read()函数，该方法根据不同的binder\_work->type以及不同的状态生成不同的响应码

代码太多了，我只截取了部分代码，具体代码在/kernel/drivers/android/binder.c 2650行

```
static int binder_thread_read（）{
     //*** 省略部分代码 ***
    // 根据 wait_for_proc_work 来决定wait在当前线程还是进程的等待队列
    if (wait_for_proc_work) {
         //*** 省略部分代码 ***
        ret = wait_event_freezable_exclusive(proc->wait, binder_has_proc_work(proc, thread));
          //*** 省略部分代码 ***
    } else {
        //*** 省略部分代码 ***
        ret = wait_event_freezable(thread->wait, binder_has_thread_work(thread));
       //*** 省略部分代码 ***
    }
   //*** 省略部分代码 ***
    while (1) {
         //*** 省略部分代码 ***
         //当&thread->todo和&proc->todo都为空时，goto到retry标志处，否则往下执行：
    if (!list_empty(&thread->todo)) {
        w = list_first_entry(&thread->todo, struct binder_work,entry);
    } else if (!list_empty(&proc->todo) && wait_for_proc_work) {
        w = list_first_entry(&proc->todo, struct binder_work,entry);
    } else {
        /* no data added */
        if (ptr - buffer == 4 &&
             !(thread->looper & BINDER_LOOPER_STATE_NEED_RETURN))
                goto retry;
        break;
    }
        struct binder_transaction_data tr;
        struct binder_transaction *t = NULL;
        switch (w->type) {
          case BINDER_WORK_TRANSACTION: ...
          case BINDER_WORK_TRANSACTION_COMPLETE: ...
          case BINDER_WORK_NODE: ...
          case BINDER_WORK_DEAD_BINDER: ...
          case BINDER_WORK_DEAD_BINDER_AND_CLEAR: ...
          case BINDER_WORK_CLEAR_DEATH_NOTIFICATION: ...
        }
        ...
    }
done:
    *consumed = ptr - buffer;
        //当满足请求线程  加上 已准备线程数 等于0  并且 其已启动线程小于最大线程数15，并且looper状态为已注册或已进入时创建新的线程。
    if (proc->requested_threads + proc->ready_threads == 0 &&
        proc->requested_threads_started < proc->max_threads &&
        (thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
         BINDER_LOOPER_STATE_ENTERED)) /* the user-space code fails to */
         /*spawn a new thread if we leave this out */) {
        proc->requested_threads++;
        binder_debug(BINDER_DEBUG_THREADS,
                 "%d:%d BR_SPAWN_LOOPER\n",
                 proc->pid, thread->pid);
                //生成BR_SPAWN_LOOPER命令，用于创建新的线程
        if (put_user(BR_SPAWN_LOOPER, (uint32_t __user *)buffer))
            return -EFAULT;
        binder_stat_br(proc, thread, BR_SPAWN_LOOPER);
    }
    return 0;
}
```

当transaction堆栈为空，且线程todo链表为空，且non\_block=false时，意味着没有任务事物需要处理，会进入等待客户端请求的状态。当有事物需要处理时便会进入循环处理过程，并生成相应的响应码。

> 在Binder驱动层面，只有进入binder\_thread\_read()方法时，同时满足以下条件，才会生成BR\_SPAWN\_LOOPER命令，当用户态进程收到该命令则会创新新线程：

-   binder\_proc的requested\_threads线程数为0
-   binder\_proc的ready\_threads线程数为0
-   binder\_proc的requested\_threads\_started个数小于15(即最大线程个数)
-   binder\_thread的looper状态为BINDER\_LOOPER\_STATE\_REGISTERED或者BINDER\_LOOPER\_STATE\_ENTERED

那么问题来了，什么时候处理的响应码？通过上面的Binder通信协议图，可以知道处理响应码的过程是在用户态处理，下一篇文章会讲解到用户控件IPCThreadState类中IPCThreadState::waitForResponse()和IPCThreadState::executeCommand()两个方法共同处理Binder协议的18个响应码

###### 1.3、BR\_PROTOCOL 响应码

> binder响应码，在[binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199102&objectType=1&isNewArticle=undefined)里面的278行是用enum binder\_driver\_return\_protocol来定义的，是binder设备向应用程序回复的消息，应用程序包含Client端和Serve端，以BR\_开头，合计18条。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/ht9jun65jg.png)

BR响应码.png

说几个难点：

-   BR\_SPAWN\_LOOPER:binder驱动已经检测到进程中没有线程等待即将到来的事物。那么当一个进程接收到这条命令时，该进程必须创建一条新的服务线程并注册该线程，在接下来的响应过程会看到合何时生成该响应嘛
-   BR\_TRANSACTION\_COMPLETE:当Client端向Binder驱动发送BC\_TRANSACTION命令后，Client会受到BR\_TRANSACTION\_COMPLETE命令，告知Client端请求命令发送成功能；对于Server向Binder驱动发送BC\_REPLY命令后，Server端会受到BR\_TRANSACTION\_COMPLETE命令，告知Server端请求回应命令发送成功。
-   BR\_READ\_REPLY: 当应用层向Binder驱动发送Binder调用时，若Binder应用层的另一个端已经死亡，则驱动回应BR\_DEAD\_BINDER命令。
-   BR\_FAILED\_REPLY:当应用层向Binder驱动发送Binder调用是，若transcation出错，比如调用的函数号不存在，则驱动回应BR\_FAILED\_REPLY。

### 五、Binder内存

为了让大家更好的理解Binder机制，特意插播一条"广告"，主要是讲解Binder内存，Binder内存我主要分3个内容来依次讲解，分别为：

-   1、mmap机制
-   2、内存分配
-   3、内存释放

##### (一) mmap机制

上面从代码的角度阐释了binder\_mmap，也是Binder进程通信效率高的核心机制所在，如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/q9qcienfsa.png)

mmap机制1.png

![](https://ask.qcloudimg.com/http-save/yehe-2957818/tm3stbkkk3.png)

mmap机制2.png

虚拟进程地址空间(vm\_area\_struct)和虚拟内核地址空间(vm\_struct)都映射到同一块物理内存空间。当Client端与Server端发送数据时，Client(作为数据发送端)先从自己的进程空间把IPC通信数据copy\_from\_user拷贝到内核空间，而Server端(作为数据接收端)与内核共享数据，不再需要拷贝数据，而是通过内存地址空间的偏移量，即可获悉内存地址，整个过程只要发生一次内存拷贝。一般地做法，需要Client端进程空间拷贝到内核空间，再由内核空间拷贝到Server进程，会发生两次拷贝。

> 对于进程和内核虚拟地址映射到同一个物理内存的操作是发生在数据接收端，而数据发送端还是需要将用户态的数据复制到内核态。到此，可能会有同学会问，为什么不直接让发送端和接收端直接映射到同一个物理空间，这样不就是连一次复制操作都不需要了，0次复制操作就是与Linux标准内核的共享内存你的IPC机制没有区别了，对于共享内存虽然效率高，但是对于多进程的同步问题比较复杂，而管道/[消息队列](https://cloud.tencent.com/product/message-queue-catalog?from_column=20065&from=20065)等IPC需要复制2次，效率低。关于Linux效率的对比，请看前面的文章。总之Android选择Binder是基于速度和安全性的考虑。

下面这图是从Binder在进程间数据通信的流程图，从图中更能明白Binder的内存转移关系。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/4s02xbpfjv.png)

binder内存转移.png

##### (二) 内存分配

> Binder内存分配方法通过binder\_alloc\_buf()方法，内存管理单元为binder\_buffer结构体，只有在binder\_transaction过程中才需要分配buffer。具体代码在/kernel/drivers/android/binder.c 678行，我选取了重点部分

```
static struct binder_buffer *binder_alloc_buf(struct binder_proc *proc,
                          size_t data_size, size_t offsets_size, int is_async)
{ 
     //*** 省略部分代码 ***
      //如果不存在虚拟地址空间为，则直接返回
    if (proc->vma == NULL) {
        pr_err("%d: binder_alloc_buf, no vma\n",
               proc->pid);
        return NULL;
    }
        
    data_offsets_size = ALIGN(data_size, sizeof(void *)) +
        ALIGN(offsets_size, sizeof(void *));
        //非法的size，直接返回
    if (data_offsets_size < data_size || data_offsets_size < offsets_size) {
        binder_user_error("%d: got transaction with invalid size %zd-%zd\n",
                proc->pid, data_size, offsets_size);
        return NULL;
    }
    size = data_offsets_size + ALIGN(extra_buffers_size, sizeof(void *));
        //非法的size，直接返回
    if (size < data_offsets_size || size < extra_buffers_size) {
        binder_user_error("%d: got transaction with invalid extra_buffers_size %zd\n",
                  proc->pid, extra_buffers_size);
        return NULL;
    }
        //如果 剩余的异步空间太少，以至于满足需求，也直接返回
    if (is_async &&
        proc->free_async_space < size + sizeof(struct binder_buffer)) {
        binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
                 "%d: binder_alloc_buf size %zd failed, no async space left\n",
                  proc->pid, size);
        return NULL;
    }
    //从binder_buffer的红黑树丛中，查找大小相等的buffer块
        while (n) {
        buffer = rb_entry(n, struct binder_buffer, rb_node);
        BUG_ON(!buffer->free);
        buffer_size = binder_buffer_size(proc, buffer);

        if (size < buffer_size) {
            best_fit = n;
            n = n->rb_left;
        } else if (size > buffer_size)
            n = n->rb_right;
        else {
            best_fit = n;
            break;
        }
    }
   //如果内存分配失败，地址为空
    if (best_fit == NULL) {
        pr_err("%d: binder_alloc_buf size %zd failed, no address space\n",
            proc->pid, size);
        return NULL;
    }
    if (n == NULL) {
        buffer = rb_entry(best_fit, struct binder_buffer, rb_node);
        buffer_size = binder_buffer_size(proc, buffer);
    }
    binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
             "%d: binder_alloc_buf size %zd got buffer %p size %zd\n",
              proc->pid, size, buffer, buffer_size);

    has_page_addr =
        (void *)(((uintptr_t)buffer->data + buffer_size) & PAGE_MASK);
    if (n == NULL) {
        if (size + sizeof(struct binder_buffer) + 4 >= buffer_size)
            buffer_size = size; /* no room for other buffers */
        else
            buffer_size = size + sizeof(struct binder_buffer);
    }
    end_page_addr =
        (void *)PAGE_ALIGN((uintptr_t)buffer->data + buffer_size);
    if (end_page_addr > has_page_addr)
        end_page_addr = has_page_addr;
    if (binder_update_page_range(proc, 1,
        (void *)PAGE_ALIGN((uintptr_t)buffer->data), end_page_addr, NULL))
        return NULL;

    rb_erase(best_fit, &proc->free_buffers);
    buffer->free = 0;
    binder_insert_allocated_buffer(proc, buffer);
    if (buffer_size != size) {
        struct binder_buffer *new_buffer = (void *)buffer->data + size;

        list_add(&new_buffer->entry, &buffer->entry);
        new_buffer->free = 1;
        binder_insert_free_buffer(proc, new_buffer);
    }
 binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
             "%d: binder_alloc_buf size %zd got %p\n",
              proc->pid, size, buffer);
    buffer->data_size = data_size;
    buffer->offsets_size = offsets_size;
    buffer->extra_buffers_size = extra_buffers_size;
    buffer->async_transaction = is_async;
    if (is_async) {
        proc->free_async_space -= size + sizeof(struct binder_buffer);
        binder_debug(BINDER_DEBUG_BUFFER_ALLOC_ASYNC,
                 "%d: binder_alloc_buf size %zd async free %zd\n",
                  proc->pid, size, proc->free_async_space);
    }
    return buffer;
}
```

##### (三)、内存释放

内存释放相关函数：

-   binder\_free\_buf() : 在/kernel/drivers/android/binder.c 847行
-   binder\_delete\_free\_buffer()：在/kernel/drivers/android/binder.c 802行
-   binder\_transaction\_buffer\_release()：在/kernel/drivers/android/binder.c 1430行

大家有兴趣可以自己去研究下，这里就不详细解说了。

### 六、附录:关于misc

##### (一)、linux子系统-miscdevice

> 杂项设备(miscdevice) 是嵌入式系统中用的比较多的一种设备类型。在Linux驱动中把无法归类的五花八门的设备定义为混杂设备(用miscdevice结构体表述)。Linux内核所提供的miscdevice有很强的包容性，各种无法归结为标准字符设备的类型都可以定义为miscdevice，譬如NVRAM,看门狗，实时时钟，字符LCD等，就像一组大杂烩。

在Linux内核里把所有misc设备组织在一起，构成一个子系统(subsys)，统一进行管理。在这个子系统里的所有miscdevice类型的设备共享一个主设备号MISC\_MAJOR(即10)，这次设备号不同。

在内核中用struct miscdevice表示miscdevice设备，具体的定义在内核头文件"include/linux/miscdevice.h"中

```
//https://github.com/torvalds/linux/blob/master/include/linux/miscdevice.h    63行
struct miscdevice  {
    int minor;
    const char *name;
    const struct file_operations *fops;
    struct list_head list;
    struct device *parent;
    struct device *this_device;
    const char *nodename;
    mode_t mode;
};
```

miscdevice的API的实现在[drivers/char/misc.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fchar%2Fmisc.c&objectId=1199102&objectType=1&isNewArticle=undefined)中，misc子系统的初始化，以及misc设备的注册，注销等接口都实现在这个文件中。通过阅读这个文件，miscdevice类型的设备实际上就是对字符设备的简单封装，最直接的证据可以看misdevice子系统的初始化函数misc\_init()，在这个函数里，撇开其他代码不看，其中有如下两行关键代码:

```
//drivers/char/misc.c  279行
...
misc_class = class_create(THIS_MODULE, "misc");
...
if (register_chrdev(MISC_MAJOR,"misc",&misc_fops))
```

代码解析：

-   第一行，创建了一个名字叫misc的类，具体表现是在/sys/class目录下创建一个名为misc的目录。以后美注册一个自己的miscdevice都会在该目录下新建一项。
-   第二行，调用register\_chrdev为给定的主设备号MISC\_MAJOR(10)注册0~255供256个次设备号，并为每个设备建立一个对应的默认的cdev结构，该函数是2.6内核之前的老函数，现在已经不建议使用了。由此可见misc设备其实也就是主设备号是MISC\_MAJOR(10)的字符设备。从面相对象的角度来看，字符设备类是misc设备类的父类。同时我们也主要注意到采用这个函数注册后实际上系统最多支持有255个驱动自定义的杂项设备，因为杂项设备子系统模块自己占用了一个次设备号0。查看源文件可知，目前内核里已经被预先使用的子设备号定义在include/linux/miscdevice.h的开头

在这个子系统中所有的miscdevice设备形成了一个链表，他们共享一个主设备号10，但它们有不同的次设备号。对设备访问时内核根据次设备号查找对应的miscdevice设备。这一点我们可以通过阅读misc子系统提供的注册接口函数misc\_register()和注销接口函数misc\_deregister()来理解。这就不深入讲解了，有兴趣的可以自己去研究。

在这个子系统中所有的miscdevice设备不仅共享了主设备号，还共享了一个misc\_open()的文件操作方法。

```
//drivers/char/misc.c       161行
static const struct file_operations misc_fops = {
    .owner        = THIS_MODULE,
    .open        = misc_open,
};
```

该方法结构在注册设备时通过register\_chrdev(MISC\_MAJOR,"misc",&misc\_fops)传递给内核。但不要以为所有的miscdevice都使用相同的文件open方法，仔细阅读misc\_open()我们发现该函数内部会检查驱动模块自已定义的miscdevice结构体对象的fops成员，如果不为空将其替换掉缺省的，参考函数中的new\_fops = fops\_get(c->fops);以及file->f\_op = new\_fops;语句。如此这般，以后内核再调用设备文件的操作时就会调用该misc设备自己定义的文件操作函数了。这种实现方式有点类似java里面函数重载的概念。

##### (二)采用miscdevice开发设备驱动的方法

大致的步骤如下：(大家联想下binder驱动)

-   第一步，定义自己misc设备的文件操作函数以及file\_operations结构体对象。如下：

```
static const struct file_operations my_misc_drv_fops = {
    .owner    = THIS_MODULE,
    .open    = my_misc_drv_open,
    .release = my_misc_drv_release,
    //根据实际情况扩充 ...
};
```

-   第二步，定义自己的misc设备对象，如下：

```
static struct miscdevice my_misc_drv_dev = {
    .minor    = MISC_DYNAMIC_MINOR,
    .name    = KBUILD_MODNAME,
    .fops    = &my_misc_drv_fops,
};
```

-   .minor 如果填充MISC\_DYNAMIC\_MINOR，则由内核动态分配次设备号，否则根据你自己定义的指定。
-   .name 给定设备的名字，也可以直接利用内核编译系统的环境变量KBUILD\_MODNAME。
-   .fops设置为第一步定义的文件操作结构体的地址
-   第三步，注销和销毁

驱动模块一般在模块初始化函数中调用misc\_register()注册自己的misc设备。实例代码如下：

```
ret = misc_register(&my_misc_drv_dev);
if (ret < 0) {
    //失败处理
}
```

注意在misc\_register()函数中有如下语句

```
misc->this_device = device_create(misc_class, misc->parent, dev,
                  misc, "%s", misc->name);
```

这句话配合前面在misc\_init()函数中的misc\_class = class\_create(THIS\_MODULE, "misc");

> 这样，class\_create()创建了一个类，而device\_create()就创建了该类的一个设备，这些都涉及linux内核的设备模型和sys文件系统额概念，暂不展开，我们只需要知道，如此这般，当该驱动模块被加载(insmod)时，和内核态的设备模型配套运行的用户态有个udev的后台服务会自动在/dev下创建一个驱动模块中注册的misc设备对应的设备文件节点，其名字就是misc->name。这样就省去了自己创建设备文件的麻烦。这样也有助于动态设备的管理。

驱动模块可以在模块卸载函数中调用misc\_deregister()注销自己的misc设备。实例代码如下：

```
misc_deregister(&my_misc_drv_dev);
```

在这个函数中会自动删除\`/dev下的同名设备文件。

##### (三)总结

> 杂项设备作为字符设备的封装，为字符设备提供简单的编程接口，如果编写新的字符驱动，可以考虑使用杂项设备接口，简单粗暴，只需要初始化一个miscdevice的结构体设备对象，然后调用misc\_register注册就可以了，不用的时候，调用misc\_deregister进行卸载。

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2017.08.07 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除