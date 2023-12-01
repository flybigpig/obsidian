# AWS

AMS是Android中最核心的服务，主要负责系统中四大组件的启动、切换、调度及应用进程的管理和调度等工作，其职责与操作系统中的进程管理和调度模块相类似，因此它在Android中非常重要。

ActivityManagerService extends ActivityManagerNative implements Watchdog.Monitor, BatteryStatsImpl.BatteryCallback

客户端使用ActivityManager类。由于AMS是系统核心服务，很多API不能开放供客户端使用，所以设计者没有让ActivityManager直接加入AMS家族。在ActivityManager类内部通过调用AMN的getDefault函数得到一个ActivityManagerProxy对象，通过它可与AMS通信。



ATMS 即 ActivityTaskManagerService，用于管理 Activity 及其容器（任务、堆栈、显示等）。ATMS 在 Android 10 中才出现，由原来的 AMS（ActivityManagerService）分离而来，承担了 AMS 的部分职责。因此，在 AMS初始化过程中（AMS启动流程），也伴随着了 ATMS 的初始化。本文主要介绍 ATMS 的启动流程和初始化过程。



```
SystemServer.run();

private void run() {
	try {
		...
		// 创建Looper
		Looper.prepareMainLooper();
		// 加载libandroid_servers.so
		System.loadLibrary("android_servers");
		// 创建系统的 Context：ContextImpl.createSystemContext(new ActivityThread())
		createSystemContext();
		// 创建 SystemServiceManager
		mSystemServiceManager = new SystemServiceManager(mSystemContext);
		LocalServices.addService(SystemServiceManager.class, mSystemServiceManager);
		...
	}
	...
	try {
		//启动引导服务，ActivityManagerService、ActivityTaskManagerService、PackageManagerService、PowerManagerService、DisplayManagerService 等
		startBootstrapServices(); 
		//启动核心服务，BatteryService、UsageStatusService 等
		startCoreServices(); 
		//启动其他服务，InputManagerService、WindowManagerService、CameraService、AlarmManagerService 等
		startOtherServices(); 
		...
	}
	...
	// 开启消息循环
	Looper.loop();
}
```



ATMS初始化

```
public ActivityTaskManagerService(Context context) {
	mContext = context;
	...
	mSystemThread = ActivityThread.currentActivityThread();
	mUiContext = mSystemThread.getSystemUiContext(); //ContextImpl.createSystemUiContext(getSystemContext())
	mLifecycleManager = new ClientLifecycleManager();
	mInternal = new LocalService(); //ActivityTaskManagerInternal 的子类
	...
}
```



###### initialize()

```
public void initialize(IntentFirewall intentFirewall, PendingIntentController intentController, Looper looper) {
	mH = new H(looper);
	mUiHandler = new UiHandler();
	mIntentFirewall = intentFirewall;
	...
	mPendingIntentController = intentController;
	mTempConfig.setToDefaults(); //定义时即被创建：mTempConfig = new Configuration()
	...
	//new ActivityStackSupervisor(this, mH.getLooper())
	mStackSupervisor = createStackSupervisor(); 
	mRootActivityContainer = new RootActivityContainer(this);
	mRootActivityContainer.onConfigurationChanged(mTempConfig);
	...
	mLockTaskController = new LockTaskController(mContext, mStackSupervisor, mH);
	mActivityStartController = new ActivityStartController(this);
	mRecentTasks = createRecentTasks(); //new RecentTasks(this, mStackSupervisor)
	mStackSupervisor.setRecentTasks(mRecentTasks);
	...
}
```

