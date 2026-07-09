# BufferQueue 生产者/消费者模型 + Vsync 帧节奏衔接（Android 10 源码解析）

> 关联前文：`SurfaceFlinger_源码解析.md`（Binder 消费方）、`binder_buffer_AIDL_解析.md`、`binder_源码解析.md`
> 本文打通 **App 绘图 → BufferQueue → SurfaceFlinger 合成 → Vsync 节奏 → 上屏** 的最后一段。

---

## 〇、全局定位

```
图形数据流（每个 Window/层 一条 BufferQueue）
┌──────────┐  dequeue/queue   ┌──────────────────┐   onFrameAvailable    ┌────────────────┐
│  Producer │ ───────────────▶│   BufferQueue     │ ────────────────────▶│  SurfaceFlinger │
│ (App/EGL) │ ◀───────────────│ (Core+Prod+Cons)  │ ◀──── acquire/release─│ (Consumer 侧)   │
└──────────┘   request/acquire └──────────────────┘                      └───────┬────────┘
                                                                                 │ Vsync 节拍
                                                                          ┌──────▼───────┐
                                                                          │  HWComposer   │
                                                                          │  (present)    │
                                                                          └──────┬───────┘
                                                                   retire fence ─┘
                                                                     （回灌 DispSync 校准）
```

`BufferQueue` = `BufferQueueCore`（共享状态）+ `BufferQueueProducer`（IGraphicBufferProducer，Binder 暴露给 App）+ `BufferQueueConsumer`（IGraphicBufferConsumer，Binder 暴露给 SF）。三者**跨进程**通过 Binder 调用（见前文 binder 分析）。

---

## 一、BufferQueueCore：共享状态根系

`frameworks/native/libs/gui/include/gui/BufferQueueCore.h`

```
BufferQueueCore :57
├── BufferQueueDefs::SlotsType mSlots;          :204 // 64 个 BufferSlot 槽位（NUM_BUFFER_SLOTS）
├── Fifo mQueue;                                :207 // 同步模式下已入队待消费的 FIFO
├── std::set<int>  mFreeSlots;                  :211 // 槽空闲且无 buffer
├── std::list<int> mFreeBuffers;                :215 // 槽空闲且已挂 buffer（可直接复用）
├── std::set<int>  mActiveBuffers;              :222 // 挂有非 FREE buffer 的槽
├── std::condition_variable mDequeueCondition;  :226 // dequeueBuffer 阻塞唤醒
├── sp<IConsumerListener> mConsumerListener;    :170 // 入队时回调消费方（SF）
├── mIsAbandoned / mConnectedApi / mConsumerUsageBits ...
└── mSharedBufferMode / mSharedBufferSlot       :320 :330 // 共享 buffer 模式
```

**关键不变量**：Producer 与 Consumer 通过 `mSlots` 槽位数组传递 buffer 所有权，**不把 GraphicBuffer 句柄走 Binder**，只传 `slot` 索引（句柄在 `requestBuffer` 时一次性映射，后续靠 slot 复用）——这是 BufferQueue 省 Binder 拷贝的根因之一。

---

## 二、BufferState 状态机（核心）

`frameworks/native/libs/gui/include/gui/BufferSlot.h :33`

```
BufferState 用 4 个计数器表示（:43-46）:
  mDequeueCount / mQueueCount / mAcquireCount / mShared

状态真值表（:48-56）:
  FREE    : mShared=false, deq=0, que=0, acq=0   → 可被 Producer dequeue
  DEQUEUED: deq=1                                  → Producer 持有，可写
  QUEUED  : que=1                                  → Producer 填完，待消费
  ACQUIRED: acq=1                                  → Consumer 持有，可读
  SHARED  : mShared=true（可与上述任意叠加）

状态转移方法（:113-171）:
  dequeue()      : deq++                          （dequeueBuffer）
  queue()        : deq--, que++                   （queueBuffer）
  acquire()      : que--, acq++                   （acquireBuffer）
  release()      : acq--                          （releaseBuffer）
  cancel()       : deq--                          （cancelBuffer）
  freeQueued()   : que--                          （异步丢弃队头）
  attach/detach* : 跨进程 attach 时增减计数
```

