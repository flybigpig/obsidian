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

---

## 六、核心成员字段详细展开

### 6.1 AMS 核心成员字段

```
ActivityManagerService extends IActivityManager.Stub
       implements Watchdog.Monitor, BatteryStatsImpl.BatteryCallback
```

#### 6.1.1 进程/内存管理

| 字段 | 类型 | 说明 |
|------|------|------|
| `mProcessList` | `ProcessList` | 全局进程列表管理器 |
| `mPidsSelfLocked` | `PidMap` | **PID→ProcessRecord** 映射（自锁） |
| `mOomAdjuster` | `OomAdjuster` | OOM Adj 计算与调整器 |
| `mLowMemDetector` | `LowMemDetector` | 低内存检测器 |
| `mOomAdjProfiler` | `OomAdjProfiler` | OOM Adj 性能分析 |
| `mAllowLowerMemLevel` | `boolean` | 是否允许下调内存级别 |
| `mLastMemoryLevel` | `int` | 上次计算的内存级别 |
| `mLastNumProcesses` | `int` | 上次进程总数 |

#### 6.1.2 四大组件管理

| 字段 | 类型 | 说明 |
|------|------|------|
| `mServices` | `ActiveServices` | Service 组件管理器 |
| `mFgBroadcastQueue` | `BroadcastQueue` | 前台广播队列 |
| `mBgBroadcastQueue` | `BroadcastQueue` | 后台广播队列 |
| `mOffloadBroadcastQueue` | `BroadcastQueue` | 卸载广播队列 |
| `mBroadcastQueues` | `BroadcastQueue[3]` | 三队列数组 |
| `mEnableOffloadQueue` | `boolean` | 是否启用卸载队列 |
| `mLastBroadcastStats` | `BroadcastStats` | 上一轮广播统计 |
| `mCurBroadcastStats` | `BroadcastStats` | 当前轮广播统计 |
| `mProviderMap` | `ProviderMap` | ContentProvider 映射表 |

#### 6.1.3 系统状态

| 字段 | 类型 | 说明 |
|------|------|------|
| `mSystemReady` | `volatile boolean` | 系统是否就绪 |
| `mProcessesReady` | `volatile boolean` | 进程系统是否就绪 |
| `mBooted` | `volatile boolean` | 是否已完成启动 |
| `mBootPhase` | `int` | 当前启动阶段 |
| `mBooting` | `volatile boolean` | 正在启动中 |
| `mFactoryTest` | `int` | 工厂测试模式 |
| `mSafeMode` | `boolean` | 安全模式 |
| `mWakefulness` | `int` | 设备唤醒状态 |
| `mOnBattery` | `volatile boolean` | 是否使用电池 |

#### 6.1.4 对其他服务的引用

| 字段 | 类型 | 说明 |
|------|------|------|
| `mActivityTaskManager` | `ActivityTaskManagerService` | **ATMS 实例** |
| `mAtmInternal` | `ActivityTaskManagerInternal` | **ATMS 内部接口** |
| `mWindowManager` | `WindowManagerService` | WMS 引用 |
| `mUgmInternal` | `UriGrantsManagerInternal` | URI 授权管理内部接口 |
| `mSystemServiceManager` | `SystemServiceManager` | 系统服务管理器 |
| `mBatteryStatsService` | `BatteryStatsService` | 电池统计服务 |
| `mUsageStatsService` | `UsageStatsManagerInternal` | 使用统计服务 |
| `mAppOpsService` | `AppOpsService` | AppOps 服务 |
| `mPackageManagerInt` | `PackageManagerInternal` | 包管理器内部接口 |
| `mLocalPowerManager` | `PowerManagerInternal` | 电源管理内部接口 |
| `mLocalDeviceIdleController` | `DeviceIdleController.LocalService` | Doze 模式 |

#### 6.1.5 Handler / Thread

| 字段 | 类型 | 说明 |
|------|------|------|
| `mHandlerThread` | `ServiceThread` | AMS 主线程 |
| `mHandler` | `MainHandler` | 主 Handler |
| `mUiHandler` | `UiHandler` | UI 线程 Handler |
| `mProcStartHandlerThread` | `ServiceThread` | 进程启动专用线程 |
| `mProcStartHandler` | `Handler` | 进程启动 Handler |
| `mProcessCpuThread` | `Thread` | CPU 采样线程 |
| `mProcessCpuTracker` | `ProcessCpuTracker` | 进程 CPU 追踪 |

#### 6.1.6 观察者/回调

| 字段 | 类型 | 说明 |
|------|------|------|
| `mProcessObservers` | `RemoteCallbackList<IProcessObserver>` | 进程状态观察者 |
| `mUidObservers` | `RemoteCallbackList<IUidObserver>` | UID 状态观察者 |
| `mActiveProcessChanges` | `ProcessChangeItem[5]` | 当前进程变更 |
| `mPendingProcessChanges` | `ArrayList<ProcessChangeItem>` | 待分发进程变更 |
| `mActiveUidChanges` | `UidRecord.ChangeItem[5]` | 当前 UID 变更 |
| `mPendingUidChanges` | `ArrayList<UidRecord.ChangeItem>` | 待分发 UID 变更 |
| `mCurOomAdjObserver` | `OomAdjObserver` | OOM Adj 观察者 |

