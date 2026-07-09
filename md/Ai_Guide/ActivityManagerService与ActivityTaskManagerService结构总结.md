# ActivityManagerService / ActivityTaskManagerService 结构总结

> 基于 cells-android10 项目 Android 10 源码整理
> 路径：`frameworks/base/services/core/java/com/android/server/am/wm`

---

## 目录

1. [整体定位与架构演进](#一整体定位与架构演进)
2. [ActivityManagerService（AMS）](#二activitymanagerserviceams)
3. [ActivityTaskManagerService（ATMS）](#三activitytaskmanagerserviceatms)
4. [两者协作关系](#四两者协作关系)
5. [核心类](#五核心类)
6. [核心成员字段详细展开](#六核心成员字段详细展开)
7. [树状展开](#七树状展开)
8. [启动链路](#八启动链路)

---

## 一、整体定位与架构演进

在 **Android 10** 中，Google 将原来 AMS 中与 Activity 任务栈、窗口模式相关的逻辑剥离出来，形成了独立的 **ActivityTaskManagerService**。这是 Android 9→10 最大的架构重构之一。

```
Android 9 之前:  AMS 承载所有工作 (Activity、Service、Broadcast、Process、OOM...)
Android 10 开始: AMS ──┐
                       ├── 进程/Service/Broadcast/ContentProvider/内存管理
                    ATMS ──  Activity/Stack/Task/Display/窗口模式 管理
```

两者通过 **双向内部接口** 解耦协作：
- AMS 持有 `ActivityTaskManagerInternal` → 调用 ATMS 的内部能力
- ATMS 持有 `ActivityManagerInternal` → 调用 AMS 的内部能力

---

## 二、ActivityManagerService（AMS）

### 2.1 基本声明

| 项目 | 内容 |
|------|------|
| **文件** | `frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java` |
| **行数** | ~19124 行 |
| **类声明** | `public class ActivityManagerService extends IActivityManager.Stub implements Watchdog.Monitor, BatteryStatsImpl.BatteryCallback` |
| **Binder 服务名** | `"activity"` (`Context.ACTIVITY_SERVICE`) |
| **生命周期管理** | 内部类 `Lifecycle extends SystemService` |

### 2.2 核心职责

- **进程管理**：进程创建、启动、调度、Kill、OOM Adj 计算
- **四大组件管理**（除 Activity 外）：
  - **Service** → `ActiveServices`
  - **BroadcastReceiver** → `BroadcastQueue`（FG/BG/Offload 三个队列）
  - **ContentProvider** → `ProviderMap`
- **内存管理**：Low Memory Killer、OOM 调整
- **BatteryStats** 统计
- **ANR / Crash 对话框**
- **权限检查、Intent 防火墙**

### 2.3 核心成员字段结构

```
ActivityManagerService
├── ■ 进程相关
│   ├── ProcessList mProcessList               // 全局进程列表
│   ├── PidMap mPidsSelfLocked                 // PID→ProcessRecord 映射
│   ├── OomAdjuster mOomAdjuster              // OOM 调整器
│   └── LowMemDetector mLowMemDetector        // 低内存检测
│
├── ■ 组件管理
│   ├── ActiveServices mServices               // Service 管理
│   ├── ProviderMap mProviderMap               // ContentProvider 管理
│   ├── BroadcastQueue mFgBroadcastQueue       // 前台广播队列
│   ├── BroadcastQueue mBgBroadcastQueue       // 后台广播队列
│   └── BroadcastQueue mOffloadBroadcastQueue  // 卸载广播队列
│
├── ■ 统计与监控
│   ├── BatteryStatsService mBatteryStatsService
│   ├── ProcessStatsService mProcessStats
│   ├── ProcessCpuTracker mProcessCpuTracker
│   ├── AppErrors mAppErrors                   // ANR/Crash 处理
│   └── PackageWatchdog mPackageWatchdog
│
├── ■ 对 ATMS 引用
│   ├── ActivityTaskManagerService mActivityTaskManager    // ATMS 实例
│   └── ActivityTaskManagerInternal mAtmInternal           // ATMS 内部接口
│
├── ■ 其他
│   ├── UserController mUserController
│   ├── PendingIntentController mPendingIntentController
│   ├── IntentFirewall mIntentFirewall
│   ├── ActivityManagerConstants mConstants
│   ├── AppOpsService mAppOpsService
│   └── WindowManagerService mWindowManager
│
└── ■ 内部类
    ├── Lifecycle extends SystemService        // 系统服务生命周期
    ├── LocalService extends ActivityManagerInternal  // 供 ATMS 调用的内部 API
    ├── MainHandler / UiHandler                // 消息处理
    ├── AppDeathRecipient                      // Binder 死亡监听
    └── PidMap / ProfileData / ProcessChangeItem ...
```

### 2.4 启动流程

1. **SystemServer** 先启动 ATMS，再通过 `Lifecycle.startService(ssm, atm)` 启动 AMS
2. 构造函数中将 `sAtm`（静态变量）传入 AMS
3. `AMS` 构造中调用 `mActivityTaskManager.initialize(...)` 初始化 ATMS
4. `start()` → 启动 ProcessCpuThread、发布 BatteryStats/AppOps、注册 `ActivityManagerInternal`

---

## 三、ActivityTaskManagerService（ATMS）

### 3.1 基本声明

| 项目 | 内容 |
|------|------|
| **文件** | `frameworks/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java` |
| **行数** | ~7488 行 |
| **类声明** | `public class ActivityTaskManagerService extends IActivityTaskManager.Stub` |
| **Binder 服务名** | `"activity_task"` (`Context.ACTIVITY_TASK_SERVICE`) |
| **生命周期管理** | 内部类 `Lifecycle extends SystemService` |

### 3.2 核心职责

- **Activity 启动**：`startActivity` / `startActivities` / `startActivityAsUser`
- **任务栈（Task/Stack）管理**：Task 创建/移除/移动、Stack 调整
- **多窗口支持**：分屏、画中画(PiP)、自由窗口、多显示器
- **Recent Tasks**（最近任务列表）
- **锁定任务模式（LockTask）**
- **屏幕方向/Configuration 变更处理**
- **Voice Interaction**（语音交互会话）
- **Keyguard 控制**
- **应用切换控制（App Switch）**

### 3.3 核心成员字段结构

```
ActivityTaskManagerService
├── ■ 核心容器
│   ├── RootActivityContainer mRootActivityContainer   // 根容器(管理所有 Display)
│   ├── ActivityStackSupervisor mStackSupervisor       // Stack 监管者
│   └── WindowManagerService mWindowManager            // WMS 引用
│
├── ■ 控制器
│   ├── ActivityStartController mActivityStartController   // Activity 启动控制器
│   ├── LockTaskController mLockTaskController             // 锁定任务
│   ├── KeyguardController mKeyguardController             // 锁屏
│   ├── VrController mVrController                         // VR 控制
│   └── PendingIntentController mPendingIntentController
│
├── ■ 任务相关
│   ├── RecentTasks mRecentTasks                        // 最近任务
│   ├── TaskChangeNotificationController mTaskChangeNotificationController
│   └── ClientLifecycleManager mLifecycleManager        // 客户端生命周期
│
├── ■ 进程（窗口视角）
│   ├── ProcessMap<WindowProcessController> mProcessNames   // 进程名映射
│   ├── WindowProcessControllerMap mProcessMap              // PID→进程
│   ├── WindowProcessController mHomeProcess                // Home 进程
│   └── WindowProcessController mPreviousProcess            // 前一个进程
│
├── ■ 多窗口支持
│   ├── boolean mSupportsMultiWindow
│   ├── boolean mSupportsSplitScreenMultiWindow
│   ├── boolean mSupportsFreeformWindowManagement
│   ├── boolean mSupportsPictureInPicture
│   └── boolean mSupportsMultiDisplay
│
├── ■ 对 AMS 的引用
│   ├── ActivityManagerInternal mAmInternal            // AMS 内部接口
│   └── UriGrantsManagerInternal mUgmInternal
│
├── ■ 其他
│   ├── AssistUtils mAssistUtils                       // Voice/Assist
│   ├── CompatModePackages mCompatModePackages         // 兼容模式
│   ├── AppWarnings mAppWarnings                       // 应用警告
│   ├── FontScaleSettingObserver                      // 字体缩放监听
│   └── ActivityRecord mLastResumedActivity            // 最后恢复的 Activity
│
└── ■ 内部类
    ├── Lifecycle extends SystemService                // 生命周期
    ├── LocalService extends ActivityTaskManagerInternal  // 供 AMS 调用的内部 API
    ├── H extends Handler                               // 主消息处理
    ├── UiHandler                                       // UI 消息处理
    └── UpdateConfigurationResult
```

### 3.4 启动流程

1. ATMS 由 **SystemServer** 首先启动：`ActivityTaskManagerService.Lifecycle` → `onStart()` → `publishBinderService` + `start()`
2. 随后 AMS 通过 `startService(ssm, atm)` 获取 ATMS 引用
3. ATMS 的 `initialize()` 创建 `ActivityStackSupervisor`、`RootActivityContainer`、`RecentTasks`、`LockTaskController` 等

---

## 四、两者协作关系

```
                    ┌──────────────────────┐
                    │     SystemServer      │
                    │  先启动 ATMS, 后 AMS   │
                    └──────┬───────┬────────┘
                           │       │
              ┌────────────▼──┐ ┌──▼───────────┐
              │  AMS (am包)    │ │  ATMS (wm包)  │
              │                │ │               │
              │ ProcessRecord──┼─┼►WindowProcessController
              │ ProcessList    │ │               │
              │ ActiveServices │ │ ActivityStack │
              │ BroadcastQueue │ │   ├─TaskRecord│
              │ ProviderMap    │ │   └─ActivityRecord
              │ OomAdjuster    │ │ TaskStack     │
              │ UserController │ │   └─Task      │
              │                │ │ ActivityDisplay│
              │ mAtmInternal ──┼─┼► LocalService  │
              │                ◄┼─ mAmInternal    │
              └────────────────┘ └───────────────┘
                       │                   │
                       ▼                   ▼
                IActivityManager    IActivityTaskManager
                (Binder Service)    (Binder Service)
```

**关键要点：**
1. **职责分离**：AMS 聚焦"进程 + 非 Activity 组件"，ATMS 聚焦"Activity + 窗口容器"
2. **锁机制**：ATMS 使用 `WindowManagerGlobalLock`（与 WMS 共享同一把锁）
3. **层级容器模型**：`RootActivityContainer` → `ActivityDisplay` → `ActivityStack` → `TaskRecord` → `ActivityRecord`
4. **进程双视角**：AMS 用 `ProcessRecord` 管理进程，ATMS 用 `WindowProcessController` 管理进程的窗口关系
5. **热路径优化**：ATMS 中标注了 `@HotPath` 注解，区分 OOM 调整、LRU 更新、进程变更等路径

---

## 五、核心类

### 5.1 ATMS 侧核心类（com.android.server.wm 包）

#### 容器层级体系

```
ConfigurationContainer<E>          ← 抽象基类，管理 Configuration 变更
  └── WindowContainer<E>           ← 窗口容器基类，父子层级管理、Surface 管理
        ├── Task                  ← AppWindowToken 容器（一个 Activity 的所有窗口）
        ├── TaskStack             ← Task 容器
        ├── ActivityStack         ← ActivityRecord 容器（Pause/Resume 等逻辑）
        ├── ActivityDisplay       ← 绑定到一个 Display 的 Stack 容器
        └── RootActivityContainer ← 根容器，管理所有 Display
```

| 类名 | 文件 | 行数 | 继承 | 职责 |
|------|------|------|------|------|
| `ConfigurationContainer` | `ConfigurationContainer.java` | ~53行起 | `abstract class` | 配置变更传递（基类） |
| `WindowContainer` | `WindowContainer.java` | ~64行起 | `extends ConfigurationContainer` | 窗口容器核心（层级/动画/Surface） |
| `Task` | `Task.java` | 809 | `extends WindowContainer<AppWindowToken>` | 一个 Activity 的所有窗口令牌 |
| `TaskStack` | `TaskStack.java` | 1972 | `extends WindowContainer<Task>` | Task 的窗口容器（与 TaskRecord 配对） |
| `ActivityRecord` | `ActivityRecord.java` | 3913 | `extends ConfigurationContainer` | Activity 的详细状态记录（核心数据结构） |
| `TaskRecord` | `TaskRecord.java` | 2989 | `extends ConfigurationContainer` | 任务记录（容纳多个 ActivityRecord） |
| `ActivityStack` | `ActivityStack.java` | 5794 | `extends ConfigurationContainer` | Activity 栈（管理 TaskRecord，核心调度逻辑） |
| `ActivityDisplay` | `ActivityDisplay.java` | 1512 | `extends ConfigurationContainer<ActivityStack>` | 单个 Display 上的所有 Stack |
| `RootActivityContainer` | `RootActivityContainer.java` | 2473 | `extends ConfigurationContainer` | 根容器（多 Display 管理入口） |

#### 管理与控制类

| 类名 | 文件 | 行数 | 职责 |
|------|------|------|------|
| `ActivityTaskManagerService` | `ActivityTaskManagerService.java` | 7487 | ATMS 本身（Binder 服务端，外部入口） |
| `ActivityStackSupervisor` | `ActivityStackSupervisor.java` | 2879 | Stack 监管者（调度、Resume 控制、Home 管理） |
| `ActivityStarter` | `ActivityStarter.java` | 3005 | Activity 启动流程的具体执行（核心链路） |
| `ActivityStartController` | `ActivityStartController.java` | 539 | 启动请求的入口控制和委托 |
| `ActivityStartInterceptor` | `ActivityStartInterceptor.java` | 341 | 启动拦截（权限/用户切换等） |
| `LockTaskController` | `LockTaskController.java` | ~88行起 | 锁定任务模式控制 |
| `KeyguardController` | `KeyguardController.java` | ~62行起 | 锁屏状态控制 |
| `VrController` | `VrController.java` | — | VR 模式控制 |
| `TaskChangeNotificationController` | `TaskChangeNotificationController.java` | 535 | Task 变化通知分发 |

#### 进程（窗口视角）

| 类名 | 文件 | 行数 | 继承 | 职责 |
|------|------|------|------|------|
| `WindowProcessController` | `WindowProcessController.java` | 1140 | `extends ConfigurationContainer` | 进程在窗口侧的表示（与 AMS 的 ProcessRecord 对应） |
| `WindowProcessListener` | `WindowProcessListener.java` | 75 | `interface` | 进程状态变更监听回调 |

### 5.2 AMS 侧核心类（com.android.server.am 包）

| 类名 | 文件 | 行数 | 职责 |
|------|------|------|------|
| `ActivityManagerService` | `ActivityManagerService.java` | 19124 | AMS 本身（Binder 服务端，外部入口） |
| `ProcessRecord` | `ProcessRecord.java` | 1653 | 进程记录（实现 `WindowProcessListener`，与 ATMS 侧 `WindowProcessController` 对应） |
| `ActiveServices` | `ActiveServices.java` | 4481 | Service 组件管理（启动/绑定/停止） |
| `BroadcastQueue` | `BroadcastQueue.java` | 2103 | 广播分发队列（串行/并行处理） |
| `ProviderMap` | `ProviderMap.java` | 455 | ContentProvider 映射表 |
| `OomAdjuster` | `OomAdjuster.java` | 2113 | OOM Adj 计算与进程优先级调整 |
| `ProcessList` | `ProcessList.java` | — | 进程列表管理（启动新进程等） |
| `UserController` | `UserController.java` | 2429 | 多用户管理（用户切换/启动/停止） |
| `AppErrors` | `AppErrors.java` | — | ANR / Crash 对话框处理 |

### 5.3 内部接口（解耦桥梁）

| 接口 | 位于 | 实现者 | 调用方 |
|------|------|--------|--------|
| `IActivityManager.Stub` | `android.app` | AMS | 外部 App / ATMS |
| `IActivityTaskManager.Stub` | `android.app` | ATMS | 外部 App / AMS |
| `ActivityManagerInternal` | `com.android.server.am` | `AMS.LocalService` | ATMS (通过 `mAmInternal`) |
| `ActivityTaskManagerInternal` | `com.android.server.wm` | `ATMS.LocalService` | AMS (通过 `mAtmInternal`) |
| `WindowProcessListener` | `com.android.server.wm` | `ProcessRecord` | ATMS (WPC 回调到 AMS) |
