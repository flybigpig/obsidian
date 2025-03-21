> 前面我们分析了init进程，zygote进程，SystemServer进程，本篇的Launcher是系统启动流程的最后一个进程。

##### 1 Launcher概述

Launcher进程是一个系统的应用程序，位于**packages/apps/Launcher3**中，它用于显示已经安装的应用程序，它通过访问PackageManagerService获取安装的应用程序，然后将他们封装成一个个的快捷图标显示到屏幕上，每一个图标包含了被启动应用程序的Intent信息，点击之后就可以启动对应应用程序。

##### 2 Launcher进程解析

```
private void startOtherServices() {
mActivityManagerService.systemReady(new Runnable() {
@Override
            public void run() {
...
mSystemServiceManager.startBootPhase(
                        SystemService.PHASE_ACTIVITY_MANAGER_READY);
}
}
}
```

Launcher的入口函数在System.startOtherServices方法中，我们进入systemReady方法中：

**frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java**

```
    public void systemReady(final Runnable goingCallback) {
    ...
mStackSupervisor.resumeFocusedStackTopActivityLocked();
...
}
```

进入到了ActivityStackSupervisor.resumeFocusedStackTopActivityLocked中

**frameworks/base/services/core/java/com/android/server/am/ActivityStackSupervisor.java**

```

boolean resumeFocusedStackTopActivityLocked() {
        return resumeFocusedStackTopActivityLocked(null, null, null);
    }

    boolean resumeFocusedStackTopActivityLocked(
            ActivityStack targetStack, ActivityRecord target, ActivityOptions targetOptions) {
        if (targetStack != null && isFocusedStack(targetStack)) {
            return targetStack.resumeTopActivityUncheckedLocked(target, targetOptions);
        }
        final ActivityRecord r = mFocusedStack.topRunningActivityLocked();
        if (r == null || r.state != RESUMED) {
            mFocusedStack.resumeTopActivityUncheckedLocked(null, null);
        }
        return false;
    }
```

在上面最后调用到了`mFocusedStack.resumeTopActivityUncheckedLocked`，mFocusedStack是ActivityStack类型的，我们进入查看源码：

**frameworks/base/services/core/java/com/android/server/am/ActivityStack.java**

```
boolean resumeTopActivityUncheckedLocked(ActivityRecord prev, ActivityOptions options) {
        if (mStackSupervisor.inResumeTopActivity) {
            // Don't even start recursing.
            return false;
        }

        boolean result = false;
        try {
            // Protect against recursion.
            mStackSupervisor.inResumeTopActivity = true;
            if (mService.mLockScreenShown == ActivityManagerService.LOCK_SCREEN_LEAVING) {
                mService.mLockScreenShown = ActivityManagerService.LOCK_SCREEN_HIDDEN;
                mService.updateSleepIfNeededLocked();
            }
            result = resumeTopActivityInnerLocked(prev, options);
        } finally {
            mStackSupervisor.inResumeTopActivity = false;
        }
        return result;
    }

private boolean resumeTopActivityInnerLocked(ActivityRecord prev, ActivityOptions options) {
        ...
        
        final TaskRecord prevTask = prev != null ? prev.task : null;
        if (next == null) {
            /***
             * 这里启动了Launcher
             */
            return isOnHomeDisplay() &&
                    mStackSupervisor.resumeHomeStackTask(returnTaskType, prev, reason);
        }
    }
```

上面从resumeTopActivityUncheckedLocked调用到了resumeTopActivityInnerLocked中，最后调用到了ActivityStackSupervisor.resumeHomeStackTask函数。

**frameworks/base/services/core/java/com/android/server/am/ActivityStackSupervisor.java**

```
boolean resumeHomeStackTask(int homeStackTaskType, ActivityRecord prev, String reason) {
        ...
        
        // Only resume home activity if isn't finishing.
        if (r != null && !r.finishing) {
            mService.setFocusedActivityLocked(r, myReason);
            return resumeFocusedStackTopActivityLocked(mHomeStack, prev, null);
        }
        return mService.startHomeActivityLocked(mCurrentUser, myReason);
    }
```

