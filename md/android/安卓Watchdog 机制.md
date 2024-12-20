在Android系统中，也设计了一个软件层面Watchdog，用于保护一些重要的系统服务，比如：AMS、WMS、PMS等，由于以上核心服务运行在system_server进程里面，所以当以上服务出现异常时，通常会将system_server进程kill掉，即让Android系统重启。

WatchDog功能主要是分析系统核心服务的重要线程、和锁 是否处于Blocked状态，即以下两个功能：

> - 监控 system_server 中几个关键的锁，原理是在 android_fg 线程中尝试加锁
> - 监控几个常用线程的执行时间，原理是在这几个线程中执行任务

WatchDog 的启动是在系统进程初始化后

```
991      private void startBootstrapServices(@NonNull TimingsTraceAndSlog t) {
992          t.traceBegin("startBootstrapServices");
993  
994          // Start the watchdog as early as possible so we can crash the system server
995          // if we deadlock during early boot
996          t.traceBegin("StartWatchdog");

// 调用 Watchdog 的构造方法初始化
997          final Watchdog watchdog = Watchdog.getInstance();

// 调用start 方法，启动线程
998          watchdog.start();
999          t.traceEnd();
1000  
1001          Slog.i(TAG, "Reading configuration...");
1002          final String TAG_SYSTEM_CONFIG = "ReadingSystemConfig";
```

## 1. WatchDog 的初始化

调用 Watchdog 的构造方法初始化

```
    public static Watchdog getInstance() {
        if (sWatchdog == null) {
// 单例模式，创建 Watchdog 对象
            sWatchdog = new Watchdog();
        }

        return sWatchdog;
    }
```

Watchdog 构造方法

```
    private Watchdog() {
// 创建线程，名字为 watchdog，线程调用run方法 
        mThread = new Thread(this::run, "watchdog");

// 监听锁机制和 前台线程 FgThread 的handler处理 ，其也是单例模式，
// 提供给其他对象使用，如 PermissionManagerService
        mMonitorChecker = new HandlerChecker(FgThread.getHandler(),
                "foreground thread", DEFAULT_TIMEOUT);

// 将 mMonitorChecker 也保存到 mHandlerCheckers 中，去监听handler 是否超时，默认超时时间为 DEFAULT_TIMEOUT 30秒
        mHandlerCheckers.add(mMonitorChecker);

// 监听系统进程的主线程
        mHandlerCheckers.add(new HandlerChecker(new Handler(Looper.getMainLooper()),
                "main thread", DEFAULT_TIMEOUT));

//  监听ui线程 UI thread.
        mHandlerCheckers.add(new HandlerChecker(UiThread.getHandler(),
                "ui thread", DEFAULT_TIMEOUT));
//  监听Io线程 
        mHandlerCheckers.add(new HandlerChecker(IoThread.getHandler(),
                "i/o thread", DEFAULT_TIMEOUT));

//  监听DisplayThread线程 
        mHandlerCheckers.add(new HandlerChecker(DisplayThread.getHandler(),
                "display thread", DEFAULT_TIMEOUT));

//  监听动画 AnimationThread线程 
        mHandlerCheckers.add(new HandlerChecker(AnimationThread.getHandler(),
                "animation thread", DEFAULT_TIMEOUT));

//  监听surface animation thread.也是关于动画的
        mHandlerCheckers.add(new HandlerChecker(SurfaceAnimationThread.getHandler(),
                "surface animation thread", DEFAULT_TIMEOUT));

// 监听是否有可用的binder 线程
        addMonitor(new BinderThreadMonitor());

// 将系统进程增加到感兴趣的进程队列中
        mInterestingJavaPids.add(Process.myPid());

        // See the notes on DEFAULT_TIMEOUT.
        assert DB ||
                DEFAULT_TIMEOUT > ZygoteConnectionConstants.WRAPPED_PID_TIMEOUT_MILLIS;

        mTraceErrorLogger = new TraceErrorLogger();
    }
```

