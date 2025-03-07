## 1 车载启动流程

        车载Android启动流程基本是在Android系统的启动流程中，**多了Car相关服务**。其他流程基本一致，下面我们来看一下Android系统的启动流程。

### 1.1 Android系统启动流程

        Android系统的启动，从设备的开机键长按开始到Android桌面展示，这个完整流程就是Android系统启动的流程。从系统层次角度可分为Linux 系统层、Android 系统服务层、Zygote进程模型三个阶段；从开机到启动Android桌面完成具体细节可分为**Android系统启动的七个步骤**。下面我们来分析一下：

![](https://i-blog.csdnimg.cn/blog_migrate/f9343302e891b18f72cf819e68e350e8.png)

#### 1.1.1 启动电源启动系统

        触当电源按下时引导芯片从预定义的地方（固化在ROM）开始执行。加载引导程序BootLoader到RAM中，然后执行。

#### 1.1.2 启动BootLoader引导程序

        引导程序BootLoader是在Android操作系统开始运行前的一个小程序，它的主要作用是将AndroidOS拉起来。

#### 1.1.3 启动linux内核

         当内核启动时，设置缓存、被保护存储器、计划列表、加载驱动。当内核完成系统设置后，它首先会在系统文件中寻找init.rc文件，并启动init.rc进程。

#### 1.1.4 启动init进程

        **init进程启动做了很多工作，但是总的来说主要就是做了一下三件事：**

        **（1）创建和挂载启动所需文件目录。** 

        **（2）初始化和启动系统属性服务。**

        **（3）解析init.rc配置文件并启动Zygote进程。**

#### 1.1.5 启动zygote进程孵化器

        **程序上app\_process进程启动zygote进程。zygote主要创建Java虚拟机并为Java虚拟机注册JNI方法；创建服务端Socket；预加载类和资源；启动SystemServer进程；等待AMS请求创建新的应用进程。**

#### 1.1.6 启动systemServer进程

        **创建并启动Binder线程池，这样可以和其他进程进行通信。**

        **创建SystemServiceManager，其用于对系统的服务进行创建、启动和生命周期的管理。**

        **启动系统中的各种服务。包括我们熟悉的AMS、PMS、WMS。**

#### 1.1.7 启动Launcher

        被SystemServer启动的AMS会启动Launcher，Launcher启动后会将已安装应用的图标显示在桌面上。

![](https://i-blog.csdnimg.cn/blog_migrate/2bdcde15b426f1bda6731851a1e21cbf.png)

### 1.2 车载Android启动的区别

        车载Android启动是在前面1.1Android系统启动流程中SystemServer开始，有区别。**车载Android在SystemServer中启动独有的CarService**。下图虚线部分：

![](https://i-blog.csdnimg.cn/blog_migrate/24a27e04f0354541f8f1b59d8f81c2a3.png)

## 2 车载CarService启动

         **CarService是车载Android系统的核心服务之一，所有应用都需要通过CarService来查询、控制整车的状态。不仅仅是车辆控制，实际上CarService几乎就是整个车载Framework最核心的组件。提供了一系列的服务与HAL层的VehicleHAL通信，进而通过车载总线(例如CAN总线)与车身进行通讯，同时它们还为应用层的APP提供接口，从而让APP能够实现对车身的控制与状态的显示。**

        **CarService启动流程和汽车相关的服务的启动主要依靠一个系统服务CarServiceHelperService开机时在SystemServer中启动。**

###  2.1 CarServiceHelperService启动

         **SystemServer进程**启动后会调用**main()->run()->startOtherService()**方法，通过判断当前系统是否是车载分支的版本，是则创建CarServiceHelperService。

```java
package com.android.server;

public final class SystemServer {

    private void startOtherServices() {
        if (
            mPackageManager.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE)
        ) {
            traceBeginAndSlog("StartCarServiceHelperService");
            mSystemServiceManager.startService(
                CAR_SERVICE_HELPER_SERVICE_CLASS
            );
            traceEnd();
        }
    }
}

```

         SystemServer中使用Service管理代理SystemServiceManager的startService方法**通过反射构建com.android.internal.car.CarServiceHelperService**。CarServiceHelperService也是继承自SystemService，初始化成功后调用onStart()方法启动。

```java
public class SystemServiceManager {

    @SuppressWarnings("unchecked")
    public SystemService startService(String className) {
        final Class<SystemService> serviceClass;
        try {
            serviceClass = (Class<SystemService>) Class.forName(className);
        } catch (ClassNotFoundException ex) {
            Slog.i(TAG, "Starting " + className);
            throw new RuntimeException(
                "Failed to create service " +
                className +
                ": service class not found, usually indicates that the caller should " +
                "have called PackageManager.hasSystemFeature() to check whether the " +
                "feature is available on this device before trying to start the " +
                "services that implement it",
                ex
            );
        }
        return startService(serviceClass);
    }

    public <T extends SystemService> T startService(Class<T> serviceClass) {
        try {
            startService(service);
            return service;
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
        }
    }

    public void startService(@NonNull final SystemService service) {
        mServices.add(service);
        long time = SystemClock.elapsedRealtime();
        try {
            service.onStart();
        } catch (RuntimeException ex) {
            throw new RuntimeException(
                "Failed to start service " +
                service.getClass().getName() +
                ": onStart threw an exception",
                ex
            );
        }
        warnIfTooLong(SystemClock.elapsedRealtime() - time, service, "onStart");
    }
}

```

### 2.2 车载CarService进程启动        

        **onStart()方法中使用AIDL启动CarService（新的进程），并加载jni库为CarService提供必要的API。CarServiceHelperService重写后的onStart()需要重点看一下：**

```java
package com.android.internal.car;

public class CarServiceHelperService extends SystemService {

    private static final String CAR_SERVICE_INTERFACE = "android.car.ICar";

    @Overridepublic
    void onStart() {
        Intent intent = new Intent();
        intent.setPackage("com.android.car");
        intent.setAction(CAR_SERVICE_INTERFACE);
        if (
            !getContext()
                .bindServiceAsUser(
                    intent,
                    mCarServiceConnection,
                    Context.BIND_AUTO_CREATE,
                    UserHandle.SYSTEM
                )
        ) {
            Slog.wtf(TAG, "cannot start car service");
        }
        System.loadLibrary("car-framework-service-jni");
    }
}

```

### 2.3 CarServer启动图 

大致流程分为以下几步：

        **1.SystemServer初始化时候调用startOtherService()。**

        **2.startOtherService()方法通过SystemServerManager对象的startService()启动CarServiceHelperService，并调用其onStart()方法。**

        **3.CarServiceHelperService通过bindServiceAsUser()方法启动CarService**

        **4.CarService被创建后，onCreate方法调用进行初始化当前对象。**

![](https://i-blog.csdnimg.cn/blog_migrate/70feea2f70f56736a2dbdade16ea7371.png)

##  3 车载CarService内部实现

![](https://i-blog.csdnimg.cn/blog_migrate/1dc6366eaca3b1a4fc7bf85ca7c733c8.png)

CarService进入启动时序后，onCreate()方法中进行一系列的自身的初始化操作，步骤如下：

        1）通过HIDL接口获取到HAL层的IHwBinder对象-IVehicle，与AIDL的用法类似，必须持有IHwBinder对象我们才可以与Vehicle HAL层进行通信。

        2）创建ICarImpl对象，并调用init方法，它就是ICar.aidl接口的实现类，我们需要通过它才能拿到其他的Service的IBinder对象。

        3）将ICar.aidl的实现类添加到ServiceManager中。

        4）设定SystemProperty，将CarService设定为创建完成状态，只有包含CarService在内的所有的核心Service都完成初始化，才能结束开机动画并发送开机广播。

```java
@Overridepublic
void onCreate() {
    Log.i(CarLog.TAG_SERVICE, "Service onCreate");
    mCanBusErrorNotifier = new CanBusErrorNotifier(this);
    mVehicle = getVehicle();
    EventLog.writeEvent(
        EventLogTags.CAR_SERVICE_CREATE,
        mVehicle == null ? 0 : 1
    );
    if (mVehicle == null) {
        throw new IllegalStateException(
            "Vehicle HAL service is not available."
        );
    }
    try {
        mVehicleInterfaceName = mVehicle.interfaceDescriptor();
    } catch (RemoteException e) {
        throw new IllegalStateException(
            "Unable to get Vehicle HAL interface descriptor",
            e
        );
    }
    Log.i(CarLog.TAG_SERVICE, "Connected to " + mVehicleInterfaceName);
    EventLog.writeEvent(
        EventLogTags.CAR_SERVICE_CONNECTED,
        mVehicleInterfaceName
    );
    mICarImpl = new ICarImpl(
        this,
        mVehicle,
        SystemInterface.Builder.defaultSystemInterface(this).build(),
        mCanBusErrorNotifier,
        mVehicleInterfaceName
    );
    mICarImpl.init();
    linkToDeath(mVehicle, mVehicleDeathRecipient);
    ServiceManager.addService("car_service", mICarImpl);
    SystemProperties.set("boot.car_service_created", "1");
    super.onCreate();
}

```

### 3.1 IVehicle对象创建

        通过HIDL接口获取到HAL层的IHwBinder对象-IVehicle，与AIDL的用法类似，必须持有IHwBinder对象我们才可以与Vehicle HAL层进行通信。

```java
@Nullableprivate
static IVehicle getVehicle() {
    final String instanceName = SystemProperties.get(
        "ro.vehicle.hal",
        "default"
    );
    try {
        return android.hardware.automotive.vehicle.V2_0.IVehicle.getService(
            instanceName
        );
    } catch (RemoteException e) {
        Log.e(
            CarLog.TAG_SERVICE,
            "Failed to get IVehicle/" + instanceName + " service",
            e
        );
    } catch (NoSuchElementException e) {
        Log.e(
            CarLog.TAG_SERVICE,
            "IVehicle/" + instanceName + " service not registered yet"
        );
    }
    return null;
}

```

### 3.2 实现Service服务，ICarImpl实现

接着我们再来看ICarImpl的实现，如下所示：

#### 3.2.1 创建各个核心服务对象。

#### 3.2.2 把服务对象缓存到CarLocalServices中，主要为了方便Service之间的相互访问。

```java
ICarImpl(Context serviceContext, IVehicle vehicle, SystemInterface systemInterface,         CanBusErrorNotifier errorNotifier, String vehicleInterfaceName,@Nullable CarUserService carUserService,@Nullable CarWatchdogService carWatchdogService) {    ...mCarPowerManagementService = new CarPowerManagementService(mContext, mHal.getPowerHal(),            systemInterface, mCarUserService);    ...CarLocalServices.addService(CarPowerManagementService.class, mCarPowerManagementService);    CarLocalServices.addService(CarPropertyService.class, mCarPropertyService);    CarLocalServices.addService(CarUserService.class, mCarUserService);    CarLocalServices.addService(CarTrustedDeviceService.class, mCarTrustedDeviceService);    CarLocalServices.addService(CarUserNoticeService.class, mCarUserNoticeService);    CarLocalServices.addService(SystemInterface.class, mSystemInterface);    CarLocalServices.addService(CarDrivingStateService.class, mCarDrivingStateService);    CarLocalServices.addService(PerUserCarServiceHelper.class, mPerUserCarServiceHelper);    CarLocalServices.addService(FixedActivityService.class, mFixedActivityService);    CarLocalServices.addService(VmsBrokerService.class, mVmsBrokerService);    CarLocalServices.addService(CarOccupantZoneService.class, mCarOccupantZoneService);    CarLocalServices.addService(AppFocusService.class, mAppFocusService);List<CarServiceBase> allServices = new ArrayList<>();    allServices.add(mFeatureController);    allServices.add(mCarUserService);    ...    allServices.add(mCarWatchdogService);addServiceIfNonNull(allServices, mCarExperimentalFeatureServiceController);    mAllServices = allServices.toArray(new CarServiceBase[allServices.size()]);}
```

#### 3.2.3 将服务对象放置一个list中。这样init方法中就可以以循环的形式直接调用服务对象的init，而不需要一个个调用。VechicleHAL的程序也会在这里完成初始化。

```java
@MainThreadvoid init() {    mBootTiming = new TimingsTraceLog(VHAL_TIMING_TAG, Trace.TRACE_TAG_HAL);    traceBegin("VehicleHal.init");    mHal.init();    traceEnd();    traceBegin("CarService.initAllServices");for (CarServiceBase service : mAllServices) {        service.init();    }    traceEnd();}
```

#### 3.2.4 最后实现ICar.aidl中定义的各个接口就可以了

```java
@Overridepublic IBinder getCarService(String serviceName) {if (!mFeatureController.isFeatureEnabled(serviceName)) {        Log.w(CarLog.TAG_SERVICE, "getCarService for disabled service:" + serviceName);return null;    }switch (serviceName) {case Car.AUDIO_SERVICE:return mCarAudioService;case Car.APP_FOCUS_SERVICE:return mAppFocusService;case Car.PACKAGE_SERVICE:return mCarPackageManagerService;       ...default:IBinder service = null;if (mCarExperimentalFeatureServiceController != null) {                service = mCarExperimentalFeatureServiceController.getCarService(serviceName);            }if (service == null) {                Log.w(CarLog.TAG_SERVICE, "getCarService for unknown service:"                        + serviceName);            }return service;    }}
```

### 3.3 CarService运行图 

![](https://i-blog.csdnimg.cn/blog_migrate/dcb48b1412248dd03767659b9279b3c4.png)

### 3.4 总结

     `CarService实现的功能几乎就是覆盖整个车载Framework的核心。`

     `然而现实中为了保证各个核心服务的稳定性，同时降低CarService协同开发的难度，一般会选择将一些重要的服务拆分单独作为一个独立的Service运行在独立的进程中，导致有的车机系统中CarService只实现了CarPropertyService的功能。`

     **`CarService实现流程可以这样理解：提供`IVehicle对象与底层交互，提供ICarImpl初始化一系列服务交给ServiceManager管理，而这些服务可以通过IVehicle对象调用底层的API，`CarService充当一个中介代理的角色存在。`**