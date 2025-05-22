---
created: 2025-05-22T14:54:58 (UTC +08:00)
tags: [Android,Linux中文技术社区,前端开发社区,前端技术交流,前端框架教程,JavaScript 学习资源,CSS 技巧与最佳实践,HTML5 最新动态,前端工程师职业发展,开源前端项目,前端技术趋势]
source: https://juejin.cn/post/7244018340880007226
author: 满嘴跑火车的小土匪
---

# 图解 Binder：概述本系列 Binder 文章，会从内核层到 Framework 层，再到应用层，深入浅出，介绍整个 - 掘金

> ## Excerpt
> 本系列 Binder 文章，会从内核层到 Framework 层，再到应用层，深入浅出，介绍整个 Binder 的设计。

---
       ![](https://p9-piu.byteimg.com/tos-cn-i-8jisjyls3a/c676d36a15f248e8aedb339deddadb90~tplv-8jisjyls3a-image.image)

Android 的 Binder 机制是一种独特的跨进程通信（IPC）系统，在整个Android系统中都发挥着至关重要的作用。这种机制在操作系统的内核层、 Android 框架层（Framework）以及应用层（Java）都有其具体实现，从底层到上层，每一层都为 IPC 机制提供了必要的支撑，构成了一个高效且灵活的通信体系。

本系列 Binder 文章，会从内核层到 Framework 层，再到 Java 层，深入浅出，介绍整个 Binder 的设计：

-   [图解Binder：初始化](https://juejin.cn/post/7201400444873293885 "https://juejin.cn/post/7201400444873293885")
-   [图解Binder：系统调用 open](https://juejin.cn/post/7204751926515449911 "https://juejin.cn/post/7204751926515449911")
-   [图解Binder：事务](https://juejin.cn/post/7244018340880187450 "https://juejin.cn/post/7244018340880187450")
-   [图解Binder：线程池](https://juejin.cn/post/7244174211970662455 "https://juejin.cn/post/7244174211970662455")
-   [图解Binder：内存管理](https://juejin.cn/post/7244734179828203579 "https://juejin.cn/post/7244734179828203579")
-   [图解Binder：ServiceManager](https://juejin.cn/post/7245141240161058871 "https://juejin.cn/post/7245141240161058871")
-   [图解Binder：AIDL](https://juejin.cn/post/7245518707581124666 "https://juejin.cn/post/7245518707581124666")

## Binder 内核层

## Binder 初始化

Binder 驱动是 Android 系统的一部分，是在编译期编译到内核中的。Binder 驱动利用 initcall 机制，将它的初始化函数 binder\_init() 添加到 initcall 队列中。当内核启动时，binder\_init() 函数就会被执行，从而完成 Binder 驱动的初始化。

在Binder驱动的初始化过程中， binder\_init() 主要工作是：

-   注册 binder 设备：调用 misc\_register()，向虚拟文件系统（VFS）注册一个名为 "binder" 的 misc 设备，该设备的设备文件路径是 "/dev/binder"。一旦注册成功，用户空间的进程就可以通过打开 "/dev/binder" 设备文件，对 Binder 驱动进行操作，从而实现跨进程通信。
-   注册 binder 文件系统：调用 register\_filesystem()，向 VFS 注册 binder 文件系统。binder 文件系统会在 init 进程启动时进行挂载。当通过系统调用 open() 打开 "/dev/binder" 设备文件时，就会沿着 VFS，最后定位到 binder 文件系统，调用其对应的 binder\_open() 实现，完成 binder 驱动的打开。

![](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/986e02443ef84e3c92f7115fdb6654d9~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

《图解 Binder：初始化》一文将详细阐述 Binder 的 binder\_init()，以及如何通过 initcall 机制进行系统初始化。

## Binder 的几个系统调用

每个要使用 Binder 进行通信的进程，都会调用 Framework 层的 ProcessState::initWithDriver() ，打开 Binder 驱动。initWithDriver() 主要涉及了几个系统调用，它们的具体的实现，主要是在内核层。相关调用如下：

ProcessState.cpp

└─[initWithDriver()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bl%3D84%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;l=84;bpv=1;bpt=0")

└──[init()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D101 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;bpv=1;bpt=0;l=101")

└───[ProcessState()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bl%3D485%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;l=485;bpv=1;bpt=0")

└────[open\_driver()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D452 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;bpv=1;bpt=0;l=452")

└─────open()

└─────ioctl() // 发送 ioctl 命令 BINDER\_VERSION，检测 Binder 驱动版本号

└─────ioctl() // 发送 ioctl 命令 BINDER\_SET\_MAX\_THREADS，设置 Binder 线程池的最大线程数

└─────ioctl() // 发送 ioctl 命令 BINDER\_ENABLE\_ONEWAY\_SPAM\_DETECTION

└────mmap()

上面的调用链路，最关键是几个系统调用：

-   open()：打开 Binder 驱动设备。最终会调用 Binder 驱动的 [binder\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5725 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5725")。
-   ioctl()：发送各种 ioctl 命令，与 Binder 驱动进行通信。最终会调用 Binder 驱动的 [binder\_ioctl()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5440 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5440")。
-   mmap()：进行 mmap 映射，提供一块虚拟地址空间，用于建立接收其他进程事务消息的缓冲区。最终会调用 Binder 驱动的 [binder\_mmap()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5698 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5698")。

这几个系统调用，最终都是通过虚拟文件系统，进入到内核层。

![1686276821653.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/6e8bfdf3d76f431cbc39cfe5d61e9283~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

在《图解 Binder：系统调用 open》里，我们会介绍是如何沿着虚拟文件系统，最终定位到 binder 文件系统，调用到 binder\_open()。

## Binder 事务

一旦Binder驱动被成功打开，我们就能发起Binder事务。事务是Binder通信的基础。

Binder 事务，就是一个进程（客户端）在调用另一个进程（服务端）中的一个函数。这个"函数调用"的过程就是通过发送一个 Binder 事务来发起，"返回值"就是 Binder 事务的回复。

![](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e4ca2d08440b428abf2ba0cf9fdd220b~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

在《图解 Binder：事务》中，我们会围绕 Binder 事务，介绍与它相关的一些概念：binder 消息、ioctl 命令、binder实体、binder 代理、binder 节点和 binder 引用以及它们的工作机制。

## Binder 线程池

每个使用 Binder 通信的进程，都会在 Framework 层建立自己的线程池，处理来自不同进程的事务。内核层也会为线程池线程维护相应的数据结构。

《图解Binder：线程池》将揭示 Binder 线程池的工作原理，也会介绍 Binder 驱动里的几个工作队列。

## Binder 内存管理

Binder 的高效性离不开其独特的内存管理机制。

每个进程在使用 Binder 进程通信之前，都会先通过 mmap 进行映射，用于建立接收其他进程事务消息的缓冲区。mmap 是 Binder 实现进程通信一次拷贝的关键。

![](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/31b0c4da71934f0eb32617274019e4ad~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

内核也为缓冲区的高效分配、释放，设计了复杂的数据结构。这也是 Binder 内存管理不可或缺的精彩部分。

在《图解Binder：内存管理》一文中，我们将深入探讨虚拟内存、mmap、缓冲区分配和释放、物理内存页分配和释放，以及内存缩减器等机制。它们共同提升 Binder 通信的性能。

## Binder Framework 层

Android 的 Framework 层是由 Java 代码和 C++ 代码共同组成的。Java 代码通常是基于 C++ 代码，进行封装的。

在讲 Binder 内核层的时候，其实就会涉及了不少 Framework 层的代码，比如：Binder 线程池、Binder 事务发送与接收的代码等（主要是 C++ 代码）。

## ServiceManager

在 Android Framework 层，Binder 的另一个主要组成部分是 ServiceManager。

ServiceManager 是 Binder 驱动中的 0 号 Binder 节点，用于管理系统级服务的 Binder 引用。AMS、WMS 这些系统级服务，都会将自己注册到 ServiceManager 中：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d9f866356579454c94ba5615742771c6~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

当其他进程需要使用这些服务的时候，可以通过 ServiceManager 来查找和获取：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/aeb5998b9dfb4178bf4f40c286379f8d~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

在《图解Binder：ServiceManager》中，我们会介绍 ServiceManager 的工作原理。

## Binder 应用层

Framework 在 Java 层为应用层提供了不少与 native 层对应的类，如 Binder、BinderProxy 等类。这些类，本质上就是对 native 层的封装，最终还是会调用 native 层的对应函数。

应用层调用这些 Java 类，就可以通过 Binder，实现跨进程通信。但是直接调用这些 Java 类，会比较复杂，需要深入了解 Binder 通信的机制。所以，Binder 为我们提供了 AIDL 来解决这个困难。

## AIDL

在 Java 层，Binder 还提供了 AIDL，便于我们实现跨进程的调用。

AIDL（Android Interface Definition Language）是一种支持跨进程通信 (IPC) 的接口定义语言。AIDL 的作用就是为我们生成一些模板代码，减轻开发者的工作量。

在《图解Binder：AIDL》一文中，我们将介绍 AIDL 的使用及工作原理。

## 代码文件目录

Binder 内核层的代码，主要在 [common/drivers/android](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android") 目录下。

负责与内核通信的 Framework 层的 C++ 代码，如 Binder 线程池、Binder 事务发送与接收的代码等，主要在 [frameworks/native/libs/binder](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2F "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/") 目录下。

Framework 层的 ServiceManager 相关代码，主要在 [frameworks/native/cmds/servicemanager/ServiceManager](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Fcmds%2Fservicemanager%2F "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/cmds/servicemanager/") 目录下。

Framework 的 Java 层的相关代码，主要在 [frameworks/base/core/java/android/os](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2F "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/android/os/") 目录下。

## 源码阅读技巧

本系列文章，都是基于Android platform 分支 android-13.0.0\_r1 和内核分支 common-android13-5.15解 析。

一些关键代码的链接，可能会因为源码的变动，发生位置偏移、丢失等现象。可以搜索函数名，重新进行定位：

![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/56760a6083c5467a9438aaa912e638a6~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

如果和当前 Android 最新代码相差不大，建议 Framework 层代码直接切换到 master 分支，内核层代码直接切换到 common-android-mainline 分支。这样就可以像 IDEA 一样，点击相关函数进行跳转，或者查看哪些函数调用了该函数（非主分支代码，无法进行跳转）。

Framework 层代码直接切换到 master 分支 :

![](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/95396db41bf2453590ad84162e5a9b84~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

内核层代码直接切换到 common-android-mainline 分支：

![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/373415bc0209408797b727e39e6e67e1~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

本文收录于以下专栏

![cover](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/95414745836549ce9143753e2a30facd~tplv-k3u1fbpfcp-jj:80:60:0:0:q75.avis)

![avatar](https://p6-passport.byteacctimg.com/img/user-avatar/35eaba805c13264fd30588752063c635~40x40.awebp)
