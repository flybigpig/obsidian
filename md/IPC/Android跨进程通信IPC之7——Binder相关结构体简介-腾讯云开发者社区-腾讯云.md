### 一、结构体binder\_work

##### 1、位置

位置在 [Linux的binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199100&objectType=1&isNewArticle=undefined) 240行

###### 2、代码注释

**binder\_work**代表binder驱动中进程要处理的工作项

```
struct binder_work {
    struct list_head entry;  //用于实现一个双向链表，存储的所有binder_work队列
    enum {
        BINDER_WORK_TRANSACTION = 1,
        BINDER_WORK_TRANSACTION_COMPLETE,
        BINDER_WORK_NODE,
        BINDER_WORK_DEAD_BINDER,
        BINDER_WORK_DEAD_BINDER_AND_CLEAR,
        BINDER_WORK_CLEAR_DEATH_NOTIFICATION,
    } type;  //描述工作项的类型
};
```

### 二、结构体binder\_thread

##### 1、代码位置

位置在 [Linux的binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199100&objectType=1&isNewArticle=undefined) 368行

##### 2、代码注释

**binder\_thread** 代表Binder线程池中的每个线程信息

```
struct binder_thread {
    //宿主进程，即线程属于那么Binder进程
    struct binder_proc *proc;  
    //红黑树的一个节点，binder_proc使用红黑树来组织Binder线程池中的线程
    struct rb_node rb_node;  
    // 当前线程的PID
    int pid;
    // 当前线程的状态信息
    int looper;
    //事务堆栈，将事务封装成binder_transaction，添加到事务堆栈中，
    //定义了要接收和发送的进程和线程消息
    struct binder_transaction *transaction_stack;
    //队列，当有来自Client请求时，将会添加到to_do队列中
    struct list_head todo;
    // 记录阅读buf事务时出现异常错误情况信息
    uint32_t return_error; /* Write failed, return error code in read buf */
    // 记录阅读事务时出现异常错误情况信息
    uint32_t return_error2; /* Write failed, return error code in read */
        /* buffer. Used when sending a reply to a dead process that */
        /* we are also waiting on */
     // 等待队列，当Binder处理事务A依赖于其他Binder线程处理事务B的情况
     // 则会在sleep在wait所描述的等待队列中，知道B事物处理完毕再唤醒
    wait_queue_head_t wait;
     // Binder线程相关统计数据
    struct binder_stats stats;
};
```

这里面说下上面looper对应的值

```
enum {
    // Binder驱动 请求创建该线程，通过BC_REGISTER_LOOPER协议通知
    //Binder驱动，注册成功设置此状态
    BINDER_LOOPER_STATE_REGISTERED  = 0x01,
    //该线程是应用程序主动注册的  通过 BC_ENTER_LOOPER 协议
    BINDER_LOOPER_STATE_ENTERED     = 0x02,
    // Binder线程退出
    BINDER_LOOPER_STATE_EXITED      = 0x04,
    // Binder线程处于无效
    BINDER_LOOPER_STATE_INVALID     = 0x08,
    // Binder线程处于空闲状态
    BINDER_LOOPER_STATE_WAITING     = 0x10,
     // Binder线程处于需要返回用户控件
     // 使用场景是：1、线程注册为Binder线程后，还没有准备好去处理进程间通信
     //，需要返回用户空间做其他初始化准备；2、调用flush来刷新Binder线程池                                                                                                                 
    BINDER_LOOPER_STATE_NEED_RETURN = 0x20
};
```

### 三、结构体binder\_stats

**binder\_stats** 代表d的是Binder线程相关统计数据

###### 1、代码位置

位置在 [Linux的binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199100&objectType=1&isNewArticle=undefined) 173行

###### 2、代码注释

```
struct binder_stats {
        // 统计各个binder响应码的个数
    int br[_IOC_NR(BR_FAILED_REPLY) + 1];
        // 统计各个binder请求码的个数
    int bc[_IOC_NR(BC_REPLY_SG) + 1];
        // 统计各种obj创建的个数
    int obj_created[BINDER_STAT_COUNT];
        // 统计各种obj删除个数
    int obj_deleted[BINDER_STAT_COUNT];
};
```

### 四、结构体binder\_proc

> **binder\_proc** 代表的是一个正在使用Binder进程通信的进程，binder\_proc为管理其信息的记录体，当一个进程open /dev/binder 时，Binder驱动程序会为其创建一个binder\_proc结构体，用以记录进程的所有相关信息，并把该结构体保存到一个全局的hash表中

##### 1、代码位置

位置在 [Linux的binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199100&objectType=1&isNewArticle=undefined) 322行

##### 2、代码注释

