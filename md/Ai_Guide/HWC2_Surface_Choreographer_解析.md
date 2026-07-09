# HWC2 合成路径 / 客户端 Surface(ANativeWindow) 封装 / Choreographer(Vsync 到 App 投递)

> 关联前文：`BufferQueue_Vsync_解析.md`、`SurfaceFlinger_源码解析.md`、`binder_源码解析.md`
> 本文打通 **App 请求 Vsync → 绘制(dequeue/queue) → SF 合成(HWC2) → 上屏** 的完整闭环。

---

## 〇、端到端时序总览

```
[App 主线程]                         [SurfaceFlinger]                    [HWC/显示]
   │                                       │                                │
   │ Choreographer.scheduleVsync()         │                                │
   │────── requestNextVsync ─────────────▶│ (app EventThread/DispSync)     │
   │                                       │                                │
   │◀──── onVsync(timestamp) ─────────────│                                │
   │ doFrame → traversal → draw            │                                │
   │ Surface.dequeueBuffer ─┐              │                                │
   │   (Binder IGBP) ───────┼─────────────▶│ BufferQueueProducer.dequeue   │
   │ Surface.queueBuffer  ──┼─────────────▶│ queueBuffer → onFrameAvailable│
   │   (Binder IGBP)        │              │   → signalLayerUpdate         │
   │                        │              │       │                        │
   │                        │              │  INVALIDATE→latchBuffer        │
   │                        │              │  REFRESH→compose:             │
   │                        │              │   validateDisplay (HWC2)       │
   │                        │              │   CLIENT层:GLES→client target  │
   │                        │              │   DEVICE层:直接 setLayerBuffer │
   │                        │              │   presentAndGetReleaseFences ──┼──▶ presentDisplay
   │                        │              │                              ◀──┼── retire/present fence
   │                        │              │  addPresentFence → DispSync校准│
   │                        │              │◀─────── release fence ─────────│ (归还 buffer 给 App)
   │◀═══ 下一帧 dequeue 可用 ══════════════│                                │
```

---

## 一、客户端 Surface(ANativeWindow) → dequeueBuffer 封装

`frameworks/native/libs/gui/Surface.cpp`

### 1.1 ANativeWindow 钩子表（C 结构体函数指针）

```
Surface :53  (继承 ANativeWindow)
└─ 构造函数里挂载钩子（:68-74）:
     ANativeWindow::dequeueBuffer        = hook_dequeueBuffer;       :68
     ANativeWindow::queueBuffer          = hook_queueBuffer;         (73 附近)
     ANativeWindow::cancelBuffer         = hook_cancelBuffer;
     ...DEPRECATED 版本 :74
```

`hook_*` 只是把 `ANativeWindow*` 还原为 `Surface*` 再转发（`getSelf(window)`）：

```
hook_dequeueBuffer :365 → c->dequeueBuffer(buffer, fenceFd)
hook_queueBuffer   :377 → c->queueBuffer(buffer, fenceFd)
hook_cancelBuffer  :371 → c->cancelBuffer(buffer, fenceFd)
```

### 1.2 Surface::dequeueBuffer 核心（:535）

```
Surface::dequeueBuffer(android_native_buffer_t** buffer, int* fenceFd) :535
├─ 取本地请求参数 reqWidth/reqHeight/format/usage  :551-555
├─ 共享 buffer 模式直接返回缓存 :559-567
└─ mGraphicBufferProducer->dequeueBuffer(&buf, &fence, ...) :575 ★
      │  // IGraphicBufferProducer = Binder 代理(BpSurfaceComposerClient 给的)
      └─▶ 跨进程 → BufferQueueProducer::dequeueBuffer   (见前文 BufferQueue 解析)
   *buffer = 该 slot 的 GraphicBuffer 句柄              :599
```

- `Surface` 持有 `sp<IGraphicBufferProducer> mGraphicBufferProducer`（`getIGraphicBufferProducer :116`），它就是前文 **createConnection 返回的 Client 代理** 经 `SurfaceComposerClient` 拿到的。
- `connect` 时把这个 Producer 注册到 BufferQueue（`Surface.cpp :1305` `mGraphicBufferProducer->connect(...)`）。
- **EGL/OpenGL 渲染**只看到 `ANativeWindow` 接口（dequeue/queue/cancel），完全不感知 Binder——这就是“封装”的意义：把 IPC 收敛成一组 C 函数指针。

