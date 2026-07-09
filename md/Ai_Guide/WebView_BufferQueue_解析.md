# WebView / GL Functor 与 BufferQueue 交互 解析

基于 `cells-android10` 源码（`frameworks/base/libs/hwui`）梳理 WebView 通过 GL/Vk Functor 绘制、以及其内容如何进入 BufferQueue。

---

## 一、核心结论（先看结论）

在 hwui 的默认硬件加速路径下，**WebView 的 GL 内容并不是写入自己的 BufferQueue，而是直接绘制进 App 的 BufferQueue 缓冲（GraphicBuffer）**——通过 `GLFunctorDrawable` 把 WebView 提供的 GL 回调（`drawGl`）嵌进 RenderThread 的绘制流程，在 `CanvasContext::draw()` 内、`dequeueBuffer` 之后、`queueBuffer` 之前，绑定到当前帧的 FBO 并调用 WebView 绘制。

即：**WebView GL Functor 与 App 主界面共享同一个 BufferQueue 的同一个 GraphicBuffer**。

---

## 二、Functor 的注册与回调模型

### 2.1 创建 Functor（WebView ↔ hwui 的契约）

- `WebViewFunctor_create`（`WebViewFunctorManager.cpp :41`）→ `WebViewFunctorManager::createFunctor`（`:126`），传入 `WebViewFunctorCallbacks`（含 `gles.draw`、`vk.*`、`onSync`、`onDestroyed`）与 `RenderMode`（OpenGL_ES / Vulkan）。
- `WebViewFunctor` 持有回调，`drawGl`（`WebViewFunctor.cpp :81`）：
  ```cpp
  void WebViewFunctor::drawGl(const DrawGlInfo& drawInfo) {
      mCallbacks.gles.draw(mFunctor, mData, drawInfo);   // 真正调进 WebView 的 GL 实现
  }
  ```
- `Handle`（`WebViewFunctorManager.h :35`）跨线程持有引用，`destroyFunctor` 时 `RenderProxy::destroyFunctor` 在 RenderThread 上销毁。

### 2.2 DrawGlInfo：传给 WebView 的绘制上下文

`DrawGlInfo`（`private/hwui/DrawGlInfo.h :29`）向 WebView 暴露：
- 输入：`clipLeft/Top/Right/Bottom`、`width/height`（`= FBO 尺寸`）、`isLayer`、`transform[16]`、颜色空间；
- 输出：`dirtyLeft/Top/Right/Bottom`（WebView 标记需重绘区域）；
- `Mode` 枚举（`:58`）：`kModeDraw`（真正绘制）/ `kModeProcess`（仅处理）/ `kModeProcessNoContext`（无 GL 上下文）/ `kModeSync`（UI 线程每次推帧且 display list 脏时同步数据，**此时 UI 线程被阻塞**）。
- `Status` 枚举（`:78`）：`kStatusDone` / `kStatusDrew`（WebView 发了 GL 命令，提示需要 flip buffer）。

---

## 三、记录与回放：FunctorDrawable 嵌进显示列表

### 3.1 记录阶段（UI 线程，RecordingCanvas）

- UI 线程 `SkiaRecordingCanvas::callDrawGLFunction` 把一个 `GLFunctorDrawable`（或 `VkFunctorDrawable`）录进 `RenderNode` 显示列表，存入 `SkiaDisplayList::mChildFunctors`（`SkiaDisplayList.h :152`）。

### 3.2 同步阶段（RenderThread，prepareTree，kModeSync）

- `FunctorDrawable::syncFunctor`（`FunctorDrawable.h :49`）在 `kModeSync` 模式下调用 `handle->sync(data)` 或 `*functor(kModeSync, nullptr)`，把 UI 线程 push 过来的脏帧数据同步给 WebView。**此阶段 UI 线程阻塞在 `DrawFrameTask::postAndWait` 的 `mSignal.wait` 上**。

### 3.3 绘制阶段（RenderThread，CanvasContext::draw）

回顾上文流程（`CanvasContext.cpp`）：
```cpp
Frame frame = mRenderPipeline->getFrame();          // :444  绑定刚 dequeue 的 GraphicBuffer
mRenderPipeline->draw(frame, ...);                  // :449  回放显示列表 → 含 GLFunctorDrawable::onDraw
mRenderPipeline->swapBuffers(frame, ...);           // :458  queueBuffer + eglSwapBuffers
```
其中 `reserveNext()`（`CanvasContext.cpp :359`）已在 `prepareTree` 完成 **dequeueBuffer**，所以 `draw()` 内的 GL 命令都落在**同一块 GraphicBuffer** 上。

