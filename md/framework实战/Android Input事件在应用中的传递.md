

> hongxi.zhu 2023-4-25

前面我们已经梳理了input事件在native层的传递，这一篇我们接着探索input事件在应用中的传递与处理，我们将按键事件和触摸事件分开梳理，这一篇就只涉及按键事件。

#### 一、事件的接收

从前面的篇幅我们知道，framework native层`InputDispatcher`向应用通过socket方式发送事件,应用的`Looper` 通过epoll方式监听sockcet的fd, 当应用的socket变为可读时（例如，它有可读事件），`Looper`将回调`handleEvent`。 此时，应用应读取已进入套接字的事件。 只要socket中有未读事件，函数 handleEvent 就会继续触发。(这个event不是真正的输入事件，只是Looper的状态event)

```cpp
//frameworks/base/core/jni/android_view_InputEventReceiver.cpp

int NativeInputEventReceiver::handleEvent(int receiveFd, int events, void* data) {
    // Allowed return values of this function as documented in LooperCallback::handleEvent
    constexpr int REMOVE_CALLBACK = 0;
    constexpr int KEEP_CALLBACK = 1;
    //注意：下面这个event不是真正的输入事件，只是Looper的状态event
    if (events & (ALOOPER_EVENT_ERROR | ALOOPER_EVENT_HANGUP)) {
        //当inputdispatcher异常导致socket被关闭或者目标窗口正在被移除或者传递窗口时输入法，但是输入法正在关闭时会直接抛弃这个事件
        // This error typically occurs when the publisher has closed the input channel
        // as part of removing a window or finishing an IME session, in which case
        // the consumer will soon be disposed as well.
        if (kDebugDispatchCycle) {
            ALOGD("channel '%s' ~ Publisher closed input channel or an error occurred. events=0x%x",
                  getInputChannelName().c_str(), events);
        }
        return REMOVE_CALLBACK;
    }
    //如果是输入事件，即是framework传递过来的事件时需要处理时
    if (events & ALOOPER_EVENT_INPUT) {
        JNIEnv* env = AndroidRuntime::getJNIEnv();
        status_t status = consumeEvents(env, false /*consumeBatches*/, -1, nullptr);
        mMessageQueue->raiseAndClearException(env, "handleReceiveCallback");
        return status == OK || status == NO_MEMORY ? KEEP_CALLBACK : REMOVE_CALLBACK;
    }
    //如果已处理的事件需要告知inputdispatcher这个事件已处理时
    if (events & ALOOPER_EVENT_OUTPUT) {
        const status_t status = processOutboundEvents();
        if (status == OK || status == WOULD_BLOCK) {
            return KEEP_CALLBACK;
        } else {
            return REMOVE_CALLBACK;
        }
    }

    ALOGW("channel '%s' ~ Received spurious callback for unhandled poll event.  events=0x%x",
          getInputChannelName().c_str(), events);
    return KEEP_CALLBACK;
}
```

`handleEvent`区分是本次Looper获取到的event, 是需要系统处理接收输入事件，还是需要回复给InputDispatcher事件处理结束的event，如果是需要处理的输入事件，就调用`consumeEvents`消费这个事件。(注意：这个event不是真正的输入事件，只是Looper的状态case)

```cpp
//frameworks/base/core/jni/android_view_InputEventReceiver.cpp

status_t NativeInputEventReceiver::consumeEvents(JNIEnv* env,
        bool consumeBatches, nsecs_t frameTime, bool* outConsumedBatch) {
    ...

    ScopedLocalRef<jobject> receiverObj(env, nullptr);
    bool skipCallbacks = false;
    for (;;) {
        uint32_t seq;
        InputEvent* inputEvent;
        //真正的去获取socket发过来的事件，并构建成具体的某种InputEvent，例如KeyEvent
        status_t status = mInputConsumer.consume(&mInputEventFactory,
                consumeBatches, frameTime, &seq, &inputEvent);
        if (status != OK && status != WOULD_BLOCK) {
            ALOGE("channel '%s' ~ Failed to consume input event.  status=%s(%d)",
                  getInputChannelName().c_str(), statusToString(status).c_str(), status);
            return status;
        }
        ...
```

`consumeEvents`中我们才开始真正的拿着对应的socket fd去读取socket的msg, 具体读取会调用`InputConsumer::consume`

```cpp
//frameworks/native/libs/input/InputTransport.cpp

status_t InputConsumer::consume(InputEventFactoryInterface* factory, bool consumeBatches,
                                nsecs_t frameTime, uint32_t* outSeq, InputEvent** outEvent) {
    ...
    *outSeq = 0;
    *outEvent = nullptr;

    // Fetch the next input message.
    // Loop until an event can be returned or no additional events are received.
    while (!*outEvent) {  //获取到一次真正的事件就退出
        if (mMsgDeferred) {
            ...
        } else {
            // Receive a fresh message.
            status_t result = mChannel->receiveMessage(&mMsg);  //通过InputChannel来接收socket中真正的InputMessage(描述事件的结构体)
            ...
        }
        ...
        }
    }
    return OK;
}
```

