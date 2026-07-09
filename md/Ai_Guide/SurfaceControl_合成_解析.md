# SurfaceControl / 层次合成 源码解析（Android 10）

本篇承接前序专题（Input 投递、RenderThread/BufferQueue、WebView/GL Functor），把
"SurfaceFlinger 如何管理 Layer 层次树、如何把各 App 的 Buffer 合成为一帧送 HWC 上屏"
完整串起来。

---

## 一、SurfaceControl（客户端）↔ Layer（服务端）

客户端每个 `Surface` 对应服务端一个 `Layer`，由 `SurfaceControl` 持有句柄：

```java
// frameworks/base/core/java/android/view/SurfaceControl.java
createSurface(...) → nativeCreate → SurfaceComposerClient::createSurface
```

```cpp
// frameworks/native/libs/gui/SurfaceControl.cpp
// SurfaceControl 只是个客户端代理，持有 Binder handle + Producer;
// 真正的 Layer 在 SurfaceFlinger 进程内由 createLayer() 创建。
```

服务端创建入口：

```cpp
// frameworks/native/services/surfaceflinger/SFlinger.cpp:4396
SurfaceFlinger::createLayer(...) {
    switch (flags & eFXSurfaceMask) {
        case eFXSurfaceBufferQueue: createBufferQueueLayer(...); break;  // 普通带 BufferQueue 的 Surface
        case eFXSurfaceBufferState: createBufferStateLayer(...); break;  // 由 Surface 自行管理 Buffer（如 ViW、task snapshot）
        case eFXSurfaceColor:       createColorLayer(...);       break;  // 纯色层（无 Buffer，仅色值）
        case eFXSurfaceContainer:   createContainerLayer(...);   break;  // 容器层（无 Buffer，仅用于建层次树）
    }
    addClientLayer(...);                      // 挂到 mCurrentState 层次树
    setTransactionFlags(eTransactionNeeded);  // 触发 handleTransactionLocked
}
```

- **BufferQueueLayer**：普通 App 窗口（`ViewRootImpl` 的 `Surface`）。自带 `BufferQueue`（`onFirstRef` 里 `BufferQueue::createBufferQueue`）。
- **BufferStateLayer**：自己管理 Buffer 的层（如 Wallpaper、task snapshot、某些 GL Surface）。
- **ColorLayer**：纯色覆盖（如 dim layer）。
- **ContainerLayer**：只用于构成 Z-order 树（如 WindowContainer 的父节点）。

> 这是整个"层次合成"的数据结构基础：**Layer 是一棵以 ContainerLayer 为内部节点、Buffer/Color 为叶子的树**。

---

## 二、层次与 Z-order：Transaction 事务

客户端通过 `SurfaceComposerClient::Transaction` 批量改属性，最后 `apply()` 一次性提交：

```cpp
// frameworks/native/libs/gui/SurfaceComposerClient.cpp
Transaction::setLayer(sc, z)            // 绝对 Z
Transaction::setRelativeLayer(sc, relTo, z) // 相对 Z（相对某层）
Transaction::reparent(sc, newParent)    // 改变父节点（层次重组）
Transaction::setPosition / setMatrix / setBuffer / setAlpha ...
```

每个属性对应 `layer_state_t::eLayerChanged` / `eRelativeLayerChanged` / `eReparent` 等标记位。
`apply()` → `SurfaceFlinger::applyTransactionState` → `setClientStateLocked` 把这些 state 写入 Layer 的 `mCurrentState`（pending），并 `setTransactionFlags(eTransactionNeeded)`。

服务端在下一个 `handleMessageRefresh` → `handleTransactionLocked` 时：
1. 遍历 `mCurrentState` 各 Layer 的 `eTransactionNeeded` 标志
2. 应用 pending state（position/alpha/z/parent/relz）到 `mCurrentState`
3. `commitTransaction()` 把 `mDrawingState` 切换为新的 `mCurrentState`

