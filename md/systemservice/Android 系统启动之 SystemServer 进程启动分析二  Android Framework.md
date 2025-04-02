SystemServer 是 Android 系统 Java 层最重要的进程之一，几乎所有的 Java 层 Binder 服务都运行在这个进程里。

SystemServer 的启动大致可分为两个阶段：

-   在 Zygote 进程中调用 fork 系统调用创建 SystemServer 进程
-   执行 SystemServer 类的 main 方法来启动系统服务

上一节我们分析了第一阶段，今天我们来分析第二阶段：

```
    // frameworks/base/services/java/com/android/server/SystemServer.java
    public static void main(String[] args) {
        new SystemServer().run();
    }
```



这里会 new 一个 SystemServer 对象，然后调用它的 run 方法：

```
    private void run() {
        try {
            traceBeginAndSlog("InitBeforeStartServices");

            // Record the process start information in sys props.
            SystemProperties.set(SYSPROP_START_COUNT, String.valueOf(mStartCount));
            SystemProperties.set(SYSPROP_START_ELAPSED, String.valueOf(mRuntimeStartElapsedTime));
            SystemProperties.set(SYSPROP_START_UPTIME, String.valueOf(mRuntimeStartUptime));

            EventLog.writeEvent(EventLogTags.SYSTEM_SERVER_START,
                    mStartCount, mRuntimeStartUptime, mRuntimeStartElapsedTime);

            // If a device's clock is before 1970 (before 0), a lot of
            // APIs crash dealing with negative numbers, notably
            // java.io.File#setLastModified, so instead we fake it and
            // hope that time from cell towers or NTP fixes it shortly.

            // 设置系统时间

            if (System.currentTimeMillis() < EARLIEST_SUPPORTED_TIME) {
                Slog.w(TAG, "System clock is before 1970; setting to 1970.");
                SystemClock.setCurrentTimeMillis(EARLIEST_SUPPORTED_TIME);
            }


            // 时区，语言、国家的设置
            //
            // Default the timezone property to GMT if not set.
            //
            String timezoneProperty = SystemProperties.get("persist.sys.timezone");
            if (timezoneProperty == null || timezoneProperty.isEmpty()) {
                Slog.w(TAG, "Timezone not set; setting to GMT.");
                SystemProperties.set("persist.sys.timezone", "GMT");
            }

            
            // If the system has "persist.sys.language" and friends set, replace them with
            // "persist.sys.locale". Note that the default locale at this point is calculated
            // using the "-Duser.locale" command line flag. That flag is usually populated by
            // AndroidRuntime using the same set of system properties, but only the system_server
            // and system apps are allowed to set them.
            //
            // NOTE: Most changes made here will need an equivalent change to
            // core/jni/AndroidRuntime.cpp
            if (!SystemProperties.get("persist.sys.language").isEmpty()) {
                final String languageTag = Locale.getDefault().toLanguageTag();

                SystemProperties.set("persist.sys.locale", languageTag);
                SystemProperties.set("persist.sys.language", "");
                SystemProperties.set("persist.sys.country", "");
                SystemProperties.set("persist.sys.localevar", "");
            }

            // Binder、Sqlite相关属性设置

            // The system server should never make non-oneway calls
            Binder.setWarnOnBlocking(true);
            // The system server should always load safe labels
            PackageItemInfo.forceSafeLabels();

            // Default to FULL within the system server.
            SQLiteGlobal.sDefaultSyncMode = SQLiteGlobal.SYNC_MODE_FULL;

            // Deactivate SQLiteCompatibilityWalFlags until settings provider is initialized
            SQLiteCompatibilityWalFlags.init(null);



            // Here  go!
            Slog.i(TAG, "Entered the Android system server!");
            int uptimeMillis = (int) SystemClock.elapsedRealtime();
            EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_SYSTEM_RUN, uptimeMillis);
            if (!mRuntimeRestart) {
                MetricsLogger.histogram(null, "boot_system_server_init", uptimeMillis);
            }

            
            // In case the runtime switched since last boot (such as when
            // the old runtime was removed in an OTA), set the system
            // property so that it is in sync. We can | xq oqi't do this in
            // libnativehelper's JniInvocation::Init code where we already
            // had to fallback to a different runtime because it is
            // running as root and we need to be the system user to set
            // the property. http://b/11463182

            // 设置当前虚拟机的运行库路径
            SystemProperties.set("persist.sys.dalvik.vm.lib.2", VMRuntime.getRuntime().vmLibrary());

            // 调整内存
            // Mmmmmm... more memory!
            VMRuntime.getRuntime().clearGrowthLimit();

            // The system server has to run all of the time, so it needs to be
            // as efficient as possible with its memory usage.
            VMRuntime.getRuntime().setTargetHeapUtilization(0.8f);

            // Some devices rely on runtime fingerprint generation, so make sure
            // we've defined it before booting further.
            Build.ensureFingerprintProperty();

            // Within the system server, it is an error to access Environment paths without
            // explicitly specifying a user.
            Environment.setUserRequired(true);

            // Within the system server, any incoming Bundles should be defused
            // to avoid throwing BadParcelableException.
            BaseBundle.setShouldDefuse(true);

            // Within the system server, when parceling exceptions, include the stack trace
            Parcel.setStackTraceParceling(true);

            // Ensure binder calls into the system always run at foreground priority.
            BinderInternal.disableBackgroundScheduling(true);

            // Increase the number of binder threads in system_server
            BinderInternal.setMaxThreads(sMaxBinderThreads);
            
            // 设置进程相关属性
            // Prepare the main looper thread (this thread).
            android.os.Process.setThreadPriority(
                    android.os.Process.THREAD_PRIORITY_FOREGROUND);
            android.os.Process.setCanSelfBackground(false);

            // Looper 的初始化
            Looper.prepareMainLooper();
            Looper.getMainLooper().setSlowLogThresholdMs(
                    SLOW_DISPATCH_THRESHOLD_MS, SLOW_DELIVERY_THRESHOLD_MS);

            // 加载 libandroid_services.so
            // Initialize native services.
            System.loadLibrary("android_servers");

            // Debug builds - allow heap profiling.
            if (Build.IS_DEBUGGABLE) {
                initZygoteChildHeapProfiling();
            }

            // Check whether we failed to shut down last time we tried.
            // This call may not return.
            performPendingShutdown();

            // Initialize the system context.
            createSystemContext();

            // Create the system service manager.
            mSystemServiceManager = new SystemServiceManager(mSystemContext);
            mSystemServiceManager.setStartInfo(mRuntimeRestart,
                    mRuntimeStartElapsedTime, mRuntimeStartUptime);
            LocalServices.addService(SystemServiceManager.class, mSystemServiceManager);
            // Prepare the thread pool for init tasks that can be parallelized
            SystemServerInitThreadPool.get();
        } finally {
            traceEnd();  // InitBeforeStartServices
        }

        // System Server 的启动阶段
        // Start services.
        try {
            traceBeginAndSlog("StartServices");
            startBootstrapServices();
            startCoreServices();
            startOtherServices();
            SystemServerInitThreadPool.shutdown();
        } catch (Throwable ex) {
            Slog.e("System", "******************************************");
            Slog.e("System", "************ Failure starting system services", ex);
            throw ex;
        } finally {
            traceEnd();
        }

        StrictMode.initVmDefaults(null);

        if (!mRuntimeRestart && !isFirstBootOrUpgrade()) {
            int uptimeMillis = (int) SystemClock.elapsedRealtime();
            MetricsLogger.histogram(null, "boot_system_server_ready", uptimeMillis);
            final int MAX_UPTIME_MILLIS = 60 * 1000;
            if (uptimeMillis > MAX_UPTIME_MILLIS) {
                Slog.wtf(SYSTEM_SERVER_TIMING_TAG,
                        "SystemServer init took too long. uptimeMillis=" + uptimeMillis);
            }
        }

        // Diagnostic to ensure that the system is in a base healthy state. Done here as a common
        // non-zygote process.
        if (!VMRuntime.hasBootImageSpaces()) {
            Slog.wtf(TAG, "Runtime is not running with a boot image!");
        }

        // Loop forever.
        Looper.loop();
        throw new RuntimeException("Main thread loop unexpectedly exited");
    }
```



