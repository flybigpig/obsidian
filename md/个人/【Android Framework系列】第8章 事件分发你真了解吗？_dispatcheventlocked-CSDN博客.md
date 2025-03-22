## 1 事件分发基本认知

### 1.1 事件分发的”事件“是指什么 

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6ce8891bf197bad40d762d208cb3ff1f.png)

### 1.2 事件处理中涉及到的点

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b63445809fc60c5bd5dfeaf6bf5ab8c6.png)

### 1.3 [Android](https://so.csdn.net/so/search?q=Android&spm=1001.2101.3001.7020) 事件处理的三个流程

在Android中，`Touch`事件的分发分`服务端`和`应用端`。在[服务端](https://so.csdn.net/so/search?q=%E6%9C%8D%E5%8A%A1%E7%AB%AF&spm=1001.2101.3001.7020)由`WindowManagerService（借助InputManagerService）负责`采集和分发的，在应用端则是由`ViewRootImpl（内部有一个mView变量指向View树的根，负责控制View树的UI绘制和事件消息的分发）负责`分发的。  
当[输入设备](https://so.csdn.net/so/search?q=%E8%BE%93%E5%85%A5%E8%AE%BE%E5%A4%87&spm=1001.2101.3001.7020)可用时，比如触屏，`Linux`内核会在`/dev/input`中创建对应的设备节点，输入事件所产生的原始信息会被`Linux`内核中的`输入子系统采集`，原始信息由`Kernel Space的驱动层`一直传递到`User Space的设备节点`  
`IMS`所做的工作就是监听`/dev/input`下的所有的设备节点，当设备节点有数据时会将数据进行加工处理并找到合适的`Window`，将输入事件派发给他  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b841ee3ab85653c1b9a3c1a027664f1b.png)

### 1.4 事件的本质

将点击事件（`MotionEvent`）向某个View进行传递并最终得到处理，即当一个点击事件发生后，系统需要将这个事件传递给一个具体的View去处理。这个事件传递的过程就是分发过程。

> adb shell getevent  
> adb shell input event 4  
> adb shell getevent -t -l

### 1.5 Linux - Posix函数了解

#### ePoll

被公认为Linux2.6下性能最好的多路I/O就绪通知方法。`epoll`只有`epoll_create`,`epoll_ctl`,`epoll_wait` 3个系统调用。

##### epoll\_create

当**创建**好`epoll`句柄后，它就是会占用一个`fd`值，在`linux`下如果查看`/proc/进程id/fd/`，是能够看到这个`fd`的，所以在使用完`epoll`后，必须调用`close()`关闭，否则可能导致`fd`被耗尽。

##### epoll\_ctl

`epoll`的**事件注册函数**，它不同于`select()`是在监听事件时告诉内核要监听什么类型的事件，而是在这里先注册要监听的事件类型。

##### epoll\_wait

收集在`epoll`监控的事件中已经发送的事件。参数`events`是分配好的`epoll_event`结构体数组，`epoll`将会把发生的事件赋值到`events`数组中（events不可以是空指针，内核只负责把数据复制到这个events数组中，不会去帮助我们在用户态中分配内存）。`maxevents`告之内核这个`events`有多大，这个 `maxevents`的值不能大于创建`epoll_create()`时的`size`，参数`timeout`是超时时间（毫秒，0会立即返回，-1将不确定，也有说法说是永久阻塞）。如果函数调用成功，返回对应I/O上已准备好的文件描述符数目，如返回0表示已超时  
[具体参考文档](https://blog.csdn.net/xiajun07061225/article/details/9250579)

#### iNotify

**它是一个内核用于通知用户空间程序文件系统变化的机制**

```
// 每一个 inotify 实例对应一个独立的排序的队列
int fd = inotify_init ();
```

```
// fd 是 inotify_init() 返回的文件描述符，path 是被监视的目标的路径名（即文件名或目录名），
// mask 是事件掩码, 在头文件 linux/inotify.h 中定义了每一位代表的事件。
// 可以使用同样的方式来修改事件掩码，即改变希望被通知的inotify 事件。
// Wd 是 watch 描述符。
inotify_add_watch
```

[具体用法参考](https://blog.csdn.net/breakout_alex/article/details/89028868)

#### socketpair

`socketpair()`函数用于创建一对无名的、相互连接的套接子。  
[具体使用参考](https://blog.csdn.net/weixin_40039738/article/details/81095013)

#### fd

`linux`中， 每一个进程在内核中，都对应有一个“打开文件”数组，存放指向文件对象的指针，而 **fd 是这个数组的下标**。

> 我们对文件进行操作时，系统调用，将fd传入内核，内核通过fd找到文件，对文件进行操作。  
> 既然是数组下标，fd的类型为int， < 0 为非法值， >=0 为合法值。  
> 在linux中，一个进程默认可以打开的文件数为1024个，fd的范围为0~1023。可以通过设置，改变最大值。  
> 在linux中，值为0、1、2的fd，分别代表标准输入、标准输出、标准错误输出。

## 2 事件采集流程

### 2.1 从内核到IMS过程

#### 2.1.1 流程示意

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/4ec8cf4494742f4295906abd45faa101.png)

#### 2.1.2 EventHub

##### 2.1.2.1 INotify

`INotify`**是Linux提供的一种文件系统变化通知机制**，什么叫文件系统变化？创建，删除，读写通通叫做变化，使用如下代码就可以将某个目录加入到INotify中

```
int inotifyFd = inotify_init();
int wd = inotify_add_watch(inotifyFd, "/dev/input", IN_CREATE | IN_DELETE);
```

上述两行代码就将`/dev/input`加入到了`INotify`中，这样，对于外部输入设备的插拔就可以很好的被检测到了。可不幸的是，`INotify`发现文件系统变化后不会主动告诉别人，它需要主动通过`read()`函数读取`inotifyFd`描述符来获取具体变化信息。也就是说，你插入了鼠标，`INotify`马上就知道了这个信息，并且将信息更新到了`inotifyFd`描述符对应的对象当中

##### 2.1.2.2 Eventhub

**`Epoll`可以用来监听多个描述符的可读写`inotifyFd`状态**，什么意思呢？比如说上面说到了外部输入设备插拔可以被INotify检测到，并且将相信信息写入到inotifyFd对用的对象中，但是我没法知道INotify什么时候捕获到了这些信息。**而`Epoll`可以监听`inotifyFd`对应的对象内容是否有变化，一旦有变化马上能进行处理，平常大部分时间没监听到变化时睡大觉**  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c8d8f14c65bc8e24770b78f06923642e.png)  
**`epoll_wait`大部分时间处于阻塞状态（这点和socket等待连接很相似），一旦`/dev/input/event0`节点有变化（即产生了输入事件），`epoll_wait`会执行完毕**

#### 2.1.3 IMS

##### 2.1.3.1 InputReaderThread

`InputReader`会不断的循环读取`EventHub`中的`原始输入事件`

##### 2.1.3.2 InputDispatcherThread

`InputDispatcher`中保存了`WMS`中的所有的`Window信息`（`WMS`会将窗口的信息实时的更新到`InputDispatcher`中），这样`InputDispatcher`就可以将输入事件派发给合适的`Window`

##### 2.1.3.3 整体流程源码分析

###### 2.1.3.3.1 由SystemServer启动一个IMS进程进程，然后进行各个对象的初始化

SystemServer.startOtherServices  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/13cd34e2fc19c816ca9bca3142c642c6.png)  
InputManagerService.InputManagerService  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c5696f100657c530410ca2d5c6aaddad.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/62acff7cf753c6c66b91e5e6ddcd30be.png)

