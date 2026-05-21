用户想了解在 AOSP 中如何通过源码引入以及使用静态库的方式。这涉及到：
1. 从源码编译 native 库（而不是预置预编译的 so 文件）
2. 使用静态库（static library）而非共享库

我需要提供完整的 Android.bp 配置，包括：
- cc_library / cc_library_static - 编译静态库
- cc_binary / cc_library_shared - 使用静态库
- 与 Android App 的集成方式
我来为你详细介绍 **AOSP 源码编译** 和 **静态库** 的完整配置方案！🔧

## 📚 AOSP Native 代码源码引入 & 静态库完全指南

---

## **一、整体架构概览**

```
┌──────────────────────────────────────────────────────┐
│                   Android.bp 构建关系图               │
├──────────────────────────────────────────────────────┤
│                                                      │
│  ┌─────────────┐     ┌─────────────┐                 │
│  │ cc_library  │     │ cc_library_ │                 │
│  │ _headers    │     │ static      │                 │
│  │ (headfile.a)│     │ ( .a)       │                 │
│  └──────┬──────┘     └──────┬──────┘                 │
│         │                   │                        │
│         └────────┬──────────┘                        │
│                  ▼                                   │
│         ┌──────────────────────┐                     │
│         │ cc_library_shared    │                     │
│         │        ││            │                     │
│         │  Dynamic library.so  │                     │
│         └───────┬──────────────┘                     │
│                 │                                    │
│                 ▼                                    │
│  ┌────────────────────────────┐                      │
│  │ android_app /              │                      │
│  │ android_app_import         │                      │
│  │ // final APK               │                      │
│  └────────────────────────────┘                      │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## **二、基础：源码编译静态库**

### **1️⃣ 项目目录结构（推荐）**

```
packages/apps/MySystemApp/
├── Android.bp                    # 主构建文件
├── AndroidManifest.xml           # 清单文件
├── jni/                          # Native 源码根目录
│   ├── Android.bp                # Native 构建配置
│   ├── include/                  # 公共头文件
│   │   ├── common.h
│   │   ├── utils.h
│   │   └── myapi.h
│   ├── libcore/                  # 核心静态库源码
│   │   ├── core.cpp
│   │   ├── core.h
│   │   └── utils.cpp
│   ├── libcrypto/                # 加密模块（可选）
│   │   ├── crypto.cpp
│   │   └── crypto.h
│   └── main/                     # 动态库入口
│       ├── jni_entry.cpp
│       ├── com_example_myapp_MyNative.cpp  # JNI 实现
│       └── loader.cpp
├── src/                          # Java 源码
│   └── com/example/myapp/
│       └── MainActivity.java
└── res/                          # 资源文件
```

### **2️⃣ 头文件模块 (`cc_library_headers`)**

```json
// jni/Android.bp

// ==================== 步骤1: 导出头文件 ====================
cc_library_headers {
    name: "myapp_headers",

    // 头文件搜索路径
    export_include_dirs: ["include"],

    // 可见性控制（哪些模块可以使用这些头文件）
    visibility: [
        "//packages/apps/MySystemApp:__subpackages__",
        "//vendor/your_company:__subpackages__",
    ],

    // 主机支持（可选）
    host_supported: true,
}
```

对应的头文件：

```cpp
// jni/include/myapi.h - API 声明
#ifndef MYAPP_API_H
#define MYAPP_API_H

#include <cstdint>
#include <string>

namespace myapp {

// 工具类
class Utils {
public:
    static std::string getVersion();
    static int calculate(int a, int b);
};

// 核心功能类
class CoreEngine {
public:
    CoreEngine();
    ~CoreEngine();

    bool initialize();
    void process(const uint8_t* data, size_t len);
    std::string getResult() const;

private:
    class Impl;
    Impl* impl_;
};

// 加密工具（示例）
class CryptoUtil {
public:
    static std::string md5Hash(const std::string& input);
    static std::string sha256Hash(const std::string& input);
};

} // namespace myapp

