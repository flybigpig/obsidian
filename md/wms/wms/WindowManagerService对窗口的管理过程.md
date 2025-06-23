
## 前言

上一篇我们具体分析了WindowManager的addView方法添加窗口的过程，我们知道WindowManager最终会调用到WindowManagerService的addWindow方法。本篇文章我们将在此基础上，具体来分析WindowManagerService的addWindow方法添加窗口的过程。这个方法的代码逻辑非常长，提前做好心理准备。

## 一、检测窗口权限和窗口类型

1、WindowManagerService的addWindow方法如下所示。

> frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java

```java
public class WindowManagerService extends IWindowManager.Stub
        implements Watchdog.Monitor, WindowManagerPolicy.WindowManagerFuncs {
        
    //窗口管理策略的接口类，用来定义一个窗口策略所要遵循的通用规范，具体实现类为PhoneWindowManager。
    final WindowManagerPolicy mPolicy;
    
    public int addWindow(Session session, IWindow client, LayoutParams attrs, int viewVisibility,
            int displayId, int requestUserId, InsetsVisibilities requestedVisibilities,
            InputChannel outInputChannel, InsetsState outInsetsState,
            InsetsSourceControl[] outActiveControls) {
        Arrays.fill(outActiveControls, null);
        int[] appOp = new int[1];
        final boolean isRoundedCornerOverlay = (attrs.privateFlags
                & PRIVATE_FLAG_IS_ROUNDED_CORNERS_OVERLAY) != 0;
        //检测窗口权限，没有权限的客户端不能添加窗口
        int res = mPolicy.checkAddPermission(attrs.type, isRoundedCornerOverlay, attrs.packageName,
                appOp);  
        if (res != ADD_OKAY) {
            return res;
        }
		...代码省略...
    }
}
```

addWindow方法首先会调用WindowManagerPolicy的checkAddPermission方法进行权限判断，WindowManagerPolicy是一个接口，具体实现者为PhoneWindowManager。

2、PhoneWindowManager的checkAddPermission方法如下所示。

> frameworks/base/services/core/java/com/android/server/policy/PhoneWindowManager.java

```java
public class PhoneWindowManager implements WindowManagerPolicy {

    @Override
    public int checkAddPermission(int type, boolean isRoundedCornerOverlay, String packageName,
            int[] outAppOp) {
        if (isRoundedCornerOverlay && mContext.checkCallingOrSelfPermission(INTERNAL_SYSTEM_WINDOW)
                != PERMISSION_GRANTED) {
            return ADD_PERMISSION_DENIED;
        }

        outAppOp[0] = AppOpsManager.OP_NONE;
		//判断是不是一个合法的窗口类型，必须是应用窗口、子窗口、系统窗口三种类型中的一种。
        if (!((type >= FIRST_APPLICATION_WINDOW && type <= LAST_APPLICATION_WINDOW)
                || (type >= FIRST_SUB_WINDOW && type <= LAST_SUB_WINDOW)
                || (type >= FIRST_SYSTEM_WINDOW && type <= LAST_SYSTEM_WINDOW))) {
            return WindowManagerGlobal.ADD_INVALID_TYPE;
        }
		//如果不是系统窗口类型，直接返回
        if (type < FIRST_SYSTEM_WINDOW || type > LAST_SYSTEM_WINDOW) {
            // Window manager will make sure these are okay.
            return ADD_OKAY;
        }
		//如果窗口不是系统弹窗类型
        if (!isSystemAlertWindowType(type)) {//此方法位于WindowManager中
            switch (type) {
                case TYPE_TOAST:
                    // Only apps that target older than O SDK can add window without a token, after
                    // that we require a token so apps cannot add toasts directly as the token is
                    // added by the notification system.
                    // Window manager does the checking for this.
                    outAppOp[0] = OP_TOAST_WINDOW;
                    return ADD_OKAY;
                case TYPE_INPUT_METHOD:
                case TYPE_WALLPAPER:
                case TYPE_PRESENTATION:
                case TYPE_PRIVATE_PRESENTATION:
                case TYPE_VOICE_INTERACTION:
                case TYPE_ACCESSIBILITY_OVERLAY:
                case TYPE_QS_DIALOG:
                case TYPE_NAVIGATION_BAR_PANEL:
                    // The window manager will check these.
                    return ADD_OKAY;
            }

            return (mContext.checkCallingOrSelfPermission(INTERNAL_SYSTEM_WINDOW)
                    == PERMISSION_GRANTED) ? ADD_OKAY : ADD_PERMISSION_DENIED;
        }

        // Things get a little more interesting for alert windows...
        outAppOp[0] = OP_SYSTEM_ALERT_WINDOW;

        final int callingUid = Binder.getCallingUid();
        // system processes will be automatically granted privilege to draw
        if (UserHandle.getAppId(callingUid) == Process.SYSTEM_UID) {
            return ADD_OKAY;
        }
		//获取包名对应的应用信息
        ApplicationInfo appInfo;
        try {
            appInfo = mPackageManager.getApplicationInfoAsUser(
                            packageName,
                            0 /* flags */,
                            UserHandle.getUserId(callingUid));
        } catch (PackageManager.NameNotFoundException e) {
            appInfo = null;
        }

        if (appInfo == null || (type != TYPE_APPLICATION_OVERLAY && appInfo.targetSdkVersion >= O)) {
            /**
             * Apps targeting >= {@link Build.VERSION_CODES#O} are required to hold
             * {@link android.Manifest.permission#INTERNAL_SYSTEM_WINDOW} (system signature apps)
             * permission to add alert windows that aren't
             * {@link android.view.WindowManager.LayoutParams#TYPE_APPLICATION_OVERLAY}.
             */
            return (mContext.checkCallingOrSelfPermission(INTERNAL_SYSTEM_WINDOW)
                    == PERMISSION_GRANTED) ? ADD_OKAY : ADD_PERMISSION_DENIED;
        }

        if (mContext.checkCallingOrSelfPermission(SYSTEM_APPLICATION_OVERLAY)
                == PERMISSION_GRANTED) {
            return ADD_OKAY;
        }

        // check if user has enabled this operation. SecurityException will be thrown if this app
        // has not been allowed by the user. The reason to use "noteOp" (instead of checkOp) is to
        // make sure the usage is logged.
        final int mode = mAppOpsManager.noteOpNoThrow(outAppOp[0], callingUid, packageName,
                null /* featureId */, "check-add");
        switch (mode) {
            case AppOpsManager.MODE_ALLOWED:
            case AppOpsManager.MODE_IGNORED:
                // although we return ADD_OKAY for MODE_IGNORED, the added window will
                // actually be hidden in WindowManagerService
                return ADD_OKAY;
            case AppOpsManager.MODE_ERRORED:
                // Don't crash legacy apps
                if (appInfo.targetSdkVersion < M) {
                    return ADD_OKAY;
                }
                return ADD_PERMISSION_DENIED;
            default:
                // in the default mode, we will make a decision here based on
                // checkCallingPermission()
                return (mContext.checkCallingOrSelfPermission(SYSTEM_ALERT_WINDOW)
                        == PERMISSION_GRANTED) ? ADD_OKAY : ADD_PERMISSION_DENIED;
        }
    }
        
}
```

