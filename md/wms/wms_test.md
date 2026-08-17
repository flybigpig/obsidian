让我全面读取 WMS 构造方法、初始化流程以及相关调用链。


让我再查看一些关键组件的初始化细节，以便给出更完整的分析。


## WindowManagerService (WMS) 构造方法与初始化详解

---

### 一、WMS 在 SystemServer 中的创建时序

在 [SystemServer.java](file:///c:/D/android_project/cells-android10/frameworks/base/services/java/com/android/server/SystemServer.java#L1023-L1047) 中，WMS 的创建位于 IMS 之后，并经历多个初始化阶段：

```
① new InputManagerService(context)                  ← L1024 先创建 IMS
② WindowManagerService.main(context, inputManager, ...) ← L1033 创建 WMS
③ ServiceManager.addService(WINDOW_SERVICE, wm)     ← L1035 注册到 ServiceManager
④ ServiceManager.addService(INPUT_SERVICE, inputManager) ← L1037 注册 IMS
⑤ mActivityManagerService.setWindowManager(wm)      ← L1042 AMS 关联 WMS
⑥ wm.onInitReady()                                  ← L1046 WMS 初始化就绪
   ...
⑦ wm.displayReady()                                 ← L1150 显示就绪
```

---

### 二、静态工厂方法 main()

[WindowManagerService.main()](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java#L1010-L1018)

```java
public static WindowManagerService main(final Context context, final InputManagerService im,
        final boolean showBootMsgs, final boolean onlyCore, WindowManagerPolicy policy,
        ActivityTaskManagerService atm, TransactionFactory transactionFactory) {
    // WMS 的创建运行在 "android.display" 线程中
    DisplayThread.getHandler().runWithScissors(() ->
            sInstance = new WindowManagerService(context, im, showBootMsgs, onlyCore,
                    policy, atm, transactionFactory), 0);
    return sInstance;
}
```

**关键设计：**
- 通过 `DisplayThread.getHandler().runWithScissors()` 确保 WMS 在 **DisplayThread**（`android.display` 线程）中创建
- DisplayThread 是一个单例前台线程，专供 WMS、DisplayManager、InputManager 共享使用
- `runWithScissors` 会同步等待构造完成后才返回，确保 `sInstance` 已就绪

**参数说明：**

| 参数 | 类型 | 来源 | 说明 |
|------|------|------|------|
| `context` | Context | SystemServer | 系统上下文 |
| `im` | InputManagerService | 步骤①创建 | 输入管理服务（必须在 WMS 之前创建） |
| `showBootMsgs` | boolean | `!mFirstBoot` | 是否显示启动消息（非首次启动时显示） |
| `onlyCore` | boolean | SystemServer | 是否仅运行核心模式 |
| `policy` | WindowManagerPolicy | `new PhoneWindowManager()` | 窗口策略实现 |
| `atm` | ActivityTaskManagerService | AMS 内部 | Activity 任务管理服务 |
| `transactionFactory` | TransactionFactory | `SurfaceControl.Transaction::new` | Surface 事务工厂 |

---

### 三、构造方法详解

[WindowManagerService()](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java#L1046-L1225) 共约 180 行，按职责分为 **8 个阶段**：

#### 阶段 1：锁与基础引用（L1049-L1054）

```java
installLock(this, INDEX_WINDOW);          // 安装 Binder 调用锁（防止死锁）
mGlobalLock = atm.getGlobalLock();        // 共享 ATM 的全局锁
mAtmService = atm;                        // ActivityTaskManager 引用
mContext = context;                       // 系统上下文
mAllowBootMessages = showBootMsgs;        // 是否显示启动消息
mOnlyCore = onlyCore;                     // 核心模式标志
```

> **设计要点**：WMS 与 ATM 共享同一把 `mGlobalLock`，保证窗口操作与 Activity 操作的原子性，避免跨锁死锁。

#### 阶段 2：资源配置读取（L1055-L1072）

从系统资源（`config.xml`）中读取窗口管理相关的配置：

| 成员变量 | 资源 ID | 说明 |
|----------|---------|------|
| `mLimitedAlphaCompositing` | `config_sf_limitedAlpha` | 是否限制 Alpha 合成 |
| `mHasPermanentDpad` | `config_hasPermanentDpad` | 是否有永久方向键 |
| `mInTouchMode` | `config_defaultInTouchMode` | 默认是否触摸模式 |
| `mDrawLockTimeoutMillis` | `config_drawLockTimeoutMillis` | 绘制锁超时 |
| `mAllowAnimationsInLowPowerMode` | `config_allowAnimationsInLowPowerMode` | 省电模式允许动画 |
| `mMaxUiWidth` | `config_maxUiWidth` | UI 最大宽度限制 |
| `mDisableTransitionAnimation` | `config_disableTransitionAnimation` | 禁用过渡动画 |
| `mPerDisplayFocusEnabled` | `config_perDisplayFocusEnabled` | 每屏独立焦点 |
| `mLowRamTaskSnapshotsAndRecents` | `config_lowRamTaskSnapshotsAndRecents` | 低内存任务快照 |

#### 阶段 3：核心组件创建（L1073-L1087）

```java
mInputManager = inputManager;              // IMS 引用（必须在 createDisplayContent 之前）
mDisplayManagerInternal = LocalServices.getService(DisplayManagerInternal.class);
mDisplayWindowSettings = new DisplayWindowSettings(this);   // 显示窗口设置持久化

mTransactionFactory = transactionFactory;
mTransaction = mTransactionFactory.make(); // SurfaceFlinger 事务对象
mPolicy = policy;                          // PhoneWindowManager 策略
mAnimator = new WindowAnimator(this);      // 窗口动画管理器
mRoot = new RootWindowContainer(this);     // ★ 窗口容器树根节点

mWindowPlacerLocked = new WindowSurfacePlacer(this);       // 窗口 Surface 布局器
mTaskSnapshotController = new TaskSnapshotController(this); // 任务快照控制器
mWindowTracing = WindowTracing.createDefaultAndStartLooper(...); // 窗口追踪调试
```

**核心数据结构 — 窗口容器树：**

```
RootWindowContainer (mRoot)
    └── DisplayContent (默认屏幕)
            └── TaskStack
                    └── Task
                            └── AppWindowToken
                                    └── WindowState (实际窗口)
```

- [RootWindowContainer](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/wm/RootWindowContainer.java#L155-L158)：容器树根节点，管理所有 DisplayContent，负责焦点更新和遍历操作
- [WindowAnimator](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/wm/WindowAnimator.java#L88-L101)：在 AnimationThread 中运行，通过 Choreographer 驱动窗口动画帧回调
- [WindowSurfacePlacer](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/wm/WindowSurfacePlacer.java#L58-L65)：负责窗口 Surface 的布局计算和放置（`performSurfacePlacement`）
- [TaskSnapshotController](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/wm/TaskSnapshotController.java#L117-L130)：管理任务快照（最近任务缩略图的截图、持久化、缓存）

#### 阶段 4：系统服务获取（L1089-L1128）

```java
LocalServices.addService(WindowManagerPolicy.class, mPolicy);  // 注册窗口策略

mDisplayManager = (DisplayManager) context.getSystemService(Context.DISPLAY_SERVICE);
mKeyguardDisableHandler = KeyguardDisableHandler.create(...);  // 键盘锁禁用处理

mPowerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
mPowerManagerInternal = LocalServices.getService(PowerManagerInternal.class);

mActivityManager = ActivityManager.getService();               // AMS Binder 代理
mActivityTaskManager = ActivityTaskManager.getService();       // ATM Binder 代理
mAmInternal = LocalServices.getService(ActivityManagerInternal.class);
mAtmInternal = LocalServices.getService(ActivityTaskManagerInternal.class);
mAppOps = (AppOpsManager) context.getSystemService(Context.APP_OPS_SERVICE);
mPmInternal = LocalServices.getService(PackageManagerInternal.class);
```

#### 阶段 5：省电模式监听（L1098-L1119）

```java
mPowerManagerInternal.registerLowPowerModeObserver(result -> {
    synchronized (mGlobalLock) {
        if (mAnimationsDisabled != enabled && !mAllowAnimationsInLowPowerMode) {
            mAnimationsDisabled = enabled;
            dispatchNewAnimatorScaleLocked(null);  // 动态调整全局动画缩放
        }
    }
});
```

当设备进入省电模式时，自动将动画缩放设为 0（除非配置允许省电模式下播放动画）。

#### 阶段 6：WakeLock 与权限监控（L1120-L1152）

**WakeLock 管理：**
- `mScreenFrozenLock`（PARTIAL_WAKE_LOCK）— 屏幕冻结期间保持 CPU 唤醒
- `mHoldingScreenWakeLock`（SCREEN_BRIGHT_WAKE_LOCK）— 保持屏幕亮起

**AppOps 权限监控：**
```java
mAppOps.startWatchingMode(OP_SYSTEM_ALERT_WINDOW, null, opListener);  // 悬浮窗权限
mAppOps.startWatchingMode(AppOpsManager.OP_TOAST_WINDOW, null, opListener); // Toast 窗口权限
```

当应用的悬浮窗或 Toast 权限发生变化时，触发 `updateAppOpsState()` 重新评估窗口可见性。

**包挂起广播监听：**
```java
context.registerReceiverAsUser(new BroadcastReceiver() {
    public void onReceive(Context context, Intent intent) {
        updateHiddenWhileSuspendedState(...);  // 更新被挂起包的窗口隐藏状态
    }
}, UserHandle.ALL, suspendPackagesFilter, null, null);
```

#### 阶段 7：设置读取与广播注册（L1154-L1191）

**动画设置读取：**
```java
mWindowAnimationScaleSetting = Settings.Global.getFloat(resolver,
        Settings.Global.WINDOW_ANIMATION_SCALE, ...);        // 窗口动画缩放
mTransitionAnimationScaleSetting = Settings.Global.getFloat(resolver,
        Settings.Global.TRANSITION_ANIMATION_SCALE, ...);    // 过渡动画缩放
setAnimatorDurationScale(Settings.Global.getFloat(resolver,
        Settings.Global.ANIMATOR_DURATION_SCALE, ...));      // 动画时长缩放
```

**其他设置：**
- `mForceDesktopModeOnExternalDisplays` — 外接屏幕强制桌面模式（开发者选项）

**广播注册：**
- `ACTION_DEVICE_POLICY_MANAGER_STATE_CHANGED` — 设备策略管理器状态变化（启用/禁用键盘锁）

**交互控制器：**
- `mTaskPositioningController` — 任务窗口拖拽定位控制
- `mDragDropController` — 拖放操作控制
- `mHighRefreshRateBlacklist` — 高刷新率黑名单

#### 阶段 8：系统手势与本地服务（L1193-L1224）

**系统手势排除区域：**
```java
mSystemGestureExclusionLimitDp = Math.max(MIN_GESTURE_EXCLUSION_LIMIT_DP,
        DeviceConfig.getInt(..., KEY_SYSTEM_GESTURE_EXCLUSION_LIMIT_DP, 0));
```

从 `DeviceConfig` 读取手势排除区域限制（dp），并注册动态配置变更监听器，当限制值变化时遍历所有屏幕更新排除区域。

**注册本地服务：**
```java
LocalServices.addService(WindowManagerInternal.class, new LocalService());
```

---

### 四、onInitReady() — 就绪初始化

[onInitReady()](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java#L1231-L1245)

在 SystemServer 中 `AMS.setWindowManager(wm)` 之后调用：

```java
public void onInitReady() {
    initPolicy();                              // ① 初始化窗口策略

    Watchdog.getInstance().addMonitor(this);   // ② 注册 Watchdog 监控

    openSurfaceTransaction();
    try {
        createWatermarkInTransaction();        // ③ 创建水印（调试版本标识）
    } finally {
        closeSurfaceTransaction("createWatermarkInTransaction");
    }

    showEmulatorDisplayOverlayIfNeeded();      // ④ 模拟器叠加层
}
```

| 步骤 | 方法 | 说明 |
|------|------|------|
| ① | [initPolicy()](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java#L1020-L1028) | 在 **UiThread** 中调用 `PhoneWindowManager.init()`，初始化窗口策略（拦截器、布局规则等） |
| ② | `Watchdog.addMonitor` | 将 WMS 注册为 Watchdog 监控对象，定期检测是否死锁 |
| ③ | `createWatermarkInTransaction` | 在屏幕上创建水印 Surface（显示构建信息，仅 userdebug/eng 版本） |
| ④ | `showEmulatorDisplayOverlayIfNeeded` | 模拟器环境下显示叠加层 |

---

### 五、displayReady() — 显示就绪

[displayReady()](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java#L4494-L4514)

在 SystemServer 中所有显示配置完成后调用：

```java
public void displayReady() {
    synchronized (mGlobalLock) {
        if (mMaxUiWidth > 0) {
            mRoot.forAllDisplays(dc -> dc.setMaxUiWidth(mMaxUiWidth));  // 设置最大 UI 宽度
        }
        final boolean changed = applyForcedPropertiesForDefaultDisplay(); // 应用默认显示属性
        mAnimator.ready();                    // 动画管理器就绪
        mDisplayReady = true;                 // ★ 标记显示就绪（此后才允许 addWindow）
        if (changed) {
            reconfigureDisplayLocked(getDefaultDisplayContentLocked()); // 重新配置默认显示
        }
        mIsTouchDevice = mContext.getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_TOUCHSCREEN);
    }
    mActivityTaskManager.updateConfiguration(null); // 通知 ATM 更新配置
    updateCircularDisplayMaskIfNeeded();            // 更新圆角显示遮罩
}
```

---

### 六、完整初始化流程图

```
SystemServer.startOtherServices()
    │
    ├── ① new InputManagerService(context)
    │
    ├── ② WindowManagerService.main(context, im, ...)
    │       │
    │       └── DisplayThread.runWithScissors(() -> new WindowManagerService(...))
    │               │
    │               ├── 锁与基础引用 ── installLock, mGlobalLock(共享ATM)
    │               │
    │               ├── 资源配置读取 ── Alpha合成/方向键/触摸模式/动画/最大宽度等
    │               │
    │               ├── 核心组件创建
    │               │       ├── InputManager 引用
    │               │       ├── SurfaceControl.Transaction
    │               │       ├── PhoneWindowManager (策略)
    │               │       ├── WindowAnimator (动画管理器)
    │               │       ├── RootWindowContainer (容器树根) ★
    │               │       ├── WindowSurfacePlacer (Surface布局)
    │               │       ├── TaskSnapshotController (任务快照)
    │               │       └── WindowTracing (追踪调试)
    │               │
    │               ├── 系统服务获取 ── DisplayManager/PowerManager/AMS/ATM/AppOps/PMS
    │               │
    │               ├── 省电模式监听 ── 注册 LowPowerModeObserver
    │               │
    │               ├── WakeLock 管理 ── ScreenFrozenLock + HoldingScreenWakeLock
    │               │
    │               ├── 权限监控 ── AppOps(悬浮窗/Toast) + 包挂起广播
    │               │
    │               ├── 设置读取 ── 动画缩放/桌面模式/设备策略广播
    │               │
    │               ├── 交互控制 ── 拖拽/定位/高刷黑名单/手势排除
    │               │
    │               └── 本地服务注册 ── WindowManagerInternal
    │
    ├── ③ ServiceManager.addService(WINDOW_SERVICE, wm)
    │
    ├── ④ ServiceManager.addService(INPUT_SERVICE, im)
    │
    ├── ⑤ AMS.setWindowManager(wm)
    │       └── mWindowManager = wm; ATM.setWindowManager(wm)
    │
    ├── ⑥ wm.onInitReady()
    │       ├── initPolicy() → PhoneWindowManager.init() [UiThread]
    │       ├── Watchdog.addMonitor(this)
    │       ├── createWatermarkInTransaction()
    │       └── showEmulatorDisplayOverlayIfNeeded()
    │
    │   ... (SystemServer 继续其他初始化) ...
    │
    └── ⑦ wm.displayReady()
            ├── 设置最大 UI 宽度
            ├── 应用默认显示属性
            ├── mAnimator.ready()
            ├── mDisplayReady = true  ← 此后允许 addWindow
            ├── 重新配置默认显示
            └── 通知 ATM 更新配置
```

---

### 七、WMS 与 IMS 的协作关系

```
                    ┌─────────────────────────────────────┐
                    │           SystemServer              │
                    │                                     │
                    │  IMS ──────────→ WMS               │
                    │  (InputManager)   (WindowManager)   │
                    │       │               │             │
                    │       │  共享 DisplayThread          │
                    │       │  共享 mGlobalLock (via ATM)  │
                    │       │               │             │
                    │       ▼               ▼             │
                    │  ┌─────────────────────────┐       │
                    │  │   PhoneWindowManager     │       │
                    │  │   (窗口策略实现)          │       │
                    │  └─────────────────────────┘       │
                    └─────────────────────────────────────┘
```

- **IMS 先于 WMS 创建**：WMS 构造方法中需要 IMS 引用来处理输入事件分发
- **共享 DisplayThread**：两者都在 DisplayThread 中创建和运行，保证低延迟
- **通过 PhoneWindowManager 协作**：IMS 通过 `WindowManagerCallbacks` 回调 WMS，WMS 通过 `PhoneWindowManager` 处理拦截逻辑（如电源键、Home 键）
- **共享全局锁**：WMS 通过 `atm.getGlobalLock()` 与 ATM 共享同一把锁，保证窗口操作与 Activity 操作的原子性

**总结**：WMS 的初始化是一个**分阶段、跨线程**的过程。构造方法在 DisplayThread 中完成核心组件创建和系统服务关联；`onInitReady()` 在 UiThread 中完成策略初始化；`displayReady()` 最终标记显示系统就绪。整个过程中，WMS 建立了以 `RootWindowContainer` 为核心的窗口容器树，并与 IMS、AMS、PowerManager 等系统服务建立了紧密的协作关系。