#### 6.1.7 用户/权限/其他

| 字段 | 类型 | 说明 |
|------|------|------|
| `mUserController` | `UserController` | 多用户管理 |
| `mPendingIntentController` | `PendingIntentController` | PendingIntent 管理 |
| `mIntentFirewall` | `IntentFirewall` | Intent 防火墙 |
| `mAppErrors` | `AppErrors` | ANR/Crash 处理 |
| `mPackageWatchdog` | `PackageWatchdog` | 包看门狗 |
| `mConstants` | `ActivityManagerConstants` | 可调参数 |
| `mHiddenApiBlacklist` | `HiddenApiSettings` | Hidden API 黑名单 |
| `mProcessStats` | `ProcessStatsService` | 进程长期统计 |
| `mDeviceOwnerName` | `String` | DeviceOwner 包名 |
| `mProfileData` | `ProfileData` | Profiling 信息 |
| `mTrackingAssociations` | `boolean` | 是否追踪包关联 |

---

### 6.2 ATMS 核心成员字段

```
ActivityTaskManagerService extends IActivityTaskManager.Stub
```

#### 6.2.1 容器层级

| 字段 | 类型 | 说明 |
|------|------|------|
| `mRootActivityContainer` | `RootActivityContainer` | **根容器** |
| `mStackSupervisor` | `ActivityStackSupervisor` | **Stack 监管者** |

#### 6.2.2 控制器

| 字段 | 类型 | 说明 |
|------|------|------|
| `mActivityStartController` | `ActivityStartController` | Activity 启动入口控制 |
| `mLockTaskController` | `LockTaskController` | 锁定任务模式控制 |
| `mKeyguardController` | `KeyguardController` | 锁屏状态控制 |
| `mVrController` | `VrController` | VR 模式控制 |
| `mTaskChangeNotificationController` | `TaskChangeNotificationController` | Task 变化通知 |
| `mLifecycleManager` | `ClientLifecycleManager` | 客户端生命周期事务 |

#### 6.2.3 对外引用

| 字段 | 类型 | 说明 |
|------|------|------|
| `mAmInternal` | `ActivityManagerInternal` | **AMS 内部接口** |
| `mUgmInternal` | `UriGrantsManagerInternal` | URI 授权管理 |
| `mPmInternal` | `PackageManagerInternal` | 包管理 |
| `mWindowManager` | `WindowManagerService` | WMS 引用 |
| `mPowerManagerInternal` | `PowerManagerInternal` | 电源管理 |
| `mUsageStatsInternal` | `UsageStatsManagerInternal` | 使用统计 |

#### 6.2.4 进程相关（窗口视角）

| 字段 | 类型 | 说明 |
|------|------|------|
| `mProcessNames` | `ProcessMap<WindowProcessController>` | **进程名→WPC** 映射 |
| `mProcessMap` | `WindowProcessControllerMap` | **PID→WPC** 映射 |
| `mHomeProcess` | `WindowProcessController` | 当前 Home 进程 |
| `mHeavyWeightProcess` | `WindowProcessController` | 重量级进程 |
| `mPreviousProcess` | `WindowProcessController` | 用户上一个进程 |
| `mPreviousProcessVisibleTime` | `long` | 上一个进程最后可见时间 |
| `mActiveUids` | `MirrorActiveUids` | 活跃 UID 镜像 |
| `mPendingTempWhitelist` | `SparseArray<String>` | 待处理临时白名单 |

#### 6.2.5 任务/最近任务

| 字段 | 类型 | 说明 |
|------|------|------|
| `mRecentTasks` | `RecentTasks` | 最近任务列表 |
| `mPendingIntentController` | `PendingIntentController` | PendingIntent 管理 |
| `mIntentFirewall` | `IntentFirewall` | Intent 防火墙 |

#### 6.2.6 窗口/多窗口模式

| 字段 | 类型 | 说明 |
|------|------|------|
| `mSupportsMultiWindow` | `boolean` | 是否支持多窗口 |
| `mSupportsSplitScreenMultiWindow` | `boolean` | 是否支持分屏 |
| `mSupportsFreeformWindowManagement` | `boolean` | 是否支持自由窗口 |
| `mSupportsPictureInPicture` | `boolean` | 是否支持画中画 |
| `mSupportsMultiDisplay` | `boolean` | 是否支持多显示器 |
| `mForceResizableActivities` | `boolean` | 强制可调整大小 |

#### 6.2.7 Activity 状态

