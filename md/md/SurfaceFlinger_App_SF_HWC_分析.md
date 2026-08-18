# SurfaceFlinger 启动流程 与 App→SF→HWC 分层链路拆解

> 源码基线：本仓库 `frameworks/native`（tag `v1.0.0`，composer 已切到 **AIDL HAL** `android.hardware.graphics.composer3`，即 Android 14 形态）
> 核心文件：
> - `frameworks/native/services/surfaceflinger/main_surfaceflinger.cpp`
> - `frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp`
> - `frameworks/native/services/surfaceflinger/DisplayHardware/{HWComposer,AidlComposerHal}.cpp`
> - `frameworks/native/libs/gui/SurfaceComposerClient.cpp`

---

## 一、核心结论（先看这张表，白纸画图全靠它）

| 链路段 | 客户端进程 | 服务端进程 | IPC 机制 | 服务/接口名 |
|---|---|---|---|---|
| **App → SF** | app 进程（每个应用） | `surfaceflinger` 进程 | **Binder**（C++ `libbinder`） | `ISurfaceComposer`（`"SurfaceFlinger"`）；新 AIDL 接口 `gui::ISurfaceComposer`（`"SurfaceFlingerAIDL"`） |
| （App→SF 的图形数据） | app 进程 | `surfaceflinger` 进程 | **共享内存**（gralloc / dma-buf），**不经 Binder 拷贝** | `BufferQueue`：buffer handle 经 Binder 传 fd，像素在共享内存里 |
| **SF → HWC** | `surfaceflinger` 进程（system） | `hwcomposer` HAL 进程（**vendor**） | **AIDL Binder**（NDK AIDL 后端） | `android.hardware.graphics.composer3.IComposer`，实例名 `IComposer/default` |
| HWC → SF（回调） | `hwcomposer` 进程 | `surfaceflinger` 进程 | **AIDL Binder 反向调用** | `BnComposerCallback`（vsync / hotplug / refreshRateChanged） |

> 一句话：**三段跑在三个不同进程**（app / surfaceflinger / hwcomposer-vendor），中间两段都是 Binder，但图形像素走共享内存不走 Binder。

---

## 二、main_surfaceflinger.cpp 的 main()：怎么启动 SF、怎么 addService

入口 `main_surfaceflinger.cpp:79` `int main(int, char**)`。完整启动顺序（带行号）：

```
L82  hardware::configureRpcThreadpool(1, false);          // HIDL RPC 线程池（给下面注册的 IAllocator 用）
L85  startGraphicsAllocatorService();                     // 注册 HIDL 图形分配器 IAllocator(passthrough)
L89  ProcessState::self()->setThreadPoolMaxThreadCount(4); // SF 进程 binder 线程上限=4
L93  SurfaceFlinger::setSchedAttr(true);                   // 给所有线程设 uclamp.min（实时性）
L102-115 把当前线程设为 SCHED_FIFO/优先级1               // 让随后起的 binder 线程池继承 RT 策略
L118-119 ps->startThreadPool();                            // 启动 binder 线程池
L129 sp<SurfaceFlinger> flinger = surfaceflinger::createSurfaceFlinger();  // ① 实例化
L134-136 flinger->setMinSchedulerPolicy(SCHED_FIFO, 1);    // 最低调度策略兜底
L138 setpriority(PRIO_PROCESS,0,PRIORITY_URGENT_DISPLAY);  // 当前线程= urgent display
L143 flinger->init();                                      // ② 初始化（必须在客户端能连上前）
L146-148 sm->addService(...SurfaceFlinger::getServiceName(), flinger,...); // ③ 注册进 servicemanager
L151-153 sm->addService("SurfaceFlingerAIDL", composerAIDL,...);            // 新 AIDL 接口也注册
L155 startDisplayService();                                // 依赖 SF 已注册
L157 SurfaceFlinger::setSchedFifo(true);
L162 flinger->run();                                       // ④ 主线程进入消息循环，永不返回
```

### ① 怎么实例化（start）
- `createSurfaceFlinger()` 定义在 `SurfaceFlingerFactory.cpp:26`，内部 `static DefaultFactory factory; return sp<SurfaceFlinger>::make(factory);`。
- `SurfaceFlinger` 继承自 `BnSurfaceComposer`（即它是 Binder 服务端 `BBinder`），所以才能被 `addService` 注册。