###### 2.1.3.3.2 同步会在native层进行初始化，开启一个InputManager对象|

开启两个线程：  
**一个Reader线程用来读取触摸信号  
一个Dispatcher用来转发信息**  
EventHub负责采集底层信号  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/77a2eaaca87926c2335ca4edd2235b0b.png)  
com\_android\_server\_input\_InputManagerService.cpp  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/18bcc2c6c07ba079e274f7d0ebd13491.png)  
com\_android\_server\_input\_InputManagerService.cpp  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/871975056077bc4268ba78e55c3afca9.png)  
InputManager.cpp.InputManager  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/58c089b0ced3239c734a51fd947d1c92.png)  
InputManager.cpp.initialize  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6cc708c2e1a6987d1ede0b7e36c054b5.png)  
InputReader.cpp.InputReader  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/685e9fb44b8293e6c95882d9888d9e97.png)  
InputDispatcher.cpp.InputDispatcher  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/4b75b8d6a955b1c0eabe0171b68616eb.png)

###### 2.1.3.3.3 通过IMS.start启动上面两个线程

SystemServer.startOtherService  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/efe21499c34c26f37bf538f05832a4c8.png)  
InputManagerService.start  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5e8036dcaec25916d423cb6ed132ad96.png)  
com\_android\_server\_input\_InputManagerService.cpp  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/329d2c5ac870fa00ad071b372ebb91f4.png)  
Inputmanager.cpp  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/ebf38c374be48052587a2447bdbfe3c3.png)

###### 2.1.3.3.4 InputRerader线程处理过程

整体示意  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d466fbaaa1e54f493bc9a053f9744f83.png)

###### 2.1.3.3.4.1 线程运行

InputReader.cpp  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/00f8e3defafadb17a0f43eccaf0c695a.png)

###### 2.1.3.3.4.2 处理事件，注意getEvent的阻塞问题

InputReader.cpp

```
void InputReader::loopOnce() {
358    int32_t oldGeneration;
359    int32_t timeoutMillis;
360    bool inputDevicesChanged = false;
361    Vector<InputDeviceInfo> inputDevices;
362    { // acquire lock
363        AutoMutex _l(mLock);
364
365        oldGeneration = mGeneration;
366        timeoutMillis = -1;
367         //查看InputReader配置是否修改
368        uint32_t changes = mConfigurationChangesToRefresh;
369        if (changes) {
370            mConfigurationChangesToRefresh = 0;
371            timeoutMillis = 0;
372            refreshConfigurationLocked(changes);
373        } else if (mNextTimeout != LLONG_MAX) {
374            nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
375            timeoutMillis = toMillisecondTimeoutDelay(now, mNextTimeout);
376        }
377    } // release lock
378      //从Eventhub中读取事件,这里注意因为之前将的EventHub.getEvent中有epoll_wait进行阻塞，若FD发生改变则执行后面代码
379    size_t count = mEventHub->getEvents(timeoutMillis, mEventBuffer, EVENT_BUFFER_SIZE);
380
381    { // acquire lock
382        AutoMutex _l(mLock);
383        mReaderIsAliveCondition.broadcast();
384
385        if (count) {//处理事件
386            processEventsLocked(mEventBuffer, count);
387        }
388
389        if (mNextTimeout != LLONG_MAX) {
390            nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
391            if (now >= mNextTimeout) {
392#if DEBUG_RAW_EVENTS
393                ALOGD("Timeout expired, latency=%0.3fms", (now - mNextTimeout) * 0.000001f);
394#endif
395                mNextTimeout = LLONG_MAX;
396                timeoutExpiredLocked(now);
397            }
398        }
399
400        if (oldGeneration != mGeneration) {
401            inputDevicesChanged = true;
402            getInputDevicesLocked(inputDevices);
403        }
404    } // release lock
405
406    // Send out a message that the describes the changed input devices.
407    if (inputDevicesChanged) {
408        mPolicy->notifyInputDevicesChanged(inputDevices);//发送一个消息，该消息描述已更改
409    }
410
411    // Flush queued events out to the listener.
412    // This must happen outside of the lock because the listener could potentially call
413    // back into the InputReader's methods, such as getScanCodeState, or become blocked
414    // on another thread similarly waiting to acquire the InputReader lock thereby
415    // resulting in a deadlock.  This situation is actually quite plausible because the
416    // listener is actually the input dispatcher, which calls into the window manager,
417    // which occasionally calls into the input reader.
418    mQueuedListener->flush();//将时间传递到InputDispatcher
419}
```