| 字段 | 类型 | 说明 |
|------|------|------|
| `mLastResumedActivity` | `ActivityRecord` | 最后 Resumed 的 Activity |
| `mTracedResumedActivity` | `ActivityRecord` | 当前 Trace 的 Resumed Activity |
| `mLastANRState` | `String` | 上次 ANR 状态 |
| `mCurAppTimeTracker` | `AppTimeTracker` | 当前应用时长追踪 |

#### 6.2.8 锁/Handler

| 字段 | 类型 | 说明 |
|------|------|------|
| `mGlobalLock` | `WindowManagerGlobalLock` | **全局锁**（与 WMS 共享） |
| `mGlobalLockWithoutBoost` | `Object` | 绕过优先级提升机制的全局锁 |
| `mH` | `H` | 内部 Handler |
| `mUiHandler` | `UiHandler` | UI Handler |

#### 6.2.9 其他

| 字段 | 类型 | 说明 |
|------|------|------|
| `mContext` | `Context` | 系统 Context |
| `mUiContext` | `Context` | 可换主题的 UI Context |
| `mSystemThread` | `ActivityThread` | 系统主线程 |
| `mAssistUtils` | `AssistUtils` | Voice/Assist 工具 |
| `mCompatModePackages` | `CompatModePackages` | 兼容模式包管理 |
| `mAppWarnings` | `AppWarnings` | 应用警告 |
| `mSleeping` | `boolean` | 是否在睡眠中 |
| `mShuttingDown` | `boolean` | 是否正在关机 |
| `mTopProcessState` | `int` | Top 进程状态 |
| `mAppSwitchesAllowedTime` | `long` | 允许应用切换的时间点 |
| `mDeviceOwnerUid` | `int` | DeviceOwner UID |

---

### 6.3 ActivityRecord

```
final class ActivityRecord extends ConfigurationContainer
```

#### 6.3.1 身份标识

| 字段 | 类型 | 说明 |
|------|------|------|
| `info` | `ActivityInfo` | ActivityInfo |
| `appInfo` | `ApplicationInfo` | ApplicationInfo |
| `intent` | `Intent` | 原始 Intent |
| `mActivityComponent` | `ComponentName` | 组件名 |
| `packageName` | `String` | 包名 |
| `processName` | `String` | 进程名 |
| `taskAffinity` | `String` | 任务亲和性 |
| `mUserId` | `int` | 用户 ID |
| `launchedFromPid` | `int` | 启动者 PID |
| `launchedFromUid` | `int` | 启动者 UID |
| `launchedFromPackage` | `String` | 启动者包名 |

#### 6.3.2 关联关系

| 字段 | 类型 | 说明 |
|------|------|------|
| `task` | `TaskRecord` | **所属 Task** |
| `app` | `WindowProcessController` | **所属进程** |
| `resultTo` | `ActivityRecord` | **启动者** |
| `resultWho` | `String` | resultTo 附加标识 |
| `requestCode` | `int` | requestCode |
| `results` | `ArrayList<ResultInfo>` | 待处理 ActivityResult |
| `pendingResults` | `HashSet<WeakReference<PendingIntentRecord>>` | 待处理 PendingIntent |
| `newIntents` | `ArrayList<ReferrerIntent>` | single-top 新 Intent |

#### 6.3.3 生命周期状态

| 字段 | 类型 | 说明 |
|------|------|------|
| `mState` | `ActivityState` | **当前状态** |
| `stopped` | `boolean` | Pause 是否完成 |
| `finishing` | `boolean` | 是否在 pending finish 列表 |
| `delayedResume` | `boolean` | 是否延迟 Resume |
| `frontOfTask` | `boolean` | 是否是 Task 根 Activity |
| `launchFailed` | `boolean` | 启动失败标记 |
| `idle` | `boolean` | 是否已 idle |
| `hasBeenLaunched` | `boolean` | 是否曾被启动 |
| `launchCount` | `int` | 启动次数 |
| `lastLaunchTime` | `long` | 上次启动时间 |
| `haveState` | `boolean` | 是否已获得状态 |
| `icicle` | `Bundle` | 上次保存状态 |
| `persistentState` | `PersistableBundle` | 持久化状态 |

#### 6.3.4 可见性/窗口

| 字段 | 类型 | 说明 |
|------|------|------|
| `visible` | `boolean` | 窗口是否需要显示 |
| `visibleIgnoringKeyguard` | `boolean` | 忽略 Keyguard 后是否可见 |
| `nowVisible` | `boolean` | 窗口当前是否可见 |
| `mDrawn` | `boolean` | 窗口是否已绘制 |
| `sleeping` | `boolean` | 是否 sleep |
| `keysPaused` | `boolean` | 按键分发是否暂停 |
| `immersive` | `boolean` | 沉浸模式 |
| `fullscreen` | `boolean` | 是否全屏 |
| `noDisplay` | `boolean` | 是否不显示 |
| `mShowWhenLocked` | `boolean` | 锁屏时显示 |
| `mTurnScreenOn` | `boolean` | 启动时点亮屏幕 |
| `mStartingWindowState` | `int` | Starting Window 状态 |
| `mTaskOverlay` | `boolean` | Task 覆盖层 |

