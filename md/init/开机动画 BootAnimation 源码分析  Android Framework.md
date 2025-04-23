---
created: 2025-04-23T13:51:53 (UTC +08:00)
tags: []
source: http://ahaoframework.tech/005.%E7%B3%BB%E7%BB%9F%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B%E5%88%86%E6%9E%90/014.%E5%BC%80%E6%9C%BA%E5%8A%A8%E7%94%BB%20BootAnimation%20%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90.html
author: 
---

# 开机动画 BootAnimation 源码分析 | Android Framework

> ## Excerpt
> 在开机时，Android 手机会播放一段动画，以提供一个良好的开机体验。开机动画由 native 进程 BootAnimation 实现，本节我们就来分析 BootAnimation 的具体实现。

---
在开机时，Android 手机会播放一段动画，以提供一个良好的开机体验。开机动画由 native 进程 BootAnimation 实现，本节我们就来分析 BootAnimation 的具体实现。

## [#](http://ahaoframework.tech/005.%E7%B3%BB%E7%BB%9F%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B%E5%88%86%E6%9E%90/014.%E5%BC%80%E6%9C%BA%E5%8A%A8%E7%94%BB%20BootAnimation%20%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90.html#_1-bootanimation-%E7%9A%84%E5%90%AF%E5%8A%A8) 1. BootAnimation 的启动

BootAnimation 的源码位于 `frameworks/base/cmds/bootanimation`，在源码目录下有一个 `bootanim.rc` 文件：

```
service bootanim /system/bin/bootanimation
    class core animation
    user graphics
    group graphics audio
    disabled
    oneshot
    ioprio rt 0
    writepid /dev/stune/top-app/tasks
```


这里定义了一个 bootanim 服务，服务中定义了 disable 属性，也就是说该服务不会随 class 启动，也就是 init 解析 init.rc 的时候不会自动启动这个服务。

接着再看 `Android.bp`：

```
cc_binary {
    name: "bootanimation",
    defaults: ["bootanimation_defaults"],

    //......

    init_rc: ["bootanim.rc"],
}
```



这里有个 `init_rc: ["bootanim.rc"]` 的配置。加了这个配置之后，在编译系统的时候会把 `bootanim.rc` 文件预制到 `/system/etc/init/` 目录。init 进程在启动的时候会加载这个目录下的所有 rc 文件.


```
SurfaceFlinger 是 Android 系统中负责管理和合成图形表面的关键服务，其启动过程与 Android 系统的整体启动流程紧密相关，涉及多个系统进程和步骤。

  

1. **init 进程启动**：Android 内核完成初始化后，启动 init 进程，其进程 ID 为 1。init 进程是用户空间的首个进程，主要职责是解析并执行初始化脚本，启动系统各项服务。
2. **解析配置文件**：init 进程会读取一系列`.rc`配置文件，这些文件分布在`/init.rc` 、`/system/etc/init`以及`/vendor/etc/init`等目录下。其中，`surfaceflinger.rc`文件定义了 SurfaceFlinger 服务的启动参数和依赖关系 。比如，定义了服务名为`surfaceflinger`，对应的可执行文件路径为`/system/bin/surfaceflinger` ；指定服务属于`core`类，会在系统启动早期启动；设置服务以`system`用户和`graphics`、`drmrpc`组的身份运行，并赋予`SYS_NICE`权限；还规定了服务重启时会同时重启`zygote`进程等。
3. **启动服务进程**：init 进程解析`surfaceflinger.rc`文件后，通过调用`fork()`和`exec()`系统调用，创建一个新进程来运行`/system/bin/surfaceflinger`可执行文件。在这个过程中，init 进程会依据配置文件设置新进程的用户、组、权限等运行环境。
4. **执行初始化操作**：新创建的进程开始执行 SurfaceFlinger 的`main()`函数。在`main()`函数中，会进行一系列初始化操作，包括创建 SurfaceFlinger 对象、初始化显示设备、创建图形缓冲区等。例如，创建 SurfaceFlinger 对象，初始化相关资源，为后续的图形合成和显示功能做准备。
5. **启动消息循环与注册服务**：SurfaceFlinger 会启动一个消息循环，用于处理各种图形合成和显示相关的事件。同时，它会将自己注册到`ServiceManager`中。其他服务可以通过`ServiceManager`获取 SurfaceFlinger 的服务，实现与它的交互，从而完成系统图形显示相关的功能 。
```


bootanim 服务要定义成 disable 的？为什么不直接在 init 阶段启动？因为 bootanim 需要借助于 surfaceflinger 来显示动画，所以需要等待 surfacefinger 启动后，再启动 bootanim。

