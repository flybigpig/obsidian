### Binder究竟是什么？

先说答案:Binder是android提出的一种跨进程的通信机制，也可以说是一个驱动设备，是Android系统模拟的一个硬件设备，通过这个设备，我们可以实现跨进程的通信。在android系统中，一个进程对应一个应用程序，但是一个应用程序可以有多个进程，市面上的App，很多都是多进程的。例如抖音、QQ、微信、等等。一个进程中可以包含多个线程，在Android中应用只有一个主线程，也叫UI线程，在UI线程中才可以做UI更新操作。主线程也是不可以做耗时操作的。在android系统中进程之间是互相隔离的，当我们需要数据传递的时候就需要使用跨进程通讯了，Binder就是跨进程方案中的一种。

微信： ![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/9b181b8a1f684e5db3c775dcfbd942bd~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

抖音:

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/4a16396997aa441b8222c657c502b56d~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

QQ音乐：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/5fcb604eb44548f3a61a6ff3dcd8f8c3~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

### Android为什么要使用Binder？

跨进程的方案有很多，比如Android SDK给我们提供的Service、BroadCast等，也可以通过Socket、Pipe、Binder、Semaphore 、共享内存等。既然有这么多的方式，Android为什么要选择Binder呢？

#### 1.性能

Binder的数据拷贝只需要一次，而Pipe、socket都需要两次，但共享内存一次拷贝都不需要，从性能上来讲Binder性能仅次于共享内存。

#### 2.稳定性

Binder是基于C/S架构的，Server和Client相对独立，稳定性比较好；但是共享内存实现起来复杂，没有客户端和服务端的区别，需要自己解决同步问题，否则会出现死锁等问题。所以从稳定性来看，Binder优于共享内存。

#### 3.安全

Android给每个安装好的应用程序分配了自己的UID，进程UID是身份的重要标志，Android系统要求只对外暴露Client端，Client将数据发送给Server端，Server会根据策略，判断UID是否有权限访问。其他的IPC由用户在数据包里面填入UID等 来进行校验保证安全；所以在Android由系统在内核层来生成\\校验保证你的安全信息。所以从安全来看，Binder比传统的IPC机制安全性更高。

#### 4.总结

|  | Binder | 共享内存 | Pipe | Socket |
| --- | --- | --- | --- | --- |
| 性能 | 一次内存拷贝 | 0次内存拷贝 | 两次内存拷贝 | 两次内存拷贝 |
| 稳定性 | C/S架构稳定性高 | 同步问题、死锁问题 | 仅支持父子进程通信，单全功效率低 | C/S架构，传输需要握手、挥手、效率低，开销大 |
| 安全 | 内核层对App分配UID，安全性高 | 自定义协议，需要自己实现安全，并且接口对外开放 | 自定义协议，需要自己实现安全，并且接口对外开放 | 自定义协议，需要自己实现安全，并且接口对外开放 |

### Binder通信流程（图解）

![Binder通信过程.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/340faf5ddec84c75851d08d8f715d8be~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

服务端：将服务注册到ServiceMananger中。

客户端：从ServiceManager取出服务的代理，通过代理对服务端进行数据通信。

### Binder的注册

Binder驱动的源码在binder.c中，函数不多，主要有4个关键函数。我们来看看这四个函数做了什么事情：

#### 1.binder\_init

注册binder驱动函数，调用misc\_register对binder驱动进行注册，后续我们调用open("/dev/binder")就是使用该设备。

```
static struct miscdevice binder_miscdev = {//binder设备的一些信息
 .minor = MISC_DYNAMIC_MINOR,//设备号信息是Linux，0表示使用过该设备，1表示没使用过。
 .name = "binder",// 该设备的名称
 .fops = &binder_fops //该设备支持的一些操作 比如open mmap  flush release等 对Native层开放的函数sysCall
};

static int __init binder_init(void)
{
 int ret;
//创建在一个线程中运行的workqueue，命名为binder
 binder_deferred_workqueue = create_singlethread_workqueue("binder");
 if (!binder_deferred_workqueue)
  return -ENOMEM;
 binder_debugfs_dir_entry_root = debugfs_create_dir("binder", NULL);
 if (binder_debugfs_dir_entry_root)
  binder_debugfs_dir_entry_proc = debugfs_create_dir("proc",
       binder_debugfs_dir_entry_root);
  //注册驱动设备
 ret = misc_register(&binder_miscdev);
 return ret;
}
```

#### 2.binder\_open

初始化binder\_proc结构体，初始化4个红黑树，todo工作列表，以及把当前binder节点加入到渠道的列表中。注：binder\_proc是当前进程的biner管理器，内部包含了binder的线程红黑树，当前进程存在的binder，以及依赖的其他binder。

```
struct binder_proc {
 struct hlist_node proc_node;
 struct rb_root threads; //binder的线程信息
 struct rb_root nodes; //自己binder的root信息
 struct rb_root refs_by_desc;//其他进程对应的binder对象 以handle为key
 struct rb_root refs_by_node;//其他进程的binder对象，内存地址为key
 int pid;
 struct vm_area_struct *vma; //用户内存的映射管理 
 struct mm_struct *vma_vm_mm;//虚拟内存信息
 struct task_struct *tsk;//进程管理
 struct files_struct *files;
 struct hlist_node deferred_work_node;
 int deferred_work;
 void *buffer;//内核空间对应的首地址
 ptrdiff_t user_buffer_offset;//用户空间和内核空间的偏移量。

 struct list_head buffers;
 struct rb_root free_buffers;
 struct rb_root allocated_buffers;
 size_t free_async_space;

 struct page **pages;
 size_t buffer_size;
 uint32_t buffer_free;
 struct list_head todo;//todo 队列  目标进程的任务
 wait_queue_head_t  wait;//wati队列 当前进程的任务
 struct binder_stats stats;
 struct list_head delivered_death;
 int max_threads;//最大线程数
 int requested_threads;
 int requested_threads_started;
 int ready_threads;
 long default_priority;
 struct dentry *debugfs_entry;
};
```

```
static int binder_open(struct inode *nodp, struct file *filp)
{
 struct binder_proc *proc;//binder的结构体 很关键
 proc = kzalloc(sizeof(*proc), GFP_KERNEL);//申请binder_proc大小的一段连续内存空间地址
 if (proc == NULL)
  return -ENOMEM;
 get_task_struct(current);
 proc->tsk = current;
 INIT_LIST_HEAD(&proc->todo);//初始化todo队列  很重要
 init_waitqueue_head(&proc->wait);//初始化wait队列   很重要
 proc->default_priority = task_nice(current);
//加入互斥锁
 binder_lock(__func__);

 binder_stats_created(BINDER_STAT_PROC);
 hlist_add_head(&proc->proc_node, &binder_procs);//将当前的binder_proc添加到binder_procs中。
 proc->pid = current->group_leader->pid;
 INIT_LIST_HEAD(&proc->delivered_death);
 filp->private_data = proc;

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

#### 3.binder\_mmap

根据用户空间内存的大小来分配一块内核的内存，通过kzalloc按照Page分配对应的物理内存，然后把这块内存映射到用户空间和内核空间。

```
static int binder_mmap(struct file *filp, struct vm_area_struct *vma/*用户空间*/)
{
 int ret;
 struct vm_struct *area;//内核的虚拟内存
 struct binder_proc *proc = filp->private_data;
 const char *failure_string;
 struct binder_buffer *buffer;

 if ((vma->vm_end - vma->vm_start) > SZ_4M)//最大空间
  vma->vm_end = vma->vm_start + SZ_4M;

 vma->vm_flags = (vma->vm_flags | VM_DONTCOPY) & ~VM_MAYWRITE;

 mutex_lock(&binder_mmap_lock);
 if (proc->buffer) {
  ret = -EBUSY;
  failure_string = "already mapped";
  goto err_already_mapped;
 }
 area = get_vm_area(vma->vm_end - vma->vm_start, VM_IOREMAP);//将内核空间和用户空间的内存大小进行同步
 proc->buffer = area->addr;//当前进程的buffer指向内核空间的地址
 proc->user_buffer_offset = vma->vm_start - (uintptr_t)proc->buffer;//计算出来用户空间和内核空间的偏移值
 mutex_unlock(&binder_mmap_lock);
 proc->pages = kzalloc(sizeof(proc->pages[0]) * ((vma->vm_end - vma->vm_start) / PAGE_SIZE), GFP_KERNEL);
 proc->buffer_size = vma->vm_end - vma->vm_start;

 vma->vm_ops = &binder_vm_ops;
 vma->vm_private_data = proc;
//分配或者释放binder内存空间以及用户空间的映射
 if (binder_update_page_range(proc, 1, proc->buffer, proc->buffer + PAGE_SIZE, vma)) {
  ret = -ENOMEM;
  failure_string = "alloc small buf";
  goto err_alloc_small_buf_failed;
 }
  buffer = proc->buffer;
 INIT_LIST_HEAD(&proc->buffers);
 list_add(&buffer->entry, &proc->buffers);
 buffer->free = 1;
 binder_insert_free_buffer(proc, buffer);
 proc->free_async_space = proc->buffer_size / 2;//异步的话使用的buffersize 只有1/2
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

```
static int binder_update_page_range(struct binder_proc *proc, int allocate,
        void *start, void *end,
        struct vm_area_struct *vma)
{
 void *page_addr;
 unsigned long user_page_addr;
 struct vm_struct tmp_area;
 struct page **page;
 struct mm_struct *mm;

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

 if (allocate == 0)//释放内核空间
  goto free_range;
//申请空间
 for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
  int ret;

  page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];

  BUG_ON(*page);
   //分配一个物理page页 4k
  *page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO);
  tmp_area.addr = page_addr;
  tmp_area.size = PAGE_SIZE + PAGE_SIZE /* guard page? */;
   //物理空间分配到内核空间
  ret = map_vm_area(&tmp_area, PAGE_KERNEL, page);
   //物理内存映射到用户空间
  user_page_addr =
   (uintptr_t)page_addr + proc->user_buffer_offset;
  ret = vm_insert_page(vma, user_page_addr, page[0]);
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

#### 4.binder\_ioctl

对数据相关的操作都在这个函数中，非常关键的一个函数。在这里会根据调用放的命令 来进行对应的操作，比如读写数据，设置管理者等。

```
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
 int ret;
 struct binder_proc *proc = filp->private_data;
 struct binder_thread *thread;
 unsigned int size = _IOC_SIZE(cmd);
 void __user *ubuf = (void __user *)arg;
 ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
 if (ret)
  goto err_unlocked;
 binder_lock(__func__);
 thread = binder_get_thread(proc);
 switch (cmd) {
 case BINDER_WRITE_READ://读写数据
  ret = binder_ioctl_write_read(filp, cmd, arg, thread);
  if (ret)
   goto err;
  break;
 case BINDER_SET_MAX_THREADS://设置最大线程数
  if (copy_from_user(&proc->max_threads, ubuf, sizeof(proc->max_threads))) {
   ret = -EINVAL;
   goto err;
  }
  break;
 case BINDER_SET_CONTEXT_MGR://设置binder_manager
  ret = binder_ioctl_set_ctx_mgr(filp);
  if (ret)
   goto err;
  break;
 case BINDER_THREAD_EXIT://退出binder线程
  binder_debug(BINDER_DEBUG_THREADS, "%d:%d exit\n",
        proc->pid, thread->pid);
  binder_free_thread(proc, thread);
  thread = NULL;
  break;
 case BINDER_VERSION: {//binder_version的校验
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
 return ret;
}
```

```
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
 if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {//拷贝用户空间的数据头，也就是最初的命令 比如BC_ENTER_LOOP这些命令信息。
  ret = -EFAULT;
  goto out;
 }
 if (bwr.write_size > 0) {//根据写的大小来判断是写操作还是读操作
  ret = binder_thread_write(proc, thread,
       bwr.write_buffer,
       bwr.write_size,
       &bwr.write_consumed);
  if (ret < 0) {
   bwr.read_consumed = 0;
   if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
    ret = -EFAULT;
   goto out;
  }
 }
 if (bwr.read_size > 0) {//如果是读取数据就进行读操作
  ret = binder_thread_read(proc, thread, bwr.read_buffer,
      bwr.read_size,
      &bwr.read_consumed,
      filp->f_flags & O_NONBLOCK);
  trace_binder_read_done(ret);
  if (!list_empty(&proc->todo))//如果目标进程的todo队列有任务就挂起当前进程，等待目标进程处理。
   wake_up_interruptible(&proc->wait);
  if (ret < 0) {//把数据拷贝到用户空间
   if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
    ret = -EFAULT;
   goto out;
  }
 }
 if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {
  ret = -EFAULT;
  goto out;
 }
out:
 return ret;
}
```

### Binder的初始化

#### 1.binder方法的注册

在Java的世界中，Binder提供了很多的Native函数，那么这些Native函数是在什么时候进行注册的呢❓其实是在startReg的时候，这个startReg就是在虚拟器启动的时候，因为这些函数的虚拟机需要去注册的函数。那么虚拟机是什么时候开启的呢❓这就要说到init进程了，当内核启动完成后，通过inittable 唤起用户空间的第一个进程：init进程，在init进程中解析init.rc文件，会开启我们的各种系统服务，比如system\_server、media、SurfaceFlinger、zygote等等。在开启Zygote的时候就会开启我们的虚拟机，以及注册我们最熟悉的JNI了。这里需要来回跳转Java层和Native层代码很多，所以我们就贴出关键代码。

```
static const JNINativeMethod gBinderMethods[] = {
    { "getCallingPid", "()I", (void*)android_os_Binder_getCallingPid },
    { "getCallingUid", "()I", (void*)android_os_Binder_getCallingUid },
    { "clearCallingIdentity", "()J", (void*)android_os_Binder_clearCallingIdentity },
    { "restoreCallingIdentity", "(J)V", (void*)android_os_Binder_restoreCallingIdentity },
    { "setThreadStrictModePolicy", "(I)V", (void*)android_os_Binder_setThreadStrictModePolicy },
    { "getThreadStrictModePolicy", "()I", (void*)android_os_Binder_getThreadStrictModePolicy },
    { "flushPendingCommands", "()V", (void*)android_os_Binder_flushPendingCommands },
    { "init", "()V", (void*)android_os_Binder_init },
    { "destroy", "()V", (void*)android_os_Binder_destroy }
};
```

#### 2.binder的启动

binder的启动和应用是密不可分的。当init解析init.rc文件的时候就会启动Zygote的main函数，然后开启java层的Zygote调用Native的onZygoteInit。害怕大家会晕，所以就不贴出代码了，直接看onZygoteInit函数。

```
   virtual void onZygoteInit()
    {
        atrace_set_tracing_enabled(true);
     //当zygote初始化的时候会调用self函数，创建进程信息。
        sp<ProcessState> proc = ProcessState::self();
        proc->startThreadPool();
    }
```

```
sp<ProcessState> ProcessState::self()
{
    Mutex::Autolock _l(gProcessMutex);
    if (gProcess != NULL) {
        return gProcess;
    }
    gProcess = new ProcessState;
    return gProcess;
}
```

```
ProcessState::ProcessState()
    : mDriverFD(open_driver())
    , mVMStart(MAP_FAILED)
    , mManagesContexts(false)
    , mBinderContextCheckFunc(NULL)
    , mBinderContextUserData(NULL)
    , mThreadPoolStarted(false)
    , mThreadPoolSeq(1)
{
    if (mDriverFD >= 0) {
        // XXX Ideally, there should be a specific define for whether we
        // have mmap (or whether we could possibly have the kernel module
        // availabla).
[[if]] !defined(HAVE_WIN32_IPC)
        // mmap the binder, providing a chunk of virtual address space to receive transactions.
        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
        if (mVMStart == MAP_FAILED) {
            // *sigh*
            ALOGE("Using /dev/binder failed: unable to mmap transaction memory.\n");
            close(mDriverFD);
            mDriverFD = -1;
        }
[[else]]
        mDriverFD = -1;
[[endif]]
    }
```

```
static int open_driver()
{
     //打开我们的binder驱动
    int fd = open("/dev/binder", O_RDWR);
    if (fd >= 0) {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        int vers;
        status_t result = ioctl(fd, BINDER_VERSION, &vers);
        if (result == -1) {
            close(fd);
            fd = -1;
        }
        if (result != 0 || vers != BINDER_CURRENT_PROTOCOL_VERSION) {
            close(fd);
            fd = -1;
        }
        //设置应用的最大数
        size_t maxThreads = 15;
        result = ioctl(fd, BINDER_SET_MAX_THREADS, &maxThreads);
    } else {
        ALOGW("Opening '/dev/binder' failed: %s\n", strerror(errno));
    }
    return fd;
}
```

所以说安卓应用天生就是可以跨进程的，我们开发者并不需要做特殊处理，Zygote替我们做了。👍🏻

### Binder是如何运作的？

#### 1.ServiceManager

ServiceManager是Android系统为开发者提供的一个服务大管家，当开机之后，由内核态进入用户态之后，会启动system\_server进程，在该进程里面会对AMS，PKMS，PMS等等进行创建，然后添加到ServiceManager中，这里面就是通过binder进行跨进程通信的。我们就以他为例来看看Binder是如何运作的。

#### 2.ServiceManager是如何开启的

通过解析init.rc找到ServiceManager，调用main函数。

```
service servicemanager /system/bin/servicemanager
    class core
    user system
    group system
    critical
    onrestart restart healthd
    onrestart restart zygote
    onrestart restart media
    onrestart restart surfaceflinger
    onrestart restart drm
```

在main函数中打开binder驱动，并且调用binder\_loop函数。

```
int main(int argc, char **argv)
{
    struct binder_state *bs;
    void *svcmgr = BINDER_SERVICE_MANAGER;
    bs = binder_open(128*1024);//打开binder设备进行注册 128* 1024 
    if (binder_become_context_manager(bs)) {//设置自己为binder管理者
        return -1;
    }
    svcmgr_handle = svcmgr;
  //进入binder_loop状态。svcmgr_handler是一个函数指针 稍后我们还会回到这里来
    binder_loop(bs, svcmgr_handler);
    return 0;
}
```

打开binder设备，并对内存空间进行映射。将用户空间和内核空间映射到同一片内存中。

```
struct binder_state *binder_open(unsigned mapsize)
{
    struct binder_state *bs;

    bs = malloc(sizeof(*bs));
    if (!bs) {
        errno = ENOMEM;
        return 0;
    }
    bs->fd = open("/dev/binder", O_RDWR);//真正的打开binder
    bs->mapsize = mapsize;
    //通过mmap 将用户空间和内核空间绑定在一块内存区域中。
    bs->mapped = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, bs->fd, 0);
    return bs;
}
```

映射完成之后，调用binder\_loop跟binder进行通信：在请求的readBuff中写入BC\_ENTER\_LOOP。然后调用binder\_write函数进行数据的写入。

```
void binder_loop(struct binder_state *bs, binder_handler func)
{
    int res;
    struct binder_write_read bwr;
    unsigned readbuf[32];
    bwr.write_size = 0;
    bwr.write_consumed = 0;
    bwr.write_buffer = 0;
    readbuf[0] = BC_ENTER_LOOPER;
 //给binder设备的readBuf 写入了一个BC_ENTER_LOOPER命令 
    binder_write(bs, readbuf, sizeof(unsigned));
    for (;;) {
        bwr.read_size = sizeof(readbuf);
        bwr.read_consumed = 0;
        bwr.read_buffer = (unsigned) readbuf;
      //write函数执行完成之后会写入一个BINDER_WRITE_READ，那么写入的是什么呢？没忘记吧，是BC_ENTER_LOOP
        res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);
        if (res < 0) {
            break;
        }
        res = binder_parse(bs, 0, readbuf, bwr.read_consumed, func);
        if (res == 0) {
            break;
        }
        if (res < 0) {
            break;
        }
    }
}
```

在写数据的时候将请求的BC\_ENTER\_LOOP写入到writeBuffer中，然后调用ioctl 对binder就行写操作，这时传入的命令是\*\*\*\* BINDER\_WRITE\_READ \*\*\*\*

```
int binder_write(struct binder_state *bs, void *data, unsigned len)
{
    struct binder_write_read bwr;
    int res;
    bwr.write_size = len;//设置写的长度
    bwr.write_consumed = 0;
    bwr.write_buffer = (unsigned) data;//此时write_buffer为BC_ENTER_LOOP
    bwr.read_size = 0;
    bwr.read_consumed = 0;
    bwr.read_buffer = 0;
  //调用binder的ioctl 写入BINDER_WRITE_READ指令。
    res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);
    if (res < 0) {
        fprintf(stderr,"binder_write: ioctl failed (%s)\n",
                strerror(errno));
    }
    return res;
}
```

现在我们需要跳转到binder的ioctl函数，在上边有列出，我就不全部列出了，列出我们需要看的关键函数：

```
//binder_ioctl函数:注意 此时我们接受到的命令是BINDER_WRITE_READ 所以会调用binder_ioctl_write_read
case BINDER_WRITE_READ:
  ret = binder_ioctl_write_read(filp, cmd, arg, thread);
  if (ret)
   goto err;
  break;

//binder_ioctl_write_read函数:会对write_size进行判断，此时是>0的，所以会调用binder_thread_write函数，这个函数之前没列出来所以下面就列出来 注意此时传递的write数据是**** BC_ENTER_LOOP **** 
  if (bwr.write_size > 0) {
  ret = binder_thread_write(proc, thread,
       bwr.write_buffer,
       bwr.write_size,
       &bwr.write_consumed);
  if (ret < 0) {
   bwr.read_consumed = 0;
   if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
    ret = -EFAULT;
   goto out;
  }
 }

```

由于binder\_thread\_write函数太长，只列出我们关心的逻辑。

```
 case BC_ENTER_LOOPER:
   if (thread->looper & BINDER_LOOPER_STATE_REGISTERED) {
    thread->looper |= BINDER_LOOPER_STATE_INVALID;
     proc->pid, thread->pid);
   }
   thread->looper |= BINDER_LOOPER_STATE_ENTERED;//设置当前thread的looper状态为BINDER_LOOPER_STATE_ENTERED
   break;
