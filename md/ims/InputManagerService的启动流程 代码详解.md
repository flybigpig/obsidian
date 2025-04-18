

`InputManagerService`（IMS）是 Android 系统中负责管理输入设备和分发输入事件的核心服务。下面从代码层面详细剖析其启动流程。

### 1. 从 `SystemServer` 开始

`SystemServer` 是 Android 系统启动过程中非常重要的一个系统进程，它会启动各种系统服务，`InputManagerService` 也在其中。以下是关键代码片段：

  

java

```java
// frameworks/base/services/java/com/android/server/SystemServer.java
public final class SystemServer {
    private void startOtherServices() {
        // ...
        InputManagerService inputManager = new InputManagerService(context);
        // ...
        wm = WindowManagerService.main(context, inputManager,
                mFactoryTestMode != FactoryTest.FACTORY_TEST_LOW_LEVEL,
                !mFirstBoot, mOnlyCore);
        ServiceManager.addService(Context.INPUT_SERVICE, inputManager);
        inputManager.setWindowManagerCallbacks(wm.getInputMonitor());
        inputManager.start();
        // ...
    }
}
```

  

**代码解释**：

  

- `new InputManagerService(context)`：创建 `InputManagerService` 实例。
- `wm = WindowManagerService.main(...)`：创建 `WindowManagerService` 实例。
- `ServiceManager.addService(Context.INPUT_SERVICE, inputManager)`：将 `InputManagerService` 注册到 `ServiceManager` 中，这样其他组件就可以通过 `Context.INPUT_SERVICE` 来获取该服务。
- `inputManager.setWindowManagerCallbacks(wm.getInputMonitor())`：设置 `WindowManagerService` 的输入监视器回调，用于处理输入事件和窗口管理之间的交互。
- `inputManager.start()`：启动 `InputManagerService`。

### 2. `InputManagerService` 构造方法

java

```java
// frameworks/base/services/core/java/com/android/server/input/InputManagerService.java
public InputManagerService(Context context) {
    this.mContext = context;
    this.mHandler = new InputManagerHandler(DisplayThread.get().getLooper());
    mUseDevInputEventForAudioJack =
            context.getResources().getBoolean(R.bool.config_useDevInputEventForAudioJack);
    Slog.i(TAG, "Initializing input manager, mUseDevInputEventForAudioJack="
            + mUseDevInputEventForAudioJack);
    mPtr = nativeInit(this, mContext, mHandler.getLooper().getQueue());
    // ...
}
```

  

**代码解释**：

  

- `this.mHandler = new InputManagerHandler(DisplayThread.get().getLooper())`：创建一个 `InputManagerHandler` 实例，用于处理输入相关的消息。
- `mPtr = nativeInit(this, mContext, mHandler.getLooper().getQueue())`：调用 `nativeInit` 方法，这是一个 JNI 方法，会进入到 C++ 层进行初始化操作。

### 3. `nativeInit` 方法（JNI 层）

cpp

```cpp
// frameworks/base/services/core/jni/com_android_server_input_InputManagerService.cpp
static jlong nativeInit(JNIEnv* env, jclass /* clazz */,
        jobject serviceObj, jobject contextObj, jobject messageQueueObj) {
    sp<MessageQueue> messageQueue = android_os_MessageQueue_getMessageQueue(env, messageQueueObj);
    if (messageQueue == nullptr) {
        jniThrowRuntimeException(env, "MessageQueue is not initialized.");
        return 0;
    }

    NativeInputManager* im = new NativeInputManager(contextObj, serviceObj,
            messageQueue->getLooper());
    im->incStrong(0);
    return reinterpret_cast<jlong>(im);
}
```

  

**代码解释**：

  

- `sp<MessageQueue> messageQueue = android_os_MessageQueue_getMessageQueue(env, messageQueueObj)`：获取 Java 层的消息队列。
- `NativeInputManager* im = new NativeInputManager(contextObj, serviceObj, messageQueue->getLooper())`：创建 `NativeInputManager` 实例。

### 4. `NativeInputManager` 构造方法

cpp

```cpp
// frameworks/base/services/core/jni/com_android_server_input_InputManagerService.cpp
NativeInputManager::NativeInputManager(jobject contextObj,
        jobject serviceObj, const sp<Looper>& looper) :
        mLooper(looper), mInteractive(true) {
    JNIEnv* env = jniEnv();

    mServiceObj = env->NewGlobalRef(serviceObj);
    mContextObj = env->NewGlobalRef(contextObj);

    {
        AutoMutex _l(mLock);
        mInputManager = new InputManager(this, this);
    }

    mInputManager->start();
}
```

  

**代码解释**：

  

- `mInputManager = new InputManager(this, this)`：创建 `InputManager` 实例，该实例负责管理输入设备和事件分发。
- `mInputManager->start()`：启动 `InputManager`。

### 5. `InputManager` 构造方法及 `start` 方法

cpp