接下来我们来看看 SurfaceFlinger 的初始化函数：

```
// frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp
void SurfaceFlinger::init() {

    // ......
    
    // 初始化显示系统
    initializeDisplays();

    getRenderEngine().primeCache();

    const bool presentFenceReliable =
            !getHwComposer().hasCapability(HWC2::Capability::PresentFenceIsNotReliable);
    mStartPropertySetThread = getFactory().createStartPropertySetThread(presentFenceReliable);

    // 启动 mStartPropertySetThread 线程
    if (mStartPropertySetThread->Start() != NO_ERROR) {
        ALOGE("Run StartPropertySetThread failed!");
    }
}
```


-   调用 `initializeDisplays()` 函数，初始化显示系统，目前了解即可，不是本节重点，显示系统专题会做详细分析
-   调用 `mStartPropertySetThread->Start()` 函数，启动 mStartPropertySetThread 线程

接下来就会执行到 mStartPropertySetThread 线程的 threadLoop 函数：

```
// frameworks/native/services/surfaceflinger/StartPropertySetThread.cpp
bool StartPropertySetThread::threadLoop() {
    // Set property service.sf.present_timestamp, consumer need check its readiness
    property_set(kTimestampProperty, mTimestampPropertyValue ? "1" : "0");
    // Clear BootAnimation exit flag
    property_set("service.bootanim.exit", "0");
    // Start BootAnimation if not started
    property_set("ctl.start", "bootanim");
    // Exit immediately
    return false;
}
```



在 `mStartPropertySetThread` 线程中会设置 `ctl.start` 属性的值为 `bootanim`，在属性系统部分我们讲过这样会启动 `bootanim service`。

## [#](http://ahaoframework.tech/005.%E7%B3%BB%E7%BB%9F%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B%E5%88%86%E6%9E%90/014.%E5%BC%80%E6%9C%BA%E5%8A%A8%E7%94%BB%20BootAnimation%20%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90.html#_2-bootanimation-%E7%9A%84%E6%89%A7%E8%A1%8C%E8%BF%87%E7%A8%8B) 2. BootAnimation 的执行过程

BootAnimation 的主函数定义在 `frameworks/base/cmds/bootanimation/bootanimation_main.cpp`：

```
int main()
{
    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_DISPLAY);

    // 关注点1
    // 是否支持开机动画，由 ro.boot.quiescent 这个属性决定
    bool noBootAnimation = bootAnimationDisabled();

    ALOGI_IF(noBootAnimation,  "boot animation disabled");
    if (!noBootAnimation) {
        
        // 关注点2
        // 初始化 Binder
        sp<ProcessState> proc(ProcessState::self());
        ProcessState::self()->startThreadPool();

        // 关注点3
        // create the boot animation object (may take up to 200ms for 2MB zip)
        sp<BootAnimation> boot = new BootAnimation(audioplay::createAnimationCallbacks());

        // 关注点4
        // 等待 SurfaceFlinger 初始化完成并加入到 ServiceManager。
        // 因为 bootanim 服务是在 SurfaceFlinger.init() 里面启动的，但在 SurfaceFlinger 服务是在执行完 SurfaceFlinger.init() 之后才加入到 ServiceManager 的，所以这里需要等一下。
        waitForSurfaceFlinger();

        // 关注点5
        // 运行动画线程，开始播放动画。
        boot->run("BootAnimation", PRIORITY_DISPLAY);

        ALOGV("Boot animation set up. Joining pool.");

        // 关注点6
        IPCThreadState::self()->joinThreadPool();
    }
    return 0;
}
```



在 BootAnimation 主函数中，有 6 个关注点。

**关注点 1** 处通过 `bootAnimationDisabled` 函数判断是否支持开机动画：

```
bool bootAnimationDisabled() {
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.sf.nobootanimation", value, "0");
    if (atoi(value) > 0) {
        return true;
    }

    property_get("ro.boot.quiescent", value, "0");
    return atoi(value) > 0;
}
```



实际内部就是获取 `ro.boot.quiescent` 属性值，该属性值用于记录是否支持开机动画。

**关注点 2 和关注点 6** 处都是对 Binder 的初始化，这个在 Binder 专题已经讲过了。

**关注点 3** 初始化了一个 `BootAnimation` 对象，这个对象是动画加载播放的核心。这里有一点比较隐藏，不容易注意到，BootAnimation 类继承自 Thread，Thread 类又继承自 RefBase，在智能指针章节我们讲过，RefBase 的子类被第一次引用时，会执行到 `onFirstRef()` 函数。初始化的过程会调用到 BootAnimation 的 onFirstRef 函数：