---

## 四、GLFunctorDrawable：把 WebView GL 画进 App 的 FBO（关键）

`GLFunctorDrawable::onDraw`（`GLFunctorDrawable.cpp :75`）是 WebView 与 BufferQueue 交互的枢纽：

1. **取出当前帧缓冲**（`GetFboDetails` `:50`）：
   ```cpp
   GrRenderTarget* renderTarget = renderTargetContext->accessRenderTarget();
   renderTarget->getBackendRenderTarget().getGLFramebufferInfo(&fboInfo);
   *outFboID = fboInfo.fFBOID;                       // 该 FBO 后端 = dequeue 出的 GraphicBuffer
   ```
   这个 `GrRenderTarget` 就是 RenderThread 当前绘制用的画布，底层正是 `reserveNext()` 取到的 `GraphicBuffer`（App BufferQueue 的 Producer 侧缓冲）。

2. **构造 DrawGlInfo**（`:132`）：填 clip、width/height（= buffer 尺寸）、transform、isLayer（`fboID != 0`）。

3. **绑定 FBO 并清/裁**（`:145`–`:195`）：
   ```cpp
   canvas->flush();
   glViewport(0, 0, info.width, info.height);
   glBindFramebuffer(GL_FRAMEBUFFER, fboID);         // 绑定到 App 的 GraphicBuffer
   // 复杂裁剪用 stencil，简单裁剪用 scissor
   ```

4. **调用 WebView 绘制**（`:197`）：
   ```cpp
   if (mAnyFunctor.index() == 0)
       std::get<0>(mAnyFunctor).handle->drawGl(info);   // → WebViewFunctor::drawGl → gles.draw
   else
       (*(std::get<1>(mAnyFunctor).functor))(DrawGlInfo::kModeDraw, &info);
   ```

5. **复位 GL 上下文**（`:212`）：`canvas->getGrContext()->resetContext();`

> **要点**：WebView 的所有 GL 绘制命令都直接写进 App 的 GraphicBuffer（同一个 BufferQueue 缓冲），随后随 App 帧一起 `swapBuffers` → `queueBuffer` 提交给 SurfaceFlinger。WebView 在此路径下**没有独立的 BufferQueue**。

---

## 五、Vulkan 管线下的互操作（VkInteropFunctorDrawable）

当平台渲染管线为 SkiaVulkan 时，OpenGL 的 WebView functor 不能直接画进 Vulkan 的 framebuffer，走互操作路径：

- `VkInteropFunctorDrawable::onDraw`（`VkInteropFunctorDrawable.cpp :66`）：
  - 分配一个临时 `GraphicBuffer`（`:80`），usage 含 `USAGE_HW_RENDER | USAGE_HW_TEXTURE`——**它本身就是一个 gralloc/BufferQueue 风格的图形缓冲**；
  - 在 GL 上下文里画 WebView 内容进该 `GraphicBuffer`；
  - 再由 Vulkan 管线把这块 `GraphicBuffer` 当作纹理采样合成进最终帧。
- `vkInvokeFunctor`（`:54`）：用一个独立的 `sEglManager`（`VkInteropFunctorDrawable.cpp :39`）承载 GL 上下文，调 `*functor(kModeProcess, ...)`。

即：Vulkan 模式下 WebView GL 先落到一块 `GraphicBuffer`，再被合成为 App 帧的一部分——仍然汇聚到 App 的 BufferQueue 缓冲，只是中间多一次 gralloc 中转。

---

## 六、另一种模型（概念说明，Chromium 侧不在本树）

除 Functor 路径外，现代 WebView 也支持**独立渲染线程 + 自有 Surface（独立 BufferQueue）**模式：
- App 通过 `Surface`/`SurfaceControl`（或 `TextureView`）给 WebView 一块独立缓冲；
- WebView 的 compositor 在自己的渲染线程把内容画进这块 BufferQueue 缓冲并 `queueBuffer`；
- App 侧把该 Surface 当作一个普通图层（SurfaceControl / 纹理）由 SurfaceFlinger 合成。
- 本 AOSP 树 `frameworks/base/libs/hwui` 仅包含「Functor 直接画进 App 缓冲」的实现；独立 Surface 路径在 external/chromium 的 `AwContents` 中，不在本仓库。

