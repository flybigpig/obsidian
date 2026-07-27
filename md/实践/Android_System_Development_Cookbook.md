# Android 系统定制开发 — 七大工作场景实战指南

> **适用版本**: Android 14 (UpsideDownCake, API 34) / AOSP `android-14.0.0_rXX`  
> **目标读者**: 车载/工控 Android 底层开发者、Framework 定制工程师  
> **文档定位**: 每个场景从「为什么做 → 怎么做 → 源码路径 → 验证方法」完整闭环

---

## 总览速查表

| # | 工作内容     | 核心能力域                        | 典型案例                        | 改动层级                   | 难度   |
| - | -------- | ---------------------------- | --------------------------- | ---------------------- | ---- |
| 1 | 新增系统服务   | Binder IPC、SystemServer 启动流程 | 车身信息管理服务（AIDL）              | Framework + HAL        | ⭐⭐⭐  |
| 2 | 修改系统行为   | AMS/ATMS/WMS 内部逻辑            | Launcher 多任务动画、禁用系统对话框      | Framework              | ⭐⭐   |
| 3 | 适配硬件外设   | HAL 层、JNI、Input 子系统          | CAN 总线、串口屏、自定义键盘            | HAL + Kernel + SELinux | ⭐⭐⭐⭐ |
| 4 | 裁剪/定制系统  | 编译系统、分区、SELinux、Treble       | 移除系统应用、定制 Settings          | Build + sepolicy       | ⭐⭐   |
| 5 | 性能/稳定性优化 | 启动流程、ANR/GKI 内核              | 开机速度优化、ANR 根因分析             | Framework + Kernel     | ⭐⭐⭐⭐ |
| 6 | 安全策略配置   | SELinux、权限模型、non-SDK         | 新硬件 sepolicy、hiddenapi 限制放开 | sepolicy + Framework   | ⭐⭐⭐  |

---

## 场景一：新增系统服务 — 以「车身信息管理服务」为例

### 1.1 需求背景

车载项目需要新增一个 **VehicleBodyInfoService**（车身信息管理服务），对外提供车速、油量、车门状态等数据的查询接口。APP 通过 AIDL 跨进程调用获取数据。

### 1.2 整体架构

```
┌─────────────────────────────────────────────┐
│              第三方 APP                      │
│   (通过 AIDL 接口调用 IVehicleBodyInfo)       │
└──────────────┬──────────────────────────────┘
               │ ① Binder IPC (system 侧 AIDL)
               ▼
┌─────────────────────────────────────────────┐
│        SystemServer 进程 (system 分区)        │
│  ┌──────────────────────────────────┐       │
│  │  VehicleBodyInfoService          │       │
│  │  - 实现 IVehicleBodyInfo.Stub   │       │
│  │  - 注册到 ServiceManager         │       │
│  │  - 持有 IVehicleBody HAL 代理    │       │
│  └──────────┬───────────────────────┘       │
│             │ ② Binder (AIDL Stable HAL)
│             │   服务名: ...IVehicleBody/default
│             ▼                                 │
└──────────────┬──────────────────────────────┘
               │ ③ Binder 跨进程（system→vendor）
               ▼
┌─────────────────────────────────────────────┐
│   vendor.vehiclebody-hal 进程 (vendor 分区)   │
│  ┌──────────────────────────────────┐       │
│  │  VehicleBody (BnVehicleBody)     │       │
│  │  - 解析 CAN 报文 → 车速/油量/门  │       │
│  │  - 调用 CanBusReader 读 /dev/can0 │      │
│  └──────────┬───────────────────────┘       │
└──────────────┬──────────────────────────────┘
               │ ④ SocketCAN (PF_CAN socket)
               ▼
┌─────────────────────────────────────────────┐
│      Linux 内核 SocketCAN 驱动 (vendor/内核)    │
│     读取 CAN 总线 / 底层传感器数据            │
└─────────────────────────────────────────────┘
```

> 四段 IPC 链路：**APP →(Binder)→ Framework Service →(Binder/AIDL HAL)→ Vendor HAL 进程 →(SocketCAN)→ Kernel CAN 驱动**。其中 ② 即为本示例新增的 Stable HAL 边界，满足 Treble 隔离（system 不直接依赖 vendor 实现）。

### 1.3 实现步骤

#### Step 1: 定义 AIDL 接口

**文件路径**: `frameworks/base/core/java/android/os/IVehicleBodyInfo.aidl`

```aidl
// IVehicleBodyInfo.aidl
// ============================================================
// 车身信息管理服务 AIDL 接口定义
// 对外暴露：车速、油量、车门状态等只读查询能力
// 调用方：第三方车载 APP 通过 ServiceManager 获取代理
// ============================================================

package android.os;

/** @hide */  // @hide = 不对公开 SDK 暴露，仅系统内部使用
interface IVehicleBodyInfo {
    /**
     * 获取当前车速，单位 km/h
     * @return 当前车速，获取失败返回 -1
     */
    float getVehicleSpeed();

    /**
     * 获取当前油量百分比
     * @return 油量 0~100，-1 表示传感器异常
     */
    int getFuelLevel();

    /**
     * 获取所有车门状态
     * @return 位掩码格式：bit0=左前, bit1=右前, bit2=左后, bit3=右后
     *         1=开启, 0=关闭
     */
    int getDoorStatus();

    /**
     * 批量获取车身综合信息（减少 IPC 调用次数）
     * @return JSON 格式字符串，包含 speed/fuel/doors/engineTemp 等
     */
    String getAllBodyInfo();
}
```

#### Step 2: 实现 Service 端

**文件路径**: `frameworks/base/services/core/java/com/android/server/VehicleBodyInfoService.java`

```java
// VehicleBodyInfoService.java
// ============================================================
// 车身信息管理服务实现类
// 运行在 system_server 进程中，由 SystemServer 启动时实例化并注册
// ============================================================

package com.android.server;

import android.content.Context;
import android.hardware.vehiclebody.IVehicleBody;  // HAL 代理（AIDL Stable HAL）
import android.os.IBinder;
import android.os.IVehicleBodyInfo;
import android.os.ServiceManager;
import android.util.Slog;

/**
 * 车身信息管理服务的系统端实现。
 *
 * <p>生命周期说明：
 * <ul>
 *   <li>构造：SystemServer.main() 阶段通过 startOtherServices() 创建</li>
 *   <li>注册：构造完成后 publishBinderService() 注册到 ServiceManager</li>
 *   <li>销毁：随 system_server 退出而终止（正常情况不会销毁）</li>
 * </ul>
 *
 * <p>线程模型：
 * 所有 Binder 调用在 Binder 线程池中执行，
 * 无需额外创建线程。但如果底层 HAL 调用是阻塞的
 * （如 CAN 总线读取），建议异步化处理避免阻塞 Binder 线程。
 */
public final class VehicleBodyInfoService extends IVehicleBodyInfo.Stub {

    private static final String TAG = "VehicleBodyInfoService";
    // 服务名称，用于 ServiceManager 查找
    // 命名规范：android.os.Xxx 或 com.android.server.Xxx
    private static final String SERVICE_NAME = "vehicle_body_info";

    private final Context mContext;

    // ---- HAL 客户端代理（AIDL Stable HAL，binder 传输）----
    // HAL 接口：android.hardware.vehiclebody.IVehicleBody
    // 传输后端：binder（AIDL 默认后端，区别于 HIDL 的 hwbinder）
    // 服务实例名：android.hardware.vehiclebody.IVehicleBody/default
    // 运行进程：vendor.vehiclebody-hal（位于 vendor 分区）
    private volatile IVehicleBody mVehicleHal;
    // HAL 代理访问锁：getXxx() 可能在 HAL 代理尚未就绪时被并发调用
    private final Object mHalLock = new Object();

    public VehicleBodyInfoService(Context context) {
        mContext = context;
        Slog.i(TAG, "VehicleBodyInfoService initialized in system_server");
        // HAL 服务（vendor.vehiclebody-hal）可能晚于本服务启动，
        // 因此异步获取代理并带重试，避免阻塞 system_server 启动主线程。
        initVehicleHal();
    }

    /**
     * 异步获取 HAL 代理对象。
     *
     * <p>设计要点：
     * <ul>
     *   <li>不能在构造中同步等待——HAL 由 init 在 late-init 阶段拉起，
     *       此时可能尚未向 ServiceManager 注册。</li>
     *   <li>采用懒连接 + 退避重试：最多 5 次，每次间隔 1s。</li>
     *   <li>需用独立线程，否则会拖慢 system_server 整体启动。</li>
     * </ul>
     *
     * <p>如果最终仍未拿到代理，则 getXxx() 会返回约定错误码（-1），
     * 调用方据此判断为「硬件不可用」而非崩溃。
     */
    private void initVehicleHal() {
        final String halName = "android.hardware.vehiclebody.IVehicleBody/default";
        new Thread(() -> {
            IBinder binder = ServiceManager.getService(halName);
            for (int i = 0; i < 5 && binder == null; i++) {
                try { Thread.sleep(1000); } catch (InterruptedException ignored) {}
                binder = ServiceManager.getService(halName);
            }
            if (binder != null) {
                synchronized (mHalLock) {
                    mVehicleHal = IVehicleBody.Stub.asInterface(binder);
                }
                Slog.i(TAG, "HAL 代理已就绪: " + halName);
            } else {
                Slog.w(TAG, "HAL 代理获取失败(5次重试后)，将返回默认值");
            }
        }, "vbi-hal-init").start();
    }

    // ==================== AIDL 接口实现 ====================

    @Override
    public float getVehicleSpeed() {
        // 权限检查：确保调用者有权限访问车身信息
        enforceCallingPermission();

        // 通过 HAL 代理读取车速（跨进程 → vendor 分区 HAL 实现）
        synchronized (mHalLock) {
            if (mVehicleHal != null) {
                try {
                    return mVehicleHal.getSpeed();
                } catch (RemoteException e) {
                    Slog.e(TAG, "HAL getSpeed() 调用失败", e);
                }
            }
        }
        return -1.0f;  // HAL 不可用时的约定错误码
    }

    @Override
    public int getFuelLevel() {
        enforceCallingPermission();

        synchronized (mHalLock) {
            if (mVehicleHal != null) {
                try {
                    return mVehicleHal.getFuelLevel();
                } catch (RemoteException e) {
                    Slog.e(TAG, "HAL getFuelLevel() 调用失败", e);
                }
            }
        }
        return -1;  // 传感器异常 / HAL 不可用
    }

    @Override
    public int getDoorStatus() {
        enforceCallingPermission();

        // HAL 返回位掩码：bit0=左前, bit1=右前, bit2=左后, bit3=右后
        synchronized (mHalLock) {
            if (mVehicleHal != null) {
                try {
                    return mVehicleHal.getDoorStatus();
                } catch (RemoteException e) {
                    Slog.e(TAG, "HAL getDoorStatus() 调用失败", e);
                }
            }
        }
        return 0x00;  // 默认全部关闭
    }

    @Override
    public String getAllBodyInfo() {
        enforceCallingPermission();

        // 批量打包返回，减少 IPC 往返次数
        StringBuilder sb = new StringBuilder("{");
        sb.append("\"speed\":").append(getVehicleSpeed()).append(",");
        sb.append("\"fuel\":").append(getFuelLevel()).append(",");  // 修复：原 sb.append 调用语法错误
        sb.append("\"doors\":\"").append(Integer.toBinaryString(getDoorStatus())).append("\",");
        sb.append("\"engineTemp\":95.5");  // 示例字段
        sb.append("}");
        return sb.toString();
    }

    // ==================== 辅助方法 ====================

    /**
     * 权限校验：验证调用者是否持有车身信息访问权限。
     *
     * <p>需要在 platform_private.xml 中声明自定义权限：
     * <pre>
     * <permission
     *     android:name="android.permission.ACCESS_VEHICLE_BODY_INFO"
     *     android:protectionLevel="signature|privileged">
     * </pre>
     *
     * @throws SecurityException 调用者无权限时抛出
     */
    private void enforceCallingPermission() {
        mContext.enforceCallingOrSelfPermission(
            "android.permission.ACCESS_VEHICLE_BODY_INFO",
            "VehicleBodyInfoService: 需要车身信息访问权限");
    }
}
```

