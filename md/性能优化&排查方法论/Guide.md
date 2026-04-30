用户想深入了解 Android 性能优化与排查的方法论，包括 Perfetto、Systrace、Proto Log 和 dumpsys 这四大排查工具。这是一个非常实用的主题，我需要系统性地讲解每个工具的使用方法、适用场景、以及它们之间的配合使用方式。
# Android 性能优化 & 排查方法论完整指南

## 一、四大工具定位总览

```
┌─────────────────────────────────────────────────────────────────────┐
│                    排查工具选择决策树                                 │
│                                                                     │
│  遇到问题?                                                          │
│     │                                                               │
│     ├── 卡顿/丢帧 ──────→ Perfetto / Systrace (时间线分析)          │
│     │                                                                │
│     ├── ANR/崩溃 ────────→ dumpsys + logcat (状态快照 + 日志)       │
│     │                                                                │
│     ├── 系统内部行为 ────→ Proto Log (结构化日志，需集成)            │
│     │                                                                │
│     ├── 内存/GC 问题 ───→ Perfetto Memory + dumpsys meminfo         │
│     │                                                                │
│     └── 电量/唤醒 ──────→ Battery Historian + dumpsys battery       │
│                                                                     │
│  ★ 核心原则：先用轻量级工具(dumpsys)缩小范围，再用重量级工具(Perfetto)精确定位  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 二、Perfetto 深度使用指南

### 2.1 什么是 Perfetto

```
Systrace 的替代者/升级版 (Android 10+ 默认推荐)

┌──────────────┐         ┌──────────────┐         ┌──────────────┐
│   atrace     │         │   ftrace     │         │   heapprofd  │
│   (内核追踪)  │         │   (函数追踪)  │         │   (内存分配)  │
└──────┬───────┘         └──────┬───────┘         └──────┬───────┘
       │                        │                        │
       └────────────────────────┼────────────────────────┘
                                ▼
                     ┌─────────────────────┐
                     │    Perfetto (统一收集) │
                     │  .perfetto-trace 文件  │
                     └──────────┬──────────┘
                                ▼
                     ┌─────────────────────┐
                     │  Perfetto UI (可视化) │
                     │  https://ui.perfetto.dev│
                     └─────────────────────┘
```

### 2.2 抓取 Trace

```bash
# 方式1: 命令行抓取 (推荐，最灵活)
# 配置文件 perfetto_config.pbtx 或直接命令

adb shell "cat > /data/local/tmp/config.pbtx" << 'EOF'
buffers: {
    size_kb: 102400
    fill_policy: RING_BUFFER
}
data_sources: {
    config {
        name: "linux.ftrace"
        ftrace_config {
            buffer_size_kb: 4096
            drain_interval_ms: 200
            ftrace_events: "sched/sched_switch"
            ftrace_events: "sched/sched_waking"
            ftrace_events: "sched/sched_blocked_reason"
            ftrace_events: "power/cpu_frequency"
            ftrace_events: "power/cpu_idle"
            ftrace_events: "sched/sched_process_exit"
            ftrace_events: "sched/sched_process_free"
            function_name: "__schedule"
        }
    }
}
data_sources: {
    config {
        name: "linux.process_stats"
    }
}
duration_ms: 15000
EOF

# 开始录制
adb shell perfetto \
  --txt-config /data/local/tmp/config.pbtx \
  --out /data/local/tmp/trace.perfetto-trace

# ... 复现问题 ...

# Ctrl+C 停止或等 duration 到期
# 拉取到本地
adb pull /data/local/tmp/trace.perfetto-trace ./trace.perfetto-trace


# 方式2: Android Studio 内置 (简单场景)
# Profile → Capture system trace → 选择时间范围


# 方式3: Python 脚本自动化抓取 (适合CI/回归)
python -c "
import subprocess, time

# 启动 perfetto
subprocess.run(['adb', 'shell', 'perfetto',
    '--txt-config', '/data/local/tmp/config.pbtx',
    '--out', '/data/local/tmp/trace.perfetto-trace',
    '--background'])

time.sleep(15)  # 录制 15 秒

