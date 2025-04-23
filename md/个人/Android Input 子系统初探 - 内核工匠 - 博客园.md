---
created: 2025-04-15T13:09:32 (UTC +08:00)
tags: []
source: https://www.cnblogs.com/Linux-tech/p/13873883.html
author: 内核工匠
---

# Android Input 子系统初探 - 内核工匠 - 博客园

> ## Excerpt
> Android系统基于Linux内核实现，内核作为整个操作系统的核心，对下，它负责整个硬件的驱动、实现对硬件器件的控制管理；对上，它提供各种系统所需的核心功能。Android系统支持的输入设备较多，如按键、触摸屏、手柄等，面对种类繁杂的输入设备，内核通过抽...

---
Android系统基于Linux内核实现，内核作为整个操作系统的核心，对下，它负责整个硬件的驱动、实现对硬件器件的控制管理；对上，它提供各种系统所需的核心功能。Android系统支持的输入设备较多，如按键、触摸屏、手柄等，面对种类繁杂的输入设备，内核通过抽象化的方式来使得各输入设备的的核心处理流程统一化，细节处理流程差异化（通过不同类型的回调实现），这就是Input子系统所要完成的内容，总结来说，它在内核中主要作用为：

-   规范化Input Device的定义方式及其数据的上报格式；
    
-   规范化Input Handler的定义方式及其需要实现的回调；
    
-   为Input Device和Input Handler提供核心服务；
    
-   提供标准化用户空间接口；
    

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X2pwZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNSDhTQW9kMjBGeUdtVmdxSjZhV3F4ZWljNXJpYXhKVFQ0V2ljajlzVTJpYVNRbVkzUWtPaWJtaWFvQVFRLzY0MA?x-oss-process=image/format,png)

图1 Input事件处理整体框图

整体的Input事件处理框图如图1所示，本文主要围绕这张图来详述Input子系统的各个方面，如定义、初始化、注册匹配、事件传递、与用户空间的交互等。介于本人理解有限，如有叙述不当的地方，还请谅解指出。

**一．Input子系统相关定义**

在前序内容中，我们提前用到了Input Device、Input Handler等名词，但还没有进行相关的解释说明。本节的内容旨在了解Input子系统中几个重要的结构体定义，以便于Input子系统的后续介绍。

**1. Input\_dev**

struct input\_dev用来抽象所有的输入设备，由于不同的输入设备上报的事件或形式存在差异，抽象的input\_dev必然需要包含差异的内容，形成一种x+(y1、y2、y3..)的方式（其中，x为所有输入设备共有的成员，y1/y2/y3为输入设备差异化成员），所以在实际的特定输入设备驱动开发工作中，只需要填充部分成员即可完成input\_dev的定义。详细的成员定义说明如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNRmhRMEhnaWM1QzBFems4aWEwSzV5c1o1QjloejNHbnVHWmlhdENjaG81ckp4MHJwQ3hBNjkycmhRLzY0MA?x-oss-process=image/format,png)

**2. Input\_handler**

struct input\_handler用于抽象事件处理，不同的输入设备对应的事件处理方式会存在差异，linux内核抽象该结构体保证input事件的处理流程一致，具体的实现部分通过input\_handler的函数指针回调完成，主要包括匹配、建立连接、事件传递/过滤等，具体的成员说明如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNbVdhTG1lVEFVQ2Z4OUo3NEwwT0g3ZHNBZ2liUklUTXU0WGV5N2lhaFUwaDdvSjVTYnhjdzI3M0EvNjQw?x-oss-process=image/format,png)

**3. Input\_handle**

在抽象input\_dev和input\_handler之后，我们知道一个input\_dev上报的事件可以被多个input\_handler接收处理，一个input\_handler也可以处理多个input\_dev上报的事件，这样多个input\_dev和多个input\_handler之间可能会形成交织的网状（如下图2）。在这种情况下，需要一个桥梁来搭建两者之间的联系，两边的函数调用都可以通过这个“中介”进行，input\_handle就是这个桥梁。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X2pwZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNNURPeUZET3hCdGFpYXBaZVp3NnppYmtHbmJjNzNlUTlrc3BjUm4wNllTdVpJUkNtOTFpYUlFSkRRLzY0MA?x-oss-process=image/format,png)

图2 device与handler示意图

