# Android 10 PackageManagerService（PMS）树状图核心分析

> 导出时间：2026-07-09
> 工作目录：`c:\D\android_project\cells-android10`（Android 10）
> 涉及文件：
> - `frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java`（约 1.16 MB）

---

# 一、类声明与定位

```
:414  public class PackageManagerService extends IPackageManager.Stub
      implements PackageSender
```

- **Binder 接口**：`IPackageManager.Stub` —— 对外（App、ADB、SystemServer 其它服务）暴露包查询、安装、卸载、权限、组件解析等能力
- **对外入口**：`IPackageManager.aidl`（应用侧 `Context.getPackageManager()` 最终落到此）
- **内部接口**：`PackageManagerInternalImpl`（`PackageManagerInternal`，:24218）—— 供 framework 内部（如 ATMS/WMS/AMS）调用
- **Native 接口**：`PackageManagerNative`（:24129，`IPackageManagerNative.Stub`）—— 供 shell / 测试
- **核心职责**：包扫描与解析、安装/卸载、权限授予与校验、组件（Activity/Service/Receiver/Provider）解析、多用户、Instant App、Apex、Dex 优化

## 两把锁（易混淆，务必分清）
| 锁 | 声明行 | 保护范围 |
|----|--------|----------|
| `mPackages` | :684（对象自身即锁） | 包数据结构（mPackages、mSettings 中包表）的读改写，要求快速、不可重入 `mInstallLock` |
| `mInstallLock` | :676 | 实际磁盘安装/卸载/重命名等重 I/O 操作；**不可与 `mPackages` 同时持有**（避免死锁），调用 Installer 前必须持此锁 |

---

# 二、PMS 树状图