> 设计要点：用**引用计数**而非枚举，使同一 buffer 可被多个消费者 `acquire` 多次（如屏幕 + 录屏），`isFree()=!isAcquired&&!isDequeued&&!isQueued`（:87）。

---

## 三、Producer 侧（App / EGL 端）

`frameworks/native/libs/gui/BufferQueueProducer.cpp`

```
BufferQueueProducer
├── dequeueBuffer(int* outSlot, ...)       :356 ★
│     └─ waitForFreeSlotThenRelock(Dequeue) :415  // 阻塞等 FREE 槽
│     └─ mSlots[found].mBufferState.dequeue() :464
│     └─ 若需重分配 → 置 BUFFER_NEEDS_REALLOCATION :478
├── requestBuffer(slot, *buf)              :67
│     └─ *buf = mSlots[slot].mGraphicBuffer :93  // 把 GraphicBuffer 句柄给 Producer
├── queueBuffer(slot, ...)                 :763 ★
│     └─ mSlots[slot].mBufferState.queue() :864
│     └─ ++mCore->mFrameCounter; item.mFrameNumber :868-870
│     └─ mCore->mQueue.push_back(item)     :912  // 入 FIFO
│     └─ frameAvailableListener = mCore->mConsumerListener :913  // 记录回调
│     └─ 异步/可丢弃时覆盖队尾旧 buffer :919-937（丢帧逻辑）
│     └─ mCore->mDequeueCondition.notify_all() :958  // 唤醒等待 dequeue 的 Producer
└── cancelBuffer / attachBuffer / allocateBuffers ...
```

**Producer 完整周期**：`dequeueBuffer`（拿槽+句柄）→ CPU/GPU 渲染写 buffer → `queueBuffer`（填完入队，触发 `mConsumerListener->onFrameAvailable`）。

---

## 四、Consumer 侧（SurfaceFlinger 端）

`frameworks/native/libs/gui/BufferQueueConsumer.cpp`

```
BufferQueueConsumer
├── acquireBuffer(BufferItem* out, expectedPresent, maxFrame) :54 ★
│     └─ 超限检查 mMaxAcquiredBufferCount+1 :73
│     └─ 队列空 → 返回 NO_BUFFER_AVAILABLE :85-87
│     └─ 丢帧：mQueue.size()>1 且时间戳早 → freeQueued 丢队头 :115-177
│     └─ expectedPresent 未到 → PRESENT_LATER（延迟获取） :181-195
│     └─ mSlots[slot].mBufferState.acquire() :251
│     └─ mCore->mQueue.erase(front) :263
├── releaseBuffer(slot, frameNumber, ...)   :411 ★
│     └─ mSlots[slot].mBufferState.release() :448
│     └─ 非 shared → mActiveBuffers.erase(slot) :458
└── connect / attachBuffer / discardBuffer ...
```

**Consumer 完整周期**：`acquireBuffer`（按 Vsync present 时间取最合适的帧）→ 合成读取 → `releaseBuffer`（用完归还，唤醒 Producer 可再次 dequeue）。

### 4.1 SurfaceFlinger 如何消费（桥接层）

```
BufferQueueLayer（SurfaceFlinger 中每个 Layer 的对象）
├── onFrameAvailable(item)                  :442 ★
│     └─ mQueueItems.push_back(item)        :467  // SF 自有队列
│     └─ mFlinger->signalLayerUpdate()      :483  // → 发 INVALIDATE 消息
│     └─ mConsumer->onBufferAvailable(item) :485  // BufferLayerConsumer 缓存 Image
├── latchBuffer(...)                        // 由 handleMessageInvalidate 调用
└── onFrameReplaced(...)                    :488

BufferLayerConsumer（封装 acquireBuffer）
├── onBufferAvailable(item)                 :490  // 预建 Image（GPU 纹理）
├── acquireBufferLocked(...)                :210  // → ConsumerBase::acquireBufferLocked
└── updateAndReleaseLocked(item, ...)       :230  // 切换 mCurrentTexture，release 旧 buffer
```

`SurfaceFlinger.cpp` 取帧入口：
```
SurfaceFlinger::onMessageReceived(what)      :1813
├── case INVALIDATE :1819
│     └─ handleMessageInvalidate()          :1897 → 各 Layer latchBuffer → acquireBufferLocked
└── case REFRESH   :1916
      └─ handleMessageRefresh()             :1917 → 合成上屏（doComposition）
```