### ③ 怎么 addService（把自身注册成 "SurfaceFlinger"）
```cpp
// main_surfaceflinger.cpp:146-148
sp<IServiceManager> sm(defaultServiceManager());
sm->addService(String16(SurfaceFlinger::getServiceName()), flinger, false,
               IServiceManager::DUMP_FLAG_PRIORITY_CRITICAL | IServiceManager::DUMP_FLAG_PROTO);
```
- `getServiceName()` 返回字面量 `"SurfaceFlinger"`。
- `defaultServiceManager()` 拿到的是 **servicemanager 的 Binder 代理**；`addService` 本身也是一次 **Binder 调用**（SF 进程 → servicemanager 进程），把 `flinger`(BBinder) 挂到 `"SurfaceFlinger"` 名下。
- 新版 AIDL 接口 `gui::ISurfaceComposer` 同时以 `"SurfaceFlingerAIDL"` 注册（`L151-153`），`SurfaceComposerClient` 新路径走这个。

> 关键点：`init()` 在 `addService` **之前**调用（L143 < L146）。也就是“先把子系统全部初始化好、连上 HWC、配好显示器”，才允许 app 连上来，避免客户端连上后用到未就绪的状态。

### ④ run()
`flinger->run()` 让 main 线程进入 SF 主循环（处理事务、合成、present）。此后该线程不再返回，binder 并发请求由 L119 起的线程池处理。

---

## 三、SurfaceFlinger::init()：初始化了哪些子系统

位置 `SurfaceFlinger.cpp:804` `void SurfaceFlinger::init()`。按**实际调用顺序**拆（这是画流程图的主干）：

| 顺序 | 子系统 | 源码行 | 做了什么 |
|---|---|---|---|
| 1 | Transaction 过滤器 | `L807` `addTransactionReadyFilters()` | 注册事务就绪过滤器 |
| 2 | **RenderEngine** | `L813-830` | `renderengine::RenderEngine::create(builder.build())` → `mRenderEngine`。GPU 合成后端（GLES/Vulkan），建完即 `mCompositionEngine->setRenderEngine()` |
| 3 | CompositionEngine + **HWComposer** | `L837-839` | `mCompositionEngine->setHwComposer(getFactory().createHWComposer(mHwcServiceName))`，随后 `setCallback(*this)` 注册 HWC 回调 |
| 4 | 启动显示配置 | `L849` `configureLocked()` | 处理开机已连接的 display hotplug，失败 FATAL |
| 5 | 提交主显 | `L853-864` `processDisplayAdded(...)` | 把 primary display 加入 drawing state |
| 6 | **Scheduler** | `L873` `initScheduler(display)` | 建 Scheduler + EventThread + 两个 ConnectionHandle（详见下） |
| 7 | 分发 hotplug | `L874` `dispatchDisplayHotplugEvent` | 通知各监听者主显已连 |
| 8 | 次显 / 绘制态 | `L877-882` | `processDisplayChangesLocked()`、初始化 `mDrawingState` |
| 9 | PowerAdvisor | `L887` `mPowerAdvisor->init()` | 功耗建议器 |
| 10 | Shader 缓存预热 | `L889-901` | `mRenderEngine->primeCache()`（先降优先级再恢复 SCHED_FIFO） |
| 11 | 属性设置线程 | `L905-911` | `createStartPropertySetThread(presentFenceReliable)`，启动 `StartPropertySetThread` 去设置 `service.bootanim.exit` 等属性、拉起开机动画 |

### 3.1 HWC 子系统（SF→HWC 的源头）
- 工厂入口：`DefaultFactory::createHWComposer()`（`SurfaceFlingerDefaultFactory.cpp:43`）→ `std::make_unique<android::impl::HWComposer>(serviceName)`。
- `HWComposer.cpp:92` 构造函数：`HWComposer(Hwc2::Composer::create(composerServiceName))`。
- `serviceName` = `mHwcServiceName`，来自属性 `debug.sf.hwc_service_name`，默认 `"default"`（`SurfaceFlinger.cpp:368`）。
- HAL 绑定在 **`AidlComposerHal.cpp:230-233`**：
  ```cpp
  AidlComposer::AidlComposer(const std::string& serviceName) {
      mAidlComposer = AidlIComposer::fromBinder(
          ndk::SpAIBinder(AServiceManager_waitForService(instance(serviceName).c_str())));
      ...
      mAidlComposer->createClient(&mAidlComposerClient);   // 拿到 IComposerClient
  }
  // instance() = AidlIComposer::descriptor + "/" + serviceName  →  "android.hardware.graphics.composer3.IComposer/default"
  ```
  - 即 SF 作为 **AIDL 客户端**，通过 `AServiceManager_waitForService` 等 vendor 侧的 `hwcomposer` HAL 服务上线（binder 跨 system→vendor，符合 Treble 隔离）。
  - HWC 回调（vsync / hotplug）走 `BnComposerCallback`（`AidlIComposerCallbackWrapper`，`AidlComposerHal.cpp:171`）反向 Binder 调回 SF。

