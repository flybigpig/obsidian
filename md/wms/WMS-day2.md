好的，你想了解 **WindowManagerService (WMS)**！我看到你已经打开了 `WindowManagerService.java` 文件。让我先读取这个文件来了解 WMS 的核心实现。
让我继续读取 WMS 类的定义和核心方法：

让我继续搜索 WMS 的核心方法，如 `addWindow`、`performLayout` 等：

现在让我搜索 WMS 的核心布局流程 `performLayoutNoTrace` 和 `performSurfacePlacement`：
现在我已经收集了足够的信息。让我为你详细解析 **WindowManagerService (WMS)** 的核心架构和代码实现：

---

## WindowManagerService (WMS) 核心解析

### 一、WMS 架构概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         SystemServer 进程                                 │
│  ┌───────────────────────────────────────────────────────────────────┐ │
│  │                    WindowManagerService                             │ │
│  │  ┌─────────────┐  ┌──────────────┐  ┌────────────────────────┐  │ │
│  │  │ mWindowMap  │  │ mRoot        │  │ mSessions              │  │ │
│  │  │ (IBinder -> │  │ (RootWindow  │  │ (Session -> Process)   │  │ │
│  │  │  WindowState)│  │  Container)  │  │                        │  │ │
│  │  └─────────────┘  └──────────────┘  └────────────────────────┘  │ │
│  │  ┌─────────────┐  ┌──────────────┐  ┌────────────────────────┐  │ │
│  │  │ mAnimator   │  │ mPolicy      │  │ mInputManager          │  │ │
│  │  │ (动画调度)    │  │ (PhoneWindow │  │ (InputDispatcher关联)    │  │ │
│  │  └─────────────┘  │  Manager)    │  └────────────────────────┘  │ │
│  │                     └──────────────┘                               │ │
│  └───────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        ▼                     ▼                     ▼
┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│ Client App   │      │ Client App   │      │ InputManager │
│ (ViewRootImpl│      │ (ViewRootImpl│      │ Service      │
│  -> Session) │      │  -> Session) │      │              │
└──────────────┘      └──────────────┘      └──────────────┘
```

### 二、核心数据结构

```java
// WindowManagerService.java:295
public class WindowManagerService extends IWindowManager.Stub
        implements Watchdog.Monitor, 
                   WindowManagerPolicy.WindowManagerFuncs {
    
    // ★ 核心容器：所有窗口的映射表 (IBinder -> WindowState)
    final HashMap<IBinder, WindowState> mWindowMap = new HashMap<>();
    
    // ★ 窗口层级根节点 (RootWindowContainer)
    RootWindowContainer mRoot;
    
    // ★ 客户端会话映射 (每个 App 进程一个 Session)
    final ArraySet<Session> mSessions = new ArraySet<>();
    
    // ★ 窗口策略 (PhoneWindowManager)
    final WindowManagerPolicy mPolicy;
    
    // ★ 动画调度器
    final WindowAnimator mAnimator;
    
    // ★ 输入管理器关联
    final InputManagerService mInputManager;
}
```

### 三、窗口层级结构 (Window Hierarchy)

```
RootWindowContainer (mRoot)
    │
    ├── DisplayContent (每个物理显示屏)
    │   │
    │   ├── WindowToken (窗口令牌，标识窗口组)
    │   │   │
    │   │   └── AppWindowToken (应用窗口令牌)
    │   │       │
    │   │       └── WindowState (单个窗口)
    │   │           │
    │   │           └── WindowState (子窗口, 如 Dialog)
    │   │
    │   ├── WindowToken (系统窗口，如状态栏、导航栏)
    │   │   └── WindowState (StatusBar)
    │   │
    │   └── ...
    │
    └── ...
