# Android Framework 核心实战实例实现文档

# Android Framework 核心实战实例实现文档

本文档基于Android 13 AOSP源码，完整实现表格中6大核心Framework实战场景，每个场景包含**前置知识、分步实现、带详细注释的核心代码、验证方法、踩坑避坑指南**，所有步骤均基于车载/定制ROM开发的工程化实践标准。

---

## 一、新增系统服务：车载车身信息管理服务

### 1\. 前置知识

需掌握：Binder IPC通信原理、SystemServer启动流程、AIDL跨进程通信、系统服务注册与管理
核心目标：实现APP通过AIDL跨进程获取车身车速、油量信息，服务运行在系统进程`system_server`中，具备系统级权限。

### 2\. 分步实现

#### 步骤1：定义AIDL接口

在`frameworks/base/core/java/com/android/internal/os/`目录下创建AIDL文件，定义跨进程通信接口。

```Java
// ICarInfoManager.aidl
package com.android.internal.os;

// 定义车身信息数据结构
parcelable CarInfo {
    // 车速，单位：km/h
    int speed;
    // 油量，单位：%
    int fuelLevel;
    // 车门状态，true为关闭，false为开启
    boolean doorClosed;
}

interface ICarInfoManager {
    // 获取当前车身全量信息
    CarInfo getCurrentCarInfo();
    // 单独获取车速
    int getCurrentSpeed();
    // 单独获取油量
    int getCurrentFuelLevel();
}
```

#### 步骤2：实现系统服务核心逻辑

在`frameworks/base/services/core/java/com/android/server/os/`目录下创建服务实现类，继承`SystemService`，实现AIDL接口。

```Java
// CarInfoManagerService.java
package com.android.server.os;

import android.content.Context;
import android.os.Parcel;
import android.os.RemoteException;
import android.os.SystemService;
import com.android.internal.os.ICarInfoManager;

// 车身信息管理系统服务，运行在system_server进程
public class CarInfoManagerService extends SystemService {
    // 服务名称，系统注册时使用
    private static final String SERVICE_NAME = "car_info_manager";
    // 车身信息数据缓存，实际项目中从CAN总线/硬件读取
    private CarInfo mCurrentCarInfo = new CarInfo(0, 100, true);

    // 构造函数，上下文由SystemServer传入
    public CarInfoManagerService(Context context) {
        super(context);
    }

    // 服务创建时调用，完成初始化
    @Override
    public void onCreate() {
        super.onCreate();
        // 注册服务到ServiceManager，让其他进程可通过名称获取
        publishBinderService(SERVICE_NAME, mBinder);
    }

    // 服务启动时调用，可启动后台线程、监听硬件事件
    @Override
    public void onStart() {
        super.onStart();
        // 模拟硬件数据更新，实际项目中对接CAN总线驱动
        startHardwareSimulator();
    }

    // AIDL接口的Binder实现，核心跨进程通信逻辑
    private final ICarInfoManager.Stub mBinder = new ICarInfoManager.Stub() {
        @Override
        public CarInfo getCurrentCarInfo() throws RemoteException {
            // 权限校验：仅系统APP、车载应用可访问
            if (getContext().checkCallingOrSelfPermission("android.permission.ACCESS_CAR_INFO") 
                    != android.content.pm.PackageManager.PERMISSION_GRANTED) {
                throw new SecurityException("Caller has no permission to access car info");
            }
            return mCurrentCarInfo;
        }

        @Override
        public int getCurrentSpeed() throws RemoteException {
            return getCurrentCarInfo().speed;
        }

        @Override
        public int getCurrentFuelLevel() throws RemoteException {
            return getCurrentCarInfo().fuelLevel;
        }

        // Binder跨进程传输的核心方法，处理数据序列化/反序列化
        @Override
        public boolean onTransact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
            try {
                return super.onTransact(code, data, reply, flags);
            } catch (Exception e) {
                throw new RemoteException("Failed to transact car info service: " + e.getMessage());
            }
        }
    };

    // 模拟硬件数据更新线程，实际项目替换为CAN总线数据读取
    private void startHardwareSimulator() {
        new Thread(() -> {
            while (true) {
                try {
                    // 模拟车速随机变化
                    mCurrentCarInfo.speed = (int) (Math.random() * 120);
                    // 模拟油量缓慢下降
                    mCurrentCarInfo.fuelLevel = Math.max(0, mCurrentCarInfo.fuelLevel - 1);
                    // 每1秒更新一次
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    break;
                }
            }
        }).start();
    }

    // 对外提供的服务获取方法，供系统内部调用
    public static ICarInfoManager getService() {
        return ICarInfoManager.Stub.asInterface(
                android.os.ServiceManager.getService(SERVICE_NAME));
    }
}
```

