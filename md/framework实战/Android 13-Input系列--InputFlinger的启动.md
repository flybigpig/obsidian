> hongxi.zhu 2023-7-11  
> Android T

### 服务的启动

frameworks/base/services/java/com/android/server/SystemServer.java

```java
    private void startOtherServices(@NonNull TimingsTraceAndSlog t) {
        t.traceBegin("startOtherServices");
        ...
        WindowManagerService wm = null;
    	InputManagerService inputManager = null;

        try {
        	//1. 获取InputManagerService，并走初始化inputManager流程
        	...
    	    t.traceBegin("StartInputManagerService");
            inputManager = new InputManagerService(context);
            t.traceEnd();
			...
			//2. WindowManagerService服务持有inputManager对象
            t.traceBegin("StartWindowManagerService");
            wm = WindowManagerService.main(context, inputManager, !mFirstBoot, mOnlyCore,
                    new PhoneWindowManager(), mActivityManagerService.mActivityTaskManager);
           	...
           	//3. 向ServiceManager注册java层ims服务，name = "input"，(native层的服务name = "inputflinger")
            ServiceManager.addService(Context.INPUT_SERVICE, inputManager,
                    /* allowIsolated= */ false, DUMP_FLAG_PRIORITY_CRITICAL);
            t.traceEnd();

			...
			//4. 启动inputflinger，处理输入事件
            t.traceBegin("StartInputManager");
            inputManager.setWindowManagerCallbacks(wm.getInputManagerCallback());
            inputManager.start();
            t.traceEnd();
            ...
        } catch (Throwable e) {
			...
        }
```

1.  获取InputManagerService，并走初始化inputManager（java层和native层）流程
2.  WindowManagerService服务构造时传入inputManager对象，用户wms和ims交互
3.  向ServiceManager注册java层ims服务
4.  启动inputflinger，处理输入事件

我们重点看下1和4

### 1\. 初始化InputFlinger

frameworks/base/services/core/java/com/android/server/input/InputManagerService.java

```java
    public InputManagerService(Context context) {
        this(new Injector(context, DisplayThread.get().getLooper()));
    }

    @VisibleForTesting
    InputManagerService(Injector injector) {
		...
        mContext = injector.getContext();
        mHandler = new InputManagerHandler(injector.getLooper());  //创建一个handle（使用DisplayThread，这个线程用户wms，display，input三个服务使用，对延迟敏感）
        mNative = injector.getNativeService(this);  //创建native input服务（NativeInputManagerService）
		...
        injector.registerLocalService(new LocalService());  //将InputManagerService中的InputManagerInternal实现类加入LocalServices，供同进程其他服务调用相关功能
    }

    @VisibleForTesting
    static class Injector {
        private final Context mContext;
        private final Looper mLooper;

        Injector(Context context, Looper looper) {
            mContext = context;
            mLooper = looper;
        }

        Context getContext() {
            return mContext;
        }

        Looper getLooper() {
            return mLooper;
        }
		//创建native层对应的service
        NativeInputManagerService getNativeService(InputManagerService service) {
            return new NativeInputManagerService.NativeImpl(service, mContext, mLooper.getQueue());
        }
		//将InputManagerService中的InputManagerInternal实现类加入LocalServices，供同进程其他服务调用相关功能
        void registerLocalService(InputManagerInternal localService) {
            LocalServices.addService(InputManagerInternal.class, localService);
        }
    }
```

#### NativeInputManagerService.NativeImpl

```java
public interface NativeInputManagerService {
	...
    class NativeImpl implements NativeInputManagerService {
        /** Pointer to native input manager service object, used by native code. */
        @SuppressWarnings({"unused", "FieldCanBeLocal"})
        private final long mPtr;

        NativeImpl(InputManagerService service, Context context, MessageQueue messageQueue) {
            mPtr = init(service, context, messageQueue);  //初始化native层服务并返回该服务的对象指针到java层
        }

        private native long init(InputManagerService service, Context context,
                MessageQueue messageQueue);
        ...
	}
	...
}
```

#### nativeInit

frameworks/base/services/core/jni/com\_android\_server\_input\_InputManagerService.cpp

