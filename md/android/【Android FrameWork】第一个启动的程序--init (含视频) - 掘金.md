---
title: 第一个启动的程序--init
date: 2022-01-07 22:53:43
tags:
---

## 前言

失业了，时间就多了起来，把之前整理的知识都记录成文档，分享出来。可以整理下自己的经历，留下来，希望可以帮助到大家，也是对自己知识的一个汇总。

## 前提

首先明确下前提条件：

```
 1.有C/C++基础，不要求非常精通，但是能够阅读代码
 2.有Linux系统的基础知识
 3.有Android的基础知识，有相关经验更好     
```

大家都是Android开发，对于阅读源代码都是必须要经历的。业务开发经常所面对常用Api的调用，UI搭建，架构。随着深入，仅仅只会Api的调用是无法满足需求的，如果想成为高级/资深工程师，那么阅读系统源码是必修。例如我们在开发的过程中遇到的问题，有很多是需要通过分析源码解决。 还有性能优化，卡顿优化，ANR等 都需要我们对源码非常熟悉才能进行。我们也可以从源码中学到很多很多的知识，能够做一些看似无法实现的功能，例如插件化、保活等等。总之源码是我们提高都必修课也是最好的学习材料。

## 开始

接下来正文开始，这是一张详细的架构图，层次分别是Application层、FrameWork层、Runtime Libraries层以及Hal层和Kernel层。架构还是很清晰的。对Application层熟悉能满足我们的日常开发需求。从Application层开始我们需要一层层往下走。那么我们就从Android开机启动的第一个程序init做为入口，打开FrameWork的神秘大门吧。

`注：本次分享针对Android10。`

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/70ced50db91040b6a7f2e17a41523872~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 正文开始

不管Java还是C++运行一个程序都是以main方法作为入口。所以我们先看看init.cpp的main函数.

目录：`/system/core/init/main.cpp`

