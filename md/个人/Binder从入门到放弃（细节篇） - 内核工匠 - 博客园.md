---
created: 2025-04-15T10:25:23 (UTC +08:00)
tags: []
source: https://www.cnblogs.com/Linux-tech/p/12961294.html
author: 内核工匠
---

# Binder从入门到放弃（细节篇） - 内核工匠 - 博客园

> ## Excerpt
> 前言Binder从入门到放弃包括了上下篇，上篇是框架部分，下篇通过几个典型的binder通信过程来呈现其实现细节，即本文。一、启动service manager流程Service manager进程和bind...

---
**前言**

Binder从入门到放弃包括了上下篇，上篇是框架部分，下篇通过几个典型的binder通信过程来呈现其实现细节，即本文。

**一、启动service manager**

1.  **流程**
    

Service manager进程和binder驱动的交互如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTBwRnV2TkhWSWhUaWJNOW05ZElWaFdQY3VIS3FHRWJORHNWYUFBa1FTVVppYjQ2WDYxWm5FNEMwcEttbmZxMmlhamFCeXJpY1dzMTdpYU53LzY0MA?x-oss-process=image/format,png)

在安卓系统启动过程中，init进程会启动service manager进程。service manager会打开/dev/binder设备，一个进程打开binder设备就意味着该进程会使用binder这种IPC机制，这时候，在内核态会相应的构建一个binder proc对象，来管理该进程相关的binder资源（binder ref、binder node、binder thread等）。为了方便binder内存管控，这时候还会映射一段128K的内存地址用于binder通信。之后，service manager会把自己设定为context manager。所谓context manager实际上就是一个“名字服务器”，可以完成service组件名字的解析。随后service manager会通过binder协议（BC\_ENTER\_LOOPER）告知驱动自己已经准备好接收请求了。最后，service manager会进入读阻塞状态，等待来自其他进程的服务请求。

完成上面的一系列操作之后，内核相关的数据结构如下所示：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTBwRnV2TkhWSWhUaWJNOW05ZElWaFdSNWprcGRpYzNnaWNHWUVoV1Q3WjhiQmliazI3TmZJUUpkT0doQ3hCM0FHZDBuNm9hMVc3bGowQWcvNjQw?x-oss-process=image/format,png)

由于Service manager也算是一个特殊的service组件，因此在内核态也有一个binder node对象与之对应。service manager和其他的service组件不同的是它没有使用线程池模型，而是一个单线程的进程，因此它在内核态只有一个binder proc和binder thread。整个系统系统只有一个binder context，系统中所有的binder proc都指向这个全局唯一的binder上下文对象。而找到了binder context也就找到了service manager对应的binder node。

binder proc使用了红黑树来管理其所属的binder thread和binder node，不过在Service manager这个场景中，binder proc只管理了一个binder thread和binder node，看起来似乎有些小题大做，不过在其他场景（例如system server）中，binder proc会创建线程池，也可能注册多个service组件。

1.  **相关数据结构**
    

在内核态，每一个参与binder通信的进程都会用一个唯一的struct binder\_proc对象来表示。struct binder\_proc主要成员如下表所示：

<table><tbody><tr><td><p>成员变量</p></td><td><p>描述</p></td></tr><tr><td><p>struct hlist_node</p><p>proc_node</p></td><td><p>系统中的所有binder proc挂入binder_procs的链表中，这个成员是挂入全局binder_procs的链表的节点</p></td></tr><tr><td><p>struct rb_root threads</p></td><td><p>binder进程对应的所有binder thread组成的红黑树，tid作为key</p></td></tr><tr><td><p>struct rb_root nodes</p></td><td><p>一个binder进程可以注册多个service组件，因此binder proc可以有很多的binder node。Binder proc对应的所有binder node组成一颗红黑树。当然对于service manager而言，它只有一个binder node。</p></td></tr><tr><td><p>struct list_head</p><p>waiting_threads</p></td><td><p>该binder进程的线程池中等待处理binder work的binder thread链表</p></td></tr><tr><td><p>int pid</p></td><td><p>进程ID</p></td></tr><tr><td><p>struct task_struct *tsk</p></td><td><p>指向该binder进程对应的进程描述符（指向thread group leader对应的task struct）</p></td></tr><tr><td><p>struct list_head todo</p></td><td><p>需要该binder进程处理的binder work链表</p></td></tr><tr><td><p>int max_threads</p></td><td><p>线程池中运行的最大数目</p></td></tr><tr><td><p>struct binder_alloc alloc</p></td><td><p>管理binder 内存分配的数据结构</p></td></tr><tr><td><p>struct binder_context</p><p>*context</p></td><td><p>保存binder上下文管理者的信息。通过binder context可以找到service manager对应的bind node。</p></td></tr></tbody></table>

