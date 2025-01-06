Vsync信号是SurfaceFlinger进程中核心的一块逻辑，我们主要从以下几个方面着手讲解。

- 软件Vsync是怎么实现的，它是如何保持有效性的？
- systrace中看到的VSYNC信号如何解读，这些脉冲信号是在哪里打印的？
- 为什么VSYNC-sf / VSYNC-app 时断时续？
- SF请求VSYNC-SF信号进行合成的流程是怎样的？
- “dumpsys SurfaceFlinger --dispsync"命令输出如何解读？

#### 1. VSYNC 信号

当前手机屏幕显示屏大部分都是60Hz（也有一部分是90Hz/120Hz/165Hz）,意味着显示屏每隔16.66毫秒刷新一次，如果在显示屏刷新时刻去更新显示的内容，就会导致屏幕撕裂（其中还有掉帧问题，就是连续的两个刷新周期，显示屏只能显示同一帧图像，具体请查询Android黄油计划），为了避免这种情况，我们在显示屏幕两次刷新之间的空档期去更新显示内容，当可以安全的更新内容时，系统会收到显示屏发来的信号，处于历史原因，我们称之为VSYNC信号。

##### 1.1 硬件VSYNC和软件VSYNC

通过systrace来认识VSYNC

![](https://upload-images.jianshu.io/upload_images/26874665-1aa54475b3f46239.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20220512154819566.png

1. 因为我们只有一个显示屏，所以只有一个硬件VSYNC，即HW_VSYNC。HW_VSYNC_**displayid**脉冲的宽度是16ms，因此显示屏的帧率是60Hz。
2. HW_VSYNC_ON_**displayid**表示硬件VSYNC是否打开。可见硬件VSYNC大部分时间是关闭的，只有在特殊场景下才会打开（比如更新SW VSYNC模型的时候）,**displayId**在sf中标识这个显示屏的唯一字符串。
3. App的绘制以及SF的合成分别由对应的软件VSYNC来驱动的：VSYNC-app驱动App进行绘制；VSYNC-sf驱动SF对相关的Layer进行合成。
4. VSYNC-app与VSYNC-sf是”按需发射“的，如果App要更新界面，它得”申请“VSYNC-app，如果没有App申请VSYNC-app，那么VSYNC-app将不再发射。同样，当App更新了界面，它会把对应的Graphic Buffer放到Buffer Queue中。Buffer Queue通知SF进行合成，此时SF会申请VSYNC-sf。如果SF不再申请VSYNC-sf，VSYNC-sf将不再发射。注意，默认情况下这些申请都是一次性的，意味着，如果App要持续不断的更新，它就得不断去申请VSYNC-app；而对SF来说，只要有合成任务，它就得再去申请VSYNC-sf。
5. VSYNC-app与VSYNC-sf是相互独立的。VSYNC-app触发App的绘制，Vsync-sf触发SF合成。App绘制与SF合成都会加大CPU的负载，为了避免绘制与合成打架造成的性能问题，VSYNC-app可以与VSYNC-sf稍微错开一下，像下图一样：

![](https://upload-images.jianshu.io/upload_images/26874665-0ff2a3b27be1d372.png?imageMogr2/auto-orient/strip|imageView2/2/w/740/format/webp)

image-20220512155339894.png

6. 从我们抓的systrace中也可看到这种偏移，但是要注意：systrace中VSYNC脉冲，上升沿与下降沿各是一次VSYNC信号。这里的高、低电平只是一种示意，如果要查看VSYNC-app与VSYNC-sf的偏移，不能错误的以为“同是上升沿或者同是下降沿进行比对”。忘记上升沿或者下降沿吧，只需拿两个人相邻的VSYNC信号进行比对。如下图所示，VSYNC-app领先VSYNC-sf有85微秒。不过要注意，这个85微秒只是软件误差，算不得数，在我们的系统中，VSYNC-app与VSYNC-sf并没有错开。有必要再补充下：SF进行合成的是App的上一帧，而App当前正在绘制的那一帧，要等到下一个VSYNC-sf来临时再进行合成。

![](https://upload-images.jianshu.io/upload_images/26874665-2f01d64e4f00804b.png?imageMogr2/auto-orient/strip|imageView2/2/w/1069/format/webp)

image-20220512160425281.png

##### 1.2 与VSYNC相关的线程

抓了好几份systrace，都没有显示出线程的名字，按照后面讲解代码中的理解，我用679手机查看SurfaceFlinger进程的线程信息，大概列出和VSYNC相关的线程名字。

![](https://upload-images.jianshu.io/upload_images/26874665-b8cc0fc3c8fa2df2.png?imageMogr2/auto-orient/strip|imageView2/2/w/877/format/webp)

image-20220512161026999.png

- TimerDispatch线程： TimerDispatch充当软件VSYNC的信号泵，这个线程包装成VsyncDispatchTimeQueue这个类，里面有一个CallbackMap变量，存放的是那些关心VSYNC信号的人（appEventThread, appSfEventThread, sf的MessageQueue），TimerDispatch就是根据模型计算的唤醒时间对着它们发送SW VSYNC。
- appEventThread线程：它是EventThread类型的实例，它是VSYNC-app寄宿的线程。很明显，它就是VSYNC-app的掌门人。一方面，它接收App对VSYNC-app的请求，如果没有App请求VSYNC-app，它就进入休眠；另一方面，它接收TimerDispatch发射过来VSYNC-app，控制App的绘制。
- appSfEventThread线程：它是EventThread类型的实例，它是VSYNC-appSf寄宿的线程，和appEventThread线程功能是类似的，用于调试代码，暂时忽略。
- MessageQueue（表示主线程）： 它是VSYNC-sf寄宿的线程，很明显，它就是VSYNC-sf的掌门人，不过它专给SF一个人服务。一方面，如果SF有合成需求，会向它提出申请；另一方面，它接收TimerDispatch发射过来的VSYNC-sf，控制SF的合成。

HW VSYNC/SW VSYNC/VSYNC/VSYNC-app与VSYNC-SF的关联可以用一个PLL图来表示：

![](https://upload-images.jianshu.io/upload_images/26874665-01f659c681071705.png?imageMogr2/auto-orient/strip|imageView2/2/w/672/format/webp)

image-20221025145430443.png

##### 1.3 VSYNC信号从哪里开始初始化的？

因为Android大版本每次更新，SurfaceFlinger模块都要进行代码重构，Android S现在VSYNC的逻辑和Android R版本代码有很大的差别，所以我们就从Android S代码的源头开始讲起。

我们在讲解SurfaceFlinger::init方法的时候，init会去初始化HWComposer并注册回调函数，如下：

```c
/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp

mCompositionEngine->setTimeStats(mTimeStats);
mCompositionEngine->setHwComposer(getFactory().createHWComposer(mHwcServiceName));
mCompositionEngine->getHwComposer().setCallback(this);
```

1. 创建一个HWComposer对象并传入一个name属性，再把该对象设置到mCompositionEngine中。
    
2. 这里的this就是SurfaceFlinger本身，它实现了HWC2::ComposerCallback回调方法。
    

定义ComposerCallback回调方法

```c
/frameworks/native/services/surfaceflinger/DisplayHardware/HWC2.h

struct ComposerCallback {
    virtual void onComposerHalHotplug(hal::HWDisplayId, hal::Connection) = 0;
    virtual void onComposerHalRefresh(hal::HWDisplayId) = 0;
    virtual void onComposerHalVsync(hal::HWDisplayId, int64_t timestamp,
                                    std::optional<hal::VsyncPeriodNanos>) = 0;
    virtual void onComposerHalVsyncPeriodTimingChanged(hal::HWDisplayId,
                                                       const hal::VsyncPeriodChangeTimeline&) = 0;
    virtual void onComposerHalSeamlessPossible(hal::HWDisplayId) = 0;

protected:
    ~ComposerCallback() = default;
};
```

根据HWC2::ComposerCallback的设计逻辑，SurfaceFlinger::init方法中设置完HWC的回调之后，会立刻收到一个Hotplug事件，并在SurfaceFlinger::onComposerHalHotplug中去处理，所以流程就走到了：

```c
/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp
    
void SurfaceFlinger::onComposerHalHotplug(hal::HWDisplayId hwcDisplayId, hal::Connection connection) {
  ...
    if (std::this_thread::get_id() == mMainThreadId) {
        // Process all pending hot plug events immediately if we are on the main thread.
        processDisplayHotplugEventsLocked();
    }
  ...
}
```

继续分析processDisplayHotplugEventsLocked的方法

```c
/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp
    
void SurfaceFlinger::processDisplayHotplugEventsLocked() {
    for (const auto& event : mPendingHotplugEvents) {
        std::optional<DisplayIdentificationInfo> info =
                getHwComposer().onHotplug(event.hwcDisplayId, event.connection);
        ...
        if (event.connection == hal::Connection::CONNECTED) {
            ...
            if (it == mPhysicalDisplayTokens.end()) {
                ...
                if (event.hwcDisplayId == getHwComposer().getInternalHwcDisplayId()) {
                    initScheduler(state);
                }
                ...
            } 
    }
    ...
}
```

就会走到了initScheduler方法，这个方法就是初始化VSYNC信号的函数。

```c
/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp

void SurfaceFlinger::initScheduler(const DisplayDeviceState& displayState) {
    if (mScheduler) {
        // In practice it's not allowed to hotplug in/out the primary display once it's been
        // connected during startup, but some tests do it, so just warn and return.
        ALOGW("Can't re-init scheduler");
        return;
    }
    const auto displayId = displayState.physical->id;
    scheduler::RefreshRateConfigs::Config config =
            {.enableFrameRateOverride = android::sysprop::enable_frame_rate_override(false),
             .frameRateMultipleThreshold =
                     base::GetIntProperty("debug.sf.frame_rate_multiple_threshold", 0)};
    mRefreshRateConfigs =
            std::make_unique<scheduler::RefreshRateConfigs>(displayState.physical->supportedModes,
                                                            displayState.physical->activeMode
                                                                    ->getId(),
                                                            config);
    const auto currRefreshRate = displayState.physical->activeMode->getFps();
    mRefreshRateStats = std::make_unique<scheduler::RefreshRateStats>(*mTimeStats, currRefreshRate,
                                                                      hal::PowerMode::OFF);

    mVsyncConfiguration = getFactory().createVsyncConfiguration(currRefreshRate);
    mVsyncModulator = sp<VsyncModulator>::make(mVsyncConfiguration->getCurrentConfigs());

    // start the EventThread
    mScheduler = getFactory().createScheduler(*mRefreshRateConfigs, *this);
    const auto configs = mVsyncConfiguration->getCurrentConfigs();
    const nsecs_t vsyncPeriod = currRefreshRate.getPeriodNsecs();
    mAppConnectionHandle =
            mScheduler->createConnection("app", mFrameTimeline->getTokenManager(),
                                         /*workDuration=*/configs.late.appWorkDuration,
                                         /*readyDuration=*/configs.late.sfWorkDuration,
                                         impl::EventThread::InterceptVSyncsCallback());
    mSfConnectionHandle =
            mScheduler->createConnection("appSf", mFrameTimeline->getTokenManager(),
                                         /*workDuration=*/std::chrono::nanoseconds(vsyncPeriod),
                                         /*readyDuration=*/configs.late.sfWorkDuration,
                                         [this](nsecs_t timestamp) {
                                             mInterceptor->saveVSyncEvent(timestamp);
                                         });

    mEventQueue->initVsync(mScheduler->getVsyncDispatch(), *mFrameTimeline->getTokenManager(),
                           configs.late.sfWorkDuration);

    mRegionSamplingThread =
            new RegionSamplingThread(*this, RegionSamplingThread::EnvironmentTimingTunables());
    mFpsReporter = new FpsReporter(*mFrameTimeline, *this);
    // Dispatch a mode change request for the primary display on scheduler
    // initialization, so that the EventThreads always contain a reference to a
    // prior configuration.
    //
    // This is a bit hacky, but this avoids a back-pointer into the main SF
    // classes from EventThread, and there should be no run-time binder cost
    // anyway since there are no connected apps at this point.
    mScheduler->onPrimaryDisplayModeChanged(mAppConnectionHandle, displayId,
                                            displayState.physical->activeMode->getId(),
                                            vsyncPeriod);
    static auto ignorePresentFences =
            base::GetBoolProperty("debug.sf.vsync_reactor_ignore_present_fences"s, false);
    mScheduler->setIgnorePresentFences(
            ignorePresentFences ||
            getHwComposer().hasCapability(hal::Capability::PRESENT_FENCE_IS_NOT_RELIABLE));
}
```

1. 先判断mScheduler是否为空，避免重复初始化。
    
2. 初始化mRefreshRateConfigs对象，这个对象包含了刷新率的配置信息，包含当前屏幕的刷新率，刷新周期等信息。
    
3. currRefreshRate是一个Fps对象，其中存储了刷新率fps和刷新周期period。
    
4. 初始化mVsyncConfiguration对象，这个类封装了不同刷新率下的Vsync配置信息，app phase就是VSYNC-app的偏移量，sf phase是VSYNC-sf的偏移量。这个类会再创建appEventThread或者sf的回调函数中把偏移量传递进去，主要是为了计算SW VSYNC唤醒VSYNC-app或者VSYNC-sf的时间，这个类可以通过属性进行配置，代码实现中也固定了部分参数。
    
5. 初始化Scheduler对象 mScheduler，这个类的构造函数中，初始化了 VsyncSchedule这个结构体，该结构体里面的三个对象都非常重要，dispatch就是TimerDispatcher的线程，也就是VYSNC信号的节拍器（心跳），其他两个对象是为dispatch服务的。
    
    ```c
    /frameworks/native/services/surfaceflinger/Scheduler/Scheduler.h
        
    struct VsyncSchedule {
        std::unique_ptr<scheduler::VsyncController> controller;
        std::unique_ptr<scheduler::VSyncTracker> tracker;
        std::unique_ptr<scheduler::VSyncDispatch> dispatch;
    };
    ```
    
6. 创建appEventThread和appSfEventThread，appEventThread/appSfEventThread就是上面说的这个线程，同步绑定回调函数到VsyncDispatch上面，名字是"app","appSf","sf"。
    
7. mEventQueue的initVsync方法主要是绑定一个回调函数到VsyncDispatch上面，回调名字是"sf"。
    

那么从上面的代码逻辑中，我们可以知道节拍器线程（心跳）一共绑定了3个Callback，分别是"app" ,"appSf","sf"，简单画个调用关系图如下：

![](https://upload-images.jianshu.io/upload_images/26874665-8b090b48d9ad7a1f.png?imageMogr2/auto-orient/strip|imageView2/2/w/449/format/webp)

image-20220513152401542.png

#### 2.1 VSYNC-sf/VSYNC-app的申请与投递

我们先看看通道的建立过程，也是从源代码开始看起。

##### 2.1.1 向VsyncDispatch注册Callback

我们知道VsyncDispatch是节拍器（心跳），也就是TimerDispatch的线程所在，所以我们需要了解下VsyncDispatch是在什么时候初始化的？在前面Vsync信号初始化的逻辑中，我们了解到Scheduler类再构造方法中会创建VsyncDispatch对象，而这个对象也就是SurfaceFlinger系统中唯一的，相关代码如下：

```c
/frameworks/native/services/surfaceflinger/Scheduler/Scheduler.cpp

Scheduler::Scheduler(const scheduler::RefreshRateConfigs& configs, ISchedulerCallback& callback,
                     Options options)
      : Scheduler(createVsyncSchedule(options.supportKernelTimer), configs, callback,
                  createLayerHistory(configs), options) {
   ...
}
```

调用createVsyncSchedule方法

```c
/frameworks/native/services/surfaceflinger/Scheduler/Scheduler.cpp

Scheduler::VsyncSchedule Scheduler::createVsyncSchedule(bool supportKernelTimer) {
    auto clock = std::make_unique<scheduler::SystemClock>();
    auto tracker = createVSyncTracker();
    auto dispatch = createVSyncDispatch(*tracker);

    // TODO(b/144707443): Tune constants.
    constexpr size_t pendingFenceLimit = 20;
    auto controller =
            std::make_unique<scheduler::VSyncReactor>(std::move(clock), *tracker, pendingFenceLimit,
                                                      supportKernelTimer);
    return {std::move(controller), std::move(tracker), std::move(dispatch)};
}
```

调用createVsyncDispatch方法

```c
/frameworks/native/services/surfaceflinger/Scheduler/Scheduler.cpp

std::unique_ptr<scheduler::VSyncDispatch> createVSyncDispatch(scheduler::VSyncTracker& tracker) {
    // TODO(b/144707443): Tune constants.
    constexpr std::chrono::nanoseconds vsyncMoveThreshold = 3ms;
    constexpr std::chrono::nanoseconds timerSlack = 500us;
    return std::make_unique<
            scheduler::VSyncDispatchTimerQueue>(std::make_unique<scheduler::Timer>(), tracker,
                                                timerSlack.count(), vsyncMoveThreshold.count());
}
```

从该方法可以看出，Scheduler对象初始化的时候，会间接的构造出VsyncDispatchTimerQueue对象，这个时候有小伙伴就疑问怎么不是VsyncDispatch对象呢？

这边我们把这几个类图的关系画出来，如下：

![](https://upload-images.jianshu.io/upload_images/26874665-9eaa87702bb09114.png?imageMogr2/auto-orient/strip|imageView2/2/w/503/format/webp)

image-20220513164519305.png

VsyncDispatchTimerQueue是继承VsyncDispatch，而节拍器（心跳）线程也就是该对象中的mTimeKeeper，这个Timer.cpp中会创建TimerDispatch这个名字的线程。

```c
/frameworks/native/services/surfaceflinger/Scheduler/Timer.cpp

Timer::Timer() {
    reset();
    mDispatchThread = std::thread([this]() { threadMain(); });
}

void Timer::reset() {
    cleanup();
    mTimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    mEpollFd = epoll_create1(EPOLL_CLOEXEC);
    if (pipe2(mPipes.data(), O_CLOEXEC | O_NONBLOCK)) {
        ALOGE("could not create TimerDispatch mPipes");
        return;
    };
    setDebugState(DebugState::Reset);
}

void Timer::threadMain() {
    while (dispatch()) { //核心方法
        reset();
    }
}

bool Timer::dispatch() {
    ...
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        ALOGW("Failed to set SCHED_FIFO on dispatch thread");
    }

    if (pthread_setname_np(pthread_self(), "TimerDispatch")) { //线程命名
        ALOGW("Failed to set thread name on dispatch thread");
    }
    ...
     
    while (true) {
         ...
        int nfds = epoll_wait(mEpollFd, events, DispatchType::MAX_DISPATCH_TYPE, -1);
        ...  
        for (auto i = 0; i < nfds; i++) {
            if (events[i].data.u32 == DispatchType::TIMER) {
                ...
                if (cb) {
                    setDebugState(DebugState::InCallback);
                    cb();//回调操作
                    setDebugState(DebugState::Running);
                }
            }
        ...
        }
    }
}
```

1. Timer类在构造方法会创建的一个线程mDispatchThread。
    
2. 在这里用到了timerfd，timerfd是Linux为用户程序提供一个定时器接口，这个接口基于文件描述符，通过文件描述符的可读事件进行超时通知，因此可以配合epoll等使用，timerfd_create() 函数创建一个定时器对象，同时返回一个与之关联的文件描述符。
    
    clockid: CLOCK_REALTIME:系统实时时间，随着系统实时时间改变而改变，即从UTC1970-1-1 0:0:0开始计时，CLOCK_MONOTONIC:从系统启动这一刻开始
    
    计时，不受系统时间被用户改变的影响。
    
    flags: TFD_NONBLOCK(非堵塞模式)/TFD_CLOEXEC(表示程序执行exec函数时本fd将被系统自动关闭，表示不传递)
    
    timerfd_settime（）这个函数用于设置新的超时时间，并开始计时，能够启动和停止定时器。
    
    fd: 参数fd是timerfd_create函数返回的文件句柄。
    
    flags: 参数flags为1设置是绝对时间，0代表是相对时间。
    
    new_value: 参数new_value指定的定时器的超时时间以及超时间隔时间。
    
    old_value: 参数old_value如果不为NULL, old_vlaue返回之前定时器设置的超时时间，具体参考timerfd_gettimer()函数。
    

**注意：it_interval不为0则表示是周期性定时器，it_value和it_interval都为0表示停止定时器。**

```c
/frameworks/native/services/surfaceflinger/Scheduler/Timer.cpp

void Timer::alarmAt(std::function<void()> const& cb, nsecs_t time) {
    std::lock_guard lock(mMutex);
    using namespace std::literals;
    static constexpr int ns_per_s =
            std::chrono::duration_cast<std::chrono::nanoseconds>(1s).count();

    mCallback = cb;

    struct itimerspec old_timer;
     struct itimerspec new_timer {
         .it_interval = {.tv_sec = 0, .tv_nsec = 0},
         .it_value = {.tv_sec = static_cast<long>(time / ns_per_s),
                      .tv_nsec = static_cast<long>(time % ns_per_s)},
     };
 
     if (timerfd_settime(mTimerFd, TFD_TIMER_ABSTIME, &new_timer, &old_timer)) { //核心方法
         ALOGW("Failed to set timerfd %s (%i)", strerror(errno), errno);
     }
 }
```

3. timerfd配合epoll函数使用，如果定时器时间到了，就会执行上图中alarmAt函数传入的函数指针，这个函数指针是VsyncDispatchTimerQueue.cpp类的timerCallback()函数，而这个函数中，就是对注册的callback执行回调。

##### 2.1.2 SF向VsyncDispatch注册回调的过程

下面，我们跟踪下SF是如何注册自己的回调函数。

我们知道，App有个主线程（ActivityThread）专门进行UI处理，该主线程是由一个消息队列（Looper/handler）驱动，主线程不断的从消息队列中取出消息，处理消息，如此往复。

```java
//@ActivityThread.java
public static void main(String[] args) {
    ......
    //准备UI主线程的消息循环
    Looper.prepareMainLooper();

    //创建ActivityThread，并调用attach方法
    ActivityThread thread = new ActivityThread();
    thread.attach(false);

    if (sMainThreadHandler == null) {
        sMainThreadHandler = thread.getHandler();
    }
    //进入主线程消息循环
    Looper.loop();
}
```

SF也类似，有个主线程负责处理合成相关的事物，同时有一个消息队列来驱动，从第一章中SurfaceFlinger模块的主流程模块中讲解了MessageQueue的初始化过程，当初始化完毕之后，SF主线程就进入了消息循环，等待有申请合入相关的事物，然后去做相应的处理。

MessageQueue中有个方法initVsync(),在前面讲解的VSYNC信号的初始化过程中，其中调用了MessageQueue的initVsync函数。

```c
/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp

mEventQueue->initVsync(mScheduler->getVsyncDispatch(), *mFrameTimeline->getTokenManager(),
                       configs.late.sfWorkDuration);
```

在initVsync函数中初始化mVsync.registation对象，这个对象是VSyncDispatch.h文件中定义的类 VSyncCallbackRegistration，这个类的作用是操作已经注册回调的帮助类，在该类的构造函数中间接调用dispatch.registerCallback()。

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncDispatchTimerQueue.cpp

VSyncCallbackRegistration::VSyncCallbackRegistration(VSyncDispatch& dispatch,
                                                     VSyncDispatch::Callback const& callbackFn,
                                                     std::string const& callbackName)
      : mDispatch(dispatch),
        mToken(dispatch.registerCallback(callbackFn, callbackName)),
        mValidToken(true) {}
```

dispatch的registerCallback函数就是注册需要监听SW-VSYNC的信号的函数，具体实现如下：

```c
VSyncDispatchTimerQueue::CallbackToken VSyncDispatchTimerQueue::registerCallback(
        Callback const& callbackFn, std::string callbackName) {
    std::lock_guard lock(mMutex);
    return CallbackToken{
            mCallbacks
                    .emplace(++mCallbackToken,
                             std::make_shared<VSyncDispatchTimerQueueEntry>(callbackName,
                                                                            callbackFn,
                                                                            mMinVsyncDistance))
                    .first->first};
}
```

VsyncDispatch的注册函数就会往mCallbacks注册封装了callbackFn的VsyncDispatchTimerQueueEntry对象。从上面的几个步骤来看就完成了SF向VsyncDispatch注册的全部流程，相对Android S之前的系统版本实现优化了下，并没有采用EventThread的方式。

回调过程如下：

1. 当VsyncDispatch发送VSYNC-sf的信号时，会走到MessageQueue类注册的回调函数。

```c
void MessageQueue::vsyncCallback(nsecs_t vsyncTime, nsecs_t targetWakeupTime, nsecs_t readyTime) {
    ATRACE_CALL();
    // Vsync-sf的trace节点
    mVsync.value = (mVsync.value + 1) % 2;

    {
        std::lock_guard lock(mVsync.mutex);
        mVsync.lastCallbackTime = std::chrono::nanoseconds(vsyncTime);
        mVsync.scheduled = false;
    }
    mHandler->dispatchInvalidate(mVsync.tokenManager->generateTokenForPredictions(
                                         {targetWakeupTime, readyTime, vsyncTime}),
                                 vsyncTime);
}
```

2. 在这个回调函数中，通过Handler将VSYNC事件转换成内部的msg，投递到消息队列中。
3. SF主线程从消息队列中取出消息，回调到SF->onMessageReceived()

可见，MessageQueue是接收VSYNC-SF信号的，将VsyncDispatch发送的VSYNC-SF信号通过自身转到SF，驱动SF执行合成操作。

##### 2.1.3 DispSyncSource是VsyncDispatch与EventThread之间的桥梁

DispSyncSource是对标准SW VSYNC的细分，产生VSYNC-app，它可以认为是信号源，仍然需要触发下游组件来接受信号，对DisplaySyncSource来说，它的下游组件就是EventThread。所以说DispSyncSource是VsyncDispatch与EventThread之间通讯的纽带。

在DispSyncSource类中，下游组件用mCallback来表示，mCallback是VSyncSource::Callback类型，而EventThread也继承自VsyncSource::Callback。

相关代码如下：

DispSyncSource是怎么和VsyncDispatch建立联系？

这个和SF向VsyncDispatch注册很类似，DispSyncSource有个mCallbackRepeater对象，该对象在初始化的时候，会传入DispSyncSource的回调接口DispSyncsource::onVsyncCallback。

```c
/frameworks/native/services/surfaceflinger/Scheduler/DispSyncSource.cpp

DispSyncSource::DispSyncSource(scheduler::VSyncDispatch& vSyncDispatch,
                               std::chrono::nanoseconds workDuration,
                               std::chrono::nanoseconds readyDuration, bool traceVsync,
                               const char* name)
      : mName(name),
        mValue(base::StringPrintf("VSYNC-%s", name), 0),
        mTraceVsync(traceVsync),
        mVsyncOnLabel(base::StringPrintf("VsyncOn-%s", name)),
        mWorkDuration(base::StringPrintf("VsyncWorkDuration-%s", name), workDuration),
        mReadyDuration(readyDuration) {
    mCallbackRepeater = //创建CallbackRepeater对象
            std::make_unique<CallbackRepeater>(vSyncDispatch,
                                               std::bind(&DispSyncSource::onVsyncCallback, this,
                                                         std::placeholders::_1,
                                                         std::placeholders::_2,
                                                         std::placeholders::_3),
                                               name, workDuration, readyDuration,
                                               std::chrono::steady_clock::now().time_since_epoch());
}
```

```c
/frameworks/native/services/surfaceflinger/Scheduler/DispSyncSource.cpp

class CallbackRepeater {
public:
    CallbackRepeater(VSyncDispatch& dispatch, VSyncDispatch::Callback cb, const char* name,
                     std::chrono::nanoseconds workDuration, std::chrono::nanoseconds readyDuration,
                     std::chrono::nanoseconds notBefore)
          : mName(name),
            mCallback(cb),
            mRegistration(dispatch, //初始化mRegistration对象
                          std::bind(&CallbackRepeater::callback, this, std::placeholders::_1,
                                    std::placeholders::_2, std::placeholders::_3),
                          mName),
            mStarted(false),
            mWorkDuration(workDuration),
            mReadyDuration(readyDuration),
            mLastCallTime(notBefore) {}
     ...
     VSyncCallbackRegistration mRegistration GUARDED_BY(mMutex); //mRegistration对象
 };
```

```c
/frameworks/native/services/surfaceflinger/Scheduler/EventThread.cpp

EventThread::EventThread(std::unique_ptr<VSyncSource> vsyncSource,
                         android::frametimeline::TokenManager* tokenManager,
                         InterceptVSyncsCallback interceptVSyncsCallback,
                         ThrottleVsyncCallback throttleVsyncCallback,
                         GetVsyncPeriodFunction getVsyncPeriodFunction)
      : mVSyncSource(std::move(vsyncSource)),
        mTokenManager(tokenManager),
        mInterceptVSyncsCallback(std::move(interceptVSyncsCallback)),
        mThrottleVsyncCallback(std::move(throttleVsyncCallback)),
        mGetVsyncPeriodFunction(std::move(getVsyncPeriodFunction)),
        mThreadName(mVSyncSource->getName()) {
    ...
    mVSyncSource->setCallback(this); //注册
    ...
}
```

1. 在初始化DispSyncSource的时候，会创建mCallbackRepeater对象，这个对象需要传入VsyncDispatch和DispSyncSource回调函数。
    
2. 在CallbackRepeater的构造方法中，会创建VsyncCallbackRegistration这个对象，这个对象在创建的时候，会给VsyncDispatch注册回调函数。
    
3. 当VsyncDispatch发送信号的时候，先传递给CallbackRepeater，再传递到DispSyncsource中。
    
4. 当DispSyncSource收到信息会把信号发送到EventThread中。
    

##### 2.1.4 App向EventThread注册Connection

如果有App关心VSYN-APP，则需要向appEventThread注册Connection，可能有多个App同时关注VSYNC-app信号，所以在EventThread的内部有一个mDisplayEventConnections来保存着Connection，Connection是一个Bn对象，因为要与APP进行binder通讯。

```c
 /frameworks/native/services/surfaceflinger/Scheduler/EventThread.h
 
 std::vector<wp<EventThreadConnection>> mDisplayEventConnections GUARDED_BY(mMutex); //app Connection集合
```

```c
status_t EventThread::registerDisplayEventConnection(const sp<EventThreadConnection>& connection) { //注册Connection
    std::lock_guard<std::mutex> lock(mMutex);

    // this should never happen
    auto it = std::find(mDisplayEventConnections.cbegin(),
            mDisplayEventConnections.cend(), connection);
    if (it != mDisplayEventConnections.cend()) {
        ALOGW("DisplayEventConnection %p already exists", connection.get());
        mCondition.notify_all();
        return ALREADY_EXISTS;
    }

    mDisplayEventConnections.push_back(connection);
    mCondition.notify_all();
    return NO_ERROR;
}

void EventThread::removeDisplayEventConnectionLocked(const wp<EventThreadConnection>& connection) { //删除Connection
    auto it = std::find(mDisplayEventConnections.cbegin(),
            mDisplayEventConnections.cend(), connection);
    if (it != mDisplayEventConnections.cend()) {
        mDisplayEventConnections.erase(it);
    }
}
```

以上贴的三段代码，分别是定义Connection的集合对象，往appEventThread注册Connection和删除Connection。

#### 2.2 SF请求VSYNC-sf

##### 2.2.1 invalidate

当应用上帧的时候，也就是当BufferQueue有新的Graphic Buffer到达时，应用会通过binder通讯，调用到SurfaceFlinger的setTransactionState方法，再去调用setTransactionFlags方法，通知SF有新的Graphic Buffer到达：

```c
/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp

void SurfaceFlinger::signalTransaction() {
    mScheduler->resetIdleTimer();
    mPowerAdvisor.notifyDisplayUpdateImminent();
    mEventQueue->invalidate();
}
```

SF的signalTransaction方法中调用MessageQueue的invalidate方法。

```c
/frameworks/native/services/surfaceflinger/Scheduler/MessageQueue.cpp

void MessageQueue::postMessage(sp<MessageHandler>&& handler) {
    mLooper->sendMessage(handler, Message());
}

void MessageQueue::invalidate() {
    ATRACE_CALL();

    {
        std::lock_guard lock(mInjector.mutex);
        if (CC_UNLIKELY(mInjector.connection)) {
            ALOGD("%s while injecting VSYNC", __FUNCTION__);
            mInjector.connection->requestNextVsync();
            return;
        }
    }

    std::lock_guard lock(mVsync.mutex);
    mVsync.scheduled = true;
    mVsync.expectedWakeupTime =
            mVsync.registration->schedule({.workDuration = mVsync.workDuration.get().count(),
                                           .readyDuration = 0,
                                           .earliestVsync = mVsync.lastCallbackTime.count()});
}
```

invalidate方法就是SF去申请一次性的VSYNC。

##### 2.2.2 schedule

上面的代码是通过Vsync结构体的registration对象调用schedule方法。

```c
/frameworks/native/services/surfaceflinger/Scheduler/MessageQueue.h

struct Vsync {
    frametimeline::TokenManager* tokenManager = nullptr;
    std::unique_ptr<scheduler::VSyncCallbackRegistration> registration; //registration对象

    std::mutex mutex;
    TracedOrdinal<std::chrono::nanoseconds> workDuration
            GUARDED_BY(mutex) = {"VsyncWorkDuration-sf", std::chrono::nanoseconds(0)};
    std::chrono::nanoseconds lastCallbackTime GUARDED_BY(mutex) = std::chrono::nanoseconds{0};
    bool scheduled GUARDED_BY(mutex) = false;
    std::optional<nsecs_t> expectedWakeupTime GUARDED_BY(mutex);
    TracedOrdinal<int> value = {"VSYNC-sf", 0};
};
```

间接的调用到VsynDispatch的schedule方法。

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncDispatchTimerQueue.cpp

ScheduleResult VSyncCallbackRegistration::schedule(VSyncDispatch::ScheduleTiming scheduleTiming) {
    if (!mValidToken) {
        return std::nullopt;
    }
    return mDispatch.get().schedule(mToken, scheduleTiming);//发射Vsync信号
}
```

其中的mToken是当初SF注册的到VsyncDispatch的索引，通过mToken可以找到注册到VsyncDispatch中的VsyncDispatchTimerQueueEntry对象，这个对象记录了很多信息，包括回调到SF的函数地址，下一次发送VSYNC信号的时间等等。

##### 2.3 VSYNC-sf产生和发射

从前面的代码可以看到，当应用上帧的时候，SurfaceFlinger就会去申请VSYNC-sf的信号，那申请的VSYNC-sf的信号，什么时候会发给SurfaceFlinger，去做合成的动作。从前面的代码，已经可以看到申请信息的时候，已经调用到VsyncDispatch的schedule的方法。

要了解VSYNC-sf的发射路径，需要仔细阅读VsyncDispatch的子类的实现逻辑，查看VSyncDispatchTimerQueue.cpp的代码如下：

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncDispatchTimerQueue.cpp

ScheduleResult VSyncDispatchTimerQueue::schedule(CallbackToken token,
                                                 ScheduleTiming scheduleTiming) {
    ScheduleResult result;
    {
        std::lock_guard lock(mMutex);

        auto it = mCallbacks.find(token);
        if (it == mCallbacks.end()) {
            return result;
        }
        auto& callback = it->second;
        auto const now = mTimeKeeper->now();

        /* If the timer thread will run soon, we'll apply this work update via the callback
         * timer recalculation to avoid cancelling a callback that is about to fire. */
        auto const rearmImminent = now > mIntendedWakeupTime;
        if (CC_UNLIKELY(rearmImminent)) {
            callback->addPendingWorkloadUpdate(scheduleTiming);
            return getExpectedCallbackTime(mTracker, now, scheduleTiming);
        }

        result = callback->schedule(scheduleTiming, mTracker, now);
        if (!result.has_value()) {
            return result;
        }

        if (callback->wakeupTime() < mIntendedWakeupTime - mTimerSlack) {
            rearmTimerSkippingUpdateFor(now, it);
        }
    }

    return result;
}
```

从上面的代码，token是编号，就是代表sf，app或者appSF注册到VsyncDispatch的索引值，VsyncDispatch中有一个集合记录这三个的回调信息，也就是mCallbacks，这个里面存储了一个对象VsyncDispatchtimerQueueEntry，这个类很关键，它保存了回调的函数指针，回调的名字和两个信号直接的误差值等等。

```c
 /frameworks/native/services/surfaceflinger/Scheduler/VSyncDispatchTimerQueue.h
    
// VSyncDispatchTimerQueueEntry是一个辅助类表示每个实体的内部状态
class VSyncDispatchTimerQueueEntry {
public:
    // This is the state of the entry. There are 3 states, armed, running, disarmed.
    // Valid transition: disarmed -> armed ( when scheduled )
    // Valid transition: armed -> running -> disarmed ( when timer is called)
    // Valid transition: armed -> disarmed ( when cancelled )
    VSyncDispatchTimerQueueEntry(std::string const& name, VSyncDispatch::Callback const& fn,
                                 nsecs_t minVsyncDistance);
    ...

private:
    std::string const mName; //名字
    ...
    struct ArmingInfo { //记录Vsync发射相关的时间信息
        nsecs_t mActualWakeupTime;
        nsecs_t mActualVsyncTime;
        nsecs_t mActualReadyTime;
    };
    std::optional<ArmingInfo> mArmedInfo;
    std::optional<nsecs_t> mLastDispatchTime;
    ...
 };
```

这个类保存了几个关键信息，有一个ArmingInfo是参与计算Vsync唤醒的时间信息。当SurfaceFlinger申请Vsync-sf的信号，从上面的schedule方法传入一个ScheduleTiming结构体。

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncDispatch.h

struct ScheduleTiming {
    nsecs_t workDuration = 0;
    nsecs_t readyDuration = 0;
    nsecs_t earliestVsync = 0;

     bool operator==(const ScheduleTiming& other) const {
         return workDuration == other.workDuration && readyDuration == other.readyDuration &&
                 earliestVsync == other.earliestVsync;
     }

     bool operator!=(const ScheduleTiming& other) const { return !(*this == other); }
 };
```

这个结构体会记录三个值，workDuration，readyDuration。这两个值是固定的，而且这两个值在不同的刷新率下都是不一样的，都是参与计算Vsync信号发射的时间，我们这边只重点关注earliestVsync，这个是上一个Vsync发射的时间。 这个值是很关键的，根据这个值，再通过一个软件模型校准的值，获得下一次Vsync发射的时间值。

前面的schedule方法中，假如是sf的token来申请Vsync信息，会调用callback->schedule这个方法，这个方法很重要，主要是根据上一次的vysnc发射时间计算下一次的Vsync发射时间。

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncDispatchTimerQueue.cpp

ScheduleResult VSyncDispatchTimerQueueEntry::schedule(VSyncDispatch::ScheduleTiming timing,
                                                      VSyncTracker& tracker, nsecs_t now) {
    auto nextVsyncTime = tracker.nextAnticipatedVSyncTimeFrom(
            std::max(timing.earliestVsync, now + timing.workDuration + timing.readyDuration));
    auto nextWakeupTime = nextVsyncTime - timing.workDuration - timing.readyDuration;

    bool const wouldSkipAVsyncTarget =
            mArmedInfo && (nextVsyncTime > (mArmedInfo->mActualVsyncTime + mMinVsyncDistance));
    bool const wouldSkipAWakeup =
            mArmedInfo && ((nextWakeupTime > (mArmedInfo->mActualWakeupTime + mMinVsyncDistance)));
    if (wouldSkipAVsyncTarget && wouldSkipAWakeup) {
        return getExpectedCallbackTime(nextVsyncTime, timing);
    }

    bool const alreadyDispatchedForVsync = mLastDispatchTime &&
            ((*mLastDispatchTime + mMinVsyncDistance) >= nextVsyncTime &&
             (*mLastDispatchTime - mMinVsyncDistance) <= nextVsyncTime);
     if (alreadyDispatchedForVsync) {
         nextVsyncTime =
                 tracker.nextAnticipatedVSyncTimeFrom(*mLastDispatchTime + mMinVsyncDistance);
         nextWakeupTime = nextVsyncTime - timing.workDuration - timing.readyDuration;
     }
 
     auto const nextReadyTime = nextVsyncTime - timing.readyDuration;
     mScheduleTiming = timing;
     mArmedInfo = {nextWakeupTime, nextVsyncTime, nextReadyTime};
     return getExpectedCallbackTime(nextVsyncTime, timing);
 }
```

这个是最核心的逻辑，从上面代码可以看到下面几个点的逻辑顺序。

1. 先传入之前的Vsync发射的时间，timeing这个对象，就是sf上一次发射信息的时间信息，这边有三个值，workDuration和readyDuration的值在不同刷新率下是不一样的，而且sf和app的配置也是不一样的，这两个值在参与计算值感觉只是一个阈值，并没有什么实际作用。 我们先举例这两个值都是0，在解释下上面的代码，我们先判断当前时间和上一次Vsync-sf发射的时间的最大值。
2. 把最大值传递个VsyncTracker中的nextAnticipatedVsyncTimeFrom方法中，从传入的参数根据这个方法名字，可以获得下一次Vsync发射的时间，如果获取的发射时间大于mArmedInfo中记录的上一次发射的时间，需要把这次的申请的发射时间跳过不处理，还是用之前的发射时间。
3. 如果还记录着最后一次vsync发射的时间，这个时间和下一次vsync发射的时间在一定的误差之中，重新校正下一次发发射时间，拿上一次最后发射的时间传到VsyncTracker中，获取下一次Vsync发射时间。
4. 然后把发射时间，减去固定的值，保存到mArmedInfo中，用于后面的设置定时器。

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncDispatchTimerQueue.cpp

if (callback->wakeupTime() < mIntendedWakeupTime - mTimerSlack) {
    rearmTimerSkippingUpdateFor(now, it);//发射Vsync信号
}
```

这个方法执行完毕之后，会判断下一次发射的时间，和上一次设置的发射的时间做比较，如果小于这个值，需要把最近的发射时间重新设置到定时器中，这个mIntendedWakeupTiem变量在每次正常发射之后，这个值通常会设置为默认值，是int 8个字节的最大值 9223372036854775807，所以通常就会走到rearmTimerSkippingUpdateFor的函数中。

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncDispatchTimerQueue.cpp

void VSyncDispatchTimerQueue::rearmTimerSkippingUpdateFor(
        nsecs_t now, CallbackMap::iterator const& skipUpdateIt) {
    std::optional<nsecs_t> min;
    std::optional<nsecs_t> targetVsync;
    std::optional<std::string_view> nextWakeupName;
    for (auto it = mCallbacks.begin(); it != mCallbacks.end(); it++) {
        auto& callback = it->second;
        if (!callback->wakeupTime() && !callback->hasPendingWorkloadUpdate()) {
            continue;
        }

        if (it != skipUpdateIt) {
            callback->update(mTracker, now);
        }
        auto const wakeupTime = *callback->wakeupTime();
        if (!min || (min && *min > wakeupTime)) {
            nextWakeupName = callback->name();
            min = wakeupTime;
            targetVsync = callback->targetVsync();
        }
    }

    if (min && (min < mIntendedWakeupTime)) {
        if (targetVsync && nextWakeupName) {
            mTraceBuffer.note(*nextWakeupName, *min - now, *targetVsync - now);
        }
        setTimer(*min, now);//调用Timer的定时器方法
    } else {
        ATRACE_NAME("cancel timer");
        cancelTimer();
    }
}
```

从上面的函数中，可以很明显的看出来，SurfaceFlinger申请的Vsync-sf发射时间，把下一次唤醒的时间传入这个函数中，首先在mCallbacks中查找有没有发现发射更早的时间，假如app申请的发射时间在处理中，如果传过来的是Vsync-sf的发射时间，会把app或者appSf的发射时间更新下，然后从中找一个最近的，最快的发射时间设置到定时器中。

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncDispatchTimerQueue.cpp

void VSyncDispatchTimerQueue::setTimer(nsecs_t targetTime, nsecs_t /*now*/) {
    mIntendedWakeupTime = targetTime;
    mTimeKeeper->alarmAt(std::bind(&VSyncDispatchTimerQueue::timerCallback, this),
                         mIntendedWakeupTime);
    mLastTimerSchedule = mTimeKeeper->now();
}
```

定时器就是前面介绍的mTimer，我们把下次发射的时间设置到定时器中，会在对应的时间内回调到VsynDispatchTimerQueue的timerCallback方法中。然后把最近的一次发射时间设置给mIntendedWakerupTime这个变量。

假如Vsync-sf的定时器设置给Timer之后，接下来就是Vsync-sf的发射过程，假如Timer的定时器到时间之后，会调用到VsynDispatchTimerQueue的timerCallback中，这个timerCallback方法很重要。是分发SW-VSYNC的地方。

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncDispatchTimerQueue.cpp

void VSyncDispatchTimerQueue::timerCallback() {
    struct Invocation {
        std::shared_ptr<VSyncDispatchTimerQueueEntry> callback;
        nsecs_t vsyncTimestamp;
        nsecs_t wakeupTimestamp;
        nsecs_t deadlineTimestamp;
    };
    std::vector<Invocation> invocations;
    {
        std::lock_guard lock(mMutex);
        auto const now = mTimeKeeper->now();
        mLastTimerCallback = now;
        for (auto it = mCallbacks.begin(); it != mCallbacks.end(); it++) {
            auto& callback = it->second;
            auto const wakeupTime = callback->wakeupTime();
            if (!wakeupTime) {
                continue;
            }

            auto const readyTime = callback->readyTime();

            auto const lagAllowance = std::max(now - mIntendedWakeupTime, static_cast<nsecs_t>(0));
            if (*wakeupTime < mIntendedWakeupTime + mTimerSlack + lagAllowance) {
                callback->executing();
                invocations.emplace_back(Invocation{callback, *callback->lastExecutedVsyncTarget(),
                                                    *wakeupTime, *readyTime});
            }
        }

        mIntendedWakeupTime = kInvalidTime;
        rearmTimer(mTimeKeeper->now());
    }

    for (auto const& invocation : invocations) { //分发Vsync事件
        invocation.callback->callback(invocation.vsyncTimestamp, invocation.wakeupTimestamp,
                                      invocation.deadlineTimestamp);
    }
}
```

从上面的代码流程中，可以看到当发射的时间回调这个方法中，会在mCallbacks的集合中查找符合这次发射的时间的匹配者， 先判断该对象中的发射时间是否有效，如果有效的话，获取当前的时间信息和发射时间的差值。因为设置给定时器的唤醒时间，和当前时间按理是一致的，但是因为软件实现肯定是有偏差值的，所以拿发射的时间值，和真正的发射的时间值有个校验。如果符合发射的时间，则把需要发射的对象放到invocation的集合中。然后遍历这个集合挨个把Vsync信号发射给对应的代码。

##### 2.4 Vsync-app的申请和发射

###### 2.4.1 应用向surfaceflinger注册connection

前面讲了Vsync-sf的发射，为什么这两块要分开说，因为再Android S版本之前的版本，Vsync-app和Vsync-sf都是EventThread的形式，在12版本上Vsync-sf的逻辑去掉EventThread的形式，谷歌做了重构，所以就剩下Vsync-app还是采用EventThead的形式。

接下来我们讲下应用怎么去申请Vsync-app的信号，本章节主要讲解SurfaceFlinger里面的逻辑，针对应用怎么申请Vsync-app信息，简单的说下，就是通过Choreographer这个对象去申请Vsync-app的信号，然后通过其内部类FrameDisplayEventReceiver来接受vsync信号，也就是Vsync-app的发射最后到这个对象里面，来触发app刷新，核心就是FrameDisplayEventReceiver类，这个类的初始化在是Choreographer的构造函数中。

```java
/frameworks/base/core/java/android/view/Choreographer.java


private Choreographer(Looper looper, int vsyncSource) {
    mLooper = looper;
    mHandler = new FrameHandler(looper);
    mDisplayEventReceiver = USE_VSYNC
            ? new FrameDisplayEventReceiver(looper, vsyncSource)
            : null;
    mLastFrameTimeNanos = Long.MIN_VALUE;

    mFrameIntervalNanos = (long)(1000000000 / getRefreshRate());

    mCallbackQueues = new CallbackQueue[CALLBACK_LAST + 1];
    for (int i = 0; i <= CALLBACK_LAST; i++) {
        mCallbackQueues[i] = new CallbackQueue();
    }
    // b/68769804: For low FPS experiments.
    setFPSDivisor(SystemProperties.getInt(ThreadedRenderer.DEBUG_FPS_DIVISOR, 1));
}
```

FrameDisplayEventReceiver继承DisplayEventReceiver，在DisplayEventReceiver的构造方法中，调用nativeInit方法。

```java
/frameworks/base/core/java/android/view/DisplayEventReceiver.java

106      public DisplayEventReceiver(Looper looper, int vsyncSource, int eventRegistration) {
107          if (looper == null) {
108              throw new IllegalArgumentException("looper must not be null");
109          }
110  
111          mMessageQueue = looper.getQueue();
112          mReceiverPtr = nativeInit(new WeakReference<DisplayEventReceiver>(this), mMessageQueue,
113                  vsyncSource, eventRegistration);
114      }
```

这个方法会在初始化NativeDisplayEventReceiver对象，NativeDisplayEventReceiver对象继承DisplayEventDispatcher对象，这个对象在初始化的时候，会初始化mReceiver对象，初始化这个mReceiver对象的时候会创建DisplayEventReceiver对象。

```c
/frameworks/native/libs/gui/DisplayEventReceiver.cpp

DisplayEventReceiver::DisplayEventReceiver(
        ISurfaceComposer::VsyncSource vsyncSource,
        ISurfaceComposer::EventRegistrationFlags eventRegistration) {
    sp<ISurfaceComposer> sf(ComposerService::getComposerService());
    if (sf != nullptr) {
        mEventConnection = sf->createDisplayEventConnection(vsyncSource, eventRegistration);
        if (mEventConnection != nullptr) {
            mDataChannel = std::make_unique<gui::BitTube>();
            mEventConnection->stealReceiveChannel(mDataChannel.get());
        }
    }
}
```

```c
/frameworks/native/services/surfaceflinger/Scheduler/EventThread.cpp

EventThreadConnection::EventThreadConnection(
        EventThread* eventThread, uid_t callingUid, ResyncCallback resyncCallback,
        ISurfaceComposer::EventRegistrationFlags eventRegistration)
      : resyncCallback(std::move(resyncCallback)),
        mOwnerUid(callingUid),
        mEventRegistration(eventRegistration),
        mEventThread(eventThread),
        mChannel(gui::BitTube::DefaultSize) {}

status_t EventThreadConnection::stealReceiveChannel(gui::BitTube* outChannel) {
    outChannel->setReceiveFd(mChannel.moveReceiveFd());
    outChannel->setSendFd(base::unique_fd(dup(mChannel.getSendFd())));
    return NO_ERROR;
}

void EventThreadConnection::onFirstRef() {
    // NOTE: mEventThread doesn't hold a strong reference on us
    mEventThread->registerDisplayEventConnection(this);
}
```

这个构造方法中有很重要的步骤，具体如下：

1. 获取SurfaceFlinger的binder代理对象BpSurfaceComposer，就可以调用SurfaceFlinger binder服务端的接口
    
2. 调用SurfaceFlinger的binder接口创建一个connection，这个connection就是注册到EventThread中，用来判断是不是要接受Vsync-app信号。
    
    在SurfaceFlinger创建这个这个connection是会走到EventThread的createEventConnection，在EventThreadConnection的构造方法中会创建一个sockert对象。这个mEventConnection也是一个binder对象，IDisplayEventConnection，SurfaceFlinger进程返回BpDisplayEventConnection赋值给mEventConection。而服务端就是EventThreadConnection。
    
3. 在DisplayEventReceiver构造方法中也会创建一个空的gui::BitTubet对象，并且调用connection的binder接口，把socket对象设置到EventThreadConnection对象中，这个操作就是把两边关联起来，从代码实现可以看出是讲SurfaceFlinger进程中服务端创建的gui::BitTube对象赋值给应用端空的gui::BitTube对象。
    
4. 然后EventThreadConnection初始化好之后，在第一次引用调用的时候，会把自己注册到EventThread的集合中mDisplayEventConnections。
    

###### 2.4.2 Vsync-app的申请和发射

接下来我们主要讲解app怎么向SurfaceFlinger申请Vsync-app的，然后Vsync-app的信号怎么发射到应用的。

正常应用要申请Vsync信号，都是通过Choregrapher对象调用postFrameCallback方法，而应用在绘制的时候也会调用这个方法，就是ViewRootImpl中的scheduleTraversals方法，其实在函数实现中也是调用了Choregrapher的postFrameCallback方法。

而postFrameCallback方法其实是调用Choreographer的scheduleFameLocked方法，调用到scheduleVsyncLocked方法，在调用到NativeDisplayEventReceiver的scheduleVsync方法中。因为继承关系查看DisplayEventDispather的scheduleVsync方法，可以看到是通过DisplayEventReceiver去请求下一个Vsync信号。

我们看下DisplayEventReceiver的requestNextVsync方法

```c
 /frameworks/native/libs/gui/DisplayEventReceiver.cpp

status_t DisplayEventReceiver::requestNextVsync() {
    if (mEventConnection != nullptr) {
        mEventConnection->requestNextVsync();
        return NO_ERROR;
    }
    return NO_INIT;
}
```

会调用到mEeventConnection的requestNextVsync接口，mEventConnection是binder的代理，最终会调用到SurfaceFlinger进程的binder服务端EventThreadConnection的requestNextVsync，接下来就是申请Vsyn-app信号，SurfaceFlinger模块的代码逻辑了。

```c
/frameworks/native/services/surfaceflinger/Scheduler/EventThread.cpp

void EventThread::requestNextVsync(const sp<EventThreadConnection>& connection) {
    if (connection->resyncCallback) {
        connection->resyncCallback();
    }

    std::lock_guard<std::mutex> lock(mMutex);

    if (connection->vsyncRequest == VSyncRequest::None) {
        connection->vsyncRequest = VSyncRequest::Single;//申请Vsync app的信号
        mCondition.notify_all();
    } else if (connection->vsyncRequest == VSyncRequest::SingleSuppressCallback) {
        connection->vsyncRequest = VSyncRequest::Single;
    }
}
```

从代码中可以看出来，会把当前的申请Vsync-app的Connection的vsyncRequest赋值为 VsyncRequest::Single。我们可以理解一个应用就代表一个Connection。

如果某个应用的申请了Vsync-app信号，就会把对应的EventThreadConnection对象中的vsyncRequest变量进行重新赋值。

接下来看看EventThread如何处理，我们要从EventThread的线程函数看起：

```c
/frameworks/native/services/surfaceflinger/Scheduler/EventThread.cpp

void EventThread::threadMain(std::unique_lock<std::mutex>& lock) {
    DisplayEventConsumers consumers;

    while (mState != State::Quit) {
        std::optional<DisplayEventReceiver::Event> event;

        // Determine next event to dispatch.
        if (!mPendingEvents.empty()) { //有Vsync信号
            event = mPendingEvents.front();
            mPendingEvents.pop_front();

            switch (event->header.type) {
                case DisplayEventReceiver::DISPLAY_EVENT_HOTPLUG:
                    if (event->hotplug.connected && !mVSyncState) {
                        mVSyncState.emplace(event->header.displayId);
                    } else if (!event->hotplug.connected && mVSyncState &&
                               mVSyncState->displayId == event->header.displayId) {
                        mVSyncState.reset();
                    }
                    break;

                case DisplayEventReceiver::DISPLAY_EVENT_VSYNC:
                    if (mInterceptVSyncsCallback) {
                        mInterceptVSyncsCallback(event->header.timestamp);
                    }
                    break;
            }
        }

        bool vsyncRequested = false;

        // Find connections that should consume this event.
        auto it = mDisplayEventConnections.begin();
        while (it != mDisplayEventConnections.end()) {
            if (const auto connection = it->promote()) {
                vsyncRequested |= connection->vsyncRequest != VSyncRequest::None;

                if (event && shouldConsumeEvent(*event, connection)) {
                    consumers.push_back(connection);
                }

                ++it;
            } else {
                it = mDisplayEventConnections.erase(it);
            }
        }

        if (!consumers.empty()) { //申请Vsync app的信号集合不为空
            dispatchEvent(*event, consumers);
            consumers.clear();
        }

        State nextState;
        if (mVSyncState && vsyncRequested) {
            nextState = mVSyncState->synthetic ? State::SyntheticVSync : State::VSync;
        } else {
            ALOGW_IF(!mVSyncState, "Ignoring VSYNC request while display is disconnected");
            nextState = State::Idle;
        }

        if (mState != nextState) {
            if (mState == State::VSync) {
                mVSyncSource->setVSyncEnabled(false);
            } else if (nextState == State::VSync) {
                mVSyncSource->setVSyncEnabled(true);
            }

            mState = nextState;
        }

        if (event) {
            continue;
        }

        // Wait for event or client registration/request.
        if (mState == State::Idle) {
            mCondition.wait(lock);
        } else {
            // Generate a fake VSYNC after a long timeout in case the driver stalls. When the
            // display is off, keep feeding clients at 60 Hz.
            const std::chrono::nanoseconds timeout =
                    mState == State::SyntheticVSync ? 16ms : 1000ms;
            if (mCondition.wait_for(lock, timeout) == std::cv_status::timeout) {
                if (mState == State::VSync) {
                    ALOGW("Faking VSYNC due to driver stall for thread %s", mThreadName);
                    std::string debugInfo = "VsyncSource debug info:\n";
                    mVSyncSource->dump(debugInfo);
                    // Log the debug info line-by-line to avoid logcat overflow
                    auto pos = debugInfo.find('\n');
                    while (pos != std::string::npos) {
                        ALOGW("%s", debugInfo.substr(0, pos).c_str());
                        debugInfo = debugInfo.substr(pos + 1);
                        pos = debugInfo.find('\n');
                    }
                }

                LOG_FATAL_IF(!mVSyncState);
                const auto now = systemTime(SYSTEM_TIME_MONOTONIC);
                const auto deadlineTimestamp = now + timeout.count();
                const auto expectedVSyncTime = deadlineTimestamp + timeout.count();
                const int64_t vsyncId = [&] {
                    if (mTokenManager != nullptr) {
                        return mTokenManager->generateTokenForPredictions(
                                {now, deadlineTimestamp, expectedVSyncTime});
                    }
                    return FrameTimelineInfo::INVALID_VSYNC_ID;
                }();
                mPendingEvents.push_back(makeVSync(mVSyncState->displayId, now,
                                                   ++mVSyncState->count, expectedVSyncTime,
                                                   deadlineTimestamp, vsyncId));
            }
        }
    }
}
```

EventThread的线程函数循环调用，一方面检测是否有Vsync信号发送过来了mPendingEvent，一方面检查是否有app请求了Vsync信号，如果有Vsync信号，而且有app请求了Vsync，则通过Connection把Vsync事件发送到对端。

从代码的的细节可以看出几个点：

1. 检查是否有VsyncDispatch是否发送Vsync过来，所以要要遍历mPendingEvent
2. 检查是否有app对Vsync感兴趣，所以要遍历EventThread的mDisplayEventConnections。
3. 如果有Vsyn事件过来，但是没人对它感兴趣，说们本次Vsync就可以关闭了，见上面的mVsyncSource->setVsyncEnabled(false)方法。
4. 如果有app申请了Vsync，但是没有接受到Vsync事件，可能是把之前的Vsync关了，所以要从新打开，并坐等下次Vsync的到来，但是为了保证安全，不能死等，所以设置一个timeout的时间。

###### 2.4.3 setVsyncEnabled

这个方法是开关Vsync-app信号的函数，从这个函数的实现，是间接调用mCallbackRepeater的start和stop方法。而CallbackRepeater是在创建DispSyncSource对象构造方法中创建的。

```c
/frameworks/native/services/surfaceflinger/Scheduler/DispSyncSource.cpp

DispSyncSource::DispSyncSource(scheduler::VSyncDispatch& vSyncDispatch,
                               std::chrono::nanoseconds workDuration,
                               std::chrono::nanoseconds readyDuration, bool traceVsync,
                               const char* name)
      : mName(name),
        mValue(base::StringPrintf("VSYNC-%s", name), 0),
        mTraceVsync(traceVsync),
        mVsyncOnLabel(base::StringPrintf("VsyncOn-%s", name)),
        mWorkDuration(base::StringPrintf("VsyncWorkDuration-%s", name), workDuration),
        mReadyDuration(readyDuration) {
    mCallbackRepeater =
            std::make_unique<CallbackRepeater>(vSyncDispatch,
                                               std::bind(&DispSyncSource::onVsyncCallback, this,
                                                         std::placeholders::_1,
                                                         std::placeholders::_2,
                                                         std::placeholders::_3),
                                               name, workDuration, readyDuration,
                                               std::chrono::steady_clock::now().time_since_epoch());
}
```

可以看出CallbackRepeater对象传入了几个参数，一个是VsyncDispatch对象，一个回调的函数，是为了接受Vsync-app发射的信号。而在CallbackRepeater对象中的构造方法会把CallbackRepeater的回调函数，初始化VsyncCallbackRegistration，这个是一个辅助类，在构造方法中会在VsyncDispatch注册回调函数和回调的名字等信息。可以这样理解，DispSyncSource是EventThread和VsyncDispatch的纽带。

DispsyncSource中，VsyncCallbackRegistration是一个辅助类主要是帮助VsyncDispatch注册回调函数而且。

所以app申请Vsycn-app信号，调用DispVsynSource的setVsyncEnabled的函数，是间接调用CallbackRepeater的start的函数，就是这个类封装了VsyncDispatch的操作，也就是调用VsyncDispatch的schedule函数。

```c
/frameworks/native/services/surfaceflinger/Scheduler/DispSyncSource.cpp

void start(std::chrono::nanoseconds workDuration, std::chrono::nanoseconds readyDuration) {
    std::lock_guard lock(mMutex);
    mStarted = true;
    mWorkDuration = workDuration;
    mReadyDuration = readyDuration;

    auto const scheduleResult =
            mRegistration.schedule({.workDuration = mWorkDuration.count(),
                                    .readyDuration = mReadyDuration.count(),
                                    .earliestVsync = mLastCallTime.count()});
    LOG_ALWAYS_FATAL_IF((!scheduleResult.has_value()), "Error scheduling callback");
}
```

从前面讲解Vsync-sf的申请和发射，我们知道了这个schedule函数是请求Vsync-app信号的函数，这块代码和Vsync-sf的申请是一样的，就是计算下一次Vsync-app唤醒的时间，通过timer机制，把这个Vsync-app信号回调到注册到VsyncDiaptch的函数。

从Vsync-app的申请来看，最后会回调到CallbackRepeater的callback函数中，在这个函数中会调用mCallback函数，而这个函数的回调方法是DispSyncSource中的onVysncCallback函数中。

```c
/frameworks/native/services/surfaceflinger/Scheduler/DispSyncSource.cpp

void DispSyncSource::onVsyncCallback(nsecs_t vsyncTime, nsecs_t targetWakeupTime,
                                     nsecs_t readyTime) {
    VSyncSource::Callback* callback;
    {
        std::lock_guard lock(mCallbackMutex);
        callback = mCallback;
    }

    if (mTraceVsync) {
        mValue = (mValue + 1) % 2; //Vsync-app的trace节点
    }

    if (callback != nullptr) {
        callback->onVSyncEvent(targetWakeupTime, vsyncTime, readyTime);
    }
}
```

在这个函数中，首先会在Vsync-app的trace上标记信息，也即是开头那张图片的信息，所以为什么是断断续续的，是因为Vsync-app申请本来就是随机的。

然后调用callback的onVysncEvent函数，而callback就是EventThread对象，最终调用到EventThread的onVsyncEvent中。

```c
/frameworks/native/services/surfaceflinger/Scheduler/EventThread.cpp

void EventThread::onVSyncEvent(nsecs_t timestamp, nsecs_t expectedVSyncTimestamp,
                               nsecs_t deadlineTimestamp) {
    std::lock_guard<std::mutex> lock(mMutex);

    LOG_FATAL_IF(!mVSyncState);
    const int64_t vsyncId = [&] {
        if (mTokenManager != nullptr) {
            return mTokenManager->generateTokenForPredictions(
                    {timestamp, deadlineTimestamp, expectedVSyncTimestamp});
        }
        return FrameTimelineInfo::INVALID_VSYNC_ID;
    }();

    mPendingEvents.push_back(makeVSync(mVSyncState->displayId, timestamp, ++mVSyncState->count,
                                       expectedVSyncTimestamp, deadlineTimestamp, vsyncId));
    mCondition.notify_all();
}
```

从代码中可以看到，Vsync-app的信号加入到mPendingEvents中，然后唤醒theadMain的线程循环，然后找到对应的申请的应用，然后调用dispatchEvent函数

```c
/frameworks/native/services/surfaceflinger/Scheduler/EventThread.cpp

void EventThread::dispatchEvent(const DisplayEventReceiver::Event& event,
                                const DisplayEventConsumers& consumers) {
    for (const auto& consumer : consumers) {
        DisplayEventReceiver::Event copy = event;
        if (event.header.type == DisplayEventReceiver::DISPLAY_EVENT_VSYNC) {
            copy.vsync.frameInterval = mGetVsyncPeriodFunction(consumer->mOwnerUid);
        }
        switch (consumer->postEvent(copy)) {
            case NO_ERROR:
                break;

            case -EAGAIN:
                // TODO: Try again if pipe is full.
                ALOGW("Failed dispatching %s for %s", toString(event).c_str(),
                      toString(*consumer).c_str());
                break;

            default:
                // Treat EPIPE and other errors as fatal.
                removeDisplayEventConnectionLocked(consumer);
        }
    }
}
```

遍历DisplayEventConsumers的对象，挨个调用postEvent方法。

```c
 /frameworks/native/services/surfaceflinger/Scheduler/EventThread.h
 
 using DisplayEventConsumers = std::vector<sp<EventThreadConnection>>;
```

这个DisplayEventConsumers就是connection的vector集合对象，然后通过connection对象把Vsync事件发送出去。后面应用怎么接受到这个Vsync-app的信号，本章节就不分析，大家有兴趣的话可以自己下来了解下。

#### 3 SW VSYNC模型和校准

在Android S之前的版本，开关硬件VSync开关是有一个线程都做的，在12版本上面都已经做了重构。

##### 3.1 resyncToHardwareVsync

在前面的根据上一次的发射时间获取下一次的发射时间，调用VsyncTracker的nextAnticipatedVsyncTimeFrom方法中。在这个模型中，我们要关注几个核心参数：

- period: VSYNC周期
    
- mTimestamps: 硬件的时间戳样本集合
    

在开机的时候，SurfaceFlinger在初始化Display之后，会调用resyncToHardwareVsync方法与硬件VSYNC进行同步，调用链如下：

```c
SurfaceFlinger::init()
 └-->initializeDisplays()
      └-->onInitializeDisplays()
           └-->setPowerModeInternal()
                 └-->resyncToHardwareVsync()
```

resyncToHardwareVsync的代码如下：

```c
/frameworks/native/services/surfaceflinger/Scheduler/Scheduler.cpp

void Scheduler::resyncToHardwareVsync(bool makeAvailable, nsecs_t period) {
    {
        std::lock_guard<std::mutex> lock(mHWVsyncLock);
        if (makeAvailable) {
            mHWVsyncAvailable = makeAvailable;
        } else if (!mHWVsyncAvailable) {
            // Hardware vsync is not currently available, so abort the resync
            // attempt for now
            return;
        }
    }

    if (period <= 0) {
        return;
    }

    setVsyncPeriod(period);
}
```

makeAvailable默认传入true，period传入的是当前屏幕刷新率的周期值，这个在SurfaceFlinger初始化的时候，把硬件支持的帧率和周期都一对一保存起来，例如fps是60，period是16.666666。fps是90，period是11.111111。再调用到setVsyncPeriod，从这个方法名字可以看到，当屏幕的刷新率发生变化，软件模型肯定要重新同步硬件的时间戳信息，重新计算当前屏幕刷新率对应的period值。

```c
/frameworks/native/services/surfaceflinger/Scheduler/Scheduler.cpp

void Scheduler::setVsyncPeriod(nsecs_t period) {
    std::lock_guard<std::mutex> lock(mHWVsyncLock);
    mVsyncSchedule.controller->startPeriodTransition(period);

    if (!mPrimaryHWVsyncEnabled) {
        mVsyncSchedule.tracker->resetModel();
        mSchedulerCallback.setVsyncEnabled(true);
        mPrimaryHWVsyncEnabled = true;
    }
}
```

mPrimaryHWVsyncEnabled这个变量默认为false，就会走到下面的逻辑中，resetModel方法会清空软件模型的记录的硬件时间戳集合，setVsyncEnabled方法把硬件回调给SurfaceFlinger的开关打开，这个回调方法打开之后，硬件的Vsync信息会通过回调接口通知给SurfaceFlinger，在这个回调接口中，会把硬件的Vsync信息保存到VsyncTracker中。

```c
/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp

void SurfaceFlinger::onComposerHalVsync(hal::HWDisplayId hwcDisplayId, int64_t timestamp,
                                        std::optional<hal::VsyncPeriodNanos> vsyncPeriod) {
    ATRACE_CALL();

    Mutex::Autolock lock(mStateLock);

    if (const auto displayId = getHwComposer().toPhysicalDisplayId(hwcDisplayId)) {
        auto token = getPhysicalDisplayTokenLocked(*displayId);
        auto display = getDisplayDeviceLocked(token);
        display->onVsync(timestamp);
    }

    if (!getHwComposer().onVsync(hwcDisplayId, timestamp)) {
        return;
    }

    if (hwcDisplayId != getHwComposer().getInternalHwcDisplayId()) {
        // For now, we don't do anything with external display vsyncs.
        return;
    }

    bool periodFlushed = false;
    mScheduler->addResyncSample(timestamp, vsyncPeriod, &periodFlushed); //增加硬件Vsync样本
    if (periodFlushed) {
        modulateVsync(&VsyncModulator::onRefreshRateChangeCompleted);
    }
}
```

如代码所示，mScheduler->addResyncSample方法把硬件的时间戳信息timestamp保存起来。

等于上面的代码干了三件事情：

1. 首先从HWC获取到硬件VSYNC的周期period，设置给VsyncController中。
2. VsyncTracker先清理之前记录的采样信息，准备开始硬件VSYNC采样
3. 通过mSchedulerCallback的setVsyncEnabled方法打开硬件VSYNC事件上报

相关代码如下： resetModel方法

```c
//frameworks/native/services/surfaceflinger/Scheduler/VSyncPredictor.cpp

void VSyncPredictor::resetModel() {
    std::lock_guard lock(mMutex);
    mRateMap[mIdealPeriod] = {mIdealPeriod, 0};
    clearTimestamps();
}

void VSyncPredictor::clearTimestamps() {
    if (!mTimestamps.empty()) {
        auto const maxRb = *std::max_element(mTimestamps.begin(), mTimestamps.end());
        if (mKnownTimestamp) {
            mKnownTimestamp = std::max(*mKnownTimestamp, maxRb);
        } else {
            mKnownTimestamp = maxRb;
        }

        mTimestamps.clear();
        mLastTimestampIndex = 0;
    }
}
```

会清空mRateMap对应period的value对象，这个是一个结构体Model，会记录软件模型计算出来的Vsync周期。

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncPredictor.h

struct Model {
    nsecs_t slope;
    nsecs_t intercept;
};
```

##### 3.2 SW VSYNC模型更新与校准

前面已经把硬件的VSYNC回调打开了，那么每次HW VSYNC事件上报时，会调用Schedule的 addResyncSample方法，也就是会调用VsyncController中的addHwVsynctimestamp，从方法的名字可以看出，把硬件VSYNC的时间戳信息添加这个对象中。

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncReactor.cpp

bool VSyncReactor::addHwVsyncTimestamp(nsecs_t timestamp, std::optional<nsecs_t> hwcVsyncPeriod,
                                       bool* periodFlushed) {
    assert(periodFlushed);

    std::lock_guard lock(mMutex);
    if (periodConfirmed(timestamp, hwcVsyncPeriod)) {
        ATRACE_NAME("VSR: period confirmed");
        if (mPeriodTransitioningTo) {
            mTracker.setPeriod(*mPeriodTransitioningTo);
            *periodFlushed = true;
        }

        if (mLastHwVsync) {
            mTracker.addVsyncTimestamp(*mLastHwVsync);
        }
        mTracker.addVsyncTimestamp(timestamp);

        endPeriodTransition();
        mMoreSamplesNeeded = mTracker.needsMoreSamples();
    } else if (mPeriodConfirmationInProgress) {
        ATRACE_NAME("VSR: still confirming period");
        mLastHwVsync = timestamp;
        mMoreSamplesNeeded = true;
        *periodFlushed = false;
    } else {
        ATRACE_NAME("VSR: adding sample");
        *periodFlushed = false;
        mTracker.addVsyncTimestamp(timestamp);
        mMoreSamplesNeeded = mTracker.needsMoreSamples();
    }

    if (!mMoreSamplesNeeded) {
        setIgnorePresentFencesInternal(false);
    }
    return mMoreSamplesNeeded;
}
```

这个函数有三个操作，首先会把当前的硬件上报的时间戳信息和当前的屏幕刷新率对于的固定period传入periodConfirmed方法中。这个periodConfirmed，就是确认是否有新的period设置进来，就是有没有发生屏幕刷新率切换。如果没有发生切换，这个函数默认返回false，如果没有发生刷新率切换，就是在保持同一个刷新率的情况下，最后走到else的逻辑中。也就是把timestamp这个变量添加到VsyncTracker对象中，然后调用该对象的needsMoreSamples方法判断要不要更多的样本，这边默认是6个样本，所以如果样本个数还没有达到，是需要一直增加样本到6个。就不需要样本了，就会把HW SYNC的硬件上报开关关闭掉。

可以说做2件事情：

1. mTracker.addVsyncTimestamp方法，把样本加入到VsycnTracker的子类VsyncPredictor对象中。
2. 通过needsMoreSamples方法，判断要不要获取更多的样本，如果样本足够，调用schedule的disableHardwareVsync函数，关闭硬件校准上报开关。

##### 3.2 1 addVsyncTimestamp

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncPredictor.cpp

bool VSyncPredictor::addVsyncTimestamp(nsecs_t timestamp) {
    std::lock_guard lock(mMutex);

    if (!validate(timestamp)) {
        // VSR could elect to ignore the incongruent timestamp or resetModel(). If ts is ignored,
        // don't insert this ts into mTimestamps ringbuffer. If we are still
         // in the learning phase we should just clear all timestamps and start
         // over.
         if (mTimestamps.size() < kMinimumSamplesForPrediction) {
             // Add the timestamp to mTimestamps before clearing it so we could
             // update mKnownTimestamp based on the new timestamp.
             mTimestamps.push_back(timestamp);
             clearTimestamps();
         } else if (!mTimestamps.empty()) {
             mKnownTimestamp =
                     std::max(timestamp, *std::max_element(mTimestamps.begin(), mTimestamps.end()));
         } else {
             mKnownTimestamp = timestamp;
         }
         return false;
     }
 
     if (mTimestamps.size() != kHistorySize) {
         mTimestamps.push_back(timestamp);
         mLastTimestampIndex = next(mLastTimestampIndex);
     } else {
         mLastTimestampIndex = next(mLastTimestampIndex);
         mTimestamps[mLastTimestampIndex] = timestamp;
     }
 
     if (mTimestamps.size() < kMinimumSamplesForPrediction) {
         mRateMap[mIdealPeriod] = {mIdealPeriod, 0};
         return true;
     }
 
     // This is a 'simple linear regression' calculation of Y over X, with Y being the
     // vsync timestamps, and X being the ordinal of vsync count.
     // The calculated slope is the vsync period.
     // Formula for reference:
     // Sigma_i: means sum over all timestamps.
     // mean(variable): statistical mean of variable.
     // X: snapped ordinal of the timestamp
     // Y: vsync timestamp
     //
     //         Sigma_i( (X_i - mean(X)) * (Y_i - mean(Y) )
     // slope = -------------------------------------------
     //         Sigma_i ( X_i - mean(X) ) ^ 2
     //
     // intercept = mean(Y) - slope * mean(X)
     //
     std::vector<nsecs_t> vsyncTS(mTimestamps.size());
     std::vector<nsecs_t> ordinals(mTimestamps.size());
 
     // normalizing to the oldest timestamp cuts down on error in calculating the intercept.
     auto const oldest_ts = *std::min_element(mTimestamps.begin(), mTimestamps.end());
     auto it = mRateMap.find(mIdealPeriod);
     auto const currentPeriod = it->second.slope;
     // TODO (b/144707443): its important that there's some precision in the mean of the ordinals
     //                     for the intercept calculation, so scale the ordinals by 1000 to continue
     //                     fixed point calculation. Explore expanding
     //                     scheduler::utils::calculate_mean to have a fixed point fractional part.
     static constexpr int64_t kScalingFactor = 1000;
 
     for (auto i = 0u; i < mTimestamps.size(); i++) {
         traceInt64If("VSP-ts", mTimestamps[i]);
 
         vsyncTS[i] = mTimestamps[i] - oldest_ts;
         ordinals[i] = ((vsyncTS[i] + (currentPeriod / 2)) / currentPeriod) * kScalingFactor;
     }
 
     auto meanTS = scheduler::calculate_mean(vsyncTS);
     auto meanOrdinal = scheduler::calculate_mean(ordinals);
     for (size_t i = 0; i < vsyncTS.size(); i++) {
         vsyncTS[i] -= meanTS;
         ordinals[i] -= meanOrdinal;
     }
 
     auto top = 0ll;
     auto bottom = 0ll;
     for (size_t i = 0; i < vsyncTS.size(); i++) {
         top += vsyncTS[i] * ordinals[i];
         bottom += ordinals[i] * ordinals[i];
     }
 
     if (CC_UNLIKELY(bottom == 0)) {
         it->second = {mIdealPeriod, 0};
         clearTimestamps();
         return false;
     }
 
     nsecs_t const anticipatedPeriod = top * kScalingFactor / bottom;
     nsecs_t const intercept = meanTS - (anticipatedPeriod * meanOrdinal / kScalingFactor);
 
     auto const percent = std::abs(anticipatedPeriod - mIdealPeriod) * kMaxPercent / mIdealPeriod;
     if (percent >= kOutlierTolerancePercent) {
         it->second = {mIdealPeriod, 0};
         clearTimestamps();
         return false;
     }
 
     traceInt64If("VSP-period", anticipatedPeriod);
     traceInt64If("VSP-intercept", intercept);
 
     it->second = {anticipatedPeriod, intercept};
 
     ALOGV("model update ts: %" PRId64 " slope: %" PRId64 " intercept: %" PRId64, timestamp,
           anticipatedPeriod, intercept);
     return true;
 }
```

这块代码是SW 模型更新的核心，是最关键的部分，是通过硬件VSYNC的样本计算出当前屏幕刷新率对于的Vsync周期，在这个方法中，谷歌采用了简单一元线性回归分析预测法，回归分析是一种预测性的建模技术，它研究的是因变量和自变量之间的关系。它能够表明自多个自变量对一个因变量的影响强度。这种技术通常用于预测分析、时间序列模型以及发现变量之间的因果关系。回归分析是一种通过建立模型来研究变量之间相互关系的密切程度、结构状态及进行模型预测的有效工具，是建模和分析数据的重要工具。

由于很多现象需要多个因素做全面分析，只有当众多因素中确实存在一个对因变量影响作用明显高于其他因素的变量，才能将它作为自变量，应用一元相关的回归分析进行预测，而谷歌采用的是回归算法中的最小二乘法。

最小二乘法求回归直线方程式：

![](https://upload-images.jianshu.io/upload_images/26874665-0ad5d9b09954ee6b.png?imageMogr2/auto-orient/strip|imageView2/2/w/627/format/webp)

image-20220727143813632.png

在这个方程式中，b就是回归系数，a就是截距。

如果提供了一组x因变量的一组数据，再提供一组y自变量的一组数据，就可以通过上面的方程式推导出回归系数b和截距a。

回到代码，按照默认的流程分析这个函数，首先有一个集合mTimestamps会存储硬件的VSYNC样本，刚开始的时候这个样本集合会清空，最多采6个样本就可以进行计算，简单描述上述代码的流程如下：

1. 清空mTimestamps的样本集合，打开硬件VSYNC开关，开始采集样本。
    
2. 传入的时间戳会做一些校验工作，validate这个函数会对数据做一些处理，例如重复的数据等等。
    
3. 如果传入的数据没有问题，则会一直添加到mTimestamps集合中，直到采6个样本信息就关闭VSYNC开关。
    
4. 通过着6个样本，计算出x的因变量集合 ordinals，和y的自变量集合vsyncTS。通过6个样本把这两个集合的数据都计算出来，然后通过上面的方程式把回归系数和截距都计算出来，这块的回归系数就是Vsync的时间周期，前面我加过日志，我把这两个集合的内容可以贴出来看下，以下是90fps的vsync信息。
    

x的集合内容 {0，1000，2000，3000，4000，5000} ，从集合的内容是vsync的个数信息。

y的集合内容{0，11027000，22053000，33080000，44106000，55132000}，从代码中了解是硬件vsync时间戳的递增值，因为两个硬件vsync的时间戳的差值可以理解是一个vsync周期。

5. 从这两个集合数据计算出回归系数b，和截距a，保存到当前的屏幕刷新率作为key的mRateMap的value中，这个value是一个结构体，保存两个值，当前屏幕刷新率对于的回归系数和截距。

##### 3.2.2 nextAnticipatedVsyncTimeFromLocked

有了这个回归系数和截距，就可以传入上一次app或者sf发射的时间，计算出下一次发射的时间 ，在前面讲解Vsync-sf的发射流程，有一个很重要的点就是要计算下一次发射的时间，就是调用VsyncTracker的nextAnticipatedVsyncTimeFromLocked方法。

代码如下：

```c
nsecs_t VSyncPredictor::nextAnticipatedVSyncTimeFromLocked(nsecs_t timePoint) const {
    auto const [slope, intercept] = getVSyncPredictionModelLocked();

    if (mTimestamps.empty()) {
        traceInt64If("VSP-mode", 1);
        auto const knownTimestamp = mKnownTimestamp ? *mKnownTimestamp : timePoint;
        auto const numPeriodsOut = ((timePoint - knownTimestamp) / mIdealPeriod) + 1;
        return knownTimestamp + numPeriodsOut * mIdealPeriod;
    }

    auto const oldest = *std::min_element(mTimestamps.begin(), mTimestamps.end());

    // See b/145667109, the ordinal calculation must take into account the intercept.
    auto const zeroPoint = oldest + intercept;
    auto const ordinalRequest = (timePoint - zeroPoint + slope) / slope;
    auto const prediction = (ordinalRequest * slope) + intercept + oldest;

    traceInt64If("VSP-mode", 0);
    traceInt64If("VSP-timePoint", timePoint);
    traceInt64If("VSP-prediction", prediction);

    auto const printer = [&, slope = slope, intercept = intercept] {
        std::stringstream str;
        str << "prediction made from: " << timePoint << "prediction: " << prediction << " (+"
            << prediction - timePoint << ") slope: " << slope << " intercept: " << intercept
            << "oldestTS: " << oldest << " ordinal: " << ordinalRequest;
        return str.str();
    };

    ALOGV("%s", printer().c_str());
    LOG_ALWAYS_FATAL_IF(prediction < timePoint, "VSyncPredictor: model miscalculation: %s",
                        printer().c_str());

    return prediction;
}
```

从上面的代码可以看到这个流程。

1. 先判断mTimestamps的集合是否为空，如果为空，则拿默认值，90的帧率就是11.11111us去参与计算，mKnownTimestamp是之前样本的最大值，和传入上一次发射的时间做差值除Vsync的周期时间，我们理解样本的时间是比上一次的发射时间大，因为surfaceflinger在做合成的时候会把之前的fence时间的时间戳也存到这个集合中，这边会固定计算出下一个vsync发射的时间。
2. 如果mTimestamps的集合不为空，通过这个集合的数据和传入的发射时间，算出一次线程回归方式的因变量x值，然后根据回归系数和截距，用方程式计算出自变量y值，而y值，也就是代码中的prediction，作为下一次vsync发射的时间。

以上的两个函数是最核心的逻辑，然后有同学会问，什么时候会打硬件Vsync开关，什么时候会关闭。除了刚开机的时候，会打开硬件Vsync开关，如果模型校准完成之后，再关闭。还有切换刷新率的时候也会打开Vsycn开关。

还有正常情况下，如果app要去做刷新的动作，首先要去申请Vsync-app的信号。就会走到EventThread.cpp的requestNextVsync函数中。

```c
/frameworks/native/services/surfaceflinger/Scheduler/EventThread.cpp

void EventThread::requestNextVsync(const sp<EventThreadConnection>& connection) {
    if (connection->resyncCallback) {
        connection->resyncCallback();
    }

    std::lock_guard<std::mutex> lock(mMutex);

    if (connection->vsyncRequest == VSyncRequest::None) {
        connection->vsyncRequest = VSyncRequest::Single;
        mCondition.notify_all();
    } else if (connection->vsyncRequest == VSyncRequest::SingleSuppressCallback) {
        connection->vsyncRequest = VSyncRequest::Single;
    }
}
```

它会先调用connection的resyncCallback的方法。这个方法是创建这个Connection的时候，传入的回调函数。

```c
 /frameworks/native/services/surfaceflinger/Scheduler/Scheduler.cpp

sp<EventThreadConnection> Scheduler::createConnectionInternal(
        EventThread* eventThread, ISurfaceComposer::EventRegistrationFlags eventRegistration) {
    return eventThread->createEventConnection([&] { resync(); }, eventRegistration);
}
```

等于每次app要申请的时候，会走到resyncAndRefresh中，这个函数就会强制进行一次硬件的VSYNC校准。

```c
/frameworks/native/services/surfaceflinger/Scheduler/Scheduler.cpp

void Scheduler::resync() {
    static constexpr nsecs_t kIgnoreDelay = ms2ns(750);

    const nsecs_t now = systemTime();
    const nsecs_t last = mLastResyncTime.exchange(now);

    if (now - last > kIgnoreDelay) {
        resyncToHardwareVsync(false, mRefreshRateConfigs.getCurrentRefreshRate().getVsyncPeriod()); //硬件校准
    }
}
```

这个红色框框的部分，就是通知VsyncControler告诉VsyncTracker把时间戳清空掉，然后开始添加新的VSYNC时间戳信息，然后再进行校准。

除了上面的这种情况，还有一种情况，就是SurfaceFlinger再进行合成的时候，会把上一帧的完成合成的fence的时间也会同时添加到VsyncTracker的的时间戳集合。这个集合再情况的情况下，除了会增加6个硬件采样之外，这个集合也会添加fence的时间信息。

```c
/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp
 void SurfaceFlinger::postComposition() {
 ...
     if (display && display->isPrimary() && display->getPowerMode() == hal::PowerMode::ON &&
         mPreviousPresentFences[0].fenceTime->isValid()) {
         mScheduler->addPresentFence(mPreviousPresentFences[0].fenceTime);
     }
 ...
 }
```

```c
/frameworks/native/services/surfaceflinger/Scheduler/Scheduler.cpp

void Scheduler::addPresentFence(const std::shared_ptr<FenceTime>& fenceTime) {
    if (mVsyncSchedule.controller->addPresentFence(fenceTime)) {
        enableHardwareVsync();
    } else {
        disableHardwareVsync(false);
    }
}
```

如上图的代码所示，我们通过给VsyncController中添加fence的时间信息，也会判断当前要不要打开Vsync进行校准，但是默认都是不打开VSYNC校准的，因为每一帧的合成都会把fence的时间传入到这个VsyncTracker中的时间戳集合中，所以这个函数会每次合成的时候都会重新计算回归系数和截距。

1. 从HWC获取的display完成显示的fence， HW VSYNC就是display显示完成后发出来的，因此这个fence的时间戳可以看作是发射HW VSYNC的恰当时刻，虽然HW VSYNC可能已经关闭了。
2. 将fence样本加入到VsyncTracker中，会重新校准出新的Vsync周期，如果再校准中的过程中发生误差过大，会重新打开HW VSYNC进行校准，所谓的校准，就是重新采集HW VSYNC样本，重新计算出新的回归习系数（vsync周期）和截距。

```c
/frameworks/native/services/surfaceflinger/Scheduler/VSyncReactor.cpp

bool VSyncReactor::addPresentFence(const std::shared_ptr<android::FenceTime>& fence) {
    if (!fence) {
        return false;
    }

    nsecs_t const signalTime = fence->getCachedSignalTime();
    if (signalTime == Fence::SIGNAL_TIME_INVALID) {
        return true;
    }

    std::lock_guard lock(mMutex);
    if (mExternalIgnoreFences || mInternalIgnoreFences) {
        return true;
    }

    bool timestampAccepted = true;
    for (auto it = mUnfiredFences.begin(); it != mUnfiredFences.end();) {
        auto const time = (*it)->getCachedSignalTime();
        if (time == Fence::SIGNAL_TIME_PENDING) {
            it++;
        } else if (time == Fence::SIGNAL_TIME_INVALID) {
            it = mUnfiredFences.erase(it);
        } else {
            timestampAccepted &= mTracker.addVsyncTimestamp(time);

            it = mUnfiredFences.erase(it);
        }
    }

    if (signalTime == Fence::SIGNAL_TIME_PENDING) {
        if (mPendingLimit == mUnfiredFences.size()) {
            mUnfiredFences.erase(mUnfiredFences.begin());
        }
        mUnfiredFences.push_back(fence);
    } else {
        timestampAccepted &= mTracker.addVsyncTimestamp(signalTime);
    }

    if (!timestampAccepted) {
        mMoreSamplesNeeded = true;
        setIgnorePresentFencesInternal(true);
        mPeriodConfirmationInProgress = true;
    }

    return mMoreSamplesNeeded;
}
```

以上就是在SurfaceFlinger的postComposition中一直调用的方法。

#### 3.3 dump

从前面的代码，我们可以看到好多常量参数，或者VsyncTracker中计算出当前屏幕刷新率的回归系数和截距。都可以通过命令打印出来。

![](https://upload-images.jianshu.io/upload_images/26874665-15b3c084d6c1776c.png?imageMogr2/auto-orient/strip|imageView2/2/w/1003/format/webp)

image-20220727170021551.png

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/5e9c558d1543  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。