#### Step 3: 在 SystemServer 中注册启动

**文件路径**: `frameworks/base/services/java/com/android/server/SystemServer.java`

在 `startOtherServices()` 方法中添加注册代码：

```java
// ===== 在 SystemServer.startOtherServices() 中添加 =====
// 搜索位置：在 "ActivityManagerService.Service" 注册附近添加

try {
    // --- 新增：注册车身信息管理服务 ---
    // 时机说明：放在 AMS/WMS 之后启动，确保系统基础服务就绪
    // 如果该服务依赖 WindowManager（如需要显示 UI），则必须在 WMS 之后
    VehicleBodyInfoService vbi = new VehicleBodyInfoService(context);
    // publishBinderService 将服务注册到 ServiceManager
    // 之后任何进程都可以通过 ServiceManager.getService("vehicle_body_info") 获取
    ServiceManager.addService("vehicle_body_info", vbi);

    Slog.i(TAG, "VehicleBodyInfoService 已注册到 ServiceManager");
} catch (Throwable e) {
    Slog.e(TAG, "启动 VehicleBodyInfoService 失败", e);
}

// ---- 注意：不要放在 try-catch 块之外 ----
// SystemServer 的设计哲学是：单个服务启动失败不应导致整个系统崩溃
// 所以每个服务启动都应有独立的异常捕获
```

#### Step 4: 编译配置

**文件路径**: `frameworks/base/services/Android.bp`（或对应模块的 `Android.mk`）

在 `filegroup` / `srcs` 中加入新文件：

```blueprint
// frameworks/base/services/Android.bp
// 在 java_srcs 中追加：
java_libs_zip: {
    srcs: [
        // ... 其他文件 ...
        "core/java/com/android/server/VehicleBodyInfoService.java",
    ],
},
```

同时确保 AIDL 文件被编译：

```blueprint
// frameworks/base/Android.bp
// 确保 aidl 包含新接口
filegroup {
    name: "framework-core-sources",
    srcs: [
        "core/java/android/os/IVehicleBodyInfo.aidl",
        // ... 其他 aidl 文件 ...
    ],
}
```

#### Step 5: 权限声明

**文件路径**: `frameworks/base/core/res/AndroidManifest.xml` / `platform_private.xml`

```xml

<permission
    android:name="android.permission.ACCESS_VEHICLE_BODY_INFO"
    android:label="@string/permlab_access_vehicle_body"
    android:description="@string/permdesc_access_vehicle_body"
    android:protectionLevel="signature|privileged" />
```

`signature|privileged` 含义：

- **signature**: 只有与平台签名相同的 APK 才能获得此权限
- **privileged**: 系统特权应用（位于 `/system/priv-app`）也可获得
- 这保证了只有可信的系统/厂商应用能访问车身数据

### 1.4 APP 端调用示例

```java
// 第三方 APP 调用示例
public class VehicleInfoClient {
    private static final String SERVICE_NAME = "vehicle_body_info";

    public static float getCurrentSpeed(Context ctx) {
        // 1. 通过 ServiceManager 获取 Binder 代理
        IBinder binder = ServiceManager.getService(SERVICE_NAME);
        if (binder == null) {
            Log.e("VBI", "服务未找到，确认 system_server 是否已注册");
            return -1f;
        }

        // 2. 将 IBinder 转换为 AIDL 接口代理
        IVehicleBodyInfo service = IVehicleBodyInfo.Stub.asInterface(binder);

        try {
            // 3. 跨进程调用（实际走 Binder IPC）
            return service.getVehicleSpeed();
        } catch (RemoteException e) {
            Log.e("VBI", "IPC 调用异常", e);
            return -1f;
        }
    }
}
```

### 1.5 关键源码路径速查

| 组件                | AOSP 路径 (android-14.0.0_rXX)                                         | 说明                       |
| ----------------- | -------------------------------------------------------------------- | ------------------------ |
| SystemServer 主入口  | `frameworks/base/services/java/com/android/server/SystemServer.java` | 系统服务启动总调度                |
| ServiceManager 注册 | `frameworks/base/core/java/android/os/ServiceManager.java`           | addService / getService  |
| Binder 框架层        | `frameworks/base/core/java/android/os/Binder.java`                   | IPC 传输核心                 |
| AIDL 编译器          | `tools/base/aidl/`                                                   | .aidl → Java Stub 生成     |
| 权限管理              | `frameworks/base/core/java/android/app/ContextImpl.java`             | enforceCallingPermission |
| 日志系统              | `frameworks/base/core/java/android/util/Slog.java`                   | 系统级日志（非 logcat）          |
| HAL 接口（AIDL）    | `hardware/interfaces/vehiclebody/aidl/android/hardware/vehiclebody/IVehicleBody.aidl` | Stable HAL 契约 |
| HAL 编译模块         | `hardware/interfaces/vehiclebody/aidl/Android.bp`                   | `aidl_interface{ stability:"vintf" }` |
| HAL C++ 实现         | `hardware/interfaces/vehiclebody/default/VehicleBody.cpp`            | BnVehicleBody 子类 |
| HAL 进程入口         | `hardware/interfaces/vehiclebody/default/service.cpp`                | `AServiceManager_addService` |
| HAL SELinux 域       | `device/<vendor>/sepolicy/private/hal_vehiclebody_default.te`       | HAL 进程域策略 |

### 1.6 HAL 层完整实现（AIDL Stable HAL）

> **为什么需要独立的 HAL 层**：车身数据最终来自 CAN 总线 / 传感器，这些硬件驱动位于 **vendor 分区**。根据 Treble 隔离规范，system 分区（Framework）**禁止直接依赖** vendor 的具体实现，只能通过 **稳定的 AIDL/HIDL 接口** 访问。因此我们把「读 CAN → 解析报文 → 返回车速/油量」的逻辑封装成一个 Stable HAL，Framework 只调用接口，不关心底层是 FlexCAN 还是 MCP2515。

#### 1.6.1 目录结构与分层

```
hardware/interfaces/vehiclebody/
├── aidl/                              # AIDL Stable HAL 接口定义（vintf 冻结）
│   ├── Android.bp                    # aidl_interface 编译模块
│   ├── aidl_api/                    # 冻结后的 API 快照（frozen 时生成）
│   │   └── android.hardware.vehiclebody/
│   │       └── 1/                  # 版本 1 的接口哈希快照
│   └── android/hardware/vehiclebody/
│       ├── IVehicleBody.aidl        # 主接口
│       └── VehicleBodyInfo.aidl     # 可选：结构化返回体
└── default/                          # HAL 默认实现（C++，编译进 vendor 分区）
    ├── Android.bp                    # cc_binary + hal 依赖
    ├── service.cpp                   # 进程入口（binder 服务注册）
    ├── VehicleBody.h                # BnVehicleBody 子类声明
    ├── VehicleBody.cpp              # 接口实现（读 CAN / 解析）
    ├── CanBusReader.cpp             # 底层 CAN 读取（与 kernel SocketCAN 交互）
    ├── android.hardware.vehiclebody-service.rc   # init 启动脚本
    └── android.hardware.vehiclebody-service.xml  # VINTF manifest fragment
```

> **AIDL vs HIDL 选型**：Android 14 已废弃 HIDL（仅旧 HAL 保留），**新增 HAL 一律用 AIDL**。`stability: "vintf"` 表示接口受 CTS/VTS 冻结约束——一旦 `frozen: true`，接口签名不可再改（改了要升版本号）。

#### 1.6.2 AIDL HAL 接口定义

**文件路径**: `hardware/interfaces/vehiclebody/aidl/android/hardware/vehiclebody/IVehicleBody.aidl`

```aidl
// IVehicleBody.aidl
// ============================================================
// 车身信息 HAL 接口（AIDL Stable HAL）
// 传输后端：binder（区别于 HIDL 的 hwbinder）
// 实现位置：vendor 分区 (hardware/interfaces/vehiclebody/default)
// Framework 侧调用方：com.android.server.VehicleBodyInfoService
// ============================================================

package android.hardware.vehiclebody;

// 结构化返回体示例（可选，比散装字段更省 IPC 往返）
parcelable VehicleBodyInfo {
    float speed;        // 车速 km/h
    int fuelLevel;      // 油量百分比 0~100
    int doorStatus;     // 位掩码: bit0=左前 bit1=右前 bit2=左后 bit3=右后
    float engineTemp;   // 发动机温度 ℃
}

interface IVehicleBody {
    /**
     * 获取当前车速
     * @return 车速 km/h，异常返回 -1.0f
     */
    float getSpeed();

    /**
     * 获取油量
     * @return 0~100，异常返回 -1
     */
    int getFuelLevel();

    /**
     * 获取车门状态（位掩码）
     */
    int getDoorStatus();

    /**
     * 一次性获取全部车身信息（推荐，减少跨进程次数）
     */
    VehicleBodyInfo getAllInfo();
}
```

#### 1.6.3 AIDL 编译模块（接口冻结）

**文件路径**: `hardware/interfaces/vehiclebody/aidl/Android.bp`

```blueprint
// ============================================================
// aidl_interface 模块：把 .aidl 编译成 C++/Java/NDK 三套 stub
// 关键字段：
//   stability = "vintf"  → 纳入 Treble 兼容性冻结
//   backend.cpp / java / ndk → 生成哪些语言绑定
//   versions / frozen     → 接口版本与冻结状态
// ============================================================

aidl_interface {
    name: "android.hardware.vehiclebody",
    srcs: ["android/hardware/vehiclebody/*.aidl"],

    // 稳定性声明：接口受 VINTF 兼容性约束（升版本需保持向前兼容）
    stability: "vintf",

    // 生成 C++ (NDK) 与 Java 两套绑定
    backend: {
        cpp: {
            enabled: true,
            // NDK 后端生成 libbinder_ndk 风格的 Bn/Bp
        },
        java: {
            enabled: true,
            // Java 后端供 Framework（system_server）侧 asInterface 使用
        },
        ndk: { enabled: true },
    },

    // 接口版本管理
    versions: ["1"],
    // 开发阶段设为 false；接口稳定后改为 true 并提交 aidl_api/ 快照
    // 冻结后任何签名变更都会编译失败，强制走版本升级流程
    frozen: false,
}
```

#### 1.6.4 HAL C++ 实现 — 头文件

**文件路径**: `hardware/interfaces/vehiclebody/default/VehicleBody.h`

```cpp
// VehicleBody.h
// ============================================================
// AIDL HAL 接口的 C++ 服务端实现
// 继承 aidl 生成的 BnVehicleBody（Bn = Binder native / 服务端）
// 生成的头文件位于：
//   out/soong/.intermediates/hardware/interfaces/vehiclebody/.../
//       android/hardware/vehiclebody/BnVehicleBody.h
// ============================================================

#pragma once

#include <aidl/android/hardware/vehiclebody/BnVehicleBody.h>
#include <android/binder_interface_utils.h>

namespace aidl {
namespace android {
namespace hardware {
namespace vehiclebody {

class VehicleBody : public BnVehicleBody {
public:
    // 构造函数：初始化底层 CAN 读取器
    VehicleBody();
    ~VehicleBody() override;

    // ---- AIDL 接口实现（override 生成类声明的纯虚函数）----
    ndk::ScopedAStatus getSpeed(float* _aidl_return) override;
    ndk::ScopedAStatus getFuelLevel(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getDoorStatus(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getAllInfo(
            VehicleBodyInfo* _aidl_return) override;

private:
    // 底层 CAN 读取器（封装 SocketCAN 的 socket 操作）
    // 详见 1.6.7
    std::unique_ptr<CanBusReader> mCanReader;

    // 缓存最近一次解析结果的时间戳，用于简单节流
    // （避免每一帧 IPC 都触发一次 CAN 读，降低总线负载）
    std::chrono::steady_clock::time_point mLastRead;
};

}  // namespace vehiclebody
}  // namespace hardware
}  // namespace android
}  // namespace aidl
```

