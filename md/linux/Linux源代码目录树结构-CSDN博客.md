Linux用来支持各种体系结构的源代码包含大约4500个C语言程序，存放在270个左右的子目录下，总共大约包含200万行代码，大概占用58MB磁盘空间。 

源代码所有在目录：/usr/src/linux (大部分linux发行版本中) 

![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) init 内核初始化代码   
![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) kernel 内核核心部分：进程、定时、程序执行、信号、模块。。。   
![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif)  mm [内存](https://so.csdn.net/so/search?q=%E5%86%85%E5%AD%98&spm=1001.2101.3001.7020)处理   
![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif)  arch 平台相关代码   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) i386 IBM的PC[体系结构](https://so.csdn.net/so/search?q=%E4%BD%93%E7%B3%BB%E7%BB%93%E6%9E%84&spm=1001.2101.3001.7020)   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) kernel 内核核心部分   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) mm 内存管理   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) math-emu 浮点单元软件仿真   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) lib 硬件相关工具函数   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) boot 引导程序   
         ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) compressed 压缩内核处理   
         ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) tools 生成压缩内核映像的程序   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) alpha 康柏的Alpha体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) s390 IBM的System/390体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) sparc Sun的SPARC体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) sparc64 Sun的Ultra-SPARC体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) mips SGI的MIPS体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) ppc Motorola-IBM的基于PowerPC的体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) m68k Motorola的基于MC680x0的体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) arm 基于ARM处理器的体系结构   
![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif)  fs 文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) proc /proc虚拟文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) devpts /dev/pts虚拟文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) ext2 Linux本地的Ext2文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) isofs ISO9660文件系统（CD-ROM）   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) nfs 网络文件系统（NFS）   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) nfsd 集成的网络文件系统服务器   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) fat 基于FAT的文件系统的通用代码   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) msdos 微软的MS-DOS文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) vfat 微软的Windows文件系统（VFAT）   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) nls 本地语言支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) ntfs 微软的Windows NT文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) smbfs 微软的Windows服务器消息块（SMB）文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) umsdos UMSDOS文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) minix MINIX文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) hpfs IBM的OS/2文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) sysv SystemV、SCO、Xenix、Coherent和Version7文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) ncpfs Novell的Netware核心协议（NCP0   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) ufs UnixBSD、SunOs、FreeBSD、NetBSD、OpenBSD和NeXTStep文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) affs Amiga的快速文件系统（FFS）   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) coda Coda网络文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) hfs 苹果的Macintosh文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) adfs Acorn磁盘填充文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) efs SGI IRIX的EFS文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) qnx4 QNX4 OS使用不的文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) romfs 只读小文件系统   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) autofs 目录自动装载程序的支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) lockd 远程文件锁定的支持   
![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif)  Net 网络代码   
![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif)  Ipc System V的进程间通信   
![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif)  Drivers 设备驱动程序   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) block 块设备驱动程序   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) paride 从并口访问IDE设备的支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) scsi SCSI设备驱动程序   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) char 字符设备驱动程序   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) joystick 游戏杆   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) ftape 磁带流设备   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) hfmodem 无线电设备   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) ip2 IntelliPort的多端口串行控制器   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) net 网卡设备   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) sound 音频卡设备   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) video 视频卡设备   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) cdrom 专用CD-ROM设备（除ATAPI和SCSI之外）   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) isd0n ISDN设备   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) apl000 富士的AP1000设备   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) macintosh 苹果的Macintosh设备   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) sgi SGI的设备   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) fc4 光纤设备   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) acorn Acorn的设备   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) misc 杂项设备   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) pnp 即插即用的支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) usb 通用串行总线（USB）的支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) pci PCI总线的支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) sbus Sun的SPARC SBus的支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) nubus 苹果的Macintosh Nubus的支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) zorro Amiga的Zorro总线的支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) dio 惠普的HP300 DIO总线的支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) tc Sun的TurboChannel支持（尚未完成）   
![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif)  Lib 通用内核函数   
![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif)  Include 头文件（.h）   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) linux 内核核心部分   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) lockd 远程文件加锁   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) nfsd 集成的网络文件服务器   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) sunrpc Sun的远程过程调用   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) byteorder 字节交换函数   
      ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) modules 模块支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) asm-generic 平台无关低级头文件   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) asm-i386 IBM的PC体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) asm-alpha 康柏的Alpha体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) asm-mips SGI的MIPS体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) asm-m68k Motorola-IBM的基于PowerPC的体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) asm-ppc Motorola-IBM的PowerPC体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) asm-s390 IBM的System/390体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) asm-sparc Sun的SPARC体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) asm-sparc64 Sun的Ultra-SPARC体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) asm-arm 基于ARM处理器的体系结构   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) net 网络   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) scsi SCSI支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) video 视频卡支持   
   ![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif) config 定义内核配置的宏所在的头文件   
![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif)  scripts 生成内核映像的外部程序   
![](https://i-blog.csdnimg.cn/blog_migrate/ca99f97cad6d776f06c387e27132625d.gif)  Documentation有关内核各个部分的通用解释和注释的文本文件