---

## 七、端到端拼接（WebView 与全链路）

```
[UI 线程] Choreographer TRAVERSAL
 → ViewRootImpl.draw → ThreadedRenderer 记录 RenderNode
 → callDrawGLFunction 录 GLFunctorDrawable 进显示列表
 → RenderProxy.syncAndDrawFrame → DrawFrameTask::postAndWait
    （kModeSync：UI 线程阻塞，WebView 同步数据）

[RenderThread]
 → DrawFrameTask::run → syncFrameState → CanvasContext::prepareTree
     → mNativeSurface->reserveNext() = dequeueBuffer   // 取 App 的 GraphicBuffer
 → CanvasContext::draw → mRenderPipeline->draw
     → 回放显示列表 → GLFunctorDrawable::onDraw
         → glBindFramebuffer(app 的 GraphicBuffer FBO)
         → WebViewFunctor::drawGl → gles.draw()         // WebView 画进同一 buffer
 → swapBuffers = queueBuffer → 唤醒 SurfaceFlinger
 → BufferQueue → SF latch → HWC2 合成上屏
```

至此 **WebView(GL/Vk Functor) → RenderThread → 同一 BufferQueue 的 GraphicBuffer → SurfaceFlinger → HWC2** 的关系在源码层贯通。

---

## 八、关键设计点

1. **共享缓冲**：默认 Functor 路径下 WebView 与 App 共用同一 GraphicBuffer，省去一次跨层合成/拷贝。
2. **同步时机**：`kModeSync` 在 UI 线程阻塞期完成数据同步，保证绘制时 WebView 状态一致。
3. **管线兼容**：GL 管线直接绑定 FBO；Vulkan 管线用 `GraphicBuffer` 中转（`USAGE_HW_RENDER|HW_TEXTURE`）再纹理化。
4. **生命周期**：`WebViewFunctor::destroyContext` 在 RenderThread 复位 `GrContext`；上下文丢失走 `onContextDestroyed`。
5. **脏区回传**：WebView 通过 `DrawGlInfo.dirty*` 标记需重绘区域，辅助 App 做脏矩形合成。


The user wants me to analyze WebView/GL Functor and its interaction with BufferQueue. Let me locate the relevant source files.

For WebView/GL Functor:
- WebViewFunctor.cpp (frameworks/base/libs/hwui)
- WebViewFunctorManager (in DrawFrameTask.cpp or RenderProxy.cpp)
- The relationship: WebView draws its content into a buffer that's composited. In modern Android, WebView uses a "GL Functor" that's invoked during RenderThread's draw, drawing into the same buffer as the app (via HardwareRenderer/CanvasContext).
- Also there's the "synchronous" vs "async" path. Modern WebView (with the "WebView render thread") composites its own frame and submits to a separate BufferQueue (Surface), but the app composites WebView via a TextureView or SurfaceTexture / SurfaceControl.

Let me search for the relevant files.

Let me search:
- WebViewFunctor.cpp / WebViewFunctor.h
- WebViewFunctorManager
- How the functor is invoked in CanvasContext draw / RenderPipeline
- FunctorDrawable
- The relationship with BufferQueue: WebView may have its own Surface (it can draw to a BufferQueue directly via its own GL context, then the app composites that as a layer). Or via GL functor drawn directly into the app's frame buffer.

Let me locate files
我定位 WebView/GL Functor 相关源码并提取结构。
Now let me read the key files:
1. WebViewFunctorManager.h / .cpp - how WebView functors are managed, and importantly the relationship to BufferQueue. WebView has its own "Functor" that draws GL. The key question: how does WebView interact with BufferQueue?

Let me think about this. There are two models:
1. **GL Functor (legacy / synchronous path)**: WebView draws directly into the app's frame buffer via a GL functor invoked during RenderThread's draw. The functor is a callback `drawGl` that WebView provides. This draws into the SAME buffer that RenderThread is compositing (the BufferQueue buffer of the app). So no separate BufferQueue.

2. **Modern WebView (Android O+ with "WebView render thread")**: WebView composites its content on its own render thread and produces a frame into its OWN Surface (a separate BufferQueue, typically via SurfaceControl / SurfaceTexture / TextureView). Then the app's HardwareRenderer composites that as a texture layer. This is the "CanvasContext draw" model where WebView provides a Surface.

