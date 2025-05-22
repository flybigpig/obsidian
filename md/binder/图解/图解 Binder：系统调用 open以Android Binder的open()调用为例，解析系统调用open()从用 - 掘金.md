---
created: 2025-05-22T14:55:45 (UTC +08:00)
tags: [Android,Linux中文技术社区,前端开发社区,前端技术交流,前端框架教程,JavaScript 学习资源,CSS 技巧与最佳实践,HTML5 最新动态,前端工程师职业发展,开源前端项目,前端技术趋势]
source: https://juejin.cn/post/7204751926515449911
author: 满嘴跑火车的小土匪
---

# 图解 Binder：系统调用 open以Android Binder的open()调用为例，解析系统调用open()从用 - 掘金

> ## Excerpt
> 以Android Binder的open()调用为例，解析系统调用open()从用户空间到内核空间的流程。本文主要分为三个部分：1.用户空间的相关调用 2.虚拟文件系统 3.内核空间的相关调用

---
> 这是一系列的 Binder 文章，会从内核层到 Framework 层，再到 Java 层，深入浅出，介绍整个 Binder 的设计。详见《[图解 Binder：概述](https://juejin.cn/post/7244018340880007226 "https://juejin.cn/post/7244018340880007226")》。
> 
> 本文基于 Android platform 分支 android-13.0.0\_r1 和内核分支 common-android13-5.15 解析。
> 
> 一些关键代码的链接，可能会因为源码的变动，发生位置偏移、丢失等现象。可以搜索函数名，重新进行定位。

本文以Android Binder的open()调用为例，解析系统调用open()从用户空间到内核空间的流程。

Binder的open()的调用代码如下：

```
open("/dev/binder", O_RDWR | O_CLOEXEC);
```

open函数可分为用户空间调用、内核空间调用两部分。内核空间的相关调用，又涉及了Linux的虚拟文件系统VFS这个概念。大致的流程如下：

![](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/4be77b50588a4eba8d6529882ed9f7ff~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

所以本文主要分为三个部分：

-   用户空间的相关调用
-   虚拟文件系统
-   内核空间的相关调用

## 用户空间的相关调用

用户空间的相关调用如下：

-   [open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fbionic%2Fopen.cpp%3Bl%3D56%3Bbpv%3D1%3Bbpt%3D1 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/bionic/open.cpp;l=56;bpv=1;bpt=1")
    -   [\_\_openat()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fbionic%2Fopen.cpp%3Bl%3D37%3Bdrc%3Df42a611faa92d6bff1dfeade9b4e9775e7d15b9f%3Bbpv%3D1%3Bbpt%3D1 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/bionic/open.cpp;l=37;drc=f42a611faa92d6bff1dfeade9b4e9775e7d15b9f;bpv=1;bpt=1")

`__openat()`即[openat()](https://link.juejin.cn/?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman3%2Fopenat.3p.html "https://man7.org/linux/man-pages/man3/openat.3p.html")，它实际是一个汇编函数，是由[gensyscalls.py](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Ftools%2Fgensyscalls.py%3Bl%3D430 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/tools/gensyscalls.py;l=430")根据SYSCALLS.TXT里记录的信息，生成相关代码。

[SYSCALLS.TXT](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2FSYSCALLS.TXT%3Bl%3D147 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/SYSCALLS.TXT;l=147")：

```
int __openat:openat(int, const char*, int, mode_t) all
```

[gensyscalls.py](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Ftools%2Fgensyscalls.py%3Bl%3D430 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/tools/gensyscalls.py;l=430")根据上述信息，将会根据不同的cpu架构，生成不同的汇编代码，比如：

arm32

```
ENTRY(__openat)
    mov     ip, r7
    .cfi_register r7, ip
    ldr     r7, =__NR_openat
    swi     #0
    mov     r7, ip
    .cfi_restore r7
    cmn     r0, #(MAX_ERRNO + 1)
    bxls    lr
    neg     r0, r0
    b       __set_errno_internal
END(__openat)
```

arm64

```
ENTRY(__openat)
    mov x8, __NR_openat
    svc #0

    cmn x0, #(MAX_ERRNO + 1)
    cneg x0, x0, hi
    b.hi __set_errno_internal

    ret
END(__openat)
```

`__NR_openat`是一个系统调用号。`swi`或`svc`会触发软中断，进入内核态，并根据系统调用号，从[系统调用表](https://link.juejin.cn/?target=https%3A%2F%2Fbaike.baidu.com%2Fitem%2F%25E7%25B3%25BB%25E7%25BB%259F%25E8%25B0%2583%25E7%2594%25A8%25E8%25A1%25A8%2F23611131 "https://baike.baidu.com/item/%E7%B3%BB%E7%BB%9F%E8%B0%83%E7%94%A8%E8%A1%A8/23611131")中，定位到对应的函数地址，完成系统调用。

何为软中断？来自[@贝贝先生](https://link.juejin.cn/?target=https%3A%2F%2Fwww.zhihu.com%2Fpeople%2Fzhang-bei-bei-70-85 "https://www.zhihu.com/people/zhang-bei-bei-70-85")的一段解释：

> 中断在本质上是软件或者硬件发生了某种情形而通知处理器的行为，处理器进而停止正在运行的指令流，去转去执行预定义的中断处理程序。软件中断也就是通知内核的机制的泛化名称，目的是促使系统切换到内核态去执行异常处理程序。这里的异常处理程序其实就是一种系统调用，软件中断可以当做一种异常。总之，软件中断是当前进程切换到系统调用的过程。

## 虚拟文件系统

内核空间的相关调用，比较复杂，涉及了Linux的虚拟文件系统VFS。所以，在分析内核空间的相关调用源码之前，我们需要先了解一些VFS相关的知识点。

虚拟文件系统VFS（Virtual Filesystem Switch）不是一种实际的文件系统，它只存在于内存中。它是一个内核软件层，是在具体的文件系统之上抽象的一层，用来处理与Posix文件系统相关的所有调用。它屏蔽了底层各种文件系统复杂的调用实现，为各种文件系统提供一个通用的接口。

在open()的系统调用的内核代码中，是通过VFS，最终访问到Binder文件系统，找到Binder的binder\_open()，并执行。如下图：

![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/e0f63ec8c68245729a5dd1410e5eaef8~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## VFS的四大对象和一张表

**VFS有四大对象**：super\_block、dentry、inode和file，贯穿了VFS的设计。

-   [super\_block](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Ffs.h%3Bl%3D1498 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/fs.h;l=1498")：超级块。存放挂载的文件系统的相关信息，挂载Binder文件系统到全局文件系统树时，会创建属于它的super\_block。
-   [dentry](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Fdcache.h%3Bl%3D92 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/dcache.h;l=92")：目录项。文件系统将目录也当作文件，目录的数据由目录项组成，而每个目录项存储一个目录或文件的名称、对应的inode等内容。open一个文件"/home/xxx/yyy.txt"，那么/、home、xxx、yyy.txt都是一个文件路径组件（`组件`这词主要来自源码中的`component`），都有一个对应的dentry。
-   [inode](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Ffs.h%3Bl%3D640 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/fs.h;l=640")：索引节点。描述磁盘上的一个文件元数据（文件属性、位置等）。
-   [file](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Ffs.h%3Bl%3D985 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/fs.h;l=985")：文件对象。open()时创建，close()时销毁。open一个文件/home/xxx/yyy.txt，会用file来记录yyy.txt对应的inode、dentry以及file\_operations。

这几大对象之间的关联如下：

![](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/27301a55043c4698958dcbc293e55792~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

-   一个文件系统中，只会有一个super\_block。
-   dentry中有指向super\_block的指针，即`d_sb`。dentry中有指向它对应的inode的指针，即`d_inode`。dentry中有指向它的父dentry的指针，即`d_parent`。
-   多个dentry能会指向同一个inode。不同的dentry可能是同一个inode的别名，inode里有个`i_dentry`字段，它是一个链表，链表的节点是dentry里的`d_alias`。当持有一个inode对象时，利用`i_dentry`和`d_adlias`，我们可以通过`container_of`获取到对应的dentry。详见[d\_find\_alias()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D1035 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=1035")。
-   inode中有指向super\_block的指针，即`i_sb`。
-   file中有指向对应的`inode`的指针，即`f_inode`。file里的`f_path`是结构体[path](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Fpath.h%3Bl%3D8 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/path.h;l=8")的对象，它记录了指向对应的dentry的指针。
-   file中的`f_op`，通常等价于对应的`inode`的`i_fop`。`f_op`是结构体[file\_operations](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Ffs.h%3Bl%3D2041 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/fs.h;l=2041")的对象，记录着一些函数指针，代表系统调用open()的目标文件所支持的操作，比如open()、read()、write()等。

**VFS有一张表**：dentry\_hashtable。它是一个hash表，用来保存对应一个dentry的hlist\_bl\_node，用拉链法来解决哈希冲突。它的好处是，能根据路径组件名称的hash值，快速定位到对应的hlist\_bl\_node，最后通过hlist\_bl\_node获得对应的dentry。

dentry\_hashtable是一个指向[hlist\_bl\_head](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Flist_bl.h%3Bl%3D34 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/list_bl.h;l=34")的指针的数组。通过name的hash值，调用[d\_hash()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D103 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=103")即可获取dentry\_hashtable中对应的`hlist_bl_head`。

```
struct hlist_bl_head {
struct hlist_bl_node *first;
};
```

而结构体[hlist\_bl\_node](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Flist_bl.h%3Bl%3D38 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/list_bl.h;l=38")类似一个双向链表，采用的是头插法（[hlist\_bl\_add\_head()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Flist_bl.h%3Bl%3D77 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/list_bl.h;l=77")）。它的定义如下：

```
struct hlist_bl_node {
struct hlist_bl_node *next, **pprev;
};
```

![](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/07527aa361d44e8b84a4a2330edaf976~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

`pprev`这个二级指针，指向的不是前面节点，而是指向**前面节点的`next`指针的存储地址**（如果是头节点，`pprev`会指向`hlist_bl_head`的`first`的存储地址）。这样的好处是：从链表中移除自身的时候，只需要将`next->pprev = pprev`即可。详见[\_\_hlist\_bl\_del()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Flist_bl.h%3Bl%3D115 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/list_bl.h;l=115")。

-   判断节点是否在链表中，只需要判断`pprev`是否为空。 详见[hlist\_bl\_unhashed()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Flist_bl.h%3Bl%3D52 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/list_bl.h;l=52")。
    
-   dentry\_hashtable的初始化：可能的初始化地方有两个，[dcache\_init\_early()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D3196 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=3196")和[dcache\_init()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D3217 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=3217")。都是通过[alloc\_large\_system\_hash()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fmm%2Fpage_alloc.c%3Bl%3D8894 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/mm/page_alloc.c;l=8894")分配一个容量是2的整数次幂的内存块。该内存块即一个数组，用来保存denstry。
    
-   hlist\_bl\_node中并没有dentry相关信息，查询dentry\_hashtable，如何通过hlist\_bl\_node指针获取对应的dentry？dentry里有一个字段`struct hlist_bl_node d_hash`，我们可以用hlist\_bl\_node指针，通过[container\_of()](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fwzc18743083828%2Farticle%2Fdetails%2F118730678 "https://blog.csdn.net/wzc18743083828/article/details/118730678")获得对应的dentry。
    
-   如何区分是否是目标dentry? dentry\_hashtable是采用拉链法解决冲突的（链表遍历详见[hlist\_bl\_for\_each\_entry\_rcu()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Frculist_bl.h%3Bl%3D95 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/rculist_bl.h;l=95")）。由于是根据路径组件名称来做hash的，所以：
    
    -   当hash相同，路径组件名称不同，比较一下路径组件名称的字符串就知道了
    -   当hash相同，路径组件名称相同，通过`hlist_bl_node`获取对应的`dentry`，判断一下`dentry->d_parent`是否是它的parent就知道了。因为同一目录下不可能有2个同名的文件，所以名称相同的，parent肯定不同。

## VFS的挂载

**挂载是指将一个文件系统，挂载到全局文件系统树上**。除根文件系统外，挂载点要求是一个已存在的文件系统的目录。

根文件系统rootfs，将会在系统启动的时候创建，挂载到"/"，也是全局文件系统树的根节点。

一个目录被文件系统挂载时，原来目录中包含的其他子目录或文件会被隐藏。以后进入该目录时，我们将会**通过VFS切换到新挂载的文件系统所在的根目录**，看到的是该文件系统的根目录下的内容。

**VFS挂载有三大对象**：

-   [mount](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fmount.h%3Bl%3D39 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/mount.h;l=39")：对应一次文件系统的挂载。记录了挂载点的dentry、vfsmount以及文件系统在文件系统树上的父节点mount。也记录了代表它自身的hlist\_node，与dentry类似，也是可以用hlist\_node的指针，通过container\_of()获取到对应的mountpoint()
-   [vfsmount](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Fmount.h%3Bl%3D72 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/mount.h;l=72")：记录了一个文件系统的super\_block和根目录的dentry。
-   [mountpoint](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fmount.h%3Bl%3D32 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/mount.h;l=32")：记录一个挂载点的dentry和代表它自身的hlist\_node，可以用hlist\_node的指针，通过container\_of()获取到对应的mountpoint()。

![](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/85e1f72672c44ce481f5f929c0a79ad8~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

-   mount持有指向mountpoint的指针。它的`mnt_mountpoint`和mountpoint的`m_dentry`是指向同一个dentry。该dentry对应的是挂载目录。
-   mount持有的是vfsmount的对象，不是指针。当持有指向该vfsmount的指针时，就可以通过container\_of()获得该mount了，详见[real\_mount()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fmount.h%3Bl%3D84 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/mount.h;l=84")。

**VFS挂载有两张表**：两张表的结构设计和dentry\_hashtable是一样的。

-   [mount\_hashtable](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D71 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=71")：通过**父mount的vfsmount**和**挂载点的dentry**，生成hash值，通过该表获得mount。详见[\_\_lookup\_mnt()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D625 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=625")。
-   [mountpoint\_hashtable](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D72 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=72")：通过挂载点的dentry，生成hash值，通过该表获得mountpoint。详见[lookup\_mountpoint()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D719 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=719")。

## VFS的路径行走

访问"/dev/binderfs/binder"时，可能经历了什么？

假设"/"是根文件系统rootfs的根目录。"dev"是根文件系统的根目录下的一个普通的目录，非挂载点。"binderfs"既根文件系统下的位于/dev里的目录，又是是binder文件系统的挂载点。"binder"是binder文件系统的挂载点下的一个代表binder设备的文件。

下图是访问"/dev/binderfs/binder"，VFS的路径行走流程：

![](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b939dbd1828f44eeb49a2b546ded429c~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

-   [path](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Fpath.h%3Bl%3D8 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/path.h;l=8")是一个vfsmount、dentry二元组。path中的vfsmount记录的是当前所在文件系统的根目录信息，而dentry是当前路径行走所在的组件。
-   VFS路径行走的起点是在根文件系统rootfs的根目录。所以一开始，path①记录的是根文件系统的vfsmount①和代表根文件系统根目录"/"的dentry①。
-   当我们进入到"dev"时，path②记录的dentry将是代表组件"dev"的dentry，由于还是在根文件系统，所以记录的vfsmount仍是vfsmount①。
-   当我们进入到"binderfs"时，path③记录的dentry将是代表组件"binderfs"的dentry，由于还是在根文件系统，所以记录的vfsmount仍是vfsmount①。但是，当我们检查dentry时，发现它有个`DCACHE_MOUNTED`的flag，表示它是一个挂载点。这时，我们就要利用path里的信息，即父mount①的vfsmount①和挂载点的dentry，从`mount_hashtable`获得Binder文件系统的mount②，实现函数是[\_\_lookup\_mnt()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D625 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=625")。最终获得vfsmount②里的dentry④，**成功切换到Binder文件系统的根目录**（Binder文件系统的根目录在我们访问的路径上是看不出来的，VFS屏蔽了用户的感知）。
-   在整个VFS的路径行走中，我们需要先进入根文件系统，最后再进入Binder文件系统。
-   注意，dentry③和dentry④是有区别的。dentry③是由根文件系统创建。而dentry④是由Binder文件系统，在挂载的时候创建的。

VFS的路径行走，可以说是open()的内核调用的核心设计。

## VFS的重复挂载

VFS可能会存在在一个挂载点上，先后挂载多个的文件系统的情况（如果挂载的文件系统类型相同，文件系统所在磁盘分区不同，也是可以的）。比如，我们在"binderfs"上，先挂载ext2文件系统，再挂载ext4系统，最后再挂载Binder文件系统。这时候，只有最后挂载的Binder文件系统是生效的。它们的挂载关联如下：

![](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d57a0b990f454590909a2c50241f1de7~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

因为重复挂载的缘故，为了找到最后挂载的Binder文件系统的mount，我们需要轮询调用[\_\_lookup\_mnt()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D625 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=625")。边轮询调用\_\_lookup\_mnt()，边更新path，直到\_\_lookup\_mnt()返回的mount\*为NULL：

```
//循环调用的示例代码
void sample_lookup(struct path *path) {
    struct vfsmount *mnt = path->mnt;
    struct dentry *dentry = path->dentry;
    for (;;) {
        if (flags & DCACHE_MOUNTED) {
                //通过父mount的vfsmount和挂载点的dentry，获取对应的mount
                struct mount *mounted = __lookup_mnt(path->mnt, dentry);
                //mounted不为NULL，表示不是最后一个挂载在该挂载点的文件系统
if (mounted) {
                        //更新path
path->mnt = &mounted->mnt;
                        //更新path里的dentry
dentry = path->dentry = mounted->mnt.mnt_root;
flags = dentry->d_flags;
continue;
}
                //return的时候，path里记录的就是最后一个挂载在该挂载点的文件系统的vfsmount
                //和文件系统根目录"/"的dentry。这也意味着我们切换到了该文件系统。
                return;
        }
        return;
    }
}


//__lookup_mnt实现
struct mount *__lookup_mnt(struct vfsmount *mnt, struct dentry *dentry)
{
        //通过父mount的vfsmount和在父文件系统的挂载点的dentry，获得hlist_head
struct hlist_head *head = m_hash(mnt, dentry);
struct mount *p;

        //遍历hlist_node链表，找到对应的mount
hlist_for_each_entry_rcu(p, head, mnt_hash)
if (&p->mnt_parent->mnt == mnt && p->mnt_mountpoint == dentry)
return p;
return NULL;
}
```

当我们取消挂载Binder文件系统时，之前挂载的ext4系统文件又会重新生效。同样，取消挂载ext4时，ext2又会重新生效。

## Binder文件系统的注册与挂载

在分析系统调用`open("/dev/binder")`的内核代码之前，我们还得先了解一下Binder文件系统的注册和挂载。

`open("/dev/binder")`最终其实是调用Binder文件系统的open()实现。而想要调用该open()实现，我们得先提前注册和挂载Binder文件系统。

### 注册

Binder驱动初始化的时候，会将Binder文件系统注册到全局文件系统中。

> Binder驱动初始化相关可以参考《[图解 Binder：初始化](https://juejin.cn/post/7201400444873293885 "https://juejin.cn/post/7201400444873293885")》

代码流程如下：

-   [binder\_init()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D6376 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=6376")
    -   [init\_binderfs()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinderfs.c%3Bl%3D788 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binderfs.c;l=788")
        -   register\_filesystem()

```
static struct file_system_type binder_fs_type = {
.name= "binder",
.init_fs_context= binderfs_init_fs_context,
.parameters= binderfs_fs_parameters,
.kill_sb= kill_litter_super,
.fs_flags= FS_USERNS_MOUNT,
};

int __init init_binderfs(void)
{
    ret = register_filesystem(&binder_fs_type);
}
```

`binder_fs_type`记录了Binder文件系统的一些信息。`init_fs_context`是一个函数指针，用来初始化文件系统上下文，这里被赋值了`binderfs_init_fs_context`。

```
static const struct fs_context_operations binderfs_fs_context_ops = {
.free= binderfs_fs_context_free,
.get_tree= binderfs_fs_context_get_tree,
.parse_param= binderfs_fs_context_parse_param,
.reconfigure= binderfs_fs_context_reconfigure,
};

static int binderfs_init_fs_context(struct fs_context *fc)
{
    fc->ops = &binderfs_fs_context_ops;
}
```

`binderfs_fs_context_ops`里的`get_tree`指向了`binderfs_fs_context_get_tree`。当将Binder文件系统挂载到全局文件系统树时，它就会执行`get_tree`，即`binderfs_fs_context_get_tree`。

### 挂载

Binder文件系统挂载到VFS的时机在init进程启动的时候。相关挂载指令在[system/core/rootdir/init.rc](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Frootdir%2Finit.rc%3Bl%3D269 "https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Frootdir%2Finit.rc%3Bl%3D269")中：

```
mkdir /dev/binderfs
mount binder binder /dev/binderfs stats=global
chmod 0755 /dev/binderfs

symlink /dev/binderfs/binder /dev/binder
symlink /dev/binderfs/hwbinder /dev/hwbinder
symlink /dev/binderfs/vndbinder /dev/vndbinder
```

`mount`指令的解析函数是[do\_mount()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Finit%2Fbuiltins.cpp%3Bl%3D478 "https://link.juejin.cn?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Finit%2Fbuiltins.cpp%3Bl%3D478")。

`symlink`指令的解析函数是[do\_symlink()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Finit%2Fbuiltins.cpp%3Bl%3D820 "https://link.juejin.cn?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Finit%2Fbuiltins.cpp%3Bl%3D820")。

do\_mount()会调用[mount()](https://link.juejin.cn/?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fmount.2.html "https://link.juejin.cn?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fmount.2.html")将Binder文件系统挂载到路径"/dev/binderfs"。

do\_symlink()会调用[symlink()](https://link.juejin.cn/?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fsymlink.2.html "https://link.juejin.cn?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fsymlink.2.html")，为相应的文件路径创建链接。比如，"/dev/binder"其实就是"/dev/binderfs/binder"的链接。当我们执行`open("/dev/binder", O_RDWR | O_CLOEXEC)`时，实际就是打开位于"/dev/binderfs/binder"的Binder驱动。

[mount()](https://link.juejin.cn/?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fmount.2.html "https://link.juejin.cn?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fmount.2.html")最终会调用内核的do\_mount()（不同于上面提到的init.rc里的mount指令的解析函数do\_mount()）。

由于do\_mount()不是本文分析的主要内容，所以这里简单给出do\_mount()挂载已注册的Binder文件系统到"/dev/binderfs"的大致调用流程。感兴趣的，可以自己点击相关源码链接，进行分析。

-   [do\_mount()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D3328 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=3328") //把Binder文件系统挂载到"/dev/binderfs"
    -   [user\_path\_at()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Fnamei.h%3Bl%3D54 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/namei.h;l=54") //分析路径"/dev/binderfs"，最终获得组件"binderfs"对应的`path`
    -   [path\_mount()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D3249 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=3249") //完成Binder文件系统的挂载
        -   [do\_new\_mount()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D2953 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=2953")
            -   [get\_fs\_type()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Ffilesystems.c%3Bl%3D273 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/filesystems.c;l=273") //获取我们之前注册的file\_system\_type类型的`binder_fs_type`
            -   [fs\_context\_for\_mount()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Ffs_context.c%3Bl%3D301 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/fs_context.c;l=301") //为即将挂载的Binder文件系统创建一个上下文`fs_context`
            -   [vfs\_get\_tree()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fsuper.c%3Bl%3D1488 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/super.c;l=1488") //（**vfs\_get\_tree()的调用，后面会进一步详细分析**）完成Binder文件系统的super\_block、根目录的dentry、inode的创建和初始化。
            -   [do\_new\_mount\_fc()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D2912 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=2912") //根据super\_block等相关信息，完成挂载
                -   [vfs\_create\_mount()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D966 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=966") //根据fs\_context，创建"binderfs"挂载点的mount。这时候，`mount->mnt.mnt_root = fc->root`，fc->root是在`binderfs_fs_context_get_tree`调用时创建的代表Binder文件系统根目录的dentry。
                -   [lock\_mount()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D2243 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=2243") //轮询调用\_\_lookup\_mnt()，兼容重复挂载的情况。
                -   [do\_add\_mount()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D2878 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=2878") //添加vfs\_create\_mount()新创建的mount到mount tree中
                    -   [graft\_tree()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D2284 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=2284")
                        -   [attach\_recursive\_mnt()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D2154 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=2154")
                            -   [mnt\_set\_mountpoint()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D861 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=861") //完成新建的mount的参数设置，比如`mount->mnt_parent`指向之前的挂载点mount
                            -   [commit\_tree()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D909 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=909")
                                -   [\_\_attach\_mnt()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D873 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=873") //添加新建的mount到mount\_hashtable中

#### vfs\_get\_tree()

vfs\_get\_tree()是在挂载的时候，利用之前注册时提供的信息，完成Binder文件系统的super\_block、根目录的dentry、inode的创建和初始化。

-   [vfs\_get\_tree()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fsuper.c%3Bl%3D1488 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/super.c;l=1488")
    -   [get\_tree()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinderfs.c%3Bl%3D758 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binderfs.c;l=758") //注册时，binderfs\_fs\_context\_ops里有个函数指针get\_tree被赋值为`binderfs_fs_context_get_tree`，这里其实就是调用binderfs\_fs\_context\_get\_tree()
        -   [binderfs\_fs\_context\_get\_tree()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinderfs.c%3Bl%3D744 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binderfs.c;l=744")
            -   [get\_tree\_nodev()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fsuper.c%3Bl%3D1167 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/super.c;l=1167") //创建代表Binder文件系统的super\_block
                -   [binderfs\_fill\_super()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinderfs.c%3Bl%3D658 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binderfs.c;l=658") //填充super\_block，创建根目录对应的dentry、inode等
                    -   [binderfs\_binder\_ctl\_create()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinderfs.c%3Bl%3D404 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binderfs.c;l=404") //创建代表binder-control设备的inode和dentry
                    -   [binderfs\_binder\_device\_create()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinderfs.c%3Bl%3D111 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binderfs.c;l=111") //会执行3次，分别创建代表binder、hwbinder、vndbinder这几个Binder设备的inode和dentry

```
static int binderfs_fill_super(struct super_block *sb, struct fs_context *fc)
{
    struct binderfs_device device_info = {};
    const char *name;
    
    //创建Binder文件系统根目录的inode
    inode = new_inode(sb);
    //创建binder-control设备，并为其创建对应的inode和dentry
    ret = binderfs_binder_ctl_create(sb);
    //创建Binder文件系统根目录的dentry
    sb->s_root = d_make_root(inode);
    //binder_devices_param为"binder,hwbinder,vndbinder"
    name = binder_devices_param;
    for (len = strcspn(name, ","); len > 0; len = strcspn(name, ",")) {
            strscpy(device_info.name, name, len + 1);
            //会执行3次，分别创建代表binder、hwbinder、vndbinder这几个Binder设备的inode和dentry
            ret = binderfs_binder_device_create(inode, NULL, &device_info);
            name += len;
            if (*name == ',')
name++;
    }
}

static int binderfs_binder_device_create(struct inode *ref_inode,
 struct binderfs_device __user *userp,
 struct binderfs_device *req)
{
    struct dentry *dentry, *root;
    struct super_block *sb = ref_inode->i_sb;
    inode = new_inode(sb);
    inode->i_fop = &binder_fops;
    
    root = sb->s_root;
    //根据name，在dentry_hashtable中查找是否存在对应的dentry，没有则创建一个，加入到dentry_hashtable
    //该dentry的d_parent会指向sb->s_root
    dentry = lookup_one_len(name, root, name_len);
    
    //inode的i_private会记录对应的binder设备信息
    inode->i_private = device;
    //dentry的d_inode会指向inode，并将dentry的d_alias加入到inode的i_dentry链表
    d_instantiate(dentry, inode);
}
```

注意在`binderfs_binder_device_create()`中，`inode->i_fop = &binder_fops`。[binder\_fops](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D6579 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=6579")是结构体[file\_operations](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Ffs.h%3Bl%3D2041 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/fs.h;l=2041")的对象，它记录着一些函数指针，代表Binder文件系统支持的系统调用的最终实现。

```
const struct file_operations binder_fops = {
.owner = THIS_MODULE,
.poll = binder_poll,
.unlocked_ioctl = binder_ioctl,
.compat_ioctl = compat_ptr_ioctl,
.mmap = binder_mmap,
.open = binder_open,
.flush = binder_flush,
.release = binder_release,
};
```

**`binder_open`就是Binder文件系统的open()实现**。`open("/dev/binder", O_RDWR | O_CLOEXEC)`最终就是通过VFS，获取到代表该binder设备的inode，根据`i_fop`记录的信息，获取到指向`binder_open()`的函数指针，完成调用。

## 内核空间的相关调用

了解了VFS后，我们可以进一步分析内核空间的相关调用。

从上文中，我们知道调用汇编函数`__openat`，触发软中断，进入内核态后，就来到了内核空间。内核最终会根据系统调用号，从系统调用表中，找到对应的系统调用函数，完成内核空间的相关调用。

## 系统调用表

系统调用表是一个数组，我们以对应的系统调用号为下标，就可以获得指向对应的系统调用函数的指针。

系统调用表`sys_call_table`的定义在[common/arch/arm64/kernel/sys.c](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Farch%2Farm64%2Fkernel%2Fsys.c%3Bl%3D58 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/arch/arm64/kernel/sys.c;l=58")

```
const syscall_fn_t sys_call_table[__NR_syscalls] = {
[0 ... __NR_syscalls - 1] = __arm64_sys_ni_syscall,
#include <asm/unistd.h>
};
```

## 系统调用号\_\_NR\_openat与对应的系统调用函数

前面我们知道了open()对应的系统调用号是`__NR_openat`。它在用户空间和内核空间都有对应的定义：

用户空间的\_\_NR\_openat定义在[bionic/libc/kernel/uapi/asm-generic/unistd.h](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fkernel%2Fuapi%2Fasm-generic%2Funistd.h%3Bl%3D95 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/kernel/uapi/asm-generic/unistd.h;l=95")

```
#define __NR_openat 56
```

内核空间的\_\_NR\_openat定义在 [common/include/uapi/asm-generic/unistd.h](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Fuapi%2Fasm-generic%2Funistd.h%3Bl%3D183 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/uapi/asm-generic/unistd.h;l=183")

```
#define __NR_openat 56
__SYSCALL(__NR_openat, sys_openat)
```

宏`__SYSCALL`的定义在[common/arch/arm64/kernel/sys.c](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Farch%2Farm64%2Fkernel%2Fsys.c%3Bl%3D51 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/arch/arm64/kernel/sys.c;l=51")

```
#undef __SYSCALL
#define __SYSCALL(nr, sym)asmlinkage long __arm64_##sym(const struct pt_regs *);
```

展开就是：

```
asmlinkage long __arm64_sys_openat(const struct pt_regs *);
```

`__NR_openat`其实就是对应`__arm64_sys_openat`函数。该函数的定义在[common/fs/open.c](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fopen.c%3Bl%3D1261 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/open.c;l=1261")：

```
SYSCALL_DEFINE4(openat, int, dfd, const char __user *, filename, int, flags,
umode_t, mode)
{
if (force_o_largefile())
flags |= O_LARGEFILE;
return do_sys_open(dfd, filename, flags, mode);
}
```

`SYSCALL_DEFINE4`这个宏定义的相关代码在[common/include/linux/syscalls.h](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Fsyscalls.h%3Bl%3D219 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/syscalls.h;l=219")和[common/arch/arm64/include/asm/syscall\_wrapper.h](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Farch%2Farm64%2Finclude%2Fasm%2Fsyscall_wrapper.h%3Bl%3D51 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/arch/arm64/include/asm/syscall_wrapper.h;l=51")中：

```
//syscalls.h
#define SYSCALL_DEFINE4(name, ...) SYSCALL_DEFINEx(4, _##name, __VA_ARGS__)

#define __MAP0(m,...)
#define __MAP1(m,t,a,...) m(t,a)
#define __MAP2(m,t,a,...) m(t,a), __MAP1(m,__VA_ARGS__)
#define __MAP3(m,t,a,...) m(t,a), __MAP2(m,__VA_ARGS__)
#define __MAP4(m,t,a,...) m(t,a), __MAP3(m,__VA_ARGS__)
#define __MAP5(m,t,a,...) m(t,a), __MAP4(m,__VA_ARGS__)
#define __MAP6(m,t,a,...) m(t,a), __MAP5(m,__VA_ARGS__)
#define __MAP(n,...) __MAP##n(__VA_ARGS__)

#define __SC_DECL(t, a)t a
#define __TYPE_AS(t, v)__same_type((__force t)0, v)
#define __TYPE_IS_L(t)(__TYPE_AS(t, 0L))
#define __TYPE_IS_UL(t)(__TYPE_AS(t, 0UL))
#define __TYPE_IS_LL(t) (__TYPE_AS(t, 0LL) || __TYPE_AS(t, 0ULL))
#define __SC_LONG(t, a) __typeof(__builtin_choose_expr(__TYPE_IS_LL(t), 0LL, 0L)) a
#define __SC_CAST(t, a)(__force t) a
#define __SC_ARGS(t, a)a
#define __SC_TEST(t, a) (void)BUILD_BUG_ON_ZERO(!__TYPE_IS_LL(t) && sizeof(t) > sizeof(long))

//syscall_wrapper.h
#define SC_ARM64_REGS_TO_ARGS(x, ...)\
__MAP(x,__SC_ARGS\
      ,,regs->regs[0],,regs->regs[1],,regs->regs[2]\
      ,,regs->regs[3],,regs->regs[4],,regs->regs[5])

#define __SYSCALL_DEFINEx(x, name, ...)\
asmlinkage long __arm64_sys##name(const struct pt_regs *regs);\
static long __se_sys##name(__MAP(x,__SC_LONG,__VA_ARGS__));\
static inline long __do_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__));\
asmlinkage long __arm64_sys##name(const struct pt_regs *regs)\
{\
return __se_sys##name(SC_ARM64_REGS_TO_ARGS(x,__VA_ARGS__));\
}\
static long __se_sys##name(__MAP(x,__SC_LONG,__VA_ARGS__))\
{\
long ret = __do_sys##name(__MAP(x,__SC_CAST,__VA_ARGS__));\
__MAP(x,__SC_TEST,__VA_ARGS__);\
__PROTECT(x, ret,__MAP(x,__SC_ARGS,__VA_ARGS__));\
return ret;\
}\
static inline long __do_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__))
```

[SYSCALL\_DEFINE4(openat, int, dfd,...)](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.10%3Acommon%2Ffs%2Fopen.c%3Bl%3D1238 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.10:common/fs/open.c;l=1238")这部分代码进行宏展开后就是：

```
asmlinkage long __arm64_sys_openat(const struct pt_regs *regs);
static long __se_sys_openat(long dfd, long filename, long flags, long mode);
static inline long __do_sys_openat(int dfd, const char __user * filename, int flags, umode_t mod);

asmlinkage long __arm64_sys_openat(const struct pt_regs *regs)
{
    return __se_sys_openat(regs->regs[0],regs->regs[1],regs->regs[2],regs->regs[3]);
}

static long __se_sys_openat(long dfd, long filename, long flags, long mode)
{
    long ret = __do_sys_openat((int) dfd, (const char __user *) filename, (int) flags, (umode_t) mode);
    ...
    return ret;
}

static inline long __do_sys_openat(int dfd, const char __user * filename, int flags, umode_t mod)
{
    if (force_o_largefile())
        flags |= O_LARGEFILE;
    return do_sys_open(dfd, filename, flags, mode);
}
```

`SYSCALL_DEFINE4`这个宏其实定义了下面几个函数，并完成了调用：

-   \_\_arm64\_sys\_openat()
    -   \_\_se\_sys\_openat()
        -   \_\_do\_sys\_openat()

## 内核空间的调用链路

整个调用链路的核心设计，就是VFS的路径行走：

-   [SYSCALL\_DEFINE4(openat, int, dfd,...)](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fopen.c%3Bl%3D1261 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/open.c;l=1261")
    -   [do\_sys\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fopen.c%3Bl%3D1247 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/open.c;l=1247")
        -   [do\_sys\_openat2()  
            ](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fopen.c%3Bl%3D1218 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/open.c;l=1218")
            -   [do\_filp\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D3634 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=3634") //初始化结构体nameidata的实例，并将open()相关参数保存在nameidata的实例中
                -   [path\_openat()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D3595 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=3595")
                    -   [path\_init()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D2320 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=2320") //获取根文件系统的vfsmount、根目录的dentry，并保存在nameidata中
                    -   [link\_path\_walk()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D2217 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=2217") //路径行走：逐个解析文件路径组件（除了最后一个组件），获取对应的dentry、inode
                        -   [walk\_component()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1953 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1953")
                            -   [lookup\_fast()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1568 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1568")
                                -   [\_\_d\_lookup\_rcu()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D2268 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=2268") //实现了rcu-walk
                                    -   [d\_hash()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bbpv%3D0%3Bl%3D103 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;bpv=0;l=103") //根据hash值，从dentry\_hashtable中获取对应的hlist\_bl\_head
                                    -   [hlist\_bl\_for\_each\_entry\_rcu()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Frculist_bl.h%3Bl%3D95 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/rculist_bl.h;l=95") //遍历链表hlist\_bl\_head，寻找对应的dentry
                                -   [\_\_d\_lookup()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D2393 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=2393") //实现了ref-walk
                            -   [lookup\_slow()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1669 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1669") //在lookup\_fast()中没有找到dentry，会获取父dentry对应的inode，通过`inode->i_op->lookup`去查找、创建
                                -   [\_\_lookup\_slow()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1632 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1632")
                                    -   [d\_alloc\_parallel()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D2568 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=2568") //创建一个新的dentry，并用in\_lookup\_hashtable检测、处理并发的创建操作
                                    -   [inode->i\_op->lookup](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1659 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1659") 通过父dentry的inode去查找对应的dentry：其实就是通过它所在的文件系统，获取对应的信息，创建对应的dentry
                            -   [step\_into()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1798 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1798") //处理dentry是一个链接或者挂载点的情况
                                -   [handle\_mounts()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1475 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1475") //处理一个挂载点的情况，获取最后一个挂载在挂载点的文件系统信息
                                    -   [\_\_follow\_mount\_rcu()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1425 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1425") //轮询调用\_\_lookup\_mnt()，处理重复挂载（查找标记有LOOKUP\_RCU时调用）
                                        -   [\_\_lookup\_mnt()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D625 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=625")
                                    -   [traverse\_mounts()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1373 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1373") //作用与\_\_follow\_mount\_rcu()类似
                                -   [pick\_link()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1720 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1720") //处理是一个链接的情况，获取对应的真实路径
                    -   [open\_last\_lookups()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D3350 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=3350") //分析最后的路径组件，有部分代码与link\_path\_walk()类似
                        -   [lookup\_fast()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1568 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1568")
                        -   [step\_into()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1798 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1798")
                    -   [do\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D3436 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=3436") //根据最后路径组件的inode，调用binder设备的binder\_open()
                        -   [vfs\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fopen.c%3Bl%3D955 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/open.c;l=955")
                            -   [do\_dentry\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fopen.c%3Bl%3D771 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/open.c;l=771") //将最后的路径组件对应的inode，将inode支持的file\_operations，保存在file中。最后调用file中的file\_operations里的open函数指针，最终会调用binder\_open()
                                -   [binder\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5708 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5708") //打开binder设备，进行相关初始化

### do\_filp\_open()

先来看看do\_filp\_open()：

-   [do\_filp\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D3634 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=3634")
    -   [set\_nameidata()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D605 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=605")
        -   [\_\_set\_nameidata()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D591 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=591")

```
struct file *do_filp_open(int dfd, struct filename *pathname,
const struct open_flags *op)
{
    //初始化结构体nameidata的实例
    struct nameidata nd;
    struct file *filp;
    //将open()相关参数保存在nd中
    set_nameidata(&nd, dfd, pathname, NULL);
    //第一次调用path_openat，flags中加入LOOKUP_RCU
    filp = path_openat(&nd, op, flags | LOOKUP_RCU);
    //path_openat失败，后续可能再调两次，这里就不深入分析了
    if (unlikely(filp == ERR_PTR(-ECHILD)))
filp = path_openat(&nd, op, flags);
    if (unlikely(filp == ERR_PTR(-ESTALE)))
filp = path_openat(&nd, op, flags | LOOKUP_REVAL);
}

static void __set_nameidata(struct nameidata *p, int dfd, struct filename *name)
{
    p->depth = 0;
    //dfd是在用户空间调用__openat()时传的第一个参数，固定为AT_FDCWD
    p->dfd = dfd;
    //把"/dev/binder"保存在nameidata的name字段中
    p->name = name;
}
```

-   结构体[nameidata](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D563 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=563")：主要记录open()相关参数。后续在遍历分析"/dev/binder"的各个路径名name时，会用nameidata来记录当前name对应的path和inode。

```
struct nameidata {
    struct pathpath; //当前路径组件对应的dentry、vfsmount
    struct qstrlast;
    struct pathroot; //根文件系统根目录的Path
    struct inode*inode
    struct filename*name; //filename里有个const char *name，即"/dev/binder"
    ...
}
```

### path\_openat()

现在进入到path\_openat()看看：

-   [path\_openat()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D3595 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=3595")
    -   [path\_init()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D2320 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=2320")
        -   [nd\_jump\_root()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D951 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=951")
            -   [set\_root()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D924 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=924")

```
static struct file *path_openat(struct nameidata *nd,
const struct open_flags *op, unsigned flags)
{
struct file *file;
        //分配一个VFS的空的file
        file = alloc_empty_file(op->open_flag, current_cred());
        if (...) {
} else {
                //s即"/dev/binder"
                const char *s = path_init(nd, flags);
                //
                while (!(error = link_path_walk(s, nd)) &&
       (s = open_last_lookups(nd, file, op)) != NULL)
;
if (!error)
error = do_open(nd, file, op);
        }
}

static const char *path_init(struct nameidata *nd, unsigned flags)
{
        //取出之前保存在nameidata中的文件路径
        const char *s = nd->name->name;
        //RCU加锁
        if (flags & LOOKUP_RCU)
rcu_read_lock();
        if (*s == '/' && !(flags & LOOKUP_IN_ROOT)) {
                //获取文件系统的根目录，并保存在nameidata中
error = nd_jump_root(nd);
return s;
}
        ...
}

static int nd_jump_root(struct nameidata *nd)
{
if (!nd->root.mnt) {
                //获取根目录的Path信息，保存到nd->root中
int error = set_root(nd);
}
if (nd->flags & LOOKUP_RCU) {
                //将根文件系统的Path、inode记录到nameidata中
struct dentry *d;
nd->path = nd->root;
d = nd->path.dentry;
nd->inode = d->d_inode;
nd->seq = nd->root_seq;
} else {
path_put(&nd->path);
nd->path = nd->root;
path_get(&nd->path);
nd->inode = nd->path.dentry->d_inode;
}
nd->state |= ND_JUMPED;
return 0;
}
        
static int set_root(struct nameidata *nd)
{
        //获取当前进程的fs_struct
struct fs_struct *fs = current->fs;
        
if (nd->flags & LOOKUP_RCU) {
unsigned seq;
do {
seq = read_seqcount_begin(&fs->seq);
                        //从fs中获取根目录"/"的Path，里面有它对应的dentry，保存在nd->root中
nd->root = fs->root;
nd->root_seq = __read_seqcount_begin(&nd->root.dentry->d_seq);
} while (read_seqcount_retry(&fs->seq, seq));
}
return 0;
}
```

-   RCU是Read-copy\_update，是一种数据同步机制，允许读写同时进行。读操作不存在睡眠、阻塞、轮询，不会形成死锁，相比读写锁效率更高。写操作时先拷贝一个副本，在副本上进行修改、发布，并在合适时间释放原来的旧数据。
-   `current`的定义在[common/arch/arm64/include/asm/current.h](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.10%3Acommon%2Farch%2Farm64%2Finclude%2Fasm%2Fcurrent.h%3Bl%3D24 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.10:common/arch/arm64/include/asm/current.h;l=24")里。`current`是一个结构体[task\_struct](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.10%3Acommon%2Finclude%2Flinux%2Fsched.h%3Bl%3D656 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.10:common/include/linux/sched.h;l=656")，表示当前正在运行进程的进程描述符，记录着进程相关信息，包括进程状态、内存信息等等。
-   结构体[fs\_struct](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Ffs_struct.h%3Bl%3D9 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/fs_struct.h;l=9")记录当前进程的文件系统相关信息。比如根文件系统的根目录[Path](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Fpath.h%3Bl%3D8 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/path.h;l=8")（Path中记录了dentry和vfsmount）。

### link\_path\_walk()

现在我们来看看VFS是如何解析文件路径组件，获取对应的dentry、inode。

-   [link\_path\_walk()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D2217 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=2217") //路径行走：逐个解析文件路径组件（除了最后一个组件），获取对应的dentry、inode
    -   [walk\_component()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1953 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1953") //查找组件对应的dentry、inode

```
static int link_path_walk(const char *name, struct nameidata *nd)
{
        int depth = 0; // depth <= nd->depth
        //指针移动，"/dev/binder"变成了"dev/binder"
        while (*name=='/')
name++;
        //轮询处理各个文件路径名
        for(;;) {
                const char *link;
u64 hash_len;
int type;
                //计算name中除根路径组件外的第一个路径组件的hash值和名字长度。
                //第一次循环的第一个路径组件是"dev"
                //返回值hash_len是64位的无符号整数，高32位记录name的长度len，低32位记录name的hash值
                hash_len = hash_name(nd->path.dentry, name);
                
                type = LAST_NORM;
                //处理路径组件中含有'.'的情况，不作分析
                if (name[0] == '.') switch (hashlen_len(hash_len)) {
                    ...
                }
                if (likely(type == LAST_NORM)) {
                        //第一次循环parent代表根目录"/"，在前面的path_init()中获取到的
                        //第二次循环parent代表目录"dev"...
struct dentry *parent = nd->path.dentry;
nd->state &= ~ND_JUMPED;
}
                //保存信息到nameidata中
                nd->last.hash_len = hash_len;
nd->last.name = name;
nd->last_type = type;
                
                //指针移动，第一次循环时，"dev/binder"变成了"/binder"
                name += hashlen_len(hash_len);
                //注意通常循环处理到最后一个组件时，if (!*name)是为true的
                if (!*name)
goto OK;
                //指针移动，第一次循环时，"/binder"变成了"binder"
                do {
name++;
} while (unlikely(*name == '/'));
                
                if (unlikely(!*name)) {
OK:
/* pathname or trailing symlink, done */
if (!depth) {
                                //路径最后一个组件，不管是否是symlink，
                                //都会走到这里，不会执行walk_component()
nd->dir_uid = i_uid_into_mnt(mnt_userns, nd->inode);
nd->dir_mode = nd->inode->i_mode;
nd->flags &= ~LOOKUP_PARENT;
return 0;
}
/* last component of nested symlink */
                        //symlink即链接。只有嵌套在文件路径中的symlink才可能走这里。
                        //如果是位于文件路径末尾的symlink，都会先视为最后一个组件，不会走这里
name = nd->stack[--depth].name;
link = walk_component(nd, 0);
} else {
/* not the last component */
                        //不是最后一个组件，查找对应的dentry、inode，检查是否是挂载点
link = walk_component(nd, WALK_MORE);
}
                //link指symlink对应的真实路径
if (unlikely(link)) {
/* a symlink to follow */
                        //只有嵌套在文件路径中的symlink，才会走到这里。
                        //如"/a/b/c"，检查组件a时，发现它其实是个嵌套symlink，代表文件路径"e/f"。
                        //在执行walk_component()后，返回的link就是"e/f"
                        //这时候，name就是"b/c"，将它保存在nd->stack中，并自增depth。
                        //walk_component完symlink对应的真实路径后，才会取出来"b/c"，对它执行walk_component()
nd->stack[depth++].name = name;
                        //将"e/f"赋值给name，继续循环，执行walk_component()
name = link;
continue;
}
        }
}

static const char *walk_component(struct nameidata *nd, int flags)
{
struct dentry *dentry;
struct inode *inode;
        unsigned seq;
        
        //处理路径组件中含有'.'的情况，不作分析
        if (unlikely(nd->last_type != LAST_NORM)) {
...
}
        //先执行快查找
dentry = lookup_fast(nd, &inode, &seq);
if (unlikely(!dentry)) {
                //快速查找失败，才会执行慢查找
dentry = lookup_slow(&nd->last, nd->path.dentry, nd->flags);
}
if (!(flags & WALK_MORE) && nd->depth)
put_link(nd);
        //检查是否是链接、挂载点：
        //是链接，则获取到对应的真实路径。
        //是挂载点，则获取最后一个挂载在挂载点的文件系统信息。
return step_into(nd, flags, dentry, inode, seq);
}
```

前面在讲Binder文件系统挂载的时候，有提到：`/dev/binder`就是`/binderfs/binder`的symlink。不过`binder`是路径"/dev/binder"的最后一个组件，所以它在第一次执行link\_path\_walk()，不会对其用walk\_component()进行查找。整体的处理逻辑，得看调用link\_path\_walk()的地方path\_openat()：

```
static struct file *path_openat(struct nameidata *nd,
const struct open_flags *op, unsigned flags)
{
        
        //s即"/dev/binder"
        const char *s = path_init(nd, flags);
        //在第一次循环时，s是"/dev/binder"，执行link_path_walk()，"binder"是最后一个组件，
        //跳出link_path_walk()的循环处理，调用open_last_lookups()处理，但发现"binder"是一个symlink。
        //open_last_lookups()会返回"binder"对应的"binderfs/binder"（又或者是"dev/binderfs/binder"?）。
        
        //第二次循环时，s已经是"binderfs/binder", 再次执行link_path_walk()，"binder"是最后一个组件，
        //不过这个"binder"不同于上次循环那个，不是symlink，而是一个真实存在的路径组件
        //最后调用open_last_lookups()即可获得最后一个组件"binder"对应的dentry、inode等信息
        while (!(error = link_path_walk(s, nd)) &&
(s = open_last_lookups(nd, file, op)) != NULL)
;
}
```

### lookup\_fast()

`lookup_fast()`是根据路径组件的名称，快速找到对应的dentry、inode的实现，主要分为`rcu-walk`和`ref-walk`，具体设计详见[path-lookup.txt](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android-mainline%3Acommon%2FDocumentation%2Ffilesystems%2Fpath-lookup.txt%3Bl%3D1%3Fq%3D%252Ffilesystems%252Fpath-lookup.txt%26ss%3Dandroid%252Fkernel%252Fsuperproject "https://cs.android.com/android/kernel/superproject/+/common-android-mainline:common/Documentation/filesystems/path-lookup.txt;l=1?q=%2Ffilesystems%2Fpath-lookup.txt&ss=android%2Fkernel%2Fsuperproject")。

-   [lookup\_fast()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1568 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1568")
    -   [\_\_d\_lookup\_rcu()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D2268 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=2268") //实现了rcu-walk
        -   [d\_hash()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bbpv%3D0%3Bl%3D103 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;bpv=0;l=103") //根据hash值，从dentry\_hashtable中获取对应的hlist\_bl\_head
        -   [hlist\_bl\_for\_each\_entry\_rcu()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Frculist_bl.h%3Bl%3D95 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/rculist_bl.h;l=95") //遍历链表hlist\_bl\_head，寻找对应的dentry
    -   [\_\_d\_lookup()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D2393 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=2393") //实现了ref-walk

```
static struct dentry *lookup_fast(struct nameidata *nd,
  struct inode **inode,
          unsigned *seqp)
{
struct dentry *dentry, *parent = nd->path.dentry;
        //flags里有LOOKUP_RCU标记，则执行rcu-walk，否则执行ref-walk
        if (nd->flags & LOOKUP_RCU) {
                //rcu-walk
dentry = __d_lookup_rcu(parent, &nd->last, &seq);
if (unlikely(!dentry)) {
                        //移除flags里的LOOKUP_RCU标记，尝试切换到ref-walk
                        //成功则在下一个组件的lookup中，会采用ref-walk。
                        //当前的组件，看流程，不会换到ref-walk，而是用lookup_slow进行查找
if (!try_to_unlazy(nd))
return ERR_PTR(-ECHILD);
return NULL;
                }
                ...
        } else  {
                //ref-walk
                dentry = __d_lookup(parent, &nd->last);
                if (unlikely(!dentry))
return NULL;
        }
        return dentry;
}
```

#### rcu-walk和ref-walk

rcu-walk的实现是[\_\_d\_lookup\_rcu()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D2268 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=2268")。

ref-walk的实现是[\_\_d\_lookup()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D2393 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=2393")。

两者的实现代码类似，都是从dentry\_hashtable中查询，主要差异有：

1.  `__d_lookup_rcu()`遍历查找目标dentry的时候，使用了顺序锁`seqlock`，读操作不会被写操作阻塞，写操作也不会被读操作阻塞。但读操作可能会反复读取相同的数据：当它发现`sequence`发生了变化，即它执行期间，有写操作更改了数据。另外，seqlock要求进入临界区的写操作只有一个，多个写操作之间仍然是互斥的。关键代码如下：

```
struct dentry *__d_lookup_rcu(const struct dentry *parent,
const struct qstr *name,
unsigned *seqp)
{
        //hashlen_hash()获取hash值。
        //d_hash()根据hash值，从dentry_hashtable中获取对应的hlist_bl_head
        struct hlist_bl_head *b = d_hash(hashlen_hash(hashlen));
struct hlist_bl_node *node;
struct dentry *dentry;
        
        //遍历链表b里的dentry，查找目标dentry
        hlist_bl_for_each_entry_rcu(dentry, node, b, d_hash) {
unsigned seq;
seqretry:
        //获取当前dentry->d_seq，即sequence
        seq = raw_seqcount_begin(&dentry->d_seq);
        //检查是否有写操作，导致sequence发生了变化，有则跳转到seqretry处，重新执行读操作
        if (read_seqcount_retry(&dentry->d_seq, seq)) {
            cpu_relax();
            goto seqretry;
        }
    ...
}
```

-   dentry->d\_seq的类型是seqcount\_spinlock\_t。seqcount\_spinlock\_t经过一些复杂的宏定义包含了[seqcount\_t](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Fseqlock.h%3Bl%3D65 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/seqlock.h;l=65")，可以简单认为seqcount\_spinlock\_t就是一个int序列号。
-   seqlock的设计可以参考[文档](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android-mainline%3Acommon%2FDocumentation%2Flocking%2Fseqlock.rst%3Fq%3Dseqcount_spinlock_t%26ss%3Dandroid%252Fkernel%252Fsuperproject "https://cs.android.com/android/kernel/superproject/+/common-android-mainline:common/Documentation/locking/seqlock.rst?q=seqcount_spinlock_t&ss=android%2Fkernel%2Fsuperproject")。

2.  [`__d_lookup()`](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D2363 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=2363")遍历查找目标dentry的时候，使用了自旋锁`spin_lock`，多个读操作会发生锁竞争。它还会更新查找到的dentry的引用计数，这也是它为什么叫"ref-walk"的原因。关键代码如下：

```
struct dentry *__d_lookup(const struct dentry *parent, const struct qstr *name)
{
    //遍历链表b里的dentry，查找目标dentry
    hlist_bl_for_each_entry_rcu(dentry, node, b, d_hash) {
        //spin_lock加锁
        spin_lock(&dentry->d_lock);
        ...
        //查找到目标dentry，增加引用计数
        dentry->d_lockref.count++;
found = dentry;
        //解锁
spin_unlock(&dentry->d_lock);
break;
next:
        //没有查找到目标dentry会跳转到next这里，解锁，继续查找
spin_unlock(&dentry->d_lock);  
    }
    ...
}
```

-   RCU仍然用于ref-walk中的dentry哈希查找，但不是在整个ref-walk过程中都使用。
-   频繁地加减reference count可能造成cacheline的刷新，这也是ref-walk开销更大的原因之一。

### lookup\_slow()

lookup\_slow()与lookup\_fast()不一样。lookup\_fast的两种模式，都是查询的dentry\_hashtable。而lookup\_slow是兜底方案，lookup\_fast失败后，才会调用。

lookup\_slow()是通过当前所在的文件系统，获取对应的信息，创建对应的dentry和inode，将新的dentry添加到dentry\_hashtable中。

-   [lookup\_slow()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1669 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1669") //在lookup\_fast()中没有找到dentry，会获取父dentry对应的inode，通过`inode->i_op->lookup`去查找、创建
    -   [\_\_lookup\_slow()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1632 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1632")
        -   [d\_alloc\_parallel()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fdcache.c%3Bl%3D2568 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/dcache.c;l=2568") //创建一个新的dentry，并用in\_lookup\_hashtable检测、处理并发的创建操作
        -   [inode->i\_op->lookup](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1659 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1659") 通过父dentry的inode去查找对应的dentry：其实就是通过它所在的文件系统，获取对应的信息，创建对应的dentry

要注意的是，在前面的Binder文件系统挂载的这一小节，我们已经提到过`binderfs_binder_device_create()`会为binder设备创建对应的dentry、inode，并将dentry加入到dentry\_hashtable中。所以`lookup_slow()`通常是没有机会走的。

### step\_into()

step\_into()主要是处理dentry是一个链接或者挂载点的情况。

-   [step\_into()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1798 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1798")
    -   [handle\_mounts()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1475 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1475") //处理一个挂载点的情况，获取最后一个挂载在挂载点的文件系统信息
        -   [\_\_follow\_mount\_rcu()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1425 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1425") //轮询调用\_\_lookup\_mnt()，处理重复挂载（查找标记有LOOKUP\_RCU时调用）
            -   [\_\_lookup\_mnt()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamespace.c%3Bl%3D625 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namespace.c;l=625")
        -   [traverse\_mounts()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1373 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1373") //作用与\_\_follow\_mount\_rcu()类似
    -   [pick\_link()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D1720 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=1720") //处理是一个链接的情况，获取对应的真实路径

```
static const char *step_into(struct nameidata *nd, int flags,
     struct dentry *dentry, struct inode *inode, unsigned seq)
{
struct path path;
        //处理组件是挂载点的情况
int err = handle_mounts(nd, dentry, &path, &inode, &seq);
        //检查是否是链接
if (likely(!d_is_symlink(path.dentry)) ||
   ((flags & WALK_TRAILING) && !(nd->flags & LOOKUP_FOLLOW)) ||
   (flags & WALK_NOFOLLOW)) {
/* not a symlink or should not follow */
if (!(nd->flags & LOOKUP_RCU)) {
dput(nd->path.dentry);
if (nd->path.mnt != path.mnt)
mntput(nd->path.mnt);
}
                //更新path
nd->path = path;
nd->inode = inode;
nd->seq = seq;
return NULL;
}
        //处理组件是链接的情况
return pick_link(nd, &path, inode, seq, flags);
}
```

### do\_open()

do\_open()是我们的最后一站。到这里时，我们已经获得了最后路径组件的inode，也就是找到了我们的binder设备，可以尝试打开它了。

-   [do\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fnamei.c%3Bl%3D3436 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/namei.c;l=3436") //根据最后路径组件的inode，调用binder设备的binder\_open()
    -   [vfs\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fopen.c%3Bl%3D955 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/open.c;l=955")
        -   [do\_dentry\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Ffs%2Fopen.c%3Bl%3D771 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/fs/open.c;l=771") //将binder设备支持的file\_operations，保存在file中。最后调用file中的file\_operations里的open函数指针，最终会调用binder\_open()
            -   [binder\_open()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5708 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5708") //打开binder设备，进行相关初始化

```
static int do_open(struct nameidata *nd,
   struct file *file, const struct open_flags *op)
{
        if (!error && !(file->f_mode & FMODE_OPENED))
error = vfs_open(&nd->path, file);
}

int vfs_open(const struct path *path, struct file *file)
{
file->f_path = *path;
return do_dentry_open(file, d_backing_inode(path->dentry), NULL);
}

static int do_dentry_open(struct file *f,
  struct inode *inode,
  int (*open)(struct inode *, struct file *))
{
        f->f_inode = inode;
        //将binder设备支持的file_operations，保存在file中
        f->f_op = fops_get(inode->i_fop);
        if (!open)
                //f->f_op->open即binder_open()
open = f->f_op->open;
        if (open) {
        //调用binder_open()
error = open(inode, f);
}

//common/drivers/android/binder.c
static int binder_open(struct inode *nodp, struct file *filp)
{
    //打开binder设备，执行相关初始化
    ...
}
```

## 参考资料

[Linux 虚拟文件系统四大对象：超级块、inode、dentry、file之间关系](https://link.juejin.cn/?target=https%3A%2F%2Fzhuanlan.zhihu.com%2Fp%2F354100369 "https://zhuanlan.zhihu.com/p/354100369")

[深入理解Linux文件系统之文件系统挂载(上)](https://link.juejin.cn/?target=https%3A%2F%2Fcloud.tencent.com%2Fdeveloper%2Farticle%2F1857535 "https://cloud.tencent.com/developer/article/1857535")

[深入理解Linux文件系统之文件系统挂载(下)](https://link.juejin.cn/?target=https%3A%2F%2Fcloud.tencent.com%2Fdeveloper%2Farticle%2F1857533 "https://cloud.tencent.com/developer/article/1857533")

[RCU到底是什么？为什么快？为什么可以读写并行？](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fs2603898260%2Farticle%2Fdetails%2F120802419 "https://blog.csdn.net/s2603898260/article/details/120802419")

[Linux下多挂载点mount实验](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Ftimonium%2Farticle%2Fdetails%2F123411102 "https://blog.csdn.net/timonium/article/details/123411102")
