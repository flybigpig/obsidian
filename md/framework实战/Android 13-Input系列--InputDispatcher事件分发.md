> hongxi.zhu 2023-7-20  
> Android 13

#### InputDispatcher::dispatchOnce()

frameworks/native/services/inputflinger/dispatcher/InputDispatcher.cpp

```cpp
void InputDispatcher::dispatchOnce() {
    nsecs_t nextWakeupTime = LONG_LONG_MAX;
    { // acquire lock
        std::scoped_lock _l(mLock);
        mDispatcherIsAlive.notify_all();  //唤醒在这个condition上wait的所有地方

        // Run a dispatch loop if there are no pending commands.
        // The dispatch loop might enqueue commands to run afterwards.
        // 如果当前没有待办的command需要处理（command一般是需要处理一些异常情况，比如ANR）
        // 就进行事件的分发
        if (!haveCommandsLocked()) {
            dispatchOnceInnerLocked(&nextWakeupTime);  //事件分发
        }

        // Run all pending commands if there are any.
        // If any commands were run then force the next poll to wake up immediately.
        // 如果mCommandQueue不为空，就执行完所有command，并将下次唤醒时间设置为最小值，强制下一次poll唤醒线程
        if (runCommandsLockedInterruptable()) {
            nextWakeupTime = LONG_LONG_MIN;
        }

        // If we are still waiting for ack on some events,
        // we might have to wake up earlier to check if an app is anr'ing.
        // 如果此时正在等待分发出去的事件的ack(目标应用的响应)，我们需要早点唤醒去检查这个应用是否正在ANR
        const nsecs_t nextAnrCheck = processAnrsLocked();
        nextWakeupTime = std::min(nextWakeupTime, nextAnrCheck);

        // We are about to enter an infinitely long sleep, because we have no commands or
        // pending or queued events
        // 如果唤醒时间还是LONG_LONG_MAX没有被修改
        // 说明上面没有待处理的事件也没有待处理的command，那么将进入无限期的休眠中
        if (nextWakeupTime == LONG_LONG_MAX) {
            mDispatcherEnteredIdle.notify_all();  //唤醒等待进入idle状态的condition
        }
    } // release lock

    // Wait for callback or timeout or wake.  (make sure we round up, not down)
    nsecs_t currentTime = now();
    int timeoutMillis = toMillisecondTimeoutDelay(currentTime, nextWakeupTime);
    mLooper->pollOnce(timeoutMillis);  //进入阻塞等待唤醒或者timeout或者callback回调
}
```

#### InputDispatcher::dispatchOnceInnerLocked

