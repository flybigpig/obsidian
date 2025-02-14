大致流程：

1.  SystemServer启动CarServiceHelperService服务
2.  在调用startService后，CarServiceHelperService的onStart方法通过bindService的方式启动CarService（一个系统级别的APK，位于system/priv-app）
3.  启动CarService后首先调用onCreate，创建ICarImpl对象并初始化，在此时创建了一系列car相关的核心服务，并遍历init初始化
4.  然后调用onBind将该ICarImpl对象返回给CarServiceHelperService，CarServiceHelperService在内部的一个Binder对象ICarServiceHelperImpl传递给CarService，建立双向跨进程

### **序列图**

![](https://picx.zhimg.com/v2-7185c2bf212abc3811e1cf7bc2bb19cb_1440w.jpg)

### **启动CarServiceHelperService服务**

frameworks/base/services/java/com/android/server/SystemServer.java - run() —-> startOtherServices()

```
    private static final String CAR_SERVICE_HELPER_SERVICE_CLASS =
            "com.android.internal.car.CarServiceHelperService";
            ......
            if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE)) {
                traceBeginAndSlog("StartCarServiceHelperService");
                mSystemServiceManager.startService(CAR_SERVICE_HELPER_SERVICE_CLASS);
                traceEnd();
            }
```

—–> frameworks/base/services/core/java/com/android/server/SystemServiceManager.java - startService

```
    @SuppressWarnings("unchecked")
    public SystemService startService(String className) {
        ....
        return startService(serviceClass);
    }

    public <T extends SystemService> T startService(Class<T> serviceClass) {
        ...
        startService(service);
        ...
    }

    public void startService(@NonNull final SystemService service) {
        ......
        try {
            service.onStart();
            ...
        }
```

### **绑定carservice服务**

—–> frameworks/opt/car/services/src/com/android/internal/car/CarServiceHelperService.java - onStart()

```
    //这就是系统中和汽车相关的核心服务CarService，相关源代码在packages/services/Car/service目录下
    private static final String CAR_SERVICE_INTERFACE = "android.car.ICar";

    @Override
    public void onStart() {
        Intent intent = new Intent();
        intent.setPackage("com.android.car");  //绑定包名，设置广播仅对该包有效
        //绑定action，表明想要启动能够响应设置的这个action的活动，并在清单文件AndroidManifest.xml中设置action属性
        intent.setAction(CAR_SERVICE_INTERFACE);
        //绑定后回调
        if (!getContext().bindServiceAsUser(intent, mCarServiceConnection, Context.BIND_AUTO_CREATE,
                UserHandle.SYSTEM)) {
            Slog.wtf(TAG, "cannot start car service");
        }
        System.loadLibrary("car-framework-service-jni");
    }
```

-   service源码路径：packages/services/Car/service/AndroidManifest.xml

**sharedUserId是系统级别的，类似SystemUI，它编译出来同样是一个APK文件**

设备文件路径在： `/system/priv-app/CarService/CarService.apk`

```
//packages/services/Car/service/AndroidManifest.xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
        xmlns:androidprv="http://schemas.android.com/apk/prv/res/android"
        package="com.android.car"
        coreApp="true"
        android:sharedUserId="android.uid.system"> 
        ......
<application android:label="Car service"
                 android:directBootAware="true"
                 android:allowBackup="false"
                 android:persistent="true">
        <service android:name=".CarService"
                android:singleUser="true">
            <intent-filter>
                <action android:name="android.car.ICar" />
            </intent-filter>
        </service>
        <service android:name=".PerUserCarService" android:exported="false" />
    </application>
```

### **bindService[启动流程](https://zhida.zhihu.com/search?content_id=224627098&content_type=Article&match_order=2&q=%E5%90%AF%E5%8A%A8%E6%B5%81%E7%A8%8B&zhida_source=entity)**

```
context.bindService()  ——> onCreate()  ——> onBind()  ——> Service running  ——> onUnbind()  ——> onDestroy()  ——> Service stop
```

onBind()将返回给客户端一个IBind接口实例，IBind允许客户端回调服务的方法，比如得到Service的实例、运行状态或其他操作。这个时候把调用者（Context，例如Activity）会和Service绑定在一起，Context退出了，Srevice就会调用onUnbind->onDestroy相应退出。

所以调用bindService的生命周期为：`onCreate --> onBind(只一次，不可多次绑定) --> onUnbind --> onDestroy`

在Service每一次的开启关闭过程中，只有onStart可被多次调用(通过多次startService调用)，其他onCreate，onBind，onUnbind，onDestroy在一个生命周期中只能被调用一次

## **Car Service启动**

### **onCreate**

——–> packages/services/Car/service/src/com/android/car/CarService.java - onCreate()

创建ICarImpl实例

```
    @Nullable
    private static IVehicle getVehicle() {
        try {
            //该service启动文件hardware/interfaces/automotive/vehicle/2.0/default/android.hardware.automotive.vehicle@2.0-service.rc
            return android.hardware.automotive.vehicle.V2_0.IVehicle.getService();
        } ....
        return null;
    }

    @Override
    public void onCreate() {
        Log.i(CarLog.TAG_SERVICE, "Service onCreate");
        //获取hal层的Vehicle service
        mVehicle = getVehicle();

        //创建ICarImpl实例
        mICarImpl = new ICarImpl(this,
                mVehicle,
                SystemInterface.Builder.defaultSystemInterface(this).build(),
                mCanBusErrorNotifier,
                mVehicleInterfaceName);
        //然后调用ICarImpl的init初始化方法
        mICarImpl.init();
        //设置boot.car_service_created属性
        SystemProperties.set("boot.car_service_created", "1");

        linkToDeath(mVehicle, mVehicleDeathRecipient);
        //最后将该service注册到ServiceManager
        ServiceManager.addService("car_service", mICarImpl);
        super.onCreate();
    }
```

```
//packages/services/Car/service/src/com/android/car/ICarImpl.java
    private final VehicleHal mHal;
    //构造函数启动一大堆服务
    public ICarImpl(Context serviceContext, IVehicle vehicle, SystemInterface systemInterface,
            CanBusErrorNotifier errorNotifier, String vehicleInterfaceName) {
        mContext = serviceContext;
        mSystemInterface = systemInterface;
        //创建VehicleHal对象
        mHal = new VehicleHal(vehicle);
        mVehicleInterfaceName = vehicleInterfaceName;
        mSystemActivityMonitoringService = new SystemActivityMonitoringService(serviceContext);
        mCarPowerManagementService = new CarPowerManagementService(mContext, mHal.getPowerHal(),
                systemInterface);
        mCarPropertyService = new CarPropertyService(serviceContext, mHal.getPropertyHal());
        .....
        //InstrumentClusterService service启动
        mInstrumentClusterService = new InstrumentClusterService(serviceContext,
                mAppFocusService, mCarInputService);
        mSystemStateControllerService = new SystemStateControllerService(serviceContext,
                mCarPowerManagementService, mCarAudioService, this);
        mPerUserCarServiceHelper = new PerUserCarServiceHelper(serviceContext);
        // mCarBluetoothService = new CarBluetoothService(serviceContext, mCarPropertyService,
        //        mPerUserCarServiceHelper, mCarUXRestrictionsService);
        mVmsSubscriberService = new VmsSubscriberService(serviceContext, mHal.getVmsHal());
        mVmsPublisherService = new VmsPublisherService(serviceContext, mHal.getVmsHal());
        mCarDiagnosticService = new CarDiagnosticService(serviceContext, mHal.getDiagnosticHal());
        mCarStorageMonitoringService = new CarStorageMonitoringService(serviceContext,
                systemInterface);
        mCarConfigurationService =
                new CarConfigurationService(serviceContext, new JsonReaderImpl());
        mUserManagerHelper = new CarUserManagerHelper(serviceContext);

        //注意排序，service存在依赖
        List<CarServiceBase> allServices = new ArrayList<>();
        allServices.add(mSystemActivityMonitoringService);
        allServices.add(mCarPowerManagementService);
        allServices.add(mCarPropertyService);
        allServices.add(mCarDrivingStateService);
        allServices.add(mCarUXRestrictionsService);
        allServices.add(mCarPackageManagerService);
        allServices.add(mCarInputService);
        allServices.add(mCarLocationService);
        allServices.add(mGarageModeService);
        allServices.add(mAppFocusService);
        allServices.add(mCarAudioService);
        allServices.add(mCarNightService);
        allServices.add(mInstrumentClusterService);
        allServices.add(mCarProjectionService);
        allServices.add(mSystemStateControllerService);
        // allServices.add(mCarBluetoothService);
        allServices.add(mCarDiagnosticService);
        allServices.add(mPerUserCarServiceHelper);
        allServices.add(mCarStorageMonitoringService);
        allServices.add(mCarConfigurationService);
        allServices.add(mVmsSubscriberService);
        allServices.add(mVmsPublisherService);

        if (mUserManagerHelper.isHeadlessSystemUser()) {
            mCarUserService = new CarUserService(serviceContext, mUserManagerHelper);
            allServices.add(mCarUserService);
        }

        mAllServices = allServices.toArray(new CarServiceBase[allServices.size()]);
    }

    @MainThread
    void init() {
        traceBegin("VehicleHal.init");
        mHal.init();
        traceEnd();
        traceBegin("CarService.initAllServices");
        //启动的所有服务遍历调用init初始化（各个都继承了CarServiceBase）
        for (CarServiceBase service : mAllServices) {
            service.init();
        }
        traceEnd();
    }
```

### **onBind**

将上面onCreate创建的mICarImpl对象返回：

1.  onBind()回调方法会继续传递通过bindService()传递来的intent对象（即上面的`bindServiceAsUser`方法）
2.  onUnbind()会处理传递给unbindService()的intent对象。如果service允许绑定，onBind()会返回客户端与服务互相联系的通信句柄

```
//packages/services/Car/service/src/com/android/car/CarService.java
    @Override
    public IBinder onBind(Intent intent) {
        return mICarImpl;
    }
```

所以此处的mICarImpl会作为IBinder返回给`CarServiceHelperService.java - bindServiceAsUser`方法中的参数mCarServiceConnection（回调）

### **onDestroy**

释放mICarImpl创建的资源，包含一系列的服务：

```
    @Override
    public void onDestroy() {
        Log.i(CarLog.TAG_SERVICE, "Service onDestroy");
        mICarImpl.release();
        mCanBusErrorNotifier.removeFailureReport(this);

        if (mVehicle != null) {
            try {
                mVehicle.unlinkToDeath(mVehicleDeathRecipient);
                mVehicle = null;
            } catch (RemoteException e) {
                // Ignore errors on shutdown path.
            }
        }

        super.onDestroy();
    }
```

## **回调ServiceConnection**

> ICarImpl初始化完毕，会作为IBinder返回给`CarServiceHelperService.java - bindServiceAsUser`方法中绑定此服务的mCarServiceConnection（回调）

mCarServiceConnection初始化如下：

1.  其中返回的ICarImpl被保存在了CarServiceHelperService的mCarService
2.  mCarService.transact跨进程通信，调用ICar.aidl中定义的第一个方法setCarServiceHelper

```
//frameworks/opt/car/services/src/com/android/internal/car/CarServiceHelperService.java
private static final String CAR_SERVICE_INTERFACE = "android.car.ICar";
private IBinder mCarService;
private final ICarServiceHelperImpl mHelper = new ICarServiceHelperImpl();

private final ServiceConnection mCarServiceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
            Slog.i(TAG, "**CarService connected**");
            //1. 返回的ICarImpl被保存在了CarServiceHelperService的mCarService
            mCarService = iBinder;
            // Cannot depend on ICar which is defined in CarService, so handle binder call directly
            // instead. 
            // void setCarServiceHelper(in IBinder helper)
            Parcel data = Parcel.obtain();
            data.writeInterfaceToken(CAR_SERVICE_INTERFACE);
            //将ICarServiceHelperImpl类型的对象作为数据跨进程传递
            data.writeStrongBinder(mHelper.asBinder());
            try {
                //2.跨进程传输
                //对端是mCarService即ICarImpl，调用binder的transact进行跨进程通信
                //其code代表需要调用的对端方法，data为携带的传输数据
                //FIRST_CALL_TRANSACTION  = 0x00000001，即调用对端ICar.aidl中定义的第一个方法setCarServiceHelper
                mCarService.transact(IBinder.FIRST_CALL_TRANSACTION, // setCarServiceHelper
                        data, null, Binder.FLAG_ONEWAY);
            } catch (RemoteException e) {
                Slog.w(TAG, "RemoteException from car service", e);
                handleCarServiceCrash();
            }
        }

        @Override 
        public void onServiceDisconnected(ComponentName componentName) {
            handleCarServiceCrash();
        }
    };
```

## **跨进程setCarServiceHelper**

```
    @Override
    public void setCarServiceHelper(IBinder helper) {
        int uid = Binder.getCallingUid();
        if (uid != Process.SYSTEM_UID) {
            throw new SecurityException("Only allowed from system");
        }
        synchronized (this) {
            //将ICarServiceHelper的代理端保存在ICarImpl内部mICarServiceHelper
            mICarServiceHelper = ICarServiceHelper.Stub.asInterface(helper);
            //同时也传给了SystemInterface
            //此时他们有能力跨进程访问CarServiceHelperService
            mSystemInterface.setCarServiceHelper(mICarServiceHelper);
        }
    }
```

![](https://pic1.zhimg.com/v2-420987722604a78319b50e4e59c300de_1440w.jpg)

以上就是车载技术中的carservice启动流程的解析；更多车载技术可以参考**[《车载技术手册》](https://link.zhihu.com/?target=https%3A//docs.qq.com/doc/DUldvclB5d0JZSVFn)**有BYD高级工程师整理提供；里面有着大部分车载开发技术的讲解。