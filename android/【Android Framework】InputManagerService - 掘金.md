## 回顾

回顾一下: 1.第一个启动的就是`init`进程，它解析了`init.rc`文件，启动各种`service`:`zygote`,`surfaceflinger`,`service_manager`。

2.接着就讲了`Zygote`，Zygote就是一个孵化器，它开启了`system_server`以及开启了`ZygoteServer`用来接收客户端的请求，当客户端请求来了之后就会`fork`出来子进程，并且初始化`binder` 和`进程信息`，为了加速`Zygote`还会预加载一些`class`和`Drawable`、`color`等系统资源。

3.接下来讲了`system_server`，它是系统启动管理`service`的`入口`，比如`AMS`、`PMS`、`WMS`等等，它加载了`framework-res.apk`,接着调用`startBootstrapService`,`startCoreService`,`startOtherService`开启非常多的服务，还开启了`WatchDog`，来监控service。

4.接着讲了`service_manager`，他是一个独立的进程，它存储了系统各种服务的`Binder`，我们经常通过`ServiceMananger`来获取，其中还详细说了`Binder`机制，`C/S`架构，大家要记住`客户端`、`Binder`、`Server`三端的工作流程。

5.之后讲了`Launcher`，它由`system_server`启动，通过`LauncherModel`进行`Binder`通信 通过`PMS`来查询所有的应用信息，然后绑定到`RecyclerView`中，它的点击事件是通过`ItemClickHandler`来处理。

6.接着讲了`AMS`是如何开启应用进程的,首先我们从`Launcher`的点击开始，调用到`Activity`的`startActivity`函数，通过`Instrumentation`的`execStartActivity`经过两次`IPC`(1.通过ServiceManager获取到ATMS 2.调用ATMS的startActivity) 调用到`AMS`端在AMS端进行了一系列的信息处理，会判断进程是否存在，`没有存在的话就会开启进程(通过Socket，给ZygoteServer发送信息)`,传入`entryPoint`为`ActivityThread`,通过`Zygote`来`fork`出来子进程（应用进程）调用`ActivityThread.main`，应用进程创建之后会调用到`AMS`，由`AMS`来`attachApplication`存储进程信息,然后告诉`客户端`，让客户端来创建`Application`，并在客户端创建成功之后 继续执行开启`Activity`的流程。客户端接收到`AMS`的数据之后会创建`loadedApk`,`Instrumentation` 以及`Application调用attach(attachBaseContext)`，调用`Instrumentation`的`callApplicationOncreate`执行`Application`的`Oncreate`周期.

7.应用执行完`Application`的`OnCreate`之后 回到`ATMS`的`attachApplication` 接着调用 `realStartActivityLocked` 创建了`ClientTransaction`，设置`callBack`为`LaunchActivityItem`添加了`stateRequest` 为`ResumeActivityItem`，然后通过`IApplicationThread` 回到客户端执行这两个事务,调用了`ActivityThread`的`scheduleTransaction` 函数，调用`executeCallBack` 执行了`LaunchActivityItem`的execute 他会调用ActivityThread的 `handleLaunchActivity`，会创建Activity Context，通过`Instrumentation.newActivity 反射创建Activity` 并调用`attach 绑定window` 再通过`Instrumentation`的`callActivityOnCreate`执行Activity的`onCreate`，在Activity的onCreate中分发监听给`ActivityLifecycleCallbacks`。最后设置`ActivityClientRecord`的state为`ON_CREATE`。 接着执行`executeLifecycleState`，调用了`cycleToPath`，之前设置了state为ON\_CREATE，所以会返回一个Int数组`{2}` 调用`performLifecycleSequence`会执行到ActivityThread的`handleStartActivity`分发`ActivityLifecycleCallbacks`，并且分发给Fragments，调用`Instrumentation`的`callActivityOnStart` 执行Activity的`onStart`并设置state为`ON_START`，接着执行`ResumeActivityItem`的`execute`，会调用到ActivityThread的`handleResumeActivity`，调用`performResume` 分发resume事件给ActivityLifecycleCallbacks，分发Fragments，调用Instrumentation的`callActivityOnResume` 执行Activity的onResume。 最后会调用`ActivityClientRecord.activity.makeVisible` 通过`WindowManager` 添加当前View 和 WMS(IPC) 通信 绘制UI，接着postResume 会执行 `ATMS`的`activityresume` 设置 AMS的Activity的状态。

8.接着讲了Service，介绍了Service是如何开启的，生命周期是怎么执行的，ANR 是如何弹出的。Service的启动分两种:`startService`和`bindService`。先回忆下startService:startService会调用到ContextImpl的startService，它会直接调用AMS的startService。在`AMS`这里会先检查Service是否可以执行(常驻内存、蓝牙、电源白名单允许直接启动服务)，接着调用`bringUpServiceLocked` 判断是否需要隔离进程如果非隔离 就看是否已经启动进程 执行`realStartServiceLocked`，否则是隔离进程 直接开启新进程。开启成功之后会将ServiceRecord添加到`mPendingServices`中去。进程创建之后，会调用`AMS`的`attachApplication` 接着处理`service(ActiveService)`，之前创建之后会添加到`mPendingServices`中，现在继续处理调用`realStartServiceLocked`来开启Service，在开启的过程中会埋入一个炸弹(给Handler发送一个`SERVICE_TIMEOUT_MSG`) 如果超时未处理会弹出`ANR`，然后调用`app.thread.scheduleCreateService`通知客户端创建反射`Service、Context` 调用`onCreate` 把`Service`存入到`mService`中，调用`AMS`的`serviceDoneExecuting`进行炸弹的拆除。 然后再埋炸弹 调用`app.thread.scheduleServiceArgs` 调用`service.onStartCommand`再通知AMS 拆除炸弹。 这样Service就运行起来了。

9.再回忆下bindService:首先`ServiceConnection`是无法跨进程通信的，所以在`LoadedApk.java`中帮我们封装了`InnerConnection` 它继承自Stub，也就是native层的JavaBBinder,然后通过`bindIsolatedService`将`sd(InnerConnection)`写入到Parcel中，调用`AMS(BinderProxy)`的`transact`进行IPC 通信，把InnerConnection存入到客户端的`nodes`中,以及给`AMS`的`refs_by_node`和`refs_by_desc`挂上`InnerConnection` 接着唤醒`AMS`,唤醒之后调用`onTransact`读取到传过来的`sd`并且包装成`BpBinder(BinderProxy)`返回,这样AMS就在`bindIsolatedService`的时候拿到了InnerConnection的BpBinder，接着AMS 通过`pkms`通过intent查找到服务端进程,创建`AppBindRecord`和`ConnectionRecord`,接着调用`bringUpServiceLocked` 看是否开启进程 如果没有开启就先开启进程，开启了进程之后 调用`realStartServiceLocked` 也埋了炸弹 接着调用 `app.thread.scheduleCreateService`通知客户端创建反射`Service、Context` 调用`onCreate` 把`Service`存入到`mService`中,调用`serviceDoneExecutingLocked`把炸弹拆除，再调用`requestServiceBindingsLocked` 去执行`r.app.thread.scheduleBindService` 执行服务端的onBind函数拿到了返回的`Binder`再调用AMS的`publishService`把服务发布到AMS（在服务端的进程创建`binder_proc(server)` 在AMS的`refs_by_node` 和`refs_by_desc 挂上server）`,`AMS`的`onTransact` 中读取到服务端返回的`server(Binder)`包装成`BpBinder`，然后调用`c.conn.connected(从refs_by_desc 和 refs_by_node 找到InnerConnection)`调用connect 所以到了客户端进程会把server的BpBinder 挂在`refs_by_desc`和`refs_by_node`上边。再调用`Servcie.onServiceConnected` 之后拆除炸弹。

10.获取ContentProvider 调用者进程调用到`AMS`中，如果`ContentProvider`已经发布了 通过`canRunHere`判断`ContentProvider`是否可以运行在调用者的进程中，如果`允许` 不会把已经发布的ContentProvider返回，而是返回新的ContentProviderHoder 但是会把`provider`设置成`null`。 如果`不允许` 设置OOMAdj 和 更新进程LRU 最终调用cpr.newHolder(conn)。如果`没有发布`还是会检查是否可以在调用者进程来创建，接下来看ContentProvider的进程是否创建，如果`没有创建`调用`startProcessLocked`启动进程。如果已经创建进程 调用ContentProvider进程的`proc.thread.scheduleInstallProvider`。 两种情况都会把ContentProvider添加到`mLaunchingProviders`和`mProviderMap`中。然后等待ContentProvider安装完成 唤醒。唤醒之后返回`ContentProviderHolder`。

11.安装ContentProvider 遍历AMS传递过来的`providers`调用`installProvider`反射创建`ContentProvider`调用`attachInfo` 执行`onCreate` 创建`ProviderClientRecord(持有provider和ContentProviderInfo 以及ContentProviderHolder)` 存入`mLocalProviders`和`mLocalProvidersByName` 返回`ContentProviderHolder`.然后调用AMS的publishContentProviders进行发布:存入mProviderMap中 一个是以ComponentName为key 一个是以authority为key(authority可以是多个;分割 但是dst是同一个)。然后唤醒 之前的`getContentProviderImpl`返回ContentProviderHolder， ，通过provider(IContentProvider BinderProxy) 再去进行ipc通信。 当然也可以在本进程创建。

具体的细节可以参考之前写的文章和视频：

