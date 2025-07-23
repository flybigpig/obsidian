
> Android 在 Java 层弱化了进程的概念，建立了四大组件及其配套框架。这套框架中最核心的组件就是 AMS，在 Android10 中，AMS 的部分功能迁移到了 ATMS。接下来我们通过分析四大组件的启动过程来了解 AMS/ATMS 的内部实现。我们首先分析 Activity 的启动过程，Activity 主要由 ATMS 管理。


英文缩写说明：

- AMS：ActivityManagerService
- ATMS：ActivityTaskManagerService


### 1. 整体流程

Activity 启动过程非常复杂，涉及多种情况的处理，有各种各样的分支，全盘分析很容易迷失在源码中，我们针对具体的 App [冷启动](https://so.csdn.net/so/search?q=%E5%86%B7%E5%90%AF%E5%8A%A8&spm=1001.2101.3001.7020)场景进行分析，梳理出主干流程：

- 使用 Android Studio 新建一个空项目，将其安装到模拟器中
- 点击 Launcher 中的图标，启动这个 App

Activity 的冷启动过程涉及到多个进程：

- 源 App 进程，一般是 Launcher
- SystemServer 进程
- Zygote 进程
- 目标 App 进程

启动的整体流程如下：

![91d28580d975c41e8ad683b13ffd3b39.png](https://img-blog.csdnimg.cn/img_convert/91d28580d975c41e8ad683b13ffd3b39.png)

1. 用户点击 App 图标，Launcher 进程启动目标 Activity
2. SystemServer 中的 AMS/ATMS 收到请求，创建对应的 ActivityRecord 和 Task，并挂载到窗口层级树中
3. AMS/ATMS pause 源 Activity
4. 源 Activity pause 完成后，告知 AMS/ATMS pause 过程完成，AMS/ATMS 通知到 Zygote 创建新进程
5. 目标 App 进程启动后，向 AMS/ATMS attach 当前进程信息
6. AMS/ATMS 远程调用到 app ，app 初始化 Application，执行 onCreate 生命周期方法，初始化 Activity，执行 onCreate OnResume 等生命周期方法

### 2.Binder 通信通道

在分析代码之前我们需要了解 App（包括了源 App 与目标 App） 与 SystemServer 之间的 Binder 通信通道。

#### 2.1 App 访问 ATMS

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

![f8298352621bb36133047ea7a5c585aa.png](https://img-blog.csdnimg.cn/img_convert/f8298352621bb36133047ea7a5c585aa.png)

ATMS 通过 AIDL 实现，相关类的类图如下：

![9ca527ad7d34d694e42dbf78435decbe.png](https://img-blog.csdnimg.cn/img_convert/9ca527ad7d34d694e42dbf78435decbe.png)

#### 2.2 ATMS 访问 App

在 App 进程启动的过程中，会初始化一个匿名 Java Binder 服务 ApplicationThread，ATMS 可以通过调用 ApplicationThread 的 Binder 客户端对象提供的接口，远程调用到 App 端，更新 Activity 的状态：

```cobol
bindApplicationscheduleTransactionscheduleLowMemoryscheduleSleeping//......
```

![228438728be18bc8271685387b79f58c.png](https://img-blog.csdnimg.cn/img_convert/228438728be18bc8271685387b79f58c.png)

此时，App 进程是服务端，SystemServer 是客户端。也就是说 App 和 SystemServer 互为客户端服务端。

ApplicationThread 同样基于 AIDL 实现，相关类的类图如下：

![f1fdc73bde71fc6090528ad76400015f.png](https://img-blog.csdnimg.cn/img_convert/f1fdc73bde71fc6090528ad76400015f.png)


#### 客户端流程


ATMS 内部代码非常繁琐，涉及多种情景的处理，直接分析代码非常容易迷失。这里给出三个相对简单的具体场景：

- 情景一：从 Launcher 页面点击 App 图标启动一个全新的 App
- 情景二：在应用内从 Activity A 跳转到 Activity B
- 情景三：启动 App 后，按 Home 键回到 Launcher ，再点击 App 图标

接下来，我们以情景一为例，分析 Activity 的启动过程：

Activity 启动是一个 RPC 过程，涉及到 App 进程和 SystemServer 进程，本节我们主要关心 App 进程（客户端）中的流程。

  