在 SystemServer 启动过程中初始化 Watchdog。Watchdog 除了在初始化时创建一个线程mThread ，还会构建很多 HandlerChecker，大致可以分为两类：

> 1. Monitor Checker，用于检查 Monitor 对象可能发生的死锁，AMS，IMS，WMS PMS 等核心的系统服务都是 Monitor 对象
> 2. Looper Checker，用于检查线程的消息队列是否长时间处于工作状态。Watchdog 自身的消息队列，ui，io， Display 这些全局的消息队列都是被检查的对象。此外，一些重要的线程的消息队列，也会加入到 Looper Checker中，譬如 AMS，WMS 这些是在对应的对象初始化时加入的

new HandlerChecker 的构造函数初始化

```
// HandlerChecker 实现了 Runnable接口，会回调run 方法
    public final class HandlerChecker implements Runnable {
        private final Handler mHandler;
        private final String mName;
        private final long mWaitMax;
        private final ArrayList mMonitors = new ArrayList();
        private final ArrayList mMonitorQueue = new ArrayList();
        private boolean mCompleted;
        private Monitor mCurrentMonitor;
        private long mStartTime;
        private int mPauseCount;

        HandlerChecker(Handler handler, String name, long waitMaxMillis) {
            mHandler = handler;
            mName = name;
// 等待的最长时间设置给 mWaitMax
            mWaitMax = waitMaxMillis;
// 初始化 mCompleted 为 true
            mCompleted = true;
        }

// 增加监听锁的方法，调用的接口是 addMonitor
        void addMonitorLocked(Monitor monitor) {
            // We don't want to update mMonitors when the Handler is in the middle of checking
            // all monitors. We will update mMonitors on the next schedule if it is safe
            mMonitorQueue.add(monitor);
        }

    public void addMonitor(Monitor monitor) {
        synchronized (mLock) {
            mMonitorChecker.addMonitorLocked(monitor);
        }
    }
```

系统进程的service 给watchdog 监听

由前面分析，监听是否持锁时间长，在 前台线程去监听

```
// ams 实现了 Watchdog.Monitor 接口
431  public class ActivityManagerService extends IActivityManager.Stub
432          implements Watchdog.Monitor, BatteryStatsImpl.BatteryCallback, ActivityManagerGlobalLock {

2226      public ActivityManagerService(Context systemContext, ActivityTaskManagerService atm) {
2227          LockGuard.installLock(this, LockGuard.INDEX_ACTIVITY);
2228          mInjector = new Injector(systemContext);
2229          mContext = systemContext;

。。。。
// 监听是否持锁时间长
2328          Watchdog.getInstance().addMonitor(this);
// 监听ams 的handler 是否超时
2329          Watchdog.getInstance().addThread(mHandler);

// ams 实现了 Watchdog.Monitor 接口，会回调 monitor 方法
15024      public void monitor() {
15025          synchronized (this) { }
15026      }
```

同样wms 也监听了锁是否超时

```
330  public class WindowManagerService extends IWindowManager.Stub
331          implements Watchdog.Monitor, WindowManagerPolicy.WindowManagerFuncs {

1417      public void onInitReady() {
1418          initPolicy();
1419  
1420          // Add ourself to the Watchdog monitors.
1421          Watchdog.getInstance().addMonitor(this);

=========
6658      @Override
6659      public void monitor() {

// 监听mGlobalLock 锁
6660          synchronized (mGlobalLock) { }
6661      }
```

## 2. 调用start 方法，启动WatchDog 线程监听

```
    public void start() {
// 即调用this::run 方法
        mThread.start();
    }
```

调用this::run 方法