```
struct binder_proc {
        /** 进程相关参数 */

        //上述全局hash表中一个节点，用以标记该进程
    struct hlist_node proc_node;
        // 进程组ID
    int pid;
         // 任务控制模块 
    struct task_struct *tsk; 
        // 文件结构体数组
    struct files_struct *files;

       /**  Binder线程池每一个Binder进程都有一个线程池，由Binder驱动来维护，Binder线程池中所有线程由一个红黑树来组织，RB树以线程ID为关键字  */
        //上述红黑树的根节点
    struct rb_root threads;
        
       /** 一系列Binder实体对象(binder_node)和Binder引用对象(binder_ref) */
       /** 在用户控件：运行在Server端称为Binder本地对象，运行在Client端称为Binder代理对象*/
       /**  在内核空间：Binder实体对象用来描述Binder本地对象，Binder引用对象来描述Binder代理对象 */
         // Binder实体对象列表(RB树)，关键字 ptr
    struct rb_root nodes;
         // Binder引用对象，关键字  desc
    struct rb_root refs_by_desc;
         // Binder引用对象，关键字  node
    struct rb_root refs_by_node;
         // 这里有两个引用对象，是为了方便快速查找 


     /**  进程可以调用ioctl注册线程到Binder驱动程序中，当线程池中没有足够空闲线程来处理事务时，Binder驱动可以主动要求进程注册更多的线程到Binder线程池中 */
        // Binder驱动程序最多可以请求进程注册线程的最大数量
    int max_threads;
        // Binder驱动每主动请求进程添加注册一个线程的时候，requested_threads+1
    int requested_threads;
        // 进程响应Binder要求后，requested_thread_started+1，request_threads-1，表示Binder已经主动请求注册的线程数目
    int requested_threads_started;

        // 进程当前空闲线程的数目
    int ready_threads;
        // 线程优先级，初始化为进程优先级
    long default_priority;
        //进程的整个虚拟地址空间
    struct mm_struct *vma_vm_mm;

        /** mmap 内核缓冲区*/
        // mmap——分配的内核缓冲区  用户控件地址(相较于buffer)
    struct vm_area_struct *vma; 
        // mmap——分配内核缓冲区，内核空间地址(相交于vma)  两者都是虚拟地址
    void *buffer;
        // mmap——buffer与vma之间的差值
    ptrdiff_t user_buffer_offset;

        /** buffer指向的内核缓冲区，被划分为很多小块进行性管理；这些小块保存在列表中，buffer就是列表的头部 */
         // 内核缓冲列表
    struct list_head buffers;
         // 空闲的内存缓冲区(红黑树)
    struct rb_root free_buffers;
         // 正在使用的内存缓冲区(红黑树)
    struct rb_root allocated_buffers;
        // 当前可用来保存异步事物数据的内核缓冲区大小
    size_t free_async_space;
        //  对应用于vma 、buffer虚拟机地址，这里是他们对应的物理页面
    struct page **pages;
        //  内核缓冲区大小
    size_t buffer_size;
        // 空闲内核缓冲区大小
    uint32_t buffer_free;

         /** 进程每接收到一个通信请求，Binder将其封装成一个工作项，保存在待处理队列to_do中  */
        //待处理队列
    struct list_head todo;
        // 等待队列，存放一些睡眠的空闲Binder线程
    wait_queue_head_t wait;
        // hash表，保存进程中可以延迟执行的工作项
    struct hlist_node deferred_work_node;
        // 延迟工作项的具体类型
    int deferred_work;
    
        //统计进程相关数据，具体参考binder_stats结构体
    struct binder_stats stats;
        // 表示 Binder驱动程序正在向进程发出死亡通知
    struct list_head delivered_death;
        // 用于debug
    struct dentry *debugfs_entry;
        // 连接 存储binder_node和binder_context_mgr_uid以及name
    struct binder_context *context;
};
```

### 五、结构体binder\_node

**binder\_node** 代表的是Binder实体对象，每一个service组件或者ServiceManager在Binder驱动程序中的描述，Binder驱动通过强引用和弱引用来维护其生命周期，通过node找到空间的Service对象

##### 1、代码位置

位置在 [Linux的binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199100&objectType=1&isNewArticle=undefined) 252行

##### 2、代码注释

```
struct binder_node {
        // debug调试用的
    int debug_id;
    struct binder_work work;  //binder驱动中进程要处理的工作项 

        /** 每一个binder进程都由一个binder_proc来描述，binder进程内部所有Binder实体对象，
        由一个红黑树来进行组织(struct rb_root nodes)  ； rb_node 则对应nodes的一个节点 */
    union {
        //用于本节点连接红黑树
        struct rb_node rb_node;
        // 如果Binder 实体对象对应的进程死亡，销毁节点时需要将rb_node从红黑树中删除，
        //如果本节点还没有引用切断，则用dead_node将其隔离到另一个链表中，
        //直到通知所有进程切断与该节点的引用后，该节点才能销毁
        struct hlist_node dead_node;
    };

    // 指向该Binder实体对象 对应的进程，进程由binder_proc描述
    struct binder_proc *proc;
    // 该 Binder实体对象可能同时被多个Client组件引用，所有指向本实体对象的引用都
    //保存在这个hash队列中refs表示队列头部；这些引用可能隶属于不同进程，遍历该
    //hash表能够得到这些Client组件引用了这些对象
    struct hlist_head refs;

    /** 引用计数 
     * 1、当一个Binder实体对象请求一个Service组件来执行Binder操作时。会增加该Service
     * 组件的强/弱引用计数同时，Binder实体对象将会has_strong_ref与has_weak_ref置为1 
     *2、当一个Service组件完成一个Binder实体对象请求的操作后，Binder对象会请求减少该
     * Service组件的强/弱引用计数
     * 3、Binder实体对象在请求一个Service组件增加或减少强/弱引用计数的过程中，
     * 会将pending_strong_ref和pending_weak_ref置为1，当Service组件完成增加
     * 或减少计数时，Binder实体对象会将这两个变量置为0
     */

    //远程强引用 计数
    int internal_strong_refs;  //实际上代表了一个binder_node与多少个binder_ref相关联
    //本地弱引用技数
    int local_weak_refs;
     //本地强引用计数
    int local_strong_refs;

    unsigned has_strong_ref:1;
    unsigned pending_strong_ref:1;
    unsigned has_weak_ref:1;
    unsigned pending_weak_ref:1;

     /** 用来描述用户控件中的一个Service组件 */
    // 描述用户控件的Service组件，对应Binder实体对应的Service在用户控件的(BBinder)的引用
    binder_uintptr_t ptr;
    // 描述用户空间的Service组件，Binder实体对应的Service在用户控件的本地Binder(BBinder)地址
    binder_uintptr_t cookie;

     // 异步事务处理，单独讲解
    unsigned has_async_transaction:1;
    struct list_head async_todo;
    // 表示该Binder实体对象能否接收含有该文件描述符的进程间通信数据。当一个进程向
    //另一个进程发送数据中包含文件描述符时，Binder会在目标进程中打开一个相同的文件
    //故设为accept_fds为0 可以防止源进程在目标进程中打开文件
    unsigned accept_fds:1;
     // 处理Binder请求的线程最低优先级
    unsigned min_priority:8;

};
```