主要分为两个阶段：

-   SystemServer 启动前的准备阶段工作
    -   系统时间、时区、语言的设置
    -   虚拟机运行库、内存参数、内存利用率的调整
    -   设置当前线程的优先级
    -   加载 libandroid\_services.so 库
    -   初始化 Looper
    -   SystemContext 与 SystemServiceManager 的初始化
-   SystemServer 启动
    -   执行 startBootstrapServices、startCoreServices、startOtherServices 启动所有的系统服务
    -   调用 Looper.loop() 循环处理消息

接下来我们来分析 createSystemContext() 方法的执行过程：

```
    private void createSystemContext() {
        ActivityThread activityThread = ActivityThread.systemMain();
        mSystemContext = activityThread.getSystemContext();
        mSystemContext.setTheme(DEFAULT_SYSTEM_THEME);

        final Context systemUiContext = activityThread.getSystemUiContext();
        systemUiContext.setTheme(DEFAULT_SYSTEM_THEME);
    }
```



-   通过 `ActivityThread.systemMain()` 获得一个 ActivityThread 对象
-   然后调用 ActivityThread 对象的 getSystemContext()/getSystemUiContext() 方法来得到对应的 Context 对象
-   通过 Context 对象设置一个默认的 theme

接着看看 `ActivityThread.systemMain()` 的具体实现：