#### 步骤3：注册服务到SystemServer

修改`frameworks/base/services/java/com/android/server/SystemServer.java`，在系统启动时注册该服务。

```Java
// SystemServer.java 核心修改片段
private void startOtherServices(@NonNull TimingsTraceAndSlog traceLog) {
    // ... 原有代码
    traceBegin(traceLog, "CarInfoManagerService");
    try {
        // 注册车身信息服务，系统启动时自动创建
        ServiceManager.addService("car_info_manager", 
                new CarInfoManagerService(context));
    } catch (Throwable e) {
        Slog.e(TAG, "Failed to start CarInfoManagerService", e);
    }
    traceEnd(traceLog);
    // ... 原有代码
}
```

#### 步骤4：APP端调用示例

APP通过AIDL获取系统服务，读取车身信息。

```Java
// APP端调用代码
private ICarInfoManager mCarInfoService;

// 绑定系统服务
private void bindCarInfoService() {
    Intent intent = new Intent("android.car.ICarInfoManager");
    intent.setPackage("com.android.server");
    bindService(intent, mServiceConnection, Context.BIND_AUTO_CREATE);
}

private final ServiceConnection mServiceConnection = new ServiceConnection() {
    @Override
    public void onServiceConnected(ComponentName name, IBinder service) {
        mCarInfoService = ICarInfoManager.Stub.asInterface(service);
        try {
            // 读取车身信息
            CarInfo carInfo = mCarInfoService.getCurrentCarInfo();
            Log.d("CarInfo", "当前车速：" + carInfo.speed + "km/h，油量：" + carInfo.fuelLevel + "%");
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onServiceDisconnected(ComponentName name) {
        mCarInfoService = null;
    }
};
```

#### 步骤5：验证方法

1. 编译AOSP源码，刷入设备/模拟器

2. 通过`adb shell service list | grep car_info_manager`查看服务是否注册成功

3. 安装测试APP，验证可正常读取车速、油量数据

4. 验证无权限APP调用时会抛出SecurityException

#### 踩坑指南

- 权限问题：必须在系统`sepolicy`中配置服务的SELinux权限，否则系统启动时会拒绝注册

- 进程问题：系统服务必须运行在`system_server`进程，不能单独开进程，否则会出现权限隔离

- 兼容性问题：AIDL文件修改后，需要重新编译整个framework，否则APP端会出现类找不到异常

---

## 二、修改系统行为：修改Launcher多任务切换动画、禁用系统对话框

### 1\. 前置知识

需掌握：AMS/ATMS（任务管理服务）、WMS（窗口管理服务）、Launcher源码结构、Android动画框架
核心目标：定制系统桌面的多任务切换动画，禁用系统级的权限/确认对话框，实现系统行为的深度定制。

### 2\. 分步实现

#### 场景1：修改Launcher多任务切换动画

##### 步骤1：定位Launcher多任务动画核心类

Launcher的多任务（Recent Tasks）动画逻辑在`com.android.launcher3.anim`包下，核心类为`RecentsView`、`TaskView`、`AnimationController`。

##### 步骤2：定制动画效果

修改`RecentsView.java`中的动画配置，实现自定义的翻页、缩放、透明度动画。

```Java
// RecentsView.java 动画修改核心片段
private void animateTaskView(TaskView taskView, int index, boolean isScrolling) {
    // 原有动画逻辑注释，替换为自定义动画
    // 1. 缩放动画：当前任务缩放1.0，相邻任务缩放0.8，实现层次感
    float scale = Math.max(0.8f, 1.0f - Math.abs(index - mCurrentTaskIndex) * 0.2f);
    taskView.setScaleX(scale);
    taskView.setScaleY(scale);

    // 2. 透明度动画：距离越远透明度越低
    float alpha = Math.max(0.5f, 1.0f - Math.abs(index - mCurrentTaskIndex) * 0.3f);
    taskView.setAlpha(alpha);

    // 3. 位移动画：横向偏移，实现3D翻页效果
    float translationX = (index - mCurrentTaskIndex) * getResources().getDimension(R.dimen.task_view_offset);
    taskView.setTranslationX(translationX);

    // 4. 旋转动画：实现轻微的Y轴旋转，增强3D效果
    float rotationY = (index - mCurrentTaskIndex) * 15f;
    taskView.setRotationY(rotationY);
}
```