```

### 四、核心流程 1：添加窗口 `addWindow()` — `WindowManagerService.java:1276`

```java
public int addWindow(Session session, IWindow client, int seq,
        LayoutParams attrs, int viewVisibility, int displayId,
        Rect outFrame, Rect outContentInsets, Rect outStableInsets,
        Rect outOutsets, DisplayCutout.ParcelableWrapper outDisplayCutout,
        InputChannel outInputChannel, InsetsState outInsetsState) {
    
    // ═══════════════════════════════════════════════════════
    // 第一步：权限检查
    // ═══════════════════════════════════════════════════════
    int res = mPolicy.checkAddPermission(attrs, appOp);
    if (res != WindowManagerGlobal.ADD_OKAY) return res;

    synchronized (mGlobalLock) {
        // ═══════════════════════════════════════════════════════
        // 第二步：获取 DisplayContent
        // ═══════════════════════════════════════════════════════
        final DisplayContent displayContent = 
            getDisplayContentOrCreate(displayId, attrs.token);

        // ═══════════════════════════════════════════════════════
        // 第三步：检查窗口令牌 (WindowToken)
        // ═══════════════════════════════════════════════════════
        WindowToken token = displayContent.getWindowToken(
            hasParent ? parentWindow.mAttrs.token : attrs.token);

        // ═══════════════════════════════════════════════════════
        // 第四步：创建 WindowState (核心窗口对象)
        // ═══════════════════════════════════════════════════════
        final WindowState win = new WindowState(this, session, client,
                token, parentWindow, appOp[0], seq, attrs, viewVisibility,
                session.mUid, session.mCanAddInternalSystemWindow);

        // ═══════════════════════════════════════════════════════
        // 第五步：调整窗口参数 (根据策略)
        // ═══════════════════════════════════════════════════════
        mPolicy.adjustWindowParamsLw(win, win.mAttrs, 
                                     BITMAP_SYSTEM_ALERT_WINDOW);

        // ═══════════════════════════════════════════════════════
        // 第六步：创建 Surface (与 SurfaceFlinger 交互)
        // ═══════════════════════════════════════════════════════
        win.attach();  // 创建 SurfaceSession 和 SurfaceControl

        // ═══════════════════════════════════════════════════════
        // 第七步：创建输入通道 (InputChannel)
        // ═══════════════════════════════════════════════════════
        if (outInputChannel != null && (attrs.inputFeatures &
                INPUT_FEATURE_NO_INPUT_CHANNEL) == 0) {
            String name = win.makeInputChannelName();
            InputChannel[] inputChannels = InputChannel.openInputChannelPair(name);
            win.setInputChannel(inputChannels[0]);
            inputChannels[1].transferTo(outInputChannel);
            
            // ★ 注册到 InputDispatcher
            mInputManager.registerInputChannel(inputChannels[1], win.mInputWindowHandle);
        }

        // ═══════════════════════════════════════════════════════
        // 第八步：将窗口加入层级结构
        // ═══════════════════════════════════════════════════════
        win.getParent().addChild(win, mWindowComparator);
        
        // 加入全局映射表
        mWindowMap.put(client.asBinder(), win);

        return ADD_OKAY;
    }
}
```

### 五、核心流程 2：窗口布局 `performLayoutNoTrace()` — `DisplayContent.java:3841`

```java
private void performLayoutNoTrace(boolean initial, boolean updateInputWindows) {
    if (!isLayoutNeeded()) return;
    clearLayoutNeeded();

    final int dw = mDisplayInfo.logicalWidth;
    final int dh = mDisplayInfo.logicalHeight;

    // ═══════════════════════════════════════════════════════
    // 第一步：更新 DisplayFrames (屏幕装饰区域)
    // ═══════════════════════════════════════════════════════
    mDisplayFrames.onDisplayInfoUpdated(mDisplayInfo, 
        calculateDisplayCutoutForRotation(mDisplayInfo.rotation));
    mDisplayFrames.mRotation = mRotation;
    
    // ★ 计算状态栏、导航栏等系统窗口的布局
    mDisplayPolicy.beginLayoutLw(mDisplayFrames, getConfiguration().uiMode);

    // ═══════════════════════════════════════════════════════
    // 第二步：布局根窗口 (非子窗口)
    // ═══════════════════════════════════════════════════════
    forAllWindows(mPerformLayout, true /* traverseTopToBottom */);
    // 对每个 WindowState 调用:
    //   win.computeFrameLw(mDisplayFrames);  // 计算窗口框架
    //   win.pendingSeamlessRotate = ...;     // 处理无缝旋转

    // ═══════════════════════════════════════════════════════
    // 第三步：布局子窗口 (如 Dialog 依附于 Activity)
    // ═══════════════════════════════════════════════════════
    forAllWindows(mPerformLayoutAttached, true /* traverseTopToBottom */);

    // ═══════════════════════════════════════════════════════
    // 第四步：更新 InputWindows (告诉 InputDispatcher)
    // ═══════════════════════════════════════════════════════
    mInputMonitor.layoutInputConsumers(dw, dh);
    mInputMonitor.setUpdateInputWindowsNeededLw();
    if (updateInputWindows) {
        mInputMonitor.updateInputWindowsLw(false);
    }
}
```

### 六、核心流程 3：Surface 放置 `performSurfacePlacementLoop()` — `WindowSurfacePlacer.java:112`

```java
private void performSurfacePlacementLoop() {
    Trace.traceBegin(Trace.TRACE_TAG_WINDOW_MANAGER, "wmLayout");
    mInLayout = true;

    try {
        // ★ 核心：遍历所有 DisplayContent 进行 Surface 操作
        mService.mRoot.performSurfacePlacement(recoveringMemory);
        mInLayout = false;

        // ═══════════════════════════════════════════════════════
        // 处理待销毁的 Surface
        // ═══════════════════════════════════════════════════════
        mService.scheduleRemoveStartingWindow();  // 移除启动窗口
        
        // ═══════════════════════════════════════════════════════
        // 执行 SurfaceFlinger 事务
        // ═══════════════════════════════════════════════════════
        mService.mRoot.executeAppTransaction();
        
    } catch (RuntimeException e) {
        Slog.wtf(TAG, "Unhandled exception in surface placement!", e);
    }
}
```

### 七、窗口类型与 Z-Order 层级

```java
// WindowManagerService.java:306
static final int TYPE_LAYER_MULTIPLIER = 10000;  // 每层基础间隔
static final int TYPE_LAYER_OFFSET = 1000;       // 同类型内偏移
static final int WINDOW_LAYER_MULTIPLIER = 5;   // 窗口间间隔

