> hongxi.zhu 2023-7-21  
> Android 13

前面我们已经梳理了input事件在native层的传递，这一篇我们接着探索input事件在应用中的传递与处理，我们将按键事件和触摸事件分开梳理，这一篇就只涉及触摸事件。

#### 一、事件的接收

从前面的篇幅我们知道，framework native层`InputDispatcher`向应用通过socket方式发送事件,应用的`Looper` 通过epoll方式监听sockcet的fd, 当应用的socket变为可读时（例如，inputDispatcher向socket中写入数据），`Looper`将回调`handleEvent`。 此时，应用应读取已进入套接字的事件。 只要socket中有未读事件，函数 handleEvent 就会继续触发。

##### NativeInputEventReceiver::handleEvent

```cpp
//frameworks/base/core/jni/android_view_InputEventReceiver.cpp

int NativeInputEventReceiver::handleEvent(int receiveFd, int events, void* data) {
    // Allowed return values of this function as documented in LooperCallback::handleEvent
    constexpr int REMOVE_CALLBACK = 0;
    constexpr int KEEP_CALLBACK = 1;
    //注意：下面这个event不是真正的输入事件，只是Looper的event
    if (events & (ALOOPER_EVENT_ERROR | ALOOPER_EVENT_HANGUP)) {
        //当inputdispatcher异常导致socket被关闭或者目标窗口正在被移除或者传递窗口时输入法，但是输入法正在关闭时会直接抛弃这个事件
        return REMOVE_CALLBACK;
    }
    //如果是传入的事件，即是inputDispatcher传递过来的事件时需要处理时
    //回调java层的consumeEvents方法
    if (events & ALOOPER_EVENT_INPUT) {
        JNIEnv* env = AndroidRuntime::getJNIEnv();
        status_t status = consumeEvents(env, false /*consumeBatches*/, -1, nullptr);
        mMessageQueue->raiseAndClearException(env, "handleReceiveCallback");
        return status == OK || status == NO_MEMORY ? KEEP_CALLBACK : REMOVE_CALLBACK;
    }
    //如果是要传出的事件，即已处理的事件需要告知inputdispatcher这个事件已处理时
    if (events & ALOOPER_EVENT_OUTPUT) {
        const status_t status = processOutboundEvents();
        if (status == OK || status == WOULD_BLOCK) {
            return KEEP_CALLBACK;
        } else {
            return REMOVE_CALLBACK;
        }
    }

    return KEEP_CALLBACK;
}
```

这里既会监视`inputDispatcher`发送过来的事件（准确的说应该是`InputPublisher`发过来的）也监视当前进程发送已经消费的事件发给Looper的行为，无论是接收来自`InputPublisher`的事件，还是来自当前进程的事件，都会被looper监听到并回调`handleEvent`。这里就用events中的标志位来区分（`ALOOPER_EVENT_INPUT`和`ALOOPER_EVENT_OUTPUT`）对于接收来自`InputPublisher`的事件则调`consumeEvents`方法处理.

##### NativeInputEventReceiver::consumeEvents

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
        //获取mInputConsumer发过来的事件，并构建成具体的某种InputEvent，例如MotionEvent
        status_t status = mInputConsumer.consume(&mInputEventFactory,
                consumeBatches, frameTime, &seq, &inputEvent);
        if (status != OK && status != WOULD_BLOCK) {
            ALOGE("channel '%s' ~ Failed to consume input event.  status=%s(%d)",
                  getInputChannelName().c_str(), statusToString(status).c_str(), status);
            return status;
        }
        ...
```

##### InputConsumer::consume

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

##### InputChannel::receiveMessage

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

##### InputChannel::receiveMessage

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

通过`InputChannel`去读取真正的事件信息，并装入InputMessage对象，最后返回OK, 然后回到前面的`InputConsumer::consume`

##### InputConsumer::consume

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
				...
            }
        }

        switch (mMsg.header.type) {
        	...
            case InputMessage::Type::MOTION: {
                ssize_t batchIndex = findBatch(mMsg.body.motion.deviceId, mMsg.body.motion.source);
                ALOGD("hongxi.zhu: batchIndex = %zd", batchIndex);
                if (batchIndex >= 0) {  //不进行批处理
					...
                }

                // Start a new batch if needed.
                if (mMsg.body.motion.action == AMOTION_EVENT_ACTION_MOVE ||
					...  //不进行批处理
                }

                MotionEvent* motionEvent = factory->createMotionEvent();  //创建一个MotionEvent
                if (!motionEvent) return NO_MEMORY;

                updateTouchState(mMsg);  //更新触摸的状态，记录当前的触摸信息
                initializeMotionEvent(motionEvent, &mMsg);  //将InputMessager转化为MotionEvent
                *outSeq = mMsg.header.seq;  //事件传输的seq
                *outEvent = motionEvent;
                
                break;
            }
            ...
        }
    }
    return OK;
}
```