```

在写入完成之后，还会执行read命令。

```
//binder_thread_read函数太长 只贴出关键函数
 if (*consumed == 0) {
  if (put_user(BR_NOOP, (uint32_t __user *)ptr))//存入BR_NOOP ptr就是刚才传入的bwr
   return -EFAULT;
  ptr += sizeof(uint32_t);
 }

 if (wait_for_proc_work) {//这里是true
  if (!(thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
     BINDER_LOOPER_STATE_ENTERED))) {
   binder_user_error("%d:%d ERROR: Thread waiting for process work before calling BC_REGISTER_LOOPER or BC_ENTER_LOOPER (state %x)\n",
    proc->pid, thread->pid, thread->looper);
   wait_event_interruptible(binder_user_error_wait,
       binder_stop_on_user_error < 2);
  }
  binder_set_nice(proc->default_priority);
  if (non_block) {//不阻塞
   if (!binder_has_proc_work(proc, thread))
    ret = -EAGAIN;
  } else //将SM进程置入挂起状态。此时ServiceManager开启成功，等待别人来唤醒
   ret = wait_event_freezable_exclusive(proc->wait, binder_has_proc_work(proc, thread));
 } else {
  if (non_block) {
   if (!binder_has_thread_work(thread))
    ret = -EAGAIN;
  } else
   ret = wait_event_freezable(thread->wait, binder_has_thread_work(thread));
 }

