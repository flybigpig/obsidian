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

## 介绍

Broadcast是一个很简单的组件，它是我们应用程序之间传输信息的一种机制。BroadcastReceiver是用来接收来自系统和应用的广播 并对其做出相应的组件，我们发送广播是通过Intent，在Intent中我们可以带我们需要传递的数据。广播的注册：1.`静态注册`(AndroidManifest中使用receiver标签) 2.`动态注册` registerReceiver方法。发送广播:`sendBroadcast`

```
//静态注册
<receiver
    android:name=".MyReceiver"//设置Receiver
    android:enabled="true" //是否开启
    android:exported="true" //是否接受本程序以外的广播
    android:process=":broadcast">
    <intent-filter>
        <action android:name="com.example.myapplication.client.MyReceiver" />//接收的action
    </intent-filter>
</receiver>

//动态注册
activity!!.registerReceiver(MyReceiver(), IntentFilter().apply {
    addAction("com.example.myapplication.client.MyReceiver")
    addAction(Telephony.Sms.Intents.SMS_RECEIVED_ACTION)
})


//发送广播
activity!!.sendBroadcast(Intent().apply {
    setPackage(activity!!.packageName)
    setAction("com.example.myapplication.client.MyReceiver")
})



```

广播的分类:

1.`普通广播`:普通广播是发送给系统当前所有注册的接收者，但是接收的顺序是不确定的。

2.`有序广播`:有序广播和普通广播的区别在于可以按照接收者的优先级来决定，默认范围是-1000~1000。通过android:priority来控制。当接收器顺序执行时，可以中止广播。

3.`粘性广播`:粘性广播可以发送给以后注册的接收者

4.`本地广播`:广播发送给和发送者同一进程的接收者

## 正文

## 1.动态注册

注册广播我们是通过`registerReceiver`我们看看`ContextImpl`是怎么注册的

```
    public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter) {
       return registerReceiver(receiver, filter, null, null);
    }

    public Intent registerReceiver(BroadcastReceiver receiver, IntentFilter filter,
            String broadcastPermission, Handler scheduler) {
        return registerReceiverInternal(receiver, getUserId(),
                filter, broadcastPermission, scheduler, getOuterContext(), 0);
    }

//broadcastPermission 为null scheduler 为null
    private Intent registerReceiverInternal(BroadcastReceiver receiver, int userId,
            IntentFilter filter, String broadcastPermission,
            Handler scheduler, Context context, int flags) {
        //看到了rd 其实他和我们之前讲到的sd是一样的
        IIntentReceiver rd = null;
        if (receiver != null) {
            if (mPackageInfo != null && context != null) {
                if (scheduler == null) {
                    scheduler = mMainThread.getHandler();
                }
                //通过LoadedApk获取到rd
                rd = mPackageInfo.getReceiverDispatcher(
                    receiver, context, scheduler,
                    mMainThread.getInstrumentation(), true);
            } else {
                if (scheduler == null) {
                    scheduler = mMainThread.getHandler();
                }
                rd = new LoadedApk.ReceiverDispatcher(
                        receiver, context, scheduler, null, true).getIIntentReceiver();
            }
        }
        try {
            //调用了AMS的registerReceiver具体的细节流程我们就不赘述了 直接到AMS 注意此时自己的binder(nodes)有一个InnerReceiver AMS(refs_by_desc refs_by_node)有两个
            final Intent intent = ActivityManager.getService().registerReceiver(
                    mMainThread.getApplicationThread(), mBasePackageName, rd, filter,
                    broadcastPermission, userId, flags);
            if (intent != null) {
                intent.setExtrasClassLoader(getClassLoader());
                intent.prepareToEnterProcess();
            }
            return intent;
        } catch (RemoteException e) {
        }
    }
    
    
    
       public IIntentReceiver getReceiverDispatcher(BroadcastReceiver r,
            Context context, Handler handler,
            Instrumentation instrumentation, boolean registered) {
        synchronized (mReceivers) {
            LoadedApk.ReceiverDispatcher rd = null;
            ArrayMap<BroadcastReceiver, LoadedApk.ReceiverDispatcher> map = null;
            if (registered) {
                map = mReceivers.get(context);
                if (map != null) {
                    rd = map.get(r);
                }
            }
            if (rd == null) {
                rd = new ReceiverDispatcher(r, context, handler,
                        instrumentation, registered);
                if (registered) {
                    if (map == null) {
                        map = new ArrayMap<BroadcastReceiver, LoadedApk.ReceiverDispatcher>();
                        mReceivers.put(context, map);
                    }
                    map.put(r, rd);
                }
            } else {
                rd.validate(context, handler);
            }
            rd.mForgotten = false;
            return rd.getIIntentReceiver();
        }
    }
    
    
    //它就是我们用来包装的类，会把我们的传进来的BroadcastReceiver
        ReceiverDispatcher(BroadcastReceiver receiver, Context context,
                Handler activityThread, Instrumentation instrumentation,
                boolean registered) {
            mIIntentReceiver = new InnerReceiver(this, !registered);
            mReceiver = receiver;
            mContext = context;
            mActivityThread = activityThread;
            mInstrumentation = instrumentation;
            mRegistered = registered;
            mLocation = new IntentReceiverLeaked(null);
            mLocation.fillInStackTrace();
        }


        //这就是我们的JavaBBinder了
        final static class InnerReceiver extends IIntentReceiver.Stub {
            final WeakReference<LoadedApk.ReceiverDispatcher> mDispatcher;
            final LoadedApk.ReceiverDispatcher mStrongRef;

            InnerReceiver(LoadedApk.ReceiverDispatcher rd, boolean strong) {
                mDispatcher = new WeakReference<LoadedApk.ReceiverDispatcher>(rd);
                mStrongRef = strong ? rd : null;
            }

            @Override
            public void performReceive(Intent intent, int resultCode, String data,
                    Bundle extras, boolean ordered, boolean sticky, int sendingUser) {
                final LoadedApk.ReceiverDispatcher rd;
                if (intent == null) {
                    Log.wtf(TAG, "Null intent received");
                    rd = null;
                } else {
                    rd = mDispatcher.get();
                }
                if (ActivityThread.DEBUG_BROADCAST) {
                    int seq = intent.getIntExtra("seq", -1);
                if (rd != null) {
                    rd.performReceive(intent, resultCode, data, extras,
                            ordered, sticky, sendingUser);
                } else {
                    IActivityManager mgr = ActivityManager.getService();
                    try {
                        if (extras != null) {
                            extras.setAllowFds(false);
                        }
                        mgr.finishReceiver(this, resultCode, data, extras, false, intent.getFlags());
                    } catch (RemoteException e) {
                        throw e.rethrowFromSystemServer();
                    }
                }
            }
        }
```

