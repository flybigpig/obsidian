##      PackageManagerService启动详解系列博客概要

Android PackageManagerService系列博客目录:

> [PKMS启动详解系列博客概要](https://blog.csdn.net/tkwxty/article/details/113050319)  
> [PKMS启动详解(一)之整体流程分析](https://blog.csdn.net/tkwxty/article/details/113137052)  
> [PKMS启动详解(二)之怎么通过packages.xml对已安装应用信息进行持久化管理?](https://blog.csdn.net/tkwxty/article/details/113340177)  
> [PKMS启动详解(三)之BOOT\_PROGRESS\_PMS\_START流程分析](https://blog.csdn.net/tkwxty/article/details/113607061)  
> [PKMS启动详解(四)之Android包信息体和包解析器(上)](https://blog.csdn.net/tkwxty/article/details/114137504)  
> [PKMS启动详解(五)之Android包信息体和包解析器(中)](https://blog.csdn.net/tkwxty/article/details/114538497)  
> [PKMS启动详解(六)之Android包信息体和包解析器(下)](https://blog.csdn.net/tkwxty/article/details/114835060)  
> [PKMS启动详解(七)之BOOT\_PROGRESS\_PMS\_SYSTEM\_SCAN\_START阶段流程分析](https://blog.csdn.net/tkwxty/article/details/114964303)  
> [PKMS启动详解(八)之BOOT\_PROGRESS\_PMS\_DATA\_SCAN\_START阶段流程分析](https://blog.csdn.net/tkwxty/article/details/115185220)

___

  

### 引言

  一直在筹划着写一个系列的博客关于PKMS服务启动流程详解的(在本篇以及后续的一系列博客中为了简述统一将PackageManagerService简写为PKMS)，但是一直自我感觉功力不够，不是因为本人不够自信，真的是事出有因啊！PKMS的启动涉及到非常多的逻辑，其启动流程是比较复杂的！这里的启动流程复杂并不是说它涉及的原理有多么多么的深奥或者晦涩，而是PKMS作为Android系统中核心服务之一，它管理着所有跟Package相关的工作，比如应用的扫描，安装，卸载等相关调度，其中涉及到了非常多的数据结构之间的关联和转换，这是PKMS学习的难点但这也是PKMS服务的精髓所在。所以本着宁缺毋滥的原则，我筹备良久，希望能将这个系列博客写得至善至美。

虽然本人为此系列博客做了很多的准备，说句心里话写本篇系列博客还是心里非常忐忑，不为别的主要担心自己功力不够，怕写出的东西有纰漏或者错误之处，但是还是硬着头皮往下干了！至少，我是在用心写着！当然如果能帮到小伙们那是最开心不过的了！

> 在这里我不得不给读者打一个预防针，那就是学习PKMS一定要打起十二分精神，做好打持久战的准备。
> 
> 因为我依稀记得我最初开始真的想深入PKMS源码学习的时候，由于没有下定狠决心，都想速成，所以每次都是匆匆一扫而过，最后发现什么也没有捞着，所以心态和坚持很重要，也许源码看一篇你还不理解，没有关系，那就二遍，直到你有所顿悟(不是遁入空门,本篇博客的重点不是在这！)

注意：本篇的介绍是基于Android 7.xx平台为基础的。

___

### 一.PKMS启动系列博客的整体概括

  对于分析源码类型的博客，我觉得重点不是咔咔的一上来就是一通源码分析，而是要站在设计者的角度出发，带领读者先从整理流程和方向上把握，然后各个突破，循序渐进(可是不幸的是，网上绝大部分博客的套路就是一上来就是贴代码，而忽略了整体的概述和引导)。本人非常欣赏无意中看到的一篇微信号的文章"我为什么这么讨厌你一行行地讲代码"？,其中最后一段话说的非常在理和实际，那就是:

> 每次，我看到这样的技术文章也是无比的蛋疼。你一上给我一行行讲代码是什么意思？难道我看不懂C语言？你能不能高屋建瓴地先给我讲一讲你要写什么东西？你写的东西整体框架是什么？至于代码，你给我点几个关键点、函数名或者流程图，我自己去撸行吗？  
> 记流水账的文章为什么对读者的帮助一般都不大？因为它真地是事倍功半。我花了很长地时间去读你，读完还是不知所云，因为全部是细节，因为我没形成整体的轮廓。

是的，这种博客看着写了非常多的东西，但是读者读起来感觉啥都有，但是又啥都没有！所以在本系列博客中，我们将抛弃一行行分析代码，着重重点的脉络，对于细枝末节如果读者感兴趣可以在将整体知识点理清楚的基础上再慢慢深入，遵循读书先将书变薄，再变厚的理论而发散进行！

  

#### 1.1 PKMS启动系列博客提纲

在正式开始PKMS启动系列博客漫长而又复杂的启动流程分析之前，让我们先对该系列博客的目录来高屋建瓴的整体概括一下，那就是会遵循PKMS启动的流程采取总分总的写作方法，先整体后细节的方法。所以这里我们有必要先来了解一下PKMS启动的整体流程，如下：

___

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c58d6b8f61326186bd6d6bbb24afbe86.png#pic_center)

___

对于上述PKMS启动的流程各个阶段，我们可以通过adb进行查看，如下:

```yaml
XXX:/ # logcat  -b events  | grep pms
01-25 20:00:20.453  4509  4509 I boot_progress_pms_start: 27556
01-25 20:00:20.962  4509  4509 I boot_progress_pms_system_scan_start: 28065
01-25 20:00:22.058  4509  4509 I boot_progress_pms_data_scan_start: 29161
01-25 20:00:22.362  4509  4509 I boot_progress_pms_scan_end: 29465
01-25 20:00:23.291  4509  4509 I boot_progress_pms_ready: 30395
```

所以在接下来的系列博客中，我们将依照上面的提纲出发采取总分总的结构，通过伪代码结合重点方法或者函数的分析来展开PKMS启动流程的讲解！带领小伙们能在整体上对PKMS启动流程有一个脉络清晰的理解，至于其中涉及到的各种小细节就靠各位小伙们自行去撸了！

  

#### 1.2 PKMS启动系列博客要解决的问题

对于PKMS启动系列博客的分析，在正式开始之前，我们先来几个灵魂拷问，看你是否能接住:

> 1.为什么Android设备终端关机的时候手机是砖头，而开机后，所有App都可以运行了，这是为什么呢？
> 
> 2、Android系统是通过什么手段或者技术来加载手机上的App(这其中包括手机系统自带的也包括后续安装的)
> 
> 3、对于上述App会以什么形式或者说数据结构形式被加载，然后PKMS对其进行管理?
> 
> 4、上述App加载完成之后，按照科学的架构设计，是不是应该存在一个管理者，来全局管理，那个这个类是什么？
> 
> 5、App应用的四大组件，是怎么注册到系统的

对于上述几个灵魂拷问，回答不上来没有关系，通过PKMS启动系列博客我相信你会找到答案的。

___

### 二.PKMS启动系列博客知识储备

PKMS启动过程中，涉及到的许多概念，需要我们提前熟悉熟悉,磨刀不误砍柴工吗！所以在这里我有必要提前来简单介绍一下。

  

#### 2.1 App安装涉及的目录

##### 2.1.1 App安装目录:

-   常见的系统App安装目录

> 1、 /system/app: Android系统自带的应用App路径，也即Android系统App路径  
> 2、/system/priv-app: 同上  
> 3、/vendor/app: odm或者oem厂商预制系统App目录  
> 4、/vendor/priva-app: 同上

-   普通应用App安装目录

> 1、 /data/app：用户App程序安装的目录。安装时Apk会被拷贝至此目录

##### 2.1.2 dex、odex保存目录:

> /data/dalvik-cache：App安装的时候会将apk中的dex文件安装到dalvik-cache目录下(dex文件是dalvik虚拟机的可执行文件,当然，ART–Android Runtime的可执行文件格式为oat，启用ART时，系统会执行dex文件转换至oat文件)

##### 2.1.3 用户数据目录:

> /data/data：存放应用程序的数据,无论是系统App还是普通App,App产生的用户数据都存放在/data/data/包名/目录下。

##### 2.1.4 App注册表目录

> /data/system ：该目录下几个比较重要的文件是:
> 
> 1、packages.xml文件，类似于Windows的注册表。这个文件是在解析apk时由writeLP()创建的，里面记录了系统的permissions，以及每个apk的name,codePath,flags,ts,version,uesrid等信息，这些信息主要通apk的AndroidManifest.xml解析获取，解析完apk后将更新信息写入这个文件并保存到flash，下次开机直接从里面读取相关信息添加到内存相关列表中。当有apk升级，安装或删除时会更新这个文件。
> 
> 这个文件在PKMS的启动过程中会被映射成为PKMS中的Settings数据结构,并且在后续的PKMS启动中会围绕着该数据结构进行频繁的操作。

  

#### 2.2 App是怎么被Android系统所接纳认可(即被安装的)

Android的App为Android的生态系统注入了各种可能，那么一个App真正被Android系统接纳和认可的流程是什么呢(通俗的来讲就是App是怎么被Android到Android系统中，并被整个Android系统认可的)?在人类社会中，小孩可以通过出生证明被认可，成大了可以以身份证来表明(经典的怎么证明我是我)。那么App要被Android系统认可要通过什么来表明呢？App的安装也是类似的,也是通过一定的管理机制进行的。App的安装不仅仅需要将Apk的实体放到系统的特定目录(/data/app/),然后创建好对应的数据目录(/data/data)，而且需要向PKMS注册包名、以及apk声明的四大组件(Activity、Service、ContentProvider、BroadcastReceiver),该App才能算是成功安装到了Android系统。

如果觉得上述文字描述有点啰嗦的话，那么我们再精简一点就是分为如下两个方面:

> 1、安装Apk到特定的目录，并且生成对应的数据结构  
> 2、注册包名App等信息、以及相关的四大组件到PKMS中

只有完成了第2步(向系统注册)，Android系统才知道有这么一个App存在,才可以管理（或者说启动）该App，这样改App才被融于到了Android系统中成为合法的公民。

  

#### 2.3 App升级的方式

在Android终端中通常意义上将App分为两种，第一种是第三方安装App，另外一种就是系统App,而它们二者的升级方式也存在一点差异，我们分别来介绍一下.

> 了解上述的两种模式，对后续PKMS启动中扫描App，然后处理升级策略和处理策略非常重要，所以非常有必要了解清楚！

##### 2.3.1 第三方App升级方式

第三发App升级比较简单，通常是采取应用内自检测升级的方式进行，这种升级方式是比较常见的。

##### 2.3.2 系统App升级方式

Android的系统App存在两种比较常见的方式，如下:

-   通过覆盖安装的方式:

> 譬如我们的Android终端，系统内置了内置了(注意措辞，是内置了)国民应用微信，然后等微信检测到有高版本的微信，则会进行覆盖安装升级，将微信App的安装到/data/app分区，然后禁用系统内的微信

-   通过系统OTA升级的方式:

> 譬如我们的Android终端，系统内置了内置了(注意措辞，是内置了)国民应用微信，然后Android终端通过OTA更新固件版本，并且最新的固件版本中升级了微信版本，此时则是通过OTA升级的方式升级了系统App应用

  

#### 2.4 Apk的拆分机制

Android L(5.0)以及以后，支持APK拆分，即一个APK可以分割成很多部分，位于相同的目录下，每一个部分都是一个单独的APK文件，所有的APK文件具备相同的签名，在APK解析过程中，会将拆分的APK重新组合成内存中的一个Package。对于一个完整的APK，Android称其为Monolithic；对于拆分后的APK，Android称其为Cluster。关于此处可以详见博客[Apk Splits](http://tools.android.com/tech-docs/new-build-system/user-guide/apk-splits)机制。

> 在Android L(5.0)以前，APK文件都是直接位于app或priv-app目录下，譬如计算器APK的目录就是/system/app/Calculator.apk；到了Android L(5.0)之后，多了一级目录结构，譬如计算器APK的目录是/system/app/Calculator/Calculator.apk，这是Android为了支持APK拆分而做的改动，如果要将计算器APP进行拆分，那所有被拆出来的APK都位于/system/app/Calculator/即可，这样在包解析时，就会变成以Cluster的方式解析目录。

  

#### 2.5 PKMS在启动过程中的职责

我们知道PKMS服务是Android包管理的核心服务，它在整个Android系统中有三大职责:

-   在Android终端启动过程中扫描App安装目录

> 1、开机后扫描应用安装目录和系统App目录，解析其中的apk文件将相关信息加载到PKMS中的数据结构中，同时对于没有对应数据目录的App生成对应的数据目录
> 
> 2、注册包名App等信息、以及相关的四大组件到PKMS中
> 
> 3、将解析到的数据同步到/data/system/packages.xml中

-   负责App的安装和卸载

> 负责第三方App的安装和卸载(这个是一个非常漫长的过程，后续会由专门博客分析)

-   对外提供服务查询App信息

> 对外提供接口，供第三方查询Android终端相关安装包信息(而实现方式主要是通过解析Intent意图,通过Intent获取到目的Activity、Provider、BroadCastReceiver 或Service。

而我们的PKMS启动系列博客将会重点要分析的就是PKMS的扫描阶段的职责，所以我们后续的分析中会重点关注在第一点上面，弱水三千只取一瓢！

___

### 小结

  没有想到，PKMS启动的前期知识准备就写了这么多了(这其中还有最最重要的包管理机制牵涉到数据结构还没有放出来，这个会在后续的博客中分析)，为了小伙们阅读的方便和排版的美观详解一就先到这里了，如果想真的了解PKMS启动的源码级别的小伙们一定要做好前期准备不然在阅读中你会越来越迷茫和无助，读着读着就有可能放弃了，因为你会发现咋这么难啊，左一个知识点，有一个知识点的。不要问我为啥这么说，因为我也是这么过来的，也迷惘和无助过心里想这啥玩意啊！所以小伙们一定要做好心理准备，千万不能半途而废！在接下来的篇章中我们就要正式的开干进入实战分析了，希望小伙们能继续关注。欢迎继续关注[PackageManagerService启动详解(一)之整体流程分析](https://blog.csdn.net/tkwxty/article/details/113137052)！