```
PackageManagerService (pm)
├── 类声明 :414  extends IPackageManager.Stub, implements PackageSender
│
├── 内部类 / 子结构
│   ├── PackageHandler :1417                 // 主线程 Handler，驱动安装/卸载消息队列
│   ├── HandlerParams :14813 (abstract)      // 安装/删除参数基类
│   ├── InstallParams :15035                 // 安装参数（+ MutiPackageInstallParams :14965）
│   ├── InstallArgs :15586 (abstract)        // 安装落盘参数基类
│   │     ├── FileInstallArgs :15706         // 文件安装（普通 apk）
│   │     └── MoveInstallArgs :15912         // 移动安装
│   ├── PackageInstalledInfo :16055          // 安装结果
│   ├── PackageRemovedInfo :18733            // 卸载结果
│   ├── PackageFreezer :23221                // 冻结包（防并发变更）
│   ├── IntentVerifierProxy :1017            // Intent 过滤器校验代理
│   ├── PendingPackageBroadcasts :1253       // 待发送广播集合
│   ├── PostInstallData :1355                // 安装后数据
│   ├── PackageManagerInternalImpl :24218    // PackageManagerInternal 实现
│   └── PackageManagerNative :24129          // IPackageManagerNative.Stub
│
├── 核心字段
│   ├── 数据结构
│   │   ├── mPackages :684        ArrayMap<String,PackageParser.Package>  // 已装包（按包名）
│   │   ├── mSettings :708        Settings    // 持久化状态（packages.xml、权限、用户限制）
│   │   ├── mComponentResolver :968  ComponentResolver  // 组件快速索引
│   │   └── mPlatformPackage :939 // 系统（android）包自身
│   ├── 安装 / 卸载
│   │   ├── mInstaller :662       Installer   // 与 installd（native）通信
│   │   ├── mInstallerService :905 PackageInstallerService  // 现代安装入口（Session）
│   │   ├── mPendingVerification :900  SparseArray<PackageVerificationState>
│   │   └── mMoveCallbacks :917   MoveCallbacks
│   ├── 线程 / 锁
│   │   ├── mHandlerThread :637 / mHandler :639 (PackageHandler)
│   │   ├── mPackages (锁，见上)
│   │   └── mInstallLock :676
│   ├── 权限
│   │   ├── mPermissionManager :966  PermissionManagerServiceInternal
│   │   └── mDefaultPermissionPolicy :965  DefaultPermissionGrantPolicy // 预装默认权限
│   ├── 多用户 / 特殊包
│   │   ├── mUserManagerInternal :972  UserManagerInternal
│   │   ├── mInstantAppRegistry :732  InstantAppRegistry
│   │   └── mProtectedPackages (ProtectedPackages)
│   ├── Dex / Apex / 其它
│   │   ├── mPackageDexOptimizer :909 / mDexManager :912 / mViewCompiler :914
│   │   ├── mApexManager :757        ApexManager
│   │   ├── mIntentFilterVerifier :945  // Intent 过滤器校验
│   │   └── mPendingBroadcasts :1316
│
└── 关键方法分区
    ├── P1 启动 / 生命周期
    │   ├── main(...) :2306               // SystemServer 调用，构造并返回单例
    │   ├── PackageManagerService(...) :2403   // 构造函数（扫描各目录）
    │   ├── scanDirTracedLI :9027         // 扫描一个目录（system/vendor/...）
    │   ├── scanPackageTracedLI :9175     // 解析并注册单个包
    │   └── systemReady :21599           // 启动完成回调
    ├── P2 安装
    │   ├── installPackageLI (installPackageLI 处理，见 :17286 上下文)
    │   ├── installPackagesTracedLI :16333
    │   ├── installPackagesLI :16971
    │   └── FileInstallArgs.doExtract / doCopy / 提交
    ├── P3 卸载
    │   ├── deletePackageX :4952
    │   └── deletePackageLIF(...)          // 卸载核心（LI=持锁）
    ├── P4 权限
    │   ├── checkPermission :5625 → checkPermissionImpl :5637
    │   └── grantRuntimePermission :5746
    ├── P5 组件解析（Intent 路由）
    │   └── queryIntentActivitiesInternal :7211   // 解析 Activity（ATMS 启动 Activity 时调用）
    ├── P6 维护
    │   ├── updatePackagesIfNeeded :9639  // OTA/升级后修复
    │   ├── performBootDexOpt (经 updatePackagesIfNeeded/performDexOpt)
    │   ├── reconcileAppsData :22866      // 多用户 app 数据目录对齐
    │   ├── onNewUserCreated :23729       // 新用户初始化包数据
    │   └── freeStorage :4813             // 存储清理
    └── P7 查询
        └── getPackageInfo / getApplicationInfo / resolveActivity 等（对外 API）
```

---

# 三、核心职责详解

| 子系统 | 关键字段 / 类 | 负责内容 |
|--------|---------------|----------|
| 包表 | `mPackages`（:684） | 内存中全部已装包 `PackageParser.Package`（含组件、权限、签名） |
| 持久化 | `mSettings`（:708，`Settings` 类） | `packages.xml`/`packages.list` 读写，记录安装状态、权限授予、用户包禁用 |
| 组件解析 | `mComponentResolver`（:968） | 将 Manifest 组件建索引，支撑 `queryIntentActivities` 等快速查询 |
| 安装执行 | `mInstaller`（:662，installd 客户端） | 真正落盘：文件拷贝、dex 优化、目录创建、SELinux 标签 |
| 安装入口 | `mInstallerService`（:905） | 现代 `PackageInstaller` Session 流程（staging → 提交） |
| 权限 | `mPermissionManager`（:966） | 运行时权限授予/撤销/查询底层实现 |
| 默认权限 | `mDefaultPermissionPolicy`（:965） | 首次启动时给系统/预装应用授予默认权限 |
| 多用户 | `mUserManagerInternal`（:972） | 按用户启用/禁用包、隔离数据 |
| Dex 优化 | `mPackageDexOptimizer`/`mDexManager` | 编译 oat、profile 引导优化、后台 dexopt |
| Apex | `mApexManager`（:757） | 低层模块（apex）包管理 |
| 校验 | `mIntentFilterVerifier`/`PendingVerification` | 安装前 Intent 过滤器合法性校验 |