在线地址:[android.googlesource.com/platform/sy…](https://link.juejin.cn/?target=https%3A%2F%2Fandroid.googlesource.com%2Fplatform%2Fsystem%2Fcore%2F%2B%2Frefs%2Fheads%2Fandroid10-release%2Finit%2Fmain.cpp "https://android.googlesource.com/platform/system/core/+/refs/heads/android10-release/init/main.cpp")

具体代码：

```
int main(int argc, char** argv) {
#if __has_feature(address_sanitizer)
    __asan_set_error_report_callback(AsanReportCallback);
#endif
    if (!strcmp(basename(argv[0]), "ueventd")) {
        return ueventd_main(argc, argv);
    }
    if (argc > 1) {
        if (!strcmp(argv[1], "subcontext")) {
            android::base::InitLogging(argv, &android::base::KernelLogger);
            const BuiltinFunctionMap function_map;
            return SubcontextMain(argc, argv, &function_map);
        }
        if (!strcmp(argv[1], "selinux_setup")) {
            return SetupSelinux(argv);
        }
        if (!strcmp(argv[1], "second_stage")) {
            return SecondStageMain(argc, argv);//
        }
    }
    return FirstStageMain(argc, argv);//在这里创建和挂载了启动所需要的目录信息包含tmpfs、devpts、proc、sysfs、selinuxfs文件系统。。
}
```

在Main函数，分了几个阶段，首先第一步是`FirstStageMain`，我们看看他做了什么？

目录:`/system/core/init/first_stage_init.cpp`

在线地址:[android.googlesource.com/platform/sy…](https://link.juejin.cn/?target=https%3A%2F%2Fandroid.googlesource.com%2Fplatform%2Fsystem%2Fcore%2F%2B%2Frefs%2Fheads%2Fandroid10-release%2Finit%2Ffirst_stage_init.cpp "https://android.googlesource.com/platform/system/core/+/refs/heads/android10-release/init/first_stage_init.cpp") 具体代码:

```
int FirstStageMain(int argc, char** argv) {
    if (REBOOT_BOOTLOADER_ON_PANIC) {
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
    // Get the basic filesystem setup we need put together in the initramdisk
    // on / and then we'll let the rc file figure out the rest.
    CHECKCALL(mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755"));//挂载了tmpfs
    CHECKCALL(mkdir("/dev/pts", 0755));//创建pts目录并且赋予0755的权限
    CHECKCALL(mkdir("/dev/socket", 0755));//创建socket目录并且赋予0755的权限
    CHECKCALL(mkdir("/dev/dm-user", 0755));//创建dm-user目录并且赋予0755的权限
    CHECKCALL(mount("devpts", "/dev/pts", "devpts", 0, NULL));//挂载devpts
#define MAKE_STR(x) __STRING(x)
    CHECKCALL(mount("proc", "/proc", "proc", 0, "hidepid=2,gid=" MAKE_STR(AID_READPROC)));//挂载proc文件系统
#undef MAKE_STR
    // Don't expose the raw commandline to unprivileged processes.
    CHECKCALL(chmod("/proc/cmdline", 0440));
    std::string cmdline;
    android::base::ReadFileToString("/proc/cmdline", &cmdline);
    // Don't expose the raw bootconfig to unprivileged processes.
    chmod("/proc/bootconfig", 0440);
    std::string bootconfig;
    android::base::ReadFileToString("/proc/bootconfig", &bootconfig);
    gid_t groups[] = {AID_READPROC};
    CHECKCALL(setgroups(arraysize(groups), groups));
    CHECKCALL(mount("sysfs", "/sys", "sysfs", 0, NULL));//挂载sysfs
    CHECKCALL(mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL));//挂载selinxfs

    CHECKCALL(mknod("/dev/kmsg", S_IFCHR | 0600, makedev(1, 11)));

    if constexpr (WORLD_WRITABLE_KMSG) {
        CHECKCALL(mknod("/dev/kmsg_debug", S_IFCHR | 0622, makedev(1, 11)));
    }

    CHECKCALL(mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8)));
    CHECKCALL(mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9)));

    CHECKCALL(mknod("/dev/ptmx", S_IFCHR | 0666, makedev(5, 2)));
    CHECKCALL(mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3)));

    CHECKCALL(mount("tmpfs", "/mnt", "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV,
                    "mode=0755,uid=0,gid=1000"));
    CHECKCALL(mkdir("/mnt/vendor", 0755));
    CHECKCALL(mkdir("/mnt/product", 0755));
    CHECKCALL(mount("tmpfs", "/debug_ramdisk", "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV,
                    "mode=0755,uid=0,gid=0"));
    CHECKCALL(mount("tmpfs", kSecondStageRes, "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV,
                    "mode=0755,uid=0,gid=0"))
#undef CHECKCALL

    SetStdioToDevNull(argv);
    InitKernelLogging(argv);


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

原来init进程的第一个阶段主要是挂载了tmpfs、devpts、proc、sysfs和selinux 文件系统，也就是说android系统启动就挂载了，关闭就消失了。

其中文件系统：

1.tmpfs:虚拟文件系统，他把所有的文件都存在虚拟内存中。tmpfs速度很快，毕竟他是驻留在RAM中。但是由于驻留在RAM中，所以断电之后文件内容不会被保存。

2.devpts：管理远程虚拟终端文件设备，挂接点是/dev/pts。pty的主复合设备/dev/ptmx被打开，就会在/dev/pts下创建pty设备文件。

3.proc:非常重要的虚拟文件系统，，通过它我们可以获得系统信息，同时也能够在运行时修改特定的内核参数。

4.sysfs:和proc类似，把连接在系统上的设备和总线组织成一个分级的文件，使得它们可以在用户空间存取。

5.selinux:对系统中每一个对象都生成一个安全上下文，每个对象访问系统资源都需要进行上下文审查。

再挂在完成之后，又返回了init 启动了selinux\_setup。

```
int SetupSelinux(char** argv) {
    SetStdioToDevNull(argv);
    InitKernelLogging(argv);

    if (REBOOT_BOOTLOADER_ON_PANIC) {
        InstallRebootSignalHandlers();
    }

    boot_clock::time_point start_time = boot_clock::now();

    MountMissingSystemPartitions();

    SelinuxSetupKernelLogging();

    LOG(INFO) << "Opening SELinux policy";

    PrepareApexSepolicy();

    // Read the policy before potentially killing snapuserd.
    std::string policy;
    ReadPolicy(&policy);
    CleanupApexSepolicy();

    auto snapuserd_helper = SnapuserdSelinuxHelper::CreateIfNeeded();
    if (snapuserd_helper) {
        snapuserd_helper->StartTransition();
    }

    LoadSelinuxPolicy(policy);

    if (snapuserd_helper) {
        // Before enforcing, finish the pending snapuserd transition.
        snapuserd_helper->FinishTransition();
        snapuserd_helper = nullptr;
    }

    if (selinux_android_restorecon("/dev/selinux/", SELINUX_ANDROID_RESTORECON_RECURSE) == -1) {
        PLOG(FATAL) << "restorecon failed of /dev/selinux failed";
    }

    SelinuxSetEnforcement();


    if (selinux_android_restorecon("/system/bin/init", 0) == -1) {
        PLOG(FATAL) << "restorecon failed of /system/bin/init failed";
    }

    setenv(kEnvSelinuxStartedAt, std::to_string(start_time.time_since_epoch().count()).c_str(), 1);

    const char* path = "/system/bin/init";
    const char* args[] = {path, "second_stage", nullptr};
    execv(path, const_cast<char**>(args));//执行second_stage

    PLOG(FATAL) << "execv(\"" << path << "\") failed";

    return 1;
}
```

selinux是安全权限相关，感兴趣的大家可以自行学习研究。 在执行完成之后回到init 执行了second\_stage。

文件目录:system/core/init/init.cpp 在线地址:

```

int SecondStageMain(int argc, char** argv) {
    if (REBOOT_BOOTLOADER_ON_PANIC) {
        InstallRebootSignalHandlers();
    }
    SetStdioToDevNull(argv);
    InitKernelLogging(argv);
    LOG(INFO) << "init second stage started!";
    if (auto result = WriteFile("/proc/1/oom_score_adj", "-1000"); !result) {
        LOG(ERROR) << "Unable to write -1000 to /proc/1/oom_score_adj: " << result.error();
    }
    GlobalSeccomp();
    keyctl_get_keyring_ID(KEY_SPEC_SESSION_KEYRING, 1);
    close(open("/dev/.booting", O_WRONLY | O_CREAT | O_CLOEXEC, 0000));
    property_init();//初始化属性服务
    process_kernel_dt();
    process_kernel_cmdline();
    export_kernel_boot_props();
    property_set("ro.boottime.init", getenv("INIT_STARTED_AT"));
    property_set("ro.boottime.init.selinux", getenv("INIT_SELINUX_TOOK"));
    const char* avb_version = getenv("INIT_AVB_VERSION");
    if (avb_version) property_set("ro.boot.avb_version", avb_version);
    const char* force_debuggable_env = getenv("INIT_FORCE_DEBUGGABLE");
    if (force_debuggable_env && AvbHandle::IsDeviceUnlocked()) {
        load_debug_prop = "true"s == force_debuggable_env;
    }
    unsetenv("INIT_STARTED_AT");
    unsetenv("INIT_SELINUX_TOOK");
    unsetenv("INIT_AVB_VERSION");
    unsetenv("INIT_FORCE_DEBUGGABLE");
    SelinuxSetupKernelLogging();
    SelabelInitialize();
    SelinuxRestoreContext();
    //创建Epoll
    Epoll epoll;
    if (auto result = epoll.Open(); !result) {
        PLOG(FATAL) << result.error();
    }
    //创建Handler，监听子进程的信号，如果子进程异常退出，init会调用对应的处理函数来进行处理。
    InstallSignalFdHandler(&epoll);
    property_load_boot_defaults(load_debug_prop);
    UmountDebugRamdisk();
    fs_mgr_vendor_overlay_mount_all();
    export_oem_lock_status();
    //开启属性服务
    StartPropertyService(&epoll);
    MountHandler mount_handler(&epoll);
    set_usb_controller();
    const BuiltinFunctionMap function_map;
    Action::set_function_map(&function_map);
    if (!SetupMountNamespaces()) {
        PLOG(FATAL) << "SetupMountNamespaces failed";
    }
    subcontexts = InitializeSubcontexts();
    ActionManager& am = ActionManager::GetInstance();
    ServiceList& sm = ServiceList::GetInstance();
    //加载配置脚本文件 init.rc
    LoadBootScripts(am, sm);
    if (false) DumpState();
    // Make the GSI status available before scripts start running.
    if (android::gsi::IsGsiRunning()) {
        property_set("ro.gsid.image_running", "1");
    } else {
        property_set("ro.gsid.image_running", "0");
    }
    am.QueueBuiltinAction(SetupCgroupsAction, "SetupCgroups");
    am.QueueEventTrigger("early-init");
    // Queue an action that waits for coldboot done so we know ueventd has set up all of /dev...
    am.QueueBuiltinAction(wait_for_coldboot_done_action, "wait_for_coldboot_done");
    // ... so that we can start queuing up actions that require stuff from /dev.
    am.QueueBuiltinAction(MixHwrngIntoLinuxRngAction, "MixHwrngIntoLinuxRng");
    am.QueueBuiltinAction(SetMmapRndBitsAction, "SetMmapRndBits");
    am.QueueBuiltinAction(SetKptrRestrictAction, "SetKptrRestrict");
    Keychords keychords;
    am.QueueBuiltinAction(
        [&epoll, &keychords](const BuiltinArguments& args) -> Result<Success> {
            for (const auto& svc : ServiceList::GetInstance()) {
                keychords.Register(svc->keycodes());
            }
            keychords.Start(&epoll, HandleKeychord);
            return Success();
        },
        "KeychordInit");
    am.QueueBuiltinAction(console_init_action, "console_init");
    // Trigger all the boot actions to get us started.
    am.QueueEventTrigger("init");
    // Starting the BoringSSL self test, for NIAP certification compliance.
    am.QueueBuiltinAction(StartBoringSslSelfTest, "StartBoringSslSelfTest");
    // Repeat mix_hwrng_into_linux_rng in case /dev/hw_random or /dev/random
    // wasn't ready immediately after wait_for_coldboot_done
    am.QueueBuiltinAction(MixHwrngIntoLinuxRngAction, "MixHwrngIntoLinuxRng");
    // Initialize binder before bringing up other system services
    am.QueueBuiltinAction(InitBinder, "InitBinder");
    // Don't mount filesystems or start core system services in charger mode.
    std::string bootmode = GetProperty("ro.bootmode", "");
    if (bootmode == "charger") {
        am.QueueEventTrigger("charger");
    } else {
        am.QueueEventTrigger("late-init");
    }
    // Run all property triggers based on current state of the properties.
    am.QueueBuiltinAction(queue_property_triggers_action, "queue_property_triggers");
    while (true) {
        // By default, sleep until something happens.
        auto epoll_timeout = std::optional<std::chrono::milliseconds>{};
        if (do_shutdown && !shutting_down) {
            do_shutdown = false;
            if (HandlePowerctlMessage(shutdown_command)) {
                shutting_down = true;
            }
        }
        if (!(waiting_for_prop || Service::is_exec_service_running())) {
            am.ExecuteOneCommand();
        }
        if (!(waiting_for_prop || Service::is_exec_service_running())) {
            if (!shutting_down) {
                auto next_process_action_time = HandleProcessActions();
                // If there's a process that needs restarting, wake up in time for that.
                if (next_process_action_time) {
                    epoll_timeout = std::chrono::ceil<std::chrono::milliseconds>(
                            *next_process_action_time - boot_clock::now());
                    if (*epoll_timeout < 0ms) epoll_timeout = 0ms;
                }
            }
            // If there's more work to do, wake up again immediately.
            if (am.HasMoreCommands()) epoll_timeout = 0ms;
        }
        if (auto result = epoll.Wait(epoll_timeout); !result) {
            LOG(ERROR) << result.error();
        }
    }
    return 0;
}

```

代码比较多，我们只需要看关键的几点 我都加了注释： 1.property\_init。属性服务类似于Windows平台上的注册表管理器，主要记录一些用户、软件的使用信息。当系统启动、重启 都是根据之前的记录进行对应的初始化。

2.创建Handler，监听子进程的信号，如果子进程异常退出，init会调用对应的处理函数来进行处理。

3.创建Epoll。

4.开启属性服务。

5.解析init.rc配置文件。

顺便提一下，Epoll面试大家遇见的也比较多。后边我会解释Epoll。

下面我们分别看一下 这几个函数都做了什么。

## 1.属性服务的初始化 ——> property\_init

```
void property_init() {
    mkdir("/dev/__properties__", S_IRWXU | S_IXGRP | S_IXOTH);
    CreateSerializedPropertyInfo();//创建了配置的info
    if (__system_property_area_init()) {//初始化property的内存区域。
        LOG(FATAL) << "Failed to initialize property area";
    }
    if (!property_info_area.LoadDefaultPath()) {
        LOG(FATAL) << "Failed to load serialized property info file";
    }
}

```

总结：创建了Property的info信息。并且初始化了property的内存区域。

## 2.开启属性服务 -->StartPropertyService

文件目录:/system/core/init/property\_service.cpp 在线文档:[cs.android.com/android/pla…](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid10-release%3Asystem%2Fcore%2Finit%2Fproperty_service.cpp "https://cs.android.com/android/platform/superproject/+/android10-release:system/core/init/property_service.cpp")

```
void StartPropertyService(Epoll* epoll) {
    selinux_callback cb;
    cb.func_audit = SelinuxAuditCallback;
    //设置selinux的callback
    selinux_set_callback(SELINUX_CB_AUDIT, cb);

    property_set("ro.property_service.version", "2");
     //创建属性的非阻塞式socket
    property_set_fd = CreateSocket(PROP_SERVICE_NAME, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
                                   false, 0666, 0, 0, nullptr);
    if (property_set_fd == -1) {
        PLOG(FATAL) << "start_property_service socket creation failed";
    }
    //listen对property_set_fd进行监听,最多可以同时为8个链接提供服务。
    listen(property_set_fd, 8);
     //epoll 注册Handler，监听property_set_fd，当有更新的时候会调用handle_property_set_fd来进行处理。
    if (auto result = epoll->RegisterHandler(property_set_fd, handle_property_set_fd); !result) {
        PLOG(FATAL) << result.error();
    }
}


//属性事件处理器
static void handle_property_set_fd() {
    static constexpr uint32_t kDefaultSocketTimeout = 2000; /* ms */ //设置超时时长

    int s = accept4(property_set_fd, nullptr, nullptr, SOCK_CLOEXEC);
    if (s == -1) {
        return;
    }

    ucred cr;
    socklen_t cr_size = sizeof(cr);
    if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, &cr, &cr_size) < 0) {
        close(s);
        PLOG(ERROR) << "sys_prop: unable to get SO_PEERCRED";
        return;
    }

    SocketConnection socket(s, cr);
    uint32_t timeout_ms = kDefaultSocketTimeout;

    uint32_t cmd = 0;
    if (!socket.RecvUint32(&cmd, &timeout_ms)) {//接收数据
        PLOG(ERROR) << "sys_prop: error while reading command from the socket";
        socket.SendUint32(PROP_ERROR_READ_CMD);
        return;
    }

    switch (cmd) {
    case PROP_MSG_SETPROP: {//设置属性事件
        char prop_name[PROP_NAME_MAX];
        char prop_value[PROP_VALUE_MAX];
        //从socket中读取name 和 value
        if (!socket.RecvChars(prop_name, PROP_NAME_MAX, &timeout_ms) ||
            !socket.RecvChars(prop_value, PROP_VALUE_MAX, &timeout_ms)) {
          PLOG(ERROR) << "sys_prop(PROP_MSG_SETPROP): error while reading name/value from the socket";
          return;
        }

        prop_name[PROP_NAME_MAX-1] = 0;
        prop_value[PROP_VALUE_MAX-1] = 0;

        const auto& cr = socket.cred();
        std::string error;
        uint32_t result =
            HandlePropertySet(prop_name, prop_value, socket.source_context(), cr, &error);//设置属性
        if (result != PROP_SUCCESS) {
            LOG(ERROR) << "Unable to set property '" << prop_name << "' to '" << prop_value
                       << "' from uid:" << cr.uid << " gid:" << cr.gid << " pid:" << cr.pid << ": "
                       << error;
        }

        break;
      }

    case PROP_MSG_SETPROP2: {//同上
        std::string name;
        std::string value;
        if (!socket.RecvString(&name, &timeout_ms) ||
            !socket.RecvString(&value, &timeout_ms)) {
          PLOG(ERROR) << "sys_prop(PROP_MSG_SETPROP2): error while reading name/value from the socket";
          socket.SendUint32(PROP_ERROR_READ_DATA);
          return;
        }

        const auto& cr = socket.cred();
        std::string error;
        uint32_t result = HandlePropertySet(name, value, socket.source_context(), cr, &error);
        if (result != PROP_SUCCESS) {
            LOG(ERROR) << "Unable to set property '" << name << "' to '" << value
                       << "' from uid:" << cr.uid << " gid:" << cr.gid << " pid:" << cr.pid << ": "
                       << error;
        }
        socket.SendUint32(result);
        break;
      }

    default:
        LOG(ERROR) << "sys_prop: invalid command " << cmd;
        socket.SendUint32(PROP_ERROR_INVALID_CMD);
        break;
    }
}

