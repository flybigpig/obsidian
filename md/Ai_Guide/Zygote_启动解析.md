# Android 10 Zygote 启动 · 树状图核心根系分析

> 导出时间：2026-07-09
> 工作目录：`c:\D\android_project\cells-android10`（Android 10）
> 涉及文件：
> - `system/core/rootdir/init.zygote64.rc`（init 启动配置，另有 32/64_32 等变体）
> - `frameworks/base/cmds/app_process/app_main.cpp`（native 入口）
> - `frameworks/base/core/java/com/android/internal/os/ZygoteInit.java`
> - `frameworks/base/core/java/com/android/internal/os/Zygote.java`
> - `frameworks/base/core/java/com/android/internal/os/ZygoteServer.java`
> - `frameworks/base/core/java/com/android/internal/os/ZygoteConnection.java`

---

# 一、根链条（从内核到 Zygote 主循环）

```
Linux 内核启动
  └── init 进程 (pid=1) 解析 init.rc → 触发 init.zygote64.rc
        └── service zygote /system/bin/app_process64 --zygote --start-system-server
              └── app_main.cpp::main :176
                    └── AppRuntime(继承 AndroidRuntime) → runtime.start("com.android.internal.os.ZygoteInit", args, zygote=true) :339
                          └── AndroidRuntime::start → 创建 JVM / 注册 JNI → 调 ZygoteInit.main
                                └── ZygoteInit.main :822
                                      ├── preload(...) :135          // 预加载类/资源/库（根系核心）
                                      ├── 注册 Zygote Socket（ZygoteServer）
                                      ├── forkSystemServer :726 → Zygote.forkSystemServer :782
                                      │     └── nativeForkSystemServer → 产生 SystemServer 进程
                                      └── zygoteServer.runSelectLoop :920  // 进入监听循环，等待 fork App
```

---

# 二、各模块树状图

## 2.1 init.zygote64.rc（启动配置）
```
service zygote /system/bin/app_process64
    --zygote             // 标记为 zygote 模式
    --start-system-server// 启动后 fork SystemServer
    class main
    socket zygote stream 660 root system   // 创建 /dev/socket/zygote（App 请求 fork 的通道）
    onrestart write ...
    ...
```
> 多架构变体：`init.zygote32.rc` / `init.zygote64.rc` / `init.zygote32_64.rc` / `init.zygote64_32.rc`（主+辅架构，支持 64/32 混合）。

## 2.2 app_main.cpp（native 入口）
```
app_main.cpp
├── class AppRuntime : public AndroidRuntime :34
│   ├── setClassNameAndArgs(...) :43
│   ├── onStarted() :79        // 调 callMain → 进入 Java 主类
│   └── onExit(...) :102
└── int main(int argc, char* const argv[]) :176
    ├── 解析参数：--zygote → niceName=zygote, startSystemServer=true :271/:273
    │           └── 否则取 className（非 zygote 模式，直接跑某 Java 类）
    ├── runtime.setClassNameAndArgs(className, ...)
    ├── runtime.setArgv0(niceName, ...) :335   // 进程名设为 "zygote"
    └── runtime.start("com.android.internal.os.ZygoteInit", args, zygote) :339
          └── ⮕ 无 className 时走 ZygoteInit；指定 className 时走 RuntimeInit :341
```

## 2.3 ZygoteInit（Java 根系核心）
```
ZygoteInit
├── main(String argv[]) :822
│   ├── 解析参数（abiList、socketName、startSystemServer）
│   ├── preload(TimingsTraceLog) :135
│   │   ├── preloadClasses() :250        // 预加载 framework 基础类（数千个）
│   │   ├── preloadResources() :388      // 预加载系统资源（drawable/color/string）
│   │   ├── preloadSharedLibraries() :186// libandroid.so 等共享库
│   │   └── preloadTextResources() :210
│   ├── 注册 Zygote Socket（ZygoteServer 内部）
│   ├── if startSystemServer
│   │   └── forkSystemServer(abiList, socketName, zygoteServer) :726
│   │         ├── Zygote.forkSystemServer(...) :782
│   │         │     └── nativeForkSystemServer(...)   // native fork 出 SystemServer
│   │         ├── (子进程) closeServerSocket :799 → handleSystemServerProcess :801 → 调 SystemServer.main
│   │         └── (父进程/Zygote) 继续
│   └── zygoteServer.runSelectLoop(abiList) :920   // 监听循环
│         └── (fork App 见 2.6)
├── handleSystemServerProcess(ZygoteArguments) :480   // SystemServer 后续初始化
└── preload* 系列（见上）
```

