---
created: 2025-04-15T14:47:55 (UTC +08:00)
tags: []
source: https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/
author: 
---

# binder 情景分析 - service manager (context manager) 的启动 | Jekton

> ## Excerpt
> service-manager 作为 binder 架构中的名字服务器，系统启动后会有 init 进程启动。在本篇，我们主要讲述 service-manager 在启动后都做了什么。

---
service-manager 作为 binder 架构中的名字服务器，系统启动后会有 init 进程启动。在本篇，我们主要讲述 service-manager 在启动后都做了什么。

## [](https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/#RPC-%E7%9A%84%E4%B8%80%E8%88%AC%E6%9E%B6%E6%9E%84 "RPC 的一般架构")RPC 的一般架构

我们知道，binder 实际上是 RPC（remote procedure call）的一种。在开始学习 binder 之前，如果能够对 RPC 有所了解，将会非常有帮助。

[![rpc-common-structure](https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/rpc-common-structure.png)](https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/rpc-common-structure.png "rpc-common-structure")rpc-common-structure

首先，系统会启动一个名字服务器（name server）。当某个服务启动的时候（如，这里的 foo service），他会跟名字服务器注册。而后的某个时间点，如果有客户端想要访问 foo service，由于他事先不知道 foo service 的位置，他就会先请求名字服务器。得到 foo service 的位置后，再向 foo service 发出服务请求。

在我们 Android 系统，binder 也是一样的架构。扮演名字服务器这一角色的，就是 **service manager**。

> 注：以下 framework 源码使用 oreo-release 分支，kernel 部分使用 common 的 android-4.9-o-release 分支。部分代码为了可读性，在不影响结果的情况下作了删改。

  

## [](https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/#%E6%80%BB%E4%BD%93%E6%B5%81%E7%A8%8B "总体流程")总体流程

[![startup-of-service_manager](https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/startup-of-service_manager.png)](https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/startup-of-service_manager.png "startup-of-service_manager")startup-of-service\_manager

图片左边表示应用层执行的系统调用，右边为 binder 驱动程序中对应的函数，binder 驱动程序运行在内核态。

1.  首先，调用 `open` 打开 binder 驱动，对应的，内核里会执行 `binder_open`。
2.  `mmap` 申请一个内存映射块。内核分配完响应的内存后，执行驱动程序注册的 `binder_mmap`。
3.  `ioctl` 将自己注册为名字服务。它则是对应 `binder_ioctl`。

下面详细说明每个步骤。

  

## [](https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/#service-manager-%E7%9A%84%E5%90%AF%E5%8A%A8 "service manager 的启动")service manager 的启动

前面我们说，名字服务器必须是最先启动的。所以，service manager 也必须在系统的早期启动。我们知道，Linux 系统中，最早启动的是 init 进程。如此一来，由 init 进程启动 service manger 似乎是一个不错的选择。在 Android 系统里，service manager 也的确是由 init 进程启动的。

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br></pre></td><td><pre><span>// system/core/rootdir/init.rc</span><br><span>on post-fs</span><br><span>    # Load properties from</span><br><span>    #     /system/build.prop,</span><br><span>    #     /odm/build.prop,</span><br><span>    #     /vendor/build.prop and</span><br><span>    #     /factory/factory.prop</span><br><span>    load_system_props</span><br><span>    # start essential services</span><br><span>    start logd</span><br><span>    start servicemanager</span><br><span>    # ...</span><br></pre></td></tr></tbody></table>

`init.rc` 文件由 init 进程在启动后解析并执行。从这里我们可以看出，service manager 确实是由 init 进程启动的。关于 init 进程的内容在这里不展开讨论，有兴趣的读者可以自行阅读（源码在 `system/core/init/` 目录下）。

  

## [](https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/#%E6%89%93%E5%BC%80-binder-%E9%A9%B1%E5%8A%A8 "打开 binder 驱动")打开 binder 驱动

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br></pre></td><td><pre><span></span><br><span><span><span>int</span> <span>main</span><span>(<span>int</span> argc, <span>char</span>** argv)</span> </span>{</span><br><span>    <span><span>struct</span> <span>binder_state</span> *<span>bs</span>;</span></span><br><span>    bs = binder_open(<span>"/dev/binder"</span>, <span>128</span>*<span>1024</span>);</span><br><span>    </span><br><span>}</span><br></pre></td></tr></tbody></table>

