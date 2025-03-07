## kernel启动流程概要

最新推荐文章于 2024-12-09 17:41:23 发布

![](https://csdnimg.cn/release/blogv2/dist/pc/img/reprint.png)

[快乐安卓](https://blog.csdn.net/yangwen123 "快乐安卓") ![](https://csdnimg.cn/release/blogv2/dist/pc/img/newCurrentTime2.png) 于 2012-11-07 11:04:54 发布

一：内核Image的组成

1 ES（Embed System）启动的时候，CPU加电，执行的第一条语句是Bootloader，这个非常类似PC机上的BIOS。BL将内核加载后，控制器移交给 LK

2 LK执行的第一条语句是什么？vmlinux是单体的内核表示。根据前面说的内核编译连接知识，第一条语句是head.S中（历史原因，MD，有很多文件都叫 head.S）

 我们需要重新分析一下内核(这里就是zImage了)的组成，（方法很简单，研究make的执行过程，通过make V=1 zImage可以得到几乎全部信息）

-   vmlinux，这个是未压缩、未strip的内核模块，ELF结构
-   Image：二进制、未压缩、但是strip后的内核
-   head.o：ARM相关的，由BL将控制权转交给它。即前面提到的head.S生成
-   pigg.gz：Image文件的gzip压缩
-   piggy.o：由piggy.S生成，这个S文件通过include Bin方式将Image包含进来。piggy的意思就是背负、肩扛。很形象不是？
-   misc.o：从上面看，涉及到一些解压方面的内容，而misc提供一些辅助函数
-   vmlinux：悲催.....这个文件是head+pigg+misc构成的vmlinux。名字一样不是？真的很混淆！
-   zImage：再由上面这个vmlinux压缩而来

图1很好得展示了这个过程。

![[Pasted image 20250218094245.png]]

图1 内核的构成

3 piggy的故事

piggy.S很有意思，建立了一个section，并且有一个标志来指示piggy.gz的边界。

piggy对应的是一个叫bootstrap的image，注意，Bootstrap和Bootloader不一样，它是在BL之后的一段代码，用来

解压kernel，设置内存等作用。也可以叫second stage boot。

4 Bootloadre和BootstrapLoader

BL和BSL的区别是什么？

-   BL只是初始化硬件，不依赖linux，不处理linux
    
-   BSL在BL后执行，依赖linux，因为要解压linux。另外一个重要点就是BSL需要为LINUX的运行建立环境
    

BSL的工作包括：

-   head.O：初始化CPU等工作
    
-   misc.O：解压，重定位（例如将kernel移动到另外一个位置上） decompress\_kernel
    
-   其他工作
    

init/main.c:start\_kernel

启动调用图见图2.

![[Pasted image 20250218094344.png]]

图2 启动调用流程图

下面来分析这个启动流程

1 kernel中的head.o分析：尽量保持CPU系列的通用，例如arm的CPU等初始化都在做。但是具体板子（例如CPU+其他硬件）怎么初始化？这就是由mach目录中的初始化函数做到的。所以，kernel初始化分为：generic CPU初始化+具体板子的初始化。head.o初始化后，跳转到main.o的start\_kernel，继续后面的流程

2 start\_kernel:(init/main.c)：start\_kernel的转移由head.O做的，不过代码一般包含在更通用的head\_common.S中

   以后想做kernel的分析，就从main开始吧. start\_kernel做了什么事情呢？

-   刚才只是初始化了cpu相关的，而具体和板子相关的由start\_arch执行
    

3 kernel 参数分析：kernel command line。注意，这个参数是由BL传递给kernel的，不过这个参数又是谁设置的呢？又存在什么地方呢？这个line放在一个global的地方，

  另外，kernel如何处理这些参数呢？有一个比较好的办法，\_\_set\_up宏，将一些参数和对应的函数指针存在一个特殊的section中，然后循环调用这个section中的函数。（和驱动module中的很像）。定义在init.h中。关于一些特殊参数的取值，在arch/arm/kernel/vmlinux.lds.S中定义。（以后得去看看ld的manual了）\_\_set\_up这个宏还有一个flags比如early，表示处理阶段是否在early-stage做。标志有\_\_init的section最终占用的内存会被抛弃..

4 子系统初始化：包括中断、等。？section嵌套section？

5 kernel\_init进程：start\_kernel最后会fork一个kernel\_init进程，而原执行进程变成idle进程了..

6 用户空间的init进程:由kernel\_init进程最终通过execve init完成

7 参考文献。ELP这本书给的参考文献都巨强..