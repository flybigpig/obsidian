
我来为您详细解析 Android 中 Activity 管理的核心类：ActivityRecord、ActivityStack、Task 等，以及它们在 ATMS 中的协作关系。
我来为您详细解析 Android 中 Activity 管理的核心类：ActivityRecord、ActivityStack、Task 等，以及它们在 ATMS 中的协作关系。

---

## 一、核心类架构与关系

### 1.1 类层次结构（Android 10+）

```
DisplayContent (Display #0, #1...)
    └── ActivityStack (Stack #0, #1, #2...)
            └── Task (Task id #1, #2...)
                    └── ActivityRecord (Hist #0, #1, #2...)
```

**关键变化**：Android 10 后 `TaskRecord` 重命名为 `Task`，`ActivityStack` 继承自 `Task`，形成更统一的窗口容器层级 。

### 1.2 核心类职责

| 类名 | 职责 | 关键属性 |
|------|------|---------|
| **ActivityRecord** | 单个 Activity 的完整信息 | token, state, taskAffinity, info |
| **Task** | Activity 的任务栈（LIFO） | mActivities, taskId, affinity, mStack |
| **ActivityStack** | Task 的容器，管理 Z 序和生命周期 | mTasks, mDisplay, mStackId |
| **ActivityDisplay** | 显示设备抽象 | mStacks, mDisplayId |
| **RootWindowContainer** | 所有 Display 的根容器 | mDisplays, mFocusedStack |
| **ActivityStackSupervisor** | 栈管理器，协调多栈交互 | mHomeStack, mFocusedStack |

---

## 二、ActivityRecord：Activity 的"身份证"

### 2.1 核心数据结构

```java
// frameworks/base/services/core/java/com/android/server/wm/ActivityRecord.java
final class ActivityRecord extends WindowToken {
    // 唯一标识
    final Token token;                    // IBinder token，跨进程标识
    final String packageName;             // 包名
    final String processName;             // 进程名
    final ActivityInfo info;              // AndroidManifest 中的配置
    
    // 状态管理
    State state;                          // INITIALIZING, RESUMED, PAUSED, STOPPED, DESTROYED
    boolean finishing;                    // 是否正在关闭
    boolean visible;                      // 是否可见
    boolean frontOfTask;                  // 是否是 Task 栈顶
    
    // 关联关系
    Task task;                            // 所属 Task
    ActivityRecord resultTo;              // 启动它的 Activity（startActivityForResult）
    ProcessRecord app;                    // 所在进程（AMS 侧）
    WindowProcessController mWpc;         // 进程控制器（ATMS 侧）
    
    // 启动信息
    Intent intent;                        // 启动 Intent
    int launchedFromUid;                  // 调用者 UID
    String launchedFromPackage;           // 调用者包名
    
    // Token 内部类，用于跨进程查找
    private static class Token extends Binder {
        WeakReference<ActivityRecord> mActivityRef;
        
        static ActivityRecord forTokenLocked(IBinder token) {
            return ((Token) token).mActivityRef.get();
        }
    }
}
```

### 2.2 状态流转

```java
// ActivityRecord 生命周期状态
enum State {
    INITIALIZING,     // 初始状态
    RESUMED,          // 前台运行（onResume）
    PAUSING,          // 正在暂停
    PAUSED,           // 已暂停（onPause）
    STOPPING,         // 正在停止
    STOPPED,          // 已停止（onStop）
    FINISHING,        // 正在完成
    DESTROYING,       // 正在销毁
    DESTROYED         // 已销毁
}

// 状态转换示例
void setState(State newState, String reason) {
    State oldState = state;
    state = newState;
    
    // 通知 ATMS 状态变化
    onStateChanged(oldState, newState);
    
    // 同步到 AMS 更新进程优先级
    if (newState == RESUMED) {
        mAtmService.mAmInternal.updateActivityUsageStats(this, ACTIVITY_RESUMED);
    }
}
```

---

