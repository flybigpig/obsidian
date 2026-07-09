# EventHub / InputReader 设备读取细节 源码解析（Android 10）

本篇承接前序 Input 投递专题，聚焦输入链路的**最源头**：内核 `evdev` 设备节点如何被
EventHub 发现、枚举、读取，以及 InputReader 如何把原始 `input_event` 变成"设备级事件"
交给 InputDispatcher（前序已分析）。

---

## 一、线程模型：InputManager 起点

```cpp
// frameworks/native/services/inputflinger/InputManager.cpp:51
status_t InputManager::start() {
    mDispatcherThread->run("InputDispatcher", PRIORITY_URGENT_DISPLAY); // 前序专题
    mReaderThread->run("InputReader",       PRIORITY_URGENT_DISPLAY);   // 本篇
}
```

- `EventHub` 由 `InputReader` 持有，单例运行在 `InputReaderThread`（独立线程，`PRIORITY_URGENT_DISPLAY`）。
- `InputReader` 不直接读 `/dev/input`，而是循环调 `EventHub::getEvents()` 取"已分类的 RawEvent 批次"。
- 读出的事件经 `InputReader` 加工（按设备/类型分派给各 `InputMapper`）后，通过 `mQueuedListener`（= `InputClassifier` → `InputDispatcher`）送往下一级（前序 `InputDispatcher.dispatchOnce`）。

---

## 二、EventHub 初始化：epoll + inotify + wake pipe

```cpp
// EventHub.cpp:253  构造函数
mEpollFd  = epoll_create1(EPOLL_CLOEXEC);
mINotifyFd = inotify_init();
mInputWd = inotify_add_watch(mINotifyFd, DEVICE_PATH, IN_DELETE | IN_CREATE); // /dev/input
// （可选）VIDEO_DEVICE_PATH 的 inotify，用于多点触控的 video frame
// 把 mINotifyFd 加入 epoll
epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mINotifyFd, &eventItem);

// wake pipe：用于外部 wake() 打断阻塞的 epoll_wait
pipe(wakeFds);
epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mWakeReadPipeFd, &eventItem);
```

三类 fd 都挂在一个 epoll 实例上：
| fd | 作用 |
|----|------|
| `mINotifyFd` | 监听 `/dev/input` 目录增删，实现设备热插拔 |
| 各设备 `device->fd` | 真实 evdev 节点的可读事件 |
| `mWakeReadPipeFd` | 被 `wake()` 写入时唤醒 `getEvents`，用于配置变更/重扫 |

---

## 三、getEvents 主循环：读事件 + 设备热插拔

```cpp
// EventHub.cpp:815  getEvents(timeoutMillis, RawEvent* buffer, bufferSize)
for (;;) {
    // 1) 需要重开设备（配置变更）→ closeAll + 置 mNeedToScanDevices，break 让调用方重扫
    if (mNeedToReopenDevices) { ...; break; }

    // 2) 报告已移除设备（DEVICE_REMOVED 合成 RawEvent）
    while (mClosingDevices) { ... event->type = DEVICE_REMOVED; ... }

    // 3) 需要扫描 → scanDevicesLocked()，并发 FINISHED_DEVICE_SCAN
    if (mNeedToScanDevices) { scanDevicesLocked(); }

    // 4) 报告新打开设备（DEVICE_ADDED）
    while (mOpeningDevices) { ... event->type = DEVICE_ADDED; ... }

    // 5) 处理 epoll 已就绪的事件
    while (mPendingEventIndex < mPendingEventCount) {
        fd = mPendingEventItems[mPendingEventIndex++].data.fd;
        if (fd == mINotifyFd)   mPendingINotify = true;          // 目录变化，先记下来
        else if (fd == mWakeReadPipeFd) { read(...); awoken = true; }
        else {
            Device* device = getDeviceByFdLocked(fd);
            // 若是 video device → read video frame
            // 否则是 input 事件：
            readSize = read(device->fd, readBuffer, sizeof(input_event)*capacity);
            // 把 struct input_event 转成 RawEvent（when/deviceId/type/code/value）
        }
    }

    // 6) 所有 pending 事件处理完后，再处理 inotify（避免关设备前漏读）
    if (mPendingINotify && done) { readNotifyLocked(); deviceChanged = true; }
    if (deviceChanged) continue;            // 设备变了，重新走 1)~6)
    if (event != buffer || awoken) break;   // 有事件就返回

    // 7) 没有事件 → 释放 wake lock，进入 epoll_wait 阻塞
    release_wake_lock(WAKE_LOCK_ID);
    pollResult = epoll_wait(mEpollFd, mPendingEventItems, EPOLL_MAX_EVENTS, timeoutMillis);
    acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_ID);
}
return event - buffer;
```