servicemanager 的 `main` 函数开始执行后，调用 `binder_open` 打开 binder 驱动。`/dev/binder` 是 binder 驱动对应的设备文件。

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br></pre></td><td><pre><span></span><br><span><span>struct binder_state *<span>binder_open</span><span>(<span>const</span> <span>char</span>* driver, <span>size_t</span> mapsize)</span> </span>{</span><br><span>    <span><span>struct</span> <span>binder_state</span> *<span>bs</span>;</span></span><br><span>    bs = <span>malloc</span>(<span>sizeof</span>(*bs));</span><br><span>    bs-&gt;fd = open(driver, O_RDWR | O_CLOEXEC);</span><br><span>    </span><br><span></span><br><span>    <span>return</span> bs;</span><br><span>}</span><br></pre></td></tr></tbody></table>

`open()` 函数打开文件后，将返回一个 `int` 类型的文件描述符（file descriptor）。后续对 binder 的操作，都通过这个 `fd` 进行。

其中，`O_RDWR` 表示打开用于读写。`O_CLOEXEC` 为 “close on exec”。意思是，如果进程调用了 `exec` 函数，需要关闭对应的文件。

`open` 执行后，将会陷入内核，最终来到 binder 驱动的 `binder_open` 函数：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br></pre></td><td><pre><span></span><br><span><span><span>static</span> <span>int</span> <span>binder_open</span><span>(struct inode *nodp, struct file *filp)</span> </span>{</span><br><span>    <span><span>struct</span> <span>binder_proc</span> *<span>proc</span>;</span></span><br><span>    proc = kzalloc(<span>sizeof</span>(*proc), GFP_KERNEL);</span><br><span>    </span><br><span>    binder_dev = container_of(filp-&gt;private_data, struct binder_device,</span><br><span>                              miscdev);</span><br><span>    proc-&gt;context = &amp;binder_dev-&gt;context;</span><br><span>    filp-&gt;private_data = proc;</span><br><span></span><br><span>    </span><br><span>}</span><br><span></span><br><span></span><br><span><span><span>struct</span> <span>binder_device</span> {</span></span><br><span>    <span><span>struct</span> <span>hlist_node</span> <span>hlist</span>;</span></span><br><span>    <span><span>struct</span> <span>miscdevice</span> <span>miscdev</span>;</span></span><br><span>    <span><span>struct</span> <span>binder_context</span> <span>context</span>;</span></span><br><span>};</span><br></pre></td></tr></tbody></table>

这里首先调用 `kzalloc` 分配一个 `struct binder_proc`。由于每个进程只会调用一次 `open("/dev/binder")`，一个进程对应一个 `binder_proc`。

`binder_dev` 是一个 `struct binder_device`，对应着我们的 binder 驱动，我们将 binder 驱动的 `binder_context` 赋值给 `proc->context`。之所以特别提到它，是因为 `struct binder_context` 存储了名字服务。这个我们在后面再详细讨论，这里需要留意的点是，`binder_context` 对应着一个 binder （虚拟）设备，他是唯一的。

前面我们说，每次操作 binder 的时候都需要传递一个文件描述符。通过这文件描述符，内核就可以拿到文件对应的 `struct file`。每个打开的文件对应一个 `struct file`。传递给 `binder_open` 的参数 `filp` 就是我们前面调用 `open` 时创建的 `struct file`。

通过把 `proc` 保存到 `filp->private_data`，每次操作 binder 的时候，binder 都可以通过 `filp` 拿到对应的 `struct binder_proc`。

  

## [](https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/#%E5%88%9B%E5%BB%BA%E5%86%85%E5%AD%98%E6%98%A0%E5%B0%84 "创建内存映射")创建内存映射

打开 binder 驱动后，service manager 会调用 `mmap` 申请一块内存映射块。

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br></pre></td><td><pre><span></span><br><span><span>struct binder_state *<span>binder_open</span><span>(<span>const</span> <span>char</span>* driver, <span>size_t</span> mapsize)</span> </span>{</span><br><span>    <span><span>struct</span> <span>binder_state</span> *<span>bs</span>;</span></span><br><span></span><br><span>    </span><br><span></span><br><span>    bs-&gt;mapsize = mapsize;</span><br><span>    bs-&gt;mapped = mmap(<span>NULL</span>, mapsize, PROT_READ, MAP_PRIVATE, bs-&gt;fd, <span>0</span>);</span><br><span></span><br><span>    </span><br><span>}</span><br></pre></td></tr></tbody></table>

