我来为您详细解释 Android 框架中的两个核心系统服务：ATMS (ActivityTaskManagerService) 与 AMS (ActivityManagerService)。
## 一、历史背景：从 AMS 到 ATMS + AMS 的拆分

Android 10（API 29）是系统服务架构的重要分水岭。在此之前，**AMS 是一个"超级服务"**，几乎包揽了应用管理的所有职责。随着系统复杂度增加，Google 在 Android 10 进行了**职责分离**：

```
Android 9 及之前：
  AMS（ActivityManagerService）
  ├─ Activity 生命周期管理
  ├─ Task/Stack 管理  
  ├─ 进程管理（ProcessList）
  ├─ 内存管理（LMK、OOM 调节）
  ├─ Service/Broadcast/Provider 管理
  └─ 权限管理

Android 10+：
  ATMS（ActivityTaskManagerService）← 新拆分
  ├─ Activity 生命周期管理
  ├─ Task/Stack/Display 管理
  └─ 多窗口/分屏模式控制
  
  AMS（ActivityManagerService）← 瘦身保留
  ├─ 进程管理（ProcessList）
  ├─ 内存管理（AppProfiler、LMK）
  ├─ Service/Broadcast/Provider 管理
  ├─ 权限管理（PermissionManager）
  └─ ANR/Crash 处理
```

**拆分的核心原因**：
1. **职责单一化**：AMS 代码量超过 2 万行，耦合严重
2. **多窗口架构支持**：折叠屏、分屏、自由窗口需要更灵活的 Task 管理
3. **减少锁竞争**：Activity 调度与进程管理分离，可并行处理

---

## 二、ATMS（ActivityTaskManagerService）：Activity 与 Task 的"大管家"

### 1. 核心职责
| 功能域 | 具体职责 |
|--------|----------|
| **Activity 生命周期** | 启动、暂停、恢复、停止、销毁的完整状态机 |
| **Task 管理** | TaskRecord 创建、维护、清理；任务栈（Back Stack）操作 |
| **启动模式** | standard/singleTop/singleTask/singleInstance 的实现 |
| **多窗口支持** | 分屏（Split-screen）、画中画（PiP）、自由窗口（Freeform） |
| **窗口层级** | 与 WMS 协同确定 Activity 窗口的 Z-order 和显示区域 |

### 2. 关键数据结构
```java
// 根窗口容器，管理所有 DisplayContent
RootWindowContainer mRootWindowContainer;

// 当前聚焦的 Task
Task mFocusedTask;

// 所有 Task 的映射
ArrayMap<Integer, Task> mTasks = new ArrayMap<>();

// Activity 记录
ActivityRecord mResumedActivity;  // 当前 Resume 的 Activity
```

### 3. 启动流程中的角色
当调用 `startActivity()` 时，ATMS 的核心调用链：
```
ATMS.startActivity() 
  → startActivityAsUser()
    → executeRequest()           // 构建 ActivityStarter
      → startActivityUnchecked() // 解析启动模式、Intent Flag
        → startActivityInner()   // 确定 Task、执行栈操作
          → resumeFocusedTasksTopActivities() // 恢复栈顶 Activity
```

### 4. 与 WMS 的紧密协作
ATMS 通过 `ActivityRecord` 与 WMS 的 `WindowToken` 关联：
- Activity 启动时，ATMS 通知 WMS 创建 `AppWindowToken`
- Task 位置变化时，ATMS 计算 bounds，WMS 执行 `setTaskBounds()`
- 多窗口模式下，两者共同维护 `TaskDisplayArea` 的层级

---

## 三、AMS（ActivityManagerService）：进程与系统资源的"调度员"

### 1. 核心职责
| 功能域 | 具体职责 |
|--------|----------|
| **进程管理** | 进程创建（zygote fork）、优先级调整（oom_adj）、杀死策略 |
| **内存管理** | 低内存杀死（LMK）、内存统计、垃圾回收协调 |
| **后台服务** | Service 的启动/绑定/解绑/销毁；前台服务限制 |
| **广播系统** | 有序广播、粘性广播、广播权限校验、ANR 检测 |
| **ContentProvider** | 跨进程数据共享的启动与管理 |
| **系统异常** | ANR 弹窗、Crash 处理、应用无响应监控 |