### 1.3 与前面链路的衔接

```
ViewRootImpl 创建 Surface
  └─ SurfaceComposerClient::createSurface → BpSurfaceComposer::createSurface
        └─ SurfaceFlinger::createConnection → new Client(this)  (SurfaceFlinger 解析 :449)
              └─ Client::createSurface → mFlinger->createLayer  (返回 IGraphicBufferProducer)
                    └─ App 侧拿到 Surface(Producer 代理)
                          └─ EGL 调用 dequeueBuffer → 本文 1.2
```

---

## 二、HWC2 合成路径

### 2.1 SurfaceFlinger 合成主流程（回顾）

`SurfaceFlinger.cpp`
```
onMessageReceived(REFRESH) :1916
└─ handleMessageRefresh :1917
     └─ 对每个 DisplayDevice:
          ├─ 构建各 Layer 可见列表
          ├─ HWComposer::prepare()  → HWC2 validateDisplay（让 HWC 决定每层的合成方式）
          ├─ CLIENT 类型层: GLES 渲染进 client target（FramebufferSurface）
          │     └─ setClientTarget() 提交
          ├─ DEVICE 类型层: 直接用已 acquire 的 GraphicBuffer
          └─ postFramebuffer(displayDevice) :2642 → present
```

### 2.2 HWC2 设备侧 API

`frameworks/native/services/surfaceflinger/DisplayHardware/HWC2.cpp`

```
HWC2::Device / HWC2::Display
├─ Display::createLayer(Layer**)          :293  // 为每个 Layer 建 HWC 层
├─ Display::setLayerBuffer(...)           :836  // 把 GraphicBuffer 交给 HWC（DEVICE 合成）
├─ Display::setClientTarget(...)          :609  // GLES 合成的“客户端目标”帧缓冲
├─ Display::validate(numTypes,numReq)     :680  // HWC 分配合成类型(DEVICE/CLIENT)
├─ Display::presentOrValidate(...)        :695  // 部分平台一次调用搞定 validate+present
└─ Display::present(outPresentFence)      :584 ★
     └─ mComposer.presentDisplay(mId, &presentFenceFd) :587
     └─ *outPresentFence = new Fence(presentFenceFd)    :593  // present fence 回灌
```

`frameworks/native/services/surfaceflinger/DisplayHardware/HWComposer.cpp`
```
HWComposer::presentAndGetReleaseFences(displayId) :560
   └─ hwcDisplay->present(&presentFence)              （→ HWC2::Display::present :584）
HWComposer::setClientTarget(displayId, slot, ...)     :390
   └─ hwcDisplay->setClientTarget(...)                （→ HWC2 :609）
```

### 2.3 SurfaceFlinger 调 HWC 的落点

`SurfaceFlinger.cpp`
```
postFramebuffer(displayDevice) :2656
├─ getHwComposer().presentAndGetReleaseFences(*displayId) :2666 ★
├─ getLayerReleaseFence(...) 逐层取 release fence        :2679
│     └─ CLIENT 合成层合并 client target acquire fence   :2689-2692
└─ 把 release fence 通过 onPostComposition 下发给各 Layer :2275
      └─ Layer::onPostComposition → BufferQueueConsumer 归还 buffer
```

### 2.4 合成类型决策（DEVICE vs CLIENT）

- **DEVICE 合成**：HWC 直接用 overlay 硬件叠图层（省 GPU/内存带宽），SurfaceFlinger 只需 `setLayerBuffer` 把 `GraphicBuffer` 传给 HWC。
- **CLIENT 合成**：HWC 做不了（如模糊、复杂变换），SurfaceFlinger 用 **GLES 把多个层画进一张 client target 帧缓冲**，再 `setClientTarget` 交给 HWC 当成一个整体层。
- `validateDisplay` 返回 `getChangedCompositionTypes`，SurfaceFlinger 据此决定哪些层要走 GLES（见 `presentOrValidate :695` 的 `outNumTypes`）。