```
    public static ActivityThread systemMain() {
        // The system process on low-memory devices do not get to use hardware
        // accelerated drawing, since this can add too much overhead to the
        // process.
        if (!ActivityManager.isHighEndGfx()) {
            ThreadedRenderer.disable(true);
        } else {
            ThreadedRenderer.enableForegroundTrimming();
        }
        ActivityThread thread = new ActivityThread();
        thread.attach(true, 0);
        return thread;
    }
```



首先判断是否需要启动硬件渲染，然后创建了一个 ActivityThread 对象，然后调用 ActivityThread 对象的 attach 方法。

ActivityThread 是应用程序的主线程类，启动应用最后执行到的就是 ActivityThread 的 main() 方法。那么为什么 SystemServer 要初始化一个 ActivityThread 对象？

实际上 SystemServer 不仅仅是一个单纯的后台进程，它也是一个运行着 Service 组件的进程，很多系统对话框就是从 SystemServer 中显示出来的，因此 SystemServer 本身也需要一个和 APK 应用类似的上下文环境，创建 ActivityThread 是获取这个环境的第一步。

ActivityThread 在 SystemServer 进程中与普通进程还是有区别的，这里主要是通过 attach(boolen system,int seq) 方法的参数 system 来标识，函数如下：

```
@UnsupportedAppUsage
    private void attach(boolean system, long startSeq) {
        sCurrentActivityThread = this;
        mSystemThread = system;
        if (!system) {
           // 普通应用走这里
        } else {
            // Don't set application object here -- if the system crashes,
            // we can't display an alert, we just want to die die die.
             // 设置在DDMS中的应用名称
            android.ddm.DdmHandleAppName.setAppName("system_process",
                    UserHandle.myUserId());
            try {
                // 创建Instrumentation对象
                // 一个ActivityThread对应一个，可以监视应用的生命周期
                mInstrumentation = new Instrumentation();
                mInstrumentation.basicInit(this);
                // 创建Context对象
                ContextImpl context = ContextImpl.createAppContext(
                        this, getSystemContext().mPackageInfo);
                // 创建 Application 对象，并调用其 onCreate 方法
                mInitialApplication = context.mPackageInfo.makeApplication(true, null);
                mInitialApplication.onCreate();
            } catch (Exception e) {
                throw new RuntimeException(
                        "Unable to instantiate Application():" + e.toString(), e);
            }
        }

        // 初始并设置 ViewRootImpl 的 ConfigChangedCallback 回调
        ViewRootImpl.ConfigChangedCallback configChangedCallback
                = (Configuration globalConfig) -> {
            synchronized (mResourcesManager) {
                // We need to apply this change to the resources immediately, because upon returning
                // the view hierarchy will be informed about it.
                if (mResourcesManager.applyConfigurationToResourcesLocked(globalConfig,
                        null /* compat */)) {
                    updateLocaleListFromAppContext(mInitialApplication.getApplicationContext(),
                            mResourcesManager.getConfiguration().getLocales());

                    // This actually changed the resources! Tell everyone about it.
                    if (mPendingConfiguration == null
                            || mPendingConfiguration.isOtherSeqNewer(globalConfig)) {
                        mPendingConfiguration = globalConfig;
                        sendMessage(H.CONFIGURATION_CHANGED, globalConfig);
                    }
                }
            }
        };
        ViewRootImpl.addConfigCallback(configChangedCallback);
    }

```