[【Android FrameWork】第一个启动的程序--init](https://juejin.cn/post/7213733567606685753 "https://juejin.cn/post/7213733567606685753")

[【Android FrameWork】Zygote](https://juejin.cn/post/7214068367607119933 "https://juejin.cn/post/7214068367607119933")

[【Android FrameWork】SystemServer](https://juejin.cn/post/7214493929566945340 "https://juejin.cn/post/7214493929566945340")

[【Android FrameWork】ServiceManager(一)](https://juejin.cn/post/7216537771448598585#heading-4 "https://juejin.cn/post/7216537771448598585#heading-4")

[【Android FrameWork】ServiceManager(二)](https://juejin.cn/post/7216536069285675045#heading-9 "https://juejin.cn/post/7216536069285675045#heading-9")

[【Android Framework】Launcher3](https://juejin.cn/post/7218129062744227896#heading-1 "https://juejin.cn/post/7218129062744227896#heading-1")

[【Android Framework】ActivityManagerService(一)](https://juejin.cn/post/7219130999685808188 "https://juejin.cn/post/7219130999685808188")

[【Android Framework】ActivityManagerService(二)](https://juejin.cn/post/7220439797565767741 "https://juejin.cn/post/7220439797565767741")

[【Android Framework】# Service](https://juejin.cn/post/7223687532067471420 "https://juejin.cn/post/7223687532067471420")

[【Android Framework】# Broadcast](https://juejin.cn/post/7224433107775471653 "https://juejin.cn/post/7224433107775471653")

[【Android Framework】# ContentProvider](https://juejin.cn/post/7226185606829031479 "https://juejin.cn/post/7226185606829031479")

## 介绍

InputManagerService是一个系统服务，主要处理Input事件的传递，包括键盘、鼠标、触摸屏等等，它和WMS密切相关。

## 正文

## 1.InputManagerService的启动

IMS是由system\_server启动的。

```

private void startOtherServices() {
    //创建InputManagerService
    inputManager = new InputManagerService(context);
    //创建WindowManagerService 传入IMS 进行绑定
    wm = WindowManagerService.main(context, inputManager, !mFirstBoot, mOnlyCore,
                    new PhoneWindowManager(), mActivityManagerService.mActivityTaskManager);
    //添加WindowManagerService
   ServiceManager.addService(Context.WINDOW_SERVICE, wm, /* allowIsolated= */ false,
                    DUMP_FLAG_PRIORITY_CRITICAL | DUMP_FLAG_PROTO);
   //添加InputManagerService
   ServiceManager.addService(Context.INPUT_SERVICE, inputManager,
                    /* allowIsolated= */ false, DUMP_FLAG_PRIORITY_CRITICAL);
   //设置WindowManagerCallbacks
   inputManager.setWindowManagerCallbacks(wm.getInputManagerCallback());
   //开启InputManagerService
   inputManager.start();
}

文件目录:/frameworks/base/services/core/java/com/android/server/input/InputManagerService.java

我们先看看构造函数:
    public InputManagerService(Context context) {
        this.mContext = context;
        //创建handler
        this.mHandler = new InputManagerHandler(DisplayThread.get().getLooper());
        //调用nativeInit创建Native层的InputManagerService 传入了MessageQueue
        mPtr = nativeInit(this, mContext, mHandler.getLooper().getQueue());
        //添加到本地的服务集合中
        LocalServices.addService(InputManagerInternal.class, new LocalService());
    }

    private static native long nativeInit(InputManagerService service,
            Context context, MessageQueue messageQueue);
            
 到Native层看看
 文件目录:/frameworks/base/services/core/jni/com_android_server_input_InputManagerService.cpp

static jlong nativeInit(JNIEnv* env, jclass /* clazz */,
        jobject serviceObj, jobject contextObj, jobject messageQueueObj) {
        //获取到Native层的MessageQueue
    sp<MessageQueue> messageQueue = android_os_MessageQueue_getMessageQueue(env, messageQueueObj);
    //创建 NativeInputManager 传入了java层的引用 以及Looper
    //创建了InputMananger 内部包含了InputDispather和InputCalssifier 然后创建了InputReader 包含了readerPolicy和mClassifier也就是NativeInputManager的this
    NativeInputManager* im = new NativeInputManager(contextObj, serviceObj,
            messageQueue->getLooper());
    //强引用+1
    im->incStrong(0);
    return reinterpret_cast<jlong>(im);
}
 
//构造函数
NativeInputManager::NativeInputManager(jobject contextObj,
        jobject serviceObj, const sp<Looper>& looper) :
        mLooper(looper), mInteractive(true) {
    JNIEnv* env = jniEnv();
    //获取到Java层的InputManagerService
    mServiceObj = env->NewGlobalRef(serviceObj);

    mInteractive = true;
    //创建InputManager
    mInputManager = new InputManager(this, this);
    //将InputManager添加到ServiceManager 命名为inputflinger
    defaultServiceManager()->addService(String16("inputflinger"),
            mInputManager, false);
}


//readerPolicy dispatcherPolicy都是NativeInputManager
InputManager::InputManager(
        const sp<InputReaderPolicyInterface>& readerPolicy,
        const sp<InputDispatcherPolicyInterface>& dispatcherPolicy) {
     //创建InputDispatcher对象 分发器
    mDispatcher = new InputDispatcher(dispatcherPolicy);
    //创建InputClassifier对象  分类器
    mClassifier = new InputClassifier(mDispatcher);
    //创建InputReader
    mReader = createInputReader(readerPolicy, mClassifier);
    //创建InputReaderThread 和 InputDispatcherThread
    initialize();
}

//创建InputReader
sp<InputReaderInterface> createInputReader(
        const sp<InputReaderPolicyInterface>& policy,
        const sp<InputListenerInterface>& listener) {
    //创建EventHub 返回创建的InputReader 
    return new InputReader(new EventHub(), policy, listener);
}


//创建EventHub 创建管道 (一个读 一个写 当写端 写入数据的时候会唤醒读端)
EventHub::EventHub(void) :
        mBuiltInKeyboardId(NO_BUILT_IN_KEYBOARD), mNextDeviceId(1), mControllerNumbers(),
        mOpeningDevices(nullptr), mClosingDevices(nullptr),
        mNeedToSendFinishedDeviceScan(false),
        mNeedToReopenDevices(false), mNeedToScanDevices(true),
        mPendingEventCount(0), mPendingEventIndex(0), mPendingINotify(false) {
    acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_ID);

    //创建Epoll 监听设备节点
    mEpollFd = epoll_create1(EPOLL_CLOEXEC);

    //初始化inotify
    mINotifyFd = inotify_init();
    //添加监听 设备节点的 /dev/input 目录的创建和删除
    mInputWd = inotify_add_watch(mINotifyFd, DEVICE_PATH, IN_DELETE | IN_CREATE);
   
    
    struct epoll_event eventItem;
    memset(&eventItem, 0, sizeof(eventItem));
    //监听inotify 可读事件
    eventItem.events = EPOLLIN;
    eventItem.data.fd = mINotifyFd;
    //把mInotifyFd添加到epoll 注册缓冲区非空事件 ，当有数据流入的时候会唤醒
    int result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mINotifyFd, &eventItem);
   
    int wakeFds[2];
   //创建管道 一个读 一个写 
    result = pipe(wakeFds);
    mWakeReadPipeFd = wakeFds[0];
    mWakeWritePipeFd = wakeFds[1];
    //设置为非阻塞
    result = fcntl(mWakeReadPipeFd, F_SETFL, O_NONBLOCK);
    result = fcntl(mWakeWritePipeFd, F_SETFL, O_NONBLOCK);
    eventItem.data.fd = mWakeReadPipeFd;//如果给写管道写入数据 会唤醒读端来处理
    //把管道的读端加入到epoll中，当管道缓冲区数据可读的时候会提醒我们进行数据的读取。为什么要创建管道呢，因为InputReader在执行getEvents的时候会因为没有事件而阻塞在epoll_wait，有时候希望可以立刻唤醒InputReader处理一些请求，这个时候只需要向mWakeWritePipeFd 写入任意数据，此时读端就有数据可以读了，就可以唤醒InputReader了
    result = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mWakeReadPipeFd, &eventItem);

}

//创建InputReader
InputReader::InputReader(const sp<EventHubInterface>& eventHub,
        const sp<InputReaderPolicyInterface>& policy,
        const sp<InputListenerInterface>& listener) :
        mContext(this), mEventHub(eventHub), mPolicy(policy),
        mNextSequenceNum(1), mGlobalMetaState(0), mGeneration(1),
        mDisableVirtualKeysTimeout(LLONG_MIN), mNextTimeout(LLONG_MAX),
        mConfigurationChangesToRefresh(0) {
    //创建InputListenerQueue 把事件分发给InputClassifier进行分类分发
    mQueuedListener = new QueuedInputListener(listener);

    { 
        //刷新配置
        refreshConfigurationLocked(0);
        //更新InputReader::GlobalMetaState 和键盘输入设备的meta按键相关
        updateGlobalMetaStateLocked();
    } 
}

void InputReader::refreshConfigurationLocked(uint32_t changes) {
    //获取mPolicy
    mPolicy->getReaderConfiguration(&mConfig);
    //过滤设备
    mEventHub->setExcludedDevices(mConfig.excludedDeviceNames);

    if (changes) {//传递的是0 不执行
    }
}

void InputReader::updateGlobalMetaStateLocked() {
    mGlobalMetaState = 0;

    for (size_t i = 0; i < mDevices.size(); i++) {
        InputDevice* device = mDevices.valueAt(i);
        mGlobalMetaState |= device->getMetaState();
    }
}

void InputManager::initialize() {
    mReaderThread = new InputReaderThread(mReader);
    mDispatcherThread = new InputDispatcherThread(mDispatcher);
}

```

`IMS`创建完成，在Native层创建了NativeInputManager(InputManager 包含了MInputDispatcher和 mReader),还创建了管道，一个读一个写，当给管道写输入数据的时候会唤醒读端来处理。 并且创建了两个Thread `InputReaderThread` 和 `InputDispatcherThread`

构造函数看完之后我们看看WMS的main函数是怎么处理的inputManager：

文件目录:`/frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java`

```

    public static WindowManagerService main(final Context context, final InputManagerService im,
            final boolean showBootMsgs, final boolean onlyCore, WindowManagerPolicy policy,
            ActivityTaskManagerService atm) {
        return main(context, im, showBootMsgs, onlyCore, policy, atm,
                SurfaceControl.Transaction::new);
    }
    
    public static WindowManagerService main(final Context context, final InputManagerService im,
            final boolean showBootMsgs, final boolean onlyCore, WindowManagerPolicy policy,
            ActivityTaskManagerService atm, TransactionFactory transactionFactory) {
        DisplayThread.getHandler().runWithScissors(() ->
                sInstance = new WindowManagerService(context, im, showBootMsgs, onlyCore, policy,
                        atm, transactionFactory), 0);
        return sInstance;
    }


    private WindowManagerService(Context context, InputManagerService inputManager,
            boolean showBootMsgs, boolean onlyCore, WindowManagerPolicy policy,
            ActivityTaskManagerService atm, TransactionFactory transactionFactory) {
           //....
           //把创建好的ims赋值给mInputManager
        mInputManager = inputManager;
        //....
        mTaskPositioningController = new TaskPositioningController(
                this, mInputManager, mActivityTaskManager, mH.getLooper());
        //....
    }
```

WMS的构造函数中并没有做复杂的处理。看看`setWindowManagerCallbacks`

```
public void setWindowManagerCallbacks(WindowManagerCallbacks callbacks) {
    mWindowManagerCallbacks = callbacks;
}

//回调接口
public interface WindowManagerCallbacks {
    public void notifyConfigurationChanged();
    //...
}
```

这里传递的callbacks是`InputManagerCallback` 文件目录:`/frameworks/base/services/core/java/com/android/server/wm/InputManagerCallback.java`

```
     //构造函数
    public InputManagerCallback(WindowManagerService service) {
        mService = service;
    }
```

接着调用了`inputManager.start()`

```
    public void start() {
        //调用native的start
        nativeStart(mPtr);

        //添加到看门狗监听
        Watchdog.getInstance().addMonitor(this);

        //动态注册广播
        mContext.registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                updatePointerSpeedFromSettings();
                updateShowTouchesFromSettings();
                updateAccessibilityLargePointerFromSettings();
            }
        }, new IntentFilter(Intent.ACTION_USER_SWITCHED), null, mHandler);

    }
    

static void nativeStart(JNIEnv* env, jclass /* clazz */, jlong ptr) {
    NativeInputManager* im = reinterpret_cast<NativeInputManager*>(ptr);
    //调用InputManager.start
    status_t result = im->getInputManager()->start();
}

status_t InputManager::start() {
    //执行mDispatcherThread的run函数 由于它继承自Thread 这个我们之前就已经讲过了Thread的执行流程 所以会执行到threadLoop函数 （具体在Launcher章节）
    status_t result = mDispatcherThread->run("InputDispatcher", PRIORITY_URGENT_DISPLAY);
    //执行mReaderThread的run函数
    result = mReaderThread->run("InputReader", PRIORITY_URGENT_DISPLAY);
    return OK;
}

//执行dispatchOnce
bool InputDispatcherThread::threadLoop() {
    mDispatcher->dispatchOnce();
    return true;
}

//执行loopOnce
bool InputReaderThread::threadLoop() {
    mReader->loopOnce();
    return true;
}
```

### 1.事件的获取

我们看看loopOnce是如何获取事件的

```

void InputReader::loopOnce() {
    int32_t oldGeneration;
    int32_t timeoutMillis;
    bool inputDevicesChanged = false;
    std::vector<InputDeviceInfo> inputDevices;
    { 
      //从EventHub读取原始事件
    size_t count = mEventHub->getEvents(timeoutMillis, mEventBuffer, EVENT_BUFFER_SIZE);

    { 

        if (count) {
            //处理事件
            processEventsLocked(mEventBuffer, count);
        }
    }

    //分发事件
    mQueuedListener->flush();
}

//处理事件
void InputReader::processEventsLocked(const RawEvent* rawEvents, size_t count) {
    for (const RawEvent* rawEvent = rawEvents; count;) {
        int32_t type = rawEvent->type;
        size_t batchSize = 1;
        if (type < EventHubInterface::FIRST_SYNTHETIC_EVENT) {//不是增加 删除 扫描 设备
            int32_t deviceId = rawEvent->deviceId;
            //调用processEventsForDeviceLocked 让对应的设备处理事件
            processEventsForDeviceLocked(deviceId, rawEvent, batchSize);
        } else {
            switch (rawEvent->type) {
            case EventHubInterface::DEVICE_ADDED://新增设备
                break;
            case EventHubInterface::DEVICE_REMOVED://移除设备
                break;
            case EventHubInterface::FINISHED_DEVICE_SCAN://扫描设备
                break;
            }
        }
        count -= batchSize;
        rawEvent += batchSize;
    }
}

//根据deviceId 交给设备来处理事件
void InputReader::processEventsForDeviceLocked(int32_t deviceId,
        const RawEvent* rawEvents, size_t count) {
    ssize_t deviceIndex = mDevices.indexOfKey(deviceId);
    InputDevice* device = mDevices.valueAt(deviceIndex);
    device->process(rawEvents, count);
}

//设备处理事件
void InputDevice::process(const RawEvent* rawEvents, size_t count) {
   
    for (const RawEvent* rawEvent = rawEvents; count != 0; rawEvent++) {
        if (mDropUntilNextSync) {//mDropUntilNextSync 为false
          
        } else if (rawEvent->type == EV_SYN && rawEvent->code == SYN_DROPPED) {
        } else {
        //InputMapper用来将原始的输入事件转换为处理过的输入数据 一个输入设备可能对应多个InputMapper,他会有多个子类 我们主要分析TouchInputMapper
            for (InputMapper* mapper : mMappers) {
                mapper->process(rawEvent);
            }
        }
        --count;
    }
}
//处理Touch事件
void TouchInputMapper::process(const RawEvent* rawEvent) {
    //处理鼠标和触摸键盘 按键事件
    mCursorButtonAccumulator.process(rawEvent);
    //处理鼠标滑动
    mCursorScrollAccumulator.process(rawEvent);
    //处理手写笔之类的
    mTouchButtonAccumulator.process(rawEvent);

    if (rawEvent->type == EV_SYN && rawEvent->code == SYN_REPORT) {
        reportEventForStatistics(rawEvent->when);
        //调用sync进行事件的同步
        sync(rawEvent->when);
    }
}

//同步touch
void TouchInputMapper::sync(nsecs_t when) {
    const RawState* last = mRawStatesPending.empty() ?
            &mCurrentRawState : &mRawStatesPending.back();


    // 同步touch
    syncTouch(when, next);

    //处理原始事件
    processRawTouches(false /*timeout*/);
}


//单点触摸同步
void SingleTouchInputMapper::syncTouch(nsecs_t when, RawState* outState) {
    if (mTouchButtonAccumulator.isToolActive()) {
        outState->rawPointerData.pointerCount = 1;
        outState->rawPointerData.idToIndex[0] = 0;
        //是否悬停
        bool isHovering = mTouchButtonAccumulator.getToolType() != AMOTION_EVENT_TOOL_TYPE_MOUSE
                && (mTouchButtonAccumulator.isHovering()
                        || (mRawPointerAxes.pressure.valid
                                && mSingleTouchMotionAccumulator.getAbsolutePressure() <= 0));
        outState->rawPointerData.markIdBit(0, isHovering);

        RawPointerData::Pointer& outPointer = outState->rawPointerData.pointers[0];
        outPointer.id = 0;
        //记录x y 坐标
        outPointer.x = mSingleTouchMotionAccumulator.getAbsoluteX();
        outPointer.y = mSingleTouchMotionAccumulator.getAbsoluteY();
        outPointer.pressure = mSingleTouchMotionAccumulator.getAbsolutePressure();
        outPointer.touchMajor = 0;
        outPointer.touchMinor = 0;
        outPointer.toolMajor = mSingleTouchMotionAccumulator.getAbsoluteToolWidth();
        outPointer.toolMinor = mSingleTouchMotionAccumulator.getAbsoluteToolWidth();
        outPointer.orientation = 0;
        outPointer.distance = mSingleTouchMotionAccumulator.getAbsoluteDistance();
        outPointer.tiltX = mSingleTouchMotionAccumulator.getAbsoluteTiltX();
        outPointer.tiltY = mSingleTouchMotionAccumulator.getAbsoluteTiltY();
        outPointer.toolType = mTouchButtonAccumulator.getToolType();
        if (outPointer.toolType == AMOTION_EVENT_TOOL_TYPE_UNKNOWN) {
            outPointer.toolType = AMOTION_EVENT_TOOL_TYPE_FINGER;
        }
        outPointer.isHovering = isHovering;
    }
}


//处理原始触摸
void TouchInputMapper::processRawTouches(bool timeout) {
   
    const size_t N = mRawStatesPending.size();
    size_t count;
    for(count = 0; count < N; count++) {//遍历所有原始事件
        const RawState& next = mRawStatesPending[count];
        //调用cookAndDispatch 进行分发
        cookAndDispatch(mCurrentRawState.when);
    }
}

//分发
void TouchInputMapper::cookAndDispatch(nsecs_t when) {
   //.....

        if (!mCurrentMotionAborted) {
            dispatchButtonRelease(when, policyFlags);
            dispatchHoverExit(when, policyFlags);
            //分发touches
            dispatchTouches(when, policyFlags);
            dispatchHoverEnterAndMove(when, policyFlags);
            dispatchButtonPress(when, policyFlags);
        }

//...
}


//调用dispatchMotion 进行分发 motion
void TouchInputMapper::dispatchTouches(nsecs_t when, uint32_t policyFlags) {
//....
            dispatchMotion(when, policyFlags, mSource,
                    AMOTION_EVENT_ACTION_MOVE, 0, 0, metaState, buttonState,
                    AMOTION_EVENT_EDGE_FLAG_NONE,
                    mCurrentCookedState.deviceTimestamp,
                    mCurrentCookedState.cookedPointerData.pointerProperties,
                    mCurrentCookedState.cookedPointerData.pointerCoords,
                    mCurrentCookedState.cookedPointerData.idToIndex,
                    currentIdBits, -1,
                    mOrientedXPrecision, mOrientedYPrecision, mDownTime);
   
}



void TouchInputMapper::dispatchMotion(nsecs_t when, uint32_t policyFlags, uint32_t source,
        int32_t action, int32_t actionButton, int32_t flags,
        int32_t metaState, int32_t buttonState, int32_t edgeFlags, uint32_t deviceTimestamp,
        const PointerProperties* properties, const PointerCoords* coords,
        const uint32_t* idToIndex, BitSet32 idBits, int32_t changedId,
        float xPrecision, float yPrecision, nsecs_t downTime) {
    PointerCoords pointerCoords[MAX_POINTERS];
    PointerProperties pointerProperties[MAX_POINTERS];
    uint32_t pointerCount = 0;
    //....
    //创建NotifyMotionArgs
    NotifyMotionArgs args(mContext->getNextSequenceNum(), when, deviceId,
            source, displayId, policyFlags,
            action, actionButton, flags, metaState, buttonState, MotionClassification::NONE,
            edgeFlags, deviceTimestamp, pointerCount, pointerProperties, pointerCoords,
            xPrecision, yPrecision, downTime, std::move(frames));
    //通知listener,看看Listener是谁(QueuedInputListener)
    getListener()->notifyMotion(&args);
}

InputListenerInterface* InputReader::ContextImpl::getListener() {
//获取到inputReader的mQueuedListener 返回(InputClassifier)
    return mReader->mQueuedListener.get();
}
//把args存入mArgsQueue
void QueuedInputListener::notifyMotion(const NotifyMotionArgs* args) {
    mArgsQueue.push_back(new NotifyMotionArgs(*args));
}

//调用flush 分发
void QueuedInputListener::flush() {
    size_t count = mArgsQueue.size();
    for (size_t i = 0; i < count; i++) {
        NotifyArgs* args = mArgsQueue[i];
        //调用对应args的notify 我们这里关注的是NotifyMotionArgs
        args->notify(mInnerListener);
        delete args;
    }
    mArgsQueue.clear();
}


void NotifyMotionArgs::notify(const sp<InputListenerInterface>& listener) const {
    //调用mInnerListener的notifyMotion 也就是InputClassifier
    listener->notifyMotion(this);
}


//调用notifyMotion 更新motion 也就是调用到dispatcher的notifyMotion
void InputClassifier::notifyMotion(const NotifyMotionArgs* args) {
    NotifyMotionArgs newArgs(*args);
    newArgs.classification = mMotionClassifier->classify(newArgs);
    mListener->notifyMotion(&newArgs);
}


void InputDispatcher::notifyMotion(const NotifyMotionArgs* args) {
    //安全性校验
    if (!validateMotionEvent(args->action, args->actionButton,
                args->pointerCount, args->pointerProperties)) {
        return;
    }

    uint32_t policyFlags = args->policyFlags;
    policyFlags |= POLICY_FLAG_TRUSTED;

    android::base::Timer t;
    //预处理(NativeInputManager最终会交给PhoneWindowManager 由PhoneWindow来决定是否要拦截)
    mPolicy->interceptMotionBeforeQueueing(args->displayId, args->eventTime, /*byref*/ policyFlags);
    bool needWake;
    { 
        //创建MotionEntry
        MotionEntry* newEntry = new MotionEntry(args->sequenceNum, args->eventTime,
                args->deviceId, args->source, args->displayId, policyFlags,
                args->action, args->actionButton, args->flags,
                args->metaState, args->buttonState, args->classification,
                args->edgeFlags, args->xPrecision, args->yPrecision, args->downTime,
                args->pointerCount, args->pointerProperties, args->pointerCoords, 0, 0);
        //入队 后边dispatch会取出来
        needWake = enqueueInboundEventLocked(newEntry);
        mLock.unlock();
    }
    if (needWake) {//唤醒分发线程
        mLooper->wake();
    }
}


bool InputDispatcher::enqueueInboundEventLocked(EventEntry* entry) {
    bool needWake = mInboundQueue.isEmpty();
    mInboundQueue.enqueueAtTail(entry);
    traceInboundQueueLengthLocked();

    switch (entry->type) {
    case EventEntry::TYPE_KEY: {//key事件
        KeyEntry* keyEntry = static_cast<KeyEntry*>(entry);
        if (isAppSwitchKeyEvent(keyEntry)) {
            if (keyEntry->action == AKEY_EVENT_ACTION_DOWN) {
                mAppSwitchSawKeyDown = true;
            } else if (keyEntry->action == AKEY_EVENT_ACTION_UP) {
                if (mAppSwitchSawKeyDown) {
                    mAppSwitchDueTime = keyEntry->eventTime + APP_SWITCH_TIMEOUT;
                    mAppSwitchSawKeyDown = false;
                    needWake = true;
                }
            }
        }
        break;
    }

    case EventEntry::TYPE_MOTION: {//motion事件
        MotionEntry* motionEntry = static_cast<MotionEntry*>(entry);
        if (motionEntry->action == AMOTION_EVENT_ACTION_DOWN
                && (motionEntry->source & AINPUT_SOURCE_CLASS_POINTER)
                && mInputTargetWaitCause == INPUT_TARGET_WAIT_CAUSE_APPLICATION_NOT_READY
                && mInputTargetWaitApplicationToken != nullptr) {
            int32_t displayId = motionEntry->displayId;
            int32_t x = int32_t(motionEntry->pointerCoords[0].
                    getAxisValue(AMOTION_EVENT_AXIS_X));
            int32_t y = int32_t(motionEntry->pointerCoords[0].
                    getAxisValue(AMOTION_EVENT_AXIS_Y));
                    //查找到windowHandle
            sp<InputWindowHandle> touchedWindowHandle = findTouchedWindowAtLocked(displayId, x, y);
            if (touchedWindowHandle != nullptr
                    && touchedWindowHandle->getApplicationToken()
                            != mInputTargetWaitApplicationToken) {
                mNextUnblockedEvent = motionEntry;
                needWake = true;
            }
        }
        break;
    }
    }

    return needWake;
}

//获取事件

size_t EventHub::getEvents(int timeoutMillis, RawEvent* buffer, size_t bufferSize) {

    AutoMutex _l(mLock);
    //创建事件读取的buffer 结构体
    struct input_event readBuffer[bufferSize];
    
    RawEvent* event = buffer;
    //bufferSize = 256
    size_t capacity = bufferSize;
    bool awoken = false;
    for (;;) {
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
        //重新打开输入设备  mNeedToReopenDevices 默认为false
        if (mNeedToReopenDevices) {
            mNeedToReopenDevices = false;
            closeAllDevicesLocked();
            mNeedToScanDevices = true;
            break; 
        }
        while (mClosingDevices) {//mClosingDevices 默认是0 所以也不会进来
            Device* device = mClosingDevices;
            mClosingDevices = device->next;
            event->when = now;
            event->deviceId = (device->id == mBuiltInKeyboardId) ?
                    ReservedInputDeviceId::BUILT_IN_KEYBOARD_ID : device->id;
            event->type = DEVICE_REMOVED;//把事件的类型设置为移除设备
            event += 1;
            delete device;
            mNeedToSendFinishedDeviceScan = true;
            if (--capacity == 0) {
                break;
            }
        }

        if (mNeedToScanDevices) {//在构造函数中设置为true 所以需要对加载的设备进行扫描
            mNeedToScanDevices = false;
            scanDevicesLocked();//根据/dev/input来找到设备
            mNeedToSendFinishedDeviceScan = true;
        }

        while (mOpeningDevices != nullptr) {//上边扫描完成之后会给mOpeningDevices赋值 所以这里不为null，存储一些设备的信息
            Device* device = mOpeningDevices;
            mOpeningDevices = device->next;
            event->when = now;
            event->deviceId = device->id == mBuiltInKeyboardId ? 0 : device->id;
            event->type = DEVICE_ADDED;
            event += 1;
            mNeedToSendFinishedDeviceScan = true;
            if (--capacity == 0) {
                break;
            }
        }

        if (mNeedToSendFinishedDeviceScan) {//上边会修改为true 所以这里会执行 设置设备的时间和类型
            mNeedToSendFinishedDeviceScan = false;
            event->when = now;
            event->type = FINISHED_DEVICE_SCAN;
            event += 1;
            if (--capacity == 0) {
                break;
            }
        }

        bool deviceChanged = false;
        while (mPendingEventIndex < mPendingEventCount) {
            const struct epoll_event& eventItem = mPendingEventItems[mPendingEventIndex++];
            if (eventItem.data.fd == mINotifyFd) {
                if (eventItem.events & EPOLLIN) {
                    mPendingINotify = true;
                } else {
                }
                continue;
            }

            if (eventItem.data.fd == mWakeReadPipeFd) {//读管道 唤醒
                if (eventItem.events & EPOLLIN) {
                    awoken = true;
                    char buffer[16];
                    ssize_t nRead;
                    do {
                    //从管道中读取
                        nRead = read(mWakeReadPipeFd, buffer, sizeof(buffer));
                    } while ((nRead == -1 && errno == EINTR) || nRead == sizeof(buffer));
                } else {
                }
                continue;
            }
            //根据fd获取到设备
            Device* device = getDeviceByFdLocked(eventItem.data.fd);
            if (!device) {
                continue;
            }
            if (eventItem.events & EPOLLIN) {
            //开始读取原始事件
                int32_t readSize = read(device->fd, readBuffer,
                        sizeof(struct input_event) * capacity);
                if (readSize == 0 || (readSize < 0 && errno == ENODEV)) {
                    deviceChanged = true;
                    closeDeviceLocked(device);
                } else {
                    int32_t deviceId = device->id == mBuiltInKeyboardId ? 0 : device->id;

                    size_t count = size_t(readSize) / sizeof(struct input_event);
                    //保存读取的信息
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
                    if (capacity == 0) {
                        mPendingEventIndex -= 1;
                        break;
                    }
                }
            } else if (eventItem.events & EPOLLHUP) {
                deviceChanged = true;
                closeDeviceLocked(device);
            } else {
            }
        }
        //mPendingINotify = true  并且所有数据都读取完成
        if (mPendingINotify && mPendingEventIndex >= mPendingEventCount) {
            mPendingINotify = false;
            //看看readNotifyLocked都做了什么(读取存储在mInotifyFd中的Notify事件，打开和关闭设备)
            readNotifyLocked();
            deviceChanged = true;
        }

        mPendingEventIndex = 0;
        //等待mEpollFd写数据的事件的发生，如果有变化 会登记设备节点到mPendingEventItems 否则阻塞等待
        int pollResult = epoll_wait(mEpollFd, mPendingEventItems, EPOLL_MAX_EVENTS, timeoutMillis);
    }

    // 返回读取的事件
    return event - buffer;
}

status_t EventHub::readNotifyLocked() {
    int res;
    char event_buf[512];
    int event_size;
    int event_pos = 0;
    struct inotify_event *event;
    //读取存储在mInotifyFd中的Notify事件 如果input目录新增或删除 都会到这里来
    res = read(mINotifyFd, event_buf, sizeof(event_buf));
    if(res < (int)sizeof(*event)) {
        if(errno == EINTR)
            return 0;
        return -1;
    }

    while(res >= (int)sizeof(*event)) {
        event = (struct inotify_event *)(event_buf + event_pos);
        if(event->len) {
            if (event->wd == mInputWd) {
                std::string filename = StringPrintf("%s/%s", DEVICE_PATH, event->name);
                if(event->mask & IN_CREATE) {//打开设备
                    openDeviceLocked(filename.c_str());
                } else {//关闭设备
                    closeDeviceByPathLocked(filename.c_str());
                }
            }
            else if (event->wd == mVideoWd) {
                if (isV4lTouchNode(event->name)) {
                    std::string filename = StringPrintf("%s/%s", VIDEO_DEVICE_PATH, event->name);
                    if (event->mask & IN_CREATE) {
                        openVideoDeviceLocked(filename);
                    } else {
                        closeVideoDeviceByPathLocked(filename);
                    }
                }
            }
        }
        event_size = sizeof(*event) + event->len;
        res -= event_size;
        event_pos += event_size;
    }
    return 0;
}


```

总结下：InputReader通过EventHub.getEvents读取原始事件的RawEvent，接着调用processEventsLocked将原始事件转换为NotifyArgs，然后存储到InputReader的QueueInputListener的mQueuedListener内部的mArgsQueue中进行等待分发。 InputReader接着调用QueuedInputListener的flush把存储的事件发送到InputDispatcher 最终封装成EventEntry类型 添加到了InputDispatcher的mInBoundQueue中，并且唤醒了InputDispatcher线程.

### 2.事件的分发

我们看看dispatchOnce 是如何分发事件的

```
void InputDispatcher::dispatchOnce() {
    nsecs_t nextWakeupTime = LONG_LONG_MAX;
    { 
        //判断是否有指令需要执行 如果没有调用 dispatchOnceInnerLocked 处理输入事件
        if (!haveCommandsLocked()) {
            dispatchOnceInnerLocked(&nextWakeupTime);
        }

        //处理事件，设置nextWakeupTime为LONG_LONG_MIN,立刻唤醒线程处理输入事件
        if (runCommandsLockedInterruptible()) {
            nextWakeupTime = LONG_LONG_MIN;
        }
    } 
    // 等待唤醒
    nsecs_t currentTime = now();
    int timeoutMillis = toMillisecondTimeoutDelay(currentTime, nextWakeupTime);
    //调用pollOnce把当前线程挂起，后续InputReader读取线程会把新的事件发送给InputDispatcher 会把事件发送给InputEventReceiver
    mLooper->pollOnce(timeoutMillis);
}



void InputDispatcher::dispatchOnceInnerLocked(nsecs_t* nextWakeupTime) {
    //调度器被冻结 不需要处理超时或者任何事件 直接return
    if (mDispatchFrozen) {
        return;
    }
    //准备获取一个新的事件
    if (! mPendingEvent) {
        if (mInboundQueue.isEmpty()) {//如果队列为空
            if (!mPendingEvent) {
                return;
            }
        } else {
            //从mInboundQueue中获取等处理的输入事件 (InputReader线程通过enqueueInBoundEventLocked 加入需要处理的事件)
            mPendingEvent = mInboundQueue.dequeueAtHead();
            traceInboundQueueLengthLocked();
        }
        //ANR
        resetANRTimeoutsLocked();
    }

     //.....

    switch (mPendingEvent->type) {
    case EventEntry::TYPE_KEY: {//处理按键
        done = dispatchKeyLocked(currentTime, typedEntry, &dropReason, nextWakeupTime);
        break;
    }

    case EventEntry::TYPE_MOTION: {//处理touch
        MotionEntry* typedEntry = static_cast<MotionEntry*>(mPendingEvent);
        //调用dispatchMotionLocked 进行分发
        done = dispatchMotionLocked(currentTime, typedEntry,
                &dropReason, nextWakeupTime);
        break;
    }
    }
}

bool InputDispatcher::dispatchMotionLocked(
        nsecs_t currentTime, MotionEntry* entry, DropReason* dropReason, nsecs_t* nextWakeupTime) {
    // 预处理
    if (! entry->dispatchInProgress) {
        entry->dispatchInProgress = true;
        logOutboundMotionDetails("dispatchMotion - ", entry);
    }

    bool isPointerEvent = entry->source & AINPUT_SOURCE_CLASS_POINTER;

    //事件源 会把所有可以接收当前输入事件的窗口加入到inputTarget中
    std::vector<InputTarget> inputTargets;

    bool conflictingPointerActions = false;
    int32_t injectionResult;
    if (isPointerEvent) {
        // 触摸屏幕 调用findTouchedWindowTargetsLocked 查找当前的window 分发window 往inputTargets中填充数据
        injectionResult = findTouchedWindowTargetsLocked(currentTime,
                entry, inputTargets, nextWakeupTime, &conflictingPointerActions);
    } else {
        // 光标
        injectionResult = findFocusedWindowTargetsLocked(currentTime,
                entry, inputTargets, nextWakeupTime);
    }
    if (injectionResult == INPUT_EVENT_INJECTION_PENDING) {
        return false;
    }
    //.....
    //分发事件 给inputTargets
    dispatchEventLocked(currentTime, entry, inputTargets);
    return true;
}



//看看是如何定位window,填充数据的
int32_t InputDispatcher::findTouchedWindowTargetsLocked(nsecs_t currentTime,
        const MotionEntry* entry, std::vector<InputTarget>& inputTargets, nsecs_t* nextWakeupTime,
        bool* outConflictingPointerActions) {
    enum InjectionPermission {
        INJECTION_PERMISSION_UNKNOWN,
        INJECTION_PERMISSION_GRANTED,
        INJECTION_PERMISSION_DENIED
    };

    //获取到当前entry的displayId
    int32_t displayId = entry->displayId;
    //拿到action
    int32_t action = entry->action;
    //标记action
    int32_t maskedAction = action & AMOTION_EVENT_ACTION_MASK;

    //TouchState是一个设备可以接受Motion事件的窗口合集，他有一个窗口队列windows用来保存当前display中所有可以接收Motion事件的窗口,把相关窗口的信息都收集到mTempTouchState种，最后会把结果给inputTargets
    const TouchState* oldState = nullptr;
    ssize_t oldStateIndex = mTouchStatesByDisplay.indexOfKey(displayId);
    if (oldStateIndex >= 0) {//寻找Motion窗口之前 先获取上一次的结果 拷贝到mTempTouchState，因为当我们down的时候遍历了所有的窗口，找到了接收当前输入事件的窗口，那么在后续move 或者up的事件 就不需要再遍历窗口了。
        oldState = &mTouchStatesByDisplay.valueAt(oldStateIndex);
        mTempTouchState.copyFrom(*oldState);
    }

    bool isSplit = mTempTouchState.split;
    bool switchedDevice = mTempTouchState.deviceId >= 0 && mTempTouchState.displayId >= 0
            && (mTempTouchState.deviceId != entry->deviceId
                    || mTempTouchState.source != entry->source
                    || mTempTouchState.displayId != displayId);
    //鼠标相关
    bool isHoverAction = (maskedAction == AMOTION_EVENT_ACTION_HOVER_MOVE
            || maskedAction == AMOTION_EVENT_ACTION_HOVER_ENTER
            || maskedAction == AMOTION_EVENT_ACTION_HOVER_EXIT);
    bool newGesture = (maskedAction == AMOTION_EVENT_ACTION_DOWN
            || maskedAction == AMOTION_EVENT_ACTION_SCROLL
            || isHoverAction);
    bool wrongDevice = false;
    if (newGesture) {//如果是新的事件
        bool down = maskedAction == AMOTION_EVENT_ACTION_DOWN;
        if (switchedDevice && mTempTouchState.down && !down && !isHoverAction) {
            goto Failed;
        }
        //重置mTempTouchState，不使用上一次的结果 并且清除之前的状态
        mTempTouchState.reset();
        mTempTouchState.down = down;
        mTempTouchState.deviceId = entry->deviceId;
        mTempTouchState.source = entry->source;
        mTempTouchState.displayId = displayId;
        isSplit = false;
    } else if (switchedDevice && maskedAction == AMOTION_EVENT_ACTION_MOVE) {
        goto Failed;
    }

    if (newGesture || (isSplit && maskedAction == AMOTION_EVENT_ACTION_POINTER_DOWN)) {//手指按下

        int32_t pointerIndex = getMotionEventActionPointerIndex(action);
        int32_t x = int32_t(entry->pointerCoords[pointerIndex].
                getAxisValue(AMOTION_EVENT_AXIS_X));
        int32_t y = int32_t(entry->pointerCoords[pointerIndex].
                getAxisValue(AMOTION_EVENT_AXIS_Y));
        bool isDown = maskedAction == AMOTION_EVENT_ACTION_DOWN;
        //通过findTouchedWindowAtLocked寻找接收触摸事件的窗口,当前窗口必须可见 以及不能包含NOT_TOUCHABLE 事件的x,y在窗口内（代码分析在下方）
        sp<InputWindowHandle> newTouchedWindowHandle = findTouchedWindowAtLocked(
                displayId, x, y, isDown /*addOutsideTargets*/, true /*addPortalWindows*/);

        std::vector<TouchedMonitor> newGestureMonitors = isDown
                ? findTouchedGestureMonitorsLocked(displayId, mTempTouchState.portalWindows)
                : std::vector<TouchedMonitor>{};

       
        if (newTouchedWindowHandle == nullptr) {
            //没有找到窗口，获取当前第一个窗口
            newTouchedWindowHandle = mTempTouchState.getFirstForegroundWindowHandle();
        }
        //合法性校验
        if (newTouchedWindowHandle == nullptr && newGestureMonitors.empty()) {
            injectionResult = INPUT_EVENT_INJECTION_FAILED;
            goto Failed;
        }
        //找到窗口
        if (newTouchedWindowHandle != nullptr) {
            //设置flag FLAG_FOREGROUND  和 FLAG_DISPATCH_AS_IS 设置为前台窗口并且不会被过滤
            int32_t targetFlags = InputTarget::FLAG_FOREGROUND | InputTarget::FLAG_DISPATCH_AS_IS;
            if (isSplit) {
                targetFlags |= InputTarget::FLAG_SPLIT;
            }
            if (isWindowObscuredAtPointLocked(newTouchedWindowHandle, x, y)) {
                targetFlags |= InputTarget::FLAG_WINDOW_IS_OBSCURED;
            } else if (isWindowObscuredLocked(newTouchedWindowHandle)) {
                targetFlags |= InputTarget::FLAG_WINDOW_IS_PARTIALLY_OBSCURED;
            }

            //更新状态
            if (isHoverAction) {
                newHoverWindowHandle = newTouchedWindowHandle;
            } else if (maskedAction == AMOTION_EVENT_ACTION_SCROLL) {
                newHoverWindowHandle = mLastHoverWindowHandle;
            }

            BitSet32 pointerIds;
            if (isSplit) {
                uint32_t pointerId = entry->pointerProperties[pointerIndex].id;
                pointerIds.markBit(pointerId);
            }
            //把flag添加到 mTempTouchState (创建TouchedWindow  push到当前的windows中)
            mTempTouchState.addOrUpdateWindow(newTouchedWindowHandle, targetFlags, pointerIds);
        }

        mTempTouchState.addGestureMonitors(newGestureMonitors);
    } else {//手指移动
        if (! mTempTouchState.down) {//手指没有按下忽略事件
            goto Failed;
        }

        //检查触摸是否超出屏幕 并且当前是move事件  单点触摸 并且没有设置SLIPPERY这个flag
        if (maskedAction == AMOTION_EVENT_ACTION_MOVE
                && entry->pointerCount == 1
                && mTempTouchState.isSlippery()) {
            int32_t x = int32_t(entry->pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_X));
            int32_t y = int32_t(entry->pointerCoords[0].getAxisValue(AMOTION_EVENT_AXIS_Y));

            sp<InputWindowHandle> oldTouchedWindowHandle =
                    mTempTouchState.getFirstForegroundWindowHandle();
                    //重新寻找一个可以接收当前输入事件的窗口
            sp<InputWindowHandle> newTouchedWindowHandle =
                    findTouchedWindowAtLocked(displayId, x, y);
            //如果找到不同的窗口(其他函数可能会通过addOrUpdateWindow向windows中添加窗口)，移除旧窗口
            if (oldTouchedWindowHandle != newTouchedWindowHandle
                    && oldTouchedWindowHandle != nullptr
                    && newTouchedWindowHandle != nullptr) {
                    //添加FLAG_DISPATCH_AS_SLIPPERY_EXIT 移除旧窗口
                mTempTouchState.addOrUpdateWindow(oldTouchedWindowHandle,
                        InputTarget::FLAG_DISPATCH_AS_SLIPPERY_EXIT, BitSet32(0));

                if (newTouchedWindowHandle->getInfo()->supportsSplitTouch()) {
                    isSplit = true;
                }

                //和之前的flag不同FLAG_DISPATCH_AS_IS->FLAG_DISPATCH_AS_SLIPPERY_ENTER 表示作为新的起点往下传送（因为我们是在move的情况下找的的新窗口，为了能让窗口接收到完整的Motion事件，所以要转换成down）
                int32_t targetFlags = InputTarget::FLAG_FOREGROUND
                        | InputTarget::FLAG_DISPATCH_AS_SLIPPERY_ENTER;
                if (isSplit) {
                    targetFlags |= InputTarget::FLAG_SPLIT;
                }
                if (isWindowObscuredAtPointLocked(newTouchedWindowHandle, x, y)) {
                    targetFlags |= InputTarget::FLAG_WINDOW_IS_OBSCURED;
                }

                BitSet32 pointerIds;
                if (isSplit) {
                    pointerIds.markBit(entry->pointerProperties[0].id);
                }
                //添加新窗口
                mTempTouchState.addOrUpdateWindow(newTouchedWindowHandle, targetFlags, pointerIds);
            }
        }
    }

    if (newHoverWindowHandle != mLastHoverWindowHandle) {
        // 上一个窗口处理
        if (mLastHoverWindowHandle != nullptr) {
            mTempTouchState.addOrUpdateWindow(mLastHoverWindowHandle,
                    InputTarget::FLAG_DISPATCH_AS_HOVER_EXIT, BitSet32(0));
        }

        // 处理新窗口
        if (newHoverWindowHandle != nullptr) {
            mTempTouchState.addOrUpdateWindow(newHoverWindowHandle,
                    InputTarget::FLAG_DISPATCH_AS_HOVER_ENTER, BitSet32(0));
        }
    }
   //安全检查 至少有一个前台窗口或者gesture monitor可以接受输入事件
    {
        bool haveForegroundWindow = false;
        for (const TouchedWindow& touchedWindow : mTempTouchState.windows) {
            if (touchedWindow.targetFlags & InputTarget::FLAG_FOREGROUND) {
                haveForegroundWindow = true;
                if (!checkInjectionPermission(touchedWindow.windowHandle,
                        entry->injectionState)) {
                    goto Failed;
                }
            }
        }
        bool hasGestureMonitor = !mTempTouchState.gestureMonitors.empty();
        if (!haveForegroundWindow && !hasGestureMonitor) {
            injectionResult = INPUT_EVENT_INJECTION_FAILED;
            goto Failed;
        }

        injectionPermission = INJECTION_PERMISSION_GRANTED;
    }

    /....

    for (const TouchedWindow& touchedWindow : mTempTouchState.windows) {
    //把结果传递给inputTargets(代码如下，会包装inputChannel)
        addWindowTargetLocked(touchedWindow.windowHandle, touchedWindow.targetFlags,
                touchedWindow.pointerIds, inputTargets);
    }

    for (const TouchedMonitor& touchedMonitor : mTempTouchState.gestureMonitors) {
        //把结果传递给inputTargets
        addMonitoringTargetLocked(touchedMonitor.monitor, touchedMonitor.xOffset,
                touchedMonitor.yOffset, inputTargets);
    }

    //mTempTouchState 保存了 所有可以接收Motion事件的窗口，所以需要进行一些处理工作(把没有FLAG_DISPATCH_AS_IS 和 FLAG_DISPATCH_AS_SLIPPERY_ENTER这两个flag的移除)
    mTempTouchState.filterNonAsIsTouchWindows();

Failed:
   //....
   
      if (!wrongDevice) {
        //......

//当前非滚动状态会保存操作 把mTempTouchState保存到mTouchStatesByDisplay，下一次进来这个函数的时候 直接获取上一次的结果，不需要遍历所有的窗口了
            if (maskedAction != AMOTION_EVENT_ACTION_SCROLL) {
                if (mTempTouchState.displayId >= 0) {
                    if (oldStateIndex >= 0) {
                        mTouchStatesByDisplay.editValueAt(oldStateIndex).copyFrom(mTempTouchState);
                    } else {
                        mTouchStatesByDisplay.add(displayId, mTempTouchState);
                    }
                } else if (oldStateIndex >= 0) {
                    mTouchStatesByDisplay.removeItemsAt(oldStateIndex);
                }
            }
            mLastHoverWindowHandle = newHoverWindowHandle;
        }

    return injectionResult;
}

void InputDispatcher::TouchState::filterNonAsIsTouchWindows() {
    for (size_t i = 0 ; i < windows.size(); ) {
        TouchedWindow& window = windows[i];
  
        if (window.targetFlags & (InputTarget::FLAG_DISPATCH_AS_IS
                | InputTarget::FLAG_DISPATCH_AS_SLIPPERY_ENTER)) {
            window.targetFlags &= ~InputTarget::FLAG_DISPATCH_MASK;
            window.targetFlags |= InputTarget::FLAG_DISPATCH_AS_IS;
            i += 1;
        } else {
         //把没有FLAG_DISPATCH_AS_IS 和 FLAG_DISPATCH_AS_SLIPPERY_ENTER这两个flag的移除
            windows.erase(windows.begin() + i);
        }
    }
}

   // 从前一直遍历到最后的window 一直找到接收事件的window
sp<InputWindowHandle> InputDispatcher::findTouchedWindowAtLocked(int32_t displayId,
        int32_t x, int32_t y, bool addOutsideTargets, bool addPortalWindows) {

    //根据displayId 获取到所有的window信息 window信息都存储到mWindowHandlesByDisplay中
    const std::vector<sp<InputWindowHandle>> windowHandles = getWindowHandlesLocked(displayId);
    for (const sp<InputWindowHandle>& windowHandle : windowHandles) {
        const InputWindowInfo* windowInfo = windowHandle->getInfo();
        if (windowInfo->displayId == displayId) {
            int32_t flags = windowInfo->layoutParamsFlags;
            if (windowInfo->visible) {//窗口必须是可见的
                if (!(flags & InputWindowInfo::FLAG_NOT_TOUCHABLE)) {//并且不能包含FLAG_NOT_TOUCHABLE 设置了这个window哪怕是可见的 也无法处理touch事件
                //不能同时包含FLAG_NOT_FOCUSABLE 和 FLAG_NOT_TOUCH_MODAL 两个tag 如果窗口没有FLAG_NOT_TOUCH_MODAL 表示该窗口会消费所有坐标事件 无论事件是否在坐标里面 
                    bool isTouchModal = (flags & (InputWindowInfo::FLAG_NOT_FOCUSABLE
                            | InputWindowInfo::FLAG_NOT_TOUCH_MODAL)) == 0;
                    if (isTouchModal || windowInfo->touchableRegionContainsPoint(x, y)) {
                        int32_t portalToDisplayId = windowInfo->portalToDisplayId;
                        if (portalToDisplayId != ADISPLAY_ID_NONE
                                && portalToDisplayId != displayId) {
                            if (addPortalWindows) {
                                mTempTouchState.addPortalWindow(windowHandle);
                            }
                            return findTouchedWindowAtLocked(
                                    portalToDisplayId, x, y, addOutsideTargets, addPortalWindows);
                        }
                        //返回windowHanlde
                        return windowHandle;
                    }
                }

                if (addOutsideTargets && (flags & InputWindowInfo::FLAG_WATCH_OUTSIDE_TOUCH)) {
                    mTempTouchState.addOrUpdateWindow(
                            windowHandle, InputTarget::FLAG_DISPATCH_AS_OUTSIDE, BitSet32(0));
                }
            }
        }
    }
    return nullptr;
}

//根据displayId找到对应的窗口
std::vector<sp<InputWindowHandle>> InputDispatcher::getWindowHandlesLocked(
        int32_t displayId) const {
    std::unordered_map<int32_t, std::vector<sp<InputWindowHandle>>>::const_iterator it =
            mWindowHandlesByDisplay.find(displayId);
    if(it != mWindowHandlesByDisplay.end()) {
    //返回结果
        return it->second;
    }

    //返回空集合
    return std::vector<sp<InputWindowHandle>>();
}



void InputDispatcher::TouchState::addOrUpdateWindow(const sp<InputWindowHandle>& windowHandle,
        int32_t targetFlags, BitSet32 pointerIds) {
    if (targetFlags & InputTarget::FLAG_SPLIT) {
        split = true;
    }

    for (size_t i = 0; i < windows.size(); i++) {
        TouchedWindow& touchedWindow = windows[i];
        if (touchedWindow.windowHandle == windowHandle) {
            touchedWindow.targetFlags |= targetFlags;
            if (targetFlags & InputTarget::FLAG_DISPATCH_AS_SLIPPERY_EXIT) {//移除旧窗口的时候会擦除FLAG_DISPATCH_AS_IS 最后在过滤窗口的时候会过滤掉
                touchedWindow.targetFlags &= ~InputTarget::FLAG_DISPATCH_AS_IS;
            }
            touchedWindow.pointerIds.value |= pointerIds.value;
            return;
        }
    }

    TouchedWindow touchedWindow;
    touchedWindow.windowHandle = windowHandle;
    touchedWindow.targetFlags = targetFlags;
    touchedWindow.pointerIds = pointerIds;
    windows.push_back(touchedWindow);
}



void InputDispatcher::addWindowTargetLocked(const sp<InputWindowHandle>& windowHandle,
        int32_t targetFlags, BitSet32 pointerIds, std::vector<InputTarget>& inputTargets) {
        //根据token获取到inputChannel(服务端)
    sp<InputChannel> inputChannel = getInputChannelLocked(windowHandle->getToken());
    if (inputChannel == nullptr) {
        return;
    }

    const InputWindowInfo* windowInfo = windowHandle->getInfo();
    InputTarget target;
    target.inputChannel = inputChannel;
    target.flags = targetFlags;
    target.xOffset = - windowInfo->frameLeft;
    target.yOffset = - windowInfo->frameTop;
    target.globalScaleFactor = windowInfo->globalScaleFactor;
    target.windowXScale = windowInfo->windowXScale;
    target.windowYScale = windowInfo->windowYScale;
    target.pointerIds = pointerIds;
    inputTargets.push_back(target);
}


//找到窗口了，看看如何分发事件的

void InputDispatcher::dispatchEventLocked(nsecs_t currentTime,
        EventEntry* eventEntry, const std::vector<InputTarget>& inputTargets) {
     //遍历inputTargets
    for (const InputTarget& inputTarget : inputTargets) {
        ssize_t connectionIndex = getConnectionIndexLocked(inputTarget.inputChannel);
        if (connectionIndex >= 0) {
        //获取到Connection
            sp<Connection> connection = mConnectionsByFd.valueAt(connectionIndex);
            //调用prepareDispatchCycleLocked进行分发
            prepareDispatchCycleLocked(currentTime, connection, eventEntry, &inputTarget);
        } else {
        }
    }
}




void InputDispatcher::prepareDispatchCycleLocked(nsecs_t currentTime,
        const sp<Connection>& connection, EventEntry* eventEntry, const InputTarget* inputTarget) {
    if (connection->status != Connection::STATUS_NORMAL) {
        return;
    }

    // 是否需要拆分输入事件
    if (inputTarget->flags & InputTarget::FLAG_SPLIT) {
        MotionEntry* originalMotionEntry = static_cast<MotionEntry*>(eventEntry);
        if (inputTarget->pointerIds.count() != originalMotionEntry->pointerCount) {
            MotionEntry* splitMotionEntry = splitMotionEvent(
                    originalMotionEntry, inputTarget->pointerIds);
            enqueueDispatchEntriesLocked(currentTime, connection,
                    splitMotionEntry, inputTarget);
            splitMotionEntry->release();
            return;
        }
    }
    //调用enqueueDispatchEntriesLocked
    enqueueDispatchEntriesLocked(currentTime, connection, eventEntry, inputTarget);
}



void InputDispatcher::enqueueDispatchEntriesLocked(nsecs_t currentTime,
        const sp<Connection>& connection, EventEntry* eventEntry, const InputTarget* inputTarget) {
      
    bool wasEmpty = connection->outboundQueue.isEmpty();

    // 如果窗口设置了FLAG_DISPATCH_AS_HOVER_EXIT 表示需要处理这种事件，封装成DispatchEntry类型 加入outBoundQueue队列
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

    if (wasEmpty && !connection->outboundQueue.isEmpty()) {
    //这里会执行  开始分发
        startDispatchCycleLocked(currentTime, connection);
    }
}



//开始分发
void InputDispatcher::startDispatchCycleLocked(nsecs_t currentTime,
        const sp<Connection>& connection) {

    while (connection->status == Connection::STATUS_NORMAL
            && !connection->outboundQueue.isEmpty()) {//遍历队列
        DispatchEntry* dispatchEntry = connection->outboundQueue.head;
        dispatchEntry->deliveryTime = currentTime;

        status_t status;
        EventEntry* eventEntry = dispatchEntry->eventEntry;
        switch (eventEntry->type) {
        case EventEntry::TYPE_KEY: {//key事件
            KeyEntry* keyEntry = static_cast<KeyEntry*>(eventEntry);
            status = connection->inputPublisher.publishKeyEvent(dispatchEntry->seq,
                    keyEntry->deviceId, keyEntry->source, keyEntry->displayId,
                    dispatchEntry->resolvedAction, dispatchEntry->resolvedFlags,
                    keyEntry->keyCode, keyEntry->scanCode,
                    keyEntry->metaState, keyEntry->repeatCount, keyEntry->downTime,
                    keyEntry->eventTime);
            break;
        }

        case EventEntry::TYPE_MOTION: {//motion事件 
            MotionEntry* motionEntry = static_cast<MotionEntry*>(eventEntry);
            //publis motion事件 发送给对端 并且执行handleEvent
            status = connection->inputPublisher.publishMotionEvent(dispatchEntry->seq,
                    motionEntry->deviceId, motionEntry->source, motionEntry->displayId,
                    dispatchEntry->resolvedAction, motionEntry->actionButton,
                    dispatchEntry->resolvedFlags, motionEntry->edgeFlags,
                    motionEntry->metaState, motionEntry->buttonState, motionEntry->classification,
                    xOffset, yOffset, motionEntry->xPrecision, motionEntry->yPrecision,
                    motionEntry->downTime, motionEntry->eventTime,
                    motionEntry->pointerCount, motionEntry->pointerProperties,
                    usingCoords);
            break;
        }

        }
        connection->outboundQueue.dequeue(dispatchEntry);
        traceOutboundQueueLength(connection);
        //加入到connection的waitQueue队列 客户端处理完成之后会从waitQueue中移除掉
        connection->waitQueue.enqueueAtTail(dispatchEntry);
        traceWaitQueueLength(connection);
    }
}



status_t InputPublisher::publishMotionEvent(//...) {

    //封装InputMessage
    InputMessage msg;
    //....
    //调用(服务端)inputChannel 的sendMessage 给客户端发送事件 交给客户端处理
    return mChannel->sendMessage(&msg);
}




status_t InputChannel::sendMessage(const InputMessage* msg) {
    const size_t msgLength = msg->size();
    InputMessage cleanMsg;
    msg->getSanitizedCopy(&cleanMsg);
    ssize_t nWrite;
    do {
    //通过socket的send函数把inputMessage 发送出去，那么发送给谁呢？我们看看这个mFd
        nWrite = ::send(mFd, &cleanMsg, msgLength, MSG_DONTWAIT | MSG_NOSIGNAL);
    } while (nWrite == -1 && errno == EINTR);

    return OK;
}


//mFd在构造函数中赋值
InputChannel::InputChannel(const std::string& name, int fd) :
        mName(name) {

    setFd(fd);
}

让我们从native层先暂时回到Java层 看看是什么时候创建的InputChannel,回到ActivityThread的handleResumeActivity


  public void handleResumeActivity(IBinder token, boolean finalStateRequest, boolean isForward,
            String reason) {
        //调用performResumeActivity
        final ActivityClientRecord r = performResumeActivity(token, finalStateRequest, reason);

        final Activity a = r.activity;
            if (a.mVisibleFromClient) {
                if (!a.mWindowAdded) {
                    a.mWindowAdded = true;
                    //wm.addView 调用到WindowManagerImpl的addView  decor就是activity在attach的时候new的PhoneWIndow
                    wm.addView(decor, l);
                } else {
                    a.onWindowAttributesChanged(l);
                }
            }

    }

    private final WindowManagerGlobal mGlobal = WindowManagerGlobal.getInstance();
    
    public void addView(@NonNull View view, @NonNull ViewGroup.LayoutParams params) {
        applyDefaultToken(params);
        mGlobal.addView(view, params, mContext.getDisplay(), mParentWindow);
    }
    
        public void addView(View view, ViewGroup.LayoutParams params,
            Display display, Window parentWindow) {
            //....

            root = new ViewRootImpl(view.getContext(), display);

            try {
            //调用serView
                root.setView(view, wparams, panelParentView);
            } catch (RuntimeException e) {
            }
        }
    }
    //调用setView
    public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
        synchronized (this) {
            if (mView == null) {
                mView = view;
                //创建inputChannel
                mInputChannel = new InputChannel();
                //.... 调用addToDisplay  mWindowSession = WindowManagerGlobal.getWindowSession 会调用windowManager.openSession 也就是WMS的openSession 这个session是用来和wms通信的  mWindow就是ViewRootImpl的W
                res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
                            getHostVisibility(), mDisplay.getDisplayId(), mTmpFrame,
                            mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
                            mAttachInfo.mOutsets, mAttachInfo.mDisplayCutout, mInputChannel,
                            mTempInsets);
              //创建inputEventReceiver  此时的mInputChannel 就是客户端的inputChannel
              mInputEventReceiver = new WindowInputEventReceiver(mInputChannel,  
Looper.myLooper());
            }
        }
    }
    
    //创建inputEventReceiver
    public InputEventReceiver(InputChannel inputChannel, Looper looper) {
        //之前拿到的客户端的inputChannel
        mInputChannel = inputChannel;
        mMessageQueue = looper.getQueue();
        //调用native层的nativeInit
        mReceiverPtr = nativeInit(new WeakReference<InputEventReceiver>(this),
                inputChannel, mMessageQueue);

        mCloseGuard.open("dispose");
    }
    
    
//native层初始化inputEventReceiver
static jlong nativeInit(JNIEnv* env, jclass clazz, jobject receiverWeak,
        jobject inputChannelObj, jobject messageQueueObj) {
        //拿到客户端的inputChannel
    sp<InputChannel> inputChannel = android_view_InputChannel_getInputChannel(env,
            inputChannelObj);
    //获取到messageQueue
    sp<MessageQueue> messageQueue = android_os_MessageQueue_getMessageQueue(env, messageQueueObj);
    //创建receiver
    sp<NativeInputEventReceiver> receiver = new NativeInputEventReceiver(env,
            receiverWeak, inputChannel, messageQueue);
    status_t status = receiver->initialize();
    //强指针+1
    receiver->incStrong(gInputEventReceiverClassInfo.clazz); 
    return reinterpret_cast<jlong>(receiver.get());
}

//客户端设置fdEvents
status_t NativeInputEventReceiver::initialize() {
    setFdEvents(ALOOPER_EVENT_INPUT);
    return OK;
}

//设置fdEvents
void NativeInputEventReceiver::setFdEvents(int events) {
    if (mFdEvents != events) {
        mFdEvents = events;
        //调用服务端的getChannel()->getFd() 拿到客户端的fd 添加到Looper的fd中 也就是ViewRootImpl 就是客户端 当服务端写数据的时候 会调用到这里来
        int fd = mInputConsumer.getChannel()->getFd();
        if (events) {
            mMessageQueue->getLooper()->addFd(fd, 0, events, this, NULL);
        } else {
            mMessageQueue->getLooper()->removeFd(fd);
        }
    }
}


//addFd
int Looper::addFd(int fd, int ident, int events, const sp<LooperCallback>& callback, void* data) {

    { 
        AutoMutex _l(mLock);

        Request request;
        request.fd = fd;
        request.ident = ident;
        request.events = events;
        request.seq = mNextRequestSeq++;
        //设置callback
        request.callback = callback;
        request.data = data;
        if (mNextRequestSeq == -1) mNextRequestSeq = 0;
        struct epoll_event eventItem;
        request.initEventItem(&eventItem);

        ssize_t requestIndex = mRequests.indexOfKey(fd);
        if (requestIndex < 0) {
            int epollResult = epoll_ctl(mEpollFd.get(), EPOLL_CTL_ADD, fd, &eventItem);
            //把request添加到mRequest中去
            mRequests.add(fd, request);
        } else {
            int epollResult = epoll_ctl(mEpollFd.get(), EPOLL_CTL_MOD, fd, &eventItem);
            if (epollResult < 0) {
                if (errno == ENOENT) {
                    epollResult = epoll_ctl(mEpollFd.get(), EPOLL_CTL_ADD, fd, &eventItem);
                    scheduleEpollRebuildLocked();
                } else {
                    return -1;
                }
            }
            mRequests.replaceValueAt(requestIndex, request);
        }
    }
    return 1;
}

//我们看看客户端怎么处理的 我们之前没有讲过Looper所以我们跟一下 了解下过程  之前在分发线程中调用了looper.pollOnce
文件目录:`/system/core/libutils/Looper.cpp`

int Looper::pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
    for (;;) {
        //调用pollInner
        result = pollInner(timeoutMillis);
    }
}



//调用pollInner
int Looper::pollInner(int timeoutMillis) {
    int result = POLL_WAKE;
    struct epoll_event eventItems[EPOLL_MAX_EVENTS];
    //等待唤醒
    int eventCount = epoll_wait(mEpollFd.get(), eventItems, EPOLL_MAX_EVENTS, timeoutMillis);

Done: ;
    for (size_t i = 0; i < mResponses.size(); i++) {
        Response& response = mResponses.editItemAt(i);
        if (response.request.ident == POLL_CALLBACK) {
            int fd = response.request.fd;
            int events = response.events;
            void* data = response.request.data;
            //拿到request的callback调用handleEvent 也就是我们当初传递的this(NativeInputEventReceiver)
            int callbackResult = response.request.callback->handleEvent(fd, events, data);
            if (callbackResult == 0) {
                removeFd(fd, response.request.seq);
            }
            response.request.callback.clear();
            result = POLL_CALLBACK;
        }
    }
    return result;
}

int NativeInputEventReceiver::handleEvent(int receiveFd, int events, void* data) {
    //... 当前的事件 之前注册的事件传递的是ALOOPER_EVENT_INPUT
    if (events & ALOOPER_EVENT_INPUT) {
        JNIEnv* env = AndroidRuntime::getJNIEnv();
        //调用consumeEvents 消费掉事件
        status_t status = consumeEvents(env, false /*consumeBatches*/, -1, NULL);
        mMessageQueue->raiseAndClearException(env, "handleReceiveCallback");
        return status == OK || status == NO_MEMORY ? 1 : 0;
    }
    //...
}

//消费事件
status_t NativeInputEventReceiver::consumeEvents(JNIEnv* env,
        bool consumeBatches, nsecs_t frameTime, bool* outConsumedBatch) {
    //...
    for (;;) {
        uint32_t seq;
        InputEvent* inputEvent;
        //读取客户端inputChannel发送的事件并且转换为inputEvent(看看如何转换的)
        status_t status = mInputConsumer.consume(&mInputEventFactory,
                consumeBatches, frameTime, &seq, &inputEvent);
        if (!skipCallbacks) {
        //获取InputEventReceiver
            if (!receiverObj.get()) {
                receiverObj.reset(jniGetReferent(env, mReceiverWeakGlobal));
            }

            jobject inputEventObj;
            switch (inputEvent->getType()) {
            case AINPUT_EVENT_TYPE_KEY://key事件
                inputEventObj = android_view_KeyEvent_fromNative(env,
                        static_cast<KeyEvent*>(inputEvent));
                break;

            case AINPUT_EVENT_TYPE_MOTION: {//motion事件
                MotionEvent* motionEvent = static_cast<MotionEvent*>(inputEvent);
                if ((motionEvent->getAction() & AMOTION_EVENT_ACTION_MOVE) && outConsumedBatch) {
                    *outConsumedBatch = true;
                }
                inputEventObj = android_view_MotionEvent_obtainAsCopy(env, motionEvent);
                break;
            }
            }

            if (inputEventObj) {
            //调用Java层的dispatchInputEvent方法
                env->CallVoidMethod(receiverObj.get(),
                        gInputEventReceiverClassInfo.dispatchInputEvent, seq, inputEventObj);
                env->DeleteLocalRef(inputEventObj);
            } else {
                skipCallbacks = true;
            }
        }
    }
}
//转换事件为InputEvent
status_t InputConsumer::consume(InputEventFactoryInterface* factory,
        bool consumeBatches, nsecs_t frameTime, uint32_t* outSeq, InputEvent** outEvent) {

    *outSeq = 0;
    *outEvent = nullptr;

    while (!*outEvent) {
              //读取消息到mMsg（看看如何读取的）
       status_t result = mChannel->receiveMessage(&mMsg);

        switch (mMsg.header.type) {
        case InputMessage::TYPE_KEY: {//key事件
            KeyEvent* keyEvent = factory->createKeyEvent();
            initializeKeyEvent(keyEvent, &mMsg);
            *outSeq = mMsg.body.key.seq;
            *outEvent = keyEvent;
            break;
        }

        case InputMessage::TYPE_MOTION: {//motion事件
            ssize_t batchIndex = findBatch(mMsg.body.motion.deviceId, mMsg.body.motion.source);
            MotionEvent* motionEvent = factory->createMotionEvent();
            if (! motionEvent) return NO_MEMORY;
            updateTouchState(mMsg);
            initializeMotionEvent(motionEvent, &mMsg);
            *outSeq = mMsg.body.motion.seq;
            *outEvent = motionEvent;
            break;
        }
        }
    }
    return OK;
}

//读取消息
status_t InputChannel::receiveMessage(InputMessage* msg) {
    ssize_t nRead;
    do {
        nRead = ::recv(mFd, msg, sizeof(InputMessage), MSG_DONTWAIT);
    } while (nRead == -1 && errno == EINTR);
    return OK;
}



//回到Java层（ViewRootImpl） 看看调用
    public void dispatchInputEvent(InputEvent event) {
        dispatchInputEvent(event, null);
    }
    
    public void dispatchInputEvent(InputEvent event, InputEventReceiver receiver) {
        SomeArgs args = SomeArgs.obtain();
        args.arg1 = event;
        args.arg2 = receiver;
        Message msg = mHandler.obtainMessage(MSG_DISPATCH_INPUT_EVENT, args);
        //设置异步消息
        msg.setAsynchronous(true);
        mHandler.sendMessage(msg);
    }


case MSG_DISPATCH_INPUT_EVENT: {
     SomeArgs args = (SomeArgs) msg.obj;
     InputEvent event = (InputEvent) args.arg1;
     InputEventReceiver receiver = (InputEventReceiver) args.arg2;
    //调用enqueueInputEvent
     enqueueInputEvent(event, receiver, 0, true);
     args.recycle();
    } break;
                
    void enqueueInputEvent(InputEvent event,
            InputEventReceiver receiver, int flags, boolean processImmediately) {
            //创建QueuedInputEvent对象 把event入队 mPendingInputEventHead指向队首
        QueuedInputEvent q = obtainQueuedInputEvent(event, receiver, flags);
        if (processImmediately) {//传递的是false
            doProcessInputEvents();
        } else {
        //最终会调用doProcessInputEvents
            scheduleProcessInputEvents();
        }
    }                


    void doProcessInputEvents() {
        while (mPendingInputEventHead != null) {//遍历队列
            QueuedInputEvent q = mPendingInputEventHead;
            //调用deliverInputEvent 分发inputEvent
            deliverInputEvent(q);
        }

    }


    private void deliverInputEvent(QueuedInputEvent q) {
        if (mInputEventConsistencyVerifier != null) {//判断属于同一系列输入事件的一致性，收集每一个检测出的错误，避免相同错误重复收集
            mInputEventConsistencyVerifier.onInputEvent(q.mEvent, 0);
        }
        //获取InputStage(它是用来实现处理输入事件的责任链中的一个阶段的基类)
        InputStage stage;
        if (q.shouldSendToSynthesizer()) {//mflag是0 因此 返回false
            stage = mSyntheticInputStage;
        } else {
        //判断是否是MotionEvent 以及输入源(触摸屏 鼠标 手写笔)返回mFirstPostImeInputStage (EarlyPostImeInputState)否则 mFirstInputStage(NativePreImeInputStage IME处理前阶段)
            stage = q.shouldSkipIme() ? mFirstPostImeInputStage : mFirstInputStage;
        }

        if (stage != null) {
            handleWindowFocusChanged();
            //调用deliver
            stage.deliver(q);
        } else {
            finishInputEvent(q);
        }
    }

        public final void deliver(QueuedInputEvent q) {
        //根据当前mFlags判断 
            if ((q.mFlags & QueuedInputEvent.FLAG_FINISHED) != 0) {//如果标记了调用forward 向下一个分发
                forward(q);
            } else if (shouldDropInputEvent(q)) {//丢弃 添加FLAG_FINISHED标记
                finish(q, false);
            } else {//处理
                apply(q, onProcess(q));
            }
        }
        
        protected int onProcess(QueuedInputEvent q) {
            if (q.mEvent instanceof KeyEvent) {//处理keyEvent
                return processKeyEvent(q);
            } else if (q.mEvent instanceof MotionEvent) {//处理MotionEvent
                return processMotionEvent(q);
            }
            return FORWARD;
        }
   //处理MotionEvent
     private int processMotionEvent(QueuedInputEvent q) {
            final MotionEvent event = (MotionEvent) q.mEvent;

            if (event.isFromSource(InputDevice.SOURCE_CLASS_POINTER)) {
                return processPointerEvent(q);
            }

            return FORWARD;
        }
        
    //处理PointerEvent
     private int processPointerEvent(QueuedInputEvent q) {
         //获取Event
            final MotionEvent event = (MotionEvent)q.mEvent;
            //获取action
            final int action = event.getAction();
            if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_SCROLL) {
                ensureTouchMode(event.isFromSource(InputDevice.SOURCE_TOUCHSCREEN));
            }

            if (action == MotionEvent.ACTION_DOWN && mAttachInfo.mTooltipHost != null) {
            //...交给下一个
            return FORWARD;
        }
//ViewPostImeInputStage的 因为会分发 所以会到这里来
private int processPointerEvent(QueuedInputEvent q) {
            final MotionEvent event = (MotionEvent)q.mEvent;
            //mView就是PhoneWindow(DecorView) 调用到View的dispatchPointerEvent 然后往下分发
            boolean handled = mView.dispatchPointerEvent(event);
            return handled ? FINISH_HANDLED : FORWARD;
        }
        
        
     public final boolean dispatchPointerEvent(MotionEvent event) {
        if (event.isTouchEvent()) {
            return dispatchTouchEvent(event);
        } else {
            return dispatchGenericMotionEvent(event);
        }
    }
    
//DecorView的dispatchTouchEvent
public boolean dispatchTouchEvent(MotionEvent ev) {
//mWindow.setCallback(this); callback在Activity的attach中设置的 设置的是this也就是Activity 调用Activity的dispatchTouchEvent
        final Window.Callback cb = mWindow.getCallback();
        return cb != null && !mWindow.isDestroyed() && mFeatureId < 0
                ? cb.dispatchTouchEvent(ev) : super.dispatchTouchEvent(ev);
    }
        
        //处理
        protected void apply(QueuedInputEvent q, int result) {
            if (result == FORWARD) {
                forward(q);
            } else if (result == FINISH_HANDLED) {
                finish(q, true);
            } else if (result == FINISH_NOT_HANDLED) {
                finish(q, false);
            } else {
                throw new IllegalArgumentException("Invalid result: " + result);
            }
        }

//Activity分发touch
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_DOWN) {//Down的时候触发onUserInteraction
            onUserInteraction();
        }
        if (getWindow().superDispatchTouchEvent(ev)) {//把事件分发给View层
            return true;
        }
        //处理在没有任何View接收的touch 比如dialog意外的区域
        return onTouchEvent(ev);
    }
//PhoneWindow中分发
    public boolean superDispatchTouchEvent(MotionEvent event) {
        return mDecor.superDispatchTouchEvent(event);
    }
//DecorView分发
public boolean superDispatchTouchEvent(MotionEvent event) {
//交给ViewGroup分发 
        return super.dispatchTouchEvent(event);
}
    
    public int addToDisplay(IWindow window, int seq, WindowManager.LayoutParams attrs,
            int viewVisibility, int displayId, Rect outFrame, Rect outContentInsets,
            Rect outStableInsets, Rect outOutsets,
            DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel,
            InsetsState outInsetsState) {
            //调用wms的addWindow 传入的window就是客户端
        return mService.addWindow(this, window, seq, attrs, viewVisibility, displayId, outFrame,
                outContentInsets, outStableInsets, outOutsets, outDisplayCutout, outInputChannel,
                outInsetsState);
    }
    
    
    public int addWindow(Session session, IWindow client, int seq,
            LayoutParams attrs, int viewVisibility, int displayId, Rect outFrame,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel,
            InsetsState outInsetsState) {
            //....
            final boolean openInputChannels = (outInputChannel != null
                    && (attrs.inputFeatures & INPUT_FEATURE_NO_INPUT_CHANNEL) == 0);
            if  (openInputChannels) {
                //调用到WindowState的openInputChannel
                win.openInputChannel(outInputChannel);
            }
            //...
    }

    void openInputChannel(InputChannel outInputChannel) {
        String name = getName();
        //调用InputChannel的openInputChannelPair 创建Native层的serverChannel和clientChannel
        InputChannel[] inputChannels = InputChannel.openInputChannelPair(name);
        mInputChannel = inputChannels[0];
        mClientChannel = inputChannels[1];
        mInputWindowHandle.token = mClient.asBinder();
        if (outInputChannel != null) {
            //调用客户端的transferTo(outInputChannel)  outInputChannel就是客户端
            mClientChannel.transferTo(outInputChannel);
            mClientChannel.dispose();
            mClientChannel = null;
        } else {
            mDeadWindowEventReceiver = new DeadWindowEventReceiver(mClientChannel);
        }
        //注册inputChannel(服务端) 并且获取mClient的Binder 也就是ViewRootImpl的W
        mWmService.mInputManager.registerInputChannel(mInputChannel, mClient.asBinder());
    }
    
    public void registerInputChannel(InputChannel inputChannel, IBinder token) {
        //服务端有客户端的token
        inputChannel.setToken(token);
        //调用native层的registerInputChannel
        nativeRegisterInputChannel(mPtr, inputChannel, Display.INVALID_DISPLAY);
    }
    
    //本地的注册InputChannel
    static void nativeRegisterInputChannel(JNIEnv* env, jclass /* clazz */,
        jlong ptr, jobject inputChannelObj, jint displayId) {
        //得到NativeInputManager
    NativeInputManager* im = reinterpret_cast<NativeInputManager*>(ptr);
    //拿到服务端
    sp<InputChannel> inputChannel = android_view_InputChannel_getInputChannel(env,
            inputChannelObj);
    //调用注册
    status_t status = im->registerInputChannel(env, inputChannel, displayId);
       //设置客户端
    android_view_InputChannel_setDisposeCallback(env, inputChannelObj,
            handleInputChannelDisposed, im);
}

status_t NativeInputManager::registerInputChannel(JNIEnv* /* env */,
        const sp<InputChannel>& inputChannel, int32_t displayId) {
        //拿到InputDispatcher 调用registerInputChannel
    return mInputManager->getDispatcher()->registerInputChannel(
            inputChannel, displayId);
}
//注册InputChannel
status_t InputDispatcher::registerInputChannel(const sp<InputChannel>& inputChannel,
        int32_t displayId) {

    { 
        //创建Connection 用来管理相关的InputChannel(服务端)的分发状态
        sp<Connection> connection = new Connection(inputChannel, false /*monitor*/);
        //获取到服务端fd
        int fd = inputChannel->getFd();
        //把connection 添加到mConnectionsByFd 以inputChannel的fd为key  可以通过fd 获取到connection
        mConnectionsByFd.add(fd, connection);
        //把inputChannel存入mInputChannelsByToken 以token为key  可以通过token 获取到inputChannel  当looper 唤醒之后会回调到handleReceiveCallback  token就是客户端的token,后边可以根据客户端的token获取到Connection
        mInputChannelsByToken[inputChannel->getToken()] = inputChannel;
        //把服务端的fd添加到mLooper中 感兴趣input事件 并且设置回调为handleReceiveCallback 我们看看 WMS在接收到客户端事件之后如何处理的
        mLooper->addFd(fd, 0, ALOOPER_EVENT_INPUT, handleReceiveCallback, this);
    }

    // 唤醒looper
    mLooper->wake();
    return OK;
}

//WMS端收到事件之后的处理
int InputDispatcher::handleReceiveCallback(int fd, int events, void* data) {
    InputDispatcher* d = static_cast<InputDispatcher*>(data);

    {
        std::scoped_lock _l(d->mLock);
        //根据fd 拿到Connection
        ssize_t connectionIndex = d->mConnectionsByFd.indexOfKey(fd);
        bool notify;
        sp<Connection> connection = d->mConnectionsByFd.valueAt(connectionIndex);
        
        if (!(events & (ALOOPER_EVENT_ERROR | ALOOPER_EVENT_HANGUP))) {//走这里
            nsecs_t currentTime = now();
            bool gotOne = false;
            status_t status;
            for (;;) {
                uint32_t seq;
                bool handled;
                //接收窗口处理完成的消息 调用到inputChannel的receiveFinishedSignal 通过channel接收信息 存储到seq和 handled中
                status = connection->inputPublisher.receiveFinishedSignal(&seq, &handled);
                //post一个command 其实就是调用doDispatchCycleFinishedLockedInterruptible 继续执行分发任务  
                d->finishDispatchCycleLocked(currentTime, connection, seq, handled);
                gotOne = true;
            }
            if (gotOne) {
            //执行command
                d->runCommandsLockedInterruptible();
                if (status == WOULD_BLOCK) {
                    return 1;
                }
            }

            notify = status != DEAD_OBJECT || !connection->monitor;
        } else {
            notify = !connection->monitor;
        }

        d->unregisterInputChannelLocked(connection->inputChannel, notify);
        return 0; 
    }
}


void InputDispatcher::finishDispatchCycleLocked(nsecs_t currentTime,
        const sp<Connection>& connection, uint32_t seq, bool handled) {
    connection->inputPublisherBlocked = false;
    //调用onDispatchCycleFinishedLocked
    onDispatchCycleFinishedLocked(currentTime, connection, seq, handled);
}

void InputDispatcher::onDispatchCycleFinishedLocked(
        nsecs_t currentTime, const sp<Connection>& connection, uint32_t seq, bool handled) {
        //post一个Command 并且设置值(添加到mCommandQueue 队列中)
    CommandEntry* commandEntry = postCommandLocked(
            & InputDispatcher::doDispatchCycleFinishedLockedInterruptible);
    commandEntry->connection = connection;
    commandEntry->eventTime = currentTime;
    commandEntry->seq = seq;
    commandEntry->handled = handled;
}



void InputDispatcher::doDispatchCycleFinishedLockedInterruptible(
        CommandEntry* commandEntry) {
    sp<Connection> connection = commandEntry->connection;
    nsecs_t finishTime = commandEntry->eventTime;
    uint32_t seq = commandEntry->seq;
    bool handled = commandEntry->handled;
    //通过connection的等待队列获取到DispatchEntry
    DispatchEntry* dispatchEntry = connection->findWaitQueueEntry(seq);
    if (dispatchEntry) {
      if (dispatchEntry == connection->findWaitQueueEntry(seq)) {
            connection->waitQueue.dequeue(dispatchEntry);//客户端(窗口)处理完成之后,出队列
            if (restartEvent && connection->status == Connection::STATUS_NORMAL) {
                connection->outboundQueue.enqueueAtHead(dispatchEntry);
                traceOutboundQueueLength(connection);
            } else {//释放资源
                releaseDispatchEntry(dispatchEntry);
            }
        }
    //...如果当前connection中的outBoundQueue还有Event开启下一轮的Dispatch
        startDispatchCycleLocked(now(), connection);
    }
}
    
    
    static void android_view_InputChannel_nativeTransferTo(JNIEnv* env, jobject obj,
        jobject otherObj) {

    NativeInputChannel* nativeInputChannel =
            android_view_InputChannel_getNativeInputChannel(env, obj);
    //设置otherObj的mptr为nativeInputChannel
    android_view_InputChannel_setNativeInputChannel(env, otherObj, nativeInputChannel);
    //设置自己的mptr为NULL
    android_view_InputChannel_setNativeInputChannel(env, obj, NULL);
}

    public static InputChannel[] openInputChannelPair(String name) {
        //调用native的函数nativeOpenInputChannelPair
        return nativeOpenInputChannelPair(name);
    }
    
    
//打开inputChannelPair
static jobjectArray android_view_InputChannel_nativeOpenInputChannelPair(JNIEnv* env,
        jclass clazz, jstring nameObj) {
    ScopedUtfChars nameChars(env, nameObj);
    std::string name = nameChars.c_str();
    sp<InputChannel> serverChannel;
    sp<InputChannel> clientChannel;
     //调用InputChannel的openInputChannelPair
    status_t result = InputChannel::openInputChannelPair(name, serverChannel, clientChannel);
        //创建java层数组对象
    jobjectArray channelPair = env->NewObjectArray(2, gInputChannelClassInfo.clazz, NULL);
    //创建java层对象
    jobject serverChannelObj = android_view_InputChannel_createInputChannel(env,
            std::make_unique<NativeInputChannel>(serverChannel));
    if (env->ExceptionCheck()) {
        return NULL;
    }

    jobject clientChannelObj = android_view_InputChannel_createInputChannel(env,
            std::make_unique<NativeInputChannel>(clientChannel));
    if (env->ExceptionCheck()) {
        return NULL;
    }
    //设置数组的值
    env->SetObjectArrayElement(channelPair, 0, serverChannelObj);
    env->SetObjectArrayElement(channelPair, 1, clientChannelObj);
    return channelPair;
}

//打开服务端和客户端
status_t InputChannel::openInputChannelPair(const std::string& name,
        sp<InputChannel>& outServerChannel, sp<InputChannel>& outClientChannel) {
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets)) {
        return result;
    }

    int bufferSize = SOCKET_BUFFER_SIZE;
    setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));
    setsockopt(sockets[0], SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));
    setsockopt(sockets[1], SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));
    setsockopt(sockets[1], SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));

    std::string serverChannelName = name;
    serverChannelName += " (server)";
    outServerChannel = new InputChannel(serverChannelName, sockets[0]);

    std::string clientChannelName = name;
    clientChannelName += " (client)";
    outClientChannel = new InputChannel(clientChannelName, sockets[1]);
    return OK;
}
```

总结:

1.ActivityThread在创建Activity的时候: attach:通过Activity的attach 创建了PhoneWindow和activity进行了绑定并且给mWindow.setCallBack(this)也就是Activity。

handleResumeActivity: 1.服务端:通过WindowManager.addView把当前的decor传递给`ViewRootImpl` 调用setView(addToDisplay)和`WMS`进行通信,在`WMS`的addToDisplay中通过session.addWindow 打开了InputChannel(Native层打开了客户端和服务端)并且给服务端的`IMS`注册了InputChannel为刚才创建的Server端InputChannel(创建Connection 把Connection添加到mConnectionsByFd和mInputChannelsByToken中，并且在mLooper中添加感兴趣的事件为`ALOOPER_EVENT_INPUT`)。

2.客户端:`ViewRootImpl.addView`中创建`WindowInputEventReceiver`,Native层创建NativeInputEventReceiver，设置looper的感兴趣事件为`ALOOPER_EVENT_INPUT`,并且设置回调为this(并且我们分析了Looper在addFd中 会创建Request 并把他添加到mRequests中)。

2.当调用`IMS`的start函数之后,InputDispatcher启动之后会调用pollOnce阻塞等待唤醒，内部调用pollInner 如果被唤醒之后会获取callback也就是我们客户端传递的this 调用`handleEvent`进行处理,调用`consumeEvents`来获取消息过来的输入事件，通过InputChannel的consume把原始事件转换为InputEvent 然后回调到Java层的`dispatchInputEvent`(ViewRootImpl),调用到`processPointerEvent` 最终传递给WindowCallBack 也就是Activity的`dispatchTouchEvent` 通过getWindow().superDispatchTouchEvent(ev) 把事件传递给View层 然后再下发，这个我们后边再讲。

## 总结

## 1.IMS 启动流程图

![InputManagerService的启动.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b3edce6175ce40c6b706384f77b7a157~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 2.Input事件的获取

![输入事件的获取.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/5b1f7288c9c84ec69abf5b69e12e40ca~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 3.Input事件的分发

![输入事件的分发.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/2a11e1f8bf0741f9bfa3dd0f1626aea6~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 4.Activity 和 WMS的一些准备

![Activity和WMS的一些准备工作.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/893828d692d84cb7a644bc315d7a976d~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 5.整体运行图

![整体流程图.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/efcb8bce9119424b978d2b908e2847d8~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

`IMS`的内容非常多，逻辑也不是很复杂，相信通过Demo的代码 和运行图 大家都能了解 他的工作原理了，那么我们下一次在`WMS`的渲染再见啦~

在线视频:

【【AndroidFrameWork】InputManagerService】 [b23.tv/ceNE1Vm](https://link.juejin.cn/?target=https%3A%2F%2Fb23.tv%2FceNE1Vm "https://b23.tv/ceNE1Vm")