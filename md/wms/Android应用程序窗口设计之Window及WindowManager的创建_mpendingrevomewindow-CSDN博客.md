##  Android应用程序窗口设计之Window及WindowManager的创建

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
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9e871cc84ce7a24de91dd30dda27bf9c.png)

为了更加方便的读者阅读博客，通过导读思维图的形式将本博客的关键点列举出来，从而方便读者取舍和阅读！

___

  

### 引言

  经过前面系列博客[Android四大组件之Activity启动流程源码实现详解](https://blog.csdn.net/tkwxty/article/details/108652250)的艰苦卓越的奋斗的成果，我们终于将Activity的启动流程分析得妥妥当当了。在前面的博客中完成了Activity的创建以及相关它的生命周期的调度，但是上述的相关的分析的重心是Activity的启动流程却并没有涉及到Activity的核心用途作为展示窗口的实现原理和流程，我们知道Activity当初被谷歌的设计的初衷就是作为Android的颜值担当呈现出各种缤纷多彩的UI界面，而上述的一切都离不开Activity对各种布局的加载和显示了(这个站在Android的设计者的角度出发就是Android应用程序窗口的设计了)，这不就给安排上了，在今天的博客以及后续的博客中将会分析Android应用程序窗口设计的实现，而我们知道Android应用程序窗口的相关载体就是我们的Activity了，所以本篇即以后的相关篇章中涉及到的Android应用程序的窗口实现我们会以Activity为例来说，而Activity相关的窗口概念中设计的最最重要的就是Activity的布局加载和绘制等相关流程了。

而大家也知道在Android体系中Activity扮演了一个界面展示的角色，这也是它与Android中另外一个很重要的组件Service最大的不同，那么大家有没有过如下的相关思考呢：

-   Activity是如何控制界面的展示呢，是一人操持整个流程，亦或者是各个服务通力协作?
-   界面的布局文件是如何加载并被管理的呢，是被Activity直接管理，亦或者是有第三方协助?
-   Android中的View/Window/WindowManager是一个怎样的概念,它和Activity是怎么关联起来的?
-   被加载好的布局文件是怎么显示出来的?

要回答上述疑问，是没有办法一蹴而就的，只能说是路漫漫而修远吾将上下而求索！但是有一点我们可以确认的就是上述疑问归根究底就是Android是怎么处理Activity中界面加载与绘制流程的(往大了，也可说是Android是怎么处理界面布局加载和绘制)！我们可以先对上述问题逐个分解，一一突破，而我们这里先分析Activity中界面布局的加载流程，在分析的过程中对于上述问题的答案就会跃然纸上了！

**注意**：本篇的介绍是基于Android 7.xx平台为基础的，其中涉及的代码路径如下：

```
frameworks/base/core/java/com/android/internal/policy/
--- DecorView.java
--- PhoneWindow.java

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

frameworks/base/services/core/java/com/android/server/pm/
 --- PackageManagerService.java

frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java
 
frameworks/base/core/java/android/content/pm/
--- ActivityInfo.java

frameworks/base/core/java/android/app/
  --- IActivityManager.java
  --- ActivityManagerNative.java (内部包含AMP)
  --- ActivityManager.java
  --- AppGlobals.java
  --- Activity.java
  --- ActivityThread.java(内含AT)
  --- LoadedApk.java  
  --- AppGlobals.java
  --- Application.java
  --- Instrumentation.java
  
  --- IApplicationThread.java
  --- ApplicationThreadNative.java (内部包含ATP)
  --- ActivityThread.java (内含ApplicationThread)
  --- ContextImpl.java
  --- SystemServiceRegistry.java
```

且在后续的源码分析过程中为了简述方便，会将做如下简述:

-   WindowManagerService简称为WMS
-   ActivityManagerService简称为AMS

在正式开始本篇博客Activity中界面布局的加载流程分析之前，这里有必要先将Activity、View、Window、WindowManager之间的类图关系先放出来，以便能够方便后续的相关阅读，使读者能够更加清楚的知道这几者之间在源码中的关系。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/945ef567db7a96bcd641835805bc7893.png#pic_center)

> 关于上述几个之间的关联，恩怨情仇我们有必要提前捋一捋。  
> 我们知道通常Android应用程序里所有的界面展示都来自于Activity，但是Activity的力量是薄弱的并不来独自完成整个界面的展示，这就需要Activity、View、Window、WindowManager的各司其职，更加需要它们之间的通力合作来完成这一历史重任，向用户展示各种绚丽多样的界面了。  
>   
> Activity:它应该是我们开发者最熟悉的一个组件了，对于开发者来说所有的界面展示都来自于Activity，但是对于用户来说他们不管你是啥(在用户眼里只有界面和交互的概念)，所以我个人认为Activity当初被设计只是用来给开发者来承载各种界面布局，有点像幕后英雄或者是总设计师的感觉(因为是以它的意愿为基础加载各种布局来呈现给我们的用户)。Window:在界面加载和布局中，Actiivty是一个比较抽象的概念，它是Android为了方便我们开发者开发而提出的，其实Activity对界面布局的管理是都是通过Window对象来实现的，Window对象，顾名思义就是一个窗口对象，而Activity从用户角度就是一个个的窗口实例，因此不难想象每个Activity中都对应着一个Window对象，而这个Window对象就是负责加载界面的。View:我们知道Android的每个应用中的每个界面基本都是不一样，即window对象给我们展示的都是不同的界面，那么是如何做到这一点的呢，这就需要我们的各种View组件上场了，通过各种View组件的排列组合搭积木般的实现了千人千面的的界面展示效果。WindowManager:通过上述三者之间的通力协作终于完成了我们布局文件的加载了，但是加载完成这还远远远不够，因为我们最终的目标是呈现给用户，这就轮到我们的WindowManager上场了。通过它我们和WMS之间建立了关联，进而将上述Window送到surfaceflinger中进行相关的渲染显示，并最终呈现给我们用户。

___

### 一. Activity布局加载前期准备

  前面整了这么一大堆，读者没有感到啰嗦我都觉得有点啰嗦和废话了！但是吗，有时候不来点前戏，直入主题又有点突兀(各位此段不是开车，真不是开车)，让人难以理解，这个各位见仁见智吗！通过前面一系列的博客我们知道，在startActivity()后，经过一些逻辑流程会通知到system\_server进程的AMS服务，AMS接收到启动Activity的请求后(无论是冷启动目标Activity还是热启动目标Activity)，最后都会通过跨进程通信调用AcitivtyThread.handleLauncherActivity()方法，我们从这里开始分析。注意我们这里只重点关注和Activity布局加载相关的逻辑代码，至于目标Activiyt启动和生命周期相关执行的代码就不是这里的博客重点关注的，因为我们在前面的博客[Activity启动流程(七)- 初始化目标Activity并执行相关生命周期流程](https://editor.csdn.net/md?articleId=109444690)中已经有重点关注过了！

  

#### 1.1 ActivityThrread.handleLaunchActivity(…)

```
//[ActivityThread.java]
    private void handleLaunchActivity(ActivityClientRecord r, Intent customIntent, String reason) {
    ...    
        WindowManagerGlobal.initialize();//这个方法的实质是获取WMS服务代理端

...
Activity a = performLaunchActivity(r, customIntent);//创建目标Activity，并创建Activity对应的Window,详见章节1.2
...
        if (a != null) {
        //此方法最终会调用到Activity的onResume()方法中
        handleResumeActivity(r.token, false, r.isForward,
                    !r.activity.mFinished && !r.startsNotResumed, r.lastProcessedSeq, reason);
        }
        ...
    }
```

handleLaunchActivity方法中有三个我们需要重点关注的点:

-   首先就是WindowManagerGlobal.initialize()方法，WindowManagerGlobal是单例模式的，一个进程内只有一个，这里调用该类的initialize初始化方法，获取WMS服务的代理端
-   接着调用performLaunchActivity方法，从而创建目标Activity，并创建Activity对应的Window，这个我们会在后面章节详细的分析
-   最好调用handleResumeActivity方法，进而执行目标Activity的onResume方法，这个也后面详细分析

这里我们先看看WindowManagerGlobal.initialize()方法的实现，代码如下:

```
//[WindowManagerGlobal.java]

private static IWindowManager sWindowManagerService;
    public static void initialize() {
        getWindowManagerService();
    }
    public static IWindowManager getWindowManagerService() {
        synchronized (WindowManagerGlobal.class) {
            if (sWindowManagerService == null) {
                sWindowManagerService = IWindowManager.Stub.asInterface(
                        ServiceManager.getService("window"));
                try {
                    sWindowManagerService = getWindowManagerService();
                    ValueAnimator.setDurationScale(sWindowManagerService.getCurrentAnimatorScale());
                } catch (RemoteException e) {
                    throw e.rethrowFromSystemServer();
                }
            }
            return sWindowManagerService;
        }
    }
```

可以看到上述方法比较简单，就是通过一个单例模式获取WMS的代理端，进而为Activity目标进程和WMS的通信打下基础！这里我们只需注意一下IWindowManager的类图关系，正是由于它的存在所以才具备了跨进程和WMS服务Binder通信的能力。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/036c5fcdf53d64b00a3f2b6c9f707e1f.png#pic_center)

  

#### 1.2 ActivityThrread.performLaunchActivity(…)加载Activity以及对应的Window窗口

  这里我们只重点关注和Activity布局加载相关的源码，关于其它的可以参见博客[Activity启动流程(七)- 初始化目标Activity并执行相关生命周期流程](https://editor.csdn.net/md?articleId=109444690)中的章节2.3有详细的介绍(在这里只会一笔带过)。

```
//[ActivityThread.java]
    private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
//开始构建目标Activity相关信息
...
        Activity activity = null;
        try {
        /*
        通过反射加载目标Activity
        这里获取的ClassLoader是前面博客已经初始化了的
        */
            java.lang.ClassLoader cl = r.packageInfo.getClassLoader();
            activity = mInstrumentation.newActivity(
                    cl, component.getClassName(), r.intent);
...
        } catch (Exception e) {
...
        }
        try {
        /*
        创建目标Actiivty应用进程Application，目标Application在前面博客中已经有被创建了，而且
        应用进程的Application是唯一的，所以会直接返回前面创建的Applcation
        */
            Application app = r.packageInfo.makeApplication(false, mInstrumentation);
            if (activity != null) {
            //创建目标Activity对应的Context上下文
                Context appContext = createBaseContextForActivity(r, activity);
...

                Window window = null;
                if (r.mPendingRemoveWindow != null && r.mPreserveWindow) {//此种情况即复用以前的Window
                    window = r.mPendingRemoveWindow;
                    r.mPendingRemoveWindow = null;
                    r.mPendingRemoveWindowManager = null;
                }   
                //将上述创建的相关信息，attach到Activity中为后续Activity显示运行做准备，详见1.3     
                activity.attach(appContext, this, getInstrumentation(), r.token,
                        r.ident, app, r.intent, r.activityInfo, title, r.parent,
                        r.embeddedID, r.lastNonConfigurationInstances, config,
                        r.referrer, r.voiceInteractor, window);
...
                activity.mCalled = false;//这个地方是干啥的呢，防止开发者在执行onCreate()方法的时候没有初始化父类Activity的onCreate()方法
                //开始执行目标Activity的onCreate()方法回调
                if (r.isPersistable()) {
                    mInstrumentation.callActivityOnCreate(activity, r.state, r.persistentState);
                } else {
                    mInstrumentation.callActivityOnCreate(activity, r.state);
                }
...

        } catch (SuperNotCalledException e) {        
            throw e;

        } catch (Exception e) {
...
        }

        return activity;
    }                             

```

上述方法在这里我们只关心两点:

-   通过传递进来的ActivityClientRecord信息，通过反射创建目标Activity实例对象。
-   然后调用目标Activity的实例对象的attach()方法(这个方法，在前面的博客中我们只是简单的一笔带过，但是在这个博客中是重中之重需要重点分析)。这里我们可以看到attach传入的参数非常非常多，毫不夸张的说我们在performLacunchActivity()方法以及更前面的一系列调用方法中，绝大部分工作都是为了填充ActivityClientRecord的信息而做的，待ActivityClientRecord的信息填充饱满以后借用attach()方法将这些参数配置到新创建的Activity对象中，从而将目标Activity和我们的应用进程，甚至是AMS等关联起来。

> 这里我们还可以看到，在attach之前可能会初始化一个Window实例对象，即如果ActivityClientRecord.mPendingRevomeWindow变量中已经保存了一个Window对象，则会在后面的attach方法中被使用，具体使用的场景会在后面中介绍。
> 
> 这里的Window是一个抽象类，它实现了对Activity的布局的管理，从而负责加载显示界面，并且每个Activity都会对应了一个Window对象。

  

#### 1.3 Activity.attach(…)为Activity附加应用进程相关信息以及创建目标Window

  此时目标Activity以及相关所需要的对象都创建好了，就需要将Activity和Application对象、ContextImpl对象绑定在一起，并且创建目标Activity对应的Window窗口了。并且我们还需要知道上述的几个之间在Activity应用进程之间存在着如下的关联(至于具体的解释，详见博客[Activity启动流程(七)- 初始化目标Activity并执行相关生命周期流程](https://editor.csdn.net/md?articleId=109444690)的第三大章节)。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f3bd40271df3390e9b460aa8026ea28b.png#pic_center)

```
//[Activity.java]
    final void attach(  
    Context context, //目标Activity对应的Context上下文信息，即前面创建的ContextImpl实例对象
    ActivityThread aThread,//Activity应用进程的主线程ActivityThread实例对象
            Instrumentation instr, //监控管理Activity运行状态的Instrumentation实例对象
            IBinder token, //这个token是啥呢，用于和AMS服务通信的IApplicationToken.Proxy代理对象，它对应的实体是在AMS为目标Activity创建对应的ActivityRecored实例对象的时候创建的
            int ident,
            Application application, //Activity应用进程对应的Application实例对象
            Intent intent, 
            ActivityInfo info,//Activity在AndroidManifest中的配置信息
            CharSequence title, 
            Activity parent, //启动当前Activity对应的Activity
            String id,
            NonConfigurationInstances lastNonConfigurationInstances,
            Configuration config, String referrer, 
            IVoiceInteractor voiceInteractor,
            Window window  //是否存在可以复用的Window
     ) 
{
        //将前面创建的上下文对象ContextImpl保存到Activity的成员变量中
        attachBaseContext(context);
        
//这个应该和Fragment有关系，这个不在我们本篇博客的关注范围之内，所以让我们大声的说pass
        mFragments.attachHost(null /*parent*/);

//此处是重点，直接以PhoneWindow实例Window窗口对象
        mWindow = new PhoneWindow(this, window);//详见章节1.4
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
        //记录记录应用程序的UI线程
        mUiThread = Thread.currentThread();

//记录应用进程的ActivityThread实例
        mMainThread = aThread;
        mInstrumentation = instr;
        mToken = token;
        mIdent = ident;
        mApplication = application;
        mIntent = intent;
        mReferrer = referrer;
        mComponent = intent.getComponent();
        mActivityInfo = info;
        mTitle = title;
        mParent = parent;
...//初始化其它的相关Actiivty成员变量

//为Activity所在的窗口设置窗口管理器，详见章节1.5，并且这里的setWindowManager没有被重构，直接调用的是父类Window的方法
        mWindow.setWindowManager(
                (WindowManager)context.getSystemService(Context.WINDOW_SERVICE),
                mToken, mComponent.flattenToString(),
                (info.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED) != 0);
        if (mParent != null) {
            mWindow.setContainer(mParent.getWindow());
        }
        //保存窗口管理器实例对象
        mWindowManager = mWindow.getWindowManager();
        mCurrentConfig = config;
    }
```

在我们哼哧哼哧进行相关的源码分析前，我们有必要来理一理attach的入参参数(不知道小伙们有没有发现，反正Android源码中一些方法或者函数的入参经常是一大把一大把的)！

| 参数类型 | 参数名称 |               参数功能 |
| --- | --- | --- |
| Context | context | 目标Activity对应的Context上下文信息，即前面创建的ContextImpl实例对象 |
| ActivityThread | aThread | Activity应用进程主线程ActivityThread实例对象 |
| Instrumentation | instr | 监控管理Activity运行状态的Instrumentation实例对象，它在整个进程中也是唯一的 |
| IBinder | token | 这个token是啥呢，它是被用于和AMS服务通信的IApplicationToken.Proxy代理对象，它对应的实体是在AMS为目标Activity创建对应的ActivityRecored实例对象的时候创建的 |
| Application | application | Activity应用进程对应的Application实例，它在整个进程中是唯一的 |
| Activity | activity | 启动当前目标Activity的Activity |
| Window | window | 表示当前Activity是否存在可以复用的Window |
| … | … | … |

好了入参的参数我们捯饬清楚了，是时候进入正题分析attach方法的主要逻辑了，它干了如下几件事情:

-   为Activity创建对应的Window实例对象，并且这里的Window类型直接点明了为PhoneWinow，此处为Activity和Window之间的关联正式建立了！并且我们后续会知道Activity中很多操作View相关的方法，例如setContentView()、findViewById()、getLayoutInflater()等，实际上都是间接调用到PhoneWindow里面的相关方法的，这样是为什么说Window是布局的管理类了
-   根据掺入的参数初始化Activity相关的成员变量
-   为上述创建的Window实例对象创建窗口管理器(注意一定要和WMS的客户端代理区分清楚，千万不要搞错了)

至此我们为Activity已经初始化了各种实例对象了，并且他们之间存在着如下的关联关系:

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/a8b6f0756446a127ff5a0abf28dd1ed0.png#pic_center)

  

#### 1.4 应用进程Activity窗口Window创建流程

在分析Activity窗口怎么建立之间，我们先看看Activity所谓窗口Window的关系图如下：  

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/de29f46c269e1021c44b0e9beaaaab5d.png#pic_center)

> Window从语义上翻译就是窗户，窗口的意思，放在现实世界中都是很具体的物件。而我们这里的Window是一个抽象的类，并没有继承任何View或者实现View，并且其子类也是如此！记得我刚开始学习关于Android应用进程窗口相关的概念是老是转弯不过来，总是感觉这里的Window好空洞啊，是不是翻译或者起的名字有问题和现实世界关联不起来啊！那么我们应该怎么理解这里的Window窗口的概念呢，先看看来可以从如下两点入手:  
> 1.读者可以这么认为这个是Android的设计者人为而设计的用来管理Activity布局的一个管理类，且它的类名恰好叫做Window而已，并且在Android相关的知识传播过程中将这里的Window统称为窗口了(并且我们在后续的分析中会知道Activity真的和窗口显示相关的是DecorView)。  
> 2.Activity的各种布局确实需要在Android给我们的划定的Window中进行各种操作，至少在这个层面说此Window被叫做窗口也不为过以上的是我的个人理解，总之至少中国的Android界是这么约定俗成的叫着，至于具体怎么理解小伙们就看各位小伙们了。网上也有的博客将Window理解为现实世界的窗户，各种View理解为窗花，这个就仁者见仁，智者见智各自理解吗！

好了前面扯了一堆我对Activity中窗口概念的理解，是时候看看Activity中的窗口是怎么被构建出来的了！

```
//[PhoneWindow.java]
    public PhoneWindow(Context context, Window preservedWindow) {
        this(context);
        mUseDecorContext = true;//注意此处，这个点后续会用到
        if (preservedWindow != null) {//重点要理解的是此处
            mDecor = (DecorView) preservedWindow.getDecorView();
            mElevation = preservedWindow.getElevation();
            mLoadElevation = false;
            mForceDecorInstall = true;
            getAttributes().token = preservedWindow.getAttributes().token;
        }

        boolean forceResizable = Settings.Global.getInt(context.getContentResolver(),
                DEVELOPMENT_FORCE_RESIZABLE_ACTIVITIES, 0) != 0;
        mSupportsPictureInPicture = forceResizable || context.getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_PICTURE_IN_PICTURE);
    }
```

这里的PhoneWindow构造方法看似平淡无奇，但是这里我们需要重点关注的就是preserviedWindow参数(从英文字面意思直译过来就是已经被保存的窗口)，从调用的流程来看这个参数最终来自于前面章节1.2中所提到的mPendingRevomeWindow变量，也许读者会有一个疑问这个参数在什么时候会不为空呢？其实这里的逻辑是用来快速重启Acitivity的，比如你的一个Activity已经启动了，但是主题换了或者configuration变了，这里只需要重新加载资源和View，没必重新再执行DecorView的创建工作(关于这块，我们可以在源码分析中暂且忽略)。

> 另一个要关注的就是mDecor变量，这个变量是DecorView类型的(它，我们这里暂且不过多分析，后续会专门分析它)，如果这里没有初始化的话，则会在调用setContentView方法中new一个DecorView对象出来。DecorView对象继承自FrameLayout，所以他本质上还是一个View，只是对FrameLayout做了一定的包装，例如添加了一些与Window需要调用的方法setWindowBackground()、setWindowFrame()等。我们知道，Acitivty界面的View是呈树状结构的，而mDecor变量在这里作为Activity的界面的根View存在。关于这三者之间的关系如果一定要找到现实世界中的映射，那么PhoneWindow就是我们经常在电梯中看到的电子广告屏，DecorView就是电子广告屏要显示的内容，而Activity就是控制电子广告屏要具体显示什么的神秘幕后的操盘手了。

  

#### 1.5 创建应用进程Activity窗口管理器

  好了经过我们前面一阵捣鼓，我们的Activity的窗口对象PhoneWindow也建立了，而且通过前面章节1.3的分析可知有了窗口Android必不会对其放任自流不管，紧接着会指定一个窗口管理器来管理这些窗口，因此在Activity启动过程中还会创建一个WindowManager对象来管理上述的窗口，我们接着看看这个窗口管理器是何方妖怪，吃我老孙一棒！

并且在进行相关的分析前，我们先来看看WindowManager的类图关系，注意它们是接口类，注意注意注意！

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/abd8814a46be51372234192dc40d56c6.png#pic_center)

```
//[Window.java]
private WindowManager mWindowManager;
    public void setWindowManager(WindowManager wm, IBinder appToken, String appName,
            boolean hardwareAccelerated) {
        mAppToken = appToken;// IApplicationToken.Proxy代理对象
        mAppName = appName;
        mHardwareAccelerated = hardwareAccelerated
                || SystemProperties.getBoolean(PROPERTY_HARDWARE_UI, false);
        if (wm == null) {
            wm = (WindowManager)mContext.getSystemService(Context.WINDOW_SERVICE);
        }
        mWindowManager = ((WindowManagerImpl)wm).createLocalWindowManager(this);
    }
```

setWindowManager方法的入参有一个需要注意的就是wm，这里的wm从前面的章节知道它是通过调用如下方式获取到的，如下:

```
(WindowManager)context.getSystemService(Context.WINDOW_SERVICE)
```

根据Context的类图关系我们可知，上述方法最终会调用到ContextImpl中去，执行如下的逻辑：

```java
//[SystemServiceRegistry.java]
    @Override
    public Object getSystemService(String name) {
        return SystemServiceRegistry.getSystemService(this, name);
    }

    registerService(Context.WINDOW_SERVICE, WindowManager.class,
            new CachedServiceFetcher<WindowManager>() {
        @Override
        public WindowManager createService(ContextImpl ctx) {
            return new WindowManagerImpl(ctx);
        }});
```

上面的相关逻辑就不展开了，总之获取到的是以Activity对应的Context上下文构建的WindowManagerImpl对象实例！

  好了重要的入参我们捯饬清楚了，我们接着来分析分析setWindowManager方法干了些啥，可以看到它主要是实例化了wWindowManager变量。它实例化的前提是将前面传递过来的参数wm强制转换为WindowManagerImpl，然后调用createLocalWindowManager()，在createLocalWindowManager()实际是执行了一个new WindowManagerImpl()方法来创建。

> 关于这部分代码本人存在一个很大的疑惑点，就是为啥Android当初要把这个地方涉及的这么复杂呢，其实传递过来的wm已经是一个WindowManagerImpl类型的实例了，这里createLocalWindowManager()重新创建一个实例，不过是使用了前面传递过来的wm的成员变量mContext，然后加上此时PhoneWindow自己的this指针而已！直接使用传递过来wm，然后添加一个方法将PhoneWindow传递过去就可以这样不香吗，反而多此一举再new一个呢！

```
//[WindowManagerImpl.java]
    private WindowManagerImpl(Context context, Window parentWindow) {
        mContext = context;
        mParentWindow = parentWindow;
    }

    public WindowManagerImpl createLocalWindowManager(Window parentWindow) {
    //注意这里的parentWindow是我们前面创建的PhoeWindow
        return new WindowManagerImpl(mContext, parentWindow);
    }
```

> 注意此处的parentWindow指向了Activity对应的PhoneWindw窗口，这个地方需要注意，后续我们会用到！

至此我们为Actiivyt的窗口创建了了窗口管理器WindowManagerImpl，并且它保存了对于单例对象WindowManagerGloble的引用，即mGlobal变量。此外，通过前面我们的类图可以看到WindowManagerImpl实现了WindowManager，并且WindowManager继承自ViewManager接口，ViewManager接口方法如下所示(可以看到它定义的接口方法都是和View有关系的)：

```
//[ViewManager.java]
public interface ViewManager
{
    public void addView(View view, ViewGroup.LayoutParams params);
    public void updateViewLayout(View view, ViewGroup.LayoutParams params);
    public void removeView(View view);
}
```

分析到这里我们发现我们Activity直接持有WindowManagerImpl，然后通过WindowManagerImpl间接持有WindowManagerGloble，并且WindowManagerGloble在整个应用进程中是唯一的(因为它采用了单例模式)。至此我们可以得出一个结论就是Android系统为每一个启动的Activity创建了一个轻量级的窗口管理器WindowManagerImpl，每个Activity通过WindowManagerImpl来访问WindowManagerGloble,并且Activity中关于窗口的相关操作最后都是由mGlobal亲自操刀的，它们三者之间的关系如下图所示：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/cbef635093af6cadf1f6d229fd16772c.png#pic_center)

___

### 小结

  本来是打算将Activity布局加载流程实现详解放在这篇博客中，一网打净的！但是，但是读者也看到了，到此还只是进行了Activity布局加载前期准备就已经有了这么多的内容了，所以为了阅读方便只能将此部分分系列进行了！

这里我们还是按照老规矩，先来对前面的知识总结一番，在本篇章节中我们重点分析到了：

-   通过前面一系列启动Activity的前期工作获取到的Activity信息，然后通过反射创建了我们的目标Activity
-   接着调用目标Activity的attach方法，将启动Activity的相关信息初始化它的成员变量
-   然后在attach方法中，创建Activiyt对应的窗口Window(这里建议读者重点理解理解Window窗口的概念)
-   接着为创建的Window窗口设置窗口管理器WindowManagerImpl

其整个流程可以通过如下的伪代码来表示:

```

AT.handleLaunchActivity(...)  --->
AT.performLaunchActivity(...) --->
通过反射创建目标Activity(new Activity()) --->
创建Context上下文 --->
Activity.attach(...) --->
ContextImpl绑定
创建PhoneWindow
创建WindowManager管理器
Activity.onCreate() --->
setContentView(...) --->

```

至此，经过上述的努力我们Activity布局加载流程实现详解的前期准备工作已经做好了，窗口已经准备了，窗口管理器也已经准备好了，就等Activity在我们的窗口上任意创作需要显示的界面了，然后加载到我们的WIndow窗口中进行显示了。而上述未完成的任务也是我们后续博客将要重点分析的了。

___

### 写在最后

  [Android应用程序窗口机制之Window及WindowManager的创建](https://editor.csdn.net/md?articleId=109597570)这里就要告一段落了，Activity布局加载流程的准备工作也已经就绪了，作画的窗口，亦或者是要展示广告的电子屏已经备好就等Activity来尽情挥洒墨水创作了。在接下来的博客中，我将会重点分析目标Activity的是怎么加载布局以及怎么将布局绘制到我们的终端界面上面的，如果有感兴趣的亲请期待。好了，青山不改绿水长流先到这里了。如果本博客对你有所帮助，麻烦关注或者点个赞，如果觉得很烂也可以踩一脚！谢谢各位了！！