#### 6.3.5 配置/资源

| 字段 | 类型 | 说明 |
|------|------|------|
| `compat` | `CompatibilityInfo` | 兼容模式 |
| `configChangeFlags` | `int` | 变更的配置值 |
| `forceNewConfig` | `boolean` | 强制新配置重建 |
| `mLastReportedConfiguration` | `MergedConfiguration` | 最后报告配置 |
| `mLastReportedDisplayId` | `int` | 最后 DisplayId |
| `mLastReportedMultiWindowMode` | `boolean` | 最后多窗口模式 |
| `mLastReportedPictureInPictureMode` | `boolean` | 最后 PiP 模式 |

#### 6.3.6 其他

| 字段 | 类型 | 说明 |
|------|------|------|
| `launchMode` | `int` | LaunchMode |
| `lockTaskLaunchMode` | `int` | LockTask 模式 |
| `createTime` | `long` | 创建时间 |
| `lastVisibleTime` | `long` | 最后可见时间 |
| `pauseTime` | `long` | 最后 Pause 开始时间 |
| `cpuTimeAtResume` | `long` | Resume 时 CPU 时间 |
| `mRelaunchReason` | `int` | 重新启动原因 |
| `pictureInPictureArgs` | `PictureInPictureParams` | PiP 参数 |
| `taskDescription` | `TaskDescription` | Recent 任务描述 |
| `voiceSession` | `IVoiceInteractionSession` | Voice 会话 |
| `appToken` | `IApplicationToken.Stub` | WM Token |
| `mAppWindowToken` | `AppWindowToken` | App Window Token |
| `mStackSupervisor` | `ActivityStackSupervisor` | StackSupervisor 引用 |
| `mRootActivityContainer` | `RootActivityContainer` | RootActivityContainer 引用 |

---

### 6.4 TaskRecord

```
class TaskRecord extends ConfigurationContainer
```

#### 6.4.1 标识

| 字段 | 类型 | 说明 |
|------|------|------|
| `taskId` | `int` | **唯一任务 ID** |
| `affinity` | `String` | 任务亲和性（可变） |
| `rootAffinity` | `String` | 根亲和性（不可变） |
| `userId` | `int` | 所属用户 |
| `effectiveUid` | `int` | 当前有效 UID |
| `realActivity` | `ComponentName` | 实际启动的组件 |
| `origActivity` | `ComponentName` | 原始组件（非别名） |
| `intent` | `Intent` | 原始启动 Intent |
| `affinityIntent` | `Intent` | Affinity 迁移 Intent |

#### 6.4.2 成员列表

| 字段 | 类型 | 说明 |
|------|------|------|
| `mActivities` | `ArrayList<ActivityRecord>` | **Task 内所有 Activity** |
| `mStack` | `ActivityStack` | 当前所属 Stack |
| `mRootProcess` | `WindowProcessController` | 根 Activity 的进程 |
| `mTask` | `Task` | 对应窗口容器 Task |

#### 6.4.3 状态

| 字段 | 类型 | 说明 |
|------|------|------|
| `inRecents` | `boolean` | 是否在最近任务列表中 |
| `isAvailable` | `boolean` | Activity 是否可启动 |
| `hasBeenVisible` | `boolean` | Task 中是否有过可见 Activity |
| `lastActiveTime` | `long` | 最后活跃时间 |
| `isPersistable` | `boolean` | 是否可持久化 |
| `mLastTimeMoved` | `long` | 最后移动时间 |
| `rootWasReset` | `boolean` | 根 Intent 是否有 RESET_TASK_IF_NEEDED |
| `autoRemoveRecents` | `boolean` | 是否自动从 Recents 移除 |
| `mUserSetupComplete` | `boolean` | 用户设置是否完成 |

#### 6.4.4 窗口/布局

| 字段 | 类型 | 说明 |
|------|------|------|
| `numFullscreen` | `int` | 全屏 Activity 数量 |
| `mResizeMode` | `int` | 调整大小模式 |
| `mSupportsPictureInPicture` | `boolean` | 是否支持 PiP |
| `mMinWidth` / `mMinHeight` | `int` | 最小宽高 |
| `mLastNonFullscreenBounds` | `Rect` | 上次非全屏边界 |
| `mLayerRank` | `int` | 可见层级排名 |
| `mDisplayedBounds` | `Rect` | 当前绘制边界 |

#### 6.4.5 任务关联

| 字段 | 类型 | 说明 |
|------|------|------|
| `mAffiliatedTaskId` | `int` | 父任务关联 ID |
| `mAffiliatedTaskColor` | `int` | 关联颜色 |
| `mPrevAffiliate` | `TaskRecord` | 链中前一个关联任务 |
| `mNextAffiliate` | `TaskRecord` | 链中后一个关联任务 |