attach()方法在system为true的情况下：

-   设置进程在 DDMS 中的名称
-   创建了 ContextImpl 和 Application 对象
-   最后调用了 Application 对象的 onCreate 方法
-   初始并设置 ViewRootImpl 的 ConfigChangedCallback 回调

SystemServer 在这里模仿一个 App 的启动，通过 `getSystemContext().mPackageInfo` 来获取到 App 的信息：

```
    // frameworks/base/core/java/android/app/ActivityThread.java
    @UnsupportedAppUsage
    public ContextImpl getSystemContext() {
        synchronized (this) {
            if (mSystemContext == null) {
                mSystemContext = ContextImpl.createSystemContext(this);
            }
            return mSystemContext;
        }
    }
```



接着调用 createSystemContext：

```
    @UnsupportedAppUsage
    static ContextImpl createSystemContext(ActivityThread mainThread) {
        LoadedApk packageInfo = new LoadedApk(mainThread);
        ContextImpl context = new ContextImpl(null, mainThread, packageInfo, null, null, null, 0,
                null, null);
        context.setResources(packageInfo.getResources());
        context.mResources.updateConfiguration(context.mResourcesManager.getConfiguration(),
                context.mResourcesManager.getDisplayMetrics());
        return context;
    }
```



这里 new 了一个 LoadedApk 对象，这个对象就是我们前面获取到的 `getSystemContext().mPackageInfo` 对象。

我们看下 LoadedApk 的构造函数：

```
    LoadedApk(ActivityThread activityThread) {
        mActivityThread = activityThread;
        mApplicationInfo = new ApplicationInfo();
        mApplicationInfo.packageName = "android";
        mPackageName = "android";
        //.....
    }
```



LoadedApk 对象用来保存一个已经加载了的apk文件的信息，上面的构造方法将使用的包名指定为 android。

我们可以通过以下命令来搜一下源码中报名是 `android` 的模块：

```
find -name "AndroidManifest.xml" | xargs grep "package=\"android\""
```

1  

通过搜索结果，我们可以找到 `frameworks/base/core/res` 这个模块就是 SystemServer 加载的 apk 模块了。

其 Android.bp 文件定义如下：

```
android_app {
    name: "framework-res",
    no_framework_libs: true,
    certificate: "platform",
    aaptflags: [
        "--private-symbols",
        "com.android.internal",
        "--no-auto-version",
        "--auto-add-overlay",
    ],
    export_package_resources: true,
}
```



该模块的名字是 framework-res，主要包括一些 App 共用的系统资源，最终会被编译为 framework-res.apk，并预制到 `/system/app` 目录下。

在 Zygote 启动过程中，preload 函数已经加载了 framework-res.apk 中的资源。所以我们的其他 App 可以直接使用系统资源。

到这里，SystemServer 准备阶段的工作已经完成，后面就是启动一系列的Service了。

启动阶段分成了三步：

```
startBootstrapServices();
startCoreServices();
startOtherServices();
```



我们一步步分析：

startBootstrapServices() 启动的都是一些很基础关键的服务，这些服务都有很复杂的相互依赖性：

