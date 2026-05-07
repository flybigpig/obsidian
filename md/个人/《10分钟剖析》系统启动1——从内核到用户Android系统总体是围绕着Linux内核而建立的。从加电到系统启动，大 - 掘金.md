        ![](https://lf-web-assets.juejin.cn/obj/juejin-web/xitu_juejin_web/img/banner.a5c9f88.jpg)


## 要看什么？


Android系统总体是围绕着Linux内核而建立的。从加电到系统启动，大体上经过：

-   bootloader
    -   kernel
        -   用户空间（init、zygote、framework、launcher等）

bootloader，距离App开发者太远了，是厂商最为关心的东西。某种角度上讲，其特定性太高，比如加电后我就要从内存芯片的第多少个offset偏移指针开始跑程序。很多是底层协议的东西，理解起来晦涩难懂且帮助不大。所以这里选择直接从Linux内核作为第一起点，以更加通用的逻辑切入系统的启动。

在系统启动系列文章中，所确立的主线是从加电启动到第一个app（也就是launcher）启动。所以，有过一定了解的朋友应该能够get到，核心是zygote。像binder会另辟系列文章加以剖析。

Let's go!


## Linux系统的启动

### head.S

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e71a90eb195~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

-   源码位置：kernel/common/arch/arm64/kernel/head.S
    
    机器加电后，不同的CPU根据不同的内存偏移设定，开始制定对应的汇编指令。以arm64架构为例，在插电后执行到了这里，指向了**start\_kernel**
    

### main.c

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e71a904ab69~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

-   源码位置：kernel/common/init.main.c
    
    **main.c**将**start\_kernel**暴露给汇编空间得以调用。被调用时，这时是处于**内核态**的，经过一些列内核的初始化，最后调用了**arch\_call\_rest\_init()**
    
    ![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e71a8f92953~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)
    
    在rest\_init中，通过**kernel\_thread**开启了pid为1的进程（0已经在sched\_init中初始化的idle thread占用走了，有兴趣自行研习下吧）
    
    ![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e71aa0d8399~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

> kernel\_thread很简单粗暴，就是用来在**内核态中开启一个线程**的（也可以理解为进程，在内核态中并没有特殊的数据结构来区分线程和进程，大意可以理解为一个东西）
> 
> ![image-20191223120032256](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e71aa1f3e95~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

**kernel\_init**这个function指针是重要的

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e71acc2e186~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

经过裁剪，核心代码如图。其中两个**execute\_command**是两个全局静态变量，接收外部指定：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e71e9ea6ec4~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e72038aeaeb~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

arm结构没有设置，但结合其他架构配置文件的设置可以看到，基本就是指向了**init**

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e71ead57368~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

再结合最后的兜底代码

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e7209a60c9a~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

可以确定，内核的1号进程（pid）的执行内容是用户空间的**init**

## init 用户空间的第一个应用程序

通过对[Soong编译系统](https://link.juejin.cn/?target=https%3A%2F%2Fsource.android.com%2Fsetup%2Fbuild "https://source.android.com/setup/build")的基本学习，不难发现，name为init的cc\_binary就再**system/core/init**下：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e7209e3dcab~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

> 同时，还可以发现，ueventd和watchdogd其实也是init这个应用程序所做的

### main.cpp

通过**Android.bp**的指引，看下**main.cpp**做了什么：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e720a988fa1~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

通过namespace的限定，可以定位到**init.cpp**

### init.cpp

-   源码位置：system/core/init/init.cpp

> init 作为第一个用户态的应用程序，做了非常多的工作，包括设置环境变量、挂载FS、权限设置等。基于探索Android系统启动的核心，我们挑最关心的部分看

在看源码之前，可以先去了解下Android初始化语言的[规范文档](https://link.juejin.cn/?target=https%3A%2F%2Fandroid.googlesource.com%2Fplatform%2Fsystem%2Fcore%2F%2B%2Fmaster%2Finit%2FREADME.md "https://android.googlesource.com/platform/system/core/+/master/init/README.md")。

初始化语言中，最重要的是两个角色：

#### Action

可以理解为当XXX发生时候，怎么怎么样：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e720b60409a~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

比如截图中：当到达early-init时，向某设备写入什么值。这里write这一行是command（命令），是具体的行为内容

#### Service

而服务是声明、定义。

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e720bfc41ab~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

如图中所示，声明了名为**ueventd**的服务，内容在下边描述，具体的执行文件是 **/sbin/ueventd**

基于对于Action和Service的初步了解，下边看源码：

#### main()

##### epoll

在开始解析配置文件init.rc等之前，创建了一个epoll句柄：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e722befd635~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

在创建epoll句柄后，马上注册了关于SIGCHLD的信号处理回调。

> epoll是Linux中的一项多路复用技术，自2.5版本引入。
> 
> 这里对于epoll的使用是说，init应用程序响应其所启动的各种子程序的SIGCHLD信号（即子程序退出、终止），根据相关数据，会有需要对一些子程序做重启操作。在这，是将SIGCHLD信号打到了socket上，让epoll监听此socket。在后边还有几处使用epoll来监听事件的操作，这里不做过多展开了

## 解析

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e722bd613b0~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

紧接着，去解析了Action和Service。解析后所构造出来的Action和Service实例存储在各自Manager、List的Vector中（详见service.h & action\_manager.h）。

解析的过程：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e722bfe0ed8~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

通过源码可以看到，可以指定初始化配置文件的地方非常多，甚至可以用环境变量。翻遍AOSP源码，也没有人去设置**ro.boot.init\_rc**这个环境变量，所以都会是init.rc。

> init.rc在于文件系统的根目录，而其携带者是boot.img，也就是说他被编译打包在boot.img中。boot.img和system.img以及recovery.img，相信有同学会很熟悉，都是需要通过BootLoader刷机才可以进行更新的。所以常规渠道来讲，只要手机出厂，init.rc中相关信息就是不可修改的

而解析的动作，是由Parser执行的：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e722c280985~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

结合前边给出的一些init.rc的配置片段，确定service开头的line被ServiceParser去解析，而on被ActionParser去解析，import最后可以理解为递归的解析每一个所引入的其他rc文件。

> 具体对文本内容逐行解析的过程在system/core/init/parser.cpp#ParseData()中，本文不做具体解析

解析结束后：

-   actions存储于ActionManager中
-   services存储于ServiceList中

##### 触发

上边说了，Service是静态的声明、定义。那么事件的驱动，一定是要靠Action推动的，所以触发的源头就是Action的流转：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e722d5a6fbe~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

这里ActionManager是把各种事件内容都放到了内部的队列中，并没有进行触发：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e722fb8524a~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

而其所能够接受的类型有三种：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e724c5ec166~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

其分别对应ActionManager的三个操作：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e724e402f4a~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp) ![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e7251ffd500~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

而init的main()中只用了1和3两种方式，很多都是通过内建Action的方式对已经解析出的\*\*actiions\_\*\*做了扩充。

下边就是真正会去触发action的地方：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e725352c8a1~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

> while-true (A)

-   进入了无限循环，没有退出的case。这里的设计思路是：
    
    1.  通过-1超时时间的设计，在没有事件发生的时候，通过epoll可做到完全休眠，还不浪费系统资源
    2.  如果真的有了epoll时间，那么每次唤醒后的动作也是在这个while-true中进行处理的
-   关机或重启事件是一个优先需要处理的，其对应着property服务的处理过程（题外话）：
    
    ![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e7253a4a135~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)
    
    而**handle\_property\_set\_fd**内部最终就会对应着以下内容：
    

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e72544125c7~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

这样，也就不难理解，关机、重启的逻辑是怎么来的了。

-   再然后，正常的，会得到一个纯净的可执行环境，通过AM去执行一条命令

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e72760c5b42~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.awebp)

> while-true (B)

细节的内容通过图中注释已经做了说明。可以发现，只要AM的action队列中还有存货，就会按照顺序流水账一样的执行下去。

#### 总结

-   Action绑定在事件上，被事件驱动。内部包含了一系列的Command，当被驱动时，会去执行command
-   Service是静态声明，下边包含了许多Options用来形容这个Service是什么，要做什么
-   init.rc及其import的各种rc文件会被init解析，化为ActionManager & ServiiceList中的数据集合
-   AM中存在一个**事件队列event\_queue**，队列中依次包含着如early-init/init/late-init等状态，以及也会从中插入一些特殊的时序敏感的Action，他们都会按顺序依次执行
-   当事件被驱动起来，绑定（依赖、监听）在其上的Action动了起来，Action中的Commands动了起来，包括一些setprop、chown等命令操作，同样也包含了对Service的start操作

## Next

zygote孵化器进程