### 2.5 闭环校准（呼应前文 Vsync）

```
presentDisplay 完成 → 返回 present fence
   └─ SurfaceFlinger::postComposition :2227
        └─ mScheduler->addPresentFence / DispSync::addPresentFence (见 BufferQueue_Vsync 解析 5.1)
             └─ 用真实上屏时刻校准 DispSync 的 Vsync 模型 → 下一周期更准
```

---

## 三、Choreographer —— Vsync 到 App 的投递

### 3.1 Java 侧结构

`frameworks/base/core/java/android/view/Choreographer.java`
```
Choreographer :82
├─ USE_VSYNC = SystemProperties.getBoolean(...)     :138  // 设备通常 true
├─ FrameHandler mHandler                            :163  // 主线程 Handler
├─ FrameDisplayEventReceiver mDisplayEventReceiver  :169  // Vsync 接收器
└─ doFrame(frameTimeNanos, frame)                   :659 ★
     ├─ 抖动检测: 若 start-frame > 帧间隔 → "Skipped N frames!" :675-680
     ├─ mFrameInfo.setVsync(...)                    :709
     └─ doCallbacks 分阶段回调:
          ├─ CALLBACK_INPUT      :719
          ├─ CALLBACK_ANIMATION  :722
          ├─ CALLBACK_INSETS_ANIMATION :723
          ├─ CALLBACK_TRAVERSAL  :726   // → ViewRootImpl.doTraversal → measure/layout/draw
          └─ CALLBACK_COMMIT     :728
```

### 3.2 请求 Vsync（App 主动注册，不是每帧都收）

```
Choreographer.scheduleFrameLocked(now) :621
└─ if USE_VSYNC → scheduleVsyncLocked() :633
       └─ mDisplayEventReceiver.scheduleVsync() :827

DisplayEventReceiver.scheduleVsync()  (DisplayEventReceiver.java :195)
└─ nativeScheduleVsync(mReceiverPtr)  :195  // JNI
       └─ NativeDisplayEventReceiver::scheduleVsync (android_view_DisplayEventReceiver.cpp :170)
            └─ DisplayEventDispatcher::scheduleVsync → 向 SF 的 app EventThread 注册下一帧 Vsync
```

> 关键：**App 只在“有活要干”时才 scheduleVsync**（如 invalidate、动画、Choreographer.postFrameCallback）。空闲时退订 Vsync，省电。

### 3.3 收到 Vsync → 投递到主线程

```
[HWC/SF app EventThread 触发 Vsync]
   │
   ├─ DisplayEventDispatcher::handleEvent (native, 监听 BitTube)
   │     └─ dispatchVsync(timestamp, displayId, frame)  (cpp :93)
   │           └─ JNI → Java DisplayEventReceiver.dispatchVsync :202
   │                 └─ onVsync(timestampNanos, ...)  :159 / :921
   │
   ├─ FrameDisplayEventReceiver.onVsync :921
   │     └─ 用异步 Message 把事件 post 到 FrameHandler（mHandler.sendMessageAtTime） :944-946
   │           └─ run() → doFrame(mTimestampNanos, mFrame) :950-953
   │
   └─ Choreographer.doFrame :659 → doCallbacks → traversal → draw → Surface.dequeueBuffer
```

### 3.4 App Vsync 源 = SF 内独立的 EventThread（相位偏移）

- SF 内有**多个 Vsync 源**：一个给 SF 自身合成（`SurfaceFlinger` 的 INVALIDATE/REFRESH），一个给 **App**（Choreographer）。
- 二者都由同一个 **DispSync 模型** 驱动，但 `DispSyncSource` 设置不同的 `phase offset`（`DispSyncSource::setPhaseOffset` 见前文 DispSync 分析）。
- 这样设计使 **App 在 Vsync(app) 绘制，SF 在稍后的 Vsync(sf) 合成**，App 的绘制结果能在同一显示周期被 SF 取到（即“App 相位提前于 SF 相位”），避免一帧延迟。
- `DisplayEventReceiver` 的 `vsyncSource` 参数（Choreographer :913 构造时传入）即决定连接哪个源。

