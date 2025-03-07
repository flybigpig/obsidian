本篇基于AOSP11简单来看看汽车专有的基础API。

汽车相关的API源码集中在`packages/services/Car/car-lib`目录下，此目录提供了一个类`Car`，为外界提供汽车所有服务和数据的访问。

打开`packages/services/Car/car-lib`的Android.bp可以发现此目录会被编译成一个名为`"android.car"`的java库

```
java_library {
    name: "android.car",
    srcs: [
        "src/**/*.java",
        "src/**/I*.aidl",
    ],
    ....
}
```

这意味着其他模块想要使用`Car`相关API需要引入此java库。

接下来我们以一个Demo来看看Car API的简单用法，我在`packages/apps/Car/`下增加了一个Demo APP，汽车的模块定义在`packages/services/Car/car_product/build/car.mk`中：

```
# Automotive specific packages
PRODUCT_PACKAGES += \
    CarFrameworkPackageStubs \
    CarService \
    CarShell \
    CarDialerApp \
    CarRadioApp \
    OverviewApp \
    CarLauncher \
    CarSystemUI \
    LocalMediaPlayer \
    CarMediaApp \
    CarMessengerApp \
    CarHvacApp \
    CarMapsPlaceholder \
    CarLatinIME \
    CarSettings \
    CarUsbHandler \
    android.car \
    car-frameworks-service \
    com.android.car.procfsinspector \
    libcar-framework-service-jni \

```

Demo APP的Android.bp文件如下：

```
android_app {
    name: "DemoAPP",
    srcs: ["src/**/*.java"],
    platform_apis: true,
    libs: ["android.car"],
}
```

前面说过，想要使用`Car`，就需要引入`"android.car"`。

这个Demo APP只有一个`Activity`，如下：

```
package com.example.demoapp;
import android.os.Bundle;
import android.app.Activity;
import android.car.Car;
import android.car.hardware.cabin.CarCabinManager;
import android.car.hardware.CarPropertyConfig;
import java.util.List;
public class MainActivity extends Activity {
    private CarCabinManager mCarCabinManager;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
    }
     @Override
    protected void onResume(){
        super.onResume();
        Car mCar = Car.createCar(this);
        
        mCarCabinManager =
                (CarCabinManager) mCar.getCarManager(Car.CABIN_SERVICE);
                
        List<CarPropertyConfig> mList = mCarCabinManager.getPropertyList();
        
        android.util.Log.d("debug_car","mCarCabinManager = :"+mCarCabinManager+",mList.size = :"+mList.size());
    }
}
```

