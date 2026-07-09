# Input 事件核心源码 树状图分析（Android 10）

把前序 5 篇（`EventHub/InputReader` → `InputDispatcher/Choreographer INPUT` →
`RenderThread/BufferQueue` → `WebView/GL Functor` → `SurfaceControl/层次合成`）中
**与输入相关**的源码，以树状结构重新组织。

---

## 一、整体分层树（内核 → App → 合成）

```
Input 输入链路（按进程 / 层级）
│
├── ① 内核层  (kernel input subsystem)
│   └── drivers/input/evdev.c
│       └── /dev/input/eventN 节点
│           └── 触摸屏/按键 → 写 struct input_event，内核持 wake lock
│
├── ② system_server 进程  (InputManagerService ↔ native InputManager)
│   ├── InputManagerService.java
│   │   └── start() :339 → nativeStart() :341
│   └── native InputManager.cpp
│       └── start() :51
│           ├── InputReaderThread  ("InputReader",  PRIORITY_URGENT_DISPLAY)
│           └── InputDispatcherThread ("InputDispatcher", PRIORITY_URGENT_DISPLAY)
│
├── ③ EventHub  (InputReader 线程内，唯一内核 evdev 客户端)
│   ├── 构造 :253   epoll + inotify + wake pipe
│   │   ├── mEpollFd        :253  epoll_create1
│   │   ├── mINotifyFd      :256  inotify_init (监听 /dev/input)
│   │   └── mWakeReadPipeFd :276  pipe (wake() 打断 epoll_wait)
│   ├── getEvents() :815    主循环
│   │   ├── scanDevicesLocked()   :1082 → scanDirLocked() :1860
│   │   │   └── openDeviceLocked() :1189
│   │   │       ├── open(O_RDWR|O_NONBLOCK)             :1194
│   │   │       ├── ioctl EVIOCGNAME/VERSION/ID/PHYS/UNIQ :1203-1254
│   │   │       ├── ioctl EVIOCGBIT(EV_KEY/ABS/REL/SW)  :1280-1286  → device->classes
│   │   │       └── registerDeviceForEpollLocked() :1140  (fd 入 epoll)
│   │   ├── read(device->fd, input_event*) :946   → 转 RawEvent
│   │   ├── readNotifyLocked() :1810   inotify 热插拔
│   │   └── epoll_wait() :1028   （释放 wake lock 后阻塞）
│   └── wake() :1069  写 wake pipe
│
├── ④ InputReader  (同一线程)
│   ├── loopOnce() :286
│   │   ├── mEventHub->getEvents() :308
│   │   └── processEventsLocked() :350
│   │       ├── DEVICE_ADDED/REMOVED → addDeviceLocked() :388
│   │       │   └── createDeviceLocked() :447  → addMapper(...) 按 classes
│   │       │       ├── KeyboardInputMapper   (scancode→AKEYCODE)
│   │       │       ├── MultiTouchInputMapper (ABS→坐标帧)
│   │       │       ├── CursorInputMapper / SwitchInputMapper / JoystickInputMapper ...
│   │       └── 真实事件 → InputDevice::process → 各 InputMapper cook
│   │           └── mQueuedListener->notifyMotion/notifyKey (cooked NotifyArgs)
│   └── mQueuedListener->flush() :347  → 送 InputClassifier
│
├── ⑤ InputClassifier → InputDispatcher
│   └── InputClassifier  (前序：分类/加速度/预测) → mDispatcher
│   └── InputDispatcher.cpp
│       ├── dispatchOnce() :265
│       │   └── dispatchOnceInnerLocked() :290
│       │       ├── mInboundQueue 取事件
│       │       └── dispatchEventLocked() :1018
│       │           └── 按 mConnectionsByFd 找连接
│       │               └── startDispatchCycleLocked() :2162
│       │                   └── inputPublisher.publishMotionEvent() :2229
│       └── 线程间：mDispatcherThread 循环
│
├── ⑥ Binder / 跨进程通道  (socketpair)
│   └── InputTransport.cpp
│       └── openInputChannelPair() :256
│           └── socketpair(AF_UNIX, SOCK_SEQPACKET)  —— 只传 InputMessage，零拷贝
│
└── ⑦ App 进程  (ViewRootImpl)
    └── ViewRootImpl.java
        ├── WindowInputEventReceiver.onInputEvent() :7857
        ├── enqueueInputEvent() :7878
        ├── scheduleConsumeBatchedInput() :7887
        │   └── mChoreographer.postCallback(CALLBACK_INPUT, ...) :7803
        │       └── doConsumeBatchedInput() :7824 (frameTimeNanos 来自 getFrameTimeNanos)
        └── Choreographer.java  回调顺序
            ├── CALLBACK_INPUT     :719  ← 输入最先处理
            ├── CALLBACK_ANIMATION :722
            ├── CALLBACK_TRAVERSAL :726  → 可能 invalidate 触发重绘
            └── CALLBACK_COMMIT    :728
```

