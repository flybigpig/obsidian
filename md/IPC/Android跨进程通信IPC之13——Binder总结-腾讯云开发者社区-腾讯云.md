本篇文章的主要内容如下

-   1 Android为什么选用Binder作为最重要的IPC机制
-   2 Binder中相关的类简述
-   3 Binder机制概述
-   4 Binder通信概述
-   5 Binder协议
-   6 Binder架构

### 一 、 Android为什么选用Binder作为最重要的IPC机制

我们知道在Linux系统中，进程间的通信方式有socket，named pipe，message queue， signal，sharememory等。这几种通信方式的优缺点如下：

-   name pipe：任何进程都能通讯，但速度慢
-   message queue：容量受到系统限制，且要注意第一次读的时候，要考虑上一次没有读完数据的问题。
-   signal：不能传递复杂消息，只能用来同步
-   shared memory：能够容易控制容量，速度快，但要保持同步，比如写一个进程的时候，另一个进程要注意读写的问题，相当于线程中的线程安全。当然，共享内存同样可以作为线程通讯，不过没有这个必要，线程间本来就已经共享了同一个进程内的一块内存。
-   socket：本机进程之间可以利用socket通信，跨进程之间也可利用socket通信，通常RPC的实现最底层都是通过socket通信。socket通信是一种比较复杂的通信方式，通常客户端需要开启单独的监听线程来接受从服务端发过来的数据，客户端线程发送数据给服务端，如果需要等待服务端的响应，并通过监听线程接受数据，需要进行同步，是一件很麻烦的事情。socket通信速度也不快。

Android中属性服务的实现和vold服务的实现采用了socket，getprop和setprop等命令都是通过socket和init进程通信来获的属性或者设置属性，vdc命令和mount service也是通过socket和vold服务通信来操作外接设备，比如SD卡

Message queue允许任意进程共享[消息队列](https://cloud.tencent.com/product/message-queue-catalog?from_column=20065&from=20065)实现进程间通信，并由内核负责消息发送和接受之间的同步，从而使得用户在使用消息队列进行通信时不再需要考虑同步问题。这样使用方便，但是信息的复制需要额外消耗CPU时间，不适合信息量大或者操作频繁的场合。共享内存针对消息缓存的缺点改而利用内存缓冲区直接交换信息，无须复制，快递，信息量大是其优点。

共享内存块提供了在任意数量的进程之间进行高效的双向通信机制，每个使用者都可以读写数据，但是所有程序之间必须达成并遵守一定的协议，以防止诸如在读取信息之前覆盖内存空间等竞争状态的实现。不幸的是，Linux无法严格保证对内存块的独占访问，甚至是你通过使用IPC\_PRIVATE创建新的共享内存块的时候，也不能保证访问的的独占性。同时，多个使用共享内存块的进程之间必须协调使用同一个键值。

Android应用程序开发者开发应用程序时，对系统框架的进程和线程运行机制不必了解，只需要利用四大组件开发，Android应用开发时可以轻易调用别的软件提供的功能，甚至可以调用系统App，在Android的世界里，所有应用都是平等的，但实质上应用进程被隔离在不同的沙盒里。

> Android平台的进程之间需要频繁的通信，比如打开一个应用便需要在Home应用程序进程和运行在system\_server进程里的ActivityManagerService通信才能打开。正式由于Android平台的进程需要非常频繁的通信，故此对进程间通信机制要求比较高，速度要快，还要能进行复杂的数据的交换，应用开发时尽可能简单，并能提供同步调用。虽然共享内存的效率高，但是它需要复杂的同步机制，使用时很麻烦，故此不能采用。Binder能满足这些要求，所以Android选择了Binder作为最核心的进程间通信方式。Binder主要提供一下功能：

-   1、用驱动程序来推进进程间的通信方式
-   2、通过共享内存来提高性能
-   3、为进程请求分配的每个进程的线程池，每个进程默认启动的两个Binder服务线程
-   4、针对系统中的对象引入技术和跨进程对象的引用映射
-   5、进程间同步调用。

### 二、Binder中相关的类简述

为了让大家更好的理解Binder机制，我这里把每个类都简单说下，设计到C层就是结构体。每个类/结构体都有一个基本的作用，还是按照之前的分类，如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/i3fe07rsc2.png)

