## 回顾

[[Android]]系统中，第一个启动的是init进程，通过解析init.rc文件启动对应的service。Zygote就是由init启动起来的。Zygote作为应用的孵化器，所有的应用程序都是由他创建而来的。Zygote是C/S架构的，当他被fork出来之后会创建Java虚拟机，注册JNI环境 注册完成之后调用`ZygoteInit.Main`进入Java层。在`ZygoteInit.Main`中会创建`ZygoteServerSocket`,`forkSystemServer`以及循环等待客户端的请求，当请求到来之后会调用`processOneCommand`fork子进程和设置子进程的信息(创建ProcessState 初始化binder startThreadPoll)，之后根据客户端请求的startClass 通过反射找到Main函数，并执行，这样Zygote整体的流程 和 处理请求就结束了。上次我们没有分析SystemServer，现在我们就来看看SystemServer。

## 介绍

## 1.SystemServer是什么？

Android系统在启动的时候有两个非常重要的进程，一个是Zygote，另一个就是system\_server。SystemServer是系统用来启动service的入口，比如我们常用的`AMS`，`WMS`，`PMS`等等都是由它创建的。

## 2.SystemServer都做了什么？

创建`SystemServiceManager`对系统服务进行创建、启动以及管理。

## 正文：

## 1.system\_server进程的启动

system\_server的启动离不开Zygote，启动Zygote之后会调用`ZygoteInit.java`的`main`函数，会执行`forkSystemServer`。

```
 private static Runnable forkSystemServer(String abiList, String socketName,
            ZygoteServer zygoteServer) {
       
       ……………………

        /* Hardcoded command line to start the system server */
        //准备 开启system_server的参数
        String args[] = {
                "--setuid=1000",
                "--setgid=1000",
                "--setgroups=1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1018,1021,1023,"
                        + "1024,1032,1065,3001,3002,3003,3006,3007,3009,3010",
                "--capabilities=" + capabilities + "," + capabilities,
                "--nice-name=system_server",
                "--runtime-args",
                "--target-sdk-version=" + VMRuntime.SDK_VERSION_CUR_DEVELOPMENT,
                "com.android.server.SystemServer",
        };
        ZygoteArguments parsedArgs = null;

        int pid;

        try {
            //把参数包装成ZygoteArguments
            parsedArgs = new ZygoteArguments(args);
            Zygote.applyDebuggerSystemProperty(parsedArgs);
            Zygote.applyInvokeWithSystemProperty(parsedArgs);

            boolean profileSystemServer = SystemProperties.getBoolean(
                    "dalvik.vm.profilesystemserver", false);
            if (profileSystemServer) {
                parsedArgs.mRuntimeFlags |= Zygote.PROFILE_SYSTEM_SERVER;
            }

            /* Request to fork the system server process */
//            fork出来system_server 还是调用到native层com_android_internal_os_Zygote_nativeForkSystemServer
            pid = Zygote.forkSystemServer(
                    parsedArgs.mUid, parsedArgs.mGid,
                    parsedArgs.mGids,
                    parsedArgs.mRuntimeFlags,
                    null,
                    parsedArgs.mPermittedCapabilities,
                    parsedArgs.mEffectiveCapabilities);
        } catch (IllegalArgumentException ex) {
            throw new RuntimeException(ex);
        }

        /* For child process */
        if (pid == 0) {
            if (hasSecondZygote(abiList)) {
                waitForSecondaryZygote(socketName);
            }
            //关闭掉ServerSocket
            zygoteServer.closeServerSocket();
//这里我们看到返回的是一个Runnable 就可以联想到Zygote中的MethodAndArgsCaller
            return handleSystemServerProcess(parsedArgs);
        }

        return null;
    }
//fork最终会调用到Native层
//com_android_internal_os_Zygote_nativeForkSystemServer
//。文件目录/frameworks/base/core/jni/com_android_internal_os_Zygote.cpp。

static jint com_android_internal_os_Zygote_nativeForkSystemServer(
        JNIEnv* env, jclass, uid_t uid, gid_t gid, jintArray gids,
        jint runtime_flags, jobjectArray rlimits, jlong permitted_capabilities,
        jlong effective_capabilities) {
  std::vector<int> fds_to_close(MakeUsapPipeReadFDVector()),
                   fds_to_ignore(fds_to_close);

  fds_to_close.push_back(gUsapPoolSocketFD);

  if (gUsapPoolEventFD != -1) {
    fds_to_close.push_back(gUsapPoolEventFD);
    fds_to_ignore.push_back(gUsapPoolEventFD);
  }
  //调用forkCommon函数，这个之前在Zygote中有跟踪过，这里就不跟了。调用fork并返回pid_t
  pid_t pid = ForkCommon(env, true,
                         fds_to_close,
                         fds_to_ignore);
  if (pid == 0) {
      SpecializeCommon(env, uid, gid, gids, runtime_flags, rlimits,
                       permitted_capabilities, effective_capabilities,
                       MOUNT_EXTERNAL_DEFAULT, nullptr, nullptr, true,
                       false, nullptr, nullptr);
  } else if (pid > 0) {
      ALOGI("System server process %d has been created", pid);
      gSystemServerPid = pid;
      int status;
      if (waitpid(pid, &status, WNOHANG) == pid) {//做一次安全检查
          ALOGE("System server process %d has died. Restarting Zygote!", pid);
          RuntimeAbort(env, __LINE__, "System server process has died. Restarting Zygote!");
      }

      if (UsePerAppMemcg()) {
          if (!SetTaskProfiles(pid, std::vector<std::string>{"SystemMemoryProcess"})) {
              ALOGE("couldn't add process %d into system memcg group", pid);
          }
      }
  }
  return pid;
}
```

