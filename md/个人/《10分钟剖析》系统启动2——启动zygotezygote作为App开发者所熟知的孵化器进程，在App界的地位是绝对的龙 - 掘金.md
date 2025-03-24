## Previous

-   机器上电，开始初始化内核
-   内核启动了第一个用户态的app——init
-   init通过解析init.rc及其所import的各个rc文件收集action和service
-   init通过epoll的循环机制，依次触发early-init、init、late-init等状态下的各种action，间接触发关联的service

## zygote服务

zygote作为App开发者所熟知的孵化器进程，在App界的地位是绝对的龙头老大。而zygote服务也是通过init作为服务启动起来的，其服务声明如下（以64位为例）：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62afbe458f~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

其文件位置为：system/core/rootdir/init.zygote64.rc

### init.rc

在init.rc中，可以看到相关的启动动作：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62b0002756~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

而start zygote最后真是发生了什么呢？又要把目光挪回到init中。

## init

在前文中，说到过init解析了一系列的\*.rc文件，收集了大量的Action进入到ActionManager。

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62b14c78e0~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

在init逐行解析rc文件时，Action的解析如上图，触发了action的 **AddCommand()** 方法。

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62b15bfaa8~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

这个function就是后边会被调用的真正内容，这个内容可以通过FindFunction()找到：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62ef1607fe~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

对于Action，大体上内建支持的集合有如这样：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62b21e08e8~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

而start所对应的：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62b70bb183~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

一探**do\_start**究竟：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62d3d05c1a~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

到这里可以做一下简单的梳理：

在init解析出Actions给ActionManager的同时，也解析了Service给ServiceList。

再次强化一下整体理解：

1.  `on XXX`叫做**Action**，而Action下的叫做**Command**。Command的全集可以通过**system/core/init/builtins.cpp**中的\*\*BuiltinFunctionMap::map()\*\*进行检索
2.  `service XXX`叫做**Service**，Service下的东西叫做**Option**。Option的全集可以通过 **system/core/init/service.cpp** 中的 **Service::OptionParserMap::map()** 进行检索

`start zygote`就是一条Action，而Action执行的路径：

-   am.ExecuteOneCommand()
    -   action->ExecuteOneCommand()
        -   Action::ExecuteCommand()
            -   command.InvokeFunc()
                -   function(builtin\_arguments) //指针函数调用
                    -   do\_start()
                        -   service.cpp#ExpandArgsAndExecv()

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62d61a51b6~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

首先需要补充一点：

Command怎么和Function绑定在一起的就要回头看一下ActionParser了。ActionParser在解析过程中，总会调用Action::AddCommand方法：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62f1bc2ead~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

> 即，通过map找到对应命令的function，添加到 **commands\_** 中去。

看下ExpandArgsAndExecv的本体：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62f122eb6d~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

可以发现两个问题：

1.  执行**ExpandArgsAndExecv**后，正常来讲，service就一直处于running状态了，即对于init这个父进程而言会是**阻塞**的，按常理不会走到exit -127(Command not found)
2.  最终执行的命令就是通过args拼装出来的，而args是在Service解析过程中就被确定的：