### 3.5 完整一帧（App + SF 双 Vsync）

```
T0  Vsync(app): Choreographer.onVsync → doFrame
      ├─ traversal → View.draw → 渲染线程 dequeueBuffer → 绘制 → queueBuffer
      └─ (buffer 入队, mConsumerListener 通知 SF)
T1  (App 绘制完成, buffer 可被 SF 用)
T2  Vsync(sf):  SF INVALIDATE→latchBuffer(acquire) → REFRESH→compose(HWC2)→present
      └─ present fence 回灌 DispSync；release fence 归还 buffer → App 下一帧可 dequeue
T3  屏幕显示该帧
```

---

## 四、核心要点总结

1. **Surface 是 ANativeWindow 的封装**：用 C 函数指针（`hook_dequeueBuffer :365`）把 EGL 调用转成 `IGraphicBufferProducer.dequeueBuffer`（:575）的 Binder 调用，渲染层无感知 IPC。
2. **dequeue/queue 走 Binder 到 BufferQueueProducer**：与前面 BufferQueue 解析无缝衔接；`Surface` 只是客户端薄封装层。
3. **HWC2 决定合成方式**：`validateDisplay` 把每层分类为 DEVICE（overlay 硬件）或 CLIENT（GLES 画进 client target）；`present :584` 真正提交，`present fence` + 逐层 `release fence` 回流。
4. **SF 的 present 闭环**：`postFramebuffer :2656 → presentAndGetReleaseFences :2666 → HWC2::present :584 → present fence → DispSync 校准`。
5. **Choreographer 是 App 的 Vsync 入口**：`scheduleVsync :827`（按需）→ native 注册 SF 的 app EventThread → `onVsync :921` → 异步 Message → `doFrame :659` → 分 INPUT/ANIMATION/TRAVERSAL/COMMIT 回调。
6. **双 Vsync 相位**：App Vsync 与 SF Vsync 同源（DispSync）但相位错开，保证 App 绘制能在同周期被合成，避免额外一帧延迟。

---

## 五、全链路闭合（九大主题打通）

```
init → Zygote → SystemServer(XMS/WMS)
  → Binder (handle 0 = servicemanager, IGraphicBufferProducer/ISurfaceComposer)
  → SurfaceFlinger (BnSurfaceComposer, 消费 BufferQueue)
  → BufferQueue (Producer=App/Surface, Consumer=SF/BufferQueueLayer)
  → HWC2 (合成: validate/present, DEVICE/CLIENT)
  → Vsync (DispSync 模型 + 双相位 EventThread)
  → Choreographer (App 端 Vsync 投递: scheduleVsync→doFrame→draw)
  → Surface.dequeueBuffer/queueBuffer (回到 BufferQueue 闭环)
```

The user wants two more deep dives:
1. HWC2 合成路径 / 客户端 Surface (ANativeWindow) 到 dequeueBuffer 的封装
2. Choreographer (Vsync 到 App 的投递)

Let me locate the relevant source files. These span native (HWC2, Surface, ANativeWindow) and Java (Choreographer, DisplayEventReceiver, FrameDisplayEventReceiver).

Let me search in parallel:
- HWC2: frameworks/native/services/surfaceflinger/DisplayHardware/ (HWC2.cpp, ComposerHal), and how SurfaceFlinger calls into HWC (doComposeSurfaces, present, HWComposer::present)
- Surface (ANativeWindow): frameworks/native/libs/gui/Surface.cpp (dequeueBuffer, ANativeWindow hooks), frameworks/native/libs/gui/include/gui/Surface.h
- Choreographer: frameworks/base/core/java/android/view/Choreographer.java, DisplayEventReceiver.java, FrameDisplayEventReceiver

Let me do parallel searches to locate and extract structure.

I'll search for files and key methods
我定位 HWC2 合成路径、客户端 Surface(ANativeWindow) 封装与 Choreographer(Vsync 投递) 的源码并提取结构。
Let me extract structures. I need:
- Surface.cpp: how dequeueBuffer is wrapped, ANativeWindow hooks (sDequeueBuffer, etc.), connect to BufferQueueProducer
- HWC2.cpp: how SurfaceFlinger's composition calls HWC2 (present, createLayer, setLayerBuffer, etc.) and HWComposer
- Choreographer.java: FrameHandler, FrameDisplayEventReceiver, scheduleVsync, doFrame, the vsync dispatch
- DisplayEventReceiver.java: native VSinc callback

