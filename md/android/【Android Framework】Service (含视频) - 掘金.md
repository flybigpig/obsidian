## 回顾

回顾一下: 1.第一个启动的就是`init`进程，它解析了`init.rc`文件，启动各种`service`:`zygote`,`surfaceflinger`,`service_manager`。

2.接着就讲了`Zygote`，Zygote就是一个孵化器，它开启了`system_server`以及开启了`ZygoteServer`用来接收客户端的请求，当客户端请求来了之后就会`fork`出来子进程，并且初始化`binder` 和`进程信息`，为了加速`Zygote`还会预加载一些`class`和`Drawable`、`color`等系统资源。

3.接下来讲了`system_server`，它是系统启动管理`service`的`入口`，比如`AMS`、`PMS`、`WMS`等等，它加载了`framework-res.apk`,接着调用`startBootstrapService`,`startCoreService`,`startOtherService`开启非常多的服务，还开启了`WatchDog`，来监控service。

4.接着讲了`service_manager`，他是一个独立的进程，它存储了系统各种服务的`Binder`，我们经常通过`ServiceMananger`来获取，其中还详细说了`Binder`机制，`C/S`架构，大家要记住`客户端`、`Binder`、`Server`三端的工作流程。

5.之后讲了`Launcher`，它由`system_server`启动，通过`LauncherModel`进行`Binder`通信 通过`PMS`来查询所有的应用信息，然后绑定到`RecyclerView`中，它的点击事件是通过`ItemClickHandler`来处理。

6.接着讲了`AMS`是如何开启应用进程的,首先我们从`Launcher`的点击开始，调用到`Activity`的`startActivity`函数，通过`Instrumentation`的`execStartActivity`经过两次`IPC`(1.通过ServiceManager获取到ATMS 2.调用ATMS的startActivity) 调用到`AMS`端在AMS端进行了一系列的信息处理，会判断进程是否存在，`没有存在的话就会开启进程(通过Socket，给ZygoteServer发送信息)`,传入`entryPoint`为`ActivityThread`,通过`Zygote`来`fork`出来子进程（应用进程）调用`ActivityThread.main`，应用进程创建之后会调用到`AMS`，由`AMS`来`attachApplication`存储进程信息,然后告诉`客户端`，让客户端来创建`Application`，并在客户端创建成功之后 继续执行开启`Activity`的流程。客户端接收到`AMS`的数据之后会创建`loadedApk`,`Instrumentation` 以及`Application调用attach(attachBaseContext)`，调用`Instrumentation`的`callApplicationOncreate`执行`Application`的`Oncreate`周期.

7.应用执行完`Application`的`OnCreate`之后 回到`ATMS`的`attachApplication` 接着调用 `realStartActivityLocked` 创建了`ClientTransaction`，设置`callBack`为`LaunchActivityItem`添加了`stateRequest` 为`ResumeActivityItem`，然后通过`IApplicationThread` 回到客户端执行这两个事务,调用了`ActivityThread`的`scheduleTransaction` 函数，调用`executeCallBack` 执行了`LaunchActivityItem`的execute 他会调用ActivityThread的 `handleLaunchActivity`，会创建Activity Context，通过`Instrumentation.newActivity 反射创建Activity` 并调用`attach 绑定window` 再通过`Instrumentation`的`callActivityOnCreate`执行Activity的`onCreate`，在Activity的onCreate中分发监听给`ActivityLifecycleCallbacks`。最后设置`ActivityClientRecord`的state为`ON_CREATE`。 接着执行`executeLifecycleState`，调用了`cycleToPath`，之前设置了state为ON\_CREATE，所以会返回一个Int数组`{2}` 调用`performLifecycleSequence`会执行到ActivityThread的`handleStartActivity`分发`ActivityLifecycleCallbacks`，并且分发给Fragments，调用`Instrumentation`的`callActivityOnStart` 执行Activity的`onStart`并设置state为`ON_START`，接着执行`ResumeActivityItem`的`execute`，会调用到ActivityThread的`handleResumeActivity`，调用`performResume` 分发resume事件给ActivityLifecycleCallbacks，分发Fragments，调用Instrumentation的`callActivityOnResume` 执行Activity的onResume。 最后会调用`ActivityClientRecord.activity.makeVisible` 通过`WindowManager` 添加当前View 和 WMS(IPC) 通信 绘制UI，接着postResume 会执行 `ATMS`的`activityresume` 设置 AMS的Activity的状态。

具体的细节可以参考之前写的文章和视频：