Binder类分布.png

关于其中的关系，比如继承，实现如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/ad5gkkotsh.png)

类关系图.png

##### (一)、Java层相关类

###### 1、IInterface

> 类型：接口 作用：供Java层服务接口继承的接口，所有的服务提供者，必须继承这个接口

比如我们知道的ActivityManagerService继承自ActivityManagerNative，而ActivityManagerNative实现了IActivityManager，而IActivityManager继承自IInterface

###### 2、IBinder

> 类型：接口 作用：Java层的IBinder类，定义了Java层Binder通信的一些规则；提供了transact方法来调用远程服务

比如我们知道的Binder类就是实现了IBinder

###### 3、Binder

> 类型：类 作用：实现了IBinder接口，封装了JNI的实现。Java层Binder服务的基类。存在服务端的Binder对象

比如我们知道的ActivityManagerService继承自ActivityManagerNative，而ActivityManagerNative继承自Binder

###### 4、BinderProxy

> 类型：类 作用：实现了IBinder接口，封装了JNI的实现，提供了transaction()方法提供进行远程调用

存在客户端的进程的服务端Binder的代理

###### 5、Parcel

> 类型：类 作用：Java层的数据包装器，跨进程通信传递的数据的载体就是Parcel

我们经常的Parcelable其实就是将数据写入Parcel。具体可以看C++层的Parcel类

##### (二)、JNI层相关类

###### 1、JavaBBinderHolder

> 类型：类 作用：内部存储了JavaBBinder

###### 2、JavaBBinder

> 类型：类 作用：将C++端的onTransact调用传递到Java端

JavaBBinder和JavaBBinderHolder相关的类类图如下所示(若看不清，请点击看大图)，JavaBBinder继承自本地框架的BBinder，代表Binder Service服务端的实体，而JavaBBinderHolder保存了JavaBBinder指针，Java层的Binder的mObject保存的JavaBBinderHolder指针的值，故此这里用聚合关系表示。BinderProxy的mObject保存的是BpBinder对象的指针的值，故此这里用聚合关系表示