#endif // MYAPP_API_H
```

---

### **3️⃣ 核心静态库 (`cc_library_static`)**

```json
// ==================== 步骤2: 编译核心静态库 (.a) ====================
cc_library_static {
    name: "libmyapp_core",

    // C++ 源码文件
    srcs: [
        "libcore/core.cpp",
        "libcore/utils.cpp",
    ],

    // 依赖的头文件模块
    header_libs: ["myapp_headers"],
    
    // 导出依赖（让依赖此库的模块自动继承）
    export_header_libs: ["myapp_headers"],

    // C++ 标准与标志
    cpp_std: "c++17",
    
    cflags: [
        "-Wall",
        "-Werror",          # 警告当作错误（严格模式）
        "-Wextra",
        "-O2",              # 优化等级
        "-ffunction-sections",
        "-fdata-sections",
    ],
    
    cppflags: [
        "-fno-exceptions",
        "-fno-rtti",
        "-fvisibility=hidden",
    ],

    // 链接器标志
    ldflags: [
        "-Wl,--gc-sections",
    ],

    // 共享系统库依赖
    shared_libs: [
        "liblog",           # Android 日志
        "libcutils",        # C 工具库
        "libbase",          # Google 基础库
        "libutils",         # Android 工具
    ],

    // 目标架构支持（默认全部）
    target: {
        android: {
            enabled: true,
        },
        host: {
            enabled: false,
        },
        darwin: {
            enabled: false,
        },
        windows: {
            enabled: false,
        },
    },

    // 覆盖率统计（可选）
    native_coverage: false,

    // 可见性
    visibility: [
        "//packages/apps/MySystemApp/jni:__subpackages__",
    ],
}
```

核心实现代码：

```cpp
// jni/libcore/utils.cpp
#include "common.h"
#include <android/log.h>

#define LOG_TAG "MyApp::Utils"

namespace myapp {

std::string Utils::getVersion() {
    return "1.0.0-source-build";
}

int Utils::calculate(int a, int b) {
    return a * b + (a >> 2);
}

} // namespace myapp
```

```cpp
// jni/libcore/core.cpp
#include "myapi.h"
#include "common.h"
#include <cstring>
#include <memory>

#define LOG_TAG "MyApp::Core"

namespace myapp {

// PIMPL 实现细节隐藏
class CoreEngine::Impl {
public:
    bool initialized = false;
    std::string result;
    uint8_t buffer[4096];
};

CoreEngine::CoreEngine() : impl_(new Impl()) {}

CoreEngine::~CoreEngine() {
    delete impl_;
}

bool CoreEngine::initialize() {
    if (impl_->initialized) return true;

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, 
                        "Initializing Core Engine v%s", 
                        Utils::getVersion().c_str());

    memset(impl_->buffer, 0, sizeof(impl_->buffer));
    impl_->initialized = true;

    return true;
}

void CoreEngine::process(const uint8_t* data, size_t len) {
    if (!impl_->initialized || !data || len == 0) return;

    size_t copyLen = len < sizeof(impl_->buffer) ? len : sizeof(impl_->buffer);
    memcpy(impl_->buffer, data, copyLen);

    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,
                        "Processed %zu bytes", copyLen);
}

