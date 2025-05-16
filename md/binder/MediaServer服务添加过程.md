
```
int main(int argc __unused, char **argv __unused)  
{  
    OtherSystemServiceLoopRun();  
    signal(SIGPIPE, SIG_IGN);  
  
    sp<ProcessState> proc(ProcessState::self());  
    sp<IServiceManager> sm(defaultServiceManager());  
    ALOGI("ServiceManager: %p", sm.get());  
    AIcu_initializeIcuOrDie();  
    MediaPlayerService::instantiate();  
    ResourceManagerService::instantiate();  
    registerExtensions();  
    ProcessState::self()->startThreadPool();  
    IPCThreadState::self()->joinThreadPool();  
}
```

```
ProcessState::ProcessState(size_t mmap_size)  
    : mDriverFD(open_driver())  
    , mVMStart(MAP_FAILED)  
    , mThreadCountLock(PTHREAD_MUTEX_INITIALIZER)  
    , mThreadCountDecrement(PTHREAD_COND_INITIALIZER)  
    , mExecutingThreadsCount(0)  
    , mMaxThreads(DEFAULT_MAX_BINDER_THREADS)  
    , mStarvationStartTimeMs(0)  
    , mManagesContexts(false)  
    , mBinderContextCheckFunc(nullptr)  
    , mBinderContextUserData(nullptr)  
    , mThreadPoolStarted(false)  
    , mSpawnThreadOnStart(true)  
    , mThreadPoolSeq(1)  
    , mMmapSize(mmap_size)  
    , mCallRestriction(CallRestriction::NONE)  
{  
    if (mDriverFD >= 0) {  
        // mmap the binder, providing a chunk of virtual address space to receive transactions.  
        mVMStart = mmap(nullptr, mMmapSize, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);  
        if (mVMStart == MAP_FAILED) {  
            // *sigh*  
            ALOGE("Mmapping /dev/hwbinder failed: %s\n", strerror(errno));  
            close(mDriverFD);  
            mDriverFD = -1;  
        }  
    }  
    else {  
        ALOGE("Binder driver could not be opened.  Terminating.");  
    }  
  
    for(int i=0; i < MAX_CONTEXT; i++)  
    {  
        mSystemContextMgrHandle[i].binder = nullptr;  
        mSystemContextMgrHandle[i].refs = nullptr;  
    }  
}
```

> defaultServiceManager() = new BpServiceManager(new BpBinder(0))


``` 
sp<IServiceManager> defaultServiceManager()  
{  
    if (gDefaultServiceManager != nullptr) return gDefaultServiceManager;  
  
    {  
        AutoMutex _l(gDefaultServiceManagerLock);  
        while (gDefaultServiceManager == nullptr) {  
            gDefaultServiceManager = interface_cast<IServiceManager>(  
                ProcessState::self()->getContextObject(nullptr));  
            if (gDefaultServiceManager == nullptr)  
                sleep(1);  
        }  
    }  
  
    return gDefaultServiceManager;  
}
```

```
sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle )  // handle = 0  
{  
    sp<IBinder> result;  
  
    AutoMutex _l(mLock);  
  
    handle_entry* e = lookupHandleLocked(handle);  
  
    if (e != nullptr) {  
        // We need to create a new BpBinder if there isn't currently one, OR we  
        // are unable to acquire a weak reference on this current one.  See comment        // in getWeakProxyForHandle() for more info about this.        IBinder* b = e->binder;  
        if (b == nullptr || !e->refs->attemptIncWeak(this)) {  
            if (handle == 0 ||  handle >= INIT_SYSTEM_CONTEXT_MGR_HANDLE) {  
                // Special case for context manager...  
                // The context manager is the only object for which we create                // a BpBinder proxy without already holding a reference.                // Perform a dummy transaction to ensure the context manager                // is registered before we create the first local reference                // to it (which will occur when creating the BpBinder).                // If a local reference is created for the BpBinder when the                // context manager is not present, the driver will fail to                // provide a reference to the context manager, but the                // driver API does not return status.                //                // Note that this is not race-free if the context manager                // dies while this code runs.                //                // TODO: add a driver API to wait for context manager, or  
                // stop special casing handle 0 for context manager and add  
                // a driver API to get a handle to the context manager with                // proper reference counting.  
                Parcel data;  
                status_t status = IPCThreadState::self()->transact(  
                        handle, IBinder::PING_TRANSACTION, data, nullptr, 0);  
                if (status == DEAD_OBJECT)  
                   return nullptr;  
            }  
  
            b = BpBinder::create(handle);  
            e->binder = b;  
            if (b) e->refs = b->getWeakRefs();  
            result = b;  
        } else {  
            // This little bit of nastyness is to allow us to add a primary  
            // reference to the remote proxy when this team doesn't have one            // but another team is sending the handle to us.            result.force_set(b);  
            e->refs->decWeak(this);  
        }  
    }  
  
    return result;  
}
```

