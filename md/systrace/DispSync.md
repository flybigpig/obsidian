#### 文章目录

+   [参考资料](#_2)
+   [一. DispSync](#_DispSync_7)
+   [二. DispSync初始化](#_DispSync_13)
+   +   [2.1 SurfaceFlinger](#21_SurfaceFlinger_14)
    +   [2.2 DispSync创建](#22_DispSync_27)
    +   [2.3 SurfaceFlinger::SurfaceFlinger](#23_SurfaceFlingerSurfaceFlinger_42)
    +   [2.4 DispSync.init](#24_DispSyncinit_52)
    +   +   [2.4.1 DispSyncThread.threadLoop](#241_DispSyncThreadthreadLoop_83)
+   [三. enableVysncLocked后续](#_enableVysncLocked_120)
+   +   [3.1 DispSync.addEventListener](#31_DispSyncaddEventListener_122)
    +   [3.2 DispSync.DispSyncThread.addEventListener](#32_DispSyncDispSyncThreadaddEventListener_130)
+   [四. setPeriod](#_setPeriod_173)
+   +   [4.1 SurfaceFlinger.resyncToHardwareVsync](#41_SurfaceFlingerresyncToHardwareVsync_184)
    +   [4.2 DispSync.setPeriod](#42_DispSyncsetPeriod_219)
+   [五. 硬件Vsync的开关控制](#_Vsync_232)
+   +   [5.1 EventControlThread的启动](#51_EventControlThread_236)
    +   +   [5.1.1 EventControlThread.threadMain](#511_EventControlThreadthreadMain_260)
        +   [5.1.2 EventControlThread初始化](#512_EventControlThread_281)
    +   [5.2 EventControlThread.setVsyncEnabled](#52_EventControlThreadsetVsyncEnabled_293)
    +   [5.3 SurfaceFlinger.setVsyncEnabled](#53_SurfaceFlingersetVsyncEnabled_305)
    +   [5.4 HWComposer.setVsyncEnabled](#54_HWComposersetVsyncEnabled_319)
+   [六. 硬件Vsync信号更新](#_Vsync_350)
+   +   [6.1 HWC初始化](#61_HWC_353)
    +   +   [6.1.1 ComposerHal.cpp:Composer](#611_ComposerHalcppComposer_366)
        +   [6.1.2 HWComposer创建](#612_HWComposer_406)
    +   [6.2 注册回调HWComposer.registerCallback](#62_HWComposerregisterCallback_412)
    +   [6.3 Vsync信号更新](#63_Vsync_443)
    +   +   [6.3.1 ComposerCallbackBridge.onVsync](#631_ComposerCallbackBridgeonVsync_444)
        +   [6.3.2 SurfaceFlinger.onVsyncReceived](#632_SurfaceFlingeronVsyncReceived_454)
        +   [6.3.3 DispSync.addResyncSample](#633_DispSyncaddResyncSample_487)
        +   [6.3.4 DispSync.updateModelLocked](#634_DispSyncupdateModelLocked_542)
        +   [6.3.5 DispSync.DispSyncThread.updateModel](#635_DispSyncDispSyncThreadupdateModel_625)
+   [七. SW Vsync更新](#_SW_Vsync_643)
+   +   [7.1 DispSync.DispSyncThread.threadLoop](#71_DispSyncDispSyncThreadthreadLoop_645)
    +   [7.2 DispSync.DispSyncThread.computeNextEventTimeLocked](#72_DispSyncDispSyncThreadcomputeNextEventTimeLocked_727)
    +   +   [7.2.1 DispSync.DispSyncThread.computeListenerNextEventTimeLocked](#721_DispSyncDispSyncThreadcomputeListenerNextEventTimeLocked_747)
    +   [7.3 DispSync.DispSyncThread.gatherCallbackInvocationsLocked](#73_DispSyncDispSyncThreadgatherCallbackInvocationsLocked_809)

## 参考资料

1.  [Android SurfaceFlinger SW Vsync模型](https://www.jianshu.com/p/d3e4b1805c92)
2.  [DispSync](http://echuang54.blogspot.com/2015/01/dispsync.html)
3.  [DispSync详解](http://tinylab.org/android-dispsync/#dispsync-%E6%98%AF%E4%BB%80%E4%B9%88)

## 一. DispSync

DispSyncThread, 软件产生vsync的线程, 也控制硬件VSync信号同步。

接上一篇，SF EventThread在显示屏准备完毕后，会调用enableVSyncLocked，最终是在DispSync的mEventListeners中添加了一个EventListener。  
我们先看DispSync线程的创建过程。

## 二. DispSync初始化

### 2.1 SurfaceFlinger

```c++
    SurfaceFlinger::SurfaceFligner(SurfaceFlinger::SkipInitializationTag)
            :   BnSurfaceComposer(),
                mTransactionFlags(0),
                ......
                mPrimaryDispSync("PrimaryDispSync"),
                mPrimaryHWVsyncEnabled(false),
                ......
                {}
```

在SurfaceFlinger初始化的时候创建的。

### 2.2 DispSync创建

```c++
DispSync::DispSync(const char* name)
      : mName(name), mRefreshSkipCount(0), mThread(new DispSyncThread(name)) {}

explicit DispSyncThread(const char* name)
        : mName(name),
        mStop(false),
        mPeriod(0), // 注意这里的mPeriod初始化为0
        mPhase(0),
        mReferenceTime(0),
        mWakeupLatency(0),
        mFrameNumber(0) {}
```

### 2.3 SurfaceFlinger::SurfaceFlinger

```c++
SurfaceFlinger::SurfaceFlinger() : SurfaceFlinger(SkipInitialization) {
    ALOGI("SurfaceFlinger is starting");
    ......
    mPrimaryDispSync.init(SurfaceFlinger::hasSyncFramework,
            SurfaceFlinger::dispSyncPresentTimeOffset);
    ......
}
```

### 2.4 DispSync.init

```c++
void DispSync::init(bool hasSyncFramework, int64_t dispSyncPresentTimeOffset) {
    mIgnorePresentFences = !hasSyncFramework;
    mPresentTimeOffset = dispSyncPresentTimeOffset;
    // 线程改名为 DispSync，调整线程优先级
    mThread->run("DispSync", PRIORITY_URGENT_DISPLAY + PRIORITY_MORE_FAVORABLE);

    // set DispSync to SCHED_FIFO to minimize jitter
    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(mThread->getTid(), SCHED_FIFO, &param) != 0) {
        ALOGE("Couldn't set SCHED_FIFO for DispSyncThread");
    }

    reset();
    beginResync();

    if (kTraceDetailedInfo) {
        // If we're not getting present fences then the ZeroPhaseTracer
        // would prevent HW vsync event from ever being turned off.
        // Even if we're just ignoring the fences, the zero-phase tracing is
        // not needed because any time there is an event registered we will
        // turn on the HW vsync events.
        if (!mIgnorePresentFences && kEnableZeroPhaseTracer) {
            mZeroPhaseTracer = std::make_unique<ZeroPhaseTracer>();
            addEventListener("ZeroPhaseTracer", 0, mZeroPhaseTracer.get());
        }
    }
}
```

#### 2.4.1 DispSyncThread.threadLoop

```c++
    virtual bool threadLoop() {
        status_t err;
        // 获取开机到现在的时长
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);

        while (true) {
            Vector<CallbackInvocation> callbackInvocations;

            nsecs_t targetTime = 0;

            { // Scope for lock
                Mutex::Autolock lock(mMutex);

                if (kTraceDetailedInfo) {
                    ATRACE_INT64("DispSync:Frame", mFrameNumber);
                }
                ALOGV("[%s] Frame %" PRId64, mName, mFrameNumber);
                ++mFrameNumber;

                if (mStop) {
                    return false;
                }
                // 由于此时mPeriod为0，所以会进入该分支一直等待。
                if (mPeriod == 0) {
                    err = mCond.wait(mMutex);
                    if (err != NO_ERROR) {
                        ALOGE("error waiting for new events: %s (%d)", strerror(-err), err);
                        return false;
                    }
                    continue;
                }
                ......
    }
```

## 三. enableVysncLocked后续

SF EventThread在显示屏准备完毕后，会调用enableVSyncLocked

### 3.1 DispSync.addEventListener

```
status_t DispSync::addEventListener(const char* name, nsecs_t phase, Callback* callback) {
    Mutex::Autolock lock(mMutex);
    return mThread->addEventListener(name, phase, callback);
}
```

### 3.2 DispSync.DispSyncThread.addEventListener

```c++
    status_t addEventListener(const char* name, nsecs_t phase, DispSync::Callback* callback) {
        if (kTraceDetailedInfo) ATRACE_CALL();
        Mutex::Autolock lock(mMutex);

        for (size_t i = 0; i < mEventListeners.size(); i++) {
            if (mEventListeners[i].mCallback == callback) {
                return BAD_VALUE;
            }
        }

        EventListener listener;
        listener.mName = name;
        listener.mPhase = phase;
        listener.mCallback = callback;

        // We want to allow the firstmost future event to fire without
        // allowing any past events to fire
        listener.mLastEventTime = systemTime() - mPeriod / 2 + mPhase - mWakeupLatency;

        mEventListeners.push(listener);

        // threadLooper可以继续执行了
        mCond.signal();

        return NO_ERROR;
    }
```

注意这里还是运行在SurfaceFlinger主线程，在mCond.signal之后，DispSync线程就可以继续执行了。  
但是注意看：

```c++
                if (mPeriod == 0) {
                    err = mCond.wait(mMutex);
                    if (err != NO_ERROR) {
                        ALOGE("error waiting for new events: %s (%d)", strerror(-err), err);
                        return false;
                    }
                    continue;
                }
```

这里的continue意味着如果mPeriod为0，还是会一直等待。

## 四. setPeriod

这样我们就需要看mPeriod是什么时候被更改的。  
在SurfaceFlinger初始化Display后，会调用resyncToHardwareVsync跟硬件vsync进行同步。

```
initializeDisplays();
    flinger->onInitializeDisplays();
        setPowerModeInternal()
            resyncToHardwareVsync(true);
                repaintEverything();
```

### 4.1 SurfaceFlinger.resyncToHardwareVsync

```c++
void SurfaceFlinger::resyncToHardwareVsync(bool makeAvailable) {
    Mutex::Autolock _l(mHWVsyncLock);

    if (makeAvailable) {
        // mHWVsyncAvailable 表示 HW vsync 被 enable
        mHWVsyncAvailable = true;
    } else if (!mHWVsyncAvailable) {
        // Hardware vsync is not currently available, so abort the resync
        // attempt for now
        return;
    }

    //获得显示设备的刷新率，比如60HZ, 那么period就是16.6667ms,即每隔16.6667就会产生一个硬件vsync信号
    const auto& activeConfig = getBE().mHwc->getActiveConfig(HWC_DISPLAY_PRIMARY);
    const nsecs_t period = activeConfig->getVsyncPeriod();

    // 这里就是设置DispSync线程中的period
    mPrimaryDispSync.reset();
    // 4.2 设置period
    mPrimaryDispSync.setPeriod(period);

    //mPrimaryHWVsyncEnabled表示当前的硬件vsync是否enable,
    if (!mPrimaryHWVsyncEnabled) {
        mPrimaryDispSync.beginResync();
        // 如果硬件vsync没有enable,那么就通知EventControlThread去通知硬件enable VSYNC
        // 这个和DispSync的setVsyncEnabled是不一样的
        // 5.1 硬件Vsync控制
        mEventControlThread->setVsyncEnabled(true);
        mPrimaryHWVsyncEnabled = true;
    }
}
```

### 4.2 DispSync.setPeriod

```c++
void DispSync::setPeriod(nsecs_t period) {
    Mutex::Autolock lock(mMutex);
    mPeriod = period;
    mPhase = 0;
    mReferenceTime = 0;
    // Ignore recompute as mReferenceTime is zero.
    // mThread->updateModel(mPeriod, mPhase, mReferenceTime);
}
```

mPeriod表示具体的硬件产生vsync的时间间隔。这样，之后的DispSync线程中的threadLoop就可以继续执行了。

## 五. 硬件Vsync的开关控制

接上面 4.1，当设置DispSync的mPeriod之后，如果硬件Vsync开关是开启状态，则会通过EventControlThread打开HW Vsync  
我们先看看EventControlThread线程的启动，其启动在SurfaceFlinger的初始化，EventThread启动之后，显示屏初始化之前。

### 5.1 EventControlThread的启动

```c++
void SurfaceFlinger::init() {
    ......
    mEventControlThread = std::make_unique<impl::EventControlThread>(
            [this](bool enabled) { setVsyncEnabled(HWC_DISPLAY_PRIMARY, enabled); });
    ......
}

void SurfaceFlinger::setVsyncEnabled(int disp, int enabled) {
    ATRACE_CALL();
    Mutex::Autolock lock(mStateLock);
    getHwComposer().setVsyncEnabled(disp,
            enabled ? HWC2::Vsync::Enable : HWC2::Vsync::Disable);
}
```

这里初始化时传入了函数 setVsyncEnabled。

注意EventControlThread中线程的初始化是在成员变量中：

```c++
    // Must be last so that everything is initialized before the thread starts.
    std::thread mThread{&EventControlThread::threadMain, this};
```

所以先调用threadMain，后调用构造函数。

#### 5.1.1 EventControlThread.threadMain

```c++
// Unfortunately std::unique_lock gives warnings with -Wthread-safety
void EventControlThread::threadMain() NO_THREAD_SAFETY_ANALYSIS {
    auto keepRunning = true;
    auto currentVsyncEnabled = false;

    while (keepRunning) {
        // 5.3 此时currentVsyncEnabled为false
        mSetVSyncEnabled(currentVsyncEnabled);

        std::unique_lock<std::mutex> lock(mMutex);
        // 在这里等待
        mCondition.wait(lock, [this, currentVsyncEnabled, keepRunning]() NO_THREAD_SAFETY_ANALYSIS {
            return currentVsyncEnabled != mVsyncEnabled || keepRunning != mKeepRunning;
        });
        currentVsyncEnabled = mVsyncEnabled;
        keepRunning = mKeepRunning;
    }
}
```

#### 5.1.2 EventControlThread初始化

```c++
EventControlThread::EventControlThread(EventControlThread::SetVSyncEnabledFunction function)
      : mSetVSyncEnabled(function) {
    pthread_setname_np(mThread.native_handle(), "EventControlThread");

    pid_t tid = pthread_gettid_np(mThread.native_handle());
    setpriority(PRIO_PROCESS, tid, ANDROID_PRIORITY_URGENT_DISPLAY);
    set_sched_policy(tid, SP_FOREGROUND);
}
```

构造函数里面设置了线程名和优先级

### 5.2 EventControlThread.setVsyncEnabled

```c++
void EventControlThread::setVsyncEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mMutex);
    mVsyncEnabled = enabled;
    mCondition.notify_all();
}
```

mVsyncEnabled设置为true, 表明开启硬件Vsync.  
mCondition.notify\_all() 则通知EventControlThread线程继续执行，回到5.1.1的循环内。  
mSetVSyncEnabled是传入的函数SurfaceFlinger.setVsyncEnabled.

### 5.3 SurfaceFlinger.setVsyncEnabled

```c++
void SurfaceFlinger::setVsyncEnabled(int disp, int enabled) {
    ATRACE_CALL();
    Mutex::Autolock lock(mStateLock);
    getHwComposer().setVsyncEnabled(disp,
            enabled ? HWC2::Vsync::Enable : HWC2::Vsync::Disable);
}

HWComposer& getHwComposer() const { return *getBE().mHwc; }
SurfaceFlingerBE& getBE() { return mBE; }
```

这里的disp = HWC\_DISPLAY\_PRIMARY

### 5.4 HWComposer.setVsyncEnabled

```c++
void HWComposer::setVsyncEnabled(DisplayId displayId, HWC2::Vsync enabled) {
    RETURN_IF_INVALID_DISPLAY(displayId);
    auto& displayData = mDisplayData[displayId];

    if (displayData.isVirtual) {
        LOG_DISPLAY_ERROR(displayId, "Invalid operation on virtual display");
        return;
    }

    // NOTE: we use our own internal lock here because we have to call
    // into the HWC with the lock held, and we want to make sure
    // that even if HWC blocks (which it shouldn't), it won't
    // affect other threads.
    std::lock_guard lock(displayData.vsyncEnabledLock);
    if (enabled == displayData.vsyncEnabled) {
        return;
    }

    ATRACE_CALL();
    auto error = displayData.hwcDisplay->setVsyncEnabled(enabled);
    RETURN_IF_HWC_ERROR(error, displayId);

    displayData.vsyncEnabled = enabled;

    const auto tag = "HW_VSYNC_ON_" + to_string(displayId);
    ATRACE_INT(tag.c_str(), enabled == HWC2::Vsync::Enable ? 1 : 0);
}
```

## 六. 硬件Vsync信号更新

经过HWComposer使能硬件Vsync信号后，只要有硬件Vsync信号产生，就可回调 hook\_vsync函数。  
hook\_vsync函数在HWComposer的初始化的时候被注册的。

### 6.1 HWC初始化

```
void SurfaceFlinger::init() {
    ......
    // 获取硬件HWC
    getBE().mHwc.reset(
            new HWComposer(std::make_unique<Hwc2::impl::Composer>(getBE().mHwcServiceName)));
    // 注册回调
    getBE().mHwc->registerCallback(this, getBE().mComposerSequenceId);
    ......
}
```

这里这里先创建的是 Hwc2::impl::Composer,然后创建HWComposer

#### 6.1.1 ComposerHal.cpp:Composer

```c++
Composer::Composer(const std::string& serviceName)
    : mWriter(kWriterInitialSize),
      mIsUsingVrComposer(serviceName == std::string("vr"))
{
    mComposer = V2_1::IComposer::getService(serviceName);

    if (mComposer == nullptr) {
        LOG_ALWAYS_FATAL("failed to get hwcomposer service");
    }

    mComposer->createClient(
            [&](const auto& tmpError, const auto& tmpClient)
            {
                if (tmpError == Error::NONE) {
                    mClient = tmpClient;
                }
            });
    if (mClient == nullptr) {
        LOG_ALWAYS_FATAL("failed to create composer client");
    }

    // 2.2 support is optional
    sp<IComposer> composer_2_2 = IComposer::castFrom(mComposer);
    if (composer_2_2 != nullptr) {
        mClient_2_2 = IComposerClient::castFrom(mClient);
        LOG_ALWAYS_FATAL_IF(mClient_2_2 == nullptr, "IComposer 2.2 did not return IComposerClient 2.2");
    }

    if (mIsUsingVrComposer) {
        sp<IVrComposerClient> vrClient = IVrComposerClient::castFrom(mClient);
        if (vrClient == nullptr) {
            LOG_ALWAYS_FATAL("failed to create vr composer client");
        }
    }
}

```

获取composer服务。

#### 6.1.2 HWComposer创建

```c++
HWComposer::HWComposer(std::unique_ptr<android::Hwc2::Composer> composer)
      : mHwcDevice(std::make_unique<HWC2::Device>(std::move(composer))) {}
```

### 6.2 注册回调HWComposer.registerCallback

```c++
void HWComposer::registerCallback(HWC2::ComposerCallback* callback,
                                  int32_t sequenceId) {
    mHwcDevice->registerCallback(callback, sequenceId);
}

void Device::registerCallback(ComposerCallback* callback, int32_t sequenceId) {
    if (mRegisteredCallback) {
        ALOGW("Callback already registered. Ignored extra registration "
                "attempt.");
        return;
    }
    mRegisteredCallback = true;
    sp<ComposerCallbackBridge> callbackBridge(
            new ComposerCallbackBridge(callback, sequenceId));
    mComposer->registerCallback(callbackBridge);
}

void Composer::registerCallback(const sp<IComposerCallback>& callback)
{
    // mClient就是composer服务在SurfaceFlinger中的客户端
    auto ret = mClient->registerCallback(callback);
    if (!ret.isOk()) {
        ALOGE("failed to register IComposerCallback");
    }
}
```

ComposerCallbackBridge类就是实现onHotplug, onVsync等回调。  
当HWC硬件产生vsync信号时，就会回调onVsync方法。

### 6.3 Vsync信号更新

#### 6.3.1 ComposerCallbackBridge.onVsync

```c++
    Return<void> onVsync(Hwc2::Display display, int64_t timestamp) override
    {
        mCallback->onVsyncReceived(mSequenceId, display, timestamp);
        return Void();
    }
```

这里的mCallback就是SurfaceFlinger\[6.1\].

#### 6.3.2 SurfaceFlinger.onVsyncReceived

```c++
void SurfaceFlinger::onVsyncReceived(int32_t sequenceId,
        hwc2_display_t displayId, int64_t timestamp) {
    Mutex::Autolock lock(mStateLock);
    // Ignore any vsyncs from a previous hardware composer.
    if (sequenceId != getBE().mComposerSequenceId) {
        return;
    }

    int32_t type;
    // 按条件决定是否过滤，记录此次HWC接收到的硬件Vsync
    if (!getBE().mHwc->onVsync(displayId, timestamp, &type)) {
        return;
    }

    bool needsHwVsync = false;

    { // Scope for the lock
        Mutex::Autolock _l(mHWVsyncLock);
        // DISPLAY_PRIMARY为0，mPrimaryHWVsyncEnabled为true
        if (type == DisplayDevice::DISPLAY_PRIMARY && mPrimaryHWVsyncEnabled) {
            needsHwVsync = mPrimaryDispSync.addResyncSample(timestamp);
        }
    }

    if (needsHwVsync) {
        enableHardwareVsync();
    } else {
        disableHardwareVsync(false);
    }
}
```

#### 6.3.3 DispSync.addResyncSample

```c++
bool DispSync::addResyncSample(nsecs_t timestamp) {
    Mutex::Autolock lock(mMutex);

    ALOGV("[%s] addResyncSample(%" PRId64 ")", mName, ns2us(timestamp));

    // MAX_RESYNC_SAMPLES = 32，即最大只保存32次硬件vsync时间戳，用来计算SW vsync模型.
    // mNumResyncSamples 表示已经有多少个硬件vsync 样本了 ，最多记录MAX_RESYNC_SAMPLES
    //
    size_t idx = (mFirstResyncSample + mNumResyncSamples) % MAX_RESYNC_SAMPLES;
    mResyncSamples[idx] = timestamp;
    // 第一次收到Vsync信号，直接更新
    if (mNumResyncSamples == 0) {
        mPhase = 0;
        // 参考时间设置为第一个硬件vsync的时间戳
        mReferenceTime = timestamp;
        ALOGV("[%s] First resync sample: mPeriod = %" PRId64 ", mPhase = 0, "
              "mReferenceTime = %" PRId64,
              mName, ns2us(mPeriod), ns2us(mReferenceTime));
        // 6.3.5 通知更新DispSync线程收到Vsync信号
        mThread->updateModel(mPeriod, mPhase, mReferenceTime);
    }

   // 更新 mNumResyncSamples 或 mFirstResyncSample的值
    if (mNumResyncSamples < MAX_RESYNC_SAMPLES) {
        mNumResyncSamples++;
    } else {
        mFirstResyncSample = (mFirstResyncSample + 1) % MAX_RESYNC_SAMPLES;
    }
    // 6.3.4 开始计算更新SW vsync 模型
    updateModelLocked();

    if (mNumResyncSamplesSincePresent++ > MAX_RESYNC_SAMPLES_WITHOUT_PRESENT) {
        resetErrorLocked();
    }

    if (mIgnorePresentFences) {
        // If we don't have the sync framework we will never have
        // addPresentFence called.  This means we have no way to know whether
        // or not we're synchronized with the HW vsyncs, so we just request
        // that the HW vsync events be turned on whenever we need to generate
        // SW vsync events.
        return mThread->hasAnyEventListeners();
    }

    // Check against kErrorThreshold / 2 to add some hysteresis before having to
    // resync again
    bool modelLocked = mModelUpdated && mError < (kErrorThreshold / 2);
    ALOGV("[%s] addResyncSample returning %s", mName, modelLocked ? "locked" : "unlocked");
    return !modelLocked;
}
```

这里是收到硬件Vsync信号, 在SurfaceFlinger主线程执行，在经过误差更正后，通知DispSync线程处理分发事件。

#### 6.3.4 DispSync.updateModelLocked

这一步是计算模型参数如偏移、硬件Vsync更新间隔等。在分析前，我们先了解下几个重要参数的含义：

| 参数名 | 默认值 | 含义 |
| --- | --- | --- |
| mNumResyncSamples | \- | 当前保存的硬件Vsyc信号数量，最大值为32 |
| MIN\_RESYNC\_SAMPLES\_FOR\_UPDATE | 6 | 更新模型参数的最小硬件Vsync数量 |
| mPeriod | \- | 硬件刷新率，根据保存的Vsync去掉最大和最小求得的平均值 |
| mPhase | \- | 偏移时间，仅作为针对mPeriod的一个偏移 |
| mReferenceTime | 第一个硬件Vsync事件 | 每次计算sw vsync模型时的基准时间，以减少误差 |
| mRefreshSkipCount | 0 | 多少个vsync才进行刷新，可以通过这个人为的降低显示设备刷新率(软件刷新率) |

```c++
void DispSync::updateModelLocked() {
    ALOGV("[%s] updateModelLocked %zu", mName, mNumResyncSamples);
    // MIN_RESYNC_SAMPLES_FOR_UPDATE = 6, 也就是收到6次硬件Vsync之后，开始计算sw vsync模型
    if (mNumResyncSamples >= MIN_RESYNC_SAMPLES_FOR_UPDATE) {
        ALOGV("[%s] Computing...", mName);
        nsecs_t durationSum = 0;
        nsecs_t minDuration = INT64_MAX;
        nsecs_t maxDuration = 0;
        // 这里计算总时长，以及拿到最长和最短的硬件vsync间隔
        for (size_t i = 1; i < mNumResyncSamples; i++) {
            size_t idx = (mFirstResyncSample + i) % MAX_RESYNC_SAMPLES;
            size_t prev = (idx + MAX_RESYNC_SAMPLES - 1) % MAX_RESYNC_SAMPLES;
            nsecs_t duration = mResyncSamples[idx] - mResyncSamples[prev];
            durationSum += duration;
            minDuration = min(minDuration, duration);
            maxDuration = max(maxDuration, duration);
        }

        // 计算平均间隔，去掉一个最大和一个最小的间隔
        durationSum -= minDuration + maxDuration;
        mPeriod = durationSum / (mNumResyncSamples - 3);

        ALOGV("[%s] mPeriod = %" PRId64, mName, ns2us(mPeriod));

        double sampleAvgX = 0;
        double sampleAvgY = 0;
        double scale = 2.0 * M_PI / double(mPeriod);
        // 跳过第一个Vsync，因为第一个Vsync已经更新到DispSync中了。
        // mReferenceTime是第一个Vsync的时间
        for (size_t i = 1; i < mNumResyncSamples; i++) {
            size_t idx = (mFirstResyncSample + i) % MAX_RESYNC_SAMPLES;
            nsecs_t sample = mResyncSamples[idx] - mReferenceTime;
            double samplePhase = double(sample % mPeriod) * scale;
            sampleAvgX += cos(samplePhase);
            sampleAvgY += sin(samplePhase);
        }

        sampleAvgX /= double(mNumResyncSamples - 1);
        sampleAvgY /= double(mNumResyncSamples - 1);

        mPhase = nsecs_t(atan2(sampleAvgY, sampleAvgX) / scale);

        ALOGV("[%s] mPhase = %" PRId64, mName, ns2us(mPhase));
        // 如果偏移值是负值，绝对值超过了mPeroid的一半
        // 则调整偏移值为对应正值
        if (mPhase < -(mPeriod / 2)) {
            mPhase += mPeriod;
            ALOGV("[%s] Adjusting mPhase -> %" PRId64, mName, ns2us(mPhase));
        }

        if (kTraceDetailedInfo) {
            ATRACE_INT64("DispSync:Period", mPeriod);
            ATRACE_INT64("DispSync:Phase", mPhase + mPeriod / 2);
        }

        // mRefreshSkipCount表示多少个vsync才进行刷新，可以通过这个人为的降低显示设备刷新率(软件刷新率)
        mPeriod += mPeriod * mRefreshSkipCount;

        // 6.3.5 更新sw model. 这个方法会唤醒DispSync线程
        mThread->updateModel(mPeriod, mPhase, mReferenceTime);
        mModelUpdated = true;
    }
}
```

这里算偏移还用上了反三角函数。mPeriod的含义就是圆周长，最终算出来的 mPhase 就是弧BC的长度。  
也就是基于mPeriod的偏移值，如下图：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6f21b48c3a230ee45538dba7b731956a.png)

这个偏移值有什么用处呢？

#### 6.3.5 DispSync.DispSyncThread.updateModel

```c++
    void updateModel(nsecs_t period, nsecs_t phase, nsecs_t referenceTime) {
        if (kTraceDetailedInfo) ATRACE_CALL();
        Mutex::Autolock lock(mMutex);
        mPeriod = period;
        mPhase = phase;
        mReferenceTime = referenceTime;
        ALOGV("[%s] updateModel: mPeriod = %" PRId64 ", mPhase = %" PRId64
              " mReferenceTime = %" PRId64,
              mName, ns2us(mPeriod), ns2us(mPhase), ns2us(mReferenceTime));
        // 这里通知正在等待的DispSync线程开始执行
        mCond.signal();
    }
```

更新mPeriod和时间戳。  
mCond.signal 后转DispSyncThread线程\[2.4.1\]DispSyncThread.threadLoop继续执行

## 七. SW Vsync更新

硬件Vsync信号经过DispSync的简单加工，会将相应的值更新，然后唤醒DispSyncThread线程

### 7.1 DispSync.DispSyncThread.threadLoop

```c++
    virtual bool threadLoop() {
        status_t err;
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
        while (true) {
            Vector<CallbackInvocation> callbackInvocations;
            nsecs_t targetTime = 0;
            { // Scope for lock
                Mutex::Autolock lock(mMutex);
                if (kTraceDetailedInfo) {
                    ATRACE_INT64("DispSync:Frame", mFrameNumber);
                }
                ALOGV("[%s] Frame %" PRId64, mName, mFrameNumber);
                ++mFrameNumber;
                if (mStop) {
                    return false;
                }
                if (mPeriod == 0) {
                    err = mCond.wait(mMutex);
                    if (err != NO_ERROR) {
                        ALOGE("error waiting for new events: %s (%d)", strerror(-err), err);
                        return false;
                    }
                    continue;
                }

                // 7.2 计算下一个SW Vsync时间点
                targetTime = computeNextEventTimeLocked(now);
                bool isWakeup = false;
                // 如果计算出来的下一次vsync事件还没有到来，就等时间到了，才发送SW VSYNC信号
                // 可以看出 DispSyncThread的发送的vsync信号和真正硬件发生的vsync信号没有直接的关系，
                // 发送给app/sf的vsync信号都是由 DispSyncThread发送出去的.
                if (now < targetTime) {
                    if (kTraceDetailedInfo) ATRACE_NAME("DispSync waiting");

                    if (targetTime == INT64_MAX) {
                        ALOGV("[%s] Waiting forever", mName);
                        err = mCond.wait(mMutex);
                    } else {
                        ALOGV("[%s] Waiting until %" PRId64, mName, ns2us(targetTime));
                        err = mCond.waitRelative(mMutex, targetTime - now);
                    }
                    // 等待超时，主动醒来，发送SW Vsync信号
                    if (err == TIMED_OUT) {
                        isWakeup = true;
                    } else if (err != NO_ERROR) {
                        ALOGE("error waiting for next event: %s (%d)", strerror(-err), err);
                        return false;
                    }
                }

                now = systemTime(SYSTEM_TIME_MONOTONIC);

                // 计算wake up消耗的时间, 但是不能超过1.5 ms
                static const nsecs_t kMaxWakeupLatency = us2ns(1500);

                if (isWakeup) {
                    // 乍一看没明白为什么这么算。仔细想，每次wakeup时间是累加的，这个为了减小抖动？
                    mWakeupLatency = ((mWakeupLatency * 63) + (now - targetTime)) / 64;
                    mWakeupLatency = min(mWakeupLatency, kMaxWakeupLatency);
                    if (kTraceDetailedInfo) {
                        ATRACE_INT64("DispSync:WakeupLat", now - targetTime);
                        ATRACE_INT64("DispSync:AvgWakeupLat", mWakeupLatency);
                    }
                }
                // 7.3 搜集EventListener回调，一般就两个：SF和App EventThread
                // 并不是所有的wakeup都是等待了sw vsync的targetTime，如果SurfaceFlinger
                // 主线程收到硬件Vsync,也会唤醒此线程，此时isWakeup为false
                // 这里的callbackInvocations集合就为null，只有now>=targetTime才不为null
                callbackInvocations = gatherCallbackInvocationsLocked(now);
            }

            if (callbackInvocations.size() > 0) {
                fireCallbackInvocations(callbackInvocations);
            }
        }

        return false;
    }
```

### 7.2 DispSync.DispSyncThread.computeNextEventTimeLocked

```c++
    nsecs_t computeNextEventTimeLocked(nsecs_t now) {
        if (kTraceDetailedInfo) ATRACE_CALL();
        ALOGV("[%s] computeNextEventTimeLocked", mName);
        nsecs_t nextEventTime = INT64_MAX;
        // 对所有的EventListener进行分别计算，里面的mLastEventTime值不同
        // 找出一个最小的Vsync时间，即最近的时间
        for (size_t i = 0; i < mEventListeners.size(); i++) {
            nsecs_t t = computeListenerNextEventTimeLocked(mEventListeners[i], now);
            if (t < nextEventTime) {
                nextEventTime = t;
            }
        }
        ALOGV("[%s] nextEventTime = %" PRId64, mName, ns2us(nextEventTime));
        return nextEventTime;
    }
```

这里的EventListeners里面只有两个，一个是SF EventThread，另一个就是App EventThread.

#### 7.2.1 DispSync.DispSyncThread.computeListenerNextEventTimeLocked

```c++
    nsecs_t computeListenerNextEventTimeLocked(const EventListener& listener, nsecs_t baseTime) {

        // listener.mLasteEventTime就是上次SW VSync的时间点，mWakeupLatency就是上次线程醒来的耗时
        nsecs_t lastEventTime = listener.mLastEventTime + mWakeupLatency;
        // 一般baseTime也就是nowTime, 是大于lasterEventTime，除了第一次进入  
        if (baseTime < lastEventTime) {
            baseTime = lastEventTime;
        }
        // baseTime减去第一次硬件Vsync的时间，算duration时长
        baseTime -= mReferenceTime;
        // 偏移就是SW Vsync本身的偏移值加上各EventThread本身的偏移
        // sf 使用的是 SF_VSYNC_EVENT_PHASE_OFFSET_NS
        // APP使用的VSYNC_EVENT_PHASE_OFFSET_NS
        nsecs_t phase = mPhase + listener.mPhase;
        // baseTime也减去偏移
        baseTime -= phase;

        // baseTime小于0，只有第一次进入的时候才会发生。
        // 此时硬件Vsync已经发生了，所以设置baseTime为-mPeriod这样后面算的numPeriod为-1
        if (baseTime < 0) {
            baseTime = -mPeriod;
        }

        // 算出下一个SW Vsync的时间点
        // 先得到baseTime对应第几个sw Vsync，也就是现在时间点发送了多少个sw Vsync
        nsecs_t numPeriods = baseTime / mPeriod;

        // numberPeriods+1也就是下一个sw Vysnc，再加上偏移       
        nsecs_t t = (numPeriods + 1) * mPeriod + phase;
       
        t += mReferenceTime;
        ALOGV("[%s] Absolute t = %" PRId64, mName, ns2us(t));

        // 如果这个vsync距离上一个vsync时间小于3/5个mPeriod的话，
        // 为了避免连续的两个sw vsync, 那么这次sw vsync就放弃了，直接放到下一个周期里
        if (t - listener.mLastEventTime < (3 * mPeriod / 5)) {
            t += mPeriod;
        }

        // 算出来的时间减掉wakeup累积时间，最大1.5ms
        t -= mWakeupLatency;
        return t;
    }
```

如下图：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/62de346c38f09775dc04f9a1a8e98884.png)

看到这里就有一个疑问，sw vsync信号是在DispSyncThread收到第一个硬件Vsync更新sw model后就可以不依赖  
硬件Vsync信号了，后续可以自己产生。那为什么google没有在这里disable硬件Vsync呢，因为sw vsync还是有误差  
并不能与硬件Vsync完全保持一致，所以需要updateModelLocked持续消减误差。  
重新梳理一下完整流程：

1.  SurfaceFlinger主线程收到硬件Vsync
2.  DispSync.updateModelLocked及时更新sw model，并通知DispSyncThread线程
3.  DispSyncThread线程更新mPeriod，mPhase等参数通过computeNextEventTimeLocked计算新的targetTime
4.  继续等待直到新的targetTime，通知SF EventThread或者AppEventThread有sw vsync信号

我们知道SF EventThread和App EventThread是有间隔的，并不同步，这里是如何实现的呢？  
注意我们计算出来的targetTime是sf和app中最近的一次，那么继续看往下看。

### 7.3 DispSync.DispSyncThread.gatherCallbackInvocationsLocked

now是当前应该被触发的sw vsync时间点，可能是sf vsync也可能是app vsync。

```c++
    Vector<CallbackInvocation> gatherCallbackInvocationsLocked(nsecs_t now) {
        Vector<CallbackInvocation> callbackInvocations;

        // 这里为什么是拿一个vsync周期前的时间点呢？
        nsecs_t onePeriodAgo = now - mPeriod;

        // 计算各个EventListener(也就是sf 和app EventThread)的对应的下一次vsync时间.
        // 因为对于时间点now来讲，sf 和 app的下一次vsync时间可能尚未到来。
        for (size_t i = 0; i < mEventListeners.size(); i++) {
            nsecs_t t = computeListenerNextEventTimeLocked(mEventListeners[i], onePeriodAgo);
            // 如果下一次vsync时间尚未到达，这一次就不通知给对应EventListener
            if (t < now) {
                CallbackInvocation ci;
                ci.mCallback = mEventListeners[i].mCallback;
                ci.mEventTime = t;
                callbackInvocations.push(ci);
                // 记录本次sw Vsync时间点
                mEventListeners.editItemAt(i).mLastEventTime = t;
            }
        }

        return callbackInvocations;
    }
```

看完这个方法，其实不难理解，DispSyncThread中的targetTime是变化的值，有可能是app EventThread的下一次sw vsync时间，也可能是sf的。如下图：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/411dbcc6dd16ba814d0babdc71b3c420.png)

到这里，sw vsync的流程基本梳理完毕了。