接着进入到了ActivityManagerService.startHomeActivityLocked

**frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java**

```
boolean startHomeActivityLocked(int userId, String reason) {
        if (mFactoryTest == FactoryTest.FACTORY_TEST_LOW_LEVEL
                && mTopAction == null) {
            // We are running in factory test mode, but unable to find
            // the factory test app, so just sit around displaying the
            // error message and don't try to start anything.
            return false;
        }
        // 1 
        Intent intent = getHomeIntent();
        ActivityInfo aInfo = resolveActivityInfo(intent, STOCK_PM_FLAGS, userId);
        if (aInfo != null) {
            intent.setComponent(new ComponentName(aInfo.applicationInfo.packageName, aInfo.name));
            // Don't do this if the home app is currently being
            // instrumented.
            aInfo = new ActivityInfo(aInfo);
            aInfo.applicationInfo = getAppInfoForUser(aInfo.applicationInfo, userId);
            ProcessRecord app = getProcessRecordLocked(aInfo.processName,
                    aInfo.applicationInfo.uid, true);
            if (app == null || app.instrumentationClass == null) {
                intent.setFlags(intent.getFlags() | Intent.FLAG_ACTIVITY_NEW_TASK);
                mActivityStarter.startHomeActivityLocked(intent, aInfo, reason);
            }
        } else {
            Slog.wtf(TAG, "No home screen found for " + intent, new Throwable());
        }

        return true;
    }
```

在注释1处的getHomeIntent方法代码如下：

```
Intent getHomeIntent() {
        Intent intent = new Intent(mTopAction, mTopData != null ? Uri.parse(mTopData) : null);
        intent.setComponent(mTopComponent);
        intent.addFlags(Intent.FLAG_DEBUG_TRIAGED_MISSING);
        if (mFactoryTest != FactoryTest.FACTORY_TEST_LOW_LEVEL) {
            intent.addCategory(Intent.CATEGORY_HOME);//1 
        }
        return intent;
    }
```

在注释1处添加了一个`CATEGORY_HOME`，`CATEGORY_HOME = "android.intent.category.HOME"`，这里我们锁定就是启动了Launcher页面，因为我们查看系统应用Launcher的清单文件中匹配了它，Launcher的清单文件如下：

```
<manifest
    xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.android.launcher3">
    <uses-sdk android:targetSdkVersion="23" android:minSdkVersion="16"/>

    <uses-permission android:name="android.permission.CALL_PHONE" />
    ...
    
    <application
        android:allowBackup="@bool/enable_backup"
        android:backupAgent="com.android.launcher3.LauncherBackupAgentHelper"
        android:hardwareAccelerated="true"
        android:icon="@mipmap/ic_launcher_home"
        android:label="@string/app_name"
        android:largeHeap="@bool/config_largeHeap"
        android:restoreAnyVersion="true"
        android:supportsRtl="true" >

        <activity
            android:name="com.android.launcher3.Launcher"
            android:launchMode="singleTask"
            android:clearTaskOnLaunch="true"
            android:stateNotNeeded="true"
            android:theme="@style/Theme"
            android:windowSoftInputMode="adjustPan"
            android:screenOrientation="nosensor"
            android:configChanges="keyboard|keyboardHidden|navigation"
            android:resumeWhilePausing="true"
            android:taskAffinity=""
            android:enabled="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.HOME" />
                <category android:name="android.intent.category.DEFAULT" />
                <category android:name="android.intent.category.MONKEY"/>
            </intent-filter>
        </activity>      
    </application>
</manifest>
```

从上面可以看出Launcher的intent-filter匹配了上面的Intent，所以Launcher就被启动起来了。就会执行onCreate方法。

  

##### 3 Launcher进程图标显示过程

Launcher.onCreate函数如下：

**packages/apps/Launcher3/src/com/android/launcher3/Launcher.java**

```
@Override
    protected void onCreate(Bundle savedInstanceState) {
       
        super.onCreate(savedInstanceState);
//1
        LauncherAppState app = LauncherAppState.getInstance();
//2
        mModel = app.setLauncher(this);
       
        setContentView(R.layout.launcher);
        
        if (!mRestoring) {
           //3
                mModel.startLoader(PagedView.INVALID_RESTORE_PAGE);
            } else {
                // We only load the page synchronously if the user rotates (or triggers a
                // configuration change) while launcher is in the foreground
                mModel.startLoader(mWorkspace.getRestorePage());
            }
        }
    }
```

