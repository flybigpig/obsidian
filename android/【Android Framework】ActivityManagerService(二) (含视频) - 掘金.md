## 回顾

先简单的回顾一下，在Android中，第一个开启的是`init`进程，它解析了`init.rc`文件，启动各种`service`:`zygote`,`surfaceflinger`,`service_manager`。接着就讲了`Zygote`，Zygote就是一个孵化器，它开启了`system_server`以及开启了`ZygoteServer`用来接收客户端的请求，当客户端请求来了之后就会`fork`出来子进程，并且初始化`binder` 和`进程信息`，为了加速`Zygote`还会预加载一些`class`和`Drawable`、`color`等系统资源。接下来讲了`system_server`，它是系统启动管理`service`的`入口`，比如`AMS`、`PMS`、`WMS`等等，它加载了`framework-res.apk`,接着调用`startBootstrapService`,`startCoreService`,`startOtherService`开启非常多的服务，还开启了`WatchDog`，来监控service。接着讲了`service_manager`，他是一个独立的进程，它存储了系统各种服务的`Binder`，我们经常通过`ServiceMananger`来获取，其中还详细说了`Binder`机制，`C/S`架构，大家要记住`客户端`、`Binder`、`Server`三端的工作流程。之后讲了`Launcher`，它由`system_server`启动，通过`LauncherModel`进行`Binder`通信 通过`PMS`来查询所有的应用信息，然后绑定到`RecyclerView`中，它的点击事件是通过`ItemClickHandler`来处理。接着讲了`AMS`是如何开启应用进程的,首先我们从`Launcher`的点击开始，调用到`Activity`的`startActivity`函数，通过`Instrumentation`的`execStartActivity`经过两次`IPC`(1.通过ServiceManager获取到ATMS 2.调用ATMS的startActivity) 调用到`AMS`端在AMS端进行了一系列的信息处理，会判断进程是否存在，`没有存在的话就会开启进程(通过Socket，给ZygoteServer发送信息)`,传入`entryPoint`为`ActivityThread`,通过`Zygote`来`fork`出来子进程（应用进程）调用`ActivityThread.main`，应用进程创建之后会调用到`AMS`，由`AMS`来`attachApplication`存储进程信息,然后告诉`客户端`，让客户端来创建`Application`，并在客户端创建成功之后 继续执行开启`Activity`的流程。客户端接收到`AMS`的数据之后会创建`loadedApk`,`Instrumentation` 以及`Application调用attach(attachBaseContext)`，调用`Instrumentation`的`callApplicationOncreate`执行`Application`的`Oncreate`周期。

具体的细节可以参考之前写的文章和视频：

