本篇基于AOSP11代码分析`CarService`相关启动流程，和汽车相关的服务的启动主要依靠一个系统服务`CarServiceHelperService`开机时在`SystemServer`中启动：

### CarServiceHelperService启动

```
private static final String CAR_SERVICE_HELPER_SERVICE_CLASS =
            "com.android.internal.car.CarServiceHelperService";
            
private void startOtherServices(@NonNull TimingsTraceAndSlog t) {
......

if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE)) {
                t.traceBegin("StartCarServiceHelperService");
                mSystemServiceManager.startService(CAR_SERVICE_HELPER_SERVICE_CLASS);
                t.traceEnd();
            }
     
     ......
     
     }
```

`CarServiceHelperService`被定义在frameworks/opt/car/下，它和其他系统服务一样，属于`SystemService`的子类，通过`SystemServiceManager.startService`启动：

### SystemServiceManager.startService

```
public SystemService startService(String className) {
        final Class<SystemService> serviceClass = loadClassFromLoader(className,
                this.getClass().getClassLoader());
        return startService(serviceClass);
    }
    private static Class<SystemService> loadClassFromLoader(String className,
            ClassLoader classLoader) {
        try {
            return (Class<SystemService>) Class.forName(className, true, classLoader);
        } catch (ClassNotFoundException ex) {
            ...
        }
    }
```

`loadClassFromLoader`通过类加载器直接获取`CarServiceHelperService`的class对象，拿到class对象进而再调用`startService`重载方法：

```
    public <T extends SystemService> T startService(Class<T> serviceClass) {
        try {
            final String name = serviceClass.getName();
           

            // Create the service.
            if (!SystemService.class.isAssignableFrom(serviceClass)) {
                throw new RuntimeException("Failed to create " + name
                        + ": service must extend " + SystemService.class.getName());
            }
            final T service;
            try {
                Constructor<T> constructor = serviceClass.getConstructor(Context.class);
                service = constructor.newInstance(mContext);
            } catch (InstantiationException ex) {
                ...
            } catch (IllegalAccessException ex) {
                ...
            } catch (NoSuchMethodException ex) {
                ...
            } catch (InvocationTargetException ex) {
               ...
            }

            startService(service);
            return service;
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
        }
    }
```

接着通过反射构造`CarServiceHelperService`的实例对象，然后再调用`startService`重载方法：

```
 public void startService(@NonNull final SystemService service) {
        // Register it.
        mServices.add(service);
        // Start it.
        long time = SystemClock.elapsedRealtime();
        try {
            service.onStart();
        } catch (RuntimeException ex) {
            throw new RuntimeException("Failed to start service " + service.getClass().getName()
                    + ": onStart threw an exception", ex);
        }
        warnIfTooLong(SystemClock.elapsedRealtime() - time, service, "onStart");
    }
```

这里先将`CarServiceHelperService`保存到`mServices`这个list中，然后调用`CarServiceHelperService`的`onStart`方法正式启动此服务。

### CarServiceHelperService.onStart

```
 @Override
    public void onStart() {
        EventLog.writeEvent(EventLogTags.CAR_HELPER_START, mHalEnabled ? 1 : 0);

        IntentFilter filter = new IntentFilter(Intent.ACTION_REBOOT);
        filter.addAction(Intent.ACTION_SHUTDOWN);
        mContext.registerReceiverForAllUsers(mShutdownEventReceiver, filter, null, null);
        mCarWatchdogDaemonHelper.addOnConnectionChangeListener(mConnectionListener);
        mCarWatchdogDaemonHelper.connect();
        Intent intent = new Intent();
        intent.setPackage("com.android.car");
        intent.setAction(ICarConstants.CAR_SERVICE_INTERFACE);
        if (!mContext.bindServiceAsUser(intent, mCarServiceConnection, Context.BIND_AUTO_CREATE,
                UserHandle.SYSTEM)) {
            Slog.wtf(TAG, "cannot start car service");
        }
        loadNativeLibrary();
    }

```

这里首先注册了开关机广播，`CarWatchdogDaemonHelper`用于监控此服务，接着会绑定一个包名为`"com.android.car"`，Action为`"android.car.ICar"`的服务，这就是系统中和汽车相关的核心服务`CarService`，相关源代码在packages/services/Car/service目录下，然后我们先去看看`CarService`，等下再回头来看绑定此服务之后的`mCarServiceConnection`回调部分。

