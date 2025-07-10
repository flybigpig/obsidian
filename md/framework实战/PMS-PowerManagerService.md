> hongxi.zhu 2023-7-28  
> Android 13

**PowerManagerService(简称PMS)主要是负责协调、管理设备CPU资源，并提供功能接口给应用框架层或应用层申请获取CPU资源的一个服务，例如：亮灭屏、关机、WakeLock管理、Dreamland(屏保模式)、休眠时间等行为。**

### 1\. PMS的启动流程

PMS是在SystemServer的startBootstrapServices()中通过SystemServiceManager.startService()启动，很多服务依赖PMS服务，所以它需要尽早的启动并注册到ServiceManage中，所以它在startBootstrapServices()中来启动

```java
frameworks/base/services/java/com/android/server/SystemServer.java

    private void startBootstrapServices(@NonNull TimingsTraceAndSlog t) {
        t.traceBegin("StartPowerManager");
        mPowerManagerService = mSystemServiceManager.startService(PowerManagerService.class);
        t.traceEnd();
    }
```

SystemServiceManager.startService()通过反射拿到PowerManagerService对象

```java
frameworks/base/services/core/java/com/android/server/SystemServiceManager.java

    public <T extends SystemService> T startService(Class<T> serviceClass) {
        try {
            final String name = serviceClass.getName();
            final T service;
            try {
            	//获取类构造方法，构造对应对象
                Constructor<T> constructor = serviceClass.getConstructor(Context.class);
                service = constructor.newInstance(mContext);
            } catch (InstantiationException ex) {
                throw new RuntimeException("Failed to create service " + name
                        + ": service could not be instantiated", ex);
            } catch (IllegalAccessException ex) {
				...
            }
			//调用startService启动服务
            startService(service);
            //返回服务对象
            return service;
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
        }
    }

    public void startService(@NonNull final SystemService service) {
        // Check if already started
        String className = service.getClass().getName();

        mServiceClassnames.add(className);

        // Register it.
        //注册服务
        mServices.add(service);

        // Start it.
        long time = SystemClock.elapsedRealtime();
        try {
        	//调用服务的onStart()方法启动服务
            service.onStart();
        } catch (RuntimeException ex) {
            throw new RuntimeException("Failed to start service " + service.getClass().getName()
                    + ": onStart threw an exception", ex);
        }
        warnIfTooLong(SystemClock.elapsedRealtime() - time, service, "onStart");
    }
```

将服务加入mServices列表中，然后调用服务的onStart()方法，mServices列表是管理着所有需要接收系统启动过程中生命周期的服务

#### 1.1 PowerManagerService构造方法

```java
frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java

    public PowerManagerService(Context context) {
        this(context, new Injector());  //内部类Injector对象，管理创建其他对象
    }

    PowerManagerService(Context context, Injector injector) {
        super(context);

        mContext = context;
        mBinderService = new BinderService();  //用于IPC交互, BinderService 继承于 IPowerManager.Stub
        mLocalService = new LocalService();  //用户跨线程交互(SystemServer进程内部)
        //创建一个NativeWrapper对象，管理所有的native交互
        mNativeWrapper = injector.createNativeWrapper();
        //创建SystemPropertiesWrapper对象，用于获取系统属性等信息
        mSystemProperties = injector.createSystemPropertiesWrapper();
        mClock = injector.createClock();
        //内部类Injector对象，管理创建其他对象
        mInjector = injector;
        
		//创建PMS主线程，优先级为DISPLAY级别
        mHandlerThread = new ServiceThread(TAG,
                Process.THREAD_PRIORITY_DISPLAY, /* allowIo= */ false);
        mHandlerThread.start();  //启动主线程
        //创建PMS主线程Handler
        mHandler = injector.createHandler(mHandlerThread.getLooper(),
                new PowerManagerHandlerCallback());
        //用于管理Settings.Global下的常量
        mConstants = new Constants(mHandler);
        //创建AmbientDisplayConfiguration对象，管理一些显示配置，如AOD
        mAmbientDisplayConfiguration = mInjector.createAmbientDisplayConfiguration(context);
        //创建AmbientDisplaySuppressionController对象，用于管理AmbientDisplay显示相关，和AOD显示有关
        mAmbientDisplaySuppressionController =
                mInjector.createAmbientDisplaySuppressionController(context);
        //创建AttentionDetector对象，用来检查用户活动是否需要再更新
        mAttentionDetector = new AttentionDetector(this::onUserAttention, mLock);
        
        mFaceDownDetector = new FaceDownDetector(this::onFlip);
        
        mScreenUndimDetector = new ScreenUndimDetector();
		//创建BatterySavingStats对象，用来记录电池耗电率
        mBatterySavingStats = new BatterySavingStats(mLock);
        //创建BatterySaverPolicy对象，管理一些省电策略
        mBatterySaverPolicy =
                mInjector.createBatterySaverPolicy(mLock, mContext, mBatterySavingStats);
        //创建BatterySaverController对象，管理省电策略的切换
        mBatterySaverController = mInjector.createBatterySaverController(mLock, mContext,
                mBatterySaverPolicy, mBatterySavingStats);
        //创建BatterySaverStateMachine对象，负责省电策略的开启/关闭
        mBatterySaverStateMachine = mInjector.createBatterySaverStateMachine(mLock, mContext,
                mBatterySaverController);

        mLowPowerStandbyController = mInjector.createLowPowerStandbyController(mContext,
                Looper.getMainLooper());
                
        mInattentiveSleepWarningOverlayController =
                mInjector.createInattentiveSleepWarningController();

        mAppOpsManager = injector.createAppOpsManager(mContext);

        mPowerGroupWakefulnessChangeListener = new PowerGroupWakefulnessChangeListener();
        
		...		//获取一些配置初始值
		
        synchronized (mLock) {
        	//创建SuspendBlockerImpl对象，"PowerManagerService.Booting"类型对象负责在开机时负责保活CPU
            mBootingSuspendBlocker =
                    mInjector.createSuspendBlocker(this, "PowerManagerService.Booting");
	       //创建SuspendBlockerImpl对象，WakeLock的最终反映，"PowerManagerService.WakeLocks"类型对象负责保活CPU
            mWakeLockSuspendBlocker =
                    mInjector.createSuspendBlocker(this, "PowerManagerService.WakeLocks");
            //创建SuspendBlockerImpl对象，"PowerManagerService.Display"类型对象负责保持屏幕常亮
            mDisplaySuspendBlocker =
                    mInjector.createSuspendBlocker(this, "PowerManagerService.Display");
            //申请mBootingSuspendBlocker，保持开机启动过程CPU资源
            if (mBootingSuspendBlocker != null) {
                mBootingSuspendBlocker.acquire();
                mHoldingBootingSuspendBlocker = true;
            }
            //申请mDisplaySuspendBlocker，保持屏幕常亮
            if (mDisplaySuspendBlocker != null) {
                mDisplaySuspendBlocker.acquire(HOLDING_DISPLAY_SUSPEND_BLOCKER);
                mHoldingDisplaySuspendBlocker = true;
            }
            //auto-suspend 模式是否可用
            mHalAutoSuspendModeEnabled = false;
            //是否是可交互状态
            mHalInteractiveModeEnabled = true;
			//设置设备状态为唤醒状态
            mWakefulnessRaw = WAKEFULNESS_AWAKE;
            //静默模式，会控制背光的点亮，使用场景不多
            sQuiescent = mSystemProperties.get(SYSTEM_PROPERTY_QUIESCENT, "0").equals("1")
                    || InitProperties.userspace_reboot_in_progress().orElse(false);
                    
			//mNativeWrapper对象进行native层初始化工作
            mNativeWrapper.nativeInit(this);
            mNativeWrapper.nativeSetAutoSuspend(false);  //设置auto suspend状态
            mNativeWrapper.nativeSetPowerMode(Mode.INTERACTIVE, true);  //设置interactive状态
            mNativeWrapper.nativeSetPowerMode(Mode.DOUBLE_TAP_TO_WAKE, false);  //设置双击唤醒模式状态
            mInjector.invalidateIsInteractiveCaches();
        }
    }
```

