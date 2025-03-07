## 前提

## 1.回顾

之前讲了Android中，第一个启动的是`init` 解析init.rc文件，启动对应的service。`Zygote`就是由`init`启动起来的。Zygote作为一个孵化器，孵化了`system_server`以及`其他`的启用程序。SystemServer是系统用来启动Service的入口，比如我们常用的`AMS`，`WMS`，`PMS`等等都是由它创建的，由`SystemServer`来控制管理。SystemServer进程做为一个系统进程他创建了ActivityThread加载了对应的apk`framewok-res.apk`(比如系统的一些弹窗都是由他弹出)，接着调用了`startBootstrapServices`、`startCoreServices`、`startOtherServices`开启了非常多的服务`Installer`、`ActivityTaskManagerService、ActivityManagerService`、`PowerManagerService`、`PackageManagerService`等等。把他们添加到了`SystemServiceManager`的`mServices(ArrayList)`中去。顺便讲了一下Android中都WatchDog，它是用来`监控SystemServer`中的`Services`的，一旦出现问题就会杀死`system_server`，进而杀死`Zygote`，由`init重启Zygote再重启system_server`。

具体的文章细节大家可以看我往期发布的:

[【Android FrameWork】第一个启动的程序--init](https://juejin.cn/post/7213733567606685753 "https://juejin.cn/post/7213733567606685753")

[【Android FrameWork】Zygote](https://juejin.cn/post/7214068367607119933 "https://juejin.cn/post/7214068367607119933")

[【Android FrameWork】SystemServer](https://juejin.cn/post/7214493929566945340 "https://juejin.cn/post/7214493929566945340")

## 2.介绍

### 1.先介绍下ServiceManager

ServiceManager是Android系统为开发者提供的一个服务大管家，当开机之后，由内核态进入用户态之后，会启动system\_server进程，在该进程里面会对AMS，PKMS，PMS等等进行创建。然后添加到ServiceManager中。

### 2.ServiceManager的作用

这里大家可能会有疑问，`Service`不是在`SystemServer`中存储了吗？为什么还要在`ServiceManager`中去存储呢？我们不是可以直接从`SystemServer`中取吗？ 我的理解是这样的`SystemServer算是一个大管家，他整合了系统的各种服务，监控着我们服务，管理服务的周期。`而`ServiceManager只有一个功能就是提供binder通信，让应用可以获取到系统提供的服务。`所以他们并不冲突，责任很明确。

### 3.再介绍下Binder

`Binder`是Android特有的一种通信方式。Android Binder的前身是`OpenBinder`，后来在OpenBinder的基础上开发了Android Binder。 Android基于Linux所以支持Linux原生的IPC通信机制：共享内存、Pipe、Socket。Binder是Android特有的。 我就画个表来说明下他们的区别吧。

|  | Binder | 共享内存 | Pipe | Socket |
| --- | --- | --- | --- | --- |
| 性能 | 一次内存拷贝 | 0次内存拷贝 | 两次内存拷贝 | 两次内存拷贝 |
| 稳定性 | C/S架构稳定性高 | 同步问题、死锁问题 | 仅支持父子进程通信，单全功效率低 | C/S架构，传输需要握手、挥手、效率低，开销大 |
| 安全 | 内核层对App分配UID，安全性高 | 自定义协议，需要自己实现安全，并且接口对外开放 | 自定义协议，需要自己实现安全，并且接口对外开放 | 自定义协议，需要自己实现安全，并且接口对外开放 |

有兴趣的可以看看我之前的文章： [Binder究竟是什么？](https://juejin.cn/post/7092230588664397860#heading-16 "https://juejin.cn/post/7092230588664397860#heading-16")

### 4.吐槽

其实这一块很难去讲，ServiceManager一定会掺杂这Binder，很难讲，但是分开讲大家可能会看的云里雾里的。所以还是花点时间和Binder一起讲。之前也写过类似的文章，那时是以Binder为主角写的，现在开始了Android FrameWork系列，那我就以ServiceManager为主角，看看ServiceManager是怎么添加服务，提供服务的吧。

## 3.正文

### 1.ServiceManager的启动

ServiceManager它是由init进程拉起来的，而且启动时机要比Zygote早，我们看下init.rc文件是怎么描述的。 文件目录:`/frameworks/native/cmds/servicemanager/servicemanager.rc`

```
service servicemanager /system/bin/servicemanager //可执行文件
    class core animation //className =core animation
    user system
```

之前没有讲`init.rc`服务分三类:`core`、`main`、和 `late_start`。顺序分别是core>main>late\_start。感兴趣大家可以再看看`init.cpp`。

ServiceManager的源代码在:`/frameworks/native/cmds/servicemanager/service_manager.c` 我们看看它的入口函数`main`

```
int main(int argc, char** argv)
{
    struct binder_state *bs;
    union selinux_callback cb;
    char *driver;

    if (argc > 1) {
        driver = argv[1];
    } else {
        driver = "/dev/binder";
    }

    bs = binder_open(driver, 128*1024);//调用binder_open 打开binder驱动 传递大小为128*1024也就是128k
  //………………

    if (binder_become_context_manager(bs)) {//设置自己成为binder设备的上下文管理者
    }    
    //………………
    //调用binder_loop 让binder进入loop状态，等待客户端连接，并且传入回调函数svcmgr_handler
    binder_loop(bs, svcmgr_handler);

    return 0;
}
```

这三个函数文件目录:`/frameworks/native/cmds/servicemanager/binder.c`

我们先看第一个函数`binder_open`:

```
struct binder_state *binder_open(const char* driver, size_t mapsize)
{
    struct binder_state *bs;
    struct binder_version vers;
    //在堆上开辟binder_state。
    bs = malloc(sizeof(*bs));
    if (!bs) {
        errno = ENOMEM;
        return NULL;
    }
    //driver ='/dev/binder' 调用open函数打开binder驱动 其实这里就调用到驱动层了，我们稍后在驱动层再详细讲
    bs->fd = open(driver, O_RDWR | O_CLOEXEC);
    if (bs->fd < 0) {
    }
    //进行Binder版本校验，其实这里也会调用到驱动层的代码，稍后在驱动层再讲。
    if ((ioctl(bs->fd, BINDER_VERSION, &vers) == -1) ||
        (vers.protocol_version != BINDER_CURRENT_PROTOCOL_VERSION)) {
    }

    bs->mapsize = mapsize;
    //mapsize = 128 通过mmap映射128k的空间 可读 私有
    bs->mapped = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, bs->fd, 0);
    if (bs->mapped == MAP_FAILED) {
    }

    return bs;

}
```

这里打开了`Binder(/dev/binder)`，并且对Binder进行了操作校验了`BINDER_VERSION`。通过`mmap`映射了`128k`的空间。

第二个函数:`binder_become_context_manager`:

```

int binder_become_context_manager(struct binder_state *bs)
{
    //创建flat_binder_object结构体 这个结构体很重要 Binder通信数据都存在这个里面
    struct flat_binder_object obj;
    memset(&obj, 0, sizeof(obj));
    obj.flags = FLAT_BINDER_FLAG_TXN_SECURITY_CTX;
    //调用Binder驱动层 写入BINDER_SET_CONTEXT_MGR_EXT，并且把obj传递进去
    int result = ioctl(bs->fd, BINDER_SET_CONTEXT_MGR_EXT, &obj);
    if (result != 0) {//如果失败调用老方法
        android_errorWriteLog(0x534e4554, "121035042");

        result = ioctl(bs->fd, BINDER_SET_CONTEXT_MGR, 0);
    }
    return result;
}
```

这里创建了flat\_binder\_object obj 结构体，这个结构体非常重要，我们的数据都在这里面存储。后续我们再传递数据比较多的时候再详细分析。

第三个函数`binder_loop`：

```
void binder_loop(struct binder_state *bs, binder_handler func)
{
    int res;
    struct binder_write_read bwr;//创建了binder_write_read结构体，这个结构体也非常重要 稍后详解
    uint32_t readbuf[32];//创建一个readBuf 32字节的数组
    bwr.write_size = 0; //指定bwr的write_size = 0
    bwr.write_consumed = 0; //指定bwr的 write_consume=0
    bwr.write_buffer = 0;//指定bwr的_write_buffer=0 这几个设置也很重要 我们到驱动层再看
    readbuf[0] = BC_ENTER_LOOPER;//给readBuf[0]设置成BC_ENTER_LOOPER。
    binder_write(bs, readbuf, sizeof(uint32_t));//调用binder_write将BC_ENTER_LOOPER数据交给Binder(驱动层)处理
    for (;;) {//死循环
        bwr.read_size = sizeof(readbuf);//读取readBuf的size
        bwr.read_consumed = 0;
        bwr.read_buffer = (uintptr_t) readbuf;//将readBuf的数据传递给当前bwr的read_buffer
        //调用ioctl 将BINDER_WRITE_READ命令 和数据bwr传给Binder驱动。
        res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);
        //获取到驱动的返回结果并解析
        res = binder_parse(bs, 0, (uintptr_t) readbuf, bwr.read_consumed, func);
    }
}

//给binder写入数据  data = BC_ENTER_LOOPER
int binder_write(struct binder_state *bs, void *data, size_t len)
{
    struct binder_write_read bwr;
    int res;

    bwr.write_size = len;
    bwr.write_consumed = 0;
    bwr.write_buffer = (uintptr_t) data;//要写入的数据是BC_ENTER_LOOPER
    bwr.read_size = 0;
    bwr.read_consumed = 0;
    bwr.read_buffer = 0;
    //调用binder驱动 传入BINDER_WRITE_READ 数据是上边的bwr 
    res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);
    if (res < 0) {
        fprintf(stderr,"binder_write: ioctl failed (%s)\n",
                strerror(errno));
    }
    return res;
}

//解析binder返回的数据并执行对应的命令 
int binder_parse(struct binder_state *bs, struct binder_io *bio,
                 uintptr_t ptr, size_t size, binder_handler func)
{
    int r = 1;
    uintptr_t end = ptr + (uintptr_t) size;
    while (ptr < end) {
        uint32_t cmd = *(uint32_t *) ptr;
        ptr += sizeof(uint32_t);
        switch(cmd) {
        case BR_TRANSACTION_SEC_CTX:
        case BR_TRANSACTION: {//服务端返回的话会返回这样的cmd 到这里来处理数据
            struct binder_transaction_data_secctx txn;
            if (cmd == BR_TRANSACTION_SEC_CTX) {
                memcpy(&txn, (void*) ptr, sizeof(struct binder_transaction_data_secctx));
                ptr += sizeof(struct binder_transaction_data_secctx);
            } else /* BR_TRANSACTION */ {//在这里来处理
                //把数据ptr(readBuf)拷贝到&txn.transaction_data 
                memcpy(&txn.transaction_data, (void*) ptr, sizeof(struct binder_transaction_data));
                ptr += sizeof(struct binder_transaction_data);

                txn.secctx = 0;
            }

            binder_dump_txn(&txn.transaction_data);
            if (func) {//进行回调 func = svcmgr_handler
                unsigned rdata[256/4];
                struct binder_io msg;
                struct binder_io reply;
                int res;

                bio_init(&reply, rdata, sizeof(rdata), 4);
                bio_init_from_txn(&msg, &txn.transaction_data);
                res = func(bs, &txn, &msg, &reply);
                if (txn.transaction_data.flags & TF_ONE_WAY) {
                    binder_free_buffer(bs, txn.transaction_data.data.ptr.buffer);
                } else {
                    binder_send_reply(bs, &reply, txn.transaction_data.data.ptr.buffer, res);
                }
            }
            break;
        }
    }

    return r;
}
```

service\_manager.c 总结下：

main函数主要分了`3`步:

1.调用binder\_open 打开Binder驱动，设置大小为128k。

2.调用binder\_become\_context\_manager，设置自己成为Binder设备的上下文管理者

3.调用binder\_loop让binder进入loop状态，并且等待客户端请求并处理，处理完成之后会把数据写入readbuf,进行svcmgr\_handler回调。

`注意：Binder的ioctl函数会阻塞。`

这样我们的ServiceManager就启动了，等待客户端的请求连接。

### 2.Binder的启动

Binder驱动的源码在内核层binder.c中，函数不多，主要有4个函数，分别是binder\_ini、binder\_open、binder\_mmap、binder\_ioctl，我们拆开看下各个函数。

#### 0.binder的前提知识

在讲Binder之前我们需要先补充下常用的进程通信机制。首先大家需要明白`进程隔离`，我就不长篇大论的说了，简单的说就是各个进程之间不共享，无法互相访问；`进程空间`:`用户空间`:0~3g。`内核空间`:3g~4g。接着画个图来说明下传统IPC。图画的比较抽象 😂 可以参考视频理解。后续讲完之后我会再画一张Binder的图。

![传统IPC.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/69f3d1effd214c45a04ced1479a1a75a~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

#### 1.binder\_init

```
static int __init binder_init(void)
{
   int ret;
   char *device_name, *device_names, *device_tmp;
   struct binder_device *device;
   struct hlist_node *tmp;

   ret = binder_alloc_shrinker_init();
   if (ret)
      return ret;

   atomic_set(&binder_transaction_log.cur, ~0U);
   atomic_set(&binder_transaction_log_failed.cur, ~0U);
   //创建在一个线程中运行的workqueue，命名为binder
   binder_deferred_workqueue = create_singlethread_workqueue("binder");
   if (!binder_deferred_workqueue)
      return -ENOMEM;

   binder_debugfs_dir_entry_root = debugfs_create_dir("binder", NULL);


    //device_name =  binder也有hwbinder和vndbinder 这个就是框架/应用之间   框架和供应商  供应商和供应商之间的binder
   device_names = kzalloc(strlen(binder_devices_param) + 1, GFP_KERNEL);
   if (!device_names) {
      ret = -ENOMEM;
      goto err_alloc_device_names_failed;
   }
   strcpy(device_names, binder_devices_param);

   device_tmp = device_names;
   while ((device_name = strsep(&device_tmp, ","))) {
       //调用init_binder_device
      ret = init_binder_device(device_name);
      if (ret)
         goto err_init_binder_device_failed;
   }

   return ret;
}



static int __init init_binder_device(const char *name)
{
   int ret;
   struct binder_device *binder_device;//创建binder_device结构体
   
   binder_device = kzalloc(sizeof(*binder_device), GFP_KERNEL);
   if (!binder_device)
      return -ENOMEM;
   //制定fops = binder_fops
   binder_device->miscdev.fops = &binder_fops;
   binder_device->miscdev.minor = MISC_DYNAMIC_MINOR;
   binder_device->miscdev.name = name;//name就是上边传进来的binder
   //这里设置uid位invalid 后续在ServiceManager设置上下文的时候才配置，名字位/dev/binder
   binder_device->context.binder_context_mgr_uid = INVALID_UID;
   binder_device->context.name = name;
   mutex_init(&binder_device->context.context_mgr_node_lock);
    //注册misc设备
   ret = misc_register(&binder_device->miscdev);
   if (ret < 0) {
      kfree(binder_device);
      return ret;
   }
    //把binder设备添加到hlist中
   hlist_add_head(&binder_device->hlist, &binder_devices);

   return ret;
}

//binder的一些操作
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

注册了binder驱动，以及fops有 `poll,unlocked_ioctl compat_ioctl mmap open flush release`。

#### 2.binder\_open

在看binder\_open之前，我们先来看一个结构体`binder_proc`,它是binder中本进程的一个描述，`threads`当前进程的binder线程的信息，`nodes`是自己进程的binder 信息，`refs_by_desc`是其他进程对应的binder对象，是以handle做为k的，`refs_by_node`也是其他进程的binder对象，是以内存地址为key。他们都是`红黑树`的数据结构。以及`vm_area_struct` 用户空间内存的映射管理,`vma_vm_mm`虚拟内存信息，`task_struct` 进程信息,`*buffer`是内核空间对应的地址，`user_buffer_offset`是用户空间和内核空间的偏移量。 `todo`就是当前进程需要做的任务队列,`wait`就是当前进程在等待的队列。`max_threads`是当前进程的最大线程数。上代码:

```
struct binder_proc {
 struct hlist_node proc_node;
 struct rb_root threads; //binder的线程信息
 struct rb_root nodes; //自己binder的root信息 其实内部保存的就是flat_binder_object的数据
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

我们看看binder\_open函数：

```
文件目录:include/linux/sched.h
[[define]] get_task_struct(tsk) do { atomic_inc(&(tsk)->usage); } while(0)

static int binder_open(struct inode *nodp, struct file *filp)
{
    struct binder_proc *proc;//创建binder_proc结构体
    struct binder_device *binder_dev;
    ………………
    //在内核空间申请binder_proc的内存
    proc = kzalloc(sizeof(*proc), GFP_KERNEL);
    //初始化内核同步自旋锁
    spin_lock_init(&proc->inner_lock);
    spin_lock_init(&proc->outer_lock);
    //原子操作赋值
    atomic_set(&proc->tmp_ref, 0);
    //使执行当前系统调用进程的task_struct.usage加1
    get_task_struct(current->group_leader);
    //获取到当前进程的 binder_proc->tsk = task_struct 在Linux中线程和进程都用task_struct来描述，都是使用PCB进程控制快来管理，区别就是线程可以使用一些公共资源。
    proc->tsk = current->group_leader;
    //初始化文件锁
    mutex_init(&proc->files_lock);
    //初始化todo列表
    INIT_LIST_HEAD(&proc->todo);
    //设置优先级
    if (binder_supported_policy(current->policy)) {
        proc->default_priority.sched_policy = current->policy;
        proc->default_priority.prio = current->normal_prio;
    } else {
        proc->default_priority.sched_policy = SCHED_NORMAL;
        proc->default_priority.prio = NICE_TO_PRIO(0);
    }
    //找到binder_device结构体的首地址
    binder_dev = container_of(filp->private_data, struct binder_device,
                  miscdev);
    //使binder_proc的上下文指向binder_device的上下文
    proc->context = &binder_dev->context;
    //初始化binder缓冲区
    binder_alloc_init(&proc->alloc);
    binder_stats_created(BINDER_STAT_PROC);
    //设置当前进程id
    proc->pid = current->group_leader->pid;
    //初始化已分发的死亡通知列表
    INIT_LIST_HEAD(&proc->delivered_death);
    //初始化等待线程列表
    INIT_LIST_HEAD(&proc->waiting_threads);
    //保存binder_proc数据
    filp->private_data = proc;

    //因为binder支持多线程，所以需要加锁
    mutex_lock(&binder_procs_lock);
    //将proc->binder_proc添加到binder_procs链表中
    hlist_add_head(&proc->proc_node, &binder_procs);
    //释放锁
    mutex_unlock(&binder_procs_lock);

    //在binder/proc目录下创建文件，以执行当前系统调用的进程id为名
    if (binder_debugfs_dir_entry_proc) {
        char strbuf[11];
        snprintf(strbuf, sizeof(strbuf), "%u", proc->pid);
        proc->debugfs_entry = debugfs_create_file(strbuf, 0444,
            binder_debugfs_dir_entry_proc,
            (void *)(unsigned long)proc->pid,
            &binder_proc_fops);
    }

    return 0;
}

```

`binder_open`函数就是创建了这样的一个结构体，设置了`当前进程`并且初始化了`todo`和`wait`队列并将`proc_node`添加到binder的`binder_procs`中。大家这里可能会好奇为什么添加proc的`proc_node`，这是因为可以通过计算的方式获取到`binder_proc`。

#### 3.binder\_mmap

在看binder\_mmap之前，我们首先得知道应用的用户空间和内核空间是一片连续的虚拟内存地址。我们可以通过偏移量来得到对应的地址.

```
//ServiceManager传递过来的file就是binder，*vma就是128k的内存地址
static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
   int ret;
   //在binder_open的时候已经指定了private_data就是binder_proc 所以拿到了当前进程的binder_proc
   struct binder_proc *proc = filp->private_data;
   const char *failure_string;
   //进程校验
   if (proc->tsk != current->group_leader)
      return -EINVAL;
   //如果你申请的空间大于>4M 也就给你4M
   if ((vma->vm_end - vma->vm_start) > SZ_4M)
      vma->vm_end = vma->vm_start + SZ_4M;

   if (vma->vm_flags & FORBIDDEN_MMAP_FLAGS) {
      ret = -EPERM;
      failure_string = "bad vm_flags";
      goto err_bad_arg;
   }
   //表示vma不可以被fork复制
   vma->vm_flags |= VM_DONTCOPY | VM_MIXEDMAP;
   vma->vm_flags &= ~VM_MAYWRITE;

   vma->vm_ops = &binder_vm_ops;
   //vma的vm_private_data指向binder_proc
   vma->vm_private_data = proc;
    //调用binder_alloc_mmap_handler 建立用户空间和内核空间的地址映射关系
   ret = binder_alloc_mmap_handler(&proc->alloc, vma);
   if (ret)
      return ret;
   mutex_lock(&proc->files_lock);
   //获取进程的打开文件信息结构体file_struct，并把引用+1
   proc->files = get_files_struct(current);
   mutex_unlock(&proc->files_lock);
   return 0;
   return ret;
}


文件目录:/kernel_msm-android-msm-wahoo-4.4-android11/drivers/android/binder_alloc.c
int binder_alloc_mmap_handler(struct binder_alloc *alloc,
               struct vm_area_struct *vma)
{
   int ret;
   struct vm_struct *area;
   const char *failure_string;
   struct binder_buffer *buffer;

   mutex_lock(&binder_alloc_mmap_lock);
   //如果已经分配过的逻辑
   if (alloc->buffer) {
      ret = -EBUSY;
      failure_string = "already mapped";
      goto err_already_mapped;
   }
    //申请128k的内核空间
   area = get_vm_area(vma->vm_end - vma->vm_start, VM_ALLOC);
  //把申请到的空间给proc->buffer 也就是proc->buffer是内核空间的首地址 
   alloc->buffer = area->addr;
   //通过vma的首地址和内核空间proc->buffer(area->addr)相减得到用户空间和内核空间的偏移量，就可以在内核空间拿到内核空间的地址，也可以再内核空间拿到用户空间的地址
   alloc->user_buffer_offset =
      vma->vm_start - (uintptr_t)alloc->buffer;
   mutex_unlock(&binder_alloc_mmap_lock);

[[ifdef]] CONFIG_CPU_CACHE_VIPT
   if (cache_is_vipt_aliasing()) {
      while (CACHE_COLOUR(
            (vma->vm_start ^ (uint32_t)alloc->buffer))) {
         pr_info("binder_mmap: %d %lx-%lx maps %pK bad alignment\n",
            alloc->pid, vma->vm_start, vma->vm_end,
            alloc->buffer);
         vma->vm_start += PAGE_SIZE;
      }
   }
[[endif]]
    //申请内存为一页大小
   alloc->pages = kzalloc(sizeof(alloc->pages[0]) *
               ((vma->vm_end - vma->vm_start) / PAGE_SIZE),
                GFP_KERNEL);
   if (alloc->pages == NULL) {
      ret = -ENOMEM;
      failure_string = "alloc page array";
      goto err_alloc_pages_failed;
   }
   //得到buffer_size
   alloc->buffer_size = vma->vm_end - vma->vm_start;
    //在物理内存上申请buufer的内存空间
   buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
   if (!buffer) {
      ret = -ENOMEM;
      failure_string = "alloc buffer struct";
      goto err_alloc_buf_struct_failed;
   }
    //指向内核空间的地址
   buffer->data = alloc->buffer;
   //把buffer->entry添加到alloc->buffer的红黑树中
   list_add(&buffer->entry, &alloc->buffers);
   buffer->free = 1;
   binder_insert_free_buffer(alloc, buffer);
   alloc->free_async_space = alloc->buffer_size / 2;//如果算一步的话需要/2
   barrier();
   //设置alloc的vma是当前的用户空间
   alloc->vma = vma;
   alloc->vma_vm_mm = vma->vm_mm;
    //引用计数+1
   atomic_inc(&alloc->vma_vm_mm->mm_count);
   return 0;

}

```

这里比较抽象，我们画张图。画图之后一切简单清晰。

![binder_mmap.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a9da3d993c904476b6565a739eb5fb33~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

#### 3.binder\_ioctl

```
//在binder_open中 第一个调用ioctl 传递的参数是binder_version 我们看看怎么处理的 arg = &ver
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
   int ret;
   struct binder_proc *proc = filp->private_data;//拿到当前进程的proc
   struct binder_thread *thread;
   //得到cmd的大小
   unsigned int size = _IOC_SIZE(cmd);
   void __user *ubuf = (void __user *)arg;

   binder_selftest_alloc(&proc->alloc);

   trace_binder_ioctl(cmd, arg);
    //进入休眠状态，等待被唤醒 这里不会被休眠 binder_stop_on_user_error>2才会被休眠
   ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
   if (ret)
      goto err_unlocked;
    //唤醒后拿到当前的thread 第一次肯定是没有的 会创建thread
   thread = binder_get_thread(proc);
   if (thread == NULL) {
      ret = -ENOMEM;
      goto err;
   }

   switch (cmd) {//cmd = BINDER_VERSION
   case BINDER_VERSION: {
   //在这里 拿到了ver
      struct binder_version __user *ver = ubuf;
      //把版本信息放入到ver->protocol_version
      if (put_user(BINDER_CURRENT_PROTOCOL_VERSION,
              &ver->protocol_version)) {
         ret = -EINVAL;
         goto err;
      }
      break;
   }
   return ret;
}

static struct binder_thread *binder_get_thread(struct binder_proc *proc)
{
   struct binder_thread *thread;
   struct binder_thread *new_thread;

   binder_inner_proc_lock(proc);
   //从当前proc的thread.rb_nodes中来找，如果找不到返回NULL
   thread = binder_get_thread_ilocked(proc, NULL);
   binder_inner_proc_unlock(proc);
   if (!thread) {//当前为空 创建newThread
      new_thread = kzalloc(sizeof(*thread), GFP_KERNEL);
      if (new_thread == NULL)
         return NULL;
      binder_inner_proc_lock(proc);
      //再进去找 不过这个时候new_thread不为null
      thread = binder_get_thread_ilocked(proc, new_thread);
      binder_inner_proc_unlock(proc);
      if (thread != new_thread)
         kfree(new_thread);
   }
   return thread;
}

//从当前进程的rb_node中来找thread 如果找到返回 如果没找到看new_thread是否为NULL 是Null 所以返回Null
static struct binder_thread *binder_get_thread_ilocked(
      struct binder_proc *proc, struct binder_thread *new_thread)
{
   struct binder_thread *thread = NULL;
   struct rb_node *parent = NULL;
   struct rb_node **p = &proc->threads.rb_node;

   while (*p) {
      parent = *p;
      thread = rb_entry(parent, struct binder_thread, rb_node);

      if (current->pid < thread->pid)
         p = &(*p)->rb_left;
      else if (current->pid > thread->pid)
         p = &(*p)->rb_right;
      else
         return thread;
   }
   if (!new_thread)
      return NULL;
     //new_thread不为Null  会帮我们设置信息
   thread = new_thread;
   binder_stats_created(BINDER_STAT_THREAD);
   thread->proc = proc;//设置proc为当前proc
   thread->pid = current->pid;//pid
   get_task_struct(current);//引用计数+1
   thread->task = current;
   atomic_set(&thread->tmp_ref, 0);
   //初始化wait队列
   init_waitqueue_head(&thread->wait);
   //初始化todo队列
   INIT_LIST_HEAD(&thread->todo);
   //插入红黑树
   rb_link_node(&thread->rb_node, parent, p);
   //调整颜色
   rb_insert_color(&thread->rb_node, &proc->threads);
   thread->looper_need_return = true;
   thread->return_error.work.type = BINDER_WORK_RETURN_ERROR;
   thread->return_error.cmd = BR_OK;
   thread->reply_error.work.type = BINDER_WORK_RETURN_ERROR;
   thread->reply_error.cmd = BR_OK;
   INIT_LIST_HEAD(&new_thread->waiting_thread_node);
   return thread;
}


static struct binder_thread *binder_get_thread(struct binder_proc *proc)
{
   struct binder_thread *thread = NULL;
   struct rb_node *parent = NULL;
   struct rb_node **p = &proc->threads.rb_node;//第一次进来是NULL

   while (*p) {//第一次进来不会查找到
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
   //创建thread
      thread = kzalloc(sizeof(*thread), GFP_KERNEL);
      if (thread == NULL)
         return NULL;
      binder_stats_created(BINDER_STAT_THREAD);
      thread->proc = proc;//thread绑定proc
      thread->pid = current->pid;//指定pid
      init_waitqueue_head(&thread->wait);//初始化thread的wait队列
      INIT_LIST_HEAD(&thread->todo);//初始化thread的todo队列
      rb_link_node(&thread->rb_node, parent, p);//链接到proc->threads
      rb_insert_color(&thread->rb_node, &proc->threads);
      thread->looper |= BINDER_LOOPER_STATE_NEED_RETURN;//设置looper的状态
      thread->return_error = BR_OK;
      thread->return_error2 = BR_OK;
   }
   return thread;
}

```

`binder_ioctl`函数会根据传入的cmd 执行对应的操作，例如我们之前`service_manager`中`binder_open`的时候调用了ioctl 传入`cmd=BINDER_VERSION` 就会将`version存入ver->protocol_version`,然后会进行version的校验。`(ioctl(bs->fd, BINDER_VERSION, &vers) == -1)`。可以返回上边的binder\_open 注意是`service_manager里面的binder.c不是驱动层`。

总结下:

1.`binder_init` 注册了Binder驱动， `poll,unlocked_ioctl compat_ioctl mmap open flush release`。

2.`binder_open` 创建了`binder_proc`以及初始化进程信息，todo，wait队列，并且把proc->node添加到`binder_procs`。

3.`binder_mmap` 开辟`内核空间(128k)`,同时开辟`物理内存空间(128k)`，然后把`内核空间和物理空间进行映射`，使他们2个指向`同一个内存地址`。这里我们看到开辟的内存大小为1页也就是4k，这么小根本不够用，其实他是在数据传递的时候按需开辟内存的，我们等讲到数据传递的时候再来看 如何开辟。

4.`binder_ioctl` 对binder设备进行读写操作，我们上边的代码中只带入了service\_manager.c的binder\_open调用的时候传递的BINDER\_VERSION校验。后边我们会再进来了解的。

### 3.再看service\_manager

#### 1.成为上下文管理者 binder\_become\_context\_manager

在`binder_open`打开了binder创建了binder\_proc，`ioctl(bs->fd, BINDER_VERSION, &vers)`校验了`binder_version`,调用了`mmap`对`用户空间、内核空间、物理内存` 进行了映射。之后调用了`binder_become_context_manager(bs)`,将binder设置成为上下文管理者。我们来看看

```
int binder_become_context_manager(struct binder_state *bs)
{
    struct flat_binder_object obj;//创建了flat_binder_object结构体 
    memset(&obj, 0, sizeof(obj));
    obj.flags = FLAT_BINDER_FLAG_TXN_SECURITY_CTX;
    //调用驱动层的binder_ioctl cmd=BINDER_SET_CONTEXT_MGR_EXT obj传入
    int result = ioctl(bs->fd, BINDER_SET_CONTEXT_MGR_EXT, &obj);

    // fallback to original method
    if (result != 0) {
        android_errorWriteLog(0x534e4554, "121035042");

        result = ioctl(bs->fd, BINDER_SET_CONTEXT_MGR, 0);
    }
    return result;
}
```

在这里调用Binder驱动，传入的cmd是`BINDER_SET_CONTEXT_MGR_EXT`，那么Binder如何处理的呢？上边 贴过代码了，所以我们这次就贴处理部分的代码(具体代码在上方`binder_ioctl`):

```
case BINDER_SET_CONTEXT_MGR_EXT: {
   struct flat_binder_object fbo;
    //从用户空间拷贝数据到fbo  service_manager传递的是一个flat_binder_object obj
   if (copy_from_user(&fbo, ubuf, sizeof(fbo))) {
      ret = -EINVAL;
      goto err;
   }
   //调用binder_ioctl_set_ctx_mgr 设置成为管理员
   ret = binder_ioctl_set_ctx_mgr(filp, &fbo);
   if (ret)
      goto err;
   break;
}

//fob 就是service_manager传递的flat_binder_object
static int binder_ioctl_set_ctx_mgr(struct file *filp,
                struct flat_binder_object *fbo)
{
   int ret = 0;
   //获取到当前进程的proc
   struct binder_proc *proc = filp->private_data;
   //获取到当前binder的管理者
   struct binder_context *context = proc->context;
   struct binder_node *new_node;//创建binder_node
   kuid_t curr_euid = current_euid();

   mutex_lock(&context->context_mgr_node_lock);
   if (context->binder_context_mgr_node) {//如果有就直接返回，第一次来默认是空的
      pr_err("BINDER_SET_CONTEXT_MGR already set\n");
      ret = -EBUSY;
      goto out;
   }
   //检查当前进程是否有注册成为mgr的权限 感兴趣的大家可以去看看Selinux 我在这里就简单说下service_manager的权限配置文件在servicemanager.te中 设置了是domain 具有unconfined_domain的属性 只要有这个属性都有设置ContextMananger的权限。
   ret = security_binder_set_context_mgr(proc->tsk);
   if (ret < 0)//没有权限就执行out
      goto out;
   if (uid_valid(context->binder_context_mgr_uid)) {//校验uid
      if (!uid_eq(context->binder_context_mgr_uid, curr_euid)) {
         pr_err("BINDER_SET_CONTEXT_MGR bad uid %d != %d\n",
                from_kuid(&init_user_ns, curr_euid),
                from_kuid(&init_user_ns,
                context->binder_context_mgr_uid));
         ret = -EPERM;
         goto out;
      }
   } else {//设置context->binder_context_mgr_uid
      context->binder_context_mgr_uid = curr_euid;
   }
   //创建binder_node
   new_node = binder_new_node(proc, fbo);
   if (!new_node) {
      ret = -ENOMEM;
      goto out;
   }
   //锁
   binder_node_lock(new_node);
   //软引用++
   new_node->local_weak_refs++;
   //强指针++
   new_node->local_strong_refs++;
   new_node->has_strong_ref = 1;
   new_node->has_weak_ref = 1;
   //context->binder_context_mgr_node = new_node;设置当前node为binder的上下文管理者
   context->binder_context_mgr_node = new_node;
   binder_node_unlock(new_node);
   binder_put_node(new_node);
out:
   mutex_unlock(&context->context_mgr_node_lock);
   return ret;
}

//创建binder_node binder_node就是保护用户空间的各种service的指针用来找binder对象的。
static struct binder_node *binder_new_node(struct binder_proc *proc,
                  struct flat_binder_object *fp)
{
   struct binder_node *node;
   //申请binder_node的内存
   struct binder_node *new_node = kzalloc(sizeof(*node), GFP_KERNEL);
    //锁
   binder_inner_proc_lock(proc);
   //初始化binder_node
   node = binder_init_node_ilocked(proc, new_node, fp);
   binder_inner_proc_unlock(proc);
   if (node != new_node)//返回的是同一个
      kfree(new_node);
    //返回node
   return node;
}

//初始化binder_node fp是service_manager传递过来的 里面也是0值
static struct binder_node *binder_init_node_ilocked(
                  struct binder_proc *proc,
                  struct binder_node *new_node,
                  struct flat_binder_object *fp)
{
   struct rb_node **p = &proc->nodes.rb_node;
   struct rb_node *parent = NULL;
   struct binder_node *node;
   //fp不为null 所以值是fp->binder 不过fp->binder也没值 所以ptr cookie都是0
   binder_uintptr_t ptr = fp ? fp->binder : 0;
   binder_uintptr_t cookie = fp ? fp->cookie : 0;
   __u32 flags = fp ? fp->flags : 0;
   s8 priority;

   assert_spin_locked(&proc->inner_lock);

    //proc->nodes.rb_nodes 插入到红黑树中
   while (*p) {

      parent = *p;
      node = rb_entry(parent, struct binder_node, rb_node);

      if (ptr < node->ptr)
         p = &(*p)->rb_left;
      else if (ptr > node->ptr)
         p = &(*p)->rb_right;
      else {
         /*
          * A matching node is already in
          * the rb tree. Abandon the init
          * and return it.
          */
         binder_inc_node_tmpref_ilocked(node);
         return node;
      }
   }
   node = new_node;
   binder_stats_created(BINDER_STAT_NODE);
   node->tmp_refs++;
   rb_link_node(&node->rb_node, parent, p);
   rb_insert_color(&node->rb_node, &proc->nodes);
   node->debug_id = atomic_inc_return(&binder_last_id);
   node->proc = proc;
   node->ptr = ptr;
   node->cookie = cookie;
   node->work.type = BINDER_WORK_NODE;
   priority = flags & FLAT_BINDER_FLAG_PRIORITY_MASK;
   node->sched_policy = (flags & FLAT_BINDER_FLAG_SCHED_POLICY_MASK) >>
      FLAT_BINDER_FLAG_SCHED_POLICY_SHIFT;
   node->min_priority = to_kernel_prio(node->sched_policy, priority);
   node->accept_fds = !!(flags & FLAT_BINDER_FLAG_ACCEPTS_FDS);
   node->inherit_rt = !!(flags & FLAT_BINDER_FLAG_INHERIT_RT);
   node->txn_security_ctx = !!(flags & FLAT_BINDER_FLAG_TXN_SECURITY_CTX);
   spin_lock_init(&node->lock);
   INIT_LIST_HEAD(&node->work.entry);
   INIT_LIST_HEAD(&node->async_todo);
   binder_debug(BINDER_DEBUG_INTERNAL_REFS,
           "%d:%d node %d u%016llx c%016llx created\n",
           proc->pid, current->pid, node->debug_id,
           (u64)node->ptr, (u64)node->cookie);

   return node;
}


```

也就是说把`service_mananger`的`proc->context-> binder_context_mgr_node`设置成我们新创建的`binder_node`。

最后调用了`binder_loop`。我们就来看看service\_manager的binder\_loop对Binder做了什么?上边我们贴过binder\_loop的代码了，这里就不贴了，只贴关键的代码:

```
readbuf[0] = BC_ENTER_LOOPER; //在申请的readbuf写入 BC_ENTER_LOOPER
binder_write(bs, readbuf, sizeof(uint32_t));//调用binder_write和binder通信
```

我们看看`binder_write`函数:

```
int binder_write(struct binder_state *bs, void *data, size_t len)
{
   struct binder_write_read bwr;
   int res;
   bwr.write_size = len;
   bwr.write_consumed = 0;
   bwr.write_buffer = (uintptr_t) data;//wirte_buffer = BC_ENTER_LOOPER
   bwr.read_size = 0;
   bwr.read_consumed = 0;
   bwr.read_buffer = 0;
   res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);
   if (res < 0) {
       fprintf(stderr,"binder_write: ioctl failed (%s)\n",
               strerror(errno));
   }
   return res;
}
```

Binder驱动是如何处理的呢？

```
case BINDER_WRITE_READ://此时的cmd = BINDER_WRITE_READ  args是 bwr write_size = len(也就是4字节) 
   ret = binder_ioctl_write_read(filp, cmd, arg, thread);
   if (ret)
      goto err;
   break;
   

static int binder_ioctl_write_read(struct file *filp,
            unsigned int cmd, unsigned long arg,
            struct binder_thread *thread)
{
   int ret = 0;
   struct binder_proc *proc = filp->private_data;//拿到当前进程的proc
   unsigned int size = _IOC_SIZE(cmd);//cmd是BINDER_WRITE_READ
   void __user *ubuf = (void __user *)arg;
   struct binder_write_read bwr;

   if (size != sizeof(struct binder_write_read)) {
      ret = -EINVAL;
      goto out;
   }
   if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {//拷贝用户空间的ubuf也就是传递的bwr到内核空间的的bwr中
      ret = -EFAULT;
      goto out;
   }

   if (bwr.write_size > 0) {//write是大于0的
      ret = binder_thread_write(proc, thread,
                 bwr.write_buffer,
                 bwr.write_size,
                 &bwr.write_consumed);
      trace_binder_write_done(ret);
      if (ret < 0) {
         bwr.read_consumed = 0;
         if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
            ret = -EFAULT;
         goto out;
      }
   }
   //……………………
   if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {
      ret = -EFAULT;
      goto out;
   }
out:
   return ret;
}

//进入写函数 binder_buffer是BC_ENTER_LOOPER size是4字节
static int binder_thread_write(struct binder_proc *proc,
         struct binder_thread *thread,
         binder_uintptr_t binder_buffer, size_t size,
         binder_size_t *consumed)
{
   uint32_t cmd;
   struct binder_context *context = proc->context;
   //传递过来的数据BC_ENTER_LOOPER
   void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
   void __user *ptr = buffer + *consumed;
   //结束的位置
   void __user *end = buffer + size;
   while (ptr < end && thread->return_error.cmd == BR_OK) {
      int ret;
      //从用户空间拷贝4个字节到cmd中 也就是cmd = BC_ENTER_LOOPER 
      if (get_user(cmd, (uint32_t __user *)ptr))
         return -EFAULT;
        //指针偏移
      ptr += sizeof(uint32_t);
      trace_binder_command(cmd);
      if (_IOC_NR(cmd) < ARRAY_SIZE(binder_stats.bc)) {
         atomic_inc(&binder_stats.bc[_IOC_NR(cmd)]);
         atomic_inc(&proc->stats.bc[_IOC_NR(cmd)]);
         atomic_inc(&thread->stats.bc[_IOC_NR(cmd)]);
      }
      switch (cmd) {
      //……………………
      case BC_ENTER_LOOPER:
         binder_debug(BINDER_DEBUG_THREADS,
                 "%d:%d BC_ENTER_LOOPER\n",
                 proc->pid, thread->pid);
         if (thread->looper & BINDER_LOOPER_STATE_REGISTERED) {
            thread->looper |= BINDER_LOOPER_STATE_INVALID;
            binder_user_error("%d:%d ERROR: BC_ENTER_LOOPER called after BC_REGISTER_LOOPER\n",
               proc->pid, thread->pid);
         }
         thread->looper |= BINDER_LOOPER_STATE_ENTERED;
         break;
         //…………………………
         }
      *consumed = ptr - buffer;
   }
   return 0;
}

写入BINDER_LOOPER_STATE_ENTERED之后，我们回到binder_loop中 看看接下来写了什么


binder_loop中的代码:
for (;;) {//无限循环
    bwr.read_size = sizeof(readbuf);//这个时候read_size >0  read_buffer=BC_ENTER_LOOPER
    bwr.read_consumed = 0;
    bwr.read_buffer = (uintptr_t) readbuf;
    //调用Binder驱动传入BINDER_WRITE_READ 并且传入bwr
    res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);

    if (res < 0) {
        ALOGE("binder_loop: ioctl failed (%s)\n", strerror(errno));
        break;
    }

    res = binder_parse(bs, 0, (uintptr_t) readbuf, bwr.read_consumed, func);
    if (res == 0) {
        ALOGE("binder_loop: unexpected reply?!\n");
        break;
    }
    if (res < 0) {
        ALOGE("binder_loop: io error %d %s\n", res, strerror(errno));
        break;
    }
}

驱动层binder.c的binder_ioctl_write_read函数:
之前writesize>0现在write_size =0,read_size>0了
if (bwr.read_size > 0) {
   ret = binder_thread_read(proc, thread, bwr.read_buffer,
             bwr.read_size,
             &bwr.read_consumed,
             filp->f_flags & O_NONBLOCK);
   trace_binder_read_done(ret);
   binder_inner_proc_lock(proc);
   if (!binder_worklist_empty_ilocked(&proc->todo))
      binder_wakeup_proc_ilocked(proc);
   binder_inner_proc_unlock(proc);
   if (ret < 0) {
      if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
         ret = -EFAULT;
      goto out;
   }
}

//执行read函数
static int binder_thread_read(struct binder_proc *proc,
               struct binder_thread *thread,
               binder_uintptr_t binder_buffer, size_t size,
               binder_size_t *consumed, int non_block)
{
//用户空间传递过来的数据readBuf
   void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
   void __user *ptr = buffer + *consumed;
   void __user *end = buffer + size;

   int ret = 0;
   int wait_for_proc_work;

   if (*consumed == 0) {//这里传递的是0 给用户空间put了一个BR_NOOP ptr就是上边传来的bwr
      if (put_user(BR_NOOP, (uint32_t __user *)ptr))
         return -EFAULT;
      ptr += sizeof(uint32_t);
   }

retry:
   binder_inner_proc_lock(proc);
   //检查是否有工作需要处理 检查todo队列 和 transaction_stack!=null thread->todo是空 wait_for_proc_work = true
   wait_for_proc_work = binder_available_for_proc_work_ilocked(thread);
   binder_inner_proc_unlock(proc);
//设置looper的状态为等待
   thread->looper |= BINDER_LOOPER_STATE_WAITING;

   trace_binder_wait_for_work(wait_for_proc_work,
               !!thread->transaction_stack,
               !binder_worklist_empty(proc, &thread->todo));
   if (wait_for_proc_work) {//这里是true
      if (!(thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
               BINDER_LOOPER_STATE_ENTERED))) {//进入了enter_loop 也就是之前处理的BC_ENTER_LOOPER
               //调用等待函数进行等待，等待客户端的唤醒
         wait_event_interruptible(binder_user_error_wait,
                   binder_stop_on_user_error < 2);
      }
      //恢复优先级
      binder_restore_priority(current, proc->default_priority);
   }

   if (non_block) {//非阻塞模式  默认是阻塞的
      if (!binder_has_work(thread, wait_for_proc_work))
         ret = -EAGAIN;
   } else {
   //看看binder_proc中是否有工作
      ret = binder_wait_for_work(thread, wait_for_proc_work);
   }

//解除等待
   thread->looper &= ~BINDER_LOOPER_STATE_WAITING;

   if (ret)
      return ret;

   while (1) {
      uint32_t cmd;
      struct binder_transaction_data_secctx tr;
      struct binder_transaction_data *trd = &tr.transaction_data;
      struct binder_work *w = NULL;
      struct list_head *list = NULL;
      struct binder_transaction *t = NULL;
      struct binder_thread *t_from;
      size_t trsize = sizeof(*trd);

      binder_inner_proc_lock(proc);
      if (!binder_worklist_empty_ilocked(&thread->todo))
         list = &thread->todo;
      else if (!binder_worklist_empty_ilocked(&proc->todo) &&
            wait_for_proc_work)
         list = &proc->todo;
      else {
         binder_inner_proc_unlock(proc);

         /* no data added */
         if (ptr - buffer == 4 && !thread->looper_need_return)
            goto retry;
         break;
      }

      if (end - ptr < sizeof(tr) + 4) {
         binder_inner_proc_unlock(proc);
         break;
      }
      w = binder_dequeue_work_head_ilocked(list);
      if (binder_worklist_empty_ilocked(&thread->todo))
         thread->process_todo = false;

      switch (w->type) {
      case BINDER_WORK_TRANSACTION: {
       //……………………
      } break;
      case BINDER_WORK_RETURN_ERROR: {
       //……………………
      } break;
      case BINDER_WORK_TRANSACTION_COMPLETE: {
      //……………………
      } break;
      case BINDER_WORK_NODE: {
      //……………………
      } break;
      case BINDER_WORK_DEAD_BINDER:
      case BINDER_WORK_DEAD_BINDER_AND_CLEAR:
      case BINDER_WORK_CLEAR_DEATH_NOTIFICATION: {
      //……………………
      break;
   }

done:

   *consumed = ptr - buffer;
   binder_inner_proc_lock(proc);
   if (proc->requested_threads == 0 &&
       list_empty(&thread->proc->waiting_threads) &&
       proc->requested_threads_started < proc->max_threads &&
       (thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
        BINDER_LOOPER_STATE_ENTERED)) /* the user-space code fails to */
        /*spawn a new thread if we leave this out */) {
      proc->requested_threads++;
      binder_inner_proc_unlock(proc);
      binder_debug(BINDER_DEBUG_THREADS,
              "%d:%d BR_SPAWN_LOOPER\n",
              proc->pid, thread->pid);
      if (put_user(BR_SPAWN_LOOPER, (uint32_t __user *)buffer))
         return -EFAULT;
      binder_stat_br(proc, thread, BR_SPAWN_LOOPER);
   } else
      binder_inner_proc_unlock(proc);
   return 0;
}

```

首先写入了`BC_ENTER_LOOPER` 让`thread->loop`设置成了`BINDER_LOOPER_STATE_ENTERED`,然后进入`read`模式 此时`transaction_stack`和`todo`为空，所以进入`睡眠`，等待客户端唤醒。

至此service\_manager开启完毕，会一直等待客户端的请求连接。

`擦擦汗，service_manager已经进入休眠了，让我们再来看看 怎么唤醒它的吧。下面我们先跳出Binder回到ServiceManager。`

### 4.ServiceMananger的获取与添加

#### 1.前提线索

文件目录:`/frameworks/base/services/java/com/android/server/SystemServer.java`          `/frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java`

让我们回到`system_server`，看看`AMS`是怎么添加到`ServiceManager`中去的吧。在`startBootstrapServices`函数中调用 `mActivityManagerService = ActivityManagerService.Lifecycle.startService( mSystemServiceManager, atm);`创建了`AMS`。然后调用`mActivityManagerService.setSystemProcess()`。我们看看`setSystemProcess`

```
public void setSystemProcess() {
    //……………………… 代码比较多我就只留下关键代码
        ServiceManager.addService(Context.ACTIVITY_SERVICE, this, /* allowIsolated= */ true,
                DUMP_FLAG_PRIORITY_CRITICAL | DUMP_FLAG_PRIORITY_NORMAL | DUMP_FLAG_PROTO);
               // ……………………
}
```

通过调用`ServiceManager`的静态方法`addService`传递了`Context.ACTIVITY_SERVICE（值是 activity）`,`this`,`true`。

我们进入`ServiceManager`:`/android/frameworks/base/core/java/android/os/ServiceManager.java`

#### 2\. addService

```
public static void addService(String name, IBinder service, boolean allowIsolated,
     int dumpPriority) {
 try {
     getIServiceManager().addService(name, service, allowIsolated, dumpPriority);
 } catch (RemoteException e) {
     Log.e(TAG, "error in addService", e);
 }
}
```

调用了`getIServiceManager().addService()`;参数原地不动的传入了进去，我们看看getIServiceManager是谁

```
private static IServiceManager getIServiceManager() {
    if (sServiceManager != null) {
        return sServiceManager;
    }

    // Find the service manager
    //asInterface(BpBinder)
    //调用了BinderInternal.getContextObject();我们看看他返回的是啥
    sServiceManager = ServiceManagerNative
            .asInterface(Binder.allowBlocking(BinderInternal.getContextObject()));
    return sServiceManager;
}
```

文件目录:`/frameworks/base/core/java/com/android/internal/os/BinderInternal.java`

```
public static final native IBinder getContextObject();
```

是一个`native`函数。走，去native看看 文件目录:`/frameworks/base/core/jni/android_util_Binder.cpp`

```
static jobject android_os_BinderInternal_getContextObject(JNIEnv* env, jobject clazz)
{
     //熟悉的朋友ProcessState回来了 看看他的getContextObject
    sp<IBinder> b = ProcessState::self()->getContextObject(NULL);
    //这里的b就是BpBinder(0)
    //包装成Java层的android/os/BinderProxy 并返回
    return javaObjectForIBinder(env, b);
}
//ProcessState.cpp
sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/)
{
    return getStrongProxyForHandle(0);
}
//handle = 0
sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);
    //拿到handle_entry
    handle_entry* e = lookupHandleLocked(handle);

    if (e != nullptr) {
        IBinder* b = e->binder;//binder==null 新建的嘛
        if (b == nullptr || !e->refs->attemptIncWeak(this)) {
            if (handle == 0) {//handle == 0 
                Parcel data;//创建Parcel 
                //调用IPCThreadState的transact 传入PING_TRANSACTION我们稍后再看
                status_t status = IPCThreadState::self()->transact(
                        0, IBinder::PING_TRANSACTION, data, nullptr, 0);
                if (status == DEAD_OBJECT)
                   return nullptr;
            }
            //创建一个BpBinder(handle = 0);
            b = BpBinder::create(handle);
            e->binder = b;//给handle_entry->binder赋值
            if (b) e->refs = b->getWeakRefs();
            result = b;//返回BpBinder(0)
        } else {
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }

    return result;
}

//看当前是否有 如果没有就新建一个
ProcessState==handle_entry* ProcessState==lookupHandleLocked(int32_t handle)
{
    const size_t N=mHandleToObject.size();//这里是0
    if (N <= (size_t)handle) {
        handle_entry e;
        e.binder = nullptr;
        e.refs = nullptr;
        status_t err = mHandleToObject.insertAt(e, N, handle+1-N);
        if (err < NO_ERROR) return nullptr;
    }
    return &mHandleToObject.editItemAt(handle);
}

//把native层的BpBinder(0)包装成BinderProxy返回
jobject javaObjectForIBinder(JNIEnv* env, const sp<IBinder>& val)
{
    if (val == NULL) return NULL;

    if (val->checkSubclass(&gBinderOffsets)) { // == false
        jobject object = static_cast<JavaBBinder*>(val.get())->object();
        LOGDEATH("objectForBinder %p: it's our own %p!\n", val.get(), object);
        return object;
    }

    BinderProxyNativeData* nativeData = new BinderProxyNativeData();
    nativeData->mOrgue = new DeathRecipientList;
    nativeData->mObject = val;

    //BinderProxy.getInstance mNativeData   nativeData
    jobject object = env->CallStaticObjectMethod(gBinderProxyOffsets.mClass,
            gBinderProxyOffsets.mGetInstance, (jlong) nativeData, (jlong) val.get());
    if (env->ExceptionCheck()) {
        // In the exception case, getInstance still took ownership of nativeData.
        return NULL;
    }
    BinderProxyNativeData* actualNativeData = getBPNativeData(env, object);
    if (actualNativeData == nativeData) {//是同一个创建一个新的Proxy
        // Created a new Proxy
        uint32_t numProxies = gNumProxies.fetch_add(1, std::memory_order_relaxed);
        uint32_t numLastWarned = gProxiesWarned.load(std::memory_order_relaxed);
        if (numProxies >= numLastWarned + PROXY_WARN_INTERVAL) {
            // Multiple threads can get here, make sure only one of them gets to
            // update the warn counter.
            if (gProxiesWarned.compare_exchange_strong(numLastWarned,
                        numLastWarned + PROXY_WARN_INTERVAL, std::memory_order_relaxed)) {
                ALOGW("Unexpectedly many live BinderProxies: %d\n", numProxies);
            }
        }
    } else {
        delete nativeData;
    }

    return object;
}

```

也就是说`getContextObject()`这个函数返回了Java层的BinderProxy,native层的`BpBinder`。注意`handle=0`。 回到`ServiceManager`的`getIserviceMananger()`:

```
//BinderInternal.getContextObject() 这里返回了BinderProxy
//Binder.allowBlocking做了什么呢？
sServiceManager = ServiceManagerNative
        .asInterface(Binder.allowBlocking(BinderInternal.getContextObject()));
        
        //设置mWarnOnBlocking = false
public static IBinder allowBlocking(IBinder binder) {
    try {
        if (binder instanceof BinderProxy) {//这里是true 设置mWarnOnBlocking = false
            ((BinderProxy) binder).mWarnOnBlocking = false;
        } else if (binder != null && binder.getInterfaceDescriptor() != null
                && binder.queryLocalInterface(binder.getInterfaceDescriptor()) == null) {
            Log.w(TAG, "Unable to allow blocking on interface " + binder);
        }
    } catch (RemoteException ignored) {
    }
    return binder;
}
那么ServiceManagerNative.asInterface呢？
文件目录:`/frameworks/base/core/java/android/os/ServiceManagerNative.java`

//注意这里的obj是BinderProxy
static public IServiceManager asInterface(IBinder obj)
{
    if (obj == null) {
        return null;
    }
    IServiceManager in =
        (IServiceManager)obj.queryLocalInterface(descriptor);
    if (in != null) {//所以这里是null
        return in;
    }
//调用 ServiceManagerProxy(BinderProxy)
    return new ServiceManagerProxy(obj);
}
文件目录:`/frameworks/base/core/java/android/os/BinderProxy.java`
 public IInterface queryLocalInterface(String descriptor) {
        return null;
    }
  回到ServiceManagerNative.java，class  `ServiceManagerProxy`在这里: 
  
public ServiceManagerProxy(IBinder remote) {//remote就是BinderProxy 并且指向的是BpBinder(0)
    mRemote = remote;
}

```

接下来看看调用的`addService`方法:

```
public void addService(String name, IBinder service, boolean allowIsolated, int dumpPriority)
        throws RemoteException {
       
    Parcel data = Parcel.obtain();
    Parcel reply = Parcel.obtain();
    
    //descriptor= "android.os.IServiceManager"
    data.writeInterfaceToken(IServiceManager.descriptor);
    data.writeString(name);//name ="activity"
    
    data.writeStrongBinder(service);//service就是ActivityManagerService的this指针 service写入data
    data.writeInt(allowIsolated ? 1 : 0);//allowIsolated是ture所以这里是1
    data.writeInt(dumpPriority);
    //这里调用的是BinderProxy的transact(ADD_SERVICE_TRANSACTION,data就是parce,reply不为null,0)
    mRemote.transact(ADD_SERVICE_TRANSACTION, data, reply, 0);
    reply.recycle();
    data.recycle();
}

我们先看看writeStrongBinder
文件目录:`/frameworks/base/core/jni/android_os_Parcel.cpp`

static void android_os_Parcel_writeStrongBinder(JNIEnv* env, jclass clazz, jlong nativePtr, jobject object)
{
    Parcel* parcel = reinterpret_cast<Parcel*>(nativePtr);
    if (parcel != NULL) {
        const status_t err = parcel->writeStrongBinder(ibinderForJavaObject(env, object));
        if (err != NO_ERROR) {
            signalExceptionForError(env, clazz, err);
        }
    }
}
文件目录:`/frameworks/native/libs/binder/Parcel.cpp`

status_t Parcel::writeStrongBinder(const sp<IBinder>& val)
{
    return flatten_binder(ProcessState::self(), val, this);
}

status_t flatten_binder(const sp<ProcessState>& /*proc*/,
    const sp<IBinder>& binder, Parcel* out)
{
    flat_binder_object obj;//老朋友了，传递的service数据都写在这里的

    if (IPCThreadState::self()->backgroundSchedulingDisabled()) {
        /* minimum priority for all nodes is nice 0 */
        obj.flags = FLAT_BINDER_FLAG_ACCEPTS_FDS;
    } else {
        /* minimum priority for all nodes is MAX_NICE(19) */
        obj.flags = 0x13 | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    }

    if (binder != nullptr) {//我们传递的是ActivityManagerService 看看AMS的localBinder返回的 是什么？我这里没编译Android源码 😂 硬盘不够了，所以就不看` 
IActivityManager.Stub`我们知道他继承自Binder.java 代码贴在下边
//文件目录:`/frameworks/base/core/java/android/os/Binder.java`
        BBinder *local = binder->localBinder();//返回的是ams的BBInder的this指针 分析在下边
        if (!local) {//这里不为null,不走这里的逻辑
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
            obj.hdr.type = BINDER_TYPE_BINDER;
            obj.binder = reinterpret_cast<uintptr_t>(local->getWeakRefs());
            obj.cookie = reinterpret_cast<uintptr_t>(local);
        }
    } else {
    //走这里的逻辑 type是Binder_type_binder
        obj.hdr.type = BINDER_TYPE_BINDER;
        obj.binder = 0;//binder== 0
        obj.cookie = 0;//cookie == 0
    }
    //把obj写入out
    return finish_flatten_binder(binder, obj, out);
}


//AMS继承自Binder 他的午餐构造函数会调用有参构造函数并且传递null
public Binder() {
    this(null);
}
public Binder(@Nullable String descriptor)  {
//通过native方法获取生成BBinder
    mObject = getNativeBBinderHolder();
    NoImagePreloadHolder.sRegistry.registerNativeAllocation(this, mObject);

    if (FIND_POTENTIAL_LEAKS) {
        final Class<? extends Binder> klass = getClass();
        if ((klass.isAnonymousClass() || klass.isMemberClass() || klass.isLocalClass()) &&
                (klass.getModifiers() & Modifier.STATIC) == 0) {
            Log.w(TAG, "The following Binder class should be static or leaks might occur: " +
                klass.getCanonicalName());
        }
    }
    mDescriptor = descriptor;
}

代码位置：android_util_Binder.cpp
static jlong android_os_Binder_getNativeBBinderHolder(JNIEnv* env, jobject clazz)
{
//返回JavaBBinderHolder 也就是JavaBBinder
    JavaBBinderHolder* jbh = new JavaBBinderHolder();
    return (jlong) jbh;
}

JavaBBinder中并没有loaclBinder的实现，但是他继承自BBinder

//class JavaBBinder : public BBinder
文件目录:`/frameworks/native/libs/binder/Binder.cpp`
//这里返回的是this指针
BBinder* BBinder::localBinder()
{
    return this;
}


//out写入数据
inline static status_t finish_flatten_binder(
    const sp<IBinder>& /*binder*/, const flat_binder_object& flat, Parcel* out)
{
    return out->writeObject(flat, false);
}

```

`addService`把AMS写入Parcel，然后通过调用transact来进行通信。

接下来看看是怎么通信的调用了`BinderProxy`的`transact`方法

```

public boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
    if (mWarnOnBlocking && ((flags & FLAG_ONEWAY) == 0)) {
        mWarnOnBlocking = false;
    }
    //…………………………

    try {
        return transactNative(code, data, reply, flags);
    } finally {
//……………………
}
android_util_Binder.cpp：

static jboolean android_os_BinderProxy_transact(JNIEnv* env, jobject obj,
        jint code, jobject dataObj, jobject replyObj, jint flags) // throws RemoteException
{
    //从java层的对象解出来native的Parcel
    Parcel* data = parcelForJavaObject(env, dataObj);
    Parcel* reply = parcelForJavaObject(env, replyObj);
    //obj是BpBinder(0)
    IBinder* target = getBPNativeData(env, obj)->mObject.get();
    bool time_binder_calls;
    int64_t start_millis;
    if (kEnableBinderSample) {
        // Only log the binder call duration for things on the Java-level main thread.
        // But if we don't
        time_binder_calls = should_time_binder_calls();

        if (time_binder_calls) {
            start_millis = uptimeMillis();
        }
    }

    //调用BpBinder的transact通信
    status_t err = target->transact(code, *data, reply, flags);

    if (err == NO_ERROR) {
        return JNI_TRUE;
    } else if (err == UNKNOWN_TRANSACTION) {
        return JNI_FALSE;
    }

    signalExceptionForError(env, obj, err, true /*canThrowRemoteException*/, data->dataSize());
    return JNI_FALSE;
}



status_t BpBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
  
    if (mAlive) {
    //调用IPCThreadState::transact(0,code,data,reply,flag),上边的PINBINDER我们没有讲这里一起看看
        status_t status = IPCThreadState::self()->transact(
            mHandle, code, data, reply, flags);
        if (status == DEAD_OBJECT) mAlive = 0;
        return status;
    }

    return DEAD_OBJECT;
}
文件目录:`/frameworks/native/libs/binder/IPCThreadState.cpp`

IPCThreadState* IPCThreadState::self()
{
    if (gHaveTLS) {//第一次肯定是false
restart:
        const pthread_key_t k = gTLS;
        IPCThreadState* st = (IPCThreadState*)pthread_getspecific(k);
        if (st) return st;
        return new IPCThreadState;
    }

    if (gShutdown) {
        ALOGW("Calling IPCThreadState::self() during shutdown is dangerous, expect a crash.\n");
        return nullptr;
    }

    pthread_mutex_lock(&gTLSMutex);
    if (!gHaveTLS) {
    //帮我们创建gtls
        int key_create_value = pthread_key_create(&gTLS, threadDestructor);
        if (key_create_value != 0) {
            pthread_mutex_unlock(&gTLSMutex);
            ALOGW("IPCThreadState::self() unable to create TLS key, expect a crash: %s\n",
                    strerror(key_create_value));
            return nullptr;
        }
        gHaveTLS = true;
    }
    pthread_mutex_unlock(&gTLSMutex);
    goto restart;
}

//写数据进行通信  handle = 0 code = ADD_SERVICE_TRANSACTION  data=AMS
status_t IPCThreadState::transact(int32_t handle,
                                  uint32_t code, const Parcel& data,
                                  Parcel* reply, uint32_t flags)
{
    status_t err;

    flags |= TF_ACCEPT_FDS;
    //调用writeTransactionData 来写数据
    err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, nullptr);

    if ((flags & TF_ONE_WAY) == 0) {//同步的
        if (UNLIKELY(mCallRestriction != ProcessState==CallRestriction==NONE)) {
       
        if (reply) {
        //调用waitForResponse等待返回结果
            err = waitForResponse(reply);
        } else {
            Parcel fakeReply;
            err = waitForResponse(&fakeReply);
        }
        [[if]] 0
        if (code == 4) { // relayout
            ALOGI("<<<<<< RETURNING transaction 4");
        } else {
            ALOGI("<<<<<< RETURNING transaction %d", code);
        }
        [[endif]]

        IF_LOG_TRANSACTIONS() {
            TextOutput::Bundle _b(alog);
            alog << "BR_REPLY thr " << (void*)pthread_self() << " / hand "
                << handle << ": ";
            if (reply) alog << indent << *reply << dedent << endl;
            else alog << "(none requested)" << endl;
        }
    } else {//异步
        err = waitForResponse(nullptr, nullptr);
    }

    return err;
}

```

通过BpBinder，最终调用到`IPCThreadState`的`writeTransactionData`

```
//把数据写入到mOut cmd = BC_TRANSACTION
status_t IPCThreadState::writeTransactionData(int32_t cmd, uint32_t binderFlags,
    int32_t handle, uint32_t code, const Parcel& data, status_t* statusBuffer)
{
    binder_transaction_data tr;//创建tr
    tr.target.ptr = 0; 
    tr.target.handle = handle;//handle = 0
    tr.code = code;//code = ADD_SERVICE_TRANSACTION
    tr.flags = binderFlags;
    tr.cookie = 0;
    tr.sender_pid = 0;
    tr.sender_euid = 0;

    const status_t err = data.errorCheck();
    if (err == NO_ERROR) {
        tr.data_size = data.ipcDataSize();//数据大小
        tr.data.ptr.buffer = data.ipcData();//数据
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
    }
    //写入cmd = BC_TRANSACTION
    mOut.writeInt32(cmd);
   //写入数据
    mOut.write(&tr, sizeof(tr));

    return NO_ERROR;
}
```

接着调用`waitForResponse`

```
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    uint32_t cmd;
    int32_t err;

    while (1) {//死循环 
        //调用talkWithDriver进行Binder通信 😂 可算到正题了
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
        case BR_TRANSACTION_COMPLETE:
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

        default:
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

//终于到正题了doReceive = true
status_t IPCThreadState::talkWithDriver(bool doReceive)
{
    if (mProcess->mDriverFD <= 0) {
        return -EBADF;
    }

    binder_write_read bwr;

    const bool needRead = mIn.dataPosition() >= mIn.dataSize();//0 所以是true

    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;//这里是true返回的是mOut.dataSize()

    bwr.write_size = outAvail;
    bwr.write_buffer = (uintptr_t)mOut.data();//获取到刚才写入的data code = ADD_SERVICE_TRANSACTION  data=AMS

    if (doReceive && needRead) {//true
        bwr.read_size = mIn.dataCapacity(); //read_size = 0
        bwr.read_buffer = (uintptr_t)mIn.data();//read_buffer= null
    } else {
        bwr.read_size = 0;
        bwr.read_buffer = 0;
    }


    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    status_t err;
    do {
[[if]] defined(__ANDROID__)
//调用ioctl和Binder通信 传入的cmd是BINDER_WRITE_READ
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
        else
            err = -errno;
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

```

把`bwr`的`read_size`置为`0`，`write_size`设置为`数据大小` `write_buffer`为上层带过来的数据`code = ADD_SERVICE_TRANSACTION data=AMS`。调用ioctl进入binder层 传入的是Binder\_WRITE\_READ 我们去看看驱动层怎么处理的。

`注：这里我就不从binder_ioctl来跟了，我们知道write_size大于0 是执行 binder_thread_write`

```
static int binder_thread_write(struct binder_proc *proc,
         struct binder_thread *thread,
         binder_uintptr_t binder_buffer, size_t size,
         binder_size_t *consumed)
{
   uint32_t cmd;
   struct binder_context *context = proc->context;
   //这里是我们带进来的数据AMS的 以及code = ADD_SERVICE_TRANSACTION 
   void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
   void __user *ptr = buffer + *consumed;
   void __user *end = buffer + size;

   while (ptr < end && thread->return_error.cmd == BR_OK) {
      int ret;
        //先读取cmd cmd是BC_TRANSACTION
      if (get_user(cmd, (uint32_t __user *)ptr))
         return -EFAULT;
      ptr += sizeof(uint32_t);
      trace_binder_command(cmd);
      if (_IOC_NR(cmd) < ARRAY_SIZE(binder_stats.bc)) {
         atomic_inc(&binder_stats.bc[_IOC_NR(cmd)]);
         atomic_inc(&proc->stats.bc[_IOC_NR(cmd)]);
         atomic_inc(&thread->stats.bc[_IOC_NR(cmd)]);
      }
      switch (cmd) {
      case BC_TRANSACTION:
      case BC_REPLY: {//此时的命令是BC_TRANSACTION
         struct binder_transaction_data tr;
           //从用户空间把tr拷贝到当前tr tr.code = ADD_SERVICE_TRANSACTION  tr.target.handle == 0 tr.data.ptr.buffer 等于传递过来的AMS
         if (copy_from_user(&tr, ptr, sizeof(tr)))
            return -EFAULT;
         ptr += sizeof(tr);
         //调用binder_transaction  cmd==BC_REPLY吗？ false的
         binder_transaction(proc, thread, &tr,
                  cmd == BC_REPLY, 0);
         break;
      }
      }
      *consumed = ptr - buffer;
   }
   return 0;
}

//进行通信
static void binder_transaction(struct binder_proc *proc,
                struct binder_thread *thread,
                struct binder_transaction_data *tr, int reply,
                binder_size_t extra_buffers_size)
{
   if (reply) {//这里是false 所以先把这里代码删除掉 实在太多了
   } else {
      if (tr->target.handle) {//handle == 0 所以这里不来 也删除掉
      } else {
         mutex_lock(&context->context_mgr_node_lock);
         target_node = context->binder_context_mgr_node;//注意我们是非null的 因为我们ServiceManager已经设置了context->binder_context_mgr_node
         if (target_node)//所以我们会进到这里来
         //通过target_node获取到binder_node也就是service_manager的binder_node
            target_node = binder_get_node_refs_for_txn(
                  target_node, &target_proc,
                  &return_error);
        
         mutex_unlock(&context->context_mgr_node_lock);
         if (target_node && target_proc == proc) {//这里不相等
              //………………
         }
      }
        //安全检查 感兴趣的可以自己去跟踪去看
      if (security_binder_transaction(proc->tsk,
                  target_proc->tsk) < 0) {
         //………………
      }
      binder_inner_proc_lock(proc);
      if (!(tr->flags & TF_ONE_WAY) && thread->transaction_stack) {
      }
   if (target_thread)
      e->to_thread = target_thread->pid;
   e->to_proc = target_proc->pid;

   //创建binder_transaction节点
   t = kzalloc(sizeof(*t), GFP_KERNEL);
   binder_stats_created(BINDER_STAT_TRANSACTION);
   spin_lock_init(&t->lock);
  //创建binder_work节点
   tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);
   binder_stats_created(BINDER_STAT_TRANSACTION_COMPLETE);

   t->debug_id = t_debug_id;

  

   if (!reply && !(tr->flags & TF_ONE_WAY))//这里为true 得知道自己从哪里来的
      t->from = thread;
   else
      t->from = NULL;
     
      
   t->sender_euid = task_euid(proc->tsk);
   t->to_proc = target_proc;//设置要去的proc 服务端也就是service_manager
   t->to_thread = target_thread;
   t->code = tr->code;//code=ADD_SERVICE_TRANSACTION
   t->flags = tr->flags;
   //设置优先级
   if (!(t->flags & TF_ONE_WAY) &&
       binder_supported_policy(current->policy)) {
      t->priority.sched_policy = current->policy;
      t->priority.prio = current->normal_prio;
   } else {
      t->priority = target_proc->default_priority;
   }
   //安全相关
   if (target_node && target_node->txn_security_ctx) {
      u32 secid;
      size_t added_size;

      security_task_getsecid(proc->tsk, &secid);
      ret = security_secid_to_secctx(secid, &secctx, &secctx_sz);
      added_size = ALIGN(secctx_sz, sizeof(u64));
      extra_buffers_size += added_size;
      if (extra_buffers_size < added_size) {
      }
   }

   trace_binder_transaction(reply, t, target_node);
    //分配缓存 建立用户空间，内核空间，物理内存的映射关系，让他们指向同一个地址
   t->buffer = binder_alloc_new_buf(&target_proc->alloc, tr->data_size,
      tr->offsets_size, extra_buffers_size,
      !reply && (t->flags & TF_ONE_WAY));
    
   t->buffer->debug_id = t->debug_id;
   t->buffer->transaction = t;//设置 传输的数据是t 也就是他自己
   t->buffer->target_node = target_node;//记录目标target_node也就是service_manager

   off_start = (binder_size_t *)(t->buffer->data +
                  ALIGN(tr->data_size, sizeof(void *)));
   offp = off_start;
    //开始从用户空间(tr->data.ptr.buffer)拷贝数据到t->buffer->data
   if (copy_from_user(t->buffer->data, (const void __user *)(uintptr_t)
            tr->data.ptr.buffer, tr->data_size)) {
            //…………失败会进来
   }
   if (copy_from_user(offp, (const void __user *)(uintptr_t)
            tr->data.ptr.offsets, tr->offsets_size)) {
          //…………失败会进来
   }
   //对齐检查
   if (!IS_ALIGNED(tr->offsets_size, sizeof(binder_size_t))) {
    //…………失败会进来
   }
   if (!IS_ALIGNED(extra_buffers_size, sizeof(u64))) {
      //…………失败会进来
   }
   off_end = (void *)off_start + tr->offsets_size;
   sg_bufp = (u8 *)(PTR_ALIGN(off_end, sizeof(void *)));
   sg_buf_end = sg_bufp + extra_buffers_size -
      ALIGN(secctx_sz, sizeof(u64));
   off_min = 0;
   //循环遍历每一个binder对象
   for (; offp < off_end; offp++) {
      struct binder_object_header *hdr;
      size_t object_size = binder_validate_object(t->buffer, *offp);

      if (object_size == 0 || *offp < off_min) {
             //…………失败会进来
      }

      hdr = (struct binder_object_header *)(t->buffer->data + *offp);
      off_min = *offp + object_size;
      switch (hdr->type) {
      case BINDER_TYPE_BINDER://我们之前传递的type是binder_type_binder
      case BINDER_TYPE_WEAK_BINDER: {
         struct flat_binder_object *fp;

         fp = to_flat_binder_object(hdr);
         //会把我们当前传入的type修改成binder_type_handle
         ret = binder_translate_binder(fp, t, thread);
      } break;
      }
   }
   //设置自己的binder_work的type为BINDER_WORK_TRANSACTION_COMPLETE 其实啥也不干 goto finsish
   tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
   //设置目标binder_proc的work.type=BINDER_WORK_TRANSACTION
   t->work.type = BINDER_WORK_TRANSACTION;

   if (reply) {//这里是false
   } else if (!(t->flags & TF_ONE_WAY)) {//这里是true
      binder_inner_proc_lock(proc);
      //把tcomplete插入到自己的binder的todo队列中
      binder_enqueue_deferred_thread_work_ilocked(thread, tcomplete);
      t->need_reply = 1;//设置reply = 1
      t->from_parent = thread->transaction_stack;
     //插入transaction_task链表中
      thread->transaction_stack = t;
      binder_inner_proc_unlock(proc);
      //插入目标进程的todo队列，并唤醒它 当前是service_manager
      if (!binder_proc_transaction(t, target_proc, target_thread)) {

      }
   } else {//异步
   }
   //减少临时引用计数
   if (target_thread)
      binder_thread_dec_tmpref(target_thread);
   binder_proc_dec_tmpref(target_proc);
   if (target_node)
      binder_dec_node_tmpref(target_node);
   return;
}

//看看怎么把binder引用转换成binder对象
static int binder_translate_binder(struct flat_binder_object *fp,
               struct binder_transaction *t,
               struct binder_thread *thread)
{
   struct binder_node *node;
   struct binder_proc *proc = thread->proc;
   struct binder_proc *target_proc = t->to_proc;
   struct binder_ref_data rdata;
   int ret = 0;
    //从proc->nodes.rb_node查找binder_node 找不到的话返回null 第一次是null 自己的进程
   node = binder_get_node(proc, fp->binder);
   
    if (!node) {
       node = binder_new_node(proc, fp);//创建新的binder_node
       if (!node)
          return -ENOMEM;
    }

  //安全校验
   if (security_binder_transfer_binder(proc->tsk, target_proc->tsk)) {
     
   }
    //查找binder_ref 并且引用计数+1 如果没有找到就创建一个 这个函数很重要我们看看
   ret = binder_inc_ref_for_node(target_proc, node,
         fp->hdr.type == BINDER_TYPE_BINDER,
         &thread->todo, &rdata);
   if (ret)
      goto done;
    //此时的binder_type是BINDER_TYPE_BINDER 转换成BINDER_TYPE_HANDLE
   if (fp->hdr.type == BINDER_TYPE_BINDER)
      fp->hdr.type = BINDER_TYPE_HANDLE;
   else
      fp->hdr.type = BINDER_TYPE_WEAK_HANDLE;
   fp->binder = 0;
   fp->handle = rdata.desc;//handle赋值为0
   fp->cookie = 0;

done:
   binder_put_node(node);
   return ret;
}

//创建新的binder_node
static struct binder_node *binder_new_node(struct binder_proc *proc,
                  struct flat_binder_object *fp)
{
   struct binder_node *node;
   struct binder_node *new_node = kzalloc(sizeof(*node), GFP_KERNEL);

   if (!new_node)
      return NULL;
   binder_inner_proc_lock(proc);
   node = binder_init_node_ilocked(proc, new_node, fp);
   binder_inner_proc_unlock(proc);
   if (node != new_node)//如果已经添加了
      /*
       * The node was already added by another thread
       */
      kfree(new_node);

   return node;
}
//创建binder引用
static int binder_inc_ref_for_node(struct binder_proc *proc,
         struct binder_node *node,
         bool strong,
         struct list_head *target_list,
         struct binder_ref_data *rdata)
{
   struct binder_ref *ref;
   struct binder_ref *new_ref = NULL;
   int ret = 0;

   binder_proc_lock(proc);
   ref = binder_get_ref_for_node_olocked(proc, node, NULL);
   if (!ref) {
      binder_proc_unlock(proc);
      new_ref = kzalloc(sizeof(*ref), GFP_KERNEL);
      if (!new_ref)
         return -ENOMEM;
      binder_proc_lock(proc);
      ref = binder_get_ref_for_node_olocked(proc, node, new_ref);
   }
   ret = binder_inc_ref_olocked(ref, strong, target_list);
   *rdata = ref->data;
   binder_proc_unlock(proc);
   if (new_ref && ref != new_ref)
      /*
       * Another thread created the ref first so
       * free the one we allocated
       */
      kfree(new_ref);
   return ret;
}
//设置binder_node的信息
static struct binder_ref *binder_get_ref_for_node_olocked(
               struct binder_proc *proc,
               struct binder_node *node,
               struct binder_ref *new_ref)
{
   struct binder_context *context = proc->context;
   struct rb_node **p = &proc->refs_by_node.rb_node;
   struct rb_node *parent = NULL;
   struct binder_ref *ref;
   struct rb_node *n;

   while (*p) {
      parent = *p;
      ref = rb_entry(parent, struct binder_ref, rb_node_node);

      if (node < ref->node)
         p = &(*p)->rb_left;
      else if (node > ref->node)
         p = &(*p)->rb_right;
      else
         return ref;
   }
   if (!new_ref)
      return NULL;

   binder_stats_created(BINDER_STAT_REF);
   new_ref->data.debug_id = atomic_inc_return(&binder_last_id);
   new_ref->proc = proc;
   new_ref->node = node;
   rb_link_node(&new_ref->rb_node_node, parent, p);
   rb_insert_color(&new_ref->rb_node_node, &proc->refs_by_node);
//如果当前是service_manager desc就是handle为0 否则为1
   new_ref->data.desc = (node == context->binder_context_mgr_node) ? 0 : 1;
   //从树中找到最后一个把desc+1 ams的desc就会被+1
   for (n = rb_first(&proc->refs_by_desc); n != NULL; n = rb_next(n)) {
      ref = rb_entry(n, struct binder_ref, rb_node_desc);
      if (ref->data.desc > new_ref->data.desc)
         break;
      new_ref->data.desc = ref->data.desc + 1;
   }

   p = &proc->refs_by_desc.rb_node;
   while (*p) {
      parent = *p;
      ref = rb_entry(parent, struct binder_ref, rb_node_desc);

      if (new_ref->data.desc < ref->data.desc)
         p = &(*p)->rb_left;
      else if (new_ref->data.desc > ref->data.desc)
         p = &(*p)->rb_right;
      else {
         dump_ref_desc_tree(new_ref, n);
         BUG();
      }
   }
   rb_link_node(&new_ref->rb_node_desc, parent, p);
   rb_insert_color(&new_ref->rb_node_desc, &proc->refs_by_desc);

   binder_node_lock(node);
   hlist_add_head(&new_ref->node_entry, &node->refs);

   binder_debug(BINDER_DEBUG_INTERNAL_REFS,
           "%d new ref %d desc %d for node %d\n",
            proc->pid, new_ref->data.debug_id, new_ref->data.desc,
            node->debug_id);
   binder_node_unlock(node);
   return new_ref;
}



//外面传入的proc指向procp地址
static struct binder_node *binder_get_node_refs_for_txn(
      struct binder_node *node,
      struct binder_proc **procp,
      uint32_t *error)
{
   struct binder_node *target_node = NULL;

   binder_node_inner_lock(node);
   if (node->proc) {
      target_node = node;
      binder_inc_node_nilocked(node, 1, 0, NULL);//binder_node的强指针+1
      binder_inc_node_tmpref_ilocked(node);//临时引用+1
      atomic_inc(&node->proc->tmp_ref);
      *procp = node->proc;//外面传入的proc指向procp地址
   } else
      *error = BR_DEAD_REPLY;
   binder_node_inner_unlock(node);

   return target_node;
}

//分配内存，建立映射  extra_buffers_size =0  is_async=true datasize = 要写入的AMS的data_size
struct binder_buffer *binder_alloc_new_buf(struct binder_alloc *alloc,
                  size_t data_size,
                  size_t offsets_size,
                  size_t extra_buffers_size,
                  int is_async)
{
   struct binder_buffer *buffer;

   mutex_lock(&alloc->mutex);
   buffer = binder_alloc_new_buf_locked(alloc, data_size, offsets_size,
                    extra_buffers_size, is_async);
   mutex_unlock(&alloc->mutex);
   return buffer;
}

//内存映射 extra_buffers_size = 0  is_async = true
static struct binder_buffer *binder_alloc_new_buf_locked(
            struct binder_alloc *alloc,
            size_t data_size,
            size_t offsets_size,
            size_t extra_buffers_size,
            int is_async)
{
   struct rb_node *n = alloc->free_buffers.rb_node;
   struct binder_buffer *buffer;
   size_t buffer_size;
   struct rb_node *best_fit = NULL;
   void *has_page_addr;
   void *end_page_addr;
   size_t size, data_offsets_size;
   int ret;

   if (alloc->vma == NULL) {//不等于null
      return ERR_PTR(-ESRCH);
   }
  //计算需要的缓冲区大小 需要做对齐
   data_offsets_size = ALIGN(data_size, sizeof(void *)) +
      ALIGN(offsets_size, sizeof(void *));

   size = data_offsets_size + ALIGN(extra_buffers_size, sizeof(void *));
   size = max(size, sizeof(void *));
    //从binder_alloc 所有空闲的空间中找到一个大小合适的binder_buffer
   while (n) {
        //buffer等于找到的空间地址
      buffer = rb_entry(n, struct binder_buffer, rb_node);
      BUG_ON(!buffer->free);
      buffer_size = binder_alloc_buffer_size(alloc, buffer);
      //查找策略 感兴趣可以自己看
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
   if (best_fit == NULL) {//没找到}
   //此时buffer指向的是所需要的空间的父节点 所以我们要让他指向真正需要的
   if (n == NULL) {
      buffer = rb_entry(best_fit, struct binder_buffer, rb_node);
      buffer_size = binder_alloc_buffer_size(alloc, buffer);
   }
//计算出buffer的结束位置(向下对齐)
   has_page_addr =
      (void *)(((uintptr_t)buffer->data + buffer_size) & PAGE_MASK);
   WARN_ON(n && buffer_size != size);
   //计算出实际需要接收数据的空间的结束位置
   end_page_addr =
      (void *)PAGE_ALIGN((uintptr_t)buffer->data + size);
      //如果超出了可用的，回复到正常可用的结束位置
   if (end_page_addr > has_page_addr)
      end_page_addr = has_page_addr;
      //调用binder_update_page_range申请内存 建立映射
   ret = binder_update_page_range(alloc, 1,
       (void *)PAGE_ALIGN((uintptr_t)buffer->data), end_page_addr);
   if (ret)
      return ERR_PTR(ret);
    //如果有剩余空间，分割buffer，把剩余的加入alloc的空闲中去
   if (buffer_size != size) {
      struct binder_buffer *new_buffer;

      new_buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
      if (!new_buffer) {
         pr_err("%s: %d failed to alloc new buffer struct\n",
                __func__, alloc->pid);
         goto err_alloc_buf_struct_failed;
      }
      new_buffer->data = (u8 *)buffer->data + size;
      list_add(&new_buffer->entry, &buffer->entry);
      new_buffer->free = 1;
      binder_insert_free_buffer(alloc, new_buffer);
   }
   //我们已经使用了 所以需要把他从空闲列表中移除
   rb_erase(best_fit, &alloc->free_buffers);
   buffer->free = 0;//标记为非空闲
   buffer->allow_user_free = 0;
   //插入到已经分配的alloc空间中
   binder_insert_allocated_buffer_locked(alloc, buffer);
   binder_alloc_debug(BINDER_DEBUG_BUFFER_ALLOC,
           "%d: binder_alloc_buf size %zd got %pK\n",
            alloc->pid, size, buffer);
   buffer->data_size = data_size;
   buffer->offsets_size = offsets_size;
   buffer->async_transaction = is_async;
   buffer->extra_buffers_size = extra_buffers_size;
   if (is_async) {
   }
   return buffer;
}
//我们看看他是怎么开辟空间的

static int binder_update_page_range(struct binder_alloc *alloc, int allocate,
                void *start, void *end)
{
   void *page_addr;
   unsigned long user_page_addr;
   struct binder_lru_page *page;
   struct vm_area_struct *vma = NULL;
   struct mm_struct *mm = NULL;//mm就是虚拟内存管理的结构体 整个应用的进程描述。 比如我们的so .text  .data   pgd 等都在这里进行管理存储 
   bool need_mm = false;
    //地址校验
   if (end <= start)
      return 0;

   if (allocate == 0)//allocate是1
      goto free_range;
 //检查是否有页框需要分配
   for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
      page = &alloc->pages[(page_addr - alloc->buffer) / PAGE_SIZE];
      if (!page->page_ptr) {
         need_mm = true;
         break;
      }
   }

   if (need_mm && atomic_inc_not_zero(&alloc->vma_vm_mm->mm_users))
      mm = alloc->vma_vm_mm;//获取到用户空间的mm

   if (mm) {
      down_read(&mm->mmap_sem);//获取mm_struct的读信号量
      if (!mmget_still_valid(mm)) {//有效性检查
         if (allocate == 0)
            goto free_range;
         goto err_no_vma;
      }
      vma = alloc->vma;
   }

   if (!vma && need_mm) {//无法映射用户空间内存
    //…………
   }
//start = buffer->data(找到的空闲地址)的首地址  一直开辟到(end_page_addr)，一页大小的开
   for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
      int ret;
      bool on_lru;
      size_t index;
       //算出来页框的地址
      index = (page_addr - alloc->buffer) / PAGE_SIZE;
      page = &alloc->pages[index];

      if (page->page_ptr) {//如果不为null说明之前映射过了
         continue;
      }
        //分配一个物理页内存
      page->page_ptr = alloc_page(GFP_KERNEL |
                   __GFP_HIGHMEM |
                   __GFP_ZERO);
      if (!page->page_ptr) {//没有分配成功
      //……
      }
      page->alloc = alloc;
      INIT_LIST_HEAD(&page->lru);
       //将物理内存空间映射到内核空间
      ret = map_kernel_range_noflush((unsigned long)page_addr,
                      PAGE_SIZE, PAGE_KERNEL,
                      &page->page_ptr);
      flush_cache_vmap((unsigned long)page_addr,
            (unsigned long)page_addr + PAGE_SIZE);
      //根据偏移量计算出用户空间的内存地址
      user_page_addr =
         (uintptr_t)page_addr + alloc->user_buffer_offset;
         //将物理内存空间映射到用户空间地址
      ret = vm_insert_page(vma, user_page_addr, page[0].page_ptr);
      

      if (index + 1 > alloc->pages_high)
         alloc->pages_high = index + 1;

      trace_binder_alloc_page_end(alloc, index);
      /* vm_insert_page does not seem to increment the refcount */
   }
   if (mm) {//释放读的信号量
      up_read(&mm->mmap_sem);
      mmput(mm);
   }
   return 0;
}
```

客户端调用`binder_thread_write()`，获取到`cmd`\=`BC_TRANSACTION`，接着从用户空间copy\_from\_user把数据`tr`拷贝到内核空间来 等于传递过来的AMS,调用`binder_transaction`进行通信 此时的cmd!=reply所以是不需要回复的。找到`service_manager(handle=0)`创建`目标进程传递的数据t(binder_transact)`和`本进程的tcomplete(binder_work)`,调用`binder_alloc_new_buf`申请内存并建立用户空间，内核空间，物理内存的映射关系，让他们指向同一个地址,接着从用户空间拷贝传递的数据`buffer`到申请都内存中，之前传递的`obj.hdr.type = BINDER_TYPE_BINDER` 这里进入`BINDER_TYPE_BINDER`逻辑 会调用`binder_translate_binder`修改`fp(flat_binder_object)的type为binder_type_handle`,然后这只自己的进程type=`BINDER_WORK_TRANSACTION_COMPLETE`,设置目标进程的type = `BINDER_WORK_TRANSACTION`，把`tcomplete`加入到自己的`todo`队列中，设置`need_reply`为1,最后把t插入到目标进程`service_manager`的`todo`队列并且调用`binder_proc_transaction`唤醒它.客户端进程就在`talkWithDriver`。看看怎么唤醒服务端的。

```
static bool binder_proc_transaction(struct binder_transaction *t,
                struct binder_proc *proc,
                struct binder_thread *thread)
{
   struct binder_node *node = t->buffer->target_node;
   struct binder_priority node_prio;
   bool oneway = !!(t->flags & TF_ONE_WAY);//这里是false 我们是同步的
   bool pending_async = false;
   binder_inner_proc_lock(proc);
      //进程如果死了 
   if (proc->is_dead || (thread && thread->is_dead)) {
      binder_inner_proc_unlock(proc);
      binder_node_unlock(node);
      return false;
   }
    //如果没有传递目标进程
   if (!thread && !pending_async)
      thread = binder_select_thread_ilocked(proc);

   if (thread) {
   //设置优先级
      binder_transaction_priority(thread->task, t, node_prio,
                   node->inherit_rt);
                   //把t->work插入到目标进程的todo队列中
      binder_enqueue_thread_work_ilocked(thread, &t->work);
   } else if (!pending_async) {
      binder_enqueue_work_ilocked(&t->work, &proc->todo);
   } else {
      binder_enqueue_work_ilocked(&t->work, &node->async_todo);
   }

   if (!pending_async)
      binder_wakeup_thread_ilocked(proc, thread, !oneway /* sync */);//唤醒目标进程的等待队列

   binder_inner_proc_unlock(proc);
   binder_node_unlock(node);

   return true;
}


static void binder_wakeup_thread_ilocked(struct binder_proc *proc,
                struct binder_thread *thread,
                bool sync)
{
   assert_spin_locked(&proc->inner_lock);

   if (thread) {
      if (sync)
         wake_up_interruptible_sync(&thread->wait);
      else//走这里
         wake_up_interruptible(&thread->wait);
      return;
   }

   binder_wakeup_poll_threads_ilocked(proc, sync);
}

```

到这里service\_manager服务端成功唤醒

在线视频:

[www.bilibili.com/video/BV1RT…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1RT411q7WQ%2F%3Fvd_source%3D689a2ec078877b4a664365bdb60362d3 "https://www.bilibili.com/video/BV1RT411q7WQ/?vd_source=689a2ec078877b4a664365bdb60362d3")