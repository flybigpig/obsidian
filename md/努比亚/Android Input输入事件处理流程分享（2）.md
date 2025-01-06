_努比亚技术团队原创内容，转载请务必注明出处。_

- Native层传递过程
    - InputEventReceiver的事件来源于哪里
    - InputConsumer
        - InputConsumer处理事件
        - InputConsumer的构建
    - InputChannel
        - InputChannel的创建
        - server端InputChannel的注册
        - client端InputChannel读取事件并传递
        - 小结
    - InputManagerService
        - IMS的创建
        - IMS的启动
        - 小结
    - InputDispatcher
        - InputDispatcher启动
        - InputDispatcher分发事件
    - InputReader（InputDispatcher事件的来源）
        - InputReader启动
        - InputReader处理事件
    - EventHub
        - EventHub的创建
        - EventHub如何获取输入事件
            - EventHub处理reopen设备
            - EventHub处理close设备
            - EventHub扫描设备
            - EventHub处理open设备
            - EventHub处理event
            - EventHub处理inotify事件
        - 小结

## Native层传递过程

此小节主要介绍输入事件是如何从InputReader获取，然后InputDispatcher又是如何将它们分发出去，这个过程中使用什么技术进行事件的传递的。

![](https://upload-images.jianshu.io/upload_images/26874665-114fe48e1fdb91f5.png?imageMogr2/auto-orient/strip|imageView2/2/w/814/format/webp)

Natvie事件传递时序图

### InputEventReceiver的事件来源于哪里

上节中有介绍在Native层NativeInputEventReceiver的consumeEvents方法中会通过jni方式调用Java层的InputEventReceiver的dispatchInputEvent方法将事件传递到上层。那consumeEvents的事件又来源于哪里呢？我们继续看这个consumeEvents方法。

```cpp
status_t NativeInputEventReceiver::consumeEvents(JNIEnv* env,
        bool consumeBatches, nsecs_t frameTime, bool* outConsumedBatch) {
    // 省略若干行
    for (;;) {
        // 省略若干行
        InputEvent* inputEvent;
        // 这里调用了InputConsumer的consume方法来获取输入事件
        status_t status = mInputConsumer.consume(&mInputEventFactory,
                consumeBatches, frameTime, &seq, &inputEvent,
                &motionEventType, &touchMoveNum, &flag);
        // 省略若干行
        if (skipCallbacks) {
            mInputConsumer.sendFinishedSignal(seq, false);
        }
    }
}
```

### InputConsumer

consume方法中主要是从InputChannel获取输入事件的信息，然后根据消息中获取的事件类型构造出对应的event，并将消息中的事件信息赋值给event对象。

#### InputConsumer处理事件

从上面分析我们能够看到，再NativeInputEventReceiver的consumeEvents方法中，会循环调用InputConsumer的consume方法获取事件并进行处理。InputConsumer的consume方法中会通过InputChannel从socket中通过recv系统调用获取下层传递的事件，获取到事件后就会通过jni向Java层传递。

![](https://upload-images.jianshu.io/upload_images/26874665-d38961e3238f5016.png?imageMogr2/auto-orient/strip|imageView2/2/w/529/format/webp)

InputConsumer处理事件

```cpp
status_t InputConsumer::consume(InputEventFactoryInterface* factory, bool consumeBatches,
                                nsecs_t frameTime, uint32_t* outSeq, InputEvent** outEvent,
                                int* motionEventType, int* touchMoveNumber, bool* flag) {
    // 省略若干行
    *outSeq = 0;
    *outEvent = nullptr;
    // Fetch the next input message.
    // Loop until an event can be returned or no additional events are received.
    while (!*outEvent) {
        if (mMsgDeferred) {
            // mMsg contains a valid input message from the previous call to consume
            // that has not yet been processed.
            mMsgDeferred = false;
        } else {
            // Receive a fresh message.
            // 这里通过调用InputChannel的receiveMessage来获取消息
            status_t result = mChannel->receiveMessage(&mMsg);
            // 省略若干行
        }
        // 根据消息的类型生成不同的Event
        switch (mMsg.header.type) {
            case InputMessage::Type::KEY: {
                // 构造一个KeyEvent
                KeyEvent* keyEvent = factory->createKeyEvent();
                if (!keyEvent) return NO_MEMORY;
                // 从msg中获取事件的各属性，并赋值给构造出的Event对象
                initializeKeyEvent(keyEvent, &mMsg);
                *outSeq = mMsg.body.key.seq;
                *outEvent = keyEvent;
                if (DEBUG_TRANSPORT_ACTIONS) {
                    ALOGD("channel '%s' consumer ~ consumed key event, seq=%u",
                          mChannel->getName().c_str(), *outSeq);
                }
            break;
            }
  
            case InputMessage::Type::MOTION: {
                // 构造一个MotionEvent
                MotionEvent* motionEvent = factory->createMotionEvent();
                if (!motionEvent) return NO_MEMORY;
                updateTouchState(mMsg);
                // 从msg中获取事件的各属性，并赋值给构造出的Event对象
                initializeMotionEvent(motionEvent, &mMsg);
                *outSeq = mMsg.body.motion.seq;
                *outEvent = motionEvent;
  
                if (DEBUG_TRANSPORT_ACTIONS) {
                    ALOGD("channel '%s' consumer ~ consumed motion event, seq=%u",
                          mChannel->getName().c_str(), *outSeq);
                }
                break;
            }
            // 省略若干行
  
        }
    }
    return OK;
}
```

这里我们先看下event的构造和初始化，输入消息的获取随后再介绍。先看下factory->createMotionEvent，这里factory是PreallocatedInputEventFactory的实例。

```cpp
class PreallocatedInputEventFactory : public InputEventFactoryInterface {
public:
    PreallocatedInputEventFactory() { }
    virtual ~PreallocatedInputEventFactory() { }
    // 可以看到这里返回的是全局变量的地址
    virtual KeyEvent* createKeyEvent() override { return &mKeyEvent; }
    virtual MotionEvent* createMotionEvent() override { return &mMotionEvent; }
    virtual FocusEvent* createFocusEvent() override { return &mFocusEvent; }
  
private:
    // 这里定义不同类型的事件变量
    KeyEvent mKeyEvent;
    MotionEvent mMotionEvent;
    FocusEvent mFocusEvent;
};
```

好了，我们继续看event的初始化。这里主要是从msg中获取对应事件的详细信息然后赋值给对应的event对象上。

```cpp
// 对key事件进行初始化
void InputConsumer::initializeKeyEvent(KeyEvent* event, const InputMessage* msg) {
    event->initialize(msg->body.key.eventId, msg->body.key.deviceId, msg->body.key.source,
                      msg->body.key.displayId, msg->body.key.hmac, msg->body.key.action,
                      msg->body.key.flags, msg->body.key.keyCode, msg->body.key.scanCode,
                      msg->body.key.metaState, msg->body.key.repeatCount, msg->body.key.downTime,
                      msg->body.key.eventTime);
}
  
// 对motion事件进行初始化
void InputConsumer::initializeMotionEvent(MotionEvent* event, const InputMessage* msg) {
    // 省略若干行
    event->initialize(msg->body.motion.eventId, msg->body.motion.deviceId, msg->body.motion.source,
                      msg->body.motion.displayId, msg->body.motion.hmac, msg->body.motion.action,
                      msg->body.motion.actionButton, msg->body.motion.flags,
                      msg->body.motion.edgeFlags, msg->body.motion.metaState,
                      msg->body.motion.buttonState, msg->body.motion.classification,
                      msg->body.motion.xScale, msg->body.motion.yScale, msg->body.motion.xOffset,
                      msg->body.motion.yOffset, msg->body.motion.xPrecision,
                      msg->body.motion.yPrecision, msg->body.motion.xCursorPosition,
                      msg->body.motion.yCursorPosition, msg->body.motion.downTime,
                      msg->body.motion.eventTime, pointerCount, pointerProperties, pointerCoords);
}
```

然后我们继续看msg的获取方法：InputChannel的receiveMessage方法。

```cpp
status_t InputChannel::receiveMessage(InputMessage* msg) {
    ssize_t nRead;
    do {
        // 这里通过recv系统调用从socket中读取消息
        nRead = ::recv(mFd.get(), msg, sizeof(InputMessage), MSG_DONTWAIT);
    } while (nRead == -1 && errno == EINTR);
    // 省略若干行
    return OK;
}
```

可以看到这个方法主要是从socket中读取消息。那这里的socket是什么时候建立的呢？我们继续向下看。

#### InputConsumer的构建

在NativeInputEventReceiver的构造方法中，会创建出NativeInputEventReceiver，并将InputChannel传入。而NativeInputEventReceiver的构建是在Java层InputEventReceiver的native方法nativeInit中创建，并且能够看到，这里的InputChannel是从Java层传递下来的。

```cpp
InputConsumer::InputConsumer(const sp<InputChannel>& channel) :
        mResampleTouch(isTouchResamplingEnabled()),
        // 初始化InputChannel
        mChannel(channel), mMsgDeferred(false) {
}
```

我们发现在InputConsumer构造时对InputChannel进行了初始化，那就继续超前看InputConsumer在哪里构建的。

```cpp
NativeInputEventReceiver::NativeInputEventReceiver(JNIEnv* env,
        jobject receiverWeak, const sp<InputChannel>& inputChannel,
        const sp<MessageQueue>& messageQueue) :
        mReceiverWeakGlobal(env->NewGlobalRef(receiverWeak)),
        mInputConsumer(inputChannel), mMessageQueue(messageQueue),
        mBatchedInputEventPending(false), mFdEvents(0) {
    if (kDebugDispatchCycle) {
        ALOGD("channel '%s' ~ Initializing input event receiver.", getInputChannelName().c_str());
    }
}
```

回到NativeInputEventReceiver中，发现它的构造方法中传入了InputChannel，那么继续看NativeInputEventReceiver的构建。

```cpp
static jlong nativeInit(JNIEnv* env, jclass clazz, jobject receiverWeak,
        jobject inputChannelObj, jobject messageQueueObj) {
    // 通过jni获取java创建的InputChannel
    sp<InputChannel> inputChannel = android_view_InputChannel_getInputChannel(env,
            inputChannelObj);
    // 省略若干行
    // 构建出NativeInputEventReceiver
    sp<NativeInputEventReceiver> receiver = new NativeInputEventReceiver(env,
            receiverWeak, inputChannel, messageQueue);
    // 初始化Receiver
    status_t status = receiver->initialize();
    // 省略若干行
    receiver->incStrong(gInputEventReceiverClassInfo.clazz); // retain a reference for the object
    return reinterpret_cast<jlong>(receiver.get());
}
```

通过上述分析，我们发现NativeInputEventReceiver中获取底层事件的InputChannel是来自于Java层的传递，那么，InputChannel又是如何创建的呢？

### InputChannel

InputChannel会作为句柄传递到下层，后面分发事件的时候会通过它来进行。而且这里会创建出两个，一个作为server端注册到InputManagerService，最终会注册到InputDispatcher中去，另一个则作为client端来接收server端的事件。

#### InputChannel的创建

通过前面分析，我们发现NativeInputEventReceiver中的InputChanel来源于Java层的InputChannel。上述nativeInit是Java层InputEventReceiver的native方法，继续看Java层的InputEventReceiver。

![](https://upload-images.jianshu.io/upload_images/26874665-d4e4f19629ab4626.png?imageMogr2/auto-orient/strip|imageView2/2/w/876/format/webp)

InputChannel的创建

```java
public InputEventReceiver(InputChannel inputChannel, Looper looper) {
    // 省略若干行
    mInputChannel = inputChannel;
    mMessageQueue = looper.getQueue();
    // 将Java层的inputChannel向下层传递
    mReceiverPtr = nativeInit(new WeakReference<InputEventReceiver>(this),
            inputChannel, mMessageQueue);
    mCloseGuard.open("dispose");
}
```

Java层InputEventReceiver构造时传入了InputChannel。在ViewRootImpl的setView方法中会创建InputChannel，然后会调用Session的addToDisplayAsUser方法初始化InputChannel

```java
public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView,
        int userId) {
    synchronized (this) {
        if (mView == null) {
            // 省略若干行
            InputChannel inputChannel = null;
            if ((mWindowAttributes.inputFeatures
                    & WindowManager.LayoutParams.INPUT_FEATURE_NO_INPUT_CHANNEL) == 0) {
                inputChannel = new InputChannel();
            }
            // 省略若干行
            // 调用Session的addToDisplayAsUser方法来添加window，
            // 会初始化InputChannel
            res = mWindowSession.addToDisplayAsUser(mWindow, mSeq, mWindowAttributes,
                    getHostVisibility(), mDisplay.getDisplayId(), userId, mTmpFrame,
                    mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
                    mAttachInfo.mDisplayCutout, inputChannel,
                    mTempInsets, mTempControls);
            // 省略若干行
            if (inputChannel != null) {
                if (mInputQueueCallback != null) {
                    mInputQueue = new InputQueue();
                    mInputQueueCallback.onInputQueueCreated(mInputQueue);
                }
                // 将InputChannel传入InputEventReceiver
                mInputEventReceiver = new WindowInputEventReceiver(inputChannel,
                        Looper.myLooper());
            }
            // 省略若干行
        }
    }
}
```

Session的addToDisplayAsUser方法会继续调用WindowManagerService的addWindow方法。

```java
public int addToDisplayAsUser(IWindow window, int seq, WindowManager.LayoutParams attrs,
        int viewVisibility, int displayId, int userId, Rect outFrame,
        Rect outContentInsets, Rect outStableInsets,
        DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel,
        InsetsState outInsetsState, InsetsSourceControl[] outActiveControls) {
    // 直接调用WindowManagerService的addWindow方法
    return mService.addWindow(this, window, seq, attrs, viewVisibility, displayId, outFrame,
            outContentInsets, outStableInsets, outDisplayCutout, outInputChannel,
            outInsetsState, outActiveControls, userId);
}
```

addWindow方法中会调用WindowState打开InputChannel。

```java
public int addWindow(Session session, IWindow client, int seq,
        LayoutParams attrs, int viewVisibility, int displayId, Rect outFrame,
        Rect outContentInsets, Rect outStableInsets,
        DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel,
        InsetsState outInsetsState, InsetsSourceControl[] outActiveControls,
        int requestUserId) {
    // 省略若干行
    final WindowState win = new WindowState(this, session, client, token, parentWindow,
            appOp[0], seq, attrs, viewVisibility, session.mUid, userId,
            session.mCanAddInternalSystemWindow);
    // 省略若干行
    final boolean openInputChannels = (outInputChannel != null
            && (attrs.inputFeatures & INPUT_FEATURE_NO_INPUT_CHANNEL) == 0);
    if  (openInputChannels) {
        // 这里会调用WindowState的openInputChannel来打开inputChannel
        win.openInputChannel(outInputChannel);
    }
    // 省略若干行
    return res;
}
```

继续看WindowState的openInputChannel方法。首先会通过调用InputChannel的静态方法openInputChannelPair来创建两个InputChannel，一个作为client一个作为server；然后还会调用InputManagerService的registerInputChannel来注册server端的InputChannel；最后将client端的InputChannel设置到outInputChannel中。

```java
void openInputChannel(InputChannel outInputChannel) {
    if (mInputChannel != null) {
        throw new IllegalStateException("Window already has an input channel.");
    }
    String name = getName();
    // 通过openInputChannelPair方法创建出两个InputChannel
    InputChannel[] inputChannels = InputChannel.openInputChannelPair(name);
    mInputChannel = inputChannels[0];
    mClientChannel = inputChannels[1];
    // 注册server端的InputChannel到InputManagerService中
    mWmService.mInputManager.registerInputChannel(mInputChannel);
    mInputWindowHandle.token = mInputChannel.getToken();
    if (outInputChannel != null) {
        // 将client端的InputChannel设置到outInputChannel
        mClientChannel.transferTo(outInputChannel);
        mClientChannel.dispose();
        mClientChannel = null;
    } else {
        // If the window died visible, we setup a dummy input channel, so that taps
        // can still detected by input monitor channel, and we can relaunch the app.
        // Create dummy event receiver that simply reports all events as handled.
        mDeadWindowEventReceiver = new DeadWindowEventReceiver(mClientChannel);
    }
    mWmService.mInputToWindowMap.put(mInputWindowHandle.token, this);
}
```

上述openInputChannelPair方法中会直接调用InputChannel的native方法nativeOpenInputChannelPair来创建出一对InputChannel。

```java
public static InputChannel[] openInputChannelPair(String name) {
    if (name == null) {
        throw new IllegalArgumentException("name must not be null");
    }
  
    if (DEBUG) {
        Slog.d(TAG, "Opening input channel pair '" + name + "'");
    }
    // 继续调用natvie方法创建出两个InputChannel
    return nativeOpenInputChannelPair(name);
}
```

jni方法nativeOpenInputChannelPair中会继续调用InputChannel的openInputChannelPair静态方法。然后将创建出的两个inputChannel分别添加到数组中，然后返回给上层。

```cpp
static jobjectArray android_view_InputChannel_nativeOpenInputChannelPair(JNIEnv* env,
        jclass clazz, jstring nameObj) {
    ScopedUtfChars nameChars(env, nameObj);
    std::string name = nameChars.c_str();
  
    sp<InputChannel> serverChannel;
    sp<InputChannel> clientChannel;
    // 创建出server端和client端的InputChannel
    status_t result = InputChannel::openInputChannelPair(name, serverChannel, clientChannel);
    // 省略若干行
    // 添加到数组中，然后返回给上层
    env->SetObjectArrayElement(channelPair, 0, serverChannelObj);
    env->SetObjectArrayElement(channelPair, 1, clientChannelObj);
    return channelPair;
}
```

openInputChannelPair方法中会首先通过socketpair创建一对相互连接的套接字，然后分别给socket设置相应的选项值；然后通过InputChannel的create方法创建出两个分别与socket关联的inuptChannel。

```cpp
status_t InputChannel::openInputChannelPair(const std::string& name,
        sp<InputChannel>& outServerChannel, sp<InputChannel>& outClientChannel) {
    int sockets[2];
    // 创建一对相互连接的socket
    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets)) {
        status_t result = -errno;
        // 创建失败做相应的处理
        ALOGE("channel '%s' ~ Could not create socket pair.  errno=%d",
                name.c_str(), errno);
        outServerChannel.clear();
        outClientChannel.clear();
        return result;
    }
  
    // 分别设置两个socket的可读可写buffer
    int bufferSize = SOCKET_BUFFER_SIZE;
    setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));
    setsockopt(sockets[0], SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));
    setsockopt(sockets[1], SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));
    setsockopt(sockets[1], SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));
  
    sp<IBinder> token = new BBinder();
  
    std::string serverChannelName = name + " (server)";
    android::base::unique_fd serverFd(sockets[0]);
    // 创建出server端InputChannel，并于socket关联
    outServerChannel = InputChannel::create(serverChannelName, std::move(serverFd), token);
  
    std::string clientChannelName = name + " (client)";
    android::base::unique_fd clientFd(sockets[1]);
    // 创建出client端InputChannel，并于socket关联
    outClientChannel = InputChannel::create(clientChannelName, std::move(clientFd), token);
    return OK;
}
```

通过InputChannel的create方法构建出InputChannel并返回。

```cpp
sp<InputChannel> InputChannel::create(const std::string& name, android::base::unique_fd fd,
                                      sp<IBinder> token) {
    // 设置文件描述符fd的状态属性为O_NONBLOCK
    const int result = fcntl(fd, F_SETFL, O_NONBLOCK);
    if (result != 0) {
        LOG_ALWAYS_FATAL("channel '%s' ~ Could not make socket non-blocking: %s", name.c_str(),
                         strerror(errno));
        return nullptr;
    }
    // 创建出InputChannel并返回
    return new InputChannel(name, std::move(fd), token);
}
```

至此，InputChannel便创建并关联上socket上了。并且通过前面的介绍，我们知道了获取输入事件时是从client端的socket中读取消息并进行事件封装，然后传递到上层。但是这里我们发现有一个问题，就是client端socket中的数据是从哪里来的呢？我们继续看一下WindowState的openInputChannel方法。

#### server端InputChannel的注册

在通过openInputChannel开启InputChannel后，会调用了InputManagerService的registerInputChannel方法注册server端的InputChannel

![](https://upload-images.jianshu.io/upload_images/26874665-8b5b2c3ba3c86492.png?imageMogr2/auto-orient/strip|imageView2/2/w/821/format/webp)

server端InputChannel的注册

```java
void openInputChannel(InputChannel outInputChannel) {
    // 省略若干行
    // 通过openInputChannelPair方法创建出两个InputChannel
    InputChannel[] inputChannels = InputChannel.openInputChannelPair(name);
    mInputChannel = inputChannels[0];
    mClientChannel = inputChannels[1];
    // 注册server端的InputChannel到InputManagerService中
    mWmService.mInputManager.registerInputChannel(mInputChannel);
    // 省略若干行
}
```

我们发现server端的InputChannel被注册到了InputManagerService中去了，那么，我们继续向下看。

```java
public void registerInputChannel(InputChannel inputChannel) {
    if (inputChannel == null) {
        throw new IllegalArgumentException("inputChannel must not be null.");
    }
    // 调用native方法继续注册
    nativeRegisterInputChannel(mPtr, inputChannel);
}
```

在InputManagerService的registerInputChannel方法中直接调用了native方法nativeRegisterInputChannel，我们继续。

```cpp
static void nativeRegisterInputChannel(JNIEnv* env, jclass /* clazz */,
        jlong ptr, jobject inputChannelObj) {
    NativeInputManager* im = reinterpret_cast<NativeInputManager*>(ptr);
    // 获取InputChannel
    sp<InputChannel> inputChannel = android_view_InputChannel_getInputChannel(env,
            inputChannelObj);
    if (inputChannel == nullptr) {
        throwInputChannelNotInitialized(env);
        return;
    }
    // 将inputChannel注册到NativeInputManager中
    status_t status = im->registerInputChannel(env, inputChannel);
    // 设置dispose的callback，在inputChannel
    // dispose之后会调用函数指针handleInputChannelDisposed
    // 来调用NativeInputManager的unregisterInputChannel
    // 解注册inputChannel
    android_view_InputChannel_setDisposeCallback(env, inputChannelObj,
            handleInputChannelDisposed, im);
}
```

在native方法中，先调用了NativeInputManager的registerInputChannel方法注册inputChannel，然后会给inputChannel设置dispose callback，并且callback中执行了inputChannel的解注册。在NativeInputManager的registerInputChannel方法中，会获取InputDispatcher，并将inputChannel注册到其中去。

```cpp
status_t NativeInputManager::registerInputChannel(JNIEnv* /* env */,
        const sp<InputChannel>& inputChannel) {
    ATRACE_CALL();
    return mInputManager->getDispatcher()->registerInputChannel(inputChannel);
}
```

在InputDispatcher的registerInputChannel方法中，会通过InputChannel构建出Connection，然后将其添加到注册列表当中。

```cpp
status_t InputDispatcher::registerInputChannel(const sp<InputChannel>& inputChannel) {
#if DEBUG_REGISTRATION
    ALOGD("channel '%s' ~ registerInputChannel", inputChannel->getName().c_str());
#endif
  
    { // acquire lock
        std::scoped_lock _l(mLock);
        // 省略若干行
        // 创建connection并添加的注册列表中
        sp<Connection> connection = new Connection(inputChannel, false /*monitor*/, mIdGenerator);
        int fd = inputChannel->getFd();
        mConnectionsByFd[fd] = connection;
        mInputChannelsByToken[inputChannel->getConnectionToken()] = inputChannel;
        // 将inputChannel的fd添加到looper中，并且对应的event是ALOOPER_EVENT_INPUT
        // 传入的looper callback为handleReceiveCallback方法，
        // 因此当事件到来时，会触发此callback
        mLooper->addFd(fd, 0, ALOOPER_EVENT_INPUT, handleReceiveCallback, this);
    } // release lock
  
    // Wake the looper because some connections have changed.
    mLooper->wake();
    return OK;
}
```

好了，到这里我们就知道了，server端的inputChannel最终被注册到了InputDispatcher的注册列表中去了，所以InputDispatcher中就可以通过向server端的socket中写入消息，然后client端就可以读取到了。但是，这里还发现存在一个问题：那就是server端写入事件消息后，怎么通知到client去开始处理呢？我们在回过头来看一下前面介绍的InputEventReceiver的构造函数。

#### client端InputChannel读取事件并传递

```java
public InputEventReceiver(InputChannel inputChannel, Looper looper) {
    // 省略若干层
    mInputChannel = inputChannel;
    mMessageQueue = looper.getQueue();
    // 将Java层的inputChannel向下层传递
    mReceiverPtr = nativeInit(new WeakReference<InputEventReceiver>(this),
            inputChannel, mMessageQueue);
    mCloseGuard.open("dispose");
}
```

在InputEventReceiver的构造方法中调用了native方法nativeInit进行native层的初始化

```cpp
static jlong nativeInit(JNIEnv* env, jclass clazz, jobject receiverWeak,
        jobject inputChannelObj, jobject messageQueueObj) {
    // 省略若干行
    sp<NativeInputEventReceiver> receiver = new NativeInputEventReceiver(env,
            receiverWeak, inputChannel, messageQueue);
    // 初始化Receiver
    status_t status = receiver->initialize();
    // 省略若干行
    return reinterpret_cast<jlong>(receiver.get());
}
```

在NativeInputEventReceiver初始化时，会将inputChannel的文件描述符fd添加到looper中去，并且添加了looper callback为NativeInputEventReceiver实例自身，所以，当server端写入事件消息时，就会触发callback，于是便调用到NativeInputEventReceiver的handleEvent方法。

```cpp
status_t NativeInputEventReceiver::initialize() {
    // 设置文件描述符对应的event为ALOOPER_EVENT_INPUT
    setFdEvents(ALOOPER_EVENT_INPUT);
    return OK;
}
  
void NativeInputEventReceiver::setFdEvents(int events) {
    if (mFdEvents != events) {
        mFdEvents = events;
        int fd = mInputConsumer.getChannel()->getFd();
        if (events) {
            // 将inputChannel的文件描述符添加到looper中
            // 对应的event为ALOOPER_EVENT_INPUT
            // 并传入了this作为loopercallback
            mMessageQueue->getLooper()->addFd(fd, 0, events, this, nullptr);
        } else {
            mMessageQueue->getLooper()->removeFd(fd);
        }
    }
}
```

我们发现在handleEvent方法中，调用了consumeEvents方法来处理事件，而consumeEvents方法便是我们前面介绍过了的，在其内部会通过jni的方式将事件向Java层传递到InputEventReceiver的dispatchInputEvent，从而便实现了事件的分发。

```cpp
int NativeInputEventReceiver::handleEvent(int receiveFd, int events, void* data) {
    // 省略若干行
    // 接收添加的ALOOPER_EVENT_INPUT事件
    if (events & ALOOPER_EVENT_INPUT) {
        JNIEnv* env = AndroidRuntime::getJNIEnv();
        // 调用consumeEvents方法处理事件
        status_t status = consumeEvents(env, false /*consumeBatches*/, -1, nullptr);
        mMessageQueue->raiseAndClearException(env, "handleReceiveCallback");
        return status == OK || status == NO_MEMORY ? 1 : 0;
    }
    // 省略若干行
    return 1;
}
```

#### 小结

通过以上分析，我们便明白了InputDispatcher是如果通过InputChannel将事件向上层进行分发的整个过程。首先是创建一对InputChannel，并且会开启一对相互连接的socket作为事件传递的媒介。server端的InputChannel会注册到InputDispatcher中去以完成事件的分发，并且会将其fd添加到looper中，而client端的InputChannel会在InputEventReceiver初始化时也会将其fd添加到looper中，并传入callback类接收server端写入的事件，这样整个过程便串联起来了。但是，这里还存在一个问题：InputDispatcher的事件从哪里来呢？

### InputManagerService

InputManagerService简称IMS，和其他系统服务一样，是在SystemServer中创建并启动，它主要是用来监测和加工输入事件，并向上层传递。而且，上文所说的InputDispatcher以及InputReader均是在InputManagerService中构建出来的。

#### IMS的创建

在SystemServer的startOtherServices方法中，直接通过new的方式创建出IMS实例。

```java
private void startOtherServices(@NonNull TimingsTraceAndSlog t) {
    // 省略若干行
    t.traceBegin("StartInputManagerService");
    // 创建IMS
    inputManager = new InputManagerService(context);
    t.traceEnd();
    // 省略若干行
}
```

InputManagerService的构造方法中，会先创建出Handler，然后通过native方法nativeInit来实现IMS的初始化，主要是构建native层的IMS。

```java
public InputManagerService(Context context) {
    this.mContext = context;
    // 创建出handler
    this.mHandler = new InputManagerHandler(DisplayThread.get().getLooper());
    // 省略若干行
    // 调用native方法来构建native层IMS
    mPtr = nativeInit(this, mContext, mHandler.getLooper().getQueue());
    // 省略若干行
}
```

native方法中先获取到上层传下来的messageQueue，然后获取对应的Looper，并构建出NativeInputManager。

```cpp
static jlong nativeInit(JNIEnv* env, jclass /* clazz */,
        jobject serviceObj, jobject contextObj, jobject messageQueueObj) {
    // 获取上层传递的MessageQueue
    sp<MessageQueue> messageQueue = android_os_MessageQueue_getMessageQueue(env, messageQueueObj);
    if (messageQueue == nullptr) {
        jniThrowRuntimeException(env, "MessageQueue is not initialized.");
        return 0;
    }
    // 构建NativeInputManager
    NativeInputManager* im = new NativeInputManager(contextObj, serviceObj,
            messageQueue->getLooper());
    im->incStrong(0);
    return reinterpret_cast<jlong>(im);
}
```

NativeInputManager构造中，先创建出native层IMS实例，然后将其添加到serviceManager中。

```cpp
NativeInputManager::NativeInputManager(jobject contextObj,
        jobject serviceObj, const sp<Looper>& looper) :
        mLooper(looper), mInteractive(true) {
    // 省略若干行
    // 构建出native层的IMS，即InputManager
    mInputManager = new InputManager(this, this);
    // 将IMS添加到serviceManager中
    defaultServiceManager()->addService(String16("inputflinger"),
            mInputManager, false);
}
```

在InputManager构建时，会分别创建出InputDispatcher、InputListener以及InputReader实例。这里将InputDispatcher作为InputListener传递到InputClassifier，并最终传递到InputReader中去。

```cpp
InputManager::InputManager(
        const sp<InputReaderPolicyInterface>& readerPolicy,
        const sp<InputDispatcherPolicyInterface>& dispatcherPolicy) {
    mDispatcher = createInputDispatcher(dispatcherPolicy);
    mClassifier = new InputClassifier(mDispatcher);
    mReader = createInputReader(readerPolicy, mClassifier);
}
```

通过以上分析，我们发现在IMS创建的最后，会创建出InputDispatcher和InputReader，InputDispatcher我们前面已经介绍了，主要是用于分发事件；而InputReader是用来获取底层输入事件的，这个我们后面会介绍到。

#### IMS的启动

我们继续来看SystemServer的startCoreServices方法，在创建出IMS实例后，会调用其的start方法来启动服务。

```java
private void startOtherServices(@NonNull TimingsTraceAndSlog t) {
    // 省略若干行
    t.traceBegin("StartInputManager");
    // 将WindowCallback传递给IMS
    inputManager.setWindowManagerCallbacks(wm.getInputManagerCallback());
    // 调用start启动服务
    inputManager.start();
    t.traceEnd();
    // 省略若干行
}
```

start方法中会直接调用native的nativeStart方法来启动native层的IMS。

```java
public void start() {
    Slog.i(TAG, "Starting input manager");
    // 调用native方法来启动底层IMS
    nativeStart(mPtr);
    // 省略若干行
}
```

nativeStart方法中会获取到InputManager，然后调用它的start方法。

```cpp
static void nativeStart(JNIEnv* env, jclass /* clazz */, jlong ptr) {
    NativeInputManager* im = reinterpret_cast<NativeInputManager*>(ptr);
    // 调用InputManager的start方法
    status_t result = im->getInputManager()->start();
    if (result) {
        jniThrowRuntimeException(env, "Input manager could not be started.");
    }
}
```

在InputManager的start方法中，先调用了InputDispatcher的start方法来启动InputDispatcher，然后调用InputReader的start方法启动InputReader。

```cpp
status_t InputManager::start() {
    status_t result = mDispatcher->start();
    if (result) {
        ALOGE("Could not start InputDispatcher thread due to error %d.", result);
        return result;
    }
  
    result = mReader->start();
    if (result) {
        ALOGE("Could not start InputReader due to error %d.", result);
  
        mDispatcher->stop();
        return result;
    }
  
    return OK;
}
```

#### 小结

通过以上IMS的创建和启动过程分析，我们能够看到在IMS的创建和启动都是在SystemServer的startCoreServices方法中触发的，另外在创建的时候也会分别创建出InputDispatcher和InputReader；而且，在调用start方法启动的时候，最终也会触发调用InputDispatcher和InputReader的start方法来启动各自实例。

### InputDispatcher

通过上述分析，知道了InputDispatcher的创建是在IMS创建时创建，那么它是如何启动起来的呢？我们继续看InputDispatcher的start方法。

#### InputDispatcher启动

在InputDispatcher的start方法中，会创建出InputThread线程，并传入了两个函数指针：dispatchOnce以及mLooper->wake。

```cpp
status_t InputDispatcher::start() {
    if (mThread) {
        return ALREADY_EXISTS;
    }
    // 直接构造出Thread，传入两个回调函数
    mThread = std::make_unique<InputThread>(
            "InputDispatcher", [this]() { dispatchOnce(); }, [this]() { mLooper->wake(); });
    return OK;
}
```

继续看InputThread的构造过程，发现初始化列表中对传入的回调函数进行了保存，然后构建InputThreadImpl并调用其run方法将线程启动起来。

```cpp
InputThread::InputThread(std::string name, std::function<void()> loop, std::function<void()> wake)
    // 这里保存wake回调函数
      : mName(name), mThreadWake(wake) {
    // 将loop函数传入InputThreadImpl
    mThread = new InputThreadImpl(loop);
    mThread->run(mName.c_str(), ANDROID_PRIORITY_URGENT_DISPLAY);
}
  
InputThread::~InputThread() {
    mThread->requestExit();
    // 调用wake函数
    if (mThreadWake) {
        mThreadWake();
    }
    mThread->requestExitAndWait();
}
  
class InputThreadImpl : public Thread {
public:
    explicit InputThreadImpl(std::function<void()> loop)
                                            // 保存loop函数
          : Thread(/* canCallJava */ true), mThreadLoop(loop) {}
  
    ~InputThreadImpl() {}
  
private:
    std::function<void()> mThreadLoop;
  
    bool threadLoop() override {
        // 在线程的loop循环中调用了传入的loop函数。
        mThreadLoop();
        // 返回true线程会一直运行，直到requestExit被调用时退出
        return true;
    }
};
```

通过以上分析，我们发现InputThread构造时会创建出线程并将其启动起来，传入的loop函数（dispatchOnce）最终会作为线程的loop来执行，而wake函数（mLooper->wake）也会在InputThread析构时调用。

#### InputDispatcher分发事件

通过前面的介绍，我们了解到在InputDispatcher启动时创建了线程，并且将dispatchOnce作为线程的执行函数传入到InputThread中。所以，当InputDispatcher线程被唤醒后就会执行dispatchOnce方法来分发事件。

![](https://upload-images.jianshu.io/upload_images/26874665-ed9dd1de5139dfca.png?imageMogr2/auto-orient/strip|imageView2/2/w/797/format/webp)

InputDispatcher分发事件

我们继续，在dispatchOnce方法中，首先会判断是否有command需要处理（如：configChanged，focusChanged等），如果有就会调用runCommandsLockedInterruptible方法执行所有command，然后会再次触发wake执行事件的处理；如果没有则直接调用dispatchOnceInnerLocked来处理输入事件；最后，looper会再次进入睡眠等待下一次唤醒。

```cpp
void InputDispatcher::dispatchOnce() {
    nsecs_t nextWakeupTime = LONG_LONG_MAX;
    { // acquire lock
        std::scoped_lock _l(mLock);
        mDispatcherIsAlive.notify_all();
  
        // Run a dispatch loop if there are no pending commands.
        // The dispatch loop might enqueue commands to run afterwards.
        if (!haveCommandsLocked()) {
            // 这里没有command需要处理，就开始分发事件
            dispatchOnceInnerLocked(&nextWakeupTime);
        }
  
        // Run all pending commands if there are any.
        // If any commands were run then force the next poll to wake up immediately.
        // 处理command，并修改nextWakeupTime
        if (runCommandsLockedInterruptible()) {
            nextWakeupTime = LONG_LONG_MIN;
        }
  
        // If we are still waiting for ack on some events,
        // we might have to wake up earlier to check if an app is anr'ing.
        // 检测是否事件分发出现anr
        const nsecs_t nextAnrCheck = processAnrsLocked();
        nextWakeupTime = std::min(nextWakeupTime, nextAnrCheck);
  
        // We are about to enter an infinitely long sleep, because we have no commands or
        // pending or queued events
        if (nextWakeupTime == LONG_LONG_MAX) {
            mDispatcherEnteredIdle.notify_all();
        }
    } // release lock
  
    // Wait for callback or timeout or wake.  (make sure we round up, not down)
    nsecs_t currentTime = now();
    int timeoutMillis = toMillisecondTimeoutDelay(currentTime, nextWakeupTime);
    // 处理完成后调用looper的pollOnce进入睡眠状态，等待下一次唤醒，
    // 如果是处理了command，则这个timeoutMillis为0
    // 所以会接着执行一次loop
    mLooper->pollOnce(timeoutMillis);
}
```

dispatchOnceInnerLocked方法会先判断是否有待分发的事件，没有则从事件队列中取出一个事件；然后根据事件不同的type调用不同的dispatch方法进行事件分发。

```cpp
void InputDispatcher::dispatchOnceInnerLocked(nsecs_t* nextWakeupTime) {
    // 省略若干行
    // Ready to start a new event.
    // If we don't already have a pending event, go grab one.
    if (!mPendingEvent) {
        // 如果没有待分发的事件
        if (mInboundQueue.empty()) {
            // 省略若干行
            // Nothing to do if there is no pending event.
            // 事件队列为空，并且没有待分发的事件，直接返回
            if (!mPendingEvent) {
                return;
            }
        } else {
            // Inbound queue has at least one entry.
            // 从队列中取出一个事件
            mPendingEvent = mInboundQueue.front();
            mInboundQueue.pop_front();
            traceInboundQueueLengthLocked();
        }
        // 省略若干行
    switch (mPendingEvent->type) {
        // 省略若干行
        // 根据事件的type分别进行处理
        case EventEntry::Type::KEY: {
            KeyEntry* typedEntry = static_cast<KeyEntry*>(mPendingEvent);
            // 省略若干行
            // 分发key事件
            done = dispatchKeyLocked(currentTime, typedEntry, &dropReason, nextWakeupTime);
            break;
        }
  
        case EventEntry::Type::MOTION: {
            // 省略若干行
            // 分发motion事件
            done = dispatchMotionLocked(currentTime, typedEntry, &dropReason, nextWakeupTime);
            break;
        }
    }
}
```

这里以key事件为例继续介绍，dispatchKeyLocked中首先通过findFocusedWindowTargetsLocked方法查找到焦点窗口，然后调用dispatchEventLocked朝焦点窗口上分发事件。

```cpp
bool InputDispatcher::dispatchKeyLocked(nsecs_t currentTime, KeyEntry* entry,
                                        DropReason* dropReason, nsecs_t* nextWakeupTime) {
    // 省略若干行
    // Identify targets.
    std::vector<InputTarget> inputTargets;
    // 查找focus window
    int32_t injectionResult =
            findFocusedWindowTargetsLocked(currentTime, *entry, inputTargets, nextWakeupTime);
    // 省略若干行
    // 将事件分发到对应window上去
    dispatchEventLocked(currentTime, entry, inputTargets);
    return true;
}
```

dispatchEventLocked方法中会遍历所有查找到的focus窗口（inputTarget），然后通过inputChannel获取到链接对象connection，最后通过prepareDispatchCycleLocked将事件分发出去。

```cpp
void InputDispatcher::dispatchEventLocked(nsecs_t currentTime, EventEntry* eventEntry,
                                          const std::vector<InputTarget>& inputTargets) {
    // 省略若干行
    // 遍历所有的inputTarget
    for (const InputTarget& inputTarget : inputTargets) {
        // 通过inputChannel获取connection
        sp<Connection> connection =
                getConnectionLocked(inputTarget.inputChannel->getConnectionToken());
        if (connection != nullptr) {
            // 开始事件分发
            prepareDispatchCycleLocked(currentTime, connection, eventEntry, inputTarget);
        }
        // 省略若干行
    }
}
```

prepareDispatchCycleLocked方法中先判断是否需要split motion事件并进行处理，最后调用enqueueDispatchEntriesLocked方法将待分发的事件添加到mOutboundQueue队列中。

```cpp
void InputDispatcher::prepareDispatchCycleLocked(nsecs_t currentTime,
                                                 const sp<Connection>& connection,
                                                 EventEntry* eventEntry,
                                                 const InputTarget& inputTarget) {
    // 省略若干行
    // Split a motion event if needed.
    // 如果需要split motion事件则进行处理
    if (inputTarget.flags & InputTarget::FLAG_SPLIT) {
        LOG_ALWAYS_FATAL_IF(eventEntry->type != EventEntry::Type::MOTION,
                            "Entry type %s should not have FLAG_SPLIT",
                            EventEntry::typeToString(eventEntry->type));
  
        const MotionEntry& originalMotionEntry = static_cast<const MotionEntry&>(*eventEntry);
        if (inputTarget.pointerIds.count() != originalMotionEntry.pointerCount) {
            // 省略若干行
            enqueueDispatchEntriesLocked(currentTime, connection, splitMotionEntry, inputTarget);
            splitMotionEntry->release();
            return;
        }
    }
  
    // Not splitting.  Enqueue dispatch entries for the event as is.
    // 将将要分发的事件入队
    enqueueDispatchEntriesLocked(currentTime, connection, eventEntry, inputTarget);
}
```

enqueueDispatchEntriesLocked方法中会分别处理不同的flag对应的事件将其添加到outboundQueue中，最后通过调用startDispatchCycleLocked开始事件的分发。

```cpp
void InputDispatcher::enqueueDispatchEntriesLocked(nsecs_t currentTime,
                                                   const sp<Connection>& connection,
                                                   EventEntry* eventEntry,
                                                   const InputTarget& inputTarget) {
    // 省略若干行
  
    bool wasEmpty = connection->outboundQueue.empty();
    // 分别处理不同flag对应的event，并将其添加到outboundQueue队列中
    // Enqueue dispatch entries for the requested modes.
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
                               InputTarget::FLAG_DISPATCH_AS_HOVER_EXIT);
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
                               InputTarget::FLAG_DISPATCH_AS_OUTSIDE);
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
                               InputTarget::FLAG_DISPATCH_AS_HOVER_ENTER);
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
                               InputTarget::FLAG_DISPATCH_AS_IS);
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
                               InputTarget::FLAG_DISPATCH_AS_SLIPPERY_EXIT);
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
                               InputTarget::FLAG_DISPATCH_AS_SLIPPERY_ENTER);
  
    // If the outbound queue was previously empty, start the dispatch cycle going.
    if (wasEmpty && !connection->outboundQueue.empty()) {
        // 如果有event被添加到队列则开始处理
        startDispatchCycleLocked(currentTime, connection);
    }
}
```

startDispatchCycleLocked方法中通过遍历outboundQueue队列，取出所有的event，然后根据其type分别调用InputPublisher的publishXxxEvent将事件分发出去。

```cpp
void InputDispatcher::startDispatchCycleLocked(nsecs_t currentTime,
                                               const sp<Connection>& connection) {
    // 省略若干行
    // 循环遍历outboundQueue队列
    while (connection->status == Connection::STATUS_NORMAL && !connection->outboundQueue.empty()) {
        // 从outboundQueue队列取出事件
        DispatchEntry* dispatchEntry = connection->outboundQueue.front();
        dispatchEntry->deliveryTime = currentTime;
        const nsecs_t timeout =
                getDispatchingTimeoutLocked(connection->inputChannel->getConnectionToken());
        dispatchEntry->timeoutTime = currentTime + timeout;
  
        // Publish the event.
        status_t status;
        EventEntry* eventEntry = dispatchEntry->eventEntry;
        // 根据event的不同type分别进行分发
        switch (eventEntry->type) {
            case EventEntry::Type::KEY: {
                const KeyEntry* keyEntry = static_cast<KeyEntry*>(eventEntry);
                std::array<uint8_t, 32> hmac = getSignature(*keyEntry, *dispatchEntry);
  
                // Publish the key event.
                // 分发key事件
                status =
                        connection->inputPublisher
                                .publishKeyEvent(dispatchEntry->seq, dispatchEntry->resolvedEventId,
                                                 keyEntry->deviceId, keyEntry->source,
                                                 keyEntry->displayId, std::move(hmac),
                                                 dispatchEntry->resolvedAction,
                                                 dispatchEntry->resolvedFlags, keyEntry->keyCode,
                                                 keyEntry->scanCode, keyEntry->metaState,
                                                 keyEntry->repeatCount, keyEntry->downTime,
                                                 keyEntry->eventTime);
                break;
            }
  
            case EventEntry::Type::MOTION: {
                MotionEntry* motionEntry = static_cast<MotionEntry*>(eventEntry);
  
                PointerCoords scaledCoords[MAX_POINTERS];
                const PointerCoords* usingCoords = motionEntry->pointerCoords;
                // 省略若干行
                // 分发motion事件
                // Publish the motion event.
                status = connection->inputPublisher
                                 .publishMotionEvent(dispatchEntry->seq,
                                                     dispatchEntry->resolvedEventId,
                                                     motionEntry->deviceId, motionEntry->source,
                                                     motionEntry->displayId, std::move(hmac),
                                                     dispatchEntry->resolvedAction,
                                                     motionEntry->actionButton,
                                                     dispatchEntry->resolvedFlags,
                                                     motionEntry->edgeFlags, motionEntry->metaState,
                                                     motionEntry->buttonState,
                                                     motionEntry->classification, xScale, yScale,
                                                     xOffset, yOffset, motionEntry->xPrecision,
                                                     motionEntry->yPrecision,
                                                     motionEntry->xCursorPosition,
                                                     motionEntry->yCursorPosition,
                                                     motionEntry->downTime, motionEntry->eventTime,
                                                     motionEntry->pointerCount,
                                                     motionEntry->pointerProperties, usingCoords);
                reportTouchEventForStatistics(*motionEntry);
                break;
            }
        // 省略若干行
    }
}
```

这里以key事件为例继续介绍，在publishKeyEvent方法中，首先会根据传入的event详细信息构建出InputMessage，然后再调用InputChannel的sendMessage方法将msg发送出去。

```cpp
status_t InputPublisher::publishKeyEvent(uint32_t seq, int32_t eventId, int32_t deviceId,
                                         int32_t source, int32_t displayId,
                                         std::array<uint8_t, 32> hmac, int32_t action,
                                         int32_t flags, int32_t keyCode, int32_t scanCode,
                                         int32_t metaState, int32_t repeatCount, nsecs_t downTime,
                                         nsecs_t eventTime) {
    // 省略若干行
    // 根据event信息构建InputMessage
    InputMessage msg;
    msg.header.type = InputMessage::Type::KEY;
    msg.body.key.seq = seq;
    msg.body.key.eventId = eventId;
    msg.body.key.deviceId = deviceId;
    msg.body.key.source = source;
    msg.body.key.displayId = displayId;
    msg.body.key.hmac = std::move(hmac);
    msg.body.key.action = action;
    msg.body.key.flags = flags;
    msg.body.key.keyCode = keyCode;
    msg.body.key.scanCode = scanCode;
    msg.body.key.metaState = metaState;
    msg.body.key.repeatCount = repeatCount;
    msg.body.key.downTime = downTime;
    msg.body.key.eventTime = eventTime;
    // 通过InputChannel的sendMessage方法将event发送出去
    return mChannel->sendMessage(&msg);
}
```

sendMessage主要就是先copy一份事件msg，然后调用send将msg循环写入socket，从而实现输入事件的分发。

```cpp
status_t InputChannel::sendMessage(const InputMessage* msg) {
    const size_t msgLength = msg->size();
    InputMessage cleanMsg;
    // copy一份msg
    msg->getSanitizedCopy(&cleanMsg);
    ssize_t nWrite;
    do {
        // 通过socket循环写入msg
        nWrite = ::send(mFd.get(), &cleanMsg, msgLength, MSG_DONTWAIT | MSG_NOSIGNAL);
    } while (nWrite == -1 && errno == EINTR);
    // 省略若干行
    return OK;
}
```

### InputReader（InputDispatcher事件的来源）

InputDispatcher中的事件是从InputReader中来的，InputReader从EventHub中获取到输入事件后，会通过调用InputDispatcher的notifyXxx方法来将事件传递到InuptDispatcher中。

#### InputReader启动

在IMS的start方法中会调用InputReader的start方法来启动InputReader，我们继续看InputReader的start方法。在start方法中，会创建出InputThread线程，这里注意，创建线程时传入了两个函数指针（laumda表达式）：loopOnce和mEventHub->wake。通过上面对InputThread的介绍，我们知道最终，loopOnce会作为线程的循环方法进行调用，而mEventHub->wake最终也会在线程析构时触发。

```cpp
status_t InputReader::start() {
    if (mThread) {
        return ALREADY_EXISTS;
    }
    // 直接构造出Thread，传入两个回调函数
    mThread = std::make_unique<InputThread>(
            "InputReader", [this]() { loopOnce(); }, [this]() { mEventHub->wake(); });
    return OK;
}
```

#### InputReader处理事件

InputReader在其线程的threadLoop中会调用loopOnce从EventHub中获取输入事件，如果获取到事件，则继续调用processEventsLocked进行处理。接着会调用到InputDevice -> InputMapper -> InputDispatcher(InputListenerInterface)，在InputDispatcher中触发notifyXxx方法，从而将事件分发出去。

![](https://upload-images.jianshu.io/upload_images/26874665-001ccfdaf87dcc01.png?imageMogr2/auto-orient/strip|imageView2/2/w/696/format/webp)

InputReader处理事件

```cpp
void InputReader::loopOnce() {
    int32_t oldGeneration;
    int32_t timeoutMillis;
    bool inputDevicesChanged = false;
    std::vector<InputDeviceInfo> inputDevices;
    // 省略若干行
    // 从EventHub中获取事件
    size_t count = mEventHub->getEvents(timeoutMillis, mEventBuffer, EVENT_BUFFER_SIZE);
  
    { // acquire lock
        AutoMutex _l(mLock);
        mReaderIsAliveCondition.broadcast();
        // 获取到输入事件则调用processEventsLocked进行处理
        if (count) {
            processEventsLocked(mEventBuffer, count);
        }
    // 省略若干行
}
```

processEventsLocked方法中会根据事件的type，分别处理device的变更事件以及输入事件。输入事件则继续调用processEventsForDeviceLocked来处理，device改变则同步改变mDevices。

```cpp
void InputReader::processEventsLocked(const RawEvent* rawEvents, size_t count) {
    for (const RawEvent* rawEvent = rawEvents; count;) {
        int32_t type = rawEvent->type;
        size_t batchSize = 1;
        if (type < EventHubInterface::FIRST_SYNTHETIC_EVENT) {
            // 省略若干行
            // 这里事件类型如果不是device change事件则继续处理
            processEventsForDeviceLocked(deviceId, rawEvent, batchSize);
        } else {
            // device change事件
            switch (rawEvent->type) {
                case EventHubInterface::DEVICE_ADDED:
                    // device接入，将device添加到全局map中（mDevices）
                    addDeviceLocked(rawEvent->when, rawEvent->deviceId);
                    break;
                case EventHubInterface::DEVICE_REMOVED: // device断开
                    removeDeviceLocked(rawEvent->when, rawEvent->deviceId);
                    break;
                case EventHubInterface::FINISHED_DEVICE_SCAN: // device scan
                    handleConfigurationChangedLocked(rawEvent->when);
                    break;
                default:
                    ALOG_ASSERT(false); // can't happen
                    break;
            }
        }
        count -= batchSize;
        rawEvent += batchSize;
    }
}
```

processEventsForDeviceLocked中从device的map中根据eventHubId查找device，如果找到则调用对应device的process方法继续处理。

```cpp
void InputReader::processEventsForDeviceLocked(int32_t eventHubId, const RawEvent* rawEvents,
                                               size_t count) {
    // 通过eventHubId从map中查找InputDevice
    auto deviceIt = mDevices.find(eventHubId);
    if (deviceIt == mDevices.end()) {
        // 没有对应的device则直接返回
        ALOGW("Discarding event for unknown eventHubId %d.", eventHubId);
        return;
    }
  
    std::shared_ptr<InputDevice>& device = deviceIt->second;
    // device被忽略则返回
    if (device->isIgnored()) {
        // ALOGD("Discarding event for ignored deviceId %d.", deviceId);
        return;
    }
    // 调用InputDevice的process继续处理事件
    device->process(rawEvents, count);
}
```

InputDevice的process中会遍历所有的event，并且根据event中的deviceId从mDevices中找到对应的device，然后遍历其所有的InputMapper，并调用mapper的process进行事件处理。

```cpp
void InputDevice::process(const RawEvent* rawEvents, size_t count) {
    // Process all of the events in order for each mapper.
    // We cannot simply ask each mapper to process them in bulk because mappers may
    // have side-effects that must be interleaved.  For example, joystick movement events and
    // gamepad button presses are handled by different mappers but they should be dispatched
    // in the order received.
    for (const RawEvent* rawEvent = rawEvents; count != 0; rawEvent++) {
        // 省略若干行
        // 从devices中找到对应的device，然后遍历其所有inputMapper，并调用其process方法进行处理
        for_each_mapper_in_subdevice(rawEvent->deviceId, [rawEvent](InputMapper& mapper) {
            mapper.process(rawEvent);
        });
        --count;
    }
}
  
inline void for_each_mapper_in_subdevice(int32_t eventHubDevice,
                                            std::function<void(InputMapper&)> f) {
    auto deviceIt = mDevices.find(eventHubDevice);
    // 查找对应的device
    if (deviceIt != mDevices.end()) {
        auto& devicePair = deviceIt->second;
        auto& mappers = devicePair.second;
        // 遍历该device的所有InputMapper，并调用函数指针f
        for (auto& mapperPtr : mappers) {
            f(*mapperPtr);
        }
    }
}
```

InputMapper在InputReader中处理device接入事件触发时会调用addDeviceLocked方法，然后会调用到createDeviceLocked方法来创建出对应的InputDevice，创建出device后，便调用它的addEventHubDevice来创建出相应的InputMapper并添加到全局map中。

```cpp
void InputReader::addDeviceLocked(nsecs_t when, int32_t eventHubId) {
    // 根据eventHubId查找device
    if (mDevices.find(eventHubId) != mDevices.end()) {
        ALOGW("Ignoring spurious device added event for eventHubId %d.", eventHubId);
        return;
    }
  
    InputDeviceIdentifier identifier = mEventHub->getDeviceIdentifier(eventHubId);
    // 创建device
    std::shared_ptr<InputDevice> device = createDeviceLocked(eventHubId, identifier);
    // 省略若干行
}
  
std::shared_ptr<InputDevice> InputReader::createDeviceLocked(
        int32_t eventHubId, const InputDeviceIdentifier& identifier) {
    // 省略若干行
    std::shared_ptr<InputDevice> device;
    if (deviceIt != mDevices.end()) {
        // 如果device已经存在则直接返回
        device = deviceIt->second;
    } else {
        // 否则创建出对应的InputDevice
        int32_t deviceId = (eventHubId < END_RESERVED_ID) ? eventHubId : nextInputDeviceIdLocked();
        device = std::make_shared<InputDevice>(&mContext, deviceId, bumpGenerationLocked(),
                                               identifier);
    }
    // 调用addEventHubDevice，构建出相应的mapper
    device->addEventHubDevice(eventHubId);
    return device;
}
```

通过addEventHubDevice方法，可以看出针对不同的device类型，会构建出不同的mapper，最后将mapper数组添加到了mDevices的全局map中。后面我们以KeyboardInputMapper为例介绍key事件的传递过程。

```cpp
void InputDevice::addEventHubDevice(int32_t eventHubId, bool populateMappers) {
    if (mDevices.find(eventHubId) != mDevices.end()) {
        return;
    }
    std::unique_ptr<InputDeviceContext> contextPtr(new InputDeviceContext(*this, eventHubId));
    uint32_t classes = contextPtr->getDeviceClasses();
    std::vector<std::unique_ptr<InputMapper>> mappers;
  
    // Check if we should skip population
    if (!populateMappers) {
        mDevices.insert({eventHubId, std::make_pair(std::move(contextPtr), std::move(mappers))});
        return;
    }
  
    // Switch-like devices.
    if (classes & INPUT_DEVICE_CLASS_SWITCH) {
        mappers.push_back(std::make_unique<SwitchInputMapper>(*contextPtr));
    }
  
    // Scroll wheel-like devices.
    if (classes & INPUT_DEVICE_CLASS_ROTARY_ENCODER) {
        mappers.push_back(std::make_unique<RotaryEncoderInputMapper>(*contextPtr));
    }
  
    // Vibrator-like devices.
    if (classes & INPUT_DEVICE_CLASS_VIBRATOR) {
        mappers.push_back(std::make_unique<VibratorInputMapper>(*contextPtr));
    }
  
    // Keyboard-like devices.
    uint32_t keyboardSource = 0;
    int32_t keyboardType = AINPUT_KEYBOARD_TYPE_NON_ALPHABETIC;
    if (classes & INPUT_DEVICE_CLASS_KEYBOARD) {
        keyboardSource |= AINPUT_SOURCE_KEYBOARD;
    }
    if (classes & INPUT_DEVICE_CLASS_ALPHAKEY) {
        keyboardType = AINPUT_KEYBOARD_TYPE_ALPHABETIC;
    }
    if (classes & INPUT_DEVICE_CLASS_DPAD) {
        keyboardSource |= AINPUT_SOURCE_DPAD;
    }
    if (classes & INPUT_DEVICE_CLASS_GAMEPAD) {
        keyboardSource |= AINPUT_SOURCE_GAMEPAD;
    }
  
    if (keyboardSource != 0) {
        mappers.push_back(
                std::make_unique<KeyboardInputMapper>(*contextPtr, keyboardSource, keyboardType));
    }
  
    // Cursor-like devices.
    if (classes & INPUT_DEVICE_CLASS_CURSOR) {
        mappers.push_back(std::make_unique<CursorInputMapper>(*contextPtr));
    }
  
    // Touchscreens and touchpad devices.
    if (classes & INPUT_DEVICE_CLASS_TOUCH_MT) {
        mappers.push_back(std::make_unique<MultiTouchInputMapper>(*contextPtr));
    } else if (classes & INPUT_DEVICE_CLASS_TOUCH) {
        mappers.push_back(std::make_unique<SingleTouchInputMapper>(*contextPtr));
    }
  
    // Joystick-like devices.
    if (classes & INPUT_DEVICE_CLASS_JOYSTICK) {
        mappers.push_back(std::make_unique<JoystickInputMapper>(*contextPtr));
    }
  
    // External stylus-like devices.
    if (classes & INPUT_DEVICE_CLASS_EXTERNAL_STYLUS) {
        mappers.push_back(std::make_unique<ExternalStylusInputMapper>(*contextPtr));
    }
  
    // insert the context into the devices set
    mDevices.insert({eventHubId, std::make_pair(std::move(contextPtr), std::move(mappers))});
}
```

回到InputDevice的process方法中，循环遍历了所有的mapper并调用其process方法，这里以KeyboardInputMapper来介绍key事件的处理过程。

```cpp
void KeyboardInputMapper::process(const RawEvent* rawEvent) {
    switch (rawEvent->type) {
        // 如果是key事件
        case EV_KEY: {
            int32_t scanCode = rawEvent->code;
            int32_t usageCode = mCurrentHidUsage;
            mCurrentHidUsage = 0;
            // 如果code为keyboard或者游戏面板对应的key
            if (isKeyboardOrGamepadKey(scanCode)) {
                processKey(rawEvent->when, rawEvent->value != 0, scanCode, usageCode);
            }
            break;
        }
        // 省略若干行
    }
}
```

processKey方法中，会根据event是否为down以及event的其他属性构建出NotifyKeyArgs，然后通过getListener方法获取到InputListener，并通过其notifyKey方法将事件传递到InputDispatcher中。

```cpp
void KeyboardInputMapper::processKey(nsecs_t when, bool down, int32_t scanCode, int32_t usageCode) {
    int32_t keyCode;
    int32_t keyMetaState;
    uint32_t policyFlags;
    // 省略若干行
    // 根据event内容构建相应的args
    NotifyKeyArgs args(getContext()->getNextId(), when, getDeviceId(), mSource, getDisplayId(),
                       policyFlags, down ? AKEY_EVENT_ACTION_DOWN : AKEY_EVENT_ACTION_UP,
                       AKEY_EVENT_FLAG_FROM_SYSTEM, keyCode, scanCode, keyMetaState, downTime);
    // 获取InputListener，并调用其notifyKey方法传递key事件
    getListener()->notifyKey(&args);
}
```

notifyKey方法中首先会构建出KeyEvent事件对象，并通过IMS传递到Java层的interceptKeyBeforeQueueing方法；然后根据args构建KeyEnvtry，并将其添加到mInboundQueue队列中；最后调用wake方法唤醒looper。

```cpp
void InputDispatcher::notifyKey(const NotifyKeyArgs* args) {
    // 省略若干行
    // 根据args构建KeyEvent
    KeyEvent event;
    event.initialize(args->id, args->deviceId, args->source, args->displayId, INVALID_HMAC,
                     args->action, flags, keyCode, args->scanCode, metaState, repeatCount,
                     args->downTime, args->eventTime);
  
    android::base::Timer t;
    // 调用IMS的interceptKeyBeforeQueueing
    mPolicy->interceptKeyBeforeQueueing(&event, /*byref*/ policyFlags);
    // 省略若干行
    // 构建KeyEntry
    KeyEntry* newEntry =
            new KeyEntry(args->id, args->eventTime, args->deviceId, args->source,
                            args->displayId, policyFlags, args->action, flags, keyCode,
                            args->scanCode, metaState, repeatCount, args->downTime);
    // 将KeyEntry添加到mInboundQueue里面
    needWake = enqueueInboundEventLocked(newEntry);
    // 省略若干行
    // 如果需要wake则唤醒looper
    if (needWake) {
        mLooper->wake();
    }
}
```

enqueueInboundEventLocked方法中会将EventEntry添加到mInboundQueue队列中，然后如果需要wake就唤醒looper，然后就会触发threadLoop，从而调用dispatchOnce方法回到InputDispatcher中分发事件。

```cpp
bool InputDispatcher::enqueueInboundEventLocked(EventEntry* entry) {
    bool needWake = mInboundQueue.empty();
    // 将EventEntry添加到mInboundQueue队列中
    mInboundQueue.push_back(entry);
    traceInboundQueueLengthLocked();
    // 省略若干行
    return needWake;
}
```

### EventHub

通过前面InputReader的介绍，我们发现输入事件的源头是通过调用EventHub的getEvents方法获取的。那么，EventHub是如何创建以及进行事件的获取的呢？

![](https://upload-images.jianshu.io/upload_images/26874665-e4d840ae87a2f356.png?imageMogr2/auto-orient/strip|imageView2/2/w/420/format/webp)

EventHub获取事件

#### EventHub的创建

我们回到InputReader的构造方法，发现在InputReader构造方法的初始化列表中，会赋值全局变量mEventHub。

```cpp
InputReader::InputReader(std::shared_ptr<EventHubInterface> eventHub,
                         const sp<InputReaderPolicyInterface>& policy,
                         const sp<InputListenerInterface>& listener)
      : mContext(this),
      // 初始化mEventHub
        mEventHub(eventHub),
        mPolicy(policy),
        mGlobalMetaState(0),
        mGeneration(1),
        mNextInputDeviceId(END_RESERVED_ID),
        mDisableVirtualKeysTimeout(LLONG_MIN),
        mNextTimeout(LLONG_MAX),
        mConfigurationChangesToRefresh(0) {
    mQueuedListener = new QueuedInputListener(listener);
    { // acquire lock
        AutoMutex _l(mLock);
  
        refreshConfigurationLocked(0);
        updateGlobalMetaStateLocked();
    } // release lock
}
```

在初始化列表中对全局变量mEventHub进行了初始化，通过前面介绍我们知道，InputReader是在InputManager中构建出来的，那么我们继续看。

```cpp
InputManager::InputManager(
        const sp<InputReaderPolicyInterface>& readerPolicy,
        const sp<InputDispatcherPolicyInterface>& dispatcherPolicy) {
    mDispatcher = createInputDispatcher(dispatcherPolicy);
    mClassifier = new InputClassifier(mDispatcher);
    // 通过createInputReader创建出InputReader
    mReader = createInputReader(readerPolicy, mClassifier);
}
  
sp<InputReaderInterface> createInputReader(const sp<InputReaderPolicyInterface>& policy,
                                           const sp<InputListenerInterface>& listener) {
                          // 这里直接通过std::make_unique构建出EventHub实例
    return new InputReader(std::make_unique<EventHub>(), policy, listener);
}
```

在InputManager中通过createInputReader构建出InputReader实例，而在createInputReader方法中，会首先通过std::make_unique构建出EventHub实例，继续看EventHub的构造函数。

```cpp
EventHub::EventHub(void)
      : mBuiltInKeyboardId(NO_BUILT_IN_KEYBOARD),
        mNextDeviceId(1),
        mControllerNumbers(),
        mOpeningDevices(nullptr),
        mClosingDevices(nullptr),
        mNeedToSendFinishedDeviceScan(false),
        mNeedToReopenDevices(false),
        mNeedToScanDevices(true),
        mPendingEventCount(0),
        mPendingEventIndex(0),
        mPendingINotify(false) {
    ensureProcessCanBlockSuspend();
    // 创建出epoll
    mEpollFd = epoll_create1(EPOLL_CLOEXEC);
    LOG_ALWAYS_FATAL_IF(mEpollFd < 0, "Could not create epoll instance: %s", strerror(errno));
    // 创建inotify
    mINotifyFd = inotify_init();
    // 通过inotify来监听DEVICE_PATH路径（/dev/input)下的文件改变（增加或者删除）
    mInputWd = inotify_add_watch(mINotifyFd, DEVICE_PATH, IN_DELETE | IN_CREATE);
    // 省略若干行
    struct epoll_event eventItem = {};
    eventItem.events = EPOLLIN | EPOLLWAKEUP;
    eventItem.data.fd = mINotifyFd;
    // 将mINotifyFd添加到epoll中
    int result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mINotifyFd, &eventItem);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not add INotify to epoll instance.  errno=%d", errno);
    // 创建管道
    int wakeFds[2];
    result = pipe(wakeFds);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not create wake pipe.  errno=%d", errno);
  
    mWakeReadPipeFd = wakeFds[0];
    mWakeWritePipeFd = wakeFds[1];
    // 设置管道读取端非阻塞属性
    result = fcntl(mWakeReadPipeFd, F_SETFL, O_NONBLOCK);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not make wake read pipe non-blocking.  errno=%d",
                        errno);
    // 设置管道写入端非阻塞属性
    result = fcntl(mWakeWritePipeFd, F_SETFL, O_NONBLOCK);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not make wake write pipe non-blocking.  errno=%d",
                        errno);
  
    eventItem.data.fd = mWakeReadPipeFd;
    // 将读取端管道描述符添加到epoll
    result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mWakeReadPipeFd, &eventItem);
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not add wake read pipe to epoll instance.  errno=%d",
                        errno);
}
```

在EventHub的构造方法中，我们可以看到：首先会创建出epoll，然后创建出inotify来监听/dev/input路径下文件的增删，并将inotify添加到epoll中进行监听，还会创建出一个管道，并将管道读取端也添加到epoll中。这样当有新的输入设备接入或者删除事，就会触发唤醒epoll进行处理。

#### EventHub如何获取输入事件

在上面介绍的InputReader中，我们了解到loopOnce方法中通过调用EventHub的getEvents来获取输入事件。那么，我们继续看getEvents方法，此方法比较长，我们先看下大概的框架。

```cpp
size_t EventHub::getEvents(int timeoutMillis, RawEvent* buffer, size_t bufferSize) {
    for (;;) {
        // Reopen input devices if needed.
        if (mNeedToReopenDevices) {
            // 如果存在需要reopen的设备，则先关闭所有device
            // 然后设置需要scan设备的标识
        }
  
        // Report any devices that had last been added/removed.
        while (mClosingDevices) {
            // 如果存在需要关闭的设备，则遍历所有需要关闭的设备链表，
            // 删除对应的device，并构建event
        }
        // 需要扫描device，则调用scanDevicesLocked方法扫描
        // 最后更新device列表
        if (mNeedToScanDevices) {
        }
        //存在需要open的device，则更新mOpeningDevices链表
        // 并构建event
        while (mOpeningDevices != nullptr) {
        }
        // 需要scanFinish事件，则构建对应event
        if (mNeedToSendFinishedDeviceScan) {
        }
  
        // Grab the next input event.
        // 遍历需要处理的事件列表
        while (mPendingEventIndex < mPendingEventCount) {
            const struct epoll_event& eventItem = mPendingEventItems[mPendingEventIndex++];
            if (eventItem.data.fd == mINotifyFd) {
                // 如果是inotify事件，则修改对应标识，后面会扫描处理对于的变更
            }
  
            if (eventItem.data.fd == mWakeReadPipeFd) {
                // 管道事件，则设置wake为true，跳出循环继续执行
            }
            // This must be an input event
            if (eventItem.events & EPOLLIN) {
                // 真正的输入事件
            }
        }
        // 开始wait时释放锁
        mLock.unlock(); // release lock before poll
        // epoll等待唤醒
        int pollResult = epoll_wait(mEpollFd, mPendingEventItems, EPOLL_MAX_EVENTS, timeoutMillis);
        // 唤醒开始执行时则加锁
        mLock.lock(); // reacquire lock after poll
    }
  
    // All done, return the number of events we read.
    return event - buffer;
}
```

通过以上getEvents方法的大致流程，能够看到首先会查看是否有需要reopen的device并进行处理，接着处理需要close的device，然后是判断是否需要扫描设备并进行device扫描；接着处理新接入的设备，然后开始遍历待处理的事件，并分别处理inotify、管道以及真正的输入事件；过程中如果有event被处理则就会break掉for循环继续进行下一次处理，如果所有事件都已处理完就会走到下面的epoll_wait进入wait状态等待唤醒。

##### EventHub处理reopen设备

```cpp
// Reopen input devices if needed.
if (mNeedToReopenDevices) {
    // 设置mNeedToReopenDevices为false，避免下次循环继续处理
    mNeedToReopenDevices = false;
  
    ALOGI("Reopening all input devices due to a configuration change.");
    // 关闭所有device
    closeAllDevicesLocked();
    // 标识需要扫描device，后面循环会进行扫描设备
    mNeedToScanDevices = true;
    // 跳出for循环，继续后续处理
    break; // return to the caller before we actually rescan
}
```

处理reopen设备，首先是重置reopen标识，然后调用closeAllDevicesLocked来关闭所有的device，接着标识设备需要扫描，最后break退出此次循环，继续下一次循环处理。继续看closeAllDevicesLocked方法：

```cpp
void EventHub::closeAllDevicesLocked() {
    mUnattachedVideoDevices.clear();
    while (mDevices.size() > 0) {
        // 循环遍历所有device，并调用closeDeviceLocked来进行关闭
        closeDeviceLocked(mDevices.valueAt(mDevices.size() - 1));
    }
}
  
void EventHub::closeDeviceLocked(Device* device) {
    // 省略若干行
    // 从epoll移除此device的监听
    unregisterDeviceFromEpollLocked(device);
    // 省略若干行
    // 从device列表中移除此设备
    mDevices.removeItem(device->id);
    // 关闭device
    device->close();
  
    // Unlink for opening devices list if it is present.
    Device* pred = nullptr;
    bool found = false;
    // 从已经打开的device列表中查找对应的device
    for (Device* entry = mOpeningDevices; entry != nullptr;) {
        if (entry == device) {
            found = true;
            break;
        }
        pred = entry;
        entry = entry->next;
    }
    // 如果找到，则从打开的device列表中将其移除
    if (found) {
        // Unlink the device from the opening devices list then delete it.
        // We don't need to tell the client that the device was closed because
        // it does not even know it was opened in the first place.
        ALOGI("Device %s was immediately closed after opening.", device->path.c_str());
        if (pred) {
            pred->next = device->next;
        } else {
            mOpeningDevices = device->next;
        }
        // 删除对应device
        delete device;
    } else {
        // Link into closing devices list.
        // The device will be deleted later after we have informed the client.
        // 打开的device列表中没找到，则将device添加到待移除的设备列表中
        device->next = mClosingDevices;
        mClosingDevices = device;
    }
}
```

##### EventHub处理close设备

关闭device时首先从epoll中删除对应的监听并从device列表中将其移除，然后在已经打开的device列表中查找，如果找到则将其从open的device列表中移除，否则就将其添加到close的device列表中去，后面会处理close列表。这里我们继续看下epoll移除device的unregisterDeviceFromEpollLocked方法：

```cpp
status_t EventHub::unregisterDeviceFromEpollLocked(Device* device) {
    if (device->hasValidFd()) {
        // 如果设备存在有效的fd，则调用unregisterFdFromEpoll将其从epoll中移除
        status_t result = unregisterFdFromEpoll(device->fd);
    }
    return OK;
}
  
status_t EventHub::unregisterFdFromEpoll(int fd) {
    // 调用epoll_ctl并传递EPOLL_CTL_DEL的flag将fd从epoll中移除
    if (epoll_ctl(mEpollFd, EPOLL_CTL_DEL, fd, nullptr)) {
        ALOGW("Could not remove fd from epoll instance: %s", strerror(errno));
        return -errno;
    }
    return OK;
}
```

接着我们继续看getEvents中对带关闭设备的处理过程：

```cpp
// Report any devices that had last been added/removed.
// 遍历所有需要关闭的device链表
while (mClosingDevices) {
    Device* device = mClosingDevices;
    ALOGV("Reporting device closed: id=%d, name=%s\n", device->id, device->path.c_str());
    // 移动表头到到下一个位置
    mClosingDevices = device->next;
    // 构建设备移除的event
    event->when = now;
    event->deviceId = (device->id == mBuiltInKeyboardId)
            ? ReservedInputDeviceId::BUILT_IN_KEYBOARD_ID
            : device->id;
    event->type = DEVICE_REMOVED;
    event += 1;
    // 删除对于device
    delete device;
    // 标识需要构建扫描完成的条件
    mNeedToSendFinishedDeviceScan = true;
}
```

处理关闭的device，首先是遍历整个需要关闭的device链表，并依次对每一个device，构造设备移除的event，然后删除对应的device，最后标识需要构建扫描完成的事件条件，待后面添加扫描完成的event。

##### EventHub扫描设备

前面如果有处理reopen设备，则会关闭所有设备，并设置需要扫描设备的标识，然后这里会调用scanDevicesLocked方法来扫描device。

```cpp
if (mNeedToScanDevices) {
    // 重置需要扫描的标识，避免下一次继续扫描设备
    mNeedToScanDevices = false;
    // 开始扫描设备
    scanDevicesLocked();
    // 标识后面需要有扫描完成的event
    mNeedToSendFinishedDeviceScan = true;
}
```

扫描设备会调用scanDevicesLocked方法进行扫描处理，我们继续看：

```cpp
void EventHub::scanDevicesLocked() {
    // 继续调用scanDirLocked来扫描设备，这里传入的路径为/dev/input
    status_t result = scanDirLocked(DEVICE_PATH);
    if (result < 0) {
        ALOGE("scan dir failed for %s", DEVICE_PATH);
    }
    // 省略若干行
    // 如果存在虚拟的键盘，则在这里创建虚拟键盘
    if (mDevices.indexOfKey(ReservedInputDeviceId::VIRTUAL_KEYBOARD_ID) < 0) {
        createVirtualKeyboardLocked();
    }
}
```

首先调用scanDirLocked扫描/dev/input目录来索引可用的device，然后判断如果存在虚拟的键盘，则调用createVirtualKeyboardLocked方法来创建虚拟键盘设备。

```cpp
status_t EventHub::scanDirLocked(const char* dirname) {
    char devname[PATH_MAX];
    char* filename;
    DIR* dir;
    struct dirent* de;
    // 打开/dev/input目录
    dir = opendir(dirname);
    if (dir == nullptr) return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    // 遍历/dev/input目录
    while ((de = readdir(dir))) {
        if (de->d_name[0] == '.' &&
            (de->d_name[1] == '\0' || (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        strcpy(filename, de->d_name);
        // 如果文件名有效，则打开对于device
        openDeviceLocked(devname);
    }
    // 关闭目录
    closedir(dir);
    return 0;
}
```

扫描device主要是扫描/dev/input目录，遍历每一个设备文件并调用openDeviceLocked方法来打开对应的device，最后关闭目录。

```cpp
status_t EventHub::openDeviceLocked(const char* devicePath) {
    char buffer[80];
  
    ALOGV("Opening device: %s", devicePath);
    // 打开device文件
    int fd = open(devicePath, O_RDWR | O_CLOEXEC | O_NONBLOCK);
    // 省略若干行
    // 然后依次获取设备的名称、驱动版本、设备的厂商等信息、物理路径、唯一的id等信息
    // 接着判断是键盘或者游戏面板、鼠标等设备类型进行特殊处理
    // 最后将设备添加到epoll中进行监听
    if (registerDeviceForEpollLocked(device) != OK) {
        // 添加失败则删除设备并退出
        delete device;
        return -1;
    }
    // 省略若干行
    // 将设备添加到device列表
    addDeviceLocked(device);
    return OK;
}
```

openDeviceLocked方法中，首先通过open方法打开对应的设备文件，然后会获取设备的各种信息并进行相应的处理，接着通过registerDeviceForEpollLocked方法将device添加到epoll中去，最后再通过addDeviceLocked方法将device添加到设备列表当中去。

```cpp
status_t EventHub::registerDeviceForEpollLocked(Device* device) {
    // 省略若干行
    // 调用registerFdForEpoll将设备描述符添加到epoll中去
    status_t result = registerFdForEpoll(device->fd);
    // 省略若干行
    return result;
}
  
status_t EventHub::registerFdForEpoll(int fd) {
    // TODO(b/121395353) - consider adding EPOLLRDHUP
    struct epoll_event eventItem = {};
    // 设置event类型为EPOLLIN 和 EPOLLWAKEUP
    eventItem.events = EPOLLIN | EPOLLWAKEUP;
    eventItem.data.fd = fd;
    // 通过epoll_ctl调用并传入EPOLL_CTL_ADD的flag添加对应fd到epoll中
    if (epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &eventItem)) {
        ALOGE("Could not add fd to epoll instance: %s", strerror(errno));
        return -errno;
    }
    return OK;
}
  
void EventHub::addDeviceLocked(Device* device) {
    // 将设备添加到device列表中
    mDevices.add(device->id, device);
    // 将device添加到open的设备链表中
    device->next = mOpeningDevices;
    mOpeningDevices = device;
}
```

我们能够看到在registerFdForEpoll方法中，设备的fd被添加为EPOLLIN和EPOLLWAKEUP类型的，所以这两种类型的事件到来时就可以唤醒epoll工作，然后在addDeviceLocked方法中会将设备添加到device列表和open的device链表中去。

##### EventHub处理open设备

在getEvents方法中，会遍历整个open的设备链表，迭代每个设备，然后构建设备添加的event，最后标识扫描完成的变量；如果在处理过程中，buffer已经满了，则会break掉，未处理的设备会在下一次迭代时继续处理。

```cpp
// 迭代每一个open的device
 while (mOpeningDevices != nullptr) {
    Device* device = mOpeningDevices;
    ALOGV("Reporting device opened: id=%d, name=%s\n", device->id, device->path.c_str());
    // 修改头指针
    mOpeningDevices = device->next;
    // 构建DEVICE_ADDED的event
    event->when = now;
    event->deviceId = device->id == mBuiltInKeyboardId ? 0 : device->id;
    event->type = DEVICE_ADDED;
    event += 1;
    // 设置scan完成标识
    mNeedToSendFinishedDeviceScan = true;
    // buffer满了，则跳出
    if (--capacity == 0) {
        break;
    }
}
```

##### EventHub处理event

```cpp
// 迭代处理所有event
while (mPendingEventIndex < mPendingEventCount) {
    const struct epoll_event& eventItem = mPendingEventItems[mPendingEventIndex++];
    if (eventItem.data.fd == mINotifyFd) {
        // event为device变更，则标识mPendingINotify，后面会进行处理
        if (eventItem.events & EPOLLIN) {
            mPendingINotify = true;
        } else {
            ALOGW("Received unexpected epoll event 0x%08x for INotify.", eventItem.events);
        }
        continue;
    }
    // event是wake管道消息
    if (eventItem.data.fd == mWakeReadPipeFd) {
        if (eventItem.events & EPOLLIN) {
            ALOGV("awoken after wake()");
            // 标识被唤醒，后面epoll就不会进入wait状态
            awoken = true;
            char buffer[16];
            ssize_t nRead;
            do {// 从管道中读取出消息内容
                nRead = read(mWakeReadPipeFd, buffer, sizeof(buffer));
            } while ((nRead == -1 && errno == EINTR) || nRead == sizeof(buffer));
        } else {
            ALOGW("Received unexpected epoll event 0x%08x for wake read pipe.",
                    eventItem.events);
        }
        continue;
    }
    // 通过event中的设备描述符获取对应device
    Device* device = getDeviceByFdLocked(eventItem.data.fd);
    // 省略若干行
    // This must be an input event
    if (eventItem.events & EPOLLIN) {
        // event是input事件
        // 从device中读取出事件内容
        int32_t readSize =
                read(device->fd, readBuffer, sizeof(struct input_event) * capacity);
        if (readSize == 0 || (readSize < 0 && errno == ENODEV)) {
            // Device was removed before INotify noticed.
            ALOGW("could not get event, removed? (fd: %d size: %" PRId32
                    " bufferSize: %zu capacity: %zu errno: %d)\n",
                    device->fd, readSize, bufferSize, capacity, errno);
            // 出错，则关闭对应device，并标识设备发生变更
            deviceChanged = true;
            closeDeviceLocked(device);
        }
        // 省略若干行
        else {
            // 获取deviceId
            int32_t deviceId = device->id == mBuiltInKeyboardId ? 0 : device->id;
            // 遍历读取到的所有event，并构建出RawEvent
            size_t count = size_t(readSize) / sizeof(struct input_event);
            for (size_t i = 0; i < count; i++) {
                struct input_event& iev = readBuffer[i];
                event->when = processEventTimestamp(iev);
                event->deviceId = deviceId;
                event->type = iev.type;
                event->code = iev.code;
                event->value = iev.value;
                event += 1;
                capacity -= 1;
            }
            // 如果buffer满了，则break掉
            if (capacity == 0) {
                // The result buffer is full.  Reset the pending event index
                // so we will try to read the device again on the next iteration.
                mPendingEventIndex -= 1;
                break;
            }
        }
    }
}
```

这里处理event时，首先处理设备的变更事件，然后会处理wake管道事件，最后才会处理真正的input事件；处理input事件时会遍历每一个获取到的event，并构建出对应的RawEvent。

##### EventHub处理inotify事件

```cpp
// readNotify() will modify the list of devices so this must be done after
// processing all other events to ensure that we read all remaining events
// before closing the devices.
if (mPendingINotify && mPendingEventIndex >= mPendingEventCount) {
    // 标识已经处理过
    mPendingINotify = false;
    // 处理inotify事件
    readNotifyLocked();
    deviceChanged = true;
}
  
status_t EventHub::readNotifyLocked() {
    // 省略若干行
    // 从mINotifyFd读取事件内容
    res = read(mINotifyFd, event_buf, sizeof(event_buf));
    // 省略若干行
    // 遍历每一个inotify_event
    while (res >= (int)sizeof(*event)) {
        event = (struct inotify_event*)(event_buf + event_pos);
        if (event->len) {
            if (event->wd == mInputWd) {
                // 是input类型的变更
                // 获取设备对应的文件路径
                std::string filename = StringPrintf("%s/%s", DEVICE_PATH, event->name);
                if (event->mask & IN_CREATE) {
                    // 是设备连入事件，则调用openDeviceLocked来添加设备
                    // 上面已经介绍过，这里就不展开了
                    openDeviceLocked(filename.c_str());
                } else {
                    // 否则是设备断开事件，则调用closeDeviceByPathLocked关闭设备
                    ALOGI("Removing device '%s' due to inotify event\n", filename.c_str());
                    closeDeviceByPathLocked(filename.c_str());
                }
            }
            // 省略若干行
        }
        event_size = sizeof(*event) + event->len;
        res -= event_size;
        event_pos += event_size;
    }
    return 0;
}
```

这里首先会从mINotifyFd读取对应的inotify事件，然后遍历每一个event，判断是设备接入还是断开并分别进行设备添加和移除的处理。

```cpp
void EventHub::closeDeviceByPathLocked(const char* devicePath) {
    // 从device列表中根据设备路径查找device
    Device* device = getDeviceByPathLocked(devicePath);
    if (device) {
        // 找到则调用closeDeviceLocked来关闭device
        // 上面reopen时已经介绍过，这里不再展开
        closeDeviceLocked(device);
        return;
    }
    ALOGV("Remove device: %s not found, device may already have been removed.", devicePath);
}
  
EventHub::Device* EventHub::getDeviceByPathLocked(const char* devicePath) const {
    // 遍历整个device列表
    for (size_t i = 0; i < mDevices.size(); i++) {
        Device* device = mDevices.valueAt(i);
        // 根据设备路径配备device
        if (device->path == devicePath) {
            return device;
        }
    }
    return nullptr;
}
```

这里关闭设备，首先是通过设备的路径从device列表中查找对应的device，如果找到了，则调用closeDeviceLocked去处理设备的关闭。

#### 小结

通过以上介绍，我们可以了解到EventHub的创建过程以及其采用epoll加inotify的方式来实现input设备以及input事件的监听；另外，在getEvents方法中，包含了整个设备的添加、删除以及input事件的处理，而这些事件的处理正是基于EventHub创建时采用的epoll加inotify机制实现的。

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/e28e227161a5  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。