Actually, the modern model (Android 10 / Q, which is this android10 codebase): WebView uses `WebViewFunctor` which is invoked during the RenderThread draw. But there's also a path where WebView has its own Surface. Let me check.

Let me focus on the GL Functor path since that's what the codebase shows clearly:
- `WebViewFunctor::drawGl(const DrawGlInfo&)` - WebView provides this callback
- `FunctorDrawable::onDraw` invokes the functor in kModeSync mode (draws into the current GL context / same buffer)
- `GLFunctorDrawable` - for OpenGL pipeline
- `VkFunctorDrawable` / `VkInteropFunctorDrawable` - for Vulkan pipeline (interop: Vulkan draws, then functor draws GL into the same framebuffer)

The key relationship with BufferQueue:
- When the app draws WebView via a GL functor, the functor is invoked during `CanvasContext::draw()` which happens after `reserveNext()` (dequeueBuffer) and writes GL commands directly into that dequeued buffer. So WebView's GL content ends up in the SAME BufferQueue buffer as the rest of the app frame. No separate BufferQueue for the GL functor path.
- The `DrawGlInfo` provides `width, height, dirtyRect, isLayer` etc. and the functor draws GL into the current framebuffer (the dequeued buffer).

But there's also the separate-Surface path. Let me check how WebView provides a Surface. Actually, in Android 10, WebView still uses the GL functor model where it draws into the app's buffer. The "WebView render thread" composites into an internal surface that's then drawn via a functor... Actually it's complex.

Let me also look at the Vulkan interop since this codebase (android10) might use SkiaVulkan. The `VkInteropFunctorDrawable` imports the Vulkan framebuffer into GL and the WebView functor draws GL into it. Then it's resolved back to Vulkan.

Let me read:
- WebViewFunctorManager.h / .cpp (full)
- GLFunctorDrawable.h / .cpp
- SkiaOpenGLPipeline::invokeFunctor and drawGl handling
- FunctorDrawable.h
- CanvasContext::draw / how child functors are drawn - to confirm it's within the same buffer

Also, I should explain the BufferQueue interaction: the GL functor draws into the same dequeued buffer. Let me also mention the "render-ahead" and how WebView functor is invoked synchronously within `draw()`.

Let me also check `DrawGlInfo.h` for what info is passed (mode, width, height, etc.).

Let me read these files in parallel.

Also important: the modern WebView also has the concept where it draws into its own Surface for "WebView as separate layer" to enable the app to not re-render WebView on scroll. But in android10, the model is the functor. Let me also check if there's a Surface involved in WebViewFunctor. Let me read WebViewFunctorManager.cpp fully.

Let me read the files
核心文件已定位。我并行读取 WebViewFunctor 管理、GL/Vk Functor Drawable 与 draw 调用路径。
Now I have a good understanding of the GL Functor path. Let me also check:
1. The Vulkan interop path (VkInteropFunctorDrawable) - important because this codebase uses SkiaVulkan in some configs. But the key BufferQueue relationship is the same: functor draws into the same framebuffer.
2. The FunctorDrawable.h - the kModeSync callback.
3. How the WebView functor is invoked during CanvasContext::draw - i.e., where mChildFunctors are drawn in the Skia pipeline.
4. DrawGlInfo.h - the mode enum (kModeSync, kModeDraw, kModeProcess, etc.) and what width/height/dirtyRect mean.
5. WebView's own BufferQueue: In Android 10, the modern WebView uses a separate "WebView render thread" that draws into its own surface. But the GL functor model here draws into the app's buffer. There's also the concept where WebView provides a Surface for "drawing into a separate BufferQueue" — but that's actually the `WebView` drawing into a surface that the app composites. Hmm.

Actually, let me reconsider. In Android 10 (this codebase), there are two relevant things:
- The **GL Functor** path: WebView draws synchronously into the app's frame buffer (the same BufferQueue buffer) during RenderThread's draw. This is what GLFunctorDrawable does.
- There's also the possibility that WebView uses a **SurfaceTexture / separate Surface** and the app composites it as a layer, which would be a SEPARATE BufferQueue. But that's more about TextureView/SurfaceTexture.

