---
created: 2025-04-15T13:12:08 (UTC +08:00)
tags: []
source: https://www.cnblogs.com/Linux-tech/p/12961295.html
author: 内核工匠
---

# dumpsys meminfo 的原理和应用 - 内核工匠 - 博客园

> ## Excerpt
> 什么是dumpsys meminfoAndroid中通过命令dumpsys meminfo package_name|pid, 查看指定进程的内存使用情况.通过输出的信息,可以看出来应用在内存哪里分配出现了问题,...

---
**什么是dumpsys meminfo**

Android中通过命令dumpsys meminfo package\_name|pid, 查看指定进程的内存使用情况.通过输出的信息,可以看出来应用在内存哪里分配出现了问题,比如native heap 分配高,就要查一下自己的native部分的代码哪里分配后没有及时释放。

这条命令是如何工作的?

Android中的Service都会注册到ServiceManager,每个Service要实现binder定义的接口,其中的dump方法dump(int fd, const Vector<String16>& args),通过给定的参数,把信息送到文件描述符fd中。

dumpsys位于/system/bin/dumpsys.运行时它首先后获得ServiceManager,然后根据参数查找服务,找到后,然后binder调用服务的dump方法.而meminfo是系统的一个服务(内部则是一个MemBinder的类实现的),当dumpsys找到这个服务后,调用dump方法,通过传递的参数,显示出进程内存使用情况。

 下面是一个进程的meminfo。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qUGp5bFRDaWFMaGJUTlhlNXFxMDNWaWFxTEZ3dHVBdkFuMzdXeGgwbWZ2VURJYVZBMXZMMHE1bmhtcE5zaWIyZXRHQ0c1ekRYQURRM1lmZy82NDA?x-oss-process=image/format,png)

上面图中的数据分A和B两个部分, A 部分是进程内存的详细信息, B 部分是对详细信息进行的总结。

而A部分的数据是从哪里读到的?

通过以下三个方式,图中A.1部分的信息是通过解析/proc/pid/smaps获得的, A.2 通过IPC (binder 和pipe)方式获得。而A.3通过memtrack.vendor\_xxx.so中的接口获得的。

下面将从A, B 两个部分,详细介绍这些数值是如何生成的。

## **A部分**

### **A.1 smaps**

下面是meminfo读取进程的smaps从上到下的调用关系图。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qUGp5bFRDaWFMaGJUTlhlNXFxMDNWaWFxQjZ5VU1LSHhZQURieG9LNmZIeXhBeE1hMDdyUW1jblg1Nkk2eWlib293UnY3WmVHcVI5S1VlQS82NDA?x-oss-process=image/format,png)

读取进程smaps的节点

根据属性的不同，进程的虚拟地址空间被划分成若干个vma，每一个vma通过vm\_next和vm\_prev组成双向链表,链表头位于进程的task->mm->mmap.当通过proc接口读取进程的smaps文件时,内核会首先找到该进程的vma链表头,遍历链表中的每一个vma， 通过walk\_page\_vma统计这块vma的使用情况,最后显示出来。

下图是一块vma的统计信息

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qUGp5bFRDaWFMaGJUTlhlNXFxMDNWaWFxR3FOcFZZOEk4eHdpY3ljU1U5cnFXcENqUWJnWlFRMnBHRWYwVEpPSGhVb0dGdURVdkhYUVFKUS82NDA?x-oss-process=image/format,png)

解析smaps用到了以下几个字段.

-   Pss  
    PSS (Proportional Set Size) = 进程独占的内存 + 进程程共享的内存 / 映射次数。  
    内存的管理是以 page 为单位的, 如果 page 的 \_refcount或者 \_mapcount为 1, 那么就是进程独占的内存. 也叫 private. 如果 page 的 \_mapcount 为 n ( n >= 2), 这块内存以 page / n 来统计。
    
-   Private\_Dirty  
    Private 在上面已经说过了。 而 Dirty 分为 PageDirty和 pte\_dirty. PageDirty就是所说的脏页( 文件读到内存中被修改过, 就会标记为脏页)。 pte\_dirty则当 vma 用于 anonymous 的时候, 读写这段 vma 时候, 触发 page fault, 调用 do\_anonymous\_page , 如果vma\_flags中包含 VM\_WRITE, 则会通过 pte\_mkdirty(entry)标记。
    