//设置属性
uint32_t HandlePropertySet(const std::string& name, const std::string& value,
                           const std::string& source_context, const ucred& cr, std::string* error) {
    if (auto ret = CheckPermissions(name, value, source_context, cr, error); ret != PROP_SUCCESS) {
        return ret;
    }

    if (StartsWith(name, "ctl.")) {//控制属性的处理
        HandleControlMessage(name.c_str() + 4, value, cr.pid);
        return PROP_SUCCESS;
    }

    // sys.powerctl is a special property that is used to make the device reboot.  We want to log
    // any process that sets this property to be able to accurately blame the cause of a shutdown.
    if (name == "sys.powerctl") {//记录重启设备原因的属性
        std::string cmdline_path = StringPrintf("proc/%d/cmdline", cr.pid);
        std::string process_cmdline;
        std::string process_log_string;
        if (ReadFileToString(cmdline_path, &process_cmdline)) {
            // Since cmdline is null deliminated, .c_str() conveniently gives us just the process
            // path.
            process_log_string = StringPrintf(" (%s)", process_cmdline.c_str());
        }
        LOG(INFO) << "Received sys.powerctl='" << value << "' from pid: " << cr.pid
                  << process_log_string;
    }

    if (name == "selinux.restorecon_recursive") {
        return PropertySetAsync(name, value, RestoreconRecursiveAsync, error);
    }
    //设置普通属性
    return PropertySet(name, value, error);
}
我们只看普通属性，因为控制属性需要客户端的权限。