std::string CoreEngine::getResult() const {
    if (!impl_->initialized) return "";

    // 简单示例：返回缓冲区的 hex 表示
    char hex[8192] = {0};
    for (size_t i = 0; i < sizeof(impl_->buffer); i++) {
        snprintf(hex + i * 2, 3, "%02x", impl_->buffer[i]);
    }
    return hex;
}

} // namespace myapp
```

---

### **4️⃣ 静态库的静态链接（生成动态库 `.so`）**

```json
// ==================== 步骤3: 将静态库链接为动态库 (.so) ====================
cc_library_shared {
    name: "libmyapp_jni",

    // JNI 入口源码
    srcs: [
        "main/jni_entry.cpp",
        "main/com_example_myapp_MyNative.cpp",
        "main/loader.cpp",
    ],

    // ==================== 关键: 静态库链接 ====================
    static_libs: [
        "libmyapp_core",      // 我们的核心静态库
    ],
    
    // 如果还有其他第三方静态库
    static_libs: [
        "libmyapp_core",
        // "libssl_static",    # 示例: OpenSSL 静态库
        // "libcrypto_static", # 示例: Crypto 静态库
        // "libz_static",      # 示例: Zlib 静态库
    ],

    // 头文件依赖
    header_libs: ["myapp_headers"],

    // SDK 版本
    sdk_version: "current",
    min_sdk_version: "28",
    stl: "c++_shared",       // 或 "c++_static", "none"

    // C/C++ 标准
    c_std: "c11",
    cpp_std: "c++17",

    // 编译标志
    cflags: [
        "-fPIC",              # 位置无关代码（生成 .so 必需）
        "-DANDROID_STL=c++_shared",
        "-DNDEBUG",
        "-Wall",
        "-O2",
    ],

    // 链接标志（解决符号可见性问题）
    ldflags: [
        "-Wl,-soname,libmyapp_jni.so",
        "-Wl,--exclude-libs,ALL",
        "-Wl,--gc-sections",
    ],

    // 运行时共享库
    shared_libs: [
        "liblog",
        "libdl",              # dlopen 等
        "libandroid",
    ],

    // 导出符号表（JNI 函数必须导出）
    version_script: "exports.txt",

    // 目标支持
    target: {
        android_arm64: {
            // ARM64 特定优化
            cflags: ["-march=armv8-a+crypto+crc"],
        },
        android_arm: {
            // ARM32 特定优化
            cflags: ["-march=armv7-a-neon"],
        },
        android_x86_64: {
            // x86_64 特定优化
            cflags: ["-msse4.2", "-mpopcnt"],
        },
    },

    // 是否安装到系统镜像
    relative_install_path: "myapp",

    // 可见性
    visibility: [
        "//packages/apps/MySystemApp:__pkg__",
    ],
}
```

**导出符号文件 `jni/main/exports.txt`**：

```
# 只导出 JNI 函数，隐藏其他内部符号
{
global:
    Java_*;
    JNI_OnLoad;
    JNI_OnUnload;
local:
    *;
};
```

---

### **5️⃣ 最终 APK 集成**

```json
// packages/apps/MySystemApp/Android.bp (顶层)

package {
    default_applicable_licenses: ["Apache-2.0"],
}

// ==================== Java/Kotlin App ====================
android_app {
    name: "MySystemApp",

    srcs: ["src/**/*.java"],
    resource_dirs: ["res"],
    manifest: "AndroidManifest.xml",

    // 平台签名
    certificate: "platform",
    privileged: true,
    system_ext_specific: true,

    // SDK 配置
    sdk_version: "system_current",
    target_sdk_version: "34",
    min_sdk_version: "28",

    // ==================== 关联 Native 库 ====================
    jni_libs: ["libmyapp_jni"],  # 引用上面编译的动态库

    // 或使用 required 强制依赖
    required: ["libmyapp_jni"],

    // Java 依赖
    static_libs: [
        "androidx.appcompat_appcompat",
        "com.google.android.material_material",
    ],

    optimize: {
        enabled: true,
        proguard_flags_files: ["proguard-rules.pro"],
    },
}

// 引用子目录的构建文件
soong_config_module_type {}
```

**Java 层调用 Native：**

```java
// src/com/example/myapp/MainActivity.java
package com.example.myapp;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

public class MainActivity extends Activity {
    
    private static final String TAG = "MySystemApp";

    // 加载 native 库
    static {
        try {
            System.loadLibrary("myapp_jni");
            Log.i(TAG, "Native library loaded successfully");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library: " + e.getMessage());
        }
    }

    // 声明 native 方法
    public static native String getNativeVersion();
    public static native int nativeCalculate(int a, int b);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        TextView textView = new TextView(this);
        
        try {
            String version = getNativeVersion();
            int result = nativeCalculate(10, 20);
            
            textView.setText(
                "✅ System App Running\n" +
                "Native Version: " + version + "\n" +
                "Calc Result: " + result
            );
        } catch (Exception e) {
            textView.setText("❌ Error: " + e.getMessage());
        }

        setContentView(textView);
    }
}
```

**JNI 实现：**

```cpp
// jni/main/com_example_myapp_MyNative.cpp
#include <jni.h>
#include "myapi.h"
#include <android/log.h>

#define LOG_TAG "MyApp_JNI"

