## 前提

## 回顾

首先对之前的内容进行一个简单的回顾，在Android中，第一个启动的进程是`init`，它主要`解析init.rc`文件，启动对应的各种各样的`service`，例如`media，zygote，surfaceflinger`，以及开启了`属性服务`(类似Windows的注册表)等。接下来我们讲了`Zygote`，Zygote是由`init`启动起来的，它是一个孵化器，`孵化了system_server以及其他的应用程序`，`预加载了一些class类和drawable color等资源`。`system_server`是系统用来启动管理`service`的`入口`，比如我们常用的`AMS、WMS、PMS`等等都是它来创建的,system\_server加载了`framework-res.apk`，接着调用`startBootstrapServices`、`startCoreServices`、`startOtherServices`开启了非常多的服务，以及开启了`WatchDog`监控service。接下来讲了`ServiceManage`，它是一个服务的提供者，可以让应用获取到系统的各种服务，在这里我们讲了Android中很重要的`IPC`机制-->`Binder`机制，横跨了应用层，FrameWork层，驱动层，非常复杂，视频都讲了5个小时，也希望大家可以明白Binder是如何工作的。

具体的细节可以参考之前写的文章和视频：

[【Android FrameWork】第一个启动的程序--init](https://juejin.cn/post/7213733567606685753 "https://juejin.cn/post/7213733567606685753")

[【Android FrameWork】Zygote](https://juejin.cn/post/7214068367607119933 "https://juejin.cn/post/7214068367607119933")

[【Android FrameWork】SystemServer](https://juejin.cn/post/7214493929566945340 "https://juejin.cn/post/7214493929566945340")

[【Android FrameWork】ServiceManager(一)](https://juejin.cn/post/7216537771448598585#heading-4 "https://juejin.cn/post/7216537771448598585#heading-4")

[【Android FrameWork】ServiceManager(二)](https://juejin.cn/post/7216536069285675045#heading-9 "https://juejin.cn/post/7216536069285675045#heading-9")

## 介绍

Android系统启动的最后一步是启动一个`桌面应用`，这个应用用来显示我们已经安装的应用程序，它就是Launcher。这次我们就来认识认识这位新朋友。

## 正文

## 1.Launcher的启动

Launcher是由`system_server`启动的。直接上代码,在`startOtherServices`中

```
    private void startOtherServices() {
          //……………………
        mActivityManagerService.systemReady(() -> {/*……………………*/}, BOOT_TIMINGS_TRACE_LOG);
    }
```

在这里调用了`mActivityManagerService.systemReady`，看看ready中是如何开启`Launcher`的

```
public void systemReady(final Runnable goingCallback, TimingsTraceLog traceLog) {
       //……………………
        //开启Launcher
        mAtmInternal.startHomeOnAllDisplays(currentUserId, "systemReady");
    //……………………
    }
}
```

这个`mAtmInternal`是谁呢？他是`ActivityTaskManager`的一个内部类`ActivityTaskManagerInternal`，`ActivityTaskManager`主要负责Activity的管理和调度。而`ActivityTaskManagerInternal`是我们Activity管理器的一个本地服务接口,他的具体实现在`ActivityTaskManager`。 文件目录:`/frameworks/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java`

```
final class LocalService extends ActivityTaskManagerInternal {
//………………

//开启Launcher
@Override
public boolean startHomeOnAllDisplays(int userId, String reason) {
    synchronized (mGlobalLock) {
        return mRootActivityContainer.startHomeOnAllDisplays(userId, reason);
    }
}
}
```

`mRootActivityContainer`是`root Activity`的容器类，调用`startHomeOnAllDisplays` 文件目录:`/frameworks/base/services/core/java/com/android/server/wm/RootActivityContainer.java`

```
boolean startHomeOnAllDisplays(int userId, String reason) {
    boolean homeStarted = false;
    for (int i = mActivityDisplays.size() - 1; i >= 0; i--) {
        final int displayId = mActivityDisplays.get(i).mDisplayId;
        //调用了startHomeOnDisplay 3个参数的
        homeStarted |= startHomeOnDisplay(userId, reason, displayId);
    }
    return homeStarted;
}

boolean startHomeOnDisplay(int userId, String reason, int displayId) {
//接着调用了5个参数的方法
    return startHomeOnDisplay(userId, reason, displayId, false /* allowInstrumenting */,
            false /* fromHomeKey */);
}

boolean startHomeOnDisplay(int userId, String reason, int displayId, boolean allowInstrumenting,
        boolean fromHomeKey) {
    // Fallback to top focused display if the displayId is invalid.
    if (displayId == INVALID_DISPLAY) {
        displayId = getTopDisplayFocusedStack().mDisplayId;
    }

    Intent homeIntent = null;
    ActivityInfo aInfo = null;
    if (displayId == DEFAULT_DISPLAY) {
        //拿到需要启动的Launcher的intent，intent action是Intent.ACTION_MAIN   category是Intent.CATEGORY_HOME
        homeIntent = mService.getHomeIntent();
        //拿到需要启动的activityInfo
        aInfo = resolveHomeActivity(userId, homeIntent);
    } else if (shouldPlaceSecondaryHomeOnDisplay(displayId)) {
        Pair<ActivityInfo, Intent> info = resolveSecondaryHomeActivity(userId, displayId);
        aInfo = info.first;
        homeIntent = info.second;
    }
    if (aInfo == null || homeIntent == null) {
        return false;
    }

    if (!canStartHomeOnDisplay(aInfo, displayId, allowInstrumenting)) {
        return false;
    }

    //设置参数Component 就是查到的packageName
    homeIntent.setComponent(new ComponentName(aInfo.applicationInfo.packageName, aInfo.name));
     //设置flag为new_task
    homeIntent.setFlags(homeIntent.getFlags() | FLAG_ACTIVITY_NEW_TASK);
    // Updates the extra information of the intent.
    if (fromHomeKey) {
        homeIntent.putExtra(WindowManagerPolicy.EXTRA_FROM_HOME_KEY, true);
    }
    final String myReason = reason + ":" + userId + ":" + UserHandle.getUserId(
            aInfo.applicationInfo.uid) + ":" + displayId;
            //调用ActivityStartController的startHomeActivity方法开启Launcher
    mService.getActivityStartController().startHomeActivity(homeIntent, aInfo, myReason,
            displayId);
    return true;
}

//获取到Launcher的Intent
String mTopAction = Intent.ACTION_MAIN;
//返回的intent action是Intent.ACTION_MAIN   category是Intent.CATEGORY_HOME
Intent getHomeIntent() {
    Intent intent = new Intent(mTopAction, mTopData != null ? Uri.parse(mTopData) : null);
    intent.setComponent(mTopComponent);
    intent.addFlags(Intent.FLAG_DEBUG_TRIAGED_MISSING);
    if (mFactoryTest != FactoryTest.FACTORY_TEST_LOW_LEVEL) {
        intent.addCategory(Intent.CATEGORY_HOME);
    }
    return intent;
}
//通过homeIntent向PackageManagerService 找对应的ActivityInfo
ActivityInfo resolveHomeActivity(int userId, Intent homeIntent) {
    final int flags = ActivityManagerService.STOCK_PM_FLAGS;
    final ComponentName comp = homeIntent.getComponent();
    ActivityInfo aInfo = null;
    try {
        if (comp != null) {
            // Factory test.
            aInfo = AppGlobals.getPackageManager().getActivityInfo(comp, flags, userId);
        } else {
            final String resolvedType =
                    homeIntent.resolveTypeIfNeeded(mService.mContext.getContentResolver());
            final ResolveInfo info = AppGlobals.getPackageManager()
                    .resolveIntent(homeIntent, resolvedType, flags, userId);
            if (info != null) {
                aInfo = info.activityInfo;
            }
        }
    } catch (RemoteException e) {
        // ignore
    }

    if (aInfo == null) {
        Slog.wtf(TAG, "No home screen found for " + homeIntent, new Throwable());
        return null;
    }

    aInfo = new ActivityInfo(aInfo);
    aInfo.applicationInfo = mService.getAppInfoForUser(aInfo.applicationInfo, userId);
    return aInfo;
}


文件目录`/frameworks/base/services/core/java/com/android/server/wm/ActivityStartController.java`

void startHomeActivity(Intent intent, ActivityInfo aInfo, String reason, int displayId) {
    final ActivityOptions options = ActivityOptions.makeBasic();
    options.setLaunchWindowingMode(WINDOWING_MODE_FULLSCREEN);
    if (!ActivityRecord.isResolverActivity(aInfo.name)) {
        options.setLaunchActivityType(ACTIVITY_TYPE_HOME);
    }
    options.setLaunchDisplayId(displayId);
    mLastHomeActivityStartResult = obtainStarter(intent, "startHomeActivity: " + reason)
            .setOutActivity(tmpOutRecord)
            .setCallingUid(0)
            .setActivityInfo(aInfo)
            .setActivityOptions(options.toBundle())
            .execute();//这里执行开启activity 我们先不跟了，后边等将AMS的startActivity的时候再详解。
    mLastHomeActivityStartRecord = tmpOutRecord[0];
    final ActivityDisplay display =
            mService.mRootActivityContainer.getActivityDisplay(displayId);
    final ActivityStack homeStack = display != null ? display.getHomeStack() : null;
    if (homeStack != null && homeStack.mInResumeTopActivity) {
    //如果已经开启过 就重新拉回栈顶
        mSupervisor.scheduleResumeTopActivities();
    }
}
```

总结下：在`system_server`中调用了`AMS`的`systemReady`,调用`RootActivityContainer`的`startHomeOnAllDisplays` 获取到`Launcher`的`Action`和`Category` 通过`PackageManagerService`来查询到需要开启的`Activity（Launcher）`,这里我们没讲PackageManagerService，后边我们再讲。

## 2.Launcher

我们先来看看Launcher的配置文件。

```
<application
    android:backupAgent="com.android.launcher3.LauncherBackupAgent"
    android:fullBackupOnly="true"
    android:fullBackupContent="@xml/backupscheme"
    android:hardwareAccelerated="true"
    android:icon="@drawable/ic_launcher_home"
    android:label="@string/derived_app_name"
    android:theme="@style/AppTheme"
    android:largeHeap="@bool/config_largeHeap"
    android:restoreAnyVersion="true"
    android:supportsRtl="true" >

   <!--Launcher Activity -->
    <activity
        android:name="com.android.launcher3.Launcher"
        android:launchMode="singleTask"
        android:clearTaskOnLaunch="true"
        android:stateNotNeeded="true"
        android:windowSoftInputMode="adjustPan"
        android:screenOrientation="unspecified"
        android:configChanges="keyboard|keyboardHidden|mcc|mnc|navigation|orientation|screenSize|screenLayout|smallestScreenSize"
        android:resizeableActivity="true"
        android:resumeWhilePausing="true"
        android:taskAffinity=""
        android:enabled="true">
        <intent-filter>
            <action android:name="android.intent.action.MAIN" />
            <category android:name="android.intent.category.HOME" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.MONKEY"/>
            <category android:name="android.intent.category.LAUNCHER_APP" />
        </intent-filter>
        <meta-data
            android:name="com.android.launcher3.grid.control"
            android:value="${packageName}.grid_control" />
    </activity>

</application>
```

只配置了一个`Activity`。我们看看这个Activity。首先看看onCreate

```
protected void onCreate(Bundle savedInstanceState) {
LauncherAppState app = LauncherAppState.getInstance(this);
//获取到LauncherModel,用来查询我们安装过的App
mModel = app.setLauncher(this);
}

文件目录:`/packages/apps/Launcher3/src/com/android/launcher3/LauncherAppState.java`
private LauncherAppState(Context context) {
    //创建LauncherModel
    mModel = new LauncherModel(this, mIconCache, AppFilter.newInstance(mContext));
    //调用startLoader开启查询App信息
    if (!mModel.startLoader(currentScreen)) {
        if (!internalStateHandled) {
            mDragLayer.getAlphaProperty(ALPHA_INDEX_LAUNCHER_LOAD).setValue(0);
        }
    }

}

LauncherModel setLauncher(Launcher launcher) {
    //调用initialize函数进行初始化
    mModel.initialize(launcher);
    return mModel;
}

```

在`onCreate`中初始化了`mModel(LauncherModel)`并且调用`startLoader`加载App信息。我们先看看初始化`initialize`。

```
public void initialize(Callbacks callbacks) {
    synchronized (mLock) {
        Preconditions.assertUIThread();
        //指定了CallBack为Launcher
        mCallbacks = new WeakReference<>(callbacks);
    }
}
```

在`initialize`中设置了各种回调。再来看看关键函数`startLoader`

```
public boolean startLoader(int synchronousBindPage) {
//……………………
    synchronized (mLock) {
        
        if (mCallbacks != null && mCallbacks.get() != null) {
            final Callbacks oldCallbacks = mCallbacks.get();
            
            mUiExecutor.execute(oldCallbacks::clearPendingBinds);

            stopLoader();
            LoaderResults loaderResults = new LoaderResults(mApp, sBgDataModel,
                    mBgAllAppsList, synchronousBindPage, mCallbacks);
            if (mModelLoaded && !mIsLoaderTaskRunning) {
                //加载过了就绑定数据
                loaderResults.bindWorkspace();
                loaderResults.bindAllApps();
                loaderResults.bindDeepShortcuts();
                loaderResults.bindWidgets();
                return true;
            } else {
            //如果没有加载就加载数据
                startLoaderForResults(loaderResults);
            }
        }
    }
    return false;
}

//创建了LoaderTask 来进行数据的查询
public void startLoaderForResults(LoaderResults results) {
    synchronized (mLock) {
        stopLoader();
        mLoaderTask = new LoaderTask(mApp, mBgAllAppsList, sBgDataModel, results);
        sWorker.post(mLoaderTask);
    }
}

文件目录:`/packages/apps/Launcher3/src/com/android/launcher3/model/LoaderTask.java`
class LoaderTask implements Runnable 继承子Runnable 那么我们就直接看run函数。

public void run() {
    synchronized (this) {
        if (mStopped) {
            return;
        }
    }

    TraceHelper.beginSection(TAG);
    try (LauncherModel.LoaderTransaction transaction = mApp.getModel().beginLoader(this)) {
        TraceHelper.partitionSection(TAG, "step 1.1: loading workspace");
        //加载工作区
        loadWorkspace();

        verifyNotStopped();
        TraceHelper.partitionSection(TAG, "step 1.2: bind workspace workspace");
        //绑定工作区
        mResults.bindWorkspace();

        TraceHelper.partitionSection(TAG, "step 1.3: send first screen broadcast");
        sendFirstScreenActiveInstallsBroadcast();

        TraceHelper.partitionSection(TAG, "step 1 completed, wait for idle");
        waitForIdle();
        verifyNotStopped();

        TraceHelper.partitionSection(TAG, "step 2.1: loading all apps");
        //加载安装的app
        List<LauncherActivityInfo> allActivityList = loadAllApps();

        TraceHelper.partitionSection(TAG, "step 2.2: Binding all apps");
        verifyNotStopped();
        //绑定安装的app
        mResults.bindAllApps();

        verifyNotStopped();
        TraceHelper.partitionSection(TAG, "step 2.3: Update icon cache");
        IconCacheUpdateHandler updateHandler = mIconCache.getUpdateHandler();
        setIgnorePackages(updateHandler);
        updateHandler.updateIcons(allActivityList,
                new LauncherActivtiyCachingLogic(mApp.getIconCache()),
                mApp.getModel()::onPackageIconsUpdated);

        TraceHelper.partitionSection(TAG, "step 2 completed, wait for idle");
        waitForIdle();
        verifyNotStopped();

        TraceHelper.partitionSection(TAG, "step 3.1: loading deep shortcuts");
        loadDeepShortcuts();

        verifyNotStopped();
        TraceHelper.partitionSection(TAG, "step 3.2: bind deep shortcuts");
        mResults.bindDeepShortcuts();

        TraceHelper.partitionSection(TAG, "step 3 completed, wait for idle");
        waitForIdle();
        verifyNotStopped();

        TraceHelper.partitionSection(TAG, "step 4.1: loading widgets");
        List<ComponentWithLabel> allWidgetsList = mBgDataModel.widgetsModel.update(mApp, null);

        verifyNotStopped();
        TraceHelper.partitionSection(TAG, "step 4.2: Binding widgets");
        mResults.bindWidgets();

        verifyNotStopped();
        TraceHelper.partitionSection(TAG, "step 4.3: Update icon cache");
        updateHandler.updateIcons(allWidgetsList, new ComponentCachingLogic(mApp.getContext()),
                mApp.getModel()::onWidgetLabelsUpdated);

        verifyNotStopped();
        TraceHelper.partitionSection(TAG, "step 5: Finish icon cache update");
        updateHandler.finish();

        transaction.commit();
    } catch (CancellationException e) {
        TraceHelper.partitionSection(TAG, "Cancelled");
    }
    TraceHelper.endSection(TAG);
}

//直接看查询所有App的函数
private List<LauncherActivityInfo> loadAllApps() {
    final List<UserHandle> profiles = mUserManager.getUserProfiles();
    List<LauncherActivityInfo> allActivityList = new ArrayList<>();
    mBgAllAppsList.clear();
    for (UserHandle user : profiles) {
        //查询app list
        final List<LauncherActivityInfo> apps = mLauncherApps.getActivityList(null, user);
        if (apps == null || apps.isEmpty()) {
            return allActivityList;
        }
        boolean quietMode = mUserManager.isQuietModeEnabled(user);
        for (int i = 0; i < apps.size(); i++) {
            LauncherActivityInfo app = apps.get(i);
            //查询完以后添加到mBgAllAppsList
            mBgAllAppsList.add(new AppInfo(app, user, quietMode), app);
        }
        allActivityList.addAll(apps);
    }

    if (FeatureFlags.LAUNCHER3_PROMISE_APPS_IN_ALL_APPS) {
        for (PackageInstaller.SessionInfo info :
                mPackageInstaller.getAllVerifiedSessions()) {
            mBgAllAppsList.addPromiseApp(mApp.getContext(),
                    PackageInstallerCompat.PackageInstallInfo.fromInstallingState(info));
        }
    }

    mBgAllAppsList.added = new ArrayList<>();
    return allActivityList;
}

```

调用了`mLauncherApps`的`getActivityList`，`mLauncherApps`其实就是`LauncherAppsCompatVL`。

```
//调用了mLauncherApps的getActivityList  packageName是Null
public List<LauncherActivityInfo> getActivityList(String packageName, UserHandle user) {
    return mLauncherApps.getActivityList(packageName, user);
}

//mLauncherApps就是系统提供的一个服务，这里就用到了Binder了，所以我们这里获取的就是BinderProxy 我们直接看Service端
mLauncherApps = (LauncherApps) context.getSystemService(Context.LAUNCHER_APPS_SERVICE);

有兴趣大家可以自己跟一下
Service端的代码：
文件目录：`/frameworks/base/services/core/java/com/android/server/pm/LauncherAppsService.java`

public ParceledListSlice<ResolveInfo> getLauncherActivities(String callingPackage,
        String packageName, UserHandle user) throws RemoteException {
        //查询的是Action = ACTION_MAIN category = CATEGORY_LAUNCHER
    ParceledListSlice<ResolveInfo> launcherActivities = queryActivitiesForUser(
            callingPackage,
            new Intent(Intent.ACTION_MAIN)
                    .addCategory(Intent.CATEGORY_LAUNCHER)
                    .setPackage(packageName),
            user);
        //……………………
}


private ParceledListSlice<ResolveInfo> queryActivitiesForUser(String callingPackage,
        Intent intent, UserHandle user) {
    if (!canAccessProfile(user.getIdentifier(), "Cannot retrieve activities")) {
        return null;
    }

    final int callingUid = injectBinderCallingUid();
    long ident = injectClearCallingIdentity();
    try {
        //调用PackageManagerService来查询所有的ResolveInfo
        final PackageManagerInternal pmInt =
                LocalServices.getService(PackageManagerInternal.class);
        List<ResolveInfo> apps = pmInt.queryIntentActivities(intent,
                PackageManager.MATCH_DIRECT_BOOT_AWARE
                        | PackageManager.MATCH_DIRECT_BOOT_UNAWARE,
                callingUid, user.getIdentifier());
        return new ParceledListSlice<>(apps);
    } finally {
        injectRestoreCallingIdentity(ident);
    }
}

```

通过`Binder`调用了`LauncherAppsService`接着调用了`PackageManagerService`的`queryIntentActivities`函数 查询到所有符合条件的`ResolveInfo`返回。这里有个疑问，这里调用PackageManagerService不就又跨进程了一次吗？大家知道这里为什么要这样写吗？

接着来看`LoadTask`,`loadAllApps`之后调用了`bindAllApps`。 文件目录:`/packages/apps/Launcher3/src/com/android/launcher3/model/BaseLoaderResults.java`

```
public void bindAllApps() {
    ArrayList<AppInfo> list = (ArrayList<AppInfo>) mBgAllAppsList.data.clone();
    //这里的c就是CallBack 由Launcher实现 回到Launcher中
    executeCallbacksTask(c -> c.bindAllApplications(list), mUiExecutor);
}


public void bindAllApplications(ArrayList<AppInfo> apps) {
    mAppsView.getAppsStore().setApps(apps);
}


文件目录:
/packages/apps/Launcher3/src/com/android/launcher3/allapps/AllAppsContainerView.java
/packages/apps/Launcher3/src/com/android/launcher3/allapps/AllAppsStore.java


public AllAppsStore getAppsStore() {
    return mAllAppsStore;
}

public void setApps(List<AppInfo> apps) {
    mComponentToAppMap.clear();
    addOrUpdateApps(apps);
}


public void addOrUpdateApps(List<AppInfo> apps) {
    for (AppInfo app : apps) {//所有的app存储到map中
        mComponentToAppMap.put(app.toComponentKey(), app);
    }
    notifyUpdate();
}


private void notifyUpdate() {
    if (mDeferUpdatesFlags != 0) {
        mUpdatePending = true;
        return;
    }
    int count = mUpdateListeners.size();
    for (int i = 0; i < count; i++) {
    //回调回去AllAppContainerView
        mUpdateListeners.get(i).onAppsUpdated();
    }
}


private void onAppsUpdated() {
    if (FeatureFlags.ALL_APPS_TABS_ENABLED) {
        boolean hasWorkApps = false;
        for (AppInfo app : mAllAppsStore.getApps()) {
            if (mWorkMatcher.matches(app, null)) {
                hasWorkApps = true;
                break;
            }
        }
        //绑定数据
        rebindAdapters(hasWorkApps);
    }
}

private void rebindAdapters(boolean showTabs) {
    rebindAdapters(showTabs, false /* force */);
}


private void rebindAdapters(boolean showTabs, boolean force) {
     //……………………
    //替换RecyclerView的内容
    replaceRVContainer(showTabs);
    mUsingTabs = showTabs;
    //……………………
    ```
    if (mUsingTabs) {//分页策略 这里是false
        mAH[AdapterHolder.MAIN].setup(mViewPager.getChildAt(0), mPersonalMatcher);
        mAH[AdapterHolder.WORK].setup(mViewPager.getChildAt(1), mWorkMatcher);
        onTabChanged(mViewPager.getNextPage());
    } else {
        mAH[AdapterHolder.MAIN].setup(findViewById(R.id.apps_list_view), null);
        //设置recyclerView为Null
        mAH[AdapterHolder.WORK].recyclerView = null;
    }

}


void setup(@NonNull View rv, @Nullable ItemInfoMatcher matcher) {
    appsList.updateItemFilter(matcher);
    recyclerView = (AllAppsRecyclerView) rv;
    recyclerView.setEdgeEffectFactory(createEdgeEffectFactory());
    recyclerView.setApps(appsList, mUsingTabs);//设置数据源 appList是AlphabeticalAppsList类型 在查询出来所有app之后update 会更新数据源
    recyclerView.setLayoutManager(layoutManager);//设置layoutmanager
    recyclerView.setAdapter(adapter);//设置adapter
    recyclerView.setHasFixedSize(true);
    // No animations will occur when changes occur to the items in this RecyclerView.
    recyclerView.setItemAnimator(null);
    FocusedItemDecorator focusedItemDecorator = new FocusedItemDecorator(recyclerView);
    recyclerView.addItemDecoration(focusedItemDecorator);
    adapter.setIconFocusListener(focusedItemDecorator.getFocusListener());
    applyVerticalFadingEdgeEnabled(verticalFadingEdge);
    applyPadding();
}



private void replaceRVContainer(boolean showTabs) {
    for (int i = 0; i < mAH.length; i++) {
        if (mAH[i].recyclerView != null) {
            mAH[i].recyclerView.setLayoutManager(null);
        }
    }
    View oldView = getRecyclerViewContainer();
    int index = indexOfChild(oldView);
    removeView(oldView);
    int layout = showTabs ? R.layout.all_apps_tabs : R.layout.all_apps_rv_layout;
    View newView = LayoutInflater.from(getContext()).inflate(layout, this, false);
    addView(newView, index);
    if (showTabs) {
        mViewPager = (AllAppsPagedView) newView;
        mViewPager.initParentViews(this);
        mViewPager.getPageIndicator().setContainerView(this);
    } else {
        mViewPager = null;
    }
}

```

查询完成之后在`AllAppsContainerView`的`rebindAdapters`中设置数据源`appList`、`adapter`、`LayoutManager`等。appList的类型是`AlphabeticalAppList`,也实现了`AllAppsStore.OnUpdateListener`当LoadTask查询完之后也会update到这里来，函数如下:

```
public void onAppsUpdated() {
    // Sort the list of apps
    mApps.clear();

    for (AppInfo app : mAllAppsStore.getApps()) {
        if (mItemFilter == null || mItemFilter.matches(app, null) || hasFilter()) {
            mApps.add(app);
        }
    }
    //排序
    Collections.sort(mApps, mAppNameComparator);
    Locale curLocale = mLauncher.getResources().getConfiguration().locale;
    boolean localeRequiresSectionSorting = curLocale.equals(Locale.SIMPLIFIED_CHINESE);
    if (localeRequiresSectionSorting) {
        TreeMap<String, ArrayList<AppInfo>> sectionMap = new TreeMap<>(new LabelComparator());
        for (AppInfo info : mApps) {
            String sectionName = getAndUpdateCachedSectionName(info.title);

            ArrayList<AppInfo> sectionApps = sectionMap.get(sectionName);
            if (sectionApps == null) {
                sectionApps = new ArrayList<>();
                sectionMap.put(sectionName, sectionApps);
            }
            sectionApps.add(info);
        }
        mApps.clear();
        for (Map.Entry<String, ArrayList<AppInfo>> entry : sectionMap.entrySet()) {
            mApps.addAll(entry.getValue());
        }
    } else {
        for (AppInfo info : mApps) {
            // Add the section to the cache
            getAndUpdateCachedSectionName(info.title);
        }
    }
    updateAdapterItems();
}

//更新adapter的items
private void updateAdapterItems() {
    //重新填充adapter的items
    refillAdapterItems();
    //刷新recyclerView
    refreshRecyclerView();
}


private void refillAdapterItems() {
    String lastSectionName = null;
    FastScrollSectionInfo lastFastScrollerSectionInfo = null;
    int position = 0;
    int appIndex = 0;
    mFilteredApps.clear();
    mFastScrollerSections.clear();
    mAdapterItems.clear();//清空recyclerView的内容
    //获取所有的应用 并且创建AdapterItem 添加到mAdapterItems和mFilteredApps
    for (AppInfo info : getFiltersAppInfos()) {
        String sectionName = getAndUpdateCachedSectionName(info.title);
        if (!sectionName.equals(lastSectionName)) {
            lastSectionName = sectionName;
            lastFastScrollerSectionInfo = new FastScrollSectionInfo(sectionName);
            mFastScrollerSections.add(lastFastScrollerSectionInfo);
        }
        AdapterItem appItem = AdapterItem.asApp(position++, sectionName, info, appIndex++);
        if (lastFastScrollerSectionInfo.fastScrollToItem == null) {
            lastFastScrollerSectionInfo.fastScrollToItem = appItem;
        }
        mAdapterItems.add(appItem);
        mFilteredApps.add(info);
    }

    if (hasFilter()) {//如果正在搜索
        if (hasNoFilteredResults()) {
            mAdapterItems.add(AdapterItem.asEmptySearch(position++));
        } else {
            mAdapterItems.add(AdapterItem.asAllAppsDivider(position++));
        }
        mAdapterItems.add(AdapterItem.asMarketSearch(position++));
    }

    if (mNumAppsPerRow != 0) {//记录行列数
        int numAppsInSection = 0;
        int numAppsInRow = 0;
        int rowIndex = -1;
        for (AdapterItem item : mAdapterItems) {
            item.rowIndex = 0;
            if (AllAppsGridAdapter.isDividerViewType(item.viewType)) {
                numAppsInSection = 0;
            } else if (AllAppsGridAdapter.isIconViewType(item.viewType)) {
                if (numAppsInSection % mNumAppsPerRow == 0) {
                    numAppsInRow = 0;
                    rowIndex++;
                }
                item.rowIndex = rowIndex;
                item.rowAppIndex = numAppsInRow;
                numAppsInSection++;
                numAppsInRow++;
            }
        }
        mNumAppRowsInAdapter = rowIndex + 1;
        //完成右边用于快速滑到某一页的滑动条
        switch (mFastScrollDistributionMode) {
            case FAST_SCROLL_FRACTION_DISTRIBUTE_BY_ROWS_FRACTION:
                float rowFraction = 1f / mNumAppRowsInAdapter;
                for (FastScrollSectionInfo info : mFastScrollerSections) {
                    AdapterItem item = info.fastScrollToItem;
                    if (!AllAppsGridAdapter.isIconViewType(item.viewType)) {
                        info.touchFraction = 0f;
                        continue;
                    }

                    float subRowFraction = item.rowAppIndex * (rowFraction / mNumAppsPerRow);
                    info.touchFraction = item.rowIndex * rowFraction + subRowFraction;
                }
                break;
            case FAST_SCROLL_FRACTION_DISTRIBUTE_BY_NUM_SECTIONS:
                float perSectionTouchFraction = 1f / mFastScrollerSections.size();
                float cumulativeTouchFraction = 0f;
                for (FastScrollSectionInfo info : mFastScrollerSections) {
                    AdapterItem item = info.fastScrollToItem;
                    if (!AllAppsGridAdapter.isIconViewType(item.viewType)) {
                        info.touchFraction = 0f;
                        continue;
                    }
                    info.touchFraction = cumulativeTouchFraction;
                    cumulativeTouchFraction += perSectionTouchFraction;
                }
                break;
        }
    }
    if (shouldShowWorkFooter()) {
        mAdapterItems.add(AdapterItem.asWorkTabFooter(position++));
    }
}



private void refreshRecyclerView() {
    if (TestProtocol.sDebugTracing) {
        android.util.Log.d(TestProtocol.NO_START_TAG,
                "refreshRecyclerView @ " + android.util.Log.getStackTraceString(
                        new Throwable()));
    }
    if (mAdapter != null) {
    //调用adapter的notifyDataSetChanged进行刷新
        mAdapter.notifyDataSetChanged();
    }
}

```

以上就把所有的应用信息都填充到了`AllAppsContainerView`中了，现在`Launcher`就有了我们安装的应用数据了。 接下来看看点击事件。 文件目录:`/packages/apps/Launcher3/src/com/android/launcher3/allapps/AllAppsGridAdapter.java`

```
public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
    switch (viewType) {
        case VIEW_TYPE_ICON:
            BubbleTextView icon = (BubbleTextView) mLayoutInflater.inflate(
                    R.layout.all_apps_icon, parent, false);
            //在这里设置了点击事件
            icon.setOnClickListener(ItemClickHandler.INSTANCE);
            icon.setOnLongClickListener(ItemLongClickListener.INSTANCE_ALL_APPS);
            icon.setLongPressTimeoutFactor(1f);
            icon.setOnFocusChangeListener(mIconFocusListener);
            icon.getLayoutParams().height = mLauncher.getDeviceProfile().allAppsCellHeightPx;
            return new ViewHolder(icon);
   
    }
}

private static void onClick(View v, String sourceContainer) {
    if (TestProtocol.sDebugTracing) {
        android.util.Log.d(TestProtocol.NO_START_TAG,
                "onClick 1");
    }
    if (v.getWindowToken() == null) {
        if (TestProtocol.sDebugTracing) {
            android.util.Log.d(TestProtocol.NO_START_TAG,
                    "onClick 2");
        }
        return;
    }

    Launcher launcher = Launcher.getLauncher(v.getContext());
    if (!launcher.getWorkspace().isFinishedSwitchingState()) {
        if (TestProtocol.sDebugTracing) {
            android.util.Log.d(TestProtocol.NO_START_TAG,
                    "onClick 3");
        }
        return;
    }

    Object tag = v.getTag();//根据tag来区分 那么我们应用item的tag是什么呢？我们看看数据填充的函数 知道设置的tag是AppInfo
   if (tag instanceof AppInfo) {
        if (TestProtocol.sDebugTracing) {
            android.util.Log.d(TestProtocol.NO_START_TAG,
                    "onClick 4");
        }
        //在这里开启activity
        startAppShortcutOrInfoActivity(v, (AppInfo) tag, launcher,
                sourceContainer == null ? CONTAINER_ALL_APPS: sourceContainer);
    }
}


public void onBindViewHolder(ViewHolder holder, int position) {
    switch (holder.getItemViewType()) {
        case VIEW_TYPE_ICON:
            AppInfo info = mApps.getAdapterItems().get(position).appInfo;
            BubbleTextView icon = (BubbleTextView) holder.itemView;
            icon.reset();
            //在这里设置tag 所以我们的tag是AppInfo
            icon.applyFromApplicationInfo(info);
            break;
    }
    if (mBindViewCallback != null) {
        mBindViewCallback.onBindView(holder);
    }
}

```

通过`ViewHolder`的创建和绑定我们知道给itemView设置的`tag`是`AppInfo`，所以再点击事件中`tag`就是`AppInfo`，调用了`startAppShortcutOrInfoActivity`。

```
private static void startAppShortcutOrInfoActivity(View v, ItemInfo item, Launcher launcher,
        @Nullable String sourceContainer) {
    Intent intent;
    if (item instanceof PromiseAppInfo) {
        PromiseAppInfo promiseAppInfo = (PromiseAppInfo) item;
        intent = promiseAppInfo.getMarketIntent(launcher);
    } else {//拿到对应的intent 从PackageManagerService中查询出来的
        intent = item.getIntent();
    }
    if (intent == null) {
        throw new IllegalArgumentException("Input must have a valid intent");
    }
    if (item instanceof WorkspaceItemInfo) {
        WorkspaceItemInfo si = (WorkspaceItemInfo) item;
        if (si.hasStatusFlag(WorkspaceItemInfo.FLAG_SUPPORTS_WEB_UI)
                && Intent.ACTION_VIEW.equals(intent.getAction())) {
            intent = new Intent(intent);
            intent.setPackage(null);
        }
    }
    if (v != null && launcher.getAppTransitionManager().supportsAdaptiveIconAnimation()) {
        FloatingIconView.fetchIcon(launcher, v, item, true /* isOpening */);
    }
    //调用launcher的startActivitySafely
    launcher.startActivitySafely(v, intent, item, sourceContainer);
}

在Launcher中
public boolean startActivitySafely(View v, Intent intent, ItemInfo item,
        @Nullable String sourceContainer) {

    if (!hasBeenResumed()) {
        addOnResumeCallback(() -> startActivitySafely(v, intent, item, sourceContainer));
        UiFactory.clearSwipeSharedState(true /* finishAnimation */);
        return true;
    }
    //调用startActivitySafely来开启activity
    boolean success = super.startActivitySafely(v, intent, item, sourceContainer);
    if (success && v instanceof BubbleTextView) {
        BubbleTextView btv = (BubbleTextView) v;
        btv.setStayPressed(true);
        addOnResumeCallback(btv);
    }
    return success;
}


public boolean startActivitySafely(View v, Intent intent, @Nullable ItemInfo item,
        @Nullable String sourceContainer) {
    Bundle optsBundle = (v != null) ? getActivityLaunchOptionsAsBundle(v) : null;
    UserHandle user = item == null ? null : item.user;
    //设置flag为new_task
    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    if (v != null) {
        intent.setSourceBounds(getViewBounds(v));
    }
    try {
        boolean isShortcut = (item instanceof WorkspaceItemInfo)
                && (item.itemType == Favorites.ITEM_TYPE_SHORTCUT
                || item.itemType == Favorites.ITEM_TYPE_DEEP_SHORTCUT)
                && !((WorkspaceItemInfo) item).isPromise();
        if (isShortcut) {
            startShortcutIntentSafely(intent, optsBundle, item, sourceContainer);
        } else if (user == null || user.equals(Process.myUserHandle())) {//到这里来
             //调用startActivity进入开启Activity的流程。
            startActivity(intent, optsBundle);
            AppLaunchTracker.INSTANCE.get(this).onStartApp(intent.getComponent(),
                    Process.myUserHandle(), sourceContainer);
        } else {
            LauncherAppsCompat.getInstance(this).startActivityForProfile(
                    intent.getComponent(), user, intent.getSourceBounds(), optsBundle);
            AppLaunchTracker.INSTANCE.get(this).onStartApp(intent.getComponent(), user,
                    sourceContainer);
        }
        getUserEventDispatcher().logAppLaunch(v, intent);
        getStatsLogManager().logAppLaunch(v, intent);
        return true;
    } catch (ActivityNotFoundException|SecurityException e) {
    }
    return false;
}

```

## Launcher 总结

1.Launcher是由`system_server`启动，在开启其他服务`startOtherServices`中，调用了`AMS`的`systemReady`,在systemReady中调用了`startHomeOnAllDisplays`,通过`Intent`来找到对应的Launcher，条件是`action=ACTION_MAIN,CATEGORY=CATEGORY_HOME`，找到后开启Launcher

2.Launcher中初始化了`LauncherModel`并设置了回调函数(`CallBacks`)调用`startLoader`函数，创建`LoadTask`，通过`Binder`访问`LauncherAppService`的`queryIntentActivies`再调用`PackageManagerService`的`queryIntentActivities`获取到`Launcher`的信息返回

3.在`LoadTask`的回调`onAppsUpdated`调用`rebindAdapters`对数据进行绑定 填充

4.在`ViewHolder`的创建中设置`点击事件`，在绑定中设置`tag`为`AppInfo`,在点击事件中通过`tag`调用`startAppShortcutOrInfoActivity`最终调用到`Activity`的`startActivity`函数。

## 3.开机动画

在开启`Launcher`之后会关闭掉开机动画,在此之前我们先看看开机动画在哪里开启的。得回到我们的老朋友(init)中。

代码目录：`/frameworks/base/cmds/bootanimation/bootanim.rc`

```
service bootanim /system/bin/bootanimation
    class core animation
```

这里我就不详细赘述 配置文件了，感兴趣的可以去之前的文章中 寻找答案。 它的代码是 `frameworks/base/cmds/bootanimation/BootAnimation.cpp` `frameworks/base/cmds/bootanimation/bootanimation_main.cpp`。

```
int main()
{
    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_DISPLAY);

    bool noBootAnimation = bootAnimationDisabled();//判断是否需要开机动画
    ALOGI_IF(noBootAnimation,  "boot animation disabled");
    if (!noBootAnimation) {

        sp<ProcessState> proc(ProcessState::self());
        ProcessState::self()->startThreadPool();

        // create the boot animation object (may take up to 200ms for 2MB zip)
        sp<BootAnimation> boot = new BootAnimation(audioplay::createAnimationCallbacks());

        waitForSurfaceFlinger();//等待SurfaceFlinger 因为我们想要画到屏幕上就必须要通过SurfaceFlinger
        //调用run函数
        boot->run("BootAnimation", PRIORITY_DISPLAY);

        ALOGV("Boot animation set up. Joining pool.");

        IPCThreadState::self()->joinThreadPool();
    }
    return 0;
}



//从属性服务中获取是否开启开机动画
bool bootAnimationDisabled() {
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.sf.nobootanimation", value, "0");
    if (atoi(value) > 0) {
        return true;
    }

    property_get("ro.boot.quiescent", value, "0");
    return atoi(value) > 0;
}
BootAnimation继承自Thread，
关于Thread之前没有讲 大家可以参考`/system/core/libutils/Threads.cpp`，第一次会调用readyToRun。然后会调用threadLoop


bool BootAnimation::threadLoop()
{
    bool r;
    if (mZipFileName.isEmpty()) {
        r = android();
    } else {//走这里,调用movie函数
        r = movie();
    }

    eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(mDisplay, mContext);
    eglDestroySurface(mDisplay, mSurface);
    mFlingerSurface.clear();
    mFlingerSurfaceControl.clear();
    eglTerminate(mDisplay);
    eglReleaseThread();
    IPCThreadState::self()->stopProcess();
    return r;
}

bool BootAnimation::movie()
{
    if (mAnimation == nullptr) {
    //加载动画资源 这里我就不跟了 感兴趣的可以自行查看
        mAnimation = loadAnimation(mZipFileName);
    }

    if (mAnimation == nullptr)
        return false;

    // mCallbacks->init() may get called recursively,
    // this loop is needed to get the same results
    for (const Animation::Part& part : mAnimation->parts) {
        if (part.animation != nullptr) {
            mCallbacks->init(part.animation->parts);
        }
    }
    mCallbacks->init(mAnimation->parts);

    bool anyPartHasClock = false;
    for (size_t i=0; i < mAnimation->parts.size(); i++) {
        if(validClock(mAnimation->parts[i])) {
            anyPartHasClock = true;
            break;
        }
    }
    if (!anyPartHasClock) {
        mClockEnabled = false;
    }

    mUseNpotTextures = false;
    String8 gl_extensions;
    const char* exts = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (!exts) {
        glGetError();
    } else {
        gl_extensions.setTo(exts);
        if ((gl_extensions.find("GL_ARB_texture_non_power_of_two") != -1) ||
            (gl_extensions.find("GL_OES_texture_npot") != -1)) {
            mUseNpotTextures = true;
        }
    }

    // Blend required to draw time on top of animation frames.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(GL_FLAT);
    glDisable(GL_DITHER);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_TEXTURE_2D);
    glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    bool clockFontInitialized = false;
    if (mClockEnabled) {
        clockFontInitialized =
            (initFont(&mAnimation->clockFont, CLOCK_FONT_ASSET) == NO_ERROR);
        mClockEnabled = clockFontInitialized;
    }

    if (mClockEnabled && !updateIsTimeAccurate()) {
        mTimeCheckThread = new TimeCheckThread(this);
        mTimeCheckThread->run("BootAnimation::TimeCheckThread", PRIORITY_NORMAL);
    }
    //调用playAnimation 播放动画
    playAnimation(*mAnimation);

    if (mTimeCheckThread != nullptr) {
        mTimeCheckThread->requestExit();
        mTimeCheckThread = nullptr;
    }

    if (clockFontInitialized) {
        glDeleteTextures(1, &mAnimation->clockFont.texture.name);
    }

    releaseAnimation(mAnimation);
    mAnimation = nullptr;

    return false;
}
播放动画 并且调用checkExit进行检测 是否播放完成
bool BootAnimation::playAnimation(const Animation& animation)
{
    const size_t pcount = animation.parts.size();
    nsecs_t frameDuration = s2ns(1) / animation.fps;
    const int animationX = (mWidth - animation.width) / 2;
    const int animationY = (mHeight - animation.height) / 2;

    for (size_t i=0 ; i<pcount ; i++) {
        const Animation::Part& part(animation.parts[i]);
        const size_t fcount = part.frames.size();
        glBindTexture(GL_TEXTURE_2D, 0);

        // Handle animation package
        if (part.animation != nullptr) {
            playAnimation(*part.animation);
            if (exitPending())
                break;
            continue; //to next part
        }

        for (int r=0 ; !part.count || r<part.count ; r++) {
            if(exitPending() && !part.playUntilComplete)
                break;

            mCallbacks->playPart(i, part, r);

            glClearColor(
                    part.backgroundColor[0],
                    part.backgroundColor[1],
                    part.backgroundColor[2],
                    1.0f);

            for (size_t j=0 ; j<fcount && (!exitPending() || part.playUntilComplete) ; j++) {
                const Animation::Frame& frame(part.frames[j]);
                nsecs_t lastFrame = systemTime();

                if (r > 0) {
                    glBindTexture(GL_TEXTURE_2D, frame.tid);
                } else {
                    if (part.count != 1) {
                        glGenTextures(1, &frame.tid);
                        glBindTexture(GL_TEXTURE_2D, frame.tid);
                        glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    }
                    int w, h;
                    initTexture(frame.map, &w, &h);
                }

                const int xc = animationX + frame.trimX;
                const int yc = animationY + frame.trimY;
                Region clearReg(Rect(mWidth, mHeight));
                clearReg.subtractSelf(Rect(xc, yc, xc+frame.trimWidth, yc+frame.trimHeight));
                if (!clearReg.isEmpty()) {
                    Region::const_iterator head(clearReg.begin());
                    Region::const_iterator tail(clearReg.end());
                    glEnable(GL_SCISSOR_TEST);
                    while (head != tail) {
                        const Rect& r2(*head++);
                        glScissor(r2.left, mHeight - r2.bottom, r2.width(), r2.height());
                        glClear(GL_COLOR_BUFFER_BIT);
                    }
                    glDisable(GL_SCISSOR_TEST);
                }
                glDrawTexiOES(xc, mHeight - (yc + frame.trimHeight),
                              0, frame.trimWidth, frame.trimHeight);
                if (mClockEnabled && mTimeIsAccurate && validClock(part)) {
                    drawClock(animation.clockFont, part.clockPosX, part.clockPosY);
                }
                handleViewport(frameDuration);

                eglSwapBuffers(mDisplay, mSurface);

                nsecs_t now = systemTime();
                nsecs_t delay = frameDuration - (now - lastFrame);
                //SLOGD("%lld, %lld", ns2ms(now - lastFrame), ns2ms(delay));
                lastFrame = now;

                if (delay > 0) {
                    struct timespec spec;
                    spec.tv_sec  = (now + delay) / 1000000000;
                    spec.tv_nsec = (now + delay) % 1000000000;
                    int err;
                    do {
                        err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &spec, nullptr);
                    } while (err<0 && errno == EINTR);
                }
                //检测释放播放完成
                checkExit();
            }

            usleep(part.pause * ns2us(frameDuration));
            if(exitPending() && !part.count && mCurrentInset >= mTargetInset)
                break;
        }

    }
    for (const Animation::Part& part : animation.parts) {
        if (part.count != 1) {
            const size_t fcount = part.frames.size();
            for (size_t j = 0; j < fcount; j++) {
                const Animation::Frame& frame(part.frames[j]);
                glDeleteTextures(1, &frame.tid);
            }
        }
    }

    return true;
}