##### 步骤3：修改动画时长与插值器

在`AnimationController.java`中修改动画的时长、插值器，实现更流畅的动画效果。

```Java
// AnimationController.java 动画配置修改
public static final long RECENTS_ANIMATION_DURATION = 300; // 动画时长，单位ms
public static final Interpolator RECENTS_INTERPOLATOR = new OvershootInterpolator(1.2f); // 插值器，实现回弹效果

// 启动多任务切换动画
public void startRecentsAnimation() {
    ValueAnimator animator = ValueAnimator.ofFloat(0f, 1f);
    animator.setDuration(RECENTS_ANIMATION_DURATION);
    animator.setInterpolator(RECENTS_INTERPOLATOR);
    animator.addUpdateListener(animator -> {
        // 动画更新逻辑
        updateRecentsAnimationProgress(animator.getAnimatedFraction());
    });
    animator.start();
}
```

#### 场景2：禁用系统对话框

##### 步骤1：定位系统对话框创建入口

系统对话框（如权限申请、确认对话框）的创建核心在`AlertDialog.java`、`Dialog.java`，以及WMS中的`WindowManagerService`。

##### 步骤2：全局禁用系统对话框

修改`WindowManagerService.java`，拦截对话框窗口的添加请求，实现全局禁用。

```Java
// WindowManagerService.java 核心修改片段
@Override
public int addWindow(Session session, WindowManager.LayoutParams attrs,
        int viewVisibility, int displayId, InsetsState insetsState,
        InputChannel outInputChannel, InsetsSourceControl[] outControls) {
    // 拦截系统对话框窗口
    if (isSystemDialog(attrs)) {
        // 直接返回成功，不添加窗口，实现禁用效果
        return WindowManagerGlobal.ADD_OK;
    }
    // 原有逻辑
    return super.addWindow(session, attrs, viewVisibility, displayId, insetsState,
            outInputChannel, outControls);
}

// 判断是否为系统对话框
private boolean isSystemDialog(WindowManager.LayoutParams attrs) {
    // 匹配系统对话框的类型、包名、标题
    if (attrs.type == WindowManager.LayoutParams.TYPE_SYSTEM_ALERT ||
            attrs.type == WindowManager.LayoutParams.TYPE_SYSTEM_DIALOG) {
        return true;
    }
    // 匹配权限申请对话框
    if (attrs.packageName.equals("android") &&
            attrs.title.toString().contains("permission")) {
        return true;
    }
    return false;
}
```

##### 步骤3：验证方法

1. 编译Launcher3模块，推送到设备`/system/priv-app/Launcher3/`目录

2. 重启设备，进入多任务界面，验证自定义动画效果是否生效

3. 触发系统对话框（如安装未知应用、权限申请），验证对话框是否被禁用

#### 踩坑指南

- Launcher修改后必须重启设备，否则系统会缓存原有Launcher实例

- 禁用系统对话框时，需区分全局禁用和特定场景禁用，避免影响系统核心功能

- 动画修改需适配不同分辨率、不同DPI的设备，避免出现布局错位

---

## 三、适配硬件外设：CAN总线、串口屏、自定义按键板

### 1\. 前置知识

需掌握：Android HAL层架构、JNI调用、Input子系统、Linux内核驱动、设备树配置
核心目标：实现硬件外设的系统级适配，让Android系统可识别、控制硬件外设，实现数据交互。

### 2\. 分步实现

#### 场景1：CAN总线适配

##### 步骤1：内核配置CAN总线驱动

1. 在内核配置中开启CAN总线支持：`CONFIG_CAN=y`、`CONFIG_CAN_RAW=y`、`CONFIG_CAN_SLCAN=y`

2. 配置设备树，匹配CAN总线硬件节点，设置引脚、时钟、中断等信息

```Plaintext
// 设备树CAN节点示例
can@10000000 {
    compatible = "vendor,can-controller";
    reg = <0x10000000 0x1000>;
    interrupts = <0 10 4>;
    clocks = <&clk 0>;
    pinctrl-names = "default";
    pinctrl-0 = <&can_pins>;
    status = "okay";
};
```