#### 1.6.5 HAL C++ 实现 — 实现文件

**文件路径**: `hardware/interfaces/vehiclebody/default/VehicleBody.cpp`

```cpp
// VehicleBody.cpp
// ============================================================
// 接口方法实现：从 CAN 总线读取报文 → 解析 → 返回
// 资源控制要点（车载场景强制）：
//   - CAN socket 在读线程内创建，避免阻塞 binder 线程
//   - 解析结果带 50ms 缓存，降低总线访问频率（反压）
//   - 所有异常路径返回约定错误码，不崩溃
// ============================================================

#include "VehicleBody.h"
#include "CanBusReader.h"
#include <android-base/logging.h>
#include <chrono>

namespace aidl {
namespace android {
namespace hardware {
namespace vehiclebody {

using namespace std::chrono_literals;

VehicleBody::VehicleBody() {
    // 构造时初始化 CAN 读取器（打开 can0 socket，绑定 0x7E0 等报文 ID）
    mCanReader = std::make_unique<CanBusReader>("/dev/can0");
    if (!mCanReader->init()) {
        LOG(WARNING) << "CAN 初始化失败，车身数据将返回错误码";
    }
}

VehicleBody::~VehicleBody() = default;

// ---- 车速：假设车速报文 ID=0x0C9，字节 1~2 为 big-endian km/h*10 ----
ndk::ScopedAStatus VehicleBody::getSpeed(float* _aidl_return) {
    if (!mCanReader || !mCanReader->isReady()) {
        *_aidl_return = -1.0f;
        return ndk::ScopedAStatus::ok();
    }
    auto frame = mCanReader->readFrame(0x0C9);
    if (!frame) {
        *_aidl_return = -1.0f;
        return ndk::ScopedAStatus::ok();
    }
    // 解析：字节 [1]<<8 | [2]，单位 0.1 km/h
    uint16_t raw = (frame->data[1] << 8) | frame->data[2];
    *_aidl_return = raw / 10.0f;
    return ndk::ScopedAStatus::ok();
}

// ---- 油量：报文 ID=0x1A0，字节 0 为 0~100 百分比 ----
ndk::ScopedAStatus VehicleBody::getFuelLevel(int32_t* _aidl_return) {
    if (!mCanReader || !mCanReader->isReady()) {
        *_aidl_return = -1;
        return ndk::ScopedAStatus::ok();
    }
    auto frame = mCanReader->readFrame(0x1A0);
    *_aidl_return = frame ? frame->data[0] : -1;
    return ndk::ScopedAStatus::ok();
}

// ---- 车门状态：报文 ID=0x2F1，bit 映射四门 ----
ndk::ScopedAStatus VehicleBody::getDoorStatus(int32_t* _aidl_return) {
    if (!mCanReader || !mCanReader->isReady()) {
        *_aidl_return = 0;
        return ndk::ScopedAStatus::ok();
    }
    auto frame = mCanReader->readFrame(0x2F1);
    *_aidl_return = frame ? (frame->data[0] & 0x0F) : 0;
    return ndk::ScopedAStatus::ok();
}

// ---- 批量获取（推荐路径）：一次返回结构，省去 3 次 IPC ----
ndk::ScopedAStatus VehicleBody::getAllInfo(VehicleBodyInfo* _aidl_return) {
    _aidl_return->speed      = getSpeedValue();     // 内部 helper
    _aidl_return->fuelLevel = getFuelValue();
    _aidl_return->doorStatus = getDoorValue();
    _aidl_return->engineTemp = 95.5f;             // 示例
    return ndk::ScopedAStatus::ok();
}

}  // namespace vehiclebody
}  // namespace hardware
}  // namespace android
}  // namespace aidl
```

#### 1.6.6 HAL 进程入口（binder 服务注册）

**文件路径**: `hardware/interfaces/vehiclebody/default/service.cpp`

```cpp
// service.cpp
// ============================================================
// HAL 守护进程入口：注册 binder 服务 → 进入线程池
// 关键点：
//   1. 用 NDK 的 AServiceManager_addService（AIDL over binder）
//      注意：不是 hwbinder 的 defaultPassthroughServiceImplementation
//   2. 实例名 = "<descriptor>/default"
//      descriptor = "android.hardware.vehiclebody.IVehicleBody"
//   3. 线程池最大 4 线程（车载场景足够，避免资源浪费）
// ============================================================

#include <android-base/logging.h>
#include <android/binder_process.h>
#include <android/binder_manager.h>
#include <android/binder_status.h>

#include "VehicleBody.h"

using aidl::android::hardware::vehiclebody::VehicleBody;

int main() {
    // 1. 设置 binder 线程池上限
    ABinderProcess_setThreadPoolMaxThreadCount(4);

    // 2. 创建 HAL 实现实例
    std::shared_ptr<VehicleBody> service =
        ndk::SharedRefBase::make<VehicleBody>();

    // 3. 拼装实例名并注册到 ServiceManager
    std::string instance =
        std::string(IVehicleBody::descriptor) + "/default";
    binder_status_t status =
        AServiceManager_addService(service->asBinder().get(),
                                   instance.c_str());
    CHECK(status == STATUS_OK)
        << "注册 HAL 服务失败: " << instance;

    LOG(INFO) << "VehicleBody HAL 已注册: " << instance;

    // 4. 进入线程池（阻塞，处理后续 binder 请求）
    ABinderProcess_joinThreadPool();
    return 0;  // 正常情况不会执行到这里
}
```

#### 1.6.7 底层 CAN 读取器（HAL ↔ Kernel SocketCAN）

**文件路径**: `hardware/interfaces/vehiclebody/default/CanBusReader.cpp`（节选核心）

```cpp
// CanBusReader.cpp（节选）
// ============================================================
// 通过 PF_CAN socket 读取内核 SocketCAN 报文
// 与 场景三 的 canutils 同源，但这里是 C++ 内嵌实现
// 资源控制：
//   - 非阻塞 socket + 短超时，避免读线程卡死
//   - 按 CAN ID 过滤，只收关心的报文（减少上下文切换）
// ============================================================

#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>

bool CanBusReader::init() {
    mSock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (mSock < 0) return false;

    struct ifreq ifr{};
    strcpy(ifr.ifr_name, mIfname.c_str());   // "can0"
    ioctl(mSock, SIOCGIFINDEX, &ifr);

    struct sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    bind(mSock, (struct sockaddr*)&addr, sizeof(addr));

    // 设置非阻塞 + 接收过滤器（只看 0x0C9/0x1A0/0x2F1）
    // ... setsockopt(SOL_CAN_RAW, CAN_RAW_FILTER, ...) ...
    return true;
}
```

#### 1.6.8 HAL 模块编译配置

**文件路径**: `hardware/interfaces/vehiclebody/default/Android.bp`

```blueprint
// ============================================================
// HAL 守护进程：编译为 vendor 分区的 cc_binary
// 关键属性：
//   init_rc         → 随镜像打包 init 脚本
//   vintf_fragments → 注册到设备 manifest（VINTF 兼容性扫描）
//   vendor: true   → 明确打进 vendor 分区（Treble 要求）
// ============================================================

cc_binary {
    name: "android.hardware.vehiclebody-service",
    vendor: true,                         // 必须，HAL 在 vendor 分区
    relative_install_path: "hw",          // 安装到 /vendor/bin/hw/
    srcs: [
        "service.cpp",
        "VehicleBody.cpp",
        "CanBusReader.cpp",
    ],
    shared_libs: [
        "libbinder_ndk",                 // AIDL NDK 后端
        "libbase",                       // android::base::LOG
        "android.hardware.vehiclebody-ndk_platform",
    ],
    init_rc: ["android.hardware.vehiclebody-service.rc"],
    vintf_fragments: ["android.hardware.vehiclebody-service.xml"],
}
```

**VINTF manifest fragment** — 告诉系统「我提供了一个 AIDL HAL」：

**文件路径**: `hardware/interfaces/vehiclebody/default/android.hardware.vehiclebody-service.xml`

```xml
<!-- VINTF 兼容性声明：必须存在，否则 lshal/vintf 扫描会报 missing HAL -->
<manifest version="1.0" type="device">
    <hal format="aidl">
        <name>android.hardware.vehiclebody</name>
        <version>1</version>
        <interface>
            <name>IVehicleBody</name>
            <instance>default</instance>
        </interface>
    </hal>
</manifest>
```

**init 启动脚本**：

**文件路径**: `hardware/interfaces/vehiclebody/default/android.hardware.vehiclebody-service.rc`

```
# ============================================================
# init.rc 片段：在 late-init 阶段拉起 HAL 守护进程
# class hal → 随 hwservicemanager 一同启动（vendor 进程组）
# user/vendor 域 → 以最低权限运行
# ============================================================

service vendor.vehiclebody-hal /vendor/bin/hw/android.hardware.vehiclebody-service
    class hal
    user system
    group system can            # can 组：访问 /dev/can0 所需
    capabilities NET_ADMIN       # 配置 CAN 网络接口所需
    oneshot                     # 异常退出不自动重启（调试期）；稳定后改为不写
```

#### 1.6.9 SELinux 策略（HAL 专属）

> 与场景三/六 的通用 sepolicy 不同，HAL 守护进程有专门的域宏。把策略放在 `device/<vendor>/sepolicy/` 下。

**`private/hal_vehiclebody.te`** — 定义服务类型：

```selinux
# ============================================================
# HAL 服务类型声明
# hal_vehiclebody_service 用于标记 binder 服务名的安全上下文
# ============================================================
type hal_vehiclebody_service, service_manager_type;
```

**`private/hal_vehiclebody_default.te`** — HAL 进程域 + 可执行文件域：

```selinux
# ============================================================
# HAL 守护进程域：hal_server_domain 自动授予
#   - binder 服务端基础权限
#   - 与 framework 的 binder 通信能力
# init_daemon_domain 让其由 init 以指定用户启动
# ============================================================
type hal_vehiclebody_default, domain;
type hal_vehiclebody_default_exec, exec_type, file_type;

# 继承 HAL 服务端基础域
hal_server_domain(hal_vehiclebody_default, hal_vehiclebody)

# init 以 vendor.vehiclebody-hal 拉起进程时打域
init_daemon_domain(hal_vehiclebody_default)

# 允许向 ServiceManager 注册本 HAL 服务
binder_service(hal_vehiclebody_default, hal_vehiclebody_service)

# 允许 system_server 调用本 HAL（Framework → HAL 方向的 binder call）
binder_call(system_server, hal_vehiclebody_default)

# CAN 设备 / sysfs 访问（读 can0 报文、配置接口）
allow hal_vehiclebody_default can_device:chr_file { read write open ioctl };
allow hal_vehiclebody_default sysfs_net:file { read write open };
allow hal_vehiclebody_default sysfs_net:dir { search read };

# netlink 配置 CAN 接口（ip link set can0 up）
allow hal_vehiclebody_default self:netlink_route_socket { create bind write read };
```

**`private/service_contexts`** — 把 binder 服务名映射到类型：

```
# 格式：<binder 服务全名> u:object_r:<type>:s0
android.hardware.vehiclebody.IVehicleBody/default u:object_r:hal_vehiclebody_service:s0
```

**`private/file_contexts`** — 给 HAL 可执行文件打标签：

```
/vendor/bin/hw/android\.hardware\.vehiclebody-service u:object_r:hal_vehiclebody_default_exec:s0
```