---

## 二、一次触摸事件的"纵向"调用树

```
[触摸] 手指按下
│
├─ kernel: evdev 把 input_event 写入 /dev/input/eventN
│
├─ EventHub::getEvents :815
│   ├─ epoll_wait :1028 返回 device fd
│   ├─ read(device->fd) :946  → RawEvent{type=EV_KEY/BTN_TOUCH, ...}
│   └─ 返回 RawEvent 批次给 InputReader
│
├─ InputReader::loopOnce :286
│   ├─ processEventsLocked :350
│   ├─ InputDevice::process → MultiTouchInputMapper
│   │   └─ 累积 SYN_REPORT 前 ABS 坐标 → NotifyMotionArgs(down)
│   └─ mQueuedListener->flush :347
│
├─ InputClassifier → InputDispatcher::enqueueInboundEventLocked
│   └─ mInboundQueue.push(event)
│
├─ InputDispatcher::dispatchOnce :265
│   ├─ dispatchOnceInnerLocked :290
│   ├─ dispatchEventLocked :1018 (查 mConnectionsByFd)
│   └─ startDispatchCycleLocked :2162
│       └─ publishMotionEvent :2229  (写入 InputChannel)
│
├─ socketpair : InputTransport.cpp:256 (跨进程)
│
├─ App: WindowInputEventReceiver.onInputEvent :7857
│   ├─ enqueueInputEvent :7878
│   └─ scheduleConsumeBatchedInput :7887
│
├─ Choreographer: postCallback(CALLBACK_INPUT) :7803
│   └─ doConsumeBatchedInput :7824  ← 与 Vsync 对齐消费
│
└─ 后续（前序 RenderThread 专题）
    ├─ CALLBACK_TRAVERSAL :726 → ThreadedRenderer.draw → RenderThread
    ├─ dequeueBuffer → GL 绘制 → queueBuffer
    └─ SurfaceFlinger 合成上屏（SurfaceControl 专题）
```

---

## 三、关键类 / 文件归属树

```
frameworks/
├── native/services/inputflinger/
│   ├── InputManager.cpp          start() :51   启双线程
│   ├── EventHub.cpp              内核 evdev 客户端
│   │   ├── 构造 :253 / getEvents :815 / openDeviceLocked :1189
│   │   └── readNotifyLocked :1810 / scanDevicesLocked :1082
│   ├── InputReader.cpp           原始事件 → 设备级事件
│   │   ├── loopOnce :286 / processEventsLocked :350
│   │   └── addDeviceLocked :388 / createDeviceLocked :447
│   ├── InputDispatcher.cpp       派发（前序）
│   │   ├── dispatchOnce :265 / dispatchOnceInnerLocked :290
│   │   └── dispatchEventLocked :1018 / startDispatchCycleLocked :2162
│   └── InputTransport.cpp        openInputChannelPair :256 (socketpair)
│
├── base/services/core/java/com/android/server/input/
│   └── InputManagerService.java  start :339 / nativeStart :341
│
└── base/core/java/android/view/
    ├── ViewRootImpl.java         WindowInputEventReceiver :7857 / enqueueInputEvent :7878
    │                             scheduleConsumeBatchedInput :7887 / doConsumeBatchedInput :7824
    └── Choreographer.java        CALLBACK_INPUT :719 / TRAVERSAL :726 / COMMIT :728
```

