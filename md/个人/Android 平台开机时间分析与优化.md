

*阿豪* *12/27/2023*

## [#](#_1-开机时间的分析方法) 1. 开机时间的分析方法

### [#](#_1-1-通过日志信息获取开机耗时信息) 1.1 通过日志信息获取开机耗时信息

Android 系统把 Log 分为了四类，用于记录不同的 Log 信息：

+   main：应用层 App Log
+   events：系统事件相关的 Log 信息
+   radio：无线/电话相关的 Log 信息
+   system：低级别的系统调试 Log 信息

默认通过 logcat 抓取的是 main 信息，我们可以通过 -b 选项来指定日志的类别。开机的关键节点日志通过 events 日志记录，我们可以通过如下命令获取到：

```bash
adb logcat -d -b "events" | grep "boot_progress"

12-23 11:03:11.541  1502  1502 I boot_progress_start: 4134
12-23 11:03:11.699  1502  1502 I boot_progress_preload_start: 4291
12-23 11:03:12.025  1502  1502 I boot_progress_preload_end: 4617
12-23 11:03:12.173  1610  1610 I boot_progress_system_run: 4765
12-23 11:03:12.459  1610  1610 I boot_progress_pms_start: 5052
12-23 11:03:12.507  1610  1610 I boot_progress_pms_system_scan_start: 5099
12-23 11:03:12.751  1610  1610 I boot_progress_pms_data_scan_start: 5343
12-23 11:03:12.752  1610  1610 I boot_progress_pms_scan_end: 5345
12-23 11:03:12.799  1610  1610 I boot_progress_pms_ready: 5391
12-23 11:03:13.251  1610  1610 I boot_progress_ams_ready: 5843
12-23 11:03:13.677  1610  1695 I boot_progress_enable_screen: 6269

adb logcat -d -b "events" | grep "sf_stop_bootanim"
12-23 11:03:15.260  1553  1653 I sf_stop_bootanim: 7852

adb logcat -d -b "events" | grep "wm_boot_animation_done"
12-23 11:03:15.260  1610  1695 I wm_boot_animation_done: 7852
```



每条数据的最后是距离开机时的事件，倒数第二个数据是关键节点的名字：

+   boot\_progress\_start：系统进入用户空间，标志着 kernel 启动完成
+   boot\_progress\_preload\_start：Zygote preload 过程启动
+   boot\_progress\_preload\_end：Zygote preload 过程结束
+   boot\_progress\_system\_run：开始启动 Android 系统服务
+   boot\_progress\_pms\_start：PMS 开始扫描安装的应用
+   boot\_progress\_pms\_system\_scan\_start：PMS 先行扫描 /system 目录下的安装包
+   boot\_progress\_pms\_data\_scan\_start：PMS 扫描 /data 目录下的安装包
+   boot\_progress\_pms\_scan\_end：PMS 扫描结束
+   boot\_progress\_pms\_ready：PMS 就绪
+   boot\_progress\_ams\_ready：AMS 就绪
+   boot\_progress\_enable\_screen：AMS 启动完成后开始激活屏幕，从此以后屏幕才能响应用户的触摸，它在 WindowManagerService 发出退出开机动画的时间节点之前
+   sf\_stop\_bootanim：surfaceflinger 设置 service.bootanim.exit 属性值为 1，标志系统要结束开机动画了
+   wm\_boot\_animation\_done：开机动画结束，这一步用户能直观感受到开机结束

补充说明下开机动画关闭的时机：

Launcher 启动完之后，我们还看不到 Launcher，因为被 BootAnimation 的画面挡住了。BootAnimation　的退出也比较复杂，大概是第一个 Launcher 起来之后，其 ActivityThread 线程进入空闲状态时(使用 IdleHandler)，把 BootAnimation 给退出。这样就能确保在 BootAnimation 退出后，用户看到的不是黑屏，而是我们的桌面了。

这些时间通常使用 excel 或者 wps 表格制作成折线图更为直观：