```
void BootAnimation::onFirstRef() {
    status_t err = mSession->linkToComposerDeath(this);
    SLOGE_IF(err, "linkToComposerDeath failed (%s) ", strerror(-err));
    if (err == NO_ERROR) {
        // Load the animation content -- this can be slow (eg 200ms)
        // called before waitForSurfaceFlinger() in main() to avoid wait
        ALOGD("%sAnimationPreloadTiming start time: %" PRId64 "ms",
                mShuttingDown ? "Shutdown" : "Boot", elapsedRealtime());
        // 加载动画
        preloadAnimation();
        ALOGD("%sAnimationPreloadStopTiming start time: %" PRId64 "ms",
                mShuttingDown ? "Shutdown" : "Boot", elapsedRealtime());
    }
}

bool BootAnimation::preloadAnimation() {
    findBootAnimationFile();
    if (!mZipFileName.isEmpty()) {
        mAnimation = loadAnimation(mZipFileName);
        return (mAnimation != nullptr);
    }

    return false;
}
```


在 `onFirstRef()` 函数中，会调用到 `preloadAnimation()` 函数，`preloadAnimation()` 函数进一步调用到 `findBootAnimationFile()` 函数查找动画文件，接着调用 `loadAnimation` 函数加载开机动画文件。

`findBootAnimationFile()` 函数的具体实现如下：

```
void BootAnimation::findBootAnimationFile() {

    // 关注点1，加密启动动画
    // If the device has encryption turned on or is in process
    // of being encrypted we show the encrypted boot animation.
    char decrypt[PROPERTY_VALUE_MAX];
    property_get("vold.decrypt", decrypt, "");

    bool encryptedAnimation = atoi(decrypt) != 0 ||
        !strcmp("trigger_restart_min_framework", decrypt);

    if (!mShuttingDown && encryptedAnimation) {
        static const char* encryptedBootFiles[] =
            {PRODUCT_ENCRYPTED_BOOTANIMATION_FILE, SYSTEM_ENCRYPTED_BOOTANIMATION_FILE};
        for (const char* f : encryptedBootFiles) {
            if (access(f, R_OK) == 0) {
                mZipFileName = f;
                return;
            }
        }
    }

    // 关注点2，普通启动动画
    const bool playDarkAnim = android::base::GetIntProperty("ro.boot.theme", 0) == 1;
    static const char* bootFiles[] =
        {APEX_BOOTANIMATION_FILE, playDarkAnim ? PRODUCT_BOOTANIMATION_DARK_FILE : PRODUCT_BOOTANIMATION_FILE,
         OEM_BOOTANIMATION_FILE, SYSTEM_BOOTANIMATION_FILE};
    static const char* shutdownFiles[] =
        {PRODUCT_SHUTDOWNANIMATION_FILE, OEM_SHUTDOWNANIMATION_FILE, SYSTEM_SHUTDOWNANIMATION_FILE, ""};

    for (const char* f : (!mShuttingDown ? bootFiles : shutdownFiles)) {
        if (access(f, R_OK) == 0) {
            mZipFileName = f;
            return;
        }
    }
}
```



关注点 1，判断设备如果开启了加密，则使用加密动画文件，加密动画文件由PRODUCT\_ENCRYPTED\_BOOTANIMATION\_FILE 和 SYSTEM\_ENCRYPTED\_BOOTANIMATION\_FILE 两个常量指定，其中优先级 PRODUCT\_ENCRYPTED\_BOOTANIMATION\_FILE > SYSTEM\_ENCRYPTED\_BOOTANIMATION\_FILE，选择好的动画文件保存在 mZipFileName 变量中。

关注点 2，普通情况下（非加密），从 `APEX_BOOTANIMATION_FILE、PRODUCT_BOOTANIMATION_FILE、 OEM_BOOTANIMATION_FILE、SYSTEM_BOOTANIMATION_FILE` 几个常量中获取动画文件，优先级依次降低，获取到的动画文件同样保存在 mZipFileName 变量中。

loadAnimation 函数用于加载动画文件，功能流程都相对固定，基本不会修改，我们就不在分析其内部源码了。

回到主函数的**关注点 4**， 这里调用 `waitForSurfaceFlinger()` 等待 SurfaceFlinger 初始化完成并加入到 ServiceManager。

