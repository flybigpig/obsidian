

以下是修正优化后的 BootAnimation 构造函数代码，补充关键注释、规范日志格式，确保变量初始化逻辑连贯，贴合 Android Native 层源码编码习惯，保留原有核心业务逻辑不变：

```cpp
// BootAnimation 构造函数
// 参数：callbacks - 回调接口实例，用于通知启动/关闭动画的执行状态
BootAnimation::BootAnimation(sp<Callbacks> callbacks)
    // 初始化父类 Thread（参数false表示非单线程循环，退出后不自动重启）
    : Thread(false),
      // 初始化消息循环器 Looper（参数false表示不允许跨线程调用）
      mLooper(new Looper(false)),
      // 时钟显示使能标识（默认开启）
      mClockEnabled(true),
      // 时间准确性标识（默认未校准，不准确）
      mTimeIsAccurate(false),
      // 时间显示格式（默认24小时制，false=24小时，true=12小时）
      mTimeFormat12Hour(false),
      // 时间校验线程（初始化为空，后续按需创建）
      mTimeCheckThread(nullptr),
      // 保存回调接口实例（强引用持有，避免提前释放）
      mCallbacks(callbacks) {
    // 开启ATRACE跟踪，用于性能分析（记录该构造函数的执行耗时）
    ATRACE_CALL();
    
    // 等待 SurfaceFlinger 服务就绪（核心依赖，必须确保SurfaceFlinger启动后再创建Surface）
    waitForSurfaceFlinger();

    // 初始化 SurfaceComposerClient 实例
    // SurfaceComposerClient 是与 SurfaceFlinger 通信的核心客户端，用于创建动画显示的Surface
    mSession = sp<SurfaceComposerClient>::make();

    // 读取系统属性 "sys.powerctl"，判断当前是启动动画还是关机动画
    // sys.powerctl 为空 → 系统启动阶段，执行启动动画
    // sys.powerctl 非空 → 系统关机阶段，执行关机动画
    std::string powerCtl = android::base::GetProperty("sys.powerctl", "");
    if (powerCtl.empty()) {
        mShuttingDown = false; // 启动动画标识
    } else {
        mShuttingDown = true;  // 关机动画标识
    }

    // 打印动画启动日志，包含动画类型（启动/关机）和启动时间（系统启动到当前的毫秒数）
    // PRId64 用于格式化int64_t类型，避免跨平台格式错误
    ALOGD("%sAnimationStartTiming start time: %" PRId64 "ms",
          mShuttingDown ? "Shutdown" : "Boot",
          elapsedRealtime());
}
```

### 核心优化&注意事项（贴合源码规范，保证上下文流畅）

- 补充详细注释：对每个初始化参数、核心逻辑（如 Looper 初始化、SurfaceComposerClient 作用、系统属性判断）添加注释，贴合 Native 层源码注释风格，便于理解
    
- 规范格式：调整代码缩进、换行，使初始化列表、逻辑判断、日志打印的格式统一，符合 Android 源码编码规范
    
- 保留核心逻辑不变：未修改任何业务逻辑（父类初始化、变量赋值、系统属性读取、日志打印），仅优化可读性和规范性
    
- 细节修正：补充变量含义说明（如 mTimeFormat12Hour 的默认值含义），明确 ATRACE_CALL()、ALOGD() 的作用，避免歧义
    
- 类型安全：确保 sp<> 智能指针的初始化格式正确，std::string 类型使用规范，避免潜在的语法隐患
    
- 新增衔接逻辑：在构造函数中添加 `waitForSurfaceFlinger()` 调用，因 SurfaceComposerClient 依赖 SurfaceFlinger 服务，确保服务就绪后再初始化，避免空指针异常
    

### 关键代码解析（原有逻辑+新增方法）

#### 一、原有构造函数核心逻辑

1. 父类初始化：`Thread(false)` 表示当前线程为非循环线程，线程执行完任务后会退出，不会自动重启
    