```cpp
static const JNINativeMethod gInputManagerMethods[] = {
        /* name, signature, funcPtr */
        {"init",
         "(Lcom/android/server/input/InputManagerService;Landroid/content/Context;Landroid/os/"
         "MessageQueue;)J",
         (void*)nativeInit},  //init(java)->nativeInit(native)
        ...
};

static jlong nativeInit(JNIEnv* env, jclass /* clazz */,
        jobject serviceObj, jobject contextObj, jobject messageQueueObj) {
    sp<MessageQueue> messageQueue = android_os_MessageQueue_getMessageQueue(env, messageQueueObj);
	...
	// 创建native层NativeInputManager，保存java层的context对象、ims对象、msgqueue
    NativeInputManager* im = new NativeInputManager(contextObj, serviceObj,
            messageQueue->getLooper());
    im->incStrong(0);  //NativeInputManager对象强引用 + 1
    return reinterpret_cast<jlong>(im);  //将native层NativeInputManager对象地址返回给java层，这个地址可以在native层获取出同一个对象
}

//NativeInputManager类声明，实现了三个重要接口抽象类
/*class NativeInputManager : public virtual RefBase,
    public virtual InputReaderPolicyInterface,
    public virtual InputDispatcherPolicyInterface,
    public virtual PointerControllerPolicyInterface {
    */
    
NativeInputManager::NativeInputManager(jobject contextObj,
        jobject serviceObj, const sp<Looper>& looper) :
        mLooper(looper), mInteractive(true) {
    JNIEnv* env = jniEnv();  //获取虚拟机环境指针

    mServiceObj = env->NewGlobalRef(serviceObj);  //将java层的ims对象保存为全局引用

    {
        AutoMutex _l(mLock);
        mLocked.systemUiLightsOut = false;
        mLocked.pointerSpeed = 0;
        mLocked.pointerAcceleration = android::os::IInputConstants::DEFAULT_POINTER_ACCELERATION;
        mLocked.pointerGesturesEnabled = true;
        mLocked.showTouches = false;
        mLocked.pointerDisplayId = ADISPLAY_ID_DEFAULT;
    }
    mInteractive = true;

    InputManager* im = new InputManager(this, this);  //创建native层InputManager
    mInputManager = im;
    defaultServiceManager()->addService(String16("inputflinger"), im);  //向ServiceManager注册native层inputflinger服务
}
```

#### new InputManager

frameworks/native/services/inputflinger/InputManager.cpp

```cpp
/**
 * The event flow is via the "InputListener" interface, as follows:
 * InputReader -> UnwantedInteractionBlocker -> InputClassifier -> InputDispatcher
 */
InputManager::InputManager(
        const sp<InputReaderPolicyInterface>& readerPolicy,
        const sp<InputDispatcherPolicyInterface>& dispatcherPolicy) {
    mDispatcher = createInputDispatcher(dispatcherPolicy);
    mClassifier = std::make_unique<InputClassifier>(*mDispatcher);
    mBlocker = std::make_unique<UnwantedInteractionBlocker>(*mClassifier);
    mReader = createInputReader(readerPolicy, *mBlocker);
}
```

创建并初始化四个event flow中的重要对象, 事件从前到后传递，前面的对象依次持有下一个阶段的对象引用  
`event flow: InputReader -> UnwantedInteractionBlocker -> InputClassifier -> InputDispatcher`

#### createInputDispatcher

frameworks/native/services/inputflinger/dispatcher/InputDispatcher.cpp

```cpp
std::unique_ptr<InputDispatcherInterface> createInputDispatcher(
        const sp<InputDispatcherPolicyInterface>& policy) {
    return std::make_unique<android::inputdispatcher::InputDispatcher>(policy);
}

InputDispatcher::InputDispatcher(const sp<InputDispatcherPolicyInterface>& policy)
      : InputDispatcher(policy, STALE_EVENT_TIMEOUT) {}

InputDispatcher::InputDispatcher(const sp<InputDispatcherPolicyInterface>& policy,
                                 std::chrono::nanoseconds staleEventTimeout)
      : mPolicy(policy),  //将接口实现对象传进来，用户和wms服务交互
	 ...//一些成员变量初始化，太多了不列出
	 {
	 
    mLooper = new Looper(false);  //用于InputDispatcher线程
    mReporter = createInputReporter();
	//注册SurfaceComposer监听，当window状态改变时回调此接口onWindowInfosChanged通知inputflinger
    mWindowInfoListener = new DispatcherWindowListener(*this);
    SurfaceComposerClient::getDefault()->addWindowInfosListener(mWindowInfoListener);

    mKeyRepeatState.lastKeyEntry = nullptr;

    policy->getDispatcherConfiguration(&mConfig);
}
```

