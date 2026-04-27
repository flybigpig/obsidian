用户想了解 Android 系统启动过程中 BOOT_PROGRESS 的各个阶段，特别是与 PMS 相关的阶段。让我搜索更多关于 BOOT_PROGRESS 的详细信息。
让我搜索 EventLogTags 文件来找到 BOOT_PROGRESS 的所有阶段定义。
EventLogTags.java 可能是自动生成的。让我搜索 BOOT_PROGRESS 相关的完整定义。
让我用不同的方式搜索 SystemServer.java 和 BOOT_PROGRESS 相关内容。
找到了 SystemServer.java，让我搜索 BOOT_PROGRESS 相关的阶段。
让我搜索更多 BOOT_PROGRESS 阶段，包括 AMS 和其他系统组件中的阶段。
搜索结果为空，可能是因为文件太大或者搜索范围问题。让我尝试更精确地搜索。
让我搜索更多关于 BOOT_PROGRESS 的定义，包括 PMS 的 logtags 文件。
让我搜索 PMS 中的 logtags 文件。
让我读取 PMS 构造函数中各阶段之间做了哪些工作，以便给用户更详细的解释。
现在我已经有了足够的信息来详细解释 BOOT_PROGRESS 阶段，特别是 PMS 相关的各个阶段。
## Android BOOT_PROGRESS 启动阶段详解

### 全局启动阶段概览

Android 系统启动过程中通过 `EventLog` 记录各关键阶段的时间戳（单位：ms），用于 **性能分析、启动优化和 ANR 追踪**。可通过 `adb logcat -b events` 查看：

```
adb logcat -b events | grep boot_progress
```

---

### PMS 内部 5 大阶段

```
时间轴 ──────────────────────────────────────────────────────────────►

PMS_START ─────► SYSTEM_SCAN_START ───► DATA_SCAN_START ──► SCAN_END ──────► PMS_READY
   │                    │                     │                │               │
   ▼                    ▼                     ▼                ▼               ▼
 构造函数           扫描系统分区          扫描数据分区       权限/设置        持久化完成
 开始执行           (system/vendor)      (/data/app)        更新            可对外服务
```

---

### 阶段 1: `BOOT_PROGRESS_PMS_START` (第 2407 行)

```java
EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_START, SystemClock.uptimeMillis());
```

**时机**：构造函数入口，PMS 创建的起点



**此阶段工作内容**：


| 序号  | 操作                                                      | 说明                                                            |
| --- | ------------------------------------------------------- | ------------------------------------------------------------- |
| 1   | 安装全局锁                                                   | `LockGuard.installLock(mPackages)`                            |
| 2   | 初始化 Context                                             | `mContext = context`                                          |
| 3   | 注册本地服务                                                  | `LocalServices.addService(PackageManagerInternal.class, ...)` |
| 4   | 创建 UserManagerService                                   | 多用户管理子服务                                                      |
| 5   | 创建 ComponentResolver                                    | Intent 解析器                                                    |
| 6   | 创建 PermissionManagerService                             | 权限管理服务                                                        |
| 7   | 创建 Settings 对象                                          | 从 `packages.xml` 读取持久化配置                                      |
| 8   | 注册共享用户                                                  | system/phone/log/bluetooth/shell/se/networkstack              |
| 9   | 创建 PackageDexOptimizer / DexManager / ArtManagerService | DEX 编译管理                                                      |

---

### 阶段 2: `BOOT_PROGRESS_PMS_SYSTEM_SCAN_START` (第 2565 行)

```java
EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_SYSTEM_SCAN_START, startTime);
```

**时机**：开始扫描系统分区中的所有 APK 包

**扫描顺序与目录**：
```
1. /vendor/overlay         ← Vendor 资源覆盖层     [SCAN_AS_VENDOR]
2. /product/overlay        ← Product 资源覆盖层    [SCAN_AS_PRODUCT]  
3. /odm/overlay            ← ODM 资源覆盖层        [SCAN_AS_ODM]
4. /oem/overlay            ← OEM 资源覆盖层        [SCAN_AS_OEM]

5. /system/framework       ← Framework 资源包      [SCAN_NO_DEX | SCAN_AS_PRIVILEGED]
   ★ 必须包含 "android" 包（否则抛异常）

6. /system/priv-app       ← 特权系统应用          [SCAN_AS_PRIVILEGED]
7. /system/app            ← 普通系统应用           [SCAN_AS_SYSTEM]

8. /vendor/priv-app       ← Vendor 特权应用        [SCAN_AS_VENDOR | SCAN_AS_PRIVILEGED]
9. /vendor/app            ← Vendor 应用             [SCAN_AS_VENDOR]

10. /odm/priv-app         ← ODM 特权应用           [SCAN_AS_VENDOR | SCAN_AS_PRIVILEGED]
11. /odm/app              ← ODM 应用                [SCAN_AS_VENDOR]
```

每个目录调用 `scanDirTracedLI()` → 内部遍历 APK 文件 → `scanPackageTracedLI()` → 解析 AndroidManifest.xml → 构建 `PackageParser.Package` → 加入 `mPackages`

---

### 阶段 3: `BOOT_PROGRESS_PMS_DATA_SCAN_START` (第 2935 行)

```java
EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_DATA_SCAN_START, SystemClock.uptimeMillis());
scanDirTracedLI(sAppInstallDir, 0, scanFlags | SCAN_REQUIRE_KNOWN, 0);
```

**时机**：系统分区扫描完成后，开始扫描数据分区

**此阶段工作内容**：

