本文的主要内容如下：

-   1 Android整体架构
-   2 IPC原理
-   3 Binder简介
-   4 Binder通信机制
-   5 Binder协议

### 一、Android整体架构

##### (一) Android的整体架构

为了让大家更好的理解Binder机制，我们先来看下Android的整体架构。因为这样大家就知道在Android架构中Binder出于什么地位。 用一下官网上的图片

![](https://ask.qcloudimg.com/http-save/yehe-2957818/890v05w6hw.png)

Android架构.png

从下往上依次为：

-   内核层：Linux内核和各类硬件设备的驱动，这里需要注意的是，Binder IPC驱动也是这一层的实现，比较特殊。
-   硬件抽象层：封装"内核层"硬件驱动，提供可供"系统服务层"调用的统一硬件接口
-   系统服务层：提供核心服务，并且提供可供"应用程序框架层"调用的接口
-   Binder IPC 层：作为"系统服务层"与"应用程序框架层"的IPC桥梁，相互传递接口调用的数据，实现跨进程的通信。
-   应用程序框架层：这一层可以理解为Android SDK，提供四大组件，View绘制等平时开发中用到的基础部件

##### (二) Android的架构解析

> 在一个大的项目里面，分层是非常重要的，处于最底层的接口最具有"通用性"，接口颗粒度最细，越往上层通用性降低。理论上来说上面每一层都可以"开放"给开发者调用，例如开发者可以直接调用硬件抽象层的接口去操作硬件，或者直接调用系统服务层中的接口去直接操作系统服务，甚至像Windows开发一样，开发者可以在内核层写程序，运行在内核中。不过开放带来的问题就是开发者权利太大，对于系统的稳定性是没有任何好处的，一个病毒制作者搞一个内核层的病毒出来，系统可能就永远起不来。所以谷歌的做法是将开发者的权利收拢到"应用程序框架层"，开发者只能调用这一层的接口。

在上面的层次中，内核层与硬件抽象层均用C/C++实现，系统服务层是以Java实现，硬件抽象层编译为so文件，以JNI的形式供系统服务层使用。系统服务层中的服务随着系统启动而启动，只要不关机，就会一直运行。这些服务干什么事情？其实很简单，就是完成一个手机有的核心功能如[短信](https://cloud.tencent.com/product/sms?from_column=20065&from=20065)的收发管、电话的接听、挂断以及应用程序的包管理、Activity的管理等等。每一个服务均运行在一个独立的进程中，因此是以Java实现，所以本质上来说是运行在一个独立进程的Dalvik虚拟机中。那么问题来了，开发者的App也运行在一个独立的进程空间中，如果调用到系统的服务层中的接口？答案是IPC(Inter-Process Communication)，进程间通讯是和RPC(Remote Procedure Call)不一样的，实现原理也不一样。每一个系统服务在应用框架层都有一个Manager与之对应，方便开发者调用其相关功能，具体关系如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/62j7cwmvx6.png)

调用关系.png

##### (三)、总结一下：

-   Android 从下而上分了内核层、硬件抽象层、系统服务层、Binder IPC 层、应用程序框架层
-   Android 中"应用程序框架层"以 SDK 的形式开放给开发者使用，"系统服务层" 中的核心服务随系统启动而运行，通过应用层序框架层提供的 Manager 实时为应用程序提供服务调用。系统服务层中每一个服务运行在自己独立的进程空间中，应用程序框架层中的 Manager 通过 Binder IPC 的方式调用系统服务层中的服务。

### 二、IPC原理

从进程的角度来看IPC机制

![](https://ask.qcloudimg.com/http-save/yehe-2957818/okl004eswj.png)

image.png

每个Android进程，只能运行在自己的进程所拥有的虚拟地址空间，如果是32位的系统，对应一个4GB的虚拟地址空间，其中3GB是用户空，1GB是内核空间，而内核空间的大小是可以通过参数配置的。对于用户空间，不同进程之间彼此是不能共享的，而内核空间确实可以共享的。Client进程与Server进程通信，恰恰是利用进程间可共享的内核内空间来完成底层通信工作的，Client端与Server端进程往往采用ioctl等方法跟内核空间的驱动进行。

### 三、Binder综述

##### (一) Binder简介

##### 1、Binder的由来

> 简单的说，Binder是Android平台上的一种跨进程通信技术。该技术最早并不是谷歌公司提出的，它前身是Be Inc公司开发的OpenBinder，而且在Palm中也有应用。后来OpenBinder的作者Dianne Hackborn加入了谷歌公司，并负责Android平台开发的工作，所以把这项技术也带进了Android。

我们知道，在Android的应用层次上，基本上已经没有了进程的概念了，然后在具体的实现层次上，它毕竟还是要构建一个个进程之上的。实际上，在Android内部，哪些支持应用的组件往往会身处不同的继承，那么应用的底层必然会涉及大批的跨进程通信，为了保证了通信的高效性，Android提供了Binder机制。

##### 2、什么是Binder

让我们从四个维度来看Binder，这样会让大家对理解Binder机制更有帮助

-   1 从来类的角度来说，Binder就是Android的一个类，它继承了IBinder接口
-   2 从IPC的角度来说，Binder是Android中的一个中的一种跨进程通信方式，Binder还可以理解为一种虚拟的物理设备，它的设备驱动是/dev/binder，该通信方式在Linux中没有(由于耦合性太强，而Linux没有接纳)
-   3 从Android Framework角度来说，Binder是ServiceManager连接各种Manager(ActivityManager、WindowManager等)和相应的ManagerService的桥梁
-   4 从Android应用层的角度来说，Binder是客户端和服务端进行通信的媒介，当你bindService的时候，服务端会返回一个包含了服务端业务调用的Binder对象，通过这个Binder对象，客户端就可以获取服务端提供的服务或者数据，这里的服务包括普通服务和基于AIDL的服务。

##### 3、Binder机制的意义

Binder机制具有两层含义：

-   1 是一种跨进程通信的方式(IPC)
-   2 是一种远程过程调用方式(PRC)

而从实现的角度来说，Binder核心被实现成一个Linux驱动程序，并运行于内核态。这样它才能具有强大的跨进程访问能力。

##### 4、和传统IPC机制相比，谷歌为什么采用Binder

我们先看下Linux中的IPC通信机制:

-   1、传统IPC：匿名管道(PIPE)、信号(signal)、有名管道(FIFO)
-   2、AT&T Unix：共享内存，信号量，[消息队列](https://cloud.tencent.com/product/message-queue-catalog?from_column=20065&from=20065)
-   3、BSD Unix：Socket

关于这块如果大家不了解，请看前面的文章。

虽然Android继承Linux内核，但是Linux与Android通信机制是不同的。Android中有大量的C/S(Client/Server)应用方式，这就要求Android内部提供IPC方法，而如果采用Linux所支持的进程通信方式有两个问题：性能和安全性。那

-   性能：目前Linux支持的IPC包括传统的管道，System V IPC(包括消息队列/共享内存/信号量)以及socket，但是只有socket支持Client/Server的通信方式，由于socket是一套通用当初网络通信方式，其效率低下，且消耗比较大(socket建立连接过程和中断连接过程都有一定的开销)，明显在手机上不适合大面积使用socket。而消息队列和管道采用"存储-转发" 方式，即数据先从发送方缓存区拷贝到内核开辟的缓存区中，然后再从内核缓存中拷贝到接收方缓存中，至少有两次拷贝过程。共享内存虽然无需拷贝，但控制复杂，难以使用。
-   安全性：在安全性方面，Android作为一个开放式，拥有众多开发者的平台，应用程序的来源广泛，确保智能终端的安全是非常重要的。终端用户不希望从网上下载的程序在不知情的情况下偷窥隐私数据，连接无线网络，长期操作底层设备导致电池很快耗尽的情况。传统IPC没有任何安全措施，完全依赖上层协议来去报。首先传统IPC的接受方无法获取对方进程可靠的UID/PID(用户ID/进程ID)，从而无法鉴别对方身份。Android为每个安装好的应用程序分配了自己的UID，故进程的UID是鉴别进程的身份的重要标志。使用传统IPC只能由用户在数据包里填入UID/PID，但这样不可靠，容易被恶意程序利用。可靠的身份标记只由IPC机制本身在内核中添加。其次传统IPC访问接入点是开放的，无法建立私有通道。比如命名管道、system V的键值，socket的ip地址或者文件名都是开放的，只要知道这些接入点的程序都可以对端建立连接，不管怎样都无法阻止恶意程序通过接收方地址获得连接。

基于以上原因，Android需要建立一套新的IPC机制来满足系统对通信方式，传输性能和安全性的要求，所以就有了Binder。Binder基于Client/Server通信模式，传输过程只需要一次拷贝，为发送发添加UID/PID身份，鸡翅实名Binder也支持匿名Binder，安全性高。下图为Binder通信过程示例：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/bck9b5c86d.png)

Binder通信过程.png

-   相比于传统的跨进程通信手段，通信双方必须要处理线程同步，内存管理等问题，工作量大，而且问题多，就像我们前面介绍的传统IPC 命名管道(FIFO) 信号量(semaphore) 消息队列已经从Android中去掉了，同其他IPC相比，Socket是一种比较成熟的通信手段了，同步控制也很容易实现。Socket用于网络通信非常合适，但是用于进程间通信就效率很低。
-   Android在架构上一直希望模糊进程的概念，取而代之以组件的概念。应用也不需要关心组件存放的位置、组件运行在那个进程中、组件的生命周期等问题。随时随地的，只要拥有Binder对象，就能使用组件的功能。Binder就像一张网，将整个系统的组件，跨进程和线程的组织在一起。

Binder是整个系统的运行的中枢。Android在进程间传递数据使用共享内存的方式，这样数据只需要复制一次就能从一个进程到达另一个进城了(前面文章说了，一般IPC都需要两步，第一步用户进程复制到内核，第二步再从内核复制到服务进程。)

> PS: 整个Androdi系统架构中，虽然大量采用了Binder机制作为IPC(进程间通信)方式，但是也存在部分其他的IPC方式，比如Zygote通信就是采用socket。

##### 5、Binder在Service服务中的作用

在Android中，有很多Service都是通过Binder来通信的，比如MediaService名下的众多Service：

-   AudioFlinger音频核心服务
-   AudioPolicyService：音频策略相关的重要服务
-   MediaPlayerService:多媒体系统中的重要服务
-   CarmeraService:有关摄像/照相的重要服务

那具体是怎么应用或者通信机制是什么那？那就让我们来详细了解下

##### (二)、总结

> Android Binder 是在OpenBinder上定制实现的。原先的OpenBinder 框架现在已经不再继续开发，所以也可以说Android让原先的OpenBinder得到了重生。Binder是Android上大量使用的IPC(Inter-process communication，进程间通讯)机制。无论是应用程序对系统服务的请求，还是应用程序自身提供对外服务，都需要使用Binder。

##### 整体架构

![](https://ask.qcloudimg.com/http-save/yehe-2957818/xb4cymlndv.png)

整体架构.png

从图中可以看出，Binder的实现分为这几层，按照大的框架理解是

-   framewor层
    -   java 层
    -   jni 层
    -   native/ C++层
-   linux驱动层 c语言

让我们来仔细研究下。

-   其中Linux驱动层位于Linux内核中，它提供了最底层的数据传递，对象标示，线程管理，通过调用过程控制等功能。驱动层其实是Binder机制的核心。
-   Framework层以Linux驱动层为基础，提供了应用开发的基础设施。Framework层既包含了C++部分的实现，也包含了Java基础部分的实现。为了能将C++ 的实现复用到Java端，中间通过JNI进行衔接。

开发者可以在Framework之上利用Binder提供的机制来进行具体的业务逻辑开发。其实不仅仅是第三方开发者，Android系统中本身也包含很多系统服务都是基于Binder框架开发的。其中Binder框架是典型的C/S架构。所以在后面中， 我们把服务的请求方称为Client，服务的实现方称之Server。Clinet对于Server的请求会经由Binder驱动框架由上至下传递到内核的Binder驱动中，请求中包含了Client将要调用的命令和参数。请求到了Binder驱动以后，在确定了服务的提供方之后，再讲从下至上将请求传递给具体的服务。如下图所示：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/uplzb0z6ek.png)

Binder调用.png

如果大家对网络协议有所了解的话，其实会发现整个数据的传递过程和网络协议如此的相似。

### 四、Binder通信机制

Android内部采用C/S架构。而Binder通信也是采用C/S架构。那我们来看下Binder在C/S的中的流程。

##### (一) Binder在C/S中的流程

如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/436asmgqvx.png)

Binder流程.png

具体流程如下：

-   1、相应的Service需要注册服务。Service作为很多Service的拥有者，当它想向Client提供服务时，得先去Service Manager(以后缩写成SM)那儿注册自己的服务。Server可以向SM注册一个或者多个服务。
-   2、Client申请服务。Client作为Service的使用者，当他想使用服务时，得向SM申请自己所需要的服务。Client可以申请一个或者多个服务。
-   3、当Client申请服务成功后，Client就可以使用服务了。

SM一方面管理Server所提供的服务，同时又响应Client的请求并为之分配响应的服务。扮演角色相当于月老，两边前线。这种通信方式的好处是：一方面，service和Client请求便于管理，另一方面在应用程序开发时，只需要为Client建立到Server的连接即可，这样只需要花很少的时间成本去实现Server的相应功能。那么Binder与这个通信有什么关系？其实三者的通信方式就是Binder机制(比如Server向SM注册服务，使用Binder通信；Client申请请求也是Binder通信。)

> PS:注意这里的ServiceManager是指Nativie层的ServiceManager(C++)，并非是framework层的ServiceManager(Java)。ServiceManager是整个Binder通信机制的大管家，是Android进程间通信机制的守护进程。

##### (二)Binder通信整体框架

这里先提前和大家说下，后面我们会不断的提及两个概念，一个是Server，还有一个是Service，我这里先强调下，Server是Server，Service是Service，大家不要混淆，一个Server下面可能有很多Service，但是一个Servcie也只能隶属于一个Server。下面我们将从三个角度来看Binder框架，这样

###### 1、从内核和用户空间的角度来看

Binder通信模型如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/qa7hsvdo7f.png)

Binder通信整体框架.png

我们可以发现：

-   1、Client和Server是存在于用户空间
-   2、Client和Server通信实现是由Binder驱动在内核的实现
-   3、SM作为守护进程，处理客户端请求，管理所有服务

如果大家不好理解上面的意思，我们可以把SM理解成为DNS[服务器](https://cloud.tencent.com/product/cvm/?from_column=20065&from=20065)，那么Binder Driver就相当于路由的功能。这里就涉及到Client和Server是如何通信的问题。

###### 2、从Android的层级的角度

如下图：(注意图片的右边)

![](https://ask.qcloudimg.com/http-save/yehe-2957818/si4rp6p4cx.png)

Binder原理.png

图中Client/Server/ServiceManager之间的相互通信都是基于Binder机制。图中Clinet/Server/ServiceManager之间交互都是虚线表示，是由于他们彼此之间不直接交互，都是通过Binder驱动进行交互，从而实现IPC通信方式。其中Binder驱动位于内核空间，Client/Server/ServiceManager可以看做是Android平台的基础架构。而Client和Server是Android的应用层，开发人员只需要自定义实现client、Server端，借助Android的基本平台架构就可以直接进行IPC通信。

###### 3、从Binder的架构角度来看

如下图:

![](https://ask.qcloudimg.com/http-save/yehe-2957818/5n49gtaole.png)

Binder架构.png

同样

> Binder IPC 属于 C/S 结构，Client 部分是用户代码，用户代码最终会调用 Binder Driver 的 transact 接口，Binder Driver 会调用 Server，这里的 Server 与 service 不同，可以理解为 Service 中 onBind 返回的 Binder 对象，请注意区分下。

Client端：用户需要实现的代码，如 AIDL 自动生成的接口类 Binder Driver：在内核层实现的 Driver Server端：这个 Server 就是 Service 中 onBind 返回的 IBinder 对象 需要注意的是，上面绿色的色块部分都是属于用户需要实现的部分，而蓝色部分是系统去实现了。也就是说 Binder Driver 这块并不需要知道，Server 中会开启一个线程池去处理客户端调用。为什么要用线程池而不是一个单线程队列呢？试想一下，如果用单线程队列，则会有任务积压，多个客户端同时调用一个服务的时候就会有来不及响应的情况发生，这是绝对不允许的。

对于调用 Binder Driver 中的 transact 接口，客户端可以手动调用，也可以通过 AIDL 的方式生成的代理类来调用，服务端可以继承 Binder 对象，也可以继承 AIDL 生成的接口类的 Stub 对象。这些细节下面继续接着说，这里暂时不展开。

> 切记，这里 Server 的实现是线程池的方式，而不是单线程队列的方式，区别在于，单线程队列的话，Server 的代码是线程安全的，线程池的话，Server 的代码则不是线程安全的，需要开发者自己做好多线程同步。

##### (三)、Binder流程

###### 1、Server向SM注册服务

![](https://ask.qcloudimg.com/http-save/yehe-2957818/069r66mzi1.png)

注册.png

-   1、首先 XXServer(XXX代表某个)在自己的进程中向Binder驱动申请创建一个XXXService的Binder实体。
-   2、Binder驱动为这个XXXService创建位于内核中的Binder实体节点以及Binder的引用，注意，是将名字和新建的引用打包传递给SM(实体没有传给SM)，通知SM注册一个名叫XXX的Service。
-   3、SM收到数据包后，从中取出XXXService名字和引用，填入一张查找表中
-   4、此时，如果有Client向SM发送申请服务XXXService的请求，那么SM就可以查找表中该Service的Binder引用，并把BInder引用(XXXBpBinder返回给Client)

在进一步了解Binder通信机制之前，我们先弄清楚几个概念。

-   引用和实体。这里，对于一个用于通信的实体(可以理解为真实空间的Object)，可以额有多个该实体的引用(没有真实空间，可以理解成实体的一个链接，操作引用就可以操作对应链接上的实体)。如果一个进程持有某个实体，其他进程也想操作该实体，最高效的做法是去获取该实体的引用，再去操作这个引用。
-   有些资源也把实体成本本地对象，应用称为远程对象。所以也可以这么理解：应用是从本地进程发送给其他进程操作实体之用，所以有本地和远程对象之名。

为了大家在后面更好的理解，这里补充几个概念

-   **Binder实体对** :Binder实体对象就是Binder实体对象就是Binder服务的提供者。一个提供Binder服务的类必须继承BBinder类，因此，有时为了强调对象类型，也用"BBinder对象"来代替"Binder实体对象"。
-   **Binder引用对象** :Binder引用对象是Binder实体对象在客户进程的代表，每个引用对象的类型都是BpBiner类，同样可以用名称"BpBinder对象"来代替"Binder引用对象"。
-   **Binder代理对象** ：代理对象也成为接口对象，它主要是为了客户端的上层应用提供接口服务，从IInterface类派生。它实现了Binder服务的函数接口，当然只是一个转调的空壳。通过代理对象，应用能像使用本地对象一样使用远端实体对象提供服务。
-   **IBiner对象** ：BBinder和BpBinder类是从IBinder类中继承来。在很多场合，不需要刻意地去区分实体对象和引用对象，这时候也可以统一使用"IBinder对象"来统一称呼他们。
-   **Binder代理对象** 主要是和应用程序打交道，将Binder代理对象和Binder引用对应(BpBinder对象)分开的好处是代理对象可以有很多实例，但是它们包含的是同一个引用对象，这样方便了应用层的使用。如下图所示：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/8ssnrz3tc5.png)

Binder对象关系图.png

这样应用层可以直接抛开接口对象直接使用Binder的引用对象，但是这样开发的程序兼容性不好。也正是客户端将引用对象和代理对象分离，Android才能用一套架构来同时为Java和native层提供Binder服务。隔离后，Binder底层不需要关系上层的实现细节，只需要和Binder实体对象和引用对象进行交互。

> PS:BpBinder(Binder引用对象,在客户端)和BBinder(Binder实体,在服务端)都是Android中Binder通信相关的代表，它们都是从IBiner类中派生而来(BpBinder和BBinder在Native层，不在framework层)，关系图如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/bxlx5bwrnh.png)

image.png

client端：BpBinder通过调用transact()来发送事物请求 server端：BBinder通过onTransact()会接受到相应的事物

这时候再来看下这个图，然后大家思考一下，就会明白很多事情。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/9yevtsquzd.png)

Binder原理.png

###### 2、如何获得一个SM的远程接口

![](https://ask.qcloudimg.com/http-save/yehe-2957818/0n6e8m3hun.png)

获取.png

如果你足够细心，你会发现这里有一个问题:

> SM和Server都是进程，Server向SM注册Binder需要进程间通信，当前实现的是进程间通信，却又用到进程间通信。有点晕是不，就好像先有鸡还是先有蛋这个问题。

其实Binder是这么解决这个问题的：

-   针对Binder的通信机制，Server端拥有的是Binder的实体(BBinder)；Client拥有的是Binder的引用(BpBinder)。
-   如果把SM看做Server端，让它在Binder驱动一起运行起来时就有自己的实体(BBinder)(代码中设置ServiceManager的Binder其handle的值恒为0)。这个Binder实体没有名字也不需要注册，所有的Client都认为handle值为0的binder引用(BpBinder)是用来与SM通信的。那么这个问题就解决了。
-   但是问题又来了，Client和Server中handle的值为0(值为0的引用是专门与SM通信用的)，还不行，还需要让SM的handle值为0的实体(BBinder)为0才算大功告成。怎么实现的? 当一个进程调用Binder驱动时，使用\*\* "BINDER\_SET\_CONTEXXT\_MGR" \*\* 命名(在binder\_ioctl中)将自己注册成SM时，Binder驱动会自动为她创建Binder实体。这个Binder的引用对所有Client都为0。

###### 3、Client从SM中获得Service的远程接口

> Server向SM注册了Binder实体及其名字后，Client就可以Service的名字在SM在查找表中获得了该Binder的引用(BpBinder)了。Client也利用了保留的handle值为0的引用向SM请求访问某个Service:当申请访问XXXService的引用。SM就会从请求数据包中获得XXXService的名字，在查找表中找到名字对应的条目，取出Binder的引用打包回复给Client。然后，Client就可以利用XXXService的引用使用XXXService的服务了。如果有更多的Client请求该Service，系统中就会有更多的Client获得这个引用。

如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/5whwzqb9xg.png)

获取远程接口.png

###### 4、建立C/S连接后

首先要明白一个事情：

> Client要拥有自己的自己的Binder实体，以及Server的Binder的应用；Server有用自己的Binder的实体，以及Client的Binder引用。

我们也可以按照网络请求的方式来分析：

-   从Client向Server发送数据：Client为发送方，拥有Binder实体；Server为接收方，拥有Binder引用。
-   从Server向Client发送数据：Server为发送方，拥有Binder实体：Client为接收方，拥有Binder引用。

其实，在我们建立C/S连接后，无需考虑谁是Client，谁是Server。只要理清谁是发送方，谁是接收方，就能知道Binder的实体和应用在那边。

那我们看下建立C/S连接后的，具体流程,如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/42jeltckw0.png)

建立C/S连接后的流程.png

那我们说下具体的流程:

-   第一步，发送方通过Binder实体请求发送操作
-   第二步，Binder驱动会处理这个操作请求，把发送方的数据放入写缓存(binder\_write\_read.write\_buffer)(对于接受方来说为读缓存区)，并把read\_size(接收方读数据)置为数据大小。
-   第三步，接收方之前一直在阻塞状态中，当写缓存又数据，则会读取数据，执行命令操作
-   第四步，接收方执行完后，会把返回结果同样采用binder\_transaction\_data结构体封装，写入缓冲区(对于发送方，为读缓冲区)

##### (四) 匿名Binder

在Android中Binder还可以建立点对点的私有通道，匿名Binder就是这种方式。在Binder通信中，并不是所有通信的Binder实体都需要注册给SM的，Server可以通过已建立的实体Binder连接将创建的Binder实体传给Client。而这个Binder没有向SM注册名字。这样Server和Client通信就有很高的隐私性和安全性。

如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/f663bpw0gx.png)

