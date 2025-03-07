## 添加窗口的入口

在应用层，无论是Activity还是Dialog还是普通的Window，最终都是通过WindowManager的addView将布局view给添加到窗口。

```
//Activity添加布局，位于ActivityThread的handleResumeActivity中
public void handleResumeActivity(ActivityClientRecord r, boolean finalStateRequest,
     boolean isForward, String reason){
     ...
     ViewManager wm = a.getWindowManager();
     ...
     wm.addView(decor, l);
}

//Dialog添加布局，位于Dialog的show方法中
public void show() {
...
mWindowManager.addView(mDecor, l);
...
}

//普通Window
public void showWindow() {
...
WindowManager wm = a.getWindowManager();
wm .addView(mView, l);
...
}
```

最终都会通过ViewRootImpl的setView将布局提交给WSM

```
public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView,
            int userId) {
     ...
 res = mWindowSession.addToDisplayAsUser(mWindow, mWindowAttributes,
                            getHostVisibility(), mDisplay.getDisplayId(), userId,
                            mInsetsController.getRequestedVisibilities(), inputChannel,mTempInsets,
                            mTempControls);
...
}
```

mWindowSession具有跨进程通信的能力，代表一次window的会话，从此流程进入framework。

而[Toast](https://so.csdn.net/so/search?q=Toast&spm=1001.2101.3001.7020)则有些不同，是通过NotificationManager提交，最终通过NotificationManagerService与WMS交互，后面我们再单独看WMS中的不同之处

```
//Toast的show方法
public void show() {
...
INotificationManager service = getService();
...
service.enqueueToast(pkg, mToken, tn, mDuration, displayId);
...
}

//与NotificationManagerService交互,再与WMS交互
private void enqueueToast(String pkg, IBinder token, @Nullable CharSequence text,
                @Nullable ITransientNotification callback, int duration, int displayId,
                @Nullable ITransientNotificationCallback textCallback) {
    ...
Binder windowToken = new Binder();
                        mWindowManagerInternal.addWindowToken(windowToken, TYPE_TOAST, displayId,
                                null /* options */);
...//windowToken添加
mWindowManagerInternal.addWindowToken(windowToken, TYPE_TOAST, displayId,null /* options */);
}
```

## WMS收到应用添加窗口的请求

上面我们知道mWindowSession具有跨进程通信的能力，那在framework对端就是com.android.server.wm.Session对象  
应用层如何获取Session对象的呢？

```
    public static IWindowSession getWindowSession() {
        synchronized (WindowManagerGlobal.class) {
            if (sWindowSession == null) {
                try {
                    InputMethodManager.ensureDefaultInstanceForDefaultDisplayIfNecessary();
                    IWindowManager windowManager = getWindowManagerService();
                    sWindowSession = windowManager.openSession(
                            new IWindowSessionCallback.Stub() {
                                @Override
                                public void onAnimatorScaleChanged(float scale) {
                                    ValueAnimator.setDurationScale(scale);
                                }
                            });
                } catch (RemoteException e) {
                    throw e.rethrowFromSystemServer();
                }
            }
            return sWindowSession;
        }
    }
```

比较简单，应用层通过WMS的openSession方法，拿到Session对象。

继续上面的流程Session的addToDisplayAsUser

```
    @Override
    public int addToDisplayAsUser(IWindow window, WindowManager.LayoutParams attrs,
            int viewVisibility, int displayId, int userId, InsetsVisibilities requestedVisibilities,
            InputChannel outInputChannel, InsetsState outInsetsState,
            InsetsSourceControl[] outActiveControls) {
        return mService.addWindow(this, window, attrs, viewVisibility, displayId, userId,
                requestedVisibilities, outInputChannel, outInsetsState, outActiveControls);
    }
```

此处的mService就是WindowManagerService，Session什么都没做，就直接调用的WMS的addWindow方法，这里的参数比较多  
关注几个相对重要的  
**window**：IWindow 对象，从应用层传递过来，是一个[binder](https://so.csdn.net/so/search?q=binder&spm=1001.2101.3001.7020)对象。  
**attrs**：布局参数  
**outInputChannel**：输入管道，注册输入事件  
继续看WindowManagerService的addWindow，addWindow整个方法比较长，有400多行，主要分几个部分来看，

```
    public int addWindow(Session session, IWindow client, LayoutParams attrs, int viewVisibility,
            int displayId, int requestUserId, InsetsVisibilities requestedVisibilities,
            InputChannel outInputChannel, InsetsState outInsetsState,
            InsetsSourceControl[] outActiveControls) {
...
            }
```

## 窗口的初步验证

```
    public int addWindow(Session session, IWindow client, LayoutParams attrs, int viewVisibility,
            int displayId, int requestUserId, InsetsVisibilities requestedVisibilities,
            InputChannel outInputChannel, InsetsState outInsetsState,
            InsetsSourceControl[] outActiveControls) {
            ......
            
//displayContent表示一块显示器，如果目标显示器不存在，肯定会返回一个错误ADD_INVALID_DISPLAY
            if (displayContent == null) {
                return WindowManagerGlobal.ADD_INVALID_DISPLAY;
            }
            
            //此窗口是否有权限访问当前显示器
            if (!displayContent.hasAccess(session.mUid)) {
                return WindowManagerGlobal.ADD_INVALID_DISPLAY;
            }

//如果窗口已经添加过一次，那mWindowMap肯定会存在。这里是禁止窗口二次添加
            if (mWindowMap.containsKey(client.asBinder())) {
                return WindowManagerGlobal.ADD_DUPLICATE_ADD;
            }

//对子窗口的处理
            if (type >= FIRST_SUB_WINDOW && type <= LAST_SUB_WINDOW) {
                parentWindow = windowForClientLocked(null, attrs.token, false);
                //如果当前窗口是子窗口类型，那么父窗口必须存在，否则ADD_BAD_SUBWINDOW_TOKEN
                if (parentWindow == null) {
                    return WindowManagerGlobal.ADD_BAD_SUBWINDOW_TOKEN;
                }
                 //如果当前窗口是子窗口类型，那么父窗口也是子窗口类型，返回ADD_BAD_SUBWINDOW_TOKEN，因为不允许在子窗口类型下再添加子窗口
                if (parentWindow.mAttrs.type >= FIRST_SUB_WINDOW
                        && parentWindow.mAttrs.type <= LAST_SUB_WINDOW) {
                    return WindowManagerGlobal.ADD_BAD_SUBWINDOW_TOKEN;
                }
            }

//TYPE_PRIVATE_PRESENTATION是私有虚拟显示器上的演示窗口，不允许显示在非私有显示器上，标记为FLAG_PRIVATE的显示器为私有显示器
            if (type == TYPE_PRIVATE_PRESENTATION && !displayContent.isPrivate()) {
                return WindowManagerGlobal.ADD_PERMISSION_DENIED;
            }
//TYPE_PRESENTATION外部显示器上进行演示的窗口，若显示器不是公共演示显示器，则禁止显示。
            if (type == TYPE_PRESENTATION && !displayContent.getDisplay().isPublicPresentation()) {
                return WindowManagerGlobal.ADD_INVALID_DISPLAY;
            }
//对userId的验证
            int userId = UserHandle.getUserId(session.mUid);
            if (requestUserId != userId) {
                try {
                    mAmInternal.handleIncomingUser(callingPid, callingUid, requestUserId,
                            false /*allowAll*/, ALLOW_NON_FULL, null, null);
                } catch (Exception exp) {
                    return WindowManagerGlobal.ADD_INVALID_USER;
                }
                // It's fine to use this userId
                userId = requestUserId;
            }
        ......
    }
```

以上主要是对显示器类型，子窗口以及userId的验证，一般情况下，上述验证都会通过，我们知道窗口类型总体可分为系统窗口，应用窗口和子窗口，下面进入窗口类型的验证

## 窗口的类型及token的验证

```
    public int addWindow(Session session, IWindow client, LayoutParams attrs, int viewVisibility,
            int displayId, int requestUserId, InsetsVisibilities requestedVisibilities,
            InputChannel outInputChannel, InsetsState outInsetsState,
            InsetsSourceControl[] outActiveControls) {
...
//先从dc中获取WindowToken，如果有父窗口，那使用父窗口的token去拿到WindowToken。
//getWindowToken主要是从一个mTokenMap中获取，displayContent中的成员mTokenMap是以binder为key，WindowToken为value的map对象
            WindowToken token = displayContent.getWindowToken(
                    hasParent ? parentWindow.mAttrs.token : attrs.token);
            final int rootType = hasParent ? parentWindow.mAttrs.type : type;
}
```

token 何时添加到displayContent的mTokenMap中的呢？就在WindowToken 的构造方法中

```
    protected WindowToken(WindowManagerService service, IBinder _token, int type,
            boolean persistOnEmpty, DisplayContent dc, boolean ownerCanManageAppTokens,
            boolean roundedCornerOverlay, boolean fromClientToken, @Nullable Bundle options) {
        super(service);
        token = _token;
        windowType = type;
        mOptions = options;
        mPersistOnEmpty = persistOnEmpty;
        mOwnerCanManageAppTokens = ownerCanManageAppTokens;
        mRoundedCornerOverlay = roundedCornerOverlay;
        mFromClientToken = fromClientToken;
        //在这里
        if (dc != null) {
            dc.addWindowToken(token, this);
        }
    }
```

ActivityRecord是activity启动过程中的重要对象，ActivityRecord继承至WindowToken，所以对于activity，很早就完成了将token添加到mTokenMap中。继续看addWindow

```
    public int addWindow(Session session, IWindow client, LayoutParams attrs, int viewVisibility,
            int displayId, int requestUserId, InsetsVisibilities requestedVisibilities,
            InputChannel outInputChannel, InsetsState outInsetsState,
            InsetsSourceControl[] outActiveControls) {
...
WindowToken token = displayContent.getWindowToken(
                    hasParent ? parentWindow.mAttrs.token : attrs.token);
if (token == null) {
if (!unprivilegedAppCanCreateTokenWith(parentWindow, callingUid, type,
                        rootType, attrs.token, attrs.packageName)) {
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (rootType >= FIRST_APPLICATION_WINDOW
                    && rootType <= LAST_APPLICATION_WINDOW) {
                activity = token.asActivityRecord();
                if (activity == null) {
                    return WindowManagerGlobal.ADD_NOT_APP_TOKEN;
                } else if (activity.getParent() == null) {
                    return WindowManagerGlobal.ADD_APP_EXITING;
                } else if (type == TYPE_APPLICATION_STARTING) {
                    if (activity.mStartingWindow != null) {
                        return WindowManagerGlobal.ADD_DUPLICATE_ADD;
                    }
                    if (activity.mStartingData == null) {
                        return WindowManagerGlobal.ADD_DUPLICATE_ADD;
                    }
                }
            } else if (rootType == TYPE_INPUT_METHOD) {
                if (token.windowType != TYPE_INPUT_METHOD) {
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (rootType == TYPE_VOICE_INTERACTION) {
                if (token.windowType != TYPE_VOICE_INTERACTION) {
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (rootType == TYPE_WALLPAPER) {
                if (token.windowType != TYPE_WALLPAPER) {
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (rootType == TYPE_ACCESSIBILITY_OVERLAY) {
                if (token.windowType != TYPE_ACCESSIBILITY_OVERLAY) {
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (type == TYPE_TOAST) {
                addToastWindowRequiresToken = doesAddToastWindowRequireToken(attrs.packageName,
                        callingUid, parentWindow);
                if (addToastWindowRequiresToken && token.windowType != TYPE_TOAST) {
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (type == TYPE_QS_DIALOG) {
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (token.asActivityRecord() != null) {

            }
}
 
```

这一步主要是完成对token的赋值，通过displayContent取出token，一般情况下，首次添加普通窗口token都为nul，但是有几类特殊的窗口，在这之前就要完成token的赋值，看unprivilegedAppCanCreateTokenWith

```
    private boolean unprivilegedAppCanCreateTokenWith(WindowState parentWindow,
            int callingUid, int type, int rootType, IBinder tokenForLog, String packageName) {
        if (rootType >= FIRST_APPLICATION_WINDOW && rootType <= LAST_APPLICATION_WINDOW) {
            return false;
        }
        if (rootType == TYPE_INPUT_METHOD) {
            return false;
        }
        if (rootType == TYPE_VOICE_INTERACTION) {
            return false;
        }
        if (rootType == TYPE_WALLPAPER) {
            return false;
        }
        if (rootType == TYPE_QS_DIALOG) {
            return false;
        }
        if (rootType == TYPE_ACCESSIBILITY_OVERLAY) {
            return false;
        }
        if (type == TYPE_TOAST) {
            // Apps targeting SDK above N MR1 cannot arbitrary add toast windows.
            if (doesAddToastWindowRequireToken(packageName, callingUid, parentWindow)) {
                return false;
            }
        }
        return true;
    }
```

**FIRST\_APPLICATION\_WINDOW  
TYPE\_INPUT\_METHOD  
TYPE\_VOICE\_INTERACTION  
TYPE\_WALLPAPER  
TYPE\_QS\_DIALOG  
TYPE\_ACCESSIBILITY\_OVERLAY  
TYPE\_TOAST**  
这几类窗口必须在这之前就要完成对token的赋值，也就是添加到displayContent中，如FIRST\_APPLICATION\_WINDOW 应用窗口，应用窗口的ActivityRecord继承至WindowToken，上面说过在构造方法中就将token添加到displayContent中了。  
else if分支就是对这几类型窗口与token的验证，例如不能用一个语音窗口的token去添加一个输入窗口，不能用同一个apptoken添加多个应用窗口等等。验证完成后就是对WindowState的赋值。

## 描述具体的窗口WindowState

```
    public int addWindow(Session session, IWindow client, LayoutParams attrs, int viewVisibility,
            int displayId, int requestUserId, InsetsVisibilities requestedVisibilities,
            InputChannel outInputChannel, InsetsState outInsetsState,
            InsetsSourceControl[] outActiveControls) {
...
    final WindowState win = new WindowState(this, session, client, token, parentWindow,
                  appOp[0], attrs, viewVisibility, session.mUid, userId,
                  session.mCanAddInternalSystemWindow);
       //走到这里一般情况下表示窗口已完成验证，可以添加窗口，首先创建WindowState表示一个具体的窗口，

//根据窗口策略，对窗口属性做一些调整
       final DisplayPolicy displayPolicy = displayContent.getDisplayPolicy();
            displayPolicy.adjustWindowParamsLw(win, win.mAttrs);
            
            //与注册输入管道相关，输入事件在下一篇文章分析
            if  (openInputChannels) {
                win.openInputChannel(outInputChannel);
            }
            ...
            //走到这儿才真正表示本次窗口添加没有异常。
            win.attach(); //记录窗口已经添加
            mWindowMap.put(client.asBinder(), win); // mWindowMap保存着binder与WindowState 
            win.mToken.addWindow(win); // win.mToken是WindowToken ，WindowState 作为 WindowToken 的Child。
            //这里需要先看WindowContainer的各种继承关系才好理解WindowState 为什么要作为WindowToken 的Child。

//后面对焦点的处理
if (win.canReceiveKeys()) {
                focusChanged = updateFocusedWindowLocked(UPDATE_FOCUS_WILL_ASSIGN_LAYERS,
                        false /*updateInputWindows*/);
                if (focusChanged) {
                    imMayMove = false;
                }
            }
            displayContent.getInputMonitor().setUpdateInputWindowsNeededLw();
            if (focusChanged) {
                displayContent.getInputMonitor().setInputFocusLw(displayContent.mCurrentFocus,
                        false /*updateInputWindows*/);
            }
            displayContent.getInputMonitor().updateInputWindowsLw(false /*force*/);
            
            return res;
}
```

## 小结

addWindow整个方法主要是对窗口的验证，包括对窗口类型，token等等，验证成功后，会创建对应的WindowState与WindowToken ，这两个对象是窗口里非常重要的。我们平时开发中遇到一些添加窗口失败的异常，基本都可以在addWindow里找到原因。到此也只是添加窗口，窗口的surface何时创建，又如何传递给应用层的流程还并未开始，在下一篇继续分析WMS中如何创建surface以及传递给应用开启绘制的流程。