**`private/system_server.te`** — 允许 Framework 侧 find 该服务：

```selinux
# system_server 查找 HAL 服务（ServiceManager.getService）
allow system_server hal_vehiclebody_service:service_manager find;
```

#### 1.6.10 HAL 验证步骤

```bash
# === 1. 确认 HAL 进程已启动 ===
adb shell ps -A -Z | grep vehiclebody
# 期望输出（域应为 hal_vehiclebody_default）：
# u:r:hal_vehiclebody_default:s0 ... /vendor/bin/hw/android.hardware.vehiclebody-service

# === 2. 用 lshal 查看 AIDL HAL 注册情况 ===
adb shell lshal list -i | grep vehiclebody
# 期望: android.hardware.vehiclebody::IVehicleBody/default

# === 3. 用 service list 确认 binder 服务名 ===
adb shell service list | grep vehiclebody
# 期望: android.hardware.vehiclebody.IVehicleBody: [android.hardware.vehiclebody.IVehicleBody]

# === 4. 端到端验证：直接调 HAL（绕过 Framework）===
adb shell cmd vehiclebody getSpeed        # 若实现了 shell 命令
# 或写个临时 test client 调 AServiceManager_getService

# === 5. 确认 SELinux 无拒绝 ===
adb shell dmesg | grep avc | grep vehiclebody
# 空输出 = 策略正确

# === 6. 验证 VINTF 兼容性 ===
adb shell vintf status | grep vehiclebody
# 期望: 无 "missing" / "incompatible" 报错

# === 7. 全链路（App→Framework→HAL→CAN）===
# 在 Earth/车载 App 中调用 getVehicleSpeed()
# 同时另一端用 candump 发测试帧：
#   cansend can0 0C9#00000064   # 车速 = 0x0064/10 = 10.0 km/h
# App 应返回 10.0
```

#### 1.6.11 HAL 关键源码/配置路径速查

| 组件 | AOSP 路径 | 说明 |
|------|-----------|------|
| AIDL 接口定义 | `hardware/interfaces/vehiclebody/aidl/android/hardware/vehiclebody/IVehicleBody.aidl` | HAL 契约 |
| 接口编译模块 | `hardware/interfaces/vehiclebody/aidl/Android.bp` | `aidl_interface` |
| HAL 实现头文件 | `hardware/interfaces/vehiclebody/default/VehicleBody.h` | BnVehicleBody 子类 |
| HAL 实现 | `hardware/interfaces/vehiclebody/default/VehicleBody.cpp` | 报文解析逻辑 |
| 进程入口 | `hardware/interfaces/vehiclebody/default/service.cpp` | `AServiceManager_addService` |
| 编译配置 | `hardware/interfaces/vehiclebody/default/Android.bp` | `cc_binary` + `vintf_fragments` |
| VINTF 声明 | `.../android.hardware.vehiclebody-service.xml` | manifest fragment |
| init 脚本 | `.../android.hardware.vehiclebody-service.rc` | `service vendor.vehiclebody-hal` |
| NDK 绑定生成 | `out/soong/.intermediates/.../android/hardware/vehiclebody/` | Bn/Bp Stub |
| SELinux 域 | `device/<vendor>/sepolicy/private/hal_vehiclebody_default.te` | HAL 进程域 |
| 服务上下文 | `device/<vendor>/sepolicy/private/service_contexts` | binder 名→type |

---

## 场景二：修改系统行为 — 以「Launcher 多任务切换动画」和「禁用系统对话框」为例

### 2.1 案例 A：修改 Launcher3 最近任务切换动画

#### 需求

将默认的最近任务（Recents）切换动画改为自定义缩放效果，适配车载大屏交互。

#### 关键修改点

**文件路径**: `packages/apps/Launcher3/quickstep/src/com/android/quickstep/TouchInteractionService.java`

```java
// TouchInteractionService.java
// ============================================================
// Launcher3 的触摸交互服务入口
// 负责处理上滑手势触发最近任务预览的逻辑
// ============================================================

// ---- 原始逻辑（默认行为）----
// 上滑手势检测 → 开始 RecentAnimationController → 执行系统默认过渡动画

// ---- 修改方案：替换为自定义缩放动画 ----
// 1. 找到 onGestureStarted() 方法中的动画启动逻辑
// 2. 将默认的 Transition 动画替换为自定义 AnimatorSet

@Override
// 位于: packages/apps/Launcher3/quickstep/src/com/android/systemui/shared/recents/
//          animation/RecentAnimationController.java
public void startAnimation(
        Runnable animStartedListener,
        @Nullable Consumer<Boolean> onFinishedListener,
        boolean targetChange) {

    // ===== 原始代码：使用系统默认 Transition =====
    // mTransitionAnimator.startTransition(target, ...);

    // ===== 修改为：自定义缩放动画 =====
    // 车载场景优化要点：
    // - 动画时长缩短至 200ms（原默认 300~400ms），响应更快
    // - 使用 ScaleX/ScaleY 替代 Translation，视觉更紧凑
    // - 禁用 Z 轴升降（原版有 elevation 变化），减少 GPU 负担

    AnimatorSet customAnim = new AnimatorSet();
    ObjectAnimator scaleX = ObjectAnimator.ofFloat(mTargetView, View.SCALE_X,
            1.0f, 0.85f);      // 缩小到 85%
    ObjectAnimator scaleY = ObjectAnimator.ofFloat(mTargetView, View.SCALE_Y,
            1.0f, 0.85f);
    ObjectAnimator alpha = ObjectAnimator.ofFloat(mTargetView, View.ALPHA,
            1.0f, 0.7f);       // 半透明叠加

    customAnim.playTogether(scaleX, scaleY, alpha);
    customAnim.setDuration(200);  // 车载场景缩短动画时间
    customAnim.setInterpolator(new DecelerateInterpolator(1.5f));
    customAnim.addListener(new AnimatorListenerAdapter() {
        @Override
        public void onAnimationEnd(Animator animation) {
            if (onFinishedListener != null) {
                onFinishedListener.accept(true);
            }
        }
    });
    customAnim.start();

    if (animStartedListener != null) {
        animStartedListener.run();
    }
}
```

**另一个关键文件** — 任务视图布局:

**文件路径**: `packages/apps/Launcher3/quickstep/src/com/android/quickstep/views/TaskView.java`

```java
// TaskView.java
// ============================================================
// 最近任务列表中单个任务卡片的 View
// 修改卡片圆角、阴影、图标大小以适配车载屏幕
// ============================================================

// 修改 1: 卡片圆角加大（车载屏远距离观看）
mTaskCardRadius = dpToPx(16);  // 原 default 为 8dp

// 修改 2: 减少卡片间距（利用大屏空间）
mTaskCardMargin = dpToPx(8);   // 原 default 为 16dp

// 修改 3: 缩略图采样率降低（性能优化）
mThumbnailScale = 0.35f;        // 降低缩略图分辨率，节省显存
```

#### AMS/ATMS 相关修改（如需控制任务可见性）

**文件路径**: `frameworks/base/services/core/java/com/android/server/wm/TaskFragment.java`

```java
// TaskFragment.java
// ============================================================
// Task 的容器，控制 Task 的显示/隐藏/动画调度
// 如需禁止某些 Activity 出现在最近任务列表中，可在此拦截
// ============================================================

// 场景：特定包名的 Activity 不出现在 Recents 列表中
@Override
boolean shouldIgnoreForRecents(ActivityRecord r) {
    // 原始逻辑：仅判断是否为 home/assistant/launcher
    // if (r.isActivityTypeHome() || r.isActivityTypeAssistant()) ...

    // 修改：增加自定义过滤规则
    if (IGNORED_PACKAGES.contains(r.info.packageName)) {
        return true;  // 从最近任务列表中排除
    }
    return super.shouldIgnoreForRecents(r);
}
```

### 2.2 案例 B：禁用某个系统对话框

#### 需求

禁用系统级的「应用未响应」(ANR) 对话框，或禁用「USB 选择」对话框，适用于无人值守的车载设备。

#### 方案 1: 通过 Overlay 禁用（推荐，无需改源码）

```xml




<integer name="anr_delay">2147483647</integer>


<bool name="config_showAnrDialog">false</bool>
```

编译 overlay 并打包：

```bash
# 创建 RRO 包
mkdir -p vendor/myvendor/overlay/no_anr_dialog
cd vendor/myvendor/overlay/no_anr_dialog

# Android.bp
runtime_resource_overlay {
    name: "NoAnrDialogOverlay",
    target_package: "android",  // 覆盖 framework-res
}
```

#### 方案 2: 源码级修改（彻底禁用）

**文件路径**: `frameworks/base/services/core/java/com/android/server/am/AppErrors.java`

```java
// AppErrors.java
// ============================================================
// 应用错误处理中心，负责 ANR/Crash 弹窗逻辑
// handleAppCrash() / handleShowAnrUi() 是两个核心入口
// ============================================================

/**
 * 处理 ANR 弹窗显示逻辑
 * 原始行为：弹出 AppNotRespondingDialog 让用户选择"等待"或"强制停止"
 * 修改目标：静默记录日志，不弹窗
 */
@Override
public boolean handleShowAnrUi(Message msg) {
    AppNotRespondingDialog dialog = (AppNotRespondingDialog) msg.obj;

    // ===== 原始逻辑 =====
    // dialog.show();  // 显示对话框等待用户操作

    // ===== 修改：静默处理 =====
    // 1. 记录 ANR 到 logcat（保留排查线索）
    Slog.w(TAG, "ANR 静默处理: " + dialog.app.processName
            + " (PID=" + dialog.app.pid + ")");

    // 2. 自动选择"强制停止"策略（可选）
    // killProcess(dialog.app.pid);  // 直接杀进程

    // 3. 返回 true 表示已处理（不再弹窗）
    return true;

    // 注意：返回 false 则会继续走原始弹窗逻辑
}
```

**WMS 层面禁用系统对话框**:

**文件路径**: `frameworks/base/services/core/java/com/android/server/wm/AlertWindowNotification.java`

```java
// AlertWindowNotification.java
// ============================================================
// 系统 Alert 窗口（TYPE_SYSTEM_ALERT / TYPE_SYSTEM_DIALOG）的管理
// 可在此处按类型过滤不需要的系统弹窗
// ============================================================

// 拦截 USB 选择对话框
private boolean shouldSuppressSystemDialog(WindowState win) {
    // USB 配置选择弹窗的包名特征
    if ("com.android.systemui".equals(win.getOwningPackage())
            && win.attrs.type == TYPE_SYSTEM_DIALOG) {
        // 检查是否为 USB 相关 Dialog
        String title = win.attrs.getTitle().toString();
        if (title.contains("USB") || title.contains("usb")) {
            Slog.i(TAG, "已抑制系统对话框: " + title);
            return true;
        }
    }
    return false;
}
```

### 2.3 关键源码路径速查

| 组件              | AOSP 路径                                                                                                              | 说明        |
| --------------- | -------------------------------------------------------------------------------------------------------------------- | --------- |
| Launcher3 手势处理  | `packages/apps/Launcher3/quickstep/src/com/android/quickstep/TouchInteractionService.java`                           | 上滑手势入口    |
| 最近任务动画控制器       | `packages/apps/Launcher3/quickstep/src/com/android/systemui/shared/recents/animation/RecentAnimationController.java` | 过渡动画编排    |
| 任务视图            | `packages/apps/Launcher3/quickstep/src/com/android/quickstep/views/TaskView.java`                                    | 单个任务卡片    |
| ANR 错误处理        | `frameworks/base/services/core/java/com/android/server/am/AppErrors.java`                                            | ANR 弹窗控制  |
| Crash 处理        | `frameworks/base/services/core/java/com/android/server/am/ProcessRecord.java`                                        | 进程崩溃管理    |
| WMS 窗口策略        | `frameworks/base/services/core/java/com/android/server/wm/DisplayPolicy.java`                                        | 窗口类型权限判定  |
| SystemUI Dialog | `frameworks/base/packages/SystemUI/src/com/android/systemui/dialogs/`                                                | 各类系统对话框实现 |

