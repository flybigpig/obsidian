
## 前言

上一篇我们具体分析了窗口管理者WindowManagerService的启动流程，对于WindowManagerService有了一个初步的认识。在此基础上，我本打算应该进一步分析WindowManagerService是如何管理系统中的各种窗口的，然而由于Android系统的架构设计，在分析WindowManagerService之前，我们必须先对WindowManager有一个基本的认识，才能更好的理解WindowManagerService的对窗口的管理过程。  
![系统对Window的操作](https://i-blog.csdnimg.cn/blog_migrate/e5b048c3269766af11e4e8217229aa69.png)  
如上图所示，系统主要是通过WindowManager和WindowManagerService对窗口进行操作管理的，WindowManager更上层一些，WindowManagerService更底层一些，WindowManager对窗口的各种处理最终都是通过调用WindowMnagerService实现的。不同类型的窗口，WindowManager的添加过程可能会有所不同，但是WindowManagerService处理的部分，基本上是一样的。

## 一、窗口类型

在分析WindowManager对窗口的管理之前，我们需要先来认识一下Android系统中的窗口类型，因为不同的窗口类型，WindowManager的添加过程会有所不同。

Window的类型有很多种，比如应用程序窗口、系统错误窗口、输入法窗口、PopupWindow、Toast、Dialog等。总的来说Window分为三大类型，分别是Application Window（应用程序窗口）、Sub Window（子窗口）、System Window（系统窗口），每个大类型中又分很多小类型，它们都定义在WindowManager的静态内部类LayoutParams中，下面简单介绍一下Window的三大类型。

### 1.1 应用程序窗口

Activity就是一个典型的应用程序窗口，应用程序窗口包含的类型如下所示：

> frameworks/base/core/java/android/view/WindowManager.java

```java
public interface WindowManager extends ViewManager {
    public static class LayoutParams extends ViewGroup.LayoutParams implements Parcelable {
        //应用程序窗口的开始值
        public static final int FIRST_APPLICATION_WINDOW = 1; 
        //应用程序窗口的基础值，其他窗口的值都要大于这个值
        public static final int TYPE_BASE_APPLICATION   = 1;
        //普通的应用程序窗口类型
        public static final int TYPE_APPLICATION        = 2;
        //应用程序启动窗口类型，用户系统在应用程序窗口启动前显示的窗口
        public static final int TYPE_APPLICATION_STARTING = 3;
        //这是TYPE_APPLICATION的变体,确保WindowManager在APP展示之前绘制完成此窗口
        public static final int TYPE_DRAWN_APPLICATION = 4;
        ///应用程序窗口的开始值
        public static final int LAST_APPLICATION_WINDOW = 99;
}
}
```

应用程序窗口的Type值的范围为1~99。

### 1.2 子窗口

子窗口不能单独存在，需要依附于其他窗口才行，PopupWindow就属于子窗口。子窗口的类型定义如下所示：

> frameworks/base/core/java/android/view/WindowManager.java

```java
public interface WindowManager extends ViewManager {
    public static class LayoutParams extends ViewGroup.LayoutParams implements Parcelable {
       //子窗口类型初始值
       public static final int FIRST_SUB_WINDOW = 1000;
        public static final int TYPE_APPLICATION_PANEL = FIRST_SUB_WINDOW;
        public static final int TYPE_APPLICATION_MEDIA = FIRST_SUB_WINDOW + 1;
        public static final int TYPE_APPLICATION_SUB_PANEL = FIRST_SUB_WINDOW + 2;
        public static final int TYPE_APPLICATION_ATTACHED_DIALOG = FIRST_SUB_WINDOW + 3;
        public static final int TYPE_APPLICATION_MEDIA_OVERLAY  = FIRST_SUB_WINDOW + 4;
        public static final int TYPE_APPLICATION_ABOVE_SUB_PANEL = FIRST_SUB_WINDOW + 5;
        //子窗口类型结束值
        public static final int LAST_SUB_WINDOW = 1999;
}
}
```

子窗口的Type值的范围为1000~1999。

### 1.3 系统窗口

Toast、输入法窗口、系统音量条窗口、系统错误窗口都属于系统窗口。系统窗口的类型定义如下所示：

```java
public interface WindowManager extends ViewManager {
    public static class LayoutParams extends ViewGroup.LayoutParams implements Parcelable {
        //系统窗口类型初始值
        public static final int FIRST_SYSTEM_WINDOW     = 2000;
        //系统状态栏，会显示在所有用户窗口中
        public static final int TYPE_STATUS_BAR         = FIRST_SYSTEM_WINDOW; 
        //搜索条，会显示在所有用户窗口中
        public static final int TYPE_SEARCH_BAR         = FIRST_SYSTEM_WINDOW+1; 
        //通话界面，会显示在所有用户窗口中
        @Deprecated //use TYPE_APPLICATION_OVERLAY instead
        public static final int TYPE_PHONE              = FIRST_SYSTEM_WINDOW+2; 
        //系统Alert，只会显示在拥有者的窗口中
        @Deprecated //use TYPE_APPLICATION_OVERLAY instead
        public static final int TYPE_SYSTEM_ALERT       = FIRST_SYSTEM_WINDOW+3; 
       //锁屏界面，会显示在所有用户窗口中
        public static final int TYPE_KEYGUARD           = FIRST_SYSTEM_WINDOW+4;
        //Toast窗口，只会显示在拥有者的窗口中
        @Deprecated//use TYPE_APPLICATION_OVERLAY instead
        public static final int TYPE_TOAST              = FIRST_SYSTEM_WINDOW+5; 
        //系统重载窗口，只会显示在拥有者的窗口中
        @Deprecated //use TYPE_APPLICATION_OVERLAY instead
        public static final int TYPE_SYSTEM_OVERLAY     = FIRST_SYSTEM_WINDOW+6;
        //高权限通话窗口，会显示在所有用户窗口中
        @Deprecated //use TYPE_APPLICATION_OVERLAY instead
        public static final int TYPE_PRIORITY_PHONE     = FIRST_SYSTEM_WINDOW+7;
        //系统弹窗，会显示在所有用户窗口中
        public static final int TYPE_SYSTEM_DIALOG      = FIRST_SYSTEM_WINDOW+8;
        //锁屏弹窗，会显示在所有用户窗口中
        public static final int TYPE_KEYGUARD_DIALOG    = FIRST_SYSTEM_WINDOW+9;
        //系统错误，只会显示在拥有者的窗口中
        @Deprecated//use TYPE_APPLICATION_OVERLAY instead
        public static final int TYPE_SYSTEM_ERROR       = FIRST_SYSTEM_WINDOW+10;
        //输入法，只会显示在拥有者的窗口中
        public static final int TYPE_INPUT_METHOD       = FIRST_SYSTEM_WINDOW+11;
        //输入法弹窗，只会显示在拥有者的窗口中
        public static final int TYPE_INPUT_METHOD_DIALOG= FIRST_SYSTEM_WINDOW+12;
        //壁纸，只会显示在拥有者的窗口中
        public static final int TYPE_WALLPAPER          = FIRST_SYSTEM_WINDOW+13;
        //状态栏控制面板，会显示在所有用户窗口中
        public static final int TYPE_STATUS_BAR_PANEL   = FIRST_SYSTEM_WINDOW+14;
        //安全系统重载，只会显示在拥有者的窗口中
        public static final int TYPE_SECURE_SYSTEM_OVERLAY = FIRST_SYSTEM_WINDOW+15;
       //拖拽，只会显示在拥有者的窗口中
        public static final int TYPE_DRAG               = FIRST_SYSTEM_WINDOW+16;
       //状态栏子控制面板，会显示在所有用户窗口中
        public static final int TYPE_STATUS_BAR_SUB_PANEL = FIRST_SYSTEM_WINDOW+17;
        //焦点，会显示在所有用户窗口中
        public static final int TYPE_POINTER = FIRST_SYSTEM_WINDOW+18;
        //系统导航栏，会显示在所有用户窗口中
       public static final int TYPE_NAVIGATION_BAR = FIRST_SYSTEM_WINDOW+19;
        //音量，会显示在所有用户窗口中
        public static final int TYPE_VOLUME_OVERLAY = FIRST_SYSTEM_WINDOW+20;
        //Boot进度条，会显示在所有用户窗口中
        public static final int TYPE_BOOT_PROGRESS = FIRST_SYSTEM_WINDOW+21;
        //当状态栏被隐藏后的自定义输入事件，会显示在所有用户窗口中
        public static final int TYPE_INPUT_CONSUMER = FIRST_SYSTEM_WINDOW+22;
       //导航栏控制面板，会显示在所有用户窗口中
        public static final int TYPE_NAVIGATION_BAR_PANEL = FIRST_SYSTEM_WINDOW+24;
        //显示屏重载窗口，会显示在所有用户窗口中
        @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.R, trackingBug = 170729553)
        public static final int TYPE_DISPLAY_OVERLAY = FIRST_SYSTEM_WINDOW+26;
        //放大器窗口，会显示在所有用户窗口中
        public static final int TYPE_MAGNIFICATION_OVERLAY = FIRST_SYSTEM_WINDOW+27;
        //提示窗口上的私有虚拟显示
        public static final int TYPE_PRIVATE_PRESENTATION = FIRST_SYSTEM_WINDOW+30;
        //窗口的声音交互层
        public static final int TYPE_VOICE_INTERACTION = FIRST_SYSTEM_WINDOW+31;

        public static final int TYPE_ACCESSIBILITY_OVERLAY = FIRST_SYSTEM_WINDOW+32;
        //启动窗口的声音交互层
        public static final int TYPE_VOICE_INTERACTION_STARTING = FIRST_SYSTEM_WINDOW+33;
        //调整分屏模式下每个窗口大小的分割线
        public static final int TYPE_DOCK_DIVIDER = FIRST_SYSTEM_WINDOW+34;
        //快捷设置弹窗
        public static final int TYPE_QS_DIALOG = FIRST_SYSTEM_WINDOW+35;
        //截屏，会显示在所有用户窗口中
        public static final int TYPE_SCREENSHOT = FIRST_SYSTEM_WINDOW + 36;

        public static final int TYPE_PRESENTATION = FIRST_SYSTEM_WINDOW + 37;
        //很多废弃的系统弹窗类型都可以使用这个进行替换，只会显示在拥有者的窗口中
        public static final int TYPE_APPLICATION_OVERLAY = FIRST_SYSTEM_WINDOW + 38;
        //会显示在所有用户窗口中
        public static final int TYPE_ACCESSIBILITY_MAGNIFICATION_OVERLAY = FIRST_SYSTEM_WINDOW + 39;
        //会显示在所有用户窗口中
        public static final int TYPE_NOTIFICATION_SHADE = FIRST_SYSTEM_WINDOW + 40;
        //会显示在所有用户窗口中
        public static final int TYPE_STATUS_BAR_ADDITIONAL = FIRST_SYSTEM_WINDOW + 41;
        //系统窗口类型的结束值
        public static final int LAST_SYSTEM_WINDOW      = 2999;
}
}
```

系统窗口的类型值有接近40个，系统窗口的Type值范围为2000~2999。

### 1.4 窗口的显示次序

1、当一个进程向系统申请一个窗口的时候，系统会为窗口确定显示次序。为了方便窗口显示次序的管理，手机屏幕可以虚拟地用X、Y、Z轴来表示，其中Z轴垂直于屏幕，从屏幕内指向屏幕外，这样窗口的显示次序其实就是窗口在Z轴上的次序，这个次序称为Z-Oder。Type值是Z-Order排序的依据，我们知道应用程序窗口的Type值范围为1-99，子窗口为1000-1999，系统窗口为2000-2999，在一般情况下，Type值越大则Z~Order排序越靠前，窗口越靠近用户。  
不过窗口显示次序的逻辑并不仅仅依靠窗口的Type，情况是比较多的；最常见的情况，当多个窗口的Type都是Type\_APPLICATION，这时系统还需要结合具体情况来计算最终的Z-Oder。

2、决定窗口显示层级的关键代码如下所示。

> frameworks/base/services/core/java/com/android/server/policy/WindowManagerPolicy.java

```java
public interface WindowManagerPolicy extends WindowManagerPolicyConstants {

    default int getWindowLayerFromTypeLw(int type, boolean canAddInternalSystemWindow,
            boolean roundedCornerOverlay) {
        // Always put the rounded corner layer to the top most.
        if (roundedCornerOverlay && canAddInternalSystemWindow) {
            return getMaxWindowLayer();
        }
        if (type >= FIRST_APPLICATION_WINDOW && type <= LAST_APPLICATION_WINDOW) {
            return APPLICATION_LAYER;
        }

        switch (type) {
            case TYPE_WALLPAPER:
                // wallpaper is at the bottom, though the window manager may move it.
                return  1;
            case TYPE_PRESENTATION:
            case TYPE_PRIVATE_PRESENTATION:
            case TYPE_DOCK_DIVIDER:
            case TYPE_QS_DIALOG:
            case TYPE_PHONE:
                return  3;
            case TYPE_SEARCH_BAR:
            case TYPE_VOICE_INTERACTION_STARTING:
                return  4;
            case TYPE_VOICE_INTERACTION:
                // voice interaction layer is almost immediately above apps.
                return  5;
            case TYPE_INPUT_CONSUMER:
                return  6;
            case TYPE_SYSTEM_DIALOG:
                return  7;
            case TYPE_TOAST:
                // toasts and the plugged-in battery thing
                return  8;
            case TYPE_PRIORITY_PHONE:
                // SIM errors and unlock.  Not sure if this really should be in a high layer.
                return  9;
            case TYPE_SYSTEM_ALERT:
                // like the ANR / app crashed dialogs
                // Type is deprecated for non-system apps. For system apps, this type should be
                // in a higher layer than TYPE_APPLICATION_OVERLAY.
                return  canAddInternalSystemWindow ? 13 : 10;
            case TYPE_APPLICATION_OVERLAY:
                return  12;
            case TYPE_INPUT_METHOD:
                // on-screen keyboards and other such input method user interfaces go here.
                return  15;
            case TYPE_INPUT_METHOD_DIALOG:
                // on-screen keyboards and other such input method user interfaces go here.
                return  16;
            case TYPE_STATUS_BAR:
                return  17;//状态栏
            case TYPE_STATUS_BAR_ADDITIONAL:
                return  18;
            case TYPE_NOTIFICATION_SHADE:
                return  19;
            case TYPE_STATUS_BAR_SUB_PANEL:
                return  20;
            case TYPE_KEYGUARD_DIALOG:
                return  21;
            case TYPE_VOLUME_OVERLAY:
                // the on-screen volume indicator and controller shown when the user
                // changes the device volume
                return  22;
            case TYPE_SYSTEM_OVERLAY:
                // the on-screen volume indicator and controller shown when the user
                // changes the device volume
                return  canAddInternalSystemWindow ? 23 : 11;
            case TYPE_NAVIGATION_BAR:
                // the navigation bar, if available, shows atop most things
                return  24;
            case TYPE_NAVIGATION_BAR_PANEL:
                // some panels (e.g. search) need to show on top of the navigation bar
                return  25;
            case TYPE_SCREENSHOT:
                // screenshot selection layer shouldn't go above system error, but it should cover
                // navigation bars at the very least.
                return  26;
            case TYPE_SYSTEM_ERROR:
                // system-level error dialogs
                return  canAddInternalSystemWindow ? 27 : 10;
            case TYPE_MAGNIFICATION_OVERLAY:
                // used to highlight the magnified portion of a display
                return  28;
            case TYPE_DISPLAY_OVERLAY:
                // used to simulate secondary display devices
                return  29;
            case TYPE_DRAG:
                // the drag layer: input for drag-and-drop is associated with this window,
                // which sits above all other focusable windows
                return  30;
            case TYPE_ACCESSIBILITY_OVERLAY:
                // overlay put by accessibility services to intercept user interaction
                return  31;
            case TYPE_ACCESSIBILITY_MAGNIFICATION_OVERLAY:
                return 32;
            case TYPE_SECURE_SYSTEM_OVERLAY:
                return  33;
            case TYPE_BOOT_PROGRESS:
                return  34;
            case TYPE_POINTER:
                // the (mouse) pointer layer
                return  35;
            default:
                Slog.e("WindowManager", "Unknown window type: " + type);
                return 3;
        }
    }
 }

```

## 二、窗口的添加过程

### 2.1 系统窗口StatusBar的添加过程

前面我们介绍过Window的三种类型，像是应用开发最常见的Activity，它所对应的Window是应用程序类型。由于分析Activity对应的Window的添加过程还需要先分析它们所对应的Window的创建过程，这里我们简单点，先以系统状态栏窗口StatusBar为例，跟随源码梳理下一下WindowMnager是如何添加状态栏窗口的。

StatusBar是SystemUI的重要组成部分，具体指的就是系统状态栏，我们在[Android 12系统源码\_SystemUI（二）系统状态栏StatusBar的创建流程](https://blog.csdn.net/abc6368765/article/details/128092977)具体分析过它的相关源码。StatusBar主要是调用createAndAddWindows方法实现状态栏窗口的添加的。

> frameworks/base/packages/SystemUI/src/com/android/systemui/statusbar/phone/StatusBar.java

```java
public class StatusBar extends SystemUI implements ActivityStarter, LifecycleOwner {

   private final StatusBarWindowController mStatusBarWindowController;//状态栏窗口控制器
   private final StatusBarComponent.Factory mStatusBarComponentFactory;//Dagger2状态栏组件工厂
   private StatusBarComponent mStatusBarComponent;//Dagger2状态栏子组件

   protected PhoneStatusBarView mStatusBarView;//状态栏视图
   private PhoneStatusBarViewController mPhoneStatusBarViewController;//状态栏视图控制器

    public StatusBar(
		...代码省略...
		StatusBarWindowController statusBarWindowController,//状态栏控制器
        StatusBarComponent.Factory statusBarComponentFactory,//状态栏Dagger2组件工厂
		...代码省略...
	){
		...代码省略...
		//为状态栏控制器mStatusBarWindowController赋值
        mStatusBarWindowController = statusBarWindowController;
      	//为状态栏Dagger2组件工厂mStatusBarComponentFactory赋值
        mStatusBarComponentFactory = statusBarComponentFactory;
        ...代码省略...
    }   

    @Override
    public void start() {
    	...代码省略...
    	createAndAddWindows(result);
    	...代码省略...
    }

    public void createAndAddWindows(@Nullable RegisterStatusBarResult result) {
        makeStatusBarView(result);
        ...代码省略...
        mStatusBarWindowController.attach();
    }

   protected void makeStatusBarView(@Nullable RegisterStatusBarResult result) {
		...代码省略...
        inflateStatusBarWindow();
        ...代码省略...
        mStatusBarWindowController.getFragmentHostManager()
                .addTagListener(CollapsedStatusBarFragment.TAG, (tag, fragment) -> {代码省略})
                .getFragmentManager()
                .beginTransaction()
                .replace(R.id.status_bar_container,
                        mStatusBarComponent.createCollapsedStatusBarFragment(),
                        CollapsedStatusBarFragment.TAG)
                .commit();
        ...代码省略...
   	}
       
    private void inflateStatusBarWindow() {
     	mStatusBarComponent = mStatusBarComponentFactory.create();
     	...代码省略...
     }
}

```

createAndAddWindows方法首先调用makeStatusBarView构建状态栏视图，然后会调用StatusBarWindowController的attach方法，将状态栏视图添加到窗口上。  
StatusBarWindowController类和attach方法相关的代码如下所示。

> frameworks/base/packages/SystemUI/src/com/android/systemui/statusbar/phone/StatusBarWindowController.java

```java
@SysUISingleton
public class StatusBarWindowController {

    private final WindowManager mWindowManager;
    private WindowManager.LayoutParams mLp;
    @Inject
    public StatusBarWindowController(
            Context context,
            @StatusBarWindowModule.InternalWindowView StatusBarWindowView statusBarWindowView,
            WindowManager windowManager,
            IWindowManager iWindowManager,
            StatusBarContentInsetsProvider contentInsetsProvider,
            @Main Resources resources) {
		...代码省略...
        mWindowManager = windowManager;
        mStatusBarWindowView = statusBarWindowView;
        if (mBarHeight < 0) {
            mBarHeight = SystemBarUtils.getStatusBarHeight(mContext);
        }
    }
    
    public void attach() {
    	//获取状态栏类型窗口所需要的布局参数
        mLp = getBarLayoutParams(mContext.getDisplay().getRotation());
        //调用WindowManager的addView方法将状态栏窗口添加到Window中。
        mWindowManager.addView(mStatusBarWindowView, mLp);
		...代码省略...
    }
    
    private WindowManager.LayoutParams getBarLayoutParams(int rotation) {
        WindowManager.LayoutParams lp = getBarLayoutParamsForRotation(rotation);
        lp.paramsForRotation = new WindowManager.LayoutParams[4];
        for (int rot = Surface.ROTATION_0; rot <= Surface.ROTATION_270; rot++) {
            lp.paramsForRotation[rot] = getBarLayoutParamsForRotation(rot);
        }
        return lp;
    }

    private WindowManager.LayoutParams getBarLayoutParamsForRotation(int rotation) {
        int height = mBarHeight;
        if (INSETS_LAYOUT_GENERALIZATION) {
            switch (rotation) {
                case ROTATION_UNDEFINED:
                case Surface.ROTATION_0:
                case Surface.ROTATION_180:
                    height = SystemBarUtils.getStatusBarHeightForRotation(
                            mContext, Surface.ROTATION_0);
                    break;
                case Surface.ROTATION_90:
                    height = SystemBarUtils.getStatusBarHeightForRotation(
                            mContext, Surface.ROTATION_90);
                    break;
                case Surface.ROTATION_270:
                    height = SystemBarUtils.getStatusBarHeightForRotation(
                            mContext, Surface.ROTATION_270);
                    break;
            }
        }
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.MATCH_PARENT,//填充设备整个宽度
                height,//根据当前屏幕旋转角度所得到的状态栏高度
                WindowManager.LayoutParams.TYPE_STATUS_BAR,//指定窗口类型为状态栏
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_SPLIT_TOUCH
                        | WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS,
                PixelFormat.TRANSLUCENT);//窗口背景半透明
        lp.privateFlags |= PRIVATE_FLAG_COLOR_SPACE_AGNOSTIC;
        lp.token = new Binder();
        lp.gravity = Gravity.TOP;
        lp.setFitInsetsTypes(0 /* types */);
        lp.setTitle("StatusBar");
        lp.packageName = mContext.getPackageName();
        lp.layoutInDisplayCutoutMode = LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        return lp;
    }

}

```

StatusBarWindowController的attach首先调用getBarLayoutParams方法获取状态栏类型窗口所需要的布局参数，在成功获取状态栏类型窗口所需要的布局参数后，便会调用WindowManager的addView方法将状态栏视图窗口添加到Window中。

### 2.2 Actiivty窗口的添加过程

> frameworks/base/core/java/android/app/ActivityThread.java

```java
public final class ActivityThread extends ClientTransactionHandler
        implements ActivityThreadInternal {

    private void attach(boolean system, long startSeq) {
    		...代码省略...
            mInstrumentation = new Instrumentation();
            mInstrumentation.basicInit(this);
            ContextImpl context = ContextImpl.createAppContext(
                    this, getSystemContext().mPackageInfo);
            //触发应用Application的attach和onCreate方法
            mInitialApplication = context.mPackageInfo.makeApplication(true, null);
            mInitialApplication.onCreate();
            ...代码省略...
    }

    public Activity handleLaunchActivity(ActivityClientRecord r,
            PendingTransactionActions pendingActions, Intent customIntent) {
         ...代码省略...
         //调用performLaunchActivity方法
         final Activity a = performLaunchActivity(r, customIntent);   
         ...代码省略...
     }

    //启动Activity的核心方法
    private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
         ...代码省略...
         //通过反射创建Activity实例对象
         activity = mInstrumentation.newActivity(cl, component.getClassName(), r.intent);
     	 ...代码省略...
         //注释1，调用Activity的attach方法，此方法是Activity对象最早被调用的方法
         activity.attach(appContext, this, getInstrumentation(), r.token,
                r.ident, app, r.intent, r.activityInfo, title, r.parent,
                r.embeddedID, r.lastNonConfigurationInstances, config,
                r.referrer, r.voiceInteractor, window, r.configCallback,
                r.assistToken, r.shareableActivityToken);
        return activity;
    }

    public void handleResumeActivity(ActivityClientRecord r, boolean finalStateRequest,
            boolean isForward, String reason) {
            ...代码省略...
            //注释3，调用performResumeActivity方法，最终会调用Activity的onResume方法
           if (!performResumeActivity(r, finalStateRequest, reason)) {
            	return;
        	}
        	...代码省略...
            View decor = r.window.getDecorView();
            decor.setVisibility(View.INVISIBLE);
            ViewManager wm = a.getWindowManager();
            WindowManager.LayoutParams l = r.window.getAttributes();
            a.mDecor = decor;
            //注释5，设置Activity对应窗口视图的窗口类型
            l.type = WindowManager.LayoutParams.TYPE_BASE_APPLICATION;//窗口类型
           ...代码省略...
           //注释6，将Activity对应的窗口添加到WMS中
           wm.addView(decor, l);
	}
	
    public boolean performResumeActivity(ActivityClientRecord r, boolean finalStateRequest, String reason) {
         ...代码省略...     
         //调用Activity的performResume方法 
      	 r.activity.performResume(r.startsNotResumed, reason);
         ...代码省略...
	}
}
```

> frameworks/base/core/java/android/app/Activity.java

```java
public class Activity extends ContextThemeWrapper
        implements LayoutInflater.Factory2,
        Window.Callback, KeyEvent.Callback,
        OnCreateContextMenuListener, ComponentCallbacks2,
        Window.OnWindowDismissedCallback,
        AutofillManager.AutofillClient, ContentCaptureManager.ContentCaptureClient {

    private Window mWindow;
    private WindowManager mWindowManager;
  
    final void attach(Context context, ActivityThread aThread,
            Instrumentation instr, IBinder token, int ident,
            Application application, Intent intent, ActivityInfo info,
            CharSequence title, Activity parent, String id,
            NonConfigurationInstances lastNonConfigurationInstances,
            Configuration config, String referrer, IVoiceInteractor voiceInteractor,
            Window window, ActivityConfigCallback activityConfigCallback, IBinder assistToken,
            IBinder shareableActivityToken) {
		...代码省略...
		//注释2，创建Activity对应的窗口视图
        mWindow = new PhoneWindow(this, window, activityConfigCallback);
        mWindow.setWindowManager(
                (WindowManager)context.getSystemService(Context.WINDOW_SERVICE),
                mToken, mComponent.flattenToString(),
                (info.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED) != 0);
        if (mParent != null) {
            mWindow.setContainer(mParent.getWindow());
        }
        mWindowManager = mWindow.getWindowManager();
		...代码省略...
    }
    
    final void performResume(boolean followedByPause, String reason) {
    		//调用Instrumentation的callActivityOnResume方法
            mInstrumentation.callActivityOnResume(this);
    }    
```

> frameworks/base/core/java/android/app/Instrumentation.java

```java
public class Instrumentation {
    public void callActivityOnResume(Activity activity) {
        activity.mResumed = true;
        //注释4，调用Activity的onResume方法
        activity.onResume();
	}
}
```

在注释1处，也就是ActivityThread的performLaunchActivity方法中启动一个新的Activity的时候，会先创建一个Activity实例对象，然后调用该Activity的attach方法；在注释2处，Activity的attach方法中，会创建Activity对应的窗口视图，

### 2.3 Dialog窗口的添加过程

```java
        Dialog dialog = new Dialog(mContext);
        dialog.setContentView(R.layout.dialog_test);
        dialog.show();
```

```java
public class Dialog implements DialogInterface, Window.Callback,
        KeyEvent.Callback, OnCreateContextMenuListener, Window.OnWindowDismissedCallback {
    private final WindowManager mWindowManager;

    private final WindowManager mWindowManager;
    final Context mContext;
    final Window mWindow;
    
    Dialog(@UiContext @NonNull Context context, @StyleRes int themeResId,
            boolean createContextThemeWrapper) {
		...代码省略...
        mWindowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        final Window w = new PhoneWindow(mContext);
        mWindow = w;
		...代码省略...
        w.setGravity(Gravity.CENTER);
		...代码省略...
    }

    public void show() {
    	...代码省略...
        WindowManager.LayoutParams l = mWindow.getAttributes();
        mWindowManager.addView(mDecor, l);
        ...代码省略...
    }        
}
```

```java
public abstract class Window {
    private final WindowManager.LayoutParams mWindowAttributes =
        new WindowManager.LayoutParams();
        
    public final WindowManager.LayoutParams getAttributes() {
        return mWindowAttributes;
    }        
}     
//frameworks/base/core/java/android/view/WindowManager.java
public interface WindowManager extends ViewManager {
    public static class LayoutParams extends ViewGroup.LayoutParams implements Parcelable {
        public LayoutParams() {
            super(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
            type = TYPE_APPLICATION;
            format = PixelFormat.OPAQUE;
        }
    }
}   
```

### 2.4 Toast窗口的添加构成

```java
        Toast toast = new Toast(mContext);
        toast.setText("你好");
        toast.show();
```

```java
public class Toast {
    public Toast(Context context) {
        this(context, null);
    }

    public Toast(@NonNull Context context, @Nullable Looper looper) {
        mContext = context;
        mToken = new Binder();
        looper = getLooper(looper);
        mHandler = new Handler(looper);
        mCallbacks = new ArrayList<>();
        mTN = new TN(context, context.getPackageName(), mToken, mCallbacks, looper);
        // <dimen name="toast_y_offset">24dp</dimen>
        mTN.mY = context.getResources().getDimensionPixelSize(
                com.android.internal.R.dimen.toast_y_offset);
        //<integer name="config_toastDefaultGravity">0x00000051</integer>
        mTN.mGravity = context.getResources().getInteger(
                com.android.internal.R.integer.config_toastDefaultGravity);
    }
    
    public void setText(CharSequence s) {
        if (Compatibility.isChangeEnabled(CHANGE_TEXT_TOASTS_IN_THE_SYSTEM)) {
            if (mNextView != null) {
                throw new IllegalStateException(
                        "Text provided for custom toast, remove previous setView() calls if you "
                                + "want a text toast instead.");
            }
            mText = s;
        } else {
            if (mNextView == null) {
                throw new RuntimeException("This Toast was not created with Toast.makeText()");
            }
            TextView tv = mNextView.findViewById(com.android.internal.R.id.message);
            if (tv == null) {
                throw new RuntimeException("This Toast was not created with Toast.makeText()");
            }
            tv.setText(s);
        }
    }

    public void show() {
      	...代码省略...
        NotificationManager service = getService();

    }
}
```

```java
public class NotificationManagerService extends SystemService {
    private WindowManagerInternal mWindowManagerInternal;

        private void enqueueToast(String pkg, IBinder token, @Nullable CharSequence text,
                @Nullable ITransientNotification callback, int duration, int displayId,
                @Nullable ITransientNotificationCallback textCallback) {
              ...代码省略...
              //
              mWindowManagerInternal.addWindowToken(windowToken, TYPE_TOAST, displayId);
              ...代码省略...
        }
   	}
   	
}

public class WindowManagerService extends IWindowManager.Stub implements Watchdog.Monitor, WindowManagerPolicy.WindowManagerFuncs {
    private final class LocalService extends WindowManagerInternal {
        @Override
	    public void addWindowToken(IBinder binder, int type, int displayId) {
	        addWindowTokenWithOptions(binder, type, displayId, null /* options */,
	                null /* packageName */, false /* fromClientToken */);
	    }
    }
}
```

## 三、调用WindowManager的addView方法添加窗口

1、关于WindowManager的addView方法，具体是在WindowManagerImpl中实现的。

> frameworks/base/core/java/android/view/WindowManagerImpl.java

```java
public final class WindowManagerImpl implements WindowManager {

    @UnsupportedAppUsage
    private final WindowManagerGlobal mGlobal = WindowManagerGlobal.getInstance();
    
    @Override
    public void addView(@NonNull View view, @NonNull ViewGroup.LayoutParams params) {
        applyTokens(params);
        mGlobal.addView(view, params, mContext.getDisplayNoVerify(), mParentWindow,
                mContext.getUserId());
    }
    
}
```

2、WindowManagerImpl的addView又进一步调用WindowManagerGlobal的addView方法。

> frameworks/base/core/java/android/view/WindowManagerGlobal.java

```java
public final class WindowManagerGlobal {

    @UnsupportedAppUsage
    private final ArrayList<View> mViews = new ArrayList<View>();//当前存在的View列表
    @UnsupportedAppUsage
    private final ArrayList<ViewRootImpl> mRoots = new ArrayList<ViewRootImpl>();//当前存在的ViewRootImpl列表
    @UnsupportedAppUsage
    private final ArrayList<WindowManager.LayoutParams> mParams =
            new ArrayList<WindowManager.LayoutParams>();//当前存在的布局参数列表
            
    public void addView(View view, ViewGroup.LayoutParams params,
            Display display, Window parentWindow, int userId) {
        if (view == null) {
            throw new IllegalArgumentException("view must not be null");
        }
        if (display == null) {
            throw new IllegalArgumentException("display must not be null");
        }
        if (!(params instanceof WindowManager.LayoutParams)) {
            throw new IllegalArgumentException("Params must be WindowManager.LayoutParams");
        }

        final WindowManager.LayoutParams wparams = (WindowManager.LayoutParams) params;
        if (parentWindow != null) {
            parentWindow.adjustLayoutParamsForSubWindow(wparams);
        } else {
            // If there's no parent, then hardware acceleration for this view is
            // set from the application's hardware acceleration setting.
            final Context context = view.getContext();
            if (context != null
                    && (context.getApplicationInfo().flags
                            & ApplicationInfo.FLAG_HARDWARE_ACCELERATED) != 0) {
                wparams.flags |= WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED;
            }
        }

        ViewRootImpl root;
        View panelParentView = null;

        synchronized (mLock) {
            // Start watching for system property changes.
            if (mSystemPropertyUpdater == null) {
                mSystemPropertyUpdater = new Runnable() {
                    @Override public void run() {
                        synchronized (mLock) {
                            for (int i = mRoots.size() - 1; i >= 0; --i) {
                                mRoots.get(i).loadSystemProperties();
                            }
                        }
                    }
                };
                SystemProperties.addChangeCallback(mSystemPropertyUpdater);
            }

            int index = findViewLocked(view, false);
            if (index >= 0) {
                if (mDyingViews.contains(view)) {
                    // Don't wait for MSG_DIE to make it's way through root's queue.
                    mRoots.get(index).doDie();
                } else {
                    throw new IllegalStateException("View " + view
                            + " has already been added to the window manager.");
                }
                // The previous removeView() had not completed executing. Now it has.
            }

            // If this is a panel window, then find the window it is being
            // attached to for future reference.
            if (wparams.type >= WindowManager.LayoutParams.FIRST_SUB_WINDOW &&
                    wparams.type <= WindowManager.LayoutParams.LAST_SUB_WINDOW) {
                final int count = mViews.size();
                for (int i = 0; i < count; i++) {
                    if (mRoots.get(i).mWindow.asBinder() == wparams.token) {
                        panelParentView = mViews.get(i);
                    }
                }
            }

            root = new ViewRootImpl(view.getContext(), display);

            view.setLayoutParams(wparams);

            mViews.add(view);
            mRoots.add(root);
            mParams.add(wparams);

            // do this last because it fires off messages to start doing things
            try {
                root.setView(view, wparams, panelParentView, userId);
            } catch (RuntimeException e) {
                // BadTokenException or InvalidDisplayException, clean up.
                if (index >= 0) {
                    removeViewLocked(index, true);
                }
                throw e;
            }
        }
    }

}
```

1）WindowManagerGlobal中维护了和Window操作相关的3个关键列表，在窗口的添加、更新和删除过程中都会涉及这3个列表，他们分别是View列表，ViewRootImpl列表，布局参数列表。  
2）addView方法首先会对传入的参数view、params、display进行检查，另外如果parentWindow不为空，还需要父窗口根据WindowManager.LayoutParams类型的wparams参数对view进行相应调整。  
3）一切准备就绪会创建ViewRootImpl对象实例，在将view、root、mparams保存到对应的数据列表中后，便会调用ViewRootImpl的setView方法将窗口和窗口参数设置到ViewRootImpl中。  
4）ViewRootImpl肩负了很多职责，主要有以下几点：

+   View树的根并管理View树
+   触发View的测量、布局和绘制
+   输入事件的中转站
+   管理Surface
+   负责与WindowManagerService进行进程间通信

3、 了解了ViewRootImpl的职责以后，继续来着看ViewRootImpl的setView的方法。

> frameworks/base/core/java/android/view/ViewRootImpl.java

```java
public final class ViewRootImpl implements ViewParent,
        View.AttachInfo.Callbacks, ThreadedRenderer.DrawCallbacks {
    
    final IWindowSession mWindowSession;
    final W mWindow;
    final View.AttachInfo mAttachInfo;

    public ViewRootImpl(@UiContext Context context, Display display, IWindowSession session,
            boolean useSfChoreographer) {
        mContext = context;
        mWindowSession = session;
        mDisplay = display;
        mBasePackageName = context.getBasePackageName();
        mThread = Thread.currentThread();
        mLocation = new WindowLeaked(null);
        mLocation.fillInStackTrace();
        mWidth = -1;
        mHeight = -1;
        mDirty = new Rect();
        mTempRect = new Rect();
        mVisRect = new Rect();
        mWinFrame = new Rect();
        mWindow = new W(this);    
        ...代码省略...
        mAttachInfo = new View.AttachInfo(mWindowSession, mWindow, display, this, mHandler, this,
                context);
        ...代码省略...         
	｝
	
    public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView,
            int userId) {
        synchronized (this) {
              ...代码省略...
                try {
                    mOrigWindowType = mWindowAttributes.type;
                    mAttachInfo.mRecomputeGlobalAttributes = true;
                    collectViewAttributes();
                    adjustLayoutParamsForCompatibility(mWindowAttributes);
                    controlInsetsForCompatibility(mWindowAttributes);
                    res = mWindowSession.addToDisplayAsUser(mWindow, mWindowAttributes,
                            getHostVisibility(), mDisplay.getDisplayId(), userId,
                            mInsetsController.getRequestedVisibilities(), inputChannel, mTempInsets,
                            mTempControls);
                    if (mTranslator != null) {
                        mTranslator.translateInsetsStateInScreenToAppWindow(mTempInsets);
                        mTranslator.translateSourceControlsInScreenToAppWindow(mTempControls);
                    }
                } 
              ...代码省略...
    }
}
```

setView方法中有很多逻辑代码，这里只截取了最关键的一部分，调用了mWindowSession的addToDisplay方法，mWindowSession是IWindowSession类型的，它是一个Binder对象，用于进行进程间通信，IWindowSession是Client端的代理，它的Server端的实现为Session，此前的代码逻辑都是运行在本地进程的，而Session的addToDisplay方法则运行在WindowManagerService所在的进程（SystemServer）中。  
![ViewRootImpl与WindowManagerService通信](https://i-blog.csdnimg.cn/blog_migrate/9fc5c1c7612307dca97f244cdc5aceb3.png)  
从上图可以看出，本地进程的ViewRootImpl要想和WindowManagerService进行通信需要经过Session，那么Session为何包含在WindowManagerService中呢？

4、继续看Session的addToDisplay方法。

> frameworks/base/services/core/java/com/android/server/wm/Session.java

```java
class Session extends IWindowSession.Stub implements IBinder.DeathRecipient {

    final WindowManagerService mService;

    public Session(WindowManagerService service, IWindowSessionCallback callback) {
        mService = service;
        ...代码省略...
 	}
 	
    @Override
    public int addToDisplayAsUser(IWindow window, WindowManager.LayoutParams attrs,
            int viewVisibility, int displayId, int userId, InsetsVisibilities requestedVisibilities,
            InputChannel outInputChannel, InsetsState outInsetsState,
            InsetsSourceControl[] outActiveControls) {
        return mService.addWindow(this, window, attrs, viewVisibility, displayId, userId,
                requestedVisibilities, outInputChannel, outInsetsState, outActiveControls);
    }
}    
```

1）addToDisplay方法会进一步调用WindowManagerService的addWindow方法，并将自身作为参数传了进去，每个应用程序进程都会对应一个Session，WindowManagerService会用ArrayList来保存这些Session，这就是为什么WindowManagerService包含Session的原因。  
2）之后的工作就全都交给了WindowManagerService处理，WindowManagerService会为这个添加的窗口分配Surface，并确定窗口显示次序，可见负责显示界面的是画布Surface，而不是窗口本身。WindowManagerService会将它所管理的Surface交由SurfaceFlinger处理，SurfaceFlinger会将这些Surface混合并绘制到屏幕上。

5、系统状态栏窗口的添加过程涉及到的主要过程如下所示。  
![系统状态栏窗口的添加过程](https://i-blog.csdnimg.cn/blog_migrate/dae506c5d552f18c9c618aafa985ec6f.png)

## 四、调用WindowManager的updateViewLayout方法更新窗口视图属性

1、在我们成功添加窗口之后，经常需要去更新窗口的视图属性，这个时候我们可以通过WindowManager的updateViewLayout方法来实现，关于WindowManager的updateViewLayout方法，同样是在WindowManagerImpl中实现的。

> frameworks/base/core/java/android/view/WindowManagerImpl.java

```java
public final class WindowManagerImpl implements WindowManager {

    @UnsupportedAppUsage
    private final ArrayList<View> mViews = new ArrayList<View>();//当前存在的View列表
    @UnsupportedAppUsage
    private final ArrayList<ViewRootImpl> mRoots = new ArrayList<ViewRootImpl>();//当前存在的ViewRootImpl列表
    @UnsupportedAppUsage
    private final ArrayList<WindowManager.LayoutParams> mParams =
            new ArrayList<WindowManager.LayoutParams>();//当前存在的布局参数列表

    public void updateViewLayout(View view, ViewGroup.LayoutParams params) {
        if (view == null) {
            throw new IllegalArgumentException("view must not be null");
        }
        if (!(params instanceof WindowManager.LayoutParams)) {
            throw new IllegalArgumentException("Params must be WindowManager.LayoutParams");
        }

        final WindowManager.LayoutParams wparams = (WindowManager.LayoutParams)params;

        view.setLayoutParams(wparams);

        synchronized (mLock) {
            int index = findViewLocked(view, true);//获取当前view在集合mViews中对应的索引
            ViewRootImpl root = mRoots.get(index);//获取当前view在集合mRoots中对应的ViewRootImpl实例
            mParams.remove(index);				  //移除当前view在集合mParams中的布局参数
            mParams.add(index, wparams);		  //在集合mParams中添加当前view对应的新的布局参数
            root.setLayoutParams(wparams, false); //调用ViewRootImpl的setLayoutParams方法
        }
    }
    
    private int findViewLocked(View view, boolean required) {
        final int index = mViews.indexOf(view);
        if (required && index < 0) {
            throw new IllegalArgumentException("View=" + view + " not attached to window manager");
        }
        return index;
   }
    
}
```

updateViewLayout方法更新了view对应的布局参数，并获取该view对应的ViewRootImpl实例对象，然后调用该对象的setLayoutParams方法。

2、ViewRootImpl的setLayoutParams方法如下所示。

> frameworks/base/core/java/android/view/ViewRootImpl.java

```java
public final class ViewRootImpl implements ViewParent,
        View.AttachInfo.Callbacks, ThreadedRenderer.DrawCallbacks {
   
   //当前ViewRootImpl对应的窗口属性
    public final WindowManager.LayoutParams mWindowAttributes = new WindowManager.LayoutParams();
    
    @VisibleForTesting
    public void setLayoutParams(WindowManager.LayoutParams attrs, boolean newView) {
        synchronized (this) {
            final int oldInsetLeft = mWindowAttributes.surfaceInsets.left;
            final int oldInsetTop = mWindowAttributes.surfaceInsets.top;
            final int oldInsetRight = mWindowAttributes.surfaceInsets.right;
            final int oldInsetBottom = mWindowAttributes.surfaceInsets.bottom;
            final int oldSoftInputMode = mWindowAttributes.softInputMode;
            final boolean oldHasManualSurfaceInsets = mWindowAttributes.hasManualSurfaceInsets;

            if (DEBUG_KEEP_SCREEN_ON && (mClientWindowLayoutFlags
                    & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) != 0
                    && (attrs.flags&WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) == 0) {
                Slog.d(mTag, "setLayoutParams: FLAG_KEEP_SCREEN_ON from true to false!");
            }

            // Keep track of the actual window flags supplied by the client.
            mClientWindowLayoutFlags = attrs.flags;

            // Preserve compatible window flag if exists.
            final int compatibleWindowFlag = mWindowAttributes.privateFlags
                    & WindowManager.LayoutParams.PRIVATE_FLAG_COMPATIBLE_WINDOW;

            //保存系统system ui visibility属性
            final int systemUiVisibility = mWindowAttributes.systemUiVisibility;
            final int subtreeSystemUiVisibility = mWindowAttributes.subtreeSystemUiVisibility;

            //保存窗口对应的外观属性和行为属性
            final int appearance = mWindowAttributes.insetsFlags.appearance;
            final int behavior = mWindowAttributes.insetsFlags.behavior;
            final int appearanceAndBehaviorPrivateFlags = mWindowAttributes.privateFlags
                    & (PRIVATE_FLAG_APPEARANCE_CONTROLLED | PRIVATE_FLAG_BEHAVIOR_CONTROLLED);
			
			//拷贝窗口属性，如果属性没变返回0，否则返回非0
            final int changes = mWindowAttributes.copyFrom(attrs);
            if ((changes & WindowManager.LayoutParams.TRANSLUCENT_FLAGS_CHANGED) != 0) {
                //需要重新计算system ui visibility属性
                mAttachInfo.mRecomputeGlobalAttributes = true;
            }
            if ((changes & WindowManager.LayoutParams.LAYOUT_CHANGED) != 0) {
                // Request to update light center.
                mAttachInfo.mNeedsUpdateLightCenter = true;
            }
            if (mWindowAttributes.packageName == null) {
                mWindowAttributes.packageName = mBasePackageName;
            }

            //恢复前面保存的窗口属性
            mWindowAttributes.systemUiVisibility = systemUiVisibility;
            mWindowAttributes.subtreeSystemUiVisibility = subtreeSystemUiVisibility;
            mWindowAttributes.insetsFlags.appearance = appearance;
            mWindowAttributes.insetsFlags.behavior = behavior;
            mWindowAttributes.privateFlags |= compatibleWindowFlag
                    | appearanceAndBehaviorPrivateFlags
                    | WindowManager.LayoutParams.PRIVATE_FLAG_USE_BLAST;

            if (mWindowAttributes.preservePreviousSurfaceInsets) {
                // Restore old surface insets.
                mWindowAttributes.surfaceInsets.set(
                        oldInsetLeft, oldInsetTop, oldInsetRight, oldInsetBottom);
                mWindowAttributes.hasManualSurfaceInsets = oldHasManualSurfaceInsets;
            } else if (mWindowAttributes.surfaceInsets.left != oldInsetLeft
                    || mWindowAttributes.surfaceInsets.top != oldInsetTop
                    || mWindowAttributes.surfaceInsets.right != oldInsetRight
                    || mWindowAttributes.surfaceInsets.bottom != oldInsetBottom) {
                mNeedsRendererSetup = true;
            }

            applyKeepScreenOnFlag(mWindowAttributes);

			//是否是新view
            if (newView) {
                mSoftInputMode = attrs.softInputMode;
                requestLayout();
            }

            // Don't lose the mode we last auto-computed.
            if ((attrs.softInputMode & SOFT_INPUT_MASK_ADJUST)
                    == WindowManager.LayoutParams.SOFT_INPUT_ADJUST_UNSPECIFIED) {
                mWindowAttributes.softInputMode = (mWindowAttributes.softInputMode
                        & ~SOFT_INPUT_MASK_ADJUST) | (oldSoftInputMode & SOFT_INPUT_MASK_ADJUST);
            }

            if (mWindowAttributes.softInputMode != oldSoftInputMode) {
                requestFitSystemWindows();
            }
			//窗口属性发生了变化
            mWindowAttributesChanged = true;
            //调用scheduleTraversals方法
            scheduleTraversals();
        }
    }
}
```

## 💡 技术无价，赞赏随心

写文不易，如果本文帮你避开了“八小时踩坑”，或者让你直呼“学到了！”  
[欢迎扫码赞赏，让我知道这篇内容值得！](https://gitee.com/AFinalStone/RewardAndIncentive)