```cpp
void InputDispatcher::dispatchOnceInnerLocked(nsecs_t* nextWakeupTime) {
    nsecs_t currentTime = now();

    // Reset the key repeat timer whenever normal dispatch is suspended while the
    // device is in a non-interactive state.  This is to ensure that we abort a key
    // repeat if the device is just coming out of sleep.
    // 如果设备处于不可交互时，则不进行事件的分发
    // 如果处于不可分发状态则重置按键重复计时器
    if (!mDispatchEnabled) {
        resetKeyRepeatLocked();
    }

    // If dispatching is frozen, do not process timeouts or try to deliver any new events.
    // 如果处于frozen状态，则不处理任何有关目标进程是否有简单窗口的timeout行为，也不会进行事件的分发
    if (mDispatchFrozen) {
        if (DEBUG_FOCUS) {
            ALOGD("Dispatch frozen.  Waiting some more.");
        }
        return;
    }

    // Optimize latency of app switches.
    // Essentially we start a short timeout when an app switch key (HOME / ENDCALL) has
    // been pressed.  When it expires, we preempt dispatch and drop all other pending events.
    // 对APP切换时的优化措施，当按下类似HOME键时，启动一个超时机制(0.5s)，当timeout时，会立即分发事件并抛弃其他的未处理事件
    bool isAppSwitchDue = mAppSwitchDueTime <= currentTime;
    if (mAppSwitchDueTime < *nextWakeupTime) {
        *nextWakeupTime = mAppSwitchDueTime;
    }

    // Ready to start a new event.
    // If we don't already have a pending event, go grab one.
    if (!mPendingEvent) {  //正常一次分发前mPendingEvent = nullptr
        if (mInboundQueue.empty()) {
			...
        } else {  //因为inputReader已经往Inbound queue中添加了事件，所以不为空
            // Inbound queue has at least one entry.
            mPendingEvent = mInboundQueue.front(); //拿出一个待分发的事件
            mInboundQueue.pop_front(); //从mInboundQueue中移除这个事件
            traceInboundQueueLengthLocked();  //systrace、perfetto中可以看到这个队列size发生变化
        }

        // Poke user activity for this event.
        // 每次分发事件时都调PMS的updatePowerStateLocked更新电源的状态
        if (mPendingEvent->policyFlags & POLICY_FLAG_PASS_TO_USER) {
            pokeUserActivityLocked(*mPendingEvent);
        }
    }

    // Now we have an event to dispatch.
    // All events are eventually dequeued and processed this way, even if we intend to drop them.
    ALOG_ASSERT(mPendingEvent != nullptr);
    bool done = false;
    DropReason dropReason = DropReason::NOT_DROPPED;
    if (!(mPendingEvent->policyFlags & POLICY_FLAG_PASS_TO_USER)) {  //如果事件不是分发给用户的则抛弃drop
        dropReason = DropReason::POLICY;
    } else if (!mDispatchEnabled) {  //如果当前属于不可交互的状态则drop
        dropReason = DropReason::DISABLED;
    }

    if (mNextUnblockedEvent == mPendingEvent) {
        mNextUnblockedEvent = nullptr;
    }

    switch (mPendingEvent->type) {
		...
        case EventEntry::Type::KEY: {
			...
        }

        case EventEntry::Type::MOTION: {
            std::shared_ptr<MotionEntry> motionEntry =
                    std::static_pointer_cast<MotionEntry>(mPendingEvent);
            if (dropReason == DropReason::NOT_DROPPED && isAppSwitchDue) {
                dropReason = DropReason::APP_SWITCH;  //处于APP切换情形drop的事件
            }
            if (dropReason == DropReason::NOT_DROPPED && isStaleEvent(currentTime, *motionEntry)) {
           	    //事件超过了一个事件允许分发的最大时间(10s)(事件太老)则drop这个事件
           	    //时间计算：currentTime - entry.eventTime
                dropReason = DropReason::STALE;
            }
            if (dropReason == DropReason::NOT_DROPPED && mNextUnblockedEvent) {
            	//当焦点将要移到到新的应用上，则抛弃期间的事件
                dropReason = DropReason::BLOCKED;
            }
            //事件的分发（无论这个事件要分发还是要drop都要走这里）
            done = dispatchMotionLocked(currentTime, motionEntry, &dropReason, nextWakeupTime);
            break;
        }
		...
    }

    if (done) {
        if (dropReason != DropReason::NOT_DROPPED) {
            dropInboundEventLocked(*mPendingEvent, dropReason);
        }
        mLastDropReason = dropReason;

        releasePendingEventLocked();  //将mPendingEvent设置为nullptr，重置为下一个事件分发准备
        //  强制下一个poll中立即唤醒inputDispatcher线程来干活
        *nextWakeupTime = LONG_LONG_MIN; // force next poll to wake up immediately
    }
}
```

#### InputDispatcher::dispatchMotionLocked

