
在 Android 系统中，“Session” 有多种不同的含义和应用场景，下面为你详细介绍常见的几种情况：

### 1. WindowManagerSession

#### 概述

  

`WindowManagerSession` 是 `WindowManagerService`（WMS）与应用进程之间进行通信的桥梁，它在窗口管理系统中扮演着重要角色。每个应用进程与 WMS 建立连接时，都会创建一个对应的 `WindowManagerSession` 实例。

#### 作用

  

- **跨进程通信**：应用进程通过 `WindowManagerSession` 向 WMS 发送窗口相关的请求，如创建窗口、调整窗口大小和位置、设置窗口属性等。WMS 则通过 `WindowManagerSession` 向应用进程返回处理结果和通知。
- **窗口管理**：`WindowManagerSession` 负责管理应用进程中的所有窗口，记录窗口的状态和属性。它会将应用进程的窗口请求传递给 WMS 进行处理，并确保窗口的显示和布局符合系统的要求。

#### 工作流程

  

- **建立连接**：当应用进程需要与 WMS 进行交互时，会通过 `Binder` 机制建立与 WMS 的连接，并创建一个 `WindowManagerSession` 实例。
- **发送请求**：应用进程通过 `WindowManagerSession` 的方法向 WMS 发送窗口相关的请求，如 `addWindow()`、`updateWindow()` 等。
- **处理请求**：WMS 接收到请求后，会对请求进行处理，并将处理结果通过 `WindowManagerSession` 返回给应用进程。

### 2. InputSession

#### 概述

  

`InputSession` 是 Android 输入系统中的一个重要概念，用于管理输入事件的传递和处理。它在输入设备（如触摸屏、键盘等）和应用窗口之间建立了一个会话通道，确保输入事件能够准确地传递到目标窗口。

#### 作用

  

- **输入事件传递**：`InputSession` 负责将输入设备产生的事件（如触摸事件、按键事件等）从输入系统传递到目标应用窗口。它会根据窗口的焦点和布局信息，确定事件的接收者，并将事件发送给相应的窗口。
- **事件处理协调**：在事件传递过程中，`InputSession` 会协调输入系统和应用窗口之间的交互，确保事件的处理顺序和方式符合系统的要求。例如，它会处理事件的拦截、分发和消费等操作。

#### 工作流程

  

- **创建会话**：当应用窗口获得焦点时，输入系统会为该窗口创建一个 `InputSession` 实例，并将其与窗口关联起来。
- **事件传递**：输入设备产生的事件会首先传递到输入系统，输入系统会根据 `InputSession` 的信息将事件发送到目标窗口。
- **事件处理**：目标窗口接收到事件后，会对事件进行处理，并将处理结果反馈给输入系统。输入系统会根据处理结果决定是否继续传递事件或进行其他操作。

### 3. HttpSession（在 Android 网络开发中）

#### 概述

  

在 Android 网络开发中，`HttpSession` 通常指的是 HTTP 会话，用于在客户端和服务器之间保持状态信息。它允许服务器在多个 HTTP 请求之间跟踪客户端的状态，从而实现一些需要状态管理的功能，如用户登录、购物车等。

#### 作用

  

- **状态管理**：`HttpSession` 可以在服务器端存储客户端的状态信息，如用户的登录状态、会话 ID 等。在后续的请求中，客户端可以通过会话 ID 来标识自己的身份，服务器可以根据会话 ID 获取客户端的状态信息。
- **数据共享**：`HttpSession` 还可以在同一个会话中共享数据，使得不同的请求可以访问和修改相同的数据。例如，在购物车应用中，用户添加商品到购物车的操作会在 `HttpSession` 中记录，后续的结算操作可以直接从 `HttpSession` 中获取购物车的信息。

#### 工作流程

  

- **创建会话**：当客户端第一次访问服务器时，服务器会为该客户端创建一个 `HttpSession` 实例，并生成一个唯一的会话 ID。服务器会将会话 ID 通过响应头（如 `Set-Cookie`）发送给客户端。
- **传递会话 ID**：客户端在后续的请求中会将会话 ID 通过请求头（如 `Cookie`）发送给服务器，服务器根据会话 ID 来识别客户端的会话。
- **会话管理**：服务器会对 `HttpSession` 进行管理，包括会话的创建、销毁、过期处理等操作。当会话过期或客户端主动退出时，服务器会销毁相应的 `HttpSession` 实例。

  

综上所述，“Session” 在 Android 系统中有多种不同的含义和应用场景，分别在窗口管理、输入系统和网络开发等方面发挥着重要作用。