### Wake lock 的微妙配合（EventHub.cpp:1012 注释）
- EventHub **始终持有用户态 PARTIAL_WAKE_LOCK**，仅在 `epoll_wait()` 阻塞期间释放。
- 设备驱动有未读事件时持有内核 wake lock；读完最后一个事件后内核释放→此时用户锁仍持有，保证系统不会在"正在处理事件"时睡死。
- timeout 只是建议值，设备睡眠时不会专门被唤醒来服务它。

---

## 四、设备枚举：openDeviceLocked

首次 `scanDevicesLocked` → `scanDirLocked(DEVICE_PATH)` → 对目录下每个节点 `openDeviceLocked`。

```cpp
// EventHub.cpp:1189  openDeviceLocked(devicePath)
int fd = open(devicePath, O_RDWR | O_CLOEXEC | O_NONBLOCK);   // 打开 evdev 节点

// 通过 ioctl 向驱动查询设备属性：
ioctl(fd, EVIOCGNAME, buffer);    // 设备名
ioctl(fd, EVIOCGVERSION, ...);    // 驱动版本
ioctl(fd, EVIOCGID,   &inputId);  // bus/vendor/product/version
ioctl(fd, EVIOCGPHYS, ...);       // 物理位置（如 "input0"）
ioctl(fd, EVIOCGUNIQ, ...);       // 唯一 id
assignDescriptorLocked(identifier);// 计算 descriptor（稳定指纹，用于映射布局）

// 关键：用 EVIOCGBIT 取设备"能报哪些事件类"的位掩码
ioctl(fd, EVIOCGBIT(EV_KEY, ...), keyBitmask);
ioctl(fd, EVIOCGBIT(EV_ABS, ...), absBitmask);  // 绝对坐标（触控）
ioctl(fd, EVIOCGBIT(EV_REL, ...), relBitmask);  // 相对坐标（鼠标）
ioctl(fd, EVIOCGBIT(EV_SW,  ...), swBitmask);
ioctl(fd, EVIOCGBIT(EV_LED, ...), ledBitmask);
ioctl(fd, EVIOCGPROP,           propBitmask);

// 由位掩码推导 device->classes：
if (keyboard keys)        device->classes |= INPUT_DEVICE_CLASS_KEYBOARD;
if (BTN_MOUSE+REL_X+REL_Y) device->classes |= INPUT_DEVICE_CLASS_CURSOR;
if (EV_ABS multitouch)     device->classes |= INPUT_DEVICE_CLASS_TOUCH;  // 单/多指
// ... switch / joystick / rotaryEncoder / external stylus / vibrator / mic 等
```

`classes` 是后续 InputReader 决定"给该设备安装哪些 InputMapper"的唯一依据。
设备 fd 通过 `registerDeviceForEpollLocked`（`EventHub.cpp:1140`，`EPOLLIN | EPOLLWAKEUP`）加入 epoll，之后有事件时 `epoll_wait` 才会返回它的 fd。

> 这些 `EVIOCG*` / `EVIOCGBIT` 都是 Linux evdev 标准 ioctl，对应内核 `drivers/input/evdev.c`。
> 设备节点由内核 input 子系统在设备注册时于 `/dev/input/eventN` 创建。

---

## 五、热插拔：inotify → readNotifyLocked

