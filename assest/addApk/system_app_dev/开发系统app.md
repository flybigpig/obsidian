The user is asking about developing system apps using Android Studio. This is a different topic from the kernel native service. Let me provide a comprehensive guide on developing system apps (privileged apps with system signature or privileged permissions) using Android Studio.

A "system app" in Android context means:
1. An app that's pre-installed in `/system/priv-app/` or `/system/app/`
2. Has access to system/signature-level permissions
3. Can use hidden APIs, internal APIs
4. May need to be signed with platform keys

Let me write a comprehensive guide.

好的，下面是 **Android Studio 开发系统 App** 的完整指南。

---

# Android Studio 开发系统 App 完整指南

## 1. 什么是系统 App

| 类型 | 安装路径 | 签名 | 权限级别 | 示例 |
|------|---------|------|---------|------|
| **普通 App** | `/data/app/` | 开发者自签名 | 普通 | 第三方应用 |
| **预装 App** | `/system/app/` | 平台签名或自签名 | 普通 | 计算器、日历 |
| **特权 App** | `/system/priv-app/` | 平台签名 | signature\|privileged | Settings、SystemUI |
| **平台签名 App** | 任意路径 | 平台签名 (platform.pk8) | signature | Phone、System |

> **系统 App = 预装到 `/system` 分区 + 平台签名 + `android:sharedUserId="android.uid.system"`**

## 2. 整体架构

```
flowchart TB
    AS["Android Studio<br/>开发 & 调试"] -->|gradle assemble| APK["系统 APK"]
    APK -->|adb install -r| TEST["临时测试<br/>(权限受限)"]
    APK -->|push 到 /system/priv-app/| PROD["正式系统 App<br/>(完整权限)"]
    PROD -->|platform.pk8 签名| SIGN["平台签名 APK"]
```

## 3. 项目配置

### 3.1 build.gradle (app 级)

```groovy
android {
    compileSdk 34

    defaultConfig {
        applicationId "com.mycompany.systemapp"
        minSdk 29
        targetSdk 34
    }

    signingConfigs {
        platform {
            // 方式1: 直接引用平台密钥文件
            storeFile file("../keys/platform.jks")
            storePassword "android"
            keyAlias "platform"
            keyPassword "android"
        }
    }

    buildTypes {
        release {
            signingConfig signingConfigs.platform
            minifyEnabled false
        }
        debug {
            signingConfig signingConfigs.platform
        }
    }
}
```

### 3.2 AndroidManifest.xml

```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:tools="http://schemas.android.com/tools"
    package="com.mycompany.systemapp"
    android:sharedUserId="android.uid.system"
    android:process="system">

    <!-- 系统级权限 -->
    <uses-permission android:name="android.permission.INTERACT_ACROSS_USERS_FULL" />
    <uses-permission android:name="android.permission.WRITE_SECURE_SETTINGS" />
    <uses-permission android:name="android.permission.MANAGE_USERS" />
    <uses-permission android:name="android.permission.REBOOT"
        tools:ignore="ProtectedPermissions" />
    <uses-permission android:name="android.permission.FORCE_STOP_PACKAGES" />
    <uses-permission android:name="android.permission.CONNECTIVITY_INTERNAL" />
    <uses-permission android:name="android.permission.READ_PRIVILEGED_PHONE_STATE" />

    <application
        android:allowBackup="false"
        android:icon="@mipmap/ic_launcher"
        android:label="@string/app_name"
        android:supportsRtl="true"
        android:theme="@style/Theme.SystemApp"
        android:persistent="true"
        tools:ignore="MissingClass">

        <activity
            android:name=".MainActivity"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>

    </application>
</manifest>
```

关键属性说明：

| 属性 | 作用 |
|------|------|
| `android:sharedUserId="android.uid.system"` | 以 system UID 运行，获得系统权限 |
| `android:process="system"` | 运行在 system_server 进程（可选，慎用） |
| `android:persistent="true"` | 开机自启，被杀后自动重启 |