# 拉取 trace
subprocess.run(['adb', 'pull', '/data/local/tmp/trace.perfetto-trace'])
"
```

### 2.3 Perfetto UI 核心分析能力

#### 📊 能力一：冷启动耗时分析

```
操作步骤：
1. 打开 https://ui.perfetto.dev
2. 加载 trace.perfetto-trace
3. 左侧搜索栏输入 Activity name 或 package name
4. 在时间线上找到对应 Slice

查看的关键指标:
┌──────────────────────────────────────────────────────────┐
│  Main 线程                                               │
│  │ bindApplication ............│onCreate...│onResume..│  │
│  │←──── Application阶段 ──→│←─Activity阶段→│           │
│  │                                                          │
│  各段耗时一目了然                                          │
│  ★ 点击每个 Slice 可以看详细堆栈                            │
└──────────────────────────────────────────────────────────┘

关键时间点标记:
  T0 = intent received (进程启动信号)
  T1 = Application.onCreate() 开始
  T2 = MainActivity.onCreate() 开始  
  T3 = first frame drawn (首帧渲染完成)
  
  冷启动时间 = T3 - T0
  Application耗时 = T2 - T1
  Activity创建+渲染 = T3 - T2
```

#### 📊 能力二：一帧渲染全链路分析（帧耗时到屏幕显示）

```
Perfetto 中一帧的完整生命周期的 Track 视图:

┌─────────────────────────────────────────────────────────────┐
│  SurfaceFlinger                                              │
│  │ VSYNC-app │VSYNC-sf │ Display                           │
│  │    |      │    |    │    |                               │
│  │    ├──────┼────┼────┼────►  屏幕显示                      │
│  │                                                                  │
│  ───────────────── 一帧的完整路径 ─────────────────               │
│                                                                     │
│  [Main Thread]          [RenderThread]        [SurfaceFlinger]      │
│  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐      │
│  │ record        │      │ draw         │      │合成 Layer    │      │
│  │ layout&measure│ ───► │ OpenGL/Vulkan │ ───► │ HWComposer   │      │
│  │ dispatchDraw │      │ 同步到GPU     │      │ 送显         │      │
│  └──────────────┘      └──────────────┘      └──────────────┘      │
│       │                       │                    │                │
│       ▼                       ▼                    ▼                │
│    doFrame              flushCommands         onMessageReceived     │
│                                                                     
│  ★ 在 Perfetto 中可以看到这三个线程的时间对齐关系                      
│  ★ 如果 RenderThread 明显滞后于 Main → GPU 渲染瓶颈                   
│  ★ 如果 SF 合成滞后 → SurfaceFlinger 瓶颈                             
└─────────────────────────────────────────────────────────────┘

发现瓶颈的具体方法:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
现象1: Main线程有红色长Slice (>16ms)
原因: 主线程做了耗时计算/I/O/锁等待
解决: 异步化、懒加载、减少布局层级

现象2: Main很快但RenderThread慢
原因: GPU过载、绘制复杂度过高(大Bitmap、阴影)
解决: 减少overdraw、降分辨率、缓存

现象3: SF中出现 missed frame / late acquire
原因: App没赶上VSync信号
解决: 减少上一帧耗时、Pipeline优化

现象4: 出现 Jank / Frame dropped 标记
原因: 整体流水线某环节超时
解决: 按 Main→RT→SF 顺序逐层排查
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

#### 📊 能力三：线程状态分析

```
Perfetto 中的线程调度视图:

Thread State 颜色含义:
┌────────────────────────────────────┐
│ ████ Running  (绿色/蓝色)  正在执行  │
│ ░░░░ Runnable (黄色)      可运行但  │
│                         没抢到CPU   │
│ ▓▓▓▓ Sleeping  (灰色)      休眠中   │
│ █░█░ Uninterruptible(紫色) 磁盘IO等  │
│ ▒▒▒▒ Blocked   (橙色/红)   等待锁   │
└────────────────────────────────────┘

典型问题模式:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
模式A: 主线程长时间Blocked
  Main: RRRRRRRRRRBBBBBBBBBRRRR
                   ^^^^^^^^
                   等待某个锁释放!
  → 找到对应的锁持有者线程
  → 分析为何持有时间长

模式B: 主线程频繁Runnable但非Running
  Main: RrRrRrRrRrRrRrRRrrrRRr
       ^^ ^^ ^^ 
       可运行但被抢占!
  → CPU被其他高优线程抢走
  → 查看 sched_switch 事件找抢占者

模式C: 主线程Sleeping
  Main: RRRSSSSSSSRRR
         ^^^^^^^
         在sleep/wait!
  → 可能是 Binder 同步调用卡住
  → 查看对应 Binder Transaction
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

#### 📊 能力四：Binder 调用分析

```
在 Perfetto 中搜索 "binder" 相关 track:

┌──────────────────────────────────────────────────┐
│  Binder Transaction 视图                          │
│                                                    │
│  Caller Thread:                                   │
│  │ BC_TRANSACTION ───────────────────┐            │
│  │                                    │            │
│  │  waiting for reply (阻塞中!) │            │
│  │                                    │            │
│  │ BC_REPLY ◄────────────────────────┘            │
│                                                    │
│  Target (Server) Thread:                           │
│  │ BR_TRANSACTION                                  │
│  │ processing......                                │
│  │ BR_REPLY                                        │
│                                                    │
│  ★ 如果 "waiting for reply" 很长 → Server处理慢    │
│  ★ 如果是 oneway → 不应该看到 wait                 │
│  ★ 注意 Transaction 数据大小 (是否接近1MB限制)      │
└──────────────────────────────────────────────────┘

常见Binder性能问题:
┌────────────────────────────────────────┐
│ 1. TransactionTooLarge                 │
│    单次传输>1MB → 改用Ashmem/分片       │
│                                        │
│ 2. 同步调用链过长                       │
│    A→B→C→D 串行 → 考虑并行或回调       │
│                                        │
│ 3. oneway 洪水                         │
│    大量oneway塞满Binder线程池           │
│    → 加限流队列                        │
│                                        │
│ 4. 死锁                                │
│    A持锁1等锁2, B持锁2等锁1            │
│    → 统一加锁顺序                      │
└────────────────────────────────────────┘
```

### 2.4 Perfetto SQL 查询 (高级用法)

```sql
-- Perfetto 支持 SQL 直接查询 trace 数据!

-- 例1: 找出所有耗时超过50ms的主线程方法
SELECT 
    name, 
    dur / 1e6 AS duration_ms,
    ts
FROM slice 
WHERE 
    thread_name = 'main' 
    AND dur > 50000000  -- 50ms in nanoseconds
ORDER BY dur DESC
LIMIT 20;


-- 例2: 统计每秒的帧率
SELECT 
    CAST(ts / 1e9 AS INTEGER) AS second,
    COUNT(*) AS frame_count,
    SUM(CASE WHEN dur > 16666667 THEN 1 ELSE 0 END) AS jank_frames
FROM slice 
WHERE name LIKE '%Choreographer#doFrame%'
GROUP BY second
ORDER BY second;


-- 例3: 找出导致主线程阻塞的所有等待事件
SELECT 
    s.name AS blocked_slice,
    s.dur / 1e6 AS blocked_ms,
    (SELECT name FROM slice s2 WHERE s2.ts < s.ts AND s2.ts + s2.dur > s.ts LIMIT 1) AS blocking_cause
FROM slice s
WHERE 
    thread_name = 'main'
    AND dur > 16000000
ORDER BY s.dur DESC;
```

---

## 三、Systrace (传统工具，仍可用于低版本)

### 3.1 与 Perfetto 的区别

| 特性 | Systrace | Perfetto |
|------|----------|----------|
| **Android 版本** | 4.3+ (老版本主力) | 10+ (推荐) |
| **数据源** | 仅 atrace | atrace + ftrace + heapprofd + statsd |
| **分析界面** | Chrome://tracing | Web UI (ui.perfetto.dev) |
| **SQL 查询** | 不支持 | ✅ 支持 |
| **内存分析** | 弱 | ✅ heapprofd 强 |
| **自定义 Trace** | `Trace.beginSection()` | 同上 + Proto Log |

### 3.2 Systrace 使用（兼容方案）

```bash
# 抓取 Systrace (输出可被 Perfetto 打开)
python $ANDROID_SDK/platform-tools/systrace.py \
    --app=com.your.package \
    --cpu-freq \
    --cpu-idle \
    --cpu-load \
    --binder-driver \
    -b 32768 \
    -t 10 \
    -o my_trace.html

