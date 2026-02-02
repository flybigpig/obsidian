# Activity 生命周期 - 事务项 - Handle/Perform 方法 精准对应表（附核心作用 + 调用链路）

你梳理的这份**生命周期（Lifecycle）- 事务项（Item）-Handle 方法 - Perform 方法**对应关系，是 Android 10+ 中 Activity 客户端事务执行的**核心映射规则**，完整覆盖了 Activity 全生命周期从「ATMS 下发事务」到「客户端实际执行回调」的方法链路，以下基于这份对应表做**精准补充 + 核心作用解析 + 调用链路梳理**，让每个环节的职责、调用关系一目了然。

## 一、完整精准对应表（基于你的梳理优化，补充核心说明）

|生命周期阶段|客户端事务项（ClientTransactionItem）|Handler 处理方法（ActivityThread.mH）|Perform 执行方法（ActivityThread）|关键补充（是否有独立 Perform / 核心职责）|
|---|---|---|---|---|
|ON_CREATE|LaunchActivityItem|handleLaunchActivity|performLaunchActivity|有独立 Perform，**Activity 实例创建 + onCreate 触发**核心方法|
|ON_START|StartActivityItem|handleStartActivity|无独立 Perform|直接在 handle 方法内完成 onStart 触发，无单独封装|
|ON_RESUME|ResumeActivityItem|handleResumeActivity|performResumeActivity|有独立 Perform，**onResume 触发 + 窗口显示**核心方法|
|ON_PAUSE|PauseActivityItem|handlePauseActivity|performPauseActivity|有独立 Perform，**onPause 触发 + 窗口暂停**核心方法|
|ON_STOP|StopActivityItem|handleStopActivity|performStopActivityInner|有内部 Perform（Inner 后缀），**onStop 触发 + 资源释放**核心方法|
|ON_DESTROY|DestroyActivityItem|handleDestroyActivity|performDestroyActivity|有独立 Perform，**onDestroy 触发 + 实例销毁 + 资源回收**核心方法|

## 二、核心概念先明确（3 层方法 / 类的职责边界，避免混淆）

在这份对应关系中，**生命周期阶段、事务项、Handle 方法、Perform 方法**是**自上而下的调用链路**，且每层职责单一、边界清晰，这是理解的核心：

### 1. 生命周期阶段（Lifecycle）

ATMS 统一管理的 Activity 生命周期状态（如 ON_CREATE/ON_RESUME），是**事务下发的依据**——ATMS 根据服务端 `ActivityRecord` 的状态，下发对应事务项到应用进程。

### 2. 客户端事务项（ClientTransactionItem）

ATMS 封装的**最小执行单元**，每个生命周期阶段对应一个专属事务项（如 ON_CREATE 对应 LaunchActivityItem），事务项中存储了执行该生命周期所需的所有参数（如 Intent、ActivityInfo），是跨进程指令的**载体**。

### 3. Handler 处理方法（handleXXXActivity）

ActivityThread 内部主线程 Handler（`mH`）的**消息处理方法**，负责**接收事务消息 + 前置初始化 + 调用 Perform 方法**，是主线程执行生命周期的**入口方法**，做通用的前置 / 后置处理。

### 4. Perform 执行方法（performXXXActivity/Inner）

ActivityThread 中**生命周期的实际执行方法**，负责**直接触发 Activity 的生命周期回调**（如 onCreate/onResume），是**业务落地的核心**，封装了该生命周期阶段的所有核心逻辑。

## 三、每对方法的核心作用 + 调用细节（按生命周期顺序）

结合 Android 源码核心逻辑，解析每一个生命周期对应的「事务项 - Handle-Perform」的**具体职责**和**调用细节**，重点说明「无独立 Perform」「Inner 后缀 Perform」的设计原因：

### 1. ON_CREATE → LaunchActivityItem → handleLaunchActivity → performLaunchActivity

- **LaunchActivityItem**：ATMS 下发的「启动 Activity 核心事务项」，封装 Activity 创建所需的 Intent、ActivityInfo、窗口参数等，是唯一能创建 Activity 实例的事务项；
- **handleLaunchActivity**：主线程 Handler 接收到启动事务后的**入口处理**，负责初始化 Activity 的窗口上下文、关联 WMS 窗口、检查进程状态，最终调用 performLaunchActivity；
- **performLaunchActivity**：**Activity 创建的核心方法**，通过**类加载器 + 反射**创建 Activity 实例、调用`Activity.attach()`初始化上下文（Application/Window/LayoutInflater）、最终触发`Activity.onCreate()`，是 ON_CREATE 生命周期的**实际落地方法**。

### 2. ON_START → StartActivityItem → handleStartActivity → 无独立 Perform

- **StartActivityItem**：ATMS 下发的「启动 Activity 后续事务项」，通常跟随 LaunchActivityItem 一起下发（也可单独下发，如 Activity 从后台切回前台）；
- **handleStartActivity**：主线程 Handler 的入口处理，**无独立 Perform 方法**，直接在该方法内完成核心逻辑 —— 调用`Activity.onStart()`，并更新 ActivityClientRecord 的状态为 STARTED；
- **设计原因**：ON_START 的逻辑简单（仅触发回调 + 状态更新），无复杂的资源初始化 / 窗口操作，无需单独封装 Perform 方法，简化调用链路。

### 3. ON_RESUME → ResumeActivityItem → handleResumeActivity → performResumeActivity

