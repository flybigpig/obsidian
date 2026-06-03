## 焦点窗口路径（Key）与命中窗口路径（Touch）

只记主线：**WMS 更新窗口信息/焦点** → **InputDispatcher 选目标窗口** → **InputChannel 派发** → **App 回执 finish**
## 一、核心原理

**Key 事件**：走 `findFocusedWindowTargetLocked()`，核心是"当前 display 的焦点窗口 token"。

**Touch 事件**：走 `findTouchedWindowTargetsLocked()`，核心是"坐标命中 + 触摸状态机 TouchState"。

两条路径都会生成 `InputTarget`，后续派发流程统一。

---

## 二、完整流程图

### 焦点窗口/命中窗口选择与派发闭环

```
1) WMS/InputMonitor 更新窗口信息与焦点
    ↓
2) InputDispatcher 接收 windowInfos/focus 变更
    ↓
3) FocusResolver 维护每个 display 的 focused token
    ↓
4A Key 分支：findFocusedWindowTargetLocked()，按焦点窗口找目标。
4B Touch 分支：findTouchedWindowTargetsLocked()，按坐标命中+TouchState找目标。
    ↓
4) 统一生成 InputTarget
    ↓
5) prepare/startDispatchCycle 派发
    ↓
6) InputChannel → App 处理 → finishInputEvent
    ↓
7) doDispatchCycleFinished：继续下一条 / fallback / ANR
```

### 常见排查断点（4个）

1. `findFocusedWindowTargetLocked`：是否拿到 focused token。
2. `PAUSE_DISPATCHING / no-focused-window`：是否进入 PENDING。
3. `publishKeyEvent`：事件是否真正写入 InputChannel。
4. `doDispatchCycleFinished`：是否收到 `finishInputEvent` 回执。

### dumpsys input 速查（5行）

1. **Focused display**：当前默认接收焦点分发的显示屏。
2. **Focused window**：Key 事件的主目标窗口。
3. **DispatchEnabled / DispatchFrozen**：是否允许继续派发。
4. **Connection waitQueue/outboundQueue**：是否有回执阻塞。
5. **Recent ANR / Unresponsive**：是否已触发窗口无响应链路。

---

## 三、源码主线（Key）

**文件：** InputDispatcher.cpp（Key）

1. **入口：** `dispatchKeyLocked()`
2. **找焦点：** `findFocusedWindowTargetLocked()`
3. **焦点来源：** `FocusResolver.getFocusedWindowToken(displayId)`
4. **无焦点窗口：** 进入 `PENDING` 并等待超时。
5. **有焦点窗口：** `addWindowTargetLocked(...FOREGROUND)`
6. **派发链路：** `prepareDispatchCycleLocked` → `startDispatchCycleLocked` → `publishKeyEvent`
7. **App 回执：** `finishInputEvent` 后进入 `doDispatchCycleFinishedCommand`
8. **未消费：** 可能走 `dispatchUnhandledKey` fallback。

---

## 四、源码主线（Touch）

**文件：** InputDispatcher.cpp（Touch）

1. **入口：** `dispatchMotionLocked()`
2. **指针事件目标选择：** `findTouchedWindowTargetsLocked()`
3. **坐标命中：** `findTouchedWindowAtLocked(x,y)`
4. **手势状态：** TouchState 维护 down/move/up/split/slippery/outside。
5. **生成目标：** 多个 `InputTarget`（前台窗口/监控窗口）。
6. **派发链路：** `prepareDispatchCycleLocked` → `publishMotionEvent`
7. **App 处理：** `processPointerEvent` 后调用 `finishInputEvent`。

---

## 五、新手实践步骤

先验证"当前焦点是谁"，再看"事件到底打到了谁"。

```bash
# 1) 看窗口焦点
adb shell dumpsys window windows | grep -E "mCurrentFocus|mFocusedApp"

# 2) 看输入系统状态（focus display/window、dispatcher）
adb shell dumpsys input

# 3) 触发按键/触摸，观察目标是否符合预期
adb shell cmd input keyevent 3
adb shell cmd input tap 300 600
```

---

## 六、Framework 中的同类应用

- **Recents / PIP / Wallpaper** 的 InputConsumer 都依赖同一套输入窗口元数据更新流程。

### 这三类 InputConsumer 的共用主线

1. `InputMonitor.updateInputWindows()` 中统一获取并布局：`INPUT_CONSUMER_RECENTS_ANIMATION`、`INPUT_CONSUMER_PIP`、`INPUT_CONSUMER_WALLPAPER`。
2. 统一写入 `InputWindowHandle` 元数据，并通过 `setInputWindowInfoIfNeeded()` 下发。
3. InputDispatcher 统一在 `onWindowInfosChanged()` / `setInputWindowsLocked()` 消费这些元数据并参与目标窗口选择。

- **拖拽（Drag）** 和 **Pointer Capture** 都会影响目标窗口选择与取消事件注入。
- **点窗外切焦点（pointer down outside focus）** 本质也是输入目标变化触发的焦点迁移。

---

## 七、面试速记（6行）

1. WMS 把窗口列表和焦点信息持续同步给 InputDispatcher。
2. Key 事件走 `findFocusedWindowTargetLocked`，核心看 focused token。
3. Touch 事件走 `findTouchedWindowTargetsLocked`，核心看坐标命中和 TouchState。
4. 两条路径都会生成 InputTarget，后续派发流程统一。
5. 事件经 InputChannel 发给 App，App 处理后必须 `finishInputEvent`。
6. 没焦点会等待/超时，没回执会进入 waitQueue 超时并触发输入 ANR。

---

## 八、Key 时序图（函数级）

### Key 事件如何找到焦点窗口并完成回执

```
┌─────────┐     ┌──────────────┐     ┌───────────────┐     ┌─────┐
│   WMS   │     │InputDispatcher │     │FocusResolver  │     │ App │
└────┬────┘     └──────┬───────┘     └───────┬───────┘     └──┬──┘
     │                 │                     │                │
     │ setFocusedWindow/onWindowInfosChanged │                │
     │────────────────>│                     │                │
     │                 │ setFocusedWindow/setInputWindows      │
     │                 │────────────────────>│                │
     │                 │                     │                │
     │                 │ getFocusedWindowToken                 │
     │                 │<────────────────────│                │
     │                 │                     │                │
     │                 │ dispatchKeyLocked   │                │
     │                 │ findFocusedWindowTargetLocked         │
     │                 │ addWindowTarget(FOREGROUND)           │
     │                 │                     │                │
     │                 │ prepare/startDispatchCycle            │
     │                 │ publishKeyEvent(seq,...)              │
     │                 │──────────────────────────────────────>│
     │                 │                     │                │
     │                 │                     │  processKeyEvent │
     │                 │                     │  finishInputEvent(seq, handled)
     │                 │<──────────────────────────────────────│
     │                 │                     │                │
     │                 │ doDispatchCycleFinished               │
     │                 │ handled=false -> dispatchUnhandledKey │
     │                 │ start next cycle / ANR                │
     │                 │                     │                │
```

---

## 九、本文小结

**主线就两句：Key 看焦点窗口，Touch 看坐标命中 + 手势状态。**

两者最终都转成 `InputTarget`，走统一派发与回执闭环。

---