extern "C" {

JNIEXPORT jstring JNICALL
Java_com_example_myapp_MainActivity_getNativeVersion(
    JNIEnv* env,
    jclass /* clazz */) {

    try {
        std::string ver = myapp::Utils::getVersion();
        return env->NewStringUTF(ver.c_str());
    } catch (...) {
        return env->NewStringUTF("error");
    }
}

JNIEXPORT jint JNICALL
Java_com_example_myapp_MainActivity_nativeCalculate(
    JNIEnv* env,
    jclass /* clazz */,
    jint a,
    jint b) {

    return myapp::Utils::calculate(a, b);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "JNI Library loaded. Initializing...");

    // 初始化核心引擎（可选）
    myapp::CoreEngine engine;
    if (engine.initialize()) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "Core engine initialized successfully");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "Failed to initialize core engine!");
    }

    return JNI_VERSION_1_6;
}

} // extern "C"
```

---

## **三、高级场景：多静态库组合**

### **场景 A：分层架构（推荐用于大型项目）**

```
jni/
├── include/              # 公共接口
├── libs/
│   ├── base/             # libmyapp_base.a
│   ├── network/          # libmyapp_network.a
│   ├── crypto/           # libmyapp_crypto.a
│   └── ui/               # libmyapp_ui.a
└── app/                  # 最终动态库入口
```

```json
// jni/libs/base/Android.bp
cc_library_static {
    name: "libmyapp_base",
    srcs: ["base_utils.cpp", "config_manager.cpp"],
    header_libs: ["myapp_headers"],
    export_header_libs: ["myapp_headers"],
    cpp_std: "c++17",
    cflags: ["-Wall", "-Werror", "-O2"],
}

// jni/libs/crypto/Android.bp
cc_library_static {
    name: "libmyapp_crypto",
    srcs: ["md5.cpp", "sha256.cpp", "aes.cpp"],
    header_libs: ["myapp_headers"],
    export_header_libs: ["myapp_headers"],
    static_libs: ["libmyapp_base"],  // 依赖 base 库！
    cpp_std: "c++17",
    cflags: ["-Wall", "-O2"],
}

// jni/libs/network/Android.bp
cc_library_static {
    name: "libmyapp_network",
    srcs: ["http_client.cpp", "websocket.cpp"],
    header_libs: ["myapp_headers"],
    export_header_libs: ["myapp_headers"],
    static_libs: ["libmyapp_base"],
    cpp_std: "c++17",
    cflags: ["-Wall", "-O2"],
}

// 最终动态库
cc_library_shared {
    name: "libmyapp_full",
    srcs: ["app/main.cpp", "app/jni_bridge.cpp"],
    
    // 组合所有静态库
    static_libs: [
        "libmyapp_base",
        "libmyapp_crypto",
        "libmyapp_network",
    ],
    
    header_libs: ["myapp_headers"],
    shared_libs: ["liblog", "libutils"],
    sdk_version: "current",
    stl: "c++_shared",
}
```

### **场景 B：第三方静态库集成**

假设你有预编译好的第三方 `.a` 文件：

```
jni/third_party/
├── openssl/
│   ├── include/           # openssl 头文件
│   └── libs/
│       ├── arm64-v8a/libssl.a
│       ├── arm64-v8a/libcrypto.a
│       ├── armeabi-v7a/libssl.a
│       ├── armeabi-v7a/libcrypto.a
│       ├── x86_64/libssl.a
│       └── x86/libssl.a
└── ...
```

```json
// jni/third_party/openssl/Android.bp
cc_library_static {
    name: "openssl_ssl_static",

    strip: {
        none: true,
    },

    target: {
        android_arm64: {
            srcs: ["libs/arm64-v8a/libssl.a"],
        },
        android_arm: {
            srcs: ["libs/armeabi-v7a/libssl.a"],
        },
        android_x86_64: {
            srcs: ["libs/x86_64/libssl.a"],
        },
        android_x86: {
            srcs: ["libs/x86/libssl.a"],
        },
    },

    // 导出 OpenSSL 头文件
    export_include_dirs: ["include"],
    
    shared_libs: ["libdl"],
}

// Crypto 库类似
cc_library_static {
    name: "openssl_crypto_static",
    
    strip: { none: true },

    target: {
        android_arm64: { srcs: ["libs/arm64-v8a/libcrypto.a"] },
        android_arm:   { srcs: ["libs/armeabi-v7a/libcrypto.a"] },
        android_x86_64:{ srcs: ["libs/x86_64/libcrypto.a"] },
        android_x86:   { srcs: ["libs/x86/libcrypto.a"] },
    },

    export_include_dirs: ["include"],
}