```
    private void run() {
        boolean waitedHalf = false;
        while (true) {
            List blockedCheckers = Collections.emptyList();
            String subject = "";
            boolean allowRestart = true;
            int debuggerWasConnected = 0;
            boolean doWaitedHalfDump = false;
            final ArrayList pids;
            synchronized (mLock) {

// timeout 为 30 秒
                long timeout = CHECK_INTERVAL;
                // Make sure we (re)spin the checkers that have become idle within
                // this wait-and-check interval

// 2-1）遍历所有的 HandlerCheckers，去post消息看是否超时
                for (int i=0; i 0) {
                    debuggerWasConnected--;
                }

// 记录开始的时间
                long start = SystemClock.uptimeMillis();
                while (timeout > 0) {
                    if (Debug.isDebuggerConnected()) {
                        debuggerWasConnected = 2;
                    }
                    try {
// 等待 30 秒
                        mLock.wait(timeout);
                        // Note: mHandlerCheckers and mMonitorChecker may have changed after waiting
                    } catch (InterruptedException e) {
                        Log.wtf(TAG, e);
                    }
                    if (Debug.isDebuggerConnected()) {
                        debuggerWasConnected = 2;
                    }
// 保证执行等待30秒，然后跳出循环
                    timeout = CHECK_INTERVAL - (SystemClock.uptimeMillis() - start);
                }

// 2-2）去计算等待的状态 evaluateCheckerCompletionLocked
                final int waitState = evaluateCheckerCompletionLocked();
                if (waitState == COMPLETED) {
                    waitedHalf = false;
                    continue;
                } else if (waitState == WAITING) {
                    continue;
// 2-3）处理时间大于 30秒但是小于60秒流程
                } else if (waitState == WAITED_HALF) {
                    if (!waitedHalf) {
                        Slog.i(TAG, "WAITED_HALF");
// 则设置 waitedHalf = true
                        waitedHalf = true;
                        // We've waited half, but we'd need to do the stack trace dump w/o the lock.
                        pids = new ArrayList<>(mInterestingJavaPids);
// 设置 doWaitedHalfDump = true
                        doWaitedHalfDump = true;
                    } else {
                        continue;
                    }
                } else {
// 2-4）执行超时的流程
                    blockedCheckers = getBlockedCheckersLocked();
                    subject = describeCheckersLocked(blockedCheckers);
                    allowRestart = mAllowRestart;
                    pids = new ArrayList<>(mInterestingJavaPids);
                }
            } // END synchronized (mLock)

            if (doWaitedHalfDump) {
// 超时 30秒后，会ams先dump 消息
                ActivityManagerService.dumpStackTraces(pids, null, null,
                        getInterestingNativePids(), null, subject);
                continue;
            }

// 超时会打印下列log
            EventLog.writeEvent(EventLogTags.WATCHDOG, subject);
        public void scheduleCheckLocked() {
            if (mCompleted) {
                // Safe to update monitors in queue, Handler is not in the middle of work
                mMonitors.addAll(mMonitorQueue);
                mMonitorQueue.clear();
            }

// 当不是前台线程 FgThread，并且在轮询状态；或者调用了 pauseLocked 则直接return 出去
            if ((mMonitors.size() == 0 && mHandler.getLooper().getQueue().isPolling())
                    || (mPauseCount > 0)) {
                // Don't schedule until after resume OR
                // If the target looper has recently been polling, then
                // there is no reason to enqueue our checker on it since that
                // is as good as it not being deadlocked.  This avoid having
                // to do a context switch to check the thread. Note that we
                // only do this if we have no monitors since those would need to
                // be executed at this point.
                mCompleted = true;
                return;
            }
// mCompleted 为false 则表示在查询
            if (!mCompleted) {
                // we already have a check in flight, so no need
                return;
            }

            mCompleted = false;
            mCurrentMonitor = null;
// 设置开始调用的时间
            mStartTime = SystemClock.uptimeMillis();
// 将this 是runable，插入到消息队列的对头
            mHandler.postAtFrontOfQueue(this);
        }
```

如果消息处理的话，则会执行run 方法：

```
        @Override
        public void run() {

            final int size = mMonitors.size();
// 如果是 FgThread 则有监听持锁是否久，则会回调 monitor 方法
            for (int i = 0 ; i < size ; i++) {
                synchronized (mLock) {
                    mCurrentMonitor = mMonitors.get(i);
                }
// 可能会卡住，则不会设置 mCompleted 为 true
                mCurrentMonitor.monitor();
            }

            synchronized (mLock) {
// 执行完则会mCompleted设置为 true
                mCompleted = true;
                mCurrentMonitor = null;
            }
        }
```