# 生成的 html 可以用 Chrome 打开
# 或者上传到 ui.perfetto.dev 转换分析
```

---

## 四、dumpsys 快速排查法

### 4.1 dumpsys 是什么

```
dumpsys = Dump System Service (系统服务状态快照)

特点：
✅ 无需 root (大部分)
✅ 实时获取，无需预先抓取
✅ 轻量级，不影响性能
❌ 只能看当前时刻的状态快照（非时序）
❌ 无法回溯历史

★ 最佳实践: 先用 dumpsys 缩小范围，再用 Perfetto 精确定位
```

### 4.2 常用 dumpsys 命令速查表

```bash
# ═══════════════════════════════════════════
# 【Input 系统】触摸/按键问题
# ═══════════════════════════════════════════
adb shell dumpsys input

# 重点看:
# Input Dispatcher State:
#   DispatchEnabled: true
#   DispatchFrozen: false        ← 如果true说明dispatcher冻结了!
#   FocusedWindow: xxx          ← 当前焦点窗口
#   FocusedApplication: xxx     ← 当前焦点应用
#   TouchStates: ...             ← 触摸状态
#   mInboundQueue: []            ← IQ 状态
#   OutboundQueue: []            ← OQ 状态
#   WaitQueue: []                ← WQ 状态 (有值=App未finish!)
#   PendingQueue: []             ← PQ 状态 (有值=窗口不就绪!)

# ═══════════════════════════════════════════
# 【Activity/WMS】窗口管理问题
# ═══════════════════════════════════════════
adb shell dumpsys activity activities

# 重点看:
#   * ResumedActivity: 当前前台Activity
#   * mResumedActivity: 同上
#   * Hist #0: Activity栈顶信息
#   * TaskRecord: 任务栈情况

adb shell dumpsys window windows

# 重点看:
#   Window #0: 窗口详细信息
#     mOwnerUid: xxx
#     mRequestedWidth/Height: 尺寸
#     mViewVisibility: 0 (VISIBLE)
#     mCurrentFocus: 当前焦点
#     mFocusedApp: 聚焦应用

# ═══════════════════════════════════════════
# 【内存】内存泄漏/OOM问题
# ═══════════════════════════════════════════
adb shell dumpsys meminfo com.your.package

# 输出示例:
# Applications Memory Usage (kB):
#   Native:     12345     ← native堆 (C++/Bitmap)
#   Dalvik:     23456     ← Java堆
#   Total PSS:  45678     ← 物理内存占用 (最重要!)

# Java Heap详情
adb shell dumpsys meminfo --oom com.your.package

# 查看所有进程按PSS排序
adb shell dumpsys meminfo --sort-by pss

# ═══════════════════════════════════════════
# 【CPU】CPU占用/调度问题
# ═══════════════════════════════════════════
adb shell dumpsys cpuinfo

# 显示各进程CPU使用率和运行时长

# ═══════════════════════════════════════════
# 【Battery】电量/唤醒锁问题
# ═══════════════════════════════════════════
adb shell dumpsys batterystats --charged com.your.package
# 或
adb shell dumpsys batteryinfo

# 查看WakeLock (防止后台休眠的锁)
adb shell dumpsys power | grep -A5 "mWakeLocks"

# ═══════════════════════════════════════════
# 【Package】安装/权限/组件问题
# ═══════════════════════════════════════════
adb shell dumpsys package com.your.package

# 包含: versionCode, permissions, 
#       Activities/Services/Receivers/Providers 列表
#       签名信息, 安装路径等

# ═══════════════════════════════════════════
# 【Alarm】定时任务问题
# ═══════════════════════════════════════════
adb shell dumpsys alarm

# 看是否有alarm频繁触发导致耗电/wakeup

# ═══════════════════════════════════════════
# 【Database】数据库性能问题
# ═══════════════════════════════════════════
adb shell dbshma "$(/adb shell pm path com.your.package | cut -d: -f2)"

