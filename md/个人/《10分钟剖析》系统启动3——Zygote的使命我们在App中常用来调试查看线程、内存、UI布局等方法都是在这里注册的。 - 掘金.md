## Previous

-   用户态1字号（pid=1）应用程序 **init** 透过 **app\_process** 发起zygote启动动作
-   **app\_process** 通过操作 **AppRuntime** （ **AndroidRuntime** 的派生类）初始化并启动JVM
-   ART虚拟机得到启动，JNI调用环境得到初始化，众多Android API相关JNI得以注册
-   通过 **env->CallStaticVoidMethod()** 发起对\*\*ZygoteInit.java#main()\*\*的调用，世界切换至Java

![砸瓦鲁多](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bbe580cb6d~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

总的层次调用为：

-   bootloader
    -   kernel
        -   init（首次切换到用户态）
            -   app\_process64（对64位机型来说）在此启动Java世界，启动zygote

> ZygoteInit.java的main()会做什么？

源码位置：frameworks/base/core/java/com/android/internal/os/ZygoteInit.java

## new ZygoteServer、ZygoteHooks、setpgid

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bbe5bbe4d3~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

-   创建ZygoteServer，它的职能是借助socket暴露API（后边会详细介绍ZygoteServer）
    
-   而后通过ZygoteHooks和虚拟机进行交互，告知虚拟机在zygote创建期间不要开启新的线程，否则会报错
    
-   Os.setpgid(0,0)并不是真的能将自己的pid设置成0，详见其底层方法的[官方解释](https://link.juejin.cn/?target=http%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fsetpgid.2.html "http://man7.org/linux/man-pages/man2/setpgid.2.html")
    
    > ![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bbe7821272~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)
    > 
    > 无关部分隐去。Linux API手册中说的是，如果都给0，那么会跟随当前所处的进程的pid和pgid（对于zygote来说就是init在调用app\_process时产生的进程的pid和pgid了）
    

## 使能DDMS

-   RuntimeInit.enableDdms()
    
    -   android.ddm.DdmRegister.registerHandlers()
        
        ![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bbe6a7e4a5~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)
        

我们在App中常用来调试查看线程、内存、UI布局等方法都是在这里注册的。

> 如果做性能收集专项，学习下系统是如何做的，可以从这里入手。

## 解析参数

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bbe5c44628~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

还记得传入ZygoteInit#main()方法的参数吗？

-   com.android.internal.os.ZygoteInit
-   start-system-server
-   \--abi-list=arm64-v8a

所以for循环里i从1开始计数，自然也是可以理解的了。而socket又是哪里来的呢？这个可以回头看一下init在start service时做了什么。

### zygote socket插曲

回忆init.zygote64.rc

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bbe6bcabe7~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

在做rc文件解析时，Service解析过程中：

> system/core/init/service.cpp

-   Service::ParseLine(每行的文字内容)
    -   OptionParserMap::FindFunction(透传) //通过socket找到ParseSocket函数指针
        -   构造SocketInfo到descriptors\_中

待Service:Start()被调用时：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bc0b7d5022~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

-   DecriptorInfo::CreateAndPublish()
    -   SocketInfo::Create()
        -   util.cpp#CreateSocket()
    -   setenv //对于SocketInfo而言设置的是环境变量 ANDROID\_SOCKET\_**socket\_name**

即，在Service启动时就同时创建了名为zygote的socket，具体的socket地址是：

`/dev/socket/zygote`

创建完成后，将socket句柄数据以环境变量的方式，放置在了 **ANDROID\_SOCKET\_zygote** 的key下。

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bc0bdb5284~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

> 回到参数解析。默认给了`socketName = zygote`，而之前我们记录的参数中也并无socket配置，所以最后生效的socket名字就是`zygote`无疑

## 连接socket

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bc0d1afecf~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

zygoteServer的连接也是后边详细说明，这里直接说结论：这里是将init所创建的名为zygote的socket给注册到Java空间来。

## LazyPreload 懒-预加载

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bc0d1189d7~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

上边的参数解析中，并没有指定lazy-perload，所以这里enableLazyPreload是false的，那么会进行preload()。这里的逻辑是：

1.  如果是Zygote，也就是不需要进行懒加载，那么就先行预加载一些东西
2.  如果需要懒加载的话，会将线程的优先级调低。可以理解为，需要懒加载，那么必然就代表着不是很重要吧

preload的内容：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bc2377b6bd~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

其中和我们最息息相关的就是class的预加载了，这里边包含了众多API的Java类、Android内部类，最常见的比如Activity、Fragment、Service等也在预加载之列，而预加载的方式就是通过Class.forName：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bc23c46a58~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

以我的AOSP源码环境为例，体量巨大，高达6500+行：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bc2faefd6e~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

## Zygote.nativeSecurityInit() 访问安全

接下来，调用了`Zygote.nativeSecurityInit();`进行native的初始化，对应着：framworks/base/core/jni/com\_android\_internal\_os\_Zygote.cpp# **com\_android\_internal\_os\_Zygote\_nativeSecurityInit()**

> 随时复习，这个是在AndroidRuntime初始化时进行注册的，就是在进入Java世界前的事情，详细参见上一篇

具体的内容是对SELinux进行了操作，因为系统设计上，只有系统能做SELinux相关的操作，App进程是不允许的。作为Java世界的头号玩家，Zygote在做fork（衍生出其他App前）自然要先做好安全这一步。

## Zygote.nativeUnmountStorageOnInit() 隔离存储

在这一步调用了com\_android\_internal\_os\_Zygote.cpp中的com\_android\_internal\_os\_Zygote\_nativeUnmountStorageOnInit()方法。目的是将整体的存储目录/storage卸载，取而代之的是挂载临时目录，这个动作和Android9及10所说的 **隔离存储** 有关，也可以叫做 **沙盒存储** 。这一部分可以参照[官方说明](https://link.juejin.cn/?target=https%3A%2F%2Fsource.android.com%2Fdevices%2Fstorage "https://source.android.com/devices/storage")。

## ZygoteHooks.stopZygoteNoThreadCreation()

与上边呼应，告诉ART虚拟机，现在可以在zygote进程中创建线程了，下图是虚拟机中实际创建线程的入口方法，可以看到，会通过对应的标记为去判断究竟是否可以创建线程，如果触发红线的话，会抛出InternalError：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bc446d46fb~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

> 在继续跟进之前，需要理清楚一个关键的角色 **ZygoteServer**

## ZygoteServer

其在ZygoteInit.java#main()，除了构造，其被调用到的方法和顺序是：

-   registerServerSocketFromEnv(socketName) //zygote
-   runSelectLoop()
-   closeServerSocket()

### registerServerSocketFromEnv

前面说到，在init运行zygote server时候，创建了名为zygote的socket，然后将对应的socket句柄（int值）以环境变量的方式存储在了key => **ANDROID\_SOCKET\_zygote** 中。

在此方法中，通过对应的环境变量获取到了这个socket的句柄数据，然后将其转换为了Java空间的ServerSocket：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bcab368372~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

在 **LocalServerSocket** 中，又透过 **LocalSocketImpl** 开始了真正的监听。

> 这里只是注册，开始了监听，但还没有去获取其中的数据

### runSelectLoop

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bcaa59ef2e~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

ZygoteServer通过poll的方式轮训检查zygote socket。检查时对两类socket做区分处理：

1.  刚刚创建的ServerSocket，当这个句柄收到新数据时，代表的是有新的socket连接进入，此时会构造新的ZygoteConnection，放入下一轮的轮训句柄集合中（ServerSocket在集合中的位置永远是0），并开启新一轮轮训
2.  对于ZygoteConnection，也就是来自客户端的连接。会调用`ZygoteConnection.processOneCommand()`解析命令，获得对应的Runnable可执行命令，这个过程中也就发生了fork。fork后在父进程中关闭连接，而子进程中就返回了这个Runnable

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bc445a127c~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

> 大概流程如上图，忽略序号的顺序，因为有fork产生了子进程，所以顺序上看连线判定吧

### closeServerSocket

最后，顾名思义，关闭ServerSocket，不再接受连接。也就是说，zygote进程的服务终止了，这对于Android系统来说是致命错误，正常运行情况下是绝无可能发生的。那为什么还有这个方法呢？

上边的时序图中，有一个点：fork后，在父进程是不返回的，只是在ZygoteConnection中关闭了来自客户端的连接（因为此时已经处理完了）。而在fork出来的子进程中是不需要在有zygote服务的，所以这里的关闭理论上是为了在子进程中关闭无用的zygote服务所说。

### 总结

分析完ZygoteServer三个关键的方法后，发现其定位就是zygote服务和客户端的连接器、处理器。客户端通过名为zygote的socket发来一些启动请求，由zygote进程fork出来子进程，享用zygote在启动初期所做好的一切（JVM初始化、JNI初始化、class预加载、资源预加载等），而后执行通过命令解析出的Runnable（下边会直接列出解析的流程）。这就是一个新的App启动过程中至关重要的一个部分，后边会分析App启动，也会碰触到这一块。

#### processOneCommand解析Runnable过程

-   ZygoteConnection.processOneCommand
    -   readArgumentList //从socket中读取参数列表
    -   new Arguments //解析参数
    -   fork
        -   父进程
            -   return null
        -   子进程
            -   ZygoteServer.setForkChild() //告知ZygoteServer当前在子进程
            -   ZygoteServer.closeServerSocket() //如上所说，子进程中已经不在需要zygote服务
            -   handleChildProc
                -   Process.setArgV0 //设置Process Name，zygote进程的名字也是这么set来的
                -   ZygoteInit.zygoteInit()
                    -   RuntimeInit.applicationInit()
                        -   findStaticMain() [//一个执行android.app.ActivityThread#main()方法的Runnable](https://xn--android-4t3kgm404o5d9e.app.activitythread/#main()%E6%96%B9%E6%B3%95%E7%9A%84Runnable "//xn--android-4t3kgm404o5d9e.app.ActivityThread#main()%E6%96%B9%E6%B3%95%E7%9A%84Runnable")

## fork SystemServer

> 经过了ZygoteServer的梳理，现在回到ZygoteHooks.stopZygoteNoThreadCreation()后的systemserver fork进程中

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bcae021917~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

### Arguments

下面是用来启动system\_server的关键参数：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bcb0b461d5~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

uid、gid和组别信息，niceName、runtime参数、以及最重要的 **classPath=com.android.server.SystemServer** 。

以上的参数会被ZygoteConnection的内部类Arguments解析，解析过程大体如下：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bcb3aed481~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

-   `--`会被视为是要跳过的参数字符
-   其他的参数会有重复解析的报错

当所有已知的内容被解析过之后，会检查剩余是否还有参数：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bcc750bc05~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

根据上述用于启动system\_server的参数，我们在这里可以判定，会走到最后的case中，即剩余的参数会被保存在remainingArgs中，即`com.android.server.SystemServer`

### Zygote.forkSystemServer()

参数解析完成后，ZygoteInit调用`Zygote.forkSystemServer()`着手进行fork。

> 此方运行调用后，会触发两次返回，先后顺序不一定（同样也不重要）。
> 
> -   一次返回是带着大于0的pid回来，此时runtime处于父进程，这个pid就是fork出来的子进程的pid
> -   另一次返回是带着等于0的pid回来，此时runtime处于子进程

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bcd263f577~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

#### ZygoteHooks.preFork()

这里是做fork前的准备，主要是通过调用`Daemon.stop()`来操作守护线程的停止：

-   堆守护
-   引用队列守护
-   内存回收守护
-   内存回收看门狗守护线程

#### resetNicePriority()

重置进程（主线程）优先级为 **5** ，这一点通过我们自己开发的App线程信息可以查看到

#### nativeForkSystemServer()

这里进行了真正的fork，使得整个流程在这里发起了两次的返回。一个比较关键的细节是：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bcd3c74974~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

在fork进行完成后，在 **system\_server** 进程（子）中透过JNI触发了Java空间的回调（Zygote.java）：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bcd3d4ca1b~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

这个调用最终在ART虚拟机中生效，虚拟机为从zygote衍生出来的子进程做了一系列的配置。当然，这个过程中，对于system\_server有过很多特殊的待遇（比如，关闭JIT）。

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bce956b467~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

#### ZygoteHooks.postForkCommon()

在这里又会启动上边所说的四个守护线程，并且重新设置线程优先级。很关键的一点是，这里会分别在父进程、子进程执行。zygote进程中，此时也是首次开启这几个守护线程。而system\_server进程，以及后边可能的其他三方App的守护线程也是在这里开启的。

> 至此，system\_server进程的fork就完成了，后边才开始在进程中做事

## 在sysetm\_server进程中运行

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bcf0a0220f~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

### return null

这里优先解释下最后的return null，把上文呼应的问题说清楚。

在ZygoteInit#main()方法中，我们之前对最后一段做过分析，回头复习一下：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bcea9d18a7~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

在fork完成system\_server进程后，父进程中直接返回了null。这样，在zygote的进程中就会执行到runSelectLoop，也就是上边所解释过的：处理zygote socket接收到的命令。

### system\_server子进程中

#### 关于second zygote socket

如果ABI（arm64等架构）有多个，那么会有第二个zygote socket，优先会去等待这第二个socket就绪。方法很传统，等待20s：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bd08f3298f~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

这部分不做过多讨论，我们聚焦于主线。

#### zygoteServer.closeServerSocket()

关于zygoteServer的关闭，在上边也解释过，子进程并不需要保持zygote socket的连接了，所以进行断开操作。这并不会影响主进程的server socket继续工作。

#### handleSystemServerProcess()

很关键的一点，关于入参，上边有一笔带过，在parsedArgs中还存留着一个叫做remainingArgs的变量，其内容是`com.android.server.SystemServer`。

-   首先，通过调用熟悉的`Process.setArgV0(parsedArgs.niceName)`将system\_server进程的名字真正命名为了`system_server`
-   然后，通过SYSTEMSERVERCLASSPATH环境变量获取到一些jar文件（实际是最终机器上的/system/frameworks/下的jar文件，是做system\_server的classpath），提前做dex opt优化（关于installd服务部分不在这里展开）
-   而后，创建classloader（PathClassLoader），并设置给主线程
-   最后，通过ZygoteInit构造可执行的Runnable（这里传入了我们关心的remainingArgs）

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/1/3/16f699bd08f77752~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

最后，正是这个Runnable，被return到了**ZygoteInit#main**方法中，被其调用。最终调用了`com.android.server.SystemServer#main()`方法，具体内容留作下次分析。