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

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/e28e227161a5  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。