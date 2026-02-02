

一、安卓系统启动的init.rc流程解析

      上一篇实际没有写完，本章我继续写一下一些源码细节。

1. init.rc解析

源码中解析的位置如下：

这里是调用的地方

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoXlzth5I42aL4AEs2Gh3wPReDOcomo8fjvruw4q6CFo6LTSl2LUqQz9eMuqlRp2Yo5aYPSH0bW8Ug/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=0)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoXlzth5I42aL4AEs2Gh3wPRZ5ZuC43DKdd4dGHic1sGWNk9WC2HLeHrssH8wIbIKZZpbJ1XYcvVkxA/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=1)

ActionManager是 Android init 的全局单例类（整个系统只有一个实例），核心职责是管理所有从 RC 文件中解析出的Action（动作），比如：

on init：系统初始化时执行的动作；

on boot：系统启动完成时执行的动作；

on property:ro.bootmode=recovery：特定属性触发的动作。

ServiceList同样是全局单例类，核心职责是管理所有从 RC 文件中解析出的Service（系统服务），比如：

zygote（应用进程孵化器）；

surfaceflinger（图形显示服务）；

vold（存储管理服务）；

它是 Android 系统服务的 “全局注册表”，后续 init 启动、重启、管理服务时，都从这个单例中查找对应的 Service 配置。

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoXlzth5I42aL4AEs2Gh3wPR2ibbia4ahwHyibbtSPiavoAOQMVarrreU18P1b0eow69BBpVfEvSPeq2Zw/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=2)

启动顺序如下：

这里我们在源码中看到亦是如此，只不过最新的AOSP中这块儿加了很多东西，可以细看。

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoXlzth5I42aL4AEs2Gh3wPRCug0bCfm9QLoe5uwT9Sgvr7z1br2kbib3XsbToBaX9icwFmn5hWVEb9A/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=3)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoXlzth5I42aL4AEs2Gh3wPRXhFiccMZPtFtTSdIz1fjTj7hjqQ6mDe5Cd8t2WkcicbWnHcP5aTyfSaw/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=4)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoXlzth5I42aL4AEs2Gh3wPRZJn2OECibbmW87QpoYE426k47aAEPSXUicYQeXsUIh0t0nibT437kHloQ/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=5)

类似的，我们还看到了这样的代码片段。其实init.rc就相当于Linux shell指令的集合，之所以能够运行完全也是相当于挨个执行shell语句。https://blog.csdn.net/hinewcc/article/details/155242651 这篇博客对init.rc做了详细介绍。但是这些博客始终介绍了很多理论知识，详细的看，这些initrc指令的核心执行逻辑在如下这个文件中，底层逻辑还是通过Linux的 exec函数去执行。

  

如下这个文件中的这段代码实际就是执行这种类似shell命令的函数。

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoXlzth5I42aL4AEs2Gh3wPR6hic6GpRRypXt9SMJJs7tdwHOmhZyXiandHYIiaAoIvTjict1DaiaFW3Kxw/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=6)

从如下这个代码中也可以看出，赋值权限的就是lchown这个Linux函数。

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoXlzth5I42aL4AEs2Gh3wPRVF8QWMZwbbibNIbNnLqBqqnbX3y6HScukqRddudv7R3Mmic0BwaDEQlw/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=7)

这个exec实际最终追踪下来再这里，还是离不开execv这种Linux的基本函数。

  

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoXlzth5I42aL4AEs2Gh3wPR6WP3iaakDen430ZrwKevibXxibWEicd0WRbTVrYZ1K9lgxZsiaQBxdJAqJw/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=8)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoXlzth5I42aL4AEs2Gh3wPR1B3xncyO0lVicp1mDXrm6TnfGkicqy8uLDePju6J1jzWUv6AZAoo1mdQ/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=9)

至此，安卓的启动过程基本算是了解了大概，无非就是Linux的机制加入了一些定制化的需求，可以看到，init.rc为什么使用类似Linux命令行的方式由系统调用执行，这其实就全部算到用户进程这块了。用户进程调用Linux系统调用执行也理所应当。

