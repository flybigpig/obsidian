# Android 10 WMS 与窗口策略（PhoneWindowManager）树状图核心分析

> 导出时间：2026-07-09
> 工作目录：`c:\D\android_project\cells-android10`（Android 10）
> 涉及文件：
> - `frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java`
> - `frameworks/base/services/core/java/com/android/server/policy/PhoneWindowManager.java`
>
> **关于 "PWS"**：源码中无 `PWS` 字面符号。与 WMS 配套、作为窗口策略中枢的是
> `PhoneWindowManager`（实现 `WindowManagerPolicy`），在 WMS 中以字段 `mPolicy` 持有，
> 构造期调用 `mPolicy.init(...)`（:1025）完成绑定。本文以该组件对应 "PWS"。

---

# 一、WMS（WindowManagerService）

## 1.1 类声明与定位

```
:295  public class WindowManagerService extends IWindowManager.Stub
      implements Watchdog.Monitor, ...
```

- **Binder 接口**：`IWindowManager.Stub` —— 对外（App 进程、ATMS、AMS）暴露窗口增删、布局、焦点、旋转等能力
- **职责**：窗口（WindowState）全生命周期、Surface 层级与布局、焦点管理、输入窗口同步、屏幕旋转、截图（TaskSnapshot）、拖拽、Keyguard 开关、与 ATMS/AMS 协作
- **全局锁**：`mGlobalLock`（`WindowManagerGlobalLock`）—— 与 ATMS 共用，避免死锁（见前文 AMS/ATMS 分析）

## 1.2 WMS 树状图

```
WindowManagerService (wm)
├── 类声明 :295  extends IWindowManager.Stub, implements Watchdog.Monitor
│
├── 内部类 / 子结构
│   ├── SettingsObserver :699           // 监听系统设置（策略控制、沉浸模式）
│   ├── H :4574                          // 主线程 Handler（动画缩放、配置等消息）
│   ├── MousePositionTracker :6725      // 指针位置跟踪（PointerEventListener）
│   ├── LocalService :7166               // 实现 WindowManagerInternal（供 framework 内部）
│   └── WindowState / WindowToken / AppWindowToken / DisplayContent
│       （均为 wm 包内独立类，由 WMS 通过 mRoot / mWindowMap 统一管理）
│
├── 核心字段（窗口与系统视图）
│   ├── mRoot :603                      RootWindowContainer    // 所有 Display 的根容器
│   ├── mWindowMap :507                 WindowHashMap          // 全部 WindowState（按 IBinder）
│   ├── mSessions :504                  ArraySet<Session>      // 各客户端 Session
│   ├── mPolicy :485                    WindowManagerPolicy    // ← PWS（PhoneWindowManager）
│   ├── mGlobalLock :510                WindowManagerGlobalLock// 全局锁
│   ├── mWindowPlacerLocked :432        WindowSurfacePlacer    // 执行布局与层级放置
│   ├── mAnimator :835                  WindowAnimator         // 动画驱动
│   ├── mSurfaceAnimationRunner :836    SurfaceAnimationRunner // Surface 动画
│   ├── mTaskSnapshotController :669    TaskSnapshotController // 任务截图
│   ├── mDragDropController :823        DragDropController     // 拖拽
│   ├── mKeyguardDisableHandler :402    KeyguardDisableHandler // Keyguard 禁用
│   ├── mWindowTracing :400             WindowTracing          // 窗口追踪（Proto 日志）
│   ├── mInputManager :806              InputManagerService    // 输入
│   ├── mDisplayManagerInternal :807    DisplayManagerInternal // 显示
│   ├── mAmInternal :490                ActivityManagerInternal// 反向调 AMS
│   ├── mAtmInternal :491               ActivityTaskManagerInternal // 反向调 ATMS
│   ├── mRotationWatchers :640          ArrayList<RotationWatcher>
│   ├── mHoldingScreenOn/WakeLock :816/:817  // 保持屏幕唤醒
│   └── mDisplayReady :585 / mSystemReady :592  // 启动阶段标志
│
└── 关键方法分区
    ├── W1 启动 / 生命周期
    │   ├── onInitReady :1231
    │   ├── displayReady :4494          // mDisplayReady=true
    │   └── systemReady :4517           // mSystemReady=true → mPolicy.systemReady() + 各 DisplayPolicy.systemReady()
    ├── W2 会话与窗口增删
    │   ├── openSession :5040           // 创建 IWindowSession（客户端入口）
    │   ├── addWindow :1276             // 添加窗口（核心入口）
    │   ├── removeWindow :1808          // 移除窗口
    │   └── removeWindowToken :2518 / :7339
    ├── W3 布局与重排
    │   ├── relayoutWindow :2027        // 重排（核心入口，计算大小/可见性）
    │   ├── requestTraversal :5442      // 请求一次遍历
    │   └── WindowSurfacePlacer.performSurfacePlacement（由 mWindowPlacerLocked 驱动）
    ├── W4 焦点
    │   └── updateFocusedWindowLocked :5455 → mRoot.updateFocusedWindowLocked(...)
    ├── W5 旋转 / 配置
    │   └── mPolicy + DisplayPolicy 协同（RotationWatcher）
    └── W6 协作回调
        ├── mAtmInternal.notifyAppTransitionCancelled() :934 / notifyAppTransitionFinished() :939
        └── mPolicy.init(...) :1025
```

