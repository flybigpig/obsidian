
> Android 在 Java 层弱化了进程的概念，建立了四大组件及其配套框架。这套框架中最核心的组件就是 AMS，在 Android10 中，AMS 的部分功能迁移到了 ATMS。接下来我们通过分析四大组件的启动过程来了解 AMS/ATMS 的内部实现。我们首先分析 Activity 的启动过程，Activity 主要由 ATMS 管理。


#### ATMS

首先明确 Activity 的启动过程涉及到两个进程，App 进程和 SystemServer 进程，是一个客户端服务端模型。SystemServer 中注册了一个 Java Binder 服务 ATMS，其主要作用是作为服务端向 App 提供管理 Activity 的接口：

```
startActivity
finishActivity
activityResumed
activityPaused
activityStopped
activityDestroyed
```

App 进程作为客户端通过 Binder RPC 调用到这些方法，实现 Activity 的管理：

ATMS 通过 AIDL 实现

在 App 进程启动的过程中，会启动一个匿名 Java Binder 服务 ApplicationThread，ATMS 可以通过调用 ApplicationThread 提供的接口，告知 App 进程更新 Activity 的状态：

```
bindApplication
scheduleTransaction
scheduleLowMemory
scheduleSleeping
```


ApplicationThread 同样基于 AIDL 实现


#### 客户端流程


ATMS 内部代码非常繁琐，涉及多种情景的处理，直接分析代码非常容易迷失。这里给出三个相对简单的具体场景：

- 情景一：从 Launcher 页面点击 App 图标启动一个全新的 App
- 情景二：在应用内从 Activity A 跳转到 Activity B
- 情景三：启动 App 后，按 Home 键回到 Launcher ，再点击 App 图标

接下来，我们以情景一为例，分析 Activity 的启动过程：

Activity 启动是一个 RPC 过程，涉及到 App 进程和 SystemServer 进程，本节我们主要关心 App 进程（客户端）中的流程。

  