static uint32_t PropertySet(const std::string& name, const std::string& value, std::string* error) {
    size_t valuelen = value.size();

    if (!IsLegalPropertyName(name)) {
        *error = "Illegal property name";
        return PROP_ERROR_INVALID_NAME;
    }

    if (valuelen >= PROP_VALUE_MAX && !StartsWith(name, "ro.")) {
        *error = "Property value too long";
        return PROP_ERROR_INVALID_VALUE;
    }

    if (mbstowcs(nullptr, value.data(), 0) == static_cast<std::size_t>(-1)) {
        *error = "Value is not a UTF8 encoded string";
        return PROP_ERROR_INVALID_VALUE;
    }
    //从属性内存空间查找，属性存在就更新属性值，否则创建并且添加进去。
    prop_info* pi = (prop_info*) __system_property_find(name.c_str());
    if (pi != nullptr) {
        // ro.* properties are actually "write-once".
        if (StartsWith(name, "ro.")) {
            *error = "Read-only property was already set";
            return PROP_ERROR_READ_ONLY_PROPERTY;
        }

        __system_property_update(pi, value.c_str(), valuelen);
    } else {
        int rc = __system_property_add(name.c_str(), name.size(), value.c_str(), valuelen);
        if (rc < 0) {
            *error = "__system_property_add failed";
            return PROP_ERROR_SET_FAILED;
        }
    }

    if (persistent_properties_loaded && StartsWith(name, "persist.")) {
        WritePersistentProperty(name, value);
    }
    property_changed(name, value);
    return PROP_SUCCESS;
}

