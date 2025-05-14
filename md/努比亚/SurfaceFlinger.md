
# SurfaceFlinger合成显示

`SurfaceFlinger`合成显示部分完全属于`Android`系统`GUI`中图形显示的内容，逻辑结构也比较复杂，但不属于本文介绍内容的重点。所以本小节中只是总体上介绍一下其工作原理与思想，不再详细分析源码，感兴趣的读者可以关注笔者后续的文章再来详细分析讲解。简单的说`SurfaceFlinger`作为系统中独立运行的一个`Native`进程，**借用`Android`官网的描述，其职责就是负责接受来自多个来源的数据缓冲区，对它们进行合成，然后发送到显示设备。**如下图所示：  

![](https:////upload-images.jianshu.io/upload_images/26874665-cb33efbd47f23d22.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/537/format/webp)

SurfaceFlinger工作原理.jpg

  
从上图可以看出，其实`SurfaceFlinger`在`Android`系统的整个图形显示系统中是起到一个**承上启下的作用**：

- **对上**：通过Surface与不同的应用进程建立联系，接收它们写入Surface中的绘制缓冲数据，对它们进行统一合成。
- **对下**：通过屏幕的后缓存区与屏幕建立联系，发送合成好的数据到屏幕显示设备。

图形的传递是通过`Buffer`作为载体，`Surface`是对`Buffer`的进一步封装，也就是说`Surface`内部具有多个`Buffer`供上层使用，如何管理这些`Buffer`呢？答案就是`BufferQueue` ，下面我们来看看`BufferQueue`的工作原理：

## 9.1 BufferQueue机制

借用一张经典的图来描述`BufferQueue`的工作原理：  

![](https:////upload-images.jianshu.io/upload_images/26874665-05c18df7fb448c79.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/481/format/webp)

BufferQueue状态转换图.jpg

  
`BufferQueue`是一个**典型的生产者-消费者模型中的数据结构**。在`Android`应用的渲染流程中，应用扮演的就是“生产者”的角色，而`SurfaceFlinger`扮演的则是“消费者”的角色，**其配合工作的流程如下**：

1. 应用进程中在开始界面的绘制渲染之前，需要通过`Binder`调用`dequeueBuffer`接口从`SurfaceFlinger`进程中管理的`BufferQueue` 中申请一张处于`free`状态的可用`Buffer`，如果此时没有可用`Buffer`则阻塞等待；
2. 应用进程中拿到这张可用的`Buffer`之后，选择使用`CPU`软件绘制渲染或`GPU`硬件加速绘制渲染，渲染完成后再通过`Binder`调用`queueBuffer`接口将缓存数据返回给应用进程对应的`BufferQueue`（如果是 `GPU` 渲染的话，这里还有个 `GPU`处理的过程，所以这个 `Buffer` 不会马上可用，需要等 `GPU` 渲染完成的`Fence`信号），并申请`sf`类型的`Vsync`以便唤醒“消费者”`SurfaceFlinger`进行消费；
3. `SurfaceFlinger` 在收到 `Vsync` 信号之后，开始准备合成，使用 `acquireBuffer`获取应用对应的 `BufferQueue` 中的 `Buffer` 并进行合成操作；
4. 合成结束后，`SurfaceFlinger` 将通过调用 `releaseBuffer`将 `Buffer` 置为可用的`free`状态，返回到应用对应的 `BufferQueue`中。

## 9.2 Vsync同步机制

`Vysnc`垂直同步是`Android`在“黄油计划”中引入的一个重要机制，本质上是为了协调`BufferQueue`的应用生产者生成UI数据动作和`SurfaceFlinger`消费者的合成消费动作，避免出现画面撕裂的`Tearing`现象。`Vysnc`信号分为两种类型：

1. `app`类型的`Vsync`：**`app`类型的`Vysnc`信号由上层应用中的`Choreographer`根据绘制需求进行注册和接收，用于控制应用UI绘制上帧的生产节奏**。根据第7小结中的分析：应用在UI线程中调用invalidate刷新界面绘制时，需要先透过`Choreographer`向系统申请注册app类型的`Vsync`信号，待`Vsync`信号到来后，才能往主线程的消息队列放入待绘制任务进行真正UI的绘制动作；
2. `sf`类型的`Vsync`:**`sf`类型的`Vsync`是用于控制`SurfaceFlinger`的合成消费节奏**。应用完成界面的绘制渲染后，通过`Binder`调用`queueBuffer`接口将缓存数据返还给应用对应的`BufferQueue`时，会申请`sf`类型的`Vsync`，待`SurfaceFlinger` 在其UI线程中收到 `Vsync` 信号之后，便开始进行界面的合成操作。

`Vsync`信号的生成是参考屏幕硬件的刷新周期的，其架构如下图所示：  

![](https:////upload-images.jianshu.io/upload_images/26874665-7a7e75039d05d786.png?imageMogr2/auto-orient/strip|imageView2/2/w/582/format/webp)

vsync.png

  
本小节所描述的流程，从systrace上看`SurfaceFlinger`处理应用上帧工作的流程如下图所示：  

![](https:////upload-images.jianshu.io/upload_images/26874665-9d5cefb49aa75c16.png?imageMogr2/auto-orient/strip|imageView2/2/w/856/format/webp)

requestVsync.png

  

![](https:////upload-images.jianshu.io/upload_images/26874665-d7e5bebe790e020e.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

SurfaceFlinger处理.png

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/37370c1d17fc  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。