```

总结：ServiceManager的main函数中会将binder设备进行打开，映射。并将自己注册为binder的管理者。设置自己为loop状态，并且进入休眠，等待客户端的唤醒。

#### 3.服务的获取

ServiceManager中的getIServiceManager()获取ServiceManager。

```

    @UnsupportedAppUsage
    private static IServiceManager getIServiceManager() {
        if (sServiceManager != null) {
            return sServiceManager;
        }
        // Find the service manager
      //这里其实就是调用的bpServiceManager
        sServiceManager = ServiceManagerNative
                .asInterface(Binder.allowBlocking(BinderInternal.getContextObject()));
        return sServiceManager;
    }

//BinderInternal.getContextObject() 是一个native函数 其实获取了BpBInder(0)，并包装成Java层BinderProxy;
public static final native IBinder getContextObject();

    static public IServiceManager asInterface(IBinder obj)
    {
        if (obj == null) {
            return null;
        }
      //通过BpServiceManager的queryLocalInterface
        IServiceManager in =
            (IServiceManager)obj.queryLocalInterface(descriptor);
        if (in != null) {
            return in;
        }
//所以就会创建ServiceManagerProxy(BpBinder(0))
        return new ServiceManagerProxy(obj);
    }
//queryLocalInterface 可以看到BinderProxy直接返回的是null
    public IInterface queryLocalInterface(String descriptor) {
        return null;
    }
```

```
sp<IServiceManager> defaultServiceManager()
{
    if (gDefaultServiceManager != NULL) return gDefaultServiceManager;
    
    {
        AutoMutex _l(gDefaultServiceManagerLock);
        while (gDefaultServiceManager == NULL) {
            gDefaultServiceManager = interface_cast<IServiceManager>(
                ProcessState::self()->getContextObject(NULL));//这里获取到了BpBinder(0) 然后进行强转换成IServiceManager
            if (gDefaultServiceManager == NULL)
                sleep(1);
        }
    }
    
    return gDefaultServiceManager;
}
```

通过ProcessState::self()->getContextObject(NULL);获取到ServiceManager:

```
sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& caller)
{
    return getStrongProxyForHandle(0);
}

sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;
    AutoMutex _l(mLock);
    handle_entry* e = lookupHandleLocked(handle);
      if (e != NULL) {
        IBinder* b = e->binder;
        if (b == NULL || !e->refs->attemptIncWeak(this)) {
            if (handle == 0) {//此时handle是0，调用IPCThreadState的transact函数发送了PING_TRANSACTION命令
                Parcel data;
              //发送一个ping命令检测当前binder状态
                status_t status = IPCThreadState::self()->transact(
                        0, IBinder::PING_TRANSACTION, data, NULL, 0);
                if (status == DEAD_OBJECT)
                   return NULL;
            }
//创建一个BpBinder
            b = new BpBinder(handle); 
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }
//返回当前BpBinder(0)
    return result;
}
```

这里的interface\_cast是一个宏函数，所以我们需要拆宏:

```
 [[define]] DECLARE_META_INTERFACE(INTERFACE)                               \
    static const android::String16 descriptor;                          \
    static android::sp<I##INTERFACE> asInterface(                       \
            const android==sp<android==IBinder>& obj);                  \
    virtual const android::String16& getInterfaceDescriptor() const;    \
    I##INTERFACE();                                                     \
    virtual ~I##INTERFACE();       


拆完宏的函数是:
[[define]] DECLARE_META_INTERFACE(INTERFACE)                               \
    static const android::String16 descriptor;                          \
    static android::sp<IServiceManager> asInterface(                       \
            const android==sp<android==IBinder>& obj);                  \
    virtual const android::String16& getInterfaceDescriptor() const;    \
    IServiceManager();                                                     \
    virtual ~IServiceManager();   




android==sp<IServiceManager> IServiceManager==asInterface(                \
            const android==sp<android==IBinder>& obj)                   \
    {                                                                   \
        android::sp<IServiceManager> intr;                                 \
        if (obj != NULL) {                                              \
            intr = static_cast<IServiceManager*>(                          \
                obj->queryLocalInterface(                               \
                        IServiceManager::descriptor).get());               \
            if (intr == NULL) {                                         \
                intr = new BpServiceManager(obj);                          \
            }                                                           \
        }                                                               \
        return intr;                                                    \
    }           

相当于我们调用了BpServiceManager(BpBinder(0));//这样调用的

```

创建了BpServiceManager。

```
    BpServiceManager(const sp<IBinder>& impl)
        : BpInterface<IServiceManager>(impl)
    {
    }
```

所以相当于BpServiceManager继承BpInterface，BpInterface继承自BpRefBase。所以我们可以看到mRemote就是BpBinder。

```
BpRefBase::BpRefBase(const sp<IBinder>& o)
    : mRemote(o.get()), mRefs(NULL), mState(0)
{//mRemote内部会调用BpBinder相关的
    extendObjectLifetime(OBJECT_LIFETIME_WEAK);
    if (mRemote) {
        mRemote->incStrong(this);           // Removed on first IncStrong().
        mRefs = mRemote->createWeak(this);  // Held for our entire lifetime.
    }
}
```

这样我们就获取到了BpServiceManager这个其实就是Java层的ServiceManagerProxy，就是ServiceManager

#### 4.服务的添加

开启system\_server之后，就需要将服务添加到ServiceManager中去了。我们看看system\_server是怎么添加的呢？

启动SystemServer的run方法。

```
   public static void main(String[] args) {
        new SystemServer().run();
    }
```

函数太长，我们看看关键的地方:

```
 startBootstrapServices();//开启引导服务
 startCoreServices();//开启核心服务
 startOtherServices();//开启其他服务
```

例如ActivityManagerService就是在引导服务中。

```
mActivityManagerService = ActivityManagerService.Lifecycle.startService(
                mSystemServiceManager, atm);
mActivityManagerService.setSystemProcess();
//通过反射创建了ActivityManagerService
//调用SystemServiceManager的start方法
public static ActivityManagerService startService(
                SystemServiceManager ssm, ActivityTaskManagerService atm) {
            sAtm = atm;
            return ssm.startService(ActivityManagerService.Lifecycle.class).getService();
      }

//将服务添加到SystemServiceManager中的mServices集合中并调用ActivityManager的onStart函数
    public void startService(@NonNull final SystemService service) {
        // Register it.
        mServices.add(service);
        // Start it.
        long time = SystemClock.elapsedRealtime();
        try {
            service.onStart();
        } catch (RuntimeException ex) {
            throw new RuntimeException("Failed to start service " + service.getClass().getName()
                    + ": onStart threw an exception", ex);
        }
        warnIfTooLong(SystemClock.elapsedRealtime() - time, service, "onStart");
    }
```

这里就完成了ams的注册，注册到了SystemManagerService中。接着会调用ActivityManagerService的setSystemProcess把自己添加到ServiceManager中。包括添加了进程的状态，内存信息，graphics等。

```
public void setSystemProcess() {
        try {
            ServiceManager.addService(Context.ACTIVITY_SERVICE, this, /* allowIsolated= */ true,
                    DUMP_FLAG_PRIORITY_CRITICAL | DUMP_FLAG_PRIORITY_NORMAL | DUMP_FLAG_PROTO);
            ServiceManager.addService(ProcessStats.SERVICE_NAME, mProcessStats);
            ServiceManager.addService("meminfo", new MemBinder(this), /* allowIsolated= */ false,
                    DUMP_FLAG_PRIORITY_HIGH);
            ServiceManager.addService("gfxinfo", new GraphicsBinder(this));
            ServiceManager.addService("dbinfo", new DbBinder(this));
            if (MONITOR_CPU_USAGE) {
                ServiceManager.addService("cpuinfo", new CpuBinder(this),
                        /* allowIsolated= */ false, DUMP_FLAG_PRIORITY_CRITICAL);
            }
            ServiceManager.addService("permission", new PermissionController(this));
            ServiceManager.addService("processinfo", new ProcessInfoService(this));

            ApplicationInfo info = mContext.getPackageManager().getApplicationInfo(
                    "android", STOCK_PM_FLAGS | MATCH_SYSTEM_ONLY);
            mSystemThread.installSystemApplicationInfo(info, getClass().getClassLoader());

            synchronized (this) {
                ProcessRecord app = mProcessList.newProcessRecordLocked(info, info.processName,
                        false,
                        0,
                        new HostingRecord("system"));
                app.setPersistent(true);
                app.pid = MY_PID;
                app.getWindowProcessController().setPid(MY_PID);
                app.maxAdj = ProcessList.SYSTEM_ADJ;
                app.makeActive(mSystemThread.getApplicationThread(), mProcessStats);
                mPidsSelfLocked.put(app);
                mProcessList.updateLruProcessLocked(app, false, null);
                updateOomAdjLocked(OomAdjuster.OOM_ADJ_REASON_NONE);
            }
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException(
                    "Unable to find android system package", e);
        }
        // Start watching app ops after we and the package manager are up and running.
        mAppOpsService.startWatchingMode(AppOpsManager.OP_RUN_IN_BACKGROUND, null,
                new IAppOpsCallback.Stub() {
                    @Override public void opChanged(int op, int uid, String packageName) {
                        if (op == AppOpsManager.OP_RUN_IN_BACKGROUND && packageName != null) {
                            if (mAppOpsService.checkOperation(op, uid, packageName)
                                    != AppOpsManager.MODE_ALLOWED) {
                                runInBackgroundDisabled(uid);
                            }
                        }
                    }
                });
    }
```

当调用addService的时候就调用的是ServiceManagerProxy的addService方法。

```
   public void addService(String name, IBinder service, boolean allowIsolated, int dumpPriority)
            throws RemoteException {
        Parcel data = Parcel.obtain();
        Parcel reply = Parcel.obtain();
        data.writeInterfaceToken(IServiceManager.descriptor);
        data.writeString(name);
     //将ActivityManagerService添加进去。
        data.writeStrongBinder(service);
        data.writeInt(allowIsolated ? 1 : 0);
        data.writeInt(dumpPriority);
     //进行通信
        mRemote.transact(ADD_SERVICE_TRANSACTION, data, reply, 0);
        reply.recycle();
        data.recycle();
    }