包装了`InnerReceiver`,调用了AMS的registerReceiver。

```
    public Intent registerReceiver(IApplicationThread caller, String callerPackage,
            IIntentReceiver receiver, IntentFilter filter, String permission, int userId,
            int flags) {
            
           //确认不是隔离进程
        enforceNotIsolatedCaller("registerReceiver");
        ArrayList<Intent> stickyIntents = null;
        ProcessRecord callerApp = null;
        final boolean visibleToInstantApps
                = (flags & Context.RECEIVER_VISIBLE_TO_INSTANT_APPS) != 0;
        int callingUid;
        int callingPid;
        boolean instantApp;
        synchronized(this) {
            if (caller != null) {
                //获取到caller的ProcessRecord
                callerApp = getRecordForAppLocked(caller);
                if (callerApp == null) {
                }
                if (callerApp.info.uid != SYSTEM_UID &&
                        !callerApp.pkgList.containsKey(callerPackage) &&
                        !"android".equals(callerPackage)) {
                }
                callingUid = callerApp.info.uid;
                callingPid = callerApp.pid;
            } else {
                callerPackage = null;
                callingUid = Binder.getCallingUid();
                callingPid = Binder.getCallingPid();
            }

            instantApp = isInstantApp(callerApp, callerPackage, callingUid);
            //获取到uid
            userId = mUserController.handleIncomingUser(callingPid, callingUid, userId, true,
                    ALLOW_FULL_ONLY, "registerReceiver", callerPackage);
            //获取到actions
            Iterator<String> actions = filter.actionsIterator();
            if (actions == null) {
                ArrayList<String> noAction = new ArrayList<String>(1);
                noAction.add(null);
                actions = noAction.iterator();
            }
            int[] userIds = { UserHandle.USER_ALL, UserHandle.getUserId(callingUid) };
            //循环遍历actions
            while (actions.hasNext()) {
                String action = actions.next();
                for (int id : userIds) {
                    //根据action遍历每个userid下的粘性广播 存入stickyIntents
                    ArrayMap<String, ArrayList<Intent>> stickies = mStickyBroadcasts.get(id);
                    if (stickies != null) {
                        //查找符合action的Intent集合
                        ArrayList<Intent> intents = stickies.get(action);
                        if (intents != null) {
                            if (stickyIntents == null) {
                                stickyIntents = new ArrayList<Intent>();
                            }
                            //添加到stickyIntents中
                            stickyIntents.addAll(intents);
                        }
                    }
                }
            }
        }

        ArrayList<Intent> allSticky = null;
        if (stickyIntents != null) {
            final ContentResolver resolver = mContext.getContentResolver();
             //查找所有符合当前action的intent 存入allSticky
            for (int i = 0, N = stickyIntents.size(); i < N; i++) {
                Intent intent = stickyIntents.get(i);
                if (instantApp &&
                        (intent.getFlags() & Intent.FLAG_RECEIVER_VISIBLE_TO_INSTANT_APPS) == 0) {
                    continue;
                }
                if (filter.match(resolver, intent, true, TAG) >= 0) {
                    if (allSticky == null) {
                        allSticky = new ArrayList<Intent>();
                    }
                    allSticky.add(intent);
                }
            }
        }
        Intent sticky = allSticky != null ? allSticky.get(0) : null;
        //如果注册时传入的receiver为null 直接返回sticky(符合action的第一个粘性广播)
        if (receiver == null) {
            return sticky;
        }
        synchronized (this) {
            if (callerApp != null && (callerApp.thread == null
                    || callerApp.thread.asBinder() != caller.asBinder())) {
                return null;
            }
            //看看之前是否注册过receiver 第一次肯定没注册过
            ReceiverList rl = mRegisteredReceivers.get(receiver.asBinder());
            if (rl == null) {
                 //创建ReceiverList
                rl = new ReceiverList(this, callerApp, callingPid, callingUid,
                        userId, receiver);
                 //app就是caller所以不为null
                if (rl.app != null) {
                    final int totalReceiversForApp = rl.app.receivers.size();
                    //最大上限是1000个 不能超过1000个注册者
                    if (totalReceiversForApp >= MAX_RECEIVERS_ALLOWED_PER_APP) {
                    }
                    //添加进来
                    rl.app.receivers.add(rl);
                } else {
                    try {
                        receiver.asBinder().linkToDeath(rl, 0);
                    } catch (RemoteException e) {
                        return sticky;
                    }
                    rl.linkedToDeath = true;
                }
                //存入到mRegisteredReceivers
                mRegisteredReceivers.put(receiver.asBinder(), rl);
            } else if (rl.uid != callingUid) {
            } else if (rl.pid != callingPid) {
            } else if (rl.userId != userId) {
            }
            //创建BroadcastFilter
            BroadcastFilter bf = new BroadcastFilter(filter, rl, callerPackage,
                    permission, callingUid, userId, instantApp, visibleToInstantApps);
            if (rl.containsFilter(filter)) {//已经注册了 不做处理
            } else {
            //没有注册过添加进来 我们的没有添加过
                rl.add(bf);
                if (!bf.debugCheck()) {
                    Slog.w(TAG, "==> For Dynamic broadcast");
                }
                //添加bf
                mReceiverResolver.addFilter(bf);
            }
            //处理粘性广播
            if (allSticky != null) {
                ArrayList receivers = new ArrayList();
                receivers.add(bf);

                final int stickyCount = allSticky.size();
                for (int i = 0; i < stickyCount; i++) {
                    Intent intent = allSticky.get(i);
                    //根据intent决定是前台还是后台广播
                    BroadcastQueue queue = broadcastQueueForIntent(intent);
                    //创建BroadcastRecord
                    BroadcastRecord r = new BroadcastRecord(queue, intent, null,
                            null, -1, -1, false, null, null, OP_NONE, null, receivers,
                            null, 0, null, null, false, true, true, -1, false,
                            false /* only PRE_BOOT_COMPLETED should be exempt, no stickies */);
                   //BroadcastRecord入队列
                    queue.enqueueParallelBroadcastLocked(r);
                    //执行
                    queue.scheduleBroadcastsLocked();
                }
            }
            return sticky;
        }
    }
    //看看是前台还是后台广播
      BroadcastQueue broadcastQueueForIntent(Intent intent) {
        if (isOnOffloadQueue(intent.getFlags())) {
            return mOffloadBroadcastQueue;
        }

        final boolean isFg = (intent.getFlags() & Intent.FLAG_RECEIVER_FOREGROUND) != 0;]
        return (isFg) ? mFgBroadcastQueue : mBgBroadcastQueue;
    }
    
    
    public void scheduleBroadcastsLocked() {
        if (mBroadcastsScheduled) {//默认是false
            return;
        }
        //发送BROADCAST_INTENT_MSG
        mHandler.sendMessage(mHandler.obtainMessage(BROADCAST_INTENT_MSG, this));
        //设置为true
        mBroadcastsScheduled = true;
    }
    
    
    //handler来处理广播
    private final class BroadcastHandler extends Handler {
        public BroadcastHandler(Looper looper) {
            super(looper, null, true);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case BROADCAST_INTENT_MSG: {
                    processNextBroadcast(true);
                } break;
                case BROADCAST_TIMEOUT_MSG: {
                    synchronized (mService) {
                        broadcastTimeoutLocked(true);
                    }
                } break;
            }
        }
    }
    
```