如下是`CarService`的AndroidManifest部分截图，可以看到`CarService`的`sharedUserId`是系统级别的，这是一个系统级服务，类似`SystemUI`，它编译出来同样是一个APK文件。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/45d5772ab3b34ca07429506af4ed2a38.png)来具体看`CarService`，它的`onStartCommand`没什么东西，主要来看`onBind`：

### CarService.onBind

```
@Override
    public IBinder onBind(Intent intent) {
        return mICarImpl;
    }
```

`ICarImpl`是一个Binder服务端，其顶级接口为`ICar`，在`onCreate`中初始化：

### CarService.onCreate

```
 @Override
    public void onCreate() {
        //通知用户有关 CAN 总线故障 
        mCanBusErrorNotifier = new CanBusErrorNotifier(this /* context */);
        //获取Vehicle hal的client端
        mVehicle = getVehicle();
        
        if (mVehicle == null) {
            throw new IllegalStateException("Vehicle HAL service is not available.");
        }
        try {
            mVehicleInterfaceName = mVehicle.interfaceDescriptor();
        } catch (RemoteException e) {
             ...
        }
      
        //实例化ICarImpl
        mICarImpl = new ICarImpl(this,
                mVehicle,
                SystemInterface.Builder.defaultSystemInterface(this).build(),
                mCanBusErrorNotifier,
                mVehicleInterfaceName);
        //初始化
        mICarImpl.init();
        //Vehicle hal对端死亡回调
        linkToDeath(mVehicle, mVehicleDeathRecipient);
        //将mICarImpl注册到ServiceManager
        ServiceManager.addService("car_service", mICarImpl);
        //修改boot.car_service_created属性为1
        SystemProperties.set("boot.car_service_created", "1");
        super.onCreate();
    }
```

此方法中主要会对`ICarImpl`实例化，之后`init`进行初始化，最后将其注册到`ServiceManager`。

### ICarImpl构造方法

```
ICarImpl(Context serviceContext, IVehicle vehicle, SystemInterface systemInterface,
            CanBusErrorNotifier errorNotifier, String vehicleInterfaceName,
            @Nullable CarUserService carUserService,
            @Nullable CarWatchdogService carWatchdogService) {
......
mPerUserCarServiceHelper = new PerUserCarServiceHelper(serviceContext, mCarUserService);
        mCarBluetoothService = new CarBluetoothService(serviceContext, mPerUserCarServiceHelper);
        mCarInputService = new CarInputService(serviceContext, mHal.getInputHal(), mCarUserService);
        mCarProjectionService = new CarProjectionService(
                serviceContext, null /* handler */, mCarInputService, mCarBluetoothService);
        mGarageModeService = new GarageModeService(mContext);
        mAppFocusService = new AppFocusService(serviceContext, mSystemActivityMonitoringService);
        mCarAudioService = new CarAudioService(serviceContext);
        mCarNightService = new CarNightService(serviceContext, mCarPropertyService);
        mFixedActivityService = new FixedActivityService(serviceContext);
        mInstrumentClusterService = new InstrumentClusterService(serviceContext,
                mAppFocusService, mCarInputService);
        mSystemStateControllerService = new SystemStateControllerService(
                serviceContext, mCarAudioService, this);
        mCarStatsService = new CarStatsService(serviceContext);
        mCarStatsService.init();
        if (mFeatureController.isFeatureEnabled(Car.VEHICLE_MAP_SERVICE)) {
            mVmsBrokerService = new VmsBrokerService(mContext, mCarStatsService);
        } else {
            mVmsBrokerService = null;
        }
        if (mFeatureController.isFeatureEnabled(Car.DIAGNOSTIC_SERVICE)) {
            mCarDiagnosticService = new CarDiagnosticService(serviceContext,
                    mHal.getDiagnosticHal());
        } else {
            mCarDiagnosticService = null;
        }
        if (mFeatureController.isFeatureEnabled(Car.STORAGE_MONITORING_SERVICE)) {
            mCarStorageMonitoringService = new CarStorageMonitoringService(serviceContext,
                    systemInterface);
        } else {
            mCarStorageMonitoringService = null;
        }
        mCarConfigurationService =
                new CarConfigurationService(serviceContext, new JsonReaderImpl());
        mCarLocationService = new CarLocationService(serviceContext);
        mCarTrustedDeviceService = new CarTrustedDeviceService(serviceContext);
        mCarMediaService = new CarMediaService(serviceContext, mCarUserService);
        mCarBugreportManagerService = new CarBugreportManagerService(serviceContext);
......
 CarLocalServices.addService(CarPowerManagementService.class, mCarPowerManagementService);
        CarLocalServices.addService(CarPropertyService.class, mCarPropertyService);
        CarLocalServices.addService(CarUserService.class, mCarUserService);
        CarLocalServices.addService(CarTrustedDeviceService.class, mCarTrustedDeviceService);
        CarLocalServices.addService(CarUserNoticeService.class, mCarUserNoticeService);
        CarLocalServices.addService(SystemInterface.class, mSystemInterface);
        CarLocalServices.addService(CarDrivingStateService.class, mCarDrivingStateService);
        CarLocalServices.addService(PerUserCarServiceHelper.class, mPerUserCarServiceHelper);
        CarLocalServices.addService(FixedActivityService.class, mFixedActivityService);
        CarLocalServices.addService(VmsBrokerService.class, mVmsBrokerService);
        .....
        List<CarServiceBase> allServices = new ArrayList<>();
        allServices.add(mFeatureController);
        allServices.add(mCarUserService);
        allServices.add(mSystemActivityMonitoringService);
        allServices.add(mCarPowerManagementService);
        allServices.add(mCarPropertyService);
        allServices.add(mCarDrivingStateService);
        ...
        mAllServices = allServices.toArray(new CarServiceBase[allServices.size()]);
}
```