###### 2.1.3.3.4.3 向下衍生调用，ProcessEventsForDeviceLocked是处理数据流，下面是针对于文件状态处理

processEventsLocked  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/dcb6694ac5ecb81e10ff29471ad7c436.png)

###### 2.1.3.3.4.4 最终调用Device->process

processEventsForDeviceLocked  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d0eb2472a45453adb1e98cac6f5d2325.png)

###### 2.1.3.3.4.5 调用

InputDevice::process  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/65f1ae8645e5b313a5bb3692be78ae00.png)

###### 2.1.3.3.4.6 InputMapper::process

这里注意InputMapper有多个子类，  
我们要找触摸事件的相关处理就是找到TouchInputMapper  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d930df8fdca5e4f9c4cf60521adb075f.png)

###### 2.1.3.3.4.7 TouchInputManager::sync

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6c31d3f14eabf7c4c2735ae5f80d5fd3.png)

###### 2.1.3.3.4.8 然后最终数据流

1.processRawTouches(false /_timeout_/);  
![4345](https://i-blog.csdnimg.cn/blog_migrate/cc4191412a964ba95ec9169ed3b64b77.png)  
2.cookAndDispatch  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c00bfec474038dbe2021294ec6753ba5.png)  
3.dispatchTouches  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/638ac59fc0922b9356da06c7a7b51bc4.png)  
4.dispatchMotion  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c23be45fbe0b9d6758209345b46bb0bc.png)  
5.当前这个getListener实际获取是mQueuedListener = new QueuedInputListener(listener);实例  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f914f3b862f1f565fcf38edcd35a1fc9.png)  
6.InputLintener.cpp.QueuedInputListener::notifyMotion  
这里我们实际可以看到他就是往一个队列当中添加数据  
实际上是我们将触摸相关的事件进行包装之后，将其加入到一个ArgsQueue队列，  
到此，我们已经将数据加入到参数队列中  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/e9b7619aa8e364d46317377c82670353.png)  
7.这时我们在回到上面processEventsLocked调用之后，发现当前mQueueListener->flush的调用  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/34f4e2f0858be4330b2ceb165e82b591.png)  
8.InputLintener.cpp.QueuedInputListener::flush

遍历整个`mArgsQueue`数组, 在input架构中NotifyArgs的实现子类主要有以下几类

> NotifyConfigurationChangedArgs  
> NotifyKeyArgs  
> NotifyMotionArgs 通知motion事件  
> NotifySwitchArgs  
> NotifyDeviceResetArgs

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9d928babb1288d124d9b591137327216.png)

9.NotifyMotionArgs  
这里的listener根据溯源可以看到是InputDispatcher  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/771efa5fd750723dde081d6dd314fb0b.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9212d48e3c748b04dde0ebb6e9c85d9c.png)  
所以综上分析，是调用InputDispatcher中的notifyMotion,这里就完成了向InputDispatcher的信息传递  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5820fc1eeea2893c9a808376fc2635d5.png)  
10.执行notifyMotion中的唤醒操作  
InputDispatcher.cpp  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/2aa855875120ef44a47b28b0b0df6bcf.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9b8694d0b9d5e3df763ac21bbf814f2a.png)

###### 2.1.3.3.5 InputDispatcher线程处理过程

整体流程示意  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f95df90147e66ae1bda680df261df722.png)

###### 2.1.3.3.5.1 InputDispatcher.cpp.threadLoop()

![4623](https://i-blog.csdnimg.cn/blog_migrate/a91d4d996baaf7b2dc688c93ba31033c.png)

###### 2.1.3.3.5.2 InputDispatcher.cpp.dispatchOnce()

`pollOnce`这里是等待被唤醒，进入`epoll_wait`等待状态，当发生以下任一情况则退出等待状态：  
`callback`：通过回调方法来唤醒；  
`timeout`：到达`nextWakeupTime`时间，超时唤醒；  
`wake`: 主动调用`Looper`的`wake()`方法；  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6c49ad95b5c0b6b1ed08d196c41f6df3.png)

###### 2.1.3.3.5.3 dispatchOnceInnerLocked