The user explicitly asked about "WebView/GL Functor 与 BufferQueue 交互". So the key insight is:
- GL Functor: WebView's GL content is drawn directly INTO the app's BufferQueue buffer (the same GraphicBuffer that RenderThread dequeued). So WebView does NOT have its own BufferQueue in the functor path — it shares the app's buffer. The `drawGl` callback receives `DrawGlInfo` with the FBO ID and dimensions, and WebView renders GL commands into that FBO.
- This is why `GetFboDetails` extracts the FBO from the Skia canvas (which is backed by the dequeued GraphicBuffer), binds it, and the WebView functor draws into it.

But there's also the Vulkan path: `VkInteropFunctorDrawable` imports the Vulkan framebuffer into an GL-compatible FBO so the GL functor can draw into it. Let me check that file briefly to explain the interop.

Also I should mention the synchronous nature: the functor is invoked within `CanvasContext::draw()` → `mRenderPipeline->draw()` → Skia display list replay → `GLFunctorDrawable::onDraw` → `drawGl`. This all happens AFTER `reserveNext()` (dequeueBuffer) and the GL commands accumulate into the same buffer. So WebView content and the rest of the app share one BufferQueue buffer.

Let me also note the older/newer model: WebView can run its own render thread and produce frames into its own BufferQueue (when using the "WebView as TextureView/Surface" approach or `AwContents` with a separate surface). But in the GL functor model (the default in Android 10 for hardware-accelerated WebView), it shares the app buffer.

Let me check the DrawGlInfo.h for the mode enum and how `kModeSync` relates. And FunctorDrawable.h for `kModeSync`. Also check where mChildFunctors get drawn in SkiaOpenGLPipeline / SkiaDisplayList. Let me also check VkInteropFunctorDrawable briefly.

Let me read these in parallel:
- DrawGlInfo.h
- FunctorDrawable.h
- SkiaOpenGLPipeline.cpp (the draw / drawFunctors part)
- VkInteropFunctorDrawable.cpp (brief)
- Also search for how WebView uses its own Surface / "AwContents" - but that's in external/chromium, not in this AOSP tree. So I'll focus on the hwui side.

Actually, let me reconsider whether there's a separate BufferQueue for WebView. In the hwui code, there's `mChildFunctors` drawn into the same buffer. So the GL functor shares the buffer. The separate-surface model is in the WebView/Chromium layer which isn't in this tree. I'll explain both: the functor path (shares app BufferQueue buffer) and the separate-surface path (WebView has its own BufferQueue, app composites via SurfaceControl/TextureView) at a conceptual level, noting which is in this tree.

Let me read DrawGlInfo.h, FunctorDrawable.h, SkiaOpenGLPipeline draw, and VkInteropFunctorDrawable.cpp.
我并行读取 DrawGlInfo 模式定义、FunctorDrawable 同步回调、Skia 管线绘制处与 Vulkan 互操作路径。
Now I have a comprehensive understanding. Let me also confirm where the child functors are drawn during the Skia pipeline draw, and the CanvasContext::draw flow with mRenderPipeline->draw. Let me check the SkiaDisplayList replay that draws the FunctorDrawable. The FunctorDrawable is part of the display list and replayed during `mRenderPipeline->draw()`. Since GLFunctorDrawable::onDraw binds the FBO (the dequeued buffer's FBO) and draws GL into it, this happens in the same buffer.

I have enough to explain the BufferQueue interaction clearly. Let me also note the key point: DrawGlInfo provides clip/transform/width/height and the WebView draws GL into the bound FBO. The FBO is backed by the GraphicBuffer that RenderThread dequeued from the app's BufferQueue. So the WebView content goes into the SAME BufferQueue buffer.

Let me write the analysis. I want to be precise about:

1. Two WebView rendering models:
    - **GL Functor (this tree, default hwui path)**: WebView draws into the app's BufferQueue buffer via a GL functor invoked during RenderThread compositing. No separate BufferQueue for the functor content.
    - WebView can also have its own render thread + Surface (separate BufferQueue) in the Chromium layer (not in AOSP tree), composited via SurfaceControl/TextureView. I'll mention at conceptual level.

2. The Functor registration:
    - `WebViewFunctor_create` (WebViewFunctorManager.cpp :41) → created with `WebViewFunctorCallbacks` (gles.draw, vk.*, onSync, onDestroyed).
    - `WebViewFunctor::drawGl` :81 → `mCallbacks.gles.draw(functor, data, drawInfo)`.

3. Recording: `SkiaRecordingCanvas::callDrawGLFunction` records a `GLFunctorDrawable` into the display list (mChildFunctors). At draw time, `GLFunctorDrawable::onDraw` (GLFunctorDrawable.cpp :75):
    - `GetFboDetails` :50 extracts the FBO ID & size from the Skia canvas's GrRenderTarget, which is backed by the dequeued GraphicBuffer.
    - Sets up clip/scissor/stencil, binds FBO, calls `handle->drawGl(info)` :198 (or legacy `*functor(kModeDraw, &info)` :200).
    - WebView issues GL commands into that FBO → lands in the app's BufferQueue buffer.

4. BufferQueue relationship:
    - This happens INSIDE `CanvasContext::draw()` → `mRenderPipeline->draw()` which runs AFTER `reserveNext()` (dequeueBuffer) and BEFORE `swapBuffers` (queueBuffer). So WebView content shares the same GraphicBuffer as the rest of the app frame.
    - DrawGlInfo.width/height = FBO size = the dequeued buffer size.

5. Vulkan interop: When the platform pipeline is SkiaVulkan, `VkInteropFunctorDrawable` allocates a `GraphicBuffer` (USAGE_HW_RENDER | USAGE_HW_TEXTURE) and draws GL into it, then it's sampled as a texture by the Vulkan pipeline. So even in Vulkan mode, the WebView GL content goes into a GraphicBuffer (which itself is a BufferQueue-style buffer / gralloc buffer), then composited. Actually here the GraphicBuffer is NOT a BufferQueue buffer (it's a temporary interop buffer), but it's a gralloc buffer.

