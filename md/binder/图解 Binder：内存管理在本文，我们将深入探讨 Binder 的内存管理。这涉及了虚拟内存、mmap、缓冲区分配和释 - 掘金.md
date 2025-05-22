---
created: 2025-05-22T13:45:59 (UTC +08:00)
tags: [Android,Linux中文技术社区,前端开发社区,前端技术交流,前端框架教程,JavaScript 学习资源,CSS 技巧与最佳实践,HTML5 最新动态,前端工程师职业发展,开源前端项目,前端技术趋势]
source: https://juejin.cn/post/7244734179828203579
author: 满嘴跑火车的小土匪
---

# 图解 Binder：内存管理在本文，我们将深入探讨 Binder 的内存管理。这涉及了虚拟内存、mmap、缓冲区分配和释 - 掘金

> ## Excerpt
> 在本文，我们将深入探讨 Binder 的内存管理。这涉及了虚拟内存、mmap、缓冲区分配和释放、物理内存页分配和释放，以及内存缩减器等机制。它们共同提升 Binder 通信的性能。

---
> 这是一系列的 Binder 文章，会从内核层到 Framework 层，再到应用层，深入浅出，介绍整个 Binder 的设计。详见《[图解 Binder：概述](https://juejin.cn/post/7244018340880007226 "https://juejin.cn/post/7244018340880007226")》。
> 
> 本文基于 Android platform 分支 android-13.0.0\_r1 和内核分支 common-android13-5.15 解析。
> 
> 一些关键代码的链接，可能会因为源码的变动，发生位置偏移、丢失等现象。可以搜索函数名，重新进行定位。

每个进程在使用 Binder 进程通信之前，都会先通过 mmap 进行映射，用于建立接收其他进程事务消息的缓冲区。mmap 是 Binder 实现进程通信一次拷贝的关键。

内核也为缓冲区的高效分配、释放，设计了复杂的数据结构。

在本文，我们将深入探讨 Binder 的内存管理。这涉及了虚拟内存、mmap、缓冲区分配和释放、物理内存页分配和释放，以及内存缩减器等机制。它们共同提升 Binder 通信的性能。

## 虚拟内存

在深入了解 Binder 的内存管理之前，我们需要先掌握一下虚拟内存的基本概念。

虚拟内存是计算机系统的一种内存管理技术，它的优势在于：

-   它可以让每个程序认为自己独占了全部的（虚拟）内存，简化了内存管理和数据保护。
-   它使得大型程序的开发变得更容易，程序员无需关心内存物理地址的分配和管理，只需要处理逻辑地址。
-   它提供了有效的内存保护机制，每个进程运行在其自己独立的地址空间内，互不干扰。
-   通过将虚拟内存分页，物理内存也按照相同大小分页，每个虚拟页面可以映射到任意物理页面，使得物理内存可以更加有效地利用。

## 虚拟寻址

在使用了虚拟内存的计算机系统里，进程拥有的是虚拟地址。在访问物理内存之前，每个虚拟地址都需要先转换为对应的物理地址。地址翻译是通过 CPU 上的内存管理单元（MMU）实现的。MMU会使用一种称为页表（Page Table）的数据结构来实现这种转换。页表本质上是一种映射关系的表，它把虚拟地址映射到物理地址。

下图展示了地址翻译的过程：

![1685487660083.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/578f8329f96b4a78ada745132934b31f~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=937&h=211&s=24699&e=png&b=fffdfd)

## 页与页框

**页**：页是虚拟内存分配的基本单位，通常页的大小是 2 的幂，如 4KB。

**页框**：即物理内存页。物理内存被划分为大小相等的块，每一块被称为页框，其大小与虚拟内存的页大小相同。

页表会记录虚拟页和物理页框之间的映射关系。当程序访问一个虚拟地址时，硬件会首先查找页表，确定这个虚拟地址属于哪一个虚拟页，然后从页表中找到对应的物理页框，然后再访问这个页框中的实际数据。

虚拟地址是由页号和页内偏移量组成的。页号用于在页表中查找相应的页表条目，该条目包含了页框的地址。页内偏移量则用于在页框中找到具体的数据。

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/66e4379a40b74c5490c17df1483de356~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1041&h=315&s=94184&e=png&b=fffdfd)

## 虚拟内存的划分

虚拟内存技术使得每个进程都有自己的虚拟地址空间。虚拟地址空间通常被分为两部分：用户空间和内核空间。

-   用户空间：这部分虚拟内存被分配给用户级的应用程序使用。应用程序的代码、数据和堆栈都位于用户空间。
    
-   内核空间：这部分虚拟内存用于操作系统内核。内核代码和数据、页表、缓存以及驱动程序都在内核空间。用户空间程序不能直接访问这部分内存。
    

这个分割通常是静态的。在 32 位的 Linux 系统中，通常最高的 1GB 虚拟内存是内核空间，而剩下的 3GB 是用户空间（这个比例在 64 位系统中会有不同，这里只讨论 32 位系统）。

![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d163c0157f774a3aaf6e86bce4d46595~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=555&h=326&s=92036&e=png&b=ffffff)

如上图，在 32 位的 Linux 系统中，通常的虚拟地址空间分布是这样的：

-   用户空间：虚拟地址范围是 0x00000000 - 0xBFFFFFFF，这部分占据了虚拟地址空间的 3GB（也就是 0-3GB ），供用户程序使用。
-   内核空间：虚拟地址范围是 0xC0000000 - 0xFFFFFFFF，这部分占据了虚拟地址空间的 1GB（也就是 3-4GB ），供内核使用。

**每个进程都有自己的用户空间，相互独立，并且所有的进程都共享同一个内核空间**。如下图：

![](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b03175c0c94449018dbb5a9eb06c2785~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=484&h=401&s=118130&e=png&b=ffffff)

内核空间有自己的页表，不会随进程切换变化。而用户空间对应进程，每个进程都有自己的用户空间，都有自己的页表。所以每当进程切换，用户空间也会切换。内核空间、不同用户空间的页表都由内核管理着。