![294](https://i-blog.csdnimg.cn/blog_migrate/9a0138707de35a5a0d548634a71f972d.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/e667f0b2374bfbe0552723b2dc47b65c.png)

###### 2.1.3.3.5.4 dispatchMotionLocked

这里两条线，  
1.findTouchedWindowTargetLocked 负责找到要分发的Window  
2.dispatchEventLocked 负责具体分发目标  
![864](https://i-blog.csdnimg.cn/blog_migrate/84cb83c32005867ba3697deb77a495c3.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/e8fd7bc7daf30bf887a8f1b5311196cd.png)  
1.findTouchedWindowTargetLocked 负责找到能够分发的Window  
![1166](https://i-blog.csdnimg.cn/blog_migrate/bdc27039954088a1bf50177c49b2b2cc.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/ee636cf69fc52d1643e1785961376192.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d15504b79f89d85f88854d0acfd528c2.png)  
这是一个很长的方法  
大体是判断这个事件的类型  
获取能够处理这个事件的`forceground window`，如果这个`window`不能够继续处理事件，就是说这个`window`的主线程被某些耗时操作占据，`handleTargetsNotReadyLocked`这个方法主要是判定当前的`window`是否发生ANR如果没有则处理下面的`addWindowTargetLocked`。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9e69da964d489f1969b0a947327068e7.png)  
addWindowTargetLocked  
这里主要的目的是将`inputChannel`添加至`InputTarget`中  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/064e20f0e53a44453bc31bf6ff00df7b.png)

2.dispatchEventLocked 负责具体分发目标  
1.dispatchEventLocked  
![958](https://i-blog.csdnimg.cn/blog_migrate/b7db5df37c5e321fde33e40c83bd19f1.png)

2.prepareDispatchCycleLocked  
![1837](https://i-blog.csdnimg.cn/blog_migrate/cb089a7d1ab3eceda69a9b0156388f62.png)

3.enqueueDispatchEntriesLocked  
该方法主要功能:  
根据dispatchMode来决定是否需要加入outboundQueue队列;  
根据EventEntry,来生成DispatchEntry事件;  
将dispatchEntry加入到connection的outbound队列.  
执行到这里,其实等于由做了一次搬运的工作,将InputDispatcher中mInboundQueue中的事件取出后, 找到目标window后,封装dispatchEntry加入到connection的outbound队列.  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5f7096a22cc39b4f97e57a6039ad3515.png)

4.startDispatchCycleLocked  
startDispatchCycleLocked的主要功能: 从outboundQueue中取出事件,重新放入waitQueue队列

startDispatchCycleLocked触发时机：当起初connection.outboundQueue等于空, 经enqueueDispatchEntryLocked处理后, outboundQueue不等于空。

startDispatchCycleLocked主要功能: 从outboundQueue中取出事件,重新放入waitQueue队列

publishMotionEvent执行结果status不等于OK的情况下：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/dcd89277c981c90bdba08ec9882e9f46.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b3530382984312816944958c955d8b25.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/faad264ad970afcc5f6ecba84ee4b882.png)  
5.至此调用了connection的inputPublisher的publishMotionEvent方法将事件分发消耗。  
InputTransport.cpp  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c15df90900234ad91901c9be3d2e6390.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/113f8724ef1728b619406e07b12bc7aa.png)

### 2.2 从IMS到WMS过程

#### 2.2.1 InputChannel

##### 2.2.1.1 概念

`InputReaderThread`和`InputDispatcherThread`是运行在`SystemServer`进程中的  
用户进程是和其不在同一个进程中的，那么这中间一定也是有进程间的通信机制在里面  
`InputChannel`，主要的核心目的是为了与用户进程进行通信，这个`InputChannel`是由`WMS管理`，触发构建生成

##### 2.2.1.2 模型示意

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5c431fd6944371134e8d4ccdcdaaa13a.png)

##### 2.2.1.3 创建InputChannel过程

1.在`ViewRootImpl`中的`setView`函数中的`session.addToDispaly`会触发`WMS`的`addWindow`调用  
这里注意的是在进行`addTodisplay`之前在java层面构建了一个`InputChannel`对象  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/4ed5ea4a0d80c4568dfce822a6b1979d.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5a414d262a46305164d3d30c1cdb6627.png)  
2.`addWindow`中通过`WindowState`去进行`openInputChannel`  
这里其实就是到native层面去构建一个通信机制，  
考虑性能问题，一般还是依赖于linux底层的通讯机制  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6b1ecedace8f985daae5c06b8936e75c.png)  
3.openInputChannel  
构建并且注册  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d6bfc5ec0af9b54507b72330e217b7f5.png)  
4.InputChannel.openInputChannelPair实际上就是调用了native层去创建一组通信  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d24096416ccd7ebea1bc8e9f158a3529.png)  
注意一下这个函数在InputTransport.cpp  
这里很明显的能看到，构建了两组socket通信

Linux实现了一个`socketpair`调用可以实现上述在同一个文件描述符中进行读写的功能（该调用目前也是`POSIX`规范的一部分 。该系统调用能创建一对已连接的`（UNIX族）无名socket`。在Linux中，完全可以把这一对`socket`当成`pipe`返回的文件描述符一样使用，唯一的区别就是这一对文件描述符中的任何一个都可读和可写  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f66cf21ae6499639b7ab4297c4a1ab30.png)  
5.创建完成后，回到上面3点最后一句，这里是在将构建好的通道注册到InputDispatcher中  
最终调用到InputDispatcher.cpp.registerInputChannel函数  
至此，当前`InputChannel`对象传递至`InputDispatcher`中，且进行了一定封装，同时传入了窗体句柄过来  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/a257186be3db801744747c231abc207f.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/a148e80cf991323fb036f1fc5aba9b6f.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/97418c5572aae523198b59741f2382e0.png)

##### 2.2.1.4 开启输入事件的监听接收事件

1.`setView`中，我们创建了`InputChannel`之后，开启了对于`InputChannel`中输入事件的监听  
ViewRootImpl.setview  
![846](https://i-blog.csdnimg.cn/blog_migrate/eeacd450f02e5f2b856cedd98c2352d0.png)  
这里是调用到父类的构造  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/7b42cfc36b0a82d9372af8056356174e.png)  
2.在native层构建一个监听事件的监听器  
android\_view\_InputEventReceiver.cpp  
![339](https://i-blog.csdnimg.cn/blog_migrate/5e9f68a4064442dd2d004e6df1069f24.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/92bfc6f43c5a3f9e295fa2b12b315d10.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f3a044b19ada43cb932a34de837a4515.png)  
该方法所执行的操作就是对传递的fd添加`epoll`监控，`Looper`会循环调用`pollOnce`方法，而`pollOnce`方法的核心实现就是`pollInner`。  
大致实现内容为等待消息的到来，当有消息到来后，根据消息类型做一些判断处理，然后调用其相关的callback。  
我们当前是对于开启的`socket`的一个监听，当有数据到来，我们便会执行相应的回调。这里对于`InputChannel`的回调是在调用了`NativeInputEventReceiver的handleEvent`方法  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/0f374b1b4fc0025daeee857f5380f863.png)  
3.消息接收到后回调  
android\_view\_InputEventReceiver.cpp::handleEvent  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f8a71e4bfbcd3ca9d443e4dcf47bd7f7.png)  
android\_view\_InputEventReceiver.cpp::consumeEvents  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/7be3c0a01727bbb60f2b10b6278be3d0.png)  
consume  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6a79fb84586498908267a4f08fb890c9.png)  
android\_view\_InputEventReceiver.cpp::consumeEvents  
上面消息接收完毕之后，执行完`consume`后回到`consumeEvent`中，这时根据读取的数据进行判断处理，然后向上`call`到Java函数  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/7a0c39f811d6ccfd2bd4916a8703fc57.png)

