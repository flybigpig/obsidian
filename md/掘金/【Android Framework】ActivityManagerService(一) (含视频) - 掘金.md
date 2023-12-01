## 回顾

照例，我们对之前的内容进行一个简单的回顾，在Android中，第一个启动的进程是`init`，它主要`解析init.rc`文件，启动对应的各种各样的`service`，例如`media，zygote，surfaceflinger`，以及开启了`属性服务`(类似Windows的注册表)等。接下来我们讲了`Zygote`，Zygote是由`init`启动起来的，它是一个孵化器，`孵化了system_server以及其他的应用程序`，`预加载了一些class类和drawable color等资源`。`system_server`是系统用来启动管理`service`的`入口`，比如我们常用的`AMS、WMS、PMS`等等都是它来创建的,system\_server加载了`framework-res.apk`，接着调用`startBootstrapServices`、`startCoreServices`、`startOtherServices`开启了非常多的服务，以及开启了`WatchDog`监控service。接下来讲了`ServiceManage`，它是一个服务的提供者，可以让应用获取到系统的各种服务，还有`Binder`机制。接下来我们讲了`Launcher`的启动，它是由`system_server`开启的,通过`LauncherModel`进行`IPC`通信`(Binder)`调用`PackageManagerService`的`queryIntentActivities`获取到所有应用的信息，然后绑定数据源到`RecyclerView`中，而它的点击事件则是通过`ItemClickHandler`来进行分发的。

具体的细节可以参考之前写的文章和视频：