构造方法主要做了几件事：

1.  创建用于IPC的BinderService对象和system\_server进程内跨线程交互的LocalService对象；
2.  创建PMS的主线程，并使用该HandlerThread的Looper实例化PowerManagerHandler，将作为PMS主线程Handler进行Message的处理
3.  相关配置参数的读取，如系统配置各种亮度参数
4.  获取了三个Suspend锁对象，SuspendBlocker是一种锁机制，上层申请的wakelock锁在PMS中都会反映为SuspendBlocker锁
5.  通过mNativeWrapper对象和底层进行状态交互

#### 1.2 onStart()中注册发布服务

执行完构造方法后接着执行了onStart()

```java
frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java

    @Override
    public void onStart() {
    	//向ServiceManager进程注册当前服务mBinderService(BinderServic才是PMS的Binder服务端)
        publishBinderService(Context.POWER_SERVICE, mBinderService, /* allowIsolated= */ false,
                DUMP_FLAG_PRIORITY_DEFAULT | DUMP_FLAG_PRIORITY_CRITICAL);
        //发布本地服务提供给SystemServer进程中其他组件访问
        publishLocalService(PowerManagerInternal.class, mLocalService);
		//添加watchdog监听
        Watchdog.getInstance().addMonitor(this);
        Watchdog.getInstance().addThread(mHandler);
    }
```

这个方法中会进行对PMS的Binder服务和Local服务的注册，BinderService注册后，其他模块中就可以通过ServiceManager获取到对应的Binder对象，进行IPC通信；LocalService注册后，在system\_server进程内就可以通过获取LocalService对象跨线程交互。  
PMS中的BinderService，是继承自IPowerManager.Stub的一个内部类BinderService，IPowerManager.Stub继承自Binder并且实现了IPowerManager，因此，PMS和其他模块的跨进程交互，实际上就是通过PMS.BinderService实现。

Binder服务注册的过程在SystemService中的publishBinderService方法，通过ServiceManager.addService注册：

```java
frameworks/base/services/core/java/com/android/server/SystemService.java

    protected final void publishBinderService(String name, IBinder service,
            boolean allowIsolated, int dumpPriority) {
        ServiceManager.addService(name, service, allowIsolated, dumpPriority);
    }
```

```java
frameworks/base/core/java/android/os/ServiceManager.java

    public static void addService(String name, IBinder service) {
        addService(name, service, false, IServiceManager.DUMP_FLAG_PRIORITY_DEFAULT);
    }
```

当通过ServiceManager注册后，就可以根据Context.POWER\_SERVICE在其他服务中获得服务代理对象，进行跨进程交互了。

LocalService则用于system\_server进程内部交互，注册的对象是另一个内部类——继承自PowerManagerInternal的LocalService(带有Internal的类一般都在System进程内使用)。Local Service的注册是在LocalServices中进行，其注册方法如下：

```java
frameworks/base/services/core/java/com/android/server/SystemService.java

    protected final <T> void publishLocalService(Class<T> type, T service) {
        LocalServices.addService(type, service);
    }
```

```java
frameworks/base/core/java/com/android/server/LocalServices.java

    public static <T> void addService(Class<T> type, T service) {
        synchronized (sLocalServiceObjects) {
            if (sLocalServiceObjects.containsKey(type)) {
                throw new IllegalStateException("Overriding service registration");
            }
            sLocalServiceObjects.put(type, service); //添加到sLocalServiceObjects容器中
        }
    }
```

#### 1.3 onBootPhase()进行各个启动阶段的处理

回到SytemServer中，startBootstrapServices()方法中PMS的实例化和注册流程执行完成了，接下来会开始执行SystemService的生命周期，会开始执行SystemServiceManager.onBootPhase()，这个方法为所有的已进行实例化和注册的服务(通过前面的startService启动的服务)设置启动阶段，以便在不同的启动阶段进行不同的工作，方法如下：

```java
frameworks/base/services/core/java/com/android/server/SystemServiceManager.java

    public void startBootPhase(@NonNull TimingsTraceAndSlog t, int phase) {
        mCurrentPhase = phase;
        try {
            final int serviceLen = mServices.size();
            for (int i = 0; i < serviceLen; i++) {
                final SystemService service = mServices.get(i);
                try {
                    service.onBootPhase(mCurrentPhase);
                } catch (Exception ex) {
					...
                }
            }
        } finally {
            t.traceEnd();
        }

        if (phase == SystemService.PHASE_BOOT_COMPLETED) {
            final long totalBootTime = SystemClock.uptimeMillis() - mRuntimeStartUptime;
            t.logDuration("TotalBootTime", totalBootTime);
            SystemServerInitThreadPool.shutdown();
        }
    }
```

在SystemServiceManager#startBootPhase()中，通过在SystemServiceManager中传入不同的形参，遍历SystemServiceManager#mServices列表，调用各个SystemService#onBootPhase(int)方法，根据参数在方法实现中完成不同的工作。在SystemService中定义了七个代表启动阶段的参数：

> PHASE\_WAIT\_FOR\_DEFAULT\_DISPLAY：第一个启动阶段，用于在启动PKMS之前，需要确保已经存在默认逻辑屏，只有DisplayManagerService使用该阶段；  
> PHASE\_LOCK\_SETTINGS\_READY：第二个启动阶段，该阶段的执行，意味这Lock Pattern/Password相关服务已经准备完毕，只有DisplayManagerService使用该阶段；  
> PHASE\_SYSTEM\_SERVICES\_READY：第三个启动阶段，该阶段的执行，意味这其他服务可以安全地使用核心系统服务;  
> PHASE\_DEVICE\_SPECIFIC\_SERVICES\_READY：第四个启动阶段，该阶段的执行，意味这其他服务可以安全地使用设备指定的一些系统服务，这些服务在config\_deviceSpecificSystemServices中进行配置;  
> PHASE\_ACTIVITY\_MANAGER\_READY：第五个启动阶段，该阶段的执行，意味这其他AMS组件已经启动完成，可以进行广播操作;  
> PHASE\_THIRD\_PARTY\_APPS\_CAN\_START：第六个启动阶段，该阶段的执行，意味这系统可以启动APP，并可以进行service的bind/start操作了；  
> PHASE\_BOOT\_COMPLETED：第六个启动阶段，该阶段的执行，意味这启动完成，Home应用也已经启动完成，并且可以和设备进行交互了。

PMS#onBootPhase()方法只对以上的3个阶段做了处理：

