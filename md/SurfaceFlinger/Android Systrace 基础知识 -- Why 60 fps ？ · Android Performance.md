本文是 Systrace 系列文章的第三篇，解释一下为何大家总是强调 60 fps。60 fps 是一个软件的概念，与屏幕刷新率里面提到的 60hz 是不一样的，可以参考这篇文章：[新的流畅体验，90Hz 漫谈](https://www.androidperformance.com/2019/05/15/90hz-on-android/)

本系列的目的是通过 Systrace 这个工具，从另外一个角度来看待 Android 的运行，从另外一个角度来对 Framework 进行学习。也许你看了很多讲 Framework 的文章，但是总是记不住代码，或者不清楚其运行的流程，也许从 Systrace 这个图形化的角度，你可以理解的更深入一些。

## [](https://www.androidperformance.com/2019/05/27/why-60-fps/#%E7%B3%BB%E5%88%97%E6%96%87%E7%AB%A0%E7%9B%AE%E5%BD%95 "系列文章目录")系列文章目录

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

## [](https://www.androidperformance.com/2019/05/27/why-60-fps/#%E6%AD%A3%E6%96%87 "正文")正文

今天来讲一下为何我们讲到流畅度，要首先说 60 帧。

我们先来理一下基本的概念：

1.  60 fps 的意思是说，画面每秒更新 60 次
2.  这 60 次更新，是要均匀更新的，不是说一会快，一会慢，那样视觉上也会觉得不流畅
3.  每秒 60 次，也就是 1/60 ~= 16.67 ms 要更新一次

在理解了上面的基本概念之后，我们再回到 Android 这边，为何 Android 现在的渲染机制，是使用 60 fps 作为标准呢？这主要和屏幕的刷新率有关。

## [](https://www.androidperformance.com/2019/05/27/why-60-fps/#%E5%9F%BA%E6%9C%AC%E6%A6%82%E5%BF%B5 "基本概念")基本概念[](https://www.androidperformance.com/2019/05/27/why-60-fps/#%E5%9F%BA%E6%9C%AC%E6%A6%82%E5%BF%B5)

1.  我们前面说的 60 fps，是针对软件的，我们一般称为 fps
2.  屏幕的刷新率，是针对硬件的，现在大部分手机屏幕的刷新率，都维持在60 HZ，**移动设备上一般使用60HZ，是因为移动设备对于功耗的要求更高，提高手机屏幕的刷新率，对于手机来说，逻辑功耗会随着频率的增加而线性增大，同时更高的刷新率，意味着更短的TFT数据写入时间，对屏幕设计来说难度更大。**
3.  屏幕刷新率 60 HZ 只能说**够用**，在目前的情况下是最优解，但是未来肯定是高刷新率屏幕的天下（2023 年的现在 120Hz 已经是 Android 手机的标配了，连 iOS 都已经上到了 120Hz），个人觉得主要依赖下面几点的突破：
    4.  电池技术
    5.  软件技术
    6.  硬件能力

目前的情况下

1.  60 FPS 的情况下：Android 的渲染机制是 16.67 ms 绘制一次， 60hz 的屏幕也是 16.67 ms 刷新一次
2.  120 FPS 的情况下：Android 的渲染机制是 8.33 ms 绘制一次， 120hz 的屏幕也是 8.33 ms 刷新一次

## [](https://www.androidperformance.com/2019/05/27/why-60-fps/#%E6%95%88%E6%9E%9C%E6%8F%90%E5%8D%87 "效果提升")效果提升[](https://www.androidperformance.com/2019/05/27/why-60-fps/#%E6%95%88%E6%9E%9C%E6%8F%90%E5%8D%87)

如果要提升，那么软件和硬件需要一起提升，光提升其中一个，是基本没有效果的，比如你屏幕刷新率是 75 hz，软件是 60 fps，每秒软件渲染60次，你刷新 75 次，是没有啥效果的，除了重复帧率费电；同样，如果你屏幕刷新率是 30 hz，软件是 60 fps，那么软件每秒绘制的60次有一半是没有显示就被抛弃了的。

如果你想体验120hz 刷新率的屏幕，建议你试试 ipad pro ，用过之后你会觉得，60 hz 的屏幕确实有改善的空间。

这一篇主要是简单介绍，如果你想更深入的去了解，可以去 Google 一下，另外 Google 出过一个短视频，介绍了 Why 60 fps， 有条件翻墙的同学可以去看看 ：

1.  [Why 60 fps](https://www.youtube.com/watch?v=CaMTIgxCSqU)
2.  [玩游戏为何要60帧才流畅，电影却只需24帧](https://www.youtube.com/watch?v=--OKrYxOb6Y)

下面这张图是 Android 应用在一帧内所需要完成的任务，后续我们还会详细讲这个：

[![GPU Profile 的含义](https://www.androidperformance.com/images/media/15225938262396.jpg)](https://www.androidperformance.com/images/media/15225938262396.jpg)

## [](https://www.androidperformance.com/2019/05/27/why-60-fps/#%E5%85%B3%E4%BA%8E%E6%88%91-amp-amp-%E5%8D%9A%E5%AE%A2 "关于我 && 博客")关于我 && 博客

下面是个人的介绍和相关的链接，期望与同行的各位多多交流，三人行，则必有我师!

1.  [博主个人介绍](https://www.androidperformance.com/about/) ：里面有个人的微信和微信群链接。
2.  [本博客内容导航](https://androidperformance.com/2019/12/01/BlogMap/) ：个人博客内容的一个导航。
3.  [个人整理和搜集的优秀博客文章 - Android 性能优化必知必会](https://androidperformance.com/2018/05/07/Android-performance-optimization-skills-and-tools/) ：欢迎大家自荐和推荐 （微信私聊即可）
4.  [Android性能优化知识星球](https://www.androidperformance.com/2023/12/30/the-performance/) ： 欢迎加入，多谢支持～

> **一个人可以走的更快 , 一群人可以走的更远**

[![微信扫一扫](https://www.androidperformance.com/images/WechatIMG581.png)](https://www.androidperformance.com/images/WechatIMG581.png)