这里说下binder\_proc和binder\_node关系：

> 可以将binder\_proc理解为一个进程，而将binder\_noder理解为一个Service。binder\_proc里面有一个红黑树，用来保存所有在它所描述的进程里面创建Service。而每一个Service在Binder驱动里面都有一个binder\_node来描述。

##### 3、异步事物处理

> 异步事务处理，目的在于为同步交互让路，避免长时间阻塞发送送端 异步事务定义：(相对于同步事务)单向进程间通信要求，即不需要等待应答的进程间通信请求 Binder驱动程序认为异步事务的优先级低于同步事务，则在同一时刻，一个Binder实体对象至多只有一个异步事物会得到处理。而同步事务则无此限制。 Binder将事务保存在一个线程binder\_thread的todo队列中，表示由该线程来处理该事务。每一个事务都关联Binder实体对象(union target)，表示该事务的目标处理对象，表示要求该Binder实体对象对应的Service组件在制定线程中处理该事务，而如果Binder发现一个事务时异步事务，则会将其保存在目标Binder对象的async\_todo的异步事务中等待处理

### 六、结构体binder\_ref

**binder\_ref** 代表的是Binder的引用对象，每一个Clinet组件在Binder驱动中都有一个Binder引用对象，用来描述它在内核中的状态。

###### 1、代码位置

位置在 [Linux的binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199100&objectType=1&isNewArticle=undefined) 281行

###### 2、代码注释

```
struct binder_ref {
    /* Lookups needed: */
    /*   node + proc => ref (transaction) */
    /*   desc + proc => ref (transaction, inc/dec ref) */
    /*   node => refs + procs (proc exit) */
        //debug 调试用的
        int debug_id;
     
        /** binder_proc中使用红黑树(对应两个rb_root变量) 来存储器内部所有引用对象，
         *下面的rb_node则是红黑树中的节点
         */
        //Binder引用的宿主进程
        struct binder_proc *proc;
        //对应 refs_by_desc，以句柄desc索引  关联到binder_proc->refs_by_desc红黑树 
        struct rb_node rb_node_desc;
         //对应refs_by_node，以Binder实体对象地址作为关键字关联到binder_proc->refs_by_node红黑树
        struct rb_node rb_node_node;

        /** Client通过Binder访问Service时，仅需指定一个句柄，Binder通过该desc找到对应的binder_ref，
         *  再根据该binder_ref中的node变量得到binder_node(实体对象)，进而找到对应的Service组件
         */
        // 对应Binder实体对象中(hlist_head) refs引用对象队列中的一个节点
        struct hlist_node node_entry;
        // 引用对象所指向的Binder实体对象
        struct binder_node *node;
        // Binder引用的句柄值，Binder驱动为binder驱动引用分配一个唯一的int型整数（进程范围内唯一）
        // ，通过该值可以在binder_proc->refs_by_desc中找到Binder引用，进而可以找到Binder引用对应的Binder实体
        uint32_t desc;

        // 强引用 计数
        int strong;
        // 弱引用 计数
        int weak;
      
        //  表示Service组件接受到死亡通知
        struct binder_ref_death *death;
};
```

### 七、结构体binder\_ref\_death

**binder\_ref\_death** 一个死亡通知的结构体 我们知道Client组件无法控制它所引用的Service组件的生命周期，由于Service组件所在的进程可能意外崩溃。Client进程需要能够在它所引用的Service组件死亡时获的通知，进而进行响应。则Client进程就需要向Binder驱动注册一个用来接收死亡通知的对象地址(这里的cookie)

##### 1、代码位置

位置在 [Linux的binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199100&objectType=1&isNewArticle=undefined) 276行

##### 2、代码注释

```
struct binder_ref_death {
         //标志该通知具体的死亡类型
        struct binder_work work;   
         // 保存负责接收死亡通知的对象地址
        binder_uintptr_t cookie;
};
```

这里随带说一下Binder驱动向Client进程发送死亡通知的情况：

