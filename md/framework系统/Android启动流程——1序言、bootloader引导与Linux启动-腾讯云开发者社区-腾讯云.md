前面讲解的很多内容都很抽象，所以本次系列决定"接点地气"，准备开始讲解大家熟悉的Activity了，为了让我以及大家更好的理解Activity，我决定本系列的课程主要分为4大流程和2大模块。

4大流程如下：

-   1、bootloader与Linux启动
-   2、init进程
-   3、zygote进程
-   4、systemServer启动

在某个流程内部我又会分为

-   1、理论知识：比如这个类的作用，他的父类是什么，设立理论部分的主要目的是让我们更好地理解它的设计思想
-   2、方法跟踪：从方法这个级别一级一级的跟踪，追踪溯源，看到谷歌团队到底是如何设计的。

Android系统的启动，主要是指Android手机关机后，长按电源键后，Android手机开机的过程。从系统角度看，Android的启动程序可分为：

-   1、bootloader引导
-   2、装载与启动Linux内核
-   3、启动Android系统
    -   3.1、启动Init进程
    -   3.1、启动Zygote
    -   3.1、启动SystemService
    -   3.1、启动Launcher

整体流程大致如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/n292msdgz.png)

启动.png

本片文章的内容如下：

-   1、Bootloader启动
-   2、Linux系统启动

### 一、Bootloader启动

#### (一) 概述

> 开机，开机就是给系统开始供电，此时硬件电路会产生一个确定的复位时序，保证CPU是最后一个被复位的器件，为什么CPU要最后被复位呢？因为，如果CPU第一个被复位，则当CPU复位后开始运行时，其他硬件内部的寄存器状态可能还没有准备好，比如磁盘或者内存，那么久可能出现外围硬件初始化错误。当正确完成复位后，CPU开始执行第一条指令，该指令所在的内存你地址是固定的，这由CPU的制造者指定。不同的CPU可能会从不同的地址获取指令，但这个地址必须是固定的，这个固定地址所保存的程序往往被称为"引导程序(BootLoader)"，因为其作用是装载真正的用户程序。

至于如何装载，则是一个策略问题，不同的CPU会提供不同的装载方式，比如有的是通过普通的并口存储器，有的则通过SD卡，还有的还是通过RS232接口。无论硬件上使用何种接口装载，装载过程必须提供以下信息，具体包括：

-   1 从哪里读取用户程序
-   2 用户程序的长度是什么
-   3 装载完用户程序后，应该跳转到哪里，即用户程序的执行入口在哪里？

不同硬件系统会采用不同的策略，但只要以上三个信息是确定的，用户程序就会被装载到确定的地址，并执行相同的操作。

#### (二)、Bootloader的定义和种类

简单地说，BootLoader是在操作系统运行之前运行的一段程序，它可以将系统的软硬件环境带到一个合适的状态，为运行操作系统做好准备，这样描述是比较抽象的，但是它的任务确实不多，终极目标就是把操作系统拉起来运行。

> 在嵌入式系统世界里存在各种各样的BootLoader，种类划分也有多种方式。除了按照处理器体系结构不同划分以外，还有功能复杂程度的不同。 先来区分一下Bootloader和Monitor：

-   Bootloader只是引导操作系统运行起来的代码
-   Monitor另外还提供很多命令接口，可以进行调试、读写内存、配置环境变量等。 在开始过程中Monitor提供了很好的调试功能，不过在开始结束之后，可以将其设置成一个Bootloader。所以习惯上将其叫做Bootloader。

| Bootloader | Monitor？ | 描述                               | X86 | ARM | PowerPC |
| ---------- | -------- | -------------------------------- | --- | --- | ------- |
| U-boot     | 是        | 通用引导程序                           | 是   | 是   | 是       |
| ReBoot     | 是        | 是基于eCos的引导程序                     | 是   | 是   | 是       |
| BLOB       | 否        | (StrongARM架构) LART(主板)等硬件平台的引导程序 | 否   | 是   | 否       |
| LILO       | 否        | Linux磁盘引导程序                      | 是   | 否   | 否       |
| GRUB       | 否        | GNU的LILO替代程序                     | 是   | 否   | 否       |
| Loadlin    | 否        | 从DOS引导Linux                      | 是   | 否   | 否       |
| Vivi       | 是        | 韩国mizi公司开发的bootloader            | 否   | 是   | 否       |

