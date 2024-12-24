**阅读目录**

-   [概述](https://www.cnblogs.com/linhaostudy/p/18611180#_label0)
-   [run\_init\_process](https://www.cnblogs.com/linhaostudy/p/18611180#_label1)
-   [main](https://www.cnblogs.com/linhaostudy/p/18611180#_label2)
-   [FirstStageMain](https://www.cnblogs.com/linhaostudy/p/18611180#_label3)
    -   [InstallRebootSignalHandlers](https://www.cnblogs.com/linhaostudy/p/18611180#_label3_0)
    -   [InitFatalReboot](https://www.cnblogs.com/linhaostudy/p/18611180#_label3_1)
    -   [DoFirstStageMount](https://www.cnblogs.com/linhaostudy/p/18611180#_label3_2)
-   [SetupSelinux](https://www.cnblogs.com/linhaostudy/p/18611180#_label4)
-   [SecondStageMain](https://www.cnblogs.com/linhaostudy/p/18611180#_label5)
    -   [PropertyInit](https://www.cnblogs.com/linhaostudy/p/18611180#_label5_0)
    -   [StartPropertyService](https://www.cnblogs.com/linhaostudy/p/18611180#_label5_1)
    -   [LoadBootScripts](https://www.cnblogs.com/linhaostudy/p/18611180#_label5_2)
    -   [添加内置动作和事件触发器](https://www.cnblogs.com/linhaostudy/p/18611180#_label5_3)
    -   [SecondStageMain 循环处理事件](https://www.cnblogs.com/linhaostudy/p/18611180#_label5_4)
    -   [内置action和触发器执行](https://www.cnblogs.com/linhaostudy/p/18611180#_label5_5)
    -   [trigger zygote-start](https://www.cnblogs.com/linhaostudy/p/18611180#_label5_6)
    -   [trigger boot](https://www.cnblogs.com/linhaostudy/p/18611180#_label5_7)
-   [总结](https://www.cnblogs.com/linhaostudy/p/18611180#_label6)

**正文**

## 概述

init是 Android 启动的第一个用户空间进程，它的地位非常重要，它fork产生系统的一些关键进程(如zygote，surfaceflinger进程)，而zygote进一步fork产生system\_server和其他应用进程，通过这套逻辑构建了Android的进程层次结构体系。init进程的功能包含但不限于以下：

-   挂载系统分区和加载一些内核模块
-   加载sepolicy 及使能 selinux
-   支持属性服务
-   启动脚本rc文件解析
-   执行事件触发器和属性改变的事件
-   子进程死亡监听，回收僵尸进程
-   非oneshot服务保活

通过[ps命令](https://so.csdn.net/so/search?q=ps%E5%91%BD%E4%BB%A4&spm=1001.2101.3001.7020)看看init进程信息

```
# ps -A|grep init                                                                                                           
root             1     0 10847128  4020 do_epoll_wait       0 S init  # 这个是 init 进程
root           166     1 10817360  1916 do_sys_poll         0 S init  # 这个是 subcontext 进程
```

在启动内核的start\_kernel[函数](https://marketing.csdn.net/p/3127db09a98e0723b83b2914d9256174?pId=2782&utm_source=glcblog&spm=1001.2101.3001.7020)流程中，会调用run\_init\_process函数执行init程序，来启动init进程

## run\_init\_process

在Android中执行的init是/init

```
/// @kernel_common/init/main.c
static int run_init_process(const char *init_filename)
{
argv_init[0] = init_filename;
pr_info("Run %s as init process\n", init_filename);
return do_execve(getname_kernel(init_filename),
(const char __user *const __user *)argv_init,
(const char __user *const __user *)envp_init);
}
```

/init 实际上是一个软链接，指向的是/system/bin/init

```
# ls /init -lZ                                                                                           
lrwxr-x--- 1 root shell u:object_r:init_exec:s0  16 2021-12-20 15:52 /init -> /system/bin/init
```

接下来，进入init的main函数。

## main

main执行分为几个阶段：

-   FirstStage 挂载一些基础文件系统和加载内核模块等
-   selinux\_setup 执行selinux的初始化
-   SecondStage 挂载其他文件系统，启动属性服务，执行boot流程等，主要逻辑都在这里实现

```
/// @system/core/init/main.cpp
int main(int argc, char** argv) {
#if __has_feature(address_sanitizer)
    __asan_set_error_report_callback(AsanReportCallback);
#endif
    // Boost prio which will be restored later
    setpriority(PRIO_PROCESS, 0, -20);
    if (!strcmp(basename(argv[0]), "ueventd")) { // 处理uventd启动，共用一个main
        return ueventd_main(argc, argv);
    }

    if (argc > 1) {
        if (!strcmp(argv[1], "subcontext")) { // subcontext 子进程入口，用于执行来自init的某些任务
            android::base::InitLogging(argv, &android::base::KernelLogger);
            const BuiltinFunctionMap& function_map = GetBuiltinFunctionMap();

            return SubcontextMain(argc, argv, &function_map);
        }

        if (!strcmp(argv[1], "selinux_setup")) {// selinux初始化阶段
            return SetupSelinux(argv);
        }

        if (!strcmp(argv[1], "second_stage")) {// 启动第二阶段
            return SecondStageMain(argc, argv);
        }
    }

    return FirstStageMain(argc, argv); // 启动第一阶段
}

```

## FirstStageMain

第一阶段初始化

```
/// @system/core/init/first_stage_init.cpp
int FirstStageMain(int argc, char** argv) {
    if (REBOOT_BOOTLOADER_ON_PANIC) {// 设置panic处理器
        InstallRebootSignalHandlers();
    }

    boot_clock::time_point start_time = boot_clock::now();

    std::vector<std::pair<std::string, int>> errors;
#define CHECKCALL(x) \
    if ((x) != 0) errors.emplace_back(#x " failed", errno);

    // Clear the umask.
    umask(0);

    CHECKCALL(clearenv());
    CHECKCALL(setenv("PATH", _PATH_DEFPATH, 1));
    // 挂载一些基础文件系统
    // Get the basic filesystem setup we need put together in the initramdisk
    // on / and then we'll let the rc file figure out the rest.
    CHECKCALL(mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755"));
    CHECKCALL(mkdir("/dev/pts", 0755));
    CHECKCALL(mkdir("/dev/socket", 0755));
    CHECKCALL(mkdir("/dev/dm-user", 0755));
    CHECKCALL(mount("devpts", "/dev/pts", "devpts", 0, NULL));
#define MAKE_STR(x) __STRING(x)
    // /proc 伪文件系统，记录进程、线程相关实时状态
    CHECKCALL(mount("proc", "/proc", "proc", 0, "hidepid=2,gid=" MAKE_STR(AID_READPROC)));
#undef MAKE_STR
    // Don't expose the raw commandline to unprivileged processes.
    CHECKCALL(chmod("/proc/cmdline", 0440)); // 只读
    std::string cmdline;
    android::base::ReadFileToString("/proc/cmdline", &cmdline);
    // Don't expose the raw bootconfig to unprivileged processes.
    chmod("/proc/bootconfig", 0440);
    std::string bootconfig;
    android::base::ReadFileToString("/proc/bootconfig", &bootconfig);
    gid_t groups[] = {AID_READPROC};
    CHECKCALL(setgroups(arraysize(groups), groups));
    CHECKCALL(mount("sysfs", "/sys", "sysfs", 0, NULL));
    CHECKCALL(mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL));

    CHECKCALL(mknod("/dev/kmsg", S_IFCHR | 0600, makedev(1, 11)));

    if constexpr (WORLD_WRITABLE_KMSG) {
        CHECKCALL(mknod("/dev/kmsg_debug", S_IFCHR | 0622, makedev(1, 11)));
    }

    CHECKCALL(mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8)));
    CHECKCALL(mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9)));

    // This is needed for log wrapper, which gets called before ueventd runs.
    CHECKCALL(mknod("/dev/ptmx", S_IFCHR | 0666, makedev(5, 2)));
    CHECKCALL(mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3)));

    // 重要的在第一阶段挂载，其他可以在rc执行流程中挂载
    // These below mounts are done in first stage init so that first stage mount can mount
    // subdirectories of /mnt/{vendor,product}/.  Other mounts, not required by first stage mount,
    // should be done in rc files.
    // Mount staging areas for devices managed by vold
    // See storage config details at http://source.android.com/devices/storage/
    CHECKCALL(mount("tmpfs", "/mnt", "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV,
                    "mode=0755,uid=0,gid=1000"));
    // /mnt/vendor is used to mount vendor-specific partitions that can not be
    // part of the vendor partition, e.g. because they are mounted read-write.
    CHECKCALL(mkdir("/mnt/vendor", 0755));
    // /mnt/product is used to mount product-specific partitions that can not be
    // part of the product partition, e.g. because they are mounted read-write.
    CHECKCALL(mkdir("/mnt/product", 0755));

    // /debug_ramdisk is used to preserve additional files from the debug ramdisk
    CHECKCALL(mount("tmpfs", "/debug_ramdisk", "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV,
                    "mode=0755,uid=0,gid=0"));

    // /second_stage_resources is used to preserve files from first to second
    // stage init
    CHECKCALL(mount("tmpfs", kSecondStageRes, "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV,
                    "mode=0755,uid=0,gid=0"))
#undef CHECKCALL

    SetStdioToDevNull(argv);
    // Now that tmpfs is mounted on /dev and we have /dev/kmsg, we can actually
    // talk to the outside world...
    InitKernelLogging(argv); // 初始化 kernel logger

    if (!errors.empty()) {
        for (const auto& [error_string, error_errno] : errors) {
            LOG(ERROR) << error_string << " " << strerror(error_errno);
        }
        LOG(FATAL) << "Init encountered errors starting first stage, aborting";
    }

    LOG(INFO) << "init first stage started!";

    auto old_root_dir = std::unique_ptr<DIR, decltype(&closedir)>{opendir("/"), closedir};
    if (!old_root_dir) {
        PLOG(ERROR) << "Could not opendir(\"/\"), not freeing ramdisk";
    }

    struct stat old_root_info;
    if (stat("/", &old_root_info) != 0) {
        PLOG(ERROR) << "Could not stat(\"/\"), not freeing ramdisk";
        old_root_dir.reset();
    }

    auto want_console = ALLOW_FIRST_STAGE_CONSOLE ? FirstStageConsole(cmdline, bootconfig) : 0;

    boot_clock::time_point module_start_time = boot_clock::now();
    int module_count = 0;
    // 加载内核模块
    if (!LoadKernelModules(IsRecoveryMode() && !ForceNormalBoot(cmdline, bootconfig), want_console,
                           module_count)) {
        if (want_console != FirstStageConsoleParam::DISABLED) {
            LOG(ERROR) << "Failed to load kernel modules, starting console";
        } else {
            LOG(FATAL) << "Failed to load kernel modules";
        }
    }
    if (module_count > 0) {
        auto module_elapse_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                boot_clock::now() - module_start_time);
        setenv(kEnvInitModuleDurationMs, std::to_string(module_elapse_time.count()).c_str(), 1);
        LOG(INFO) << "Loaded " << module_count << " kernel modules took "
                  << module_elapse_time.count() << " ms";
    }


    bool created_devices = false;
    if (want_console == FirstStageConsoleParam::CONSOLE_ON_FAILURE) {
        if (!IsRecoveryMode()) {
            created_devices = DoCreateDevices();
            if (!created_devices){
                LOG(ERROR) << "Failed to create device nodes early";
            }
        }
        StartConsole(cmdline);
    }

    // 拷贝prop，Copied ramdisk prop to /second_stage_resources/system/etc/ramdisk/build.prop
    if (access(kBootImageRamdiskProp, F_OK) == 0) {
        std::string dest = GetRamdiskPropForSecondStage();
        std::string dir = android::base::Dirname(dest);
        std::error_code ec;
        if (!fs::create_directories(dir, ec) && !!ec) {
            LOG(FATAL) << "Can't mkdir " << dir << ": " << ec.message();
        }
        if (!fs::copy_file(kBootImageRamdiskProp, dest, ec)) {
            LOG(FATAL) << "Can't copy " << kBootImageRamdiskProp << " to " << dest << ": "
                       << ec.message();
        }
        LOG(INFO) << "Copied ramdisk prop to " << dest;
    }

    // If "/force_debuggable" is present, the second-stage init will use a userdebug
    // sepolicy and load adb_debug.prop to allow adb root, if the device is unlocked.
    if (access("/force_debuggable", F_OK) == 0) {
        constexpr const char adb_debug_prop_src[] = "/adb_debug.prop";
        constexpr const char userdebug_plat_sepolicy_cil_src[] = "/userdebug_plat_sepolicy.cil";
        std::error_code ec;  // to invoke the overloaded copy_file() that won't throw.
        if (access(adb_debug_prop_src, F_OK) == 0 &&
            !fs::copy_file(adb_debug_prop_src, kDebugRamdiskProp, ec)) {
            LOG(WARNING) << "Can't copy " << adb_debug_prop_src << " to " << kDebugRamdiskProp
                         << ": " << ec.message();
        }
        if (access(userdebug_plat_sepolicy_cil_src, F_OK) == 0 &&
            !fs::copy_file(userdebug_plat_sepolicy_cil_src, kDebugRamdiskSEPolicy, ec)) {
            LOG(WARNING) << "Can't copy " << userdebug_plat_sepolicy_cil_src << " to "
                         << kDebugRamdiskSEPolicy << ": " << ec.message();
        }
        // setenv for second-stage init to read above kDebugRamdisk* files.
        setenv("INIT_FORCE_DEBUGGABLE", "true", 1);
    }

    if (ForceNormalBoot(cmdline, bootconfig)) {
        mkdir("/first_stage_ramdisk", 0755);
        // SwitchRoot() must be called with a mount point as the target, so we bind mount the
        // target directory to itself here.
        if (mount("/first_stage_ramdisk", "/first_stage_ramdisk", nullptr, MS_BIND, nullptr) != 0) {
            LOG(FATAL) << "Could not bind mount /first_stage_ramdisk to itself";
        }
        SwitchRoot("/first_stage_ramdisk");
    }

    // Mounts partitions specified by fstab in device tree.
// 通过fstab的配置挂载，在Android中通常在 /system/etc/fstab.xxx ，/vendor/etc/fstab.xxx 等地方
    if (!DoFirstStageMount(!created_devices)) {// 挂载一些必要分区,如/system
        LOG(FATAL) << "Failed to mount required partitions early ...";
    }

    struct stat new_root_info;
    if (stat("/", &new_root_info) != 0) {
        PLOG(ERROR) << "Could not stat(\"/\"), not freeing ramdisk";
        old_root_dir.reset();
    }

    if (old_root_dir && old_root_info.st_dev != new_root_info.st_dev) {
        FreeRamdisk(old_root_dir.get(), old_root_info.st_dev);
    }

    SetInitAvbVersionInRecovery();

    setenv(kEnvFirstStageStartedAt, std::to_string(start_time.time_since_epoch().count()).c_str(),
           1);

    /// 再次执行init，此处传入了 selinux_setup 参数，会进入 selinux初始化阶段
    /// 从init main 方法可知，此过程调用 SetupSelinux
    const char* path = "/system/bin/init";
    const char* args[] = {path, "selinux_setup", nullptr};
    auto fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
    execv(path, const_cast<char**>(args));

    // execv() only returns if an error happened, in which case we
    // panic and never fall through this conditional.
    PLOG(FATAL) << "execv(\"" << path << "\") failed";

    return 1;
}

```

### InstallRebootSignalHandlers

init信号处理器，调试版本当init crash，默认重启到 bootLoader

```
void InstallRebootSignalHandlers() {
    // Instead of panic'ing the kernel as is the default behavior when init crashes,
    // we prefer to reboot to bootloader on development builds, as this will prevent
    // boot looping bad configurations and allow both developers and test farms to easily
    // recover.
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    sigfillset(&action.sa_mask);
    action.sa_handler = [](int signal) {
        // These signal handlers are also caught for processes forked from init, however we do not
        // want them to trigger reboot, so we directly call _exit() for children processes here.
        if (getpid() != 1) { // 非init直接退出
            _exit(signal);
        }

        // Calling DoReboot() or LOG(FATAL) is not a good option as this is a signal handler.
        // RebootSystem uses syscall() which isn't actually async-signal-safe, but our only option
        // and probably good enough given this is already an error case and only enabled for
        // development builds.
        InitFatalReboot(signal); // 执行重启操作
    };
    action.sa_flags = SA_RESTART;
    // 设置信号处理器
    sigaction(SIGABRT, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGSEGV, &action, nullptr);
#if defined(SIGSTKFLT)
    sigaction(SIGSTKFLT, &action, nullptr);
#endif
    sigaction(SIGSYS, &action, nullptr);
    sigaction(SIGTRAP, &action, nullptr);
}
```

### InitFatalReboot

默认执行重启的 init\_fatal\_reboot\_target 的值是 bootloader

```
/// @system/core/init/reboot_utils.cpp
static std::string init_fatal_reboot_target = "bootloader";

void __attribute__((noreturn)) InitFatalReboot(int signal_number) {
    auto pid = fork();

    if (pid == -1) {
        // Couldn't fork, don't even try to backtrace, just reboot.
        RebootSystem(ANDROID_RB_RESTART2, init_fatal_reboot_target);
    } else if (pid == 0) { // 子进程确保能重启
        // Fork a child for safety, since we always want to shut down if something goes wrong, but
        // its worth trying to get the backtrace, even in the signal handler, since typically it
        // does work despite not being async-signal-safe.
        sleep(5);
        RebootSystem(ANDROID_RB_RESTART2, init_fatal_reboot_target);
    }

    // 先尝试获取 backtrace ，然后执行重启，
    // In the parent, let's try to get a backtrace then shutdown.
    LOG(ERROR) << __FUNCTION__ << ": signal " << signal_number;
    std::unique_ptr<Backtrace> backtrace(
            Backtrace::Create(BACKTRACE_CURRENT_PROCESS, BACKTRACE_CURRENT_THREAD));
    if (!backtrace->Unwind(0)) {
        LOG(ERROR) << __FUNCTION__ << ": Failed to unwind callstack.";
    }
    for (size_t i = 0; i < backtrace->NumFrames(); i++) {
        LOG(ERROR) << backtrace->FormatFrameData(i);
    }
// 在SetFatalRebootTarget函数读取是否触发panic和重启目标 默认bootloader
    if (init_fatal_panic) { // 若init退出触发panic
        LOG(ERROR) << __FUNCTION__ << ": Trigger crash";
        android::base::WriteStringToFile("c", PROC_SYSRQ); // 通过/proc/sysrq-trigger 触发死机
        LOG(ERROR) << __FUNCTION__ << ": Sys-Rq failed to crash the system; fallback to exit().";
        _exit(signal_number);
    }
    RebootSystem(ANDROID_RB_RESTART2, init_fatal_reboot_target);
}

```

### DoFirstStageMount

这里探究一下，在这个first stage挂载了那些分区

```
/// @system/core/init/first_stage_mount.cpp
// Mounts partitions specified by fstab in device tree.
bool DoFirstStageMount(bool create_devices) {
    // Skips first stage mount if we're in recovery mode.
    if (IsRecoveryMode()) {
        LOG(INFO) << "First stage mount skipped (recovery mode)";
        return true;
    }
// 创建 FirstStageMount，在此函数里读取fstab
    auto fsm = FirstStageMount::Create();
    if (!fsm.ok()) {
        LOG(ERROR) << "Failed to create FirstStageMount " << fsm.error();
        return false;
    }

    if (create_devices) {
        if (!(*fsm)->DoCreateDevices()) return false;
    }

    return (*fsm)->DoFirstStageMount(); // 执行mount
}

```

#### FirstStageMount::DoFirstStageMount

```
/// @system/core/init/first_stage_mount.cpp
bool FirstStageMount::DoFirstStageMount() {
// 判断 fstab和逻辑分区存在
    if (!IsDmLinearEnabled() && fstab_.empty()) {
        // Nothing to mount.
        LOG(INFO) << "First stage mount skipped (missing/incompatible/empty fstab in device tree)";
        return true;
    }
// 执行挂载fstab中的分区
    if (!MountPartitions()) return false;

    return true;
}
```

#### FirstStageMount::MountPartitions

```
bool FirstStageMount::MountPartitions() {
 // 挂载 /system
    if (!TrySwitchSystemAsRoot()) return false;
// 移除不需要挂载的分区
    if (!SkipMountingPartitions(&fstab_, true /* verbose */)) return false;
// 循环挂载fstab中的分区
    for (auto current = fstab_.begin(); current != fstab_.end();) {
        // We've already mounted /system above.
        if (current->mount_point == "/system") {
            ++current;
            continue;
        }

        // Handle overlayfs entries later.
        if (current->fs_type == "overlay") { // 延时 overlay
            ++current;
            continue;
        }

        // Skip raw partition entries such as boot, dtbo, etc.
        // Having emmc fstab entries allows us to probe current->vbmeta_partition
        // in InitDevices() when they are AVB chained partitions.
        if (current->fs_type == "emmc") {
            ++current;
            continue;
        }

        Fstab::iterator end;
        if (!MountPartition(current, false /* erase_same_mounts */, &end)) { // 挂载指定分区
            if (current->fs_mgr_flags.no_fail) {
                LOG(INFO) << "Failed to mount " << current->mount_point
                          << ", ignoring mount for no_fail partition";
            } else if (current->fs_mgr_flags.formattable) {
                LOG(INFO) << "Failed to mount " << current->mount_point
                          << ", ignoring mount for formattable partition";
            } else {
                PLOG(ERROR) << "Failed to mount " << current->mount_point;
                return false;
            }
        }
        current = end;
    }

    for (const auto& entry : fstab_) {
        if (entry.fs_type == "overlay") { // 处理 overlay
            fs_mgr_mount_overlayfs_fstab_entry(entry);
        }
    }
... // overlayfs, ScratchPartition
    return true;
}
```

从上面分析可知，挂载的信息存储在fstab\_ 里面，它是在FirstStageMount::Create函数中读取的

```
Result<std::unique_ptr<FirstStageMount>> FirstStageMount::Create() {
    auto fstab = ReadFirstStageFstab(); // 此处读取 fstab
    if (!fstab.ok()) {
        return fstab.error();
    }

    if (IsDtVbmetaCompatible(*fstab)) { // 根据 compatible 创建不同对象
        return std::make_unique<FirstStageMountVBootV2>(std::move(*fstab));
    } else {
        return std::make_unique<FirstStageMountVBootV1>(std::move(*fstab));
    }
}
```

#### ReadFirstStageFstab

```
/// @system/core/init/first_stage_mount.cpp
static Result<Fstab> ReadFirstStageFstab() {
    Fstab fstab;
    if (!ReadFstabFromDt(&fstab)) { // 首先读取device tree， 默认值 /proc/device-tree/firmware/android/fstab
        if (ReadDefaultFstab(&fstab)) { // 没有读到，再读默认Fstab
            fstab.erase(std::remove_if(fstab.begin(), fstab.end(),
                                       [](const auto& entry) {
                                           return !entry.fs_mgr_flags.first_stage_mount;
                                       }),
                        fstab.end());
        } else {
            return Error() << "failed to read default fstab for first stage mount";
        }
    }
    return fstab;
}
```

##### ReadFstabFromDt

```
/// @system/core/fs_mgr/fs_mgr_fstab.cpp
std::string ReadFstabFromDt() {
    if (!is_dt_compatible() || !IsDtFstabCompatible()) {
        return {};
    }
// 默认值 /proc/device-tree/firmware/android/fstab
    std::string fstabdir_name = get_android_dt_dir() + "/fstab";
    std::unique_ptr<DIR, int (*)(DIR*)> fstabdir(opendir(fstabdir_name.c_str()), closedir);
    if (!fstabdir) return {};

    dirent* dp;
    // Each element in fstab_dt_entries is <mount point, the line format in fstab file>.
    std::vector<std::pair<std::string, std::string>> fstab_dt_entries;
    while ((dp = readdir(fstabdir.get())) != NULL) { // 读取 fstab 信息
        // skip over name, compatible and .
        if (dp->d_type != DT_DIR || dp->d_name[0] == '.') continue;

        // create <dev> <mnt_point>  <type>  <mnt_flags>  <fsmgr_flags>\n
...
}
}
```

##### ReadDefaultFstab

```
/// @system/core/fs_mgr/fs_mgr_fstab.cpp
// Loads the fstab file and combines with fstab entries passed in from device tree.
bool ReadDefaultFstab(Fstab* fstab) {
    fstab->clear();
    ReadFstabFromDt(fstab, false /* verbose */); // 重新从 device tree 读取一次 ??

    std::string default_fstab_path;
    // Use different fstab paths for normal boot and recovery boot, respectively
    if (access("/system/bin/recovery", F_OK) == 0) { // recovery模式
        default_fstab_path = "/etc/recovery.fstab";
    } else {  // normal boot
        default_fstab_path = GetFstabPath(); // 获取 fstab 文件路径
    }

    Fstab default_fstab;
// 从 fstab 文件读取 fstab信息
    if (!default_fstab_path.empty() && ReadFstabFromFile(default_fstab_path, &default_fstab)) {
        for (auto&& entry : default_fstab) {
            fstab->emplace_back(std::move(entry));
        }
    } else {
        LINFO << __FUNCTION__ << "(): failed to find device default fstab";
    }

    return !fstab->empty();
}
```

看看GetFstabPath实现，决定从哪读取fstab

```
// Return the path to the fstab file.  There may be multiple fstab files; the
// one that is returned will be the first that exists of fstab.<fstab_suffix>,
// fstab.<hardware>, and fstab.<hardware.platform>.  The fstab is searched for
// in /odm/etc/ and /vendor/etc/, as well as in the locations where it may be in
// the first stage ramdisk during early boot.  Previously, the first stage
// ramdisk's copy of the fstab had to be located in the root directory, but now
// the system/etc directory is supported too and is the preferred location.
std::string GetFstabPath() {
    for (const char* prop : {"fstab_suffix", "hardware", "hardware.platform"}) {
        std::string suffix;
// 从 ro.boot.（prop值）或 kernel cmdline 等处读取文件名后缀，
// 从我的模拟器测试获取 ro.boot.hardware 为 ranchu
        if (!fs_mgr_get_boot_config(prop, &suffix)) continue;
// 遍历访问 prefix + suffix 路径的文件是否存在， 比如 /vendor/etc/fstab.ranchu
        for (const char* prefix : {// late-boot/post-boot locations
                                   "/odm/etc/fstab.", "/vendor/etc/fstab.",
                                   // early boot locations
                                   "/system/etc/fstab.", "/first_stage_ramdisk/system/etc/fstab.",
                                   "/fstab.", "/first_stage_ramdisk/fstab."}) {
            std::string fstab_path = prefix + suffix;
            if (access(fstab_path.c_str(), F_OK) == 0) {
                return fstab_path;
            }
        }
    }

    return "";
}
```

查看 /vendor/etc/fstab.ranchu , 看其中相关分区信息, 比如 /system、/data

```
$ cat /vendor/etc/fstab.ranchu
# Android fstab file.
#<dev>  <mnt_point> <type>  <mnt_flags options> <fs_mgr_flags>
system   /system     ext4    ro,barrier=1     wait,logical,avb=vbmeta,first_stage_mount
vendor   /vendor     ext4    ro,barrier=1     wait,logical,first_stage_mount
product  /product    ext4    ro,barrier=1     wait,logical,first_stage_mount
system_ext  /system_ext  ext4   ro,barrier=1   wait,logical,first_stage_mount
/dev/block/vdc   /data     ext4      noatime,nosuid,nodev,nomblk_io_submit,errors=panic   wait,check,quota,fileencryption=aes-256-xts:aes-256-cts,reservedsize=128M,fsverity,keydirectory=/metadata/vold/metadata_encryption,latemount
/dev/block/pci/pci0000:00/0000:00:06.0/by-name/metadata    /metadata    ext4    noatime,nosuid,nodev    wait,formattable,first_stage_mount
/devices/\*\/block/vdf auto   auto      defaults    voldmanaged=sdcard:auto,encryptable=userdata
dev/block/zram0 none swap  defaults zramsize=75%
```

## SetupSelinux

初始化 selinux 阶段

```
/// @system/core/init/selinux.cpp
// The SELinux setup process is carefully orchestrated around snapuserd. Policy
// must be loaded off dynamic partitions, and during an OTA, those partitions
// cannot be read without snapuserd. But, with kernel-privileged snapuserd
// running, loading the policy will immediately trigger audits.
//
// We use a five-step process to address this:
//  (1) Read the policy into a string, with snapuserd running.
//  (2) Rewrite the snapshot device-mapper tables, to generate new dm-user
//      devices and to flush I/O.
//  (3) Kill snapuserd, which no longer has any dm-user devices to attach to.
//  (4) Load the sepolicy and issue critical restorecons in /dev, carefully
//      avoiding anything that would read from /system.
//  (5) Re-launch snapuserd and attach it to the dm-user devices from step (2).
//
// After this sequence, it is safe to enable enforcing mode and continue booting.
int SetupSelinux(char** argv) {
    SetStdioToDevNull(argv);
    InitKernelLogging(argv);

    if (REBOOT_BOOTLOADER_ON_PANIC) { // panic 重启到 BootLoader
        InstallRebootSignalHandlers();
    }

    boot_clock::time_point start_time = boot_clock::now();

    MountMissingSystemPartitions();

    SelinuxSetupKernelLogging();

    LOG(INFO) << "Opening SELinux policy";

    // Read the policy before potentially killing snapuserd.
    std::string policy;
// 首先获取 precompiled policy文件，没有则执行/system/bin/secilc将policy编译到一个文件，
// 然后从上面获取的文件读取策略文件
    ReadPolicy(&policy);

    auto snapuserd_helper = SnapuserdSelinuxHelper::CreateIfNeeded();
    if (snapuserd_helper) { // kill snapused
        // Kill the old snapused to avoid audit messages. After this we cannot
        // read from /system (or other dynamic partitions) until we call
        // FinishTransition().
        snapuserd_helper->StartTransition();
    }

    LoadSelinuxPolicy(policy); // 加载 selinux policy

    if (snapuserd_helper) { // resume snapused
        // Before enforcing, finish the pending snapuserd transition.
        snapuserd_helper->FinishTransition();
        snapuserd_helper = nullptr;
    }

    SelinuxSetEnforcement(); // 设置 selinux policy 启动状态, 写 /sys/fs/selinux/enforce

    // We're in the kernel domain and want to transition to the init domain.  File systems that
    // store SELabels in their xattrs, such as ext4 do not need an explicit restorecon here,
    // but other file systems do.  In particular, this is needed for ramdisks such as the
    // recovery image for A/B devices.
    if (selinux_android_restorecon("/system/bin/init", 0) == -1) {
        PLOG(FATAL) << "restorecon failed of /system/bin/init failed";
    }

    setenv(kEnvSelinuxStartedAt, std::to_string(start_time.time_since_epoch().count()).c_str(), 1);

    /// 再次执行init，此处传入了 second_stage 参数，会进入 启动第二阶段
    /// 从init main 方法可知，此过程调用 SecondStageMain
    const char* path = "/system/bin/init";
    const char* args[] = {path, "second_stage", nullptr};
    execv(path, const_cast<char**>(args));

    // execv() only returns if an error happened, in which case we
    // panic and never return from this function.
    PLOG(FATAL) << "execv(\"" << path << "\") failed";

    return 1;
}
```

## SecondStageMain

第二阶段执行

```
/// system/core/init/init.cpp
int SecondStageMain(int argc, char** argv) {
    if (REBOOT_BOOTLOADER_ON_PANIC) {
        InstallRebootSignalHandlers();// 设置Signal处理器
    }

    boot_clock::time_point start_time = boot_clock::now();
    // shutdown 处理函数
    trigger_shutdown = [](const std::string& command) { shutdown_state.TriggerShutdown(command); };

    SetStdioToDevNull(argv);
    InitKernelLogging(argv);
    LOG(INFO) << "init second stage started!";

    // Update $PATH in the case the second stage init is newer than first stage init, where it is
    // first set.
    if (setenv("PATH", _PATH_DEFPATH, 1) != 0) {
        PLOG(FATAL) << "Could not set $PATH to '" << _PATH_DEFPATH << "' in second stage";
    }

    // Init should not crash because of a dependence on any other process, therefore we ignore
    // SIGPIPE and handle EPIPE at the call site directly.  Note that setting a signal to SIG_IGN
    // is inherited across exec, but custom signal handlers are not.  Since we do not want to
    // ignore SIGPIPE for child processes, we set a no-op function for the signal handler instead.
    {
        struct sigaction action = {.sa_flags = SA_RESTART};
        action.sa_handler = [](int) {};
        sigaction(SIGPIPE, &action, nullptr);  // 对 SIGPIPE 进行拦截处理
    }

    // Set init and its forked children's oom_adj.
    if (auto result =
                WriteFile("/proc/1/oom_score_adj", StringPrintf("%d", DEFAULT_OOM_SCORE_ADJUST));
        !result.ok()) { // 设置 oom_score_adj ， DEFAULT_OOM_SCORE_ADJUST = -1000
        LOG(ERROR) << "Unable to write " << DEFAULT_OOM_SCORE_ADJUST
                   << " to /proc/1/oom_score_adj: " << result.error();
    }

    // Set up a session keyring that all processes will have access to. It
    // will hold things like FBE encryption keys. No process should override
    // its session keyring.
    keyctl_get_keyring_ID(KEY_SPEC_SESSION_KEYRING, 1);

    // Indicate that booting is in progress to background fw loaders, etc.
    close(open("/dev/.booting", O_WRONLY | O_CREAT | O_CLOEXEC, 0000));

    // See if need to load debug props to allow adb root, when the device is unlocked.
    const char* force_debuggable_env = getenv("INIT_FORCE_DEBUGGABLE");
    bool load_debug_prop = false;
    if (force_debuggable_env && AvbHandle::IsDeviceUnlocked()) {// 是否加载 debug props
        load_debug_prop = "true"s == force_debuggable_env;
    }
    unsetenv("INIT_FORCE_DEBUGGABLE");

    // Umount the debug ramdisk so property service doesn't read .prop files from there, when it
    // is not meant to.
    if (!load_debug_prop) {
        UmountDebugRamdisk();
    }

    PropertyInit(); // 属性环境相关初始化，及固有属性加载

    // Umount second stage resources after property service has read the .prop files.
    UmountSecondStageRes(); //  umount /second_stage_resources

    // Umount the debug ramdisk after property service has read the .prop files when it means to.
    if (load_debug_prop) {
        UmountDebugRamdisk();
    }

    // Mount extra filesystems required during second stage init
    MountExtraFilesystems();  // mount 其他文件系统，如 /apex ，/linkerconfig

    // Now set up SELinux for second stage.
    SelinuxSetupKernelLogging();
    SelabelInitialize();
    SelinuxRestoreContext();

    Epoll epoll;
    if (auto result = epoll.Open(); !result.ok()) { // 创建 Epoll
        PLOG(FATAL) << result.error();
    }

    InstallSignalFdHandler(&epoll);  // 将 signalFd 添加到 epoll 监听
    InstallInitNotifier(&epoll);  // 将wake_main_thread_fd添加到 epoll 监听，用于唤醒main线程
    StartPropertyService(&property_fd); // 启动属性服务

    // Make the time that init stages started available for bootstat to log.
    RecordStageBoottimes(start_time);

    // Set libavb version for Framework-only OTA match in Treble build.
    if (const char* avb_version = getenv("INIT_AVB_VERSION"); avb_version != nullptr) {
        SetProperty("ro.boot.avb_version", avb_version);
    }
    unsetenv("INIT_AVB_VERSION");

    fs_mgr_vendor_overlay_mount_all();
    export_oem_lock_status();
    MountHandler mount_handler(&epoll);
    SetUsbController();
    SetKernelVersion();
// 内置函数映射， 比如  {"trigger", {1,     1,    {false,  do_trigger}}}
// rc 的 trigger 对应  do_trigger
    const BuiltinFunctionMap& function_map = GetBuiltinFunctionMap();
    Action::set_function_map(&function_map);

    if (!SetupMountNamespaces()) {
        PLOG(FATAL) << "SetupMountNamespaces failed";
    }

    InitializeSubcontext();
// 创建 action管理者，用于管理相关事件对应的动作，对应的是rc中 on xxx 以及 Builtin Action
    ActionManager& am = ActionManager::GetInstance();
// 创建服务列表，管理rc中的 service
    ServiceList& sm = ServiceList::GetInstance();

    LoadBootScripts(am, sm); // 解析 init相关rc文件

    // Turning this on and letting the INFO logging be discarded adds 0.2s to
    // Nexus 9 boot time, so it's disabled by default.
    if (false) DumpState();

    // Make the GSI status available before scripts start running.
    auto is_running = android::gsi::IsGsiRunning() ? "1" : "0";
    SetProperty(gsi::kGsiBootedProp, is_running);
    auto is_installed = android::gsi::IsGsiInstalled() ? "1" : "0";
    SetProperty(gsi::kGsiInstalledProp, is_installed);

    am.QueueBuiltinAction(SetupCgroupsAction, "SetupCgroups");
    am.QueueBuiltinAction(SetKptrRestrictAction, "SetKptrRestrict");
    am.QueueBuiltinAction(TestPerfEventSelinuxAction, "TestPerfEventSelinux");
    am.QueueEventTrigger("early-init");

    // Queue an action that waits for coldboot done so we know ueventd has set up all of /dev...
    am.QueueBuiltinAction(wait_for_coldboot_done_action, "wait_for_coldboot_done");
    // ... so that we can start queuing up actions that require stuff from /dev.
    am.QueueBuiltinAction(SetMmapRndBitsAction, "SetMmapRndBits");
    Keychords keychords;
    am.QueueBuiltinAction(
            [&epoll, &keychords](const BuiltinArguments& args) -> Result<void> {
                for (const auto& svc : ServiceList::GetInstance()) {
                    keychords.Register(svc->keycodes());
                }
                keychords.Start(&epoll, HandleKeychord);
                return {};
            },
            "KeychordInit");

    // Trigger all the boot actions to get us started.
    am.QueueEventTrigger("init");

    // Don't mount filesystems or start core system services in charger mode.
    std::string bootmode = GetProperty("ro.bootmode", "");
    if (bootmode == "charger") {
        am.QueueEventTrigger("charger");
    } else {
        am.QueueEventTrigger("late-init");
    }

    // Run all property triggers based on current state of the properties.
    am.QueueBuiltinAction(queue_property_triggers_action, "queue_property_triggers");

    // Restore prio before main loop
    setpriority(PRIO_PROCESS, 0, 0);
    while (true) { // 主循环，后面介绍
...
auto pending_functions = epoll.Wait(epoll_timeout); // 等待到新消息到来或超时
...
    }

    return 0;
}
```

接下来看一些主要流程

### PropertyInit

```
void PropertyInit() {
    selinux_callback cb;
    cb.func_audit = PropertyAuditCallback;
    selinux_set_callback(SELINUX_CB_AUDIT, cb);

    mkdir("/dev/__properties__", S_IRWXU | S_IXGRP | S_IXOTH);
    CreateSerializedPropertyInfo(); // 创建property se contexts
    if (__system_property_area_init()) { // 将 /dev/__properties__/properties_serial 映射到内存, 创建 ContextNodes,对每个node打开映射
        LOG(FATAL) << "Failed to initialize property area";
    }
    if (!property_info_area.LoadDefaultPath()) { // 加载/dev/__properties__/property_info 映射进内存
        LOG(FATAL) << "Failed to load serialized property info file";
    }

    // If arguments are passed both on the command line and in DT,
    // properties set in DT always have priority over the command-line ones.
    ProcessKernelDt(); // 解析 /proc/device-tree/firmware/android/
    ProcessKernelCmdline(); // 解析 /proc/cmdline ， 将其中键值对满足key为androidboot.* 的，将key替换为ro.boot.*，然后添加到属性
    ProcessBootconfig(); // 解析 /proc/bootconfig ，同上

    // Propagate the kernel variables to internal variables
    // used by init as well as the current required properties.
    ExportKernelBootProps(); // 给一些kernel属性初始赋值，如果没有设置的话

    // 加载默认的属性文件的属性
    // 如 /system/build.prop /vendor/default.prop /vendor/build.prop
    // 还会解析其他分区 如 odm、product、system_ext
    PropertyLoadBootDefaults();
}
```

### StartPropertyService

启动系统服务，建立与init之间通信socket，以及设置属性监听

```
/// @system/core/init/property_service.cpp
void StartPropertyService(int* epoll_socket) {
    InitPropertySet("ro.property_service.version", "2");

    int sockets[2];
    // 创建 socket 对，用于init和属性服务间通信
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
        PLOG(FATAL) << "Failed to socketpair() between property_service and init";
    }
    *epoll_socket = from_init_socket = sockets[0]; // 回传给init端
    init_socket = sockets[1]; // 持有此fd端
    StartSendingMessages(); // 设置 accept_messages = true ， 表示可以处理请求消息

    // 创建接收属性请求的 socket
    if (auto result = CreateSocket(PROP_SERVICE_NAME, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
                                   false, 0666, 0, 0, {});
        result.ok()) {
        property_set_fd = *result; // 记录此fd
    } else {
        LOG(FATAL) << "start_property_service socket creation failed: " << result.error();
    }

    listen(property_set_fd, 8); // 监听

    auto new_thread = std::thread{PropertyServiceThread}; // 创建新线程，调用PropertyServiceThread用于处理请求，
    property_service_thread.swap(new_thread);
}
```

### LoadBootScripts

加载并解析 init rc 脚本

```
static void LoadBootScripts(ActionManager& action_manager, ServiceList& service_list) {
    Parser parser = CreateParser(action_manager, service_list);

    std::string bootscript = GetProperty("ro.boot.init_rc", "");
    if (bootscript.empty()) {
        parser.ParseConfig("/system/etc/init/hw/init.rc"); // 首先解析 init.rc
        if (!parser.ParseConfig("/system/etc/init")) { // 解析 /system/etc/init 目录
            late_import_paths.emplace_back("/system/etc/init"); // 解析失败延时解析
        }
        // late_import is available only in Q and earlier release. As we don't
        // have system_ext in those versions, skip late_import for system_ext.
        parser.ParseConfig("/system_ext/etc/init"); // 解析 /system_ext/etc/init 目录
        if (!parser.ParseConfig("/vendor/etc/init")) { // 解析 /vendor/etc/init 目录
            late_import_paths.emplace_back("/vendor/etc/init");
        }
        if (!parser.ParseConfig("/odm/etc/init")) { // 解析 /odm/etc/init 目录
            late_import_paths.emplace_back("/odm/etc/init");
        }
        if (!parser.ParseConfig("/product/etc/init")) { // 解析 /product/etc/init 目录
            late_import_paths.emplace_back("/product/etc/init");
        }
    } else {
        parser.ParseConfig(bootscript);
    }
}
```

### 添加内置动作和事件触发器

-   内置动作（Builtin Action）  
    只在代码里面调用QueueBuiltinAction的action，其他action在rc里使用 on 声明。action通常需要一些事件来触发
    
-   事件触发器（Trigger）  
    调用QueueEventTrigger插入事件触发器
    

```
// 添加相关action，会同时添加到 事件队列 和 action队列
am.QueueBuiltinAction(SetupCgroupsAction, "SetupCgroups");
am.QueueBuiltinAction(SetKptrRestrictAction, "SetKptrRestrict");
am.QueueBuiltinAction(TestPerfEventSelinuxAction, "TestPerfEventSelinux");
am.QueueEventTrigger("early-init"); // 触发 early-init

// Queue an action that waits for coldboot done so we know ueventd has set up all of /dev...
am.QueueBuiltinAction(wait_for_coldboot_done_action, "wait_for_coldboot_done");
// ... so that we can start queuing up actions that require stuff from /dev.
am.QueueBuiltinAction(SetMmapRndBitsAction, "SetMmapRndBits");
Keychords keychords;
am.QueueBuiltinAction(...,"KeychordInit");

// Trigger all the boot actions to get us started.
am.QueueEventTrigger("init"); // 触发 init

// Don't mount filesystems or start core system services in charger mode.
std::string bootmode = GetProperty("ro.bootmode", "");
if (bootmode == "charger") { // 充电模式
am.QueueEventTrigger("charger");
} else { // 正常模式， 触发 late-init
am.QueueEventTrigger("late-init");
}

// Run all property triggers based on current state of the properties.
// 添加属性触发器。在queue_property_triggers_action中添加的trigger执行之后开始处理属性变化事件, 同时将已匹配的属性事件触发
am.QueueBuiltinAction(queue_property_triggers_action, "queue_property_triggers");
```

以上操作实际上只是向事件队列和action集合添加，而没有真正的去执行，真正触发执行是在主循环中，通过调用 ActionManager#ExecuteOneCommand。

### SecondStageMain 循环处理事件

如下是 init 主循环，负责处理相关事件。

```
int SecondStageMain(int argc, char** argv) {
...
// Restore prio before main loop
setpriority(PRIO_PROCESS, 0, 0);
while (true) {
    // By default, sleep until something happens. 计算epool超时
    auto epoll_timeout = std::optional<std::chrono::milliseconds>{};

    auto shutdown_command = shutdown_state.CheckShutdown();
    if (shutdown_command) { // 处理关机请求
        LOG(INFO) << "Got shutdown_command '" << *shutdown_command
                  << "' Calling HandlePowerctlMessage()";
        HandlePowerctlMessage(*shutdown_command);
        shutdown_state.set_do_shutdown(false);
    }
    // 当没有要等待的属性或执行的服务 则从事件队列中取出一个执行其对应的 action，可能对应多个action
    if (!(prop_waiter_state.MightBeWaiting() || Service::is_exec_service_running())) {
        am.ExecuteOneCommand();
    }
    if (!IsShuttingDown()) { // 不是正在关机， 如有需要重启的服务，需要据此重新计算超时时间
        auto next_process_action_time = HandleProcessActions();

        // If there's a process that needs restarting, wake up in time for that.
        if (next_process_action_time) {
            epoll_timeout = std::chrono::ceil<std::chrono::milliseconds>(
                    *next_process_action_time - boot_clock::now());
            if (*epoll_timeout < 0ms) epoll_timeout = 0ms;
        }
    }

    if (!(prop_waiter_state.MightBeWaiting() || Service::is_exec_service_running())) {
        // If there's more work to do, wake up again immediately.
        if (am.HasMoreCommands()) epoll_timeout = 0ms; // 还有事件需要处理，超时为0，即里面处理下个事件
    }

    auto pending_functions = epoll.Wait(epoll_timeout); // 等待到新消息到来或超时
    if (!pending_functions.ok()) {
        LOG(ERROR) << pending_functions.error();
    } else if (!pending_functions->empty()) { // 有待执行命令，比如唤醒要执行的回调 clear_eventfd
        // We always reap children before responding to the other pending functions. This is to
        // prevent a race where other daemons see that a service has exited and ask init to
        // start it again via ctl.start before init has reaped it.
        ReapAnyOutstandingChildren(); // 首先回收已退出的进程，回收僵尸进程
        for (const auto& function : *pending_functions) { // 执行相关回调
            (*function)();
        }
    }
    if (!IsShuttingDown()) { // 不是正在关机，
        HandleControlMessages(); // 处理 ctl 属性消息
        SetUsbController();
    }
}

return 0;
}
```

### 内置action和触发器执行

当初次进入会直接调用 ActionManager::ExecuteOneCommand，去执行之前的事件，因此会依次执行

-   触发SetKptrRestrict， 调用 SetupCgroupsAction
    
-   触发SetKptrRestrict， 调用 SetKptrRestrictAction
    
-   触发 early-init
    
    -   启动 ueventd
-   触发wait\_for\_coldboot\_done，调用 wait\_for\_coldboot\_done\_action
    
    -   等待ro.cold\_boot\_done=true，即ueventd执行完
-   触发SetMmapRndBits，调用 SetMmapRndBitsAction
    
-   触发KeychordInit
    
-   触发init
    
    -   启动logd、servicemanager、hwservicemanager、vndservicemanager
-   触发late-init / charger(充电模式下)，下面都是在 late-init 情况下触发
    
    -   trigger early-fs
        
    -   trigger fs
        
    -   trigger post-fs
        
    -   trigger late-fs
        
    -   trigger post-fs-data
        
    -   trigger load\_bpf\_programs
        
    -   trigger zygote-start # 触发启动zygote 框架
        
    -   trigger firmware\_mounts\_complete
        
    -   trigger early-boot
        
    -   trigger boot
        
-   触发queue\_property\_triggers， 调用queue\_property\_triggers\_action
    

#### late-init

```
# Mount filesystems and start core system services.
on late-init
    trigger early-fs// 启动 vold

    # Mount fstab in init.{$device}.rc by mount_all command. Optional parameter
    # '--early' can be specified to skip entries with 'latemount'.
    # /system and /vendor must be mounted by the end of the fs stage,
    # while /data is optional.
    trigger fs// 如 mount_all /vendor/etc/fstab.ranchu --early
    trigger post-fs // 创建和挂载一些目录 如 /mnt/user/0 -> /storage

    # Mount fstab in init.{$device}.rc by mount_all with '--late' parameter
    # to only mount entries with 'latemount'. This is needed if '--early' is
    # specified in the previous mount_all command on the fs stage.
    # With /system mounted and properties form /system + /factory available,
    # some services can be started.
    trigger late-fs  // 如 mount_all /vendor/etc/fstab.ranchu --late , /data配置的latemount

    # Now we can mount /data. File encryption requires keymaster to decrypt
    # /data, which in turn can only be loaded when system properties are present.
    trigger post-fs-data// 挂载 /data，创建一些主要目录

    # Should be before netd, but after apex, properties and logging is available.
    trigger load_bpf_programs

    # Now we can start zygote for devices with file based encryption
    trigger zygote-start//启动zygote和相关服务

    # Remove a file to wake up anything waiting for firmware.
    trigger firmware_mounts_complete

    trigger early-boot
    trigger boot // 启动 hal、core 类别的 service，也就是 native daemons

```

trigger 会触发调用do\_trigger，向事件队列添加相关触发器，之后会依次取出相关事件执行对应的action

```
/// @system/core/init/builtins.cpp
static Result<void> do_trigger(const BuiltinArguments& args) {
    ActionManager::GetInstance().QueueEventTrigger(args[1]);
    return {};
}

/// system/core/init/action_manager.cpp
void ActionManager::QueueEventTrigger(const std::string& trigger) {
    auto lock = std::lock_guard{event_queue_lock_};
    event_queue_.emplace(trigger);
}
```

#### queue\_property\_triggers

这个触发器是在 late-init 触发器之后加入事件队列的，早于late-init的action中添加的触发器，比如early-fs。该触发器对应的action是queue\_property\_triggers\_action

```
/// @system/core/init/init.cpp
static Result<void> queue_property_triggers_action(const BuiltinArguments& args) {
// 添加一个enable_property_trigger，将触发init使能处理属性事件。 从时序来看，将晚于 boot trigger 执行
// late-init -> queue_property_triggers -> boot -> enable_property_trigger
    ActionManager::GetInstance().QueueBuiltinAction(property_enable_triggers_action, "enable_property_trigger");
    ActionManager::GetInstance().QueueAllPropertyActions(); // 将所有属性满足的action添加到队列
    return {};
}

static Result<void> property_enable_triggers_action(const BuiltinArguments& args) {
    /* Enable property triggers. */
    property_triggers_enabled = 1;
    return {};
}
```

将所有属性匹配的action添加到队列。

```
/// @system/core/init/action_manager.cpp
void ActionManager::QueueAllPropertyActions() {
    QueuePropertyChange("", "");
}

// 比如当此时 persist.traced_perf.enable 的值已经为1 ，则会添加相关action到队列，最终会执行 start traced_perf
// init/traced_perf.rc
on property:persist.traced_perf.enable=1
    start traced_perf
```

### trigger zygote-start

zygote-start触发器是用来启动zygote和相关进程的，整个action的执行会依赖加密状态来执行，这些encrypted状态是在执行 mount\_all 操作中设置的。可以看到，依次启动了statsd、netd和zygote等进程，zygote的启动会建立系统服务system\_server进程的创建。

```
on zygote-start && property:ro.crypto.state=encrypted && property:ro.crypto.type=file
    wait_for_prop odsign.verification.done 1
    # A/B update verifier that marks a successful boot.
    exec_start update_verifier_nonencrypted
    start statsd
    start netd
    start zygote# 启动zygote进程
    start zygote_secondary
```

### trigger boot

触发boot事件

```
on boot
...
# Update dm-verity state and set partition.*.verified properties.
verity_update_state

# Start standard binderized HAL daemons
class_start hal// 启动类别为hal的服务(在rc中使用 class hal定义), 比如vendor.audio-hal

class_start core// 启动类别为core的服务，比如 surfaceflinger

```

## 总结

init是kernel启动的第一个用户空间进程(pid为1)，它在经历FirstStage、selinux\_setup和SecondStage后，进入loop循环等待事件发生，比如属性事件或者子进程死亡处理。流程大致如下(正常开机模式)：

-   FirstStage 挂载一些基础文件系统和加载内核模块等
    
-   selinux\_setup 执行selinux的初始化
    
-   SecondStage 挂载其他文件系统，启动属性服务，执行boot流程等，主要逻辑都在这里实现
    
    -   PropertyInit - StartPropertyService 初始化和启动属性服务
        
    -   LoadBootScripts 解析开机脚本 rc文件
        
    -   early-init 早期init阶段,执行启动 ueventd
        
    -   init init阶段，在此阶段会启动logd、servicemanager、hwservicemanager、vndservicemanager
        
    -   late-init 末期init
        
        -   early-fs 启动 vold
        -   fs 使用mount\_all挂载 init.{$device}.rc 中的fstab相关分区，使用 --early 参数
        -   post-fs 创建和挂载一些目录 如 /mnt/user/0 -> /storage
        -   late-fs 使用mount\_all挂载 init.{$device}.rc 中的fstab相关分区，使用 --late 参数
        -   post-fs-data 创建/data一些主要目录
        -   load\_bpf\_programs
        -   zygote-start 触发启动zygote 框架
        -   firmware\_mounts\_complete
        -   early-boot 在boot之前的一个事件
        -   boot 启动核心native服务，如 surfaceflinger、audioserver
    -   queue\_property\_triggers 添加property\_triggers，早于early-fs 晚于late-init
        
        -   enable\_property\_trigger 晚于boot trigger添加，在其之后执行。触发使能init处理属性事件
        -   QueueAllPropertyActions 将所有属性匹配的action添加到队列
    -   进入loop循环等待事件发生并处理(主要以下几种)
        
        -   处理build-in action，在执行结束后被移除(oneshot)
        -   处理唤醒事件
        -   处理属性事件
        -   处理子进程死亡事件

  

___

```
如果您觉得阅读本文对您有帮助，请点一下“推荐”按钮，您的“推荐”将是我最大的写作动力！
```