---

## 五、Vsync 帧节奏衔接

### 5.1 DispSync：硬件 Vsync 的软件模型

`frameworks/native/services/surfaceflinger/Scheduler/DispSync.h :33` / `:89`

```
DispSync（impl）
├── 维护“硬件 Vsync 周期模型”
├── addPresentFence(fence)                  :50 :108  // 用 retire fence 校准模型
├── beginResync / addResyncSample / endResync :121  // 开机/漂移时重新对齐
├── setPeriod(period)                       :54      // 刷新率切换（60↔90Hz）
├── computeNextRefresh / expectedPresentTime :61 :63
└── mPresentFences[NUM_PRESENT_SAMPLES]     :252    // 采样窗口
```
> DispSync 不是直接收 HW 中断，而是用**历史 present fence 时间戳**拟合出稳定的 Vsync 周期（抗抖动）。

### 5.2 DispSyncSource → EventThread → MessageQueue

```
HWComposer Vsync 中断
   └─ DispSync（模型）──按时回调──▶ DispSyncSource::onDispSyncEvent :113
                                      └─ mCallback->onVSyncEvent(when)   (DispSyncSource.cpp)
   └─ EventThread（按 phase offset 分发）
        └─ Connection → MessageQueue（SurfaceFlinger 主线程消息队列）
             └─ SurfaceFlinger::onMessageReceived(MessageQueue::INVALIDATE/REFRESH) :1813
```

- `DispSyncSource::setVSyncEnabled(bool)` `:43` —— 控制是否向 SF 派发 Vsync（省电时关）。
- `SurfaceFlinger::setVsyncEnabledInHWC` `:4650` —— 真正开关 HWComposer 的 Vsync 中断。
- `Scheduler::setVsyncPeriod(period)` `Scheduler.cpp :291` —— 刷新率变化时同步 DispSync 周期。

### 5.3 一帧的完整节奏（Vsync 驱动）

```
[Vsync 到来]
   │
   ├─▶ INVALIDATE 消息 :1819
   │     ├─ populateExpectedPresentTime()  :1823   // 本帧统一的“期望上屏时刻”
   │     ├─ handleMessageTransaction()     :1894   // 应用窗口属性变更
   │     └─ handleMessageInvalidate()      :1897   // 各 Layer latchBuffer → acquireBuffer
   │                                              //   （按 expectedPresent 选帧，见 4.1）
   │           → 若拿到新 buffer，signalRefresh() :1911
   │
   ├─▶ REFRESH 消息 :1916
   │     └─ handleMessageRefresh()         :1917   // 合成（HWC/GPU）→ doComposition
   │           → postComposition → present 到屏幕
   │
   └─▶ 屏幕显示 + retire fence 回灌
         └─ DispSync::addPresentFence()             // 校准下一周期模型（闭环）
```

**App 侧的同步（三缓冲）**：App 在 `dequeueBuffer` 若所有 buffer 都被 SF 持有（acquired/queued），会阻塞在 `mDequeueCondition`（:226, :958），直到 SF `releaseBuffer` 归还——这就形成 **dequeue → render → queue → 等待 → dequeue** 的生产消费节拍，与 Vsync 对齐即“每 Vsync 一帧”。

---

## 六、核心要点总结

1. **所有权用 slot 传递，不走 Binder 拷贝**：`mSlots[64]` + `mFreeBuffers/mActiveBuffers/mQueue`（`BufferQueueCore.h :204-222`）管理 buffer 在 Prod/Cons 间流转；`GraphicBuffer` 句柄只在 `requestBuffer :67` 映射一次。
2. **状态机是计数而非枚举**（`BufferSlot.h :33`）：支持多消费者、共享 buffer；`dequeue/queue/acquire/release` 四个计数器精确描述生命周期。
3. **异步丢帧在两端都有**：Producer `queueBuffer :919` 覆盖可丢弃旧帧；Consumer `acquireBuffer :115` 按 `expectedPresent` 丢过期帧，保证低延迟。
4. **SF 是 Consumer 但不是直接 acquire**：`BufferQueueLayer::onFrameAvailable :442` 把帧存入 `mQueueItems`，等下一次 `INVALIDATE` 才 `latchBuffer`，使合成与 Vsync 对齐。
5. **Vsync 来自 DispSync 模型而非裸中断**：用 `addPresentFence :108` 的 present fence 持续校准，`setVsyncPeriod :291` 支撑可变刷新率。
6. **闭环自校准**：`present → retire fence → DispSync 校准 → 下一 Vsync 派发`，形成稳定的帧节奏。