---

## 四、横向"职责切分"树（谁做什么）

```
职责切分
├── 发现 & 读取（EventHub）
│   ├── 设备枚举/热插拔：inotify + openDeviceLocked + EVIOCGBIT
│   ├── 事件读取：epoll_wait + read(evdev)
│   └── 同步保证：wake lock 双持锁
├── 语义加工（InputReader + InputMapper）
│   ├── 设备分类：classes 位掩码 → 装哪些 mapper
│   ├── 坐标/按键翻译：ABS→坐标帧、scancode→AKEYCODE
│   └── 输出：cooked NotifyArgs（与设备无关的统一事件）
├── 决策 & 派发（InputDispatcher）
│   ├── 策略：焦点窗口命中、拦截（InputClassifier 辅助）
│   ├── 排队：mInboundQueue
│   └── 投递：publishMotionEvent 写入 InputChannel
├── 跨进程传输（InputChannel / socketpair）
│   └── SOCK_SEQPACKET，只传 InputMessage，低延迟
└── App 对齐 Vsync（ViewRootImpl + Choreographer）
    ├── 同周期 INPUT 阶段消费
    └── 触发 TRAVERSAL 重绘（接 RenderThread）
```

---

## 五、与前序专题的衔接

```
Input 事件（本篇）
   └─> App 绘制触发（Choreographer TRAVERSAL）
         └─> RenderThread/BufferQueue 专题：dequeue→GL→queue
               └─> WebView/GL Functor 专题：WebView 画进共享 buffer
                     └─> SurfaceControl/层次合成 专题：SF latch→合成→HWC 上屏
```


The user wants a tree-structured analysis of Input event core source code. This is a summary/synthesis task drawing on all the previous analysis I've done across the conversation. I should produce a tree diagram (ASCII/markdown tree) that shows the Input event flow through source code, with file:line references.

Let me build a comprehensive tree based on the source I've already explored:

1. Kernel evdev layer (device nodes /dev/input/eventN)
2. EventHub (read/hotplug)
3. InputReader (cook → InputMapper)
4. InputClassifier
5. InputDispatcher (dispatch)
6. InputChannel (socketpair)
7. App side: WindowInputEventReceiver → Choreographer INPUT

I have all the details from previous reads. Let me create a markdown tree diagram file. I don't necessarily need to read more files - I have enough source references from prior turns. But let me make sure the tree is accurate with the file:line references I've collected.

References I have:
- InputManager.cpp:51 start → InputReaderThread / InputDispatcherThread
- EventHub.cpp:253 constructor (epoll/inotify), :815 getEvents, :1082 scanDevicesLocked, :1189 openDeviceLocked, :1810 readNotifyLocked, :1140 registerDeviceForEpollLocked
- InputReader.cpp:286 loopOnce, :350 processEventsLocked, :388 addDeviceLocked, :447 createDeviceLocked, :308 getEvents call
- InputDispatcher.cpp:265 dispatchOnce, :290 dispatchOnceInnerLocked, :1018 dispatchEventLocked, :2162 startDispatchCycleLocked, publishMotionEvent :2229
- InputChannel socketpair — InputTransport.cpp:256 openInputChannelPair
- ViewRootImpl.java:7857 onInputEvent / WindowInputEventReceiver, :7803 postCallback CALLBACK_INPUT, :7824 doConsumeBatchedInput
- Choreographer.java:719/722/726/728 CALLBACK_INPUT/ANIMATION/TRAVERSAL/COMMIT
- InputManagerService.java:339 start → :341 nativeStart → InputManager::start

