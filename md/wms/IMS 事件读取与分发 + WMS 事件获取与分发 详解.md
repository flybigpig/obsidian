让我全面搜索 IMS 和 WMS 的事件读取与分发相关代码。


## IMS 事件读取与分发 + WMS 事件获取与分发 详解

---

### 一、整体事件流架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          内核空间                                        │
│   /dev/input/event0  /dev/input/event1  /dev/input/event2 ...          │
│   (触摸屏)            (按键)             (鼠标/轨迹球)                    │
└────────────┬────────────────────────────────────────────────────────────┘
             │ read()
             ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  Native 层 — InputReaderThread (优先级: PRIORITY_URGENT_DISPLAY)         │
│                                                                         │
│  EventHub::getEvents()        ← 从 /dev/input/ 读取原始内核事件           │
│       ↓                                                                 │
│  InputReader::loopOnce()      ← 主循环                                   │
│       ↓                                                                 │
│  InputReader::processEventsLocked() ← 按设备分批处理                      │
│       ↓                                                                 │
│  InputDevice::process()       ← 各 Mapper 转换坐标/按键映射               │
│       ↓                                                                 │
│  InputClassifier::notifyMotion/Key() ← 事件分类（触摸手势识别）            │
│       ↓                                                                 │
│  InputDispatcher::notifyMotion/Key() ← 加入 InboundQueue               │
└────────────┬────────────────────────────────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  Native 层 — InputDispatcherThread (优先级: PRIORITY_URGENT_DISPLAY)     │
│                                                                         │
│  InputDispatcher::dispatchOnce()          ← 分发主循环                   │
│       ↓                                                                 │
│  InputDispatcher::dispatchOnceInnerLocked() ← 从 InboundQueue 取事件     │
│       ↓                                                                 │
│  ┌─ Policy 拦截 ─────────────────────────────────────────────────────┐  │
│  │ interceptKeyBeforeQueueing() → JNI → Java PhoneWindowManager     │  │
│  │ interceptKeyBeforeDispatching() → JNI → Java PhoneWindowManager  │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│       ↓                                                                 │
│  dispatchKey/Motion Locked()   ← 查找目标窗口 (InputTarget)              │
│       ↓                                                                 │
│  dispatchEventLocked()         ← 遍历 InputTarget 列表                   │
│       ↓                                                                 │
│  prepareDispatchCycleLocked()  ← 通过 Connection 序列化事件               │
│       ↓                                                                 │
│  InputChannel (Socket)         ← 通过 Unix Socket 发送到应用进程          │
└────────────┬────────────────────────────────────────────────────────────┘
             │ Unix Domain Socket
             ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  Java 层 — 应用进程                                                      │