```

调用parcel.writeStrongBinder将binder存入Parcel。

```
status_t Parcel::writeStrongBinder(const sp<IBinder>& val)
{
    return flatten_binder(ProcessState::self(), val, this);
}
```

```
status_t flatten_binder(const sp<ProcessState>& /*proc*/,
    const sp<IBinder>& binder, Parcel* out)
{
    flat_binder_object obj;
    if (IPCThreadState::self()->backgroundSchedulingDisabled()) {
        obj.flags = FLAT_BINDER_FLAG_ACCEPTS_FDS;
    } else {
        obj.flags = 0x13 | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    }
    if (binder != nullptr) {//binder是ams，这个binder是BBinder
        BBinder *local = binder->localBinder();
        if (!local) {
            BpBinder *proxy = binder->remoteBinder();
            if (proxy == nullptr) {
                ALOGE("null proxy");
            }
            const int32_t handle = proxy ? proxy->handle() : 0;
            obj.hdr.type = BINDER_TYPE_HANDLE;
            obj.binder = 0; /* Don't pass uninitialized stack data to a remote process */
            obj.handle = handle;
            obj.cookie = 0;
        } else {
            if (local->isRequestingSid()) {
                obj.flags |= FLAT_BINDER_FLAG_TXN_SECURITY_CTX;
            }
            obj.hdr.type = BINDER_TYPE_BINDER;//指令就是BINDER_TYPE_BINDER.
            obj.binder = reinterpret_cast<uintptr_t>(local->getWeakRefs());
            obj.cookie = reinterpret_cast<uintptr_t>(local);
        }
    } else {
        obj.hdr.type = BINDER_TYPE_BINDER;
        obj.binder = 0;
        obj.cookie = 0;
    }
    return finish_flatten_binder(binder, obj, out);
}

//BBinder的localBinder返回的是this
BBinder* BBinder::localBinder()
{
    return this;
}
//把obj写入parcel
inline static status_t finish_flatten_binder(
    const sp<IBinder>& /*binder*/, const flat_binder_object& flat, Parcel* out)
{
    return out->writeObject(flat, false);
}
```

调用transact进行数据传递，调用的是BpBinder的transact(ADD\_SERVICE\_TRANSACTION)

```
//注意命令传递的是ADD_SERVICE_TRANSACTION
static jboolean android_os_BinderProxy_transact(JNIEnv* env, jobject obj,
        jint code, jobject dataObj, jobject replyObj, jint flags) // throws RemoteException
{
    Parcel* data = parcelForJavaObject(env, dataObj);
    Parcel* reply = parcelForJavaObject(env, replyObj);
  //获取到对应的BpBinder
    IBinder* target = getBPNativeData(env, obj)->mObject.get();
    bool time_binder_calls;
    int64_t start_millis;
    if (kEnableBinderSample) {
        time_binder_calls = should_time_binder_calls();
        if (time_binder_calls) {
            start_millis = uptimeMillis();
        }
    }
    status_t err = target->transact(code, *data, reply, flags);
    if (kEnableBinderSample) {
        if (time_binder_calls) {
            conditionally_log_binder_call(start_millis, target, code);
        }
    }

    if (err == NO_ERROR) {
        return JNI_TRUE;
    } 
    signalExceptionForError(env, obj, err, true /*canThrowRemoteException*/, data->dataSize());
    return JNI_FALSE;
}
```

调用BpBinder的transact进行数据传递，其实还是调用的IPCThread::self()->transact()

```
status_t BpBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    if (mAlive) {//调用IPCThreadState的transact进行数据通信
        status_t status = IPCThreadState::self()->transact(
            mHandle, code, data, reply, flags);
        if (status == DEAD_OBJECT) mAlive = 0;
        return status;
    }

    return DEAD_OBJECT;
}
```

IPCThreadState的transact进行数据的传递:

```
status_t IPCThreadState::transact(int32_t handle,
                                  uint32_t code, const Parcel& data,
                                  Parcel* reply, uint32_t flags)
{
    status_t err = data.errorCheck();
    flags |= TF_ACCEPT_FDS;
    IF_LOG_TRANSACTIONS() {
        TextOutput::Bundle_b(alog);
    }
    if (err == NO_ERROR) {//给binder驱动写入命令BC_TRANSACTION，code是？
        err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, NULL);
    }
    if (err != NO_ERROR) {
        if (reply) reply->setError(err);
        return (mLastError = err);
    }
    if ((flags & TF_ONE_WAY) == 0) {//同步
        if (reply) {//调用waitForResponse
            err = waitForResponse(reply);
        } else {
            Parcel fakeReply;
            err = waitForResponse(&fakeReply);
        }
        IF_LOG_TRANSACTIONS() {
            TextOutput::Bundle _b(alog);
            alog << "BR_REPLY thr " << (void*)pthread_self() << " / hand "
                << handle << ": ";
            if (reply) alog << indent << *reply << dedent << endl;
            else alog << "(none requested)" << endl;
        }
    } else {//异步
        err = waitForResponse(NULL, NULL);
    }
    return err;
}

//writeTransactionData(BC_TRANSACTION, flags, handle, code, data, NULL)
status_t IPCThreadState::writeTransactionData(int32_t cmd, uint32_t binderFlags,
    int32_t handle, uint32_t code, const Parcel& data, status_t* statusBuffer)
{
    binder_transaction_data tr;
    tr.target.ptr = 0;
    tr.target.handle = handle;//此时的handle是？
    tr.code = code;
    tr.flags = binderFlags;
    tr.cookie = 0;
    tr.sender_pid = 0;
    tr.sender_euid = 0;
    const status_t err = data.errorCheck();
    if (err == NO_ERROR) {
        tr.data_size = data.ipcDataSize();
        tr.data.ptr.buffer = data.ipcData();
        tr.offsets_size = data.ipcObjectsCount()*sizeof(binder_size_t);
        tr.data.ptr.offsets = data.ipcObjects();
    } else if (statusBuffer) {
        tr.flags |= TF_STATUS_CODE;
        *statusBuffer = err;
        tr.data_size = sizeof(status_t);
        tr.data.ptr.buffer = reinterpret_cast<uintptr_t>(statusBuffer);
        tr.offsets_size = 0;
        tr.data.ptr.offsets = 0;
    } else {
        return (mLastError = err);
    }//把命令写入out，命令是BC_TRANSACTION
    mOut.writeInt32(cmd);
    mOut.write(&tr, sizeof(tr));
    return NO_ERROR;
}

```

调用waitForResponse()。

```
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    uint32_t cmd;
    int32_t err;
    while (1) {//这里特别重要↓
        if ((err=talkWithDriver()) < NO_ERROR) break;
        err = mIn.errorCheck();
        if (err < NO_ERROR) break;
        if (mIn.dataAvail() == 0) continue;
        cmd = (uint32_t)mIn.readInt32();
        IF_LOG_COMMANDS() {
            alog << "Processing waitForResponse Command: "
                << getReturnString(cmd) << endl;
        }
        switch (cmd) {
        case BR_TRANSACTION_COMPLETE://拿到返回值,reply不为null 所以会在这里继续循环。继续走入talkWithDriver,此时是readBuff是有值的所以走到binder_thread_read方法。
            if (!reply && !acquireResult) goto finish;
            break;
        case BR_DEAD_REPLY:
            err = DEAD_OBJECT;
            goto finish;
        case BR_FAILED_REPLY:
            err = FAILED_TRANSACTION;
            goto finish;
        case BR_ACQUIRE_RESULT:
            {
                ALOG_ASSERT(acquireResult != NULL, "Unexpected brACQUIRE_RESULT");
                const int32_t result = mIn.readInt32();
                if (!acquireResult) continue;
                *acquireResult = result ? NO_ERROR : INVALID_OPERATION;
            }
            goto finish;
        case BR_REPLY://后边唤醒服务端后也需要处理BR_REPLY
            {
                binder_transaction_data tr;
                err = mIn.read(&tr, sizeof(tr));
                ALOG_ASSERT(err == NO_ERROR, "Not enough command data for brREPLY");
                if (err != NO_ERROR) goto finish;

                if (reply) {
                    if ((tr.flags & TF_STATUS_CODE) == 0) {
                        reply->ipcSetDataReference(
                            reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                            tr.data_size,
                            reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                            tr.offsets_size/sizeof(binder_size_t),
                            freeBuffer, this);
                    } else {
                        err = *reinterpret_cast<const status_t*>(tr.data.ptr.buffer);
                        freeBuffer(nullptr,
                            reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                            tr.data_size,
                            reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                            tr.offsets_size/sizeof(binder_size_t), this);
                    }
                } else {
                    freeBuffer(nullptr,
                        reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                        tr.data_size,
                        reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                        tr.offsets_size/sizeof(binder_size_t), this);
                    continue;
                }
            }
            goto finish;
        default://服务端返回的数据也需要经过这里。
            err = executeCommand(cmd);
            if (err != NO_ERROR) goto finish;
            break;
        }
    }
finish:
    if (err != NO_ERROR) {
        if (acquireResult) *acquireResult = err;
        if (reply) reply->setError(err);
        mLastError = err;
    }
    return err;
}




//talkWithDriver() 跟binder驱动进行通信
status_t IPCThreadState::talkWithDriver(bool doReceive)
{
    if (mProcess->mDriverFD <= 0) {
        return -EBADF;
    }
    binder_write_read bwr;
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();
    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;
    bwr.write_size = outAvail;
    bwr.write_buffer = (uintptr_t)mOut.data();
    if (doReceive && needRead) {
        bwr.read_size = mIn.dataCapacity();
        bwr.read_buffer = (uintptr_t)mIn.data();
    } else {
        bwr.read_size = 0;
        bwr.read_buffer = 0;
    }
    if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;
    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    status_t err;
    do {
[[if]] defined(__ANDROID__)//在这里对binder进行写数据的操作,把bwr写进去 bwr的命令是BC_TRANSACTION
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
        else
            err = -errno;
[[else]]
        err = INVALID_OPERATION;
[[endif]]
        if (mProcess->mDriverFD <= 0) {
            err = -EBADF;
        }
    } while (err == -EINTR);
    if (err >= NO_ERROR) {
        if (bwr.write_consumed > 0) {
            if (bwr.write_consumed < mOut.dataSize())
                mOut.remove(0, bwr.write_consumed);
            else {
                mOut.setDataSize(0);
                processPostWriteDerefs();
            }
        }
        if (bwr.read_consumed > 0) {
            mIn.setDataSize(bwr.read_consumed);
            mIn.setDataPosition(0);
        }
        return NO_ERROR;
    }

    return err;
}


//binder_thread_write 
case BC_TRANSACTION:
  case BC_REPLY: {//到这里来了BC_REPLY
   struct binder_transaction_data tr;
   if (copy_from_user(&tr, ptr, sizeof(tr)))
    return -EFAULT;
   ptr += sizeof(tr);//注意此时的cmd是什么.
   binder_transaction(proc, thread, &tr, cmd == BC_REPLY);
   break;
 }

//complete之后进入binder_thread_read
//wait_event_freezable_exclusive()调用让客户端心如休眠状态