```
void waitForSurfaceFlinger() {
    // TODO: replace this with better waiting logic in future, b/35253872
    int64_t waitStartTime = elapsedRealtime();
    sp<IServiceManager> sm = defaultServiceManager();
    const String16 name("SurfaceFlinger");
    const int SERVICE_WAIT_SLEEP_MS = 100;
    const int LOG_PER_RETRIES = 10;
    int retry = 0;
    // 查询 ServiceManager 中是否有 SurfaceFlinger 服务
    while (sm->checkService(name) == nullptr) {
        retry++;
        if ((retry % LOG_PER_RETRIES) == 0) {
            ALOGW("Waiting for SurfaceFlinger, waited for %" PRId64 " ms",
                  elapsedRealtime() - waitStartTime);
        }
        // 每隔 100 ms 检查一次
        usleep(SERVICE_WAIT_SLEEP_MS * 1000);
    };
    int64_t totalWaited = elapsedRealtime() - waitStartTime;
    if (totalWaited > SERVICE_WAIT_SLEEP_MS) {
        ALOGI("Waiting for SurfaceFlinger took %" PRId64 " ms", totalWaited);
    }
}
```



这里的逻辑比较简单就是每隔 100ms 检查一下 ServiceManager 里是否已存在了 SurfaceFlinger 服务，也就是 SurfaceFlinger 是否准备完毕。

为什么要在这里做检查呢？因为 bootanim 服务是在 SurfaceFlinger.init() 里面启动的，但 SurfaceFlinger 服务是在执行完 SurfaceFlinger.init() 之后才加入到 ServiceManager 的，所以这里需要等一下。

这是应该可以使用 eventfd 来优化以下，貌似又是一个开机启动的优化点。

**关注点 5**调用 BootAnimation 的 run 方法启动 BootAnimation 线程来播放动画。

这里有个需要注意的点，BootAnimation 的父类 Thread 有一个虚函数 readyToRun，BootAnimation 重写了这个函数。在调用 Thread.run 函数启动线程的时候会先调用 readyToRun，然后再调用 threadLoop。

readyToRun 的主要工作做播放动画前的初始化。

```
status_t BufferProducerThread::readyToRun() {
    sp<ANativeWindow> anw(mSurface);
    status_t err = native_window_set_usage(anw.get(), mStream.buffer_producer.usage);
    if (err != NO_ERROR) {
        return err;
    }
    err = native_window_set_buffers_dimensions(
            anw.get(), mStream.buffer_producer.width, mStream.buffer_producer.height);
    if (err != NO_ERROR) {
        return err;
    }
    err = native_window_set_buffers_format(anw.get(), mStream.buffer_producer.format);
    if (err != NO_ERROR) {
        return err;
    }
    return NO_ERROR;
}
```


我们接着看线程的执行函数 `threadLooper`

```
bool BootAnimation::threadLoop()
{
    bool r;
    // We have no bootanimation file, so we use the stock android logo
    // animation.
    if (mZipFileName.isEmpty()) {
        r = android();
    } else {
        r = movie();
    }
    //...
}
```



这里调用了 movie 函数播放动画：

```
bool BootAnimation::movie()
{
    //...
    playAnimation(*mAnimation);
    //...
    releaseAnimation(mAnimation);
    //...
}
```


进一步调用 playAnimation 函数播放动画：

```
bool BootAnimation::playAnimation(const Animation& animation)
{
    //...
    for (size_t i=0 ; i<pcount ; i++) { //动画是分段的，依次播放各片段
        //...
        for (int r=0 ; !part.count || r<part.count ; r++) { //每个片段循环播放次数
            //...
            for (size_t j=0 ; j<fcount && (!exitPending() || part.playUntilComplete) ; j++) { //一个片段的一次播放
                //逐帧播放动画，如何播放动画会在显示专题再讲
                //...
                //查检是否开机完成，可以退出动画了
                checkExit();
            }
            usleep(part.pause * ns2us(frameDuration));
            // For infinite parts, we've now played them at least once, so perhaps exit
            if(exitPending() && !part.count && mCurrentInset >= mTargetInset)
                break;
        }
    }
    ...
}
```



这里通过循环分段循环播放动画，每播放一段动画，都会调用 `checkExit()` 函数来检查是否开机完成，如果开机完成就退出动画。

## [#](http://ahaoframework.tech/005.%E7%B3%BB%E7%BB%9F%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B%E5%88%86%E6%9E%90/014.%E5%BC%80%E6%9C%BA%E5%8A%A8%E7%94%BB%20BootAnimation%20%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90.html#_2-bootanimation-%E7%9A%84%E9%80%80%E5%87%BA) 2. BootAnimation 的退出

上一节说到，动画执行过程中会通过 `checkExit()` 函数来检查是否开机完成：