fork出来子进程之后 执行handleSystemServerProcess并返回一个Runnable，联想到之前Zygore返回的MethodAndArgsCaller。看看这里返回的是什么呢

```
private static Runnable handleSystemServerProcess(ZygoteArguments parsedArgs) {
    Os.umask(S_IRWXG | S_IRWXO);

    if (parsedArgs.mNiceName != null) {
        Process.setArgV0(parsedArgs.mNiceName);
    }

    final String systemServerClasspath = Os.getenv("SYSTEMSERVERCLASSPATH");
    if (systemServerClasspath != null) {
        if (performSystemServerDexOpt(systemServerClasspath)) {
            sCachedSystemServerClassLoader = null;
        }
        boolean profileSystemServer = SystemProperties.getBoolean(
                "dalvik.vm.profilesystemserver", false);
        if (profileSystemServer && (Build.IS_USERDEBUG || Build.IS_ENG)) {
            try {
                prepareSystemServerProfile(systemServerClasspath);
            } catch (Exception e) {
                Log.wtf(TAG, "Failed to set up system server profile", e);
            }
        }
    }

    if (parsedArgs.mInvokeWith != null) {//这里是Null
        String[] args = parsedArgs.mRemainingArgs;
        if (systemServerClasspath != null) {
            String[] amendedArgs = new String[args.length + 2];
            amendedArgs[0] = "-cp";
            amendedArgs[1] = systemServerClasspath;
            System.arraycopy(args, 0, amendedArgs, 2, args.length);
            args = amendedArgs;
        }

        WrapperInit.execApplication(parsedArgs.mInvokeWith,
                parsedArgs.mNiceName, parsedArgs.mTargetSdkVersion,
                VMRuntime.getCurrentInstructionSet(), null, args);

        throw new IllegalStateException("Unexpected return from WrapperInit.execApplication");
    } else {
        createSystemServerClassLoader();//创建SystemServer的ClassLoader
        ClassLoader cl = sCachedSystemServerClassLoader;//将创建好的ClassLoader赋值给cl
        if (cl != null) {
            Thread.currentThread().setContextClassLoader(cl);
        }
        //调用ZygoteInit.zygoteInit 和Zygote一样 创建ProcessState 打开binder 并调用RuntimeInit.applicationInit 返回startClass的Main函数的caller也 也就是执行com.android.server.SystemServer的main函数
        return ZygoteInit.zygoteInit(parsedArgs.mTargetSdkVersion,
                parsedArgs.mRemainingArgs, cl);
    }

}


```

这里就很熟悉了，之前Zygote已经讲过的流程了，他是从参数中获取到class是`com.android.server.SystemServer`调用他的main函数，那么我们看看SystemServer.main吧。

## 2.system\_server都做了什么？

