> 前言：生活不止眼前的苟且，还有诗和远方。

![深入研究Android启动速度优化（下）.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/876121e357044057bb1c34a4c1fda04e~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1100&h=689&s=585678&e=png&b=043cc0)

在上一篇文章[《深入研究Android启动速度优化（上）- 看完这些启动优化已经完成80%了》](https://juejin.cn/post/7354233812593246248 "https://juejin.cn/post/7354233812593246248")中，梳理了应用启动的整个过程和问题，启动优化阶段与指标是什么，启动耗时方法的数据统计八种工具与分析，以及一些常见的启动时间问题。可以说是完成了**启动优化工作最难的一部分**。

## 启动优化大纲

![Android启动速度优化-大纲.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/50daf04af7464be5834604800e3c68df~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1812&h=5958&s=678609&e=png&b=ffffff) 下面我们来进行启动优化具体方案实战，还有什么方法可以做进一步优化？怎么证明你的应用启动速度秒杀竞品 App？如何在线上衡量启动优化的效果？又是怎么保障和监控启动速度？

## 十、Application阶段优化方案

### 1\. 视觉优化

冷启动过程中会创建一个空白的 Window，等到应用创建第一个 Activity 后才将该 Window 替换。如果你的 Application 或 Activity 启动的过程太慢，导致系统的 BackgroundWindow 没有及时被替换，就会出现启动时白屏或黑屏的情况。

这样会造成用户感觉到当点击 App 图标时会有**卡顿**现象。为了解决这一问题，Google 的做法是在 App 创建的过程中，先展示一个空白页面，让用户体会到点击图标之后立马就有响应。

1.  在 drawable 目录下创建背景文件：

```
<layer-list xmlns:android="http://schemas.android.com/apk/res/android" android:opacity="opaque">
    <item android:drawable="@android:color/white"/>
    <item>
        <bitmap
            android:src="@drawable/bg_welcome"
            android:gravity="center"/>
    </item>
</layer-list>
```

2.  定义一个启动页主题，将 `windowBackground` 设置为上面定义的背景图：

```
<style name="SplashAppTheme" parent="Theme.MaterialComponents.DayNight.NoActionBar">
    <item name="windowActionBar">false</item>
    <item name="android:windowNoTitle">true</item>
    <item name="android:windowFullscreen">true</item>
    <!-- 启动背景 -->
    <item name="android:windowBackground">@drawable/splash_bg</item>
</style>
```

3.  将 `SplashAppTheme` 配置给第一个启动的 Activity：

```
<activity
        android:name="com.sum.main.ui.SplashActivity"
        android:launchMode="singleTask"
        android:windowSoftInputMode="adjustPan"
        android:theme="@style/SplashAppTheme"
        android:exported="true">
    <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="android.intent.category.LAUNCHER" />
    </intent-filter>
</activity>
```

4.  在 Activity 的 `onCreate()` 中把 Theme 换回应用的 App theme：`R.style.AppTheme`：

```
override fun onCreate(savedInstanceState: Bundle?) {
    setTheme(R.style.AppTheme)
    super.onCreate(savedInstanceState)
}
```

这种方案都只是提高了用户体验，并没有真正的加快启动速度。

### 2\. 异步优化

在应用启动的时候，通常会有很多任务需要做，为了提高启动速度，尽可能将这些任务并发进行。**核心思想：子线程分担主线程任务，(子线程和主线程同时执行)并行减少时间。**

![主线程和多线程Task图.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ea61b6c6beb34752ba7e1d9acb9545f2~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=603&h=226&s=13146&e=png&b=ffffff)

现在手机一般都是8核的设备， 根据不同的手机厂商分配给 APP 有可能是4核也有可能是8核，但是如果只用一个线程则只占一个核，其他3个核是处于一个浪费的状态。**创建异步任务能充分利用资源**。

Android 中异步方式有很多种：

-   **Thread**：最简单、常见的异步方式，不易复用，频繁创建及销毁开销大。一般不推荐直接使用。
-   **HandlerThread**：封装好的异步消息处理类，内部继承自 Thread 并封装了 Handler。保证多线程并发需要更新 UI 线程时的线程安全。
-   **IntentService**：内部实现 `Service + HandlerThread`，实现多线程，不占用主线程，优先级较高，不易被系统Kill，用于处理异步请求。
-   **AsyncTask**：内部封装了 Handler 和线程池，一个处理异步任务的类。它存在不同版本任务串行并行，容易内存泄露等问题。
-   **RxJava**：一个基于事件流，实现异步的强大操作库。链式调用，实现优雅，逻辑简洁。
-   **线程池**：提供线程复用，避免频繁的创建和销毁带来的性能消耗，能有效控制最大并发数，防止大量线程抢占资源导致系统阻塞。
-   **[Kotlin 协程](https://juejin.cn/post/6987724340775108622 "https://juejin.cn/post/6987724340775108622")**：协程就像轻量级的线程，提供了一种避免阻塞线程并用更简单、更可控的操作替代线程阻塞的方法。

我们可以根据实际场景选择适合的方式，按照先后顺序，越后面推荐优先级越高。一般情况禁止使用**使用 new Thread 方式**创建线程，实际中需要提供**基础线程池**供各个业务线使用，避免各个业务线各自维护一套线程池，导致线程数过多。

因为任何时候，**只有一个线程占用CPU，处于运行状态**，多线程并发，是轮流去获取 CPU 使用权。JVM 负责线程调度，按照特定机制分配 CPU 使用权。

这里我使用 AppExecutors 线程池异步执行 Application 中的初始化任务：

```
class SumApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        // 线程池执行异步任务
        AppExecutors.cpuIO.execute {
            initSumHelper()
        }
        AppExecutors.cpuIO.execute {
            initDeviceId()
        }
        AppExecutors.cpuIO.execute {
            initMmkv()
        }
        AppExecutors.cpuIO.execute {
            initAppManager()
        }
        AppExecutors.cpuIO.execute {
            initRefreshLayout()
        }
        AppExecutors.cpuIO.execute {
            initArouter()
        }
    }
}
```

但是这种异步方案会存在几种问题：

1.  **代码不够优雅**：如果有多个并行异步任务时会有多个 `execute{}` 重复代码块，维护成本比较高。
2.  **无法实现依赖关系**：初始化任务之间存在依赖关系，比如极光推送需要设备 ID，那么 `initDeviceId()` 需要在初始化极光推送 SDK 之前执行完成。
3.  **无法在指定函数中完成**：任务 `initArouter()` 需要在 `onCreate()` 中执行完成（依赖关系），不好控制。

### 3\. 异步优化 - 启动器

初始化任务之间可能存在前后依赖关系，所以需要保证它们执行顺序的正确性。 **启动器的核心思想是在线程池任务基础上，充分利用多核 CPU ，自动梳理任务顺序**。核心流程如下：

1.  对初始化任务代码 Task 化，启动任务抽象为 Task 类；
2.  根据所有任务依赖关系排序生成有向无环图，也就是对所有任务进行有向无环图的拓扑排序算法排序，将**并行效率最大化**；
3.  多线程按照排序之后的优先级依次执行任务。

比如我们现在有 A、B、C 三个任务，任务 B 依赖于任务 A(A 执行完成后才能执行 B)，这时候通过有向无环图排序为任务 A、C 同时执行，任务 B 在任务 A 完成后开始执行。 ![任务执行流程图.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ce0e96cffc7f4b178e93eefa2d6b6820~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1400&h=693&s=64072&e=png&b=fefefe)

启动器的主要流程是**异步并发 Task 和主线程 Task**。 `Head Task` 与 `Tail Task` 并不包含在启动器的主题流程中，它仅仅是用于**处理所有任务启动前/启动后的一些通用任务**，例如我们可以在 `Head Task` 中做一些获取通用信息的操作，在 `Tail Task` 可以做一些 log 输出、数据上报等操作。`Idle Task` 表示程序空闲时才执行的任务。

**ITask**：Task 任务接口，定义任务相关功能接口。包括任务优先级，指定线程池，任务依赖关系等，**`run()` 是初始化任务正在执行的地方**。

```
interface ITask {
    // 优先级的范围，可根据Task重要程度及工作量指定；之后根据实际情况决定是否有必要放更大
    fun priority(): Int

    // 任务真正执行的地方
    fun run()

    // Task执行所在的线程池，可指定，一般默认
    fun runOn(): Executor?

    // 依赖关系 需要依赖执行的任务队列
    fun dependsOn(): List<Class<out Task?>?>?

    // 异步线程执行的Task是否需要在被调用await的时候等待，默认不需要
    fun needWait(): Boolean

    // 是否在主线程执行
    fun runOnMainThread(): Boolean
    ······
}
```

**Task：任务抽象类，任务优先级，指定线程池，任务依赖关系等默认实现，所有初始化任务都需要继承这个类，并且复写相关方法实现具体逻辑**。

```
abstract class Task : ITask {
    // 当前Task依赖的Task数量（需要等待被依赖的Task执行完毕才能执行自己），默认没有依赖
    private val mDepends = CountDownLatch(
        dependsOn()?.size ?: 0
    )

    // 当前Task等待，让依赖的Task先执行
    fun waitToSatisfy() {
        mDepends.await()
    }

    // Task执行在哪个线程池，默认在IO的线程池；
    override fun runOn(): ExecutorService? {
        return DispatcherExecutor.iOExecutor
    }

    // 当前Task依赖的Task集合（需要等待被依赖的Task执行完毕才能执行自己），默认没有依赖
    override fun dependsOn(): List<Class<out Task?>?>? {
        return null
    }

    // 运行在主线程
    override fun runOnMainThread(): Boolean {
        return false
    }
    ······
}
```

启动任务抽象为 Task，进行二次包装，代码 Task 化，根据所有任务依赖关系排序生成有向无环图，异步队列按照排序之后的优先级依次执行。

![有向无环图.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/5d7da952f1d34302ad77f417dc3a3d72~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=564&h=825&s=46156&e=png&b=ffffff)

从 start 节点开始到 end 节点结束，每个 Task 都是一个初始化任务，箭头代表着任务之间的执行顺序以及依赖关系。任务链有开始和结束节点，树形结构图里面，节点可以存在依赖关系，但是不能存在环形依赖，称为有向无环树形结构。

**启动器类结构说明：**

| 类/接口 | 说明 |
| --- | --- |
| **ITask** | 面向接口，定义Task相关功能接口 |
| **Task** | 启动被初始化的任务，可以是同步执行也可以是异步执行，可以指定依赖关系，执行线程池，是否需要等待 |
| **MainTask** | 在主线程执行的任务 |
| **run()** | 任务初始化正在执行的地方 |
| **DispatcherExecutor** | 线程管理类 |
| **DirectionGraph** | 有向无环图的拓扑排序算法，实现拓扑排序 |
| **TaskDispatcher** | 任务调度管理类，管理一组任务，根据任务依赖关系，按照任务顺序执行 |

**使用步骤：**

1.  创建任务：任务继承 Task，实现 `run()` 方法：

```
class InitMmkvTask() : Task() {
    // 异步线程执行的Task在被调用await的时候等待
    override fun needWait(): Boolean {
        return true
    }

    // 依赖某些任务，在InitSumHelperTask任务完成后才能执行当前任务
    override fun dependsOn(): MutableList<Class<out Task>> {
        val tasks = mutableListOf<Class<out Task?>>()
        tasks.add(InitSumHelperTask::class.java)
        return tasks
    }

    // 执行所在的线程池
    override fun runOn(): ExecutorService? {
        return DispatcherExecutor.iOExecutor
    }

    // 执行任务，任务真正的执行逻辑
    override fun run() {
        // 初始化 MMKV
        val rootDir: String = MMKV.initialize(SumAppHelper.getApplication())
        MMKV.setLogLevel(
            if (BuildConfig.DEBUG) {
                MMKVLogLevel.LevelDebug
            } else {
                MMKVLogLevel.LevelError
            }
        )
    }
}
```

创建一个 InitMmkvTask，在 `run()` 里面执行 Mmkv 的初始化的具体逻辑，`dependsOn()` 中添加了任务 InitSumHelperTask，InitMmkvTask 需要在这个任务执行完成后方可执行。`runOn()` 方法指定了这个任务执行在 `DispatcherExecutor.iOExecutor` 线程池中。

2.  创建分发器，添加任务并启动：

```
    override fun onCreate() {
        super.onCreate()
        //1.启动器：TaskDispatcher初始化
        TaskDispatcher.init(this)
        //2.创建dispatcher实例
        val dispatcher: TaskDispatcher = TaskDispatcher.createInstance()

        //3.添加任务并且启动任务
        dispatcher.addTask(InitSumHelperTask(this))
                .addTask(InitMmkvTask())
                .addTask(InitAppManagerTask())
                .addTask(InitRefreshLayoutTask())
                .addTask(InitArouterTask())
                .start()

        //4.等待，needWait = true的任务执行完才可以往下执行
        dispatcher.await()
    }
```

将 TaskDispatcher 初始化，通过 `addTask()` 将相关任务加入分发队列中，调用 `start()` 开启任务队列执行， `await()` 方法表示任务队列中 `needWait = true` 的任务执行完后 `onCreate()` 才可以往下执行。

异步任务启动器已经分析完毕，具体可参考：[github.com/suming77/Su…](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2Fsuming77%2FSumTea_Android "https://github.com/suming77/SumTea_Android")

**启动器注意事项：**

1.  主线程任务 MainTask 不需要添加 `needWait() = true` ;
2.  如果 TaskDispatcher 中的任务没有需要 `needWait()` 的，则不需要调用`dispatcher.await()`;
3.  CPU 密集型的任务一定要切换到 `DispatcherExecutor.getCPUExecutor()`。

### 4\. 延迟优化

我们应用启动中可能存在部分优先级不高的初始化任务，一般考虑把这些任务进行延迟初始化。

常规的方案就是在 Application 中或者在[首页列表首帧显示](https://juejin.cn/post/7354233812593246248 "https://juejin.cn/post/7354233812593246248")进行延迟几秒后再进行初始化。通过 `Handler().postDelay()` 的方式：

```
// 首页首帧时间回调
homeBannerAdapter.onFirstFrameTimeCall = {
    AppExecutors.mainThread.executeDelay(Runnable {
        // 任务延迟3s执行
        initToastTask()
    }, 3000)
}
```

-   这种方式是简单暴力，但是时机不容易控制，`postDelayed()` 指定的延迟时间不好估计。
-   `onFirstFrameTimeCall` 执行在主线程，界面显示后，该任务如果执行时间2s，主线程被卡2s导致界面 UI 卡顿，那么用户就无法交互，体验较差。

**IDLEHandler**

**另一种方案：延迟启动器利用了 IdleHandler 主线程空闲时才执行任务的特性实现对延迟任务分批初始化（消息队列空闲的时候执行）**。

```
class DelayInitDispatcher {
    // 任务集合
    private val mDelayTasks: Queue<Task> = LinkedList()

    private val mIdleHandler = IdleHandler {
        // 分批执行的好处在于每一个task占用主线程的时间相对来说很短暂，并且此时CPU是空闲的，这些能更有效地避免UI卡顿
        if (mDelayTasks.size > 0) {
            val task = mDelayTasks.poll()
            DispatchRunnable(task).run()
        }
        // 系统空闲时会回调queueIdle()，返回值表示是否移除这个监听。
        !mDelayTasks.isEmpty()
    }

    // 添加任务
    fun addTask(task: Task): DelayInitDispatcher {
        mDelayTasks.add(task)
        return this
    }

    // 开启延迟启动器
    fun start() {
        Looper.myQueue().addIdleHandler(mIdleHandler)
    }
}
```

在 DelayInitDispatcher 中，mDelayTasks 任务队列通过 `addTask()` 将每个 task 存储起来，调用 `start()` 将 mIdleHandler 添加到主线程消息队列中。**当 CPU 空闲时，mIdleHandler 便会回调自身的 queueIdle 方法，这时候就会将 task 逐个取出并执行**。

```
// 首帧显示
homeBannerAdapter.onFirstFrameTimeCall = {
    //延迟执行启动器
    DelayInitDispatcher().addTask(InitToastTask()).start()
}
```

这种分批执行的好处在于每一个 Task 占用主线程的时间相对来说很短暂，并且此时 CPU 是空闲的，这样能更有效地避免 UI 卡顿，真正地提升用户的体，并且执行时机明确。

延迟任务启动器适用于支持各种场景、各种业务把自己的启动过程任务或者非启动过程任务放在启动流程结束之后运行，这也有助于我们自己在优化的过程中，更加轻松的将非必需低优先级任务进行排布。

**注意：**

-   能异步执行的 Task 优先使用异步启动器在 Application 中执行，或者是必须在 `Application#onCreate()` 完成前必须执行完的非异步 Task。
-   对于不能异步的 Task，我们可以利用延迟启动器进行初始化。如果任务可以到使用时再加载，可以使用懒加载的方式。

源码地址：[延迟任务启动器](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2Fsuming77%2FSumTea_Android "https://github.com/suming77/SumTea_Android")

### 5\. 线程优化

**线程优化主要是减少 CPU 调度带来的波动，让启动时间更稳定**。如果启动过程中有太多的线程一起启动，会给 CPU 带来非常大的压力，尤其是比较低端的机器。线程的频繁创建是耗性能的，过多的线程同时跑会让主线程的 Sleep 和 Runnable 状态变多，应用的启动速度变慢，优化的过程中要注意以下三点：

1.  **控制线程数量**。线程数量太多会相互竞争 CPU 资源，导致 CPU 频繁切换，降低线程运行效率，因此要有统一的线程池，并且根据机器性能来控制数量。

```
// 当前设备可以使用的 CPU 核数
private val CPU_COUNT = Runtime.getRuntime().availableProcessors()

// 线程池核心线程数，其数量在2 ~ 5这个区域内
private val CORE_POOL_SIZE = Math.max(2, Math.min(CPU_COUNT - 1, 5))
```

2.  **检查线程间的锁 ，防止依赖等待**。为了提高启动过程任务执行的速度，通过去除不必要的锁、降低锁粒度、减少持锁时间以及其他通用的方案减少锁问题对启动的影响。通过 systrace 可以看到锁等待的事件，我们需要排查这些等待是否可以优化，特别是防止主线程出现长时间的空转。
    
3.  **合理使用 CPU 架构**。IO 密集型任务不消耗 CPU，核心池可以很大。常见的 IO 密集型任务如文件读取、写入，网络请求等等。CPU 密集型任务核心池大小和 CPU 核心数相关，常见的 CPU 密集型任务如比较复杂的计算操作。
    

### 6\. 子进程优化

**很多 APP 都有多个子进程，无论是主动还是被动，子进程会共享 CPU 资源，导致主进程 CPU 资源紧张。如果好几个进程同时启动，系统负担则会加倍，SystemServer 也会更繁忙**。例如百度地图，极光推送等。

子进程和主进程的优先级是一样高的，如果在启动时创建子进程，那么 CPU 核心会在启动时去支持子进程的创建，可以在首页首帧时间显示之后再创建子进程。在线下情况下我们可以通过对 logcat 中 “**Start proc**” 关键字过滤信息，去发现是否存在启动子进程的情况，以及获取子进程组件相关信息。

```
ActivityManager:Start proc 11797:com.sum.tea/u0a749 for activity com.sum.tea/.ui.SplashActivity
ActivityManager:Start proc 11863:com.sum.tea:xg_vip_service/u0a749 for service com.sum.tea/com.tencent.android.tpush.service.XGVipPushService
ActivityManager:Start proc 11916:com.sum.tea:core/u0a749 for service com.sum.tea/com.qiyukf.nimlib.service.NimService
ActivityManager:com.google.android.webview:sandboxed_process0/u0i20 for webview_service com.sum.tea/org.chromium.content.app.SandboxedProcessService0
```

对于一些复杂的工程或者是三方 SDK，我们即使知道了启动进程的组件，也比较难定位到具体的启动逻辑，我们可以通过对 startService、bindService 等启动 **Service、Recevier、ContentProvider** 组件调用进行插桩，输入调用堆栈的方式，结合 “**Start proc**” 中组件的去精准定位我们的触发点。在 manifest 中声明的进程可能还存在一些 fork 出 native 进程的情况，这种进程我们可以通过 **adb shell ps** 的方式去发现。

另外，如果项目是多进程架构，只在主进程执行 Application 的 `onCreate()`。

### 7\. 系统调度优化

应用启动的时候，如果主线程的工作过多，也会造成主线程过于繁忙。

**启动过程中减少系统调用**，避免与 AMS、WMS 竞争锁。启动过程中本身 AMS 和 WMS 的工作就很多，且 AMS 和 WMS 很多操作都是带锁的，如果此时 App 再有过多的 Binder 调用与 AMS、WMS 通信，SystemServer 就会出现大量的锁等待，阻塞关键操作。

**启动过程中除了 Activity 之外的组件启动要谨慎**，因为四大组件的启动都是在主线程的，如果组件启动慢，占用了 Message 通道，也会影响应用的启动速度，系统分配的核心数有限并且分配的频率并不是最高的。

### 8\. WebView启动优化

WebView 首次创建因为 WebView UA 的原因比较耗时，我们可以采用**本地缓存**的方式解决：WebView UA 记录的是 Webview 的版本等信息，其在绝大部分情况下是不会发生变化的，因此我们完全可以把 Webview UA 缓存在本地，后续直接从本地进行读取，并且在每次应用切到后台时，去获取一次 WebView UA 更新到本地缓存，以避免造成使用过程中的卡顿。

## 十一、Activity阶段优化方案

### 1\. 类的预加载优化

一个类的加载耗时不多，但是在几百上千的基数上，也会延迟启动时间。**将进入首页的 class 对象，使用线程池提前预加载进来，在类下次使用时则可以直接使用而不需要触发类加载。**

-   `Class.forName()` 只加载类本身以及静态变量的引用类。
-   new 类实例 可以额外加载类成员变量的引用类。

怎么确定哪些类需要提前加载？切换系统的 ClassLoader，在自定义 ClassLoader 里面每个类 load 时加一个 log，在项目中运行一次，这样就可以拿到所有 log，也就是需要异步加载的类。

### 2\. SharedPreferences 加载优化

SharedPreferences 是一个 xml 的读取和存储操作，在使用前都会调用 `getSharedPreferences` 方法，这时它会去异步加载文件当中的配置文件，load 到内存当中，该文件是实际是 html，再调用 get 或 put 属性时候，如果 load 内存的操作没有执行完成，那么就会一直阻塞进行等待，都是拿同一把锁，它既然是 IO 操作，如果这文件存在很久，这个时间就会很长。如果项目比较大，有几十个类使用 SharedPreferences 文件，里面的文件也非常多。

```
val sp = getApplication().getSharedPreferences(name, Context.MODE_PRIVATE)
// 存储数据
sp.edit().putString(key, value).commit()
// 获取数据
val value = sp.getString(key, "")
```

因此解决方案就是提前进行了 Sharedpreferences 的加载，让你在使用的时候就直接可以用，避免了用的时候的等待。

1.  在 Application 中 MultiDex 之前加载 SharedPreferences：

```
class SumApplication : Application() {
    override fun attachBaseContext(base: Context?) {
        super.attachBaseContext(base)
        SPUtils.instance("SumTea", getApplicationContext())
        MultiDex.install(base)
    }

    // 复写返回this
    override fun getApplicationContext(): Context {
        return this
    }
}
```

-   **Multidex 之前加载**，此阶段的 CPU 是利用不满的。如果其他类在 Multidex 之前加载进行操作，会因为一些类不在主 dex 当中，导致崩溃。Sharedpreferences 是系统类，不会报错。
-   在 attachBaseContext() 之前的 `getApplicationContext()` 方法返回的 context 是 null，可以复写并且返回 this。

2.  创建 SharedPreferences 并且保存到 Map 中，那么需要的时候可以在 `SP_MAP` 中直接获取。

```
object SPUtils  {
    // 保存SharedPreferences
    val SP_MAP = ConcurrentHashMap<String, SharedPreferences>()

    open fun instance(name: String, applicationContext: Context): SharedPreferences? {
        var sp = SP_MAP[name]
        if (sp == null) {
            val sp = applicationContext.getSharedPreferences(name, Context.MODE_PRIVATE)
            SP_MAP[name] = sp
        }
        return sp
    }
}
```

### 3\. 启动页与首页合并

默认 App 的启动窗口流程:

> StartingWindow(SystemWindow) -> MainActivity(AppWindow)

大部分 App 启动流程:

> StartingWindow(SystemWindow) -> SplashActivity(AppWindow) -> MainActivity(AppWindow)

其实对用户来说，第一种启动流程是最好的，只涉及到一次窗口的切换；但是更多的 App 由于广告页的需求，会使用第二种流程。

启动页主要承载广告逻辑，无法对业务本身做一些预加载或者并发加载，首页的业务都在 MainActivity 里面，启动阶段需要连续启动两个 Activity，至少带来百毫秒级别的劣化。如果把这两个 Activity 进行合并，我们可以取得**两方面的收益**：

1.  减少一次 Activity 的启动过程；
2.  利用读取开屏信息和等待广告的时间，做一些与 Activity 强关联的并发任务，比如异步 View 预加载，数据加载等。

![启动页与主页合并.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/fcfc01c2bd154e2ea47265b0784aeec3~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=2242&h=768&s=114043&e=png&b=fffefe)

当然，将 SplashActivity 承接的所有内容转移到 MainActivity 上，需要注意 Activity 之后的单实例问题：

-   MainAcitvity 的 launch mode 需要设置为 `singleTop`，否则会出现 App 从后台进前台，非 MainActivity 走生命周期的现象
-   跳转到 MainAcitvity 之后，其他二级页面需要全部都关闭掉，站内跳转到 MainActivity 则附带 `CLEAR_TOP | NEW_TASK` 的标记。

### 4\. 布局优化

布局优化是一个老生常谈的问题了，**布局越复杂，测量布局绘制的时间就越长**。主要注意以下几点：

1.  一个控件的属性越少，解析越快，删除控件中的无用属性。
2.  布局的层级越少，加载速度越快。减少布局层级、降低布局嵌套。使用 Android Studio 的 Layout Inspector工具进行分析会显示当前界面的布局嵌套情况，可以通过进行分析删掉不必要的布局来达到优化的目的。
3.  `<include/>` 标签，常用于将布局中的公共部分提取出来供其他 layout 共用，以实现布局模块化，这在布局编写方便提供了大大的便利。
4.  `<ViewStub/>` 标签一样可以用来引入一个外部布局，默认不绘制，从而在解析 layout 时节省 cpu 和内存。
5.  `<merge/>` 标签用于减少布局的嵌套层次。
6.  尽可能少用 `wrap_content`，`wrap_content` 会增加布局 measure 时的计算成本，已知宽高为固定值时，不用 `wrap_content`。

**AsyncLayoutInflater 异步加载布局**

在 androidx 中已经有提供了 AsyncLayoutInflater 用于进行 xml 的异步加载。以下图中 fragment 的 rootview 为例，它是在 UI 渲染的 measure 阶段被 inflate 出来的，在 App 的启动阶段异步加载 View，子线程提前将这些 view 加载到内存，这样在首页上要使用布局时，在 measure 阶段再直接从内存中进行读取。

![2-异步加载view.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/35648d52a7d74f93b9f3754841c9a649~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1080&h=273&s=291111&e=png&a=1&b=eaded2)

LayoutInflate 进行 xml 加载包括三个步骤：

1.  将 xml 文件解析到内存中 XmlResourceParser 的 IO 过程；
2.  根据 XmlResourceParser 的 Tag name 获取 Class 的 Java 反射过程；
3.  创建 View 实例，最终生成 View 树。

AsyncLayoutInflater 的使用需要注意以下几点问题：

1.  **锁的问题**：LayoutInflate 中存在着对象锁，并且即使通过构建不同的 LayoutInflate 对象绕过这个对象锁，在 AssetManager 层、Native 层仍然会有其他锁，甚至导致了更多的耗时。
2.  **LayoutParams 的问题**： 如果传入的 root 为 null，那么 View 的 LayoutParams 将会为 null，在这个 View 被添加到父布局时会采用默认值，这会导致被 Inflate view 的属性丢失。
3.  **inflate 线程优先级的问题**：使用线程池时需要提高线程优先级，在进行异步 inflate 时可能会因为 inflate 线程优先级过低导致来不及预加载甚至比不进行预加载更耗时的情况。
4.  **对 Handler 问题**：一些自定义 View 在创建的时候会去创建 handler，需要为其指定主线程的 Looper。
5.  **动画问题**：自定义 View 里使用了动画，动画在 start 时会校验是否是 UI 线程主线程，这种情况我们需要去修改业务代码，将相关逻辑移动到后续真正添加到 View tree 时。
6.  **需要使用 Activity context 的场景**：一是在 Activity 启动之后再进行异步预加载，但是预加载的并发空间可能会被压缩；二是利用全局 context 进行预加载，但是在 add 到 view tree 之前将 context 替换为 Activity 的，以满足 Dialog 显示、LiveData 使用等场景的需求。

**布局懒加载**

**Kotlin by lazy，这种就是适用于布局是懒加载的场景**，所以其实很多时候 by lazy 用起来会更加方便。对 binding 使用 by lazy ，这样只有在真正要使用 binding 时，才会去 inflate。

```
val mBinding by lazy {
    FragmentHomeBinding.inflate(LayoutInflater.from(context), null, false)
}
```

**xml2Code**

Compose 是推荐用于构建原生 Android 界面的新工具包。它可简化并加快 Android 上的界面开发，帮助您使用更少的代码、强大的工具和直观的 Kotlin API。编写代码只需要采用`Kotlin`，而不用拆分成 `Kotlin + XML`方式了，从命令式 UI 向声明式 UI 转变。具体可参考 [《Android Compose》](https://link.juejin.cn/?target=https%3A%2F%2Fdeveloper.android.com%2Fdevelop%2Fui%2Fcompose%3Fhl%3Dzh-cn "https://developer.android.com/develop/ui/compose?hl=zh-cn")

### 5\. 页面数据预加载

启动页、首页的数据预加载：闪屏广告、首页数据 采用**内存-磁盘-网络三级缓存策略**，下次进入页面时优先直接读取缓存数据，再去网络中加载数据。

也可以在 Activity 打开之前就预加载数据，在 Activity 的 UI 布局初始化完成后显示预加载的数据，大大缩短启动时间。 **但需要注意的是过多的线程预加载会让我们的逻辑变得更加复杂**。可以参考 ：[《PreLoader》](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2Fluckybilly%2FPreLoader "https://github.com/luckybilly/PreLoader")

### 6\. 启动网络链路优化

**问题分析**

-   发送处理阶段：网络库 bindService 影响前x个请求，图片并发限制图片库线程排队。
-   网络耗时：部分请求响应 size 大，包括 SO 文件，Cache 资源，图片原图大尺寸等。
-   返回处理：个别数据网关请求 json 串复杂解析严重耗时(3s)，且历史线程排队设计不合适。
-   上屏阻塞：回调 UI 线程被阻，反映主线程卡顿严重。高端机达1s，低端机恶化达3s以上。
-   回调阻塞：部分业务回调执行耗时，阻塞主线程或回调线程。

**优化策略**

-   多次重复的请求，业务方务必收敛请求次数，减少非必须请求。
-   数据大的请求如资源文件、so 文件，非启动必须统一延后或取消。
-   业务方回调执行阻塞主线程耗时过长整改。我们知道，肉眼可见流畅运行，需要运行60帧/秒， 意味着每帧的处理时间不超过16ms。针对主线程执行回调超过16ms的业务方，推动主线程执行优化。
-   协议 json 串过于复杂导致解析耗时严重，网络并发线程数有限，解析耗时过长意味着请求长时间占用 MTOP 线程影响其他关键请求执行。推动业务方 handler 注入使用自己的线程解析或简化 json 串。

### 7\. 第三方库懒加载

很多第三方开源库都说在 Application 中进行初始化，十几个开源库都放在 Application 中，肯定对冷启动会有影响，所以可以考虑按需初始化，特别是**针对于一些应用启动时不需要初始化的库**，可以等到用时才进行加载。

例如 Glide，可以放在自己封装的图片加载类中，调用到再初始化，其它库也是同理，让 Application 变得更轻。

## 十二、业务优化方案

### 1\. 业务梳理

启动优化中，删减和重排启动业务是最为复杂的，特别是对于中大型 App，业务比较复杂繁琐。很多任务初始化都放在 Application 中做，需要梳理清楚当前启动过程的业务。

1.  梳理清楚启动过程中的每一个模块，哪些是一定需要的，哪些是可以砍掉，哪些是可以懒加载的。
2.  对于中低端机器，我们要学会降级，学会推动产品经理做一些功能取舍。
3.  根据不同的业务场景决定不同的启动模式。
4.  懒加载防止集中化，否则容易出现首页显示后用户无法操作的情形。

**一句话概述，要提高应用的启动速度，核心思想是在启动过程中少做事情，越少越好**。用以下四个维度分整理启动的各个点：

-   **必要且耗时**：不可延迟，提高优先级执行完成。启动初始化中必要且耗时的任务，考虑用异步来初始化。
-   **必要不耗时**：可以放在主线程中执行。比如某些插件初始化，只是赋值一个 context，这种耗时可以忽略，正常初始化。
-   **非必要但耗时**：可以使用异步初始化，任务低优先级执行，或者延迟初始化。比如数据上报、插件初始化。
-   **非必要不耗时**：这类任务基本对 App 正常运作无决定性影响或者业务本身流程靠后，直接在启动流程中移除，可以放在启动阶段结束之后再后台执行。

### 2\. 业务优化

通过梳理之后，剩下的都是启动过程一定要用的模块。**只能硬着头皮去做进一步的优化。优化业务中的代码效率，优化前期需要“抓大放小”，先从比较明显的瓶颈处下手，逐步进行优化**。

业务优化做到后面，会发现一些架构和历史包袱会拖累我们前进的步伐。一些历史包袱又非常沉重，而且“**牵一发动全身**”，改动风险比较大。但是历史债务要偿还，有问题的历史代码要重构，不能一直拖着。

下面有几点方案参考：

1.  流程梳理，延后执行。实际上，这一步对项目启动加速最有效果。通过流程梳理发现部分流程调用时机偏失等。
2.  更新 App 等操作无需在首屏尚未展示就调用，造成资源竞争。
3.  修改广告闪屏逻辑为下次生效。
4.  去掉用无但被执行的老代码，去掉开发阶段使用但线上被执行的代码。
5.  去掉重复逻辑执行代码。
6.  去掉调用三方 SDK 里或者 Demo 里的多余代码。

业务的梳理和优化也是最快出成果的。不过这个过程我们要学会取舍，很多产品经理为了提升自己负责的模块的数据或者达到某种效果，总会逼迫开发者做各种各样的不合理的逻辑。但是大家都想快，最后的结果就是代码一团糟，肯定都快不起来。

比如只有 1% 用户使用的功能，却让所有用户都做预加载。面对这种情况，我们要狠下心来，只留下那些真正不能删除的业务，或者通过场景化直接找到那 1% 的用户。跟产品经理 PK 可能不是那么容易，关键在于数据。**我们需要证明启动优化带来整体留存、转化的正向价值，是大于某个业务取消预加载带来的负面影响。**

## 十三、进阶优化方案

以下进阶优化方案，难度比较大，一个人完成这些是难度是有的，更有甚至往往需要一个团队。说来惭愧，以下大部分方案我亦未有在项目中实践，这里收集了各个大厂的方案做一个汇总。如果你有这方面的需求，可以参考，查漏补缺。很多方案是要根据具体的业务去做优化的，所以没有对每一种方案进行详细的介绍，要用到哪一个方案的时候，可以具体去网上查找对应方案的具体实现方法。

### 1\. Multidex 预加载优化

应用安装或者升级后首次 MultiDex 花费的时间过于漫长，我们需要进行 Multidex 的预加载优化。

**方案一：子线程执行**

**开启异步任务去执行 MultiDex 逻辑，MultiDex 不影响冷启动速度，但是难维护。**

```
AppExecutors.cpuIO.execute(Runnable {
    MultiDex.install(base)
})
```

**方案二：新进程异步加载**

在 Application 的 `attachBaseContext` 方法里，启动另一个进程的 LoadDexActivity 去异步执行 MultiDex 逻辑，显示 Loading。然后主进程 Application 进入 while 循环，不断检测 MultiDex 操作是否完成，MultiDex 执行完之后主进程 Application 继续正常的逻辑。

[Multidex优化Demo地址](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2Flanshifu%2FMultiDexTest "https://link.juejin.cn?target=https%3A%2F%2Fgithub.com%2Flanshifu%2FMultiDexTest")

**方案三：抖音BoostMultiDex优化**

为了彻底解决 MutiDex 加载时间慢的问题，抖音团队深入挖掘了 Dalvik 虚拟机的底层系统机制，对 DEX 相关的处理逻辑进行了重新设计与优化，并推出了 BoostMultiDex 方案[《抖音BoostMultiDex优化实践》](https://juejin.im/post/6844904079206907911 "https://juejin.im/post/6844904079206907911")，它能够减少 80% 以上的黑屏等待时间，挽救低版本 Android 用户的升级安装体验。

具体的实现原理为：在第一次启动的时候，直接加载没有经过 OPT 优化的原始 DEX，先使得 APP 能够正常启动。然后在后台启动一个单独进程，慢慢地做完 DEX 的 OPT 工作，尽可能避免影响到前台 APP 的正常使用。

**注意**

Android 5.0 以上默认使用 ART，在安装时已将 Class.dex 转换为 oat 文件了，无需优化，所以应判断只有在主进程及 SDK 5.0以下才进行 Multidex 的预加载。

### 2\. 类加载优化

类的使用都是通过 ClassLoader 进行类加载，这个过程会有一系列的附加操作，**第一次加载的时候，会进行校验和一系列的优化等操作，校验方法的每一个指令，是一个比较耗时的操作**。classverify 过程主要是校验 class 是否符合 java 规范，如果不符合规范则会在 verify 阶段抛出 verify 相关的异常。

![2-Class verify 优化.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/73c1055ff6374987bada6e2a3f548fec~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=750&h=184&s=118378&e=png&b=fdfdfd)

事实上 classverify 主要是针对网络下发的字节码进行校验，运行时的 classverify 是没有必要的，可以通过**关闭 classverrify** 来优化这些类的加载。目前业界已经有一些比较优秀的方案，比如运行时在内存中定位出 verify\_ 所在内存地址，然后将其设置成跳过 verify 模式以实现跳过 classverify。

```
 // If kNone, verification is disabled. kEnable by default.
  verifier::VerifyMode verify_;

  // If true, the runtime may use dex files directly with the interpreter if an oat file is not available/usable.
  bool allow_dex_file_fallback_;

  // List of supported cpu abis.
  std::vector<std::string> cpu_abilist_;

  // Specifies target SDK version to allow workarounds for certain API levels.
  int32_t target_sdk_version_;
```

这个方案并不一定对所有的应用都有价值，在进行优化之前可以通过 oatdump 命令输出一下宿主、插件中在运行时进行 classverify 的类信息，对于存在大量类在运行时 verify 的情况可以采用上面介绍的方案进行优化。

```
oatdump --oat-file=xxx.odex > dump.txt
cat dump.txt  | grep -i "verified at runtime" |wc -l
```

对象第一次创建的时候，JVM 首先检查对应的 Class 对象是否已经加载。如果没有加载，JVM 会根据类名查找 `.class` 文件，将其 Class 对象载入。同一个类第二次 new 的时候就不需要加载类对象，而是直接实例化，创建时间就缩短了。

### 3\. GC 抑制优化

**在启动阶段伴随的内存申请和释放，对于这个过程也是非常耗时的**。触发 GC 后可能会抢占我们的 cpu 资源甚至导致我们的线程被挂起，如果启动过程中存在大量的 GC，那么我们的启动速度将会受到比较大的影响。

虽然不同版本有提升，但这个时间仍然很长。可以通过 GC 抑制的通用办法去减少 GC 对启动速度的影响，这个方案实际上是 nativeHook 的方案。

对 Dalvik 来说，我们可以通过 systrace 单独查看整个启动过程 GC 的时间:

```
python systrace.py dalvik -b 90960 -a com.sample.gc
```

GC 耗时情况报告：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8b7de251becb4211b20cc7bc0d31b4b1~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1920&h=446&s=129017&e=png&b=f5f5f5)

使用 `Debug.startAllocCounting` 来监控启动过程总 GC 的耗时情况，特别是阻塞式同步 GC 的总次数和耗时:

```
// GC使用的总耗时，单位是毫秒
Debug.getRuntimeStat("art.gc.gc-time");
// 阻塞式GC的总耗时
Debug.getRuntimeStat("art.gc.blocking-gc-time");
```

如果我们发现主线程出现比较多的 GC 同步等待，那就需要通过 Allocation 工具做进一步的分析。需要注意以下几点：

1.  启动过程避免进行大量的字符串操作，特别是序列化跟反序列化过程。
2.  一些频繁创建的对象，例如网络库和图片库中的 Byte 数组、Buffer 可以复用。
3.  如果一些模块实在需要频繁创建对象，可以考虑移到 Native 实现。

Java 对象的逃逸也很容易引起 GC 问题，我们在写代码的时候比较容易忽略这个点。我们应该保证对象生命周期尽量的短，在栈上就进行销毁。

最根本办法：**减少我们启动阶段代码的执行**，减少内存资源的申请与占用，这个方案需要我们去改造我们的代码实现。可以参考[《支付宝客户端架构解析：Android 客户端启动速度优化之「垃圾回收」》](https://link.juejin.cn/?target=https%3A%2F%2Fmp.weixin.qq.com%2Fs%2FePjxcyF3N1vLYvD5dPIjUw "https://mp.weixin.qq.com/s/ePjxcyF3N1vLYvD5dPIjUw")

### 4\. CPU 锁频

手机分配给 CPU 的核数是固定的，比如固定是8核，但是 CPU 的频率不是很高，比如可以给到100%的频率，但是实际只给50%频率，CPU 锁频也就是对频率进行拉升起来，对于提高启动速度是非常有帮助的。

如果我们在启动阶段或者打开 Activity 时拉升 CPU 的频率1S，但是大部分手机很难在1S内打开 APP。可以根据手机设备低、中、高端进行平均启动耗时设置三个值，按照不同段位设备CPU拉升时间不同，比如低端设备拉升3s，中端设备拉升2s，高端设备拉升1s。

在 Android 系统中，**CPU 相关的信息存储在 `/sys/devices/system/cpu` 目录的文件中，通过对该目录下的特定文件进行写值，实现对 CPU 频率等状态信息的更改**。

这个方案有个很明显的缺点就是**耗电量增加**。

### 5\. I/O 优化

在高负载的时候，I/O 性能下降得会比较快。特别是对于低端机，同样的 I/O 操作耗时可能是高端设备的十倍或者更多。启动过程不建议出现网络 I/O，而且磁盘 I/O 也是需要优化的。

下面图中可以看到低内存的时候，启动应用主线程有较多的 IO 等待（UI Thread 这一栏，橘红色代表 IO 等待 ）

![io优化.jpeg](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/0cdc4ad2fc7445be8a6a42efd6e747b5~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1552&h=329&s=87649&e=jpg&b=edecec)

首先我们要梳理清楚启动过程中读了什么文件、多少个字节、Buffer 是多大、耗时多久、在什么线程等一系列信息。有一些用户本地积累了非常多的数据，重度用户是启动优化一定要覆盖的群体，我们要做一些特殊的优化策略。

比如在启动时解析用户数据需要100ms，但是只用到其中的一个数据，我们可以将这个数据单独存储，在启动时单独获取这个数据，这就大大减少了解析的耗时。

还有一个是数据结构的选择问题，我们在启动过程中 SharedPreference 在初始化的时候还是要全部数据一起解析。如果它的数据量超过 1000 条，启动过程解析时间可能就超过 100 毫秒。如果只解析启动过程用到的数据项则会很大程度减少解析时间，启动过程适合使用随机读写的数据结构。

总的来说，通过减少启动阶段不必要的 IO、对关键链路上的 IO 进行预读以及其他通用的 IO 优化方案提升 IO 效率。

### 6\. 数据重排

Dex 文件用的到的类和安装包 APK 里面各种资源文件一般都比较小，但是读取非常频繁。我们可以利用系统这个机制将它们按照读取顺序重新排列，减少真实的磁盘 I/O 次数。

**类重排**

启动过程中类加载顺序可以通过复写 ClassLoader 得到：

```
class GetClassLoader extends PathClassLoader {
    public Class<?> findClass(String name) {
        // 将 name 记录到文件
        writeToFile(name，"coldstart_classes.txt");
        return super.findClass(name);
    }
}
```

然后通过 ReDex 的 Interdex 调整类在 Dex 中的排列顺序，最后可以利用 010 Editor 查看修改后的效果。具体实现可以参考 [《Redex 初探与 Interdex：Andorid 冷启动优化》](https://link.juejin.cn/?target=https%3A%2F%2Fmp.weixin.qq.com%2Fs%2FBf41Kez_OLZTyty4EondHA "https://mp.weixin.qq.com/s/Bf41Kez_OLZTyty4EondHA")。

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/149cb3fe08f44f47b59c0ceeb75cbdb7~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1920&h=581&s=540461&e=png&b=403e3a)

[《ReDex》](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2Ffacebook%2Fredex "https://github.com/facebook/redex") 是 Facebook 开源的 Dex 优化工具，它里面有非常多好用的东西。

**资源文件重排**

Facebook 在比较早的时候就使用“**资源热图**”来实现资源文件的重排，最近支付宝在[《通过安装包重排布优化 Android 端启动性能》](https://link.juejin.cn/?target=https%3A%2F%2Fmp.weixin.qq.com%2Fs%2F79tAFx6zi3JRG-ewoapIVQ "https://mp.weixin.qq.com/s/79tAFx6zi3JRG-ewoapIVQ")中也详细讲述了资源重排的原理和落地方法。

在实现上，它们都是通过修改 Kernel 源码，单独编译了一个特殊的 ROM。统计应用启动过程加载了安装包中哪些资源文件，跟类重排一样，我们可以得到一个资源加载的顺序列表。

## 十四、黑科技优化方案

虽然是黑科技，这种优化方案效果比较明显，但是难度也更大。我们需要慎重选择，当你足够了解它们内部的机制以后，可以选择性的使用。

### 1\. 应用保活

**保活**，是各个应用开发者的噩梦，也是 Android 厂商关注和打击的重点。不过从启动的角度来看，可以减少 Application 创建跟初始化的时间，让冷启动变成温启动。如果应用进程不被杀，那么启动自然就快了，所以保活对应用启动速度也是有极大的帮助。不过在 Android Target 26 之后，保活的确变得越来越难。

当然这里说的保活，并不是建议大家用各种黑科技、相互唤醒、通知轰炸这种保活手段，而是提供真正的功能，能让用户觉得你在后台是合理的、可以接收的。比如在后台的时候，资源能释放的都释放掉，不要一直在后台做耗电操作，该停的服务停掉，该关的动画关掉。

对于应用开发者来说，上面说的都太过理想化了，而且目前的手机厂商也会很暴力，应用到了后台就会处理掉。不过这毕竟是一个方向，Google 也在规范应用后台行为和规范厂商处理应用这两方面都在做努力，Android 系统的生态，还是需要应用开发者和 Android 厂商一起去改善。

### 2\. 厂商合作

**与厂商的合作，在 App 启动的时候，系统对要启动的应用做绝对的资源倾斜，比如 CPU、IO、GPU 等，进行硬核代码优化，系统策略优化**。优化后台内存、去掉重复拉起、去掉流氓逻辑、积极响应低内存警告，做好这些话后可以跟系统厂商联系，放到查杀白名单和自启动白名单的可行性。

部分厂商也提供了资源调度的 SDK ，应用可以接入这些 SDK，在需要资源的时候直接调用 SDK 获取。例如微信的 Hardcoder 方案和 OPPO 推出的[《Hyper Boost》](https://link.juejin.cn/?target=https%3A%2F%2Fwww.geekpark.net%2Fnews%2F233791 "https://www.geekpark.net/news/233791")方案。根据 OPPO 的数据，对于手机 QQ、淘宝、微信启动场景会直接有 20% 以上的优化。

应用加固对启动速度来说简直是灾难，有时候我们需要做一些权衡和选择。我们还是更希望通过手段可以真正优化整个耗时，而不是一些取巧的方式。

### 3\. 插件化和热修复

插件化曾经一度是 Android 开发重要的技术方向，各个大厂都推出了自己的插件化框架，从360的 replugin 到阿里的 atlas。它最终都是时代的产物，随着安卓的发展，慢慢淡出视野，现在我们可能也不再需要插件化的框架了。

插件化能动态发布更新，增加用户的体验。使用户不用重新安装 apk 就能升级 app，减少版本的发布频率，能让多条业务线并行开发，也可以按需加载不同的模块，实现灵活的功能配置，还可减少主包的体积，间接解决65535和多本 dex 问题，甚至还承载着热修复的能力。

到了 2015 年，淘宝的 Dexposed、支付宝的 AndFix 以及微信的 Tinker 等热修复技术开始“百花齐放”。事实上大部分的框架在设计上都存在大量的 Hook 和私有 API 调用。

Android Runtime 每个版本都有很多的优化，因为插件化和热修复用到的一些黑科技，导致底层 Runtime 的优化我们是享受不到的。Tinker 框架在加载补丁后，应用启动速度会降低 5%～10%。

## 十五、应用启动监控

终于千辛万苦的优化好了，我们还要找一套合理、准确的方法来度量优化的成果。同时还要对它做全方位的监控，以免被人破坏劳动果实。

### 1\. 线下监控

我们很难拿到竞品的线上数据，所以实验室监控也非常适合做竞品的对比测试。具体的启动数据分析可以参考[《深入研究Android启动速度优化（上）- 启动耗时统计的八种方式》](https://juejin.cn/post/7354233812593246248 "https://juejin.cn/post/7354233812593246248")

启动的实验室监控可以定期自动去跑，需要注意的是，我们应该覆盖高、中、低端机不同的场景。

### 2\. 线上监控

实验室覆盖的场景和机型还是有限的，是驴是马我们还是要发布到线上进行验证。针对线上，启动监控会更加复杂一些，线上监控多阶段的耗时时间（APPlication、Activity、View 渲染等耗时）。[《Android Vitals》](https://link.juejin.cn/?target=https%3A%2F%2Fdeveloper.android.google.cn%2Ftopic%2Fperformance%2Fvitals%2Flaunch-time%3Fhl%3Dzh-cn "https://developer.android.google.cn/topic/performance/vitals/launch-time?hl=zh-cn") 可以对应用冷启动、温启动时间做监控。

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/319c50f6bc764726a6090c8caa10fb24~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=1908&h=490&s=45398&e=png&b=fefefe)

事实上，每个应用启动的流程都非常复杂，上面的图并不能真实反映每个应用的启动耗时。启动耗时的计算需要考虑非常多的细节，比如：

1.  启动结束的统计时机。是否是使用用户真正可以操作的时间作为启动结束的时间。
2.  启动时间扣除的逻辑。闪屏、广告和新手引导这些时间都应该从启动时间里扣除。
3.  启动排除逻辑。Broadcast、Server 拉起，启动过程进入后台这些都需要排除出统计。

经过精密的扣除和排除逻辑，我们最终可以得到用户的线上启动耗时。正如我在上一篇文章所说的，准确的启动耗时统计是非常重要的。有很多优化在实验室完成之后，还需要在线上灰度验证效果。这个前提是启动统计是准确的，整个效果评估是真实的。如果启动时间变高，可以查询新加了什么代码，或者业务逻辑。

除了指标的监控，启动的线上堆栈监控更加困难。字节开源的[《btrace》](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2Fbytedance%2Fbtrace "https://github.com/bytedance/btrace")能够满足线上的监控要求。对启动的整个流程耗时做监控，并且在后台直接对不同的版本做自动化对比，监控新版本是否有新增耗时的函数。

## 十六、防劣化机制

### 1\. 技术评审

技术评审的目的是以技术的角度评估本次项目功能的实现方式，业务架构，采取的技术方案可行性（优劣性），有没有逻辑缺陷和技术缺陷，可能遇到的重难点，可以采取的降级策略进行论证。

相关开发在开发涉及到启动相关需求时，拉通该模块负责人进行技术方案评审。经过技术评审阶段，业务估时也更加的合理。更加充分的了解需求，有哪些问题尽早抛出来，该换方案的换方案，该加时间的加时间。

> 许多公司是没有过剩的能力进行技术评审的，开发人员也如公交车一般上下，这也是时间越久代码质量越差的原因。

### 2\. Code Review 机制

代码评审机制在帮助团队找到代码缺陷有巨大作用，一般可以找到65%的代码错误，最高可以80%。能传播知识，增进代码质量，找出潜在的 bug。

1.  对代码健壮性检查，是否有潜在安全、性能风险，异常情况有没有足够的容错处理；
2.  对代码质量检测，代码写的越多，潜在的问题就越多，采用的数据结构是否合理，业务代码对新旧版本的影响，有没有不按照规范的写法。

Code Review 机制是防劣化机制建设的重点。

## 十七、总结

Android 启动优化主要说了四大部分内容，第一部分内容是启动流程和阶段分析，第二部分内容是耗时分析工具，第三部分内容是启动优化实战方案，第四部分是监控和防劣化。

从创建进程，启动的应用，界面绘制三个阶段中，Application 和 Activity 生命周期阶段启动速度的优化方向；也重新定义了首帧时间，选择在列表上面第一个 itemView 的 `perDrawCallback()` 方法的回调时机当作时间结束点。

前一篇文章提到的几种工具，Traceview 性能损耗太大，得出的结果并不真实；systrace 可以很方便地追踪关键系统调用的耗时情况，但是不支持应用程序代码的耗时分析。综合来看，在卡顿优化中提到 `systrace + 函数插桩` 似乎是比较理想的方案，而且它还可以看到系统的一些关键事件，例如 GC、System Server、CPU 调度等。

这里主要讲述启动优化方法，可以进一步减少启动耗时，详细介绍了 Application阶段优化方案，Activity 阶段优化方案，业务优化方案。进阶优化方案汇总了各个大厂的方案，大家可以参考学习研究。然后我们探讨了一些黑科技对启动的影响，对于黑科技我们需要两面看，在选择时也要慎重。最后我们探讨了如何监控启动速度和防劣化机制的建设。

启动优化是一个需要持续迭代与打磨的的过程，需要耐得住寂寞，把整个流程摸清摸透，一点点把时间抠出来，特别是对于低端机和系统繁忙的场景。

不管怎么说，你都需要谨记一点：**对于启动优化要警惕 KPI 化，我们要解决的不是一个数字，而是用户真正的体验问题**。

![启动优化实战方案.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/76179bf837754033b95bc56735ddf0fc~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=2592&h=6070&s=1352123&e=png&b=ffffff)

## 点关注，不迷路

好了各位，以上就是这篇文章的全部内容了，很感谢您阅读这篇文章。我是suming，感谢支持和认可，您的**点赞**就是我创作的最大动力。**山水有相逢**，我们下篇文章见！

本人水平有限，文章难免会有错误，请批评指正，不胜感激 ！

## 参考链接

-   [Google官网：应用启动](https://link.juejin.cn/?target=https%3A%2F%2Fdeveloper.android.google.cn%2Ftopic%2Fperformance%2Fvitals%2Flaunch-time%3Fhl%3Dzh-cn "https://developer.android.google.cn/topic/performance/vitals/launch-time?hl=zh-cn")
-   [Improving App Performance with Baseline Profiles](https://link.juejin.cn/?target=https%3A%2F%2Fandroid-developers.googleblog.com%2F2022%2F01%2Fimproving-app-performance-with-baseline.html "https://android-developers.googleblog.com/2022/01/improving-app-performance-with-baseline.html")
-   [Android 开发高手课](https://link.juejin.cn/?target=https%3A%2F%2Ftime.geekbang.org%2Fcolumn%2Fintro%2F142%3Fgk_ucode%3D6891EC39BDEA36%26tab%3Dcatalog "https://time.geekbang.org/column/intro/142?gk_ucode=6891EC39BDEA36&tab=catalog")
-   [国内Top团队大牛带你玩转Android性能分析与优化](https://link.juejin.cn/?target=https%3A%2F%2Fcoding.imooc.com%2Fclass%2Fpackage%2F308.html "https://coding.imooc.com/class/package/308.html")
-   [Android App 启动优化全记录](https://link.juejin.cn/?target=https%3A%2F%2Fandroidperformance.com%2F2019%2F11%2F18%2FAndroid-App-Lunch-Optimize "https://androidperformance.com/2019/11/18/Android-App-Lunch-Optimize")
-   [抖音 Android 性能优化系列：启动优化实践](https://juejin.cn/post/7080065015197204511 "https://juejin.cn/post/7080065015197204511")
-   [深入探索Android启动速度优化（上）](https://juejin.cn/post/6844904093786308622#heading-164 "https://juejin.cn/post/6844904093786308622#heading-164")
-   [深入探索Android启动速度优化（下）](https://juejin.cn/post/6870457006784774152 "https://juejin.cn/post/6870457006784774152")
-   [Android启动优化实践 - 秒开率从17%提升至75%](https://juejin.cn/post/7306692609497546752#heading-19 "https://juejin.cn/post/7306692609497546752#heading-19")
-   [探索 Android 启动优化方法](https://juejin.cn/post/6844903919580086280#heading-15 "https://juejin.cn/post/6844903919580086280#heading-15")
-   [历时1年，上百万行代码！首次揭秘手淘全链路性能优化（上）](https://link.juejin.cn/?target=https%3A%2F%2Fdeveloper.aliyun.com%2Farticle%2F710466 "https://developer.aliyun.com/article/710466")

### 希望我们能成为朋友，在 [Github](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2Fsuming77%2FAndroid-Core-Realm "https://github.com/suming77/Android-Core-Realm")、[掘金](https://juejin.cn/user/1654096907477549 "https://juejin.cn/user/1654096907477549") 上一起分享知识，一起共勉！Keep Moving!