##### 2.2.1.5 回调上层JAVA函数

### 2.3 从WMS转入ViewRootImpl过程

1.InputEventReceiver.dispatchInputEvent  
这里调用的InputEvent是下发到子类当中，因为具体实现类为`WindowInputEventReceiver`  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/746676756715aaec55e96c9aa27fadab.png)

2.ViewRootImpl.WindowInputEventReceiver.onInputEvent  
![8182](https://i-blog.csdnimg.cn/blog_migrate/6b8f5d75973b30d1b1797acb7b674e21.png)

3.ViewRootImpl.QueuedInputEvent.enqueueInputEvent  
![7992](https://i-blog.csdnimg.cn/blog_migrate/66daaaa99b0e631f6812e32fa3d15b23.png)

4.ViewRootImpl.QueuedInputEvent.doProcessInputEvents  
![8031](https://i-blog.csdnimg.cn/blog_migrate/2b2aa05b66b32e40e20ff43bc56ce002.png)

5.ViewRootImpl.QueuedInputEvent.deliverInputEvent  
![8080](https://i-blog.csdnimg.cn/blog_migrate/5af57b6407d37a1b9a2b18dfd08e692f.png)

6.在`ViewRootImpl`中，有一系列类似于`InputStage（输入事件舞台）`的概念，  
每种`InputStage`可以处理一定的事件类型，  
子类有：`AsyncInputStage`、`ViewPreImeInputStage`、`ViewPostImeInputStage`等  
对于点击事件来说，`ViewPostImeInputStage`可以处理它，  
`ViewPostImeInputStage`中，有一个`processPointerEvent`方法，  
它会调用`mView`的`dispatchPointerEvent`方法，注意，这里的`mView`其实就是`DecorView`

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f44503524017301284916d79ce98d241.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/86644a5ec6a62cadc4039a1759f4ea10.png)  
注意这里肯定是调用子类`onProcess`  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f422022bb0615d58c8101297f74e9192.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/48fccbd6c5a56fab768f15b08be1a876.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b307c07eb7f969742ceca64753f428ca.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/227acf97003107fc5e1772d55ac8387f.png)  
还记得这里的View是谁？？ `DecorView!`  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6a0e8438fcc2cfc3c09c3d4baafb0048.png)  
这里可以看到最终调用到`View`的`dispatchPointerEvent`类，这里调用的是`dispatchTouchEvent`  
但是这里考虑到实现类是`DecView`，所以调用的是子类的该方法  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/af6ca6747f2ab4c47b661fb320482c64.png)  
这里可以看到他是在对于一个`window`的`callback`调用  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/bf33e931310fbc874e503e196957a75d.png)  
`callback`是在`Activity.attach()`中设置的， 就是当前的`activity`  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d53178f64c29aaab0eff6460387a83c9.png)

所以这里调用的是`Activity`的`dispatchTouchEvent`  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5d19412e33bce8c18bd63940d27c076c.png)

## 3 事件分发过程

### 3.1 责任链模式

**责任链模式**是一种行为模式，为请求创建一个接收者的对象链.这样就避免,一个请求链接多个接收者的情况.进行外部解耦.类似于单向链表结构

#### 3.1.1 优点

1.  **降低耦合度**。它将请求的发送者和接收者解耦。
2.  **简化了对象**。使得对象不需要知道链的结构。
3.  **增强给对象指派职责的灵活性**。通过改变链内的成员或者调动它们的次 序，允许动态地新增或者删除责任。
4.  **增加新的请求处理类很方便**。

#### 3.1.2 缺点

1.**不能保证请求一定被接收**  
2.**系统性能将受到一定影响**，而且在进行代码调试时不太方便，可能会造成循环调用  
3.**可能不容易观察运行时的特征，有碍于除错**

#### 3.1.3 在Android事件分发机制是责任链模式最典型的应用：

`dispatchTouchEvent`就是责任链中的将事件交给下一级处理  
`onInterceptTouchEvent`就是责任链中,处理自己处理事务的方法  
`onTouchEvent`是责任链中事件上报的事件链

### 3.2 事件在哪些对象之间进行传递![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/7817dce3ef42e7a1273a469537c37eb2.png)

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/dfd651f530fded7ff349d00eb1370e34.png)

### 3.3 事件分发顺序

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/66da99283f813e06941715c6a31809ce.png)

### 3.4 事件分发过程由哪些方法协作完成？

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b515e97f54d2bd13a64399e32325b931.png)

### 3.5 关于事件分发消费中的U型链与L型链的阐述

#### 3.5.1 事件分发过程

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/dcc590a57d66fd1ca5717fcf88c0bade.png)  
事件分发机制详细流程

> super：调用父类方法  
> true：消费事件，即事件不继续往下传递  
> false：不消费事件，事件也不继续往下传递 / 交由给父控件onTouchEvent（）处理

##### 3.5.1.1 Activity

`Activity`要做的事情是转发到下层，都没有消费的情况下，最后由`Activity`来收尾兜底

##### 3.5.1.2 ViewGroup

要做的事情

1.  是否在容器层面拦截
2.  该时间是否要分发，取消事件不需要消费
3.  传递子`View`消费之前还要看是不是有效的，是点在了哪一个`View`上
4.  所以需要遍历所有子`View`确定是否存在

源码分析