// 窗口类型层级 (从底到顶):
//   1. 应用窗口 (APPLICATION_WINDOW)         → base 1-99
//   2. 子窗口 (SUB_WINDOW)                   → base 1000+
//   3. 系统窗口 (SYSTEM_WINDOW):
//      - STATUS_BAR                          → 很高层级
//      - NAVIGATION_BAR                      → 很高层级
//      - TYPE_APPLICATION_OVERLAY            → 较高层级
//      - TYPE_INPUT_METHOD                   → 输入法层级
//      - TYPE_WALLPAPER                      → 最底层
```

### 八、与 InputDispatcher 的交互

```
WMS                                    InputDispatcher
 │                                            │
 │  addWindow()                                │
 │    ├── 创建 InputChannel ──────────────────►│
 │    │     registerInputChannel()             │
 │    │                                        │
 │  updateInputWindowsLw()                     │
 │    ├── 计算窗口 Z-Order 和触摸区域           │
 │    └── setInputWindows() ──────────────────►│
 │         (传递 InputWindowHandle[])           │
 │                                            │
 │  removeWindow()                             │
 │    └── unregisterInputChannel() ──────────►│
 │                                            │
 │  <─────────── 触摸事件分发 ──────────────── │
 │         (通过 InputChannel 返回给应用)        │
```

### 九、启动流程 (SystemServer)

```java
// SystemServer.java (伪代码)
private void startOtherServices() {
    // 1. 创建 WMS
    wm = WindowManagerService.main(context, inputManager,
            !hasFeatureLeanback, !hasFeatureWatch, new PhoneLayoutInflater());
    
    // 2. 初始化显示
    wm.onInitReady();        // 初始化 PhoneWindowManager
    wm.displayReady();       // 显示就绪
    wm.systemReady();        // 系统服务就绪
    
    // 3. 开启动画
    wm.computeNewConfiguration();
}
```

### 十、核心线程模型

```
┌─────────────────┐
│  SystemServer   │
│  (主线程)        │
└────────┬────────┘
         │
    ┌────▼────┐
    │ WMS     │  ← mGlobalLock (全局锁)
    │ Handler │
    └────┬────┘
         │
    ┌────▼────────────────────────────────────┐
    │  Choreographer (V-Sync 信号)            │
    │    │                                    │
    │    └── doFrame()                        │
    │         └── performTraversals()         │
    │              └── 触发 WMS 布局/动画      │
    └─────────────────────────────────────────┘
```

---

## 总结

| 模块 | 职责 |
|------|------|
| **WindowState** | 代表一个窗口，存储位置、大小、Surface 等信息 |
| **WindowToken** | 窗口组标识，用于权限控制和窗口管理 |
| **DisplayContent** | 显示屏内容容器，管理该屏所有窗口 |
| **RootWindowContainer** | 所有 DisplayContent 的根容器 |
| **PhoneWindowManager** | 窗口策略，处理导航栏、状态栏、按键等 |
| **WindowAnimator** | 动画调度，协调所有窗口动画 |
| **InputMonitor** | 桥接 WMS 和 InputDispatcher |

WMS 的核心工作就是：**管理窗口的添加/移除、计算布局、协调 Surface 显示、与 Input 系统配合处理触摸事件**。