```cpp
// EventHub.cpp:1810  readNotifyLocked()
read(mINotifyFd, event_buf, ...);              // 读 inotify_event
while (...) {
    if (event->wd == mInputWd) {
        filename = DEVICE_PATH + "/" + event->name;
        if (event->mask & IN_CREATE) openDeviceLocked(filename);          // 插上
        else                         closeDeviceByPathLocked(filename);   // 拔下
    } else if (event->wd == mVideoWd) {
        // 触控 video 设备同理
    }
}
```

`openDeviceLocked` 结束后把设备挂到 `mOpeningDevices` 链表，下一次 `getEvents` 循环会把它
作为 `DEVICE_ADDED` 合成 RawEvent 上报。InputReader 据此 `addDeviceLocked` 动态增删设备。

---

## 六、InputReader：原始事件 → 设备级事件

```cpp
// InputReader.cpp:286  loopOnce()
size_t count = mEventHub->getEvents(timeoutMillis, mEventBuffer, EVENT_BUFFER_SIZE);
if (count) processEventsLocked(mEventBuffer, count);
mQueuedListener->flush();   // 把加工后的事件 flush 给 InputClassifier/Dispatcher
```

### 6.1 分派

```cpp
// InputReader.cpp:350  processEventsLocked
// - FIRST_SYNTHETIC_EVENT 之前的是真实输入：按 deviceId 批量聚合，调
//     processEventsForDeviceLocked(deviceId, rawEvent, batchSize)
// - DEVICE_ADDED / DEVICE_REMOVED / FINISHED_DEVICE_SCAN 触发设备增删
```

### 6.2 设备实例化（按 classes 装 InputMapper）

```cpp
// InputReader.cpp:388  addDeviceLocked → :447 createDeviceLocked
InputDevice* device = new InputDevice(...);
if (classes & INPUT_DEVICE_CLASS_SWITCH)        device->addMapper(new SwitchInputMapper(device));
if (classes & INPUT_DEVICE_CLASS_ROTARY_ENCODER) device->addMapper(new RotaryEncoderInputMapper(device));
if (classes & INPUT_DEVICE_CLASS_KEYBOARD)      device->addMapper(new KeyboardInputMapper(device, ...));
if (classes & INPUT_DEVICE_CLASS_CURSOR)        device->addMapper(new CursorInputMapper(device));
if (classes & INPUT_DEVICE_CLASS_TOUCH) {
    if (multitouch) device->addMapper(new MultiTouchInputMapper(device));  // 绝大多数手机屏
    else            device->addMapper(new SingleTouchInputMapper(device));
}
if (classes & INPUT_DEVICE_CLASS_JOYSTICK)      device->addMapper(new JoystickInputMapper(device));
// ...
```

每个设备有一个 `InputDevice`，内含一组 `InputMapper`（一个设备可同时是键盘+触控）。

### 6.3 事件加工（cook）

`InputDevice::process(rawEvents, count)` 把每个 RawEvent 分发给各 `InputMapper`。以触控为例：

- `MultiTouchInputMapper` 累积一次 `EV_SYN/SYN_REPORT` 之前的 `EV_ABS`（x/y/pressure/slot）
  得到一帧多点坐标，做坐标变换/校准/抖动过滤后，生成 `NotifyMotionArgs`；
- `KeyboardInputMapper` 把 `EV_KEY` 的 scancode 经 keymap 翻译成 `AKEYCODE_*`，生成 `NotifyKeyArgs`；
- 最终通过 `mQueuedListener->notifyMotion/notifyKey` 入队，经 `InputClassifier`（前序）转交 `InputDispatcher`，
  与前序专题的 `InputDispatcher::enqueueInboundEventLocked` 衔接。

> 至此完成"内核 evdev → RawEvent → 设备/类型分派 →  cooked NotifyArgs → Dispatcher"全链路。

---

## 七、端到端输入源头到前序

```
[kernel] 触摸屏/按键 → evdev 写 /dev/input/eventN，内核持 wake lock
[EventHub] epoll_wait 返回 device fd
        → read() 取 struct input_event → 转 RawEvent（deviceId/type/code/value）
        → 热插拔：inotify → readNotifyLocked → open/closeDeviceLocked
[InputReader] loopOnce → processEventsLocked
        → 按 deviceId 分派 → InputDevice::process → 各 InputMapper cook
        → mQueuedListener->notifyMotion/Key → InputClassifier
[InputDispatcher] （前序专题）enqueue → dispatch → InputChannel socketpair → App
[App] WindowInputEventReceiver → Choreographer INPUT 阶段
```