[【Android FrameWork】第一个启动的程序--init](https://juejin.cn/post/7213733567606685753 "https://juejin.cn/post/7213733567606685753")

[【Android FrameWork】Zygote](https://juejin.cn/post/7214068367607119933 "https://juejin.cn/post/7214068367607119933")

[【Android FrameWork】SystemServer](https://juejin.cn/post/7214493929566945340 "https://juejin.cn/post/7214493929566945340")

[【Android FrameWork】ServiceManager(一)](https://juejin.cn/post/7216537771448598585#heading-4 "https://juejin.cn/post/7216537771448598585#heading-4")

[【Android FrameWork】ServiceManager(二)](https://juejin.cn/post/7216536069285675045#heading-9 "https://juejin.cn/post/7216536069285675045#heading-9")

[Android Framework】Launcher3](https://juejin.cn/post/7218129062744227896#heading-1 "https://juejin.cn/post/7218129062744227896#heading-1")

## 介绍

首先介绍下`ActivityManagerService`，它是`Android`系统的`核心`，它管理了系统的四大组件:`Activity`、`Service`、`ContentProvider`、`Broadcast`。它除了管理四大组件外，同时也负责管理和调度所有的进程。

再介绍下我们这次的第二个主角`Activity`，它是最复杂的一个组件，它负责`UI`的显示以及处理各种输入事件(绘制、点击)。关于他的生命周期 借用下官方的图:

![activity.gif](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3342cdc95cee4ce1a1f415f60976279a~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

`onCreate`:当`Activity`被创建后第一个执行

`onStart`:当`Activity`的准备完毕后调用

`onResume`:当`Activity`成为激活状态时调用

`onPause`:当`Activity`被切换到后台的时候，进入暂停状态并且调用

`onStop`:当`Activity`不可见的时候调用

`onDestory`:当`Activity`被销毁时调用

## 正文

上次讲到了`Launcher`的点击事件,会根据Appinfo 来调用到`Activity`的`startActivity`函数 ,现在我们就跟一下,看看`startActivity`是怎么和`AMS`进行通信的，以及`AMS`是怎么处理的呢？

## 1.AMS的启动

之前在讲`system_server`的时候，我们已经知道`AMS`是运行在`system_server`中的，代码如下:

```
文件目录:/frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java
private void startBootstrapServices() {
//……………………
//创建ActivityTaskManagerService  ActivityManagerService 并关联     mSystemServiceManager installer
ActivityTaskManagerService atm = mSystemServiceManager.startService(
        ActivityTaskManagerService.Lifecycle.class).getService();
mActivityManagerService = ActivityManagerService.Lifecycle.startService(
        mSystemServiceManager, atm);
mActivityManagerService.setSystemServiceManager(mSystemServiceManager);
mActivityManagerService.setInstaller(installer);
//将服务添加到ServiceManager
mActivityManagerService.setSystemProcess();
//……………………
}

public Lifecycle(Context context) {
    super(context);
    //创建了AMS的实例
    mService = new ActivityManagerService(context, sAtm);
}

public ActivityManagerService(Context systemContext, ActivityTaskManagerService atm) {
    LockGuard.installLock(this, LockGuard.INDEX_ACTIVITY);
    mInjector = new Injector();
    mContext = systemContext;//设置context 这里的context 不是我们应用的context是framework-res.apk

    mFactoryTest = FactoryTest.getMode();
    //获取system_server中的ActivityThread
    mSystemThread = ActivityThread.currentActivityThread();
    mUiContext = mSystemThread.getSystemUiContext();

    Slog.i(TAG, "Memory class: " + ActivityManager.staticGetMemoryClass());
    //创建处理消息的线程和Handler
    mHandlerThread = new ServiceThread(TAG,
            THREAD_PRIORITY_FOREGROUND, false /*allowIo*/);
    mHandlerThread.start();
    mHandler = new MainHandler(mHandlerThread.getLooper());
    mUiHandler = mInjector.getUiHandler(this);
       
    //创建管理广播的队列 前台和后台
    mFgBroadcastQueue = new BroadcastQueue(this, mHandler,
            "foreground", foreConstants, false);
    mBgBroadcastQueue = new BroadcastQueue(this, mHandler,
            "background", backConstants, true);
    mOffloadBroadcastQueue = new BroadcastQueue(this, mHandler,
            "offload", offloadConstants, true);
    mBroadcastQueues[0] = mFgBroadcastQueue;
    mBroadcastQueues[1] = mBgBroadcastQueue;
    mBroadcastQueues[2] = mOffloadBroadcastQueue;
    //管理service的对象
    mServices = new ActiveServices(this);
    //管理ContentProvider的对象
    mProviderMap = new ProviderMap(this);
    mPackageWatchdog = PackageWatchdog.getInstance(mUiContext);
    mAppErrors = new AppErrors(mUiContext, this, mPackageWatchdog);
    //获取系统目录
    final File systemDir = SystemServiceManager.ensureSystemDir();
    //创建电池状态管理的service
    mBatteryStatsService = new BatteryStatsService(systemContext, systemDir,
            BackgroundThread.get().getHandler());
     //创建进程状态管理的服务
    mProcessStats = new ProcessStatsService(this, new File(systemDir, "procstats"));
    
    mAppOpsService = mInjector.getAppOpsService(new File(systemDir, "appops.xml"), mHandler);

    mUgmInternal = LocalServices.getService(UriGrantsManagerInternal.class);
    //创建UserController
    mUserController = new UserController(this);

    mPendingIntentController = new PendingIntentController(
            mHandlerThread.getLooper(), mUserController);

    if (SystemProperties.getInt("sys.use_fifo_ui", 0) != 0) {
        mUseFifoUiScheduling = true;
    }

    mTrackingAssociations = "1".equals(SystemProperties.get("debug.track-associations"));
    mIntentFirewall = new IntentFirewall(new IntentFirewallInterface(), mHandler);
    //赋值ActivityTaskManagerService
    mActivityTaskManager = atm;
    //进行初始化
    mActivityTaskManager.initialize(mIntentFirewall, mPendingIntentController,
            DisplayThread.get().getLooper());
    mAtmInternal = LocalServices.getService(ActivityTaskManagerInternal.class);
    //创建CPU的统计线程
    mProcessCpuThread = new Thread("CpuTracker") {
        @Override
        public void run() {
            synchronized (mProcessCpuTracker) {
                mProcessCpuInitLatch.countDown();
                mProcessCpuTracker.init();
            }
            while (true) {
                try {
                    try {
                        synchronized(this) {
                            final long now = SystemClock.uptimeMillis();
                            long nextCpuDelay = (mLastCpuTime.get()+MONITOR_CPU_MAX_TIME)-now;
                            long nextWriteDelay = (mLastWriteTime+BATTERY_STATS_TIME)-now;
                            //Slog.i(TAG, "Cpu delay=" + nextCpuDelay
                            //        + ", write delay=" + nextWriteDelay);
                            if (nextWriteDelay < nextCpuDelay) {
                                nextCpuDelay = nextWriteDelay;
                            }
                            if (nextCpuDelay > 0) {
                                mProcessCpuMutexFree.set(true);
                                this.wait(nextCpuDelay);
                            }
                        }
                    } catch (InterruptedException e) {
                    }
                    updateCpuStatsNow();
                } catch (Exception e) {
                    Slog.e(TAG, "Unexpected exception collecting process stats", e);
                }
            }
        }
    };

    mHiddenApiBlacklist = new HiddenApiSettings(mHandler, mContext);
    //添加到看门狗 进行检测
    Watchdog.getInstance().addMonitor(this);
    Watchdog.getInstance().addThread(mHandler);

    updateOomAdjLocked(OomAdjuster.OOM_ADJ_REASON_NONE);
    try {
        Process.setThreadGroupAndCpuset(BackgroundThread.get().getThreadId(),
                Process.THREAD_GROUP_SYSTEM);
        Process.setThreadGroupAndCpuset(
                mOomAdjuster.mAppCompact.mCompactionThread.getThreadId(),
                Process.THREAD_GROUP_SYSTEM);
    } catch (Exception e) {
        Slog.w(TAG, "Setting background thread cpuset failed");
    }

}


private void start() {
    removeAllProcessGroups();
    //开启cpu线程
    mProcessCpuThread.start();
    //把电池状态管理添加到ServiceManager中
    mBatteryStatsService.publish();
    //把AppOpsService添加到ServiceManager中
    mAppOpsService.publish(mContext);
    Slog.d("AppOps", "AppOpsService published");
    LocalServices.addService(ActivityManagerInternal.class, new LocalService());
    mActivityTaskManager.onActivityManagerInternalAdded();
    mUgmInternal.onActivityManagerInternalAdded();
    mPendingIntentController.onActivityManagerInternalAdded();
    // Wait for the synchronized block started in mProcessCpuThread,
    // so that any other access to mProcessCpuTracker from main thread
    // will be blocked during mProcessCpuTracker initialization.
    try {
        mProcessCpuInitLatch.await();
    } catch (InterruptedException e) {
        Slog.wtf(TAG, "Interrupted wait during start", e);
        Thread.currentThread().interrupt();
        throw new IllegalStateException("Interrupted wait during start");
    }
}


public static ActivityManagerService startService(
        SystemServiceManager ssm, ActivityTaskManagerService atm) {
    sAtm = atm;
    //反射创建，并添加到mServices中，调用onStart函数
    return ssm.startService(ActivityManagerService.Lifecycle.class).getService();
}


private void startOtherServices() {
//……………………
    // 调用 AMS 的 systemReady
    mActivityManagerService.systemReady(...);
}


```

我们知道是通过反射构建了`ActivityManagerService.Lifecycle`对象，初始化了管理四大组件的对象以及一些服务。然后调用`setSystemProcess`。

```
public void setSystemProcess() {
    try {
    //添加自己到ServiceManager
        ServiceManager.addService(Context.ACTIVITY_SERVICE, this, /* allowIsolated= */ true,
                DUMP_FLAG_PRIORITY_CRITICAL | DUMP_FLAG_PRIORITY_NORMAL | DUMP_FLAG_PROTO);
       //添加进程管理服务 可以获取每个进程内存使用情况
        ServiceManager.addService(ProcessStats.SERVICE_NAME, mProcessStats);
        //添加 MemBinder，是用来dump每个进程内存信息的服务
        ServiceManager.addService("meminfo", new MemBinder(this), /* allowIsolated= */ false,
                DUMP_FLAG_PRIORITY_HIGH);
        //添加GraphicsBinder 可以获取每个进程图形加速的服务
        ServiceManager.addService("gfxinfo", new GraphicsBinder(this));
        //添加 DbBinder  获取每个进程数据库的服务
        ServiceManager.addService("dbinfo", new DbBinder(this));
        if (MONITOR_CPU_USAGE) {
            ServiceManager.addService("cpuinfo", new CpuBinder(this),
                    /* allowIsolated= */ false, DUMP_FLAG_PRIORITY_CRITICAL);
        }
        //权限服务
        ServiceManager.addService("permission", new PermissionController(this));
        //进程相关信息
        ServiceManager.addService("processinfo", new ProcessInfoService(this));
        //获取到当前的ApplicationInfo 也就是framework-res.apk
        ApplicationInfo info = mContext.getPackageManager().getApplicationInfo(
                "android", STOCK_PM_FLAGS | MATCH_SYSTEM_ONLY);
        mSystemThread.installSystemApplicationInfo(info, getClass().getClassLoader());

        synchronized (this) {
        //把system_server添加到AMS的process管理者中
            ProcessRecord app = mProcessList.newProcessRecordLocked(info, info.processName,
                    false,
                    0,
                    new HostingRecord("system"));
            app.setPersistent(true);
            app.pid = MY_PID;
            app.getWindowProcessController().setPid(MY_PID);
            app.maxAdj = ProcessList.SYSTEM_ADJ;
            app.makeActive(mSystemThread.getApplicationThread(), mProcessStats);
            mPidsSelfLocked.put(app);
            mProcessList.updateLruProcessLocked(app, false, null);
            updateOomAdjLocked(OomAdjuster.OOM_ADJ_REASON_NONE);
        }
    } catch (PackageManager.NameNotFoundException e) {
        throw new RuntimeException(
                "Unable to find android system package", e);
    }
    //应用启动的监听
    mAppOpsService.startWatchingMode(AppOpsManager.OP_RUN_IN_BACKGROUND, null,
            new IAppOpsCallback.Stub() {
                @Override public void opChanged(int op, int uid, String packageName) {
                    if (op == AppOpsManager.OP_RUN_IN_BACKGROUND && packageName != null) {
                        if (mAppOpsService.checkOperation(op, uid, packageName)
                                != AppOpsManager.MODE_ALLOWED) {
                            runInBackgroundDisabled(uid);
                        }
                    }
                }
            });
}
```

在`systemProcess`中，又注册了一些服务，获取到了当前的`ApplicationInfo`对象，然后把`ApplicationInfo`转换成`ProcessRecord`对象，添加到`AMS`的`mPidsSelfLocked`管理中。接着在`system_server`的`startOtherServices`中调用了`systemReady`。

```
public void systemReady(final Runnable goingCallback, TimingsTraceLog traceLog) {
    synchronized(this) {
        if (mSystemReady) {
        if (goingCallback != null) {
            goingCallback.run();
        }
        return;
    }
        mLocalDeviceIdleController
                = LocalServices.getService(DeviceIdleController.LocalService.class);
                //调用ActivityTaskManagerService的systemReady 准备和task相关的服务
        mActivityTaskManager.onSystemReady();
        //装载用户Profile的信息
        mUserController.onSystemReady();
        //启动App权限监听
        mAppOpsService.systemReady();
        mSystemReady = true;
    }

    //查找需要kill的进程
    ArrayList<ProcessRecord> procsToKill = null;
    synchronized(mPidsSelfLocked) {
        for (int i=mPidsSelfLocked.size()-1; i>=0; i--) {
            ProcessRecord proc = mPidsSelfLocked.valueAt(i);
            //判断进程是否有FLAG_PERSISTENT标志
            if (!isAllowedWhileBooting(proc.info)){
                if (procsToKill == null) {
                    procsToKill = new ArrayList<ProcessRecord>();
                }
                procsToKill.add(proc);
            }
        }
    }

    synchronized(this) {
        if (procsToKill != null) {//清理
            for (int i=procsToKill.size()-1; i>=0; i--) {
                ProcessRecord proc = procsToKill.get(i);
                mProcessList.removeProcessLocked(proc, true, false, "system update done");
            }
        }
        //清理完成
        mProcessesReady = true;
    }
  //………………

    synchronized (this) {
          //开启 开机就需要启动的app
        startPersistentApps(PackageManager.MATCH_DIRECT_BOOT_AWARE);

        //开启Launcher
        mAtmInternal.startHomeOnAllDisplays(currentUserId, "systemReady");
        
 //发送ACTION_USER_STARTED 广播
Intent intent = new Intent(Intent.ACTION_USER_STARTED);
intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY
        | Intent.FLAG_RECEIVER_FOREGROUND);
intent.putExtra(Intent.EXTRA_USER_HANDLE, currentUserId);
broadcastIntentLocked(null, null, intent,
        null, null, 0, null, null, null, OP_NONE,
        null, false, false, MY_PID, SYSTEM_UID, callingUid, callingPid,
        currentUserId);
 //发送ACTION_USER_STARTING广播
intent = new Intent(Intent.ACTION_USER_STARTING);
intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY);
intent.putExtra(Intent.EXTRA_USER_HANDLE, currentUserId);
broadcastIntentLocked(null, null, intent,
        null, new IIntentReceiver.Stub() {
            @Override
            public void performReceive(Intent intent, int resultCode, String data,
                    Bundle extras, boolean ordered, boolean sticky, int sendingUser)
                    throws RemoteException {
            }
        }, 0, null, null,
        new String[] {INTERACT_ACROSS_USERS}, OP_NONE,
        null, true, false, MY_PID, SYSTEM_UID, callingUid, callingPid,
        UserHandle.USER_ALL);
        }
    }
}

    final ProcessRecord addAppLocked(ApplicationInfo info, String customProcess, boolean isolated,
            boolean disableHiddenApiChecks, boolean mountExtStorageFull, String abiOverride) {
        ProcessRecord app;
        // isolated 为 true 表示要启动一个新的进程
        if (!isolated) {
            // 在已经启动的进程列表中查找
            app = getProcessRecordLocked(customProcess != null ? customProcess : info.processName,
                    info.uid, true);
        } else {
            app = null;
        }

        if (app == null) {
            // 新建一个 ProcessRecord 对象
            app = mProcessList.newProcessRecordLocked(info, customProcess, isolated, 0,
                    new HostingRecord("added application",
                            customProcess != null ? customProcess : info.processName));
            mProcessList.updateLruProcessLocked(app, false, null);
            updateOomAdjLocked(OomAdjuster.OOM_ADJ_REASON_PROCESS_BEGIN);
        }

        // This package really, really can not be stopped.
        try {
            // 将包的stopped状态设置为false
            // 如果为ture那么所有广播都无法接收，除非带有标记FLAG_INCLUDE_STOPPED_PACKAGES的广播
            // 正经的广播都不会带有这个标记
            AppGlobals.getPackageManager().setPackageStoppedState(
                    info.packageName, false, UserHandle.getUserId(app.uid));
        } catch (RemoteException e) {
        } catch (IllegalArgumentException e) {
            Slog.w(TAG, "Failed trying to unstop package "
                    + info.packageName + ": " + e);
        }

        if ((info.flags & PERSISTENT_MASK) == PERSISTENT_MASK) {
//            设置persistent标记
            app.setPersistent(true);
            app.maxAdj = ProcessList.PERSISTENT_PROC_ADJ;
        }
        if (app.thread == null && mPersistentStartingProcesses.indexOf(app) < 0) {
            mPersistentStartingProcesses.add(app);
            //启动进程
            mProcessList.startProcessLocked(app, new HostingRecord("added application",
                    customProcess != null ? customProcess : app.processName),
                    disableHiddenApiChecks, mountExtStorageFull, abiOverride);
        }

        return app;
    }

```

在`systemReady`中，调用了`ATMS(ActivityTaskManagerService)`的`onSystemReady`，以及`AppopsService`、`UserController`进行初始化工作，接着从`mPidsSelfLocked`集合中查找非`FLAG_PERSISTENT`标志的进程，并且进行清理。开启需要开机启动的应用( `android:persistent`)，接着开启`Launcher`,发送`ACTION_USER_STARTED`和`ACTION_USER_STARTING`广播。 /**稍后我们在创建进程的时候会接触到这个集合。这个标记就和`PMS`相关了，我们之前没有介绍，简单看一下就是在updatePackagesIfNeeded . getPackagesForDexopt的方法中发送一个ACTION\_PRE\_BOOT\_COMPLETED的广播，如果应用相应这个广播并且加入FLAG\_PERSISTENT标志，就可以存活下来。对接收这个广播的package加载要优先于systemReady，也就是优先其他package的加载。用于开机优先打开的app flag\_persistent**/

## 2.进程的启动

### 1.Launcher 创建请求的发送

让我们暂时离开`AMS`,回到`Activity`的`startActivity`。

```
public void startActivity(Intent intent, @Nullable Bundle options) {
    if (options != null) {//走这里
        startActivityForResult(intent, -1, options);
    } else {
        // Note we want to go through this call for compatibility with
        // applications that may have overridden the method.
        startActivityForResult(intent, -1);
    }
}

public void startActivityForResult(@RequiresPermission Intent intent, int requestCode,
        @Nullable Bundle options) {
    if (mParent == null) {
        options = transferSpringboardActivityOptions(options);
        Instrumentation.ActivityResult ar =
        //调用Instrumentation的execStartActivity
            mInstrumentation.execStartActivity(
                this, mMainThread.getApplicationThread(), mToken, this,
                intent, requestCode, options);
        if (ar != null) {
            mMainThread.sendActivityResult(
                mToken, mEmbeddedID, requestCode, ar.getResultCode(),
                ar.getResultData());
        }
        if (requestCode >= 0) {
            mStartedActivity = true;
        }

        cancelInputsAndStartExitTransition(options);
    }
}


public ActivityResult execStartActivity(
        Context who, IBinder contextThread, IBinder token, Activity target,
        Intent intent, int requestCode, Bundle options) {
        //记录从哪里开启的
    IApplicationThread whoThread = (IApplicationThread) contextThread;
   //target是Launcher本身
    Uri referrer = target != null ? target.onProvideReferrer() : null;
    if (referrer != null) {
        intent.putExtra(Intent.EXTRA_REFERRER, referrer);
    }
    if (mActivityMonitors != null) {
        synchronized (mSync) {
            final int N = mActivityMonitors.size();
            for (int i=0; i<N; i++) {
                final ActivityMonitor am = mActivityMonitors.get(i);
                ActivityResult result = null;
                if (am.ignoreMatchingSpecificIntents()) {
                    result = am.onStartActivity(intent);
                }
                if (result != null) {
                    am.mHits++;
                    return result;
                } else if (am.match(who, null, intent)) {
                    am.mHits++;
                    if (am.isBlocking()) {
                        return requestCode >= 0 ? am.getResult() : null;
                    }
                    break;
                }
            }
        }
    }
    try {
        intent.migrateExtraStreamToClipData();
        intent.prepareToLeaveProcess(who);
        //在这里进行跨进程通信 调用ATMS的startActivity 注意这里是两次IPC通信 首先获取ATMS(BinderProxy) 然后调用ATMS的startActivity
        int result = ActivityTaskManager.getService()
            .startActivity(whoThread, who.getBasePackageName(), intent,
                    intent.resolveTypeIfNeeded(who.getContentResolver()),
                    token, target != null ? target.mEmbeddedID : null,
                    requestCode, 0, null, options);
        checkStartActivityResult(result, intent);
    } catch (RemoteException e) {
        throw new RuntimeException("Failure from system", e);
    }
    return null;
}

```

最终调用到`/frameworks/base/core/java/android/app/Instrumentation.java`的`execStartActivity`函数。在这里进行了两次IPC通信（首先获取ATMS，再通过BinderProxy调用startActivity）。 文件目录:`/frameworks/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java` 到了`ATMS`的`startActivity`

```
public final int startActivity(IApplicationThread caller, String callingPackage,
        Intent intent, String resolvedType, IBinder resultTo, String resultWho, int requestCode,
        int startFlags, ProfilerInfo profilerInfo, Bundle bOptions) {
    return startActivityAsUser(caller, callingPackage, intent, resolvedType, resultTo,
            resultWho, requestCode, startFlags, profilerInfo, bOptions,
            UserHandle.getCallingUserId());
}


public int startActivityAsUser(IApplicationThread caller, String callingPackage,
        Intent intent, String resolvedType, IBinder resultTo, String resultWho, int requestCode,
        int startFlags, ProfilerInfo profilerInfo, Bundle bOptions, int userId) {
    return startActivityAsUser(caller, callingPackage, intent, resolvedType, resultTo,
            resultWho, requestCode, startFlags, profilerInfo, bOptions, userId,
            true /*validateIncomingUser*/);
}


int startActivityAsUser(IApplicationThread caller, String callingPackage,
        Intent intent, String resolvedType, IBinder resultTo, String resultWho, int requestCode,
        int startFlags, ProfilerInfo profilerInfo, Bundle bOptions, int userId,
        boolean validateIncomingUser) {
    enforceNotIsolatedCaller("startActivityAsUser");

    userId = getActivityStartController().checkTargetUser(userId, validateIncomingUser,
            Binder.getCallingPid(), Binder.getCallingUid(), "startActivityAsUser");

    return getActivityStartController().obtainStarter(intent, "startActivityAsUser")
            .setCaller(caller)
            .setCallingPackage(callingPackage)
            .setResolvedType(resolvedType)
            .setResultTo(resultTo)
            .setResultWho(resultWho)
            .setRequestCode(requestCode)
            .setStartFlags(startFlags)
            .setProfilerInfo(profilerInfo)
            .setActivityOptions(bOptions)
            .setMayWait(userId)
            .execute();

}


int execute() {
        if (mRequest.mayWait) {//这里是true
            return startActivityMayWait(mRequest.caller, mRequest.callingUid,
                    mRequest.callingPackage, mRequest.realCallingPid, mRequest.realCallingUid,
                    mRequest.intent, mRequest.resolvedType,
                    mRequest.voiceSession, mRequest.voiceInteractor, mRequest.resultTo,
                    mRequest.resultWho, mRequest.requestCode, mRequest.startFlags,
                    mRequest.profilerInfo, mRequest.waitResult, mRequest.globalConfig,
                    mRequest.activityOptions, mRequest.ignoreTargetSecurity, mRequest.userId,
                    mRequest.inTask, mRequest.reason,
                    mRequest.allowPendingRemoteAnimationRegistryLookup,
                    mRequest.originatingPendingIntent, mRequest.allowBackgroundActivityStart);
        } else {
            return startActivity(mRequest.caller, mRequest.intent, mRequest.ephemeralIntent,
                    mRequest.resolvedType, mRequest.activityInfo, mRequest.resolveInfo,
                    mRequest.voiceSession, mRequest.voiceInteractor, mRequest.resultTo,
                    mRequest.resultWho, mRequest.requestCode, mRequest.callingPid,
                    mRequest.callingUid, mRequest.callingPackage, mRequest.realCallingPid,
                    mRequest.realCallingUid, mRequest.startFlags, mRequest.activityOptions,
                    mRequest.ignoreTargetSecurity, mRequest.componentSpecified,
                    mRequest.outActivity, mRequest.inTask, mRequest.reason,
                    mRequest.allowPendingRemoteAnimationRegistryLookup,
                    mRequest.originatingPendingIntent, mRequest.allowBackgroundActivityStart);
        }
  
}

//调用到mayWait
private int startActivityMayWait(IApplicationThread caller, int callingUid,
        String callingPackage, int requestRealCallingPid, int requestRealCallingUid,
        Intent intent, String resolvedType, IVoiceInteractionSession voiceSession,
        IVoiceInteractor voiceInteractor, IBinder resultTo, String resultWho, int requestCode,
        int startFlags, ProfilerInfo profilerInfo, WaitResult outResult,
        Configuration globalConfig, SafeActivityOptions options, boolean ignoreTargetSecurity,
        int userId, TaskRecord inTask, String reason,
        boolean allowPendingRemoteAnimationRegistryLookup,
        PendingIntentRecord originatingPendingIntent, boolean allowBackgroundActivityStart) {
  
    mSupervisor.getActivityMetricsLogger().notifyActivityLaunching(intent);
    
    //根据intent获取到需要启动的Activity信息
    ActivityInfo aInfo = mSupervisor.resolveActivity(intent, rInfo, startFlags, profilerInfo);
    synchronized (mService.mGlobalLock) {
        //创建ActivityRecord
        final ActivityRecord[] outRecord = new ActivityRecord[1];
        //调用startActivity
        int res = startActivity(caller, intent, ephemeralIntent, resolvedType, aInfo, rInfo,
                voiceSession, voiceInteractor, resultTo, resultWho, requestCode, callingPid,
                callingUid, callingPackage, realCallingPid, realCallingUid, startFlags, options,
                ignoreTargetSecurity, componentSpecified, outRecord, inTask, reason,
                allowPendingRemoteAnimationRegistryLookup, originatingPendingIntent,
                allowBackgroundActivityStart)

        return res;
    }
}


private int startActivity(IApplicationThread caller, Intent intent, Intent ephemeralIntent,
        String resolvedType, ActivityInfo aInfo, ResolveInfo rInfo,
        IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
        IBinder resultTo, String resultWho, int requestCode, int callingPid, int callingUid,
        String callingPackage, int realCallingPid, int realCallingUid, int startFlags,
        SafeActivityOptions options, boolean ignoreTargetSecurity, boolean componentSpecified,
        ActivityRecord[] outActivity, TaskRecord inTask, String reason,
        boolean allowPendingRemoteAnimationRegistryLookup,
        PendingIntentRecord originatingPendingIntent, boolean allowBackgroundActivityStart) {

    mLastStartReason = reason;
    mLastStartActivityTimeMs = System.currentTimeMillis();
    mLastStartActivityRecord[0] = null;
    //调用startActivity
    mLastStartActivityResult = startActivity(caller, intent, ephemeralIntent, resolvedType,
            aInfo, rInfo, voiceSession, voiceInteractor, resultTo, resultWho, requestCode,
            callingPid, callingUid, callingPackage, realCallingPid, realCallingUid, startFlags,
            options, ignoreTargetSecurity, componentSpecified, mLastStartActivityRecord,
            inTask, allowPendingRemoteAnimationRegistryLookup, originatingPendingIntent,
            allowBackgroundActivityStart);

    if (outActivity != null) {
        outActivity[0] = mLastStartActivityRecord[0];
    }

    return getExternalResult(mLastStartActivityResult);
}


private int startActivity(IApplicationThread caller, Intent intent, Intent ephemeralIntent,
        String resolvedType, ActivityInfo aInfo, ResolveInfo rInfo,
        IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
        IBinder resultTo, String resultWho, int requestCode, int callingPid, int callingUid,
        String callingPackage, int realCallingPid, int realCallingUid, int startFlags,
        SafeActivityOptions options,
        boolean ignoreTargetSecurity, boolean componentSpecified, ActivityRecord[] outActivity,
        TaskRecord inTask, boolean allowPendingRemoteAnimationRegistryLookup,
        PendingIntentRecord originatingPendingIntent, boolean allowBackgroundActivityStart) {
   
    int err = ActivityManager.START_SUCCESS;
   
    if (caller != null) {
    //获取到caller的进程信息 Launcher
        callerApp = mService.getProcessController(caller);
        if (callerApp != null) {
            callingPid = callerApp.getPid();
            callingUid = callerApp.mInfo.uid;
        } else {
            Slog.w(TAG, "Unable to find app for caller " + caller
                    + " (pid=" + callingPid + ") when starting: "
                    + intent.toString());
            err = ActivityManager.START_PERMISSION_DENIED;
        }
    }

    //检查调用者的相关权限
    boolean abort = !mSupervisor.checkStartAnyActivityPermission(intent, aInfo, resultWho,
            requestCode, callingPid, callingUid, callingPackage, ignoreTargetSecurity,
            inTask != null, callerApp, resultRecord, resultStack);
            //防火墙是否屏蔽了intent
    abort |= !mService.mIntentFirewall.checkStartActivity(intent, callingUid,
            callingPid, resolvedType, aInfo.applicationInfo);
    abort |= !mService.getPermissionPolicyInternal().checkStartActivity(intent, callingUid,
            callingPackage);

    boolean restrictedBgActivity = false;
    ActivityOptions checkedOptions = options != null
            ? options.getOptions(intent, aInfo, callerApp, mSupervisor) : null;
    if (allowPendingRemoteAnimationRegistryLookup) {
        checkedOptions = mService.getActivityStartController()
                .getPendingRemoteAnimationRegistry()
                .overrideOptionsIfNeeded(callingPackage, checkedOptions);
    }
    if (mService.mController != null) {
        try {
        //monkey相关的 返回是否需要拦截Activity的启动 return true
            Intent watchIntent = intent.cloneFilter();
            abort |= !mService.mController.activityStarting(watchIntent,
                    aInfo.applicationInfo.packageName);
        } catch (RemoteException e) {
            mService.mController = null;
        }
    }

    if (abort) {//需要拦截 直接退出
        return START_ABORTED;
    }

   //审核完成之后 开始启动
    //创建ActivityRecord
    ActivityRecord r = new ActivityRecord(mService, callerApp, callingPid, callingUid,
            callingPackage, intent, resolvedType, aInfo, mService.getGlobalConfiguration(),
            resultRecord, resultWho, requestCode, componentSpecified, voiceSession != null,
            mSupervisor, checkedOptions, sourceRecord);
    if (outActivity != null) {
        outActivity[0] = r;
    }

    if (r.appTimeTracker == null && sourceRecord != null) {
        r.appTimeTracker = sourceRecord.appTimeTracker;
    }
    //获取到当前的ActivityStack
    final ActivityStack stack = mRootActivityContainer.getTopDisplayFocusedStack();
    //检查是否可以进行切换
    if (voiceSession == null && (stack.getResumedActivity() == null
            || stack.getResumedActivity().info.applicationInfo.uid != realCallingUid)) {
        if (!mService.checkAppSwitchAllowedLocked(callingPid, callingUid,
                realCallingPid, realCallingUid, "Activity start")) {
            if (!(restrictedBgActivity && handleBackgroundActivityAbort(r))) {
            //如果不能切换进程 把Activity存入mPendingActivityLaunches中去
                mController.addPendingActivityLaunch(new PendingActivityLaunch(r,
                        sourceRecord, startFlags, stack, callerApp));
            }
            ActivityOptions.abort(checkedOptions);
            return ActivityManager.START_SWITCHES_CANCELED;
        }
    }
    mService.onStartActivitySetDidAppSwitch();
    mController.doPendingActivityLaunches(false);
    //调用startActivity
    final int res = startActivity(r, sourceRecord, voiceSession, voiceInteractor, startFlags,
            true /* doResume */, checkedOptions, inTask, outActivity, restrictedBgActivity);
    mSupervisor.getActivityMetricsLogger().notifyActivityLaunched(res, outActivity[0]);
    return res;
}


private int startActivity(final ActivityRecord r, ActivityRecord sourceRecord,
            IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
            int startFlags, boolean doResume, ActivityOptions options, TaskRecord inTask,
            ActivityRecord[] outActivity, boolean restrictedBgActivity) {
    int result = START_CANCELED;
    final ActivityStack startedActivityStack;
        mService.mWindowManager.deferSurfaceLayout();
        //调用startActivityUnchecked
        result = startActivityUnchecked(r, sourceRecord, voiceSession, voiceInteractor,
                startFlags, doResume, options, inTask, outActivity, restrictedBgActivity);
    return result;
}


private int startActivityUnchecked(final ActivityRecord r, ActivityRecord sourceRecord,
        IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
        int startFlags, boolean doResume, ActivityOptions options, TaskRecord inTask,
        ActivityRecord[] outActivity, boolean restrictedBgActivity) {

    
    //判断Activity 是否需要插入新的task
    //1.设置了new_task标签
    //2.启动模式是singleTask
    //3.启动模式是singleInstance
    ActivityRecord reusedActivity = getReusableIntentActivity();

  //如果正在启动的活动与当前在顶部的活动相同，那么我们需要检查它是否应该只启动一次。
    final ActivityStack topStack = mRootActivityContainer.getTopDisplayFocusedStack();
    final ActivityRecord topFocused = topStack.getTopActivity();
    final ActivityRecord top = topStack.topRunningNonDelayedActivityLocked(mNotTop);
    final boolean dontStart = top != null && mStartActivity.resultTo == null
            && top.mActivityComponent.equals(mStartActivity.mActivityComponent)
            && top.mUserId == mStartActivity.mUserId
            && top.attachedToProcess()
            && ((mLaunchFlags & FLAG_ACTIVITY_SINGLE_TOP) != 0
            || isLaunchModeOneOf(LAUNCH_SINGLE_TOP, LAUNCH_SINGLE_TASK))
            && (!top.isActivityTypeHome() || top.getDisplayId() == mPreferredDisplayId);
    if (dontStart) { //不需要启动
        topStack.mLastPausedActivity = null;
        if (mDoResume) {
            mRootActivityContainer.resumeFocusedStacksTopActivities();
        }
        ActivityOptions.abort(mOptions);
        if ((mStartFlags & START_FLAG_ONLY_IF_NEEDED) != 0) {
            return START_RETURN_INTENT_TO_CALLER;
        }
        //执行newIntent
        deliverNewIntent(top);

        mSupervisor.handleNonResizableTaskIfNeeded(top.getTaskRecord(), preferredWindowingMode,
                mPreferredDisplayId, topStack);

        return START_DELIVERED_TO_TOP;
    }

    boolean newTask = false;
    final TaskRecord taskToAffiliate = (mLaunchTaskBehind && mSourceRecord != null)
            ? mSourceRecord.getTaskRecord() : null;
    int result = START_SUCCESS;
    //判断是否需要一个新的task
    if (mStartActivity.resultTo == null && mInTask == null && !mAddingToTask
            && (mLaunchFlags & FLAG_ACTIVITY_NEW_TASK) != 0) {
        newTask = true;
        result = setTaskFromReuseOrCreateNewTask(taskToAffiliate);
    } else if (mSourceRecord != null) {
        result = setTaskFromSourceRecord();
    } else if (mInTask != null) {
        result = setTaskFromInTask();
    } else {
        result = setTaskToCurrentTopOrCreateNewTask();
    }
    if (result != START_SUCCESS) {
        return result;
    }
    //开启activity 主要是把ActivityReecord对象加入task的顶部，把task加入到mTaskHistory顶部
    mTargetStack.startActivityLocked(mStartActivity, topFocused, newTask, mKeepCurTransition,
            mOptions);
    if (mDoResume) {
        final ActivityRecord topTaskActivity =
                mStartActivity.getTaskRecord().topRunningActivityLocked();
        if (!mTargetStack.isFocusable()
                || (topTaskActivity != null && topTaskActivity.mTaskOverlay
                && mStartActivity != topTaskActivity)) {//没有焦点，无法resume 执行动画
            mTargetStack.ensureActivitiesVisibleLocked(mStartActivity, 0, !PRESERVE_WINDOWS);
            //执行动画
            mTargetStack.getDisplay().mDisplayContent.executeAppTransition();
        } else {//不可见
            if (mTargetStack.isFocusable()
                    && !mRootActivityContainer.isTopDisplayFocusedStack(mTargetStack)) {
                mTargetStack.moveToFront("startActivityUnchecked");
            }
            mRootActivityContainer.resumeFocusedStacksTopActivities(
                    mTargetStack, mStartActivity, mOptions);
        }
    } else if (mStartActivity != null) {
        mSupervisor.mRecentTasks.add(mStartActivity.getTaskRecord());
    }

    return START_SUCCESS;
}

void startActivityLocked(ActivityRecord r, ActivityRecord focusedTopActivity,
        boolean newTask, boolean keepCurTransition, ActivityOptions options) {
    if (!r.mLaunchTaskBehind && (taskForIdLocked(taskId) == null || newTask)) {
        insertTaskAtTop(rTask, r);
    }
}

//调用resumeFocusedStacksTopActivities
boolean resumeFocusedStacksTopActivities(
        ActivityStack targetStack, ActivityRecord target, ActivityOptions targetOptions) {

    if (!mStackSupervisor.readyToResume()) {
        return false;
    }

    boolean result = false;
    //判断目标ActivityStack是否处于焦点，是的话调用resumeTopActivityUnckedLocked进行进一步启动流程
    if (targetStack != null && (targetStack.isTopStackOnDisplay()
            || getTopDisplayFocusedStack() == targetStack)) {
        result = targetStack.resumeTopActivityUncheckedLocked(target, targetOptions);
    }
    //遍历所有显示的activityDisplays
    for (int displayNdx = mActivityDisplays.size() - 1; displayNdx >= 0; --displayNdx) {
        boolean resumedOnDisplay = false;
        final ActivityDisplay display = mActivityDisplays.get(displayNdx);
        for (int stackNdx = display.getChildCount() - 1; stackNdx >= 0; --stackNdx) {
            //获取到activityStack
            final ActivityStack stack = display.getChildAt(stackNdx);
            //获取到topActivity
            final ActivityRecord topRunningActivity = stack.topRunningActivityLocked();
            if (!stack.isFocusableAndVisible() || topRunningActivity == null) {
                continue;
            }
            if (stack == targetStack) {
                resumedOnDisplay |= result;
                continue;
            }
            if (display.isTopStack(stack) && topRunningActivity.isState(RESUMED)) {
                stack.executeAppTransition(targetOptions);//执行动画
            } else {
                resumedOnDisplay |= topRunningActivity.makeActiveIfNeeded(target);
            }
        }
        if (!resumedOnDisplay) {//没有被激活
            final ActivityStack focusedStack = display.getFocusedStack();
            if (focusedStack != null) {
                focusedStack.resumeTopActivityUncheckedLocked(target, targetOptions);
            }
        }
    }

    return result;
}


boolean resumeTopActivityUncheckedLocked(ActivityRecord prev, ActivityOptions options) {
//mInResumeTopActivity 表示当前处于resumeTopActivity过程中，避免重复递归调用
    if (mInResumeTopActivity) {
        return false;
    }

    boolean result = false;
    try {
        mInResumeTopActivity = true;
        //在这里真正开启Activity
        result = resumeTopActivityInnerLocked(prev, options);
        final ActivityRecord next = topRunningActivityLocked(true /* focusableOnly */);
        if (next == null || !next.canTurnScreenOn()) {
            checkReadyForSleep();
        }
    } finally {
        mInResumeTopActivity = false;
    }

    return result;
}


private boolean resumeTopActivityInnerLocked(ActivityRecord prev, ActivityOptions options) {
    if (!mService.isBooting() && !mService.isBooted()) {//AMS还没开启成功
        return false;
    }
    //获取到将要显示的Activity
    ActivityRecord next = topRunningActivityLocked(true /* focusableOnly */);

    final boolean hasRunningActivity = next != null;
    //判断是否没有Attached
    if (hasRunningActivity && !isAttached()) {
        return false;
    }
//清理AppWindowToekn
    mRootActivityContainer.cancelInitializingActivities();
    //标记是否调用Activity的performUserLeaving回调 要离开的回调
    boolean userLeaving = mStackSupervisor.mUserLeaving;
    mStackSupervisor.mUserLeaving = false;

    if (!hasRunningActivity) {
        return resumeNextFocusableActivityWhenStackIsEmpty(prev, options);
    }
    //是否延迟显示
    next.delayedResume = false;
    final ActivityDisplay display = getDisplay();

    mStackSupervisor.mStoppingActivities.remove(next);
    mStackSupervisor.mGoingToSleepActivities.remove(next);
    next.sleeping = false;
    //判断是否有没暂停的activity
    if (!mRootActivityContainer.allPausedActivitiesComplete()) {
        return false;
    }
    //用于应用耗电统计 wake lock关联的WorkSource
    mStackSupervisor.setLaunchSource(next.info.applicationInfo.uid);
    //pause 非当前焦点的ActivityStack中的Activity。
    boolean pausing = getDisplay().pauseBackStacks(userLeaving, next, false);
    //mResumedActivity 表示的是当前正在显示的Activity 也就是Launcher
    if (mResumedActivity != null) {
    //pause launcher
        pausing |= startPausingLocked(userLeaving, false, next, false);
    }
     // Whoops, need to restart this activity!
     if (!next.hasBeenLaunched) {//没启动
            next.hasBeenLaunched = true;
        } else {//重启
            if (SHOW_APP_STARTING_PREVIEW) {
                next.showStartingWindow(null /* prev */, false /* newTask */,
                        false /* taskSwich */);
            }
            if (DEBUG_SWITCH) Slog.v(TAG_SWITCH, "Restarting: " + next);
        }
        if (DEBUG_STATES) Slog.d(TAG_STATES, "resumeTopActivityLocked: Restarting " + next);
        mStackSupervisor.startSpecificActivityLocked(next, true, true);
   

    return true;
}




```

通过`PMS`查询到需要开启的activity 以及应用信息，最终会调到`ActivityStarter`的`startActivity`,判断各种flag，权限，把需要开启的`Activity`调整到`task`顶部，把task调整到`HistoryTasks`的顶部。暂停当前的显示界面，再判断如果需要启动的`Activity`已经显示了，直接调用`resume`的流程，否则调用`startSpecificActivityLocked`进入开启流程。

```
void startSpecificActivityLocked(ActivityRecord r, boolean andResume, boolean checkConfig) {
    // Is this activity's application already running?
    final WindowProcessController wpc =
            mService.getProcessController(r.processName, r.info.applicationInfo.uid);

    boolean knownToBeDead = false;
    if (wpc != null && wpc.hasThread()) {//应用进程已经存在
        try {
        //直接开启Activity
            realStartActivityLocked(r, wpc, andResume, checkConfig);
            return;
        } catch (RemoteException e) {
            Slog.w(TAG, "Exception when starting activity "
                    + r.intent.getComponent().flattenToShortString(), e);
        }

        knownToBeDead = true;
    }

    if (getKeyguardController().isKeyguardLocked()) {
        r.notifyUnknownVisibilityLaunched();
    }

        //启动进程
        final Message msg = PooledLambda.obtainMessage(
                ActivityManagerInternal::startProcess, mService.mAmInternal, r.processName,
                r.info.applicationInfo, knownToBeDead, "activity", r.intent.getComponent());
        mService.mH.sendMessage(msg);
  
}
```

我们从Launcher点击过来是没有开启进程的，所以我们需要先开启进程。终于到了进程的启动了，上面的逻辑代码判断比较多，感兴趣的，可以自己去跟踪下，我就先介绍启动流程了。走到了`startProcess`。

```
//这里的processName就是包名 applicationInfo就是Application
public void startProcess(String processName, ApplicationInfo info,
        boolean knownToBeDead, String hostingType, ComponentName hostingName) {
        synchronized (ActivityManagerService.this) {
            startProcessLocked(processName, info, knownToBeDead, 0 /* intentFlags */,
                    new HostingRecord(hostingType, hostingName),
                    false /* allowWhileBooting */, false /* isolated */,
                    true /* keepIfLarge */);
      }
}



//isolated = false 表示没启动过
final ProcessRecord startProcessLocked(String processName,
        ApplicationInfo info, boolean knownToBeDead, int intentFlags,
        HostingRecord hostingRecord, boolean allowWhileBooting,
        boolean isolated, boolean keepIfLarge) {
    return mProcessList.startProcessLocked(processName, info, knownToBeDead, intentFlags,
            hostingRecord, allowWhileBooting, isolated, 0 /* isolatedUid */, keepIfLarge,
            null /* ABI override */, null /* entryPoint */, null /* entryPointArgs */,
            null /* crashHandler */);
}


文件目录:/frameworks/base/services/core/java/com/android/server/am/ProcessList.java


final ProcessRecord startProcessLocked(String processName, ApplicationInfo info,
        boolean knownToBeDead, int intentFlags, HostingRecord hostingRecord,
        boolean allowWhileBooting, boolean isolated, int isolatedUid, boolean keepIfLarge,
        String abiOverride, String entryPoint, String[] entryPointArgs, Runnable crashHandler) {
   
    if (app == null) {
       //创建ProcessRecord 对象
        app = newProcessRecordLocked(info, processName, isolated, isolatedUid, hostingRecord);
        if (app == null) {
            return null;
        }
        app.crashHandler = crashHandler;
        app.isolatedEntryPoint = entryPoint;
        app.isolatedEntryPointArgs = entryPointArgs;
        checkSlow(startTime, "startProcess: done creating new process record");
    }
    //调用 startProcessLocked开启进程
    final boolean success = startProcessLocked(app, hostingRecord, abiOverride);
    return success ? app : null;
}

boolean startProcessLocked(ProcessRecord app, HostingRecord hostingRecord,
        boolean disableHiddenApiChecks, boolean mountExtStorageFull,
        String abiOverride) {
    if (app.pendingStart) {
        return true;
    }
    long startTime = SystemClock.elapsedRealtime();
    if (app.pid > 0 && app.pid != ActivityManagerService.MY_PID) {
    //从集合中移除进程的id，避免重复创建
        mService.mPidsSelfLocked.remove(app);
        //移除消息队列的PROC_START_TIMEOUT_MSG消息
        mService.mHandler.removeMessages(PROC_START_TIMEOUT_MSG, app);
        app.setPid(0);
        app.startSeq = 0;
    }


    mService.mProcessesOnHold.remove(app);
        //设置entryPoint 为"android.app.ActivityThread"
    final String entryPoint = "android.app.ActivityThread";
    //调用startProcessLocked
    return startProcessLocked(hostingRecord, entryPoint, app, uid, gids,
                runtimeFlags, mountExternal, seInfo, requiredAbi, instructionSet, 
}


boolean startProcessLocked(HostingRecord hostingRecord,
        String entryPoint,
        ProcessRecord app, int uid, int[] gids, int runtimeFlags, int mountExternal,
        String seInfo, String requiredAbi, String instructionSet, String invokeWith,
        long startTime) {
    app.pendingStart = true;
    app.killedByAm = false;
    app.removed = false;
    app.killed = false;
    final long startSeq = app.startSeq = ++mProcStartSeqCounter;
    app.setStartParams(uid, hostingRecord, seInfo, startTime);
    app.setUsingWrapper(invokeWith != null
            || SystemProperties.get("wrap." + app.processName) != null);
    mPendingStarts.put(startSeq, app);

    if (mService.mConstants.FLAG_PROCESS_START_ASYNC) {//异步启动
        if (DEBUG_PROCESSES) Slog.i(TAG_PROCESSES,
                "Posting procStart msg for " + app.toShortString());
        mService.mProcStartHandler.post(() -> {
            try {
                final Process.ProcessStartResult startResult = startProcess(app.hostingRecord,
                        entryPoint, app, app.startUid, gids, runtimeFlags, mountExternal,
                        app.seInfo, requiredAbi, instructionSet, invokeWith, app.startTime);
                synchronized (mService) {
                //发送一个延时消息 用来监听应用启动
                    handleProcessStartedLocked(app, startResult, startSeq);
                }
            } catch (RuntimeException e) {
                synchronized (mService) {
                    Slog.e(ActivityManagerService.TAG, "Failure starting process "
                            + app.processName, e);
                    mPendingStarts.remove(startSeq);
                    app.pendingStart = false;
                    mService.forceStopPackageLocked(app.info.packageName,
                            UserHandle.getAppId(app.uid),
                            false, false, true, false, false, app.userId, "start failure");
                }
            }
        });
        return true;
    } else {
        try {
        //启动应用
            final Process.ProcessStartResult startResult = startProcess(hostingRecord,
                    entryPoint, app,
                    uid, gids, runtimeFlags, mountExternal, seInfo, requiredAbi, instructionSet,
                    invokeWith, startTime);
                    //发送一个延时消息，来监听应用启动
            handleProcessStartedLocked(app, startResult.pid, startResult.usingWrapper,
                    startSeq, false);
        } catch (RuntimeException e) {
            Slog.e(ActivityManagerService.TAG, "Failure starting process "
                    + app.processName, e);
            app.pendingStart = false;
            mService.forceStopPackageLocked(app.info.packageName, UserHandle.getAppId(app.uid),
                    false, false, true, false, false, app.userId, "start failure");
        }
        return app.pid > 0;
    }
}

private Process.ProcessStartResult startProcess(HostingRecord hostingRecord, String entryPoint,
        ProcessRecord app, int uid, int[] gids, int runtimeFlags, int mountExternal,
        String seInfo, String requiredAbi, String instructionSet, String invokeWith,
        long startTime) {
        final Process.ProcessStartResult startResult;
          //我们是到这里来 创建AppZygote 调用start函数来开启
        final AppZygote appZygote = createAppZygoteForProcessIfNeeded(app);
        startResult = appZygote.getProcess().start(entryPoint,
                    app.processName, uid, uid, gids, runtimeFlags, mountExternal,
                    app.info.targetSdkVersion, seInfo, requiredAbi, instructionSet,
                    app.info.dataDir, null, app.info.packageName,
                    /*useUsapPool=*/ false,
                    new String[] {PROC_START_SEQ_IDENT + app.startSeq});
        return startResult;
}


//看看如何创建AppZygote的
private AppZygote createAppZygoteForProcessIfNeeded(final ProcessRecord app) {
    synchronized (mService) {
    //获取到uid
        final int uid = app.hostingRecord.getDefiningUid();
        //这里是拿不到的 默认是没有的
        AppZygote appZygote = mAppZygotes.get(app.info.processName, uid);
        final ArrayList<ProcessRecord> zygoteProcessList;
        if (appZygote == null) {
            final IsolatedUidRange uidRange =
                    mAppIsolatedUidRangeAllocator.getIsolatedUidRangeLocked(
                            app.info.processName, app.hostingRecord.getDefiningUid());
            final int userId = UserHandle.getUserId(uid);
            final int firstUid = UserHandle.getUid(userId, uidRange.mFirstUid);
            final int lastUid = UserHandle.getUid(userId, uidRange.mLastUid);
            ApplicationInfo appInfo = new ApplicationInfo(app.info);
            appInfo.packageName = app.hostingRecord.getDefiningPackageName();
            appInfo.uid = uid;
            //创建AppZygote 存入mAppZygotes中
            appZygote = new AppZygote(appInfo, uid, firstUid, lastUid);
            mAppZygotes.put(app.info.processName, uid, appZygote);
            zygoteProcessList = new ArrayList<ProcessRecord>();
            mAppZygoteProcesses.put(appZygote, zygoteProcessList);
        } else {
            mService.mHandler.removeMessages(KILL_APP_ZYGOTE_MSG, appZygote);
            zygoteProcessList = mAppZygoteProcesses.get(appZygote);
        }
        zygoteProcessList.add(app);

        return appZygote;
    }
}

//看看他的start函数

public final Process.ProcessStartResult start(@NonNull final String processClass,
                                              final String niceName,
                                              int uid, int gid, @Nullable int[] gids,
                                              int runtimeFlags, int mountExternal,
                                              int targetSdkVersion,
                                              @Nullable String seInfo,
                                              @NonNull String abi,
                                              @Nullable String instructionSet,
                                              @Nullable String appDataDir,
                                              @Nullable String invokeWith,
                                              @Nullable String packageName,
                                              boolean useUsapPool,
                                              @Nullable String[] zygoteArgs) {
    if (fetchUsapPoolEnabledPropWithMinInterval()) {
        informZygotesOfUsapPoolStatus();
    }

    try {
    //调用startViaZygote
        return startViaZygote(processClass, niceName, uid, gid, gids,
                runtimeFlags, mountExternal, targetSdkVersion, seInfo,
                abi, instructionSet, appDataDir, invokeWith, /*startChildZygote=*/ false,
                packageName, useUsapPool, zygoteArgs);
    } catch (ZygoteStartFailedEx ex) {
        Log.e(LOG_TAG,
                "Starting VM process through Zygote failed");
        throw new RuntimeException(
                "Starting VM process through Zygote failed", ex);
    }
}
 
//到了开启进程的地方了 processClass就是我们传入的ActivityThread
private Process.ProcessStartResult startViaZygote(@NonNull final String processClass,
                                                  @Nullable final String niceName,
                                                  final int uid, final int gid,
                                                  @Nullable final int[] gids,
                                                  int runtimeFlags, int mountExternal,
                                                  int targetSdkVersion,
                                                  @Nullable String seInfo,
                                                  @NonNull String abi,
                                                  @Nullable String instructionSet,
                                                  @Nullable String appDataDir,
                                                  @Nullable String invokeWith,
                                                  boolean startChildZygote,
                                                  @Nullable String packageName,
                                                  boolean useUsapPool,
                                                  @Nullable String[] extraArgs)
                                                  throws ZygoteStartFailedEx {
    ArrayList<String> argsForZygote = new ArrayList<>();

    argsForZygote.add("--runtime-args");
    argsForZygote.add("--setuid=" + uid);
    argsForZygote.add("--setgid=" + gid);
    argsForZygote.add("--runtime-flags=" + runtimeFlags);
    if (mountExternal == Zygote.MOUNT_EXTERNAL_DEFAULT) {
        argsForZygote.add("--mount-external-default");
    } else if (mountExternal == Zygote.MOUNT_EXTERNAL_READ) {
        argsForZygote.add("--mount-external-read");
    } else if (mountExternal == Zygote.MOUNT_EXTERNAL_WRITE) {
        argsForZygote.add("--mount-external-write");
    } else if (mountExternal == Zygote.MOUNT_EXTERNAL_FULL) {
        argsForZygote.add("--mount-external-full");
    } else if (mountExternal == Zygote.MOUNT_EXTERNAL_INSTALLER) {
        argsForZygote.add("--mount-external-installer");
    } else if (mountExternal == Zygote.MOUNT_EXTERNAL_LEGACY) {
        argsForZygote.add("--mount-external-legacy");
    }

    argsForZygote.add("--target-sdk-version=" + targetSdkVersion);

    if (gids != null && gids.length > 0) {
        StringBuilder sb = new StringBuilder();
        sb.append("--setgroups=");

        int sz = gids.length;
        for (int i = 0; i < sz; i++) {
            if (i != 0) {
                sb.append(',');
            }
            sb.append(gids[i]);
        }

        argsForZygote.add(sb.toString());
    }

    if (niceName != null) {
        argsForZygote.add("--nice-name=" + niceName);
    }

    if (seInfo != null) {
        argsForZygote.add("--seinfo=" + seInfo);
    }

    if (instructionSet != null) {
        argsForZygote.add("--instruction-set=" + instructionSet);
    }

    if (appDataDir != null) {
        argsForZygote.add("--app-data-dir=" + appDataDir);
    }

    if (invokeWith != null) {
        argsForZygote.add("--invoke-with");
        argsForZygote.add(invokeWith);
    }

    if (startChildZygote) {
        argsForZygote.add("--start-child-zygote");
    }

    if (packageName != null) {
        argsForZygote.add("--package-name=" + packageName);
    }

    argsForZygote.add(processClass);

    if (extraArgs != null) {
        Collections.addAll(argsForZygote, extraArgs);
    }

    synchronized(mLock) {
        //开始和Zygote通信
        return zygoteSendArgsAndGetResult(openZygoteSocketIfNeeded(abi),
                                          useUsapPool,
                                          argsForZygote);
    }
}


//打开Zygote的socket
private ZygoteState openZygoteSocketIfNeeded(String abi) throws ZygoteStartFailedEx {
    try {
        attemptConnectionToPrimaryZygote();

        if (primaryZygoteState.matches(abi)) {
            return primaryZygoteState;
        }

        if (mZygoteSecondarySocketAddress != null) {
            // The primary zygote didn't match. Try the secondary.
            attemptConnectionToSecondaryZygote();

            if (secondaryZygoteState.matches(abi)) {
                return secondaryZygoteState;
            }
        }
    } catch (IOException ioe) {
        throw new ZygoteStartFailedEx("Error connecting to zygote", ioe);
    }

    throw new ZygoteStartFailedEx("Unsupported zygote ABI: " + abi);
}

private void attemptConnectionToPrimaryZygote() throws IOException {
    if (primaryZygoteState == null || primaryZygoteState.isClosed()) {
    //进行zygote连接 address = zygote 创建zygote的时候命名就是zygote 包装成ZygoteState
        primaryZygoteState =
                ZygoteState.connect(mZygoteSocketAddress, mUsapPoolSocketAddress);
    
        maybeSetApiBlacklistExemptions(primaryZygoteState, false);
        maybeSetHiddenApiAccessLogSampleRate(primaryZygoteState);
        maybeSetHiddenApiAccessStatslogSampleRate(primaryZygoteState);
    }
}


//准备参数
private Process.ProcessStartResult zygoteSendArgsAndGetResult(
        ZygoteState zygoteState, boolean useUsapPool, @NonNull ArrayList<String> args)
        throws ZygoteStartFailedEx {
    String msgStr = args.size() + "\n" + String.join("\n", args) + "\n";

    if (useUsapPool && mUsapPoolEnabled && canAttemptUsap(args)) {//这里是false
        try {
            return attemptUsapSendArgsAndGetResult(zygoteState, msgStr);
        } catch (IOException ex) {
            // If there was an IOException using the USAP pool we will log the error and
            // attempt to start the process through the Zygote.
            Log.e(LOG_TAG, "IO Exception while communicating with USAP pool - "
                    + ex.getMessage());
        }
    }

    return attemptZygoteSendArgsAndGetResult(zygoteState, msgStr);
}


private Process.ProcessStartResult attemptZygoteSendArgsAndGetResult(
        ZygoteState zygoteState, String msgStr) throws ZygoteStartFailedEx {
        final BufferedWriter zygoteWriter = zygoteState.mZygoteOutputWriter;
        final DataInputStream zygoteInputStream = zygoteState.mZygoteInputStream;
        //参数写入
        zygoteWriter.write(msgStr);
        zygoteWriter.flush();
         //创建result对象
        Process.ProcessStartResult result = new Process.ProcessStartResult();
        result.pid = zygoteInputStream.readInt();//获取返回的pid
        result.usingWrapper = zygoteInputStream.readBoolean();
        return result;
   
}

```

`AMS`、`ATMS`的准备工作还是蛮多的，这里主要也贴出了Activity的准备工作，但是我没还没有开启进程，所以我们先看开启进程的，调用到Process的start函数，最终调用到`attemptZygoteSendArgsAndGetResult`,给Zygote发送数据。

### 2.Zygote接收到数据之后的处理

让我们回到Zygote中，文件为`ZygoteServer.java`的`runSelectLoop`中。

```
Runnable runSelectLoop(String abiList) {
    ArrayList<FileDescriptor> socketFDs = new ArrayList<FileDescriptor>();
    ArrayList<ZygoteConnection> peers = new ArrayList<ZygoteConnection>();

    socketFDs.add(mZygoteSocket.getFileDescriptor());
    peers.add(null);

    while (true) {
        fetchUsapPoolPolicyPropsWithMinInterval();

        int[] usapPipeFDs = null;
        StructPollfd[] pollFDs = null;//需要监听的fds

        if (mUsapPoolEnabled) {
            usapPipeFDs = Zygote.getUsapPipeFDs();
            pollFDs = new StructPollfd[socketFDs.size() + 1 + usapPipeFDs.length];
        } else {
            pollFDs = new StructPollfd[socketFDs.size()];
        }


        int pollIndex = 0;
        for (FileDescriptor socketFD : socketFDs) {
            pollFDs[pollIndex] = new StructPollfd();
            pollFDs[pollIndex].fd = socketFD;
            pollFDs[pollIndex].events = (short) POLLIN;//对每一个socket需要关注POLLIN
            ++pollIndex;
        }

        final int usapPoolEventFDIndex = pollIndex;

        if (mUsapPoolEnabled) {
            pollFDs[pollIndex] = new StructPollfd();
            pollFDs[pollIndex].fd = mUsapPoolEventFD;
            pollFDs[pollIndex].events = (short) POLLIN;
            ++pollIndex;

            for (int usapPipeFD : usapPipeFDs) {
                FileDescriptor managedFd = new FileDescriptor();
                managedFd.setInt$(usapPipeFD);

                pollFDs[pollIndex] = new StructPollfd();
                pollFDs[pollIndex].fd = managedFd;
                pollFDs[pollIndex].events = (short) POLLIN;
                ++pollIndex;
            }
        }

        try {//zygote阻塞在这里等待poll事件
            Os.poll(pollFDs, -1);
        } catch (ErrnoException ex) {
            throw new RuntimeException("poll failed", ex);
        }

        boolean usapPoolFDRead = false;
        //注意这里是采用的倒序方式的，也就是说优先处理已经建立连接的请求，后处理新建立链接的请求
        while (--pollIndex >= 0) {
            if ((pollFDs[pollIndex].revents & POLLIN) == 0) {
                continue;
            }

            if (pollIndex == 0) {//初始化的时候socketFDs中只有server socket。所以会创建新的通信连接调用acceptCommandPeer
                // Zygote server socket

                ZygoteConnection newPeer = acceptCommandPeer(abiList);
                peers.add(newPeer);
                socketFDs.add(newPeer.getFileDescriptor());

            } else if (pollIndex < usapPoolEventFDIndex) {
                // Session socket accepted from the Zygote server socket

                try {//获取到客户端链接
                    ZygoteConnection connection = peers.get(pollIndex);
                    //客户端有请求进行处理
                    final Runnable command = connection.processOneCommand(this);

                    if (mIsForkChild) {
                        if (command == null) {
                            throw new IllegalStateException("command == null");
                        }

                        return command;
                    } else {
                        if (command != null) {
                            throw new IllegalStateException("command != null");
                        }

                        // We don't know whether the remote side of the socket was closed or
                        // not until we attempt to read from it from processOneCommand. This
                        // shows up as a regular POLLIN event in our regular processing loop.
                        if (connection.isClosedByPeer()) {
                            connection.closeSocket();
                            peers.remove(pollIndex);
                            socketFDs.remove(pollIndex);
                        }
                    }
                } catch (Exception e) {
                    if (!mIsForkChild) {

                        Slog.e(TAG, "Exception executing zygote command: ", e);

                        ZygoteConnection conn = peers.remove(pollIndex);
                        conn.closeSocket();

                        socketFDs.remove(pollIndex);
                    } else {
                        Log.e(TAG, "Caught post-fork exception in child process.", e);
                        throw e;
                    }
                } finally {
                    mIsForkChild = false;
                }
            } else {
                long messagePayload = -1;

                try {
                    byte[] buffer = new byte[Zygote.USAP_MANAGEMENT_MESSAGE_BYTES];
                    int readBytes = Os.read(pollFDs[pollIndex].fd, buffer, 0, buffer.length);

                    if (readBytes == Zygote.USAP_MANAGEMENT_MESSAGE_BYTES) {
                        DataInputStream inputStream =
                                new DataInputStream(new ByteArrayInputStream(buffer));

                        messagePayload = inputStream.readLong();
                    } else {
                        Log.e(TAG, "Incomplete read from USAP management FD of size "
                                + readBytes);
                        continue;
                    }
                } catch (Exception ex) {
                    if (pollIndex == usapPoolEventFDIndex) {
                        Log.e(TAG, "Failed to read from USAP pool event FD: "
                                + ex.getMessage());
                    } else {
                        Log.e(TAG, "Failed to read from USAP reporting pipe: "
                                + ex.getMessage());
                    }

                    continue;
                }

                if (pollIndex > usapPoolEventFDIndex) {
                    Zygote.removeUsapTableEntry((int) messagePayload);
                }

                usapPoolFDRead = true;
            }
        }

        if (usapPoolFDRead) {
            int[] sessionSocketRawFDs =
                    socketFDs.subList(1, socketFDs.size())
                            .stream()
                            .mapToInt(fd -> fd.getInt$())
                            .toArray();

            final Runnable command = fillUsapPool(sessionSocketRawFDs);

            if (command != null) {
                return command;
            }
        }
    }
}

```

进入 `processOneCommand`处理客户端的请求.

```
Runnable processOneCommand(ZygoteServer zygoteServer) {
        String args[];
        ZygoteArguments parsedArgs = null;
        FileDescriptor[] descriptors;

            args = Zygote.readArgumentList(mSocketReader);

            descriptors = mSocket.getAncillaryFileDescriptors();
  

        if (args == null) {
            isEof = true;
            return null;
        }

        int pid = -1;
        FileDescriptor childPipeFd = null;
        FileDescriptor serverPipeFd = null;
        //解析传递过来的参数
        parsedArgs = new ZygoteArguments(args);
        
        //调用native的fork
        pid = Zygote.forkAndSpecialize(parsedArgs.mUid, parsedArgs.mGid, parsedArgs.mGids,
                parsedArgs.mRuntimeFlags, rlimits, parsedArgs.mMountExternal, parsedArgs.mSeInfo,
                parsedArgs.mNiceName, fdsToClose, fdsToIgnore, parsedArgs.mStartChildZygote,
                parsedArgs.mInstructionSet, parsedArgs.mAppDataDir, parsedArgs.mTargetSdkVersion);

        try {
            if (pid == 0) {
                // 在子进程中 首先关闭掉ServerSocket，因为它是Zygote孵化出来的 所以socket 需要关闭掉。
                zygoteServer.setForkChild();

                zygoteServer.closeServerSocket();
                IoUtils.closeQuietly(serverPipeFd);
                serverPipeFd = null;
                //调用HandleChildProc mStartChildZygote = --start-child-zygote
                return handleChildProc(parsedArgs, descriptors, childPipeFd,
                        parsedArgs.mStartChildZygote);
            } else {
                // In the parent. A pid < 0 indicates a failure and will be handled in
                // handleParentProc.
                IoUtils.closeQuietly(childPipeFd);
                childPipeFd = null;
                handleParentProc(pid, descriptors, serverPipeFd);
                return null;
            }
        } finally {
            IoUtils.closeQuietly(childPipeFd);
            IoUtils.closeQuietly(serverPipeFd);
        }
    }



static jint com_android_internal_os_Zygote_nativeForkAndSpecialize(
        JNIEnv* env, jclass, jint uid, jint gid, jintArray gids,
        jint runtime_flags, jobjectArray rlimits,
        jint mount_external, jstring se_info, jstring nice_name,
        jintArray managed_fds_to_close, jintArray managed_fds_to_ignore, jboolean is_child_zygote,
        jstring instruction_set, jstring app_data_dir) {
    ……………………
    pid_t pid = ForkCommon(env, false, fds_to_close, fds_to_ignore);

    if (pid == 0) {
      SpecializeCommon(env, uid, gid, gids, runtime_flags, rlimits,
                       capabilities, capabilities,
                       mount_external, se_info, nice_name, false,
                       is_child_zygote == JNI_TRUE, instruction_set, app_data_dir);
    }
    return pid;
}

到native层fork进程
static jint com_android_internal_os_Zygote_nativeForkAndSpecialize(
        JNIEnv* env, jclass, jint uid, jint gid, jintArray gids,
        jint runtime_flags, jobjectArray rlimits,
        jint mount_external, jstring se_info, jstring nice_name,
        jintArray managed_fds_to_close, jintArray managed_fds_to_ignore, jboolean is_child_zygote,
        jstring instruction_set, jstring app_data_dir) {
    ……………………
    pid_t pid = ForkCommon(env, false, fds_to_close, fds_to_ignore);

    if (pid == 0) {
      SpecializeCommon(env, uid, gid, gids, runtime_flags, rlimits,
                       capabilities, capabilities,
                       mount_external, se_info, nice_name, false,
                       is_child_zygote == JNI_TRUE, instruction_set, app_data_dir);
    }
    return pid;
}



static pid_t ForkCommon(JNIEnv* env, bool is_system_server,
                        const std::vector<int>& fds_to_close,
                        const std::vector<int>& fds_to_ignore) {
  SetSignalHandlers();
  ……………………
  pid_t pid = fork();//fork出来客户端请求的进程。
  if (pid == 0) {
    PreApplicationInit();
    DetachDescriptors(env, fds_to_close, fail_fn);
    ClearUsapTable();
    gOpenFdTable->ReopenOrDetach(fail_fn);
    android_fdsan_set_error_level(fdsan_error_level);
  } else {
    ALOGD("Forked child process %d", pid);
  }
后边流程同Zygote我就省略了 可以参考我之前写的Zygote 
```

`Zygote`fork出了子进程，然后调用了`onZygoteInit`初始化了`ProcessState`打开了binder等等。这里传入的processClass 是`ActivityThread`，所以会进入到`ActivityThread`的`main`函数。这里的`ActivityThread`就是我们应用进程的了。

### 3.应用进程的初始化

文件目录:`/frameworks/base/core/java/android/app/ActivityThread.java`

```
public static void main(String[] args) {
    AndroidOs.install();
    CloseGuard.setEnabled(false);

    Environment.initForCurrentUser();
    TrustedCertificateStore.setDefaultUserDirectory(configDir);

    Process.setArgV0("<pre-initialized>");
    //准备MainLooper
    Looper.prepareMainLooper();

    long startSeq = 0;
    if (args != null) {
        for (int i = args.length - 1; i >= 0; --i) {
            if (args[i] != null && args[i].startsWith(PROC_START_SEQ_IDENT)) {
                startSeq = Long.parseLong(
                        args[i].substring(PROC_START_SEQ_IDENT.length()));
            }
        }
    }
    //创建ActivityThread 之前system_server是通过静态函数systemMain来创建的 现在直接new的,设置了mResourcesManager
    ActivityThread thread = new ActivityThread();
    //调用了attach
    thread.attach(false, startSeq);

    if (sMainThreadHandler == null) {
        sMainThreadHandler = thread.getHandler();
    }

    if (false) {
        Looper.myLooper().setMessageLogging(new
                LogPrinter(Log.DEBUG, "ActivityThread"));
    }
    Looper.loop();//开启loop
}

private void attach(boolean system, long startSeq) {
    sCurrentActivityThread = this;
    mSystemThread = system;
    if (!system) {//之前system_server 不走这里，现在我们走这里了
    //设置AppName
        android.ddm.DdmHandleAppName.setAppName("<pre-initialized>",
                                                UserHandle.myUserId());
        RuntimeInit.setApplicationObject(mAppThread.asBinder());
        final IActivityManager mgr = ActivityManager.getService();
        try {
        //调用AMS的attachApplication 这里又是一次IPC通信 因为我们当前是应用进程了
            mgr.attachApplication(mAppThread, startSeq);
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    ViewRootImpl.addConfigCallback(configChangedCallback);
}

AMS的attachApplication
public final void attachApplication(IApplicationThread thread, long startSeq) {
    if (thread == null) {
        throw new SecurityException("Invalid application interface");
    }
    synchronized (this) {
        int callingPid = Binder.getCallingPid();
        final int callingUid = Binder.getCallingUid();
        final long origId = Binder.clearCallingIdentity();
        //调用attachApplicationLocked
        attachApplicationLocked(thread, callingPid, callingUid, startSeq);
        Binder.restoreCallingIdentity(origId);
    }
}


private boolean attachApplicationLocked(@NonNull IApplicationThread thread,
        int pid, int callingUid, long startSeq) {

    //根据pid获取到app信息
    app = mPidsSelfLocked.get(pid);
    //thread指的是ActivityThread 所以这里是IPC通信
    thread.bindApplication(processName, appInfo, providers,
        instr2.mClass,
        profilerInfo, instr2.mArguments,
        instr2.mWatcher,
        instr2.mUiAutomationConnection, testMode,
        mBinderTransactionTrackingEnabled, enableTrackAllocation,
        isRestrictedBackupMode || !normalMode, app.isPersistent(),
        new Configuration(app.getWindowProcessController().getConfiguration()),
        app.compat, getCommonServicesLocked(app.isolated),
        mCoreSettingsObserver.getCoreSettingsLocked(),
        buildSerial, autofillOptions, contentCaptureOptions);
        
//从starting applications中移除
mPersistentStartingProcesses.remove(app);

    if (normalMode) {
        try {
        //启动activity
            didSomething = mAtmInternal.attachApplication(app.getWindowProcessController());
        } catch (Exception e) {
            Slog.wtf(TAG, "Exception thrown launching activities in " + app, e);
            badApp = true;
        }
    }
    if (!badApp) {
        try {
            didSomething |= mServices.attachApplicationLocked(app, processName);
            checkTime(startTime, "attachApplicationLocked: after mServices.attachApplicationLocked");
        } catch (Exception e) {
            Slog.wtf(TAG, "Exception thrown starting services in " + app, e);
            badApp = true;
        }
    }

    return true;
}


//到了ActivityThread的bindApplication
public final void bindApplication(String processName, ApplicationInfo appInfo,
        List<ProviderInfo> providers, ComponentName instrumentationName,
        ProfilerInfo profilerInfo, Bundle instrumentationArgs,
        IInstrumentationWatcher instrumentationWatcher,
        IUiAutomationConnection instrumentationUiConnection, int debugMode,
        boolean enableBinderTracking, boolean trackAllocation,
        boolean isRestrictedBackupMode, boolean persistent, Configuration config,
        CompatibilityInfo compatInfo, Map services, Bundle coreSettings,
        String buildSerial, AutofillOptions autofillOptions,
        ContentCaptureOptions contentCaptureOptions) {
    setCoreSettings(coreSettings);

    AppBindData data = new AppBindData();
    data.processName = processName;
    data.appInfo = appInfo;
    data.providers = providers;
    data.instrumentationName = instrumentationName;
    data.instrumentationArgs = instrumentationArgs;
    data.instrumentationWatcher = instrumentationWatcher;
    data.instrumentationUiAutomationConnection = instrumentationUiConnection;
    data.debugMode = debugMode;
    data.enableBinderTracking = enableBinderTracking;
    data.trackAllocation = trackAllocation;
    data.restrictedBackupMode = isRestrictedBackupMode;
    data.persistent = persistent;
    data.config = config;
    data.compatInfo = compatInfo;
    data.initProfilerInfo = profilerInfo;
    data.buildSerial = buildSerial;
    data.autofillOptions = autofillOptions;
    data.contentCaptureOptions = contentCaptureOptions;
    //给handle 发送BIND_APPLICATION data就是上边的data
    sendMessage(H.BIND_APPLICATION, data);
}
在handleMessage中

case BIND_APPLICATION:
    Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "bindApplication");
    AppBindData data = (AppBindData)msg.obj;
    //调用handleBindApplication
    handleBindApplication(data);
    Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
    break;
    
    
    
private void handleBindApplication(AppBindData data) {
    //把UI线程注册成运行时 敏感线程
    VMRuntime.registerSensitiveThread();

    //记录进程运行时间
    Process.setStartTimes(SystemClock.elapsedRealtime(), SystemClock.uptimeMillis());
    //设置进程名
    android.ddm.DdmHandleAppName.setAppName(data.processName,
                                            UserHandle.myUserId());
    VMRuntime.setProcessPackageName(data.appInfo.packageName);
    //应用目录
    VMRuntime.setProcessDataDirectory(data.appInfo.dataDir);

    if (mProfiler.profileFd != null) {
        mProfiler.startProfiling();
    }
//获取到loadedApk 并存入到mPackages中
data.info = getPackageInfoNoCheck(data.appInfo, data.compatInfo);


//创建Instrumentation
mInstrumentation = new Instrumentation();
mInstrumentation.basicInit(this);

    //创建Application
    Application app;
    //创建应用
        app = data.info.makeApplication(data.restrictedBackupMode, null);
        mInitialApplication = app;
        mInstrumentation.onCreate(data.instrumentationArgs);
        mInstrumentation.callApplicationOnCreate(app);
    }
}
//创建Application
public Application makeApplication(boolean forceDefaultAppClass,
        Instrumentation instrumentation) {
    if (mApplication != null) {//如果已经创建过了 返回
        return mApplication;
    }
    Application app = null;
    //拿到applicationName 就是我们配置的Application 默认是android.app.Application
    String appClass = mApplicationInfo.className; 
    if (forceDefaultAppClass || (appClass == null)) {
        appClass = "android.app.Application";
    }
        java.lang.ClassLoader cl = getClassLoader();
        //创建上下文
        ContextImpl appContext = ContextImpl.createAppContext(mActivityThread, this);
        //创建Application
        app = mActivityThread.mInstrumentation.newApplication(
                cl, appClass, appContext);
        appContext.setOuterContext(app);
   //添加到mAllApplications中
    mActivityThread.mAllApplications.add(app);
    mApplication = app;

    if (instrumentation != null) {
    //执行oncreate
            instrumentation.callApplicationOnCreate(app);
    }

    return app;
}



//调用Application的onCreate函数
public void callApplicationOnCreate(Application app) {
    app.onCreate();
}


//Instrumentation中创建application
public Application newApplication(ClassLoader cl, String className, Context context)
        throws InstantiationException, IllegalAccessException, 
        ClassNotFoundException {
    Application app = getFactory(context.getPackageName())
            .instantiateApplication(cl, className);
            //调用Application的attach方法
    app.attach(context);
    return app;
}

//返回AppComponentFactory
private AppComponentFactory getFactory(String pkg) {
    if (pkg == null) {
        Log.e(TAG, "No pkg specified, disabling AppComponentFactory");
        return AppComponentFactory.DEFAULT;
    }
    if (mThread == null) {
        Log.e(TAG, "Uninitialized ActivityThread, likely app-created Instrumentation,"
                + " disabling AppComponentFactory", new Throwable());
        return AppComponentFactory.DEFAULT;
    }
    //之前在handlebindApplication 已经存入了 现在获取出来apk
    LoadedApk apk = mThread.peekPackageInfo(pkg, true);
    if (apk == null) apk = mThread.getSystemContext().mPackageInfo;
    return apk.getAppFactory();
}


文件目录:/frameworks/base/core/java/android/app/AppComponentFactory.java 
//通过反射创建Application
public @NonNull Application instantiateApplication(@NonNull ClassLoader cl,
        @NonNull String className)
        throws InstantiationException, IllegalAccessException, ClassNotFoundException {
    return (Application) cl.loadClass(className).newInstance();
}


final void attach(Context context) {
    attachBaseContext(context);//调用attachBaseContext
    //拿到mLoadedApk
    mLoadedApk = ContextImpl.getImpl(context).mPackageInfo;
}
```

代码有点多,总体的流程是在`ActivityThread`的main中创建了ActivityThread以及调用了attach函数，通过IPC调用到`AMS`的`attachApplication`把pid等信息存入到`mPidsSelefLocked`中，然后IPC调用 告诉应用`bindApplication`把一些数据返回,在`bindApplication`之后再调用`ATMSInternal`的`attachApplication`开启activity。在ActivityThread的`handleBindApplication`中 再通过`Instrumentation`反射创建了`Application`，调用`attach`， `callApplicationOnCreate`执行`Application`的生命周期

attemptZygoteSendArgsAndGetResult `方法中 设置了result的`pid`以及`usingWrapper\`。

## 总结

## 图片总结

AMS的启动流程图 ![ams的启动流程图.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/17a3841da8804001aabec76a6dc71a9f~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

AMS创建进程的流程图

![创建进程的时序图.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8373788c59a749229924f2b733f5a072~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

Launcer AMS zygote ActivityThread的交互图

![创建进程的流程图.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/634f075f0dae4e46ae95b3068c37d1d9~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

在线视频(一):

[www.bilibili.com/video/BV18T…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV18T411x7w5%2F%3Fspm_id_from%3D333.999.0.0%26vd_source%3D689a2ec078877b4a664365bdb60362d3 "https://www.bilibili.com/video/BV18T411x7w5/?spm_id_from=333.999.0.0&vd_source=689a2ec078877b4a664365bdb60362d3")

在线视频(二):

[www.bilibili.com/video/BV1nV…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1nV4y1D78L%2F%3Fspm_id_from%3D333.999.0.0%26vd_source%3D689a2ec078877b4a664365bdb60362d3 "https://www.bilibili.com/video/BV1nV4y1D78L/?spm_id_from=333.999.0.0&vd_source=689a2ec078877b4a664365bdb60362d3")