| 序号 | 操作 | 说明 |
|------|------|------|
| 1 | 扫描 `/data/app` | 第三方安装的应用 |
| 2 | OTA 清理 | 处理已删除/更新的系统包 (`possiblyDeletedUpdatedSystemApps`) |
| 3 | 回退恢复 | 如果 `/data` 分区的更新包不存在，回退到系统版本 (`mExpectingBetter`) |

---

### 阶段 4: `BOOT_PROGRESS_PMS_SCAN_END` (第 3165 行)

```java
EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_SCAN_END, SystemClock.uptimeMillis());
Slog.i(TAG, "Time to scan packages: " + ((SystemClock.uptimeMillis()-startTime)/1000f) + " seconds");
```

**时机**：所有包扫描完成

**此阶段工作内容**：

| 序号 | 操作 | 说明 |
|------|------|------|
| 1 | 读取包使用统计 | `mPackageUsage.read(mPackages)` |
| 2 | 读取编译器统计 | `mCompilerStats.read()` |
| 3 | SDK 升级处理 | 若 SDK 版本变更，重新授予权限 `updateAllPermissions()` |
| 4 | 默认首选应用 | `applyDefaultPreferredAppsLPw()` (首次启动时) |
| 5 | 域名验证初始化 | `primeDomainVerificationsLPw()` |
| 6 | 准备应用存储 | `reconcileAppsDataLI()` + 异步线程准备 AppData |
| 7 | OTA 缓存清理 | 清除代码缓存目录 (`clearAppDataLIF`) |
| 8 | Q 升级兼容性 | 隐藏旧版非系统应用的图标 |

---

### 阶段 5: `BOOT_PROGRESS_PMS_READY` (第 3291 行)

```java
mSettings.writeLPr();  // 持久化 packages.xml
EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_READY, SystemClock.uptimeMillis());
```

**时机**：PMS 初始化完成，可以接受外部请求

**此后操作**：
- 设置验证器包 (`mRequiredVerifierPackage`)
- 设置安装器包 (`mRequiredInstallerPackage`)
- 设置 Intent 过滤验证组件
- 注册 Package 交互回调 (`registerDexModuleRegisterCallback()`)
- GC 回收内存 (`Runtime.getRuntime().gc()`)

---

### 系统级其他 BOOT_PROGRESS 阶段

```
┌────────────────────────────────────────────────────────────────────────────┐
│                         Android 完整启动流程                                │
│                                                                            │
│  Bootloader                                                                │
│    ↓                                                                       │
│  Kernel init                                                               │
│    ↓                                                                       │
│  Init 进程                                                                 │
│    │                                                                       │
│    ├─→ Zygote (fork SystemServer)                                          │
│    │       │                                                               │
│    │       ▼                                                               │
│    │   ┌──────────────────────────────────────────────────────────┐        │
│    │   │              SystemServer.main()                        │        │
│    │   │                                                       │        │
│    │   │   BOOT_PROGRESS_SYSTEM_RUN  (EventLog:433)             │        │
│    │   │       │                                               │        │
│    │   │       ├── startBootstrapServices()                    │        │
│    │   │       │   └── PMS.main()                              │        │
│    │   │       │       └── new PackageManagerService()         │        │
│    │   │       │           ├── PMS_START                       │        │
│    │   │       │           ├── PMS_SYSTEM_SCAN_START           │        │
│    │   │       │           ├── PMS_DATA_SCAN_START             │        │
│    │   │       │           ├── PMS_SCAN_END                    │        │
│    │   │       │           └── PMS_READY ◄──── 当前位置        │        │
│    │   │       │                                           │        │
│    │   │       ├── startCoreServices()                      │        │
│    │   │       ├── startOtherServices()                     │        │
│    │   │       │   └── AMS.systemReady()                    │        │
│    │   │       │       └── BOOT_PROGRESS_AMS_READY          │        │
│    │   │       │                                          │        │
│    │   │       └── Looper.loop()                           │        │
│    │   │                                                  │        │
│    │   │   BOOT_PROGRESS_ENABLE_SCREEN                    │        │
│    │   │       (ActivityManagerService.enableScreenAfterBoot())     │
│    │   └──────────────────────────────────────────────────────────┘        │
│    │                                                                       │
│    └─→ Launcher 启动，显示桌面                                              │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘
```

---

### EventLogTags 定义 (AMS logtags)

```logtags
# ActivityManagerService.java: EventLogTags.logtags
# ActivityManagerService.systemReady() starts:
3040 boot_progress_ams_ready (time|2|3)

# ActivityManagerService calls enableScreenAfterBoot():
3050 boot_progress_enable_screen (time|2|3)
```

---

### 各阶段耗时示例（典型值）

| 阶段 | 典型耗时 | 说明 |
|------|---------|------|
| `PMS_START` → `SYSTEM_SCAN_START` | ~200ms | 子组件创建 + Settings 读取 |
| `SYSTEM_SCAN_START` → `DATA_SCAN_START` | ~3000~8000ms | 系统分区包扫描（取决于包数量） |
| `DATA_SCAN_START` → `SCAN_END` | ~500~2000ms | 数据分区扫描 + OTA 处理 |
| `SCAN_END` → `PMS_READY` | ~500ms | 权限更新 + 写入 settings |
| **PMS 总计** | **~5~12s** | **开机耗时的主要瓶颈之一** |
| `PMS_READY` → `AMS_READY` | ~1000ms | 其他服务启动 |
| `AMS_READY` → `ENABLE_SCREEN` | ~500ms | 屏幕解锁 |

> **优化重点**: `SYSTEM_SCAN_START` → `DATA_SCAN_START` 是最耗时的阶段，可通过 **减少预装应用数量**、**使用 VAB/A/B 分区减少 OTA 扫描**、**DEX 预编译** 等方式优化。