```

总结：创建Android的注册表，将一些用户、应用的信息存储下来 系统会根据这些属性进行初始化工作。创建了property的socket 并且将socket注册到Epoll中，在handle\_property\_set\_fd中将属性添加到内存空间中去。

## 3.加载init.rc配置文件 --> LoadBootScripts

```
static void LoadBootScripts(ActionManager& action_manager, ServiceList& service_list) {
    Parser parser = CreateParser(action_manager, service_list);//创建解析器

    std::string bootscript = GetProperty("ro.boot.init_rc", "");//根据ro.boot.init_rc的属性值来获取需要执行的脚本
    if (bootscript.empty()) {
        parser.ParseConfig("/init.rc");
        if (!parser.ParseConfig("/system/etc/init")) {
            late_import_paths.emplace_back("/system/etc/init");
        }
        if (!parser.ParseConfig("/product/etc/init")) {
            late_import_paths.emplace_back("/product/etc/init");
        }
        if (!parser.ParseConfig("/product_services/etc/init")) {
            late_import_paths.emplace_back("/product_services/etc/init");
        }
        if (!parser.ParseConfig("/odm/etc/init")) {
            late_import_paths.emplace_back("/odm/etc/init");
        }
        if (!parser.ParseConfig("/vendor/etc/init")) {
            late_import_paths.emplace_back("/vendor/etc/init");
        }
    } else {
        parser.ParseConfig(bootscript);
    }
}

//1.根据action on  和import类型的不同创建不同的解析器
Parser CreateParser(ActionManager& action_manager, ServiceList& service_list) {
    Parser parser;

    parser.AddSectionParser("service", std::make_unique<ServiceParser>(
                                               &service_list, GetSubcontext(), std::nullopt));
    parser.AddSectionParser("on", std::make_unique<ActionParser>(&action_manager, GetSubcontext()));
    parser.AddSectionParser("import", std::make_unique<ImportParser>(&parser));

    return parser;
}

/*2.读取init.rc文件 在init.rc中根据系统导入对应的zygote.rc  在init进程中创建Zygote进程，该进程的可执行文件在/system/bin/app_process  并且传入参数 -Xzygote /system/bin --zygote --start-system-server 指定classname=main */


service zygote /system/bin/app_process -Xzygote /system/bin --zygote --start-system-server
    class main
    priority -20
    user root
    group root readproc reserved_disk
    socket zygote stream 660 root system
    socket usap_pool_primary stream 660 root system
    onrestart write /sys/android_power/request_state wake
    onrestart write /sys/power/state on
    onrestart restart audioserver
    onrestart restart cameraserver
    onrestart restart media
    onrestart restart netd
    onrestart restart wificond
    writepid /dev/cpuset/foreground/tasks
    
    
3.解析器根据文件的格式来执行对应的动作。
文件目录：system/core/init/service.cpp
在线地址:https://cs.android.com/android/platform/superproject/+/android10-release:system/core/init/service.cpp

//开始解析
Result<Success> ServiceParser::ParseSection(std::vector<std::string>&& args,
                                            const std::string& filename, int line) {
    if (args.size() < 3) {//判断可执行参数 我们传递的参数大于3个的 所以不会到这里来。
        return Error() << "services must have a name and a program";
    }

    const std::string& name = args[1];
    if (!IsValidName(name)) {//校验Name
        return Error() << "invalid service name '" << name << "'";
    }

    filename_ = filename;

    Subcontext* restart_action_subcontext = nullptr;
    if (subcontexts_) {
        for (auto& subcontext : *subcontexts_) {
            if (StartsWith(filename, subcontext.path_prefix())) {
                restart_action_subcontext = &subcontext;
                break;
            }
        }
    }

    std::vector<std::string> str_args(args.begin() + 2, args.end());

    if (SelinuxGetVendorAndroidVersion() <= __ANDROID_API_P__) {
        if (str_args[0] == "/sbin/watchdogd") {
            str_args[0] = "/system/bin/watchdogd";
        }
    }
    //创建service对象
    service_ = std::make_unique<Service>(name, restart_action_subcontext, str_args);
    return Success();
}

//解析完成
Result<Success> ServiceParser::EndSection() {
    if (service_) {//如果service存在就将原来的service删除 再添加新的
        Service* old_service = service_list_->FindService(service_->name());
        if (old_service) {
            if (!service_->is_override()) {
                return Error() << "ignored duplicate definition of service '" << service_->name()
                               << "'";
            }

            if (StartsWith(filename_, "/apex/") && !old_service->is_updatable()) {
                return Error() << "cannot update a non-updatable service '" << service_->name()
                               << "' with a config in APEX";
            }

            service_list_->RemoveService(*old_service);
            old_service = nullptr;
        }
        //调用service_list->addService函数进行添加
        service_list_->AddService(std::move(service_));
    }

    return Success();
}