`InputConsumer::consume`中获取事件实际上是通过InputChannel去读取

```cpp
////frameworks/native/libs/input/InputTransport.cpp

status_t InputChannel::receiveMessage(InputMessage* msg) {
    ssize_t nRead;
    do {
        nRead = ::recv(getFd(), msg, sizeof(InputMessage), MSG_DONTWAIT); //在这里真正的读取socket fd,并将输入事件信息装入msg（InputMessage)
    } while (nRead == -1 && errno == EINTR);
    
    ...
    
    return OK;  //最后返回OK
}
```

通过`InputChannel`去读取真正的事件信息，并装入InputMessage对象，最后返回OK

```cpp
//frameworks/native/libs/input/InputTransport.cpp

status_t InputConsumer::consume(InputEventFactoryInterface* factory, bool consumeBatches,
                                nsecs_t frameTime, uint32_t* outSeq, InputEvent** outEvent) {
    ...
    *outSeq = 0;
    *outEvent = nullptr;

    // Fetch the next input message.
    // Loop until an event can be returned or no additional events are received.
    while (!*outEvent) {  //获取到一次真正的事件就退出
        if (mMsgDeferred) {
            // mMsg contains a valid input message from the previous call to consume
            // that has not yet been processed.
            mMsgDeferred = false;
        } else {
            // Receive a fresh message.
            status_t result = mChannel->receiveMessage(&mMsg);  //通过InputChannel来接收socket中真正的InputMessage(描述事件的结构体)
            if (result == OK) {
                mConsumeTimes.emplace(mMsg.header.seq, systemTime(SYSTEM_TIME_MONOTONIC));
            }
            if (result) {  //result = OK = 0 ，所以并不会进入批处理流程
                // Consume the next batched event unless batches are being held for later.
                if (consumeBatches || result != WOULD_BLOCK) {
                    result = consumeBatch(factory, frameTime, outSeq, outEvent);
                    if (*outEvent) {
                        if (DEBUG_TRANSPORT_ACTIONS) {
                            ALOGD("channel '%s' consumer ~ consumed batch event, seq=%u",
                                  mChannel->getName().c_str(), *outSeq);
                        }
                        break;
                    }
                }
                return result;
            }
        }

        switch (mMsg.header.type) {
            case InputMessage::Type::KEY: {
                KeyEvent* keyEvent = factory->createKeyEvent();  //创建KeyEvent
                if (!keyEvent) return NO_MEMORY;

                initializeKeyEvent(keyEvent, &mMsg);  //将InputMessage信息填充到keyEvent
                *outSeq = mMsg.header.seq;
                *outEvent = keyEvent;  // 返回到上一级使用（keyEvent 继承于InputEvent）
                if (DEBUG_TRANSPORT_ACTIONS) {
                    ALOGD("channel '%s' consumer ~ consumed key event, seq=%u",
                          mChannel->getName().c_str(), *outSeq);
                }
            break;
            }
            ...
        }
    }
    return OK;
}
```

回到上一级，`InputConsumer::consume`中,`receiveMessage`中获取到的InputMessage在这里转化为对应的event类型，对应的按键事件就是KeyEvent，`*outEvent = keyEvent`返回到前面的`NativeInputEventReceiver::consumeEvents`中

```cpp
//frameworks/base/core/jni/android_view_InputEventReceiver.cpp

status_t NativeInputEventReceiver::consumeEvents(JNIEnv* env,
        bool consumeBatches, nsecs_t frameTime, bool* outConsumedBatch) {
    ...
    for (;;) {
        uint32_t seq;
        InputEvent* inputEvent;
        //真正的去获取socket发过来的事件，并构建成具体的某种InputEvent，例如KeyEvent
        status_t status = mInputConsumer.consume(&mInputEventFactory,
                consumeBatches, frameTime, &seq, &inputEvent);
        ...
        if (!skipCallbacks) {
            jobject inputEventObj;
            switch (inputEvent->getType()) {
            case AINPUT_EVENT_TYPE_KEY:
                if (kDebugDispatchCycle) {
                    ALOGD("channel '%s' ~ Received key event.", getInputChannelName().c_str());
                }
                inputEventObj = android_view_KeyEvent_fromNative(env,
                        static_cast<KeyEvent*>(inputEvent));  //将consume()中拿到的inputEvent转为相应的KeyEvent（在内部我们创建的就是KeyEvent）
                break;
            ...
            if (inputEventObj) {
                ...
                env->CallVoidMethod(receiverObj.get(),
                        gInputEventReceiverClassInfo.dispatchInputEvent, seq, inputEventObj);  //jni调用InputEventReceiver.java中的InputEventReceiver，将事件传递到java的世界
                ...
                env->DeleteLocalRef(inputEventObj);
            }
            ...
        }
    }
}
```