```
public boolean dispatchTouchEvent(MotionEvent ev) {
        //验证事件是否连续
        if (mInputEventConsistencyVerifier != null) {
            mInputEventConsistencyVerifier.onTouchEvent(ev, 1);
        }
        //这个变量用于记录事件是否被处理完
        boolean handled = false;
        //过滤掉一些不合法的事件：当前的View的窗口被遮挡了。
        if (onFilterTouchEventForSecurity(ev)) {
            //如果事件发生的View在的窗口，没有被遮挡
            final int action = ev.getAction();
            //重置前面为0 ，只留下后八位，用于判断相等时候，可以提高性能。
            final int actionMasked = action & MotionEvent.ACTION_MASK;
            //判断是不是Down事件，如果是的话，就要做初始化操作
            if (actionMasked == MotionEvent.ACTION_DOWN) {
                //如果是down事件，就要清空掉之前的状态，比如,重置手势判断什么的。
                //比如，之前正在判断是不是一个单点的滑动，但是第二个down来了，就表示，不可能是单点的滑动，要重新开始判断触摸的手势
                //清空掉mFirstTouchTarget
                // Throw away all previous state when starting a new touch gesture.
                // The framework may have dropped the up or cancel event for the previous gesture
                // due to an app switch, ANR, or some other state change.
                cancelAndClearTouchTargets(ev);
                resetTouchState();
            }
 
 
            //检查是否拦截事件
            final boolean intercepted;
            //如果当前是Down事件，或者已经有处理Touch事件的目标了
            if (actionMasked == MotionEvent.ACTION_DOWN
                    || mFirstTouchTarget != null) {
                //判断允不允许这个View拦截
                //使用与运算作为判断，可以让我们在flag中，存储好几个标志
                final boolean disallowIntercept = (mGroupFlags & FLAG_DISALLOW_INTERCEPT) != 0;
                //如果说允许拦截事件
                if (!disallowIntercept) {
                    //确定是不是拦截了
                    intercepted = onInterceptTouchEvent(ev);
                    //重新恢复Action，以免action在上面的步骤被人为地改变了
                    ev.setAction(action); // restore action in case it was changed
                } else {
                    intercepted = false;
                }
            } else {
                // There are no touch targets and this action is not an initial down
                // so this view group continues to intercept touches.
                //如果说，事件已经初始化过了，并且没有子View被分配处理，那么就说明，这个ViewGroup已经拦截了这个事件
                intercepted = true;
            }
 
            // Check for cancelation.
            //如果viewFlag被设置了PFLAG_CANCEL_NEXT_UP_EVENT ,那么就表示，下一步应该是Cancel事件
            //或者如果当前的Action为取消，那么当前事件应该就是取消了。
            final boolean canceled = resetCancelNextUpFlag(this)
                    || actionMasked == MotionEvent.ACTION_CANCEL;
 
            // Update list of touch targets for pointer down, if needed.
            //如果需要（不是取消，也没有被拦截）的话，那么在触摸down事件的时候更新触摸目标列表
            //split代表，当前的ViewGroup是不是支持分割MotionEvent到不同的View当中
            final boolean split = (mGroupFlags & FLAG_SPLIT_MOTION_EVENTS) != 0;
            //新的触摸对象，
            TouchTarget newTouchTarget = null;
            //是否把事件分配给了新的触摸
            boolean alreadyDispatchedToNewTouchTarget = false;
            //事件不是取消事件，也没有拦截那么就要判断
            if (!canceled && !intercepted) {
                //如果是个全新的Down事件
                //或者是有新的触摸点
                //或者是光标来回移动事件（不太明白什么时候发生）
                if (actionMasked == MotionEvent.ACTION_DOWN
                        || (split && actionMasked == MotionEvent.ACTION_POINTER_DOWN)
                        || actionMasked == MotionEvent.ACTION_HOVER_MOVE) {
                    //这个事件的索引，也就是第几个事件，如果是down事件就是0
                    final int actionIndex = ev.getActionIndex(); // always 0 for down
                    //获取分配的ID的bit数量
                    final int idBitsToAssign = split ? 1 << ev.getPointerId(actionIndex)
                            : TouchTarget.ALL_POINTER_IDS;
 
                    // Clean up earlier touch targets for this pointer id in case they
                    // have become out of sync.
                    //清理之前触摸这个指针标识,以防他们的目标变得不同步。
                    removePointersFromTouchTargets(idBitsToAssign);
 
                    final int childrenCount = mChildrenCount;
                    //如果新的触摸对象为null（这个不是铁定的吗）并且当前ViewGroup有子元素
                    if (newTouchTarget == null && childrenCount != 0) {
                        final float x = ev.getX(actionIndex);
                        final float y = ev.getY(actionIndex);
                        // Find a child that can receive the event.
                        // Scan children from front to back.
                        //下面所做的工作，就是找到可以接收这个事件的子元素
                        final View[] children = mChildren;
                        //是否使用自定义的顺序来添加控件
                        final boolean customOrder = isChildrenDrawingOrderEnabled();
                        for (int i = childrenCount - 1; i >= 0; i--) {
                            //如果是用了自定义的顺序来添加控件，那么绘制的View的顺序和mChildren的顺序是不一样的
                            //所以要根据getChildDrawingOrder取出真正的绘制的View
                            //自定义的绘制，可能第一个会画到第三个，和第四个，第二个画到第一个，这样里面的内容和Children是不一样的
                            final int childIndex = customOrder ?
                                    getChildDrawingOrder(childrenCount, i) : i;
                            final View child = children[childIndex];
                            //如果child不可以接收这个触摸的事件，或者触摸事件发生的位置不在这个View的范围内
                            if (!canViewReceivePointerEvents(child)
                                    || !isTransformedTouchPointInView(x, y, child, null)) {
                                continue;
                            }
                            //获取新的触摸对象，如果当前的子View在之前的触摸目标的列表当中就返回touchTarget
                            //子View不在之前的触摸目标列表那么就返回null
                            newTouchTarget = getTouchTarget(child);
                            if (newTouchTarget != null) {
                                // Child is already receiving touch within its bounds.
                                // Give it the new pointer in addition to the ones it is handling.
                                //如果新的触摸目标对象不为空，那么就把这个触摸的ID赋予它，这样子，
                                //这个触摸的目标对象的id就含有了好几个pointer的ID了
 
                                newTouchTarget.pointerIdBits |= idBitsToAssign;
                                break;
                            }
                            //如果子View不在之前的触摸目标列表中，先重置childView的标志，去除掉CACEL的标志
                            resetCancelNextUpFlag(child);
                            //调用子View的dispatchTouchEvent,并且把pointer的id 赋予进去
                            //如果说，子View接收并且处理了这个事件，那么就更新上一次触摸事件的信息，
                            //并且为创建一个新的触摸目标对象，并且绑定这个子View和Pointer的ID
                            if (dispatchTransformedTouchEvent(ev, false, child, idBitsToAssign)) {
                                // Child wants to receive touch within its bounds.
                                mLastTouchDownTime = ev.getDownTime();
                                mLastTouchDownIndex = childIndex;
                                mLastTouchDownX = ev.getX();
                                mLastTouchDownY = ev.getY();
                                //这里给mFirstTouchTarget赋值
                                newTouchTarget = addTouchTarget(child, idBitsToAssign);
                                alreadyDispatchedToNewTouchTarget = true;
                                break;
                            }
                        }
                    }
                    //如果newTouchTarget为null，就代表，这个事件没有找到子View去处理它，
                    //那么，如果之前已经有了触摸对象（比如，我点了一张图，另一个手指在外面图的外面点下去）
                    //那么就把这个之前那个触摸目标定为第一个触摸对象，并且把这个触摸（pointer）分配给最近添加的触摸目标
                    if (newTouchTarget == null && mFirstTouchTarget != null) {
                        // Did not find a child to receive the event.
                        // Assign the pointer to the least recently added target.
                        newTouchTarget = mFirstTouchTarget;
                        while (newTouchTarget.next != null) {
                            newTouchTarget = newTouchTarget.next;
                        }
                        newTouchTarget.pointerIdBits |= idBitsToAssign;
                    }
                }
            }
 
            // Dispatch to touch targets.
            //如果没有触摸目标
            if (mFirstTouchTarget == null) {
                // No touch targets so treat this as an ordinary view.
                //那么就表示我们要自己在这个ViewGroup处理这个触摸事件了
                handled = dispatchTransformedTouchEvent(ev, canceled, null,
                        TouchTarget.ALL_POINTER_IDS);
            } else {
                // Dispatch to touch targets, excluding the new touch target if we already
                // dispatched to it.  Cancel touch targets if necessary.
                TouchTarget predecessor = null;
                TouchTarget target = mFirstTouchTarget;
                //遍历TouchTargt树.分发事件，如果我们已经分发给了新的TouchTarget那么我们就不再分发给newTouchTarget
                while (target != null) {
                    final TouchTarget next = target.next;
                    if (alreadyDispatchedToNewTouchTarget && target == newTouchTarget) {
                        handled = true;
                    } else {
                        //是否让child取消处理事件，如果为true，就会分发给child一个ACTION_CANCEL事件
                        final boolean cancelChild = resetCancelNextUpFlag(target.child)
                                || intercepted;
                        //派发事件
                        if (dispatchTransformedTouchEvent(ev, cancelChild,
                                target.child, target.pointerIdBits)) {
                            handled = true;
                        }
                        //cancelChild也就是说，派发给了当前child一个ACTION_CANCEL事件，
                        //那么就移除这个child
                        if (cancelChild) {
                            //没有父节点，也就是当前是第一个TouchTarget
                            //那么就把头去掉
                            if (predecessor == null) {
                                mFirstTouchTarget = next;
                            } else {
                                //把下一个赋予父节点的上一个，这样当前节点就被丢弃了
                                predecessor.next = next;
                            }
                            //回收内存
                            target.recycle();
                            //把下一个赋予现在
                            target = next;
                            //下面的两行不执行了，因为我们已经做了链表的操作了。
                            //主要是我们不能执行predecessor=target，因为删除本节点的话，父节点还是父节点
                            continue;
                        }
                    }
                    //如果没有删除本节点，那么下一轮父节点就是当前节点，下一个节点也是下一轮的当前节点
                    predecessor = target;
                    target = next;
                }
            }
 
            // Update list of touch targets for pointer up or cancel, if needed.
            //遇到了取消事件、或者是单点触摸下情况下手指离开，我们就要更新触摸的状态
            if (canceled
                    || actionMasked == MotionEvent.ACTION_UP
                    || actionMasked == MotionEvent.ACTION_HOVER_MOVE) {
                resetTouchState();
            } else if (split && actionMasked == MotionEvent.ACTION_POINTER_UP) {
                //如果是多点触摸下的手指抬起事件，就要根据idBit从TouchTarget中移除掉对应的Pointer(触摸点)
                final int actionIndex = ev.getActionIndex();
                final int idBitsToRemove = 1 << ev.getPointerId(actionIndex);
                removePointersFromTouchTargets(idBitsToRemove);
            }
        }
 
        if (!handled && mInputEventConsistencyVerifier != null) {
            mInputEventConsistencyVerifier.onUnhandledEvent(ev, 1);
        }
        return handled;
    }
```

