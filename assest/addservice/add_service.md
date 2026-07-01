下面是"在 Android framework 中添加系统服务"的完整实例,采用 AOSP 14 的 **HIDL/AIDL + 系统服务注册** 模式,包含 native + Java 两侧代码,可以直接照着改。

# Android Framework 添加系统服务完整实例

下面以添加一个名为 **"MyCustomService"**(能力:读/写一个 string 状态,提供 binder 调用)的系统服务为例,演示完整链路。

## 1. 服务整体架构

```
flowchart TB
    App["App 客户端<br/>通过 ServiceManager.getService] --> SM
    SM["ServiceManager<br/>(system_server 持有)"] --> SVC["MyCustomService<br/>(system_server 进程)"]
    SVC --> Native["native 实现<br/>frameworks/native/.../MyCustomService.cpp"]
    SVC --> HIDL["HIDL/AIDL 接口<br/>IMyCustomService.aidl"]
```

两种实现方式:

| 方式 | 适用 | 推荐度 |
|---|---|---|
| **A. 纯 AIDL/Java 服务**(全 Java) | 简单逻辑,无 native 依赖 | ★★★★★ (AOSP 14 主推) |
| **B. HIDL + native + Java** | 涉及 HAL/驱动/性能敏感 | ★★★★ (兼容老项目) |
| **C. 纯 native Service + JNI** | 需要挂到 ServiceManager 的 C/S | ★★★ |

下面给 **A + B 混合的最小可用版**(AIDL 定义接口,Java 注册到 system_server,带可选 native 桩)。

## 2. 目录结构

```
frameworks/base/
├── core/
│   ├── java/android/custom/
│   │   ├── IMyCustomService.aidl         # AIDL 接口
│   │   └── MyCustomManager.java          # 客户端 Manager
│   └── java/android/server/custom/
│       ├── MyCustomService.java          # 服务端 Stub
│       └── SystemServer.smali 补丁?       # 注册入口
│
├── services/
│   ├── core/
│   │   └── java/com/android/server/
│   │       ├── SystemServer.java         # 改:启动服务
│   │       └── custom/
│   │           ├── CustomService.java    # 服务本体(若走 HIDL)
│   │           └── CustomServiceNative.cpp # native 部分
│   └── Android.bp                        # 改:把 .aidl 加入编译
│
└── Android.bp                            # 改:导出 .aidl 到 SDK
```

> 推荐放在 `frameworks/base/services/` 之外再开一层 `vendor/<vendor>/services/` 或 `frameworks/extras/`,与 AOSP 主仓解耦,**不污染主仓**。下面示例按"放进 `frameworks/base/`"写。

## 3. 定义 AIDL 接口

**`frameworks/base/core/java/android/custom/IMyCustomService.aidl`**

```java
package android.custom;

/** {@hide} */
interface IMyCustomService {

    /** 写一个 key/value,内部存到 native */
    int setValue(String key, String value);

    /** 读一个 key,不存在返回 null */
    String getValue(String key);

    /** 列出所有 key */
    String[] listKeys();
}
```

把它加进 `core` 的 `Android.bp`,让 SDK 编译时导出:

```diff
// frameworks/base/core/Android.bp
filegroup {
    name: "framework-aidl",
    srcs: [
+       "java/android/custom/IMyCustomService.aidl",
    ],
    ...
}
```

## 4. 服务端实现(Java 端 Stub)

**`frameworks/base/core/java/android/server/custom/MyCustomService.java`**

```java
package android.server.custom;

import android.custom.IMyCustomService;
import android.os.RemoteException;
import android.util.Log;

import java.util.HashMap;
import java.util.Map;

/** {@hide} */
public class MyCustomService extends IMyCustomService.Stub {

    private static final String TAG = "MyCustomService";

    private final Map<String, String> mStore = new HashMap<>();

    public MyCustomService() {
        Log.d(TAG, "MyCustomService created");
    }

    @Override
    public int setValue(String key, String value) throws RemoteException {
        if (key == null) return -1;
        mStore.put(key, value);
        Log.d(TAG, "setValue " + key + "=" + value);
        return 0;
    }

    @Override
    public String getValue(String key) throws RemoteException {
        return mStore.get(key);
    }

    @Override
    public String[] listKeys() throws RemoteException {
        return mStore.keySet().toArray(new String[0]);
    }
}
```