-   1、Binder驱动检测到Service组件死亡时，会找到对应Serivce实体对象(binder\_node)，再通过refs变量找到引用它的所有Client进程(binder\_ref)，再通过death变量找到Client进程向Binder注册的死亡通知接收地址；Binder将死亡通知binder\_ref\_death封装成工作项，添加到Client进程to\_do队列中等待处理。这种情况binder\_work类型为BINDER\_WORK\_DEAD\_BINDER
-   2、Client进程向Binder驱动注册一个死亡接收通知时，如果它所引用的Service组件已经死亡，Binder会立即发送通知给Client进程。这种情况binder\_work类型为BINDER\_WORK\_DEAD\_BINDER
-   3、当Client进程向Binder驱动注销一个死亡通知时，也会发送通知，来响应注销结果
-   ①当Client注销时，Service组件还未死亡：Binder会找到之前Client注册的binder\_ref\_death，当binder\_work修改为BINDER\_CLEAR\_NOTIFICATION，并将通知按上述步骤添加到Client的to\_do队列中
-   @当Client注销时，Service已经死亡，Binder同理将binder\_work修改为WORK\_DEAD\_BINDER\_AND\_CLEAR，然后添加到todo中

### 八、结构体binder\_state

**binder\_state** 代表着binder设备文件的状态

##### 1、代码位置

位置在 [/frameworks/native/cmds/servicemanager/binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Fcmds%2Fservicemanager%2Fbinder.c&objectId=1199100&objectType=1&isNewArticle=undefined) 89行

##### 2、代码注释

```
struct binder_state
{
    //打开 /dev/binder之后得到的文件描述符
    int fd;
    //mmap将设备文件映射到本地进程的地址空间，映射后的到地址空间中，映射后得到的地址空间地址，及大小。
    void *mapped;
    // 分配内存的大小，默认是128K
    size_t mapsize;
};
```

### 九、结构体binder\_buffer

**binder\_buffer** 内核缓冲区，用以在进程间传递数据。binder驱动程序管理这个内存映射地址空间方法，即管理buffer~（buffer+buffer\_size）这段地址空间的，这个地址空间被划分为一段一段来管理，每一段是结构体struct binder\_buffer来描述。每一个binder\_buffer通过其成员entry从低到高地址连入到struct binder\_proc中的buffers表示链表中去

##### 1、代码位置

位置在 [Linux的binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199100&objectType=1&isNewArticle=undefined) 298行

##### 2、代码注释

```
struct binder_buffer {
        //entry对应内核缓冲区列表的buffers(内核缓冲区列表)
        struct list_head entry; /* free and allocated entries by address */
        //结合free,如果free=1，则rb_node对应free_buffers中一个节点(内核缓冲区)
        //如果free!=1，则对应allocated_buffers中的一个节点
        struct rb_node rb_node; /* free entry by size or allocated entry */
                /* by address */
        unsigned free:1;

         /**  Binder将事务数据保存到一个内核缓冲区(binder_transaction.buffer)，然后交由Binder
          * 实体对象(target_node) 处理，而target_node会将缓冲区的内容交给对应的Service组件
          * (proc) 来处理，Service组件处理完事务后，若allow_user_free=1，则请求Binder释放该
          * 内核缓冲区
          */
        unsigned allow_user_free:1;
         // 描述一个内核缓冲区正在交给那个事务transaction，用以中转请求和返回结果
        struct binder_transaction *transaction;
         // 描述该缓冲区正在被那个Binder实体对象使用
        struct binder_node *target_node;

        //表示事务时异步的；异步事务的内核缓冲区大小是受限的，这样可以保证事务可以优先放到缓冲区
        unsigned async_transaction:1;
         //调试专用
        unsigned debug_id:29;

        /** 
         *  存储通信数据，通信数据中有两种类型数据：普通数据与Binder对象
         *  在数据缓冲区最后，有一个偏移数组，记录数据缓冲区中每一个Binder
         *  对象在缓冲区的偏移地址
         */
        //  数据缓冲区大小
        size_t data_size;
        // 偏移数组的大小(其实也是偏移位置)
        size_t offsets_size;
        // 用以保存通信数据，数据缓冲区，大小可变
        uint8_t data[0];
        // 额外缓冲区大小
        size_t extra_buffers_size;
};
```

### 十、结构体binder\_transaction

\*\* binder\_transaction \*\* 描述Binder进程中通信过程，这个过程称为一个transaction(事务)，用以中转请求和返回结果，并保存接受和要发送的进程信息

##### 1、代码位置

位置在 [Linux的binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199100&objectType=1&isNewArticle=undefined) 383行

##### 2、代码注释

