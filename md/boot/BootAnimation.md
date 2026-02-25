
```
// frameworks/base/cmds/bootanimation/bootanimation_main.cpp
int main()
{
    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_DISPLAY);

    bool noBootAnimation = bootAnimationDisabled();
    ALOGI_IF(noBootAnimation,  "boot animation disabled");
    if (!noBootAnimation) {

        sp<ProcessState> proc(ProcessState::self());
        ProcessState::self()->startThreadPool();

        // create the boot animation object (may take up to 200ms for 2MB zip)
        sp<BootAnimation> boot = new BootAnimation(audioplay::createAnimationCallbacks());

        waitForSurfaceFlinger();

        boot->run("BootAnimation", PRIORITY_DISPLAY);

        ALOGV("Boot animation set up. Joining pool.");

        IPCThreadState::self()->joinThreadPool();
    }
    return 0;
}
```



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