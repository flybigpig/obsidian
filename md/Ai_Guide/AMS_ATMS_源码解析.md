# Android 10 AMS 与 ATMS 源码深度解析

> 会话导出时间：2026-07-09
> 分析对象：`cells-android10` 项目中的 Android 10 framework 源码
> 主要涉及文件：
> - `frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java`
> - `frameworks/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java`

---

## 目录

1. [整体结构总结](#1-整体结构总结)
2. [核心成员字段及负责内容](#2-核心成员字段及负责内容)
3. [树状图详细整理](#3-树状图详细整理)
4. [各子树细化调用树](#4-各子树细化调用树)
5. [关键跨服务协作](#5-关键跨服务协作)

---

## 1. 整体结构总结

Android 10 将原本集中在 `ActivityManagerService`（AMS）中的 Activity/Task/Window 编排职责拆分到独立的 `ActivityTaskManagerService`（ATMS），两者通过内部接口（`ActivityManagerInternal` / `ActivityTaskManagerInternal`）协作。

### 1.1 AMS（进程与系统总管）

- **包声明**：`com.android.server.am`
- **类定义**：`public class ActivityManagerService extends IActivityManager.Stub implements Watchdog.Monitor, BatteryStatsImpl.BatteryCallback`
- **Binder 接口**：`IActivityManager.Stub` —— 对外（App 进程、SystemServer 其它服务）暴露进程、广播、OOM、权限、ANR 等总管能力
- **职责范围**：
  - 进程生命周期（fork、attach、死亡清理）
  - 广播（ordered / parallel）
  - OOM Adj 计算与内存管理
  - 权限校验
  - ANR 触发与处理（实际 `appNotResponding` 定义在 `ProcessRecord`）
  - 通过 `mActivityTaskManager` 委托 Activity 启动

### 1.2 ATMS（Activity/Task/Window 编排总管）

- **包声明**：`com.android/server/wm`
- **类定义**：`public class ActivityTaskManagerService extends IActivityTaskManager.Stub`
- **Binder 接口**：`IActivityTaskManager.Stub`
- **职责范围**：
  - Activity 启动流程编排（ActivityStartController → ActivityStarter → RootActivityContainer → ActivityStackSupervisor）
  - Task / Stack 的增删改查与焦点管理
  - 窗口可见性、Configuration 变更
  - RecentTasks
  - 进程在窗口侧的投影 `WindowProcessController`

### 1.3 二者关系

| 维度 | AMS | ATMS |
|------|-----|------|
| 进程视图 | `ProcessRecord`（生死 / OOM） | `WindowProcessController`（窗口侧投影） |
| 共享锁 | —— | `mGlobalLock`（WindowManagerGlobalLock，与 WMS 共用） |
| 内部接口 | `ActivityManagerInternal`（暴露给 framework 内部） | `ActivityTaskManagerInternal` |
| 协作方式 | 启动 Activity 时委托 `mActivityTaskManager.startActivity` | `attachApplication` 时回调 `mAmInternal` |

---

## 2. 核心成员字段及负责内容

### 2.1 AMS 核心字段

| 字段 | 类型 | 行号 | 负责内容 |
|------|------|------|----------|
| `mProcessList` | `ProcessList` | :663 | 进程创建、fork、进程列表统一管理 |
| `mPidsSelfLocked` | `PidMap` | :733 | 按 pid 索引的进程映射表；`put` 时回调 `onProcessMapped`，`remove` 时回调 `onProcessUnMapped`（与 ATMS 同步） |
| `mAtmInternal` | `ActivityTaskManagerInternal` | :1478 | 反向调用 ATMS 的内部接口句柄 |
| `mProcessNames` / `mProcessMap` | —— | —— | 按名字 / pid 的进程集合 |
| `mOomAdjuster` | `OomAdjuster` | —— | OOM Adj 计算与应用的执行器 |
| `mIntentFirewall` | `IntentFirewall` | —— | 广播 / Intent 防火墙校验（与 ATMS 共享实例） |
| `mPendingIntentController` | `PendingIntentController` | —— | PendingIntent 管理（与 ATMS 共享实例） |
| `mActiveUids` | `ActiveUids` | —— | 活跃 UID 集合（与 ATMS 共享实例） |

### 2.2 ATMS 核心字段

| 字段 | 类型 | 行号 | 负责内容 |
|------|------|------|----------|
| `mContext` | `Context` | :343 | SystemServer 上下文 |
| `mUiContext` | `Context` | :349 | UI 上下文（用于 Toast / 对话框） |
| `mGlobalLock` | `WindowManagerGlobalLock` | :366 | 全局锁，与 WMS 共用，避免死锁 |
| `mAmInternal` | `ActivityManagerInternal` | :353 | 反向调用 AMS 的内部接口句柄 |
| `mStackSupervisor` | `ActivityStackSupervisor` | :375 | Activity / Stack 管理的核心监督者 |
| `mRootActivityContainer` | `RootActivityContainer` | :376 | 根 Activity 容器，管理所有 Display 的 Stack |
| `mRecentTasks` | `RecentTasks` | :401 | 最近任务列表 |
| `mProcessNames` / `mProcessMap` | —— | :384 / :386 | ATMS 侧进程映射（WindowProcessController） |
| `mActivityStartController` | `ActivityStartController` | :465 | Activity 启动控制器，提供 `obtainStarter` |

---

## 3. 树状图详细整理

### 3.1 ActivityManagerService 树

```
ActivityManagerService (am)
├── 类声明 :401  implements IActivityManager.Stub, Watchdog.Monitor, BatteryStatsImpl.BatteryCallback
│
├── 内部类 / 子结构
│   ├── Lifecycle :2209            // SystemService 宿主，创建并发布 AMS
│   ├── Lifecycles :2256           // 其它系统服务生命周期辅助
│   ├── UiHandler :1580            // UI 线程消息（Toast、崩溃对话框）
│   ├── MainHandler :1668          // 主线程消息
│   ├── AppDeathRecipient :1483    // 进程死亡 Binder 死亡回调
│   ├── LocalService :17839        // 实现 ActivityManagerInternal
│   └── ProcessRecord (独立类)     // 进程记录，含 appNotResponding :1407
│
├── 核心字段（见第 2.1 节）
│
└── 关键方法分区
    ├── A1 进程管理
    │   ├── startProcessLocked :3102
    │   ├── attachApplication :5255
    │   ├── attachApplicationLocked :4842
    │   ├── handleAppDiedLocked :3673
    │   └── cleanUpApplicationRecordLocked :13730
    ├── A2 广播
    │   ├── broadcastIntentLocked :14870（双重载）
    │   └── finishReceiver :15729
    ├── A3 OOM / 内存
    │   ├── updateOomAdjLocked :16828
    │   └── updateOomAdjLocked :17027
    ├── A4 ANR
    │   └── appNotResponding（委托 ProcessRecord）
    └── 委托入口
        └── startActivity :3572 → mActivityTaskManager.startActivity(...)
```

### 3.2 ActivityTaskManagerService 树

```
ActivityTaskManagerService (wm)
├── 类声明 :303  extends IActivityTaskManager.Stub
│
├── 内部类 / 子结构
│   ├── Lifecycle :975             // SystemService 宿主
│   ├── LocalService :6111         // 实现 ActivityTaskManagerInternal
│   └── WindowProcessController     // 进程窗口侧投影（am/wm 共享概念）
│
├── 核心字段（见第 2.2 节）
│
└── 关键方法分区
    ├── B1 Activity 启动
    │   ├── startActivity :1009
    │   ├── startActivityAsUser :1032
    │   └── obtainStarter → ActivityStarter.execute
    ├── B2 Task / Stack
    │   ├── moveTaskToFront :2357
    │   ├── removeTask :2116
    │   └── resizeTask :3239
    ├── B3 Configuration / 可见性
    │   ├── updateConfiguration :4478
    │   ├── updateConfigurationLocked :5171
    │   └── ensureConfigAndVisibilityAfterUpdate :5820
    ├── B4 进程同步
    │   ├── onProcessMapped(pid, WPC)    // 由 AMS.mPidsSelfLocked.put 触发
    │   └── onProcessUnMapped(pid)        // 由 AMS.mPidsSelfLocked.remove 触发
    └── 初始化
        ├── initialize :810
        └── onActivityManagerInternalAdded :837
```

---

## 4. 各子树细化调用树

> 下列调用链均基于源码方法实体，非推测。

### 4.1 A1 进程管理调用树

```
startProcessLocked :3102
└── mProcessList.startProcessLocked(...)
    └── ZYGOTE fork → 新进程启动

attachApplication :5255
└── attachApplicationLocked :4842
    ├── mAtmInternal.attachApplication(WPC)        // [ATMS] 窗口侧 attach
    └── mServices.attachApplicationLocked(...)     // [ActiveServices] 服务绑定

handleAppDiedLocked :3673
└── cleanUpApplicationRecordLocked :13730
    └── 清理 ProcessRecord / 通知 ATMS 进程移除

cleanUpApplicationRecordLocked :13730
└── mAtmInternal.onProcessUnMapped(pid)            // 同步移除窗口侧投影
```

### 4.2 A2 广播调用树

```
broadcastIntentLocked :14870
├── enqueueParallelBroadcastLocked(...)            // 并行广播入队
├── enqueueOrderedBroadcastLocked(...)             // 有序广播入队
└── scheduleBroadcastsLocked(...)
    └── deliverToRegisteredReceiverLocked(...)
        └── receiver.onReceive(...)               // 分发到接收者

finishReceiver :15729
└── 广播完成，推进有序广播队列下一位
```

### 4.3 A3 OOM / 内存调用树

```
updateOomAdjLocked :16828
└── updateOomAdjLocked :17027
    └── mOomAdjuster.updateOomAdjLocked(...)
        ├── computeOomAdjLocked(...)              // 计算 adj / procState
        └── applyOomAdjLocked(...)                // 应用至内核（oom_score_adj 等）
```

### 4.4 A4 ANR 调用树

```
触发方（BroadcastQueue / ActiveServices / AMS.appNotRespondingViaProvider）
└── ProcessRecord.appNotResponding :1407
    ├── mWindowProcessController.appNotResponding(...)   // [ATMS 侧] 弹 ANR 对话框
    └── kill(...)                                        // 必要时杀进程
```

### 4.5 B1 Activity 启动调用树（跨服务核心链）

```
ATMS.startActivity :1009
└── startActivityAsUser :1032
    └── getActivityStartController().obtainStarter(...).execute()   // ActivityStartController
        └── [ActivityStarter] execute
            └── startActivityUnchecked(...)
                └── RootActivityContainer.resumeFocusedStacksTopActivities
                    └── ActivityStackSupervisor.startSpecificActivityLocked
                        ├── (进程未起) AMS.startProcessLocked   ──► 见 A1
                        └── (已起)   realStartActivityLocked
                            └── app.thread.scheduleTransaction(...)   // Binder → App 端
```

### 4.6 B2 / B3 Task、Configuration 调用树（节选）

```
moveTaskToFront :2357
└── RootActivityContainer / StackSupervisor 焦点栈切换
    └── ensureConfigAndVisibilityAfterUpdate :5820

updateConfiguration :4478
└── updateConfigurationLocked :5171
    └── ensureConfigAndVisibilityAfterUpdate :5820
        └── 通知各 Stack / Activity 配置变更与可见性
```

### 4.7 B4 进程同步调用树（AMS ↔ ATMS 双轨视图）

```
AMS.mPidsSelfLocked.put(pid, proc) :746
└── mAtmInternal.onProcessMapped(pid, WPC)         // ATMS 建立 WindowProcessController 投影

AMS.mPidsSelfLocked.remove(pid) :764
└── mAtmInternal.onProcessUnMapped(pid)            // ATMS 移除投影
```

---

## 5. 关键跨服务协作

### 5.1 点击图标启动 App（端到端）

```
Launcher 点击
  → AMS.startActivity :3572
    → mActivityTaskManager.startActivity   [委托 ATMS]
      → ATMS.startActivity :1009 → ... → RootActivityContainer
        → ActivityStackSupervisor.startSpecificActivityLocked
          ├── 进程未起 → AMS.startProcessLocked → ZYGOTE fork
          │     → app 启动 → attachApplication → ATMS.attachApplication
          └── 进程已起 → realStartActivityLocked → app.thread.scheduleTransaction
```

### 5.2 进程视图双轨同步

- AMS 以 `ProcessRecord` 为权威（生/死、OOM Adj）
- ATMS 以 `WindowProcessController` 为投影（窗口可见性、ANR 对话框）
- 同步点：`mPidsSelfLocked.put/remove` 回调 `onProcessMapped/onProcessUnMapped`

### 5.3 常见误解澄清

- ❌ 误解：`appNotResponding` 是 AMS 的直接方法。
- ✅ 事实：`appNotResponding` 定义在 `ProcessRecord`（am 包内，:1407），由 BroadcastQueue / ActiveServices / AMS 多处触发，内部再调用 `WindowProcessController.appNotResponding` 与 `kill`。

---

## 附：关键行号速查表

| 内容 | 文件 | 行号 |
|------|------|------|
| AMS 类声明 | ActivityManagerService.java | :401 |
| ProcessList 字段 | ActivityManagerService.java | :663 |
| mPidsSelfLocked 字段 | ActivityManagerService.java | :733 |
| mAtmInternal 字段 | ActivityManagerService.java | :1478 |
| startProcessLocked | ActivityManagerService.java | :3102 |
| attachApplication | ActivityManagerService.java | :5255 |
| attachApplicationLocked | ActivityManagerService.java | :4842 |
| handleAppDiedLocked | ActivityManagerService.java | :3673 |
| cleanUpApplicationRecordLocked | ActivityManagerService.java | :13730 |
| broadcastIntentLocked | ActivityManagerService.java | :14870 |
| updateOomAdjLocked | ActivityManagerService.java | :16828 / :17027 |
| LocalService | ActivityManagerService.java | :17839 |
| ATMS 类声明 | ActivityTaskManagerService.java | :303 |
| mGlobalLock 字段 | ActivityTaskManagerService.java | :366 |
| mStackSupervisor 字段 | ActivityTaskManagerService.java | :375 |
| mRootActivityContainer 字段 | ActivityTaskManagerService.java | :376 |
| mActivityStartController 字段 | ActivityTaskManagerService.java | :465 |
| startActivity | ActivityTaskManagerService.java | :1009 |
| startActivityAsUser | ActivityTaskManagerService.java | :1032 |
| moveTaskToFront | ActivityTaskManagerService.java | :2357 |
| removeTask | ActivityTaskManagerService.java | :2116 |
| updateConfiguration | ActivityTaskManagerService.java | :4478 |
| updateConfigurationLocked | ActivityTaskManagerService.java | :5171 |
| ensureConfigAndVisibilityAfterUpdate | ActivityTaskManagerService.java | :5820 |
| LocalService | ActivityTaskManagerService.java | :6111 |
| Lifecycle | ActivityTaskManagerService.java | :975 |
| ProcessRecord.appNotResponding | ProcessRecord.java | :1407 |

---

*本文档由会话自动整理导出，所有行号均基于 `cells-android10`（Android 10）源码快照。*
