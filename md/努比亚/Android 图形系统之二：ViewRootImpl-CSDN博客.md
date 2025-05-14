---
created: 2025-05-14T16:32:05 (UTC +08:00)
tags: [viewrootimpl]
source: https://blog.csdn.net/wudexiaoade2008/article/details/144145609
author: 成就一亿技术人!
---

# Android 图形系统之二：ViewRootImpl-CSDN博客

> ## Excerpt
> 文章浏览阅读1.4k次，点赞24次，收藏16次。ViewRootImpl 是 Android UI 系统的核心类之一，负责将 View 层级树与窗口管理器 WindowManager 联系起来。它是 Android 应用视图的根节点，与 WindowManager 结合，实现视图的绘制、事件分发、窗口更新等功能。虽然 ViewRootImpl 是一个内部类（非公开 API），但它在整个视图渲染管线中至关重要。_viewrootimpl

---
![](https://csdnimg.cn/release/blogv2/dist/pc/img/original.png)

[Winston -\_-](https://blog.csdn.net/wudexiaoade2008 "Winston -_-") ![](https://csdnimg.cn/release/blogv2/dist/pc/img/newUpTime2.png) 已于 2024-12-02 10:24:15 修改

于 2024-11-29 21:10:26 首次发布

版权声明：本文为博主原创文章，遵循 [CC 4.0 BY-SA](http://creativecommons.org/licenses/by-sa/4.0/) 版权协议，转载请附上原文出处链接和本声明。

#### **ViewRootImpl简介**

`ViewRootImpl` 是 Android UI 系统的核心类之一，负责将 `View` 层级树与窗口管理器 `WindowManager` 联系起来。它是[Android 应用](https://so.csdn.net/so/search?q=Android%20%E5%BA%94%E7%94%A8&spm=1001.2101.3001.7020)视图的根节点，与 `WindowManager` 结合，实现视图的绘制、事件分发、窗口更新等功能。虽然 `ViewRootImpl` 是一个[内部类](https://so.csdn.net/so/search?q=%E5%86%85%E9%83%A8%E7%B1%BB&spm=1001.2101.3001.7020)（非公开 API），但它在整个视图渲染管线中至关重要。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/626106fe046e4780b90a64dddc1a0e1b.jpeg#pic_center)图片参考自[Android | 理解 ViewRootImpl](https://juejin.cn/post/7077458342007799845)

#### **ViewRootImpl 的主要功能**

1.  **管理视图层级树：**
    -   ViewRootImpl 是每个窗口视图树的根节点，负责承载整个 View 层级。
    -   通过 ViewRootImpl 的实例，DecorView（应用窗口的顶层视图）被添加到窗口管理器中。
2.  **事件分发：**
    -   处理从系统窗口（如触摸屏、按键）到应用视图的输入事件。
    -   使用 dispatchInputEvent 将事件分发到具体的 View 节点。
3.  **视图的测量、布局和绘制：**
    -   调用视图树的 measure、layout 和 draw 方法。
    -   基于 Choreographer 的帧同步机制，协调视图的刷新和绘制。
4.  **与 WMS（Window Manager Service）的通信：**
    -   ViewRootImpl 通过 WindowSession 和 Surface 与 WMS 交互，管理窗口生命周期、属性等。
5.  **VSync 信号响应：**
    -   ViewRootImpl 利用 Choreographer 实现与屏幕刷新频率同步的绘制，提升绘制性能。
6.  **处理窗口焦点和输入法：**
    -   控制焦点变化以及软键盘的弹出与收起逻辑。

#### **核心组件与重要方法**

##### **核心字段**

-   **mView**: 应用窗口的顶级视图（通常是 DecorView）。
-   **mSurface**: 用于呈现窗口内容的绘制表面。
-   **mChoreographer**: 帧调度器，协调 UI 绘制与 VSync 信号。
-   **mWindowAttributes**: 窗口属性，用于配置窗口特性（例如宽高、标志）。

##### **重要方法**

以下是一些关键方法的功能和简要介绍：

1.  **setView(View view, WindowManager.LayoutParams attrs, View panelParentView)**
    -   将视图层次结构与 ViewRootImpl 绑定，并传递窗口属性。
    -   执行视图树的首次测量、布局和绘制。
2.  **performTraversals()**
    -   核心的视图遍历方法，用于处理 measure、layout 和 draw。
    -   是三大过程的入口，被 Choreographer 触发。
3.  **dispatchInputEvent(InputEvent event)**
    -   输入事件分发入口，将事件传递给视图树中的具体 View。
4.  **invalidate()**
    -   通知 ViewRootImpl 某个区域需要重新绘制。
    -   最终触发绘制流程并更新 UI。
5.  **requestLayout()**
    -   请求重新布局，最终通过 performTraversals() 完成测量和布局。
6.  **handleMessage(Message msg)**
    -   作为 Handler 的核心实现，处理异步消息（如窗口更新、绘制等）。

#### **ViewRootImpl 的生命周期**

`ViewRootImpl` 的生命周期和窗口的生命周期紧密相关，以下是其主要阶段：

1.  **初始化：**
    -   通过 WindowManager.addView() 创建 ViewRootImpl 并调用 setView() 绑定视图。
2.  **运行：**
    -   根据系统的输入、刷新信号，持续处理消息和事件。
3.  **销毁：**
    -   当窗口被移除时，ViewRootImpl 被销毁，并释放相关资源（如 Surface）。

#### **ViewRootImpl 的绘制流程**

1.  **输入事件**（如点击屏幕）或**刷新信号**（如 invalidate()）触发绘制。
2.  调用 Choreographer.postFrameCallback() 注册下一帧回调。
3.  在 VSync 信号到来后，触发 doFrame()，执行视图的 performTraversals()。
4.  按以下顺序执行：
    -   **测量（Measure）**: 计算每个视图的尺寸。
    -   **布局（Layout）**: 确定每个视图的位置。
    -   **绘制（Draw）**: 绘制视图内容到屏幕。

#### **与开发的关系**

虽然 `ViewRootImpl` 是隐藏 API，但了解其机制对于优化 UI 性能、定位渲染问题（如掉帧）非常重要。例如：

-   **分析过长的绘制时间**：使用 Systrace 或 Perfetto 分析 performTraversals() 中的耗时部分。
-   **优化输入事件处理**：确保 dispatchInputEvent 的处理逻辑高效，避免卡顿。

#### **示例：窗口添加流程**

以下为 `ViewRootImpl` 与 `WindowManager` 的关系示意代码：

```
// WindowManagerImpl 调用 addView()
public void addView(View view, ViewGroup.LayoutParams params) {
    ViewRootImpl root = new ViewRootImpl(context, display);
    root.setView(view, params, parentWindow);
}
```

当 `addView()` 被调用时，`ViewRootImpl` 负责绑定 `DecorView`，并触发绘制流程。