#### 6.4.6 LockTask

| 字段 | 类型 | 说明 |
|------|------|------|
| `mLockTaskAuth` | `int` | LockTask 授权等级 |
| `mLockTaskUid` | `int` | 调用 startLockTask 的 UID |

#### 6.4.7 其他

| 字段 | 类型 | 说明 |
|------|------|------|
| `lastTaskDescription` | `TaskDescription` | 最近任务描述 |
| `voiceSession` | `IVoiceInteractionSession` | Voice 会话 |
| `mCallingUid` / `mCallingPackage` | — | 调用者信息 |
| `maxRecents` | `int` | 最大 Recent 条目数 |
| `mService` | `ActivityTaskManagerService` | ATMS 引用 |

---

### 6.5 ActivityStack

```
class ActivityStack extends ConfigurationContainer
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `mTaskHistory` | `ArrayList<TaskRecord>` | **栈内所有 Task（历史顺序）** |
| `mLRUActivities` | `ArrayList<ActivityRecord>` | LRU 排序的 Activity 列表 |
| `mResumedActivity` | `ActivityRecord` | **当前 Resumed 的 Activity** |
| `mPausingActivity` | `ActivityRecord` | **正在 Pausing 的 Activity** |
| `mLastPausedActivity` | `ActivityRecord` | 上次 Paused 的 Activity |
| `mLastNoHistoryActivity` | `ActivityRecord` | 上次 no-history Activity |
| `mTranslucentActivityWaiting` | `ActivityRecord` | 等待半透明转换的 Activity |
| `mUndrawnActivitiesBelowTopTranslucent` | `ArrayList<ActivityRecord>` | 未绘制的半透明下层 Activity |
| `mService` | `ActivityTaskManagerService` | ATMS 引用 |
| `mWindowManager` | `WindowManagerService` | WMS 引用 |
| `mStackId` | `int` | **Stack 唯一 ID** |
| `mDisplayId` | `int` | 所属 Display ID |
| `mCurrentUser` | `int` | 当前用户 ID |
| `mConfigWillChange` | `boolean` | 配置即将变更标记 |
| `mForceHidden` | `boolean` | 强制标记为不可见 |
| `mInResumeTopActivity` | `boolean` | 防递归标记 |
| `mRestoreOverrideWindowingMode` | `int` | 瞬态窗口模式恢复值 |

---

### 6.6 ActivityStackSupervisor

```
public class ActivityStackSupervisor implements RecentTasks.Callbacks
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `mService` | `ActivityTaskManagerService` | ATMS 引用 |
| `mRootActivityContainer` | `RootActivityContainer` | 根容器引用 |
| `mRecentTasks` | `RecentTasks` | 最近任务管理 |
| `mRunningTasks` | `RunningTasks` | 运行中任务查询 |
| `mWindowManager` | `WindowManagerService` | WMS 引用 |
| `mHandler` / `mLooper` | `ActivityStackSupervisorHandler` / `Looper` | Handler 和 Looper |
| `mKeyguardController` | `KeyguardController` | 锁屏控制器 |
| `mPowerManager` | `PowerManager` | 电源管理器 |
| `mLaunchParamsController` | `LaunchParamsController` | 启动参数控制器 |
| `mLaunchParamsPersister` | `LaunchParamsPersister` | 启动参数持久化 |
| `mCurTaskIdForUser` | `SparseIntArray` | 每个用户的当前 Task ID 计数 |
| `mTopResumedActivity` | `ActivityRecord` | 系统最顶层 Resumed 的 Activity |
| `mTopResumedActivityWaitingForPrev` | `boolean` | 等待上一个 top 处理状态丢失 |
| `mStoppingActivities` | `ArrayList<ActivityRecord>` | 待 Stopped 的 Activity 列表 |
| `mFinishingActivities` | `ArrayList<ActivityRecord>` | 待 Finished 的 Activity 列表 |
| `mGoingToSleepActivities` | `ArrayList<ActivityRecord>` | 正在 sleep 的 Activity 列表 |
| `mMultiWindowModeChangedActivities` | `ArrayList<ActivityRecord>` | 多窗口模式变更列表 |
| `mPipModeChangedActivities` | `ArrayList<ActivityRecord>` | PiP 模式变更列表 |
| `mNoAnimActivities` | `ArrayList<ActivityRecord>` | 跳过动画的 Activity 列表 |
| `mWaitingForActivityVisible` | `ArrayList<WaitInfo>` | 等待 Activity 可见的进程 |
| `mWaitingActivityLaunched` | `ArrayList<WaitResult>` | 等待 Activity 启动的进程 |
| `mLaunchingActivityWakeLock` | `PowerManager.WakeLock` | 启动期间持有 WakeLock |
| `mGoingToSleepWakeLock` | `PowerManager.WakeLock` | 进入睡眠期间持有 WakeLock |
| `mDockedStackResizing` | `boolean` | 分屏 Stack 是否在调整大小中 |
| `mHasPendingDockedBounds` | `boolean` | 是否有待应用的分屏边界 |
| `mAllowDockedStackResize` | `boolean` | 是否允许分屏 Stack 调整大小 |
| `mResizingTasksDuringAnimation` | `ArraySet<Integer>` | 动画中处于调整大小模式的 Task |
| `mUserLeaving` | `boolean` | 是否需要发出 onUserLeaving 回调 |
| `mDeferResumeCount` | `int` | 推迟 Resume 计数 |
| `mStartingUsers` | `ArrayList<UserState>` | 正在启动的用户 |
| `mAppVisibilitiesChangedSinceLastPause` | `boolean` | 上次 Pause 后可见性是否发生变化 |
| `mActivityMetricsLogger` | `ActivityMetricsLogger` | Activity 启动指标日志 |