创建并使用InputMessager中的事件信息填充MotionEvent，然后继续往下执行 `NativeInputEventReceiver::consumeEvents`

##### NativeInputEventReceiver::consumeEvents

```cpp
//frameworks/base/core/jni/android_view_InputEventReceiver.cpp

status_t NativeInputEventReceiver::consumeEvents(JNIEnv* env,
        bool consumeBatches, nsecs_t frameTime, bool* outConsumedBatch) {
    ...
    for (;;) {
        uint32_t seq;
        InputEvent* inputEvent;
        //真正的去获取socket发过来的事件，并构建成具体的某种InputEvent，例MotionEvent
        status_t status = mInputConsumer.consume(&mInputEventFactory,
                consumeBatches, frameTime, &seq, &inputEvent);
        ...
        if (!skipCallbacks) {
            jobject inputEventObj;
            switch (inputEvent->getType()) {
            case AINPUT_EVENT_TYPE_MOTION: {
                MotionEvent* motionEvent = static_cast<MotionEvent*>(inputEvent);
                if ((motionEvent->getAction() & AMOTION_EVENT_ACTION_MOVE) && outConsumedBatch) {
                    *outConsumedBatch = true;
                }
                //创建一个java层MotionEvent对象
                inputEventObj = android_view_MotionEvent_obtainAsCopy(env, motionEvent);
                break;
                }
            }
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

##### dispatchInputEvent

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

##### onInputEvent

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
                //对M版本之前的触摸事件的兼容处理
                processedEvents =
                    mInputCompatProcessor.processInputEventForCompatibility(event); 
            } finally {
                Trace.traceEnd(Trace.TRACE_TAG_VIEW);
            }
            if (processedEvents != null) {
				...
            } else {  //因为上面返回null 所以走到这里
                //在这里将自己this传入
                //processImmediately 为true意味着需要马上处理，而不是延迟处理       
                enqueueInputEvent(event, this, 0, true);
            }
        }
    ...
```

`onInputEvent`中会通过`QueuedInputEvent`的`enqueueInputEvent`将事件加入队列中再处理

##### enqueueInputEvent

