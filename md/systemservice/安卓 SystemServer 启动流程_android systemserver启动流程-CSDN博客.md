**目录**

[引言](https://blog.csdn.net/qq_42137577/article/details/144721434#%E5%BC%95%E8%A8%80)

[Android系统服务启动顺序](https://blog.csdn.net/qq_42137577/article/details/144721434#t0) 

[zygote fork SystemServer 进程](https://blog.csdn.net/qq_42137577/article/details/144721434#t1)

[SystemServer启动流程](https://blog.csdn.net/qq_42137577/article/details/144721434#t2)

[1、SystemServer.main()](https://blog.csdn.net/qq_42137577/article/details/144721434#t3)

[2、SystemServer.run()](https://blog.csdn.net/qq_42137577/article/details/144721434#t4)

[3、初始化系统上下文](https://blog.csdn.net/qq_42137577/article/details/144721434#t5)

[4、创建系统服务管理](https://blog.csdn.net/qq_42137577/article/details/144721434#t6)

[5、启动系统各种服务](https://blog.csdn.net/qq_42137577/article/details/144721434#t7)

[总结](https://blog.csdn.net/qq_42137577/article/details/144721434#t8)

___

## **引言**

开机启动时 PowerManagerService 调用 AudioService 音频接口可能会导致 JE 崩溃，进而 system\_server 崩溃。

分析：应该和系统服务SystemServer启动的顺序有关

## **Android系统服务启动顺序** 

[Android](https://so.csdn.net/so/search?q=Android&spm=1001.2101.3001.7020)整体启动流程概括为：启动BootLoader->加载系统内核->启动Init进程->启动Zygote进程->启动Runtime进程->启动本地服务->启动Home Launcher

![](https://i-blog.csdnimg.cn/direct/6b4b097341cf4a06aa65009eb9afc26e.png)

SystemServer 服务进程是 Android 系统 Java 层框架的核心，它维护着 Android 系统的 核心服务，比如：ActivityManagerService、WindowManagerService、PackageManagerService 等，是 Android 系统中一个非常重要的进程。在 Android 系统中，应用程序出现问题，对系统影响不大，而 Init、Zygote、SystemServer 三大进程对系统的影响则非常大，因为其中任何一个 crash，都会造成系统崩溃，出现重启现象。

## zygote fork SystemServer 进程

SystemServer 是由 Zygote 孵化而来的一个进程，通过 ps 命令，我们发现其进程名为：system\_server。在分析 zygote 进程时，我们知道当 zygote 进程进入到 java 世界后，在 ZygoteInit.java 中，将调用 forkSystemServer 方法启动 SystemServer 进程。

```java
// frameworks/base/core/java/com/android/internal/os/ZygoteInit.java
 
public class ZygoteInit {
 
    public static void main(String argv[]) {
 
        try {
        
            if (startSystemServer) {
                // fork 出 system_server
                Runnable r = forkSystemServer(abiList, zygoteSocketName, zygoteServer);
 
                if (r != null) {
                    r.run();
                    return;
                }
            }
 
        }
        
    }
    
    private static Runnable forkSystemServer(String abiList, String socketName,
            ZygoteServer zygoteServer) {
 
        try {
            ... ...
 
            /* Request to fork the system server process */
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
    
    }
 
}
```


## SystemServer[启动流程](https://so.csdn.net/so/search?q=%E5%90%AF%E5%8A%A8%E6%B5%81%E7%A8%8B&spm=1001.2101.3001.7020)

### **1、SystemServer.main()**

接下来就进到了 SystemServer.java 的 main() 函数处理流程：

```java
// frameworks/base/services/java/com/android/server/SystemServer.java
 
public final class SystemServer {
 
    /**
     * The main entry point from zygote.
     */
    public static void main(String[] args) {
        new SystemServer().run();        // 创建并运行，简单粗暴！
    }
    
}
```

这里直接 new 出一个 SystemServer 对象 并执行其 run() 方法。

### 2、SystemServer.run()

``` java
// frameworks/base/services/java/com/android/server/SystemServer.java
 
public final class SystemServer {
 
    private void run() {
        try {
            traceBeginAndSlog("InitBeforeStartServices");
            ... ...
 
            // 如果系统时钟早于1970年，则设置系统始终从1970年开始
            if (System.currentTimeMillis() < EARLIEST_SUPPORTED_TIME) {
                Slog.w(TAG, "System clock is before 1970; setting to 1970.");
                SystemClock.setCurrentTimeMillis(EARLIEST_SUPPORTED_TIME);
            }
 
            ... ...    
 
            if (!SystemProperties.get("persist.sys.language").isEmpty()) {
                // 设置区域，语言等选项    
                final String languageTag = Locale.getDefault().toLanguageTag();
 
                SystemProperties.set("persist.sys.locale", languageTag);
                SystemProperties.set("persist.sys.language", "");
                SystemProperties.set("persist.sys.country", "");
                SystemProperties.set("persist.sys.localevar", "");
            }
            ... ...
 
           // 清除 vm 内存增长上限，由于启动过程需要较多的虚拟机内存空间
            VMRuntime.getRuntime().clearGrowthLimit();                                     
 
            // 设置堆栈利用率，GC 后会重新计算堆栈空间大小
            VMRuntime.getRuntime().setTargetHeapUtilization(0.8f);                         
            
            // 针对部分设备依赖于运行时就产生指纹信息，因此需要在开机完成前已经定义
            Build.ensureFingerprintProperty();                                             
            
            // 访问环境变量前，需要明确地指定用户
            Environment.setUserRequired(true);                                             
            ... ...
 
            // 加载动态库 libandroid_services.so
            System.loadLibrary("android_servers");                                         
            
            // 检测上次关机过程是否失败，该方法可能不会返回
            performPendingShutdown();                                                        
            
           // 在 SystemServer 进程中也需要创建 Context 对象，初始化系统上下文
            createSystemContext();                                                         
            
            // 创建 SystemServiceManager 对象
            mSystemServiceManager = new SystemServiceManager(mSystemContext);  
            // SystemServer 进程主要是用来构建系统各种 service 服务，
            // 而 SystemServiceManager 就是这些服务的管理对象            
            mSystemServiceManager.setStartInfo(mRuntimeRestart,
                    mRuntimeStartElapsedTime, mRuntimeStartUptime);        
            
            // 将 SystemServiceManager 对象保存到 SystemServer 进程中的一个数据结构中            
            LocalServices.addService(SystemServiceManager.class, mSystemServiceManager);   
            
            // Prepare the thread pool for init tasks that can be parallelized
            SystemServerInitThreadPool.get();
        } finally {
            traceEnd();    // InitBeforeStartServices
        }
 
        // Start services.
        try {
            traceBeginAndSlog("StartServices");
            startBootstrapServices();    // 主要用于启动系统 Boot 级服务        
            startCoreServices();         // 主要用于启动系统核心的服务       
            startOtherServices();        // 主要用于启动一些非紧要或者非需要及时启动的服务    
            
        } catch (Throwable ex) {
            Slog.e("System", "******************************************");
            Slog.e("System", "************ Failure starting system services", ex);
            throw ex;
        } finally {
            traceEnd();
        }
        ... ...
 
        // 启动looper，以处理到来的消息，一直循环执行
        Looper.loop();                                                                       
        throw new RuntimeException("Main thread loop unexpectedly exited");
    }
 
}
```

以上就是 SystemServer.run() 方法的整个流程，简化如下

```java
// frameworks/base/services/java/com/android/server/SystemServer.java
 
public final class SystemServer {
 
    private void run() {
        try {
            // Initialize the system context.
            createSystemContext();    // 01. 初始化系统上下文
            
            // 02. 创建系统服务管理                                       
            mSystemServiceManager = new SystemServiceManager(mSystemContext);
            mSystemServiceManager.setRuntimeRestarted(mRuntimeRestart);                  
            LocalServices.addService(SystemServiceManager.class, mSystemServiceManager);   
        } finally {
            traceEnd();  // InitBeforeStartServices
        }
 
        // 03.启动系统各种服务
        try {
            startBootstrapServices();    // 启动引导服务
            startCoreServices();         // 启动核心服务
            startOtherServices();        // 启动其他服务
            
        }
 
        // Loop forever.
        Looper.loop();    // 一直循环执行  
        throw new RuntimeException("Main thread loop unexpectedly exited");
    }
 
}
```

接下来我们针对 SystemServer 所做的 三部分工作 单独分析！

### 3、[初始化](https://so.csdn.net/so/search?q=%E5%88%9D%E5%A7%8B%E5%8C%96&spm=1001.2101.3001.7020)系统上下文

```java
// frameworks/base/services/java/com/android/server/SystemServer.java
 
public final class SystemServer {
 
    private void createSystemContext() {
        ActivityThread activityThread = ActivityThread.systemMain();
        mSystemContext = activityThread.getSystemContext();
        mSystemContext.setTheme(DEFAULT_SYSTEM_THEME);
 
        final Context systemUiContext = activityThread.getSystemUiContext();
        systemUiContext.setTheme(DEFAULT_SYSTEM_THEME);
    }
 
}
```

跟踪 systemMain() 方法

```java
// frameworks/base/core/java/android/app/ActivityThread.java
 
public final class ActivityThread extends ClientTransactionHandler {
 
    public static ActivityThread systemMain() {
        // The system process on low-memory devices do not get to use hardware
        // accelerated drawing, since this can add too much overhead to the
        // process.
        if (!ActivityManager.isHighEndGfx()) {
            ThreadedRenderer.disable(true);    // 对于低内存的设备，禁用硬件加速
        } else {
            ThreadedRenderer.enableForegroundTrimming();
        }
        ActivityThread thread = new ActivityThread();
        thread.attach(true, 0);    // 调用 attach() 方法
        return thread;
    }
 
}
```

跟踪 attach() 方法

```java
// frameworks/base/core/java/android/app/ActivityThread.java
 
public final class ActivityThread extends ClientTransactionHandler {
 
    private void attach(boolean system, long startSeq) {
        sCurrentActivityThread = this;
        mSystemThread = system;
        if (!system) {
            ... ...
 
        } else {
            // Don't set application object here -- if the system crashes,
            // we can't display an alert, we just want to die die die.
            // 设置 SystemServer 进程在 DDMS 中显示的名字为 "system_process"
            android.ddm.DdmHandleAppName.setAppName("system_process",
                    UserHandle.myUserId());    // 如不设置，则显示"?"，无法调试该进程
            try {
                mInstrumentation = new Instrumentation();
                mInstrumentation.basicInit(this);
                // 首先通过 getSystemContext() 创建系统上下文，然后创建应用上下文
                ContextImpl context = ContextImpl.createAppContext(
                        this, getSystemContext().mPackageInfo);
                // 创建 Application
                mInitialApplication = context.mPackageInfo.makeApplication(true, null);
                // 调用 Application的 onCreate()
                mInitialApplication.onCreate();
            } catch (Exception e) {
                throw new RuntimeException(
                        "Unable to instantiate Application():" + e.toString(), e);
            }        
        }
        ... ...
 
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
        // 添加回调
        ViewRootImpl.addConfigCallback(configChangedCallback);                             
    }
 
}                               }}
```

我们发现 attach() 方法主要做了四件事：  
       （1）创建系统上下文：getSystemContext()；  
       （2）创建应用上下文：createAppContext()；  
       （3）创建 Application：makeApplication()；  
       （4）添加回调 configChangedCallback 到 ViewRootImpl。  
（1）创建系统上下文

```java
// frameworks/base/core/java/android/app/ActivityThread.java
 
public final class ActivityThread extends ClientTransactionHandler {
 
    public ContextImpl getSystemContext() {
        synchronized (this) {
            if (mSystemContext == null) {
                mSystemContext = ContextImpl.createSystemContext(this);
            }
            return mSystemContext;
        }
    }
 
}
```

```java
// frameworks/base/core/java/android/app/ContextImpl.java
 
class ContextImpl extends Context {
 
    static ContextImpl createSystemContext(ActivityThread mainThread) {
        // 这边 new 出来的 LoadedApk 将作为创建应用上下文的参数 packageInfo
        LoadedApk packageInfo = new LoadedApk(mainThread);
        // ContextImpl() 创建系统上下文 
        ContextImpl context = new ContextImpl(null, mainThread, packageInfo, null, null, null, 0,
                null, null);
        context.setResources(packageInfo.getResources());
        context.mResources.updateConfiguration(context.mResourcesManager.getConfiguration(),
                context.mResourcesManager.getDisplayMetrics());
        return context;
    }
 
}
```

（2）创建应用上下文

```java
// frameworks/base/core/java/android/app/ContextImpl.java
 
class ContextImpl extends Context {
 
    static ContextImpl createAppContext(ActivityThread mainThread, LoadedApk packageInfo) {
        return createAppContext(mainThread, packageInfo, null);
    }
 
    static ContextImpl createAppContext(ActivityThread mainThread, LoadedApk packageInfo,
            String opPackageName) {
        if (packageInfo == null) throw new IllegalArgumentException("packageInfo");
        // ContextImpl()创建应用上下文
        ContextImpl context = new ContextImpl(null, mainThread, packageInfo, null, null, null, 0,
                null, opPackageName);
        context.setResources(packageInfo.getResources());
        return context;
    }
 
}
```

我们可以看出：new ContextImpl() 时，系统上下文和应用上下文的参数是一样的，createAppContext() 中的参数 packageInfo，就是 createSystemContext() 中 new 的 LoadedApk。  
创建完成之后，系统上下文赋值给了 ActivityThread 的成员变量 mSystemContext，而应用上下文只是作为函数中的局部变量临时使用。  
（3）创建 Application

```java
// frameworks/base/core/java/android/app/LoadedApk.java
 
public final class LoadedApk {
 
    public Application makeApplication(boolean forceDefaultAppClass,
            Instrumentation instrumentation) {
        if (mApplication != null) {
            return mApplication;
        }
 
        Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "makeApplication");
 
        Application app = null;
 
        String appClass = mApplicationInfo.className;
        if (forceDefaultAppClass || (appClass == null)) {    // forceDefaultAppClass 为 true                               
            appClass = "android.app.Application";
        }
 
        try {
            java.lang.ClassLoader cl = getClassLoader();
            // 此 LoadedApk 对象是 createSystemContext 时 new 的，mPackageName = "android"
            if (!mPackageName.equals("android")) {                                         
                initializeJavaContextClassLoader();
            }
            // 又创建了一个局部应用上下文
            ContextImpl appContext = ContextImpl.createAppContext(mActivityThread, this);  
            // 创建 Application 
            app = mActivityThread.mInstrumentation.newApplication(
                    cl, appClass, appContext);
            appContext.setOuterContext(app);
        } catch (Exception e) {
            if (!mActivityThread.mInstrumentation.onException(app, e)) {
                Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
                throw new RuntimeException(
                    "Unable to instantiate application " + appClass
                    + ": " + e.toString(), e);
            }
        }
 
        // 将前面创建的 app 添加到应用列表
        mActivityThread.mAllApplications.add(app);
        mApplication = app;
        ... ...
 
        Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
 
        return app;
    }
 
}
```

### **4、创建系统服务管理**

回顾下创建系统服务管理相关代码：

```java
// frameworks/base/services/java/com/android/server/SystemServer.java
 
public final class SystemServer {
 
    private void run() {
        try {
            // Initialize the system context.
            createSystemContext();    // 01. 初始化系统上下文
            
            // 02. 创建系统服务管理                                       
            mSystemServiceManager = new SystemServiceManager(mSystemContext);
            mSystemServiceManager.setRuntimeRestarted(mRuntimeRestart);                  
            LocalServices.addService(SystemServiceManager.class, mSystemServiceManager);   
        } finally {
            traceEnd();  // InitBeforeStartServices
        }
    ... ...
    
}
```

这一步只是 new 了一个 SystemServiceManager，并将其添加到本地服务列表中。mSystemContext 为第一步中创建的系统上下文。本地服务列表是以类为 key 保存的一个列表，即列表中某种类型的对象最多只能有一个。  
new SystemServiceManager()

``` java
// frameworks/base/services/core/java/com/android/server/SystemServiceManager.java
 
public class SystemServiceManager {
    ... ...
 
    // 系统服务列表，系统服务必须继承 SystemService
    private final ArrayList<SystemService> mServices = new ArrayList<SystemService>();
 
    // 当前处于开机过程的哪个阶段
    private int mCurrentPhase = -1;
 
    SystemServiceManager(Context context) {
        mContext = context;
    }
 
    @SuppressWarnings("unchecked")
    // 通过类名启动系统服务，可能会找不到类而抛异常
    public SystemService startService(String className) {
        final Class<SystemService> serviceClass;
        try {
            serviceClass = (Class<SystemService>)Class.forName(className);
        } catch (ClassNotFoundException ex) {
            Slog.i(TAG, "Starting " + className);
            throw new RuntimeException("Failed to create service " + className
                    + ": service class not found, usually indicates that the caller should "
                    + "have called PackageManager.hasSystemFeature() to check whether the "
                    + "feature is available on this device before trying to start the "
                    + "services that implement it", ex);
        }
        return startService(serviceClass);
    }
 
    @SuppressWarnings("unchecked")
    // 创建并启动系统服务，系统服务类必须继承 SystemService
    public <T extends SystemService> T startService(Class<T> serviceClass) {
        try {
            final String name = serviceClass.getName();
            Slog.i(TAG, "Starting " + name);
            Trace.traceBegin(Trace.TRACE_TAG_SYSTEM_SERVER, "StartService " + name);
 
            // Create the service.
            if (!SystemService.class.isAssignableFrom(serviceClass)) {
                throw new RuntimeException("Failed to create " + name
                        + ": service must extend " + SystemService.class.getName());
            }
            final T service;
            try {
                Constructor<T> constructor = serviceClass.getConstructor(Context.class);
                service = constructor.newInstance(mContext);
            } catch (InstantiationException ex) {
                ... ...
 
            }
 
            startService(service);
            return service;
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
        }
    }
 
    public void startService(@NonNull final SystemService service) {
        // Register it.
        mServices.add(service);
        // Start it.
        long time = SystemClock.elapsedRealtime();
        try {
            service.onStart();
        } catch (RuntimeException ex) {
            throw new RuntimeException("Failed to start service " + service.getClass().getName()
                    + ": onStart threw an exception", ex);
        }
        warnIfTooLong(SystemClock.elapsedRealtime() - time, service, "onStart");
    }
 
    // 通知系统服务到了开机的哪个阶段，会遍历调用所有系统服务的 onBootPhase() 函数
    public void startBootPhase(final int phase) {
        ... ...
 
        try {
            Trace.traceBegin(Trace.TRACE_TAG_SYSTEM_SERVER, "OnBootPhase " + phase);
            final int serviceLen = mServices.size();
            for (int i = 0; i < serviceLen; i++) {
                final SystemService service = mServices.get(i);
                long time = SystemClock.elapsedRealtime();
                Trace.traceBegin(Trace.TRACE_TAG_SYSTEM_SERVER, service.getClass().getName());
                try {
                    service.onBootPhase(mCurrentPhase);
                } catch (Exception ex) {
                    throw new RuntimeException("Failed to boot service "
                            + service.getClass().getName()
                            + ": onBootPhase threw an exception during phase "
                            + mCurrentPhase, ex);
                }
                warnIfTooLong(SystemClock.elapsedRealtime() - time, service, "onBootPhase");
                Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
            }
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
        }
    }
    
}
```

### **5、启动系统各种服务**

``` java
// frameworks/base/services/java/com/android/server/SystemServer.java
 
public final class SystemServer {
 
    private void run() {
        // 01. 初始化系统上下文
        // 02. 创建系统服务管理                                       
        // 03.启动系统各种服务
        try {
            startBootstrapServices();    // 启动引导服务
            startCoreServices();         // 启动核心服务
            startOtherServices();        // 启动其他服务
            
        }
 
        // Loop forever.
        Looper.loop();    // 一直循环执行  
        throw new RuntimeException("Main thread loop unexpectedly exited");
    }
 
}
```

（1）启动引导服务startBootstrapServices()

``` java
// frameworks/base/services/java/com/android/server/SystemServer.java
 
public final class SystemServer {
 
    /**
     * Starts the small tangle of critical services that are needed to get the system off the
     * ground.  These services have complex mutual dependencies which is why we initialize them all
     * in one place here.  Unless your service is also entwined in these dependencies, it should be
     * initialized in one of the other functions.
     */
    private void startBootstrapServices() {
        ... ...
 
        // 启动 Installer 服务，阻塞等待与 installd 建立 socket 通道
        Installer installer = mSystemServiceManager.startService(Installer.class);
 
        mSystemServiceManager.startService(DeviceIdentifiersPolicyService.class);
 
        mSystemServiceManager.startService(UriGrantsManagerService.Lifecycle.class);
 
        // 启动 ActivityManagerService
        ActivityTaskManagerService atm = mSystemServiceManager.startService(
                ActivityTaskManagerService.Lifecycle.class).getService();
        mActivityManagerService.setSystemServiceManager(mSystemServiceManager);
        mActivityManagerService.setInstaller(installer);
        mWindowManagerGlobalLock = atm.getGlobalLock();
 
        // 启动 PowerManagerService
        mPowerManagerService = mSystemServiceManager.startService(PowerManagerService.class);
 
        mSystemServiceManager.startService(ThermalManagerService.class);
 
        // PowerManagerService 就绪，AMS 初始化电源管理
        mActivityManagerService.initPowerManagement();
 
        mActivityManagerService.initPowerManagement();
 
        mSystemServiceManager.startService(RecoverySystemService.class);
 
        RescueParty.noteBoot(mSystemContext);
 
        // 启动 LightsService
        mSystemServiceManager.startService(LightsService.class);
 
        // 启动 DisplayManagerService
        mDisplayManagerService = mSystemServiceManager.startService(DisplayManagerService.class);
 
        // We need the default display before we can initialize the package manager.
        mSystemServiceManager.startBootPhase(SystemService.PHASE_WAIT_FOR_DEFAULT_DISPLAY);
 
        // 当设备正在加密时，仅运行核心应用
        String cryptState = SystemProperties.get("vold.decrypt");
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
 
        // 启动 PackageManagerService
        try {
            Watchdog.getInstance().pauseWatchingCurrentThread("packagemanagermain");
            mPackageManagerService = PackageManagerService.main(mSystemContext, installer,
                    mFactoryTestMode != FactoryTest.FACTORY_TEST_OFF, mOnlyCore);
        } finally {
            Watchdog.getInstance().resumeWatchingCurrentThread("packagemanagermain");
        }
 
        mFirstBoot = mPackageManagerService.isFirstBoot();
        mPackageManager = mSystemContext.getPackageManager();
        ... ...
 
        // 将 UserManagerService 添加到服务列表，该服务是在 PackageManagerService 中初始化的        
        mSystemServiceManager.startService(UserManagerService.LifeCycle.class);
 
        // 初始化用来缓存包资源的属性缓存
        AttributeCache.init(mSystemContext);
 
        // Set up the Application instance for the system process and get started.
        mActivityManagerService.setSystemProcess();
        ... ...
 
        mSensorServiceStart = SystemServerInitThreadPool.get().submit(() -> {
            TimingsTraceLog traceLog = new TimingsTraceLog(
                    SYSTEM_SERVER_TIMING_ASYNC_TAG, Trace.TRACE_TAG_SYSTEM_SERVER);
            traceLog.traceBegin(START_SENSOR_SERVICE);
            startSensorService();    // 启动传感器服务
            traceLog.traceEnd();
        }, START_SENSOR_SERVICE);
    }
 
}
```

首先等待 installd 启动完成，然后启动一些相互依赖的关键服务。

（2）启动核心服务startCoreServices()

``` java
// frameworks/base/services/java/com/android/server/SystemServer.java
 
public final class SystemServer {
 
    /**
     * Starts some essential services that are not tangled up in the bootstrap process.
     */
    private void startCoreServices() {
        // 启动 BatteryService，用于统计电池电量，需要 LightService
        mSystemServiceManager.startService(BatteryService.class);
 
        // 启动 UsageStatsService，用于统计应用使用情况
        mSystemServiceManager.startService(UsageStatsService.class);
        mActivityManagerService.setUsageStatsManager(
                LocalServices.getService(UsageStatsManagerInternal.class));
 
        // 启动 WebViewUpdateService
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_WEBVIEW)) {
            mWebViewUpdateService = mSystemServiceManager.startService(WebViewUpdateService.class);
        }
        .... ...
 
    }
 
}
```

（3）启动其他服务startOtherServices()

代码很长（1200多行...），但是逻辑简单，主要是启动各种服务。

``` java
// frameworks/base/services/java/com/android/server/SystemServer.java
 
public final class SystemServer {
 
    /**
     * Starts a miscellaneous grab bag of stuff that has yet to be refactored and organized.
     */
    private void startOtherServices() {
        ... ...
 
        try {
            ... ...
 
            // 调度策略
            ServiceManager.addService("scheduling_policy", new SchedulingPolicyService());
 
            mSystemServiceManager.startService(TelecomLoaderService.class);
 
            // 提供电话注册、管理服务，可以获取电话的链接状态、信号强度等
            telephonyRegistry = new TelephonyRegistry(context);
            ServiceManager.addService("telephony.registry", telephonyRegistry);
 
            mEntropyMixer = new EntropyMixer(context);
            
            mContentResolver = context.getContentResolver();
 
            // 提供所有账号、密码、认证管理等等的服务
            mSystemServiceManager.startService(ACCOUNT_SERVICE_CLASS);
            ... ...
 
            // 振动器服务
            vibrator = new VibratorService(context);
            ServiceManager.addService("vibrator", vibrator);
            ... ...
 
            inputManager = new InputManagerService(context);
 
            // WMS needs sensor service ready
            ConcurrentUtils.waitForFutureNoInterrupt(mSensorServiceStart, START_SENSOR_SERVICE);
            mSensorServiceStart = null;
            wm = WindowManagerService.main(context, inputManager, !mFirstBoot, mOnlyCore,
                    new PhoneWindowManager(), mActivityManagerService.mActivityTaskManager);
            ServiceManager.addService(Context.WINDOW_SERVICE, wm, /* allowIsolated= */ false,
                    DUMP_FLAG_PRIORITY_CRITICAL | DUMP_FLAG_PROTO);
            ServiceManager.addService(Context.INPUT_SERVICE, inputManager,
                    /* allowIsolated= */ false, DUMP_FLAG_PRIORITY_CRITICAL);
            ... ...
 
        } catch (RuntimeException e) {
            Slog.e("System", "******************************************");
            Slog.e("System", "************ Failure starting core service", e);
        }
        ... ...
 
        LockSettingsService               // 屏幕锁定服务，管理每个用户的相关锁屏信息
        DeviceIdleController              // Doze模式的主要驱动
        DevicePolicyManagerService        // 提供一些系统级别的设置及属性
        StatusBarManagerService           // 状态栏管理服务
        ClipboardService                  // 系统剪切板服务
        NetworkManagementService          // 网络管理服务
        TextServicesManagerService        // 文本服务，例如文本检查等
        NetworkScoreService               // 网络评分服务
        NetworkStatsService               // 网络状态服务
        NetworkPolicyManagerService       // 网络策略服务
        WifiP2pService                    // Wifi Direct服务
        WifiService                       // Wifi服务
        WifiScanningService               // Wifi扫描服务
        RttService                        // Wifi相关
        EthernetService                   // 以太网服务
        ConnectivityService               // 网络连接管理服务
        NsdService                        // 网络发现服务
        NotificationManagerService        // 通知栏管理服务
        DeviceStorageMonitorService       // 磁盘空间状态检测服务
        LocationManagerService            // 位置服务，GPS、定位等
        CountryDetectorService            // 检测用户国家
        SearchManagerService              // 搜索管理服务
        DropBoxManagerService             // 用于系统运行时日志的存储于管理
        WallpaperManagerService           // 壁纸管理服务
        AudioService                      // AudioFlinger的上层管理封装，主要是音量、音效、声道及铃声等的管理
        DockObserver                      // 如果系统有个座子，当手机装上或拔出这个座子的话，就得靠他来管理了
        WiredAccessoryManager             // 监视手机和底座上的耳机
        UsbService                        // USB服务
        SerialService                     // 串口服务
        TwilightService                   // 指出用户当前所在位置是否为晚上，被 UiModeManager 等用来调整夜间模式
        BackupManagerService              // 备份服务
        AppWidgetService                  // 提供Widget的管理和相关服务
        VoiceInteractionManagerService    // 语音交互管理服务
        DiskStatsService                  // 磁盘统计服务，供dumpsys使用
        SamplingProfilerService           // 用于耗时统计等
        NetworkTimeUpdateService          // 监视网络时间，当网络时间变化时更新本地时间。
        CertBlacklister                   // 提供一种机制更新SSL certificate blacklist
        DreamManagerService               // 屏幕保护
        PrintManagerService               // 打印服务
        HdmiControlService                // HDMI控制服务
        FingerprintService                // 指纹服务
        ... ...
 
    }
 
}
```

## 总结

![](https://i-blog.csdnimg.cn/img_convert/40c4f66ce2ae5f73e47f9458d6752043.png)

可以看到PowerManagerService先于AudioService启动，此时如果在PowerManagerService启动后立刻调用AudioService中的接口就会引发 JE 崩溃，进而导致system\_server崩溃。