下一篇文章我会再去补充一下zygto的流程，今后的很多篇博客都计划采用这种方式，开篇先延生下安卓的启动流程，然后接着开始总结安卓的其他核心知识点。

        2. 源码阅读

       上面几篇博客提到的有个查看安卓源码的网址最近似乎挂了，也没有大神给出新的服务器网站，因此我还是决定下载源码，考虑到全部下载太庞大，因此在网上搜到了AOSP16单个 版本的源码，通过某网盘下载，当然建议开个会员，速度快。这里附上地址，感谢这位开源大佬https://zwc365.com/2020/09/13/ubuntu18-build-android10-11。下载之后使用Android Studio打开即可，当然最好配置好jdk，这样后面跳转都很方便，关于AS的使用我这里不再赘述，也可以选用vistual studio code 或者source insight等工具。如果磁盘空间剩余很多，建议可以多搜一些方式，从国内的清华大学镜像下载全量代码，这样可以包各个版本。https://unicom.mirrors.ustc.edu.cn/help/aosp.html, 参考这个链接，中科大的源应该还可以。

二、binder

      为什么直接开始学习binder，因为binder对于安卓的重要性及其高，安卓中的几乎所有跨进程调用全部由它完成，且binder实际是一套很复杂的机制，在各种性能问题中，binder也高频出现。不过，值得注意的是，binder实际存在一个线程池中，所以binder的资源会耗尽。有些binder耗时长达1s的，往往与binder驱动、还有binder自身业务有关系，这些往往都会导致noio的问题阻塞，有时候也可能与系统内存有关，也可能与上层业务相互阻塞有关。

       binder的学习路程也比较艰难，我这里借鉴下gityuan这位博主的图做一个简单开篇：

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoXlzth5I42aL4AEs2Gh3wPRKLwrDJDWs6J7Yg20lRAQsia7lNnboysr7shoWGPAlfic76zKxczvpl1Q/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=10)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoXlzth5I42aL4AEs2Gh3wPRneIfRzOk7MDG2gRQrfZGVHnBRdmibaJ8ofs2LbOibSzrtophwYpBebng/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=11)

我们可以看到，binder存在于安卓各个层次，从framework到JNI，再到native，再到kernel。上层framework层的Binder逻辑是建立在Native层架构基础之上的，核心逻辑都是交予Native层方法来处理。

        我们依旧从上层开始学习起来：

1. 上层如何使用binder方法

上层 APP 不会直接操作内核态的 Binder 驱动，而是通过 Android Framework 层封装的android.os包下的 API 来使用 Binder，这些 API 封装了底层的跨进程通信（IPC）逻辑。

     上层 APP 接触的 Binder 相关方法主要分为四类：IBinder 接口核心方法（通信基础）、Binder 类扩展方法（服务端身份管理）、ServiceManager 相关方法（获取系统服务）、AIDL 封装方法（上层常用），以下逐一说明：

（1）IBinder 接口核心方法（所有 Binder 对象都实现此接口）

IBinder 是 Binder 通信的基础接口，所有 Binder 对象（包括客户端代理、服务端实现）都必须实现它，核心方法如下：

  

transact(int code, Parcel data, Parcel reply, int flags) 客户端向远程进程发送跨进程请求，传递参数并接收响应（AIDL 自动封装此方法） code是请求标识（对应 AIDL 方法编号），data是序列化的请求参数，reply是接收响应的容器，flags是通信模式（0 = 双向、1 = 单向）；底层将数据打包成 Binder 协议包，通过 Binder 驱动发送到目标进程，触发服务端onTransact。

  

onTransact(int code, Parcel data, Parcel reply, int flags) 服务端重写此方法，解析客户端请求、执行业务逻辑、返回结果 Binder 驱动将客户端的transact请求转发到服务端进程，服务端 Binder 线程池调用此方法；方法内根据code解析data参数，执行对应逻辑后将结果写入reply，返回false会让客户端抛出RemoteException（可用于权限校验）。

pingBinder() 快速检查目标 Binder 对象所在进程是否存活，避免调用已死亡的 Binder 发送轻量级 ping 请求到目标进程，Binder 驱动仅验证进程存活状态，无业务数据传输，目标进程返回确认则返回true，否则false。

  

