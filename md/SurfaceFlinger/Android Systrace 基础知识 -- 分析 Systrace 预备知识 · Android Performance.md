本文是 Systrace 系列文章的第二篇，主要是讲解一些分析 Systrace 的预备知识, 有了这些预备知识, 分析 Systrace 才会事半功倍, 更快也更有效率地找到问题点.

本文介绍了如何查看 Systrace 中的线程状态 , 如何对线程的唤醒信息进行分析, 如何解读信息区的数据, 以及介绍了常用的快捷键. 通过本篇文章的学习, 相信你可以掌握进程和线程相关的一些信息, 也知道如何查看复杂的 Systrace 中包含的关键信息

## [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%B3%BB%E5%88%97%E6%96%87%E7%AB%A0%E7%9B%AE%E5%BD%95 "系列文章目录")系列文章目录

1.  [Systrace 简介](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/)
2.  [Systrace 基础知识 - Systrace 预备知识](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/)
3.  [Systrace 基础知识 - Why 60 fps ？](https://www.androidperformance.com/2019/05/27/why-60-fps/)
4.  [Systrace 基础知识 - SystemServer 解读](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/)
5.  [Systrace 基础知识 - SurfaceFlinger 解读](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/)
6.  [Systrace 基础知识 - Input 解读](https://www.androidperformance.com/2019/11/04/Android-Systrace-Input/)
7.  [Systrace 基础知识 - Vsync 解读](https://www.androidperformance.com/2019/12/01/Android-Systrace-Vsync/)
8.  [Systrace 基础知识 - Vsync-App ：基于 Choreographer 的渲染机制详解](https://androidperformance.com/2019/10/22/Android-Choreographer/)
9.  [Systrace 基础知识 - MainThread 和 RenderThread 解读](https://www.androidperformance.com/2019/11/06/Android-Systrace-MainThread-And-RenderThread/)
10.  [Systrace 基础知识 - Binder 和锁竞争解读](https://www.androidperformance.com/2019/12/06/Android-Systrace-Binder/)
11.  [Systrace 基础知识 - Triple Buffer 解读](https://www.androidperformance.com/2019/12/15/Android-Systrace-Triple-Buffer)
12.  [Systrace 基础知识 - CPU Info 解读](https://www.androidperformance.com/2019/12/21/Android-Systrace-CPU)
13.  [Systrace 流畅性实战 1 ：了解卡顿原理](https://www.androidperformance.com/2021/04/24/android-systrace-smooth-in-action-1/)
14.  [Systrace 流畅性实战 2 ：案例分析: MIUI 桌面滑动卡顿分析](https://www.androidperformance.com/2021/04/24/android-systrace-smooth-in-action-2/)
15.  [Systrace 流畅性实战 3 ：卡顿分析过程中的一些疑问](https://www.androidperformance.com/2021/04/24/android-systrace-smooth-in-action-3/)
16.  [Systrace 响应速度实战 1 ：了解响应速度原理](https://www.androidperformance.com/2021/09/13/android-systrace-Responsiveness-in-action-1/)
17.  [Systrace 响应速度实战 2 ：响应速度实战分析-以启动速度为例](https://www.androidperformance.com/2021/09/13/android-systrace-Responsiveness-in-action-2/)
18.  [Systrace 响应速度实战 3 ：响应速度延伸知识](https://www.androidperformance.com/2021/09/13/android-systrace-Responsiveness-in-action-3/)
19.  [Systrace 线程 CPU 运行状态分析技巧 - Runnable 篇](https://www.androidperformance.com/2022/01/21/android-systrace-cpu-state-runnable/)
20.  [Systrace 线程 CPU 运行状态分析技巧 - Running 篇](https://www.androidperformance.com/2022/03/13/android-systrace-cpu-state-running/)
21.  [Systrace 线程 CPU 运行状态分析技巧 - Sleep 和 Uninterruptible Sleep 篇](https://www.androidperformance.com/2022/03/13/android-systrace-cpu-state-sleep/)

## [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E6%AD%A3%E6%96%87 "正文")正文

## [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%BA%BF%E7%A8%8B%E7%8A%B6%E6%80%81%E6%9F%A5%E7%9C%8B "线程状态查看")线程状态查看[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%BA%BF%E7%A8%8B%E7%8A%B6%E6%80%81%E6%9F%A5%E7%9C%8B)

Systrace 会用不同的颜色来标识不同的线程状态, 在每个方法上面都会有对应的线程状态来标识目前线程所处的状态，通过查看线程状态我们可以知道目前的瓶颈是什么, 是 cpu 执行慢还是因为 Binder 调用, 又或是进行 io 操作, 又或是拿不到 cpu 时间片

线程状态主要有下面几个

### [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%BB%BF%E8%89%B2-%E8%BF%90%E8%A1%8C%E4%B8%AD%EF%BC%88Running%EF%BC%89 "绿色 : 运行中（Running）")绿色 : 运行中（Running）[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%BB%BF%E8%89%B2-%E8%BF%90%E8%A1%8C%E4%B8%AD%EF%BC%88Running%EF%BC%89)

只有在该状态的线程才可能在 cpu 上运行。而同一时刻可能有多个线程处于可执行状态，这些线程的 task\_struct 结构被放入对应 cpu 的可执行队列中（一个线程最多只能出现在一个 cpu 的可执行队列中）。调度器的任务就是从各个 cpu 的可执行队列中分别选择一个线程在该cpu 上运行

作用：我们经常会查看 Running 状态的线程，查看其运行的时间，与竞品做对比，分析快或者慢的原因：

1.  是否频率不够？
2.  是否跑在了小核上？
3.  是否频繁在 Running 和 Runnable 之间切换？为什么？
4.  是否频繁在 Running 和 Sleep 之间切换？为什么？
5.  是否跑在了不该跑的核上面？比如不重要的线程占用了超大核

[![](https://www.androidperformance.com/images/15638915926547.jpg)](https://www.androidperformance.com/images/15638915926547.jpg)

### [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E8%93%9D%E8%89%B2-%E5%8F%AF%E8%BF%90%E8%A1%8C%EF%BC%88Runnable%EF%BC%89 "蓝色 : 可运行（Runnable）")蓝色 : 可运行（Runnable）[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E8%93%9D%E8%89%B2-%E5%8F%AF%E8%BF%90%E8%A1%8C%EF%BC%88Runnable%EF%BC%89)

线程可以运行但当前没有安排，在等待 cpu 调度

作用：Runnable 状态的线程状态持续时间越长，则表示 cpu 的调度越忙，没有及时处理到这个任务：

1.  是否后台有太多的任务在跑？
2.  没有及时处理是因为频率太低？
3.  没有及时处理是因为被限制到某个 cpuset 里面，但是 cpu 很满？
4.  此时 Running 的任务是什么？为什么？

[![](https://www.androidperformance.com/images/15638916092620.jpg)](https://www.androidperformance.com/images/15638916092620.jpg)

### [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%99%BD%E8%89%B2-%E4%BC%91%E7%9C%A0%E4%B8%AD%EF%BC%88Sleep%EF%BC%89 "白色 : 休眠中（Sleep）")白色 : 休眠中（Sleep）[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%99%BD%E8%89%B2-%E4%BC%91%E7%9C%A0%E4%B8%AD%EF%BC%88Sleep%EF%BC%89)

线程没有工作要做，可能是因为线程在互斥锁上被阻塞。

作用 ： 这里一般是在等事件驱动  
[![](https://www.androidperformance.com/images/15638916218040.jpg)](https://www.androidperformance.com/images/15638916218040.jpg)

### [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E6%A9%98%E8%89%B2-%E4%B8%8D%E5%8F%AF%E4%B8%AD%E6%96%AD%E7%9A%84%E7%9D%A1%E7%9C%A0%E6%80%81-%EF%BC%88Uninterruptible-Sleep-IO-Block%EF%BC%89 "橘色 : 不可中断的睡眠态 （Uninterruptible Sleep - IO Block）")橘色 : 不可中断的睡眠态 （Uninterruptible Sleep - IO Block）[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E6%A9%98%E8%89%B2-%E4%B8%8D%E5%8F%AF%E4%B8%AD%E6%96%AD%E7%9A%84%E7%9D%A1%E7%9C%A0%E6%80%81-%EF%BC%88Uninterruptible-Sleep-IO-Block%EF%BC%89)

线程在I / O上被阻塞或等待磁盘操作完成，一般底线都会标识出此时的 callsite ：wait\_on\_page\_locked\_killable

作用：这个一般是标示 io 操作慢，如果有大量的橘色不可中断的睡眠态出现，那么一般是由于进入了低内存状态，申请内存的时候触发 pageFault, linux 系统的 page cache 链表中有时会出现一些还没准备好的 page(即还没把磁盘中的内容完全地读出来) , 而正好此时用户在访问这个 page 时就会出现 wait\_on\_page\_locked\_killable 阻塞了. 只有系统当 io 操作很繁忙时, 每笔的 io 操作都需要等待排队时, 极其容易出现且阻塞的时间往往会比较长.

[![](https://www.androidperformance.com/images/15638916331888.jpg)](https://www.androidperformance.com/images/15638916331888.jpg)

### [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%B4%AB%E8%89%B2-%E4%B8%8D%E5%8F%AF%E4%B8%AD%E6%96%AD%E7%9A%84%E7%9D%A1%E7%9C%A0%E6%80%81%EF%BC%88Uninterruptible-Sleep%EF%BC%89 "紫色 : 不可中断的睡眠态（Uninterruptible Sleep）")紫色 : 不可中断的睡眠态（Uninterruptible Sleep）[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%B4%AB%E8%89%B2-%E4%B8%8D%E5%8F%AF%E4%B8%AD%E6%96%AD%E7%9A%84%E7%9D%A1%E7%9C%A0%E6%80%81%EF%BC%88Uninterruptible-Sleep%EF%BC%89)

线程在另一个内核操作（通常是内存管理）上被阻塞。

作用：一般是陷入了内核态，有些情况下是正常的，有些情况下是不正常的，需要按照具体的情况去分析  
[![](https://www.androidperformance.com/images/15638916451317.jpg)](https://www.androidperformance.com/images/15638916451317.jpg)

## [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%BA%BF%E7%A8%8B%E5%94%A4%E9%86%92%E4%BF%A1%E6%81%AF%E5%88%86%E6%9E%90 "线程唤醒信息分析")线程唤醒信息分析[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%BA%BF%E7%A8%8B%E5%94%A4%E9%86%92%E4%BF%A1%E6%81%AF%E5%88%86%E6%9E%90)

Systrace 会标识出一个非常有用的信息，可以帮助我们进行线程调用等待相关的分析。

一个线程被唤醒的信息往往比较重要，知道他被谁唤醒，那么我们也就知道了他们之间的调用等待关系，如果一个线程出现一段比较长的 sleep 情况，然后被唤醒，那么我们就可以去看是谁唤醒了这个线程，对应的就可以查看唤醒者的信息，看看为什么唤醒者这么晚才唤醒。

一个常见的情况是：应用主线程程使用 Binder 与 SystemServer 的 AMS 进行通信，但是恰好 AMS 的这个函数正在等待锁释放（或者这个函数本身执行时间很长），那么应用主线程就需要等待比较长的时间，那么就会出现性能问题，比如响应慢或者卡顿，这就是为什么后台有大量的进程在运行，或者跑完 Monkey 之后，整机性能会下降的一个主要原因

另外一个场景的情况是：应用主线程在等待此应用的其他线程执行的结果，这时候线程唤醒信息就可以用来分析主线程到底被哪个线程 Block 住了，比如下面这个场景，这一帧 doFrame 执行了 152ms，有明显的异常，但是大部分时间是在 sleep

[![image-20211210185851589](https://www.androidperformance.com/images/Android-Systrace-Pre/image-20211210185851589.png)](https://www.androidperformance.com/images/Android-Systrace-Pre/image-20211210185851589.png)

这时候放大来看，可以看到是一段一段被唤醒的，这时候点击图中的 runnable ，下面的信息区就会出现唤醒信息，可以顺着看这个线程到底在做什么

[![image-20211213145728467](https://www.androidperformance.com/images/Android-Systrace-Pre/image-20211213145728467.png)](https://www.androidperformance.com/images/Android-Systrace-Pre/image-20211213145728467.png)

20424 线程是 RenderHeartbeat，这就牵扯到了 App 自身的代码逻辑，需要 App 自己去分析 RenderHeartbeat 到底做了什么事情

[![image-20211210190921614](https://www.androidperformance.com/images/Android-Systrace-Pre/image-20211210190921614.png)](https://www.androidperformance.com/images/Android-Systrace-Pre/image-20211210190921614.png)

Systrace 可以标示出这个的一个原因是，一个任务在进入 Running 状态之前，会先进入 Runnable 状态进行等待，而 Systrace 会把这个状态也标示在 Systrace 上（非常短，需要放大进行看）

[![](https://www.androidperformance.com/images/15638916556947.jpg)](https://www.androidperformance.com/images/15638916556947.jpg)

拉到最上面查看对应的 cpu 上的 taks 信息，会标识这个 task 在被唤醒之前的状态：  
[![](https://www.androidperformance.com/images/15638916674736.jpg)](https://www.androidperformance.com/images/15638916674736.jpg)

顺便贴一下 Linux 常见的进程状态

1.  **D** 无法中断的休眠状态（通常 IO 的进程）；
2.  **R** 正在可运行队列中等待被调度的；
3.  **S** 处于休眠状态；
4.  **T** 停止或被追踪；
5.  **W** 进入内存交换 （从内核2.6开始无效）；
6.  **X** 死掉的进程 （基本很少見）；
7.  **Z** 僵尸进程；
8.  **<** 优先级高的进程
9.  **N** 优先级较低的进程
10.  **L** 有些页被锁进内存
11.  **s** 进程的领导者（在它之下有子进程）
12.  **l** 多进程的（使用 CLONE\_THREAD, 类似 NPTL pthreads）
13.  **+** 位于后台的进程组

## [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E4%BF%A1%E6%81%AF%E5%8C%BA%E6%95%B0%E6%8D%AE%E8%A7%A3%E6%9E%90 "信息区数据解析")信息区数据解析[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E4%BF%A1%E6%81%AF%E5%8C%BA%E6%95%B0%E6%8D%AE%E8%A7%A3%E6%9E%90)

### [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%BA%BF%E7%A8%8B%E7%8A%B6%E6%80%81%E4%BF%A1%E6%81%AF%E8%A7%A3%E6%9E%90 "线程状态信息解析")线程状态信息解析[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E7%BA%BF%E7%A8%8B%E7%8A%B6%E6%80%81%E4%BF%A1%E6%81%AF%E8%A7%A3%E6%9E%90)

[![](https://www.androidperformance.com/images/15638916860044.jpg)](https://www.androidperformance.com/images/15638916860044.jpg)

### [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E5%87%BD%E6%95%B0-Slice-%E4%BF%A1%E6%81%AF%E8%A7%A3%E6%9E%90 "函数 Slice 信息解析")函数 Slice 信息解析[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E5%87%BD%E6%95%B0-Slice-%E4%BF%A1%E6%81%AF%E8%A7%A3%E6%9E%90)

[![](https://www.androidperformance.com/images/15638916944506.jpg)](https://www.androidperformance.com/images/15638916944506.jpg)

### [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#Counter-Sample-%E4%BF%A1%E6%81%AF%E8%A7%A3%E6%9E%90 "Counter Sample 信息解析")Counter Sample 信息解析[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#Counter-Sample-%E4%BF%A1%E6%81%AF%E8%A7%A3%E6%9E%90)

[![](https://www.androidperformance.com/images/15638917076247.jpg)](https://www.androidperformance.com/images/15638917076247.jpg)

### [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#Async-Slice-%E4%BF%A1%E6%81%AF%E8%A7%A3%E6%9E%90 "Async Slice 信息解析")Async Slice 信息解析[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#Async-Slice-%E4%BF%A1%E6%81%AF%E8%A7%A3%E6%9E%90)

[![](https://www.androidperformance.com/images/15638917151530.jpg)](https://www.androidperformance.com/images/15638917151530.jpg)

### [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#CPU-Slice-%E4%BF%A1%E6%81%AF%E8%A7%A3%E6%9E%90 "CPU Slice 信息解析")CPU Slice 信息解析[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#CPU-Slice-%E4%BF%A1%E6%81%AF%E8%A7%A3%E6%9E%90)

[![](https://www.androidperformance.com/images/15638917222302.jpg)](https://www.androidperformance.com/images/15638917222302.jpg)

### [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#User-Expectation-%E4%BF%A1%E6%81%AF%E8%A7%A3%E6%9E%90 "User Expectation 信息解析")User Expectation 信息解析[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#User-Expectation-%E4%BF%A1%E6%81%AF%E8%A7%A3%E6%9E%90)

位于整个 Systrace 最上面的部分,标识了 Rendering Response 和 Input Response  
[![](https://www.androidperformance.com/images/15638917348214.jpg)](https://www.androidperformance.com/images/15638917348214.jpg)

## [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E5%BF%AB%E6%8D%B7%E9%94%AE%E4%BD%BF%E7%94%A8 "快捷键使用")快捷键使用[](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E5%BF%AB%E6%8D%B7%E9%94%AE%E4%BD%BF%E7%94%A8)

快捷键的使用可以加快查看 Systrace 的速度,下面是一些常用的快捷键

**W** : 放大 Systrace , 放大可以更好地看清局部细节  
**S** : 缩小 Systrace, 缩小以查看整体  
**A** : 左移  
**D** : 右移  
**M** : 高亮选中当前鼠标点击的段(这个比较常用,可以快速标识出这个方法的左右边界和执行时间,方便上下查看)

鼠标模式快捷切换 : 主要是针对鼠标的工作模式进行切换 , 默认是 1 ,也就是选择模式,查看 Systrace 的时候,需要经常在各个模式之间切换 , 所以点击切换模式效率比较低,直接用快捷键切换效率要高很多

**数字键1** : 切换到 **Selection 模式** , 这个模式下鼠标可以点击某一个段查看其详细信息, 一般打开 Systrace 默认就是这个模式 , 也是最常用的一个模式 , 配合 M 和 ASDW 可以做基本的操作  
**数字键2** : 切换到 **Pan 模式** , 这个模式下长按鼠标可以左右拖动, 有时候会用到  
**数字键3** : 切换到 **Zoom 模式** , 这个模式下长按鼠标可以放大和缩小, 有时候会用到  
**数字键4** : 切换到 **Timing 模式** , 这个模式下主要是用来衡量时间的,比如选择一个起点, 选择一个终点, 查看起点和终点这中间的操作所花费的时间.

## [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E6%9C%AC%E6%96%87%E7%9F%A5%E4%B9%8E%E5%9C%B0%E5%9D%80 "本文知乎地址")本文知乎地址

由于博客留言交流不方便，点赞或者交流，可以移步本文的知乎或者掘金页面  
[知乎 - Systrace 基础知识 – 分析 Systrace 预备知识](https://zhuanlan.zhihu.com/p/82522750)  
[掘金 - Systrace 基础知识 – 分析 Systrace 预备知识](https://juejin.im/post/5dc18576f265da4d307f1878)

## [](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/#%E5%85%B3%E4%BA%8E%E6%88%91-amp-amp-%E5%8D%9A%E5%AE%A2 "关于我 && 博客")关于我 && 博客

下面是个人的介绍和相关的链接，期望与同行的各位多多交流，三人行，则必有我师!

1.  [博主个人介绍](https://www.androidperformance.com/about/) ：里面有个人的微信和微信群链接。
2.  [本博客内容导航](https://androidperformance.com/2019/12/01/BlogMap/) ：个人博客内容的一个导航。
3.  [个人整理和搜集的优秀博客文章 - Android 性能优化必知必会](https://androidperformance.com/2018/05/07/Android-performance-optimization-skills-and-tools/) ：欢迎大家自荐和推荐 （微信私聊即可）
4.  [Android性能优化知识星球](https://www.androidperformance.com/2023/12/30/the-performance/) ： 欢迎加入，多谢支持～

> **一个人可以走的更快 , 一群人可以走的更远**

[![微信扫一扫](https://www.androidperformance.com/images/WechatIMG581.png)](https://www.androidperformance.com/images/WechatIMG581.png)