-   Private\_Clean  
    与 Private\_Dirty 相反。
    
-   Swap  
    一般情况下, 在 Android 中就是 zram, 通过压缩内存页面并将其放入动态分配的内存交换区来增加系统中的可用内存量, 压缩的都是匿名页。
    

但下面这段vma是文件映射的,但还有swap字段的,这是因为这个文件是通过mmap到进程地址空间的。当标记中有MAP\_PRIVATE时,这表示是一个copy-on-write的映射,虽然是file-backed , 但当向这个数据写入数据的时候,会把数据拷贝到匿名页里,所以看到上面的Anonymous:也不为0。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qUGp5bFRDaWFMaGJUTlhlNXFxMDNWaWFxaWNuTHJ2YUR0T29MTmtFdHo4NDN0dkpQV1hEQ0N2anpsUkZFWjJlTGlidFJyNmxsclZBN1kwaFEvNjQw?x-oss-process=image/format,png)

A.1信息的分类则通过vma的名字进行字符串匹配,然后将内存信息划分为不同的部分

1\. vma用于file时

  
当vma->vm\_file不为空的时候,名字为file->f\_path文件路径,文件包含存储介质的文件,也包含设备文件,虚拟文件等(everything is a file)。

下面是meminfo的字段对照表

<table><tbody><tr><td><p><strong>类型</strong></p></td><td><p><strong>匹配字符串</strong></p></td></tr><tr><td><p>Ashmem</p></td><td><p>/dev/ashmem</p></td></tr><tr><td><p>Gfx dev</p></td><td><p>/dev/kgsl-3d0</p></td></tr><tr><td><p>Other dev</p></td><td><p>/dev/*</p></td></tr><tr><td><p>.so mmap</p></td><td><p>*.so 例 /system/lib64/libstdc++.so</p></td></tr><tr><td><p>.jar mmap</p></td><td><p>*.jar 例 /system/framework/framework.jar</p></td></tr><tr><td><p>.apk mmap</p></td><td><p>*.apk 例 /system/framework/framework-res.apk</p></td></tr><tr><td><p>.ttf mmap</p></td><td><p>*.ttf 例 /system/fonts/Roboto-BlackItalic.ttf</p></td></tr><tr><td><p>.dex mmap</p></td><td><p>*.vdex 例 /system/framework/boot.vdex</p></td></tr><tr><td><p>.oat mmap</p></td><td><p>*.oat 例 /system/framework/arm64/boot-ext.oat</p></td></tr><tr><td><p>.art mmap</p></td><td><p>*.art 例 /system/framework/arm64/boot.art</p></td></tr><tr><td><p>Other mmap</p></td><td><p>不属于上面的类型</p></td></tr></tbody></table>

例如在解析vma过程中,如果vma的名字包含/dev/kgsl-3d0的时,就把它归在Gfx Dev 这类。

2\. vma 用于anonymous时

  
这种用途的vma的名字\[anon:开头,虽然叫匿名,但显示哪里分配的,比如说上图的\[anon:libc\_malloc\],就说明这段vma是从libc\_malloc中分配出去的。  
老的内核版本是没有这个域的,后来因为userspace的进程有很多种不同的分配器(allocators) , 如果出现异常不知道哪里出了问题,就比如说如何区分Dalvik Heap 和Native Heap, 后来android提了一笔可以修改内存区域的名字的补丁,通过prctl(PR\_SET\_VMA, PR\_SET\_VMA\_ANON\_NAME, start, len, (unsigned long)name)系统调用,就可以把名字保存在vma\_area\_struct ->shared.anon\_name有趣的这是个userspace指针,来降低mm子系统的复杂性.同时复用了vma\_area\_struct结构体的shared(利用union),shared用在file-backed mappings,这样就不和用于匿名的vma冲突.连内存都省了(这可能就是程序的魅力所在)。

下面是meminfo的字段对照表

<table><tbody><tr><td><p><strong>类型</strong></p></td><td><p><strong>匹配字符串</strong></p></td></tr><tr><td><p>Native Heap</p></td><td><p>[anon:libc_malloc] 或 [heap]</p></td></tr><tr><td><p>Dalvik Heap</p></td><td><p>[anon:dalvik-*</p></td></tr><tr><td><p>Dalvik Other</p></td><td><p>Dalvik Heap 的部分 比如 [anon:dalvik-indirect ref 开头的)</p></td></tr></tbody></table>

### **A.2进程中读取**

下面是从meminfo读取进程运行状态的信息调用图.这部分信息是通过IPC方式获得。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qUGp5bFRDaWFMaGJUTlhlNXFxMDNWaWFxaWNwaWF6RmFuWmcxVTZHM1JMNmUxamljclhWeW9KTm9QUDZDVWlhUWZrSXVFeHJ4bEpZbmxnVmRUUS82NDA?x-oss-process=image/format,png)

Meminfo打开一个pipe,通过异步binder调用并传递了pipe fd, 然后开始监听fd,应用进程获得了这个pipe fd后 读取需要的内存数据,通过pipe传递到AMS中。

-   mallinfo()  
    返回内存分配的统计信息, 函数声明在 android/bionic/libc/include/malloc.h,函数定义在 android/bionic/libc/bionic/malloc\_common.h.
    

#if defined(USE\_SCUDO)

#include "scudo.h"  
#define Malloc(function) scudo\_ ## function

#else

#include "jemalloc.h"  
#define Malloc(function) je\_ ## function

#endif

函数调用Malloc(mallinfo),这是对mallinfo进行封装,来加载不同的分配器对应函数的实现.因为Android使用的是jemalloc,也就会调用je\_mallinfo()。

malloc结构体定义,返回struct mallinfo。

下面是meminfo和mallinfo字段对照表

<table><tbody><tr><td><p>Native Heap Size</p></td><td><p>mallinfo.usmblks</p></td><td><p>Maximum total allocated space</p></td></tr><tr><td><p>Native Heap Alloc</p></td><td><p>mallinfo.uordblks</p></td><td><p>Total allocated space</p></td></tr><tr><td><p>Native Heap Free</p></td><td><p>mallinfo.fordblks</p></td><td><p>Total free space</p></td></tr></tbody></table>

-   runtime
    

Runtime runtime = Runtime.getRuntime();

// Do GC since countInstancesOfClass counts unreachable objects.  
runtime.gc();

long dalvikMax = runtime.totalMemory() / 1024;  
long dalvikFree = runtime.freeMemory() / 1024;  
long dalvikAllocated = dalvikMax - dalvikFree;

通过上面获得Java虚拟机的内存使用情况

下面是meminfo和runtime的字段对照表

<table><tbody><tr><td><p>Dalvik Heap Free</p></td><td><p>runtime.freeMemory()</p></td></tr><tr><td><p>Dalvik Heap Size</p></td><td><p>runtime.totalMemory()</p></td></tr><tr><td><p>Dalvik Heap Alloc</p></td><td><p>Dalvik Heap Size - Dalvik Heap Free</p></td></tr></tbody></table>

### **A.3 memtrack.vendor\_xxx.so**

下面是meminfo读取进程在GPU上分配内存的从上到下调用关系图。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qUGp5bFRDaWFMaGJUTlhlNXFxMDNWaWFxVThMZ3k1Z1ZoaWNmQWs5aWM2aWFScUFocmFYb1BTME10TGh6WGRVZlQ0RVNkUGRtc2liOHlEb1Fjdy82NDA?x-oss-process=image/format,png)

读取进程在GPU上分配的内存

每个厂商统计的策略可能不一样,这也是出现HAL层的原因. 下面以高通SDM710为例。从代码中可以看到也是读内核中GPU文件节点,然后解析数据的。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qUGp5bFRDaWFMaGJUTlhlNXFxMDNWaWFxaktBeFd4OXZiRlRJRFhHYWx0T1FqQndFZlJjbENHdzg1MXVhOE1pYmd0Z1FEUHE3QmhNQkdyZy82NDA?x-oss-process=image/format,png)

## **B部分** 

-   Java Heap
    

// dalvik private\_dirty  
 dalvikPrivateDirty  
// art mmap private\_dirty + private\_clean    
\+ getOtherPrivate(OTHER\_ART);

dalvik private dirty包含任何写过zygote分配的页面(应用是从zygote fork 出来的),和应用本身分配的。

art mmap是应用的bootimage,任何private页面也算在应用上。

-   Native Heap
    

nativePrivateDirty; // libc\_malloc

通过libc\_malloc库分配的大小

-   Code
    

// so mmap private\_dirty + private\_clean    
 getOtherPrivate(OTHER\_SO)    
// jar mmap private\_dirty + private\_clean      
\+ getOtherPrivate(OTHER\_JAR)  
// apk mmap private\_dirty + private\_clean        
\+ getOtherPrivate(OTHER\_APK)  
// ttf mmap private\_dirty + private\_clean        
\+ getOtherPrivate(OTHER\_TTF)  
// dex mmap private\_dirty + private\_clean        
\+ getOtherPrivate(OTHER\_DEX)  
// oat mmap private\_dirty + private\_clean  
\+ getOtherPrivate(OTHER\_OAT);

所有私有静态资源求和。

-   Stack
    

// stack private\_dirty  
getOtherPrivateDirty(OTHER\_STACK);

进程本身栈占用的大小。

-   Graphic
    

//Gfx Dev private\_dirty + private\_clean  
getOtherPrivate(OTHER\_GL\_DEV)  
// EGL mtrack private\_dirty + private\_clean        
\+ getOtherPrivate(OTHER\_GRAPHICS)  
// GL mtrack private\_dirty + private\_clean        
\+ getOtherPrivate(OTHER\_GL);

进程在GPU上分配的内存。

-   System
    

Pss Total 的和- Private Dirty 和Private Clean 的和

系统占用的内存,例如一些共享的字体,图像资源等。

## **应用**

当我们查看应用的内存消耗情况时,可以通过meminfo提供的信息,判断应用中哪个模块使用的内存出现问题。

下面是同一个应用A和B版本的meminfo：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qUGp5bFRDaWFMaGJUTlhlNXFxMDNWaWFxSUtmYkRpY0dmRzRjNVcyOWlhdFZ4dFZ2aGh3ZFR3WTdPQ2VLVGc2bTB5SnRHdWF3YWRwNzVsVVEvNjQw?x-oss-process=image/format,png)

A版本

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qUGp5bFRDaWFMaGJUTlhlNXFxMDNWaWFxQkF3Nndua0puN3Y3bEZDT21hemUzVXI4d3ZqUUYxbUREUERDYnFiRWp4THViSk9uWnVvaWNZZy82NDA?x-oss-process=image/format,png)

B版本

对比A B 版本,先看App Summary, 可以明显发现Graphic占用高,而Graphic是由Gfx, EGL 和GL组成,从两图对比看主要是EGL部分, EGL 是进程在GPU上分配的内存,查看平台对应的GPU内存调试的接口

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qUGp5bFRDaWFMaGJUTlhlNXFxMDNWaWFxTktoQm9vYnh4WFlHWjlqVm0xbmRGZkRjdTBZQ0g4cDZ6NUVHUnpiVlA3cXhiSVAzRVVJSW1nLzY0MA?x-oss-process=image/format,png)

通过对比发现B版本EGL部分主要是发生在使用egl\_image上,查看代码发现egl\_image是从hardware bitmap 分配的,这是Android O 上的一个bitmap的新特性。B 版本使用后,没有及时释放所致。

## 参考文献：

\[1\]https://developer.android.com/studio/command-line/dumpsys

\[2\]http://man7.org/linux/man-pages/man5/proc.5.html

\[3\]https://bumptech.github.io/glide/doc/hardwarebitmaps.html

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X2dpZi9kNGhvWUpseE9qTlNyNE9XSjlrdWtpYkZuc3h2U3pkSWlicjJqRDVZVDNqQU1mOWVrZDJDc0I3ME9HallxbjBKdFB3QjFtSXkxWlduQ216R1JMSzJFaWN5dy82NDA?x-oss-process=image/format,png)

**扫码关注**  
**“内核工匠”微信公众号**  
Linux 内核黑科技 | 技术文章 | 精选教程
