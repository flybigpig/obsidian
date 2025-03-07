##    Android四大组件之Activity启动流程源码实现详解概要

Android四大组件源码实现详解系列博客目录:

> [Android应用进程创建流程大揭秘](https://blog.csdn.net/tkwxty/article/details/106941015)  
> \[[Android四大组件之bindService源码实现详解](https://blog.csdn.net/tkwxty/article/details/108628884)  
> [Android四大组件之Activity启动流程源码实现详解概要](https://blog.csdn.net/tkwxty/article/details/108652250)  
> [Activity启动流程(一)发起端进程请求启动目标Activity](https://blog.csdn.net/tkwxty/article/details/108680198)  
> [Activity启动流程(二)system\_server进程处理启动Activity请求](https://blog.csdn.net/tkwxty/article/details/108745996)  
> [Activity启动流程(三)-Activity Task调度算法复盘分析](https://blog.csdn.net/tkwxty/article/details/108995543)  
> [Activity启动流程(四)-Pause前台显示Activity，Resume目标Activity](https://blog.csdn.net/tkwxty/article/details/108850530)  
> [Activity启动流程(五)请求并创建目标Activity进程](https://blog.csdn.net/tkwxty/article/details/109115397)  
> [Activity启动流程(六)注册目标Activity进程到system\_server进程以及创建目标Activity进程Application](https://blog.csdn.net/tkwxty/article/details/109304542)  
> [Activity启动流程(七)初始化目标Activity并执行相关生命周期流程](https://blog.csdn.net/tkwxty/article/details/109444690)

___

本篇博客编写思路总结和关键点说明:

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/74ab8dfb2b32015d074b529d7cf8d5f0.png)  
为了更加方便的读者阅读博客，通过导读思维图的形式将本博客的关键点列举出来，从而方便读者取舍和阅读！

___

  

### 引言

  一直在筹划着写一个系列的博客关于Activity启动流程详解的，但是一直自我感觉功力不够，不是因为本人不够自信，真的是事出有因啊！Activity的启动涉及到非常多的逻辑，其启动流程是比较复杂的，它涉及到了Android进程的创建，涉及到了Activity中的生命周期的调度，涉及到了Task栈的选择和调度，更加涉及到了Android中进程通信Binder IPC机制等等方方面面的知识点！对于这一些列的博客我已经准备了良久,并且对于其中将要涉及的重点知识已经提前打好了基础：

-   关于Android Binder方面的知识我准备了系列博客[Android Binder框架实现](https://blog.csdn.net/tkwxty/article/details/103974368)详细介绍了Android的Binder框架
-   关于Android进程的创建准备了博客[Android应用进程创建流程大揭秘](https://blog.csdn.net/tkwxty/article/details/106941015)，详细介绍了Android应用进程创建的流程
-   同时为了让小伙们来个热身特意提前选择了Android四大组件中的Service，以bindService为例详细分析了其流程，详见博客[Android四大组件之bindService源码实现详解](https://blog.csdn.net/tkwxty/article/details/108628884)

虽然有了上述如此多的准备，说句心里话写本篇系列博客还是心里非常忐忑，不为别的主要担心自己功力不够，怕写出的东西有纰漏或者错误之处，但是还是硬着头皮往下干了！至少，我是在用心写着！当然如果能帮到小伙们那是最开心不过的了！

-   **注意**：本篇的介绍是基于Android 7.xx平台为基础的，其中涉及的代码路径如下：

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

frameworks/base/services/core/java/com/android/server/pm、
 --- PackageManagerService.java

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
```

-   并且在后续的源码分析过程中为了简述方便，会将做如下简述:  
    ApplicationThreadProxy简称为ATP  
    ActivityManagerProxy简称为AMP  
    ActivityManagerService简称为AMS  
    ActivityManagerNative简称AMN  
    ApplicationThreadNative简称ATN  
    PackageManagerService简称为PKMS  
    ApplicationThread简称为AT  
    ActivityStarter简称为AS,这里不要和ActivityServices搞混淆了  
    ActivityStackSupervisor简称为ASS

___

### 一. Activity启动的整体概括和前期知识准备

  在写该系列博客的时候，无意中看到了一篇微信号的文章[我为什么这么讨厌你一行行地讲代码？](https://mp.weixin.qq.com/s/eDyxLpRZ4pXb73U34vGi-g)，其中最后一段话说的很在理！

>   每次，我看到这样的技术文章也是无比的蛋疼。你一上给我一行行讲代码是什么意思？难道我看不懂C语言？你能不能高屋建瓴地先给我讲一讲你要写什么东西？你写的东西整体框架是什么？至于代码，你给我点几个关键点、函数名或者流程图，我自己去撸行吗？  
>   记流水账的文章为什么对读者的帮助一般都不大？因为它真地是事倍功半。我花了很长地时间去读你，读完还是不知所云，因为全部是细节，因为我没形成整体的轮廓。

是的，这种博客看着写了非常多的东西，但是读者读起来感觉啥都有，但是又啥都没有！所以在本系列博客中，我们将抛弃一行行分析代码，着重重点的脉络，对于细枝末节如果读者感兴趣可以在将整体知识点理清楚的基础上再慢慢深入，遵循读书先将书变薄，再变厚的理论而发散进行！

  

#### 1.1 Activity启动的整体概括

  在正式开始Activity漫长而又复杂的启动流程分析之前，让我们先来高屋建瓴的整体概括一下Activity的启动流程(以冷启动为例),其大致可以划分为如下阶段：

-   **Souruce端进程发送请求目标Activity启动阶段**：  
     **1**.Source端发起显示/隐式启动请求启动Activity
    
-   **system\_server进程通过AMS处理启动Activity请求**：  
     **2**.解析启动目标Activity的Intent  
     **3**.创建目标Activity对应的ActivityRecord  
     **4**.为目标Activity查找/分配或者创建最优Task栈  
     **5**.Pause前台Activity  
     **6**.Resume请求的目标Activity  
     **7**.AMS请求zygote进程为目标Activity创建所属进程
    
-   **zygote进程处理system\_server进程发送的创建目标Activity进程请求阶段**：  
     **8**.zygote接受AMS请求fork创建目标Activity所属进程  
     **9**.调用RuntimeInit，初始化目标进程运行环境  
     **10**.通过反射调用目标进程ActivityThread主线程main方法，开启目标进程新时代
    
-   **目标Activity进程启动阶段**：  
     **11**.开启目标Activity进程的的Looper消息循环  
     **12**.注册ApplicationThread到system\_server进程  
     **13**.创建目标进程Application，并执行其onCreate方法
    
-   **开启目标Activity生命周期阶段**：  
     **14**.真正启动目的端Activity  
     **15**.通过反射加载目标Activity  
     **16**.执行目标Activity生命周期  
     **17**.初始化目标Activity窗口为显示做准备
    
-   **目标Activity显示阶段**：  
     **18**.新建DecorView  
     **19**.新建ViewRootImpl  
     **20**.将window添加到WMS准备显示
    
-   **Source端Activity收尾工作**：  
     **21**.Source端Activity执行onStop()等逻辑
    

在接下来的系列博客中，我们将依照上面的提纲出发，通过伪代码结合重点方法或者函数的分析来展开Activity启动流程的讲解！带领小伙们能在整体上对Activity启动流程有一个脉络清晰的理解，至于其中涉及到的各种小细节就靠各位小伙们自行去撸了！其启动流程时序图如下所示(其中只包含了部分的主分支，细节没有放出来)：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/cf711a0f2f9f3613b821388c9dcef011.png#pic_center)

___

### 二. Activity启动前期知识准备

  虽然我们前面已经为Activity的启动储备了一些知识，但是还是远远不够，Activity的启动过程中涉及到的许多概念需要我们提前熟悉熟悉,磨刀不误砍柴工吗！

#### 2.1 Activity冷启动和热启动

  我想小伙们经常会看到一些博客提到Activity的冷热启动，但是有些小伙们可能是第一次听到这个概念，这里的冷热启动可不同于开车中的冷热启动，让我们对Android中Activity中的冷热启动这两种方式简单描述一下：

##### 2.1.1 Activity冷启动

>   当启动该Activity时，后台没有该Activity对应的应用的进程存在，这时Android系统会为该Activity创建对应进程，然后接着执行Activity创建和显示的流程，这个启动方式就是冷启动。其最主要的特点就是冷启动因为系统会通过zygote创建一个新的进程分配给它，所以会先创建和初始化Application类，再创建和初始化目标Activity类（包括一系列的测量、布局、绘制），最后显示在界面上。

##### 2.1.2 Activity热启动

>   当启动目标Activity时候，Android后台已有该Activity对应的应用的进程（例：按back键、home键，应用虽然会退出，但是该应用的进程是依然会保留在后台，可进入任务列表查看），所以在已有进程的情况下，这种启动会从已有的进程中来启动应用，这个方式叫热启动。该种启动方式最大的特点就是一个App应用从新进程的创建到进程的销毁，Application只会初始化一次，所以不必创建和初始化Application，直接走目标Activity的创建和显示（包括一系列的测量、布局、绘制）。

  

#### 2.2 Activity的生命周期

  这个我想是很多小伙们刚入门Android时候必备的一个知识点了，即Activity的生命周期，其如下所示：

> -   protected void onCreate(Bundle savedInstanceState)
> -   protected void onRestart(…)
> -   protected void onStart(…)
> -   protected void onResume(…)
> -   protected void onPause(…)
> -   protected void onStop(…)
> -   protected void onDestory(…)

以上为Activity生命周期中的各个时期的回调方法，在不同的方法中我们可以执行不同的逻辑,其经典的示意图如下：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/e1b11b8d72f2a90b29467499e49d4456.png#pic_center)

  

#### 2.3 Activity的显示启动和隐式启动

  众所周知 Android组件的启动模式有两种 显式调用和隐式调用，这两种调用方式从字面意思就很好理解。我们分别来说明一下。

##### 2.3.1 显示启动

  显式调用需要指定被启动的组件比如：

```
Intent intent = new Intent();
intent.setClass(this,XXXActivity.class);
startActivity(intent);
```

这里可以看到显示启动就是在初始化Intent对象的时候直接引用需要启动的Activity的字节码，显示启动的好处就是可以直接告诉Intent对象启动的Activity对象不需要执行intent filter索引需要启动哪一个Activity。网上很多博客说显示启动不能启动其他进程的Activity对象，因为无法获取其他进程的Activity对象的字节码，这个说法我认为是不正确的,譬如下面所示，这不就是明显跨进程启动了远端进程的Activity了吗!

```
 Intent intent=new Intent();
 ComponentName cName = new ComponentName("com.android.settings","com.android.settings.Settings$WifiSettingsActivity");
 intent.setComponent(cName);
 startActivity(intent);
```

##### 2.3.2 隐式启动

  隐式调用无需要指定被启动的组件，而是通过Action进行匹配比如：

```
        Intent intent = new Intent();
        intent.setAction("xxx.xxx.xxx");
        startActivity(intent);
```

这里我们可以看到隐式启动通过配置Action启动带有该Action配置的intent-filte的Activity，通常隐式启动用于那些不知道组件具体信息，但是知道对外提供Action的组件的启动。

  

#### 2.4 Activity的启动模式

  这个知识点也是面试时候，考官一般喜欢用的，并且也是我们经常在实战的时候运用比较多的一个只是点。Activity在\\AndroidManifest文件中定义Activity的时候，你可以通过元素的launchMode属性来指定这个Activity应该如何与任务进行关联，其取值有如下四种：

-   standard(默认的启动模式)
-   singleTop
-   singleTask
-   singleInstance

不同的启动模式在启动Activity时会执行不同的逻辑，系统会按不同的启动模式将Activity存放到不同的Activity栈中，关于上述四种模式的详解介绍可以参见谷歌文档[了解任务和返回堆栈](https://developer.android.google.cn/guide/components/activities/tasks-and-back-stack)。

  

#### 2.5 ActivityManagerService类图浅析

  ActivityManagerService是Android提供负责系统中四大组件的启动、切换、调度及应用进程的管理和调度等工作的Java层Binder服务，但是其代码繁琐而冗长，如果不对其进行提前熟悉，那么后面的分析中会晕头转向的。我们先看看其类图实现，如下：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/12ce40ea603ea4724f8d814b5e60b3ec.png#pic_center)

下面对其中涉及的类功能简单介绍一下：

-   **ActivityManagerService**  
    Activity管理机制的Binder服务端，属于一个系统服务。用于管理Activity的各种行为，控制Activity的生命周期，派发消息事件，低内存管理等等。实现了IBinder接口，可以用于进程间通信
    
-   **ActivityManagerProxy**  
    AMS服务代理端，第三方应用借助该类实现对AMS的远程RPC请求
    
-   **ActivityStarter**  
    AMS要处理的事情太多了，分身泛术！所以创造了ActivityStarter用来负责处理Activity的Intent和Flags, 还有关联相关的Stack和TaskRecord
    
-   **ActivityStack**  
    为了让许多 Activity协同工作而不至于产生混乱，Android平台设计了一种堆栈机制用于管理Activity，其遵循先进后出的原则，系统总是显示位于栈顶的Activity
    
-   **ActivityStackSupervisor**  
    负责所有Activity栈的管理。内部管理了mHomeStack、mFocusedStack和mLastFocusedStack三个Activity栈。其中，mHomeStack管理的是Launcher相关的Activity栈；mFocusedStack管理的是当前显示在前台Activity的Activity栈；mLastFocusedStack管理的是上一次显示在前台Activity的Activity栈
    

  

#### 2.6 ApplicationThread类图浅析

  在AMS借助zygote进程创建完目标进程以后，需要借助一个中介完成对目标进程组件的控制，这个就轮到了我们的ApplicationThread上场了，AMS正是通过它完成了对目标进程四大组件的管理和调度，让我们看看其类图:

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/4774f720eef6802b6d6c7827ef458bf6.png#pic_center)  
下面对其中涉及的类功能简单介绍一下：

-   **ApplicationThread**  
    可以看到其是一个Binder实体对象，主要负责与AMS的通信接收AMS的各种RPC指令，进而完成对四大组件的管理和控制  
    **ApplicationThreadProxy**  
    ApplicationThread服务代理端，第三方通常借助其完成对ApplicationThread的远程调用
-   **ActivityThread**  
    里面有进程主线程的入口，类似于main的功能，负责建立Looper循环，同时协助AMS的四大组建、生命周期的各种调度

  

#### 2.7 ActivityRecord、TaskRecord、ActivityStack简介

  在Activity的启动中涉及到几个非常重要的数据结构，那就是ActivityRecord，TaskRecord，ActivityStack和ProcessRecord，关于这几个数据结构的关系可以用如下的示意关系来表示：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/330eec503ee8c45f4a384b14861644bb.png#pic_center)  
在这里我们心中只需对上述类图有一个概括，至于它们是怎么串联起来的暂且不表，待后续源码中会有提到，我们先对其单个拎出来简单介绍一下：

-   **ProcessRecord**  
    该数据结构和我们的App进程相对应，其对应一个或者多个ActivityRecord（当然可能还有Service等其他四大组件中的其它）
-   **ActivityRecord**  
    顾名思义，该数据结构和我们的Activiyt相对应，存储者Activiyt的相关信息，并且每个ActivityRecord会对应到一个TaskRecord，ActivityRecord中类型为TaskRecord的成员task，记录所属的Task，这里有一点需要注意的是Activity和ActivityRecord并不是一对一的，而是一对多，因为一个Actitiy可能存在多个启动方式进而导致存在多个ActivityRecord
-   **TaskRecord**  
    一个TaskRecord由一个或者多个ActivityRecord组成，这就是我们常说的任务栈，具有后进先出的特点
-   **ActivityStack**  
    用来管理TaskRecord，它有一个ArrayList类型的成员mTaskHistory，用于存储TaskRecord
-   **ActivityStackSupervisor**  
    负责所有Activity栈的管理。内部管理了mHomeStack、mFocusedStack和mLastFocusedStack三个Activity栈。其中，mHomeStack管理的是Launcher相关的Activity栈；mFocusedStack管理的是当前显示在前台Activity的Activity栈；mLastFocusedStack管理的是上一次显示在前台Activity的Activity栈

好了这里先就不深入了，先让小伙们有一个基本概念。关于上述的知识点强烈推荐博客[ActivityRecord、TaskRecord、ActivityStack以及Activity启动模式详解](https://www.jianshu.com/p/94816e52cd77)，你值得拥有！

___

### 小结

  没有想到，Activity启动的前期知识准备就写了这么多了，为了小伙们阅读的方便和排版的美观详解一就先到这里了，如果想真的了解Acitivity启动的源码级别的小伙们一定要做好前期准备不然在阅读中你会越来越迷茫和无助，读着读着就有可能放弃了，因为你会发现咋这么难啊，左一个知识点，有一个知识点的。不要问我为啥这么说，因为我也是这么过来的，也迷惘和无助过心里想这啥玩意啊！所以小伙们一定要做好心理准备，千万不能半途而废！在接下来的篇章中我们就要正式的开干进入实战分析了，希望小伙们能继续关注。关于Activity的启动详细分解，详见[Android四大组件之Activity启动流程源码实现详解(一)](https://editor.csdn.net/md/?articleId=108680198)。