#### InputClassifier

frameworks/native/services/inputflinger/InputClassifier.cpp

```cpp
//构建InputClassifier，传入mQueuedListener = listener 这个就是mDispatcher对象，这样就可以回调mDispatcher的方法
InputClassifier::InputClassifier(InputListenerInterface& listener) : mQueuedListener(listener) {}
```

#### UnwantedInteractionBlocker

```cpp
//UnwantedInteractionBlocker是所有输入事件都会经历的一个阶段
//inputReader通过它notifyXXX方法向InputDispatcher传递对应事件
//其中对于触摸事件，如果支持手掌误触等功能，则会在这里有特殊处理
UnwantedInteractionBlocker::UnwantedInteractionBlocker(InputListenerInterface& listener)
      : UnwantedInteractionBlocker(listener, isPalmRejectionEnabled()){};
//isPalmRejectionEnabled 检测是否开始手掌误触功能
UnwantedInteractionBlocker::UnwantedInteractionBlocker(InputListenerInterface& listener,
                                                       bool enablePalmRejection)
      : mQueuedListener(listener), mEnablePalmRejection(enablePalmRejection) {}
```

#### InputReader

frameworks/native/services/inputflinger/reader/InputReader.cpp

```cpp
std::unique_ptr<InputReaderInterface> createInputReader(
        const sp<InputReaderPolicyInterface>& policy, InputListenerInterface& listener) {
    return std::make_unique<InputReader>(std::make_unique<EventHub>(), policy, listener);
}

InputReader::InputReader(std::shared_ptr<EventHubInterface> eventHub,
                         const sp<InputReaderPolicyInterface>& policy,
                         InputListenerInterface& listener)
      : mContext(this),
        mEventHub(eventHub), //1.初始化EventHub
        mPolicy(policy),
        mQueuedListener(listener),  //2. 传入UnwantedInteractionBlocker对象
        mGlobalMetaState(AMETA_NONE),
        mLedMetaState(AMETA_NONE),
        mGeneration(1),
        mNextInputDeviceId(END_RESERVED_ID),
        mDisableVirtualKeysTimeout(LLONG_MIN),
        mNextTimeout(LLONG_MAX),
        mConfigurationChangesToRefresh(0) {
    refreshConfigurationLocked(0);
    updateGlobalMetaStateLocked();
}
```

`InputReader`最重要就是：

1.  创建并初始化`EventHub`, 通过它向驱动获取上报的事件
2.  注册`UnwantedInteractionBlocker` listener, 通过它向`inputDispater`传递事件

#### EventHub

frameworks/native/services/inputflinger/reader/EventHub.cpp

