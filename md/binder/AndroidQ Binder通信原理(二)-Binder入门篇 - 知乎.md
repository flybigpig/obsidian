---
created: 2025-05-21T09:57:28 (UTC +08:00)
tags: [Android,Android框架]
source: https://zhuanlan.zhihu.com/p/2621079498
author: 关于作者麦芽知行合一，笃行致远，知易行难。回答7文章94关注者139关注他发私信
---

# AndroidQ Binder通信原理(二)-Binder入门篇 - 知乎

> ## Excerpt
> 摘要：本节主要来讲解Android10.0 Binder的设计原理，如何设计一个Binder通信1.概述在Android应用、Android系统开发的时候，相信很多人都听过Binder的概念，而且无意间就用到了Binder机制，例如：写一个应用打开手…

---
**摘要：**本节主要来讲解Android10.0 Binder的设计原理，如何设计一个Binder通信

## 1.概述

在Android应用、Android系统开发的时候，相信很多人都听过Binder的概念，而且无意间就用到了Binder机制，例如：写一个应用打开手电筒功能，某个应用启动服务等。

这些动作都涉及到一个概念-进程间通信。Android 中的每个应用都是独立的进程，都有自己虚拟内存，两个进程之间不能互相访问数据。

在Android中，应用进程间互相访问数据，常用的通信方式就Binder。