和进程抽象类似，binder proc也是管理binder资源的实体，但是真正执行binder通信的实体是binder thread。struct binder\_thread主要成员如下表所示：

<table><tbody><tr><td><p>成员变量</p></td><td><p>描述</p></td></tr><tr><td><p>struct binder_proc *proc</p></td><td><p>该binder thread所属的binder proc</p></td></tr><tr><td><p>struct rb_node rb_node</p></td><td><p>挂入binder proc红黑树的节点</p></td></tr><tr><td><p>struct list_head</p><p>waiting_thread_node</p></td><td><p>无事可做的时候，binder thread会挂入binder proc的等待队列</p></td></tr><tr><td><p>int pid</p></td><td><p>Thread id</p></td></tr><tr><td><p>struct binder_transaction</p><p>*transaction_stack</p></td><td><p>该binder thread正在处理的transaction</p></td></tr><tr><td><p>struct list_head todo</p></td><td><p>需要该binder线程处理的binder work链表</p></td></tr><tr><td><p>struct task_struct *task</p></td><td><p>该binder thread对应的进程描述符</p></td></tr></tbody></table>

Binder node是用户空间service组件对象的内核态实体对象，struct binder\_node主要成员如下表所示：

<table><tbody><tr><td><p>成员变量</p></td><td><p>描述</p></td></tr><tr><td><p>struct rb_node rb_node;</p></td><td><p>一个binder proc可能有多个service组件（提供多种服务），属于一个binder proc的binder node会挂入binder proc的红黑树，这个成员是嵌入红黑树的节点。</p></td></tr><tr><td><p>struct binder_proc *proc</p></td><td><p>该binder node所属的binder proc</p></td></tr><tr><td><p>int debug_id</p></td><td><p>唯一标示该node的id，用于调试</p></td></tr><tr><td><p>struct hlist_head refs</p></td><td><p>一个service组件可能会有多个client发起服务请求，也就是说每一个client都是对binder node的一次引用，这个成员是就是保存binder ref的哈希表</p></td></tr><tr><td><p>binder_uintptr_t ptr</p><p>binder_uintptr_t cookie</p></td><td><p>指向用户空间service组件相关的信息</p></td></tr><tr><td><p>u8 sched_policy:2;</p><p>u8 inherit_rt:1;</p><p>u8 min_priority;</p></td><td><p>这些属性定义了该service组件在处理transaction的时候优先级的设定。</p></td></tr><tr><td><p>bool has_async_transaction</p></td><td><p>是否有异步通信需要处理</p></td></tr><tr><td><p>struct list_head async_todo</p></td><td><p>异步binder通信的队列</p></td></tr></tbody></table>

**二、client如何找到service manager？**

**1、流程**

为了完成service组件注册，Client需要首先定位service manager组件。在client这个binder process中，我们使用handle作为地址来标记service组件。Service manager比较特殊，对任何一个binder process而言，handle等于0的那个句柄就是指向service manager组件。对内核态binder驱动而言，寻找service manager实际上就是寻找其对应的binder node。下面是一个binder client向service manager请求注册服务的过程示例，我们重点关注binder驱动如何定位service manager：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTBwRnV2TkhWSWhUaWJNOW05ZElWaFdKUkZXSHRXUFhpYWNtMEx3aWJxcjlPTW4yUmJTUVRqaWJkSmZ3eVdEUXM5S3NTdXdqMTM2alV5Y2cvNjQw?x-oss-process=image/format,png)

想要访问service manager的进程需要首先打开binder driver，这时候内核会创建该进程对应的binder proc对象，并建立binder proc和context manager的关系，这样进一步可以找到service manager对应的binder node。随后，client进程会调用mmap映射了（1M-8K）的binder内存空间。之所以映射这么怪异的内存size主要是为了有效的利用虚拟地址空间（VMA之间有4K的gap）。完成上面两步操作之后，client process就可以通过ioctl向service manager发起transaction请求了，同时告知目标对象handle等于0。

实际上这个阶段的主要工作在用户空间，主要是service manager组件代理BpServiceManager以及BpBinder的创建过程。一般的通信过程需要为组件代理对象分配一个句柄，但是service manager访问比较特殊，对于每一个进程，等于0的句柄都保留给了service manager，因此这里就不需要分配句柄这个过程了。