│                                                                         │
│  InputEventReceiver::onInputEvent()  ← Looper 监听到 Socket 可读         │
│       ↓                                                                 │
│  WindowInputEventReceiver::onInputEvent() ← ViewRootImpl 内部类          │
│       ↓                                                                 │
│  ViewRootImpl::enqueueInputEvent()   ← 加入事件队列                      │
│       ↓                                                                 │
│  ViewRootImpl::doProcessInputEvents() ← 遍历队列处理                      │
│       ↓                                                                 │
│  View::dispatchKeyEvent/MotionEvent() ← 最终到达 View 层级               │
└─────────────────────────────────────────────────────────────────────────┘
```

---

### 二、IMS 事件读取（InputReader 层）

#### 2.1 InputReaderThread 主循环

[InputReaderThread::threadLoop()](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/InputReaderBase.cpp#L45-L48)

```cpp
bool InputReaderThread::threadLoop() {
    mReader->loopOnce();  // 每次循环读取并处理一批事件
    return true;          // 持续运行
}
```

#### 2.2 EventHub — 从内核读取原始事件

[EventHub::getEvents()](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/EventHub.cpp#L815)

```cpp
size_t EventHub::getEvents(int timeoutMillis, RawEvent* buffer, size_t bufferSize) {
    struct input_event readBuffer[bufferSize];  // Linux 内核输入事件缓冲区

    for (;;) {
        // 1. 检查是否需要重新打开设备
        if (mNeedToReopenDevices) { closeAllDevicesLocked(); ... }

        // 2. 报告已关闭/已添加的设备
        while (mClosingDevices) { ... generate DEVICE_REMOVED ... }
        while (mOpeningDevices != nullptr) { ... generate DEVICE_ADDED ... }

        // 3. 使用 epoll 等待内核事件
        //    监听所有 /dev/input/eventX 文件描述符
        struct epoll_event eventBuffer[EPOLL_MAX_EVENTS];
        int eventCount = epoll_wait(mEpollFd, eventBuffer, EPOLL_MAX_EVENTS, timeoutMillis);

        // 4. 从设备文件读取原始事件
        for (int i = 0; i < eventCount; i++) {
            int fd = eventBuffer[i].data.fd;
            if (fd == mWakeReadFd) { /* 唤醒事件 */ }
            else {
                // 从 /dev/input/eventX 读取 Linux input_event 结构
                size_t size = read(fd, readBuffer, sizeof(struct input_event) * capacity);
                // 转换为 RawEvent 格式
                for (size_t j = 0; j < size / sizeof(struct input_event); j++) {
                    event->deviceId = deviceId;
                    event->type = readBuffer[j].type;    // EV_KEY / EV_ABS / EV_REL ...
                    event->code = readBuffer[j].code;    // 具体键码/轴码
                    event->value = readBuffer[j].value;  // 按键状态/坐标值
                }
            }
        }
    }
}
```

**核心机制**：通过 `epoll` 同时监听所有输入设备文件描述符，当任何设备有数据可读时立即唤醒并批量读取。

#### 2.3 InputReader::loopOnce() — 事件处理主循环

[InputReader::loopOnce()](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/InputReader.cpp#L286-L326)

```cpp
void InputReader::loopOnce() {
    // 1. 从 EventHub 获取原始事件（阻塞等待）
    size_t count = mEventHub->getEvents(timeoutMillis, mEventBuffer, EVENT_BUFFER_SIZE);

    {
        AutoMutex _l(mLock);

        // 2. 按设备分批处理事件
        if (count) {
            processEventsLocked(mEventBuffer, count);
        }

        // 3. 处理超时（如手势超时）
        if (mNextTimeout != LLONG_MAX) {
            if (now >= mNextTimeout) {
                timeoutExpiredLocked(now);
            }
        }

        // 4. 如果有设备变化，通知 listener
        if (inputDevicesChanged) {
            mListener->onInputDevicesChanged(inputDevices);
        }
    }
}
```

#### 2.4 processEventsLocked — 按设备分类处理

[InputReader::processEventsLocked()](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/InputReader.cpp#L350-L380)

```cpp
void InputReader::processEventsLocked(const RawEvent* rawEvents, size_t count) {
    for (const RawEvent* rawEvent = rawEvents; count;) {
        int32_t type = rawEvent->type;

        if (type < FIRST_SYNTHETIC_EVENT) {
            // 普通硬件事件（按键/触摸/鼠标移动等）
            // 按相同 deviceId 批量处理
            processEventsForDeviceLocked(deviceId, rawEvent, batchSize);
        } else {
            // 合成事件（设备添加/移除/扫描完成）
            switch (type) {
                case DEVICE_ADDED:   addDeviceLocked(...); break;
                case DEVICE_REMOVED: removeDeviceLocked(...); break;
                case FINISHED_DEVICE_SCAN: handleConfigurationChangedLocked(...); break;
            }
        }
    }
}
```

#### 2.5 InputDevice::process() — 各 Mapper 处理

[InputReader::processEventsForDeviceLocked()](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/InputReader.cpp#L522-L537)

```cpp
void InputReader::processEventsForDeviceLocked(int32_t deviceId,
        const RawEvent* rawEvents, size_t count) {
    InputDevice* device = mDevices.valueAt(deviceIndex);
    device->process(rawEvents, count);
    // 内部遍历所有 Mapper：
    //   SwitchMapper    — 翻盖/滑块等开关事件
    //   KeyboardMapper  — 键码映射（扫描码 → 键码）
    //   TouchMapper     — 触摸坐标旋转/缩放
    //   CursorMapper    — 鼠标/轨迹球坐标转换
    //   VibratorMapper  — 振动反馈
    // 最终调用 notifyKey() / notifyMotion() 传递给 Classifier
}
```

---

### 三、IMS 事件分类（InputClassifier 层）

[InputClassifier::notifyMotion()](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/InputClassifier.cpp#L416-L428)

```cpp
void InputClassifier::notifyMotion(const NotifyMotionArgs* args) {
    // 仅触摸事件需要分类
    const bool sendToMotionClassifier = mMotionClassifier && isTouchEvent(*args);
    if (!sendToMotionClassifier) {
        mListener->notifyMotion(args);  // 非触摸事件直接传递给 Dispatcher
        return;
    }

    // 触摸事件经过 MotionClassifier 分类
    NotifyMotionArgs newArgs(*args);
    newArgs.classification = mMotionClassifier->classify(newArgs);
    // 分类结果：AMBIGUOUS_GESTURE / CERTAIN_GESTURE / NO_GESTURE 等
    mListener->notifyMotion(&newArgs);  // 传递给 InputDispatcher
}
```

---

### 四、IMS 事件分发（InputDispatcher 层）

#### 4.1 InputDispatcherThread 主循环

[InputDispatcherThread::threadLoop()](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/InputDispatcher.cpp#L5238-L5241)

```cpp
bool InputDispatcherThread::threadLoop() {
    mDispatcher->dispatchOnce();  // 每次循环执行一次分发
    return true;
}
```

#### 4.2 InputDispatcher::dispatchOnce()

[InputDispatcher::dispatchOnce()](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/InputDispatcher.cpp#L265-L288)

```cpp
void InputDispatcher::dispatchOnce() {
    nsecs_t nextWakeupTime = LONG_LONG_MAX;
    {
        std::scoped_lock _l(mLock);

        // 1. 如果没有待处理的命令，执行分发循环
        if (!haveCommandsLocked()) {
            dispatchOnceInnerLocked(&nextWakeupTime);
        }

        // 2. 执行所有待处理的命令
        if (runCommandsLockedInterruptible()) {
            nextWakeupTime = LONG_LONG_MIN;  // 立即唤醒
        }
    }

    // 3. 等待下一次唤醒（通过 Looper epoll）
    int timeoutMillis = toMillisecondTimeoutDelay(currentTime, nextWakeupTime);
    mLooper->pollOnce(timeoutMillis);
}
```

#### 4.3 dispatchOnceInnerLocked — 核心分发逻辑

[InputDispatcher::dispatchOnceInnerLocked()](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/InputDispatcher.cpp#L290-L430)

```cpp
void InputDispatcher::dispatchOnceInnerLocked(nsecs_t* nextWakeupTime) {
    // 1. 分发冻结时不处理任何事件
    if (mDispatchFrozen) return;

    // 2. 如果没有待分发事件，从 InboundQueue 取出一个
    if (!mPendingEvent) {
        if (mInboundQueue.isEmpty()) {
            // 检查是否需要合成按键重复事件
            if (mKeyRepeatState.lastKeyEntry && currentTime >= nextRepeatTime) {
                mPendingEvent = synthesizeKeyRepeatLocked(currentTime);
            }
            if (!mPendingEvent) return;  // 无事可做
        } else {
            mPendingEvent = mInboundQueue.dequeueAtHead();  // 从队列头部取出
        }
        pokeUserActivityLocked(mPendingEvent);  // 用户活动检测
        resetANRTimeoutsLocked();                // 重置 ANR 计时
    }

    // 3. 根据事件类型分发
    switch (mPendingEvent->type) {
        case TYPE_CONFIGURATION_CHANGED:
            dispatchConfigurationChangedLocked(...); break;
        case TYPE_DEVICE_RESET:
            dispatchDeviceResetLocked(...); break;
        case TYPE_KEY:
            dispatchKeyLocked(...); break;       // ★ 按键事件分发
        case TYPE_MOTION:
            dispatchMotionLocked(...); break;    // ★ 触摸/鼠标事件分发
    }
}
```

#### 4.4 Policy 拦截 — 系统按键拦截

在分发到应用之前，InputDispatcher 会通过 JNI 回调 Java 层的 `PhoneWindowManager` 进行策略拦截：

**Native → JNI → Java 调用链：**

```
InputDispatcher (Native)
    │
    ├── interceptKeyBeforeQueueing()     ← 按键入队前拦截
    │       ↓ JNI
    │   NativeInputManager::interceptKeyBeforeQueueing()
    │       ↓ JNI CallIntMethod
    │   InputManagerService.interceptKeyBeforeQueueing()
    │       ↓
    │   WindowManagerCallbacks.interceptKeyBeforeQueueing()
    │       ↓
    │   PhoneWindowManager.interceptKeyBeforeQueueing()  ← 处理电源键/Home键/音量键等
    │
    └── interceptKeyBeforeDispatching()  ← 按键分发前拦截
            ↓ (同上调用链)
        PhoneWindowManager.interceptKeyBeforeDispatching()  ← 处理菜单键等
