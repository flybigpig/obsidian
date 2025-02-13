## WindowToken 类

### WindowToken 定义和关键变量

我们先查看windowtoken类的源码：

```
/**
 * Container of a set of related windows in the window manager. Often this is an AppWindowToken,
 * which is the handle for an Activity that it uses to display windows. For nested windows, there is
 * a WindowToken created for the parent window to manage its children.
 */
class WindowToken extends WindowContainer<WindowState> {
    private static final String TAG = TAG_WITH_CLASS_NAME ? "WindowToken" : TAG_WM;

    // The actual token.
    final IBinder token;//事实上窗口的token

    // The type of window this token is for, as per WindowManager.LayoutParams.
    final int windowType;//窗口类型

    /** {@code true} if this holds the rounded corner overlay */
    final boolean mRoundedCornerOverlay;
......
    // For printing.
    String stringName;
......
    // Should this token's windows be hidden?
    private boolean mHidden;

    // Temporary for finding which tokens no longer have visible windows.
    boolean hasVisible;

    // Set to true when this token is in a pending transaction where it
    // will be shown.
    boolean waitingToShow;
......
    /** The owner has {@link android.Manifest.permission#MANAGE_APP_TOKENS} */
    final boolean mOwnerCanManageAppTokens;
```

可以看到源码中对windowtoken的解释为：  
Container of a set of related windows in the window manager(也就是在WM中相关联的窗口集合)

此类的源码不多，关键的几个变量也不多，可以直接看源码中的注解就基本上明白。

### WindowToken 的dump信息

查看WindowToken .dum方法:

```
void dump(PrintWriter pw, String prefix, boolean dumpAll) {
    super.dump(pw, prefix, dumpAll);
    pw.print(prefix); pw.print("windows="); pw.println(mChildren);
    pw.print(prefix); pw.print("windowType="); pw.print(windowType);
            pw.print(" hidden="); pw.print(mHidden);
            pw.print(" hasVisible="); pw.println(hasVisible);
    if (waitingToShow || sendingToBottom) {
        pw.print(prefix); pw.print("waitingToShow="); pw.print(waitingToShow);
                pw.print(" sendingToBottom="); pw.print(sendingToBottom);
    }
}
```