// 在你的项目中使用
cc_library_shared {
    name: "libmyapp_with_openssl",
    srcs: ["src/main.cpp"],

    static_libs: [
        "libmyapp_core",           // 自己的库
        "openssl_ssl_static",      // 第三方 SSL
        "openssl_crypto_static",   // 第三方 Crypto
    ],

    shared_libs: [
        "liblog",
        "libdl",
    ],
    
    ldflags: ["-Wl,-Bsymbolic"],
}
```

---

## **四、完整构建流程**

### **编译命令序列：**

```bash
# ==========================================
# AOSP Native 源码编译标准流程
# ==========================================

cd /path/to/aosp/source

# 1. 设置环境
source build/envsetup.sh

# 2. 选择目标产品
lunch aosp_arm64-eng    # 或你的设备目标

# 3. 方法A：只编译特定模块 (推荐)
m libmyapp_core          # 先编译静态库
m libmyapp_jni           # 再编译动态库
m MySystemApp            # 最后编译 APK

# 4. 方法B：一次性编译整个模块树
mm packages/apps/MySystemApp

# 5. 方法C：强制重编所有依赖
mmm packages/apps/MySystemApp -j$(nproc)

# 6. 验证输出
find out/target/product/generic -name "*myapp*"
# 应该看到:
# out/.../obj/SHARED_INTERMEDIATES/libmyapp_jni_intermediates/libmyapp_jni.so
# out/.../obj/STATIC_LIBRARIES/libmyapp_core_intermediates/libmyapp_core.a
# out/.../system_ext/app/MySystemApp/MySystemApp.apk
```

### **调试构建问题：**

```bash
# 查看详细的构建日志
m libmyapp_jni -j1 2>&1 | tee build_log.txt

# 检查 Soong 模块信息
m nothing --module-info libmyapp_jni

# 查看依赖关系
m nothing --make-mode showcommands libmyapp_jni

# 清理单个模块
m clean-libmyapp_jni
```

---

## **五、常见错误与解决方案**

| **错误类型** | **原因** | **解决方案** |
|-------------|---------|-------------|
| `undefined reference to 'xxx'` | 静态库未链接或顺序不对 | 添加 `static_libs: ["xxx"]` |
| `redefinition of 'yyy'` | 符号冲突 | 添加 `-fvisibility=hidden` 并用版本脚本 |
| `multiple definition of 'zzz'` | 多个库定义了相同符号 | 检查是否重复包含 `.o` 文件 |
| `cannot find -lxxx` | 缺少共享库依赖 | 在 `shared_libs` 中添加 |
| `file not recognized: File truncated` | 架构不匹配 | 检查 `target:` 配置 |

### **关键注意事项清单：**

```markdown
- [ ] 所有 cc_library_static 都设置了正确的 srcs
- [ ] 头文件通过 cc_library_headers 统一管理
- [ ] export_header_libs 正确传递依赖
- [ ] cc_library_shared 包含了所有需要的 static_libs
- [ ] 使用 -fPIC 编译动态库相关代码
- [ ] exports.txt 控制符号导出范围
- [ ] visibility 限制模块访问权限
- [ ] stl 设置一致 (c++_shared vs c++_static)
- [ ] 多架构 target 配置完整
```

---

需要我针对某个具体部分深入展开吗？例如：

- 🔧 **复杂项目的多模块依赖管理**
- 🧪 **单元测试与 Mock 集成**
- ⚡ **性能分析与编译优化选项**
- 🔒 **安全加固与混淆策略**


<!-- final android bp -->
用户想要看到最终的完整 Android.bp 文件。让我整理一个完整的、可以直接使用的最终版 Android.bp 文件，整合之前讨论的所有内容：源码编译、静态库、多架构支持等。
好的！我来为你提供 **最终完整版的 `Android.bp`**，这是一个生产级可直接使用的配置：

## 📋 最终完整版 Android.bp（一站式配置）

### **项目目录结构**

```
packages/apps/MySystemApp/
├── Android.bp                    # ★ 主构建文件 (下面这个)
├── AndroidManifest.xml
├── jni/
│   ├── Android.bp                # Native 子模块
│   ├── include/                  # 头文件
│   │   ├── common.h
│   │   └── myapi.h
│   ├── libcore/                  # 静态库源码
│   │   ├── core.cpp
│   │   └── utils.cpp
│   └── main/                     # JNI 入口 & 动态库
│       ├── jni_entry.cpp
│       └── com_example_MyNative.cpp
├── src/
│   └── com/example/myapp/
│       └── MainActivity.java
└── res/
    ├── layout/
    │   └── activity_main.xml
    └── values/
        └── strings.xml