//对binder的数据进行读写操作
static void binder_transaction(struct binder_proc *proc,
                   struct binder_thread *thread,
                   struct binder_transaction_data *tr, int reply)
{
    struct binder_transaction *t;
    struct binder_work *tcomplete;
    binder_size_t *offp, *off_end;
    struct binder_proc *target_proc;
    struct binder_thread *target_thread = NULL;
    struct binder_node *target_node = NULL;
    struct list_head *target_list;
    wait_queue_head_t *target_wait;
    struct binder_transaction *in_reply_to = NULL;
    struct binder_transaction_log_entry *e;
    uint32_t return_error;

    e = binder_transaction_log_add(&binder_transaction_log);
    e->call_type = reply ? 2 : !!(tr->flags & TF_ONE_WAY);
    e->from_proc = proc->pid;
    e->from_thread = thread->pid;
    e->target_handle = tr->target.handle;
    e->data_size = tr->data_size;
    e->offsets_size = tr->offsets_size;
    if (reply) {
        in_reply_to = thread->transaction_stack;
        if (in_reply_to == NULL) {
            binder_user_error("%d:%d got reply transaction with no transaction stack\n",
                      proc->pid, thread->pid);
            return_error = BR_FAILED_REPLY;
            goto err_empty_call_stack;
        }
        binder_set_nice(in_reply_to->saved_priority);
        if (in_reply_to->to_thread != thread) {
            binder_user_error("%d:%d got reply transaction with bad transaction stack, transaction %d has target %d:%d\n",
                proc->pid, thread->pid, in_reply_to->debug_id,
                in_reply_to->to_proc ?
                in_reply_to->to_proc->pid : 0,
                in_reply_to->to_thread ?
                in_reply_to->to_thread->pid : 0);
            return_error = BR_FAILED_REPLY;
            in_reply_to = NULL;
            goto err_bad_call_stack;
        }
        thread->transaction_stack = in_reply_to->to_parent;
        target_thread = in_reply_to->from;
        if (target_thread == NULL) {
            return_error = BR_DEAD_REPLY;
            goto err_dead_binder;
        }
        if (target_thread->transaction_stack != in_reply_to) {
            binder_user_error("%d:%d got reply transaction with bad target transaction stack %d, expected %d\n",
                proc->pid, thread->pid,
                target_thread->transaction_stack ?
                target_thread->transaction_stack->debug_id : 0,
                in_reply_to->debug_id);
            return_error = BR_FAILED_REPLY;
            in_reply_to = NULL;
            target_thread = NULL;
            goto err_dead_binder;
        }
        target_proc = target_thread->proc;
    } else {//此时reply是false
        if (tr->target.handle) {//此时的handle是？
            struct binder_ref *ref;
            ref = binder_get_ref(proc, tr->target.handle);
            if (ref == NULL) {
                binder_user_error("%d:%d got transaction to invalid handle\n",
                    proc->pid, thread->pid);
                return_error = BR_FAILED_REPLY;
                goto err_invalid_target_handle;
            }
            target_node = ref->node;
        } else {//得到服务管理者，还记得是谁吗？
            target_node = binder_context_mgr_node;
            if (target_node == NULL) {
                return_error = BR_DEAD_REPLY;
                goto err_no_context_mgr_node;
            }
        }
        e->to_node = target_node->debug_id;
        target_proc = target_node->proc;//目标进程也就是ServiceManager
        if (target_proc == NULL) {
            return_error = BR_DEAD_REPLY;
            goto err_dead_binder;
        }
        if (!(tr->flags & TF_ONE_WAY) && thread->transaction_stack) {
            struct binder_transaction *tmp;

            tmp = thread->transaction_stack;
            if (tmp->to_thread != thread) {
                binder_user_error("%d:%d got new transaction with bad transaction stack, transaction %d has target %d:%d\n",
                    proc->pid, thread->pid, tmp->debug_id,
                    tmp->to_proc ? tmp->to_proc->pid : 0,
                    tmp->to_thread ?
                    tmp->to_thread->pid : 0);
                return_error = BR_FAILED_REPLY;
                goto err_bad_call_stack;
            }
            while (tmp) {
                if (tmp->from && tmp->from->proc == target_proc)
                    target_thread = tmp->from;
                tmp = tmp->from_parent;
            }
        }
    }
    if (target_thread) {//拿到todo 和wait队列
        e->to_thread = target_thread->pid;
        target_list = &target_thread->todo;
        target_wait = &target_thread->wait;
    } else {
        target_list = &target_proc->todo;
        target_wait = &target_proc->wait;
    }
    e->to_proc = target_proc->pid;
  //初始化t
    t = kzalloc(sizeof(*t), GFP_KERNEL);
    if (t == NULL) {
        return_error = BR_FAILED_REPLY;
        goto err_alloc_t_failed;
    }
    binder_stats_created(BINDER_STAT_TRANSACTION);
//初始化tcompilete
    tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);
    if (tcomplete == NULL) {
        return_error = BR_FAILED_REPLY;
        goto err_alloc_tcomplete_failed;
    }
    binder_stats_created(BINDER_STAT_TRANSACTION_COMPLETE);
    t->debug_id = ++binder_last_id;
    e->debug_id = t->debug_id;

    if (reply)
        binder_debug(BINDER_DEBUG_TRANSACTION,
                 "%d:%d BC_REPLY %d -> %d:%d, data %016llx-%016llx size %lld-%lld\n",
                 proc->pid, thread->pid, t->debug_id,
                 target_proc->pid, target_thread->pid,
                 (u64)tr->data.ptr.buffer,
                 (u64)tr->data.ptr.offsets,
                 (u64)tr->data_size, (u64)tr->offsets_size);
    else
        binder_debug(BINDER_DEBUG_TRANSACTION,
                 "%d:%d BC_TRANSACTION %d -> %d - node %d, data %016llx-%016llx size %lld-%lld\n",
                 proc->pid, thread->pid, t->debug_id,
                 target_proc->pid, target_node->debug_id,
                 (u64)tr->data.ptr.buffer,
                 (u64)tr->data.ptr.offsets,
                 (u64)tr->data_size, (u64)tr->offsets_size);

    if (!reply && !(tr->flags & TF_ONE_WAY))
        t->from = thread;
    else
        t->from = NULL;
  //把SM 相关的东西存入t
    t->sender_euid = task_euid(proc->tsk);
    t->to_proc = target_proc;
    t->to_thread = target_thread;
    t->code = tr->code;
    t->flags = tr->flags;
    t->priority = task_nice(current);

    trace_binder_transaction(reply, t, target_node);
//把t->buffer指向目标进程的内存空间。
    t->buffer = binder_alloc_buf(target_proc, tr->data_size,
        tr->offsets_size, !reply && (t->flags & TF_ONE_WAY));
    if (t->buffer == NULL) {
        return_error = BR_FAILED_REPLY;
        goto err_binder_alloc_buf_failed;
    }
    t->buffer->allow_user_free = 0;
    t->buffer->debug_id = t->debug_id;
    t->buffer->transaction = t;
    t->buffer->target_node = target_node;
    trace_binder_transaction_alloc_buf(t->buffer);
    if (target_node)
        binder_inc_node(target_node, 1, 0, NULL);

    offp = (binder_size_t *)(t->buffer->data +
                 ALIGN(tr->data_size, sizeof(void *)));