linkToDeath(IBinder.DeathRecipient recipient, int flags) 注册 Binder 死亡监听，当目标进程崩溃 / 被杀时触发binderDied()回调 Binder 驱动维护 Binder 对象的引用计数和死亡通知列表，进程死亡时，驱动遍历注册的DeathRecipient，主动触发回调（上层可实现服务重连）。

unlinkToDeath(IBinder.DeathRecipient recipient, int flags) 取消死亡监听，避免内存泄漏（如 Activity 销毁时调用） 从 Binder 驱动的死亡通知列表中移除该DeathRecipient，停止接收死亡回调。

（2）Binder 类（服务端 Binder 基类，继承 IBinder）

Binder 类是服务端实现 Binder 的基类，扩展了身份管理相关方法，仅服务端可调用：

  

getCallingPid() 获取调用方的进程 ID（PID），用于初步身份校验 / 日志记录 Binder 驱动在跨进程请求中携带调用方 PID，方法从 Binder 调用上下文提取。

getCallingUid() 获取调用方的用户 ID（UID），用于可靠身份验证（UID 是 APP 唯一标识） Binder 驱动在请求中携带调用方 UID（关联 APP 包名 / 签名），比 PID 更安全（PID 可复用）。

  

clearCallingIdentity() 服务端临时切换到自身 UID/PID 执行操作，避免使用调用方低权限 保存当前调用方身份（返回 token），将 Binder 上下文的 UID/PID 切换为服务端自身标识。

restoreCallingIdentity(long token) 恢复调用方身份，保证后续权限检查正确 通过clearCallingIdentity返回的 token，还原 Binder 上下文的 UID/PID。

（3）ServiceManager 相关方法（获取系统服务）

ServiceManager 是 Android 核心服务，维护所有系统服务的 Binder 注册表，上层 APP 通过它获取系统服务的 Binder 引用：

  

ServiceManager.getService(String name) 获取系统服务的 Binder 代理对象（如ActivityManager、WindowManager） 向 ServiceManager 发送跨进程查询请求，ServiceManager 从注册表中返回对应服务的 Binder 代理对象，底层通过 Binder 驱动完成通信。

  

ServiceManager.addService(String name, IBinder service, boolean allowIsolated) 注册自定义服务到 ServiceManager（上层 APP 极少用，主要是系统服务） 向 ServiceManager 发送注册请求，将服务名称与 Binder 对象的映射存入注册表，供其他进程查询。

（4）AIDL 封装的方法（上层最常用）

你定义的 AIDL 接口方法（如add(int a, int b)）会被自动封装为可直接调用的方法，无需手动处理transact和序列化，是上层 APP 实现 IPC 的主流方式：

用途：直接调用跨进程的业务方法，无需关注底层 Binder 通信细节。

原理：AIDL 工具自动生成代码，将业务方法调用转换为transact请求（自动分配code、序列化参数到Parcel）；服务端自动重写onTransact，解析参数并调用实际业务方法，再将结果序列化返回。

  

上述是通过豆包获取的知识，我想还是积累下比较好，免得后续记不清。

事实上，我们在使用context中的startService接口的过程中，这个接口虽然自身不是binder接口，但是执行过程却完全依赖binder过程。

具体流程如下：

A[APP进程调用startService] --> B[Context封装启动参数（Intent）]

    B --> C[通过ActivityThread获取AMS的Binder代理对象]

    C --> D[调用IBinder.transact()发送跨进程请求]

    D --> E[Binder驱动转发请求到system_server进程的AMS]

    E --> F[AMS处理请求：检查权限、创建/启动Service]

    F --> G[AMS通过Binder回调APP进程的Service.onCreate/onStartCommand]

  

startService这个方法作用非常大，主要用于拉起服务，当一个进程由很多服务构成的时候，第一个启动的服务就是这个进程的进城名，但是所有的服务也仅仅算是一种超级线程而已，其中可以继续创建thread。

  

实际上，这里有一个很好的实例来展示binder的基本原理 https://blog.csdn.net/weixin_52527621/article/details/155187622 ，我相信这对从0到1学习如何创建binder是有好处的。但是安卓为了降低开发门槛，搞了AIDL接口这种结构，使得可以使用AS机制生成代码，方便调用，实际还是onTransact的实现。

  

  

  

  