## 4. 生成平台签名密钥

### 4.1 从 AOSP 源码转换

AOSP 的平台签名密钥在 `build/target/product/security/` 下：

```bash
# platform.pk8 + platform.x509.pem → platform.jks

# 1. pk8 → pk12
openssl pkcs8 -in platform.pk8 -inform DER -outform PEM -out platform.pem -nocrypt

# 2. pem + x509 → p12
openssl pkcs12 -export -in platform.x509.pem -inkey platform.pem -out platform.p12 \
    -name platform -password pass:android

# 3. p12 → jks
keytool -importkeystore -srckeystore platform.p12 -srcstoretype PKCS12 \
    -srcstorepass android -destkeystore platform.jks -deststoretype JKS \
    -deststorepass android -srcalias platform -destalias platform \
    -srckeypass:android -destkeypass android
```

### 4.2 将 platform.jks 放到项目根目录

```
MySystemApp/
├── keys/
│   └── platform.jks
├── app/
│   ├── build.gradle
│   └── src/
└── build.gradle
```

## 5. 使用隐藏 API (@hide)

系统 App 经常需要调用 framework 的 `@hide` API。有三种方式：

### 5.1 方式一：android.jar 替换（推荐）

从编译产物中取出包含隐藏 API 的 framework jar：

```bash
# AOSP 编译产物路径
cp out/target/common/obj/JAVA_LIBRARIES/framework_intermediates/classes.jar \
   ~/MySystemApp/libs/framework.jar
```

**`app/build.gradle`**

```groovy
android {
    // ...

    // 用完整 framework.jar 替换 SDK 的 android.jar
    gradle.projectsEvaluated {
        tasks.withType(JavaCompile) {
            Set<File> pathSet = options.bootstrapClasspath.files
            pathSet = files("$projectDir/libs/framework.jar") + pathSet
            options.bootstrapClasspath = files(pathSet)
        }
    }
}
```

### 5.2 方式二：反射调用

```java
public class HiddenApiBridge {

    public static void setStayOnWhilePlugged(Context ctx, int mode) {
        try {
            Settings.Global.putInt(ctx.getContentResolver(),
                    Settings.Global.STAY_ON_WHILE_PLUGGED_IN, mode);
        } catch (Exception e) {
            try {
                Method method = Settings.Global.class.getMethod("putInt",
                        ContentResolver.class, String.class, int.class);
                method.invoke(null, ctx.getContentResolver(),
                        "stay_on_while_plugged_in", mode);
            } catch (Exception ex) {
                Log.e("HiddenApi", "setStayOnWhilePlugged failed", ex);
            }
        }
    }

    public static String getSystemProperty(String key, String def) {
        try {
            Class<?> cls = Class.forName("android.os.SystemProperties");
            Method method = cls.getMethod("get", String.class, String.class);
            return (String) method.invoke(null, key, def);
        } catch (Exception e) {
            return def;
        }
    }
}
```

### 5.3 方式三：SDK 生成 stub

```bash
# 从 AOSP 生成包含 @hide API 的 SDK stub
make sdk -j$(nproc)
# 产物: out/target/common/obj/PACKAGING/android_jar_intermediates/android.jar
```

## 6. 调用系统服务

### 6.1 调用已注册的 Framework 服务

```java
import android.os.ServiceManager;
import android.os.IBinder;

// 获取服务 binder
IBinder binder = ServiceManager.getService("custom");
if (binder != null) {
    // 通过 AIDL Stub 转换
    IMyCustomService svc = IMyCustomService.Stub.asInterface(binder);
    svc.setValue("hello", "world");
}
```

### 6.2 直接使用反射调用 ServiceManager