[【Android FrameWork】第一个启动的程序--init](https://juejin.cn/post/7213733567606685753 "https://juejin.cn/post/7213733567606685753")

[【Android FrameWork】Zygote](https://juejin.cn/post/7214068367607119933 "https://juejin.cn/post/7214068367607119933")

[【Android FrameWork】SystemServer](https://juejin.cn/post/7214493929566945340 "https://juejin.cn/post/7214493929566945340")

[【Android FrameWork】ServiceManager(一)](https://juejin.cn/post/7216537771448598585#heading-4 "https://juejin.cn/post/7216537771448598585#heading-4")

[【Android FrameWork】ServiceManager(二)](https://juejin.cn/post/7216536069285675045#heading-9 "https://juejin.cn/post/7216536069285675045#heading-9")

[【Android Framework】Launcher3](https://juejin.cn/post/7218129062744227896#heading-1 "https://juejin.cn/post/7218129062744227896#heading-1")

[【Android Framework】ActivityManagerService(一)](https://juejin.cn/post/7219130999685808188 "https://juejin.cn/post/7219130999685808188")

## 介绍

之前我们介绍了`AMS`是如何开启进程的，这次我们就接着之前的内容往下走，看看在创建`Application`之后，是如何开启`Activity`的。

## 正文

1.  Activity的启动 之前讲了进程的启动，现在我们来看看进程启动之后是如何启动`Activity`的。首先让我们回到`AMS`，当`ActivityThread`创建之后会通知`AMS`进行`attachApplication`。

```

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
```

调用`mAtmInternal`的`attachApplication`。

```
public boolean attachApplication(WindowProcessController wpc) throws RemoteException {
    synchronized (mGlobalLockWithoutBoost) {
        return mRootActivityContainer.attachApplication(wpc);
    }
}



boolean attachApplication(WindowProcessController app) throws RemoteException {
    //获取到进程名 也就是包名
    final String processName = app.mName;
    boolean didSomething = false;
    for (int displayNdx = mActivityDisplays.size() - 1; displayNdx >= 0; --displayNdx) {
        final ActivityDisplay display = mActivityDisplays.get(displayNdx);
        final ActivityStack stack = display.getFocusedStack();
        if (stack != null) {
            //从mTaskHistory后往前找activity 只有我们之前insert的Activity
            stack.getAllRunningVisibleActivitiesLocked(mTmpActivityList);
            //从前台stack栈顶获取没有finsish 的Activity
            final ActivityRecord top = stack.topRunningActivityLocked();
            final int size = mTmpActivityList.size();
            for (int i = 0; i < size; i++) {
                final ActivityRecord activity = mTmpActivityList.get(i);
                //如果Activity 没运行 并且uid一致  进程名一致
                if (activity.app == null && app.mUid == activity.info.applicationInfo.uid
                        && processName.equals(activity.processName)) {
                    try {
                    //开启activity
                        if (mStackSupervisor.realStartActivityLocked(activity, app,
                                top == activity /* andResume */, true /* checkConfig */)) {
                            didSomething = true;
                        }
                    } catch (RemoteException e) {
                    }
                }
            }
        }
    }
    if (!didSomething) {
        ensureActivitiesVisible(null, 0, false /* preserve_windows */);
    }
    return didSomething;
}
```

调用了`RootActivityContainer.java`的`attachApplication`，进行一些信息判断，然后执行`StackSupervisor`的`realStartActivityLocked`

```
//WindowProcessController 用来和AMS的ProcessRecord进行通信，当ProcessRecord中对应应用进程做出修改之后通过他来和WM进行通信
boolean realStartActivityLocked(ActivityRecord r, WindowProcessController proc,
        boolean andResume, boolean checkConfig) throws RemoteException {
    //判断是否都暂停成功
    if (!mRootActivityContainer.allPausedActivitiesComplete()) {
        return false;
    }
    //根据activity 获取到taskRecord
    final TaskRecord task = r.getTaskRecord();
    //获取到ActivityStack
    final ActivityStack stack = task.getStack();

    //resume引用++
    beginDeferResume();

    try {
        r.startFreezingScreenLocked(proc, 0);
        r.startLaunchTickingLocked();
        //设置WPC
        r.setProcess(proc);
        if (andResume && !r.canResumeByCompat()) {
            andResume = false;
        }
          //launchCount++ 记录启动次数
        r.launchCount++;
        //记录启动时间
        r.lastLaunchTime = SystemClock.uptimeMillis();
        //添加activity到WPC
        proc.addActivityIfNeeded(r);
        try {
            //创建ClientTransaction 用来和客户端通信 客户端收到后执行对应的事务
            final ClientTransaction clientTransaction = ClientTransaction.obtain(
            //client为当前应用进程 以及apptoken
                    proc.getThread(), r.appToken);
            //获取到DisplayContent 屏幕显示设备
            final DisplayContent dc = r.getDisplay().mDisplayContent;
            //添加callback 这个会触发onCreate
            clientTransaction.addCallback(LaunchActivityItem.obtain(new Intent(r.intent),
                    System.identityHashCode(r), r.info,
                    mergedConfiguration.getGlobalConfiguration(),
                    mergedConfiguration.getOverrideConfiguration(), r.compat,
                    r.launchedFromPackage, task.voiceInteractor, proc.getReportedProcState(),
                    r.icicle, r.persistentState, results, newIntents,
                    dc.isNextTransitionForward(), proc.createProfilerInfoIfNeeded(),
                            r.assistToken));
             //创建ActivityLifecycleItem 决定执行resume还是pause
            final ActivityLifecycleItem lifecycleItem;
            if (andResume) {//这里为true
            //设置为on_resume 
                lifecycleItem = ResumeActivityItem.obtain(dc.isNextTransitionForward());
            } else {
                lifecycleItem = PauseActivityItem.obtain();
            }
            //设置状态请求为resume
            clientTransaction.setLifecycleStateRequest(lifecycleItem);    
 //执行事务           mService.getLifecycleManager().scheduleTransaction(clientTransaction);

        } catch (RemoteException e) {
        }
    } finally {
        endDeferResume();
    }

    r.launchFailed = false;
    if (stack.updateLRUListLocked(r)) {
    }
    if (andResume && readyToResume()) {
        stack.minimalResumeActivityLocked(r);
    } else {
        r.setState(PAUSED, "realStartActivityLocked");
    }
    //oom相关 更新进程的状态信息
    proc.onStartActivity(mService.mTopProcessState, r.info);
    if (mRootActivityContainer.isTopDisplayFocusedStack(stack)) {
        mService.getActivityStartController().startSetupActivity();
    }
    //更新关心的服务
    if (r.app != null) {
        r.app.updateServiceConnectionActivities();
    }

    return true;
}
```

在`realStartActivityLocked`中 判断Activity是否都暂停成功，并且创建`ClientTransaction`添加`LaunchActivityItem`用来和客户端通信。我们看看是如何执行的任务。 首先说明下`ClientTransactionHandler`，它是用来处理`AMS`和`Activity`的整个生命周期的类，而且`ActivityThread`继承自它。 `ClientTransaction`是一系列客户端处理的事务，客户端取出事务执行，例如`LaunchActivityItem`、`ResumeActivityItem`等。`ClientLifecycleManager`是客户端管理生命周期执行。

所以在AMS设置完信息之后，调用`ClientLifecycleManager`的`scheduleTransaction`

```
void scheduleTransaction(ClientTransaction transaction) throws RemoteException {
    final IApplicationThread client = transaction.getClient();
    transaction.schedule();
    if (!(client instanceof Binder)) {
        transaction.recycle();
    }
}


文件目录:/frameworks/base/core/java/android/app/servertransaction/ClientTransaction.java
public void schedule() throws RemoteException {
//这里的mClient就是我们的应用进程ActivityThread
    mClient.scheduleTransaction(this);
}

//ActivityThread中 执行的是父类的
@Override
public void scheduleTransaction(ClientTransaction transaction) throws RemoteException {
    ActivityThread.this.scheduleTransaction(transaction);
}

//执行ClientTransactionHandler的scheduleTransaction
void scheduleTransaction(ClientTransaction transaction) {
    //执行客户端事务之前需要处理的事务
    transaction.preExecute(this);
    //发送消息给mH执行EXECUTE_TRANSACTION
    sendMessage(ActivityThread.H.EXECUTE_TRANSACTION, transaction);
}

//发送消息给mH
private void sendMessage(int what, Object obj, int arg1, int arg2, boolean async) {
    Message msg = Message.obtain();
    msg.what = what;
    msg.obj = obj;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    if (async) {
        msg.setAsynchronous(true);
    }
    mH.sendMessage(msg);
}

看看怎么处理的这个消息.
case EXECUTE_TRANSACTION:
    //拿到ClientTransaction
    final ClientTransaction transaction = (ClientTransaction) msg.obj;
    //调用TransactionExecutor(远程事务的执行者)执行事务
    mTransactionExecutor.execute(transaction);
    if (isSystem()) {
        transaction.recycle();
    }
    break;
    
    //在这里执行事务
public void execute(ClientTransaction transaction) {
    //拿到binder
    final IBinder token = transaction.getActivityToken();
    //执行事务 LaunchActivityItem
    executeCallbacks(transaction);
    //执行状态
    executeLifecycleState(transaction);
    //清空actions
    mPendingActions.clear();
    if (DEBUG_RESOLVER) Slog.d(TAG, tId(transaction) + "End resolving transaction");
}

//执行事务 也就是LauncherActivityItem的执行 
public void executeCallbacks(ClientTransaction transaction) {
    final List<ClientTransactionItem> callbacks = transaction.getCallbacks();
    //没有callback 返回 我们是有callback的就是LauncherActivityItem
    if (callbacks == null || callbacks.isEmpty()) {
        // No callbacks to execute, return early.
        return;
    }

    final IBinder token = transaction.getActivityToken();
    //获取到ActivityClientRecord 获取到客户端的ActivityRecord
    ActivityClientRecord r = mTransactionHandler.getActivityClient(token);

    final int size = callbacks.size();
    for (int i = 0; i < size; ++i) {
        final ClientTransactionItem item = callbacks.get(i);
        //执行item的execute item就是LaunchActivityItem
        item.execute(mTransactionHandler, token, mPendingActions);
        //调用postExecute
        item.postExecute(mTransactionHandler, token, mPendingActions);
       
        }
    }
}

//execute 获取到ActivityClientRecord
public void execute(ClientTransactionHandler client, IBinder token,
        PendingTransactionActions pendingActions) {
    ActivityClientRecord r = new ActivityClientRecord(token, mIntent, mIdent, mInfo,
            mOverrideConfig, mCompatInfo, mReferrer, mVoiceInteractor, mState, mPersistentState,
            mPendingResults, mPendingNewIntents, mIsForward,
            mProfilerInfo, client, mAssistToken);
      //调用ActivityThread的handleLaunchActivity
    client.handleLaunchActivity(r, pendingActions, null /* customIntent */);
}

public Activity handleLaunchActivity(ActivityClientRecord r,
        PendingTransactionActions pendingActions, Intent customIntent) {
    mSomeActivitiesChanged = true;
    //初始化WMG 之后说WMS的时候在介绍 和window 页面相关的
    WindowManagerGlobal.initialize();
    
    //调用performLaunchActivity
    final Activity a = performLaunchActivity(r, customIntent);

    if (a != null) {
        r.createdConfig = new Configuration(mConfiguration);
        reportSizeConfigurations(r);
        if (!r.activity.mFinished && pendingActions != null) {
            pendingActions.setOldState(r.state);
            pendingActions.setRestoreInstanceState(true);
            pendingActions.setCallOnPostCreate(true);
        }
    } else {
    }

    return a;
}

//创建Activity
private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
    //拿到ActivityInfo
    ActivityInfo aInfo = r.activityInfo;
    //拿到component
    ComponentName component = r.intent.getComponent();
    if (component == null) {
        component = r.intent.resolveActivity(
            mInitialApplication.getPackageManager());
        r.intent.setComponent(component);
    }
    //根据ActivityClientRecord来创建activity context 
    ContextImpl appContext = createBaseContextForActivity(r);
    Activity activity = null;
    try {
        //获取到类加载器 也就是我们app的类加载器 PathCalssLoader
        java.lang.ClassLoader cl = appContext.getClassLoader();
        //通过mInstrumentation的newActivity  反射创建Activity 
        activity = mInstrumentation.newActivity(
                cl, component.getClassName(), r.intent);
        r.intent.setExtrasClassLoader(cl);
        r.intent.prepareToEnterProcess();
        if (r.state != null) {
            r.state.setClassLoader(cl);
        }
    } catch (Exception e) {
    }

    try {
        //获取app
        Application app = r.packageInfo.makeApplication(false, mInstrumentation);
        
        if (activity != null) {
            //创建Configuration 屏幕大小 屏幕方向 和一些配置的信息
            Configuration config = new Configuration(mCompatConfiguration);
            if (r.overrideConfig != null) {
                config.updateFrom(r.overrideConfig);
            }
            Window window = null;
            if (r.mPendingRemoveWindow != null && r.mPreserveWindow) {
                window = r.mPendingRemoveWindow;
                r.mPendingRemoveWindow = null;
                r.mPendingRemoveWindowManager = null;
            }
            //设置OuterContext
            appContext.setOuterContext(activity);
            //调用activity的attach window相关
            activity.attach(appContext, this, getInstrumentation(), r.token,
                    r.ident, app, r.intent, r.activityInfo, title, r.parent,
                    r.embeddedID, r.lastNonConfigurationInstances, config,
                    r.referrer, r.voiceInteractor, window, r.configCallback,
                    r.assistToken);

            if (customIntent != null) {
                activity.mIntent = customIntent;
            }
            r.lastNonConfigurationInstances = null;
            checkAndBlockForNetworkAccess();
            activity.mStartedActivity = false;
            int theme = r.activityInfo.getThemeResource();
            if (theme != 0) {
                activity.setTheme(theme);
            }
            activity.mCalled = false;
            //执行mInstrumentation的callActivityOncreate
            if (r.isPersistable()) {
                mInstrumentation.callActivityOnCreate(activity, r.state, r.persistentState);
            } else {
                mInstrumentation.callActivityOnCreate(activity, r.state);
        }
            r.activity = activity;
        }
        //设置状态为ON_CREATE
        r.setState(ON_CREATE);
        synchronized (mResourcesManager) {
            //存入map
            mActivities.put(r.token, r);
        }

    } catch (SuperNotCalledException e) {
    } catch (Exception e) {
    }
    return activity;
}
//Instrumentation的newActivity
public Activity newActivity(ClassLoader cl, String className,
        Intent intent)
        throws InstantiationException, IllegalAccessException,
        ClassNotFoundException {
    String pkg = intent != null && intent.getComponent() != null
            ? intent.getComponent().getPackageName() : null;
    return getFactory(pkg).instantiateActivity(cl, className, intent);
}
//反射创建Activity 返回
public @NonNull Activity instantiateActivity(@NonNull ClassLoader cl, @NonNull String className,
        @Nullable Intent intent)
        throws InstantiationException, IllegalAccessException, ClassNotFoundException {
    return (Activity) cl.loadClass(className).newInstance();
}

//调用Activity的attach 为Activity关联上下文环境
final void attach(Context context, ActivityThread aThread,
        Instrumentation instr, IBinder token, int ident,
        Application application, Intent intent, ActivityInfo info,
        CharSequence title, Activity parent, String id,
        NonConfigurationInstances lastNonConfigurationInstances,
        Configuration config, String referrer, IVoiceInteractor voiceInteractor,
        Window window, ActivityConfigCallback activityConfigCallback, IBinder assistToken) {
        //调用attachBaseContext
    attachBaseContext(context);
    //执行fragments的attach
    mFragments.attachHost(null /*parent*/);
    //创建PhoneWindow
    mWindow = new PhoneWindow(this, window, activityConfigCallback);
    mWindow.setWindowControllerCallback(this);
    mWindow.setCallback(this);
    mWindow.setOnWindowDismissedCallback(this);
    mWindow.getLayoutInflater().setPrivateFactory(this);
    if (info.softInputMode != WindowManager.LayoutParams.SOFT_INPUT_STATE_UNSPECIFIED) {
        mWindow.setSoftInputMode(info.softInputMode);
    }
    if (info.uiOptions != 0) {
        mWindow.setUiOptions(info.uiOptions);
    }
   //设置ui线程
    mUiThread = Thread.currentThread();
    //设置mainThread
    mMainThread = aThread;
   //设置Instrumentation
    mInstrumentation = instr;
    mToken = token;
    mAssistToken = assistToken;
    mIdent = ident;
    mApplication = application;
    mIntent = intent;
    mReferrer = referrer;
    mComponent = intent.getComponent();
    mActivityInfo = info;
    mTitle = title;
    mParent = parent;
    mEmbeddedID = id;
    mLastNonConfigurationInstances = lastNonConfigurationInstances;
    if (voiceInteractor != null) {
        if (lastNonConfigurationInstances != null) {
            mVoiceInteractor = lastNonConfigurationInstances.voiceInteractor;
        } else {
            mVoiceInteractor = new VoiceInteractor(voiceInteractor, this, this,
                    Looper.myLooper());
        }
    }
    mWindow.setWindowManager(
            (WindowManager)context.getSystemService(Context.WINDOW_SERVICE),
            mToken, mComponent.flattenToString(),
            (info.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED) != 0);
    if (mParent != null) {
        mWindow.setContainer(mParent.getWindow());
    }
    mWindowManager = mWindow.getWindowManager();
    mCurrentConfig = config;

    mWindow.setColorMode(info.colorMode);

    setAutofillOptions(application.getAutofillOptions());
    setContentCaptureOptions(application.getContentCaptureOptions());
}

//执行activity的OnCreate
public void callActivityOnCreate(Activity activity, Bundle icicle) {
//从waitActivities中移除
    prePerformCreate(activity);
    //执行Activity的生命周期OnCreate
    activity.performCreate(icicle);
    //监控ActivityMonitor
    postPerformCreate(activity);
}

final void performCreate(Bundle icicle) {
    performCreate(icicle, null);
}

final void performCreate(Bundle icicle, PersistableBundle persistentState) {
//回调Activity的监听onActivityPreCreated 例如在Application中注册了
    dispatchActivityPreCreated(icicle);
    mCanEnterPictureInPicture = true;
    restoreHasCurrentPermissionRequest(icicle);
    //调用onCreate
    if (persistentState != null) {
        onCreate(icicle, persistentState);
    } else {
        onCreate(icicle);
    }
  
    mActivityTransitionState.readState(icicle);
    a
    mVisibleFromClient = !mWindow.getWindowStyle().getBoolean(
            com.android.internal.R.styleable.Window_windowNoDisplay, false);
    //Fragments的分发
    mFragments.dispatchActivityCreated();
    mActivityTransitionState.setEnterActivityOptions(this, getActivityOptions());
    //回调Activity的监听post
    dispatchActivityPostCreated(icicle);
}


//就是我们复写的onCreate
protected void onCreate(@Nullable Bundle savedInstanceState) {
    if (DEBUG_LIFECYCLE) Slog.v(TAG, "onCreate " + this + ": " + savedInstanceState);

    if (mLastNonConfigurationInstances != null) {
        mFragments.restoreLoaderNonConfig(mLastNonConfigurationInstances.loaders);
    }
    if (mActivityInfo.parentActivityName != null) {
        if (mActionBar == null) {
            mEnableDefaultActionBarUp = true;
        } else {
            mActionBar.setDefaultDisplayHomeAsUpEnabled(true);
        }
    }
    if (savedInstanceState != null) {
        mAutoFillResetNeeded = savedInstanceState.getBoolean(AUTOFILL_RESET_NEEDED, false);
        mLastAutofillId = savedInstanceState.getInt(LAST_AUTOFILL_ID,
                View.LAST_APP_AUTOFILL_ID);

        if (mAutoFillResetNeeded) {
            getAutofillManager().onCreate(savedInstanceState);
        }

        Parcelable p = savedInstanceState.getParcelable(FRAGMENTS_TAG);
        mFragments.restoreAllState(p, mLastNonConfigurationInstances != null
                ? mLastNonConfigurationInstances.fragments : null);
    }
    mFragments.dispatchCreate();
    dispatchActivityCreated(savedInstanceState);
    if (mVoiceInteractor != null) {
        mVoiceInteractor.attachActivity(this);
    }
    mRestoredFromBundle = savedInstanceState != null;
    mCalled = true;
}
```

回到`ActivityThread`的`scheduleTransaction`来执行事务，给`mH`发送`EXECUTE_TRANSACTION`执行，调用`TransactionExecutor`来执行，主要是两个函数`executeCallbacks`来执行`LauncherActivityItem`，它会调用`Instrumentation`的`newActivity`来`反射创建Activity` 以及`ActivityContext`以及设置`window`相关信息，调用`Instrumentation`的`callActivityOnCreate`执行`Applicaiton`监听的`Activity`创建的回调，以及`Activity`的`OnCreate`函数。此时已经将状态设置为`ON_CREATE`。接着会调用`executeLifecycleState`,我们看看他做了什么?

```
private void executeLifecycleState(ClientTransaction transaction) {
//我们之前在这里设置了ResumeActivityItem
    final ActivityLifecycleItem lifecycleItem = transaction.getLifecycleStateRequest();
    if (lifecycleItem == null) {
        return;
    }
    final IBinder token = transaction.getActivityToken();
    //获取到ActivityClientRecord
    final ActivityClientRecord r = mTransactionHandler.getActivityClient(token);
     //lifecycleItem是ResumeActivityItem 它的targetState是ON_RESUME
    cycleToPath(r, lifecycleItem.getTargetState(), true /* excludeLastState */, transaction);
    lifecycleItem.execute(mTransactionHandler, token, mPendingActions);
    lifecycleItem.postExecute(mTransactionHandler, token, mPendingActions);
}



private void cycleToPath(ActivityClientRecord r, int finish, boolean excludeLastState,
        ClientTransaction transaction) {
        //获取到当前状态是ON_CREATE
    final int start = r.getLifecycleState();
    //数组里面存储的是2
    final IntArray path = mHelper.getLifecyclePath(start, finish, excludeLastState);
    //执行lifecyleSequence
    performLifecycleSequence(r, path, transaction);
}


public IntArray getLifecyclePath(int start, int finish, boolean excludeLastState) {
    mLifecycleSequence.clear();
    if (finish >= start) {//当前start是1 finish是3 
        for (int i = start + 1; i <= finish; i++) {
            //添加2和3
            mLifecycleSequence.add(i);
        }
    } else { // finish < start, can't just cycle down
        if (start == ON_PAUSE && finish == ON_RESUME) {
            // Special case when we can just directly go to resumed state.
            mLifecycleSequence.add(ON_RESUME);
        } else if (start <= ON_STOP && finish >= ON_START) {
            // Restart and go to required state.

            // Go to stopped state first.
            for (int i = start + 1; i <= ON_STOP; i++) {
                mLifecycleSequence.add(i);
            }
            // Restart
            mLifecycleSequence.add(ON_RESTART);
            // Go to required state
            for (int i = ON_START; i <= finish; i++) {
                mLifecycleSequence.add(i);
            }
        } else {
            // Relaunch and go to required state

            // Go to destroyed state first.
            for (int i = start + 1; i <= ON_DESTROY; i++) {
                mLifecycleSequence.add(i);
            }
            // Go to required state
            for (int i = ON_CREATE; i <= finish; i++) {
                mLifecycleSequence.add(i);
            }
        }
    }
    //删除最后一个3
    if (excludeLastState && mLifecycleSequence.size() != 0) {
        mLifecycleSequence.remove(mLifecycleSequence.size() - 1);
    }

    return mLifecycleSequence;
}

private void performLifecycleSequence(ActivityClientRecord r, IntArray path,
        ClientTransaction transaction) {
    final int size = path.size();//size = 2
    for (int i = 0, state; i < size; i++) {
        state = path.get(i);
        switch (state) {
            case ON_START://2
                mTransactionHandler.handleStartActivity(r, mPendingActions);
                break;
            case ON_RESUME://3
                mTransactionHandler.handleResumeActivity(r.token, false /* finalStateRequest */,
                        r.isForward, "LIFECYCLER_RESUME_ACTIVITY");
                break;
        }
    }
}

//处理startActivity
public void handleStartActivity(ActivityClientRecord r,
        PendingTransactionActions pendingActions) {
    final Activity activity = r.activity;
    //执行onStart 分发fragments的onStart 以及监听函数的执行
    activity.performStart("handleStartActivity");
    //设置状态为ON_START
    r.setState(ON_START);
//执行onRestoreInstanceState
    if (pendingActions.shouldRestoreInstanceState()) {
        if (r.isPersistable()) {
            if (r.state != null || r.persistentState != null) {
                mInstrumentation.callActivityOnRestoreInstanceState(activity, r.state,
                        r.persistentState);
            }
        } else if (r.state != null) {
            mInstrumentation.callActivityOnRestoreInstanceState(activity, r.state);
        }
    }

//执行onPostCreate
    if (pendingActions.shouldCallOnPostCreate()) {
        activity.mCalled = false;
        if (r.isPersistable()) {
            mInstrumentation.callActivityOnPostCreate(activity, r.state,
                    r.persistentState);
        } else {
            mInstrumentation.callActivityOnPostCreate(activity, r.state);
        }
    }
}


final void performStart(String reason) {
    //分发pre start
    dispatchActivityPreStarted();
    mActivityTransitionState.setEnterActivityOptions(this, getActivityOptions());
    mFragments.noteStateNotSaved();
    mCalled = false;
    mFragments.execPendingActions();
    //调用Activity的onStart
    mInstrumentation.callActivityOnStart(this);
    //fragments分发start
    mFragments.dispatchStart();
    mFragments.reportLoaderStart();
    
    boolean isAppDebuggable =
            (mApplication.getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;

    boolean isDlwarningEnabled = SystemProperties.getInt("ro.bionic.ld.warning", 0) == 1;

    if (isAppDebuggable || isDlwarningEnabled) {
        String dlwarning = getDlWarning();
        if (dlwarning != null) {
            String appName = getApplicationInfo().loadLabel(getPackageManager())
                    .toString();
            String warning = "Detected problems with app native libraries\n" +
                             "(please consult log for detail):\n" + dlwarning;
            if (isAppDebuggable) {//是否debug
                  new AlertDialog.Builder(this).
                      setTitle(appName).
                      setMessage(warning).
                      setPositiveButton(android.R.string.ok, null).
                      setCancelable(false).
                      show();
            } else {
            }
        }
    }
    GraphicsEnvironment.getInstance().showAngleInUseDialogBox(this);
    mActivityTransitionState.enterReady(this);
    //执行post回调
    dispatchActivityPostStarted();
}

//执行activity的onStart
public void callActivityOnStart(Activity activity) {
    activity.onStart();
}

//Activity的onStart
protected void onStart() {
    mCalled = true;
       //分发Fragments
    mFragments.doLoaderStart();
    //分发监听
    dispatchActivityStarted();

    if (mAutoFillResetNeeded) {
        getAutofillManager().onVisibleForAutofill();
    }
}


//执行activity的onPostCreate
public void callActivityOnPostCreate(@NonNull Activity activity,
        @Nullable Bundle savedInstanceState,
        @Nullable PersistableBundle persistentState) {
    activity.onPostCreate(savedInstanceState, persistentState);
}


protected void onPostCreate(@Nullable Bundle savedInstanceState) {
    if (!isChild()) {
        mTitleReady = true;
        //更新title
        onTitleChanged(getTitle(), getTitleColor());
    }
    mCalled = true;
    notifyContentCaptureManagerIfNeeded(CONTENT_CAPTURE_START);
}

```

执行`executeCallbacks`之后设置状态为`ON_START`,在`executeLifecycleState`中获取到`start(ON_CREATE)`、`finsih(ON_RESUME)`,所以`path`的值为`2`，执行`performLifecycleSequence`会调用`mTransactionHandler.handleStartActivity`。接着回调用`item.execute`就是`ResumeActivityItem`的`execute`。

```
//调用到ActivityThread的handleResumeActivity
public void execute(ClientTransactionHandler client, IBinder token,
        PendingTransactionActions pendingActions) {
    Trace.traceBegin(TRACE_TAG_ACTIVITY_MANAGER, "activityResume");
    client.handleResumeActivity(token, true /* finalStateRequest */, mIsForward,
            "RESUME_ACTIVITY");
    Trace.traceEnd(TRACE_TAG_ACTIVITY_MANAGER);
}

public void handleResumeActivity(IBinder token, boolean finalStateRequest, boolean isForward,
        String reason) {
        //调用performResumeActivity
    final ActivityClientRecord r = performResumeActivity(token, finalStateRequest, reason);
    final Activity a = r.activity;
    final int forwardBit = isForward
            ? WindowManager.LayoutParams.SOFT_INPUT_IS_FORWARD_NAVIGATION : 0;
    boolean willBeVisible = !a.mStartedActivity;
      //第一次来是null所以会创建window
    if (r.window == null && !a.mFinished && willBeVisible) {
        r.window = r.activity.getWindow();
        View decor = r.window.getDecorView();
        decor.setVisibility(View.INVISIBLE);
        ViewManager wm = a.getWindowManager();
        WindowManager.LayoutParams l = r.window.getAttributes();
        a.mDecor = decor;
        l.type = WindowManager.LayoutParams.TYPE_BASE_APPLICATION;
        l.softInputMode |= forwardBit;
        if (r.mPreserveWindow) {
            a.mWindowAdded = true;
            r.mPreserveWindow = false;
            ViewRootImpl impl = decor.getViewRootImpl();
            if (impl != null) {
                impl.notifyChildRebuilt();
            }
        }
        if (a.mVisibleFromClient) {
            if (!a.mWindowAdded) {
                a.mWindowAdded = true;
                wm.addView(decor, l);
            } else {
                a.onWindowAttributesChanged(l);
            }
        }
    } else if (!willBeVisible) {
        r.hideForNow = true;
    }
    cleanUpPendingRemoveWindows(r, false /* force */);
    if (!r.activity.mFinished && willBeVisible && r.activity.mDecor != null && !r.hideForNow) {
        if (r.newConfig != null) {
            performConfigurationChangedForActivity(r, r.newConfig);
            }
            r.newConfig = null;
        }
        WindowManager.LayoutParams l = r.window.getAttributes();
        if ((l.softInputMode
                & WindowManager.LayoutParams.SOFT_INPUT_IS_FORWARD_NAVIGATION)
                != forwardBit) {
            l.softInputMode = (l.softInputMode
                    & (~WindowManager.LayoutParams.SOFT_INPUT_IS_FORWARD_NAVIGATION))
                    | forwardBit;
            if (r.activity.mVisibleFromClient) {
                ViewManager wm = a.getWindowManager();
                View decor = r.window.getDecorView();
                wm.updateViewLayout(decor, l);
            }
        }
        r.activity.mVisibleFromServer = true;
        mNumVisibleActivities++;
        if (r.activity.mVisibleFromClient) {
        //设置可见 获取WindowManager 添加view 和WMS通信 
            r.activity.makeVisible();
        }
    }
    r.nextIdle = mNewActivities;
    mNewActivities = r;
    //和AMS通信 感兴趣的可以自己跟一下
    Looper.myQueue().addIdleHandler(new Idler());
}


public ActivityClientRecord performResumeActivity(IBinder token, boolean finalStateRequest,
        String reason) {
        //获取到ActivityClientRecord
    final ActivityClientRecord r = mActivities.get(token);
    
    if (r == null || r.activity.mFinished) {
        return null;
    }
    if (finalStateRequest) {
        r.hideForNow = false;
        r.activity.mStartedActivity = false;
    }
    try {
        r.activity.onStateNotSaved();
        r.activity.mFragments.noteStateNotSaved();
        checkAndBlockForNetworkAccess();
        if (r.pendingIntents != null) {
            deliverNewIntents(r, r.pendingIntents);
            r.pendingIntents = null;
        }
        if (r.pendingResults != null) {
            deliverResults(r, r.pendingResults, reason);
            r.pendingResults = null;
        }
        //看是否执行OnRestart 以及执行OnResume
        r.activity.performResume(r.startsNotResumed, reason);

        r.state = null;
        r.persistentState = null;
        设置状态为ON_RESUME
        r.setState(ON_RESUME);
        reportTopResumedActivityChanged(r, r.isTopResumedActivity, "topWhenResuming");
    } catch (Exception e) {
    }
    return r;
}

//调用Activity的performResume
final void performResume(boolean followedByPause, String reason) {
//分发resume
    dispatchActivityPreResumed();
    //如果stop了需要执行onRestart
    performRestart(true /* start */, reason);

    mFragments.execPendingActions();

    mLastNonConfigurationInstances = null;

    if (mAutoFillResetNeeded) {
        mAutoFillIgnoreFirstResumePause = followedByPause;
        if (mAutoFillIgnoreFirstResumePause && DEBUG_LIFECYCLE) {
        }
    }

    mCalled = false;
    //调用activity的onResume
    mInstrumentation.callActivityOnResume(this);
    mCalled = false;
    //给Fragments分发Resume
    mFragments.dispatchResume();
    mFragments.execPendingActions();
    //执行onPostResume
    onPostResume();
    //分发postresume监听
    dispatchActivityPostResumed();
}

//Instrumentation调用resume
public void callActivityOnResume(Activity activity) {
    activity.mResumed = true;
    activity.onResume();
    
    if (mActivityMonitors != null) {
        synchronized (mSync) {
            final int N = mActivityMonitors.size();
            for (int i=0; i<N; i++) {
                final ActivityMonitor am = mActivityMonitors.get(i);
                am.match(activity, activity, activity.getIntent());
            }
        }
    }
}

//Activity的resume
protected void onResume() {
    //分发Resume的监听
    dispatchActivityResumed();
    mActivityTransitionState.onResume(this);
    enableAutofillCompatibilityIfNeeded();
    if (mAutoFillResetNeeded) {
        if (!mAutoFillIgnoreFirstResumePause) {
            View focus = getCurrentFocus();
            if (focus != null && focus.canNotifyAutofillEnterExitEvent()) {
                getAutofillManager().notifyViewEntered(focus);
            }
        }
    }
    notifyContentCaptureManagerIfNeeded(CONTENT_CAPTURE_RESUME);
    mCalled = true;
}

```

在`ResumeActivityItem`的`execute`执行到`ActivityThread`的`handleResumeActivity`,判断是否是`stoped`来执行`onRestart` 接着执行`OnResume`,并且设置状态为`ON_RESUME`,调用`Activity`的`makeVisible`就和`WMS`通信了，这个我们后边再讲。在客户端处理完成之后通过IDLEHandler和`AMS`通信，`AMS`再更新`Activity`相关的信息，感兴趣的可以自己去看看了。

## 总结

至此，整个Activity的创建流程就结束了，从`Launcher`的点击一直到`Activity`的`OnResume`的执行。我们来简单的文字总结下流程： 1.在`Launcher`的`ItemClickHandler`,最终会调用到`Activity`的`startActivity`函数,它会调用到`Instrumentation`的`execStartActivity`函数。

2.通过`Instrumentation`的`execStartActivity`进行了两次`IPC`通信,获取到`ATMS`调用`startActivity`，它会根据intent来查找activity信息，并且暂停当前Activity，调用到`ActivityStackSupervisor`的`startSpecificActivityLocked`判断进程是否存在(wpc.thread)，如果进程存在调用`realStartActivityLocked`来开启`Activity`,否则调用`startProcess`开启进程。

3.我们第一次进来进程是不存在的，所以我们会进入开启进程的流程，调用 `ProcessList`的`startProcessLocked`设置`entryPoint`为`ActivityThread`，通过`ZygoteProcess`来设置参数，`ZygoteConnect`发送数据给`Zygote`,当`ZygoteServer`接收到数据之后开启`ActivityThread`的`main`会创建`ActivityThread` 调用`attach`(注意此时我们已经在子进程了)，然后`IPC`告诉`AMS` 开始`attachApplication`服务端会把进程存入`mPidsSelfLocked`进行管理，然后通过`thread.bindApplication(IPC)`告诉应用端，应用进程就可以创建`Application loadedApk Context`调用`Application`的`OnCreate`。 创建完进程就4次`IPC`通信了(start的时候两次，创建进程之后告诉AMS 继续AMS的流程 一次，thread.bindApplication 一次)。

4.客户端创建完`Application`之后在`ATMS`中会调用`attachApplication` 接着会调用`realStartActivityLocked`创建`ClientTransaction`，设置`callback`为`LaunchActivityItem` 添加`stateRequest`为`ResumeActivityItem`，调用`scheduleTransaction(IPC)`调用到`ActivityThread`的`scheduleTransaction`函数，调用`executeCallBack` 也就是执行`LaunchActivityItem`的execute 它会调用到ActivityThread的`handleLaunchActivity`,会创建ActivityContext ，通过Instrumentation `反射创建Activity` 调用activity的attach 绑定window 再调用callActivityOnCreate 执行`Activity`的`OnCreate`。在`Activity`的`OnCreate`中分发监听给`ActivityLifecycleCallbacks`。最后设置当前状态为`ON_CREATE`。

5.OnCreate之后就会执行`executeLifecycleState`，之前传递的是`ResumeActivityItem`,接着调用`cycleToPath`，之前设置了是`ON_CREATE`，所以现在里面会存储2 也就是需要执行OnStart，调用`performLifecycleSequence` 调用`ActivityThread.handleStartActivity` 分发`ActivityLifecycleCallbacks`,分发`Fragments` 调用`Instrumentation`的`callActivityOnStart` 设置state为`ON_START`

6.调用`ResumeActivityItem`的`execute`,调用到`ActivityThread.handleResumeActivity`，调用`performResume` 分发resume事件给`ActivityLifecycleCallbacks`,分发`Fragments`，执行`onPostResume` 分发`onPostResume`监听 调用`Instrumentation`的`callActivityOnresume` 会调用到`Activity`的`onResume`。 最后会再调用`r.activity.makeVisible`通过`WindowManager` 添加当前`view`和`WMS(IPC)`通信，再给Looper发送一个`Idler`和`AMS(IPC)`通信，让AMS设置Activity的状态信息。

![activity流程.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/2b84d162369c4771bb5410ef22040416~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 面试

Q1:介绍一下Activity的启动流程。

A1:分两种情况，进程未存在，进程已存在。 没存在的话会先跟Zygote建立通信，由`Zygote` Fork出来子进程 并进入`ActivityThread`，然后告诉`AMS``attachApplication`，`AMS`再告诉客户端 创建`Application`、`loadedApk`、`Instrumentation` 通过`Instrumentation`的`callApplicationOncreate`执行`Application`的`Oncreate` 生命周期。 接着`AMS`会调用`realStartActivity` 创建`ClientTransaction`，设置`callback`为`LaunchActivityItem` 添加`stateRequest`为`ResumeActivityItem`，分别执行OnCreate,onStart,OnResume，在OnResume中会调用WM和WMS通信绘制UI，再给Looper发送一个`Idler`和`AMS(IPC)`通信，让AMS设置Activity的状态信息。

Q2:ActivityManagerService在创建应用的时候为什么使用socket 而不是用Binder呢？

A2:这个交给大家自己去想哈~

好了，关于`AMS`是如何开启应用，如何开启`Activity`的流程我们都讲完了，Activity是最复杂的一个组件了，很多逻辑，一次彻底明白是不可能的，当我们需要分析的时候再回来看吧。下一次我们讲另外一个组件`Service`。大家 下次再见吧。

在线视频: [www.bilibili.com/video/BV17m…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV17m4y1m7wY%2F%3Fspm_id_from%3D333.999.0.0%26vd_source%3D689a2ec078877b4a664365bdb60362d3 "https://www.bilibili.com/video/BV17m4y1m7wY/?spm_id_from=333.999.0.0&vd_source=689a2ec078877b4a664365bdb60362d3")