```
struct binder_transaction {
        //调试调用
        int debug_id;
        // 用来描述的处理的工作事项，这里会将type设置为BINDER_WORK_TRANSACTION，具体结构看binder_work
        struct binder_work work;

        /**   源线程 */
        // 源线程，即发起事务的线程
        struct binder_thread *from;
        // 源线程的优先级
        long    priority;
        //源 线程的用户 ID
        kuid_t  sender_euid;

        /**  目标线程*/
        //  目标进程:处理该事务的进程
        struct binder_proc *to_proc;
        // 目标线程：处理该事务的线程
        struct binder_thread *to_thread;
   
         // 表示另一个事务要依赖事务(不一定要在同一个线程中)
        struct binder_transaction *from_parent;
         // 目标线程下一个需要处理的事务
        struct binder_transaction *to_parent;

        // 标志事务是同步/异步；设为1表示同步事务，需要等待对方回复；设为0异步
        unsigned need_reply:1;
        /* unsigned is_dead:1; */   /* not used at the moment */
     
        /* 参考binder_buffer中解释，指向Binder为该事务分配内核缓冲区
         *  code与flag参见binder_transaction_data
         */
        struct binder_buffer *buffer;
        unsigned int    code;
        unsigned int    flags;

        /**  目标线程设置事务钱，Binder需要修改priority；修改前需要将线程原来的priority保存到
         *    saved_priority中，用以处理完事务回复到原来优先级
         *   优先级设置：目标现场处理事务时，优先级应不低于目标Serivce要求的线程优先级，也
         *   不低于源线程的优先级，故设为两者的较大值。
         */
        long    saved_priority;

};
```

### 十一、结构体binder\_transaction\_data

**binder\_transaction\_data** 代表进程间通信所传输的数据：Binder对象的传递时通过binder\_transaction\_data来实现的，即Binder对象实际是封装在binder\_transaction\_data结构体中

##### 1、代码位置

位置在 [Linux的binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199100&objectType=1&isNewArticle=undefined) 217行

##### 2、代码注释

```
struct binder_transaction_data {
         //target很重要，我下面重点介绍
         /* The first two are only used for bcTRANSACTION and brTRANSACTION,
         * identifying the target and contents of the transaction.
         */
         union {
               /* target descriptor of command transaction */
               __u32    handle;
               /* target descriptor of return transaction */
               binder_uintptr_t ptr;
         } target;
      
         //Binder实体带有的附加数据
         binder_uintptr_t   cookie; /* target object cookie */
         // code是一个命令，描述了请求Binder对象执行的操作，表示要对目标对象请求的命令代码
         __u32      code;       /* transaction command */

         /* General information about the transaction. */
         // 事务标志，详细看transaction_flag结构体
         __u32          flags;
         // 发起请求的进程PID
         pid_t      sender_pid;
         // 发起请求的进程UID
         uid_t      sender_euid;
         // data.buffer缓冲区的大小，data见最下面的定义；命令的真正要传输的数据就保存data.buffer缓冲区
         binder_size_t  data_size;  /* number of bytes of data */
          // data.offsets缓冲区的大小
         binder_size_t  offsets_size;   /* number of bytes of offsets */

         /* If this transaction is inline, the data immediately
         * follows here; otherwise, it ends with a pointer to
         * the data buffer.
         */
         union {
               struct {
                        /* transaction data */
                        binder_uintptr_t    buffer;
                        /* offsets from buffer to flat_binder_object structs */
                        binder_uintptr_t    offsets;
               } ptr;
               __u8 buf[8];
         } data;
};
```

这里重点说两个共用体target和data target

> 一个共用体target，当这个BINDER\_WRITE\_READ命令的目标对象是本地Binder的实体时，就用ptr来表示这个对象在本进程的地址，否则就使用handle来表示这个Binder的实体引用。

只有目标对象是Binder实体时，cookie成员变量才有意义，表示一些附加数据。 详解解释一下：传输的数据是一个复用数据联合体，对于BINDER类型，数据就是一个Binder本地对象。如果是HANDLE类型，这个数据就是远程Binder对象。很多人会说怎么区分本地Binder对象和远程Binder对象，主要是角度不同而已。本地对象还可以带有额外数据，保存在cookie中。

data

-   命令的真正要传输的数据就保存在data.buffer缓冲区中，前面的一成员变量都是一些用来描述数据的特征。data.buffer所表示的缓冲区数据分为两类，一类是普通数据，Binder驱动程序不关心，一类是Binder实体或者Binder引用，这需要Binder驱动程序介入处理。为什么?因为如果一个进程A传递了一个Binder实体或Binder引用给进程B，那么，Binder驱动程序就需要介入维护这个Binder实体或者引用引用计数。防止B进程还在使用这个Binder实体时，A却销毁这个实体，这样的话，B进程就会crash了。所以在传输数据时，如果数据中含有Binder实体和Binder引用，就需要告诉Binder驱动程序他们的具体位置，以便Binder驱动程序能够去维护它们。data.offsets的作用就在这里，它指定在data.buffer缓冲区中，所以Binder实体或者引用的偏移位置。
-   进程间传输的数据被称为Binder对象(Binder Object)，它是一个flat\_binder\_object。Binder对象的传递时通过binder\_transaction\_data来实现的，即Binder对象实际是封装在binder\_transaction\_data结构体中。

### 十二、结构体transaction\_flags

**transaction\_flags** 描述传输方式，比如同步或者异步等

##### 1、代码位置

位置在 [Linux的binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199100&objectType=1&isNewArticle=undefined) 210行

##### 2、代码注释

```
enum transaction_flags {
         //当前事务异步，不需要等待
        TF_ONE_WAY  =  0x01,    /* this is a one-way call: async, no return */
        // 包含内容是根对象
        TF_ROOT_OBJECT  =  0x04,    /* contents are the component's root object */
        // 表示data所描述的数据缓冲区内 同时一个4bit的状态码
        TF_STATUS_CODE  =  0x08,    /* contents are a 32-bit status code */
        // 允许数据中包含文件描述
        TF_ACCEPT_FDS   =  0x10,    /* allow replies with file descriptors */
};
```