---

## 七、与前文的链路闭合

```
App 渲染线程
  └─ EGL/ANativeWindow → BufferQueueProducer::dequeueBuffer/queueBuffer   (本文)
        └─ Binder(IGraphicBufferProducer) 跨进程                          (binder 分析)
              └─ SurfaceFlinger::BufferQueueLayer::onFrameAvailable :442  (SurfaceFlinger 分析)
                    └─ signalLayerUpdate → INVALIDATE/REFRESH :1813
                          └─ latchBuffer(acquireBuffer) → 合成 → present
                                └─ retire fence → DispSync::addPresentFence (本文 Vsync)
                                      └─ 模型校准 → 下一次 Vsync（闭环）
```


The user wants me to continue with either BufferQueue producer/consumer model or Vsync frame pacing. Let me pick BufferQueue since it's the natural next step after SurfaceFlinger, and it's a core piece. Actually, let me do both in one analysis since they're related, but focus primarily. Let me locate the source files.

Let me search for BufferQueue, BufferQueueCore, BufferQueueProducer, BufferQueueConsumer, and Vsync related (DispSync, VSyncDispatch, scheduler).

Let me be efficient and parallel search
我定位 BufferQueue 生产者/消费者模型与 Vsync 帧节奏相关源码并提取结构。
Good. Let me extract the key structures. BufferQueueCore is the central. Let me look at BufferQueueCore.h for the core data structures (slots, state, etc.), BufferQueueProducer key methods (dequeueBuffer, queueBuffer), BufferQueueConsumer (acquireBuffer, releaseBuffer), and Vsync (DispSync, Scheduler).

Let me parallel search
BufferQueue 与 Vsync 源码已定位。我并行提取核心结构：BufferQueueCore 的数据结构、Producer/Consumer 关键方法、Vsync 衔接。
I have good data. Let me read key sections to get accurate line numbers for the analysis:
- BufferQueueCore.h for BufferSlot/BufferState enum and mSlots
- BufferQueueProducer dequeueBuffer (356) and queueBuffer (763) key logic
- BufferQueueConsumer acquireBuffer (54) and releaseBuffer (411)
- DispSync.h for the model
- Scheduler.h for frame scheduling