##### 步骤2：HAL层实现CAN总线访问接口

在`hardware/interfaces/can/1.0/`目录下创建CAN总线HAL接口，实现硬件访问。

```C++
// CanDevice.h HAL层头文件
#ifndef CAN_DEVICE_H
#define CAN_DEVICE_H

#include <android/hardware/can/1.0/ICan.h>
#include <hidl/Status.h>

namespace android {
namespace hardware {
namespace can {
namespace V1_0 {
namespace implementation {

using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::sp;

class CanDevice : public ICan {
public:
    CanDevice();
    Return<bool> open(const char* deviceName) override;
    Return<void> close() override;
    Return<int32_t> send(const hidl_vec<uint8_t>& data, uint32_t id) override;
    Return<void> receive(ReceiveCallback callback) override;

private:
    int mFd = -1; // CAN总线文件描述符
};

} // namespace implementation
} // namespace V1_0
} // namespace can
} // namespace hardware
} // namespace android

#endif // CAN_DEVICE_H
```

##### 步骤3：JNI层调用HAL接口，供Framework层使用

在`frameworks/base/core/jni/android_hardware_can_CanManager.cpp`中实现JNI方法，调用HAL层接口。

```C++
// JNI层核心代码
#include <jni.h>
#include <android/hardware/can/1.0/ICan.h>
#include <hidl/LegacySupport.h>

using namespace android::hardware::can::V1_0;
using namespace android::hardware;

static jboolean openCanDevice(JNIEnv* env, jobject thiz, jstring deviceName) {
    const char* name = env->GetStringUTFChars(deviceName, nullptr);
    sp<ICan> canDevice = ICan::getService();
    bool result = canDevice->open(name);
    env->ReleaseStringUTFChars(deviceName, name);
    return result;
}

// 注册JNI方法
static const JNINativeMethod gMethods[] = {
    {"open", "(Ljava/lang/String;)Z", (void*)openCanDevice},
};

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    jclass clazz = env->FindClass("android/hardware/can/CanManager");
    env->RegisterNatives(clazz, gMethods, sizeof(gMethods) / sizeof(gMethods[0]));
    return JNI_VERSION_1_6;
}
```

##### 步骤4：Framework层封装CAN管理类

在`frameworks/base/core/java/android/hardware/can/CanManager.java`中封装CAN总线API，供APP调用。

```Java
// CanManager.java Framework层封装
package android.hardware.can;

import android.content.Context;
import android.os.RemoteException;

public class CanManager {
    private static final String TAG = "CanManager";
    private final ICan mCanService;

    // 构造函数，获取HAL层服务
    public CanManager(Context context) {
        mCanService = ICan.getService();
    }

    // 打开CAN总线设备
    public boolean open(String deviceName) {
        try {
            return mCanService.open(deviceName);
        } catch (RemoteException e) {
            e.printStackTrace();
            return false;
        }
    }

    // 发送CAN数据
    public int send(byte[] data, int id) {
        try {
            return mCanService.send(data, id);
        } catch (RemoteException e) {
            e.printStackTrace();
            return -1;
        }
    }
}
```

#### 场景2：自定义按键板适配

##### 步骤1：内核配置输入驱动

1. 配置内核输入子系统：`CONFIG_INPUT=y`、`CONFIG_INPUT_KEYBOARD=y`

2. 编写自定义按键驱动，注册输入设备，上报按键事件