---

# 四、关键流程调用树

## 4.1 启动扫描（开机）
```
SystemServer → PackageManagerService.main :2306
  → new PackageManagerService(...) :2403
      ├── 初始化 mInstaller / mSettings / mPermissionManager / mInstallerService ...
      ├── scanDirTracedLI(systemDir) :9027
      │     └── 遍历文件 → scanPackageTracedLI :9175
      │           ├── PackageParser.parsePackage(...)   // 解析 Manifest
      │           ├── 校验签名 / 权限
      │           └── 注册到 mPackages(:684) + mComponentResolver(:968) + mSettings(:708)
      ├── scanDirTracedLI(vendorDir / productDir / dataDir ...)  // 多分区依次扫描
      └── mSettings.writeLPr()  // 落盘
```

## 4.2 安装（PackageInstaller Session 路径）
```
PackageInstallerSession.commit()
  → PMS.mInstallerService → PackageHandler 消息
    → installPackagesTracedLI :16333
        └── installPackagesLI :16971
            ├── scanPackageTracedLI(...)  // 解析待装包
            ├── 校验（签名/权限/冲突）
            ├── FileInstallArgs.doCopy/doExtract  // 落盘（持 mInstallLock）
            ├── mInstaller 调 installd 完成 dexopt
            └── 更新 mPackages / mSettings → 发 PACKAGE_ADDED 广播
```

## 4.3 卸载
```
PackageManager.deletePackage → PMS.deletePackageX :4952
  → PackageHandler → deletePackageLIF(...)   // 持 mPackages 锁
      ├── 移除 mPackages / mComponentResolver 记录
      ├── FileInstallArgs.doDeleteLI → mInstaller 删文件
      ├── 更新 mSettings（writeLPr）
      └── 发 PACKAGE_REMOVED 广播
```

## 4.4 组件解析（与 ATMS 协作）
```
ATMS 启动 Activity → PMS.resolveActivity / queryIntentActivitiesInternal :7211
  └── mComponentResolver 匹配 Intent 的 Activity/Action/Category
        → 返回 ResolveInfo 供 ATMS 决定目标
```

## 4.5 权限校验
```
App 请求权限 → Context.checkSelfPermission
  → PMS.checkPermission :5625 → checkPermissionImpl :5637
       └── 查 mSettings / mPermissionManager 中授予状态
运行时授予 → grantRuntimePermission :5746 → mPermissionManager 更新并持久化
```

---

# 五、与 AMS / ATMS / WMS 的关系

| 协作方 | PMS 提供 / 使用 | 场景 |
|--------|----------------|------|
| AMS | `mPackages`、权限、`PackageManagerInternal` | AMS 判断进程/包可见性、权限（如 INTERACT_ACROSS_USERS） |
| ATMS | `queryIntentActivitiesInternal` :7211 | 启动 Activity 前解析目标组件 |
| WMS | `mComponentResolver`、包信息 | 窗口/壁纸/输入法包解析 |
| installd | `mInstaller`（native socket） | 落盘与 dexopt 真正执行者 |
| `PackageManagerInternal` | `PackageManagerInternalImpl` :24218 | 上述服务通过此内部接口访问，绕过 Binder 提升性能 |

---

# 六、关键行号速查表