```

[PhoneWindowManager.interceptKeyBeforeQueueing()](file:///c:/D/android_project/cells-android10/frameworks/base/services/core/java/com/android/server/policy/PhoneWindowManager.java#L3667) 负责拦截：
- **电源键** → 亮屏/锁屏/关机对话框
- **Home 键** → 回到桌面
- **音量键** → 音量调节
- **ENDCALL 键** → 挂断电话
- **MENU 键** → 菜单显示

#### 4.5 dispatchEventLocked — 发送到目标窗口

[InputDispatcher::dispatchEventLocked()](file:///c:/D/android_project/cells-android10/frameworks/native/services/inputflinger/InputDispatcher.cpp#L1018-L1033)

```cpp
void InputDispatcher::dispatchEventLocked(nsecs_t currentTime,
        EventEntry* eventEntry, const std::vector<InputTarget>& inputTargets) {
    pokeUserActivityLocked(eventEntry);

    for (const InputTarget& inputTarget : inputTargets) {
        ssize_t connectionIndex = getConnectionIndexLocked(inputTarget.inputChannel);
        if (connectionIndex >= 0) {
            sp<Connection> connection = mConnectionsByFd.valueAt(connectionIndex);
            // 通过 Connection 将事件序列化并通过 InputChannel 发送
            prepareDispatchCycleLocked(currentTime, connection, eventEntry, &inputTarget);
        }
    }
}
```

**InputTarget 查找过程**（以触摸事件为例）：
1. 根据触摸坐标查找命中的窗口（`findTouchedWindowLocked`）
2. 考虑窗口层级（Z-order）、可见性、可触摸性
3. 处理多窗口同时触摸（split touch）
4. 返回 `InputTarget` 列表（包含 InputChannel 和坐标偏移）

---

### 五、WMS 事件接收（Java 应用层）

#### 5.1 InputChannel — 进程间通信管道

```
InputDispatcher (system_server 进程)
    │
    │  Unix Domain Socket（双向管道）
    │  ┌──────────────────────────┐
    │  │  InputChannel (server端)  │ ←→  InputChannel (client端)
    │  └──────────────────────────┘
    │
    ▼