层次树因此在线程安全、原子的方式下更新——这就是"层次 compositing tree"的核心。

```cpp
// SurfaceFlinger.cpp:2715 handleTransaction → 2735 handleTransactionLocked → 3223 commitTransaction
```

`reparent` 实质是改变 Layer 的父节点（在 `mCurrentState.layersSortedByZ` 这棵树里移动）。
真正绘制时的可见顺序由 `displayDevice->getVisibleLayersSortedByZ()` 决定——它遍历这棵树并按 Z 排序。

---

## 三、App 的 Buffer 怎么进到 Layer：latch 流程

App 侧 `Surface.dequeueBuffer → queueBuffer` 之后，Binder 调用进入 `BufferQueueLayer`：

```cpp
// BufferQueueLayer.cpp:442  onFrameAvailable(BufferItem item)
//   - 把 BufferItem 压入 mQueueItems 队列
//   - mFlinger->signalLayerUpdate();  触发 SF 走 refresh 流程
```

合成前 SF 调用 `BufferLayer::latchBuffer`（BufferLayer.cpp:398）：

```cpp
bool BufferLayer::latchBuffer(...) {
    if (!fenceHasSignaled()) return false;  // acquire fence 未就绪，等下一帧
    updateTexImage(...);     // 从 BufferQueue 取已就绪的 GraphicBuffer
    updateActiveBuffer();    // 锁存为 mActiveBuffer（将要合成的 buffer）
    mRefreshPending = true;
}
```

关键：**acquire fence**。App 的 GPU 还在写这块 buffer 时，fence 没 signal；SF 必须等 fence signal 后才能 latch、合成。这是"BufferQueue + fences"实现生产者/消费者同步的核心（与前面 RenderThread 的 dequeue/queue 对应）。

---

## 四、一帧合成：handleMessageRefresh

```cpp
// SurfaceFlinger.cpp:1946  handleMessageRefresh()
preComposition();         // 逐 Layer 调 latchBuffer / onPreComposition
rebuildLayerStings();     // 重建每 Display 的可见 Layer 列表
calculateWorkingSet();
beginFrame(display);
prepareFrame(display);    // 让 HWC 先 validate，决定哪些走 DEVICE、哪些走 CLIENT
doComposition(display);   // 实际绘制
postFrame();
postComposition();
```

### prepareFrame → HWC validate（决定合成方式）

`prepareFrame` 调 `HwcComposer` 的 `validate`。HWC 根据每个 Layer 的几何/混合/secure 等
特性，决定每个 Layer 用 **DEVICE**（硬件 overlay）还是 **CLIENT**（GPU 合成）方式：

```cpp
// SurfaceFlinger.cpp:2084  若 Layer 需要 CLIENT 合成 → forceClientComposition → setCompositionType(CLIENT)
```

- **DEVICE / SIDEBAND / CURSOR**：HWC 直接合成，GPU 不参与（省电、低延迟）
- **CLIENT**：必须 GPU 画进一个临时 GraphicBuffer（"client target"），再交给 HWC

### doComposeSurfaces → GPU 画 CLIENT 层

```cpp
// SurfaceFlinger.cpp:3549  doComposeSurfaces()
if (hasClientComposition) {
    buf = display->getRenderSurface()->dequeueBuffer(&fd);  // 取一个 framebuffer 用的 GraphicBuffer
    for (auto& layer : visibleLayersByZ) {
        if (layer->getCompositionType() == CLIENT)
            layer->prepareClientLayer(renderArea, clip, ..., layerSettings);  // 收集各 CLIENT 层参数
    }
    renderEngine.drawLayers(clientCompositionDisplay, clientCompositionLayers,
                            buf->getNativeBuffer(), ...);  // GPU 一次性把所有 CLIENT 层画进 buf
}
```

`prepareClientLayer`（`Layer.cpp:517`）把 Layer 的几何（boundaries/transform）、alpha、混合、source buffer（纹理）
打包成 `renderengine::LayerSettings`。