### 3.2 Scheduler 子系统（含 MessageQueue 的“去哪了”）
`initScheduler`（`SurfaceFlinger.cpp:3929-3987`）：
```cpp
mScheduler = std::make_unique<Scheduler>(static_cast<ICompositor&>(*this),
                                         static_cast<ISchedulerCallback&>(*this), features, ...); // L3959
mScheduler->registerDisplay(...);          // L3962
mScheduler->startTimers();                 // L3965
mAppConnectionHandle = mScheduler->createEventThread(Scheduler::Cycle::Render, ...);     // L3969 App 侧 vsync
mSfConnectionHandle  = mScheduler->createEventThread(Scheduler::Cycle::LastComposite, ...); // L3974 SF 侧 vsync
mScheduler->initVsync(...);                // L3980
```
- **MessageQueue 去哪了？** 在 Android 11 以前，SF 有 `mQueue.init(this)` 起的 `android::MessageQueue` + `EventThread` 经典模型。到本版本（Android 14 形态），**MessageQueue 已被 Scheduler + EventThread + ConnectionHandle 取代**：`mAppConnectionHandle` / `mSfConnectionHandle` 就是原来“App 的 vsync 消息队列”和“SF 的合成消息队列”的句柄。画白纸图时，**把旧版 `MessageQueue` 理解成“SF 内部被 vsync 驱动的消息/事件循环”，现在它体现在 Scheduler 的两条 EventThread 连接上**。
- EventThread 内部仍基于 `Looper`/消息机制向对应连接推送 vsync 信号，驱动 App 渲染节奏与 SF 合成节奏。

### 3.3 RenderEngine 子系统
- `renderengine::RenderEngine::create(...)`（`SurfaceFlinger.cpp:827`）封装 GLES/Vulkan 上下文，负责 GPU 合成（Client 合成）与特效（模糊、色彩管理等）。`mMaxRenderTargetSize` 取自 `getMaxTextureSize()`。

---

## 四、分层图（白纸可复刻版 + Mermaid）

### 4.1 文字版（照着画即可）
```
┌─────────────────────────┐         Binder(ISurfaceComposer)         ┌──────────────────────────┐
│       App 进程           │  ───────────────────────────────────▶   │      surfaceflinger 进程   │
│  (每个应用, 自己的进程)   │   事务/命令(createSurface, setLayer)      │   (独立进程, main_surface   │
│                         │                                          │    flinger.cpp 拉起)       │
│  SurfaceComposerClient  │   ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─▶  │  SurfaceFlinger 主线程      │
│        │                │   BufferQueue: 图形 buffer 走            │   ├ init(): RenderEngine    │
│        ▼                │   共享内存(gralloc/dma-buf),            │   ├ init(): HWComposer(HAL) │
│   UI 渲染 → 填 buffer    │   handle 经 Binder 传 fd(不拷贝像素)    │   ├ initScheduler(): Scheduler│
│        │                │                                          │   │   EventThread×2          │
│        ▼                │                                          │   └ MessageQueue(已被并入)  │
│  queueBuffer(buffer)    │                                          │         │                  │
└─────────────────────────┘                                          │         │ AIDL Binder       │
                                                                      │         ▼                  │
                                                                      │  IComposer::createLayer /  │
                                                                      │  present / setLayerXXX     │
                                                                      └──────────┬───────────────┘
                                                                                 │ AIDL Binder
                                                                                 ▼ (跨 system→vendor)
                                                                      ┌──────────────────────────┐
                                                                      │   hwcomposer HAL 进程      │
                                                                      │   (vendor, Treble 隔离)    │
                                                                      │  android.hardware.graphics │
                                                                      │  .composer3.IComposer      │
                                                                      │  实例: IComposer/default    │
                                                                      │   ↓ 真正送显(DRM/KMS/Panel) │
                                                                      └──────────────────────────┘
                                                                      ▲ vsync/hotplug 经 BnComposer
                                                                      │ Callback 反向 Binder 回 SF
```