---

### 6.7 RootActivityContainer

```
class RootActivityContainer extends ConfigurationContainer
       implements DisplayManager.DisplayListener
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `mService` | `ActivityTaskManagerService` | ATMS 引用 |
| `mStackSupervisor` | `ActivityStackSupervisor` | StackSupervisor 引用 |
| `mWindowManager` | `WindowManagerService` | WMS 引用 |
| `mDisplayManager` | `DisplayManager` | DisplayManager 实例 |
| `mDisplayManagerInternal` | `DisplayManagerInternal` | DisplayManager 内部接口 |
| `mRootWindowContainer` | `RootWindowContainer` | 对应的窗口根容器 |
| `mActivityDisplays` | `ArrayList<ActivityDisplay>` | **所有 ActivityDisplay 列表（按 z-order）** |
| `mDefaultDisplay` | `ActivityDisplay` | 默认 Display 引用 |
| `mDisplayAccessUIDs` | `SparseArray<IntArray>` | 各 Display 的访问 UID 列表 |
| `mCurrentUser` | `int` | 当前用户 |
| `mUserStackInFront` | `SparseIntArray` | 用户切换时前台的 Stack ID |
| `mSleepTokens` | `ArrayList<SleepToken>` | Sleep Token 列表 |
| `mIsDockMinimized` | `boolean` | 分屏 Dock 是否最小化 |
| `mDefaultMinSizeOfResizeableTaskDp` | `int` | 可调整 Task 的最小默认尺寸 |
| `mTaskLayersChanged` | `boolean` | Task 层级是否变化（触发 OOM 评分） |
| `mTmpActivityList` | `ArrayList<ActivityRecord>` | 临时 Activity 列表 |
| `mTmpFindTaskResult` | `FindTaskResult` | 临时用的 Task 查找结果 |

---

### 6.8 ActivityStarter

```
class ActivityStarter
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `mService` | `ActivityTaskManagerService` | ATMS |
| `mRootActivityContainer` | `RootActivityContainer` | 根容器 |
| `mSupervisor` | `ActivityStackSupervisor` | StackSupervisor |
| `mInterceptor` | `ActivityStartInterceptor` | 启动拦截器 |
| `mController` | `ActivityStartController` | 启动控制器 |
| `mStartActivity` | `ActivityRecord` | **目标启动 Activity** |
| `mIntent` | `Intent` | 启动 Intent |
| `mCallingUid` | `int` | 调用者 UID |
| `mOptions` | `ActivityOptions` | Activity 选项 |
| `mRestrictedBgActivity` | `boolean` | 是否限制后台启动 |
| `mLaunchMode` | `int` | 启动模式 |
| `mLaunchFlags` | `int` | 启动 Flags |
| `mLaunchTaskBehind` | `boolean` | 是否后台启动 |
| `mLaunchParams` | `LaunchParams` | 启动参数 |
| `mPreferredDisplayId` | `int` | 优先 Display |
| `mSourceRecord` | `ActivityRecord` | **源 Activity** |
| `mNotTop` | `ActivityRecord` | 非顶层的候选 |
| `mInTask` | `TaskRecord` | 要加入的 Task |
| `mReuseTask` | `TaskRecord` | 要复用的 Task |
| `mNewTaskInfo` | `ActivityInfo` | 新 Task 的 ActivityInfo |
| `mNewTaskIntent` | `Intent` | 新 Task 的 Intent |
| `mSourceStack` | `ActivityStack` | 源 Stack |
| `mTargetStack` | `ActivityStack` | **目标 Stack** |
| `mMovedToFront` | `boolean` | 是否已移到前台 |
| `mAddingToTask` | `boolean` | 是否正添加到 Task |
| `mAvoidMoveToFront` | `boolean` | 是否避免移到前台 |
| `mDoResume` | `boolean` | 是否执行 Resume |
| `mStartFlags` | `int` | startFlags |
| `mNoAnimation` | `boolean` | 是否禁用动画 |
| `mKeepCurTransition` | `boolean` | 是否保持当前过渡动画 |
| `mFrozeTaskList` | `boolean` | 是否冻结 Task 列表 |
| `mIntentDelivered` | `boolean` | Intent 是否已交付 |
| `mVoiceSession` | `IVoiceInteractionSession` | Voice 会话 |
| `mVoiceInteractor` | `IVoiceInteractor` | Voice 交互器 |
| `mLastStartActivityRecord` | `ActivityRecord[1]` | 上次启动的 Activity |
| `mLastStartActivityResult` | `int` | 上次启动结果 |
| `mLastStartActivityTimeMs` | `long` | 上次启动时间 |
| `mLastStartReason` | `String` | 上次启动原因 |
| `mRequest` | `Request` | 启动请求参数 |

