##        Android应用程序窗口设计之窗口的添加

Android应用程序窗口设计系列博客:

> [Android应用程序窗口设计之Window及WindowManager的创建](https://blog.csdn.net/tkwxty/article/details/109597570)  
> [Android应用程序窗口设计之setContentView布局加载的实现](https://blog.csdn.net/tkwxty/article/details/110005502)  
> [普法Android的Token前世今生以及在APP,AMS,WMS之间传递](https://blog.csdn.net/tkwxty/article/details/110563212)  
> [Android应用程序窗口设计之窗口的添加](https://blog.csdn.net/tkwxty/article/details/110524244)  
> [Android应用程序窗口设计之建立与WMS服务之间的通信过程](https://blog.csdn.net/tkwxty/article/details/111152011)  
> [Android窗口设计之Dialog、PopupWindow、系统窗口的实现](https://blog.csdn.net/tkwxty/article/details/111311014)  
> [Android应用程序建立与AMS服务之间的通信过程](https://blog.csdn.net/tkwxty/article/details/110817983)

___

本篇博客编写思路总结和关键点说明:

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/475cc976b54af436b4fe333734eb905c.png)

为了更加方便的读者阅读博客，通过导读思维图的形式将本博客的关键点列举出来，从而方便读者取舍和阅读！

___

  

### 引言

  通过前面博客[Android应用程序窗口设计之Window及WindowManager的创建](https://blog.csdn.net/tkwxty/article/details/109597570)和[Android应用程序窗口设计之setContentView布局加载的实现](https://blog.csdn.net/tkwxty/article/details/110005502)的源码分析以及结合实际开发的相关操作，我们完成了对Android应用程序窗口设计的如下积累:

-   掌握了Android应用程序窗口以及窗口管理器的创建
-   掌握了Android应用程序窗口中的布局文件是怎么被加载实现的(即Activity怎么通过setContentView加载布局 )

那么Android应用程序窗口的实现就此完了吗，当然不是！此时距离窗口展示到我们的终端界面还有很长的路要走(其中主要遗留的是Android系统是怎么对窗口进行管理和添加以及绘制等)，但是不要灰心当然也不要丧气，路是一步步走的，饭是一口口吃的不是，今天的博客我将带领读者攻破Android应用程序窗口添加机制，从而为全盘掌握Android应用程序窗口的实现又下一城！

注意：本篇的介绍是基于Android 7.xx平台为基础的，且窗口的添加是以Activity对应的窗口为例来说明的！其中涉及的代码路径如下：

```
frameworks/base/core/java/android/view/
--- WindowManager.java
 --- View.java
--- ViewManager.java
--- ViewRootImpl.java
--- Window.java
--- Display.java
--- WindowManagerImpl.java
--- WindowManager.java
--- WindowManagerGlobal.java
--- IWindowManager.aidl
--- IWindow.aidl
--- IWindowSession.aidl

frameworks/base/services/core/java/com/android/server/wm/
---WindowManagerService.java
---AppWindowToken.java
---WindowState.java
---Session.java
---WindowToken.java
```

在正式开始本篇博客Android应用程序窗口添加机制及源码分析前，这里有必要先将涉及关于Android应用程序窗口相关类以及类图图关系先放出来，以便能够方便后续的相关阅读，使读者能够更加清楚的知道这几者之间在源码中的关系。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/522a831c2fa25f3a38dcdedce3baabf2.png)

___

### 一.前期知识准备

  老话说的好，磨刀不误砍柴工！所以我们也得先磨磨刀，有几个相关知识点我们在正式开始相关的源码分析前，必须先亮出来！

> 关于下面要亮出的几个知识点，没有一定知识积累的可能一时不一定能理解，没有关系!我在这里提前印出来只是为了让读者心里有一个底，提前熟悉下，后面在源码分析中会一一讲解到的。

  

#### 1.1 Android窗口的分类

Android的窗口被大致分为三类，分别是:

-   Android应用程序窗口,这个是最常见的（拥有自己的WindowToken)譬如：Activity与Dialog
-   Android应用程序子窗口（必须依附到其他非子窗口才能存在，通常这个被依附的窗口类型Activity窗口） 例如：PopupWindow
-   Android系统窗口，其中我们最最常见的就是Toast窗口了

> 这里Dialog比较特殊，从表现上来说偏向于子窗口，必须依附到Activity才能存在，而从性质上来说，仍然是应用窗口，有自己的AppWindowToken。

  

#### 1.2 窗口管理中涉及的几个重要概念

在接下来要分析的窗口添加以及窗口的管理中，有几个非常只要的知识点有必要提前点名一下来一个眼前熟，它们分别是:

-   IWindow： APP端窗口暴露给WMS的抽象实例，同时也是WMS向APP端发送消息的Binder通道，它在APP端的实现为W
-   IWindowSession:WMS端暴露给窗口端的用于和WMS服务端通信的Binder通道
-   WindowState：WMS端窗口的令牌，与IWindow窗口一一对应，是WMS管理窗口的重要依据
-   WindowToken：是窗口的令牌，也是窗口分组的依据，在WMS端，和分组对应的数据结构是WindowToken
-   Token：是在AMS构建Activity对应的ActivityRecord时里面的IApplicationToken的实例，会在Activity创建过程中传递到AMS中，并且Token会在Activity从创建到显示的过程中会在App进程和AMS，WMS之间进行传递，关于Token可以详见博客[普法Android的Token前世今生以及在APP,AMS,WMS之间传递](https://editor.csdn.net/md?articleId=110563212)，此处很重要！

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/af55abced9c8737de86dfa05cb86d742.png)

-   IWindowSession:WMS服务用于提供给ViewRootImpl来和其进行跨Binder通信的接口

好了前面的这几个知识点，读者先心里大概有个了解即可了，通过后面的源码了解翻过头来看你就会有一种大彻大悟的感觉了。

### 二.APP应用程序端开始处理窗口的添加流程

  通过前面的博客[Android应用程序窗口设计之setContentView布局加载的实现](https://editor.csdn.net/md/?articleId=110005502)分析我们可知在将窗口布局加载OK以后，会调用wm.addView()方法添加，在该方法中的decor参数为Acitity对应的Window中的视图DecorView，wm为在创建PhoneWindow时创建的WindowManagerImpl对象(对于此处还有疑问的读者，可以回看博客)，该对象的addView方法实际最终调用到到是单例对象WindowManagerGlobal的addView方法(前文有提到),其关系如下：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3ac6257ae1d4c303721933a603e091b4.png)  
所以本章节我们会从此处继续续写前缘，接着分析！

```
//[WindowManagerImpl.java]
    public void addView(@NonNull View view, @NonNull ViewGroup.LayoutParams params) {
        android.util.SeempLog.record_vg_layout(383,params);
        applyDefaultToken(params);//设置默认Token
        //这里我们需要注意的是此时的mParentWindow指向了前面创建的Activity对应的窗口PhoneWIndow
        mGlobal.addView(view, params, mContext.getDisplay(), mParentWindow);//详见章节2.1
    }
```

  

#### 2.1 WindowManagerGlobal.addView(…)添加窗口处理

在看addView代码前，我先来看看WindowManagerGlobal对象成员变量(前面类图关系中也可以看到)，如下:

```
//[WindowManagerGlobal.java]
private static WindowManagerGlobal sDefaultWindowManager;
    private static IWindowManager sWindowManagerService;
    private static IWindowSession sWindowSession;

    private final Object mLock = new Object();

    private final ArrayList<View> mViews = new ArrayList<View>();
    private final ArrayList<ViewRootImpl> mRoots = new ArrayList<ViewRootImpl>();
    private final ArrayList<WindowManager.LayoutParams> mParams =
            new ArrayList<WindowManager.LayoutParams>();
    private final ArraySet<View> mDyingViews = new ArraySet<View>();
```

这里我们可以看到它有非常重要的三个成员变量mViews、mRoots和mParams分别是类型为View、ViewRootImpl和WindowManager.LayoutParams的数组(它们之间是什么关系呢，这个后续我们会重点分析)。此外还有一个成员变量mDyView，保存的则是已经不需要但还未被系统会收到View。

> 这里的View与LayoutParams比较好理解(这个是最最基本的Android概念)，那么这里的突然冒出的ViewRootImpl对象的作用是什么呢？我们知道WindowManagerGlobal被称作为窗口管理器，我们也知道管理者吗一般是管理并不会直接得参与行动，它通常是根据Acitity和Window的调用请求，找到合适的做事的人，而真正执行WindowManagerGlobal相关功能的就是ViewRootImpl类了，它可以说是WindowManagerGlobal最得力的干将没有之一。
> 
> 这里我们先代为介绍下ViewRootImpl的基本实战技能有如下几点(这个在后续的ViewRootImpl中会有所体现):  
> 1.完成了绘制过程。在ViewRootImpl类中，实现了perfromMeasure()、performDraw()、performLayout()等绘制相关的方法。  
> 2.与系统服务进行交互，例如与AMS，WMS，DisplayService、AudioService等进行通信，保证了Acitity相关功能等正常运转。  
> 3.处理触屏事件等分发逻辑的实现

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c781711a41c83fee4c5a352f1bf1c501.png)

好了，前面介绍了这么多是时候直接开撸源码了，翠花上源码addView如下:

```
//[WindowManagerGlobal.java]
    public void addView(View view, //此处的view指向DecorView
    ViewGroup.LayoutParams params,//这里的params为Window对应的默认WindowManager.LayoutParams实例对象mWindowAttributes
            Display display, //这里的Display具体指向表示物理显示设备有关的逻辑显示的大小(譬如尺寸分辨率)和密度的信息
            Window parentWindow) //这里的parentWindow指向了Activity对应的窗口实例对象PhoneWindow
   {
        //参数有效性的检查
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
/*
注意此时的parentWindow为不为null，
指向的是Activity对应的PhoneWindow窗口,这个在窗口的创建过程中已经有说明

根据窗口类型调整LayoutParams的相关参数

*/
if (parentWindow != null) {
            parentWindow.adjustLayoutParamsForSubWindow(wparams);
        } else {
...
        }

        ViewRootImpl root;
        View panelParentView = null;        
        synchronized (mLock) {
            if (mSystemPropertyUpdater == null) {//监听Property的变化
...
            }

/*
从mViews中查找是否已经有添加过同样的View
*/
            int index = findViewLocked(view, false);
            if (index >= 0) {
...
            }

...
/*
构建ViewRootImpl
它很重要，它很重要，它很重要，它是WindowManagerGlobal最最得力的干将没有之一
基本包揽了WindowManagerGlobal绝大部分工作
*/
            root = new ViewRootImpl(view.getContext(), display);//详见章节2.2

            view.setLayoutParams(wparams);

            mViews.add(view);//这三者之间是一一对应的情况
            mRoots.add(root);
            mParams.add(wparams);
        }

        try {
        /*
        将DecorView添加到ViewRootImpl中,最后添加到WMS中
        注意这里传递的参数
        */
            root.setView(view, wparams, panelParentView);//详见章节2.3
        } catch (RuntimeException e) {
            synchronized (mLock) {//发生异常处理情况
                final int index = findViewLocked(view, false);
                if (index >= 0) {
                    removeViewLocked(index, true);
                }
            }
            throw e;
        }
    }                 
```

addView()方法涉及的代码不是很多，但是它是简约而不简单啊，这里我们先来看看它的入参参数，如下:

| 参数类型 | 参数名称 | 参数功能和含义 |
| --- | --- | --- |
| View | view | 此处的view指向DecorView的实例对象 |
| ViewGroup.LayoutParams | params | 这里的params为Window对应的默认WindowManager.LayoutParams  
实例对象mWindowAttributes，并且它的成员变量type默认值为TYPE\_APPLICATION(这个很关键) |
| Display | display | 这里的Display具体指向表示物理显示设备有关的逻辑显示的大小(譬如尺寸分辨率)和密度的信息 |
| Window | parentWindow | 这里的parentWindow指向了Activity对应的窗口实例对象PhoneWindow |

参数分析完毕，正式开始源码的分析。可以看到这里会对入参的参数进行相关的合法性检测(这个不是重点)，当我们传入的参数parentWindow不为null时，调用它的方法adjustLayoutParamsForSubWindow()对传入的参数params进行调整，这个方法我们简单看下:

```
//[Window.java]
    void adjustLayoutParamsForSubWindow(WindowManager.LayoutParams wp) {
...
/*
判断窗口类型是不是应用程序子窗口类型
*/
if (wp.type >= WindowManager.LayoutParams.FIRST_SUB_WINDOW &&
                wp.type <= WindowManager.LayoutParams.LAST_SUB_WINDOW) {
            if (wp.token == null) {
                View decor = peekDecorView();
                if (decor != null) {
//LayoutParams的token设置为W本地Binder对象
                    wp.token = decor.getWindowToken();
                }
            }
...
        } else if (wp.type >= WindowManager.LayoutParams.FIRST_SYSTEM_WINDOW &&
                wp.type <= WindowManager.LayoutParams.LAST_SYSTEM_WINDOW) {
...
        } else {
        /*
        如果不是子窗口和系统窗口，同时当前窗口没有指定容器
        则设置token为当前窗口的mAppToken代理对象
        否则设置为指定容器的mAppToken代理对象
        此时的mAppToken是从AMS传递过来的
        */
            if (wp.token == null) {
                wp.token = mContainer == null ? mAppToken : mContainer.mAppToken;
            }
...
    }
```

还记得我们本博客一开始就重点说明了Android的窗口分为三种类型吗，上述的源码就是对不同类型窗口的布局参数进行相应的设置，比如布局参数中的token设置！这里我们先不分析WindowManager.LayoutParams的源码，对于它我们现在暂且只需知道如下三点:

-   应用程序窗口:对应的type取值范围为FIRST\_APPLICATION\_WINDOW到LAST\_APPLICATION\_WINDOW之间,譬如Activity和Dialog
    
-   子窗口:对应的type取值范围为FIRST\_SUB\_WINDOW到LAST\_SUB\_WINDOW之间，譬如PopupWindow
    
-   系统窗口:对应的type取值范围为FIRST\_SYSTEM\_WINDOW到LAST\_SYSTEM\_WINDOW之间，譬如Toast
    

好了这里让我们看看该方法中是怎么对布局参数进行调整的，它遵循了三个原则:

-   如果是应用程序子窗口类型，则设置token为W本地Binder对象(这里的W本地对象也在开始有简单介绍过了，这里先不指明后续会有分析）
-   对于系统类型窗口token没有调整
-   如果不是应用程序子窗口和系统窗口，且同时当前窗口没有指定窗口容器，则设置token为当前窗口的IApplicationToken.Proxy代理对象，否则设置为指定窗口的IApplicationToken.Proxy代理对象

> 由于我们这里是以Activity的窗口添加为例的，所以会走前面的第三条原则，设置为当前Activity对应的窗口的IApplicationToken.Proxy代理对象，而它通过前面我们博客[普法Android的Token前世今生以及在APP,AMS,WMS之间传递](https://editor.csdn.net/md?articleId=110563212)知道它的实体端是ActivityRecord持有的Token对象。

分析至此我们知道，当应用程序向窗口管理器中添加一个窗口视图对象时，首先会为该视图对象创建一个ViewRootImpl对象，并且将视图对象、ViewRootImpl对象、视图布局参数分别保存到窗口管理器WindowManagerGlobal的mViews、mRoots、mParams数组中(并且他们三者在对应的数组中的索引是一一对应的)，如下图所示：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9fcaa884b66d368bd40574f60266e406.png)  
最后通过ViewRootImpl对象来完成窗口视图的添加和显示过程！

  

#### 2.2 ViewRootImpl得力干将构建过程

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/1b95d59ebf85a517cefb022474ecede7.png#pic_center)

还记得在前面章节2.1的一开始我们就将ViewRootImpl的功能夸上了天，是时候展示下真正的实力了，我们先来看看它的构造方法如下:

```
//[ViewRootImpl.java]

final IWindowSession mWindowSession;
final ViewRootHandler mHandler = new ViewRootHandler();
// These can be accessed by any thread, must be protected with a lock.
    // Surface can never be reassigned or cleared (use Surface.clear()).
    final Surface mSurface = new Surface();
    
    public ViewRootImpl(Context context, Display display) {
        mContext = context;

/*
得到IWindowSession的代理对象，该对象用于和WMS通信
看到这里读者会不会心里有一个疑问就是为什么不是WMS的Binder代理端，而是
来了一个IWindowSession呢，咋和Activity启动过程中直接使用AMP和AMS有点不同呢
*/
        mWindowSession = WindowManagerGlobal.getWindowSession();//详见章节2.3
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
/*
创建了一个W本地Binder对象，用于WMS通知应用程序进程，
有点像ActiviytThread中的IApplicationThread实例对象

关于Activity和WMS的交互完全可以参考Activity启动过程中和AMS的交互
但是ActivityThread中的IApplicationThread是单一实例，而这里的W却是每一个
ViewRootImpl都拥有一个
*/
        mWindow = new W(this);

        mTargetSdkVersion = context.getApplicationInfo().targetSdkVersion;
        mViewVisibility = View.GONE;
        mTransparentRegion = new Region();
        mPreviousTransparentRegion = new Region();
        mFirst = true; // true for the first time the view is added
        mAdded = false;

/*
构造一个AttachInfo对象
注意传入的一个参数为Handler类型ViewRootHandler实例mHandler，
用于处理Android应用程序的窗口信息
*/
        mAttachInfo = new View.AttachInfo(mWindowSession, mWindow, display, this, mHandler, this);

        mAccessibilityManager = AccessibilityManager.getInstance(context);
        mAccessibilityInteractionConnectionManager =
            new AccessibilityInteractionConnectionManager();
        mAccessibilityManager.addAccessibilityStateChangeListener(
                mAccessibilityInteractionConnectionManager);
        mHighContrastTextManager = new HighContrastTextManager();
        mAccessibilityManager.addHighTextContrastStateChangeListener(
                mHighContrastTextManager);
        mViewConfiguration = ViewConfiguration.get(context);
        mDensity = context.getResources().getDisplayMetrics().densityDpi;
        mNoncompatDensity = context.getResources().getDisplayMetrics().noncompatDensityDpi;
        mFallbackEventHandler = new PhoneFallbackEventHandler(context);

/*
获取Choreographer实例对象mChoreographer(注意它是单例模式)，所以一个Android应用程序只会拥有一个
Choreographer将会被用于统一调度窗口绘图
江湖外号"编舞者"
*/
        mChoreographer = Choreographer.getInstance();
        mDisplayManager = (DisplayManager)context.getSystemService(Context.DISPLAY_SERVICE);
        loadSystemProperties();
    }        
```

可以看到在ViewRootImpl的构造方法以及它的方法区中创建并初始化了一些关键的成员变量(大哥也要吃饭不是，必须整几个小弟干活的)，我们来看看ViewRootImpl中关键的成员变量以及意义:

-   通过WindowManagerGlobal.getWindowSession()获取IWindowSession的代理对象，用于后续和WMS服务通信，这个后续详细掰持掰持
    
-   创建IWindow的Binder实体端类W的实例对象mWindow(注意它是一个匿名Binder类)，用于WMS通知APP端进行跨进程通信
    
-   采用单例模式创建了一个Choreographer对象，用于统一调度窗口绘图(这个本篇博客不重点关注，这个涉及到窗口绘制的内容)
    
-   创建ViewRootHandler对象，用于处理当前视图消息的处理
    
-   构造一个AttachInfo对象；
    
-   创建Surface对象(注意此时它还只是一个空壳子)，用于绘制当前视图，当然该Surface对象的真正创建是由WMS来完成的，只不过是WMS传递给应用程序进程的(这个涉及到Binder跨进程传递数据了，Binder很重要啊！)
    

如果对ViewRootImpl以及它的小弟之间的关系，错了和它成员类之间的关系还是有点迷糊的话，没有关系来个终极大杀器来张结构图，这样应该会清晰明了些！

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3c3bc91d3770c2740e8f027d7b60c06e.png)

  

#### 2.3 getWindowSession()获取和WMS通信的Binder代理端IWindowSession

在开始分析getWindowSession()的源码前，我们先来认识一下IWindowSession，正所谓知己知彼方能百战百胜吗！它是一个aidl接口类(肯定和Binder通信有关了)，Android源码对它的定义如下:

```
//[IWindowSession.aidl]
/**
中文翻译一下就是用于每个应用程序和WMS进行通信的接口
 * System private per-application interface to the window manager.
 *
 * {@hide}
 */
interface IWindowSession {
}
```

其用类图关系表示如下:  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/01cc9124fe7ccd2668aa44166f53db38.png#pic_center)  
好了，是时候开始我们真正的表演了，不真真的分析了！

```
//[WindowManagerGlobal.java]
    public static IWindowSession getWindowSession() {
        synchronized (WindowManagerGlobal.class) {
            if (sWindowSession == null) {
                try {
//获取输入法管理器服务Binder代理端
                    InputMethodManager imm = InputMethodManager.getInstance();
//获取窗口管理器服务Binder代理端
                    IWindowManager windowManager = getWindowManagerService();
//得到IWindowSession代理对象
//这里需要注意的是传递的两个参数都是匿名Binder，所以匿名Binder读者最好能掌握掌握
                    sWindowSession = windowManager.openSession(
                            new IWindowSessionCallback.Stub() {
                                @Override
                                public void onAnimatorScaleChanged(float scale) {
                                    ValueAnimator.setDurationScale(scale);
                                }
                            },
                            imm.getClient(), imm.getInputContext());
                } catch (RemoteException e) {
                    throw e.rethrowFromSystemServer();
                }
            }
            return sWindowSession;
        }
    }
```

这里需要注意地是注意这里的IWindowManager不要和WindowManager搞混淆了，IWindowManager也是一个aidl接口类，被WMS服务实现了，我们先来看看它的类图：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c29c878659e83c825931c5a184933906.png)

有了以上支持的加持，分析上述的源码就水到渠成了，以上通过WMS服务的代理端跨Binder调用到的WMS服务的openSession()方法创建应用程序与WMS之间的连接通道，即获取IWindowSession代理对象，并将该代理对象保存到WindowManagerGlobal的静态成员变量sWindowSession中，因此在整个应用程序进程中IWindowSession代理对象是唯一的！

这里我们简单来看看WMS服务是怎么处理openSession()请求的，如下:

```
//[windowManagerService.java]
    @Override
    public IWindowSession openSession(IWindowSessionCallback callback, IInputMethodClient client,
            IInputContext inputContext) {
        //异常检测
        if (client == null) throw new IllegalArgumentException("null client");
        if (inputContext == null) throw new IllegalArgumentException("null inputContext");
        //直接明了，构建一个Session实例
        Session session = new Session(this, callback, client, inputContext);
        return session;
    }
```

WMS端对openSession()的处理简单明了，直接通过传递过来的参数构建了Session的Binder实体端，然后跨Binder返回回去。至此构建了如下的一条Binder通信通道：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d04b2e0663912219a4708fde6c4effab.png)

  

#### 2.4 IWindow的Binder实体端类W的构造过程

在开始分析W的构造之间，我们先来认识一下IWindow，正所谓知己知彼方能百战百胜吗！它是一个aidl接口类(肯定和Binder通信有关了)，Android源码对它的定义如下:

```
//[IWindow.aidl]
/**
这里我用我蹩脚的英文简单翻译一下就是，WMS服务用于通知客户端窗口一些事情的通道
 * API back to a client window that the Window Manager uses to inform it of
 * interesting things happening.
 *
 * {@hide}
 */
oneway interface IWindow {
}
```

好了有了上面Android的官方解释我们就很清楚了，此处的W本地对象用于WMS通知应用程序进程，有点像ActiviytThread中的IApplicationThread实例对象用于AMS和APP进程通信的方法，都是使用匿名Binder！

> 关于Activity和WMS的交互完全可以参考Activity启动过程中和AMS的交互但是ActivityThread中的IApplicationThread是单一实例，而这里的W却是每一个ViewRootImpl都拥有一个

W的构造方法比较简单，没有什么过多看的，我们只要了解清楚了它的类图关系以及被设计的初衷即可了！

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6a0b080502dd2e145ca9a41231309181.png#pic_center)

  

#### 2.5 AttachInfo构造过程

AttachInfo的构造过程就比较简单了，我们简单过下！

```
//[View.java]
        AttachInfo(IWindowSession session, IWindow window, Display display,
                ViewRootImpl viewRootImpl, Handler handler, Callbacks effectPlayer) {
            mSession = session;//IWindowSession代理对象，用于和WMS通信
            mWindow = window;//W对象
            mWindowToken = window.asBinder();//W本地Binder对象
            mDisplay = display;
            mViewRootImpl = viewRootImpl;//ViewRootImpl实例
            mHandler = handler;//ViewRootHandler对象
            mRootCallbacks = effectPlayer;//回调实例对象
        }
```

  

#### 2.6 ViewRootImpl.setView()开始窗口视图的添加

前面将ViewRootImpl的得力小弟一一介绍完毕了，气势搞起来了，是时候来看看ViewRootImpl是怎么处理窗口视图的添加了！

```
    /**
 这里我们分析的是Activity的DecorView窗口视图添加的逻辑，所以此时不存在父视图的概念，
      不会走到这里，此时的panelParentView为null
     */
    public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
        synchronized (this) {
            if (mView == null) {
/*
初始化成员变量mView、mWindowAttraibutes
mAttachInfo是View类的一个内部类AttachInfo类的实例对象
该类的主要作用就是储存一组当View attach给它的父Window的时候Activity各种属性的信息
将DecorView保存到ViewRootImpl的成员变量mView中
*/
                mView = view;

                mAttachInfo.mDisplayState = mDisplay.getState();
                mDisplayManager.registerDisplayListener(mDisplayListener, mHandler);

                mViewLayoutDirectionInitial = mView.getRawLayoutDirection();
                mFallbackEventHandler.setView(view);
                mWindowAttributes.copyFrom(attrs);//拷贝参数
                if (mWindowAttributes.packageName == null) {
                    mWindowAttributes.packageName = mBasePackageName;
                }
                attrs = mWindowAttributes;
...

/*
我们的DecorView实现了RootViewSurfaceTaker接口，所以会进入此分支
*/
if (view instanceof RootViewSurfaceTaker) {
                    mSurfaceHolderCallback =
                            ((RootViewSurfaceTaker)view).willYouTakeTheSurface();
                    if (mSurfaceHolderCallback != null) {
                        mSurfaceHolder = new TakenSurfaceHolder();
                        mSurfaceHolder.setFormat(PixelFormat.UNKNOWN);
                    }
                }
...

/*
继续相关的初始化
*/

                mSoftInputMode = attrs.softInputMode;
                mWindowAttributesChanged = true;
                mWindowAttributesChangesFlag = WindowManager.LayoutParams.EVERYTHING_CHANGED;
                mAttachInfo.mRootView = view;
                mAttachInfo.mScalingRequired = mTranslator != null;
                mAttachInfo.mApplicationScale =
                        mTranslator == null ? 1.0f : mTranslator.applicationScale;
                if (panelParentView != null) {
                    mAttachInfo.mPanelParentWindowToken
                            = panelParentView.getApplicationWindowToken();
                }
//标记DecorView是否已经添加
                mAdded = true;
                int res; /* = WindowManagerImpl.ADD_OKAY; */

                
                /*
                 这里调用异步刷新请求，最终会调用performTraversals方法来完成View的绘制
                 在向WMS添加窗口前进行UI布局,这个本篇博客将不会重点分析，我们先重点关注窗口的添加
                */
                requestLayout();

                if ((mWindowAttributes.inputFeatures
                        & WindowManager.LayoutParams.INPUT_FEATURE_NO_INPUT_CHANNEL) == 0) {
                    mInputChannel = new InputChannel();
                }
...
                try {

...
/*
将窗口添加到WMS服务中，mWindow为W本地Binder对象，
通过Binder传输到WMS服务端后，变为IWindow代理对象
*/
                    res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
                            getHostVisibility(), mDisplay.getDisplayId(),
                            mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
                            mAttachInfo.mOutsets, mInputChannel);
                } catch (RemoteException e) {
...
                    throw new RuntimeException("Adding window failed", e);
                } finally {
                    if (restore) {
                        attrs.restore();
                    }
                }

                if (mTranslator != null) {
                    mTranslator.translateRectInScreenToAppWindow(mAttachInfo.mContentInsets);
                }
...
                if (res < WindowManagerGlobal.ADD_OKAY) {
 ...
 //检查异常情况，并抛出异常给上层处理
                }

//建立窗口消息通道
if (view instanceof RootViewSurfaceTaker) {
                    mInputQueueCallback =
                        ((RootViewSurfaceTaker)view).willYouTakeTheInputQueue();
                }

//建立输入事件通道
                if (mInputChannel != null) {
                    if (mInputQueueCallback != null) {
                        mInputQueue = new InputQueue();
                        mInputQueueCallback.onInputQueueCreated(mInputQueue);
                    }
                    mInputEventReceiver = new WindowInputEventReceiver(mInputChannel,
                            Looper.myLooper());
                }

    ...
            }
        }
    }
```

此处的代码有点多，我这里采取四舍五入的方法将一些细节处理的代码暂时省略了，在对该源码分析前我们先来回忆回忆脑补一下，我们是如何走到现在的阶段的:

-   首先我们在Activity启动的最开始阶段为它创建了窗口和窗口管理器
-   然后用户自定义的UI作为一个子View被添加到DecorView中，然后将顶级视图DecorView添加到应用程序进程的窗口管理器中，窗口管理器首先为当前添加的View创建一个ViewRootImpl对象、一个布局参数对象ViewGroup.LayoutParams，然后将这三个对象分别保存到当前应用程序进程的窗口管理器WindowManagerImpl中
-   最后就是调用ViewRootImpl的setView进行下一步的处理  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d9f678b39c05c0c7cff93e44139b84f6.png)

好了，来龙我们整清楚了，我们得看看去脉，看看ViewRootImpl是怎么通过setView方法向WMS服务添加一个窗口的:

-   首先调用ViewRootImpl内部方法requestLayout，对窗口视图中的UI进行窗口布局等操作(三件套操作，测量，布局，绘图)，这个先不放在这个博客中分析，后续专门博客来讲
    
-   然后调用前面的获取的代理端IWindowSession的addToDisplay向WMS添加一个窗口对象
    
-   最后为ViewRootImpl注册一些列的窗口和输入事件通道，用于处理一系列事件
    

  

#### 2.7 APP应用程序端开始处理窗口的添加流程小结

> 注意:上述源码分析我将关于Android应用程序窗口UI布局绘制的流程没有分析，因为那个牵涉的东西非常多，不是一下子能分析清楚的，总之UI布局三个老套路测量，布局，绘图！上述关于窗口UI布局牵涉的知识点也比较多，我们会在后续的专门的博客中进行分析。我们本篇博客的主体是将Android应用程序窗口的添加流程分析清楚！

至此APP应用程序端处理窗口添加的流程就到此结束了，我们先不乘胜追击先来复盘一下我们前面的整体流程：

-   首先我们在Activity启动的最开始阶段为它创建了窗口和窗口管理器
-   然后用户自定义的UI作为一个子View被添加到DecorView中，然后将顶级视图DecorView添加到应用程序进程的窗口管理器中，窗口管理器首先为当前添加的View创建一个ViewRootImpl对象、一个布局参数对象ViewGroup.LayoutParams，然后将这三个对象分别保存到当前应用程序进程的窗口管理器WindowManagerImpl中-
-   最后就是通过ViewRootImpl对象构造过程中和WIMS建立的IWindowSession匿名Binder通道将当前视图对象注册到WMS服务中

在前面2.6章节中已经有放出了简单的流程图，这里我就先伪代码来简单描述下:

```
WindowManagerImpl.addView(...)--->
WindowManagerGlobal.addView(...)--->
parentWindow.adjustLayoutParamsForSubWindow(...)--->
new ViewRootImpl(...)--->
WindowManagerGlobal.getWindowSession()--->
new W(this)--->
Choreographer.getInstance()--->
mViews.add(view)--->
    mRoots.add(root)--->
    mParams.add(wparams)--->
ViewRootImpl.setView(...)--->
mWindowSession.addToDisplay(...)--->
```

### 三.WMS端开始处理窗口的添加流程

不容易啊，窗口的添加我们Android应用程序端已经处理完毕，现在要正式开始进入WMS服务端进行处理了，激不激动，期不期待。废话不多说直接开始干了。

  

#### 3.1 Session.addToDisplay()处理窗口添加

还记得大明湖畔的夏雨荷吗！错了，还记得前面章节的我们的调用了mWindowSession.addToDisplay向WMS发起了添加窗口的请求，此时IWindowSession跨进程Binder的能力是时候体现出来了。我们源码展开分析一下:

```
//[Sesstion.java'
    @Override
    public int addToDisplay(IWindow window, int seq, WindowManager.LayoutParams attrs,
            int viewVisibility, int displayId, Rect outContentInsets, Rect outStableInsets,
            Rect outOutsets, InputChannel outInputChannel) {
        //注意这里的mService是在前面WMS调用openSession方法构造Session时传递过来的引用
        return mService.addWindow(this, window, seq, attrs, viewVisibility, displayId,
                outContentInsets, outStableInsets, outOutsets, outInputChannel);//详见3.2
    }
```

好吗，其实Session就是一个中间人而已(掮客，不需要留下买路钱的那种)，APP应用程序段通过它的跨进程Binder能力发送来的请求，它想都不想直接传给WMS服务进行处理了。我是干饭人！

  

#### 3.2 WMS开始真正开始窗口添加

前面的所有所有，都是为了此时的荣誉时刻！我们来看看窗口的真正管理者WMS是怎么处理窗口添加流程的(前方高能，读者最好能先活动活动脑路，否则在接下来的源码分析中很难理清楚某些知识点，牛逼的人不算啊！)。但是在这之前WMS的几个成员变量我们有必要提前重点点名下:

```
//[WindowManagerService.java]
    /**
     * All currently active sessions with clients.
     */
    final ArraySet<Session> mSessions = new ArraySet<>();

    /**
     * Mapping from an IWindow IBinder to the server's Window object.
     * This is also used as the lock for all of our state.
     * NOTE: Never call into methods that lock ActivityManagerService while holding this object.
     */
    final HashMap<IBinder, WindowState> mWindowMap = new HashMap<>();

    /**
     * Mapping from a token IBinder to a WindowToken object.
     */
    final HashMap<IBinder, WindowToken> mTokenMap = new HashMap<>();
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/be57ac6d6c250aa1b447e3aba5ec7997.png#pic_center)  
好了，我们对前面的两个哈希列表先有个了解，我们通过源码来分析深入它二者的设计意图:

```
//[WindowManagerService.java]
    public int addWindow(Session session, IWindow client, int seq,
            WindowManager.LayoutParams attrs, int viewVisibility, int displayId,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            InputChannel outInputChannel) {

//这里的client为IWindow的代理对象，用于WMS和Activity进行通信

        ...
        WindowState attachedWindow = null;
...

        synchronized(mWindowMap) {


//判断我们添加的窗口是否已经存在
            if (mWindowMap.containsKey(client.asBinder())) {
                Slog.w(TAG_WM, "Window " + client + " is already added");
                return WindowManagerGlobal.ADD_DUPLICATE_ADD;
            }

//判断添加的窗口类型是否为子窗口
            if (type >= FIRST_SUB_WINDOW && type <= LAST_SUB_WINDOW) {

/*   
根据attrs.token从mWindowMap中取出应用程序窗口在WMS服务中的描述符WindowState
这里的前提是父窗口必须在WMS中已经被添加了
*/

attachedWindow = windowForClientLocked(null, attrs.token, false);
                if (attachedWindow == null) {
                    return WindowManagerGlobal.ADD_BAD_SUBWINDOW_TOKEN;
                }
                if (attachedWindow.mAttrs.type >= FIRST_SUB_WINDOW
                        && attachedWindow.mAttrs.type <= LAST_SUB_WINDOW) {
                    return WindowManagerGlobal.ADD_BAD_SUBWINDOW_TOKEN;
                }
            }

            if (type == TYPE_PRIVATE_PRESENTATION && !displayContent.isPrivate()) {
                Slog.w(TAG_WM, "Attempted to add private presentation window to a non-private display.  Aborting.");
                return WindowManagerGlobal.ADD_PERMISSION_DENIED;
            }

            boolean addToken = false;
//根据attrs.token从mTokenMap中取出应用程序窗口在WMS服务中的描述符WindowToken
            WindowToken token = mTokenMap.get(attrs.token);
            AppWindowToken atoken = null;
            boolean addToastWindowRequiresToken = false;

            if (token == null) {//第一次add的情况下token怎么会有值呢，为什么呢
                if (type >= FIRST_APPLICATION_WINDOW && type <= LAST_APPLICATION_WINDOW) {
                    Slog.w(TAG_WM, "Attempted to add application window with unknown token "
                          + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
                if (type == TYPE_INPUT_METHOD) {
                    Slog.w(TAG_WM, "Attempted to add input method window with unknown token "
                          + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
                if (type == TYPE_VOICE_INTERACTION) {
                    Slog.w(TAG_WM, "Attempted to add voice interaction window with unknown token "
                          + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
                if (type == TYPE_WALLPAPER) {
                    Slog.w(TAG_WM, "Attempted to add wallpaper window with unknown token "
                          + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
                if (type == TYPE_DREAM) {
                    Slog.w(TAG_WM, "Attempted to add Dream window with unknown token "
                          + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
                if (type == TYPE_QS_DIALOG) {
                    Slog.w(TAG_WM, "Attempted to add QS dialog window with unknown token "
                          + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
                if (type == TYPE_ACCESSIBILITY_OVERLAY) {
                    Slog.w(TAG_WM, "Attempted to add Accessibility overlay window with unknown token "
                            + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
                if (type == TYPE_TOAST) {
                    if (doesAddToastWindowRequireToken(attrs.packageName, callingUid,
                            attachedWindow)) {
                        Slog.w(TAG_WM, "Attempted to add a toast window with unknown token "
                                + attrs.token + ".  Aborting.");
                        return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                    }
                }
                //此处什么逻辑会走到这个地方呢
                token = new WindowToken(this, attrs.token, -1, false);
                addToken = true;
            } 
//处理应用程序窗口逻辑
else if (type >= FIRST_APPLICATION_WINDOW && type <= LAST_APPLICATION_WINDOW) {
                atoken = token.appWindowToken;
                if (atoken == null) {
                    Slog.w(TAG_WM, "Attempted to add window with non-application token "
                          + token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_NOT_APP_TOKEN;
                } else if (atoken.removed) {
                    Slog.w(TAG_WM, "Attempted to add window with exiting application token "
                          + token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_APP_EXITING;
                }
                if (type == TYPE_APPLICATION_STARTING && atoken.firstWindowDrawn) {
                    // No need for this guy!
                    if (DEBUG_STARTING_WINDOW || localLOGV) Slog.v(
                            TAG_WM, "**** NO NEED TO START: " + attrs.getTitle());
                    return WindowManagerGlobal.ADD_STARTING_NOT_NEEDED;
                }
            } //输入法窗口
else if (type == TYPE_INPUT_METHOD) {
                if (token.windowType != TYPE_INPUT_METHOD) {
                    Slog.w(TAG_WM, "Attempted to add input method window with bad token "
                            + attrs.token + ".  Aborting.");
                      return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (type == TYPE_VOICE_INTERACTION) {
                if (token.windowType != TYPE_VOICE_INTERACTION) {
                    Slog.w(TAG_WM, "Attempted to add voice interaction window with bad token "
                            + attrs.token + ".  Aborting.");
                      return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            }//壁纸窗口
else if (type == TYPE_WALLPAPER) {
                if (token.windowType != TYPE_WALLPAPER) {
                    Slog.w(TAG_WM, "Attempted to add wallpaper window with bad token "
                            + attrs.token + ".  Aborting.");
                      return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            }//Dream窗口 
else if (type == TYPE_DREAM) {
                if (token.windowType != TYPE_DREAM) {
                    Slog.w(TAG_WM, "Attempted to add Dream window with bad token "
                            + attrs.token + ".  Aborting.");
                      return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (type == TYPE_ACCESSIBILITY_OVERLAY) {
                if (token.windowType != TYPE_ACCESSIBILITY_OVERLAY) {
                    Slog.w(TAG_WM, "Attempted to add Accessibility overlay window with bad token "
                            + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (type == TYPE_TOAST) {
                addToastWindowRequiresToken = doesAddToastWindowRequireToken(attrs.packageName,
                        callingUid, attachedWindow);
                if (addToastWindowRequiresToken && token.windowType != TYPE_TOAST) {
                    Slog.w(TAG_WM, "Attempted to add a toast window with bad token "
                            + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (type == TYPE_QS_DIALOG) {
                if (token.windowType != TYPE_QS_DIALOG) {
                    Slog.w(TAG_WM, "Attempted to add QS dialog window with bad token "
                            + attrs.token + ".  Aborting.");
                    return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                }
            } else if (token.appWindowToken != null) {//系统窗口
                Slog.w(TAG_WM, "Non-null appWindowToken for system window of type=" + type);
                // It is not valid to use an app token with other system types; we will
                // instead make a new token for it (as if null had been passed in for the token).
                attrs.token = null;
                token = new WindowToken(this, null, -1, false);
                addToken = true;
            }

//为Activity窗口(注意这里只是因为分析的是Activity窗口的添加而已)创建WindowState对象
            WindowState win = new WindowState(this, session, client, token,
                    attachedWindow, appOp[0], seq, attrs, viewVisibility, displayContent);
 
            res = WindowManagerGlobal.ADD_OKAY;

            if (excludeWindowTypeFromTapOutTask(type)) {
                displayContent.mTapExcludedWindows.add(win);
            }

            origId = Binder.clearCallingIdentity();

//以键值对<IWindow.Proxy/Token,WindowToken>形式保存到mTokenMap表中
//在Activity的窗口添加过程中，不会走到此处，因为在Activity启动中在创建的时候已经有添加了，这个在Token传递的篇章里面已经有分析过了
            if (addToken) {
                mTokenMap.put(attrs.token, token);
            }
            win.attach();//详见章节3.2
//以键值对<IWindow的代理对象,WindowState>形式保存到mWindowMap表中
            mWindowMap.put(client.asBinder(), win);

...

            boolean imMayMove = true;

            if (type == TYPE_INPUT_METHOD) {
...
            } else if (type == TYPE_INPUT_METHOD_DIALOG) {
...
                addWindowToListInOrderLocked(win, true);
            } else {
                addWindowToListInOrderLocked(win, true);//将WindowState添加到关联的AppWindowToken中,详见章节3.3
...
            }
...
        return res;
    }
```

> 在我们正式开始上述的源码解析前，读者一定要读Token要有所了解，如果还不是很清楚强烈建议参见博客[普法Android的Token前世今生以及在APP,AMS,WMS之间传递](https://editor.csdn.net/md?articleId=110563212)。

老规矩，我们先来看看该方法入参的几个重要参数的含义，如下：

| 参数类型 | 参数名称 | 参数功能和含义 |
| --- | --- | --- |
| Session | session | 为前面创建的Sesseion对象实例(Binder代理端经过跨进程传输后转变成Binder实体端) |
| WindowManager.LayoutParams attrs | attrs | 这里的attrs为Window对应的默认WindowManager.LayoutParams  
实例对象mWindowAttributes经过一系列赋值之后的对象，  
其中它的最最重要的两个参数为token和type |
| IWindow | client | 这里的client为IWindow的代理对象，用于WMS和Activity进行通信 |

前面的参数我们简单的总结了一下，这个时候得开始真正的表演了(下面的陈述可能有点拗口，不过没有关系多理解多分析就好了，我们一定要参数理解清楚）！

-   从我们前面的分析可知知道当应用程序进程添加一个DecorView到窗口管理器时，会为当前添加的窗口创建ViewRootImpl对象，同时构造了一个W本地Binder对象，无论是窗口视图对象DecorView还是ViewRootImpl对象，都只是存在于应用程序进程中，在添加窗口过程中仅仅将该窗口的W对象传递给WMS服务，经过Binder传输后，到达WMS服务端进程后变为IWindow.Proxy代理对象，因此该函数的参数client的类型为IWindow.Proxy，这个就是我们client参数的终极由来
    
-   参数attrs的类型为WindowManager.LayoutParams，这里的attrs为Window对应的默认WindowManager.LayoutParams实例对象mWindowAttributes经过一系列赋值之后的对象，其中它的最最重要的两个参数为token和type，由于WindowManager.LayoutParams实现了Parcelable接口，因此WindowManager.LayoutParams对象可以跨进程传输，WMS服务的addWindow函数中的attrs参数就是应用程序进程发送过来的窗口布局参数。
    
-   在WindowManagerGlobal的addView中我们调用了Window方法的adjustLayoutParamsForSubWindow()方法为窗口布局参数设置了相应的token，如果是应用程序窗口，则布局参数的token设为Token传递到APP端的代理对象。如果是应用程序子窗口则设置为token设为W本地Binder对象，由于应用程序和WMS分属于两个不同的进程空间，因此经过Binder传输后，布局参数的令牌attrs.token就转变为IWindow.Proxy或者Token。
    

> 注意这里我们是以Android应用程序窗口为说明的

关于参数的入参的分析我真的尽力了，如果读者还是没有弄清楚的话，我只能说臣妾做不到啊！了解了参数的含义，该方法也就比较好理解了，其处理的流程如下:

-   首先判断窗口是否添加过以及是否合法，如果已经添加则直接返回
    
-   然后根据attrs.token从mWindowMap中取出应用程序窗口在WMS服务中的描述符WindowToken
    
-   我们将上述得到的参数聚合在一起然后在构造一个WindowState对象，并将添加的窗口信息记录到mTokenMap和mWindowMap哈希表中  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6eeaac6dbf64b5898bf2b9a52d22bc06.png)
    
-   接着调用WindowState的attach()来进一步完成窗口添加
    
-   最后将上述的WindowState添加到关联的WindowToken中的列表中，这个地方需要重点分析下
    

  

#### 3.3 WindowState继续处理窗口的添加

在正式开始WindowState继续处理窗口的添加前，我们先来看看WindowState的构造方法如下:

```
//[WindowState.java]
    WindowState(WindowManagerService service, Session s, IWindow c, WindowToken token,
           WindowState attachedWindow, int appOp, int seq, WindowManager.LayoutParams a,
           int viewVisibility, final DisplayContent displayContent) {
        mService = service;//WMS的引用
        mSession = s;//和APP端对应的Session端
        mClient = c;//APP端W对应的Binder代理端
        mAppOp = appOp;
        mToken = token;//此处的Token为AppWindowToken，Token从AMS传递到WMS过程中创建的

...
WindowState appWin = this;
        while (appWin.isChildWindow()) {
            appWin = appWin.mAttachedWindow;
        }
        WindowToken appToken = appWin.mToken;
        mRootToken = appToken;
        mAppToken = appToken.appWindowToken;
        ...
   }
```

它的构造方法主要是将传入的初始化成员变量！我们接着继续分析它的attach方法逻辑

```
//[WindowState.java]
    void attach() {
        if (WindowManagerService.localLOGV) Slog.v(
            TAG, "Attaching " + this + " token=" + mToken
            + ", list=" + mToken.windows);
        mSession.windowAddedLocked();
    }
```

无话，接着往下看

```
//[WindowState.java]
    void windowAddedLocked() {
        if (mSurfaceSession == null) {
            if (WindowManagerService.localLOGV) Slog.v(
                TAG_WM, "First window added to " + this + ", creating SurfaceSession");
            mSurfaceSession = new SurfaceSession();
            if (SHOW_TRANSACTIONS) Slog.i(
                    TAG_WM, "  NEW SURFACE SESSION " + mSurfaceSession);
            mService.mSessions.add(this);
            if (mLastReportedAnimatorScale != mService.getCurrentAnimatorScale()) {
                mService.dispatchNewAnimatorScaleLocked(this);
            }
        }
        mNumWindow++;//记录对应的某个应用程序添加的窗口数量
    }
```

  

#### 3.4 WMS调用addWindowToListInOrderLocked方法处理WindowState

```
//[WindowManagerService.java]
    private void addWindowToListInOrderLocked(final WindowState win, boolean addToToken) {
        if (DEBUG_FOCUS) Slog.d(TAG_WM, "addWindowToListInOrderLocked: win=" + win +
                " Callers=" + Debug.getCallers(4));
        if (win.mAttachedWindow == null) {
            final WindowToken token = win.mToken;
            int tokenWindowsPos = 0;
            if (token.appWindowToken != null) {
                tokenWindowsPos = addAppWindowToListLocked(win);
            } else {
                addFreeWindowToListLocked(win);
            }
            if (addToToken) {
                if (DEBUG_ADD_REMOVE) Slog.v(TAG_WM, "Adding " + win + " to " + token);
                token.windows.add(tokenWindowsPos, win);
            }
        } else {
            addAttachedWindowToListLocked(win, addToToken);
        }

        final AppWindowToken appToken = win.mAppToken;
        if (appToken != null) {
            if (addToToken) {
                appToken.addWindow(win);//AppWindowToken进行管理
            }
        }
    }
```

在创建完窗口对应的WindowState之后，会接着addWindowToListInOrderLocked将WindowState放入AppWindowToken进行管理。(这个管理方式有点类似AMS中对Activity的堆栈管理)。经过上述处理以后AppWindowToken和WindowState有如下的对应关系。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c29ef8cf72fa5666f95176e4c7444106.png)  
对于上述关系我们也可以通过命令dumpsys来证明:

```
XXX:/ # dumpsys window displays
WINDOW MANAGER DISPLAY CONTENTS (dumpsys window displays)
  Display: mDisplayId=0
    init=720x1280 320dpi cur=720x1280 app=720x1184 rng=720x672-1184x1136
    deferred=false layoutNeeded=false

  Application tokens in top down Z order:
    mStackId=1
    mDeferDetach=false
    mFullscreen=true
    mBounds=[0,0][720,1280]
      taskId=120
        mFullscreen=true
        mBounds=[0,0][720,1280]
        mdr=false
        appTokens=[AppWindowToken{7e5b066 token=Token{977da8 ActivityRecord{6fd71cb u0 com.example.window/.MainActivity t120}}}]
        mTempInsetBounds=[0,0][0,0]
          Activity #0 AppWindowToken{7e5b066 token=Token{977da8 ActivityRecord{6fd71cb u0 com.example.window/.MainActivity t120}}}
          windows=[Window{9eb8f33 u0 com.example.window/com.example.window.MainActivity}, Window{cbaaa6d u0 hello}]
          windowType=2 hidden=false hasVisible=true
          app=true voiceInteraction=false
          allAppWindows=[Window{cbaaa6d u0 hello}, Window{9eb8f33 u0 com.example.window/com.example.window.MainActivity}]
          task={taskId=120 appTokens=[AppWindowToken{7e5b066 token=Token{977da8 ActivityRecord{6fd71cb u0 com.example.window/.MainActivity t120}}}] mdr=false}
           appFullscreen=true requestedOrientation=-1
          hiddenRequested=false clientHidden=false reportedDrawn=true reportedVisible=true
          numInterestingWindows=2 numDrawnWindows=2 inPendingTransaction=false allDrawn=true (animator=true)
          startingData=null removed=false firstWindowDrawn=true mIsExiting=false
```

___

### 四.小结

  至此Android应用程序窗口设计之窗口的添加就到此告一段落了，这里的告一段落也是对于博主来说而已，对于读者来说还有很长的路要走，因为窗口添加的概念中涉及了太多的知识点和概念了，那么怎么把这些抽象的概念具体化呢，其实Android为我们提供了不错的命令工具，dumpsys window，这个命令能够很好的打印出我们窗口前面所说的一些概念的信息，这个命令值得大家去使用，我们简单看下:

```
WINDOW MANAGER LAST ANR (dumpsys window lastanr)
  <no ANR has occurred since boot>
  
WINDOW MANAGER POLICY STATE (dumpsys window policy)
    mSafeMode=false mSystemReady=true mSystemBooted=true

WINDOW MANAGER ANIMATOR STATE (dumpsys window animator)
    DisplayContentsAnimator #0:
      Window #0: WindowStateAnimator{2fa13aa com.android.systemui.ImageWallpaper}

WINDOW MANAGER SESSIONS (dumpsys window sessions)
  Session Session{7bf2bd 5690:u0a10023}:

WINDOW MANAGER DISPLAY CONTENTS (dumpsys window displays)
  Display: mDisplayId=0
    init=720x1280 320dpi cur=720x1280 app=720x1184 rng=720x672-1184x1136
    deferred=false layoutNeeded=false


WINDOW MANAGER TOKENS (dumpsys window tokens)
  All tokens:
  AppWindowToken{4513043 token=Token{65e80fd ActivityRecord{cfdc954 u0 com.android.launcher3/.Launcher t118}}}

WINDOW MANAGER WINDOWS (dumpsys window windows)
  Window #9 Window{4b8f2da u0 NavigationBar}:
    mDisplayId=0 stackId=0 mSession=Session{abf850b 4764:u0a10014} mClient=android.os.BinderProxy@be79485
```

好了这里限于篇幅就不一一展开了，总之经过前面的一系列分析我们应该知道了:

-   应用程序端的每个窗口对应的ViewRootImpl对持有一个W对象，而WMS端持有它的代理端用于和APP的信息回调
-   然后每个应用程序进程都持有一个与WMS服务会话通道IWindowSession，系统中创建的所有IWindowSession都被记录到WMS服务的mSessions成员变量中，这样WMS就可以知道自己正在处理那些应用程序的请求。到此我们来梳理一下在WMS服务端都创建了那些对象：
-   然后为每个窗口在我们是WMS端创建一个WindowState对象，该对象是应用程序窗口在WMS服务端的描述符，有点类似于Activity在AMS端的描述符ActivityRecord

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/7d51498d3619e1cbfa9dcd446f481e52.png)

___

### 四.写在最后

  到这里本篇博客[Android应用程序窗口设计之窗口的添加](https://editor.csdn.net/md?articleId=110524244)真的就宣告结束了，相信分析至此读者应该对Android应用程序窗口的添加有了一定的理解了，但是想要说深入那还很远，很远。但是没有关系万事开头难吗，只有我们有信心放肆的干，我想关于Android应用程序窗口设计的整体流程一定会被我们攻破的。在接下来的博客中我们会接着分析Android系统窗口和子窗口的添加流程，欢迎读者继续关注！好了，青山不改绿水长流先到这里了。如果本博客对你有所帮助，麻烦关注或者点个赞，如果觉得很烂也可以踩一脚！谢谢各位了！同时如果读者对Android系统中其它窗口实现感兴趣的也可以继续阅读[Android窗口设计之Dialog、PopupWindow、系统窗口的实现](https://editor.csdn.net/md?articleId=111311014),欢迎大家多多阅读和关注！