```java
public class ServiceBridge {

    public static IBinder getService(String name) {
        try {
            Class<?> cls = Class.forName("android.os.ServiceManager");
            Method method = cls.getMethod("getService", String.class);
            return (IBinder) method.invoke(null, name);
        } catch (Exception e) {
            Log.e("ServiceBridge", "getService failed", e);
            return null;
        }
    }

    public static String[] listServices() {
        try {
            Class<?> cls = Class.forName("android.os.ServiceManager");
            Method method = cls.getMethod("listServices");
            return (String[]) method.invoke(null);
        } catch (Exception e) {
            return new String[0];
        }
    }
}
```

### 6.3 执行 Shell 命令

```java
public class ShellExecutor {

    public static String exec(String command) {
        try {
            Process process = Runtime.getRuntime().exec(new String[]{"sh", "-c", command});
            BufferedReader reader = new BufferedReader(
                    new InputStreamReader(process.getInputStream()));
            StringBuilder output = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                output.append(line).append("\n");
            }
            process.waitFor();
            return output.toString();
        } catch (Exception e) {
            Log.e("ShellExecutor", "exec failed", e);
            return "";
        }
    }

    // 需要 android.permission.INSTALL_PACKAGES
    public static void installPackage(Context ctx, String apkPath) {
        PackageManager pm = ctx.getPackageManager();
        try {
            Class<?> cls = pm.getClass();
            Method method = cls.getMethod("installPackage",
                    Uri.class, IPackageInstallObserver.class,
                    int.class, String.class);
            method.invoke(pm, Uri.fromFile(new File(apkPath)),
                    null, 0, null);
        } catch (Exception e) {
            Log.e("ShellExecutor", "installPackage failed", e);
        }
    }
}
```

## 7. 预装到系统镜像

### 7.1 方式一：手动 push（调试用）

```bash
# 1. 重新挂载 system 分区
adb root
adb remount

# 2. push APK
adb push app-release.apk /system/priv-app/MySystemApp/MySystemApp.apk

# 3. 设置权限
adb shell chmod 644 /system/priv-app/MySystemApp/MySystemApp.apk

# 4. 重启生效
adb reboot
```

### 7.2 方式二：集成到 AOSP 编译

**`vendor/mycompany/apps/MySystemApp/Android.mk`**

```makefile
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := MySystemApp
LOCAL_SRC_FILES := MySystemApp.apk
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_CERTIFICATE := platform
LOCAL_PRIVILEGED_MODULE := true
LOCAL_DEX_PREOPT := nostripping
include $(BUILD_PREBUILT)
```

**`device/mycompany/mydevice/device.mk`**

```makefile
PRODUCT_PACKAGES += \
    MySystemApp
```

### 7.3 方式三：Android.bp 集成

```bp
android_app_import {
    name: "MySystemApp",
    apk: "MySystemApp.apk",
    certificate: "platform",
    privileged: true,
    dex_preopt: {
        enabled: false,
    },
}
```

## 8. 特权权限白名单

Android 12+ 要求特权 App 声明的权限必须在白名单中，否则不生效。

**`frameworks/base/data/etc/privapp-permissions-mycompany.xml`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<permissions>
    <privapp-permissions package="com.mycompany.systemapp">
        <permission name="android.permission.INTERACT_ACROSS_USERS_FULL" />
        <permission name="android.permission.WRITE_SECURE_SETTINGS" />
        <permission name="android.permission.MANAGE_USERS" />
        <permission name="android.permission.REBOOT" />
        <permission name="android.permission.FORCE_STOP_PACKAGES" />
        <permission name="android.permission.CONNECTIVITY_INTERNAL" />
        <permission name="android.permission.READ_PRIVILEGED_PHONE_STATE" />
        <permission name="android.permission.INSTALL_PACKAGES" />
    </privapp-permissions>
</permissions>
```

将此文件复制到：

```
system/etc/permissions/privapp-permissions-mycompany.xml
```

或在 AOSP 编译中通过 `COPY_OUT_SYSTEM_ETC` 自动安装。

## 9. SELinux 策略

系统 App 需要额外的 SELinux 权限：

**`vendor/mycompany/sepolicy/system_app.te`**（追加）

```te
# 允许系统 App 调用自定义服务
allow system_app custom_service:service_manager find;