```cpp
bool InputDispatcher::dispatchMotionLocked(nsecs_t currentTime, std::shared_ptr<MotionEntry> entry,
                                           DropReason* dropReason, nsecs_t* nextWakeupTime) {
    ATRACE_CALL();
    // Preprocessing.
    //将当前事件的dispatchInProgress设置为true表示处理中
    if (!entry->dispatchInProgress) {
        entry->dispatchInProgress = true;

        logOutboundMotionDetails("dispatchMotion - ", *entry);
    }

    // Clean up if dropping the event.
    //如果事件需要抛弃
    if (*dropReason != DropReason::NOT_DROPPED) {
        setInjectionResult(*entry,
                           *dropReason == DropReason::POLICY ? InputEventInjectionResult::SUCCEEDED
                                                             : InputEventInjectionResult::FAILED);
        return true;
    }
	//从entry->source判断事件的源类型
    const bool isPointerEvent = isFromSource(entry->source, AINPUT_SOURCE_CLASS_POINTER);

    // Identify targets.
    std::vector<InputTarget> inputTargets;

    bool conflictingPointerActions = false;
    InputEventInjectionResult injectionResult;
    if (isPointerEvent) {  //触屏这这里
        // Pointer event.  (eg. touchscreen)
        //重要的方法，获取当前焦点窗口，后续单独展开分析这个，这里就展开
        //获取到的焦点窗口为inputTargets
        injectionResult =
                findTouchedWindowTargetsLocked(currentTime, *entry, inputTargets, nextWakeupTime,
                                               &conflictingPointerActions);
    } else {
		...
    }
	...

    setInjectionResult(*entry, injectionResult);
	...

    // Add monitor channels from event's or focused display.
    //将全局的监视器窗口加入分发的目标窗口列表中（举个例子：开发者选项中的显示触摸轨迹图层就一个全局监视器）
    //只要事件在前面没有被抛弃，那么无论后面的流程是否拦截事件，这些全局监视器都能收到事件
    addGlobalMonitoringTargetsLocked(inputTargets, getTargetDisplayId(*entry));

    // Dispatch the motion.
	...
    dispatchEventLocked(currentTime, entry, inputTargets);  //往目标窗口分发事件
    return true;  //return true就可以进行下一次从iq中读取新事件并分发
}
```

#### InputDispatcher::dispatchEventLocked

```cpp
void InputDispatcher::dispatchEventLocked(nsecs_t currentTime,
                                          std::shared_ptr<EventEntry> eventEntry,
                                          const std::vector<InputTarget>& inputTargets) {
    ATRACE_CALL();

    updateInteractionTokensLocked(*eventEntry, inputTargets);

    ALOG_ASSERT(eventEntry->dispatchInProgress); // should already have been set to true

    pokeUserActivityLocked(*eventEntry);  //再次更新电源状态，避免进入息屏等行为

	//遍历所有找到的目标窗口inputTargets，通过他们的向他们inputChannel分发当前的事件
    for (const InputTarget& inputTarget : inputTargets) {
        sp<Connection> connection =
                getConnectionLocked(inputTarget.inputChannel->getConnectionToken());
        if (connection != nullptr) {
            prepareDispatchCycleLocked(currentTime, connection, eventEntry, inputTarget);
        } else {
            if (DEBUG_FOCUS) {
                ALOGD("Dropping event delivery to target with channel '%s' because it "
                      "is no longer registered with the input dispatcher.",
                      inputTarget.inputChannel->getName().c_str());
            }
        }
    }
}
```

#### InputDispatcher::prepareDispatchCycleLocked

```cpp
void InputDispatcher::prepareDispatchCycleLocked(nsecs_t currentTime,
                                                 const sp<Connection>& connection,
                                                 std::shared_ptr<EventEntry> eventEntry,
                                                 const InputTarget& inputTarget) {
	...
    // Skip this event if the connection status is not normal.
    // We don't want to enqueue additional outbound events if the connection is broken.
	// 如果目标窗口的连接状态不正常，比如进程已经死了，那么就不向它分发了
    if (connection->status != Connection::Status::NORMAL) {
        return;
    }

    // Split a motion event if needed.
    // 如果当前系统处于分屏的状态，就需要考虑分离触摸的情形，检查目标窗口是否设置分离触摸标志
    if (inputTarget.flags & InputTarget::FLAG_SPLIT) {
        const MotionEntry& originalMotionEntry = static_cast<const MotionEntry&>(*eventEntry);
        if (inputTarget.pointerIds.count() != originalMotionEntry.pointerCount) {
            std::unique_ptr<MotionEntry> splitMotionEntry =
                    splitMotionEvent(originalMotionEntry, inputTarget.pointerIds);
            if (!splitMotionEntry) {
                return; // split event was dropped
            }
            if (splitMotionEntry->action == AMOTION_EVENT_ACTION_CANCEL) {
                std::string reason = std::string("reason=pointer cancel on split window");
                android_log_event_list(LOGTAG_INPUT_CANCEL)
                        << connection->getInputChannelName().c_str() << reason << LOG_ID_EVENTS;
            }

            enqueueDispatchEntriesLocked(currentTime, connection, std::move(splitMotionEntry),
                                         inputTarget);
            return;
        }
    }

    // Not splitting.  Enqueue dispatch entries for the event as is.
    //一般走这里，继续事件的分发，将事件加入目标窗口的即将分发的队列中
    enqueueDispatchEntriesLocked(currentTime, connection, eventEntry, inputTarget);
}
```