```
public static void main(String[] args) {
    //转头就钓了run函数
    new SystemServer().run();
}



private void run() {
    try {
        traceBeginAndSlog("InitBeforeStartServices");

        SystemProperties.set(SYSPROP_START_COUNT, String.valueOf(mStartCount));
        SystemProperties.set(SYSPROP_START_ELAPSED, String.valueOf(mRuntimeStartElapsedTime));
        SystemProperties.set(SYSPROP_START_UPTIME, String.valueOf(mRuntimeStartUptime));

        EventLog.writeEvent(EventLogTags.SYSTEM_SERVER_START,
                mStartCount, mRuntimeStartUptime, mRuntimeStartElapsedTime);

        if (System.currentTimeMillis() < EARLIEST_SUPPORTED_TIME) {
            Slog.w(TAG, "System clock is before 1970; setting to 1970.");
            SystemClock.setCurrentTimeMillis(EARLIEST_SUPPORTED_TIME);
        }

        String timezoneProperty = SystemProperties.get("persist.sys.timezone");
        if (timezoneProperty == null || timezoneProperty.isEmpty()) {
            Slog.w(TAG, "Timezone not set; setting to GMT.");
            SystemProperties.set("persist.sys.timezone", "GMT");
        }

        if (!SystemProperties.get("persist.sys.language").isEmpty()) {
            final String languageTag = Locale.getDefault().toLanguageTag();

            SystemProperties.set("persist.sys.locale", languageTag);
            SystemProperties.set("persist.sys.language", "");
            SystemProperties.set("persist.sys.country", "");
            SystemProperties.set("persist.sys.localevar", "");
        }

        Binder.setWarnOnBlocking(true);
        PackageItemInfo.forceSafeLabels();

        SQLiteGlobal.sDefaultSyncMode = SQLiteGlobal.SYNC_MODE_FULL;

        SQLiteCompatibilityWalFlags.init(null);

        Slog.i(TAG, "Entered the Android system server!");
        int uptimeMillis = (int) SystemClock.elapsedRealtime();
        EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_SYSTEM_RUN, uptimeMillis);
        if (!mRuntimeRestart) {
            MetricsLogger.histogram(null, "boot_system_server_init", uptimeMillis);
        }

        SystemProperties.set("persist.sys.dalvik.vm.lib.2", VMRuntime.getRuntime().vmLibrary());

        VMRuntime.getRuntime().clearGrowthLimit();
        VMRuntime.getRuntime().setTargetHeapUtilization(0.8f);
        Build.ensureFingerprintProperty();
        Environment.setUserRequired(true);
        BaseBundle.setShouldDefuse(true);
        Parcel.setStackTraceParceling(true);
        BinderInternal.disableBackgroundScheduling(true);
        BinderInternal.setMaxThreads(sMaxBinderThreads);
        android.os.Process.setThreadPriority(
                android.os.Process.THREAD_PRIORITY_FOREGROUND);
        android.os.Process.setCanSelfBackground(false);
        Looper.prepareMainLooper();//创建消息的Looper
        Looper.getMainLooper().setSlowLogThresholdMs(
                SLOW_DISPATCH_THRESHOLD_MS, SLOW_DELIVERY_THRESHOLD_MS);
        //加载函数库 libandroid_servers.so
        System.loadLibrary("android_servers");

        if (Build.IS_DEBUGGABLE) {
            initZygoteChildHeapProfiling();
        }

        performPendingShutdown();

        // Initialize the system context.
        createSystemContext();//创建系统Context

        // Create the system service manager. 创建ServiceManager
        mSystemServiceManager = new SystemServiceManager(mSystemContext);
        mSystemServiceManager.setStartInfo(mRuntimeRestart,
                mRuntimeStartElapsedTime, mRuntimeStartUptime);
        //将SystemServiceManager添加到LocalServices中 LocalService 内部其实就是一个ArrayMap
        LocalServices.addService(SystemServiceManager.class, mSystemServiceManager);
        // Prepare the thread pool for init tasks that can be parallelized
        SystemServerInitThreadPool.get();
    } finally {
        traceEnd();  // InitBeforeStartServices
    }

    // Start services. 开启服务
    try {
        traceBeginAndSlog("StartServices");
        startBootstrapServices();//开启引导服务
        startCoreServices();//开启核心服务
        startOtherServices();//开启其他服务
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
    if (!VMRuntime.hasBootImageSpaces()) {
        Slog.wtf(TAG, "Runtime is not running with a boot image!");
    }

    // Loop forever.
    //loop启动
    Looper.loop();
    throw new RuntimeException("Main thread loop unexpectedly exited");
}

```

😅main函数转头就掉了run函数，run函数中 创建了Looper、加载了`android_server`函数库、创建了`SystemServiceManager(对服务进行创建、启动和管理)`并且添加到`LocalService（一个ArrayMap）`中、`startBootstrapServices(开启引导服务)`、`startCoreServices(开启核心服务)`、`startOtherServices(开启其他服务)`、`Looper开启`。 先看看`createSystemContext`做了什么