通过`consume`中拿到`inputEvent`,然后转为`KeyEvent`，最通过Jni的方式调用java中的`InputEventReceiver`的方法`dispatchInputEvent`，正式开始事件的分发。

```java
//frameworks/base/core/java/android/view/InputEventReceiver.java

public abstract class InputEventReceiver {
    ...
    // Called from native code.
    @SuppressWarnings("unused")
    @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.R, trackingBug = 170729553)
    private void dispatchInputEvent(int seq, InputEvent event) {
        mSeqMap.put(event.getSequenceNumber(), seq);
        onInputEvent(event);
    }
    ...
}
```

InputEventReceiver是一个抽象类，但是对应的dispatchInputEvent方法，它的子类WindowInputEventReceiver并没有实现，所以native层调用父类的InputEventReceiver的方法，这个方法中接着调用了onInputEvent接着处理。onInputEvent子类是有实现的，所以会走子类的方法。

```java
//frameworks/base/core/java/android/view/ViewRootImpl.java
    ...
    final class WindowInputEventReceiver extends InputEventReceiver {
        public WindowInputEventReceiver(InputChannel inputChannel, Looper looper) {
            super(inputChannel, looper);
        }

        @Override
        public void onInputEvent(InputEvent event) {
            Trace.traceBegin(Trace.TRACE_TAG_VIEW, "processInputEventForCompatibility");
            List<InputEvent> processedEvents;
            try {
                //对M版本之前的触摸事件的兼容处理，按键事件不涉及, return null
                processedEvents =
                    mInputCompatProcessor.processInputEventForCompatibility(event); 
            } finally {
                Trace.traceEnd(Trace.TRACE_TAG_VIEW);
            }
            if (processedEvents != null) {
                if (processedEvents.isEmpty()) {
                    // InputEvent consumed by mInputCompatProcessor
                    finishInputEvent(event, true);
                } else {
                    for (int i = 0; i < processedEvents.size(); i++) {
                        enqueueInputEvent(
                                processedEvents.get(i), this,
                                QueuedInputEvent.FLAG_MODIFIED_FOR_COMPATIBILITY, true);
                    }
                }
            } else {  //因为上面返回null 所以走到这里
                //在这里将自己this传入
                //processImmediately 为true意味着需要马上处理，而不是延迟处理       
                enqueueInputEvent(event, this, 0, true);
            }
        }
    ...
```

`onInputEvent`中会通过`QueuedInputEvent`的`enqueueInputEvent`将事件加入队列中再处理

```java
//frameworks/base/core/java/android/view/ViewRootImpl.java

    @UnsupportedAppUsage
    void enqueueInputEvent(InputEvent event,
            InputEventReceiver receiver, int flags, boolean processImmediately) {
        QueuedInputEvent q = obtainQueuedInputEvent(event, receiver, flags);  //将事件加入队列，确保事件的有序处理

        if (event instanceof MotionEvent) {
            ...
        } else if (event instanceof KeyEvent) { //如果案件事件是一个key的canceled事件
            KeyEvent ke = (KeyEvent) event;
            if (ke.isCanceled()) {
                EventLog.writeEvent(EventLogTags.VIEW_ENQUEUE_INPUT_EVENT, "Key - Cancel",
                        getTitle().toString());
            }
        }
        // 无论时间戳如何，始终按顺序排列输入事件。
        // 我们这样做是因为应用本身或 IME 可能会注入事件，
        // 我们希望确保注入的按键按照接收到的顺序进行处理，不能仅仅通过时间戳的前后来确定顺序。
        // Always enqueue the input event in order, regardless of its time stamp.
        // We do this because the application or the IME may inject key events
        // in response to touch events and we want to ensure that the injected keys
        // are processed in the order they were received and we cannot trust that
        // the time stamp of injected events are monotonic.
        QueuedInputEvent last = mPendingInputEventTail;
        if (last == null) {
            mPendingInputEventHead = q;
            mPendingInputEventTail = q;
        } else {
            last.mNext = q;
            mPendingInputEventTail = q;
        }
        mPendingInputEventCount += 1;
        Trace.traceCounter(Trace.TRACE_TAG_INPUT, mPendingInputEventQueueLengthCounterName,
                mPendingInputEventCount);

        if (processImmediately) {
            doProcessInputEvents();  //前面传进来的processImmediately = true所以走这里处理
        } else {
            scheduleProcessInputEvents();
        }
    }
```

