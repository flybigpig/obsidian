---
created: 2025-04-11T11:14:31 (UTC +08:00)
tags: [还在用handler]
source: https://blog.csdn.net/allisonchen/article/details/120618884
author: 成就一亿技术人!
---

# 都 2021 年了，还有人在研究 Handler？建议收藏~_还在用handler-CSDN博客

> ## Excerpt
> 文章浏览阅读948次，点赞3次，收藏7次。我们经常提及 Android 中极为重要的线程间通信方式即 Handler 机制，貌似 Handler 类发挥了很大的作用。事实上当你了解它的原理之后，会发现 Handler 只是该机制的调用入口和回调而已，最重要的东西是 Looper 和 MessagQueue，以及不断流转的 Message。本次针对该机制常被问及的 18 个问题进行整理和回答，供大家解惑和回顾~1. 简述下 Handler 的总体原理？Looper#prepare() 初始化线程独有的 Looper 以及 Messag.._还在用handler

---
## [Handler](https://so.csdn.net/so/search?q=Handler&spm=1001.2101.3001.7020) 机制的终极 20 问，都了解了吗？

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/78b2c776643756e7cbd820f351a68912.png#pic_center)

我们经常使用和提及 Android 中特有的线程间通信方式即 `Handler` 机制，缘于该机制特别好用、极为重要！

初尝 Handler 机制的时候，原以为 Handler 类发挥了很大的作用。当你深入了解它的原理之后，会发现 Handler 只是该机制的**调用入口和回调**而已，最重要的东西是 `Looper` 和 `MessagQueue`，以及不断流转的 `Message`。

本次针对 Handler 机制常被提及和容易困扰的 **20** 个问题进行整理和回答，供大家解惑和回顾~

#### 文章目录