```
private void createSystemContext() {
    //创建ActivityThread
    ActivityThread activityThread = ActivityThread.systemMain();
    mSystemContext = activityThread.getSystemContext();
    mSystemContext.setTheme(DEFAULT_SYSTEM_THEME);

    final Context systemUiContext = activityThread.getSystemUiContext();
    systemUiContext.setTheme(DEFAULT_SYSTEM_THEME);
}


public static ActivityThread systemMain() {
    if (!ActivityManager.isHighEndGfx()) {//是否开启硬件渲染
        ThreadedRenderer.disable(true);
    } else {
        ThreadedRenderer.enableForegroundTrimming();
    }
    //创建ActivityThread对象返回
    ActivityThread thread = new ActivityThread();
    thread.attach(true, 0);
    return thread;
}

//调用attach 反射创建Application 执行onCreate
private void attach(boolean system, long startSeq) {
    //传递的system = true,startSeq = 0;
    sCurrentActivityThread = this;
    mSystemThread = system;
    if (!system) {//这里逻辑不走
        android.ddm.DdmHandleAppName.setAppName("<pre-initialized>",
                                                UserHandle.myUserId());
        RuntimeInit.setApplicationObject(mAppThread.asBinder());
        final IActivityManager mgr = ActivityManager.getService();
        try {
            mgr.attachApplication(mAppThread, startSeq);
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
        // Watch for getting close to heap limit.
        BinderInternal.addGcWatcher(new Runnable() {
            @Override public void run() {
                if (!mSomeActivitiesChanged) {
                    return;
                }
                Runtime runtime = Runtime.getRuntime();
                long dalvikMax = runtime.maxMemory();
                long dalvikUsed = runtime.totalMemory() - runtime.freeMemory();
                if (dalvikUsed > ((3*dalvikMax)/4)) {
                    if (DEBUG_MEMORY_TRIM) Slog.d(TAG, "Dalvik max=" + (dalvikMax/1024)
                            + " total=" + (runtime.totalMemory()/1024)
                            + " used=" + (dalvikUsed/1024));
                    mSomeActivitiesChanged = false;
                    try {
                        ActivityTaskManager.getService().releaseSomeActivities(mAppThread);
                    } catch (RemoteException e) {
                        throw e.rethrowFromSystemServer();
                    }
                }
            }
        });
    } else {
        //设置应用程序名称:system_process
        android.ddm.DdmHandleAppName.setAppName("system_process",
                UserHandle.myUserId());
        try {
            //创建Instrumentation 用来管理应用的生命周期
            mInstrumentation = new Instrumentation();
            mInstrumentation.basicInit(this);
            //创建ContextImpl 上下文
            ContextImpl context = ContextImpl.createAppContext(
                    this, getSystemContext().mPackageInfo);
            //反射创建android.app.Application 并执行onCreate 这里创建的是framewok-res.apk 感兴趣的大家可以自己追踪
            mInitialApplication = context.mPackageInfo.makeApplication(true, null);
            mInitialApplication.onCreate();
        } catch (Exception e) {
            throw new RuntimeException(
                    "Unable to instantiate Application():" + e.toString(), e);
        }
    }
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

通过调用`ActivityThread.systemMain()`创建`ActivityThread`并且加载了`framewok-res.apk`执行了app的onCreate。这里的app是系统使用的，比如系统的对话框就是从system\_server中弹出来的。 接下来就到了启动服务了。

### startBootstrapServices

先看代码:

```
private void startBootstrapServices() {
    traceBeginAndSlog("StartWatchdog");
    final Watchdog watchdog = Watchdog.getInstance();
    watchdog.start();//开启看门狗
    traceEnd();

    Slog.i(TAG, "Reading configuration...");
    final String TAG_SYSTEM_CONFIG = "ReadingSystemConfig";
    traceBeginAndSlog(TAG_SYSTEM_CONFIG);
    SystemServerInitThreadPool.get().submit(SystemConfig::getInstance, TAG_SYSTEM_CONFIG);
    traceEnd();

    traceBeginAndSlog("StartInstaller");
    //开启Installer服务
    Installer installer = mSystemServiceManager.startService(Installer.class);
    traceEnd();

    traceBeginAndSlog("DeviceIdentifiersPolicyService");
    //开启设备标识符访问策略服务
    mSystemServiceManager.startService(DeviceIdentifiersPolicyService.class);
    traceEnd();

    traceBeginAndSlog("UriGrantsManagerService");
    mSystemServiceManager.startService(UriGrantsManagerService.Lifecycle.class);
    traceEnd();

    // Activity manager runs the show.
    traceBeginAndSlog("StartActivityManager");
    //启动ActivityTaskManagerService、mActivityManagerService服务并关联systemServiceManager以及installer
    ActivityTaskManagerService atm = mSystemServiceManager.startService(
            ActivityTaskManagerService.Lifecycle.class).getService();
    mActivityManagerService = ActivityManagerService.Lifecycle.startService(
            mSystemServiceManager, atm);
    mActivityManagerService.setSystemServiceManager(mSystemServiceManager);
    mActivityManagerService.setInstaller(installer);
    mWindowManagerGlobalLock = atm.getGlobalLock();
    traceEnd();

    // Power manager needs to be started early because other services need it.
    // Native daemons may be watching for it to be registered so it must be ready
    // to handle incoming binder calls immediately (including being able to verify
    // the permissions for those calls).
    traceBeginAndSlog("StartPowerManager");
    //开启PowerManagerService服务
    mPowerManagerService = mSystemServiceManager.startService(PowerManagerService.class);
    traceEnd();

    traceBeginAndSlog("StartThermalManager");
    mSystemServiceManager.startService(ThermalManagerService.class);
    traceEnd();

    // Now that the power manager has been started, let the activity manager
    // initialize power management features.
    traceBeginAndSlog("InitPowerManagement");
    mActivityManagerService.initPowerManagement();
    traceEnd();

    // Bring up recovery system in case a rescue party needs a reboot
    traceBeginAndSlog("StartRecoverySystemService");
    mSystemServiceManager.startService(RecoverySystemService.class);
    traceEnd();

    // Now that we have the bare essentials of the OS up and running, take
    // note that we just booted, which might send out a rescue party if
    // we're stuck in a runtime restart loop.
    RescueParty.noteBoot(mSystemContext);

    // Manages LEDs and display backlight so we need it to bring up the display.
    traceBeginAndSlog("StartLightsService");
    mSystemServiceManager.startService(LightsService.class);
    traceEnd();

    traceBeginAndSlog("StartSidekickService");
    // Package manager isn't started yet; need to use SysProp not hardware feature
    if (SystemProperties.getBoolean("config.enable_sidekick_graphics", false)) {
        mSystemServiceManager.startService(WEAR_SIDEKICK_SERVICE_CLASS);
    }
    traceEnd();

    // Display manager is needed to provide display metrics before package manager
    // starts up.
    traceBeginAndSlog("StartDisplayManager");
    mDisplayManagerService = mSystemServiceManager.startService(DisplayManagerService.class);
    traceEnd();

    // We need the default display before we can initialize the package manager.
    traceBeginAndSlog("WaitForDisplay");
    mSystemServiceManager.startBootPhase(SystemService.PHASE_WAIT_FOR_DEFAULT_DISPLAY);
    traceEnd();

    // Only run "core" apps if we're encrypting the device.
    String cryptState = VoldProperties.decrypt().orElse("");
    if (ENCRYPTING_STATE.equals(cryptState)) {
        Slog.w(TAG, "Detected encryption in progress - only parsing core apps");
        mOnlyCore = true;
    } else if (ENCRYPTED_STATE.equals(cryptState)) {
        Slog.w(TAG, "Device encrypted - only parsing core apps");
        mOnlyCore = true;
    }

    // Start the package manager.
    if (!mRuntimeRestart) {
        MetricsLogger.histogram(null, "boot_package_manager_init_start",
                (int) SystemClock.elapsedRealtime());
    }
    traceBeginAndSlog("StartPackageManagerService");
    try {
        Watchdog.getInstance().pauseWatchingCurrentThread("packagemanagermain");
        //创建PackageManagerService服务
        mPackageManagerService = PackageManagerService.main(mSystemContext, installer,
                mFactoryTestMode != FactoryTest.FACTORY_TEST_OFF, mOnlyCore);
    } finally {
        Watchdog.getInstance().resumeWatchingCurrentThread("packagemanagermain");
    }
    mFirstBoot = mPackageManagerService.isFirstBoot();
    mPackageManager = mSystemContext.getPackageManager();
    traceEnd();
    if (!mRuntimeRestart && !isFirstBootOrUpgrade()) {
        MetricsLogger.histogram(null, "boot_package_manager_init_ready",
                (int) SystemClock.elapsedRealtime());
    }
    // Manages A/B OTA dexopting. This is a bootstrap service as we need it to rename
    // A/B artifacts after boot, before anything else might touch/need them.
    // Note: this isn't needed during decryption (we don't have /data anyways).
    if (!mOnlyCore) {
        boolean disableOtaDexopt = SystemProperties.getBoolean("config.disable_otadexopt",
                false);
        if (!disableOtaDexopt) {
            traceBeginAndSlog("StartOtaDexOptService");
            try {
                Watchdog.getInstance().pauseWatchingCurrentThread("moveab");
                OtaDexoptService.main(mSystemContext, mPackageManagerService);
            } catch (Throwable e) {
                reportWtf("starting OtaDexOptService", e);
            } finally {
                Watchdog.getInstance().resumeWatchingCurrentThread("moveab");
                traceEnd();
            }
        }
    }

    traceBeginAndSlog("StartUserManagerService");
    mSystemServiceManager.startService(UserManagerService.LifeCycle.class);
    traceEnd();

    // Initialize attribute cache used to cache resources from packages.
    traceBeginAndSlog("InitAttributerCache");
    AttributeCache.init(mSystemContext);
    traceEnd();

    // Set up the Application instance for the system process and get started.
    traceBeginAndSlog("SetSystemProcess");
    mActivityManagerService.setSystemProcess();
    traceEnd();

    // Complete the watchdog setup with an ActivityManager instance and listen for reboots
    // Do this only after the ActivityManagerService is properly started as a system process
    traceBeginAndSlog("InitWatchdog");
    watchdog.init(mSystemContext, mActivityManagerService);
    traceEnd();

    // DisplayManagerService needs to setup android.display scheduling related policies
    // since setSystemProcess() would have overridden policies due to setProcessGroup
    mDisplayManagerService.setupSchedulerPolicies();

    // Manages Overlay packages
    traceBeginAndSlog("StartOverlayManagerService");
    mSystemServiceManager.startService(new OverlayManagerService(mSystemContext, installer));
    traceEnd();

    traceBeginAndSlog("StartSensorPrivacyService");
    mSystemServiceManager.startService(new SensorPrivacyService(mSystemContext));
    traceEnd();

    if (SystemProperties.getInt("persist.sys.displayinset.top", 0) > 0) {
        // DisplayManager needs the overlay immediately.
        mActivityManagerService.updateSystemUiContext();
        LocalServices.getService(DisplayManagerInternal.class).onOverlayChanged();
    }

    // The sensor service needs access to package manager service, app ops
    // service, and permissions service, therefore we start it after them.
    // Start sensor service in a separate thread. Completion should be checked
    // before using it.
    mSensorServiceStart = SystemServerInitThreadPool.get().submit(() -> {
        TimingsTraceLog traceLog = new TimingsTraceLog(
                SYSTEM_SERVER_TIMING_ASYNC_TAG, Trace.TRACE_TAG_SYSTEM_SERVER);
        traceLog.traceBegin(START_SENSOR_SERVICE);
        startSensorService();
        traceLog.traceEnd();
    }, START_SENSOR_SERVICE);
}
```

开启了`看门狗(用于监控system_server，一旦出现问题会杀死SystemServer，SystemServer撕掉会告诉Zygote Zygote接收到之后会杀死自己 然后告诉init init又会重启Zygote并且激活对应的onRestart)`以及相当多的服务，其中有我们熟悉的`Installer`、`ActivityTaskManagerService、ActivityManagerService`、`PowerManagerService`、`PackageManagerService`等等，基本上就是通过`startService`方法，我们看看startService.

```
public <T extends SystemService> T startService(Class<T> serviceClass) {
    try {
        final String name = serviceClass.getName();
        Slog.i(TAG, "Starting " + name);
        Trace.traceBegin(Trace.TRACE_TAG_SYSTEM_SERVER, "StartService " + name);

        if (!SystemService.class.isAssignableFrom(serviceClass)) {
            throw new RuntimeException("Failed to create " + name
                    + ": service must extend " + SystemService.class.getName());
        }
        final T service;
        try {
            Constructor<T> constructor = serviceClass.getConstructor(Context.class);
            service = constructor.newInstance(mContext);
        } catch (InstantiationException ex) {
            throw new RuntimeException("Failed to create service " + name
                    + ": service could not be instantiated", ex);
        } catch (IllegalAccessException ex) {
            throw new RuntimeException("Failed to create service " + name
                    + ": service must have a public constructor with a Context argument", ex);
        } catch (NoSuchMethodException ex) {
            throw new RuntimeException("Failed to create service " + name
                    + ": service must have a public constructor with a Context argument", ex);
        } catch (InvocationTargetException ex) {
            throw new RuntimeException("Failed to create service " + name
                    + ": service constructor threw an exception", ex);
        }
        //通过反射创建传递过来的Service对象，调用startService
        startService(service);
        return service;
    } finally {
        Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
    }
}