## 1.3 WMS 核心职责

| 子系统 | 关键字段 / 类 | 负责内容 |
|--------|---------------|----------|
| 窗口容器层级 | `mRoot`(RootWindowContainer) + `DisplayContent` | 多屏、Stack、Task、App/WindowToken 的树形组织 |
| 窗口实例表 | `mWindowMap`(WindowHashMap) | 以 `IWindow` 的 IBinder 为键索引全部 `WindowState` |
| 客户端会话 | `mSessions`(Session) | 每个 App 进程一个 `Session`，是 `addWindow/relayoutWindow` 的调用方 |
| 策略决策 | `mPolicy`(PhoneWindowManager) | 窗口层级、可见性、按键拦截策略（见 PWS 节） |
| 布局与层级放置 | `mWindowPlacerLocked`(WindowSurfacePlacer) | 计算层级、执行 Surface 放置、动画编排 |
| 动画 | `mAnimator` + `mSurfaceAnimationRunner` | 窗口进出、转场、Surface 动画 |
| 截图 | `mTaskSnapshotController` | 任务快照（近期任务缩略、恢复） |
| 输入同步 | `mInputManager` | 将焦点/窗口层信息同步给 InputDispatcher |
| 旋转 | `mRotationWatchers` + DisplayPolicy | 屏幕方向变更与配置下发 |

---

# 二、PWS — PhoneWindowManager（窗口策略）

## 2.1 类声明与定位

```
:245  public class PhoneWindowManager implements WindowManagerPolicy
```

- **接口**：`WindowManagerPolicy`（WMS 通过 `mPolicy` 调用；`WindowManagerFuncs` 反向回调 WMS）
- **职责**：系统级按键拦截（电源、音量、Home、Back、Recent）、屏幕开关流程、Keyguard 协调、状态栏/导航栏控制、系统手势、全局动作菜单、旋转/配置策略、沉浸模式
- **与 WMS 关系**：WMS 持有 `mPolicy`；PWM 通过 `mWindowManagerFuncs`（即 WMS 自身，:1025 传入 `WindowManagerService.this`）回写

## 2.2 PhoneWindowManager 树状图