## 2.4 Zygote（fork 原语封装）
```
Zygote
├── forkAndSpecialize(...) :234          // App 进程 fork + 特化
│   └── nativeForkAndSpecialize(...) :241 → JNI → fork()
│       └── (子进程) specializeAppProcess(...) :282   // 设 uid/gid/namespace/seccomp/art
├── forkSystemServer(...) :726 调用点使用 :782
│   └── nativeForkSystemServer(...) :340 → JNI → fork()
├── nativeForkAndSpecialize(...) :256   // JNI 声明
├── nativeForkSystemServer(...) :352    // JNI 声明
└── applyDebuggerSystemProperty(...) :766
```
> `forkAndSpecialize` 中的 "specialize" = 特化：fork 出与 Zygote 共享内存的子进程后，按参数剥离/定制（改 uid、gid、mount namespace、SELinux、capabilities、ART 运行时参数），使其成为独立的 App 进程。

## 2.5 ZygoteServer（Socket 监听与连接管理）
```
ZygoteServer
├── 核心字段
│   ├── mZygoteSocket :91           // LocalServerSocket（/dev/socket/zygote）
│   ├── mUsapPoolSocket :96         // USAP 池 socket（预 fork 的未特化进程池）
│   └── mUsapPoolEventFD :102
├── createZygoteSocket(...) :183     // 建立 mZygoteSocket
├── acceptCommandPeer(String abiList) :196  // 接受新连接 → 新建 ZygoteConnection
├── runSelectLoop(String abiList) :373      // ⭐ 主循环
│   └── poll 多路复用（zygote socket + 已连 peer）
│         └── 新连接 → acceptCommandPeer :447
│         └── 已有 peer 可读 → peer.runOnce()  // 处理一次 fork 请求
└── closeServerSocket() :214
```

## 2.6 ZygoteConnection（处理单次 fork 请求）
```
ZygoteConnection
├── runOnce()                        // 处理一个客户端请求（readArgumentList 后）
│   ├── Zygote.readArgumentList(mSocketReader) :135  // 读取 AMS 发来的参数
│   ├── (Zygote) forkAndSpecialize(...)             // 真正 fork
│   ├── handleParentProc(...) :617   // 父进程（Zygote）回收/通知
│   └── handleChildProc(...) :560    // 子进程（App）特化后执行目标
├── readArgumentList(...)            // 解析参数列表
├── setChildPgid(...) :713           // 设置进程组
└── closeSocket() :537
```

---

# 三、核心根系分析

## 3.1 为什么需要 Zygote（核心设计）
- **共享内存 / Copy-on-Write**：Zygote 在 `preload`（:135）阶段把 framework 类、资源、共享库一次性加载进内存；后续 `fork()` 出的 App 进程通过 **写时复制（COW）** 直接共享这份只读内存，避免每个 App 重复加载 dex/资源，极大缩短启动时间、降低内存占用。
- **单一可信模板**：所有 App 进程都是 Zygote 的副本，统一了 ART 运行时、SELinux 上下文、基础 classpath，安全性与一致性更强。

## 3.2 启动三阶段
1. **preload（根系）**：`preloadClasses`(:250) 预加载数千个 framework 类，`preloadResources`(:388) 预加载系统资源，`preloadSharedLibraries`(:186) 加载 native 共享库。此阶段耗时最长，是开机性能关键路径。
2. **forkSystemServer**：`forkSystemServer`(:726) → `nativeForkSystemServer` 产生 SystemServer（AMS/ATMS/WMS/PMS/IMS 等全部驻留于此进程）。
3. **runSelectLoop**：`runSelectLoop`(:920) 进入永久监听，AMS 每次启动 App 都通过 `/dev/socket/zygote` 发请求，Zygote 调用 `forkAndSpecialize`(:234) 孵化新进程。

