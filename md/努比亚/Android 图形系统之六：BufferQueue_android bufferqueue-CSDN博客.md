---
created: 2025-05-14T16:37:59 (UTC +08:00)
tags: [android bufferqueue]
source: https://blog.csdn.net/wudexiaoade2008/article/details/144167780
author: 成就一亿技术人!
---

# Android 图形系统之六：BufferQueue_android bufferqueue-CSDN博客

> ## Excerpt
> 文章浏览阅读1.4k次，点赞29次，收藏16次。BufferQueue是Android图形系统的核心组件之一，用于实现生产者-消费者模型的图像数据传递。它负责协调图像缓冲区的分配、传递、显示，广泛用于窗口系统、Surface、OpenGL ES渲染管道等场景。_android bufferqueue

---
![](https://csdnimg.cn/release/blogv2/dist/pc/img/original.png)

[Winston -\_-](https://blog.csdn.net/wudexiaoade2008 "Winston -_-") ![](https://csdnimg.cn/release/blogv2/dist/pc/img/newCurrentTime2.png) 于 2024-12-01 12:29:12 发布

版权声明：本文为博主原创文章，遵循 [CC 4.0 BY-SA](http://creativecommons.org/licenses/by-sa/4.0/) 版权协议，转载请附上原文出处链接和本声明。

`BufferQueue`是[Android](https://so.csdn.net/so/search?q=Android&spm=1001.2101.3001.7020)图形系统的核心组件之一，用于实现生产者-消费者模型的图像数据传递。它负责协调**图像缓冲区**的分配、传递、显示，广泛用于窗口系统、Surface、[OpenGL](https://so.csdn.net/so/search?q=OpenGL&spm=1001.2101.3001.7020) ES渲染管道等场景。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/62e1a78adf664e7485eb97e63f8b9732.png#pic_center)

#### **BufferQueue的核心概念**

1.  **生产者与消费者模型**
    -   **生产者（Producer）**：通过IGraphicBufferProducer接口写入图像数据，常见的生产者包括应用层的Surface、OpenGL等。
    -   **消费者（Consumer）**：通过IGraphicBufferConsumer接口读取图像数据，常见的消费者包括SurfaceFlinger、视频解码器等。
2.  **双端缓冲队列**
    -   使用双端队列(std::deque)管理缓冲区。
    -   生产者写入空闲缓冲区（dequeueBuffer），消费者从队列中获取已填充缓冲区（acquireBuffer）。

#### **BufferQueue核心组成**

BufferQueue主要由以下部分组成：

1.  **BufferQueueCore**
    -   实现核心逻辑，包括缓冲区管理和同步机制。
    -   提供线程安全的操作。
2.  **BufferQueueProducer**
    -   实现IGraphicBufferProducer接口。
    -   提供给生产者使用，用于管理缓冲区的生产操作（如申请、填充）。
3.  **BufferQueueConsumer**
    -   实现IGraphicBufferConsumer接口。
    -   提供给消费者使用，用于获取和消费缓冲区。
4.  **同步机制**
    -   使用信号量（如 Fence）确保生产者和消费者的操作同步。
    -   Fence 保证当生产者提交缓冲区时，消费者可以正确等待图像数据写入完成。

#### **BufferQueue的源码解析**

BufferQueue的主要代码位于`frameworks/native/libs/gui`中，其核心文件如下：

-   **BufferQueue.h/BufferQueue.cpp**：定义并实现生产者和消费者的逻辑。
-   **IGraphicBufferProducer.h/IGraphicBufferConsumer.h**：定义生产者和消费者的IPC接口。
-   **BufferItem.h**：表示队列中每个缓冲区的元数据结构。
-   **BufferQueueCore** 管理了缓冲区的核心数据结构。源码路径：`frameworks/native/libs/gui/BufferQueueCore.cpp`

```
BufferQueueCore::BufferQueueCore(const sp& allocator)
    : mAllocator(allocator),
      mConnectedApi(NO_CONNECTED_API),
      mQueue(mAllocator->createBufferQueue())
{
    ALOGV("BufferQueueCore constructed");
}
```

#### **BufferQueue的工作原理**

以下是BufferQueue的工作流程，结合源码简述其运作机制：

1.  **生产者操作**：
    -   **dequeueBuffer()** 生产者从BufferQueue中申请一个空闲缓冲区。核心代码片段：

```
status_t BufferQueueProducer::dequeueBuffer(...) {
    Mutex::Autolock lock(mCore->mMutex);
    int found = INVALID_BUFFER_SLOT;
    // 从空闲队列中查找一个可用缓冲区
    for (int i = 0; i < NUM_BUFFER_SLOTS; ++i) {
        if (mSlots[i].mBufferState == BufferSlot::FREE) {
            found = i;
            break;
        }
    }
    ...
    return found;
}
```

-   **queueBuffer()** 将填充完成的缓冲区提交到BufferQueue。核心代码片段：

```
status_t BufferQueueProducer::queueBuffer(...) {
    Mutex::Autolock lock(mCore->mMutex);
    // 更新缓冲区状态为已填充
    mSlots[slot].mBufferState = BufferSlot::QUEUED;
    mCore->mQueue.push_back(slot);
    mCore->mDequeueCondition.signal();
    return NO_ERROR;
}
```

2.  **消费者操作**：
    -   **acquireBuffer()** 从队列中获取已填充缓冲区。核心代码片段：

```
status_t BufferQueueConsumer::acquireBuffer(BufferItem* buffer) {
    Mutex::Autolock lock(mCore->mMutex);
    if (mCore->mQueue.empty()) {
        return NO_BUFFER_AVAILABLE;
    }
    // 从队列中取出缓冲区
    *buffer = mCore->mQueue.front();
    mCore->mQueue.pop_front();
    return NO_ERROR;
}
```

-   **releaseBuffer()** 消费完成后将缓冲区返回给BufferQueue。核心代码片段：

```
status_t BufferQueueConsumer::releaseBuffer(...) {
    Mutex::Autolock lock(mCore->mMutex);
    // 更新缓冲区状态为可用
    mSlots[slot].mBufferState = BufferSlot::FREE;
    mCore->mFreeList.push_back(slot);
    return NO_ERROR;
}
```

#### **BufferQueue的应用场景**

1.  **Surface与SurfaceFlinger**
    -   应用层通过Surface将图像提交到BufferQueue。
    -   SurfaceFlinger作为消费者，从BufferQueue获取图像并显示。
2.  **视频解码器**
    -   解码器通过BufferQueue将解码后的帧传递给渲染器。
3.  **GPU渲染**
    -   OpenGL ES渲染管道通过BufferQueue与显示设备交互。

#### **BufferQueue的优点与问题**

##### 优点

-   **高效数据传递**：无拷贝数据传递，减少了内存和CPU开销。
-   **线程安全**：支持多线程操作。

##### 问题

-   **复杂的锁机制**：过多的锁操作可能带来性能瓶颈。
-   **死锁风险**：在特殊情况下，生产者和消费者可能相互等待。

#### 调试工具

-   **dumpsys SurfaceFlinger**: 查看当前系统的缓冲区状态。
-   **Perfetto/Systrace**: 分析生产者与消费者之间的交互性能。

#### **总结**

BufferQueue是Android图形系统的核心模块，通过IPC接口实现生产者与消费者的高效通信。在阅读源码时，关注`BufferQueueCore`的锁机制、缓冲区状态流转逻辑以及生产者-消费者的接口调用关系，可以深入理解其工作原理和性能优化点。