# 或查看SQL统计
adb shell dumpsys dbinfo com.your.package

# ═══════════════════════════════════════════
# 【Network】网络连接状态
# ═══════════════════════════════════════════
adb shell dumpsys connectivity | grep -A10 "Active default network"

# ═══════════════════════════════════════════
# 【Graphics/SurfaceFlinger】渲染问题
# ═══════════════════════════════════════════
adb shell dumpsys SurfaceFlinger --list-layers  # 列出所有Layer
adb shell dumpsys SurfaceFlinger --displayId    # Display信息
```

### 4.3 dumpsys 自动化脚本模板

```bash
#!/bin/bash
# dumpsys_all.sh - 一键采集所有关键服务状态
# 用法: bash dumpsys_all.sh com.your.package

PKG=${1:-"com.your.package"}
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
DIR="./dump_${TIMESTAMP}"
mkdir -p $DIR

echo "=== 开始采集 dumpsys 信息 ==="
echo "目标包名: $PKG"
echo "输出目录: $DIR"

# 基础信息
adb shell getprop ro.build.version.release > $DIR/version.txt
adb shell date > $DIR/device_time.txt

# 核心服务
for service in input activity window power alarm battery graphics cpuinfo; do
    echo "正在采集: $service ..."
    adb shell dumpsys $service > $DIR/dumpsys_$service.txt 2>&1 &
done

# 应用特定
adb shell dumpsys meminfo $PKG > $DIR/meminfo.txt 2>&1 &
adb shell dumpsys package $PKG > $DIR/package.txt 2>&1 &

wait
echo "=== 采集完成! 文件在 $DIR ==="

# 可选: 自动打包
tar czf ${DIR}.tar.gz $DIR
```

---

## 五、Proto Log 深度实战

### 5.1 Proto Log vs 传统日志 对比

```
传统 ALOGD/SLOGI 方式:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 代码中:
void someFunction() {
    SLOGI("Window focus changed: window=%p, focused=%d", this, focused);
    // ↑ 字符串格式化在这里就执行了! 无论是否需要!
}

编译后的效果:
UserBuild (用户发布版本):
    ❌ 日志依然打印 (只是你看不到logcat)
    ❌ 字符串拼接开销仍在
    ❌ 参数序列化开销仍在
    ❌ I/O 写入 /dev/log (虽然会被丢弃)

DebugBuild (开发/内测版本):
    ✅ 正常打印
    ⚠️ 但大量日志会导致 logcat 冲击
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━


Proto Log 方式:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 代码中:
@ProtoLogGroup(ProtoLogGroup.TWM)
@ProtoLogMethod(
    message = "Window focus changed: window=%{window}, focused=%{boolean}",
    level = LogLevel.DEBUG
)
static void logFocusChanged(WindowState win, boolean focused) {
    // 编译器自动生成实现!
}

编译后的效果 (编译器 = protologtool):
UserBuild:
    void logFocusChanged(WindowState win, boolean focused) {
        // 方法体为空!!! 零开销!!!
        // 编译器直接删除了所有代码
    }

Debug/Eng Build:
    void logFocusChanged(WindowState win, boolean focused) {
        if (sEnabled) {  // 运行时可动态开关
            // 只记录参数ID + 二进制参数值
            // 不做字符串格式化!
            ProtoLog.log(MESSAGE_ID_WINDOW_FOCUS, win, focused);
        }
    }
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### 5.2 Proto Log 工作流程图

