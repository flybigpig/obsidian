
``` java
ActivityStarter.startActivityLocked(){

 ......


ActivityRecord r = new ActivityRecord(mService, callerApp, callingPid, callingUid,  
	callingPackage, intent, resolvedType, aInfo, mService.getGlobalConfiguration(),  
	resultRecord, resultWho, requestCode, componentSpecified, voiceSession != null,  
	mSupervisor, checkedOptions, sourceRecord);


.......

}
```

```
ActivityRecord(ActivityTaskManagerService _service, WindowProcessController _caller,  
        int _launchedFromPid, int _launchedFromUid, String _launchedFromPackage, Intent _intent,  
        String _resolvedType, ActivityInfo aInfo, Configuration _configuration,  
        ActivityRecord _resultTo, String _resultWho, int _reqCode, boolean _componentSpecified,  
        boolean _rootVoiceInteraction, ActivityStackSupervisor supervisor,  
        ActivityOptions options, ActivityRecord sourceRecord) {  
    mAtmService = _service;  
    mRootActivityContainer = _service.mRootActivityContainer;  

    appToken = new Token(this, _intent);    // appToken


    info = aInfo;  
    launchedFromPid = _launchedFromPid;  
    launchedFromUid = _launchedFromUid;  
    launchedFromPackage = _launchedFromPackage;  
    mUserId = UserHandle.getUserId(aInfo.applicationInfo.uid);  
    intent = _intent;  
    shortComponentName = _intent.getComponent().flattenToShortString();  
    resolvedType = _resolvedType;  
    componentSpecified = _componentSpecified;  
    rootVoiceInteraction = _rootVoiceInteraction;  
    mLastReportedConfiguration = new MergedConfiguration(_configuration);  
    resultTo = _resultTo;  
    resultWho = _resultWho;  
    requestCode = _reqCode;  
    setState(INITIALIZING, "ActivityRecord ctor");  
    frontOfTask = false;  
    launchFailed = false;  
    stopped = false;  
    delayedResume = false;  
    finishing = false;  
    deferRelaunchUntilPaused = false;  
    keysPaused = false;  
    inHistory = false;  
    visible = false;  
    nowVisible = false;  
    mDrawn = false;  
    idle = false;  
    hasBeenLaunched = false;  
    mStackSupervisor = supervisor;  
  
    // This starts out true, since the initial state of an activity is that we have everything,  
    // and we shouldn't never consider it lacking in state to be removed if it dies.   
     haveState = true;
     
 ......
}
```


```
performLaunchActivity(){

	......

	Window window = null;  
        if (r.mPendingRemoveWindow != null && r.mPreserveWindow) {  
            window = r.mPendingRemoveWindow;  
            r.mPendingRemoveWindow = null;  
            r.mPendingRemoveWindowManager = null;  
        }  
        appContext.setOuterContext(activity);  
        activity.attach(appContext, this, getInstrumentation(), r.token,  
                r.ident, app, r.intent, r.activityInfo, title, r.parent,  
                r.embeddedID, r.lastNonConfigurationInstances, config,  
                r.referrer, r.voiceInteractor, window, r.configCallback,  
                r.assistToken); 
	......
}
```


```
* 
 * @param context 上下文环境，用于访问应用程序特定资源和类
 * @param aThread 活动线程对象，用于在主线程执行操作
 * @param instr 用于监控和测试应用程序的工具对象
 * @param token 代表活动的唯一标识符
 * @param ident 活动的标识符
 * @param application 应用程序实例，用于全局状态管理
 * @param intent 启动活动的意图，包含启动信息和数据
 * @param info 活动的信息，包括配置和属性
 * @param title 活动的标题
 * @param parent 父活动，如果有的话
 * @param id 活动的嵌入ID，如果有的话
 * @param lastNonConfigurationInstances 保存的非配置实例状态
 * @param config 当前的配置信息
 * @param referrer 指示从哪里启动活动的引用者信息
 * @param voiceInteractor 用于语音交互的对象
 * @param window 活动的窗口对象
 * @param activityConfigCallback 活动配置回调对象
 * @param assistToken 辅助功能的令牌
 */
@UnsupportedAppUsage
final void attach(Context context, ActivityThread aThread,
        Instrumentation instr, IBinder token, int ident,
        Application application, Intent intent, ActivityInfo info,
        CharSequence title, Activity parent, String id,
        NonConfigurationInstances lastNonConfigurationInstances,
        Configuration config, String referrer, IVoiceInteractor voiceInteractor,
        Window window, ActivityConfigCallback activityConfigCallback, IBinder assistToken) {

    // 附加基础上下文
    attachBaseContext(context);

    // 附加片段主机，null表示没有父活动
    mFragments.attachHost(null /*parent*/);

    // 初始化电话窗口，并设置相关回调
    mWindow = new PhoneWindow(this, window, activityConfigCallback);
    mWindow.setWindowControllerCallback(this);
    mWindow.setCallback(this);
    mWindow.setOnWindowDismissedCallback(this);
    mWindow.getLayoutInflater().setPrivateFactory(this);

    // 根据活动信息设置软输入模式和UI选项
    if (info.softInputMode != WindowManager.LayoutParams.SOFT_INPUT_STATE_UNSPECIFIED) {
        mWindow.setSoftInputMode(info.softInputMode);
    }
    if (info.uiOptions != 0) {
        mWindow.setUiOptions(info.uiOptions);
    }

    // 设置当前线程为UI线程
    mUiThread = Thread.currentThread();

    // 初始化活动线程、工具、令牌等
    mMainThread = aThread;
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

    // 初始化语音交互对象
    if (voiceInteractor != null) {
        if (lastNonConfigurationInstances != null) {
            mVoiceInteractor = lastNonConfigurationInstances.voiceInteractor;
        } else {
            mVoiceInteractor = new VoiceInteractor(voiceInteractor, this, this,
                    Looper.myLooper());
        }
    }

    // 设置窗口管理器，并根据硬件加速标志配置窗口
    mWindow.setWindowManager(
            (WindowManager)context.getSystemService(Context.WINDOW_SERVICE),
            mToken, mComponent.flattenToString(),
            (info.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED) != 0);

    // 如果有父活动，将当前活动的窗口设置为父活动窗口的容器
    if (mParent != null) {
        mWindow.setContainer(mParent.getWindow());
    }

    // 初始化窗口管理器和当前配置
    mWindowManager = mWindow.getWindowManager();
    mCurrentConfig = config;

    // 设置窗口的颜色模式
    mWindow.setColorMode(info.colorMode);

    // 设置自动填充选项和内容捕获选项
    setAutofillOptions(application.getAutofillOptions());
    setContentCaptureOptions(application.getContentCaptureOptions());
}
```


