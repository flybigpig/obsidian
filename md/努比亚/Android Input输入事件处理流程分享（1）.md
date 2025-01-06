_努比亚技术团队原创内容，转载请务必注明出处。_

- 传递流程
- Java层事件传递流程
    - 传递过程
    - 详细传递过程
        - Native传递事件到Java
        - InputEventReceiver分发事件
            - Java层事件入口
            - 对事件进行兼容性处理
            - 转变事件类型并加入队列
            - 循环事件链并进行分发
            - 将事件分发到InputStage
        - InputStage
            - 分发传递事件
            - 处理input事件
            - 处理key事件
        - DecorView处理事件
        - Window.Callback（Activity）分发事件
        - PhoneWindow中处理事件
        - DecorView继续分发事件到View树
        - 小结

安卓中输入事件主要分为KeyEvent和MotionEvent两种，本篇我主要介绍输入事件是如何产生，然后又通过什么方式向用户层传递，过程中都发生了什么，文中所列出的代码均是基于Android 11的源码进行介绍。考虑到大家对上层事件的传递比较熟悉，所以，此篇我采用从上至下倒序方式来介绍，主要介绍事件从native层到Java视图之间的传递流程。

## 传递流程

在介绍之前先看一下安卓输入事件传递的大致流程，本篇我会基于下图中所列出的路线从上至下的方式进行介绍：

