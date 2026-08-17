让我查找 `InputManagerService` 的构造方法和初始化逻辑。


现在让我查看 native 层初始化以及 SystemServer 中的调用流程。


现在让我查看 IMS 的 `InputManagerHandler` 和 `onInitReady` 阶段的调用。


## InputManagerService (IMS) 构造方法与初始化详解

---

### 一、IMS 在 SystemServer 中的创建时序

在 [SystemServer.java](file:///c:/D/android_project/cells-android10/frameworks/base/services/java/com/android/server/SystemServer.java#L1023-L1025) 中，IMS 的创建**先于 WMS**：

```
① new InputManagerService(context)        ← L1024 创建 IMS
② WindowManagerService.main(context, inputManager, ...)  ← L1033 创建 WMS（传入 IMS）
③ ServiceManager.addService(INPUT_SERVICE, inputManager) ← L1037 注册到 ServiceManager
④ wm.onInitReady()                        ← L1046 WMS 初始化就绪
⑤ inputManager.start()                    ← （在 startOtherServices 内部调用）
⑥ inputManagerF.systemRunning()           ← L2238 系统就绪回调
```

---

### 二、Java 层构造方法

[InputManagerService(context)](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/input/InputManagerService.java#L313-L329)

```java
public InputManagerService(Context context) {
    this.mContext = context;
    this.mHandler = new InputManagerHandler(DisplayThread.get().getLooper());

    mUseDevInputEventForAudioJack =
            context.getResources().getBoolean(R.bool.config_useDevInputEventForAudioJack);

    mPtr = nativeInit(this, mContext, mHandler.getLooper().getQueue());

    String doubleTouchGestureEnablePath = context.getResources().getString(
            R.string.config_doubleTouchGestureEnableFile);
    mDoubleTouchGestureEnableFile = TextUtils.isEmpty(doubleTouchGestureEnablePath) ? null :
        new File(doubleTouchGestureEnablePath);

    LocalServices.addService(InputManagerInternal.class, new LocalService());
}
```

**构造方法逐步解析：**

| 步骤 | 代码 | 说明 |
|------|------|------|
| 1 | `mContext = context` | 保存系统上下文 |
| 2 | `new InputManagerHandler(DisplayThread.get().getLooper())` | 创建消息处理器，绑定到 **DisplayThread**（与 WMS 共享同一个前台线程，保证低延迟显示操作） |
| 3 | `mUseDevInputEventForAudioJack` | 读取配置：是否使用 `/dev/input/event` 子系统检测音频插孔 |
| 4 | `nativeInit(...)` | **关键步骤** — 通过 JNI 创建 Native 层 InputManager，返回 native 指针 `mPtr` |
| 5 | `mDoubleTouchGestureEnableFile` | 读取双击手势使能文件路径（用于折叠屏等设备） |
| 6 | `LocalServices.addService(...)` | 注册 `InputManagerInternal` 本地服务，供系统进程内其他服务直接调用 |

---

### 三、JNI 层 — nativeInit

[com_android_server_input_InputManagerService.cpp](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/jni/com_android_server_input_InputManagerService.cpp#L1317-L1329)

```cpp
static jlong nativeInit(JNIEnv* env, jclass, jobject serviceObj, 
                        jobject contextObj, jobject messageQueueObj) {
    sp<MessageQueue> messageQueue = android_os_MessageQueue_getMessageQueue(env, messageQueueObj);
    NativeInputManager* im = new NativeInputManager(contextObj, serviceObj,
            messageQueue->getLooper());
    im->incStrong(0);
    return reinterpret_cast<jlong>(im);
}
```

从 Java 层传入 `MessageQueue`，获取其绑定的 `Looper`，然后创建 `NativeInputManager`。

---

### 四、Native 层 — NativeInputManager 构造

[NativeInputManager::NativeInputManager](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/jni/com_android_server_input_InputManagerService.cpp#L333-L354)

```cpp
NativeInputManager::NativeInputManager(jobject contextObj,
        jobject serviceObj, const sp<Looper>& looper) :
        mLooper(looper), mInteractive(true) {
    JNIEnv* env = jniEnv();
    mServiceObj = env->NewGlobalRef(serviceObj);

    {
        AutoMutex _l(mLock);
        mLocked.systemUiVisibility = ASYSTEM_UI_VISIBILITY_STATUS_BAR_VISIBLE;
        mLocked.pointerSpeed = 0;
        mLocked.pointerGesturesEnabled = true;
        mLocked.showTouches = false;
        mLocked.pointerCapture = false;
        mLocked.pointerDisplayId = ADISPLAY_ID_DEFAULT;
    }
    mInteractive = true;

    mInputManager = new InputManager(this, this);
    defaultServiceManager()->addService(String16("inputflinger"), mInputManager, false);
}
```

**初始化内容：**

| 步骤 | 说明 |
|------|------|
| 保存 Looper | 将 DisplayThread 的 Looper 保存到 `mLooper`，用于后续事件循环 |
| JNI 全局引用 | 创建 Java 层 `InputManagerService` 对象的全局引用，防止被 GC 回收 |
| 锁定状态初始化 | `pointerSpeed=0`、`showTouches=false`、`pointerGesturesEnabled=true` 等默认值 |
| **创建 InputManager** | `new InputManager(this, this)` — NativeInputManager 同时充当 ReaderPolicy 和 DispatcherPolicy |
| 注册 Binder 服务 | 将 InputManager 注册为 `"inputflinger"` Binder 服务 |

---

### 五、核心 — InputManager 构造方法

[InputManager::InputManager](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/InputManager.cpp#L33-L40)

```cpp
InputManager::InputManager(
        const sp<InputReaderPolicyInterface>& readerPolicy,
        const sp<InputDispatcherPolicyInterface>& dispatcherPolicy) {
    mDispatcher = new InputDispatcher(dispatcherPolicy);
    mClassifier = new InputClassifier(mDispatcher);
    mReader = createInputReader(readerPolicy, mClassifier);
    initialize();
}
```

**这是整个输入系统的核心架构，创建了三大组件：**

```
InputManager
    │
    ├── InputReader（读取原始输入事件）
    │       ↓ 传递给
    ├── InputClassifier（分类输入事件：触摸/按键/鼠标等）
    │       ↓ 传递给
    └── InputDispatcher（分发事件到目标窗口）
```

| 组件 | 创建方式 | 职责 |
|------|----------|------|
| **InputDispatcher** | `new InputDispatcher(dispatcherPolicy)` | 负责将输入事件分发到正确的窗口，管理焦点、触摸目标查找 |
| **InputClassifier** | `new InputClassifier(mDispatcher)` | 对输入事件进行分类和预处理（如手势识别），作为 Reader 和 Dispatcher 之间的桥梁 |
| **InputReader** | `createInputReader(readerPolicy, mClassifier)` | 从内核 `/dev/input/` 设备节点读取原始输入事件，进行坐标转换、设备管理等 |

随后调用 [initialize()](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/InputManager.cpp#L46-L49)：

```cpp
void InputManager::initialize() {
    mReaderThread = new InputReaderThread(mReader);
    mDispatcherThread = new InputDispatcherThread(mDispatcher);
}
```

创建两个独立的工作线程：
- **InputReaderThread** — 持续轮询 `/dev/input/` 读取原始事件
- **InputDispatcherThread** — 持续将分类后的事件分发到应用窗口

---

### 六、IMS 的 start() 方法

[InputManagerService.start()](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/input/InputManagerService.java#L339-L362)

```java
public void start() {
    nativeStart(mPtr);                     // 启动 native 层的 Reader/Dispatcher 线程

    Watchdog.getInstance().addMonitor(this); // 注册 Watchdog 监控

    registerPointerSpeedSettingObserver();   // 注册指针速度设置观察者
    registerShowTouchesSettingObserver();    // 注册"显示触摸"设置观察者
    registerAccessibilityLargePointerSettingObserver(); // 注册无障碍大指针观察者

    // 注册广播接收器：用户切换时更新设置
    mContext.registerReceiver(new BroadcastReceiver() { ... },
            new IntentFilter(Intent.ACTION_USER_SWITCHED), null, mHandler);

    // 初始加载设置
    updatePointerSpeedFromSettings();
    updateShowTouchesFromSettings();
    updateAccessibilityLargePointerFromSettings();
}
```

其中 `nativeStart` 调用到 native 层 [InputManager::start()](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/InputManager.cpp#L51-L67)：

```cpp
status_t InputManager::start() {
    // 先启动 Dispatcher 线程（优先级 URGENT_DISPLAY）
    status_t result = mDispatcherThread->run("InputDispatcher", PRIORITY_URGENT_DISPLAY);
    // 再启动 Reader 线程
    result = mReaderThread->run("InputReader", PRIORITY_URGENT_DISPLAY);
    return OK;
}
```

---

### 七、systemRunning() — 系统就绪阶段

[InputManagerService.systemRunning()](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/input/InputManagerService.java#L365-L399)

在 `ActivityManagerService.systemReady()` 回调中被调用：

| 步骤 | 说明 |
|------|------|
| 获取 NotificationManager | 用于后续发送键盘布局切换通知 |
| 设置 `mSystemReady = true` | 标记系统已就绪 |
| 注册包管理广播 | 监听 `PACKAGE_ADDED/REMOVED/CHANGED/REPLACED`，触发键盘布局更新 |
| 注册蓝牙别名广播 | 监听 `ACTION_ALIAS_CHANGED`，重新加载设备别名 |
| 发送初始消息 | `MSG_RELOAD_DEVICE_ALIASES` + `MSG_UPDATE_KEYBOARD_LAYOUTS` |
| 通知 WiredAccessory | 调用 `mWiredAccessoryCallbacks.systemReady()` 初始化有线配件检测 |

---

### 八、完整初始化流程图

```
SystemServer.startOtherServices()
    │
    ├── ① new InputManagerService(context)          [Java 层]
    │       │
    │       ├── new InputManagerHandler(DisplayThread.Looper)
    │       │
    │       ├── nativeInit()                         [JNI 层]
    │       │       │
    │       │       └── new NativeInputManager(context, service, looper)  [Native 层]
    │       │               │
    │       │               ├── 初始化锁定状态（指针速度、触摸显示等）
    │       │               │
    │       │               ├── new InputManager(readerPolicy, dispatcherPolicy)
    │       │               │       │
    │       │               │       ├── new InputDispatcher()     ← 事件分发器
    │       │               │       ├── new InputClassifier()     ← 事件分类器
    │       │               │       ├── createInputReader()       ← 事件读取器
    │       │               │       └── initialize()
    │       │               │               ├── new InputReaderThread
    │       │               │               └── new InputDispatcherThread
    │       │               │
    │       │               └── 注册 "inputflinger" Binder 服务
    │       │
    │       └── LocalServices.addService(InputManagerInternal)
    │
    ├── ② WindowManagerService.main(context, inputManager, ...)
    │
    ├── ③ ServiceManager.addService(INPUT_SERVICE, inputManager)
    │
    └── ④ inputManager.start()
            │
            ├── nativeStart() → InputManager::start()
            │       ├── InputDispatcherThread.run()   ← 分发线程启动
            │       └── InputReaderThread.run()       ← 读取线程启动
            │
            ├── 注册 Watchdog 监控
            ├── 注册设置观察者（指针速度、显示触摸等）
            └── 初始加载设置值

SystemServer → AMS.systemReady() callback
    │
    └── ⑤ inputManagerF.systemRunning()
            ├── 获取 NotificationManager
            ├── 注册包管理/蓝牙广播
            └── 通知 WiredAccessory 就绪
```

**总结**：IMS 的初始化是一个**跨三层**（Java → JNI → Native）的过程。Java 层负责生命周期管理和系统服务注册；JNI 层做对象桥接；Native 层构建输入系统的核心三大组件（Reader → Classifier → Dispatcher）并创建独立工作线程。整个输入管道从内核设备读取原始事件，经过分类处理后，最终分发到正确的应用窗口。