```java
frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java

    @Override
    public void onBootPhase(int phase) {
        if (phase == PHASE_SYSTEM_SERVICES_READY) {
            systemReady();  //如果是PHASE_SYSTEM_SERVICES_READY阶段则调用PMS的systemReady()

        } else if (phase == PHASE_THIRD_PARTY_APPS_CAN_START) {
        	//统计系统启动次数，adb shell settings get global boot_count可查看
            incrementBootCount();

        } else if (phase == PHASE_BOOT_COMPLETED) {
            synchronized (mLock) {
                final long now = mClock.uptimeMillis();
                mBootCompleted = true;  //表示启动完成
                mDirty |= DIRTY_BOOT_COMPLETED;  //最重要的标志位
                
				//执行BatterySaverStateMachine.onBootCompleted()
                mBatterySaverStateMachine.onBootCompleted();
                //更新用户活动时间
                userActivityNoUpdateLocked(
                        now, PowerManager.USER_ACTIVITY_EVENT_OTHER, 0, Process.SYSTEM_UID);
				//更新全局状态信息
                updatePowerStateLocked();
                if (sQuiescent) {
                    sleepPowerGroupLocked(mPowerGroups.get(Display.DEFAULT_DISPLAY_GROUP),
                            mClock.uptimeMillis(),
                            PowerManager.GO_TO_SLEEP_REASON_QUIESCENT,
                            Process.SYSTEM_UID);
                }
				//注册DeviceStateManager服务的回调接口
                mContext.getSystemService(DeviceStateManager.class).registerCallback(
                        new HandlerExecutor(mHandler), new DeviceStateListener());
            }
        }
    }
```

mDirty是一个二进制的标记位，用来表示电源状态哪一部分发生了改变，通过对其进行置位（|操作）、清零（～操作），得到二进制数各个位的值(0或1)，进行不同的处理，这个变量非常重要。  
最后，调用updatePowerStateLocked()方法，这是整个PMS中最重要的方法，会在下面进行详细分析。  
此时，启动过程中SystemService的生命周期方法全部执行完毕。

#### 1.4 sytemReady()

当onBootPhase处于PHASE\_SYSTEM\_SERVICES\_READY阶段时，执行sytemReady()

```java
    private void systemReady() {
		//方法太长了
    }
```

这个方法中主要做了以下几个操作，方法太长了相关方法代码就不再粘贴:

获取各类本地服务和远程服务，如Dreamland服务(DreamMangerService)、窗口服务(PhoneWindowManager)、电池状态监听(BatteryService)等服务；  
注册用于和其他SytemService交互的广播；  
调用updateSettingsLocked()方法更新Settings中值的变化；  
调用readConfigurationLocked()方法读取配置文件中的默认值；  
注册SettingsObserver监听；

到此为止，PMS的启动过程完成。

### 2\. 核心方法updatePowerStateLocked()

updatePowerStateLocked()方法是整个PMS模块的核心方法，也是整个PSM中最重要的一个方法，它用来更新整个Power状态。当Power状态发生改变时，如亮灭屏、电池状态改变、暗屏、WakeLock锁申请/释放…都会调用该方法，并调用其他同级方法进行各个状态的更新：

```java
frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java

    private void updatePowerStateLocked() {
        if (!mSystemReady || mDirty == 0 || mUpdatePowerStateInProgress) {
            return;
        }

        Trace.traceBegin(Trace.TRACE_TAG_POWER, "updatePowerState");
        mUpdatePowerStateInProgress = true;
        try {
            // Phase 0: Basic state updates. 基本状态的更新
            updateIsPoweredLocked(mDirty);  //更新充电状态
            updateStayOnLocked(mDirty);  //更新当前是否为屏幕常亮状态，由mStayOn控制
            updateScreenBrightnessBoostLocked(mDirty);  //更新是否需要增强亮度变量

            // Phase 1: Update wakefulness. 更新唤醒状态
            // Loop because the wake lock and user activity computations are influenced
            // by changes in wakefulness.
            // 循环是因为唤醒锁和用户活动计算受到唤醒状态变化的影响。
            final long now = mClock.uptimeMillis();
            int dirtyPhase2 = 0;
            for (;;) { // 循环进行更新流程，直到updateWakefulnessLocked()返回false
                int dirtyPhase1 = mDirty;
                dirtyPhase2 |= dirtyPhase1;
                mDirty = 0;  //清空标记
				// 更新用于统计wakelock的标记值mWakeLockSummary属性
                updateWakeLockSummaryLocked(dirtyPhase1);
                // 更新用于统计用户活动状态的标记值mUserActivitySummary属性
                updateUserActivitySummaryLocked(now, dirtyPhase1);
                // 更新细微模式状态
                updateAttentiveStateLocked(now, dirtyPhase1);
                // 更新唤醒状态，如果状态改变返回true
                if (!updateWakefulnessLocked(dirtyPhase1)) {
                    break;
                }
            }

            // Phase 2: Lock profiles that became inactive/not kept awake.
            updateProfilesLocked(now);

            // Phase 3: Update power state of all PowerGroups. 更新PowerGroups状态
            final boolean powerGroupsBecameReady = updatePowerGroupsLocked(dirtyPhase2);

            // Phase 4: Update dream state (depends on power group ready signal). 更新Dreamland状态
            updateDreamLocked(dirtyPhase2, powerGroupsBecameReady);

            // Phase 5: Send notifications, if needed. 如果wakefulness改变，做最后的收尾工作
            finishWakefulnessChangeIfNeededLocked();

            // Phase 6: Update suspend blocker.
            // Because we might release the last suspend blocker here, we need to make sure
            // we finished everything else first! 更新SuspendBlocker锁状态
            updateSuspendBlockerLocked();
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_POWER);
            mUpdatePowerStateInProgress = false;
        }
    }
```

下面对以上内容中所有方法逐个进行分析

#### 2.1 updateIsPoweredLocked()更新充电状态

这个方法用于更新mIsPowered属性，它代表当前的充电状态。插拔USB点亮屏幕功能的逻辑，就是在这个方法中。该方法如下：

```java
    private void updateIsPoweredLocked(int dirty) {
        if ((dirty & DIRTY_BATTERY_STATE) != 0) {
            // 记录旧值
            final boolean wasPowered = mIsPowered;
            final int oldPlugType = mPlugType;
            // 获取新值
            mIsPowered = mBatteryManagerInternal.isPowered(BatteryManager.BATTERY_PLUGGED_ANY);
            mPlugType = mBatteryManagerInternal.getPlugType();
            mBatteryLevel = mBatteryManagerInternal.getBatteryLevel();
            mBatteryLevelLow = mBatteryManagerInternal.getBatteryLevelLow();

			// 当充电状态发生变化
            if (wasPowered != mIsPowered || oldPlugType != mPlugType) {
                mDirty |= DIRTY_IS_POWERED; // 设置标记位DIRTY_IS_POWERED

                // 更新无线充电状态
                final boolean dockedOnWirelessCharger = mWirelessChargerDetector.update(
                        mIsPowered, mPlugType);

                final long now = mClock.uptimeMillis();
                // 插拔USB是否需要亮屏
                if (shouldWakeUpWhenPluggedOrUnpluggedLocked(wasPowered, oldPlugType,
                        dockedOnWirelessCharger)) {
                    // 亮屏流程
                    wakePowerGroupLocked(mPowerGroups.get(Display.DEFAULT_DISPLAY_GROUP),
                            now, PowerManager.WAKE_REASON_PLUGGED_IN,
                            "android.server.power:PLUGGED:" + mIsPowered, Process.SYSTEM_UID,
                            mContext.getOpPackageName(), Process.SYSTEM_UID);
                }
				// 更新用户活动时间
                userActivityNoUpdateLocked(mPowerGroups.get(Display.DEFAULT_DISPLAY_GROUP), now,
                        PowerManager.USER_ACTIVITY_EVENT_OTHER, 0, Process.SYSTEM_UID);

				// 播放充电提示音和动画
                if (mBootCompleted) {  //只有在开机完成后才允许播放充电提示声
                    if (mIsPowered && !BatteryManager.isPlugWired(oldPlugType)
                            && BatteryManager.isPlugWired(mPlugType)) {
                        mNotifier.onWiredChargingStarted(mUserId);
                    } else if (dockedOnWirelessCharger) {
                        mNotifier.onWirelessChargingStarted(mBatteryLevel, mUserId);
                    }
                }
            }
			//将充电状态更新到BatterySaverStateMachine
            mBatterySaverStateMachine.setBatteryStatus(mIsPowered, mBatteryLevel, mBatteryLevelLow);
        }
    }
```