关键设计点：
- **EventHub 是唯一的"内核 evdev 客户端"**，把多设备、热插拔、wake lock 全封装起来，InputReader 只看到统一的 RawEvent 流。
- **ioctl EVIOCGBIT 探测 + classes 位掩码** 决定了设备能力分类，是 mapper 装配的依据。
- **wake lock 双持锁** 保证"有事件在飞"时系统不睡，是输入低延迟与省电的平衡。

---

## 八、关键源码索引

- `InputManager.cpp:51` `start()` — 启 InputReader/Dispatcher 双线程
- `EventHub.cpp:253` 构造 — epoll/inotify/wake pipe 初始化
- `EventHub.cpp:815` `getEvents` — 主循环
- `EventHub.cpp:1082` `scanDevicesLocked` / `:1860` `scanDirLocked`
- `EventHub.cpp:1189` `openDeviceLocked` — `open()` + `EVIOCG*`/`EVIOCGBIT` 探测 → classes
- `EventHub.cpp:1810` `readNotifyLocked` — inotify 热插拔
- `EventHub.cpp:1140` `registerDeviceForEpollLocked` — 设备 fd 进 epoll
- `InputReader.cpp:286` `loopOnce` / `:350` `processEventsLocked`
- `InputReader.cpp:388` `addDeviceLocked` / `:447` `createDeviceLocked` — 按 classes 装 InputMapper

The user wants the EventHub/InputReader device reading details source code analysis. Let me locate and read the relevant files.

Key files:
- EventHub.cpp / EventHub.h
- InputReader.cpp / InputReader.h
- InputDevice.cpp, InputMapper subclasses (Keyboard, Touch, etc.)
- InputManager.cpp (start threads)

Let me search for these files and read relevant sections. Let me do parallel searches/reads.
我定位 EventHub / InputReader 相关源码并提取设备读取细节。
I have all the key files. Let me read the key sections:
1. EventHub.cpp constructor (250-300) - epoll/inotify setup
2. EventHub::getEvents - main read loop
3. EventHub::scanDevicesLocked / openDeviceLocked - device enumeration
4. EventHub::readNotifyLocked - hotplug
5. InputReader.cpp loop (around 308 - loopOnce)
6. InputManager.cpp - thread start

Let me read these in parallel
Now I need the EventHub::getEvents main loop (815 onwards), scanDevicesLocked, openDeviceLocked, readNotifyLocked. Let me read getEvents fully and the device open/scan/readNotify.

Let me read getEvents (815 onwards) and openDeviceLocked / scanDevicesLocked / readNotifyLocked.
Now let me read the rest of getEvents (after 944 - the read of input events), scanDevicesLocked, openDeviceLocked, readNotifyLocked. Let me read from 944 to ~1082 (the read loop), then scanDevicesLocked 1082-1189, openDeviceLocked 1189-1420, readNotifyLocked 1810+.

Let me read in parallel: 944-1082, 1082-1189, 1189-1300, 1810-1880.
I have plenty of detail now. Let me also get the InputReader.cpp processEventsLocked and the device/mapper creation (addDeviceLocked) briefly to round out "InputReader device reading details". Let me search processEventsLocked and addDeviceLocked in InputReader.cpp.

Also I should cover how raw events become cooked events via InputMapper (touch/keyboard). Let me grab addDeviceLocked / createDeviceLocked and processEventsLocked locations
I now have a comprehensive picture. Let me write the analysis. I have enough to cover:

1. Thread model: InputManager::start → InputReaderThread + InputDispatcherThread (PRIORITY_URGENT_DISPLAY)
2. EventHub init: epoll + inotify, wake pipe
3. getEvents loop: reopen/scan, device add/remove reporting, read events via epoll_wait, read() input_event
4. Device enumeration: scanDevicesLocked → scanDirLocked → openDeviceLocked (open, ioctl EVIOCGNAME/VERSION/ID/PHYS/UNIQ, EVIOCGBIT to get classes, register to epoll)
5. Hotplug: inotify → readNotifyLocked → open/close device
6. Wake lock dance
7. InputReader loopOnce → getEvents → processEventsLocked → per-device batch → InputDevice::process → InputMapper (keyboard/touch)
8. addDeviceLocked → createDeviceLocked → addMapper based on classes

