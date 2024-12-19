Framework是一个中间层，它对接了底层的实现，封装了复杂的内部逻辑，并提供外部使用接口。Framework层是应用程序开发的基础。Binder Framework层为了C++和Java两个部分，为了达到功能的复用，中间通过JNI进行衔接。Binder Framework的C++部分，头文件位于这个路径：/frameworks/native/include/binder/。实现位于这个路径：/frameworks/native/libs/binder/。binder库最终会编译成一个动态链接库：/libbinder.so，供其他进程连接使用。今天按照android Binder的流程来源码分析Binder，本篇主要是Framwork层里面C++的内容，里面涉及到的驱动层的调用，请看上一篇文章。我们知道要要想号获取相应的服务，服务必须现在ServiceManager中注册，那么问题来了，ServiceMamanger是什么时候启动的？所以本篇的主要内容如下：

-   1、ServiceManager的启动
-   2、ServiceManager的核心服务
-   3、ServiceManager的获得
-   4、注册服务
-   5、获得服务

### 一、ServiceManager的启动

##### (一) ServiceManager启动简述

ServiceManager(后边简称 SM) 是 Binder的守护进程。就像前面说的，它本身也是一个Binder的服务。是通过编写binder.c直接和Binder驱动来通信，里面含量一个循环binder\_looper来进行读取和处理事务。因为毕竟是手机，只有这样才能达到简单高效。

经过前面几篇文章，大家也知道SM的工作也很简单，就是两个：

-   1、注册服务
-   2、查询

因为Binder里面的通信一般都是由BpBinder和BBinder来实现的，就像ActivityManagerProxy与ActivityManagerService之间的通信。

##### (二)源码的位置

由于Binder中大部分的代码都是在C层，所以我特意把源码的地址发上来。里面涉及几个类，代码路径如下：

```
framework/native/cmds/servicemanager/
  - service_manager.c
  - binder.c
system/core/rootdir
   -/init.rc
kernel/drivers/ (不同Linux分支路径略有不同)
  - android/binder.c 
```

大家如果想看源码请点击下面的对应的类即可

-   [service\_manager.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Fcmds%2Fservicemanager%2Fservice_manager.c&objectId=1199104&objectType=1&isNewArticle=undefined)
-   [binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Fcmds%2Fservicemanager%2Fbinder.c&objectId=1199104&objectType=1&isNewArticle=undefined)
-   [init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Frootdir%2Finit.rc&objectId=1199104&objectType=1&isNewArticle=undefined)

##### kernel下binder.c这个文件已经不在android的源码里面了，在Linux源码里面

-   [binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199104&objectType=1&isNewArticle=undefined)

#### 强调一下这里面有两个binder.c文件，一个是framework/native/cmds/servicemanager/binder.c，另外一个是kernel/drivers/android/binder.c ，绝对不是同一个东西，千万不要弄混了。

##### (三) 启动过程

在前面文章讲解Binder驱动的时候，我们就说到了：任何使用Binder机制的进程都必须要对**/dev/binder**设备进行open以及mmap之后才能使用，这部分逻辑是所有使用Binder机制进程通用的，SM也不例外。那我们就来看看

启动流程图下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/tat6ll427t.png)

启动整体流程图.png

> ServiceManager是由init进程通过解析[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Frootdir%2Finit.rc&objectId=1199104&objectType=1&isNewArticle=undefined)文件而创建的，其所对应的可执行程序是/system/bin/servicemanager，所对应的源文件是service\_manager.c，进程名为/system/bin/servicemanager。

代码如下：

```
// init.rc  602行
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

###### 1、service\_manager.c

启动Service Manager的入口函数是service\_manager.c的main()方法如下：

```
 //service_manager.c    347行
int main(int argc, char **argv)
{
    struct binder_state *bs;
    //打开binder驱动，申请128k字节大小的内存空间
    bs = binder_open(128*1024);
    ...
    //省略部分代码
    ...
    //成为上下文管理者 
    if (binder_become_context_manager(bs)) {
        return -1;
    }

    selinux_enabled = is_selinux_enabled(); //selinux权限是否使能
    sehandle = selinux_android_service_context_handle();
    selinux_status_open(true);

    if (selinux_enabled > 0) {
        if (sehandle == NULL) {  
            abort(); //无法获取sehandle
        }
        if (getcon(&service_manager_context) != 0) {
            abort(); //无法获取service_manager上下文
        }
    }
    union selinux_callback cb;
    cb.func_audit = audit_callback;
    selinux_set_callback(SELINUX_CB_AUDIT, cb);
    cb.func_log = selinux_log_callback;
    selinux_set_callback(SELINUX_CB_LOG, cb);
    //进入无限循环，充当Server角色，处理client端发来的请求 
    binder_loop(bs, svcmgr_handler);
    return 0;
}
```

**PS:svcmgr\_handler是一个方向指针，相当于binder\_loop的每一次循环调用到svcmgr\_handler()函数。** 这部分代码 主要分为3块

-   bs = binder\_open(128\*1024)：打开binder驱动，申请128k字节大小的内存空间
-   binder\_become\_context\_manager(bs)：变成上下文的管理者
-   binder\_loop(bs, svcmgr\_handler)：进入轮询，处理来自client端发来的请求

下面我们就详细的来看下这三块的代码

###### 1.1、 binder\_open(128\*1024)

这块代码在framework/native/cmds/servicemanager/binder.c中

```
 // framework/native/cmds/servicemanager/binder.c   96行
struct binder_state *binder_open(size_t mapsize)
{
    struct binder_state *bs;
    struct binder_version vers;

    bs = malloc(sizeof(*bs));
    if (!bs) {
        errno = ENOMEM;
        return NULL;
    }

    //通过系统调用进入内核，打开Binder的驱动设备
    bs->fd = open("/dev/binder", O_RDWR);
    if (bs->fd < 0) {
        //无法打开binder设备
        goto fail_open; 
    }
    
    //通过系统调用，ioctl获取binder版本信息
    if ((ioctl(bs->fd, BINDER_VERSION, &vers) == -1) ||
        (vers.protocol_version != BINDER_CURRENT_PROTOCOL_VERSION)) {
        //如果内核空间与用户空间的binder不是同一版本
        goto fail_open; 
    }

    bs->mapsize = mapsize;
    //通过系统调用，mmap内存映射，mmap必须是page的整数倍
    bs->mapped = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, bs->fd, 0);
    if (bs->mapped == MAP_FAILED) {
        //binder设备内存映射失败
        goto fail_map; // binder
    }

    return bs;

fail_map:
    close(bs->fd);
fail_open:
    free(bs);
    return NULL;
}
```

-   1、打开binder相关操作，先调用open()打开binder设备，open()方法经过系统调用，进入Binder驱动，然后调用方法binder\_open()，该方法会在Binder驱动层创建一个**binder\_proc**对象，再将 **binder\_proc** 对象赋值给fd->private\_data，同时放入全局链表binder\_proc。
-   2、再通过ioctl检验当前binder版本与Binder驱动层的版本是否一致。
-   3、调用mmap()进行内存映射，同理mmap()方法经过系统调用，对应Binder驱动层binde\_mmap()方法，该方法会在Binder驱动层创建Binder\_buffer对象，并放入当前binder\_proc的\*\* proc->buffers \*\* 链表

###### PS:这里重点说下binder\_state

```
//framework/native/cmds/servicemanager/binder.c  89行
struct binder_state
{
    int fd;                           //dev/binder的文件描述
    void *mapped;             //指向mmap的内存地址 
    size_t mapsize;           //分配内存的大小，默认是128K
};
```

至此，整个binder\_open就已经结束了。

###### 1.2、binder\_become\_context\_manager()函数解析

代码很简单，如下：

```
 //framework/native/cmds/servicemanager/binder.c   146行
