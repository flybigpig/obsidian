> 获取ServiceManager  
> hongxi.zhu 2023-7-1

**以SurfaceFlinger为例，分析客户端进程如何获取ServiceManager代理服务对象**

### 主要流程

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/ed2f76e8adbdcad15e9e993a51177edd.png#pic_center)

#### SurfaceFlinger中获取SM服务

frameworks/native/services/surfaceflinger/main\_surfaceflinger.cpp

```cpp
    // publish surface flinger
    sp<IServiceManager> sm(defaultServiceManager());
    sm->addService(String16(SurfaceFlinger::getServiceName()), flinger, false,
                   IServiceManager::DUMP_FLAG_PRIORITY_CRITICAL | IServiceManager::DUMP_FLAG_PROTO);
```

在`SurfaceFlinger`的`main`函数里，首先是获取`IServiceManager`的对象，其实也就是`BpServiceManager`对象，然后再通过`BpServiceManager->addService()`注册相关SurfaceFlinger相关的两个服务。那么看下这个IServiceManager的对象是怎么获取的, 从上面可以看到是从`defaultServiceManager()`方法获取到的ServiceManager。

#### defaultServiceManager()

frameworks/native/libs/binder/IServiceManager.cpp

```cpp
using AidlServiceManager = android::os::IServiceManager;
...
[[clang::no_destroy]] static std::once_flag gSmOnce;
[[clang::no_destroy]] static sp<IServiceManager> gDefaultServiceManager;

sp<IServiceManager> defaultServiceManager()
{
    std::call_once(gSmOnce, []() {
        sp<AidlServiceManager> sm = nullptr;
        while (sm == nullptr) {
            sm = interface_cast<AidlServiceManager>(ProcessState::self()->getContextObject(nullptr));
            if (sm == nullptr) {
                ALOGE("Waiting 1s on context object on %s.", ProcessState::self()->getDriverName().c_str());
                sleep(1);  //循环等待SM就绪
            }
        }

        gDefaultServiceManager = sp<ServiceManagerShim>::make(sm);
    });

    return gDefaultServiceManager;
}
```

这个方法是`libbinder`中的`IServiceManager.cpp`中实现，这是一个`call_once`实现的单例方法，通过`interface_cast<AidlServiceManager>(ProcessState::self()->getContextObject(nullptr))`获取一个`IServiceManager`对象, 然后再构造一个`IServiceManager`子类`ServiceManagerShim`对象返回给客户端使用（ServiceManagerShim是Google AIDL化改造ServiceManager后新的一个中间类，负责SM具体的功能）

#### ProcessState::self()

frameworks/native/libs/binder/ProcessState.cpp

```cpp
sp<ProcessState> ProcessState::self()
{
    return init(kDefaultDriver, false /*requireDefault*/); //kDefaultDriver = "/dev/binder";
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
        gProcess = sp<ProcessState>::make(driver);  //构造ProcessState实例
    });
	...
    return gProcess;
}
```

`ProcessState::self()`主要是通过单例的方式获取一个进程独有的`ProcessState`对象, 那首次构造时需要走构造方法。

```cpp
#define BINDER_VM_SIZE ((1 * 1024 * 1024) - sysconf(_SC_PAGE_SIZE) * 2)
#define DEFAULT_MAX_BINDER_THREADS 15
#define DEFAULT_ENABLE_ONEWAY_SPAM_DETECTION 1
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
    base::Result<int> opened = open_driver(driver);

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

#### open\_driver

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

#### mmap

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

回归上面的分析，我们当前是注册服务，从上面我们分析了`ProcessState::self()`, 它获取了`ProcessState`对象，然后接着调用它的`getContextObject()`方法

#### getContextObject(nullptr)

```cpp
sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/)
{
    sp<IBinder> context = getStrongProxyForHandle(0);  //BpServiceManager的handle是特殊的（handle = 0）
	...
    return context;
}
```

#### getStrongProxyForHandle(0)

```cpp
sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);

    handle_entry* e = lookupHandleLocked(handle);  //查找handle对应的handle_entry

    if (e != nullptr) {
        IBinder* b = e->binder;  //handle_entry会保存handle对应的BpBinder对象
        if (b == nullptr || !e->refs->attemptIncWeak(this)) {
            if (handle == 0) {  //ServiceManager很特殊，固定的handle = 0
                IPCThreadState* ipc = IPCThreadState::self();  //创建IPCThreadState实例，线程独有，用于发起真正的跨进程通信动作。
                CallRestriction originalCallRestriction = ipc->getCallRestriction();  //权限检查
                ipc->setCallRestriction(CallRestriction::NONE);

                Parcel data;
                status_t status = ipc->transact(
                        0, IBinder::PING_TRANSACTION, data, nullptr, 0);  //向handle(0)发起PING_TRANSACTION事务，检测Binder是否已经正常工作，对端的BBdiner里会处理这个请求并返回NO_ERROR

                ipc->setCallRestriction(originalCallRestriction);

                if (status == DEAD_OBJECT)
                   return nullptr;
            }

            sp<BpBinder> b = BpBinder::PrivateAccessor::create(handle);  //根据handle创建BpBinder
            e->binder = b.get();
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
			...
        }
    }

    return result;
}
```

查找handle(0)对应的BpBinder对象，如果是首次调用就，先通过`IPCThreadState::transact`向binder驱动发起往对端进程一个Ping的事务，看看往SM端的binder链路是否正常工作，然后创建handle(0)对应的BpBinder(0), 并保存到handle\_entry中，实际上就是返回new BpBinder(0)

#### lookupHandleLocked(0)

```cpp
ProcessState::handle_entry* ProcessState::lookupHandleLocked(int32_t handle)
{
    const size_t N=mHandleToObject.size();
    if (N <= (size_t)handle) {  //如果handle大于mHandleToObject.size，就创建handle+1-N个新的handle_entry并插入，第一次进入时，N = 0，handle = 0,所以handle = 0肯定是第一个元素，从这里也看出，除了第一个元素外，每个进程里Bpbinder对应的handle值不一定相同
        handle_entry e;
        e.binder = nullptr;  //首次创建时binder为nullptr
        e.refs = nullptr;
        status_t err = mHandleToObject.insertAt(e, N, handle+1-N);
        if (err < NO_ERROR) return nullptr;
    }
    return &mHandleToObject.editItemAt(handle);  //返回对应元素的地址
}
```

`handle_entry`是在用户进程中保存BpBinder映射关系的结构体，`mHandleToObject`通过`handle`作为下标可以查询到对应的`BpBinder`  
所以`ProcessState::self()->getContextObject(nullptr)`返回的实际上就是`new BpBInder(0)`是个`BpBInder对象`, 通过`IInterface`的`interface_cast`转换为`android::os::IServiceManager`的接口实现对象`BpServiceManager`。