| 内容 | 行号 |
|------|------|
| 类声明 | :414 |
| `mInstallLock` | :676 |
| `mInstaller` | :662 |
| `mHandlerThread` / `mHandler`(PackageHandler) | :637 / :639 |
| `mPackages` | :684 |
| `mSettings` | :708 |
| `mInstantAppRegistry` | :732 |
| `mApexManager` | :757 |
| `mInstallerService`(PackageInstallerService) | :905 |
| `mPackageDexOptimizer` | :909 |
| `mDexManager` | :912 |
| `mMoveCallbacks` | :917 |
| `mPendingVerification` | :900 |
| `mPlatformPackage` | :939 |
| `mIntentFilterVerifier` | :945 |
| `mDefaultPermissionPolicy` | :965 |
| `mPermissionManager` | :966 |
| `mComponentResolver` | :968 |
| `mUserManagerInternal` | :972 |
| `mPendingBroadcasts` | :1316 |
| PackageHandler | :1417 |
| HandlerParams | :14813 |
| InstallParams | :15035 |
| InstallArgs | :15586 |
| FileInstallArgs | :15706 |
| MoveInstallArgs | :15912 |
| PackageInstalledInfo | :16055 |
| PackageRemovedInfo | :18733 |
| PackageFreezer | :23221 |
| PackageManagerNative | :24129 |
| PackageManagerInternalImpl | :24218 |
| main(...) | :2306 |
| 构造函数 | :2403 |
| scanDirTracedLI | :9027 |
| scanPackageTracedLI | :9175 |
| installPackagesTracedLI | :16333 |
| installPackagesLI | :16971 |
| deletePackageX | :4952 |
| checkPermission | :5625 |
| checkPermissionImpl | :5637 |
| grantRuntimePermission | :5746 |
| queryIntentActivitiesInternal | :7211 |
| updatePackagesIfNeeded | :9639 |
| systemReady | :21599 |
| reconcileAppsData | :22866 |
| onNewUserCreated | :23729 |
| freeStorage | :4813 |

---


The user wants a tree diagram and core analysis of PackageManagerService (PMS). Let me locate the file and extract structure.

Let me find PackageManagerService.java.
我定位 PMS 源码并提取其结构与核心字段/方法。
PMS is large (1.16 MB). I need to be strategic. Let me search for class declaration, key inner classes (Settings, PackageParser, etc.), and core fields.

Let me run parallel searches:
1. class declaration of PMS
2. Inner classes
3. Key fields (mPackages, mSettings, mInstaller, mUserManager, mPermissionManager, mDefaultPermissionPolicy, mHandler, mPackageDexOptimizer, etc.)

Let me search
PMS 文件较大（1.16 MB）。我并行提取类声明、内部类与核心字段/方法行号。
Good. Now I need more: the main lifecycle (main(), constructor, systemReady, scanDirTracedLI), and key method partitions. Let me search for:
- `static PackageManagerService main` or `main(`
- `public PackageManagerService(` constructor
- `systemReady`
- `scanDirTracedLI` / `scanPackageTracedLI` / `scanPackageDirtyLI`
- `installPackage` / `installPackageLI`
- `deletePackage` / `deletePackageLI`
- `checkPermission` / `grantPermission`
- `resolveActivity` / `queryIntentActivities`

Let me search for these
我再补取 PMS 生命周期入口、扫描/安装/卸载与权限查询等关键方法行号。
I have enough. Let me also note the internal impl `PackageManagerInternalImpl :24218` and `PackageManagerNative :24129`. And the key locks: mPackages (object), mInstallLock :676. Also `mSettings` (Settings class with mPackages - different).

Also note there's a `Settings` inner class (the file's `mSettings` is of type `Settings`). Actually Settings is in `com.android.server.pm.Settings` separate class. mSettings :708.