为什么需要加入队列处理？一般来说，当InputDispatcher发送一个事件给应用，应用需要处理完并反馈给InputDispatcher，后者才会发送下一个事件，本身操作流程就是串行的，看起来是不需要队列来保证串行处理。但是不要忘记，除了来自底层驱动的事件外，应用和IME都是可以往应用进程注入事件的，那么就需要保证处理的顺序。那么下一步就是从队头依次拿出事件来分发了，对应方式是`doProcessInputEvents`

```java
//frameworks/base/core/java/android/view/ViewRootImpl.java

    void doProcessInputEvents() {
        // Deliver all pending input events in the queue.
        while (mPendingInputEventHead != null) {
            QueuedInputEvent q = mPendingInputEventHead;  //从队列中拿出数据分发，确保有序
            mPendingInputEventHead = q.mNext;
            if (mPendingInputEventHead == null) {
                mPendingInputEventTail = null;
            }
            q.mNext = null;

            mPendingInputEventCount -= 1;
            Trace.traceCounter(Trace.TRACE_TAG_INPUT, mPendingInputEventQueueLengthCounterName,
                    mPendingInputEventCount);

            mViewFrameInfo.setInputEvent(mInputEventAssigner.processEvent(q.mEvent));

            deliverInputEvent(q);  //开始分发事件
        }

        // We are done processing all input events that we can process right now
        // so we can clear the pending flag immediately.
        if (mProcessInputEventsScheduled) {
            mProcessInputEventsScheduled = false;
            mHandler.removeMessages(MSG_PROCESS_INPUT_EVENTS);
        }
    }
```

#### 二、事件的传递

前面将事件入队，然后在`doProcessInputEvents`就开始从队头拿出并通过`deliverInputEvent`开始分发

```java
//frameworks/base/core/java/android/view/ViewRootImpl.java

    private void deliverInputEvent(QueuedInputEvent q) {
        Trace.asyncTraceBegin(Trace.TRACE_TAG_VIEW, "deliverInputEvent",
                q.mEvent.getId());
        ...
        try {
            ...
            InputStage stage;
            if (q.shouldSendToSynthesizer()) {
                stage = mSyntheticInputStage;
            } else {
                //如果忽略输入法窗口则从mFirstPostImeInputStage阶段开始分发，否则从mFirstInputStage开始
                stage = q.shouldSkipIme() ? mFirstPostImeInputStage : mFirstInputStage;
            }
            ...
            if (stage != null) {
                handleWindowFocusChanged();  //在分发前确认是否焦点窗口变化了，如果变化就需要更新焦点的信息
                stage.deliver(q);  //调用对应的stage阶段的deliver方法分发事件
            } else {
                finishInputEvent(q);
            }
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_VIEW);
        }
    }
```

把事件从拿出，下一步就是往view或者IME分发，分发的过程这里会分为多个阶段(InputStage)来顺序执行, 这些阶段在ViewRootImpl中setView时会指定

```java
//frameworks/base/core/java/android/view/ViewRootImpl.java

    /**
     * We have one child
     */
    public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView,
            int userId) {
        synchronized (this) {
            if (mView == null) {
            
                ...
                // Set up the input pipeline.
                CharSequence counterSuffix = attrs.getTitle();
                mSyntheticInputStage = new SyntheticInputStage();
                InputStage viewPostImeStage = new ViewPostImeInputStage(mSyntheticInputStage);
                InputStage nativePostImeStage = new NativePostImeInputStage(viewPostImeStage,
                        "aq:native-post-ime:" + counterSuffix);
                InputStage earlyPostImeStage = new EarlyPostImeInputStage(nativePostImeStage);
                InputStage imeStage = new ImeInputStage(earlyPostImeStage,
                        "aq:ime:" + counterSuffix);
                InputStage viewPreImeStage = new ViewPreImeInputStage(imeStage);
                InputStage nativePreImeStage = new NativePreImeInputStage(viewPreImeStage,
                        "aq:native-pre-ime:" + counterSuffix);

                mFirstInputStage = nativePreImeStage;
                mFirstPostImeInputStage = earlyPostImeStage;
                mPendingInputEventQueueLengthCounterName = "aq:pending:" + counterSuffix;
            }
        }
    }
```

`InputStage`这里采用责任链的设计模式，从抽象类`InputStage`内容可以知道，每一个子类都会将next指向下一个stage子类对象

```java
//frameworks/base/core/java/android/view/ViewRootImpl.java

    abstract class InputStage {
        private final InputStage mNext;

        protected static final int FORWARD = 0;
        protected static final int FINISH_HANDLED = 1;
        protected static final int FINISH_NOT_HANDLED = 2;

        private String mTracePrefix;

        /**
         * Creates an input stage.
         * 将所有的阶段都组成一个链表，next指向下一个阶段
         * @param next The next stage to which events should be forwarded.
         */
        public InputStage(InputStage next) {
            mNext = next;
        }
        ...
```