-   [Handler 机制的终极 20 问，都了解了吗？](https://blog.csdn.net/allisonchen/article/details/120618884#Handler__20__1)
-   -   -   -   -   [1\. 简述下 Handler 机制的总体原理？](https://blog.csdn.net/allisonchen/article/details/120618884#1__Handler__12)
                -   [2\. Looper 存在哪？如何可以保证线程独有？](https://blog.csdn.net/allisonchen/article/details/120618884#2_Looper__33)
                -   [3\. 如何理解 ThreadLocal 的作用？](https://blog.csdn.net/allisonchen/article/details/120618884#3__ThreadLocal__47)
                -   [4\. 主线程 Main Looper 和一般 Looper 的异同？](https://blog.csdn.net/allisonchen/article/details/120618884#4__Main_Looper__Looper__53)
                -   [5\. Handler 或者说 Looper 如何切换线程？](https://blog.csdn.net/allisonchen/article/details/120618884#5_Handler__Looper__80)
                -   [6\. Looper 的 loop() 死循环为什么不卡死？](https://blog.csdn.net/allisonchen/article/details/120618884#6_Looper__loop__94)
                -   [7\. Looper 的等待是如何能够准确唤醒的？](https://blog.csdn.net/allisonchen/article/details/120618884#7_Looper__110)
                -   [8\. Message 如何获取？为什么这么设计？](https://blog.csdn.net/allisonchen/article/details/120618884#8_Message__124)
                -   [9\. MessageQueue 如何管理 Message？](https://blog.csdn.net/allisonchen/article/details/120618884#9_MessageQueue__Message_132)
                -   [10\. 理解 Message 和 MessageQueue 的异同？](https://blog.csdn.net/allisonchen/article/details/120618884#10__Message__MessageQueue__138)
                -   [11\. Message 的执行时刻如何管理？](https://blog.csdn.net/allisonchen/article/details/120618884#11_Message__151)
                -   [12\. Handler、Mesage 和 Runnable 的关系如何理解？](https://blog.csdn.net/allisonchen/article/details/120618884#12_HandlerMesage__Runnable__173)
                -   [13\. IdleHandler 空闲 Message 了解过吗？有什么用？](https://blog.csdn.net/allisonchen/article/details/120618884#13_IdleHandler__Message__181)
                -   [14\. 异步 Message 或同步屏障了解过吗？怎么用？什么原理？](https://blog.csdn.net/allisonchen/article/details/120618884#14__Message__213)
                -   [15\. Looper 和 MessageQueue、Message 及 Handler 的关系？](https://blog.csdn.net/allisonchen/article/details/120618884#15_Looper__MessageQueueMessage__Handler__226)
                -   [16\. Native 侧的 NativeMessageQueue 和 Looper 的作用是？](https://blog.csdn.net/allisonchen/article/details/120618884#16_Native__NativeMessageQueue__Looper__265)
                -   [17\. Native 侧如何使用 Looper？](https://blog.csdn.net/allisonchen/article/details/120618884#17_Native__Looper_271)
                -   [18\. Handler 为什么可能导致内存泄露？如何避免？](https://blog.csdn.net/allisonchen/article/details/120618884#18_Handler__279)
                -   [19\. Handler 在系统当中的应用](https://blog.csdn.net/allisonchen/article/details/120618884#19_Handler__296)
                -   [20\. Android 为什么不允许并发访问 UI？](https://blog.csdn.net/allisonchen/article/details/120618884#20_Android__UI_307)
                -   [结语](https://blog.csdn.net/allisonchen/article/details/120618884#_321)
                -   [推荐阅读](https://blog.csdn.net/allisonchen/article/details/120618884#_327)

###### 1\. 简述下 Handler 机制的总体原理？

1.  Looper 准备和开启轮循：
    
    -   Looper#`prepare()` 初始化线程独有的 `Looper` 以及 `MessageQueue`
    -   Looper#`loop()` 开启**死循环**读取 MessageQueue 中下一个满足执行时间的 Message
        -   尚无 Message 的话，调用 Native 侧的 `pollOnce()` 进入**无限等待**
        -   存在 Message，但执行时间 `when` 尚未满足的话，调用 pollOnce() 时传入剩余时长参数进入**有限等待**
2.  Message 发送、入队和出队：
    
    -   Native 侧如果处于无限等待的话：任意线程向 `Handler` 发送 `Message` 或 `Runnable` 后，Message 将按照 when 条件的先后，被插入 Handler 持有的 Looper 实例所对应的 MessageQueue 中**适当的位置**。 MessageQueue 发现有合适的 Message 插入后将调用 Native 侧的 `wake()` 唤醒无限等待的线程。这将促使 MessageQueue 的读取继续**进入下一次循环**，此刻 Queue 中已有满足条件的 Message 则出队返回给 Looper
    -   Native 侧如果处于有限等待的话：在等待指定时长后 epoll\_wait 将返回。线程继续读取 MessageQueue，此刻因为时长条件将满足将其出队
3.  Looper 处理 Message 的实现：
    
    Looper 得到 Message 后回调 Message 的 `callback` 属性即 Runnable，或依据 `target` 属性即 Handler，去执行 Handler 的回调。
    
    -   存在 `mCallback` 属性的话回调 `Handler$Callback`
    -   反之，回调 `handleMessage()`

###### 2\. Looper 存在哪？如何可以保证线程独有？

-   Looper 实例被管理在静态属性 `sThreadLocal` 中
-   `ThreadLocal` 内部通过 `ThreadLocalMap` 持有 Looper，`key` 为 ThreadLocal 实例本身，`value` 即为 Looper 实例
-   每个 Thread 都有一个自己的 ThreadLocalMap，这样可以保证每个线程对应一个独立的 Looper 实例，进而保证 `myLooper()` 可以获得线程独有的 Looper

**彩蛋：一个 App 拥有几个 Looper 实例？几个 ThreadLocal 实例？几个 MessageQueue 实例？几个 Message 实例？几个 Handler 实例**

-   一个线程只有一个 Looper 实例
-   一个 Looper 实例只对应着一个 MessageQueue 实例
-   一个 MessageQueue 实例可对应多个 Message 实例，其从 Message 静态池里获取，存在 50 的上限
-   一个线程可以拥有多个 Handler 实例，其Handler 只是发送和执行任务逻辑的入口和出口
-   ThreadLocal 实例是静态的，整个进程**共用一个实例**。每个 Looper 存放的 ThreadLocalMap 均弱引用它作为 key

###### 3\. 如何理解 ThreadLocal 的作用？

-   首先要明确并非不是用来切换线程的，**只是为了让每个线程方便程获取自己的 Looper 实例**，见 Looper#`myLooper()`
    -   后续可供 Handler 初始化时**指定其所属的 Looper 线程**
    -   也可用来线程判断自己是否**是主线程**

###### 4\. 主线程 Main Looper 和一般 Looper 的异同？

-   区别：
    
    1.  Main Looper **不可 `quit`**
    
    主线程需要不断读取系统消息和用书输入，是进程的入口，只可被系统直接终止。进而其 Looper 在创建的时候设置了**不可 `quit` 的标志**，而**其他线程的 Looper 则可以也必须手动 quit**
    
    2.  Main Looper 实例还被**静态缓存**
    
    为了便于每个线程获得主线程 Looper 实例，见 Looper#getMainLooper()，Main Looper 实例还作为 `sMainLooper` 属性缓存到了 Looper 类中。
    
-   相同点：
    
    1.  都是通过 Looper#prepare() 间接调用 Looper 构造函数创建的实例
    2.  都被静态实例 ThreadLocal 管理，方便每个线程获取自己的 Looper 实例

**彩蛋：主线程为什么不用初始化 Looper？**

App 的入口并非 MainActivity，也不是 Application，而是 ActivityThread。

其为了 Application、ContentProvider、Activity 等组件的运行，必须事先启动不停接受输入的 Looper 机制，所以在 main() 执行的最后将调用 prepareMainLooper() 创建 Looper 并调用 loop() 轮循。

不需要我们调用，也不可能有我们调用。

可以说如果主线程没有创建 Looper 的话，我们的组件也不可能运行得到！

###### 5\. Handler 或者说 Looper 如何切换线程？

1.  Handler 创建的时候指定了其所属线程的 Looper，进而持有了 Looper 独有的 MessageQueue
    
2.  Looper#loop() 会持续读取 MessageQueue 中合适的 Message，没有 Message 的时候进入等待
    
3.  当向 Handler 发送 Message 或 Runnable 后，会向持有的 MessageQueue 中插入 Message
    
4.  Message 抵达并满足条件后会唤醒 MessageQueue 所属的线程，并将 Message 返回给 Looper
    
5.  Looper 接着回调 Message 所指向的 Handler Callback 或 Runnable，达到线程切换的目的
    

**简言之，向 Handler 发送 Message 其实是向 Handler 所属线程的独有 MessageQueue 插入 Message。而线程独有的 Looper 又会持续读取该 MessageQueue。所以向其他线程的 Handler 发送完 Message，该线程的 Looper 将自动响应。**

###### 6\. Looper 的 loop() 死循环为什么不卡死？

为了让主线程持续处理用户的输入，loop() 是**死循环**，持续调用 MessageQueue#`next()` 读取合适的 Message。

但当没有 Message 的时候，会调用 `pollOnce()` 并通过 Linux 的 `epoll` 机制进入等待并释放资源。同时 `eventFd` 会监听 Message 抵达的写入事件并进行唤醒。

这样可以**空闲时释放资源、不卡死线程，同时能持续接收输入的目的**。

**彩蛋1：loop() 后的处理为什么不可执行**

因为 loop() 是死循环，直到 quit 前后面的处理无法得到执行，所以避免将处理放在 loop() 的后面。

\*\*彩蛋2：Looper 等待的时候线程到底是什么状态？ \*\*

调用 Linux 的 epoll 机制进入**等待**，事实上 Java 侧打印该线程的状态，你会发现线程处于 `Runnable` 状态，只不过 CPU 资源被暂时释放。

###### 7\. Looper 的等待是如何能够准确唤醒的？

读取合适 Message 的 MessageQueue#next() 会因为 Message 尚无或执行条件尚未满足进行两种等的等待：

-   **无限等待**
    
    尚无 Message（队列中没有 Message 或建立了同步屏障但尚无异步 Message）的时候，调用 Natvie 侧的 `pollOnce()` 会传入参数 **\-1**。
    
    Linux 执行 `epoll_wait()` 将进入无限等待，其等待合适的 Message 插入后调用 Native 侧的 `wake()` 向唤醒 fd 写入事件触发唤醒 MessageQueue 读取的下一次循环
    
-   **有限等待**
    
    有限等待的场合将下一个 Message **剩余时长作为参数**交给 epoll\_wait()，epoll 将等待一段时间之后**自动返回**，接着回到 MessageQueue 读取的下一次循环
    

###### 8\. Message 如何获取？为什么这么设计？

-   享元设计模式：通过 Message 的静态方法 `obatin()` 获取，因为该方法不是无脑地 new，而是**从单链表池子里获取实例**，并在 `recycle()` 后将其放回池子
    
-   好处在于复用 Message 实例，满足频繁使用 Message 的场景，更加高效
    
-   当然，缓存池存在上限 **50**，因为没必要无限制地缓存，这本身也是一种浪费
    
-   需要留意缓存池是静态的，也就是整个进程共用一个缓存池
    

###### 9\. MessageQueue 如何管理 Message？

-   MessageQueue 通过单链表管理 Message，不同于进程共用的 Message Pool，其是线程独有的
-   通过 Message 的执行时刻 when 对 Message 进行排队和出队
-   MessageQueue 除了管理 Message，还要管理空闲 Handler 和 同步屏障

###### 10\. 理解 Message 和 MessageQueue 的异同？

-   相同点：都是通过**单链表来管理** Message 实例；
    
    -   Message 通过 **obtain() 和 recycle()** 向单链表获取插入节点
        
    -   MessageQueue 通过 **enqueueMessage() 和 next()** 向单链表获取和插入节点
        
-   区别：
    
    -   Message 单链表是**静态的，供进程使用的缓存池**
-   MessageQueue 单链表**非静态，只供 Looper 线程使用**
    

###### 11\. Message 的执行时刻如何管理？

-   发送的 Message 都是按照执行时刻 `when` 属性的先后管理在 MessageQueue 里
    -   延时 Message 的 when 等于`调用的当前时刻`和 `delay` 之和
    -   非延时 Message 的 when 等于`当前时刻`（delay 为 `0`）
    -   插队 Message 的 when 固定为 `0`，便于插入队列的 `head`
-   之后 MessageQueue 会根据**读取的时刻和 when 进行比较**
    -   将 when 已抵达的出队，
    -   尚未抵达的计算出**当前时刻和目标 when 的插值**，交由 Native 等待对应的时长，时间到了自动唤醒继续进行 Message 的读取

事实上，无论上述哪种 Message 都不能保证在其对应的 when 时刻执行，往往都会延迟一些！因为必须等当前执行的 Message 处理完了才有机会读取队列的下一个 Message。

比如发送了非延时 Message，when 即为发送的时刻，可它们不会立即执行。都要等主线程现有的任务（Message）走完才能有机会出队，而当这些任务执行完 when 的时刻已经过了。假使队列的前面还有其他 Message 的话，延迟会更加明显！

**彩蛋：. onCreate() 里向 Handler 发送大量 Message 会导致主线程卡顿吗？**

不会，发送的大量 Message 并非立即执行，只是先放到队列当中而已。

onCreate() 以及之后同步调用的 onStart() 和 onResume() 处理，本质上也是 Message。等这个 Message 执行完之后，才会进行读取 Message 的下一次循环，这时候才能回调 onCreate 里发送的 Message。

需要说明的是，如果发送的是 FrontOfQueue 将 Message 插入队首也不会立即先执行，因为 onStart 和 onResume 是 onCreate 之后同步调用的，本质上是同一个 Message 的作业周期

###### 12\. Handler、Mesage 和 Runnable 的关系如何理解？

-   作为使用 Handler 机制的入口，**Handler 是发送 Message 或 Runnable 的起点**
-   发送的 **Runnable 本质上也是 Message**，只不过作为 `callback` 属性被持有
-   **Handler 作为 `target` 属性被持有在 Mesage 中**，在 Message 执行条件满足的时候供 Looper 回调

事实上，Handler 只是供 App 使用 Handler 机制的 API，实质来说，Message 是更为重要的载体。

###### 13\. IdleHandler 空闲 Message 了解过吗？有什么用？

-   适用于期望**空闲时候执行，但不影响主线程操作**的任务
    
-   系统应用：
    
    1.  Activity `destroy` 回调就放在了 `IdleHandler` 中
    2.  `ActivityThread` 中 `GCHandler` 使用了 IdleHandler，在空闲的时候执行 **GC** 操作
-   App 应用：
    
    1.  **发送一个返回 true 的 IdleHandler，在里面让某个 View 不停闪烁，这样当用户发呆时就可以诱导用户点击这个 View**
    2.  **将某部分初始化放在 IdleHandler 里不影响 Activity 的启动**
-   **彩蛋问题：**
    
    1.  `add/remove` IdleHandler 的方法，是否需要成对使用？
    
    不需要，回调返回 false 也可以移除
    
    2.  当 `mIdleHanders` 一直不为空时，为什么不会进入死循环？
    
    执行过 IdleHandler 之后会将计数重置为 **0**，确保下一次循环不重复执行
    
    3.  是否可以将一些不重要的启动服务，搬移到 IdleHandler 中去处理？
    
    **最好不要，回调时机不太可控**，需要搭配 `remove` 谨慎使用
    
    4.  IdleHandle 的 `queueIdle()` 运行在那个线程？
    
    取决于 IdleHandler add 到的 MessageQueue 所处的线程
    

###### 14\. 异步 Message 或同步屏障了解过吗？怎么用？什么原理？

-   异步 Message：设置了 `isAsync` 属性的 Message 实例
    
    -   可以用异步 Handler 发送
    -   也可以调用 Message#`setAsynchronous()` 直接设置为异步 Message
-   同步屏障：在 MessageQueue 的**某个位置放一个 target 属性为 null 的 Message**，确保此后的非异步 Message 无法执行，只能执行异步 Message
    
-   原理：当 MessageQueue 轮循 Message 时候**发现建立了同步屏障的时候，会去跳过其他 Message，读取下个 async 的 Message 并执行，屏障移除之前同步 Message 都会被阻塞**
    
-   应用：比如**屏幕刷新 `Choreographer` 就使用到了同步屏障**，确保屏幕刷新事件不会因为队列负荷影响屏幕及时刷新。
    
-   注意：**同步屏障的添加或移除 API 并未对外公开，App 需要使用的话需要依赖反射机制**
    

###### 15\. Looper 和 MessageQueue、Message 及 Handler 的关系？

-   Message 是承载任务的载体，在 Handler 机制中贯穿始终
-   Handler 则是对外公开的 API，负责发送 Message 和处理任务的回调，是 **Message 的生产者**
-   MessagQueue 负责管理待处理 Message 的入队和出队，是 **Message 的容器**
-   Looper 负责轮循 MessageQueue，保持线程持续运行任务，是 **Message 的消费者**

**彩蛋：如何保证 MessageQueue 并发访问安全？**

任何线程都可以通过 Handler 生产 Message 并放入 MessageQueue 中，可 Queue 所属的 Looper 在持续地读取并尝试消费 Message。如何保证两者不产生死锁？

Looper 在消费 Message 之前要先拿到 MessageQueue 的锁，\*\*只不过没有 Message 或 Message 尚未满足条件的进行等待前会事先释放锁，\*\*具体在于 nativePollOnce() 的调用在 synchronized 方法块的外侧。

Message 入队前也需先拿到 MessageQueue 的锁，而这时 Looper 线程正在等待且不持有锁，可以确保 Message 的成功入队。入队后执行唤醒后释放锁，Native 收到 event 写入后恢复 MessagQueue 的读取并可以拿到锁，成功出队。

这样一种在没有 Message 可以消费时执行等待同时不占着锁的机制，避免了生产和消费的死锁。

```
boolean enqueueMessage(Message msg, long when) {
        synchronized (this) {
            ...
        }
        return true;
    }
```

```
Message next() {
    ...
    for (;;) {
        nativePollOnce(ptr, nextPollTimeoutMillis);
        synchronized (this) {
            ...
        }
        ...
    }
}
```

###### 16\. Native 侧的 NativeMessageQueue 和 Looper 的作用是？

-   NativeMessageQueue 负责连接 Java 侧的 MessageQueue，进行后续的 `wait` 和 `wake`，后续将创建 wake 的FD，并通过 epoll 机制等待或唤醒。**但并不参与管理 Java 的 Message**
    
-   Native 侧也需要 Looper 机制，等待和唤醒的需求是同样的，所以将这部分实现都封装到了 JNI 的NativeMessageQueue 和 Native 的 Looper 中，**供 Java 和 Native 一起使用**
    

###### 17\. Native 侧如何使用 Looper？

-   Looper Native 部分承担了 Java 侧 Looper 的等待和唤醒，除此之外其还提供了 Message、`MessageHandler` 或 `WeakMessageHandler`、`LooperCallback` 或 `SimpleLooperCallback` 等 API
    
-   这些部分可供 Looper 被 Native 侧直接调用，比如 `InputFlinger` 广泛使用了 Looper
    
-   主要方法是调用 Looper 构造函数或 prepare 创建 Looper，然后通过 poll 开始轮询，接着 `sendMessage` 或 `addEventFd`，等待 Looper 的唤醒。**使用过程和 Java 的调用思路类似**
    

###### 18\. Handler 为什么可能导致内存泄露？如何避免？

-   持有 Activity 实例的内名内部类或内部类的**生命周期**应当和 Activity 保持一致，否则产生内存泄露的风险。
-   如果 Handler 使用不当，将造成不一致，表现为：匿名内部类或内部类写法的 Handler、Handler$Callback、Runnable，或者Activity 结束时仍有活跃的 Thread 线程或 Looper 子线程
-   具体在于：异步任务仍然活跃或通过发送的 Message 尚未处理完毕，将使得内部类实例的**生命周期被错误地延长**。造成本该回收的 Activity 实例**被别的 `Thread` 或 `Main Looper` 占据而无法及时回收**（活跃的 Thread 或 静态属性 sMainLooper 是 GC Root 对象）
-   建议的做法：
    -   无论是 Handler、Handler$Callback 还是 Runnable，尽量采用**静态内部类 + 弱引用**的写法，确保尽管发生不当引用的时候也可以因为弱引用能清楚持有关系
    -   另外在 Activity 销毁的时候及时地**终止 Thread、停止子线程的 Looper 或清空 Message**，确保彻底切断 Activity 经由 Message 抵达 GC Root 的引用源头（Message 清空后会其与 Handler 的引用关系，Thread 的终止将结束其 GC Root 的源头）

注意：静态的 sThreadLocal 实例不持有存放 Looper 实例的 ThreadLocalMap，而是由 Thread 持有。从这个角度上来讲，Looper 会被活跃的 GC Root Thread 持有，进而也可能导致内存泄露。

**彩蛋：网传的 Handler$Callback 方案能否解决内存泄露？**

不能。

Callback 采用内部类或匿名内部类写法的话，默认持有 Activity 的引用，而 Callback 被 Handler 持有。这最终将导致 Message -> Handler -> Callback -> Activity 的链条仍然存在。

###### 19\. Handler 在系统当中的应用

特别广泛，比如：

-   Activity 生命周期的管理
-   屏幕刷新
-   HandlerThread、IntentService
-   AsyncTask 等。

主要利用 Handler 的切换线程、主线程异步 Message 的重要特性。注意：Binder 线程非主线程，但很多操作比如生命周期的管理都要回到主线程，所以很多 Binder 调用过来后都要通过 Handler 切换回主线程执行后续任务，比如 ActviityThread$H 就是 extends Handler。

###### 20\. Android 为什么不允许并发访问 UI？

Android 中 UI 非线程安全，并发访问的话会造成数据和显示错乱。

但此限制的检查始于ViewRootImpl#checkThread()，其会在刷新等多个访问 UI 的时机被调用，去检查当前线程，非主线程的话抛出异常。

而 ViewRootImpl 的创建在 onResume() 之后，也就是说如果在 onResume() 执行前启动线程访问 UI 的话是不会报错的，这点需要留意！

**彩蛋：onCreate() 里子线程更新 UI 有问题吗？为什么？**

不会。

因为异常的检测处理在 ViewRootImpl 中，该实例的创建和检测在 onResume() 之后进行。

###### 结语

能力和精力有限，如果出现遗漏、错误或细节不明的地方，欢迎不吝赐教。

让我们共同维护这几十个问题，彻底吃透 Handler 机制！

###### 推荐阅读

-   如果对 Message 创建和复用的细节感兴趣，可以查阅[《Handler 的 Message 实例怎么创建，为什么不是直接 new？》](https://blog.csdn.net/allisonchen/article/details/120278268?spm=1001.2014.3001.5501)。
-   如果并不确定 Message 有多少种、已经背后执行的细节，或者对空闲 Handler、同步屏障等特殊用法不了解的话，建议阅读[《万字复盘 Handler 中各式 Message 的使用和原理》](https://blog.csdn.net/allisonchen/article/details/120477818?spm=1001.2014.3001.5501)。
-   如果对 Looper quit 的理由和背后原理好奇的话，可以参考[《Looper 需要手动 quit，那主线程 Looper 呢？》](https://blog.csdn.net/allisonchen/article/details/120369326?spm=1001.2014.3001.5501)。
-   如果对于 Handler 内存泄漏的问题存在疑惑，想要一次性搞清楚的话，可以试着看看这篇文章：[《重新理解为什么 Handler 可能导致内存泄露？》](https://blog.csdn.net/allisonchen/article/details/120573264?spm=1001.2014.3001.5501)。