![](https://ask.qcloudimg.com/http-save/yehe-2957818/qk8md1fdz8.png)

Binder.png

###### 3、Java层Binder对象和NativeBinder对象相互转化的方法

这里涉及两个重要的函数

-   1、javaObjectForBinder(JNIEnv\* env, const sp<IBinder>& val)——>将Native层的IBinder对象转化为Java层的IBinder对象。
-   2、ibinderForJavaObject(JNIEnv\* env, jobject obj)——> 将Java层的IBinder对象转化为Native层的IBinder对象

这样就实现了两层对象的转化

##### (三)、Native层相关类

###### 1、BpRefBase

> 类型：基类 作用：RefBase子类，提供remote方法获取远程Binder，Client端在查询ServiceManager获取所需的BpBinder后，BpRefBase负责管理当前获取的BpBinder实例。

###### 2、IInterface

> 类型：基类 作用：Binder服务接口的基类，Binder服务通常需要同时提供本地接口和远程接口。它的子类分别声明了Client/Server能够实现所有的方法。

###### 3、BpInterface

> 类型： 接口 作用：远程接口的积累，远程接口是供客户端调用的接口集 如通client端想要使用 Binder IPC与Service通信，那么首先会从SerrviceManager处查询并获得server端service的BpBinder，在client端，这个对象被认为是server端的远程代理。为了使Client能能够像本地一样调用一个远程server，server需要向client提供一个接口，client在这个接口的基础上创建一个BpInterface，使用这个对象，client的应用能够像本地一样直接调用server端的方法。而不是用去关心具体的Binder IPC实现

BpInterface的原型：

```
template<typename INTERFACE>
class BpInterface : public INTERFACE, public BpRefBase
```

BpInterface是一个模板类，当server提供了INTERFACE接口(例如IXXXService),通常会继承BpInterface模板实现了一个BpXXXService

```
class BpXXXService: public BpInterface<IXXXService>
```

> 实际上BpXXXService实现了双继承IXXXService和BpRefBase，这样既实现了Service中各方法的本地操作，将每个方法的参数以Parcel的形式发给Binder Driver；同时又将BpBinder作为自己的成员来管理，将BpBinder存储在mRemote中，通过调用BpRefBase的remote()函数来获取BpBinder的指针。

###### 4、BnInterface

> 类型 ：接口 作用：本地接口的基类，本地接口是需要服务中真正实现的接口集合 BnInterface也是一个模板类。在定义Native端的Service时，基于server提供的INTERFACE接口(IXXXService)，通常会继承BnInterface模板类实现一个BnXXXService，而Native端的Service继承自BnXXXService。BnXXXService定义了一个onTransact函数，这个函数负责解包收到的Parcel并执行client端的请求方法。BnInterface的原型是：

```
template<typename INTERFACE>
class BnInterface : public INTERFACE, public BBinder
```

BnXXXService 例如：

```
class BnXXXService: public BnInterface<IXXXService>
```

IXXXService为client端的代理接口BpXXXService和Server端的BnXXXServer的共同接口类，这个共同接口类的目的就是保证Service方法在C/S两端的一致性。

> 每个Service都可视为一个binder，而真正的Service端的Binder操作及状态的维护就是通过继承自BBinder来实现的。BBinder是Service作为BBinder的本质所在

那么，BBinder与BpBinder的区别是什么？BpBinder是Client端创建的用于向Server发送消息的代理，而BBinder是Server端用于接受消息的通道。他们代码中虽然均有transact方法，但两者的作用不同，BpBinder的transact方法时向IPCThreadStata实例发送消息，通知其有消息要发送给Binder Driver；而BBinder则当IPCThreadState实例收到Binder Driver消息时，通过BBinder的transact方法将其传递给它的子类BnXXXService的onTransact函数执行Server端的操作

###### 5、IBiner

> 类型 接口 作用 Binder对象的基类，BBinder和BpBinder都是这个的类的子类这个类比较重要，说一下他的几个方法

|方法名|说明|
|---|---|
|localBinder|获取本地Binder对象|
|remoteBinder|获取远程Binder对象|
|transact|进行一次Binder操作|
|queryLocalInterface|获取本地Binder，如果没有则返回NULL|
|getInterfaceDescriptor|获取Binder的服务接口描述，其实就是Binder服务的唯一标识|
|isBinderAlive|查询Binder服务是否还活着|
|pingBinder|发送PING_TRANSACTION给Binder服务|

###### 6、BpBinder

> 类型 类 作用 远程Binder对象，这个类提供transact方法来发送请求，BpXXX中会用到。

BpBinder的实例代表了远程Binder，这个类的对象将被客户端调用。其中**handle()函数**将返回指向Binder服务实现者的句柄，这个类最重要的就是提供了**transact()函数**，这个函数将远程调用的参数封装好发送给Binder驱动。

###### 7、BBinder

> 类型 基类 作用 本地Binder，服务实现方的基类，提供了onTransact接口来接收请求。

BBinder的实例代表了本地的Binder，它描述了服务的提供方，所有Binder服务的实现者都继承这个类(的子类)，在继承类中，最重要的就是实现**onTransact()函数**，因为这个方法是所有请求的入口。因此，这个方法是和BpBinder中的**transact()函数**对应的。

由于BBinder与BpBinder都是IBinder的子类，具体区别如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/t8zbpokyv5.png)

BBinder与BpBinder.png

###### 8、ProcessState

> 类型 类 作用 代表Binder的进程

ProcessState是以单例模式设计的，每个进程在使用Binder机制进行通信时，均需要维护一个ProcessState实例来描述当前进程在Binder通信时Binder状态。

> ProcessState有两个主要功能：

-   1、创建一个thread，该线程负责与内核中的Binder模块进行通信，该线程称为Poolthread；
-   2、为指定的handle创建一个BpBinder对象，并管理该进程中所有的BpBinder对象。

在Binder IPC中，所有进程均会启动一个thread来负责与Binder Drive来通信，也就是不停的读写Binder Drive。Poolthread的启动方式为

```
ProcessState::self()->startThreadPool();
```

BpBinder的主要功能是负责Client向Binder Driver发送调用请求的数据，它是Client端Binder通信的核心对象，通过调用transact函数向Binder Driver发送调用请求数据。BpBinder的构造函数为

```
BpBinder(int32_t handle);
```