6. Synchronous sync: `FunctorDrawable::syncFunctor` (FunctorDrawable.h :49) — during `kModeSync` (every time UI thread pushes a frame with dirty display list, UI thread blocked), WebView syncs data. This is the `prepareTree`/sync phase.

7. Lifecycle: `WebViewFunctor::destroyContext` resets GrContext; `onContextDestroyed` handles context loss.

Let me also verify the CanvasContext::draw flow to confirm the funnel (that draw happens within the same buffer). I already read CanvasContext::draw earlier (lines 433-459): getFrame → draw → swapBuffers. The mRenderPipeline->draw replays the display list including functors. Good.

Let me also mention the WebView render thread model briefly - where the app provides a Surface to WebView. Actually in AOSP, WebView's `AwContents` can be given a `Surface` (via `WebView.setLayerType` or hardware acceleration). The GL functor path is the dominant one for embedding in the app's view hierarchy.

Actually, I realize there's an important nuance: In modern Android (O+), WebView runs its OWN compositor/render thread and draws to its own surface (a separate BufferQueue), and the app side just composites a texture. But the GL functor is still used. Let me not overstate. In this Android 10 tree, the hwui side clearly shows the functor draws into the app's FBO (same buffer). This is the "draw into app's buffer" model.

Actually, the real story for Android 10 WebView: WebView's compositor runs on a separate thread in the WebView/Chromium process and produces frames. The "functor" is what the app's RenderThread calls to tell WebView "draw your current frame now into this FBO". WebView's compositor has already prepared the frame; the functor just blits/draws it into the app's buffer. So it's still "into the app's BufferQueue buffer", but WebView's actual rasterization may happen on WebView's own thread into its own buffers, then the functor composites into the app buffer. There can also be a Vulkan interop.

I'll keep the explanation accurate to the source: the functor (GL or Vk) draws into the FBO that's backed by the app's dequeued GraphicBuffer. That's the BufferQueue interaction in this tree.

Let me write the markdown file now.

