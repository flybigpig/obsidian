##   普法Android Token的前世今生以及在APP,AMS,WMS之间传递

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
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3cd1477372a7a3b29b5ea1345836293c.png)

为了更加方便的读者阅读博客，通过导读思维图的形式将本博客的关键点列举出来，从而方便读者取舍和阅读！

___

  

### 引言

  我们知道Activity从启动到展示到我们的Android终端设备上涉及到了非常多与Android核心服务和进程的交互，特别是目标Activity进程和AMS，WMS服务之间的交互！而通过前面的系列博客[Activity启动流程源码实现详解](https://blog.csdn.net/tkwxty/article/details/108652250)和[Android应用程序窗口设计](https://blog.csdn.net/tkwxty/article/details/109597570)系列博客前几篇我们也验证了这一点，而在他们的三者交互之间频繁的大量的使用到了一个东西"Token" ,而今天的博客要解决的就是关于它的灵魂三大拷问:

-   Token的起源是什么
-   Android源码框架中存在那些关联的Token
-   Token是怎么在App进程和AMS，WMS之间传递的
-   Android为什么要设计这么一个Token的概念

> 我们知道在通常的网络登录认证中有一个Token令牌的概念，虽然我们这边的Token和上述的Token虽然不完全是同一个概念，但是也有一部分的功能即进行相关的安全验证的功能这点倒是有点相似，估摸着谷歌的工程师可能也偷摸的借用了这个设计的理论！

注意：本篇的介绍是基于Android 7.xx平台为基础的，其中涉及的代码路径如下：

```
frameworks/base/services/core/java/com/android/server/am/
  --- ActivityManagerService.java
  --- ProcessRecord.java
  --- ActivityRecord.java
  --- ActivityResult.java
  --- ActivityStack.java
  --- ActivityStackSupervisor.java
  --- ActivityStarter.java
  --- TaskRecord.java 

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

frameworks/base/services/core/java/com/android/server/wm/
---WindowManagerService.java
---AppWindowToken.java

kernel/drivers/staging/android/binder.c
```

> 并且在后续的源码分析过程中为了简述方便，会将做如下简述:  
> ActivityManagerService简称为AMS  
> WindowManagerService简称为WMS

在正式开始本篇博客普法Android的Token在APP,AMS,WMS之间传递之前，这里有必要先将Token在APP,AMS,WMS传递的简单交互图先放出来，以便能够方便后续的相关阅读，使读者能够更加清楚的知道这几者之间在源码中的关系。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/2df125f520e3f8523e86111b20dfbd58.png)

___

### 一.前期知识储备