//数据在这里进行拷贝。拷贝到共享区域
    if (copy_from_user(t->buffer->data, (const void __user *)(uintptr_t)
               tr->data.ptr.buffer, tr->data_size)) {
        binder_user_error("%d:%d got transaction with invalid data ptr\n",
                proc->pid, thread->pid);
        return_error = BR_FAILED_REPLY;
        goto err_copy_data_failed;
    }
    if (copy_from_user(offp, (const void __user *)(uintptr_t)
               tr->data.ptr.offsets, tr->offsets_size)) {
        binder_user_error("%d:%d got transaction with invalid offsets ptr\n",
                proc->pid, thread->pid);
        return_error = BR_FAILED_REPLY;
        goto err_copy_data_failed;
    }
    if (!IS_ALIGNED(tr->offsets_size, sizeof(binder_size_t))) {
        binder_user_error("%d:%d got transaction with invalid offsets size, %lld\n",
                proc->pid, thread->pid, (u64)tr->offsets_size);
        return_error = BR_FAILED_REPLY;
        goto err_bad_offset;
    }
    off_end = (void *)offp + tr->offsets_size;
    for (; offp < off_end; offp++) {
        struct flat_binder_object *fp;

        if (*offp > t->buffer->data_size - sizeof(*fp) ||
            t->buffer->data_size < sizeof(*fp) ||
            !IS_ALIGNED(*offp, sizeof(u32))) {
            binder_user_error("%d:%d got transaction with invalid offset, %lld\n",
                      proc->pid, thread->pid, (u64)*offp);
            return_error = BR_FAILED_REPLY;
            goto err_bad_offset;
        }
        fp = (struct flat_binder_object *)(t->buffer->data + *offp);
        switch (fp->type) {
        case BINDER_TYPE_BINDER:
        case BINDER_TYPE_WEAK_BINDER: {
            struct binder_ref *ref;
            struct binder_node *node = binder_get_node(proc, fp->binder);

            if (node == NULL) {
                node = binder_new_node(proc, fp->binder, fp->cookie);
                if (node == NULL) {
                    return_error = BR_FAILED_REPLY;
                    goto err_binder_new_node_failed;
                }
                node->min_priority = fp->flags & FLAT_BINDER_FLAG_PRIORITY_MASK;
                node->accept_fds = !!(fp->flags & FLAT_BINDER_FLAG_ACCEPTS_FDS);
            }
            if (fp->cookie != node->cookie) {
                binder_user_error("%d:%d sending u%016llx node %d, cookie mismatch %016llx != %016llx\n",
                    proc->pid, thread->pid,
                    (u64)fp->binder, node->debug_id,
                    (u64)fp->cookie, (u64)node->cookie);
                return_error = BR_FAILED_REPLY;
                goto err_binder_get_ref_for_node_failed;
            }
            ref = binder_get_ref_for_node(target_proc, node);
            if (ref == NULL) {
                return_error = BR_FAILED_REPLY;
                goto err_binder_get_ref_for_node_failed;
            }
            if (fp->type == BINDER_TYPE_BINDER)
                fp->type = BINDER_TYPE_HANDLE;
            else
                fp->type = BINDER_TYPE_WEAK_HANDLE;
            fp->handle = ref->desc;
            binder_inc_ref(ref, fp->type == BINDER_TYPE_HANDLE,
                       &thread->todo);

            trace_binder_transaction_node_to_ref(t, node, ref);
            binder_debug(BINDER_DEBUG_TRANSACTION,
                     "        node %d u%016llx -> ref %d desc %d\n",
                     node->debug_id, (u64)node->ptr,
                     ref->debug_id, ref->desc);
        } break;
        case BINDER_TYPE_HANDLE:
        case BINDER_TYPE_WEAK_HANDLE: {
            struct binder_ref *ref = binder_get_ref(proc, fp->handle);

            if (ref == NULL) {
                binder_user_error("%d:%d got transaction with invalid handle, %d\n",
                        proc->pid,
                        thread->pid, fp->handle);
                return_error = BR_FAILED_REPLY;
                goto err_binder_get_ref_failed;
            }
            if (ref->node->proc == target_proc) {
                if (fp->type == BINDER_TYPE_HANDLE)
                    fp->type = BINDER_TYPE_BINDER;
                else
                    fp->type = BINDER_TYPE_WEAK_BINDER;
                fp->binder = ref->node->ptr;
                fp->cookie = ref->node->cookie;
                binder_inc_node(ref->node, fp->type == BINDER_TYPE_BINDER, 0, NULL);
                trace_binder_transaction_ref_to_node(t, ref);
                binder_debug(BINDER_DEBUG_TRANSACTION,
                         "        ref %d desc %d -> node %d u%016llx\n",
                         ref->debug_id, ref->desc, ref->node->debug_id,
                         (u64)ref->node->ptr);
            } else {
                struct binder_ref *new_ref;

                new_ref = binder_get_ref_for_node(target_proc, ref->node);
                if (new_ref == NULL) {
                    return_error = BR_FAILED_REPLY;
                    goto err_binder_get_ref_for_node_failed;
                }
                fp->handle = new_ref->desc;
                binder_inc_ref(new_ref, fp->type == BINDER_TYPE_HANDLE, NULL);
                trace_binder_transaction_ref_to_ref(t, ref,
                                    new_ref);
                binder_debug(BINDER_DEBUG_TRANSACTION,
                         "        ref %d desc %d -> ref %d desc %d (node %d)\n",
                         ref->debug_id, ref->desc, new_ref->debug_id,
                         new_ref->desc, ref->node->debug_id);
            }
        } break;

        case BINDER_TYPE_FD: {
            int target_fd;
            struct file *file;

            if (reply) {
                if (!(in_reply_to->flags & TF_ACCEPT_FDS)) {
                    binder_user_error("%d:%d got reply with fd, %d, but target does not allow fds\n",
                        proc->pid, thread->pid, fp->handle);
                    return_error = BR_FAILED_REPLY;
                    goto err_fd_not_allowed;
                }
            } else if (!target_node->accept_fds) {
                binder_user_error("%d:%d got transaction with fd, %d, but target does not allow fds\n",
                    proc->pid, thread->pid, fp->handle);
                return_error = BR_FAILED_REPLY;
                goto err_fd_not_allowed;
            }

            file = fget(fp->handle);
            if (file == NULL) {
                binder_user_error("%d:%d got transaction with invalid fd, %d\n",
                    proc->pid, thread->pid, fp->handle);
                return_error = BR_FAILED_REPLY;
                goto err_fget_failed;
            }
            target_fd = task_get_unused_fd_flags(target_proc, O_CLOEXEC);
            if (target_fd < 0) {
                fput(file);
                return_error = BR_FAILED_REPLY;
                goto err_get_unused_fd_failed;
            }
            task_fd_install(target_proc, target_fd, file);
            trace_binder_transaction_fd(t, fp->handle, target_fd);
            binder_debug(BINDER_DEBUG_TRANSACTION,
                     "        fd %d -> %d\n", fp->handle, target_fd);
            /* TODO: fput? */
            fp->handle = target_fd;
        } break;
        default:
            binder_user_error("%d:%d got transaction with invalid object type, %x\n",
                proc->pid, thread->pid, fp->type);
            return_error = BR_FAILED_REPLY;
            goto err_bad_object_type;
        }
    }
    if (reply) {
        BUG_ON(t->buffer->async_transaction != 0);
        binder_pop_transaction(target_thread, in_reply_to);
    } else if (!(t->flags & TF_ONE_WAY)) {//同步
        BUG_ON(t->buffer->async_transaction != 0);
        t->need_reply = 1;
        t->from_parent = thread->transaction_stack;
        thread->transaction_stack = t;//保存t，也就是保存客户端信息。
    } else {//异步
        BUG_ON(target_node == NULL);
        BUG_ON(t->buffer->async_transaction != 1);
        if (target_node->has_async_transaction) {
            target_list = &target_node->async_todo;
            target_wait = NULL;
        } else
            target_node->has_async_transaction = 1;
    }
    t->work.type = BINDER_WORK_TRANSACTION;//指定客户端的work.type = BINDER_WORK_TRANSACTION
    list_add_tail(&t->work.entry, target_list);
    tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;//tcomplete->type=BINDER_WORK_TRANSACTION_COMPLETE；告诉客户端数据传递完成，你可以休息会了
    list_add_tail(&tcomplete->entry, &thread->todo);
    if (target_wait)//target_wait 唤醒SM
        wake_up_interruptible(target_wait);
    return;
}
//那么他是如何挂起客户端的呢？大家是否还记得之前，binder读写的顺序？没错先写后读取，所以此时我们的readBuff是有值的。
case BINDER_WORK_TRANSACTION_COMPLETE: {//设置一些状态信息。把BR_TRANSACTION_COMPLETE存入用户空间,这个时候talkWithDriver执行完成
   cmd = BR_TRANSACTION_COMPLETE;
   if (put_user(cmd, (uint32_t __user *)ptr))
    return -EFAULT;
   ptr += sizeof(uint32_t);
   binder_stat_br(proc, thread, cmd);
   list_del(&w->entry);
   kfree(w);
   binder_stats_deleted(BINDER_STAT_TRANSACTION_COMPLETE);
  } break;

//SM的处理：在SM挂起的地方进行唤醒binder_thread_read 然后执行BINDER_WORK_TRANSACTION
case BINDER_WORK_TRANSACTION: {//命令的处理 调整work队列。
   t = container_of(w, struct binder_transaction, work);
  } break;
if (!t)
   continue;
  BUG_ON(t->buffer == NULL);
  if (t->buffer->target_node) {//唤醒客户端后这里不会进入
   struct binder_node *target_node = t->buffer->target_node;
   tr.target.ptr = target_node->ptr;
   tr.cookie =  target_node->cookie;
   t->saved_priority = task_nice(current);
   if (t->priority < target_node->min_priority &&
       !(t->flags & TF_ONE_WAY))
    binder_set_nice(t->priority);
   else if (!(t->flags & TF_ONE_WAY) ||
     t->saved_priority > target_node->min_priority)
    binder_set_nice(target_node->min_priority);
   cmd = BR_TRANSACTION;//发送BR_TRANSACTION命令给SM。唤醒后，继续binder_loop循环对命令进行解析。binder_parse
  } else {//唤醒客户端后命令是BR_REPLY，所以客户端需要处理BR_REPLY，然后执行到IPCThreadState.waitResponse()//执行BR_REPLY收尾工作
   tr.target.ptr = 0;
   tr.cookie = 0;
   cmd = BR_REPLY;
  }
  tr.code = t->code;
  tr.flags = t->flags;
  tr.sender_euid = from_kuid(current_user_ns(), t->sender_euid);

  if (t->from) {
   struct task_struct *sender = t->from->proc->tsk;

   tr.sender_pid = task_tgid_nr_ns(sender,
       task_active_pid_ns(current));
  } else {
   tr.sender_pid = 0;
  }

//binder_loop收到唤醒后，解析命令 命令就是binder给的BR_TRANSACTION
int binder_parse(struct binder_state *bs, struct binder_io *bio,
                 uintptr_t ptr, size_t size, binder_handler func)
{
    int r = 1;
    uintptr_t end = ptr + (uintptr_t) size;

    while (ptr < end) {
        uint32_t cmd = *(uint32_t *) ptr;
        ptr += sizeof(uint32_t);
[[if]] TRACE
        fprintf(stderr,"%s:\n", cmd_name(cmd));
[[endif]]
        switch(cmd) {
        case BR_NOOP:
            break;
        case BR_TRANSACTION_COMPLETE:
            break;
        case BR_INCREFS:
        case BR_ACQUIRE:
        case BR_RELEASE:
        case BR_DECREFS:
[[if]] TRACE
            fprintf(stderr,"  %p, %p\n", (void *)ptr, (void *)(ptr + sizeof(void *)));
[[endif]]
            ptr += sizeof(struct binder_ptr_cookie);
            break;
        case BR_TRANSACTION_SEC_CTX:
        case BR_TRANSACTION: {//在这里对数据进行获取
            struct binder_transaction_data_secctx txn;
            if (cmd == BR_TRANSACTION_SEC_CTX) {
                if ((end - ptr) < sizeof(struct binder_transaction_data_secctx)) {
                    ALOGE("parse: txn too small (binder_transaction_data_secctx)!\n");
                    return -1;
                }
                memcpy(&txn, (void*) ptr, sizeof(struct binder_transaction_data_secctx));
                ptr += sizeof(struct binder_transaction_data_secctx);
            } else /* BR_TRANSACTION */ {
                if ((end - ptr) < sizeof(struct binder_transaction_data)) {
                    ALOGE("parse: txn too small (binder_transaction_data)!\n");
                    return -1;
                }
                memcpy(&txn.transaction_data, (void*) ptr, sizeof(struct binder_transaction_data));
                ptr += sizeof(struct binder_transaction_data);

                txn.secctx = 0;
            }

            binder_dump_txn(&txn.transaction_data);
            if (func) {//这个func 还有记得的吗？
                unsigned rdata[256/4];
                struct binder_io msg;
                struct binder_io reply;
                int res;

                bio_init(&reply, rdata, sizeof(rdata), 4);
                bio_init_from_txn(&msg, &txn.transaction_data);
                res = func(bs, &txn, &msg, &reply);
                //把处理结果返回回去
                binder_send_reply(bs, &reply, txn.transaction_data.data.ptr.buffer, res);
            }
            break;
        }
        case BR_REPLY: {
            struct binder_transaction_data *txn = (struct binder_transaction_data *) ptr;
            if ((end - ptr) < sizeof(*txn)) {
                return -1;
            }
            binder_dump_txn(txn);
            if (bio) {
                bio_init_from_txn(bio, txn);
                bio = 0;
            } else {
                /* todo FREE BUFFER */
            }
            ptr += sizeof(*txn);
            r = 0;
            break;
        }
        case BR_DEAD_BINDER: {
            struct binder_death *death = (struct binder_death *)(uintptr_t) *(binder_uintptr_t *)ptr;
            ptr += sizeof(binder_uintptr_t);
            death->func(bs, death->ptr);
            break;
        }
        case BR_FAILED_REPLY:
            r = -1;
            break;
        case BR_DEAD_REPLY:
            r = -1;
            break;
        default:
            ALOGE("parse: OOPS %d\n", cmd);
            return -1;
        }
    }

    return r;
}
//执行完read之后

```

binder\_send\_reply:把数据返回给binder。

```

void binder_send_reply(struct binder_state *bs,
                       struct binder_io *reply,
                       binder_uintptr_t buffer_to_free,
                       int status)
{
    struct {
        uint32_t cmd_free;
        binder_uintptr_t buffer;
        uint32_t cmd_reply;
        struct binder_transaction_data txn;
    } __attribute__((packed)) data;
    data.cmd_free = BC_FREE_BUFFER;//释放buffer
    data.buffer = buffer_to_free;
    data.cmd_reply = BC_REPLY;//回复命令 返回上边的binder_thread_write 数据处理完成后会给客户端发送 BINDER_WORK_TRANSACTION唤醒客户端， 服务端自己处理BINDER_WORK_TRANSACTION_COMPLETE进入挂起状态。
    data.txn.target.ptr = 0;
    data.txn.cookie = 0;
    data.txn.code = 0;
    if (status) {
        data.txn.flags = TF_STATUS_CODE;
        data.txn.data_size = sizeof(int);
        data.txn.offsets_size = 0;
        data.txn.data.ptr.buffer = (uintptr_t)&status;
        data.txn.data.ptr.offsets = 0;
    } else {
        data.txn.flags = 0;
        data.txn.data_size = reply->data - reply->data0;
        data.txn.offsets_size = ((char*) reply->offs) - ((char*) reply->offs0);
        data.txn.data.ptr.buffer = (uintptr_t)reply->data0;
        data.txn.data.ptr.offsets = (uintptr_t)reply->offs0;
    }
    binder_write(bs, &data, sizeof(data));
}