This connects to the earlier InputDispatcher analysis (mClassifier → mDispatcher). Let me write the markdown.

Let me write the file.
已收集完整源码链路。导出 `EventHub / InputReader` 设备读取细节分析至 `c:\D\android_project\cells-android10\EventHub_InputReader_解析.md`。
已完成 **EventHub / InputReader 设备读取细节** 专题，导出至 `c:\D\android_project\cells-android10\EventHub_InputReader_解析.md`。

## 核心要点

**1. 线程模型**
`InputManager::start`（InputManager.cpp:51）起两个 `PRIORITY_URGENT_DISPLAY` 线程：`InputReader`（本篇）和 `InputDispatcher`（前序）。`EventHub` 由 InputReader 单例持有。

**2. EventHub 初始化（EventHub.cpp:253）**
一个 epoll 实例挂三类 fd：
- `mINotifyFd` — inotify 监听 `/dev/input` 增删（热插拔）
- 各设备 `device->fd` — 真实 evdev 节点
- `mWakeReadPipeFd` — `wake()` 写管道，打断阻塞的 `epoll_wait`

**3. getEvents 主循环（EventHub.cpp:815）**
流程：重开设备检查 → 上报 `DEVICE_REMOVED` → `scanDevicesLocked` 扫描 → 上报 `DEVICE_ADDED` → `epoll_wait` 返回后对每个就绪 fd `read(device->fd, ...)` 把 `struct input_event` 转成 `RawEvent{when,deviceId,type,code,value}` → 处理完再 `readNotifyLocked` 处理 inotify。**wake lock 双持锁**：`epoll_wait` 阻塞期间才释放，保证"事件在飞"时系统不睡。

**4. 设备枚举（openDeviceLocked:1189）**
`open(O_RDWR|O_NONBLOCK)` 节点 → 一连串 `ioctl`：`EVIOCGNAME/VERSION/ID/PHYS/UNIQ` 取属性，`EVIOCGBIT(EV_KEY/EV_ABS/EV_REL/...)` 取事件能力位掩码 → 推得 `device->classes`（KEYBOARD/CURSOR/TOUCH…），这是 InputReader 装配 mapper 的唯一依据。最后 `registerDeviceForEpollLocked`（EPOLLIN|EPOLLWAKEUP）把 fd 加进 epoll。

**5. 热插拔（readNotifyLocked:1810）**
inotify 事件 → `IN_CREATE` 调 `openDeviceLocked`、`IN_DELETE` 调 `closeDeviceByPathLocked`，下次循环作为 `DEVICE_ADDED/REMOVED` 上报。

**6. InputReader 加工（InputReader.cpp）**
`loopOnce:286` → `getEvents` → `processEventsLocked:350`（按 deviceId 批量分派）→ `InputDevice::process` → 各 `InputMapper`：触控 `MultiTouchInputMapper` 累积 `SYN_REPORT` 前一帧 ABS 坐标、键盘 `KeyboardInputMapper` 把 scancode 经 keymap 翻成 `AKEYCODE_*`，生成 cooked 的 `NotifyMotionArgs/NotifyKeyArgs` → `mQueuedListener->flush()` 经 `InputClassifier` 交给前序的 `InputDispatcher`。`addDeviceLocked:388`/`createDeviceLocked:447` 按 classes 给设备安装对应 mapper。

至此完整闭环：`kernel evdev → EventHub(RawEvent) → InputReader(cooked) → InputClassifier → InputDispatcher → socketpair → App Choreographer INPUT`。

至此全链路五篇（`EventHub/InputReader` → `InputDispatcher/Choreographer INPUT` → `RenderThread/BufferQueue` → `WebView/GL Functor` → `SurfaceControl/层次合成`）贯通。需要继续 **Vsync/HWC 时序与相位** 或 **InputClassifier/策略** 可告知。