> 在正式开始后续的博客分析前，我们得先磨磨刀后砍柴！  
> 本篇博客写作的出发点是为了我的系列博客[Android应用程序窗口设计](https://blog.csdn.net/tkwxty/article/details/109597570)而做的铺垫，因为在写到Android应用窗口的添加的时候发现关于Token的知识点不花一点时间来先将阐述清楚后续就很难分析写去了，所以这里才会花一个专门的篇章来讲述此问题。

  

#### 1.1 Binder知识储备

> 先分析Android源码特别是framework层的相关东西，真的强烈建议不说熟读Android的Binder知识，但是也一定要对Android的Binder知识要有一定的了解不然你会很难前进的。

  我们先站在上帝的视角提前来看下token(或者是Token)，这个token其实是一个IBinder对象，有的类中虽然定义token时没有直接指定为IBinder，但是追根溯源，发现其实最终还是指向了IBinder。说到IBinder之后，读者肯定立马就想到了Android特有的跨进程IPC通信，这个是Android系统的核心特色之一。我们知道，Android的framework框架在整个系统中扮演的角色相当于服务端，而我们开发的应用程序相当于客户端，Activity的创建、启动等操作都是通过IPC的方式来实现服务端和客户端之间的通信的。

但是这里的token和我们通常意思的Binder服务又是不同的，它并没有注册到servicemanager服务大管家之中而是匿名存在的，只能通过实名Binder进行传递。所以读者在阅读今天的博客前，一定要对Binder知识有一个比较深入的掌握程度，不然只会陷入其中而不能理解！如果对Binder特别是匿名/实名Binder还没有了解的读者强烈建议先阅读一下该篇以及一系列的博客[Android Binder框架实现之何为匿名/实名Binder](https://blog.csdn.net/tkwxty/article/details/108343847)。

> 通过前面的博客我们知道匿名Binder有两个比较重要的用途:  
> 一个是拿到Binder代理端后可跨Binder调用实体端的函数接口  
> 另一个作用便是在多个进程中标识同一个对象
> 
> 并且匿名Binder的上述两个作用是同时存在的，比如我们这里将要研究的Token就同时存在这两个作用，但最重要的便是后者，Token标识了一个ActivityRecord对象，即间接标识了一个Activity，从而确保Activity在App进程和system\_server进程的AMS和WMS服务之间传递的合法性

  

#### 1.2 Activity启动和显示知识储备

  我们今天所说的Token所涉及的流程，主要是在Activity启动到显示到Android设备终端的过程中的，所以读者在阅读此博客前一定要对Activity启动和显示的几个核心关键步骤有所了解(反过来一想，如果没有了解也不会看Token的传递了，可能是我在这里多此一举了吗)。这里大伙可以看看系列博客[Activity启动流程源码实现详解](https://blog.csdn.net/tkwxty/article/details/108652250)和[Android应用程序窗口设计](https://blog.csdn.net/tkwxty/article/details/109597570)。

___

### 一.Token的诞生

  自古中国古代牛逼的人物诞生都是电闪雷鸣，天降祥瑞啥的(莫非我们的Token诞生也是如此)。但是很遗憾的是我们的Token的诞生并没有这么大的排场，几乎是静悄悄的如果不是特别点明几乎不会有人留意到，所以让八卦的读者可能要失望了(开个玩笑啊)！

我们知道在Activity的启动过程中会在AMS服务中构建一个ActivityRecord实例对象来标记Activity关于它的创建流程可以详见博客[Activity启动流程(一)发起端进程请求AMS启动目标Activity](https://blog.csdn.net/tkwxty/article/details/108680198))，而这里的Token是ActivityRecord的内部静态类它就是在这个时候被创建的，让我们先来看下该Token的继承关系如下：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/236375d4b89a27318a734ad531ae01b8.png)

这里我们用伪代码来表述一下Token诞生记，如下:

```
   ActivityStarter.startActivityLocked(...)
  ActivityRecord r = new ActivityRecord(callerApp,intent,aInfo,mSupervisor,...)
appToken = new Token(this, service);
static class Token extends IApplicationToken.Stub {
....
}
 
```

> 这里可以看到，根据Binder的机制可以知道此处的Token是一个匿名Binder实体类，这个匿名Binder实体会传递给其他进程，其它进程拿到的是Token的代理端IApplicationToken.Stub.Proxy(这点很重要，很重要，很重要)。

至此我们的Token诞生了，还没有等待它的长大它就要开始它的传递之旅了！

___

### 二.Token的传递之旅

  前面突突的讲了这么多都是为了引出全文的关键，即Token在AMS，WMS，APP进程之间的传递流程，我们先来看一个整体传递框架的流程图如下：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b3979d28df6579c8b7dec5359db972a6.png)

> 这里的图示需要注意红色的箭头表示的跨进程Binder通信  
> 红色框表示的Token是它的实体端，而黄色表示的是它的Binder代理端，所以这里WMS和AMS之间的Token其实指向的是同一个

通过上述的交互图我们可以看到，Token的创建和传递可以分为如下四条分支(传递路线是Activity的启动到显示的流程进行的):

-   在Activity的启动过程中会在AMS中创建一个构建一个ActivityRecord实例对象来标记Activity，构造过程中会创建一个Token的Binder实体对象保存在ActivityRecord的appToken成员变量之间
    
-   AMS—>WMS:AMS创建完Token之后会调用WMS服务的addAppToken()方法在WMS中构建一个AppWindowToken实例对象，然后将Token对象保存在它的appToken成员变量之中！这里注意的是AMS和WMS同属于system\_server进程，所以它们之间是不需要跨进程传递的，所以传递的是Token的Binder实体。
    
-   AMS—>APP进程：随着Actiivyt的启动流程的进行Token会随着Activity所属进程的启动，通过AMS跨Binder调用ActivityThread的scheduleLaunchActivity()方法传递到ActivityThread类中，然后将其保存在ActivityClientRecord的token成员变量中，注意此处的Token经过Binder跨进程通信以后已经变为Binder的代理端了
    
-   APP进程—>WMS：继续随着Activity的启动和显示的进行，Android的窗口管理器会通过Binder跨进程通信调用WMS服务的addWindow()方法添加Activity的窗口给WMS服务进行管理，此时在添加的过程中就会携带Token的Binder代理端！虽然在跨进程传递前Token还只是BInder代理端但是传递到WMS来之后此时的Token代理端就会自动转变成为Token的实体端对象了(这个是Binder机制传输决定的)。从而我们的WMS就可以根据此处的Token对象进而找到前面先前创建添加的AppWindowToken，从而就可以确定当前我们要添加的窗口是属于那个Activity了从而很好的发挥了令牌的作用。
    

> 这里需要补充一点的是关于Binder实体和代理端在Binder驱动中传输的转换关系可以参加[Android Binder框架实现之何为匿名/实名Binder](https://blog.csdn.net/tkwxty/article/details/108343847)以及一系列相关的博客，或者读者可以重点阅读binder.c(内核中的)的binder\_transaction()函数关于Binder数据传输的解析流程，这里就不过多展开了。

前面的交互图为我们指明了方向，而我们接下来就是沿着光明的道路前进即可，分明展开来看看具体的传递流程！

  

#### 2.1 AMS—>WMS token传递第一棒

  在Activity的启动过程中，AMS会调用startActivityLocked()方法向WMS服务传递Token实体对象(详见博客[system\_server进程处理启动Activity请求](https://blog.csdn.net/tkwxty/article/details/108745996))，其使用伪代码来表示的流程如下:

```
ActivityStarter.startActivityUnchecked()--->
ActivityStack.startActivityLocked()--->
addConfigOverride()---->
mWindowManager.addAppToken()--->
```

我们这里来看下WMS服务是怎么处理addAppToken流程的！

```
//[WindowManagerService.java]
    @Override
    public void addAppToken(int addPos, IApplicationToken token, int taskId, int stackId,
            int requestedOrientation, boolean fullscreen, boolean showForAllUsers, int userId,
            int configChanges, boolean voiceInteraction, boolean launchTaskBehind,
            Rect taskBounds, Configuration config, int taskResizeMode, boolean alwaysFocusable,
            boolean homeTask, int targetSdkVersion, int rotationAnimationHint) {
...
//查找是否已经向WMS服务中传递过Token，并且这里的AppWindowToken是WindowToken的子类
AppWindowToken atoken = findAppWindowToken(token.asBinder());
            if (atoken != null) {
                Slog.w(TAG_WM, "Attempted to add existing app token: " + token);
                return;
            }
       //生成ActivityRecord在WMS中对应的AppWindowToken,并引用到ActivityRecord中的Token
       atoken = new AppWindowToken(this, token, voiceInteraction);
       ...
        /*
        如果没有Task, 就创建一个task, 并加入到stack中，
            这里的task/stack都是与AMS中task/stack就一一对应的            
            */
            Task task = mTaskIdToTask.get(taskId);
            if (task == null) {
            //没有则创建
                task = createTaskLocked(taskId, stackId, userId, atoken, taskBounds, config);
            }
            //将AppWindowToken加入到task中管理起来
            task.addAppToken(addPos, atoken, taskResizeMode, homeTask);
//加入到mTokenMap中
            mTokenMap.put(token.asBinder(), atoken);
    }
```

这里我们看到WMS中对于Token的管理和AMS中对于Activity的管理很类似，采取堆栈Task的形式！看来AMS和WMS是一对好基友啊！那么我们怎么验证呢，伟大的谷歌为我们提供了伟大的工具，我们可以通过dumpsys window命令进行相关的查看(注意这里限于篇幅，只显示了一部分)，如下:

```
xxx/#dumpsys window
WINDOW MANAGER DISPLAY CONTENTS (dumpsys window displays)
  Display: mDisplayId=0
    init=720x1280 320dpi cur=720x1280 app=720x1184 rng=720x672-1184x1136
    deferred=false layoutNeeded=false

  Application tokens in top down Z order:
    mStackId=1
    mDeferDetach=false
    mFullscreen=true
    mBounds=[0,0][720,1280]
      taskId=103
        mFullscreen=true
        mBounds=[0,0][720,1280]
        mdr=false
        appTokens=[AppWindowToken{5e6032b token=Token{6264fa5 ActivityRecord{8ee899c u0 com.example.window/.MainActivity t103}}}]
        mTempInsetBounds=[0,0][0,0]
          Activity #0 AppWindowToken{5e6032b token=Token{6264fa5 ActivityRecord{8ee899c u0 com.example.window/.MainActivity t103}}}
          windows=[Window{4163ff6 u0 com.example.window/com.example.window.MainActivity}]
          windowType=2 hidden=false hasVisible=true
          app=true voiceInteraction=false
          allAppWindows=[Window{4163ff6 u0 com.example.window/com.example.window.MainActivity}]
          task={taskId=103 appTokens=[AppWindowToken{5e6032b token=Token{6264fa5 ActivityRecord{8ee899c u0 com.example.window/.MainActivity t103}}}] mdr=false}
           appFullscreen=true requestedOrientation=-1
          hiddenRequested=false clientHidden=false reportedDrawn=true reportedVisible=true
          numInterestingWindows=1 numDrawnWindows=1 inPendingTransaction=false allDrawn=true (animator=true)
          startingData=null removed=false firstWindowDrawn=true mIsExiting=false
    mStackId=0
    mDeferDetach=false
    mFullscreen=true
    mBounds=[0,0][720,1280]
      taskId=102
        mFullscreen=true
        mBounds=[0,0][720,1280]
        mdr=false
        appTokens=[AppWindowToken{a014b86 token=Token{7700dba ActivityRecord{fb0cee5 u0 com.android.launcher3/.Launcher t102}}}]
        mTempInsetBounds=[0,0][0,0]
          Activity #0 AppWindowToken{a014b86 token=Token{7700dba ActivityRecord{fb0cee5 u0 com.android.launcher3/.Launcher t102}}}
          windows=[Window{92f807 u0 com.android.launcher3/com.android.launcher3.Launcher}]
          windowType=2 hidden=true hasVisible=true
          app=true voiceInteraction=false
          allAppWindows=[Window{92f807 u0 com.android.launcher3/com.android.launcher3.Launcher}]
          task={taskId=102 appTokens=[AppWindowToken{a014b86 token=Token{7700dba ActivityRecord{fb0cee5 u0 com.android.launcher3/.Launcher t102}}}] mdr=false}
           appFullscreen=true requestedOrientation=5
          hiddenRequested=true clientHidden=true reportedDrawn=false reportedVisible=false
          mAppStopped=true
          numInterestingWindows=1 numDrawnWindows=1 inPendingTransaction=false allDrawn=true (animator=true)
          startingData=null removed=false firstWindowDrawn=true mIsExiting=false
```

在接下来的分析前，我们有必要把AppWindowToken的类图关系捋一捋(不然会被它之间的token关系给整懵去)！

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/241787db69363ea5ce9b9b7b45241350.png#pic_center)

我们接着往下看AppWindowToken的构造方法，如下:

```
//[AppWindowToken.java]
    AppWindowToken(WindowManagerService _service, IApplicationToken _token,
            boolean _voiceInteraction) {
        //将Token的Binder实体传递给AppWindowToken父类WindowToken
        super(_service, _token.asBinder(),
                WindowManager.LayoutParams.TYPE_APPLICATION, true);
        appWindowToken = this;
        appToken = _token;//将ActiivityRecord中的Token赋给此处的appToken
        voiceInteraction = _voiceInteraction;
        mInputApplicationHandle = new InputApplicationHandle(this);
        mAppAnimator = new AppWindowAnimator(this);
    }
```

  

#### 2.2 AMS—>App进程token传递第二棒

  随着Actiivyt的启动流程的进行Token会随着Activity所属进程的启动，通过AMS跨Binder调用ActivityThread的scheduleLaunchActivity()方法传递到ActivityThread类中，然后将其保存在ActivityClientRecord的token成员变量中，注意此处的Token经过Binder跨进程通信以后已经变为Binder的代理端了(关于此处具体的调用流程详见[Activity启动流程(七)初始化目标Activity并执行相关生命周期流程](https://blog.csdn.net/tkwxty/article/details/109444690)，其大致伪代码流程如下:

```
attachApplicationLocked
    realStartActivityLocked
        app.thread.scheduleLaunchActivity(new Intent(r.intent), r.appToken, );

public final void scheduleLaunchActivity(Intent intent, IBinder token, ) {
            //生成App中的ActivityClientRecord
            ActivityClientRecord r = new ActivityClientRecord();
            r.token = token;  //将AMS中的token保存到 ActivityClientRecord中 见P5
}
```

你以为这个传递就完结了吗，不不不！随着Activity在目标进程生命周期的调度过程中AMS传递过来的Token几乎遍布了App进程的各个角落，这里我先总结一下然后来说明其在App进程中的传递流程的。

| 所在类 | 变量信息 |
| --- | --- |
| ActivityClientRecord |        IBinder token |
| Activity |       IBinder mToken |
| Window |        IBinder mAppToken |
| WindowManager.LayoutParams |        IBinder token |

##### 2.2.1 Token传递至ActivityClientRecord

Token传递至ActivityClientRecord前面也已经看到了，所以这里就不重复了它是AMS将Token参数传递到App进程的Token代理端接收的第一站。

##### 2.2.1 Token传递至Activity

Token传递至Activity是调用它的attach()方法的，这个对于Activity启动流程熟悉的读者应该一点都不陌生了，我们来看看它的传递赋值流程如下:

```
//[Activity.java]
    final void attach(Context context, ActivityThread aThread,
            Instrumentation instr, IBinder token, int ident,
            Application application, Intent intent, ActivityInfo info,
            CharSequence title, Activity parent, String id,
            NonConfigurationInstances lastNonConfigurationInstances,
            Configuration config, String referrer, IVoiceInteractor voiceInteractor,
            Window window) {
            ...
            mToken = token;
            ...
     }
```

此时此刻的Activity就持有了我们的Token的Binder代理端了，就是如此的潇洒简单(不熟悉的朋友参见博客 [Android应用程序窗口设计之Activity窗口以及窗口管理器的创建](https://blog.csdn.net/tkwxty/article/details/109597570))。

##### 2.2.2 Token传递至Window

Token传递至Window是通过调用它的setWindowManager()方法来实现的，这个流程[Android应用程序窗口设计之Activity窗口以及窗口管理器的创建](https://blog.csdn.net/tkwxty/article/details/109597570))在该篇博客中有体现。

```
//[Window.java]

    public void setWindowManager(WindowManager wm, IBinder appToken, String appName,
            boolean hardwareAccelerated) {
        mAppToken = appToken;//将Token的Binder代理端赋予Window的mAppToken成员变量
        mAppName = appName;
        mHardwareAccelerated = hardwareAccelerated
                || SystemProperties.getBoolean(PROPERTY_HARDWARE_UI, false);
        if (wm == null) {
            wm = (WindowManager)mContext.getSystemService(Context.WINDOW_SERVICE);
        }
        mWindowManager = ((WindowManagerImpl)wm).createLocalWindowManager(this);
    }
```

> 这里我们可以看到每一个Window对象都有一个mAppToken变量，但是小伙伴注意区分Window和现实世界中窗口的概念（我们的Activity所对应的界面本质上就是一个DecorView，而Window是一个应用窗口的抽象只是被人叫做窗口而已，在Activity相关的VIew树组织结构中有一些操作是公共的，Window把这些公共行为抽象出来，是为Window）。
> 
> 在实际开发中，由于Window并不总是对应一个Activity，我们常见的Dialog，ContextMenu等等中也会包含一个Window，而这个时候Window中的mAppToken为空，否则mAppToken和Activity中的mToken是相同的。

##### 2.2.3 Token传递至WindowManager.LayoutParams

Token是怎么传递至WindowManager.LayoutParams中去的呢，这个就牵涉到Android应用程序窗口实现机制了，我们来看下传递流程如下：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/317f88b2d4e13b769dc08224fda0e03a.png)  
伪代码表示流程如下：

```
WindowManagerImpl.addView()--->
WindowManagerGlobal.addView()--->
```

我们接着往下看，注意此处的Window即我们前面的Activity对应的Window对象。

```
//[WindowManagerGlobal.java]
    public void addView(View view, ViewGroup.LayoutParams params,
            Display display, Window parentWindow) {
            
            ...
/*
注意此时的parentWindow为不为null，
指向的是Activity对应的PhoneWindow窗口
*/
if (parentWindow != null) {
            parentWindow.adjustLayoutParamsForSubWindow(wparams);
        } else {
...
            }
        }            
            ...
    }
```

我们接着继续分析

```
//[Window.java]
    void adjustLayoutParamsForSubWindow(WindowManager.LayoutParams wp) {
        CharSequence curTitle = wp.getTitle();

/*
判断窗口类型是不是子窗口
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
                wp.type <= WindowManager.LayoutParams.LAST_SYSTEM_WINDOW) {//当是系统窗口时token为null
...
        } else {
        /*
        最后一种类型，通常为Activity的窗口类型
        则设置token为当前窗口的IApplicationToken.Proxy代理对象，
        否则设置为父窗口的IApplicationToken.Proxy代理对象
        */
            if (wp.token == null) {
                wp.token = mContainer == null ? mAppToken : mContainer.mAppToken;
            }
        }
    }
```

这里我们可以看到此处的token的取值一共有三种情况：

-   如果我们创建的是一个应用窗口，比如Activity，那么这里的token的值和Window中的mAppToken的值相同，也就是和Activity的mToken的值相同，都是指向ActivityRecord中的Token的Binder实体对象。
    
-   如果我们创建的窗口为子窗口，即Dialog、PopupWindow等，那么token就为其父窗口的W对象
    
-   如果创建的是系统窗口，则对对应的token不做任何处理
    

  

#### 2.3 App进程—>WMS之间Token传递最后一棒

  Token传递的前两个分支已经讲解完毕了，这里到了最后的一条分支路线了就是App进程—>WMS之间token传递的传递(它是随着Android窗口添加的进行而进行的)，其传递的伪代码可以使用下述的流程来表示:

> 此处的Token和WindowManager.LayoutParams中的token是一致的

```
ViewRootImpl.setView()--->
mWindowSession.addToDisplay()--->
Session.addToDisplay()--->
WMS.addWindow()--->
```

这里我们对WMS服务的addWindow()方法展开来简单过下，看看它是怎么处理传递过来的Token实体对象(注意此时经过跨Bindr传递,Token已经从代理端转变成Token实体端了)！

```
//[WindowManagerService.java]
    public int addWindow(Session session, IWindow client, int seq,
            WindowManager.LayoutParams attrs, int viewVisibility, int displayId,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            InputChannel outInputChannel) {

//这里的client为IWindow的代理对象，用于WMS和Activity进行通信
...
//根据attrs.token从mWindowMap中取出应用程序窗口在WMS服务中的描述符WindowState
            WindowToken token = mTokenMap.get(attrs.token);
            AppWindowToken atoken = null;

//为Activity窗口创建WindowState对象
            WindowState win = new WindowState(this, session, client, token,
                    attachedWindow, appOp[0], seq, attrs, viewVisibility, displayContent);

//以键值对<IWindow.Proxy/Token,WindowToken>形式保存到mTokenMap表中
            if (addToken) {
                mTokenMap.put(attrs.token, token);
            }
            win.attach();
//以键值对<IWindow的代理对象,WindowState>形式保存到mWindowMap表中
            mWindowMap.put(client.asBinder(), win);
...
}
```

> 这里需要注意的是mTokenMap容器中在前面的2.1章节中已经有被填充过了，以Token为key，以构建的AppWindowToken(WindowToken的子类)为Value，所以这里取出来的就是前面的AppWindowToken实例对象。

我们接着来看下WindowState类的构造方法，这里需要注意的是传入的参数token为前面章节2.1中创建的AppWindowToken。

```
WindowState(WindowManagerService service, Session s, IWindow c, WindowToken token,
           WindowState attachedWindow, int appOp, int seq, WindowManager.LayoutParams a) {
        ...
        mToken = token; //WindowState引用到WindowToken

        WindowState appWin = this;
        while (appWin.isChildWindow()) {
            //一个Activity上可能有多个窗口，这里找到父窗口
            appWin = appWin.mAttachedWindow;  
        }
        WindowToken appToken = appWin.mToken; // 
        while (appToken.appWindowToken == null) {
            WindowToken parent = mService.mTokenMap.get(appToken.token);
            if (parent == null || appToken == parent) {
                break;
            }
            appToken = parent;
        }
        mRootToken = appToken;
        //这里mAppToken就是WindowToken的子类 AppWindowToken
        mAppToken = appToken.appWindowToken; 
...
}
```

至此Token在App进程—>WMS之间Token传递也就告一段落了！

  

#### 2.4 Token传递之旅小结

  Token传递之旅到这里就已经over了，它的传递之旅其实也是我们Activity从启动到用户界面展示的源码流程之旅，所以如果读者感兴趣可以依照上述的流程自行走读一番，你会发现别样的美！

上述分析Token传递我是站在进程的之间Binder的传递的角度来进行的，无意之间在博客中看到有位博主(但是可惜已经不从事Android开发了)有一张图是以类基本元素来描述Toekn的传递之旅，各位感兴趣的也可以拜读拜读写的也很不错，如下:

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d8c7948a7b3454137df70b74f405cdfb.png)

___

### 三.Token的作用

  我们先抛开Android为什么设计一个这个Token以及它的传递不说，我们透过现象看本质，通过前面章节一Token的诞生我们可以知道它是一个匿名Binder对象，那么它就逃不出匿名Binder的两大天生功能:

-   一个是拿到Binder代理端后可跨Binder调用实体端的方法接口
-   另一个作用便是在多个进程中标识同一个对象

上述两个作用是毋庸置疑Android的设计者在设计Token的时候一定考虑到了，而我们此处的Token还有另外一层的功能就是Android窗口添加的过程中用于判断窗口添加的类型，譬如添加的是系统窗口，还是应用，亦或者是子窗口，而我们本大章节也会从这三个角度分别举例说明。

#### 3.1 Token作为Binder服务端使用

  Token作为Binder服务端使用，那么自然是接收客户端的请求！那么这里的Token提供了那些服务呢，我们从章节一知道Token实现了IApplicationToken接口，我们来看看它提供了那些方法:

```
//[IApplicationToken.aidl]
interface IApplicationToken
{
    void windowsDrawn();
    void windowsVisible();
    void windowsGone();
    boolean keyDispatchingTimedOut(String reason);
    long getKeyDispatchingTimeout();
}
```

这里可以看到此处的方法分为两大类，一类是窗口有关系的，而另一类是和key分发超时有关系的！而Token的代理端正是通过Binder传递给AMS告知上述相关的结果，让AMS做出相关的处理。

我们接着看下Token作为IApplicationToken的Binder实体端，它是怎么处理上述代理端的调用的呢！

```
//[ActivityRecord.java]
    static class Token extends IApplicationToken.Stub {
        private final WeakReference<ActivityRecord> weakActivity;
        private final ActivityManagerService mService;

        Token(ActivityRecord activity, ActivityManagerService service) {
            weakActivity = new WeakReference<>(activity);
            mService = service;
        }

        @Override
        public void windowsDrawn() {
            synchronized (mService) {
                ActivityRecord r = tokenToActivityRecordLocked(this);//因为Token持有ActivityRecord的弱引用，找到对应的ActivityRecord
                if (r != null) {
                    r.windowsDrawnLocked();//然后通过ActivityRecord对windowsDrawnLocked事件做出相关的处理
                }
            }
        }
        ...
   }
```

慢着，此处Token的Binder实体是有了，那么谁会调用它呢！在我没有公布之前，读者约莫着思考下！好了，不多废话了我们来揭开谜底，如下：

```
wtoken.appToken.windowsDrawn()
appWindowToken.appToken.keyDispatchingTimedOut(reason)
wtoken.appToken.windowsVisible()
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d4bece0878e18ead527de017f7b384c3.png)  
好吗，通过上面可以看到是WMS用来向AMS传递Window和按键的一些信息(此处注意此时的Token并不是代理端，而是实体端他们是同属同一进程)！

  

#### 3.2 通过Token来标识对象

  Token来标识对象这个在多处有体现到，这个我们来分开说明.

##### 3.2.1 ActivityClientRecord中的Token

我们知道Activity在AMS中有ActivityRecord类用来标记它，而在我们的App进程中有ActivityClientRecord来标记它，但是一个App进程通常对应着的不单单是一个Activiyt那么就会存在许许多多的ActivityClientRecord，那么在AMS在对Activity管理过程中是怎么来找到正确的Activity呢，此时就需要通过Token来标识了。

```
//[ActivityRecord.java]
     public final void scheduleDestroyActivity(IBinder token, boolean finishing,
             int configChanges) {
         sendMessage(H.DESTROY_ACTIVITY, token, finishing ? 1 : 0,
                 configChanges);
     }
    private ActivityClientRecord performDestroyActivity(IBinder token, boolean finishing,
            int configChanges, boolean getNonConfigInstance) {
        ActivityClientRecord r = mActivities.get(token);
        mInstrumentation.callActivityOnDestroy(r.activity);
        ...
    }
```

即先通过Token的代理从mActivities列表中找到ActivityClientRecord ，然后再找到Activity，然后再执行相关的操作。

##### 3.2.2 Activity中的Token

这个过程和章节3.2.1的就是反过来了，譬如我们的Activiyt可能需要请求AMS服或者其它服务务执行具体功能，但是AMS服务或者其它服务怎么来确定是那个Activiyt发来的申请呢，这个就轮到了我们的token上场了，我在Activity中简单一搜就发现了不少！

```
ActivityManagerNative.getDefault().showLockTaskEscapeMessage(mToken);
WindowManagerGlobal.getInstance().setStoppedState(mToken, true);

    public void startActivityForResult(
            String who, Intent intent, int requestCode, @Nullable Bundle options) {
        Uri referrer = onProvideReferrer();
        if (referrer != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, referrer);
        }
        options = transferSpringboardActivityOptions(options);
        Instrumentation.ActivityResult ar =
            mInstrumentation.execStartActivity(
                this, mMainThread.getApplicationThread(), mToken, who,
                intent, requestCode, options);
        if (ar != null) {
            mMainThread.sendActivityResult(
                mToken, who, requestCode,
                ar.getResultCode(), ar.getResultData());
        }
        cancelInputsAndStartExitTransition(options);
    }
```

这里搜出来看到还多了一种就是startActivityForResult，它的作用是发起端希望在Activity启动时得到一些结果反馈，那么AMS在把Activity启动后，会把result传递给resultTo(mToken对应的那个Activity)，如此就完成了结果传递的作用！

  

#### 3.3 通过Token来标识窗口类型

  通过Token来标识窗口类型，这个不是直接通过Token来进行的，而是间接的。在WMS中的addWindow方法中，有如下的逻辑:

```
 public int addWindow(Session session, IWindow client, int seq,
            WindowManager.LayoutParams attrs, int viewVisibility, int displayId,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            InputChannel outInputChannel) {

//这里的client为IWindow的代理对象，用于WMS和Activity进行通信
//根据attrs.token从mWindowMap中取出应用程序窗口在WMS服务中的描述符WindowState
            WindowToken token = mTokenMap.get(attrs.token);
            AppWindowToken atoken = null;
if (token == null) {//第一次add的情况下token怎么会有值
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
                    // Apps targeting SDK above N MR1 cannot arbitrary add toast windows.
                    if (doesAddToastWindowRequireToken(attrs.packageName, callingUid,
                            attachedWindow)) {
                        Slog.w(TAG_WM, "Attempted to add a toast window with unknown token "
                                + attrs.token + ".  Aborting.");
                        return WindowManagerGlobal.ADD_BAD_APP_TOKEN;
                    }
                }
                token = new WindowToken(this, attrs.token, -1, false);
                addToken = true;
            } 
//应用程序窗口
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
                // Apps targeting SDK above N MR1 cannot arbitrary add toast windows.
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
            } else if (token.appWindowToken != null) {
                Slog.w(TAG_WM, "Non-null appWindowToken for system window of type=" + type);
                // It is not valid to use an app token with other system types; we will
                // instead make a new token for it (as if null had been passed in for the token).
                attrs.token = null;
                token = new WindowToken(this, null, -1, false);
                addToken = true;
            }
...

 }
```

可以看到以attrs.token为可以从mTokenMap中取出WindowToken然后来判断添加窗口的类型做不同的处理，而这里的attrs.token正是在前面章节2.2.3中传递过来的。

___

### 总结

  好了，普法Android的Token前世今生以及在APP,AMS,WMS之间传递到这里就基本告一段落了，通过本篇读者应该对Token的诞生以及传递和功能有了深刻的了解和认识了！而这篇博客也是为了深入WMS服务已经Android应用程序窗口设计深入必须跨过的坎坎，否则你会被WMS中各种Token给弄得晕头转向不知所踪！总之对于AMS中ActivityRecord中的Token我们要扣住其本质是一个匿名Binder那么它就逃不出匿名Binder的基本功能了，然后再辅以额外的功能就圆满了。好了，青山不改绿水长流先到这里了。如果本博客对你有所帮助，麻烦关注或者点个赞，如果觉得很烂也可以踩一脚！谢谢各位了！

参阅博客:  
[AMS\_WMS\_APP 中Token惟一性](https://www.jianshu.com/p/5e2efbaa2949)  
[深入理解Activity——Token之旅](https://blog.csdn.net/guoqifa29/article/details/46819377)