该构造函数可见，BpBinder会将通信中的Server的handle记录下来，当有数据发送时，会通知Binder Driver数据的发送目标。 ProcessState通过下下述方式来获取BpBinder对象。

```
ProcessState::self()->getContextObject(handle);
```

ProcessState创建的BpBinder实例，一般情况下会作为参数创建一个Client端的Service代理接口，例如BpXXX，在和ServiceManager通信时，Client会创建一个代理接口BpServieManager。

###### 9、IPCThreadState

> 类型 类 作用 代表了使用Binder的线程，这个类中封装了与Binder驱动通信的逻辑，说白了就是负责与Binder Driver的驱动

IPCThreadState是以单例模式设计的。因为每个进程只维护了一个ProcessState实例，同时Process state只启动了一个Poolthread，因此每个进程只需要一个IPCThreadState即可。

Poolthread实际内容为：

```
IPCThreadState::self()->joinThreadPool();
```

> ProcessState中有两个Parcel对象，mIn和mOut。Poolthread会不停的查询Binder Driver中是否有数据可读，如果有，将其读出并保存到mIn;同时不听的查询mOut是否有数据要想Binder Driver发送，如果有，则将其内容写入Binder Driver中。总而言之，从Binder Driver读出的数据保存在mIn，待写入到Binder Driver中的数据保存在mOut中。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/apr91qfs5d.png)

in与out.png

ProcessState中生成了BpBinder实例通过调用IPCThreadState的transact函数来向mOut中写入数据。

> IPCThreadState有两个重要的函数，talkWithDriver函数负责从Binder Driver续写数据，executeCommand函数负责解析并执行mIn中的数据

###### 10、Parcel

> 类型 类 作用 在Binder上传递数据的包装器。

Parcel是Binder IPC中最基本的通信单元，它存储C/S间函数调用的参数。Parccel只能存储基本的数据类型，如果是复杂的数据类型的话，在存储时，需要将其拆分为基本的数据类型来存储。

> Binder通信的双方即可以作为Client，也可以作为Server，此时Binder通信是一个半双工的通信，操作的过程会比单工的情况复杂，但是基本原理是一样的。

###### 11、补充

> 这里补充一下 这里的IInterface、IBinder和C++层的两个类是同名的。这个同名并不是巧合：它们不仅仅同名，它们所起的作用，以及其中包含的接口都是几乎一样的，区别仅仅是一个在C++层，一个是在Java层

###### 12、类关系

下图描述了这些类之间的关系：

> PS:Binder的服务实现类(图中紫色部分)通常都会遵守下面的命名规则：

-   服务的接口使用I字母作为前缀
-   远程接口使用Bp作为前缀
-   本地接口使用Bn作为前缀