代码很简单，在`onResume`时通过`createCar`创建`Car`实例，然后就可以通过其`getCarManager`获取各种manager，有了manager就可以访问汽车的各种数据以及状态，运行结果如下：  
![运行结果](https://i-blog.csdnimg.cn/blog_migrate/0c187e655e244ee379e3ecbb96bfed2b.png)

接下来去看看源码，我们首先来看`Car`的创建：

### Car.createCar

```
    public static Car createCar(Context context) {
        return createCar(context, (Handler) null);
    }

    @Nullable
    public static Car createCar(Context context, @Nullable Handler handler) {
        assertNonNullContext(context);
        Car car = null;
        IBinder service = null;
        boolean started = false;
        int retryCount = 0;
        while (true) {
            service = ServiceManager.getService(CAR_SERVICE_BINDER_SERVICE_NAME);
            if (car == null) {
                // service can be still null. The constructor is safe for null service.
                car = new Car(context, ICar.Stub.asInterface(service),
                        null /*serviceConnectionListener*/, null /*statusChangeListener*/, handler);
            }
            if (service != null) {
                if (!started) {  // specialization for most common case.
                    // Do this to crash client when car service crashes.
                    car.startCarService();
                    return car;
                }
                break;
            }
            if (!started) {
                car.startCarService();
                started = true;
            }
            retryCount++;
            if (retryCount > CAR_SERVICE_BINDER_POLLING_MAX_RETRY) {
                Log.e(TAG_CAR, "cannot get car_service, waited for car service (ms):"
                                + CAR_SERVICE_BINDER_POLLING_INTERVAL_MS
                                * CAR_SERVICE_BINDER_POLLING_MAX_RETRY,
                        new RuntimeException());
                return null;
            }
            try {
                Thread.sleep(CAR_SERVICE_BINDER_POLLING_INTERVAL_MS);
            } catch (InterruptedException e) {
                Log.e(CarLibLog.TAG_CAR, "interrupted while waiting for car_service",
                        new RuntimeException());
                return null;
            }
        }
        // Can be accessed from mServiceConnectionListener in main thread.
        synchronized (car) {
            if (car.mService == null) {
                car.mService = ICar.Stub.asInterface(service);
                Log.w(TAG_CAR,
                        "waited for car_service (ms):"
                                + CAR_SERVICE_BINDER_POLLING_INTERVAL_MS * retryCount,
                        new RuntimeException());
            }
            car.mConnectionState = STATE_CONNECTED;
        }
        return car;
    }
```

`Car`提供了多个`createCar`的重载方法，原理都差不多，看其中一个即可，上面的代码逻辑很简单，有两个目的，1. 向`ServiceManager`获取名为`"car_service"`的binder服务。 2. new出一个`Car`实例。

首先来看步骤一，这里会死循环的获取`"car_service"`服务，尝试次数为100，其中每次间隔50ms，`“car_service"`这个服务我们在上一片文章[  
Android Automotive之CarService开机启动  
](https://blog.csdn.net/qq_34211365/article/details/117510997)中看到了，`CarService`被绑定之后，其`onCreate`方法中初始化了`"car_service”`，并将其注册到了`ServiceManager`：

```
 @Override
    public void onCreate() {
       ...

        mICarImpl = new ICarImpl(this,
                mVehicle,
                SystemInterface.Builder.defaultSystemInterface(this).build(),
                mCanBusErrorNotifier,
                mVehicleInterfaceName);
        mICarImpl.init();

        ServiceManager.addService("car_service", mICarImpl);
        ...
        super.onCreate();
    }
```

`"car_service"`实际上就是`ICarImpl`，作为`ICar`接口的实现类。

继续来看步骤二，new出一个`Car`实例:

### Car构造方法

```
    private Car(Context context, @Nullable ICar service,
            @Nullable ServiceConnection serviceConnectionListener,
            @Nullable CarServiceLifecycleListener statusChangeListener,
            @Nullable Handler handler) {
        mContext = context;
        mEventHandler = determineEventHandler(handler);
        mMainThreadEventHandler = determineMainThreadEventHandler(mEventHandler);

        mService = service;
        if (service != null) {
            mConnectionState = STATE_CONNECTED;
        } else {
            mConnectionState = STATE_DISCONNECTED;
        }
        mServiceConnectionListenerClient = serviceConnectionListener;
        mStatusChangeCallback = statusChangeListener;
        // Store construction stack so that client can get help when it crashes when car service
        // crashes.
        if (serviceConnectionListener == null && statusChangeListener == null) {
            mConstructionStack = new RuntimeException();
        } else {
            mConstructionStack = null;
        }
    }
```

`determineEventHandler`和`determineMainThreadEventHandler`这两个方法用来保证`Car`内部的`Handler`不能为空并且必须是运行在主线程，`mService`保存了`"car_service"`的binder代理对象，`mServiceConnectionListenerClient`用作通知APP端`CarService`已经启动，此回调可以为空。  
`Car`构造方法中还有个比较重要的变量`mConnectionState`，用来描述当前客户端与`"car_service"`的连接状态，有三个状态值：断开连接，正在连接，已经连接。

```
    private static final int STATE_DISCONNECTED = 0;
    private static final int STATE_CONNECTING = 1;
    private static final int STATE_CONNECTED = 2;
  /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(prefix = "STATE_", value = {
            STATE_DISCONNECTED,
            STATE_CONNECTING,
            STATE_CONNECTED,
    })
```

到此`Car.createCar`就看完了，其核心就两点，1. 要获取`"car_service"`，将其代理对象保存在`Car`的成员变量`mService`中，以便后续使用，2. 实例化`Car`对象。

其实前面还有个方法`startCarService`没看，这是因为此方法的目的同样是“获取`"car_service"`，将其代理对象保存在`Car`的成员变量`mService`中”，只不过用的是绑定服务的方式，如果失败会最多尝试20次。

接着回到前面的Demo APP，`Car`创建完成之后就可以拿`Car`对象获取各个相关manager，我们这里获取的是名为`"cabin"`的manager：

### Car.getCarManager

```
    public Object getCarManager(String serviceName) {
        CarManagerBase manager;
        synchronized (mLock) {
            if (mService == null) {
                Log.w(TAG_CAR, "getCarManager not working while car service not ready");
                return null;
            }
            manager = mServiceMap.get(serviceName);
            if (manager == null) {
                try {
                    IBinder binder = mService.getCarService(serviceName);
                    if (binder == null) {
                        Log.w(TAG_CAR, "getCarManager could not get binder for service:"
                                + serviceName);
                        return null;
                    }
                    manager = createCarManagerLocked(serviceName, binder);
                    if (manager == null) {
                        Log.w(TAG_CAR, "getCarManager could not create manager for service:"
                                        + serviceName);
                        return null;
                    }
                    mServiceMap.put(serviceName, manager);
                } catch (RemoteException e) {
                    handleRemoteExceptionFromCarService(e);
                }
            }
        }
        return manager;
    }
```

`mServiceMap`是一个缓存集合，如果这里面已经有要求的manager则直接返回，否则需要创建，`mService`刚刚才看了，保存了`"car_service"`服务的client端，所以这里实际是去`ICarImpl`中根据字符串名称`"cabin"`获取其Binder服务：

### ICarImpl.getCarService

```
    @Override
    public IBinder getCarService(String serviceName) {
        if (!mFeatureController.isFeatureEnabled(serviceName)) {
            Log.w(CarLog.TAG_SERVICE, "getCarService for disabled service:" + serviceName);
            return null;
        }
        switch (serviceName) {
            ...
            
            case Car.CABIN_SERVICE:
                return mCarPropertyService;
                
            ...
            case Car.BLUETOOTH_SERVICE:
                return mCarBluetoothService;
            ...
            default:
                IBinder service = null;
                if (mCarExperimentalFeatureServiceController != null) {
                    service = mCarExperimentalFeatureServiceController.getCarService(serviceName);
                }
                if (service == null) {
                    Log.w(CarLog.TAG_SERVICE, "getCarService for unknown service:"
                            + serviceName);
                }
                return service;
        }
    }
```

这里面有很多`service`，都是在`ICarImpl`构造方法中创建的，我们`"cabin"`就拿到了`CarPropertyService`这个服务，再回到前面`createCarManagerLocked`方法：

### Car.createCarManagerLocked

```
    @Nullable
    private CarManagerBase createCarManagerLocked(String serviceName, IBinder binder) {
        CarManagerBase manager = null;
        switch (serviceName) {
            ...
            case CABIN_SERVICE:
                manager = new CarCabinManager(this, binder);
                break;
            ...
            default:
                ...
                break;
        }
        return manager;
    }
```

new了一个`CarCabinManager`：

```
    public CarCabinManager(Car car, IBinder service) {
        super(car);
        ICarProperty mCarPropertyService = ICarProperty.Stub.asInterface(service);
        mCarPropertyMgr = new CarPropertyManager(car, mCarPropertyService);
    }
```

得到`CarPropertyService`代理端，并构造`CarPropertyManager`。

到这里我们可以发现，`Car`这个类仅仅作为外界的访问入口，它内部主要还是依靠`ICarImpl`这个Binder服务来获取各种manager，而各种manager又作为访问service的入口，这些service才是实现汽车具体业务逻辑的核心。

比如我们Demo APP中用到的`mCarCabinManager.getPropertyList()`方法，其具体实现在`CarPropertyService`中，但最终的实现都会绕到`vehicle hal`中的`getPropConfigs`中。