用户问的是 "pengingQueue"，这应该是 "PendingQueue" 的拼写错误。在 Android Input 系统中，除了之前讲到的 IQ、OQ、WQ 之外，还有一个 PendingQueue。让我详细讲解这个队列的含义和作用。

结合之前的对话上下文，用户是在问 Android Input 模块中的 PendingQueue（待处理队列）。
你问的应该是 **PendingQueue**（拼写为 pending，不是 penging）——这是 Android Input 系统中的**第四个重要队列**，之前我们讲了 IQ、OQ、WQ，现在补全这个 **PQ**：

---

## PendingQueue (PQ) 详解

### 定义

**PendingQueue** = **连接队列 / 挂起队列**

存放的是：**当前窗口尚未就绪、暂时无法分发、需要"等待"的事件**

### 四大队列完整关系图

```
                    ┌─────────────────────────────────┐
                    │        InputReader               │
                    │   (独立线程, 读取硬件事件)          │
                    └──────────────┬──────────────────┘
                                   │ push
                                   ▼
┌──────────────────────────────────────────────────────────────┐
│                     ★ IQ (InboundQueue)                      │
│              原始事件入队，等待 Dispatcher 处理                │
│              [event1] → [event2] → [event3]                   │
└──────────────────────────┬───────────────────────────────────┘
                           │ dispatchOnce() 取出
                           ▼
┌──────────────────────────────────────────────────────────────┐
│            InputDispatcher.dispatchInnerLocked()             │
│                                                               │
│     ┌─────────────┐    ┌───────────────────────────┐         │
│     │ 目标窗口就绪? │──Yes→│ ★ OQ (OutboundQueue)    │         │
│     │ (focused?    │       │ 待发送给目标窗口的事件      │         │
│     │  paused?     │       │ 准备通过 Socket 发送      │         │
│     │  有ANR?)     │       └───────────┬─────────────┘         │
│     └──────┬──────┘                   │ sendEvent              │
│            │ No                       ▼                        │
│            ▼              ┌───────────────────────────┐         │
│     │ ★ PQ (PendingQueue) │   ★ WQ (WaitQueue)         │         │
│     │ 暂时无法分发的事件    │   已发送,等待App finish确认  │         │
│     │ 原因:               │                             │         │
│     │ - 窗口 paused       │  [event1-finish?]           │         │
│     │ - 窗口未 ready      │  [event2-finish?]           │         │
│     │ - 等待 ANR 超时判定  │                             │         │
│     └─────────────────────┴─────────────────────────────┘         │
└──────────────────────────────────────────────────────────────┘
```

### PQ 存放的典型场景

| 触发条件 | PQ 行为 | 说明 |
|----------|---------|------|
| **窗口处于 `PAUSED` 状态** | 事件进入 PQ 排队 | Activity 被 Dialog/部分遮挡 |
| **窗口未完成首帧绘制** | 事件进入 PQ 排队 | `mDrawDone` 为 false |
| **窗口连接断开/正在重连** | 事件进入 PQ 排队 | Socket 异常恢复期 |
| **触摸模式切换瞬间** | 事件短暂进入 PQ | KEY → TOUCH 模式转换间隙 |

### 核心源码逻辑

```cpp
// frameworks/native/services/inputflinger/dispatcher/InputDispatcher.cpp

bool InputDispatcher::dispatchKeyLocked(
        nsecs_t currentTime, std::shared_ptr<KeyEntry> entry,
        DropReason* dropReason, nsecs_t* nextWakeupTime) {
    
    // ... 权限校验等前置判断 ...

    // ★ 关键：检查目标窗口状态
    std::vector<InputTarget> inputTargets;
    
    // 查找焦点窗口
    sp<IBinder> focusedToken = mFocusResolver.getFocusedWindowToken(displayId);
    
    // 判断是否可以分发
    InjectionResult injectionResult =
        findFocusedWindowTargetsLocked(currentTime, entry, inputTargets, nextWakeupTime);

    switch (injectionResult) {
        case InjectionResult::SUCCESS:
            // ★ 成功 → 放入 OQ，准备发送
            addDispatchEntryLocked(currentTime, entry, inputTargets, 
                                   InputTarget::FLAG_DISPATCH_AS_IS);
            break;
            
        case InjectionResult::TARGET_PAUSED:
        case InjectionResult::TARGET_NOT_READY:
            // ★ 窗口不就绪 → 放入 PQ！等待下次 retry
            *dropReason = DropReason::NOT_DROPPED;
            // 事件不会被丢弃，而是挂起到 PendingQueue
            mPendingEvent = entry;  // 或加入 PendingQueue
            break;
            
        case InjectionResult::PERMISSION_DENIED:
            // 无权限 → 直接丢弃
            *dropReason = DropReason::POLICY;
            return false;
    }
}
```