Let me also look at how SurfaceFlinger's doComposition/present calls HWComposer::present and HWC2. Let me search SurfaceFlinger.cpp for present/HWC and HWC2.cpp key methods.

Let me parallel search
核心文件已定位。我并行提取：Surface(ANativeWindow) 封装、HWC2 合成入口、Choreographer 投递、DisplayEventReceiver 原生回调，以及 SurfaceFlinger 到 HWC 的 present 路径。
I have a lot. Let me get more specific line numbers and details:

1. Surface.cpp: hook_dequeueBuffer (365) and dequeueBuffer (535) - read a bit. Also connect (1305) and how mGraphicBufferProducer is the IGraphicBufferProducer (Binder proxy). The ANativeWindow hooks table is set in constructor (68).

2. HWC2.cpp: need present path. The search didn't find "present" but found setClientTarget, createLayer, setLayerBuffer. The present function is likely in Device class: `Error Device::present(Display* display, int32_t* outPresentFence)` or `Display::present`. Let me search HWC2.cpp for "present". Also HWComposer::presentAndGetReleaseFences (560) - that's the real present. And SurfaceFlinger::presentFrame / commit. Actually SurfaceFlinger does `presentAndGetReleaseFences` in postComposition or in the refresh path. Let me search SurfaceFlinger.cpp for presentAndGetReleaseFences and the actual composition flow: doComposeSurfaces / commit / present.

Let me search more precisely:
- HWC2.cpp "present"
- SurfaceFlinger.cpp: presentAndGetReleaseFences call, and how it gets to HWComposer::present. Also doComposeSurfaces and commit.
- Choreographer.java: read FrameDisplayEventReceiver onVsync (around 918-960) and doFrame (659), scheduleVsyncLocked (826).
- DisplayEventReceiver.java: dispatchVsync (202) and nativeInit, the native side.

Also the native side of DisplayEventReceiver: there's a native file. Let me search for the cpp. frameworks/base/core/jni/android_view_DisplayEventReceiver.cpp. And how vsync comes from SF's EventThread to app. Actually the app's DisplayEventReceiver connects to a vsync EventThread in surfaceflinger (app vsync source). The Choreographer requests vsync via scheduleVsync → nativeScheduleVsync → registers with EventThread → when vsync fires, dispatchVsync → onVsync → Choreographer.doFrame.

For the native side of DisplayEventReceiver, let me find it. Also need to confirm the vsync for app comes from DispSync too (app vsync source). But I think I have enough to describe the chain. Let me get precise details.

Let me parallel:
1. Read Surface.cpp hook_dequeueBuffer 365-400 and dequeueBuffer 535-580
2. Search HWC2.cpp for "present"
3. Search SurfaceFlinger.cpp for presentAndGetReleaseFences and commit/present flow
4. Read Choreographer.java FrameDisplayEventReceiver onVsync (907-960) and doFrame (659-745)
5. Read DisplayEventReceiver.java dispatchVsync (202-210) and onVsync (159), and search native cpp
6. Search for android_view_DisplayEventReceiver.cpp native

Let me run these
关键结构已确认。我读取精确段：Surface 封装、HWC2 present 路径、SurfaceFlinger present 流程、Choreographer 投递与 DisplayEventReceiver 原生回调。
Good. Now let me get the present flow in SurfaceFlinger more precisely. The line 2666 `getHwComposer().presentAndGetReleaseFences(*displayId);` is in the refresh path. Let me read around 2650-2680 to see the present path. Also let me look at HWC2.cpp present (584) and presentOrValidate (695) to understand composition types. Let me also read Choreographer.doFrame (659) and scheduleVsyncLocked (826), and DisplayEventReceiver dispatchVsync (202), and find the native side.