> frameworks/base/core/java/android/view/WindowManager.java

```java
public interface WindowManager extends ViewManager {
    public static class LayoutParams extends ViewGroup.LayoutParams implements Parcelable {
       public static boolean isSystemAlertWindowType(@WindowType int type) {
            switch (type) {
                case TYPE_PHONE:
                case TYPE_PRIORITY_PHONE:
                case TYPE_SYSTEM_ALERT:
                case TYPE_SYSTEM_ERROR:
                case TYPE_SYSTEM_OVERLAY:
                case TYPE_APPLICATION_OVERLAY:
                    return true;
            }
            return false;
        }
      }
  }
```

checkAddPermission会做一些窗口类型以及安全相关的权限判断，并将结果返回赋值给res。如果res不等于WindowManagerGlobal.ADD\_OKAY的话WindowManagerService的addWindow就会直接返回，而结合上一篇文章我们知道，该返回结果会被ViewRootImpl收到。

3、ViewRootImpl的setView方法收到返回值后的相关操作如下所示。

```java
public final class ViewRootImpl implements ViewParent,
        View.AttachInfo.Callbacks, ThreadedRenderer.DrawCallbacks {

    /**
     * We have one child
     */
    public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
        setView(view, attrs, panelParentView, UserHandle.myUserId());
    }

    /**
     * We have one child
     */
    public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView,
            int userId) {
        synchronized (this) {
              ...代码省略...
                 res = mWindowSession.addToDisplayAsUser(mWindow, mWindowAttributes,
                         getHostVisibility(), mDisplay.getDisplayId(), userId,
                         mInsetsController.getRequestedVisibilities(), inputChannel, mTempInsets,
                         mTempControls);
                 if (mTranslator != null) {
                     mTranslator.translateInsetsStateInScreenToAppWindow(mTempInsets);
                     mTranslator.translateSourceControlsInScreenToAppWindow(mTempControls);
                 }
              ...代码省略...
              //会根据WindowManagerService返回的判断窗口是否添加成功，以便抛出各种异常
             if (res < WindowManagerGlobal.ADD_OKAY) {
                  mAttachInfo.mRootView = null;
                  mAdded = false;
                  mFallbackEventHandler.setView(null);
                  unscheduleTraversals();
                  setAccessibilityFocus(null, null);
                  switch (res) {
                      case WindowManagerGlobal.ADD_BAD_APP_TOKEN:
                      case WindowManagerGlobal.ADD_BAD_SUBWINDOW_TOKEN:
                          throw new WindowManager.BadTokenException(
                                  "Unable to add window -- token " + attrs.token
                                  + " is not valid; is your activity running?");
                      case WindowManagerGlobal.ADD_NOT_APP_TOKEN:
                          throw new WindowManager.BadTokenException(
                                  "Unable to add window -- token " + attrs.token
                                  + " is not for an application");
                      case WindowManagerGlobal.ADD_APP_EXITING:
                          throw new WindowManager.BadTokenException(
                                  "Unable to add window -- app for token " + attrs.token
                                  + " is exiting");
                      case WindowManagerGlobal.ADD_DUPLICATE_ADD:
                          throw new WindowManager.BadTokenException(
                                  "Unable to add window -- window " + mWindow
                                  + " has already been added");
                      case WindowManagerGlobal.ADD_STARTING_NOT_NEEDED:
                          // Silently ignore -- we would have just removed it
                          // right away, anyway.
                          return;
                      case WindowManagerGlobal.ADD_MULTIPLE_SINGLETON:
                          throw new WindowManager.BadTokenException("Unable to add window "
                                  + mWindow + " -- another window of type "
                                  + mWindowAttributes.type + " already exists");
                      case WindowManagerGlobal.ADD_PERMISSION_DENIED:
                          throw new WindowManager.BadTokenException("Unable to add window "
                                  + mWindow + " -- permission denied for window type "
                                  + mWindowAttributes.type);
                      case WindowManagerGlobal.ADD_INVALID_DISPLAY:
                          throw new WindowManager.InvalidDisplayException("Unable to add window "
                                  + mWindow + " -- the specified display can not be found");
                      case WindowManagerGlobal.ADD_INVALID_TYPE:
                          throw new WindowManager.InvalidDisplayException("Unable to add window "
                                  + mWindow + " -- the specified window type "
                                  + mWindowAttributes.type + " is not valid");
                  }
                  throw new RuntimeException(
                          "Unable to add window -- unknown error code " + res);
            }
    }
}

```

ViewRootImpl会根据WindowManagerService的返回值来判断窗口是否添加成功，以便决定是否抛出各种异常信息。

4、重新回到WindowManagerService的addWindow方法中，继续往下看。

```java
public class WindowManagerService extends IWindowManager.Stub
        implements Watchdog.Monitor, WindowManagerPolicy.WindowManagerFuncs {

    public int addWindow(Session session, IWindow client, int seq,
            LayoutParams attrs, int viewVisibility, int displayId, Rect outFrame,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel) {
	        Arrays.fill(outActiveControls, null);
	        int[] appOp = new int[1];
	        final boolean isRoundedCornerOverlay = (attrs.privateFlags
	                & PRIVATE_FLAG_IS_ROUNDED_CORNERS_OVERLAY) != 0;
	        //检测窗口权限，没有权限的客户端不能添加窗口        
	        int res = mPolicy.checkAddPermission(attrs.type, isRoundedCornerOverlay, attrs.packageName,
	                appOp);
	        if (res != ADD_OKAY) {
	            return res;
	        }
			WindowState parentWindow = null;//父窗口
	        final int callingUid = Binder.getCallingUid();//调用者id
	        final int callingPid = Binder.getCallingPid();//调用者进程id
	        final long origId = Binder.clearCallingIdentity();
	        final int type = attrs.type;// 窗口类型
	
	        synchronized (mGlobalLock) {
	            if (!mDisplayReady) {
	                throw new IllegalStateException("Display has not been initialialized");
	            }
	            //根据屏幕设备id获取当前窗口对应的屏幕设备信息对象并判断该对象是否为空
	            final DisplayContent displayContent = getDisplayContentOrCreate(displayId, attrs.token);
	            if (displayContent == null) {
	                ProtoLog.w(WM_ERROR, "Attempted to add window to a display that does "
	                        + "not exist: %d. Aborting.", displayId);
	                return WindowManagerGlobal.ADD_INVALID_DISPLAY;
	            }
	            if (!displayContent.hasAccess(session.mUid)) {
	                ProtoLog.w(WM_ERROR,
	                        "Attempted to add window to a display for which the application "
	                                + "does not have access: %d.  Aborting.",
	                        displayContent.getDisplayId());
	                return WindowManagerGlobal.ADD_INVALID_DISPLAY;
	            }
	            //判断当前窗口是否已经存在
	            if (mWindowMap.containsKey(client.asBinder())) {
	                ProtoLog.w(WM_ERROR, "Window %s is already added", client);
	                return WindowManagerGlobal.ADD_DUPLICATE_ADD;
	            }
	            //判断窗口是否是子窗口类型
	            if (type >= FIRST_SUB_WINDOW && type <= LAST_SUB_WINDOW) {
	                parentWindow = windowForClientLocked(null, attrs.token, false);
	                if (parentWindow == null) {//如果自己是子窗口且父类为空，提示异常
	                    ProtoLog.w(WM_ERROR, "Attempted to add window with token that is not a window: "
	                            + "%s.  Aborting.", attrs.token);
	                    return WindowManagerGlobal.ADD_BAD_SUBWINDOW_TOKEN;
	                }
	                if (parentWindow.mAttrs.type >= FIRST_SUB_WINDOW
	                        && parentWindow.mAttrs.type <= LAST_SUB_WINDOW) {//如果父类也是子窗口，提示异常
	                    ProtoLog.w(WM_ERROR, "Attempted to add window with token that is a sub-window: "
	                            + "%s.  Aborting.", attrs.token);
	                    return WindowManagerGlobal.ADD_BAD_SUBWINDOW_TOKEN;
	                }
	            }
	            //当前窗口是私有的而将要添加的屏幕设备不是私有的，提示异常
	            if (type == TYPE_PRIVATE_PRESENTATION && !displayContent.isPrivate()) {
	                ProtoLog.w(WM_ERROR,
	                        "Attempted to add private presentation window to a non-private display.  "
	                                + "Aborting.");
	                return WindowManagerGlobal.ADD_PERMISSION_DENIED;
	            }
	            //当前窗口公开而将要添加的屏幕设备不公开，提示异常
	            if (type == TYPE_PRESENTATION && !displayContent.getDisplay().isPublicPresentation()) {
	                ProtoLog.w(WM_ERROR,
	                        "Attempted to add presentation window to a non-suitable display.  "
	                                + "Aborting.");
	                return WindowManagerGlobal.ADD_INVALID_DISPLAY;
	            }
	           ...代码省略...
            }
}
```