主要做了这三件事：

1.  检查是否拦截事件
2.  检查子view并传递事件
3.  自己处理事件

整体流程：

1.  `DOWN`事件下，通过轮询当前`ViewGroup`的下层`View`
2.  同步进行判断，是否在这个`View`的矩形范围内
3.  如果在的话，那么选中这个`View`进行`dispatch`转发，他是`ViewGroup`会继续转发下层，如果是`View`直接消费处理！
4.  `MOVE`类型，因为其特性的原因，量大，用轮询不合适
5.  所以，在`DOWN`下来的时候直接记录当前这个`View`，然后默认后续直接转发这个`View`
6.  如果手指画出当前`View`，那么事件信息传递过来的`ActionID`会改变，改变ID后，进行将`target`变更！`UP\ CANCEL`会重置

##### 3.5.1.3 View

源码分析

```
/**
     * Pass the touch screen motion event down to the target view, or this
     * view if it is the target.
     * 将屏幕的按压事件传递给目标view，或者当前view即目标view
     * @param event The motion event to be dispatched.
     * @return True if the event was handled by the view, false otherwise.
     */
    public boolean dispatchTouchEvent(MotionEvent event) {
        //最前面这一段就是判断当前事件是否能获得焦点，
        // 如果不能获得焦点或者不存在一个View，那我们就直接返回False跳出循环
        // If the event should be handled by accessibility focus first.
        if (event.isTargetAccessibilityFocus()) {
            // We don't have focus or no virtual descendant has it, do not handle the event.
            if (!isAccessibilityFocusedViewOrHost()) {
                return false;
            }
            // We have focus and got the event, then use normal event dispatch.
            event.setTargetAccessibilityFocus(false);
        }
        //设置返回的默认值
        boolean result = false;
        //这段是系统调试方面，可以直接忽略
        if (mInputEventConsistencyVerifier != null) {
            mInputEventConsistencyVerifier.onTouchEvent(event, 0);
        }

        //Android用一个32位的整型值表示一次TouchEvent事件,低8位表示touch事件的具体动作，比如按下，抬起，滑动，还有多点触控时的按下，抬起，这个和单点是区分开的，下面看具体的方法:
        //1 getAction:触摸动作的原始32位信息，包括事件的动作，触控点信息
        //2 getActionMasked:触摸的动作,按下，抬起，滑动，多点按下，多点抬起
        //3 getActionIndex:触控点信息
        final int actionMasked = event.getActionMasked();
        if (actionMasked == MotionEvent.ACTION_DOWN) {
            //当我们手指按到View上时,其他的依赖滑动都要先停下
            // Defensive cleanup for new gesture
            stopNestedScroll();
        }
        //过滤掉一些不合法的事件,比如当前的View的窗口被遮挡了。
        if (onFilterTouchEventForSecurity(event)) {
            //ListenerInfo 是view的一个内部类 里面有各种各样的listener,
            // 例如OnClickListener，OnLongClickListener，OnTouchListener等等
            ListenerInfo li = mListenerInfo;

            //首先判断如果监听li对象!=null 且我们通过setOnTouchListener设置了监听，
            // 即是否有实现OnTouchListener，
            // 如果有实现就判断当前的view状态是不是ENABLED,
            // 如果实现的OnTouchListener的onTouch中返回true，并处理事件，则
            if (li != null && li.mOnTouchListener != null
                    && (mViewFlags & ENABLED_MASK) == ENABLED
                    && li.mOnTouchListener.onTouch(this, event)) {
                //如果满足这些条件那么返回true，这个事件就在此处理意味着这个View需要事件分发
                result = true;
            }
            //如果上一段判断的条件没有满足（没有在代码里面setOnTouchListener的话），
            // 就判断View自身的onTouchEvent方法有没有处理，没有处理最后返回false，处理了返回true；
            //也就是前面的判断优先级更高

            if (!result && onTouchEvent(event)) {
                result = true;
            }
        }
        系统调试分析相关，没有影响
        if (!result && mInputEventConsistencyVerifier != null) {
            mInputEventConsistencyVerifier.onUnhandledEvent(event, 0);
        }

        // Clean up after nested scrolls if this is the end of a gesture;
        // also cancel it if we tried an ACTION_DOWN but we didn't want the rest
        // of the gesture.
        //如果这是手势的结尾，则在嵌套滚动后清理
        if (actionMasked == MotionEvent.ACTION_UP ||
                actionMasked == MotionEvent.ACTION_CANCEL ||
                (actionMasked == MotionEvent.ACTION_DOWN && !result)) {
            stopNestedScroll();
        }

        return result;
    }
```