![](https://ask.qcloudimg.com/http-save/yehe-2957818/5gtvlsuxct.png)

Native层类关系.png

###### 12、Native流程

那我们来看下在Native层中的Binder流程，以MediaPlayer为例

![](https://ask.qcloudimg.com/http-save/yehe-2957818/8rsa9n99no.png)

Native层的Binder流程.png

总结一下：

-   1、在已知服务名的情况下，App通过getService()从ServiceManager获取服务的信息，该信息封装在Parcel里。
-   2、应用程序收到返回的这个Parcel对象(通过Binder Drive)，从中读取出flat\_binder\_object对象，最终从对象中得到服务对应的服务号，mHandle。
-   3、以该号码作为参数输入生成一个IBinder对象(实际上是BpBinder)。
-   4、应用获取该对象后，通过asInterface(IBinder\*)生成服务对应的Proxy对象(BpXXX)，并将其强转为接口对象(IXXX)，然后直接调用接口函数。
-   5、所有接口对象调用最终会走到BpBinder->transact()函数，这个函数调用IPCThreadState->transact()并以Service号作为参数之一。
-   6、最终通过系统调用ioctl()进入内核空间，Binder驱动根据传进来的Service号寻找该Service正处于等待状态的Binder Thread，唤醒它并在该线程内执行相应的函数，并返回结果给App。

强调一下：

> 1、从应用程序的角度来看，他只认识IBinder和IMediaPlayer这两个类，但真正的实现在BpBinder和BpMediaPlayer，这正式是设计模式中所推崇的“Programs to interface，not implementations”，可以说Android是一个严格遵循设计模式思想精心设计的系统。 2、客户端应该层层的封装，最终目的就是获取和传递这个mHandle值，从图中，我们看到，这个mHandle值来自与IServiceManager，他是一个管理其他服务的服务，通过服务的名字我们可以拿到这个服务对应的Handle号，类似网络[域名](https://dnspod.cloud.tencent.com/?from_column=20065&from=20065)服务系统。但是我们说了，IServiceManager也是服务，要访问他我们也需要一个Handle号，对了，就如同你必须为你的机器设置DNS[服务器](https://cloud.tencent.com/product/cvm/?from_column=20065&from=20065)地址，你才能获得DNS服务。在Android系统里，默认的将ServiceManager的Handler号设为0，0就是DNS服务器的地址，这样，我们通过调用getStrongProxyForHandle(0)就可以拿到ServiceManager的IBinder对象，当然系统提供一个getService(char\*)函数来完成这个过程。 3 Android Binder设计目的就是让访问远端服务就像调用本地函数一样简单，但是远端对象不再本地控制之内，我们必须保证调用过程中远端的对象不能被析构，否则本地应用程序将很可能崩溃。同时，万一远端服务异常退出，如Crash，本地对象必须知晓从而避免后续的错误。Android通过智能指针和DeathNotifacation来支持这两个请求。

Binder的Native层设计逻辑简单介绍完毕。我们接下来看看Binder的底层设计。

##### (四)、Linux内核层的结构体

Binder驱动中有很多结构体，驱动中的结构体可以分为两类：

-   与用户空间共用的，定义在[binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199113&objectType=1&isNewArticle=undefined)中
-   仅在Binder Driver中使用的，定义在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199113&objectType=1&isNewArticle=undefined)中

###### 1、与用户控件共用的结构体

|结构体名称|说明|
|---|---|
|flat_binder_object|描述在Binder在IPC中传递的对象|
|binder_write_read|存储一次读写操作的数据|
|binder_version|存储Binder的版本号|
|transaction_flags|描述事务的flag，例如是否是异步请求，是否支持fd|
|binder_transaction_data|存储一次事务的数据|
|binder_ptr_cookie|包含了一个指针和一个cookie|
|binder_handle_cookie|包含了一个句柄和一个cookie|

这里面**binder_write_read**和**binder_transaction_data**这两个结构体最为重要，它们存储了IPC调用过程中的数据

###### 2、仅在Binder驱动中使用
|结构体名称|说明|
|---|---|
|binder_node|描述了Binder实体节点，即对应一个Server|
|binder_ref|描述对于Binder实体的引用|
|binder_buffer|描述Binder通信过程中存储数据的Buffer|
|binder_proc|描述使用Binder的进程|
|binder_thread|描述Binder的线程|
|binder_work|描述通信的一项任务|
|binder_transaction|描述一次事务的相关信息|
|binder_deferred_state|描述延迟任务|
|binder_ref_death|描述Binder实体死亡的信息|
|binder_transaction_log|debugfs日志|
|binder_transaction_log_entry|debugfs日志条目|
###### 3、总结

-   1 当一个service向Binder Driver注册时(通过flat\_binder\_object)，Binder Driver会创建一个binder\_node，并挂载到service所在进程的nodes红黑树中。
-   2 这个service的binder线程在proc->wait队列上进入睡眠等待。等待一个binder\_work的到来。
-   3 客户端的BpBinder创建的时候，它在Binder Driver内部也产生了一个binder\_ref对象，并指向某个binder\_node，在Binder Driver内部，将client和server关联起来。如果它需要或者Service的死亡状态，则会生成相应的binder\_ref\_death。
-   4 客户端通过transact() (对应内核命令BC\_TRANSACTION)请求远端服务，Driver 通过ref->node的映射，找到service所在进程，生产一个binder\_buffer，binder\_transaction和binder\_work并插入proc->todo对下列，接着唤醒某个睡在proc->wait队列上的Binder\_thread，与此同时，该客户端线程在其线程的wait队列上进入睡眠，等待返回值。
-   5 这个binder thread 从proc->todo队列中读出一个binder\_transaction，封装成transaction\_data(命令为BR\_TRANSACTION)并送到用户控件。Binder用户线程唤醒并最终执行对应的on\_transact()函数
-   6 Binder用户线程通过transact()向内核发送BC\_REPLY命令，Driver收到后从其thread->transaction\_stack中找到对应的binder\_trannsaction，从而知道是哪个客户端线程正在等待这个返回
-   7 Driver 生产新的binder\_transaction(命令 BR\_REPLY)，binder\_buffer，binder\_work，将其插入应用线程的todo队列，并将该线程唤醒。
-   8 客户端的用户线程收到回复数据，该Transaction完成。
-   9 当service所在进程发生异常，Driver的release函数被调用到，在某位内核work\_queue线程里完成该service在内核态的清理工作(thread,buffer,node,work...)，并找到所有引用它的binder\_ref，如果某个binder\_ref有不在空的binder\_ref\_death，生成新的binder\_work，送入其线程的todo队列，唤醒它来执行剩余工作，用户端的 DeathRecipient会最终调用来完成client端的清理工作。

西面这张时序图描述了上述一个transaction完成的过程。不同颜色代表不同的线程。注意的是，虽然Kernel和User space线程颜色是不一样的，但所有的系统调用都发生在用户进程的上下文李(所谓上下文，就是Kernel能通过某种方式找到关联的进程，并完成进程的相关操作，比如唤醒某个睡眠线程，或跟用户控件交换数据，copy\_from，copy\_to，与之相对应的是中断上下文，其完全异步出发， 因此无法做任何与进程相关的操作，比如睡眠，锁等).

![](https://ask.qcloudimg.com/http-save/yehe-2957818/t8xwoaxyy7.png)

transaction过程.png

##### (五)、Binder整个通信个过程

![](https://ask.qcloudimg.com/http-save/yehe-2957818/unozqgzbp8.png)

Binder整个通信个过程.png

### 三、Binder机制概述

前面几篇文章分别从驱动，Native，Framework层介绍了Binder，那我们就来总结一下：

-   1、从IPC角度来说：Binder是Android中的一种跨进程通信方式，该方式在linux中没有，是Android独有的。
-   2、从Android Driver层：Binder还可以理解为一种虚拟物理设备，它的设备驱动是/dev/binder。
-   3、从Android Native层：Binder创建Service Manager以及BpBinder/BBinder模型，大推荐与binder驱动的桥梁
-   4、从Android Framework层：Binder是各种Manager(ActivityManager,WindowManager等)和相应的xxManagerService的桥梁
-   5、从Android App层：Binder是客户端和服务端进行通信的媒介，当binderService时候，服务端会返回一个包含了服务端业务调用的Binder对象，通过这个Binder对象，客户端就可以获取服务端提供的服务或者数据，这里的服务包括普通服务和基于AIDL的服务。

### 四、Binder通信概述

Binder通信是一种C/S结构的通信结构。

-   从表面上来看，是client通过获得一个server的代理接口，对server进行直接调用。
-   实际上，代理接口中定义的方法与server中定义的方法是一一对应的。
-   Client端调用某这个代理接口中的方法时，代理接口的方法会将client传递的参数打包成Parcel对象。
-   代理接口将该Parcel发送给内核中的Binder Driver。
-   Server端会读取Binder Driver中的请求的数据，如果是发送给自己的，解包Parcel对象，处理并将结果返回。
-   整个的调用过程是一个同步过程，在server处理的时候，client会block住。

整体流程如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/gbfzq80fdp.png)

Binder通信概述.png

### 五、Binder协议

Binder协议可以分为控制协议和驱动协议两类

##### (一) Binder控制协议

Binder控制协议是 进程通过 ioctl("/dev/binder") 与Binder设备进行通讯的协议，该协议包含以下几种命令：

|命令|说明|参数类型|
|---|---|---|
|BINDER_WRITE_READ|读写操作，最常用的命令。IPC过程就是通过这个命令进行数据传递|binder_write_read|
|BINDER_SET_MAX_THREADS|设置进程支持的最大线程数量|size_t|
|BINDER_SET_CONTEXT_MGR|设置自身为ServiceManager|无|
|BINDER_THREAD_EXIT|通知驱动Binder线程退出|无|
|BINDER_VERSION|获取Binder驱动的版本号|binder_version|

##### (二) Binder驱动协议

Binder驱动协议描述了对Binder驱动的具体使用过程。驱动协议又可以分为两类：

-   binder\_driver\_command\_protocol:描述了进程发送给Binder驱动的命令
-   binder\_driver\_return\_protocol:描述了Binder驱动发送给进程的命令

###### 1、binder\_driver\_command\_protocol 共包含17条命令，分别如下：
|命令|说明|参数类型|
|---|---|---|
|BC_TRANSACTION|Binder事务，即：Client对于Server的请求|binder_transaction_data|
|BC_REPLAY|事务的应答，即Server对于Client的回复|binder_transaction_data|
|BC_FREE_BUFFER|通知驱动释放Buffer|binder_uintptr_t|
|BC_ACQUIRE|强引用技术+1|_u32|
|BC_RELEASE|强引用技术-1|_u32|
|BC_INCREFS|弱引用+1|_u32|
|BC_DECREFS|弱引用 -1|_u32|
|BC_ACQUIRE_DONE|BR_ACQUIRE的回复|binder_ptr_cookie|
|BC_INCREFS_DONE|BR_INCREFS的回复|binder_ptr_cookie|
|BC_ENTER_LOOPER|通知驱动主线程ready|void|
|BC_REGISTER_LOOPER|通知驱动子线程ready|void|
|BC_EXIT_LOOPER|通知驱动线程已经退出|void|
|BC_REQUEST_DEATH_NOTIFICATION|请求接受死亡通知|binder_ptr_cookie|
|BC_CLEAR_DEATH_NOTIFICATION|去除接收死亡通知|binder_ptr_cookie|
|BC_DEAD_BINDER_DONE|已经处理完死亡通知|binder_uintptr_t|

**BC\_ATTEMPT\_ACQUIRE** 和 **BC\_ACQUIRE\_RESULT** 暂未实现。

###### 2、binder\_driver\_return\_protocol 共包含18条命令，分别如下：
|返回类型|说明|参数类型|
|---|---|---|
|BR_OK|操作完成|void|
|BR_NOOP|操作完成|void|
|BR_ERROR|发生错误|_s32|
|BR_TRANSACTION|通知进程收到一次Binder请求(Server端)|binder_transaction_data|
|BR_REPLAY|通知进程收到Binder请求的回复(Client端)|binder_transaction_data|
|BR_TRANSACTION_COMPLETE|驱动对于接受请求的确认税负|void|
|BR_FAILED_REPLAY|告知发送发通信目标不存在|void|
|BR_SPWAN_LOOPER|通知Binder进程创建一个新的线程|void|
|BR_ACQUIRE|强引用计数+1|binder_ptr_cookie|
|BR_RELEASE|强引用计数-1|binder_ptr_cookie|
|BR_INCREFS|弱引用计数+1|binder_ptr_cookie|
|BR_DECREFS|弱引用计数-1|binder_ptr_cookie|
|BR_DEAD_BINDER|发送死亡通知|binder_uintptr_t|
|BR_CELAR_DEATH_NOTIFACATION_DONE|清理死亡通知完成|binder_uintptr_t|
|BR_DEAD_REPLY|告知发送方对方已经死亡|void|

**BR\_ACQUIRE\_RESULT** 、**BR\_FINISHED** 和 **BR\_ATTEMPT\_ACQUIRE** 暂未实现。

单独看上面的协议可能很难理解，这里我们可以将一次Binder请求过程来看一下Binder协议是如何通信的，就比较好理解了，如下图：

图说明：

-   Binder是C/S机构，通信从过程涉及到Client、Service以及Binder驱动三个角色
-   Client对于Server的请求以及Server对于Client回复都需要通过Binder驱动来中转数据
-   BC\_XXX 命令是进城发送给驱动的命令
-   BR\_XXX 命令是驱动发送给进程的命令
-   整个通信过程有Binder驱动控制

![](https://ask.qcloudimg.com/http-save/yehe-2957818/q9huzvqh8i.png)

Binder通信.png

补充说明，通过上面的Binder协议，我们知道，Binder协议的通信过程中，不仅仅是发送请求和接收数据这些命令。同时包括了对于引用计数的管理和对于死亡通知管理的功能(告知一方，通信的另外一方已经死亡)。这些功能的通信过程和上面这幅图类似：

> 一方发送BC\_XXX，然后由驱动控制通信过程，接着发送对应BR\_XXX命令给通信的另外一方。因为这种相似性，我就不多说了。

上面提到了Binder的架构，那我们下面就来研究下Binder的结构

### 六、Binder架构

##### (一)、Binder架构的思考

在说到Binder架构之前，先简单说说大家熟悉的TCP/IP五层通信系统结构

![](https://ask.qcloudimg.com/http-save/yehe-2957818/lv26madvyo.png)

TCP/IP五层模型结构.png

-   应用层：直接为用户提供服务
-   传输层：传输的是报文(TCP数据)或者用户数据报(UDP数据)
-   网络层：传输的是包(Paceket)，例如路由器
-   数据链路层：传输的是帧(Frame)，例如以太网交互机
-   物理层：相邻节点间传输bit，例如集线器，双绞线等

这是经典的五层TCP/IP协议体系，这样分层设计的思想，让每一个子问题都设计成一个独立的协议，这协议的设计/分析/实现/测试都变得更加简单：

-   层与层具有独立性，例如应用层可以使用传输层提供的功能而无需知晓其原理原理。
-   设计灵活，层与层之间都定义好接口，即便层内方法发生变化，只有接口不变，对这个系统便毫无影响。
-   结构的解耦合，让每一层可以用更适合的技术方案，更适合的语言
-   方便维护，可分层调试和定位问题

Binder架构也是采用分层架构设计，每一层都有其不同的功能，以大家平时用的startService为例子，AMP为ActivityManagerProxy，AMS为ActivityManagerSerivce 如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/b8u3kmmlzj.png)

Binder分层.png

-   Java应用层：对于上层通过调用AMP.startService，完全可以不用去关心底层，经过层层调用，最终必然会调用到AMS.startService。
-   Java IPC层：Binder采用的是C/S脚骨，Android系统的基础架构便已经设计好的Binder在Java Framework层的Binder 客户端BinderProxy和服务端Binder。
-   Native IPC层： 对于Native层，如果需要使用Binder，则可以直接使用BpBinder和BBinder(也包括JavaBBindder)即可，对于上一层Java IPC通信也是基于这个层面。
-   Kernel物理层：这里是Binder Driver，前面三层都跑在用户控件，对于用户控件内存资源是不共享的，每个Android的进程只能运行在自己基础讷航所拥有的虚拟地址空间，而内核空间却是可共享的。真正通信的核心环节还是Binder Driver。

##### (二) 、Binder结构

![](https://ask.qcloudimg.com/http-save/yehe-2957818/lbxu93hjed.png)

Binder架构.png

> Binder在整个Android系统中有着举足轻重的地位，在Native层有一套完成的Binder通信的C/S架构（图中的蓝色），BpBinder作为客户端，BBinder作为服务端。基于native层的Binder框架，Java也有一套镜像功能的Binder C/S架构，通过JNI技术，与Native层的Binder对应，Java层的Binder功能最终都是交给native的Binder来完成。从kernel到native，jni，framwork层的架构所涉及的所有有关类和方法见下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/sxivocsv3g.jpeg)

java\_binder\_framework.jpg

##### (三) 、startService的流程

如下图:

![](https://ask.qcloudimg.com/http-save/yehe-2957818/qy8qrs48jt.png)

startService的流程.png

##### (四)、SeviceManager自身的注册和其他service的注册

这里放一张图说明整个过程

![](https://ask.qcloudimg.com/http-save/yehe-2957818/ad5w285ln6.png)

注册.png

详细经过这么多篇文章的讲解，大家对Binder有一点的了解，为了让大家加深对Binder的理解，推荐下面几篇文章 
[听说你Binder机制学的不错，来面试下这几个问题（一）](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Fwww.jianshu.com%2Fp%2Fadaa1a39a274&objectId=1199113&objectType=1&isNewArticle=undefined) 
[听说你Binder机制学的不错，来面试下这几个问题（二）](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Fwww.jianshu.com%2Fp%2Ffa652f57a552&objectId=1199113&objectType=1&isNewArticle=undefined) 
[听说你Binder机制学的不错，来面试下这几个问题（三）](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Fwww.jianshu.com%2Fp%2F9128f1b65586&objectId=1199113&objectType=1&isNewArticle=undefined)

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2017.08.23 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除