### 十三、结构体flat\_binder\_object

**flat\_binder\_object** 描述进程中通信过程中传递的Binder实体/引用对象或文件描述符

##### 1、代码位置

位置在 [Linux的binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199100&objectType=1&isNewArticle=undefined) 68行

##### 2、代码注释

```
/*
 * This is the flattened representation of a Binder object for transfer
 * between processes.  The 'offsets' supplied as part of a binder transaction
 * contains offsets into the data where these structures occur.  The Binder
 * driver takes care of re-writing the structure type and data as it moves
 * between processes.
 */
struct flat_binder_object {
    struct binder_object_header hdr;
    __u32               flags;

    /* 8 bytes of data. */
    union {
        binder_uintptr_t    binder; /* local object */
        __u32           handle; /* remote object */
    };

    /* extra data associated with local object */
    binder_uintptr_t    cookie;
};
```

这个比较重要我们就一个一个来说，首先看下**struct binder\_object\_header hdr;** 里面涉及到一个结构体binder\_object\_header，这个结构体在[Linux的binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199100&objectType=1&isNewArticle=undefined) 57行，代码如下：

```
/**
 * struct binder_object_header - header shared by all binder metadata objects.
 * @type:   type of the object
 */
struct binder_object_header {
    __u32        type;
};
```

flat\_binder\_object通过type来区分描述类型，可选的描述类型如下： 代码在 [Linux的binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199100&objectType=1&isNewArticle=undefined) 30行

```
enum {
        //强类型Binder实体对象
    BINDER_TYPE_BINDER  = B_PACK_CHARS('s', 'b', '*', B_TYPE_LARGE),
        //弱类型Binder实体对象
    BINDER_TYPE_WEAK_BINDER = B_PACK_CHARS('w', 'b', '*', B_TYPE_LARGE),
         // 强类型引用对象
    BINDER_TYPE_HANDLE  = B_PACK_CHARS('s', 'h', '*', B_TYPE_LARGE),
         // 弱类型引用对象
    BINDER_TYPE_WEAK_HANDLE = B_PACK_CHARS('w', 'h', '*', B_TYPE_LARGE),
        // 文件描述符 
    BINDER_TYPE_FD      = B_PACK_CHARS('f', 'd', '*', B_TYPE_LARGE),
    BINDER_TYPE_FDA     = B_PACK_CHARS('f', 'd', 'a', B_TYPE_LARGE),
    BINDER_TYPE_PTR     = B_PACK_CHARS('p', 't', '*', B_TYPE_LARGE),
};
```

当描述实体对象时，cookie表示Binder实体对应的Service在用户空间的本地Binder(BBinder)地址，binder表示Binder实体对应的当描述引用对象时，handle表示该引用的句柄值。

PS:

> 关于BINDER\_TYPE\_FDA和BINDER\_TYPE\_PTR我也不是很清楚，有懂的兄弟，在下面留言，谢谢

### 十四、结构体binder\_write\_read

**binder\_write\_read** 描述进程间通信过程中所传输的数据

##### 1、代码位置

位置在 [Linux的binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199100&objectType=1&isNewArticle=undefined) 165行

##### 2、代码注释

```
/*
 * On 64-bit platforms where user code may run in 32-bits the driver must
 * translate the buffer (and local binder) addresses appropriately.
 */

struct binder_write_read {

        /** 输入数据 从用户控件传输到Binder驱动程序的数据
         *  数据协议代码为命令协议码，由binder_driver_command_protocol定义
         */
        // 写入的大小
        binder_size_t       write_size; /* bytes to write */
        // 记录了从缓冲区取了多少字节的数据
        binder_size_t       write_consumed; /* bytes consumed by driver */
        // 指向一个用户控件缓冲区的地址，里面的内容即为输入数据，大小由write_size指定
        binder_uintptr_t    write_buffer;
        
         /** 输出数据，从Binder驱动程序，返回给用户空间的数据
          *  数据协议代码为返回协议代码，由binder_driver_return_protocol定义
          */
        //读出的大小
        binder_size_t       read_size;  /* bytes to read */
        // read_buffer中读取的数据量
        binder_size_t       read_consumed;  /* bytes consumed by driver */
        // 指向一个用户缓冲区一个地址，里面保存输出的数据
        binder_uintptr_t    read_buffer;
};
```

里面涉及到两个协议，我们就在这里详细讲解下

###### 3、binder\_driver\_command\_protocol协议

代码在[Linux的binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199100&objectType=1&isNewArticle=undefined) 366行

