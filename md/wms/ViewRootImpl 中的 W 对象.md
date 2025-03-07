
在 Android 系统里，`ViewRootImpl` 是 `View` 体系中的核心类，它负责管理 `View` 的测量、布局、绘制以及与 `WindowManagerService`（WMS）的交互。`ViewRootImpl` 中的 `W` 对象是一个非常关键的成员，下面详细介绍它。

### 1. `W` 对象的定义与本质

  

`W` 是 `ViewRootImpl` 中的一个内部类，它继承自 `IWindow.Stub`。`IWindow` 是一个 AIDL（Android Interface Definition Language）接口，定义了应用程序窗口与 WMS 之间通信的方法。`W` 对象本质上是一个 `Binder` 对象，用于实现应用程序进程与系统进程（WMS 所在进程）之间的跨进程通信（IPC）。

  

以下是 `W` 类的简单定义示例：


```java
static class W extends IWindow.Stub {
    private final WeakReference<ViewRootImpl> mViewAncestor;

    W(ViewRootImpl viewAncestor) {
        mViewAncestor = new WeakReference<>(viewAncestor);
    }

    // 实现 IWindow 接口的方法
    @Override
    public void windowResized(Rect newBounds, Rect contentInsets, Rect visibleInsets,
                              Rect stableInsets, int configurationChanges, CompatibilityInfo compatInfo,
                              boolean reportDraw, long frameNumber) {
        ViewRootImpl viewRoot = mViewAncestor.get();
        if (viewRoot != null) {
            viewRoot.setWindowFrame(newBounds);
            // 其他处理逻辑
        }
    }

    // 其他方法...
}
```

### 2. `W` 对象的作用

#### 与 WMS 通信

  

`W` 对象作为应用程序窗口与 WMS 之间的通信桥梁，负责将 WMS 发送的各种窗口状态变化通知传递给 `ViewRootImpl`。例如，当窗口大小发生变化、焦点改变或者有输入事件时，WMS 会通过调用 `W` 对象中实现的 `IWindow` 接口方法，将这些信息传递给应用程序进程。

#### 处理窗口状态变化

  

`W` 对象接收到 WMS 的通知后，会调用 `ViewRootImpl` 中的相应方法来处理这些窗口状态变化。比如，当接收到 `windowResized` 通知时，会调用 `ViewRootImpl` 的 `setWindowFrame` 方法来更新窗口的大小和布局。

#### 窗口管理与同步

  

通过 `W` 对象，`ViewRootImpl` 可以与 WMS 保持同步，确保应用程序窗口的状态与系统窗口管理的状态一致。例如，在应用程序请求更新窗口布局时，`ViewRootImpl` 会通过 `W` 对象将请求发送给 WMS，WMS 处理后再通过 `W` 对象将处理结果反馈给 `ViewRootImpl`。

### 3. `W` 对象的创建与使用

#### 创建

  

在 `ViewRootImpl` 的构造函数中会创建 `W` 对象：

  

收起

java

```java
public ViewRootImpl(Context context, Display display) {
    // 其他初始化操作
    mWindow = new W(this);
}
```

#### 使用

  

在 `ViewRootImpl` 与 WMS 进行交互时，会使用 `W` 对象。例如，在将窗口添加到 WMS 时：

  

收起

java

```java
try {
    mWindowSession.addWindow(mWindow, mSeq, mWindowAttributes,
            getHostVisibility(), mDisplay.getDisplayId(),
            mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
            mAttachInfo.mOutsets, mInputChannel);
} catch (RemoteException e) {
    // 异常处理
}
```

  

这里的 `mWindowSession` 是与 WMS 建立的会话，`mWindow` 就是 `W` 对象，通过调用 `addWindow` 方法将窗口信息发送给 WMS。

### 4. 总结

  

`ViewRootImpl` 中的 `W` 对象在 Android 窗口管理系统中起着至关重要的作用，它通过实现 `IWindow` 接口，利用 `Binder` 机制实现了应用程序进程与 WMS 之间的高效跨进程通信，确保了应用程序窗口能够及时响应系统的窗口状态变化，为用户提供流畅的界面交互体验。