2. 从上层开始看到的binder调用在perfetto中的体现

     正如我在我的之前的文章中提到的，perfetto trace实际记载了整个关键调用点的运转记录。这里我们看到WeChat的小程序Cold pooll自身在运行的时候就有binder调用，这个binder调用不是很长，只有几百微秒，注意，当你看到binder调用长达几百ms或者几十ms，一般经验而言，这可能存在问题，比如binder阻塞，高负载等。

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoVtcUiadMksBmodxTrf6TjOiaLVeicdGxfCmiahiaJcMNPDbYWjdvpw6Hd92V8wKyb88V867al4R5vIM2g/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=12)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoVtcUiadMksBmodxTrf6TjOiagODicOa0etzvSy23EwBYhWYmACDhv5EX3e1Eqtiao2Gq64AqKiaDic0icDg/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=13)

这个binder调用到systemserver层面实际的堆栈是一个AIDL接口，关于这个接口我简单搜索了下整个binder调用如下图所示。

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoVtcUiadMksBmodxTrf6TjOiaTNicnFwMcjWmQg4KEfrmBxPQ9vsuVic7ib01AksiaYx2offJSK1tMuR2Sg/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=14)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoVtcUiadMksBmodxTrf6TjOiawn7rNFWMNqnbiawXyViaOibYia8kYh93bMkmaUN0bOJRh901BL6f7oHMRQ/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=15)

这里我不得不提到，豆包、deepseek这些大模型帮了我们大忙，但是AI会让我们自以为很博学，其实我们应该比他们更资深才对，而且很多大模型的解析不一定正确，需要甄别。但是我们也要拥抱时代变化，很多程序员都有非要写代码的执念，实际人的精力有限，写代码没问题，但是业务意味着你没有那么多代码可写，而且AI有时候写的比人还好，所以要拥抱一些AI工具，最重要的是人的发散思维，这是AI所没有的。好了，废话不多说，我们继续本篇文章的旅程。

  

  

3. 一个binder通信的实例

     通常很多进程都是包含一些service服务的，但是当这个进程不存在的时候，我们要用这个服务的时候可以直接调用bindservice这个方法，进程不存在的时候首先创建进程，然后拉起服务，整个过程也是充满了binder调用的逻辑，主要有如下步骤：

     阶段 1：客户端发起bindService()，向 AMS 发送跨进程请求：

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoVtcUiadMksBmodxTrf6TjOiau2OnfxA0vpQMxzQY6uyhBMwBMqWUB1utf7RN98RvKficicQBIciasvwQQ/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=16)

阶段 2：AMS 校验与进程创建决策（system_server 进程内）

AMS 收到客户端的绑定请求后，先执行一系列校验，再判断是否需要创建进程，核心步骤：

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoVtcUiadMksBmodxTrf6TjOiaSVL7Efuic6NDbsZttfSlmGXcAbiaILa8AgNKYpJg9Y4RR8icW4Z0zvQLg/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=17)

阶段 3：Zygote 孵化目标服务进程（Zygote -> 目标进程）：zygtofork进程之后执行如下步骤：

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoVtcUiadMksBmodxTrf6TjOia11O89aDRIkJ8kmOzecBIQa848Odbh3buOTIs5aECLL2gTC6nnMX1pA/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=18)

阶段 4：目标进程初始化与 Service 创建（目标进程内）

  

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoVtcUiadMksBmodxTrf6TjOiamuzxUcibyFkRydDVP6OicEf9eibib29aicaBntFuROc9LWCgCGz3A4t8DGQ/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=19)

阶段 5：Binder 绑定通道建立与回调（AMS -> 客户端进程）

  

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoVtcUiadMksBmodxTrf6TjOiaN2s6cPhDTctA8kN2qoZy5kIZPHNRmhdkcQ6IGBHpYq3W81AKV7qBQw/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=20)

整个过程还是充满了各种binder调用，基本都是AMS和客户端以及服务端和systemserver的交互。注意，binder在上层是个线程池，往往位于systemserver中。

  

4. 关于AIDL

gityuan在他的博客中也着重介绍了AIDL，这是链接，https://gityuan.com/2015/11/23/binder-aidl/  实际的核心也就是代理 CS、parcel数据封装以及AIDL接口。