## 三、Task：Activity 的"任务栈"

### 3.1 核心结构

```java
// frameworks/base/services/core/java/com/android/server/wm/Task.java
class Task extends WindowContainer<ActivityRecord> {
    final int taskId;                     // 唯一任务 ID
    String affinity;                      // 任务亲和性（默认包名）
    Intent intent;                        // 启动 Task 的根 Intent
    ComponentName realActivity;           // 实际 Activity 组件
    
    // 栈结构
    private final ArrayList<ActivityRecord> mActivities = new ArrayList<>();
    
    // 所属 Stack
    ActivityStack mStack;
    
    // 最近任务相关
    boolean inRecents;                    // 是否显示在最近任务
    long lastActiveTime;                  // 最后活跃时间
    
    // 添加 Activity（栈顶）
    void addActivityToTop(ActivityRecord r) {
        mActivities.add(r);
        r.task = this;
        r.frontOfTask = (mActivities.size() == 1);
    }
    
    // 获取栈顶 Activity
    ActivityRecord getTopActivity() {
        for (int i = mActivities.size() - 1; i >= 0; i--) {
            ActivityRecord r = mActivities.get(i);
            if (!r.finishing) return r;
        }
        return null;
    }
    
    // 返回键处理：移除栈顶
    ActivityRecord removeActivity(ActivityRecord r) {
        mActivities.remove(r);
        if (mActivities.isEmpty()) {
            // Task 为空，通知 Stack 移除
            mStack.removeTask(this);
        }
        return getTopActivity();  // 返回新的栈顶
    }
}
```

### 3.2 Task 与 ActivityStack 的关系

```java
// Task 的栈操作
void moveToFront() {
    mStack.moveTaskToFrontLocked(this);  // 移动到 Stack 顶部
}

void setStack(ActivityStack stack) {
    if (mStack != null) {
        mStack.removeTask(this);
    }
    mStack = stack;
    if (stack != null) {
        stack.addTask(this, true);  // 添加到新 Stack
    }
}
```

---

## 四、ActivityStack：Task 的"容器"

### 4.1 核心实现

```java
// frameworks/base/services/core/java/com/android/server/wm/ActivityStack.java
class ActivityStack extends Task {
    final int mStackId;                   // 栈 ID
    final ActivityType mActivityType;     // HOME, STANDARD, RECENTS 等
    
    // 管理的 Tasks（继承自 Task，但作为容器使用）
    private final ArrayList<Task> mTasks = new ArrayList<>();
    
    // 所属 Display
    ActivityDisplay mDisplay;
    
    // 焦点管理
    ActivityRecord mResumedActivity;      // 当前 RESUMED 的 Activity
    ActivityRecord mPausingActivity;      // 正在 PAUSING 的 Activity
    
    // 启动 Activity 到 Stack
    boolean startActivityLocked(ActivityRecord r, Task task, boolean newTask) {
        if (newTask || task == null) {
            // 创建新 Task
            task = new Task(mNextTaskId++, r.info, r.intent);
            addTask(task, true);  // 添加到 Stack 顶部
        }
        
        // 将 Activity 添加到 Task
        task.addActivityToTop(r);
        r.setStack(this);
        
        // 更新状态
        if (mResumedActivity == null) {
            // 第一个 Activity，直接 Resume
            resumeTopActivityUncheckedLocked(r, null);
        } else {
            // 暂停当前 Activity，启动新 Activity
            startPausingLocked(false, null);
        }
        return true;
    }
    
    // 获取栈顶 Task
    Task getTopTask() {
        return mTasks.isEmpty() ? null : mTasks.get(mTasks.size() - 1);
    }
    
    // 获取栈顶 Activity（跨所有 Task）
    ActivityRecord getTopActivity() {
        for (int i = mTasks.size() - 1; i >= 0; i--) {
            ActivityRecord top = mTasks.get(i).getTopActivity();
            if (top != null) return top;
        }
        return null;
    }
}
```