//到客户端的BR_REPLY
 case BR_REPLY:
            {
                binder_transaction_data tr;
                err = mIn.read(&tr, sizeof(tr));
                ALOG_ASSERT(err == NO_ERROR, "Not enough command data for brREPLY");
                if (err != NO_ERROR) goto finish;

                if (reply) {
                    if ((tr.flags & TF_STATUS_CODE) == 0) {
                        reply->ipcSetDataReference(
                            reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                            tr.data_size,
                            reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                            tr.offsets_size/sizeof(binder_size_t),
                            freeBuffer, this);
                    } else {
                        err = *reinterpret_cast<const status_t*>(tr.data.ptr.buffer);
                        freeBuffer(nullptr,
                            reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                            tr.data_size,
                            reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                            tr.offsets_size/sizeof(binder_size_t), this);
                    }
                } else {
                    freeBuffer(nullptr,
                        reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                        tr.data_size,
                        reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                        tr.offsets_size/sizeof(binder_size_t), this);
                    continue;
                }
            }
            goto finish;
```

```
//没错对应的func就是这里，还记得我们传入的code是什么吗？
int svcmgr_handler(struct binder_state *bs,
                   struct binder_txn *txn,
                   struct binder_io *msg,
                   struct binder_io *reply)
{
    struct svcinfo *si;
    uint16_t *s;
    unsigned len;
    void *ptr;
    uint32_t strict_policy;
    int allow_isolated;

    if (txn->target != svcmgr_handle)
        return -1;
    strict_policy = bio_get_uint32(msg);
    s = bio_get_string16(msg, &len);
    if ((len != (sizeof(svcmgr_id) / 2)) ||
        memcmp(svcmgr_id, s, sizeof(svcmgr_id))) {
        fprintf(stderr,"invalid id %s\n", str8(s));
        return -1;
    }

    switch(txn->code) {
    case SVC_MGR_GET_SERVICE:
    case SVC_MGR_CHECK_SERVICE:
        s = bio_get_string16(msg, &len);
        ptr = do_find_service(bs, s, len, txn->sender_euid);
        if (!ptr)
            break;
        bio_put_ref(reply, ptr);
        return 0;

    case SVC_MGR_ADD_SERVICE://在这里，执行do_add_service函数
        s = bio_get_string16(msg, &len);
        ptr = bio_get_ref(msg);
        allow_isolated = bio_get_uint32(msg) ? 1 : 0;
        if (do_add_service(bs, s, len, ptr, txn->sender_euid, allow_isolated))
            return -1;
        break;

    case SVC_MGR_LIST_SERVICES: {
        unsigned n = bio_get_uint32(msg);

        si = svclist;
        while ((n-- > 0) && si)
            si = si->next;
        if (si) {
            bio_put_string16(reply, si->name);
            return 0;
        }
        return -1;
    }
    default:
        ALOGE("unknown code %d\n", txn->code);
        return -1;
    }
    bio_put_uint32(reply, 0);
    return 0;
}
```

执行do\_add\_service对服务进行添加。

```

int do_add_service(struct binder_state *bs, const uint16_t *s, size_t len, uint32_t handle,
                   uid_t uid, int allow_isolated, uint32_t dumpsys_priority, pid_t spid, const char* sid) {
    struct svcinfo *si;
    if (!handle || (len == 0) || (len > 127))
        return -1;
    if (!svc_can_register(s, len, spid, sid, uid)) {
        return -1;
    }
    si = find_svc(s, len);
    if (si) {//如果存在
        if (si->handle) {//非ServiceManager就先death掉handle
            svcinfo_death(bs, si);
        }//重新设置handle值，因为handle的值在添加后+1 递增。
        si->handle = handle;
    } else {//不存在的话就在堆上开辟service空间 然后存入svclist
        si = malloc(sizeof(*si) + (len + 1) * sizeof(uint16_t));
        if (!si) {
            return -1;
        }
        si->handle = handle;
        si->len = len;
        memcpy(si->name, s, (len + 1) * sizeof(uint16_t));
        si->name[len] = '\0';
        si->death.func = (void*) svcinfo_death;
        si->death.ptr = si;
        si->allow_isolated = allow_isolated;
        si->dumpsys_priority = dumpsys_priority;
        si->next = svclist;
        svclist = si;//把service添加到svclist
    }
    binder_acquire(bs, handle);
    binder_link_to_death(bs, handle, &si->death);
    return 0;
}
```

BR\_REPLY处理完成以后就会继续执行waitResponse,服务端继续执行waitResponse执行BR\_TRANSACTION命令,回调到java层的ServiceManager.java中的onTransact方法。

```
status_t IPCThreadState::executeCommand(int32_t cmd)
{
    BBinder* obj;
    RefBase::weakref_type* refs;
    status_t result = NO_ERROR;

    switch ((uint32_t)cmd) {

    case BR_OK:
        break;

    case BR_TRANSACTION_SEC_CTX:
    case BR_TRANSACTION://这里处理服务端的BR_TRANSACTION调到java层继续处理。
        {
            binder_transaction_data_secctx tr_secctx;
            binder_transaction_data& tr = tr_secctx.transaction_data;

            if (cmd == (int) BR_TRANSACTION_SEC_CTX) {
                result = mIn.read(&tr_secctx, sizeof(tr_secctx));
            } else {
                result = mIn.read(&tr, sizeof(tr));
                tr_secctx.secctx = 0;
            }
            ALOG_ASSERT(result == NO_ERROR,
                "Not enough command data for brTRANSACTION");
            if (result != NO_ERROR) break;
            mIPCThreadStateBase->pushCurrentState(
                IPCThreadStateBase==CallState==BINDER);
            Parcel buffer;
            buffer.ipcSetDataReference(
                reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                tr.data_size,
                reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                tr.offsets_size/sizeof(binder_size_t), freeBuffer, this);
            const pid_t origPid = mCallingPid;
            const char* origSid = mCallingSid;
            const uid_t origUid = mCallingUid;
            const int32_t origStrictModePolicy = mStrictModePolicy;
            const int32_t origTransactionBinderFlags = mLastTransactionBinderFlags;
            const int32_t origWorkSource = mWorkSource;
            const bool origPropagateWorkSet = mPropagateWorkSource;
            clearCallingWorkSource();
            clearPropagateWorkSource();

            mCallingPid = tr.sender_pid;
            mCallingSid = reinterpret_cast<const char*>(tr_secctx.secctx);
            mCallingUid = tr.sender_euid;
            mLastTransactionBinderFlags = tr.flags;

            Parcel reply;
            status_t error;
            IF_LOG_TRANSACTIONS() {
                TextOutput::Bundle _b(alog);
            }
            if (tr.target.ptr) {
                if (reinterpret_cast<RefBase::weakref_type*>(
                        tr.target.ptr)->attemptIncStrong(this)) {//调用BBinder的transact方法
                    error = reinterpret_cast<BBinder*>(tr.cookie)->transact(tr.code, buffer,
                            &reply, tr.flags);
                    reinterpret_cast<BBinder*>(tr.cookie)->decStrong(this);
                } else {
                    error = UNKNOWN_TRANSACTION;
                }

            } else {
                error = the_context_object->transact(tr.code, buffer, &reply, tr.flags);
            }

            mIPCThreadStateBase->popCurrentState();
            if ((tr.flags & TF_ONE_WAY) == 0) {
                LOG_ONEWAY("Sending reply to %d!", mCallingPid);
                if (error < NO_ERROR) reply.setError(error);
                sendReply(reply, 0);
            } else {
                LOG_ONEWAY("NOT sending reply to %d!", mCallingPid);
            }
            mCallingPid = origPid;
            mCallingSid = origSid;
            mCallingUid = origUid;
            mStrictModePolicy = origStrictModePolicy;
            mLastTransactionBinderFlags = origTransactionBinderFlags;
            mWorkSource = origWorkSource;
            mPropagateWorkSource = origPropagateWorkSet;
            IF_LOG_TRANSACTIONS() {
                TextOutput::Bundle _b(alog);
            }

        }
        break;
    case BR_FINISHED:
        result = TIMED_OUT;
        break;

    case BR_NOOP:
        break;
    default:
        ALOGE("*** BAD COMMAND %d received from Binder driver\n", cmd);
        result = UNKNOWN_ERROR;
        break;
    }

    if (result != NO_ERROR) {
        mLastError = result;
    }

    return result;
}


// BBinder的transact函数。
status_t BBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    data.setDataPosition(0);
    status_t err = NO_ERROR;
    switch (code) {
        case PING_TRANSACTION:
            reply->writeInt32(pingBinder());
            break;
        default://调用onTransact函数。此时的code就是之前的ADD_SERVICE_TRANSACTION
            err = onTransact(code, data, reply, flags);
            break;
    }

    if (reply != nullptr) {
        reply->setDataPosition(0);
    }

    return err;
}

//onTransact 注意此时native层的是JavaBBinder对象，他覆盖了BBinder的OnTransact

    status_t onTransact(
        uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags = 0) override
    {
        JNIEnv* env = javavm_to_jnienv(mVM);
        IPCThreadState* thread_state = IPCThreadState::self();
        const int32_t strict_policy_before = thread_state->getStrictModePolicy();
      //通过jni调用到java层的Binder，执行execTransact函数
        jboolean res = env->CallBooleanMethod(mObject, gBinderOffsets.mExecTransact,
            code, reinterpret_cast<jlong>(&data), reinterpret_cast<jlong>(reply), flags);
        if (env->ExceptionCheck()) {
            ScopedLocalRef<jthrowable> excep(env, env->ExceptionOccurred());
            report_exception(env, excep.get(),
                "*** Uncaught remote exception!  "
                "(Exceptions are not yet supported across processes.)");
            res = JNI_FALSE;
        }
        if (thread_state->getStrictModePolicy() != strict_policy_before) {
            set_dalvik_blockguard_policy(env, strict_policy_before);
        }
        if (env->ExceptionCheck()) {
            ScopedLocalRef<jthrowable> excep(env, env->ExceptionOccurred());
            report_exception(env, excep.get(),
                "*** Uncaught exception in onBinderStrictModePolicyChange");
        }
        if (code == SYSPROPS_TRANSACTION) {
            BBinder::onTransact(code, data, reply, flags);
        }
        return res != JNI_FALSE ? NO_ERROR : UNKNOWN_TRANSACTION;
    }