解析完成后需要启动 在init.rc中我们找到对应的启动 之前说到的class = main  而class_start是一个command
on nonencrypted
    class_start main
    class_start late_start
    
 文件目录: /android/system/core/init   
 
static Result<Success> do_class_start(const BuiltinArguments& args) {
    // Do not start a class if it has a property persist.dont_start_class.CLASS set to 1.
    if (android::base::GetBoolProperty("persist.init.dont_start_class." + args[1], false))
        return Success();
    // Starting a class does not start services which are explicitly disabled.
    // They must  be started individually.
    for (const auto& service : ServiceList::GetInstance()) {//遍历serviceList 
        if (service->classnames().count(args[1])) {
            if (auto result = service->StartIfNotDisabled(); !result) {//执行startIfNotDisable函数
                LOG(ERROR) << "Could not start service '" << service->name()
                           << "' as part of class '" << args[1] << "': " << result.error();
            }
        }
    }
    return Success();
}

文件目录:/system/core/init/service.cpp
Result<Success> Service::StartIfNotDisabled() {
    if (!(flags_ & SVC_DISABLED)) { //判断有没有在init.rc中配置disabled
        return Start();//开启Zygote
    } else {
        flags_ |= SVC_DISABLED_START;
    }
    return Success();
}


//调用start 开启zygote服务
Result<Success> Service::Start() {
    if (is_updatable() && !ServiceList::GetInstance().IsServicesUpdated()) {
        ServiceList::GetInstance().DelayService(*this);
        return Error() << "Cannot start an updatable service '" << name_
                       << "' before configs from APEXes are all loaded. "
                       << "Queued for execution.";
    }

    bool disabled = (flags_ & (SVC_DISABLED | SVC_RESET));
    flags_ &= (~(SVC_DISABLED|SVC_RESTARTING|SVC_RESET|SVC_RESTART|SVC_DISABLED_START));

    if (flags_ & SVC_RUNNING) {//如果服务已经开启 就return Success
        if ((flags_ & SVC_ONESHOT) && disabled) {
            flags_ |= SVC_RESTART;
        }
        // It is not an error to try to start a service that is already running.
        return Success();
    }

    bool needs_console = (flags_ & SVC_CONSOLE);
    if (needs_console) {
        if (console_.empty()) {
            console_ = default_console;
        }

        int console_fd = open(console_.c_str(), O_RDWR | O_CLOEXEC);
        if (console_fd < 0) {
            flags_ |= SVC_DISABLED;
            return ErrnoError() << "Couldn't open console '" << console_ << "'";
        }
        close(console_fd);
    }

    struct stat sb;
    //判断对应的执行文件是否存在也就算zygote.rc中的/system/bin/app_process。手机中的/system/bin/app_process文件。
    if (stat(args_[0].c_str(), &sb) == -1) {
        flags_ |= SVC_DISABLED;
        return ErrnoError() << "Cannot find '" << args_[0] << "'";
    }

    std::string scon;
    if (!seclabel_.empty()) {
        scon = seclabel_;
    } else {
        auto result = ComputeContextFromExecutable(args_[0]);
        if (!result) {
            return result.error();
        }
        scon = *result;
    }

    if (!IsRuntimeApexReady() && !pre_apexd_) {
        pre_apexd_ = true;
    }

    post_data_ = ServiceList::GetInstance().IsPostData();

    LOG(INFO) << "starting service '" << name_ << "'...";
    //判断进程是否启动，没有启动就fork子进程。所以zygote是init的子进程。
    pid_t pid = -1;
    if (namespace_flags_) {
        pid = clone(nullptr, nullptr, namespace_flags_ | SIGCHLD, nullptr);
    } else {
        pid = fork();
    }

    if (pid == 0) {//子进程(zygote)中执行的函数
        umask(077);

        if (auto result = EnterNamespaces(); !result) {
            LOG(FATAL) << "Service '" << name_ << "' could not enter namespaces: " << result.error();
        }

        if (namespace_flags_ & CLONE_NEWNS) {
            if (auto result = SetUpMountNamespace(); !result) {
                LOG(FATAL) << "Service '" << name_
                           << "' could not set up mount namespace: " << result.error();
            }
        }

        if (namespace_flags_ & CLONE_NEWPID) {
            if (auto result = SetUpPidNamespace(); !result) {
                LOG(FATAL) << "Service '" << name_
                           << "' could not set up PID namespace: " << result.error();
            }
        }

        for (const auto& [key, value] : environment_vars_) {
            setenv(key.c_str(), value.c_str(), 1);
        }

        std::for_each(descriptors_.begin(), descriptors_.end(),
                      std::bind(&DescriptorInfo::CreateAndPublish, std::placeholders::_1, scon));

        std::string cpuset_path;
        if (CgroupGetControllerPath("cpuset", &cpuset_path)) {
            auto cpuset_predicate = [&cpuset_path](const std::string& path) {
                return StartsWith(path, cpuset_path + "/");
            };
            auto iter =
                    std::find_if(writepid_files_.begin(), writepid_files_.end(), cpuset_predicate);
            if (iter == writepid_files_.end()) {
                std::string default_cpuset = GetProperty("ro.cpuset.default", "");
                if (!default_cpuset.empty()) {
                    if (default_cpuset.front() != '/') {
                        default_cpuset.insert(0, 1, '/');
                    }
                    if (default_cpuset.back() != '/') {
                        default_cpuset.push_back('/');
                    }
                    writepid_files_.push_back(
                            StringPrintf("%s%stasks", cpuset_path.c_str(), default_cpuset.c_str()));
                }
            }
        } else {
            LOG(ERROR) << "cpuset cgroup controller is not mounted!";
        }
        std::string pid_str = std::to_string(getpid());
        for (const auto& file : writepid_files_) {
            if (!WriteStringToFile(pid_str, file)) {
                PLOG(ERROR) << "couldn't write " << pid_str << " to " << file;
            }
        }

        if (ioprio_class_ != IoSchedClass_NONE) {
            if (android_set_ioprio(getpid(), ioprio_class_, ioprio_pri_)) {
                PLOG(ERROR) << "failed to set pid " << getpid()
                            << " ioprio=" << ioprio_class_ << "," << ioprio_pri_;
            }
        }

        if (needs_console) {
            setsid();
            OpenConsole();
        } else {
            ZapStdio();
        }

        SetProcessAttributes();
        //调用execv函数，启动service子进程
        if (!ExpandArgsAndExecv(args_, sigstop_)) {
            PLOG(ERROR) << "cannot execve('" << args_[0] << "')";
        }

        _exit(127);
    }

    if (pid < 0) {
        pid_ = 0;
        return ErrnoError() << "Failed to fork";
    }

    if (oom_score_adjust_ != -1000) {
        std::string oom_str = std::to_string(oom_score_adjust_);
        std::string oom_file = StringPrintf("/proc/%d/oom_score_adj", pid);
        if (!WriteStringToFile(oom_str, oom_file)) {
            PLOG(ERROR) << "couldn't write oom_score_adj";
        }
    }

    time_started_ = boot_clock::now();
    pid_ = pid;
    flags_ |= SVC_RUNNING;
    start_order_ = next_start_order_++;
    process_cgroup_empty_ = false;

    bool use_memcg = swappiness_ != -1 || soft_limit_in_bytes_ != -1 || limit_in_bytes_ != -1 ||
                      limit_percent_ != -1 || !limit_property_.empty();
    errno = -createProcessGroup(uid_, pid_, use_memcg);
    if (errno != 0) {
        PLOG(ERROR) << "createProcessGroup(" << uid_ << ", " << pid_ << ") failed for service '"
                    << name_ << "'";
    } else if (use_memcg) {
        if (swappiness_ != -1) {
            if (!setProcessGroupSwappiness(uid_, pid_, swappiness_)) {
                PLOG(ERROR) << "setProcessGroupSwappiness failed";
            }
        }

        if (soft_limit_in_bytes_ != -1) {
            if (!setProcessGroupSoftLimit(uid_, pid_, soft_limit_in_bytes_)) {
                PLOG(ERROR) << "setProcessGroupSoftLimit failed";
            }
        }

        size_t computed_limit_in_bytes = limit_in_bytes_;
        if (limit_percent_ != -1) {
            long page_size = sysconf(_SC_PAGESIZE);
            long num_pages = sysconf(_SC_PHYS_PAGES);
            if (page_size > 0 && num_pages > 0) {
                size_t max_mem = SIZE_MAX;
                if (size_t(num_pages) < SIZE_MAX / size_t(page_size)) {
                    max_mem = size_t(num_pages) * size_t(page_size);
                }
                computed_limit_in_bytes =
                        std::min(computed_limit_in_bytes, max_mem / 100 * limit_percent_);
            }
        }

        if (!limit_property_.empty()) {
            computed_limit_in_bytes = android::base::GetUintProperty(
                    limit_property_, computed_limit_in_bytes, SIZE_MAX);
        }

        if (computed_limit_in_bytes != size_t(-1)) {
            if (!setProcessGroupLimit(uid_, pid_, computed_limit_in_bytes)) {
                PLOG(ERROR) << "setProcessGroupLimit failed";
            }
        }
    }

    NotifyStateChange("running");
    return Success();
}