### 4.2 Mermaid（渲染版）
```mermaid

flowchart TB
    subgraph APP["App 进程（每个应用）"]
        SC["SurfaceComposerClient<br/>(libs/gui/SurfaceComposerClient.cpp)"]
        UI["UI 渲染 → 填 GraphicBuffer"]
        SC --> UI
    end

    subgraph SF["surfaceflinger 进程（system）"]
        MAIN["main() 主线程<br/>main_surfaceflinger.cpp:79"]
        INIT["SurfaceFlinger::init()<br/>SurfaceFlinger.cpp:804"]
        RE["RenderEngine<br/>(SurfaceFlinger.cpp:827)"]
        HWC["HWComposer (HAL 客户端)<br/>HWComposer.cpp:92 → AidlComposerHal.cpp:230"]
        SCH["Scheduler + EventThread×2<br/>(SurfaceFlinger.cpp:3929)"]
        MAIN --> INIT
        INIT --> RE
        INIT --> HWC
        INIT --> SCH
    end

    subgraph HWC_PROC["hwcomposer HAL 进程（vendor, Treble）"]
        HAL["IComposer (AIDL)<br/>实例 IComposer/default"]
        PANEL["DRM/KMS → Panel 送显"]
        HAL --> PANEL
    end

    SC -->|"Binder: ISurfaceComposer (SurfaceFlinger / SurfaceFlingerAIDL)"| MAIN
    UI -.->|"BufferQueue 共享内存: gralloc/dma-buf, handle 经 Binder 传 fd, 像素不拷贝"| HWC
    HWC -->|"AIDL Binder: IComposer::createLayer / present"| HAL
    HAL -.->|"BnComposerCallback: vsync / hotplug 反向 Binder"| INIT
```

---

## 五、App→SF→HWC 三段：进程 & IPC 速查卡（背这个）

| 段 | 进程 | IPC | 关键源码落点 |
|---|---|---|---|
| **App → SF** | app 进程 → `surfaceflinger` | **Binder**（`ISurfaceComposer` / `gui::ISurfaceComposer` AIDL） | `SurfaceComposerClient.cpp:97-98` `waitForService<ISurfaceComposer>("SurfaceFlinger")`；AIDL 版 `:144-145` `"SurfaceFlingerAIDL"` |
| **App → SF 的图形数据** | app 进程 → `surfaceflinger` | **共享内存**（gralloc/dma-buf，BufferQueue） | `queueBuffer` 只传 buffer handle(fd)，像素在共享内存，不经 Binder 拷贝 |
| **SF → HWC** | `surfaceflinger`(system) → `hwcomposer`(vendor) | **AIDL Binder**（`IComposer`，跨 Treble） | `AidlComposerHal.cpp:230-233` `AServiceManager_waitForService("…IComposer/default")` |
| **HWC → SF** | `hwcomposer` → `surfaceflinger` | **AIDL Binder 反向**（`BnComposerCallback`） | `AidlComposerHal.cpp:171` `AidlIComposerCallbackWrapper` |

---

## 六、关键类 / 函数速查表（带文件路径）

| 类 / 函数 | 文件 : 行 | 作用 |
|---|---|---|
| `main()` | `main_surfaceflinger.cpp:79` | SF 进程入口：建线程池、实例化、init、addService、run |
| `surfaceflinger::createSurfaceFlinger()` | `SurfaceFlingerFactory.cpp:26` | 经 DefaultFactory 造 `sp<SurfaceFlinger>` |
| `SurfaceFlinger::getServiceName()` | （返回 `"SurfaceFlinger"`） | addService 用到的服务名 |
| `SurfaceFlinger::init()` | `SurfaceFlinger.cpp:804` | 初始化全部子系统 |
| `renderengine::RenderEngine::create()` | `SurfaceFlinger.cpp:827` | 建 GPU 合成后端 |
| `getFactory().createHWComposer()` | `SurfaceFlinger.cpp:838` / `SurfaceFlingerDefaultFactory.cpp:43` | 建 HWComposer（HAL 客户端） |
| `HWComposer::HWComposer(name)` | `DisplayHardware/HWComposer.cpp:92` | 经 `Hwc2::Composer::create` 连 HAL |
| `AidlComposer::AidlComposer()` | `DisplayHardware/AidlComposerHal.cpp:230` | `AServiceManager_waitForService` 等 `IComposer/default`，`createClient` |
| `SurfaceFlinger::initScheduler()` | `SurfaceFlinger.cpp:3929` | 建 Scheduler + 两条 EventThread 连接（替代旧 MessageQueue） |
| `SurfaceFlinger::run()` | `main_surfaceflinger.cpp:162` | 主线程进入合成主循环 |
| `SurfaceComposerClient` 连接 | `libs/gui/SurfaceComposerClient.cpp:97` | App 侧 `waitForService<ISurfaceComposer>("SurfaceFlinger")` |