```cpp
// 下次 dispatchOnce 时会尝试重新分发 PQ 中的事件
void InputDispatcher::dispatchOnceInnerLocked(nsecs_t* nextWakeupTime) {
    
    // Step 1: 如果有 PendingQueue 中的事件，先尝试重新分发
    if (mPendingEvent != nullptr) {
        if (currentTime >= mPendingEvent->policyActionTimeout) {
            // 超时了 → 可能触发 ANR
        }
        
        // 再次尝试 findFocusedWindowTargets
        // 如果这次窗口就绪了 → 从 PQ 移到 OQ 发送
        if (isWindowReadyNow()) {
            dispatchCurrentEventLocked(...);  // 从 PQ 取出发送
        } else {
            // 仍然不就绪 → 继续在 PQ 等待
            *nextWakeupTime = mPendingEvent->readTime + DISPATCHING_TIMEOUT;
        }
    }
    
    // Step 2: 从 IQ 取新事件
    // ...
}
```

### PQ 与其他队列的关键区别

```
┌────────────┬────────────────┬──────────────────┬─────────────────┐
│   队列名称   │    存放内容     │    移出条件      │    是否算 ANR   │
├────────────┼────────────────┼──────────────────┼─────────────────┤
│ IQ         │ 所有原始输入事件 │ Dispatcher取走  │ 不算            │
│ OQ         │ 即将发送的事件   │ 通过Socket发送  │ 不算            │
│ WQ         │ 等待finish确认  │ App回复finish   │ ★ 超时算ANR     │
│ PQ         │ 暂存无法分发事件 │ 窗口就绪后retry  │ ★ 超时也算ANR   │
└────────────┴────────────────┴──────────────────┴─────────────────┘

★ PQ 超时 ANR 场景示例：
  用户点击按钮 → 事件进入 PQ（因为目标窗口 paused）
  → 窗口一直不 resume
  → 超过 5s (INPUT_DISPATCHING_TIMEOUT)
  → 系统弹出 "XXX 应用无响应"
  
  注意：这个 ANR 是因为 PQ 中事件积压导致，
  不是 WQ 阻塞导致的，两者报错信息略有不同
```

### 实际排查中的应用

```bash
# 查看 InputDispatcher 各队列状态
adb shell dumpsys input | grep -A 5 "Queue"

# 典型输出：
# InboundQueue: count=0          ← IQ 空（Reader正常消费）
# OutboundQueue: count=0         ← OQ 空（正常发送）
# WaitQueue: count=1             ← WQ 有1个在等finish
# PendingQueue: count=3          ← ★ PQ 有3个事件卡住！

# 看到 PQ 有堆积时：
# 1. 检查目标窗口状态：dumpsys activity activities | grep mResumed
# 2. 检查是否有 Dialog/浮窗遮挡
# 3. 检查窗口 Surface 是否正常创建
# 4. 查看是否有 Connection Broken 日志
```

### PQ 与 WQ 的对比总结图

```
时间线视角理解四个队列:

T1  T2  T3  T4  T5  T6  T7  T8  T9  T10
│   │   │   │   │   │   │   │   │   │
▼   ▼   ▼   ▼   ▼   ▼   ▼   ▼   ▼   ▼
[硬件产生触摸]
    │
    ├─→ ★ IQ ◄───────────────── Event 在此排队等 Reader 读
    │       │
    │       ▼ (被取出)
    │   [Dispatcher 分析目标]
    │       │
    │   ├── 窗口OK? ──→ ★ OQ ◄─── Event 在此等 Socket 发送
    │   │                   │
    │   │                   ▼ (发送成功)
    │   │               ★ WQ ◄─── Event 在此等 App finish
    │   │                   │
    │   │                   ▼ (收到finish)
    │   │               [流程结束 ✓]
    │   │
    │   └── 窗口NOT OK! ──→ ★ PQ ◄─── Event 在此等窗口就绪
    │                           │
    │                           ▼ (窗口resume)
    │                       [回到 OQ 继续发]
    │
    └─→ (下一个事件...)
```

---

### 一句话记忆法

| 队列 | 口诀 |
|------|------|
| **IQ** | **I**n = **进**来，原始事件的入口 |
| **OQ** | **O**ut = **出**去，准备发给对方 |
| **WQ** | **W**ait = **等**着对方回复 finish |
| **PQ** | **P**ending = **悬**着，对方没准备好，先挂着 |

这样 **IQOWQP** 四大队列就完整了！如果还有其他问题或者想深入了解某个具体队列的实现细节，随时问我。