#### InputDispatcher::enqueueDispatchEntriesLocked

```cpp
void InputDispatcher::enqueueDispatchEntriesLocked(nsecs_t currentTime,
                                                   const sp<Connection>& connection,
                                                   std::shared_ptr<EventEntry> eventEntry,
                                                   const InputTarget& inputTarget) {

    bool wasEmpty = connection->outboundQueue.empty();

    // Enqueue dispatch entries for the requested modes.
    // 将事件加入分发队列outboundQueue，这里有些复杂
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
                               InputTarget::FLAG_DISPATCH_AS_HOVER_EXIT);  //2048
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
                               InputTarget::FLAG_DISPATCH_AS_OUTSIDE);  //512
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
                               InputTarget::FLAG_DISPATCH_AS_HOVER_ENTER);  //1024
    //FLAG_DISPATCH_AS_IS表示事件按原样分发，不改变事件的分发模式，一般都是走这里，其他的是特殊情形
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
                               InputTarget::FLAG_DISPATCH_AS_IS);  //256  
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
                               InputTarget::FLAG_DISPATCH_AS_SLIPPERY_EXIT);  //4096
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
                               InputTarget::FLAG_DISPATCH_AS_SLIPPERY_ENTER);  //8192

    // If the outbound queue was previously empty, start the dispatch cycle going.
    if (wasEmpty && !connection->outboundQueue.empty()) {
        startDispatchCycleLocked(currentTime, connection);  //outboundQueue中已经有事件，可以分发
    }
}
```

#### InputDispatcher::enqueueDispatchEntryLocked