Now I have:
- Class: PackageManagerService :414 extends IPackageManager.Stub implements PackageSender
- main :2306, constructor :2403
- Locks: mPackages (object, guarded), mInstallLock :676
- mHandlerThread :637, mHandler (PackageHandler) :639
- mInstaller :662
- mPackages :684
- mSettings :708
- mInstantAppRegistry :732
- mApexManager :757
- mInstallerService (PackageInstallerService) :905
- mPackageDexOptimizer :909, mDexManager :912, mViewCompiler :914
- mMoveCallbacks :917
- mPendingVerification :900
- mPlatformPackage :939
- mIntentFilterVerifier :945
- mDefaultPermissionPolicy :965
- mPermissionManager :966
- mComponentResolver :968
- mUserManagerInternal :972
- mPendingBroadcasts :1316
- Inner classes: PackageHandler :1417, IntentVerifierProxy :1017, InstallParams :15035, FileInstallArgs :15706, MoveInstallArgs :15912, HandlerParams :14813, InstallArgs :15586, PackageInstalledInfo :16055, PackageRemovedInfo :18733, PackageFreezer :23221, PackageManagerNative :24129, PackageManagerInternalImpl :24218, MultiPackageInstallParams :14965
- Methods: scanDirTracedLI :9027, scanPackageTracedLI :9175, installPackageLI (around 17286 area - actually installPackageLI is referenced at 17286, but defined elsewhere; let me note installPackagesLI :16971, installPackagesTracedLI :16333, deletePackageLIF, deletePackageX :4952, checkPermission :5625, grantRuntimePermission :5746, queryIntentActivitiesInternal :7211, freeStorage :4813, systemReady (need line). Let me get systemReady line. It wasn't in the output clearly. Let me search. Actually I'll just search once more for systemReady and performBootDexOpt and a couple more.

Actually I have enough to write a solid tree. Let me grab systemReady line specifically to be accurate. And the Settings class reference. Let me do a quick search for systemReady.

Let me just do one more targeted search to confirm systemReady and the dexopt/scan boot flow, plus the SELinux/verify. Actually, I'll add a quick search.
Now I have enough. Let me write the analysis file and present the tree in chat.

systemReady :21599.

Let me write the file
数据齐备。下面给出 PMS 树状图与核心分析，并导出于 `c:\D\android_project\cells-android10\PMS_源码解析.md`。
已完成 PMS 树状图与核心分析，导出至 `c:\D\android_project\cells-android10\PMS_源码解析.md`。

## PMS 树状图（核心）
```
PackageManagerService :414  extends IPackageManager.Stub, implements PackageSender
├── 内部类: PackageHandler :1417 / HandlerParams :14813 / InstallParams :15035 /
│           FileInstallArgs :15706 / MoveInstallArgs :15912 / PackageFreezer :23221 /
│           PackageManagerInternalImpl :24218 / PackageManagerNative :24129
├── 核心字段
│   ├── 数据结构: mPackages :684 / mSettings :708 / mComponentResolver :968 / mPlatformPackage :939
│   ├── 安装: mInstaller :662 / mInstallerService :905 / mPendingVerification :900 / mMoveCallbacks :917
│   ├── 锁/线程: mPackages(锁) / mInstallLock :676 / mHandler :639
│   ├── 权限: mPermissionManager :966 / mDefaultPermissionPolicy :965
│   ├── 用户/特殊: mUserManagerInternal :972 / mInstantAppRegistry :732
│   └── Dex/Apex: mPackageDexOptimizer :909 / mApexManager :757
└── 方法分区: main :2306 / 构造函数 :2403 / scanDirTracedLI :9027 / scanPackageTracedLI :9175 /
             installPackagesLI :16971 / deletePackageX :4952 / checkPermission :5625 /
             queryIntentActivitiesInternal :7211 / systemReady :21599
```

## 核心要点
- **两把锁（易混淆）**：`mPackages`（:684，包数据结构锁，要求快、不可与 `mInstallLock` 同持）与 `mInstallLock`（:676，重 I/O 安装锁，调用 installd 前必须持有）。
- **数据流**：`mPackages`（内存包表）↔ `mSettings`（packages.xml 持久化）↔ `mComponentResolver`（组件索引）三方一致。
- **安装实际执行者**：`mInstaller`（:662）对接 native `installd` 完成落盘与 dexopt；现代入口是 `mInstallerService`（PackageInstaller Session）。
- **与兄弟服务关系**：ATMS 启动 Activity 前调 `queryIntentActivitiesInternal`（:7211）解析目标组件；AMS 通过 `PackageManagerInternal`（:24218）访问包/权限信息（绕过 Binder）。

如需将**安装流程**（Session→installPackagesLI→FileInstallArgs→installd）或**启动扫描流程**展开到第三、四层调用树，告诉我即可。


The user wants IMS tree diagram and core analysis. IMS could be:
- InputManagerService (most likely, given the pattern of system services: AMS, ATMS, WMS, PMS, now IMS)
- Or it could be "Instrumentation" but no.

In Android, IMS = InputManagerService. Given the sequence (AMS, ATMS, WMS, PMS), the natural next is InputManagerService. But also there's "IMMS" (InputMethodManagerService) for IME. Hmm. IMS most commonly = InputManagerService.

Let me locate InputManagerService.java
我定位 IMS（InputManagerService）源码并提取结构。
Now let me get the core fields and key methods. Let me search in parallel.
I have a good set of fields and methods. Let me also get line numbers for a few key methods: setWindowManagerCallbacks :331, start :339, systemReady :2110 area, setInputWindows, interceptKeyBeforeDispatch (via mWindowManagerCallbacks), injectInputEvent, freeze/thaw. Let me search a few more to be accurate, plus the WindowManagerCallbacks interface and InputMonitor/InputFilter. Also the `mPtr` native pointer :137.

Let me also check `setDisplayViewports`, `setFocusedWindow`, `onInputChannelDestroyed`. And the `LocalService` (InputManagerInternal) :2308. And `InputManagerHandler` :2116, `InputFilterHost` :2152, `InputMonitorHost` :2178.

I have enough for a solid tree. Let me gather a few more method lines quickly: setInputWindows, injectInputEvent, freezeInputDispatching, interceptKeyBeforeDispatch (in callbacks). Let me do one more search.
I have enough. Let me write the analysis. The WindowManagerCallbacks interface is at :2065 with methods notifyInputChannelBroken, notifyFocusChanged, etc. Let me note key callback methods: interceptKeyBeforeDispatch, interceptMotionBeforeDispatch, getKeyCodeState, etc. They are inside WindowManagerCallbacks. Let me note line 2065 for the interface.

Let me write the file. I'll provide the tree and core analysis.

Key fields:
- mPtr :137 (native pointer)
- mContext :139
- mHandler (InputManagerHandler) :140
- mWindowManagerCallbacks :147 (set via setWindowManagerCallbacks :331)
- mSystemReady :149
- mTabletModeChangedListeners :154
- mDataStore (PersistentDataStore) :160
- mInputDevicesLock :163, mInputDevices :165, mInputDevicesChangedListeners :166
- mInputFilterLock :184, mInputFilter :185, mInputFilterHost :186
- mFocusedWindow :188

Inner classes:
- InputManagerHandler :2116
- InputFilterHost :2152
- InputMonitorHost :2178
- InputDevicesChangedListenerRecord :2230
- TabletModeChangedListenerRecord :2258
- VibratorToken :2286
- LocalService (InputManagerInternal) :2308

Methods:
- nativeInit :321
- setWindowManagerCallbacks :331
- start :339
- systemReady :2110 (interface? Actually it's in WindowManagerCallbacks? No, the result showed line 2110 `public void systemReady();` — that's within WindowManagerCallbacks interface? Wait, the interface starts at 2065. Let me reconsider: line 2110 `public void systemReady();` — that's likely inside WindowManagerCallbacks. Hmm but systemReady is typically a PMS/WMS method. Actually in IMS, systemReady is in the WindowManagerCallbacks interface? That seems odd. Let me re-check: the search output showed:
```
2110:        public void systemReady();
```
This is within the WindowManagerCallbacks interface (starts 2065). Actually that might be the callbacks' systemReady. Interesting. But IMS itself also has a systemReady? Let me not overstate. Actually for IMS, the lifecycle: constructor, then `start()` :339. There's `systemReady` in the callbacks. Hmm. Actually let me just present both: IMS has `start()` :339; the WindowManagerCallbacks interface includes systemReady at 2110 (called by WMS after it's ready). That's fine.

Let me also note: `monitor()` (Watchdog.Monitor) :1754, `dump` :1725.

Methods:
- setWindowManagerCallbacks :331
- start :339
- reloadKeyboardLayouts :401
- setDisplayViewportsInternal :415
- registerInputChannel :547
- unregisterInputChannel :564
- injectInputEvent :616
- injectInputEventInternal :620
- setFocusedApplication :1491
- setInputDispatchMode :1530
- setSystemUiVisibility :1534
- setPointerSpeedUnchecked :1580
- vibrate :1654
- cancelVibrate :1682
- dump :1725
- monitor :1754
- notifyConfigurationChanged :1760 (native callback) / :2066 in interface
- WindowManagerCallbacks interface :2065
- LocalService :2308 (setDisplayViewports :2310, injectInputEvent :2315)

Now let me write the file. I'll include relationship with WMS (WMS sets callbacks, sets input windows) and how input flows: EventHub (native) → InputDispatcher → IMS → WMS.focused window → app. Actually the IMS is the Java wrapper; native side does the heavy lifting (InputReader, InputDispatcher). I'll mention nativeInit :321 establishes the native InputManager.

Let me write.
数据齐备。下面给出 IMS（InputManagerService）树状图与核心分析，导出至 `c:\D\android_project\cells-android10\IMS_源码解析.md`。
已完成 IMS（InputManagerService）树状图与核心分析，导出至 `c:\D\android_project\cells-android10\IMS_源码解析.md`。

## IMS 树状图（核心）
```
InputManagerService :121  extends IInputManager.Stub, implements Watchdog.Monitor
├── 内部类/接口: InputManagerHandler :2116 / InputFilterHost :2152 / InputMonitorHost :2178 /
│                LocalService :2308(InputManagerInternal) / interface WindowManagerCallbacks :2065
├── 核心字段
│   ├── 原生: mPtr :137（nativeInit :321 建立）
│   ├── 与 WMS: mWindowManagerCallbacks :147 / mFocusedWindow :188
│   ├── 设备: mInputDevices :165 / mInputDevicesChangedListeners :166
│   ├── 过滤: mInputFilter :185
│   └── 键盘: mDataStore :160
└── 方法分区: setWindowManagerCallbacks :331 / start :339 /
             registerInputChannel :547 / injectInputEvent :616 /
             setFocusedApplication :1491 / setInputDispatchMode :1530 /
             reloadKeyboardLayouts :401 / vibrate :1654 / monitor :1754
```

## 核心要点
- **native 桥梁**：`mPtr`（:137）通过 `nativeInit`（:321）持有 native `InputManager`（InputReader + InputDispatcher + EventHub），真正的事件读取/派发在 native 层；Java 侧主要做回调与配置。
- **与 WMS 的灵魂接口**：WMS 在启动期通过 `setWindowManagerCallbacks`（:331）注入自身，`WindowManagerCallbacks.interceptKeyBeforeDispatch` 是按键拦截入口——事件先到此处，再交由 PhoneWindowManager（PWS）决定电源/Home 语义，串起了 **IMS → WMS → PWS** 链路。
- **焦点/视口同步**：WMS 调 `setFocusedApplication`（:1491）/`registerInputChannel`（:547），IMS 据此切换派发目标；`setDisplayViewportsInternal`（:415）同步多屏视口给 native。
- **派发控制**：`setInputDispatchMode`（:1530）可冻结/恢复输入（转屏、锁屏场景）；`injectInputEvent`（:616）支持系统/测试注入。

至此已覆盖五大核心服务：**AMS / ATMS / WMS(+PWS) / PMS / IMS**。若需继续（如 `PowerManagerService`、`NotificationManagerService`，或把 IMS 的 `interceptKeyBeforeDispatch` 链路与 PWS 按键拦截合并展开到第四层），告诉我即可。

*本文档由会话分析自动整理导出；行号基于 cells-android10（Android 10）源码快照。*
