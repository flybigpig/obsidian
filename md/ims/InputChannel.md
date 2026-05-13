WindowManagerService.addWindow() 注册时，会创建一个 pair 的 InputChannel
## `WindowManagerService.addWindow()` 与 InputChannel pair 创建详解

你问的这句话涉及 Android 窗口系统最核心的机制之一。下面我带你完整走一遍从 `addWindow` 到 `InputChannel` 创建的源码链路。

---

## 1. 完整调用链

```
ActivityThread.handleResumeActivity()
    → WindowManagerImpl.addView()
        → WindowManagerGlobal.addView()
            → ViewRootImpl.setView()
                → WindowSession.addToDisplay()  [IPC]
                    → WindowManagerService.addWindow()
                        → InputChannel 被创建出来
                        → Session.windowAddedLocked()
                            → mService.mInputManager.registerInputChannel()
```

---

## 2. 关键源码分解

### 阶段一：`ViewRootImpl.setView()` 中创建 `InputChannel` 对象

```java
// frameworks/base/core/java/android/view/ViewRootImpl.java
public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
    synchronized (this) {
        if (mView == null) {
            mView = view;
            // ...

            // 创建一个空的 InputChannel，将由 WMS 填充
            InputChannel inputChannel = new InputChannel();

            // 通过 Binder IPC 调用 WMS
            res = mWindowSession.addToDisplay(
                    mWindow, mSeq, mWindowAttributes,
                    getHostVisibility(), mDisplay.getDisplayId(),
                    mTmpFrame, mAttachInfo.mContentInsets,
                    mAttachInfo.mStableInsets, mAttachInfo.mOutsets,
                    mAttachInfo.mDisplayCutout, inputChannel,
                    mTempInsets, mTempControls);

            // WMS 返回后，inputChannel 已经被填充为 socket 的一端
            mInputEventReceiver = new WindowInputEventReceiver(inputChannel,
                    Looper.myLooper());
        }
    }
}
```

**关键点**：
- `InputChannel` 是一个 **out 参数**，客户端先创建空壳传给 WMS，WMS 填充其 native 句柄（socket fd）。
- `WindowInputEventReceiver` 内部会创建一个 `InputConsumer`，通过 `InputChannel.nativeOpenInputChannel()` 打开 socket 连接，注册到当前线程的 `Looper` 的 `epoll` 中。

### 阶段二：`Session.addToDisplay()` → `WMS.addWindow()`

```java
// frameworks/base/services/core/java/com/android/server/wm/Session.java
@Override
public int addToDisplay(IWindow window, int seq, WindowManager.LayoutParams attrs,
        int viewVisibility, int displayId, Rect outFrame, Rect outContentInsets,
        Rect outStableInsets, Rect outOutsets, DisplayCutout.ParcelableWrapper outDisplayCutout,
        InputChannel outInputChannel, InsetsState outInsetsState,
        InsetsSourceControl[] outActiveControls) {
    return mService.addWindow(this, window, seq, attrs, viewVisibility, displayId,
            outFrame, outContentInsets, outStableInsets, outOutsets, outDisplayCutout,
            outInputChannel, outInsetsState, outActiveControls);
}
```

### 阶段三：`WMS.addWindow()` 核心逻辑

```java
// frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java
public int addWindow(Session session, IWindow client, int seq,
        LayoutParams attrs, int viewVisibility, int displayId, Rect outFrame,
        Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
        DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel,
        InsetsState outInsetsState, InsetsSourceControl[] outActiveControls) {

    // ... 权限检查、token 验证 ...

    synchronized (mGlobalLock) {
        // ... 很多校验逻辑 ...

        WindowState win = new WindowState(this, session, client, token, parentWindow,
                appOp, seq, attrs, viewVisibility, session.mUid,
                session.mCanAddInternalSystemWindow);

        // ⭐ 关键：创建 InputChannel pair
        // mInputChannel 保存在 WindowState 中（服务端端）
        // outInputChannel 返回给客户端（App 端）
        if (outInputChannel != null && (attrs.inputFeatures
                & WindowManager.LayoutParams.INPUT_FEATURE_NO_INPUT_CHANNEL) == 0) {
            // 📌 调用 native 方法创建 socket pair
            String name = win.makeInputChannelName();
            InputChannel[] inputChannels = InputChannel.openInputChannelPair(name);
            win.setInputChannel(inputChannels[0]);  // 服务端保留
            inputChannels[1].transferTo(outInputChannel);  // 拷贝到客户端

            // 📌 注册到 InputManagerService
            mInputManager.registerInputChannel(win.mInputChannel);
        }

        // ... 添加到窗口列表 ...
    }
}
```