## 二、检测窗口令牌的类型

1、继续往下看WindowManagerService的addWindow方法。

```java
public class WindowManagerService extends IWindowManager.Stub
        implements Watchdog.Monitor, WindowManagerPolicy.WindowManagerFuncs {

    public int addWindow(Session session, IWindow client, int seq,
            LayoutParams attrs, int viewVisibility, int displayId, Rect outFrame,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel) { 
			...代码省略...
            if (token == null) {//Window的令牌为空
                if (!unprivilegedAppCanCreateTokenWith(parentWindow, callingUid, type,
                        rootType, attrs.token, attrs.packageName)) {
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
                if (hasParent) {//存在父类则使用父类的窗口令牌
                    // Use existing parent window token for child windows.
                    token = parentWindow.mToken;
                } else if (mWindowContextListenerController.hasListener(windowContextToken)) {
                    // Respect the window context token if the user provided it.
                    final IBinder binder = attrs.token != null ? attrs.token : windowContextToken;
                    final Bundle options = mWindowContextListenerController
                            .getOptions(windowContextToken);
                    //系统主动为窗口创建令牌
                    token = new WindowToken.Builder(this, binder, type)
                            .setDisplayContent(displayContent)
                            .setOwnerCanManageAppTokens(session.mCanAddInternalSystemWindow)
                            .setRoundedCornerOverlay(isRoundedCornerOverlay)
                            .setFromClientToken(true)
                            .setOptions(options)
                            .build();
                } else {
                    //系统主动为窗口创建令牌
                    final IBinder binder = attrs.token != null ? attrs.token : client.asBinder();
                    token = new WindowToken.Builder(this, binder, type)
                            .setDisplayContent(displayContent)
                            .setOwnerCanManageAppTokens(session.mCanAddInternalSystemWindow)
                            .setRoundedCornerOverlay(isRoundedCornerOverlay)
                            .build();
                }
            }
            //窗口类型属于应用程序窗口
            else if (rootType >= FIRST_APPLICATION_WINDOW
                    && rootType <= LAST_APPLICATION_WINDOW) {
                activity = token.asActivityRecord();
                if (activity == null) {//令牌对应的activity为空
                    ProtoLog.w(WM_ERROR, "Attempted to add window with non-application token "
                            + ".%s Aborting.", token);
                    return WindowManagerGlobal.ADD_NOT_APP_TOKEN;
                } else if (activity.getParent() == null) {//Activity父类为空
                    ProtoLog.w(WM_ERROR, "Attempted to add window with exiting application token "
                            + ".%s Aborting.", token);
                    return WindowManagerGlobal.ADD_APP_EXITING;
                } else if (type == TYPE_APPLICATION_STARTING) {
                    if (activity.mStartingWindow != null) {
                        ProtoLog.w(WM_ERROR, "Attempted to add starting window to "
                                + "token with already existing starting window");
                        return WindowManagerGlobal.ADD_DUPLICATE_ADD;
                    }
                    if (activity.mStartingData == null) {
                        ProtoLog.w(WM_ERROR, "Attempted to add starting window to "
                                + "token but already cleaned");
                        return WindowManagerGlobal.ADD_DUPLICATE_ADD;
                    }
                }
            } else if (rootType == TYPE_INPUT_METHOD) {
                //窗口类型为输入法，如果令牌类型不为输入法则提示异常
                if (token.windowType != TYPE_INPUT_METHOD) {
                    ProtoLog.w(WM_ERROR, "Attempted to add input method window with bad token "
                            + "%s.  Aborting.", attrs.token);
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            }else if (rootType == TYPE_VOICE_INTERACTION) {
                //窗口类型为音量调控，如果令牌类型不为音量调控则提示异常
                if (token.windowType != TYPE_VOICE_INTERACTION) {
                    ProtoLog.w(WM_ERROR, "Attempted to add voice interaction window with bad token "
                            + "%s.  Aborting.", attrs.token);
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (rootType == TYPE_WALLPAPER) {
                //窗口类型为壁纸，如果令牌类型不为壁纸则提示异常
                if (token.windowType != TYPE_WALLPAPER) {
                    ProtoLog.w(WM_ERROR, "Attempted to add wallpaper window with bad token "
                            + "%s.  Aborting.", attrs.token);
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (rootType == TYPE_ACCESSIBILITY_OVERLAY) {
                //窗口类型为无障碍服务，如果令牌类型不为壁纸则提示异常
                if (token.windowType != TYPE_ACCESSIBILITY_OVERLAY) {
                    ProtoLog.w(WM_ERROR,
                            "Attempted to add Accessibility overlay window with bad token "
                                    + "%s.  Aborting.", attrs.token);
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (type == TYPE_TOAST) {//窗口类型为吐司
                // Apps targeting SDK above N MR1 cannot arbitrary add toast windows.
                addToastWindowRequiresToken = doesAddToastWindowRequireToken(attrs.packageName,
                        callingUid, parentWindow);
                if (addToastWindowRequiresToken && token.windowType != TYPE_TOAST) {
                    ProtoLog.w(WM_ERROR, "Attempted to add a toast window with bad token "
                            + "%s.  Aborting.", attrs.token);
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (type == TYPE_QS_DIALOG) {//窗口类型为QS弹窗
                if (token.windowType != TYPE_QS_DIALOG) {
                    ProtoLog.w(WM_ERROR, "Attempted to add QS dialog window with bad token "
                            + "%s.  Aborting.", attrs.token);
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (token.asActivityRecord() != null) {//令牌不为空且Activity也不为空
                ProtoLog.w(WM_ERROR, "Non-null activity for system window of rootType=%d",
                        rootType);
                // It is not valid to use an app token with other system types; we will
                // instead make a new token for it (as if null had been passed in for the token).
                attrs.token = null;
                token = new WindowToken.Builder(this, client.asBinder(), type)
                        .setDisplayContent(displayContent)
                        .setOwnerCanManageAppTokens(session.mCanAddInternalSystemWindow)
                        .build();
            }
            ...代码省略...
      }
      
    private boolean unprivilegedAppCanCreateTokenWith(WindowState parentWindow,
        int callingUid, int type, int rootType, IBinder tokenForLog, String packageName) {
        if (rootType >= FIRST_APPLICATION_WINDOW && rootType <= LAST_APPLICATION_WINDOW) {//应用程序窗口
            ProtoLog.w(WM_ERROR, "Attempted to add application window with unknown token "
                    + "%s.  Aborting.", tokenForLog);
            return false;
        }
        if (rootType == TYPE_INPUT_METHOD) {//输入法
            ProtoLog.w(WM_ERROR, "Attempted to add input method window with unknown token "
                    + "%s.  Aborting.", tokenForLog);
            return false;
        }
        if (rootType == TYPE_VOICE_INTERACTION) {//音量调控
            ProtoLog.w(WM_ERROR,
                    "Attempted to add voice interaction window with unknown token "
                            + "%s.  Aborting.", tokenForLog);
            return false;
        }
        if (rootType == TYPE_WALLPAPER) {//壁纸
            ProtoLog.w(WM_ERROR, "Attempted to add wallpaper window with unknown token "
                    + "%s.  Aborting.", tokenForLog);
            return false;
        }
        if (rootType == TYPE_QS_DIALOG) {//QS弹窗
            ProtoLog.w(WM_ERROR, "Attempted to add QS dialog window with unknown token "
                    + "%s.  Aborting.", tokenForLog);
            return false;
        }
        if (rootType == TYPE_ACCESSIBILITY_OVERLAY) {//无障碍
            ProtoLog.w(WM_ERROR,
                    "Attempted to add Accessibility overlay window with unknown token "
                            + "%s.  Aborting.", tokenForLog);
            return false;
        }
        if (type == TYPE_TOAST) {//吐司
            // Apps targeting SDK above N MR1 cannot arbitrary add toast windows.
            if (doesAddToastWindowRequireToken(packageName, callingUid, parentWindow)) {
                ProtoLog.w(WM_ERROR, "Attempted to add a toast window with unknown token "
                        + "%s.  Aborting.", tokenForLog);
                return false;
            }
        }
        return true;
    }
}            
```

