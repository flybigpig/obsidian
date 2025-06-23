## 前言

在 Android 系统中，WindowToken（窗口令牌） 和 WindowState（窗口状态）是两个重要的类，用于管理窗口视图的显示和布局。  
WindowToken用于标识窗口中的视图层级，而WindowState用于管理窗口的状态和属性。这两个类在 Android 系统中起着重要的作用，帮助确保应用程序界面正确地显示和布局，只有认识了这两个类我们才能更好的理解WindowManagerService这个服务。

## WindowContainer

### 类介绍

```java
/**
 * Defines common functionality for classes that can hold windows directly or through their
 * children in a hierarchy form.
 * The test class is {@link WindowContainerTests} which must be kept up-to-date and ran anytime
 * changes are made to this class.
 */
class WindowContainer<E extends WindowContainer> extends ConfigurationContainer<E>
        implements Comparable<WindowContainer>, Animatable, SurfaceFreezer.Freezable {
}
```

WindowContainer为哪些可以在层级树表格中可以直接持有窗口或者通过子类进行持有的类定义了一些通用的方法和属性。

### 继承关系

代码中常见的RootWindowContainer、DisplayContent、DisplayArea、DisplayArea.Tokens、TaskDisplayArea、Task、ActivityRecord、WindowToken、WindowState都是直接或间接的继承该类，具体继承关系可以参看下图。![继承关系](https://i-blog.csdnimg.cn/direct/ada7bd054ed74555a2c79f3ef7990677.png)

+   RootWindowContainer：根窗口容器，树的根是它。通过它遍历寻找，可以找到窗口树上的窗口。它的孩子是DisplayContent。
+   DisplayContent：该类是对应着显示屏幕的，Android是支持多屏幕的，所以可能存在多个DisplayContent对象。上图只画了一个对象的结构，其他对象的结构也是和画的对象的结构是相似的。
+   DisplayArea：该类是对应着显示屏幕下面的，代表一组窗口合集,具有多个子类，如Tokens，TaskDisplayArea等
+   TaskDisplayArea：它为DisplayContent的孩子，对应着窗口层次的第2层。第2层作为应用层，看它的定义：int APPLICATION\_LAYER = 2，应用层的窗口是处于第2层。TaskDisplayArea的孩子是Task类，其实它的孩子类型也可以是TaskDisplayArea。而Task的孩子则可以是ActivityRecord，也可以是Task。
+   Tokens：代表专门包含WindowTokens的容器，它的孩子是WindowToken，而WindowToken的孩子则为WindowState对象。WindowState是对应着一个窗口的。
+   ImeContainer：它是输入法窗口的容器，它的孩子是WindowToken类型。WindowToken的孩子为WindowState类型，而WindowState类型则对应着输入法窗口。
+   Task：任务，它的孩子可以是Task，也可以是ActivityRecord类型。
+   ActivityRecord：是对应着应用进程中的Activity的。ActivityRecord是继承WindowToken的，它的孩子类型为WindowState。
+   WindowState：WindowState是对应着一个窗口的。

### 重要成员变量

```java
class WindowContainer<E extends WindowContainer> extends ConfigurationContainer<E>
        implements Comparable<WindowContainer>, Animatable, SurfaceFreezer.Freezable {
    //窗口层级的父节点
    private WindowContainer<WindowContainer> mParent = null;

    //子节点，而且子节点的list顺序代表就是z轴的层级显示顺序，尾部的节点比头部的节点的z轴层级要高。
    protected final WindowList<E> mChildren = new WindowList<E>();
}
```

## 窗口层级结构树

如果我们一上来就结合系统源码去学习WMS关于窗口的层级结构，那么总会有一种盲人摸象的感觉，比较好的方法是我们先对实际运行种的系统的窗口层级数有个整体认识，然后再结合源码进行分析。

### 层级结构树堆栈信息

通过adb shell dumpsys activity containers可以打印出当前系统中的窗口层级数信息。

```java
ACTIVITY MANAGER CONTAINERS (dumpsys activity containers)
ROOT type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
  #0 Display 0 name="内置屏幕" type=undefined mode=fullscreen override-mode=fullscreen requested-bounds=[0,0][1080,2400] bounds=[0,0][1080,2400]
   #4 Leaf:100:100 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
   #3 HideDisplayCutout:58:99 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #2 OneHanded:67:99 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 FullscreenMagnification:67:99 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
      #0 Leaf:67:99 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #1 FullscreenMagnification:66:66 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 Leaf:66:66 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
      #0 WindowToken{bd56012 type=2015 android.view.OplusViewRootImplHooks$ColorW@fd9139d} type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
       #0 98f51e0 PointerLocation - display 0 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #0 OneHanded:58:65 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #2 FullscreenMagnification:61:65 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
      #0 Leaf:61:65 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
       #1 WindowToken{61c5371 type=2026 android.os.BinderProxy@e64d18} type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #0 8445e51 OplusPrivacyIconTopRight type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
       #0 WindowToken{376c16c type=2026 android.os.BinderProxy@c65d140} type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #0 ca5537a OplusPrivacyIconTopLeft type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #1 Leaf:60:60 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 FullscreenMagnification:58:59 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
      #0 Leaf:58:59 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
   #2 Leaf:56:57 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #3 WindowToken{cf318e2 type=2024 android.os.BinderProxy@789edad} type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 38d3f30 ColorSideGestureRight type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #2 WindowToken{624f0b type=2024 android.os.BinderProxy@ac2eeda} type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 664d0e7 ColorSideGestureLeft type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #1 WindowToken{4b77211 type=2024 android.os.BinderProxy@ad64238} type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 102ef76 pip-dismiss-overlay type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #0 WindowToken{fec29ba type=2019 android.os.BinderProxy@22239dc} type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 c20a06b NavigationBar0 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
   #1 HideDisplayCutout:32:55 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #0 OneHanded:32:55 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #1 FullscreenMagnification:33:55 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
      #0 Leaf:33:55 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 Leaf:32:32 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
   #0 WindowedMagnification:0:31 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #4 HideDisplayCutout:18:31 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 OneHanded:18:31 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
      #0 FullscreenMagnification:18:31 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
       #0 Leaf:18:31 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #3 OneHanded:17:17 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 FullscreenMagnification:17:17 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
      #0 Leaf:17:17 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
       #0 WindowToken{ea3abd8 type=2040 android.os.BinderProxy@712fb4a} type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #0 b1c2b31 NotificationShade type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #2 HideDisplayCutout:16:16 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 OneHanded:16:16 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
      #0 FullscreenMagnification:16:16 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
       #0 Leaf:16:16 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #1 OneHanded:15:15 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 FullscreenMagnification:15:15 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
      #0 Leaf:15:15 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
       #0 WindowToken{44ec633 type=2000 android.os.BinderProxy@dd7496d} type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #0 4601df0 StatusBar type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
    #0 HideDisplayCutout:0:14 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
     #0 OneHanded:0:14 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
      #1 ImePlaceholder:13:14 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
       #0 ImeContainer type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #0 WindowToken{e664e78 type=2011 android.os.Binder@994c3db} type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #0 bcd1b0e InputMethod type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
      #0 FullscreenMagnification:0:12 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
       #2 Leaf:3:12 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #0 WindowToken{4065228 type=2038 android.os.BinderProxy@a616862} type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #0 c3e851a ShellDropTarget type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
       #1 DefaultTaskDisplayArea type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #3 Task=51 type=standard mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #0 ActivityRecord{eb42d1c u0 com.example.empty/.InputActivity} t51} type=standard mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
          #0 5c35675 com.example.empty/com.example.empty.InputActivity type=standard mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #2 Task=1 type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #0 Task=2 type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
          #0 ActivityRecord{bc97017 u0 com.android.launcher/.Launcher} t2} type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
           #1 353c57c com.coloros.assistantscreen type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
           #0 11637b5 com.android.launcher/com.android.launcher.Launcher type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #1 Task=3 type=undefined mode=fullscreen override-mode=fullscreen requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #0 Task=4 type=undefined mode=fullscreen override-mode=fullscreen requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #1 Task=6 type=undefined mode=multi-window override-mode=multi-window requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #0 Task=5 type=undefined mode=multi-window override-mode=multi-window requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
       #0 Leaf:0:1 type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #0 WallpaperWindowToken{c7090c2 token=android.os.Binder@1f4d90d} type=undefined mode=fullscreen override-mode=fullscreen requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #0 7f48c51 com.android.systemui.ImageWallpaper type=undefined mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
```

可以将以上dumpsys的输入信息转化为图表，看起来会更加直观。

## 一、WindowToken（窗口令牌）

1、介绍

+   WindowToken主要用于标识Android系统中窗口中的特定视图层级，它代表了窗口中的一个视图层级的令牌。
+   在 Android 中，窗口是视图层级的容器，用于组织和管理应用程序界面中的不同元素。每个窗口都有一个关联的 WindowToken，用于标识该窗口中的视图层级。
+   WindowToken 通常与窗口管理器一起使用，用于跟踪窗口的属性和状态，并确保正确地显示和管理窗口。
+   例如，当应用程序中有多个窗口（例如活动、对话框、弹出窗口等）时，每个窗口都有自己的 WindowToken，用于标识其包含的视图层级。

2、WindowToken这个类如下所示。

> frameworks/base/services/core/java/com/android/server/wm/WindowToken.java

```java
class WindowToken extends WindowContainer<WindowState> {

    final IBinder token;
    final int windowType;
    final Bundle mOptions;
    private final boolean mFromClientToken;
    private boolean mClientVisible;
    
    protected WindowToken(WindowManagerService service, IBinder _token, int type,
            boolean persistOnEmpty, DisplayContent dc, boolean ownerCanManageAppTokens) {
        this(service, _token, type, persistOnEmpty, dc, ownerCanManageAppTokens,
                false /* roundedCornerOverlay */, false /* fromClientToken */, null /* options */);
    }

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
        if (dc != null) {
            dc.addWindowToken(token, this);
        }
    }

    static class Builder {
        private final WindowManagerService mService;
        private final IBinder mToken;
        @WindowType
        private final int mType;

        private boolean mPersistOnEmpty;
        private DisplayContent mDisplayContent;
        private boolean mOwnerCanManageAppTokens;
        private boolean mRoundedCornerOverlay;
        private boolean mFromClientToken;
        @Nullable
        private Bundle mOptions;

        Builder(WindowManagerService service, IBinder token, int type) {
            mService = service;
            mToken = token;
            mType = type;
        }

        /** @see WindowToken#mPersistOnEmpty */
        Builder setPersistOnEmpty(boolean persistOnEmpty) {
            mPersistOnEmpty = persistOnEmpty;
            return this;
        }

        /** Sets the {@link DisplayContent} to be associated. */
        Builder setDisplayContent(DisplayContent dc) {
            mDisplayContent = dc;
            return this;
        }

        /** @see WindowToken#mOwnerCanManageAppTokens */
        Builder setOwnerCanManageAppTokens(boolean ownerCanManageAppTokens) {
            mOwnerCanManageAppTokens = ownerCanManageAppTokens;
            return this;
        }

        /** @see WindowToken#mRoundedCornerOverlay */
        Builder setRoundedCornerOverlay(boolean roundedCornerOverlay) {
            mRoundedCornerOverlay = roundedCornerOverlay;
            return this;
        }

        /** @see WindowToken#mFromClientToken */
        Builder setFromClientToken(boolean fromClientToken) {
            mFromClientToken = fromClientToken;
            return this;
        }

        /** @see WindowToken#mOptions */
        Builder setOptions(Bundle options) {
            mOptions = options;
            return this;
        }

        WindowToken build() {
            return new WindowToken(mService, mToken, mType, mPersistOnEmpty, mDisplayContent,
                    mOwnerCanManageAppTokens, mRoundedCornerOverlay, mFromClientToken, mOptions);
        }
    }
 }
```

+   WindowToken将属于同一个应用的组件窗口组织在一起。

所谓的应用组件可以是 Activity、InputMethod、Wallpaper以及Dream。在WMS对窗口的管理过程中，用 WindowToken指代一个应用组件。例如在进行窗口ZOrder排序时，属于同一个 WindowToken的窗口会被安排在一起，而且在其中定义的一些属性将会影响所有属于此WindowToken的窗口。这些都表明了属于同一个WindowToken的窗口之间的紧密联系。

+   WindowToken具有令牌的作用，是对应用组件的行为进行规范管理的一个手段。

WindowToken由应用组件或其管理者负责向WMS声明并持有。结合[Android 12系统源码\_窗口管理（三）WindowManagerService对窗口的管理过程](https://blog.csdn.net/abc6368765/article/details/130891975)我们可以知道，应用组件在需要新的窗口时，必须提供WindowToken以表明自己的身份，并且窗口的类型必须与所持有的WindowToken类型一致。

3、既然应用组件在创建一个窗口时必须指定一个有效的WindowToken才行，那么 WindowToken究竟该如何声明呢？来看一下WMS的addWindowToken方法。

> frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java

```java
public class WindowManagerService extends IWindowManager.Stub
        implements Watchdog.Monitor, WindowManagerPolicy.WindowManagerFuncs {

    @Override
    public void addWindowToken(@NonNull IBinder binder, int type, int displayId,
                               @Nullable Bundle options) {
        //需要声明Token的调用者拥有MANAGE_APP_TOKENS的权限，否则直接抛出异常
        if (!checkCallingPermission(MANAGE_APP_TOKENS, "addWindowToken()")) {
            throw new SecurityException("Requires MANAGE_APP_TOKENS permission");
        }
        synchronized (mGlobalLock) {
            final DisplayContent dc = getDisplayContentOrCreate(displayId, null /* token */);
            if (dc == null) {
                ProtoLog.w(WM_ERROR, "addWindowToken: Attempted to add token: %s"
                        + " for non-exiting displayId=%d", binder, displayId);
                return;
            }
            WindowToken token = dc.getWindowToken(binder);
            if (token != null) {
                ProtoLog.w(WM_ERROR, "addWindowToken: Attempted to add binder token: %s"
                        + " for already created window token: %s"
                        + " displayId=%d", binder, token, displayId);
                return;
            }
            if (type == TYPE_WALLPAPER) {//如果是壁纸类型，则创建壁纸窗口令牌
                new WallpaperWindowToken(this, binder, true, dc,
                        true /* ownerCanManageAppTokens */, options);
            } else {
                //创建通用的窗口令牌
                new WindowToken.Builder(this, binder, type)
                        .setDisplayContent(dc)
                        .setPersistOnEmpty(true)
                        .setOwnerCanManageAppTokens(true)
                        .setOptions(options)
                        .build();
            }
        }
    }

}
```

## 二、WindowState（窗口状态）

1、介绍

+   WindowState 是一个表示窗口状态的类。它用于跟踪窗口的属性和状态，以确保窗口正确地显示和布局。
+   WindowState 包含了窗口的各种属性，如位置、大小、可见性等。通过管理这些属性，系统可以管理窗口的显示和布局。
+   WindowState 还可以包含其他与窗口相关的信息，如焦点状态、输入事件处理等。
+   与 WindowToken 不同，WindowState 更专注于管理窗口的状态和属性，而不是标识窗口中的特定视图层级。

## 💡 技术无价，赞赏随心

写文不易，如果本文帮你避开了“八小时踩坑”，或者让你直呼“学到了！”  
[欢迎扫码赞赏，让我知道这篇内容值得！](https://gitee.com/AFinalStone/RewardAndIncentive)