### 2. 关键数据结构
```java
// 进程列表管理
ProcessList mProcessList;          // 维护所有运行中的进程

// LRU 进程列表（用于内存回收）
final ArrayList<ProcessRecord> mLruProcesses = new ArrayList<>();

// 服务管理
ActiveServices mServices;          // Service 记录与生命周期

// 广播管理
BroadcastQueue mBroadcastQueues[]; // 并行/串行广播队列

// 内存统计
AppProfiler mAppProfiler;          // 内存、CPU 使用监控
```

### 3. 进程管理的核心逻辑
AMS 通过 `ProcessRecord` 维护进程状态：
```java
public final class ProcessRecord {
    final String processName;      // 进程名
    final ApplicationInfo info;    // 应用信息
    int pid;                       // 进程 ID
    int setAdj;                    // 当前 oom_adj 值
    int curAdj;                    // 请求中的 oom_adj
    boolean killedByAm;            // 是否被 AMS 杀死
    // ...
}
```

**oom_adj 调度策略**：
- `FOREGROUND_APP_ADJ (0)`：前台 Activity
- `PERCEPTIBLE_APP_ADJ (2)`：可感知（后台播放音乐）
- `SERVICE_ADJ (5)`：活跃 Service
- `CACHED_APP_ADJ (9)`：缓存进程（ LRU 列表淘汰）

---

## 四、ATMS 与 AMS 的协作机制

### 1. 双向引用关系
```java
// ATMS 持有 AMS 引用（用于进程相关操作）
public class ActivityTaskManagerService {
    final ActivityManagerService mAm;
    
    void startProcessAsync(...) {
        mAm.startProcessLocked(...);  // 需要创建进程时回调 AMS
    }
}

// AMS 持有 ATMS 引用（用于 Activity 相关查询）
public class ActivityManagerService {
    final ActivityTaskManagerService mAtm;
    
    List<RunningTaskInfo> getTasks(...) {
        return mAtm.getTasks(...);    // 获取任务信息时委托 ATMS
    }
}
```

### 2. 典型协作场景：Activity 冷启动
```
1. App 调用 startActivity()
   ↓
2. ATMS 解析 Intent，发现目标进程未运行
   ↓
3. ATMS 请求 AMS: startProcessLocked()
   ↓
4. AMS fork 新进程，创建 ProcessRecord，启动 ApplicationThread
   ↓
5. 新进程 attach 到 AMS，AMS 通知 ATMS: attachApplication()
   ↓
6. ATMS 在该进程中实例化 Activity，执行生命周期
   ↓
7. Activity 创建后，ATMS 通知 WMS 添加窗口，AMS 更新进程优先级
```

### 3. Binder 接口分离
| 接口 | 服务 | 功能 |
|------|------|------|
| `IActivityTaskManager` | ATMS | `startActivity()`, `getTasks()`, `moveTaskToFront()` |
| `IActivityManager` | AMS | `startService()`, `broadcastIntent()`, `killBackgroundProcesses()` |

---

## 五、调试与监控命令

```bash
# 查看 ATMS 状态（Task 和 Activity 信息）
adb shell dumpsys activity activities

# 查看 AMS 进程和内存状态
adb shell dumpsys activity processes

# 查看服务状态
adb shell dumpsys activity services

# 查看广播队列
adb shell dumpsys activity broadcasts

# 实时查看 Activity 切换（查看 ATMS 日志）
adb shell logcat -s ActivityTaskManager

# 查看进程优先级调整（查看 AMS 日志）
adb shell logcat -s ActivityManager
```

---

## 六、总结对比

| 维度 | ATMS | AMS |
|------|------|-----|
| **出现版本** | Android 10+ | Android 1.0+ |
| **核心关注点** | **界面导航**（What to show） | **资源管控**（How to run） |
| **直接交互** | WMS（窗口）、InputDispatcher（输入） | zygote（进程）、kernel（内存） |
| **典型问题** | 黑屏、ANR、返回栈异常、多窗口布局错乱 | 后台杀死、内存泄漏、Service 无法启动、广播丢失 |
| **调试重点** | Task 栈状态、Activity 生命周期回调、窗口层级 | 进程优先级、内存水位、ANR  traces |

**一句话记忆**：  
- **ATMS** 管"看得见"的：Activity 怎么跳、Task 怎么叠、窗口怎么摆  
- **AMS** 管"看不见"的：进程活不活、内存够不够、后台保不保  

两者通过紧密协作，共同支撑 Android 应用的生命周期管理与系统资源调度。