```cpp
// frameworks/native/services/inputflinger/InputManager.cpp
InputManager::InputManager(
        const sp<InputReaderPolicyInterface>& readerPolicy,
        const sp<InputDispatcherPolicyInterface>& dispatcherPolicy) {
    mDispatcher = new InputDispatcher(dispatcherPolicy);
    mReader = new InputReader(readerPolicy, mDispatcher);
    initialize();
}

void InputManager::start() {
    mReaderThread = new InputReaderThread(mReader);
    mDispatcherThread = new InputDispatcherThread(mDispatcher);
    mReaderThread->run("InputReader", PRIORITY_URGENT_DISPLAY);
    mDispatcherThread->run("InputDispatcher", PRIORITY_URGENT_DISPLAY);
}
```

  

  

![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIQAAAAgCAYAAADTydBfAAAACXBIWXMAABYlAAAWJQFJUiTwAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAQASURBVHgB7ZpPbtpAFMa/IaQgNZVYBYekqnuDdlOpm8rddlM4Adwg7QVacoKQE5ScoFl1W6ubSt00Ug8QRwqJUTdIVRUIhOkbMKnH2I7/EByk+UkIMp4ZE+Z53vfeG8bfvDgBoCMbLPblx1Mo7g05ZGcMyPjeCh9yUChcKINQSCiDUEjksQLoul7q91HCArBty0pzj6DxSdE0XXf/XSyiZ1lWDzFJ+xvN/q+VMIh+f1jlwCcsAE3beW3bZ6bPPVp0j/pt48tbOz3O+VEO63tpjUMYA8fwxN12OcAxvT1HTKJ+/+DvMv1dlMuIC+clBjQ4G/7crFSqSMEYw4/z8+MZLY6BjFAGkRSOEhuzz2kWjwzL8J2ajXeREcogUsLBE7kyTdtuIDAPwwyhCZABjDKVHBlCmUqGFGjkiDnWJD/ctTup5gy9F8vvk9uQXAUD89UlYZS17a8I2CGcOUmjnDWxZNQOEQObVGT34qxGH03PJQMxcCILI6wPZ/wtMkAZRAJo+zmVW/gTxGLU8Jn0QJ4yG3G5EmHnfYN8rMcA2CliQLpDCg/JPZj9Qr5ZGIzqIoq56ccmrsmMMuei8hBqh4iB+NE3tYoIFQ13ey43Po46x2blsVhkXW7lh71pMurQ070eVVxO8xDDk6Sv2W6kdgiH8tZ2i57OUL99ORhRDoJ5F8g6Pz8/QkQY597kkWXbnfb0Go5o9/kfclJoezkYNuhTC0tC7RAOtBi00EwPfbm285txwB4iMhGT8xHKza7gRCqmNIhjqeJSGUQKpqFhpx21P2U3fTKbo7bUJ5c78HQwlikulctIAkOPcVaLm3ugp31XngZtEcq6264erJmFAe/FFZfF4vo7EpVNJEQU1cS7MgiH6zW01q5Z2+8aZ6jSAkm+HTERTzlFF7rcyrwiUpQ6e9T3gLSEu84hxGUzrArqXItdJfWiDMLhd6cTGCmUdP14LiScpqwjnwcdU6jpSZ9aQTtMv5hvFfquwtcSxaXSEBGYhoTcKx51epKbUcYLMSkqpO62MDHqhKCm1LgkcZluh3j4CHj5Klrf79+Av3+wqnQvOi2qP4hFMWZt5Nt3abHbt5+LGBrzbddm2AghWGkXMlxNE3EZtKtQsayd5jzErB6TziA2NoD3H6L1/VVbaYMQzC0SbeWcXe3Tp1rYOI8e8BWTXsTiiMM4STOXSVEuIwaTp5MxT82BVcPCQueaLrfOi0k/KDfiDUHrd10WVwYRkwHVHMgoJDUfdiZiDL/MZLRwVYhLyDeaics7I/15iPJWtH7dC9/mtOchBN6Dqos+COvFr5AUdDg27SHaqOPTFrdm8678ARnFYlEuQyGhDEIhoQxCIUEGwSxkRpb3VvjxD2XE04wfmQluAAAAAElFTkSuQmCC)

  

**代码解释**：

  

- `mDispatcher = new InputDispatcher(dispatcherPolicy)`：创建 `InputDispatcher` 实例，负责将输入事件分发给合适的窗口。
- `mReader = new InputReader(readerPolicy, mDispatcher)`：创建 `InputReader` 实例，负责从输入设备读取输入事件。
- `mReaderThread = new InputReaderThread(mReader)` 和 `mDispatcherThread = new InputDispatcherThread(mDispatcher)`：分别创建 `InputReaderThread` 和 `InputDispatcherThread` 线程。
- `mReaderThread->run("InputReader", PRIORITY_URGENT_DISPLAY)` 和 `mDispatcherThread->run("InputDispatcher", PRIORITY_URGENT_DISPLAY)`：启动这两个线程，开始读取和分发输入事件。

### 总结

`InputManagerService` 的启动流程涉及 Java 层和 C++ 层的交互，主要步骤包括：

  

1. 在 `SystemServer` 中创建 `InputManagerService` 实例。
2. 在 `InputManagerService` 构造方法中调用 `nativeInit` 方法进入 C++ 层。
3. 在 C++ 层创建 `NativeInputManager`、`InputManager`、`InputReader` 和 `InputDispatcher` 等对象。
4. 启动 `InputReaderThread` 和 `InputDispatcherThread` 线程，开始读取和分发输入事件。