---

`com.android.server.wm.Session` 类是 Android 系统中 `WindowManagerService`（WMS）的重要组成部分，主要用于管理应用进程与 `WindowManagerService` 之间的通信会话。以下从类的定义、作用、关键方法、工作流程等方面详细介绍。

### 类的定义与继承关系

  

`Session` 类位于 `com.android.server.wm` 包中，它继承自 `IWindowSession.Stub`。`IWindowSession` 是一个 AIDL（Android Interface Definition Language）接口，定义了应用进程与 `WindowManagerService` 之间通信的方法，而 `Session` 类实现了这些方法，从而可以处理应用进程发送的窗口管理相关请求。

  

收起

java

```
public class Session extends IWindowSession.Stub {
    // 类的具体实现代码
}
```

### 作用

  

`Session` 类的核心作用是作为应用进程和 `WindowManagerService` 之间的通信桥梁，具体体现在以下几个方面：

  

- **跨进程通信**：应用进程通过 `Session` 类的方法与 `WindowManagerService` 进行跨进程通信，发送诸如创建窗口、更新窗口属性、删除窗口等请求。
- **窗口管理**：`Session` 负责管理应用进程中所有窗口的相关信息，协助 `WindowManagerService` 对窗口进行布局、绘制和显示等操作。
- **权限检查**：在处理应用进程的请求时，`Session` 会进行权限检查，确保应用进程有足够的权限执行相应的操作。

### 关键方法

#### 1. `addWindow()`

  

用于向 `WindowManagerService` 请求创建一个新的窗口。

  

收起

java

```
@Override
public int addWindow(IWindow client, int seq, WindowManager.LayoutParams attrs,
        int viewVisibility, int displayId, Rect outContentInsets, Rect outStableInsets,
        InputChannel outInputChannel) {
    // 权限检查
    // 处理窗口添加请求
    // 调用 WindowManagerService 的相关方法进行窗口创建
    return WindowManagerGlobal.ADD_OKAY;
}
```

  

在该方法中，会进行权限检查，确保应用有创建窗口的权限，然后将请求转发给 `WindowManagerService` 进行实际的窗口创建操作。

#### 2. `removeWindow()`

  

用于请求删除一个已经存在的窗口。

  

收起

java

```
@Override
public void removeWindow(IWindow client) {
    // 查找要删除的窗口
    // 调用 WindowManagerService 的相关方法进行窗口删除
}
```

  

此方法会查找对应的窗口并通知 `WindowManagerService` 进行删除操作。

#### 3. `relayoutWindow()`

  

用于更新窗口的布局参数，如大小、位置等。

  

收起

java

```
@Override
public int relayoutWindow(IWindow client, int seq, WindowManager.LayoutParams attrs,
        int requestedWidth, int requestedHeight, int viewVisibility, int flags,
        long frameNumber, Rect outFrame, Rect outOverscanInsets, Rect outContentInsets,
        Rect outVisibleInsets, Rect outStableInsets, Rect outBackdropFrame,
        DisplayCutout.ParcelableWrapper outDisplayCutout, MergedConfiguration outMergedConfiguration,
        InputChannel outInputChannel) {
    // 处理窗口布局更新请求
    // 调用 WindowManagerService 的相关方法进行布局更新
    return WindowManagerGlobal.RELAYOUT_OK;
}
```

  

它会处理窗口布局的更新请求，并将新的布局参数传递给 `WindowManagerService`。

### 工作流程

  

1. **建立会话**：当应用进程需要与 `WindowManagerService` 进行窗口管理操作时，会通过 `Binder` 机制建立与 `WindowManagerService` 的连接，`WindowManagerService` 会为该应用进程创建一个 `Session` 实例。
2. **发送请求**：应用进程通过 `Session` 类的方法向 `WindowManagerService` 发送窗口管理请求，如 `addWindow()`、`removeWindow()` 等。
3. **处理请求**：`Session` 类接收到请求后，会进行权限检查和一些预处理操作，然后将请求转发给 `WindowManagerService` 进行实际的处理。
4. **返回结果**：`WindowManagerService` 处理完请求后，会将处理结果返回给 `Session` 类，`Session` 类再将结果返回给应用进程。

### 总结

  

`com.android.server.wm.Session` 类在 Android 窗口管理系统中起着至关重要的作用，它使得应用进程能够与 `WindowManagerService` 进行有效的通信，实现窗口的创建、管理和销毁等操作，保证了 Android 系统中窗口显示的正常运行。