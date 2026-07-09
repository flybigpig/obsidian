# Android 10 AMS 与 ATMS 源码解析 · 会话完整导出

> 导出时间：2026-07-09
> 工作目录：`c:\D\android_project\cells-android10`
> 分析源码：Android 10（cells-android10）
> 涉及文件：
> - `frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java`
> - `frameworks/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java`

---

# 用户提问 1：ActivityManagerService, ActivityTaskManagerService 结构总结

## 回答

Android 10 将原本集中在 AMS 中的 Activity/Task/Window 编排职责拆分到独立的 ATMS，两者通过内部接口（`ActivityManagerInternal` / `ActivityTaskManagerInternal`）协作。

### AMS（进程与系统总管）
- **包**：`com.android.server.am`
- **类定义**：`public class ActivityManagerService extends IActivityManager.Stub implements Watchdog.Monitor, BatteryStatsImpl.BatteryCallback`
- **Binder 接口**：`IActivityManager.Stub`
- **职责**：进程生命周期、广播、OOM Adj、权限、ANR 触发（实际 `appNotResponding` 在 `ProcessRecord`）、通过 `mActivityTaskManager` 委托 Activity 启动。

### ATMS（Activity/Task/Window 编排总管）
- **包**：`com.android/server/wm`
- **类定义**：`public class ActivityTaskManagerService extends IActivityTaskManager.Stub`
- **Binder 接口**：`IActivityTaskManager.Stub`
- **职责**：Activity 启动编排（ActivityStartController → ActivityStarter → RootActivityContainer → ActivityStackSupervisor）、Task/Stack 管理、窗口可见性、Configuration、RecentTasks、进程窗口侧投影 `WindowProcessController`。

### 二者关系
| 维度 | AMS | ATMS |
|------|-----|------|
| 进程视图 | `ProcessRecord`（生死 / OOM） | `WindowProcessController`（窗口侧投影） |
| 共享锁 | —— | `mGlobalLock`（WindowManagerGlobalLock，与 WMS 共用） |
| 内部接口 | `ActivityManagerInternal` | `ActivityTaskManagerInternal` |
| 协作 | 启动 Activity 委托 `mActivityTaskManager.startActivity` | `attachApplication` 回调 `mAmInternal` |

---

# 用户提问 2：核心成员字段结构 以及负责内容

## 回答

### AMS 核心字段
| 字段 | 类型 | 行号 | 负责内容 |
|------|------|------|----------|
| `mProcessList` | `ProcessList` | :663 | 进程创建、fork、进程列表统一管理 |
| `mPidsSelfLocked` | `PidMap` | :733 | 按 pid 索引的进程映射；`put`/`remove` 回调 ATMS 同步 |
| `mAtmInternal` | `ActivityTaskManagerInternal` | :1478 | 反向调用 ATMS 的内部接口 |
| `mOomAdjuster` | `OomAdjuster` | —— | OOM Adj 计算与应用执行器 |
| `mIntentFirewall` | `IntentFirewall` | —— | 广播 / Intent 防火墙（与 ATMS 共享） |
| `mPendingIntentController` | `PendingIntentController` | —— | PendingIntent 管理（与 ATMS 共享） |
| `mActiveUids` | `ActiveUids` | —— | 活跃 UID 集合（与 ATMS 共享） |

### ATMS 核心字段
| 字段 | 类型 | 行号 | 负责内容 |
|------|------|------|----------|
| `mContext` | `Context` | :343 | SystemServer 上下文 |
| `mUiContext` | `Context` | :349 | UI 上下文（Toast / 对话框） |
| `mGlobalLock` | `WindowManagerGlobalLock` | :366 | 全局锁，与 WMS 共用 |
| `mAmInternal` | `ActivityManagerInternal` | :353 | 反向调用 AMS 的内部接口 |
| `mStackSupervisor` | `ActivityStackSupervisor` | :375 | Activity/Stack 管理核心 |
| `mRootActivityContainer` | `RootActivityContainer` | :376 | 根容器，管理所有 Display 的 Stack |
| `mRecentTasks` | `RecentTasks` | :401 | 最近任务列表 |
| `mProcessNames`/`mProcessMap` | —— | :384 / :386 | ATMS 侧进程映射（WindowProcessController） |
| `mActivityStartController` | `ActivityStartController` | :465 | 启动控制器，`obtainStarter` |

---

# 用户提问 3：树状图 详细整理

## 回答

### AMS 树
```
ActivityManagerService (am)
├── 类声明 :401  implements IActivityManager.Stub, Watchdog.Monitor, BatteryStatsImpl.BatteryCallback
├── 内部类 / 子结构
│   ├── Lifecycle :2209
│   ├── Lifecycles :2256
│   ├── UiHandler :1580
│   ├── MainHandler :1668
│   ├── AppDeathRecipient :1483
│   ├── LocalService :17839          // ActivityManagerInternal
│   └── ProcessRecord (独立类, appNotResponding :1407)
├── 核心字段（见提问 2）
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
    │   └── updateOomAdjLocked :16828 / :17027
    ├── A4 ANR → ProcessRecord.appNotResponding
    └── 委托入口 startActivity :3572 → mActivityTaskManager.startActivity
```