---

### 6.9 ProcessRecord（AMS 侧）

```
class ProcessRecord implements WindowProcessListener
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `mWindowProcessController` | `WindowProcessController` | **对应的 WPC** |
| `mService` | `ActivityManagerService` | AMS 引用 |
| `info` / `uid` / `userId` / `processName` | — | 进程基本信息 |
| `thread` | `IApplicationThread` | 应用进程 Binder |
| `pid` | `int` | 进程 PID |
| `gids` | `int[]` | 进程 GID 列表 |
| `curAdj` / `setAdj` / `maxAdj` | `int` | OOM 相关 |
| `mCurProcState` / `setProcState` | `int` | 进程状态 |
| `mCurSchedGroup` / `setSchedGroup` | `int` | 调度组 |
| `services` / `executingServices` | `ArraySet<ServiceRecord>` | 正在运行的 Service |
| `connections` | `ArraySet<ConnectionRecord>` | Service 绑定连接 |
| `receivers` | `ArraySet<ReceiverList>` | 注册的广播接收器 |
| `curReceivers` | `ArraySet<BroadcastRecord>` | 当前正在执行的广播 |
| `pubProviders` | `ArrayMap<String, ContentProviderRecord>` | 发布的 Provider |
| `conProviders` | `ArrayList<ContentProviderConnection>` | 使用的 Provider 连接 |
| `pkgList` | `PackageList` | 进程内包列表 |
| `killedByAm` / `killed` | `boolean` | 被 AM 杀死标记 |
| `crashDialog` / `anrDialog` | `Dialog` | ANR/Crash 对话框 |
| `empty` / `cached` | `boolean` | 是否是空/缓存进程 |

---

### 6.10 WindowProcessController（ATMS 侧）

```
public class WindowProcessController extends ConfigurationContainer<ConfigurationContainer>
       implements ConfigurationContainerListener
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `mListener` | `WindowProcessListener` | **回调 AMS 侧的 ProcessRecord** |
| `mAtm` | `ActivityTaskManagerService` | ATMS 引用 |
| `mInfo` / `mName` / `mUid` / `mUserId` / `mOwner` | — | 进程基本信息 |
| `mPid` | `volatile int` | PID |
| `mThread` | `IApplicationThread` | 应用 Binder |
| `mPkgList` | `ArraySet<String>` | 进程内包列表 |
| `mCurProcState` / `mRepProcState` | `volatile int` | 进程状态 |
| `mCurSchedGroup` | `volatile int` | 调度组 |
| `mCrashing` / `mNotResponding` / `mPersistent` / `mDebugging` | `volatile boolean` | 异常/调试状态 |
| `mHasForegroundServices` / `mHasForegroundActivities` | `volatile boolean` | 前台 Service/Activity |
| `mHasTopUi` / `mHasOverlayUi` / `mPendingUiClean` | `volatile boolean` | UI 状态 |
| `mActivities` | `ArrayList<ActivityRecord>` | **进程内所有 Activity** |
| `mRecentTasks` | `ArrayList<TaskRecord>` | 进程运行过的 Task |
| `mRunningRecentsAnimation` / `mRunningRemoteAnimation` | `boolean` | 动画状态 |
| `mLastActivityLaunchTime` / `mLastActivityFinishTime` | `long` | Activity 启动/结束时间 |
| `mLastReportedConfiguration` | `Configuration` | 最后报告配置 |

---

## 七、树状展开