```
开发阶段:
┌─────────────┐     注解处理器      ┌──────────────────┐
│  *.java 源码  │ ──────────────→ │  编译期转换        │
│ @ProtoLogMethod│                  │  (protologtool)   │
│ message="..."  │                  │                   │
└─────────────┘                  └────────┬─────────┘
                                           │
                    ┌──────────────────────┼────────────────┐
                    ▼                      ▼                 │
           ┌────────────────┐    ┌─────────────────┐         │
           │ UserBuild 变体  │    │ EngBuild 变体    │         │
           │ 方法体 = 空     │    │ 记录二进制参数   │         │
           │ (真·零开销)    │    │ 到 RingBuffer   │         │
           └────────────────┘    └────────┬────────┘         │
                                           │
运行阶段 (Eng Build):                             │
                                           ▼
                              ┌─────────────────────────┐
                              │  ProtoLogImpl           │
                              │  RingBuffer (循环缓冲区) │
                              │  只存 ID + 参数值        │
                              └────────┬────────────────┘
                                       │
                                       ▼ (触发转储时)
                              ┌─────────────────────────┐
                              │  Bugreport / 手动触发    │
                              │  ↓                       │
                              │  读取 Buffer              │
                              │  + .proto 格式化定义      │
                              │  = 结构化文本输出          │
                              └────────┬────────────────┘
                                       │
                                       ▼
                              ┌─────────────────────────┐
                              │  Perfetto UI 导入        │
                              │  图形化展示 WMS 事件流    │
                              │  可过滤/搜索/关联分析     │
                              └─────────────────────────┘
```

### 5.3 Proto Log 在 WMS 中的实际使用示例

```java
// frameworks/base/services/core/java/com/android/server/wm/
//   WindowManagerLog.java (定义)

@ProtoLogClass
@ProtoLogGroup(ProtoLogGroup.TWM, wmLogTag)
public class WindowManagerLog {
    
    // 窗口焦点变化
    @ProtoLogMethod(message = "Add window %{window} to display #{int}",
                   LogLevel.INFO)
    public static void logWindowAdd(WindowState win, int displayId) {}
    
    // 窗口移除
    @ProtoLogMethod(message = "Remove window %{window}",
                   LogLevel.INFO)
    public static void logWindowRemove(WindowState win) {}
    
    // Input 分发
    @ProtoLogMethod(message = "Dispatching touch to %{window}, flags=%{int}",
                   LogLevel.DEBUG)
    public static void logInputDispatch(WindowState win, int flags) {}
    
    // Surface 变化
    @ProtoLogMethod(message = "Surface created for %{window}, size=%{dx}x%{dy}",
                   LogLevel.VERBOSE)
    public static void logSurfaceCreate(WindowState win, int w, int h) {}
    
    // 动画状态
    @ProtoLogMethod(message = "Animation start: %{window}, type=%{animation_type}",
                   LogLevel.DEBUG)
    public static void logAnimationStart(WindowState win, int type) {}
}


// 调用处 (WMS 代码中):
class WindowManagerService extends IWindowManager.Stub {
    void addWindow(Session session, IWindow client, ...) {
        // ...
        WindowManagerLog.logWindowAdd(winState, displayId);  // 一行搞定
        // ...
    }
}
```

### 5.4 查看 Proto Log

```bash
# 方法1: 从 bugreport 中提取
adb bugreport bugreport.zip
unzip bugreport.zip
# proto log 在: proto_log WindowManagerService*.proto-log

# 方法2: Perfetto 中查看 (如果配置了 data source)
# 直接加载 trace 后搜索 TWM 相关 event

# 方法3: 运行时手动触发转储
adb shell "setprop log.tag.window_manager DEBUG"  # 开启
adb shell "setprop log.tag.window_manager INFO"   # 关闭
```

---

## 六、综合排查实战案例

### 案例1: 用户反馈 App 点击按钮后 2 秒才有反应

```
Step 1: 用 dumpsys 快速初查
━━━━━━━━━━━━━━━━━━━━━━━━━━━━
$ adb shell dumpsys input | grep -E "(Queue|Focused|Frozen)"
  FocusedWindow: Window{xxx com.your.app/com.your.app.MainActivity}
  DispatchFrozen: false
  InboundQueue: []
  WaitQueue: []          
  PendingQueue: []

→ InputDispatcher 侧正常，事件已成功分发出去
→ 问题可能在 App 侧消费太慢

Step 2: 抓取 Perfetto Trace
━━━━━━━━━━━━━━━━━━━━━━━━━━━━
$ adb shell perfetto \
    --txt-config config_ftrace.pbtx \
    --out tap_slow.perfetto-trace

# 复现: 点击按钮，等待 2 秒响应
# Ctrl+C 停止

Step 3: Perfetto 分析
━━━━━━━━━━━━━━━━━━━━━━━━━━━━
打开 ui.perfetto.dev → 加载 trace

1. 定位点击时间点:
   搜索 "dispatchTouchEvent" 或 "ACTION_DOWN"
   
2. 查看 Main 线程在该时间段的活动:
   
   ┌──────────────────────────────────────────────┐
   │ Main Thread at click moment:                  │
   │                                               │
   │ dispatchTouchEvent()  ██████████  2ms  ✓ OK    │
   │ onClick()             ██████████████████ 18ms  │
   │ doInBackground()      ████████████████████ 25ms │
   │   └ bitmapDecode()    ████████████████████████ │
   │                       ████████████████████████  │
   │                       1800ms !!! ★ 找到了!      │
   │                                               │
   │ 结论: onClick 中在主线程解码了一个大 Bitmap!   │
   └──────────────────────────────────────────────┘

3. 解决方案:
   将 Bitmap 解码放到子线程，或者预加载缓存
```