我们打开一个文件管理器主界面，执行[adb](https://so.csdn.net/so/search?q=adb&spm=1001.2101.3001.7020) shell dumpsys，就可以查看到对应的WindowToken的dump信息：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/636564569d8ac0bac74dfd7f13ac37da.png#pic_center)

### WindowToken.WindowToken的创建

日志添加：

```
WindowToken(WindowManagerService service, IBinder _token, int type, boolean persistOnEmpty,
        DisplayContent dc, boolean ownerCanManageAppTokens) {
    this(service, _token, type, persistOnEmpty, dc, ownerCanManageAppTokens,
            false /* roundedCornersOverlay */);
}
```

对应[输出日志](https://so.csdn.net/so/search?q=%E8%BE%93%E5%87%BA%E6%97%A5%E5%BF%97&spm=1001.2101.3001.7020)：

```
WindowManager: at com.android.server.wm.WindowToken.<init>(WindowToken.java:113)
WindowManager: at com.android.server.wm.AppWindowToken.<init>(AppWindowToken.java:373)
WindowManager: at com.android.server.wm.AppWindowToken.<init>(AppWindowToken.java:347)
WindowManager: at com.android.server.wm.ActivityRecord.createAppWindow(ActivityRecord.java:1220)
WindowManager: at com.android.server.wm.ActivityRecord.createAppWindowToken(ActivityRecord.java:1152)
WindowManager: at com.android.server.wm.ActivityStack.startActivityLocked(ActivityStack.java:3562)
WindowManager: at com.android.server.wm.ActivityStarter.startActivityUnchecked(ActivityStarter.java:1928)
WindowManager: at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:1608)
WindowManager: at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:1004)
WindowManager: at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:608)
WindowManager: at com.android.server.wm.ActivityStarter.startActivityMayWait(ActivityStarter.java:1502)
WindowManager: at com.android.server.wm.ActivityStarter.execute(ActivityStarter.java:539)
WindowManager: at com.android.server.wm.ActivityTaskManagerService.startActivityAsUser(ActivityTaskManagerService.java:1258)
WindowManager: at com.android.server.wm.ActivityTaskManagerService.startActivityAsUser(ActivityTaskManagerService.java:1232)
WindowManager: at com.android.server.wm.ActivityTaskManagerService.startActivity(ActivityTaskManagerService.java:1209)
WindowManager: at android.app.IActivityTaskManager$Stub.onTransact(IActivityTaskManager.java:1866)
WindowManager: at android.os.Binder.execTransactInternal(Binder.java:1021)
WindowManager: at android.os.Binder.execTransact(Binder.java:994)
```

借用参考资料中的说法：  
WindowToken将属于同一个应用组件的窗口组织在了一起。所谓的应用组件可以是Activity、InputMethod、Wallpaper以及Dream。在WMS对窗口的管理过程中，用WindowToken指代一个应用组件。

### WMS添加和删除WindowToken

应用组件在创建一个窗口时必须指定一个有效的WindowToken，我们查看wms的添加和删除WindowToken：

WindowManagerService.addWindowToken()

```
public void addWindowToken(IBinder binder, int type, int displayId) {
    if (!checkCallingPermission(MANAGE_APP_TOKENS, "addWindowToken()")) {
        throw new SecurityException("Requires MANAGE_APP_TOKENS permission");
    }

    synchronized (mGlobalLock) {
        final DisplayContent dc = getDisplayContentOrCreate(displayId, null /* token */);
        if (dc == null) {
            Slog.w(TAG_WM, "addWindowToken: Attempted to add token: " + binder
                    + " for non-exiting displayId=" + displayId);
            return;
        }

        WindowToken token = dc.getWindowToken(binder);
        if (token != null) {
            Slog.w(TAG_WM, "addWindowToken: Attempted to add binder token: " + binder
                    + " for already created window token: " + token
                    + " displayId=" + displayId);
            return;
        }
        if (type == TYPE_WALLPAPER) {
            new WallpaperWindowToken(this, binder, true, dc,
                    true /* ownerCanManageAppTokens */);//壁纸的windowtoken
        } else {
            new WindowToken(this, binder, type, true, dc, true /* ownerCanManageAppTokens */);//windowtoken对象
        }
    }
}
```

WindowManagerService.removeWindowToken()

```
public void removeWindowToken(IBinder binder, int displayId) {
    if (!checkCallingPermission(MANAGE_APP_TOKENS, "removeWindowToken()")) {
        throw new SecurityException("Requires MANAGE_APP_TOKENS permission");
    }

    final long origId = Binder.clearCallingIdentity();
    try {
        synchronized (mGlobalLock) {
            final DisplayContent dc = mRoot.getDisplayContent(displayId);
            if (dc == null) {
                Slog.w(TAG_WM, "removeWindowToken: Attempted to remove token: " + binder
                        + " for non-exiting displayId=" + displayId);
                return;
            }

            final WindowToken token = dc.removeWindowToken(binder);
            if (token == null) {
                Slog.w(TAG_WM,
                        "removeWindowToken: Attempted to remove non-existing token: " + binder);
                return;
            }

            dc.getInputMonitor().updateInputWindowsLw(true /*force*/);
        }
    } finally {
        Binder.restoreCallingIdentity(origId);
    }
}
```

## AppWindowToken

### AppWindowToken的定义

先看AppWindowToken的源码：

```
/**
 * Version of WindowToken that is specifically for a particular application (or
 * really activity) that is displaying windows.
 */
class AppWindowToken extends WindowToken implements WindowManagerService.AppFreezeListener,
        ConfigurationContainerListener {
```

AppWindowToken字面意义是表示特定的应用(或真实的activity)类型的WindowToken，其是WindowToken的子类。

### AppWindowToken的关键变量

```
// Non-null only for application tokens.
final IApplicationToken appToken;//token对象
final ComponentName mActivityComponent;//类名
WindowState startingWindow;//WindowState 对象
// TODO: Remove after unification
ActivityRecord mActivityRecord;//ActivityRecord 对象
/**
 * This gets used during some open/close transitions as well as during a change transition
 * where it represents the starting-state snapshot.
 */
private AppWindowThumbnail mThumbnail;
```

### AppWindowToken的dump信息

查看AppWindowToken.dump方法:  
其参数对应adb [shell](https://so.csdn.net/so/search?q=shell&spm=1001.2101.3001.7020) dumpsys中：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9e66fec8f5df672bcf1c70d174c4261f.png)

### AppWindowToken.AppWindowToken的创建

日志添加：

```
AppWindowToken(WindowManagerService service, IApplicationToken token,
        ComponentName activityComponent, boolean voiceInteraction, DisplayContent dc,
        long inputDispatchingTimeoutNanos, boolean fullscreen, boolean showForAllUsers,
        int targetSdk, int orientation, int rotationAnimationHint,
        boolean launchTaskBehind, boolean alwaysFocusable,
        ActivityRecord activityRecord) {
    this(service, token, activityComponent, voiceInteraction, dc, fullscreen);
```

对应输出日志：

```
WindowManager: at com.android.server.wm.AppWindowToken.<init>(AppWindowToken.java:348)
WindowManager: at com.android.server.wm.ActivityRecord.createAppWindow(ActivityRecord.java:1220)
WindowManager: at com.android.server.wm.ActivityRecord.createAppWindowToken(ActivityRecord.java:1152)
WindowManager: at com.android.server.wm.ActivityStack.startActivityLocked(ActivityStack.java:3562)
WindowManager: at com.android.server.wm.ActivityStarter.startActivityUnchecked(ActivityStarter.java:1928)
WindowManager: at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:1608)
WindowManager: at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:1004)
WindowManager: at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:608)
WindowManager: at com.android.server.wm.ActivityStarter.startActivityMayWait(ActivityStarter.java:1502)
WindowManager: at com.android.server.wm.ActivityStarter.execute(ActivityStarter.java:539)
WindowManager: at com.android.server.wm.ActivityTaskManagerService.startActivityAsUser(ActivityTaskManagerService.java:1258)
WindowManager: at com.android.server.wm.ActivityTaskManagerService.startActivityAsUser(ActivityTaskManagerService.java:1232)
WindowManager: at com.android.server.wm.ActivityTaskManagerService.startActivity(ActivityTaskManagerService.java:1209)
WindowManager: at android.app.IActivityTaskManager$Stub.onTransact(IActivityTaskManager.java:1866)
WindowManager: at android.os.Binder.execTransactInternal(Binder.java:1021)
WindowManager: at android.os.Binder.execTransact(Binder.java:994)
```

其对应逻辑：  
ActivityRecord.createAppWindowToken

```
mAppWindowToken = createAppWindow(mAtmService.mWindowManager, appToken,
        task.voiceSession != null, container.getDisplayContent(),
        ActivityTaskManagerService.getInputDispatchingTimeoutLocked(this)
                * 1000000L, fullscreen,
        (info.flags & FLAG_SHOW_FOR_ALL_USERS) != 0, appInfo.targetSdkVersion,
        info.screenOrientation, mRotationAnimationHint,
        mLaunchTaskBehind, isAlwaysFocusable());
```

ActivityRecord.createAppWindow

```
AppWindowToken createAppWindow(WindowManagerService service, IApplicationToken token,
        boolean voiceInteraction, DisplayContent dc, long inputDispatchingTimeoutNanos,
        boolean fullscreen, boolean showForAllUsers, int targetSdk, int orientation,
        int rotationAnimationHint, boolean launchTaskBehind,
        boolean alwaysFocusable) {
    return new AppWindowToken(service, token, mActivityComponent, voiceInteraction, dc,
            inputDispatchingTimeoutNanos, fullscreen, showForAllUsers, targetSdk, orientation,
            rotationAnimationHint, launchTaskBehind, alwaysFocusable,
            this);
}
```

## Task

### Task定义

```
class Task extends WindowContainer<AppWindowToken> implements ConfigurationContainerListener{
```

通过类的源码定义，我们可以知道，Task 就是AppWindowToken的集合容器，包含了许多AppWindowToken对象。

### Task的关键变量

```
// TODO: Track parent marks like this in WindowContainer.
TaskStack mStack;
final int mTaskId;
 // TODO: remove after unification
 TaskRecord mTaskRecord;
```

其构造方法：

```
Task(int taskId, TaskStack stack, int userId, WindowManagerService service, int resizeMode,
        boolean supportsPictureInPicture, TaskDescription taskDescription,
        TaskRecord taskRecord) {
    super(service);
    mTaskId = taskId;
    mStack = stack;
    mUserId = userId;
    mResizeMode = resizeMode;
    mSupportsPictureInPicture = supportsPictureInPicture;
    mTaskRecord = taskRecord;
    if (mTaskRecord != null) {
        // This can be null when we call createTaskInStack in WindowTestUtils. Remove this after
        // unification.
        mTaskRecord.registerConfigurationChangeListener(this);
    }
    setBounds(getRequestedOverrideBounds());
    mTaskDescription = taskDescription;

    // Tasks have no set orientation value (including SCREEN_ORIENTATION_UNSPECIFIED).
    setOrientation(SCREEN_ORIENTATION_UNSET);
}
```

### Task的dump信息

查看Task.dump方法:

```
 public void dump(PrintWriter pw, String prefix, boolean dumpAll) {
     super.dump(pw, prefix, dumpAll);
     final String doublePrefix = prefix + "  ";

     pw.println(prefix + "taskId=" + mTaskId);
     pw.println(doublePrefix + "mBounds=" + getBounds().toShortString());
     pw.println(doublePrefix + "mdr=" + mDeferRemoval);
     pw.println(doublePrefix + "appTokens=" + mChildren);
     pw.println(doublePrefix + "mDisplayedBounds=" + mOverrideDisplayedBounds.toShortString());

     final String triplePrefix = doublePrefix + "  ";
     final String quadruplePrefix = triplePrefix + "  ";

     for (int i = mChildren.size() - 1; i >= 0; i--) {
         final AppWindowToken wtoken = mChildren.get(i);
         pw.println(triplePrefix + "Activity #" + i + " " + wtoken);
         wtoken.dump(pw, quadruplePrefix, dumpAll);
     }
 }
```

其参数对应adb shell dumpsys中：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/120131539eea8bca54739872517d5fcb.png#pic_center)

### Task.Task的创建

日志添加：

```
Task(int taskId, TaskStack stack, int userId, WindowManagerService service, int resizeMode,
     boolean supportsPictureInPicture, TaskDescription taskDescription,
     TaskRecord taskRecord) {
 super(service);
```

对应输出日志：

```
WindowManager:  Task
WindowManager: at com.android.server.wm.Task.<init>(Task.java:123)
WindowManager: at com.android.server.wm.TaskRecord.createTask(TaskRecord.java:566)
WindowManager: at com.android.server.wm.ActivityStack.createTaskRecord(ActivityStack.java:6067)
WindowManager: at com.android.server.wm.ActivityStarter.setTaskFromReuseOrCreateNewTask(ActivityStarter.java:2546)
WindowManager: at com.android.server.wm.ActivityStarter.startActivityUnchecked(ActivityStarter.java:1898)
WindowManager: at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:1608)
WindowManager: at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:1004)
WindowManager: at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:608)
WindowManager: at com.android.server.wm.ActivityStarter.startActivityMayWait(ActivityStarter.java:1502)
WindowManager: at com.android.server.wm.ActivityStarter.execute(ActivityStarter.java:539)
WindowManager: at com.android.server.wm.ActivityTaskManagerService.startActivityAsUser(ActivityTaskManagerService.java:1258)
WindowManager: at com.android.server.wm.ActivityTaskManagerService.startActivityAsUser(ActivityTaskManagerService.java:1232)
WindowManager: at com.android.server.wm.ActivityTaskManagerService.startActivity(ActivityTaskManagerService.java:1209)
WindowManager: at android.app.IActivityTaskManager$Stub.onTransact(IActivityTaskManager.java:1866)
WindowManager: at android.os.Binder.execTransactInternal(Binder.java:1021)
WindowManager: at android.os.Binder.execTransact(Binder.java:994)
```

## TaskStack

### TaskStack的定义

```
public class TaskStack extends WindowContainer<Task> implements
        BoundsAnimationTarget, ConfigurationContainerListener {
```

### TaskStack的关键变量

```
/** Unique identifier */
final int mStackId;
// TODO: remove after unification.
ActivityStack mActivityStack;
```

### TaskStack的dump信息

查看Task.dump方法，其参数对应adb shell dumpsys中：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/8d5ec7b3cfefd3b5400cd7cbde8bd6c3.png#pic_center)

### TaskStack.TaskStack的创建

日志添加：

```
TaskStack(WindowManagerService service, int stackId, ActivityStack activityStack) {
    super(service);
```

对应输出日志：

```
TaskStack:  TaskStack
TaskStack: at com.android.server.wm.TaskStack.<init>(TaskStack.java:173)
TaskStack: at com.android.server.wm.ActivityStack.createTaskStack(ActivityStack.java:566)
TaskStack: at com.android.server.wm.ActivityStack.<init>(ActivityStack.java:550)
TaskStack: at com.android.server.wm.ActivityDisplay.createStackUnchecked(ActivityDisplay.java:552)
TaskStack: at com.android.server.wm.ActivityDisplay.createStack(ActivityDisplay.java:542)
TaskStack: at com.android.server.wm.ActivityDisplay.getOrCreateStack(ActivityDisplay.java:449)
TaskStack: at com.android.server.wm.ActivityDisplay.getOrCreateStack(ActivityDisplay.java:468)
TaskStack: at com.android.server.wm.RootActivityContainer.getLaunchStack(RootActivityContainer.java:1767)
TaskStack: at com.android.server.wm.ActivityStarter.getLaunchStack(ActivityStarter.java:2930)
TaskStack: at com.android.server.wm.ActivityStarter.computeStackFocus(ActivityStarter.java:2833)
TaskStack: at com.android.server.wm.ActivityStarter.setTaskFromReuseOrCreateNewTask(ActivityStarter.java:2539)
TaskStack: at com.android.server.wm.ActivityStarter.startActivityUnchecked(ActivityStarter.java:1898)
TaskStack: at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:1608)
TaskStack: at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:1004)
TaskStack: at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:608)
TaskStack: at com.android.server.wm.ActivityStarter.startActivityMayWait(ActivityStarter.java:1502)
TaskStack: at com.android.server.wm.ActivityStarter.execute(ActivityStarter.java:539)
TaskStack: at com.android.server.wm.ActivityTaskManagerService.startActivityAsUser(ActivityTaskManagerService.java:1258)
TaskStack: at com.android.server.wm.ActivityTaskManagerService.startActivityAsUser(ActivityTaskManagerService.java:1232)
TaskStack: at com.android.server.wm.ActivityTaskManagerService.startActivity(ActivityTaskManagerService.java:1209)
TaskStack: at android.app.IActivityTaskManager$Stub.onTransact(IActivityTaskManager.java:1866)
TaskStack: at android.os.Binder.execTransactInternal(Binder.java:1021)
TaskStack: at android.os.Binder.execTransact(Binder.java:994)
```

## DisplayContent

### DisplayContent的定义

```
/**
 * Utility class for keeping track of the WindowStates and other pertinent contents of a
 * particular Display.
 */
class DisplayContent extends WindowContainer<DisplayContent.DisplayChildWindowContainer>
        implements WindowManagerPolicy.DisplayContentInfo {
```

从字面意思理解，DisplayContent 就是一个管理WindowStates 和其他的显示的管理帮助类。

### DisplayContent的关键变量

```
// TODO: Remove once unification is complete.
ActivityDisplay mAcitvityDisplay;
private final DisplayInfo mDisplayInfo = new DisplayInfo();
private final Display mDisplay;
private final DisplayMetrics mDisplayMetrics = new DisplayMetrics();
private final DisplayPolicy mDisplayPolicy;
private DisplayRotation mDisplayRotation;
DisplayFrames mDisplayFrames;
/**
* The foreground app of this display. Windows below this app cannot be the focused window. If
* the user taps on the area outside of the task of the focused app, we will notify AM about the
* new task the user wants to interact with.
*/
AppWindowToken mFocusedApp = null;
```

### DisplayContent的dump信息

查看DisplayContent.dump方法，其打印的信息非常的多：

```
public void dump(PrintWriter pw, String prefix, boolean dumpAll) {
    super.dump(pw, prefix, dumpAll);
    pw.print(prefix); pw.print("Display: mDisplayId="); pw.println(mDisplayId);
    final String subPrefix = "  " + prefix;
    pw.print(subPrefix); pw.print("init="); pw.print(mInitialDisplayWidth); pw.print("x");
        pw.print(mInitialDisplayHeight); pw.print(" "); pw.print(mInitialDisplayDensity);
        pw.print("dpi");
        if (mInitialDisplayWidth != mBaseDisplayWidth
                || mInitialDisplayHeight != mBaseDisplayHeight
                || mInitialDisplayDensity != mBaseDisplayDensity) {
            pw.print(" base=");
            pw.print(mBaseDisplayWidth); pw.print("x"); pw.print(mBaseDisplayHeight);
            pw.print(" "); pw.print(mBaseDisplayDensity); pw.print("dpi");
        }
        if (mDisplayScalingDisabled) {
            pw.println(" noscale");
        }
        pw.print(" cur=");
        pw.print(mDisplayInfo.logicalWidth);
        pw.print("x"); pw.print(mDisplayInfo.logicalHeight);
        pw.print(" app=");
        pw.print(mDisplayInfo.appWidth);
        pw.print("x"); pw.print(mDisplayInfo.appHeight);
        pw.print(" rng="); pw.print(mDisplayInfo.smallestNominalAppWidth);
        pw.print("x"); pw.print(mDisplayInfo.smallestNominalAppHeight);
        pw.print("-"); pw.print(mDisplayInfo.largestNominalAppWidth);
        pw.print("x"); pw.println(mDisplayInfo.largestNominalAppHeight);
        pw.print(subPrefix + "deferred=" + mDeferredRemoval
                + " mLayoutNeeded=" + mLayoutNeeded);
        pw.println(" mTouchExcludeRegion=" + mTouchExcludeRegion);

    pw.println();
    pw.print(prefix); pw.print("mLayoutSeq="); pw.println(mLayoutSeq);
    pw.print(prefix);
    pw.print("mDeferredRotationPauseCount="); pw.println(mDeferredRotationPauseCount);

    pw.print("  mCurrentFocus="); pw.println(mCurrentFocus);
    if (mLastFocus != mCurrentFocus) {
        pw.print("  mLastFocus="); pw.println(mLastFocus);
    }
    if (mLosingFocus.size() > 0) {
        pw.println();
        pw.println("  Windows losing focus:");
        for (int i = mLosingFocus.size() - 1; i >= 0; i--) {
            final WindowState w = mLosingFocus.get(i);
            pw.print("  Losing #"); pw.print(i); pw.print(' ');
            pw.print(w);
            if (dumpAll) {
                pw.println(":");
                w.dump(pw, "    ", true);
            } else {
                pw.println();
            }
        }
    }
    pw.print("  mFocusedApp="); pw.println(mFocusedApp);
    if (mLastStatusBarVisibility != 0) {
        pw.print("  mLastStatusBarVisibility=0x");
        pw.println(Integer.toHexString(mLastStatusBarVisibility));
    }

    pw.println();
    mWallpaperController.dump(pw, "  ");

    pw.println();
    pw.print("mSystemGestureExclusion=");
    if (mSystemGestureExclusionListeners.getRegisteredCallbackCount() > 0) {
        pw.println(mSystemGestureExclusion);
    } else {
        pw.println("<no lstnrs>");
    }

    pw.println();
    pw.println(prefix + "Application tokens in top down Z order:");
    for (int stackNdx = mTaskStackContainers.getChildCount() - 1; stackNdx >= 0; --stackNdx) {
        final TaskStack stack = mTaskStackContainers.getChildAt(stackNdx);
        stack.dump(pw, prefix + "  ", dumpAll);
    }

    pw.println();
    if (!mExitingTokens.isEmpty()) {
        pw.println();
        pw.println("  Exiting tokens:");
        for (int i = mExitingTokens.size() - 1; i >= 0; i--) {
            final WindowToken token = mExitingTokens.get(i);
            pw.print("  Exiting #"); pw.print(i);
            pw.print(' '); pw.print(token);
            pw.println(':');
            token.dump(pw, "    ", dumpAll);
        }
    }

    pw.println();

    // Dump stack references
    final TaskStack homeStack = getHomeStack();
    if (homeStack != null) {
        pw.println(prefix + "homeStack=" + homeStack.getName());
    }
    final TaskStack pinnedStack = getPinnedStack();
    if (pinnedStack != null) {
        pw.println(prefix + "pinnedStack=" + pinnedStack.getName());
    }
    final TaskStack splitScreenPrimaryStack = getSplitScreenPrimaryStack();
    if (splitScreenPrimaryStack != null) {
        pw.println(prefix + "splitScreenPrimaryStack=" + splitScreenPrimaryStack.getName());
    }

    pw.println();
    mDividerControllerLocked.dump(prefix, pw);
    pw.println();
    mPinnedStackControllerLocked.dump(prefix, pw);

    pw.println();
    mDisplayFrames.dump(prefix, pw);
    pw.println();
    mDisplayPolicy.dump(prefix, pw);
    pw.println();
    mDisplayRotation.dump(prefix, pw);
    pw.println();
    mInputMonitor.dump(pw, "  ");
    pw.println();
    mInsetsStateController.dump(prefix, pw);
}
```

其参数对应adb shell dumpsys中：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/556dbf5edffef09f08b680909d10c529.png#pic_center)  
我这列举的不多，事实上dumpsys window displays命令显示的都是DisplayContent的dump信息。

### DisplayContent.DisplayContent的创建

日志添加：

```
/**
 * Create new {@link DisplayContent} instance, add itself to the root window container and
 * initialize direct children.
 * @param display May not be null.
 * @param service You know.
 * @param activityDisplay The ActivityDisplay for the display container.
 */
DisplayContent(Display display, WindowManagerService service,
        ActivityDisplay activityDisplay) {
    super(service);
```

对应输出日志：

```
WindowManager: DisplayContent
WindowManager: at com.android.server.wm.DisplayContent.<init>(DisplayContent.java:916)
WindowManager: at com.android.server.wm.RootWindowContainer.createDisplayContent(RootWindowContainer.java:238)
WindowManager: at com.android.server.wm.ActivityDisplay.createDisplayContent(ActivityDisplay.java:215)
WindowManager: at com.android.server.wm.ActivityDisplay.<init>(ActivityDisplay.java:205)
WindowManager: at com.android.server.wm.RootActivityContainer.setWindowManager(RootActivityContainer.java:249)
WindowManager: at com.android.server.wm.ActivityTaskManagerService.setWindowManager(ActivityTaskManagerService.java:1052)
WindowManager: at com.android.server.am.ActivityManagerService.setWindowManager(ActivityManagerService.java:2165)
WindowManager: at com.android.server.SystemServer.startOtherServices(SystemServer.java:1071)
WindowManager: at com.android.server.SystemServer.run(SystemServer.java:530)
WindowManager: at com.android.server.SystemServer.main(SystemServer.java:367)
WindowManager: at java.lang.reflect.Method.invoke(Native Method)
WindowManager: at com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run(RuntimeInit.java:492)
WindowManager: at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:913)
```

### DisplayContent.DisplayContent的创建流程

简单的查看其创建流程：  
SystemServer.startOtherServices

```
traceBeginAndSlog("SetWindowManagerService");
mActivityManagerService.setWindowManager(wm);
traceEnd();
```

ActivityManagerService.setWindowManager

```
public void setWindowManager(WindowManagerService wm) {
   synchronized (this) {
       mWindowManager = wm;
       mActivityTaskManager.setWindowManager(wm);
   }
}
```

ActivityTaskManagerService.setWindowManager

```
public void setWindowManager(WindowManagerService wm) {
    synchronized (mGlobalLock) {
        mWindowManager = wm;
        mLockTaskController.setWindowManager(wm);
        mStackSupervisor.setWindowManager(wm);
        mRootActivityContainer.setWindowManager(wm);
    }
}
```

RootActivityContainer.setWindowManager

```
void setWindowManager(WindowManagerService wm) {
    mWindowManager = wm;
    setWindowContainer(mWindowManager.mRoot);
    mDisplayManager = mService.mContext.getSystemService(DisplayManager.class);
    mDisplayManager.registerDisplayListener(this, mService.mUiHandler);
    mDisplayManagerInternal = LocalServices.getService(DisplayManagerInternal.class);

    final Display[] displays = mDisplayManager.getDisplays();
    for (int displayNdx = 0; displayNdx < displays.length; ++displayNdx) {
        final Display display = displays[displayNdx];
        final ActivityDisplay activityDisplay = new ActivityDisplay(this, display);
        if (activityDisplay.mDisplayId == DEFAULT_DISPLAY) {
            mDefaultDisplay = activityDisplay;
        }
        addChild(activityDisplay, ActivityDisplay.POSITION_TOP);
    }
    .......
```

ActivityDisplay.

```
 ActivityDisplay(RootActivityContainer root, Display display) {
     mRootActivityContainer = root;
     mService = root.mService;
     mDisplayId = display.getDisplayId();
     mDisplay = display;
     mDisplayContent = createDisplayContent();
```

ActivityDisplay.createDisplayContent

```
protected DisplayContent createDisplayContent() {
    return mService.mWindowManager.mRoot.createDisplayContent(mDisplay, this);
}
```

RootWindowContainer.createDisplayContent

```
DisplayContent createDisplayContent(final Display display, ActivityDisplay activityDisplay) {
    final int displayId = display.getDisplayId();

    // In select scenarios, it is possible that a DisplayContent will be created on demand
    // rather than waiting for the controller. In this case, associate the controller and return
    // the existing display.
    final DisplayContent existing = getDisplayContent(displayId);

    if (existing != null) {
        existing.mAcitvityDisplay = activityDisplay;
        existing.initializeDisplayOverrideConfiguration();
        return existing;
    }

    final DisplayContent dc = new DisplayContent(display, mWmService, activityDisplay);
    ......
```

## 总结

参考android开发浅谈之ActivityDisplay/ActivityStack/TaskRecord/ActivityRecord,我们可以总结出AppWindowToken,Task,TaskStack和DisplayContent的关系图  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5ac452f9ed78fbe169e69ba400d97b8c.png)  
也就是activity和window的对应关系：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/431b7da01eb01a11040f300a7c054340.png)  
从整体界面的关系来看：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/1dc58276d1b39f950e436e18ccbff811.png)

## 参考资料

[(1)4.2.1理解WindowToken](https://www.kancloud.cn/alex_wsc/android-deep3/416391)  
[https://www.kancloud.cn/alex\_wsc/android-deep3/416391](https://www.kancloud.cn/alex_wsc/android-deep3/416391)  
[(2)android开发浅谈之ActivityDisplay/ActivityStack/TaskRecord/ActivityRecord](https://blog.csdn.net/hfreeman2008/article/details/113309272)  
[https://blog.csdn.net/hfreeman2008/article/details/113309272](https://blog.csdn.net/hfreeman2008/article/details/113309272)  
(3)Android AMS(六) Activity与WMS的连接过程之AppWindowToken  
https://blog.csdn.net/liu362732346/article/details/85336050