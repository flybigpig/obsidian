哈喽大家我是Zzz.今天分享的blog是system\_server进程的启动，环境基于aosp13\_r6，话不多说直接上干货

前言：system\_server 是系统核心服务的一个进程，这个进程包含了系统中核心Service的运行，给系统提供如：Window的管理、Activity的管理，

系统的核心功能都在System\_server中；

**在ZygoteInit中systemserver已经被fork出来了(具体可以看上一篇文章)**

\--javascripttypescriptbashsqljsonhtmlcssccppjavarubypythongorustmarkdown

```
if (startSystemServer) {
    Runnable r = forkSystemServer(abiList, zygoteSocketName, zygoteServer);
 
    // {@code r == null} in the parent (zygote) process, and {@code r != null}
    // child (system_server) process.
    if (r != null) {
        r.run();
        return;
    }
}
```

**进入到forkSystemServer方法后分析到了 这里开启了一个子进程**

\--javascripttypescriptbashsqljsonhtmlcssccppjavarubypythongorustmarkdown

```
 
 
 private static Runnable forkSystemServer(String abiList, String socketName,
            ZygoteServer zygoteServer) {
      ........
        /* Hardcoded command line to start the system server */
        String[] args = {
                "--setuid=1000",
                "--setgid=1000",
                "--setgroups=1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1018,1021,1023,"
                        + "1024,1032,1065,3001,3002,3003,3005,3006,3007,3009,3010,3011,3012",
                "--capabilities=" + capabilities + "," + capabilities,
                "--nice-name=system_server",
                "--runtime-args",
                "--target-sdk-version=" + VMRuntime.SDK_VERSION_CUR_DEVELOPMENT,
                "com.android.server.SystemServer",
        };
        ZygoteArguments parsedArgs;
 
        int pid;
 
        try {
          ........
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
 
            zygoteServer.closeServerSocket();
            return handleSystemServerProcess(parsedArgs);
        }
 
        return null;
    }
```

**即在Zygote中又调用了forkSystemServer方法一直到native层才调用了真正的linux的 fork（）**  
**在Zygote.forkSystemServer返回后看代码如下：**

\--javascripttypescriptbashsqljsonhtmlcssccppjavarubypythongorustmarkdown

```
/* For child process */
if (pid == 0) {
    if (hasSecondZygote(abiList)) {
        waitForSecondaryZygote(socketName);
    }
 
    zygoteServer.closeServerSocket();//关闭子进程的socket
    return handleSystemServerProcess(parsedArgs);
}
```

**由于SystemServer是复制Zygote的进程，因此也会包含Zygote的zygoteServer，对于SystemServer没有其他作用，需要先将其关闭；通过调用handleSystemServerProcess:处理一下systemServer的任务**

\--javascripttypescriptbashsqljsonhtmlcssccppjavarubypythongorustmarkdown

```
private static Runnable handleSystemServerProcess(ZygoteArguments parsedArgs) {
    // set umask to 0077 so new files and directories will default to owner-only permissions.
    Os.umask(S_IRWXG | S_IRWXO);
 
    if (parsedArgs.mNiceName != null) {
        Process.setArgV0(parsedArgs.mNiceName);
    }
 
    final String systemServerClasspath = Os.getenv("SYSTEMSERVERCLASSPATH");//获取一下systemServer的系统环境的路径 adb shell env SYSTEMSERVERCLASSPATH
/*SYSTEMSERVERCLASSPATH=/system/framework/com.android.location.provider.jar
:/system/framework/services.jar
:/apex/com.android.adservices/javalib/service-adservices.jar
:/apex/com.android.adservices/javalib/service- sdksandbox.jar
:/apex/com.android.appsearch/javalib/service-appsearch.jar
:/apex/com.android.art/javalib/service-art.jar
:/apex/com.android.media/javalib/service-media-s.jar
:/apex/com.android.permission/javalib/service-permission.jar*/	
 
    if (systemServerClasspath != null) { //刚刚进行了搜索这里不会空
        // Capturing profiles is only supported for debug or eng builds since selinux normally
        // prevents it.
        if (shouldProfileSystemServer() && (Build.IS_USERDEBUG || Build.IS_ENG)) {//这里为false adb shell getprop SYSTEMSERVERCLASSPATH
            try {
                Log.d(TAG, "Preparing system server profile");
                prepareSystemServerProfile(systemServerClasspath);//这个方法根据 ： 获取每一个jar包的路径然后进行加载
            } catch (Exception e) {
                Log.wtf(TAG, "Failed to set up system server profile", e);
            }
        }
    }
 
    if (parsedArgs.mInvokeWith != null) {//这里跟了一下代码就是看看有没有设置参数--invoke-with 发现没有就执行 else
        String[] args = parsedArgs.mRemainingArgs;
        // If we have a non-null system server class path, we'll have to duplicate the
        // existing arguments and append the classpath to it. ART will handle the classpath
        // correctly when we exec a new process.
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
 
        ClassLoader cl = getOrCreateSystemServerClassLoader();//创建SYSTEMSERVERPATHCLASS的classloader 但这里为空就不走这个方法了
        if (cl != null) {
            Thread.currentThread().setContextClassLoader(cl);
        }
 
        /*
         * Pass the remaining arguments to SystemServer.
         */
        return ZygoteInit.zygoteInit(parsedArgs.mTargetSdkVersion,
                parsedArgs.mDisabledCompatChanges,
                parsedArgs.mRemainingArgs, cl);
    }
 
    /* should never reach here */
}
```