从`setView`方法中的内容，我们得出整个链条的结构

NativePreImeInputStage

ViewPreImeInputStage

ImeInputStage

EarlyPostImeInputStage

NativePostImeInputStage

ViewPostImeInputStage

SyntheticInputStage

分发阶段就会从第一个创建的stage子类开始执行到最后一个stage子类,无论要不要处理，都要从链表的头传递到尾。  
回到`deliverInputEvent`方法中`stage.deliver(q)`正式进入stage的分发中,观察下完整的一个stage的处理流程

```java
//frameworks/base/core/java/android/view/ViewRootImpl.java

        /**
         * Delivers an event to be processed.
         */
        public final void deliver(QueuedInputEvent q) {
            if ((q.mFlags & QueuedInputEvent.FLAG_FINISHED) != 0) {  //如果上一stage中事件被处理（FLAG_FINISHED）那么本stage就不会再处理（onProcess），直接传递到下一个stage(无论是要处理，链表都要走完)
                forward(q);
            } else if (shouldDropInputEvent(q)) {
                finish(q, false);
            } else {
                traceEvent(q, Trace.TRACE_TAG_VIEW);
                final int result;
                try {
                    result = onProcess(q);  //如果前面的阶段没有被处理，本stage就需要走处理流程
                } finally {
                    Trace.traceEnd(Trace.TRACE_TAG_VIEW);
                }
                apply(q, result);  //判断是否需要下一个阶段走处理流程
            }
        }

        /**
         * Marks the input event as finished then forwards it to the next stage.
         *  如果事件在当前阶段被结束，q.mFlags被标记为FLAG_FINISHED，并通过forward(q)传递给下一个阶段
         */
        protected void finish(QueuedInputEvent q, boolean handled) {
            q.mFlags |= QueuedInputEvent.FLAG_FINISHED;
            if (handled) {
                q.mFlags |= QueuedInputEvent.FLAG_FINISHED_HANDLED;
            }
            forward(q);
        }

        /**
         * Forwards the event to the next stage.
         * 往下一个阶段分发
         */
        protected void forward(QueuedInputEvent q) {
            onDeliverToNext(q);// 继续往下一个阶段传递
        }

        /**
         * Applies a result code from {@link #onProcess} to the specified event.
         * 判断是否需要继续接着往下一个阶段分发
         */
        protected void apply(QueuedInputEvent q, int result) {
            if (result == FORWARD) {  //如果上一个阶段还没处理这个事件，则继续往下一个阶段分发处理
                forward(q);
            } else if (result == FINISH_HANDLED) {   //如果事件被处理了，就标记为FLAG_FINISHED|FLAG_FINISHED_HANDLED，然后继续传递给下一个阶段（但不走onProcess()了）
                finish(q, true);
            } else if (result == FINISH_NOT_HANDLED) {  //如果事件没有被处理则标记为FLAG_FINISHED，然后继续传递给下一个阶段（但不走onProcess()了）
                finish(q, false);
            } else {
                throw new IllegalArgumentException("Invalid result: " + result);
            }
        }

        /**
         * Called when an event is ready to be processed.
         * @return A result code indicating how the event was handled.
         */
        protected int onProcess(QueuedInputEvent q) {
            return FORWARD;
        }

        /**
         * Called when an event is being delivered to the next stage.
         * 继续执行下一阶段的deliver
         */
        protected void onDeliverToNext(QueuedInputEvent q) {
            if (DEBUG_INPUT_STAGES) {
                Log.v(mTag, "Done with " + getClass().getSimpleName() + ". " + q);
            }
            if (mNext != null) {
                mNext.deliver(q);  //如果下一阶段不为空就继续执行下一阶段的deliver，继续往下一阶段传递
            } else {
                finishInputEvent(q);
            }
        }
```

具体如流程图：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/2fec8ce2f6c4162d2cc5f93e9fd7851d.png#pic_center)  
从`NativePreImeInputStage`开始deliver，事件经过每一个stage, 如果该事件没有被处理（标记为）`FLAG_FINISHED`或者该事件应该被抛弃`（shouldDropInputEvent`），那么就应该传给本阶段`（stage）`处理`（onProcess）`，按照这个逻辑一直跑完整个链表。

#### 三、View树的分发

在这里阶段里我们本篇比较关心往View树分发的阶段，即`ViewPostImeInputStage`

