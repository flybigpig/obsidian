本文是 Systrace 系列文章的第五篇，主要是对 SurfaceFlinger 的工作流程进行简单介绍，介绍了 SurfaceFlinger 中几个比较重要的线程，包括 Vsync 信号的解读、应用的 Buffer 展示、卡顿判定等，由于 Vsync 这一块在 [Systrace 基础知识 - Vsync 解读](https://www.androidperformance.com/2019/12/01/Android-Systrace-Vsync/) 和 [Android 基于 Choreographer 的渲染机制详解](https://androidperformance.com/2019/10/22/Android-Choreographer/) 这两篇文章里面已经介绍过，这里就不再做详细的讲解了。

本系列的目的是通过 Systrace 这个工具，从另外一个角度来看待 Android 系统整体的运行，同时也从另外一个角度来对 Framework 进行学习。也许你看了很多讲 Framework 的文章，但是总是记不住代码，或者不清楚其运行的流程，也许从 Systrace 这个图形化的角度，你可以理解的更深入一些。

## [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#%E7%B3%BB%E5%88%97%E6%96%87%E7%AB%A0%E7%9B%AE%E5%BD%95 "系列文章目录")系列文章目录

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

## [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#%E6%AD%A3%E6%96%87 "正文")正文

这里直接上官方对于 [SurfaceFlinger 的定义](https://source.android.google.cn/devices/graphics/arch-sf-hwc.html?authuser=0&hl=de)

1.  大多数应用在屏幕上一次显示三个层：屏幕顶部的状态栏、底部或侧面的导航栏以及应用界面。有些应用会拥有更多或更少的层（例如，默认主屏幕应用有一个单独的壁纸层，而全屏游戏可能会隐藏状态栏）。每个层都可以单独更新。状态栏和导航栏由系统进程渲染，而应用层由应用渲染，两者之间不进行协调。
2.  设备显示会按一定速率刷新，在手机和平板电脑上通常为 60 fps。如果显示内容在刷新期间更新，则会出现撕裂现象；因此，请务必只在周期之间更新内容。在可以安全更新内容时，系统便会收到来自显示设备的信号。由于历史原因，我们将该信号称为 VSYNC 信号。
3.  刷新率可能会随时间而变化，例如，一些移动设备的帧率范围在 58 fps 到 62 fps 之间，具体要视当前条件而定。对于连接了 HDMI 的电视，刷新率在理论上可以下降到 24 Hz 或 48 Hz，以便与视频相匹配。由于每个刷新周期只能更新屏幕一次，因此以 200 fps 的帧率为显示设备提交缓冲区就是一种资源浪费，因为大多数帧会被舍弃掉。SurfaceFlinger 不会在应用每次提交缓冲区时都执行操作，而是在显示设备准备好接收新的缓冲区时才会唤醒。
4.  当 VSYNC 信号到达时，SurfaceFlinger 会遍历它的层列表，以寻找新的缓冲区。如果找到新的缓冲区，它会获取该缓冲区；否则，它会继续使用以前获取的缓冲区。SurfaceFlinger 必须始终显示内容，因此它会保留一个缓冲区。如果在某个层上没有提交缓冲区，则该层会被忽略。
5.  SurfaceFlinger 在收集可见层的所有缓冲区之后，便会询问 Hardware Composer 应如何进行合成。」

—- 引用自[SurfaceFlinger 和 Hardware Composer](https://source.android.google.cn/devices/graphics/arch-sf-hwc.html?authuser=0&hl=de)

下面是上述流程所对应的流程图， 简单地说， SurfaceFlinger 最主要的功能:**SurfaceFlinger 接受来自多个来源的数据缓冲区，对它们进行合成，然后发送到显示设备。**

[![](https://www.androidperformance.com/images/15816781462135.jpg)](https://www.androidperformance.com/images/15816781462135.jpg)

那么 Systrace 中，我们关注的重点就是上面这幅图对应的部分

1.  App 部分
2.  BufferQueue 部分
3.  SurfaceFlinger 部分
4.  HWComposer 部分

这四部分，在 Systrace 中都有可以对应的地方，以时间发生的顺序排序就是 1、2、3、4，下面我们从 Systrace 的这四部分来看整个渲染的流程

## [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#App-%E9%83%A8%E5%88%86 "App 部分")App 部分[](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#App-%E9%83%A8%E5%88%86)

关于 App 部分，其实在[Systrace 基础知识 - MainThread 和 RenderThread 解读](https://www.androidperformance.com/2019/11/06/Android-Systrace-MainThread-And-RenderThread/)这篇文章里面已经说得比较清楚了，不清楚的可以去这篇文章里面看，其主要的流程如下图：

[![](https://www.androidperformance.com/images/15818258189902.jpg)](https://www.androidperformance.com/images/15818258189902.jpg)

从 SurfaceFlinger 的角度来看，App 部分主要负责生产 SurfaceFlinger 合成所需要的 Surface。

App 与 SurfaceFlinger 的交互主要集中在三点

1.  Vsync 信号的接收和处理
2.  RenderThread 的 dequeueBuffer
3.  RenderThread 的 queueBuffer

### [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#Vsync-%E4%BF%A1%E5%8F%B7%E7%9A%84%E6%8E%A5%E6%94%B6%E5%92%8C%E5%A4%84%E7%90%86 "Vsync 信号的接收和处理")Vsync 信号的接收和处理[](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#Vsync-%E4%BF%A1%E5%8F%B7%E7%9A%84%E6%8E%A5%E6%94%B6%E5%92%8C%E5%A4%84%E7%90%86)

关于这部分内容可以查看 [Android 基于 Choreographer 的渲染机制详解](https://www.androidperformance.com/2019/10/22/Android-Choreographer/) 这篇文章，App 和 SurfaceFlinger 的第一个交互点就是 Vsync 信号的请求和接收，如上图中第一条标识，Vsync-App 信号到达，就是指的是 SurfaceFlinger 的 Vsync-App 信号。应用收到这个信号后，开始一帧的渲染准备

[![](https://www.androidperformance.com/images/15822547481351.jpg)](https://www.androidperformance.com/images/15822547481351.jpg)

### [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#RenderThread-%E7%9A%84-dequeueBuffer "RenderThread 的 dequeueBuffer")RenderThread 的 dequeueBuffer[](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#RenderThread-%E7%9A%84-dequeueBuffer)

dequeue 有出队的意思，dequeueBuffer 顾名思义，就是从队列中拿出一个 Buffer，这个队列就是 SurfaceFlinger 中的 BufferQueue。如下图，应用开始渲染前，首先需要通过 Binder 调用从 SurfaceFlinger 的 BufferQueue 中获取一个 Buffer，其流程如下：

**App 端的 Systrace 如下所示**  
[![-w1249](https://www.androidperformance.com/images/15822556410563.jpg)](https://www.androidperformance.com/images/15822556410563.jpg)

**SurfaceFlinger 端的 Systrace 如下所示**  
[![-w826](https://www.androidperformance.com/images/15822558376614.jpg)](https://www.androidperformance.com/images/15822558376614.jpg)

### [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#RenderThread-%E7%9A%84-queueBuffer "RenderThread 的 queueBuffer")RenderThread 的 queueBuffer[](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#RenderThread-%E7%9A%84-queueBuffer)

queue 有入队的意思，queueBuffer 顾名思义就是讲 Buffer 放回到 BufferQueue，App 处理完 Buffer 后（写入具体的 drawcall），会把这个 Buffer 通过 eglSwapBuffersWithDamageKHR -> queueBuffer 这个流程，将 Buffer 放回 BufferQueue，其流程如下

**App 端的 Systrace 如下所示**  
[![-w1165](https://www.androidperformance.com/images/15822960954718.jpg)](https://www.androidperformance.com/images/15822960954718.jpg)

**SurfaceFlinger 端的 Systrace 如下所示**  
[![-w1295](https://www.androidperformance.com/images/15822964913781.jpg)](https://www.androidperformance.com/images/15822964913781.jpg)

通过上面三部分，大家应该对下图中的流程会有一个比较直观的了解了  
[![-w410](https://www.androidperformance.com/images/15822965692055.jpg)](https://www.androidperformance.com/images/15822965692055.jpg)

## [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#BufferQueue-%E9%83%A8%E5%88%86 "BufferQueue 部分")BufferQueue 部分[](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#BufferQueue-%E9%83%A8%E5%88%86)

BufferQueue 部分其实在[Systrace 基础知识 - Triple Buffer 解读](https://www.androidperformance.com/2019/12/15/Android-Systrace-Triple-Buffer/#BufferQueue) 这里有讲，如下图，结合上面那张图，每个有显示界面的进程对应一个 BufferQueue，使用方创建并拥有 BufferQueue 数据结构，并且可存在于与其生产方不同的进程中，BufferQueue 工作流程如下：

[![](https://www.androidperformance.com/images/15823652509728.jpg)](https://www.androidperformance.com/images/15823652509728.jpg)

上图主要是 dequeue、queue、acquire、release ，在这个例子里面，App 是**生产者**，负责填充显示缓冲区（Buffer）；SurfaceFlinger 是**消费者**，将各个进程的显示缓冲区做合成操作

1.  dequeue(生产者发起) ： 当生产者需要缓冲区时，它会通过调用 dequeueBuffer() 从 BufferQueue 请求一个可用的缓冲区，并指定缓冲区的宽度、高度、像素格式和使用标记。
2.  queue(生产者发起)：生产者填充缓冲区并通过调用 queueBuffer() 将缓冲区返回到队列。
3.  acquire(消费者发起) ：消费者通过 acquireBuffer() 获取该缓冲区并使用该缓冲区的内容
4.  release(消费者发起) ：当消费者操作完成后，它会通过调用 releaseBuffer() 将该缓冲区返回到队列

## [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#SurfaceFlinger-%E9%83%A8%E5%88%86 "SurfaceFlinger 部分")SurfaceFlinger 部分[](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#SurfaceFlinger-%E9%83%A8%E5%88%86)

### [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#%E5%B7%A5%E4%BD%9C%E6%B5%81%E7%A8%8B "工作流程")工作流程[](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#%E5%B7%A5%E4%BD%9C%E6%B5%81%E7%A8%8B)

从最前面我们知道 SurfaceFlinger 的主要工作就是合成：

> 当 VSYNC 信号到达时，SurfaceFlinger 会遍历它的层列表，以寻找新的缓冲区。如果找到新的缓冲区，它会获取该缓冲区；否则，它会继续使用以前获取的缓冲区。SurfaceFlinger 必须始终显示内容，因此它会保留一个缓冲区。如果在某个层上没有提交缓冲区，则该层会被忽略。SurfaceFlinger 在收集可见层的所有缓冲区之后，便会询问 Hardware Composer 应如何进行合成。

其 Systrace 主线程可用看到其主要是在收到 Vsync 信号后开始工作  
[![-w1296](https://www.androidperformance.com/images/15822972813466.jpg)](https://www.androidperformance.com/images/15822972813466.jpg)

其对应的代码如下,主要是处理两个 Message

1.  MessageQueue::INVALIDATE — 主要是执行 handleMessageTransaction 和 handleMessageInvalidate 这两个方法
2.  MessageQueue::REFRESH — 主要是执行 handleMessageRefresh 方法

frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br><span>40</span><br><span>41</span><br><span>42</span><br><span>43</span><br><span>44</span><br><span>45</span><br><span>46</span><br><span>47</span><br><span>48</span><br><span>49</span><br><span>50</span><br><span>51</span><br><span>52</span><br><span>53</span><br><span>54</span><br><span>55</span><br><span>56</span><br><span>57</span><br><span>58</span><br><span>59</span><br><span>60</span><br><span>61</span><br><span>62</span><br><span>63</span><br><span>64</span><br><span>65</span><br><span>66</span><br><span>67</span><br><span>68</span><br><span>69</span><br><span>70</span><br><span>71</span><br><span>72</span><br><span>73</span><br><span>74</span><br><span>75</span><br><span>76</span><br></pre></td><td><pre><span><span><span>void</span> <span>SurfaceFlinger::onMessageReceived</span><span>(<span>int32_t</span> what)</span> NO_THREAD_SAFETY_ANALYSIS </span>{</span><br><span>    <span>ATRACE_CALL</span>();</span><br><span>    <span>switch</span> (what) {</span><br><span>        <span>case</span> MessageQueue::INVALIDATE: {</span><br><span>            ......</span><br><span>            <span>bool</span> refreshNeeded = <span>handleMessageTransaction</span>();</span><br><span>            refreshNeeded |= <span>handleMessageInvalidate</span>();</span><br><span>            ......</span><br><span>            <span>break</span>;</span><br><span>        }</span><br><span>        <span>case</span> MessageQueue::REFRESH: {</span><br><span>            <span>handleMessageRefresh</span>();</span><br><span>            <span>break</span>;</span><br><span>        }</span><br><span>    }</span><br><span>}</span><br><span></span><br><span></span><br><span><span><span>bool</span> <span>SurfaceFlinger::handleMessageInvalidate</span><span>()</span> </span>{</span><br><span>    <span>ATRACE_CALL</span>();</span><br><span>    <span>bool</span> refreshNeeded = <span>handlePageFlip</span>();</span><br><span></span><br><span>    <span>if</span> (mVisibleRegionsDirty) {</span><br><span>        <span>computeLayerBounds</span>();</span><br><span>        <span>if</span> (mTracingEnabled) {</span><br><span>            mTracing.<span>notify</span>(<span>"visibleRegionsDirty"</span>);</span><br><span>        }</span><br><span>    }</span><br><span></span><br><span>    <span>for</span> (<span>auto</span>&amp; layer : mLayersPendingRefresh) {</span><br><span>        Region visibleReg;</span><br><span>        visibleReg.<span>set</span>(layer-&gt;<span>getScreenBounds</span>());</span><br><span>        <span>invalidateLayerStack</span>(layer, visibleReg);</span><br><span>    }</span><br><span>    mLayersPendingRefresh.<span>clear</span>();</span><br><span>    <span>return</span> refreshNeeded;</span><br><span>}</span><br><span></span><br><span></span><br><span><span><span>void</span> <span>SurfaceFlinger::handleMessageRefresh</span><span>()</span> </span>{</span><br><span>    <span>ATRACE_CALL</span>();</span><br><span></span><br><span>    mRefreshPending = <span>false</span>;</span><br><span></span><br><span>    <span>const</span> <span>bool</span> repaintEverything = mRepaintEverything.<span>exchange</span>(<span>false</span>);</span><br><span>    <span>preComposition</span>();</span><br><span>    <span>rebuildLayerStacks</span>();</span><br><span>    <span>calculateWorkingSet</span>();</span><br><span>    <span>for</span> (<span>const</span> <span>auto</span>&amp; [token, display] : mDisplays) {</span><br><span>        <span>beginFrame</span>(display);</span><br><span>        <span>prepareFrame</span>(display);</span><br><span>        <span>doDebugFlashRegions</span>(display, repaintEverything);</span><br><span>        <span>doComposition</span>(display, repaintEverything);</span><br><span>    }</span><br><span></span><br><span>    <span>logLayerStats</span>();</span><br><span></span><br><span>    <span>postFrame</span>();</span><br><span>    <span>postComposition</span>();</span><br><span></span><br><span>    mHadClientComposition = <span>false</span>;</span><br><span>    mHadDeviceComposition = <span>false</span>;</span><br><span>    <span>for</span> (<span>const</span> <span>auto</span>&amp; [token, displayDevice] : mDisplays) {</span><br><span>        <span>auto</span> display = displayDevice-&gt;<span>getCompositionDisplay</span>();</span><br><span>        <span>const</span> <span>auto</span> displayId = display-&gt;<span>getId</span>();</span><br><span>        mHadClientComposition =</span><br><span>                mHadClientComposition || <span>getHwComposer</span>().<span>hasClientComposition</span>(displayId);</span><br><span>        mHadDeviceComposition =</span><br><span>                mHadDeviceComposition || <span>getHwComposer</span>().<span>hasDeviceComposition</span>(displayId);</span><br><span>    }</span><br><span></span><br><span>    mVsyncModulator.<span>onRefreshed</span>(mHadClientComposition);</span><br><span></span><br><span>    mLayersWithQueuedFrames.<span>clear</span>();</span><br><span>}</span><br><span></span><br></pre></td></tr></tbody></table>

handleMessageRefresh 中按照重要性主要有下面几个功能

1.  准备工作
    1.  preComposition();
    2.  rebuildLayerStacks();
    3.  calculateWorkingSet();
2.  合成工作
    1.  begiFrame(display);
    2.  prepareFrame(display);
    3.  doDebugFlashRegions(display, repaintEverything);
    4.  doComposition(display, repaintEverything);
3.  收尾工作
    1.  logLayerStats();
    2.  postFrame();
    3.  postComposition();

由于显示系统有非常庞大的细节，这里就不一一进行讲解了，如果你的工作在这一部分，那么所有的流程都需要熟悉并掌握，如果只是想熟悉流程，那么不需要太深入，知道 SurfaceFlinger 的主要工作逻辑即可

### [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#%E6%8E%89%E5%B8%A7 "掉帧")掉帧[](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#%E6%8E%89%E5%B8%A7)

通常我们通过 Systrace 判断应用是否**掉帧**的时候，一般是直接看 SurfaceFlinger 部分，主要是下面几个步骤

1.  SurfaceFlinger 的主线程在每个 Vsync-SF 的时候是否没有合成？
2.  如果没有合成操作，那么需要看没有合成的原因：
    1.  因为 SurfaceFlinger 检查发现没有可用的 Buffer 而没有合成操作？
    2.  因为 SurfaceFlinger 被其他的工作占用（比如截图、HWC 等）？
    3.  因为 SurfaceFlinger 在等 presentFence ？
    4.  因为 SurfaceFlinger 在等 GPU fence？
3.  如果有合成操作，那么需要看 **你关心的 App** 的 可用 Buffer 个数是否正常：如果 App 此时可用 Buffer 为 0，那么看 App 端为何没有及时 queueBuffer（这就一般是应用自身的问题了），因为 SurfaceFlinger 合成操作触发可能是其他的进程有可用的 Buffer

关于这一部分的 Systrace 怎么看，在 [Systrace 基础知识 - Triple Buffer 解读-掉帧检测](https://www.androidperformance.com/2019/12/15/Android-Systrace-Triple-Buffer/#%E9%80%BB%E8%BE%91%E6%8E%89%E5%B8%A7) 部分已经有比较详细的解读，大家可以过去看这一段

## [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#HWComposer-%E9%83%A8%E5%88%86 "HWComposer 部分")HWComposer 部分[](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#HWComposer-%E9%83%A8%E5%88%86)

关于 HWComposer 的功能部分我们就直接看 [官方的介绍](https://source.android.google.cn/devices/graphics/arch-sf-hwc.html?authuser=0&hl=de) 即可

1.  Hardware Composer HAL (HWC) 用于确定通过可用硬件来合成缓冲区的最有效方法。作为 HAL，其实现是特定于设备的，而且通常由显示设备硬件原始设备制造商 (OEM) 完成。
2.  当您考虑使用叠加平面时，很容易发现这种方法的好处，它会在显示硬件（而不是 GPU）中合成多个缓冲区。例如，假设有一部普通 Android 手机，其屏幕方向为纵向，状态栏在顶部，导航栏在底部，其他区域显示应用内容。每个层的内容都在单独的缓冲区中。您可以使用以下任一方法处理合成（后一种方法可以显著提高效率）：
    1.  将应用内容渲染到暂存缓冲区中，然后在其上渲染状态栏，再在其上渲染导航栏，最后将暂存缓冲区传送到显示硬件。
    2.  将三个缓冲区全部传送到显示硬件，并指示它从不同的缓冲区读取屏幕不同部分的数据。
3.  显示处理器功能差异很大。叠加层的数量（无论层是否可以旋转或混合）以及对定位和叠加的限制很难通过 API 表达。为了适应这些选项，HWC 会执行以下计算（由于硬件供应商可以定制决策代码，因此可以在每台设备上实现最佳性能）：
    1.  SurfaceFlinger 向 HWC 提供一个完整的层列表，并询问“您希望如何处理这些层？”
    2.  HWC 的响应方式是将每个层标记为叠加层或 GLES 合成。
    3.  SurfaceFlinger 会处理所有 GLES 合成，将输出缓冲区传送到 HWC，并让 HWC 处理其余部分。
4.  当屏幕上的内容没有变化时，叠加平面的效率可能会低于 GL 合成。当叠加层内容具有透明像素且叠加层混合在一起时，尤其如此。在此类情况下，HWC 可以选择为部分或全部层请求 GLES 合成，并保留合成的缓冲区。如果 SurfaceFlinger 返回来要求合成同一组缓冲区，HWC 可以继续显示先前合成的暂存缓冲区。这可以延长闲置设备的电池续航时间。
5.  运行 Android 4.4 或更高版本的设备通常支持 4 个叠加平面。尝试合成的层数多于叠加层数会导致系统对其中一些层使用 GLES 合成，这意味着应用使用的层数会对能耗和性能产生重大影响。

——– 引用自[SurfaceFlinger 和 Hardware Composer](https://source.android.google.cn/devices/graphics/arch-sf-hwc.html?authuser=0&hl=de)

我们继续接着看 SurfaceFlinger 主线程的部分，对应上面步骤中的第三步，下图可以看到 SurfaceFlinger 与 HWC 的通信部分  
[![-w1149](https://www.androidperformance.com/images/15823673746926.jpg)](https://www.androidperformance.com/images/15823673746926.jpg)

这也对应了最上面那张图的后面部分  
[![-w563](https://www.androidperformance.com/images/15823674500263.jpg)](https://www.androidperformance.com/images/15823674500263.jpg)

不过这其中的细节非常多，这里就不详细说了。至于为什么要提 HWC，因为 HWC 不仅是渲染链路上重要的一环，其性能也会影响整机的性能，[Android 中的卡顿丢帧原因概述 - 系统篇](https://www.androidperformance.com/2019/09/05/Android-Jank-Due-To-System/#3-WHC-Service-%E6%89%A7%E8%A1%8C%E8%80%97%E6%97%B6) 这篇文章里面就有列有 HWC 导致的卡顿问题（性能不足，中断信号慢等问题）

想了解更多 HWC 的知识，可以参考这篇文章[Android P 图形显示系统（一）硬件合成HWC2](https://www.jianshu.com/p/824a9ddf68b9),当然，作者的[Android P 图形显示系](https://www.jianshu.com/nb/28304383)这个系列大家可以仔细看一下

## [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#%E5%8F%82%E8%80%83%E6%96%87%E7%AB%A0 "参考文章")参考文章

1.  [Android P 图形显示系统（一）硬件合成HWC2](https://www.jianshu.com/p/824a9ddf68b9)
2.  [Android P 图形显示系统](https://www.jianshu.com/nb/28304383)
3.  [SurfaceFlinger 的定义](https://source.android.google.cn/devices/graphics/arch-sf-hwc.html?authuser=0&hl=de)
4.  [surfacefliner](https://github.com/openthos/display-analysis/blob/master/repo/android%E5%90%AF%E5%8A%A8%E5%9B%BE%E5%BD%A2%E7%95%8C%E9%9D%A2%E7%9B%B8%E5%85%B3log%E6%8A%A5%E5%91%8A/surface%E5%88%86%E6%9E%90%E6%96%87%E6%A1%A3.md)

## [](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/#%E5%85%B3%E4%BA%8E%E6%88%91-amp-amp-%E5%8D%9A%E5%AE%A2 "关于我 && 博客")关于我 && 博客

下面是个人的介绍和相关的链接，期望与同行的各位多多交流，三人行，则必有我师!

1.  [博主个人介绍](https://www.androidperformance.com/about/) ：里面有个人的微信和微信群链接。
2.  [本博客内容导航](https://androidperformance.com/2019/12/01/BlogMap/) ：个人博客内容的一个导航。
3.  [个人整理和搜集的优秀博客文章 - Android 性能优化必知必会](https://androidperformance.com/2018/05/07/Android-performance-optimization-skills-and-tools/) ：欢迎大家自荐和推荐 （微信私聊即可）
4.  [Android性能优化知识星球](https://www.androidperformance.com/2023/12/30/the-performance/) ： 欢迎加入，多谢支持～

> **一个人可以走的更快 , 一群人可以走的更远**

[![微信扫一扫](https://www.androidperformance.com/images/WechatIMG581.png)](https://www.androidperformance.com/images/WechatIMG581.png)