static bool ExpandArgsAndExecv(const std::vector<std::string>& args, bool sigstop) {
    std::vector<std::string> expanded_args;
    std::vector<char*> c_strings;

    expanded_args.resize(args.size());
    c_strings.push_back(const_cast<char*>(args[0].data()));
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (!expand_props(args[i], &expanded_args[i])) {
            LOG(FATAL) << args[0] << ": cannot expand '" << args[i] << "'";
        }
        c_strings.push_back(expanded_args[i].data());
    }
    c_strings.push_back(nullptr);

    if (sigstop) {
        kill(getpid(), SIGSTOP);
    }
    //执行app_process 也就是调用app_main.cpp的main函数 这样就进入到zygote了。
    return execv(c_strings[0], c_strings.data()) == 0;
}


```

init.rc是一个配置文件，主要包含了5中类型语句：Action、Command、Service、Option和Import。具体的语法大家可以参考ReadMe 里面写的非常详细。

文件地址:[cs.android.com/android/pla…](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid10-release%3Asystem%2Fcore%2Frootdir%2Finit.rc "https://cs.android.com/android/platform/superproject/+/android10-release:system/core/rootdir/init.rc")

总结：加载并解析init.rc 根据rc文件创建Service对象，最终将service添加到service\_List中，然后解析到on 执行 class\_start的时候会开启服务调用Service::Start函数,fork子进程，并且执行app\_process文件 开启了zygote，也就到了zygote的main函数中了。 zygote的main函数如下：

```