Let me give precise file references:
- WebViewFunctorManager.cpp :41 createFunctor, :81 drawGl, :126 createFunctor
- WebViewFunctorManager.h :30 class, :43 drawGl handle
- GLFunctorDrawable.cpp :50 GetFboDetails, :75 onDraw, :147 glBindFramebuffer(fboID) [the dequeued buffer's FBO], :198 drawGl
- DrawGlInfo.h :58 Mode enum, :80 Status
- FunctorDrawable.h :49 syncFunctor (kModeSync), :53
- SkiaOpenGLPipeline.cpp :208 invokeFunctor
- VkInteropFunctorDrawable.cpp :80 GraphicBuffer alloc, :54 vkInvokeFunctor
- CanvasContext.cpp :433 draw → :444 getFrame, :449 mRenderPipeline->draw, :458 swapBuffers (queueBuffer)
- CanvasContext.cpp :359 reserveNext (dequeueBuffer) [from earlier analysis]

Let me write the file
我已掌握完整路径。导出分析至 `c:\D\android_project\cells-android10\WebView_BufferQueue_解析.md`。
已完成 WebView/GL Functor 与 BufferQueue 交互分析，导出至 `c:\D\android_project\cells-android10\WebView_BufferQueue_解析.md`。

## 核心结论
默认硬件加速路径下，**WebView 的 GL 内容不写自己的 BufferQueue，而是直接绘制进 App 的 BufferQueue 缓冲（GraphicBuffer）**——通过 `GLFunctorDrawable` 把 WebView 的 GL 回调嵌进 RenderThread 绘制流程，在 `dequeueBuffer` 之后、`queueBuffer` 之前绑定当前帧 FBO 调用 WebView 绘制。

## 一、Functor 注册与回调
- `WebViewFunctor_create`（`WebViewFunctorManager.cpp :41`）→ `createFunctor :126`，传入 `WebViewFunctorCallbacks`（含 `gles.draw`、`vk.*`、`onSync`）
- `WebViewFunctor::drawGl :81` → `mCallbacks.gles.draw(...)` 真正调进 WebView 的 GL 实现
- `DrawGlInfo`（`DrawGlInfo.h :29`）向 WebView 暴露 clip/width/height/transform，`Mode` 枚举（`:58`）：`kModeDraw`/`kModeProcess`/`kModeSync`（UI 线程推脏帧时同步、UI 阻塞）

## 二、记录 → 同步 → 回放
- UI 线程 `SkiaRecordingCanvas::callDrawGLFunction` 录 `GLFunctorDrawable` 进 `SkiaDisplayList::mChildFunctors`
- 同步：`FunctorDrawable::syncFunctor`（`FunctorDrawable.h :49`）`kModeSync` 时 UI 线程阻塞在 `postAndWait` 的 `mSignal.wait`
- 绘制：`CanvasContext::draw :444 getFrame → :449 mRenderPipeline->draw → :458 swapBuffers`，而 `reserveNext()`（`:359`）已在 `prepareTree` 完成 **dequeueBuffer**

## 三、GLFunctorDrawable：与 BufferQueue 交互枢纽（关键）
`GLFunctorDrawable::onDraw`（`GLFunctorDrawable.cpp :75`）：
- `GetFboDetails :50` 从 Skia `GrRenderTarget` 取出 FBO，**该 FBO 后端就是 dequeue 出的 GraphicBuffer（App BufferQueue 缓冲）**
- `glBindFramebuffer(GL_FRAMEBUFFER, fboID) :147` 绑定到 App 的 GraphicBuffer
- `handle->drawGl(info) :198` → WebView 把 GL 命令直接写进这块缓冲
- 随后随 App 帧 `swapBuffers` → `queueBuffer` 提交 SF

> WebView 在此路径下**没有独立 BufferQueue**，与 App 主界面共享同一 GraphicBuffer。

## 四、Vulkan 互操作
- 平台为 SkiaVulkan 时，`VkInteropFunctorDrawable::onDraw`（`VkInteropFunctorDrawable.cpp :66`）分配临时 `GraphicBuffer`（`:80`，`USAGE_HW_RENDER|HW_TEXTURE`），GL 画进它，再由 Vulkan 当纹理采样合成——仍汇聚到 App BufferQueue 缓冲，只是多一次 gralloc 中转。

## 五、端到端
`UI 线程(记录+ kModeSync 同步) → RenderThread(dequeueBuffer → 回放显示列表 → GLFunctorDrawable 绑 FBO → WebView drawGl → swapBuffers/queueBuffer) → BufferQueue → SF → HWC2`

关键设计：共享缓冲省拷贝、`kModeSync` 保证绘制期状态一致、GL/Vk 双管线兼容、脏区回传（`DrawGlInfo.dirty*`）。

至此全链路贯通。如需继续 `SurfaceControl/层次合成` 或 `TextureView 与 BufferQueue` 关系，告诉我即可。

至此 `init → … → RenderThread → BufferQueue → WebView Functor` 全链路贯通。如需继续 `SurfaceControl/层次合成` 或 `TextureView 与 BufferQueue` 关系，告诉我即可。