Let me also look at the BufferState enum (FREE, DEQUEUED, QUEUED, ACQUIRED, etc.) and how SurfaceFlinger consumes (the BufferLayer::onFrameAvailable / Layer's acquireBuffer). And how Vsync ties: DispSync, and the Scheduler's callback to SurfaceFlinger's onMessageReceived (Vsync signal). Let me also get the connection between SurfaceFlinger receiving vsync → doComposition, and BufferQueue onFrameAvailable → invalidate/refresh.

Let me read several key regions in parallel. I'll read:
1. BufferQueueCore.h lines 150-260 (slots, queue, free, active buffers, state)
2. BufferQueueProducer.cpp dequeueBuffer 356-460 and queueBuffer 763-900
3. BufferQueueConsumer.cpp acquireBuffer 54-270 and releaseBuffer 411-480
4. DispSync.h 30-130 (model)
5. Scheduler.h 39-120 and maybe the vsync dispatch

Let me also get how SurfaceFlinger registers for vsync - search for "onFrameAvailable" in BufferQueue and "MessageQueue" / "setEventThread" / vsync in SurfaceFlinger. Actually let me keep focused. Let me read the key pieces.

Let me also get the BufferState enum lines in BufferQueueCore.h. The search didn't show it; let me search for "enum BufferState" specifically and "FREE" "DEQUEUED" "QUEUED" "ACQUIRED".

Let me do parallel reads.
核心结构已确认。我读取关键段以获取精确行号与状态机细节。
Good. Now I need the BufferState enum location. Let me search for it in BufferQueueDefs or BufferSlot. Let me search across libs/gui. Also I want to confirm how SurfaceFlinger acquires buffers (BufferLayer::onFrameAvailable → mQueueItem / latchBuffer) and how Vsync is delivered to SurfaceFlinger (DispSyncSource → EventThread → MessageQueue → onMessageReceived). Let me also check the queueBuffer tail and the producer's onBufferQueued callback to consumer (mConsumerListener->onBufferAvailable).

Let me also grep for BufferState enum and the key state transitions, plus SurfaceFlinger's connect to BufferQueue consumer (BufferLayer::onFirstRef / createBufferQueue) and the onFrameAvailable path.

Let me do parallel searches:
1. BufferState enum location
2. queueBuffer tail (763 onward) key lines
3. BufferLayer onFrameAvailable / latchBuffer / acquireBuffer in surfaceflinger
4. Vsync delivery chain: EventThread, MessageQueue::Handler, setVsyncEnabled, SurfaceFlinger::onMessageReceived VSYNC
5. DispSync computeModel / updateModel line numbers

Let me run
BufferState 枚举与 Vsync 衔接需进一步确认。我并行定位状态枚举、queueBuffer 尾部、SurfaceFlinger 的 BufferLayer 取帧、以及 Vsync 投递链。
I have rich data. Let me get the BufferState enum definition (it's in BufferQueueDefs.h probably) and the SurfaceFlinger's MessageQueue/VSYNC delivery. Let me search for BufferState enum in BufferQueueDefs.h and the SurfaceFlinger onMessageReceived vsync message handling (INVALIDATE / REFRESH). Also the EventThread and MessageQueue to connect Vsync to SF.

Let me do a few targeted searches:
1. BufferState enum (in BufferQueueDefs.h or BufferSlot.h) - search "enum BufferState" pattern more specifically with file filter
2. SurfaceFlinger::onMessageReceived INVALIDATE/REFRESH/TRANSACTION cases
3. MessageQueue.h / Handler for VSYNC - and how onMessageReceived gets called
4. BufferQueueLayer::onFrameAvailable (442) and latchBuffer path

Let me read:
- search BufferQueueDefs.h for enum BufferState
- SurfaceFlinger.cpp onMessageReceived around 1813 (already know it exists). Let me search for "case MessageQueue::INVALIDATE" and "case MessageQueue::REFRESH"
- BufferQueueLayer.cpp onFrameAvailable 442 and latchBuffer

Let me run parallel.
我定位 BufferState 枚举定义、SurfaceFlinger 的 VSYNC 消息处理与 BufferQueueLayer 取帧路径。
Now I need the BufferState enum. Let me search for it specifically. It's likely in BufferQueueDefs.h but my search returned 0 for "enum BufferState". Let me search the file for "BufferState" generally. Actually the search pattern may have failed because the file might use a different structure. Let me grep for "BufferState" in BufferQueueDefs.h.

Also I want to confirm the Vsync delivery: DispSyncSource → EventThread → MessageQueue → SurfaceFlinger. Let me search for DispSyncSource and how onVSyncEvent connects, and how SurfaceFlinger registers for vsync (setVsyncEnabled, mScheduler->setVsyncPeriod). Also the onMessageReceived INVALIDATE/REFRESH at 1813.

Let me also get handleMessageInvalidate (1984) which calls latchBuffer (the consumer acquire). And handleMessageRefresh does composition. And read BufferState enum.

Let me run parallel:
1. search BufferQueueDefs.h for "BufferState"
2. read SurfaceFlinger.cpp onMessageReceived 1813-1920
3. search DispSyncSource.h/.cpp for onVSyncEvent and connection to EventThread
4. search Scheduler.cpp for onVSyncEvent / setVsyncPeriod / connection to MessageQueue

Let me do it
我定位 BufferState 枚举及 Vsync 投递链（DispSyncSource → EventThread → MessageQueue → SurfaceFlinger）。
Now I have a comprehensive picture. Let me find the BufferState enum location. It's actually in `ui/BufferQueueDefs.h` (the include at line 21). Let me check the framework's ui header. Actually the BufferState is defined in `frameworks/native/libs/ui/include/ui/BufferQueueDefs.h`. Let me search there.

Also I should confirm the Vsync flow: DispSyncSource::onVSyncEvent → EventThread → connection → MessageQueue → onMessageReceived. The DispSyncSource.cpp line 113 `callback->onVSyncEvent(when)` - callback is the EventThread. And then EventThread posts to MessageQueue. Let me verify Scheduler connects DispSyncSource to EventThread. Actually there's a lot; my analysis can describe the chain conceptually with the evidence I have.

Let me get the BufferState enum from ui/BufferQueueDefs.h to be precise, and confirm acquireBufferLocked path in BufferLayerConsumer (490 onBufferAvailable → signalLayerUpdate). Actually onBufferAvailable just signals. The acquire happens in latchBuffer/updateTexImage.

Let me grab:
1. BufferState enum in ui/BufferQueueDefs.h
2. BufferLayerConsumer::onBufferAvailable (490) and acquireBufferLocked (210)

Then I have enough. Let me also note the Vsync → signalRefresh → onMessageReceived REFRESH → handleMessageRefresh → doComposition chain which I already have at 1916.

Let me search the enum and read onBufferAvailable
我确认 BufferState 枚举定义与 BufferLayerConsumer 取帧回调，完成最终衔接。
The BufferState enum isn't in ui/BufferQueueDefs.h displayed. Let me search for the enum definition file. It's probably in `BufferSlot.h` or `BufferState`. But I have an autolock: it's `mBufferState.dequeue()`, `queue()`, `acquire()`, `freeQueued()`, `release()`, `acquireNotInQueue()`, `detachProducer()`, `detachConsumer()`, `isFree()`, `isDequeued()`, `isAcquired()`, `isShared()`. The enum likely: FREE, DEQUEUED, QUEUED, ACQUIRED, plus shared flag. I can describe it without exact line. Let me quickly grep for "enum BufferState" file.

Actually my earlier search for "enum BufferState" returned 783 results but all from tests. The actual enum must be in a header. Let me search more precisely: pattern `enum BufferState\s*\{` with outputMode files_with_matches or grep the libs/gui/include/gui.

Let me just do one more search to nail the BufferState enum location, then write the analysis. Actually I have enough to describe state machine conceptually. But for precision, let me grep.

Let me search "enum BufferState" in include dirs
状态枚举需精确行号。我定位 BufferState 定义文件。
The BufferState enum is defined somewhere but search isn't finding "enum BufferState" with the pattern. Let me grep just "BufferState" in all .h files to find the header. Actually it might be defined as a nested class. Let me grep for "mBufferState.dequeue" - already know. Let me search the BufferState class definition broadly.

Actually I recall in Android, `BufferState` is a struct/class in `BufferQueueDefs.h` located in `frameworks/native/libs/gui/include/gui/BufferQueueDefs.h`. But my search of that file returned only namespace. Wait, the gui one (libs/gui/include/gui/BufferQueueDefs.h line 21 includes ui/BufferQueueDefs.h, and line 26-28 namespace BufferQueueDefs with something. Let me read the gui BufferQueueDefs.h fully to see BufferState enum.

Let me read libs/gui/include/gui/BufferQueueDefs.h.
BufferState is in BufferSlot.h. Let me read BufferSlot.h to get the BufferState enum
Now I have all the data I need. Let me write a comprehensive analysis file combining BufferQueue producer/consumer model AND Vsync pacing, since the user said "or" — I'll cover both but structure clearly. Actually the user said "继续 BufferQueue 生产者/消费者模型或 Vsync 帧节奏衔接" — I'll cover both since they're deeply related, giving a complete picture.

Let me write the markdown with:
1. BufferQueue overview tree
2. BufferState enum / slot state machine (BufferSlot.h 33-172)
3. Producer side: dequeueBuffer (356), requestBuffer, queueBuffer (763) → mQueue push, notify consumer
4. Consumer side: acquireBuffer (54), releaseBuffer (411)
5. mSlots/mQueue/mFreeBuffers/mActiveBuffers (BufferQueueCore.h 204-222)
6. The flow with SurfaceFlinger: BufferQueueLayer::onFrameAvailable (442) → signalLayerUpdate → onMessageReceived INVALIDATE (1813) → handleMessageInvalidate (1984) → latchBuffer → acquireBufferLocked → updateAndReleaseLocked
7. Vsync: DispSync model (DispSync.h 33) maintains vsync period model from HWComposer present fences; DispSyncSource (43 setVSyncEnabled, 113 onVSyncEvent → callback); EventThread → MessageQueue → SurfaceFlinger::onMessageReceived (1813); REFRESH (1916) → handleMessageRefresh → doComposition
8. The full frame loop: app draws → queueBuffer → onFrameAvailable → signalLayerUpdate (INVALIDATE) → latch at next vsync → compose → present → present fence → addPresentFence → DispSync resync

Let me write the file. I'll export to `BufferQueue_Vsync_解析.md`.

Let me make it well-structured with tree diagrams and core analysis, consistent with prior docs. Use line number citations.

Let me write.
状态机与 Vsync 链已全部确认。导出完整分析至 `c:\D\android_project\cells-android10\BufferQueue_Vsync_解析.md`。
已完成 BufferQueue 生产者/消费者模型 + Vsync 帧节奏衔接，导出至 `c:\D\android_project\cells-android10\BufferQueue_Vsync_解析.md`。

## 一、BufferQueue 模型树（核心）
```
BufferQueue = BufferQueueCore + Producer + Consumer（三者走 Binder 跨进程）
├── Core 共享状态 BufferQueueCore.h :57
│   ├── mSlots[64] :204  // 槽位传所有权，不走 Binder 拷贝
│   ├── mQueue(FIFO) :207 / mFreeBuffers :215 / mActiveBuffers :222
│   └── mConsumerListener :170（入队回调 SF）
├── BufferState 状态机 BufferSlot.h :33
│   └─ FREE/DEQUEUED/QUEUED/ACQUIRED 用 4 个计数器表示（:43）
├── Producer：dequeueBuffer :356 → requestBuffer :67 → queueBuffer :763（入 FIFO + 唤醒）
└── Consumer：acquireBuffer :54（按 expectedPresent 选帧/丢帧）→ releaseBuffer :411
```

## 二、SurfaceFlinger 消费桥接
- `BufferQueueLayer::onFrameAvailable :442` 收帧入 `mQueueItems` → `signalLayerUpdate()`
- `SurfaceFlinger::onMessageReceived :1813`：`INVALIDATE :1819` 触发 `handleMessageInvalidate :1897`(`latchBuffer`→`acquireBufferLocked`)；`REFRESH :1916` 触发 `handleMessageRefresh :1917`(合成上屏)

## 三、Vsync 帧节奏
- **DispSync 模型**（`DispSync.h :33`）：用硬件 Vsync + `addPresentFence :108` 的 present fence 拟合稳定周期，抗抖动；`setVsyncPeriod :291` 支撑可变刷新率
- 投递链：`HW Vsync → DispSync → DispSyncSource::onVSyncEvent :113 → EventThread → MessageQueue → onMessageReceived`
- **闭环**：present → retire fence → DispSync 校准 → 下一 Vsync 派发

## 四、关键设计
1. **slot 传所有权**：`GraphicBuffer` 句柄只 `requestBuffer :67` 映射一次，后续靠 slot 复用（省 Binder 拷贝）
2. **计数式状态机**：支持多消费者、共享 buffer（`isFree :87`）
3. **两端丢帧**：Producer `queueBuffer :919` 覆盖旧帧；Consumer `acquireBuffer :115` 按时间戳丢过期帧
4. **三缓冲节拍**：App `dequeueBuffer` 阻塞在 `mDequeueCondition :226`（等 SF `releaseBuffer` 归还），与 Vsync 对齐成“每 Vsync 一帧”

至此 **init → Zygote → SystemServer → Binder → SurfaceFlinger → BufferQueue → Vsync** 全链路贯通。如需继续 `HWC2 合成路径` 或 `Choreographer(Vsync 到 App 的投递)`，告诉我即可。

至此 **init → Zygote → SystemServer → Binder → SurfaceFlinger(合成) → BufferQueue(帧缓冲) → Vsync(节奏)** 全链路贯通。
如需继续 `HWC2 合成路径 / 客户端 Surface(ANativeWindow) 到 dequeueBuffer 的封装` 或 `Choreographer(Vsync 到 App 的投递)`，告诉我即可。