```cpp
void InputDispatcher::enqueueDispatchEntryLocked(const sp<Connection>& connection,
                                                 std::shared_ptr<EventEntry> eventEntry,
                                                 const InputTarget& inputTarget,
                                                 int32_t dispatchMode) {
                                                 
    int32_t inputTargetFlags = inputTarget.flags;
	//这里很关键，enqueueDispatchEntryLocked从上面来看，貌似会调五次，但是
	// 这里有限制条件，根据inputTargetFlags是否等于本次传入的dispatchMode，不同则return
	// 所以是否入队还是由inputTargetFlags决定，正常事件都是inputTargetFlags = FLAG_DISPATCH_AS_IS
	// 所以一般只有dispatchMode = FLAG_DISPATCH_AS_IS时才会往下执行
    if (!(inputTargetFlags & dispatchMode)) {
        return;
    }
    inputTargetFlags = (inputTargetFlags & ~InputTarget::FLAG_DISPATCH_MASK) | dispatchMode;

    // This is a new event.
    // Enqueue a new dispatch entry onto the outbound queue for this connection.
    // 创建DispatchEntry，DispatchEntry是对EventEntry的包装，加入分发的一些跟踪状态属性值
    // 关联eventEntry，inputTarget，inputTarget，
    std::unique_ptr<DispatchEntry> dispatchEntry =
            createDispatchEntry(inputTarget, eventEntry, inputTarget);

    // Use the eventEntry from dispatchEntry since the entry may have changed and can now be a
    // different EventEntry than what was passed in.
    EventEntry& newEntry = *(dispatchEntry->eventEntry);
    // Apply target flags and update the connection's input state.
    switch (newEntry.type) {
        case EventEntry::Type::KEY: {
			...
            break;
        }

        case EventEntry::Type::MOTION: {
            const MotionEntry& motionEntry = static_cast<const MotionEntry&>(newEntry);
            // Assign a default value to dispatchEntry that will never be generated by InputReader,
            // and assign a InputDispatcher value if it doesn't change in the if-else chain below.
            constexpr int32_t DEFAULT_RESOLVED_EVENT_ID =
                    static_cast<int32_t>(IdGenerator::Source::OTHER);
            dispatchEntry->resolvedEventId = DEFAULT_RESOLVED_EVENT_ID;
            
            if (dispatchMode & InputTarget::FLAG_DISPATCH_AS_OUTSIDE) {
                dispatchEntry->resolvedAction = AMOTION_EVENT_ACTION_OUTSIDE;
            } else if (dispatchMode & InputTarget::FLAG_DISPATCH_AS_HOVER_EXIT) {
                dispatchEntry->resolvedAction = AMOTION_EVENT_ACTION_HOVER_EXIT;
            } else if (dispatchMode & InputTarget::FLAG_DISPATCH_AS_HOVER_ENTER) {
                dispatchEntry->resolvedAction = AMOTION_EVENT_ACTION_HOVER_ENTER;
            } else if (dispatchMode & InputTarget::FLAG_DISPATCH_AS_SLIPPERY_EXIT) {
                dispatchEntry->resolvedAction = AMOTION_EVENT_ACTION_CANCEL;
            } else if (dispatchMode & InputTarget::FLAG_DISPATCH_AS_SLIPPERY_ENTER) {
                dispatchEntry->resolvedAction = AMOTION_EVENT_ACTION_DOWN;
            } else {  //由上面可知dispatchMode = FLAG_DISPATCH_AS_IS所以走else，保留事件原本的action和id
                dispatchEntry->resolvedAction = motionEntry.action;
                dispatchEntry->resolvedEventId = motionEntry.id;
            }
            
            if (dispatchEntry->resolvedAction == AMOTION_EVENT_ACTION_HOVER_MOVE &&
				...
            }

            dispatchEntry->resolvedFlags = motionEntry.flags;
            //判断当前目标窗口是否被其他可见窗口遮挡的情况并设置到dispatchEntry->resolvedFlags, 全遮挡还是部分遮挡
            if (dispatchEntry->targetFlags & InputTarget::FLAG_WINDOW_IS_OBSCURED) {
                dispatchEntry->resolvedFlags |= AMOTION_EVENT_FLAG_WINDOW_IS_OBSCURED;
            }
            if (dispatchEntry->targetFlags & InputTarget::FLAG_WINDOW_IS_PARTIALLY_OBSCURED) {
                dispatchEntry->resolvedFlags |= AMOTION_EVENT_FLAG_WINDOW_IS_PARTIALLY_OBSCURED;
            }
			...
			// 从上面可知，当dispatchMode = FLAG_DISPATCH_AS_IS时
			// 事件的resolvedEventId仍然保留原有motionEntry.id
            dispatchEntry->resolvedEventId =
                    dispatchEntry->resolvedEventId == DEFAULT_RESOLVED_EVENT_ID
                    ? mIdGenerator.nextId()
                    : motionEntry.id;

            if ((motionEntry.flags & AMOTION_EVENT_FLAG_NO_FOCUS_CHANGE) &&
                (motionEntry.policyFlags & POLICY_FLAG_TRUSTED)) {
                // Skip reporting pointer down outside focus to the policy.
                break;
            }
			//处理分发事件目标窗口不是当前焦点窗口的情况
            dispatchPointerDownOutsideFocus(motionEntry.source, dispatchEntry->resolvedAction,
                                            inputTarget.inputChannel->getConnectionToken());

            break;
        }
		...
    }

    // Remember that we are waiting for this dispatch to complete.
    if (dispatchEntry->hasForegroundTarget()) {
        incrementPendingForegroundDispatches(newEntry);
    }

    // Enqueue the dispatch entry.
    // 将dispatchEntry加入当前目标窗口的outboundQueue
    connection->outboundQueue.push_back(dispatchEntry.release());
    traceOutboundQueueLength(*connection);  //systrace可以看到outboundQueue的size发生改变
}
```

#### InputDispatcher::startDispatchCycleLocked

