---
created: 2025-05-14T16:36:30 (UTC +08:00)
tags: [surfacecontrol]
source: https://blog.csdn.net/wudexiaoade2008/article/details/144145921
author: 成就一亿技术人!
---

# Android 图形系统之三：SurfaceControl-CSDN博客

> ## Excerpt
> 文章浏览阅读2.3k次，点赞16次，收藏23次。在 Android 系统中，SurfaceControl 是一个关键的类，用于管理应用窗口和屏幕上的显示内容。它与 SurfaceFlinger 紧密交互，通过 BufferQueue 提供高效的图形缓冲区管理能力。_surfacecontrol

---
在 [Android](https://so.csdn.net/so/search?q=Android&spm=1001.2101.3001.7020) 系统中，`SurfaceControl` 是一个关键的类，用于管理应用窗口和屏幕上的显示内容。它与 SurfaceFlinger 紧密交互，通过 BufferQueue 提供[高效的](https://so.csdn.net/so/search?q=%E9%AB%98%E6%95%88%E7%9A%84&spm=1001.2101.3001.7020)图形缓冲区管理能力。`SurfaceControl` 是 Android 的显示架构中不可或缺的部分，主要作用包括：

1.  **创建和控制显示表面 (Surface)：**  
    应用可以通过它创建和管理显示内容的基础表面。
2.  **管理子层级关系 (Layer Hierarchy)：**  
    用于定义表面之间的层级关系（父子关系）。
3.  **动画和变换 (Transformations)：**  
    提供旋转、缩放、平移等操作以控制表面的位置和形状。
4.  **直接和 SurfaceFlinger 交互：**  
    通过 `Binder` 调用与系统的 SurfaceFlinger 服务通信。

以下将结合源码，从关键方法、实现机制以及它在[图形渲染](https://so.csdn.net/so/search?q=%E5%9B%BE%E5%BD%A2%E6%B8%B2%E6%9F%93&spm=1001.2101.3001.7020)系统中的角色等方面详细解析。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/d51a603622ba4dec810c230e79ce355b.png#pic_center)  
图片参考自[Android的UI显示原理之Surface的创建](https://juejin.cn/post/6844903761292886030)

#### SurfaceControl 的关键方法

##### 1\. **创建 SurfaceControl 对象**

-   **Java 层接口：**

```
SurfaceControl.Builder builder = new SurfaceControl.Builder();
SurfaceControl surfaceControl = builder
        .setName("MySurface")
        .setBufferSize(1080, 1920)
        .build();
```

-   **关键代码：**  
    SurfaceControl 的构造函数通过 JNI 与底层 native 层的 `android::SurfaceComposerClient` 交互。

```
sp SurfaceComposerClient::createSurface(
    const String8& name, uint32_t width, uint32_t height, PixelFormat format, uint32_t flags) {
    // 向 SurfaceFlinger 请求创建 SurfaceControl
    return SurfaceFlinger::createSurface(name, width, height, format, flags);
}
```

##### 2\. **绑定到 Surface**

-   SurfaceControl 创建的表面可以与 Surface 绑定，用于绘制图形内容。

```
Surface surface = new Surface(surfaceControl);
Canvas canvas = surface.lockCanvas(null);
canvas.drawColor(Color.RED);
surface.unlockCanvasAndPost(canvas);
```

##### 3\. **更新 Surface 属性**

通过事务 (`SurfaceControl.Transaction`) 修改表面参数。

```
SurfaceControl.Transaction transaction = new SurfaceControl.Transaction();
transaction.setPosition(surfaceControl, 100, 200);
transaction.setLayer(surfaceControl, 5);
transaction.apply();
```

-   **对应 native 层：**  
    setPosition 等操作最终会通过 android::Transaction 被序列化，并传递给 SurfaceFlinger。

#### 底层实现解析

##### 1\. **SurfaceControl 的核心数据结构**

在 native 层，`SurfaceControl` 是 `sp<SurfaceControl>` 类型的一个智能指针，主要管理一个 Layer（层）的生命周期。

```
class SurfaceControl {
    sp mHandle; // SurfaceFlinger 服务的句柄
    sp mProducer; // 对应 BufferQueue 的生产者端
};
```

-   mHandle：是通过 Binder 与 SurfaceFlinger 通信的关键。
-   mProducer：对应图形缓冲区生产者，与 BufferQueue 相连。

##### 2\. **Layer 和 SurfaceFlinger 的交互**

每个 `SurfaceControl` 对应一个 Layer，所有 Layer 在 SurfaceFlinger 中维护。

```
status_t SurfaceFlinger::createLayer(const sp& client, const String8& name,
                                     uint32_t w, uint32_t h, uint32_t flags,
                                     sp* handle,
                                     sp* gbp) {
    // 创建 Layer 并初始化 BufferQueue
    sp layer = new Layer(...);
    *handle = layer->getHandle();
    *gbp = layer->getBufferQueue();
}
```

##### 3\. **事务提交**

`SurfaceControl.Transaction` 在 native 层通过 `android::Transaction` 表示。

```
status_t SurfaceFlinger::setTransactionState(
    const Vector& state, const Vector& displays, uint32_t flags) {
    // 解析事务操作并更新 Layer 树
    for (const ComposerState& composerState : state) {
        applyState(composerState);
    }
}
```

SurfaceFlinger 将事务中的操作应用到 Layer 树，并在下一帧提交渲染。

#### SurfaceControl 的使用场景

1.  **应用窗口渲染：**  
    `SurfaceControl` 是 Android View 系统渲染机制的核心，`WindowManager` 通过它管理窗口。
2.  **硬件加速和视频播放：**  
    视频播放器如 ExoPlayer，使用 SurfaceControl 提供的 Surface 绘制视频帧。
3.  **多窗口和手势导航：**  
    Android 的多窗口模式，以及系统手势的动画都依赖于它。

#### SurfaceControl 的性能优势

-   **高效缓冲区管理：**  
    通过 `BufferQueue` 提供生产者和消费者分离的模型，支持异步渲染和显示。
-   **分层架构：**  
    Layer 树的分层设计支持复杂的动画效果和变换。
-   **GPU 和硬件优化：**  
    SurfaceFlinger 直接调用 OpenGL 或 Vulkan，实现硬件加速。

#### 总结

`SurfaceControl` 是 Android 图形渲染体系的核心部分，其设计贯穿从应用层到硬件层的每一个细节。它抽象了图形缓冲区管理和 Layer 树操作，通过 SurfaceFlinger 实现高效的图形显示管理。通过理解 `SurfaceControl` 的源码和底层架构，可以深入掌握 Android 图形系统的工作原理，有助于优化 UI 性能和开发复杂动画效果。