![image-20191225182557198](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62ef269231~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

而Service的构造是通过ServiceParser::ParseSection()来的：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e631d7a21e0~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

具体rc文件的解析过程有兴趣的同学可以围观system/core/init/parser.cpp，这里就不做详细分析了，过于枯燥。

结论是，结合之前zygote的service声明，最终会执行的命令就是：

`/system/bin/app_process64 -Xzygote /system/bin --zygote --start-system-server`

至此，init进程关于zygote的生涯结束。

## app\_process

源码位置：frameworks/base/cmds/app\_process

通过Android.mk可以确定app\_process64可执行文件就在此生成（新版本已改为Android.bp）：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62f71d25a0~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

下面深入源码剖析。

### app\_main.cpp

调用C/C++可执行程序，众所周知要从main方法看起：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e62f87b6268~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

这是一个标准的main入口方法，值得注意的有两点：

1.  AppRuntime这个runtime的存在
2.  通过argc的减一和argv的指针位移，跳过了"app\_process64"这个argv\[0\]

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e631321709c~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

至于AndroidRuntime，这是个非常重要的角色，是Android运行时真正的起点，整个framework的奠基人，后边做详细分析。

此时argv指针所指向的完整字符串是：`-Xzygote /system/bin --zygote --start-system-server`

在解析之前会有一个预检查：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e631510e46c~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

不难发现，在经过这个预检查后，i作为字符串指针，所代表的参数变成了：`/system/bin --zygote --start-system-server`

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e6317ede04a~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png) ![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e6331be0d41~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

-   /system/bin是没有用的
-   zygote = true
-   startSystemServer = true
-   niceName = "zygote64"

下面就为真正启动runtime开始筹备新的args了：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e6332aabb9b~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

在组装args后，真正发起了调用：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e63350bd400~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

其中**setArgv0**是为当前线程（fork出来的进程的默认线程）设置名称（pthread\_setname\_np）。

当前args中包含的内容只有两个：

-   start-system-server
-   \--abi-list=arm64-v8a （或armeabi-v7a armeabi）

> 关于app\_process还有什么场景使用，后续会拓展衍生篇

## AppRuntime

AppRuntime这个类存在于app\_main.cpp中，它其实本身很薄。它的父类**AndroidRuntime**是背后的集大成者，AppRuntime更多的是针对于其父类所暴露的一些生命周期虚函数做了必要的实现，比如：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e635144f3f5~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

完整的类图：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e63525bbfb5~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

其父类的详细代码可以在线查看：[AndroidRuntime](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fmaster%3Aframeworks%2Fbase%2Fcore%2Fjni%2Finclude%2Fandroid_runtime%2FAndroidRuntime.h%3Fq%3DAndroidRuntime.h%26ss%3Dandroid%252Fplatform%252Fsuperproject "https://cs.android.com/android/platform/superproject/+/master:frameworks/base/core/jni/include/android_runtime/AndroidRuntime.h?q=AndroidRuntime.h&ss=android%2Fplatform%2Fsuperproject")

### 构造

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e63528d6a65~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

mClass是jclass类型。这里做了NULL初始化，其他传入了父类：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e635939b944~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

> zygote并不是通过init所启动的第一个service、第一个进程。通过源码可以看到，init.rc中early-init第一个启动其实是ueventd（实际上还是init），甚至在之后的late-init环节中，binder相关启动时机比zygote还要早

### start

再回头看一下调用start的参数是什么：

`runtime.start("com.android.internal.os.ZygoteInit", args, zygote);`

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e637577952b~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

-   className = com.android.internal.os.ZygoteInit
-   options（同上边的args）
    -   start-system-server
    -   \--abi-list=arm64-v8a （或armeabi-v7a armeabi）
-   zygote = true

AndroidRuntime的start过程略长，分四部分看：

#### 1\. 启动ART虚拟机

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e635c8eefb4~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

##### JniInvocation

jni\_invocation.Init(NULL)内部去加载了**libart.so**（通过dlopen），并将操作句柄保存在了**JniInvocation**的 **handler\_** 中：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e6375d3d6b6~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

handle\_作为后续做调用dlsym的参数使用。同时还对ART虚拟机做了三个灵魂拷问：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e6376de3c54~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

1.  **JNI\_GetDefaultJavaVMInitArgs** 你可不可以获取默认JVM初始化参数？（从源码上看，这部分是没有内容的，返回了-1，但方法存在就叫做支持）
2.  **JNI\_CreateJavaVM** 你能不能创建JVM？
3.  **JNI\_GetCreatedJavaVMs** 能不能把刚刚创建的JVM给我？

这三个方法都存在，那么说明这是一个合格的**libart.so**。

##### startVm()

此方法内部是大量的配置参数组装过程，目标是为了创建一个JVM出来，也就是实践上一步的第二个问题。zygote参数影响的是JVM的JDWP配置，其他两个就是透过ART Runtime赋值的了：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e637e814729~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

> 这里需要临时插入另一个问题，JniInvocation作为另一个so——libnativehelper.so中的方法，为什么在AndroidRuntime.cpp中可以直接使用？并没有见到先dlopen它的地方。这个可以通过frameworks/base/core/jni/Android.bp找到答案，AndroidRuntime.cpp就属于这里。在这个（Makefile）文件中，可以发现，libnativehelper是作为其shared\_libs使用的，所以可以直接调用。

-   JniInvocation.cpp#JNI\_CreateJavaVM()
    -   JniInvication::JNI\_CreateJavaVM()
        -   JniInvication::JNI\_CreateJavaVM\_函数指针（指向ART虚拟机的JNI\_CreateJavaVM方法）
            -   java\_vm\_ext.cc#[JNI\_CreateJavaVM](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fmaster%3Aart%2Fruntime%2Fjni%2Fjava_vm_ext.cc%3Bl%3D1188%3Bbpv%3D0%3Bbpt%3D1 "https://cs.android.com/android/platform/superproject/+/master:art/runtime/jni/java_vm_ext.cc;l=1188;bpv=0;bpt=1")

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e63947118de~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

此时，虚拟机的实例有了，用于JNI调用的Env也有了。

##### onVmCreated()

AppRuntime复写了父类的虚函数：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e63985a6d4e~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

zygote的case下，这里是不会做任何事情的。

至此，VM创建成功，也有了基础的JNI调用环境。第一步结束。

#### 2\. Android JNI

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e639cc911fc~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

startReg的实现：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e639f2565b6~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

gRegJNI是非常大体量的JNI注册集合体：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e63b3535983~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

> 以我的源码数下来，总共有155个注册JNI的方法，都是与Android系统各个API息息相关的内容

以RuntimeInit为例：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e63b4b56165~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

startReg中所传入的env参数会被作为入参传入到register\*方法中，而最终会触发**env->RegisterNatives**方法（涉及虚拟机的部分不再深入了）。

而关于LocalFrame，官方解释如下：

> Every "register" function calls one or more things that return a local reference (e.g. FindClass). Because we haven't really started the VM yet, they're all getting stored in the base frame and never released. Use Push/Pop to manage the storage.

之前我们启动了VM，只是我们没有在这个VM中做真正的方法调用。每一个register函数都会通过env执行起来，而他们都会产生本地引用，如果不加操作，那么这些本地引用不会被自动销毁，造成无用数据的累积，所以这里使用LocalFrame收集他们，并最后释放。

总结：这一步就是将众多Android系统相关功能的JNI注册到JVM中去，便于后续Java环境中进行调用。

#### 3\. 参数组装

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e63b701c608~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

strArray会用来存储所组装出来的参数，如图中注解所写，参数的内容会是：

-   com.android.internal.os.ZygoteInit
-   start-system-server
-   \--abi-list=arm64-v8a

#### 4\. 调用main

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2019/12/26/16f41e63b9f88382~tplv-t2oaga2asx-jj-mark:3024:0:0:0:q75.png)

如官方注释所说，当前所在的这个线程将会是即将到达的Java世界的主线程。

当前线程的特点：

-   C++程序
-   app\_process64 可执行程序
-   相对于Linux内核空间，当前处于用户空间
-   是被用户空间第一个app——init（pid=1）启动起来的

在发起JVM调用前，结合上边的各种梳理，各种参数为：

-   startClass = com/android/internal/os/ZygoteInit
-   startMeth 是这样的main方法
-   （strArray在上一小节）

接下来，随着env->CallStaticVoidMethod()的调用，启动流程进入到以ZygoteInit为起点的Java领域。

## Next

`ZygoteInit.java`