![](https://cdn.jsdelivr.net/gh/stingerzou/MyImages//20231225215856.png)

以上特定日志的分析，粒度较大，只能定位出大概的耗时流程，之后还需分析流程内部具体的耗时情况。开机各流程内部也有相应的日志，可以进行更加细致的分析。例如在 SystemServiceManager 类中启动服务时，会打印启动某项服务的日志。通过查看某个服务 A 与下一个服务的日志时间，可以计算出启动服务A的耗时。

```bash
adb logcat -d | grep "SystemService"          
12-23 11:03:12.276  1610  1610 I SystemServiceManager: Starting com.android.server.pm.Installer
12-23 11:03:12.278  1610  1610 I SystemServiceManager: Starting com.android.server.os.DeviceIdentifiersPolicyService
12-23 11:03:12.278  1610  1610 I SystemServiceManager: Starting com.android.server.uri.UriGrantsManagerService$Lifecycle
12-23 11:03:12.280  1610  1610 I SystemServiceManager: Starting com.android.server.wm.ActivityTaskManagerService$Lifecycle
12-23 11:03:12.282  1610  1610 I SystemServiceManager: Starting com.android.server.am.ActivityManagerService$Lifecycle
12-23 11:03:12.339  1610  1610 I SystemServiceManager: Starting com.android.server.power.PowerManagerService
12-23 11:03:12.347  1610  1610 I SystemServiceManager: Starting com.android.server.power.ThermalManagerService
12-23 11:03:12.347  1610  1610 I SystemServer: StartRecoverySystemService
12-23 11:03:12.348  1610  1610 I SystemServiceManager: Starting com.android.server.RecoverySystemService
12-23 11:03:12.348  1610  1610 D SystemServerTiming: StartRecoverySystemService took to complete: 0ms
12-23 11:03:12.348  1610  1610 I SystemServiceManager: Starting com.android.server.lights.LightsService
12-23 11:03:12.450  1610  1610 I SystemServiceManager: Starting com.android.server.display.DisplayManagerService
KeyChainSystemService

# ......
```



### [#](#_1-2-使用-bootchart-分析开机启动时间) 1.2 使用 bootchart 分析开机启动时间

bootchart 是一个能对 GNU/Linux boot 过程进行性能分析并把结果直观化的开源工具，在系统启动过程中自动收集 CPU 占用率、磁盘吞吐率、进程等信息，并以图形方式显示分析结果，可用作指导优化系统启动过程。BootChart 包含数据收集工具和图像产生工具，数据收集工具在原始的 BootChart 中是独立的 shell 程序，但在 Android 中，数据收集工具被集成到了 init 程序中。

在 Android 中会通过一个叫 `do_bootchart_start` 的函数来判断是否抓取 bootchar 数据

```cpp
// system/core/init/bootchart.cpp
static Result<Success> do_bootchart_start() {
    // We don't care about the content, but we do care that /data/bootchart/enabled actually exists.
    std::string start;
     // 只要存在/data/bootchart/enabled文件，即抓取 bootchart 数据
    if (!android::base::ReadFileToString("/data/bootchart/enabled", &start)) {
        LOG(VERBOSE) << "Not bootcharting";
        return Success();
    }

    g_bootcharting_thread = new std::thread(bootchart_thread_main);
    return Success();
}
```


所以我们只要创建一个 `/data/bootchart/enabled` 即可开启 bootchar 数据的抓取。

```bash
adb shell touch /data/bootchart/enabled
adb reboot
```



接着我们准备用于生成 bootchart 图的分析软件 pybootchartgui：

```bash
# 下载 pybootchartgui
sudo apt install python-is-python3
cd ~/Documents
git clone https://github.com/xrmx/bootchart.git
cd bootchart/pybootchartgui
mv main.py.in main.py
# 建立
sudo ln -s ~/Documents/bootchart/pybootchartgui.py /usr/bin/pybootchartgui
```


回到 Android 源码目录下执行：

```bash
system/core/init/grab-bootchart.sh

parsing '/tmp/android-bootchart/bootchart.tgz'
parsing 'header'
parsing 'proc_stat.log'
parsing 'proc_ps.log'
parsing 'proc_diskstats.log'
merged 0 logger processes
pruned 47 process, 0 exploders, 2 threads, and 1 runs
bootchart written to 'bootchart.png'
Clean up /tmp/android-bootchart/ and ./bootchart.png when done
```



在源码目录下就生成了 `bootchart.png` 图：

![](https://cdn.jsdelivr.net/gh/stingerzou/MyImages//bootchart.png)

从生成的图片可以更加直观详细的看到开机耗时以及硬件使用情况.

### [#](#_1-3-抓取-trace-文件分析开机启动时间) 1.3 抓取 trace 文件分析开机启动时间

修改 `frameworks/native/cmds/atrace/atrace.rc` 中 `service boottrace` 的内容：

```bash
service boottrace /system/bin/atrace --async_start -b 30720 gfx input view webview wm am sm audio video binder_lock binder_driver camera hal res dalvik rs bionic power pm ss database network adb vibrator aidl sched  
    disabled
    oneshot
```

 

接着重新编译源码，启动虚拟机。

打开抓取 boottrace 的属性开关

```bash
adb  shell setprop persist.debug.atrace.boottrace 1
```



手机启动完成之后等待几秒，关闭 boottrace 属性开关

```bash
adb  shell setprop persist.debug.atrace.boottrace 0
```



生成和拉取 boottrace 文件

```bash
adb shell atrace --async_stop -z -c -o /data/local/tmp/boot_trace
adb  pull /data/local/tmp/boot_trace
```

 

自此，我们就得到了启动阶段的 trace 文件，可以通过 perfetto 工具打开并分析它了。

## [#](#_2-开机启动优化) 2. 开机启动优化

开启启动优化可以分为两类：

+   程序异常，导致开机时间加长或无法开机
+   正常开机时间的基础上加快开机速度

第一种情况，很多时候是某个模块的魔改导致的，我们的主要任务是找到这个模块，一般通过上一节介绍的三种开机时间分析的手段就可以很容易的找到了。

更多的时候是第二种情况，Android 启动可以优化的阶段主要有：

+   Bootloader
+   Kernel
+   Framework

我们主要关注 Framework 阶段的优化，常见的优化手段有：

**在 Zygote 进程启动后，加载自定义的驱动，启动自定义的进程**

zygote 是在 late-init 阶段启动

```bash
on late-init
    trigger early-fs
    trigger fs
    trigger post-fs
    trigger late-fs
    trigger post-fs-data
    trigger load_persist_props_action
    # zygote 启动
    trigger zygote-start
    trigger firmware_mounts_complete
    trigger early-boot
    trigger boot
```

  

加载自定义的驱动，启动自定义的进程两类操作，我们可以放到 early-boot 或者 boot 阶段启动，这样可以让 Zygote 进程尽早的启动，加快系统的启动。

```bash
on boot
    insmod /vendor/lib/modules/btusb.ko
    start xxxx
    # .....
```



**调整 Zygote 启动参数**

```bash
service zygote /system/bin/app_process -Xzygote /system/bin --zygote --start-system-server --enable-lazy-preload
    class main
    priority -20
    user root
    group root readproc reserved_disk
    socket zygote stream 660 root system
    socket usap_pool_primary stream 660 root system
    onrestart exec_background - system system -- /system/bin/vdc volume abort_fuse
    onrestart write /sys/power/state on
    onrestart restart audioserver
    onrestart restart cameraserver
    onrestart restart media
    onrestart restart netd
    onrestart restart wificond
    task_profiles ProcessCapacityHigh MaxPerformance
```
 

这里改动了两个地方 `--enable-lazy-preload`，`task_profiles ProcessCapacityHigh MaxPerformance`。

添加了 `--enable-lazy-preload` 参数，在开机阶段不会执行 `preload` 预加载操作。在开机后，通过 system-server 发送指令给 zygote 做资源加载操作，在发送指令前，system-server 会加载一部分自己使用的类，会和 zygote 中存在相同的一部分备份，所有会多耗费一些内存。

`task_profiles ProcessCapacityHigh MaxPerformance` 会让内核使用尽可能多的资源来启动 Zygote。

**开机阶段 CPU 开启性能模式**

```bash
# cpu 开启性能模式
on early-init
    write /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor performance

# 开机完成后，CPU 频率变成自适应
on property:sys.boot_completed=1
    write /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor schedutil
```



**读写 IO 调整**

可以通过调整事先预读数据的 Kb 数以及默认 IO 请求队列的长度来加快 IO 的读写以提高开机速度

```bash
# on late-fs
#     # boot time fs tune
#     write /sys/block/mmcblk0/queue/iostats 0
#     # 增大事先预读数据的 Kb 数
#     write /sys/block/mmcblk0/queue/read_ahead_kb 2048
#     # 默认 IO 请求队列的长度
#     write /sys/block/mmcblk0/queue/nr_requests 256

# on property:sys.boot_completed=1
#     # end boot time fs tune
#     write /sys/block/mmcblk0/queue/read_ahead_kb 128

on late-fs
  # boot time fs tune
    write /sys/block/sda/queue/iostats 0
    write /sys/block/sda/queue/scheduler cfq
    write /sys/block/sda/queue/iosched/slice_idle 0
    # 增大事先预读数据的 Kb 数
    write /sys/block/sda/queue/read_ahead_kb 2048
    # IO 请求队列的长度
    write /sys/block/sda/queue/nr_requests 256
    write /sys/block/dm-0/queue/read_ahead_kb 2048
    write /sys/block/dm-1/queue/read_ahead_kb 2048

on property:sys.boot_completed=1
    # end boot time fs tune
    write /sys/block/sda/queue/read_ahead_kb 512
```



**移除没有用的模块**

主要是修改 SystemServer，删除一些用不到的服务,比如有的产品是电视、音响，就不会用到电话，指纹、定位、打印等相关的模块。

一般可裁剪的模块有：

```bash
TelecomLoaderService
TelephonyRegistry
StatusBarManagerService
SearchManagerService
SerialService
FingerprintService
CameraService
MmsService
```



**延时启动 persist app**

修改 `startPersistentApps` 方法，延时启动 persist app

```java
void startPersistentApps(int matchFlags) {
    if (mFactoryTest == FactoryTest.FACTORY_TEST_LOW_LEVEL) return;

    synchronized (this) {
        try {
            final List<ApplicationInfo> apps = AppGlobals.getPackageManager()
                    .getPersistentApplications(STOCK_PM_FLAGS | matchFlags).getList();
            for (ApplicationInfo app : apps) {
                if (!"android".equals(app.packageName)) {
                    mHandler.postDelayed(() -> {
                        addAppLocked(app, null, false, null /* ABI override */,
                                ZYGOTE_POLICY_FLAG_BATCH_LAUNCH);
                    }, 1000);
                }
            }
        } catch (RemoteException ex) {
        }
    }
}
```



**精简 preload 的 classes**

可以根据产品的类型修改 `frameworks/base/config/preloaded-classes` 文件，来删减一些用不到的 preload classes

常见可删除的预加载 classes 有：

```bash
// 生物识别
android.hardware.biometrics

// 人脸识别
android.hardware.face

// 打印服务
android.hardware.fingerprint
android.print.

// 部分定位相关, 还有GPS定位相关
android.hardware.location
com.android.internal.location.GpsNetInitiatedHandler
android.location.Gnss*

// 手机通话相关
android.telephony.
android.telecom.
com.android.i18n.phonenumbers.
com.android.ims
android.hardware.radio

// nfc相关
android.nfc.
```



如果需要再做一些大的裁剪，可以使用 `frameworks\base\config\generate-preloaded-classes.sh` 脚本来重新生成 preloaded-classes

## [#](#参考资料) 参考资料

+   [Android开机全解析 (opens new window)](https://www.jianshu.com/p/3f23e027b591)
+   [Android开机启动优化 (opens new window)](https://0uyangsheng.github.io/2018/05/14/Android%20boot%20optimization/)
+   [优化启动时间 (opens new window)](https://source.android.com/docs/core/perf/boot-times?hl=zh-cn)
+   [Android TV开机优化 (opens new window)](https://blog.csdn.net/he980725866/article/details/123030906)