```java
//frameworks/base/core/java/android/view/ViewRootImpl.java

    @UnsupportedAppUsage
    void enqueueInputEvent(InputEvent event,
            InputEventReceiver receiver, int flags, boolean processImmediately) {
        QueuedInputEvent q = obtainQueuedInputEvent(event, receiver, flags);  //将事件加入队列，确保事件的有序处理

        if (event instanceof MotionEvent) {
            MotionEvent me = (MotionEvent) event;
            if (me.getAction() == MotionEvent.ACTION_CANCEL) {
				//inputDispatcher中会根据实际焦点和触摸坐标的关系或者事件是有down无up情形设置ACTION_CANCEL
            }
        } else if (event instanceof KeyEvent) {
			...
        }
        // Always enqueue the input event in order, regardless of its time stamp.
        // We do this because the application or the IME may inject key events
        // in response to touch events and we want to ensure that the injected keys
        // are processed in the order they were received and we cannot trust that
        // the time stamp of injected events are monotonic.
        // 无论时间戳如何，始终按顺序排列输入事件。
        // 我们这样做是因为应用程序或 IME 可能会注入按键事件以响应触摸事件，
        // 我们希望确保注入的按键按照接收到的顺序进行处理，不能仅仅通过时间戳的前后来确定顺序。
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

##### doProcessInputEvents

```java
//frameworks/base/core/java/android/view/ViewRootImpl.java

    void doProcessInputEvents() {
        // Deliver all pending input events in the queue.
        while (mPendingInputEventHead != null) {
            QueuedInputEvent q = mPendingInputEventHead;  //从队头开始处理
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
        //已经处理完所有待办的输入事件，是时候移除主线程mHandler中的MSG_PROCESS_INPUT_EVENTS消息了
        if (mProcessInputEventsScheduled) {
            mProcessInputEventsScheduled = false;
            mHandler.removeMessages(MSG_PROCESS_INPUT_EVENTS);
        }
    }
```

#### 二、事件的传递

前面将事件入队，然后在`doProcessInputEvents`就开始从队头拿出并通过`deliverInputEvent`开始分发

##### deliverInputEvent

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

##### setView

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

##### InputStage

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

##### ViewPostImeInputStage

```java
    /**
     * Delivers post-ime input events to the view hierarchy.
     */
    final class ViewPostImeInputStage extends InputStage {
        public ViewPostImeInputStage(InputStage next) {
            super(next);
        }

        @Override
        protected int onProcess(QueuedInputEvent q) {
            if (q.mEvent instanceof KeyEvent) {
                return processKeyEvent(q);  //按键事件
            } else {
                final int source = q.mEvent.getSource();
                if ((source & InputDevice.SOURCE_CLASS_POINTER) != 0) {
                    return processPointerEvent(q);  //pointer类型（包含触摸事件）
                } else if ((source & InputDevice.SOURCE_CLASS_TRACKBALL) != 0) {
                    return processTrackballEvent(q);  //轨迹球
                } else {
                    return processGenericMotionEvent(q);  //其他
                }
            }
        }

        @Override
        protected void onDeliverToNext(QueuedInputEvent q) {
			...
            super.onDeliverToNext(q);
        }

        private int processPointerEvent(QueuedInputEvent q) {
            final MotionEvent event = (MotionEvent)q.mEvent;
            mHandwritingInitiator.onTouchEvent(event);

            mAttachInfo.mUnbufferedDispatchRequested = false;
            mAttachInfo.mHandlingPointerEvent = true;
            
            boolean handled = mView.dispatchPointerEvent(event);  //mView实际上是DecorView, 在addView时添加
            
            maybeUpdatePointerIcon(event);
            maybeUpdateTooltip(event);
            mAttachInfo.mHandlingPointerEvent = false;
            if (mAttachInfo.mUnbufferedDispatchRequested && !mUnbufferedInputDispatch) {
                mUnbufferedInputDispatch = true;
                if (mConsumeBatchedInputScheduled) {
                    scheduleConsumeBatchedInputImmediately();
                }
            }
            return handled ? FINISH_HANDLED : FORWARD;
        }
        ...
 }
```

回顾下继承关系：DecorView->FrameLayout->ViewGroup->View  
`dispatchPointerEvent`方法并没有被`DecorView->FrameLayout->ViewGroup`实现，是祖父类View实现了这个方法

##### View::dispatchPointerEvent

```java
    public final boolean dispatchPointerEvent(MotionEvent event) {
        if (event.isTouchEvent()) {  //根据MotionEvent事件的类型，如果是触摸事件
            return dispatchTouchEvent(event);  //此时是this对象是DecorView子类对象，所以调的是它的dispatchTouchEvent
        } else {  //其他（如鼠标）
            return dispatchGenericMotionEvent(event);
        }
    }
```

##### DecorView::dispatchTouchEvent

```java
    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        final Window.Callback cb = mWindow.getCallback();
        //这个cb实际上是Activity对象，(当调Activity的attach方法时， 通过mWindow.setCallback(this)传入)
        //这里判断条件为真，走cb.dispatchTouchEvent(ev)，也就是Activity的方法
        return cb != null && !mWindow.isDestroyed() && mFeatureId < 0
                ? cb.dispatchTouchEvent(ev) : super.dispatchTouchEvent(ev);
    }
```

##### Activity::dispatchTouchEvent

```java
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_DOWN) {
            onUserInteraction(); //按下时通知通知栏进行相应的变化
        }
        //getWindow获取到的是PhoneWindow对象(在Activity的attach方法中创建的)
        //所以这里会传递给PhoneWindow::superDispatchTouchEvent
        if (getWindow().superDispatchTouchEvent(ev)) {
            return true;
        }
        //如果上面的链路都没人处理，一路都是返回的false，那么最终还是由Activity来消费
        return onTouchEvent(ev);
    }
```

##### PhoneWindow::superDispatchTouchEvent

```java
    @Override
    public boolean superDispatchTouchEvent(MotionEvent event) {
    	// 兜兜转转又到DecorView手里，原因就是它是View树的最顶部ViewGroup呀，还是得从它开始的
        return mDecor.superDispatchTouchEvent(event);
    }
```

##### DecorView::superDispatchTouchEvent

```java
    public boolean superDispatchTouchEvent(MotionEvent event) {
    	//调用父类ViewGroup的dispatchTouchEvent开始遍历子成员分发
        return super.dispatchTouchEvent(event);
    }