动态注册比较简单，没注册过就创建`ReceiverList` 添加到`MregisteredReceivers`中,将`filter`添加到`mReceiverResolver`。

## 2.广播的发送

```
    public void sendBroadcast(Intent intent) {
        warnIfCallingFromSystemProcess();
         //返回MIME的类型
        String resolvedType = intent.resolveTypeIfNeeded(getContentResolver());
        try {
            intent.prepareToLeaveProcess(this);
            //调用AMS的broadcastIntent
            ActivityManager.getService().broadcastIntent(
                    mMainThread.getApplicationThread(), intent, resolvedType, null,
                    Activity.RESULT_OK, null, null, null, AppOpsManager.OP_NONE, null, false, false,
                    getUserId());
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }
    
    
 到了AMS
 
     public final int broadcastIntent(IApplicationThread caller,
            Intent intent, String resolvedType, IIntentReceiver resultTo,
            int resultCode, String resultData, Bundle resultExtras,
            String[] requiredPermissions, int appOp, Bundle bOptions,
            boolean serialized, boolean sticky, int userId) {
        enforceNotIsolatedCaller("broadcastIntent");
        synchronized(this) {
            //校验intent的合法
            intent = verifyBroadcastLocked(intent);
            //获取到callerApp的ProcessRecord
            final ProcessRecord callerApp = getRecordForAppLocked(caller);
            final int callingPid = Binder.getCallingPid();
            final int callingUid = Binder.getCallingUid();

            final long origId = Binder.clearCallingIdentity();
            try {
                return broadcastIntentLocked(callerApp,
                        callerApp != null ? callerApp.info.packageName : null,
                        intent, resolvedType, resultTo, resultCode, resultData, resultExtras,
                        requiredPermissions, appOp, bOptions, serialized, sticky,
                        callingPid, callingUid, callingUid, callingPid, userId);
            } finally {
                Binder.restoreCallingIdentity(origId);
            }
        }
    }
    
    final int broadcastIntentLocked(ProcessRecord callerApp,
            String callerPackage, Intent intent, String resolvedType,
            IIntentReceiver resultTo, int resultCode, String resultData,
            Bundle resultExtras, String[] requiredPermissions, int appOp, Bundle bOptions,
            boolean ordered, boolean sticky, int callingPid, int callingUid, int realCallingUid,
            int realCallingPid, int userId) {
        return broadcastIntentLocked(callerApp, callerPackage, intent, resolvedType, resultTo,
            resultCode, resultData, resultExtras, requiredPermissions, appOp, bOptions, ordered,
            sticky, callingPid, callingUid, realCallingUid, realCallingPid, userId,
            false /* allowBackgroundActivityStarts */);
    }
    

//发送广播
    final int broadcastIntentLocked(ProcessRecord callerApp,
            String callerPackage, Intent intent, String resolvedType,
            IIntentReceiver resultTo, int resultCode, String resultData,
            Bundle resultExtras, String[] requiredPermissions, int appOp, Bundle bOptions,
            boolean ordered, boolean sticky, int callingPid, int callingUid, int realCallingUid,
            int realCallingPid, int userId, boolean allowBackgroundActivityStarts) {
         //创建intnet
        intent = new Intent(intent);
        //添加flag FLAG_EXCLUDE_STOPPED_PACKAGES
        intent.addFlags(Intent.FLAG_EXCLUDE_STOPPED_PACKAGES);
            //如果ams还没启动 不允许开启新进程
        if (!mProcessesReady && (intent.getFlags()&Intent.FLAG_RECEIVER_BOOT_UPGRADE) == 0) {
            intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY);
        }
        //获取userid
        userId = mUserController.handleIncomingUser(callingPid, callingUid, userId, true,
                ALLOW_NON_FULL, "broadcast", callerPackage);
        //检测发送广播的进程是否还在运行
        if (userId != UserHandle.USER_ALL && !mUserController.isUserOrItsParentRunning(userId)) {
            if ((callingUid != SYSTEM_UID
                    || (intent.getFlags() & Intent.FLAG_RECEIVER_BOOT_UPGRADE) == 0)
                    && !Intent.ACTION_SHUTDOWN.equals(intent.getAction())) {
                return ActivityManager.BROADCAST_FAILED_USER_STOPPED;
            }
        }
        //拿到action
        final String action = intent.getAction();
        BroadcastOptions brOptions = null;
        if (bOptions != null) {//传递的是null
        }
        //广播是否为Protected 也就是受保护的广播只能是系统来发送
        final boolean isProtectedBroadcast;
        try {
            isProtectedBroadcast = AppGlobals.getPackageManager().isProtectedBroadcast(action);
        } catch (RemoteException e) {
            return ActivityManager.BROADCAST_SUCCESS;
        }
        //发送广播的是否是系统应用
        final boolean isCallerSystem;
        switch (UserHandle.getAppId(callingUid)) {
            case ROOT_UID:
            case SYSTEM_UID:
            case PHONE_UID:
            case BLUETOOTH_UID:
            case NFC_UID:
            case SE_UID:
            case NETWORK_STACK_UID:
                isCallerSystem = true;
                break;
            default:
                isCallerSystem = (callerApp != null) && callerApp.isPersistent();
                break;
        }

        if (!isCallerSystem) {//非系统用户
            if (isProtectedBroadcast) {//如果是受保护的广播
                String msg = "Permission Denial: not allowed to send broadcast "
                        + action + " from pid="
                        + callingPid + ", uid=" + callingUid;
                throw new SecurityException(msg);

            } else if (AppWidgetManager.ACTION_APPWIDGET_CONFIGURE.equals(action)
                    || AppWidgetManager.ACTION_APPWIDGET_UPDATE.equals(action)) {
                if (callerPackage == null) {
                    String msg = "Permission Denial: not allowed to send broadcast "
                            + action + " from unknown caller.";
                    throw new SecurityException(msg);
                } else if (intent.getComponent() != null) {
                    if (!intent.getComponent().getPackageName().equals(
                            callerPackage)) {
                        String msg = "Permission Denial: not allowed to send broadcast "
                                + action + " to "
                                + intent.getComponent().getPackageName() + " from "
                                + callerPackage;
                        throw new SecurityException(msg);
                    }
                } else {
                    intent.setPackage(callerPackage);
                }
            }
        }

        boolean timeoutExempt = false;

        if (action != null) {
            //看当前action是否支持隐式广播 SystemConfig.getInstance().getAllowImplicitBroadcasts()
            if (getBackgroundLaunchBroadcasts().contains(action)) {
            //添加flag
                intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
            }
            //根据action 做一些特殊处理
            switch (action) {
                case Intent.ACTION_UID_REMOVED:
                case Intent.ACTION_PACKAGE_REMOVED:
                case Intent.ACTION_PACKAGE_CHANGED:
                case Intent.ACTION_EXTERNAL_APPLICATIONS_UNAVAILABLE:
                case Intent.ACTION_EXTERNAL_APPLICATIONS_AVAILABLE:
                case Intent.ACTION_PACKAGES_SUSPENDED:
                case Intent.ACTION_PACKAGES_UNSUSPENDED:
                    //如果是关于删除 或者应用修改 移除uid task等
                    if (checkComponentPermission(
                            android.Manifest.permission.BROADCAST_PACKAGE_REMOVED,
                            callingPid, callingUid, -1, true)
                            != PackageManager.PERMISSION_GRANTED) {//检查权限
                        String msg = "Permission Denial: " + intent.getAction()
                                + " broadcast from " + callerPackage + " (pid=" + callingPid
                                + ", uid=" + callingUid + ")"
                                + " requires "
                                + android.Manifest.permission.BROADCAST_PACKAGE_REMOVED;
                        throw new SecurityException(msg);
                    }
                    break;
                case Intent.ACTION_PACKAGE_REPLACED://覆盖安装
                {
                    final Uri data = intent.getData();
                    final String ssp;
                    if (data != null && (ssp = data.getSchemeSpecificPart()) != null) {
                        ApplicationInfo aInfo = null;
                        try {
                            aInfo = AppGlobals.getPackageManager()
                                    .getApplicationInfo(ssp, STOCK_PM_FLAGS, userId);
                        } catch (RemoteException ignore) {}
                        if (aInfo == null) {
                            return ActivityManager.BROADCAST_SUCCESS;
                        }
                        updateAssociationForApp(aInfo);
                        mAtmInternal.onPackageReplaced(aInfo);
                        mServices.updateServiceApplicationInfoLocked(aInfo);
                        sendPackageBroadcastLocked(ApplicationThreadConstants.PACKAGE_REPLACED,
                                new String[] {ssp}, userId);
                    }
                    break;
                }
                case Intent.ACTION_PACKAGE_ADDED://新增应用
                {
                    Uri data = intent.getData();
                    String ssp;
                    if (data != null && (ssp = data.getSchemeSpecificPart()) != null) {
                        final boolean replacing =
                                intent.getBooleanExtra(Intent.EXTRA_REPLACING, false);
                        mAtmInternal.onPackageAdded(ssp, replacing);

                        try {
                            ApplicationInfo ai = AppGlobals.getPackageManager().
                                    getApplicationInfo(ssp, STOCK_PM_FLAGS, 0);
                            mBatteryStatsService.notePackageInstalled(ssp,
                                    ai != null ? ai.longVersionCode : 0);
                        } catch (RemoteException e) {
                        }
                    }
                    break;
                }
                case Intent.ACTION_PACKAGE_DATA_CLEARED://清空数据
                {
                    Uri data = intent.getData();
                    String ssp;
                    if (data != null && (ssp = data.getSchemeSpecificPart()) != null) {
                        mAtmInternal.onPackageDataCleared(ssp);
                    }
                    break;
                }
                case Intent.ACTION_TIMEZONE_CHANGED://时区修改
                    mHandler.sendEmptyMessage(UPDATE_TIME_ZONE);
                    break;
                case Intent.ACTION_TIME_CHANGED://修改时间
                    final int NO_EXTRA_VALUE_FOUND = -1;
                    final int timeFormatPreferenceMsgValue = intent.getIntExtra(
                            Intent.EXTRA_TIME_PREF_24_HOUR_FORMAT,
                            NO_EXTRA_VALUE_FOUND /* defaultValue */);
                    if (timeFormatPreferenceMsgValue != NO_EXTRA_VALUE_FOUND) {
                        Message updateTimePreferenceMsg =
                                mHandler.obtainMessage(UPDATE_TIME_PREFERENCE_MSG,
                                        timeFormatPreferenceMsgValue, 0);
                        mHandler.sendMessage(updateTimePreferenceMsg);
                    }
                    BatteryStatsImpl stats = mBatteryStatsService.getActiveStatistics();
                    synchronized (stats) {
                        stats.noteCurrentTimeChangedLocked();
                    }
                    break;
                case Intent.ACTION_CLEAR_DNS_CACHE://清空dns缓存
                    mHandler.sendEmptyMessage(CLEAR_DNS_CACHE_MSG);
                    break;
                case Proxy.PROXY_CHANGE_ACTION://代理修改
                    mHandler.sendMessage(mHandler.obtainMessage(UPDATE_HTTP_PROXY_MSG));
                    break;
                case android.hardware.Camera.ACTION_NEW_PICTURE://相机拍照
                case android.hardware.Camera.ACTION_NEW_VIDEO://相机录视频
                    intent.addFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY);
                    break;
                case android.security.KeyChain.ACTION_TRUST_STORE_CHANGED:
                    mHandler.sendEmptyMessage(HANDLE_TRUST_STORAGE_UPDATE_MSG);
                    break;
                case "com.android.launcher.action.INSTALL_SHORTCUT":
                    return ActivityManager.BROADCAST_SUCCESS;
                case Intent.ACTION_PRE_BOOT_COMPLETED:
                    timeoutExempt = true;
                    break;
            }

            if (Intent.ACTION_PACKAGE_ADDED.equals(action) ||
                    Intent.ACTION_PACKAGE_REMOVED.equals(action) ||
                    Intent.ACTION_PACKAGE_REPLACED.equals(action)) {
                final int uid = getUidFromIntent(intent);
                if (uid != -1) {
                    final UidRecord uidRec = mProcessList.getUidRecordLocked(uid);
                    if (uidRec != null) {
                        uidRec.updateHasInternetPermission();
                    }
                }
            }
        }

        if (sticky) {//如果是粘性广播
          //权限检查
            if (checkPermission(android.Manifest.permission.BROADCAST_STICKY,
                    callingPid, callingUid)
                    != PackageManager.PERMISSION_GRANTED) {
            }
            if (requiredPermissions != null && requiredPermissions.length > 0) {
            //发送粘性广播不能指定权限
                return ActivityManager.BROADCAST_STICKY_CANT_HAVE_PERMISSION;
            }
            //粘性广播不能指定Component
            if (intent.getComponent() != null) {
                throw new SecurityException(
                        "Sticky broadcasts can't target a specific component");
            }
            //如果广播不是发送给所有用户，检查是否存在一个发送给所有用户的相同广播
            if (userId != UserHandle.USER_ALL) {
                ArrayMap<String, ArrayList<Intent>> stickies = mStickyBroadcasts.get(
                        UserHandle.USER_ALL);
                if (stickies != null) {
                    ArrayList<Intent> list = stickies.get(intent.getAction());
                    if (list != null) {
                        int N = list.size();
                        int i;
                        for (i=0; i<N; i++) {
                            if (intent.filterEquals(list.get(i))) {
                                throw new IllegalArgumentException(
                                        "Sticky broadcast " + intent + " for user "
                                        + userId + " conflicts with existing global broadcast");
                            }
                        }
                    }
                }
            }
            //根据userId获取到所有的粘性广播 保存到mStickyBroadcasts
            ArrayMap<String, ArrayList<Intent>> stickies = mStickyBroadcasts.get(userId);
            if (stickies == null) {
                stickies = new ArrayMap<>();
                mStickyBroadcasts.put(userId, stickies);
            }
            //获取到符合action的Intent 存入stickies
            ArrayList<Intent> list = stickies.get(intent.getAction());
            if (list == null) {
                list = new ArrayList<>();
                stickies.put(intent.getAction(), list);
            }
            final int stickiesCount = list.size();
            int i;
            //遍历list 如果已经存在 覆盖
            for (i = 0; i < stickiesCount; i++) {
                if (intent.filterEquals(list.get(i))) {
                    list.set(i, new Intent(intent));
                    break;
                }
            }
            if (i >= stickiesCount) {
                list.add(new Intent(intent));
            }
        }

        int[] users;
        if (userId == UserHandle.USER_ALL) {
            //广播给所有用户
            users = mUserController.getStartedUserArray();
        } else {
        //广播给特定用户
            users = new int[] {userId};
        }

    //所有接收Intent的receiver
        List receivers = null;
        List<BroadcastFilter> registeredReceivers = null;
        if ((intent.getFlags()&Intent.FLAG_RECEIVER_REGISTERED_ONLY)
                 == 0) {
                //如果没有设置FLAG_RECEIVER_REGISTERED_ONLY 收集静态注册的Receiver(从pkms中查询AppGlobals.getPackageManager()  
.queryIntentReceivers)
            receivers = collectReceiverComponents(intent, resolvedType, callingUid, users);
        }
        if (intent.getComponent() == null) {
            //所有用户并且调用者是SHELL_UID
            if (userId == UserHandle.USER_ALL && callingUid == SHELL_UID) {
                for (int i = 0; i < users.length; i++) {
                    if (mUserController.hasUserRestriction(
                            UserManager.DISALLOW_DEBUGGING_FEATURES, users[i])) {
                        continue;
                    }
                    //之前动态注册的放入了mReceiverResolver
                    List<BroadcastFilter> registeredReceiversForUser =
                            mReceiverResolver.queryIntent(intent,
                                    resolvedType, false /*defaultOnly*/, users[i]);
                    if (registeredReceivers == null) {
                        registeredReceivers = registeredReceiversForUser;
                    } else if (registeredReceiversForUser != null) {
                        registeredReceivers.addAll(registeredReceiversForUser);
                    }
                }
            } else {
                registeredReceivers = mReceiverResolver.queryIntent(intent,
                        resolvedType, false /*defaultOnly*/, userId);
            }
        }

        final boolean replacePending =
                (intent.getFlags()&Intent.FLAG_RECEIVER_REPLACE_PENDING) != 0;
        int NR = registeredReceivers != null ? registeredReceivers.size() : 0;
        if (!ordered && NR > 0) {
        //不是有序广播 并且有接收者 有序不会先发送动态的
            if (isCallerSystem) {
                checkBroadcastFromSystem(intent, callerApp, callerPackage, callingUid,
                        isProtectedBroadcast, registeredReceivers);
            }
            //根据intent获取到队列
            final BroadcastQueue queue = broadcastQueueForIntent(intent);
            //创建BroadcastRecord
            BroadcastRecord r = new BroadcastRecord(queue, intent, callerApp,
                    callerPackage, callingPid, callingUid, callerInstantApp, resolvedType,
                    requiredPermissions, appOp, brOptions, registeredReceivers, resultTo,
                    resultCode, resultData, resultExtras, ordered, sticky, false, userId,
                    allowBackgroundActivityStarts, timeoutExempt);
            final boolean replaced = replacePending
                    && (queue.replaceParallelBroadcastLocked(r) != null);
            if (!replaced) {
                //加入队列
                queue.enqueueParallelBroadcastLocked(r);
                //执行发送广播(动态注册的)
                queue.scheduleBroadcastsLocked();
            }
            registeredReceivers = null;
            NR = 0;
        }

        int ir = 0;
        if (receivers != null) {//处理静态注册的
            int NT = receivers != null ? receivers.size() : 0;
            int it = 0;
            ResolveInfo curt = null;
            BroadcastFilter curr = null;
            while (it < NT && ir < NR) {
                if (curt == null) {
                //拿到静态接收者
                    curt = (ResolveInfo)receivers.get(it);
                }
                if (curr == null) {
                //获取动态接收者
                    curr = registeredReceivers.get(ir);
                }
                if (curr.getPriority() >= curt.priority) {
                    //同优先级下，动态接收者会被插入到静态接收者前边
                    receivers.add(it, curr);
                    ir++;
                    curr = null;
                    it++;
                    NT++;
                } else {
                    it++;
                    curt = null;
                }
            }
        }
        //剩余的动态接收者存入receivers
        while (ir < NR) {
            if (receivers == null) {
                receivers = new ArrayList();
            }
            receivers.add(registeredReceivers.get(ir));
            ir++;
        }
        
        if ((receivers != null && receivers.size() > 0)
                || resultTo != null) {
                //获取到执行队列
            BroadcastQueue queue = broadcastQueueForIntent(intent);
            //创建BroadcastRecord
            BroadcastRecord r = new BroadcastRecord(queue, intent, callerApp,
                    callerPackage, callingPid, callingUid, callerInstantApp, resolvedType,
                    requiredPermissions, appOp, brOptions, receivers, resultTo, resultCode,
                    resultData, resultExtras, ordered, sticky, false, userId,
                    allowBackgroundActivityStarts, timeoutExempt);
            final BroadcastRecord oldRecord =
                    replacePending ? queue.replaceOrderedBroadcastLocked(r) : null;
            if (oldRecord != null) {
            //如果存在旧广播调用performReceiveLocked
                if (oldRecord.resultTo != null) {
                    final BroadcastQueue oldQueue = broadcastQueueForIntent(oldRecord.intent);
                    try {
                        oldQueue.performReceiveLocked(oldRecord.callerApp, oldRecord.resultTo,
                                oldRecord.intent,
                                Activity.RESULT_CANCELED, null, null,
                                false, false, oldRecord.userId);
                    } catch (RemoteException e) {
                    }
                }
            } else {
            //入队
                queue.enqueueOrderedBroadcastLocked(r);
            //执行广播
                queue.scheduleBroadcastsLocked();
            }
        } else {
            if (intent.getComponent() == null && intent.getPackage() == null
                    && (intent.getFlags()&Intent.FLAG_RECEIVER_REGISTERED_ONLY) == 0) {
                addBroadcastStatLocked(intent.getAction(), callerPackage, 0, 0, 0);
            }
        }
        return ActivityManager.BROADCAST_SUCCESS;
    }

```