# 允许执行 shell 命令
allow system_app shell_exec:file { execute execute_no_trans };

# 允许写 sysfs（如需要）
allow system_app sysfs:file write;

# 允许与 native 服务通信
allow system_app my_native_service:binder call;
```

## 10. 调试技巧

### 10.1 Android Studio 直接调试

```bash
# 1. 先 push APK 到 /system
adb root && adb remount
adb push app-debug.apk /system/priv-app/MySystemApp/MySystemApp.apk
adb shell chmod 644 /system/priv-app/MySystemApp/MySystemApp.apk
adb reboot

# 2. Android Studio 中 Attach Debugger
# Run -> Attach Debugger to Android Process -> 选择 com.mycompany.systemapp
```

### 10.2 日志过滤

```bash
# 按 TAG 过滤
adb logcat -s MySystemApp:D HiddenApi:D ServiceBridge:D

# 按 UID 过滤（system UID = 1000）
adb logcat --uid=1000

# 实时看崩溃
adb logcat -s AndroidRuntime:E
```

### 10.3 权限检查

```bash
# 查看App权限
adb shell dumpsys package com.mycompany.systemapp | grep -A 50 "granted=true"

# 查看SELinux拒绝
adb shell dmesg | grep avc | grep system_app

# 查看App的UID和进程
adb shell ps -A | grep mycompany
```

## 11. 完整项目模板

```
MySystemApp/
├── keys/
│   └── platform.jks                 # 平台签名密钥
├── libs/
│   └── framework.jar                # 包含 @hide API 的 framework
├── app/
│   ├── build.gradle
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── java/com/mycompany/systemapp/
│       │   ├── MainActivity.java
│       │   ├── HiddenApiBridge.java  # 隐藏 API 桥接
│       │   ├── ServiceBridge.java    # 系统服务桥接
│       │   └── ShellExecutor.java    # Shell 命令执行
│       └── res/
├── build.gradle
├── settings.gradle
└── gradle.properties
```

## 12. 关键点小结

| 步骤 | 关键配置 | 作用 |
|------|---------|------|
| 平台签名 | `platform.jks` + `signingConfig` | 以 system UID 运行 |
| sharedUserId | `android:sharedUserId="android.uid.system"` | 获得系统权限 |
| 预装路径 | `/system/priv-app/` | 特权 App 安装位置 |
| 隐藏 API | `framework.jar` 替换 bootstrapClasspath | 调用 @hide 方法 |
| 特权白名单 | `privapp-permissions-*.xml` | Android 12+ 必须声明 |
| SELinux | `system_app.te` 追加规则 | 允许 binder/sysfs 访问 |
| AOSP 集成 | `Android.mk` + `PRODUCT_PACKAGES` | 编译进系统镜像 |

## 13. 易踩的坑

1. **没有平台签名就装到 `/system/priv-app/`** → UID 不匹配，权限全部拒绝。
2. **`android:sharedUserId` 和签名不匹配** → 安装失败 `INSTALL_FAILED_SHARED_USER_INCOMPATIBLE`。
3. **特权白名单漏了权限** → Android 12+ 上 `grantRuntimePermission` 也不生效。
4. **`android:persistent="true"` 滥用** → 所有 persistent App 在 system_ready 前启动，拖慢开机。
5. **直接用 `Runtime.exec("su")`** → 系统App不需要 su，直接用平台权限即可。
6. **`framework.jar` 版本不匹配** → 编译时用的 AOSP 版本和设备版本不一致，运行时 `ClassNotFoundException`。
7. **调试时用 `adb install` 安装** → 普通安装无法获得系统权限，必须 push 到 `/system/`。
8. **忘了 `adb remount`** → push 到 `/system/` 报 `Read-only file system`。

---

如果你希望我将此内容写入文件（如 `c:\D\otherproject\obsidian\assest\addservice\system_app.md`），请告诉我。