### 案例2: 滑动列表时偶发掉帧

```
Step 1: dumpsys 查看基础状态
━━━━━━━━━━━━━━━━━━━━━━━━━━━━
$ adb shell dumpsys gfxinfo com.your.app framestats

# 输出每一帧的各阶段耗时:
# Flags, IntendedVsync, Vsync, OldestInputEvent, 
# NewestInputEvent, HandleInputStart, AnimationStart,
# PerformTraversalsStart, DrawStart, SyncQueued,
# SyncStart, IssueDrawCommandsStart, SwapBuffers,
# FrameCompleted

# 关注: PerformTraversalsStart → DrawStart (layout/measure 时间)

Step 2: Perfetto 深入分析
━━━━━━━━━━━━━━━━━━━━━━━━━━━━
配置 trace 时加入:
  ftrace_events: "sched/sched_switch"
  ftrace_events: "power/cpu_frequency"
  atrace_categories: "gfx" "view"

发现:
┌─────────────────────────────────────────────┐
│ 正常帧 (16ms):                               │
│ Choreographer#doFrame ████ 2ms               │
│ traversal ████ 8ms                            │
│ RecordView#draw ███ 4ms                       │
│ 总计 ≈ 14ms ✓                                │
│                                              │
│ 掉帧帧 (35ms):                               │
│ Choreographer#doFrame ████ 2ms               │
│ traversal ████████████████████████ 28ms !!    │
│   └ RecyclerView#LayoutManager ████ 22ms !!  │
│     └ onCreateViewHolder ███ 18ms !!        │
│ RecordView#draw ███ 5ms                       │
│ 总计 ≈ 35ms ✗ → 丢帧!                        │
│                                              │
│ 原因: 滑动时频繁 create ViewHolder!           │
│ 解决: 扩大 RecycledViewPool 缓存容量          │
└─────────────────────────────────────────────┘
```

### 案例3: App 后台被杀/ANR

```
Step 1: ANR trace 分析
━━━━━━━━━━━━━━━━━━━━━━━━━━━━
$ adb pull /data/anr/traces.txt .

# 搜索 main 线程堆栈:
"main" prio=10 tid=1 Blocked
  | group="main" sCount=1 dsCount=1 flags=1 obj=0xxxx self=0xxxxx
  | sysTid=12345 nice=0
  #00  pc 0xxxxx  /system/lib/libart.so
  #01  park (java.util.concurrent.locks.LockSupport)
  #02  lock (java.util.concurrent.locks.ReentrantLock)
  #03  execute (com.your.network.HttpClient)
  #04  onResponse (com.your.MainActivity$1.onResponse)
  
→ 主线程在做同步网络请求! 导致 ANR

Step 2: 结合 Perfetto 看 Binder 调用链
━━━━━━━━━━━━━━━━━━━━━━━━━━━━
如果 traces.txt 不够明确:

Perfetto 中:
1. 找到 ANR 时间点
2. 查看 Main 线程状态 → Blocked
3. 查看谁持有该锁 → Binder线程在做HTTP请求
4. 为什么Binder线程慢?
   → DNS解析? 连接超时? 服务端响应慢?

Step 3: dumpsys 补充验证
━━━━━━━━━━━━━━━━━━━━━━━━━━━━
$ adb shell dumpsys activity services com.your.app
# 确认是否有 Service 卡死

$ adb shell dumpsys power | grep -i wakelock
# 是否有 WakeLock 阻止系统休眠导致异常
```