**2、路由过程**

在binder C/S通信结构中，binder client中的BpBinder找到binder server中的BBinder的过程需要如下过程：

1.  binder client用户空间中的service组件代理（BpBinder）用句柄表示要访问的server中的service组件（BBinder）
    
2.  对于每一个句柄，binder client内核空间使用binder ref对象与之对应
    
3.  binder ref对象会指向一个binder node对象
    
4.  binder node对象对应一个binder server进程的service组件
    

在我们这个场景中，binder ref是在client第一次通过ioctl和binder驱动交互时候完成的。这时候，binder驱动的binder\_ioctl函数中会建立上面路由过程需要的完整的数据对象：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTBwRnV2TkhWSWhUaWJNOW05ZElWaFc1YWVDcXhyNWlhVUdLREdYTXdza0dpYlJpY0VRMW1UQWdKSFpBbE94VDZQVFBpYlgyNVppYzM2cWljMVEvNjQw?x-oss-process=image/format,png)

Service manager的路由比较特殊，没有采用binder ref--->binder node的过程。在binder驱动中，看到0号句柄自然就知道是去往service manager的请求。因此，通过binder proc--->binder context-----binder node这条路径就找到了service manager。

**三、注册Service组件**

1.  **流程**
    

上一节描述了client如何找到service manager的过程，这是整个注册service组件的前半部分，这一节我们补全整个流程。由于client和service manager都完成了open和mmap的过程，双方都准备好，后续可以通过ioctl进行binder transaction的通信过程了，因此下面的流程图主要呈现binder transaction的流程（忽略client/server和binder驱动系统调用的细节）：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTBwRnV2TkhWSWhUaWJNOW05ZElWaFdHWTR5aWFZR1dsaEgyeWRkcHFOUkxvVHZpYUNhdHJmeVRENXRDeWlhbmljVUtRM050VW84alVlYXV3LzY0MA?x-oss-process=image/format,png)

Service manager是一个service组件管理中心，任何一个service组件都需要向service manager进行注册（add service），以便其他的APP可以通过service manager定位到该service组件（check service）。

**2、数据对象综述**

注册服务相关数据结构全图如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTBwRnV2TkhWSWhUaWJNOW05ZElWaFdCOEtObzNpY1dCT1Z5R3dhb0N0M1NHaWJheGNlZTFEdkJLMmFJU09rMnFzaWFXSzFqd1Eyd0VoNHcvNjQw?x-oss-process=image/format,png)

配合上面的流程，binder驱动会为client和server分别创建对应的各种数据结构对象，具体过程如下：

1.  假设我们现在准备注册A服务组件，绑定A服务组件的进程在add service这个场景下是client process，它在用户空间首先会创建了service组件对象，在递交BC\_TRANSACTION的时候会携带service组件的信息（把service组件地址信息封装在flat\_binder\_object数据结构中）。
    
2.  在系统调用接口层面，我们使用ioctl（BINDER\_WRITE\_READ）来完成具体transaction的递交过程。具体的transaction数据封装在struct binder\_write\_read对象中，具体如下图所示：
    

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTBwRnV2TkhWSWhUaWJNOW05ZElWaFdCSFI0aWNxR3FxTFJtejYzUVIyVFA3MTh3d1lVWFVqdFN4V2U4b0xJWlREaWJhcWo3TzZZcGd6QS82NDA?x-oss-process=image/format,png)

1.  Binder驱动创建binder\_transaction对象来控制完成本次binder transaction。首先要初始化transaction，具体包括：和谁通信（用户空间通过binder\_transaction\_data的target成员告知binder驱动transaction的target）、为何通信（binder\_transaction\_data的code）等
    
2.  对于每一个service组件，内核都会创建一个binder node与之对应。用户空间通过flat\_binder\_object这个数据结构把本次要注册的service组件扁平化，传递给binder驱动。驱动根据这个flat\_binder\_object创建并初始化了该service组件对应的binder node。由于是注册到service manager，也就是说service manager会有一个对本次注册组件的引用，所以需要在target proc（即service manager）中建立一个binder ref对象（指向这个要注册的binder实体）并分配一个handle。
    
3.  把一个BINDER\_WORK\_TRANSACTION\_COMPLETE类型的binder work挂入client binder thread的todo list，通知client其请求的transaction已经被binder处理完毕，可以进行其他工作了（当然对于同步binder通信，client一般会通过read类型的ioctl进入阻塞态，等待server端的回应）。
    
