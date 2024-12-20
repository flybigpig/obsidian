本文基于 android-14.0.0\_r15 版本讲解

英文缩写说明：

-   AMS：ActivityManagerService
-   ATMS：ActivityTaskManagerService

Android 在 Java 层弱化了进程的概念，建立了四大组件框架。这套框架中最核心的组件就是 AMS，在 Android10 及以后，AMS 的部分功能迁移到了 ATMS。接下来我们通过分析四大组件的启动过程来了解 AMS/ATMS 的内部实现。我们首先分析 Activity 的启动过程。

## [#](http://ahaoframework.tech/008.%E5%BA%94%E7%94%A8%E5%B1%82%E6%A1%86%E6%9E%B6/001.Activity%20%E5%90%AF%E5%8A%A8%E5%88%86%E6%9E%90%E4%B9%8B%E6%95%B4%E4%BD%93%E6%B5%81%E7%A8%8B.html#_1-%E6%95%B4%E4%BD%93%E6%B5%81%E7%A8%8B) 1. 整体流程

Activity 启动过程非常复杂，涉及多种情况的处理，有各种各样的分支，全盘分析很容易迷失在源码中，我们针对具体的 App 冷启动场景进行分析，梳理出主干流程：

-   使用 Android Studio 新建一个空项目，将其安装到模拟器中
-   点击 Launcher 中的图标，启动这个 App

Activity 的冷启动过程涉及到多个进程：

-   源 App 进程，一般是 Launcher
-   SystemServer 进程
-   Zygote 进程
-   目标 App 进程

启动的整体流程如下：
![[Pasted image 20241220132244.png]]
![20240830182218](https://frameworkpictures.oss-cn-beijing.aliyuncs.com/20240830182218.png)

1.  用户点击 App 图标，Launcher 进程启动目标 Activity
2.  SystemServer 中的 AMS/ATMS 收到请求，创建对应的 ActivityRecord 和 Task，并挂载到窗口层级树中
3.  AMS/ATMS pause 源 Activity
4.  源 Activity pause 完成后，告知 AMS/ATMS pause 过程完成，AMS/ATMS 通知到 Zygote 创建新进程
5.  目标 App 进程启动后，向 AMS/ATMS attach 当前进程信息
6.  AMS/ATMS 远程调用到 app ，app 初始化 Application，执行 onCreate 生命周期方法，初始化 Activity，执行 onCreate OnResume 等生命周期方法

## [#](http://ahaoframework.tech/008.%E5%BA%94%E7%94%A8%E5%B1%82%E6%A1%86%E6%9E%B6/001.Activity%20%E5%90%AF%E5%8A%A8%E5%88%86%E6%9E%90%E4%B9%8B%E6%95%B4%E4%BD%93%E6%B5%81%E7%A8%8B.html#_2-binder-%E9%80%9A%E4%BF%A1%E9%80%9A%E9%81%93) 2.Binder 通信通道

在分析代码之前我们需要了解 App（包括了源 App 与目标 App） 与 SystemServer 之间的 Binder 通信通道。

### [#](http://ahaoframework.tech/008.%E5%BA%94%E7%94%A8%E5%B1%82%E6%A1%86%E6%9E%B6/001.Activity%20%E5%90%AF%E5%8A%A8%E5%88%86%E6%9E%90%E4%B9%8B%E6%95%B4%E4%BD%93%E6%B5%81%E7%A8%8B.html#_2-1-app-%E8%AE%BF%E9%97%AE-atms) 2.1 App 访问 ATMS

SystemServer 在启动时会注册一个 Java Binder 服务 ATMS：

```
// /frameworks/base/services/java/com/android/server/SystemServer.java
// # SystemServer
private void startBootstrapServices(@NonNull TimingsTraceAndSlog t) {
    //......
    ActivityTaskManagerService atm = mSystemServiceManager.startService(
                ActivityTaskManagerService.Lifecycle.class).getService();
    //......
}
```



ATMS 的主要作用是作为服务端向客户端 App 提供管理 Activity 的接口：

```
startActivity
finishActivity
activityResumed
activityPaused
activityStopped
activityDestroyed
// ......
```



App 进程作为客户端通过 Binder RPC 调用到这些方法，实现 Activity 的管理：

![20240112180519](https://cdn.jsdelivr.net/gh/stingerzou/MyImages@main/images20240112180519.png)

ATMS 通过 AIDL 实现，相关类的类图如下：

![20240103154533](https://cdn.jsdelivr.net/gh/stingerzou/MyImages@main/images20240103154533.png)

### [#](http://ahaoframework.tech/008.%E5%BA%94%E7%94%A8%E5%B1%82%E6%A1%86%E6%9E%B6/001.Activity%20%E5%90%AF%E5%8A%A8%E5%88%86%E6%9E%90%E4%B9%8B%E6%95%B4%E4%BD%93%E6%B5%81%E7%A8%8B.html#_2-2-atms-%E8%AE%BF%E9%97%AE-app) 2.2 ATMS 访问 App

在 App 进程启动的过程中，会初始化一个匿名 Java Binder 服务 ApplicationThread，ATMS 可以通过调用 ApplicationThread 的 Binder 客户端对象提供的接口，远程调用到 App 端，更新 Activity 的状态：

```
bindApplication
scheduleTransaction
scheduleLowMemory
scheduleSleeping
//......
```


![20240112180736](https://cdn.jsdelivr.net/gh/stingerzou/MyImages@main/images20240112180736.png)

此时，App 进程是服务端，SystemServer 是客户端。也就是说 App 和 SystemServer 互为客户端服务端。

ApplicationThread 同样基于 AIDL 实现，相关类的类图如下：

![20240103154559](https://cdn.jsdelivr.net/gh/stingerzou/MyImages@main/images20240103154559.png)

## [#](http://ahaoframework.tech/008.%E5%BA%94%E7%94%A8%E5%B1%82%E6%A1%86%E6%9E%B6/001.Activity%20%E5%90%AF%E5%8A%A8%E5%88%86%E6%9E%90%E4%B9%8B%E6%95%B4%E4%BD%93%E6%B5%81%E7%A8%8B.html#%E5%8F%82%E8%80%83%E8%B5%84%E6%96%99) 参考资料

-   [Activity启动流程(一)发起端进程请求启动目标Activity (opens new window)](https://blog.csdn.net/tkwxty/article/details/108680198)
-   [Android13 Activity启动流程 (opens new window)](https://blog.csdn.net/ss520k/article/details/129147496)
-   [Android四大组件之Activity启动流程源码实现详解概要 (opens new window)](https://blog.csdn.net/tkwxty/article/details/108652250)
-   [Android应用启动全流程分析（源码深度剖析） (opens new window)](https://www.jianshu.com/p/37370c1d17fc)
-   [【Android 14源码分析】Activity启动流程-1 (opens new window)](https://juejin.cn/post/7340301649766727721)