ViewRootImpl (应用进程)
```

当 WMS 调用 `addWindow()` 时，会创建一对 `InputChannel`：
- 服务端注册到 InputDispatcher 的 Connection
- 客户端传递给应用进程的 ViewRootImpl

#### 5.2 InputEventReceiver — 接收输入事件

[InputEventReceiver](file:///c:/D/android_project/cells-android10/frameworks/base/core/java/android/view/InputEventReceiver.java#L33-L53)

```java
public abstract class InputEventReceiver {
    private InputChannel mInputChannel;
    private MessageQueue mMessageQueue;

    public InputEventReceiver(InputChannel inputChannel, Looper looper) {
        mInputChannel = inputChannel;
        mMessageQueue = looper.getQueue();
        // JNI 初始化：将 InputChannel 注册到 Looper 的 epoll 中
        mReceiverPtr = nativeInit(new WeakReference<>(this), inputChannel, looper.getQueue());
    }

    // 当 epoll 检测到 InputChannel 可读时回调
    public void onInputEvent(InputEvent event) { ... }
}
```

#### 5.3 WindowInputEventReceiver — ViewRootImpl 内部类

[WindowInputEventReceiver](file:///c:/D/android_project/cells-android10/frameworks/base/core/java/android/view/ViewRootImpl.java#L7851-L7871)

```java
final class WindowInputEventReceiver extends InputEventReceiver {
    @Override
    public void onInputEvent(InputEvent event) {
        // 兼容性处理
        List<InputEvent> processedEvents =
                mInputCompatProcessor.processInputEventForCompatibility(event);
        // 入队到 ViewRootImpl 的事件队列
        enqueueInputEvent(event, this, 0, true);  // processImmediately = true
    }
}
```

#### 5.4 ViewRootImpl 事件队列处理

[ViewRootImpl.enqueueInputEvent()](file:///c:/D/android_project/cells-android10/frameworks/base/core/java/android/view/ViewRootImpl.java#L7652-L7677)

```java
void enqueueInputEvent(InputEvent event, InputEventReceiver receiver,
        int flags, boolean processImmediately) {
    QueuedInputEvent q = obtainQueuedInputEvent(event, receiver, flags);

    // 加入链表队列尾部（保证顺序）
    QueuedInputEvent last = mPendingInputEventTail;
    if (last == null) {
        mPendingInputEventHead = q;
        mPendingInputEventTail = q;
    } else {
        last.mNext = q;
        mPendingInputEventTail = q;
    }
    mPendingInputEventCount++;

    if (processImmediately) {
        doProcessInputEvents();    // 立即处理
    } else {
        scheduleProcessInputEvents();  // 延迟到下一帧处理
    }
}
```

最终调用链：
```
doProcessInputEvents()
    → deliverInputEvent()
        → processInputEvent()
            → View.dispatchKeyEvent() / View.dispatchTouchEvent()
                → Activity.onKeyDown() / Activity.onTouchEvent()
