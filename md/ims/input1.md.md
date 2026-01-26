Android Input

[](https://github.com/i-rtfsc/note/edit/main/docs/Android/input/InputDispatcher%E4%B8%8EInputChannel.md "编辑此页")

InputDispatcher接收InputReader读取到的事件，分发给对应窗口，InputDispatcher属于system\_server进程和各个应用不在同一进程，它们之间的联系靠的就是InputChannel。

## handleResumeActivity[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#handleresumeactivity "Permanent link")

直接从ActivityThread的handleResumeActivity开始，Activity的DecorView会被添加到Window

```
<span></span><code id="code-lang-typescript">@Override
public void handleResumeActivity(IBinder token, boolean finalStateRequest, boolean isForward,
        String reason) {
        ......
        if (a.mVisibleFromClient) {
            if (!a.mWindowAdded) {
                a.mWindowAdded = true;
                wm.addView(decor, l);
            } else { 
                ......
                a.onWindowAttributesChanged(l);
            }
        }
      ......
}</code>
```

## addView[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#addview "Permanent link")

frameworks/base/core/java/android/view/WindowManagerImpl.java

```
<span></span><code id="code-lang-less">@Override
public void addView(@NonNull View view, @NonNull ViewGroup.LayoutParams params) {
    applyDefaultToken(params);
    mGlobal.addView(view, params, mContext.getDisplay(), mParentWindow);
}</code>
```

frameworks/base/core/java/android/view/WindowManagerGlobal.java

```
<span></span><code id="code-lang-csharp">public void addView(View view, ViewGroup.LayoutParams params,
    Display display, Window parentWindow) {
    ......

    root = new ViewRootImpl(view.getContext(), display);
    ......
    root.setView(view, wparams, panelParentView);
    ......
}</code>
```

## setView[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#setview "Permanent link")

frameworks/base/core/java/android/view/ViewRootImpl.java 创建mInputChannel，并传递给WMS

```
/**
 * We have one child
 */
public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
    synchronized (this) {
    ......
    if ((mWindowAttributes.inputFeatures
                    &amp; WindowManager.LayoutParams.INPUT_FEATURE_NO_INPUT_CHANNEL) == 0) {
                mInputChannel = new InputChannel();
           }
           res = mWindowSession.addToDisplay(mWindow, mSeq,mWindowAttributes,getHostVisibility(), mDisplay.getDisplayId(), mTmpFrame,mAttachInfo.mContentInsets,
           mAttachInfo.mStableInsets,mAttachInfo.mOutsets,
           mAttachInfo.mDisplayCutout, mInputChannel,
           mTempInsets);
     ......
}</code>
```

## addToDisplay[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#addtodisplay "Permanent link")

frameworks/base/services/core/java/com/android/server/wm/Session.java

```
<span></span><code id="code-lang-java">@Override
public int addToDisplay(IWindow window, int seq, WindowManager.LayoutParams attrs,
      int viewVisibility, int displayId, Rect outFrame, Rect outContentInsets,
      Rect outStableInsets, Rect outOutsets,
      DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel,
      InsetsState outInsetsState) {
      return mService.addWindow(this, window, seq, attrs, viewVisibility, displayId, outFrame,
              outContentInsets, outStableInsets, outOutsets, outDisplayCutout, outInputChannel,
              outInsetsState);
}</code>
```

## addWindow[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#addwindow "Permanent link")

frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java

```
<span></span><code id="code-lang-java">public int addWindow(Session session, IWindow client, int seq,
        LayoutParams attrs, int viewVisibility, int displayId, Rect outFrame,
        Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
        DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel,
        InsetsState outInsetsState) {
       ......
       final WindowState win = new WindowState(this, session, client, token, parentWindow,
                appOp[0], seq, attrs, viewVisibility, session.mUid,
                session.mCanAddInternalSystemWindow);
       final boolean openInputChannels = (outInputChannel != null
                &amp;&amp; (attrs.inputFeatures &amp; INPUT_FEATURE_NO_INPUT_CHANNEL) == 0);
        if  (openInputChannels) {
            win.openInputChannel(outInputChannel);
        }
       ......
}</code>
```

## win.openInputChannel[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#winopeninputchannel "Permanent link")

```
<span></span><code id="code-lang-javascript">void openInputChannel(InputChannel outInputChannel) {
    if (mInputChannel != null) {
        throw new IllegalStateException("Window already has an input channel.");
    }
    String name = getName();
    //创建InputChannelPair
    InputChannel[] inputChannels = InputChannel.openInputChannelPair(name);
    //服务端InputChannel
    mInputChannel = inputChannels[0];
    //客户端InputChannel
    mClientChannel = inputChannels[1];
    mInputWindowHandle.token = mClient.asBinder();
    if (outInputChannel != null) {
        //将客户端InputChannel发送回ViewRootImpl
        mClientChannel.transferTo(outInputChannel);
        mClientChannel.dispose();
        mClientChannel = null;
    } else {
        // If the window died visible, we setup a dummy input channel, so that taps
        // can still detected by input monitor channel, and we can relaunch the app.
        // Create dummy event receiver that simply reports all events as handled.
        mDeadWindowEventReceiver = new DeadWindowEventReceiver(mClientChannel);
    }
    //将服务端InputChannel注册到InputDispatcher
    mWmService.mInputManager.registerInputChannel(mInputChannel, mClient.asBinder());
}</code>
```

## openInputChannelPair[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#openinputchannelpair "Permanent link")

```
<span></span><code id="code-lang-typescript">/**
 * Creates a new input channel pair.  One channel should be provided to the input
 * dispatcher and the other to the application's input queue.
 * @param name The descriptive (non-unique) name of the channel pair.
 * @return A pair of input channels.  The first channel is designated as the
 * server channel and should be used to publish input events.  The second channel
 * is designated as the client channel and should be used to consume input events.
 */
public static InputChannel[] openInputChannelPair(String name) {
    if (name == null) {
        throw new IllegalArgumentException("name must not be null");
    }

    if (DEBUG) {
        Slog.d(TAG, "Opening input channel pair '" + name + "'");
    }
    return nativeOpenInputChannelPair(name);
}</code>
```

## nativeOpenInputChannelPair[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#nativeopeninputchannelpair "Permanent link")

frameworks/base/core/jni/android\_view\_InputChannel.cpp

```
static jobjectArray android_view_InputChannel_nativeOpenInputChannelPair(JNIEnv* env,
        jclass clazz, jstring nameObj) {
    ScopedUtfChars nameChars(env, nameObj);
    std::string name = nameChars.c_str();

    sp&lt;InputChannel&gt; serverChannel;
    sp&lt;InputChannel&gt; clientChannel;
    //创建Native层serverChannel和clientChannel
    status_t result = InputChannel::openInputChannelPair(name, serverChannel, clientChannel);
    .......
    //创建java层数组，用在后面存放java层InputChannel，并返回
    jobjectArray channelPair = env-&gt;NewObjectArray(2, gInputChannelClassInfo.clazz, NULL);
    ......
    //创建java层的server端的InputChannel对象
    jobject serverChannelObj = android_view_InputChannel_createInputChannel(env,
            std::make_unique&lt;NativeInputChannel&gt;(serverChannel));
    ......
    //创建java层的clien端的InputChannel对象
    jobject clientChannelObj = android_view_InputChannel_createInputChannel(env,
            std::make_unique&lt;NativeInputChannel&gt;(clientChannel));
    ......
    env-&gt;SetObjectArrayElement(channelPair, 0, serverChannelObj);
    env-&gt;SetObjectArrayElement(channelPair, 1, clientChannelObj);
    return channelPair;
}</code>
```

## InputChannel::openInputChannelPair[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#inputchannelopeninputchannelpair "Permanent link")

此方法中创建了一对NativeInputChannel和一对socket，在InputChannel构造函数中通过setFd将两个InputChanne和两个socket一一关联上,后去可以通过getFd拿到对于InputChannel的socket连接

frameworks/native/libs/input/InputTransport.cpp

```
status_t InputChannel::openInputChannelPair(const std::string&amp; name,
        sp&lt;InputChannel&gt;&amp; outServerChannel, sp&lt;InputChannel&gt;&amp; outClientChannel) {
    //一对socket数组
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets)) {
        status_t result = -errno;
        ALOGE("channel '%s' ~ Could not create socket pair.  errno=%d",
                name.c_str(), errno);
        outServerChannel.clear();
        outClientChannel.clear();
        return result;
    }

    int bufferSize = SOCKET_BUFFER_SIZE;
    //对两个socker分别设置
    setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF, &amp;bufferSize, sizeof(bufferSize));
    setsockopt(sockets[0], SOL_SOCKET, SO_RCVBUF, &amp;bufferSize, sizeof(bufferSize));
    setsockopt(sockets[1], SOL_SOCKET, SO_SNDBUF, &amp;bufferSize, sizeof(bufferSize));
    setsockopt(sockets[1], SOL_SOCKET, SO_RCVBUF, &amp;bufferSize, sizeof(bufferSize));

    std::string serverChannelName = name;
    //创建ServerChannel，后缀名+server
    serverChannelName += " (server)";
    //通过setFd将sockets[0]和ServerChannel关联上
    outServerChannel = new InputChannel(serverChannelName, sockets[0]);

    std::string clientChannelName = name;
    //创建ClientChannel，后缀名+client
    clientChannelName += " (client)";
    //通过setFd将sockets[1]和ClientChannel关联上
    outClientChannel = new InputChannel(clientChannelName, sockets[1]);
    return OK;
}
InputChannel::InputChannel(const std::string&amp; name, int fd) :
          mName(name) {
          ......
          setFd(fd);
  }</code>
```

继续看nativeOpenInputChannelPair方法后半部分 android\_view\_InputChannel\_createInputChannel 此方法会创建一个java数组，一对java层InputChannel,并将一对NativeInputChannel赋值给java层InputChannel的mPtr属性，并将server端InputChannel赋值给数组0，将client端InputChannel赋值给数组1,最后将数组返回给java层

```
//指向java层InputChannel的mPtr属性
gInputChannelClassInfo.mPtr = GetFieldIDOrDie(env, gInputChannelClassInfo.clazz, "mPtr", "J");
static jobjectArray android_view_InputChannel_nativeOpenInputChannelPair(JNIEnv* env,
        jclass clazz, jstring nameObj) {
   //创建一个java数组，大小为2,类型为InputChannel
   jobjectArray channelPair = env-&gt;NewObjectArray(2, gInputChannelClassInfo.clazz, NULL);
   ......
   //将server端的NativeInputChannel赋值给java层的server端的InputChannel
   jobject serverChannelObj = android_view_InputChannel_createInputChannel(env,
            std::make_unique&lt;NativeInputChannel&gt;(serverChannel));
    ......
    //将client端的NativeInputChannel赋值给java层的client端的InputChannel
    jobject clientChannelObj = android_view_InputChannel_createInputChannel(env,
            std::make_unique&lt;NativeInputChannel&gt;(clientChannel));
    ......
    static jobject android_view_InputChannel_createInputChannel(JNIEnv* env,
        std::unique_ptr&lt;NativeInputChannel&gt; nativeInputChannel) {
        //创建java层inputChannel对象
    jobject inputChannelObj = env-&gt;NewObject(gInputChannelClassInfo.clazz,
            gInputChannelClassInfo.ctor);
       //当inputChannel不为空
    if (inputChannelObj) {
        //给InputChannel的mPtr属性赋值为NativeInputChannel
        android_view_InputChannel_setNativeInputChannel(env, inputChannelObj,
                 nativeInputChannel.release());
    }
    return inputChannelObj;
}
static void android_view_InputChannel_setNativeInputChannel(JNIEnv* env, jobject inputChannelObj,
        NativeInputChannel* nativeInputChannel) {
    //jni方法，给inputChannelObj对象的gInputChannelClassInfo.mPtr属性赋值为reinterpret_cast&lt;jlong&gt;(nativeInputChannel)
    env-&gt;SetLongField(inputChannelObj, gInputChannelClassInfo.mPtr,
             reinterpret_cast&lt;jlong&gt;(nativeInputChannel));
}
    //将server端InputChannel赋值给数组0的位置
    env-&gt;SetObjectArrayElement(channelPair, 0, serverChannelObj);
    //将client端InputChannel赋值给数组0的位置
    env-&gt;SetObjectArrayElement(channelPair, 1, clientChannelObj);
    //将InputChannel数组返回给java层
    return channelPair;
    }</code>
```

## 回到WindowState[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#windowstate "Permanent link")

通过openInputChannelPair方法已经获取得到了InputChannel数组，并且数组0的位置是server端的InputChannel，数组1的位置是client端的InputChannel

frameworks/base/services/core/java/com/android/server/wm/WindowState.java

```
<span></span><code id="code-lang-javascript">void openInputChannel(InputChannel outInputChannel) {
    if (mInputChannel != null) {
        throw new IllegalStateException("Window already has an input channel.");
    }
    String name = getName();
    //native层返回的InputChannel数组，server端和cilent端
    InputChannel[] inputChannels = InputChannel.openInputChannelPair(name);
    //server端InputChannel
    mInputChannel = inputChannels[0];
    //client端InputChannel
    mClientChannel = inputChannels[1];
    mInputWindowHandle.token = mClient.asBinder();
    if (outInputChannel != null) {
        //将client端InputChannel传回到ViewRootImpl
        mClientChannel.transferTo(outInputChannel);
        mClientChannel.dispose();
        mClientChannel = null;
    } else {
        // If the window died visible, we setup a dummy input channel, so that taps
        // can still detected by input monitor channel, and we can relaunch the app.
        // Create dummy event receiver that simply reports all events as handled.
        mDeadWindowEventReceiver = new DeadWindowEventReceiver(mClientChannel);
    }
    //server端InputChannel注册到native层InputDispatcher
    mWmService.mInputManager.registerInputChannel(mInputChannel, mClient.asBinder());
}</code>
```

将client端InputChannel传回到ViewRootImpl transferTo

frameworks/base/core/java/android/view/InputChannel.java

```
<span></span><code id="code-lang-typescript">/**
 * Transfers ownership of the internal state of the input channel to another
 * instance and invalidates this instance.  This is used to pass an input channel
 * as an out parameter in a binder call.
 * @param other The other input channel instance.
 */
public void transferTo(InputChannel outParameter) {
    if (outParameter == null) {
        throw new IllegalArgumentException("outParameter must not be null");
    }

    nativeTransferTo(outParameter);
}</code>
```

## android\_view\_InputChannel\_nativeTransferTo[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#android_view_inputchannel_nativetransferto "Permanent link")

frameworks/base/core/jni/android\_view\_InputChannel.cpp

```
<span></span><code id="code-lang-javascript">/*
mClientChannel.transferTo(outInputChannel);
*/
//注意该方法参数中的otherObj和obj，otherObj代表传递下来的参数outInputChannel，obj代表调用此方法的对象mClientChannel
static void android_view_InputChannel_nativeTransferTo(JNIEnv* env, jobject obj,
        jobject otherObj) {
        //当java层的outInputChannel的mPtr已经指向了一个NativeInputChannel时抛一个异常返回
    if (android_view_InputChannel_getNativeInputChannel(env, otherObj) != NULL) {
        jniThrowException(env, "java/lang/IllegalStateException",
                "Other object already has a native input channel.");
        return;
    }
    //获取java层mClientChannel的mPtr属性，mPtr属性会指向一个NativeInputChannel对象，根据前面分析此时mClientChannel的mPtr指向client端的NativeInputChannel
    NativeInputChannel* nativeInputChannel =
            android_view_InputChannel_getNativeInputChannel(env, obj);
    android_view_InputChannel_setNativeInputChannel(env, otherObj, nativeInputChannel);
    android_view_InputChannel_setNativeInputChannel(env, obj, NULL);
}
static NativeInputChannel* android_view_InputChannel_getNativeInputChannel(JNIEnv* env,
        jobject inputChannelObj) {
    //jni方法，获取inputChannelObj对象的gInputChannelClassInfo.mPtr属性
    jlong longPtr = env-&gt;GetLongField(inputChannelObj, gInputChannelClassInfo.mPtr);
    return reinterpret_cast&lt;NativeInputChannel*&gt;(longPtr);
}
static void android_view_InputChannel_setNativeInputChannel(JNIEnv* env, jobject inputChannelObj,
        NativeInputChannel* nativeInputChannel) {
        //jni方法，将inputChannelObj的gInputChannelClassInfo.mPtr属性赋值为nativeInputChannel
    env-&gt;SetLongField(inputChannelObj, gInputChannelClassInfo.mPtr,
             reinterpret_cast&lt;jlong&gt;(nativeInputChannel));
}</code>
```

总结：上面的方法主要作用就是将mClientChannel的mPtr属性赋值给outInputChannel，并将mClientChannel的mPtr赋值为NULL，这样就实现了将mClientChannel与outInputChannel互换，outInputChannel是ViewRootImpl的传递过来的mInputChannel，达到了将mClientChannel传递个应用客户端的目的。

## 继续回到WindowState[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#windowstate_1 "Permanent link")

/frameworks/base/services/core/java/com/android/server/wm/WindowState.java

```
<span></span><code id="code-lang-javascript">void openInputChannel(InputChannel outInputChannel) {
    if (mInputChannel != null) {
        throw new IllegalStateException("Window already has an input channel.");
    }
    String name = getName();
    //native层返回的InputChannel数组，server端和cilent端
    InputChannel[] inputChannels = InputChannel.openInputChannelPair(name);
    //server端InputChannel
    mInputChannel = inputChannels[0];
    //client端InputChannel
    mClientChannel = inputChannels[1];
    mInputWindowHandle.token = mClient.asBinder();
    if (outInputChannel != null) {
        //将client端InputChannel传回到ViewRootImpl
        mClientChannel.transferTo(outInputChannel);
        mClientChannel.dispose();
        mClientChannel = null;
    } else {
        // If the window died visible, we setup a dummy input channel, so that taps
        // can still detected by input monitor channel, and we can relaunch the app.
        // Create dummy event receiver that simply reports all events as handled.
        mDeadWindowEventReceiver = new DeadWindowEventReceiver(mClientChannel);
    }
    //server端InputChannel注册到native层InputDispatcher
    mWmService.mInputManager.registerInputChannel(mInputChannel, mClient.asBinder());
}</code>
```

client端的InputChannel已经传递回了ViewRootImpl，接着看server端的InputChannel如何注册到InputDispatcher的 //server端InputChannel注册到native层InputDispatcher mWmService.mInputManager.registerInputChannel(mInputChannel, mClient.asBinder());

## registerInputChannel[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#registerinputchannel "Permanent link")

frameworks/base/services/core/java/com/android/server/input/InputManagerService.java

```
<span></span><code id="code-lang-typescript">/**
 * Registers an input channel so that it can be used as an input event target.
 * @param inputChannel The input channel to register.
 * @param inputWindowHandle The handle of the input window associated with the
 * input channel, or null if none.
 */
public void registerInputChannel(InputChannel inputChannel, IBinder token) {
    if (inputChannel == null) {
        throw new IllegalArgumentException("inputChannel must not be null.");
    }
    if (token == null) {
        token = new Binder();
    }
    inputChannel.setToken(token);
    //native方法
    nativeRegisterInputChannel(mPtr, inputChannel, Display.INVALID_DISPLAY);
}</code>
```

## nativeRegisterInputChannel[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#nativeregisterinputchannel "Permanent link")

frameworks/base/services/core/jni/com\_android\_server\_input\_InputManagerService.cpp

```
static void nativeRegisterInputChannel(JNIEnv* env, jclass /* clazz */,
        jlong ptr, jobject inputChannelObj, jint displayId) {
    //ptr是java层InputManagerService中的mPtr属性，在InputManagerService初始化时指向了NativeInputManager
    NativeInputManager* im = reinterpret_cast&lt;NativeInputManager*&gt;(ptr);
    //通过java层传下来的inputChannels[0]获取到Native层的server端InputChannel
    sp&lt;InputChannel&gt; inputChannel = android_view_InputChannel_getInputChannel(env,
            inputChannelObj);
    if (inputChannel == nullptr) {
        throwInputChannelNotInitialized(env);
        return;
    }
    //注册inputChannel
    status_t status = im-&gt;registerInputChannel(env, inputChannel, displayId);
    if (status) {
        std::string message;
        message += StringPrintf("Failed to register input channel.  status=%d", status);
        jniThrowRuntimeException(env, message.c_str());
        return;
    }
    //设置handleInputChannelDisposed回调
    android_view_InputChannel_setDisposeCallback(env, inputChannelObj,
            handleInputChannelDisposed, im);
 }</code>
```

## android\_view\_InputChannel\_getInputChannel[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#android_view_inputchannel_getinputchannel "Permanent link")

frameworks/base/core/jni/android\_view\_InputChannel.cpp

```
<span></span><code id="code-lang-bash">sp&lt;InputChannel&gt; android_view_InputChannel_getInputChannel(JNIEnv* env, jobject inputChannelObj) {
    NativeInputChannel* nativeInputChannel =
            android_view_InputChannel_getNativeInputChannel(env, inputChannelObj);
    //inline sp&lt;InputChannel&gt; getInputChannel() { return mInputChannel; },返回server端的Nativa层的InputChannel
    return nativeInputChannel != NULL ? nativeInputChannel-&gt;getInputChannel() : NULL;
}</code>
```

## 注册inputChannel，registerInputChannel[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#inputchannelregisterinputchannel "Permanent link")

frameworks/base/services/core/jni/com\_android\_server\_input\_InputManagerService.cpp

```
<span></span><code id="code-lang-scss">status_t NativeInputManager::registerInputChannel(JNIEnv* /* env */,
        const sp&lt;InputChannel&gt;&amp; inputChannel, int32_t displayId) {
    ATRACE_CALL();
    return mInputManager-&gt;getDispatcher()-&gt;registerInputChannel(
            inputChannel, displayId);
}</code>
```

## registerInputChannel[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#registerinputchannel_1 "Permanent link")

调到InputDispatcher的registerInputChannel方法， 1.此方法会创建Connection，将server端InputChannel作为参数传进去， 2.获取之前InputChannel的Fd即socket\[0\]，将fd和connection关联，后续InputDispatch通过fd可以获取对应connection，通过connection获取server InputChannel和client InputChannel通信从而将事件分发到对应的应用窗口 3.将fd添加到Looper线程进行监听

```
<span></span><code tabindex="0" id="code-lang-cpp">//InputDispatcher.h
// All registered connections mapped by channel file descriptor.
KeyedVector&lt;int, sp&lt;Connection&gt; &gt; mConnectionsByFd GUARDED_BY(mLock);
status_t InputDispatcher::registerInputChannel(const sp&lt;InputChannel&gt;&amp; inputChannel,
        int32_t displayId) {
    { // acquire lock
        ......
        //创建Connection对象
        sp&lt;Connection&gt; connection = new Connection(inputChannel, false /*monitor*/);
        //获取之前通过setFd存入的socket[0]
        int fd = inputChannel-&gt;getFd();
        //将fd和connection关联,后续InputDispatch通过fd可以获取对应connection，通过connection获取server InputChannel和client InputChannel通信从而将事件分发到对应的应用窗口
        mConnectionsByFd.add(fd, connection);
        mInputChannelsByToken[inputChannel-&gt;getToken()] = inputChannel;
        //将fd添加到Looper线程进行监听，在收到对端相应事件之后回调handleReceiveCallback
        mLooper-&gt;addFd(fd, 0, ALOOPER_EVENT_INPUT, handleReceiveCallback, this);
    } // release lock
    // Wake the looper because some connections have changed.
    mLooper-&gt;wake();
    return OK;
}</code>
```

在创建Connection时将server端的inputChannel同步传给InputPublisher的mChannel

frameworks/native/services/inputflinger/InputDispatcher.cpp

```
<span></span><code id="code-lang-css">// --- InputDispatcher::Connection ---
InputDispatcher::Connection::Connection(const sp&lt;InputChannel&gt;&amp; inputChannel, bool monitor) :
        status(STATUS_NORMAL), inputChannel(inputChannel),
        monitor(monitor),
        inputPublisher(inputChannel), inputPublisherBlocked(false) {
}</code>
```

frameworks/native/libs/input/InputTransport.cpp

```
<span></span><code id="code-lang-css">InputPublisher::InputPublisher(const sp&lt;InputChannel&gt;&amp; channel) :
        mChannel(channel) {
}</code>
```

好了，从java层传下来的inputChannels\[0\]就已经注册到了InputDispatcher并且将对应的Fd添加到了InputDispatcher的Loop进行监听，并且传进去了一个回调handleReceiveCallback，应用端窗口处理完输入事件之后回调此方法

接着需要看inputChannels\[1\]的Fd是如何添加到应用端进行监听的

## 回到ViewRootImpl[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#viewrootimpl "Permanent link")

在上面流程addToDisplay方法执行完之后ViewRootImpl中的mInputChannel已经变成了client端的InputChannel即inputChannels\[1\]

frameworks/base/core/java/android/view/ViewRootImpl.java

```
<span></span><code id="code-lang-typescript">public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
      ......
      mInputChannel = new InputChannel();
      res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
                            getHostVisibility(), mDisplay.getDisplayId(), mTmpFrame,
                            mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
                            mAttachInfo.mOutsets, mAttachInfo.mDisplayCutout, mInputChannel,
                            mTempInsets);
                            ......
        if (mInputChannel != null) {
                    if (mInputQueueCallback != null) {
                        mInputQueue = new InputQueue();
                        mInputQueueCallback.onInputQueueCreated(mInputQueue);
                    }
                    mInputEventReceiver = new WindowInputEventReceiver(mInputChannel,
                            Looper.myLooper());
                }       
      ......
}</code>
```

## 看WindowInputEventReceiver的创建[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#windowinputeventreceiver "Permanent link")

```
<span></span><code id="code-lang-java">final class WindowInputEventReceiver extends InputEventReceiver {
    public WindowInputEventReceiver(InputChannel inputChannel, Looper looper) {
        //client端InputChannel，UI线程的looper
        super(inputChannel, looper);
    }</code>
```

调父类InputEventReceiver构造函数

```
<span></span><code id="code-lang-java">/**
     * Creates an input event receiver bound to the specified input channel.
     *
     * @param inputChannel The input channel.
     * @param looper The looper to use when invoking callbacks.
     */
    public InputEventReceiver(InputChannel inputChannel, Looper looper) {
        if (inputChannel == null) {
            throw new IllegalArgumentException("inputChannel must not be null");
        }
        if (looper == null) {
            throw new IllegalArgumentException("looper must not be null");
        }
        mInputChannel = inputChannel;
        //UI线程的messagequeue
        mMessageQueue = looper.getQueue();
        //native方法，将自己，client端的InputChannel和UI线程的messagequeue传进去
        mReceiverPtr = nativeInit(new WeakReference&lt;InputEventReceiver&gt;(this),
                inputChannel, mMessageQueue);

        mCloseGuard.open("dispose");
    }</code>
```

## nativeInit[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#nativeinit "Permanent link")

frameworks/base/core/jni/android\_view\_InputEventReceiver.cpp

```
<span></span><code id="code-lang-kotlin">static jlong nativeInit(JNIEnv* env, jclass clazz, jobject receiverWeak,
        jobject inputChannelObj, jobject messageQueueObj) {
    //见过很多次了该方法，通过传下来的cilent端的InputChannel获取对应的native层InputChannel
    sp&lt;InputChannel&gt; inputChannel = android_view_InputChannel_getInputChannel(env,
            inputChannelObj);
    if (inputChannel == NULL) {
        jniThrowRuntimeException(env, "InputChannel is not initialized.");
        return 0;
    }
    //通过传下来的messagequeue获取对于native层的messagequeue
    sp&lt;MessageQueue&gt; messageQueue = android_os_MessageQueue_getMessageQueue(env, messageQueueObj);
    if (messageQueue == NULL) {
        jniThrowRuntimeException(env, "MessageQueue is not initialized.");
        return 0;
    }
    //创建NativeInputEventReceiver
    sp&lt;NativeInputEventReceiver&gt; receiver = new NativeInputEventReceiver(env,
            receiverWeak, inputChannel, messageQueue);
    status_t status = receiver-&gt;initialize();
    if (status) {
        String8 message;
        message.appendFormat("Failed to initialize input event receiver.  status=%d", status);
        jniThrowRuntimeException(env, message.string());
        return 0;
    }
    receiver-&gt;incStrong(gInputEventReceiverClassInfo.clazz); // retain a reference for the object
    return reinterpret_cast&lt;jlong&gt;(receiver.get());
}</code>
```

## 创建NativeInputEventReceiver[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#nativeinputeventreceiver "Permanent link")

frameworks/base/core/jni/android\_view\_InputEventReceiver.cpp

```
<span></span><code id="code-lang-css">//给NativeInputEventReceiver成员变量赋值
NativeInputEventReceiver::NativeInputEventReceiver(JNIEnv* env,
        jobject receiverWeak, const sp&lt;InputChannel&gt;&amp; inputChannel,
        const sp&lt;MessageQueue&gt;&amp; messageQueue) :
        mReceiverWeakGlobal(env-&gt;NewGlobalRef(receiverWeak)),
        mInputConsumer(inputChannel), mMessageQueue(messageQueue),
        mBatchedInputEventPending(false), mFdEvents(0) {
    }
}</code>
```

## initialize[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#initialize "Permanent link")

frameworks/base/core/jni/android\_view\_InputEventReceiver.cpp

```
status_t NativeInputEventReceiver::initialize() {
    setFdEvents(ALOOPER_EVENT_INPUT);
    return OK;
}
void NativeInputEventReceiver::setFdEvents(int events) {
    if (mFdEvents != events) {
        mFdEvents = events;
        //获取client端InputChannel的Fd即socket[1]
        int fd = mInputConsumer.getChannel()-&gt;getFd();
        if (events) {
            //将fd添加到UI线程进行监听,回调传的this
            mMessageQueue-&gt;getLooper()-&gt;addFd(fd, 0, events, this, NULL);
        } else {
            mMessageQueue-&gt;getLooper()-&gt;removeFd(fd);
        }
    }
}</code>
```

到此client端的Fd也已经传递到了UI线程进行监听，InputDispatch与ViewRootImpl的socket连接已经建立

## 总结：[¶](https://i-rtfsc.github.io/android/input/inputDispatcher-inputChannel/#_1 "Permanent link")

当应用窗口添加到WMS会创建一个WindowState描述此窗口，接着创建一对java层InputChannel对应创建一对socket，对应创建一对native层InputChannel，还有一个server端的connection，connection，inputChannel，socket创建完成后通道就已经建立，InputDispatcher就可以将事件发送给对应窗口了 它们的联系如下: server端： 1.java层的server端InputChannel的mPtr属性指向native层的server端InputChannel 2.native层的server端InputChannel创建时会创建server端socket，并在自己的构造方法中通过setFd将此socket存在mFd变量 3.将server端InputChannel向InputDispatch注册时会创建一个connection对象，并通过getFd方法获取到socket，以fd为key，connection为值放入一个类似map的数据结构mConnectionsByFd

client端和server一样只是少了创建connection