## 用户空间、内核空间的相互访问

所有内存访问，无论是用户空间还是内核空间，都是通过虚拟地址进行的。

但是用户空间即便拥有内核空间的虚拟地址，也不能直接访问内核空间的内存，反之亦然。主要原因有两个：

-   安全性：如果用户程序可以直接访问内核空间，那么恶意程序就可能会攻击内核，破坏系统的安全性。
-   稳定性：如果用户程序可以直接更改内核数据，那么它可能会破坏内核的稳定性，导致系统崩溃。

操作系统通过在不同的权限级别上运行用户程序和内核来实现这种保护。

虽然内核有权限访问用户空间，但直接访问可能会带来一些问题，比如内核空间无法保证用户空间的数据始终有效和可用。用户进程可能会因为各种原因（如被其他进程杀死）导致其虚拟地址中的数据无效，如果内核在这种情况下直接访问用户空间的虚拟地址，就可能会发生严重的错误，如内核崩溃。所以为了保护内核，内核通常会将用户空间的数据，拷贝到内核，再进行访问。

内核里有两个函数是专门用来确保用户空间、内核空间相互访问的安全：

**copy\_from\_user**：将n个字节的用户空间内存从地址 from 拷贝到内核空间的地址 to 。

**copy\_to\_user**：将n个字节的内核空间内存从地址from拷贝到用户空间的地址to。

这两个函数的目标不仅仅是数据拷贝。它们的主要目的是在内核代码安全地访问用户空间内存时，处理各种可能出现的问题，如无效地址、地址未对齐、页面未映射等问题。

## mmap

前面我们了解了虚拟内存相关的概念。现在我们开始学习 mmap，正式开启 Binder 内存管理的旅程。

我们先来了解一下传统 IO 的 write/read 和 mmap 的差异。

## 一次拷贝的 write/read

read 和 write 每次调用都会发生一次数据拷贝。以 read 系统调用为例：当用户进程读取磁盘文件时，它会调用 read。这时，文件数据会从磁盘读取到内核空间的一个缓冲区，再拷贝到用户空间。

![](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7596dd5638d442a8a83958cbb5e8d220~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=774&h=627&s=204014&e=png&b=fffefe)

> “拷贝”通常指的是将数据从一个内存区域复制到另一个内存区域，这个过程会消耗 CPU 资源。
> 
> 读取/写入文件、设备的时候，通常会采用一个叫 DMA（Direct Memory Access）的硬件技术。它允许设备（比如硬盘或网络接口）直接读取和写入主内存，无需经过 CPU，从而避免了传统意义上的“拷贝”。
> 
> 因此，我们不把 DMA 算作“拷贝”。因为它不涉及 CPU，可以直接从一个地方传输数据到另一个地方。

## 零拷贝的 mmap

mmap 函数本身其实并不涉及到数据的拷贝。**mmap 是一种内存映射技术**：它将用户空间的一段虚拟地址和内核空间的一段虚拟地址，映射到同一块物理内存上，即让用户空间、内核空间共享这块物理内存。

在读取磁盘文件时，我们先用 mmap 完成映射，然后再读取文件到内核空间的缓冲区。由于用户空间和内核空间共享这块物理内存，所以，进程通过用户空间的虚拟地址，直接就可以读取到文件数据，避免了 copy\_to\_user，所以是“零拷贝”：

![](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/cec72e16dc3f45e1a4553d979734ab9d~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=770&h=629&s=220245&e=png&b=fefcfc)

## mmap 的优缺

当调用 mmap 时，mmap 通常会映射较大的一块物理内存。但操作系统通常不会立马分配对应的物理内存，而是创建一个映射表，将用户空间、内核空间的虚拟地址与物理地址相关联。当你真正访问这个内存区域的时候，可能触发缺页（page fault），此时操作系统才会分配对应的物理内存页，再从硬盘上把数据读入到物理内存。

相比之下，当调用 read 函数时，我们需要的缓冲区一般是 4KB，也就是一个物理页，不会像 mmap 那样，产生大量的缺页和建立页表映射的开销。

但是，这并不意味着 read/write 比 mmap 更高效。比如 read，它每次都需要进行数据拷贝，而 mmap 只在第一次访问时触发缺页，如果这个内存区域被频繁访问，那么 mmap 在长期运行下来可能更高效。此外，mmap 也支持多个进程共享同一个文件映射，这是 read 无法做到的。

总的来说，mmap 和 read/write 各有优劣，适用的场景也有所不同。在处理大文件的时候，mmap 显然更有优势。

## Binder 的 mmap

## Binder 进程通信一次拷贝的秘密

当客户端想要发送数据到服务端数据时，常规方式就是先从客户端进程拷贝数据到内核，再从内核拷贝数据到服务端进程，发生了两次内存拷贝：

![](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c181487173524f68a7a008aca98e0672~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1132&h=576&s=289570&e=png&b=fffefe)

而在 Binder 驱动中，服务端进程会事先调用 mmap 将服务端的用户空间的一段虚拟内存和内核空间的一段虚拟内存，映射到同一块物理内存上。这段物理内存，将作为一个缓冲区，接收来自不同客户端的数据。比如，客户端发送事务数据时，内核会通过 copy\_from\_user 将数据放进缓冲区，然后通知服务端。服务端只需要通过用户空间相应的虚拟地址，就能直接访问数据：

![](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/45201917431148d8be70dc928b012a2a~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1127&h=565&s=295806&e=png&b=fffdfd)

