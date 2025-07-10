---
created: 2025-07-10T13:44:21 (UTC +08:00)
tags: [timingstraceandslog]
source: https://blog.csdn.net/qq_40731414/article/details/130656131?spm=1001.2014.3001.5502
author: 成就一亿技术人!
---

# 开机日志分析积累_timingstraceandslog-CSDN博客

> ## Excerpt
> 文章浏览阅读791次。开机阶段日志_timingstraceandslog

---
## 开机日志分析积累

> hongxi.zhu 2022-11-20

### 1\. 开机各个阶段的日志和描述

`logcat -b all` 抓取所有类型开机日志，从下面的阶段event log可以了解每个阶段的耗时,方便我们定位开机性能的问题。  
例子：

```
Line    62: 11-10 23:03:54.396   792   792 I boot_progress_start: 3632
Line    71: 11-10 23:03:54.953   792   792 I boot_progress_preload_start: 4189
Line    88: 11-10 23:03:56.254   792   792 I boot_progress_preload_end: 5489
Line    92: 11-10 23:03:56.642  1367  1367 I boot_progress_system_run: 5878
Line   559: 11-10 23:03:57.485  1367  1367 I boot_progress_pms_start: 6721
Line   872: 11-10 23:03:57.663  1367  1367 I boot_progress_pms_system_scan_start: 6898
Line   894: 11-10 23:03:57.938  1367  1367 I boot_progress_pms_data_scan_start: 7174
Line   900: 11-10 23:03:57.953  1367  1367 I boot_progress_pms_scan_end: 7189
Line   929: 11-10 23:03:58.139  1367  1367 I boot_progress_pms_ready: 7375
Line  2021: 11-10 23:03:59.639  1367  1367 I boot_progress_ams_ready: 8875
Line  3361: 11-10 23:04:01.016  1367  1533 I boot_progress_enable_screen: 10251
Line  5251: 11-10 23:04:03.237   644   675 I sf_stop_bootanim: 12473
Line  5252: 11-10 23:04:03.238  1367  1533 I wm_boot_animation_done: 12473
```

| log tag | desc |
| --- | --- |
| boot\_progress\_start | 系统进入用户空间，标志着kernel启动完成 |
| boot\_progress\_preload\_start | Zygote启动 |
| boot\_progress\_preload\_end | Zygote结束 |
| boot\_progress\_system\_run | SystemServer ready,开始启动Android系统服务 |
| boot\_progress\_pms\_start | PMS开始扫描安装的应用 |
| boot\_progress\_pms\_system\_scan\_start | PMS先行扫描/system目录下的安装包 |
| boot\_progress\_pms\_data\_scan\_start | PMS扫描/data目录下的安装包 |
| boot\_progress\_pms\_scan\_end | PMS扫描结束 |
| boot\_progress\_pms\_ready | PMS就绪 |
| boot\_progress\_ams\_ready | AMS就绪 |
| boot\_progress\_enable\_screen | AMS启动完成后开始激活屏幕，从此以后屏幕才能响应用户的触摸，它在WindowManagerService发出退出开机动画的时间节点之前 |
| sf\_stop\_bootanim | SF设置service.bootanim.exit属性值为1，标志系统要结束开机动画了 |
| wm\_boot\_animation\_done | 开机动画结束，这一步用户能直观感受到开机结束 |

从Android 12源码中找到上述的节点log位置：

1.  boot\_progress\_start

```
# frameworks/base/core/jni/AndroidRuntime.cpp:1208

void AndroidRuntime::start(const char* className, const Vector<String8>& options, bool zygote)
{
    ALOGD(">>>>>> START %s uid %d <<<<<<\n",
            className != NULL ? className : "(unknown)", getuid());

    static const String8 startSystemServer("start-system-server");
    // Whether this is the primary zygote, meaning the zygote which will fork system server.
    bool primary_zygote = false;

    /*
     * 'startSystemServer == true' means runtime is obsolete and not run from
     * init.rc anymore, so we print out the boot start event here.
     */
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == startSystemServer) {
            primary_zygote = true;
           /* track our progress through the boot sequence */
           const int LOG_BOOT_PROGRESS_START = 3000;
           LOG_EVENT_LONG(LOG_BOOT_PROGRESS_START,  ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));  //boot_progress_start
        }
    }
    ...
```

2.  boot\_progress\_preload\_start

```
frameworks/base/core/java/com/android/internal/os/ZygoteInit.java:954

   public static void main(String[] argv) {
            ...
            // In some configurations, we avoid preloading resources and classes eagerly.
            // In such cases, we will preload things prior to our first fork.
            if (!enableLazyPreload) {
                bootTimingsTraceLog.traceBegin("ZygotePreload");
                EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_START,
                        SystemClock.uptimeMillis());  //boot_progress_preload_start
                preload(bootTimingsTraceLog);
                EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_END,
                        SystemClock.uptimeMillis());
                bootTimingsTraceLog.traceEnd(); // ZygotePreload
            }
            ...
```