4.  至此，client端已经完成了所有操作，现在我们开始进入server端的数据流了。Binder驱动会把一个BINDER\_WORK\_TRANSACTION类型的binder work（内嵌在binder transaction）挂入binder线程的todo list，然后唤醒它起来干活。
    
5.  binder server端会使用ioctl（BINDER\_WRITE\_READ）进入读阻塞状态，等待client的请求到来。一旦有请求到来，Service manager进程会从binder\_thread\_read中醒来处理队列上的binder work。所谓处理binder work其实完成client transaction的向上递交过程。具体的transaction数据封装在struct binder\_write\_read对象中，具体如下图所示：
    

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTBwRnV2TkhWSWhUaWJNOW05ZElWaFdWUjJMNTYxWXVWOUN1cFdtM3V5azZraEl0U0ZxU2xEeHpzd05SRUZWUnZtdzdDQ1FCQTNyQlEvNjQw?x-oss-process=image/format,png)

需要强调的一点是：在步骤2中，flat\_binder\_object传递的是binder node，而这里传递的是handle（即binder ref，步骤4中创建的）

1.  在Service manager进程的用户态，识别了本次transaction的code是add service，那么它会把（service name，handle）数据写入其数据库，完成服务注册。
    
2.  从transaction的角度看，上半场已经完成。现在开始下半场的transaction的处理，即BC\_REPLY的处理。和BC\_TRANSACTION处理类似，也是通过binder\_ioctl ---> binder\_ioctl\_write\_read ---> binder\_thread\_write ---> binder\_transaction这个调用链条进入binder transaction处理流程的。
    
3.  和上半场类似，在这里Binder驱动同样会创建一个binder\_transaction对象来控制完成本次BC\_REPLY的binder transaction。通过thread->transaction\_stack可以找到其对应的BC\_TRANSACTION的binder transaction对象，进而找到回应给哪一个binder process和thread。后续的处理和上半场类似，这里就不再赘述了。
    

1.  **相关数据结构**
    

struct transaction主要用来表示binder client和server之间的一次通信，该数据结构的主要成员如下表所示：

<table><tbody><tr><td><p>成员变量</p></td><td><p>描述</p></td></tr><tr><td><p>work</p></td><td><p>本次transaction涉及的binder work，它会挂入target proc或者target binder thread的todo list中。</p></td></tr><tr><td><p>from</p></td><td><p>发起binder通信的线程</p></td></tr><tr><td><p>to_proc</p></td><td><p>处理binder请求的进程</p></td></tr><tr><td><p>to_thread</p></td><td><p>处理binder请求的线程</p></td></tr><tr><td><p>buffer</p></td><td><p>binder通信使用的buffer，当A向B服务请求binder通信的时候，B进程分配buffer，并copy A的数据（user space）到buffer中。这是binder通信唯一一次内存拷贝。</p></td></tr><tr><td><p>code</p></td><td><p>本次transaction的操作码。Binder server端根据操作码提供相应的服务</p></td></tr><tr><td><p>flags</p></td><td><p>本次transaction的一些属性标记</p></td></tr><tr><td><p>Priority</p><p>saved_priority</p></td><td><p>和优先级处理相关的成员</p></td></tr></tbody></table>

BC\_TRANSACTION、BC\_REPLY、BR\_TRANSACTION和BR\_REPLY这四个协议码的协议数据是struct binder\_transaction\_data，该数据结构的主要成员如下表所示：

<table><tbody><tr><td><p>成员变量</p></td><td><p>描述</p></td></tr><tr><td><p>target</p></td><td><p>本次transation去向何方？Target有两种形式，一种是本地binder实体，另外一种是表示远端binder实体的句柄。</p><p>在client向service manager发起transaction的时候，那么target.handle等于0。当该transaction到达service manager的时候，binder实体变成本地对象，因此用</p><p>Target.ptr和cookie来表示。</p></td></tr><tr><td><p>cookie</p></td><td><p>如果transaction的目的地是本地binder实体，那么这个成员保存了binder实体对象的用户空间地址</p></td></tr><tr><td><p>code</p></td><td><p>Client和service 组件之间的操作码，binder驱动不关心这个码字。</p></td></tr><tr><td><p>flags</p></td><td><p>描述transaction特性的flag。例如TF_ONE_WAY说明是同步还是异步binder通信</p></td></tr><tr><td><p>sender_pid</p><p>sender_euid</p></td><td><p>是谁发起transaction？在binder驱动中会根据当前线程设定。</p></td></tr><tr><td><p>data_size</p><p>offsets_size</p><p>data</p></td><td><p>本次transaction的数据缓冲区信息。</p></td></tr></tbody></table>