上面代码主要做了三个方向的判断。  
1）窗口令牌为空，结合窗口根类型判断是否需要返回异常，如果令牌为空且窗口类型为应用程序、输入法、音量调控、壁纸、QS弹窗、无障碍、吐司则需要返回异常，否则不但不返回异常最后还会帮窗口创建隐式令牌。  
2）窗口令牌不为空，则会进一步结合窗口令牌的类型来判断是否需要返回异常。  
3）窗口令牌不为空，如果令牌对应的Activity也不为空，则帮床阔创建隐式令牌

## 三、创建窗口状态对象

1、在窗口通过各种验证之后，系统首先会为其创建一个类型为WindowState的窗口状态对象。

```java
public class WindowManagerService extends IWindowManager.Stub
        implements Watchdog.Monitor, WindowManagerPolicy.WindowManagerFuncs {

    public int addWindow(Session session, IWindow client, int seq,
            LayoutParams attrs, int viewVisibility, int displayId, Rect outFrame,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel) { 
			...代码省略...
            创建窗口状态对象，该对象包含窗口所对应的所有信息，它就是要添加的窗口对象
            final WindowState win = new WindowState(this, session, client, token, parentWindow,
                    appOp[0], attrs, viewVisibility, session.mUid, userId,
                    session.mCanAddInternalSystemWindow);
           ...代码省略...
         }
   }
```

2、窗口状态对象WindowState的构造方法如下所示。

> frameworks/base/services/core/java/com/android/server/wm/WindowState.java