**接下来走到了ZygoteInit.zygoteInit。**

\--javascripttypescriptbashsqljsonhtmlcssccppjavarubypythongorustmarkdown

```
public static Runnable zygoteInit(int targetSdkVersion, long[] disabledCompatChanges,
        String[] argv, ClassLoader classLoader) {
    if (RuntimeInit.DEBUG) {
        Slog.d(RuntimeInit.TAG, "RuntimeInit: Starting application from zygote");
    }
 
    Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "ZygoteInit");
    RuntimeInit.redirectLogStreams();
   //配置一些公共的初始化比如log之类的
    RuntimeInit.commonInit();
  //这里调用了一个native 方法 其实就是启动了一个binder，AndroidRuntime.cpp#com_android_internal_os_ZygoteInit_nativeZygoteInit--》app_main#onZygoteInit 
    ZygoteInit.nativeZygoteInit();
    return RuntimeInit.applicationInit(targetSdkVersion, disabledCompatChanges, argv,
            classLoader);
}
```

**然后查看applicationInit**

\--javascripttypescriptbashsqljsonhtmlcssccppjavarubypythongorustmarkdown

```
protected static Runnable applicationInit(int targetSdkVersion, long[] disabledCompatChanges,
        String[] argv, ClassLoader classLoader) {
    // If the application calls System.exit(), terminate the process
    // immediately without running any shutdown hooks.  It is not possible to
    // shutdown an Android application gracefully.  Among other things, the
    // Android runtime shutdown hooks close the Binder driver, which can cause
    // leftover running threads to crash before the process actually exits.
    nativeSetExitWithoutCleanup(true);
 
    VMRuntime.getRuntime().setTargetSdkVersion(targetSdkVersion);
    VMRuntime.getRuntime().setDisabledCompatChanges(disabledCompatChanges);
 
    final Arguments args = new Arguments(argv);
 
    // The end of of the RuntimeInit event (see #zygoteInit).
    Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
 
    // Remaining arguments are passed to the start class's static main
    //主要是这个方法 找到类的main方法 进行调用使用的反射调用
    return findStaticMain(args.startClass, args.startArgs, classLoader);
}
```

**目前为止zygote孵化进程后的流程已经走完了，等于是返回了某个类的main方法，那么传递的是哪个类的main呢？**

这里可以看到是args.startClass

\--javascripttypescriptbashsqljsonhtmlcssccppjavarubypythongorustmarkdown

```
findStaticMain(args.startClass, args.startArgs, classLoader);
```

**在Runnable forkSystemServer(String abiList, String socketName,ZygoteServer zygoteServer) 就设置了 com.android.server.SystemServer**

**接下来根据代码我们进入com.android.server.SystemServer的main方法，我们看到这里是启动一个线程**

\--javascripttypescriptbashsqljsonhtmlcssccppjavarubypythongorustmarkdown

```
public static void main(String[] args) {
    new SystemServer().run();
}
```

\--javascripttypescriptbashsqljsonhtmlcssccppjavarubypythongorustmarkdown