//execTransact 函数：调用的就是java层的Binder
  gBinderOffsets.mExecTransact = GetMethodIDOrDie(env, clazz, "execTransact", "(IJJI)Z");

 private boolean execTransact(int code, long dataObj, long replyObj,
            int flags) {
        final int callingUid = Binder.getCallingUid();
        final long origWorkSource = ThreadLocalWorkSource.setUid(callingUid);
        try {//执行execTransactInternal方法
            return execTransactInternal(code, dataObj, replyObj, flags, callingUid);
        } finally {
            ThreadLocalWorkSource.restore(origWorkSource);
        }
    }
    private boolean execTransactInternal(int code, long dataObj, long replyObj, int flags,
            int callingUid) {
        final BinderInternal.Observer observer = sObserver;
        final CallSession callSession =
                observer != null ? observer.callStarted(this, code, UNSET_WORKSOURCE) : null;
        Parcel data = Parcel.obtain(dataObj);
        Parcel reply = Parcel.obtain(replyObj);
        boolean res;
        final boolean tracingEnabled = Binder.isTracingEnabled();
        try {
            if (tracingEnabled) {
                final String transactionName = getTransactionName(code);
                Trace.traceBegin(Trace.TRACE_TAG_ALWAYS, getClass().getName() + ":"
                        + (transactionName != null ? transactionName : code));
            }//调用OnTransact
            res = onTransact(code, data, reply, flags);
        } catch (RemoteException|RuntimeException e) {
            if (observer != null) {
                observer.callThrewException(callSession, e);
            }
            if ((flags & FLAG_ONEWAY) != 0) {
                if (e instanceof RemoteException) {
                    Log.w(TAG, "Binder call failed.", e);
                } else {
                    Log.w(TAG, "Caught a RuntimeException from the binder stub implementation.", e);
                }
            } else {
                reply.setDataSize(0);
                reply.setDataPosition(0);
                reply.writeException(e);
            }
            res = true;
        } finally {
            if (tracingEnabled) {
                Trace.traceEnd(Trace.TRACE_TAG_ALWAYS);
            }
            if (observer != null) {
                final int workSourceUid = sWorkSourceProvider.resolveWorkSourceUid(
                        data.readCallingWorkSourceUid());
                observer.callEnded(callSession, data.dataSize(), reply.dataSize(), workSourceUid);
            }
        }
        checkParcel(this, code, reply, "Unreasonably large binder reply buffer");
        reply.recycle();
        data.recycle();
        StrictMode.clearGatheredViolations();
        return res;
    }

//调用到ServiceManangerNative的onTransact 处理java层的addService
    public boolean onTransact(int code, Parcel data, Parcel reply, int flags)
    {
        try {
            switch (code) {
                case IServiceManager.GET_SERVICE_TRANSACTION: {
                    data.enforceInterface(IServiceManager.descriptor);
                    String name = data.readString();
                    IBinder service = getService(name);
                    reply.writeStrongBinder(service);
                    return true;
                }

                case IServiceManager.CHECK_SERVICE_TRANSACTION: {
                    data.enforceInterface(IServiceManager.descriptor);
                    String name = data.readString();
                    IBinder service = checkService(name);
                    reply.writeStrongBinder(service);
                    return true;
                }

                case IServiceManager.ADD_SERVICE_TRANSACTION: {//添加服务
                    data.enforceInterface(IServiceManager.descriptor);
                    String name = data.readString();
                    IBinder service = data.readStrongBinder();
                    boolean allowIsolated = data.readInt() != 0;
                    int dumpPriority = data.readInt();
                    addService(name, service, allowIsolated, dumpPriority);
                    return true;
                }

                case IServiceManager.LIST_SERVICES_TRANSACTION: {
                    data.enforceInterface(IServiceManager.descriptor);
                    int dumpPriority = data.readInt();
                    String[] list = listServices(dumpPriority);
                    reply.writeStringArray(list);
                    return true;
                }

                case IServiceManager.SET_PERMISSION_CONTROLLER_TRANSACTION: {
                    data.enforceInterface(IServiceManager.descriptor);
                    IPermissionController controller =
                            IPermissionController.Stub.asInterface(
                                    data.readStrongBinder());
                    setPermissionController(controller);
                    return true;
                }
            }
        } catch (RemoteException e) {
        }

        return false;
    }

    public IBinder asBinder()
    {
        return this;
    }
}
```

线程的创建:spawnPooledThread。BR\_SPAWN\_LOOPER命令。

#### 总结:

##### 一张图概括交互流程:

![binder简易图.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/4be9e8d0091c420994dccc265e6c9c40~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

##### 一张图概括关系模型:

![binder关系图.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f415ac3bd19a4e10bcc766188d4f7046~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

### aidl文件浅析

我们可以通过AndroidStudio，直接帮我们创建一份aidl文件:

```
package com.example.myapplication;
interface ITestAidlInterface {
    void add(int a, int b);
    void mul(int a,int b);
    void minus(int a,int b);
    void divide(int a,int b);
}
```

我们定义了四个方法，加减乘除。来看看帮我们生成的文件是什么样的:

```
package com.example.myapplication;
public interface ITestAidlInterface extends android.os.IInterface
{
  public static class Default implements com.example.myapplication.ITestAidlInterface
  {
    @Override public void add(int a, int b) throws android.os.RemoteException
    {
    }
    @Override public void mul(int a, int b) throws android.os.RemoteException
    {
    }
    @Override public void minus(int a, int b) throws android.os.RemoteException
    {
    }
    @Override public void divide(int a, int b) throws android.os.RemoteException
    {
    }
    @Override
    public android.os.IBinder asBinder() {
      return null;
    }
  }
  public static abstract class Stub extends android.os.Binder implements com.example.myapplication.ITestAidlInterface
  {
    private static final java.lang.String DESCRIPTOR = "com.example.myapplication.ITestAidlInterface";
    public Stub()
    {
      this.attachInterface(this, DESCRIPTOR);
    }
    public static com.example.myapplication.ITestAidlInterface asInterface(android.os.IBinder obj)
    {
      if ((obj==null)) {
        return null;
      }
      android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
      if (((iin!=null)&&(iin instanceof com.example.myapplication.ITestAidlInterface))) {
        return ((com.example.myapplication.ITestAidlInterface)iin);
      }
      return new com.example.myapplication.ITestAidlInterface.Stub.Proxy(obj);
    }
    @Override public android.os.IBinder asBinder()
    {
      return this;
    }
    //关键函数 用来处理请求的 所以这里就是native层的BBInder,JNI层的JavaBBinderHolder
    @Override public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException
    {
      java.lang.String descriptor = DESCRIPTOR;
      switch (code)
      {
        case INTERFACE_TRANSACTION:
        {
          reply.writeString(descriptor);
          return true;
        }
        case TRANSACTION_add://这里用来处理客户端的add请求
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          int _arg1;
          _arg1 = data.readInt();
          this.add(_arg0, _arg1);
          reply.writeNoException();  //处理完成以后把数据写到parcel
          return true;
        }
        case TRANSACTION_mul://这里用来处理客户端的mul请求
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          int _arg1;
          _arg1 = data.readInt();
          this.mul(_arg0, _arg1);
          reply.writeNoException();
          return true;
        }
        case TRANSACTION_minus://这里用来处理客户端的minus请求
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          int _arg1;
          _arg1 = data.readInt();
          this.minus(_arg0, _arg1);
          //处理完成以后把数据写到parcel
          reply.writeNoException();
          return true;
        }
        case TRANSACTION_divide://这里用来处理客户端的divide请求
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          int _arg1;
          _arg1 = data.readInt();
          this.divide(_arg0, _arg1);
          //处理完成以后把数据写到parcel
          reply.writeNoException();
          return true;
        }
        default:
        {
          return super.onTransact(code, data, reply, flags);
        }
      }
    }
    //这里就是Native层的BpBinder，也就是ServiceManagerNative中的Proxy 在这里主要是数据的发送
    private static class Proxy implements com.example.myapplication.ITestAidlInterface{
      private android.os.IBinder mRemote;
      Proxy(android.os.IBinder remote)
      {
        mRemote = remote;
      }
      @Override public android.os.IBinder asBinder()
      {
        return mRemote;
      }
      public java.lang.String getInterfaceDescriptor()
      {
        return DESCRIPTOR;
      }
      @Override public int add(int a, int b) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        int _result;
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(a);
          _data.writeInt(b);
          //像我们的服务端 也就是mRemote发送请求，这里会阻塞等待客户端返回。
          boolean _status = mRemote.transact(Stub.TRANSACTION_add, _data, _reply, 0);
          if (!_status && getDefaultImpl() != null) {
            return getDefaultImpl().add(a, b);
          }
          _reply.readException();
          //客户端通过Parcel读取到服务端返回的数据再返回给上层。
          _result = _reply.readInt();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
        return _result;
      }
      @Override public int mul(int a, int b) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        int _result;
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(a);
          _data.writeInt(b);
          boolean _status = mRemote.transact(Stub.TRANSACTION_mul, _data, _reply, 0);
          if (!_status && getDefaultImpl() != null) {
            return getDefaultImpl().mul(a, b);
          }
          _reply.readException();
          _result = _reply.readInt();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
        return _result;
      }
      @Override public int minus(int a, int b) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        int _result;
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(a);
          _data.writeInt(b);
          boolean _status = mRemote.transact(Stub.TRANSACTION_minus, _data, _reply, 0);
          if (!_status && getDefaultImpl() != null) {
            return getDefaultImpl().minus(a, b);
          }
          _reply.readException();
          _result = _reply.readInt();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
        return _result;
      }
      @Override public int divide(int a, int b) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        int _result;
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(a);
          _data.writeInt(b);
          boolean _status = mRemote.transact(Stub.TRANSACTION_divide, _data, _reply, 0);
          if (!_status && getDefaultImpl() != null) {
            return getDefaultImpl().divide(a, b);
          }
          _reply.readException();
          _result = _reply.readInt();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
        return _result;
      }
      public static com.example.myapplication.ITestAidlInterface sDefaultImpl;
    }
    //这里就是我们发送的请求code 也就是proxy会发送的code。是从1开始递增的。
    static final int TRANSACTION_add = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
    static final int TRANSACTION_mul = (android.os.IBinder.FIRST_CALL_TRANSACTION + 1);
    static final int TRANSACTION_minus = (android.os.IBinder.FIRST_CALL_TRANSACTION + 2);
    static final int TRANSACTION_divide = (android.os.IBinder.FIRST_CALL_TRANSACTION + 3);
    public static boolean setDefaultImpl(com.example.myapplication.ITestAidlInterface impl) {
      if (Stub.Proxy.sDefaultImpl != null) {
        throw new IllegalStateException("setDefaultImpl() called twice");
      }
      if (impl != null) {
        Stub.Proxy.sDefaultImpl = impl;
        return true;
      }
      return false;
    }
    public static com.example.myapplication.ITestAidlInterface getDefaultImpl() {
      return Stub.Proxy.sDefaultImpl;
    }
  }
  public int add(int a, int b) throws android.os.RemoteException;
  public int mul(int a, int b) throws android.os.RemoteException;
  public int minus(int a, int b) throws android.os.RemoteException;
  public int divide(int a, int b) throws android.os.RemoteException;
}

```

### 扩展:多进程能给我们带来什么好处？

#### 好处：

1.安全隔离

比如播放器，网页可以放在独立的进程中，即使播放器crash了，不会直接导致应用主进程的崩溃，可以更好的保护主进程。

2.节省主进程的空间

Android系统对每个应用分配的内存空间都是有限制的，每个应用都会分开16M的堆空间，当你占用的空间越多，被oomadj的几率就越大，所以我们可以通过子进程做一些服务，来给主进程释放更多的空间，增加我们应用的存活率。

#### 坏处：

1.占用更多的手机空间

2.程序架构变得复杂，需要处理多个进程之间的关系

3.静态成员失效、多进程并发问题