```java
class WindowState extends WindowContainer<WindowState> implements WindowManagerPolicy.WindowState,
        InsetsControlTarget, InputTarget {

    final WindowStateAnimator mWinAnimator;//窗口动画

    WindowState(WindowManagerService service, Session s, IWindow c, WindowToken token,
            WindowState parentWindow, int appOp, WindowManager.LayoutParams a, int viewVisibility,
            int ownerId, int showUserId, boolean ownerCanAddInternalSystemWindow) {
        this(service, s, c, token, parentWindow, appOp, a, viewVisibility, ownerId, showUserId,
                ownerCanAddInternalSystemWindow, new PowerManagerWrapper() {
                    @Override
                    public void wakeUp(long time, @WakeReason int reason, String details) {
                        service.mPowerManager.wakeUp(time, reason, details);
                    }

                    @Override
                    public boolean isInteractive() {
                        return service.mPowerManager.isInteractive();
                    }
                });
    }

    WindowState(WindowManagerService service, Session s, IWindow c, WindowToken token,
            WindowState parentWindow, int appOp, WindowManager.LayoutParams a, int viewVisibility,
            int ownerId, int showUserId, boolean ownerCanAddInternalSystemWindow,
            PowerManagerWrapper powerManagerWrapper) {
        super(service);
        mTmpTransaction = service.mTransactionFactory.get();
        mSession = s;//会话
        mClient = c;//客户端可以通过此对象回传消息
        mAppOp = appOp;
        mToken = token;//令牌
        mActivityRecord = mToken.asActivityRecord();//窗口对应的Activity
        mOwnerUid = ownerId;
        mShowUserId = showUserId;
        mOwnerCanAddInternalSystemWindow = ownerCanAddInternalSystemWindow;
        mWindowId = new WindowId(this);
        mAttrs.copyFrom(a);//拷贝窗口属性
        mLastSurfaceInsets.set(mAttrs.surfaceInsets);
        mViewVisibility = viewVisibility;
        mPolicy = mWmService.mPolicy;//窗口策略对象
        mContext = mWmService.mContext;
        DeathRecipient deathRecipient = new DeathRecipient();
        mPowerManagerWrapper = powerManagerWrapper;
        mForceSeamlesslyRotate = token.mRoundedCornerOverlay;
        mInputWindowHandle = new InputWindowHandleWrapper(new InputWindowHandle(
                mActivityRecord != null
                        ? mActivityRecord.getInputApplicationHandle(false /* update */) : null,
                getDisplayId()));
        mInputWindowHandle.setOwnerPid(s.mPid);
        mInputWindowHandle.setOwnerUid(s.mUid);
        mInputWindowHandle.setName(getName());
        mInputWindowHandle.setPackageName(mAttrs.packageName);
        mInputWindowHandle.setLayoutParamsType(mAttrs.type);
        // Check private trusted overlay flag and window type to set trustedOverlay variable of
        // input window handle.
        mInputWindowHandle.setTrustedOverlay(
                ((mAttrs.privateFlags & PRIVATE_FLAG_TRUSTED_OVERLAY) != 0
                        && mOwnerCanAddInternalSystemWindow)
                        || InputMonitor.isTrustedOverlay(mAttrs.type));
        if (DEBUG) {
            Slog.v(TAG, "Window " + this + " client=" + c.asBinder()
                            + " token=" + token + " (" + mAttrs.token + ")" + " params=" + a);
        }
        try {
            c.asBinder().linkToDeath(deathRecipient, 0);
        } catch (RemoteException e) {
            mDeathRecipient = null;
            mIsChildWindow = false;
            mLayoutAttached = false;
            mIsImWindow = false;
            mIsWallpaper = false;
            mIsFloatingLayer = false;
            mBaseLayer = 0;
            mSubLayer = 0;
            mWinAnimator = null;
            mWpcForDisplayAreaConfigChanges = null;
            return;
        }
        mDeathRecipient = deathRecipient;//窗口消亡回调对象，可以根据此对象是否为空则来判断窗口是否已经死亡
        if (mAttrs.type >= FIRST_SUB_WINDOW && mAttrs.type <= LAST_SUB_WINDOW) {
            // The multiplier here is to reserve space for multiple
            // windows in the same type layer.
            mBaseLayer = mPolicy.getWindowLayerLw(parentWindow)
                    * TYPE_LAYER_MULTIPLIER + TYPE_LAYER_OFFSET;
            mSubLayer = mPolicy.getSubWindowLayerFromTypeLw(a.type);
            mIsChildWindow = true;

            mLayoutAttached = mAttrs.type !=
                    WindowManager.LayoutParams.TYPE_APPLICATION_ATTACHED_DIALOG;
            mIsImWindow = parentWindow.mAttrs.type == TYPE_INPUT_METHOD
                    || parentWindow.mAttrs.type == TYPE_INPUT_METHOD_DIALOG;
            mIsWallpaper = parentWindow.mAttrs.type == TYPE_WALLPAPER;
        } else {
            // The multiplier here is to reserve space for multiple
            // windows in the same type layer.
            mBaseLayer = mPolicy.getWindowLayerLw(this)
                    * TYPE_LAYER_MULTIPLIER + TYPE_LAYER_OFFSET;
            mSubLayer = 0;
            mIsChildWindow = false;
            mLayoutAttached = false;
            mIsImWindow = mAttrs.type == TYPE_INPUT_METHOD
                    || mAttrs.type == TYPE_INPUT_METHOD_DIALOG;
            mIsWallpaper = mAttrs.type == TYPE_WALLPAPER;
        }
        mIsFloatingLayer = mIsImWindow || mIsWallpaper;

        if (mActivityRecord != null && mActivityRecord.mShowForAllUsers) {
            // Windows for apps that can show for all users should also show when the device is
            // locked.
            mAttrs.flags |= FLAG_SHOW_WHEN_LOCKED;
        }

        mWinAnimator = new WindowStateAnimator(this);//窗口状态动画
        mWinAnimator.mAlpha = a.alpha;//动画透明度

        mRequestedWidth = 0;
        mRequestedHeight = 0;
        mLastRequestedWidth = 0;
        mLastRequestedHeight = 0;
        mLayer = 0;
        mOverrideScale = mWmService.mAtmService.mCompatModePackages.getCompatScale(
                mAttrs.packageName, s.mUid);

        // Make sure we initial all fields before adding to parentWindow, to prevent exception
        // during onDisplayChanged.
        if (mIsChildWindow) {
            ProtoLog.v(WM_DEBUG_ADD_REMOVE, "Adding %s to %s", this, parentWindow);
            parentWindow.addChild(this, sWindowSubLayerComparator);
        }

        // System process or invalid process cannot register to display area config change.
        mWpcForDisplayAreaConfigChanges = (s.mPid == MY_PID || s.mPid < 0)
                ? null
                : service.mAtmService.getProcessController(s.mPid, s.mUid);
    }
}    
```

## 四、对客户端发来的窗口参数进行消毒和调整

1、在创建完窗口状态对象之后，系统会对窗口参数做适当的调整。

```java
public class WindowManagerService extends IWindowManager.Stub
        implements Watchdog.Monitor, WindowManagerPolicy.WindowManagerFuncs {

    public int addWindow(Session session, IWindow client, int seq,
            LayoutParams attrs, int viewVisibility, int displayId, Rect outFrame,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel) { 
			...代码省略...
            //创建窗口状态对象，该对象包含窗口所对应的所有信息
            final WindowState win = new WindowState(this, session, client, token, parentWindow,
                    appOp[0], attrs, viewVisibility, session.mUid, userId,
                    session.mCanAddInternalSystemWindow);
            if (win.mDeathRecipient == null) {//窗口已经死亡，返回异常
                // Client has apparently died, so there is no reason to
                // continue.
                ProtoLog.w(WM_ERROR, "Adding window client %s"
                        + " that is dead, aborting.", client.asBinder());
                return WindowManagerGlobal.ADD_APP_EXITING;
            }
            if (win.getDisplayContent() == null) {//窗口对应的屏幕设备为空，返回异常
                ProtoLog.w(WM_ERROR, "Adding window to Display that has been removed.");
                return WindowManagerGlobal.ADD_INVALID_DISPLAY;
            }
            //获取当前屏幕的显示策略
            final DisplayPolicy displayPolicy = displayContent.getDisplayPolicy();
            //调整窗口参数
            displayPolicy.adjustWindowParamsLw(win, win.mAttrs);
            //设置窗口对应的WindowInsets的可见性
            win.setRequestedVisibilities(requestedVisibilities);
            attrs.flags = sanitizeFlagSlippery(attrs.flags, win.getName(), callingUid, callingPid);
            //检验同一种类型的系统栏窗口是否被重复添加
            res = displayPolicy.validateAddingWindowLw(attrs, callingPid, callingUid);
            if (res != ADD_OKAY) {
                return res;
            }
            ...代码省略...
         }
   }

```

系统先是判断窗口是否已经死亡，然后判断窗口对应的屏幕设备是否为空，紧接着会获取当前屏幕设备的窗口策略对象，并调用该对象的adjustWindowParamsLw方法来适当调整客户端法来的窗口参数，之后还会再次调用DispalyPolicy的validateAddingWindowLw方法来检验同一种类型的系统栏窗口是否被重复添加。

2、DisplayPolicy和adjustWindowParamsLw方法和validateAddingWindowLw方法相关的代码如下所示。

> frameworks/base/services/core/java/com/android/server/wm/DisplayPolicy.java