//检测是否完成
void BootAnimation::checkExit() {
    char value[PROPERTY_VALUE_MAX];
    //根据属性 来判断是否完成
    property_get(EXIT_PROP_NAME, value, "0");
    int exitnow = atoi(value);
    if (exitnow) {
        requestExit();//退出BootAnimation
        mCallbacks->shutdown();
    }
}

文件目录:`/frameworks/base/cmds/bootanimation/audioplay.cpp`
//mCallBacks 音频播放器也需要关闭
void shutdown() override {
    audioplay::setPlaying(false);
    audioplay::destroy();
};


//做一些准备工作，例如创建surface 设置layer，纹理等。
status_t BootAnimation::readyToRun() {
    mAssets.addDefaultAssets();

    mDisplayToken = SurfaceComposerClient::getInternalDisplayToken();
    if (mDisplayToken == nullptr)
        return -1;

    DisplayInfo dinfo;
    status_t status = SurfaceComposerClient::getDisplayInfo(mDisplayToken, &dinfo);
    if (status)
        return -1;

    //创建Native的Surface
    sp<SurfaceControl> control = session()->createSurface(String8("BootAnimation"),
            dinfo.w, dinfo.h, PIXEL_FORMAT_RGB_565);

    SurfaceComposerClient::Transaction t;
    //设置Layer
    t.setLayer(control, 0x40000000)
        .apply();

    sp<Surface> s = control->getSurface();

    // initialize opengl and egl
    const EGLint attribs[] = {
            EGL_RED_SIZE,   8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE,  8,
            EGL_DEPTH_SIZE, 0,
            EGL_NONE
    };
    EGLint w, h;
    EGLint numConfigs;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(display, nullptr, nullptr);
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    surface = eglCreateWindowSurface(display, config, s.get(), nullptr);
    context = eglCreateContext(display, config, nullptr, nullptr);
    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE)
        return NO_INIT;

    mDisplay = display;
    mContext = context;
    mSurface = surface;
    mWidth = w;
    mHeight = h;
    mFlingerSurfaceControl = control;
    mFlingerSurface = s;
    mTargetInset = -1;

    return NO_ERROR;
}
```

在`bootanimation_main.cpp`的`main`中创建了`BootAnimation`并设置了回调监听`AudioAnimationCallbacks`,`BootAnimation`继承`Thread`所以会先执行`readyToRun`然后执行`threadLoop`,再`readyToRun`中创建`Surface`,`threadLoop`中 进行资源(`loadAnimation`)的加载和播放(`playAnimation`),播放过程中 不停的调用`checkExit`进行检测 检测属性为: `service.bootanim.exit`，如果退出 则退出线程以及关闭音频。

## 4.开机动画结束

开机动画的结束就和`Launcher`有关了。在`Launcher`创建完成之后执行`ActivityThread`的 `handleResumeActivity`函数中给`MessageQueue`插入了一个Idel

```
private class Idler implements MessageQueue.IdleHandler {
    @Override
    public final boolean queueIdle() {
        ActivityClientRecord a = mNewActivities;
        boolean stopProfiling = false;
        if (mBoundApplication != null && mProfiler.profileFd != null
                && mProfiler.autoStopProfiler) {
            stopProfiling = true;
        }
        if (a != null) {
            mNewActivities = null;
            IActivityTaskManager am = ActivityTaskManager.getService();
            ActivityClientRecord prev;
            do {
                if (localLOGV) Slog.v(
                    TAG, "Reporting idle of " + a +
                    " finished=" +
                    (a.activity != null && a.activity.mFinished));
                if (a.activity != null && !a.activity.mFinished) {
                    try {
                    //调用ActivityTaskManagerService的activityIdle
                        am.activityIdle(a.token, a.createdConfig, stopProfiling);
                        a.createdConfig = null;
                    } catch (RemoteException ex) {
                        throw ex.rethrowFromSystemServer();
                    }
                }
                prev = a;
                a = a.nextIdle;
                prev.nextIdle = null;
            } while (a != null);
        }
        if (stopProfiling) {
            mProfiler.stopProfiling();
        }
        applyPendingProcessState();
        return false;//return false  只执行一次
    }
}