3.  boot\_progress\_preload\_end

```
frameworks/base/core/java/com/android/internal/os/ZygoteInit.java:957

   public static void main(String[] argv) {
            ...
            // In some configurations, we avoid preloading resources and classes eagerly.
            // In such cases, we will preload things prior to our first fork.
            if (!enableLazyPreload) {
                bootTimingsTraceLog.traceBegin("ZygotePreload");
                EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_START,
                        SystemClock.uptimeMillis());
                preload(bootTimingsTraceLog);
                EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_END,
                        SystemClock.uptimeMillis());  //boot_progress_preload_end
                bootTimingsTraceLog.traceEnd(); // ZygotePreload
            }
            ...
```

4.  boot\_progress\_system\_run

```
frameworks/base/services/java/com/android/server/SystemServer.java:759

        private void run() {
        TimingsTraceAndSlog t = new TimingsTraceAndSlog();
        try {
            t.traceBegin("InitBeforeStartServices");
            ...
            // Here we go!
            Slog.i(TAG, "Entered the Android system server!");
            final long uptimeMillis = SystemClock.elapsedRealtime();
            EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_SYSTEM_RUN, uptimeMillis);  //boot_progress_system_run
            if (!mRuntimeRestart) {
                FrameworkStatsLog.write(FrameworkStatsLog.BOOT_TIME_EVENT_ELAPSED_TIME_REPORTED,
                        FrameworkStatsLog
                                .BOOT_TIME_EVENT_ELAPSED_TIME__EVENT__SYSTEM_SERVER_INIT_START,
                        uptimeMillis);
            }
            ...
```

5.  boot\_progress\_pms\_start

```
# frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java:7386

    public PackageManagerService(Injector injector, boolean onlyCore, boolean factoryTest,
            final String buildFingerprint, final boolean isEngBuild,
            final boolean isUserDebugBuild, final int sdkVersion, final String incrementalVersion) {
        mIsEngBuild = isEngBuild;
        mIsUserDebugBuild = isUserDebugBuild;
        mSdkVersion = sdkVersion;
        mIncrementalVersion = incrementalVersion;
        mInjector = injector;
        mInjector.getSystemWrapper().disablePackageCaches();

        final TimingsTraceAndSlog t = new TimingsTraceAndSlog(TAG + "Timing",
                Trace.TRACE_TAG_PACKAGE_MANAGER);
        mPendingBroadcasts = new PendingPackageBroadcasts();

        mInjector.bootstrap(this);
        mLock = injector.getLock();
        mInstallLock = injector.getInstallLock();
        LockGuard.installLock(mLock, LockGuard.INDEX_PACKAGES);
        EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_START,
                SystemClock.uptimeMillis());  //boot_progress_pms_start
        ...
```

6.  boot\_progress\_pms\_system\_scan\_start

```
# frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java:7587
    public PackageManagerService(Injector injector, boolean onlyCore, boolean factoryTest,
            final String buildFingerprint, final boolean isEngBuild,
            final boolean isUserDebugBuild, final int sdkVersion, final String incrementalVersion) {
            ...
            EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_SYSTEM_SCAN_START,
                    startTime);  //boot_progress_pms_system_scan_start
            ...
            for (int i = mDirsToScanAsSystem.size() - 1; i >= 0; i--) {
                final ScanPartition partition = mDirsToScanAsSystem.get(i);
                if (partition.getOverlayFolder() == null) {
                    continue;
                }
                scanDirTracedLI(partition.getOverlayFolder(), systemParseFlags,
                        systemScanFlags | partition.scanFlag, 0,
                        packageParser, executorService);
            }
            ...
```

7.  boot\_progress\_pms\_data\_scan\_start

```
# frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java:7790
            ...
            final long systemScanTime = SystemClock.uptimeMillis() - startTime;
            final int systemPackagesCount = mPackages.size();
            Slog.i(TAG, "Finished scanning system apps. Time: " + systemScanTime
                    + " ms, packageCount: " + systemPackagesCount
                    + " , timePerPackage: "
                    + (systemPackagesCount == 0 ? 0 : systemScanTime / systemPackagesCount)
                    + " , cached: " + cachedSystemApps);
            if (mIsUpgrade && systemPackagesCount > 0) {
                //CHECKSTYLE:OFF IndentationCheck
                FrameworkStatsLog.write(FrameworkStatsLog.BOOT_TIME_EVENT_DURATION_REPORTED,
                    BOOT_TIME_EVENT_DURATION__EVENT__OTA_PACKAGE_MANAGER_SYSTEM_APP_AVG_SCAN_TIME,
                    systemScanTime / systemPackagesCount);
                //CHECKSTYLE:ON IndentationCheck
            }
            if (!mOnlyCore) {
                EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_DATA_SCAN_START,
                        SystemClock.uptimeMillis());  // boot_progress_pms_data_scan_start
                scanDirTracedLI(mAppInstallDir, 0, scanFlags | SCAN_REQUIRE_KNOWN, 0,
                        packageParser, executorService);

            }
            ...
```

