

主要流程：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/465442dd05f4fc73cc72e05d3d44a3e5.png#pic_center)

#### 1\. 启动ServiceManager进程

`ServiceManager`是由`init(pid = 1)`进程启动的  
system/core/rootdir/init.rc

```cpp
on init
	...
	...
    # Start essential services.
    start servicemanager  //framework层使用
    start hwservicemanager  //hal层
    start vndservicemanager  //vendor内部
```

进程被创建后，进入`main`方法

#### 2\. 执行main方法

frameworks/native/cmds/servicemanager/main.cpp

```cpp
int main(int argc, char** argv) {
	...
    const char* driver = argc == 2 ? argv[1] : "/dev/binder";

    sp<ProcessState> ps = ProcessState::initWithDriver(driver); //创建ProcessState对象（进程唯一），并进行open、ioctl、mmap操作
    ps->setThreadPoolMaxThreadCount(0);  //初始线程数实际为0
    ps->setCallRestriction(ProcessState::CallRestriction::FATAL_IF_NOT_ONEWAY);

    sp<ServiceManager> manager = sp<ServiceManager>::make(std::make_unique<Access>());  //构造自己时构造Access对象，这个是用于权限检测
    //先注册自己作为服务
    if (!manager->addService("manager", manager, false /*allowIsolated*/, IServiceManager::DUMP_FLAG_PRIORITY_DEFAULT).isOk()) {
        LOG(ERROR) << "Could not self register servicemanager";
    }

    IPCThreadState::self()->setTheContextObject(manager); //保存ServiceManager作为BBinder对象到IPCThreadState实例中
    ps->becomeContextManager();  //向驱动注册自己成为全局唯一的ContextManager，全局只有一个ServiceManager

    sp<Looper> looper = Looper::prepare(false /*allowNonCallbacks*/); //获取一个Looper

    BinderCallback::setupTo(looper);  //将binder fd添加到Looper中监听，当驱动有事件时，回调handleEvent()处理
    ClientCallbackCallback::setupTo(looper, manager);  //这个是用于告知客户端当前服务端有多少个客户端绑定的回调监听

    while(true) {  //循环等待事件到来
        looper->pollAll(-1);  //阻塞等待event的到来，然后进行ioctl和驱动交互获取数据
    }

    // should not be reached
    return EXIT_FAILURE;
}
```

1.  创建ProcessState对象，通过这个对象打开驱动节点，检验Binder版本和进行内存映射等操作，
2.  设置线程池的初始大小为0，实际线程数并不是固定的，这个要根据实际任务决定是否新加线程到线程池（binder驱动中处理这个逻辑
3.  然后创建`ServiceManager`对象，并通过`addService`添加自己到服务列表中。
4.  保存ServiceManager作为BBinder对象到IPCThreadState实例中，并向Binder驱动注册自己成为全局唯一的ContextManager
5.  然后创建Looper，通过Looper监听binder驱动的消息，当驱动有消息时回调handleEvent()处理（然后真正读数据是ioctl）
6.  进入大循环阻塞等待事件到来

#### 3\. ProcessState::initWithDriver(driver)

frameworks/native/libs/binder/ProcessState.cpp

```cpp
sp<ProcessState> ProcessState::initWithDriver(const char* driver)
{
    return init(driver, true /*requireDefault*/);
}

sp<ProcessState> ProcessState::init(const char *driver, bool requireDefault)
{
	...
    [[clang::no_destroy]] static std::once_flag gProcessOnce;
    std::call_once(gProcessOnce, [&](){  //call_once单例模式确保每个进程只有一个ProcessState
        if (access(driver, R_OK) == -1) {  //测试下binder的节点是否可读
            ALOGE("Binder driver %s is unavailable. Using /dev/binder instead.", driver);
            driver = "/dev/binder";
        }
		...
        std::lock_guard<std::mutex> l(gProcessMutex);
        gProcess = sp<ProcessState>::make(driver);  //调用构造函数，构造ProcessState实例
    });
	...
    return gProcess;
}

#define BINDER_VM_SIZE ((1 * 1024 * 1024) - sysconf(_SC_PAGE_SIZE) * 2)  //ServiceManager申请用于binder通信的虚拟内存大小也是 1MB-8KB
#define DEFAULT_MAX_BINDER_THREADS 15   //最大工作线程数 15 + 1（本身）
#define DEFAULT_ENABLE_ONEWAY_SPAM_DETECTION 1  //开启异步垃圾通信检测
...
ProcessState::ProcessState(const char* driver)
      : mDriverName(String8(driver)),
        mDriverFD(-1),
        mVMStart(MAP_FAILED),
        mThreadCountLock(PTHREAD_MUTEX_INITIALIZER),
        mThreadCountDecrement(PTHREAD_COND_INITIALIZER),
        mExecutingThreadsCount(0),
        mWaitingForThreads(0),
        mMaxThreads(DEFAULT_MAX_BINDER_THREADS),
        mStarvationStartTimeMs(0),
        mForked(false),
        mThreadPoolStarted(false),
        mThreadPoolSeq(1),
        mCallRestriction(CallRestriction::NONE) {
    base::Result<int> opened = open_driver(driver);  //进行open、ioctl

    if (opened.ok()) {
        // mmap the binder, providing a chunk of virtual address space to receive transactions.
        mVMStart = mmap(nullptr, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE,
                        opened.value(), 0);
        if (mVMStart == MAP_FAILED) {
            close(opened.value());
            // *sigh*
            opened = base::Error()
                    << "Using " << driver << " failed: unable to mmap transaction memory."; //内存不足
            mDriverName.clear();
        }
    }
	...
}
```

