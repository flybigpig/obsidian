**面向 Android Framework 新手的主线拆解（原理 + 源码 + 实践）**

先记一句话：**InputMonitor 本质是给 InputDispatcher 增加一个"旁路接收者"**。正常目标窗口照常收事件，监听者拿到同一批事件副本（或通过 SPY 窗口参与手势流）。



## 一、先搞清 2 条监听通路

| 通路 | 核心入口 | 核心特点 |
|------|---------|---------|
| **A. Global Monitor 通道** | `InputManagerService.monitorInput()` | Dispatcher 在 Key/Touch 派发时附加 monitor target，监听通道拿到副本 |
| **B. Gesture InputMonitor（Spy Window）** | `InputManager.monitorGestureInput()` | IMS 创建 `InputConfig.SPY` 窗口参与触摸目标选择，可 `pilferPointers` |

**注意：**

1. 这类能力是系统级能力，核心权限是 `MONITOR_INPUT`。
2. AOSP 注释明确写了：新场景不建议继续扩展 Gesture Monitor，倾向使用更精确的 Spy Window 配置。

---

## 二、完整流程图（稳定版）

### InputMonitor 全局监听主线（两条通路合并图）

```
1) 客户端发起监听请求（monitorInput 或 monitorGestureInput）
    ↓
2) IMS 做权限与 display 校验，创建 InputChannel
    ↓
3A Global Monitor：Dispatcher createInputMonitor()，放入 mGlobalMonitorsByDisplay。
3B Spy Window：IMS 创建 GestureMonitorSpyWindow，设置 InputConfig.SPY。
    ↓
3) 输入事件到达 Dispatcher（Key/Touch）
    ↓
5A：dispatchKeyLocked/dispatchMotionLocked 里调用 addGlobalMonitoringTargetsLocked()。
5B：Spy 窗口通过 findTouchedSpyWindowsAtLocked() 参与目标计算。
    ↓
4) 事件写入监听 InputChannel，InputEventReceiver.onInputEvent() 回调
    ↓
5) 监听侧处理后 finishInputEvent；必要时 pilferPointers 接管手势
    ↓
6) 释放：dispose / removeInputChannel，回收监控资源
```

---

## 三、源码主线（API → IMS → Dispatcher）

### 主线 A：Global Monitor 通道（事件副本）

1. **IMS：** `monitorInput()` 调 `mNative.createInputMonitor(...)`（InputManagerService.java:773-781）。
2. **Dispatcher：** `createInputMonitor()` 打开 channel pair，连接标记 monitor=true，并登记到 `mGlobalMonitorsByDisplay`（InputDispatcher.cpp:6208-6240）。
3. **Key/Touch 派发时都会加 monitor target：** `dispatchKeyLocked()` 与 `dispatchMotionLocked()` 中调用 `addGlobalMonitoringTargetsLocked()`（1975-1977、2117-2119、3093-3108）。

### 主线 B：Gesture InputMonitor（Spy Window）

1. **API：** `InputManager.monitorGestureInput()` 已标注 deprecated，注释建议转向 Spy Window 思路（InputManager.java:1120-1135）。
2. **IMS：** 校验 `MONITOR_INPUT`，创建 GestureMonitorSpyWindow（InputManagerService.java:824-857）。
3. **Spy Window 关键配置：** `NOT_FOCUSABLE | SPY`，并要求 trusted overlay（GestureMonitorSpyWindow.java:65-69；InputDispatcher.cpp:5412-5417）。
4. **Touch 目标计算会收集 touched spy windows**（InputDispatcher.cpp:1502-1531），从而监听同一手势流。
5. **`pilferPointers`** 通过 host 回 IMS 再到 native，接管当前指针流（InputManagerService.java:3283-3298）。

### 接收端必做

监听端一般基于 `InputEventReceiver` 读取 InputChannel；收到事件后必须调用 `finishInputEvent`，否则队列会阻塞（InputEventReceiver.java:137-139, 213-230）。

---

## 四、新手实践步骤（最小可用）

下面这套流程适合先打通"能监听、能释放、不卡输入"。

