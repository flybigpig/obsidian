# Android Input事件传递流程梳理

一、Android输入系统介绍

1）相关模块

-   **InputReader: 负责从硬件获取输入，转换成事件（Event), 并传给Input Dispatcher。**
-   **InputDispatcher: 将InputReader传送过来的Events分发给合适的窗口，并监控ANR。**
-   **InputManagerService：负责InputReader 和 InputDispatcher的创建，并提供Policy 用于Events的预处理。**
-   **WindowManagerService：管理InputManager、View（Window）以及ActivityManager 之间的通信。**

-   **View&Activity：接收按键并处理。**
    

-   **ActivityManagerService：ANR 处理。**

**2）相关进程**  
system\_server 与 应用进程。  

**3）进程对应的主要工作线程**  
其中system\_server中包含InputReaderThread和InputDispatcherThread。  
应用进程相关的主要是 UIThread。

二、Input事件传递流程

1）`TP事件形成`：屏幕的Firmware按一定频率扫描到电流变化开始计算触摸的位置并上报，报点信息通过TP driver处理最终写入相关设备节点（/dev/input/eventXXX）。

2）`事件获取`：EventHub收集底层硬件设备tp报点。打开"/dev/input/"目录下的input设备，并将其注册到epoll的监控队列中。一旦对应设备上有可读的input事件，马上包装成event，上报给InputReader。

3）`事件读取`：InputReader获取到事件后，通过DeviceId和对应的InputMapper来确认是什么设备的什么类型事件，并对其进行首次数据结果封装，结果放入InputDispatcher的mInboundQueue中等待被分发处理。

4）`事件分发`：InputDispatcher从mInboundQueue队头取出事件，寻找焦点窗口，确认InputChannel连接是否有效（InputChannel注册是在WMS创建窗口时候做的），一切就绪就把当前事件放入outBoundQueue, 然后将事件发送给应用进程，同时将当前事件对象从outBoundQueue转移到waitQueue.这里要注意ANR的问题，如果下一个Input事件到来，发现当前waitQueue队列不为空，且头事件分发超过了500ms，那么就开始ANR计时，超过5S会生成ANR对应的command命令，在下一次触发command命令时走ANR流程。

Input事件在系统层面传递的整体流程图如下：

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/48JRFhNgRqIKGkAnsIsN7mSc6dibic5yC7MfLaMES90CCiapBsjNKcqINubUAobMVaCr1og92q3bkE6tQvniafyV1lLd1W7583vibzGAolMOc46s/640?wx_fmt=other&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=0)

  
5）`事件消费`：应用进程获取到事件，按事件类型匹配InputStage具体实现类来负责消费事件，以touch事件为例，下面就是事件分发流程。

6）`事件分发`：dispatchTouchEvent(分发)、onInteceptTouchEvent(过滤)、onTouchEvent(处理)。其中onInteceptTouchEvent只有ViewGroup才有。

-   如果onInteceptTouchEvent拦截，则交给当前ViewGroup来消费，如果不拦截，事件向下传递给子类。onTouchEvent消费就自己处理，不消费就向上传递给父类去消费。
    

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/48JRFhNgRqIfu9pGrSr599icbGibicDLEa6ultOiamhE2p6o8OkhQLn58rB9QKqUAYZbepT1n29p4DBT9ZsG2o9C0zFj86wcF8XbJokm0lyV5aI/640?wx_fmt=other&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=1)

事件分发规则

-   事件传递顺序：Activity > ViewGroup > View
    
-   不同事件类型消费顺序：onTouchListener.onTouch > onTouchEvent > OnClickListener.OnLongClickListener >OnClickListener.onClick。
    

![图片](https://mmbiz.qpic.cn/sz_mmbiz_jpg/48JRFhNgRqL58nbGP1oLA8Sw2gCmNOAfbaIqJrO9d2mxsoJ8MTYMGvzicnscAEv4Yp2kRGsNOZzIJpnQCIibUXNMONvibe8f8mQd7594g1y47E/640?wx_fmt=other&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=2)