从前一节，我们知道从Android 8.0 开始，Binder机制，被拆分成了Binder(System分区 进程间通信)、[HwBinder](https://zhida.zhihu.com/search?content_id=249546969&content_type=Article&match_order=1&q=HwBinder&zhida_source=entity)(支持System/Vendor分区进程间通信)、[VndBinder](https://zhida.zhihu.com/search?content_id=249546969&content_type=Article&match_order=1&q=VndBinder&zhida_source=entity)(Vendor分区进程间通信)。

现在我们先单独分析一下Binder的机制，HwBinder和VndBinder留到后面慢慢分析。

下图中涉及到Binder模型的4类角色：[Binder驱动](https://zhida.zhihu.com/search?content_id=249546969&content_type=Article&match_order=1&q=Binder%E9%A9%B1%E5%8A%A8&zhida_source=entity)，ServiceManager，Server和Client。Binder机制的目的是实现IPC(Inter-Process Communication)，即Client和Server之间的通信。

其中Server，Client，ServiceManager运行于用户空间，Binder驱动运行于内核空间。这四个角色的关系和互联网类似：Server是服务器，Client是客户终端，ServiceManager是域名服务器（DNS），驱动是路由器。

![](https://pic3.zhimg.com/v2-047a1a73aaf6508fe3f8f49901fbcf06_1440w.jpg)

## 3.一些概念的理解

### 3.1 IPC (进程间通信-Inter process communication)

IPC属于通信机制，Android中常用的IPC通信：管道、共享内存、消息队列、信号量、socket、binder。

### 3.2 [RPC](https://zhida.zhihu.com/search?content_id=249546969&content_type=Article&match_order=1&q=RPC&zhida_source=entity) (远程过程调用 Remote Procedure call)

RPC属于通信机制中的调用方法，目的：不同的进程之间，一个进程调用另一个进程的对象。

RPC在调用一个远程过程后，自己进入等待状态，传往远程过程的参数包括过程参数，返回参数包括执行结果；

当收到包括执行结果的消息后，本地进程从消息中取得结果，调用进程重新开始执行。在服务器一方，有一个程序在等待调用，当有一个调用到达时，服务器进程取得进程参数，计算结果，然后返回结果。

调用可以同步的也可以是异步的；服务器可以创建一个线程来接收用户请求，也可以自己来接收用户请求

### 3.3 代理模式

为其他对象提供代理对象，以控制对这个对象的访问。

由于[进程隔离](https://zhida.zhihu.com/search?content_id=249546969&content_type=Article&match_order=1&q=%E8%BF%9B%E7%A8%8B%E9%9A%94%E7%A6%BB&zhida_source=entity)的存在，一个进程内部的对象对另外一个进程来说没有任何意义。另外如果是代理对象的话，它可以存在各个进程内，就好比咱们的AMS和PMS。

### 3.4 进程隔离

进程隔离是为保护操作系统中进程互不干扰而设计的一组不同硬件和软件的技术。这个技术是为了避免进程A写入进程B的情况发生。进程的隔离实现，使用了虚拟地址空间。进程A的虚拟地址和进程B的虚拟地址不同，这样就防止进程A将数据信息写入进程B。

### 3.5 内核空间

可以访问受保护的内存空间，有访问底层硬件设备的所有权限。

### 3.6 用户空间

相对于内核空间，上层应用程序所运行的空间以及Native层进程运行的空间就是用户空间，用户空间访问内核空间的唯一方式就是[系统调用](https://zhida.zhihu.com/search?content_id=249546969&content_type=Article&match_order=1&q=%E7%B3%BB%E7%BB%9F%E8%B0%83%E7%94%A8&zhida_source=entity)。

### 3.7 系统调用/内核态/用户态

从逻辑上看，内核空间和用户空间是独立的，那么用户空间总要有办法调用内核空间，唯一的调用方式就是系统调用(System Call)，通过这个统一入口接口，所有的资源访问都是在内核的控制下执行，以免导致对用户程序对系统资源的越权访问，从而保障了系统的安全和稳定。

当一个任务（进程）执行系统调用而陷入内核代码中执行时，我们就称进程处于内核运行态（或简称为内核态）。

当进程在执行用户自己的代码时，则称其处于用户运行态（用户态）。

## 4.什么是Binder？

Binder的英文意思是粘结剂，把两个不相关的进程粘在一起，让两个进程可以进行数据交互。 比如我们写一个应用，打开手电筒功能，那么要把打开这个动作，发给管理服务，这个发送的过程就是通过Binder机制来实现。

## 5.为什么要用Binder？

Android使用的Linux内核拥有着非常多的跨进程通信机制，比如管道、共享内存、消息队列、信号量、socket等；为什么还需要单独搞一个Binder出来呢？

主要有两点，性能和安全。

### 5.1 高效

在移动设备上，广泛地使用跨进程通信肯定对通信机制本身提出了严格的要求；Binder相对出传统的Socket方式，更加高效；

![](https://pic1.zhimg.com/v2-6aabdaadf7f95015f8f3d506ab4fdea2_1440w.jpg)

IPC 内存拷贝次数

Binder只需要一次拷贝就能将A进程用户空间的数据为B进程所用

数据从用户空间拷贝到内核中的时候，是直接拷贝到目标进程的内核空间，这个过程是在请求端线程中处理的，只不过操作对象是目标进程的内核空间。

Binder拷贝方式： 数据发送端(虚拟内存) copy\_from\_user --> 内核虚拟内存 <--mmap--> 数据接收端(虚拟内存)

内核虚拟内存和数据接收端虚拟内存采用mmap映射到同一块物理内存，不存在拷贝动作，数据发送端(Client)要把IPC数据 拷贝到内核虚拟内存空间，存在一次拷贝，所以Binder只存在一次内存拷贝

管道、队列等需要两次内存拷贝：

发送方缓存区--memcpy-->内核缓存区 --memcpy-->接收方缓存区 ，存在两次拷贝

共享内存SMD(Shared Memory Driver)，虽然无需拷贝，但控制复杂，难以使用。

socket作为一款通用接口，其传输效率低，开销大，主要用在跨网络的进程间通信和本机上进程间的低速通信。

### 5.2 安全

传统的进程通信方式对于通信双方的身份并没有做出严格的验证，接收方无法获得对方进程可靠的UID/PID(用户ID/进程ID)，只有在上层协议上进行架设；

比如Socket通信ip地址是客户端手动填入的，都可以进行伪造；而Binder机制为每个进程分配了UID/PID来作为鉴别身份的标示，从协议本身就支持对通信双方做身份校检，因而大大提升了安全性。

### 5.3 可以很好的实现Client-Server(CS)架构

对于Android系统，Google想提供一套基于Client-Server的通信方式。当Client需要获取某Server的服务时，只需要Client向Server发送相应的请求，Server收到请求之后进行处理，处理完毕再将反馈内容发送给Client。

但是，目前Linux支持的"传统的管道/消息队列/共享内存/信号量/Socket等"IPC通信手段中，只有Socket是Client-Server的通信方式。但是，Socket主要用于网络间通信以及本机中进程间的低速通信，它的传输效率太低。

## 6.如何设计一个Binder？

在理解Binder架构前，我们来考虑下，如果是你，该如何设计一个Binder的进程间通信机制。

要实现一个IPC通信那么需要几个核心要素：

-   1)发起端：肯定包括发起端所从属的进程，以及实际执行传输动作的线程
-   2)接收端：接收发送端的数据。
-   3)待传输的数据
-   4)内存映射，内核态

**首先先画一个最简单的IPC通信图：**

**![](https://pic4.zhimg.com/v2-4073d303e4b4ce63fa02d9c7dada301d_1440w.jpg)**

进程Process1和进程Process2 通过IPC通信机制进行通信。

再进行扩展调整，把IPC机制换成Binder机制，那么就变成如下的图形：

![](https://pic1.zhimg.com/v2-f5f9c2108ed661a40bf07f49a165b002_1440w.jpg)

由于Android存在进程隔离，那么两个进程之间是不能直接传输数据的，Process1需要得到Process2的代理，Process2需要一个实体。

![](https://picx.zhimg.com/v2-5c726e6ec2543e76be967b1c0d01e2b7_1440w.jpg)

为了实现RPC，我们的代理都是提供接口，称为“接口代理”，实体需要提供“接口实体”，如下图所示：

![](https://pic1.zhimg.com/v2-7fef76e5a124e25fcd36c99261758fb6_1440w.jpg)

我们把代理改成BpBinder，实体改成BBinder，接口代理改成BpInterface，接口实现体改成BnInterface。

我们都知道两个进程的数据共享，需要陷入内核态，那就需要一个驱动设备“/dev/binder”，同时需要一个守护进行来进行service管理，我们成为ServiceManager。

![](https://picx.zhimg.com/v2-6d635404769367ff2b4cbbc527320af5_1440w.jpg)

**假如我们想要把通过Process1 的微信信息发送给Process2的微信，我们需要做下面几步：**

-   0)Process2在腾讯服务器中进行注册(包括微信名称、当前活动的IP地址等)
-   1)Process1从朋友列表中中查找到 Process2的名称，这就是Process2的别名："service\_name"
-   2)Process1 编写消息消息内容，点击发送。 这里的消息内容就是IPC数据
-   3)数据会发送到腾讯的服务器，服务器理解为Binder驱动
-   4)服务器从数据库中解析出IPC数据，找到Process2信息，转到Process2注册的地址， 数据库理解为ServiceManager
-   5)把数据发给Process2，完成Process1和Process2的通信

**我们可以简单的把上面的顺序内容进行转换**：

-   1)Binder驱动---腾讯服务器
-   2)数据库--ServiceManager
-   3)Service\_name: Process2的微信名称
-   4)IPC数据：Process1 发送的微信消息

