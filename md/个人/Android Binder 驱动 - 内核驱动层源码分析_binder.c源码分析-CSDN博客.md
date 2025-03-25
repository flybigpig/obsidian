相关文章链接：

[1\. Android Framework - 学习启动篇](https://blog.csdn.net/z240336124/article/details/98117999)  
[2\. Android Binder 驱动 - Media 服务的添加过程](https://blog.csdn.net/z240336124/article/details/100145625)  
[3\. Android Binder 驱动 - 启动 ServiceManager 进程](https://blog.csdn.net/z240336124/article/details/100171210)  
[4\. Android Binder 驱动 - 内核驱动层源码分析](https://blog.csdn.net/z240336124/article/details/100576575)  
[5\. Android Binder 驱动 - 从驱动层来分析服务的添加过程](https://blog.csdn.net/z240336124/article/details/100631395)  
[6\. Android Binder 驱动 - 从 Java 层来跟踪服务的查找过程](https://blog.csdn.net/z240336124/article/details/101051776)

相关源码文件：

```
/drivers/android/binder.c
/drivers/staging/android/binder.c
```

在[《Android Binder 驱动 - Media 服务的添加过程》](https://blog.csdn.net/z240336124/article/details/100145625) 与[《Android Binder 驱动 - 启动 ServiceManager 进程》](https://blog.csdn.net/z240336124/article/details/100171210) 中遗留几个驱动层的方法 binder\_open、binder\_mmap 和 binder\_ioctl，本文我们从内核驱动层来做一次彻底的分析。

#### 1\. binder\_init

用户态的程序调用 Kernel 层方法需要陷入内核态，进行系统调用(syscall)，打开 Binder 驱动方法的调用链为： open-> \_\_open() -> binder\_open()。 open() 为用户空间的方法，\_\_open() 便是系统调用中相应的处理方法，内部通过查找会找到对应的内核 binder 驱动的 binder\_open() 方法。但在驱动启动时首先会调用驱动的 xxx\_init 方法。

```
static int __init binder_init(void)
{
    int ret;
    //创建名为binder的工作队列
    binder_deferred_workqueue = create_singlethread_workqueue("binder");
    ...

     // 基于 misc_class 构造一个设备，将 miscdevice 结构挂载到 misc_list 列表上，并初始化与 linux 设备模型相关的结构  
    ret = misc_register(&binder_miscdev);
    return ret;
}

static struct miscdevice binder_miscdev = {
    // 次设备号 动态分配
    .minor = MISC_DYNAMIC_MINOR, 
    // 设备名
    .name = "binder",  
    // 设备的文件操作结构，这是 file_operations 结构   
    .fops = &binder_fops  
};

// 相对应的一些操作
const struct file_operations binder_fops = {
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

#### 2\. binder\_open

```
static int binder_open(struct inode *nodp, struct file *filp)
{
    // 当前 binder 进程结构体
    struct binder_proc *proc; 
    // 在内核开辟连续空间，大小不能超过 128K，默认初始化值为 0 
    proc = kzalloc(sizeof(*proc), GFP_KERNEL); 

    if (proc == NULL)
        return -ENOMEM;
    get_task_struct(current);
    // 将当前线程的 task 保存到 binder 进程的 tsk
    proc->tsk = current;   
    // 初始化 todo 列表
    INIT_LIST_HEAD(&proc->todo); 
    // 初始化 wait 队列
    init_waitqueue_head(&proc->wait); 
    // 将当前进程的 nice 值转换为进程优先级
    proc->default_priority = task_nice(current);  
    // 同步锁，因为 binder 支持多线程访问
    binder_lock(__func__);  
    // BINDER_PROC 对象创建数加1 
    binder_stats_created(BINDER_STAT_PROC); 
    // 将 proc_node 节点添加到 binder_procs 为表头的队列
    hlist_add_head(&proc->proc_node, &binder_procs); 
    proc->pid = current->group_leader->pid;
    // 初始化已分发的死亡通知列表
    INIT_LIST_HEAD(&proc->delivered_death); 
    // file 文件指针的 private_data 变量指向 binder_proc 数据
    filp->private_data = proc;   
    // 释放同步锁    
    binder_unlock(__func__); 
    return 0;
}

struct binder_proc {
struct hlist_node proc_node;
    // threads 树 保存 binder_proc 进程内用于处理用户请求的线程
struct rb_root threads;
    // nodes树 保存 binder_proc 进程内的 Binder 实体；
struct rb_root nodes;
    // 进程内的 Binder 引用，即引用的其它进程的 Binder 实体，以句柄作 key 值来组织
struct rb_root refs_by_desc;
    // 进程内的 Binder 引用，即引用的其它进程的 Binder 实体，以地址作 key 值来组织
struct rb_root refs_by_node;
    // 当前进程 id
int pid;
struct vm_area_struct *vma;
struct mm_struct *vma_vm_mm;
    // 将当前线程的 task_struct
struct task_struct *tsk;
struct files_struct *files;
struct hlist_node deferred_work_node;
int deferred_work;
    // 指向内核虚拟空间的地址
void *buffer;
    // 用户虚拟地址空间与内核虚拟地址空间的偏移量
ptrdiff_t user_buffer_offset;

struct list_head buffers;
struct rb_root free_buffers;
struct rb_root allocated_buffers;
size_t free_async_space;
    // 物理页的指针数组
struct page **pages;
    // 映射虚拟内存的大小
size_t buffer_size;
uint32_t buffer_free;
struct list_head todo;
wait_queue_head_t wait;
struct binder_stats stats;
struct list_head delivered_death;
int max_threads;
int requested_threads;
int requested_threads_started;
int ready_threads;
long default_priority;
struct dentry *debugfs_entry;
};
```

创建 binder\_proc 对象，并把当前进程等信息保存到 binder\_proc 对象，该对象管理 IPC 所需的各种信息并拥有其他结构体的根结构体；再把 binder\_proc 对象保存到文件指针 filp，以及把 binder\_proc 加入到全局链表 binder\_procs。

#### 3\. binder\_mmap

```
static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret;
    // 内核虚拟空间
    struct vm_struct *area;
    // 从 filp 中获取之前打开保存的
    struct binder_proc *proc = filp->private_data;
    const char *failure_string;
    struct binder_buffer *buffer; 

    if (proc->tsk != current)
        return -EINVAL;
    // 保证映射内存大小不超过 4M
    if ((vma->vm_end - vma->vm_start) > SZ_4M)
        vma->vm_end = vma->vm_start + SZ_4M;  
    // 同步锁
    mutex_lock(&binder_mmap_lock);  
    // 采用 IOREMAP 方式，分配一个连续的内核虚拟空间，与进程虚拟空间大小一致
    area = get_vm_area(vma->vm_end - vma->vm_start, VM_IOREMAP);
    if (area == NULL) {
        ret = -ENOMEM;
        failure_string = "get_vm_area";
        goto err_get_vm_area_failed;
    }
    // 指向内核虚拟空间的地址
    proc->buffer = area->addr; 
    // 地址偏移量 = 用户虚拟地址空间 - 内核虚拟地址空间
    proc->user_buffer_offset = vma->vm_start - (uintptr_t)proc->buffer;
    // 释放锁
    mutex_unlock(&binder_mmap_lock); 

    ...
    // 分配物理页的指针数组，数组大小为 vma 的等效 page 个数；
    proc->pages = kzalloc(sizeof(proc->pages[0]) * ((vma->vm_end - vma->vm_start) / PAGE_SIZE), GFP_KERNEL);
    if (proc->pages == NULL) {
        ret = -ENOMEM;
        failure_string = "alloc page array";
        goto err_alloc_pages_failed;
    }
    proc->buffer_size = vma->vm_end - vma->vm_start;

    vma->vm_ops = &binder_vm_ops;
    vma->vm_private_data = proc;

    //分配物理页面，同时映射到内核空间和进程空间，先分配1个物理页
    if (binder_update_page_range(proc, 1, proc->buffer, proc->buffer + PAGE_SIZE, vma)) {
        ret = -ENOMEM;
        failure_string = "alloc small buf";
        goto err_alloc_small_buf_failed;
    }
    // binder_buffer 对象 指向 proc 的 buffer 地址
    buffer = proc->buffer; 
    // 创建进程的 buffers 链表头
    INIT_LIST_HEAD(&proc->buffers); 
    // 将 binder_buffer 地址 加入到所属进程的 buffers 队列
    list_add(&buffer->entry, &proc->buffers); 
    buffer->free = 1;
    // 将空闲 buffer 放入 proc->free_buffers 中
    binder_insert_free_buffer(proc, buffer);
    // 异步可用空间大小为 buffer 总大小的一半。
    proc->free_async_space = proc->buffer_size / 2;
    barrier();
    proc->files = get_files_struct(current);
    proc->vma = vma;
    proc->vma_vm_mm = vma->vm_mm;
    return 0;

    ...// 错误flags跳转处，free释放内存之类的操作
    return ret;
}

static int binder_update_page_range(struct binder_proc *proc, int allocate,
            void *start, void *end,
            struct vm_area_struct *vma)
{
  void *page_addr;
  unsigned long user_page_addr;
  struct page **page;
  // 内存结构体
  struct mm_struct *mm; 

  if (vma)
        // binder_mmap 过程 vma 不为空，其他情况都为空
  mm = NULL;
  else
        // 获取 mm 结构体，从 tsk 中获取
  mm = get_task_mm(proc->tsk); 
      
  if (mm) {
    // 获取 mm_struct 的写信号量
    down_write(&mm->mmap_sem); 
    vma = proc->vma;
  }

  // 此处 allocate 为 1，代表分配过程。如果为 0 则代表释放过程
  if (allocate == 0)
    goto free_range;

  for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
    int ret;
    page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];
    // 分配一个 page 的物理内存
    *page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO);
    // 物理空间映射到虚拟内核空间
    ret = map_kernel_range_noflush((unsigned long)page_addr,
          PAGE_SIZE, PAGE_KERNEL, page);
    //  用户空间地址 = 内核空间地址 + 偏移量
    user_page_addr = (uintptr_t)page_addr + proc->user_buffer_offset;
    //物理空间映射到虚拟进程空间
    ret = vm_insert_page(vma, user_page_addr, page[0]);
  }
  
  if (mm) {
    // 释放内存的写信号量
    up_write(&mm->mmap_sem); 
     // 减少 mm->mm_users 计数
    mmput(mm);
  }
  return 0;

  //释放内存的流程
  free_range:
  ...
  return -ENOMEM;
}
```

上面这些代码大部分都是 linux 内核方面的知识了，我们来简单解释下：task\_struct 代表的是进程或者线程管理的进程控制结构体，mm\_struct 是 task\_struct 结构体中虚拟地址管理的结构体，vm\_area\_struct 代表的是虚拟用户空间映射管理的结构体，vm\_struct 代表的是内核空间管理的结构体。alloc\_page 方法的作用是分配一个物理内存，map\_kernel\_range\_noflush 方法的作用是将物理空间映射到虚拟内核空间，vm\_insert\_page 方法的作用是将物理空间映射到虚拟用户空间。binder\_mmap 的主要作用就是开辟一块连续的内核空间，并且开辟一个物理页的地址空间，同时映射到用户空间和内核空间。

#### 4\. binder\_ioctl

```
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret;
    struct binder_proc *proc = filp->private_data;
    struct binder_thread *thread;  // binder线程
    unsigned int size = _IOC_SIZE(cmd);
    void __user *ubuf = (void __user *)arg;
    // 进入休眠状态，直到中断唤醒
    ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
    if (ret)
        goto err_unlocked;

    binder_lock(__func__);
    // 获取 binder_thread 
    thread = binder_get_thread(proc);
    if (thread == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    switch (cmd) {
    // 进行 binder 的读写操作
    case BINDER_WRITE_READ: 
        ret = binder_ioctl_write_read(filp, cmd, arg, thread);
        if (ret)
            goto err;
        break;
    // 设置 binder 最大支持的线程数
    case BINDER_SET_MAX_THREADS: 
        if (copy_from_user(&proc->max_threads, ubuf, sizeof(proc->max_threads))) {
            ret = -EINVAL;
            goto err;
        }
        break;
    // 成为 binder 的上下文管理者，也就是 ServiceManager 成为守护进程
    case BINDER_SET_CONTEXT_MGR: 
        ret = binder_ioctl_set_ctx_mgr(filp);
        if (ret)
            goto err;
        break;
    // 当 binder 线程退出，释放 binder 线程
    case BINDER_THREAD_EXIT:   
        binder_free_thread(proc, thread);
        thread = NULL;
        break;
    // 获取 binder 的版本号
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

err_unlocked:
    trace_binder_ioctl_done(ret);
    return ret;
}

static struct binder_thread *binder_get_thread(struct binder_proc *proc)
{
    struct binder_thread *thread = NULL;
    struct rb_node *parent = NULL;
    struct rb_node **p = &proc->threads.rb_node;
    // 根据当前进程的 pid，从 binder_proc 中查找相应的 binder_thread
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
        // 新建 binder_thread 结构体
        thread = kzalloc(sizeof(*thread), GFP_KERNEL); 
        if (thread == NULL)
            return NULL;
        binder_stats_created(BINDER_STAT_THREAD);
        thread->proc = proc;
        // 保存当前进程(线程)的 pid
        thread->pid = current->pid;  
        init_waitqueue_head(&thread->wait);
        // 初始化线程的 todo 队列
        INIT_LIST_HEAD(&thread->todo);
        // 把线程加入 proc->threads 
        rb_link_node(&thread->rb_node, parent, p);
        rb_insert_color(&thread->rb_node, &proc->threads);
        thread->looper |= BINDER_LOOPER_STATE_NEED_RETURN;
        thread->return_error = BR_OK;
        thread->return_error2 = BR_OK;
    }
    return thread;
}

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
    // 把用户空间数据 ubuf 拷贝到 bwr
    if (copy_from_user(&bwr, ubuf, sizeof(bwr))) { 
        ret = -EFAULT;
        goto out;
    }

    // 当写缓存中有数据，则执行 binder 写操作
    if (bwr.write_size > 0) {
        ret = binder_thread_write(proc, thread,
                      bwr.write_buffer, bwr.write_size, &bwr.write_consumed);
        trace_binder_write_done(ret);
        // 当写失败，再将 bwr 数据写回用户空间，并返回
        if (ret < 0) { 
            bwr.read_consumed = 0;
            if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
                ret = -EFAULT;
            goto out;
        }
    }
    // 当读缓存中有数据，则执行 binder 读操作
    if (bwr.read_size > 0) {
        ret = binder_thread_read(proc, thread,
                      bwr.read_buffer, bwr.read_size, &bwr.read_consumed,
                      filp->f_flags & O_NONBLOCK);
        trace_binder_read_done(ret);
        // 唤醒等待状态的线程
        if (!list_empty(&proc->todo))
            wake_up_interruptible(&proc->wait); 
        // 当读失败，再将bwr数据写回用户空间，并返回
        if (ret < 0) { 
            if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
                ret = -EFAULT;
            goto out;
        }
    }
    // 将内核数据 bwr 拷贝到用户空间 ubuf
    if (copy_to_user(ubuf, &bwr, sizeof(bwr))) { 
        ret = -EFAULT;
        goto out;
    }
out:
    return ret;
}
```

通过以上分析，ioctl 命令有 BINDER\_WRITE\_READ （binder 读写交互）、BINDER\_SET\_CONTEXT\_MGR（servicemanager进程成为上下文管理者）、BINDER\_SET\_MAX\_THREADS（设置最大线程数）、BINDER\_VERSION（获取 binder 版本）。有两个核心复杂方法 binder\_thread\_write 和 binder\_thread\_read，由于这里面的代码逻辑较为复杂，因此到后面我们带着线索再去具体分析。

视频地址：https://pan.baidu.com/s/13TKOUH\_byzRz17QSH8fNww  
视频密码：k112