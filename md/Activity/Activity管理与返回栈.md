
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