- **ResumeActivityItem**：ATMS 下发的「恢复 Activity 核心事务项」，是 Activity 显示到屏幕的**关键事务项**；
- **handleResumeActivity**：主线程 Handler 的入口处理，负责检查 Activity 的窗口状态、关联输入事件（InputDispatcher）、通知 WMS 将窗口置为前台，最终调用 performResumeActivity；
- **performResumeActivity**：**Activity 恢复的核心方法**，依次触发`Activity.onStart()`（若未执行）、`Activity.onResume()`，并更新 ActivityClientRecord 的状态为 RESUMED，是 Activity**进入前台可交互状态**的实际落地方法。

### 4. ON_PAUSE → PauseActivityItem → handlePauseActivity → performPauseActivity

- **PauseActivityItem**：ATMS 下发的「暂停 Activity 核心事务项」，用户退到后台、启动新 Activity 时优先下发（遵循「先暂停后启动」的生命周期规则）；
- **handlePauseActivity**：主线程 Handler 的入口处理，负责通知 WMS 暂停窗口绘制、移除输入事件关联、标记 Activity 为暂停状态，最终调用 performPauseActivity；
- **performPauseActivity**：**Activity 暂停的核心方法**，触发`Activity.onPause()`，并保存 Activity 的临时状态（如 EditText 输入内容），是 Activity**退出前台不可交互**的实际落地方法。

### 5. ON_STOP → StopActivityItem → handleStopActivity → performStopActivityInner

- **StopActivityItem**：ATMS 下发的「停止 Activity 核心事务项」，Activity 完全不可见时下发；
- **handleStopActivity**：主线程 Handler 的入口处理，负责检查 Activity 的可见性、释放窗口临时资源，最终调用 performStopActivityInner；
- **performStopActivityInner**：**Activity 停止的核心内部方法**（加 Inner 后缀表示「ActivityThread 内部专用」），触发`Activity.onStop()`，释放非必要资源（如网络连接、广播接收器），更新状态为 STOPPED；
- **设计原因**：ON_STOP 的逻辑有「内部状态校验」的强依赖，仅能在 ActivityThread 内部调用，不对外暴露，因此加 Inner 后缀标识**私有执行方法**。

### 6. ON_DESTROY → DestroyActivityItem → handleDestroyActivity → performDestroyActivity

- **DestroyActivityItem**：ATMS 下发的「销毁 Activity 核心事务项」，封装销毁原因、是否需要保存状态等参数；
- **handleDestroyActivity**：主线程 Handler 的入口处理，负责通知 WMS 销毁窗口、移除 ActivityClientRecord 的引用、触发资源回收，最终调用 performDestroyActivity；
- **performDestroyActivity**：**Activity 销毁的核心方法**，触发`Activity.onDestroy()`，释放所有资源（如 View 树、数据库连接、Handler），移除 ActivityThread 中对 Activity 实例的引用，完成 Activity**生命周期的最终收尾**。

## 四、通用调用链路（所有生命周期的统一执行流程）

无论是否有独立 Perform 方法、是否带 Inner 后缀，所有生命周期的执行都遵循 **「ATMS 下发 → 事务项执行 → Handler 转发 → Perform 落地」**的**统一链路 **，这是 Activity 客户端事务管理的核心规则，通用流程如下：

plaintext

```
1. ATMS 侧：根据ActivityRecord状态，构建对应ClientTransactionItem（如ON_CREATE→LaunchActivityItem），封装到ClientTransaction中跨进程下发；
2. 应用进程Binder线程：IApplicationThread.scheduleTransaction()接收事务，转发至ActivityThread主线程；
3. 主线程Handler（mH）：接收MESSAGE_EXECUTE_TRANSACTION消息，根据事务项类型，回调对应的handleXXXActivity方法（如LaunchActivityItem→handleLaunchActivity）；
4. handleXXXActivity方法：完成**前置初始化/状态校验/资源关联**（如窗口、WMS、输入事件）；
5. Perform方法（performXXXActivity/Inner）：**直接触发Activity的生命周期回调**（如onCreate/onResume），完成业务落地；
6. 后置处理：更新ActivityClientRecord状态，回传执行结果给ATMS，完成闭环。
```

## 五、关键特殊点总结（易混淆 / 易考点）

1. **ON_START 无独立 Perform**：因逻辑简单（仅触发 onStart + 状态更新），无复杂资源操作，直接在 handleStartActivity 内完成，简化调用链路；
2. **ON_STOP 的 Perform 带 Inner 后缀**：performStopActivityInner 是 ActivityThread**私有方法**，仅内部可调用，因包含强依赖的内部状态校验，不对外暴露；
3. **LaunchActivityItem 是核心**：是唯一能创建 Activity 实例的事务项，其他事务项均依赖其创建的 Activity 实例，无法单独执行；
4. **handle 方法是「通用入口」**：所有 handleXXXActivity 都做**通用的前置 / 后置处理**（如窗口、WMS、状态检查），不直接触发生命周期；
5. **Perform 方法是「业务落地」**：所有生命周期回调（onCreate/onResume 等）**仅在 Perform 方法中触发**，是客户端事务执行的最终目的。

## 六、面试 / 开发核心记忆点

这份对应关系是**理解 Activity 生命周期底层执行逻辑**的关键，也是 Android 系统层面试的高频考点，核心记忆原则：

- **一对一映射**：除 ON_START/ON_STOP 的特殊设计，其余生命周期均为「事务项→handle 方法→Perform 方法」一对一对应；
- **职责分层**：handle 方法管「通用处理 / 初始化」，Perform 方法管「实际触发生命周期」，边界清晰；
- **核心载体**：所有生命周期的执行，均以**ClientTransactionItem**为跨进程指令载体，由 ATMS 统一调度。

掌握这份对应表 + 调用链路，就能彻底打通「ATMS 调度 → 应用进程执行 → Activity 生命周期回调」的完整底层逻辑，不再局限于表面的生命周期回调顺序！