---
created: 2025-05-14T16:37:00 (UTC +08:00)
tags: [android choreographer]
source: https://blog.csdn.net/wudexiaoade2008/article/details/144146156
author: 成就一亿技术人!
---

# Android 图形系统之四：Choreographer_android choreographer-CSDN博客

> ## Excerpt
> 文章浏览阅读1.5k次，点赞27次，收藏10次。Choreographer 是 Android 系统中负责帧同步的核心组件，它协调输入事件、动画和绘制任务，以确保界面以固定频率（通常是每 16ms，一帧）流畅渲染。通过管理 VSYNC 信号和调度任务，Choreographer 是实现流畅 UI 体验和高效资源利用的关键。_android choreographer

---
`Choreographer` 是 [Android](https://so.csdn.net/so/search?q=Android&spm=1001.2101.3001.7020) 系统中负责帧同步的核心组件，它协调输入事件、动画和绘制任务，以确保界面以固定频率（通常是每 16ms，一帧）流畅渲染。通过管理 VSYNC 信号和调度任务，`Choreographer` 是实现流畅 UI 体验和高效资源利用的关键。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/74f9186cd6e24ba1bcb2cb6777b1c7cd.png#pic_center)  
图片参考自[UI Performance Rendering](https://think-it.io/insights/Android-UI-Performance-rendering)

以下是系统性的介绍，结合了作用机制、源码解析，以及典型应用场景。

### **Choreographer 的作用**

1.  **帧同步管理** `Choreographer` 是 UI 渲染任务的中央调度器，负责以帧为单位同步动画和绘制任务，确保它们在 VSYNC 信号到达时运行。
2.  **协调输入、动画和绘制** 它按照固定顺序依次处理输入事件、动画逻辑和界面更新，优化任务间的节奏，防止任务冲突或不必要的渲染。
3.  **减少资源浪费** 通过将任务与屏幕刷新（VSYNC）同步，避免了无效的重复绘制，节省了 CPU 和 GPU 的资源。

### **Choreographer 的工作机制**

1.  **VSYNC 信号监听** 系统底层通过 `FrameDisplayEventReceiver` 捕获 VSYNC 信号，并通知 `Choreographer`。
2.  **回调机制** 提供 `postFrameCallback` 方法，允许开发者将任务加入帧调度队列，任务会在下一帧按需执行。
3.  **帧的分阶段处理** 一帧通常分为以下阶段：
    -   **Input（输入处理）**：分发触摸、键盘等输入事件。
    -   **Animation（动画更新）**：执行动画计算和逻辑。
    -   **Traversal（界面遍历）**：触发视图的测量、布局和绘制。
4.  **线程绑定** 每个线程有一个独立的 `Choreographer` 实例，通常主线程上的 `Choreographer` 是 UI 渲染的核心。

### **Choreographer 源码解析**

以下是 `Choreographer` 的核心代码和机制分析。

#### **1\. 初始化**

`Choreographer` 的构造方法如下：

```
private Choreographer(Looper looper, int vsyncSource) {
    mLooper = looper;
    mHandler = new FrameHandler(looper);
    mDisplayEventReceiver = new FrameDisplayEventReceiver(looper, vsyncSource);
    mCallbackQueues = new CallbackQueue[CALLBACK_LAST + 1];
    for (int i = 0; i <= CALLBACK_LAST; i++) {
        mCallbackQueues[i] = new CallbackQueue();
    }
}
```

##### **分析**

-   mHandler：基于传入的 Looper 创建，用于任务调度。
-   mDisplayEventReceiver：监听 VSYNC 信号，触发帧更新。
-   mCallbackQueues：维护不同类型的回调队列，如输入、动画和绘制任务。

#### **2\. 注册帧回调**

开发者可以通过 `postFrameCallback` 方法注册下一帧需要执行的任务：

```
public void postFrameCallback(FrameCallback callback) {
    postFrameCallbackDelayed(callback, 0);
}

public void postFrameCallbackDelayed(FrameCallback callback, long delayMillis) {
    long now = SystemClock.uptimeMillis();
    long dueTime = now + delayMillis;
    mCallbackQueues[CALLBACK_ANIMATION].addCallbackLocked(dueTime, callback, null);
    scheduleFrameLocked(now);
}
```

##### **分析**

-   mCallbackQueues 将任务加入 CALLBACK\_ANIMATION 队列。
-   scheduleFrameLocked 检查是否需要安排新的帧。

#### **3\. VSYNC 信号处理**

VSYNC 信号通过 `FrameDisplayEventReceiver` 捕获，触发帧调度：

```
@Override
public void onVsync(long timestampNanos, int builtInDisplayId, int frame) {
    Message msg = Message.obtain(mHandler, this::doFrame, timestampNanos);
    msg.setAsynchronous(true);
    mHandler.sendMessageAtTime(msg, timestampNanos / 1000000);
}
```

##### **分析**

-   onVsync 将信号包装成异步消息，通过 Handler 提交到主线程。
-   消息最终调用 doFrame，启动任务回调。

#### **4\. 帧的处理（doFrame）**

`doFrame` 方法负责执行帧内的所有任务回调：

```
void doFrame(long frameTimeNanos) {
    mFrameScheduled = false;

    doCallbacks(CALLBACK_INPUT, frameTimeNanos);
    doCallbacks(CALLBACK_ANIMATION, frameTimeNanos);
    doCallbacks(CALLBACK_TRAVERSAL, frameTimeNanos);
}
```

##### **分析**

-   doCallbacks 按顺序执行输入、动画、布局绘制任务。
-   每帧回调在帧时间戳（frameTimeNanos）下运行，确保与屏幕刷新同步。

### **Choreographer 与其他组件的协作**

1.  **InputEventReceiver** 负责捕获触摸和键盘事件，将输入事件调度到 `Choreographer` 的 `CALLBACK_INPUT`。
2.  **ViewRootImpl** 核心视图管理类，依赖 `Choreographer` 触发测量、布局和绘制阶段。
3.  **动画系统（ValueAnimator/动画框架）** 动画更新依赖 `CALLBACK_ANIMATION`，确保在 VSYNC 同步时平滑执行。

### **Choreographer 的应用场景**

1.  **实现自定义动画** 开发者可以通过 `postFrameCallback` 在下一帧执行自定义动画逻辑，保持与系统的渲染节奏一致。

```
choreographer.postFrameCallback(frameTimeNanos -> {
    // 自定义动画逻辑
    choreographer.postFrameCallback(this);
});
```

2.  **性能优化**

-   使用工具（如 Perfetto）分析帧间隔，定位卡顿原因。
-   避免阻塞 CALLBACK\_TRAVERSAL 队列，提高帧渲染效率。

3.  **任务分阶段调度** 在不同阶段安排任务，确保关键操作在合适的时机执行。

### **总结**

`Choreographer` 是 Android UI 渲染的核心，通过监听 VSYNC 信号和分阶段[调度任务](https://so.csdn.net/so/search?q=%E8%B0%83%E5%BA%A6%E4%BB%BB%E5%8A%A1&spm=1001.2101.3001.7020)，它能够高效管理输入事件、动画和绘制任务，保证帧同步和流畅的用户体验。深入理解其原理和实现，可以帮助开发者优化 UI 性能，设计更高效、更流畅的应用。