---

## 场景三：适配硬件外设 — 以 CAN 总线、串口屏、自定义键盘为例

### 3.1 案例 A: 适配 CAN 总线

#### 架构层次

```
┌──────────────────────────────────────────┐
│           APP / CarService                │
├──────────────────────────────────────────┤
│          Vehicle HAL (AIDL)              │  ← hardware/interfaces/vehicle
├──────────────────────────────────────────┤
│          Vehicle HAL Daemon               │  ← hardware/interfaces/vehicle/impl
├──────────┬───────────────────────────────┤
│ SocketCAN│ Linux Kernel CAN Driver        │  ← drivers/net/can/
│ (can-utils)│ (m_can / sja1000 etc.)      │
├──────────┴───────────────────────────────┤
│          DTS 设备树 (CAN 控制器节点)       │
└──────────────────────────────────────────┘
```

#### Step 1: 内核驱动与 DTS 配置

**DTS 设备树节点** (以 NXP i.MX8 的 FlexCAN 为例):

```dts
// arch/arm64/boot/dts/freescale/imx8mp-evk.dts
// ============================================================
// CAN 控制器设备树节点配置
// 关键属性：clocks / pinctrl / status / assigned-clocks
// ============================================================

/* CAN1: 车辆主总线 (动力CAN) */
&flexcan1 {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_flexcan1>;   /* 引脚复用配置 */

    /* CAN 时钟源：必须匹配硬件手册要求 */
    assigned-clocks = <&clk IMX8MP_CLK_CAN1_ROOT>;
    assigned-clock-parents = <&clk IMX8MP_SYS_PLL1_40M>;
    assigned-clock-rates = <40000000>;  /* 40MHz 时钟 */

    status = "okay";                    /* 使能节点 */

    /* 可选：最大传输单元（标准帧 vs 扩展帧） */
    /* can-transceiver { */
    /*     max-bitrate = <5000000>; */  /* 5Mbps (CAN FD) */
    /* }; */
};

/* 引脚复用：CAN1_TX / CAN1_RX */
&iomuxc {
    pinctrl_flexcan1: flexcan1grp {
        fsl,pins = <
            MX8MP_IOMUXC_I2C2_SCL__FLEXCAN1_TX   /* CAN 发送引脚 */
            MX8MP_IOMUXC_I2C2_SDA__FLEXCAN1_RX   /* CAN 接收引脚 */
        >;
    };
};
```

**内核 defconfig 必要宏开关**:

```bash
# arch/arm64/configs/xxx_defconfig
# ============================================================
# CAN 子系统内核配置项
# 缺少任一项都会导致 CAN 设备无法识别或功能缺失
# ============================================================

CONFIG_CAN=y                    # CAN 核心框架（必须）
CONFIG_CAN_DEV=y                # CAN 通用设备接口（必须）
CONFIG_CAN_FLEXCAN=y            # NXP FlexCAN 驱动（根据芯片选型）
CONFIG_CAN_M_CAN=y              # Bosch M_CAN 驱动（替代方案）
CONFIG_CAN_RAW=y                # SocketCAN 原始套接字支持
CONFIG_CAN_BCM=y                # Broadcast Manager（广播多帧合并）
CONFIG_NET_CAN=y                # CAN 网络子系统
CONFIG_CAN_J1939=y              # J1939 协议栈（商用车场景可选）
CONFIG_ISOTP=y                  # ISO-TP 传输协议（UDS 诊断必选）
```

#### Step 2: 用户空间 CAN 测试验证

```bash
# === CAN 总线基本测试命令 ===

# 1. 加载驱动后查看 CAN 网络接口
ip link show type can
# 输出示例: 1: can0: <NOARP,ECHO> mtu 16 ...

# 2. 配置 CAN 接口（波特率 500kbps，采样点 87.5%）
ip link set can0 up type can bitrate 500000 \
    sample-point 0.875 \
    restart-ms 100
#   bitrate:   总线波特率（常见值：125k/250k/500k/1M）
#   sample-point: 采样点位置（0.000~1.000，推荐 0.750~0.875）
#   restart-ms:  Bus-Off 自动恢复间隔(ms)，0=不自动恢复

# 3. CAN-FD 模式（如果硬件支持）
ip link set can0 up type can fd on \
    dbitrate 2000000 dsample-point 0.800

# 4. 发送测试帧（标准帧 ID=0x123，数据=[0xDE, 0xAD, 0xBE, 0xEF]）
cansend can0 123#DEADBEEF

# 5. 持续接收 CAN 帧（带时间戳）
candump can0 -ta
#   -t: 显示绝对时间戳
#   -a: 显示全部帧（不过滤）

# 6. 使用 candump 过滤特定 ID
candump can0,0x100:0x7FF,0x200:0x7FF  # 只看 ID 0x100~0x1FF 和 0x200~0x2FF

# 7. 性能统计
ip -details -statistics link show can0
```

#### Step 3: SELinux 策略适配

**文件路径**: `device/<vendor>/<product>/sepolicy/private/file.te` / `.te`

```selinux
# ============================================================
# CAN 设备节点的 SELinux 域/类型定义
# 确保系统服务/APP 有权访问 /dev/ 和 /sys/class/net/can*
# ============================================================

# 定义 CAN 设备类型
type can_device, dev_type;
type sysfs_can, sysfs_type, file_type;

# Vehicle HAL Daemon 域访问 CAN 设备
allow vehicle_hal can_device:chr_file { read write open ioctl };
allow vehicle_hal sysfs_can:dir { search read };
allow vehicle_hal sysfs_can:file { read write open };

# 允许使用 netlink socket 操作 CAN 网络接口
allow vehicle_hal self:netlink_route_socket { create bind write read };

# 允许执行 ip-link 命令配置 CAN 接口（通过 vold/domain.te）
# 或直接在 domain.te 中添加通用规则:
# allow domain self:netlink_route_socket create;
```

**SELinux AVC denied 排障命令**:

```bash
# 1. 实时监控 AVC 拒绝日志
adb shell "dmesg -w | grep avc"

# 2. 从已有日志提取拒绝规则
adb shell "dmesg | grep avc | audit2allow -p out/target/product/xxx/sepolicy"

# 3. 一键生成补丁 te 文件
adb pull /sys/fs/selinux/policy
audit2allow -p policy < avc_log.txt > can_fix.te
```

### 3.2 案例 B: 串口屏适配

#### 架构方案

```
APP (HMI)
  │
  ├── JNI 调用 ──→ libserial_port.so (native 库)
  │                   │
  └─── 或直接 ─────→ /dev/ttyS* (串口设备节点)
```

**JNI 层实现**:

**文件路径**: `frameworks/base/core/jni/android_hardware_SerialPort.cpp`

```cpp
// android_hardware_SerialPort.cpp
// ============================================================
// 串口通信 JNI 桥接层
// 提供 open/close/read/write 给 Java 层调用
// 底层通过 termios2 配置串口参数
// ============================================================

#include <jni.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <asm/ioctls.h>

#define SERIAL_PORT_PATH "/dev/ttyS3"  // 串口屏对应的 tty 设备

extern "C" JNIEXPORT jint JNICALL
Java_android_hardware_SerialPort_nativeOpen(JNIEnv *env, jclass clazz) {
    // 打开串口设备
    int fd = open(SERIAL_PORT_PATH, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        LOGE("无法打开串口 %s: %s", SERIAL_PORT_PATH, strerror(errno));
        return -1;
    }

    // 配置串口参数（8N1, 115200bps）
    struct termios2 tio;
    ioctl(fd, TCGETS2, &tio);
    tio.c_cflag &= ~PARENB;    // 无校验
    tio.c_cflag &= ~CSTOPB;    // 1 位停止位
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;        // 8 位数据位
    tio.c_ispeed = 115200;     // 输入波特率
    tio.c_ospeed = 115200;     // 输出波特率
    ioctl(fd, TCSETS2, &tio);

    LOGI("串口 %s 已打开, fd=%d, 115200 8N1", SERIAL_PORT_PATH, fd);
    return fd;
}

extern "C" JNIEXPORT jint JNICALL
Java_android_hardware_SerialPort_nativeWrite(
        JNIEnv *env, jclass clazz, jint fd, jbyteArray data) {
    jsize len = env->GetArrayLength(data);
    jbyte *buf = env->GetByteArrayElements(data, nullptr);
    ssize_t written = write(fd, buf, len);
    env->ReleaseByteArrayElements(data, buf, 0);
    return written;
}
```

### 3.3 案例 C: 自定义键盘/输入设备

#### Input 子系统架构

```
用户空间
  │
  ├── EventHub (扫描 /dev/input/event*)
  ├── InputReader (解析 raw event)
  ├── InputDispatcher (分发 key/touch/motion event)
  │
  ▼
内核空间
  ├── drivers/input/evdev.c       (event 设备接口)
  ├── drivers/input/keyboard/     (键盘驱动)
  └── drivers/hid/                (USB HID 设备)
```

**自定义按键映射文件**:

**文件路径**: `device/<vendor>/<product>/gpio-keys.kl`

```
# ============================================================
# GPIO 按键映射文件 (.kl = Key Layout)
# 格式: key <Linux keycode> <Android keycode>:<flags>
# 系统启动时由 InputReader 加载
# ============================================================

# 自定义车载硬键映射
key 115   KEYCODE_MEDIA_PLAY_PAUSE   VOLUME  # 多媒体键
key 402   KEYCODE_NAVIGATION_UP      WAKE    # 导航上键 + 唤醒
key 403   KEYCODE_NAVIGATION_DOWN    WAKE    # 导航下键
key 404   KEYCODE_NAVIGATION_LEFT    WAKE    # 导航左键
key 405   KEYCODE_NAVIGATION_RIGHT   WAKE    # 导航右键
key 102   KEYCODE_HOME               WAKE    # HOME 键
key 158   KEYCODE_BACK               WAKE    # 返回键

# flags 说明:
#   WAKE   : 按下时唤醒屏幕
#   VOLUME : 音量键特殊处理（长按连续触发）
#   WAKE_DROPPED: 唤醒但不传递给应用
```

**InputDeviceType 配置** (决定设备类别):

**文件路径**: `device/<vendor>/<product>/input/device_id.idc`

```
# ============================================================
# Input Device Configuration (.idc)
# 控制输入设备的分类和行为参数
# ============================================================

# 触摸屏参数（串口屏触摸部分）
touch.deviceType = touchScreen
touch.orientationAware = 1

keyboard.layout = gpio_keys
keyboard.characterMap = gpio_keys.kl

# 自定义键盘的特殊行为
keyboard.builtIn = true          # 内建键盘（不被拔插影响）
keyboard.navigationKeys = 1      # 作为导航键处理
```

### 3.4 关键源码路径速查

| 组件               | AOSP 路径                                                                  | 说明                |
| ---------------- | ------------------------------------------------------------------------ | ----------------- |
| CAN 驱动 (FlexCAN) | `drivers/net/can/flexcan.c`                                              | NXP FlexCAN 驱动主文件 |
| CAN 驱动 (M_CAN)   | `drivers/net/can/m_can/m_can.c`                                          | Bosch M_CAN 驱动    |
| SocketCAN 核心     | `include/uapi/linux/can.h`                                               | CAN 帧结构体定义        |
| InputReader      | `frameworks/native/services/inputflinger/reader/InputReader.cpp`         | 输入事件读取            |
| InputDispatcher  | `frameworks/native/services/inputflinger/dispatcher/InputDispatcher.cpp` | 输入事件分发            |
| EventHub         | `frameworks/native/services/inputflinger/EventHub.cpp`                   | /dev/input 扫描     |
| 按键映射加载           | `frameworks/native/libs/input/KeyLayoutMap.cpp`                          | .kl 文件解析          |
| JNI 寄存器          | `frameworks/base/core/jni/android_runtime_AndroidRuntime.cpp`            | native 方法注册表      |