```

---

### **🎯 最终完整版：顶层 `Android.bp`**

```json
// ============================================================
//  MySystemApp - AOSP 系统应用完整构建配置
//  支持: 源码编译 / 静态库 / 多ABI / 平台签名
//  作者: AI Assistant
//  版本: v1.0 Final
// ============================================================

package {
    default_applicable_licenses: ["Apache-2.0"],
}

// ==================== [1] 头文件模块 ====================
cc_library_headers {
    name: "myapp_headers",

    // 导出头文件搜索路径 (相对于当前 bp 所在目录)
    export_include_dirs: ["jni/include"],

    // 可见性控制：哪些模块可以使用这些头文件
    visibility: [
        "//packages/apps/MySystemApp/jni:__subpackages__",
        "//vendor/your_company/apps:__subpackages__",
    ],
}

// ==================== [2] 核心静态库 (.a) ====================
cc_library_static {
    name: "libmyapp_core",

    // ---- C/C++ 源码文件 ----
    srcs: [
        "jni/libcore/core.cpp",
        "jni/libcore/utils.cpp",
    ],

    // ---- 头文件依赖 ----
    header_libs: ["myapp_headers"],
    
    // 导出给下游模块 (关键!)
    export_header_libs: ["myapp_headers"],

    // ---- 编译标准 ----
    c_std: "c11",
    cpp_std: "c++17",

    // ---- 编译标志 ----
    cflags: [
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-O2",
        "-ffunction-sections",
        "-fdata-sections",
        "-fvisibility=hidden",     // 隐藏内部符号
    ],
    
    cppflags: [
        "-fno-exceptions",          // 禁用异常 (减小体积)
        "-fno-rtti",               // 禁用 RTTI
    ],

    // ---- 链接器标志 ----
    ldflags: [
        "-Wl,--gc-sections",       // 移除未使用代码
    ],

    // ---- 系统共享库依赖 ----
    shared_libs: [
        "liblog",                  // Android 日志
        "libcutils",              // C 工具函数
        "libbase",                // Google 基础库
    ],

    // ---- 目标平台限制 (可选) ----
    target: {
        android: {
            enabled: true,
        },
        host: {
            enabled: false,       // 不需要主机版本
        },
    },

    // ---- 覆盖率统计 (可选) ----
    native_coverage: false,

    // ---- 可见性 ----
    visibility: [
        "//packages/apps/MySystemApp/jni:__subpackages__",
    ],
}

// ==================== [3] 动态库 (.so) - JNI 接口 ====================
cc_library_shared {
    name: "libmyapp_jni",

    // ---- JNI 入口源码 ----
    srcs: [
        "jni/main/jni_entry.cpp",
        "jni/main/com_example_MyNative.cpp",
    ],

    // ★★★ 关键: 静态链接我们的核心库 ★★★
    static_libs: [
        "libmyapp_core",
    ],

    // ---- 头文件依赖 ----
    header_libs: ["myapp_headers"],

    // ---- SDK 与 STL 配置 ----
    sdk_version: "current",
    min_sdk_version: "28",
    stl: "c++_shared",

    // ---- 语言标准 ----
    c_std: "c11",
    cpp_std: "c++17",

    // ---- 编译标志 (动态库必须 -fPIC) ----
    cflags: [
        "-fPIC",                   // 位置无关代码 (必需!)
        "-DANDROID_STL=c++_shared",
        "-DNDEBUG",
        "-Wall",
        "-Werror",                 // 开发期启用
        "-O2",
        "-fvisibility=hidden",
    ],

    // ---- 链接器标志 ----
    ldflags: [
        "-Wl,-soname,libmyapp_jni.so",
        "-Wl,--exclude-libs,ALL",  // 不导出静态库内部符号
        "-Wl,--gc-sections",
        "-Wl,-Bsymbolic-functions",
    ],

    // ---- 导出符号表 (只暴露 JNI 函数) ----
    version_script: "jni/exports.txt",

    // ---- 运行时共享系统库 ----
    shared_libs: [
        "liblog",
        "libdl",                  // dlopen/dlsym
        "libandroid",
    ],

    // ---- 多架构优化 (可选) ----
    target: {
        android_arm64: {
            cflags: [
                "-march=armv8-a+crypto+crc",  // ARM64 优
            ],
        },
        android_arm: {
            cflags: [
                "-march=armv7-a-neon",         // ARM32 NEON
            ],
        },
        android_x86_64: {
            cflags: [
                "-msse4.2",                     // x86_64 SSE4.2
                "-mpopcnt",
            ],
        },
        android_x86: {
            cflags: [
                "-msse3",                       // x86 SSE3
            ],
        },
    },

    // ---- 安装路径 ----
    relative_install_path: "myapp",

    // ---- 可见性 ----
    visibility: [
        "//packages/apps/MySystemApp:__pkg__",
    ],
}