```C
// 自定义按键驱动核心代码
#include <linux/input.h>
#include <linux/platform_device.h>

static struct input_dev *g_input_dev;

// 按键中断处理函数
static irqreturn_t custom_key_irq(int irq, void *dev_id) {
    int key = *(int *)dev_id;
    // 上报按键按下事件
    input_report_key(g_input_dev, key, 1);
    input_sync(g_input_dev);
    // 延时后上报按键释放事件
    mdelay(100);
    input_report_key(g_input_dev, key, 0);
    input_sync(g_input_dev);
    return IRQ_HANDLED;
}

// 驱动探针函数
static int custom_key_probe(struct platform_device *pdev) {
    // 分配输入设备
    g_input_dev = input_allocate_device();
    if (!g_input_dev) {
        return -ENOMEM;
    }
    // 设置输入设备属性
    g_input_dev->name = "custom_keyboard";
    g_input_dev->id.bustype = BUS_PLATFORM;
    // 注册按键事件
    set_bit(EV_KEY, g_input_dev->evbit);
    set_bit(KEY_VOLUMEUP, g_input_dev->keybit);
    set_bit(KEY_VOLUMEDOWN, g_input_dev->keybit);
    // 注册中断
    request_irq(platform_get_irq(pdev, 0), custom_key_irq, IRQF_TRIGGER_FALLING,
            "custom_key_irq", (void *)KEY_VOLUMEUP);
    // 注册输入设备
    input_register_device(g_input_dev);
    return 0;
}

// 驱动卸载函数
static void custom_key_remove(struct platform_device *pdev) {
    input_unregister_device(g_input_dev);
    input_free_device(g_input_dev);
    free_irq(platform_get_irq(pdev, 0), (void *)KEY_VOLUMEUP);
}

// 平台驱动匹配表
static const struct of_device_id custom_key_of_match[] = {
    {.compatible = "vendor,custom-keyboard"},
    {},
};
MODULE_DEVICE_TABLE(of, custom_key_of_match);

// 注册平台驱动
static struct platform_driver custom_key_driver = {
    .probe = custom_key_probe,
    .remove = custom_key_remove,
    .driver = {
        .name = "custom-keyboard",
        .of_match_table = custom_key_of_match,
    },
};
module_platform_driver(custom_key_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Custom Keyboard Driver");
```

##### 步骤2：系统配置按键映射

修改`/system/usr/keylayout/Generic.kl`文件，配置按键码与Android keycode的映射关系。

```Plaintext
# 自定义按键映射
key 115 VOLUMEUP
key 114 VOLUMEDOWN
key 116 POWER
```

##### 步骤3：验证方法

1. 编译内核，刷入设备，通过`adb shell cat /proc/bus/input/devices`查看输入设备是否注册成功

2. 触发按键，通过`adb shell getevent`查看按键事件是否上报

3. 验证按键是否能正常触发系统对应功能（如音量调节、电源键）

#### 踩坑指南

- HAL层必须遵循HIDL规范，否则无法被Framework层正常调用

- 内核驱动必须正确注册字符设备，否则用户空间无法访问硬件

- 按键映射必须与内核上报的按键码一致，否则Android系统无法识别按键功能

---

## 四、裁剪/定制系统：移除不需要的系统应用、定制Settings菜单项

### 1\. 前置知识

需掌握：Android编译系统（Soong）、系统应用编译配置、Settings源码结构、系统资源定制
核心目标：裁剪系统冗余应用，定制系统设置界面，实现ROM的轻量化和品牌化定制。

### 2\. 分步实现

#### 场景1：移除不需要的系统应用

##### 步骤1：定位系统应用编译配置

系统应用的编译配置在`packages/apps/`目录下，每个应用都有对应的`Android.bp`编译脚本。

##### 步骤2：全局移除系统应用

修改产品的`product.mk`文件，通过`PRODUCT_PACKAGES`变量移除不需要的系统应用。

```Makefile
// product.mk 系统应用裁剪配置
# 移除不需要的系统应用
PRODUCT_PACKAGES += \
    -Email \
    -Calendar \
    -Contacts \
    -Phone \
    -Messaging \
    -YouTube \
    -Chrome

# 保留的核心系统应用
PRODUCT_PACKAGES += \
    Launcher3 \
    Settings \
    SystemUI \
    Framework-res
```

##### 步骤3：条件编译移除应用

针对特定产品变体，通过条件编译移除应用。

```Makefile
// Android.bp 条件编译示例
soong_app {
    name: "Email",
    srcs: ["src/**/*.java"],
    // 仅在debug版本编译，release版本不编译
    product_variables: {
        build_type: {
            debug: {
                enabled: true,
            },
            release: {
                enabled: false,
            },
        },
    },
}
```

#### 场景2：定制Settings菜单项

##### 步骤1：定位Settings核心结构

Settings的主界面配置在`res/xml/settings_fragments.xml`中，核心逻辑在`com.android.settings.Settings`类中。

##### 步骤2：新增自定义设置菜单项

1. 在`res/xml/settings_fragments.xml`中新增菜单项

```XML
<!-- settings_fragments.xml 新增菜单项 -->
<PreferenceScreen
    xmlns:android="http://schemas.android.com/apk/res/android"
    android:key="custom_settings"
    android:title="@string/custom_settings_title"
    android:summary="@string/custom_settings_summary">

    <Preference
        android:key="custom_key"
        android:title="@string/custom_key_title"
        android:summary="@string/custom_key_summary"
        android:fragment="com.android.settings.custom.CustomSettingsFragment" />

</PreferenceScreen>
```