这里 `mapsize = 128*1024`，`PROT_READ` 表示读权限，我们只会对所申请的内存做读操作（binder 驱动会写入内容）。

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br></pre></td><td><pre><span></span><br><span><span><span>static</span> <span>int</span> <span>binder_mmap</span><span>(struct file *filp, struct vm_area_struct *vma)</span> </span>{</span><br><span>    <span><span>struct</span> <span>binder_proc</span> *<span>proc</span> = <span>filp</span>-&gt;<span>private_data</span>;</span></span><br><span></span><br><span>    </span><br><span></span><br><span>    binder_alloc_mmap_handler(&amp;proc-&gt;alloc, vma);</span><br><span>    <span>return</span> <span>0</span>;</span><br><span>}</span><br></pre></td></tr></tbody></table>

这里印证了上面的说法，我们确实是通过 `filp` 来获取对应的 `struct binder_proc`。

`struct vm_area_struct` 是内核用于管理内存的结构体，这里它对应于我们通过 `mmap` 所申请的内存块。

`proc->alloc` 是一个 `struct binder_alloc`。binder 驱动程序通过它来管理所申请的内存块。

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br></pre></td><td><pre><span></span><br><span><span><span>int</span> <span>binder_alloc_mmap_handler</span><span>(struct binder_alloc *alloc,</span></span></span><br><span><span><span>                              struct vm_area_struct *vma)</span> </span>{</span><br><span>    <span>int</span> ret;</span><br><span>    <span><span>struct</span> <span>vm_struct</span> *<span>area</span>;</span></span><br><span>    <span><span>struct</span> <span>binder_buffer</span> *<span>buffer</span>;</span></span><br><span></span><br><span>    </span><br><span>    area = get_vm_area(vma-&gt;vm_end - vma-&gt;vm_start, VM_IOREMAP);</span><br><span>    alloc-&gt;buffer = area-&gt;addr;</span><br><span>    alloc-&gt;user_buffer_offset =</span><br><span>        vma-&gt;vm_start - (<span>uintptr_t</span>)alloc-&gt;buffer;</span><br><span></span><br><span>    </span><br><span>    alloc-&gt;pages = kzalloc(<span>sizeof</span>(alloc-&gt;pages[<span>0</span>]) *</span><br><span>                   ((vma-&gt;vm_end - vma-&gt;vm_start) / PAGE_SIZE),</span><br><span>                   GFP_KERNEL);</span><br><span>    alloc-&gt;buffer_size = vma-&gt;vm_end - vma-&gt;vm_start;</span><br><span></span><br><span>    buffer = kzalloc(<span>sizeof</span>(*buffer), GFP_KERNEL);</span><br><span></span><br><span>    </span><br><span>    </span><br><span>    __binder_update_page_range(alloc, <span>1</span>, alloc-&gt;buffer,</span><br><span>                               alloc-&gt;buffer + BINDER_MIN_ALLOC, vma);</span><br><span>    buffer-&gt;data = alloc-&gt;buffer;</span><br><span>    list_add(&amp;buffer-&gt;entry, &amp;alloc-&gt;buffers);</span><br><span>    buffer-&gt;<span>free</span> = <span>1</span>;</span><br><span>    binder_insert_free_buffer(alloc, buffer);</span><br><span>    alloc-&gt;free_async_space = alloc-&gt;buffer_size / <span>2</span>;</span><br><span>    barrier();</span><br><span>    alloc-&gt;vma = vma;</span><br><span>    alloc-&gt;vma_vm_mm = vma-&gt;vm_mm;</span><br><span></span><br><span>    <span>return</span> <span>0</span>;</span><br><span>}</span><br></pre></td></tr></tbody></table>

`binder_alloc_mmap_handler` 执行后的内存如下图所示。

[![binder-mapped-memory](https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/binder-mapped-memory.png)](https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/binder-mapped-memory.png "binder-mapped-memory")binder-mapped-memory

这里先调用 `get_vm_area` 在内核空间分配了一个地址段，然后为这个地址段分配 `struct page`（默认情况下，一个 page 为 4K，`struct page` 包括了相应内存页对应的物理页帧(page frame)的信息）。最后将这些 `struct page` 与对应的物理内存页映射起来。

最后，用户空间、内核空间都有一个内存段，对应着相同的物理内存页。内核写入数据后，应用程序即可以直接读到。

最后值得留意的是 `alloc->user_buffer_offset`。它保存了内核地址跟用户地址的差值，以后还会用到它。

  