注释1处单例获取LauncherAppState的实例，在注释2处调用它的setLauncher函数并将Launcher对象传入，setLauncher函数如下所示：

**packages/apps/Launcher3/src/com/android/launcher3/LauncherAppState.java**

```
 LauncherModel setLauncher(Launcher launcher) {
        getLauncherProvider().setLauncherProviderChangeListener(launcher);
        mModel.initialize(launcher);
        mAccessibilityDelegate = ((launcher != null) && Utilities.ATLEAST_LOLLIPOP) ?
            new LauncherAccessibilityDelegate(launcher) : null;
        return mModel;
    }
```

接着调用到了LauncherModel的initialize函数：

**packages/apps/Launcher3/src/com/android/launcher3/LauncherModel.java**

```
/**
     * Set this as the current Launcher activity object for the loader.
     */
    public void initialize(Callbacks callbacks) {
        synchronized (mLock) {
            // Disconnect any of the callbacks and drawables associated with ItemInfos on the
            // workspace to prevent leaking Launcher activities on orientation change.
            unbindItemInfosAndClearQueuedBindRunnables();
            mCallbacks = new WeakReference<Callbacks>(callbacks);
        }
    }
```

我们看到上面其实就是设置了一个回调函数，回调函数就是Launcher自身，不过是以一个弱引用的方式，这个回调后面会用到。

接着分析onCreate中的注释3的部分，launcherModel.startLoader方法：

```
  @Thunk static final HandlerThread sWorkerThread = new HandlerThread("launcher-loader");
    static {
        sWorkerThread.start();
    }
    @Thunk static final Handler sWorker = new Handler(sWorkerThread.getLooper());

public void startLoader(int synchronousBindPage, int loadFlags) {
        // Enable queue before starting loader. It will get disabled in Launcher#finishBindingItems
        InstallShortcutReceiver.enableInstallQueue();
        synchronized (mLock) {
            // Clear any deferred bind-runnables from the synchronized load process
            // We must do this before any loading/binding is scheduled below.
            synchronized (mDeferredBindRunnables) {
                mDeferredBindRunnables.clear();
            }

            // Don't bother to start the thread if we know it's not going to do anything
            if (mCallbacks != null && mCallbacks.get() != null) {
                // If there is already one running, tell it to stop.
                stopLoaderLocked();
                mLoaderTask = new LoaderTask(mApp.getContext(), loadFlags);
                if (synchronousBindPage != PagedView.INVALID_RESTORE_PAGE
                        && mAllAppsLoaded && mWorkspaceLoaded && !mIsLoaderTaskRunning) {
                    mLoaderTask.runBindSynchronousPage(synchronousBindPage);
                } else {
                    sWorkerThread.setPriority(Thread.NORM_PRIORITY);
                    sWorker.post(mLoaderTask);
                }
            }
        }
    }
```

上面创建了一个HandlerThread的子线程的Handler，用来处理耗时任务，在上面的最后post了一个任务，这个任务的类是LoaderTask.java，实现了Runnable接口，run方法如下：

```
public void run() {
            synchronized (mLock) {
                if (mStopped) {
                    return;
                }
                mIsLoaderTaskRunning = true;
            }
            // Optimize for end-user experience: if the Launcher is up and // running with the
            // All Apps interface in the foreground, load All Apps first. Otherwise, load the
            // workspace first (default).
            keep_running: {
                if (DEBUG_LOADERS) Log.d(TAG, "step 1: loading workspace");
                //1 
                loadAndBindWorkspace();

                if (mStopped) {
                    break keep_running;
                }

                waitForIdle();

                // second step
                if (DEBUG_LOADERS) Log.d(TAG, "step 2: loading all apps");
                // 2
                loadAndBindAllApps();
            }

            // Clear out this reference, otherwise we end up holding it until all of the
            // callback runnables are done.
            mContext = null;

            synchronized (mLock) {
                // If we are still the last one to be scheduled, remove ourselves.
                if (mLoaderTask == this) {
                    mLoaderTask = null;
                }
                mIsLoaderTaskRunning = false;
                mHasLoaderCompletedOnce = true;
            }
        }
```