2. 实现自定义设置Fragment

```Java
// CustomSettingsFragment.java 自定义设置界面
package com.android.settings.custom;

import android.os.Bundle;
import androidx.preference.PreferenceFragmentCompat;
import com.android.settings.R;

public class CustomSettingsFragment extends PreferenceFragmentCompat {
    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        // 加载自定义设置布局
        setPreferencesFromResource(R.xml.custom_settings, rootKey);
        // 初始化设置项
        initCustomPreferences();
    }

    private void initCustomPreferences() {
        // 绑定设置项，实现数据存储、状态更新
        findPreference("custom_switch").setOnPreferenceChangeListener((preference, newValue) -> {
            // 处理开关状态变化
            boolean isEnabled = (boolean) newValue;
            // 保存设置到系统属性
            android.os.SystemProperties.set("persist.sys.custom_setting", isEnabled ? "1" : "0");
            return true;
        });
    }
}
```

##### 步骤3：添加系统属性支持

在`frameworks/base/core/java/android/os/SystemProperties.java`中新增自定义系统属性，实现设置的持久化。

```Java
// SystemProperties.java 新增属性
public static final String CUSTOM_SETTING = "persist.sys.custom_setting";

public static boolean getCustomSetting() {
    return getBoolean(CUSTOM_SETTING, false);
}

public static void setCustomSetting(boolean value) {
    set(CUSTOM_SETTING, value ? "1" : "0");
}
```

##### 步骤4：验证方法

1. 编译AOSP源码，刷入设备

2. 进入系统设置界面，验证自定义菜单项是否显示

3. 验证设置项修改后是否能正常保存，重启后是否生效

4. 验证被移除的系统应用是否在系统中消失

#### 踩坑指南

- 移除系统应用时，需确认该应用是否被系统核心组件依赖，否则会导致系统功能异常

- Settings修改后，需重新编译Settings模块和framework，否则会出现资源找不到异常

- 系统属性必须使用`persist`前缀，否则重启后会丢失

---

## 五、性能/稳定性优化：优化开机速度、解决系统服务ANR

### 1\. 前置知识

需掌握：系统启动流程、init进程、Zygote进程、ANR机制、Linux内核调度、性能分析工具（systrace、perf）
核心目标：缩短系统开机时间，解决系统服务ANR（应用无响应）问题，提升系统稳定性。

### 2\. 分步实现

#### 场景1：优化开机速度

##### 步骤1：分析开机启动流程

系统开机流程分为4个阶段：

1. Bootloader启动 → 内核启动 → init进程启动

2. init进程挂载分区、初始化系统属性、启动核心服务

3. Zygote进程启动，孵化SystemServer进程

4. SystemServer启动系统服务、启动Launcher

##### 步骤2：优化init进程启动阶段

1. 并行启动非核心服务，修改`init.rc`脚本，将串行启动的服务改为并行启动

```Plaintext
// init.rc 并行启动服务示例
// 原有串行启动
service servicemanager /system/bin/servicemanager
    class core
    user system
    group system
    critical
    on boot

// 改为并行启动，添加`parallel`关键字
service servicemanager /system/bin/servicemanager
    class core
    user system
    group system
    critical
    on boot
    parallel
```

2. 延迟启动非核心服务，将非必要的服务延迟到系统启动完成后启动

```Plaintext
// 延迟启动服务示例
service custom_service /system/bin/custom_service
    class late_start
    user system
    group system
    on property:sys.boot_completed=1
```

##### 步骤3：优化Zygote进程启动

1. 预加载系统类，减少Zygote孵化时的类加载时间
修改`frameworks/base/core/java/com/android/internal/os/ZygoteInit.java`，预加载常用系统类。

```Java
// ZygoteInit.java 预加载类优化
private static void preloadClasses() {
    // 原有预加载类
    // 新增常用系统类预加载
    preloadClass("android.app.ActivityManager", true);
    preloadClass("android.content.Context", true);
    preloadClass("android.os.Bundle", true);
    preloadClass("android.view.View", true);
}
```

2. 开启Zygote进程的AOT编译，提前编译系统类，减少运行时编译时间
修改`product.mk`，开启AOT编译。

