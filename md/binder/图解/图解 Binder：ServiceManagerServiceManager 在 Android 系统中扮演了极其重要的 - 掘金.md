---
created: 2025-05-22T14:56:41 (UTC +08:00)
tags: [Android,Linux中文技术社区,前端开发社区,前端技术交流,前端框架教程,JavaScript 学习资源,CSS 技巧与最佳实践,HTML5 最新动态,前端工程师职业发展,开源前端项目,前端技术趋势]
source: https://juejin.cn/post/7245141240161058871
author: 满嘴跑火车的小土匪
---

# 图解 Binder：ServiceManagerServiceManager 在 Android 系统中扮演了极其重要的 - 掘金

> ## Excerpt
> ServiceManager 在 Android 系统中扮演了极其重要的角色，它是所有系统服务的注册中心。

---
> 这是一系列的 Binder 文章，会从内核层到 Framework 层，再到应用层，深入浅出，介绍整个 Binder 的设计。详见《[图解 Binder：概述](https://juejin.cn/post/7244018340880007226 "https://juejin.cn/post/7244018340880007226")》。
> 
> 本文基于 Android platform 分支 android-13.0.0\_r1 和内核分支 common-android13-5.15 解析。
> 
> 一些关键代码的链接，可能会因为源码的变动，发生位置偏移、丢失等现象。可以搜索函数名，重新进行定位。

ServiceManager 在 Android 系统中扮演了极其重要的角色，它是所有系统服务的注册中心。许多系统服务（比如 ActivityManagerService、WindowManagerService 等）都会将自己注册到 ServiceManager 中。当其他组件或者应用需要使用这些服务的时候，可以通过 ServiceManager 来查找和获取。

## ServiceManager 类

ServiceManager 是一个类，它是单独运行在 ServiceManager 进程里的。它是一个 Binder 实体，它的实现在 Android 系统的 native 层，即 C++ 层，具体的实现文件是[service\_manager.cpp](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Fcmds%2Fservicemanager%2FServiceManager.cpp%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/cmds/servicemanager/ServiceManager.cpp;bpv=1;bpt=0")。

ServiceManager 类的部分继承关系如下图：

![](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/0fe2e949ad7a4e8a893088c57de96dd0~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

从图中可以看出，它是一个 BBinder，即 Binder 实体。

ServiceManager 类的实现有几个关键函数：

-   addService()：允许服务将自己注册到 ServiceManager 中，以便客户端可以查找它们。一旦注册，服务端的 Binder 引用，就可以被客户端通过 ServiceManager 获取到。
    
-   getService() 和 checkService()：允许客户端查找注册在 ServiceManager 中的服务。客户端可以通过 getService() 获取到服务的 Binder 引用，然后就可以发起 Binder 事务，调用该服务的方法。
    

## addService()

[addService()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Fcmds%2Fservicemanager%2FServiceManager.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D295 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/cmds/servicemanager/ServiceManager.cpp;bpv=1;bpt=0;l=295") 的实现比较简单，最核心的一句代码是：

```
mNameToService[name] = Service {
    .binder = binder, // 即服务的 Binder 引用
    .allowIsolated = allowIsolated,
    .dumpPriority = dumpPriority,
    .debugPid = ctx.debugPid,
};
```

[mNameToService](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Fcmds%2Fservicemanager%2FServiceManager.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D295 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/cmds/servicemanager/ServiceManager.cpp;bpv=1;bpt=0;l=295") 是一个 map，以服务的名称为 key，用来记录服务的 Binder 引用等信息。

```
using ServiceMap = std::map<std::string, Service>;
ServiceMap mNameToService;
```

SystemServer 进程启动后，就会启动 AMS、WMS 这些服务。服务启动后，就会通过 Binder 驱动，获取 ServiceManger 的 Binder 引用：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/fa025cf4b5e3456e84a646a52a0e7c00~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

服务获取到 ServiceManager 的 Binder 引用后，发起 Binder 事务，调用 ServiceManager 的 addService()，注册它自己：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d9f866356579454c94ba5615742771c6~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## getService() 和 checkService()

[getService()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Fcmds%2Fservicemanager%2FServiceManager.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D232 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/cmds/servicemanager/ServiceManager.cpp;bpv=1;bpt=0;l=232") 和 [checkService()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Fcmds%2Fservicemanager%2FServiceManager.cpp%3Bl%3D238%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/cmds/servicemanager/ServiceManager.cpp;l=238;bpv=1;bpt=0") 都是用来查询服务的，两者都是调用 tryGetService()，只是传参不一样：

```
Status ServiceManager::getService(const std::string& name, sp<IBinder>* outBinder) {
    *outBinder = tryGetService(name, true);
    return Status::ok();
}

Status ServiceManager::checkService(const std::string& name, sp<IBinder>* outBinder) {
    *outBinder = tryGetService(name, false);
    return Status::ok();
}
```

[tryGetService()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Fcmds%2Fservicemanager%2FServiceManager.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D244 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/cmds/servicemanager/ServiceManager.cpp;bpv=1;bpt=0;l=244") 就是利用服务的 name，通过 mNameToService，找到服务的 Binder 引用：

```
sp<IBinder> ServiceManager::tryGetService(const std::string& name, bool startIfNotFound) {
    sp<IBinder> out;
    Service* service = nullptr;
    if (auto it = mNameToService.find(name); it != mNameToService.end()) {
        service = &(it->second);
        out = service->binder;
    }


    if (!out && startIfNotFound) {
        tryStartService(name);
    }

    return out;
}
```

通过 getService() 或 checkService()，普通的用户进程就可以利用 AMS、WMS 这些服务的名称，通过 ServiceManager 获取到 AMS、WMS 这些服务对应的 Binder 引用：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/aeb5998b9dfb4178bf4f40c286379f8d~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

### 懒服务

tryGetService() 的 startIfNotFound 参数为 true 时，可能会调用 [tryStartService()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Fcmds%2Fservicemanager%2FServiceManager.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D543 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/cmds/servicemanager/ServiceManager.cpp;bpv=1;bpt=0;l=543") 启动懒服务：

```
void ServiceManager::tryStartService(const std::string& name) {
    std::thread([=] {
        if (!base::SetProperty("ctl.interface_start", "aidl/" + name)) {
            LOG(INFO) << "Tried to start aidl service " << name
                      << " as a lazy service, but was unable to. Usually this happens when a "
                         "service is not installed, but if the service is intended to be used as a "
                         "lazy service, then it may be configured incorrectly.";
        }
    }).detach();
}
```

懒服务是一种按需加载的服务，即只有当客户端请求服务时，服务才会启动。这种方式可以减少系统启动时间和内存占用。

具体来说，这段代码执行了以下操作：

-   创建一个新的线程以执行启动服务的操作。这是因为启动服务可能需要一定的时间，如果在主线程中执行可能会阻塞主线程，导致系统响应缓慢。
    
-   在新的线程中，通过调用 base::SetProperty("ctl.interface\_start", "aidl/" + name) 来尝试启动服务。这里的 "ctl.interface\_start" 是一个 Android 系统属性，用于控制系统服务的启动。"aidl/" + name 则是要启动的服务的名字。
    

具体的启动原理是：Android 的 init 进程有一个属性监听器，可以监听系统属性的变化。当 "ctl.interface\_start" 这个属性被修改时，init进程会收到通知，然后根据属性值启动对应的服务。

其他资料：[动态运行 AIDL 服务](https://link.juejin.cn/?target=https%3A%2F%2Fsource.android.com%2Fdocs%2Fcore%2Farchitecture%2Faidl%2Fdynamic-aidl%3Fhl%3Dzh-cn "https://source.android.com/docs/core/architecture/aidl/dynamic-aidl?hl=zh-cn")。

## ServiceManager 进程的启动

ServiceManager 进程是一个单独运行的 native 进程，由 init 进程启动，它不属于 Zygote 孵化的进程。

init 进程通过读取 Android 初始化语言写的两个配置文件提供的信息启动 ServiceManager 进程：

[servicemanager.rc](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Fcmds%2Fservicemanager%2Fservicemanager.rc "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/cmds/servicemanager/servicemanager.rc")

```
service servicemanager /system/bin/servicemanager
    class core animation
    user system
    group system readproc
    ...
```

[init.rc](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Frootdir%2Finit.rc%3Bl%3D501 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/rootdir/init.rc;l=501")

```
on init
    ...
    start servicemanager
```

ServiceManager 进程启动后，会调用 frameworks/native/cmds/servicemanager/main.cpp 的 [main()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Fcmds%2Fservicemanager%2Fmain.cpp%3Bl%3D113%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/cmds/servicemanager/main.cpp;l=113;bpv=1;bpt=0")：

```
int main(int argc, char** argv) {
    const char* driver = argc == 2 ? argv[1] : "/dev/binder";
    // 初始化进程所属的 ProcessState 实例
    // 打开并初始化 Binder 驱动
    sp<ProcessState> ps = ProcessState::initWithDriver(driver);
    // 将 Binder 线程池的最大线程数设置为 0
    ps->setThreadPoolMaxThreadCount(0);
    // 限制 ServiceManager 进程只能向 Binder 驱动发送异步事务，即不能发送同步事务
    ps->setCallRestriction(ProcessState::CallRestriction::FATAL_IF_NOT_ONEWAY);
    // 创建 ServiceManager 的 Binder 实体。
    sp<ServiceManager> manager = sp<ServiceManager>::make(std::make_unique<Access>());
    // 通过 addService() 注册它自己
    if (!manager->addService("manager", manager, false /*allowIsolated*/,
        IServiceManager::DUMP_FLAG_PRIORITY_DEFAULT).isOk()) {
        LOG(ERROR) << "Could not self register servicemanager";
    }
    
    // 创建当前线程所属的 IPCThreadState 实例
    IPCThreadState::self()->setTheContextObject(manager);
    // 向 Binder 驱动发送注册消息，成为一个 Context Manager
    ps->becomeContextManager();

    // 创建一个 Looper 实例。Looper 的构造函数里，会调用 epoll_create1() 创建一个 Epoll 实例
    sp<Looper> looper = Looper::prepare(false /*allowNonCallbacks*/);
    // 通过  epoll_ctl() 将 Binder 驱动的文件描述符添加到监听项里
    BinderCallback::setupTo(looper);
    ClientCallbackCallback::setupTo(looper, manager);

    // 死循环
    while(true) {
        // 通过 epoll_wait() 监听事件的发生，主要监听 Binder 事务
        looper->pollAll(-1);
    }

    // should not be reached
    return EXIT_FAILURE;
}
```

上述启动代码最关键的几步是：

-   打开 Binder 驱动
-   注册 0 号 Binder 节点
-   epoll 机制监听 Binder 事务

打开 Binder 驱动的代码，主要在 ProcessState::initWithDriver() 里，相关调用如下：

ProcessState.cpp

└─[initWithDriver()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bl%3D84%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;l=84;bpv=1;bpt=0")

└──[init()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D101 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;bpv=1;bpt=0;l=101")

└───[ProcessState()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bl%3D485%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;l=485;bpv=1;bpt=0")

└────[open\_driver()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D452 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;bpv=1;bpt=0;l=452")

└─────open()

└─────ioctl() // 发送 ioctl 命令 BINDER\_VERSION，检测 Binder 驱动版本号

└─────ioctl() // 发送 ioctl 命令 BINDER\_SET\_MAX\_THREADS，设置 Binder 线程池的最大线程数

└─────ioctl() // 发送 ioctl 命令 BINDER\_ENABLE\_ONEWAY\_SPAM\_DETECTION，启用单向垃圾消息检测

└────mmap()

上面的调用链路，最关键是几个系统调用：

-   open()：打开 Binder 驱动设备。最终会调用 Binder 驱动的 [binder\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5725 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5725")。
-   ioctl()：发送各种 ioctl 命令，与 Binder 驱动进行通信。最终会调用 Binder 驱动的 [binder\_ioctl()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5440 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5440")。
-   mmap()：进行 mmap 映射，提供一块虚拟地址空间，用于建立接收其他进程事务消息的缓冲区。最终会调用 Binder 驱动的 [binder\_mmap()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5698 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5698")。

这几个系统调用，最终都是通过虚拟文件系统，进入到内核层。

![1686276821653.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/6e8bfdf3d76f431cbc39cfe5d61e9283~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 注册 0 号 Binder 节点

之前在 Binder 事务一文，我们提到了一个问题：Binder 实体需要由一个事务发送出去。但是事务是要由一个 Binder 代理发起的，那么最开始的一个 Binder 代理，它的 Binder 引用又是从哪里来的呢？

这是个先有鸡还是先有蛋的问题。ServiceManager 就是那只最开始的鸡。ServiceManager 在 Binder 驱动中，通常是第一个建立的节点。其他进程，通过 0 号引用，就可以访问它。

ServiceManager 进程初始化时，就会调用 ProcessState::becomeContextManager() ，向 Binder 驱动发送 ioctl 命令 BINDER\_SET\_CONTEXT\_MGR\_EXT 或 BINDER\_SET\_CONTEXT\_MGR，将当前进程设置成 Context Manager，即 ServiceManager。

```
bool ProcessState::becomeContextManager()
{
    flat_binder_object obj {
        .flags = FLAT_BINDER_FLAG_TXN_SECURITY_CTX,
    };

    int result = ioctl(mDriverFD, BINDER_SET_CONTEXT_MGR_EXT, &obj);

    if (result != 0) {
        int unused = 0;
        result = ioctl(mDriverFD, BINDER_SET_CONTEXT_MGR, &unused);
    }
    return result == 0;
}
```

-   注：BINDER\_SET\_CONTEXT\_MGR\_EXT 和 BINDER\_SET\_CONTEXT\_MGR 区别不大。在 Android 8.0 及更高版本中，新增了 BINDER\_SET\_CONTEXT\_MGR\_EXT 命令，允许将 Binder 的 Context Manager 与一个特定的 SELinux 安全策略关联起来，增强系统的安全性。

ioclt() 最终调用到了内核的 binder\_ioctl()：

binder.c

└─[binder\_ioctl()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5440 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5440")

└──[binder\_ioctl\_set\_ctx\_mgr()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5237 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5237")

└───[binder\_new\_node()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5237 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5237")

从调用链路可以看出，最终会为 ServiceManager 建立一个 Binder 节点。另外，binder\_ioctl\_set\_ctx\_mgr() 里有几句关键的代码：

```
new_node = binder_new_node(proc, fbo);
context->binder_context_mgr_node = new_node;
```

ServiceManager 会记录成 binder\_context\_mgr\_node。

在其他进程想要对 ServiceManger 发起事务，通过 [ProcessState::getContextObject()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D144 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;bpv=1;bpt=0;l=144") 就可以构建一个 Binder 代理，它持有 0 号引用：

```
sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/)
{
    // 创建一个 BpBinder 实例，引用为 0
    sp<IBinder> context = getStrongProxyForHandle(0);
    return context;
}
```

当其他进程通过 0 号引用，发起事务时，在 Binder 驱动处理事务的函数 [binder\_transaction()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D3040 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=3040") 里，有几句关键的代码就会执行：

```
// 如果 Binder 引用不为 0
if (tr->target.handle) {
    ...
} else {
    // Binder 引用为 0，将目标 Binder 节点，设置为 binder_context_mgr_node，即 ServiceManager
    target_node = context->binder_context_mgr_node;
}
```

Binder 驱动在发现事务数据里的 Binder 引用为 0 时，就会将它的目标 Binder 节点，设置为代表 ServiceManger 的节点。

## epoll 机制监听 Binder 事务

与普通的用户进程不同的是，ServiceManager 进程不是通过 Binder 线程池来等待、处理 Binder 事务的，而是在主线程上使用了 epoll 机制监听 Binder 事务。

epoll 是 Linux 中的一种 I/O 复用机制，是 select 和 poll 的替代品，它可以处理大量的并发事件。

epoll 使用一组函数来管理和检测事件：

-   [epoll\_create()](https://link.juejin.cn/?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fepoll_create.2.html "https://man7.org/linux/man-pages/man2/epoll_create.2.html"): 创建一个 epoll 文件描述符，该文件描述符会代表一个内核事件表，用来存储需要监听的文件描述符及其相应的事件。
    
-   [epoll\_ctl()](https://link.juejin.cn/?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fepoll_ctl.2.html "https://man7.org/linux/man-pages/man2/epoll_ctl.2.html"): 用于向内核事件表中添加、删除或者修改需要监听的文件描述符及其相应的事件。
    
-   [epoll\_wait()](https://link.juejin.cn/?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fepoll_wait.2.html "https://man7.org/linux/man-pages/man2/epoll_wait.2.html"): 用于等待内核事件表中的事件发生（例如，数据可读、数据可写、连接关闭等）。它有一个 timeout 参数：
    
    -   timeout 为负数时，epoll\_wait() 会一直阻塞，直到有一个事件发生。
    -   timeout 为 0 时，epoll\_wait() 会立即返回，不管是否有事件发生。
    -   timeout 为大于 0 的值 x 时，epoll\_wait() 会阻塞直到有一个事件发生或者 x 毫秒时间到达。

![](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/86bb527a0b334ff6a45829890c7e1dcd~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

在使用 select 和 poll 进行 I/O 多路复用时，当监听的文件描述符数量很大时，它们会存在一些性能问题：

-   每次调用 select 或 poll 都需要遍历所有的文件描述符，检查哪些文件描述符准备就绪，时间复杂度是 O(n)，会随文件描述符的数量增长，降低效率。
-   每次调用 select 或 poll 需要将文件描述符数组，从用户空间拷贝到内核空间。
-   select 和 poll 的通知方式是水平触发，即只要有数据到来就会不断触发通知，直到数据被处理完。这会导致系统资源的浪费。
-   select 能处理的文件描述符的数量受到 FD\_SETSIZE 的限制。

epoll 则解决了这些问题：

-   不随监听的文件描述符数目增长而降低效率。epoll 是基于事件驱动的，当 epoll\_ctl() 注册的文件描述符上的事件发生时，才会将该文件描述符添加到一个就绪列表。当我们调用 epoll\_wait() ，内核只需要返回就绪列表的文件描述符，不需要遍历所有的文件描述符。
-   使用 epoll\_ctl() 注册的文件描述符，内核会将这些描述符放到一个红黑树上，只需要在调用 epoll\_ctl() 注册时进行一次拷贝即可，不需要重复拷贝。
-   epoll 同时支持水平触发（LT）和边缘触发（ET）。边缘触发模式下，当检测到一次事件发生后，只会通知一次，直到下一次事件发生，无论中间是否有数据到达。这样可以减少无用的事件通知，进一步提高效率。
-   epoll 没有描述符数量的限制，仅受限于可用内存。

回顾一下 ServiceManager 进程启动时的 main()：

```
int main(int argc, char** argv) {
    ...
    // 创建一个 Looper 实例。Looper 的构造函数里，会调用 epoll_create1() 创建一个 Epoll 实例
    sp<Looper> looper = Looper::prepare(false /*allowNonCallbacks*/);
    // 通过  epoll_ctl() 将 Binder 驱动的文件描述符添加到监听项里
    BinderCallback::setupTo(looper);
    ClientCallbackCallback::setupTo(looper, manager);

    // 死循环
    while(true) {
        // 通过 epoll_wait() 监听事件的发生，主要监听 Binder 事务。epoll_wait() 的 timeout 设置为 -1
        looper->pollAll(-1);
    }

    // should not be reached
    return EXIT_FAILURE;
}
```

相关调用如下：

main.cpp

└─[main()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Fcmds%2Fservicemanager%2Fmain.cpp%3Bl%3D113%3Bbpv%3D0%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/cmds/servicemanager/main.cpp;l=113;bpv=0;bpt=0")

└──Looper.cpp

└───[prepare()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Flibutils%2FLooper.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D124 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/libutils/Looper.cpp;bpv=0;bpt=0;l=124")

└────[Looper()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Flibutils%2FLooper.cpp%3Bl%3D72%3Bbpv%3D0%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/libutils/Looper.cpp;l=72;bpv=0;bpt=0")

└─────[rebuildEpollLocked()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Flibutils%2FLooper.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D142 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/libutils/Looper.cpp;bpv=0;bpt=0;l=142")

└──────**epoll\_create1()**

└──BinderCallback.cpp

└───[setupTo()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Fcmds%2Fservicemanager%2Fmain.cpp%3Bl%3D41%3Bbpv%3D0%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/cmds/servicemanager/main.cpp;l=41;bpv=0;bpt=0")

└────IPCThreadState.cpp

└─────[setupPolling()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D671 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=0;bpt=0;l=671") // 向缓冲区写入 BC\_ENTER\_LOOPER 消息；获取 Binder 驱动的文件描述符

└────Looper.cpp

└─────[addFd()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Flibutils%2FLooper.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D428 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/libutils/Looper.cpp;bpv=0;bpt=0;l=428")

└──────[addFd()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Flibutils%2FLooper.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D436 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/libutils/Looper.cpp;bpv=0;bpt=0;l=436")

└───────**epoll\_ctl()**

└──Looper.cpp

└───[pollAll()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Flibutils%2FLooper.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D378 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/libutils/Looper.cpp;bpv=0;bpt=0;l=378")

└────[pollOnce()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Flibutils%2FLooper.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D181 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/libutils/Looper.cpp;bpv=0;bpt=0;l=181")

└─────[pollInner()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Flibutils%2FLooper.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D217 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/libutils/Looper.cpp;bpv=0;bpt=0;l=217")

└──────**epoll\_wait()**

## 通过 ServiceManager 获取 AMS 的 Binder 引用

在 Activity 的启动流程里，我们能看到下面的一行代码：

```
final IBinder b = ServiceManager.getService(Context.ACTIVITY_SERVICE);
```

getService() 的返回，就是 AMS 的 Binder 引用。

调用流程图如下：

![1686536063265.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/9b4ffde10b0649bdb66b8fd3c22290e8~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

调用链路如下：

ServiceManager.java

└─[getService()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FServiceManager.java%3Bl%3D141 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/android/os/ServiceManager.java;l=141")

└──[rawGetService()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FServiceManager.java%3Bl%3D369 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/android/os/ServiceManager.java;l=369")

└───[getIServiceManager()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FServiceManager.java%3Bl%3D122 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/android/os/ServiceManager.java;l=122") // 获取代表 ServiceManager 的 Binder 引用

└────BinderInternal.java

└─────[getContextObject()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fcom%2Fandroid%2Finternal%2Fos%2FBinderInternal.java%3Bl%3D180%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/com/android/internal/os/BinderInternal.java;l=180;bpv=1;bpt=0")

└──────android\_util\_Binder.cpp

└───────[android\_os\_BinderInternal\_getContextObject()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp%3Bl%3D1141 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/jni/android_util_Binder.cpp;l=1141")

└────────ProcessState.cpp

└─────────getContextObject() // 构建一个引用为 0 的 Binder 引用，代表 ServiceManager

└───ServiceManagerProxy.java

└────[getService()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FServiceManagerNative.java%3Bl%3D61 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/android/os/ServiceManagerNative.java;l=61")

└─────IServiceManager.Proxy.java

└──────[checkService()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Frefs%2Fheads%2Fmaster%3Aout%2Fsoong%2F.intermediates%2Fframeworks%2Fbase%2Fframework-minus-apex-intdefs%2Fandroid_common%2Fxref36%2Fsrcjars.xref%2Fandroid%2Fos%2FIServiceManager.java%3Bdrc%3D7346c436e5a11ce08f6a80dcfeb8ef941ca30176%3Bl%3D432 "https://cs.android.com/android/platform/superproject/+/refs/heads/master:out/soong/.intermediates/frameworks/base/framework-minus-apex-intdefs/android_common/xref36/srcjars.xref/android/os/IServiceManager.java;drc=7346c436e5a11ce08f6a80dcfeb8ef941ca30176;l=432")

注：IServiceManager.Proxy.java 是 AIDL 自动生成的代码，后续在 AIDL 一章会有类似的内容，这里不做进一步探究。