```
template<typename INTERFACE>  
inline sp<INTERFACE> interface_cast(const sp<IBinder>& obj)  
{  
    return INTERFACE::asInterface(obj);  
}
```

```
// ----------------------------------------------------------------------  
  // 申明
#define DECLARE_META_INTERFACE(INTERFACE)                               \  
public:                                                                 \  
    static const ::android::String16 descriptor;                        \ 
    // asInterface                                                      \ 
    static ::android::sp<I##INTERFACE> asInterface(                     \  
            const ::android::sp<::android::IBinder>& obj);              \  
    virtual const ::android::String16& getInterfaceDescriptor() const;  \  
    I##INTERFACE();                                                     \  
    virtual ~I##INTERFACE();                                            \  
    static bool setDefaultImpl(std::unique_ptr<I##INTERFACE> impl);     \  
    static const std::unique_ptr<I##INTERFACE>& getDefaultImpl();       \  
private:                                                                \  
    static std::unique_ptr<I##INTERFACE> default_impl;                  \  
public:                                                                 \  
  
  // 实现
#define IMPLEMENT_META_INTERFACE(INTERFACE, NAME)                       \  
    const ::android::String16 I##INTERFACE::descriptor(NAME);           \  
    const ::android::String16&                                          \  
            I##INTERFACE::getInterfaceDescriptor() const {              \  
        return I##INTERFACE::descriptor;                                \  
    }                                                                   \  
    ::android::sp<I##INTERFACE> I##INTERFACE::asInterface(              \  
            const ::android::sp<::android::IBinder>& obj)               \  
    {                                                                   \  
        ::android::sp<I##INTERFACE> intr;                               \  
        if (obj != nullptr) {                                           \  
            intr = static_cast<I##INTERFACE*>(                          \  
                obj->queryLocalInterface(                               \  
                        I##INTERFACE::descriptor).get());               \  
            if (intr == nullptr) {                                      \  
                intr = new Bp##INTERFACE(obj);                          \  
            }                                                           \  
        }                                                               \  
        return intr;                                                    \  
    }                                                                   \  
    std::unique_ptr<I##INTERFACE> I##INTERFACE::default_impl;           \  
    bool I##INTERFACE::setDefaultImpl(std::unique_ptr<I##INTERFACE> impl)\  
    {                                                                   \  
        if (!I##INTERFACE::default_impl && impl) {                      \  
            I##INTERFACE::default_impl = std::move(impl);               \  
            return true;                                                \  
        }                                                               \  
        return false;                                                   \  
    }                                                                   \  
    const std::unique_ptr<I##INTERFACE>& I##INTERFACE::getDefaultImpl() \  
    {                                                                   \  
        return I##INTERFACE::default_impl;                              \  
    }                                                                   \  
    I##INTERFACE::I##INTERFACE() { }                                    \  
    I##INTERFACE::~I##INTERFACE() { }                                   \  
  
  
#define CHECK_INTERFACE(interface, data, reply)                         \  
    do {                                                                \  
      if (!(data).checkInterface(this)) { return PERMISSION_DENIED; }   \  
    } while (false)                                                     \  
  
  
// ----------------------------------------------------------------------
```

```
IMPLEMENT_META_INTERFACE(ServiceManager, "android.os.IServiceManager");
```


> class BpServiceManager : public BpInterface<IServiceManager>
```
virtual status_t addService(const String16& name, const sp<IBinder>& service,  
                            bool allowIsolated, int dumpsysPriority) {  
    Parcel data, reply;  
    data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());  
    data.writeString16(name);  
    data.writeStrongBinder(service);  
    data.writeInt32(allowIsolated ? 1 : 0);  
    data.writeInt32(dumpsysPriority);  
    status_t err = remote()->transact(ADD_SERVICE_TRANSACTION, data, &reply);  
    return err == NO_ERROR ? reply.readExceptionCode() : err;  
}
```