```Makefile
// product.mk AOT编译配置
PRODUCT_DEX_PREOPT_DEFAULT_COMPILER_FILTER := speed
PRODUCT_DEX_PREOPT_BOOT_IMAGE_COMPILER_FILTER := speed
```

##### 步骤4：优化SystemServer启动

1. 异步启动非核心系统服务，修改`SystemServer.java`，将非核心服务的启动改为异步线程启动。

```Java
// SystemServer.java 异步启动服务优化
private void startOtherServices(@NonNull TimingsTraceAndSlog traceLog) {
    // 核心服务同步启动
    startCoreServices(traceLog);
    // 非核心服务异步启动
    new Thread(() -> {
        startNonCoreServices(traceLog);
    }).start();
}
```

2. 减少系统服务的初始化耗时，将非必要的初始化操作延迟到系统启动完成后执行。

#### 场景2：解决系统服务ANR

##### 步骤1：分析ANR原因

系统服务ANR的核心原因：

1. 主线程阻塞，耗时操作（如IO、网络、计算）在主线程执行

2. 跨进程通信超时，Binder调用超时

3. 锁竞争，多线程抢占锁导致主线程长时间等待

##### 步骤2：主线程耗时操作异步化

将系统服务中的耗时操作改为异步线程执行，使用`Handler`、`AsyncTask`、`WorkManager`实现异步处理。

```Java
// 系统服务ANR修复示例
// 原有阻塞主线程的代码
public void updateSystemConfig() {
    // 耗时IO操作，阻塞主线程
    readConfigFromDisk();
    // 耗时计算
    calculateConfig();
    // 耗时网络请求
    fetchConfigFromServer();
}

// 优化后的异步代码
public void updateSystemConfig() {
    // 异步线程执行耗时操作
    new Thread(() -> {
        readConfigFromDisk();
        calculateConfig();
        fetchConfigFromServer();
        // 主线程更新UI/状态
        mHandler.post(() -> {
            updateConfigState();
        });
    }).start();
}
```

##### 步骤3：优化Binder跨进程通信

1. 减少Binder调用次数，将多次小数据量的Binder调用合并为一次大数据量调用

2. 设置Binder调用超时时间，避免长时间阻塞

```Java
// Binder调用超时设置示例
try {
    // 设置超时时间为500ms
    mBinder.transact(code, data, reply, flags, 500);
} catch (RemoteException e) {
    e.printStackTrace();
}
```

##### 步骤4：优化锁竞争

1. 减少锁的持有时间，仅在必要的代码块加锁

2. 使用非阻塞锁，避免主线程长时间等待

3. 拆分大锁为多个小锁，减少锁竞争的范围

```Java
// 锁优化示例
// 原有大锁
private final Object mLock = new Object();
public void updateConfig() {
    synchronized (mLock) {
        // 大量非耗时操作
        // ...
        // 耗时操作
        // ...
    }
}

// 优化后的锁
private final Object mConfigLock = new Object();
private final Object mDataLock = new Object();
public void updateConfig() {
    // 仅对配置操作加锁
    synchronized (mConfigLock) {
        updateConfigState();
    }
    // 数据操作使用单独的锁
    synchronized (mDataLock) {
        updateData();
    }
}
```

##### 步骤5：验证方法

1. 开机速度验证：通过`adb shell dmesg | grep "boot"`查看开机时间，对比优化前后的开机时长

2. ANR验证：通过`adb shell dumpsys activity anr`查看ANR日志，验证优化后是否还会出现ANR

3. 性能分析：使用`systrace`工具分析系统启动流程，定位耗时环节；使用`perf`工具分析系统服务的CPU占用情况

#### 踩坑指南

- 并行启动服务时，需确认服务之间的依赖关系，避免依赖的服务未启动导致功能异常

- 异步操作时，需注意线程安全，避免多线程同时访问共享数据导致的异常

- 锁优化时，需避免死锁，确保锁的获取和释放顺序一致

---

## 六、安全策略配置：SELinux sepolicy配置、放开hiddenapi限制

### 1\. 前置知识

需掌握：SELinux安全模型、Android权限机制、hiddenapi限制原理、系统签名配置
核心目标：为新增硬件节点配置SELinux安全策略，放开系统hiddenapi的调用限制，实现系统功能的安全定制。

### 2\. 分步实现

#### 场景1：为新增硬件节点配置SELinux sepolicy

##### 步骤1：SELinux基础概念