```cpp
void InputDispatcher::startDispatchCycleLocked(nsecs_t currentTime,
                                               const sp<Connection>& connection) {

	//循环从outboundQueue中取出事件通过目标窗口的inputchannel向目标窗口分发
    while (connection->status == Connection::Status::NORMAL && !connection->outboundQueue.empty()) {
        DispatchEntry* dispatchEntry = connection->outboundQueue.front();  //取出DispatchEntry
        dispatchEntry->deliveryTime = currentTime;  //记录分发事件
        const std::chrono::nanoseconds timeout = getDispatchingTimeoutLocked(connection);
         //事件处理的超时时间点 = currentTime + 5s  超时会引发ANR
        dispatchEntry->timeoutTime = currentTime + timeout.count();

        // Publish the event.
        status_t status;
        const EventEntry& eventEntry = *(dispatchEntry->eventEntry);  //获取EventEntry
        switch (eventEntry.type) {
            case EventEntry::Type::KEY: {
				...
            }

            case EventEntry::Type::MOTION: {
                const MotionEntry& motionEntry = static_cast<const MotionEntry&>(eventEntry);  //转化为触摸对应MotionEntry

                PointerCoords scaledCoords[MAX_POINTERS];
                const PointerCoords* usingCoords = motionEntry.pointerCoords;

                // Set the X and Y offset and X and Y scale depending on the input source.
                //根据输入源设置 X 和 Y 偏移以及 X 和 Y 比例。
                if ((motionEntry.source & AINPUT_SOURCE_CLASS_POINTER) &&
                    !(dispatchEntry->targetFlags & InputTarget::FLAG_ZERO_COORDS)) {  //模拟器走这里
                    float globalScaleFactor = dispatchEntry->globalScaleFactor;
                    if (globalScaleFactor != 1.0f) {
                        for (uint32_t i = 0; i < motionEntry.pointerCount; i++) {
                            scaledCoords[i] = motionEntry.pointerCoords[i];
                            // Don't apply window scale here since we don't want scale to affect raw
                            // coordinates. The scale will be sent back to the client and applied
                            // later when requesting relative coordinates.
                            //如果当前窗口的全局坐标比例不是1.0f,那么这里先把每个pointer的比例设置为1.0f
                            scaledCoords[i].scale(globalScaleFactor, 1 /* windowXScale */,
                                                  1 /* windowYScale */);
                        }
                        usingCoords = scaledCoords;
                    }
                } else {
					...
                }

                std::array<uint8_t, 32> hmac = getSignature(motionEntry, *dispatchEntry);

                // Publish the motion event.
                // 向目标窗口分发触摸事件
                status = connection->inputPublisher
                                 .publishMotionEvent(dispatchEntry->seq,
                                                     dispatchEntry->resolvedEventId,
                                                     motionEntry.deviceId, motionEntry.source,
                                                     motionEntry.displayId, std::move(hmac),
                                                     dispatchEntry->resolvedAction,
                                                     motionEntry.actionButton,
                                                     dispatchEntry->resolvedFlags,
                                                     motionEntry.edgeFlags, motionEntry.metaState,
                                                     motionEntry.buttonState,
                                                     motionEntry.classification,
                                                     dispatchEntry->transform,
                                                     motionEntry.xPrecision, motionEntry.yPrecision,
                                                     motionEntry.xCursorPosition,
                                                     motionEntry.yCursorPosition,
                                                     dispatchEntry->rawTransform,
                                                     motionEntry.downTime, motionEntry.eventTime,
                                                     motionEntry.pointerCount,
                                                     motionEntry.pointerProperties, usingCoords);
                break;
            }
			...
        }

        // Check the result.
        // 检测分发的结果，正常时status = 0
        if (status) {
            if (status == WOULD_BLOCK) {
                if (connection->waitQueue.empty()) {
                	// 当前waitQueue是空的，说明socket中也应该是空的，但是却是WOULD_BLOCK，说明这时一个异常的情况，中断分发
                    abortBrokenDispatchCycleLocked(currentTime, connection, true /*notify*/);
                } else {
                    // Pipe is full and we are waiting for the app to finish process some events
                    // before sending more events to it.
                    // socket满了，等待应用进程处理掉一些事件
                }
            } else {
            	//其他异常的情况
                abortBrokenDispatchCycleLocked(currentTime, connection, true /*notify*/);
            }
            return;
        }

        // Re-enqueue the event on the wait queue.
        // 将已经分发的事件dispatchEntry从outboundQueue中移除
        connection->outboundQueue.erase(std::remove(connection->outboundQueue.begin(),
                                                    connection->outboundQueue.end(),
                                                    dispatchEntry));
        traceOutboundQueueLength(*connection); //systrace中跟踪outboundQueue的长度
        
        // 将已经分发的事件dispatchEntry加入目标窗口waitQueue中，记录下已经分发到目标窗口侧的事件，便于监控ANR等行为
        connection->waitQueue.push_back(dispatchEntry);

	  //如果目标窗口进程（例如应用进程）可响应，则将这个事件超时事件点和目标窗口连接对象token加入mAnrTracker中监控
	  //如果不可响应，则不再向它分发更多的事件，直到它消耗了已经分发给它的事件
        if (connection->responsive) {
            mAnrTracker.insert(dispatchEntry->timeoutTime,
                               connection->inputChannel->getConnectionToken());
        }
        traceWaitQueueLength(*connection); //systrace中跟踪waitQueue的长度
    }
}
```