```
public void setWindowManager(WindowManager wm, IBinder appToken, String appName,  
        boolean hardwareAccelerated) {  
        
    mAppToken = appToken;  
    
    mAppName = appName;  
    mHardwareAccelerated = hardwareAccelerated;  
    if (wm == null) {  
        wm = (WindowManager)mContext.getSystemService(Context.WINDOW_SERVICE);  
    }  
    mWindowManager = ((WindowManagerImpl)wm).createLocalWindowManager(this);  
}
```

AMS通过Activity传递给WMS

```java
//Activity
void makeVisible() {  
    if (!mWindowAdded) {  
        ViewManager wm = getWindowManager();  
        wm.addView(mDecor, getWindow().getAttributes());    //addView
        mWindowAdded = true;  
    }  
    mDecor.setVisibility(View.VISIBLE);  
}
```

```java
// windowmanagerimpl
@Override  
public void addView(@NonNull View view, @NonNull ViewGroup.LayoutParams params) {  
    applyDefaultToken(params);  
    mGlobal.addView(view, params, mContext.getDisplay(), mParentWindow);  
}
```

```java
// windowmanagerGlobal
public void addView(View view, ViewGroup.LayoutParams params,  
        Display display, Window parentWindow) {  

		......
  
        root = new ViewRootImpl(view.getContext(), display);  
  
        view.setLayoutParams(wparams);  
  
        mViews.add(view);  
        mRoots.add(root);  
        mParams.add(wparams);  
  
        // do this last because it fires off messages to start doing things  
        try {  
            root.setView(view, wparams, panelParentView);  
        } catch (RuntimeException e) {  
            // BadTokenException or InvalidDisplayException, clean up.  
            if (index >= 0) {  
                removeViewLocked(index, true);  
            }  
            throw e;  
        }  

		......
}

```

```java
// viewrootimpl
public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {  
    synchronized (this) {  
        if (mView == null) {  
            mView = view;  
  
            ......
            mAdded = true;  
            int res; /* = WindowManagerImpl.ADD_OKAY; */  
  
            // Schedule the first layout -before- adding to the window            // manager, to make sure we do the relayout before receiving            // any other events from the system.            requestLayout();  
            if ((mWindowAttributes.inputFeatures  
                    & WindowManager.LayoutParams.INPUT_FEATURE_NO_INPUT_CHANNEL) == 0) {  
                mInputChannel = new InputChannel();  
            }  
            mForceDecorViewVisibility = (mWindowAttributes.privateFlags  
                    & PRIVATE_FLAG_FORCE_DECOR_VIEW_VISIBILITY) != 0;  
            try {  
                mOrigWindowType = mWindowAttributes.type;  
                mAttachInfo.mRecomputeGlobalAttributes = true;  
                collectViewAttributes();  
                res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,  
                        getHostVisibility(), mDisplay.getDisplayId(), mTmpFrame,  
                        mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,  
                        mAttachInfo.mOutsets, mAttachInfo.mDisplayCutout, mInputChannel,  
                        mTempInsets);  
                setFrame(mTmpFrame);  
            } catch (RemoteException e) {  
                mAdded = false;  
                mView = null;  
                mAttachInfo.mRootView = null;  
                mInputChannel = null;  
                mFallbackEventHandler.setView(null);  
                unscheduleTraversals();  
                setAccessibilityFocus(null, null);  
                throw new RuntimeException("Adding window failed", e);  
            } finally {  
                if (restore) {  
                    attrs.restore();  
                }  
            }  
  
            ......
}

```