![](https://upload-images.jianshu.io/upload_images/26874665-ad0e8040984bc384.png?imageMogr2/auto-orient/strip|imageView2/2/w/583/format/webp)

Input事件传递

这里首先eventhub构建的时候会遍历整个/dev/input路径下的fd，并将其添加到epoll中，同时还会监听此路径下新的设备的创建和卸载。当driver向特定描述符写入事件后，会触发唤醒epoll起来工作，这时候eventHub通过read方法从描述符中读取原始事件，然后通过简单封装成rawEvent并传递给InputReader。  
InputReader中的threadLoop中会调用eventHub的getEvents来获取输入事件，然后通过调用notifyxxx方法将事件传递到InputDispater并最终通过notifyxxx方法传递到上层。

## Java层事件传递流程

Java层主要介绍事件从Native层如果向上层传递，并且事件是如何到达Activity、Window以及ViewGroup的整个树上的。

### 传递过程

先来看一下Java层整个过程的调用时序图：

![](https://upload-images.jianshu.io/upload_images/26874665-2d1e2e9f6c5c0fa8.png?imageMogr2/auto-orient/strip|imageView2/2/w/895/format/webp)

Java层传递时序图

### 详细传递过程

#### Native传递事件到Java

在android_view_InputEventReceiver中的consumeEvents方法中会从InputConsumer中获取input事件，然后通过jni将事件向上层传递。

![](https://upload-images.jianshu.io/upload_images/26874665-08cb1b2d9665bfa1.png?imageMogr2/auto-orient/strip|imageView2/2/w/500/format/webp)

consumeEvents事件到Java

```cpp
status_t NativeInputEventReceiver::consumeEvents(JNIEnv* env,
        bool consumeBatches, nsecs_t frameTime, bool* outConsumedBatch) {
    if (kDebugDispatchCycle) {
        ALOGD("channel '%s' ~ Consuming input events, consumeBatches=%s, frameTime=%" PRId64,
              getInputChannelName().c_str(), toString(consumeBatches), frameTime);
    }
    // 省略若干行
    // 这里通过InputConsumer的consume方法获取到inputevent
    status_t status = mInputConsumer.consume(&mInputEventFactory,
            consumeBatches, frameTime, &seq, &inputEvent,
            &motionEventType, &touchMoveNum, &flag);
    // 省略若干行
    if (inputEventObj) {
        if (kDebugDispatchCycle) {
            ALOGD("channel '%s' ~ Dispatching input event.", getInputChannelName().c_str());
        }
        // 此处通过jni调用InputEventReceiver的dispatchInputEvent方法进行事件的分发，
        // 从而将input事件传递到java层
        env->CallVoidMethod(receiverObj.get(),
                gInputEventReceiverClassInfo.dispatchInputEvent, seq, inputEventObj);
        if (env->ExceptionCheck()) {
            ALOGE("Exception dispatching input event.");
            skipCallbacks = true;
        }
        env->DeleteLocalRef(inputEventObj);
    } else {
        ALOGW("channel '%s' ~ Failed to obtain event object.",
                getInputChannelName().c_str());
        skipCallbacks = true;
    }
  
    if (skipCallbacks) {
        mInputConsumer.sendFinishedSignal(seq, false);
    }
}
  
```

#### InputEventReceiver分发事件

在InputEventReceiver中的dispatchInputEvent方法中会调用onInputEvent方法进行事件的处理，InputEventReceiver是抽象类，它的一个子类WindowInputEventReceiver是用来处理input事件的，WindowInputEventReceiver是frameworks/base/core/java/android/view/ViewRootImpl.java的内部类，它复写了onInputEvent方法，该方法中会调用enqueueInputEvent方法对事件进行入队。

![](https://upload-images.jianshu.io/upload_images/26874665-5a9a72da662c6f6a.png?imageMogr2/auto-orient/strip|imageView2/2/w/652/format/webp)

InputEventReceiver传递事件到InputStage

##### Java层事件入口

```java
private void dispatchInputEvent(int seq, InputEvent event) {
    mSeqMap.put(event.getSequenceNumber(), seq);
    // 直接调用onInputEvent
    onInputEvent(event);
}
  
public void onInputEvent(InputEvent event) {
    // 直接调用finishInputEvent回收事件，所以此方法需要子类复写
    finishInputEvent(event, false);
}
```

dispatchInputEvent方法是jni直接调用的方法，属于Java层的入口方法，其中会直接调用onInputEvent方法，onInputEvent方法中直接调用了finishInputEvent回收事件，所以就需要子类去实现具体的分发逻辑。

##### 对事件进行兼容性处理

onInputEvent方法中首先调用InputCompatProcessor的processInputEventForCompatibility方法对事件进行兼容性处理，这个方法中会判断应用的targetSdkVersion如果小于M并且是motion事件则进行兼容处理并返回，否则返回null；然后会调用enqueueInputEvent方法对事件进行入队。

```java
@Override
public void onInputEvent(InputEvent event) {
    Trace.traceBegin(Trace.TRACE_TAG_VIEW, "processInputEventForCompatibility");
    List<InputEvent> processedEvents;
    try {
        // 这块儿主要是对低版本进行兼容性处理
        processedEvents =
            mInputCompatProcessor.processInputEventForCompatibility(event);
    } finally {
        Trace.traceEnd(Trace.TRACE_TAG_VIEW);
    }
    if (processedEvents != null) {
        // 安卓M及以下版本，在这里处理
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
    } else {
        // 这里对事件进行入队
        enqueueInputEvent(event, this, 0, true);
    }
}
  
public InputEventCompatProcessor(Context context) {
    mContext = context;
    // 获取应用的targetsdk版本
    mTargetSdkVersion = context.getApplicationInfo().targetSdkVersion;
    mProcessedEvents = new ArrayList<>();
}
  
public List<InputEvent> processInputEventForCompatibility(InputEvent e) {
    // 小于M并且是motion事件则进行兼容处理
    if (mTargetSdkVersion < Build.VERSION_CODES.M && e instanceof MotionEvent) {
        mProcessedEvents.clear();
        MotionEvent motion = (MotionEvent) e;
        final int mask =
                MotionEvent.BUTTON_STYLUS_PRIMARY | MotionEvent.BUTTON_STYLUS_SECONDARY;
        final int buttonState = motion.getButtonState();
        final int compatButtonState = (buttonState & mask) >> 4;
        if (compatButtonState != 0) {
            motion.setButtonState(buttonState | compatButtonState);
        }
        mProcessedEvents.add(motion);
        return mProcessedEvents;
    }
    return null;
}
```

##### 转变事件类型并加入队列

此方法主要是将InputEvent转变为QueuedInputEvent并将其放到链表的末尾，然后调用doProcessInputEvents方法进行处理

```java
void enqueueInputEvent(InputEvent event,
        InputEventReceiver receiver, int flags, boolean processImmediately) {
    // 转变为QueuedInputEvent
    QueuedInputEvent q = obtainQueuedInputEvent(event, receiver, flags);
  
    // 将事件插入到链表的末尾
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
        // 调用doProcessInputEvents继续处理
        doProcessInputEvents();
    } else {
        scheduleProcessInputEvents();
    }
}
```

##### 循环事件链并进行分发

此方法中会遍历整个事件链表，对每个事件调用deliverInputEvent方法进行分发

```java
void doProcessInputEvents() {
    // 遍历整个链表，对事件进行分发
    while (mPendingInputEventHead != null) {
        QueuedInputEvent q = mPendingInputEventHead;
        mPendingInputEventHead = q.mNext;
        if (mPendingInputEventHead == null) {
            mPendingInputEventTail = null;
        }
        q.mNext = null;
        // 省略若干行
        // 分发事件
        deliverInputEvent(q);
    }
}
```

##### 将事件分发到InputStage

此方法中会根据flag获取InputStage，然后调用InputStage的deliver方法分发事件。这里的InputStage后面会介绍到，它主要是用来将事件的处理分成多个阶段进行，详细介绍见后文。

```java
private void deliverInputEvent(QueuedInputEvent q) {
    // 省略若干行
    try {
        // 省略若干行
        InputStage stage;
        if (q.shouldSendToSynthesizer()) {
            // flag包含FLAG_UNHANDLED会走这里
            stage = mSyntheticInputStage;
        } else {
            // 是否跳过输入法窗口进行分发
            stage = q.shouldSkipIme() ? mFirstPostImeInputStage : mFirstInputStage;
        }
        // 省略若干行
        if (stage != null) {
            // 处理窗口焦点变更
            handleWindowFocusChanged();
            // 分发事件
            stage.deliver(q);
        } else {
            finishInputEvent(q);
        }
    } finally {
        Trace.traceEnd(Trace.TRACE_TAG_VIEW);
    }
}
```

#### InputStage

InputStage主要是用来将事件的处理分成若干个阶段（stage）进行，事件依次经过每一个stage，如果该事件没有被处理（标识为FLAG_FINISHED），则该stage就会调用onProcess方法处理，然后调用forward执行下一个stage的处理；如果该事件被标识为处理则直接调用forward，执行下一个stage的处理，直到没有下一个stage（也就是最后一个SyntheticInputStage）。这里一共有7中stage，各个stage间串联起来，形成一个链表，各个stage的处理过程大致如下图所示：

![](https://upload-images.jianshu.io/upload_images/26874665-91d192129aeeec09.png?imageMogr2/auto-orient/strip|imageView2/2/w/727/format/webp)

InputStage

先来看下这些stage是如何串起来的，所有的Stage都继承于InputStage，而InputStage是抽象类，它的定义如下：

```java
abstract class InputStage {
    private final InputStage mNext;
    /**
        * Creates an input stage.
        * @param next The next stage to which events should be forwarded.
        */
    public InputStage(InputStage next) {
        // 从构成函数的定义能够看到，传入的next会赋值给当前实例的next，
        // 因此，先插入的就会是最后一个节点（头插法），最终会形成一个链表
        mNext = next;
    }
}
```

在ViewRootImpl的setView方法中有以下代码段：

```java
// 如下创建出来的7个实例会串在一起形成一个链表，
// 链表的头是最后创建出来的nativePreImeStage，
// 链表的尾是首先构造出来的mSyntheticInputStage
// Set up the input pipeline.
mSyntheticInputStage = new SyntheticInputStage();
InputStage viewPostImeStage = new ViewPostImeInputStage(mSyntheticInputStage);
InputStage nativePostImeStage = new NativePostImeInputStage(viewPostImeStage,
        "aq:native-post-ime:" + counterSuffix);
InputStage earlyPostImeStage = new EarlyPostImeInputStage(nativePostImeStage);
// 输入法对应的stage
InputStage imeStage = new ImeInputStage(earlyPostImeStage,
        "aq:ime:" + counterSuffix);
InputStage viewPreImeStage = new ViewPreImeInputStage(imeStage);
InputStage nativePreImeStage = new NativePreImeInputStage(viewPreImeStage,
        "aq:native-pre-ime:" + counterSuffix);
// 第一个处理事件的stage为NativePreImeInputStage
mFirstInputStage = nativePreImeStage;
mFirstPostImeInputStage = earlyPostImeStage;
```

##### 分发传递事件

该方法中会通过flag判断事件是否已经处理，若已经处理则向前调用下一个stage处理事件（next的deliver方法），否则会调用onProcess来处理事件（此方法需要子类实现），然后会根据处理的结果判断是否需要调用链表中的下一个stage来继续处理。

```java
public final void deliver(QueuedInputEvent q) {
    if ((q.mFlags & QueuedInputEvent.FLAG_FINISHED) != 0) {
        // 调用next的deliver方法继续分发处理
        forward(q);
    } else if (shouldDropInputEvent(q)) {
        finish(q, false);
    } else {
        traceEvent(q, Trace.TRACE_TAG_VIEW);
        final int result;
        try {
            // 自身处理事件
            result = onProcess(q);
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_VIEW);
        }
        // 判断是否需要继续分发处理
        apply(q, result);
    }
}
```

##### 处理input事件

这里我们以ViewPostImeInputStage为例(此stage会将事件传递到视图层）介绍事件的分发过程，在onProcess方法中根据event的类型调用不同的方法进行分发。

```java
@Override
protected int onProcess(QueuedInputEvent q) {
    if (q.mEvent instanceof KeyEvent) {
        // 处理key事件
        return processKeyEvent(q);
    } else {
        final int source = q.mEvent.getSource();
        if ((source & InputDevice.SOURCE_CLASS_POINTER) != 0) {
            // 处理pointer事件
            return processPointerEvent(q);
        } else if ((source & InputDevice.SOURCE_CLASS_TRACKBALL) != 0) {
            // 处理轨迹球事件
            return processTrackballEvent(q);
        } else {
            // 处理一般的motion事件
            return processGenericMotionEvent(q);
        }
    }
}
```

##### 处理key事件

此方法中会调用view的dispatchKeyEvent方法来将input事件分发到view树上，然后按照view的事件分发机制继续分发处理事件，需要注意的是这里的mView是decorview实例，在ViewRootImpl的setView方法中被设置。

```java
private int processKeyEvent(QueuedInputEvent q) {
    final KeyEvent event = (KeyEvent)q.mEvent;
    // 省略若干行
    // 调用view的dispatchKeyEvent来分发事件，此处的mView是decorview
    if (mView.dispatchKeyEvent(event)) {
        return FINISH_HANDLED;
    }
    // 省略若干行
    // 继续调用链表的下一个stage来处理
    return FORWARD;
}
```

#### DecorView处理事件

mView在ViewRootImpl的setView方法中被赋值，被赋值的对象便是DecorView的实例，而在ViewPostImeInputStage的onPressess方法中会将key事件通过dispatchKeyEvent方法传递到DecorView中。

##### dispatchKeyEvent

此方法中先获取Window.Callback对象，然后调用其dispatchKeyEvent继续处理，若callback为null则调用父类同名方法来处理。最后，还会回调window的onKeyDown和onKeyUp方法，需要注意的是Activity和Dialog都默认实现了Window.Callback接口的方法，所以这里就会将事件传递到Activity或者Dialog里。

```java
public boolean dispatchKeyEvent(KeyEvent event) {
    // 省略若干行
    if (!mWindow.isDestroyed()) {
        // window没有销毁，如何存在Window.Callback，
        // 则调用callback的dispatchKeyEvent继续处理
        // 否则调用父类的dispatchKeyEvent来处理
        final Window.Callback cb = mWindow.getCallback();
        final boolean handled = cb != null && mFeatureId < 0 ? cb.dispatchKeyEvent(event)
                : super.dispatchKeyEvent(event);
        if (handled) {
            return true;
        }
    }
  
    // 这里回调window的onKeyDown和onKeyUp方法
    return isDown ? mWindow.onKeyDown(mFeatureId, event.getKeyCode(), event)
            : mWindow.onKeyUp(mFeatureId, event.getKeyCode(), event);
}
```

#### Window.Callback（Activity）分发事件

这里以Activity来介绍事件的传递流程，查看Activity的dispatchKeyEvent方法。

##### dispatchKeyEvent

```java
public boolean dispatchKeyEvent(KeyEvent event) {
    // 省略若干行
    // 调用Window的superDispatchKeyEvent方法继续处理
    Window win = getWindow();
    if (win.superDispatchKeyEvent(event)) {
        return true;
    }
    View decor = mDecor;
    if (decor == null) decor = win.getDecorView();
    // 此处调用KeyEvent的dispatch，传入的receiver为当前实例，
    // 其内部会根据event的action来调用当前实例的onKeyDown和onKeyUp方法
    return event.dispatch(this, decor != null
            ? decor.getKeyDispatcherState() : null, this);
}
```

#### PhoneWindow中处理事件

Activity中getWindow返回的其实是PhoneWindow的实例，看一下PhoneWindow的方法实现。

##### superDispatchKeyEvent

此方法中会直接调用mDecor的同名方法，这个mDecor是DecorView的实例。

```java
public boolean superDispatchKeyEvent(KeyEvent event) {
    // 调用DecorView的同名方法
    return mDecor.superDispatchKeyEvent(event);
}
```

#### DecorView继续分发事件到View树

这里Activity通过调用DecorView的superDispatchKeyEvent方法，又将事件传入到DecorView里了，这里和前面事件传递到DecorView的不同之处是前面传递到dispatchKeyEvent，而这里是superDispatchKeyEvent。

##### superDispatchKeyEvent

此方法中会直接调用DecorView的父类的dispatchKeyEvent方法，如果事件没有被处理掉的话，就会通过ViewRootImpl将其作为UnhandledEvent进行处理。

```java
public boolean superDispatchKeyEvent(KeyEvent event) {
    // 省略若干行
    // 这里直接调用父类的dispatchKeyEvent方法，DecorView的父类是FrameLayout，
    // 所以就分发到ViewGroup了，接下来就按照View树的形式从根View向子View分发
    if (super.dispatchKeyEvent(event)) {
        return true;
    }
    // 如果没有被消费掉，则通过ViewRootImpl将其作为Unhandled继续处理
    return (getViewRootImpl() != null) && getViewRootImpl().dispatchUnhandledKeyEvent(event);
}
```

#### 小结

通过以上分析，我们便了解了input事件从Native层传递到Java层，然后继续传递到View树上的整个过程。需要注意的是，在各个传递环节，如果调用方法返回了true，就代表它消费掉了本次事件，那么就不会继续朝后传递。另外，对于ViewGroup来说，存在onInterceptTouchEvent方法，此方法用来拦截事件向子view分发，如果返回了true，事件就会传递到当前ViewGroup的onTouchEvent中而不在向子View传递；否则的话，会分发到当前ViewGroup，然后再传递到子View。由于View不能包含子View，所以View不存在拦截方法。

![](https://upload-images.jianshu.io/upload_images/26874665-1d94b096a7adf34c.png?imageMogr2/auto-orient/strip|imageView2/2/w/642/format/webp)

InputStage传递事件到View树

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/15660b270b8a  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。