首先会检查发送的广播是否是一些特殊广播，尤其是应用变化相关的广播，接下来判断是否是粘性广播 如果是粘性广播会检查一些权限（必须指定Component）根据userid从mStickyBroadcasts找到 并添加intent。然后接着查找 广播的接收者(静态通过pkms来找，动态就从mReceiverResolver来找)。 然后进行排序 如果是普通广播优先发给动态接收者，如果是有序广播会根据优先级排序(如果相同动态会比静态先接收) 创建`BroadcastRecord`存入队列，通过`scheduleBroadcastLocked`发送广播。 看看怎么处理的

```

    public void scheduleBroadcastsLocked() {
        if (mBroadcastsScheduled) {
            return;
        }
        //发送BROADCAST_INTENT_MSG 
        mHandler.sendMessage(mHandler.obtainMessage(BROADCAST_INTENT_MSG, this));
        mBroadcastsScheduled = true;
    }
    
    private final class BroadcastHandler extends Handler {
        public BroadcastHandler(Looper looper) {
            super(looper, null, true);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case BROADCAST_INTENT_MSG: {
                    processNextBroadcast(true);
                } break;
                case BROADCAST_TIMEOUT_MSG: {
                    synchronized (mService) {
                        broadcastTimeoutLocked(true);
                    }
                } break;
            }
        }
    }
    
    final void processNextBroadcast(boolean fromMsg) {
        synchronized (mService) {
            processNextBroadcastLocked(fromMsg, false);
        }
    }
    
    //处理广播
    final void processNextBroadcastLocked(boolean fromMsg, boolean skipOomAdj) {
        BroadcastRecord r;
        mService.updateCpuStats();
        if (fromMsg) {//当前是true  设置mBroadcastsScheduled 为false
            mBroadcastsScheduled = false;
        }
        //处理动态广播
         while (mParallelBroadcasts.size() > 0) {
            r = mParallelBroadcasts.remove(0);
            r.dispatchTime = SystemClock.uptimeMillis();
            r.dispatchClockTime = System.currentTimeMillis();
            final int N = r.receivers.size();
            for (int i=0; i<N; i++) {
                Object target = r.receivers.get(i);
                //执行动态广播
                deliverToRegisteredReceiverLocked(r, (BroadcastFilter)target, false, i);
            }
            //添加到记录
            addBroadcastToHistoryLocked(r);
        }
       
        //mPendingBroadcast存放的是等待进程启动的广播
        if (mPendingBroadcast != null) {
            boolean isDead;
            if (mPendingBroadcast.curApp.pid > 0) {
                synchronized (mService.mPidsSelfLocked) {
                    ProcessRecord proc = mService.mPidsSelfLocked.get(
                            mPendingBroadcast.curApp.pid);
                    isDead = proc == null || proc.isCrashing();
                }
            } else {
                final ProcessRecord proc = mService.mProcessList.mProcessNames.get(
                        mPendingBroadcast.curApp.processName, mPendingBroadcast.curApp.uid);
                isDead = proc == null || !proc.pendingStart;
            }
            if (!isDead) {
                return;
            } else {
                mPendingBroadcast.state = BroadcastRecord.IDLE;
                mPendingBroadcast.nextReceiver = mPendingBroadcastRecvIndex;
                mPendingBroadcast = null;
            }
        }

        boolean looped = false;
        //处理有序广播
        do {
            final long now = SystemClock.uptimeMillis();
            //
            r = mDispatcher.getNextBroadcastLocked(now);

            if (r == null) {//都处理完了
                mDispatcher.scheduleDeferralCheckLocked(false);
                mService.scheduleAppGcsLocked();
                if (looped) {
                    mService.updateOomAdjLocked(OomAdjuster.OOM_ADJ_REASON_START_RECEIVER);
                }

                if (mService.mUserController.mBootCompleted && mLogLatencyMetrics) {
                    mLogLatencyMetrics = false;
                }

                return;
            }

            boolean forceReceive = false;
            int numReceivers = (r.receivers != null) ? r.receivers.size() : 0;
            if (mService.mProcessesReady && !r.timeoutExempt && r.dispatchTime > 0) {//超时
            long now = SystemClock.uptimeMillis();
                if ((numReceivers > 0) &&
                        (now > r.dispatchTime + (2 * mConstants.TIMEOUT * numReceivers))) {
                       //中止广播
                    broadcastTimeoutLocked(false); // forcibly finish this broadcast
                    forceReceive = true;
                    r.state = BroadcastRecord.IDLE;
                }
            }

            if (r.state != BroadcastRecord.IDLE) {
                return;
            }

            if (r.receivers == null || r.nextReceiver >= numReceivers
                    || r.resultAbort || forceReceive) {
                if (r.resultTo != null) {
                    boolean sendResult = true;
                    if (r.splitToken != 0) {
                        int newCount = mSplitRefcounts.get(r.splitToken) - 1;
                        if (newCount == 0) {
                            mSplitRefcounts.delete(r.splitToken);
                        } else {
                            sendResult = false;
                            mSplitRefcounts.put(r.splitToken, newCount);
                        }
                    }
                    if (sendResult) {
                        try {
                        //把广播结果传递给发送广播的进程
                            performReceiveLocked(r.callerApp, r.resultTo,
                                    new Intent(r.intent), r.resultCode,
                                    r.resultData, r.resultExtras, false, false, r.userId);
                            r.resultTo = null;
                        } catch (RemoteException e) {
                            r.resultTo = null;
                        }
                    }
                }
                //取消广播超时
                cancelBroadcastTimeoutLocked();
                //添加记录
                addBroadcastToHistoryLocked(r);
                if (r.intent.getComponent() == null && r.intent.getPackage() == null
                        && (r.intent.getFlags()&Intent.FLAG_RECEIVER_REGISTERED_ONLY) == 0) {
                    mService.addBroadcastStatLocked(r.intent.getAction(), r.callerPackage,
                            r.manifestCount, r.manifestSkipCount, r.finishTime-r.dispatchTime);
                }
                mDispatcher.retireBroadcastLocked(r);
                r = null;
                looped = true;
                continue;
            }
        } while (r == null);
            
        int recIdx = r.nextReceiver++;
        //记录时间
        r.receiverTime = SystemClock.uptimeMillis();

        final BroadcastOptions brOptions = r.options;
        final Object nextReceiver = r.receivers.get(recIdx);

        if (nextReceiver instanceof BroadcastFilter) {
            BroadcastFilter filter = (BroadcastFilter)nextReceiver;
            //处理广播
            deliverToRegisteredReceiverLocked(r, filter, r.ordered, recIdx);
            if (r.receiver == null || !r.ordered) {
                r.state = BroadcastRecord.IDLE;
                //处理下一个广播
                scheduleBroadcastsLocked();
            } else {
            }
            return;
        }
        
        ResolveInfo info =
            (ResolveInfo)nextReceiver;
        ComponentName component = new ComponentName(
                info.activityInfo.applicationInfo.packageName,
                info.activityInfo.name);
        
        //拿到目标进程
        ProcessRecord app = mService.getProcessRecordLocked(targetProcess,
                info.activityInfo.applicationInfo.uid, false);

        //静态广播 如果进程存在
        if (app != null && app.thread != null && !app.killed) {
            try {
                app.addPackage(info.activityInfo.packageName,
                        info.activityInfo.applicationInfo.longVersionCode, mService.mProcessStats);
                maybeAddAllowBackgroundActivityStartsToken(app, r);
                //调用 processCurBroadcastLocked 调用app.thread.scheduleReceiver  执行 onReceive
                processCurBroadcastLocked(r, app, skipOomAdj); 
                //执行完成return
                return;
            } catch (RemoteException e) {
            } catch (RuntimeException e) {
            }
        }
    //没有开启进程 就开启进程
        if ((r.curApp=mService.startProcessLocked(targetProcess,
                info.activityInfo.applicationInfo, true,
                r.intent.getFlags() | Intent.FLAG_FROM_BACKGROUND,
                new HostingRecord("broadcast", r.curComponent),
                (r.intent.getFlags()&Intent.FLAG_RECEIVER_BOOT_UPGRADE) != 0, false, false))
                        == null) {
            logBroadcastReceiverDiscardLocked(r);
            finishReceiverLocked(r, r.resultCode, r.resultData,
                    r.resultExtras, r.resultAbort, false);
             //开启失败执行下一个广播
            scheduleBroadcastsLocked();
            r.state = BroadcastRecord.IDLE;
            return;
        }

        maybeAddAllowBackgroundActivityStartsToken(r.curApp, r);
        mPendingBroadcast = r; //执行后边的广播时 先看mPendingBroadcast
        mPendingBroadcastRecvIndex = recIdx;
    }



//处理注册的广播
    private void deliverToRegisteredReceiverLocked(BroadcastRecord r,
            BroadcastFilter filter, boolean ordered, int index) {
      
        try {
            if (filter.receiverList.app != null && filter.receiverList.app.inFullBackup) {
                if (ordered) {
                    skipReceiverLocked(r);
                }
            } else {
                //设置时间戳
                r.receiverTime = SystemClock.uptimeMillis();
                maybeAddAllowBackgroundActivityStartsToken(filter.receiverList.app, r);
                //调用performReceiveLocked 传入filter.receiverList.receiver 也就是我们封装的InnerReceiver
                performReceiveLocked(filter.receiverList.app, filter.receiverList.receiver,
                        new Intent(r.intent), r.resultCode, r.resultData,
                        r.resultExtras, r.ordered, r.initialSticky, r.userId);
                if (r.allowBackgroundActivityStarts && !r.ordered) {
                    postActivityStartTokenRemoval(filter.receiverList.app, r);
                }
            }
            if (ordered) {
                r.state = BroadcastRecord.CALL_DONE_RECEIVE;
            }
        } catch (RemoteException e) {
        }
    }

    void performReceiveLocked(ProcessRecord app, IIntentReceiver receiver,
            Intent intent, int resultCode, String data, Bundle extras,
            boolean ordered, boolean sticky, int sendingUser)
            throws RemoteException {
        if (app != null) {
            if (app.thread != null) {
                try {
                    //调用app的scheduleRegisteredReceiver
                    app.thread.scheduleRegisteredReceiver(receiver, intent, resultCode,
                            data, extras, ordered, sticky, sendingUser, app.getReportedProcState());
                } catch (RemoteException ex) {
                }
            } else {
            }
        } else {
            receiver.performReceive(intent, resultCode, data, extras, ordered,
                    sticky, sendingUser);
        }
    }


        public void scheduleRegisteredReceiver(IIntentReceiver receiver, Intent intent,
                int resultCode, String dataStr, Bundle extras, boolean ordered,
                boolean sticky, int sendingUser, int processState) throws RemoteException {
            updateProcessState(processState, false);
            //调用receiver 也就是我们之前传递的InnerReceiver
            receiver.performReceive(intent, resultCode, dataStr, extras, ordered,
                    sticky, sendingUser);
        }
        //App端调用receive
            public void performReceive(Intent intent, int resultCode, String data,
                    Bundle extras, boolean ordered, boolean sticky, int sendingUser) {
                final LoadedApk.ReceiverDispatcher rd;
                if (intent == null) {
                    Log.wtf(TAG, "Null intent received");
                    rd = null;
                } else {
                    rd = mDispatcher.get();
                }
                if (rd != null) {
                //调用performReceive
                    rd.performReceive(intent, resultCode, data, extras,
                            ordered, sticky, sendingUser);
                } else {
                    IActivityManager mgr = ActivityManager.getService();
                    try {
                        if (extras != null) {
                            extras.setAllowFds(false);
                        }
                        //告诉AMS finishReceiver
                        mgr.finishReceiver(this, resultCode, data, extras, false, intent.getFlags());
                    } catch (RemoteException e) {
                    }
                }
            }        
        
       public void performReceive(Intent intent, int resultCode, String data,
                Bundle extras, boolean ordered, boolean sticky, int sendingUser) {
            final Args args = new Args(intent, resultCode, data, extras, ordered,
                    sticky, sendingUser);
                    //调用了args.getRunnable
            if (intent == null || !mActivityThread.post(args.getRunnable())) {
                if (mRegistered && ordered) {
                    IActivityManager mgr = ActivityManager.getService();
                    args.sendFinished(mgr);
                }
            }
        }   
        
        
        
            public final Runnable getRunnable() {
                return () -> {
                    final BroadcastReceiver receiver = mReceiver;
                    final boolean ordered = mOrdered;

                    final IActivityManager mgr = ActivityManager.getService();
                    final Intent intent = mCurIntent;

                    mCurIntent = null;
                    mDispatched = true;
                    mRunCalled = true;
                    if (receiver == null || intent == null || mForgotten) {
                        if (mRegistered && ordered) {
                            sendFinished(mgr);
                        }
                        return;
                    }

                    try {
                        ClassLoader cl = mReceiver.getClass().getClassLoader();
                        intent.setExtrasClassLoader(cl);
                        intent.prepareToEnterProcess();
                        setExtrasClassLoader(cl);
                        receiver.setPendingResult(this);
                        //调用receiver的onReceive 也就是调用到我们注册的接收者中了
                        receiver.onReceive(mContext, intent);
                    } catch (Exception e) {
                    }

                    if (receiver.getPendingResult() != null) {//pendingResult不是null 执行finish
                        finish();
                    }
                };
            }
            
        public final void scheduleReceiver(Intent intent, ActivityInfo info,
                CompatibilityInfo compatInfo, int resultCode, String data, Bundle extras,
                boolean sync, int sendingUser, int processState) {
            updateProcessState(processState, false);
            ReceiverData r = new ReceiverData(intent, resultCode, data, extras,
                    sync, false, mAppThread.asBinder(), sendingUser);
            r.info = info;
            r.compatInfo = compatInfo;
            sendMessage(H.RECEIVER, r);
        }


    private void handleReceiver(ReceiverData data) {
        unscheduleGcIdler();
        String component = data.intent.getComponent().getClassName();
        //获取到LoadedApk
        LoadedApk packageInfo = getPackageInfoNoCheck(
                data.info.applicationInfo, data.compatInfo);
        //获取到AMS
        IActivityManager mgr = ActivityManager.getService();

        Application app;
        BroadcastReceiver receiver;
        ContextImpl context;
        try {
        //拿到Application
            app = packageInfo.makeApplication(false, mInstrumentation);
            context = (ContextImpl) app.getBaseContext();
            if (data.info.splitName != null) {
                context = (ContextImpl) context.createContextForSplit(data.info.splitName);
            }
            java.lang.ClassLoader cl = context.getClassLoader();
            data.intent.setExtrasClassLoader(cl);
            data.intent.prepareToEnterProcess();
            data.setExtrasClassLoader(cl);
            //反射创建receiver
            receiver = packageInfo.getAppFactory()
                    .instantiateReceiver(cl, data.info.name, data.intent);
        } catch (Exception e) {
        }

        try {
            sCurrentBroadcastIntent.set(data.intent);
            receiver.setPendingResult(data);
            //调用onReceive同上
            receiver.onReceive(context.getReceiverRestrictedContext(),
                    data.intent);
        } catch (Exception e) {
        } finally {
            sCurrentBroadcastIntent.set(null);
        }

        if (receiver.getPendingResult() != null) {
            data.finish();
        }
    }


//attachApplication
     if (!badApp && isPendingBroadcastProcessLocked(pid)) {
            try {
                didSomething |= sendPendingBroadcastsLocked(app);
                checkTime(startTime, "attachApplicationLocked: after sendPendingBroadcastsLocked");
            } catch (Exception e) {
                // If the app died trying to launch the receiver we declare it 'bad'
                Slog.wtf(TAG, "Exception thrown dispatching broadcasts in " + app, e);
                badApp = true;
            }
        }
        
    boolean sendPendingBroadcastsLocked(ProcessRecord app) {
        boolean didSomething = false;
        for (BroadcastQueue queue : mBroadcastQueues) {
            didSomething |= queue.sendPendingBroadcastsLocked(app);
        }
        return didSomething;
    }
    

    public boolean sendPendingBroadcastsLocked(ProcessRecord app) {
        boolean didSomething = false;
        final BroadcastRecord br = mPendingBroadcast;
        if (br != null && br.curApp.pid > 0 && br.curApp.pid == app.pid) {
            if (br.curApp != app) {
                return false;
            }
            try {
                mPendingBroadcast = null;
                //处理广播
                processCurBroadcastLocked(br, app, false);
                didSomething = true;
            } catch (Exception e) {
                logBroadcastReceiverDiscardLocked(br);
                finishReceiverLocked(br, br.resultCode, br.resultData,
                        br.resultExtras, br.resultAbort, false);
                scheduleBroadcastsLocked();
                // We need to reset the state if we failed to start the receiver.
                br.state = BroadcastRecord.IDLE;
                throw new RuntimeException(e.getMessage());
            }
        }
        return didSomething;
    }
```