public final void activityIdle(IBinder token, Configuration config, boolean stopProfiling) {
    final long origId = Binder.clearCallingIdentity();
    try {
        WindowProcessController proc = null;
        synchronized (mGlobalLock) {
            ActivityStack stack = ActivityRecord.getStackLocked(token);
            if (stack == null) {
                return;
            }
            //调用mStackSupervisor的activityIdleInternalLocked
            final ActivityRecord r = mStackSupervisor.activityIdleInternalLocked(token,
                    false /* fromTimeout */, false /* processPausingActivities */, config);
            if (r != null) {
                proc = r.app;
            }
            if (stopProfiling && proc != null) {
                proc.clearProfilerIfNeeded();
            }
        }
    } finally {
        Binder.restoreCallingIdentity(origId);
    }
}


final ActivityRecord activityIdleInternalLocked(final IBinder token, boolean fromTimeout,
        boolean processPausingActivities, Configuration config) {
  //………………

    ActivityRecord r = ActivityRecord.forTokenLocked(token);
    if (r != null) {
        if (DEBUG_IDLE) Slog.d(TAG_IDLE, "activityIdleInternalLocked: Callers="
                + Debug.getCallers(4));
        mHandler.removeMessages(IDLE_TIMEOUT_MSG, r);
        r.finishLaunchTickingLocked();
        if (fromTimeout) {
            reportActivityLaunchedLocked(fromTimeout, r, INVALID_DELAY,
                    -1 /* launchState */);
        }
        if (config != null) {
            r.setLastReportedGlobalConfiguration(config);
        }

        r.idle = true;

        if ((mService.isBooting() && mRootActivityContainer.allResumedActivitiesIdle())
                || fromTimeout) {
                //调用checkFinishBootingLocked
            booting = checkFinishBootingLocked();
        }
        r.mRelaunchReason = RELAUNCH_REASON_NONE;
    }

    //………………
    return r;
}