// ==================== [4] Java/Kotlin APK ====================
android_app {
    name: "MySystemApp",

    // ---- Java 源码 ----
    srcs: ["src/**/*.java"],
    
    // ---- 资源目录 ----
    resource_dirs: ["res"],
    
    // ---- 清单文件 ----
    manifest: "AndroidManifest.xml",

    // ★★★ 平台签名 (系统应用核心) ★★★
    certificate: "platform",
    
    // 特权应用 (可安装到 system/priv-app)
    privileged: true,

    // 分区选择:
    // system_ext_specific -> /system_ext/app/
    // product_specific    -> /product/app/
    // vendor: true        -> /vendor/app/
    system_ext_specific: true,

    // ---- SDK 版本配置 ----
    sdk_version: "system_current",
    target_sdk_version: "34",
    min_sdk_version: "28",

    // ★★★ 关联 Native 库 ★★★
    jni_libs: ["libmyapp_jni"],
    required: ["libmyapp_jni"],  # 强制安装依赖

    // ---- Java 第三方依赖 ----
    libs: ["android.hidl.base-V1.0-java"],  # HIDL 支持 (可选)

    static_libs: [
        "androidx-core-ktx",
        "androidx-appcompat-appcompat",
        "com.google.android.material_material",
        "androidx-constraintlayout_constraintlayout",
    ],

    // ---- ProGuard/R8 优化 ----
    optimize: {
        enabled: true,
        proguard_flags_files: ["proguard-rules.pro"],
    },
    
    // ---- DEX 预优化 ----
    dex_preopt: {
        enabled: true,
        app_image: true,           // 加速启动
    },
}

// ==================== [5] 导出符号定义文件 ====================
// 文件路径: jni/exports.txt (需单独创建)
//
// 内容如下 (创建此文件):
/*
{
global:
    Java_*;           # 所有 JNI 导出函数
    JNI_OnLoad;
    JNI_OnUnload;
local:
    *;                # 隐藏所有其他符号
};
*/
```

---

### **📄 必须创建的配套文件**

#### **① `jni/exports.txt`** (导出符号控制)

```text
{
global:
    Java_*;
    JNI_OnLoad;
    JNI_OnUnload;

local:
    *;
};
```

#### **② `proguard-rules.pro`** (混淆规则)

```properties
# MySystemApp ProGuard Rules

# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep application class
-keep public class com.example.myapp.** {
    public protected *;
}
```

#### **③ `AndroidManifest.xml`** (精简版)

```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.myapp">

    <uses-permission android:name="android.permission.RECEIVE_BOOT_COMPLETED"/>

    <application
        android:icon="@drawable/ic_launcher"
        android:label="@string/app_name"
        android:persistent="true"
        android:directBootAware="true">

        <activity
            android:name=".MainActivity"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity>

        <!-- 可选: 开机自启 -->
        <receiver
            android:name=".BootReceiver"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.BOOT_COMPLETED"/>
            </intent-filter>
        </receiver>

    </application>