```
private void run() {
    TimingsTraceAndSlog t = new TimingsTraceAndSlog();
    try {
       .......
		//加载一个android_servers.so
 		System.loadLibrary("android_servers");
   	   .......
		//创建一个系统的上下文
  		createSystemContext();
		......
 
        // Create the system service manager. 创建系统服务的管理者
        mSystemServiceManager = new SystemServiceManager(mSystemContext);
        mSystemServiceManager.setStartInfo(mRuntimeRestart,
                mRuntimeStartElapsedTime, mRuntimeStartUptime);
        mDumper.addDumpable(mSystemServiceManager);
 
        LocalServices.addService(SystemServiceManager.class, mSystemServiceManager);
        // Prepare the thread pool for init tasks that can be parallelized
        SystemServerInitThreadPool tp = SystemServerInitThreadPool.start();
        mDumper.addDumpable(tp);
		.....
 
    // Start services.
	//从这里就开始启动各种服务了
    try {
        t.traceBegin("StartServices");
		//启动引导服务比如下面的核心服务其他服务有依赖关系的服务
        startBootstrapServices(t);
		//启动核心服务
        startCoreServices(t);
		//启动其他服务
        startOtherServices(t);
		//定义在apexes中的服务apexes，有兴趣的可以看下源码
		/*Apex服务是指Android操作系统中的一种应用程序启动方式，它允许应用程序在设备启动时以系统服务的形式自动运行。这些服务通常包括系统应用、框架服务和系统UI等。它们在设备启动时会自动运行，并为用户提供各种基础功能和界面。
startApexServices方法会遍历所有已安装的Apex服务，并调用它们的启动方法，使它们在系统启动时自动运行。该方法在系统启动过程中被调用，是Android操作系统启动过程中的一部分。*/
        startApexServices(t);
    } catch (Throwable ex) {
        Slog.e("System", "******************************************");
        Slog.e("System", "************ Failure starting system services", ex);
        throw ex;
    } finally {
        t.traceEnd(); // StartServices
    }
 
    StrictMode.initVmDefaults(null);
 
    if (!mRuntimeRestart && !isFirstBootOrUpgrade()) {
        final long uptimeMillis = SystemClock.elapsedRealtime();
        FrameworkStatsLog.write(FrameworkStatsLog.BOOT_TIME_EVENT_ELAPSED_TIME_REPORTED,
                FrameworkStatsLog.BOOT_TIME_EVENT_ELAPSED_TIME__EVENT__SYSTEM_SERVER_READY,
                uptimeMillis);
        final long maxUptimeMillis = 60 * 1000;
        if (uptimeMillis > maxUptimeMillis) {
            Slog.wtf(SYSTEM_SERVER_TIMING_TAG,
                    "SystemServer init took too long. uptimeMillis=" + uptimeMillis);
        }
    }
 
    // Loop forever.
    Looper.loop();
    throw new RuntimeException("Main thread loop unexpectedly exited");
}
```

最后在启动完各个service后，他还会调用Service的systemReady方法

**补充：SystemServiceManager 和 ServiceManager 的区别，SystemServiceManager等于是系统中各项服务的管理类并不会直接给应用直接应用，而ServiceManager通过.getService()可以获取到对应的服务**

**大致流程为：**

**SystemServerManager----》startService-----》new IBinder---->ServiceManager.add()---->ServiceManager通过IBinder调用ServiceManager（类似DNS服务器）《---------------**

**ServiceManager#getService获取Ibinder对象调用服务《---------------第三方应用**


//定义在apexes中的服务apexes，有兴趣的可以看下源码
/Apex服务是指Android操作系统中的一种应用程序启动方式，它允许应用程序在设备启动时以系统服务的形式自动运行。这些服务通常包括系统应用、框架服务和系统UI等。它们在设备启动时会自动运行，并为用户提供各种基础功能和界面。
startApexServices方法会遍历所有已安装的Apex服务，并调用它们的启动方法，使它们在系统启动时自动运行。该方法在系统启动过程中被调用，是Android操作系统启动过程中的一部分。
```
 /**
     * Starts system services defined in apexes.
     *
     * <p>Apex services must be the last category of services to start. No other service must be
     * starting after this point. This is to prevent unnecessary stability issues when these apexes
     * are updated outside of OTA; and to avoid breaking dependencies from system into apexes.
     */
    private void startApexServices(@NonNull TimingsTraceAndSlog t) {
        if (Flags.recoverabilityDetection()) {
            // For debugging RescueParty
            if (Build.IS_DEBUGGABLE
                    && SystemProperties.getBoolean("debug.crash_system", false)) {
                throw new RuntimeException();
            }
        }

        t.traceBegin("startApexServices");
        // TODO(b/192880996): get the list from "android" package, once the manifest entries
        // are migrated to system manifest.
        List<ApexSystemServiceInfo> services = ApexManager.getInstance().getApexSystemServices();
        for (ApexSystemServiceInfo info : services) {
            String name = info.getName();
            String jarPath = info.getJarPath();
            t.traceBegin("starting " + name);
            if (TextUtils.isEmpty(jarPath)) {
                mSystemServiceManager.startService(name);
            } else {
                mSystemServiceManager.startServiceFromJar(name, jarPath);
            }
            t.traceEnd();
        }

        // make sure no other services are started after this point
        mSystemServiceManager.sealStartedServices();

        t.traceEnd(); // startApexServices
    }
```