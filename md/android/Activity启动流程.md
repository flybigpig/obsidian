![](https://csdnimg.cn/release/blogv2/dist/pc/img/original.png)

[放你去吃草](https://blog.csdn.net/qq_36191896 "放你去吃草") ![](https://csdnimg.cn/release/blogv2/dist/pc/img/newUpTime2.png) 已于 2022-03-02 11:43:07 修改

于 2022-03-02 11:39:24 首次发布

版权声明：本文为博主原创文章，遵循 [CC 4.0 BY-SA](http://creativecommons.org/licenses/by-sa/4.0/) 版权协议，转载请附上原文出处链接和本声明。

## **前言**

Activity启动流程分两种

-   一种是启动正在运行的app的Activity，即启动子Activity。如无特殊声明默认和启动该activity的activity处于同一进程。如果有声明在一个新的进程中，则处于两个进程。
-   另一种是打开新的app，即为Launcher启动新的Activity。

后边启动Activity的流程是一样的，区别是前边判断进程是否存在的那部分。

涉及到的进程

-   Launcher所在的进程
-   AMS所在的SystemServer进程
-   要启动的Activity所在的app进程

如果是启动根Activity，就涉及上述三个进程。

如果是启动子Activity，那么就只涉及[AMS](https://so.csdn.net/so/search?q=AMS&spm=1001.2101.3001.7020)进程和app所在进程。

## 流程

1\. [Launcher](https://so.csdn.net/so/search?q=Launcher&spm=1001.2101.3001.7020)（也是一个APP）：Launcher通知AMS要启动activity。

-   startActivitySafely->
-   startActivity->
-   Instrumentation.execStartActivity()(AMP.startActivity)->
-   AMS.startActivity

2\. AMS:[PMS](https://so.csdn.net/so/search?q=PMS&spm=1001.2101.3001.7020)的resoveIntent验证要启动activity是否匹配。如果匹配，通过ApplicationThread发消息给Launcher所在的主线程，暂停当前Activity(即Launcher)。

3\. 暂停完，在该activity还不可见时，通知AMS，根据要启动的Activity配置ActivityStack。然后判断要启动的Activity进程是否存在?

-   存在：发送消息LAUNCH\_ACTIVITY给需要启动的Activity主线程，执行handleLaunchActivity
-   不存在：通过socket向zygote请求创建进程。进程启动后，ActivityThread.attach

4. 判断Application是否存在，若不存在，通过LoadApk.makeApplication创建一个。在主线程中通过thread.attach方法来关联ApplicationThread。

5\. 在通过ActivityStackSupervisor来获取当前需要显示的ActivityStack。

6\. 继续通过ApplicationThread来发送消息给主线程的Handler来启动Activity（handleLaunchActivity）。

7. handleLauchActivity：调用了performLauchActivity，里边Instrumentation生成了新的activity对象，继续调用activity生命周期。

## 代码执行

Activity启动流程：

Launcher通知AMS要启动新的Activity（在Launcher所在的进程执行）

-   Launcher.startActivitySafely //首先Launcher发起启动Activity的请求
-   Activity.startActivity
-   Activity.startActivityForResult
-   Instrumentation.execStartActivity //交由Instrumentation代为发起请求
-   ActivityManager.getService().startActivity //通过IActivityManagerSingleton.get()得到一个AMP代理对象
-   ActivityManagerProxy.startActivity //通过AMP代理通知AMS启动activity

AMS先校验一下Activity的正确性，如果正确的话，会暂存一下Activity的信息。然后，AMS会通知Launcher程序pause Activity（在AMS所在进程执行）

-   ActivityManagerService.startActivity
-   ActivityManagerService.startActivityAsUser
-   ActivityStackSupervisor.startActivityMayWait
-   ActivityStackSupervisor.startActivityLocked ：检查有没有在AndroidManifest中注册
-   ActivityStackSupervisor.startActivityUncheckedLocked
-   ActivityStack.startActivityLocked ：判断是否需要创建一个新的任务来启动Activity。
-   ActivityStack.resumeTopActivityLocked ：获取栈顶的activity，并通知Launcher应该pause掉这个Activity以便启动新的activity。
-   ActivityStack.startPausingLocked
-   ApplicationThreadProxy.schedulePauseActivity

pause Launcher的Activity，并通知AMS已经paused（在Launcher所在进程执行）

-   ApplicationThread.schedulePauseActivity
-   ActivityThread.queueOrSendMessage
-   H.handleMessage
-   ActivityThread.handlePauseActivity
-   ActivityManagerProxy.activityPaused

检查activity所在进程是否存在，如果存在，就直接通知这个进程，在该进程中启动Activity；不存在的话，会调用Process.start创建一个新进程（执行在AMS进程）

-   ActivityManagerService.activityPaused
-   ActivityStack.activityPaused
-   ActivityStack.completePauseLocked
-   ActivityStack.resumeTopActivityLocked
-   ActivityStack.startSpecificActivityLocked
-   ActivityManagerService.startProcessLocked
-   Process.start //在这里创建了新进程，新的进程会导入ActivityThread类，并执行它的main函数

创建ActivityThread实例，执行一些初始化操作，并绑定Application。如果Application不存在，会调用LoadedApk.makeApplication创建一个新的Application对象。之后进入Loop循环。（执行在新创建的app进程）

-   ActivityThread.main
-   ActivityThread.attach(false) //声明不是系统进程
-   ActivityManagerProxy.attachApplication

处理新的应用进程发出的创建进程完成的通信请求，并通知新应用程序进程启动目标Activity组件（执行在AMS进程）

-   ActivityManagerService.attachApplication //AMS绑定本地ApplicationThread对象，后续通过ApplicationThreadProxy来通信。
-   ActivityManagerService.attachApplicationLocked
-   ActivityStack.realStartActivityLocked //真正要启动Activity了！
-   ApplicationThreadProxy.scheduleLaunchActivity //AMS通过ATP通知app进程启动Activity

加载MainActivity类，调用onCreate声明周期方法（执行在新启动的app进程）

-   ApplicationThread.scheduleLaunchActivity //ApplicationThread发消息给AT
-   ActivityThread.queueOrSendMessage
-   H.handleMessage //AT的Handler来处理接收到的LAUNCH\_ACTIVITY的消息
-   ActivityThread.handleLaunchActivity
-   ActivityThread.performLaunchActivity
-   Instrumentation.newActivity //调用Instrumentation类来新建一个Activity对象
-   Instrumentation.callActivityOnCreate
-   MainActivity.onCreate
-   ActivityThread.handleResumeActivity
-   AMP.activityResumed
-   AMS.activityResumed(AMS进程)

[参考借鉴：  
Android应用程序启动过程源代码分析\_老罗的Android之旅-CSDN博客\_android应用启动过程](https://blog.csdn.net/luoshengyang/article/details/6689748 "参考借鉴：Android应用程序启动过程源代码分析_老罗的Android之旅-CSDN博客_android应用启动过程")  
[Activity启动流程\_进程](https://www.sohu.com/a/292120101_494949 "Activity启动流程_进程")