# Android Framework 实战开发指南

## 一、核心分层

| 层级 | 说明 | 关键技术 |
|------|------|----------|
| **Application** | 系统应用 / 三方应用 | Java/Kotlin, SDK |
| **Framework Java** | System Server, AMS, WMS, PMS 等 | Java, AIDL, Binder |
| **Native (C++)** | SurfaceFlinger, MediaServer, AudioFlinger | C++, Binder, HIDL |
| **HAL** | 硬件抽象层 | C/C++, HIDL/AIDL HAL |
| **Linux Kernel** | 驱动, 内存管理, 进程调度 | C, Device Tree |

---

## 二、实战常见场景

### 1. 添加新的 System Service

```
frameworks/base/core/java/android/app/
frameworks/base/services/core/java/com/android/server/
```

- 定义 AIDL 接口 (`.aidl`)
- 实现 Binder Service (继承 `IIpcService.Stub`)
- 注册到 SystemServer (`SystemServiceManager`)
- 生成 `context.xml` 权限声明

### 2. 修改 WindowManager / 窗口策略

- `PhoneWindowManager.java` — 按键/手势拦截
- `WindowState.java` — 窗口层级与布局
- `DisplayContent.java` — 多屏/分屏策略

### 3. 定制开机动画 / BootAnimation

- `frameworks/base/cmds/bootanimation/`
- 替换 `bootanimation.zip` (part0/part1 帧序列 + desc.txt)

### 4. 修改 SystemUI (状态栏/通知栏/锁屏)

- `frameworks/base/packages/SystemUI/`
- Kotlin/Java 开发，类似普通 App 但依赖系统签名

### 5. 输入系统 (InputManager)

- `frameworks/native/services/inputflinger/`
- `InputReader` → `InputDispatcher` → Window 分发
- 可在此层做按键重映射、手势注入

### 6. 权限管理

- `frameworks/base/services/core/java/com/android/server/pm/permission/`
- PermissionManagerService, PermissionPolicy
- 可添加自定义权限或修改运行时权限策略

### 7. 添加系统 App 源码

系统 App 指的是随系统镜像一起编译进 `/system/app/` 或 `/system/priv-app/` 的应用，**与普通三方 App 最大的区别**是：共享系统 `uid`、拥有系统签名、可调用 `@hide` API。

#### 7.1 目录约定

```
AOSP/
├── packages/apps/                  # 普通系统 App (Settings, Camera, Music …)
│   └── HelloSystemApp/
│       ├── Android.mk
│       ├── AndroidManifest.xml
│       ├── src/com/example/hello/
│       │   └── MainActivity.java
│       └── res/
└── vendor/<vendor>/packages/apps/  # 厂商自定义系统 App
```

> `priv-app/` 下的 App 权限更高，可见未导出的系统 API，建议把需要特权的 App 放在这里。

#### 7.2 AndroidManifest.xml 关键配置

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.hellosystemapp"
    coreApp="true"
    android:sharedUserId="android.uid.system"
    android:versionCode="1"
    android:versionName="1.0">

    <!-- 静态注册广播 -->
    <uses-permission android:name="android.permission.RECEIVE_BOOT_COMPLETED" />

    <application
        android:label="@string/app_name"
        android:icon="@mipmap/ic_launcher"
        android:allowBackup="false"
        android:persistent="true">

        <activity
            android:name=".MainActivity"
            android:exported="true"
            android:process=":main"
            android:theme="@android:style/Theme.Material.Light">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
```

要点：
- `android:sharedUserId="android.uid.system"` —— 共享 system 进程的 uid，能调用 `@hide` API
- `coreApp="true"` —— 标记为核心系统 App
- `android:persistent="true"` —— 开机自启，进程被杀会被 AMS 拉起

#### 7.3 构建脚本（Android.mk）

```makefile
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res
LOCAL_PACKAGE_NAME := HelloSystemApp

# 关键：使用平台 key 签名
LOCAL_CERTIFICATE := platform

# 可选：链接 framework.jar (含 @hide API)
LOCAL_JAVA_LIBRARIES := framework

# 静态 Java 库依赖
LOCAL_STATIC_JAVA_LIBRARIES := \
    android-support-v4

include $(BUILD_PACKAGE)
```

> Android 10+ 推荐用 `Android.bp`：
> ```bp
> android_app {
>     name: "HelloSystemApp",
>     srcs: ["src/**/*.java"],
>     resource_dirs: ["res"],
>     certificate: "platform",
>     static_libs: ["android-support-v4"],
>     platform_apis: true,
> }
> ```

#### 7.4 集成到系统镜像

在 **device.mk / product.mk** 中加入：

```makefile
PRODUCT_PACKAGES += \
    HelloSystemApp