---

## 场景四：裁剪/定制系统 — 以移除系统应用和定制 Settings 为例

### 4.1 移除不需要的系统应用

#### 方案 1: 通过构建变量裁剪（最常用）

**文件路径**: `device/<vendor>/<product>/device.mk`

```makefile
# device.mk
# ============================================================
# 产品构建配置：通过 PACKAGES 变量增删系统应用
# ============================================================

# ---- 移除不需要的 AOSP 预装应用 ----
# $(call remove-package, <package-name>) 会从系统镜像中剔除指定模块

# 移除桌面相关（替换为自定义 Launcher）
$(call remove-package, Launcher3QuickStep)
$(call remove-package, Launcher3)

# 移除不需要的应用（车载场景无用）
$(call remove-package, Calendar)          # 日历
$(call remove-package, DeskClock)         # 闹钟时钟
$(call remove-package, Gallery2)          # 图库
$(call remove-package, Camera2)           # 相机（无摄像头硬件时可移除）
$(call remove-package, Email)             # 邮件客户端
$(call remove-package, Exchange)          # Exchange 同步
$(call remove-package, QuickSearchBox)    # 搜索框
$(call remove-package, SoundRecorder)     # 录音机
$(call remove-package, Calculator)        # 计算器（可选保留）

# ---- 添加自定义应用 ----
PRODUCT_PACKAGES += \
    MyCustomLauncher \     # 自定义车载 Launcher
    VehicleSettings \      # 车载专用 Settings
    CanMonitor \           # CAN 监控工具
    FactoryTest \          # 工厂测试 APP
    PreloadApps            # 预装第三方应用目录
```

#### 方案 2: 通过 `PRODUCT_PACKAGE_OVERLAYS` 覆盖

```makefile
# 用空 overlay 替换掉整个 app
PRODUCT_PACKAGE_OVERLAYS += vendor/myvendor/overlay/remove_apps

# vendor/myvendor/overlay/remove_apps/packages/apps/Calendar/Android.mk
# 内容: 空文件或 LOCAL_MODULE_TAGS := optional（不安装）
```

#### 方案 3: 运行时 pm disable（运行期禁用）

```bash
# 对于无法从编译层面移除的 GMS 组件，可在 first boot 时禁用
adb shell pm disable-user --user 0 com.google.android.googlequicksearchbox
adb shell pm disable-user --user 0 com.google.android.apps.maps
adb shell pm disable-user --user 0 com.google.android.youtube

# --user 0: 仅对主用户生效
# disable-user: 用户可通过设置重新启用（比 disable 更灵活）
```

### 4.2 定制 Settings 菜单项

#### 需求

隐藏车载场景无用的设置项（Wi-Fi 高级选项、NFC、打印等），新增车辆专属设置入口。

#### 修改 Settings Dashboard 分类

**文件路径**: `packages/apps/Settings/src/com/android/settings/dashboard/CategoryKey.java`

```java
// CategoryKey.java
// ============================================================
// Settings Dashboard 页面的分类 Key 定义
// 每个 Category 对应设置主页的一个卡片/入口
// ============================================================

// ---- 新增车辆专属分类 ----
public static final String CATEGORY_VEHICLE =
    "com.android.settings.category.vehicle";

public static final String CATEGORY_VEHICLE_DISPLAY =
    "com.android.settings.category.vehicle_display";

public static final String CATEGORY_VEHICLE_NETWORK =
    "com.android.settings.category.vehicle_network";
```

**Dashboard 片段注册**:

**文件路径**: `packages/apps/Settings/AndroidManifest.xml` / dashboard categories

```xml



<activity
    android:name=".settings.vehicle.VehicleSettingsActivity"
    android:label="@string/vehicle_settings_title"
    android:icon="@drawable/ic_car_settings">
    <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="com.android.settings.category.vehicle" />
    </intent-filter>
    
    <meta-data
        android:name="com.android.settings.category"
        android:value="com.android.settings.category.vehicle" />
    <meta-data
        android:name="com.android.settings.order"
        android:value="-120" />  
</activity>
```

**隐藏不需要的 Settings 入口**:

**文件路径**: `packages/apps/Settings/src/com/android/settings/SettingsActivity.java`

```java
// SettingsActivity.java
// ============================================================
// Settings 主 Activity，负责加载各子页面 Fragment
// 通过 isDashboardFeatureSupported() 控制入口可见性
// ============================================================

// 方案 1: 在 DashboardFeatureProvider 中过滤
@Override
public List<DashboardCategory> getAllCategories() {
    List<DashboardCategory> categories = super.getAllCategories();

    // 过滤掉车载场景不需要的分类
    categories.removeIf(cat ->
        cat.key.equals(CategoryKey.KEY_CATEGORY_NFC)           // NFC
        || cat.key.equals(CategoryKey.KEY_CATEGORY_PRINTING)    // 打印
        || cat.key.equals(CategoryKey.KEY_CATEGORY_CONNECTED_DEVICE)  // 部分蓝牙高级选项
    );

    return categories;
}
```

**更轻量的方式 — 使用 Config.xml 控制**:

```xml



<bool name="config_show_wifi_settings">true</bool>
<bool name="config_show_bluetooth_settings">true</bool>
<bool name="config_show_nfc_settings">false</bool>      
<bool name="config_show_print_settings">false</bool>     
<bool name="config_show_home_settings">false</bool>      
<bool name="config_show_sound_settings">true</bool>
<bool name="config_show_display_settings">true</bool>
<bool name="config_show_vehicle_settings">true</bool>    
```

### 4.3 Treble 分区合规性

```
┌─────────────────────────────────┐
│        System 分区 (只读)         │
│  - Framework jar/apk            │
│  - 预装 privileged app          │
│  - GSI (Generic System Image)   │
├─────────────────────────────────┤
│        Vendor 分区 (厂商定制)     │
│  - HAL 实现 (.so)               │
│  - 厂商 init.rc                 │
│  - 厂商 SELinux policy          │
│  - 厂商 RRO overlay             │
│  - 自定义内核模块 (.ko)          │
├─────────────────────────────────┤
│        Product 分区 (产品定制)    │
│  - 产品特有 app                 │
│  - 产品特有配置                  │
│  - 产品 UI 资源                  │
└─────────────────────────────────┘
```

**重要原则**:

- **Vendor 分区不能依赖 System 分区的具体实现**（只能依赖 stable AIDL/HIDL 接口）
- **Product 分区可以依赖 System 分区**
- 自定义系统服务放 **System**；HAL 实现放 **Vendor**；产品 APP 放 **Product**

### 4.4 关键源码路径速查

| 组件                     | AOSP 路径                                                                               | 说明                 |
| ---------------------- | ------------------------------------------------------------------------------------- | ------------------ |
| 构建系统主入口                | `build/make/core/main.mk`                                                             | make 编译入口          |
| 产品定义                   | `build/make/target/product/`                                                          | generic/aosp 等基线产品 |
| Package 移除宏            | `build/make/core/package_installer.mk`                                                | remove-package 实现  |
| Settings Dashboard     | `packages/apps/Settings/src/com/android/settings/dashboard/`                          | 设置页分类体系            |
| Settings Feature Flags | `packages/apps/Settings/res/values/`                                                  | config.xml 控制开关    |
| Overlay 机制             | `frameworks/base/core/res/` + `overlay/`                                              | RRO 资源覆盖           |
| PM (PackageManager)    | `frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java` | 应用安装/卸载/启用/禁用      |
| Treble 合规检查            | `build/make/tools/vts/`                                                               | VTS 测试套件           |

---

## 场景五：性能/稳定性优化 — 以开机速度和 ANR 分析为例

### 5.1 优化开机速度

#### Android 启动流程总览

```
时间轴 ──────────────────────────────────────────────────────→

BootROM → Bootloader → Kernel → Init → Zygote → SystemServer → Launcher
 0ms       ~500ms       ~2s      ~3s     ~5s       ~8s        ~10s+
 │         │           │        │       │         │          │
 │         │           │        │       │         │    ← 此阶段可优化范围最大
 ▼         ▼           ▼        ▼       ▼         ▼
 硬件初始化  加载内核    挂载    fork    启动核心    桌面可用
           zImage     文件系统  进程    系统服务
```

#### Phase 1: Kernel 启动优化

**文件路径**: `kernel/common/arch/arm64/kernel/` / 内核 defconfig

```bash
# === 内核启动加速配置 ===

# 1. 压缩内核体积（减少加载时间）
CONFIG_KERNEL_GZIP=y           # gzip 压缩（压缩率高）
# CONFIG_KERNEL_LZ4=y          # lz4 解压更快但压缩率低（二选一）

# 2. 减少不必要的驱动探测（省去逐个 probe 的时间）
# 在 defconfig 中禁用不需要的驱动 =n：
CONFIG_DRM=n                   # 不需要 DRM 显卡驱动
CONFIG_SOUND=n                 # 不需要音频驱动（如有独立模块）
CONFIG_USB=n                   # 不需要 USB（车载场景可选）

# 3. 内核并发初始化
CONFIG_INITRAMFS_SOURCE=""     # 空 initramfs（减少解压时间）
CONFIG_BLK_DEV_INITRD=y        # 但保留 initrd 支持

# 4. printk 优化：减少启动时的串口输出（串口很慢！）
# 在 kernel command line 中添加:
# quiet loglevel=0
```

#### Phase 2: Init 阶段优化

**文件路径**: `system/core/init/init.cpp` / `system/core/rootdir/init.rc`

```rc
# init.rc
# ============================================================
# Init 进程配置：按需延迟启动非关键服务
# early-init → init → late-init 三阶段
# ============================================================

on early-init
    # 挂载核心文件系统（必须尽早完成）
    mount tmpfs tmpfs /mode 0751 gid=1009

on init
    # ---- 关键优化：延迟非必要服务 ----
    # 原始：很多 service 设置 disabled=false 导致开机即启动
    # 优化：标记为 disabled，由 trigger 延迟启动

    # 不需要开机立即启动的服务示例:
    # service logd /system/bin/logd
    #     class core
    #     disabled          # ← 改为 disabled
    #     on boot           # ← 改为 boot 阶段再启动

on boot
    # 启动之前 disabled 的非关键服务
    start logd
    start vold
    # ... 其他服务

on property:sys.boot_completed=1
    # boot_completed 之后启动的服务（完全不影响开机感知速度）
    start incidentd
    start statsd
    start storaged
```

#### Phase 3: Zygote 预加载优化

**文件路径**: `frameworks/base/core/java/com/android/internal/os/ZygoteInit.java`

```java
// ZygoteInit.java
// ============================================================
// Zygote 进程初始化：预加载类和资源是开机耗时的主要来源之一
// preload() 方法在 fork 任何 app 之前执行一次
// ============================================================

static void preload(TimingsTraceLog traceLog) {
    traceLog.traceBegin("BeginPreloads");

    // === 优化 1: 预加载类列表裁剪 ===
    // 文件: frameworks/base/preloaded-classes
    // 原始: 预加载约 6000+ 个类，耗时 ~1s
    // 优化: 移除车载场景用不到的类（Telephony/Print/NFC 等）
    // 每减少 100 个类约节省 15~30ms
    preloadClasses();

    // === 优化 2: 预加载资源裁剪 ===
    // 文件: frameworks/base/core/res/preloaded_drawables
    // 原始: 预加载大量 drawable/color/dimen
    // 优化: 仅保留系统必需的资源
    preloadResources();

    // === 优化 3: 共享库预加载 ===
    // 这些库会被所有 app 继承（copy-on-write），预加载可避免重复加载
    preloadSharedLibraries();

    // === 优化 4: WebView 准备（非常耗时！）===
    // 原始: 开机就初始化 WebView（即使当前没有 web 页面）
    // 优化: 延迟到首次使用 WebViewFactory.getProvider() 时懒加载
    // prepareWebViewInZygote();  // 注释掉或延迟

    traceLog.traceEnd("EndPreloads");
}
```