```cpp
EventHub::EventHub(void)
      : mBuiltInKeyboardId(NO_BUILT_IN_KEYBOARD),
        mNextDeviceId(1),
        mControllerNumbers(),
        mNeedToSendFinishedDeviceScan(false),
        mNeedToReopenDevices(false),
        mNeedToScanDevices(true),
        mPendingEventCount(0),
        mPendingEventIndex(0),
        mPendingINotify(false) {
    ensureProcessCanBlockSuspend();
	//初始化epoll (用于监听文件描述符上的事件，用于监听具体/dev/input/eventX的fd的事件)
    mEpollFd = epoll_create1(EPOLL_CLOEXEC);
    LOG_ALWAYS_FATAL_IF(mEpollFd < 0, "Could not create epoll instance: %s", strerror(errno));
	//初始化inotify (用于监听文件和目录的变化，这里主要时用于监听/dev/input/下面的目录变化)
    mINotifyFd = inotify_init1(IN_CLOEXEC);

    std::error_code errorCode;
    bool isDeviceInotifyAdded = false;
    //检测 "/dev/input" 文件节点是否存在
    if (std::filesystem::exists(DEVICE_INPUT_PATH, errorCode)) {
    	//如果当前"/dev/input" 文件节点存在则将这个路径加入Inotify监听
        addDeviceInputInotify();
    } else {
    	//如果当前"/dev/input" 文件节点不存在，则先将这个路径"/dev"加入Inotify监听(监听dev所有节点)
    	//因为有些嵌入式设备不一定一直存在输入设备，那么仅当/dev/input出现时（插入输入设备）才添加对/dev/input内容的监听, 
        addDeviceInotify();
        isDeviceInotifyAdded = true;
    }
    
	//V4L视频设备相关
    if (isV4lScanningEnabled() && !isDeviceInotifyAdded) {
        addDeviceInotify();
    } else {
        ALOGI("Video device scanning disabled");
    }

    struct epoll_event eventItem = {};
    eventItem.events = EPOLLIN | EPOLLWAKEUP; //设置监听epoll事件类型
    eventItem.data.fd = mINotifyFd;//要处理的事件相关的文件描述符
    
    //将mINotifyFd加入epoll监听的fd池子
    int result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mINotifyFd, &eventItem);


	//创建一个管道
    int wakeFds[2];
    result = pipe2(wakeFds, O_CLOEXEC);

    mWakeReadPipeFd = wakeFds[0];  //0为读端
    mWakeWritePipeFd = wakeFds[1];  //1为写端

    result = fcntl(mWakeReadPipeFd, F_SETFL, O_NONBLOCK);

    result = fcntl(mWakeWritePipeFd, F_SETFL, O_NONBLOCK);

    eventItem.data.fd = mWakeReadPipeFd;  //将mWakeReadPipeFd设置到eventItem.data.fd，当epoll有event到来会
    //将管道读端fd加入epoll监听的fd池子，当管道写端写入数据时，读端就换监听到epoll事件
    result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mWakeReadPipeFd, &eventItem);
}
```

到这里初始化InputFlinger就完成了

### 2\. 启动inputflinger，处理输入事件

前面`SystemServer`的1、2、3步都完成后，会往下执行

```cpp
	//4. 启动inputflinger，处理输入事件
          t.traceBegin("StartInputManager");
          //ims也注册wms的回调，用于通知wms一些事件发生
          inputManager.setWindowManagerCallbacks(wm.getInputManagerCallback());
          inputManager.start();  //启动ims，开始处理输入事件
          t.traceEnd();
```

#### start()

frameworks/base/services/core/java/com/android/server/input/InputManagerService.java

```java
    public void start() {
        Slog.i(TAG, "Starting input manager");
        mNative.start();  //到native中的方法

        // Add ourselves to the Watchdog monitors.
        Watchdog.getInstance().addMonitor(this); //加入Watchdog的检测列表中

		//一系列对settings中开关的状态值监听
		...
    }
```

#### nativeStart

frameworks/base/services/core/jni/com\_android\_server\_input\_InputManagerService.cpp

```cpp
static void nativeStart(JNIEnv* env, jobject nativeImplObj) {
    NativeInputManager* im = getNativeInputManager(env, nativeImplObj);
    
    status_t result = im->getInputManager()->start();
}
```

#### InputManager::start()

```cpp
status_t InputManager::start() {
    status_t result = mDispatcher->start();
    result = mReader->start();

    return OK;
}
```

分别调用`inputDispatcher`和`inputReader`的`start`方法

#### InputDispatcher::start()

```cpp
status_t InputDispatcher::start() {

	//创建线程InputDispatcher，线程体函数dispatchOnce(), 线程唤醒函数mLooper->wake()
    mThread = std::make_unique<InputThread>(
            "InputDispatcher", [this]() { dispatchOnce(); }, [this]() { mLooper->wake(); });
    return OK;
}
```

`InputThread`继承与Thead类  
frameworks/native/services/inputflinger/InputThread.cpp