```

这样 `make systemimage` 时会自动把 APK 拷贝到 `out/.../system/app/HelloSystemApp/`。

#### 7.5 SELinux 权限（Android 8.0+ 必须）

`system/sepolicy/private/system_app.te` 或自定义 `.te`：

```te
type system_app, domain;
type system_app_exec, exec_type, vendor_file_type, file_type;

# 允许 system_app 调用自定义 Service
binder_call(system_app, my_hal_service)

# 允许访问 /dev/mydev
allow system_app my_device:chr_file rw_file_perms;
```

并在 `file_contexts` 中标注 APK 路径：

```
/system/app/HelloSystemApp(/.*)?     u:object_r:system_app_exec:s0
```

#### 7.6 编译 & 调试

```bash
# 单独编译模块
mmm packages/apps/HelloSystemApp

# 推送到设备
adb root
adb remount
adb push out/target/product/<device>/system/app/HelloSystemApp/HelloSystemApp.apk \
    /system/app/HelloSystemApp/
adb shell am start -n com.example.hellosystemapp/.MainActivity

# 查看日志（注意：系统 App 跑在 system 进程）
adb logcat -s SystemApp:V ActivityManager:I
```

#### 7.7 常见坑

| 现象 | 原因 | 解决办法 |
|------|------|----------|
| 编译报 `requires libraries that are not visible to the system` | 未使用 `platform` 签名 / 未链接 `framework` | `LOCAL_CERTIFICATE := platform` + `LOCAL_JAVA_LIBRARIES := framework` |
| 安装后启动崩溃 `SecurityException` | 缺少 SELinux 规则 | 在 `system_app.te` 追加 allow 规则 |
| App 不出现在 Launcher | `category.LAUNCHER` 没写 | 检查 `intent-filter` |
| `persistent=true` 但开机没拉起 | 没在 `coreApp` 或被 LowMemoryKiller 杀 | 检查 `oom_adj` 与 `coreApp` |
| 调用 `@hide` API 报找不到 | 普通 SDK 看不到 | 改用 `LOCAL_JAVA_LIBRARIES := framework` 并用源码全编 |

---

## 三、开发环境搭建

```bash
# 1. 下载 AOSP (Android 14+ 推荐)
repo init -u https://android.googlesource.com/platform/manifest -b android-14.0.0_r30
repo sync -j8

# 2. 编译
source build/envsetup.sh
lunch aosp_<device>-userdebug
make -j16

# 3. 单模块编译 (快速迭代)
mma frameworks/base            # 编译 framework.jar
mmm packages/apps/Settings     # 编译 Settings

# 4. 推送
adb remount
adb push out/target/product/<device>/system/framework/framework.jar /system/framework/
adb reboot
```

---

## 四、推荐学习路线

| 阶段 | 内容 | 关键源码路径 |
|------|------|-------------|
| **入门** | Activity 启动流程、Binder 通信 | `ActivityManagerService.java`, `Binder.java` |
| **进阶** | WMS 窗口管理、SurfaceFlinger | `WindowManagerService.java`, `SurfaceFlinger.cpp` |
| **高级** | 性能优化（Systrace/Perfetto）、低内存管理(LMK) | `LowMemoryKiller.java`, `lmkd` |
| **精通** | SElinux 策略、Treble 架构、OTA 升级 | `system/sepolicy/`, `update_engine/` |

---

## 五、经典书籍与资源

| 书籍/资源 | 说明 |
|-----------|------|
| **《Android Framework 开发实战》** | 国内系统性教材，覆盖 Service/WMS/PMS 实战 |
| **《深入理解 Android》** 卷 I~III | 邓凡平著，经典源码分析 |
| **《Android 系统源代码分析》** | 罗升阳博客整理版 |
| **AOSP 官方文档** | [source.android.com](https://source.android.com) |
| **Android Source (CSDN/掘金)** | 大量中文实战文章 |

---

## 六、调试利器

```bash
# 抓取 Framework 层日志
logcat -s ActivityManager WindowManager PackageManager

# dumpsys 查看系统服务状态
dumpsys activity activities    # Activity 栈
dumpsys window displays        # 窗口信息
dumpsys package packages       # 包信息

# Binder 信息
dumpsys service list
dumpsys meminfo

# Systrace 分析性能
python system/core/atrace/atrace.py -b 4096 -c gfx input view am wm
```