```java
public class DisplayPolicy {
    
    // Alternative status bar for when flexible insets mapping is used to place the status bar on
    // another side of the screen.
    private WindowState mStatusBarAlt = null;//状态栏窗口状态对象
    @WindowManagerPolicy.AltBarPosition
    private int mStatusBarAltPosition = ALT_BAR_UNKNOWN;
    // Alternative navigation bar for when flexible insets mapping is used to place the navigation
    // bar elsewhere on the screen.
    private WindowState mNavigationBarAlt = null;//导航栏窗口状态对象
    @WindowManagerPolicy.AltBarPosition
    private int mNavigationBarAltPosition = ALT_BAR_UNKNOWN;
    // Alternative climate bar for when flexible insets mapping is used to place a climate bar on
    // the screen.
    private WindowState mClimateBarAlt = null;
    @WindowManagerPolicy.AltBarPosition
    private int mClimateBarAltPosition = ALT_BAR_UNKNOWN;
    // Alternative extra nav bar for when flexible insets mapping is used to place an extra nav bar
    // on the screen.
    private WindowState mExtraNavBarAlt = null;
    @WindowManagerPolicy.AltBarPosition
    private int mExtraNavBarAltPosition = ALT_BAR_UNKNOWN;
    
    /**
     *
     * 对客户端发来的窗口参数进行消毒和调整，比如禁止特定的窗口获取输入焦点。
     *
     * @param attrs The window layout parameters to be modified.  These values
     * are modified in-place.
     */
    public void adjustWindowParamsLw(WindowState win, WindowManager.LayoutParams attrs) {
        switch (attrs.type) {
            case TYPE_SYSTEM_OVERLAY:
            case TYPE_SECURE_SYSTEM_OVERLAY:
                // These types of windows can't receive input events.
                attrs.flags |= WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE;
                attrs.flags &= ~WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH;
                break;
            case TYPE_WALLPAPER:
                // Dreams and wallpapers don't have an app window token and can thus not be
                // letterboxed. Hence always let them extend under the cutout.
                attrs.layoutInDisplayCutoutMode = LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
                break;
            case TYPE_TOAST:
                // While apps should use the dedicated toast APIs to add such windows
                // it possible legacy apps to add the window directly. Therefore, we
                // make windows added directly by the app behave as a toast as much
                // as possible in terms of timeout and animation.
                if (attrs.hideTimeoutMilliseconds < 0
                        || attrs.hideTimeoutMilliseconds > TOAST_WINDOW_TIMEOUT) {
                    attrs.hideTimeoutMilliseconds = TOAST_WINDOW_TIMEOUT;
                }
                // Accessibility users may need longer timeout duration. This api compares
                // original timeout with user's preference and return longer one. It returns
                // original timeout if there's no preference.
                attrs.hideTimeoutMilliseconds = mAccessibilityManager.getRecommendedTimeoutMillis(
                        (int) attrs.hideTimeoutMilliseconds,
                        AccessibilityManager.FLAG_CONTENT_TEXT);
                // Toasts can't be clickable
                attrs.flags |= WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE;
                break;
            case TYPE_BASE_APPLICATION:
                // A non-translucent main app window isn't allowed to fit insets, as it would create
                // a hole on the display!
                if (attrs.isFullscreen() && win.mActivityRecord != null
                        && win.mActivityRecord.fillsParent()
                        && (win.mAttrs.privateFlags & PRIVATE_FLAG_FORCE_DRAW_BAR_BACKGROUNDS) != 0
                        && attrs.getFitInsetsTypes() != 0) {
                    throw new IllegalArgumentException("Illegal attributes: Main activity window"
                            + " that isn't translucent trying to fit insets: "
                            + attrs.getFitInsetsTypes()
                            + " attrs=" + attrs);
                }
                break;
        }

        //检测确认对应系统栏的位置是否需要更新
        if (mStatusBarAlt == win) {//状态栏
            mStatusBarAltPosition = getAltBarPosition(attrs);
        }
        if (mNavigationBarAlt == win) {//导航栏
            mNavigationBarAltPosition = getAltBarPosition(attrs);
        }
        if (mClimateBarAlt == win) {
            mClimateBarAltPosition = getAltBarPosition(attrs);
        }
        if (mExtraNavBarAlt == win) {
            mExtraNavBarAltPosition = getAltBarPosition(attrs);
        }
    }
    
    @WindowManagerPolicy.AltBarPosition
    private int getAltBarPosition(WindowManager.LayoutParams params) {
        switch (params.gravity) {
            case Gravity.LEFT:
                return ALT_BAR_LEFT;//int ALT_BAR_LEFT = 1 << 0;
            case Gravity.RIGHT:
                return ALT_BAR_RIGHT;//int ALT_BAR_RIGHT = 1 << 1;
            case Gravity.BOTTOM:
                return ALT_BAR_BOTTOM;//int ALT_BAR_BOTTOM = 1 << 2;
            case Gravity.TOP:
                return ALT_BAR_TOP;//int ALT_BAR_TOP = 1 << 3;
            default:
                return ALT_BAR_UNKNOWN;//int ALT_BAR_UNKNOWN = -1;
        }
    }
    
    /**
     * 检测窗口是否可以被添加到系统中
     *
     * 目前强制两种窗口类型只能有一个单例
     * <ul>
     * <li>{@link WindowManager.LayoutParams#TYPE_STATUS_BAR}</li>
     * <li>{@link WindowManager.LayoutParams#TYPE_NOTIFICATION_SHADE}</li>
     * <li>{@link WindowManager.LayoutParams#TYPE_NAVIGATION_BAR}</li>
     * </ul>
     *
     * @param attrs 将要被添加到系统中的窗口的信息
     *
     * @return If ok, WindowManagerImpl.ADD_OKAY.  If too many singletons,
     * WindowManagerImpl.ADD_MULTIPLE_SINGLETON
     */
    int validateAddingWindowLw(WindowManager.LayoutParams attrs, int callingPid, int callingUid) {
        if ((attrs.privateFlags & PRIVATE_FLAG_TRUSTED_OVERLAY) != 0) {
            mContext.enforcePermission(
                    android.Manifest.permission.INTERNAL_SYSTEM_WINDOW, callingPid, callingUid,
                    "DisplayPolicy");
        }
        if ((attrs.privateFlags & PRIVATE_FLAG_INTERCEPT_GLOBAL_DRAG_AND_DROP) != 0) {
            ActivityTaskManagerService.enforceTaskPermission("DisplayPolicy");
        }

        switch (attrs.type) {
            case TYPE_STATUS_BAR://禁止重复添加状态栏
                mContext.enforcePermission(
                        android.Manifest.permission.STATUS_BAR_SERVICE, callingPid, callingUid,
                        "DisplayPolicy");
                if ((mStatusBar != null && mStatusBar.isAlive())
                        || (mStatusBarAlt != null && mStatusBarAlt.isAlive())) {
                    return WindowManagerGlobal.ADD_MULTIPLE_SINGLETON;
                }
                break;
            case TYPE_NOTIFICATION_SHADE://禁止重复添加通知栏
                mContext.enforcePermission(
                        android.Manifest.permission.STATUS_BAR_SERVICE, callingPid, callingUid,
                        "DisplayPolicy");
                if (mNotificationShade != null) {
                    if (mNotificationShade.isAlive()) {
                        return WindowManagerGlobal.ADD_MULTIPLE_SINGLETON;
                    }
                }
                break;
            case TYPE_NAVIGATION_BAR://禁止重复添加导航栏
                mContext.enforcePermission(
                        android.Manifest.permission.STATUS_BAR_SERVICE, callingPid, callingUid,
                        "DisplayPolicy");
                if ((mNavigationBar != null && mNavigationBar.isAlive())
                        || (mNavigationBarAlt != null && mNavigationBarAlt.isAlive())) {
                    return WindowManagerGlobal.ADD_MULTIPLE_SINGLETON;
                }
                break;
            case TYPE_NAVIGATION_BAR_PANEL:
                // Check for permission if the caller is not the recents component.
                if (!mService.mAtmInternal.isCallerRecents(callingUid)) {
                    mContext.enforcePermission(
                            android.Manifest.permission.STATUS_BAR_SERVICE, callingPid, callingUid,
                            "DisplayPolicy");
                }
                break;
            case TYPE_STATUS_BAR_ADDITIONAL:
            case TYPE_STATUS_BAR_SUB_PANEL:
            case TYPE_VOICE_INTERACTION_STARTING:
                mContext.enforcePermission(
                        android.Manifest.permission.STATUS_BAR_SERVICE, callingPid, callingUid,
                        "DisplayPolicy");
                break;
            case TYPE_STATUS_BAR_PANEL:
                return WindowManagerGlobal.ADD_INVALID_TYPE;
        }

        if (attrs.providesInsetsTypes != null) {//窗口装饰区域类型不为空
            // Recents component is allowed to add inset types.
            if (!mService.mAtmInternal.isCallerRecents(callingUid)) {
                mContext.enforcePermission(
                        android.Manifest.permission.STATUS_BAR_SERVICE, callingPid, callingUid,
                        "DisplayPolicy");
            }
            enforceSingleInsetsTypeCorrespondingToWindowType(attrs.providesInsetsTypes);

            for (@InternalInsetsType int insetType : attrs.providesInsetsTypes) {
                switch (insetType) {
                    case ITYPE_STATUS_BAR://禁止重复添加状态栏装饰区域窗口
                        if ((mStatusBar != null && mStatusBar.isAlive())
                                || (mStatusBarAlt != null && mStatusBarAlt.isAlive())) {
                            return WindowManagerGlobal.ADD_MULTIPLE_SINGLETON;
                        }
                        break;
                    case ITYPE_NAVIGATION_BAR://禁止重复添加导航栏装饰区域窗口
                        if ((mNavigationBar != null && mNavigationBar.isAlive())
                                || (mNavigationBarAlt != null && mNavigationBarAlt.isAlive())) {
                            return WindowManagerGlobal.ADD_MULTIPLE_SINGLETON;
                        }
                        break;
                    case ITYPE_CLIMATE_BAR:
                        if (mClimateBarAlt != null && mClimateBarAlt.isAlive()) {
                            return WindowManagerGlobal.ADD_MULTIPLE_SINGLETON;
                        }
                        break;
                    case ITYPE_EXTRA_NAVIGATION_BAR:
                        if (mExtraNavBarAlt != null && mExtraNavBarAlt.isAlive()) {
                            return WindowManagerGlobal.ADD_MULTIPLE_SINGLETON;
                        }
                        break;
                }
            }
        }
        return ADD_OKAY;
    }
    
     /**
     * 检测同一种装饰区域类型的窗口是否超过1个，超过1个会直接抛出异常
     * @param insetsTypes
     */
    private static void enforceSingleInsetsTypeCorrespondingToWindowType(int[] insetsTypes) {
        int count = 0;
        for (int insetsType : insetsTypes) {
            switch (insetsType) {
                case ITYPE_NAVIGATION_BAR:
                case ITYPE_STATUS_BAR:
                case ITYPE_CLIMATE_BAR:
                case ITYPE_EXTRA_NAVIGATION_BAR:
                case ITYPE_CAPTION_BAR:
                    if (++count > 1) {
                        throw new IllegalArgumentException(
                                "Multiple InsetsTypes corresponding to Window type");
                    }
            }
        }
    }
}
```

