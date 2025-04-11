
```
virtual void onZygoteInit()  
{  
    sp<ProcessState> proc = ProcessState::self();  
    ALOGV("App process: starting thread pool.\n");  
    proc->startThreadPool();  
}
```

```
ProcessState::ProcessState(const char *driver)  
    : mDriverName(String8(driver))  // 初始化驱动程序名称  
    , mDriverFD(open_driver(driver))  // 打开Binder驱动并获取文件描述符  
    , mVMStart(MAP_FAILED)  // 初始化内存映射起始地址为失败状态  
    , mThreadCountLock(PTHREAD_MUTEX_INITIALIZER)  // 初始化线程计数器锁  
    , mThreadCountDecrement(PTHREAD_COND_INITIALIZER)  // 初始化线程计数器条件变量  
    , mExecutingThreadsCount(0)  // 初始化当前执行线程数为0  
    , mMaxThreads(DEFAULT_MAX_BINDER_THREADS)  // 设置默认的最大Binder线程数  
    , mStarvationStartTimeMs(0)  // 初始化线程饥饿开始时间为0  
    , mManagesContexts(false)  // 初始化上下文管理标志为false  
    , mBinderContextCheckFunc(nullptr)  // 初始化Binder上下文检查函数为空  
    , mBinderContextUserData(nullptr)  // 初始化Binder上下文用户数据为空  
    , mThreadPoolStarted(false)  // 初始化线程池启动标志为false  
    , mThreadPoolSeq(1)  // 初始化线程池序列号为1  
    , mCallRestriction(CallRestriction::NONE)  // 初始化调用限制为无
```
