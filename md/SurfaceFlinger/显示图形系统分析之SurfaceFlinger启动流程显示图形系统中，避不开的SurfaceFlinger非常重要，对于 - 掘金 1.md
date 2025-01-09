文章主要分析了 SurfaceFlinger 的启动流程，包括在 main 函数中的一系列设置和操作，如设置信号、线程数等，重点分析了 5/7/11 步骤。还介绍了 SurfaceFlinger 指针对象的创建和初始化，以及 init 函数中的多项工作，如初始化绘制引擎、硬件合成器等，并对显示设备进行初始化，最后启动了 SurfaceFlinger 的消息处理机制。

关联问题: SurfaceFlinger如何优化？ 能定制SurfaceFlinger吗？ SurfaceFlinger对性能影响？

「这是我参与2022首次更文挑战的第5天，活动详情查看：[2022首次更文挑战](https://juejin.cn/post/7052884569032392740 "https://juejin.cn/post/7052884569032392740")」。

## SurfaceFlinger启动流程

SurfaceFlinger的启动，此前有说明过，在surfaceflinger.rc配置文件解析出一个SurfaceFlinger服务后，运行其对应的/system/bin/surfaceflinger应用程序，这个程序的入口在main\_surfaceflinger.cpp文件中

```
int main(int, char**) {
    // 将忽视SIGPIPE信号
    signal(SIGPIPE, SIG_IGN);

    // 配置HIDL支持的最大线程数
    hardware::configureRpcThreadpool(1 /* maxThreads */,
            false /* callerWillJoin */);
// 启动图形系统的内存分配器
    startGraphicsAllocatorService();

    // ...... binder机制配置流程，本篇不做详细探讨，省略

    // instantiate surfaceflinger
    // 1. 初始化一个SurfaceFlinger对象
    sp<SurfaceFlinger> flinger = surfaceflinger::createSurfaceFlinger();
// 设置进程优先级
    setpriority(PRIO_PROCESS, 0, PRIORITY_URGENT_DISPLAY);
    
    set_sched_policy(0, SP_FOREGROUND);

    // Put most SurfaceFlinger threads in the system-background cpuset
    // Keeps us from unnecessarily using big cores
    // Do this after the binder thread pool init
    // 设置CPU提供更多的cpu core来处理surfaceflinger进程的事件调度
    if (cpusets_enabled()) set_cpuset_policy(0, SP_SYSTEM);

    // initialize before clients can connect
    // 2. 初始化SurfaceFlinger
    flinger->init();
    // ...... 在ServiceManager中注册surfaceflinger服务的过程，此篇不做讨论，省略
// 启动显示设备服务
    startDisplayService(); // dependency on SF getting registered above

    // 设置surfaceflinger进程的进程调度策略为SCHED_FIFO
    if (SurfaceFlinger::setSchedFifo(true) != NO_ERROR) {
        ALOGW("Couldn't set to SCHED_FIFO: %s", strerror(errno));
    }

    // run surface flinger in this thread
    // 3. 运行SurfaceFlinger线程
    flinger->run();

    return 0;
}
```

从main函数中可知，在这边主要做了如下的事情

1.  设置忽视SIGPIPE信号，LINUX系统中SIGPIPE信号的默认处理方式是直接终止进程
2.  设置HIDL支持的最大线程数为1
3.  Binder机制，设置进程支持的最大线程数为4
4.  Binder机制，启动线程池
5.  通过surfaceflinger::createSurfaceFlinger函数初始化一个SurfaceFlinger指针对象
6.  设置surfaceflinger进程的优先级，且设置系统优先为surfaceflinger进程提供服务
7.  SurfaceFlinger::init函数调用
8.  ServiceManager中注册surfaceflinger服务
9.  startDisplayService函数调用
10.  设置surfaceflinger进程调度策略为SCHED\_FIFO
11.  SurfaceFlinger::run函数调用

本篇重点分析SurfaceFlinger服务的启动，因此重点分析上述的5/7/11步骤

## surfaceflinger::createSurfaceFlinger函数创建SurfaceFlinger指针对象

```
frameworks/native/services/surfaceflinger/SurfaceFlingerFactory.cpp
sp<SurfaceFlinger> createSurfaceFlinger() {
    // 此处的DefaultFactory类定义于frameworks/native/services/surfaceflinger/SurfaceFlingerDefaultFactory.cpp文件中
    // 其主要目的是为了SurfaceFlinger进程中创建对应的一些默认的接口对象
    static DefaultFactory factory;

    return new SurfaceFlinger(factory);
}
```

此处通过new的方式创建一个SurfaceFlinger对象，需要注意的是DefaultFactory对象，这个对象的主要作用是创建一些默认的对应对象

```
SurfaceFlinger::SurfaceFlinger(Factory& factory) : SurfaceFlinger(factory, SkipInitialization) {
    // ...... 参数赋值操作，此处不做具体分析，使用的时候使用默认值，省略代码
}

SurfaceFlinger::SurfaceFlinger(Factory& factory, SkipInitializationTag)
      : mFactory(factory),
        mInterceptor(mFactory.createSurfaceInterceptor(this)),
        mTimeStats(std::make_shared<impl::TimeStats>()),
        mFrameTracer(std::make_unique<FrameTracer>()),
        mEventQueue(mFactory.createMessageQueue()),
        mCompositionEngine(mFactory.createCompositionEngine()),
        mInternalDisplayDensity(getDensityFromProperty("ro.sf.lcd_density", true)),
        mEmulatedDisplayDensity(getDensityFromProperty("qemu.sf.lcd_density", false)) {}
```

BnSurfaceComposer

+onTransact(uint32\_t, const Parcel&, Parcel\*, uint32\_t)

SurfaceFlinger

\-Scheduler:mScheduler

\-VSyncModulator:mVSyncModulator

\-MessageQueue:mEventQueue

\-CompositionEngine:mCompositionEngine

\-ConnectionHandle:mAppConnectionHandle

\-ConnectionHandle:mSfConnectionHandle

\-createLayer()

\-onTransact(uint32\_t, const Parcel&, Parcel\*, uint32\_t)

BnInterface

ISurfaceComposer

BpInterface

BpSurfaceComposer

IInterface

从上述SurfaceFlinger的类图和构造函数中可以看到，在SurfaceFlinger类的父类是BnSurfaceComposer，它与BpSurfaceComposer之间实现了Binder机制的IPC跨进程通信， 而在SurfaceFlinger指针对象的初始化过程中，通过的DefaultFactory对象MessageQueue指针对象，MessageQueue指针对象用作管理消息的接收、分发和处理。

MessageQueue

#SurfaceFlinger：mFlinger

#Looper:mLooper

#EventThreadConnection:mEvents

#BitTube:mEventTube

#Handler:mHandler

#cb\_eventReceiver(int, int, void\*)

#eventReceiver(int fd, int events)

+init(const sp& flinger)

+setEventConnection(const sp&)

+waitMessage()

+postMessage(sp&&)

+invalidate()

+refresh()

Handler

#MessageQueue&:mQueue

#int32\_t:mEventMask

+handleMessage(const Message&)

+dispatchRefresh()

+dispatchInvalidate(nsecs\_t)

Task

#handleMessage(const Message&)

MessageHandler

+handleMessage(const Message&)

SurfaceFlinger的初始化，由于这边初始化的是一个SurfaceFlinger强指针对象，因此在初始化的时候会调用其onFirstRef函数，因此

```
void SurfaceFlinger::onFirstRef() {
    mEventQueue->init(this);
}

void MessageQueue::init(const sp<SurfaceFlinger>& flinger) {
    mFlinger = flinger;
    mLooper = new Looper(true);
    mHandler = new Handler(*this);
}
```

可见，在SurfaceFlinger指针对象初始化中，主要是初始化一些必要的指针对象和必要的默认参数，而SurfaceFlinger::onFirstRef函数的调用，更说明了，在这个服务中，主要通过消息机制来处理数据的流转和信息的交互

## SurfaceFlinger初始化

在main函数中，还会调用SurfaceFlinger::init函数，让我们去看看这个函数中具体做了什么呢？

```
void SurfaceFlinger::init() {
    // ......日志打印，省略
    // 同步锁
    Mutex::Autolock _l(mStateLock);

    // Get a RenderEngine for the given display / config (can't fail)
    // TODO(b/77156734): We need to stop casting and use HAL types when possible.
    // Sending maxFrameBufferAcquiredBuffers as the cache size is tightly tuned to single-display.
    // 1. 初始化一个RenderEngine指针对象，然后通过CompositionEngine的setRenderEngine函数设置参数
    mCompositionEngine->setRenderEngine(renderengine::RenderEngine::create(
            renderengine::RenderEngineCreationArgs::Builder()
                .setPixelFormat(static_cast<int32_t>(defaultCompositionPixelFormat))
                .setImageCacheSize(maxFrameBufferAcquiredBuffers)
                .setUseColorManagerment(useColorManagement)
                .setEnableProtectedContext(enable_protected_contents(false))
                .setPrecacheToneMapperShaderOnly(false)
                .setSupportsBackgroundBlur(mSupportsBlur)
                .setContextPriority(useContextPriority
                        ? renderengine::RenderEngine::ContextPriority::HIGH
                        : renderengine::RenderEngine::ContextPriority::MEDIUM)
                .build()));
// CompositionEngine保存TimeStats指针对象地址引用
    mCompositionEngine->setTimeStats(mTimeStats);
    // ......日志打印，省略
// 3. 初始化一个HWComposer，然后通过CompositionEngine指针对象保存HWComposer指针对象地址引用
    mCompositionEngine->setHwComposer(getFactory().createHWComposer(getBE().mHwcServiceName));
// 4. 设置Configuration，这边是一个比较复杂和重要的函数
    mCompositionEngine->getHwComposer().setConfiguration(this, getBE().mComposerSequenceId);
    // Process any initial hotplug and resulting display changes.
    // 5. 运行显示设备热插拔事件，在setConfiguration的调用过程中会触发一次显示设备的热插拔事件
    // 添加默认设备后，此处若没有其他人插拔事件的话，会直接跳过运行
    processDisplayHotplugEventsLocked();
// 6. 获取默认的显示设备
    const auto display = getDefaultDisplayDeviceLocked();
    // ...... 判错日志，省略
// ...... 使用的虚拟设备，此处我们暂不考虑虚拟显示设备，先省略这部分代码
    // initialize our drawing state
    // 初始化送显状态
    // 显示设备添加事件完成后，将状态进行转换
    mDrawingState = mCurrentState;

    // set initial conditions (e.g. unblank default device)
    // 8. 初始化显示设备
    initializeDisplays();

    char primeShaderCache[PROPERTY_VALUE_MAX];
    property_get("service.sf.prime_shader_cache", primeShaderCache, "1");
    if (atoi(primeShaderCache)) {
        getRenderEngine().primeCache();
    }

    // Inform native graphics APIs whether the present timestamp is supported:
// 9. 创建一个StartPropertySetThread线程并运行
    const bool presentFenceReliable =
            !getHwComposer().hasCapability(hal::Capability::PRESENT_FENCE_IS_NOT_RELIABLE);
    mStartPropertySetThread = getFactory().createStartPropertySetThread(presentFenceReliable);

    if (mStartPropertySetThread->Start() != NO_ERROR) {
        ALOGE("Run StartPropertySetThread failed!");
    }

    ALOGV("Done initializing");
}
```

### Opengl和EGL的启动

在SurfaceFlinger::init函数的开始，就会通过RenderEngine::create函数来初始化一个GLESRenderEngine，从他们的名称来看，这个指针对象应该是一个绘制引擎，初始化OPENGL ES的一个对象，而事实上也是如此，在这个函数的初始化过程中会对EGL和OPENGL进行初始化，在这个过程中，会加载egl和opengl的库等，这块具体的分析，等后续再来具体的分析 在init函数中，还会调用GLESRenderEngine的primeCache函数，也是一样，主要是设置OPENGL的着色器状态

### 初始化HWComposer指针对象和设置其配置

HWComposer指针对象的初始化也是通过DefaultFactory对象来初始化的，最终会通过new的方式初始化

```
// 这边传入参数默认为"default"
std::unique_ptr<HWComposer> DefaultFactory::createHWComposer(const std::string& serviceName) {
    return std::make_unique<android::impl::HWComposer>(serviceName);
}

// 同时会初始化一个Composer指针对象
HWComposer::HWComposer(const std::string& composerServiceName)
      : mComposer(std::make_unique<Hwc2::impl::Composer>(composerServiceName)) {
}

// 注意，此处传递的第一个参数是SurfaceFlinger对象本身，其实现了HWC2::ComposerCallback接口类
void HWComposer::setConfiguration(HWC2::ComposerCallback* callback, int32_t sequenceId) {
    loadCapabilities();
    loadLayerMetadataSupport();

    // 判断callback是否已经注册，若已经注册，则直接退出
    if (mRegisteredCallback) {
        ALOGW("Callback already registered. Ignored extra registration attempt.");
        return;
    }
    mRegisteredCallback = true;
    sp<ComposerCallbackBridge> callbackBridge(
            new ComposerCallbackBridge(callback, sequenceId,
                                       mComposer->isVsyncPeriodSwitchSupported()));
    mComposer->registerCallback(callbackBridge);
}
```

HWComposer

\-HWC2::Composer : mComposer

+getComposer()

+loadCapabilities()

+loadLayerMetadataSupport()

+allocatePhysicalDisplay()

+setConfiguration(HWC2::ComposerCallback\* callback, int32\_t sequenceId)

Composer

+registerCallback(const sp& callback)

这边有一个点需要注意的是，当通过HWComposer::setConfiguration向Hwc2::Composer中注册HWC2::ComposerCallback之后，底层硬件层会检测当前是否已经包含了显示屏幕（当然，在我们正常的手机/平板或者其他设备中，均会默认有一个物理显示屏），因此此处会产生一个显示屏幕设备的信号给到frameworks层，当系统收到这个信号的时候SurfaceFlinger::onHotplugReceived函数即时被调用

```
void SurfaceFlinger::onHotplugReceived(int32_t sequenceId, hal::HWDisplayId hwcDisplayId,
                                       hal::Connection connection) {
    // ...... 功能无关的判断操作，代码省略
    // 添加HotplugEvent事件
    mPendingHotplugEvents.emplace_back(HotplugEvent{hwcDisplayId, connection});
    
    if (std::this_thread::get_id() == mMainThreadId) {
        // Process all pending hot plug events immediately if we are on the main thread.
        // 运行显示设备插入事件
        processDisplayHotplugEventsLocked();
    }
    setTransactionFlags(eDisplayTransactionNeeded);
}

void SurfaceFlinger::processDisplayHotplugEventsLocked() {
    // 上述的onHotplugReceived函数刚刚添加了一个HotplugEvent事件
    for (const auto& event : mPendingHotplugEvents) {
        // 这边会判断当前的hwcDisplayID对应的DisplayIdentificationInfo是否存在
        // 若不存在，则初始化一个DisplayIdentificationInfo对象，同时会在系统层中分配当前默认Display的内存空间
        // 若存在，则更新其参数
        const std::optional<DisplayIdentificationInfo> info =
                getHwComposer().onHotplug(event.hwcDisplayId, event.connection);
        // 
        if (!info) {
            continue;
        }

        const DisplayId displayId = info->id;
        const auto it = mPhysicalDisplayTokens.find(displayId);

        if (event.connection == hal::Connection::CONNECTED) {
            if (it == mPhysicalDisplayTokens.end()) {
                ALOGV("Creating display %s", to_string(displayId).c_str());

                if (event.hwcDisplayId == getHwComposer().getInternalHwcDisplayId()) {
                    // Scheduler初始化
                    initScheduler(displayId);
                }
                // 初始化和配置DisplayDeviceState对象
                DisplayDeviceState state;
                state.physical = {displayId, getHwComposer().getDisplayConnectionType(displayId),
                                  event.hwcDisplayId};
                state.isSecure = true; // All physical displays are currently considered secure.
                state.displayName = info->name;

                sp<IBinder> token = new BBinder();
                mCurrentState.displays.add(token, state);
                mPhysicalDisplayTokens.emplace(displayId, std::move(token));
                // 提示创建Display对象的Trace
                mInterceptor->saveDisplayCreation(state);
            }
            // ...... 无法进入分支，代码省略
        }
        // ...... 无法进入分支，代码省略
        // 触发Display变化的事件
        processDisplayChangesLocked();
    }
    // 清理事件
    mPendingHotplugEvents.clear();
}
```

可以看到，这边主要做了如下几个工作

1.  调用HWComposer对象的onHotplug函数，初始化一个DisplayIdentificationInfo对象，并且在系统层在内存中分配一个Display的内存同驱动层的HWCDisplay对象关联
2.  通过initScheduler函数，初始化Scheduler指针对象，同时设置两条连接链路，用作SurfaceFlinger消息的分发和处理，此处将在后续另写一篇做详细的概述
3.  初始化和配置DisplayDeviceState对象，并且保存在mPhysicalDisplayTokens中，同时更新mCurrentState当前状态
4.  触发Display变化事件，配置设备添加后的一些配置和连接问题

在此后会通过initializeDisplays函数对新初始化的Displays进行初始化,此处我们暂不做详细分析，待后续单独更新详细文档

### 对显示设备进行初始化

在SurfaceFlinger::init函数的最后，会初始化一个StartPropertySetThread的线程，并运行

```
sp<StartPropertySetThread> DefaultFactory::createStartPropertySetThread(
        bool timestampPropertyValue) {
    return new StartPropertySetThread(timestampPropertyValue);
}

StartPropertySetThread::StartPropertySetThread(bool timestampPropertyValue):
        Thread(false), mTimestampPropertyValue(timestampPropertyValue) {}

status_t StartPropertySetThread::Start() {
    return run("SurfaceFlinger::StartPropertySetThread", PRIORITY_NORMAL);
}

bool StartPropertySetThread::threadLoop() {
    // Set property service.sf.present_timestamp, consumer need check its readiness
    property_set(kTimestampProperty, mTimestampPropertyValue ? "1" : "0");
    // Clear BootAnimation exit flag
    property_set("service.bootanim.exit", "0");
    // Start BootAnimation if not started
    // 启动bootanim进程
    property_set("ctl.start", "bootanim");
    // Exit immediately
    return false;
}
```

可以看到，此处通过线程来设置开机动画的属性，那么为何需要在此处设置这个属性呢？ 如果看开机动画的bootanim.rc文件可以看到，其默认是disable的，也就是说，开机动画进程，默认是不会自启动的，需要有其他地方来触发 而通过此处的ctl.start属性，能够触发bootanim进程，从而开始显示开机动画

### 总结

在SurfaceFlinger::init函数中主要是做了

1.  初始化GLESRenderEngine指针对象，在这个初始化的过程中，初始化OPENGL和EGL的相关库
2.  初始化HWComposer指针对象，这个类跟系统Composer驱动进行关联，并且在通过其函数setConfiguration的时候，设置ComposerCallback回调，并且在过程中初始化显示设备，并启动回调函数onHotplugReceive函数，在这个函数中，初始化系统层的Display相关的对象和配置，同Display驱动层对应的显示设备进行关联
3.  初始化和启动StartPropertySetThread线程，在这个线程中判断当前开机动画进程是否已经启动，若未启动，则直接启动该进程

## 启动SurfaceFlinger的消息处理机制

最后，调用了SurfaceFlinger::run函数，来启动其消息处理机制

```
void SurfaceFlinger::run() {
    // 直接来一个无限循环while，等待消息
    while (true) {
        mEventQueue->waitMessage();
    }
}

void MessageQueue::waitMessage() {
    do {
        IPCThreadState::self()->flushCommands();
        int32_t ret = mLooper->pollOnce(-1);
        switch (ret) {
            case Looper::POLL_WAKE:
            case Looper::POLL_CALLBACK:
                continue;
            case Looper::POLL_ERROR:
                ALOGE("Looper::POLL_ERROR");
                continue;
            case Looper::POLL_TIMEOUT:
                // timeout (should not happen)
                continue;
            default:
                // should not happen
                ALOGE("Looper::pollOnce() returned unknown status %d", ret);
                continue;
        }
    } while (true);
}
```

这样就会在无限循环的等待消息过来后，直接处理

## 总结流程图

![图片.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/0ebf1a93abb449798e37e053b53d4e1b~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)