这里省略了和`Vehicle hal`有关的初始化和分析，后续文章再看。  
`ICarImpl`构造方法中创建了一系列`CarService`模块下的服务，这些服务有部分被添加到了`CarLocalServices`内部，其提供了`getService`静态方法用于直接获取这些服务，所有服务都被保存在`ICarImpl`内部的`CarServiceBase`类型数组`mAllServices`中（所有服务都是`CarServiceBase`的子类）。

`ICarImpl`构造方法完了之后会接着调用其`init`方法:

### ICarImpl.init

```
 @MainThread
    void init() {
        mBootTiming = new TimingsTraceLog(VHAL_TIMING_TAG, Trace.TRACE_TAG_HAL);
        traceBegin("VehicleHal.init");
        //hal初始化
        //...省略
        traceEnd();
        traceBegin("CarService.initAllServices");
        for (CarServiceBase service : mAllServices) {
            service.init();
        }
        traceEnd();
    }
```

此方法很简单，遍历`mAllServices`，分别执行所有服务的`init`，各自初始化，有兴趣的可以自己去研究各个服务。

到此`ICarImpl`初始化完毕，最后会作为binder返回给绑定此服务的`mCarServiceConnection`：

```
private final ServiceConnection mCarServiceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
            if (DBG) {
                Slog.d(TAG, "onServiceConnected:" + iBinder);
            }
            handleCarServiceConnection(iBinder);
        }

        @Override
        public void onServiceDisconnected(ComponentName componentName) {
            handleCarServiceCrash();
        }
    };
```

### CarServiceHelperService.handleCarServiceConnection

```
   void handleCarServiceConnection(IBinder iBinder) {
        ...
        synchronized (mLock) {
            if (mCarService == iBinder) {
                return; // already connected.
            }
        
            mCarService = iBinder;
          ...

        sendSetCarServiceHelperBinderCall();
         ......
         
        }
    }
```

这个方法我们主要关注上面部分，返回的`ICarImpl`被保存在了`CarServiceHelperService`的`mCarService`，后续可通过`mCarService`跨进程通信。

### CarServiceHelperService.sendSetCarServiceHelperBinderCall

```
   private void sendSetCarServiceHelperBinderCall() {
        Parcel data = Parcel.obtain();
        data.writeInterfaceToken(ICarConstants.CAR_SERVICE_INTERFACE);
        data.writeStrongBinder(mHelper.asBinder());
        // void setCarServiceHelper(in IBinder helper)
        sendBinderCallToCarService(data, ICarConstants.ICAR_CALL_SET_CAR_SERVICE_HELPER);
    }
```