当 Android 的进程第一次通过 Binder 框架进行 IPC 时，就会触发 [ProcessState 的初始化](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bl%3D485%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;l=485;bpv=1;bpt=0")，调用 Binder 驱动的 mmap。

```
mVMStart = mmap(nullptr, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE,
                        opened.value(), 0);
```

上面的代码，是调用 mmap 映射的内存大小，大小为[BINDER\_VM\_SIZE](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp%3Bl%3D47 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp;l=47")，即 1MB - 2 Page：

```
#define BINDER_VM_SIZE ((1 * 1024 * 1024) - sysconf(_SC_PAGE_SIZE) * 2)
```

不设为 1MB 的原因，可以看 Android 的 [git commit](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2F_%2Fandroid%2Fplatform%2Fframeworks%2Fnative%2F%2B%2Fc0c1092183ceb38dd4d70d2732dd3a743fefd567 "https://cs.android.com/android/_/android/platform/frameworks/native/+/c0c1092183ceb38dd4d70d2732dd3a743fefd567") 记录：

> Modify the binder to request 1M - 2 pages instead of 1M. The backing store in the kernel requires a guard page, so 1M allocations fragment memory very badly. Subtracting a couple of pages so that they fit in a power of two allows the kernel to make more efficient use of its virtual address space.

这一块内存，就是每个使用 Binder 通信的进程都会事先建立的一块读写缓冲区。也是 Binder 驱动实现跨进程通信只需要一次数据拷贝的关键所在。

## Binder 的 mmap 实现

在 Linux 系统中，mmap 系统调用是通过虚拟文件系统（VFS）层和内核的内存管理子系统来实现的。

当进程执行 mmap 系统调用时，内核会检查目标文件的类型，并调用相应文件系统的 mmap 实现。对于普通的磁盘文件，这通常是文件系统驱动的 mmap 实现。对于设备文件，如 /dev/binder ，就是 Binder 驱动的 mmap 实现。

### Binder 的 mmap 调用链路

下面是一个 Binder 驱动的 mmap 实现的调用链路：

-   [mmap()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fbionic%2Fmmap.cpp%3Bl%3D58%3Bdrc%3D5ca657189aac546af0aafaba11bbc9c5d889eab3%3Bbpv%3D1%3Bbpt%3D1 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/bionic/mmap.cpp;l=58;drc=5ca657189aac546af0aafaba11bbc9c5d889eab3;bpv=1;bpt=1")
    -   [bionic/libc/SYSCALLS.TXT](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2FSYSCALLS.TXT%3Bl%3D180 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/SYSCALLS.TXT;l=180")
        -   [\_\_NR\_mmap](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Fuapi%2Fasm-generic%2Funistd.h%3Bl%3D908 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/uapi/asm-generic/unistd.h;l=908")
            -   [\_\_NR3264\_mmap](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Fuapi%2Fasm-generic%2Funistd.h%3Bl%3D645 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/uapi/asm-generic/unistd.h;l=645")
                -   [sys\_mmap()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Farch%2Fia64%2Fkernel%2Fsys_ia64.c%3Bl%3D149 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/arch/ia64/kernel/sys_ia64.c;l=149")
                    -   [ksys\_mmap\_pgoff()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fmm%2Fmmap.c%3Bl%3D1592 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/mm/mmap.c;l=1592")
                        -   [vm\_mmap\_pgoff()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fmm%2Futil.c%3Bl%3D541 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/mm/util.c;l=541")
                            -   [do\_mmap()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fmm%2Fmmap.c%3Bl%3D1413 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/mm/mmap.c;l=1413")
                                -   [get\_unmapped\_area()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fmm%2Fmmap.c%3Bl%3D2239 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/mm/mmap.c;l=2239") // 从当前进程的用户空间中找到一片空闲区域，返回这块区域的起始地址
                                -   [mmap\_region()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fmm%2Fmmap.c%3Bl%3D1729 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/mm/mmap.c;l=1729")
                                    -   [vm\_area\_alloc()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fkernel%2Ffork.c%3Bl%3D355 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/kernel/fork.c;l=355") // 用 slab 给 vm\_area\_struct 实例分配内存
                                    -   [get\_file()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Ffs.h%3Bl%3D1036 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/fs.h;l=1036")
                                    -   [call\_mmap()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Ffs.h%3Bl%3D2162 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/fs.h;l=2162") // 调用相应文件系统的 mmap 实现
                                        -   [binder\_mmap()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5698 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5698")
                                            -   [binder\_alloc\_mmap\_handler()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2F_%2Fandroid%2Fkernel%2Fcommon%2F%2B%2Fandroid13-5.15%3Adrivers%2Fandroid%2Fbinder_alloc.c%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D739 "https://cs.android.com/android/_/android/kernel/common/+/android13-5.15:drivers/android/binder_alloc.c;bpv=0;bpt=0;l=739")

Binder 驱动的 mmap 实现，就是 [binder\_mmap()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D5698 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=5698")。

调用 mmap 后，会从用户空间划分一块虚拟内存，最后在调用 Binder 驱动的 binder\_mmap 时，也会从内核空间划分一块虚拟内存出去。它们会映射到同一块物理内存（在 Binder 通信时，才会正式分配需要的物理内存）。这块内存作为一块缓冲区，专门用来接收、处理与该进程相关的 Binder 通信数据。如下图：

![](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/31b0c4da71934f0eb32617274019e4ad~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=623&h=306&s=110080&e=png&b=fefefe)

-   [vm\_area\_struct](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Fmm_types.h%3Bl%3D336 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/mm_types.h;l=336")：描述一块用户空间的虚拟内存，包括起始地址、结束地址等。
-   [vm\_struct](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Fvmalloc.h%3Bl%3D48 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/vmalloc.h;l=48")：描述一块内核空间的虚拟内存，包括起始地址、内存区域大小等。

基于 mmap 机制，服务端启动的时候，我们会为它创建一个缓冲区，用户空间、内核空间都有一块虚拟内存映射到它，服务端进程和内核都可以访问这个缓冲区，但是使用的是不同的虚拟地址（用户空间地址和内核空间地址）。

当内核接收来自客户端的事务数据时，先从缓冲区中分配一块 buffer，然后通过 copy\_from\_user 将数据拷贝到该 buffer 指向的物理内存上。这时候，内核操作的是这块 buffer 在 vm\_struct 里的地址，也就是内核空间的虚拟地址。最后内核利用偏移量计算出该 buffer 在 vm\_area\_struct 里的地址，也就是用户空间的虚拟地址，再通知服务端接收。这期间没有 copy\_to\_user，只需要一次拷贝。

### 用户空间、内核空间的虚拟地址切换

Binder 会计算并记录 vm\_area\_struct、vm\_struct 两块虚拟内存的偏移，示例代码可参考 Android9 的 [binder\_alloc\_mmap\_handler()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fp-common-android-4.9%3Acommon%2Fdrivers%2Fandroid%2Fbinder_alloc.c%3Bl%3D661%3Bbpv%3D0%3Bbpt%3D0 "https://cs.android.com/android/kernel/superproject/+/p-common-android-4.9:common/drivers/android/binder_alloc.c;l=661;bpv=0;bpt=0")：

```
int binder_alloc_mmap_handler(struct binder_alloc *alloc,
      struct vm_area_struct *vma)
{
    struct vm_struct *area;
    // 从内核空间分配一块和 vma 大小相等的虚拟内存
    area = get_vm_area(vma->vm_end - vma->vm_start, VM_ALLOC);
    // area->addr 就是 vma 的起始地址
    alloc->buffer = area->addr;
    // 计算 vma 和 vm 之间的偏移量，记录在 alloc->user_buffer_offset 里
    alloc->user_buffer_offset = vma->vm_start - (uintptr_t)alloc->buffer;
}
```

这样，通过偏移量，我们就可以通过用户空间的虚拟地址，快速获得对应的内核空间的虚拟地址：

```
void *kern_ptr;
kern_ptr = (void *)(user_ptr - alloc->user_buffer_offset);
```

或者通过内核空间的虚拟地址，快速获得对应的用户空间的虚拟地址：

```
unsigned long user_page_addr;
user_page_addr = (uintptr_t)page_addr + alloc->user_buffer_offset
```

## 新的 binder\_mmap 实现

在 Android10 以后，binder\_mmap 的实现有了比较大的改动，取消了 vm\_struct 的使用：

1.  [binder\_alloc\_mmap\_handler()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2F_%2Fandroid%2Fkernel%2Fcommon%2F%2B%2Fandroid13-5.15%3Adrivers%2Fandroid%2Fbinder_alloc.c%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D739 "https://cs.android.com/android/_/android/kernel/common/+/android13-5.15:drivers/android/binder_alloc.c;bpv=0;bpt=0;l=739") 不再从内核空间分配一块和 vma 大小相等的虚拟内存，也不再记录什么偏移量。
2.  将客户端数据拷贝到缓冲区的时候，是逐个物理页处理的：先通过 kmap 建立内核空间虚拟地址和物理内存页的映射，然后通过 copy\_from\_user 按页拷贝，最后再 kunmap 取消映射。详见 [binder\_alloc\_copy\_user\_to\_buffer()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2F_%2Fandroid%2Fkernel%2Fcommon%2F%2B%2Fandroid13-5.15%3Adrivers%2Fandroid%2Fbinder_alloc.c%3Bl%3D1207%3Bbpv%3D0%3Bbpt%3D0 "https://cs.android.com/android/_/android/kernel/common/+/android13-5.15:drivers/android/binder_alloc.c;l=1207;bpv=0;bpt=0")。

```
unsigned long
binder_alloc_copy_user_to_buffer(struct binder_alloc *alloc,
 struct binder_buffer *buffer,
 binder_size_t buffer_offset,
 const void __user *from,
 size_t bytes)
{
    while (bytes) {
        unsigned long size;
        unsigned long ret;
        struct page *page;
        pgoff_t pgoff;
        void *kptr;
        // buffer 是从缓冲区里划分的一小块，用来接收客户端数据
        // buffer 已经分配物理内存，还通过 vm_insert_page() 将用户空间虚拟地址与物理页绑定
        page = binder_alloc_get_page(alloc, buffer,
                buffer_offset, &pgoff);
        size = min_t(size_t, bytes, PAGE_SIZE - pgoff);
        // 通过 kmap ，将物理页映射到内核空间，并返回该内存页的内核空间虚拟地址
        kptr = kmap(page) + pgoff;
        // 逐页拷贝
        ret = copy_from_user(kptr, from, size);
        // 取消映射
        kunmap(page);
        if (ret)
            return bytes - size + ret;
            bytes -= size;
            from += size;
            buffer_offset += size;
    }
    return 0;
}
```

取消 vm\_struct 的使用，最大的好处是极大地避免了耗尽内核可用的地址空间。

但取消了记录偏移量，又如何获得对应的用户空间的虚拟地址呢？

当从缓冲区分配一块处理事务数据的 buffer 时，就会为 buffer 分配对应的物理页。buffer 可以简单理解为一段连续的用户空间虚拟地址。为其分配物理页的时候，会逐页绑定到对应的虚拟地址上。这是发生在上面代码的 binder\_alloc\_get\_page() 之前的。kmap 再将物理页临时映射到内核空间，那就相当于将用户空间虚拟地址、内核空间虚拟地址映射到了同一个物理页。内核通过内核空间虚拟地址调用 copy\_from\_user 把数据从客户端拷贝到 buffer 的物理页后，内核空间虚拟地址就用不上，可以通过 kunmap 取消映射。后续，服务端通过 buffer 的用户空间虚拟地址，就可以访问到对应物理页上的数据。

## Binder 内存管理

Binder 的内存管理，指的就是管理 mmap 映射的这块缓冲区。Binder 驱动为这块缓冲区，设计了复杂的数据结构，实现了高效查询、分配和。

## 缓冲区分配器：binder\_alloc

每个使用 Binder 通信的 Android 进程，都会事先建立一个缓冲区。它用 [binder\_alloc](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2F_%2Fandroid%2Fkernel%2Fcommon%2F%2B%2Fandroid13-5.15%3Adrivers%2Fandroid%2Fbinder_alloc.h%3Bl%3D101%3Bbpv%3D0%3Bbpt%3D0 "https://cs.android.com/android/_/android/kernel/common/+/android13-5.15:drivers/android/binder_alloc.h;l=101;bpv=0;bpt=0") 来管理。

binder\_alloc 的部分定义如下：

```
struct binder_alloc {
struct vm_area_struct *vma;
struct mm_struct *vma_vm_mm;
void __user *buffer;
struct list_head buffers;
struct rb_root free_buffers;
struct rb_root allocated_buffers;
struct binder_lru_page *pages;
size_t buffer_size;
};
```

-   vma：一个指针，指向的 vm\_area\_struct就是 mmap 调用时分配的。vm\_area\_struct 描述了一块用户空间的虚拟内存，包括起始地址、结束地址等，也就是 Binder 的缓冲区。
-   vma\_vm\_mm：mm\_struct 描述的是该进程的用户空间及相关信息。
-   buffer：vm\_area\_struct 的起始地址。
-   buffers：一个双向循环链表，管理着从缓冲区里划分的所有 binder\_buffer，**按 buffer 的起始的用户空间虚拟地址排序**。一开始缓冲区没有被拆分，buffers 只有一个 binder\_buffer，代表整个缓冲区。
-   free\_buffers：一棵红黑树，管理着所有可以分配的 binder\_buffer，树里**按 buffer 的大小排序**，目的是为了根据事务数据的大小在 O(logn) 时间复杂度内找到可以接受事务数据的空闲 binder\_buffer。
-   allocated\_buffers：一棵红黑树，管理着所有已分配的 binder\_buffer，树里**按 buffer 的起始的用户空间虚拟地址排序**，目的是为了根据 buffer 的起始地址在 O(logn) 时间复杂度内找到目标 binder\_buffer，进行释放。
-   pages：一个数组，每个元素都是一个指针，指向一个 binder\_lru\_page。binder\_lru\_page 对应一个物理页，被组织成一个双向循环链表，目的是在系统内存不足时，高效回收物理页。
-   buffer\_size：描述整个缓冲区的大小。

## 缓冲区：binder\_buffer

Binder 驱动用 [binder\_buffer](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2F_%2Fandroid%2Fkernel%2Fcommon%2F%2B%2Fandroid13-5.15%3Adrivers%2Fandroid%2Fbinder_alloc.h%3Bl%3D42%3Bbpv%3D0%3Bbpt%3D0 "https://cs.android.com/android/_/android/kernel/common/+/android13-5.15:drivers/android/binder_alloc.h;l=42;bpv=0;bpt=0") 来描述缓冲区。

```
struct binder_buffer {
struct list_head entry;
struct rb_node rb_node;
        unsigned free:1;
unsigned clear_on_free:1;
unsigned allow_user_free:1;
unsigned async_transaction:1;
unsigned oneway_spam_suspect:1;
unsigned debug_id:27;
struct binder_transaction *transaction;
struct binder_node *target_node;
size_t data_size;
size_t offsets_size;
size_t extra_buffers_size;
void __user *user_data;
int    pid;
};
```

-   entry：表示链表中的一个节点。将 binder\_buffer 插入 binder\_alloc 的 buffers 链表时，就是将该 entry 插入链表中。遍历链表时，拿到该 entry，即可通过 [container\_of()](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fwzc18743083828%2Farticle%2Fdetails%2F118730678 "https://blog.csdn.net/wzc18743083828/article/details/118730678") 获取对应的 binder\_buffer。
-   rb\_node：表示红黑树中的一个节点，和 entry 类似，用于在 binder\_alloc 的 free\_buffers 和 allocated\_buffers 中表示该 binder\_buffer。
-   free - debug\_id：free、clear\_on\_free、async\_transaction 和 oneway\_spam\_suspect 这几个字段，主要是一些标志位。free 标识该 binder\_buffer 是空闲的，:1 表示该字段是位字段，只占 1 个比特位。debug\_id 是用于调试的唯一ID。
-   transaction：指向与该缓冲区关联的 binder\_transaction。
-   target\_node：指向与该缓冲区关联的 binder\_node。
-   data\_size：transaction 数据的大小。
-   offsets\_size：offsets 数组的大小。
-   extra\_buffers\_size：其他对象（如 sg 列表）的空间大小。
-   user\_data：用户空间的虚拟地址，指向该缓冲区的起始位置。
-   pid：所属进程的进程 ID。

### binder\_buffer 的大小

结构体 binder\_buffer 并没有记录它表示的缓冲区的大小。大体上，缓冲区的大小等于 data\_size + offsets\_size + extra\_buffers\_size 。但由于字节对齐，缓冲区的大小可能会比这三个字段的总和稍大一点。Binder 驱动利用 buffers 链表，采用了比较巧妙的方式计算一个 binder\_buffer 的大小，详见 [binder\_alloc\_buffer\_size()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_alloc.c%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D61 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_alloc.c;bpv=0;bpt=0;l=61")：

```
static size_t binder_alloc_buffer_size(struct binder_alloc *alloc,
       struct binder_buffer *buffer)
{
    // 该 binder_buffer 是链表的最后一个节点
    if (list_is_last(&buffer->entry, &alloc->buffers))
        // alloc->buffer + alloc->buffer_size 就是该进程的整个缓冲区的结束地址
        // 用整个缓冲区的结束地址减去该 binder_buffer 起始地址，就可以得出它的大小
        return alloc->buffer + alloc->buffer_size - buffer->user_data;
    // 该 binder_buffer 不是链表的最后一个节点
    // 用后续节点表示的 binder_buffer 的结束地址减去该 binder_buffer 起始地址，就可以得出它的大小
    return binder_buffer_next(buffer)->user_data - buffer->user_data;
}
```

## 缓冲区初始化

Binder 驱动其实一开始是不会为缓冲区分配对应的物理内存的，只是先分配了一段用户空间的虚拟地址，即只是先分配了虚拟内存。

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/2aae579325be40da8e163be807ffa309~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=665&h=541&s=26619&e=png&b=ffffff)

注：allocated\_buffers 红黑树此时为空树。

初始化代码主要在 [binder\_alloc\_mmap\_handler()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_alloc.c%3Bl%3D739 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_alloc.c;l=739") 里：

```
int binder_alloc_mmap_handler(struct binder_alloc *alloc,
      struct vm_area_struct *vma)
{
    struct binder_buffer *buffer;
    // 缓冲区大小最大不会超过 4MB
    alloc->buffer_size = min_t(unsigned long, vma->vm_end - vma->vm_start,
   SZ_4M);
    // 缓冲区的起始地址就是 vma->vm_start
    alloc->buffer = (void __user *)vma->vm_start;

    alloc->pages = kcalloc(alloc->buffer_size / PAGE_SIZE,
       sizeof(alloc->pages[0]),
       GFP_KERNEL);
    // 为一个 binder_buffer 对象分配内存
    buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);

    buffer->user_data = alloc->buffer;
    // 将新创建的 binder_buffer 插入 buffers链表中
    list_add(&buffer->entry, &alloc->buffers);
    buffer->free = 1;
    // 将新创建的 binder_buffer 插入到 free_buffers 红黑树
    binder_insert_free_buffer(alloc, buffer);
    alloc->free_async_space = alloc->buffer_size / 2;

    binder_alloc_set_vma(alloc, vma);

    return 0;
}
```

## 缓冲区分配

在处理事务的时候，会从缓冲区划分一块 binder\_buffer 出来处理事务。这时候，才会为这块小缓冲区分配物理内存。

为事务分配缓冲区，有两种情况：

1.  查询 free\_buffers 红黑树，找不到大小刚好合适的缓冲区，就找大于事务数据大小的最小空闲缓冲区进行切割：

![1685935235667.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7e87ddf8f95c487990f41da9a4778b01~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1400&h=567&s=87034&e=png&b=ffffff)

-   ① 查询了 free\_buffers 红黑树，大于事务数据大小的最小空闲缓冲区就是 binder\_buffer0。对其进行切割，如图，binder\_buffer0 标识为已使用，切割出来的 binder\_buffer1 标识为空闲的。
-   ② 将 binder\_buffer1 插入 buffers 链表，插入到 binder\_buffer0 后面
-   ③ 从 free\_buffers 红黑树中移除 binder\_buffer0，并插入 binder\_buffer1。
-   ④ 将 binder\_buffer0 插入到 allocated\_buffers 红黑树中。

2.  查询 free\_buffers 红黑树，刚好找到合适的缓冲区，直接使用它（下图的缓冲区是经过一段时间使用形成的）：

![1685935384312.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8d30b5b8d6224ec3aeb3e04e2b23bc8c~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1409&h=701&s=186669&e=png&b=fffdfd)

-   ① 查询了 free\_buffers 红黑树，刚好等于事务数据大小的空闲缓冲区就是 binder\_buffer1。将binder\_buffer1 标识为已使用。
-   ② binder\_buffer1 已经在 buffers 链表中，无需做任何更改。
-   ③ 从 free\_buffers 红黑树中移除 binder\_buffer1
-   ④ 将 binder\_buffer1 插入到 allocated\_buffers 红黑树中。

缓冲区分配的代码主要在 [binder\_alloc\_new\_buf\_locked()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_alloc.c%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D371 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_alloc.c;bpv=1;bpt=0;l=371") 中：

```
static struct binder_buffer *binder_alloc_new_buf_locked(
    struct binder_alloc *alloc,
    size_t data_size,
    size_t offsets_size,
    size_t extra_buffers_size,
    int is_async,
    int pid)
{
    // ALIGN 宏用于将一个值向上对齐到指定的对齐边界。以避免任何潜在的内存对齐问题。
    // 在这里，对齐边界是 sizeof(void *)，通常是 4 字节（32 位系统）或 8 字节（64 位系统）。
    data_offsets_size = ALIGN(data_size, sizeof(void *)) +
    ALIGN(offsets_size, sizeof(void *));
                
    size = data_offsets_size + ALIGN(extra_buffers_size, sizeof(void *));

    /* Pad 0-size buffers so they get assigned unique addresses */
    size = max(size, sizeof(void *));
    
    // 遍历红黑树，寻找合适的 buffer
    while (n) {
        buffer = rb_entry(n, struct binder_buffer, rb_node);
        BUG_ON(!buffer->free);
        buffer_size = binder_alloc_buffer_size(alloc, buffer);

        // 比需要的size大，纳入考虑，但还是要继续寻找有没有更合适的，即大小更接近，遍历左子树
        if (size < buffer_size) {
            best_fit = n;
            n = n->rb_left;
        } else if (size > buffer_size) // 比需要的 size 小，不符合，遍历右子树
            n = n->rb_right;
        else {
            // 刚好 size == buffer_size，不需要再找了
            best_fit = n;
            break;
        }
    }
            
    // 在前面遍历红黑树的时候，size < buffer_size，会遍历左子树，
    // 这时候，遍历完左子树，都可能没有找到比之前更合适的，最终 n 会变为 NULL，
    // 而 buffer 在遍历过程中，也会被赋值为左子树节点的值。
    // 所以，这里需要更新为之前找到的 best_fit 的值。
    if (n == NULL) {
        buffer = rb_entry(best_fit, struct binder_buffer, rb_node);
        buffer_size = binder_alloc_buffer_size(alloc, buffer);
    }

    // 计算出的地址（has_page_addr）表示找到的 buffer 所占据的内存页的最后一个内存页的起始地址。
    has_page_addr = (void __user *)
    (((uintptr_t)buffer->user_data + buffer_size) & PAGE_MASK);
    // 计算出的地址（end_page_addr）表示实际需要的 buffer 的内存页的最后一个内存页的结束地址。
    end_page_addr =
    (void __user *)PAGE_ALIGN((uintptr_t)buffer->user_data + size);
    if (end_page_addr > has_page_addr)
        end_page_addr = has_page_addr;
                
    // 为 buffer 分配物理内存页
    ret = binder_update_page_range(alloc, 1, (void __user *)
    PAGE_ALIGN((uintptr_t)buffer->user_data), end_page_addr);

    // 分配的 buffer 比实际需要的小
    if (buffer_size != size) {
        struct binder_buffer *new_buffer;

        new_buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
        // 把多出来的内存放进一个新的 new_buffer 里
        new_buffer->user_data = (u8 __user *)buffer->user_data + size;
        // 插入双向链表 buffers 中：插入到原来的 buffer 后面
        list_add(&new_buffer->entry, &buffer->entry);
        // 标记为空闲的
        new_buffer->free = 1;
        // 将 new_buffer 插入到红黑树 free_buffers 中
        binder_insert_free_buffer(alloc, new_buffer);
    }
    // 从红黑树 free_buffers 移除已分配的 buffer
    rb_erase(best_fit, &alloc->free_buffers);
    // 标记为已使用
    buffer->free = 0;
    buffer->allow_user_free = 0;
    // 将已分配的 buffer 插入到红黑树 allocated_buffers 中
    binder_insert_allocated_buffer_locked(alloc, buffer);
    buffer->data_size = data_size;
    buffer->offsets_size = offsets_size;
    buffer->async_transaction = is_async;
    buffer->extra_buffers_size = extra_buffers_size;
    buffer->pid = pid;
    buffer->oneway_spam_suspect = false;
    return buffer;
}
```

## 缓冲区释放

处理完事务之后，就会释放为事务分配的缓冲区：

![1685944014863.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1d37ea3750334def9747ad61cf5e9d53~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1408&h=716&s=91661&e=png&b=ffffff)

-   ① binder\_buffer1 释放后，由于 binder\_buffer1 的前后都是空闲的 buffer，会进行合并，即 binder\_buffer0、binder\_buffer1 和 binder\_buffer2 合并，只剩 binder\_buffer0。
    
-   ② buffers 链表处理：
    
    -   发现 binder\_buffer1 不是尾节点，并且后续节点 binder\_binder2 是空闲的，直接移除 binder\_buffer2 节点。
    -   发现 binder\_buffer1 不是头节点，并且前驱节点 binder\_binder0 是空闲的，这时候，移除的不是 binder\_binder0，而是 binder\_buffer1 节点。
-   ③ 在处理 buffers 链表的时候，会同步更新 free\_buffers 红黑树，所以最终 free\_buffers 红黑树只剩 binder\_buffer0 节点。
    
-   ④ 从 allocated\_buffers 红黑树中移除 binder\_buffer1。
    

缓冲区释放的代码主要在 [binder\_free\_buf\_locked()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_alloc.c%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D641 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_alloc.c;bpv=0;bpt=0;l=641")：

```
static void binder_free_buf_locked(struct binder_alloc *alloc,
   struct binder_buffer *buffer)
{
    size_t size, buffer_size;

    buffer_size = binder_alloc_buffer_size(alloc, buffer);

    size = ALIGN(buffer->data_size, sizeof(void *)) +
ALIGN(buffer->offsets_size, sizeof(void *)) +
ALIGN(buffer->extra_buffers_size, sizeof(void *));

    // 释放 buffer 对应的物理内存页
    binder_update_page_range(alloc, 0,
(void __user *)PAGE_ALIGN((uintptr_t)buffer->user_data),
(void __user *)(((uintptr_t)
  buffer->user_data + buffer_size) & PAGE_MASK));
    // 将释放的 buffer 从红黑树 allocated_buffers 中移除
    rb_erase(&buffer->rb_node, &alloc->allocated_buffers);
    // 将 buffer 标识为空闲的
    buffer->free = 1;
    // 如果该 buffer 不是链表中的最后一个节点，尝试和后续节点进行合并
    if (!list_is_last(&buffer->entry, &alloc->buffers)) {
        struct binder_buffer *next = binder_buffer_next(buffer);
        // 如果后续节点是未被使用的 buffer ，将这两个 buffer 合并。
        // 合并方式很简单，将后续节点移除即可。
        if (next->free) {
            rb_erase(&next->rb_node, &alloc->free_buffers);
            binder_delete_free_buffer(alloc, next);
        }
    }
    // buffers 是个双向循环链表，当 alloc->buffers.next == buffer->entry 时，
    // 释放的 buffer 就是链表里的唯一节点，prev、next都是指向它自己，不需要合并
    if (alloc->buffers.next != &buffer->entry) {
        struct binder_buffer *prev = binder_buffer_prev(buffer);
        // 如果前驱节点是未被使用的 buffer ，将这两个 buffer 合并。
        // 合并方式不是将前驱节点移除，而是将代表释放 buffer 的节点移除
        if (prev->free) {
            binder_delete_free_buffer(alloc, buffer);
            rb_erase(&prev->rb_node, &alloc->free_buffers);
            // 将 buffer 更新为 prev
            buffer = prev;
        }
    }
    // 将释放的 buffer 插入到红黑树 free_buffers 中
    binder_insert_free_buffer(alloc, buffer);
}
```

## 分配/释放物理内存页

分配、释放一个 binder\_buffer 的物理内存页，都是在 [binder\_update\_page\_range()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_alloc.c%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D182 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_alloc.c;bpv=0;bpt=0;l=182") 里进行的：

```
static int binder_update_page_range(struct binder_alloc *alloc, int allocate,
    void __user *start, void __user *end)
{
    void __user *page_addr;
    unsigned long user_page_addr;
    struct binder_lru_page *page;
    struct vm_area_struct *vma = NULL;
    struct mm_struct *mm = NULL;
    bool need_mm = false;

    // allocate ：为 0 时是释放物理内存页，为 1 时是分配物理内存页
    if (allocate == 0)
        goto free_range;

    ......

    // 以页为单位，遍历 binder_buffer 的所有页（page_addr 就是当前处理的页的用户空间虚拟地址）
    for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
        int ret;
        bool on_lru;
        size_t index;

        index = (page_addr - alloc->buffer) / PAGE_SIZE;
        page = &alloc->pages[index];
        // page_ptr 不为空，之前已分配物理内存页，所以只是从 binder_alloc_lru 链表中移除即可
        if (page->page_ptr) {
            on_lru = list_lru_del(&binder_alloc_lru, &page->lru);
            continue;
        }
        // alloc_page() 分配一个物理内存页，返回值是指向该物理内存页 page 的指针
        page->page_ptr = alloc_page(GFP_KERNEL |
    __GFP_HIGHMEM |
    __GFP_ZERO);
        page->alloc = alloc;
        INIT_LIST_HEAD(&page->lru);

        user_page_addr = (uintptr_t)page_addr;
        // vm_insert_page() 将一个用户空间虚拟地址，绑定到指定的物理内存页上
        ret = vm_insert_page(vma, user_page_addr, page[0].page_ptr);

        if (index + 1 > alloc->pages_high)
            alloc->pages_high = index + 1;
    }
    return 0;

// 释放 binder_buffer 的物理内存页
free_range:
    for (page_addr = end - PAGE_SIZE; 1; page_addr -= PAGE_SIZE) {
        bool ret;
        size_t index;

        index = (page_addr - alloc->buffer) / PAGE_SIZE;
        page = &alloc->pages[index];
        // 只是简单得把物理内存页添加至链表 binder_alloc_lru 
        ret = list_lru_add(&binder_alloc_lru, &page->lru);
        if (page_addr == start)
            break;
    }
    return vma ? -ENOMEM : -ESRCH;
}
```

-   分配：以页为单位，遍历 binder\_buffer 的所有页（page\_addr 就是当前处理的页的用户空间虚拟地址）
    
    -   如果之前已经为该页分配物理内存页，就复用，只需要从 binder\_alloc\_lru 链表里移除即可。复用的好处是，减少了分配、映射的消耗。
    -   如果没有为该页分配过物理内存页，就通过 alloc\_page() 分配，然后用 vm\_insert\_page() 建立映射。
-   释放：以页为单位，遍历 binder\_buffer 的所有页，不取消映射、也不回收物理内存页，只是逐一地插入到 binder\_alloc\_lru 的尾部。
    

结构体 [page](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Fmm_types.h%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D75 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/mm_types.h;bpv=0;bpt=0;l=75") 就是用来描述一个物理内存页（即页框）的。

[binder\_alloc\_lru](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_alloc.c%3Bl%3D30%3Bbpv%3D0%3Bbpt%3D0 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_alloc.c;l=30;bpv=0;bpt=0") 是一个 lru 链表，主要用于内存不足时，回收物理内存页，所有用户进程共享这个 lru 链表。结构体 [binder\_lru\_page](https://link.juejin.cn/?target=69 "69") 描述了该 lru 链表的一个结点，记录着对应物理内存页的信息。

注意，虽然是多个进程共同使用 binder\_alloc\_lru。但是，由某个进程插入到binder\_alloc\_lru的页，只能由它自己再次获取并使用，不能被其他进程获取到并使用。

## 内存缩减器

内存缩减器是一种用于回收未使用的内存页面的机制，它们在系统内存紧张时被调用，回收最近最久未使用的空闲物理内存页。

Binder 驱动初始化时，即 [binder\_init()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D6675 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=6675") 被调用时，会通过 [binder\_alloc\_shrinker\_init()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2F_%2Fandroid%2Fkernel%2Fcommon%2F%2B%2Fandroid13-5.15%3Adrivers%2Fandroid%2Fbinder_alloc.c%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D1085 "https://cs.android.com/android/_/android/kernel/common/+/android13-5.15:drivers/android/binder_alloc.c;bpv=0;bpt=0;l=1085") 注册 Binder 内存缩减器。

```
struct list_lru binder_alloc_lru; 
// binder_shrinker定义了 Binder 驱动程序的内存缩减器。这里声明了缩减器的两个回调函数： 
// count_objects 和 scan_objects 
static struct shrinker binder_shrinker = { 
    .count_objects = binder_shrink_count, 
    .scan_objects = binder_shrink_scan, 
    .seeks = DEFAULT_SEEKS, 
};

int binder_alloc_shrinker_init(void)
{
    // 初始化 binder_alloc_lru 链表
    int ret = list_lru_init(&binder_alloc_lru);

    if (ret == 0) {
        // 注册 Binder 驱动的内存缩减器
        // 注册缩减器后，内核就可以在需要时调用其 scan_objects 和 count_objects 回调函数。
        ret = register_shrinker(&binder_shrinker);
        if (ret)
            list_lru_destroy(&binder_alloc_lru);
    }
    return ret;
}
```

在系统内存不足的时候，binder\_shrink\_scan() 会被调用：

-   [binder\_shrink\_scan()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2F_%2Fandroid%2Fkernel%2Fcommon%2F%2B%2Fandroid13-5.15%3Adrivers%2Fandroid%2Fbinder_alloc.c%3Bl%3D1054%3Bbpv%3D0%3Bbpt%3D0 "https://cs.android.com/android/_/android/kernel/common/+/android13-5.15:drivers/android/binder_alloc.c;l=1054;bpv=0;bpt=0")
    -   [list\_lru\_walk()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Flist_lru.h%3Bl%3D208%3Bbpv%3D0%3Bbpt%3D0 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/list_lru.h;l=208;bpv=0;bpt=0") // 遍历 binder\_alloc\_lru 链表
        -   [binder\_alloc\_free\_page()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2F_%2Fandroid%2Fkernel%2Fcommon%2F%2B%2Fandroid13-5.15%3Adrivers%2Fandroid%2Fbinder_alloc.c%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D981 "https://cs.android.com/android/_/android/kernel/common/+/android13-5.15:drivers/android/binder_alloc.c;bpv=0;bpt=0;l=981") // 回收最近最久未使用的空闲物理内存页