```java
// 1) 创建 monitor（系统权限场景）
InputManager im = context.getSystemService(InputManager.class);
InputMonitor monitor = im.monitorGestureInput("global_spy", Display.DEFAULT_DISPLAY);

// 2) 绑定 InputEventReceiver
InputEventReceiver receiver = new InputEventReceiver(monitor.getInputChannel(), Looper.getMainLooper()) {
    @Override public void onInputEvent(InputEvent event) {
        try {
            // 过滤 MotionEvent/KeyEvent 做手势识别
        } finally {
            finishInputEvent(event, false);
        }
    }
};

// 3) 必要时接管当前手势流
im.pilferPointers(monitor.getInputChannel().getToken());

// 4) 释放资源（退出时）
receiver.dispose();
monitor.dispose();
```

```bash
# 运行时验证（重点看 Gesture Monitors / Dispatcher）
adb shell dumpsys input | grep -n "Gesture Monitors\|Input Dispatcher State"

# 触发触摸
adb shell cmd input tap 300 600
```

---

## 五、业界主流做法（避免踩坑）

### 推荐做法

1. 系统侧优先采用 Spy Window 的精确监听，而不是"全 display 全量抓取"。
2. 对监听窗口设置明确区域和生命周期，手势结束立即释放 monitor。
3. 只有在需要中途接管时才 pilfer，避免破坏正常交互。
4. 必须保证 `finishInputEvent` 成对调用，避免"监听器拖垮输入"。

---

## 六、Framework 里还有哪些地方在用

| 场景 | 复用点 |
|------|--------|
| Recents / PIP / Wallpaper InputConsumer | 都依赖同一套"输入窗口元数据更新"链路：`onWindowInfosChanged()` → `setInputWindowsLocked()`（InputDispatcher.cpp:7098-7149, 5388-5439） |
| Spy/Trusted Overlay 校验 | InputDispatcher 在窗口更新时统一校验 SPY 是否为 trusted overlay（5412-5417） |
| 手势接管与多窗口协作 | pilfer 指针、wallpaper duplicate touch、drag/cancel 都在同一 touch state 机制下处理（2578-2620） |

### 参考源码链接

- [InputManager.java (AOSP main)](https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main/core/java/android/hardware/input/InputManager.java)
- [InputManagerService.java (AOSP main)](https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main/services/core/java/com/android/server/input/InputManagerService.java)
- [GestureMonitorSpyWindow.java (AOSP main)](https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main/services/core/java/com/android/server/input/GestureMonitorSpyWindow.java)
- [InputDispatcher.cpp (AOSP main)](https://android.googlesource.com/platform/frameworks/native/+/refs/heads/main/services/inputflinger/dispatcher/InputDispatcher.cpp)
- [InputEventReceiver.java (AOSP main)](https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main/core/java/android/view/InputEventReceiver.java)

---

## 七、断点排障主线（5个关键点）

| 断点位置 | 要看什么 | 异常信号 |
|---------|---------|---------|
| `monitorGestureInput()` <br>InputManagerService.java:824 | 权限与 displayId 是否合法，Surface 是否创建成功 | SecurityException / 无 monitor 返回 |
| `createInputMonitor()` <br>InputDispatcher.cpp:6208 | 是否登记到 `mGlobalMonitorsByDisplay` | 监听通道创建了但分发列表为空 |
| `dispatchMotionLocked()` <br>InputDispatcher.cpp:2117 | 是否执行 `addGlobalMonitoringTargetsLocked()` | 事件到主窗但监听收不到 |
| `selectResponsiveMonitorsLocked()` <br>InputDispatcher.cpp:2393 | monitor connection 是否仍 responsive | 出现 Unresponsive monitor，后续新手势不再分发给它 |
| `InputEventReceiver.finishInputEvent()` <br>InputEventReceiver.java:213 | 回调后是否及时 finish | 分发卡顿、队列阻塞、监听方"越跑越慢" |

**排障口诀：** 先看"有没有 monitor"，再看"有没有加入 targets"，最后看"有没有 finish 回执"。

---

## 八、本文小结

InputMonitor 监听全局输入，核心不是"抢事件"，而是给 Dispatcher 增加可控的旁路目标。

实战重点只有三件事：**权限边界**、**finish 回执**、**按需 pilfer**。

落到 Framework 主线里，它和 Recents/PIP/Wallpaper 等输入能力共用同一套窗口元数据更新与目标选择机制。