Input\_handle的定义比较简单，各成员说明如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNWnc2Q3Z0U0E3bjZOeXUybTFtak9yeEJMeHplN0NMS3RmYzZKNlVPRlNmRkI5OU5pYm9XT1dFUS82NDA?x-oss-process=image/format,png)

**二．Input子系统相关流程**

主要流程包括input core初始化、input设备注册、input\_handler注册、input设备与input\_handler匹配、input事件传递。

**1. input core初始化**

Input core通过sybsys\_initcall注册设定启动等级，保证其初始化会早于input设备和input\_handler的注册（module\_init方式注册），在初始化过程中主要完成：

1）input类注册；

2）Proc文件创建，主要用于input\_handler和devices信息查看；

3）注册字符设备，主设备号为13；

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNYVBoNzhNd3pKanFITVFQTzRMTmIwbWYycEdreWxHMUlxMmFvZzV1UjF5eENxZjlKR2FzOGFRLzY0MA?x-oss-process=image/format,png)

**2. input\_dev注册**

一般而言，Input设备驱动需要完成设备的控制和响应上报，其中响应上报是通过注册的input\_dev来完成，所以input\_dev的注册需要在input设备驱动的初始化过程中调用input\_register\_device完成，input\_register\_device执行的过程说明如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNTnpXUWgwOWhLR2JGY0RkeG9nWWtZWUZRZmFJUjBtS2lhZmNHdmhDbkJXWENHdUhtZDZrOWw0Zy82NDA?x-oss-process=image/format,png)

**3. input\_handler注册**

Input handler的注册相对于input设备的注册更为简单，在填充struct input\_handler后，直接调用input\_register\_handler完成handler的注册，input\_register\_handler的处理流程如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNZVp5eWdBZ3ZKWGliWk1OdkZUTVlzWjlkWU5uSnp4T2ZMRlMzNUtHZ1hna2xQTGJhaWM0b3BEcHcvNjQw?x-oss-process=image/format,png)

**4. input\_handler与input\_dev匹配**

Input\_handler与input\_dev的注册最终都会调用input\_attach\_handler完成自己与“相亲对象”的配对，配对完成后input\_dev、input\_handler、input\_handle之间的关系如图3所示，设备驱动和事件处理层驱动都可以通过自身访问到input\_handle，然后通过input\_handle访问到自己的“对象”，具体的匹配代码说明如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X2pwZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNS0pGb0hpY0hpYXBLMkpmMmljMnhqaWNUOXFKVW1hdW80OGZsWUF3VGF3TnpHMlV0c3RKOUpvc2F6US82NDA?x-oss-process=image/format,png)

图3 input\_handle与owner关系图

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNU2lidVB4SmljdFpZVEtoWDRjNmZkeGFKeDRCbmduSHBUOHNQaWN0Wmp5bE1pYVFpYmJnYldIT1FlOWcvNjQw?x-oss-process=image/format,png)

当handler中match回调没有实现时只用根据input\_dev中的id与input\_handler中id\_table包含的id进行匹配，如evdev\_handler匹配所有Input设备，故所有的input设备都可以通过dev/input/event\*路径获取原始上报数据。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNSjJyOXRib2w4ZkpZMzc0ejBacUJZejh4akZheHF5aWJWeE15a3VTNEdmQnpKOFE5R0VRaWFpYnZ3LzY0MA?x-oss-process=image/format,png)

在匹配成功后会调用connect回调，举例evdev\_handler中的connect内容如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNUmZVSGFKdWY2RjA2R1J3anNsdG0ya2liUGp5bVNhbUdmOGhxTU9UQWp1MEliZGliY0JQWHRpY0ZnLzY0MA?x-oss-process=image/format,png)

**5. Input事件传递**

不同的input设备上报的input事件的格式不同，比如触摸屏上报input事件时一般需要上报手指的id、x坐标、y坐标等信息（如图4为B协议报点格式， A协议报点无需上报id，会在inputReader中重新分配）。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNOHQ0d2ljWDcxMkJpYVYyaWNCeWpJUDVydmh3UGVVY0NkVGVIRzlWZGliM0RLd2hibnZ6aWJ2N005aWJRLzY0MA?x-oss-process=image/format,png)

图4 触摸屏报点事件格式

每一个事件上报都是通过input\_event接口来完成，在判定事件类型是否支持后，主要是调用input\_handle\_event来完成：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNOWtFc3JEajA0RGZWejFQdDVpY1c2aERzaWE2Uk5pYlhOZGljR2ZON1hJUGd1clA0VHRtUXNkRFF6QS82NDA?x-oss-process=image/format,png)