> 这里 `buf` 就是"client target"——所有必须用 GPU 画的层，最终都被渲染进同一个 GraphicBuffer。

### FramebufferSurface：把 client target 送回 HWC

```cpp
// frameworks/native/services/surfaceflinger/DisplayHardware/FramebufferSurface.cpp:142
onFrameAvailable() → mHwc.setClientTarget(mDisplayId, slot, acquireFence, buffer, dataspace);
```

即 GPU 画好的 `buf` 被当作 HWC 的 client target 层参与最终 overlay 合成。

### present & release fence

```cpp
// SurfaceFlinger.cpp:2666  presentAndGetReleaseFences
getHwComposer().presentAndGetReleaseFences(*displayId);
// HWComposer.cpp:560  hwcDisplay->present(&lastPresentFence);  hwcDisplay->getReleaseFences(...)
```

HWC 把 DEVICE 层 + client target 合成后真正上屏；同时把各 Layer 的 **release fence** 返回。
SF 通过 `onPostComposition` 把 release fence 回传给 App 的 BufferQueue——App 拿到 release fence
后才知道那块 buffer 可以再次 dequeue/复用（三缓冲流水线的回流）。

---

## 五、端到端一帧（串起前序专题）

```
[App 主线程] View 绘制 → [RenderThread] dequeueBuffer → 画 RenderNode + WebView GL Functor 进同 buffer
        → queueBuffer → BufferQueueLayer.onFrameAvailable → signalLayerUpdate
[SF 主线程] handleMessageRefresh
        → preComposition (latchBuffer, 等 acquire fence signal)
        → prepareFrame (HWC validate: DEVICE vs CLIENT)
        → doComposeSurfaces (CLIENT 层 → GPU renderEngine.drawLayers 进 client target buffer)
        → FramebufferSurface::onFrameAvailable → hwc.setClientTarget(client target)
        → presentAndGetReleaseFences (HWC 合成上屏, 返回 release fence)
        → onPostComposition (release fence 回 App BufferQueue)
[App] 下次 dequeueBuffer 时复用该 buffer
```

层次树视角：每个 App 的 `Surface` = 一个 `Layer`，通过 `SurfaceControl::reparent/setLayer`
挂到全局 `mCurrentState` 树里；合成时 `getVisibleLayersSortedByZ()` 遍历该树，按 Z 排序逐层合成。

---

## 六、与前序专题的衔接点

| 专题 | 关键衔接 |
|------|----------|
| Input 投递 | Choreographer TRAVERSAL 触发 App 绘制 → 本篇 App `queueBuffer` 起点 |
| RenderThread/BufferQueue | `dequeueBuffer`/`queueBuffer`/fence 即本篇 latch 与 release 的基础 |
| WebView GL Functor | WebView 内容画进 App 共享 buffer → 在本篇作为该 Layer 的 `mActiveBuffer` 被 GPU 合成 |
| SurfaceControl/合成（本篇） | 消费上述所有 buffer，经 HWC 合成上屏 |

---

## 七、关键源码索引

- `SurfaceFlinger.cpp:4396` `createLayer` — 四种 Layer 类型
- `SurfaceFlinger.cpp:4514` `createBufferQueueLayer`
- `SurfaceControl.cpp` / `SurfaceComposerClient.cpp:609,812` — setLayer / reparent 事务
- `SurfaceFlinger.cpp:3942` `applyTransactionState` → `setClientStateLocked` → `handleTransactionLocked:3004` → `commitTransaction:3223`
- `BufferQueueLayer.cpp:442` `onFrameAvailable` / `BufferLayer.cpp:398` `latchBuffer`
- `SurfaceFlinger.cpp:3549` `doComposeSurfaces` → `renderEngine.drawLayers`
- `FramebufferSurface.cpp:142` `setClientTarget`
- `SurfaceFlinger.cpp:2666` `presentAndGetReleaseFences` → `HWComposer.cpp:560` `present`