## 3.3 通信机制（Socket + 参数列表）
- App 启动请求并非 Binder，而是 **Unix Domain Socket**：AMS 通过 `ZygoteProcess` 打开 `/dev/socket/zygote`，用 `ZygoteConnection` 发送参数列表（`readArgumentList` :135），Zygote 端 `runOnce` 读取并 `forkAndSpecialize`。
- 选择 Socket 而非 Binder 的原因：fork 时若持有 Binder 线程池状态会导致死锁/状态污染；Socket 在 fork 后子进程可安全关闭，干净利落。

## 3.4 特化（specialize）做了什么
`forkAndSpecialize`(:234) → 子进程 `specializeAppProcess`(:282) 内依次：
- 切换 uid/gid（从 root → 目标 App uid）
- 设置 supplemental gids、capabilities
- 建立 mount namespace、SELinux 上下文
- 设置调度策略、cgroup
- ART 运行时特化（JIT profile、heap 参数）
完成后子进程执行 App 的 `ActivityThread.main`，成为真正独立的用户进程。

## 3.5 USAP 池（性能优化，Android 10 已引入）
- `ZygoteServer` 维护 `mUsapPoolSocket`(:96) 与 `mUsapPoolEventFD`(:102)，预先 fork 一批 **未特化（Unspecialized App Process）** 进程待用；收到请求时直接从池中特化，省去 fork 延迟，降低启动抖动。

---

# 四、端到端时序（点击 App → 新进程）

```
用户点击 Launcher 图标
  → AMS.startActivity → 判定目标 App 进程未起
  → ZygoteProcess.openZygoteSocketIfNeeded()  // 连接 /dev/socket/zygote
  → 通过 ZygoteConnection 发送参数（包名/uid/abi...）
  → Zygote.runSelectLoop 中 peer.runOnce()
        → Zygote.forkAndSpecialize :234 → nativeForkAndSpecialize :241 (fork)
        → (子进程) ZygoteConnection.handleChildProc :560
              → specializeAppProcess :282 → ActivityThread.main → App 启动
        → (父进程) handleParentProc :617 回收，继续监听
```

---

# 五、关键行号速查表

| 内容 | 文件 | 行号 |
|------|------|------|
| init 配置 service zygote（含 socket） | init.zygote64.rc | （整文件） |
| AppRuntime 定义 | app_main.cpp | :34 |
| AppRuntime.onStarted | app_main.cpp | :79 |
| app_main.cpp::main | app_main.cpp | :176 |
| runtime.start(ZygoteInit) | app_main.cpp | :339 |
| ZygoteInit.main | ZygoteInit.java | :822 |
| preload（总入口） | ZygoteInit.java | :135 |
| preloadClasses | ZygoteInit.java | :250 |
| preloadResources | ZygoteInit.java | :388 |
| preloadSharedLibraries | ZygoteInit.java | :186 |
| preloadTextResources | ZygoteInit.java | :210 |
| forkSystemServer | ZygoteInit.java | :726 |
| Zygote.forkSystemServer 调用点 | ZygoteInit.java | :782 |
| closeServerSocket（system server 子进程） | ZygoteInit.java | :799 |
| handleSystemServerProcess | ZygoteInit.java | :480 |
| runSelectLoop 调用点 | ZygoteInit.java | :920 |
| forkAndSpecialize | Zygote.java | :234 |
| nativeForkAndSpecialize 调用点 | Zygote.java | :241 |
| nativeForkAndSpecialize 声明 | Zygote.java | :256 |
| specializeAppProcess | Zygote.java | :282 |
| nativeForkSystemServer 调用点 | Zygote.java | :340 |
| nativeForkSystemServer 声明 | Zygote.java | :352 |
| ZygoteServer.mZygoteSocket | ZygoteServer.java | :91 |
| ZygoteServer.mUsapPoolSocket | ZygoteServer.java | :96 |
| createZygoteSocket | ZygoteServer.java | :183 |
| acceptCommandPeer | ZygoteServer.java | :196 |
| runSelectLoop | ZygoteServer.java | :373 |
| closeServerSocket | ZygoteServer.java | :214 |
| ZygoteConnection.readArgumentList 调用 | ZygoteConnection.java | :135 |
| runOnce（处理请求） | ZygoteConnection.java | （见 2.6，core 流程） |
| handleParentProc | ZygoteConnection.java | :617 |
| setChildPgid | ZygoteConnection.java | :713 |
| handleChildProc | ZygoteConnection.java | :560 |