flat\_binder\_object主要用来在进程之间传递Binder对象，该数据结构的主要成员如下表所示：

<table><tbody><tr><td><p>成员变量</p></td><td><p>描述</p></td></tr><tr><td><p>hdr</p></td><td><p>用来描述Binder对象的类型，目前支持的类型有：</p><ol><li><p>binder实体（本地service组件）</p></li><li><p>Binder句柄（远端的service组件）</p></li><li><p>文件描述符</p></li><li><p>......</p></li></ol><p>本文主要关注前两种对象类型</p></td></tr><tr><td><p>Binder</p><p>handle</p></td><td><p>如果flat_binder_object传递的是本地service组件，那么这个联合体中的binder成员有效，指向本地service组件（用户空间对象）的一个弱引用对象的地址。</p><p>如果flat_binder_object传递的是句柄，那么这个联合体中的handle成员有效，该handle对应的binder ref指向一个binder实体对象。</p></td></tr><tr><td><p>cookie</p></td><td><p>如果传递的是binder实体，那么这个成员保存了binder实体对象（service组件）的用户空间地址</p></td></tr></tbody></table>

struct binder\_ref主要用来表示一个对Binder实体对象（binder node）的引用，该数据结构的主要成员如下表所示：

<table><tbody><tr><td><p>成员变量</p></td><td><p>描述</p></td></tr><tr><td><p>data</p></td><td><p>这个成员最核心的数据是用户空间的句柄</p></td></tr><tr><td><p>rb_node_desc</p></td><td><p>挂入binder proc的红黑树（key是描述符，userspace的句柄）</p></td></tr><tr><td><p>rb_node_node</p></td><td><p>挂入binder proc的红黑树（key是binder node）</p></td></tr><tr><td><p>node_entry</p></td><td><p>挂入binder node的哈希表</p></td></tr><tr><td><p>proc</p></td><td><p>该binder ref属于哪一个binder proc</p></td></tr><tr><td><p>node</p></td><td><p>该binder ref引用哪一个binder node</p></td></tr></tbody></table>

**四、如何和Service组件通信**

我们以B进程向A服务组件（位于A进程）发起服务请求为例来说明具体的操作流程。B进程不能直接请求A服务组件的服务，因为B进程唯一获知的信息是A服务组件的名字而已。由于A服务组件已经注册在案，因此service manager已经有（A服务组件名字，句柄）的记录，因此B进程可以通过下面的流程获得A服务组件的信息并建立其代理组件对象：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTBwRnV2TkhWSWhUaWJNOW05ZElWaFdNVnlEUmtkaWFBOHVlNmFjWmFQd2VpY2xFbXJEY2dIZld3b1NZQ0czZjEzdnZjZjMyRDZQdWdQUS82NDA?x-oss-process=image/format,png)

B进程首先发起BC\_TRANSACTION操作，操作码是CHECK\_SERVICE，数据是A服务组件的名字。Service manager找到了句柄后将其封装到BC\_REPLY中。这里的句柄是service manager进程的句柄，这个句柄并不能直接被B进程直接使用，毕竟（进程，句柄）才对应唯一的binder实体。这里的binder driver有一个很关键的操作：把service manager中句柄A转换成B client进程中的句柄B，并封装在BR\_REPLY中。这时候（service manager进程，句柄A）和（B client进程，句柄B）都指向A服务组件对应的bind node对象。

一旦定位了A服务组件，那么可以继续进行如下的流程：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTBwRnV2TkhWSWhUaWJNOW05ZElWaFd5N1NKUFA4UnJKZGljTmtsc3JCVG5oZGFnWmdBUkJ1Zkpjb3BZeEtsQ2ZibkNtWWljeWFXNVdYQS82NDA?x-oss-process=image/format,png)

**五、Binder内存操作**

**1.逻辑过程**

在处理binder transaction的过程中，相关的内存操作如下所示：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTBwRnV2TkhWSWhUaWJNOW05ZElWaFdMWndvZDNQeUhDd2liTE1iVTBOVDdXWGpDWTdGTWZFdVFoU0hSa0hKTnhYRWZXRXh6Q25UOHF3LzY0MA?x-oss-process=image/format,png)