8.  boot\_progress\_pms\_scan\_end

```
# frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java:7982

            ...
            // Now that we know all the packages we are keeping,
            // read and update their last usage times.
            mPackageUsage.read(packageSettings);
            mCompilerStats.read();

            EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_SCAN_END,
                    SystemClock.uptimeMillis()); //boot_progress_pms_scan_end
            Slog.i(TAG, "Time to scan packages: "
                    + ((SystemClock.uptimeMillis()-startTime)/1000f)
                    + " seconds");
            ...
```

9.  boot\_progress\_pms\_ready

```
# frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java:8121

            ...
            // can downgrade to reader
            t.traceBegin("write settings");
            writeSettingsLPrTEMP();
            t.traceEnd();
            EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_READY,
                    SystemClock.uptimeMillis());  //boot_progress_pms_ready
            ...

```

10.  boot\_progress\_ams\_ready

```
# frameworksbase/services/core/java/com/android/server/am/ActivityManagerService.java:7612

    /**
     * Ready. Set. Go!
     */
    public void systemReady(final Runnable goingCallback, @NonNull TimingsTraceAndSlog t) {
        t.traceBegin("PhaseActivityManagerReady");
        mSystemServiceManager.preSystemReady();
        ...
        Slog.i(TAG, "System now ready");

        EventLogTags.writeBootProgressAmsReady(SystemClock.uptimeMillis());  //boot_progress_ams_ready
        ...
```

11.  boot\_progress\_enable\_screen

```
# framework/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java:5620

        ...
        @Override
        public void enableScreenAfterBoot(boolean booted) {
            writeBootProgressEnableScreen(SystemClock.uptimeMillis());  //boot_progress_enable_screen
            mWindowManager.enableScreenAfterBoot();
            synchronized (mGlobalLock) {
                updateEventDispatchingLocked(booted);
            }
        }

```

12.  sf\_stop\_bootanim

```
# framework/native/services/surfaceflinger/SurfaceFlinger.cpp:750

void SurfaceFlinger::bootFinished() {
    ...
    // stop boot animation
    // formerly we would just kill the process, but we now ask it to exit so it
    // can choose where to stop the animation.
    property_set("service.bootanim.exit", "1");

    const int LOGTAG_SF_STOP_BOOTANIM = 60110;
    LOG_EVENT_LONG(LOGTAG_SF_STOP_BOOTANIM,
                   ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));  //sf_stop_bootanim
    ...
```

13.  wm\_boot\_animation\_done

```
# framework/base/services/core/java/com/android/server/wm/WindowManagerService.java:3691

    private void performEnableScreen() {
            ...
            try {
                IBinder surfaceFlinger = ServiceManager.getService("SurfaceFlinger");
                if (surfaceFlinger != null) {
                    ProtoLog.i(WM_ERROR, "******* TELLING SURFACE FLINGER WE ARE BOOTED!");
                    Parcel data = Parcel.obtain();
                    data.writeInterfaceToken("android.ui.ISurfaceComposer");
                    surfaceFlinger.transact(IBinder.FIRST_CALL_TRANSACTION, // BOOT_FINISHED
                            data, null, 0);
                    data.recycle();
                }
            } catch (RemoteException ex) {
                ProtoLog.e(WM_ERROR, "Boot completed: SurfaceFlinger is dead!");
            }

            EventLogTags.writeWmBootAnimationDone(SystemClock.uptimeMillis());  //wm_boot_animation_done
            Trace.asyncTraceEnd(TRACE_TAG_WINDOW_MANAGER, "Stop bootanim", 0);
            mDisplayEnabled = true;
            ProtoLog.i(WM_DEBUG_SCREEN_ON, "******************** ENABLING SCREEN!");

            // Enable input dispatch.
            mInputManagerCallback.setEventDispatchingLw(mEventDispatchingEnabled);
        }

        try {
            mActivityManager.bootAnimationComplete();
        } catch (RemoteException e) {
        }

        mPolicy.enableScreenAfterBoot();

        // Make sure the last requested orientation has been applied.
        updateRotationUnchecked(false, false);
    }
```