> `IMyCustomService.Stub` 会在 `aidl` 编译时**自动生成**(`out/.../aidl/android/custom/IMyCustomService.java`),不要自己手写。

## 5. 注册到 system_server

**修改 `frameworks/base/services/core/java/com/android/server/SystemServer.java`**

```java
import android.server.custom.MyCustomService;
import android.os.ServiceManager;

public final class SystemServer {

    private void startOtherServices() {
        try {
            // 1. 实例化
            MyCustomService svc = new MyCustomService();

            // 2. 注册到 ServiceManager
            ServiceManager.addService("custom", svc);
            // (在 Android 14+ 也可走 SystemServiceRegistry 模式,见下文)
            Log.i("SystemServer", "MyCustomService registered");
        } catch (Throwable e) {
            reportWtf("starting MyCustomService", e);
        }
    }
}
```

### 5.1 Android 12+ 推荐:`SystemServiceRegistry` 模式(可选升级)

更"现代化"的写法,在 `frameworks/base/core/java/android/app/SystemServiceRegistry.java` 末尾追加:

```java
import android.custom.IMyCustomService;
import android.os.ServiceManager;
import android.os.ServiceManager.ServiceNotFoundException;
import android.server.custom.MyCustomService;

final class SystemServiceRegistry {

    // ... 已有 static 块 ...

    static {
        // 选一个不冲突的标识
        registerService("custom", IMyCustomService.class,
            new CachedServiceFetcher<IMyCustomService>() {
                @Override
                public IMyCustomService createService(int impl) {
                    final IBinder b = ServiceManager.getService("custom");
                    return IMyCustomService.Stub.asInterface(b);
                }
            });
    }
}
```

应用层访问入口 `MyCustomManager`:

**`frameworks/base/core/java/android/custom/MyCustomManager.java`**

```java
package android.custom;

import android.content.Context;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;

/** {@hide} */
public class MyCustomManager {
    private static final String TAG = "MyCustomManager";
    private final IMyCustomService mService;

    public MyCustomManager(Context ctx) {
        IBinder b = ServiceManager.getService("custom");
        mService = IMyCustomService.Stub.asInterface(b);
        if (mService == null) {
            throw new IllegalStateException("MyCustomService not registered");
        }
    }

    public int setValue(String k, String v) {
        try {
            return mService.setValue(k, v);
        } catch (RemoteException e) {
            Log.e(TAG, "setValue failed", e);
            return -1;
        }
    }

    public String getValue(String k) {
        try { return mService.getValue(k); }
        catch (RemoteException e) { return null; }
    }
}
```

注册到 `Context`:

**`frameworks/base/core/java/android/app/SystemServiceRegistry.java`**(上面那段里多一步)

```java
registerService(Context.CUSTOM_SERVICE, MyCustomManager.class,
    new CachedServiceFetcher<MyCustomManager>() {
        @Override
        public MyCustomManager createService(int impl) {
            return new MyCustomManager(null);
        }
    });
```

**`frameworks/base/core/java/android/content/Context.java`**(字符串常量)

```java
public static final String CUSTOM_SERVICE = "custom";
```

## 6. SELinux 策略(必加,否则 system_server 起不来)

**`system/sepolicy/service.te`**

```te
type custom_service, app_api_service, system_api_service, system_server_service;
```

**`system/sepolicy/service_contexts`**(末尾追加一行)

```
custom                            u:object_r:custom_service:s0
```

> 这一步很多人漏,**编译后会失败**:`avc: denied { find } ... service=custom`。

## 7. 客户端调用(普通 app 端)

```java
import android.custom.MyCustomManager;

MyCustomManager mgr = (MyCustomManager)
    getSystemService("custom");          // Context.CUSTOM_SERVICE
mgr.setValue("hello", "world");
String v = mgr.getValue("hello");
```

或在 `adb shell` 上测:

```bash
adb shell service call custom 1 s16 "key" s16 "value"
# 1 是 setValue 的 transaction code(从 .aidl 自动生成,详见 out 目录的 aidl 文件)
adb shell service call custom 2 s16 "key"
# 2 是 getValue
```

## 8. (可选)添加 native 桩

如果服务逻辑要落到 native,加 JNI:

**`frameworks/base/core/jni/android_custom_NativeBridge.cpp`**

```cpp
#include <jni.h>
#include <android-base/logging.h>

static void nativeInit(JNIEnv* env, jobject /* this */) {
    LOG(INFO) << "MyCustom native init";
}

static const JNINativeMethod kMethods[] = {
    { "nativeInit", "()V", (void*)nativeInit },
};

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /* reserved */) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }
    jclass cls = env->FindClass("android/custom/NativeBridge");
    if (cls == nullptr) return -1;
    if (env->RegisterNatives(cls, kMethods, sizeof(kMethods)/sizeof(kMethods[0])) < 0) {
        return -1;
    }
    return JNI_VERSION_1_6;
}
```

在 `frameworks/base/core/jni/Android.bp` 中加入该 cpp,并在 `frameworks/base/core/jni/firstboot/native_loader.cpp` 注册 JNI_OnLoad。

## 9. 集成到 product

**`build/target/product/aosp_x86_64.mk`**(默认 x86-64 product)

```makefile
PRODUCT_PACKAGES += \
    MySampleApp
# 框架系统服务不需要 PRODUCT_PACKAGES(它在 frameworks/base 编译),
# 但 sepolicy 必须随 system 一起发布,会自动带上。
```

## 10. 编译 + 验证

```bash
source build/envsetup.sh
lunch aosp_x86_64-eng

# 单模块编译
make framework services -j$(nproc)
make sepolicy -j$(nproc)

# 整编
make -j$(nproc)

# 启动模拟器
emulator

# 验证服务是否注册
adb shell service check custom                  # 期望输出:Service custom: found
adb shell service list | grep custom

# 看 SELinux 上下文
adb shell ls -lZ /system/bin/servicemanager     # 验证 service_contexts 是否生效

# 单元调用
adb shell service call custom 1 s16 "name" s16 "Alice"
adb shell service call custom 2 s16 "name"
```

## 11. 关键点小结

| 步骤 | 关键文件 | 作用 |
|---|---|---|
| AIDL 定义 | `core/java/android/custom/IMyCustomService.aidl` | 自动生成 binder proxy/stub |
| Stub 实现 | `core/java/android/server/custom/MyCustomService.java` | 继承 `Stub`,业务逻辑 |
| 注册到 system_server | `services/.../SystemServer.java` 的 `startOtherServices()` | 服务运行在 system_server 进程 |
| SELinux | `sepolicy/service.te` + `service_contexts` | 让服务可被 binder 找到 |
| 客户端 API | `core/java/android/custom/MyCustomManager.java` | 包装 binder,屏蔽 IPC 细节 |
| 字符串常量 | `Context.CUSTOM_SERVICE` | 应用层 `getSystemService` key |
| 整编验证 | `service list / service call` | 跑通端到端 |

## 12. 易踩的坑

1. **忘了 `aidl` 加进 `framework-aidl` 文件组** → `IMyCustomService` 找不到。
2. **忘了 `service_contexts` 加一行** → `getService("custom")` 返回 null。
3. **在 Android 12+ 用 `ServiceManager.addService` 直接加 string** → 仍可用,但推荐走 `SystemServiceRegistry`。
4. **`@hide` 没加** → SDK 编译会让普通 app 看到不该看到的 API。
5. **名字冲突** → `custom` 太普通,改 `vendor_custom` / `mycompany_custom` 更稳。
6. **AIDL `in/out/inout` 关键字漏了** → 跨进程传引用类型会失败。

---

**要落盘吗?** 你工作区目前还没有"系统服务"这一类笔记。我可以**新建** `c:\D\otherproject\obsidian\assest\app源码\addSystemService.md` 把上面这份实例写进去,并加 Obsidian 友好的标签。要的话告诉我"**写入 addSystemService.md**",我切到 craft 模式写文件。