```

##### ViewGroup::dispatchTouchEvent

```java
    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
		...

        boolean handled = false;
        if (onFilterTouchEventForSecurity(ev)) {
            final int action = ev.getAction();
            final int actionMasked = action & MotionEvent.ACTION_MASK;

            // Handle an initial down.
            //如果这是个ACTION_DOWN事件说明是一个新触摸行为的开始，那么重置相关的状态
            if (actionMasked == MotionEvent.ACTION_DOWN) {
                // Throw away all previous state when starting a new touch gesture.
                // The framework may have dropped the up or cancel event for the previous gesture
                // due to an app switch, ANR, or some other state change.
                cancelAndClearTouchTargets(ev);
                resetTouchState();
            }

            // Check for interception.
            //ViewGroup是否拦截当前事件，通过onInterceptTouchEvent方法。这个方法只有ViewGroup有
            final boolean intercepted;
            if (actionMasked == MotionEvent.ACTION_DOWN
                    || mFirstTouchTarget != null) {
                final boolean disallowIntercept = (mGroupFlags & FLAG_DISALLOW_INTERCEPT) != 0;
                if (!disallowIntercept) {
                    intercepted = onInterceptTouchEvent(ev);  //
                    ev.setAction(action); // restore action in case it was changed
                } else {
                    intercepted = false;
                }
            } else {
                // There are no touch targets and this action is not an initial down
                // so this view group continues to intercept touches.
                intercepted = true;
            }
            
			...
			
            if (!canceled && !intercepted) {
				//当这时一个ACTION_DOWN事件进这里
                if (actionMasked == MotionEvent.ACTION_DOWN
                        || (split && actionMasked == MotionEvent.ACTION_POINTER_DOWN)
                        || actionMasked == MotionEvent.ACTION_HOVER_MOVE) {
                    final int actionIndex = ev.getActionIndex(); // always 0 for down
                    final int idBitsToAssign = split ? 1 << ev.getPointerId(actionIndex)
                            : TouchTarget.ALL_POINTER_IDS;

                    // Clean up earlier touch targets for this pointer id in case they
                    // have become out of sync.
                    removePointersFromTouchTargets(idBitsToAssign);

                    final int childrenCount = mChildrenCount;
                    if (newTouchTarget == null && childrenCount != 0) {
                        final float x =
                                isMouseEvent ? ev.getXCursorPosition() : ev.getX(actionIndex);
                        final float y =
                                isMouseEvent ? ev.getYCursorPosition() : ev.getY(actionIndex);
                                
                        // Find a child that can receive the event.
                        // Scan children from front to back.
                        //在当前ViewGroup中找到能处理这个事件的子View或者Viewgroup
                        final ArrayList<View> preorderedList = buildTouchDispatchChildList();
                        final boolean customOrder = preorderedList == null
                                && isChildrenDrawingOrderEnabled();
                        final View[] children = mChildren;
                        for (int i = childrenCount - 1; i >= 0; i--) {
                            final int childIndex = getAndVerifyPreorderedIndex(
                                    childrenCount, i, customOrder);
                            final View child = getAndVerifyPreorderedView(
                                    preorderedList, children, childIndex);
       						....

                            newTouchTarget = getTouchTarget(child);

                            resetCancelNextUpFlag(child);
                            //传递到child，调用dispatchTouchEvent，或者如果当前ViewGroup没有child，则调用View的dispatchTouchEvent交由当前ViewGroup处理
                            if (dispatchTransformedTouchEvent(ev, false, child, idBitsToAssign)) {
                                // Child wants to receive touch within its bounds.
								...
                                mLastTouchDownX = ev.getX();
                                mLastTouchDownY = ev.getY();
                                //记录下这个能消费触摸事件的View target
                                newTouchTarget = addTouchTarget(child, idBitsToAssign);
                                alreadyDispatchedToNewTouchTarget = true;  //这个标识通过这个Down事件找到了能处理这个触摸行为的View target
                                break;
                            }

                            // The accessibility focus didn't handle the event, so clear
                            // the flag and do a normal dispatch to all children.
                            ev.setTargetAccessibilityFocus(false);
                        }
                        if (preorderedList != null) preorderedList.clear();
                    }

					...
                }
            }

            // Dispatch to touch targets.
            if (mFirstTouchTarget == null) {
                // No touch targets so treat this as an ordinary view.
                //如果找不到mFirstTouchTarget，则交给当前ViewGroup处理，即通过super.dispatchTouchEvent(event)
                handled = dispatchTransformedTouchEvent(ev, canceled, null,
                        TouchTarget.ALL_POINTER_IDS);
            } else {
                // Dispatch to touch targets, excluding the new touch target if we already
                // dispatched to it.  Cancel touch targets if necessary.
                TouchTarget predecessor = null;
                TouchTarget target = mFirstTouchTarget;
                while (target != null) {
                    final TouchTarget next = target.next;
                    if (alreadyDispatchedToNewTouchTarget && target == newTouchTarget) {
                    	//前面的dispatchTransformedTouchEvent已经处理了这个Down事件,直接标识为已消费
                        handled = true;
                    } else {
                        final boolean cancelChild = resetCancelNextUpFlag(target.child)
                                || intercepted;

						//如果被拦截，则给子child发ACTION_CANCEL事件
						//如果没有拦截则正常分发到子类，包括MOVE和UP事件
                        if (dispatchTransformedTouchEvent(ev, cancelChild,
                                target.child, target.pointerIdBits)) {
                            handled = true;
                        }
                        if (cancelChild) {
                            if (predecessor == null) {
                                mFirstTouchTarget = next;
                            } else {
                                predecessor.next = next;
                            }
                            target.recycle();
                            target = next;
                            continue;
                        }
                    }
                    predecessor = target;
                    target = next;
                }
            }

		...
        return handled;
    }