---

```
int main(int, char **) {
    // 启动其它系统服务的主循环（与 SF 并行运行的非图形类后台服务入口）
    OtherSystemServiceLoopRun();

    // 忽略 SIGPIPE 信号，避免向已关闭的对端写数据时被信号中断进程
    signal(SIGPIPE, SIG_IGN);

    // 配置 HIDL RPC 线程池：最多 1 个线程，且调用方（SF 主线程）不会主动 join
    hardware::configureRpcThreadpool(1 /* maxThreads */,
                                     false /* callerWillJoin */);

    /**
     * 分配硬件 服务
     */
    // 启动图形内存分配器（Graphics Allocator）HAL 服务，供后续分配图形缓冲区使用
    startGraphicsAllocatorService();

    // 当 SurfaceFlinger 作为独立进程启动时，将 binder 线程数上限限制为 4，避免资源浪费
    ProcessState::self()->setThreadPoolMaxThreadCount(4);

    // 启动 binder 线程池（懒启动工作线程，处理跨进程 binder 调用）
    sp<ProcessState> ps(ProcessState::self());
    ps->startThreadPool();

    // 实例化 SurfaceFlinger 主体对象（此时尚未初始化）
    sp<SurfaceFlinger> flinger = surfaceflinger::createSurfaceFlinger();

    // 提升主线程调度优先级为紧急显示级别，确保渲染不被普通任务抢占
    setpriority(PRIO_PROCESS, 0, PRIORITY_URGENT_DISPLAY);

    // 将主线程调度策略设为前台（SP_FOREGROUND），保证交互响应及时
    set_sched_policy(0, SP_FOREGROUND);

    // 将多数 SurfaceFlinger 线程放入 system-background cpuset，
    // 避免不必要地占用大核（big cores），节省功耗
    // 注意：须在 binder 线程池初始化之后再设置
    if (cpusets_enabled()) set_cpuset_policy(0, SP_SYSTEM);

    // 在客户端连接前完成初始化（建立显示、合成器等内部状态）
    flinger->init();

    // 将 SurfaceFlinger 注册到 servicemanager，并标记为高优先级、可 dump 的关键服务
    sp<IServiceManager> sm(defaultServiceManager());
    sm->addService(String16(SurfaceFlinger::getServiceName()), flinger, false,
                   IServiceManager::DUMP_FLAG_PRIORITY_CRITICAL | IServiceManager::DUMP_FLAG_PROTO);

    // 启动 Display 服务，该服务依赖上面 SF 先完成注册
    startDisplayService(); // dependency on SF getting registered above

    // 为 SF 主线程设置 SCHED_FIFO 实时调度策略，优先级 2，保障合成时序稳定
    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        ALOGE("Couldn't set SCHED_FIFO");
    }

    // 在当前主线程中运行 SurfaceFlinger 消息循环（永不返回，直到进程退出）
    flinger->run();

    return 0;
}

```