自定义binder在native和framewor层通信的实例在这个链接https://gityuan.com/2015/11/22/binder-use/。 他们的核心基本与此类似。

  

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoVtcUiadMksBmodxTrf6TjOiaiar2NaLY6GtwpbXKVIq2EM7XJ82HJYPhC3eURiaBJgBTgnzibyL0xO3Ug/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=21)

  

  

  

  

5. binder在framework层的框架

  

       参考gityuan大佬的博客，主要是如下的源码，但是他当时的AOSP源码比较久，最新的可能路径有所差异，但是应该差不多。

这里我们只作为参考，实际涉及到的地方我会补充一些。

binder在framework层，采用JNI技术来调用native(C/C++)层的binder架构为上层应用提供服务。对于java层使用CS架构实现了一套IPC通信架构。

  

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoVtcUiadMksBmodxTrf6TjOiaEWwqh9qmfSiafMe9svkQUQnyjcjVw9iauIntlMDicdtSU9VzD3zUKkOdg/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=22)

核心过程有：

（1）初始化时候注册Binder服务。虚拟机启动时候注册服务，核心是 REG_JNI(register_android_os_Binder)，这个过程建立了Binder类在Native层与framework层之间的相互调用的基础。如下所示的源码就是注册服务时候看到的。

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pq0diaST2JuNMvl79C6bIWF0GiaQIh9yYAEy2e9yaiaSZt05J2pnA5l0oQ/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=23)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pXXh1w4tlQRibCCGqxzVrzz8KfVgRfAVMJez0r5EgrL9XJocboyPibCSA/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=24)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pntLNz1j4BwXhsZibufT3XYpwapicib3EiakJ63bWiakwDaiaFfvyicDSdbLzg/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=25)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6p6riaOCE8LTVuhoU7F4zYKhIWRuia0yvaicD5ED1DVwQSqpRCw6AmH1w4A/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=26)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6psVTPLQ0T07edeR4sLjsvJPs44MwfELpkDiau037dKZxgCic9QPr5zWYg/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=27)

register_android_os_Binder的实现在如下文件中  

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pQFBDGniaxQjOqSJSAPJD7fKxyoSRj76Az3zGMrjLedGT0EmqlxlrfHA/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=28)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6p9DtYCgrT1xibuMJlcl7N2Nv41Qg0FPh7K21dXwr7LmI8WAegiaZT6LVQ/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=29)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6p7PhA9FWjNhFqzxjKMX13hp7Od3dL529fnsyTjAqmvz5n1ELbzvPXCQ/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=30)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pq9CCDkMaSDX18icIadCAa5aBTV93pbIXdIQHRYa5zWp5jeia6WnkY1IQ/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=31)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pMicjxibv7kX13zvytPf7BE0s1z1budTlxpEyxCT9zKnSGlD0xrx6ANkQ/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=32)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pq2ovlWckHcSRkR4LCPf1g9o3WQNRC0CqwhpViaNDibrYKc6oZl849h3g/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=33)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pRDnXosz7GodUM6aQZM2Zyibe5NNNM9ppkY0XZPotasXgHqoxtmNNUFg/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=34)

android_os_BinderInternal_handleGc() 是 JNI 静态函数，核心作用是在 Java GC 时更新 Binder 引用统计快照，无实际回收逻辑，仅用于监控和排查泄漏。函数核心逻辑是将「Binder 本地引用总数 + 死亡通知引用总数」赋值给快照变量 gCollectedAtRefs，三个变量均为全局静态统计变量。真正的 Binder 引用回收由 JVM GC（Java 层）和 libbinder 库 / Binder 驱动（Native / 驱动层）完成，该函数仅提供统计辅助。该函数是 Android Binder 内存管理的组成部分，其统计数据是排查 Binder 引用泄漏的依据。

这个注册过程是BinderInternal类在Native层与framework层之间的相互调用的桥梁。

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pZWo79xd6ibvrbHbMR4TjEHQiaqqjceyCcsuKdwqG8WNBicDENJcJ0Gw1Q/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=35)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pBEkzSOYeZZcib8I5TrBfdHvQs6kfRmT1XibecAakZVnwVcLOMNIoNzibQ/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=36)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pmXd8wc5kicia2TvqBkRRP7VLr9wdoGZMAECyu7gAntqoq8u0icKyfvLNA/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=37)