构造方法里做的最主要的两件事就是通过`open_driver()`、`mmap()`

#### 3.1 open\_driver

```cpp
static base::Result<int> open_driver(const char* driver) {
    int fd = open(driver, O_RDWR | O_CLOEXEC);  //通过open打开binder节点，获取binder设备驱动的fd
	...
    int vers = 0;
    status_t result = ioctl(fd, BINDER_VERSION, &vers);  //通过ioctl和binder驱动通信，查询binder驱动的binder版本，binder驱动的版本要和用户空间的binder协议的版本保持匹配，不然无法工作
	...
    size_t maxThreads = DEFAULT_MAX_BINDER_THREADS;  //DEFAULT_MAX_BINDER_THREADS = 15
    result = ioctl(fd, BINDER_SET_MAX_THREADS, &maxThreads);  //通过ioctl告知binder驱动，用户进程支持的最大binder工作线程数，默认是15+1 = 16个(加上本身)
	...
    uint32_t enable = DEFAULT_ENABLE_ONEWAY_SPAM_DETECTION;
    result = ioctl(fd, BINDER_ENABLE_ONEWAY_SPAM_DETECTION, &enable);  //开启oneway方式垃圾请求攻击检测（类似于垃圾邮件攻击检测）
	...
    return fd;  //返回驱动的fd
}
```

`open_driver`主要做四件事：

1.  通过系统调用`open`打开设备节点，获取设备驱动fd
2.  通过系统调用`ioctl`获取驱动的binder版本
3.  通过系统调用`ioctl`告知驱动用户进程支持的最大线程数，(默认是15+1，SystemServer进程默认是31+1)
4.  通过系统调用`ioctl`设置垃圾oneway异步通信检测

#### 3.2 mmap

```cpp
		...
    if (opened.ok()) {
        // mmap the binder, providing a chunk of virtual address space to receive transactions.
        mVMStart = mmap(nullptr, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE,
                        opened.value(), 0);  //BINDER_VM_SIZE = 1MB-8KB
        if (mVMStart == MAP_FAILED) {
            close(opened.value());
            // *sigh*
            opened = base::Error()
                    << "Using " << driver << " failed: unable to mmap transaction memory."; //内存不足(没有满足需求的连续内存)
            mDriverName.clear();
        }
    }
        ...
```

`ProcessState`构造函数接着会通过系统调用`mmap`将当前进程的一块虚拟内存(内核分配的虚拟地址，这个地址是进程地址空间的)映射到内核空间，这个系统调用最终实现是binder驱动中的`binder_mmap()`, binder驱动会在内核也申请一块空间（内核空间），并指向一块物理地址，注意这个仅仅用在这个进程作为服务端时，接收来自binder的消息，这个过程没有发生IO的拷贝。

#### 4\. setTheContextObject

```cpp
void IPCThreadState::setTheContextObject(const sp<BBinder>& obj)
{
    the_context_object = obj;
}
```

保存ServiceManager作为BBinder对象到IPCThreadState实例中（这样处理其他进程获取SM时就不需要每次都创建BBinder的对象）

#### 5\. becomeContextManager

```cpp
bool ProcessState::becomeContextManager()
{
    AutoMutex _l(mLock);

    flat_binder_object obj {
        .flags = FLAT_BINDER_FLAG_TXN_SECURITY_CTX,
    };

    int result = ioctl(mDriverFD, BINDER_SET_CONTEXT_MGR_EXT, &obj);  //注册自己成为ContextManager
	...
    return result == 0;
}
```

向驱动注册自己成为全局唯一的SM

#### 6\. BinderCallback::setupTo(looper)

```cpp
class BinderCallback : public LooperCallback {  //继承于LooperCallback，当Looper通过epoll监听到对应fd有event时回调cb.handleEvent
public:
    static sp<BinderCallback> setupTo(const sp<Looper>& looper) {
        sp<BinderCallback> cb = sp<BinderCallback>::make();   //创建BinderCallback对象

        int binder_fd = -1;
        IPCThreadState::self()->setupPolling(&binder_fd);
        LOG_ALWAYS_FATAL_IF(binder_fd < 0, "Failed to setupPolling: %d", binder_fd);

        int ret = looper->addFd(binder_fd,  //添加binder驱动的fd到Looper的监听fd
                                Looper::POLL_CALLBACK,
                                Looper::EVENT_INPUT,
                                cb,  //传递自己作为callback对象，有event就回调自己的handleEvent方法
                                nullptr /*data*/);
        LOG_ALWAYS_FATAL_IF(ret != 1, "Failed to add binder FD to Looper");

        return cb;
    }

    int handleEvent(int /* fd */, int /* events */, void* /* data */) override {
        IPCThreadState::self()->handlePolledCommands();  //驱动有事件上报时，就去处理
        return 1;  // Continue receiving callbacks.
    }
};
```

将binder驱动的fd注册到Looper中监听，当驱动上报事件时回调`handleEvent`, 处理相关事务

#### 6.1 setupPolling

```cpp
status_t IPCThreadState::setupPolling(int* fd)
{
    if (mProcess->mDriverFD < 0) {
        return -EBADF;
    }

    mOut.writeInt32(BC_ENTER_LOOPER);
    flushCommands();
    *fd = mProcess->mDriverFD;
    return 0;
}

void IPCThreadState::flushCommands()
{
    if (mProcess->mDriverFD < 0)
        return;
    talkWithDriver(false);  //通过ioctl 通知驱动将当前线程加入Looper状态
	...
}
```

通过ioctl通知驱动将当前线程加入Looper状态