```
    // frameworks/base/services/java/com/android/server/SystemServer.java
    private void startBootstrapServices() {
        //......
        // Installer 会通过 binder 关联 installd 服务
        // installd 在 Init 进程中就会被启动，很重要，所以放在第一位
        // installd 支持很多指令，像 ping、rename 都是在其中定义的
        // 后面在 APK 安装篇幅单独介绍
        Installer installer = mSystemServiceManager.startService(Installer.class);
        traceEnd();
        ......
        // 添加设备标识符访问策略服务
        mSystemServiceManager.startService(DeviceIdentifiersPolicyService.class);
        ......
        // 启动 ActivityManagerService，并进行一些关联操作
        mActivityManagerService = mSystemServiceManager.startService(
                ActivityManagerService.Lifecycle.class).getService();
        mActivityManagerService.setSystemServiceManager(mSystemServiceManager);
        mActivityManagerService.setInstaller(installer);
        
        // 启动 PowerManagerService
        mPowerManagerService = mSystemServiceManager.startService(PowerManagerService.class);
        ......
        // 初始化mActivityManagerServicede的电源管理相关功能
        mActivityManagerService.initPowerManagement();
        ......
        // 启动 RecoverySystemService
        // recovery system也是很重要的一个服务
        // 可以触发ota，设置或清除bootloader相关的数据
        mSystemServiceManager.startService(RecoverySystemService.class);
        ......
        // 标记启动事件
        RescueParty.noteBoot(mSystemContext);
        // 启动LightsService，管理LED、背光显示等
        mSystemServiceManager.startService(LightsService.class);
        // 通知所有服务当前状态
        mSystemServiceManager.startBootPhase(SystemService.PHASE_WAIT_FOR_DEFAULT_DISPLAY);
        ......// 省略一些看上去不重要的service
        // 启动PackageManagerService
        mPackageManagerService = PackageManagerService.main(mSystemContext, installer,
                mFactoryTestMode != FactoryTest.FACTORY_TEST_OFF, mOnlyCore);
        mFirstBoot = mPackageManagerService.isFirstBoot();
        mPackageManager = mSystemContext.getPackageManager();
        ......
        // 启动 OverlayManagerService
        OverlayManagerService overlayManagerService = new OverlayManagerService(
                mSystemContext, installer);
        mSystemServiceManager.startService(overlayManagerService);
        ......
        // 启动 SensorService 这是一个 native 方法
        // SensorService 提供的是各种传感器的服务
        startSensorService();
    }
```



服务的启动基本上都是通过 SystemServiceManager 的 startService() 方法。跟踪代码我们会找到 SystemServiceManager 的一个成员变量 ArrayList mServices。SystemServiceManager 管理的其实就是 SystemService 的集合，SystemService 是一个抽象类，如果我们想定义一个系统服务，就需要实现SystemService 这个抽象类。

我们接着看下 startCoreServices()：

```
    private void startCoreServices() {
        traceBeginAndSlog("StartBatteryService");
        // Tracks the battery level.  Requires LightService.
        // 启动电池管理service，依赖 bootstrap 中的 LightService
        // 该服务会定期广播电池的相关状态
        mSystemServiceManager.startService(BatteryService.class);
        traceEnd();

        // Tracks application usage stats.
        traceBeginAndSlog("StartUsageService");
        // 启动应用使用情况数据收集服务
        mSystemServiceManager.startService(UsageStatsService.class);
        mActivityManagerService.setUsageStatsManager(
                LocalServices.getService(UsageStatsManagerInternal.class));
        traceEnd();

        // Tracks whether the updatable WebView is in a ready state and watches for update installs.
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_WEBVIEW)) {
            traceBeginAndSlog("StartWebViewUpdateService");
            mWebViewUpdateService = mSystemServiceManager.startService(WebViewUpdateService.class);
            traceEnd();
        }

        // Tracks and caches the device state.
        traceBeginAndSlog("StartCachedDeviceStateService");
        mSystemServiceManager.startService(CachedDeviceStateService.class);
        traceEnd();

        // Tracks cpu time spent in binder calls
        traceBeginAndSlog("StartBinderCallsStatsService");
        mSystemServiceManager.startService(BinderCallsStatsService.LifeCycle.class);
        traceEnd();

        // Tracks time spent in handling messages in handlers.
        traceBeginAndSlog("StartLooperStatsService");
        mSystemServiceManager.startService(LooperStatsService.Lifecycle.class);
        traceEnd();

        // Manages apk rollbacks.
        traceBeginAndSlog("StartRollbackManagerService");
        mSystemServiceManager.startService(RollbackManagerService.class);
        traceEnd();

        // Service to capture bugreports.
        traceBeginAndSlog("StartBugreportManagerService");
        mSystemServiceManager.startService(BugreportManagerService.class);
        traceEnd();

        // Serivce for GPU and GPU driver.
        traceBeginAndSlog("GpuService");
        mSystemServiceManager.startService(GpuService.class);
        traceEnd();
    }
```