```java
//frameworks/base/core/java/android/view/ViewRootImpl.java

    /**
     * Delivers post-ime input events to the view hierarchy.
     */
    final class ViewPostImeInputStage extends InputStage {
        public ViewPostImeInputStage(InputStage next) {
            super(next);
        }

		// 子类重写了onProcess方法
        @Override
        protected int onProcess(QueuedInputEvent q) {
            if (q.mEvent instanceof KeyEvent) {
                return processKeyEvent(q);  //如果是按键事件
            } else {
                final int source = q.mEvent.getSource();
                if ((source & InputDevice.SOURCE_CLASS_POINTER) != 0) {
                    return processPointerEvent(q);
                } else if ((source & InputDevice.SOURCE_CLASS_TRACKBALL) != 0) {
                    return processTrackballEvent(q);
                } else {
                    return processGenericMotionEvent(q);
                }
            }
        }
        ...
        private int processKeyEvent(QueuedInputEvent q) {
            final KeyEvent event = (KeyEvent)q.mEvent;

            if (mUnhandledKeyManager.preViewDispatch(event)) {
                return FINISH_HANDLED;
            }

			// 往view树分发事件
            // Deliver the key to the view hierarchy.
            if (mView.dispatchKeyEvent(event)) {  // mView实际上是DecorView, 在addView时添加
                return FINISH_HANDLED;
            }
            ...
        }
```

`ViewPostImeInputStage`的处理在`onProcess`方法，其中最关键的地方就是`mView.dispatchKeyEvent(event)`，`mView`  
实际上是传入的DecorView，具体可以查看应用启动过程流程。通过DecorView的dispatchKeyEvent开始事件在View树的传递。

```java
//frameworks/base/core/java/com/android/internal/policy/DecorView.java

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        final int keyCode = event.getKeyCode();
        final int action = event.getAction();
        final boolean isDown = action == KeyEvent.ACTION_DOWN;

		//快捷键处理
        if (isDown && (event.getRepeatCount() == 0)) {
            // First handle chording of panel key: if a panel key is held
            // but not released, try to execute a shortcut in it.
            if ((mWindow.mPanelChordingKey > 0) && (mWindow.mPanelChordingKey != keyCode)) {
                boolean handled = dispatchKeyShortcutEvent(event);
                if (handled) {
                    return true;
                }
            }

            // If a panel is open, perform a shortcut on it without the
            // chorded panel key
            if ((mWindow.mPreparedPanel != null) && mWindow.mPreparedPanel.isOpen) {
                if (mWindow.performPanelShortcut(mWindow.mPreparedPanel, keyCode, event, 0)) {
                    return true;
                }
            }
        }

		//mWindow是PhoneWindow的实例
        if (!mWindow.isDestroyed()) {
            // 这个cb实际上是Activity对象，(当调Activity的attach方法时， 通过mWindow.setCallback(this)传入)
            final Window.Callback cb = mWindow.getCallback();
            // 因为Activity这里不为null,所以会掉Activity的dispatchKeyEvent
            final boolean handled = cb != null && mFeatureId < 0 ? cb.dispatchKeyEvent(event)
                    : super.dispatchKeyEvent(event);
            if (handled) {
                return true;
            }
        }

        return isDown ? mWindow.onKeyDown(mFeatureId, event.getKeyCode(), event)
                : mWindow.onKeyUp(mFeatureId, event.getKeyCode(), event);
    }
```

这里关键的是这个cb对象，根据应用的启动流程可知，这个cb是当调Activity的attach方法时， 通过`mWindow.setCallback(this)`传入的Activity对象，且不为null,所以事件会传到Activity，调用它的dispatchKeyEvent方法。

```java
//frameworks/base/core/java/android/app/Activity.java

    /**
     * Called to process key events.  You can override this to intercept all
     * key events before they are dispatched to the window.  Be sure to call
     * this implementation for key events that should be handled normally.
     *
     * @param event The key event.
     *
     * @return boolean Return true if this event was consumed.
     */
    public boolean dispatchKeyEvent(KeyEvent event) {
        onUserInteraction();  //通知通知栏进行相应的变化

        // Let action bars open menus in response to the menu key prioritized over
        // the window handling it
        // MENU键优先给ActionBar处理
        final int keyCode = event.getKeyCode();
        if (keyCode == KeyEvent.KEYCODE_MENU &&
                mActionBar != null && mActionBar.onMenuKeyEvent(event)) {
            return true;
        }

		// 这里的getWindow()拿到的是PhoneWindow对象(在Activity的attach方法中创建的)
        Window win = getWindow();
        if (win.superDispatchKeyEvent(event)) {  //这里会先分发给PhoneWindow, 实际上PhoneWindow会调DecorView的superDispatchKeyEvent
            return true;
        }
        View decor = mDecor;
        if (decor == null) decor = win.getDecorView();
        return event.dispatch(this, decor != null
                ? decor.getKeyDispatcherState() : null, this);
    }
```

从这里可以看出，Activity会调PhoneWindow的superDispatchKeyEvent将事件发给PhoneWindow处理

```java
//frameworks/base/core/java/com/android/internal/policy/PhoneWindow.java

    @Override
    public boolean superDispatchKeyEvent(KeyEvent event) {
        return mDecor.superDispatchKeyEvent(event); // 实际上调的是DecorView的方法，让DecorView分发
    }
```