---

## 七、排查方法总结流程图

```
                    ┌──────────────────────┐
                    │     收到性能/稳定性    │
                    │       问题反馈         │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │  第一步: 信息收集     │◄────────────────────┐
                    │  • 复现步骤            │                    │
                    │  • 机型/Android版本    │                    │
                    │  • 发生频率/时间点     │                    │
                    └──────────┬───────────┘                    │
                               │                                 │
                    ┌──────────▼───────────┐                    │
                    │  第二步: dumpsys 初查  │                    │
                    │  (轻量级,无侵入)       │                    │
                    │                      │                    │
                    │  • dumpsys input      │ ← 触摸/按键问题     │
                    │  • dumpsys activity   │ ← Activity问题      │
                    │  • dumpsys window     │ ← 窗口/焦点问题     │
                    │  • dumpsys meminfo    │ ← 内存/OOM         │
                    │  • anr/traces.txt     │ ← ANR/崩溃         │
                    └──────────┬───────────┘                    │
                               │                                 │
                    ┌──────────▼───────────┐                    │
                    │  能定位根因吗?         │                    │
                    └─────┬────────┬───────┘                    │
                         Yes│        │No                        │
                            ▼        ▼                          │
                    ┌──────────┐ ┌──────────────────┐           │
                    │ 直接修复  │ │ 第三步: Perfetto  │           │
                    │         │ │ (重量级,精确分析)  │           │
                    └──────────┘ └────────┬─────────┘           │
                                       │                      │
                              ┌────────▼─────────┐             │
                              │  配置Trace并复现  │             │
                              │  • atrace tags    │             │
                              │  • ftrace events  │             │
                              │  • 函数追踪(可选)  │             │
                              └────────┬─────────┘             │
                                       │                      │
                              ┌────────▼─────────┐             │
                              │  Perfetto UI 分析  │────────────┘
                              │  • 时间线对齐      │
                              │  • 线程状态分析     │
                              │  • SQL 深度查询     │
                              └────────┬─────────┘
                                       │
                              ┌────────▼─────────┐
                              │  定位瓶颈根因      │
                              │  • Main线程耗时?   │
                              │  • 锁竞争?        │
                              │  • Binder调用?     │
                              │  • GPU渲染?       │
                              │  • 内存/GC?       │
                              └────────┬─────────┘
                                       │
                              ┌────────▼─────────┐
                              │  制定修复方案      │
                              │  • 验证修复效果    │
                              │  • 回归测试        │
                              └──────────────────┘
```

---

## 八、各工具对比速查表

| 维度 | **dumpsys** | **logcat** | **Perfetto** | **Proto Log** | **Simpleperf** |
|------|------------|-----------|--------------|---------------|----------------|
| **类型** | 状态快照 | 流式日志 | 时间线追踪 | 结构化日志 | CPU性能剖析 |
| **开销** | 极低 | 低 | 中 | UserBuild为零 | 较高 |
| **是否需预埋** | 否 | 部分需 | 否 | 需注解集成 | 需编译符号表 |
| **时序分析** | ❌ | 有限 | **✅ 强项** | **✅ 强项** | ✅ |
| **历史回溯** | ❌ | 有限缓冲 | **✅ 完整** | RingBuffer | ✅ |
| **最佳场景** | 快速初查 | 崩溃/异常 | **卡顿/性能** | **Framework内部** | CPU热点函数 |

---

以上就是完整的 **Perfetto / Systrace / Proto Log / dumpsys** 排查方法体系。核心思路是 **先轻后重、层层递进**：

1. **dumpsys** → 快照，30秒内判断大概方向
2. **Perfetto** → 精确时序，找到毫秒级瓶颈
3. **Proto Log** → Framework 内部行为的结构化洞察（如果项目集成了的话）
4. **logcat/traces.txt** → 崩溃/ANR 的第一手资料

如果你有具体的问题场景想要模拟排查过程，可以告诉我，我们可以一起走一遍完整的排查流程！