private boolean checkFinishBootingLocked() {
    final boolean booting = mService.isBooting();
    boolean enableScreen = false;
    mService.setBooting(false);
    if (!mService.isBooted()) {
        mService.setBooted(true);
        enableScreen = true;
    }
    if (booting || enableScreen) {
    //发送完成动画
        mService.postFinishBooting(booting, enableScreen);
    }
    return booting;
}



void postFinishBooting(boolean finishBooting, boolean enableScreen) {
    mH.post(() -> {
        if (finishBooting) {
            mAmInternal.finishBooting();
        }
        if (enableScreen) {
            //上边的不影响，我们看这里
            mInternal.enableScreenAfterBoot(isBooted());
        }
    });
}


public void enableScreenAfterBoot(boolean booted) {
    synchronized (mGlobalLock) {
        EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_ENABLE_SCREEN,
                SystemClock.uptimeMillis());
                //调用wms的enableScreenAfterBoot
        mWindowManager.enableScreenAfterBoot();
        updateEventDispatchingLocked(booted);
    }
}


    public void enableScreenAfterBoot() {
        synchronized (mGlobalLock) {
            if (mSystemBooted) {
                return;
            }
            mSystemBooted = true;
            hideBootMessagesLocked();
            // If the screen still doesn't come up after 30 seconds, give
            // up and turn it on.
            mH.sendEmptyMessageDelayed(H.BOOT_TIMEOUT, 30 * 1000);
        }

        mPolicy.systemBooted();
        //这里
        performEnableScreen();
    }
    