```
PhoneWindowManager (policy)
├── 类声明 :245  implements WindowManagerPolicy
│
├── 反向通道
│   └── mWindowManagerFuncs :366        // WindowManagerFuncs，实际就是 WMS 实例
│
├── 核心字段（系统服务句柄 + 显示策略）
│   ├── mContext :364                   Context
│   ├── mDefaultDisplayPolicy :538      DisplayPolicy        // 默认屏的布局/装饰策略
│   ├── mKeyguardDelegate :421          KeyguardServiceDelegate // Keyguard 代理
│   ├── mPowerManager :368 / mPowerManagerInternal :375
│   ├── mActivityManagerInternal :369   // 反向调 AMS
│   ├── mStatusBarService :376 / mStatusBarManagerInternal :377
│   ├── mDreamManagerInternal :374      // 屏保
│   ├── mDisplayManager :379 / mSearchManager :383 / mAccessibilityManager :384
│   ├── mAppOpsManager :387 / mUiModeManager :474
│   ├── mGlobalActions :438              // 关机/重启/飞行菜单
│   ├── mHandler :439                   // 主线程 Handler（MSG_* 消息）
│   └── mBurnInProtectionHelper :385
│
└── 关键方法分区
    ├── P1 初始化 / 生命周期
    │   ├── init(...) :1742             // WMS 构造期调用，绑定 context/wm/funcs
    │   └── systemReady :4844           // 启动完成后触发 startedWakingUp/screenTurningOn/on
    ├── P2 按键拦截（核心）
    │   ├── interceptKeyBeforeQueueing :3667   // 入队前拦截（电源/音量/Home/Back...）
    │   ├── interceptMotionBeforeQueueingNonInteractive :4194
    │   └── dispatchUnhandledKey :3137         // 未处理按键兜底分发
    ├── P3 屏幕开关流程
    │   ├── startedGoingToSleep :4449
    │   ├── startedWakingUp :4497
    │   ├── screenTurningOn :4600 → mDefaultDisplayPolicy.screenTurnedOn + Keyguard
    │   ├── screenTurnedOn :4622
    │   ├── screenTurningOff :4632 → mWindowManagerFuncs.screenTurningOff
    │   └── finishScreenTurningOn :4656
    ├── P4 全局动作 / 导航
    │   ├── showGlobalActions :1456 / showGlobalActionsInternal :1461
    │   └── showRecentApps :3417
    └── P5 配置 / 布局策略
        └── adjustConfigurationLw :2250  // 依据键盘/导航配置调整 Configuration
```

## 2.3 PhoneWindowManager 核心职责

| 子系统 | 关键方法 / 字段 | 负责内容 |
|--------|----------------|----------|
| 按键策略 | `interceptKeyBeforeQueueing` :3667 | 决定电源键长按=关机、Home=回桌面、Back=返回、Recent=多任务等 |
| 屏幕生命周期 | `startedWakingUp`/`screenTurningOn`/`screenTurnedOff` | 协调 Keyguard 绘制、屏幕点亮/熄灭时序 |
| Keyguard | `mKeyguardDelegate` :421 | 锁屏显示、解锁、occluded 状态 |
| 状态栏/导航栏 | `mDefaultDisplayPolicy`(DisplayPolicy) | 控制状态栏/导航栏可见性、沉浸式、手势区 |
| 系统菜单 | `showGlobalActions` :1456 | 长按电源弹出的关机/重启/截屏菜单 |
| 配置调整 | `adjustConfigurationLw` :2250 | 根据外设/导航模式调整 Configuration |
| 回写 WMS | `mWindowManagerFuncs`（WMS 实例） | `screenTurningOff`、`moveDisplayToTop`、`onPowerKeyDown` 等 |

---

# 三、WMS ↔ PWS 协作与关键流程

## 3.1 绑定（构造期）
```
WMS 构造 :1079  mPolicy = policy;
          :1025 mPolicy.init(mContext, WindowManagerService.this, WindowManagerService.this);
                → PhoneWindowManager.init(...) :1742
                  mWindowManagerFuncs = WindowManagerService.this   // 反向通道建立
```

## 3.2 添加窗口（addWindow 调用树）
```
App 进程 → Session.addToDisplay
  → WMS.addWindow :1276
      ├── 权限/类型校验（受 mPolicy 影响：如 TYPE_APPLICATION 层级）
      ├── new WindowState(...) 并放入 mWindowMap :507
      ├── mRoot.attachWindow(...)            // 挂入 RootWindowContainer 树
      ├── updateFocusedWindowLocked :1638/5455
      └── mWindowPlacerLocked 触发一次 performSurfacePlacement（计算层级/Surface）
```

## 3.3 重排与焦点（relayoutWindow 调用树）
```
Session.relayout
  → WMS.relayoutWindow :2027
      ├── 计算窗口尺寸/可见性（参考 mPolicy 的布局策略）
      ├── mRoot.performLayout(...)
      ├── updateFocusedWindowLocked :2253 → mRoot.updateFocusedWindowLocked
      └── requestTraversal :5442 → WindowSurfacePlacer 落地 Surface
```