---

The user wants a tree diagram and core root analysis of Zygote startup. This is about the Android runtime/init process - Zygote is the process that forks app processes. Let me locate the Zygote source.

Key files for Zygote in Android 10:
- `frameworks/base/core/java/com/android/internal/os/ZygoteInit.java` - main entry
- `frameworks/base/core/java/com/android/internal/os/Zygote.java` - fork logic
- `frameworks/base/core/java/com/android/internal/os/ZygoteServer.java`
- `frameworks/base/core/java/com/android/internal/os/ZygoteConnection.java`
- `frameworks/base/cmds/app_process/app_main.cpp` - native entry (app_process)
- `frameworks/base/core/jni/com_android_internal_os_Zygote.cpp` - JNI
- `frameworks/base/core/java/com/android/internal/os/RuntimeInit.java`

The "核心根系" (core root system) suggests looking at the deep root: how init starts zygote, app_process, ZygoteInit.main, preload, fork loop.

Let me locate the key files. I should look at both the Java side and the init rc files. Let me search.

Let me find:
- ZygoteInit.java
- Zygote.java
- ZygoteServer.java
- ZygoteConnection.java
- app_main.cpp
- The .rc files (init.zygote*.rc) - these are in system/core/rootdir or device specific.

Let me search in parallel.
我定位 Zygote 相关源码（Java 启动、fork 逻辑、native 入口、init rc）并提取结构。
Good. Let me extract structure from the key files. Let me search:
- ZygoteInit.java: main(), preload(), forkSystemServer(), runSelectLoop(), startSystemServer()
- Zygote.java: forkAndSpecialize(), forkSystemServer(), nativeForkAndSpecialize(), nativeForkSystemServer()
- ZygoteServer.java: runSelectLoop(), acceptCommandPeer(), createZygoteSocket()
- ZygoteConnection.java: runOnce(), handleChildProc(), readArgumentList()
- app_main.cpp: main(), AppRuntime::onStarted(), start()

Let me run parallel searches
I have good info. Let me get a few more line numbers for key methods:
- ZygoteInit: startSystemServer / forkSystemServer, runSelectLoop, registerZygoteSocket, handleSystemServerProcess. The search didn't show these (they may have different names). Let me search for them specifically. Also main is at :822. preload at :135.
- Zygote.java: forkSystemServer (nativeForkSystemServer). Let me search.
- ZygoteServer: runSelectLoop line number. Let me search.
- ZygoteConnection: runOnce line. Let me search.

Let me do targeted searches.
Now I have a comprehensive picture. Let me also note the native fork in Zygote.java: forkSystemServer calls nativeForkSystemServer. Let me get line for nativeForkSystemServer and forkAndSpecialize call. I have forkAndSpecialize at :234 and specializeAppProcess at :282. The native methods: nativeForkAndSpecialize and nativeForkSystemServer. Let me get those lines quickly. Also, the runSelectLoop in ZygoteServer at :373, acceptCommandPeer at :196.

Let me get Zygote.java native method lines.
Now I have all the data I need. Let me write the analysis file covering Zygote startup tree and core root system.

Key chain:
1. init process (pid 0) reads init.zygote64.rc → starts `/system/bin/app_process64` (or app_process32) with `--zygote --start-system-server`
2. app_main.cpp main() → AppRuntime → runtime.start("com.android.internal.os.ZygoteInit", args, zygote=true)
3. AndroidRuntime::start → starts JVM, calls ZygoteInit.main
4. ZygoteInit.main (822):
    - preload (135) — classes, resources, shared libs, text resources
    - registerZygoteSocket (via ZygoteServer)
    - forkSystemServer (726) → Zygote.forkSystemServer (nativeForkSystemServer) → SystemServer process
    - runSelectLoop (920) → ZygoteServer.runSelectLoop (373) → accept connections, fork apps