一样的，还是启动一堆的服务。

最后看下 startOtherServices()，这个函数很长，我们精简一下：

```

    private void startOtherServices() {
        // ......
        // 初始化一些比较基础的服务
        // WindowManagerService、NetworkManagementService
        // WatchDog、NetworkPolicyManagerService等
        
        //.....
        // 初始化 UI 相关的服务
        // InputMethodManagerService、AccessibilityManagerService
        // StorageManagerService、NotificationManagerService
        // UiModeManagerService等
        
        //......
        // 此处省略了约800行的各式各样的服务的初始化

        // These are needed to propagate to the runnable below.
        // 将上面初始化好的一些必要的服务在 ActivityManagerService 的 systemReady 中进行进一步的处理
        // 需要进一步处理的服务就是下面这些
        final NetworkManagementService networkManagementF = networkManagement;
        .....
        final IpSecService ipSecServiceF = ipSecService;
        final WindowManagerService windowManagerF = wm;

        // 执行 ActivityManagerService 的 systemReady 方法
        mActivityManagerService.systemReady(() -> {
            //......
            // 标记状态
            mSystemServiceManager.startBootPhase(SystemService.PHASE_ACTIVITY_MANAGER_READY);
            //...... 
            startSystemUi(context, windowManagerF);
            // ......
            // 对前面创建好的服务做进一步的配置，大多是执行一些systemReady操作，如：
            // networkManagementF.systemReady()、ipSecServiceF.systemReady()、networkStatsF.systemReady()
            // connectivityF.systemReady()、networkPolicyF.systemReady(networkPolicyInitReadySignal)
            Watchdog.getInstance().start();
            // Wait for all packages to be prepared
            mPackageManagerService.waitForAppDataPrepared();
            //......
            // 通知所有服务当前状态
            mSystemServiceManager.startBootPhase(SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);
            //......
            // 尝试初次执行一些服务，像定位、地区检测等
            // locationF.systemRunning()、countryDetectorF.systemRunning()、networkTimeUpdaterF.systemRunning()
            ......
        }, BOOT_TIMINGS_TRACE_LOG);
    }
```



SystemServer 的启动阶段最后会调用到 mActivityManagerService.systemReady() 方法：

```
    public void systemReady(final Runnable goingCallback, TimingsTraceLog traceLog) {
        traceLog.traceBegin("PhaseActivityManagerReady");
        synchronized(this) {
            if (mSystemReady) {
                // If we're done calling all the receivers, run the next "boot phase" passed in
                // by the SystemServer
                if (goingCallback != null) {
                     // 执行回调
                    goingCallback.run();
                }
                return;
            }

           // ....
        }

        // ......


        synchronized (this) {
           
            // ......
            if (bootingSystemUser) {
                // 启动桌面Activity
                mAtmInternal.startHomeOnAllDisplays(currentUserId, "systemReady");
            }
            //......
        }
        // ......
    }
```



方法中传入了一个 Runnable 对象，这个对象在方法一开始就会被调用，其主要功能是：

-   启动SystemUI、Watchdog等服务
-   执行一些服务的 systemReady() 和 systemRunning() 方法

后面接着就会调用到 startHomeOnAllDisplays 方法，来启动 Launcher。

## [#](http://ahaoframework.tech/005.%E7%B3%BB%E7%BB%9F%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B%E5%88%86%E6%9E%90/012.Android%20%E7%B3%BB%E7%BB%9F%E5%90%AF%E5%8A%A8%E4%B9%8B%20SystemServer%20%E8%BF%9B%E7%A8%8B%E5%90%AF%E5%8A%A8%E5%88%86%E6%9E%90%E4%BA%8C.html#%E5%8F%82%E8%80%83%E8%B5%84%E6%96%99) 参考资料

-   [深入Android系统（九）Android系统的核心-SystemServer进程 (opens new window)](https://juejin.cn/post/6910805095294697479)
-   [Android Framework（三）——应用程序进程启动流程 \_ (opens new window)](https://www.sukaidev.top/2021/06/03/31b8eb1c/)