## 3.4 电源键 → 关机菜单（策略拦截链）
```
InputDispatcher → mPolicy.interceptKeyBeforeQueueing :3667
  └── 电源键长按 → MSG_POWER_LONG_PRESS → mHandler
        └── showGlobalActionsInternal :1461 → mGlobalActions（关机/重启菜单）
```

## 3.5 屏幕点亮流程（WMS ↔ PWS ↔ Keyguard）
```
PowerManager → WMS 通知 → mPolicy.startedWakingUp :4497
  → mPolicy.screenTurningOn :4600
        ├── mDefaultDisplayPolicy.screenTurnedOn(...)   // 显示策略确认可绘
        └── mKeyguardDelegate.onScreenTurningOn(...)     // 锁屏先绘制
  → finishScreenTurningOn :4656 → 真正点亮
```

---

# 四、与 AMS / ATMS 的三角关系（回顾）

| 维度 | AMS | ATMS | WMS / PWS |
|------|-----|------|-----------|
| 锁 | —— | `mGlobalLock`（共享） | `mGlobalLock`（共享，避免死锁） |
| 反向句柄 | `mAtmInternal` | `mAmInternal` | `mAmInternal`(:490) / `mAtmInternal`(:491) |
| 协作点 | 进程生死/OOM | Activity/Task 编排 | 窗口 Surface/焦点/布局 |
| 策略 | —— | —— | `mPolicy`=PhoneWindowManager |

关键共享：`mGlobalLock` 同时被 ATMS 与 WMS 持有，确保 activity 与 window 状态变更在同一临界区内完成，避免「Activity 已切换但窗口未更新」类竞态。

---

# 五、关键行号速查表

| 内容 | 文件 | 行号 |
|------|------|------|
| WMS 类声明 | WindowManagerService.java | :295 |
| mRoot 字段 | WindowManagerService.java | :603 |
| mWindowMap 字段 | WindowManagerService.java | :507 |
| mSessions 字段 | WindowManagerService.java | :504 |
| mPolicy 字段 | WindowManagerService.java | :485 |
| mGlobalLock 字段 | WindowManagerService.java | :510 |
| mWindowPlacerLocked | WindowManagerService.java | :432 |
| mAnimator | WindowManagerService.java | :835 |
| mTaskSnapshotController | WindowManagerService.java | :669 |
| mInputManager / mDisplayManagerInternal | WindowManagerService.java | :806 / :807 |
| onInitReady | WindowManagerService.java | :1231 |
| openSession | WindowManagerService.java | :5040 |
| addWindow | WindowManagerService.java | :1276 |
| removeWindow | WindowManagerService.java | :1808 |
| relayoutWindow | WindowManagerService.java | :2027 |
| displayReady | WindowManagerService.java | :4494 |
| systemReady | WindowManagerService.java | :4517 |
| updateFocusedWindowLocked | WindowManagerService.java | :5455 |
| requestTraversal | WindowManagerService.java | :5442 |
| SettingsObserver | WindowManagerService.java | :699 |
| H (Handler) | WindowManagerService.java | :4574 |
| LocalService (WindowManagerInternal) | WindowManagerService.java | :7166 |
| PWM 类声明 | PhoneWindowManager.java | :245 |
| mDefaultDisplayPolicy | PhoneWindowManager.java | :538 |
| mKeyguardDelegate | PhoneWindowManager.java | :421 |
| mWindowManagerFuncs | PhoneWindowManager.java | :366 |
| init(...) | PhoneWindowManager.java | :1742 |
| interceptKeyBeforeQueueing | PhoneWindowManager.java | :3667 |
| dispatchUnhandledKey | PhoneWindowManager.java | :3137 |
| showGlobalActions | PhoneWindowManager.java | :1456 |
| showRecentApps | PhoneWindowManager.java | :3417 |
| adjustConfigurationLw | PhoneWindowManager.java | :2250 |
| startedWakingUp | PhoneWindowManager.java | :4497 |
| screenTurningOn | PhoneWindowManager.java | :4600 |
| systemReady | PhoneWindowManager.java | :4844 |

---

*本文档由会话分析自动整理导出；行号基于 cells-android10（Android 10）源码快照。*