---

## 3. `InputChannel.openInputChannelPair()` — Native 层实现

这是真正的 socket pair 创建，在 native 层实现：

```cpp
// frameworks/base/core/jni/android_view_InputChannel.cpp
static jobjectArray android_view_InputChannel_openInputChannelPair(JNIEnv* env,
        jclass clazz, jstring nameObj) {
    std::string name = GetStringFromJni(env, nameObj);

    // 📌 创建一对 Unix Domain Socket (AF_UNIX, SOCK_SEQPACKET)
    int sockets[2];
    socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets);  // 0 表示成功

    // 设置非阻塞模式
    int bufSize = 1024 * 1024;  // 1MB 缓冲区
    setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize));
    setsockopt(sockets[0], SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));
    setsockopt(sockets[1], SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize));
    setsockopt(sockets[1], SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));

    // ... 创建两个 Java 层 InputChannel 对象，分别包裹两个 fd ...
    jobjectArray channelPair = env->NewObjectArray(2, gInputChannelClassInfo.clazz, NULL);
    // channelPair[0] → 服务端 (WMS 保留)
    // channelPair[1] → 客户端 (传给 App)
    return channelPair;
}
```

**socket 选项说明**：

| Socket 选项 | 值 | 作用 |
|------------|-----|------|
| `AF_UNIX, SOCK_SEQPACKET` | — | 顺序数据报，保证消息边界，可靠传输 |
| `SO_SNDBUF` | 1MB | 增大发送缓冲区，避免高频输入事件丢包 |
| `SO_RCVBUF` | 1MB | 增大接收缓冲区 |

---

## 4. 最终结构图

```
进程：system_server (WMS)               进程：App (Your Activity)
┌─────────────────────────────┐         ┌──────────────────────────────┐
│  InputManagerService        │         │  ViewRootImpl               │
│    │                        │         │    │                         │
│    ▼                        │         │    ▼                         │
│  InputDispatcher            │         │  WindowInputEventReceiver    │
│    │                        │         │    │                         │
│    ▼                        │         │    ▼                         │
│  InputChannel (server fd)   │         │  InputChannel (client fd)   │
│    ┌───────────────────┐    │         │   ┌───────────────────┐     │
│    │ socketpair[0]     │◄───┼─────────┼──►│ socketpair[1]     │     │
│    │ (UNIX Domain Sock)│    │         │   │ (UNIX Domain Sock)│     │
│    └───────────────────┘    │         │   └───────────────────┘     │
│                             │         │                              │
│ epoll 监听 server fd        │         │ epoll 监听 client fd         │
│ (等待 App 端的 consume 确认)  │         │ (等待 WMS 分发的事件)        │
└─────────────────────────────┘         └──────────────────────────────┘
```

---

## 5. 数据流方向

### 普通事件流（触摸→App）

```
InputReader (/dev/input/event0)
    ↓
InputDispatcher
    ↓  查到焦点 Window → server fd
    ↓
socketpair[0] 写入 MotionEvent (序列化后的 InputMessage)
    ↓
socketpair[1] 被 epoll 唤醒
    ↓
WindowInputEventReceiver → finishInputEvent() → 发送 finished 信号
    ↓  (socketpair[0] 收到确认，继续分发下一个事件)
```

### 事件确认回传（App→WMS）

```
finishInputEvent()
    ↓  写入 FINISHED 标志
client fd 写入 → server fd 收到
    ↓
InputDispatcher 从队列中移除该事件
    ↓   继续分发下一个
```

---

## 6. 为什么不用普通 Binder 而用 Socket？

| 方案 | 问题 |
|------|------|
| **Binder 每次调用** | 高频率触摸事件（每帧 60~120 次）会导致大量 Binder 事务，性能差 |
| **共享内存** | 需要额外的同步机制（`futex`），复杂度高 |
| **Unix Socket + epoll** | 一次注册，持续使用，无额外 IPC 开销，支持批量读取，异步非阻塞 |

---

如果你需要查看你的项目中是否涉及与 `InputChannel` 或 `ViewRootImpl` 交互的地方（例如自定义 `Dialog` 不响应触摸、`SurfaceView` 事件冲突等），我可以帮你找具体代码。