#### ActivityRecord

ActivityRecord 对象中主要记录了四类信息：

- 配置信息：这些配置信息大多来自应用中的 `AndroidManifest.xml`,比如：属于哪个 Package，所在的进程名称、任务名、组件类名、logo、主题等等，这些信息基本上是固定的

- 启动时基本信息：启动 Activity 时，才确定的信息，在每次启动时可能会不同，如 launchedFromPid、task、launchedFromPackage、processName 等

- 运行状态信息：记录 activity 当前状态的信息，比如 mState、idle、stopped、finishing 等

- 管理对象：管理当前 ActivityRecord 的对象




```
final class ActivityRecord extends ConfigurationContainer {
    
    // 配置信息
    final ActivityInfo info; // AndroidManifest.xml 中提供的信息
    final String packageName; // the package implementing intent's component
    final String processName; // process where this component wants to run
    final String taskAffinity; // as per ActivityInfo.taskAffinity
    private int labelRes;           // the label information from the package mgr.
    private int icon;               // resource identifier of activity's icon.
    private int logo;               // resource identifier of activity's logo.
    private int theme;              // resource identifier of activity's theme.
    // ......

    // 启动时基本信息
    final int launchedFromPid; // always the pid who started the activity.
    final int launchedFromUid; // always the uid who started the activity.
    final int mUserId;          // Which user is this running for?
    final Intent intent;    // the original intent that generated us
    final ComponentName mActivityComponent;  // the intent component, or target of an alias.
    final String processName; // process where this component wants to run

    // 运行状态信息
    private ActivityState mState;    // current state we are in
    
    // // frameworks/base/services/core/java/com/android/server/wm/ActivityStack.java
    // enum ActivityState {
    //     INITIALIZING,
    //     RESUMED,
    //     PAUSING,
    //     PAUSED,
    //     STOPPING,
    //     STOPPED,
    //     FINISHING,
    //     DESTROYING,
    //     DESTROYED,
    //     RESTARTING_PROCESS
    // }

    boolean idle;           // has the activity gone idle?
    boolean stopped;        // is activity pause finished?
    boolean finishing;      // activity in pending finish list?

    // 管理对象
    // 这个对象用于在多个进程中标识 Activity，后面会单独来讲
    final ActivityTaskManagerService mAtmService;// owner
    // 管理 Activity 的 ATMS 对象
    final IApplicationToken.Stub appToken;// window manager token
    // activity 所在的 task
    private TaskRecord task;        // the task this is in.

    // .....
}
```

#### TaskRecord

使用 TaskRecord 对象来描述一个任务（Task）

TaskRecord 主要成员有：

- taskid：任务的 id
- ArrayList mActivities：mActivities 用于存储和管理 task 中的 Activity。
- private ActivityStack mStack：表示当前 TaskRecord 所属的 ActivityStack。系统运行过程中有很多 TaskRecord，ActivityStack 类用于管理组织这些 TaskRecord。
- String affinity：Activity 在 `AndroidManifest.xml` 文件中有一个 `android:taskAffinity=“xxx”` 属性


TaskRecord 中有一个 ActivityRecord 的集合 mActivities，它是以 Stack 的方式来管理其中的 ActivityRecord 的，先启动的 Activity 放到栈底，后启动的 Activity 作为栈顶成员。Android 开发中常说的返回栈就是 TaskRecord 对象。

系统启动的最后阶段会启动 Launcher App，Launcher App 在打开主 Activity 的过程中会创建一个新 TaskRecord 对象，同时创建主 Activity 对应的 ActivityRecord 对象，并将其插入 TaskRecord 内部的栈。

```
class TaskRecord extends ConfigurationContainer {

    // ......

    final int taskId;       // Unique identifier for this task.
    String affinity;        // The affinity name for this task, or null; may change identity.

    /** List of all activities in the task arranged in history order */
    final ArrayList<ActivityRecord> mActivities;

    /** Current stack. Setter must always be used to update the value. */
    private ActivityStack mStack;

    // ......
}

```

>当我们在 Launcher 页面第一次打开一个 App 时，系统会创建一个新 TaskRecord 对象，同时创建 App 主 Activity 对应的 ActivityRecord 对象，并将其插入 TaskRecord 内部的栈。

![[pic/Pasted image 20250701141113.png]]


>我们在主 Activity 中启动当前应用中的另一个 Activity 时（这个 Activity 没有设置 taskAffinity），系统会把新 Activity 对应的 ActivityRecord 插入到栈顶并获得焦点。前一个 ActivityRecord 仍保留在栈中，但对应的 Activity 会被停止，所谓停止就是回调 Activity 的 onPause onStop 回调方法。


>当用户执行返回操作时，当前的 Activity 对应的 ActivityRecord 会从栈顶部弹出并销毁。前一个 ActivityRecord 恢复到栈顶，对应的 Activity 将恢复，恢复到之前的状态，这里的恢复就是执行其 onRestart onStart onResume 回调。


>一般情况下，同一时间，系统中会存在多个 Task，当前显示的 Activity 所在的 Task，我们称之为前台 Task（前台 Task 的栈顶成员会显示在屏幕上），其余均为后台 Task