```

##### ViewGroup::dispatchTransformedTouchEvent

```cpp
    private boolean dispatchTransformedTouchEvent(MotionEvent event, boolean cancel,
            View child, int desiredPointerIdBits) {
        final boolean handled;

		...

        final MotionEvent transformedEvent;
        if (newPointerIdBits == oldPointerIdBits) {
            if (child == null || child.hasIdentityMatrix()) {
                if (child == null) { //如果当前ViewGroup没有子View或者子ViewGroup，则调用View的dispatchTouchEvent，即由当前ViewGroup来处理
                    handled = super.dispatchTouchEvent(event);
                } else {
                    final float offsetX = mScrollX - child.mLeft;
                    final float offsetY = mScrollY - child.mTop;
                    event.offsetLocation(offsetX, offsetY);

                    handled = child.dispatchTouchEvent(event);  //由子View或者子ViewGroup的dispatchTouchEvent处理

                    event.offsetLocation(-offsetX, -offsetY);
                }
                return handled;
            }
            transformedEvent = MotionEvent.obtain(event);
        } else {
            transformedEvent = event.split(newPointerIdBits);
        }

		...
        return handled;
    }
```

##### View::dispatchTouchEvent

```cpp
    public boolean dispatchTouchEvent(MotionEvent event) {
		...

        if (onFilterTouchEventForSecurity(event)) {
			...
            //noinspection SimplifiableIfStatement
            ListenerInfo li = mListenerInfo;
            if (li != null && li.mOnTouchListener != null
                    && (mViewFlags & ENABLED_MASK) == ENABLED
                    && li.mOnTouchListener.onTouch(this, event)) {  //如果实现了onTouch方法，则不往下执行
                result = true;
            }

            if (!result && onTouchEvent(event)) {  //调用当前View的onTouchEvent(这个View可能是View也可能是ViewGroup)
                result = true;
            }
        }

		...

        return result;
    }
```

##### View::onTouchEvent

```cpp
    public boolean onTouchEvent(MotionEvent event) {
   
        final float x = event.getX();
        final float y = event.getY();
        final int viewFlags = mViewFlags;
        final int action = event.getAction();

        final boolean clickable = ((viewFlags & CLICKABLE) == CLICKABLE
                || (viewFlags & LONG_CLICKABLE) == LONG_CLICKABLE)
                || (viewFlags & CONTEXT_CLICKABLE) == CONTEXT_CLICKABLE;

		...

        if (clickable || (viewFlags & TOOLTIP) == TOOLTIP) {  //根据action进行处理
            switch (action) {
                case MotionEvent.ACTION_UP:
 					...
                    break;

                case MotionEvent.ACTION_DOWN:
					...
                    break;

                case MotionEvent.ACTION_CANCEL:
					...
                    break;

                case MotionEvent.ACTION_MOVE:
					...
                    break;
            }
            return true;
        }
        return false;
    }
```

这个方法也是又臭又长，核心方法了, 还是用图来说明更直接，也结束触摸事件的分析。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b3f8105fcf6b8d459efaa9825eed512f.jpeg#pic_center)

参考：https://blog.csdn.net/moliao2046/article/details/103737611