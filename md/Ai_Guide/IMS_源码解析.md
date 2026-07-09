# Android 10 InputManagerService（IMS）树状图核心分析

> 导出时间：2026-07-09
> 工作目录：`c:\D\android_project\cells-android10`（Android 10）
> 涉及文件：
> - `frameworks/base/services/core/java/com/android/server/input/InputManagerService.java`（约 95 KB）

---

# 一、类声明与定位

```
:121  public class InputManagerService extends IInputManager.Stub
      implements Watchdog.Monitor
```

- **Binder 接口**：`IInputManager.Stub` —— 对外（应用、SystemServer 其它服务、shell）暴露输入设备、键盘布局、输入通道、注入事件、振动等能力
- **角色**：Android 输入系统的 Java 层中枢，是 native 层 `InputManager`（InputReader + InputDispatcher + EventHub）的封装与回调接收方
- **与 native 的连接**：构造期 `nativeInit(this, mContext, mHandler.getLooper().getQueue())`（:321）建立 `mPtr`（:137）指针，native 侧事件经此回调回 Java
- **关键协作方**：WMS —— WMS 通过 `setWindowManagerCallbacks`（:331）把自身注册为 IMS 的 `WindowManagerCallbacks`，IMS 借此把焦点/输入通道/视图端口变化反馈给 WMS

---

# 二、IMS 树状图

```
InputManagerService (input)
├── 类声明 :121  extends IInputManager.Stub, implements Watchdog.Monitor
│
├── 内部类 / 子结构
│   ├── InputManagerHandler :2116           // 主线程 Handler（设备变更/键盘布局消息）
│   ├── InputFilterHost :2152               // IInputFilterHost.Stub（输入过滤器宿主）
│   ├── InputMonitorHost :2178              // IInputMonitorHost.Stub（输入监控宿主）
│   ├── InputDevicesChangedListenerRecord :2230   // 设备变更监听（DeathRecipient）
│   ├── TabletModeChangedListenerRecord :2258     // 平板模式监听
│   ├── VibratorToken :2286                 // 振动令牌（DeathRecipient）
│   ├── LocalService :2308                  // 实现 InputManagerInternal（供 framework 内部）
│   └── interface WindowManagerCallbacks :2065     // WMS 实现：焦点/输入通道/视口回调
│         ├── notifyInputChannelBroken :2072
│         ├── notifyFocusChanged(...)
│         ├── interceptKeyBeforeDispatch(...)      // ← 按键拦截（policy 入口）
│         ├── interceptMotionBeforeDispatch(...)
│         ├── getKeyCodeState / getScanCodeState
│         └── systemReady() :2110
│
├── 核心字段
│   ├── 原生 / 上下文
│   │   ├── mPtr :137                       // native InputManager 指针
│   │   ├── mContext :139
│   │   └── mHandler (InputManagerHandler) :140
│   ├── 与 WMS 协作
│   │   ├── mWindowManagerCallbacks :147    // WMS 回调（setWindowManagerCallbacks :331 注入）
│   │   ├── mFocusedWindow :188             // 当前焦点窗口（IWindow）
│   │   └── mFocusedWindowHasCapture :189
│   ├── 输入设备
│   │   ├── mInputDevicesLock :163
│   │   ├── mInputDevices :165              // 所有 InputDevice
│   │   ├── mInputDevicesChangedListeners :166  // SparseArray
│   │   └── mTabletModeChangedListeners :154
│   ├── 输入过滤 / 监控
│   │   ├── mInputFilterLock :184
│   │   ├── mInputFilter :185               // IInputFilter（如无障碍过滤）
│   │   └── mInputFilterHost :186
│   ├── 持久化 / 键盘
│   │   ├── mDataStore :160                 // PersistentDataStore（键盘布局/显隐）
│   │   └── mTempFullKeyboards :172
│   └── 启动标志
│       └── mSystemReady :149
│
└── 关键方法分区
    ├── I1 启动 / 生命周期
    │   ├── 构造 → nativeInit :321
    │   ├── setWindowManagerCallbacks :331
    │   ├── start :339                      // 启动 native 循环、注册广播
    │   └── WindowManagerCallbacks.systemReady :2110
    ├── I2 输入通道（窗口侧）
    │   ├── registerInputChannel :547
    │   ├── unregisterInputChannel :564
    │   ├── setFocusedApplication :1491
    │   └── setInputDispatchMode :1530      // 启用/冻结派发
    ├── I3 事件派发与拦截
    │   ├── injectInputEvent :616 → injectInputEventInternal :620
    │   ├── mWindowManagerCallbacks.interceptKeyBeforeDispatch(...)  // 按键拦截
    │   └── freeze/thaw（经 setInputDispatchMode）
    ├── I4 显示 / 视口
    │   └── setDisplayViewportsInternal :415  // 多屏视口同步给 native
    ├── I5 设备 / 键盘布局
    │   ├── reloadKeyboardLayouts :401
    │   ├── updateKeyboardLayouts :1068
    │   └── setCurrentKeyboardLayoutForInputDevice :1349
    ├── I6 振动
    │   ├── vibrate :1654
    │   └── cancelVibrate :1682
    └── I7 系统 / 调试
        ├── setSystemUiVisibility :1534
        ├── setPointerSpeedUnchecked :1580
        ├── notifyConfigurationChanged :1760
        ├── dump :1725
        └── monitor :1754                  // Watchdog.Monitor 锁检查
```

---

# 三、核心职责详解

