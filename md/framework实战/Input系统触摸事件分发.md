

发布时间：2023-03-02 14:25:49 阅读：426 作者：iii 栏目：[开发技术](https://www.yisu.com/jc/kf/)

Android开发者专用服务器限时活动，0元免费领，库存有限，领完即止！ 点击查看>>

本篇内容介绍了“Android怎么开发Input系统触摸事件分发”的有关知识，在实际案例的操作过程中，不少人都会遇到这样的困境，接下来就让小编带领大家学习一下如何处理这些情况吧！希望大家仔细阅读，能够学有所成！

### 引言

Input系统: InputReader 处理触摸事件 分析了 InputReader 对触摸事件的处理流程，最终的结果是把触摸事件包装成 NotifyMotionArgs，然后分发给下一环。根据 Input系统: InputManagerService的创建与启动 可知，下一环是 InputClassifier。然而系统目前并不支持 InputClassifier 的功能，因此事件会被直接发送到 InputDispatcher。

Input系统: 按键事件分发 分析了按键事件的分发流程，虽然分析的目标是按键事件，但是也从整体上，描绘了事件分发的框架。

### 1. InputDispatcher 收到触摸事件

void InputDispatcher::notifyMotion(const NotifyMotionArgs* args) {
    if (!validateMotionEvent(args->action, args->actionButton, args->pointerCount,
                             args->pointerProperties)) {
        return;
    }
    uint32_t policyFlags = args->policyFlags;
    // 来自InputReader/InputClassifier的 motion 事件，都是受信任的
    policyFlags |= POLICY_FLAG_TRUSTED;
    android::base::Timer t;
    // 1. 对触摸事件执行截断策略
    // 触摸事件入队前，查询截断策略，查询的结果保存到参数 policyFlags
    mPolicy->interceptMotionBeforeQueueing(args->displayId, args->eventTime, /*byref*/ policyFlags);
    if (t.duration() > SLOW_INTERCEPTION_THRESHOLD) {
        ALOGW("Excessive delay in interceptMotionBeforeQueueing; took %s ms",
              std::to_string(t.duration().count()).c_str());
    }
    bool needWake;
    { // acquire lock
        mLock.lock();
        if (shouldSendMotionToInputFilterLocked(args)) {
            // ...
        }
        // 包装成 MotionEntry
        // Just enqueue a new motion event.
        std::unique_ptr<MotionEntry> newEntry =
                std::make_unique<MotionEntry>(args->id, args->eventTime, args->deviceId,
                                              args->source, args->displayId, policyFlags,
                                              args->action, args->actionButton, args->flags,
                                              args->metaState, args->buttonState,
                                              args->classification, args->edgeFlags,
                                              args->xPrecision, args->yPrecision,
                                              args->xCursorPosition, args->yCursorPosition,
                                              args->downTime, args->pointerCount,
                                              args->pointerProperties, args->pointerCoords, 0, 0);
        // 2. 把触摸事件加入收件箱
        needWake = enqueueInboundEventLocked(std::move(newEntry));
        mLock.unlock();
    } // release lock
    // 3. 如果有必要，唤醒线程处理触摸事件
    if (needWake) {
        mLooper->wake();
    }
}

AI代码助手

InputDispatcher 收到触摸事件后的处理流程，与收到按键事件的处理流程非常相似

- 对触摸事件进行截断策略查询。
    
- 把触摸事件加入 InputDispatcher 收件箱，然后唤醒线程处理触摸事件。
    

#### 1.1 截断策略查询

void NativeInputManager::interceptMotionBeforeQueueing(const int32_t displayId, nsecs_t when,
        uint32_t&amp; policyFlags) {
    bool interactive = mInteractive.load();
    if (interactive) {
        policyFlags |= POLICY_FLAG_INTERACTIVE;
    }
    // 受信任，并且是非注入的事件
    if ((policyFlags &amp; POLICY_FLAG_TRUSTED) &amp;&amp; !(policyFlags &amp; POLICY_FLAG_INJECTED)) {
        if (policyFlags &amp; POLICY_FLAG_INTERACTIVE) {
            // 设备处于交互状态下，受信任且非注入的事件，直接发送给用户，而不经过截断策略处理
            policyFlags |= POLICY_FLAG_PASS_TO_USER;
        } else {
            // 只有设备处于非交互状态，触摸事件才需要执行截断策略
            JNIEnv* env = jniEnv();
            jint wmActions = env-&gt;CallIntMethod(mServiceObj,
                        gServiceClassInfo.interceptMotionBeforeQueueingNonInteractive,
                        displayId, when, policyFlags);
            if (checkAndClearExceptionFromCallback(env,
                    "interceptMotionBeforeQueueingNonInteractive")) {
                wmActions = 0;
            }
            handleInterceptActions(wmActions, when, /*byref*/ policyFlags);
        }
    } else { // 注入事件，或者不受信任事件
        // 只有在交互状态下，才传递给用户
        // 注意，这里还有另外一层意思: 非交互状态下，不发送给用户
        if (interactive) {
            policyFlags |= POLICY_FLAG_PASS_TO_USER;
        }
    }
}
void NativeInputManager::handleInterceptActions(jint wmActions, nsecs_t when,
        uint32_t&amp; policyFlags) {
    if (wmActions &amp; WM_ACTION_PASS_TO_USER) {
        policyFlags |= POLICY_FLAG_PASS_TO_USER;
    }
}

AI代码助手

一个触摸事件，必须满足下面三种情况，才执行截断策略

- 触摸事件是受信任的。来自输入设备的触摸事件都是受信任的。
    
- 触摸事件是非注入的。monkey 的原理就是注入触摸事件，因此它的事件是不需要经过截断策略处理的。
    
- 设备处于非交互状态。一般来说，非交互状态指的就是显示屏处于灭屏状态。
    

另外还需要关注的是，事件在什么时候是不需要经过截断策略，有两种情况

- 对于受信任且非注入的触摸事件，如果设备处于交互状态，直接发送给用户。 也就是说，如果显示屏处于亮屏状态，输入设备产生的触摸事件一定会发送给窗口。
    
- 对于不受信任，或者注入的触摸事件，如果设备处于交互状态，也是直接发送给用户。也就是说，如果显示屏处于亮屏状态，monkey 注入的触摸事件，也是直接发送给窗口的。
    

最后还要注意一件事，如果一个触摸事件是不受信任的事件，或者是注入事件，当设备处于非交互状态下(通常指灭屏)，那么它不经过截断策略，也不会发送给用户，也就是会被丢弃。

在实际工作中处理的触摸事件，通常都是来自输入设备，它肯定是受信任的，而且非注入的，因此它只有在设备处于非交互状态下(一般指灭屏)下，非会执行截断策略，而如果设备处于交互状态(通常指亮屏)，会被直接分发给窗口。

现在来看下截断策略的具体实现

// PhoneWindowManager.java
    public int interceptMotionBeforeQueueingNonInteractive(int displayId, long whenNanos,
            int policyFlags) {
        // 1. 如果策略要求唤醒屏幕，那么截断这个触摸事件
        // 一般来说，唤醒屏幕的策略取决于设备的配置文件
        if ((policyFlags &amp; FLAG_WAKE) != 0) {
            if (wakeUp(whenNanos / 1000000, mAllowTheaterModeWakeFromMotion,
                    PowerManager.WAKE_REASON_WAKE_MOTION, "android.policy:MOTION")) {
                // 返回 0，表示截断触摸事件
                return 0;
            }
        }
        // 2. 判断非交互状态下，是否截断事件
        if (shouldDispatchInputWhenNonInteractive(displayId, KEYCODE_UNKNOWN)) {
            // 返回这个值，表示不截断事件，也就是事件分发给用户
            return ACTION_PASS_TO_USER;
        }
        // 忽略 theater mode
        if (isTheaterModeEnabled() &amp;&amp; (policyFlags &amp; FLAG_WAKE) != 0) {
            wakeUp(whenNanos / 1000000, mAllowTheaterModeWakeFromMotionWhenNotDreaming,
                    PowerManager.WAKE_REASON_WAKE_MOTION, "android.policy:MOTION");
        }
        // 3. 默认截断触摸事件
        // 返回0，表示截断事件
        return 0;
    }
    private boolean shouldDispatchInputWhenNonInteractive(int displayId, int keyCode) {
        // Apply the default display policy to unknown displays as well.
        final boolean isDefaultDisplay = displayId == DEFAULT_DISPLAY
                || displayId == INVALID_DISPLAY;
        final Display display = isDefaultDisplay
                ? mDefaultDisplay
                : mDisplayManager.getDisplay(displayId);
        final boolean displayOff = (display == null
                || display.getState() == STATE_OFF);
        if (displayOff &amp;&amp; !mHasFeatureWatch) {
            return false;
        }
        // displayOff 表示屏幕处于 off 状态，但是非 off 状态，并不表示一定是亮屏状态
        // 对于 doze 状态，屏幕处于 on 状态，但是屏幕可能仍然是黑的
        // 因此，只要屏幕处于 on 状态，并且显示了锁屏，触摸事件不会截断
        if (isKeyguardShowingAndNotOccluded() &amp;&amp; !displayOff) {
            return true;
        }
        // 对于触摸事件，keyCode 的值为 KEYCODE_UNKNOWN
        if (mHasFeatureWatch &amp;&amp; (keyCode == KeyEvent.KEYCODE_BACK
                || keyCode == KeyEvent.KEYCODE_STEM_PRIMARY
                || keyCode == KeyEvent.KEYCODE_STEM_1
                || keyCode == KeyEvent.KEYCODE_STEM_2
                || keyCode == KeyEvent.KEYCODE_STEM_3)) {
            return false;
        }
        // 对于默认屏幕，如果设备处于梦境状态，那么触摸事件不截断
        // 因为 doze 组件需要接收触摸事件，可能会唤醒屏幕
        if (isDefaultDisplay) {
            IDreamManager dreamManager = getDreamManager();
            try {
                if (dreamManager != null &amp;&amp; dreamManager.isDreaming()) {
                    return true;
                }
            } catch (RemoteException e) {
                Slog.e(TAG, "RemoteException when checking if dreaming", e);
            }
        }
        // Otherwise, consume events since the user can't see what is being
        // interacted with.
        return false;
    }

AI代码助手

截断策略是否截断触摸事件，取决于策略的返回值，有两种情况

- 返回 0，表示截断触摸事件。
    
- 返回 ACTION_PASS_TO_USER ，表示不截断触摸事件，也就是把触摸事件分发给用户/窗口。
    

下面列举触摸事件截断与否的情况，但是要注意一个前提，设备处于非交互状态(一般就是指灭屏状态)

- 事件会被传递给用户，也就是不截断，情况如下
    

- 有锁屏，并且显示屏处于非 off 状态。注意，非 off 状态，并不是表示屏幕处于 on(亮屏) 状态，也可能是 doze 状态(屏幕处于低电量状态)，doze 状态屏幕也是黑的。
    
- 梦境状态。因为梦境状态下会运行 doze 组件。
    

- 事件被截断，情况如下
    

- 策略标志位包含 FLAG_WAKE ，它会导致屏幕被唤醒，因此需要截断触摸事件。FLAG_WAKE 一般来自于输入设备的配置文件。
    
- 没有锁屏，没有梦境，也没有 FLAG_WAKE，默认就会截断。
    

从上面的分析可以总结出了两条结论

- 如果系统有组件在运行，例如，锁屏、doze组件，那么触摸事件需要分发到这些组件，因此不会被截断。
    
- 如果没有组件运行，触摸事件都会被截断。触摸事件由于需要唤醒屏幕，而导致被截断，只是其中一个特例。
    

### 2. InputDispatcher 分发触摸事件

由 Input系统: InputManagerService的创建与启动 可知，InputDispatcher 通过线程循环来处理收件箱中的事件，而且一次循环只能处理一个事件

void InputDispatcher::dispatchOnce() {
    nsecs_t nextWakeupTime = LONG_LONG_MAX;
    { // acquire lock
        std::scoped_lock _l(mLock);
        mDispatcherIsAlive.notify_all();
        if (!haveCommandsLocked()) {
            // 1. 分发一个触摸事件
            dispatchOnceInnerLocked(&amp;nextWakeupTime);
        }
        // 触摸事件的分发过程不会产生命令
        if (runCommandsLockedInterruptible()) {
            nextWakeupTime = LONG_LONG_MIN;
        }
        // 2. 计算线程下次唤醒的时间点，以便处理 anr
        const nsecs_t nextAnrCheck = processAnrsLocked();
        nextWakeupTime = std::min(nextWakeupTime, nextAnrCheck);
        if (nextWakeupTime == LONG_LONG_MAX) {
            mDispatcherEnteredIdle.notify_all();
        }
    } // release lock
    // 3. 线程休眠指定的时长
    nsecs_t currentTime = now();
    int timeoutMillis = toMillisecondTimeoutDelay(currentTime, nextWakeupTime);
    mLooper-&gt;pollOnce(timeoutMillis);
}

AI代码助手

一次线程循环处理触摸事件的过程如下

- 分发一个触摸事件。
    
- 当事件分发给窗口后，会计算一个窗口反馈的超时时间，利用这个时间，计算线程下次唤醒的时间点。
    
- 利用上一步计算出的线程唤醒的时间点，计算出线程最终需要休眠多长时间。当线程被唤醒后，会检查接收触摸时间的窗口，是否反馈超时，如果超时，会引发 ANR。
    

现在来看看如何分发一个触摸事件

void InputDispatcher::dispatchOnceInnerLocked(nsecs_t* nextWakeupTime) {
    nsecs_t currentTime = now();
    if (!mDispatchEnabled) {
        resetKeyRepeatLocked();
    }
    if (mDispatchFrozen) {
        return;
    }
    // 这里是优化 app 切换的延迟
    // mAppSwitchDueTime 是 app 切换的超时时间，如果小于当前时间，那么表明app切换超时了
    // 如果app切换超时，那么在app切换按键事件之前的未处理的事件，都将会被丢弃
    bool isAppSwitchDue = mAppSwitchDueTime <= currentTime;
    if (mAppSwitchDueTime < *nextWakeupTime) {
        *nextWakeupTime = mAppSwitchDueTime;
    }
    // mPendingEvent 表示正在处理的事件
    if (!mPendingEvent) {
        if (mInboundQueue.empty()) {
            // ...
        } else {
            // 1. 从收件箱队列中取出事件
            mPendingEvent = mInboundQueue.front();
            mInboundQueue.pop_front();
            traceInboundQueueLengthLocked();
        }
        // 如果这个事件需要传递给用户，那么需要同上层的 PowerManagerService，此时有用户行为，这个作用就是延长亮屏的时间
        if (mPendingEvent->policyFlags & POLICY_FLAG_PASS_TO_USER) {
            pokeUserActivityLocked(*mPendingEvent);
        }
    }
    ALOG_ASSERT(mPendingEvent != nullptr);
    bool done = false;
    // 检测丢弃事件的原因
    DropReason dropReason = DropReason::NOT_DROPPED;
    if (!(mPendingEvent->policyFlags & POLICY_FLAG_PASS_TO_USER)) {
        // 被截断策略截断
        dropReason = DropReason::POLICY;
    } else if (!mDispatchEnabled) {
        // 一般是由于系统正在系统或者正在关闭
        dropReason = DropReason::DISABLED;
    }
    if (mNextUnblockedEvent == mPendingEvent) {
        mNextUnblockedEvent = nullptr;
    }
    switch (mPendingEvent->type) {
        // ....
        case EventEntry::Type::MOTION: {
            std::shared_ptr<MotionEntry> motionEntry =
                    std::static_pointer_cast<MotionEntry>(mPendingEvent);
            if (dropReason == DropReason::NOT_DROPPED && isAppSwitchDue) {
                // app 切换超时，导致触摸事件被丢弃
                dropReason = DropReason::APP_SWITCH;
            }
            if (dropReason == DropReason::NOT_DROPPED && isStaleEvent(currentTime, *motionEntry)) {
                // 10s 之前的事件，已经过期
                dropReason = DropReason::STALE;
            }
            // 这里是优化应用无响应的一个措施，会丢弃mNextUnblockedEvent之前的所有触摸事件
            if (dropReason == DropReason::NOT_DROPPED && mNextUnblockedEvent) {
                dropReason = DropReason::BLOCKED;
            }
            // 2. 分发触摸事件
            done = dispatchMotionLocked(currentTime, motionEntry, &dropReason, nextWakeupTime);
            break;
        }
        // ...
    }
    // 3. 如果事件被处理，重置一些状态，例如 mPendingEvent
    // 返回 true，就表示已经处理了事件
    // 事件被丢弃，或者发送完毕，都会返回 true
    // 返回 false，表示暂时不知道如何处理事件，因此线程会休眠
    // 然后，线程再次被唤醒时，再来处理这个事件
    if (done) {
        if (dropReason != DropReason::NOT_DROPPED) {
            dropInboundEventLocked(*mPendingEvent, dropReason);
        }
        mLastDropReason = dropReason;
        // 重置 mPendingEvent
        releasePendingEventLocked();
        // 立即唤醒，处理下一个事件
        *nextWakeupTime = LONG_LONG_MIN; // force next poll to wake up immediately
    }
}

AI代码助手

Input系统: 按键事件分发 已经分析过 InputDispatcher 的线程循环。而对于触摸事件，是通过 InputDispatcher::dispatchMotionLocked() 进行分发

bool InputDispatcher::dispatchMotionLocked(nsecs_t currentTime, std::shared_ptr<MotionEntry> entry,
                                           DropReason* dropReason, nsecs_t* nextWakeupTime) {
    if (!entry->dispatchInProgress) {
        entry->dispatchInProgress = true;
    }
    // 1. 触摸事件有原因需要丢弃，那么不走后面的分发流程
    if (*dropReason != DropReason::NOT_DROPPED) {
        setInjectionResult(*entry,
                           *dropReason == DropReason::POLICY ? InputEventInjectionResult::SUCCEEDED
                                                             : InputEventInjectionResult::FAILED);
        return true;
    }
    bool isPointerEvent = entry->source & AINPUT_SOURCE_CLASS_POINTER;
    std::vector<InputTarget> inputTargets;
    bool conflictingPointerActions = false;
    InputEventInjectionResult injectionResult;
    if (isPointerEvent) {
        // 寻找触摸的窗口，窗口保存到 inputTargets
        // 2. 为触摸事件，寻找触摸的窗口
        // 触摸的窗口保存到 inputTargets 中
        injectionResult =
                findTouchedWindowTargetsLocked(currentTime, *entry, inputTargets, nextWakeupTime,
                                               &conflictingPointerActions);
    } else {
        // ...
    }
    if (injectionResult == InputEventInjectionResult::PENDING) {
        // 返回 false，表示暂时不知道如何处理这个事件，这会导致线程休眠
        // 等线程下次被唤醒时，再来处理这个事件
        return false;
    }
    // 走到这里，表示触摸事件已经被处理，因此保存处理的结果
    // 只要返回的不是 InputEventInjectionResult::PENDING
    // 都表示事件被处理，无论是权限拒绝还是失败，或是成功
    setInjectionResult(*entry, injectionResult);
    if (injectionResult == InputEventInjectionResult::PERMISSION_DENIED) {
        ALOGW("Permission denied, dropping the motion (isPointer=%s)", toString(isPointerEvent));
        return true;
    }
    if (injectionResult != InputEventInjectionResult::SUCCEEDED) {
        CancelationOptions::Mode mode(isPointerEvent
                                              ? CancelationOptions::CANCEL_POINTER_EVENTS
                                              : CancelationOptions::CANCEL_NON_POINTER_EVENTS);
        CancelationOptions options(mode, "input event injection failed");
        synthesizeCancelationEventsForMonitorsLocked(options);
        return true;
    }
    // 走到这里，表示触摸事件已经成功找到触摸的窗口
    // Add monitor channels from event's or focused display.
    // 3. 触摸事件找到了触摸窗口，在分发给窗口前，保存 global monitor 到 inputTargets 中
    // 开发者选项中的 Show taps 和 Pointer location，利用的 global monitor
    addGlobalMonitoringTargetsLocked(inputTargets, getTargetDisplayId(*entry));
    if (isPointerEvent) {
        // ... 省略 portal window 处理的代码
    }
    if (conflictingPointerActions) {
        // ...
    }
    // 4. 分发事件给 inputTargets 中的所有窗口
    dispatchEventLocked(currentTime, entry, inputTargets);
    return true;
}

AI代码助手

一个触摸事件的分发过程，可以大致总结为以下几个过程

- 如果有原因表明触摸事件需要被丢弃，那么触摸事件不会走后面的分发流程，即被丢弃。
    
- 通常触摸事件是发送给窗口的，因此需要为触摸事件寻找触摸窗口。窗口最终被保存到 inputTargets 中。
    
- inputTargets 保存触摸窗口后，还要保存 global monitor 窗口。例如开发者选项中的 Show taps 和 Pointer location，就是利用这个窗口实现的。
    
- 启动分发循环，把触摸事件分发给 inputTargets 保存的窗口。 由于 Input系统: 按键事件分发 已经分发过这个过程，本文不再分析。
    

#### 2.1 寻找触摸的窗口

InputEventInjectionResult InputDispatcher::findTouchedWindowTargetsLocked(
        nsecs_t currentTime, const MotionEntry& entry, std::vector<InputTarget>& inputTargets,
        nsecs_t* nextWakeupTime, bool* outConflictingPointerActions) {
    // ...
    // 6. 对于非 DOWN 事件，获取已经 DOWN 事件保存的 TouchState
    // TouchState 保存了接收 DOWN 事件的窗口
    const TouchState* oldState = nullptr;
    TouchState tempTouchState;
    std::unordered_map<int32_t, TouchState>::iterator oldStateIt =
            mTouchStatesByDisplay.find(displayId);
    if (oldStateIt != mTouchStatesByDisplay.end()) {
        oldState = &(oldStateIt->second);
        tempTouchState.copyFrom(*oldState);
    }
    // ...
    // 第一个条件 newGesture 表示第一个手指按下
    // 后面一个条件，表示当前窗口支持 split motion，并且此时有另外一个手指按下
    if (newGesture || (isSplit && maskedAction == AMOTION_EVENT_ACTION_POINTER_DOWN)) {
        /* Case 1: New splittable pointer going down, or need target for hover or scroll. */
        // 触摸点的获取 x, y 坐标
        int32_t x;
        int32_t y;
        int32_t pointerIndex = getMotionEventActionPointerIndex(action);
        if (isFromMouse) {
            // ...
        } else {
            x = int32_t(entry.pointerCoords[pointerIndex].getAxisValue(AMOTION_EVENT_AXIS_X));
            y = int32_t(entry.pointerCoords[pointerIndex].getAxisValue(AMOTION_EVENT_AXIS_Y));
        }
        // 这里检测是否是第一个手指按下
        bool isDown = maskedAction == AMOTION_EVENT_ACTION_DOWN;
        // 1. 对于 DOWN 事件，根据触摸事件的x,y坐标，寻找触摸窗口
        // 参数 addOutsideTargets 表示，只有在第一个手指按下时，如果没有找到触摸的窗口，
        // 那么需要保存那些可以接受 OUTSIZE 事件的窗口到 tempTouchState
        newTouchedWindowHandle =
                findTouchedWindowAtLocked(displayId, x, y, &tempTouchState,
                                          isDown /*addOutsideTargets*/, true /*addPortalWindows*/);
        // 省略 ... 处理窗口异常的情况 ...
        // 2. 获取所有的 getsture monitor
        const std::vector<TouchedMonitor> newGestureMonitors = isDown
                ? selectResponsiveMonitorsLocked(
                          findTouchedGestureMonitorsLocked(displayId, tempTouchState.portalWindows))
                : tempTouchState.gestureMonitors;
        // 既没有找到触摸点所在的窗口，也没有找到 gesture monitor，那么此次寻找触摸窗口的任务就失败了
        if (newTouchedWindowHandle == nullptr && newGestureMonitors.empty()) {
            ALOGI("Dropping event because there is no touchable window or gesture monitor at "
                  "(%d, %d) in display %" PRId32 ".",
                  x, y, displayId);
            injectionResult = InputEventInjectionResult::FAILED;
            goto Failed;
        }
        // 走到这里，表示找到了触摸的窗口，或者找到 gesture monitor
        if (newTouchedWindowHandle != nullptr) {
            // 马上要保存窗口了，现在获取窗口的 flag
            int32_t targetFlags = InputTarget::FLAG_FOREGROUND | InputTarget::FLAG_DISPATCH_AS_IS;
            if (isSplit) {
                targetFlags |= InputTarget::FLAG_SPLIT;
            }
            if (isWindowObscuredAtPointLocked(newTouchedWindowHandle, x, y)) {
                targetFlags |= InputTarget::FLAG_WINDOW_IS_OBSCURED;
            } else if (isWindowObscuredLocked(newTouchedWindowHandle)) {
                targetFlags |= InputTarget::FLAG_WINDOW_IS_PARTIALLY_OBSCURED;
            }
            // Update hover state.
            if (maskedAction == AMOTION_EVENT_ACTION_HOVER_EXIT) {
                newHoverWindowHandle = nullptr;
            } else if (isHoverAction) {
                newHoverWindowHandle = newTouchedWindowHandle;
            }
            // Update the temporary touch state.
            // 如果窗口支持 split，那么用 tempTouchState 保存窗口的时候，要特别保存 pointer id
            BitSet32 pointerIds;
            if (isSplit) {
                uint32_t pointerId = entry.pointerProperties[pointerIndex].id;
                pointerIds.markBit(pointerId);
            }
            // 3. tempTouchState 保存找到的触摸的窗口
            // 如果是真的找到的触摸窗口，那么这里就是保存，如果是找到可以接受 OUTSIDE 的窗口，那么这里是更新
            tempTouchState.addOrUpdateWindow(newTouchedWindowHandle, targetFlags, pointerIds);
        } else if (tempTouchState.windows.empty()) {
            // If no window is touched, set split to true. This will allow the next pointer down to
            // be delivered to a new window which supports split touch.
            tempTouchState.split = true;
        }
        if (isDown) {
            // tempTouchState 保存所有的 gesture monitor
            // 4. 第一个手指按下时，tempTouchState 保存 gesture monitor
            tempTouchState.addGestureMonitors(newGestureMonitors);
        }
    } else {
        // ...
    }
    if (newHoverWindowHandle != mLastHoverWindowHandle) {
        // ....
    }
    {
        // 权限检测 ...
    }
    // 保存接收 AMOTION_EVENT_ACTION_OUTSIDE 的窗口
    if (maskedAction == AMOTION_EVENT_ACTION_DOWN) {
        // ...
    }
    // 第一个手指按下时，保存壁纸窗口
    if (maskedAction == AMOTION_EVENT_ACTION_DOWN) { // 
        // ...
    }
    // 走到这里，表示没有异常情况了
    injectionResult = InputEventInjectionResult::SUCCEEDED;
    // 5. 把 tempTouchState 保存了触摸窗口和gesture monitor，保存到 inputTargets 中
    for (const TouchedWindow& touchedWindow : tempTouchState.windows) {
        addWindowTargetLocked(touchedWindow.windowHandle, touchedWindow.targetFlags,
                              touchedWindow.pointerIds, inputTargets);
    }
    for (const TouchedMonitor& touchedMonitor : tempTouchState.gestureMonitors) {
        addMonitoringTargetLocked(touchedMonitor.monitor, touchedMonitor.xOffset,
                                  touchedMonitor.yOffset, inputTargets);
    }
    // Drop the outside or hover touch windows since we will not care about them
    // in the next iteration.
    tempTouchState.filterNonAsIsTouchWindows();
Failed:
    // ...
    // 6. 缓存 tempTouchState
    if (maskedAction != AMOTION_EVENT_ACTION_SCROLL) {
        if (tempTouchState.displayId >= 0) {
            mTouchStatesByDisplay[displayId] = tempTouchState;
        } else {
            mTouchStatesByDisplay.erase(displayId);
        }
    } 
    return injectionResult;
}

AI代码助手

为触摸事件寻找触摸窗口的过程，极其复杂。虽然这段代码被我省略了很多过程，但是我估计读者也会看得头晕。

对于 DOWN 事件

- 根据 x,y 坐标寻找触摸的窗口。
    
- 获取所有的 gesture monitor 窗口 。
    
- 把触摸窗口保存到 tempTouchState 中。
    
- 把所有的 gesture monitor 窗口保存到 tempTouchState 中。
    
- 为 tempTouchState 保存所有窗口，创建 InputTarget 对象，并保存到参数 inputTargets 中。
    
- 使用 mTouchStatesByDisplay 缓存 tempTouchState。
    

gesture monitor 是为了实现手势功能而添加的一个窗口。什么是手势功能？ 例如在屏幕的左边/右边，向屏幕中央滑动，会触发返回手势。这个手势功能用来替代导航键。在下一篇文章中，我会剖析这个手势功能的原理。

对于非 DOWN 事件，一般为 MOVE, UP 事件

- 获取 DOWN 事件缓存的 tempTouchState。 因为 tempTouchState 保存了处理 DOWN 事件的触摸窗口和 gesture monitor，非 DOWN 事件，也会发送给这些窗口。
    
- 重复 DOWN 事件的第5步。
    

当分析的代码量很大的时候，我们需要有一个整体的观念。为触摸事件寻找触摸窗口，最终的结果就是把找到的窗口保存到参数 inputTargets 中，后面会把事件分发给 inputTargets 保存的窗口。

##### 2.1.1 根据坐标找到触摸窗口

// addOutsideTargets 在第一个手指按下是为 true
// addPortalWindows 值为 true
// ignoreDragWindow 默认为 false
sp<InputWindowHandle> InputDispatcher::findTouchedWindowAtLocked(int32_t displayId, int32_t x,
                                                                 int32_t y, TouchState* touchState,
                                                                 bool addOutsideTargets,
                                                                 bool addPortalWindows,
                                                                 bool ignoreDragWindow) {
    if ((addPortalWindows || addOutsideTargets) && touchState == nullptr) {
        LOG_ALWAYS_FATAL(
                "Must provide a valid touch state if adding portal windows or outside targets");
    }
    // Traverse windows from front to back to find touched window.
    // 从前到后，遍历窗口
    const std::vector<sp<InputWindowHandle>>& windowHandles = getWindowHandlesLocked(displayId);
    for (const sp<InputWindowHandle>& windowHandle : windowHandles) {
        // ignoreDragWindow 默认为 false
        if (ignoreDragWindow && haveSameToken(windowHandle, mDragState->dragWindow)) {
            continue;
        }
        // 获取窗口信息
        const InputWindowInfo* windowInfo = windowHandle->getInfo();
        // 匹配属于特定屏幕的窗口
        if (windowInfo->displayId == displayId) {
            auto flags = windowInfo->flags;
            // 窗口要可见
            if (windowInfo->visible) {
                // 窗口要可触摸
                if (!flags.test(InputWindowInfo::Flag::NOT_TOUCHABLE)) {
                    // 检测是否为触摸模型: 可获取焦点，并且不允许窗口之外的触摸事件发送到它后面的窗口
                    bool isTouchModal = !flags.test(InputWindowInfo::Flag::NOT_FOCUSABLE) &&
                            !flags.test(InputWindowInfo::Flag::NOT_TOUCH_MODAL);
                    // 窗口是触摸模型，或者触摸的坐标点落在窗口上
                    if (isTouchModal || windowInfo->touchableRegionContainsPoint(x, y)) {
                        int32_t portalToDisplayId = windowInfo->portalToDisplayId;
                        // 如果是 portal window
                        if (portalToDisplayId != ADISPLAY_ID_NONE &&
                            portalToDisplayId != displayId) {
                            if (addPortalWindows) {
                                // For the monitoring channels of the display.
                                // touchState 保存 portal window
                                touchState->addPortalWindow(windowHandle);
                            }
                            // 递归调用，获取 portal display id 下的触摸窗口
                            return findTouchedWindowAtLocked(portalToDisplayId, x, y, touchState,
                                                             addOutsideTargets, addPortalWindows);
                        }
                        // 不是 portal window，直接返回找到的窗口
                        return windowHandle;
                    }
                }
                // 走到这里，表示没有找到触摸窗口。也就是说，既没有找到触摸模型的窗口，也没有找到包含触摸点的窗口
                // 当第一个手指按下是，addOutsideTargets 值为 true
                // NOT_TOUCH_MODAL 和 WATCH_OUTSIDE_TOUCH 一起使用，当第一个手指按下时，如果落在窗口之外
                // 窗口会收到 MotionEvent.ACTION_OUTSIDE 事件
                if (addOutsideTargets && flags.test(InputWindowInfo::Flag::WATCH_OUTSIDE_TOUCH)) {
                    touchState->addOrUpdateWindow(windowHandle,
                                                  InputTarget::FLAG_DISPATCH_AS_OUTSIDE,
                                                  BitSet32(0));
                }
            }
        }
    }
    return nullptr;
}

AI代码助手

这里涉及一个 portal window 的概念，由于我没有找到具体使用的地方，我大致猜测它的意思就是，设备外接一个屏幕，然后在主屏幕上显示一个窗口来操作这个外接屏幕。后面的分析，我将略过 portal window 的部分。当然，触摸掌握了触摸事件的分发流程，以后遇到了 portal window 的事情，再来分析，应该没问题的。

寻找触摸点所在的窗口，其实就是从上到下遍历所有窗口，然后找到满足条件的窗口。

窗口首先要满足前置条件

- 窗口要在指定屏幕上。
    
- 窗口要可见。
    
- 窗口要可触摸。
    

满足了所有的前置条件后，只要满足以下任意一个条件，那么就找到了触摸点所在的窗口

- 是触摸模型的窗口: 可获取焦点，并且不允许窗口之外的触摸事件发送到它后面的窗口。
    
- 触摸点的 x,y 坐标落在窗口坐标系中。
    

##### 2.1.2 保存窗口

// InputDispatcher 保存触摸窗口
void InputDispatcher::addWindowTargetLocked(const sp<InputWindowHandle>& windowHandle,
                                            int32_t targetFlags, BitSet32 pointerIds,
                                            std::vector<InputTarget>& inputTargets) {
    std::vector<InputTarget>::iterator it =
            std::find_if(inputTargets.begin(), inputTargets.end(),
                         [&windowHandle](const InputTarget& inputTarget) {
                             return inputTarget.inputChannel->getConnectionToken() ==
                                     windowHandle->getToken();
                         });
    const InputWindowInfo* windowInfo = windowHandle->getInfo();
    // 创建 InputTarget，并保存到参数 inputTargets
    if (it == inputTargets.end()) {
        InputTarget inputTarget;
        std::shared_ptr<InputChannel> inputChannel =
                getInputChannelLocked(windowHandle->getToken());
        if (inputChannel == nullptr) {
            ALOGW("Window %s already unregistered input channel", windowHandle->getName().c_str());
            return;
        }
        inputTarget.inputChannel = inputChannel;
        inputTarget.flags = targetFlags;
        inputTarget.globalScaleFactor = windowInfo->globalScaleFactor;
        inputTarget.displaySize =
                int2(windowHandle->getInfo()->displayWidth, windowHandle->getInfo()->displayHeight);
        inputTargets.push_back(inputTarget);
        it = inputTargets.end() - 1;
    }
    ALOG_ASSERT(it->flags == targetFlags);
    ALOG_ASSERT(it->globalScaleFactor == windowInfo->globalScaleFactor);
    // 保存 InputTarget 后，在保存窗口的坐标转换参数，
    // 这个参数可以把显示屏的坐标，转换为窗口的坐标
    it->addPointers(pointerIds, windowInfo->transform);
}
// InputDispatcher 保存 gesture monitor
void InputDispatcher::addMonitoringTargetLocked(const Monitor& monitor, float xOffset,
                                                float yOffset,
                                                std::vector<InputTarget>& inputTargets) {
    InputTarget target;
    target.inputChannel = monitor.inputChannel;
    target.flags = InputTarget::FLAG_DISPATCH_AS_IS;
    ui::Transform t;
    t.set(xOffset, yOffset);
    target.setDefaultPointerTransform(t);
    inputTargets.push_back(target);
}

AI代码助手

对于触摸事件，无论是触摸窗口，还是 gesture monitor，都会被转化为 InputTarget，然后保存到参数 inputTargets 中。当后面启动分发循环后，触摸事件就会发送到 inputTargets 保存的窗口中。