int main(int argc, char* const argv[])
{
    if (!LOG_NDEBUG) {
      String8 argv_String;
      for (int i = 0; i < argc; ++i) {
        argv_String.append("\"");
        argv_String.append(argv[i]);
        argv_String.append("\" ");
      }
      ALOGV("app_process main with argv: %s", argv_String.string());
    }

    AppRuntime runtime(argv[0], computeArgBlockSize(argc, argv));
    argc--;
    argv++;

    const char* spaced_commands[] = { "-cp", "-classpath" };
    // Allow "spaced commands" to be succeeded by exactly 1 argument (regardless of -s).
    bool known_command = false;

    int i;
    for (i = 0; i < argc; i++) {
        if (known_command == true) {
          runtime.addOption(strdup(argv[i]));
          ALOGV("app_process main add known option '%s'", argv[i]);
          known_command = false;
          continue;
        }

        for (int j = 0;
             j < static_cast<int>(sizeof(spaced_commands) / sizeof(spaced_commands[0]));
             ++j) {
          if (strcmp(argv[i], spaced_commands[j]) == 0) {
            known_command = true;
            ALOGV("app_process main found known command '%s'", argv[i]);
          }
        }

        if (argv[i][0] != '-') {
            break;
        }
        if (argv[i][1] == '-' && argv[i][2] == 0) {
            ++i; // Skip --.
            break;
        }

        runtime.addOption(strdup(argv[i]));
        ALOGV("app_process main add option '%s'", argv[i]);
    }

    bool zygote = false;
    bool startSystemServer = false;
    bool application = false;
    String8 niceName;
    String8 className;

    ++i;  // Skip unused "parent dir" argument.
    while (i < argc) {
        const char* arg = argv[i++];
        if (strcmp(arg, "--zygote") == 0) {
            zygote = true;
            niceName = ZYGOTE_NICE_NAME;
        } else if (strcmp(arg, "--start-system-server") == 0) {
            startSystemServer = true;
        } else if (strcmp(arg, "--application") == 0) {
            application = true;
        } else if (strncmp(arg, "--nice-name=", 12) == 0) {
            niceName.setTo(arg + 12);
        } else if (strncmp(arg, "--", 2) != 0) {
            className.setTo(arg);
            break;
        } else {
            --i;
            break;
        }
    }

    Vector<String8> args;
    if (!className.isEmpty()) {
        args.add(application ? String8("application") : String8("tool"));
        runtime.setClassNameAndArgs(className, argc - i, argv + i);

        if (!LOG_NDEBUG) {
          String8 restOfArgs;
          char* const* argv_new = argv + i;
          int argc_new = argc - i;
          for (int k = 0; k < argc_new; ++k) {
            restOfArgs.append("\"");
            restOfArgs.append(argv_new[k]);
            restOfArgs.append("\" ");
          }
          ALOGV("Class name = %s, args = %s", className.string(), restOfArgs.string());
        }
    } else {
        maybeCreateDalvikCache();

        if (startSystemServer) {
            args.add(String8("start-system-server"));
        }

        char prop[PROP_VALUE_MAX];
        if (property_get(ABI_LIST_PROPERTY, prop, NULL) == 0) {
            LOG_ALWAYS_FATAL("app_process: Unable to determine ABI list from property %s.",
                ABI_LIST_PROPERTY);
            return 11;
        }

        String8 abiFlag("--abi-list=");
        abiFlag.append(prop);
        args.add(abiFlag);

        for (; i < argc; ++i) {
            args.add(String8(argv[i]));
        }
    }

    if (!niceName.isEmpty()) {
        runtime.setArgv0(niceName.string(), true /* setProcName */);
    }

    if (zygote) {
        runtime.start("com.android.internal.os.ZygoteInit", args, zygote);
    } else if (className) {
        runtime.start("com.android.internal.os.RuntimeInit", args, zygote);
    } else {
        fprintf(stderr, "Error: no class name or --zygote supplied.\n");
        app_usage();
        LOG_ALWAYS_FATAL("app_process: no class name or --zygote supplied.");
    }
}

```

这样init的就唤起了Zygote了。

## 补充

关于Epoll，Select,Poll。这个在面试中经常会被问到。这里解释下：在Linux中，监听文件（I/O多路复用）。

select采用数组存储，查找起来就很慢 当有更新时需要遍历数组，每遍历一个socket需要切换一次上下文。而且有1024的数量限制。

poll采用链表存储，查找起来依旧很慢，当有更新时也需要遍历链表，每遍历一个socket需要切换一次上下文。但是打破了数量限制。

epoll来替换掉了select、poll。epoll他的执行效率大于select、poll，是因为epoll内部采用的是红黑树来存储的，查找速度快能够在大量并发连接中只有少量活跃的情况下高效处理事件。并且epoll是对事件敏感的，当有数据来的时候会把socket存放到rdlist，然后根据rdlist找到对应的socket进行回调，不需要进行频繁的上下文切换，所以效率非常高。

## 总结一下：

1.挂载文件系统，创建目录，初始化属性服务(类似Windows的注册表，记录一些用户信息、应用信息等，系统启动起来后，根据属性进行初始化)，Epoll机制来监听属性的变化，在 `InstallSignalFdHandler` 中做对应的处理。

/system/core/init/main.cpp:main();->FirstStageMain();->SetupSelinux(); SecondStageMain();

first\_stage\_init.cpp:FirstStageMain()挂载文件系统，创建目录

init.cpp:SecondStageMain()->property\_init(),StartPropertyService()初始化属性服务，开启属性服务，监听属性服务。

2.加载rc配置文件,读取init.rc 根据对应的语句执行对应的动作。在init\_zygotexx.rc中配置了Zygote相关命令。其中class为main，当解析到on语句的时候会调用class\_start(do\_class\_start)遍历服务列表找到对应服务 开启 调用Service::Start 判断对应的app\_process文件是否存在，并fork子进程 执行对应的可执行文件app\_process，这样就把Zygote拉起来了。

init.cpp:SecondStageMain()->LoadBootScripts()

service.cpp:do\_class\_start()->StartIfNotDisable()->Start()->ExpandArgsAndExecv()->execv(app\_process);

流程图：

![init_sequence.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/fef3ed0025704e28b33509b5c7b7627a~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

有关于Zygote的，就下次再详聊。

在线视频:[www.bilibili.com/video/BV1TM…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1TM4y1z7FL%2F%3Fshare_source%3Dcopy_web%26vd_source%3D0cf6c1c4827d129d9daea3a51b663710 "https://www.bilibili.com/video/BV1TM4y1z7FL/?share_source=copy_web&vd_source=0cf6c1c4827d129d9daea3a51b663710")

视频地址：[pan.baidu.com/s/1NfYipU9Z…](https://link.juejin.cn/?target=https%3A%2F%2Fpan.baidu.com%2Fs%2F1NfYipU9ZEXgarTYusPfr9A "https://pan.baidu.com/s/1NfYipU9ZEXgarTYusPfr9A")

提取码: aztj