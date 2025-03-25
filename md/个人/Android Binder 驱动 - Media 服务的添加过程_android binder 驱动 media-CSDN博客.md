相关文章链接：

[1\. Android Framework - 学习启动篇](https://blog.csdn.net/z240336124/article/details/98117999)  
[2\. Android Binder 驱动 - Media 服务的添加过程](https://blog.csdn.net/z240336124/article/details/100145625)  
[3\. Android Binder 驱动 - 启动 ServiceManager 进程](https://blog.csdn.net/z240336124/article/details/100171210)  
[4\. Android Binder 驱动 - 内核驱动层源码分析](https://blog.csdn.net/z240336124/article/details/100576575)  
[5\. Android Binder 驱动 - 从驱动层来分析服务的添加过程](https://blog.csdn.net/z240336124/article/details/100631395)  
[6\. Android Binder 驱动 - 从 Java 层来跟踪服务的查找过程](https://blog.csdn.net/z240336124/article/details/101051776)

相关源码文件：

```
/frameworks/av/media/mediaserver/main_mediaserver.cpp
/frameworks/native/libs/binder/ProcessState.cpp
/frameworks/av/media/libmediaplayerservice/MediaPlayerService.cpp
/frameworks/native/libs/binder/IServiceManager.cpp
/frameworks/native/include/binder/IInterface.h
/frameworks/native/libs/binder/IServiceManager.cpp
/frameworks/native/libs/binder/Binder.cpp
/frameworks/native/libs/binder/IPCThreadState.cpp
```

Media 进程是由 [init 进程](https://blog.csdn.net/z240336124/article/details/98472607)通过解析 init.rc 文件而创建的。

```
service media /system/bin/mediaserver 
    class main
    user media
    group audio camera inet net_bt net_bt_admin net_bw_acct drmrpc mediadrm     
    ioprio rt 4
```

可执行文件对应的源码在 [/frameworks/av/media/mediaserver/main\_mediaserver.cpp](http://androidxref.com/6.0.0_r1/xref/frameworks/av/media/mediaserver/main_mediaserver.cpp) 中，我们找到 main 方法，注意这里分析源码我们主要关心 Binder 驱动：

```
int main(int argc __unused, char** argv)
{
    ...
    InitializeIcuOrDie();
    // ProcessState 是一个单例， sp<ProcessState> 就看成是 ProcessState
    sp<ProcessState> proc(ProcessState::self());
    ...
    // 注册 MediaPlayerService 服务
    MediaPlayerService::instantiate();
    ...
    // 启动 Binder 线程池
    ProcessState::self()->startThreadPool();
    // 当前线程加入到线程池
    IPCThreadState::self()->joinThreadPool();
 }

sp<ProcessState> ProcessState::self()
{
    Mutex::Autolock _l(gProcessMutex);
    if (gProcess != NULL) {
        return gProcess;
    }

    // 实例化 ProcessState , 跳转到构造函数
    gProcess = new ProcessState;
    return gProcess;
}

ProcessState::ProcessState()
    : mDriverFD(open_driver()) // 注意这里还有个 open_driver() 函数，打开 Binder 驱动 
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
        // 采用内存映射函数 mmap，给 binder 分配一块虚拟地址空间
        // BINDER_VM_SIZE = 1M - 8k
        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
        if (mVMStart == MAP_FAILED) {
            // 没有足够空间分配给 /dev/binder, 则关闭驱动
            close(mDriverFD); 
            mDriverFD = -1;
        }
    }
}

static int open_driver()
{
    // 打开 /dev/binder 设备，建立与内核的 Binder 驱动的交互通道
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

        // 通过 ioctl 设置 binder 驱动，能支持的最大线程数
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

binder 驱动是整个 Android FrameWork 中比较晦涩难懂的一个部分，早些年我也曾尝试着去看这些代码，但那时的第一感觉是这辈子都不可能看懂了。因此刚开始我们不需要去细扣，还有就是 linux 基础知识很重要。这里我们只需要了解 open 是打开驱动、mmap 是映射驱动、ioctl 是操作驱动、close 是关闭驱动，分别对应驱动层的 binder\_open、binder\_mmap、binder\_ioctl 和 binder\_colse 方法就可以了。接着看下 MediaPlayerService::instantiate() ：

```
void MediaPlayerService::instantiate() {
    // 注册服务 defaultServiceManager() =  new BpServiceManager(new BpBinder(0)) 
    defaultServiceManager()->addService(String16("media.player"), new MediaPlayerService());
}

sp<IServiceManager> defaultServiceManager(){
    // 其实也是一个单例，但刚开始肯定是 == NULL
    if(gDefaultServiceManager!=NULL)return gDefaultServiceManager;
    // 加一把自动锁
    AutoMutex _l(gDefaultServiceManagerLock);
    while(gDefaultServiceManager==NULL){
        // ProcessState::self() 上面介绍过了，这里主要看 getContextObject(NULL)
        gDefaultServiceManager=interface_cast<IServiceManager> (
        ProcessState::self()->getContextObject(NULL));
        // 有可能 ServiceManager 进程还没来的及初始化，适当等待
        if(gDefaultServiceManager==NULL){
            sleep(1);
        }
    }
    return gDefaultServiceManager;
}

sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/) {
    return getStrongProxyForHandle(0);
}

sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle) {
    sp<IBinder> result;
    AutoMutex _l (mLock);
    handle_entry * e = lookupHandleLocked(handle);
    if (e != NULL) {
        //  handle_entry 是从缓存里面获取的，刚开始 e -> binder 是空
        IBinder * b = e -> binder;
        if (b == NULL || !e -> refs -> attemptIncWeak(this)) {
            // 这里 handle 是 0 ，PING_TRANSACTION 看 ServiceManager 进程能不能访问
            if (handle == 0) {
                Parcel data;
                status_t status = IPCThreadState::self () -> transact(
                        0, IBinder::PING_TRANSACTION, data, NULL, 0);
                if (status == DEAD_OBJECT) {
                        return NULL;
                }
            }
            // new BpBinder(0);
            b = new BpBinder(handle);
            e -> binder = b;
            if (b) e -> refs = b -> getWeakRefs();
            result = b;
        } else {
            result.force_set(b);
            e -> refs -> decWeak(this);
        }
    }
    return result;
}
```

ProcessState::self()->getContextObject(NULL) 返回的是 new BpBinder(0) , handle 值为 0 代表是 ServiceManager 进程，关于 ServiceManager 的启动过程后面会分析到。我们接着往下看 interface\_cast(new BpBinder(0))

```
template<typename INTERFACE>
inline sp<INTERFACE> interface_cast(const sp<IBinder>&obj) {
    return INTERFACE::asInterface (obj);
}

DECLARE_META_INTERFACE(ServiceManager);

define DECLARE_META_INTERFACE(INTERFACE)                               
static const android::String16 descriptor;                          
static android::sp<I##INTERFACE> asInterface(const android::sp<android::IBinder>& obj);                  
virtual const android::String16& getInterfaceDescriptor() const;
I##INTERFACE();                                                     
virtual ~I##INTERFACE();

IMPLEMENT_META_INTERFACE(ServiceManager, "android.os.IServiceManager");

#define IMPLEMENT_META_INTERFACE(INTERFACE, NAME)
const android::String16 I##INTERFACE::descriptor(NAME);
const android::String16&I##INTERFACE::getInterfaceDescriptor() const {
    return I##INTERFACE::descriptor;
}
android::sp<I##INTERFACE> I##INTERFACE::asInterface(const android::sp<android::IBinder>& obj) {
    android::sp<I##INTERFACE> intr;
    if (obj != NULL) {
        intr = static_cast<I##INTERFACE*>(obj->queryLocalInterface(I##INTERFACE::descriptor).get());
        if (intr == NULL) {
            intr = new Bp##INTERFACE(obj);
        }
    }
    return intr;
}
I##INTERFACE::I##INTERFACE() { }
I##INTERFACE::~I##INTERFACE() { }
```

上面主要是宏定义的展开和替换，刚开始看我也是一头雾水，在 IServiceManager 中找了半天也没找到 asInterface 方法， 因此这里最终返回的是 BpServiceManager

```
virtual status_t addService(const String16& name, const sp<IBinder>& service, bool allowIsolated) {
    // Parcel 是内存共享读写
    Parcel data, reply; 
    // 写入头信息 "android.os.IServiceManager" ，ServiceManager 进程收到请求后会判断
    data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());   
    // name为 "media.player"
    data.writeString16(name);    
    // MediaPlayerService 对象 
    data.writeStrongBinder(service);
    // allowIsolated = false
    data.writeInt32(allowIsolated ? 1 : 0); 
    // remote() 指向的是 BpBinder 对象
    status_t err = remote()->transact(ADD_SERVICE_TRANSACTION, data, &reply);
    return err == NO_ERROR ? reply.readExceptionCode() : err;
}

status_t Parcel::writeStrongBinder(const sp<IBinder>& val)
{
    return flatten_binder(ProcessState::self(), val, this);
}

status_t flatten_binder(const sp<ProcessState>& /*proc*/,
    const sp<IBinder>& binder, Parcel* out)
{
    flat_binder_object obj;

    obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    if (binder != NULL) {
        // 本地 Binder 不为空，返回的是 this ，也就是 MediaPlayerService 对象
        IBinder *local = binder->localBinder(); 
        if (!local) {
            BpBinder *proxy = binder->remoteBinder();
            const int32_t handle = proxy ? proxy->handle() : 0;
            obj.type = BINDER_TYPE_HANDLE;
            obj.binder = 0;
            obj.handle = handle;
            obj.cookie = 0;
        } else { 
            // 进入该分支，type 是 BINDER_TYPE_BINDER 
            obj.type = BINDER_TYPE_BINDER;
            obj.binder = reinterpret_cast<uintptr_t>(local->getWeakRefs());
            // cookie 传的是强引用也就是 MediaPlayerService 对象的地址
            obj.cookie = reinterpret_cast<uintptr_t>(local);
        }
    } else {
        ...
    }
    
    return finish_flatten_binder(binder, obj, out);
}

inline static status_t finish_flatten_binder(
    const sp<IBinder>& , const flat_binder_object& flat, Parcel* out)
{
    return out->writeObject(flat, false);
}

status_t BpBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    if (mAlive) {
        // code=ADD_SERVICE_TRANSACTION 交给了 IPCThreadState::self()
        status_t status = IPCThreadState::self()->transact(
            mHandle, code, data, reply, flags);
        if (status == DEAD_OBJECT) mAlive = 0;
        return status;
    }
    return DEAD_OBJECT;
}

/* TLS是指 Thread local storage (线程本地储存空间)，每个线程都拥有自己的TLS，并且是私有空间，线程之间不会共享，
从线程本地存储空间中获得保存在其中的IPCThreadState对象，
与 Java 中的 ThreadLocal 类似。*/
IPCThreadState* IPCThreadState::self()
{
    if (gHaveTLS) {
restart:
        const pthread_key_t k = gTLS;
        IPCThreadState* st = (IPCThreadState*) pthread_getspecific(k);
        if (st) return st;
        // 初始IPCThreadState 
        return new IPCThreadState; 
    }

    pthread_mutex_lock(&gTLSMutex);
    // 首次进入 gHaveTLS 为 false
    if (!gHaveTLS) {
        // 创建线程的TLS
        if (pthread_key_create(&gTLS, threadDestructor) != 0) {
            pthread_mutex_unlock(&gTLSMutex);
            return NULL;
        }
        gHaveTLS = true;
    }
    pthread_mutex_unlock(&gTLSMutex);
    goto restart;
}

IPCThreadState::IPCThreadState()
    : mProcess(ProcessState::self()),
      mMyThreadId(gettid()),
      mStrictModePolicy(0),
      mLastTransactionBinderFlags(0)
{
    // 通过 pthread_setspecific/pthread_getspecific  来设置获取 IPCThreadState
    pthread_setspecific(gTLS, this);
    clearCaller();
    // mIn 用来接收来自 Binder 设备的数据
    mIn.setDataCapacity(256);
    // mOut用来存储发往 Binder 设备的数据
    mOut.setDataCapacity(256);
}
```

上面有一个非常重要的结构体 flat\_binder\_object，参数分别有 type、 binder、handle 和 cookie ，这个是 binder 驱动处理的精髓之一。transact 最终交给了 IPCThreadState::self() 不同的线程有且只有一个单独的 IPCThreadState 对象。

```
status_t IPCThreadState::transact(int32_t handle,
                                  uint32_t code, const Parcel& data,
                                  Parcel* reply, uint32_t flags)
{
    ....
    if (err == NO_ERROR) { 
        // 传输数据 
        err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, NULL);
    }
    ...
    if ((flags & TF_ONE_WAY) == 0) {
        if (reply) {
            //等待响应 
            err = waitForResponse(reply);
        } else {
            Parcel fakeReply;
            err = waitForResponse(&fakeReply);
        }
    } else {
        // oneway，则不需要等待 reply 的场景
        err = waitForResponse(NULL, NULL);
    }
    return err;
}

status_t IPCThreadState::writeTransactionData(int32_t cmd, uint32_t binderFlags,
    int32_t handle, uint32_t code, const Parcel& data, status_t* statusBuffer)
{
    binder_transaction_data tr;
    tr.target.ptr = 0;
    // handle = 0 ，代表是要转发给 ServiceManager 进程
    tr.target.handle = handle;
    // code = ADD_SERVICE_TRANSACTION 动作是添加服务
    tr.code = code;         
    // binderFlags = 0   
    tr.flags = binderFlags;    
    tr.cookie = 0;
    tr.sender_pid = 0;
    tr.sender_euid = 0;

    // data 为记录 Media 服务信息的 Parcel 对象
    const status_t err = data.errorCheck();
    if (err == NO_ERROR) {
        // mDataSize， 这个里面有多少数据
        tr.data_size = data.ipcDataSize();  
        // mData 
        tr.data.ptr.buffer = data.ipcData(); 
        // mObjectsSize
        tr.offsets_size = data.ipcObjectsCount()*sizeof(binder_size_t); 
        // mObjects
        tr.data.ptr.offsets = data.ipcObjects(); 
    } else if (statusBuffer) {
        ...
    } else {
        return (mLastError = err);
    }
    // cmd = BC_TRANSACTION ,  驱动找到 ServiceManager 后会像客户端返回一个 BR_TRANSACTION_COMPLETE 表示通信请求已被接受，然后 Client 进入等待
    mOut.writeInt32(cmd);        
    // 写入 binder_transaction_data 数据
    mOut.write(&tr, sizeof(tr)); 
    return NO_ERROR;
}

status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    int32_t cmd;
    int32_t err;
    while (1) {
        if ((err=talkWithDriver()) < NO_ERROR) break; 
        ...
        if (mIn.dataAvail() == 0) continue;

        cmd = mIn.readInt32();
        switch (cmd) {
            case BR_TRANSACTION_COMPLETE: ...
            case BR_DEAD_REPLY: ...
            case BR_FAILED_REPLY: ...
            case BR_ACQUIRE_RESULT: ...
            case BR_REPLY: ...
                goto finish;

            default:
                err = executeCommand(cmd); 
                if (err != NO_ERROR) goto finish;
                break;
        }
    }
    ...
    return err;
}