```
void BootAnimation::checkExit() {
    // EXIT_PROP_NAME = "service.bootanim.exit"
    char value[PROPERTY_VALUE_MAX];
    property_get(EXIT_PROP_NAME, value, "0");
    int exitnow = atoi(value);
    if (exitnow) {
        requestExit();
        mCallbacks->shutdown();
    }
}
```



`checkExit()` 函数里面通过获取 `service.bootanim.exit` 这个属性值来判断是否要退出动画。如果这个属性被设置为 1, 则调用 `requestExit()` 请求结束线程。就退出播放线程。BootAnimation 的使命就完成了，可以退出了。

那 `service.bootanim.exit` 这个属性是在哪里设置的呢？

在 SurfaceFlinger 的服务端，如果收到 BOOT\_FINISHED 指令，就会调用到 `bootFinished` 函数，`bootFinished` 函数中就会设置 `service.bootanim.exit` 这个属性。

```
// frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp
status_t BnSurfaceComposer::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        ...
        case BOOT_FINISHED: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            bootFinished();
            return NO_ERROR;
        }
        //..
}

// frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp
void SurfaceFlinger::bootFinished()
{
    //...
    property_set("service.bootanim.exit", "1");
    //...
}
```

  

那么哪里会通过远程调用调用到 `bootFinished` 函数呢？

Launcher 启动完毕后，会向 Looper 添加一个 IdleHandler：

```
// frameworks/base/core/java/android/app/ActivityThread.java
public void handleResumeActivity(IBinder token, boolean finalStateRequest, boolean isForward,
        String reason) {
    //...
    final ActivityClientRecord r = performResumeActivity(token, finalStateRequest, reason);
    //...
    Looper.myQueue().addIdleHandler(new Idler());
}

private class Idler implements MessageQueue.IdleHandler {
    @Override
    public final boolean queueIdle() {
                // ......
                if (a.activity != null && !a.activity.mFinished) {
                    try {
                        am.activityIdle(a.token, a.createdConfig, stopProfiling);
                        a.createdConfig = null;
                        // ......
                    }
                    // ......
                }
                //......
    }
    //.....
}
```



在 Launcher 空闲时就会执行 Idler 的 queueIdle 方法，在这个方法中会调用到 activityIdle 方法。 activityIdle 方法经过层层调用最终会调用到 performEnableScreen 方法：

```
private void performEnableScreen() {
    synchronized (mGlobalLock) {
        // ........
        if (!mBootAnimationStopped) {
            Trace.asyncTraceBegin(TRACE_TAG_WINDOW_MANAGER, "Stop bootanim", 0);
            // stop boot animation
            // formerly we would just kill the process, but we now ask it to exit so it
            // can choose where to stop the animation.
            // 设置属性
            SystemProperties.set("service.bootanim.exit", "1");
            mBootAnimationStopped = true;
         }
        if (!mForceDisplayEnabled && !checkBootAnimationCompleteLocked()) {
            if (DEBUG_BOOT) Slog.i(TAG_WM, "performEnableScreen: Waiting for anim complete");
            return;
        }
        try {
            IBinder surfaceFlinger = ServiceManager.getService("SurfaceFlinger");
            if (surfaceFlinger != null) {
                 Slog.i(TAG_WM, "******* TELLING SURFACE FLINGER WE ARE BOOTED!");
                Parcel data = Parcel.obtain();
                 data.writeInterfaceToken("android.ui.ISurfaceComposer");
                 // 发起远程调用
                 surfaceFlinger.transact(IBinder.FIRST_CALL_TRANSACTION, // BOOT_FINISHED
                         data, null, 0);
                data.recycle();
            }
        } catch (RemoteException ex) {
             Slog.e(TAG_WM, "Boot completed: SurfaceFlinger is dead!");
        }
        // .......
```



这里会设置 `service.bootanim.exit` 属性值，同时发起 surfaceFlinger的 BOOT\_FINISHED 远程调用。 这两个地方功能是不是有点重复了？

这样的话就暂停了开机动画的播放。

## [#](http://ahaoframework.tech/005.%E7%B3%BB%E7%BB%9F%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B%E5%88%86%E6%9E%90/014.%E5%BC%80%E6%9C%BA%E5%8A%A8%E7%94%BB%20BootAnimation%20%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90.html#%E5%8F%82%E8%80%83%E8%B5%84%E6%96%99) 参考资料

-   [Android系统开发进阶-BootAnimation启动与结束流程 (opens new window)](https://qiushao.net/2020/02/23/Android%E7%B3%BB%E7%BB%9F%E5%BC%80%E5%8F%91%E8%BF%9B%E9%98%B6/BootAnimation%E5%90%AF%E5%8A%A8%E6%B5%81%E7%A8%8B/)