5. When an app launches: AMS requests fork → socket command → ZygoteConnection.runOnce → Zygote.forkAndSpecialize (234) → nativeForkAndSpecialize (241) → child becomes app process → specializeAppProcess → ActivityThread.main

Let me also mention the USAP pool (Unspecialized App Process) - ZygoteServer has mUsapPoolSocket. That's a refinement. I'll mention briefly.

Let me write the file. I'll include:
- The full root chain (init → app_process → AndroidRuntime → ZygoteInit → preload → forkSystemServer → runSelectLoop)
- The fork-and-specialize for apps
- Core fields/structure
- Tree diagrams for ZygoteInit, Zygote, ZygoteServer, ZygoteConnection, app_main.cpp
- Core analysis (preload, copy-on-write, socket IPC, specialization)
- Key line numbers

Let me write it.
数据齐备。下面给出 Zygote 启动的完整树状图与核心根系分析，导出至 `c:\D\android_project\cells-android10\Zygote_启动解析.md`。
已完成 Zygote 启动的树状图与核心根系分析，导出至 `c:\D\android_project\cells-android10\Zygote_启动解析.md`。

## 根链条（从内核到主循环）
```
init(pid=1) 解析 init.zygote64.rc
  → /system/bin/app_process64 --zygote --start-system-server
  → app_main.cpp::main :176 → AppRuntime → runtime.start("ZygoteInit") :339
  → ZygoteInit.main :822
        ├── preload :135（类/资源/共享库 — 根系核心）
        ├── forkSystemServer :726 → nativeForkSystemServer → SystemServer
        └── runSelectLoop :920（永久监听 /dev/socket/zygote）
```

## 各模块树（核心）
```
ZygoteInit :822
├── preload :135 → preloadClasses :250 / preloadResources :388 / preloadSharedLibraries :186
├── forkSystemServer :726 → Zygote.forkSystemServer :782 → nativeForkSystemServer
└── runSelectLoop :920

Zygote（fork 原语）
├── forkAndSpecialize :234 → nativeForkAndSpecialize :241 → 子进程 specializeAppProcess :282
└── nativeForkSystemServer :352

ZygoteServer :373 runSelectLoop
├── mZygoteSocket :91  / mUsapPoolSocket :96（USAP 池）
└── acceptCommandPeer :196 → ZygoteConnection.runOnce

ZygoteConnection
├── readArgumentList :135 → forkAndSpecialize → handleChildProc :560 / handleParentProc :617
```

## 核心根系要点
- **为什么有 Zygote**：`preload`（:135）把 framework 类/资源/库一次性载入内存，后续 `fork()` 出的 App 通过 **写时复制（COW）** 共享这份只读内存，避免重复加载 dex、大幅缩短启动、降低内存——这是 Android 进程模型的根基。
- **三阶段**：preload → forkSystemServer（孵化 SystemServer，AMS/ATMS/WMS/PMS/IMS 全驻留于此）→ runSelectLoop（永久监听 fork App）。
- **Socket 而非 Binder**：App 启动请求走 `/dev/socket/zygote`（`ZygoteConnection.readArgumentList` :135）。fork 时若持有 Binder 线程池状态会死锁/污染，Socket 在子进程可安全关闭。
- **特化（specialize）**：`forkAndSpecialize`(:234) 后子进程 `specializeAppProcess`(:282) 切换 uid/gid、设 namespace/SELinux/cgroup、ART 特化，再执行 `ActivityThread.main` 成为独立用户进程。
- **USAP 池**：`ZygoteServer` 维护 `mUsapPoolSocket`(:96) 预 fork 未特化进程，降低启动抖动（Android 10 已引入）。

至此已串联完整启动链：**init → Zygote → SystemServer（各 XMS）→ App 进程**。如需把 `nativeForkAndSpecialize` 的 JNI/native 层（com_android_internal_os_Zygote.cpp 中的 fork+特化系统调用序列）继续下钻，或把 SystemServer 启动各 XMS 的顺序展开，告诉我即可。

*本文档由会话分析自动整理导出；行号基于 cells-android10（Android 10）源码快照。*