只有对mDirty设置了DIRTY\_BATTERY\_STATE标记时才会进入这个方法中，那么DIRTY\_BATTERY\_STATE在什么时候会设置呢？一是当PMS执行systemReady()时，二是当电池信息变化后，由healthd模块上报给BatteryService，BatteryService中则会发出广播Intent.ACTION\_BATTERY\_CHANGED来通知其他组件，PMS中会接收这个广播并作出处理：

```java
frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java

    final class BatteryReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            synchronized (mLock) {
                handleBatteryStateChangedLocked();
            }
        }
    }

    private void handleBatteryStateChangedLocked() {
        mDirty |= DIRTY_BATTERY_STATE;
        updatePowerStateLocked();
    }
```

因此，只要电池状态发生变化，就会执行这个方法。在这个方法中：

1.  首先，更新一些如mIsPowered等全局变量。
2.  然后，通过shouldWakeUpWhenPluggedOrUnpluggedLocked()方法判断是否需要亮屏，这就是插拔USB点亮屏幕功能的逻辑。
3.  接下来，执行userActivityNoUpdateLocked()方法更新用户活动时间，这个时间决定了何时会自动灭屏，用户每次操作手机(触摸、按键、Other)都会更新到当前时间，用户最后活动时间 + 设置自动休眠时间 = 最终自动灭屏的时间点，这个方法会在后面部分分析。
4.  再接下来，则调用Notifier对象播放插拔USB音效或动画，Notifier类是PMS模块中用于发送广播、异步通知其他组件的一个类。
5.  最后，将充电状态传递给mBatterySaverStateMachine中，进行省电策略的调整。

#### 2.2 updateStayOnLocked()更新屏幕常亮状态

这个方法用来更新全局变量mStayOn的值，如果把"开发者选项—不锁定屏幕"这个选项开启后，则在充电时将保持常亮不会自动休眠：

```java
frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java

    private void updateStayOnLocked(int dirty) {
        if ((dirty & (DIRTY_BATTERY_STATE | DIRTY_SETTINGS)) != 0) {
            final boolean wasStayOn = mStayOn;
            if (mStayOnWhilePluggedInSetting != 0
                    && !isMaximumScreenOffTimeoutFromDeviceAdminEnforcedLocked()) {
                // 如果任意方式充电(AC/USB/wireless)，返回true
                mStayOn = mBatteryManagerInternal.isPowered(mStayOnWhilePluggedInSetting);
            } else {
                mStayOn = false;
            }
			// 状态发生变化时，向mDirty设置DIRTY_STAY_ON标记
            if (mStayOn != wasStayOn) {
                mDirty |= DIRTY_STAY_ON;
            }
        }
    }
```

只有mDirty设置了DIRTY\_BATTERY\_STATE或DIRTY\_SETTINGS标记位后，才会执行该方法。DIRTY\_BATTERY\_STATE在电池状态发生变化后设置，DIRTY\_SETTINGS是在Settings中的值发生变化后设置，mStayOnWhilePluggedInSetting就是从来自于Settings中"不锁定屏幕"的值，如果发生变化，且处于充电状态，则会更新mStayOn变量的值。在自动灭屏流程中，一旦mStayOn值为true，则永远不会灭屏，从而实现了“不锁定屏幕”这个功能。

#### 2.3 updateScreenBrightnessBoostLocked()更新是否增强亮度

这个方法会更新表示增强亮度的全局变量mScreenBrightnessBoostInProgress，PMS.BinderService提供了boostScreenBrightness()方法，允许其他组件通过该接口将亮度调节到最大(短时间保持)，并保持5s后恢复：

```java
    private void updateScreenBrightnessBoostLocked(int dirty) {
        if ((dirty & DIRTY_SCREEN_BRIGHTNESS_BOOST) != 0) {
            if (mScreenBrightnessBoostInProgress) {
                final long now = mClock.uptimeMillis();
                mHandler.removeMessages(MSG_SCREEN_BRIGHTNESS_BOOST_TIMEOUT);
                if (mLastScreenBrightnessBoostTime > mLastGlobalSleepTime) {
                    final long boostTimeout = mLastScreenBrightnessBoostTime +
                            SCREEN_BRIGHTNESS_BOOST_TIMEOUT;
                    // 向主线程mHandler发出一个异步消息，5s后会再次更新
                    if (boostTimeout > now) {
                        Message msg = mHandler.obtainMessage(MSG_SCREEN_BRIGHTNESS_BOOST_TIMEOUT);
                        msg.setAsynchronous(true);
                        mHandler.sendMessageAtTime(msg, boostTimeout);
                        return;
                    }
                }
                // 表示增强亮度结束
                mScreenBrightnessBoostInProgress = false;
                // 更新用户活动时间
                userActivityNoUpdateLocked(now,
                        PowerManager.USER_ACTIVITY_EVENT_OTHER, 0, Process.SYSTEM_UID);
            }
        }
    }
```

只有当mDirty设置了DIRTY\_SCREEN\_BRIGHTNESS\_BOOST标记时，才会执行这个方法。这个标记位就是通过boostScreenBrightness()设置，这个功能在Google原生逻辑中有一个使用场景：

```java
// frameworks/base/services/core/java/com/android/server/policy/PhoneWindowManager.java

//是否开启双击Power键定制行为(例如双击打开NFC,相机等)
mDoublePressOnPowerBehavior = mContext.getResources().getInteger(
        com.android.internal.R.integer.config_doublePressOnPowerBehavior);

    // must match: config_doublePressOnPowerBehavior in config.xml
    static final int MULTI_PRESS_POWER_NOTHING = 0;  //默认无
    static final int MULTI_PRESS_POWER_THEATER_MODE = 1;  //剧场模式
    static final int MULTI_PRESS_POWER_BRIGHTNESS_BOOST = 2;  //双击电源键会将亮度调到最大
    static final int MULTI_PRESS_POWER_LAUNCH_TARGET_ACTIVITY = 3;  //双击打开特定应用
```

#### 2.4 updateWakeLockSummaryLocked()更新WakeLock统计值