## [](https://jekton.github.io/2018/04/11/binder-startup-of-service-manager/#%E6%B3%A8%E5%86%8C%E4%B8%BA%E5%90%8D%E5%AD%97%E6%9C%8D%E5%8A%A1%E5%99%A8 "注册为名字服务器")注册为名字服务器

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br></pre></td><td><pre><span></span><br><span><span><span>int</span> <span>main</span><span>(<span>int</span> argc, <span>char</span>** argv)</span> </span>{</span><br><span>    <span><span>struct</span> <span>binder_state</span> *<span>bs</span>;</span></span><br><span>    </span><br><span></span><br><span>    bs = binder_open(driver, <span>128</span>*<span>1024</span>);</span><br><span>    binder_become_context_manager(bs);</span><br><span></span><br><span>    </span><br><span>}</span><br></pre></td></tr></tbody></table>

这里调用 `binder_become_context_manager()` 注册为名字服务器。基于此，也有人叫 service manager 作 **context manager**。

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br></pre></td><td><pre><span></span><br><span><span><span>int</span> <span>binder_become_context_manager</span><span>(struct binder_state *bs)</span> </span>{</span><br><span>    <span>return</span> ioctl(bs-&gt;fd, BINDER_SET_CONTEXT_MGR, <span>0</span>);</span><br><span>}</span><br></pre></td></tr></tbody></table>

`binder_become_context_manager` 很简单，就一行代码。`ioctl` 是所谓的 io control，放了一些不方便归类的杂项函数。

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br></pre></td><td><pre><span><span>#<span>include</span> <span>&lt;sys/ioctl.h&gt;</span></span></span><br><span><span><span>int</span> <span>ioctl</span><span>(<span>int</span> fildes, <span>unsigned</span> <span>long</span> request, ...)</span></span>;</span><br></pre></td></tr></tbody></table>

-   `fildes`：文件描述符
-   request：一个 `unsigned long` 型的数值，用来表示特定的服务。`BINDER_SET_CONTEXT_MGR` 表示注册为 context manager。
-   第三个参数是请求的其他参数

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br></pre></td><td><pre><span></span><br><span><span><span>static</span> <span>long</span> <span>binder_ioctl</span><span>(struct file *filp, <span>unsigned</span> <span>int</span> cmd, <span>unsigned</span> <span>long</span> arg)</span> </span>{</span><br><span>    <span>switch</span> (cmd) {</span><br><span>    </span><br><span>    <span>case</span> BINDER_SET_CONTEXT_MGR:</span><br><span>        ret = binder_ioctl_set_ctx_mgr(filp);</span><br><span>        <span>if</span> (ret)</span><br><span>            <span>goto</span> err;</span><br><span>        <span>break</span>;</span><br><span>    </span><br><span></span><br><span>    <span>return</span> ret;</span><br><span>}</span><br><span></span><br><span></span><br><span><span><span>static</span> <span>int</span> <span>binder_ioctl_set_ctx_mgr</span><span>(struct file *filp)</span> </span>{</span><br><span>    <span>int</span> ret = <span>0</span>;</span><br><span>    <span><span>struct</span> <span>binder_proc</span> *<span>proc</span> = <span>filp</span>-&gt;<span>private_data</span>;</span></span><br><span>    <span><span>struct</span> <span>binder_context</span> *<span>context</span> = <span>proc</span>-&gt;<span>context</span>;</span></span><br><span>    <span><span>struct</span> <span>binder_node</span> *<span>new_node</span>;</span></span><br><span></span><br><span>    ret = security_binder_set_context_mgr(proc-&gt;tsk);</span><br><span>    <span>if</span> (ret &lt; <span>0</span>)</span><br><span>        <span>return</span> ret;</span><br><span>    new_node = binder_new_node(proc, <span>NULL</span>);</span><br><span>    context-&gt;binder_context_mgr_node = new_node;</span><br><span></span><br><span>    <span>return</span> <span>0</span>;</span><br><span>}</span><br></pre></td></tr></tbody></table>

`security_binder_set_context_mgr` 用于检测调用进程是否有权限执行 `binder_set_context_mgr`。service manager 显然是有权限的。接下来 `binder_new_node()` 生成一个 `binder_node`。一个服务对应一个 `struct binder_node`，一个进程可以有多个服务。

我们将新生成的 `struct binder_node` 赋值给 `context->binder_context_mgr_node`，即是注册了名字服务。前面我们说过，所有的 `binder_proc->context` 都指向 `binder_device` 中的 `context`。这里对它，即可以让所有的进程都访问到 service manager 的 `binder_node`。通过这个 `binder_node`，便可以使用 service manager 提供的名字服务。

到这里，名字服务的注册便完成了。