```cpp
InputThread::InputThread(std::string name, std::function<void()> loop, std::function<void()> wake)
      : mName(name), mThreadWake(wake) {
    mThread = new InputThreadImpl(loop);
    mThread->run(mName.c_str(), ANDROID_PRIORITY_URGENT_DISPLAY);  //启动线程，线程优先级很高
}

class InputThreadImpl : public Thread {
public:
    explicit InputThreadImpl(std::function<void()> loop)
          : Thread(/* canCallJava */ true), mThreadLoop(loop) {}

    ~InputThreadImpl() {}

private:
    std::function<void()> mThreadLoop;

    bool threadLoop() override {
        mThreadLoop();
        return true;
    }
};


```

创建一个线程，用于分发事件，线程执行体`dispatchOnce()`

```cpp
void InputDispatcher::dispatchOnce() {
    nsecs_t nextWakeupTime = LONG_LONG_MAX;
    { // acquire lock
        std::scoped_lock _l(mLock);
        mDispatcherIsAlive.notify_all();

        // Run a dispatch loop if there are no pending commands.
        // The dispatch loop might enqueue commands to run afterwards.
        if (!haveCommandsLocked()) {
            dispatchOnceInnerLocked(&nextWakeupTime);  //处理事件的分发
        }

        // Run all pending commands if there are any.
        // If any commands were run then force the next poll to wake up immediately.
        if (runCommandsLockedInterruptable()) {  //处理命令队列中的命令
            nextWakeupTime = LONG_LONG_MIN;
        }

        // If we are still waiting for ack on some events,
        // we might have to wake up earlier to check if an app is anr'ing.
        const nsecs_t nextAnrCheck = processAnrsLocked();  //处理input ANR相关
        nextWakeupTime = std::min(nextWakeupTime, nextAnrCheck);

        // We are about to enter an infinitely long sleep, because we have no commands or
        // pending or queued events
        if (nextWakeupTime == LONG_LONG_MAX) {
            mDispatcherEnteredIdle.notify_all();  //线程进入idle状态
        }
    } // release lock

    // Wait for callback or timeout or wake.  (make sure we round up, not down)
    nsecs_t currentTime = now();
    int timeoutMillis = toMillisecondTimeoutDelay(currentTime, nextWakeupTime);
    mLooper->pollOnce(timeoutMillis);  //再次进入阻塞等待中
}
```

#### InputReader::start()

```cpp
status_t InputReader::start() {

    //创建线程InputReader，线程体函数loopOnce(), 线程唤醒函数mEventHub->wake()（调用EventHub的wake方法来唤醒）
    mThread = std::make_unique<InputThread>(
            "InputReader", [this]() { loopOnce(); }, [this]() { mEventHub->wake(); });
    return OK;
}

void InputReader::loopOnce() {
    int32_t oldGeneration;
    int32_t timeoutMillis;
    bool inputDevicesChanged = false;
    std::vector<InputDeviceInfo> inputDevices;
	...
	//调用EventHub的getEvents获取输入事件
    size_t count = mEventHub->getEvents(timeoutMillis, mEventBuffer, EVENT_BUFFER_SIZE);

    { // acquire lock
        std::scoped_lock _l(mLock);
        mReaderIsAliveCondition.notify_all();

        if (count) {
            processEventsLocked(mEventBuffer, count);
        }

        if (mNextTimeout != LLONG_MAX) {
            nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
            if (now >= mNextTimeout) {
                if (DEBUG_RAW_EVENTS) {
                    ALOGD("Timeout expired, latency=%0.3fms", (now - mNextTimeout) * 0.000001f);
                }
                mNextTimeout = LLONG_MAX;
                timeoutExpiredLocked(now);
            }
        }

        if (oldGeneration != mGeneration) {
            inputDevicesChanged = true;
            inputDevices = getInputDevicesLocked();
        }
    } // release lock

    // Send out a message that the describes the changed input devices.
    //如果输入设备有变化，通知其他服务
    if (inputDevicesChanged) {
        mPolicy->notifyInputDevicesChanged(inputDevices);
    }

    mQueuedListener.flush();
}
```

线程在循环从`mEventHub->getEvents`获取输入事件（这个方法会引起阻塞），如果获取到事件，EventHub会唤醒此线程，返回后会刷新`mQueuedListener`的事件队列, 唤醒`InputDispatcher`线程来消费。  
到这里inputflinger的启动就完成，开始正常等待输入事件的上报并处理。