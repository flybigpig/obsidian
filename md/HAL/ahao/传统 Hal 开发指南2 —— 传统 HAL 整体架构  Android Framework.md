---
created: 2025-07-09T09:53:34 (UTC +08:00)
tags: []
source: http://ahaoframework.tech/006.Hal%E5%BC%80%E5%8F%91%E5%85%A5%E9%97%A8%E4%B8%8E%E5%AE%9E%E8%B7%B5/003.%E4%BC%A0%E7%BB%9F%20Hal%20%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%972%20%E2%80%94%E2%80%94%20%E4%BC%A0%E7%BB%9F%20HAL%20%E6%95%B4%E4%BD%93%E6%9E%B6%E6%9E%84.html
author: 
---
## 传统 Hal 开发指南2 —— 传统 HAL 整体架构

*阿豪* *3/19/2024*

本文基于 aosp android-7.0.0\_r1 版本源码讲解。

![20240419103259](https://cdn.jsdelivr.net/gh/stingerzou/MyImages@main/images20240419103259.png)

这里以振动器（Vibrator）为例，传统 HAL 的工作流程如下：

+   SystemServer 启动时，注册 Binder 服务 VibratorService
+   App 通过 ServiceManager 获取到 VibratorService 代理端对象
+   App 通过代理端对象，发起远程调用访问
+   VibratorService 通过 JNI 加载 HAL so 库，调用 HAL so 库中操作硬件的函数
+   HAL so 库中操作硬件的函数通过 `open ioctl mmap close` 等 Linux 系统调用访问到驱动程序

接下里我们通过 App 调用振动器（Vibrator）的流程来深入理解传统 HAL 的工作流程。

## [#](#_1-app-如何访问到硬件) 1. App 如何访问到硬件

App 通过以下代码就可以操作到振动器了：

```java
Vibrator vibrator = (Vibrator) getSystemService(VIBRATOR_SERVICE);
vibrator.vibrate(2000); //振动两秒
//关闭或者停止振动器
mVibrator.cancel();
//判断是否支持震动
mVibrator.hasVibrator()
```



当然执行代码之前需要在 AndroidManifest.xml 中申明振动器权限：

```xml
<uses-permission android:name="android.permission.VIBRATE" />
```



getSystemService 定义在 `Activity` 中：

```java
// frameworks/base/core/java/android/app/Activity.java

    @Override
    public Object getSystemService(@ServiceName @NonNull String name) {
        if (getBaseContext() == null) {
            throw new IllegalStateException(
                    "System services not available to Activities before onCreate()");
        }
        // WINDOW_SERVICE 和 SEARCH_SERVICE 都缓存在 Activity 的成员变量中
        if (WINDOW_SERVICE.equals(name)) {
            return mWindowManager;
        } else if (SEARCH_SERVICE.equals(name)) {
            ensureSearchManager();
            return mSearchManager;
        }
        // 接着调用父类的 getSystemService 方法
        return super.getSystemService(name);
    }
```



Activity 的父类 ContextThemeWrapper 中 getSystemService 方法实现如下：

```java
    @Override
    public Object getSystemService(String name) {
        // LAYOUT_INFLATER_SERVICE 特殊处理
        if (LAYOUT_INFLATER_SERVICE.equals(name)) { 
            if (mInflater == null) {
                mInflater = LayoutInflater.from(getBaseContext()).cloneInContext(this);
            }
            return mInflater;
        }
        // getBaseContext 返回的是 ContextImpl 对象
        return getBaseContext().getSystemService(name);
    }
```



接着调用 ContextImpl 对象的 getSystemService 方法：

```java
    @Override
    public Object getSystemService(String name) {
        return SystemServiceRegistry.getSystemService(this, name);
    }
```

 

终于到**重点**了，SystemServiceRegistry.getSystemService 的具体实现如下:

```java
// frameworks/base/core/java/android/app/SystemServiceRegistry.java
    private static final HashMap<String, ServiceFetcher<?>> SYSTEM_SERVICE_FETCHERS =
            new HashMap<String, ServiceFetcher<?>>();

    public static Object getSystemService(ContextImpl ctx, String name) {
        ServiceFetcher<?> fetcher = SYSTEM_SERVICE_FETCHERS.get(name);
        return fetcher != null ? fetcher.getService(ctx) : null;
    }
```



先从一个 Map 中获取到 ServiceFetcher，再从 ServiceFetcher 中获取到具体的 Service。

那这些数据是什么时候初始化的呢？ SystemServiceRegistry 中定义有一个静态块：

```java
// frameworks/base/core/java/android/app/SystemServiceRegistry.java

final class SystemServiceRegistry {
    //......
    static {
        // ......

        registerService(Context.UI_MODE_SERVICE, UiModeManager.class,
                new CachedServiceFetcher<UiModeManager>() {
            @Override
            public UiModeManager createService(ContextImpl ctx) {
                return new UiModeManager();
            }});

        // ......
        registerService(Context.VIBRATOR_SERVICE, Vibrator.class,
                new CachedServiceFetcher<Vibrator>() {
            @Override
            public Vibrator createService(ContextImpl ctx) {
                return new SystemVibrator(ctx);
            }});

        // ....

        registerService(Context.WALLPAPER_SERVICE, WallpaperManager.class,
                new CachedServiceFetcher<WallpaperManager>() {
            @Override
            public WallpaperManager createService(ContextImpl ctx) {
                return new WallpaperManager(ctx.getOuterContext(),
                        ctx.mMainThread.getHandler());
            }});
        // 多次调用 registerService，都省略了
    //......
}
```



在静态块中多次调用了 registerService 方法：

```java

    private static final HashMap<Class<?>, String> SYSTEM_SERVICE_NAMES =
            new HashMap<Class<?>, String>();
    private static final HashMap<String, ServiceFetcher<?>> SYSTEM_SERVICE_FETCHERS =
            new HashMap<String, ServiceFetcher<?>>();

    private static <T> void registerService(String serviceName, Class<T> serviceClass,
            ServiceFetcher<T> serviceFetcher) {
        SYSTEM_SERVICE_NAMES.put(serviceClass, serviceName);
        SYSTEM_SERVICE_FETCHERS.put(serviceName, serviceFetcher);
    }
```


registerService 实际就是把传入的数据插入到两个 map 中。

我们看下 VIBRATOR\_SERVICE 的注册过程：

```java
        registerService(Context.VIBRATOR_SERVICE, Vibrator.class,
                new CachedServiceFetcher<Vibrator>() {
            @Override
            public Vibrator createService(ContextImpl ctx) {
                return new SystemVibrator(ctx);
            }});
```



这里传入了一个匿名对象 CachedServiceFetcher，在第一次调用 CachedServiceFetcher 的 getService 方法时，会调用匿名对象中实现的 createService 方法去 new 一个 SystemVibrator。

回到最开始的 App 调用振动器的示例中 getSystemService 返回的就是一个 SystemVibrator 对象:

```java
// frameworks/base/core/java/android/os/SystemVibrator.java
public class SystemVibrator extends Vibrator {
    private static final String TAG = "Vibrator";

    private final IVibratorService mService;
    private final Binder mToken = new Binder();

    public SystemVibrator() {
        mService = IVibratorService.Stub.asInterface(
                ServiceManager.getService("vibrator"));
    }

    public SystemVibrator(Context context) {
        super(context);
        mService = IVibratorService.Stub.asInterface(
                ServiceManager.getService("vibrator"));
    }

    // ....
}
```



SystemVibrator 内部有一个 vibrator Binder 服务代理端对象 `IVibratorService mService`，SystemVibrator 的具体功能都是通过这个代理端对象发起远程过程调用实现的。

IVibratorService 服务通过 IVibratorService.aidl 文件实现：

```java
// frameworks/base/core/java/android/os/IVibratorService.aidl
interface IVibratorService
{
    boolean hasVibrator();
    void vibrate(int uid, String opPkg, long milliseconds, int usageHint, IBinder token);
    void vibratePattern(int uid, String opPkg, in long[] pattern, int repeat, int usageHint, IBinder token);
    void cancelVibrate(IBinder token);
}
```



对应的服务端实现是 VibratorService：

```java
// frameworks/base/services/core/java/com/android/server/VibratorService.java
public class VibratorService extends IVibratorService.Stub
        implements InputManager.InputDeviceListener {
    // ......
    native static boolean vibratorExists();
    native static void vibratorInit();
    native static void vibratorOn(long milliseconds);
    native static void vibratorOff(); 
    // ......
}
```



VibratorService 内部成员众多，涉及到其它的一些逻辑和功能（目前不关心），具体落实到硬件操作上（目前关心），都是通过上面的 4 个 native 方法来实现的。

那么这些 native 方法对应的 JNI 函数是哪个呢？这里这里通过分析 SystemServer 加载 android\_servers 库来揭晓答案。

## [#](#_2-systemserver-加载-android-servers-库) 2. SystemServer 加载 android\_servers 库

SystemServer 启动过程中会加载一个叫 android\_servers 的库

```java
// frameworks/base/services/java/com/android/server/SystemServer.java

public final class SystemServer {

// ......
    private void run() {
        // ......
        System.loadLibrary("android_servers"); 
        // ......      
    }
// ......
}
```



android\_servers 库定义在 `frameworks/base/services/Android.mk` 中：

```makefile
include $(CLEAR_VARS)

LOCAL_SRC_FILES :=
LOCAL_SHARED_LIBRARIES :=

# include all the jni subdirs to collect their sources
include $(wildcard $(LOCAL_PATH)/*/jni/Android.mk)

LOCAL_CFLAGS += -DEGL_EGLEXT_PROTOTYPES -DGL_GLEXT_PROTOTYPES

LOCAL_MODULE:= libandroid_servers

include $(BUILD_SHARED_LIBRARY)
```



这里把 `frameworks/base/services/core/jni/Android.mk` 文件包含了进来：

```makefile
# This file is included by the top level services directory to collect source
# files
LOCAL_REL_DIR := core/jni

LOCAL_CFLAGS += -Wall -Werror -Wno-unused-parameter

ifneq ($(ENABLE_CPUSETS),)
ifneq ($(ENABLE_SCHED_BOOST),)
LOCAL_CFLAGS += -DUSE_SCHED_BOOST
endif
endif

LOCAL_SRC_FILES += \
    $(LOCAL_REL_DIR)/com_android_server_AlarmManagerService.cpp \
    $(LOCAL_REL_DIR)/com_android_server_am_BatteryStatsService.cpp \
    $(LOCAL_REL_DIR)/com_android_server_am_ActivityManagerService.cpp \
    $(LOCAL_REL_DIR)/com_android_server_AssetAtlasService.cpp \
    $(LOCAL_REL_DIR)/com_android_server_connectivity_Vpn.cpp \
    $(LOCAL_REL_DIR)/com_android_server_ConsumerIrService.cpp \
    $(LOCAL_REL_DIR)/com_android_server_HardwarePropertiesManagerService.cpp \
    $(LOCAL_REL_DIR)/com_android_server_hdmi_HdmiCecController.cpp \
    $(LOCAL_REL_DIR)/com_android_server_input_InputApplicationHandle.cpp \
    $(LOCAL_REL_DIR)/com_android_server_input_InputManagerService.cpp \
    $(LOCAL_REL_DIR)/com_android_server_input_InputWindowHandle.cpp \
    $(LOCAL_REL_DIR)/com_android_server_lights_LightsService.cpp \
    $(LOCAL_REL_DIR)/com_android_server_location_GnssLocationProvider.cpp \
    $(LOCAL_REL_DIR)/com_android_server_location_FlpHardwareProvider.cpp \
    $(LOCAL_REL_DIR)/com_android_server_power_PowerManagerService.cpp \
    $(LOCAL_REL_DIR)/com_android_server_SerialService.cpp \
    $(LOCAL_REL_DIR)/com_android_server_SystemServer.cpp \
    $(LOCAL_REL_DIR)/com_android_server_tv_TvUinputBridge.cpp \
    $(LOCAL_REL_DIR)/com_android_server_tv_TvInputHal.cpp \
    $(LOCAL_REL_DIR)/com_android_server_vr_VrManagerService.cpp \
    $(LOCAL_REL_DIR)/com_android_server_UsbDeviceManager.cpp \
    $(LOCAL_REL_DIR)/com_android_server_UsbMidiDevice.cpp \
    $(LOCAL_REL_DIR)/com_android_server_UsbHostManager.cpp \
    $(LOCAL_REL_DIR)/com_android_server_VibratorService.cpp \
    $(LOCAL_REL_DIR)/com_android_server_PersistentDataBlockService.cpp \
    $(LOCAL_REL_DIR)/onload.cpp

LOCAL_C_INCLUDES += \
    $(JNI_H_INCLUDE) \
    frameworks/base/services \
    frameworks/base/libs \
    frameworks/base/libs/hwui \
    frameworks/base/core/jni \
    frameworks/native/services \
    libcore/include \
    libcore/include/libsuspend \
    system/security/keystore/include \
    $(call include-path-for, libhardware)/hardware \
    $(call include-path-for, libhardware_legacy)/hardware_legacy \

LOCAL_SHARED_LIBRARIES += \
    libandroid_runtime \
    libandroidfw \
    libbinder \
    libcutils \
    liblog \
    libhardware \
    libhardware_legacy \
    libkeystore_binder \
    libnativehelper \
    libutils \
    libui \
    libinput \
    libinputflinger \
    libinputservice \
    libsensorservice \
    libskia \
    libgui \
    libusbhost \
    libsuspend \
    libEGL \
    libGLESv2 \
    libnetutils \
```


这里就是把 `frameworks/base/services/core/jni` 目录下的 cpp 文件编译打包成 libandroid\_servers.so 库。

其中 onload.cpp 中有一个 JNI\_OnLoad 函数，Java 层调用 loadLibrary 加载 so 库的过程中，会执行到 JNI\_OnLoad 函数：

```cpp
// frameworks/base/services/core/jni/onload.cpp

extern "C" jint JNI_OnLoad(JavaVM* vm, void* /* reserved */)
{
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("GetEnv failed!");
        return result;
    }
    ALOG_ASSERT(env, "Could not retrieve the env!");

    register_android_server_ActivityManagerService(env);
    register_android_server_PowerManagerService(env);
    register_android_server_SerialService(env);
    register_android_server_InputApplicationHandle(env);
    register_android_server_InputWindowHandle(env);
    register_android_server_InputManager(env);
    register_android_server_LightsService(env);
    register_android_server_AlarmManagerService(env);
    register_android_server_UsbDeviceManager(env);
    register_android_server_UsbMidiDevice(env);
    register_android_server_UsbHostManager(env);
    register_android_server_vr_VrManagerService(env);
    register_android_server_VibratorService(env);
    register_android_server_SystemServer(env);
    register_android_server_location_GnssLocationProvider(env);
    register_android_server_location_FlpHardwareProvider(env);
    register_android_server_connectivity_Vpn(env);
    register_android_server_AssetAtlasService(env);
    register_android_server_ConsumerIrService(env);
    register_android_server_BatteryStatsService(env);
    register_android_server_hdmi_HdmiCecController(env);
    register_android_server_tv_TvUinputBridge(env);
    register_android_server_tv_TvInputHal(env);
    register_android_server_PersistentDataBlockService(env);
    register_android_server_Watchdog(env);
    register_android_server_HardwarePropertiesManagerService(env);


    return JNI_VERSION_1_4;
}
```



这里会 register 很多 Service，我们看看我们关心的振动器相关的函数： `register_android_server_VibratorService`：

```cpp
// frameworks/base/services/core/jni/com_android_server_VibratorService.cpp
int register_android_server_VibratorService(JNIEnv *env)
{
    return jniRegisterNativeMethods(env, "com/android/server/VibratorService",
            method_table, NELEM(method_table));
}

static const JNINativeMethod method_table[] = {
    { "vibratorExists", "()Z", (void*)vibratorExists },
    { "vibratorInit", "()V", (void*)vibratorInit },
    { "vibratorOn", "(J)V", (void*)vibratorOn },
    { "vibratorOff", "()V", (void*)vibratorOff }
};
```



可以看出这里实际就是注册 JNI 函数。让 Java 层的 Native 方法能找到对应的 JNI 层函数。上一节的问题的答案就在这里了。

## [#](#_3-hal-库加载过程) 3. HAL 库加载过程

接下来的问题就是这些 native 函数是如何加载 HAL 层的，在了解加载过程之前，我们需要先了解 HAL 的组成。

### [#](#_3-1-两个重要的数据结构) 3.1 两个重要的数据结构

在 HAL 中，`struct hw_module_t` 结构体用于描述一类硬件抽象模块，每个硬件抽象模块都对应一个动态链接库（so 库）。每一类硬件抽象模块又包含多个独立的硬件设备，HAL使用 `hw_device_t` 结构体描述硬件模块中的独立硬件设备。

![20240313160522](https://cdn.jsdelivr.net/gh/stingerzou/MyImages@main/images20240313160522.png)

接下来分析 JNI 层加载 Hal 的过程。

### [#](#_3-2-模块的名字与位置) 3.2 模块的名字与位置

每一个硬件抽象硬件模块都对应一个动态链接库（so 库），so 库的名字通常为 `<MODULE_ID>.variant.so`：

+   MODULE\_ID 表示模块的 ID，通常是一个字符串，比如
    
    ```c
    // GPS 模块的 MODULE_ID 就是字符串 “gps”
    #define GPS_HARDWARE_MODULE_ID "gps"
    // 振动器模块
    #define VIBRATOR_HARDWARE_MODULE_ID "vibrator"
    ```
   
+   variant 可以是 `ro.hardware, ro.product.board, ro.board.platform, ro.arch` 四个系统属性值之一，系统会依次从属性系统中读取这四个值，如果读取到了，variant 的值就是对应的属性值，就不在读取后面的了。如果四个属性值都不存在，variant 的值为 default。

知道了 so 库的名字，我们还要知道 so 库存放在哪个路径。HAL规定了 3 个硬件模块动态共享库的存放路径，定义在 `/hardware/libhardware/hardware.c`：

```c
#if defined(__LP64__)
#define HAL_LIBRARY_PATH1 "/system/lib64/hw"
#define HAL_LIBRARY_PATH2 "/vendor/lib64/hw"
#define HAL_LIBRARY_PATH3 "/odm/lib64/hw"
#else
#define HAL_LIBRARY_PATH1 "/system/lib/hw"
#define HAL_LIBRARY_PATH2 "/vendor/lib/hw"
#define HAL_LIBRARY_PATH3 "/odm/lib/hw"
#endif
```



HAL 在加载所需的共享库之前，会调用 hw\_module\_exists 函数检查 HAL\_LIBRARY\_PATH3 路径下面是否存在目标库；如果没有，继续检查 HAL\_LIBRARY\_PATH2 路径下面是否存在，如果没有，继续检查 HAL\_LIBRARY\_PATH2 路径下面是否存在。 具体实现在函数 :

```c
// hardware/libhardware/hardware.c

static int hw_module_exists(char *path, size_t path_len, const char *name,
                            const char *subname)
{   
    snprintf(path, path_len, "%s/%s.%s.so",
             HAL_LIBRARY_PATH3, name, subname);
    if (access(path, R_OK) == 0)
        return 0;

    snprintf(path, path_len, "%s/%s.%s.so",
             HAL_LIBRARY_PATH2, name, subname);
    if (access(path, R_OK) == 0)
        return 0;

    snprintf(path, path_len, "%s/%s.%s.so",
             HAL_LIBRARY_PATH1, name, subname);
    if (access(path, R_OK) == 0)
        return 0;

    return -ENOENT;
}
```



### [#](#_3-3-vibrator-hal-加载源码分析) 3.3 Vibrator HAL 加载源码分析

对于振动器，Framework 层会通过 JNI 调用 vibratorInit 函数来初始化振动器：

```cpp
// frameworks/base/services/core/jni/com_android_server_VibratorService.cpp

static hw_module_t *gVibraModule = NULL;
static vibrator_device_t *gVibraDevice = NULL;

static void vibratorInit(JNIEnv /* env */, jobject /* clazz */)
{
    if (gVibraModule != NULL) {
        return;
    }

    int err = hw_get_module(VIBRATOR_HARDWARE_MODULE_ID, (hw_module_t const**)&gVibraModule);

    if (err) {
        ALOGE("Couldn't load %s module (%s)", VIBRATOR_HARDWARE_MODULE_ID, strerror(-err));
    } else {
        if (gVibraModule) {
            vibrator_open(gVibraModule, &gVibraDevice);
        }
    }
}
```



vibratorInit 函数中会调用到 hw\_get\_module 函数来加载 HAL 模块并获取到 hw\_module\_t 结构体实例，接着调用 vibrator\_open 函数获取到振动器对应的 vibrator\_device\_t 结构体实例。后续就可以通过这个 vibrator\_device\_t 结构体实例来调用驱动程序从而控制到振动器硬件了。

我们首先来看看 hw\_get\_module 函数的具体实现

```c
// hardware/libhardware/hardware.c
int hw_get_module(const char *id, const struct hw_module_t **module)
{
    return hw_get_module_by_class(id, NULL, module);
}
```



hw\_get\_module 会接着调用 hw\_get\_module\_by\_class 函数：

```c
// hardware/libhardware/hardware.c
// 传入的 class_id 是  VIBRATOR_HARDWARE_MODULE_ID
// #define VIBRATOR_HARDWARE_MODULE_ID "vibrator"
// inst 是 null
int hw_get_module_by_class(const char *class_id, const char *inst,
                           const struct hw_module_t **module)
{
    int i = 0;
    char prop[PATH_MAX] = {0};
    char path[PATH_MAX] = {0};
    char name[PATH_MAX] = {0};
    char prop_name[PATH_MAX] = {0};

    //根据 id 生成 module name
    if (inst)
        snprintf(name, PATH_MAX, "%s.%s", class_id, inst);
    else // 走 else 分支
        strlcpy(name, class_id, PATH_MAX);

    // 生成的 name 实际就是传入的 class_id “vibrator”
    // 拼凑出的 prop_name 的值为 ro.hardware.vibrator
    snprintf(prop_name, sizeof(prop_name), "ro.hardware.%s", name);

    // 查找属性 ro.hardware.vibrator 属性值
    if (property_get(prop_name, prop, NULL) > 0) {
        if (hw_module_exists(path, sizeof(path), name, prop) == 0) {
            goto found;
        }
    }

    // static const char *variant_keys[] = {
    //     "ro.hardware",  /* This goes first so that it can pick up a different
    //                    file on the emulator. */
    //     "ro.product.board",
    //     "ro.board.platform",
    //     "ro.arch"
    // };
    
    // 遍历 variant_keys 数组
    /* Loop through the configuration variants looking for a module */
    for (i=0 ; i<HAL_VARIANT_KEYS_COUNT; i++) {
        if (property_get(variant_keys[i], prop, NULL) == 0) {
            continue;
        }
        if (hw_module_exists(path, sizeof(path), name, prop) == 0) {
            goto found;
        }
    }

    /* Nothing found, try the default */
    if (hw_module_exists(path, sizeof(path), name, "default") == 0) {
        goto found;
    }

    return -ENOENT;

found:
    // 前面就是确定 so 库是否存在，如果存在就调用 load 函数加载 so 库。
    /* load the module, if this fails, we're doomed, and we should not try
     * to load a different variant. */、
    // load 函数加载 so 库，从 so 库中查找 hw_module_t 结构体
    return load(class_id, path, module);
}
```



hw\_get\_module\_by\_class 函数参数：

+   const char \*class\_id：GVIBRATOR\_HARDWARE\_MODULE\_ID，这是一个 define 常量：`#define VIBRATOR_HARDWARE_MODULE_ID "vibrator"`
+   inst 是 null
+   const struct hw\_module\_t \*\*module：作为返回值

函数执行的具体流程：

+   拼凑出 `ro.hardware.vibrator` 属性名，从属性系统中获取 `ro.hardware.vibrator` 对应的属性值
+   如果没有获取到，接着遍历 variant\_keys 数组，依次从属性系统中获取每个数组成员对应的属性值
+   如果获取到了属性值，就把属性值传入下一步中的 hw\_module\_exists 函数的最后一个参数，如果没有获取到属性值，就将 "default" 字符串传入 hw\_module\_exists 函数的最后一个参数.
+   通过命令行我们可以查看模拟器中具体的属性值，我们这里实际传入 hw\_module\_exists 函数的最后一个参数是 goldfish
    
    ```bash
    getprop ro.hardware
    goldfish
    ```
    
    1  
    2  
    

hw\_module\_exists 函数我们在前面分析过了，它会根据参数拼凑出 so 文件的完整路径，然后判断这个文件是否存在。如果没有对应的 so 文件就直接返回了，如果有，就接着调用 load 函数加载 so 文件。

接着我们就来看 load 函数是如何加载 so 文件的：

```c
// hardware/libhardware/hardware.c
static int load(const char *id,
        const char *path,
        const struct hw_module_t **pHmi)
{
    int status = -EINVAL;
    void *handle = NULL;
    struct hw_module_t *hmi = NULL;

    /*
     * load the symbols resolving undefined symbols before
     * dlopen returns. Since RTLD_GLOBAL is not or'd in with
     * RTLD_NOW the external symbols will not be global
     */ 
    handle = dlopen(path, RTLD_NOW); // dlopen 打开 so 库
    if (handle == NULL) {
        char const *err_str = dlerror();
        ALOGE("load: module=%s\n%s", path, err_str?err_str:"unknown");
        status = -EINVAL;
        goto done;
    }

    /* Get the address of the struct hal_module_info. */

    // #define HAL_MODULE_INFO_SYM_AS_STR  "HMI"
    // #define HAL_MODULE_INFO_SYM         HMI
    const char *sym = HAL_MODULE_INFO_SYM_AS_STR; 
    hmi = (struct hw_module_t *)dlsym(handle, sym); // 从 so 库中查找 HMI 符号
    if (hmi == NULL) {
        ALOGE("load: couldn't find symbol %s", sym);
        status = -EINVAL;
        goto done;
    }

    /* Check that the id matches */
    if (strcmp(id, hmi->id) != 0) {
        ALOGE("load: id=%s != hmi->id=%s", id, hmi->id);
        status = -EINVAL;
        goto done;
    }

    hmi->dso = handle;

    /* success */
    status = 0;

    done:
    if (status != 0) {
        hmi = NULL;
        if (handle != NULL) {
            dlclose(handle);
            handle = NULL;
        }
    } else {
        ALOGV("loaded HAL id=%s path=%s hmi=%p handle=%p",
                id, path, *pHmi, handle);
    }

    *pHmi = hmi;

    return status;
}
```



load 函数的具体流程如下：

+   先通过 dlopen 打开 so 文件
+   然后通过 dlsym 加载一个名为 HAL\_MODULE\_INFO\_SYM\_AS\_STR 的符号，HAL\_MODULE\_INFO\_SYM\_AS\_STR 是一个 define 常量：`#define HAL_MODULE_INFO_SYM_AS_STR "HMI"`
+   在 vibrator HAL 中 HMI 符号的定义如下：

```c
// hardware/libhardware/modules/vibrator/vibrator.c
// #define HAL_MODULE_INFO_SYM         HMI
struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .module_api_version = VIBRATOR_API_VERSION,
    .hal_api_version = HARDWARE_HAL_API_VERSION,
    .id = VIBRATOR_HARDWARE_MODULE_ID,
    .name = "Default vibrator HAL",
    .author = "The Android Open Source Project",
    .methods = &vibrator_module_methods,
};
```



+   获取到 HMI 符号好，赋值给 pHmi 参数，返回给上级

目前为止，我们获取到了 vibrator HAL 对应的 hw\_module\_t 结构体实例：

```c
typedef struct hw_module_t {
    uint32_t tag;
    uint16_t module_api_version;
#define version_major module_api_version
    uint16_t hal_api_version;
#define version_minor hal_api_version
    const char *id;
    const char *name;
    const char *author;
    struct hw_module_methods_t* methods;
    void* dso;

#ifdef __LP64__
    uint64_t reserved[32-7];
#else
    uint32_t reserved[32-7];
#endif

} hw_module_t;
```



hw\_module\_t 中有个重要的成员 hw\_module\_methods\_t：

```c
typedef struct hw_module_methods_t {
    /** Open a specific device */
    int (*open)(const struct hw_module_t* module, const char* id,
            struct hw_device_t** device);

} hw_module_methods_t;
```



hw\_module\_methods\_t 结构体中有个函数指针 open， 这个 open 函数用于获取到我们需要操作的设备对应的 hw\_device\_t 结构体。对应于 vibrator HAL，这个 open 指针指向 vibra\_open 函数：

```c
// hardware/libhardware/modules/vibrator/vibrator.c

static struct hw_module_methods_t vibrator_module_methods = {
    .open = vibra_open,
};

static int vibra_open(const hw_module_t* module, const char* id __unused,
                      hw_device_t** device __unused) {
    if (!vibra_exists()) {
        ALOGE("Vibrator device does not exist. Cannot start vibrator");
        return -ENODEV;
    }

    vibrator_device_t *vibradev = calloc(1, sizeof(vibrator_device_t));

    if (!vibradev) {
        ALOGE("Can not allocate memory for the vibrator device");
        return -ENOMEM;
    }

    vibradev->common.tag = HARDWARE_DEVICE_TAG;
    vibradev->common.module = (hw_module_t *) module;
    vibradev->common.version = HARDWARE_DEVICE_API_VERSION(1,0);
    vibradev->common.close = vibra_close;

    vibradev->vibrator_on = vibra_on;
    vibradev->vibrator_off = vibra_off;

    *device = (hw_device_t *) vibradev;

    return 0;
}
```



这里实际获取到的是 hw\_module\_t 的"子类" vibrator\_device\_t：

```c
typedef struct vibrator_device {

    struct hw_device_t common;
    int (*vibrator_on)(struct vibrator_device* vibradev, unsigned int timeout_ms);
    int (*vibrator_off)(struct vibrator_device* vibradev);
} vibrator_device_t;
```



这里，hw\_device\_t 是 vibrator\_device 的第一个成员，也就是说，指向 vibrator\_device 的指针与指向 hw\_device\_t 的指针可以相互转换。这也是 C 语言中，常用的的结构体“继承”写法。

获取到 vibrator\_device 结构体后，就可以调用 vibrator\_device 内部的 vibrator\_on vibrator\_off 函数来控制具体的硬件了。

这些函数的内部实现就是通过 `open close mmap ioctl` 等系统调用访问具体的设备驱动程序。具体的驱动如何访问，不是我们关心的重点，我们就不在深入细节分析了，这部分通常驱动开发的同事会给出详细的文档。

## [#](#参考资料) 参考资料

+   [Android硬件抽象层HAL总结 (opens new window)](https://www.jianshu.com/p/0d155f267589)