| 子系统 | 关键字段 / 方法 | 负责内容 |
|--------|----------------|----------|
| native 桥梁 | `mPtr`（:137）/ `nativeInit`（:321） | 持有 native InputManager 指针，事件从 native 回调 Java |
| 与 WMS 协作 | `mWindowManagerCallbacks`（:147） | WMS 注入自身，IMS 反馈焦点/输入通道断裂/视口 |
| 焦点窗口 | `mFocusedWindow`（:188） | 记录当前焦点窗口 token，供派发目标判定 |
| 输入设备 | `mInputDevices`（:165）/ 监听器（:166） | 维护所有输入设备、通知设备增删 |
| 输入过滤 | `mInputFilter`（:185） | 在派发前对事件做过滤/变换（无障碍、手势等） |
| 键盘布局 | `mDataStore`（:160） | 持久化键盘布局、显隐状态（`PersistentDataStore`） |
| 事件注入 | `injectInputEvent`（:616） | 系统/测试注入按键、触摸事件 |
| 派发控制 | `setInputDispatchMode`（:1530） | 冻结/恢复输入派发（如转屏、锁屏） |
| 振动 | `vibrate`（:1654） | 输入设备振动（手柄/键盘反馈） |
| 调试 | `dump`（:1725）/ `monitor`（:1754） | dump 状态、Watchdog 锁监控 |

---

# 四、关键流程调用树

## 4.1 启动（与 WMS 联动）
```
SystemServer
  → new InputManagerService(...)          // nativeInit :321 建立 mPtr
  → wms = WindowManagerService.main(...)
  → ims.setWindowManagerCallbacks(wms.getInputManagerCallback())  :331  // WMS 注入回调
  → ims.start() :339                      // 启动 native 读取/派发循环
  → wms.systemReady() → IMS.WindowManagerCallbacks.systemReady :2110
      └── native 开始真正处理输入
```

## 4.2 输入事件派发（从内核到 App）
```
Kernel 输入事件
  → native EventHub → InputReader（native）
  → InputDispatcher（native）
      ├── 经 mPtr 调 Java 回调：mWindowManagerCallbacks.interceptKeyBeforeDispatch(...)  // 按键拦截（→ PhoneWindowManager 策略，见 WMS/PWS 分析）
      ├── 依据 mFocusedWindow / 输入通道 选定目标
      └── 通过 InputChannel 将事件送往目标 App 的 UI 线程
  → App 处理（onKeyDown / onTouchEvent ...）
```

## 4.3 窗口焦点与输入通道同步
```
WMS 调整焦点窗口
  → IMS.setFocusedApplication :1491 / 更新 mFocusedWindow :188
  → IMS.notifyFocusChanged(...)（native 侧）  // 派发目标随之切换
  → WMS 注册 InputChannel：IMS.registerInputChannel :547
      └── native 将 InputChannel 加入派发表
```

## 4.4 事件注入（如自动化测试 / 系统模拟）
```
调用方 → IMS.injectInputEvent :616
  → injectInputEventInternal :620
      └── native inject → 进入正常派发链路
```

---

# 五、与兄弟服务的关系

| 协作方 | 关系 | 场景 |
|--------|------|------|
| WMS | `setWindowManagerCallbacks`（:331）注入 `WindowManagerCallbacks` | IMS 把焦点、输入通道断裂、视口变更反馈给 WMS；WMS 把焦点窗口/视口传给 IMS |
| PWS（PhoneWindowManager） | 经 `WindowManagerCallbacks.interceptKeyBeforeDispatch` | 输入按键先到 WMS 回调，再交由策略（PWM）决定电源/Home 等含义（见 WMS_PWS 分析） |
| AMS / ATMS | 经 `InputManagerInternal`（LocalService :2308） | 进程/Activity 切换时协调输入焦点与冻结 |
| native InputManager | `mPtr`（:137） | 真正的读取（InputReader）、派发（InputDispatcher）、EventHub 在 native 层 |

---

# 六、关键行号速查表

| 内容 | 行号 |
|------|------|
| 类声明 | :121 |
| mPtr（native 指针） | :137 |
| mContext | :139 |
| mHandler（InputManagerHandler） | :140 |
| mWindowManagerCallbacks | :147 |
| mSystemReady | :149 |
| mTabletModeChangedListeners | :154 |
| mDataStore（PersistentDataStore） | :160 |
| mInputDevicesLock | :163 |
| mInputDevices | :165 |
| mInputDevicesChangedListeners | :166 |
| mInputFilterLock | :184 |
| mInputFilter | :185 |
| mInputFilterHost | :186 |
| mFocusedWindow | :188 |
| nativeInit | :321 |
| setWindowManagerCallbacks | :331 |
| start | :339 |
| reloadKeyboardLayouts | :401 |
| setDisplayViewportsInternal | :415 |
| registerInputChannel | :547 |
| unregisterInputChannel | :564 |
| injectInputEvent | :616 |
| injectInputEventInternal | :620 |
| updateKeyboardLayouts | :1068 |
| setCurrentKeyboardLayoutForInputDevice | :1349 |
| setFocusedApplication | :1491 |
| setInputDispatchMode | :1530 |
| setSystemUiVisibility | :1534 |
| setPointerSpeedUnchecked | :1580 |
| vibrate | :1654 |
| cancelVibrate | :1682 |
| dump | :1725 |
| monitor（Watchdog） | :1754 |
| notifyConfigurationChanged | :1760 |
| interface WindowManagerCallbacks | :2065 |
| notifyInputChannelBroken | :2072 |
| InputManagerHandler | :2116 |
| InputFilterHost | :2152 |
| InputMonitorHost | :2178 |
| InputDevicesChangedListenerRecord | :2230 |
| TabletModeChangedListenerRecord | :2258 |
| VibratorToken | :2286 |
| LocalService（InputManagerInternal） | :2308 |
| LocalService.setDisplayViewports | :2310 |
| LocalService.injectInputEvent | :2315 |

---

*本文档由会话分析自动整理导出；行号基于 cells-android10（Android 10）源码快照。*