> - COMPLETED = 0：等待完成；
> - WAITING = 1：等待时间小于DEFAULT_TIMEOUT的一半，即30s；
> - WAITED_HALF = 2：等待时间处于30s~60s之间；
> - OVERDUE = 3：等待时间大于或等于60s。

```
// 有下列 4 种状态
    private static final int COMPLETED = 0;
    private static final int WAITING = 1;
    private static final int WAITED_HALF = 2;
    private static final int OVERDUE = 3;

    private int evaluateCheckerCompletionLocked() {
        int state = COMPLETED;
        for (int i=0; i
// 2-3）处理时间大于 30秒但是小于60秒流程
                } else if (waitState == WAITED_HALF) {
                    if (!waitedHalf) {
                        Slog.i(TAG, "WAITED_HALF");
// 则设置 waitedHalf = true
                        waitedHalf = true;
                        // We've waited half, but we'd need to do the stack trace dump w/o the lock.
                        pids = new ArrayList<>(mInterestingJavaPids);
// 设置 doWaitedHalfDump = true
                        doWaitedHalfDump = true;
                    } else {
                        continue;
                    }
                } else {
// 执行超时的流程
                    blockedCheckers = getBlockedCheckersLocked();
                    subject = describeCheckersLocked(blockedCheckers);
                    allowRestart = mAllowRestart;
                    pids = new ArrayList<>(mInterestingJavaPids);
                }
            } // END synchronized (mLock)

            if (doWaitedHalfDump) {
// 超时 30秒后，会ams先dump 消息，然后 continue 不往下执行
// dump 出 NATIVE_STACKS_OF_INTEREST 数组中的进程信息
                ActivityManagerService.dumpStackTraces(pids, null, null,
                        getInterestingNativePids(), null, subject);
                continue;
            }
                } else if (waitState == WAITED_HALF) {
                    if (!waitedHalf) {
                        Slog.i(TAG, "WAITED_HALF");
                } else {
// 执行超时的流程
// 首先getBlockedCheckersLocked获取到执行超时的 HandlerChecker 对应的线程
                    blockedCheckers = getBlockedCheckersLocked();

// 获取超时的信息 describeCheckersLocked
                    subject = describeCheckersLocked(blockedCheckers);
// 默认是允许重启的
                    allowRestart = mAllowRestart;
                    pids = new ArrayList<>(mInterestingJavaPids);
                }
            } // END synchronized (mLock)

。。。。
// 会打印下列log
            EventLog.writeEvent(EventLogTags.WATCHDOG, subject);

            final UUID errorId;
            if (mTraceErrorLogger.isAddErrorIdEnabled()) {
                errorId = mTraceErrorLogger.generateErrorId();
                mTraceErrorLogger.addErrorIdToTrace("system_server", errorId);
            } else {
                errorId = null;
            }


            FrameworkStatsLog.write(FrameworkStatsLog.SYSTEM_SERVER_WATCHDOG_OCCURRED, subject);

            long anrTime = SystemClock.uptimeMillis();
            StringBuilder report = new StringBuilder();
            report.append(MemoryPressureUtil.currentPsiState());
            ProcessCpuTracker processCpuTracker = new ProcessCpuTracker(false);
            StringWriter tracesFileException = new StringWriter();
            final File stack = ActivityManagerService.dumpStackTraces(
                    pids, processCpuTracker, new SparseArray<>(), getInterestingNativePids(),
                    tracesFileException, subject);

            // Give some extra time to make sure the stack traces get written.
            // The system's been hanging for a minute, another second or two won't hurt much.
            SystemClock.sleep(5000);

            processCpuTracker.update();
            report.append(processCpuTracker.printCurrentState(anrTime));
            report.append(tracesFileException.getBuffer());

            // Trigger the kernel to dump all blocked threads, and backtraces on all CPUs to the kernel log
            doSysRq('w');
            doSysRq('l');

            // Try to add the error to the dropbox, but assuming that the ActivityManager
            // itself may be deadlocked.  (which has happened, causing this statement to
            // deadlock and the watchdog as a whole to be ineffective)
            Thread dropboxThread = new Thread("watchdogWriteToDropbox") {
                    public void run() {
                        // If a watched thread hangs before init() is called, we don't have a
                        // valid mActivity. So we can't log the error to dropbox.
                        if (mActivity != null) {
                            mActivity.addErrorToDropBox(
                                    "watchdog", null, "system_server", null, null, null,
                                    null, report.toString(), stack, null, null, null,
                                    errorId);
                        }
                    }
                };
            dropboxThread.start();
            try {
                dropboxThread.join(2000);  // wait up to 2 seconds for it to return.
            } catch (InterruptedException ignored) {}

            IActivityController controller;
            synchronized (mLock) {
                controller = mController;
            }
            if (controller != null) {
                Slog.i(TAG, "Reporting stuck state to activity controller");
                try {
                    Binder.setDumpDisabled("Service dumps disabled due to hung system process.");
                    // 1 = keep waiting, -1 = kill system
                    int res = controller.systemNotResponding(subject);
                    if (res >= 0) {
                        Slog.i(TAG, "Activity controller requested to coninue to wait");
                        waitedHalf = false;
                        continue;
                    }
                } catch (RemoteException e) {
                }
            }

            // Only kill the process if the debugger is not attached.
            if (Debug.isDebuggerConnected()) {
                debuggerWasConnected = 2;
            }
            if (debuggerWasConnected >= 2) {
                Slog.w(TAG, "Debugger connected: Watchdog is *not* killing the system process");
            } else if (debuggerWasConnected > 0) {
                Slog.w(TAG, "Debugger was connected: Watchdog is *not* killing the system process");
            } else if (!allowRestart) {
                Slog.w(TAG, "Restart not allowed: Watchdog is *not* killing the system process");
            } else {
                Slog.w(TAG, "*** WATCHDOG KILLING SYSTEM PROCESS: " + subject);
                WatchdogDiagnostics.diagnoseCheckers(blockedCheckers);
                Slog.w(TAG, "*** GOODBYE!");
                if (!Build.IS_USER && isCrashLoopFound()
                        && !WatchdogProperties.should_ignore_fatal_count().orElse(false)) {
                    breakCrashLoop();
                }

// 杀掉系统进程
                Process.killProcess(Process.myPid());
                System.exit(10);
            }

            waitedHalf = false;
        }
    }
// 首先getBlockedCheckersLocked获取到执行超时的 HandlerChecker 对应的线程
    private ArrayList getBlockedCheckersLocked() {
        ArrayList checkers = new ArrayList();
// 同样也是遍历所有的 mHandlerCheckers
        for (int i=0; i mStartTime + mWaitMax);
        }
// 获取超时的信息 describeCheckersLocked
    private String describeCheckersLocked(List checkers) {
        StringBuilder builder = new StringBuilder(128);
        for (int i=0; i 0) {
                builder.append(", ");
            }
            builder.append(checkers.get(i).describeBlockedStateLocked());
        }
        return builder.toString();
    }

=======
        String describeBlockedStateLocked() {
// 如果 mCurrentMonitor 为空，则表示不是锁超时出现问题，而是handler，则打印handler 信息
            if (mCurrentMonitor == null) {
                return "Blocked in handler on " + mName + " (" + getThread().getName() + ")";
            } else {

// 不为空，则是锁出现问题，只能在 FgThread 中
                return "Blocked in monitor " + mCurrentMonitor.getClass().getName()
                        + " on " + mName + " (" + getThread().getName() + ")";
            }
        }
Watchdog 检测到异常的信息收集
```

- `AMS.dumpStackTraces：输出Java和Native进程的栈信息`
- `doSysRq`
- `dropBox`

```
收集完信息后便会杀死 system_server 进程。此处 allowRestart 默认值为 true，当执行 am hang 操作则设置不允许重启 (allowRestart =false), 则不会杀死 system_server 进程。
```