上面的两个注释写的很清楚，在注释1处loadAndBindWorkspace是加载工作空间，在注释2处loadAndBindAllApps用来加载所有的app。

loadAndBindAllApps源码如下：

```
 private void loadAndBindAllApps() {
            if (DEBUG_LOADERS) {
                Log.d(TAG, "loadAndBindAllApps mAllAppsLoaded=" + mAllAppsLoaded);
            }
            if (!mAllAppsLoaded) {
            // 1 
                loadAllApps();
                synchronized (LoaderTask.this) {
                    if (mStopped) {
                        return;
                    }
                }
                updateIconCache();
                synchronized (LoaderTask.this) {
                    if (mStopped) {
                        return;
                    }
                    mAllAppsLoaded = true;
                }
            } else {
                onlyBindAllApps();
            }
        }
```

注释1处调用了loadAllApps()，源码如下：

```
private void loadAllApps() {
...
        mHandler.post(new Runnable() {
            public void run() {
                final long bindTime = SystemClock.uptimeMillis();
                final Callbacks callbacks = tryGetCallbacks(oldCallbacks);
                if (callbacks != null) {
                    callbacks.bindAllApplications(added);//1
                    ...
                }
            }
        });
       ...
    }
```

这里的callbacks就是上面弱引用存储的Launcher，所以我们直接查看Launcher中的bindAllApplications，代码如下：

**packages/apps/Launcher3/src/com/android/launcher3/Launcher.java**

```
 public void bindAllApplications(final ArrayList<AppInfo> apps) {
        if (waitUntilResume(mBindAllApplicationsRunnable, true)) {
            mTmpAppsList = apps;
            return;
        }
        if (mAppsView != null) {
        //1 
            mAppsView.setApps(apps);
        }
        if (mLauncherCallbacks != null) {
            mLauncherCallbacks.bindAllApplications(apps);
        }
    }
```

在注释1处调用了 mAppsView的setApps，mAppsView是一个AllAppsContainerView类型的，我们查看它的setApps方法：

**packages/apps/Launcher3/src/com/android/launcher3/allapps/AllAppsContainerView.java**

```
private final AlphabeticalAppsList mApps;

public void setApps(List<AppInfo> apps) {
        mApps.setApps(apps);
    }
```

这里我们查看onFinishInflate函数

```
    
    private AllAppsRecyclerView mAppsRecyclerView;

@Override
    protected void onFinishInflate() {
        super.onFinishInflate();

//1 初始化控件
        mAppsRecyclerView = (AllAppsRecyclerView) findViewById(R.id.apps_list_view);
        //2 传递应用
        mAppsRecyclerView.setApps(mApps);
        mAppsRecyclerView.setLayoutManager(mLayoutManager);
        //3 设置adapter
        mAppsRecyclerView.setAdapter(mAdapter);
        mAppsRecyclerView.setHasFixedSize(true);
        mAppsRecyclerView.addOnScrollListener(mElevationController);
        mAppsRecyclerView.setElevationController(mElevationController);
    }
```

onFinishInflate在xml加载完成之后就会调用，这里launcher中app的显示逻辑和我们平时将数据显示到一个RecyclerView上基本一致，所以这里也比较好理解。

##### 4 Android系统启动流程

在此对整个Android系统启动流程做一个全面的总结：

**1 按Power键启动电源及系统启动**

当按下电源键，引导芯片代码开始从固化在ROM中预定义的地方开始执行，加载引导程序Bootloader到RAM，然后执行引导程序。

**2 引导程序Bootloader**

引导程序是Android操作系统被拉起来之前的一个程序，类似于window一样，它的作用就是把系统拉起运行起来。它是针对特定的主板与芯片的。设备制造商要么使用很受欢迎的引导程序比如redboot、uboot、qibootloader或者开发自己的引导程序，它不是Android操作系统的一部分。引导程序是OEM厂商或者运营商加锁和限制的地方。