**预加载类列表裁剪**:

**文件路径**: `frameworks/base/preloaded-classes`

```
# preloaded-classes
# ============================================================
# Zygote 预加载类列表（每行一个全限定类名）
# 由 frameworks/base/tools/preload/PreloadClassTool.java 生成
#
# 裁剪原则：
# 1. 保留所有 android.* / java.* / javax.* 核心类
# 2. 保留 com.android.internal.* 框架内部类
# 3. 移除不需要的功能类（标注 # REMOVE）
# ============================================================

android.R$styleable
android.app.Activity
android.app.AlertDialog
# android.bluetooth.*          # REMOVE: 车载无蓝牙时
# android.print.*              # REMOVE: 无打印功能
# android.nfc.*                # REMOVE: 无 NFC
# android.telephony.*          # 保留：可能用到电话状态
com.android.internal.R$styleable
com.android.internal.policy.PhoneWindow
# com.android.printservice.*   # REMOVE
```

#### Phase 4: SystemServer 启动优化

**文件路径**: `frameworks/base/services/java/com/android/server/SystemServer.java`

```java
// SystemServer.java
// ============================================================
// 系统服务启动顺序与并行度优化
// startBootstrapServices() → startCoreServices() → startOtherServices()
# ============================================================

// === 优化策略 ===

// 1. 非关键服务延迟启动
// 原始: 所有服务在 startOtherServices() 中同步启动
// 优化: 将非关键服务移到 boot completed 之后

// 示例: 将 ClipboardService 延迟到 boot 后
// 原始位置: startOtherServices() 中直接 startService()
// 优化:
/*
try {
    // 不在这里启动
    // clipboardService = new ClipboardService(...);
    // ServiceManager.addService("clipboard", clipboardService);
} catch (Throwable e) { ... }
*/
// 改为在 phase_boot_completed 回调中启动

// 2. 并行启动无依赖关系的服务
// 原始: 串行启动（AMS → PMS → WMS → ...）
// 优化: 利用 Runnable + handlerThread 并行化
// 注意：有依赖关系的不能并行（如 PKMS 必须在 AMS 之前）

// 3. 减少 PKMS 扫描
// PKMS (PackageManagerService) 扫描所有 APK 最耗时（可达 3~5s）
// 优化手段:
// - 减少 /system/priv-app 和 /system/app 下的 APK 数量
// - 使用 odex/oat 预编译缓存（dex2oat）
// - 启用 dexopt 第一阶段并行: pm.dexopt.first-use=speed-profile
```

#### Phase 5: Launcher 启动优化

**文件路径**: `packages/apps/Launcher3/src/com/android/launcher3/Launcher.java`

```java
// Launcher.java
// ============================================================
// Launcher 启动是用户感知到的"开机完成"时刻
// 主要耗时点: 图标加载、Widget 加载、数据库查询
// ============================================================

protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // === 优化 1: 延迟加载 Widget ===
    // Widgets 需要跨进程查询 Provider，非常慢
    // 优化: 先显示空桌面，异步加载 Widget
    mModel.enqueueModelUpdateTask(new AsyncWidgetLoader());

    // === 优化 2: 图标缓存预热 ===
    // 首次启动无缓存，每个图标都要 decode bitmap
    // 优化: 在 SystemServer 阶段预先生成图标缓存
    // 或使用 IconCache.preload()

    // === 优化 3: 减少首帧绘制内容 ===
    // 先绘制 Workspace 背景 + Hotseat，再渐入图标
    setAlpha(0f);
    animate().alpha(1f).setDuration(300).setInterpolator(new DecelerateInterpolator());
}
```

#### 性能 profiling 工具

```bash
# === 开机时间精确测量 ===

# 1. bootchart: 生成完整的启动过程时序图
android.bootchart=30  # 添加到 kernel cmdline
# 开机后取 /data/bootchart/*.tgz，用 bootchart 渲染

# 2. logcat 关键事件时间戳
adb shell "logcat -b main -d | grep -E '(BootProgress|SystemServer|Zygote|Launcher)'"

# 关键 tag:
# BootProgress:  各阶段开始时间
# Zygote:        Zygote 就绪
# SystemServer:  各服务启动时间
# ActivityManager: Launcher 启动完成

# 3. perfetto trace（最强大的系统级 tracing）
# 录制: adb shell 'perfetto --txt perfetto_config.pbtx --out /data/local/tmp/boot_trace'
# 分析: 打开 https://ui.perftetto.dev 加载 trace 文件

# 4. simpleperf 热点函数分析
adb shell simpleperf record -g -p $(pidof system_server) -o boot_perf.data
```

### 5.2 解决系统服务 ANR

#### ANR 触发机制

```
主线程阻塞
  │
  ▼
超过阈值时间？
  ├─ Input dispatch: 5s (前台) / 10s (后台)
  ├─ Broadcast:       foreground=10s / background=60s
  ├─ Service:         foreground=20s / background=200s
  └─ ContentProvider: publish 超时 10s
  │
  ▼
AppNotRespondingDialog.show()
  或（如果设置了 crashOnANR）
Process.killProcess(SIGQUIT) → tombstone
```

#### ANR 分析方法

**第一步：获取 ANR traces**

```bash
# ANR traces 存储位置
/data/anr/traces.txt          # 最新一次 ANR
/data/anr/traces.db           # 历史记录（如果启用持久化）

# 提取最新 ANR 信息
adb shell cat /data/anr/traces.txt | head -500

# 关注关键字段:
# "main" tid=1:  主线程堆栈 ← 这是 ANR 的根因
# "blocked on" / "waiting on":  死锁/等待信息
# "held mutexes":  持有的锁
```

**第二步：典型 ANR 场景与解决方案**

**场景 1: 主线程 Binder 调用阻塞**

```java
// ❌ 问题代码：主线程发起同步 Binder 调用
public void onClick(View v) {
    // 这个调用会阻塞主线程直到对方返回
    // 如果对方服务繁忙（如正在 GC 或处理大量请求），就会 ANR
    VehicleInfo info = mVehicleService.getAllBodyInfoSync();  // 危险！
    updateUI(info);
}

// ✅ 修复方案：异步调用
public void onClick(View v) {
    CompletableFuture.supplyAsync(() -> {
        return mVehicleService.getAllBodyInfoSync();  // 子线程调用
    }).thenAcceptAsync(info -> {
        updateUI(info);  // 回到主线程更新 UI
    }, MainThreadExecutor.get());
}
```

**场景 2: 主线程锁竞争**

```java
// ❌ 问题代码：子线程持锁太久，主线程被阻塞
private final Object mLock = new Object();

// 子线程
new Thread(() -> {
    synchronized (mLock) {
        doHeavyIoOperation();  // 耗时 6s → 主线程 ANR！
    }
}).start();

// 主线程
public void onDraw(Canvas canvas) {
    synchronized (mLock) {     // 等待子线程释放锁...
        drawData(canvas);
    }
}

// ✅ 修复方案：缩小锁粒度 / 读写锁 / 无锁数据结构
private final ReentrantReadWriteLock rwLock = new ReentrantReadWriteLock();

// 读操作用读锁（不互斥）
rwLock.readLock().lock();
drawData(canvas);
rwLock.readLock().unlock();
```

**场景 3: SystemServer 自身 ANR**

SystemServer 的 ANR 通常意味着某个系统服务的主线程 Handler 消息处理过慢。

**关键文件**: `frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java`

```java
// AMS.java
// ============================================================
// AMS 的 mainHandler 消息处理是最常见的 ANR 热点
// 常见耗时消息: LAUNCH_ACTIVITY / BIND_SERVICE / BROADCAST_INTENT
// ============================================================

// 排查方法: 在 handleMessage 中加耗时日志
class MainHandler extends Handler {
    public void handleMessage(Message msg) {
        long start = SystemClock.uptimeMillis();
        switch (msg.what) {
            case LAUNCH_ACTIVITY:
                handleLaunchActivity(msg);  // 可能耗时？
                break;
            // ...
        }
        long cost = SystemClock.uptimeMillis() - start;
        if (cost > 100) {
            Slog.w(TAG, "MainHandler msg=" + msg.what + " cost=" + cost + "ms");
        }
    }
}
```

### 5.3 GKI 内核调优

**GKI (Generic Kernel Image)** 要求厂商不能修改核心内核镜像，但允许通过以下方式优化：

```bash
# === GKI 允许的内核优化手段 ===

# 1. 内核模块 (.ko) — 可以动态加载
# vendor/lib/modules/ 下放置自定义模块
insmod /vendor/lib/modules/my_can_filter.ko

# 2. sysfs / procfs / debugfs 参数调优
# 这些不属于 GKI 保护范围

# CPU 调度策略（针对大核小核架构）
echo 1024 > /proc/sys/kernel/sched_wakeup_granularity_ns
echo 7500000 > /proc/sys/kernel/sched_latency_ns

# 内存管理
echo 10 > /proc/sys/vm/swappiness        # 减少交换倾向
echo 50 > /proc/sys/vm/dirty_ratio         # 脏页回写阈值
echo 1 > /proc/sys/vm/overcommit_memory    # 允许内存过度分配

# TCP/IP 网络（如果 CAN-over-TCP）
echo 1 > /proc/sys/net/ipv4/tcp_low_latency  # 低延迟模式

# 3. Device Tree Overlay (DTO)
# 可以在不修改主 dtb 的情况下叠加设备树节点
# 用于添加新的 CAN/I2C/SPI 设备节点
```

### 5.4 关键源码路径速查

| 组件           | AOSP 路径                                                                                | 说明             |
| ------------ | -------------------------------------------------------------------------------------- | -------------- |
| Init 主逻辑     | `system/core/init/init.cpp`                                                            | init 进程入口      |
| Init rc 解析   | `system/core/init/action_parser.cpp`                                                   | .rc 文件语法解析     |
| Zygote 初始化   | `frameworks/base/core/java/com/android/internal/os/ZygoteInit.java`                    | 预加载            |
| Zygote fork  | `frameworks/base/core/java/com/android/internal/os/Zygote.java`                        | 进程孵化           |
| SystemServer | `frameworks/base/services/java/com/android/server/SystemServer.java`                   | 服务启动总控         |
| AMS 主线程      | `frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java` | 消息循环           |
| ANR 收集       | `frameworks/base/core/java/com/android/server/am/AppNotRespondingDialog.java`          | ANR 弹窗         |
| traces 生成    | `art/runtime/thread_list.cc`                                                           | Java 线程 dump   |
| BootChart    | `system/core/logcat/event.logtags`                                                     | 启动事件标签         |
| Perfetto     | `platform2/perfetto/protos/trace/`                                                     | trace proto 定义 |

---

## 场景六：安全策略配置 — 以 sepolicy 和 hiddenapi 为例

### 6.1 为新增硬件节点配置 SELinux 策略

#### 完整流程

```
1. 发现 AVC denied (logcat/dmesg)
        ↓
2. 分析审计日志 (audit2allow)
        ↓
3. 编写/修改 .te 策略文件
        ↓
4. 编译验证 (checkpolicy -C -e)
        ↓
5. 刷机验证 (make selinux_tests)
```

#### 实战：为 CAN 设备添加完整策略

**Step 1: 定义新类型**

**文件路径**: `device/<vendor>/sepolicy/private/hwservice.te`

```selinux
# hwservice.te
# ============================================================
# HWService 类型定义
# 用于 HAL 层 hwbinder 服务的 SELinux 标签
# ============================================================

# 定义 Vehicle HAL 的 hwservice 类型
type hal_vehicle_hwservice, hwservice_manager_type;
```