```

---

### 六、完整事件生命周期总结

```
用户触摸屏幕
    │
    ▼ [内核]
    /dev/input/event0 产生 EV_ABS 事件
    │
    ▼ [InputReaderThread]
    EventHub::getEvents() ← epoll_wait 唤醒 + read() 读取
    │
    ▼
    InputReader::loopOnce()
    │
    ├── processEventsLocked() ← 按设备分批
    │       ↓
    │   InputDevice::process() ← TouchMapper 坐标转换
    │       ↓
    │   notifyMotion() ← 传递给 Classifier
    │
    ▼ [InputClassifier]
    MotionClassifier::classify() ← 手势分类（模糊/确定/无）
    │
    ▼
    InputDispatcher::notifyMotion() ← 加入 InboundQueue
    │
    ▼ [InputDispatcherThread]
    InputDispatcher::dispatchOnce()
    │
    ├── dispatchOnceInnerLocked()
    │       ↓
    │   mInboundQueue.dequeueAtHead() ← 取出事件
    │       ↓
    │   dispatchMotionLocked()
    │       ↓
    │   findTouchedWindowLocked() ← 查找触摸命中的窗口
    │       ↓
    │   dispatchEventLocked()
    │       ↓
    │   prepareDispatchCycleLocked() ← 序列化事件
    │       ↓
    │   Connection::send → InputChannel (Socket写入)
    │
    ▼ [Unix Domain Socket]
    跨进程传输到应用进程
    │
    ▼ [应用进程 - 主线程]
    InputEventReceiver (epoll 监听 Socket 可读)
    │
    ▼
    WindowInputEventReceiver::onInputEvent()
    │
    ▼
    ViewRootImpl::enqueueInputEvent()
    │
    ▼
    ViewRootImpl::doProcessInputEvents()
    │
    ▼
    DecorView → ViewGroup → View
    Activity.dispatchTouchEvent() / View.onTouchEvent()
```

**关键设计要点：**

| 设计            | 说明                                                         |
| --------------- | ------------------------------------------------------------ |
| **双线程架构**  | Reader 和 Dispatcher 各自独立线程，互不阻塞                  |
| **epoll 驱动**  | EventHub 和 InputEventReceiver 都使用 epoll 实现零延迟事件感知 |
| **策略拦截**    | 通过 `PhoneWindowManager` 在入队前和分发前两次拦截系统按键   |
| **Socket 通信** | InputChannel 基于 Unix Domain Socket，支持跨进程零拷贝传输   |
| **事件队列**    | 应用层使用链表队列保证事件顺序，支持立即/延迟处理            |
| **ANR 监控**    | Dispatcher 对每个事件设置超时计时器，应用未及时处理则触发 ANR |