### 4.2 ActivityType 分类 

```java
// 栈类型定义
static final int ACTIVITY_TYPE_STANDARD = 1;    // 普通应用
static final int ACTIVITY_TYPE_HOME = 2;        // Launcher
static final int ACTIVITY_TYPE_RECENTS = 3;     // 最近任务
static final int ACTIVITY_TYPE_ASSISTANT = 4;   // 语音助手

// Stack 创建时确定类型
ActivityStack(ActivityDisplay display, int stackId, ActivityType type) {
    mDisplay = display;
    mStackId = stackId;
    mActivityType = type;
}
```

---

## 五、容器层级与遍历机制

### 5.1 RootWindowContainer：根容器

```java
// frameworks/base/services/core/java/com/android/server/wm/RootWindowContainer.java
class RootWindowContainer extends WindowContainer<DisplayContent> {
    // 所有 Display
    private final SparseArray<ActivityDisplay> mDisplays = new SparseArray<>();
    
    // 当前聚焦的 Stack
    ActivityStack mFocusedStack;
    
    // 遍历所有 Stack 查找 Activity（AMS attachApplication 时使用）
    boolean attachApplication(WindowProcessController app) {
        // 使用 AttachApplicationHelper 遍历
        return forAllRootTasks(new AttachApplicationHelper(app));
    }
    
    // 内部类：处理 attachApplication 逻辑
    private class AttachApplicationHelper implements Consumer<Task>, Predicate<ActivityRecord> {
        WindowProcessController mApp;
        boolean mHasActivityStarted = false;
        
        @Override
        public void accept(Task rootTask) {
            // 遍历该 Stack 的所有 Activity
            rootTask.forAllActivities(this);
        }
        
        @Override
        public boolean test(ActivityRecord r) {
            // 匹配进程名和 UID
            if (r.app == null && r.processName.equals(mApp.mName) 
                    && r.info.applicationInfo.uid == mApp.mUid) {
                try {
                    // 真正启动 Activity
                    if (mTaskSupervisor.realStartActivityLocked(r, mApp, true, true)) {
                        mHasActivityStarted = true;
                    }
                } catch (RemoteException e) {
                    // 处理异常
                }
            }
            return false;  // 继续遍历
        }
    }
}
```

### 5.2 遍历机制详解 

```java
// WindowContainer 基类提供遍历方法
abstract class WindowContainer<E extends WindowContainer> {
    protected final ArrayList<E> mChildren = new ArrayList<>();
    
    // 遍历所有子容器
    void forAllTasks(Consumer<Task> callback) {
        for (E child : mChildren) {
            child.forAllTasks(callback);  // 递归
        }
    }
    
    // 遍历所有 Activity
    boolean forAllActivities(Predicate<ActivityRecord> callback) {
        for (int i = mChildren.size() - 1; i >= 0; --i) {
            if (mChildren.get(i).forAllActivities(callback)) return true;
        }
        return false;
    }
}

// ActivityRecord 叶子节点实现
@Override
boolean forAllActivities(Predicate<ActivityRecord> callback) {
    return callback.test(this);  // 执行回调
}

// Task 中间节点实现
@Override
boolean forAllActivities(Predicate<ActivityRecord> callback) {
    for (int i = mActivities.size() - 1; i >= 0; --i) {
        if (mActivities.get(i).forAllActivities(callback)) return true;
    }
    return false;
}
```

---

## 六、启动流程中的协作

### 6.1 Activity 启动时序

