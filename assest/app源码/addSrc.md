
工作区只有一个空文件。下面给你一个完整的"在 Android Framework 中添加 app 源码"实例(基于 AOSP 14 / Android 14,使用 `app/Android.mk` 方式,平台签名的系统应用)。

# Android Framework 添加 app 源码完整实例

下面以一个名为 **MySampleApp** 的系统应用为例,演示如何把 app 源码加入 AOSP framework 编译。

## 1. 目录结构

```
frameworks/base/
└── MySampleApp/
    ├── Android.mk              (旧) 或 Android.bp (新)
    ├── AndroidManifest.xml
    ├── src/
    │   └── com/example/mysample/
    │       ├── MainActivity.java
    │       └── MyService.java
    └── res/
        ├── layout/
        │   └── activity_main.xml
        └── values/
            └── strings.xml
```

> 推荐使用 **Android.bp**(新),如果你的项目仍用 `Android.mk` 写法也支持。

## 2. 源码文件

**`frameworks/base/MySampleApp/AndroidManifest.xml`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.mysample"
    android:versionCode="1"
    android:versionName="1.0"
    coreApp="true">

    <uses-sdk android:minSdkVersion="33" android:targetSdkVersion="34" />

    <application
        android:label="@string/app_name"
        android:allowBackup="false"
        android:persistent="true">
        <activity
            android:name=".MainActivity"
            android:exported="true"
            android:launchMode="singleTask">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>

        <service
            android:name=".MyService"
            android:exported="false" />
    </application>
</manifest>
```

**`frameworks/base/MySampleApp/src/com/example/mysample/MainActivity.java`**

```java
package com.example.mysample;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public class MainActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        TextView tv = new TextView(this);
        tv.setText("Hello from MySampleApp in Framework!");
        setContentView(tv);
    }
}
```

**`frameworks/base/MySampleApp/src/com/example/mysample/MyService.java`**

```java
package com.example.mysample;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

public class MyService extends Service {
    private static final String TAG = "MySampleService";

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d(TAG, "MyService onCreate");
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
```

**`frameworks/base/MySampleApp/res/layout/activity_main.xml`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:orientation="vertical">
    <TextView
        android:id="@+id/text"
        android:layout_width="wrap_content"
        android:layout_height="wrap_content"
        android:text="@string/app_name" />
</LinearLayout>
```

**`frameworks/base/MySampleApp/res/values/strings.xml`**

```xml
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="app_name">MySampleApp</string>
</resources>
```

## 3. 编译脚本(推荐 `Android.bp`)

**`frameworks/base/MySampleApp/Android.bp`**

```soong
android_app {
    name: "MySampleApp",
    srcs: ["src/**/*.java"],
    manifest: "AndroidManifest.xml",
    resource_dirs: ["res"],

    platform_apis: true,                       // 使用 platform.jar 中的 @hide API
    certificate: "platform",                   // 用 platform 签名,获得系统应用权限
    privileged: true,                          // 放到 /system/priv-app(可要权限)
    installable: true,

    static_libs: [
        "androidx.core_core",                  // 按需引入
    ],

    // 如果要引用 framework 中的 @hide API,需要加这个
    libs: [
        "framework",
    ],
}
```

> 如果工程仍使用 `Android.mk`,等价的写法:

```makefile
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := MySampleApp
LOCAL_CERTIFICATE := platform
LOCAL_PRIVILEGED_MODULE := true
LOCAL_REQUIRED_MODULES :=

LOCAL_RESOURCE_DIRS := $(LOCAL_PATH)/res
LOCAL_MANIFEST_FILE := AndroidManifest.xml

include $(BUILD_PACKAGE)
```

## 4. 集成到 framework 编译

### 方式 A:让 `framework` 这个 droidstar 目标包含你的 app(可选)

`frameworks/base/Android.bp` 默认不会把 app 编译进 system image。需要让它被 `droid` 目标收集,有几种办法:

**最简单 —— 在 device 或 product 的 `device.mk` 中添加:**

```makefile
# device/<vendor>/<product>/device.mk
PRODUCT_PACKAGES += \
    MySampleApp
```

或放到 `build/target/product/core.mk` / `mainline.mk`,只要是 droid 编译包含的 product mk 即可。

**方式 B —— 让 frameworks/base 自己"认领"该 app:**

在 `frameworks/base/Android.bp`(或同目录的 `.mk`)里增加一个 droid 兼容声明。Soong 中,`android_app` 会被 `PRODUCT_PACKAGES` 收集,所以更推荐 **方式 A**。

### 如果要把 .jar/aar 暴露给 framework 自身使用

```soong
java_library {
    name: "mysample-framework-helper",
    srcs: ["src/**/*.java"],
    installable: false,
    libs: ["framework"],
}
```

## 5. 编译并烧入

```bash
# 1. 仅编译此 app
mmm frameworks/base/MySampleApp

# 2. 整编(把 app 烧入 system image)
make -j$(nproc)

# 3. 单独推到已 root 设备
adb push out/target/product/<device>/system/priv-app/MySampleApp/MySampleApp.apk \
    /system/priv-app/MySampleApp/
adb shell pm install -r -t /system/priv-app/MySampleApp/MySampleApp.apk
```

## 6. 验证

```bash
# 查看 app 是否安装,以及签名是否为 platform
adb shell pm list packages | grep mysample
adb shell dumpsys package com.example.mysample | grep -E "signingInfo|flags"
# flags 中应包含 SYSTEM,signature 与 build/target/product/security/platform.pk8 一致
```

```bash
# 启动 Activity
adb shell am start -n com.example.mysample/.MainActivity

# 查看 Service 日志
adb logcat -s MySampleService:D
```

## 7. 关键点小结

| 项 | 设置 | 作用 |
|---|---|---|
| `coreApp="true"` | Manifest | 允许开机早启动,带 `android:persistent` |
| `android:persistent="true"` | application | 设备启动后自动启动该 app |
| `certificate: "platform"` | bp | 用 platform 签名,与系统同签名 → 共享签名权限 |
| `privileged: true` | bp | 安装到 `/system/priv-app/`,可声明 system 权限 |
| `platform_apis: true` | bp | 允许调用 `@hide` 内部 API |
| `PRODUCT_PACKAGES += MySampleApp` | device.mk | 把模块加入系统镜像 |
| `LOCAL_PACKAGE_NAME` / `name` | mk/bp | 模块名,必须与 `PRODUCT_PACKAGES` 中的字符串一致 |

---

**要不要我把这个实例直接写入 `c:\D\otherproject\obsidian\assest\app源码\addSrc.md`?** 如果需要,我可以按照你 Obsidian 的格式(标题、标签、代码块)直接落到该 md 文件中,方便你查阅。要的话告诉我"写入 addSrc.md"即可。