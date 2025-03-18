**进程隔离：**

内核空间中存放的是内核代码和数据，而进程的用户空间中存放的是用户程序的代码和数据 为了保证系统的安全，用户空间和内核空间是天然隔离的 每个进程有自己的虚拟内存空间，为了安全，每个进程只能操作自己的虚拟内存空间，只有操作系统才有权限操作物理内存空间

### 1.为什么要用Binder？

-   Android系统内核是Linux内核
-   Linux内核进程通信有：管道、内存共享、Socket、File；
-   对比：

![](https://pic3.zhimg.com/v2-c244ba2a1ac75966f77254fcea1bf6fc_1440w.jpg)

Binder的一次拷贝发生在用户空间拷贝到内核空间；

**用户空间：** App进程运行的内存空间；

**内核空间：** 系统驱动、和硬件相关的代码运行的内存空间，也就是进程ID为0的进程运行的空间；

**程序局部性原则：** 只加载少量代码；应用没有运行的代码放在磁盘中，运行时高速缓冲区进行加载要运行的代码；默认一次加载一个页（4K），若不够4K就用0补齐；

MMU:内存管理单元；

给CPU提供虚拟地址；

当对变量操作赋值时：

-   CPU拿着虚拟地址和值给到MMU
-   MMU用虚拟地址匹配到物理地址，MMU去物理内存中进行赋值；  
    **物理地址**： 物理内存的实际地址,并不是磁盘；  
    **虚拟地址**：MMU根据物理内存的实际地址翻译出的虚拟地址；提供给CPU使用；

![](https://pica.zhimg.com/v2-2c59b71f9ccc98548d92d5d720d3586c_1440w.jpg)

![](https://pic3.zhimg.com/v2-cf2aaca157bec1a34f2e7c55be27b896_1440w.jpg)

页命中：CPU读取变量时，MMU在物理内存的页表中找到了这个地址；

页未命中：CPU读取变量时，MMU在物理内存的页表中没有找到了这个地址，此时会触发MMU去磁盘读取变量并存到物理内存中；

普通的二次拷贝：

应用A拷贝到服务端：coay\_from\_user

从服务端拷贝到应用B：coay\_to\_user

**`mmap():`**

-   在物理内存中开辟一段固定大小的内存空间
-   将磁盘文件与物理内存进行映射（理解为绑定）
-   MMU将物理内存地址转换为虚拟地址给到CPU（虚拟地址映射物理内存）

#### 共享内存进程通信

-   进程A调用mmap()函数会在内核空间中虚拟地址和一块同样大小的物理内存，将两者进行映射
-   得到一个虚拟地址
-   进程B调用mmap()函数，传参和步骤1一样的话，就会得到一个和步骤2相同的虚拟地址
-   进程A和进程B都可以用同一虚拟地址对同一块映射内存进行操作
-   进程A和进程B就实现了通信
-   没有发生拷贝，共享一块内存，不安全  
#### **Binder通信原理：**

角色：Server端A、Client端B、Binder驱动、内核空间、物理内存

-   Binder驱动在物理内存中开辟一块固定大小（1M-8K）的物理内存w，与内核空间的虚拟地址x进行映射得到
-   A的用户空间的虚拟地址ax和物理内存w进行映射
-   此时内核空间虚拟地址x和物理内存w已经进行了映射，物理内存w和Server端A的用户空间虚拟地址ax进行了映射：也就是 内核空间的虚拟地址x = 物理内存w = Server端A的用户空间虚拟地址ax
-   B发送请求：将数据按照binder协议进行打包给到Binder驱动，Binder驱动调用coay_from_user()将数据拷贝到内核空间的虚拟地址x
-   因步骤3中的三块区域进行了映射
-   Server端A就得到了Client端B发送的数据
-   通过内存映射关系，只发生了一次拷贝

![](https://pic2.zhimg.com/v2-f82badfc206d886b7c562be763e300d3_1440w.jpg)

Activity跳转时，最多携带1M-8k（1兆减去8K）的数据量；

真实数据大小为：1M内存-两页的请求头数据=1M-8K；

应用A直接将数据拷贝到应用B的物理内存空间中，数据量不能超过1M-8K；拷贝次数少了一次，少了从服务端拷贝到用户；

___

**IPC通信机制：**

-   服务注册
-   服务发现
-   服务调用

以下为简单的主进程和子进程通信：

1、服务注册： 缓存中心中有三张表(暂时理解为三个HashMap，Binder用的是native的[红黑树](https://zhida.zhihu.com/search?content_id=224447353&content_type=Article&match_order=1&q=%E7%BA%A2%E9%BB%91%E6%A0%91&zhida_source=entity))：

-   第一种：放key ：String - value：类的Class；
-   第二种：放key ：Class的类名 - value：类的方法集合；
-   第三种：放key ：Class的类名 - value：类的对象；

类的方法集合：key-value;

key：方法签名：“方法名” 有参数时用 “方法名-参数类型-参数类型-参数类型......”;

value: 方法本身;

注册后，服务若没被调用则一直处于沉默状态，不会占用内存，这种情况只是指用户进程里自己创建的服务，不适用于AMS这种；

2、服务发现： 当被查询到时，要被初始化；

-   客户端B通过发送信息到服务端A
-   服务端解析消息，反序列化
-   通过反射得到消息里的类名，方法，从注册时的第一种、第二种表里找到Class，若对象没初始化则初始化对象，并将对象添加到第三种的表里；

3、服务调用：

-   使用了[动态代理](https://zhida.zhihu.com/search?content_id=224447353&content_type=Article&match_order=1&q=%E5%8A%A8%E6%80%81%E4%BB%A3%E7%90%86&zhida_source=entity)
-   客户端在服务发现时，拿到对象（其实是代理）
-   客户端调用对象方法
-   代理发送序列化数据到服务端A
-   服务端A解析消息，反序列化，得到方法进行处理，得到序列化数据结果
-   将序列化结果写入到客户端进程的容器中；
-   回调给客户端

**AIDL：** BpBinder：数据发送角色 BbBinder：数据接收角色

![](https://pica.zhimg.com/v2-6d2d0dcc2ead214cdfc4b68da6c1bb44_1440w.jpg)

编译器生成的AIDL的java接口.Stub.proxy.transact()为数据发送处；

发送的数据包含：数据+方法code+方法参数等等；

-   发送时调用了Linux的驱动
-   调用copy\_from\_user()拷贝用户发送的数据到内核空间
-   拷贝成功后又进行了一次请求头的拷贝：[copy\_from\_user](https://zhida.zhihu.com/search?content_id=224447353&content_type=Article&match_order=2&q=copy_from_user&zhida_source=entity)()
-   也就是把一次的数据分为两次拷贝

请求头：包含了目的进程、大小等等参数，这些参数占了8K

编译器生成的AIDL的java接口.Stub.onTransact()为数据接收处；

**Binder中的IPC机制：**

-   每个App进程启动时会在内核空间中映射一块1M-8K的内存
-   服务端A的服务注册到ServiceManager中：服务注册
-   客户端B想要调用服务端A的服务，就去请求ServiceManager
-   ServiceManager去让服务端A实例化服务：服务发现
-   返回一个用来发送数据的对象BpBinder给到客户端B
-   客户端B通过BpBinder发送数据到服务端A的内核的映射区域（传参时客户端会传一个reply序列化对象，在底层会将这个地址一层一层往下传，直至传到回调客户端）：这里发生了一次通信copy\_from\_user：服务调用
-   服务端A通过BBBinder得到数据并处理数据
-   服务端唤醒客户端等待的线程；将返回结果写入到客户端发送请求时传的一个reply容器地址中,调用onTransact返回；
-   客户端在onTransac中得到数据；通信结束；

ServiceManager维持了Binder这套通信框架；

### 2.APP多进程的优点

-   扩大应用可使用的内存 手机内存6G，系统分配给虚拟机的内存一般32M、48M、64M，使用多进程时，可以使用一个进程专门加载图片，防止`OOM`。
-   子进程崩溃，不会导致主进程崩溃
-   互相保活，即如果子进程被系统kill掉时，主进程拉起子进程。主进程被系统kill掉时，子进程拉起主进程。

### 3.多进程通信原理

![](https://pic2.zhimg.com/v2-9a2cf7fb82711764f1b80609c433f25f_1440w.jpg)

Android进程是运行在系统分配的[虚拟地址空间](https://zhida.zhihu.com/search?content_id=224447353&content_type=Article&match_order=1&q=%E8%99%9A%E6%8B%9F%E5%9C%B0%E5%9D%80%E7%A9%BA%E9%97%B4&zhida_source=entity)，虚拟地址空间分为用户空间和内核空间。多进程间，用户空间不共享，内核空间共享，进程间通过共享的内核空间通信。

### 4.多进程通信有哪些方式？

1.传统的`IPC`方式：`socket`，内存共享。  
2.Android特有的方式：`Binder`。

### 5.`Binder`相对其他`IPC`方式优点/为什么使用`Binder`？

![](https://pic3.zhimg.com/v2-25175b2e0bc0535e1b3d680b80ccf83c_1440w.jpg)

**1.性能：**

**A.`Socket`传输数据的过程：两次拷贝**

![](https://pic4.zhimg.com/v2-c9588dd7826a2fbc17d5fd6b49a10bc3_1440w.jpg)

**B.`Binder`传输数据的过程：一次拷贝**

![](https://pic1.zhimg.com/v2-f7c15aff4899513d5597afc74e93a60e_1440w.jpg)

内存映射：`MMAP`（`memory map`）

虚拟内存和物理内存

虚拟内存映射到物理内存，物理内存存储数据。

2.易用性

3.安全性

### 6.Binder在Android系统CS通信机制中起到的作用

-   `Android C/S`通信机制

![](https://pic2.zhimg.com/v2-82b35be42cff11a3a2b7bbb53ed58873_1440w.jpg)

-   `Binder`机制的关键概念

![](https://pica.zhimg.com/v2-4ce1a9012c5ad74a3c5f4d69399955b4_1440w.jpg)

-   `Binder`在`Android CS`通信机制中起到的作用

![](https://pic3.zhimg.com/v2-571727d5f0eebc95e02be1f1040a582a_1440w.jpg)

![](https://pic3.zhimg.com/v2-eae53b5d79ed71142083b5c81725d5f6_1440w.jpg)

![](https://pica.zhimg.com/v2-0c1f02f6447fc9c10c46fbc9ecf34072_1440w.jpg)

![](https://pic2.zhimg.com/v2-f24280640b438c445e958e49c9cc08b3_1440w.jpg)

`AIDL`和`Binder`的关系？ `AIDL`封装了`Binder`，`AIDL`调用`Binder`

### Android 学习笔录

**Android 面试题锦：[https://qr18.cn/CKV8OZ](https://link.zhihu.com/?target=https%3A//qr18.cn/CKV8OZ)  
Android Framework底层原理篇：[https://qr18.cn/AQpN4J](https://link.zhihu.com/?target=https%3A//qr18.cn/AQpN4J)  
Android 性能优化篇：`[https://qr18.cn/FVlo89](https://link.zhihu.com/?target=https%3A//qr18.cn/FVlo89)`**  
**Android 车载篇：`[https://qr18.cn/F05ZCM](https://link.zhihu.com/?target=https%3A//qr18.cn/F05ZCM)`**  
**Android 音视频篇：`[https://qr18.cn/Ei3VPD](https://link.zhihu.com/?target=https%3A//qr18.cn/Ei3VPD)`**  
**Jetpack全家桶篇（内含Compose）：`[https://qr18.cn/A0gajp](https://link.zhihu.com/?target=https%3A//qr18.cn/A0gajp)`**  
**Kotlin 篇：`[https://qr18.cn/CdjtAF](https://link.zhihu.com/?target=https%3A//qr18.cn/CdjtAF)`**  
**Flutter 篇：`[https://qr18.cn/DIvKma](https://link.zhihu.com/?target=https%3A//qr18.cn/DIvKma)`**  
**Android 八大知识体：`[https://qr18.cn/CyxarU](https://link.zhihu.com/?target=https%3A//qr18.cn/CyxarU)`**  
**Android 核心笔记：`[https://qr21.cn/CaZQLo](https://link.zhihu.com/?target=https%3A//qr21.cn/CaZQLo)`**