更过的bootloader还有：ROLO、Etherboot、ARMboot、LinuxBIOS等。对于每种体系结构，都有一些列开放源码Bootloader可以选用：

-   x86：x86的工作站和[服务器](https://cloud.tencent.com/product/cvm/?from_column=20065&from=20065)上一般使用LILO和GRUB
-   ARM：最早由为ARM720处理器开发板所做的固件，又有armboot，StrongARM平台的blob，还有S3C2410处理器开发板上的vivi等。现在armboot已并入U-Boot，所以U-Boot也支持ARM/XSALE平台。U-Boot已经成为ARM平台事实上的标准Bootloader
-   PowerPC：最早使用于ppcboot，不过现在大多数直接使用U-boot。
-   MIPS：最早都是MIPS开发商自己写的bootloader，不过现在U-boot也支持MIPS架构。
-   M68K：Redboot能够支持m68k系列的系统。





#### (三)、ARM

因为目前Android系统多运行在ARM处理器上，因此，下面主要分析运行于ARM处理器上的启动过程。在介绍之前，我先抛砖引玉，大家想一下，怎么分区：ARM、处理器、CPU?

OK，我们一起来看下

-   ARM本身是一个公司的名称，从技术的角度来看，它又是一种微处理器内核的架构。
-   处理器一种统称，可以指具体的CPU芯片，比如intel i7处理器，苹果的A11处理器等。处理器内部一般包含CPU、片上内存、片上外设接口等不同的硬件逻辑。 CPU是处理器内部的中央处理单元的缩写，CPU可以按照类型分为短指令集架构和长指令集架构两大类，ARM属于短指令集架构的一种

#### (四)、ARM特定平台的BootLoader

对于ARM处理器，当复位完毕后，处理器首先执行其片上ROM中的一小块程序。这块ROM的大小一般只有几KB，该段程序就是Bootloader程序，这段程序执行时会根据处理器上一些特定的引脚的高低电平状态，选择从何种物理接口上装载用户程序，比如UBS接口、串口、SD卡、并口Flash等。

多数基于ARM的实际硬件系统，会从并口NAND Flash 芯片中的 0x00000000地址处装载程序。对于一些小型嵌入式系统而言，该地址中的程序就是最终要执行的用户程序；对于Android而言，该地址中的程序还不是Android程序，而是一个叫做uboot或者fastboot的程序，其作用就是初始化硬件设备，比如网口、SDRAM、RS232等，并提供一些调试功能，比如像NAND Flash写入新的数据，这可用于开发过程中的内核烧写、等级等。

PS：

> 当uboot(fastboot)被装载后便开始运行，它一般会先检测用户是否按下某些特别按键，这些特别按键是uboot在编译时预先被约定好的，用于进入调试模式。如果用户没有按这些特别的按键，则uboot会从NAND Flash中装载Linux内核，装载的地址是在编译uboot时预先约定好的。

我们看下上电之后到U-boot的流程

![](https://ask.qcloudimg.com/http-save/yehe-2957818/fjrqtcjsqx.png)

上电流程.png

#### (三)、U-boot启动流程分析

最常用的bootloader还是U-boot，可以引导多种操作系统，支持多种架构的CPU。它支持的操作系统有：Linux、NetBSD、LynxOS等，支持的CPU架构有：ARM、PowerPC、MISP、X86、NIOS等。

手机系统不像其他的嵌入式系统，它还需要在启动的过程中关心CP的启动，这个时候就涉及到CP的image和唤醒时刻，而一般的嵌入式系统的uboot只负责引导OS内核。所以这里我们也暂不关心CP的启动，而主要关心AP。

而U-boot的启动过程大致上可以分为两个阶段：

-   第一阶段：汇编代码 U-boot的第一条指令从cpu/armXXX/start.S文件开始
-   第二阶段：C代码 从文件/lib\_arm/board.c的start\_armboot()函数开始。

关于这块详细资料，我也不是很熟悉，就不误人子弟了，大家可以自行查询

相关资料如下：

[bootloader](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fblog.csdn.net%25252Fcuijianzhongswust%25252Farticle%25252Fdetails%25252F6612624&objectId=1199503&objectType=1&isNewArticle=undefined)

### 二、Linux系统启动

Linux系统的启动过程由很多阶段组成，但是无论你是启动标准的x86桌面，还是启动嵌入式PowerPC目标，许多流程都是惊人的相似。从初始化引导到第一个用户空间来研究Linux启动进程。关于Linux系统启动主要分为三个阶段，第一个阶段是自解压过程，第二个是设置ARM处理器的工作模式、设置一级页表等，第三个阶段主要是C代码，包括Android的初始化的全部工作。

#### (一) 自解压过程

> 内核压缩和解压缩代码都在目录kernel/arch/boot/compressed，编译完成后将产生head.o、misc.o、piggy.gzip.o、vmlinux、decompress.o这几个文件。

-   head.o：是内核的头部文件，负责初始设置
-   misc.o：将主要负责内核的解压工作，它在head.o之后
-   piggy.gzip.o：是一个中间文件，其实是一个压缩的内核(kernel/vmlinux)，只不过没有和初始化文件及解压缩文件链接而已；
-   vmlinux：是没有(zImage是压缩过的内核)压缩过的内核，就是由piggy.gzip.o、head.o、misc.o组成的
-   decompress.o是未支持更多的压缩格式而新引入的。

BootLoader完成系统的引导以后并将Linux内核调入内核之后，调用do\_bootm\_linux()，这个函数将跳转到kernel的其实位置。如果kernel没有被压缩，就可以启动了。如果kernel被压缩过，就要进行压缩了，在压缩过的kernel头部有解压缩程序。压缩过的kernel入口第一个文件源位置正在arch/arm/boot/compressed/head.S。它将调用函数decompress\_kernel()函数，这个函数在文件的arch/arm/boot/compressed/misc.c中，decompress\_kernel()又调用proc\_decomp\_setup()，arch\_decomp\_setup()进行设置，然后打印gunzip()将内核放于指定的位置。

> 下面简单介绍一下解压缩的过程，也就是函数decompress\_kernel的实现功能：解压缩的代码位置与kernel/lib/inflate.c，inflate.c是从gzip源程序中分离出来的，包含一些对全局数据的直接引用，在使用时需要直接嵌入到代码中。gzip压缩文件时总是在前32K字节的解压缩缓冲区，它定义为windowWSIZE。inflate.c使用get\_byte()读取输入文件，它被定义成 宏 来提高效率。输入缓冲区指针必须定位inptr，inflate.c中对之有减量操作。inflate.c调用flush\_window()来输出window缓冲区的解压出的字节串，每次输出长度用outcnt变量表示。在flushwindow()中，还必须对输出字节串计算CRC并且刷新crc变量。在调用gunzip()开始解压之前，调用makecrc()初始化CRC计算表。最后gunzip()返回0表示解压成功。

#### (二) Linux初始化

Linux初始化又分为三个阶段

##### 第一阶段

> 本阶段就是上面说的到的内核解压缩完成后的阶段。

该部分的代码实现在arch/arm/kernel的 head.S中，该文件的汇编代码通过查找处理内和类型的机器码类型调用相应的初始化函数，再建立页表，最后跳转到start\_kernel()函数开始内核的初始化工作。检查处理器是汇编子函数\_\_lookup\_processor\_type中完成的，通过以下代码可实现对它的调用：bl\_\_lookup\_processor\_type(在文件head-commom.S实现)。\_\_lookup\_processor\_type调用结束返回原程序时，会将返回结果保存到寄存器中。其中r5寄存器返回一个描述处理器的结构体地址，并对r5进行判断，如果r5的值为0说明不支持这种处理器，将进入\_error\_p。r8保存了页表的标志位，r9保存了处理器的ID号，r10保存了与处理相关的struct proc\_info\_list。Head.S核心代码如下：

```
ENTRY(stext)
setmode PSR_F_BIT | PSR_I_BIT | SVC_MODE, r9 @设置SVC模式关中断
      mrc p15, 0, r9, c0, c0        @ 获得处理器ID，存入r9寄存器
      bl    __lookup_processor_type        @ 返回值r5=procinfo r9=cpuid
      movs      r10, r5                       
 THUMB( it eq )        @ force fixup-able long branch encoding
      beq __error_p                   @如果返回值r5=0，则不支持当前处理器'
      bl    __lookup_machine_type         @ 调用函数，返回值r5=machinfo
      movs      r8, r5            @ 如果返回值r5=0，则不支持当前机器（开发板）
THUMB( it   eq )             @ force fixup-able long branch encoding
      beq __error_a                   @ 机器码不匹配，转__error_a并打印错误信息
      bl    __vet_atags
#ifdef CONFIG_SMP_ON_UP    @ 如果是多核处理器进行相应设置
      bl    __fixup_smp
#endif
      bl    __create_page_tables  @最后开始创建页表
```

检查机器码类型是汇编函数\_\_lookup\_machine\_type中完成，与\_\_lookup\_processor\_type类似，通过代码“\_\_lookup\_machine\_type”来实现对它的调用。该函数返回时，会将返回结构保存在r5、r6和r7三个寄存器中，其中r5寄存器返回一个用来描述机器的机构体地址，并对r5进行判断，如果r5为0，则说明不支持这种机器，将进入\_\_error\_a。r6保存了I/O的页表偏移地址。当检测处理器类型和机器码类型结束后，将调用\_\_create\_page\_tables子函数来建立页表，它所要做的工作就是将RAM地址开始的1M空间物理地址映射到0xC0000000开始的虚拟地址处。对本项目的开发板DM3730而言，RAM挂接到物理地址0x80000000处，当调用\_\_create\_page\_tables结束后0x80000000 ～ 0x80100000物理地址将映射到0xC0000000～0xC0100000虚拟地址处。当所有的初始化结束之后，使用如下代码来跳到C程序的入口函数start\_kernel()处，开始之后的内核初始化共工作

##### 第二阶段

> 从start\_kernel函数开始

Linux内核启动的第一个阶段是从start\_kernel函数开始的。start\_kernel是所有Linux平台进入系统内核初始化后的入口函数，它主要完成剩余与硬件平台的相关初始化工作，在进行一些系列的与内核相关的初始后，调用第一个用户进程——init进程并等待用户进程的执行，这样整个Linux内核便启动完毕。该函数位于init/main.c文件中。

如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/dfdpvv2fjx.png)

main.c.png

这个函数内部的具体工作如下：

-   调用setup\_arch()函数进行与体系结构相关的第一个初始化工作；对于不同的体系结构来说该函数有不同的定义。对于ARM平台而言，该函数定义在arch/arm/kernel/setup.c。它首先通过检测出来的处理器类型进行处理其内核的初始化，然后通过bootmem\_init()函数根据系统定义的meminfo结构进行内存结构的初始化，最后调用 paging\_init()开启MMU，创建内核页表，映射所有的物理内存和IO空间。
-   创建异常向量表和初始化中断处理函数
-   初始化系统核心进程调度器和时钟中断处理机制
-   初始化串口控制台
-   创建初始化系统cache，为各种内存调用机制提供缓存，包括动态内存分配，虚拟文件系统(VirtuaFile System)及页缓存。
-   初始化内存管理，检测内存大小及被内核占用的内存情况。
-   初始化系统的进程间通信机制(IPC)；当以上所有的初始化工作结束后，start\_kernel()函数会调用rest\_init()函数来进行最后的初始化，包括创建系统的第一个进程——init进程来结束内核的启动

Linux内核启动的最后一个阶段就是挂载根文件系统，因为启动第一个init进程，必须以根文件系统为载体。

##### 第三阶段

根文件系统至少包括以下目录：

-   /etc/：存储重要的配置文件
-   /bin/：存储常用且开机时必须用到的执行文件。
-   /sbin/：存储着开机过程中所需的系统执行文件。
-   /lib/：存储/bin/及/sbin/的执行文件所需要的链接库，以及Linux的内核模块
-   /dev/：存储设备文件

上面五大目录必须存储在跟文件系统上，缺一不可。

###### 1、为什么以只读的方式

以只读的方式挂载根文件系统，之所以采用只读的方式挂载根文件系统是因为：此时Linux内核仍在启动阶段，还不是很稳定，如果采用可读可写的方式挂载跟文件系统，万一Linux不小心宕机了，一来可能破坏根文件系统上的数据，再者Linux下次开机时得花上很长时间来检查并修复根文件系统。

###### 2、挂载根文件系统的目的：

有两个：

-   安装适当的内核模块，以便驱动某些硬件设备或启动某些功能
-   启动存储于文件系统中的init服务，以便让init服务接手后续的启动工作。

###### 3、执行init服务的顺序：

Linxu内核启动的最后一个动作，就是从根文件系统上找出并执行init服务。Linux内核会依照下列的顺序寻找init服务：

-   第一步检查 /sbin/是否有init服务
-   第二步检查/etc/是否有init服务
-   第三步检查/bin/是否有init服务
-   如果都找不到你最后执行/bin/sh

找到init服务后，Linux会让init服务负责后续初始化系统使用环境的工作，init启动后，就代表系统已经顺利地启动了Linux内核。启动init服务时，init服务会读取/etc/inittab文件，根据/etc/inittab中的设置数据进行初始化系统环境工作。

###### 4、/etc/inittab：

/ect/inittabl定义init服务在Linux启动过程中必须执行以下几个脚本：

-   /etc/rc.d/rc.sysinit 主要功能是设置系统的基本环境，当init服务执行rc.sysinit时，要依次完成下面一系列工作：
    -   启动udev
    -   设置内核参数：执行sysctl -p，以便从/etc/sysctl.conf设置内核参数
    -   设置系统时间：将硬件时间设置为系统时间
    -   启动交换内存空间：执行swpaon -a -e，以便根据、etc/fstab的设置启动所有的交互内存空间。
    -   检查并挂载所有文件系统：检查所有需要挂载的文件系统，以确保这些文件系统的完整性。检查完毕后可读可写的方式挂载文件系统。
    -   初始化硬件设备：Linux除了在启动内核时以静态驱动部分的硬件外，在执行rc.sysinit时，也会试着驱动剩余的硬件设备。
    -   初始化串行端口设备：Init服务会管理所有的串行端口设备，比如调制解调器、不断电系统、串行端口控制台等。Init服务则通过rc.sysinit来初始化Linux串行端口设备。当rc.sysinit发现Linux才能在这/etc/rc.serial时，才会执行/etc/rc.serial，借以初始化所有的串行端口设备。因此，你可以在/etc/rc.serial中定义如何初始化Linux所有的串行端口设备。
    -   清除过程的锁定文件与IPC文件
    -   建立用户接口
    -   建立虚拟控制台
-   /etc/rc.d/rc
-   /etc/rc.d/rc.local

这里简单说一下建立虚拟控制台

init会在若干个虚拟控制台中执行/bin/login，以便用户可以从虚拟控制台登录Linux。Linux默认在前6个虚拟控制台，也就tty1~tty6，执行/bin/login登录程序。当所有的初始化工作结束后，cpu-idle()函数会被调用来使用系统处于闲置(idle)状态并等待用户程序的执行。至此，整个Linux内核启动完毕

最后赠送一个整体的启动流程图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/h3qt291f9d.png)

image.png

下一篇文章[Android系统启动——2 init进程](https://cloud.tencent.com/developer/article/1199503?from_column=20421&from=20421)

### 官人飞吻，你都把臣妾从头看到尾了，喜欢就点个赞呗(眉眼)！！！！

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2018.03.04 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除