```
enum binder_driver_command_protocol {
      
        /**  下面这两个命令数据类型为binder_transaction_data，是最常用到的 */
        /**一个Client进程请求目标进程执行某个事务时，会使用BC_TRANSACTION请求Binder驱
         *动程序将通信数据传递到Server目标进程
         * 使用者：Client进程   用处：传递数据
         */ 
        BC_TRANSACTION = _IOW('c', 0, struct binder_transaction_data),
         /** 当Server目标进程处理完事务后，会使用BC_REPLY请求Binder将结果返回给Client源进程
          * 使用者：Server进程  用处：返回数据
          */
        BC_REPLY = _IOW('c', 1, struct binder_transaction_data),


        /*
         * binder_transaction_data: the sent command.
         */
        //当前版本not support  在Linux中的binder.c就是这么写的
        BC_ACQUIRE_RESULT = _IOW('c', 2, __s32),

        /*
         * not currently supported
         * int:  0 if the last BR_ATTEMPT_ACQUIRE was not successful.
         * Else you have acquired a primary reference on the object.
         */

        // 数据类型为int类型，指向Binder内部一块内核缓冲区
        // 目标进程处理完源进程事务后，会使用BC_FREE_BUFFER来释放缓冲区
        BC_FREE_BUFFER = _IOW('c', 3, binder_uintptr_t),
    /*
     * void *: ptr to transaction data received on a read
     */

        //通信类型为int类型，表示binder_ref的句柄值handle
        // 增加弱引用数
        BC_INCREFS = _IOW('c', 4, __u32),
        // 减少弱引用数
        BC_DECREFS = _IOW('c', 7, __u32),
        // 增加强引用数
        BC_ACQUIRE = _IOW('c', 5, __u32),
        // 减少强引用数
        BC_RELEASE = _IOW('c', 6, __u32),
    
        /*
         * int: descriptor
         */
        /** Service进程完成增加强/弱引用的计数后，会使用这两个命令通知Binder */
        // 增加强引用计数后
        BC_INCREFS_DONE = _IOW('c', 8, struct binder_ptr_cookie),
        //增加弱引用计数后
        BC_ACQUIRE_DONE = _IOW('c', 9, struct binder_ptr_cookie),

        /*
         * void *: ptr to binder
         * void *: cookie for binder
         */
        //当前版本不支持 
        BC_ATTEMPT_ACQUIRE = _IOW('c', 10, struct binder_pri_desc),

        /*
         * not currently supported
         * int: priority
         * int: descriptor
         */
        // Binder驱动程序 请求进程注册一个线程到它的线程池中，新建立线程会使用
        //BC_REGISTER_LOOPER来通知Binder准备就绪
        BC_REGISTER_LOOPER = _IO('c', 11),

        /*
         * No parameters.
         * Register a spawned looper thread with the device.
         */
        //一个线程自己注册到Binder驱动后，会使用BC_ENTER_LOOPER通知Binder准备就绪
        BC_ENTER_LOOPER = _IO('c', 12),
        // 线程发送退出请求
        BC_EXIT_LOOPER = _IO('c', 13),

        /*
         * No parameters.
         * These two commands are sent as an application-level thread
         * enters and exits the binder loop, respectively.  They are
         * used so the binder can have an accurate count of the number
         * of looping threads it has available.
         */
        // 进程向Binder注册一个死亡通知
        BC_REQUEST_DEATH_NOTIFICATION = _IOW('c', 14,
                        struct binder_handle_cookie),
        /*
         * int: handle
         * void *: cookie
         */
         // 进程取消之前注册的死亡通知
        BC_CLEAR_DEATH_NOTIFICATION = _IOW('c', 15,
                        struct binder_handle_cookie),
        /*
         * int: handle
         * void *: cookie
         */
        // 数据指向死亡通知binder_ref_death的地址，进程获得Service组件的死亡通知，
        // 会使用该命令通知Binder其已经处理完死亡通知
        BC_DEAD_BINDER_DONE = _IOW('c', 16, binder_uintptr_t),
        /*
         * void *: cookie
         */

        BC_TRANSACTION_SG = _IOW('c', 17, struct binder_transaction_data_sg),
        BC_REPLY_SG = _IOW('c', 18, struct binder_transaction_data_sg),
        /*
         * binder_transaction_data_sg: the sent command.
         */
};
```

在上述枚举命令成员中，最重要的是BC\_TRANSACTION和BC\_REPLY命令，被作为发送操作的命令，其数据参数都是binder\_transaction\_data结构体。其中前者用于翻译和解析将要被处理的事务数据，而后者则是事务处理完成之后对返回"结果数据"的操作命令。

##### 4、binder\_driver\_return\_protocol协议

Binder驱动的响应（返回，BR\_）协议，定义了Binder命令的数据返回格式。

代码在[Linux的binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199100&objectType=1&isNewArticle=undefined) 278行