```java
// ActivityStarter.java - 启动协调器
class ActivityStarter {
    ActivityRecord mStartActivity;
    Task mTargetTask;
    ActivityStack mTargetStack;
    
    int execute() {
        // 1. 创建 ActivityRecord
        mStartActivity = new ActivityRecord(mService, callerApp, 
                callingUid, callingPackage, intent, info, ...);
        
        // 2. 查找或创建 Task
        mTargetTask = computeTargetTask();
        if (mTargetTask == null) {
            mTargetTask = createNewTask();
        }
        
        // 3. 获取或创建 Stack
        mTargetStack = getOrCreateRootTask(mStartActivity, mLaunchFlags, 
                mTargetTask, mOptions);
        
        // 4. 将 Activity 添加到 Task
        mTargetTask.addActivityToTop(mStartActivity);
        
        // 5. 启动进程（如需要）
        if (mStartActivity.app == null) {
            mService.startProcessAsync(mStartActivity, ...);
        } else {
            // 进程已存在，直接启动
            mTargetStack.resumeTopActivityUncheckedLocked(...);
        }
        
        return START_SUCCESS;
    }
}
```

### 6.2 进程创建后的 attach 流程

```java
// AMS 侧：attachApplication
boolean attachApplicationLocked(ProcessRecord app) {
    // ...
    
    // 委托 ATMS 启动 Activity
    didSomething = mAtmInternal.attachApplication(
            app.getWindowProcessController());
    
    return didSomething;
}

// ATMS 侧：attachApplication
boolean attachApplication(WindowProcessController wpc) {
    // 遍历所有 Stack 查找该进程的 Activity
    return mRootWindowContainer.attachApplication(wpc);
}

// RootWindowContainer 遍历并启动
boolean attachApplication(WindowProcessController app) {
    return forAllRootTasks(new AttachApplicationHelper(app));
}
```

---

## 七、调试命令与输出解析

### 7.1 dumpsys activity activities

```bash
adb shell dumpsys activity activities
```

**输出结构解析** ：

```
Display #0 (activities from top to bottom):
  Stack #0: type=home mode=fullscreen    # Home Stack
    Task id #1                           # Task ID
      * TaskRecord{... #1 I=com.launcher/.Launcher}   # Task 详情
        Activities=[ActivityRecord{... Launcher}]      # Activity 列表
        * Hist #0: ActivityRecord{...}   # 历史记录（栈结构）
          state=RESUMED                  # 当前状态
          frontOfTask=true               # 栈顶标记
          taskAffinity=null              # 亲和性

  Stack #402: type=standard              # 普通应用 Stack
    Task id #99900235
      * Hist #0: ActivityRecord{...}     # 微信 Activity
        state=STOPPED
        taskAffinity=com.tencent.mm      # 亲和性为包名
```

### 7.2 关键字段说明

| 字段 | 含义 |
|------|------|
| `Stack #N` | 栈编号，0 通常是 Home |
| `type=home/standard` | 栈类型 |
| `Task id #N` | 任务唯一 ID |
| `Hist #N` | 历史记录序号，越大越新 |
| `state=RESUMED` | Activity 当前状态 |
| `frontOfTask=true` | 是否为 Task 栈顶 |
| `taskAffinity` | 任务亲和性，决定 Task 归属 |
| `mResumedActivity` | 当前焦点 Activity |

---

## 八、总结

| 核心概念 | 一句话描述 | 类比 |
|---------|-----------|------|
| **ActivityRecord** | 单个 Activity 的完整信息档案 | 员工档案 |
| **Task** | 一组相关 Activity 的 LIFO 栈 | 项目小组 |
| **ActivityStack** | 管理多个 Task 的容器 | 部门 |
| **ActivityDisplay** | 显示设备抽象 | 办公楼 |
| **RootWindowContainer** | 所有 Display 的根管理器 | 集团总部 |
| **ActivityStackSupervisor** | 协调多栈交互的监督者 | 行政总监 |

**关键设计原则**：
1. **分层管理**：从 Root → Display → Stack → Task → ActivityRecord 形成树形结构
2. **状态集中**：ActivityRecord 维护完整生命周期状态，Task/Stack 管理容器关系
3. **遍历统一**：通过 `WindowContainer` 基类提供统一的递归遍历机制
4. **跨进程标识**：使用 `Token(IBinder)` 作为 ActivityRecord 的跨进程句柄 