[【Android FrameWork】第一个启动的程序--init](https://juejin.cn/post/7213733567606685753 "https://juejin.cn/post/7213733567606685753")

[【Android FrameWork】Zygote](https://juejin.cn/post/7214068367607119933 "https://juejin.cn/post/7214068367607119933")

[【Android FrameWork】SystemServer](https://juejin.cn/post/7214493929566945340 "https://juejin.cn/post/7214493929566945340")

[【Android FrameWork】ServiceManager(一)](https://juejin.cn/post/7216537771448598585#heading-4 "https://juejin.cn/post/7216537771448598585#heading-4")

[【Android FrameWork】ServiceManager(二)](https://juejin.cn/post/7216536069285675045#heading-9 "https://juejin.cn/post/7216536069285675045#heading-9")

[【Android Framework】Launcher3](https://juejin.cn/post/7218129062744227896#heading-1 "https://juejin.cn/post/7218129062744227896#heading-1")

[【Android Framework】ActivityManagerService(一)](https://juejin.cn/post/7219130999685808188 "https://juejin.cn/post/7219130999685808188")

[【Android Framework】ActivityManagerService(二)](https://juejin.cn/post/7220439797565767741 "https://juejin.cn/post/7220439797565767741")

## 介绍

Service是一种可以在后台执行长时间运行操作而不提供界面的应用组件。服务可以由其他应用组件启动，而且即使用户切换到其他应用，服务仍将在后台继续运行。此外，组件可以通过绑定到服务与之进行交互，甚至是执行进程之间的通信。

## 正文

## 1.Service的介绍

```
<service android:name=".MyService"
    android:isolatedProcess="true"
    android:directBootAware="true"
    android:process=":subService"/>
```

```
import android.app.Service
import android.content.Intent
import android.os.IBinder

class PreloadService : Service() {
    override fun onBind(p0: Intent?): IBinder? {
        return null
    }
}
```

service的标签属性比较简单，具体可以参考官网。我在这里就举几个属性的例子:

`isolatedProcess`:服务与系统其余部分隔离的特殊进程下运行。

`process`:指定服务运行的进程名称。以`:`开头，系统会根据需要时创建新进程，并且服务会在该进程运行。

`directBootAware`:服务是否支持直接启动，也就是可以在用户解锁设备之前运行。

### Service的生命周期

![Service生命周期](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/2ee883beab2840e08365939a4b8b674b~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp) 它的生命周期会和我们使用`Service`的不同 也有所不同。左边是`startService` 右边是`bindService`。我们汇编可以根据源码 再来看看执行的时机。

### Service的运行模式

1.前台Service:执行一些用户能注意到的操作，前台服务必须显示通知。

2.后台Service:后台服务不会影响用户，在26以后，系统会对后台服务施加限制。

## 2.Service的启动

### 1.startService

在源码中我们可以直接在Activity中调用`startService`,那就跟一下 看看它是如何开启Service的。 文件目录:`/android/frameworks/base/core/java/android/content/ContextWrapper.java`

```
public ComponentName startServiceAsUser(Intent service, UserHandle user) {
    return mBase.startServiceAsUser(service, user);
}
//mBase就是ContextImpl 参考Activity创建流程
Context mBase;

public ComponentName startService(Intent service) {
    warnIfCallingFromSystemProcess();
    return startServiceCommon(service, false, mUser);
}

//开启Service
private ComponentName startServiceCommon(Intent service, boolean requireForeground,
        UserHandle user) {
    try {
        //校验intent
        validateServiceIntent(service);
        //设置intent的属性，准备离开现有进程
        service.prepareToLeaveProcess(this);
        //调用AMS的startService  传入了 当前进程，service(目标servcie),true 
        ComponentName cn = ActivityManager.getService().startService(
            mMainThread.getApplicationThread(), service, service.resolveTypeIfNeeded(
                        getContentResolver()), requireForeground,
                        getOpPackageName(), user.getIdentifier());
        return cn;
    } catch (RemoteException e) {
        throw e.rethrowFromSystemServer();
    }
}
```

看看`AMS`是如何开启Service

```
public ComponentName startService(IApplicationThread caller, Intent service,
        String resolvedType, boolean requireForeground, String callingPackage, int userId)
        throws TransactionTooLargeException {
        //判断是否是隔离进程调用
    enforceNotIsolatedCaller("startService");
   
    synchronized(this) {
        //获取pid
        final int callingPid = Binder.getCallingPid();
        //获取uid
        final int callingUid = Binder.getCallingUid();
        //获取origid
        final long origId = Binder.clearCallingIdentity();
        ComponentName res;
        try {
        //调用mService的startServiceLocked 这里的mService就是之前讲AMS启动流程时创建的ActiveServices 用来管理Service的
            res = mServices.startServiceLocked(caller, service,
                    resolvedType, callingPid, callingUid,
                    requireForeground, callingPackage, userId);
        } finally {
            Binder.restoreCallingIdentity(origId);
        }
        return res;
    }
}
文件目录:/android/frameworks/base/services/core/java/com/android/server/am/ActiveServices.java

ComponentName startServiceLocked(IApplicationThread caller, Intent service, String resolvedType,
        int callingPid, int callingUid, boolean fgRequired, String callingPackage, final int userId)
        throws TransactionTooLargeException {
    return startServiceLocked(caller, service, resolvedType, callingPid, callingUid, fgRequired,
            callingPackage, userId, false);
}


ComponentName startServiceLocked(IApplicationThread caller, Intent service, String resolvedType,
        int callingPid, int callingUid, boolean fgRequired, String callingPackage,
        final int userId, boolean allowBackgroundActivityStarts)
        throws TransactionTooLargeException {

    final boolean callerFg;
    if (caller != null) {//不为null 获取到当前进程的ProcessRecord
        final ProcessRecord callerApp = mAm.getRecordForAppLocked(caller);
        callerFg = callerApp.setSchedGroup != ProcessList.SCHED_GROUP_BACKGROUND;
    } else {
        callerFg = true;
    }
    //得到ServiceRecord对象:先从mServiceMap中查找 如果有就返回 没有的话就创建（通过pkms.resolveService 查找service创建ServiceRecord包装成ServiceLookupResult返回）
    ServiceLookupResult res =
        retrieveServiceLocked(service, null, resolvedType, callingPackage,
                callingPid, callingUid, userId, true, callerFg, false, false);
     //拿到ServiceRecord  ServiceRecord继承自Binder
    ServiceRecord r = res.record;
    if (fgRequired) {//这里是false不走 前台走这里
    }

    if (forcedStandby || (!r.startRequested && !fgRequired)) {
    //判断是否运行启动服务：常驻内存、蓝牙、电源白名单允许直接启动服务 否则执行默认策略：如果大于AndroidO不允许启动后台服务
        final int allowed = mAm.getAppStartModeLocked(r.appInfo.uid, r.packageName,
                r.appInfo.targetSdkVersion, callingPid, false, false, forcedStandby);
        if (allowed != ActivityManager.APP_START_MODE_NORMAL) {//不允许启动服务的逻辑
            if (allowed == ActivityManager.APP_START_MODE_DELAYED || forceSilentAbort) {
                return null;
            }
            if (forcedStandby) {
                if (fgRequired) {//false
                    return null;
                }
            }
            UidRecord uidRec = mAm.mProcessList.getUidRecordLocked(r.appInfo.uid);
            return new ComponentName("?", "app is in background uid " + uidRec);
        }
    }
    if (r.appInfo.targetSdkVersion < Build.VERSION_CODES.O && fgRequired) {//不走这里
        fgRequired = false;
    }
    //记录时间戳
    r.lastActivity = SystemClock.uptimeMillis();
    //设置已经开启了
    r.startRequested = true;
    r.delayedStop = false;
    r.fgRequired = fgRequired;
    //添加了一个ServiceRecord.StartItem taskRemoved = false  如果是BindService 不会添加 因为他直接调用的是realStartService
    r.pendingStarts.add(new ServiceRecord.StartItem(r, false, r.makeNextStartId(),
            service, neededGrants, callingUid));

    if (fgRequired) {//这里不走
    }

    final ServiceMap smap = getServiceMapLocked(r.userId);
    boolean addToStarting = false;

    if (allowBackgroundActivityStarts) {
        r.whitelistBgActivityStartsOnServiceStart();
    }
    //调用startServiceInnerLocked
    ComponentName cmp = startServiceInnerLocked(smap, service, r, callerFg, addToStarting);
    return cmp;
}


ComponentName startServiceInnerLocked(ServiceMap smap, Intent service, ServiceRecord r,
        boolean callerFg, boolean addToStarting) throws TransactionTooLargeException {
    ServiceState stracker = r.getTracker();
    if (stracker != null) {
        stracker.setStarted(true, mAm.mProcessStats.getMemFactorLocked(), r.lastActivity);
    }
    r.callStart = false;
    synchronized (r.stats.getBatteryStats()) {
        r.stats.startRunningLocked();
    }
    //调用bringUpServiceLocked 开启进程
    String error = bringUpServiceLocked(r, service.getFlags(), callerFg, false, false);
    if (error != null) {
        return new ComponentName("!!", error);
    }
    if (r.startRequested && addToStarting) {
        boolean first = smap.mStartingBackground.size() == 0;
        smap.mStartingBackground.add(r);
        r.startingBgTimeout = SystemClock.uptimeMillis() + mAm.mConstants.BG_START_TIMEOUT;
        if (DEBUG_DELAYED_SERVICE) {
            RuntimeException here = new RuntimeException("here");
            here.fillInStackTrace();
        } else if (DEBUG_DELAYED_STARTS) {
        }
        if (first) {
            smap.rescheduleDelayedStartsLocked();
        }
    } else if (callerFg || r.fgRequired) {
        smap.ensureNotStartingBackgroundLocked(r);
    }

    return r.name;
}


private String bringUpServiceLocked(ServiceRecord r, int intentFlags, boolean execInFg,
        boolean whileRestarting, boolean permissionsReviewRequired)
        throws TransactionTooLargeException {
    if (r.app != null && r.app.thread != null) {//已经运行
        sendServiceArgsLocked(r, execInFg, false);
        return null;
    }
    if (!whileRestarting && mRestartingServices.contains(r)) {
        return null;
    }
    if (mRestartingServices.remove(r)) {
        clearRestartingIfNeededLocked(r);
    }
    if (r.delayed) {
        getServiceMapLocked(r.userId).mDelayedStartList.remove(r);
        r.delayed = false;
    }
    //是否需要进程隔离下 运行
    final boolean isolated = (r.serviceInfo.flags&ServiceInfo.FLAG_ISOLATED_PROCESS) != 0;
    //获取进程名
    final String procName = r.processName;
    //创建HostingRecord
    HostingRecord hostingRecord = new HostingRecord("service", r.instanceName);
    ProcessRecord app;

    if (!isolated) {//非隔离进程
        //获取到service的ProcessRecord
        app = mAm.getProcessRecordLocked(procName, r.appInfo.uid, false);
        if (app != null && app.thread != null) {//进程已经存在 启动了
            try {
                app.addPackage(r.appInfo.packageName, r.appInfo.longVersionCode, mAm.mProcessStats);
                realStartServiceLocked(r, app, execInFg);
                return null;
            } catch (TransactionTooLargeException e) {
                throw e;
            } catch (RemoteException e) {
            }
        }
    } else {//隔离进程
        //第一次开启isolatedProc是null
        app = r.isolatedProc;
        if (WebViewZygote.isMultiprocessEnabled()
                && r.serviceInfo.packageName.equals(WebViewZygote.getPackageName())) {
            hostingRecord = HostingRecord.byWebviewZygote(r.instanceName);
        }
        if ((r.serviceInfo.flags & ServiceInfo.FLAG_USE_APP_ZYGOTE) != 0) {
            hostingRecord = HostingRecord.byAppZygote(r.instanceName, r.definingPackageName,
                    r.definingUid);
        }
    }
    //服务还没启动调用startProcessLocked 创建进程
    if (app == null && !permissionsReviewRequired) {
        if ((app=mAm.startProcessLocked(procName, r.appInfo, true, intentFlags,
                hostingRecord, false, isolated, false)) == null) {
            bringDownServiceLocked(r);
            return msg;
        }
        if (isolated) {
            r.isolatedProc = app;
        }
    }

    if (r.fgRequired) {//不走这里 是false
        mAm.tempWhitelistUidLocked(r.appInfo.uid,
                SERVICE_START_FOREGROUND_TIMEOUT, "fg-service-launch");
    }
    if (!mPendingServices.contains(r)) {
    //假如mPendingServices中
        mPendingServices.add(r);
    }

    if (r.delayedStop) {
        r.delayedStop = false;
        if (r.startRequested) {
            stopServiceLocked(r);
        }
    }

    return null;
}
```

在`AMS`这里会先检查Service是否可以执行(常驻内存、蓝牙、电源白名单允许直接启动服务)，接着调用`bringUpServiceLocked` 判断是否需要隔离进程如果非隔离 就看是否已经启动进程 执行`realStartServiceLocked`，否则是隔离进程 直接开启新进程。我们没有设置隔离进程 所以我们会直接开启新进程，开启进程的过程在之前已经接触很多次了，我们直接跳过，开启成功之后会将ServiceRecord添加到`mPendingServices`中去。所以我们看看进程创建成功之后会如何开启Service。

```
private boolean attachApplicationLocked(@NonNull IApplicationThread thread,
        int pid, int callingUid, long startSeq) {
            //到这里来
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
      
    boolean badApp = false;
    boolean didSomething = false;
   //在这里开启activity
    if (normalMode) {
        try {
            didSomething = mAtmInternal.attachApplication(app.getWindowProcessController());
        } catch (Exception e) {
            badApp = true;
        }
    }

    
    if (!badApp) {
        try {
        //开启Service
            didSomething |= mServices.attachApplicationLocked(app, processName);
        } catch (Exception e) {
            badApp = true;
        }
    }
    return true;
}
//还是在ActiveService中

boolean attachApplicationLocked(ProcessRecord proc, String processName)
        throws RemoteException {
    boolean didSomething = false;
     //此时Services的size是大于0的
    if (mPendingServices.size() > 0) {
        ServiceRecord sr = null;
        try {
            for (int i=0; i<mPendingServices.size(); i++) {
                sr = mPendingServices.get(i);
                if (proc != sr.isolatedProc && (proc.uid != sr.appInfo.uid
                        || !processName.equals(sr.processName))) {
                    continue;
                }

                mPendingServices.remove(i);
                i--;
                proc.addPackage(sr.appInfo.packageName, sr.appInfo.longVersionCode,
                        mAm.mProcessStats);
                 //调用realStartServiceLocked开启service
                realStartServiceLocked(sr, proc, sr.createdFromFg);
                didSomething = true;
                if (!isServiceNeededLocked(sr, false, false)) {
                    bringDownServiceLocked(sr);
                }
            }
        } catch (RemoteException e) {
            throw e;
        }
    }
    return didSomething;
}

//开启service
private final void realStartServiceLocked(ServiceRecord r,
        ProcessRecord app, boolean execInFg) throws RemoteException {
    //服务和当前进程绑定
    r.setProcess(app);
    r.restartTime = r.lastActivity = SystemClock.uptimeMillis();
    //添加到ProcessRecord中
    final boolean newService = app.services.add(r);
    //埋炸弹scheduleServiceTimeoutLocked
    bumpServiceExecutingLocked(r, execInFg, "create");
    //更新进程LRU
    mAm.updateLruProcessLocked(app, false, null);
    updateServiceForegroundLocked(r.app, /* oomAdj= */ false);
    //更新OOMAdj
    mAm.updateOomAdjLocked(OomAdjuster.OOM_ADJ_REASON_START_SERVICE);
    boolean created = false;
    try {
        synchronized (r.stats.getBatteryStats()) {
            r.stats.startLaunchedLocked();
        }
        mAm.notifyPackageUse(r.serviceInfo.packageName,
                             PackageManager.NOTIFY_PACKAGE_USE_SERVICE);
        app.forceProcessStateUpTo(ActivityManager.PROCESS_STATE_SERVICE);
        //调用到应用进程 也就是服务进程 ActivityThread
        app.thread.scheduleCreateService(r, r.serviceInfo,
                mAm.compatibilityInfoForPackage(r.serviceInfo.applicationInfo),
                app.getReportedProcState());
        r.postNotification();
        created = true;
    } catch (DeadObjectException e) {
    } finally {
    }

    if (r.whitelistManager) {
        app.whitelistManager = true;
    }
    //处理Bind我们不处理 没有Bind 此时直说startService
    requestServiceBindingsLocked(r, execInFg);

    updateServiceClientActivitiesLocked(app, null, true);

    if (newService && created) {
        app.addBoundClientUidsOfNewService(r);
    }
    if (r.startRequested && r.callStart && r.pendingStarts.size() == 0) {
        r.pendingStarts.add(new ServiceRecord.StartItem(r, false, r.makeNextStartId(),
                null, null, 0));
    }
    //发送Service的args
    sendServiceArgsLocked(r, execInFg, true);
    if (r.delayed) {
        getServiceMapLocked(r.userId).mDelayedStartList.remove(r);
        r.delayed = false;
    }
}

回到ActivityThread看看它的scheduleCreateService

public final void scheduleCreateService(IBinder token,
        ServiceInfo info, CompatibilityInfo compatInfo, int processState) {
    updateProcessState(processState, false);
    CreateServiceData s = new CreateServiceData();
    s.token = token;
    s.info = info;
    s.compatInfo = compatInfo;
    //给mH发送了H.CREATE_SERVICE
    sendMessage(H.CREATE_SERVICE, s);
}

case CREATE_SERVICE:
    handleCreateService((CreateServiceData)msg.obj);
    break;
//创建Service
private void handleCreateService(CreateServiceData data) {
    unscheduleGcIdler();

    LoadedApk packageInfo = getPackageInfoNoCheck(
            data.info.applicationInfo, data.compatInfo);
    Service service = null;
    try {
        //反射创建Service
        java.lang.ClassLoader cl = packageInfo.getClassLoader();
        service = packageInfo.getAppFactory()
                .instantiateService(cl, data.info.name, data.intent);
    } catch (Exception e) {
    }

    try {
        //创建Context
        ContextImpl context = ContextImpl.createAppContext(this, packageInfo);
        context.setOuterContext(service);
        //调用makeApplication
        Application app = packageInfo.makeApplication(false, mInstrumentation);
        //调用service的attach 绑定ActivityThread token Application 以及AMS
        service.attach(context, this, data.info.name, data.token, app,
                ActivityManager.getService());
         //调用service的onCreate
        service.onCreate();
        //存入到mService中去
        mServices.put(data.token, service);
        try {
        //告诉AMS创建完成
            ActivityManager.getService().serviceDoneExecuting(
                    data.token, SERVICE_DONE_EXECUTING_ANON, 0, 0);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    } catch (Exception e) {
    }
}

public final void attach(
        Context context,
        ActivityThread thread, String className, IBinder token,
        Application application, Object activityManager) {
    attachBaseContext(context);
    mThread = thread;           // NOTE:  unused - remove?
    mClassName = className;
    mToken = token;
    mApplication = application;
    mActivityManager = (IActivityManager)activityManager;
    mStartCompatibility = getApplicationInfo().targetSdkVersion
            < Build.VERSION_CODES.ECLAIR;
}


到AMS 处理服务创建

public void serviceDoneExecuting(IBinder token, int type, int startId, int res) {
    synchronized(this) {
        mServices.serviceDoneExecutingLocked((ServiceRecord)token, type, startId, res);
    }
}

//AMS开启Service成功
void serviceDoneExecutingLocked(ServiceRecord r, int type, int startId, int res) {
    boolean inDestroying = mDestroyingServices.contains(r);
    if (r != null) {
        //在这里把炸弹拆除
        serviceDoneExecutingLocked(r, inDestroying, inDestroying);
    } 
}
发送的args
private final void sendServiceArgsLocked(ServiceRecord r, boolean execInFg,
        boolean oomAdjusted) throws TransactionTooLargeException {
    final int N = r.pendingStarts.size();
    if (N == 0) {//不是0
        return;
    }
    ArrayList<ServiceStartArgs> args = new ArrayList<>();

    while (r.pendingStarts.size() > 0) {
        ServiceRecord.StartItem si = r.pendingStarts.remove(0);
        if (si.intent == null && N > 1) {
            continue;
        }
        si.deliveredTime = SystemClock.uptimeMillis();
        r.deliveredStarts.add(si);
        si.deliveryCount++;
        //埋炸弹
        bumpServiceExecutingLocked(r, execInFg, "start");
        if (!oomAdjusted) {
            oomAdjusted = true;
            mAm.updateOomAdjLocked(r.app, true, OomAdjuster.OOM_ADJ_REASON_START_SERVICE);
        }
        if (r.fgRequired && !r.fgWaiting) {//前台Service
            if (!r.isForeground) {
                scheduleServiceForegroundTransitionTimeoutLocked(r);
            } else {
                r.fgRequired = false;
            }
        }
        int flags = 0;
        if (si.deliveryCount > 1) {
            flags |= Service.START_FLAG_RETRY;
        }
        if (si.doneExecutingCount > 0) {
            flags |= Service.START_FLAG_REDELIVERY;
        }
        args.add(new ServiceStartArgs(si.taskRemoved, si.id, flags, si.intent));
    }
    //执行ActivityThread的scheduleServiceArgs
    r.app.thread.scheduleServiceArgs(r, slice);
}


public final void scheduleServiceArgs(IBinder token, ParceledListSlice args) {
    List<ServiceStartArgs> list = args.getList();

    for (int i = 0; i < list.size(); i++) {
        ServiceStartArgs ssa = list.get(i);
        ServiceArgsData s = new ServiceArgsData();
        s.token = token;
        s.taskRemoved = ssa.taskRemoved;
        s.startId = ssa.startId;
        s.flags = ssa.flags;
        s.args = ssa.args;
        //给mH发送SERVICE_ARGS
        sendMessage(H.SERVICE_ARGS, s);
    }
}

case SERVICE_ARGS:
    handleServiceArgs((ServiceArgsData)msg.obj);
    break;

//处理Args
private void handleServiceArgs(ServiceArgsData data) {
    Service s = mServices.get(data.token);
    if (s != null) {
        try {
            if (data.args != null) {
                data.args.setExtrasClassLoader(s.getClassLoader());
                data.args.prepareToEnterProcess();
            }
            int res;
            if (!data.taskRemoved) {//这里是false 所以会执行onStartCommand
                res = s.onStartCommand(data.args, data.flags, data.startId);
            } else {
            }

            QueuedWork.waitToFinish();

            try {
                //通知AMS 拆除炸弹
                ActivityManager.getService().serviceDoneExecuting(
                        data.token, SERVICE_DONE_EXECUTING_START, data.startId, res);
            } catch (RemoteException e) {
            }
        } catch (Exception e) {
        }
    }
}
```

进程创建之后，会调用`AMS`的`attachApplication` 接着处理`service(ActiveService)`，之前创建之后会添加到`mPendingServices`中，现在继续处理调用`realStartServiceLocked`来开启Service，在开启的过程中会埋入一个炸弹(给Handler发送一个`SERVICE_TIMEOUT_MSG`) 如果超时未处理会弹出`ANR`，然后调用`app.thread.scheduleCreateService`通知客户端创建反射`Service、Context` 调用`onCreate` 把`Service`存入到`mService`中，调用`AMS`的`serviceDoneExecuting`进行炸弹的拆除。 然后再埋炸弹 调用`app.thread.scheduleServiceArgs` 调用`service.onStartCommand`再通知AMS 拆除炸弹。 这样Service就运行起来了。我们再看看Service的onStop。

还是回到`ContextImpl`

```
public boolean stopService(Intent service) {
    return stopServiceCommon(service, mUser);
}


private boolean stopServiceCommon(Intent service, UserHandle user) {
        validateServiceIntent(service);
        service.prepareToLeaveProcess(this);
        //调用AMS的stopService
        int res = ActivityManager.getService().stopService(
            mMainThread.getApplicationThread(), service,
            service.resolveTypeIfNeeded(getContentResolver()), user.getIdentifier());
        return res != 0;
}


public int stopService(IApplicationThread caller, Intent service,
        String resolvedType, int userId) {
    enforceNotIsolatedCaller("stopService");
    synchronized(this) {
    //调用ActiveService的stopServiceLocked
        return mServices.stopServiceLocked(caller, service, resolvedType, userId);
    }
}


int stopServiceLocked(IApplicationThread caller, Intent service,
        String resolvedType, int userId) {
    //获取到调用这的ProcessRecord
    final ProcessRecord callerApp = mAm.getRecordForAppLocked(caller);
    //获取ServiceRecord 
    ServiceLookupResult r = retrieveServiceLocked(service, null, resolvedType, null,
            Binder.getCallingPid(), Binder.getCallingUid(), userId, false, false, false, false);
    if (r != null) {
        if (r.record != null) {
            final long origId = Binder.clearCallingIdentity();
            try {
                //调用stopServiceLocked
                stopServiceLocked(r.record);
            } finally {
                Binder.restoreCallingIdentity(origId);
            }
            return 1;
        }
        return -1;
    }

    return 0;
}


private void stopServiceLocked(ServiceRecord service) {
    if (service.delayed) {
        service.delayedStop = true;
        return;
    }
    synchronized (service.stats.getBatteryStats()) {
        service.stats.stopRunningLocked();
    }
    service.startRequested = false;
    if (service.tracker != null) {
        service.tracker.setStarted(false, mAm.mProcessStats.getMemFactorLocked(),
                SystemClock.uptimeMillis());
    }
    service.callStart = false;

    bringDownServiceIfNeededLocked(service, false, false);
}

private final void bringDownServiceIfNeededLocked(ServiceRecord r, boolean knowConn,
        boolean hasConn) {
    if (isServiceNeededLocked(r, knowConn, hasConn)) {
        return;
    }

    if (mPendingServices.contains(r)) {
        return;
    }

    bringDownServiceLocked(r);
}



private final void bringDownServiceLocked(ServiceRecord r) {
    if (r.app != null && r.app.thread != null) {
        boolean needOomAdj = false;
        //执行unBind
        for (int i = r.bindings.size() - 1; i >= 0; i--) {
            IntentBindRecord ibr = r.bindings.valueAt(i);
            if (ibr.hasBound) {
                try {
                    bumpServiceExecutingLocked(r, false, "bring down unbind");
                    needOomAdj = true;
                    ibr.hasBound = false;
                    ibr.requested = false;
                    r.app.thread.scheduleUnbindService(r,
                            ibr.intent.getIntent());
                } catch (Exception e) {
                    serviceProcessGoneLocked(r);
                }
            }
        }
        if (needOomAdj) {
            mAm.updateOomAdjLocked(r.app, true,
                    OomAdjuster.OOM_ADJ_REASON_UNBIND_SERVICE);
        }
    }

        r.fgRequired = false;
        r.fgWaiting = false;
        ServiceState stracker = r.getTracker();
        if (stracker != null) {
            stracker.setForeground(false, mAm.mProcessStats.getMemFactorLocked(),
                    r.lastActivity);
        }
        mAm.mAppOpsService.finishOperation(AppOpsManager.getToken(mAm.mAppOpsService),
                AppOpsManager.OP_START_FOREGROUND, r.appInfo.uid, r.packageName);
        mAm.mHandler.removeMessages(
                ActivityManagerService.SERVICE_FOREGROUND_TIMEOUT_MSG, r);
        if (r.app != null) {
            Message msg = mAm.mHandler.obtainMessage(
                    ActivityManagerService.SERVICE_FOREGROUND_CRASH_MSG);
            msg.obj = r.app;
            msg.getData().putCharSequence(
                ActivityManagerService.SERVICE_RECORD_KEY, r.toString());
            mAm.mHandler.sendMessage(msg);
        }
    }
//记录销毁时间
    r.destroyTime = SystemClock.uptimeMillis();

    final ServiceMap smap = getServiceMapLocked(r.userId);
    //从map中移除
    ServiceRecord found = smap.mServicesByInstanceName.remove(r.instanceName);
    //移除
    smap.mServicesByIntent.remove(r.intent);
    r.totalRestartCount = 0;
    r.isForeground = false;
    r.foregroundId = 0;
    r.foregroundNoti = null;

    r.clearDeliveredStartsLocked();
    r.pendingStarts.clear();
    smap.mDelayedStartList.remove(r);

    if (r.app != null) {
        synchronized (r.stats.getBatteryStats()) {
            r.stats.stopLaunchedLocked();
        }
        r.app.services.remove(r);
        r.app.updateBoundClientUids();
        if (r.whitelistManager) {
            updateWhitelistManagerLocked(r.app);
        }
        if (r.app.thread != null) {
            updateServiceForegroundLocked(r.app, false);
            try {
               //埋炸弹
                bumpServiceExecutingLocked(r, false, "destroy");
                mDestroyingServices.add(r);
                r.destroying = true;
                mAm.updateOomAdjLocked(r.app, true,
                        OomAdjuster.OOM_ADJ_REASON_UNBIND_SERVICE);
                //到客户端执行scheduleStopService
                r.app.thread.scheduleStopService(r);
            } catch (Exception e) {
                serviceProcessGoneLocked(r);
            }
        } else {
        }
    } 

    if (r.bindings.size() > 0) {
    //清空bindings
        r.bindings.clear();
    }

    if (r.restarter instanceof ServiceRestarter) {
       ((ServiceRestarter)r.restarter).setService(null);
    }

    int memFactor = mAm.mProcessStats.getMemFactorLocked();
    long now = SystemClock.uptimeMillis();
    if (r.tracker != null) {
        r.tracker.setStarted(false, memFactor, now);
        r.tracker.setBound(false, memFactor, now);
        if (r.executeNesting == 0) {
            r.tracker.clearCurrentOwner(r, false);
            r.tracker = null;
        }
    }

    smap.ensureNotStartingBackgroundLocked(r);
}


public final void scheduleStopService(IBinder token) {
    sendMessage(H.STOP_SERVICE, token);
}

case STOP_SERVICE:
    handleStopService((IBinder)msg.obj);
    schedulePurgeIdler();//清空待清理的资源
    break;


private void handleStopService(IBinder token) {
//从mService中移除掉 只是从map中移除掉了引用,所以我们需要注意Service内部的thread 以及变量不要引起内存泄漏或者线程没有停止
    Service s = mServices.remove(token);
    if (s != null) {
        try {
           //调用s的onDestory
            s.onDestroy();
            s.detachAndCleanUp();
            Context context = s.getBaseContext();
            if (context instanceof ContextImpl) {
                final String who = s.getClassName();
                ((ContextImpl) context).scheduleFinalCleanup(who, "Service");
            }
            QueuedWork.waitToFinish();
            try {
            //调用AMS的serviceDoneExecuting 拆除炸弹
                ActivityManager.getService().serviceDoneExecuting(
                        token, SERVICE_DONE_EXECUTING_STOP, 0, 0);
            } catch (RemoteException e) {
                throw e.rethrowFromSystemServer();
            }
        } catch (Exception e) {
        }
    } else {
    }
}

```

stop的流程和start的流程差不多，需要注意的是`stop`只是从`map中移除了`，我们需要自己释放资源。

### 2.bindService

bindService和startService使用方式也不一样,在这里我使用的是隐式意图。需要传递`ServiceConnection`，它就是我们用来IPC通信的。

```
    //声明aidl
    var iMyAidlInterface: IMyAidlInterface? = null
    //需要创建ServiceConnection
    val serviceConnection: ServiceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            iMyAidlInterface = IMyAidlInterface.Stub.asInterface(service)
            val ret = iMyAidlInterface?.add(7, 2)
        }

        override fun onServiceDisconnected(name: ComponentName?) {
        }

    }
//隐式意图bindService
val intent = Intent("com.example.myapplication.MyService")
intent.setPackage("com.example.myapplication")
bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE)


//ServiceConnection 只是一个接口
import android.os.IBinder;  
public interface ServiceConnection {  
void onServiceConnected(ComponentName name, IBinder service);  
void onServiceDisconnected(ComponentName name);  
default void onBindingDied(ComponentName name) {  
}  
default void onNullBinding(ComponentName name) {  
}  
}

```

使用方法如上，而`ServiceConnection`只是一个接口，它是怎么进行IPC通信的呢？ 在`onServiceConnected` 中 给我们的`IBinder`是什么呢？我们跟下源码 看看bindService是如何运转的。

```
public boolean bindService(Intent service, ServiceConnection conn, int flags) {
    warnIfCallingFromSystemProcess();
    return bindServiceCommon(service, conn, flags, null, mMainThread.getHandler(), null, getUser());
}


private boolean bindServiceCommon(Intent service, ServiceConnection conn, int flags,
        String instanceName, Handler handler, Executor executor, UserHandle user) {
    IServiceConnection sd;//申请sd 这个sd 很重要
    if (mPackageInfo != null) {
        if (executor != null) {//这里传递的是null
        } else {
            //mPackageInfo就是loadedApk 拿到InnerConnection 创建native层的JavaBBinder对象
            sd = mPackageInfo.getServiceDispatcher(conn, getOuterContext(), handler, flags);
        }
    }
    //校验intent
    validateServiceIntent(service);
    try {
         //拿到当前的token
        IBinder token = getActivityToken();
        //设置参数准备离开当前进程
        service.prepareToLeaveProcess(this);
        //调用AMS的bindIsolatedService
        int res = ActivityManager.getService().bindIsolatedService(
            mMainThread.getApplicationThread(), getActivityToken(), service,
            service.resolveTypeIfNeeded(getContentResolver()),
            sd, flags, instanceName, getOpPackageName(), user.getIdentifier());
        return res != 0;
    } catch (RemoteException e) {
    }
}
文件目录:/Volumes/android/frameworks/base/core/java/android/app/LoadedApk.java



public final IServiceConnection getServiceDispatcher(ServiceConnection c,  
        Context context, Handler handler, int flags) {  
    return getServiceDispatcherCommon(c, context, handler, null, flags);  
}

//获取IServiceConnection
private IServiceConnection getServiceDispatcherCommon(ServiceConnection c,
            Context context, Handler handler, Executor executor, int flags) {
        synchronized (mServices) {
            LoadedApk.ServiceDispatcher sd = null;
            ArrayMap<ServiceConnection, LoadedApk.ServiceDispatcher> map = mServices.get(context);
            if (map != null) {//第一次是null不存在的
                sd = map.get(c);
            }
            if (sd == null) {
                if (executor != null) {//传递的是null
                    sd = new ServiceDispatcher(c, context, executor, flags);
                } else {
                    sd = new ServiceDispatcher(c, context, handler, flags);
                }
                if (map == null) {
                    map = new ArrayMap<>();
                    //存入
                    mServices.put(context, map);
                }
                map.put(c, sd);
            } else {
                sd.validate(context, handler, executor);
            }
            //返回的是mIServiceConnection 也就是InnerConnection
            return sd.getIServiceConnection();
        }
}


  ServiceDispatcher(ServiceConnection conn,
                Context context, Handler activityThread, int flags) {
            mIServiceConnection = new InnerConnection(this);//传递的是this
            mConnection = conn;//赋值
            mContext = context;
            mActivityThread = activityThread;//这里的handle 是ActivityThread的也就是我们当前进程的
            mActivityExecutor = null;
            mLocation = new ServiceConnectionLeaked(null);
            mLocation.fillInStackTrace();
            mFlags = flags;
        }
//我们可以看到InnerConnection继承自Stub
  private static class InnerConnection extends IServiceConnection.Stub {
            final WeakReference<LoadedApk.ServiceDispatcher> mDispatcher;
            InnerConnection(LoadedApk.ServiceDispatcher sd) {
                mDispatcher = new WeakReference<LoadedApk.ServiceDispatcher>(sd);
            }

            public void connected(ComponentName name, IBinder service, boolean dead)
                    throws RemoteException {
                LoadedApk.ServiceDispatcher sd = mDispatcher.get();
                if (sd != null) {
                    sd.connected(name, service, dead);
                }
            }
        }

//Stub又是继承自Binder 所以当我们创建Stub的时候都会调用Binder的无参构造函数
public static abstract class Stub extends android.os.Binder
//Binder的无参构造函数
   public Binder() {
        this(null);
    }
    
//调用的有参构造函数
    public Binder(@Nullable String descriptor)  {
        mObject = getNativeBBinderHolder();//创建NativeBBinderHolder对象我们进入Native层
        NoImagePreloadHolder.sRegistry.registerNativeAllocation(this, mObject);
        mDescriptor = descriptor;
    }
//native端的代码在android_util_Binder.cpp
static jlong android_os_Binder_getNativeBBinderHolder(JNIEnv* env, jobject clazz)  
{  
//new了一个JavaBBinderHolder返回
JavaBBinderHolder* jbh = new JavaBBinderHolder();  
return (jlong) jbh;  
}

//看看AMSProxy是怎么写数据的 这里是根据IActivityManager.aidl生成的java文件
 @Override public int bindIsolatedService(android.app.IApplicationThread caller, android.os.IBinder token, android.content.Intent service, java.lang.String resolvedType, android.app.IServiceConnection connection, int flags, java.lang.String instanceName, java.lang.String callingPackage, int userId) throws android.os.RemoteException
      {
        //和之前一样 生成两个Parcel 一个用来写数据一个用来回复数据
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        int _result;
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          //这里写入了caller 也就是我们当前调用的进程
          _data.writeStrongBinder((((caller!=null))?(caller.asBinder()):(null)));
          _data.writeStrongBinder(token);
          if ((service!=null)) {
            _data.writeInt(1);
            service.writeToParcel(_data, 0);
          }
          else {
            _data.writeInt(0);
          }
          _data.writeString(resolvedType);
          //这里写入了sd 也就是ServiceDispatcher.InnerConnection 把JavaBBinder写入到flat_binder_object
          _data.writeStrongBinder((((connection!=null))?(connection.asBinder()):(null)));
          _data.writeInt(flags);
          _data.writeString(instanceName);
          _data.writeString(callingPackage);
          _data.writeInt(userId);
          //调用mRemote.transact 进行IPC通信 这里的mRemote就是AMS的BinderProxy 也就是调用了BpBinder的transact
          boolean _status = mRemote.transact(Stub.TRANSACTION_bindIsolatedService, _data, _reply, 0);
          if (!_status && getDefaultImpl() != null) {
            return getDefaultImpl().bindIsolatedService(caller, token, service, resolvedType, connection, flags, instanceName, callingPackage, userId);
          }
          _reply.readException();
          _result = _reply.readInt();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
        return _result;
      }
      
 public final void writeStrongBinder(IBinder val) {
        nativeWriteStrongBinder(mNativePtr, val);
 }      
      
//native层是怎么写的呢？
static void android_os_Parcel_writeStrongBinder(JNIEnv* env, jclass clazz, jlong nativePtr, jobject object)
{
    //nativePtr就是Parcel的地址 
    Parcel* parcel = reinterpret_cast<Parcel*>(nativePtr);
    if (parcel != NULL) {
    //调用了ibinderForJavaObject 拿到JavaBBinder
        const status_t err = parcel->writeStrongBinder(ibinderForJavaObject(env, object));
        if (err != NO_ERROR) {
            signalExceptionForError(env, clazz, err);
        }
    }
}


sp<IBinder> ibinderForJavaObject(JNIEnv* env, jobject obj)
{
    if (obj == NULL) return NULL;
    //判断是android/os/Binder 还是BinderProxy 我们是Binder
    if (env->IsInstanceOf(obj, gBinderOffsets.mClass)) {
        JavaBBinderHolder* jbh = (JavaBBinderHolder*)
            env->GetLongField(obj, gBinderOffsets.mObject);
         //调用JavaBBinderHolder.get
        return jbh->get(env, obj);
    }
    if (env->IsInstanceOf(obj, gBinderProxyOffsets.mClass)) {
        return getBPNativeData(env, obj)->mObject;
    }
    return NULL;
}

//JavaBBinder的get
    sp<JavaBBinder> get(JNIEnv* env, jobject obj)
    {
        AutoMutex _l(mLock);
        sp<JavaBBinder> b = mBinder.promote();//第一次为null
        if (b == NULL) {
            //创建JavaBBinder
            b = new JavaBBinder(env, obj);
            mBinder = b;
        }

        return b;
    }

status_t Parcel::writeStrongBinder(const sp<IBinder>& val)  
{  
return flatten_binder(ProcessState::self(), val, this);  
}

status_t flatten_binder(const sp<ProcessState>& /*proc*/,
    const sp<IBinder>& binder, Parcel* out)
{
    flat_binder_object obj;//声明传输数据的结构体

    if (IPCThreadState::self()->backgroundSchedulingDisabled()) {
        obj.flags = FLAT_BINDER_FLAG_ACCEPTS_FDS;
    } else {
        obj.flags = 0x13 | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    }
    //当前的binder是JavaBBinder 它继承自BBinder
    if (binder != nullptr) {
    //localBinder返回的是this
        BBinder *local = binder->localBinder();
        if (!local) {
            BpBinder *proxy = binder->remoteBinder();
            if (proxy == nullptr) {
                ALOGE("null proxy");
            }
            const int32_t handle = proxy ? proxy->handle() : 0;
            obj.hdr.type = BINDER_TYPE_HANDLE;
            obj.binder = 0;
            obj.handle = handle;
            obj.cookie = 0;
        } else {//不为空走这里
            if (local->isRequestingSid()) {
                obj.flags |= FLAT_BINDER_FLAG_TXN_SECURITY_CTX;
            }
            //设置type 为 BINDER_TYPE_BINDER
            obj.hdr.type = BINDER_TYPE_BINDER;
            //设置binder位弱引用
            obj.binder = reinterpret_cast<uintptr_t>(local->getWeakRefs());
            //设置cookie为强指针
            obj.cookie = reinterpret_cast<uintptr_t>(local);
        }
    } else {
        obj.hdr.type = BINDER_TYPE_BINDER;
        obj.binder = 0;
        obj.cookie = 0;
    }
    //out中写入obj 也就是flat_binder_object
    return finish_flatten_binder(binder, obj, out);
}
//BBinder的localBinder返回了this
BBinder* BBinder::localBinder()  
{  
return this;  
}


//开始写数据 就到了Binder驱动
status_t BpBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    if (mAlive) {
        status_t status = IPCThreadState::self()->transact(
            mHandle, code, data, reply, flags);
        if (status == DEAD_OBJECT) mAlive = 0;
        return status;
    }

    return DEAD_OBJECT;
}



//直接到驱动层看看
static void binder_transaction(struct binder_proc *proc,
                   struct binder_thread *thread,
                   struct binder_transaction_data *tr, int reply,
                   binder_size_t extra_buffers_size)
{
    int ret;
    struct binder_transaction *t;
    struct binder_work *tcomplete;
    binder_size_t *offp, *off_end, *off_start;
    binder_size_t off_min;
    u8 *sg_bufp, *sg_buf_end;
    struct binder_proc *target_proc = NULL;
    struct binder_thread *target_thread = NULL;
    struct binder_node *target_node = NULL;
    struct binder_transaction *in_reply_to = NULL;
    struct binder_transaction_log_entry *e;
    uint32_t return_error = 0;
    uint32_t return_error_param = 0;
    uint32_t return_error_line = 0;
    struct binder_buffer_object *last_fixup_obj = NULL;
    binder_size_t last_fixup_min_off = 0;
    struct binder_context *context = proc->context;
    int t_debug_id = atomic_inc_return(&binder_last_id);
    char *secctx = NULL;
    u32 secctx_sz = 0;

    e = binder_transaction_log_add(&binder_transaction_log);
    e->debug_id = t_debug_id;
    e->call_type = reply ? 2 : !!(tr->flags & TF_ONE_WAY);
    e->from_proc = proc->pid;
    e->from_thread = thread->pid;
    e->target_handle = tr->target.handle;//0
    e->data_size = tr->data_size;
    e->offsets_size = tr->offsets_size;
    e->context_name = proc->context->name;

    if (reply) {//false
    } else {//我们第一次来是在这里
        if (tr->target.handle) {//当前handle 不是0而是AMS handle
            struct binder_ref *ref;
            binder_proc_lock(proc);
            //找到 AMS 目标进程的binder_node和binder_ref  proc是客户端进程
            ref = binder_get_ref_olocked(proc, tr->target.handle,
                             true);
            if (ref) {
                target_node = binder_get_node_refs_for_txn(
                        ref->node, &target_proc,
                        &return_error);
            } else {
                return_error = BR_FAILED_REPLY;
            }
            binder_proc_unlock(proc);
        } else {
        }
    //创建binder_transaction
    t = kzalloc(sizeof(*t), GFP_KERNEL);
    //binder_work 创建结构体
    tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);
    

    //分配内存空间 用户空间 内核空间 物理内存
    t->buffer = binder_alloc_new_buf(&target_proc->alloc, tr->data_size,
        tr->offsets_size, extra_buffers_size,
        !reply && (t->flags & TF_ONE_WAY));

    t->buffer->transaction = t;//transaction = t 也就是他自己
    t->buffer->target_node = target_node;//target_node 就是我们的目标进程
    off_start = (binder_size_t *)(t->buffer->data +
                      ALIGN(tr->data_size, sizeof(void *)));
    offp = off_start;
    
    off_end = (void *)off_start + tr->offsets_size;
    sg_bufp = (u8 *)(PTR_ALIGN(off_end, sizeof(void *)));
    sg_buf_end = sg_bufp + extra_buffers_size -
        ALIGN(secctx_sz, sizeof(u64));
    off_min = 0;
    //遍历每一个binder对象
    for (; offp < off_end; offp++) {
        struct binder_object_header *hdr;
        size_t object_size = binder_validate_object(t->buffer, *offp);
        
        hdr = (struct binder_object_header *)(t->buffer->data + *offp);
        off_min = *offp + object_size;
        switch (hdr->type) {
        case BINDER_TYPE_BINDER://之前传递的type是binder_type_binder
        case BINDER_TYPE_WEAK_BINDER: {
            struct flat_binder_object *fp;
            
            fp = to_flat_binder_object(hdr);
            //进入binder_translate_binder thread为当前进程(客户端) 给自己的nodes挂一个InnerConnection 给AMS挂两个InnerConnection(refs_by_desc  refs_by_node)
            ret = binder_translate_binder(fp, t, thread);
            if (ret < 0) {
            }
        } break;
        }
    }
    //设置自己的binder_work 的type为BINDER_WORK_TRANSACTION_COMPLETE
    tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
    //设置target的work.type = BINDER_WORK_TRANSACTION
    t->work.type = BINDER_WORK_TRANSACTION;

    if (reply) {
    } else if (!(t->flags & TF_ONE_WAY)) {//true
        BUG_ON(t->buffer->async_transaction != 0);
        binder_inner_proc_lock(proc);
        //tcomplete加入到自己binder的todo队列
        binder_enqueue_deferred_thread_work_ilocked(thread, tcomplete);
        t->need_reply = 1;
        t->from_parent = thread->transaction_stack;
        thread->transaction_stack = t;
        binder_inner_proc_unlock(proc);
        //唤醒target_proc = AMS
        if (!binder_proc_transaction(t, target_proc, target_thread)) {
            binder_inner_proc_lock(proc);
            binder_pop_transaction_ilocked(thread, t);
            binder_inner_proc_unlock(proc);
            goto err_dead_proc_or_thread;
        }
    } else {
    }
    return;
}

static int binder_translate_binder(struct flat_binder_object *fp,
   struct binder_transaction *t,
   struct binder_thread *thread)
{
struct binder_node *node;
struct binder_proc *proc = thread->proc;
struct binder_proc *target_proc = t->to_proc;
struct binder_ref_data rdata;
int ret = 0;
       //从proc中来找binder_node 如果找不到就返回null 客户端进程是找不到的
node = binder_get_node(proc, fp->binder);
if (!node) {
node = binder_new_node(proc, fp);//创建新的binder_node 添加到自己进程
if (!node)
return -ENOMEM;
}
if (fp->cookie != node->cookie) {//这里不会走 肯定是相等的
}
//target_proc 是AMS  把InnerConnection 插入到AMS的refs_by_node
ret = binder_inc_ref_for_node(target_proc, node,
fp->hdr.type == BINDER_TYPE_BINDER,
&thread->todo, &rdata);
if (fp->hdr.type == BINDER_TYPE_BINDER)//修改type为BINDER_TYPE_HANDLE
fp->hdr.type = BINDER_TYPE_HANDLE;
else
fp->hdr.type = BINDER_TYPE_WEAK_HANDLE;
fp->binder = 0;
fp->handle = rdata.desc;// 0
fp->cookie = 0;

trace_binder_transaction_node_to_ref(t, node, &rdata);
return ret;
}




到了AMS端

我们先看看驱动层
static int binder_thread_read(struct binder_proc *proc,
      struct binder_thread *thread,
      binder_uintptr_t binder_buffer, size_t size,
      binder_size_t *consumed, int non_block)
{
if (wait_for_proc_work) {//true
if (!(thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
BINDER_LOOPER_STATE_ENTERED))) {
                  //在这里唤醒
wait_event_interruptible(binder_user_error_wait,
 binder_stop_on_user_error < 2);
}
binder_restore_priority(current, proc->default_priority);
}
    
if (non_block) {
if (!binder_has_work(thread, wait_for_proc_work))
ret = -EAGAIN;
} else {
ret = binder_wait_for_work(thread, wait_for_proc_work);
}

thread->looper &= ~BINDER_LOOPER_STATE_WAITING;

if (ret)
return ret;

while (1) {
                //找到需要处理的todo队列
if (!binder_worklist_empty_ilocked(&thread->todo))
list = &thread->todo;
else if (!binder_worklist_empty_ilocked(&proc->todo) &&
   wait_for_proc_work)
list = &proc->todo;
else {
binder_inner_proc_unlock(proc);
if (ptr - buffer == 4 && !thread->looper_need_return)
goto retry;
break;
}

if (end - ptr < sizeof(tr) + 4) {
binder_inner_proc_unlock(proc);
break;
}
        //从todo队列中取出来一个binder_work
w = binder_dequeue_work_head_ilocked(list);
if (binder_worklist_empty_ilocked(&thread->todo))
thread->process_todo = false;

switch (w->type) {
case BINDER_WORK_TRANSACTION: {
binder_inner_proc_unlock(proc);
            //从binder_work找到binder_transaction结构体 写入的data是TRANSACTION_bindIsolatedService data:Parcel reply flags 0
t = container_of(w, struct binder_transaction, work);
} break;
}

if (!t)
continue;

if (t->buffer->target_node) {//AMS
struct binder_node *target_node = t->buffer->target_node;
struct binder_priority node_prio;

trd->target.ptr = target_node->ptr;//设置binder 用户空间的地址
trd->cookie =  target_node->cookie;
node_prio.sched_policy = target_node->sched_policy;
node_prio.prio = target_node->min_priority;
binder_transaction_priority(current, t, node_prio,
    target_node->inherit_rt);
                        //设置cmd 是BR_TRANSACTION
cmd = BR_TRANSACTION;
} else {
}
trd->code = t->code;//TRANSACTION_bindIsolatedService
trd->flags = t->flags;
trd->sender_euid = from_kuid(current_user_ns(), t->sender_euid);
        //记录from 发起方 引用计数+1
t_from = binder_get_txn_from(t);
if (t_from) {
struct task_struct *sender = t_from->proc->tsk;//记录客户端的进程信息

trd->sender_pid =
task_tgid_nr_ns(sender,
task_active_pid_ns(current));
} else {
trd->sender_pid = 0;
}
//设置数据大小 偏移量 设置数据区的首地址 也就是通过内核空间和用户空间的偏移量算出来的
trd->data_size = t->buffer->data_size;
trd->offsets_size = t->buffer->offsets_size;
trd->data.ptr.buffer = (binder_uintptr_t)
((uintptr_t)t->buffer->data +
binder_alloc_get_user_buffer_offset(&proc->alloc));
trd->data.ptr.offsets = trd->data.ptr.buffer +
ALIGN(t->buffer->data_size,
    sizeof(void *));

tr.secctx = t->security_ctx;
if (t->security_ctx) {
            cmd = BR_TRANSACTION_SEC_CTX;
            trsize = sizeof(tr);
        }
       //回复给用户的响应码是BR_TRANSACTION 放到用户空间 也就是bwr.read_buffer在binder_ioctl_write_read
if (put_user(cmd, (uint32_t __user *)ptr)) {
if (t_from)
binder_thread_dec_tmpref(t_from);

binder_cleanup_transaction(t, "put_user failed",
   BR_FAILED_REPLY);

return -EFAULT;
}
ptr += sizeof(uint32_t);
  //把数据拷贝到用户空间 也就是bwr中去
if (copy_to_user(ptr, &tr, trsize)) {
if (t_from)
binder_thread_dec_tmpref(t_from);

binder_cleanup_transaction(t, "copy_to_user failed",
   BR_FAILED_REPLY);

return -EFAULT;
}
ptr += trsize;

trace_binder_transaction_received(t);
binder_stat_br(proc, thread, cmd);
binder_debug(BINDER_DEBUG_TRANSACTION,
     "%d:%d %s %d %d:%d, cmd %d size %zd-%zd ptr %016llx-%016llx\n",
     proc->pid, thread->pid,
     (cmd == BR_TRANSACTION) ? "BR_TRANSACTION" :
(cmd == BR_TRANSACTION_SEC_CTX) ?
     "BR_TRANSACTION_SEC_CTX" : "BR_REPLY",
     t->debug_id, t_from ? t_from->proc->pid : 0,
     t_from ? t_from->pid : 0, cmd,
     t->buffer->data_size, t->buffer->offsets_size,
     (u64)trd->data.ptr.buffer,
     (u64)trd->data.ptr.offsets);

if (t_from)//临时引用计数器+1
binder_thread_dec_tmpref(t_from);
t->buffer->allow_user_free = 1;
if (cmd != BR_REPLY && !(t->flags & TF_ONE_WAY)) {//true cmd = BR_TRANSACTION
binder_inner_proc_lock(thread->proc);
t->to_parent = thread->transaction_stack;//插入到事务栈中
t->to_thread = thread;//设置目标处理进程
thread->transaction_stack = t;
binder_inner_proc_unlock(thread->proc);
} else {
binder_free_transaction(t);
}
break;
}
return 0;
}

现在进入AMS的native层 也就是IPCThreadState.cpp  executeCommand

 case BR_TRANSACTION:
        {
            binder_transaction_data_secctx tr_secctx;
            binder_transaction_data& tr = tr_secctx.transaction_data;

            if (cmd == (int) BR_TRANSACTION_SEC_CTX) {
                result = mIn.read(&tr_secctx, sizeof(tr_secctx));
            } else {//从in里面获取数据也就是之前的BR_REPLY写入的数据
                result = mIn.read(&tr, sizeof(tr));
                tr_secctx.secctx = 0;
            }

            if (result != NO_ERROR) break;

            mIPCThreadStateBase->pushCurrentState(
                IPCThreadStateBase::CallState::BINDER);
            Parcel buffer;
            //写入数据到Parcel(buffer)中
            buffer.ipcSetDataReference(
                reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                tr.data_size,
                reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                tr.offsets_size/sizeof(binder_size_t), freeBuffer, this);
            mCallingPid = tr.sender_pid;
            mCallingSid = reinterpret_cast<const char*>(tr_secctx.secctx);
            mCallingUid = tr.sender_euid;
            mLastTransactionBinderFlags = tr.flags;

    
            Parcel reply;
            status_t error;
            if (tr.target.ptr) {//当前target.ptr就是AMS的用户空间地址
                if (reinterpret_cast<RefBase::weakref_type*>(
                        tr.target.ptr)->attemptIncStrong(this)) {//strong 引用计数+1
                        //拿到AMS的强指针 调用transact 也就是JavaBBinder
                    error = reinterpret_cast<BBinder*>(tr.cookie)->transact(tr.code, buffer,
                            &reply, tr.flags);
                    reinterpret_cast<BBinder*>(tr.cookie)->decStrong(this);
                } else {
                    error = UNKNOWN_TRANSACTION;
                }

            } else {
                error = the_context_object->transact(tr.code, buffer, &reply, tr.flags);
            }

            mIPCThreadStateBase->popCurrentState();
            
            if ((tr.flags & TF_ONE_WAY) == 0) {
                if (error < NO_ERROR) reply.setError(error);
                sendReply(reply, 0);
            } else {
                LOG_ONEWAY("NOT sending reply to %d!", mCallingPid);
            }

            mCallingPid = origPid;
            mCallingSid = origSid;
            mCallingUid = origUid;
            mStrictModePolicy = origStrictModePolicy;
            mLastTransactionBinderFlags = origTransactionBinderFlags;
            mWorkSource = origWorkSource;
            mPropagateWorkSource = origPropagateWorkSet;

        }

//BBinder的transact实现
status_t BBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    data.setDataPosition(0);

    status_t err = NO_ERROR;
    switch (code) {
        case PING_TRANSACTION:
            reply->writeInt32(pingBinder());
            break;
        default://调用OnTransact
            err = onTransact(code, data, reply, flags);
            break;
    }

    if (reply != nullptr) {
        reply->setDataPosition(0);
    }

    return err;
}


//JavaBBinder的onTransact
    status_t onTransact(
        uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags = 0) override
    {
        JNIEnv* env = javavm_to_jnienv(mVM);

        IPCThreadState* thread_state = IPCThreadState::self();
        const int32_t strict_policy_before = thread_state->getStrictModePolicy();
        //android/os/Binder 调用Binder的execTransact
        jboolean res = env->CallBooleanMethod(mObject, gBinderOffsets.mExecTransact,
            code, reinterpret_cast<jlong>(&data), reinterpret_cast<jlong>(reply), flags);
        return res != JNI_FALSE ? NO_ERROR : UNKNOWN_TRANSACTION;
    }

//Binder.java
   private boolean execTransact(int code, long dataObj, long replyObj,
            int flags) {
        final int callingUid = Binder.getCallingUid();
        final long origWorkSource = ThreadLocalWorkSource.setUid(callingUid);
        try {
            return execTransactInternal(code, dataObj, replyObj, flags, callingUid);
        } finally {
            ThreadLocalWorkSource.restore(origWorkSource);
        }
    }
    private boolean execTransactInternal(int code, long dataObj, long replyObj, int flags,
            int callingUid) {
        final BinderInternal.Observer observer = sObserver;
        final CallSession callSession =
                observer != null ? observer.callStarted(this, code, UNSET_WORKSOURCE) : null;
        Parcel data = Parcel.obtain(dataObj);
        Parcel reply = Parcel.obtain(replyObj);
        boolean res;
        final boolean tracingEnabled = Binder.isTracingEnabled();
        try {
            if (tracingEnabled) {
                final String transactionName = getTransactionName(code);
                Trace.traceBegin(Trace.TRACE_TAG_ALWAYS, getClass().getName() + ":"
                        + (transactionName != null ? transactionName : code));
            }
            //调用onTransact
            res = onTransact(code, data, reply, flags);
        } catch (RemoteException|RuntimeException e) {
        }
        checkParcel(this, code, reply, "Unreasonably large binder reply buffer");
        reply.recycle();
        data.recycle();
        StrictMode.clearGatheredViolations();
        return res;
    }
    
    //我们传递的code是TRANSACTION_bindIsolatedService
@Override public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException  
{
        case TRANSACTION_bindIsolatedService:
        {
          data.enforceInterface(descriptor);
          android.app.IApplicationThread _arg0;
          _arg0 = android.app.IApplicationThread.Stub.asInterface(data.readStrongBinder());
          android.os.IBinder _arg1;
          //读取出来caller
          _arg1 = data.readStrongBinder();
          android.content.Intent _arg2;
          if ((0!=data.readInt())) {
            _arg2 = android.content.Intent.CREATOR.createFromParcel(data);
          }
          else {
            _arg2 = null;
          }
          java.lang.String _arg3;
          _arg3 = data.readString();
          android.app.IServiceConnection _arg4;
          //读取出来InnerConnection 也就是BinderProxy 我们看看怎么读取的
          _arg4 = android.app.IServiceConnection.Stub.asInterface(data.readStrongBinder());
          int _arg5;
          _arg5 = data.readInt();
          java.lang.String _arg6;
          _arg6 = data.readString();
          java.lang.String _arg7;
          _arg7 = data.readString();
          int _arg8;
          _arg8 = data.readInt();
          //调用本地的bindIsolatedService 也就是AMS的
          int _result = this.bindIsolatedService(_arg0, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8);
          reply.writeNoException();
          reply.writeInt(_result);
          return true;
        }
}
//读取Binder
static jobject android_os_Parcel_readStrongBinder(JNIEnv* env, jclass clazz, jlong nativePtr)
{
    Parcel* parcel = reinterpret_cast<Parcel*>(nativePtr);
    if (parcel != NULL) {
        //拿到InnerConnection的BpBinder 包装成BinderProxy 返回
        return javaObjectForIBinder(env, parcel->readStrongBinder());
    }
    return NULL;
}

sp<IBinder> Parcel::readStrongBinder() const  
{  
sp<IBinder> val;
readNullableStrongBinder(&val);  
return val;  
}
  
status_t Parcel::readNullableStrongBinder(sp<IBinder>* val) const  
{  
return unflatten_binder(ProcessState::self(), *this, val);  
}
//获取binder数据
status_t unflatten_binder(const sp<ProcessState>& proc,
    const Parcel& in, sp<IBinder>* out)
{
    const flat_binder_object* flat = in.readObject(false);

    if (flat) {
        switch (flat->hdr.type) {
            case BINDER_TYPE_BINDER:
                *out = reinterpret_cast<IBinder*>(flat->cookie);
                return finish_unflatten_binder(nullptr, *flat, in);
            case BINDER_TYPE_HANDLE://我们已经修改了type 为BINDER_TYPE_HANDLE
                *out = proc->getStrongProxyForHandle(flat->handle);
                return finish_unflatten_binder(
                    static_cast<BpBinder*>(out->get()), *flat, in);
        }
    }
    return BAD_TYPE;
}
//我们传递的handle就是InnerConnection的handle
sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;
    AutoMutex _l(mLock);
    //创建handle_entry 结构体
    handle_entry* e = lookupHandleLocked(handle);

    if (e != nullptr) {
        IBinder* b = e->binder;//拿到InnerConnection
        if (b == nullptr || !e->refs->attemptIncWeak(this)) {
            if (handle == 0) {

                Parcel data;
                //ping一下
                status_t status = IPCThreadState::self()->transact(
                        0, IBinder::PING_TRANSACTION, data, nullptr, 0);
                if (status == DEAD_OBJECT)
                   return nullptr;
            }
            //创建BpBinder 也就是InnerConnection的BinderProxy 返回
            b = BpBinder::create(handle);
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }

    return result;
}


//到了AMS  这里的 IServiceConnection就是BinderProxy
 public int bindIsolatedService(IApplicationThread caller, IBinder token, Intent service,
            String resolvedType, IServiceConnection connection, int flags, String instanceName,
            String callingPackage, int userId) throws TransactionTooLargeException {
        enforceNotIsolatedCaller("bindService");
        
        if (instanceName != null) {//这里是null
        }

        synchronized(this) {
            return mServices.bindServiceLocked(caller, token, service,
                    resolvedType, connection, flags, instanceName, callingPackage, userId);
        }
    }
//到了ActiveServices
 int bindServiceLocked(IApplicationThread caller, IBinder token, Intent service,
            String resolvedType, final IServiceConnection connection, int flags,
            String instanceName, String callingPackage, final int userId)
            throws TransactionTooLargeException {
            //获取到当前进程的ProcessRecord
        final ProcessRecord callerApp = mAm.getRecordForAppLocked(caller);
        if (callerApp == null) {
        }

        ActivityServiceConnectionsHolder<ConnectionRecord> activity = null;
        if (token != null) {
        //获取到当前进程Stack的ActivityRecord 如果ConnectionsHolder为null会new一个新的ActivityServiceConnectionsHolder
            activity = mAm.mAtmInternal.getServiceConnectionsHolder(token);
            if (activity == null) {
                return 0;
            }
        }

        int clientLabel = 0;
        PendingIntent clientIntent = null;
        //是否系统应用
        final boolean isCallerSystem = callerApp.info.uid == Process.SYSTEM_UID;

        if (isCallerSystem) {//不走
        }

        if ((flags&Context.BIND_TREAT_LIKE_ACTIVITY) != 0) {//我们没有设置(认为持有一个activity，进程进入Activity的LRU，而不是常规的LRU，一般用于输入法 方便快捷的切换键盘)
         mAm.enforceCallingPermission(android.Manifest.permission.MANAGE_ACTIVITY_STACKS,
                    "BIND_TREAT_LIKE_ACTIVITY");
        }
    //topApp的安全策略 系统应用才可以
        if ((flags & Context.BIND_SCHEDULE_LIKE_TOP_APP) != 0 && !isCallerSystem) {
        }
        //系统应用才可以 允许serivce绕过白名单管理
        if ((flags & Context.BIND_ALLOW_WHITELIST_MANAGEMENT) != 0 && !isCallerSystem) {
        }
    //允许Service开启后台activity
        if ((flags & Context.BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS) != 0) {
            mAm.enforceCallingPermission(
                    android.Manifest.permission.START_ACTIVITIES_FROM_BACKGROUND,
                    "BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS");
        }

        final boolean callerFg = callerApp.setSchedGroup != ProcessList.SCHED_GROUP_BACKGROUND;
        final boolean isBindExternal = (flags & Context.BIND_EXTERNAL_SERVICE) != 0;
        final boolean allowInstant = (flags & Context.BIND_ALLOW_INSTANT) != 0;
        //得到ServiceRecord对象 同startService
        ServiceLookupResult res =
            retrieveServiceLocked(service, instanceName, resolvedType, callingPackage,
                    Binder.getCallingPid(), Binder.getCallingUid(), userId, true,
                    callerFg, isBindExternal, allowInstant);
        //拿到ServiceRecord
        ServiceRecord s = res.record;

        boolean permissionsReviewRequired = false;
        //权限检查
        if (mAm.getPackageManagerInternalLocked().isPermissionsReviewRequired(
                s.packageName, s.userId)) {

            permissionsReviewRequired = true;

            final ServiceRecord serviceRecord = s;
            final Intent serviceIntent = service;

            RemoteCallback callback = new RemoteCallback(
                    new RemoteCallback.OnResultListener() {
                @Override
                public void onResult(Bundle result) {
                    synchronized(mAm) {
                        final long identity = Binder.clearCallingIdentity();
                        try {
                            if (!mPendingServices.contains(serviceRecord)) {
                                return;
                            }
                            if (!mAm.getPackageManagerInternalLocked()
                                    .isPermissionsReviewRequired(
                                            serviceRecord.packageName,
                                            serviceRecord.userId)) {
                                try {
                                    bringUpServiceLocked(serviceRecord,
                                            serviceIntent.getFlags(),
                                            callerFg, false, false);
                                } catch (RemoteException e) {
                                }
                            } else {
                                unbindServiceLocked(connection);
                            }
                        } finally {
                            Binder.restoreCallingIdentity(identity);
                        }
                    }
                }
            });
            //用户review权限是否允许
            final Intent intent = new Intent(Intent.ACTION_REVIEW_PERMISSIONS);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                    | Intent.FLAG_ACTIVITY_MULTIPLE_TASK
                    | Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
            intent.putExtra(Intent.EXTRA_PACKAGE_NAME, s.packageName);
            intent.putExtra(Intent.EXTRA_REMOTE_CALLBACK, callback);

            mAm.mHandler.post(new Runnable() {
                @Override
                public void run() {
                    mAm.mContext.startActivityAsUser(intent, new UserHandle(userId));
                }
            });
        }

        final long origId = Binder.clearCallingIdentity();

        try {
        //把service从mRestartingServices中移除
            if (unscheduleServiceRestartLocked(s, callerApp.info.uid, false)) {
            }

            if ((flags&Context.BIND_AUTO_CREATE) != 0) {//我们传递的是BIND_AUTO_CREATE
                s.lastActivity = SystemClock.uptimeMillis();//记录时间
                if (!s.hasAutoCreateConnections()) {//目前没有 所以这里是true
                    ServiceState stracker = s.getTracker();
                    if (stracker != null) {
                        stracker.setBound(true, mAm.mProcessStats.getMemFactorLocked(),
                                s.lastActivity);
                    }
                }
            }

            mAm.startAssociationLocked(callerApp.uid, callerApp.processName,
                    callerApp.getCurProcState(), s.appInfo.uid, s.appInfo.longVersionCode,
                    s.instanceName, s.processName);
            mAm.grantEphemeralAccessLocked(callerApp.userId, service,
                    UserHandle.getAppId(s.appInfo.uid), UserHandle.getAppId(callerApp.uid));
            //根据service拿到AppBindRecord 把当前的service存入bindings
            AppBindRecord b = s.retrieveAppBindingLocked(service, callerApp);
            //创建ConnectionRecord对象
            ConnectionRecord c = new ConnectionRecord(b, activity,
                    connection, flags, clientLabel, clientIntent,
                    callerApp.uid, callerApp.processName, callingPackage);
            //调用connection的asBinder 也就是服务端的this(Stub)
            IBinder binder = connection.asBinder();
            //ServiceRecord添加Connection
            s.addConnection(binder, c);
            //AppBindRecord 添加当前Connection
            b.connections.add(c);
            if (activity != null) {//当前进程的ActivityServiceConnectionsHolder添加Connection
                activity.addConnection(c);
            }
            //b的客户端进程信息添加Connection
            b.client.connections.add(c);
            c.startAssociationIfNeeded();
            //从当前的mServiceConnections中获取binder对应的clist 第一次是没有的
            ArrayList<ConnectionRecord> clist = mServiceConnections.get(binder);
            if (clist == null) {
            //创建并put进去
                clist = new ArrayList<>();
                mServiceConnections.put(binder, clist);
            }
            //clist添加当前ConnectionRecord
            clist.add(c);
//asdasd
            if ((flags&Context.BIND_AUTO_CREATE) != 0) {
                s.lastActivity = SystemClock.uptimeMillis();
                //调用bringUpServiceLocked开启进程 反射创建Service 调用onCreate  我们走没有进程的逻辑
                if (bringUpServiceLocked(s, service.getFlags(), callerFg, false,
                        permissionsReviewRequired) != null) {
                    return 0;
                }
            }

            if (s.app != null) {
                if ((flags&Context.BIND_TREAT_LIKE_ACTIVITY) != 0) {
                    s.app.treatLikeActivity = true;
                }
                if (s.whitelistManager) {
                    s.app.whitelistManager = true;
                }
                //更新进程的优先级
                mAm.updateLruProcessLocked(s.app,
                        (callerApp.hasActivitiesOrRecentTasks() && s.app.hasClientActivities())
                                || (callerApp.getCurProcState() <= ActivityManager.PROCESS_STATE_TOP
                                        && (flags & Context.BIND_TREAT_LIKE_ACTIVITY) != 0),
                        b.client);
                mAm.updateOomAdjLocked(OomAdjuster.OOM_ADJ_REASON_BIND_SERVICE);
            }

            if (s.app != null && b.intent.received) {//进程运行了 直接调用c.conn.connected
                try {
                //调用c.conn.connected也就是InnerConnection
                    c.conn.connected(s.name, b.intent.binder, false);
                } catch (Exception e) {
                }
                if (b.intent.apps.size() == 1 && b.intent.doRebind) {
                    requestServiceBindingLocked(s, b.intent, callerFg, true);
                }
            } else if (!b.intent.requested) {
            //还没有bind 进行 bind
                requestServiceBindingLocked(s, b.intent, callerFg, false);
            }

            getServiceMapLocked(s.userId).ensureNotStartingBackgroundLocked(s);

        } finally {
            Binder.restoreCallingIdentity(origId);
        }

        return 1;
    }
创建完进程之后，在AMS端realStartServiceLocked 告诉客户端 反射创建执行完onCreate之后，会调用requestServiceBindingsLocked



    private final void realStartServiceLocked(ServiceRecord r,
            ProcessRecord app, boolean execInFg) throws RemoteException {
        try {
            app.thread.scheduleCreateService(r, r.serviceInfo,
                    mAm.compatibilityInfoForPackage(r.serviceInfo.applicationInfo),
                    app.getReportedProcState());
            r.postNotification();
            created = true;
        } catch (DeadObjectException e) {
        } finally {
        }
        requestServiceBindingsLocked(r, execInFg);

        updateServiceClientActivitiesLocked(app, null, true);

        //注意此时的callStart = false 所以不会添加 也就不会执行onStartCommand
        if (r.startRequested && r.callStart && r.pendingStarts.size() == 0) {
            r.pendingStarts.add(new ServiceRecord.StartItem(r, false, r.makeNextStartId(),
                    null, null, 0));
        }

        sendServiceArgsLocked(r, execInFg, true);

    }


   private final void requestServiceBindingsLocked(ServiceRecord r, boolean execInFg)
            throws TransactionTooLargeException {
        for (int i=r.bindings.size()-1; i>=0; i--) {//之前已经存入到bindings了 在创建AppBindRecord的时候
            IntentBindRecord ibr = r.bindings.valueAt(i);
            if (!requestServiceBindingLocked(r, ibr, execInFg, false)) {
                break;
            }
        }
    }
   

      private final boolean requestServiceBindingLocked(ServiceRecord r, IntentBindRecord i,
            boolean execInFg, boolean rebind) throws TransactionTooLargeException {
        if ((!i.requested || rebind) && i.apps.size() > 0) {
            try {
            //埋炸弹
                bumpServiceExecutingLocked(r, execInFg, "bind");
                r.app.forceProcessStateUpTo(ActivityManager.PROCESS_STATE_SERVICE);
                //执行bind
                r.app.thread.scheduleBindService(r, i.intent.getIntent(), rebind,
                        r.app.getReportedProcState());
                if (!rebind) {
                    i.requested = true;
                }
                i.hasBound = true;
                i.doRebind = false;
            } catch (TransactionTooLargeException e) {
                final boolean inDestroying = mDestroyingServices.contains(r);
                serviceDoneExecutingLocked(r, inDestroying, inDestroying);
                throw e;
            } catch (RemoteException e) {
            }
        }
        return true;
    }

//到了应用进程端
     public final void scheduleBindService(IBinder token, Intent intent,
                boolean rebind, int processState) {
            updateProcessState(processState, false);
            BindServiceData s = new BindServiceData();
            s.token = token;
            s.intent = intent;
            s.rebind = rebind;
            sendMessage(H.BIND_SERVICE, s);
        }
        
        
     case BIND_SERVICE:
          handleBindService((BindServiceData)msg.obj);
          break;
          
        private void handleBindService(BindServiceData data) {
        //获取到Service 不是null了 反射创建完成之后就会存入进来
        Service s = mServices.get(data.token);
        if (s != null) {
            try {
                data.intent.setExtrasClassLoader(s.getClassLoader());
                data.intent.prepareToEnterProcess();
                try {
                    if (!data.rebind) {//rebind=false 也就会到这里来 执行AMS的publishService
                        IBinder binder = s.onBind(data.intent);
                        //进行服务的发布 发布到哪里了呢？对了AMS
                        ActivityManager.getService().publishService(
                                data.token, data.intent, binder);
                    } else {
                        s.onRebind(data.intent);
                        ActivityManager.getService().serviceDoneExecuting(
                                data.token, SERVICE_DONE_EXECUTING_ANON, 0, 0);
                    }
                } catch (RemoteException ex) {
                    throw ex.rethrowFromSystemServer();
                }
            } catch (Exception e) {
                if (!mInstrumentation.onException(s, e)) {
                    throw new RuntimeException(
                            "Unable to bind to service " + s
                            + " with " + data.intent + ": " + e.toString(), e);
                }
            }
        }
    }
    
       @Override public void publishService(android.os.IBinder token, android.content.Intent intent, android.os.IBinder service) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeStrongBinder(token);
          if ((intent!=null)) {
            _data.writeInt(1);
            intent.writeToParcel(_data, 0);
          }
          else {
            _data.writeInt(0);
          }
          //service写入Parcel
          _data.writeStrongBinder(service);
          //流程同上 因为这个时候已经启动了服务端进程 是在服务端进程调用的 也就是说会在自己nodes存入server  也会在目标进程(AMS)的refs_by_node 和refs_by_desc 存入server
          boolean _status = mRemote.transact(Stub.TRANSACTION_publishService, _data, _reply, 0);
          if (!_status && getDefaultImpl() != null) {
            getDefaultImpl().publishService(token, intent, service);
            return;
          }
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }   
    
 
进入到AMS  

    case TRANSACTION_publishService:
        {
          data.enforceInterface(descriptor);
          android.os.IBinder _arg0;
          _arg0 = data.readStrongBinder();
          android.content.Intent _arg1;
          if ((0!=data.readInt())) {
            _arg1 = android.content.Intent.CREATOR.createFromParcel(data);
          }
          else {
            _arg1 = null;
          }
          android.os.IBinder _arg2;
          //读取到server的BpBinder
          _arg2 = data.readStrongBinder();
          this.publishService(_arg0, _arg1, _arg2);
          reply.writeNoException();
          return true;
        }
     
    public void publishService(IBinder token, Intent intent, IBinder service) {
        synchronized(this) {
            mServices.publishServiceLocked((ServiceRecord)token, intent, service);
        }
    }
    
    //ActiveServices
  void publishServiceLocked(ServiceRecord r, Intent intent, IBinder service) {
        final long origId = Binder.clearCallingIdentity();
        try {
            if (r != null) {
                Intent.FilterComparison filter
                        = new Intent.FilterComparison(intent);
                IntentBindRecord b = r.bindings.get(filter);//获取到IntentBindRecord 之前存入了
                if (b != null && !b.received) {
                    b.binder = service;
                    b.requested = true;
                    b.received = true;
                    ArrayMap<IBinder, ArrayList<ConnectionRecord>> connections = r.getConnections();//之前存入了
                    for (int conni = connections.size() - 1; conni >= 0; conni--) {
                        ArrayList<ConnectionRecord> clist = connections.valueAt(conni);
                        for (int i=0; i<clist.size(); i++) {
                            ConnectionRecord c = clist.get(i);
                            if (!filter.equals(c.binding.intent.intent)) {
                                continue;
                            }
                            try {
                            //如果是当前的就调用c.conn.connected c就是ConnectionRecord 他的conn就是InnerConnection
                                c.conn.connected(r.name, service, false);
                            } catch (Exception e) {
                            }
                        }
                    }
                }
            //完成之后 拆除炸弹
                serviceDoneExecutingLocked(r, mDestroyingServices.contains(r), false);
            }
        } finally {
            Binder.restoreCallingIdentity(origId);
        }
    }
    //IserviceConnection.java
          @Override public void connected(android.content.ComponentName name, android.os.IBinder service, boolean dead) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          if ((name!=null)) {
            _data.writeInt(1);
            name.writeToParcel(_data, 0);
          }
          else {
            _data.writeInt(0);
          }
          //写入BpBinder
          _data.writeStrongBinder(service);
          _data.writeInt(((dead)?(1):(0)));
          //调用InnerConnection的BpBinder transact方法传入TRANSACTION_connected 注意当前在AMS进程 refs_by_desc 和refs_by_node 已经存了InnerConnection了 可以直接调用,所以到客户端进城之后会把server的BpBinder 挂在refs_by_desc 和refs_by_node 
          boolean _status = mRemote.transact(Stub.TRANSACTION_connected, _data, null, android.os.IBinder.FLAG_ONEWAY);
          if (!_status && getDefaultImpl() != null) {
            getDefaultImpl().connected(name, service, dead);
            return;
          }
        }
        finally {
          _data.recycle();
        }
      }
    
    
    private static class InnerConnection extends IServiceConnection.Stub {
            @UnsupportedAppUsage
            final WeakReference<LoadedApk.ServiceDispatcher> mDispatcher;

            InnerConnection(LoadedApk.ServiceDispatcher sd) {
                mDispatcher = new WeakReference<LoadedApk.ServiceDispatcher>(sd);
            }

            public void connected(ComponentName name, IBinder service, boolean dead)
                    throws RemoteException {
                LoadedApk.ServiceDispatcher sd = mDispatcher.get();
                if (sd != null) {
                //sd就是ServiceDispatcher
                    sd.connected(name, service, dead);
                }
            }
        }

   public void connected(ComponentName name, IBinder service, boolean dead) {
            if (mActivityExecutor != null) {//这里是null
                mActivityExecutor.execute(new RunConnection(name, service, 0, dead));
            } else if (mActivityThread != null) {//走这里
                mActivityThread.post(new RunConnection(name, service, 0, dead));
            } else {
                doConnected(name, service, dead);
            }
        }


   private final class RunConnection implements Runnable {
            RunConnection(ComponentName name, IBinder service, int command, boolean dead) {
                mName = name;
                mService = service;
                mCommand = command;
                mDead = dead;
            }

            public void run() {
                if (mCommand == 0) {//这里传入的是0
                    doConnected(mName, mService, mDead);
                } else if (mCommand == 1) {
                    doDeath(mName, mService);
                }
            }

            final ComponentName mName;
            final IBinder mService;
            final int mCommand;
            final boolean mDead;
        }



     public void doConnected(ComponentName name, IBinder service, boolean dead) {
            ServiceDispatcher.ConnectionInfo old;
            ServiceDispatcher.ConnectionInfo info;

            synchronized (this) {
                if (mForgotten) {
                    return;
                }
    
            if (dead) {
                mConnection.onBindingDied(name);
            }
            if (service != null) {//调用了onServiceConnected mConnection就是我们创建的ServiceConnection接口实现 也就到我们定义的方法里面了
                mConnection.onServiceConnected(name, service);
            } else {
            }
        }
```

代码比较多文字总结下，首先`ServiceConnection`是无法跨进程通信的，所以在`LoadedApk.java`中帮我们封装了`InnerConnection` 它继承自Stub，也就是native层的JavaBBinder,然后通过`bindIsolatedService`将`sd(InnerConnection)`写入到Parcel中，调用`AMS(BinderProxy)`的`transact`进行IPC 通信，把InnerConnection存入到客户端的`nodes`中,以及给`AMS`的`refs_by_node`和`refs_by_desc`挂上`InnerConnection` 接着唤醒`AMS`,唤醒之后调用`onTransact`读取到传过来的`sd`并且包装成`BpBinder(BinderProxy)`返回,这样AMS就在`bindIsolatedService`的时候拿到了InnerConnection的BpBinder，接着AMS 通过`pkms`通过intent查找到服务端进程,创建`AppBindRecord`和`ConnectionRecord`,接着调用`bringUpServiceLocked` 看是否开启进程 如果没有开启就先开启进程，开启了进程之后 调用`realStartServiceLocked` 也埋了炸弹 接着调用 `app.thread.scheduleCreateService`通知客户端创建反射`Service、Context` 调用`onCreate` 把`Service`存入到`mService`中,调用`serviceDoneExecutingLocked`把炸弹拆除，再调用`requestServiceBindingsLocked` 去执行`r.app.thread.scheduleBindService` 执行服务端的onBind函数拿到了返回的`Binder`再调用AMS的`publishService`把服务发布到AMS（在服务端的进程创建`binder_proc(server)` 在AMS的`refs_by_node` 和`refs_by_desc 挂上server）`,`AMS`的`onTransact` 中读取到服务端返回的`server(Binder)`包装成`BpBinder`，然后调用`c.conn.connected(从refs_by_desc 和 refs_by_node 找到InnerConnection)`调用connect 所以到了客户端进程会把server的BpBinder 挂在`refs_by_desc`和`refs_by_node`上边。 之后拆除炸弹 以上整体的流程就走完了,包括客户端进程如何存入InnerConnection的，AMS是如何存储InnerConnection的，以及服务端是怎么创建server的并且把server挂在AMS上的，以及AMS是如何把server给到客户端的。

## 总结

## 文字总结

### 1.startService的总结

startService会调用到ContextImpl的startService，它会直接调用AMS的startService。在`AMS`这里会先检查Service是否可以执行(常驻内存、蓝牙、电源白名单允许直接启动服务)，接着调用`bringUpServiceLocked` 判断是否需要隔离进程如果非隔离 就看是否已经启动进程 执行`realStartServiceLocked`，否则是隔离进程 直接开启新进程。开启成功之后会将ServiceRecord添加到`mPendingServices`中去。进程创建之后，会调用`AMS`的`attachApplication` 接着处理`service(ActiveService)`，之前创建之后会添加到`mPendingServices`中，现在继续处理调用`realStartServiceLocked`来开启Service，在开启的过程中会埋入一个炸弹(给Handler发送一个`SERVICE_TIMEOUT_MSG`) 如果超时未处理会弹出`ANR`，然后调用`app.thread.scheduleCreateService`通知客户端创建反射`Service、Context` 调用`onCreate` 把`Service`存入到`mService`中，调用`AMS`的`serviceDoneExecuting`进行炸弹的拆除。 然后再埋炸弹 调用`app.thread.scheduleServiceArgs` 调用`service.onStartCommand`再通知AMS 拆除炸弹。 这样Service就运行起来了。

### 2.stopService的总结

stop的流程和start的流程差不多，需要注意的是`stop`只是从`map中移除了`，我们需要自己释放资源。

### 3.bindService的总结

首先`ServiceConnection`是无法跨进程通信的，所以在`LoadedApk.java`中帮我们封装了`InnerConnection` 它继承自Stub，也就是native层的JavaBBinder,然后通过`bindIsolatedService`将`sd(InnerConnection)`写入到Parcel中，调用`AMS(BinderProxy)`的`transact`进行IPC 通信，把InnerConnection存入到客户端的`nodes`中,以及给`AMS`的`refs_by_node`和`refs_by_desc`挂上`InnerConnection` 接着唤醒`AMS`,唤醒之后调用`onTransact`读取到传过来的`sd`并且包装成`BpBinder(BinderProxy)`返回,这样AMS就在`bindIsolatedService`的时候拿到了InnerConnection的BpBinder，接着AMS 通过`pkms`通过intent查找到服务端进程,创建`AppBindRecord`和`ConnectionRecord`,接着调用`bringUpServiceLocked` 看是否开启进程 如果没有开启就先开启进程，开启了进程之后 调用`realStartServiceLocked` 也埋了炸弹 接着调用 `app.thread.scheduleCreateService`通知客户端创建反射`Service、Context` 调用`onCreate` 把`Service`存入到`mService`中,调用`serviceDoneExecutingLocked`把炸弹拆除，再调用`requestServiceBindingsLocked` 去执行`r.app.thread.scheduleBindService` 执行服务端的onBind函数拿到了返回的`Binder`再调用AMS的`publishService`把服务发布到AMS（在服务端的进程创建`binder_proc(server)` 在AMS的`refs_by_node` 和`refs_by_desc 挂上server）`,`AMS`的`onTransact` 中读取到服务端返回的`server(Binder)`包装成`BpBinder`，然后调用`c.conn.connected(从refs_by_desc 和 refs_by_node 找到InnerConnection)`调用connect 所以到了客户端进程会把server的BpBinder 挂在`refs_by_desc`和`refs_by_node`上边。再调用`Servcie.onServiceConnected` 之后拆除炸弹。

## 图片总结

### 1.startService

![startService.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ccd40d59cafa4e54993944e4925a2793~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

### 2.bindService

![bindService.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/eede36b738e64b839ea4ab02893eee4d~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

### 3.Binder存储情况

![bindservice.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3830a8922ae04652bca2aad4f4bed2dd~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 面试题

Q1:Service的两种启动方式对Service生命周期有什么影响？

A1:我们可以从源码角度来阐述为什么start不执行onbind(bindingssize为0)，而bindService(bindings有值)。bindService为什么不执行onStartCommand？(callStart = false 所以pendingStarts没有添加ServiceRecord.StartItem)。从源码上来讲格调立马就上来了，有木有？

Q2:Service的启动流程

A2:可以参考上边的两个图，最好再阐述下bindService的InnerConnection以及Binder是如何存储的，怎么执行的。

Q3:Service的onStartCommand返回值是什么意思？

A3:这里我们没有跟源码 简单说下吧 `START_STICKY(Service被kill service会重启，不会保留之前传递过来的intent)` `START_NOT_STICKY(不会重启该Service)` `START_REDELIVER_INTENT(service 被杀死之后 重启 并且保留之前的intent)`

Q4:我们可以利用Service做一系列的优化操作

A4:例如视频中讲解的，优化进程的启动，提升用户体验。

在线视频:

[www.bilibili.com/video/BV1Bh…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1Bh411E7o7%2F%3Fspm_id_from%3D333.999.0.0%26vd_source%3D689a2ec078877b4a664365bdb60362d3 "https://www.bilibili.com/video/BV1Bh411E7o7/?spm_id_from=333.999.0.0&vd_source=689a2ec078877b4a664365bdb60362d3")