```
enum binder_driver_return_protocol {
         // Binder驱动程序处理进程发送的请求时，发生异常，在返回BR_ERROR通知该进程
         // 数据类型为int，表示错误代码
        BR_ERROR = _IOR('r', 0, __s32),
    /*
     * int: error code
     */

        // 表示通知进程成功处理了该事务
        BR_OK = _IO('r', 1),
    /* No parameters! */
        
        // 与上面的 BC_ 相对应
        //  Client进程向Server进程发送通信请求(BC_) ，Binder使用BR_TRANSACTION通知Server
        // 使用者 :   Binder驱动程序     用途：通知Server
        BR_TRANSACTION = _IOR('r', 2, struct binder_transaction_data),
        // Server处理完 请求 使用 BC_ 通知Binder，Binder使用BR_REPLY通知Client
        // 使用者：Binder驱动程序，用途：通知Client
        BR_REPLY = _IOR('r', 3, struct binder_transaction_data),
    /*
     * binder_transaction_data: the received command.
     */

         // 当前不支持 
        BR_ACQUIRE_RESULT = _IOR('r', 4, __s32),
    /*
     * not currently supported
     * int: 0 if the last bcATTEMPT_ACQUIRE was not successful.
     * Else the remote object has acquired a primary reference.
     */

        // Binder处理请求时，发现目标进程或目标线程已经死亡，通知源进程
        BR_DEAD_REPLY = _IO('r', 5),
    /*
     * The target of the last transaction (either a bcTRANSACTION or
     * a bcATTEMPT_ACQUIRE) is no longer with us.  No parameters.
     */

        // Binder 接收到BC_TRANSACATION或BC_REPLY时，会返回 BR_TRANSACTION_COMPLETE通知源进程命令已经接收
        BR_TRANSACTION_COMPLETE = _IO('r', 6),
    /*
     * No parameters... always refers to the last transaction requested
     * (including replies).  Note that this will be sent even for
     * asynchronous transactions.
     */

        // 增加弱引用计数
        BR_INCREFS = _IOR('r', 7, struct binder_ptr_cookie),
        // 增加强引用计数
        BR_ACQUIRE = _IOR('r', 8, struct binder_ptr_cookie),
        // 减少强引用计数
        BR_RELEASE = _IOR('r', 9, struct binder_ptr_cookie),
         // 减少弱引用计数
        BR_DECREFS = _IOR('r', 10, struct binder_ptr_cookie),
    /*
     * void *:  ptr to binder
     * void *: cookie for binder
     */

         //当前不支持
        BR_ATTEMPT_ACQUIRE = _IOR('r', 11, struct binder_pri_ptr_cookie),
    /*
     * not currently supported
     * int: priority
     * void *: ptr to binder
     * void *: cookie for binder
     */

        // Binder通过源进程执行了一个空操作，用以可以替换为BR_SPAWN_LOOPER
        BR_NOOP = _IO('r', 12),
    /*
     * No parameters.  Do nothing and examine the next command.  It exists
     * primarily so that we can replace it with a BR_SPAWN_LOOPER command.
     */

         // Binder发现没有足够的线程处理请求时，会返回BR_SPAWN_LOOPER请求增加新的新城到Binder线程池中
        BR_SPAWN_LOOPER = _IO('r', 13),
    /*
     * No parameters.  The driver has determined that a process has no
     * threads waiting to service incoming transactions.  When a process
     * receives this command, it must spawn a new service thread and
     * register it via bcENTER_LOOPER.
     */

         // 当前暂不支持
        BR_FINISHED = _IO('r', 14),
    /*
     * not currently supported
     * stop threadpool thread
     */


        /** Binder检测到Service组件死亡时，使用BR_DEAD_BINDER通知Client进程，Client请求
         *  注销之前的死亡通知，Binder完成后，返回BR_CLEAR_DEATH_NOTIFACTION_DONE
         */
        // 告诉发送方对象已经死亡
        BR_DEAD_BINDER = _IOR('r', 15, binder_uintptr_t),

    /*
     * void *: cookie
     */
        //清理死亡通知
        BR_CLEAR_DEATH_NOTIFICATION_DONE = _IOR('r', 16, binder_uintptr_t),
    /*
     * void *: cookie
     */

         // 发生异常，通知源进程
        BR_FAILED_REPLY = _IO('r', 17),
    /*
     * The the last transaction (either a bcTRANSACTION or
     * a bcATTEMPT_ACQUIRE) failed (e.g. out of memory).  No parameters.
     */
};
```

###### 5、Binder通信协议流程

单独看上面的协议可能很难理解，这里我们以一次Binder请求为过程来详细看一下Binder协议是如何通信的，就比较好理解了。这幅图的说明如下：

-   1 Binder是C/S架构的，通信过程牵涉到Client、Server以及Binder驱动三个角色
-   2 Client对于Server的请求以及Server对于Client的回复都需要通过Binder驱动来中转数据
-   3 BC\_XXX命令是进程发送给驱动命令
-   4 BR\_XXX命令是驱动发送给进程的命令
-   5 整个通信过程由Binder驱动控制

![](https://ask.qcloudimg.com/http-save/yehe-2957818/tc74i6vruj.png)

Binder通信过程.png

PS:这里补充说明一下，通过上面的Binder协议的说明，我们看到，Binder协议的通信过程中，不仅仅是发送请求和接收数据这些命令。同时包括了对于引用计数的管理和对于死亡通知的管理(告知一方，通讯的另外一方已经死亡)。这个功能的流程和上述的功能大致一致。

### 十五、结构体binder\_ptr\_cookie

**binder\_ptr\_cookie** 用来描述一个Binder实体对象或一个Service组件的死亡通知

##### 1、代码位置

位置在 [Linux的binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199100&objectType=1&isNewArticle=undefined) 257行

##### 2、代码注释

```
struct binder_ptr_cookie {
    binder_uintptr_t ptr;
    binder_uintptr_t cookie;
};
```

-   当描述Binder实体对象：ptr,cookie见binder\_node
-   当描述死亡通知：ptr指向一个Binder引用对象的句柄值，cookie指向接收死亡通知的对象地址

### 十六、总结

结构体就是这样的

![](https://ask.qcloudimg.com/http-save/yehe-2957818/et7yatmvgu.png)

image.png

如果以结构体为标的来看整个Binder传输过程则如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/jfi0l2cm8r.png)

结构体为标的.png

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2017.08.04 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除