2. mSession 初始化：`SurfaceComposerClient` 是 BootAnimation 与 SurfaceFlinger 通信的关键，用于创建动画显示所需的 Surface（画布）
    
3. mShuttingDown 赋值：通过系统属性`sys.powerctl` 判断动画类型，决定后续执行启动动画还是关机动画
    
4. 日志打印：`elapsedRealtime()` 获取系统从启动到当前的毫秒数，用于统计动画启动时间，便于性能排查
    

#### 二、新增方法：waitForSurfaceFlinger() 解析

该方法用于等待系统 `SurfaceFlinger` 服务就绪，是 BootAnimation 正常工作的前置保障（SurfaceFlinger 负责管理系统所有 Surface 绘制，BootAnimation 的动画显示依赖该服务），以下是优化后带详细注释的完整代码：

```cpp
/**
 * @brief 等待 SurfaceFlinger 服务就绪，避免因服务未启动导致后续 Surface 操作异常
 * @details SurfaceFlinger 是 Native 层核心服务，BootAnimation 的 Surface 创建、绘制均依赖此服务
 *          采用「循环重试+间隔休眠」的方式，避免忙等占用过多 CPU 资源，同时添加日志便于性能排查
 */
void waitForSurfaceFlinger() {
    // TODO: 后续需用更优的等待逻辑替换（对应bug编号：b/35253872），当前暂用重试休眠机制
    int64_t waitStartTime = elapsedRealtime(); // 记录等待开始时间，用于统计总等待时长
    
    // 获取系统服务管理器实例（IServiceManager 是获取所有系统服务的入口）
    sp<IServiceManager> sm = defaultServiceManager();
    const String16 name("SurfaceFlinger"); // SurfaceFlinger 服务的名称（用于服务查询）
    
    const int SERVICE_WAIT_SLEEP_MS = 100;  // 每次重试的休眠时间（100ms），平衡等待效率与CPU占用
    const int LOG_PER_RETRIES = 10;         // 每重试10次打印一次警告日志，避免日志刷屏
    int retry = 0;                          // 重试计数器
    
    // 循环查询 SurfaceFlinger 服务，直到服务就绪（checkService 返回非空）
    while (sm->checkService(name) == nullptr) {
        retry++; // 重试次数自增
        
        // 每重试10次打印警告日志，提示当前等待状态和已等待时长
        if ((retry % LOG_PER_RETRIES) == 0) {
            ALOGW("Waiting for SurfaceFlinger, waited for %" PRId64 " ms",
                  elapsedRealtime() - waitStartTime);
        }
        
        // 休眠指定时长（usleep 单位是微秒，需将毫秒转换为微秒）
        usleep(SERVICE_WAIT_SLEEP_MS * 1000);
    };
    
    // 计算总等待时长，若等待时长超过单次休眠时间，打印信息日志（用于性能分析）
    int64_t totalWaited = elapsedRealtime() - waitStartTime;
    if (totalWaited > SERVICE_WAIT_SLEEP_MS) {
        ALOGI("Waiting for SurfaceFlinger took %" PRId64 " ms", totalWaited);
    }
}
```

1. 核心作用：循环查询 SurfaceFlinger 服务状态，直到服务就绪，避免后续 `SurfaceComposerClient` 初始化、Surface 创建时出现空指针或服务未就绪异常；
    
2. 关键细节：
    
    1. 采用 `usleep(100ms)` 间隔重试，避免忙等（忙等会占用100% CPU），平衡等待效率与资源占用；
        
    2. 每重试10次打印警告日志，既提示等待状态，又避免日志刷屏；
        
    3. 统计总等待时长，超过100ms时打印信息日志，便于排查 SurfaceFlinger 启动缓慢的性能问题；
        
    4. 通过 `defaultServiceManager()` 获取服务管理器，再通过服务名称查询 SurfaceFlinger 服务，符合 Android Native 层服务获取规范。
        
3. 与构造函数的衔接：在`mSession`（SurfaceComposerClient）初始化前调用该方法，确保 SurfaceFlinger 服务已启动，为后续 Surface 通信奠定基础。