```
Android Framework 服务层
├── 系统服务启动
│   └── SystemServer
│       ├── ATMS.Lifecycle.onStart() → publishBinderService("activity_task")
│       └── AMS.Lifecycle.startService(atm) → publishBinderService("activity")
│
├── 顶层服务类
│   ├── AMS
│   │   ├── 进程/内存: mProcessList, mPidsSelfLocked, mOomAdjuster, mLowMemDetector
│   │   ├── 组件: mServices, mBroadcastQueues[3], mProviderMap
│   │   ├── 状态: mSystemReady, mBooted, mBootPhase, mWakefulness, mOnBattery
│   │   ├── 跨服务: mActivityTaskManager, mAtmInternal, mWindowManager
│   │   ├── 线程: mHandlerThread, mHandler, mUiHandler, mProcessCpuThread
│   │   └── 内部类: Lifecycle, LocalService, MainHandler, UiHandler
│   └── ATMS
│       ├── 容器: mRootActivityContainer, mStackSupervisor
│       ├── 控制器: mActivityStartController, mLockTaskController, mKeyguardController
│       ├── 跨服务: mAmInternal, mWindowManager
│       ├── 进程: mProcessNames, mProcessMap, mHomeProcess
│       ├── 任务: mRecentTasks, mTaskChangeNotificationController
│       ├── 多窗口: mSupportsMultiWindow, mSupportsSplitScreenMultiWindow, mSupportsPictureInPicture
│       ├── 锁: mGlobalLock, mGlobalLockWithoutBoost
│       └── 内部类: Lifecycle, LocalService, H, UiHandler
│
├── 容器层级体系
│   ConfigurationContainer
│   └── WindowContainer
│       ├── Task (WindowContainer<AppWindowToken>)
│       ├── TaskStack (WindowContainer<Task>)
│       ├── ActivityStack (ConfigurationContainer)
│       ├── ActivityDisplay (ConfigurationContainer<ActivityStack>)
│       └── RootActivityContainer (ConfigurationContainer)
│   ActivityRecord (ConfigurationContainer)
│   TaskRecord (ConfigurationContainer)
│
├── ActivityRecord
│   ├── 身份: info, appInfo, intent, mActivityComponent, packageName, processName, taskAffinity
│   ├── 关联: task, app, resultTo, results, pendingResults, newIntents
│   ├── 状态: mState, stopped, finishing, delayedResume, frontOfTask, idle, icicle
│   ├── 可见: visible, visibleIgnoringKeyguard, nowVisible, mDrawn, fullscreen
│   ├── 配置: compat, configChangeFlags, mLastReportedConfiguration
│   └── 其他: launchMode, lockTaskLaunchMode, appToken, mAppWindowToken
│
├── TaskRecord
│   ├── 标识: taskId, affinity, rootAffinity, realActivity, intent
│   ├── 成员: mActivities, mStack, mRootProcess, mTask
│   ├── 状态: inRecents, isAvailable, isPersistable, mLastTimeMoved
│   ├── 布局: mResizeMode, mMinWidth, mMinHeight, mDisplayedBounds
│   └── 关联: mAffiliatedTaskId, mPrevAffiliate, mNextAffiliate
│
├── ActivityStack
│   ├── 成员: mTaskHistory, mLRUActivities, mResumedActivity, mPausingActivity
│   ├── 标识: mStackId, mDisplayId, mCurrentUser
│   └── 状态: mConfigWillChange, mForceHidden, mInResumeTopActivity
│
├── 管理器
│   ├── ActivityStackSupervisor: mRecentTasks, mRunningTasks, mTopResumedActivity, mStoppingActivities, mFinishingActivities
│   ├── RootActivityContainer: mActivityDisplays, mDefaultDisplay, mUserStackInFront
│   └── ActivityStartController → 分配 ActivityStarter
│
├── ActivityStarter
│   ├── 引用: mService, mRootActivityContainer, mSupervisor, mController
│   ├── 目标: mStartActivity, mIntent, mCallingUid, mOptions
│   ├── 源: mSourceRecord, mSourceStack
│   ├── 目标容器: mTargetStack, mInTask, mReuseTask
│   ├── 参数: mLaunchMode, mLaunchFlags, mLaunchParams, mPreferredDisplayId
│   └── 记录: mLastStartActivityRecord, mLastStartActivityResult
│
└── 进程双视角
    ├── AMS 侧 ProcessRecord
    │   ├── mWindowProcessController (对应 WPC)
    │   ├── thread, pid, info, uid, processName
    │   ├── curAdj, mCurProcState, mCurSchedGroup
    │   ├── services, connections, receivers, pubProviders
    │   └── crashDialog, anrDialog, killed, cached
    └── ATMS 侧 WindowProcessController
        ├── mListener (回调 ProcessRecord)
        ├── mPid, mThread, mPkgList
        ├── mCurProcState, mCurSchedGroup
        ├── mHasForegroundActivities, mHasForegroundServices
        ├── mActivities, mRecentTasks
        └── mLastReportedConfiguration
```

---

## 八、启动链路

```
startActivity()
  → ATMS.startActivityAsUser()
    → ActivityStartController.obtainStarter()
      → ActivityStarter.execute()
        → ActivityStarter.startActivityUnchecked()
          → ActivityStack.resumeTopActivityUncheckedLocked()
            → ActivityStackSupervisor.startSpecificActivityLocked()
              → AMS.startProcessLocked()
                → ProcessRecord (AMS侧)
                → WindowProcessController (ATMS侧)
```

---

*文档生成时间：2026-07-09*
*基于 cells-android10 项目 Android 10 源码*