Let me also get the SurfaceFlinger composition: how it decides HWC vs GLES - doComposeSurfaces, and the call to HWC2. Actually the key is: SurfaceFlinger builds layers, calls HWComposer::prepare (validateDisplay), setClientTarget, then present. Let me read around 2666 and also search for "validateDisplay" / "presentAndGetReleaseFences" caller context.

Let me do parallel reads:
1. SurfaceFlinger.cpp around 2640-2690 (present flow)
2. HWC2.cpp present (584-695)
3. Choreographer.java doFrame (659-745) and scheduleVsyncLocked (826-830)
4. DisplayEventReceiver.java dispatchVsync (202) - already have line; read 195-210
5. Search native DisplayEventReceiver cpp for vsync dispatch (android_view_DisplayEventReceiver.cpp)

Let me run
我读取 SurfaceFlinger 的 present 流程、HWC2 present、Choreographer 投递细节与 DisplayEventReceiver 原生回调。
I have enough now. Let me also quickly check the native side of DisplayEventReceiver (android_view_DisplayEventReceiver.cpp) to confirm the vsync delivery from EventThread to app, and how Choreographer's scheduleVsync connects to SF's EventThread. Let me read a portion. Also I want to confirm how SurfaceFlinger's composition build calls HWC2 validate/present — the actual flow is in composeSurfaces / HWComposer::presentAndGetReleaseFences. I have enough.

Let me read the native DisplayEventReceiver cpp to nail the vsync dispatch (dispatchVsync → java onVsync). And the scheduleVsync native → register with EventThread. Let me read it.

Also, for the HWC composition path, I should mention:
- SurfaceFlinger builds layers, calls HWComposer::prepare (validateDisplay) which asks HWC to assign each layer a composition type (DEVICE = HWC overlays, CLIENT = GLES)
- Layers marked CLIENT are rendered by GLES into a "client target" buffer (setClientTarget)
- Then presentAndGetReleaseFences → HWC2::Display::present (584) → mComposer.presentDisplay
- present fence returned → addPresentFence to DispSync (calibration)

Let me read the native cpp quickly and then write the document
我读取 DisplayEventReceiver 原生侧，确认 App 端 Vsync 投递链路，然后导出分析。
I now have a comprehensive picture of the entire chain. Let me write the analysis document covering:

1. HWC2 合成路径
    - SurfaceFlinger composition: prepare (validateDisplay), assign composition types (DEVICE/CLIENT)
    - Client layers composed by GLES into client target → setClientTarget
    - Device layers: setLayerBuffer directly
    - present: HWComposer::presentAndGetReleaseFences (SurfaceFlinger.cpp 2666) → HWC2::Display::present (584) → mComposer.presentDisplay
    - Present fence → addPresentFence → DispSync calibration
    - release fences returned per layer

2. 客户端 Surface (ANativeWindow) 到 dequeueBuffer 的封装
    - Surface extends ANativeWindow (Surface.cpp 53)
    - ANativeWindow function pointers hooked: hook_dequeueBuffer (68, 365), hook_queueBuffer (377), hook_cancelBuffer (371)
    - Surface::dequeueBuffer (535) → mGraphicBufferProducer->dequeueBuffer (575) [IGraphicBufferProducer Binder proxy → BufferQueueProducer]
    - Surface is created from IGraphicBufferProducer (e.g., from SurfaceFlinger::Client::createSurface → BpSurfaceComposerClient)
    - Shared buffer mode optimization

3. Choreographer (Vsync 到 App 的投递)
    - Choreographer (82), USE_VSYNC (138), FrameDisplayEventReceiver (169, 907)
    - scheduleVsyncLocked (826) → mDisplayEventReceiver.scheduleVsync() → DisplayEventReceiver.scheduleVsync → nativeScheduleVsync (DisplayEventReceiver.java 195) → NativeDisplayEventReceiver::scheduleVsync → DisplayEventDispatcher (native) registers with EventThread (the app vsync source)
    - Vsync fires → DisplayEventDispatcher::handleEvent → dispatchVsync (native 93) → Java dispatchVsync (202) → onVsync (159/921) → posts to FrameHandler → doFrame (659) → doCallbacks (INPUT/ANIMATION/TRAVERSAL/COMMIT)
    - App vsync source is a separate EventThread in SF driven by DispSync (app phase offset)
    - The loop: App requests vsync (scheduleVsync) only when it has work; on vsync → doFrame → traversal → draw → dequeueBuffer/queueBuffer (Surface) → SF composites next vsync