**Step 2: 定义域**

**文件路径**: `device/<vendor>/sepolicy/private/hal_vehicle.te`

```selinux
# hal_vehicle.te
# ============================================================
# Vehicle HAL Daemon 的 SELinux 域定义
# 该 daemon 运行在 vendor 分区，负责与 CAN 总线通信
# ============================================================

# Vehicle HAL 域：基于 halserver_default 基础域扩展
type hal_vehicle, domain;

# 继承 halserver 基础权限
halserver_domain(hal_vehicle, hal_vehicle)

# ---- 文件系统访问 ----
# 允许读写 CAN 设备节点
allow hal_vehicle can_device:chr_file { read write open ioctl };

# 允许访问 sysfs 下的 CAN 配置
allow hal_vehicle sysfs_net:file { read write open };
allow hal_vehicle sysfs_net:dir { search read };

# ---- Binder / HWBinder 通信 ----
# 允许向 framework 层的 Vehicle HAL 客户端提供 hwbinder 服务
allow hal_vehicle hal_vehicle_client:hwbinder { transfer call };

# 允许调用其他 HAL 服务（如 sensors、display）
binder_call(hal_vehicle, hal_sensors_server)
binder_call(hal_vehicle, hal_graphics_composer_default)

# ---- 网络操作 ----
# 允许使用 netlink socket 配置 CAN 网络接口
allow hal_vehicle self:netlink_route_socket { create bind write read getattr setattr };

# 允许使用 unix datagram socket 进行进程间通信
allow hal_vehicle self:unix_dgram_socket { create bind read write };

# ---- 特定权限 ----
# 允许执行系统命令（ip link 等）
allow hal_vendor shell_exec:file { execute open read };
allow hal_vendor system_file:file { execute_no_trans };

# ---- 日志 ----
allow hal_vehicle logd:socket { connectto write read };
allow hal_vehicle logfile:file { append open write };
```

**Step 3: 属性/上下文关联**

**文件路径**: `device/<vendor>/sepolicy/private/hal_vehicle_contexts`

```
# hal_vehicle_contexts
# ============================================================
# 将 HAL 服务名映射到 SELinux 安全上下文
# 格式: interface_prefix:instance_name u:object_r:security_type:s0
# ============================================================

android.hardware.vehicle::IVehicle/default u:object_r:hal_vehicle_hwservice:s0
```

**Step 4: 设备节点标签**

**文件路径**: `device/<vendor>/sepolicy/private/file_contexts`

```
# file_contexts
# ============================================================
# 设备文件的 SELinux 安全上下文映射
# 在 init 创建设备节点时自动打标签
# ============================================================

# CAN 设备节点
/dev/can(/.*)?    u:object_r:can_device:s0
/sys/class/net/can.* u:object_r:sysfs_net:s0

# Vehicle HAL 可执行文件
/vendor/bin/hw/android\.hardware\.vehicle@.*-service  u:object_r:hal_vehicle_exec:s0
```

**Step 5: 编译与验证**

```bash
# 编译 sepolicy（单独编译，无需整编）
mmm system/sepolicy

# 或使用 checkpolicy 单文件验证
checkpolicy -C -e -M -c 30 out/target/product/xxx/sepolicy.policy hal_vehicle.te

# 刷机后验证策略是否正确加载
adb shell "getenforce"                          # 应输出 Enforcing
adb shell "ls -Z /dev/can0"                       # 查看 can0 的安全上下文
adb shell "ps -AZ | grep vehicle"                 # 查看 vehicle daemon 的 domain

# 模拟操作触发策略检查
adb shell "runcon u:r:hal_vehicle:s0 ip link set can0 up type can bitrate 500000"
```

### 6.2 放开 hiddenAPI 限制

#### 背景

Android 9 起引入 **hidden API 黑名单机制**，限制 APP 通过反射调用 `@hide` 标注的 SDK 接口。车载/工控场景有时需要访问 hidden API。

#### 配置级别

```
Level 0 (max):    所有 API 都可访问（等同于关闭限制）
Level 1 (sdk):    非 greylist/blacklist 的 hidden API 可访问
Level 2 (core):   仅 sdk 公开 API + 部分 greylist
Level 3 (strict): 严格模式（默认生产环境）
```

#### 修改方法

**方法 1: 配置文件（推荐）**

**文件路径**: `frameworks/base/config/hiddenapi-force-hidden.txt` / `hiddenapi-greylist-max-o.txt`

```
# hiddenapi-greylist-max-o.txt
# ============================================================
# Hidden API 灰名单配置
# 列在此文件中的 hidden API 对特定签名 APP 可访问
# 每行一个方法签名的完整描述
# ============================================================

# 示例：开放 ActivityTaskManager 的 hidden 方法
Landroid/app/ActivityTaskManager;->moveTaskToFront(Landroid/content/Intent;I)V

# 开放 WindowManager 的 hidden 方法
Landroid/view/WindowManagerGlobal;->getWindowSession()Landroid/view/IWindowSession;

# 开发放 SystemProperties 的 hidden setter
Landroid/os/SystemProperties;->set(Ljava/lang/String;Ljava/langString;)V
```

**方法 2: 运行时豁免**

**文件路径**: `frameworks/base/core/java/com/android/internal/os/ZygoteInit.java` 或 `RuntimeInit.java`

```java
// 在 VMRuntime 中设置 hidden API 策略
VMRuntime runtime = VMRuntime.getRuntime();

// 设置 hidden API 访问策略
// setHiddenApiExemptions() 的参数是一个前缀列表
// 匹配此前缀的所有 hidden API 都将被豁免
runtime.setHiddenApiExemptions(new String[] {
    "Landroid/app/",          // 豁免 android.app 包下所有 hidden API
    "Landroid/os/",           // 豁免 android.os 包
    "Landroid/view/",         // 豁免 android.view 包
    "Lcom/android/internal/", // 豁免 internal 包
});

// 注意：这会影响所有从该 Zygote fork 出来的进程
// 如果只想针对特定 APP，应在 APP 进程创建时单独设置
```

**方法 3: 编译时注解**

```java
// 在源码中使用 @UnsupportedAppUsage 注解
// 编译器会将带此注解的方法自动加入 greylist-max-o
@UnsupportedAppUsage
public void someHiddenMethod() {
    // ...
}

// @UnsupportedAppUsage 的 maxTargetSdk 参数控制:
// maxTargetSdk=29: 仅 targetSdk <= 29 的 APP 可访问
// maxTargetSdk=Integer.MAX_VALUE: 所有版本都可访问
@UnsupportedAppUsage(maxTargetSdk = Integer.MAX_VALUE)
public HiddenApiClass getHiddenObject() {
    return new HiddenApiClass();
}
```

#### non-SDK 接口限制排查

```bash
# 当 APP 因 hidden API 限制崩溃时，logcat 输出:
# Accessing hidden field Landroid/os/MessageQueue;->mPtr:J ...
#   (dark greylist, core-platform-api, reflection)

# 查看当前设备的 hidden API 策略
adb shell "settings get global hidden_api_policy"

# 临时调试：设置为 max（不限制）
adb shell "settings put global hidden_api_policy 1"

# 查看哪些 hidden API 被访问了
adb shell "logcat -b main -d | grep 'Accessing hidden'"
```

### 6.3 权限模型补充

**自定义系统权限**:

**文件路径**: `frameworks/base/core/res/AndroidManifest.xml`

```xml

<permission
    android:name="android.permission.CAN_BUS_ACCESS"
    android:label="@string/permlab_can_access"
    android:description="@string/permdesc_can_access"
    android:protectionLevel="signature" />


<permission
    android:name="android.permission.VEHICLE_DIAGNOSTIC"
    android:label="@string/permlab_vehicle_diag"
    android:description="@string/permdesc_vehicle_diag"
    android:protectionLevel="signature|privileged" />


<permission
    android:name="android.permission.READ_VEHICLE_STATUS"
    android:label="@string/permlab_read_vehicle"
    android:description="@string/permdesc_read_vehicle"
    android:protectionLevel="dangerous" />
```

### 6.4 关键源码路径速查

| 组件                    | AOSP 路径                                                                               | 说明                |
| --------------------- | ------------------------------------------------------------------------------------- | ----------------- |
| SELinux 编译系统          | `system/sepolicy/`                                                                    | 策略源码根目录           |
| sepolicy 公共定义         | `public/attribute.te`                                                                 | 公共属性/类型           |
| sepolicy 私有定义         | `private/` / `vendor/`                                                                | 厂商私有策略            |
| 策略编译器                 | `external/selinux/libsepol/cil/`                                                      | CIL 中间语言编译        |
| Hidden API 策略         | `frameworks/base/config/hiddenapi-*.txt`                                              | 黑/白/灰名单           |
| Hidden API 强制         | `art/runtime/hidden_api.cc`                                                           | ART 运行时检查         |
| VMRuntime             | `libcore/dalvik/src/main/java/dalvik/system/VMRuntime.java`                           | hidden API 豁免接口   |
| Permission 检查         | `frameworks/base/core/java/android/app/ContextImpl.java`                              | enforcePermission |
| PackageManagerService | `frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java` | 权限授予决策            |

---

## 附录：通用调试排障工具箱

### 日志命令速查

```bash
# === 系统级日志 ===

# 实时跟踪系统服务日志
adb logcat -s "VehicleBodyInfoService:*" "ActivityManager:*" "WindowManager:*"

# 过滤 Binder 事务日志（调试 IPC 问题）
adb logcat -b all | grep -i "binder"

# 内核日志（驱动/中断问题）
adb shell dmesg -w | grep -E "(CAN|binder|oom)"

# === 进程状态 ===

# system_server 进程状态（线程数、内存、CPU）
adb shell top -p $(pidof system_server)

# Binder 事务统计（发现 IPC 瓶颈）
adb shell dumpsys binder_stats --all

# === 系统服务 Dump ===

# AMS 完整状态（Activity 栈、进程、内存）
adb shell dumpsys activity

# WMS 窗口层级
adb shell dumpsys window

# PMS 所有已安装包
adb shell dumpsys package | grep -E "(Package|versionName)"

# === ANR 排障 ===

# 获取最新 ANR traces
adb shell cat /data/anr/traces.txt

# 查看历史 ANR
adb shell ls -la /data/anr/

# === SELinux 排障 ===

# 实时监控 AVC 拒绝
adb shell "dmesg -w | grep avc | grep denied"

# 生成补丁规则
adb shell "dmesg | grep avc | audit2allow -p /sys/fs/selinux/policy"
```

### 编译命令参考

```bash
# === AOSP 标准编译流程 ===

# 1. 初始化环境
source build/envsetup.sh

# 2. 选择编译目标
lunch aosp_<target>-eng
# 常见 target:
#   aosp_arm64-eng          (ARM64 模拟器)
#   aosp_car_x86_64-eng      (车载 x86_64)
#   <vendor>_<product>-eng  (厂商定制产品)

# 3. 编译（32 线程，适合 E5-2697A v4 64G）
make -j32

# 4. 仅编译单个模块（增量编译快）
mmm frameworks/base/services
mmm packages/apps/Launcher3

# 5. 编译后刷机
adb reboot bootloader
fastboot flash system out/target/product/<target>/system.img
fastboot flash vendor out/target/product/<target>/vendor.img
fastboot reboot

# 6. 仅推送单个文件（开发调试阶段）
adb root && adb remount
adb push out/target/product/<target>/system/framework/services.jar /system/framework/
adb reboot
```

---

## 版本信息

| 项目         | 值                                   |
| ---------- | ----------------------------------- |
| 适用 AOSP 版本 | Android 14 (UpsideDownCake, API 34) |
| 源码分支       | android-14.0.0_rXX                  |
| 内核分支       | android14-6.1 (GKI)                 |
| 文档版本       | v1.0                                |
| 最后更新       | 2026-07-27                          |