处理广播是在`BroadcastHandler`中处理的 调用`processNextBroadcast`,优先处理动态广播，然后处理静态广播，都是调用`deliverToRegisteredReceiverLocked`会调用`app.thread.scheduleRegisteredReceiver` 调用到注册的receiver.onReceive;如果进程存在调用`processCurBroadcastLocked`否则创建新进程。会调用`app.thread.scheduleReceiver`执行`onReceive`。

## 3.静态注册

静态注册是在PackageManagerService中进行的，由于我们之前没有将，这次先忽略。 下次再讲PackageManagerService的时候补充。

## 总结

## 1.注册

### 1.动态注册

没注册过就创建`ReceiverList` 添加到`MregisteredReceivers`中,将`filter`添加到`mReceiverResolver`

### 2.静态注册

略

## 2.发送广播

首先会检查发送的广播是否是一些特殊广播，尤其是应用变化相关的广播，接下来判断是否是粘性广播 如果是粘性广播会检查一些权限（必须指定Component）根据userid从mStickyBroadcasts找到 并添加intent。然后接着查找 广播的接收者(静态通过pkms来找，动态就从mReceiverResolver来找)。 然后进行排序 如果是普通广播优先发给动态接收者，如果是有序广播会根据优先级排序(如果相同动态会比静态先接收) 创建`BroadcastRecord`存入队列，通过`scheduleBroadcastLocked`发送广播。在`BroadcastHandler`中处理的 调用`processNextBroadcast`,优先处理动态广播，然后处理静态广播，都是调用`deliverToRegisteredReceiverLocked`会调用`app.thread.scheduleRegisteredReceiver` 调用到注册的receiver.onReceive;如果进程存在调用`processCurBroadcastLocked`否则创建新进程。会调用`app.thread.scheduleReceiver`执行`onReceive`。

在线视频：

[www.bilibili.com/video/BV1Zs…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1Zs4y1A7Kh%2F%3Fspm_id_from%3D333.999.0.0%26vd_source%3D689a2ec078877b4a664365bdb60362d3 "https://www.bilibili.com/video/BV1Zs4y1A7Kh/?spm_id_from=333.999.0.0&vd_source=689a2ec078877b4a664365bdb60362d3")