（2）注册服务  
addService方法，这是系统方法，仅仅系统服务进程可以调用，非 SystemServer 进程若要调用 addService()，必须满足两个条件：  
进程 UID 属于系统白名单（如 android.uid.system、android.uid.root、android.uid.phone 等系统预留 UID）；  
应用 / 进程的 APK 必须使用 Android 系统签名。

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pq3ibCWyRvwVCvgFZ36AnsyEMELSdSBWY7dY8dzl1ibRZEAJzzHIEsn4w/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=38)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6p3BDIicCvJdUfsrrOubhotdKGiaBpCRgal4ygGtoUr1pnVHcgOKz4zoiag/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=39)

getIServiceManager实际还是获取native层面的服务。所以我们在ServiceManagerNative中寻找addservice的底层定义

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pYmJ0CXthVaaqeWibrSvKTYe55dEibm6CPjM5anRQy70wqVZTanagpqBw/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=40)

mServiceManager 是 Binder 代理对象，指向 Native 层 BpServiceManager注意，这个需要编译aidl文件，源码中实际没有。

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pN8v51pMdBfYLftFypw6dY9E6smyms3JmaYiccHYicWX3yfAs1TCSQU6w/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=41)

mUnifiedServiceManager 主要出现在 Android Framework 的 Java 层，用于兼容旧版 ServiceManager 和新版 ServiceManagerV2

java层的接口

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6p8uknliclSsh9zibulZJxzrnhR8SntaWibRGPGIDChrHJp3CmSbRuYTxCA/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=42)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6peAlQHibgNoawAVGJKzjR1GAYs67ibD0lzhCJUnqicFF5YZaTj49155Z9Q/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=43)

AIDL 工具生成的BpServiceManager（客户端代理）会通过remote()->transact发起跨进程调用  
  
BnServiceManager 和BpServiceManager 不是在framework 源码里的， 是aidl工具编译的时候生成的  
frameworks\native\libs\binder\aidl\android\os\IServiceManager.aidl

  

BpBinder作为客户端 Binder 代理的核心，会将调用参数封装并交给IPCThreadState处理

  

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pfHAKpqejucsK4KhcpvlogqAd76IhxiaakEkXWWHH3icH0f7rMSLpiaKgA/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=44)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pcP4ia8bzSjJbnYHuknMHgqPYMBDkIEDPYKzTHiaD3TYUHux239XoW18Q/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=45)

SM的Binder句柄固定为0，驱动会根据handle转发数据到SM进程

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pGKb6IOHU10KZ0JiaNoGr8yWt4uwEQNlISDQuibJ4YcCz1SEbVDDMSanw/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=46)

到这一层已经从上层addservice调用快接近驱动了。

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pC1JpyzE2JqIGNUkrORFL6u3Nwq4EvsYlnordg08fWXZzibo2NdlBBVw/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=47)

在这调用驱动接口

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pD6ajp4UmkevDEhlRO9CCfLkHh1ibw3uaEwoIibfcozpluicdjrYv6vzibw/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=48)

  

紧接着，收到回来的消息，

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6paDyicvLoaS65XxPa3ia2Fjph8YmFcUtLO2wlK2ePmy48UTS0hicROuiaWw/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=49)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6p6iapYDf9tic8kiadaFV6T5ypaxFxu9TKia6tAFlVIC6fY56OU4JP6aMtRw/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=50)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pYQcyJF0TzX6Cxd4iarbmNTNZfNQmIQN4sbAWHk5iccb7TibXM3u4h0zug/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=51)

  

  

参考链接https://blog.csdn.net/weixin_42065195/article/details/156681124

  

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/XQxmCObufoWnAyNEjSaAd9wEtJ7Y1W6pHBFvAsOJsF0Avh00ZmhcMu5SVulGMaQXndMOXpIFIeEs2JT57qSjWA/640?wx_fmt=jpeg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=52)

查询服务注册表，返回Binder对象，这里对应的返回的流程。

当前暂时先就源码流程探究到这里，这个过程是framework层的binder过程。