```java
//frameworks/base/core/java/com/android/internal/policy/DecorView.java

/** @hide */
public class DecorView extends FrameLayout implements RootViewSurfaceTaker, WindowCallbacks {
	...

    public boolean superDispatchKeyEvent(KeyEvent event) {
		...
        if (super.dispatchKeyEvent(event)) {
            return true;
        }

        return (getViewRootImpl() != null) && getViewRootImpl().dispatchUnhandledKeyEvent(event);
    }
    ...
}
```

实际上PhoneWindow会调DecorView的superDispatchKeyEvent，最终又回到DecorView, 为什么这样流转呢？仔细观察DecorView是继承于FrameLayout,而FrameLayout继承于ViewGroup，那它就是树中最顶端的ViewGroup, 事件应该从它开始分发。

```java
//frameworks/base/core/java/android/view/ViewGroup.java

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
		...
        if ((mPrivateFlags & (PFLAG_FOCUSED | PFLAG_HAS_BOUNDS))  //是否焦点是自己(ViewGroup)
                == (PFLAG_FOCUSED | PFLAG_HAS_BOUNDS)) {
            if (super.dispatchKeyEvent(event)) {  //调用父类的方法处理，ViewGroup也是继承于View
                return true;
            }
        } else if (mFocused != null && (mFocused.mPrivateFlags & PFLAG_HAS_BOUNDS)  //是否焦点View是自己的子view或者子ViewGroup
                == PFLAG_HAS_BOUNDS) {
            if (mFocused.dispatchKeyEvent(event)) {  //传递给自己子view或者viewgroup处理
                return true;
            }
        }

		...
        return false;
    }
```

这里会根据判断焦点view来决定分发给谁，首先判断自己本身（ViewGroup）是不是焦点View,如果是则调用父类View的dispatchKeyEvent方法处理按键事件；如果焦点view在自己的子view或者子viewgroup上，则继续往下分发。如果是子viewgroup那么和上面流程一样继续判断是否继续往下，如果是子view，就调用view的dispatchKeyEvent处理，所以最终都是View的dispatchKeyEvent处理。

```java
// frameworks/base/core/java/android/view/View.java

    /**
     * Dispatch a key event to the next view on the focus path. This path runs
     * from the top of the view tree down to the currently focused view. If this
     * view has focus, it will dispatch to itself. Otherwise it will dispatch
     * the next node down the focus path. This method also fires any key
     * listeners.
     *
     * @param event The key event to be dispatched.
     * @return True if the event was handled, false otherwise.
     */
    public boolean dispatchKeyEvent(KeyEvent event) {
		...
        // Give any attached key listener a first crack at the event.
        //noinspection SimplifiableIfStatement
        // ListenerInfo是管理各种监听器的类，它持有监听器的实例，例如：OnClickListener、OnKeyListener等
        ListenerInfo li = mListenerInfo;
        // 如果应用注册了监听器mOnKeyListener，那么就优先调用mOnKeyListener.onKey回调
        if (li != null && li.mOnKeyListener != null && (mViewFlags & ENABLED_MASK) == ENABLED
                && li.mOnKeyListener.onKey(this, event.getKeyCode(), event)) {
            return true;
        }
		//如果上述未处理，则默认走KeyEvent的dispatch来处理按键事件
        if (event.dispatch(this, mAttachInfo != null
                ? mAttachInfo.mKeyDispatchState : null, this)) {
            return true;
        }
		...
        return false;
    }
```

如果应用注册了OnKeyListener，那么就先回调OnKey()方法处理。如果没有注册OnKeyListener或者这个方法不return true(被消费)，那么就调KeyEvent的dispatch来继续处理按键事件。

