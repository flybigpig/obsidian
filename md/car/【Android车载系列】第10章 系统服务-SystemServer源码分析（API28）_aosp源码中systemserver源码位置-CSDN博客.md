![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5cda4e455e188e82656c0b4406f9ca15.png)

## 1 SystemServer启动

&emps;&emps;SystemServer进程启动，首先从SystemServer.java文件的`main()`方法开始。

```
290    /**
291     * The main entry point from zygote.
292     */
293    public static void main(String[] args) {
294        new SystemServer().run();
295    }

...

307    private void run() {

...

426        // Start services.
427        try {
428            traceBeginAndSlog("StartServices");
429            startBootstrapServices();
430            startCoreServices();
431            startOtherServices();
432            SystemServerInitThreadPool.shutdown();
433        } catch (Throwable ex) {
434            Slog.e("System", "******************************************");
435            Slog.e("System", "************ Failure starting system services", ex);
436            throw ex;
437        } finally {
438            traceEnd();
439        }

...

456    }
```

从代码中我们能看到`main()`方法主要调用了`SystemServer.run()`方法，该方法内做了一些初始化，我们来看下主要的一些初始化点，我们主要来看看这里的服务启动：

> 428行启动了80多个服务，最重要的是[AMS](https://so.csdn.net/so/search?q=AMS&spm=1001.2101.3001.7020)，WMS，PKMS，其中三个方法分别启动不同层的服务：  
> `startBootstrapServices();`底层，启动引导服务；  
> `startCoreServices();`用户层，启动核心服务；  
> `startOtherServices();`应用层，启动其它服务。

### 1.1 startBootstrapServices() 启动引导服务

下面我们来看一下底层引导服务的启动方法`startBootstrapServices()`

```
530    /**
531     * Starts the small tangle of critical services that are needed to get
532     * the system off the ground.  These services have complex mutual dependencies
533     * which is why we initialize them all in one place here.  Unless your service
534     * is also entwined in these dependencies, it should be initialized in one of
535     * the other functions.
536     */
537    private void startBootstrapServices() {
538        Slog.i(TAG, "Reading configuration...");
539        final String TAG_SYSTEM_CONFIG = "ReadingSystemConfig";
540        traceBeginAndSlog(TAG_SYSTEM_CONFIG);
541        SystemServerInitThreadPool.get().submit(SystemConfig::getInstance, TAG_SYSTEM_CONFIG);
542        traceEnd();
543
544        // Wait for installd to finish starting up so that it has a chance to
545        // create critical directories such as /data/user with the appropriate
546        // permissions.  We need this to complete before we initialize other services.
547        traceBeginAndSlog("StartInstaller");
548        Installer installer = mSystemServiceManager.startService(Installer.class);
549        traceEnd();
550
551        // In some cases after launching an app we need to access device identifiers,
552        // therefore register the device identifier policy before the activity manager.
553        traceBeginAndSlog("DeviceIdentifiersPolicyService");
554        mSystemServiceManager.startService(DeviceIdentifiersPolicyService.class);
555        traceEnd();
556
557        // Activity manager runs the show.
558        traceBeginAndSlog("StartActivityManager");
559        mActivityManagerService = mSystemServiceManager.startService(
560                ActivityManagerService.Lifecycle.class).getService();
561        mActivityManagerService.setSystemServiceManager(mSystemServiceManager);
562        mActivityManagerService.setInstaller(installer);
563        traceEnd();
564
565        // Power manager needs to be started early because other services need it.
566        // Native daemons may be watching for it to be registered so it must be ready
567        // to handle incoming binder calls immediately (including being able to verify
568        // the permissions for those calls).
569        traceBeginAndSlog("StartPowerManager");
570        mPowerManagerService = mSystemServiceManager.startService(PowerManagerService.class);
571        traceEnd();
572
573        // Now that the power manager has been started, let the activity manager
574        // initialize power management features.
575        traceBeginAndSlog("InitPowerManagement");
576        mActivityManagerService.initPowerManagement();
577        traceEnd();
578
579        // Bring up recovery system in case a rescue party needs a reboot
580        traceBeginAndSlog("StartRecoverySystemService");
581        mSystemServiceManager.startService(RecoverySystemService.class);
582        traceEnd();
583
584        // Now that we have the bare essentials of the OS up and running, take
585        // note that we just booted, which might send out a rescue party if
586        // we're stuck in a runtime restart loop.
587        RescueParty.noteBoot(mSystemContext);
588
589        // Manages LEDs and display backlight so we need it to bring up the display.
590        traceBeginAndSlog("StartLightsService");
591        mSystemServiceManager.startService(LightsService.class);
592        traceEnd();
593
594        traceBeginAndSlog("StartSidekickService");
595        // Package manager isn't started yet; need to use SysProp not hardware feature
596        if (SystemProperties.getBoolean("config.enable_sidekick_graphics", false)) {
597            mSystemServiceManager.startService(WEAR_SIDEKICK_SERVICE_CLASS);
598        }
599        traceEnd();
600
601        // Display manager is needed to provide display metrics before package manager
602        // starts up.
603        traceBeginAndSlog("StartDisplayManager");
604        mDisplayManagerService = mSystemServiceManager.startService(DisplayManagerService.class);
605        traceEnd();
606
607        // We need the default display before we can initialize the package manager.
608        traceBeginAndSlog("WaitForDisplay");
609        mSystemServiceManager.startBootPhase(SystemService.PHASE_WAIT_FOR_DEFAULT_DISPLAY);
610        traceEnd();
611
612        // Only run "core" apps if we're encrypting the device.
613        String cryptState = SystemProperties.get("vold.decrypt");
614        if (ENCRYPTING_STATE.equals(cryptState)) {
615            Slog.w(TAG, "Detected encryption in progress - only parsing core apps");
616            mOnlyCore = true;
617        } else if (ENCRYPTED_STATE.equals(cryptState)) {
618            Slog.w(TAG, "Device encrypted - only parsing core apps");
619            mOnlyCore = true;
620        }
621
622        // Start the package manager.
623        if (!mRuntimeRestart) {
624            MetricsLogger.histogram(null, "boot_package_manager_init_start",
625                    (int) SystemClock.elapsedRealtime());
626        }
627        traceBeginAndSlog("StartPackageManagerService");
628        mPackageManagerService = PackageManagerService.main(mSystemContext, installer,
629                mFactoryTestMode != FactoryTest.FACTORY_TEST_OFF, mOnlyCore);
630        mFirstBoot = mPackageManagerService.isFirstBoot();
631        mPackageManager = mSystemContext.getPackageManager();
632        traceEnd();
633        if (!mRuntimeRestart && !isFirstBootOrUpgrade()) {
634            MetricsLogger.histogram(null, "boot_package_manager_init_ready",
635                    (int) SystemClock.elapsedRealtime());
636        }
637        // Manages A/B OTA dexopting. This is a bootstrap service as we need it to rename
638        // A/B artifacts after boot, before anything else might touch/need them.
639        // Note: this isn't needed during decryption (we don't have /data anyways).
640        if (!mOnlyCore) {
641            boolean disableOtaDexopt = SystemProperties.getBoolean("config.disable_otadexopt",
642                    false);
643            if (!disableOtaDexopt) {
644                traceBeginAndSlog("StartOtaDexOptService");
645                try {
646                    OtaDexoptService.main(mSystemContext, mPackageManagerService);
647                } catch (Throwable e) {
648                    reportWtf("starting OtaDexOptService", e);
649                } finally {
650                    traceEnd();
651                }
652            }
653        }
654
655        traceBeginAndSlog("StartUserManagerService");
656        mSystemServiceManager.startService(UserManagerService.LifeCycle.class);
657        traceEnd();
658
659        // Initialize attribute cache used to cache resources from packages.
660        traceBeginAndSlog("InitAttributerCache");
661        AttributeCache.init(mSystemContext);
662        traceEnd();
663
664        // Set up the Application instance for the system process and get started.
665        traceBeginAndSlog("SetSystemProcess");
666        mActivityManagerService.setSystemProcess();
667        traceEnd();
668
669        // DisplayManagerService needs to setup android.display scheduling related policies
670        // since setSystemProcess() would have overridden policies due to setProcessGroup
671        mDisplayManagerService.setupSchedulerPolicies();
672
673        // Manages Overlay packages
674        traceBeginAndSlog("StartOverlayManagerService");
675        mSystemServiceManager.startService(new OverlayManagerService(mSystemContext, installer));
676        traceEnd();
677
678        // The sensor service needs access to package manager service, app ops
679        // service, and permissions service, therefore we start it after them.
680        // Start sensor service in a separate thread. Completion should be checked
681        // before using it.
682        mSensorServiceStart = SystemServerInitThreadPool.get().submit(() -> {
683            TimingsTraceLog traceLog = new TimingsTraceLog(
684                    SYSTEM_SERVER_TIMING_ASYNC_TAG, Trace.TRACE_TAG_SYSTEM_SERVER);
685            traceLog.traceBegin(START_SENSOR_SERVICE);
686            startSensorService();
687            traceLog.traceEnd();
688        }, START_SENSOR_SERVICE);
689    }
```

引导服务主要有`Installer`、`DeviceIdentifiersPolicyService`、 `ActivityManagerService`、`PowerManagerService`、 `RecoverySystemService` 、 `LightsService` 、 `PackageManagerService`、`UserManagerService`、`OverlayManagerService`等

#### 1.1.1 SystemServiceManager启动管理服务

Java层80多种服务都是通过`SystemServiceManager`类完成初始化和管理。下面我们来看一下`startService(service)`方法代码：

```
54    /**
55     * Starts a service by class name.
56     *
57     * @return The service instance.
58     */
59    @SuppressWarnings("unchecked")
60    public SystemService startService(String className) {
61        final Class<SystemService> serviceClass;
62        try {
63            serviceClass = (Class<SystemService>)Class.forName(className);
64        } catch (ClassNotFoundException ex) {
65            Slog.i(TAG, "Starting " + className);
66            throw new RuntimeException("Failed to create service " + className
67                    + ": service class not found, usually indicates that the caller should "
68                    + "have called PackageManager.hasSystemFeature() to check whether the "
69                    + "feature is available on this device before trying to start the "
70                    + "services that implement it", ex);
71        }
72        return startService(serviceClass);
73    }
74
75    /**
76     * Creates and starts a system service. The class must be a subclass of
77     * {@link com.android.server.SystemService}.
78     *
79     * @param serviceClass A Java class that implements the SystemService interface.
80     * @return The service instance, never null.
81     * @throws RuntimeException if the service fails to start.
82     */
83    @SuppressWarnings("unchecked")
84    public <T extends SystemService> T startService(Class<T> serviceClass) {
85        try {
86            final String name = serviceClass.getName();
87            Slog.i(TAG, "Starting " + name);
88            Trace.traceBegin(Trace.TRACE_TAG_SYSTEM_SERVER, "StartService " + name);
89
90            // Create the service.
91            if (!SystemService.class.isAssignableFrom(serviceClass)) {
92                throw new RuntimeException("Failed to create " + name
93                        + ": service must extend " + SystemService.class.getName());
94            }
95            final T service;
96            try {
97                Constructor<T> constructor = serviceClass.getConstructor(Context.class);
98                service = constructor.newInstance(mContext);
99            } catch (InstantiationException ex) {
100                throw new RuntimeException("Failed to create service " + name
101                        + ": service could not be instantiated", ex);
102            } catch (IllegalAccessException ex) {
103                throw new RuntimeException("Failed to create service " + name
104                        + ": service must have a public constructor with a Context argument", ex);
105            } catch (NoSuchMethodException ex) {
106                throw new RuntimeException("Failed to create service " + name
107                        + ": service must have a public constructor with a Context argument", ex);
108            } catch (InvocationTargetException ex) {
109                throw new RuntimeException("Failed to create service " + name
110                        + ": service constructor threw an exception", ex);
111            }
112
113            startService(service);
114            return service;
115        } finally {
116            Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
117        }
118    }
119
120    public void startService(@NonNull final SystemService service) {
121        // Register it.
122        mServices.add(service);
123        // Start it.
124        long time = SystemClock.elapsedRealtime();
125        try {
126            service.onStart();
127        } catch (RuntimeException ex) {
128            throw new RuntimeException("Failed to start service " + service.getClass().getName()
129                    + ": onStart threw an exception", ex);
130        }
131        warnIfTooLong(SystemClock.elapsedRealtime() - time, service, "onStart");
132    }
```

> 1.`SystemServer`借助`mSystemServiceManager`对服务进行启动和管理  
> 2.`startService()`方法主要通过`反射获取service对象`，然后`将service加到mServices中管理`，`onStart()启动该服务`，`onBootPhase(int phase)`进行回调阶段状态。  
> 3.启动的`AMS、PMS、WMS`等服务都是为了APP提供服务，这些服务都不在一个进程，通过`aidl--->binder进行通信`。

##### 1.1.1.1 生命周期

1.onStart() ：mSystemServiceManager.startService第一时间回调该函数。  
2.onBootPhase(int [phase](https://so.csdn.net/so/search?q=phase&spm=1001.2101.3001.7020)) ： 系统启动的各个阶段会回调该函数

> **SystemService.PHASE\_WAIT\_FOR\_DEFAULT\_DISPLAY**:这是一个依赖项，只有DisplayManagerService中进行了对应处理；  
> **SystemService.PHASE\_LOCK\_SETTINGS\_READY**:经过这个引导阶段后，服务才可以接收到wakelock相关设置数据；  
> **SystemService.PHASE\_SYSTEM\_SERVICES\_READY**:经过这个引导阶段 后，服务才可以安全地使用核心系统服务  
> **SystemService.PHASE\_ACTIVITY\_MANAGER\_READY**:经过这个引导阶 段后，服务可以发送广播  
> **SystemService.PHASE\_THIRD\_PARTY\_APPS\_CAN\_START**:经过这个引导阶段后，服务可以启动第三方应用，第三方应用也可以通过Binder来调用服务。  
> **SystemService.PHASE\_BOOT\_COMPLETED:经过这个引导阶段后**，说明服务启动完成，这时用户就可以  
> 和设备进行交互。

```
134    /**
135     * Starts the specified boot phase for all system services that have been started up to
136     * this point.
137     *
138     * @param phase The boot phase to start.
139     */
140    public void startBootPhase(final int phase) {
141        if (phase <= mCurrentPhase) {
142            throw new IllegalArgumentException("Next phase must be larger than previous");
143        }
144        mCurrentPhase = phase;
145
146        Slog.i(TAG, "Starting phase " + mCurrentPhase);
147        try {
148            Trace.traceBegin(Trace.TRACE_TAG_SYSTEM_SERVER, "OnBootPhase " + phase);
149            final int serviceLen = mServices.size();
150            for (int i = 0; i < serviceLen; i++) {
151                final SystemService service = mServices.get(i);
152                long time = SystemClock.elapsedRealtime();
153                Trace.traceBegin(Trace.TRACE_TAG_SYSTEM_SERVER, service.getClass().getName());
154                try {
155                    service.onBootPhase(mCurrentPhase);
156                } catch (Exception ex) {
157                    throw new RuntimeException("Failed to boot service "
158                            + service.getClass().getName()
159                            + ": onBootPhase threw an exception during phase "
160                            + mCurrentPhase, ex);
161                }
162                warnIfTooLong(SystemClock.elapsedRealtime() - time, service, "onBootPhase");
163                Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
164            }
165        } finally {
166            Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
167        }
168    }
```

#### 1.1.2 AMS服务初始化

我们看回到`startBootstrapServices()`方法的559行，AMS服务进行了初始化。我们看到传入的Service类为Lifecycle.class，其实Lifecycle类就是对ActivityManagerService进行了管理，赋予了生命周期。

##### 1.1.2.1 Lifecycle类

```
2877    public static final class Lifecycle extends SystemService {
2878        private final ActivityManagerService mService;
2879
2880        public Lifecycle(Context context) {
2881            super(context);
2882            mService = new ActivityManagerService(context);
2883        }
2884
2885        @Override
2886        public void onStart() {
2887            mService.start();
2888        }
2889
2890        @Override
2891        public void onBootPhase(int phase) {
2892            mService.mBootPhase = phase;
2893            if (phase == PHASE_SYSTEM_SERVICES_READY) {
2894                mService.mBatteryStatsService.systemServicesReady();
2895                mService.mServices.systemServicesReady();
2896            }
2897        }
2898
2899        @Override
2900        public void onCleanupUser(int userId) {
2901            mService.mBatteryStatsService.onCleanupUser(userId);
2902        }
2903
2904        public ActivityManagerService getService() {
2905            return mService;
2906        }
2907    }
```

##### 1.1.2.2 ActivityManagerService构造函数

```
3045    // Note: This method is invoked on the main thread but may need to attach various
3046    // handlers to other threads.  So take care to be explicit about the looper.
3047    public ActivityManagerService(Context systemContext) {
3048        LockGuard.installLock(this, LockGuard.INDEX_ACTIVITY);
3049        mInjector = new Injector();
3050        mContext = systemContext;
3051
3052        mFactoryTest = FactoryTest.getMode();
3053        mSystemThread = ActivityThread.currentActivityThread();
3054        mUiContext = mSystemThread.getSystemUiContext();
3055
3056        Slog.i(TAG, "Memory class: " + ActivityManager.staticGetMemoryClass());
3057
3058        mPermissionReviewRequired = mContext.getResources().getBoolean(
3059                com.android.internal.R.bool.config_permissionReviewRequired);
3060
3061        mHandlerThread = new ServiceThread(TAG,
3062                THREAD_PRIORITY_FOREGROUND, false /*allowIo*/);
3063        mHandlerThread.start();
3064        mHandler = new MainHandler(mHandlerThread.getLooper());
3065        mUiHandler = mInjector.getUiHandler(this);
3066
3067        mProcStartHandlerThread = new ServiceThread(TAG + ":procStart",
3068                THREAD_PRIORITY_FOREGROUND, false /* allowIo */);
3069        mProcStartHandlerThread.start();
3070        mProcStartHandler = new Handler(mProcStartHandlerThread.getLooper());
3071
3072        mConstants = new ActivityManagerConstants(this, mHandler);
3073
3074        /* static; one-time init here */
3075        if (sKillHandler == null) {
3076            sKillThread = new ServiceThread(TAG + ":kill",
3077                    THREAD_PRIORITY_BACKGROUND, true /* allowIo */);
3078            sKillThread.start();
3079            sKillHandler = new KillHandler(sKillThread.getLooper());
3080        }
3081
3082        mFgBroadcastQueue = new BroadcastQueue(this, mHandler,
3083                "foreground", BROADCAST_FG_TIMEOUT, false);
3084        mBgBroadcastQueue = new BroadcastQueue(this, mHandler,
3085                "background", BROADCAST_BG_TIMEOUT, true);
3086        mBroadcastQueues[0] = mFgBroadcastQueue;
3087        mBroadcastQueues[1] = mBgBroadcastQueue;
3088
3089        mServices = new ActiveServices(this);
3090        mProviderMap = new ProviderMap(this);
3091        mAppErrors = new AppErrors(mUiContext, this);
3092
3093        File dataDir = Environment.getDataDirectory();
3094        File systemDir = new File(dataDir, "system");
3095        systemDir.mkdirs();
3096
3097        mAppWarnings = new AppWarnings(this, mUiContext, mHandler, mUiHandler, systemDir);
3098
3099        // TODO: Move creation of battery stats service outside of activity manager service.
3100        mBatteryStatsService = new BatteryStatsService(systemContext, systemDir, mHandler);
3101        mBatteryStatsService.getActiveStatistics().readLocked();
3102        mBatteryStatsService.scheduleWriteToDisk();
3103        mOnBattery = DEBUG_POWER ? true
3104                : mBatteryStatsService.getActiveStatistics().getIsOnBattery();
3105        mBatteryStatsService.getActiveStatistics().setCallback(this);
3106
3107        mProcessStats = new ProcessStatsService(this, new File(systemDir, "procstats"));
3108
3109        mAppOpsService = mInjector.getAppOpsService(new File(systemDir, "appops.xml"), mHandler);
3110
3111        mGrantFile = new AtomicFile(new File(systemDir, "urigrants.xml"), "uri-grants");
3112
3113        mUserController = new UserController(this);
3114
3115        mVrController = new VrController(this);
3116
3117        GL_ES_VERSION = SystemProperties.getInt("ro.opengles.version",
3118            ConfigurationInfo.GL_ES_VERSION_UNDEFINED);
3119
3120        if (SystemProperties.getInt("sys.use_fifo_ui", 0) != 0) {
3121            mUseFifoUiScheduling = true;
3122        }
3123
3124        mTrackingAssociations = "1".equals(SystemProperties.get("debug.track-associations"));
3125        mTempConfig.setToDefaults();
3126        mTempConfig.setLocales(LocaleList.getDefault());
3127        mConfigurationSeq = mTempConfig.seq = 1;
3128        mStackSupervisor = createStackSupervisor();
3129        mStackSupervisor.onConfigurationChanged(mTempConfig);
3130        mKeyguardController = mStackSupervisor.getKeyguardController();
3131        mCompatModePackages = new CompatModePackages(this, systemDir, mHandler);
3132        mIntentFirewall = new IntentFirewall(new IntentFirewallInterface(), mHandler);
3133        mTaskChangeNotificationController =
3134                new TaskChangeNotificationController(this, mStackSupervisor, mHandler);
3135        mActivityStartController = new ActivityStartController(this);
3136        mRecentTasks = createRecentTasks();
3137        mStackSupervisor.setRecentTasks(mRecentTasks);
3138        mLockTaskController = new LockTaskController(mContext, mStackSupervisor, mHandler);
3139        mLifecycleManager = new ClientLifecycleManager();
3140
3141        mProcessCpuThread = new Thread("CpuTracker") {
3142            @Override
3143            public void run() {
3144                synchronized (mProcessCpuTracker) {
3145                    mProcessCpuInitLatch.countDown();
3146                    mProcessCpuTracker.init();
3147                }
3148                while (true) {
3149                    try {
3150                        try {
3151                            synchronized(this) {
3152                                final long now = SystemClock.uptimeMillis();
3153                                long nextCpuDelay = (mLastCpuTime.get()+MONITOR_CPU_MAX_TIME)-now;
3154                                long nextWriteDelay = (mLastWriteTime+BATTERY_STATS_TIME)-now;
3155                                //Slog.i(TAG, "Cpu delay=" + nextCpuDelay
3156                                //        + ", write delay=" + nextWriteDelay);
3157                                if (nextWriteDelay < nextCpuDelay) {
3158                                    nextCpuDelay = nextWriteDelay;
3159                                }
3160                                if (nextCpuDelay > 0) {
3161                                    mProcessCpuMutexFree.set(true);
3162                                    this.wait(nextCpuDelay);
3163                                }
3164                            }
3165                        } catch (InterruptedException e) {
3166                        }
3167                        updateCpuStatsNow();
3168                    } catch (Exception e) {
3169                        Slog.e(TAG, "Unexpected exception collecting process stats", e);
3170                    }
3171                }
3172            }
3173        };
3174
3175        mHiddenApiBlacklist = new HiddenApiSettings(mHandler, mContext);
3176
3177        Watchdog.getInstance().addMonitor(this);
3178        Watchdog.getInstance().addThread(mHandler);
3179
3180        // bind background thread to little cores
3181        // this is expected to fail inside of framework tests because apps can't touch cpusets directly
3182        // make sure we've already adjusted system_server's internal view of itself first
3183        updateOomAdjLocked();
3184        try {
3185            Process.setThreadGroupAndCpuset(BackgroundThread.get().getThreadId(),
3186                    Process.THREAD_GROUP_BG_NONINTERACTIVE);
3187        } catch (Exception e) {
3188            Slog.w(TAG, "Setting background thread cpuset failed");
3189        }
3190
3191    }
```

ActivityManagerService构造函数主要做了以下逻辑：

> 3094行 创建系统相关目录  
> 3100行 电量服务  
> 3107行 统计应用进程的服务  
> 3128行 activity任务栈  
> 3141行 用于收集CPU信息  
> 3183行 更新oomadj 用于处理系统如何杀死进程用的

#### 1.1.3 设置系统相关的各种服务

我们看到666行通过mActivityManagerService.setSystemProcess()设置了一系列系统服务，下面我们来看看ActivityManagerService类的setSystemProcess()方法：

```
2720    public void setSystemProcess() {
2721        try {
2722            ServiceManager.addService(Context.ACTIVITY_SERVICE, this, /* allowIsolated= */ true,
2723                    DUMP_FLAG_PRIORITY_CRITICAL | DUMP_FLAG_PRIORITY_NORMAL | DUMP_FLAG_PROTO);
2724            ServiceManager.addService(ProcessStats.SERVICE_NAME, mProcessStats);
2725            ServiceManager.addService("meminfo", new MemBinder(this), /* allowIsolated= */ false,
2726                    DUMP_FLAG_PRIORITY_HIGH);
2727            ServiceManager.addService("gfxinfo", new GraphicsBinder(this));
2728            ServiceManager.addService("dbinfo", new DbBinder(this));
2729            if (MONITOR_CPU_USAGE) {
2730                ServiceManager.addService("cpuinfo", new CpuBinder(this),
2731                        /* allowIsolated= */ false, DUMP_FLAG_PRIORITY_CRITICAL);
2732            }
2733            ServiceManager.addService("permission", new PermissionController(this));
2734            ServiceManager.addService("processinfo", new ProcessInfoService(this));
2735
2736            ApplicationInfo info = mContext.getPackageManager().getApplicationInfo(
2737                    "android", STOCK_PM_FLAGS | MATCH_SYSTEM_ONLY);
2738            mSystemThread.installSystemApplicationInfo(info, getClass().getClassLoader());
2739
2740            synchronized (this) {
2741                ProcessRecord app = newProcessRecordLocked(info, info.processName, false, 0);
2742                app.persistent = true;
2743                app.pid = MY_PID;
2744                app.maxAdj = ProcessList.SYSTEM_ADJ;
2745                app.makeActive(mSystemThread.getApplicationThread(), mProcessStats);
2746                synchronized (mPidsSelfLocked) {
2747                    mPidsSelfLocked.put(app.pid, app);
2748                }
2749                updateLruProcessLocked(app, false, null);
2750                updateOomAdjLocked();
2751            }
2752        } catch (PackageManager.NameNotFoundException e) {
2753            throw new RuntimeException(
2754                    "Unable to find android system package", e);
2755        }
2756
2757        // Start watching app ops after we and the package manager are up and running.
2758        mAppOpsService.startWatchingMode(AppOpsManager.OP_RUN_IN_BACKGROUND, null,
2759                new IAppOpsCallback.Stub() {
2760                    @Override public void opChanged(int op, int uid, String packageName) {
2761                        if (op == AppOpsManager.OP_RUN_IN_BACKGROUND && packageName != null) {
2762                            if (mAppOpsService.checkOperation(op, uid, packageName)
2763                                    != AppOpsManager.MODE_ALLOWED) {
2764                                runInBackgroundDisabled(uid);
2765                            }
2766                        }
2767                    }
2768                });
2769    }
```

> 1.ServiceManager是一个java层对c/c++层service\_manager的封装，用来管理binder服务的  
> 2.这里都是binder通信服务，不只是系统用的，还用于暴路接口给其它进程使用，比如： adb shell dumpsys meminfo  
> 3.添加ApplicationInfo 在其它应用进程需要AMS服务时，就是通过他获取的

### 1.2 startCoreServices()启动核心服务

```
691    /**
692     * Starts some essential services that are not tangled up in the bootstrap process.
693     */
694    private void startCoreServices() {
695        traceBeginAndSlog("StartBatteryService");
696        // Tracks the battery level.  Requires LightService.
697        mSystemServiceManager.startService(BatteryService.class);
698        traceEnd();
699
700        // Tracks application usage stats.
701        traceBeginAndSlog("StartUsageService");
702        mSystemServiceManager.startService(UsageStatsService.class);
703        mActivityManagerService.setUsageStatsManager(
704                LocalServices.getService(UsageStatsManagerInternal.class));
705        traceEnd();
706
707        // Tracks whether the updatable WebView is in a ready state and watches for update installs.
708        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_WEBVIEW)) {
709            traceBeginAndSlog("StartWebViewUpdateService");
710            mWebViewUpdateService = mSystemServiceManager.startService(WebViewUpdateService.class);
711            traceEnd();
712        }
713
714        // Tracks cpu time spent in binder calls
715        traceBeginAndSlog("StartBinderCallsStatsService");
716        BinderCallsStatsService.start();
717        traceEnd();
718    }
```

核心服务就几个，分别为：`BatteryService电量服务`、`UsageStatsService用户状态管理服务`、`WebViewUpdateService更新服务`、`BinderCallsStatsService绑定调用状态服务`

### 1.3 startOtherServices()启动其他服务

```
720    /**
721     * Starts a miscellaneous grab bag of stuff that has yet to be refactored
722     * and organized.
723     */
724    private void startOtherServices() {
725        final Context context = mSystemContext;
726        VibratorService vibrator = null;
727        IStorageManager storageManager = null;
728        NetworkManagementService networkManagement = null;
729        IpSecService ipSecService = null;
730        NetworkStatsService networkStats = null;
731        NetworkPolicyManagerService networkPolicy = null;
732        ConnectivityService connectivity = null;
733        NsdService serviceDiscovery= null;
734        WindowManagerService wm = null;
735        SerialService serial = null;
736        NetworkTimeUpdateService networkTimeUpdater = null;
737        CommonTimeManagementService commonTimeMgmtService = null;
738        InputManagerService inputManager = null;
739        TelephonyRegistry telephonyRegistry = null;
740        ConsumerIrService consumerIr = null;
741        MmsServiceBroker mmsService = null;
742        HardwarePropertiesManagerService hardwarePropertiesService = null;
743
744        boolean disableSystemTextClassifier = SystemProperties.getBoolean(
745                "config.disable_systemtextclassifier", false);
746        boolean disableCameraService = SystemProperties.getBoolean("config.disable_cameraservice",
747                false);
748        boolean disableSlices = SystemProperties.getBoolean("config.disable_slices", false);
749        boolean enableLeftyService = SystemProperties.getBoolean("config.enable_lefty", false);
750
751        boolean isEmulator = SystemProperties.get("ro.kernel.qemu").equals("1");
752
753        boolean isWatch = context.getPackageManager().hasSystemFeature(
754                PackageManager.FEATURE_WATCH);
755
756        // For debugging RescueParty
757        if (Build.IS_DEBUGGABLE && SystemProperties.getBoolean("debug.crash_system", false)) {
758            throw new RuntimeException();
759        }
760
761        try {
762            final String SECONDARY_ZYGOTE_PRELOAD = "SecondaryZygotePreload";
763            // We start the preload ~1s before the webview factory preparation, to
764            // ensure that it completes before the 32 bit relro process is forked
765            // from the zygote. In the event that it takes too long, the webview
766            // RELRO process will block, but it will do so without holding any locks.
767            mZygotePreload = SystemServerInitThreadPool.get().submit(() -> {
768                try {
769                    Slog.i(TAG, SECONDARY_ZYGOTE_PRELOAD);
770                    TimingsTraceLog traceLog = new TimingsTraceLog(
771                            SYSTEM_SERVER_TIMING_ASYNC_TAG, Trace.TRACE_TAG_SYSTEM_SERVER);
772                    traceLog.traceBegin(SECONDARY_ZYGOTE_PRELOAD);
773                    if (!Process.zygoteProcess.preloadDefault(Build.SUPPORTED_32_BIT_ABIS[0])) {
774                        Slog.e(TAG, "Unable to preload default resources");
775                    }
776                    traceLog.traceEnd();
777                } catch (Exception ex) {
778                    Slog.e(TAG, "Exception preloading default resources", ex);
779                }
780            }, SECONDARY_ZYGOTE_PRELOAD);
781
782            traceBeginAndSlog("StartKeyAttestationApplicationIdProviderService");
783            ServiceManager.addService("sec_key_att_app_id_provider",
784                    new KeyAttestationApplicationIdProviderService(context));
785            traceEnd();
786
787            traceBeginAndSlog("StartKeyChainSystemService");
788            mSystemServiceManager.startService(KeyChainSystemService.class);
789            traceEnd();
790
791            traceBeginAndSlog("StartSchedulingPolicyService");
792            ServiceManager.addService("scheduling_policy", new SchedulingPolicyService());
793            traceEnd();
794
795            traceBeginAndSlog("StartTelecomLoaderService");
796            mSystemServiceManager.startService(TelecomLoaderService.class);
797            traceEnd();
798
799            traceBeginAndSlog("StartTelephonyRegistry");
800            telephonyRegistry = new TelephonyRegistry(context);
801            ServiceManager.addService("telephony.registry", telephonyRegistry);
802            traceEnd();
803
804            traceBeginAndSlog("StartEntropyMixer");
805            mEntropyMixer = new EntropyMixer(context);
806            traceEnd();
807
808            mContentResolver = context.getContentResolver();
809
810            // The AccountManager must come before the ContentService
811            traceBeginAndSlog("StartAccountManagerService");
812            mSystemServiceManager.startService(ACCOUNT_SERVICE_CLASS);
813            traceEnd();
814
815            traceBeginAndSlog("StartContentService");
816            mSystemServiceManager.startService(CONTENT_SERVICE_CLASS);
817            traceEnd();
818
819            traceBeginAndSlog("InstallSystemProviders");
820            mActivityManagerService.installSystemProviders();
821            // Now that SettingsProvider is ready, reactivate SQLiteCompatibilityWalFlags
822            SQLiteCompatibilityWalFlags.reset();
823            traceEnd();
824
825            // Records errors and logs, for example wtf()
826            // Currently this service indirectly depends on SettingsProvider so do this after
827            // InstallSystemProviders.
828            traceBeginAndSlog("StartDropBoxManager");
829            mSystemServiceManager.startService(DropBoxManagerService.class);
830            traceEnd();
831
832            traceBeginAndSlog("StartVibratorService");
833            vibrator = new VibratorService(context);
834            ServiceManager.addService("vibrator", vibrator);
835            traceEnd();
836
837            if (!isWatch) {
838                traceBeginAndSlog("StartConsumerIrService");
839                consumerIr = new ConsumerIrService(context);
840                ServiceManager.addService(Context.CONSUMER_IR_SERVICE, consumerIr);
841                traceEnd();
842            }
843
844            traceBeginAndSlog("StartAlarmManagerService");
845            mSystemServiceManager.startService(AlarmManagerService.class);
846            traceEnd();
847
848            traceBeginAndSlog("InitWatchdog");
849            final Watchdog watchdog = Watchdog.getInstance();
850            watchdog.init(context, mActivityManagerService);
851            traceEnd();
852
853            traceBeginAndSlog("StartInputManagerService");
854            inputManager = new InputManagerService(context);
855            traceEnd();
856
857            traceBeginAndSlog("StartWindowManagerService");
858            // WMS needs sensor service ready
859            ConcurrentUtils.waitForFutureNoInterrupt(mSensorServiceStart, START_SENSOR_SERVICE);
860            mSensorServiceStart = null;
861            wm = WindowManagerService.main(context, inputManager,
862                    mFactoryTestMode != FactoryTest.FACTORY_TEST_LOW_LEVEL,
863                    !mFirstBoot, mOnlyCore, new PhoneWindowManager());
864            ServiceManager.addService(Context.WINDOW_SERVICE, wm, /* allowIsolated= */ false,
865                    DUMP_FLAG_PRIORITY_CRITICAL | DUMP_FLAG_PROTO);
866            ServiceManager.addService(Context.INPUT_SERVICE, inputManager,
867                    /* allowIsolated= */ false, DUMP_FLAG_PRIORITY_CRITICAL);
868            traceEnd();
869
870            traceBeginAndSlog("SetWindowManagerService");
871            mActivityManagerService.setWindowManager(wm);
872            traceEnd();
873
874            traceBeginAndSlog("WindowManagerServiceOnInitReady");
875            wm.onInitReady();
876            traceEnd();
877
878            // Start receiving calls from HIDL services. Start in in a separate thread
879            // because it need to connect to SensorManager. This have to start
880            // after START_SENSOR_SERVICE is done.
881            SystemServerInitThreadPool.get().submit(() -> {
882                TimingsTraceLog traceLog = new TimingsTraceLog(
883                        SYSTEM_SERVER_TIMING_ASYNC_TAG, Trace.TRACE_TAG_SYSTEM_SERVER);
884                traceLog.traceBegin(START_HIDL_SERVICES);
885                startHidlServices();
886                traceLog.traceEnd();
887            }, START_HIDL_SERVICES);
888
889            if (!isWatch) {
890                traceBeginAndSlog("StartVrManagerService");
891                mSystemServiceManager.startService(VrManagerService.class);
892                traceEnd();
893            }
894
895            traceBeginAndSlog("StartInputManager");
896            inputManager.setWindowManagerCallbacks(wm.getInputMonitor());
897            inputManager.start();
898            traceEnd();
899
900            // TODO: Use service dependencies instead.
901            traceBeginAndSlog("DisplayManagerWindowManagerAndInputReady");
902            mDisplayManagerService.windowManagerAndInputReady();
903            traceEnd();
904
905            // Skip Bluetooth if we have an emulator kernel
906            // TODO: Use a more reliable check to see if this product should
907            // support Bluetooth - see bug 988521
908            if (isEmulator) {
909                Slog.i(TAG, "No Bluetooth Service (emulator)");
910            } else if (mFactoryTestMode == FactoryTest.FACTORY_TEST_LOW_LEVEL) {
911                Slog.i(TAG, "No Bluetooth Service (factory test)");
912            } else if (!context.getPackageManager().hasSystemFeature
913                       (PackageManager.FEATURE_BLUETOOTH)) {
914                Slog.i(TAG, "No Bluetooth Service (Bluetooth Hardware Not Present)");
915            } else {
916                traceBeginAndSlog("StartBluetoothService");
917                mSystemServiceManager.startService(BluetoothService.class);
918                traceEnd();
919            }
920
921            traceBeginAndSlog("IpConnectivityMetrics");
922            mSystemServiceManager.startService(IpConnectivityMetrics.class);
923            traceEnd();
924
925            traceBeginAndSlog("NetworkWatchlistService");
926            mSystemServiceManager.startService(NetworkWatchlistService.Lifecycle.class);
927            traceEnd();
928
929            traceBeginAndSlog("PinnerService");
930            mSystemServiceManager.startService(PinnerService.class);
931            traceEnd();
932        } catch (RuntimeException e) {
933            Slog.e("System", "******************************************");
934            Slog.e("System", "************ Failure starting core service", e);
935        }
936
937        StatusBarManagerService statusBar = null;
938        INotificationManager notification = null;
939        LocationManagerService location = null;
940        CountryDetectorService countryDetector = null;
941        ILockSettings lockSettings = null;
942        MediaRouterService mediaRouter = null;
943
944        // Bring up services needed for UI.
945        if (mFactoryTestMode != FactoryTest.FACTORY_TEST_LOW_LEVEL) {
946            traceBeginAndSlog("StartInputMethodManagerLifecycle");
947            mSystemServiceManager.startService(InputMethodManagerService.Lifecycle.class);
948            traceEnd();
949
950            traceBeginAndSlog("StartAccessibilityManagerService");
951            try {
952                ServiceManager.addService(Context.ACCESSIBILITY_SERVICE,
953                        new AccessibilityManagerService(context));
954            } catch (Throwable e) {
955                reportWtf("starting Accessibility Manager", e);
956            }
957            traceEnd();
958        }
959
960        traceBeginAndSlog("MakeDisplayReady");
961        try {
962            wm.displayReady();
963        } catch (Throwable e) {
964            reportWtf("making display ready", e);
965        }
966        traceEnd();
967
968        if (mFactoryTestMode != FactoryTest.FACTORY_TEST_LOW_LEVEL) {
969            if (!"0".equals(SystemProperties.get("system_init.startmountservice"))) {
970                traceBeginAndSlog("StartStorageManagerService");
971                try {
972                    /*
973                     * NotificationManagerService is dependant on StorageManagerService,
974                     * (for media / usb notifications) so we must start StorageManagerService first.
975                     */
976                    mSystemServiceManager.startService(STORAGE_MANAGER_SERVICE_CLASS);
977                    storageManager = IStorageManager.Stub.asInterface(
978                            ServiceManager.getService("mount"));
979                } catch (Throwable e) {
980                    reportWtf("starting StorageManagerService", e);
981                }
982                traceEnd();
983
984                traceBeginAndSlog("StartStorageStatsService");
985                try {
986                    mSystemServiceManager.startService(STORAGE_STATS_SERVICE_CLASS);
987                } catch (Throwable e) {
988                    reportWtf("starting StorageStatsService", e);
989                }
990                traceEnd();
991            }
992        }
993
994        // We start this here so that we update our configuration to set watch or television
995        // as appropriate.
996        traceBeginAndSlog("StartUiModeManager");
997        mSystemServiceManager.startService(UiModeManagerService.class);
998        traceEnd();
999
1000        if (!mOnlyCore) {
1001            traceBeginAndSlog("UpdatePackagesIfNeeded");
1002            try {
1003                mPackageManagerService.updatePackagesIfNeeded();
1004            } catch (Throwable e) {
1005                reportWtf("update packages", e);
1006            }
1007            traceEnd();
1008        }
1009
1010        traceBeginAndSlog("PerformFstrimIfNeeded");
1011        try {
1012            mPackageManagerService.performFstrimIfNeeded();
1013        } catch (Throwable e) {
1014            reportWtf("performing fstrim", e);
1015        }
1016        traceEnd();
1017
1018        if (mFactoryTestMode != FactoryTest.FACTORY_TEST_LOW_LEVEL) {
1019            traceBeginAndSlog("StartLockSettingsService");
1020            try {
1021                mSystemServiceManager.startService(LOCK_SETTINGS_SERVICE_CLASS);
1022                lockSettings = ILockSettings.Stub.asInterface(
1023                    ServiceManager.getService("lock_settings"));
1024            } catch (Throwable e) {
1025                reportWtf("starting LockSettingsService service", e);
1026            }
1027            traceEnd();
1028
1029            final boolean hasPdb = !SystemProperties.get(PERSISTENT_DATA_BLOCK_PROP).equals("");
1030            if (hasPdb) {
1031                traceBeginAndSlog("StartPersistentDataBlock");
1032                mSystemServiceManager.startService(PersistentDataBlockService.class);
1033                traceEnd();
1034            }
1035
1036            if (hasPdb || OemLockService.isHalPresent()) {
1037                // Implementation depends on pdb or the OemLock HAL
1038                traceBeginAndSlog("StartOemLockService");
1039                mSystemServiceManager.startService(OemLockService.class);
1040                traceEnd();
1041            }
1042
1043            traceBeginAndSlog("StartDeviceIdleController");
1044            mSystemServiceManager.startService(DeviceIdleController.class);
1045            traceEnd();
1046
1047            // Always start the Device Policy Manager, so that the API is compatible with
1048            // API8.
1049            traceBeginAndSlog("StartDevicePolicyManager");
1050            mSystemServiceManager.startService(DevicePolicyManagerService.Lifecycle.class);
1051            traceEnd();
1052
1053            if (!isWatch) {
1054                traceBeginAndSlog("StartStatusBarManagerService");
1055                try {
1056                    statusBar = new StatusBarManagerService(context, wm);
1057                    ServiceManager.addService(Context.STATUS_BAR_SERVICE, statusBar);
1058                } catch (Throwable e) {
1059                    reportWtf("starting StatusBarManagerService", e);
1060                }
1061                traceEnd();
1062            }
1063
1064            traceBeginAndSlog("StartClipboardService");
1065            mSystemServiceManager.startService(ClipboardService.class);
1066            traceEnd();
1067
1068            traceBeginAndSlog("StartNetworkManagementService");
1069            try {
1070                networkManagement = NetworkManagementService.create(context);
1071                ServiceManager.addService(Context.NETWORKMANAGEMENT_SERVICE, networkManagement);
1072            } catch (Throwable e) {
1073                reportWtf("starting NetworkManagement Service", e);
1074            }
1075            traceEnd();
1076
1077            traceBeginAndSlog("StartIpSecService");
1078            try {
1079                ipSecService = IpSecService.create(context);
1080                ServiceManager.addService(Context.IPSEC_SERVICE, ipSecService);
1081            } catch (Throwable e) {
1082                reportWtf("starting IpSec Service", e);
1083            }
1084            traceEnd();
1085
1086            traceBeginAndSlog("StartTextServicesManager");
1087            mSystemServiceManager.startService(TextServicesManagerService.Lifecycle.class);
1088            traceEnd();
1089
1090            if (!disableSystemTextClassifier) {
1091                traceBeginAndSlog("StartTextClassificationManagerService");
1092                mSystemServiceManager.startService(TextClassificationManagerService.Lifecycle.class);
1093                traceEnd();
1094            }
1095
1096            traceBeginAndSlog("StartNetworkScoreService");
1097            mSystemServiceManager.startService(NetworkScoreService.Lifecycle.class);
1098            traceEnd();
1099
1100            traceBeginAndSlog("StartNetworkStatsService");
1101            try {
1102                networkStats = NetworkStatsService.create(context, networkManagement);
1103                ServiceManager.addService(Context.NETWORK_STATS_SERVICE, networkStats);
1104            } catch (Throwable e) {
1105                reportWtf("starting NetworkStats Service", e);
1106            }
1107            traceEnd();
1108
1109            traceBeginAndSlog("StartNetworkPolicyManagerService");
1110            try {
1111                networkPolicy = new NetworkPolicyManagerService(context, mActivityManagerService,
1112                        networkManagement);
1113                ServiceManager.addService(Context.NETWORK_POLICY_SERVICE, networkPolicy);
1114            } catch (Throwable e) {
1115                reportWtf("starting NetworkPolicy Service", e);
1116            }
1117            traceEnd();
1118
1119            if (!mOnlyCore) {
1120                if (context.getPackageManager().hasSystemFeature(
1121                            PackageManager.FEATURE_WIFI)) {
1122                    // Wifi Service must be started first for wifi-related services.
1123                    traceBeginAndSlog("StartWifi");
1124                    mSystemServiceManager.startService(WIFI_SERVICE_CLASS);
1125                    traceEnd();
1126                    traceBeginAndSlog("StartWifiScanning");
1127                    mSystemServiceManager.startService(
1128                        "com.android.server.wifi.scanner.WifiScanningService");
1129                    traceEnd();
1130                }
1131
1132                if (context.getPackageManager().hasSystemFeature(
1133                    PackageManager.FEATURE_WIFI_RTT)) {
1134                    traceBeginAndSlog("StartRttService");
1135                    mSystemServiceManager.startService(
1136                        "com.android.server.wifi.rtt.RttService");
1137                    traceEnd();
1138                }
1139
1140                if (context.getPackageManager().hasSystemFeature(
1141                    PackageManager.FEATURE_WIFI_AWARE)) {
1142                    traceBeginAndSlog("StartWifiAware");
1143                    mSystemServiceManager.startService(WIFI_AWARE_SERVICE_CLASS);
1144                    traceEnd();
1145                }
1146
1147                if (context.getPackageManager().hasSystemFeature(
1148                    PackageManager.FEATURE_WIFI_DIRECT)) {
1149                    traceBeginAndSlog("StartWifiP2P");
1150                    mSystemServiceManager.startService(WIFI_P2P_SERVICE_CLASS);
1151                    traceEnd();
1152                }
1153
1154                if (context.getPackageManager().hasSystemFeature(
1155                    PackageManager.FEATURE_LOWPAN)) {
1156                    traceBeginAndSlog("StartLowpan");
1157                    mSystemServiceManager.startService(LOWPAN_SERVICE_CLASS);
1158                    traceEnd();
1159                }
1160            }
1161
1162            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_ETHERNET) ||
1163                mPackageManager.hasSystemFeature(PackageManager.FEATURE_USB_HOST)) {
1164                traceBeginAndSlog("StartEthernet");
1165                mSystemServiceManager.startService(ETHERNET_SERVICE_CLASS);
1166                traceEnd();
1167            }
1168
1169            traceBeginAndSlog("StartConnectivityService");
1170            try {
1171                connectivity = new ConnectivityService(
1172                    context, networkManagement, networkStats, networkPolicy);
1173                ServiceManager.addService(Context.CONNECTIVITY_SERVICE, connectivity,
1174                            /* allowIsolated= */ false,
1175                    DUMP_FLAG_PRIORITY_HIGH | DUMP_FLAG_PRIORITY_NORMAL);
1176                networkStats.bindConnectivityManager(connectivity);
1177                networkPolicy.bindConnectivityManager(connectivity);
1178            } catch (Throwable e) {
1179                reportWtf("starting Connectivity Service", e);
1180            }
1181            traceEnd();
1182
1183            traceBeginAndSlog("StartNsdService");
1184            try {
1185                serviceDiscovery = NsdService.create(context);
1186                ServiceManager.addService(
1187                    Context.NSD_SERVICE, serviceDiscovery);
1188            } catch (Throwable e) {
1189                reportWtf("starting Service Discovery Service", e);
1190            }
1191            traceEnd();
1192
1193            traceBeginAndSlog("StartSystemUpdateManagerService");
1194            try {
1195                ServiceManager.addService(Context.SYSTEM_UPDATE_SERVICE,
1196                        new SystemUpdateManagerService(context));
1197            } catch (Throwable e) {
1198                reportWtf("starting SystemUpdateManagerService", e);
1199            }
1200            traceEnd();
1201
1202            traceBeginAndSlog("StartUpdateLockService");
1203            try {
1204                ServiceManager.addService(Context.UPDATE_LOCK_SERVICE,
1205                    new UpdateLockService(context));
1206            } catch (Throwable e) {
1207                reportWtf("starting UpdateLockService", e);
1208            }
1209            traceEnd();
1210
1211            traceBeginAndSlog("StartNotificationManager");
1212            mSystemServiceManager.startService(NotificationManagerService.class);
1213            SystemNotificationChannels.createAll(context);
1214            notification = INotificationManager.Stub.asInterface(
1215                    ServiceManager.getService(Context.NOTIFICATION_SERVICE));
1216            traceEnd();
1217
1218            traceBeginAndSlog("StartDeviceMonitor");
1219            mSystemServiceManager.startService(DeviceStorageMonitorService.class);
1220            traceEnd();
1221
1222            traceBeginAndSlog("StartLocationManagerService");
1223            try {
1224                location = new LocationManagerService(context);
1225                ServiceManager.addService(Context.LOCATION_SERVICE, location);
1226            } catch (Throwable e) {
1227                reportWtf("starting Location Manager", e);
1228            }
1229            traceEnd();
1230
1231            traceBeginAndSlog("StartCountryDetectorService");
1232            try {
1233                countryDetector = new CountryDetectorService(context);
1234                ServiceManager.addService(Context.COUNTRY_DETECTOR, countryDetector);
1235            } catch (Throwable e) {
1236                reportWtf("starting Country Detector", e);
1237            }
1238            traceEnd();
1239
1240            if (!isWatch) {
1241                traceBeginAndSlog("StartSearchManagerService");
1242                try {
1243                    mSystemServiceManager.startService(SEARCH_MANAGER_SERVICE_CLASS);
1244                } catch (Throwable e) {
1245                    reportWtf("starting Search Service", e);
1246                }
1247                traceEnd();
1248            }
1249
1250            if (context.getResources().getBoolean(R.bool.config_enableWallpaperService)) {
1251                traceBeginAndSlog("StartWallpaperManagerService");
1252                mSystemServiceManager.startService(WALLPAPER_SERVICE_CLASS);
1253                traceEnd();
1254            }
1255
1256            traceBeginAndSlog("StartAudioService");
1257            mSystemServiceManager.startService(AudioService.Lifecycle.class);
1258            traceEnd();
1259
1260            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_BROADCAST_RADIO)) {
1261                traceBeginAndSlog("StartBroadcastRadioService");
1262                mSystemServiceManager.startService(BroadcastRadioService.class);
1263                traceEnd();
1264            }
1265
1266            traceBeginAndSlog("StartDockObserver");
1267            mSystemServiceManager.startService(DockObserver.class);
1268            traceEnd();
1269
1270            if (isWatch) {
1271                traceBeginAndSlog("StartThermalObserver");
1272                mSystemServiceManager.startService(THERMAL_OBSERVER_CLASS);
1273                traceEnd();
1274            }
1275
1276            traceBeginAndSlog("StartWiredAccessoryManager");
1277            try {
1278                // Listen for wired headset changes
1279                inputManager.setWiredAccessoryCallbacks(
1280                        new WiredAccessoryManager(context, inputManager));
1281            } catch (Throwable e) {
1282                reportWtf("starting WiredAccessoryManager", e);
1283            }
1284            traceEnd();
1285
1286            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_MIDI)) {
1287                // Start MIDI Manager service
1288                traceBeginAndSlog("StartMidiManager");
1289                mSystemServiceManager.startService(MIDI_SERVICE_CLASS);
1290                traceEnd();
1291            }
1292
1293            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_USB_HOST)
1294                || mPackageManager.hasSystemFeature(
1295                PackageManager.FEATURE_USB_ACCESSORY)
1296                || isEmulator) {
1297                // Manage USB host and device support
1298                traceBeginAndSlog("StartUsbService");
1299                mSystemServiceManager.startService(USB_SERVICE_CLASS);
1300                traceEnd();
1301            }
1302
1303            if (!isWatch) {
1304                traceBeginAndSlog("StartSerialService");
1305                try {
1306                    // Serial port support
1307                    serial = new SerialService(context);
1308                    ServiceManager.addService(Context.SERIAL_SERVICE, serial);
1309                } catch (Throwable e) {
1310                    Slog.e(TAG, "Failure starting SerialService", e);
1311                }
1312                traceEnd();
1313            }
1314
1315            traceBeginAndSlog("StartHardwarePropertiesManagerService");
1316            try {
1317                hardwarePropertiesService = new HardwarePropertiesManagerService(context);
1318                ServiceManager.addService(Context.HARDWARE_PROPERTIES_SERVICE,
1319                    hardwarePropertiesService);
1320            } catch (Throwable e) {
1321                Slog.e(TAG, "Failure starting HardwarePropertiesManagerService", e);
1322            }
1323            traceEnd();
1324
1325            traceBeginAndSlog("StartTwilightService");
1326            mSystemServiceManager.startService(TwilightService.class);
1327            traceEnd();
1328
1329            if (ColorDisplayController.isAvailable(context)) {
1330                traceBeginAndSlog("StartNightDisplay");
1331                mSystemServiceManager.startService(ColorDisplayService.class);
1332                traceEnd();
1333            }
1334
1335            traceBeginAndSlog("StartJobScheduler");
1336            mSystemServiceManager.startService(JobSchedulerService.class);
1337            traceEnd();
1338
1339            traceBeginAndSlog("StartSoundTrigger");
1340            mSystemServiceManager.startService(SoundTriggerService.class);
1341            traceEnd();
1342
1343            traceBeginAndSlog("StartTrustManager");
1344            mSystemServiceManager.startService(TrustManagerService.class);
1345            traceEnd();
1346
1347            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_BACKUP)) {
1348                traceBeginAndSlog("StartBackupManager");
1349                mSystemServiceManager.startService(BACKUP_MANAGER_SERVICE_CLASS);
1350                traceEnd();
1351            }
1352
1353            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_APP_WIDGETS)
1354                || context.getResources().getBoolean(R.bool.config_enableAppWidgetService)) {
1355                traceBeginAndSlog("StartAppWidgerService");
1356                mSystemServiceManager.startService(APPWIDGET_SERVICE_CLASS);
1357                traceEnd();
1358            }
1359
1360            // We need to always start this service, regardless of whether the
1361            // FEATURE_VOICE_RECOGNIZERS feature is set, because it needs to take care
1362            // of initializing various settings.  It will internally modify its behavior
1363            // based on that feature.
1364            traceBeginAndSlog("StartVoiceRecognitionManager");
1365            mSystemServiceManager.startService(VOICE_RECOGNITION_MANAGER_SERVICE_CLASS);
1366            traceEnd();
1367
1368            if (GestureLauncherService.isGestureLauncherEnabled(context.getResources())) {
1369                traceBeginAndSlog("StartGestureLauncher");
1370                mSystemServiceManager.startService(GestureLauncherService.class);
1371                traceEnd();
1372            }
1373            traceBeginAndSlog("StartSensorNotification");
1374            mSystemServiceManager.startService(SensorNotificationService.class);
1375            traceEnd();
1376
1377            traceBeginAndSlog("StartContextHubSystemService");
1378            mSystemServiceManager.startService(ContextHubSystemService.class);
1379            traceEnd();
1380
1381            traceBeginAndSlog("StartDiskStatsService");
1382            try {
1383                ServiceManager.addService("diskstats", new DiskStatsService(context));
1384            } catch (Throwable e) {
1385                reportWtf("starting DiskStats Service", e);
1386            }
1387            traceEnd();
1388
1389            // timezone.RulesManagerService will prevent a device starting up if the chain of trust
1390            // required for safe time zone updates might be broken. RuleManagerService cannot do
1391            // this check when mOnlyCore == true, so we don't enable the service in this case.
1392            // This service requires that JobSchedulerService is already started when it starts.
1393            final boolean startRulesManagerService =
1394                    !mOnlyCore && context.getResources().getBoolean(
1395                            R.bool.config_enableUpdateableTimeZoneRules);
1396            if (startRulesManagerService) {
1397                traceBeginAndSlog("StartTimeZoneRulesManagerService");
1398                mSystemServiceManager.startService(TIME_ZONE_RULES_MANAGER_SERVICE_CLASS);
1399                traceEnd();
1400            }
1401
1402            if (!isWatch) {
1403                traceBeginAndSlog("StartNetworkTimeUpdateService");
1404                try {
1405                    networkTimeUpdater = new NetworkTimeUpdateService(context);
1406                    ServiceManager.addService("network_time_update_service", networkTimeUpdater);
1407                } catch (Throwable e) {
1408                    reportWtf("starting NetworkTimeUpdate service", e);
1409                }
1410                traceEnd();
1411            }
1412
1413            traceBeginAndSlog("StartCommonTimeManagementService");
1414            try {
1415                commonTimeMgmtService = new CommonTimeManagementService(context);
1416                ServiceManager.addService("commontime_management", commonTimeMgmtService);
1417            } catch (Throwable e) {
1418                reportWtf("starting CommonTimeManagementService service", e);
1419            }
1420            traceEnd();
1421
1422            traceBeginAndSlog("CertBlacklister");
1423            try {
1424                CertBlacklister blacklister = new CertBlacklister(context);
1425            } catch (Throwable e) {
1426                reportWtf("starting CertBlacklister", e);
1427            }
1428            traceEnd();
1429
1430            if (EmergencyAffordanceManager.ENABLED) {
1431                // EmergencyMode service
1432                traceBeginAndSlog("StartEmergencyAffordanceService");
1433                mSystemServiceManager.startService(EmergencyAffordanceService.class);
1434                traceEnd();
1435            }
1436
1437            // Dreams (interactive idle-time views, a/k/a screen savers, and doze mode)
1438            traceBeginAndSlog("StartDreamManager");
1439            mSystemServiceManager.startService(DreamManagerService.class);
1440            traceEnd();
1441
1442            traceBeginAndSlog("AddGraphicsStatsService");
1443            ServiceManager.addService(GraphicsStatsService.GRAPHICS_STATS_SERVICE,
1444                new GraphicsStatsService(context));
1445            traceEnd();
1446
1447            if (CoverageService.ENABLED) {
1448                traceBeginAndSlog("AddCoverageService");
1449                ServiceManager.addService(CoverageService.COVERAGE_SERVICE, new CoverageService());
1450                traceEnd();
1451            }
1452
1453            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_PRINTING)) {
1454                traceBeginAndSlog("StartPrintManager");
1455                mSystemServiceManager.startService(PRINT_MANAGER_SERVICE_CLASS);
1456                traceEnd();
1457            }
1458
1459            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_COMPANION_DEVICE_SETUP)) {
1460                traceBeginAndSlog("StartCompanionDeviceManager");
1461                mSystemServiceManager.startService(COMPANION_DEVICE_MANAGER_SERVICE_CLASS);
1462                traceEnd();
1463            }
1464
1465            traceBeginAndSlog("StartRestrictionManager");
1466            mSystemServiceManager.startService(RestrictionsManagerService.class);
1467            traceEnd();
1468
1469            traceBeginAndSlog("StartMediaSessionService");
1470            mSystemServiceManager.startService(MediaSessionService.class);
1471            traceEnd();
1472
1473            traceBeginAndSlog("StartMediaUpdateService");
1474            mSystemServiceManager.startService(MediaUpdateService.class);
1475            traceEnd();
1476
1477            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_HDMI_CEC)) {
1478                traceBeginAndSlog("StartHdmiControlService");
1479                mSystemServiceManager.startService(HdmiControlService.class);
1480                traceEnd();
1481            }
1482
1483            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_LIVE_TV)
1484                    || mPackageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK)) {
1485                traceBeginAndSlog("StartTvInputManager");
1486                mSystemServiceManager.startService(TvInputManagerService.class);
1487                traceEnd();
1488            }
1489
1490            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)) {
1491                traceBeginAndSlog("StartMediaResourceMonitor");
1492                mSystemServiceManager.startService(MediaResourceMonitorService.class);
1493                traceEnd();
1494            }
1495
1496            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK)) {
1497                traceBeginAndSlog("StartTvRemoteService");
1498                mSystemServiceManager.startService(TvRemoteService.class);
1499                traceEnd();
1500            }
1501
1502            traceBeginAndSlog("StartMediaRouterService");
1503            try {
1504                mediaRouter = new MediaRouterService(context);
1505                ServiceManager.addService(Context.MEDIA_ROUTER_SERVICE, mediaRouter);
1506            } catch (Throwable e) {
1507                reportWtf("starting MediaRouterService", e);
1508            }
1509            traceEnd();
1510
1511            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_FINGERPRINT)) {
1512                traceBeginAndSlog("StartFingerprintSensor");
1513                mSystemServiceManager.startService(FingerprintService.class);
1514                traceEnd();
1515            }
1516
1517            traceBeginAndSlog("StartBackgroundDexOptService");
1518            try {
1519                BackgroundDexOptService.schedule(context);
1520            } catch (Throwable e) {
1521                reportWtf("starting StartBackgroundDexOptService", e);
1522            }
1523            traceEnd();
1524
1525            traceBeginAndSlog("StartPruneInstantAppsJobService");
1526            try {
1527                PruneInstantAppsJobService.schedule(context);
1528            } catch (Throwable e) {
1529                reportWtf("StartPruneInstantAppsJobService", e);
1530            }
1531            traceEnd();
1532
1533            // LauncherAppsService uses ShortcutService.
1534            traceBeginAndSlog("StartShortcutServiceLifecycle");
1535            mSystemServiceManager.startService(ShortcutService.Lifecycle.class);
1536            traceEnd();
1537
1538            traceBeginAndSlog("StartLauncherAppsService");
1539            mSystemServiceManager.startService(LauncherAppsService.class);
1540            traceEnd();
1541
1542            traceBeginAndSlog("StartCrossProfileAppsService");
1543            mSystemServiceManager.startService(CrossProfileAppsService.class);
1544            traceEnd();
1545        }
1546
1547        if (!isWatch) {
1548            traceBeginAndSlog("StartMediaProjectionManager");
1549            mSystemServiceManager.startService(MediaProjectionManagerService.class);
1550            traceEnd();
1551        }
1552
1553        if (isWatch) {
1554            traceBeginAndSlog("StartWearConfigService");
1555            mSystemServiceManager.startService(WEAR_CONFIG_SERVICE_CLASS);
1556            traceEnd();
1557
1558            traceBeginAndSlog("StartWearConnectivityService");
1559            mSystemServiceManager.startService(WEAR_CONNECTIVITY_SERVICE_CLASS);
1560            traceEnd();
1561
1562            traceBeginAndSlog("StartWearTimeService");
1563            mSystemServiceManager.startService(WEAR_DISPLAY_SERVICE_CLASS);
1564            mSystemServiceManager.startService(WEAR_TIME_SERVICE_CLASS);
1565            traceEnd();
1566
1567            if (enableLeftyService) {
1568                traceBeginAndSlog("StartWearLeftyService");
1569                mSystemServiceManager.startService(WEAR_LEFTY_SERVICE_CLASS);
1570                traceEnd();
1571            }
1572
1573            traceBeginAndSlog("StartWearGlobalActionsService");
1574            mSystemServiceManager.startService(WEAR_GLOBAL_ACTIONS_SERVICE_CLASS);
1575            traceEnd();
1576        }
1577
1578        if (!disableSlices) {
1579            traceBeginAndSlog("StartSliceManagerService");
1580            mSystemServiceManager.startService(SLICE_MANAGER_SERVICE_CLASS);
1581            traceEnd();
1582        }
1583
1584        if (!disableCameraService) {
1585            traceBeginAndSlog("StartCameraServiceProxy");
1586            mSystemServiceManager.startService(CameraServiceProxy.class);
1587            traceEnd();
1588        }
1589
1590        if (context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_EMBEDDED)) {
1591            traceBeginAndSlog("StartIoTSystemService");
1592            mSystemServiceManager.startService(IOT_SERVICE_CLASS);
1593            traceEnd();
1594        }
1595
1596        // Statsd helper
1597        traceBeginAndSlog("StartStatsCompanionService");
1598        mSystemServiceManager.startService(StatsCompanionService.Lifecycle.class);
1599        traceEnd();
1600
1601        // Before things start rolling, be sure we have decided whether
1602        // we are in safe mode.
1603        final boolean safeMode = wm.detectSafeMode();
1604        if (safeMode) {
1605            traceBeginAndSlog("EnterSafeModeAndDisableJitCompilation");
1606            mActivityManagerService.enterSafeMode();
1607            // Disable the JIT for the system_server process
1608            VMRuntime.getRuntime().disableJitCompilation();
1609            traceEnd();
1610        } else {
1611            // Enable the JIT for the system_server process
1612            traceBeginAndSlog("StartJitCompilation");
1613            VMRuntime.getRuntime().startJitCompilation();
1614            traceEnd();
1615        }
1616
1617        // MMS service broker
1618        traceBeginAndSlog("StartMmsService");
1619        mmsService = mSystemServiceManager.startService(MmsServiceBroker.class);
1620        traceEnd();
1621
1622        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_AUTOFILL)) {
1623            traceBeginAndSlog("StartAutoFillService");
1624            mSystemServiceManager.startService(AUTO_FILL_MANAGER_SERVICE_CLASS);
1625            traceEnd();
1626        }
1627
1628        // It is now time to start up the app processes...
1629
1630        traceBeginAndSlog("MakeVibratorServiceReady");
1631        try {
1632            vibrator.systemReady();
1633        } catch (Throwable e) {
1634            reportWtf("making Vibrator Service ready", e);
1635        }
1636        traceEnd();
1637
1638        traceBeginAndSlog("MakeLockSettingsServiceReady");
1639        if (lockSettings != null) {
1640            try {
1641                lockSettings.systemReady();
1642            } catch (Throwable e) {
1643                reportWtf("making Lock Settings Service ready", e);
1644            }
1645        }
1646        traceEnd();
1647
1648        // Needed by DevicePolicyManager for initialization
1649        traceBeginAndSlog("StartBootPhaseLockSettingsReady");
1650        mSystemServiceManager.startBootPhase(SystemService.PHASE_LOCK_SETTINGS_READY);
1651        traceEnd();
1652
1653        traceBeginAndSlog("StartBootPhaseSystemServicesReady");
1654        mSystemServiceManager.startBootPhase(SystemService.PHASE_SYSTEM_SERVICES_READY);
1655        traceEnd();
1656
1657        traceBeginAndSlog("MakeWindowManagerServiceReady");
1658        try {
1659            wm.systemReady();
1660        } catch (Throwable e) {
1661            reportWtf("making Window Manager Service ready", e);
1662        }
1663        traceEnd();
1664
1665        if (safeMode) {
1666            mActivityManagerService.showSafeModeOverlay();
1667        }
1668
1669        // Update the configuration for this context by hand, because we're going
1670        // to start using it before the config change done in wm.systemReady() will
1671        // propagate to it.
1672        final Configuration config = wm.computeNewConfiguration(DEFAULT_DISPLAY);
1673        DisplayMetrics metrics = new DisplayMetrics();
1674        WindowManager w = (WindowManager)context.getSystemService(Context.WINDOW_SERVICE);
1675        w.getDefaultDisplay().getMetrics(metrics);
1676        context.getResources().updateConfiguration(config, metrics);
1677
1678        // The system context's theme may be configuration-dependent.
1679        final Theme systemTheme = context.getTheme();
1680        if (systemTheme.getChangingConfigurations() != 0) {
1681            systemTheme.rebase();
1682        }
1683
1684        traceBeginAndSlog("MakePowerManagerServiceReady");
1685        try {
1686            // TODO: use boot phase
1687            mPowerManagerService.systemReady(mActivityManagerService.getAppOpsService());
1688        } catch (Throwable e) {
1689            reportWtf("making Power Manager Service ready", e);
1690        }
1691        traceEnd();
1692
1693        traceBeginAndSlog("MakePackageManagerServiceReady");
1694        mPackageManagerService.systemReady();
1695        traceEnd();
1696
1697        traceBeginAndSlog("MakeDisplayManagerServiceReady");
1698        try {
1699            // TODO: use boot phase and communicate these flags some other way
1700            mDisplayManagerService.systemReady(safeMode, mOnlyCore);
1701        } catch (Throwable e) {
1702            reportWtf("making Display Manager Service ready", e);
1703        }
1704        traceEnd();
1705
1706        mSystemServiceManager.setSafeMode(safeMode);
1707
1708        // Start device specific services
1709        traceBeginAndSlog("StartDeviceSpecificServices");
1710        final String[] classes = mSystemContext.getResources().getStringArray(
1711                R.array.config_deviceSpecificSystemServices);
1712        for (final String className : classes) {
1713            traceBeginAndSlog("StartDeviceSpecificServices " + className);
1714            try {
1715                mSystemServiceManager.startService(className);
1716            } catch (Throwable e) {
1717                reportWtf("starting " + className, e);
1718            }
1719            traceEnd();
1720        }
1721        traceEnd();
1722
1723        traceBeginAndSlog("StartBootPhaseDeviceSpecificServicesReady");
1724        mSystemServiceManager.startBootPhase(SystemService.PHASE_DEVICE_SPECIFIC_SERVICES_READY);
1725        traceEnd();
1726
1727        // These are needed to propagate to the runnable below.
1728        final NetworkManagementService networkManagementF = networkManagement;
1729        final NetworkStatsService networkStatsF = networkStats;
1730        final NetworkPolicyManagerService networkPolicyF = networkPolicy;
1731        final ConnectivityService connectivityF = connectivity;
1732        final LocationManagerService locationF = location;
1733        final CountryDetectorService countryDetectorF = countryDetector;
1734        final NetworkTimeUpdateService networkTimeUpdaterF = networkTimeUpdater;
1735        final CommonTimeManagementService commonTimeMgmtServiceF = commonTimeMgmtService;
1736        final InputManagerService inputManagerF = inputManager;
1737        final TelephonyRegistry telephonyRegistryF = telephonyRegistry;
1738        final MediaRouterService mediaRouterF = mediaRouter;
1739        final MmsServiceBroker mmsServiceF = mmsService;
1740        final IpSecService ipSecServiceF = ipSecService;
1741        final WindowManagerService windowManagerF = wm;
1742
1743        // We now tell the activity manager it is okay to run third party
1744        // code.  It will call back into us once it has gotten to the state
1745        // where third party code can really run (but before it has actually
1746        // started launching the initial applications), for us to complete our
1747        // initialization.
1748        mActivityManagerService.systemReady(() -> {
1749            Slog.i(TAG, "Making services ready");
1750            traceBeginAndSlog("StartActivityManagerReadyPhase");
1751            mSystemServiceManager.startBootPhase(
1752                    SystemService.PHASE_ACTIVITY_MANAGER_READY);
1753            traceEnd();
1754            traceBeginAndSlog("StartObservingNativeCrashes");
1755            try {
1756                mActivityManagerService.startObservingNativeCrashes();
1757            } catch (Throwable e) {
1758                reportWtf("observing native crashes", e);
1759            }
1760            traceEnd();
1761
1762            // No dependency on Webview preparation in system server. But this should
1763            // be completed before allowing 3rd party
1764            final String WEBVIEW_PREPARATION = "WebViewFactoryPreparation";
1765            Future<?> webviewPrep = null;
1766            if (!mOnlyCore && mWebViewUpdateService != null) {
1767                webviewPrep = SystemServerInitThreadPool.get().submit(() -> {
1768                    Slog.i(TAG, WEBVIEW_PREPARATION);
1769                    TimingsTraceLog traceLog = new TimingsTraceLog(
1770                            SYSTEM_SERVER_TIMING_ASYNC_TAG, Trace.TRACE_TAG_SYSTEM_SERVER);
1771                    traceLog.traceBegin(WEBVIEW_PREPARATION);
1772                    ConcurrentUtils.waitForFutureNoInterrupt(mZygotePreload, "Zygote preload");
1773                    mZygotePreload = null;
1774                    mWebViewUpdateService.prepareWebViewInSystemServer();
1775                    traceLog.traceEnd();
1776                }, WEBVIEW_PREPARATION);
1777            }
1778
1779            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE)) {
1780                traceBeginAndSlog("StartCarServiceHelperService");
1781                mSystemServiceManager.startService(CAR_SERVICE_HELPER_SERVICE_CLASS);
1782                traceEnd();
1783            }
1784
1785            traceBeginAndSlog("StartSystemUI");
1786            try {
1787                startSystemUi(context, windowManagerF);
1788            } catch (Throwable e) {
1789                reportWtf("starting System UI", e);
1790            }
1791            traceEnd();
1792            traceBeginAndSlog("MakeNetworkManagementServiceReady");
1793            try {
1794                if (networkManagementF != null) networkManagementF.systemReady();
1795            } catch (Throwable e) {
1796                reportWtf("making Network Managment Service ready", e);
1797            }
1798            CountDownLatch networkPolicyInitReadySignal = null;
1799            if (networkPolicyF != null) {
1800                networkPolicyInitReadySignal = networkPolicyF
1801                        .networkScoreAndNetworkManagementServiceReady();
1802            }
1803            traceEnd();
1804            traceBeginAndSlog("MakeIpSecServiceReady");
1805            try {
1806                if (ipSecServiceF != null) ipSecServiceF.systemReady();
1807            } catch (Throwable e) {
1808                reportWtf("making IpSec Service ready", e);
1809            }
1810            traceEnd();
1811            traceBeginAndSlog("MakeNetworkStatsServiceReady");
1812            try {
1813                if (networkStatsF != null) networkStatsF.systemReady();
1814            } catch (Throwable e) {
1815                reportWtf("making Network Stats Service ready", e);
1816            }
1817            traceEnd();
1818            traceBeginAndSlog("MakeConnectivityServiceReady");
1819            try {
1820                if (connectivityF != null) connectivityF.systemReady();
1821            } catch (Throwable e) {
1822                reportWtf("making Connectivity Service ready", e);
1823            }
1824            traceEnd();
1825            traceBeginAndSlog("MakeNetworkPolicyServiceReady");
1826            try {
1827                if (networkPolicyF != null) {
1828                    networkPolicyF.systemReady(networkPolicyInitReadySignal);
1829                }
1830            } catch (Throwable e) {
1831                reportWtf("making Network Policy Service ready", e);
1832            }
1833            traceEnd();
1834
1835            traceBeginAndSlog("StartWatchdog");
1836            Watchdog.getInstance().start();
1837            traceEnd();
1838
1839            // Wait for all packages to be prepared
1840            mPackageManagerService.waitForAppDataPrepared();
1841
1842            // It is now okay to let the various system services start their
1843            // third party code...
1844            traceBeginAndSlog("PhaseThirdPartyAppsCanStart");
1845            // confirm webview completion before starting 3rd party
1846            if (webviewPrep != null) {
1847                ConcurrentUtils.waitForFutureNoInterrupt(webviewPrep, WEBVIEW_PREPARATION);
1848            }
1849            mSystemServiceManager.startBootPhase(
1850                    SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);
1851            traceEnd();
1852
1853            traceBeginAndSlog("MakeLocationServiceReady");
1854            try {
1855                if (locationF != null) locationF.systemRunning();
1856            } catch (Throwable e) {
1857                reportWtf("Notifying Location Service running", e);
1858            }
1859            traceEnd();
1860            traceBeginAndSlog("MakeCountryDetectionServiceReady");
1861            try {
1862                if (countryDetectorF != null) countryDetectorF.systemRunning();
1863            } catch (Throwable e) {
1864                reportWtf("Notifying CountryDetectorService running", e);
1865            }
1866            traceEnd();
1867            traceBeginAndSlog("MakeNetworkTimeUpdateReady");
1868            try {
1869                if (networkTimeUpdaterF != null) networkTimeUpdaterF.systemRunning();
1870            } catch (Throwable e) {
1871                reportWtf("Notifying NetworkTimeService running", e);
1872            }
1873            traceEnd();
1874            traceBeginAndSlog("MakeCommonTimeManagementServiceReady");
1875            try {
1876                if (commonTimeMgmtServiceF != null) {
1877                    commonTimeMgmtServiceF.systemRunning();
1878                }
1879            } catch (Throwable e) {
1880                reportWtf("Notifying CommonTimeManagementService running", e);
1881            }
1882            traceEnd();
1883            traceBeginAndSlog("MakeInputManagerServiceReady");
1884            try {
1885                // TODO(BT) Pass parameter to input manager
1886                if (inputManagerF != null) inputManagerF.systemRunning();
1887            } catch (Throwable e) {
1888                reportWtf("Notifying InputManagerService running", e);
1889            }
1890            traceEnd();
1891            traceBeginAndSlog("MakeTelephonyRegistryReady");
1892            try {
1893                if (telephonyRegistryF != null) telephonyRegistryF.systemRunning();
1894            } catch (Throwable e) {
1895                reportWtf("Notifying TelephonyRegistry running", e);
1896            }
1897            traceEnd();
1898            traceBeginAndSlog("MakeMediaRouterServiceReady");
1899            try {
1900                if (mediaRouterF != null) mediaRouterF.systemRunning();
1901            } catch (Throwable e) {
1902                reportWtf("Notifying MediaRouterService running", e);
1903            }
1904            traceEnd();
1905            traceBeginAndSlog("MakeMmsServiceReady");
1906            try {
1907                if (mmsServiceF != null) mmsServiceF.systemRunning();
1908            } catch (Throwable e) {
1909                reportWtf("Notifying MmsService running", e);
1910            }
1911            traceEnd();
1912
1913            traceBeginAndSlog("IncidentDaemonReady");
1914            try {
1915                // TODO: Switch from checkService to getService once it's always
1916                // in the build and should reliably be there.
1917                final IIncidentManager incident = IIncidentManager.Stub.asInterface(
1918                        ServiceManager.getService(Context.INCIDENT_SERVICE));
1919                if (incident != null) incident.systemRunning();
1920            } catch (Throwable e) {
1921                reportWtf("Notifying incident daemon running", e);
1922            }
1923            traceEnd();
1924        }, BOOT_TIMINGS_TRACE_LOG);
1925    }
```

这里代码高达1200+行(不用全看)，启动了应用层使用的各种服务几十个，主要有`KeyChainSystemService`、`TelecomLoaderService`、`AccountManagerService`、`ContentService`、`DropBoxManagerService`、`VibratorService`、`AlarmManagerService`、`Watchdog`、 `InputManagerService`、`WindowManagerService`、`IpConnectivityMetrics`、`NetworkWatchlistService`、`PinnerService`等服务。`mActivityManagerService.systemReady()`方法后80多个服务已经创建并启动了，需要注意：并不是所有的服务都一定是startService启动。

`mActivityManagerService.systemReady()`方法中通过`startHomeActivityLocked(currentUserId, "systemReady")` 启动了Launch.java。接下来就使用`execute`进行APP初始化操作比如：`socket通信`，`Binder通信`，`zgote fork 进程`，`冷启动`，`热启动`

## 2 车载服务启动

车载服务的启动也归属于其它服务，在`startOtherServices()`方法被启动。

```
1779            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE)) {
1780                traceBeginAndSlog("StartCarServiceHelperService");
1781                mSystemServiceManager.startService(CAR_SERVICE_HELPER_SERVICE_CLASS);
1782                traceEnd();
1783            }
```

在车载服务创建启动前，判断是否为车载平台，是则通过反射`com.android.internal.car.CarServiceHelperService`服务名进行创建。下面我们来看看`CarServiceHelperService`车载服务类：

```
33/**
34 * System service side companion service for CarService.
35 * Starts car service and provide necessary API for CarService. Only for car product.
36 */
37public class CarServiceHelperService extends SystemService {
38    private static final String TAG = "CarServiceHelper";
39    private static final String CAR_SERVICE_INTERFACE = "android.car.ICar";
40    private final ICarServiceHelperImpl mHelper = new ICarServiceHelperImpl();
41    private final Context mContext;
42    private IBinder mCarService;
43    private final ServiceConnection mCarServiceConnection = new ServiceConnection() {
44
45        @Override
46        public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
47            Slog.i(TAG, "**CarService connected**");
48            mCarService = iBinder;
49            // Cannot depend on ICar which is defined in CarService, so handle binder call directly
50            // instead.
51            // void setCarServiceHelper(in IBinder helper)
52            Parcel data = Parcel.obtain();
53            data.writeInterfaceToken(CAR_SERVICE_INTERFACE);
54            data.writeStrongBinder(mHelper.asBinder());
55            try {
56                mCarService.transact(IBinder.FIRST_CALL_TRANSACTION, // setCarServiceHelper
57                        data, null, Binder.FLAG_ONEWAY);
58            } catch (RemoteException e) {
59                Slog.w(TAG, "RemoteException from car service", e);
60                handleCarServiceCrash();
61            }
62        }
63
64        @Override
65        public void onServiceDisconnected(ComponentName componentName) {
66            handleCarServiceCrash();
67        }
68    };
69
70    public CarServiceHelperService(Context context) {
71        super(context);
72        mContext = context;
73    }
74
75    @Override
76    public void onStart() {
77        Intent intent = new Intent();
78        intent.setPackage("com.android.car");
79        intent.setAction(CAR_SERVICE_INTERFACE);
80        if (!getContext().bindServiceAsUser(intent, mCarServiceConnection, Context.BIND_AUTO_CREATE,
81                UserHandle.SYSTEM)) {
82            Slog.wtf(TAG, "cannot start car service");
83        }
84        System.loadLibrary("car-framework-service-jni");
85    }
86
87    private void handleCarServiceCrash() {
88        //TODO define recovery bahavior
89    }
90
91    private static native int nativeForceSuspend(int timeoutMs);
92
93    private class ICarServiceHelperImpl extends ICarServiceHelper.Stub {
94       /**
95         * Force device to suspend
96         *
97         * @param timeoutMs
98         */
99        @Override // Binder call
100        public int forceSuspend(int timeoutMs) {
101            int retVal;
102            mContext.enforceCallingOrSelfPermission(android.Manifest.permission.DEVICE_POWER, null);
103            final long ident = Binder.clearCallingIdentity();
104            try {
105                retVal = nativeForceSuspend(timeoutMs);
106            } finally {
107                Binder.restoreCallingIdentity(ident);
108            }
109            return retVal;
110        }
111    }
112}
```

在onStart方法中  
`intent.setPackage("com.android.car");`  
`intent.setAction(CAR_SERVICE_INTERFACE);`  
通过这个常量转向`car服务`相关的代码  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f4ce6d71985d4f35c5c10cd39aa7d3d5.png)