先调用`listener`的`onTouch`，而`onTouchEvent`的调用时根据`onTouch`结果走，  
`onTouch`的使用优先级高于`onTouchEvent`，`onTouch`通过返回值可以控制`onTouchEvent`的调用。

#### 3.5.2 事件消费过程

#### 3.5.3 U型模型

简单理解：事件从上到下传递，从下到上消费（无消费）

#### 3.5.4 L型模型

简单理解：事件从上到下传递，后被拦截（有消费）

## 4 总结

最后总结一下，**事件分发就是事件从linux层通过驱动采集数据，底层使用epoll和inotify传递出来，FrameWork层通过InputReaderThread读取，通过InputDispatcherThread分发到WMS，WMS将事件传递到Activity，上层的Activity、ViewGroup、View之间事件的分发和消费**。

具体流程：

1.  事件信号是物理文件存储数据，位置：`dev/input`
2.  `linux`有提供相关的文件监控api，其中使用了`inotify(能监控文件变化产生FD)` 和`epoll(可以监控FD，以此配合完成文件的监控与监听)`
3.  `Android`写了两个线程来处理`dev/input`下面的信号(`InputReaderThread`和`InputDispathcerThread`)
4.  专门写了一个`EventHub`对象，里面用`inotify+epoll`对`dev/input`下进行监控！
5.  将该对象放到`InputReaderThread`当中去执行，轮训`getEvent()`,这个里面有`epoll_wait`，相当于`wait-notify`机制，唤醒的触发点是`/dev/input`下的文件被改变，这个文件由驱动去推送数据
6.  `InputReaderThread`当中将`/dev/input`下的数据提取，封装，然后交给`InputDispathcerThread`
7.  `InputDispathcerThread`给最终选择到对应的`ViewRootImpl（Window）`
8.  中间的通信机制通过`socketpair`进行，两边一人一组`socketpair`
9.  然后在`ViewRootImpl`中对于`Channel`的连接的文件进行监控，一次导致能够最终上层接受到`touch`信号！