public void startService(@NonNull final SystemService service) {
        //将service添加到mServices中去
        mServices.add(service);
        long time = SystemClock.elapsedRealtime();
        try {
            //调用service的onStart函数启动对应的service
            service.onStart();
        } catch (RuntimeException ex) {
            throw new RuntimeException("Failed to start service " + service.getClass().getName()
                    + ": onStart threw an exception", ex);
        }
        warnIfTooLong(SystemClock.elapsedRealtime() - time, service, "onStart");
    }
```

通过`反射`的方式，将service创建并添加到`mServices`中去。

### startCoreServices

```
private void startCoreServices() {
    traceBeginAndSlog("StartBatteryService");
    //开启电池管理的service 开启方式也是startService
    mSystemServiceManager.startService(BatteryService.class);
    traceEnd();

    traceBeginAndSlog("StartUsageService");
    //开启收集应用使用情况的Service
    mSystemServiceManager.startService(UsageStatsService.class);
    mActivityManagerService.setUsageStatsManager(
            LocalServices.getService(UsageStatsManagerInternal.class));
    traceEnd();

    if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_WEBVIEW)) {
        traceBeginAndSlog("StartWebViewUpdateService");
        mWebViewUpdateService = mSystemServiceManager.startService(WebViewUpdateService.class);
        traceEnd();
    }

    traceBeginAndSlog("StartCachedDeviceStateService");
    mSystemServiceManager.startService(CachedDeviceStateService.class);
    traceEnd();

    traceBeginAndSlog("StartBinderCallsStatsService");
    mSystemServiceManager.startService(BinderCallsStatsService.LifeCycle.class);
    traceEnd();

    traceBeginAndSlog("StartLooperStatsService");
    mSystemServiceManager.startService(LooperStatsService.Lifecycle.class);
    traceEnd();

    traceBeginAndSlog("StartRollbackManagerService");
    mSystemServiceManager.startService(RollbackManagerService.class);
    traceEnd();

    traceBeginAndSlog("StartBugreportManagerService");
    mSystemServiceManager.startService(BugreportManagerService.class);
    traceEnd();

    traceBeginAndSlog("GpuService");
    mSystemServiceManager.startService(GpuService.class);
    traceEnd();
}
```

开启了一些核心的Service,`BatteryService`、`UsageStatsService`等等。开启方式也是startService所以也是添加到`mServices`中。

### startOtherServices

```
private void startOtherServices() {
      //代码实在太多了，我选一些
        //………………
        traceBeginAndSlog("StartWindowManagerService");
        // WMS needs sensor service ready
        ConcurrentUtils.waitForFutureNoInterrupt(mSensorServiceStart, START_SENSOR_SERVICE);
        mSensorServiceStart = null;
        //创建WindowManagerService
        wm = WindowManagerService.main(context, inputManager, !mFirstBoot, mOnlyCore,
                new PhoneWindowManager(), mActivityManagerService.mActivityTaskManager);
        ServiceManager.addService(Context.WINDOW_SERVICE, wm, /* allowIsolated= */ false,
                DUMP_FLAG_PRIORITY_CRITICAL | DUMP_FLAG_PROTO);
                //将inputManager添加到ServiceManager中 注意不是SystemServiceManager
        ServiceManager.addService(Context.INPUT_SERVICE, inputManager,
                /* allowIsolated= */ false, DUMP_FLAG_PRIORITY_CRITICAL);
        traceEnd();

        traceBeginAndSlog("SetWindowManagerService");
        mActivityManagerService.setWindowManager(wm);
        traceEnd();

        //………………
        //添加系统更新的Service
            ServiceManager.addService(Context.SYSTEM_UPDATE_SERVICE,
                    new SystemUpdateManagerService(context));
        } catch (Throwable e) {
            reportWtf("starting SystemUpdateManagerService", e);
        }
        traceEnd();
     //………………
    try {
    //调用wm的systemReady函数
        wm.systemReady();
    } catch (Throwable e) {
        reportWtf("making Window Manager Service ready", e);
    }
    //………………

    traceBeginAndSlog("MakePackageManagerServiceReady");
    mPackageManagerService.systemReady();
    traceEnd();
    //调用ActivityManagerService的systemReady
    mActivityManagerService.systemReady(() -> {
        Slog.i(TAG, "Making services ready");
        traceBeginAndSlog("StartActivityManagerReadyPhase");
        //标记状态为500 表示可以发送广播
        mSystemServiceManager.startBootPhase(
                SystemService.PHASE_ACTIVITY_MANAGER_READY);
        traceEnd();
        traceBeginAndSlog("StartObservingNativeCrashes");
        try {
            mActivityManagerService.startObservingNativeCrashes();
        } catch (Throwable e) {
            reportWtf("observing native crashes", e);
        }
        traceEnd();
         //标记状态为600 表示服务已经可以被其他应用绑定，调用。
        mSystemServiceManager.startBootPhase(
                SystemService.PHASE_THIRD_PARTY_APPS_CAN_START);
        traceEnd();
        //………………
    }, BOOT_TIMINGS_TRACE_LOG);
}
```

代码两实在太大了 1000多行，18年加了注释 表示会重构，我看了下最新的android13的代码，依旧是1000多行。啥时候重构啊?😏 `你嘛时候成为津门第一啊?` ，扯远了，回到我们正文在startOtherService中又开启了非常多的Service:`WindowManagerService`、`SystemUpdateManagerService ……`

## 总结

## 文字总结

1.system\_server是在Zygote启动的时候`fork`出来的子进程，调用的是`ZygoteInit.forkSystemServer`,最终返回MethodAndArgsCaller包装的是`com.android.server.SystemServer.Main`。

2.SystemServer.Main函数直接调用了run函数，在run函数中调用了`createSystemContext`通过`ActivityThread.systemMain`创建了`ActivityThread`，设置了`mSystemContext`并且调用`ActivityThread.attach` 通过反射创建`Application` 执行`onCreate`函数.这里创建的app是framewok-res.apk，是给系统使用的 比如系统的对话框。将`mSystemServiceManager`添加到`LocalService`的`mService(ArrayMap)`中去接着调用`startBootstrapServices`、`startCoreServices`、`startOtherServices`开启了非常多的服务`Installer`、`ActivityTaskManagerService、ActivityManagerService`、`PowerManagerService`、`PackageManagerService`等等。把他们添加到SystemServiceManager的`mServices(ArrayList)`中去。

## 一张图来概括

![system_server.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/db84c9d620d942d4bde45a34f777cebc~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 面试题

Q1:ArrayMap和HashMap的区别。

A1:ArrayMap的Key都是int来进行存放的，可以使用`二分` 加速元素的搜索.而HashMap的key是复合类型需要计算Hash。所以ArrayMap的效率比HashMap更快。

Q2:AMS、PMS是谁启动的？他们都是独立的进程吗？

A2:他们是由`system_server`启动，`不是独立进程`。他们都运行在system\_server进程中。

Q3:简述system\_server的启动

A3:由`Zygote`调用`forkSystemServer`来创建`system_server`进程，调用到`Java`层`SystemServer.java.Main`，创建SystemServer的`ActivityThread` 创建`framewok-res.apk`的`Application`调用`onCreate`，调用`startBootstrapServices`、`startCoreServices`、`startOtherServices`创建各种各样的`Service（AMS、PMS等）`,并开启`开门狗`检测服务和线程，如果出现问题就会杀死system\_server，system\_server死亡之后会通知Zygote,Zygote会杀死自己 通知init,init会重启Zygote 并执行对应的onRestart服务。

Q4:简述Android的WatchDog机制

A4:首先`WatchDog`机制是系统用来监控`service`的。在`system_server`启动之后会启动看门狗，将`AMS、WMS`等服务都添加进来进行监控。当某个服务出现问题 就会`杀死掉`当前进程也就是`system_server`，进而通知`Zygote`，`Zygote`杀死，通知`init` 重启`Zygote`，重启`system_server`。 它的原理就是用了Handler机制，去看MQ里面isPolling，如果正常就返回，如果不正常就调用`monitor`看看监控都线程是否出现问题，如果出现问题就会杀死自己。具体原理如下图。

![WatchDog.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b1e7926a9e2e4a8eb7a138e7f95627d3~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

各种Service我们这一些没有分析，留在我们讲对应的Service再回来分析吧。下面就要分析重头戏Binder了，我们Binder再见了~

在线视频：[www.bilibili.com/video/BV1cL…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1cL411S7N2%2F%3Fshare_source%3Dcopy_web%26vd_source%3D0cf6c1c4827d129d9daea3a51b663710 "https://www.bilibili.com/video/BV1cL411S7N2/?share_source=copy_web&vd_source=0cf6c1c4827d129d9daea3a51b663710")

网盘地址: [pan.baidu.com/s/1xq\_BCIQQ…](https://link.juejin.cn/?target=https%3A%2F%2Fpan.baidu.com%2Fs%2F1xq_BCIQQdnT8GUw7e7ZQ1Q "https://pan.baidu.com/s/1xq_BCIQQdnT8GUw7e7ZQ1Q")

提取码: pub4

代码地址:[github.com/iehshx/Watc…](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2Fiehshx%2FWatchDog.git "https://github.com/iehshx/WatchDog.git")