配合上面的流程，内存操作的逻辑过程如下：

1.  在binder client的用户空间中，发起transaction的一方会构建用户数据缓冲区（包括两部分：实际的数据区和offset区），把想要传递到server端的数据填充到缓冲区并封装在binder\_transaction\_data数据结构中。
    
2.  binder\_transaction\_data会被copy到内核态，binder驱动会根据它计算出本次需要binder通信的数据量。
    
3.  根据binder通信的数据量在server进程的binder VMA分配数据缓冲区（binder buffer是这个缓冲区的控制数据对象），同时根据需要也会分配对应的物理page并建立地址映射，以便用户空间可以访问这段buffer的数据。
    
4.  建立内核地址空间的映射，把用户空间的binder数据缓冲区拷贝到内核中，然后释放掉该映射。
    
5.  在把binder buffer的数据传递到server用户空间的时候，我们需要一个binder\_transaction\_data来描述binder通信的缓冲区数据，这个数据对象需要拷贝到用户地址空间，而binder buffer中的数据则不需要拷贝，因为在上面步骤3中已经建立了地址映射，server进程可以直接访问即可。
    

**2.主要的数据结构**

struct binder\_alloc用来描述binder进程内存分配器，该数据结构的主要成员如下表所示：

<table><tbody><tr><td><p>成员变量</p></td><td><p>描述</p></td></tr><tr><td><p>vma</p></td><td><p>binder内存对应的VMA</p></td></tr><tr><td><p>vma_vm_mm</p></td><td><p>binder进程对应的地址空间描述符</p></td></tr><tr><td><p>buffer</p></td><td><p>该binder proc能用于binder通信的内存地址。</p><p>该地址是mmap的用户空间虚拟地址。</p></td></tr><tr><td><p>buffers</p></td><td><p>所有的binder buffers（包括空闲的和正在使用的）</p></td></tr><tr><td><p>free_buffers</p></td><td><p>空闲binder buffers的红黑树，按照size排序</p></td></tr><tr><td><p>allocated_buffers</p></td><td><p>已经分配的binder buffers的红黑树，key是buffer address</p></td></tr><tr><td><p>free_async_space</p></td><td><p>剩余的可用于异步binder通信的内存大小。</p><p>初始化的时候配置为2M（整个binder内存的一半）</p></td></tr><tr><td><p>pages</p></td><td><p>binder内存区域对应的page们。在reclaim binder内存的时候</p></td></tr><tr><td><p>buffer_size</p></td><td><p>通过mmap映射的，用于binder通信的缓冲区大小，即binder alloc管理的整个内存的大小。</p></td></tr><tr><td><p>pid</p></td><td><p>Binder proc的pid</p></td></tr></tbody></table>

struct binder\_buffer用来描述一个用于binder通信的缓冲区，该数据结构的主要成员如下表所示：

<table><tbody><tr><td><p>成员变量</p></td><td><p>描述</p></td></tr><tr><td><p>entry</p></td><td><p>挂入binder alloc buffer链表（buffers成员）的节点</p></td></tr><tr><td><p>rb_node</p></td><td><p>挂入binder alloc红黑树的节点：如果是空闲的buffer，挂入空闲红黑树，如果是已经分配的，挂入已分配红黑树。</p></td></tr><tr><td><p>transaction</p></td><td><p>Binder缓冲区都是用于某次binder transaction的，这个成员指向对应的transaction。</p></td></tr><tr><td><p>target_node</p></td><td><p>该buffer的去向哪一个node（service组件）</p></td></tr><tr><td><p>data_size</p><p>offsets_size</p></td><td><p>Binder缓冲区的数据区域的大小以及offset区域的大小。</p></td></tr><tr><td><p>user_data</p></td><td><p>该binder buffer的用户空间地址</p></td></tr></tbody></table>

参考文献：

\[1\]Android系统源代码情景分析，罗升阳著

\[2\]http://gityuan.com/tags/#binder，袁辉辉的博客

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X2dpZi9kNGhvWUpseE9qTlNyNE9XSjlrdWtpYkZuc3h2U3pkSWlicjJqRDVZVDNqQU1mOWVrZDJDc0I3ME9HallxbjBKdFB3QjFtSXkxWlduQ216R1JMSzJFaWN5dy82NDA?x-oss-process=image/format,png)

**扫码关注**  
**“内核工匠”微信公众号**  
Linux 内核黑科技 | 技术文章 | 精选教程