int binder_become_context_manager(struct binder_state *bs)
{
    //通过ioctl，传递BINDER_SET_CONTEXT_MGR执行
    return ioctl(bs->fd, BINDER_SET_CONTEXT_MGR, 0);
}
```

变成上下文的管理者，整个系统中只有一个这样的管理者。通过ioctl()方法经过系统调用，对应的是Binder驱动的binder\_ioctl()方法。

###### 1.2.1 binder\_ioctl解析

Binder驱动在Linux 内核中，代码在kernel中 如下：

```
//kernel/drivers/android/binder.c      3134行
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
     ...
    //省略部分代码
    ...
    switch (cmd) {
       ...
        //省略部分代码
       ...
       //3279行
      case BINDER_SET_CONTEXT_MGR:
          ret = binder_ioctl_set_ctx_mgr(filp);
          if (ret)
        goto err;
      break;
      }
       ...
        //省略部分代码
       ...
    }
    ...
    //省略部分代码
    ...
}
```

根据参数BINDER\_SET\_CONTEXT\_MGR,最终调用binder\_ioctl\_set\_ctx\_mgr()方法，这个过程会持有binder\_main\_lock。

###### 1.2.2、binder\_ioctl\_set\_ctx\_mgr() 是属于Linux kernel的部分，代码

```
//kernel/drivers/android/binder.c   3198行
static int binder_ioctl_set_ctx_mgr(struct file *filp)
{
    int ret = 0;
    struct binder_proc *proc = filp->private_data;
    struct binder_context *context = proc->context;

    kuid_t curr_euid = current_euid();
       //保证binder_context_mgr_node对象只创建一次
    if (context->binder_context_mgr_node) {
        pr_err("BINDER_SET_CONTEXT_MGR already set\n");
        ret = -EBUSY;
        goto out;
    }
    ret = security_binder_set_context_mgr(proc->tsk);
    if (ret < 0)
        goto out;
    if (uid_valid(context->binder_context_mgr_uid)) {
        if (!uid_eq(context->binder_context_mgr_uid, curr_euid)) {
            pr_err("BINDER_SET_CONTEXT_MGR bad uid %d != %d\n",
                   from_kuid(&init_user_ns, curr_euid),
                   from_kuid(&init_user_ns,
                     context->binder_context_mgr_uid));
            ret = -EPERM;
            goto out;
        }
    } else {
                //设置当前线程euid作为Service Manager的uid
        context->binder_context_mgr_uid = curr_euid;
    }
        //创建ServiceManager的实体。
    context->binder_context_mgr_node = binder_new_node(proc, 0, 0);
    if (!context->binder_context_mgr_node) {
        ret = -ENOMEM;
        goto out;
    }
    context->binder_context_mgr_node->local_weak_refs++;
    context->binder_context_mgr_node->local_strong_refs++;
    context->binder_context_mgr_node->has_strong_ref = 1;
    context->binder_context_mgr_node->has_weak_ref = 1;
out:
    return ret;
}
```

进入Binder驱动，在Binder驱动中定义的静态变量

###### 1.2.3 binder\_context 结构体

```
//kernel/drivers/android/binder.c   228行
struct binder_context {
         //service manager所对应的binder_node
    struct binder_node *binder_context_mgr_node;
        //运行service manager的线程uid
    kuid_t binder_context_mgr_uid;
    const char *name;
};
```

创建了全局的binder\_node对象binder\_context\_mgr\_node，并将binder\_context\_mgr\_node的强弱引用各加1

这时候我们再来看下binder\_new\_node()方法里面

###### 1.2.4、binder\_new\_node()函数解析

```
//kernel/drivers/android/binder.c  
static struct binder_node *binder_new_node(struct binder_proc *proc,
                       binder_uintptr_t ptr,
                       binder_uintptr_t cookie)
{
    struct rb_node **p = &proc->nodes.rb_node;
    struct rb_node *parent = NULL;
    struct binder_node *node;
        //第一次进来是空
    while (*p) {
        parent = *p;
        node = rb_entry(parent, struct binder_node, rb_node);

        if (ptr < node->ptr)
            p = &(*p)->rb_left;
        else if (ptr > node->ptr)
            p = &(*p)->rb_right;
        else
            return NULL;
    }
        //给创建的binder_node 分配内存空间
    node = kzalloc(sizeof(*node), GFP_KERNEL);
    if (node == NULL)
        return NULL;
    binder_stats_created(BINDER_STAT_NODE);
        //将创建的node对象添加到proc红黑树
    rb_link_node(&node->rb_node, parent, p);
    rb_insert_color(&node->rb_node, &proc->nodes);
    node->debug_id = ++binder_last_id;
    node->proc = proc;
    node->ptr = ptr;
    node->cookie = cookie;
        //设置binder_work的type
    node->work.type = BINDER_WORK_NODE;
    INIT_LIST_HEAD(&node->work.entry);
    INIT_LIST_HEAD(&node->async_todo);
    binder_debug(BINDER_DEBUG_INTERNAL_REFS,
             "%d:%d node %d u%016llx c%016llx created\n",
             proc->pid, current->pid, node->debug_id,
             (u64)node->ptr, (u64)node->cookie);
    return node;
}
```

在Binder驱动层创建了binder\_node结构体对象，并将当前的binder\_pro加入到binder\_node的node->proc。并创建binder\_node的async\_todo和binder\_work两个队列

###### 1.3、binder\_loop()详解

```
 // framework/native/cmds/servicemanager/binder.c    372行
    void binder_loop(struct binder_state *bs, binder_handler func) {
        int res;
        struct binder_write_read bwr;
        uint32_t readbuf[ 32];

        bwr.write_size = 0;
        bwr.write_consumed = 0;
        bwr.write_buffer = 0;

        readbuf[0] = BC_ENTER_LOOPER;
        //将BC_ENTER_LOOPER命令发送给Binder驱动，让ServiceManager进行循环
        binder_write(bs, readbuf, sizeof(uint32_t));

        for (; ; ) {
            bwr.read_size = sizeof(readbuf);
            bwr.read_consumed = 0;
            bwr.read_buffer = (uintptr_t) readbuf;
            //进入循环，不断地binder读写过程
            res = ioctl(bs -> fd, BINDER_WRITE_READ, & bwr);

            if (res < 0) {
                ALOGE("binder_loop: ioctl failed (%s)\n", strerror(errno));
                break;
            }
            //解析binder信息
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
    }
```

进入循环读写操作，由main()方法传递过来的参数func指向svcmgr\_handler。binder\_write通过ioctl()将BC\_ENTER\_LOOPER命令发送给binder驱动，此时bwr只有write\_buffer有数据，进入binder\_thread\_write()方法。 接下来进入for循环，执行ioctl()，此时bwr只有read\_buffer有数据，那么进入binder\_thread\_read()方法。

主要是循环读写操作，这里有3个重点是

-   binder\_thread\_write结构体
-   binder\_write函数
-   binder\_parse函数

###### 1.3.1 binder\_thread\_write

```
//kernel/drivers/android/binder.c    2248行
static int binder_thread_write(struct binder_proc *proc,
            struct binder_thread *thread,
            binder_uintptr_t binder_buffer, size_t size,
            binder_size_t *consumed)
{
    uint32_t cmd;
    struct binder_context *context = proc->context;
    void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
    void __user *ptr = buffer + *consumed;
    void __user *end = buffer + size;
    while (ptr < end && thread->return_error == BR_OK) {
        //获取命令
        get_user(cmd, (uint32_t __user *)ptr); 
        switch (cmd) {
              //**** 省略部分代码 ****
             case BC_ENTER_LOOPER:
             //设置该线程的looper状态
             thread->looper |= BINDER_LOOPER_STATE_ENTERED;
             break;
             //**** 省略部分代码 ****
    }
       //**** 省略部分代码 ****
    return 0;
}
```

主要是从bwr.write\_buffer中拿出数据，此处为BC\_ENTER\_LOOPER，可见上层调用binder\_write()方法主要是完成当前线程的looper状态为BINDER\_LOOPER\_STATE\_ENABLE。

###### 1.3.2、 binder\_write函数

这块的函数在

```
    // framework/native/cmds/servicemanager/binder.c     151行
    int binder_write(struct binder_state *bs, void *data, size_t len) {
        struct binder_write_read bwr;
        int res;

        bwr.write_size = len;
        bwr.write_consumed = 0;
        //此处data为BC_ENTER_LOOPER
        bwr.write_buffer = (uintptr_t) data;
        bwr.read_size = 0;
        bwr.read_consumed = 0;
        bwr.read_buffer = 0;
        res = ioctl(bs -> fd, BINDER_WRITE_READ, & bwr);
        if (res < 0) {
            fprintf(stderr, "binder_write: ioctl failed (%s)\n",
                    strerror(errno));
        }
        return res;
    }
```

根据传递进来的参数，初始化bwr，其中write\_size大小为4,write\_buffer指向缓冲区的起始地址，其内容为BC\_ENTER\_LOOPER请求协议号。通过ioctl将bwr数据发送给Binder驱动，则调用binder\_ioctl函数

###### 1.3.3让我们来看下binder\_ioctl函数

```
//kernel/drivers/android/binder.c     3239行
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
      //**** 省略部分代码 ****
     //获取binder_thread
    thread = binder_get_thread(proc); 
    switch (cmd) {
      case BINDER_WRITE_READ:  
          //进行binder的读写操作
          ret = binder_ioctl_write_read(filp, cmd, arg, thread); 
          if (ret)
              goto err;
          break;
          //**** 省略部分代码 ****
    }
}  
```

主要就是根据参数 BINDER\_SET\_CONTEXT\_MGR，最终调用binder\_ioctl\_set\_ctx\_mgr()方法，这个过程会持有binder\_main\_lock。

###### binder\_ioctl\_write\_read()函数解析

```
//kernel/drivers/android/binder.c    3134
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
        //把用户空间数据ubuf拷贝到bwr中
    if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {
        ret = -EFAULT;
        goto out;
    }
    binder_debug(BINDER_DEBUG_READ_WRITE,
             "%d:%d write %lld at %016llx, read %lld at %016llx\n",
             proc->pid, thread->pid,
             (u64)bwr.write_size, (u64)bwr.write_buffer,
             (u64)bwr.read_size, (u64)bwr.read_buffer);
        // “写缓存” 有数据
    if (bwr.write_size > 0) {
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
        // "读缓存" 有数据
    if (bwr.read_size > 0) {
        ret = binder_thread_read(proc, thread, bwr.read_buffer,
                     bwr.read_size,
                     &bwr.read_consumed,
                     filp->f_flags & O_NONBLOCK);
        trace_binder_read_done(ret);
        if (!list_empty(&proc->todo))
            wake_up_interruptible(&proc->wait);
        if (ret < 0) {
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
         //将内核数据bwr拷贝到用户控件bufd
    if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {
        ret = -EFAULT;
        goto out;
    }
out:
    return ret;
}
```

此处代码就一个作用：就是讲用户空间的binder\_write\_read结构体 拷贝到内核空间。

###### 1.3.3、 binder\_parse函数解析

binder\_parse在// framework/native/cmds/servicemanager/binder.c中

```
  // framework/native/cmds/servicemanager/binder.c    204行
 int binder_parse(struct binder_state *bs, struct binder_io *bio,
                     uintptr_t ptr, size_t size, binder_handler func) {
        int r = 1;
        uintptr_t end = ptr + (uintptr_t) size;

        while (ptr < end) {
            uint32_t cmd = *(uint32_t *) ptr;
            ptr += sizeof(uint32_t);
            #if TRACE
            fprintf(stderr, "%s:\n", cmd_name(cmd));
            #endif
            switch (cmd) {
                case BR_NOOP:
                    //误操作，退出循环
                    break;
                case BR_TRANSACTION_COMPLETE:
                    break;
                case BR_INCREFS:
                case BR_ACQUIRE:
                case BR_RELEASE:
                case BR_DECREFS:
                    #if TRACE
                    fprintf(stderr, "  %p, %p\n", (void *)ptr, (void *)(ptr + sizeof(void *)));
                    #endif
                    ptr += sizeof(struct binder_ptr_cookie);
                    break;
                case BR_TRANSACTION: {
                    struct binder_transaction_data *txn = (struct binder_transaction_data *)ptr;
                    if ((end - ptr) < sizeof( * txn)){
                        ALOGE("parse: txn too small!\n");
                        return -1;
                    }
                    binder_dump_txn(txn);
                    if (func) {
                        unsigned rdata[ 256 / 4];
                        struct binder_io msg;
                        struct binder_io reply;
                        int res;

                        bio_init( & reply, rdata, sizeof(rdata), 4);
                        bio_init_from_txn( & msg, txn);
                        res = func(bs, txn, & msg, &reply);
                        binder_send_reply(bs, & reply, txn -> data.ptr.buffer, res);
                    }
                    ptr += sizeof( * txn);
                    break;
                }
                case BR_REPLY: {
                    struct binder_transaction_data *txn = (struct binder_transaction_data *)ptr;
                    if ((end - ptr) < sizeof( * txn)){
                        ALOGE("parse: reply too small!\n");
                        return -1;
                    }
                    binder_dump_txn(txn);
                    if (bio) {
                        bio_init_from_txn(bio, txn);
                        bio = 0;
                    } else {
                                        /* todo FREE BUFFER */
                    }
                    ptr += sizeof( * txn);
                    r = 0;
                    break;
                }
                case BR_DEAD_BINDER: {
                    struct binder_death *death = (struct binder_death *)
                    (uintptr_t) * (binder_uintptr_t *) ptr;
                    ptr += sizeof(binder_uintptr_t);
                    //binder死亡消息
                    death -> func(bs, death -> ptr);
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
```

主要是解析binder消息，此处参数ptr指向BC\_ENTER\_LOOPER，func指向svcmgr\_handler，所以有请求来，则调用svcmgr

这里面我们重点分析BR\_TRANSACTION里面的几个函数

-   bio\_init()函数
-   bio\_init\_from\_txn()函数

###### 1.3.3.1 bio\_init()函数

```
    // framework/native/cmds/servicemanager/binder.c      409行
    void bio_init_from_txn(struct binder_io *bio, struct binder_transaction_data *txn)
    {
        bio->data = bio->data0 = (char *)(intptr_t)txn->data.ptr.buffer;
        bio->offs = bio->offs0 = (binder_size_t *)(intptr_t)txn->data.ptr.offsets;
        bio->data_avail = txn->data_size;
        bio->offs_avail = txn->offsets_size / sizeof(size_t);
        bio->flags = BIO_F_SHARED;
    }
```

其中binder\_io的结构体在 /frameworks/native/cmds/servicemanager/binder.h 里面 [binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Fcmds%2Fservicemanager%2Fbinder.h&objectId=1199104&objectType=1&isNewArticle=undefined)

```
//frameworks/native/cmds/servicemanager/binder.h     12行
struct binder_io
{
    char *data;            /* pointer to read/write from */
    binder_size_t *offs;   /* array of offsets */
    size_t data_avail;     /* bytes available in data buffer */
    size_t offs_avail;     /* entries available in offsets array */

    char *data0;           //data buffer起点位置
    binder_size_t *offs0;  //buffer偏移量的起点位置
    uint32_t flags;
    uint32_t unused;
};
```

###### 1.3.3.2 bio\_init\_from\_txn()函数

```
// framework/native/cmds/servicemanager/binder.c    409行
void bio_init_from_txn(struct binder_io *bio, struct binder_transaction_data *txn)
{
    bio->data = bio->data0 = (char *)(intptr_t)txn->data.ptr.buffer;
    bio->offs = bio->offs0 = (binder_size_t *)(intptr_t)txn->data.ptr.offsets;
    bio->data_avail = txn->data_size;
    bio->offs_avail = txn->offsets_size / sizeof(size_t);
    bio->flags = BIO_F_SHARED;
}
```

其实很简单，就是将readbuf的数据赋给bio对象的data 将readbuf的数据赋给bio对象的data

####### 1.3.4 svcmgr\_handler

```
 //service_manager.c    244行
int svcmgr_handler(struct binder_state*bs,
                       struct binder_transaction_data*txn,
                       struct binder_io*msg,
                       struct binder_io*reply) {
        struct svcinfo*si;
        uint16_t * s;
        size_t len;
        uint32_t handle;
        uint32_t strict_policy;
        int allow_isolated;

        if (txn -> target.ptr != BINDER_SERVICE_MANAGER)
            return -1;

        if (txn -> code == PING_TRANSACTION)
            return 0;


        strict_policy = bio_get_uint32(msg);
        s = bio_get_string16(msg, & len);
        if (s == NULL) {
            return -1;
        }

        if ((len != (sizeof(svcmgr_id) / 2)) ||
                memcmp(svcmgr_id, s, sizeof(svcmgr_id))) {
            fprintf(stderr, "invalid id %s\n", str8(s, len));
            return -1;
        }

        if (sehandle && selinux_status_updated() > 0) {
            struct selabel_handle*tmp_sehandle = selinux_android_service_context_handle();
            if (tmp_sehandle) {
                selabel_close(sehandle);
                sehandle = tmp_sehandle;
            }
        }

        switch (txn -> code) {
            case SVC_MGR_GET_SERVICE:
            case SVC_MGR_CHECK_SERVICE:
                //获取服务名
                s = bio_get_string16(msg, & len);
                if (s == NULL) {
                    return -1;
                }
                //根据名称查找相应服务 
                handle = do_find_service(bs, s, len, txn -> sender_euid, txn -> sender_pid);
                if (!handle)
                    break;
                bio_put_ref(reply, handle);
                return 0;

            case SVC_MGR_ADD_SERVICE:
                //获取服务名
                s = bio_get_string16(msg, & len);
                if (s == NULL) {
                    return -1;
                }
                handle = bio_get_ref(msg);
                allow_isolated = bio_get_uint32(msg) ? 1 : 0;
                 //注册服务
                if (do_add_service(bs, s, len, handle, txn -> sender_euid,
                        allow_isolated, txn -> sender_pid))
                    return -1;
                break;

            case SVC_MGR_LIST_SERVICES: {
                uint32_t n = bio_get_uint32(msg);

                if (!svc_can_list(txn -> sender_pid)) {
                    ALOGE("list_service() uid=%d - PERMISSION DENIED\n",
                            txn -> sender_euid);
                    return -1;
                }
                si = svclist;
                while ((n-- > 0) && si)
                    si = si -> next;
                if (si) {
                    bio_put_string16(reply, si -> name);
                    return 0;
                }
                return -1;
            }
            default:
                ALOGE("unknown code %d\n", txn -> code);
                return -1;
        }
        bio_put_uint32(reply, 0);
        return 0;
    }
```

代码看着很多，其实主要就是servicemanger提供查询服务和注册服务以及列举所有服务。 这里提一下svcinfo

```
 //service_manager.c    128行
    struct svcinfo
    {
        struct svcinfo*next;
        uint32_t handle;
        struct binder_death death;
        int allow_isolated;
        size_t len;
        uint16_t name[ 0];
    };
```

每一个服务用svcinfo结构体来表示，该handle值是注册服务的过程中，又服务所在进程那一端所确定。

###### 1.3.4 总结

ServiceManager集中管理系统内的所有服务，通过权限控制进程是否有权注册服务，通过字符串名称来查找对应的Service；由于ServiceManager进程建立跟所有向其注册服务的死亡通知，那么当前服务所在进程死亡后，会只需要告知ServiceManager。每个Client通过查询ServiceManager可获取Service进程的情况，降低所有Client进程直接检测导致负载过重。

让我们再次看这张图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/tat6ll427t.png)

启动整体流程图.png

ServiceManager 启动流程：

-   打开binder驱动，并调用mmap()方法分配128k内存映射空间：binder\_open()
-   通知binder驱动使其成为守护进程：binder\_become\_context\_manager()；
-   验证selinux权限，判断进程是否有权注册或查看指定服务;
-   进入循环状态，等待Client端的请求
-   注册服务的过程，根据服务的名称，但同一个服务已注册，然后调用binder\_node\_release。这个过程便会发出死亡通知的回调。

### 二、ServiceManager的核心服务

通过上面的代码我们知道service manager的核心服务主要有4个

-   do\_add\_service()函数：注册服务
-   do\_find\_service()函数：查找服务
-   binder\_link\_to\_death()函数：结束服务
-   binder\_send\_reply()函数：将注册结果返回给Binder驱动

下面我们就挨个讲解一下

##### (一)、do\_add\_service()函数

```
//service_manager.c      194行
int do_add_service(struct binder_state *bs,
                   const uint16_t *s, size_t len,
                   uint32_t handle, uid_t uid, int allow_isolated,
                   pid_t spid)
{
    struct svcinfo *si;

    if (!handle || (len == 0) || (len > 127))
        return -1;

    //权限检查
    if (!svc_can_register(s, len, spid)) {
        return -1;
    }

    //服务检索
    si = find_svc(s, len);
    if (si) {
        if (si->handle) {
            svcinfo_death(bs, si); //服务已注册时，释放相应的服务
        }
        si->handle = handle;
    } else {
        si = malloc(sizeof(*si) + (len + 1) * sizeof(uint16_t));
        if (!si) { 
           //内存不足，无法分配足够内存
            return -1;
        }
        si->handle = handle;
        si->len = len;
        //内存拷贝服务信息
        memcpy(si->name, s, (len + 1) * sizeof(uint16_t));
        si->name[len] = '\0';
        si->death.func = (void*) svcinfo_death;
        si->death.ptr = si;
        si->allow_isolated = allow_isolated;
        // svclist保存所有已注册的服务
        si->next = svclist; 
        svclist = si;
    }

    //以BC_ACQUIRE命令，handle为目标的信息，通过ioctl发送给binder驱动
    binder_acquire(bs, handle);
    //以BC_REQUEST_DEATH_NOTIFICATION命令的信息，通过ioctl发送给binder驱动，主要用于清理内存等收尾工作。
    binder_link_to_death(bs, handle, &si->death);
    return 0;
}
```

注册服务部分主要分块内容：

-   svc\_can\_register：检查权限：检查selinux权限是否满足
-   find\_svc：服务检索，根据服务名来查询匹配的服务；
-   svcinfo\_death：释放服务，当查询到已存在的同名的服务，则先清理该服务信息，再讲当前的服务加入到服务列表svclist;

svc\_can\_register：检查权限，检查selinux权限是否满足； find\_svc：服务检索，根据服务名来查询匹配的服务； svcinfo\_death：释放服务，当查询到已存在同名的服务，则先清理该服务信息，再将当前的服务加入到服务列表svclist；

###### 1、svc\_can\_register()函数

```
//service_manager.c      110行
static int svc_can_register(const uint16_t *name, size_t name_len, pid_t spid)
{
    const char *perm = "add";
    //检查selinux权限是否满足
    return check_mac_perms_from_lookup(spid, perm, str8(name, name_len)) ? 1 : 0;
}
```

###### 2、svcinfo\_death()函数

```
//service_manager.c      153行
void svcinfo_death(struct binder_state *bs, void *ptr)
{
    struct svcinfo *si = (struct svcinfo* ) ptr;

    if (si->handle) {
        binder_release(bs, si->handle);
        si->handle = 0;
    }
}
```

###### 3、bio\_get\_ref()函数

```
// framework/native/cmds/servicemanager/binder.c     627行
uint32_t bio_get_ref(struct binder_io *bio)
{
    struct flat_binder_object *obj;

    obj = _bio_get_obj(bio);
    if (!obj)
        return 0;

    if (obj->type == BINDER_TYPE_HANDLE)
        return obj->handle;

    return 0;
}
```

##### (二)、do\_find\_service()

```
 //service_manager.c      170行
uint32_t do_find_service(struct binder_state *bs, const uint16_t *s, size_t len, uid_t uid, pid_t spid)
{
    //具体查询相应的服务
    struct svcinfo *si = find_svc(s, len);
    if (!si || !si->handle) {
        return 0;
    }

    if (!si->allow_isolated) {
        uid_t appid = uid % AID_USER;
         //检查该服务是否允许孤立于进程而单独存在
        if (appid >= AID_ISOLATED_START && appid <= AID_ISOLATED_END) {
            return 0;
        }
    }
     //服务是否满足于查询条件
    if (!svc_can_find(s, len, spid)) {
        return 0;
    }
   /返回结点中的ptr，这个ptr是binder中对应的binder_ref.des
    return si->handle;
}
```

主要就是查询目标服务，并返回该服务所对应的handle

###### 1、find\_svc()函数

```
 //service_manager.c      140行
struct svcinfo *find_svc(const uint16_t *s16, size_t len)
{
    struct svcinfo *si;
    for (si = svclist; si; si = si->next) {
        //当名字完全一致，则返回查询到的结果
        if ((len == si->len) &&
            !memcmp(s16, si->name, len * sizeof(uint16_t))) {
            return si;
        }
    }
    return NULL;
}
```

在svclist服务列表中，根据服务名遍历查找是否已经注册。当服务已经存在svclist，则返回相应的服务名，否则返回null。

> 当找到服务的handle，则调用bio\_put\_ref(reply,handle)，将handle封装到reply。

在svcmgr\_handler中当执行完do\_find\_service()函数后，会调用bio\_put\_ref()函数，让我们来一起研究下这个函数

###### 2、bio\_put\_ref()函数

```
// framework/native/cmds/servicemanager/binder.c    505行
void bio_put_ref(struct binder_io *bio, uint32_t handle)
{
    //构造了一个flat_binder_object
    struct flat_binder_object *obj;
    if (handle)
        obj = bio_alloc_obj(bio); 
    else
        obj = bio_alloc(bio, sizeof(*obj));
    if (!obj)
        return;
    obj->flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    obj->type = BINDER_TYPE_HANDLE; //返回的是HANDLE类型
    //以service manager的身份回应给kernel driver，ptr就是handler对应的ref索引值 1，2，3，4，5，6等
    obj->handle = handle;
    obj->cookie = 0;
}
```

这个段代码也不复杂，就是根据handle来判断分别执行bio\_alloc\_obj()函数和bio\_alloc()函数 那我们就来好好研究和两个函数

###### 3、bio\_alloc\_obj()函数

```
// framework/native/cmds/servicemanager/binder.c   468 行 
static struct flat_binder_object *bio_alloc_obj(struct binder_io *bio)
{
    struct flat_binder_object *obj;
    obj = bio_alloc(bio, sizeof(*obj));//[见小节3.1.4]

    if (obj && bio->offs_avail) {
        bio->offs_avail--;
        *bio->offs++ = ((char*) obj) - ((char*) bio->data0);
        return obj;
    }
    bio->flags |= BIO_F_OVERFLOW;
    return NULL;
}
```

###### 4、bio\_alloc()函数

```
// framework/native/cmds/servicemanager/binder.c   437 行 
static void *bio_alloc(struct binder_io *bio, size_t size)
{
    size = (size + 3) & (~3);
    if (size > bio->data_avail) {
        bio->flags |= BIO_F_OVERFLOW;
        return NULL;
    } else {
        void *ptr = bio->data;
        bio->data += size;
        bio->data_avail -= size;
        return ptr;
    }
}
```

##### (三) 、 binder\_link\_to\_death() 函数

```
// framework/native/cmds/servicemanager/binder.c        305行
void binder_link_to_death(struct binder_state *bs, uint32_t target, struct binder_death *death)
{
    struct {
        uint32_t cmd;
        struct binder_handle_cookie payload;
    } __attribute__((packed)) data;

    data.cmd = BC_REQUEST_DEATH_NOTIFICATION;
    data.payload.handle = target;
    data.payload.cookie = (uintptr_t) death;
    binder_write(bs, &data, sizeof(data)); //[见小节3.3.1]
}
```

binder\_write和前面的binder\_write一样，进入Binder driver后，直接调用binder\_thread\_write，处理BC\_REQUEST\_DEATH\_NOTIFICATION命令。其中binder\_ioctl\_write\_read()函数，上面已经讲解过了。这里就不详细讲解了

###### 1、 binder\_thread\_write() 函数

```
//kernel/drivers/android/binder.c    2248行
static int binder_thread_write(struct binder_proc *proc,
            struct binder_thread *thread,
            binder_uintptr_t binder_buffer, size_t size,
            binder_size_t *consumed)
{
    uint32_t cmd;
    struct binder_context *context = proc->context;
    void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
    void __user *ptr = buffer + *consumed;
    void __user *end = buffer + size;
    while (ptr < end && thread->return_error == BR_OK) {
        //获取命令
        get_user(cmd, (uint32_t __user *)ptr); 
        switch (cmd) {
              //**** 省略部分代码 ****
              // 注册死亡通知
             case BC_REQUEST_DEATH_NOTIFICATION:
         case BC_CLEAR_DEATH_NOTIFICATION: { 
              uint32_t target;
            void __user *cookie;
            struct binder_ref *ref;
            struct binder_ref_death *death;
            //获取taget
            get_user(target, (uint32_t __user *)ptr); 
            ptr += sizeof(uint32_t);
            /获取death
            get_user(cookie, (void __user * __user *)ptr); /
            ptr += sizeof(void *);
            //拿到目标服务的binder_ref
            ref = binder_get_ref(proc, target); 
            if (cmd == BC_REQUEST_DEATH_NOTIFICATION) {
                 //已设死亡通知
                if (ref->death) {
                    break; 
                }
                death = kzalloc(sizeof(*death), GFP_KERNEL);
                INIT_LIST_HEAD(&death->work.entry);
                death->cookie = cookie;
                ref->death = death;
                //当目标服务所在进程已死，则发送死亡通知
                if (ref->node->proc == NULL) {
                    //当前线程为binder线程，则直接添加到当前线程的TODO队列
                    ref->death->work.type = BINDER_WORK_DEAD_BINDER;
                    if (thread->looper & (BINDER_LOOPER_STATE_REGISTERED | BINDER_LOOPER_STATE_ENTERED)) {
                        list_add_tail(&ref->death->work.entry, &thread->todo);
                    } else {
                        list_add_tail(&ref->death->work.entry, &proc->todo);
                        wake_up_interruptible(&proc->wait);
                    }
                }
            } else {
                ...
            }
        } break;
       //**** 省略部分代码 ****
    }
       //**** 省略部分代码 ****
    return 0;
}
```

此方法中的proc，thread都是指当前的servicemanager进程信息，此时TODO队列有数据，则进入binder\_thread\_read。

那么问题来了，哪些场景会向队列增加BINDER\_WORK\_READ\_BINDER事物？那边是当binder所在进程死亡后，会调用binder\_realse方法，然后调用binder\_node\_release这个过程便会发出死亡通知的回调。

###### 2、binder\_thread\_read() 函数

```
static int binder_thread_read(struct binder_proc *proc,
                  struct binder_thread *thread,
                  binder_uintptr_t binder_buffer, size_t size,
                  binder_size_t *consumed, int non_block)
    ...
    //只有当前线程todo队列为空，并且transaction_stack也为空，才会开始处于当前进程的事务
    if (wait_for_proc_work) {
        ...
        ret = wait_event_freezable_exclusive(proc->wait, binder_has_proc_work(proc, thread));
    } else {
        ...
        ret = wait_event_freezable(thread->wait, binder_has_thread_work(thread));
    }
    //加锁
    binder_lock(__func__);
    if (wait_for_proc_work)
        //空闲的binder线程减1
        proc->ready_threads--; 
    thread->looper &= ~BINDER_LOOPER_STATE_WAITING;
    while (1) {
        uint32_t cmd;
        struct binder_transaction_data tr;
        struct binder_work *w;
        struct binder_transaction *t = NULL;
        //从todo队列拿出前面放入的binder_work, 此时type为BINDER_WORK_DEAD_BINDER
        if (!list_empty(&thread->todo)) {
            w = list_first_entry(&thread->todo, struct binder_work,
                         entry);
        } else if (!list_empty(&proc->todo) && wait_for_proc_work) {
            w = list_first_entry(&proc->todo, struct binder_work,
                         entry);
        }

        switch (w->type) {
            case BINDER_WORK_DEAD_BINDER: {
              struct binder_ref_death *death;
              uint32_t cmd;

              death = container_of(w, struct binder_ref_death, work);
              if (w->type == BINDER_WORK_CLEAR_DEATH_NOTIFICATION)
                  ...
              else
              //进入此分支
                  cmd = BR_DEAD_BINDER; 
              //拷贝用户空间
              put_user(cmd, (uint32_t __user *)ptr);
              ptr += sizeof(uint32_t);

              //此处的cookie是前面传递的svcinfo_death
              put_user(death->cookie, (binder_uintptr_t __user *)ptr);
              ptr += sizeof(binder_uintptr_t);

              if (w->type == BINDER_WORK_CLEAR_DEATH_NOTIFICATION) {
                  ...
              } else
                  list_move(&w->entry, &proc->delivered_death);
              if (cmd == BR_DEAD_BINDER)
                  goto done;
            } break;
        }
    }
    ...
    return 0;
}
```

将命令BR\_DEAD\_BINDER写到用户空间，此处的cookie是前面传递的svcinfo\_death。当binder\_loop下一次执行binder\_parse的过程便会处理该消息。 binder\_parse()函数和svcinfo\_death()函数上面已经说明了，这里就不详细说明了。

###### 3、 binder\_release() 函数

```
//frameworks/native/cmds/servicemanager/binder.c   297行
void binder_release(struct binder_state *bs, uint32_t target)
{
    uint32_t cmd[2];
    cmd[0] = BC_RELEASE;
    cmd[1] = target;
    binder_write(bs, cmd, sizeof(cmd));
}
```

向Binder Driver写入BC\_RELEASE命令，最终进入Binder Driver后执行binder\_dec\_ref(ref，1) 来减少binder node的引用。

##### (四)、 binder\_send\_reply() 函数

```
//frameworks/native/cmds/servicemanager/binder.c   170行
    void binder_send_reply(struct binder_state *bs,
                           struct binder_io *reply,
                           binder_uintptr_t buffer_to_free,
                           int status) {
        struct {
            uint32_t cmd_free;
            binder_uintptr_t buffer;
            uint32_t cmd_reply;
            struct binder_transaction_data txn;
        } __attribute__((packed)) data;
        //free buffer命令
        data.cmd_free = BC_FREE_BUFFER;
        data.buffer = buffer_to_free;
        //replay命令
        data.cmd_reply = BC_REPLY;
        data.txn.target.ptr = 0;
        data.txn.cookie = 0;
        data.txn.code = 0;
        if (status) {
            data.txn.flags = TF_STATUS_CODE;
            data.txn.data_size = sizeof(int);
            data.txn.offsets_size = 0;
            data.txn.data.ptr.buffer = (uintptr_t) & status;
            data.txn.data.ptr.offsets = 0;
        } else {
            data.txn.flags = 0;
            data.txn.data_size = reply -> data - reply -> data0;
            data.txn.offsets_size = ((char*)reply -> offs)-((char*)reply -> offs0);
            data.txn.data.ptr.buffer = (uintptr_t) reply -> data0;
            data.txn.data.ptr.offsets = (uintptr_t) reply -> offs0;
        }
        //向Binder驱动通信
        binder_write(bs, & data, sizeof(data));
    }
```

执行binder\_parse方法，先调用svcmgr\_handler()函数，然后再执行binder\_send\_reply过程，该过程会调用binder\_write进入binder驱动后，将BC\_FREE\_BUFFER和BC\_REPLY命令协议发送给Binder驱动，向Client端发送reply，其中data数据区中保存的是TYPE为HANDLE。

现在我们对ServiceManager有个初步的了解，那么我们怎么才能得到ServiceManager那？下面就让我们来看下如何获得ServiceManager。

### 三、ServiceManager的获得

##### (一)、源码信息

代码位于

```
framework/native/libs/binder/
  - ProcessState.cpp
  - BpBinder.cpp
  - Binder.cpp
  - IServiceManager.cpp
framework/native/include/binder/
  - IServiceManager.h
  - IInterface.h
```

链接为

-   [ProcessState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp&objectId=1199104&objectType=1&isNewArticle=undefined)
-   [BpBinder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FBpBinder.cpp&objectId=1199104&objectType=1&isNewArticle=undefined)
-   [Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FBinder.cpp&objectId=1199104&objectType=1&isNewArticle=undefined)
-   [IServiceManager.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIServiceManager.cpp&objectId=1199104&objectType=1&isNewArticle=undefined)
-   [IServiceManager.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Finclude%2Fbinder%2FIServiceManager.h&objectId=1199104&objectType=1&isNewArticle=undefined)
-   [IInterface.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Finclude%2Fbinder%2FIInterface.h&objectId=1199104&objectType=1&isNewArticle=undefined)

###### 这里重点提醒下framework/native/libs/binder/IServiceManager.cpp和 framework/native/include/binder/IServiceManager.h大家千万不要弄混了。

##### (二)、获取Service Manager简述

> 获取Service Manager是通过defaultServiceManager()方法来完成的。当进程 **注册服务** 与 **获取服务**之前，都需要调用defaultServiceManager()方法来获取gDefaultServiceManager对象。对于gDefaultServiceManager对象，如果存在直接返回。如果不存在直接创建该对象，创建过程包括调用open()打开binder驱动设备，利用mmap()映射内核的地址空间。

##### (三)、流程图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/xs8t03em90.png)

获取ServiceManager流程图.png

##### (四)、获取defaultServiceManager

代码如下

```
//frameworks/native/libs/binder/IServiceManager.cpp      33行
sp<IServiceManager> defaultServiceManager()
{
    if (gDefaultServiceManager != NULL) return gDefaultServiceManager;
    {
         //加锁
        AutoMutex _l(gDefaultServiceManagerLock); 
        while (gDefaultServiceManager == NULL) {
            gDefaultServiceManager = interface_cast<IServiceManager>(
                //这里才是关键和重点
                ProcessState::self()->getContextObject(NULL));
            if (gDefaultServiceManager == NULL)
                sleep(1);
        }
    }
    return gDefaultServiceManager;
}
```

获取ServiceManager 对象采用单例模式，当gDefaultServiceManager存在，则直接返回，否则创建一个新对象。这里的创建单利模式和咱们之前的java里面的单例不一样。它里面多了一层while循环，这是谷歌在2013年1月Todd Poynor提交的修改。因为当第一次尝试创建获取ServiceManager时，ServiceManager可能还未准备就绪，所以通过sleep1秒，实现延迟1秒，然后尝试去获取直到成功。

而gDefualtServiceManager的创建过程又可以分解为3个步骤

-   ProcessState：：self() ：用于获取ProcessState对象(也是单例模式)，每个进程有且只有一个ProcessState对象，存在则直接返回，不存在则创建。
-   getContextObject()： 用于获取BpBinder对象，对于hanle=0的BpBinder对象，存在则直接返回，不存在则创建。
-   interface\_cast<IServiceManager>()：用于获取BpServiceManager对象。

所以下面的 (五)(六)(七) 依次讲解ProcessState、BpBinder对象和BpServiceManager对象

##### (五)、获取ProcessState对象

###### 1、ProcessState::self

我们先来看下这块代码

```
//frameworks/native/libs/binder/ProcessState.cpp   70行
// 这又是一个进程单体
sp<ProcessState> ProcessState::self()
{
    Mutex::Autolock _l(gProcessMutex);
    if (gProcess != NULL) {
        return gProcess;
    }
    //实例化 ProcessState，首次创建
    gProcess = new ProcessState;
    return gProcess;
}
```

获取ProcessState对象：这也是一个单利模式，从而保证每一个进程只有一个ProcessState对象，其中gProccess和gProccessMutex是保持在Static.cpp的类全局变量。

那我们来一起看下ProccessState的构造函数

###### 2、ProccessState的构造函数

```
//frameworks/native/libs/binder/ProcessState.cpp       339行
ProcessState::ProcessState()
    //这里打开了打开了Binder驱动，也就是/dev/binder文件，返回文件描述符
    : mDriverFD(open_driver())
    , mVMStart(MAP_FAILED)
    , mThreadCountLock(PTHREAD_MUTEX_INITIALIZER)
    , mThreadCountDecrement(PTHREAD_COND_INITIALIZER)
    , mExecutingThreadsCount(0)
    , mMaxThreads(DEFAULT_MAX_BINDER_THREADS)
    , mManagesContexts(false)
    , mBinderContextCheckFunc(NULL)
    , mBinderContextUserData(NULL)
    , mThreadPoolStarted(false)
    , mThreadPoolSeq(1)
{
    if (mDriverFD >= 0) {
        //采用内存映射函数mmap，给binder分配一块虚拟地址空间，涌来了接收事物
        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
        if (mVMStart == MAP_FAILED) {
            //没有足够空间分配给/dev/binder，则关闭驱动。
            close(mDriverFD); 
            mDriverFD = -1;
        }
    }
}
```

通过上面的构造函数我们知道

-   ProcessState的单利模式的唯一性，因此一个进程只打开binder设备一次，其中ProcessState的成员变量mDriverFD记录binder驱动的fd，用于访问binder设备。
-   BINDER\_VM\_SIZE=(1_1024_1024- (4096_2))，所以binder分配的默认内存大小是1024_1016也就是1M-8K(1M减去8k)
-   DEFAULT\_MAX\_BINDER\_THREAD=15，binder默认的最大可并发的线程数为16。

这里面调用了open\_driver()方法，那么让我们研究下这个方法

###### 3、open\_driver()方法

```
//frameworks/native/libs/binder/ProcessState.cpp       311行
static int open_driver()
{
    // 打开/dev/binder设备，建立与内核的Binder驱动的交互通道
    int fd = open("/dev/binder", O_RDWR);
    if (fd >= 0) {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        int vers = 0;
        status_t result = ioctl(fd, BINDER_VERSION, &vers);
        if (result == -1) {
            close(fd);
            fd = -1;
        }
        if (result != 0 || vers != BINDER_CURRENT_PROTOCOL_VERSION) {
            close(fd);
            fd = -1;
        }
        size_t maxThreads = DEFAULT_MAX_BINDER_THREADS;

        // 通过ioctl设置binder驱动，能支持的最大线程数
        result = ioctl(fd, BINDER_SET_MAX_THREADS, &maxThreads);
        if (result == -1) {
            ALOGE("Binder ioctl to set max threads failed: %s", strerror(errno));
        }
    } else {
        ALOGW("Opening '/dev/binder' failed: %s\n", strerror(errno));
    }
    return fd;
}
```

open\_driver的作用就是打开/dev/binder设备，设定binder支持的最大线程数。binder驱动相应的内容请看上一篇文章。

##### (六)、获取BpBiner对象

###### 1、getContextObject()方法

```
//frameworks/native/libs/binder/ProcessState.cpp       85行
sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/)
{
    return getStrongProxyForHandle(0);  
}
```

我们发现这里面什么都没做，就是调用getStrongProxyForHandle()方法，**大家注意它的入参写死为0**，然后我们继续深入

###### 2、getStrongProxyForHandle()方法

注释有点长，我把注释删除了

```
//frameworks/native/libs/binder/ProcessState.cpp       179行
sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;
    AutoMutex _l(mLock);
    //查找handle对应的资源项
    handle_entry* e = lookupHandleLocked(handle);
    if (e != NULL) {
        IBinder* b = e->binder;
        if (b == NULL || !e->refs->attemptIncWeak(this)) {
            if (handle == 0) {
                Parcel data;
                //通过ping操作测试binder是否已经准备就绪
                status_t status = IPCThreadState::self()->transact(
                        0, IBinder::PING_TRANSACTION, data, NULL, 0);
                if (status == DEAD_OBJECT)
                   return NULL;
            }
           //当handle值所对应的IBinder不存在或弱引用无效时，则创建BpBinder对象
            b = new BpBinder(handle);
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }
    return result;
}
```

**大家注意上面函数的入参handle=0**

> 当handle值所对应的IBinder不存在或弱引用无效时会创建BpBinder，否则直接获取。针对hande==0的特殊情况，通过PING\_TRANSACTION来判断是否准备就绪。如果在context manager还未生效前，一个BpBinder的本地引用就已经被创建，那么驱动将无法提供context manager的引用。

在getStrongProxyForHandle()方法里面先后调用了lookupHandleLocked()方法和创建BpBinder对象，那我们就来详细研究下

###### 3、lookupHandleLocked()方法

```
//frameworks/native/libs/binder/ProcessState.cpp      166行
ProcessState::handle_entry* ProcessState::lookupHandleLocked(int32_t handle)
{
    const size_t N=mHandleToObject.size();
    //当handle大于mHandleToObject的长度时，进入该分支
    if (N <= (size_t)handle) {
        handle_entry e;
        e.binder = NULL;
        e.refs = NULL;
        //从mHandleToObject的第N个位置开始，插入(handle+1-N)个e到队列中
        status_t err = mHandleToObject.insertAt(e, N, handle+1-N);
        if (err < NO_ERROR) return NULL;
    }
    return &mHandleToObject.editItemAt(handle);
}
```

根据handle值来查找对应的handle\_entry,handle\_entry是一个结构体，里面记录了IBinder和weakref\_type两个指针。当handle大于mHandleToObject的Vector长度时，则向Vector中添加(handle+1-N)个handle\_entry结构体，然后再返回handle向对应位置的handle\_entry结构体指针。

###### 4、创建BpBinder

```
//frameworks/native/libs/binder/BpBinder.cpp       89行
BpBinder::BpBinder(int32_t handle)
    : mHandle(handle)
    , mAlive(1)
    , mObitsSent(0)
    , mObituaries(NULL)
{
    //延长对象的生命时间
    extendObjectLifetime(OBJECT_LIFETIME_WEAK); 
    // handle所对应的bindle弱引用+1
    IPCThreadState::self()->incWeakHandle(handle); 
}
```

创建BpBinder对象中将handle相对应的弱引用+1

##### (七)、获取BpServiceManager对象

###### 1、interface\_cast()函数

```
//frameworks/native/include/binder/IInterface.h  42行
template<typename INTERFACE>
inline sp<INTERFACE> interface_cast(const sp<IBinder>& obj)
{
    return INTERFACE::asInterface(obj); 
}
```

这是一个模板函数，可得出，interface\_cast<IServiceManager>()等价于IServiceManager::asInterface()。接下来，再说说asInterface()函数的具体功能。

###### 2、IServiceManager::asInterface()函数

对于asInterface()函数，通过搜索代码，你会发现根本找不到这个方法是在哪里定义这个函数的，其实是通过模板函数来定义的，通过下面两个代码完成的

```
// 位于IServiceManager.h     33行
DECLARE_META_INTERFACE(ServiceManager)
//位于IServiceManager.cpp    108行
IMPLEMENT_META_INTERFACE(ServiceManager,"android.os.IServiceManager")
```

那我们就来重点说下这两块代码的功能

###### 3、DECLARE\_META\_INTERFACE

```
//framework/native/include/binder/IInterface.h      74行
#define DECLARE_META_INTERFACE(INTERFACE)                               
   static const android::String16 descriptor;                          
   static android::sp<I##INTERFACE> asInterface(                       
          const android::sp<android::IBinder>& obj);                  
   virtual const android::String16& getInterfaceDescriptor() const;    
   I##INTERFACE();                                                     
   virtual ~I##INTERFACE();       
```

位于IServiceManager.h文件中,INTERFACE=ServiceManager展开即可得：

```
static const android::String16 descriptor;

static android::sp< IServiceManager > asInterface(const android::sp<android::IBinder>& obj)

virtual const android::String16& getInterfaceDescriptor() const;

IServiceManager ();
virtual ~IServiceManager();
```

该过程主要是声明asInterface()、getInterfaceDescriptor()方法。

###### 4、 IMPLEMENT\_META\_INTERFACE

```
//framework/native/include/binder/IInterface.h      83行
#define IMPLEMENT_META_INTERFACE(INTERFACE, NAME)                       \
    const android::String16 I##INTERFACE::descriptor(NAME);             \
    const android::String16&                                            \
            I##INTERFACE::getInterfaceDescriptor() const {              \
        return I##INTERFACE::descriptor;                                \
    }                                                                   \
    android::sp<I##INTERFACE> I##INTERFACE::asInterface(                \
            const android::sp<android::IBinder>& obj)                   \
    {                                                                   \
        android::sp<I##INTERFACE> intr;                                 \
        if (obj != NULL) {                                              \
            intr = static_cast<I##INTERFACE*>(                          \
                obj->queryLocalInterface(                               \
                        I##INTERFACE::descriptor).get());               \
            if (intr == NULL) {                                         \
                intr = new Bp##INTERFACE(obj);                          \
            }                                                           \
        }                                                               \
        return intr;                                                    \
    }                                                                   \
    I##INTERFACE::I##INTERFACE() { }                                    \
    I##INTERFACE::~I##INTERFACE() { }                                   \
```

位于IServiceManager.cpp文件中，INTERFACE=ServiceManager，NAME="android.os.IServiceManager" 开展即可得：

```
const 
 android::String16 
 IServiceManager::descriptor(“android.os.IServiceManager”);

const android::String16& IServiceManager::getInterfaceDescriptor() const
{
     return IServiceManager::descriptor;
}

 android::sp<IServiceManager> IServiceManager::asInterface(const android::sp<android::IBinder>& obj)
{
       android::sp<IServiceManager> intr;
        if(obj != NULL) {
           intr = static_cast<IServiceManager *>(
               obj->queryLocalInterface(IServiceManager::descriptor).get());
           if (intr == NULL) {
               intr = new BpServiceManager(obj);  //【见小节4.5】
            }
        }
       return intr;
}
IServiceManager::IServiceManager () { }
IServiceManager::~ IServiceManager() { }
```

###### 不难发现，上面说的IServiceManager::asInterface() 等价于new BpServiceManager()。在这里，更确切地说应该是new BpServiceManager(BpBinder)。

###### 4.1、 BpServiceManager实例化

```
//frameworks/native/libs/binder/IServiceManager.cpp    126行
class BpServiceManager : public BpInterface<IServiceManager>
{
public:
    BpServiceManager(const sp<IBinder>& impl)
        : BpInterface<IServiceManager>(impl)
    {
    }

    virtual sp<IBinder> getService(const String16& name) const
    {
        unsigned n;
        for (n = 0; n < 5; n++){
            sp<IBinder> svc = checkService(name);
            if (svc != NULL) return svc;
            ALOGI("Waiting for service %s...\n", String8(name).string());
            sleep(1);
        }
        return NULL;
            }

    virtual sp<IBinder> checkService( const String16& name) const
    {
        Parcel data, reply;
        data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());
        data.writeString16(name);
        remote()->transact(CHECK_SERVICE_TRANSACTION, data, &reply);
        return reply.readStrongBinder();
    }
    virtual status_t addService(const String16& name, const sp<IBinder>& service,
            bool allowIsolated)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());
        data.writeString16(name);
        data.writeStrongBinder(service);
        data.writeInt32(allowIsolated ? 1 : 0);
        status_t err = remote()->transact(ADD_SERVICE_TRANSACTION, data, &reply);
        return err == NO_ERROR ? reply.readExceptionCode() : err;
    }

    virtual Vector<String16> listServices()
    {
        Vector<String16> res;
        int n = 0;

        for (;;) {
            Parcel data, reply;
            data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());
            data.writeInt32(n++);
            status_t err = remote()->transact(LIST_SERVICES_TRANSACTION, data, &reply);
            if (err != NO_ERROR)
                break;
            res.add(reply.readString16());
        }
        return res;
    }
};
```

创建BpServiceManager对象的过程，会先初始化父类对象：

###### 4.2、 BpServiceManager实例化

```
//frameworks/native/include/binder/IInterface.h     135行
template<typename INTERFACE>
class BpInterface : public INTERFACE, public BpRefBase
{
   public: BpInterface(const sp<IBinder>& remote);
   protected:  virtual IBinder*            onAsBinder();
};
```

###### 4.3、BpRefBase初始化

```
BpRefBase::BpRefBase(const sp<IBinder>& o)
    : mRemote(o.get()), mRefs(NULL), mState(0)
{
    extendObjectLifetime(OBJECT_LIFETIME_WEAK);

    if (mRemote) {
        mRemote->incStrong(this);
        mRefs = mRemote->createWeak(this);
    }
}
```

new BpServiceManager()，在初始化过程中，比较重要的类BpRefBase的mRemote指向new BpBinder(0)，从而BpServiceManager能够利用Binder进行通信。

##### (八) 模板函数

C层的Binder架构，通过下面的两个宏，非常方便地创建了**new Bp##INTERFACE(obj)** 代码如下：

```
// 用于申明asInterface()，getInterfaceDescriptor()
#define DECLARE_META_INTERFACE(INTERFACE) 
// 用于实现上述两个方法
#define IMPLEMENT_META_INTERFACE(INTERFACE, NAME) 
```

例如:

```
// 实现BpServiceManager对象
IMPLEMENT_META_INTERFACE(ServiceManager,"android.os.IServiceManager")
```

等价于:

```
const android::String16 IServiceManager::descriptor(“android.os.IServiceManager”);
const android::String16& IServiceManager::getInterfaceDescriptor() const
{
     return IServiceManager::descriptor;
}

 android::sp<IServiceManager> IServiceManager::asInterface(const android::sp<android::IBinder>& obj)
{
       android::sp<IServiceManager> intr;
        if(obj != NULL) {
           intr = static_cast<IServiceManager *>(
               obj->queryLocalInterface(IServiceManager::descriptor).get());
           if (intr == NULL) {
               intr = new BpServiceManager(obj);
            }
        }
       return intr;
}

IServiceManager::IServiceManager () { }
IServiceManager::~ IServiceManager() { }
```

##### (九) 总结

-   defaultServiceManager 等价于new BpServiceManager(new BpBinder(0));
-   ProcessState:: self() 主要工作：
    -   调用open，打/dev/binder驱动设备
    -   调用mmap(),创建大小为 1016K的内存地址空间
    -   设定当前进程最大的并发Binder线程个数为16
-   BpServiceManager巧妙将通信层与业务层逻辑合为一体，通过继承接口IServiceManager实现接口中的业务逻辑函数;通过成员变量mRemote=new BpBinder(0) 进行Binder通信工作。BpBinder通过handle来指向所对应的BBinder，在整个Binder系统总handle=0代表ServiceManager所对应的BBinder。