**3 linux内核启动**

Android内核与桌面linux内核启动的方式差不多。内核启动时，设置缓存、被保护存储器、计划列表，加载驱动。当内核完成系统设置，它首先在系统文件中寻找”init”文件，然后启动root进程或者系统的第一个进程。

**4 init进程启动**  
创建和挂载启动所需要的文件目录，初始化和启动属性服务，解析init.rc配置并且启动Zygote进程。

**5 Zygote进程启动**

创建JavaVM并为JavaVM注册JNI，创建服务端Socket，启动SystemServer进程。

**6 SystemServer进程启动**

启动Binder线程池和SystemServiceManager，并且启动各种系统服务。

**7 Launcher启动**

被SystemServer进程启动的ActivityManagerService会启动Launcher，Launcher启动后会将已安装应用的快捷图标显示到界面上。

##### 5 面试题

1 Android系统启动流程? 答案如上所述

**2 什么是写时拷贝(copy- on-write)？**

写时拷贝copy\_on\_write技术: fork的实现是使用copy\_on\_write技术实现的，它是一种可以推迟甚至避免拷贝的技术，内核此时并不复制整个进程的地址空间，而是让父子进程共享同一个地址空间，只有在子进程真正需要写入数据的时候才会赋值地址空间，从而使得每个进程拥有自己的进程空间，也就是说资源的复制是在需要写入数据的时候才会进行，在此之前只会以只读的形式共享。

**3 孤儿进程和僵尸进程是什么？**

fork系统调用之后，父子进程将交替执行，执行顺序不定。如果父进程先退出，子进程还没退出那么子进程的父进程将变为init进程（托孤给了init进程）。（注：任何一个进程都必须有父进程）如果子进程先退出，父进程还没退出，那么子进程必须等到父进程捕获到了子进程的退出状态才真正结束，否则这个时候子进程就成为僵进程（僵尸进程：只保留一些退出信息供父进程查询).

**4 system\_server 为什么要在 Zygote 中启动，而不是由 init 直接启动呢？**

Zygote 作为一个孵化器，可以提前加载一些资源，这样 fork() 时基于 Copy-On-Write 机制创建的其他进程就能直接使用这些资源，而不用重新加载。比如 system\_server 就可以直接使用 Zygote 中的 JNI函数、共享库、常用的类、以及主题资源。

**5 为什么要专门使用 Zygote 进程去孵化应用进程，而不是让 system\_server 去孵化呢？**

system\_server 相比 Zygote 多运行了 AMS、WMS 等服务，这些对一个应用程序来说是不需要的。另外进程的 fork() 对多线程不友好，仅会将发起调用的线程拷贝到子进程，这可能会导致死锁，而system\_server 中肯定是有很多线程的。

**6 多线程的进程fork调用为什么会导致死锁？**

多线程的进程的fork调用：复制整个用户空间的数据（通常使用 copy-on-write 的策略，  
所以可以实现的速度很快）以及所有系统对象，然后仅复制当前线程到子进程。这里：所有父进程中别的线程，到了子进程中都是突然蒸发掉的假设这么一个环境，在 fork 之前，有一个子线程 lock 了某个锁，获得了对锁的所有权。fork 以后，在子进程中，所有的额外线程都人间蒸发了。而锁却被正常复制了，在子进程看来，这个锁没有主人，所以没有任何人可以对它解锁。当子进程想 lock 这个锁时，不再有任何手段可以解开了。程序发生死锁

**7 Zygote 为什么不采用 Binder 机制进行 IPC 通信？**

Binder 机制中存在 Binder 线程池，是多线程的，如果 Zygote 采用 Binder 的话就存在上面说的fork() 与 多线程的问题了。其实严格来说，Binder 机制不一定要多线程，所谓的 Binder 线程只不过是在循环读取 Binder 驱动的消息而已，只注册一个 Binder 线程也是可以工作的，比如 service manager就是这样的。实际上 Zygote 尽管没有采取 Binder 机制，它也不是单线程的，但它在 fork() 前主动停止了其他线程，fork() 后重新启动了。