该接口中，首先根据type、code判定该事件的disposition，当disposition为INPUT\_PASS\_TO\_DEVICE时，将该事件传递给input\_dev设备自身的event函数处理；当disposition为INPUT\_PASS\_TO\_HANDLERS时，即将该事件传递给事件处理层处理，此处一般是将所有的事件存储在dev的vals数组中（此处，在disposition为INPUT\_SLOT时表明上次处理的点与本次不同，故多添加一个ABS\_MT\_SLOT事件）；当disposition为INPUT\_FLUSH时或者传递的事件达到数组的极限时才将事件传递给事件处理层处理（ input\_sync时，disposition才能取得INPUT\_FLUSH这个值）。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNanBxZjRQUVRyVG9PUllibVg5Q3R1MXlQUVJBelA5MmFUMzZsQ3llYmhoNkk0T3RvbTdNeDhnLzY0MA?x-oss-process=image/format,png)

input\_get\_disposition函数是将根据type和code判定事件的disposition，此处只关心EV\_SYN、EV\_KEY、EV\_ABS事件。EV\_KEY事件中当设置了按键自动重发时的value值为2，!!test\_bit(code, dev->key) != !!value语句中都进行了两次取反操作是为了避免出现0、1之外的数据，如果本次上报的按键事件与上次不同才会进行上报给事件处理层（dev->key保存了最近按键事件的所有状态），否则不予处理。

在收到sync事件或者event buffer size接近最大值时开始同步事件，此时传递的为一包事件，input\_pass\_values接口主要是寻找input\_dev设备对应的handle处理存储的数据，另外设置的输入设备支持EV\_REP事件，则会在此处设置定时器自动重发按键事件（按键值为2）。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNN3d1eFlmTmFjdjdyUmswVWRxNW93RTNvNXFhOEhFYTZRMjJDclBJWkxRQ3E3cnoxemxQaENBLzY0MA?x-oss-process=image/format,png)

所有的input设备都会和evdev\_handler匹配，此处假设匹配的handler为evdev\_handler，则events指向的函数为evdev\_events：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNVkRVdVlBVzBFNWZDTWJxb3BIN09xUmczTXhpYXZxdzh0TExrNGdKeTd6VlVpYWhrcnFSVUxsNkEvNjQw?x-oss-process=image/format,png)

evdev\_pass\_values只是将传过来的所有事件存储在client->buffer中；kill\_fasync函数用于发送通知事件，告诉上层client->buffer中有数据可以读了。

**6. Input事件传递给用户空间**

当应用层或框架层调用read函数读取/dev/input/event\*文件时，会调用evdev\_read返回数据，其中event\_fetch\_next\_event是判断client->buffer这个循环缓冲区中的头尾指针是否相等（相等时buffer中没有数据），不相等时取出一个input\_event类型的事件放入到event中；input\_event\_to\_user函数是将此事件copy到应用层，input\_event\_size函数是用来获取一个input\_event事件的大小，循环复制client->buffer中的事件到应用层的buffer中。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTTlXV0JzVnNVcGlhR21BUGlhVEFKSXNNbm00MVVmS3lpYk5tenRQaWFRMmFsZ3hYeW1qMjhab0ZIeFk5ZTk4WENhajk3TXZ0YTRnYjBNUGcvNjQw?x-oss-process=image/format,png)

上层谁会来打开文件读这些事件？一种是getevent工具，另外一种是android框架层的inputflinger服务，其主要会创建InputReader和InputDispatcher两个线程。InputReader负责与底层的事件打交道，其先通过eventHub读取所有的事件， 然后通过设备属性或事件特征找到对应的mapper处理将底层事件转换为android设计的事件类型；InputDispatcher负责与窗口打交道，将收到的事件派发给对应注册的窗口。

**参考文献**

1. https://www.cnblogs.com/lifexy/p/7542989.html

2. https://blog.csdn.net/qq\_39937242/article/details/82631165

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X2dpZi9kNGhvWUpseE9qTlNyNE9XSjlrdWtpYkZuc3h2U3pkSWlicjJqRDVZVDNqQU1mOWVrZDJDc0I3ME9HallxbjBKdFB3QjFtSXkxWlduQ216R1JMSzJFaWN5dy82NDA?x-oss-process=image/format,png)

**扫码关注**  
**“内核工匠”微信公众号**  
Linux 内核黑科技 | 技术文章 | 精选教程