### ATMS 树
```
ActivityTaskManagerService (wm)
├── 类声明 :303  extends IActivityTaskManager.Stub
├── 内部类 / 子结构
│   ├── Lifecycle :975
│   ├── LocalService :6111           // ActivityTaskManagerInternal
│   └── WindowProcessController
├── 核心字段（见提问 2）
└── 关键方法分区
    ├── B1 Activity 启动
    │   ├── startActivity :1009
    │   └── startActivityAsUser :1032
    ├── B2 Task / Stack
    │   ├── moveTaskToFront :2357
    │   ├── removeTask :2116
    │   └── resizeTask :3239
    ├── B3 Configuration / 可见性
    │   ├── updateConfiguration :4478
    │   ├── updateConfigurationLocked :5171
    │   └── ensureConfigAndVisibilityAfterUpdate :5820
    ├── B4 进程同步 onProcessMapped / onProcessUnMapped
    └── 初始化 initialize :810 / onActivityManagerInternalAdded :837
```

---

# 用户提问 4：把其中每一棵子树 再展开更细的调用树

## 回答

### A1 进程管理
```
startProcessLocked :3102
└── mProcessList.startProcessLocked(...)
    └── ZYGOTE fork → 新进程

attachApplication :5255
└── attachApplicationLocked :4842
    ├── mAtmInternal.attachApplication(WPC)       // [ATMS]
    └── mServices.attachApplicationLocked(...)    // [ActiveServices]

handleAppDiedLocked :3673
└── cleanUpApplicationRecordLocked :13730
    └── mAtmInternal.onProcessUnMapped(pid)        // 移除窗口侧投影
```

### A2 广播
```
broadcastIntentLocked :14870
├── enqueueParallelBroadcastLocked(...)
├── enqueueOrderedBroadcastLocked(...)
└── scheduleBroadcastsLocked(...)
    └── deliverToRegisteredReceiverLocked(...)
        └── receiver.onReceive(...)

finishReceiver :15729 → 推进有序广播下一位
```

### A3 OOM / 内存
```
updateOomAdjLocked :16828
└── updateOomAdjLocked :17027
    └── mOomAdjuster.updateOomAdjLocked(...)
        ├── computeOomAdjLocked(...)
        └── applyOomAdjLocked(...)
```

### A4 ANR
```
触发方 (BroadcastQueue / ActiveServices / AMS.appNotRespondingViaProvider)
└── ProcessRecord.appNotResponding :1407
    ├── mWindowProcessController.appNotResponding(...)   // [ATMS] 弹对话框
    └── kill(...)
```

### B1 Activity 启动（跨服务核心链）
```
ATMS.startActivity :1009
└── startActivityAsUser :1032
    └── getActivityStartController().obtainStarter(...).execute()   // ActivityStartController
        └── [ActivityStarter] execute
            └── startActivityUnchecked(...)
                └── RootActivityContainer.resumeFocusedStacksTopActivities
                    └── ActivityStackSupervisor.startSpecificActivityLocked
                        ├── (进程未起) AMS.startProcessLocked → A1
                        └── (已起)   realStartActivityLocked
                            └── app.thread.scheduleTransaction(...)   // → App 端
```

### B2 / B3 Task、Configuration
```
moveTaskToFront :2357
└── RootActivityContainer / StackSupervisor 焦点切换
    └── ensureConfigAndVisibilityAfterUpdate :5820

updateConfiguration :4478
└── updateConfigurationLocked :5171
    └── ensureConfigAndVisibilityAfterUpdate :5820
```

### B4 进程同步（双轨视图）
```
AMS.mPidsSelfLocked.put(pid, proc) :746
└── mAtmInternal.onProcessMapped(pid, WPC)        // ATMS 建立投影

AMS.mPidsSelfLocked.remove(pid) :764
└── mAtmInternal.onProcessUnMapped(pid)           // ATMS 移除投影
```

---

# 跨服务协作补充

### 点击图标启动 App（端到端）
```
Launcher 点击
  → AMS.startActivity :3572
    → mActivityTaskManager.startActivity          [委托 ATMS]
      → ATMS.startActivity :1009 → ... → RootActivityContainer
        → ActivityStackSupervisor.startSpecificActivityLocked
          ├── 进程未起 → AMS.startProcessLocked → ZYGOTE fork
          │     → attachApplication → ATMS.attachApplication
          └── 进程已起 → realStartActivityLocked → app.thread.scheduleTransaction
```

### 进程视图双轨同步
- AMS：`ProcessRecord`（权威，生/死、OOM Adj）
- ATMS：`WindowProcessController`（投影，窗口可见性、ANR 对话框）
- 同步点：`mPidsSelfLocked.put/remove` 回调 `onProcessMapped/onProcessUnMapped`

### 误解澄清
- ❌ `appNotResponding` 是 AMS 直接方法
- ✅ 实际定义在 `ProcessRecord`（:1407），由 BroadcastQueue/ActiveServices/AMS 触发，内部再调 `WindowProcessController.appNotResponding` 与 `kill`

---

# 关键行号速查表

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

*本文件为当前会话完整导出，包含全部 4 条用户提问与对应作答，所有行号基于 cells-android10（Android 10）源码快照。*
