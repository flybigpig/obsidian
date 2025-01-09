本文是 Systrace 系列文章的第四篇，主要是对 SystemServer 进行简单介绍，介绍了 SystemServer 中几个比较重要的线程，由于 Input 和 Binder 比较重要，所以单独拿出来讲，在这里就没有再涉及到。

本系列的目的是通过 Systrace 这个工具，从另外一个角度来看待 Android 系统整体的运行，同时也从另外一个角度来对 Framework 进行学习。也许你看了很多讲 Framework 的文章，但是总是记不住代码，或者不清楚其运行的流程，也许从 Systrace 这个图形化的角度，你可以理解的更深入一些。

## [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#%E7%B3%BB%E5%88%97%E6%96%87%E7%AB%A0%E7%9B%AE%E5%BD%95 "系列文章目录")系列文章目录

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

## [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#%E6%AD%A3%E6%96%87 "正文")正文

## [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#%E7%AA%97%E5%8F%A3%E5%8A%A8%E7%94%BB "窗口动画")窗口动画[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#%E7%AA%97%E5%8F%A3%E5%8A%A8%E7%94%BB)

Systrace 中的 SystemServer 一个比较重要的地方就是窗口动画，由于窗口归 SystemServer 来管，那么窗口动画也就是由 SystemServer 来进行统一的处理，其中涉及到两个比较重要的线程，Android.Anim 和 Android.Anim.if 这两个线程，这两个线程的基本知识在下面有讲。

这里我们以**应用启动**为例，查看窗口时如何在两个线程之间进行切换(Android P 里面，应用的启动动画由 Launcher 和应用自己的第一帧组成，之前是在 SystemServer 里面的，现在多任务的动画为了性能部分移到了 Launcher 去实现)

首先我们点击图标启动应用的时候，由于 App 还在启动，Launcher 首先启动一个 StartingWindow，等 App 的第一帧绘制好了之后，再切换到 App 的窗口动画