```java
//frameworks/base/core/java/android/view/KeyEvent.java

    /**
     * Deliver this key event to a {@link Callback} interface.  If this is
     * an ACTION_MULTIPLE event and it is not handled, then an attempt will
     * be made to deliver a single normal event.
     *
     * @param receiver The Callback that will be given the event. // 这个Callback receiver可以是view, activity
     * @param state State information retained across events.  //这个DispatcherState 用于描述高级事件策略，比如长按
     * @param target The target of the dispatch, for use in tracking.
     *
     * @return The return value from the Callback method that was called.
     */
    public final boolean dispatch(Callback receiver, DispatcherState state,
            Object target) {
        switch (mAction) {
            case ACTION_DOWN: {
                mFlags &= ~FLAG_START_TRACKING; // 如果是down事件就重置tracking的标志位，这个标志用于跟踪是否是长按事件
                if (DEBUG) Log.v(TAG, "Key down to " + target + " in " + state
                        + ": " + this);
                boolean res = receiver.onKeyDown(mKeyCode, this); // 首先回调view.onKeyDown接口方法
                if (state != null) {
                    if (res && mRepeatCount == 0 && (mFlags&FLAG_START_TRACKING) != 0) {
                        if (DEBUG) Log.v(TAG, "  Start tracking!");
                        state.startTracking(this, target);
                    } else if (isLongPress() && state.isTracking(this)) {
                        try {
                            if (receiver.onKeyLongPress(mKeyCode, this)) {  //回调view.onKeyLongPress接口
                                if (DEBUG) Log.v(TAG, "  Clear from long press!");
                                state.performedLongPress(this); // 如果是长按事件就加入长按的跟踪队列里
                                res = true;
                            }
                        } catch (AbstractMethodError e) {
                        }
                    }
                }
                return res;
            }
            case ACTION_UP:
                if (DEBUG) Log.v(TAG, "Key up to " + target + " in " + state
                        + ": " + this);
                if (state != null) {
                    state.handleUpEvent(this);  // 判断当前事件是不是长按跟踪的事件，并做相应的跟踪的信息更新
                }
                return receiver.onKeyUp(mKeyCode, this); // 最终回调view.onKeyUp接口方法，这个方法会去处理click等事件
			...
        }
        return false;
    }
```

KeyEvent.dispatch根据key的action回调view相关的接口, 同时实现长按的事件跟踪策略和触发对应长按接口。

```java
//frameworks/base/core/java/android/view/View.java

    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (KeyEvent.isConfirmKey(keyCode) && event.hasNoModifiers()) {
            if ((mViewFlags & ENABLED_MASK) == DISABLED) {
                return true;
            }

            if (event.getRepeatCount() == 0) {  //如果该按键是首次按下
                // Long clickable items don't necessarily have to be clickable.
                final boolean clickable = (mViewFlags & CLICKABLE) == CLICKABLE
                        || (mViewFlags & LONG_CLICKABLE) == LONG_CLICKABLE;
                if (clickable || (mViewFlags & TOOLTIP) == TOOLTIP) {
                    // For the purposes of menu anchoring and drawable hotspots,
                    // key events are considered to be at the center of the view.
                    final float x = getWidth() / 2f;
                    final float y = getHeight() / 2f;
                    if (clickable) {
                        setPressed(true, x, y); // 设置按下的状态
                    }
                    checkForLongClick(  //开始长按的检测
                            ViewConfiguration.getLongPressTimeout(),
                            x,
                            y,
                            // This is not a touch gesture -- do not classify it as one.
                            TOUCH_GESTURE_CLASSIFIED__CLASSIFICATION__UNKNOWN_CLASSIFICATION);
                    return true;
                }
            }
        }

        return false;
    }
    ...
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (KeyEvent.isConfirmKey(keyCode) && event.hasNoModifiers()) {
            if ((mViewFlags & ENABLED_MASK) == DISABLED) {
                return true;
            }
            if ((mViewFlags & CLICKABLE) == CLICKABLE && isPressed()) {
                setPressed(false);  //取消按下的状态

                if (!mHasPerformedLongPress) {  // 如果是短按
                    // This is a tap, so remove the longpress check
                    removeLongPressCallback();
                    if (!event.isCanceled()) {
                        return performClickInternal();  //执行click流程
                    }
                }
            }
        }
        return false;
    }
```

上面的onKeyDown和onKeyUp是默认实现，一般需要监听Key事件时，子类都会重写这些方法。我们重点看下Up事件触发Click事件的流程，当本次事件是onKeyUp，最终会调到performClick()方法。

```java
//frameworks/base/core/java/android/view/View.java

    private boolean performClickInternal() {
        // Must notify autofill manager before performing the click actions to avoid scenarios where
        // the app has a click listener that changes the state of views the autofill service might
        // be interested on.
        notifyAutofillManagerOnClick();

        return performClick();
    }
    
    public boolean performClick() {
        // We still need to call this method to handle the cases where performClick() was called
        // externally, instead of through performClickInternal()
        notifyAutofillManagerOnClick();

        final boolean result;
        final ListenerInfo li = mListenerInfo;
        if (li != null && li.mOnClickListener != null) {
            playSoundEffect(SoundEffectConstants.CLICK);  //播放按键音
            li.mOnClickListener.onClick(this);  //回调view的OnClickListener.onClick()
            result = true;
        } else {
            result = false;
        }

        sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_CLICKED); // 发送一个无障碍点击事件

        notifyEnterOrExitForAutoFillIfNeeded(true);

        return result;
    }
```

performClick()就是我们耳熟能详的OnClickListener.onClick()接口了，同时还需要播放按键声等。  
至此，按键在应用的传递就过完了，来个图总结下把。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/083ae7b6b2a2df9baaa680c233456e01.png#pic_center)