#### InputPublisher::publishMotionEvent

frameworks/native/libs/input/InputTransport.cpp

```cpp
status_t InputPublisher::publishMotionEvent(
        uint32_t seq, int32_t eventId, int32_t deviceId, int32_t source, int32_t displayId,
        std::array<uint8_t, 32> hmac, int32_t action, int32_t actionButton, int32_t flags,
        int32_t edgeFlags, int32_t metaState, int32_t buttonState,
        MotionClassification classification, const ui::Transform& transform, float xPrecision,
        float yPrecision, float xCursorPosition, float yCursorPosition,
        const ui::Transform& rawTransform, nsecs_t downTime, nsecs_t eventTime,
        uint32_t pointerCount, const PointerProperties* pointerProperties,
        const PointerCoords* pointerCoords) {

	//创建InputMessage，将dispatchEntry转化为InputMessage
    InputMessage msg;
    msg.header.type = InputMessage::Type::MOTION;
    msg.header.seq = seq;
    msg.body.motion.eventId = eventId;
    msg.body.motion.deviceId = deviceId;
    msg.body.motion.source = source;
    msg.body.motion.displayId = displayId;
    msg.body.motion.hmac = std::move(hmac);
    msg.body.motion.action = action;
    msg.body.motion.actionButton = actionButton;
    msg.body.motion.flags = flags;
    msg.body.motion.edgeFlags = edgeFlags;
    msg.body.motion.metaState = metaState;
    msg.body.motion.buttonState = buttonState;
    msg.body.motion.classification = classification;
    msg.body.motion.dsdx = transform.dsdx();
    msg.body.motion.dtdx = transform.dtdx();
    msg.body.motion.dtdy = transform.dtdy();
    msg.body.motion.dsdy = transform.dsdy();
    msg.body.motion.tx = transform.tx();
    msg.body.motion.ty = transform.ty();
    msg.body.motion.xPrecision = xPrecision;
    msg.body.motion.yPrecision = yPrecision;
    msg.body.motion.xCursorPosition = xCursorPosition;
    msg.body.motion.yCursorPosition = yCursorPosition;
    msg.body.motion.dsdxRaw = rawTransform.dsdx();
    msg.body.motion.dtdxRaw = rawTransform.dtdx();
    msg.body.motion.dtdyRaw = rawTransform.dtdy();
    msg.body.motion.dsdyRaw = rawTransform.dsdy();
    msg.body.motion.txRaw = rawTransform.tx();
    msg.body.motion.tyRaw = rawTransform.ty();
    msg.body.motion.downTime = downTime;
    msg.body.motion.eventTime = eventTime;
    msg.body.motion.pointerCount = pointerCount;
    for (uint32_t i = 0; i < pointerCount; i++) { //多指的情况
        msg.body.motion.pointers[i].properties.copyFrom(pointerProperties[i]);
        msg.body.motion.pointers[i].coords.copyFrom(pointerCoords[i]);
    }

    return mChannel->sendMessage(&msg);  //通过socketpair传递到目标窗口所在的进程中
}
```

到这里InputDispatcher的分发就分析完了。