```
void SurfaceFlinger::init() {
    ALOGI(  "SurfaceFlinger's main thread ready to run. "
            "Initializing graphics H/W...");

    ALOGI("Phase offset NS: %" PRId64 "", mPhaseOffsets->getCurrentAppOffset());

    Mutex::Autolock _l(mStateLock);
    // 启动调度器（Scheduler），并传入两个回调：
    //   1) 开关主屏 VSYNC 使能
    //   2) 刷新率配置对象（用于计算 VSYNC 周期）
    mScheduler =
            getFactory().createScheduler([this](bool enabled) { setPrimaryVsyncEnabled(enabled); },
                                         mRefreshRateConfigs);
    // 构造一个用于让 HWC 重新同步 VSYNC 周期的回调
    auto resyncCallback =
            mScheduler->makeResyncCallback(std::bind(&SurfaceFlinger::getVsyncPeriod, this));

    // 为「应用端」创建一条 VSYNC 连接（app 偏移），用于驱动应用绘制节奏
    mAppConnectionHandle =
            mScheduler->createConnection("app", mVsyncModulator.getOffsets().app,
                                         mPhaseOffsets->getOffsetThresholdForNextVsync(),
                                         resyncCallback,
                                         impl::EventThread::InterceptVSyncsCallback());
    // 为「SF 自身」创建一条 VSYNC 连接（sf 偏移），用于驱动合成节奏，
    // 并附带一个拦截回调，把 VSYNC 事件写入拦截器（用于录制/调试）
    mSfConnectionHandle =
            mScheduler->createConnection("sf", mVsyncModulator.getOffsets().sf,
                                         mPhaseOffsets->getOffsetThresholdForNextVsync(),
                                         resyncCallback, [this](nsecs_t timestamp) {
                                             mInterceptor->saveVSyncEvent(timestamp);
                                         });

    // 注册 input 事件回调监听，过滤 vsync 事件（将 SF 的 VSYNC 连接挂到消息队列）
    mEventQueue->setEventConnection(mScheduler->getEventConnection(mSfConnectionHandle));

    // 把调度器及两条连接句柄交给 VSYNC 调制器，由其统一管理偏移策略
    mVsyncModulator.setSchedulerAndHandles(mScheduler.get(), mAppConnectionHandle.get(),
                                           mSfConnectionHandle.get());

    // 启动区域采样线程（RegionSamplingThread），用于根据画面内容动态判断暗色区域等
    mRegionSamplingThread =
            new RegionSamplingThread(*this, *mScheduler,
                                     RegionSamplingThread::EnvironmentTimingTunables());

    // 创建渲染引擎（RenderEngine），按显示/配置选定，不会失败
    // 先组装渲染特性开关：是否色彩管理、是否高优先级上下文、是否受保护内容上下文
    int32_t renderEngineFeature = 0;
    renderEngineFeature |= (useColorManagement ?
                            renderengine::RenderEngine::USE_COLOR_MANAGEMENT : 0);
    renderEngineFeature |= (useContextPriority ?
                            renderengine::RenderEngine::USE_HIGH_PRIORITY_CONTEXT : 0);
    renderEngineFeature |=
            (enable_protected_contents(false) ? renderengine::RenderEngine::ENABLE_PROTECTED_CONTEXT
                                              : 0);

    // TODO(b/77156734): 后续应改用 HAL 类型，避免此处强制类型转换
    // 把 cache 大小设为 maxFrameBufferAcquiredBuffers，该值针对单显示场景做了精细调优
    mCompositionEngine->setRenderEngine(
            renderengine::RenderEngine::create(static_cast<int32_t>(defaultCompositionPixelFormat),
                                               renderEngineFeature, maxFrameBufferAcquiredBuffers));

    // 不支持在 VR Flinger 已经激活的状态下启动，否则直接致命退出
    LOG_ALWAYS_FATAL_IF(mVrFlingerRequestsDisplay,
            "Starting with vr flinger active is not currently supported.");
    // 创建 HWComposer 并向其注册回调（this 作为回调接收者）
    mCompositionEngine->setHwComposer(getFactory().createHWComposer(getBE().mHwcServiceName));
    mCompositionEngine->getHwComposer().registerCallback(this, getBE().mComposerSequenceId);
    // 处理初始的热插拔（hotplug）事件及随之产生的显示变更
    processDisplayHotplugEventsLocked();
    const auto display = getDefaultDisplayDeviceLocked();
    // 注册完 composer 回调后必须有内屏，否则致命退出
    LOG_ALWAYS_FATAL_IF(!display, "Missing internal display after registering composer callback.");
    // 内屏必须处于连接状态，否则致命退出
    LOG_ALWAYS_FATAL_IF(!getHwComposer().isConnected(*display->getId()),
                        "Internal display is disconnected.");

    // 若启用 VR Flinger，则创建之；其请求显示模式的回调需通过主线程消息异步处理，避免死锁
    if (useVrFlinger) {
        auto vrFlingerRequestDisplayCallback = [this](bool requestDisplay) {
            // 该回调来自 vr flinger 派发线程；直接调用 signalTransaction() 需持 mStateLock，
            // 可能引发死锁（见 b/66916578），故改为向主线程投递消息异步处理
            postMessageAsync(new LambdaMessage([=] {
                ALOGE_IF(false, "VR request display mode: requestDisplay=%d", requestDisplay);
                mVrFlingerRequestsDisplay = requestDisplay;
                signalTransaction();
            }));
        };
        mVrFlinger = dvr::VrFlinger::Create(getHwComposer().getComposer(),
                                            getHwComposer()
                                                    .fromPhysicalDisplayId(*display->getId())
                                                    .value_or(0),
                                            vrFlingerRequestDisplayCallback);
        if (!mVrFlinger) {
            ALOGE("Failed to start vrflinger");
        }
    }

    // 初始化绘制状态：把当前状态复制为绘制状态
    mDrawingState = mCurrentState;

    // 设置初始显示条件（例如点亮/解除默认显示的 blank）
    initializeDisplays();

    // 预热渲染引擎缓存
    getRenderEngine().primeCache();

    // 告知 native 图形 API 当前是否支持 present 时间戳：
    // 若 HWC 不具备「PresentFence 不可靠」能力，则 presentFence 是可靠的
    const bool presentFenceReliable =
            !getHwComposer().hasCapability(HWC2::Capability::PresentFenceIsNotReliable);
    // 启动属性设置线程（负责在首帧前设置系统属性等），传入 presentFence 可靠性
    mStartPropertySetThread = getFactory().createStartPropertySetThread(presentFenceReliable);

    if (mStartPropertySetThread->Start() != NO_ERROR) {
        ALOGE("Run StartPropertySetThread failed!");
    }

    // 回调监听：刷新率改变时，加锁后在主线程切换到目标刷新率
    mScheduler->setChangeRefreshRateCallback(
            [this](RefreshRateType type, Scheduler::ConfigEvent event) {
                Mutex::Autolock lock(mStateLock);
                setRefreshRateTo(type, event);
            });
    // 设置「获取当前刷新率类型」回调：根据默认显示的活动配置反查刷新率类型，缺失则回落默认
    mScheduler->setGetCurrentRefreshRateTypeCallback([this] {
        Mutex::Autolock lock(mStateLock);
        const auto display = getDefaultDisplayDeviceLocked();
        if (!display) {
            // 没有默认显示时，回落到默认刷新率类型
            return RefreshRateType::DEFAULT;
        }

        const int configId = display->getActiveConfig();
        for (const auto& [type, refresh] : mRefreshRateConfigs.getRefreshRates()) {
            if (refresh && refresh->configId == configId) {
                return type;
            }
        }
        // 理论上不会走到这里，但稳妥起见回落默认
        return RefreshRateType::DEFAULT;
    });
    // 设置「获取 VSYNC 周期」回调
    mScheduler->setGetVsyncPeriodCallback([this] {
        Mutex::Autolock lock(mStateLock);
        return getVsyncPeriod();
    });

    // 填充刷新率配置，并让刷新率统计模块记录当前配置模式
    mRefreshRateConfigs.populate(getHwComposer().getConfigs(*display->getId()));
    mRefreshRateStats.setConfigMode(getHwComposer().getActiveConfigIndex(*display->getId()));

    ALOGV("Done initializing");
}

```

## 七、容易踩的坑（对照源码）

1. **init 必须在 addService 之前**：`SurfaceFlinger.cpp:143` 的 `init()` 早于 `main_surfaceflinger.cpp:146` 的 `addService`。若反过来，app 可能连上后命中未就绪的 HWC/Display 状态。
2. **binder 线程数限制**：`main_surfaceflinger.cpp:89` 把线程池上限设为 4，避免 SF 被并发 binder 请求打爆，同时主线程跑合成（实时性靠 `SCHED_FIFO` + `PRIORITY_URGENT_DISPLAY`）。
3. **composer 是 AIDL 不是 HIDL**：本基线只有 `AidlComposerHal.cpp`（无 `HidlComposerHal`），`createClient` 失败会 FATAL。改 HAL 版本时注意 Treble 分区（HAL 在 vendor，SF 在 system，跨进程 AIDL）。
4. **MessageQueue 已并入 Scheduler**：旧文档里的 `mQueue.init(this)` 在本版本不存在；白纸图里“SF 内部消息循环”对应 `mAppConnectionHandle` / `mSfConnectionHandle` 两条 EventThread 连接。