匿名Binder.png

### 五、 Binder的层次

从代码上看，Binder设计的类可以分成4个层级，如下图所示

![](https://ask.qcloudimg.com/http-save/yehe-2957818/oblejw6g0q.png)

Binder的层次图.png

-   最上层的是位于Framewok中的各种Binder服务类和它们的接口类。这一层的类非常多，比如常见的ActivityManagerService(缩写叫AMS)、WindowManagerService(缩写叫WMS)、PackageManagerService(缩写是PMS)等，它们为应用程序提供了各种各样的服务。
-   中间则分为为两层，上面是用于服务类和接口开发的基础，比如IBinder、BBinder、BpBinder等。下层是和驱动交互的IPCThreadState和ProcessState类。
-   这里刻意把中间的libbinder中的类划分为两个层次的原因，是在这4层中，第一层的和第二层联系很紧密，第二层中的 各种Binder类用来支撑服务类和代理类的开发。但是第三层的IPCThread和第四层之间耦合得很厉害，单独理解IPCThread或者是驱动都是一件很难的事，必须把它们结合起来理解，这一点正是Binder架构被人诟病的地方，驱动和应用层之间过于耦合，违反了Linux驱动设计的原则，因此，主流的Linux并不愿意接纳Binder。

下面我们就来详细的看来Binder

### 六、Binder协议

Biner协议格式基本是"命令+数据"，使用ioctl(fd,cmd,arg)函数实现交互。命令由参数cmd承载，数据由参数arg，随着cmd不同而不动。下表列了所有命令及其对应的数据：

| 命令                     | 含义                                                                                                                                                                                  | 参数(arg)                                                                                                                                                                                   |
| ---------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| BINDER_WRITE_READ      | 该命令向Binder写入或读取数据。参数分为两段：写部分和读部分。如果write_size不为0，就将write_buffer里的数据写入Binder；如果read_ size不为0再从Binder中读取数据存入read_buffer中。write_consumered和read_consumered表示操作完成时Binder驱动实际写入或者读出数据的个数 | struct binder_write_read{ singed long write_size;singed long write_consumed; unsigned long write_buffer; signed long read_size; signed long read_consumed; unsigned long read_buffer; } ; |
| BINDER_SET_MAX_THREADS | 该命令告知Binder驱动接收方(通常是Server端)线程池中最大的线程数。由于Client是并发向Server端发送请求的，Server端必须开辟线程池为这些并发请求提供服务 。告知驱动线程池的最大值是为了让驱动发现线程数达到该线程池的最大值是为了让驱动发现线程数达到该值时，不要再命令接收端启动先的线程。                         | int max_threads;                                                                                                                                                                          |
| BINDER_SET_CONTEXT_MGR | 当前进程注册为SM。系统中只能存在一个SM，只要当前的SM没有调用close()关闭，Binder驱动就不能有别的进程变成SM                                                                                                                     | 无                                                                                                                                                                                         |
| BINDER_TREAD_EXIT      | 通知Binder驱动当前线程退出了。Binder会为所有参与的通信线程(包括Server线程池中的线程和Client发出的请求的线程) 建立相应的数据结构。这些线程在退出时必须通知驱动释放相应的数据结构                                                                               | 无                                                                                                                                                                                         |
| BINDER_VERSION         | 获取Binder驱动的版本号                                                                                                                                                                      | 无                                                                                                                                                                                         |

这其中最常用的命令是 BINDER\_WRITE\_READ。该命令的参数包括两个部分:

-   1、是向Binder写入数据
-   2、是向Binder读出数据

驱动程序先处理写部分再处理读部分。这样安排的好处是应用程序可以很灵活的地处理命令的同步或者异步。例如若要发送异步命令可以只填入写部分而将read\_size设置为0，若要只从Binder获得的数据可以将写部分置空，即write\_size置0。如果想要发送请求并同步等待返回数据可以将两部分都置上。

##### (一)、BINDER\_WRITE\_READ 之写操作

Binder写操作的数据时格式同样也是(命令+数据)。这时候命令和数据都存放在binder\_write\_read结构中的write\_buffer域指向的内存空间里，多条命令可以连续存放。数据紧接着存放在命令后面，格式根据命令不同而不同。下表列举了Binder写操作支持的命令： 我提供两套，一套是图片，方便手机用户，一部分是文字，方便PC用户

![](https://ask.qcloudimg.com/http-save/yehe-2957818/0zet6d69ox.png)

写操作.png

上面图片，下面是文字

|命令|含义|参数(arg)|
|---|---|---|
|BC_TRANSACTION BC_REPLY|BC_TRANSACTION用于Client向Server发送请求数据；BC_REPLY用于Server向Client发送回复（应答）数据。其后面紧接着一个binder_transaction_data结构体表明要写入的数据。|struct binder_transaction_data|
|BC_ACQUIRE_RESULT|暂未实现||
|BC_FREE_BUFFER|释放一块映射内存。Binder接受方通过mmap()映射一块较大的内存空间，Binder驱动基于这片内存采用最佳匹配算法实现接受数据缓存的动态分配和释放，满足并发请求对接受缓存区的需求。应用程序处理完这篇数据后必须尽快使用费改命令释放缓存区，否则会因为缓存区耗尽而无法接受新数据|指向需要释放的缓存区的指针；该指针位于收到的Binder数据包中|
|BC_INCREFS BC_ACQUIRE BC_RELEASE BC_DECREFS|这组命令增加或减少Binder的引用计数，用以实现强指针或弱指针的功能|32位Binder引用号|
|BC_REGISTER_LOOPER BC_ENTER_LOOPER BC_EXIT_LOOPER|这组命令同BINDER_SET_MAX_THREADS 一并实现Binder驱动对接收方线程池的管理。BC_REGISTER_LOOPER通知驱动线程池中的一个线程已经创建了；BC_ENTER_LOOPER通知该驱动线程已经进入主循环，可以接受数据；BC_EXIT_LOOPER通知驱动该线程退出主循环，不在接受数据。|-----|
|BC_REQUEST_DEATH_NOTIFICATION|获得Binder引用的进程通过该命令要求驱动在Binder实体销毁得到通知。虽说强指针可以确保只要有引用就不会销毁实体，但这毕竟是个跨进程的引用，谁也无法保证实体由于所在的Server关闭Binder驱动或异常退出而消失，引用者能做的就是要求Server在此刻给出通知|uint32 *ptr;需要得到死亡的通知Binder引用|void **cookie：与死亡通知相关的信息，驱动会在发出死亡通知时返回给发出请求的进程。|
|BC_DEAD_BINDER|收到实体死亡通知书的进程在删除引用后用本命令告知驱动|void * cookie|

> 在这些命令中，最常用的h是BC\_TRANSACTION/BC\_REPLY命令对，Binder请求和应答数据就是通过这对命令发送给接受方。这对命令所承载的数据包由结构体struct binder\_transaction\_data定义。Binder交互有同步和异步之分。利用binder\_transcation\_data中的flag区域划分。如果flag区域的TF\_ONE\_WAY位为1，则为异步交互，即client发送完请求交互即结束，Server端不再返回BC\_REPLY数据包；否则Server会返回BC\_REPLY数据包，Client端必须等待接受完数据包后才能完成一次交互。

##### (二)、BINDER\_WRITE\_READ:从Binder读出数据

在Binder里读出数据格式和向Binder中写入数据格式一样，采用(消息ID+数据)形式，并且多条消息可以连续存放。下面列举从Binder读出命令及相应的参数。 为了照顾手机端的朋友，先发图片

![](https://ask.qcloudimg.com/http-save/yehe-2957818/oof9939wtv.png)

Binder读出数据.png

Binder读操作消息ID

| 消息                                               | 含义                                                                                                                          | 参数(arg)                                                   |
| ------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------- |
| BR_ERROR                                         | 发生内部错误(如内存分配失败)                                                                                                             | ----                                                      |
| BR_OK BR_NOOP                                    | 操作完成                                                                                                                        | ----                                                      |
| BR_SPAWN_LOOPER                                  | 该消息用于接受方线程池管理。当驱动发现接收方所有线程都处于忙碌状态且线程池中的线程总数没有超过BINDER_SET_MAX_THREADS设置的最大线程时，向接收方发送该命令要求创建更多的线程以备接受数据。                     | -----                                                     |
| BR_TRANSCATION BR_REPLY                          | 这两条消息分别对应发送方的 BC_TRANSACTION 和BC_REPLY，表示当前接受的数据是请求还是回复                                                                     | binder_transaction_data                                   |
| BR_ACQUIRE_RESULT BR_ATTEMPT_ACQUIRE BR_FINISHED | 尚未实现                                                                                                                        | -----                                                     |
| BR_DEAD_REPLY                                    | 交互过程中如果发现对方进程或线程已经死亡则返回该消息                                                                                                  | -----                                                     |
| BR_TRANSACTION_COMPLETE                          | 发送方通过BC_TRRANSACTION或BC_REPLY发送完一个数据包后，都能收到该消息作为成功发送的反馈。这和BR_REPLY不一样，是驱动告知发送方已经发送成功，而不是Server端返回数据。所以不管同步还是异步交互接收方都能获得本消息。 | -----                                                     |
| BR_INCREFS BR_ACQUIRE BR_RFLEASE BR_DECREFS      | 这组消息用于管理强/弱指针的引用计数。只有提供Binder实体的进程才能收到这组消息                                                                                  | void *ptr : Binder实体在用户空间中的指针 void **cookie：与该实体相关的附加数据   |
| BR_DEAD_BINDER BR_CLEAR_DEATH_NOTIFICATION_DONE  | 向获得Binder引用的进程发送Binder实体死亡通知书：收到死亡通知书的进程接下来会返回 BC_DEAD_BINDER_DONE 确认                                                       | void *cookie 在使用BC_REQUEST_DEATH_NOTIFICATION注册死亡通知时的附加参数 |
| BR_FAILED_REPLY                                  | 如果发送非法引用号则返回该消息                                                                                                             | -----                                                     |

和写数据一样，其中最重要的消息是BR\_TRANSACTION或BR\_REPLY，表明收到一个格式为binder\_transaction\_data的请求数据包(BR\_TRANSACTION或返回数据包(BR\_REPLY))

##### (三)、struct binder\_transaction\_data ：收发数据包结构

该结构是Binder接收/发送数据包的标准格式，每个成员定义如下： 下图是Binder

![](https://ask.qcloudimg.com/http-save/yehe-2957818/pnlhc0t07j.png)

Binder数据包.png

|成员|含义|
|---|---|
|union{ size_t handle; void *ptr;} target；|对于发送数据包的一方，该成员指明发送目的地。由于目的地是远端，所以在这里填入的是对Binder实体的引用，存放在target.handle中。如前述，Binder的引用在代码中也叫句柄(handle)。 当数据包到达接收方时，驱动已将该成员修改成Binder实体，即指向 Binder对象内存的指针，使用target.ptr来获取。该指针是接受方在将Binder实体传输给其他进程时提交给驱动的，驱动程序能够自动将发送方填入的引用转换成接收方的Binder对象的指针，故接收方可以直接将其当对象指针来使用(通常是将其reinpterpret_cast相应类)|
|void *cookie；|发送方忽略该成员；接收方收到数据包时，该成员存放的是创建Binder实体时由该接收方自定义的任意数值，做为与Binder指针相关的额外信息存放在驱动中。驱动基本上不关心该成员|
|unsigned int code ;|该成员存放收发双方约定的命令码，驱动完全不关心该成员的内容。通常是Server端的定义的公共接口函数的编号|
|unsigned int code;|与交互相关的标志位，其中最重要的是TF_ONE_WAY位。如果该位置上表明这次交互是异步的，Server端不会返回任何数据。驱动利用该位决定是否构建与返回有关的数据结构。另外一位TF_ACCEPT_FDS是处于安全考虑，如果发起请求的一方不希望在收到回复中接收文件的Binder可以将位置上。因为收到一个文件形式的Binder会自动为接收方打开一个文件，使用该位可以防止打开文件过多|
|pid_t send_pid uid_t sender_euid|该成员存放发送方的进程ID和用户ID，由驱动负责填入，接收方可以读取该成员获取发送方的身份。|
|size_t data_size|驱动一般情况下不关心data.buffer里存放了什么数据。但如果有Binder在其中传输则需要将其对应data.buffer的偏移位置指出来让驱动知道。有可能存在多个Binder同时在数据中传递，所以须用数组表示所有偏移位置。本成员表示该数组的大小。|
|union{ struct{ const void *buffer; const void * offset; } ptr; uint8_t buf[8];} data;|data.buffer存放要发送或接收到的数据；data.offsets指向Binder偏移位置数组，该数组可以位于data.buffer中，也可以在另外的内存空间中，并无限制。buf[8]是为了无论保证32位还是64位平台，成员data的大小都是8字节。|

> PS:这里有必要强调一下offsets\_size和data.offsets两个成员，这是Binder通信有别于其他IPC的地方。就像前面说说的，Binder采用面向对象的设计思想，一个Binder实体可以发送给其他进程从而建立许多跨进程的引用；另外这些引用也可以在进程之间传递，就像java将一个引用赋值给另外一个引用一样。为Binder在不同进程中创建引用必须有驱动参与，由驱动在内核创建并注册相关的数据结构后接收方才能使用该引用。而且这些引用可以是强类型的，需要驱动为其维护引用计数。然后这些跨进程传递的Binder混杂在应用程序发送的数据包里，数据格式由用户定义，如果不把他们一一标记出来告知驱动，驱动将无法从数据中将他们提取出来。于是就是使用数组data.offsets存放用户数据中每个Binder相对于data.buffer的偏移量，用offersets\_size表示这个数组的大小。驱动在发送数据包时会根据data.offsets和offset\_size将散落于data.buffer中的Binder找出来并一一为它们创建相关的数据结构。

##### 七、Binder的整体架构

![](https://ask.qcloudimg.com/http-save/yehe-2957818/2aca62dcmc.png)

Binder的整体架构.png

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2017.08.04 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除