这里将会进行跨进程通信，首先构造传输数据，`ICarConstants`是定义在`ExternalConstants`的静态内部类：

```
     static final class ICarConstants {
        ....
        static final String CAR_SERVICE_INTERFACE = "android.car.ICar";
        static final int ICAR_CALL_SET_CAR_SERVICE_HELPER = 0;
        ....
    }
```

`CAR_SERVICE_INTERFACE`用来标识远程服务接口，其具体传输数据是一个Binder对象，我们来看看`mHelper`是什么？

```

    private final ICarServiceHelperImpl mHelper = new ICarServiceHelperImpl();

```

`mHelper`是定义在`CarServiceHelperService`的内部类，是一个Binder对象：

```
private class ICarServiceHelperImpl extends ICarServiceHelper.Stub {
......
.....
}
```

### CarServiceHelperService.sendBinderCallToCarService

再回到前面看`sendBinderCallToCarService`方法：

```
private void sendBinderCallToCarService(Parcel data, int callNumber) {
        // Cannot depend on ICar which is defined in CarService, so handle binder call directly
        // instead.
        IBinder carService;
        synchronized (mLock) {
            carService = mCarService;
        }
        if (carService == null) {
            Slog.w(TAG, "Not calling txn " + callNumber + " because service is not bound yet",
                    new Exception());
            return;
        }
        int code = IBinder.FIRST_CALL_TRANSACTION + callNumber;
        try {
            
            carService.transact(code, data, null, Binder.FLAG_ONEWAY);
            
        } catch (RemoteException e) {
           
            handleCarServiceCrash();
        } catch (RuntimeException e) {
           
            throw e;
        } finally {
            data.recycle();
        }
    }
```

这个方法很明显就是跨进程传输的具体实现了，对端是`mCarService`即`ICarImpl`，调用binder的`transact`进行跨进程通信，其code代表需要调用的对端方法，data为携带的传输数据，`ICAR_CALL_SET_CAR_SERVICE_HELPER`等于0，这里调用的是对端的0号方法。

于是我们来看看`ICar.aidl`中定义的0号方法：

```
interface ICar {
    .....
    oneway void setCarServiceHelper(in IBinder helper) = 0;
    ......
  }
```

### ICarImpl.setCarServiceHelper

接着来看`setCarServiceHelper`具体实现：

```
 @Override
    public void setCarServiceHelper(IBinder helper) {
        //权限检查
        assertCallingFromSystemProcess();
        ICarServiceHelper carServiceHelper = ICarServiceHelper.Stub.asInterface(helper);
        synchronized (mLock) {
            mICarServiceHelper = carServiceHelper;
        }
        mSystemInterface.setCarServiceHelper(carServiceHelper);
        mCarOccupantZoneService.setCarServiceHelper(carServiceHelper);
    }
```

这里将`ICarServiceHelper`的代理端保存在`ICarImpl`内部`mICarServiceHelper`，同时也传给了`SystemInterface`和`CarOccupantZoneService`，我们暂时不需要知道这三个类拿到`ICarServiceHelper`的代理端的具体用处，只需要知道他们有能力跨进程访问`CarServiceHelperService`就行了。

到此`CarService`的启动流程我们大致看完了，总结一下：

1.  首先`CarService`是一个系统级别的服务APK，类似`SystemUI`，其在开机时由`SystemServer`通过`CarServiceHelperService`启动。
2.  `CarServiceHelperService`通过绑定服务的方式启动`CarService`，启动之后创建了一个Binder对象`ICarImpl`，并通过onBind返回给`system_server`进程。
3.  `ICarImpl`构造方法中创建了一系列和汽车相关的核心服务，并依次启动这些服务即调用各自`init`方法。
4.  `ICarImpl`返回给`CarServiceHelperService`之后，`CarServiceHelperService`也将其内部的一个Binder对象（`ICarServiceHelperImpl`）传递到了`CarService`进程，自此`CarService`和`system_server`两个进程建立了双向Binder通信。

总体来看`CarService`的启动比较简单。