```java
//session
@Override  
public int addToDisplay(IWindow window, int seq, WindowManager.LayoutParams attrs,  
        int viewVisibility, int displayId, Rect outFrame, Rect outContentInsets,  
        Rect outStableInsets, Rect outOutsets,  
        DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel,  
        InsetsState outInsetsState) {  
    return mService.addWindow(this, window, seq, attrs, viewVisibility, displayId, outFrame,  
            outContentInsets, outStableInsets, outOutsets, outDisplayCutout, outInputChannel,  
            outInsetsState);  
}

```

```java
// windowmanagerservice
public int addWindow(Session session, IWindow client, int seq,  
        LayoutParams attrs, int viewVisibility, int displayId, Rect outFrame,  
        Rect outContentInsets, Rect outStableInsets, Rect outOutsets,  
        DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel,  
        InsetsState outInsetsState) { 
         
    
  
    synchronized (mGlobalLock) {  
        if (!mDisplayReady) {  
            throw new IllegalStateException("Display has not been initialialized");  
        }  
  
        final DisplayContent displayContent = getDisplayContentOrCreate(displayId, attrs.token);  
  
  
        AppWindowToken atoken = null;  
        final boolean hasParent = parentWindow != null;  
        // Use existing parent window token for child windows since they go in the same token  
        // as there parent window so we can apply the same policy on them.        WindowToken token = displayContent.getWindowToken(  
                hasParent ? parentWindow.mAttrs.token : attrs.token);  
        // If this is a child window, we want to apply the same type checking rules as the  
        // parent window type.        final int rootType = hasParent ? parentWindow.mAttrs.type : type;  
  
        boolean addToastWindowRequiresToken = false;  
  
  
        final WindowState win = new WindowState(this, session, client, token, parentWindow,  
                appOp[0], seq, attrs, viewVisibility, session.mUid,  
                session.mCanAddInternalSystemWindow);  
        
  
        final DisplayPolicy displayPolicy = displayContent.getDisplayPolicy();  
        displayPolicy.adjustWindowParamsLw(win, win.mAttrs, Binder.getCallingPid(),  
                Binder.getCallingUid());  
        win.setShowToOwnerOnlyLocked(mPolicy.checkShowToOwnerOnly(attrs));  
  
        res = displayPolicy.prepareAddWindowLw(win, attrs);  
        if (res != WindowManagerGlobal.ADD_OKAY) {  
            return res;  
        }  
  
        final boolean openInputChannels = (outInputChannel != null  
                && (attrs.inputFeatures & INPUT_FEATURE_NO_INPUT_CHANNEL) == 0);  
        if  (openInputChannels) {  
            win.openInputChannel(outInputChannel);  
        }  
  
        // If adding a toast requires a token for this app we always schedule hiding  
        // toast windows to make sure they don't stick around longer then necessary.        // We hide instead of remove such windows as apps aren't prepared to handle        // windows being removed under them.        //        // If the app is older it can add toasts without a token and hence overlay        // other apps. To be maximally compatible with these apps we will hide the        // window after the toast timeout only if the focused window is from another        // UID, otherwise we allow unlimited duration. When a UID looses focus we        // schedule hiding all of its toast windows.        if (type == TYPE_TOAST) {  
            if (!displayContent.canAddToastWindowForUid(callingUid)) {  
                Slog.w(TAG_WM, "Adding more than one toast window for UID at a time.");  
                return WindowManagerGlobal.ADD_DUPLICATE_ADD;  
            }  
            // Make sure this happens before we moved focus as one can make the  
            // toast focusable to force it not being hidden after the timeout.            // Focusable toasts are always timed out to prevent a focused app to            // show a focusable toasts while it has focus which will be kept on            // the screen after the activity goes away.            if (addToastWindowRequiresToken  
                    || (attrs.flags & LayoutParams.FLAG_NOT_FOCUSABLE) == 0  
                    || displayContent.mCurrentFocus == null  
                    || displayContent.mCurrentFocus.mOwnerUid != callingUid) {  
                mH.sendMessageDelayed(  
                        mH.obtainMessage(H.WINDOW_HIDE_TIMEOUT, win),  
                        win.mAttrs.hideTimeoutMilliseconds);  
            }  
        }  
  
        // From now on, no exceptions or errors allowed!  
  
        res = WindowManagerGlobal.ADD_OKAY;  
        if (displayContent.mCurrentFocus == null) {  
            displayContent.mWinAddedSinceNullFocus.add(win);  
        }  
  
        if (excludeWindowTypeFromTapOutTask(type)) {  
            displayContent.mTapExcludedWindows.add(win);  
        }  
  
        origId = Binder.clearCallingIdentity();  
  
        win.attach();  
        mWindowMap.put(client.asBinder(), win);  
  
        win.initAppOpsState();  
  
        
  
        win.mToken.addWindow(win);  
        
        ......
    
}

```