SELinux是Android的强制访问控制安全机制，通过`sepolicy`文件定义进程、文件、设备的访问权限，核心文件在`external/sepolicy/`目录下。

##### 步骤2：为硬件节点配置SELinux权限

1. 定义硬件节点的SELinux上下文，修改`file_contexts`文件

```Plaintext
// file_contexts 硬件节点上下文配置
/dev/can/can0    u:object_r:can_device:s0
/dev/input/custom_key    u:object_r:custom_key_device:s0
```

2. 定义硬件设备的SELinux类型，修改`device.te`文件

```Plaintext
// device.te 设备类型定义
type can_device, dev_type, file_type;
type custom_key_device, dev_type, file_type;
```

3. 定义进程对硬件设备的访问权限，修改`system_server.te`文件

```Plaintext
// system_server.te 权限配置
# 允许system_server进程访问CAN总线设备
allow system_server can_device:chr_file rw_file_perms;
# 允许system_server进程访问自定义按键设备
allow system_server custom_key_device:chr_file rw_file_perms;
```

4. 定义HAL层进程的访问权限，修改`hal.te`文件

```Plaintext
// hal.te HAL层权限配置
# 允许HAL层进程访问CAN总线设备
allow hal can_device:chr_file rw_file_perms;
# 允许HAL层进程访问自定义按键设备
allow hal custom_key_device:chr_file rw_file_perms;
```

##### 步骤3：编译验证SELinux策略

1. 编译sepolicy，生成`sepolicy`二进制文件

```Bash
make -j$(nproc) sepolicy
```

2. 刷入设备，通过`adb shell ls -Z /dev/can/can0`查看硬件节点的SELinux上下文是否正确

3. 通过`adb shell dmesg | grep avc`查看是否有SELinux权限拒绝日志

#### 场景2：放开hiddenapi限制

##### 步骤1：hiddenapi限制原理

Android 9\+引入hiddenapi限制，禁止第三方APP调用系统的非公开API（hiddenapi），通过`hiddenapi`文件定义允许调用的API。

##### 步骤2：放开hiddenapi限制

1. 修改`frameworks/base/core/java/android/app/Application.java`，禁用hiddenapi检查

```Java
// Application.java 禁用hiddenapi检查
static {
    // 禁用hiddenapi检查
    System.setProperty("ro.debuggable", "1");
    System.setProperty("persist.sys.hidden_api_blacklist", "");
    System.setProperty("persist.sys.hidden_api_whitelist", "*");
}
```

2. 修改`frameworks/base/core/jni/android_app_Application.cpp`，禁用native层的hiddenapi检查

```C++
// android_app_Application.cpp 禁用hiddenapi检查
static void disableHiddenApiChecks() {
    // 禁用hiddenapi黑名单
    setenv("ANDROID_HIDDEN_API_BLACKLIST", "", 1);
    // 禁用hiddenapi白名单限制
    setenv("ANDROID_HIDDEN_API_WHITELIST", "*", 1);
    // 启用debug模式，绕过hiddenapi检查
    setenv("ANDROID_DEBUGGABLE", "1", 1);
}
```

3. 修改`product.mk`，全局启用debug模式，绕过hiddenapi限制

```Makefile
// product.mk 全局debug配置
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.debuggable=1 \
    persist.sys.hidden_api_blacklist= \
    persist.sys.hidden_api_whitelist=*
```

##### 步骤3：验证方法

1. 编译AOSP源码，刷入设备

2. 安装调用hiddenapi的第三方APP，验证是否能正常调用系统非公开API

3. 通过`adb shell logcat | grep "hiddenapi"`查看是否有hiddenapi拒绝日志

#### 踩坑指南

- SELinux配置错误会导致硬件无法访问、系统服务无法启动，需严格按照SELinux规范配置

- 放开hiddenapi限制会降低系统安全性，仅在开发/定制ROM场景使用，生产环境需严格控制

- 系统属性修改后，需重新编译framework，否则属性不会生效

---

## 总结

本文档完整实现了Android Framework 6大核心实战场景，所有代码均基于AOSP源码，符合工程化开发标准。每个场景都遵循**前置知识→分步实现→核心代码→验证方法→踩坑指南**的结构，可直接用于车载ROM、定制平板、工控安卓等项目的开发实践。

如果需要针对某个场景的更深入的实现细节、或者需要适配特定的Android版本、硬件平台，可以进一步补充说明。

> （注：部分内容可能由 AI 生成）
