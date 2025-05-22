---
created: 2025-05-22T14:56:27 (UTC +08:00)
tags: [Android,Linux中文技术社区,前端开发社区,前端技术交流,前端框架教程,JavaScript 学习资源,CSS 技巧与最佳实践,HTML5 最新动态,前端工程师职业发展,开源前端项目,前端技术趋势]
source: https://juejin.cn/post/7244174211970662455
author: 满嘴跑火车的小土匪
---

# 图解 Binder：线程池本文主要介绍 Binder 线程、Binder 工作队列，还有 Binder 线程池的启动流程 - 掘金

> ## Excerpt
> 本文主要介绍 Binder 线程、Binder 工作队列，还有 Binder 线程池的启动流程等等。

---
> 这是一系列的 Binder 文章，会从内核层到 Framework 层，再到应用层，深入浅出，介绍整个 Binder 的设计。详见《[图解 Binder：概述](https://juejin.cn/post/7244018340880007226 "https://juejin.cn/post/7244018340880007226")》。
> 
> 本文基于 Android platform 分支 android-13.0.0\_r1 和内核分支 common-android13-5.15 解析。
> 
> 一些关键代码的链接，可能会因为源码的变动，发生位置偏移、丢失等现象。可以搜索函数名，重新进行定位。

每个使用 Binder 通信的进程，都会在 Framework 层建立自己的线程池，处理来自不同进程的事务。内核层也会为线程池线程维护相应的数据结构。

## Binder 线程

Binder 里的线程可以分成两类：普通的用户线程、Binder 线程池里的线程。

下图展示了在 Binder 事务中，涉及的几个与 Binder 线程相关的数据结构：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/0685e9a369bb47f5844bbb12e705726b~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

-   ① Thread 指的是普通的用户线程，也就是发起 Binder 事务的线程。
    
-   ② [PoolThread](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bl%3D61 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;l=61") 指的是 Binder 线程池里的线程，用来处理发送给当前进程的 Binder 消息。
    
-   ③ [binder\_thread](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_internal.h%3Bl%3D507 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_internal.h;l=507") 是 Binder 驱动里在内核维护的一个数据结构，用来描述一个用户空间线程的信息，如线程 pid、工作队列 todo 等。binder\_thread 是通过 [binder\_get\_thread()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Frefs%2Fheads%2Fcommon-android-mainline%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5078 "https://cs.android.com/android/kernel/superproject/+/refs/heads/common-android-mainline:common/drivers/android/binder.c;l=5078") 创建的。
    
-   ④ 每当一个用户空间线程通过 Binder 驱动进行进程间通信时，都会有一个对应的 binder\_thread 对象在内核中表示它，即 **binder\_thread 与 Thread 或 PoolThread 是一对一的关系。**
    
-   ⑤ proc、target\_proc 都是结构体 [binder\_proc](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_internal.h%3Bl%3D428 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_internal.h;l=428") 的对象，分别对应客户端进程、服务端进程。binder\_proc 是 Binder 驱动在内核维护的一个数据结构，代表一个使用 Binder 通信机制的进程，记录着进程相关信息。它维护两个与 binder\_thread 相关的数据结构：
    
    -   threads：一棵红黑树，记录了该进程的正在 Binder 驱动里进行通信的所有用户空间线程对应的 binder\_thread，按照线程的 pid 大小排序。目的是为了后续能用线程的 pid 在 O(logn) 的时间复杂度内找到对应的 binder\_thread，或者在用户空间的线程销毁时，在 O(logn) 的时间复杂度内移除对应的 binder\_htread。
    -   waiting\_threads：一个双向链表，记录了当前所有空闲的线程池线程 PoolThread。
-   ⑥ Thread、PoolThread 每次进入 Binder 驱动时，首先都会通过 [binder\_get\_thread()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Frefs%2Fheads%2Fcommon-android-mainline%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5078 "https://cs.android.com/android/kernel/superproject/+/refs/heads/common-android-mainline:common/drivers/android/binder.c;l=5078") 查询当前线程 binder\_proc 里的红黑树 threads 是否有对应的 binder\_thread，没有则创建一个，并插入 threads 中。
    
-   ⑦ Binder 线程池的线程在创建的时候或者在刚完成上一个工作任务时，一旦进程的工作队列里，没有新的任务，就会进入 waiting\_threads 的链表头里，并挂起。当进程的工作队列有新的任务到来时，就会移除 waiting\_threads 的链表头的 binder\_thread，唤醒对应的线程，处理任务。（普通的用户线程 Thread 是不会进入 waiting\_threads 的）
    

在 Binder 事务中，从线程的角度来看，其实就是由 Thread 提交一个事务，经过 Binder 驱动转换处理后，选择线程池中的一个 PoolThread 处理。

## Binder 工作队列

Binder 的工作队列有三种：线程工作队列、进程工作队列和待处理的异步任务工作队列。这三种工作队列，都是双向链表。

工作队列里任务的类型，最常见的是事务，但不是只有事务。工作任务类型由 [binder\_work\_type](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_internal.h%3Bl%3D152 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_internal.h;l=152") 定义。

## 线程工作队列和进程工作队列

线程工作队列，是指 binder\_thread 里的 todo。

进程工作队列，是指 binder\_proc 里的 todo。

下图展示了 Binder 线程池里的线程处理这两个工作队列里的工作任务的流程和优先级：

![](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/10d6a6b0a20643b786264e8c5f64fa82~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

-   1）PoolThread 会先判断 binder\_thread 的 todo 是否为空，不为空则从该 todo 中出队一个工作任务，并进行处理。为空则进行步骤 2）
-   2）PoolThread 判断 binder\_proc 的 todo 是否为空，不为空则从该 todo 中出队一个工作任务，并进行处理。为空则进行步骤 3）
-   3）PoolThread 发现当前没有可处理的工作任务，进入到 binder\_proc 的 waiting\_threads 里，并挂起自己。

注意，上面提到的是 PoolThread。普通的用户线程的话，不太一样。普通的用户线程 Thread，是不会处理进程工作队列的任务的，只会处理自身 todo。并且它在等待事务回复的过程中，也不会入队到 waiting\_threads 中，只是简单地挂起。

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c468a00276f84d21ba055474a34c23a5~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

如上图，普通的用户线程 Thread 提交一个同步事务后：

-   先检测目标进程的 wait\_threads：
    -   如果 wait\_threads 不为空，则会从目标进程的 wait\_threads 出队一个空闲线程，并将该事务入队到该线程的 todo 里，最后唤醒该线程，进行处理。
    -   如果 wait\_threads 为空，则会将该事务入队到目标进程的 todo 里，等待服务端有空闲的线程。
-   用户线程 Thread 挂起，等待服务端处理该事务。
-   服务端处理完成事务后，PoolThread 会提交回复到 Thread 的 todo 里。
-   最后，唤醒 Thread，读取服务端的回复。

## 待处理的异步任务工作队列

待处理的异步任务工作队列，是指 binder\_node 里的 async\_todo。一个 binder\_node 最多只可以有一个异步任务，多余的会放进 binder\_node 的 async\_todo 队列中，异步任务处理完，才会从 async\_todo 队列中出队一个到 binder\_proc 里的 todo。

这样的设计确保了每个 binder\_node 在任何时刻都只有一个活跃的异步事务，这对于避免并发问题非常有用。同时，通过使用 async\_todo 队列，Binder 可以保证异步事务的顺序性：先请求的异步事务会先得到处理。

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/430d45eab94c4414816f6337acb9f6ca~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

如上图，两个用户线程一前一后，通过一个 Binder 代理，即 BpBinder，发起异步事务：

-   Thread1 的异步事务来到 Binder 驱动时，发现 binder\_node 当前没有异步事务，所以走了路径 ①，直接提交事务到 binder\_thread 的 todo 中，或者 binder\_proc 的 todo 中。
-   Thread2 的异步事务来到 Binder 驱动时，发现 binder\_node 当前有正在处理的异步事务（Thread1 的），所以走了路径 ②，将事务入队到 binder\_node 的 async\_todo 中。直到 Thread1 的异步事务被处理完，就会从 async\_todo 出队 Thread2 的异步事务，并提交到到 binder\_thread 的 todo 中，或者 binder\_proc 的 todo 中。

## Binder 线程池

Binder 线程池在进程启动时创建，用来接受、处理来自 Binder 驱动（源自其他进程）的消息。这时候，Binder 线程池所在进程为服务端，其他进程为客户端。

线程池里的线程分为两类：

-   主线程：线程池第一个创建的线程。
    -   通过 BC\_ENTER\_LOOPER 通知 Binder 驱动，自己进入了轮询状态。
    -   通过 BC\_EXIT\_LOOPER 通知 Binder 驱动，自己退出了轮询状态。
-   工作线程：后续创建的线程池线程。
    -   通过 BC\_REGISTER\_LOOPER 通知 Binder 驱动，自己进入了轮询状态。
    -   通过 BC\_EXIT\_LOOPER 通知 Binder 驱动，自己退出了轮询状态。

主线程和工作线程启动后，都会在 [joinThreadPool()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D638 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=638") 里陷入轮询，不断轮询处理 Binder 消息，不同的是主线程不会因超时退出轮询，而子线程会（但从源码来看，会触发 TIMED\_OUT 的，只有 BR\_FINISHED 消息，但现在在 binder.c 已经没发送该消息的代码，所以主线程、工作线程现在其实差异不大）。

## IPCThreadState

每个 Binder 线程在 Framework 层都有一个 [IPCThreadState](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp") 实例。 它用于管理 Binder IPC（进程间通信）的线程状态，负责接收和处理来自 Binder 驱动的命令，包括传递的数据和文件描述符等。

当 IPCThreadState [初始化](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bl%3D866 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;l=866")时，会设置输入和输出缓冲区的容量：

```
mIn.setDataCapacity(256);
mOut.setDataCapacity(256);
```

这里，mIn 和 mOut 都是 [Parcel 类](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/Parcel.cpp")的实例，分别表示输入和输出缓冲区。setDataCapacity(256) 将缓冲区的容量设置为 256 字节。

设置初始容量是为了在 IPC 调用过程中提供足够的缓冲区空间。在大多数情况下，这个初始容量已经足够应付大部分的数据传输需求。然而，在某些情况下，可能需要传输更大的数据块，这时候缓冲区的容量会根据需要动态增长。

在实际运行过程中，IPCThreadState 可能会处理许多不同大小的数据块。设置一个合适的初始缓冲区容量有助于提高系统性能，因为它可以减少在数据传输过程中频繁调整缓冲区大小所带来的开销。

## ProcessState

每个进程在 Framework 层都有一个 [ProcessState](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2Finclude%2Fbinder%2FProcessState.h%3Bl%3D32 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/include/binder/ProcessState.h;l=32") 实例，它负责维护与 Binder 驱动程序之间的通信，如打开 Binder 驱动程序设备、初始化线程池、管理线程池等。ProcessState 类与 IPCThreadState 类密切相关，后者负责管理每个线程的 Binder 通信状态。

以下是 ProcessState 的一些重要函数：

-   [startThreadPool()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bl%3D177 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;l=177")：用于启动 Binder 线程池。线程池中的线程负责处理进程间的 Binder 通信。一开始，线程池只有一个主线程，用于处理与进程关联的所有 Binder 请求。根据需要，线程池还可以包含其他工作线程，以协助主线程处理并发请求。
-   [spawnPooledThread()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bl%3D382 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;l=382")：用于在 Binder 线程池中创建并启动新的线程。有一个入参 isMain，为 true 时是创建主线程，为 false 时是创建工作线程。
-   [setThreadPoolMaxThreadCount()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D392 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;bpv=1;bpt=0;l=392")：设置 Binder 线程池的最大线程数。默认的 Binder 线程池的最大线程数是 15，由 [DEFAULT\_MAX\_BINDER\_THREADS](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D48 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;bpv=1;bpt=0;l=48") 定义。注意，这个最大线程数，是不包括主线程的，限制的是工作线程数。

## Binder 线程池启动

Zygote 进程在 [processCommand()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fcom%2Fandroid%2Finternal%2Fos%2FZygoteConnection.java%3Bl%3D117%3Bdrc%3Dd3cf93d7c01809c12525f096ef1e1e840267b33a "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/com/android/internal/os/ZygoteConnection.java;l=117;drc=d3cf93d7c01809c12525f096ef1e1e840267b33a") 接受到创建子进程的消息并 fork 出子进程后，在执行子进程的初始化时，就会创建并启动 Binder 线程池。

![1685367365207.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a50b5f26d5154e18910a63059ed36203~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

相关调用链路如下：

ZygoteConnection.java

└─[processCommand()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fcom%2Fandroid%2Finternal%2Fos%2FZygoteConnection.java%3Bdrc%3Dd3cf93d7c01809c12525f096ef1e1e840267b33a%3Bbpv%3D1%3Bbpt%3D1%3Bl%3D117 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/com/android/internal/os/ZygoteConnection.java;drc=d3cf93d7c01809c12525f096ef1e1e840267b33a;bpv=1;bpt=1;l=117")

└──[handleChildProc()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fcom%2Fandroid%2Finternal%2Fos%2FZygoteConnection.java%3Bdrc%3D595ba51d4306851bb4f27e6eb5014878021f5343%3Bl%3D514 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/com/android/internal/os/ZygoteConnection.java;drc=595ba51d4306851bb4f27e6eb5014878021f5343;l=514")

└───ZygoteInit.java

└────[zygoteInit()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fcom%2Fandroid%2Finternal%2Fos%2FZygoteInit.java%3Bdrc%3D595ba51d4306851bb4f27e6eb5014878021f5343%3Bl%3D990 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/com/android/internal/os/ZygoteInit.java;drc=595ba51d4306851bb4f27e6eb5014878021f5343;l=990")

└─────[nativeZygoteInit()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fcom%2Fandroid%2Finternal%2Fos%2FZygoteInit.java%3Bdrc%3D595ba51d4306851bb4f27e6eb5014878021f5343%3Bl%3D1015 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/com/android/internal/os/ZygoteInit.java;drc=595ba51d4306851bb4f27e6eb5014878021f5343;l=1015")

└──────AndroidRuntime.cpp

└───────[com\_android\_internal\_os\_ZygoteInit\_nativeZygoteInit()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjni%2FAndroidRuntime.cpp%3Bl%3D251 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/jni/AndroidRuntime.cpp;l=251")

└────────app\_main.cpp

└─────────[onZygoteInit()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcmds%2Fapp_process%2Fapp_main.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D92 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/cmds/app_process/app_main.cpp;bpv=1;bpt=0;l=92")

└──────────ProcessState.cpp

└───────────[startThreadPool()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D177 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;bpv=1;bpt=0;l=177")

└────────────[spawnPooledThread()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D382 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;bpv=1;bpt=0;l=382")

└─────────────Threads.cpp

└──────────────[run()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Flibutils%2FThreads.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D658 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/libutils/Threads.cpp;bpv=1;bpt=0;l=658")

└───────────────[\_threadLoop()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Flibutils%2FThreads.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D707 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/libutils/Threads.cpp;bpv=1;bpt=0;l=707")

└────────────────PoolThread.cpp

└─────────────────[threadLoop()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bl%3D70%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;l=70;bpv=1;bpt=0")

└──────────────────[joinThreadPool()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D638 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=638")

## Binder 线程池创建新的线程

当进程的工作线程的等待队列，即 binder\_proc 的 waiting\_threads 已经为空，并且当前 Binder 线程池里的工作线程还没有达到最大线程数的时候，Binder 驱动会发送一条 BR\_SPAWN\_LOOPER 消息，通知进程创建一个新的工作线程。

![1685366953625.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/6d1d70cdc11d4548b11e28833d0b68f7~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)
