---
created: 2025-05-22T14:56:04 (UTC +08:00)
tags: [Android,Linux中文技术社区,前端开发社区,前端技术交流,前端框架教程,JavaScript 学习资源,CSS 技巧与最佳实践,HTML5 最新动态,前端工程师职业发展,开源前端项目,前端技术趋势]
source: https://juejin.cn/post/7244018340880187450
author: 满嘴跑火车的小土匪
---

# 图解 Binder：事务Binder 事务是 Android IPC（进程间通信）机制的基本单元。它是基于 C/S 架构 - 掘金

> ## Excerpt
> Binder 事务是 Android IPC（进程间通信）机制的基本单元。它是基于 C/S 架构的，主要涉及到两个进程：客户端进程和服务端进程。

---
> 这是一系列的 Binder 文章，会从内核层到 Framework 层，再到应用层，深入浅出，介绍整个 Binder 的设计。详见《[图解 Binder：概述](https://juejin.cn/post/7244018340880007226 "https://juejin.cn/post/7244018340880007226")》。
> 
> 本文基于 Android platform 分支 android-13.0.0\_r1 和内核分支 common-android13-5.15 解析。
> 
> 一些关键代码的链接，可能会因为源码的变动，发生位置偏移、丢失等现象。可以搜索函数名，重新进行定位。

Binder 事务是 Android IPC（进程间通信）机制的基本单元。它是基于 C/S 架构的，主要涉及到两个进程：客户端进程和服务端进程。客户端进程发送一个包含指令和数据的 Binder 事务请求到服务端进程，服务端进程在收到请求后，进行处理并返回一个 Binder 事务回复。

你可以认为 Binder 事务，就是一个进程（客户端）在调用另一个进程（服务端）中的一个函数。这个"函数调用"的过程就是通过发送一个 Binder 事务来发起，"返回值"就是 Binder 事务的回复。

![](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e4ca2d08440b428abf2ba0cf9fdd220b~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

Binder 驱动在 Binder 事务中的作用，就像一个中转站，负责转发工作：

![](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/03ad9cc9be1a4f03ab20a1027cf3909f~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

本文主要围绕 Binder 事务，逐个解释它涉及的各种小概念。

## Binder 消息

Binder 消息，即 Binder 命令，笔者更喜欢称其为消息。二者并无差别。

就像网络请求中的 Http 报文一样，在进程间传递的 Binder 消息，也遵循特定的 Binder 协议。Binder 协议是 Android Binder 的底层通信协议，主要在 Android 用户空间和内核空间之间进行通信。Binder 协议定义了一组消息码，每个消息码都对应 Binder 系统中的一个操作或事件。

Binder 的消息通信基于这组消息码。消息的具体格式由 Binder 协议定义，通常包括消息码和数据两部分：

![](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/df6491c344a14d71a191d3567c707e55~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

-   消息码：一个整数值，表示要执行的操作或事件。消息码分为两大类：[BC](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fkernel%2Fuapi%2Flinux%2Fandroid%2Fbinder.h%3Bl%3D218%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/kernel/uapi/linux/android/binder.h;l=218;bpv=1;bpt=0") 、[BR](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fkernel%2Fuapi%2Flinux%2Fandroid%2Fbinder.h%3Bl%3D195%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/kernel/uapi/linux/android/binder.h;l=195;bpv=1;bpt=0")。BC 是由客户端/服务端发送给Binder 驱动的消息，BR 是 由 Binder 驱动处理过后，转发给客户端/服务端的消息。
    
-   数据：与消息码相关的数据，紧跟在消息码后面。例如，如果消息码是 BC\_TRANSACTION，则数据部分会包含事务的详细信息，如目标 Binder 对象、调用的代码的索引、调用参数等。
    

注意：不是所有的消息码后面，都会有对应的数据，如 BR\_TRANSACTION\_COMPLETE。

## ioctl 命令

Binder 消息，都是通过系统调用 ioctl() 来实现的。Binder 驱动的 ioctl 实现就是 [binder\_ioctl()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2F_%2Fandroid%2Fkernel%2Fcommon%2F%2B%2Fandroid13-5.15%3Adrivers%2Fandroid%2Fbinder.c%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D5439 "https://cs.android.com/android/_/android/kernel/common/+/android13-5.15:drivers/android/binder.c;bpv=0;bpt=0;l=5439")。

Binder 定义了一组 [ioctl 命令](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fkernel%2Fuapi%2Flinux%2Fandroid%2Fbinder.h%3Bl%3D130%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/kernel/uapi/linux/android/binder.h;l=130;bpv=1;bpt=0")，每个命令，都对应 Binder 系统中的一个操作。类似 Binder 消息，ioctl 命令通常也包括命令码和数据两部分。如：

-   BINDER\_SET\_MAX\_THREADS：设置线程池的最大线程数。该命令码后面紧跟一个无符号的32位整数，即我们设置的最大线程数。
-   BINDER\_WRITE\_READ：读写操作。Binder 消息就是通过这个 ioctl 命令发送的。该命令码后面，紧跟着一个 [binder\_write\_read](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fkernel%2Fuapi%2Flinux%2Fandroid%2Fbinder.h%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D90 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/kernel/uapi/linux/android/binder.h;bpv=1;bpt=0;l=90") 。

## binder\_write\_read

binder\_write\_read 的定义如下：

```
struct binder_write_read {
  binder_size_t write_size;
  binder_size_t write_consumed;
  binder_uintptr_t write_buffer;
  binder_size_t read_size;
  binder_size_t read_consumed;
  binder_uintptr_t read_buffer;
};
```

结构体中的字段有以下含义：

-   write\_size：用户空间希望写入驱动的字节数。
-   write\_consumed：驱动实际消耗的字节数。
-   write\_buffer：用户空间写入的数据的起始地址。
-   read\_size：用户空间希望从驱动中读取的字节数。
-   read\_consumed：驱动实际返回的字节数。
-   read\_buffer：用户空间从驱动中读取数据的缓冲区的起始地址。

在调用 BINDER\_WRITE\_READ 这个 ioctl 命令时，用户空间会设置 write\_size 和 write\_buffer 来指定要写入 Binder 驱动的数据，设置 read\_size 和 read\_buffer 来指定读取数据的缓冲区。Binder 驱动在处理完 ioctl 命令后，会更新 write\_consumed 和 read\_consumed 字段来反馈实际消耗和返回的字节数。

这个 ioctl 命令的用途非常广泛，包括但不限于向驱动发送 Binder 事务请求、接收来自驱动的回复和通知等。

## BINDER\_WRITE\_READ 命令与 Binder 消息

Binder 消息都是通过 BINDER\_WRITE\_READ 这个 ioctl 命令发送、读取的。一个 BINDER\_WRITE\_READ 里可以发送、读取多个 Binder 消息。如下：

![](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3511da646a5645cead8b8c632a9085d8~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

下面的代码揭示了如何通过一个 ioctl() 调用，发送 BC\_TRANSACTION 消息。

```
Parcel mOut; // 写缓冲区，由客户端进程写入 BC_TRANSACTION 相关数据
Parcel mIn;  // 读缓冲区，由Binder驱动写入 BR_TRANSACTION 等回复数据
binder_transaction_data tr; // Binder事务数据
Handle mHandle; // Binder的引用

tr.target.handle = handle;
tr.code = code;

mOut.writeInt32(BC_TRANSACTION);
mOut.write(&tr, sizeof(tr));

binder_write_read bwr;
bwr.write_size = mOut.dataSize();
bwr.write_buffer = (uintptr_t)mOut.data(); // 将写缓冲区的起始地址记录在 write_buffer 里
bwr.read_size = mIn.dataCapacity();
bwr.read_buffer = (uintptr_t)mIn.data();   // 将读缓冲区的起始地址记录在 read_buffer 里

ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr)
```

## Binder 事务

Binder 事务是指通过数个 Binder 消息的交互，完成跨进程的函数调用。分为同步事务、异步事务两种。

## 同步事务

同步事务是最常见的事务。当一个进程给另一个进程发送同步事务时，它会等待直到事务处理完毕。这意味着在事务处理期间，调用进程会被阻塞。当事务处理完毕后，会有一个回复数据，返回到调用进程，也就是客户端。

### 一次成功的 Binder 同步事务

![1684566948274.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e77db59ada634b46952996eb43d604e6~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

大致流程是：

-   客户端发起跨进程的函数调用时，会封装调用的函数的索引、调用参数等数据到 BC\_TRANSACTION 消息里，然后通过系统调用 ioctl ，发送给 Binder 驱动。
-   Binder 驱动接收到 BC\_TRANSACTION 消息后，会将其转换为 BR\_TRANSACTION 消息，发送给服务器，并挂起客户端线程。
-   服务器接收到 BR\_TRANSACTION 消息后，会读取消息里的数据，获得调用的函数的索引、调用参数等，完成函数调用，然后通过系统调用 ioctl 发送 BC\_REPLY 消息给 Binder 驱动。
-   Binder 驱动接收到 BC\_REPLY 消息后：
    -   回复 BR\_TRANSACTION\_COMPLETE 消息给服务端。
    -   回复 BR\_TRANSACTION\_COMPLETE 和 BR\_REPLY 消息给客户端，唤醒客户端线程。

注意：

-   一次成功的 Binder 同步事务里，有两次系统调用 ioctl。客户端、服务端各一次。
-   ⑤ 和 ⑥ 的消息是按顺序读取到的，但 ④ 不一定先于 ⑤ 被读取到，因为两者分别发生在服务端、客户端的线程里。服务端的线程接收到 BR\_TRANSACTION\_COMPLETE 消息后，也不急着做处理，在工作队列为空的情况下，通常会先挂起，直到工作队列不为空。
-   BR\_TRANSACTION\_COMPLETE 和 BR\_REPLY 消息其实都是在客户端的一次 ioctl 调用返回后，从读缓冲区里读取到的。

### 一次失败的 Binder 同步事务

失败的情况有很多种，比如服务端不存在、客户端已死亡、缓冲区溢出等等。这里举两个可能的例子：

#### 服务端不存在

![1684567039308.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c52555d4a1ec40e194af8eff207ed0be~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

Binder 驱动在处理 BC\_TRANSACTION 消息时，发现消息中指向服务端的引用是一个无效引用，就会回复 BR\_FAILED\_REPLY 消息给客户端。

#### 客户端已死亡

![1684567150346.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/017c1b6646be46e48e4ae7af43aed142~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

Binder 驱动在处理服务端的 BC\_REPLY 消息时，发现这时候客户端已经死亡，不需再回复客户端。不过，仍然需要回复 BR\_TRANSACTION\_COMPLETE 消息给服务端。

## 异步事务

异步事务是一种不需要回复的事务，它会在事务数据的 flags 中增加一个 TF\_ONE\_WAY 标志位。当一个进程发送一个异步事务到另一个进程时，它会立即返回，不需要等待事务的完成。这意味着异步事务是非阻塞的，所以它们对于需要快速响应的场景很有用。然而由于没有回复，客户端无法知道事务何时完成以及是否成功。

### 一次成功的异步事务

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ad1ed375abf64c2db43b79f82f2dceaa~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

大致流程是：

-   客户端发起跨进程的函数调用时，会封装调用的函数的索引、调用参数等数据到 BC\_TRANSACTION 消息里，然后通过系统调用 ioctl ，发送给 Binder 驱动。
-   Binder 驱动接收到 BC\_TRANSACTION 消息后，会将其转换为 BR\_TRANSACTION 消息，发送给服务器。这时候客户端线程不会被挂起，也不用等服务器处理结果。Binder 驱动会直接回复 BR\_TRANSACTION\_COMPLETE 消息给客户端。
-   服务器接收到 BR\_TRANSACTION 消息后，会读取消息里的数据，获得调用的函数的索引、调用参数等，完成函数调用（失败也不会通知 Binder 驱动）。

## Binder 对象 ：flat\_binder\_object

事务的数据，通常会包含多种数据类型，主要分为两种：普通的数据类型和对象。

-   普通的数据类型：int、float、double 等这些基本数据类型、指针和 string 等。
-   对象：Binder中的对象指的就是 [flat\_binder\_object](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fkernel%2Fuapi%2Flinux%2Fandroid%2Fbinder.h%3Bl%3D54%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/kernel/uapi/linux/android/binder.h;l=54;bpv=1;bpt=0")。

flat\_binder\_object 的定义如下：

```
struct flat_binder_object {
  struct binder_object_header hdr;
  __u32 flags;
  union {
    binder_uintptr_t binder;
    __u32 handle;
  };
  binder_uintptr_t cookie;
};
```

-   hdr：结构体 [binder\_object\_header](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fkernel%2Fuapi%2Flinux%2Fandroid%2Fbinder.h%3Bl%3D51%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/kernel/uapi/linux/android/binder.h;l=51;bpv=1;bpt=0") 记录了一个32位的无符号整数，代表对象的类型。 Binder 中对象的类型由一个匿名枚举[定义](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fkernel%2Fuapi%2Flinux%2Fandroid%2Fbinder.h%3Bl%3D25%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/kernel/uapi/linux/android/binder.h;l=25;bpv=1;bpt=0")（文末附录有所有类型的介绍）。下面只讨论其中两种：
    
    -   BINDER\_TYPE\_BINDER：指 Binder 实体
    -   BINDER\_TYPE\_HANDLE：指 Binder 引用
-   binder：指向 Binder 实体的指针。
    
-   handle：Binder 引用。
    

普通的数据类型传递给 Binder 驱动的时候，会直接拷贝到接收缓冲区。而**对象不一样，需要做特殊处理**。比如 Binder 实体传递给 Binder 驱动的时候，会被逐个处理，转换成 Binder 引用。

## 事务数据 ：binder\_transaction\_data

[binder\_transaction\_data](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fkernel%2Fuapi%2Flinux%2Fandroid%2Fbinder.h%3Bl%3D150 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/kernel/uapi/linux/android/binder.h;l=150") 描述的 Binder 事务的相关数据，是 BC\_TRANSACTION、BC\_REPLY、BR\_TRANSACTION 和 BR\_REPLY 这几个消息后面的数据。

binder\_transaction\_data 的定义如下：

```
struct binder_transaction_data {
  union {
    __u32 handle;
    binder_uintptr_t ptr;
  } target;
  __u32 code;
  __u32 flags;
  __kernel_pid_t sender_pid;
  __kernel_uid32_t sender_euid;
  binder_size_t data_size;
  binder_size_t offsets_size;
  union {
    struct {
      binder_uintptr_t buffer;
      binder_uintptr_t offsets;
    } ptr;
    __u8 buf[8];
  } data;
};
```

-   handle：Binder 引用
-   ptr：一个指针，指向 Binder 实体
-   code：期望调用的函数的索引
-   flags：表示事务的类型，[transaction\_flags](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fkernel%2Fuapi%2Flinux%2Fandroid%2Fbinder.h%3Bl%3D143%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/kernel/uapi/linux/android/binder.h;l=143;bpv=1;bpt=0") 是它的相关定义。例如，TF\_ONE\_WAY 表示这是一个异步事务。
-   sender\_pid、sender\_euid：发送此 Binder 事务的进程的 PID 和有效用户 ID。
-   data\_size：缓冲区的数据大小
-   offsets\_size：offsets 数组的大小
-   buffer：一个指针，指向用于存放传输数据的用户空间缓冲区。当一个进程想要通过 Binder 事务发送数据时，它会把要发送的数据写入一个用户空间缓冲区，然后把这个缓冲区的地址通过 buffer 字段传给 Binder 驱动。
-   offsets：一个指针，指向用户空间中的一个数组。该数组记录了多个 flat\_binder\_object 的地址相比较于 buffer 的偏移量 offset。

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/645a5a318a5d4dd585b0243386a47ed3~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

这是一段我们想要发送给其他进程的数据。buffer 就是指向这段数据的起始地址。而 offsets 则是指向数据里 offsets 数组。offsets\[0\] 、offsets\[1\] 则分别代表数据里两个 flat\_binder\_object 的地址相比较于 buffer 的偏移量，即 offset\[i\] - buffer。

可以发现，int、float、flat\_binder\_object 这些数据并不遵循一定的顺序，写入到缓冲区中。所以，这就要求**在服务端里，我们必须按照写入的顺序，读取接收缓冲区里的数据。**

## Binder 实体、Binder 代理、Binder 节点和 Binder 引用

Binder 实体、Binder 代理是 Framework 层的抽象概念。Binder 节点和 Binder 引用则是内核层的抽象概念。它们的关系如下：

![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b8b112db1d254b1faf3bb12e26449dfd~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

**Binder 实体**：**它是 Binder 事务的处理者**。它是 Framework 层的抽象，实现类是 [BBinder](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2Finclude%2Fbinder%2FBinder.h%3Bl%3D30 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/include/binder/Binder.h;l=30")，表示的是一个真实存在并且能够处理 IPC 调用的对象。BBinder 通常在服务端进程创建，并在客户端进程中用对应的 Binder 代理来表示。

**Binder 代理**：**它是 Binder 事务的发起者**。它是 Framework 层的抽象，实现类是 [BpBinder](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2Finclude%2Fbinder%2FBpBinder.h%3Bl%3D38 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/include/binder/BpBinder.h;l=38")，作用于客户端。它会记录 Binder 引用。

**Binder 节点**：**它是 Binder 实体在内核层的表示**。它的实现类是 [binder\_node](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_internal.h%3Bl%3D232 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_internal.h;l=232")。

**Binder 引用**：**它是对 Binder 节点的引用**。它是内核空间的抽象，它的实现类是 [binder\_ref](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_internal.h%3Bl%3D318 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_internal.h;l=318")。

把 Binder 实体跨进程传递时，其实就是把指向该 Binder 实体（即 BBinder 对象）的指针存储在 flat\_binder\_object 中，驱动会在为当前进程维护的红黑树里为它创建对应的 Binder 节点，然后在为目标进程的维护的红黑树里里，创建对应的 Binder 引用， 最后把引用传递给目标进程，并被记录在对应的 BpBinder 里。

我们想要调用另一个进程的函数，必须先持有另一个进程的能调用这个函数的 Binder 实体相对应的 Binder 代理。这就要求**另一个进程必须先把该 Binder 实体传递到我们当前进程**。

下图展示了将 Binder 实体通过一个 Binder 事务第一次传递给其他进程时所发生的事：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/92a6fc997a1d460bb79ef1c72412a8c6~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

-   ① 传递 Binder 实体，就是将 BBinder 的指针，写进 flat\_binder\_object 的 binder 字段里。
-   ② Binder 实体的指针在 Binder 驱动里，会被记录在 Binder 节点中，即 binder\_node。binder\_node 是 Binder 驱动在内核中维护的一个数据结构，它表示一个 Binder 实体。binder\_node 结构中存储了与该 Binder 实体相关的信息，这包括 Binder 实体所在的进程、Binder 实体的指针等。
-   ③ 内核会为 binder\_node 创建对应的 Binder 引用，即 binder\_ref。binder\_ref 记录着对应的 binder\_node 相关信息。
-   ④ proc、target\_proc 都是结构体 [binder\_proc](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_internal.h%3Bl%3D428 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_internal.h;l=428") 的对象，分别对应客户端进程、服务端进程。binder\_proc 是 Binder 驱动在内核维护的一个数据结构，代表一个使用 Binder 通信机制的进程，记录着进程相关信息。它维护着三棵红黑树：
    -   nodes：存储着 binder\_node，按 Binder 实体的指针大小排序。目的是为了后续根据 Binder 实体的指针，在 O(logn) 的时间复杂度内，从所在进程的 nodes 红黑树中获得、删除对应的 binder\_node。
    -   refs\_by\_node：存储着 binder\_ref，按 binder\_ref 对应的 binder\_node 的地址大小排序。目的是为了后续根据 binder\_node 的指针，在 O(logn) 的时间复杂度内，从目标进程的 refs\_by\_node 红黑树中获得、删除对应的 binder\_ref。
    -   refs\_by\_desc：存储着 binder\_ref，按引用大小排序（desc 即引用，一个内核分配的 int 值）。目的是为了后续根据 binder\_ref 的引用，在 O(logn) 的时间复杂度内，从目标进程的 refs\_by\_desc 红黑树中获得、删除对应的 binder\_ref。
-   ⑤ 内核为 Binder 实体创建对应的 binder\_node 后，会将其插进所在进程的 nodes 红黑树里。
-   ⑥ 内核为 Binder 实体创建对应的 binder\_ref 后，会将其插进目标进程的 refs\_by\_node 里。
-   ⑦ 内核为 Binder 实体创建对应的 binder\_ref 后，会将其插进目标进程的 refs\_by\_desc 里。
-   ⑧ 在 Binder 驱动中，从用户空间，拷贝数据到内核缓冲区时，会逐个处理 flat\_binder\_object，将 binder 置为 0，将 handle 赋值为 desc。此外，还会将 hdr 的类型，从 BINDER\_TYPE\_BINDER 更改为 BINDER\_TYPE\_HANDLE。
-   ⑨ 将经过内核修改后的 flat\_binder\_object 传送给服务端，并根据 flat\_binder\_object 里的 handle，创建对应的 BpBinder。

注意，客户端、服务端只是相对而言的。图中是指发起事务的是客户端。当服务端拿到 Binder 实体的引用时，就可以构建一个对应的 Binder 代理，即 BpBinder，向 Binder 实体发起事务。这时候，它就是客户端，而 Binder 实体所在进程就是服务端。

细心的读者，可能会有一个疑问：Binder 实体需要由一个事务发送出去。但是事务是要由一个 Binder 代理发起的，那么最开始的一个 Binder 代理，它的 Binder 引用又是从哪里来的呢？

这其实就是个鸡生蛋，蛋生鸡，到底是先有鸡还是先有蛋的问题。ServiceManager 就是那只最开始的鸡。后续在介绍 ServiceManager 时，会解释这个问题。

## Binder 事务相关调用链路

Binder 事务相关调用链路里，还涉及了 Binder 线程及相关概念，建议在阅读下一章 Binder 线程后，再进行深入。

**客户端发起事务：**

BpBinder.cpp

└─[transact()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FBpBinder.cpp%3Bl%3D275%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/BpBinder.cpp;l=275;bpv=1;bpt=0")

└──IPCThreadState.cpp

└───[self()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D287 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=287")

└────[transact()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D706 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=706")

└─────[writeTransactionData()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D1104 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=1104")

└─────[waitForResponse()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D897 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=0;bpt=0;l=897")

└──────[talkWithDriver()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D997 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=0;bpt=0;l=997") // 调用 ioctl()，发送 BC\_TRANSACTION 消息

└───────ioctl.cpp

└────────[ioctl()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fbionic%2Fioctl.cpp%3Bl%3D34%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/bionic/ioctl.cpp;l=34;bpv=1;bpt=0")

└─────────binder.c （这里已经是内核层。是 Binder 驱动相关代码）

└──────────[binder\_ioctl()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D5439 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;bpv=1;bpt=0;l=5439")

└───────────[binder\_ioctl\_write\_read()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D5169 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;bpv=1;bpt=0;l=5169")

└────────────[binder\_thread\_write()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D3904 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;bpv=1;bpt=0;l=3904") // 读取写缓冲区里的 Binder 消息的消息码，分类处理

└─────────────[binder\_transaction()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D3040 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;bpv=0;bpt=0;l=3040") // 处理写缓冲区里的事务数据，通知服务端

└────────────[binder\_thread\_read()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D4492 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;bpv=1;bpt=0;l=4492") // 挂起等待服务端的回复，回复最终会写入读缓冲区

**服务端接收事务：**

IPCThreadState.cpp

└─[joinThreadPool()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D638 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=638") // Binder 线程池的线程在启动时，会在这里陷入轮询。轮询处理 Binder 消息

└──[getAndExecuteCommand()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D540 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=540")

└───[talkWithDriver()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D997 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=997")

└────ioctl.cpp

└─────[ioctl()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fbionic%2Fioctl.cpp%3Bl%3D34%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/bionic/ioctl.cpp;l=34;bpv=1;bpt=0")

└──────binder.c （这里已经是内核层。是 Binder 驱动相关代码）

└───────[binder\_ioctl()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D5439 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;bpv=1;bpt=0;l=5439")

└────────[binder\_ioctl\_write\_read()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D5169 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;bpv=1;bpt=0;l=5169")

└─────────[binder\_thread\_read()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D4492 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;bpv=1;bpt=0;l=4492") // 进入 waiting\_threads，挂起，等待消息

└───IPCThreadState.cpp （这里重新回到用户空间，并完成 talkWithdDriver()调用）

└────[executeCommand()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D1147 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=1147") // 读取读缓冲区数据，处理 BR\_TRANSACTION 消息

└─────Binder.cpp

└──────[BBinder::transact()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FBinder.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D270 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/Binder.cpp;bpv=1;bpt=0;l=270") // 根据事务数据里的函数索引、参数，完成函数调用。

└────IPCThreadState.cpp （这里完成了 BBinder::transact() 调用，回到了 executeCommand() ）

└─────[sendReply()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D887 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=887") // 发送 BC\_REPLY 消息

└──────[writeTransactionData()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D1104 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=1104")

└──────[waitForResponse()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D897 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=0;bpt=0;l=897")

└───────[talkWithDriver()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D997 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=0;bpt=0;l=997") // 调用 ioctl()，发送 BC\_REPLY 消息

└────────后续调用栈与客户端发送 BC\_TRANSACTION 消息类似...

## 附录

## Binder 消息码

**BC 消息码**

| BC 消息码 | 描述 |
| --- | --- |
| BC\_TRANSACTION | 发起一个Binder事务（包含数据和目标Binder引用） |
| BC\_REPLY | 回复一个Binder事务 |
| BC\_ACQUIRE\_RESULT | 通知驱动该引用是否被接收 |
| BC\_FREE\_BUFFER | 释放由Binder驱动提供的数据缓冲区 |
| BC\_INCREFS | 增加Binder实体的引用计数 |
| BC\_ACQUIRE | 增加Binder实体的活跃引用计数 |
| BC\_RELEASE | 减少Binder实体的活跃引用计数 |
| BC\_DECREFS | 减少Binder实体的引用计数 |
| BC\_INCREFS\_DONE | 完成对Binder实体的引用计数的增加 |
| BC\_ACQUIRE\_DONE | 完成对Binder实体的活跃引用计数的增加 |
| BC\_ATTEMPT\_ACQUIRE | 尝试获取Binder实体的引用 |
| BC\_REGISTER\_LOOPER | 注册一个工作线程 |
| BC\_ENTER\_LOOPER | 通知驱动主线程进入looper模式 |
| BC\_EXIT\_LOOPER | 通知驱动主线程/工作线程退出looper模式 |
| BC\_REQUEST\_DEATH\_NOTIFICATION | 请求Binder实体死亡通知 |
| BC\_CLEAR\_DEATH\_NOTIFICATION | 清除Binder实体死亡通知 |
| BC\_DEAD\_BINDER\_DONE | 通知驱动已经处理完死亡通知 |
| BC\_TRANSACTION\_SG | 与BC\_TRANSACTION类似，但使用scatter-gather I/O（SG I/O）传输数据 |
| BC\_REPLY\_SG | 与BC\_REPLY类似，但使用scatter-gather I/O（SG I/O）传输数据 |

**BR 消息码**

| BR 消息码 | 描述 |
| --- | --- |
| BR\_ERROR | Binder驱动返回的错误码 |
| BR\_OK | 事务成功完成（主要用在结构体 binder\_error 里，一般用户进程不会接收到） |
| BR\_TRANSACTION\_SEC\_CTX | 收到一个带有安全上下文的Binder事务请求 |
| BR\_TRANSACTION | 收到一个Binder事务请求 |
| BR\_REPLY | 收到一个Binder事务的回复 |
| BR\_ACQUIRE\_RESULT | 获取引用的结果 |
| BR\_DEAD\_REPLY | 事务的目标已经死亡 |
| BR\_TRANSACTION\_COMPLETE | 事务完成 |
| BR\_INCREFS | 增加Binder实体的引用计数 |
| BR\_ACQUIRE | 增加Binder实体的活跃引用计数 |
| BR\_RELEASE | 减少Binder实体的活跃引用计数 |
| BR\_DECREFS | 减少Binder实体的引用计数 |
| BR\_ATTEMPT\_ACQUIRE | 尝试获取Binder实体的引用 |
| BR\_NOOP | 无操作 |
| BR\_SPAWN\_LOOPER | 需要创建一个新的looper线程 |
| BR\_FINISHED | 线程完成了所有事务并可以安全退出 |
| BR\_DEAD\_BINDER | Binder实体已死亡 |
| BR\_CLEAR\_DEATH\_NOTIFICATION\_DONE | 已完成对Binder实体死亡通知的清除 |
| BR\_FAILED\_REPLY | 回复失败 |
| BR\_FROZEN\_REPLY | 冻结的回复 |
| BR\_ONEWAY\_SPAM\_SUSPECT | 疑似单向消息垃圾邮件 |

## ioctl 命令

| ioctl 命令 | 描述 |
| --- | --- |
| BINDER\_WRITE\_READ | 最常用的 ioctl 命令。它允许用户空间进程向 Binder 驱动写入一系列 Binder 命令（例如，BC\_TRANSACTION、BC\_REPLY 等），并从 Binder 驱动读取回复（例如，BR\_TRANSACTION\_COMPLETE、BR\_TRANSACTION 等）。这个命令的参数是一个 binder\_write\_read 结构体，它指定了写入和读取的数据缓冲区。 |
| BINDER\_SET\_IDLE\_TIMEOUT | 用于设置 Binder 驱动的空闲超时时间。如果 Binder 驱动在这个时间内没有处理任何事务，它将进入休眠状态以节省资源。这个命令的参数是一个以纳秒为单位的超时时间 |
| BINDER\_SET\_MAX\_THREADS | 设置 Binder 驱动可以创建的最大线程数。这个命令的参数是一个整数，表示最大线程数。 |
| BINDER\_SET\_CONTEXT\_MGR | 设置当前进程为 Binder 上下文管理器。Binder 上下文管理器是一个特殊的进程，它负责管理所有的 Binder 引用。这个命令没有参数。 |
| BINDER\_THREAD\_EXIT | 用于通知 Binder 驱动当前线程正在退出。这可以让 Binder 驱动清理与该线程相关的资源。这个命令没有参数。 |
| BINDER\_VERSION | 获取 Binder 驱动的版本号。可以用来检查用户空间和内核空间的 Binder 实现是否兼容。这个命令的参数是一个 binder\_version 结构体，Binder 驱动会将其版本号写入该结构体。 |
| BINDER\_SET\_IDLE\_PRIORITY | 设置 Binder 驱动在空闲状态下的线程优先级。参数是一个整数，表示线程优先级。 |
| BINDER\_GET\_NODE\_DEBUG\_INFO | 获取 Binder 节点的调试信息。参数是一个 binder\_node\_debug\_info 结构体，Binder 驱动会将调试信息写入该结构体。 |
| BINDER\_GET\_NODE\_INFO\_FOR\_REF | 用于获取 Binder 引用对应的节点信息。参数是一个 binder\_node\_info\_for\_ref 结构体，Binder 驱动会将节点信息写入该结构体。 |
| BINDER\_SET\_CONTEXT\_MGR\_EXT | 这是一个扩展的版本的 BINDER\_SET\_CONTEXT\_MGR 命令，它允许设置额外的上下文管理器参数。参数是一个 binder\_context\_mgr\_ext 结构体。 |
| BINDER\_FREEZE | 用于冻结或解冻 Binder 驱动。参数是一个 binder\_freeze\_info 结构体，用于指定冻结的参数。 |
| BINDER\_GET\_FROZEN\_INFO | 用于获取 Binder 驱动的冻结信息。参数是一个 binder\_frozen\_info 结构体，Binder 驱动会将冻结信息写入该结构体。 |
| BINDER\_ENABLE\_ONEWAY\_SPAM\_DETECTION | 用于启用或禁用 Binder 的单向垃圾消息检测。参数是一个布尔值，表示是否启用检测。 |
| BINDER\_GET\_EXTENDED\_ERROR | 用于获取 Binder 驱动的扩展错误信息。参数是一个 binder\_extended\_error 结构体，Binder 驱动会将错误信息写入该结构体。 |

## Binder 对象的类型

flat\_binder\_object的类型由一个匿名枚举[定义](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fkernel%2Fuapi%2Flinux%2Fandroid%2Fbinder.h%3Bl%3D25%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/kernel/uapi/linux/android/binder.h;l=25;bpv=1;bpt=0")。

| 枚举值 | 描述 |
| --- | --- |
| BINDER\_TYPE\_BINDER | 表示一个 Binder实 体（ server 提供的服务）。在这种情况下，binder 字段指向 Binder 实体的地址。 |
| BINDER\_TYPE\_WEAK\_BINDER | 类似 BINDER\_TYPE\_BINDER，但表示的是对 Binder 实体的弱指针，而不是强指针。 |
| BINDER\_TYPE\_HANDLE | 表示一个 Binder 引用（客户端使用的服务句柄）。在这种情况下，handle 字段即Binder引用。 |
| BINDER\_TYPE\_WEAK\_HANDLE | 类似 BINDER\_TYPE\_HANDLE，但表示的是对 Binder 引用的弱引用，而不是强引用。 |
| BINDER\_TYPE\_FD | 表示一个文件描述符。在这种情况下，handle 字段包含文件描述符的值。 |
| BINDER\_TYPE\_FDA | 表示一个文件描述符数组。在这种情况下，binder 字段包含文件描述符数组的地址，而 cookie 字段包含数组的长度。 |
| BINDER\_TYPE\_PTR | 表示一个指针。在这种情况下，binder 字段包含指针的值，而 cookie 字段包含额外的数据 |