private void performEnableScreen() {
    synchronized (mGlobalLock) {
     
    //………………

        if (!mBootAnimationStopped) {
            Trace.asyncTraceBegin(TRACE_TAG_WINDOW_MANAGER, "Stop bootanim", 0);
             //设置service.bootanim.exit值为1
            SystemProperties.set("service.bootanim.exit", "1");
            mBootAnimationStopped = true;
        }
    //………………
}
native层代码：
文件目录:/frameworks/base/core/jni/android_os_SystemProperties.cpp


//传递的key是service.bootanim.exit 值是1
void SystemProperties_set(JNIEnv *env, jobject clazz, jstring keyJ,
                          jstring valJ)
{
    auto handler = [&](const std::string& key, bool) {
        std::string val;
        if (valJ != nullptr) {
            ScopedUtfChars key_utf(env, valJ);
            val = key_utf.c_str();
        }
        return android::base::SetProperty(key, val);
    };
    if (!ConvertKeyAndForward(env, keyJ, true, handler)) {
        // Must have been a failure in SetProperty.
        jniThrowException(env, "java/lang/RuntimeException",
                          "failed to set system property");
    }
}

```

在Launcher被创建之后会执行`ActivityThread`的`handleResumeActivity`函数中给`MessageQueue`插入了一个只执行一次的Idel,他会调用`ActivityTaaskManagerService`的`activityIdle`,接着调用到`postFinishBooting`会调用`enableScreenAfterBoot`,进入`WMS`调用`performEnableScreen`调用`SystemProperties.set("service.bootanim.exit", "1")`把值设置成了`1`，结束了动画的播放。

## 开机动画总结

1.在`bootanimation_main.cpp`的`main`中创建了`BootAnimation`并设置了回调监听`AudioAnimationCallbacks`,`BootAnimation`继承`Thread`所以会先执行`readyToRun`然后执行`threadLoop`,再`readyToRun`中创建`Surface`,`threadLoop`中 进行资源(`loadAnimation`)的加载和播放(`playAnimation`),播放过程中 不停的调用`checkExit`进行检测 检测属性为: `service.bootanim.exit`，如果退出 则退出线程以及关闭音频。

2.在Launcher被创建之后会执行`ActivityThrea`d的`handleResumeActivity`函数中给`MessageQueue`插入了一个只执行一次的Idel,他会调用`ActivityTaaskManagerService`的`activityIdle`,接着调用到`postFinishBooting`会调用`enableScreenAfterBoot`,进入`WMS`调用`performEnableScreen`调用`SystemProperties.set("service.bootanim.exit", "1")`把值设置成了`1`，结束了动画的播放。

在线视频:

[www.bilibili.com/video/BV1Ts…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1Ts4y1S7tG%2F "https://www.bilibili.com/video/BV1Ts4y1S7tG/")