</manifest>
```

---

### **📦 产品集成配置 (`device.mk` 或类似)****

在设备或产品的 `.mk` 文件中添加一行即可：

```makefile
PRODUCT_PACKAGES += MySystemApp
```

或创建产品定义文件：

```makefile
# products/my_product.mk
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_system.mk)

PRODUCT_NAME := my_custom_rom
PRODUCT_DEVICE := generic
PRODUCT_BRAND := Android
PRODUCT_MODEL := Custom Build with System App

# ✅ 在这里添加你的系统应用
PRODUCT_PACKAGES += \
    MySystemApp \
    OtherApp1

# 签名 (如果不用默认 platform)
# PRODUCT_DEFAULT_DEV_CERTIFICATE := vendor/your_security/releasekey
```

---

### **⚡ 一键编译命令**

```bash
#!/bin/bash
# build_myapp.sh - 完整编译流程

set -e

AOSP_ROOT=/path/to/aosp/source
cd $AOSP_ROOT

echo "=========================================="
echo "  MySystemApp 一键编译脚本"
echo "=========================================="

# 1. 初始化环境
source build/envsetup.sh

# 2. 选择目标
lunch aosp_arm64-eng  # 或你的设备代号

# 3. 按顺序编译 (确保依赖正确)
echo ""
echo "[1/4] 编译头文件模块..."
m myapp_headers || { echo "❌ 头文件模块失败"; exit 1; }
echo "✅ 头文件完成"

echo "[2/4] 编译核心静态库..."
m libmyapp_core || { echo "❌ 静态库失败"; exit 1; }
echo "✅ 静态库完成"

echo "[3/4] 编译 JNI 动态库..."
m libmyapp_jni || { echo "❌ 动态库失败"; exit 1; }
echo "✅ 动态库完成"

echo "[4/4] 编译最终 APK..."
m MySystemApp || { echo "❌ APK 编译失败"; exit 1; }
echo "✅ APK 完成"

# 4. 输出验证
echo ""
echo "=========================================="
echo "✅ 全部编译成功!"
echo "=========================================="
echo ""
echo "输出文件位置:"
find out/target/product/*/system_ext/app -name "MySystemApp*" 2>/dev/null | head -5
find out/target/product/*/system_ext/lib64 -name "*myapp*" 2>/dev/null | head -5
```

---

### **🔍 构建产物清单**

编译完成后你应该能在以下位置找到产物：

```
out/target/product/<device>/
├── system_ext/
│   ├── app/
│   │   └── MySystemApp/
│   │       └── MySystemApp.apk          ← 最终 APK
│   └── lib64/                           # 或 lib/ (32位)
│       └── myapp/
│           └── libmyapp_jni.so          ← Native 动态库
├── obj/
│   ├── STATIC_LIBRARIES/
│   │   └── libmyapp_core_intermediates/
│   │       └── libmyapp_core.a         ← 中间静态库 (调试用)
│   └── SHARED_LIBRARIES/
│       └── libmyapp_jni_intermediates/
│           └── libmyapp_jni.so         ← 中间动态库 (调试用)
```

---

### **✅ 最终检查清单**

部署前确认以下所有项：

- [ ] `Android.bp` 无语法错误 (`m nothing --mkbp` 验证)
- [ ] `jni/exports.txt` 符号导出文件已创建
- [ ] 所有 `.cpp/.h` 文件路径与 `srcs:` 匹配
- [ ] `certificate: "platform"` 已设置 (如需系统权限)
- [ ] `static_libs` 包含了所有自定义静态库
- [ ] `shared_libs` 包含了所需的系统库
- [ ] `PRODUCT_PACKAGES += MySystemApp` 已添加到产品配置
- [ ] 多 ABI 的 `target:` 配置完整（如需要）
- [ ] `stl` 设置一致 (`c++_shared`)
- [ ] 使用 `-fPIC` 编译动态库代码
- [ ] `visibility` 权限范围合理

---

这就是最终的 **生产级完整版 `Android.bp`** 配置！直接复制到你的项目中，按目录结构放置好源码文件，就可以执行编译了。

还有其他需要调整的地方吗？比如：
- 🔄 **增量编译优化**
- 🔬 **添加单元测试模块**
- 🌐 **多语言支持**
- 📦 **OTA 更新保留数据配置**