Native C/C++和内核进行通信需要通过系统调用，ServiecManager的主要用来对Service管理，提供了add\\find\\list等操作。Native进程的数据直接可以通过系统调用陷入内核态，进入图像转换，变为如下:

![](https://picx.zhimg.com/v2-212b8686da8f3968cbc7e888b9e9288b_1440w.jpg)

上面列举的是Native C/C++空间的进程进行Binder通信机制，那么JAVA层是如何通信的呢，Native层的Binder提供的是libbinder.so，那么从JAVA到Native，需要经过JNI、Framework层的封装，

JNI层的命名通常为android\_util\_xxx，我们这里是binder机制，那么JNI层的文件为 android\_util\_binder，同时Native的BBinder不能直接传给JAVA层，在JNI里面转换了一个JavaBBinder对象。

Framework层给应用层提供时，其实提供的也是一个代理，我们也称之为BinderProxy。

在JAVA侧要对应一个Binder的实体，称之为Binder。

JAVA侧的服务进行也需要一个管理者，类似于Native，创建了JAVA的ServiceManager，那么设计如下：

![](https://pic4.zhimg.com/v2-8d181e71729f1a895f07fa719c714241_1440w.jpg)

## 7.Binder的通信原理

Binder 通信采用 C/S 架构，从组件视角来说，包含 Client、 Server、 ServiceManager 以及 Binder 驱动，其中 ServiceManager 用于管理系统中的各种服务。

Binder 在 framework 层进行了封装，通过 JNI 技术调用 Native（C/C++）层的 Binder 架构。

Binder 在 Native 层以 ioctl 的方式与 Binder 驱动通讯。

**Binder通信流程如下：**

1.  首先服务端需要向ServiceManager进行服务注册，ServiceManager有一个全局的service列表svcinfo，用来缓存所有服务的handler和name。
2.  客户端与服务端通信，需要拿到服务端的对象，由于进程隔离，客户端拿到的其实是服务端的代理，也可以理解为引用。客户端通过ServiceManager从svcinfo中查找服务，ServiceManager返回服务的代理。
3.  拿到服务对象后，我们需要向服务发送请求，实现我们需要的功能。通过 BinderProxy 将我们的请求参数发送给 内核，通过共享内存的方式使用内核方法 copy\_from\_user() 将我们的参数先拷贝到内核空间，这时我们的客户端进入等待状态。然后 Binder 驱动向服务端的 todo 队列里面插入一条事务，执行完之后把执行结果通过 copy\_to\_user() 将内核的结果拷贝到用户空间（这里只是执行了拷贝命令，并没有拷贝数据，binder只进行一次拷贝），唤醒等待的客户端并把结果响应回来，这样就完成了一次通讯。
4.  在这里其实会存在一个问题，Client和Server之间通信是称为进程间通信，使用了Binder机制，那么Server和ServiceManager之间通信也叫进程间通信，Client和Server之间还会用到ServiceManager，也就是说Binder进程间通信通过Binder进程间通信来完成，这就好比是 孵出鸡前提却是要找只鸡来孵蛋，这是怎么实现的呢？
5.  Binder的实现比较巧妙：预先创造一只鸡来孵蛋：ServiceManager和其它进程同样采用Binder通信，ServiceManager是Server端，有自己的Binder对象（实体），其它进程都是Client，需要通过这个Binder的引用来实现Binder的注册，查询和获取。
6.  ServiceManager提供的Binder比较特殊，它没有名字也不需要注册，当一个进程使用BINDER\_SET\_CONTEXT\_MGR\_EXT命令将自己注册成ServiceManager时Binder驱动会自动为它创建Binder实体（这就是那只预先造好的鸡）。
7.  其次这个Binder的引用在所有Client中都固定为0(handle=0)而无须通过其它手段获得。也就是说，一个Server若要向ServiceManager注册自己Binder就必须通过0这个引用号和ServiceManager的Binder通信。
8.  类比网络通信，0号引用就好比域名服务器的地址，你必须预先手工或动态配置好。要注意这里说的Client是相对ServiceManager而言的，一个应用程序可能是个提供服务的Server，但对ServiceManager来说它仍然是个Client。

![](https://pica.zhimg.com/v2-300ab691c09e9cb5c6d30b9caf07588c_1440w.jpg)

## 8.Binder的内存管理

ServiceManager启动后，会通过系统调用mmap向内核空间申请128K的内存，用户进程会通过mmap向内核申请(1M-8K)的内存空间。这里用户空间mmap (1M-8K)的空间，为什么要减去8K，而不是直接用1M?

Android的git commit记录

> Modify the binder to request 1M - 2 pages instead of 1M. The backing store in the kernel requires a guard page, so 1M allocations fragment memory very badly. Subtracting a couple of pages so that they fit in a power of two allows the kernel to make more efficient use of its virtual address space.

大致的意思是：kernel的“backing store”需要一个保护页，这使得1M用来分配碎片内存时变得很差，所以这里减去两页来提高效率，因为减去一页就变成了奇数。

系统定义：BINDER\_VM\_SIZE ((1 \* 1024 \* 1024) - sysconf(\_SC\_PAGE\_SIZE) \* 2) = （1M- sysconf(\_SC\_PAGE\_SIZE) \* 2） 这里的8K，其实就是两个PAGE的SIZE, 物理内存的划分是按PAGE(页)来划分的，一般情况下，一个Page的大小为4K。内核会增加一个guard page，再加上内核本身的guard page，正好是两个page的大小，减去后，就是用户空间可用的大小。

> 在计算机系统中，寻址空间是指CPU可以直接访问的内存地址范围。这个范围的大小与CPU的位数（即处理数据的位宽）直接相关。对于32位系统来说，其寻址空间由32个二进制位组成。**寻址空间大小**：由于每个地址由32个二进制位组成，因此可以表示的地址数量为2的32次方。计算得到：2^32 = 4,294,967,296

在内存分配这块，还要分为32位和64位，32位的系统很好区分，虚拟内存为4G，用户空间从低地址开始占用3G，内核空间占用剩余的1G。ARM32内存占用分配

![](https://pic4.zhimg.com/v2-97d06e53797dc9327a42d6c68516f7fb_1440w.jpg)

但随着现在的硬件发展越来越迅速，应用程序的运算也越来越复杂，占用空间越来越大，原有的4G虚拟内存已经不能满足用户的需求，因此，现在的Android基本都是用64位的内存机制。

理论上讲，64位的地址总线可以支持高达16EB（2^64）的内存。AMD64架构支持52位（4PB）的地址总线和48位（256TB）的虚拟地址空间。在linux arm64中，如果页的大小为4KB，使用3级页表转换或者4级页表转换，用户空间和内核空间都支持有39bit（512GB）或者48bit（256TB）大小的虚拟地址空间。

2^64 次方太大了，Linux 内核只采用了 64 bits 的一部分（开启 CONFIG\_ARM64\_64K\_PAGES 时使用 42 bits，页大小是 4K 时使用 39 bits），该文假设使用的页大小是 4K（VA\_BITS = 39）

ARM64 有足够的虚拟地址，用户空间和内核空间可以有各自的 2^39 = 512GB 的虚拟地址。

ARM64内存占用分配：

![](https://picx.zhimg.com/v2-896f76e4cec4ccffedd7ef31ffd102dd_1440w.jpg)

用户地址空间(服务端-数据接收端)和内核地址空间都映射到同一块物理地址空间。

Client(数据发送端)先从自己的用户进程空间把IPC数据通过copy\_from\_user()拷贝到内核空间。而Server端（数据接收端）与内核共享数据(mmap到同一块物理内存)，不再需要拷贝数据，而是通过内存地址空间的偏移量，即可获悉内存地址，整个过程只发生一次内存拷贝。

![](https://pic4.zhimg.com/v2-24fcc2ab4bbedea2f7ba8ab08cffb343_1440w.jpg)

## 9.Binder中各角色之间关系

下图展示了Binder中各个角色之间的关系：

![](https://pic3.zhimg.com/v2-5ce6a591b753ce5990e6f411d8b162da_1440w.jpg)

### 9.1 Binder实体

Binder实体，是各个Server以及ServiceManager在内核中的存在形式。

Binder实体实际上是内核中binder\_node结构体的对象，它的作用是在内核中保存Server和ServiceManager的信息(例如，Binder实体中保存了Server对象在用户空间的地址)。简言之，Binder实体是Server在Binder驱动中的存在形式，内核通过Binder实体可以找到用户空间的Server对象。

在上图中，Server和ServiceManager在Binder驱动中都对应的存在一个Binder实体。

### 9.2 Binder引用\\代理

说到Binder实体，就不得不说"Binder引用"。所谓Binder引用，实际上是内核中binder\_ref结构体的对象，它的作用是在表示"Binder实体"的引用。换句话说，每一个Binder引用都是某一个Binder实体的引用，通过Binder引用可以在内核中找到它对应的Binder实体。如果将Server看作是Binder实体的话，那么Client就好比Binder引用。Client要和Server通信，它就是通过保存一个Server对象的Binder引用，再通过该Binder引用在内核中找到对应的Binder实体，进而找到Server对象，然后将通信内容发送给Server对象。

Binder实体和Binder引用都是内核(Binder驱动)中的数据结构。每一个Server在内核中就表现为一个Binder实体，而每一个Client则表现为一个Binder引用。这样，每个Binder引用都对应一个Binder实体，而每个Binder实体则可以多个Binder引用。

### 9.3 远程服务

Server都是以服务的形式注册到ServiceManager中进行管理的。如果将Server本身看作是"本地服务"的话，那么Client中的"远程服务"就是本地服务的代理。远程服务就是本地服务的一个代理，通过该远程服务Client就能和Server进行通信。

### 9.4 ServiceManager守护进程

ServiceManager是用户空间的一个守护进程。当该应用程序启动时，它会和Binder驱动进行通信，告诉Binder驱动它是服务管理者；对Binder驱动而言，它则会新建ServiceManager对应的Binder实体，并将该Binder实体设为全局变量。

## 10.源码路径：

-   1)binder驱动

/kernel/msm-4.9/drivers/android/\*

-   2)servicemanager

/frameworks/native/cmds/servicemanager/\*

-   3)libbinder

/frameworks/native/libs/binder/\*

-   4)JAVA层

/frameworks/base/core/java/android/os/\*

## 11\. 源码下载：

[https://github.com/LineageOS](https://link.zhihu.com/?target=https%3A//github.com/LineageOS)

## **12\. 参考：**

[《Binder系列—开篇》](https://link.zhihu.com/?target=http%3A//gityuan.com/2015/10/31/binder-prepare/)

[《Binder（初始篇）》](https://link.zhihu.com/?target=https%3A//my.oschina.net/youranhongcha/blog/149575)

[《Binder学习指南》](https://link.zhihu.com/?target=http%3A//weishu.me/2016/01/12/binder-index-for-newer/)

[《Android Binder 进程间通讯机制》](https://link.zhihu.com/?target=https%3A//blog.csdn.net/freekiteyu/article/details/70082302)

[《Binder的设计和框架》](https://link.zhihu.com/?target=http%3A//wangkuiwu.github.io/2014/09/01/Binder-Introduce/)

[《Android Bander设计与实现 - 设计篇》](https://link.zhihu.com/?target=https%3A//blog.csdn.net/universus/article/details/6211589)

[《Android中的代理模式》](https://link.zhihu.com/?target=https%3A//www.cnblogs.com/Free-Thinker/p/10401464.html)

[《arm64 内存布局整理》](https://link.zhihu.com/?target=https%3A//blog.csdn.net/qq_30025621/article/details/90207894)

[《linux 64 位CPU内存限制》](https://link.zhihu.com/?target=https%3A//blog.csdn.net/cd520yy/article/details/10821765)

[《Linux用户空间与内核空间（理解高端内存）》](https://link.zhihu.com/?target=https%3A//www.cnblogs.com/wuchanming/p/4360277.html)

## 13\. 原文链接：[https://blog.csdn.net/yiranfeng/article/details/105181297](https://link.zhihu.com/?target=https%3A//blog.csdn.net/yiranfeng/article/details/105181297)
