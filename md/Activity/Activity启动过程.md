
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