status_t IPCThreadState::talkWithDriver(bool doReceive)
{
    ...
    binder_write_read bwr;
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();
    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;

    bwr.write_size = outAvail;
    bwr.write_buffer = (uintptr_t)mOut.data();

    if (doReceive && needRead) {
        //接收数据缓冲区信息的填充。如果以后收到数据，就直接填在mIn中了。
        bwr.read_size = mIn.dataCapacity();
        bwr.read_buffer = (uintptr_t)mIn.data();
    } else {
        bwr.read_size = 0;
        bwr.read_buffer = 0;
    }
    //当读缓冲和写缓冲都为空，则直接返回
    if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;

    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    status_t err;
    do {
        //通过ioctl不停的读写操作，跟Binder Driver进行通信
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
        ...
    } while (err == -EINTR); //当被中断，则继续执行
    ...
    return err;
}
```

至此添加服务的过程已全部分析完毕，最后是交给了binder 驱动的 ioctl 方法，至于数据发到哪里去了其内部实现又是怎样的，这里暂时不做讲解。具体的数据有 interfaceToken（远程服务的名称）、handle（远程服务的句柄）、cookie（本地服务的地址），有两个结构体 flat\_binder\_object 和 binder\_write\_read。  
视频地址：https://pan.baidu.com/s/1j\_wgzITcgABVbThvO0VBPA  
视频密码：jj4b