Launcher 动画  
[![-w1019](https://www.androidperformance.com/images/15811380751710.jpg)](https://www.androidperformance.com/images/15811380751710.jpg)

此时对应的，App 正在启动  
[![-w1025](https://www.androidperformance.com/images/15811380510520.jpg)](https://www.androidperformance.com/images/15811380510520.jpg)

从上图可以看到，应用第一帧已经准备好了，接下来看对应的 SystemServer ，可以看到应用启动第一帧绘制完成后，动画切换到 App 的 Window 动画

[![-w1236](https://www.androidperformance.com/images/15811383348116.jpg)](https://www.androidperformance.com/images/15811383348116.jpg)

## [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#ActivityManagerService "ActivityManagerService")ActivityManagerService[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#ActivityManagerService)

AMS 和 WMS 算是 SystemServer 中最繁忙的两个 Service 了，与 AMS 相关的 Trace 一般会用 TRACE\_TAG\_ACTIVITY\_MANAGER 这个 TAG，在 Systrace 中的名字是 ActivityManager

下面是启动一个新的进程的时候，AMS 的输出  
[![-w826](https://www.androidperformance.com/images/15808922537197.jpg)](https://www.androidperformance.com/images/15808922537197.jpg)

在进程和四大组件的各种场景一般都会有对应的 Trace 点来记录，比如大家熟悉的 ActivityStart、ActivityResume、activityStop 等，这些 Trace 点有一些在应用进程，有一些在 SystemServer 进程，所以大家在看 Activity 相关的代码逻辑的时候，需要不断在这两个进程之间进行切换，这样才能从一个整体的角度来看应用的状态变化和 SystemServer 在其中起到的作用。  
[![-w660](https://www.androidperformance.com/images/15808919921881.jpg)](https://www.androidperformance.com/images/15808919921881.jpg)

## [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#WindowManagerService "WindowManagerService")WindowManagerService[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#WindowManagerService)

与 WMS 相关的 Trace 一般会用 TRACE\_TAG\_WINDOW\_MANAGER 这个 TAG，在 Systrace 中 WindowManagerService 在 SystemServer 中多在对应的 Binder 中出现，比如下面应用启动的时候，relayoutWindow 的 Trace 输出

[![-w957](https://www.androidperformance.com/images/15808923853151.jpg)](https://www.androidperformance.com/images/15808923853151.jpg)

在 Window 的各种场景一般都会有对应的 Trace 点来记录，比如大家熟悉的 relayoutWIndow、performLayout、prepareToDisplay 等  
[![-w659](https://www.androidperformance.com/images/15808918520410.jpg)](https://www.androidperformance.com/images/15808918520410.jpg)

## [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#Input "Input")Input[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#Input)

Input 是 SystemServer 线程里面非常重要的一部分，主要是由 InputReader 和 InputDispatcher 这两个 Native 线程组成，关于这一部分在 [Systrace 基础知识 - Input 解读](https://www.androidperformance.com/2019/11/04/Android-Systrace-Input/) 里面已经详细讲过，这里就不再详细讲了

[![-w725](https://www.androidperformance.com/images/15808245020456.jpg)](https://www.androidperformance.com/images/15808245020456.jpg)

## [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#Binder "Binder")Binder[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#Binder)

SystemServer 由于提供大量的基础服务，所以进程间的通信非常繁忙，且大部分通信都是通过 Binder ，所以 Binder 在 SystemServer 中的作用非常关键，很多时候当后台有大量的 App 存在的时候，SystemServer 就会由于 Binder 通信和锁竞争，导致系统或者 App 卡顿。关于这一部分在 [Binder 和锁竞争解读](https://www.androidperformance.com/2019/12/06/Android-Systrace-Binder/) 里面已经详细讲过，这里就不再详细讲了

[![-w1028](https://www.androidperformance.com/images/15808245356047.jpg)](https://www.androidperformance.com/images/15808245356047.jpg)

## [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#HandlerThread "HandlerThread")HandlerThread[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#HandlerThread)

### [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#BackgroundThread "BackgroundThread")BackgroundThread[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#BackgroundThread)

com/android/internal/os/BackgroundThread.java

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br></pre></td><td><pre><span><span>private</span> <span>BackgroundThread</span><span>()</span> {</span><br><span>    <span>super</span>(<span>"android.bg"</span>, android.os.Process.THREAD_PRIORITY_BACKGROUND);</span><br><span>}</span><br></pre></td></tr></tbody></table>

Systrace 中的 BackgroundThread  
[![-w1082](https://www.androidperformance.com/images/15808252037825.jpg)](https://www.androidperformance.com/images/15808252037825.jpg)

BackgroundThread 在系统中使用比较多，许多对性能没有要求的任务，一般都会放到 BackgroundThread 中去执行

[![-w654](https://www.androidperformance.com/images/15808271946061.jpg)](https://www.androidperformance.com/images/15808271946061.jpg)

## [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#ServiceThread "ServiceThread")ServiceThread[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#ServiceThread)

ServiceThread 继承自 HandlerThread ，下面介绍的几个工作线程都是继承自 ServiceThread ，分别实现不同的功能，根据线程功能不同，其线程优先级也不同：UIThread、IoThread、DisplayThread、AnimationThread、FgThread、SurfaceAnimationThread

每个 Thread 都有自己的 Looper 、Thread 和 MessageQueue，互相不会影响。Android 系统根据功能，会使用不同的 Thread 来完成。

### [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#UiThread "UiThread")UiThread[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#UiThread)

com/android/server/UiThread.java

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br></pre></td><td><pre><span><span>private</span> <span>UiThread</span><span>()</span> {</span><br><span>    <span>super</span>(<span>"android.ui"</span>, Process.THREAD_PRIORITY_FOREGROUND, <span>false</span> );</span><br><span>}</span><br></pre></td></tr></tbody></table>

Systrace 中的 UiThread  
[![-w1049](https://www.androidperformance.com/images/15808252975757.jpg)](https://www.androidperformance.com/images/15808252975757.jpg)

UiThread 被使用的地方如下，具体的功能可以自己去源码里面查看，关键字是 UiThread.get()  
[![-w650](https://www.androidperformance.com/images/15808258949148.jpg)](https://www.androidperformance.com/images/15808258949148.jpg)

### [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#IoThread "IoThread")IoThread[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#IoThread)

com/android/server/IoThread.java

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br></pre></td><td><pre><span><span>private</span> <span>IoThread</span><span>()</span> {</span><br><span>    <span>super</span>(<span>"android.io"</span>, android.os.Process.THREAD_PRIORITY_DEFAULT, <span>true</span> );</span><br><span>}</span><br></pre></td></tr></tbody></table>

IoThread 被使用的地方如下，具体的功能可以自己去源码里面查看，关键字是 IoThread.get()  
[![-w654](https://www.androidperformance.com/images/15808257964346.jpg)](https://www.androidperformance.com/images/15808257964346.jpg)

### [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#DisplayThread "DisplayThread")DisplayThread[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#DisplayThread)

com/android/server/DisplayThread.java

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br></pre></td><td><pre><span><span>private</span> <span>DisplayThread</span><span>()</span> {</span><br><span>    </span><br><span>    </span><br><span>    <span>super</span>(<span>"android.display"</span>, Process.THREAD_PRIORITY_DISPLAY + <span>1</span>, <span>false</span> );</span><br><span>}</span><br></pre></td></tr></tbody></table>

Systrace 中的 DisplayThread  
[![-w1108](https://www.androidperformance.com/images/15808251210767.jpg)](https://www.androidperformance.com/images/15808251210767.jpg)

[![-w656](https://www.androidperformance.com/images/15808259701453.jpg)](https://www.androidperformance.com/images/15808259701453.jpg)

### [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#AnimationThread "AnimationThread")AnimationThread[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#AnimationThread)

com/android/server/AnimationThread.java

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br></pre></td><td><pre><span><span>private</span> <span>AnimationThread</span><span>()</span> {</span><br><span>    <span>super</span>(<span>"android.anim"</span>, THREAD_PRIORITY_DISPLAY, <span>false</span> );</span><br><span>}</span><br></pre></td></tr></tbody></table>

Systrace 中的 AnimationThread  
[![-w902](https://www.androidperformance.com/images/15808255124784.jpg)](https://www.androidperformance.com/images/15808255124784.jpg)

AnimationThread 在源码中的使用，可以看到 WindowAnimator 的动画执行也是在 AnimationThread 线程中的，Android P 增加了一个 SurfaceAnimationThread 来分担 AnimationThread 的部分工作，来提高 WindowAnimation 的动画性能

[![-w657](https://www.androidperformance.com/images/15808260775808.jpg)](https://www.androidperformance.com/images/15808260775808.jpg)

### [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#FgThread "FgThread")FgThread[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#FgThread)

com/android/server/FgThread.java

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br></pre></td><td><pre><span><span>private</span> <span>FgThread</span><span>()</span> {</span><br><span>    <span>super</span>(<span>"android.fg"</span>, android.os.Process.THREAD_PRIORITY_DEFAULT, <span>true</span> );</span><br><span>}</span><br></pre></td></tr></tbody></table>

Systrace 中的 FgThread  
[![-w1018](https://www.androidperformance.com/images/15808253825450.jpg)](https://www.androidperformance.com/images/15808253825450.jpg)

FgThread 在源码中的使用，可以自己搜一下，下面是具体的使用的一个例子

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br></pre></td><td><pre><span>FgThread.getHandler().post(() -&gt; {</span><br><span>    <span>synchronized</span> (mLock) {</span><br><span>        <span>if</span> (mStartedUsers.get(userIdToLockF) != <span>null</span>) {</span><br><span>            Slog.w(TAG, <span>"User was restarted, skipping key eviction"</span>);</span><br><span>            <span>return</span>;</span><br><span>        }</span><br><span>    }</span><br><span>    <span>try</span> {</span><br><span>        mInjector.getStorageManager().lockUserKey(userIdToLockF);</span><br><span>    } <span>catch</span> (RemoteException re) {</span><br><span>        <span>throw</span> re.rethrowAsRuntimeException();</span><br><span>    }</span><br><span>    <span>if</span> (userIdToLockF == userId) {</span><br><span>        <span>for</span> (<span>final</span> KeyEvictedCallback callback : keyEvictedCallbacks) {</span><br><span>            callback.keyEvicted(userId);</span><br><span>        }</span><br><span>    }</span><br><span>});</span><br></pre></td></tr></tbody></table>

### [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#SurfaceAnimationThread "SurfaceAnimationThread")SurfaceAnimationThread[](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#SurfaceAnimationThread)

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br></pre></td><td><pre><span>com/android/server/wm/SurfaceAnimationThread.java</span><br><span><span>private</span> <span>SurfaceAnimationThread</span><span>()</span> {</span><br><span>    <span>super</span>(<span>"android.anim.lf"</span>, THREAD_PRIORITY_DISPLAY, <span>false</span> );</span><br><span>}</span><br></pre></td></tr></tbody></table>

Systrace 中的 SurfaceAnimationThread  
[![-w1148](https://www.androidperformance.com/images/15808254715766.jpg)](https://www.androidperformance.com/images/15808254715766.jpg)

SurfaceAnimationThread 的名字叫 android.anim.lf ， 与 android.anim 有区别，  
[![-w657](https://www.androidperformance.com/images/15808262588087.jpg)](https://www.androidperformance.com/images/15808262588087.jpg)

这个 Thread 主要是执行窗口动画，用于分担 android.anim 线程的一部分动画工作，减少由于锁导致的窗口动画卡顿问题，具体的内容可以看这篇文章：[Android P——LockFreeAnimation](https://zhuanlan.zhihu.com/p/44864987)

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br></pre></td><td><pre><span>SurfaceAnimationRunner(<span>@Nullable</span> AnimationFrameCallbackProvider callbackProvider,</span><br><span>        AnimatorFactory animatorFactory, Transaction frameTransaction,</span><br><span>        PowerManagerInternal powerManagerInternal) {</span><br><span>    SurfaceAnimationThread.getHandler().runWithScissors(() -&gt; mChoreographer = getSfInstance(),</span><br><span>            <span>0</span> );</span><br><span>    mFrameTransaction = frameTransaction;</span><br><span>    mAnimationHandler = <span>new</span> <span>AnimationHandler</span>();</span><br><span>    mAnimationHandler.setProvider(callbackProvider != <span>null</span></span><br><span>            ? callbackProvider</span><br><span>            : <span>new</span> <span>SfVsyncFrameCallbackProvider</span>(mChoreographer));</span><br><span>    mAnimatorFactory = animatorFactory != <span>null</span></span><br><span>            ? animatorFactory</span><br><span>            : SfValueAnimator::<span>new</span>;</span><br><span>    mPowerManagerInternal = powerManagerInternal;</span><br><span>}</span><br></pre></td></tr></tbody></table>

## [](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/#%E5%85%B3%E4%BA%8E%E6%88%91-amp-amp-%E5%8D%9A%E5%AE%A2 "关于我 && 博客")关于我 && 博客

下面是个人的介绍和相关的链接，期望与同行的各位多多交流，三人行，则必有我师!

1.  [博主个人介绍](https://www.androidperformance.com/about/) ：里面有个人的微信和微信群链接。
2.  [本博客内容导航](https://androidperformance.com/2019/12/01/BlogMap/) ：个人博客内容的一个导航。
3.  [个人整理和搜集的优秀博客文章 - Android 性能优化必知必会](https://androidperformance.com/2018/05/07/Android-performance-optimization-skills-and-tools/) ：欢迎大家自荐和推荐 （微信私聊即可）
4.  [Android性能优化知识星球](https://www.androidperformance.com/2023/12/30/the-performance/) ： 欢迎加入，多谢支持～

> **一个人可以走的更快 , 一群人可以走的更远**

[![微信扫一扫](https://www.androidperformance.com/images/WechatIMG581.png)](https://www.androidperformance.com/images/WechatIMG581.png)