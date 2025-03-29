> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

第一阶段的代码入口是FirstStageMain，我们使用这张图思维导图总结其所做的事情

![第一阶段思维导图.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/8109397017ab4cc79449096d12a898a8~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240324&x-signature=MXY0ccEjjDA8VnSvrnPC5H%2Bk3ww%3D) 主要分为如下几部分讲解：

-   InstallRebootSignalHandlers
-   挂载文件系统以及创建一些文件或目录
-   init进程日志
-   selinux初始化

## InstallRebootSignalHandlers

```
int FirstStageMain(int argc, char** argv) {
    if (REBOOT_BOOTLOADER_ON_PANIC) {
        InstallRebootSignalHandlers();
    }
}
```

InstallRebootSignalHandlers() 将各种信号量，如 SIGABRT,SIGBUS 等的行为设置为 SA\_RESTART，一旦监听到这些信号即执行重启系统（关于信号量属于Linux开发相关内容，不了解这块知识也没关系，大家了解一下这个函数的功能即可）：

```
void InstallRebootSignalHandlers() {
    // Instead of panic'ing the kernel as is the default behavior when init crashes,
    // we prefer to reboot to bootloader on development builds, as this will prevent
    // boot looping bad configurations and allow both developers and test farms to easily
    // recover.

    // 当init崩溃时，不会像默认行为那样恐慌内核，我们更喜欢在开发构建时重启到bootloader，因为这将防止引导错误的配置，让开发人员和测试人员都可以轻松地进行配置恢复。

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    sigfillset(&action.sa_mask);
    action.sa_handler = [](int signal) {
        // These signal handlers are also caught for processes forked from init, however we do not
        // want them to trigger reboot, so we directly call _exit() for children processes here.

        // 从 init 派生的进程也会捕获这些信号处理程序，但我们不会希望它们触发重启，所以我们在这里直接对子进程调用_exit()
        if (getpid() != 1) {
            _exit(signal);
        }

        // Calling DoReboot() or LOG(FATAL) is not a good option as this is a signal handler.
        // RebootSystem uses syscall() which isn't actually async-signal-safe, but our only option
        // and probably good enough given this is already an error case and only enabled for
        // development builds.

        // 调用DoReboot()或LOG(FATAL)不是一个好的选择，因为这是一个信号处理程序。重启系统使用syscall()，这实际上不是异步信号安全的，但我们唯一的选择,考虑到这已经是一个错误情况，并且只在开发构建时启用，这可能已经足够了。
        InitFatalReboot();
    };
    action.sa_flags = SA_RESTART;
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

## 文件系统相关

```
int FirstStageMain(int argc, char** argv) {
...省略代码

// Clear the umask. umask(0);
CHECKCALL(clearenv());
CHECKCALL(setenv("PATH", _PATH_DEFPATH, 1));
// Get the basic filesystem setup we need put together in the initramdisk
// on / and then we'll let the rc file figure out the rest. //将 tmpfs 文件系统挂载到 dev 目录
CHECKCALL(mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755"));
// 创建 dev/pts 目录 ：是远程登陆(telnet,ssh 等)后创建的控制台设备文件所在的目录
CHECKCALL(mkdir("/dev/pts", 0755));
//创建 dev/socket 目录，rc 脚本中 service 启动时，会创建很多域套接字在此目录
CHECKCALL(mkdir("/dev/socket", 0755));
CHECKCALL(mount("devpts", "/dev/pts", "devpts", 0, NULL));
#define MAKE_STR(x) __STRING(x)
CHECKCALL(mount("proc", "/proc", "proc", 0, "hidepid=2,gid=" MAKE_STR(AID_READPROC)));
#undef MAKE_STR
// Don't expose the raw commandline to unprivileged processes. CHECKCALL(chmod("/proc/cmdline", 0440));
gid_t groups[] = {AID_READPROC};
CHECKCALL(setgroups(arraysize(groups), groups));
//将 sysfs 文件系统挂载到 sys 目录，用来访问内核和驱动的信息
CHECKCALL(mount("sysfs", "/sys", "sysfs", 0, NULL));
//将 selinuxfs 文件系统挂载到目录/sys/fs/selinux ,通过该路径可以完成和内核 selinux 模块交互
CHECKCALL(mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL));
//创建/dev/kmsg 设备节点，用于用户空间写入日志到内核日志缓冲区中
CHECKCALL(mknod("/dev/kmsg", S_IFCHR | 0600, makedev(1, 11)));
if constexpr (WORLD_WRITABLE_KMSG) {
CHECKCALL(mknod("/dev/kmsg_debug", S_IFCHR | 0622, makedev(1, 11)));
}
//节点/dev/random 和/dev/urandom 是 Linux 系统中提供的随机伪设备，
//这两个设备的任务，是提供永不为空的随机字节数据流。
//很多解密程序与安全应用程序（如 SSH Keys,SSL Keys 等）需要它们提供的随机数据流。
CHECKCALL(mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8)));
CHECKCALL(mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9)));
// This is needed for log wrapper, which gets called before ueventd runs. // 创建伪终端
CHECKCALL(mknod("/dev/ptmx", S_IFCHR | 0666, makedev(5, 2)));
// 创建空设备
CHECKCALL(mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3)));
// These below mounts are done in first stage init so that first stage mount can mount
// subdirectories of /mnt/{vendor,product}/. Other mounts, not required by first stage mount, // should be done in rc files. // Mount staging areas for devices managed by vold
// See storage config details at http://source.android.com/devices/storage/
//将 tmpfs 文件系统到 mnt 目录，这个目录正常是挂载光驱，usb 设备的
CHECKCALL(mount("tmpfs", "/mnt", "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV, "mode=0755,uid=0,gid=1000"));
// /mnt/vendor is used to mount vendor-specific partitions that can not be
// part of the vendor partition, e.g. because they are mounted read-write. CHECKCALL(mkdir("/mnt/vendor", 0755));
// /mnt/product is used to mount product-specific partitions that can not be
// part of the product partition, e.g. because they are mounted read-write. CHECKCALL(mkdir("/mnt/product", 0755));
// /apex is used to mount APEXes
CHECKCALL(mount("tmpfs", "/apex", "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV, "mode=0755,uid=0,gid=0"));
// /debug_ramdisk is used to preserve additional files from the debug ramdisk
CHECKCALL(mount("tmpfs", "/debug_ramdisk", "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV, "mode=0755,uid=0,gid=0"));

...省略代码
}
```

这一块代码的主要作用就是在根文件系统中挂载文件系统以及创建一些文件或目录。

## init进程日志

```
int FirstStageMain(int argc, char** argv) {
...省略代码

    SetStdioToDevNull(argv);
    // Now that tmpfs is mounted on /dev and we have /dev/kmsg, we can actually
    // talk to the outside world...
    InitKernelLogging(argv);

    if (!errors.empty()) {
        for (const auto& [error_string, error_errno] : errors) {
            LOG(ERROR) << error_string << " " << strerror(error_errno);
        }
        LOG(FATAL) << "Init encountered errors starting first stage, aborting";
    }

    LOG(INFO) << "init first stage started!";

...省略代码
}
```

上面的代码调用了SetStdioToDevNull和InitKernelLogging，然后调用LOG输出一行日志

```
"init first stage started!"
```

很显然前面是一些准备工作，这里才开始一阶段的初始化，并且只有调用了InitKernelLogging后，在init进程中才能调用LOG进行log输出。

关于InitKernelLogging代码的分析我将会在《[Android Framework—Init进程—4.init进程日志](https://juejin.cn/post/7419482499598532618 "https://juejin.cn/post/7419482499598532618")》此文章中单独讲解。

## selinux初始化

```
int FirstStageMain(int argc, char** argv) {
...省略代码

    const char* path = "/system/bin/init";
    const char* args[] = {path, "selinux_setup", nullptr};
    execv(path, const_cast<char**>(args));

    // execv() only returns if an error happened, in which case we
    // panic and never fall through this conditional.
    PLOG(FATAL) << "execv(\"" << path << "\") failed";

    return 1;
}
```

最后一部分就是通过重新执行init可执行程序，并传入启动参数selinux\_setup然后调用SetupSelinux开始SeLinux的初始化（这个调用流程在前面一章中已说明）。

关于SeLinux的初始化分析我将会在《[Android Framework—Init进程—5.SEAndroid机制](https://juejin.cn/spost/7419482499598598154 "https://juejin.cn/spost/7419482499598598154")》此文章中单独讲解。