结合上面代码可以发现DisplayPolicy的adjustWindowParamsLw方法和validateAddingWindowLw方法，很重要的一部分作用是为避免重复添加同一种类型的系统栏，这也是很合理的，否则如果系统中同时出现多个状态栏、导航栏怎么办？

## 五、窗口动画和焦点

1、继续往下看WindowManagerService的addWindow方法。

```java
public class WindowManagerService extends IWindowManager.Stub
        implements Watchdog.Monitor, WindowManagerPolicy.WindowManagerFuncs {

    public int addWindow(Session session, IWindow client, int seq,
            LayoutParams attrs, int viewVisibility, int displayId, Rect outFrame,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel) { 
			...代码省略...
            final boolean openInputChannels = (outInputChannel != null
                    && (attrs.inputFeatures & INPUT_FEATURE_NO_INPUT_CHANNEL) == 0);
            if (openInputChannels) {
                win.openInputChannel(outInputChannel);
            }
            // If adding a toast requires a token for this app we always schedule hiding
            // toast windows to make sure they don't stick around longer then necessary.
            // We hide instead of remove such windows as apps aren't prepared to handle
            // windows being removed under them.
            //
            // If the app is older it can add toasts without a token and hence overlay
            // other apps. To be maximally compatible with these apps we will hide the
            // window after the toast timeout only if the focused window is from another
            // UID, otherwise we allow unlimited duration. When a UID looses focus we
            // schedule hiding all of its toast windows.
            if (type == TYPE_TOAST) {
                if (!displayContent.canAddToastWindowForUid(callingUid)) {
                    ProtoLog.w(WM_ERROR, "Adding more than one toast window for UID at a time.");
                    return WindowManagerGlobal.ADD_DUPLICATE_ADD;
                }
                // Make sure this happens before we moved focus as one can make the
                // toast focusable to force it not being hidden after the timeout.
                // Focusable toasts are always timed out to prevent a focused app to
                // show a focusable toasts while it has focus which will be kept on
                // the screen after the activity goes away.
                if (addToastWindowRequiresToken
                        || (attrs.flags & FLAG_NOT_FOCUSABLE) == 0
                        || displayContent.mCurrentFocus == null
                        || displayContent.mCurrentFocus.mOwnerUid != callingUid) {
                    mH.sendMessageDelayed(
                            mH.obtainMessage(H.WINDOW_HIDE_TIMEOUT, win),
                            win.mAttrs.hideTimeoutMilliseconds);
                }
            }
            // Switch to listen to the {@link WindowToken token}'s configuration changes when
            // adding a window to the window context. Filter sub window type here because the sub
            // window must be attached to the parent window, which is attached to the window context
            // created window token.
            if (!win.isChildWindow()
                    && mWindowContextListenerController.hasListener(windowContextToken)) {
                final int windowContextType = mWindowContextListenerController
                        .getWindowType(windowContextToken);
                final Bundle options = mWindowContextListenerController
                        .getOptions(windowContextToken);
                if (type != windowContextType) {
                    ProtoLog.w(WM_ERROR, "Window types in WindowContext and"
                            + " LayoutParams.type should match! Type from LayoutParams is %d,"
                            + " but type from WindowContext is %d", type, windowContextType);
                    // We allow WindowProviderService to add window other than windowContextType,
                    // but the WindowProviderService won't be associated with the window's
                    // WindowToken.
                    if (!isWindowProviderService(options)) {
                        return WindowManagerGlobal.ADD_INVALID_TYPE;
                    }
                } else {
                    mWindowContextListenerController.registerWindowContainerListener(
                            windowContextToken, token, callingUid, type, options);
                }
            }

            //从这里开始，不会再返回异常或者错误
            res = ADD_OKAY;

            if (mUseBLAST) {
                res |= WindowManagerGlobal.ADD_FLAG_USE_BLAST;
            }

            if (displayContent.mCurrentFocus == null) {
                displayContent.mWinAddedSinceNullFocus.add(win);
            }

            if (excludeWindowTypeFromTapOutTask(type)) {
                displayContent.mTapExcludedWindows.add(win);
            }
            //调用WindowState的attach方法
            win.attach();
            //将binder和windowState以键值对的形式存储到mWindowMap中
            mWindowMap.put(client.asBinder(), win);
            //初始化窗口状态对象的状态控制
            win.initAppOpsState();
            //判断窗口对应的应用是否被系统禁用了
            final boolean suspended = mPmInternal.isPackageSuspended(win.getOwningPackage(),
                    UserHandle.getUserId(win.getOwningUid()));
            win.setHiddenWhileSuspended(suspended);
            //是否隐藏系统弹窗
            final boolean hideSystemAlertWindows = !mHidingNonSystemOverlayWindows.isEmpty();
            win.setForceHideNonSystemOverlayWindowIfNeeded(hideSystemAlertWindows);

            boolean imMayMove = true;
            //在windowState的令牌中添加windowState自身的引用
            win.mToken.addWindow(win);
            //将当前window添加到窗口策略DisplayPolicy中
            displayPolicy.addWindowLw(win, attrs);
            displayPolicy.setDropInputModePolicy(win, win.mAttrs);
            if (type == TYPE_APPLICATION_STARTING && activity != null) {//应用程序启动期间
                activity.attachStartingWindow(win);
                ProtoLog.v(WM_DEBUG_STARTING_WINDOW, "addWindow: %s startingWindow=%s",
                        activity, win);
            } else if (type == TYPE_INPUT_METHOD) {
                displayContent.setInputMethodWindowLocked(win);
                imMayMove = false;
            } else if (type == TYPE_INPUT_METHOD_DIALOG) {
                displayContent.computeImeTarget(true /* updateImeTarget */);
                imMayMove = false;
            } else {
                if (type == TYPE_WALLPAPER) {
                    displayContent.mWallpaperController.clearLastWallpaperTimeoutTime();
                    displayContent.pendingLayoutChanges |= FINISH_LAYOUT_REDO_WALLPAPER;
                } else if (win.hasWallpaper()) {
                    displayContent.pendingLayoutChanges |= FINISH_LAYOUT_REDO_WALLPAPER;
                } else if (displayContent.mWallpaperController.isBelowWallpaperTarget(win)) {
                    // If there is currently a wallpaper being shown, and
                    // the base layer of the new window is below the current
                    // layer of the target window, then adjust the wallpaper.
                    // This is to avoid a new window being placed between the
                    // wallpaper and its target.
                    displayContent.pendingLayoutChanges |= FINISH_LAYOUT_REDO_WALLPAPER;
                }
            }
            //窗口动画
            final WindowStateAnimator winAnimator = win.mWinAnimator;
            winAnimator.mEnterAnimationPending = true;
            winAnimator.mEnteringAnimation = true;
            // Check if we need to prepare a transition for replacing window first.
            if (!win.mTransitionController.isShellTransitionsEnabled()
                    && activity != null && activity.isVisible()
                    && !prepareWindowReplacementTransition(activity)) {
                // If not, check if need to set up a dummy transition during display freeze
                // so that the unfreeze wait for the apps to draw. This might be needed if
                // the app is relaunching.
                prepareNoneTransitionForRelaunching(activity);
            }

            if (displayPolicy.getLayoutHint(win.mAttrs, token, outInsetsState,
                    win.isClientLocal())) {
                res |= WindowManagerGlobal.ADD_FLAG_ALWAYS_CONSUME_SYSTEM_BARS;
            }

            if (mInTouchMode) {
                res |= WindowManagerGlobal.ADD_FLAG_IN_TOUCH_MODE;
            }
            if (win.mActivityRecord == null || win.mActivityRecord.isClientVisible()) {
                res |= WindowManagerGlobal.ADD_FLAG_APP_VISIBLE;
            }

            displayContent.getInputMonitor().setUpdateInputWindowsNeededLw();

            boolean focusChanged = false;
            if (win.canReceiveKeys()) {//窗口可以接受按键事件
                //更新系统的窗口焦点
                focusChanged = updateFocusedWindowLocked(UPDATE_FOCUS_WILL_ASSIGN_LAYERS,
                        false /*updateInputWindows*/);
                if (focusChanged) {
                    imMayMove = false;
                }
            }

            if (imMayMove) {
                displayContent.computeImeTarget(true /* updateImeTarget */);
            }

            // Don't do layout here, the window must call
            // relayout to be displayed, so we'll do it there.
            win.getParent().assignChildLayers();

            if (focusChanged) {
            	//焦点发生变化，更新当前输入法对应的焦点
                displayContent.getInputMonitor().setInputFocusLw(displayContent.mCurrentFocus,
                        false /*updateInputWindows*/);
            }
            displayContent.getInputMonitor().updateInputWindowsLw(false /*force*/);

            ProtoLog.v(WM_DEBUG_ADD_REMOVE, "addWindow: New client %s"
                    + ": window=%s Callers=%s", client.asBinder(), win, Debug.getCallers(5));

            if (win.isVisibleRequestedOrAdding() && displayContent.updateOrientation()) {
                displayContent.sendNewConfiguration();
            }

            // This window doesn't have a frame yet. Don't let this window cause the insets change.
            displayContent.getInsetsStateController().updateAboveInsetsState(
                    win, false /* notifyInsetsChanged */);

            getInsetsSourceControls(win, outActiveControls);
        }

        Binder.restoreCallingIdentity(origId);

        return res;
         }
   }            
```

## 💡 技术无价，赞赏随心

写文不易，如果本文帮你避开了“八小时踩坑”，或者让你直呼“学到了！”  
[欢迎扫码赞赏，让我知道这篇内容值得！](https://gitee.com/AFinalStone/RewardAndIncentive)