I should build a tree. The user said "树状图分析" - tree diagram analysis. Let me write a markdown file with a tree structure, organized by layers, with the key classes/methods and line refs, plus a flow tree.

Let me produce this. I'll create a new file Input_事件_树状图分析.md.

I think it's best to give a clear tree. Let me structure:

# 输入事件核心源码 树状图分析

## 一、整体分层树（从内核到 App）
A big tree.

## 二、关键调用链树（一次触摸事件）
Step-by-step tree.

## 三、类/文件归属树
Group by process/file.

Let me write it concisely but complete. I'll output the tree in the response too, but also save to file. The user asked for analysis, I'll save to file and summarize.

Let me write the file
我已掌握前序全部源码细节，直接综合成树状图分析，导出至 `c:\D\android_project\cells-android10\Input_事件_树状图分析.md`。
已完成 **Input 事件核心源码 树状图分析**，导出至 `c:\D\android_project\cells-android10\Input_事件_树状图分析.md`。

## 树状图总览（5 棵树）

**① 整体分层树** — 从内核到 App 再到合成的完整层级：
- 内核 evdev `/dev/input/eventN` → ② `InputManagerService`/`InputManager::start:51` 启双线程 → ③ `EventHub`（epoll/inotify/`getEvents:815`/`openDeviceLocked:1189`/`readNotifyLocked:1810`）→ ④ `InputReader`（`loopOnce:286`/`processEventsLocked:350`/`createDeviceLocked:447` 按 classes 装 mapper）→ ⑤ `InputClassifier→InputDispatcher`（`dispatchOnce:265`/`publishMotionEvent:2229`）→ ⑥ socketpair（`InputTransport.cpp:256`）→ ⑦ `ViewRootImpl`（`onInputEvent:7857`/`CALLBACK_INPUT:7803`）。

**② 一次触摸事件纵向调用树** — 从手指按下到 `CALLBACK_TRAVERSAL` 触发重绘的逐层调用路径。

**③ 类/文件归属树** — `inputflinger/`（EventHub/InputReader/InputDispatcher/InputTransport）+ `InputManagerService.java` + `ViewRootImpl.java`/`Choreographer.java` 的 `文件:行号` 索引。

**④ 职责切分树** — 谁做什么：EventHub（发现/读取/同步）、InputReader（语义加工）、InputDispatcher（决策/派发）、InputChannel（跨进程传输）、Choreographer（对齐 Vsync）。

**⑤ 衔接树** — 接前序 4 篇：`Input → RenderThread/BufferQueue → WebView/GL Functor → SurfaceControl/层次合成`。

## 关键源码锚点（树中高频引用）
| 职责 | 位置 |
|------|------|
| 启双线程 | `InputManager.cpp:51` |
| epoll/inotify 初始化 | `EventHub.cpp:253` |
| 读事件主循环 | `EventHub.cpp:815`（`read:946` / `epoll_wait:1028`） |
| 设备枚举/探测 | `openDeviceLocked:1189`（`EVIOCGBIT:1280`） |
| 热插拔 | `readNotifyLocked:1810` |
| 事件加工 | `InputReader.cpp:286/350/447` |
| 派发 | `InputDispatcher.cpp:265/1018/2162` |
| 跨进程通道 | `InputTransport.cpp:256` socketpair |
| App 对齐 | `ViewRootImpl.java:7857/7803/7824`、`Choreographer.java:719` |

至此全链路（init→Zygote→SystemServer→InputManagerService→EventHub/InputReader→InputDispatcher→InputChannel→Choreographer(INPUT)→RenderThread→BufferQueue→SurfaceFlinger→HWC2）在源码层以树状结构贯通。

至此 **init → Zygote → SystemServer → InputManagerService → EventHub/InputReader →
InputDispatcher → InputChannel → Choreographer(INPUT) → RenderThread → BufferQueue →
SurfaceFlinger → HWC2** 全链路在源码层（树状）贯通。