从这个方法开始到，直到updateWakefulnessLocked()结束，将会在一个for循环中执行。  
该方法用来更新mWakeLockSummary属性，它是用来记录所有WakeLock锁状态的状态值，代表了所有的WakeLock，在请求Display状时作为判断条件确定具体的请求状态。系统规定了系统休眠状态对WakeLock锁的使用影响，如当系统休眠后，常亮锁(PowerManager.SCREEN\_BRIGHT等)将会被忽略；系统唤醒后，Doze锁(PowerManager.DOZE\_WAKE\_LOCK等）将会被忽略：

```java
    private void updateWakeLockSummaryLocked(int dirty) {
        if ((dirty & (DIRTY_WAKE_LOCKS | DIRTY_WAKEFULNESS | DIRTY_DISPLAY_GROUP_WAKEFULNESS))
                != 0) {

            mWakeLockSummary = 0;  //初始值为0
			// 将每个ProfilePowerState的mWakeLockSummary也进行重置
            final int numProfiles = mProfilePowerState.size();
            for (int i = 0; i < numProfiles; i++) {
                mProfilePowerState.valueAt(i).mWakeLockSummary = 0;
            }
			// 将每个mPowerGroups的setWakeLockSummaryLocked也进行重置
            for (int idx = 0; idx < mPowerGroups.size(); idx++) {
                mPowerGroups.valueAt(idx).setWakeLockSummaryLocked(0);
            }

            int invalidGroupWakeLockSummary = 0;
            final int numWakeLocks = mWakeLocks.size();
            // 遍历mWakeLocks列表
            for (int i = 0; i < numWakeLocks; i++) {
                final WakeLock wakeLock = mWakeLocks.get(i);
                final Integer groupId = wakeLock.getPowerGroupId();
                // a wakelock with an invalid group ID should affect all groups
                if (groupId == null || (groupId != Display.INVALID_DISPLAY_GROUP
                        && !mPowerGroups.contains(groupId))) {
                    continue;
                }

                final PowerGroup powerGroup = mPowerGroups.get(groupId);
                // 获取每个WakeLock对应的Flag标记
                final int wakeLockFlags = getWakeLockSummaryFlags(wakeLock);
                 // 标记在mWakeLockSummary上
                mWakeLockSummary |= wakeLockFlags;

                if (groupId != Display.INVALID_DISPLAY_GROUP) {
                    int wakeLockSummary = powerGroup.getWakeLockSummaryLocked();
                    wakeLockSummary |= wakeLockFlags;
                    powerGroup.setWakeLockSummaryLocked(wakeLockSummary);
                } else {
                    invalidGroupWakeLockSummary |= wakeLockFlags;
                }
				// 对每个ProfilePowerState#mWakeLockSummary也进行标记
                for (int j = 0; j < numProfiles; j++) {
                    final ProfilePowerState profile = mProfilePowerState.valueAt(j);
                    if (wakeLockAffectsUser(wakeLock, profile.mUserId)) {
                        profile.mWakeLockSummary |= wakeLockFlags;
                    }
                }
            }

            for (int idx = 0; idx < mPowerGroups.size(); idx++) {
                final PowerGroup powerGroup = mPowerGroups.valueAt(idx);
                final int wakeLockSummary = adjustWakeLockSummary(powerGroup.getWakefulnessLocked(),
                        invalidGroupWakeLockSummary | powerGroup.getWakeLockSummaryLocked());
                powerGroup.setWakeLockSummaryLocked(wakeLockSummary);
            }
			// 根据系统状态对mWakeLockSummary进行调整
            mWakeLockSummary = adjustWakeLockSummary(getGlobalWakefulnessLocked(),
                    mWakeLockSummary);
			// 对每个ProfilePowerState#mWakeLockSummary也进行调整
            for (int i = 0; i < numProfiles; i++) {
                final ProfilePowerState profile = mProfilePowerState.valueAt(i);
                profile.mWakeLockSummary = adjustWakeLockSummary(getGlobalWakefulnessLocked(),
                        profile.mWakeLockSummary);
            }
        }
    }
```

只有当mDirty设置了DIRTY\_WAKE\_LOCKS和DIRTY\_WAKEFULNESS标记位时，才会执行该方法。DIRTY\_WAKE\_LOCKS在申请WakeLock锁时设置，DIRTY\_WAKEFULNESS在系统唤醒状态发生变化时设置。  
进入该方法后，首先从保存了WakeLock的List中进行遍历，并根据WakeLock类型给mWakeLockSummary设置标记，这些标记位如下:

```java
// frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java

    // Summarizes the state of all active wakelocks.
    static final int WAKE_LOCK_CPU = 1 << 0;  // 表示需要CPU保持唤醒状态
    static final int WAKE_LOCK_SCREEN_BRIGHT = 1 << 1;  // 表示持有FULL_WAKE_LOCK锁，需要屏幕常亮
    static final int WAKE_LOCK_SCREEN_DIM = 1 << 2;  // 表示持有SCREEN_DIM_WAKE_LOCK锁，需要保持dim不灭屏
    static final int WAKE_LOCK_BUTTON_BRIGHT = 1 << 3;  //表示持有FULL_WAKE_LOCK锁，需要按键灯常亮
    static final int WAKE_LOCK_PROXIMITY_SCREEN_OFF = 1 << 4;  // 表示持有Psensor WakeLock锁
    static final int WAKE_LOCK_STAY_AWAKE = 1 << 5; // only set if already awake  // 表示保持屏幕常亮(只能用在awake状态时)
    static final int WAKE_LOCK_DOZE = 1 << 6;   // 表示持有DOZE_WAKE_LOCK锁
    static final int WAKE_LOCK_DRAW = 1 << 7;   //表示持有DRAW_WAKE_LOCK锁
```

然后将根据系统状态进行调整:

```java
// frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java

    private static int adjustWakeLockSummary(int wakefulness, int wakeLockSummary) {
        // Cancel wake locks that make no sense based on the current state.
         // 唤醒状态不处于Doze状态时，忽略掉PowerManager.DOZE_WAKE_LOCK和PowerManager.DRAW_WAKE_LOCK两类型锁
        if (wakefulness != WAKEFULNESS_DOZING) {
            wakeLockSummary &= ~(WAKE_LOCK_DOZE | WAKE_LOCK_DRAW);
        }
        // 唤醒状态处于Asleep状态或者Doze状态时，忽略掉屏幕常亮锁、PSensor锁
        if (wakefulness == WAKEFULNESS_ASLEEP
                || (wakeLockSummary & WAKE_LOCK_DOZE) != 0) {
            wakeLockSummary &= ~(WAKE_LOCK_SCREEN_BRIGHT | WAKE_LOCK_SCREEN_DIM
                    | WAKE_LOCK_BUTTON_BRIGHT);
            if (wakefulness == WAKEFULNESS_ASLEEP) {
                wakeLockSummary &= ~WAKE_LOCK_PROXIMITY_SCREEN_OFF;
            }
        }

        // Infer implied wake locks where necessary based on the current state.
        if ((wakeLockSummary & (WAKE_LOCK_SCREEN_BRIGHT | WAKE_LOCK_SCREEN_DIM)) != 0) {
        	// 唤醒状态处于Awake状态，WAKE_LOCK_STAY_AWAKE只用于awake状态时
            if (wakefulness == WAKEFULNESS_AWAKE) {
                wakeLockSummary |= WAKE_LOCK_CPU | WAKE_LOCK_STAY_AWAKE;
            } else if (wakefulness == WAKEFULNESS_DREAMING) {  // 唤醒状态处于Awake状态，WAKE_LOCK_STAY_AWAKE只用于awake状态时
                wakeLockSummary |= WAKE_LOCK_CPU;
            }
        }
        // 有DRAW_WAKE_LOCK锁时，需要CPU保持唤醒
        if ((wakeLockSummary & WAKE_LOCK_DRAW) != 0) {
            wakeLockSummary |= WAKE_LOCK_CPU;
        }

        return wakeLockSummary;
    }
```

> 在平时分析处理不灭屏相关Bug时，通过该值可确定当前系统持有哪些类型的锁。

最终得到mWakeLockSummary，在自动灭屏流程中将使用起到重要作用，当自动灭屏时，如果mWakeLockSummary设置有WAKE\_LOCK\_STAY\_AWAKE标记位，那么将不会灭屏。  
此外，上面方法中出现了对ProfilePowerState对象的处理，它用来对不同user进行不同参数的设置，在多用户模式下，可以支持不同的状态，这部分略去。

#### 2.5 updateUserActivitySummaryLocked()更新用户活动状态

这个方法用来更新全局变量mUserActivitySummary，它表示用户活动状态，有三个值:

```java
    // Summarizes the user activity state.
    static final int USER_ACTIVITY_SCREEN_BRIGHT = 1 << 0;  // 表示亮屏状态下的交互
    static final int USER_ACTIVITY_SCREEN_DIM = 1 << 1;  // 表示Dim状态下的交互
    static final int USER_ACTIVITY_SCREEN_DREAM = 1 << 2;  // 表示Dreamland状态下的交互
```

何时开始自动灭屏，就是这个方法中实现的。当设备和用户有交互时，都会根据当前时间和自动灭屏时间、Dim时长、当前唤醒状态计算下次休眠的时间，完成自动灭屏的操作。由亮屏进入Dim的时长、Dim到灭屏的时长、亮屏到屏保的时长，都是在这里计算的，这个方法的详细分析见后面的灭屏流程篇。

#### 2.6 updateAttentiveStateLocked()更新细微模式状态

这个方法用来更新细微模式状态，这是Android R上新添加的一个功能，其目的就是解决用户长时间没有操作但一直持有亮屏锁导致系统不灭屏这个场景的，算是一个省电优化项，看下是如何实现的：

```java
//frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java

    private void updateAttentiveStateLocked(long now, int dirty) {
    	// 触发细微模式的时间阈值
        long attentiveTimeout = getAttentiveTimeoutLocked();
        // Attentive state only applies to the default display group.
        // 自动休眠时间
        long goToSleepTime = mPowerGroups.get(
                Display.DEFAULT_DISPLAY_GROUP).getLastUserActivityTimeLocked() + attentiveTimeout;
         // 细微模式提示Dialog弹出时间
        long showWarningTime = goToSleepTime - mAttentiveWarningDurationConfig;
		// 是否已经弹出提升对话框
        boolean warningDismissed = maybeHideInattentiveSleepWarningLocked(now, showWarningTime);

        if (attentiveTimeout >= 0 && (warningDismissed
                || (dirty & (DIRTY_ATTENTIVE | DIRTY_STAY_ON | DIRTY_SCREEN_BRIGHTNESS_BOOST
                | DIRTY_PROXIMITY_POSITIVE | DIRTY_WAKEFULNESS | DIRTY_BOOT_COMPLETED
                | DIRTY_SETTINGS)) != 0)) {

            mHandler.removeMessages(MSG_ATTENTIVE_TIMEOUT);
			// 是否需要弹出警告
            if (isBeingKeptFromInattentiveSleepLocked()) {
                return;
            }

            long nextTimeout = -1;

            if (now < showWarningTime) {
                nextTimeout = showWarningTime;
            } else if (now < goToSleepTime) {
				// 弹出警告给用户
                mInattentiveSleepWarningOverlayController.show();
                nextTimeout = goToSleepTime;
            }
			 // 下一次进入时将会灭屏
            if (nextTimeout >= 0) {
                scheduleAttentiveTimeout(nextTimeout);
            }
        }
    }
```

这里提供了两个配置值：

+   mAttentiveWarningDurationConfig表示触发细微模式前，弹出警告的时长，到达该时间时，会弹出对话框提示用户是否还要亮屏；
+   mAttentiveTimeoutConfig表示触发细微模式的时间阈值，到达该时长后，会进行自动灭屏；  
    这种情况下，即使没有达到用户设置的自动休眠时间，也会进行自动灭屏。

#### 2.7 updateWakefulnessLocked()是否需要更新唤醒状态

这个方法也和自动灭屏流程有关。如果满足自动灭屏条件，会更新系统唤醒状态。  
这三个方法放在for(;;)循环中执行，是因为它们共同决定了设备的唤醒状态，前两个方法是汇总状态，后一个方法是根据前两个方法汇总的值而进行判断是否要改变当前的设备唤醒状态，汇总状态会受mWakefulness的影响，因此会进行循环处理。  
同时，也仅仅会在超时灭屏进入睡眠或屏保时，for循环会执行两次，其他情况下，只会执行一次。这个方法的详细分析见PMS灭屏流程。

#### 2.8 updateProfilesLocked()

以上几个方法针对全局状态进行更新，这个方法则根据ProfilePowerState中保存的状态，更新不同用户是否进入锁定状态，由于使用场景不是很高，这里暂且略过。

#### 2.9 updatePowerGroupsLocked

该方法用于请求并更新Display状态，在这个方法中，会确定多个影响Display状态的属性，并将这些值封装到DisplayPowerRequest对象中，向DisplayMangerService发起请求，最终由DMS完成Display亮度、状态的更新：

```java
    private boolean updatePowerGroupsLocked(int dirty) {
        final boolean oldPowerGroupsReady = areAllPowerGroupsReadyLocked();
        if ((dirty & (DIRTY_WAKE_LOCKS | DIRTY_USER_ACTIVITY | DIRTY_WAKEFULNESS
                | DIRTY_ACTUAL_DISPLAY_POWER_STATE_UPDATED | DIRTY_BOOT_COMPLETED
                | DIRTY_SETTINGS | DIRTY_SCREEN_BRIGHTNESS_BOOST | DIRTY_VR_MODE_CHANGED |
                DIRTY_QUIESCENT | DIRTY_DISPLAY_GROUP_WAKEFULNESS)) != 0) {
            if ((dirty & DIRTY_QUIESCENT) != 0) {
                if (areAllPowerGroupsReadyLocked()) {
                    sQuiescent = false;
                } else {
                    mDirty |= DIRTY_QUIESCENT;
                }
            }

            for (int idx = 0; idx < mPowerGroups.size(); idx++) {
                final PowerGroup powerGroup = mPowerGroups.valueAt(idx);
                final int groupId = powerGroup.getGroupId();

                // Determine appropriate screen brightness and auto-brightness adjustments.
                final boolean autoBrightness;  // 自动亮度是否开启
                final float screenBrightnessOverride;  // 是否有覆盖亮度
                if (!mBootCompleted) {
                    //启动过程中要求亮度稳定，要求默认亮度和Bootloader中设定的亮度值保持一致
                    autoBrightness = false;  //不开启自动亮度
                    screenBrightnessOverride = mScreenBrightnessDefault;  //使用默认值
                } else if (isValidBrightness(mScreenBrightnessOverrideFromWindowManager)) {
                	//使用WindowManager覆盖的亮度值
                    autoBrightness = false;
                    screenBrightnessOverride = mScreenBrightnessOverrideFromWindowManager;
                } else {
                    autoBrightness = (mScreenBrightnessModeSetting
                            == Settings.System.SCREEN_BRIGHTNESS_MODE_AUTOMATIC);
                    screenBrightnessOverride = PowerManager.BRIGHTNESS_INVALID_FLOAT;
                }
                //向DisplayMangerService发起请
                boolean ready = powerGroup.updateLocked(screenBrightnessOverride, autoBrightness,
                        shouldUseProximitySensorLocked(), shouldBoostScreenBrightness(),
                        mDozeScreenStateOverrideFromDreamManager,
                        mDozeScreenBrightnessOverrideFromDreamManagerFloat,
                        mDrawWakeLockOverrideFromSidekick,
                        mBatterySaverPolicy.getBatterySaverPolicy(ServiceType.SCREEN_BRIGHTNESS),
                        sQuiescent, mDozeAfterScreenOff, mIsVrModeEnabled, mBootCompleted,
                        mScreenBrightnessBoostInProgress, mRequestWaitForNegativeProximity);
                int wakefulness = powerGroup.getWakefulnessLocked();

                final boolean displayReadyStateChanged = powerGroup.setReadyLocked(ready);
                final boolean poweringOn = powerGroup.isPoweringOnLocked();
                if (ready && displayReadyStateChanged && poweringOn
                        && wakefulness == WAKEFULNESS_AWAKE) {
                    powerGroup.setIsPoweringOnLocked(false);
                    LatencyTracker.getInstance(mContext).onActionEnd(ACTION_TURN_ON_SCREEN);
                    Trace.asyncTraceEnd(Trace.TRACE_TAG_POWER, TRACE_SCREEN_ON, groupId);
                    final int latencyMs = (int) (mClock.uptimeMillis()
                            - powerGroup.getLastPowerOnTimeLocked());
                    // 如果亮屏流程超过200ms，输出亮屏所用时间
                    if (latencyMs >= SCREEN_ON_LATENCY_WARNING_MS) {
                        Slog.w(TAG, "Screen on took " + latencyMs + " ms");
                    }
                }
            }
            // 释放PSensor WakeLock锁时的一个标记
            mRequestWaitForNegativeProximity = false;
        }

        return areAllPowerGroupsReadyLocked() && !oldPowerGroupsReady;
    }
```

```java
    boolean updateLocked(float screenBrightnessOverride, boolean autoBrightness,
            boolean useProximitySensor, boolean boostScreenBrightness, int dozeScreenState,
            float dozeScreenBrightness, boolean overrideDrawWakeLock,
            PowerSaveState powerSaverState, boolean quiescent, boolean dozeAfterScreenOff,
            boolean vrModeEnabled, boolean bootCompleted, boolean screenBrightnessBoostInProgress,
            boolean waitForNegativeProximity) {
        // 根据系统唤醒状态获取请求的'策略'： off, doze, dim or bright.
        mDisplayPowerRequest.policy = getDesiredScreenPolicyLocked(quiescent, dozeAfterScreenOff,
                vrModeEnabled, bootCompleted, screenBrightnessBoostInProgress);
        // WindowManager覆盖的亮度值，如播放视频时调节亮度
        mDisplayPowerRequest.screenBrightnessOverride = screenBrightnessOverride;
        // 是否使用自动亮度
        mDisplayPowerRequest.useAutoBrightness = autoBrightness;
        // 是否存在PSensor Wakelock锁
        mDisplayPowerRequest.useProximitySensor = useProximitySensor;
        // 是否有增强亮度
        mDisplayPowerRequest.boostScreenBrightness = boostScreenBrightness;
		// 唤醒状态为Doze时，确定display的状态
        if (mDisplayPowerRequest.policy == DisplayPowerRequest.POLICY_DOZE) {
            mDisplayPowerRequest.dozeScreenState = dozeScreenState;
            if ((getWakeLockSummaryLocked() & WAKE_LOCK_DRAW) != 0 && !overrideDrawWakeLock) {
                if (mDisplayPowerRequest.dozeScreenState == Display.STATE_DOZE_SUSPEND) {
                    mDisplayPowerRequest.dozeScreenState = Display.STATE_DOZE;
                }
                if (mDisplayPowerRequest.dozeScreenState == Display.STATE_ON_SUSPEND) {
                    mDisplayPowerRequest.dozeScreenState = Display.STATE_ON;
                }
            }
            // Doze时的屏幕亮度
            mDisplayPowerRequest.dozeScreenBrightness = dozeScreenBrightness;
        } else {
            mDisplayPowerRequest.dozeScreenState = Display.STATE_UNKNOWN;
            mDisplayPowerRequest.dozeScreenBrightness = PowerManager.BRIGHTNESS_INVALID_FLOAT;
        }
        mDisplayPowerRequest.lowPowerMode = powerSaverState.batterySaverEnabled;
        mDisplayPowerRequest.screenLowPowerBrightnessFactor = powerSaverState.brightnessFactor;
        // 发起请求，返回值表示DisplayPowerController中是否完成这次请求
        boolean ready = mDisplayManagerInternal.requestPowerState(mGroupId, mDisplayPowerRequest,
                waitForNegativeProximity);
        mNotifier.onScreenPolicyUpdate(mGroupId, mDisplayPowerRequest.policy);
        return ready;
    }
```

在向DisplayManagerService发起请求时，会将所有的信息封装到DisplayPowerRequest对象中，其中，policy属性值有五类：

+   POLICY\_OFF：请求屏幕进入灭屏状态；
+   POLICY\_DOZE：请求屏幕进入Doze状态；
+   POLICY\_DIM：请求屏幕进入Dim状态，
+   POLICY\_BRIGHT：请求屏幕处于正常亮屏状态；
+   POLICY\_VR：VR模式相关；

在请求前，通过getDesiredScreenPolicyLocked()方法，根据当前唤醒状态和WakeLock统计状态来决定要请求的Display状态：

```java
    int getDesiredScreenPolicyLocked(boolean quiescent, boolean dozeAfterScreenOff,
            boolean vrModeEnabled, boolean bootCompleted, boolean screenBrightnessBoostInProgress) {
        final int wakefulness = getWakefulnessLocked();
        final int wakeLockSummary = getWakeLockSummaryLocked();
        // 当前唤醒状态为Asleep，则Display状态会设置为OFF
        if (wakefulness == WAKEFULNESS_ASLEEP || quiescent) {
            return DisplayPowerRequest.POLICY_OFF;
        } else if (wakefulness == WAKEFULNESS_DOZING) {  // 当前唤醒状态为Doze，则Display状态会设置为Doze指定的状态
            if ((wakeLockSummary & WAKE_LOCK_DOZE) != 0) {
                return DisplayPowerRequest.POLICY_DOZE;
            }
            // 表示跳过Doze的状态，直接设置成OFF
            if (dozeAfterScreenOff) {
                return DisplayPowerRequest.POLICY_OFF;
            }
            // Fall through and preserve the current screen policy if not configured to
            // doze after screen off.  This causes the screen off transition to be skipped.
        }

		//如果开启了VR模式则设置为VR模式
        if (vrModeEnabled) {
            return DisplayPowerRequest.POLICY_VR;
        }
		// 如果存在亮屏锁、用户活动状态为亮屏、进行增强亮度，则Display状态将设置为ON
        if ((wakeLockSummary & WAKE_LOCK_SCREEN_BRIGHT) != 0
                || !bootCompleted
                || (getUserActivitySummaryLocked() & USER_ACTIVITY_SCREEN_BRIGHT) != 0
                || screenBrightnessBoostInProgress) {
            return DisplayPowerRequest.POLICY_BRIGHT;
        }
		// 不满足以上条件，默认设置DIM
        return DisplayPowerRequest.POLICY_DIM;
    }
```

#### 2.10 updateDreamLocked()

该方法用来更新设备Dreamland状态，比如是否进入Dream、Doze或者开始休眠:

```java
// frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java

    private void updateDreamLocked(int dirty, boolean powerGroupBecameReady) {
        if ((dirty & (DIRTY_WAKEFULNESS
                | DIRTY_USER_ACTIVITY
                | DIRTY_ACTUAL_DISPLAY_POWER_STATE_UPDATED
                | DIRTY_ATTENTIVE
                | DIRTY_WAKE_LOCKS
                | DIRTY_BOOT_COMPLETED
                | DIRTY_SETTINGS
                | DIRTY_IS_POWERED
                | DIRTY_STAY_ON
                | DIRTY_PROXIMITY_POSITIVE
                | DIRTY_BATTERY_STATE)) != 0 || powerGroupBecameReady) {
            if (areAllPowerGroupsReadyLocked()) { //areAllPowerGroupsReadyLocked为ture后，才会进一步执行
            	//通过Handler异步发送一个消息
                scheduleSandmanLocked();
            }
        }
    }

    private boolean areAllPowerGroupsReadyLocked() {
        final int size = mPowerGroups.size();
        for (int i = 0; i < size; i++) {
            if (!mPowerGroups.valueAt(i).isReadyLocked()) {
                return false;
            }
        }

        return true;
    }
```

可以看到，对于Dreamland相关状态的更新，依赖areAllPowerGroupsReadyLocked的返回值，它表示每一个display是否已经准备就绪，因此只有在准备就绪的情况下才会进一步调用该方法的方法体。最后会PMS主线程中，调用handleSandman()方法执行Dreamland的操作:

```java
// frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java

    private void handleSandman(int groupId) { // runs on handler thread
        // Handle preconditions.
        final boolean startDreaming;
        final int wakefulness;
        synchronized (mLock) {
            mSandmanScheduled = false;

            final PowerGroup powerGroup = mPowerGroups.get(groupId);
            wakefulness = powerGroup.getWakefulnessLocked();
            // 如果召唤了"睡魔"，且每个Display状态已经准备完毕
            if ((wakefulness == WAKEFULNESS_DREAMING || wakefulness == WAKEFULNESS_DOZING) &&
                    powerGroup.isSandmanSummonedLocked() && powerGroup.isReadyLocked()) {
                // 判断是否可以进入Dreamland
                startDreaming = canDreamLocked(powerGroup) || canDozeLocked(powerGroup);
                powerGroup.setSandmanSummonedLocked(/* isSandmanSummoned= */ false);
            } else {
                startDreaming = false;
            }
        }

        // Start dreaming if needed.
        // We only control the dream on the handler thread, so we don't need to worry about
        // concurrent attempts to start or stop the dream.
        final boolean isDreaming;
        if (mDreamManager != null) {
            // Restart the dream whenever the sandman is summoned.
            if (startDreaming) {
                mDreamManager.stopDream(/* immediate= */ false);
                // 开始进入Dreamland
                mDreamManager.startDream(wakefulness == WAKEFULNESS_DOZING);
            }
            isDreaming = mDreamManager.isDreaming();
        } else {
            isDreaming = false;
        }

        // At this point, we either attempted to start the dream or no attempt will be made,
        // so stop holding the display suspend blocker for Doze.
        mDozeStartInProgress = false;

        // Update dream state.
        synchronized (mLock) {
			...

            // Remember the initial battery level when the dream started.
            // 记录下进入Dream模式前的初始电量值
            if (startDreaming && isDreaming) {
                mBatteryLevelWhenDreamStarted = mBatteryLevel;
                if (wakefulness == WAKEFULNESS_DOZING) {
                    Slog.i(TAG, "Dozing...");
                } else {
                    Slog.i(TAG, "Dreaming...");
                }
            }

            // If preconditions changed, wait for the next iteration to determine
            // whether the dream should continue (or be restarted).
            final PowerGroup powerGroup = mPowerGroups.get(groupId);
            if (powerGroup.isSandmanSummonedLocked()
                    || powerGroup.getWakefulnessLocked() != wakefulness) {
                return; // wait for next cycle
            }

            // Determine whether the dream should continue.
            long now = mClock.uptimeMillis();
            if (wakefulness == WAKEFULNESS_DREAMING) {
                if (isDreaming && canDreamLocked(powerGroup)) {
                    if (mDreamsBatteryLevelDrainCutoffConfig >= 0
                            && mBatteryLevel < mBatteryLevelWhenDreamStarted
                                    - mDreamsBatteryLevelDrainCutoffConfig
                            && !isBeingKeptAwakeLocked(powerGroup)) {
							//充电速度慢于消耗速度或者用户活动timeout已过则退出Dream进入睡眠
                    } else {
                        return; // continue dreaming 继续维持Dream状态
                    }
                }

                // Dream has ended or will be stopped.  Update the power state.
                // 退出Dreamland,进入休眠状态
                if (isItBedTimeYetLocked(powerGroup)) {
                    if (isAttentiveTimeoutExpired(powerGroup, now)) {
                    	//更新睡眠状态
                        sleepPowerGroupLocked(powerGroup, now,
                                PowerManager.GO_TO_SLEEP_REASON_TIMEOUT, Process.SYSTEM_UID);
                    } else {
                    	//更新doze状态
                        dozePowerGroupLocked(powerGroup, now,
                                PowerManager.GO_TO_SLEEP_REASON_TIMEOUT, Process.SYSTEM_UID);
                    }
                } else {
                	// 唤醒设备
                    wakePowerGroupLocked(powerGroup, now,
                            PowerManager.WAKE_REASON_DREAM_FINISHED,
                            "android.server.power:DREAM_FINISHED", Process.SYSTEM_UID,
                            mContext.getOpPackageName(), Process.SYSTEM_UID);
                }
            } else if (wakefulness == WAKEFULNESS_DOZING) {
                if (isDreaming) {
                    return; // continue dozing 继续保持doze状态
                }
                
                // Doze has ended or will be stopped.  Update the power state.
                // 更新睡眠状态
                sleepPowerGroupLocked(powerGroup, now,  PowerManager.GO_TO_SLEEP_REASON_TIMEOUT,
                        Process.SYSTEM_UID);
            }
        }

        // Stop dream.
        if (isDreaming) {
            mDreamManager.stopDream(/* immediate= */ false);
        }
    }
```

在以上方法中，将会调用DreamManager处理具体的Dreamland逻辑，这部分流程的分析，在DreamManagerService模块分析时会进行详细分析。

#### 2.11 finishWakefulnessChangeIfNeededLocked()

该方法主要做唤醒状态发生变化后，后半部分更新工作：

```java
//frameworks/base/services/core/java/com/android/server/power/PowerManagerService.java

    @GuardedBy("mLock")
    private void finishWakefulnessChangeIfNeededLocked() {
        if (mWakefulnessChanging && areAllPowerGroupsReadyLocked()) {
            if (getGlobalWakefulnessLocked() == WAKEFULNESS_DOZING
                    && (mWakeLockSummary & WAKE_LOCK_DOZE) == 0) {
                return; // wait until dream has enabled dozing
            } else {
                // Doze wakelock acquired (doze started) or device is no longer dozing.
                mDozeStartInProgress = false;
            }Android采码蜂:
            if (getGlobalWakefulnessLocked() == WAKEFULNESS_DOZING
                    || getGlobalWakefulnessLocked() == WAKEFULNESS_ASLEEP) {
                logSleepTimeoutRecapturedLocked();
            }
            mWakefulnessChanging = false;
            mNotifier.onWakefulnessChangeFinished();
        }
    }
```

只有当屏幕状态改变后，才会执行该方法。进入该方法后，将通过Notifier#onWakefulnessChangeFinished()方法发送亮屏、灭屏广播等。

> 该方法中的logScreenOn()方法将打印出整个亮屏流程的耗时，在平时处理问题时很有帮助。

#### 2.12 updateSuspendBlockerLocked()

这个方法用来更新SuspendBlocker锁状态。Suspend锁机制是Android框架中锁机的一种锁，它代表了框架层以上所有的WakeLock。Wakelock锁是APP或其他组建向PMS模块申请，而Suspend锁是PMS模块中对WakeLock锁的最终表现，或者说上层应用或system\_server其他组件申请了wakelock锁后，在PMS中最终都会表现为Suspend锁，通过Suspend锁向Hal层写入节点，Kernal层会读取节点，从而唤醒或者休眠CPU。  
该方法的详细分析在PMS WakeLock机制中进行汇总。  
到此为止，对PMS中所有核心方法进行了简单的分析，有些方法仅仅说明了下作用，会在后面具体业务中进行详细分析。

跟着学习自： [Android R PowerManagerService模块(1) 启动流程和核心方法](https://juejin.cn/post/6946581970960793613)