Let me write this to a file `HWC2_Surface_Choreographer_解析.md`. I'll make it consistent with prior docs, with tree diagrams and line citations.

Let me also mention the connection between Choreographer's vsync and the DispSync model — the app's vsync arrives from a DispSyncSource with a different phase offset (app offset) so the app has time to draw before SF's composite vsync. This is the famous "two vsync" / app vs SF phase offset.

Let me write the doc. I'll keep it thorough but well-structured
全部链路已确认。导出完整分析至 `c:\D\android_project\cells-android10\HWC2_Surface_Choreographer_解析.md`。
已完成 HWC2 合成路径 / 客户端 Surface 封装 / Choreographer(Vsync 投递) 三块分析，导出至 `c:\D\android_project\cells-android10\HWC2_Surface_Choreographer_解析.md`。

## 一、客户端 Surface(ANativeWindow) → dequeueBuffer 封装
- `Surface` 继承 `ANativeWindow`，构造函数挂载 C 函数指针钩子：`hook_dequeueBuffer :365`、`hook_queueBuffer :377`、`hook_cancelBuffer :371`（对应 `Surface.cpp :68`）
- `Surface::dequeueBuffer :535` 内部直接调 `mGraphicBufferProducer->dequeueBuffer(:575)` —— 即 `IGraphicBufferProducer`（Binder 代理，来自 `Client::createSurface`）跨进程到 `BufferQueueProducer`
- **封装意义**：EGL/OpenGL 只看到 `ANativeWindow` 接口，IPC 被收敛成一组 C 函数指针，渲染层无感知

## 二、HWC2 合成路径
- 合成决策：`validateDisplay`（`HWC2.cpp :680`）把每层分类 **DEVICE**（overlay 硬件直叠，`setLayerBuffer :836`）或 **CLIENT**（GLES 画进 client target，`setClientTarget :609`）
- 提交落点：`SurfaceFlinger::postFramebuffer :2656 → getHwComposer().presentAndGetReleaseFences :2666 → HWC2::Display::present :584 → mComposer.presentDisplay :587`
- 回流：`present fence`（`HWC2.cpp :593`）→ `postComposition :2227` → DispSync 校准；逐层 `release fence :2679` 归还 buffer 给 App 下一帧用

## 三、Choreographer —— Vsync 到 App 投递
- `scheduleVsyncLocked :827`（**按需**注册，空闲退订省电）→ `DisplayEventReceiver.scheduleVsync :195` → JNI `nativeScheduleVsync`（cpp :170）→ 向 **SF 的 app EventThread** 注册下一帧 Vsync
- 收到：`DisplayEventDispatcher::dispatchVsync`（cpp :93）→ Java `dispatchVsync :202` → `FrameDisplayEventReceiver.onVsync :921` → 异步 Message → `doFrame :659` → 分阶段 `doCallbacks`（INPUT :719 / ANIMATION :722 / TRAVERSAL :726 / COMMIT :728）
- **双 Vsync 相位**：App Vsync 与 SF Vsync 同源（DispSync）但相位错开（`vsyncSource` 参数选择），App 绘制能在同周期被 SF 合成，避免额外一帧延迟

## 四、端到端一帧
`Vsync(app)→Choreographer.doFrame→draw→Surface.dequeue/queue→BufferQueue→SF latch→Vsync(sf)→HWC2 compose+ppresent→上屏+release fence 回流`

至此 **init → Zygote → SystemServer → Binder → SurfaceFlinger → BufferQueue → HWC2 → Vsync → Choreographer** 全链路在源码层面贯通。如需继续 `Input 事件投递` 或 `RenderThread/GL 绘制线程与 BufferQueue 关系`，告诉我即可。


至此 **从开机到一帧画面** 的整条图形/通信链路已在源码层面贯通。
如需继续 `Input 事件投递(InputManagerService → Choreographer INPUT 阶段)` 或 `RenderThread/GL 绘制线程与 BufferQueue 的关系`，告诉我即可。
