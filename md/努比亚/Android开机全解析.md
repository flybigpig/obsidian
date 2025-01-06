对于任何操作系统来讲，开机时间的优化都是一个很关键的工作。如果用户每次启动设备都需要等待很长的时间，那么其用户体验是很差的。本文从Android12出发，分以下三部分阐述Android系统的开机优化：

- Android开机过程
- 分析开机时间
- 开机速度优化实践

_备注_  
_1. 文中所有的代码都省略了无关部分，并且省略了省略号；_  
_2. 由于作者能力有限，难免会有不正确或者不完善的地方。欢迎大家指正。_

# 1. Android开机过程

要想进行开机速度的优化，首先需要了解开机的详细流程。开机过程从CPU上电开始，到锁屏界面显示出来结束。下图为比较简洁的开机流程图。

  

![](https://upload-images.jianshu.io/upload_images/26874665-a5f7726808676a3d.png?imageMogr2/auto-orient/strip|imageView2/2/w/624/format/webp)

boot_simple.png

  

再来看一张比较详细的开机流程图

  

![](https://upload-images.jianshu.io/upload_images/26874665-6fd8e60959fa3128.png?imageMogr2/auto-orient/strip|imageView2/2/w/803/format/webp)

boot_flow_all_3.png

总的来说，开机过程分为以下六主要子过程

## 1.1 Boot ROM

Boot ROM是硬编码在CPU内部固定地址的一段ROM(在一些较老的系统上也可能使用外挂Boot ROM（相对CPU来说）)，这块代码是由CPU制造商提供。当用户按下电源键或者系统重启之后，触发CPU上电动作，此时其它硬件还未初始化，然而这块ROM就已经可读了。CPU首先执行PBL(Primary Boot Loader，主引导加载程序，固化在ROM上)代码。在必要的硬件初始化之后，Boot ROM开始加载Bootloader到RAM中，然后PC指针跳过去执行bootloader。在加载 Bootloader之前，PBL也可以进行验证。如果验证无法通过，则不会加载运行Bootloader，从而开机失败。

  

![](https://upload-images.jianshu.io/upload_images/26874665-c175ef0e9d9f974e.png?imageMogr2/auto-orient/strip|imageView2/2/w/637/format/webp)

Android_boot_1_boot_ROM.png

**A.** CPU刚上电时，CPU 处于未初始化状态，还没有设定内部时钟，此时只有内部 RAM 可用。当电源稳定后会开始执行 Boot ROM 代码。Boot ROM通过系统寄存器映射到 ASIC (Application Specific Integrated Circuit, 即专用集成电路，是指应特定用户要求和特定电子系统的需要而设计、制造的集成电路)中的物理区域来找到boot media，进而可以找到Bootloader  
**B.** boot media序列确定之后，Boot ROM 加载 Bootloader到内部 RAM 中，之后Boot ROM代码会跳到Bootloader

## 1.2Bootloader

Bootloader是一个特殊的独立于内核的程序，是CPU复位后进入操作系统之前执行的一段代码。Bootloader完成由硬件启动到操作系统启动的过渡，从而为操作系统提供基本的运行环境，如初始化CPU、时钟、堆栈、存储器系统等。Bootloader功能类似于PC机的BIOS程序,其代码与CPU芯片的内核结构、具体型号、应用系统的配置及使用的操作系统等因素有关，因此不可能有通用的bootloader,开发时需要用户根据具体情况进行移植。嵌入式Linux系统中常用的Bootloader有armboot、redboot、blob、U-Boot、Bios-lt、Bootldr等，其中U-Boot是当前比较流行，功能比较强大的Bootloader，可以支持多种体系结构，但相对也比较复杂。硬件初始化完成之后，Bootloader将boot.img(kernel + ramdisk(ramdisk.img中主要是存放android启动后第一个用户进程init可执行文件和init.*.rc等相关启动脚本以及sbin目录下的adbd工具))从flash上copy到RAM里面，然后CPU执行转向kernel。

  

![](https://upload-images.jianshu.io/upload_images/26874665-27efffe8c0a1c7c4.png?imageMogr2/auto-orient/strip|imageView2/2/w/637/format/webp)

Android_boot_2_bootloader.png

**A.** Bootloader第一阶段首先会检测和设置外部RAM  
**B.** 外部 RAM可用之后，将Bootloader代码加载到外部RAM中  
**C.** Bootloader第二阶段包含了设置文件系统，内存，网络等等。  
**D.** Bootloader查找Linux内核并将其从boot media (或者其他地方，这取决于系统配置) 加载到 RAM 中，并且会配置一些内核启动时需要的启动参数  
**E.** Bootloader执行完之后会跳转到 Linux 内核执行  
一般也可将Bootloader程序的执行分为两个阶段，如下图所示  

![](https://upload-images.jianshu.io/upload_images/26874665-8d51905b0b784990.png?imageMogr2/auto-orient/strip|imageView2/2/w/675/format/webp)

bootloader_1.png

  
执行Bootloader程序过程中，如果镜像验证失败、BootLinux (&Info) 函数启动失败或者接收到启动至 fastboot 的命令（比如使用 adb reboot bootloader进行重启、在启动时按下了电源键+下音量键组合）时，会进入到Fastboot模式(Fastboot 是一种电脑通过USB数据线对手机固件进行刷写、擦除/格式化、调试、传输各种指令的固件通信协议, 俗称线刷模式或快速引导模式)。

## 1.3Kernel

Android kernel基于上游 Linux LTS (Linux Long Term Supported，长期支持) 内核。在 Google，LTS 内核会与 Android 专用补丁结合，形成所谓的“Android 通用内核 (ACK，Android Common Kernel)”。较新的 ACK（版本 5.4 及更高版本）也称为 GKI (Generic Kernel Image，通用内核镜像 )内核。 GKI项目通过统一核心内核并将 SoC 和板级支持从核心内核移至可加载模块中，解决了内核碎片化问题。GKI 内核为内核模块提供了稳定的内核模块接口 (KMI)，因此模块和内核可以独立进行更新。GKI 具有以下特点：

- 基于 ACK 来源构建而成。
- 是每个架构和每个 LTS 版本的单内核二进制文件以及关联的可加载模块（目前只有适用于 android11-5.4 和 android12-5.4 的 arm64）。
- 已经过关联 ACK 支持的所有 Android 平台版本的测试。在 GKI 内核版本的生命周期内不会发生功能弃用。
- 为给定 LTS 中的驱动程序提供了稳定版 KMI。
- 不包含 SoC 专用代码或板卡专用代码。下图显示了 GKI 内核和供应商模块架构：  
    
    ![](https://upload-images.jianshu.io/upload_images/26874665-cc99bf8a2a73d55c.png?imageMogr2/auto-orient/strip|imageView2/2/w/838/format/webp)
    
    generic-kernel-image-architecture.png
    
      
    由于Android的kernel实际上就是Linux kernel，只是针对移动设备做了一些优化，所以与其它Linux kernel的启动方式大同小异，都是对start_kernel函数的调用和执行。Kernel主要工作内容为设置缓存、被保护存储器、计划列表，加载驱动，启动kernel守护，挂载根目录，初始化输入输出，开启中断，初始化进程表等。当内核完成这些系统设置后，接下来在系统文件中寻找”init”文件，然后启动root进程或者系统的第一个进程。  
    Kernel启动过程分为两个阶段：  
    1）内核引导阶段。通常使用汇编语言编写，主要检查内核与当前硬件是否匹配。这部分也与硬件体系结构相关。  
    2）内核启动阶段。引导阶段结束前，将调用start_kernel()进入内核启动阶段。内核启动阶段相关的代码主要位于kernel/init/main.c。  
    
    ![](https://upload-images.jianshu.io/upload_images/26874665-315b1a4c1cbd7028.png?imageMogr2/auto-orient/strip|imageView2/2/w/637/format/webp)
    
    Android_boot_3_kernel.png
    
      
    **A.** 内存管理单元和高速缓存初始化完成之后，系统便可以使用虚拟内存和启动用户空间进程  
    **B.** 内核在根目录寻找初始化程序（/system/core/init），执行该程序以启动init进程  
    Kernel启动的核心函数是start_kernel函数，它完成了内核的大部分初始化工作。这个函数在最后调用了reset_init函数进行后续的初始化。reset_init函数最主要的任务就是启动内核线程kernel_init。kernel_init函数将完成设备驱动程序的初始化，并调用init_post函数启动用户空间的init进程。到init_post函数为止，内核的初始化已经基本完成。  
    
    ![](https://upload-images.jianshu.io/upload_images/26874665-ab0384d626fb02ed.png?imageMogr2/auto-orient/strip|imageView2/2/w/389/format/webp)
    
    boot_kernel.png
    

## 1.4 init进程

用户空间的第一个进程便是init进程，进程号为1。

  

![](https://upload-images.jianshu.io/upload_images/26874665-0add1b3ed6d8929c.png?imageMogr2/auto-orient/strip|imageView2/2/w/768/format/webp)

init_process.png

  

当系统启动完成之后，init进程会作为守护进程监视其它进程。在Linux中所有的进程都是由init进程直接或间接fork出来的。在init进程启动的过程中，会相继启动servicemanager(binder服务管理者)、Zygote进程。而Zygote又会创建system_server进程以及app进程。

  

![](https://upload-images.jianshu.io/upload_images/26874665-22e7924ef564d3f1.png?imageMogr2/auto-orient/strip|imageView2/2/w/697/format/webp)

Android_boot_4_init.png

  
对于init进程的功能分为4部分：

- 解析并运行所有的init.rc相关文件
- 根据rc文件，生成相应的设备驱动节点
- 处理子进程的终止(signal方式)
- 提供属性服务的功能  
    init进程涉及的主要代码文件有

```kotlin
system/core/init/
  -main.cpp
  -init.cpp
  -parser.cpp
/system/core/rootdir/
  -init.rc
```

init进程的入口为main.cpp类的main方法。

```c
// system/core/init/main.cpp
int main(int argc, char** argv) {
#if __has_feature(address_sanitizer)
     __asan_set_error_report_callback(AsanReportCallback);
#endif
    // 创建设备节点、权限设定等
    if(!strcmp(basename(argv[0]), "ueventd")) {
        return ueventd_main(argc, argv);
    }
    if(argc > 1){
        // 初始化日志系统
        if(!strcmp(argv[1], "subcontext")){
            android::base::InitLogging(argv, &android::base::KernelLogger);
            const BuiltinFunctionMap function_map;
            return SubcontextMain(argc, argv, &function_map);
        }
        // 2.创建安全增强型Linux（SELinux）
        if(!strcmp(argv[1], "selinux_setup")){
            return SetupSelinux(argv);
        }
        // 3.解析init.rc文件、提供服务、创建epoll与处理子进程的终止等
        if (!strcmp(argv[1], "second_stage")){
            return SecondStageMain(argc, argv);
        }
    }
    // 1.挂载相关文件系统
    return FirstStageMain(argc, argv);
}
```

主要执行了三步

- FirstStageMain
- SetupSelinux
- SecondStageMain  
    **FirstStageMain**  
    init进程启动的第一步，主要是挂载相关的文件系统

```c
// system/core/init/first_stage_init.cpp
int FirstStageMain(int argc, char** argv){
    if(REBOOT_BOOTLOADER_ON_PANIC){
        InstallRebootSignalHandlers();
    }
    boot_clock::time_point start_time = boot_clock::now();
    std::vector<std::pair<std::string, int>> errors;
#define CHECKCALL(x) \
    if (x != 0) errors.emplace_back(#x " failed", errno);
    umask(0);
    // 创建于挂载相关文件系统
    CHECKCALL(clearenv());
    CHECKCALL(setenv("PATH", _PATH_DEFPATH, 1));
    // Get the basic filesystem setup we need put together in the initramdisk
    // on / and then we'll let the rc file figure out the rest.
    CHECKCALL(mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755"));
    CHECKCALL(mkdir("/dev/pts", 0755));
    CHECKCALL(mkdir("/dev/socket", 0755));
    CHECKCALL(mount("devpts", "/dev/pts", "devpts", 0, NULL));
#define MAKE_STR(x) __STRING(x)
    CHECKCALL(mount("proc", "/proc", "proc", 0, "hidepid=2,gid=" MAKE_STR(AID_READPROC)));
#undef MAKE_STR
    // 原始命令不可暴露给没有特权的进程
    CHECKCALL(chmod("/proc/cmdline", 0440));
    gid_t groups[] = {AID_READPROC};
    CHECKCALL(setgroups(arraysize(groups), groups));
    CHECKCALL(mount("sysfs", "/sys", "sysfs", 0, NULL));
    CHECKCALL(mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL));
    // tmpfs已经挂载在/dev下，并且已生成/dev/kmsg，故可以与外界通信
    // 初始化日志系统
    InitKernelLogging(argv);
    // 进入下一步
    const char* path = "/system/bin/init";
    const char* args[] = {path, "selinux_setup", nullptr};
    execv(path, const_cast<char**>(args));
 
    // 只有在错误发生的情况下execv()函数才会返回
    PLOG(FATAL) << "execv(\"" << path << "\") failed";
 
    return 1;
}
```

主要通过mount挂载对应的文件系统，mkdir创建对应的文件目录，并配置相应的访问权限。  
需要注意的是，这些文件只是在应用运行的时候存在，一旦应用运行结束就会随着应用一起消失。  
挂载的**文件系统**主要有四类：

- tmpfs: 一种虚拟内存文件系统，它会将所有的文件存储在虚拟内存中。由于tmpfs是驻留在RAM的，因此它的内容是不持久的。断电后，tmpfs 的内容就消失了，这也是被称作tmpfs的根本原因。
- devpts: 为伪终端提供了一个标准接口，它的标准挂接点是/dev/pts。只要pty(pseudo-tty, 虚拟终端)的主复合设备/dev/ptmx被打开，就会在/dev/pts下动态的创建一个新的pty设备文件。
- proc: 也是一个虚拟文件系统，它可以看作是内核内部数据结构的接口，通过它我们可以获得系统的信息，同时也能够在运行时修改特定的内核参数。
- sysfs: 与proc文件系统类似，也是一个不占有任何磁盘空间的虚拟文件系统。它通常被挂接在/sys目录下。

在FirstStageMain还会通过InitKernelLogging(argv)来初始化log日志系统。此时Android还没有自己的系统日志，采用kernel的log系统，打开的设备节点/dev/kmsg， 那么可通过cat /dev/kmsg来获取内核log。

最后会通过execv方法传递对应的path与下一阶段的参数selinux_setup。  
**SetupSelinux**

```c
// system/core/init/selinux.cpp
int SetupSelinux(char** argv) {
     //初始化本阶段内核日志
    InitKernelLogging(argv);

    if (REBOOT_BOOTLOADER_ON_PANIC) {
        InstallRebootSignalHandlers();
    }
  
     //  初始化 SELinux，加载 SELinux 策略
    SelinuxSetupKernelLogging();
    SelinuxInitialize();
    //  再次调用 main 函数，并传入 second_stage 进入第二阶段
    //  而且此次启动就已经在 SELinux 上下文中运行
    if (selinux_android_restorecon("/system/bin/init", 0) == -1) {
        PLOG(FATAL) << "restorecon failed of /system/bin/init failed";
    }

    // 进入下一步
    const char* path = "/system/bin/init";
    const char* args[] = {path, "second_stage", nullptr};
    execv(path, const_cast<char**>(args));
  
    // execv() only returns if an error happened, in which case we
    // panic and never return from this function.
    PLOG(FATAL) << "execv(\"" << path << "\") failed";

    return 1;
}
```

这阶段主要是初始化 SELinux。SELinux 是安全加强型 Linux，能够很好的对全部进程强制执行访问控制，从而让 Android 更好的保护和限制系统服务、控制对应用数据和系统日志的访问，提高系统安全性。  
接下来调用execv进入到最后阶段SecondStageMain。  
**SecondStageMain**

```c
//  system/core/init/init.cpp
int SecondStageMain(int argc, char** argv) {
    SetStdioToDevNull(argv);
    // 初始化本阶段内核日志
    InitKernelLogging(argv);
    //  系统属性初始化
    property_init();
    //  建立 Epoll
    Epoll epoll;
    //  注册信号处理
    InstallSignalFdHandler(&epoll);
    //  加载默认的系统属性
    property_load_boot_defaults(load_debug_prop);
    //  启动属性服务
    StartPropertyService(&epoll);
       subcontexts = InitializeSubcontexts();
    //加载系统启动脚本"/init.rc"
       ActionManager& am = ActionManager::GetInstance();
    ServiceList& sm = ServiceList::GetInstance();
    LoadBootScripts(am, sm);
    
       // 触发early-init，，init，late-init流程
    am.QueueEventTrigger("early-init");
    am.QueueEventTrigger("init");
    am.QueueBuiltinAction(InitBinder, "InitBinder");
    am.QueueEventTrigger("late-init");
    
    //解析启动脚本
    while (true) {
        //  执行 Action
        am.ExecuteOneCommand();
        //  还有就是重启死掉的子进程
        auto next_process_action_time = HandleProcessActions();
    }
}
```

SecondStageMain的主要工作总结

- 使用epoll对init子进程的信号进行监听
- 初始化系统属性，使用mmap共享内存
- 开启属性服务，并注册到epoll中
- 加载系统启动脚本"init.rc"
- 解析启动脚本，启动相关服务

重点介绍下init.rc文件的解析

```c
//system/core/init/init.cpp
static void LoadBootScripts(ActionManager& action_manager, ServiceList& service_list) {
    Parser parser = CreateParser(action_manager, service_list);
    std::string bootscript = GetProperty("ro.boot.init_rc", "");
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
```

通过ParseConfig来解析init.rc配置文件。.rc文件以行为单位，以空格为间隔，以#开始代表注释行。.rc文件主要包含Action、Service、Command、Options、Import，其中对于Action和Service的名称都是唯一的，对于重复的命名视为无效。init.rc中的Action、Service语句都有相应的类来解析，即ActionParser、ServiceParser。 以下为init.rc配置文件的部分内容。

```kotlin
// system/core/rootdir/init.rc
import /init.environ.rc
import /init.usb.rc
import /init.${ro.hardware}.rc
import /init.${ro.zygote}.rc
import /init.trace.rc

on early-init
    start ueventd
    mkdir /mnt 0775 root system
on init
    mount tmpfs none /sys/fs/cgroup mode=0750,uid=0,gid=1000
    mkdir /sys/fs/cgroup/memory 0750 root system
    mount cgroup none /sys/fs/cgroup/memory memory
on property:sys.boot_from_charger_mode=1
    class_stop charger
    trigger late-init

service ueventd /sbin/ueventd
    class core
    critical
    seclabel u:r:ueventd:s0
    
service logd /system/bin/logd
    class core
    socket logd stream 0666 logd logd
    socket logdr seqpacket 0666 logd logd
    socket logdw dgram 0222 logd logd
    seclabel u:r:logd:s0
    
service console /system/bin/sh
    class core
    console
    disabled
    user shell
    seclabel u:r:shell:s0

service adbd /sbin/adbd --root_seclabel=u:r:su:s0
    class core
    socket adbd stream 660 system system
    disabled
    seclabel u:r:adbd:s0
    
service servicemanager /system/bin/servicemanager
    class core
    user system
    group system
    critical
    onrestart restart healthd
    onrestart restart zygote
    onrestart restart media
    onrestart restart surfaceflinger
    onrestart restart drm
    
on late-init
     trigger early-fs
    trigger fs
    trigger post-fs
    trigger late-fs
    trigger post-fs-data
    trigger load_persist_props_action
    // 这里启动zygote-start
    trigger zygote-start
    trigger firmware_mounts_complete
    trigger early-boot
    trigger boot
```

可以看到，在解析init.rc的配置中，在late-init阶段启动了Zygote进程。

## 1.5 Zygote

![](https://upload-images.jianshu.io/upload_images/26874665-369c96357b108534.png?imageMogr2/auto-orient/strip|imageView2/2/w/697/format/webp)

Android_boot_5_zygote.png

  

Zygote进程是Android中所有Java进程的父进程。Zygote进程在Init进程启动过程中被以service服务的形式启动。Zygote进程相关的.rc配置文件为init.zygote64.rc或者init.zygote32.rc。以init.zygote64.rc为例，其内容如下

```tsx
// system/core/rootdir/init.zygote64.rc
service zygote /system/bin/app_process64 -Xzygote /system/bin --zygote --start-system-server
 class main
   priority -20
   user root
   group root readproc reserved_disk
   socket zygote stream 660 root system
   socket usap_pool_primary stream 660 root system
   onrestart exec_background - system system -- /ssystem/bin/vdc volume abort_fuse
   onrestart write /sys/power/state on
   onrestart restart audioserver
   onrestart restart cmeraserver
   onrestart restart media
   onrestart restart netd
   onrestart setprop sys.android.reboot 1
   writepid /dev/cpuset/foreground/tasks
   critical window=${zygote.critical_window.minute:-off} target=zygote-fatal
```

init进程解析init.zygote64.rc配置文件之后，会调用app_process

```c
// frameworks/base/cmds/app_process/app_main.cpp
    int main(int argc, char* const argv[])
    {
       if(zygote){
         runtime.start("com.android.internal.os.ZygoteInit", args, zygote);
         }
    }
```

执行到frameworks/base/core/jni/AndroidRuntime.cpp的start()方法

```c
// frameworks/base/core/jni/AndroidRuntime.cpp
void AndroidRuntime::start(const char* className, const Vector<String8>& options, bool zygote)
{
        ALOGD(">>>>>> START %s uid %d <<<<<<\n",class Name！= NULL？ class Name： "(unknown)", getuid() );
        // 打印LOG_BOOT_PROGRESS_START日志
        LOG_EVENT_LONG(LOG_BOOT_PROGRESS_START, ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));
        if(startVm(&mJavaVM, &env, zygote, primaryZygote) != 0){
            return;
        }
        onVmCreated(env);
        // 调用ZygoteInit类的main()方法
        env.CallStaticVoidMethod(startClass, startMeth, strArray)
    }
```

AndroidRuntime.cpp的start方法主要做了以下工作：

- 加载libart.so
- 启动虚拟机
- 加载注册JNI方法
- 启动Zygote

执行ZygoteInit.java的main()方法

```java
//frameworks/base/core/java/com/android/internal/os/ZygoteInit.java
@UnsupportedAppUsage
public static void main(String argv[]) {
    //zygote进程会fork出system_server进程
    if (startSystemServer) {
        Runnable r = forkSystemServer(abiList, socketName, zygoteServer);
        // zygote进程中，r == null；zygote子进程（如system_server进程）中， r != null
        if (r != null) {
          r.run();
          return;
        }
    }
    Log.i(TAG, "Accepting command socket connections");
    // zygote进程会在select loop死循环；而非zygote进程中之前已return。
    // loops forever in the zygote.
    caller = zygoteServer.runSelectLoop(abiList);
    if (caller != null) {
        caller.run();
    }
}
```

Zygote进程主要做了以下工作

- 加载虚拟机，并为JVM注册JNI方法
- 提前加载类PreloadClasses
- 提前加载资源PreLoadResouces
- fork system_server
- 提前加载类PreloadClasses, 调用runSelectLoop方法，等待进程孵化请求

zygote进程在fork子进程的时候可以共享虚拟机和资源，从而加快进程的启动速度，节省内存。

## 1.6 SystemServer

![](https://upload-images.jianshu.io/upload_images/26874665-6178fef048606fd6.png?imageMogr2/auto-orient/strip|imageView2/2/w/787/format/webp)

Android_boot_6_systemserver.png

  

SystemServer进程由Zygote进程fork而来，是Zygote孵化出的第一个进程。SystemServer和Zygote进程是Android Framework层的两大重要进程。SystemServer负责启动和管理整个Java frameWork。SystemServer进程在开启的时候，会初始化AMS、WMS、PMS等关键服务。同时会加载本地系统的服务库，调用createSystemContext()创建系统上下文，创建ActivityThread及开启各种服务等等。

SystemServer的启动相关代码如下

```java
// frameworks/base/services/java/com/android/server/SystemServer.java
public static void main(String[] args) {
    new SystemServer().run();
}

private void run() {
    try {
        Looper.prepareMainLooper();
        // 初始化本地服务
        System.loadLibrary("android_servers");
         // 初始化系统上下文
        createSystemContext();
        // 创建SystemServiceManager
        mSystemServiceManager = new SystemServiceManager(mSystemContext);
        mSystemServiceManager.setStartInfo(mRuntimeRestart,
                mRuntimeStartElapsedTime, mRuntimeStartUptime);
        LocalServices.addService(SystemServiceManager.class, mSystemServiceManager);
        // 构建线程池，以便并行执行一些初始化任务
        SystemServerInitThreadPool.get();
    } finally {
        traceEnd();  // InitBeforeStartServices
    }
 
    // 开启服务
    try {
        traceBeginAndSlog("StartServices");
        startBootstrapServices();// 启动引导服务
        startCoreServices();// 启动核心服务
        startOtherServices();// 启动其他服务
        SystemServerInitThreadPool.shutdown();
    } catch (Throwable ex) {
        Slog.e("System", "******************************************");
        Slog.e("System", "************ Failure starting system services", ex);
        throw ex;
    } finally {
        traceEnd();
    }
    Looper.loop();
    throw new RuntimeException("Main thread loop unexpectedly exited");
}
```

可以看到，SystemServer最重要的工作就是通过执行三个方法来启动所有服务

- startBootstrapServices();
- startCoreServices();
- startOtherServices();

分别对应引导服务、核心服务和其他服务：

- 引导服务(Bootstrap services)：这类服务包括 Installer，ActivityManagerService  
    PowerManagerService, DisplayManagerService, PackageManagerService, UserManagerService等
- 核心服务(Core services )： 这类服务包括 LightsService, BatteryService, UsageStatsServtce,  
    WebViewUpdateService等
- 其他服务：所有其它服务

在startOtherServices()方法中会启动SystemUI，之后SystemServer通知AMS系统已准备好，此时AMS启动桌面并且发送BOOT_COMPLETED广播。至此，系统层面启动流程结束。

通过下图再回顾下整个开机流程

  

![](https://upload-images.jianshu.io/upload_images/26874665-427df968133aebb9.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

Android_boot_all_1.png

# 2. 分析开机时间

要想进行开机速度的优化，我们需要分析开机时间的分布，从而找出异常耗时的地方，从而进行实际的优化工作。下面介绍如何分析开机时间。

## 2.1 分析开机日志

Android的log系统是独立于Linux内核log系统的. Android系统把Log分为了四类，不同的类别记录不同的Log信息，默认通过logcat抓取的是main信息：

- main - 主要的Log信息，大部分应用级别的Log信息都在这里
- events - 系统事件相关的Log信息
- radio - 无线/电话相关的Log信息
- system - 低级别的系统调试Log信息  
    通过查看events.txt中搜索"**boot_progress**"关键字或者通过以下命令过滤日志输出
    
    ```shell
    adb logcat -d -v time -b "events" | grep "boot_progress"
    ```
    

![](https://upload-images.jianshu.io/upload_images/26874665-d392a5b0b4942dbf.png?imageMogr2/auto-orient/strip|imageView2/2/w/702/format/webp)

init_process_log.png

  

行末数字即为此刻距开机时刻的时间间隔。每行代表开机的各个关键阶段。

|阶段|描述|
|---|---|
|boot_progress_start|系统进入用户空间，标志着kernel启动完成|
|boot_progress_preload_start|Zygote启动|
|boot_progress_preload_end|Zygote结束|
|boot_progress_system_run|SystemServer ready,开始启动Android系统服务|
|boot_progress_pms_start|PMS开始扫描安装的应用|
|boot_progress_pms_system_scan_start|PMS先行扫描/system目录下的安装包|
|boot_progress_pms_data_scan_start|PMS扫描/data目录下的安装包|
|boot_progress_pms_scan_end|PMS扫描结束|
|boot_progress_pms_ready|PMS就绪|
|boot_progress_ams_ready|AMS就绪|
|boot_progress_enable_screen|AMS启动完成后开始激活屏幕，从此以后屏幕才能响应用户的触摸，它在WindowManagerService发出退出开机动画的时间节点之前|
|sf_stop_bootanim|SF设置service.bootanim.exit属性值为1，标志系统要结束开机动画了|
|wm_boot_animation_done|开机动画结束，这一步用户能直观感受到开机结束|

各行log对应的打印代码为  
**boot_progress_start**

```c
// frameworks/base/core/jni/AndroidRuntime.cpp
void AndroidRuntime::start(const char* className, const Vector<String8>& options, bool zygote)
{
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == startSystemServer) {
            primary_zygote = true;
           /* track our progress through the boot sequence */
           const int LOG_BOOT_PROGRESS_START = 3000;
           LOG_EVENT_LONG(LOG_BOOT_PROGRESS_START,  ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));
        }
    }
}
```

**boot_progress_preload_start**

```java
// frameworks/base/core/java/com/android/internal/os/ZygoteInit.java
public static void main(String[] argv){
    if(!enableLazyPreload){
        bootTimingsTraceLog.traceBegin("ZygotePreload");
        EventLog.writeEvent(BOOT_PROGRESS_PRELOAD_START, SystemClock.uptimeMillis());
        preload(bootTimingsTraceLog);
        EventLog.writeEvent(BOOT_PROGRESS_PRELOAD_END, SystemClock.uptimeMillis());
        bootTimingsTraceLog.traceEnd();
    }
}
```

**boot_progress_system_run**

```java
// frameworks/base/services/java/com/android/server/SystemServer.java
private void run() {
   Slog.i(TAG, "Entered the Android system server!");
   final long uptimeMillis = SystemClock.elapsedRealtime();
   EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_SYSTEM_RUN, uptimeMillis);
   if (!mRuntimeRestart){
            FrameworkStatsLog.write(FrameworkStatsLog.BOOT_TIME_EVENT_ELAPSED_TIME_REPORTED,
FrameworkStatsLog.BOOT_TIME_EVENT_ELAPSED_TIME__EVENT__SYSTEM_SERVER_INIT_START,uptimeMillis);
      }
    }
```

**boot_progress_pms_start**

```java
// frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java
public PackageManagerService(Injector injector, boolean onlyCore, boolean factoryTest) {
    LockGuard.installLock(mLock, LockGuard.INDEX_PACKAGES);
    EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_START,SystemClock.uptimeMillis()
    );
}
```

**boot_progress_pms_system_scan_start**

```java
// frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java
public PackageManagerService(Injector injector, boolean onlyCore, boolean factoryTest) {
   long startTime = SystemClock.uptimeMillis();
   EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_SYSTEM_SCAN_START, startTime);
}
```

**boot_progress_pms_data_scan_start**

```java
// frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java
public PackageManagerService(Injector injector, boolean onlyCore, boolean factoryTest) {
        final long systemScanTime = SystemClock.uptimeMillis() - startTime;
        final int systemPackagesCount = mPackages.size();
        Slog.i(TAG, "Finished scanning system apps. Time: " + systemScanTime
                    + " ms, packageCount: " + systemPackagesCount
                    + " , timePerPackage: "
                    + (systemPackagesCount == 0 ? 0 : systemScanTime / systemPackagesCount)
                    + " , cached: " + cachedSystemApps);
        if (mIsUpgrade && systemPackagesCount > 0) {
            FrameworkStatsLog.write(FrameworkStatsLog.BOOT_TIME_EVENT_DURATION_REPORTED,                
BOOT_TIME_EVENT_DURATION__EVENT__OTA_PACKAGE_MANAGER_SYSTEM_APP_AVG_SCAN_TIME,
     systemScanTime / systemPackagesCount);
            }
            if (!mOnlyCore) {
            EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_DATA_SCAN_START,
            SystemClock.uptimeMillis());
            scanDirTracedLI(sAppInstallDir, 0, scanFlags | SCAN_REQUIRE_KNOWN, 0,
                        packageParser, executorService);
            }
}
```

**boot_progress_pms_scan_end**

```java
// frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java
 public PackageManagerService(Injector injector, boolean onlyCore, boolean factoryTest) {
        mPackageUsage.read(mSettings.mPackages);
        mCompilerStats.read();
        EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_SCAN_END,
                    SystemClock.uptimeMillis());
            Slog.i(TAG, "Time to scan packages: "
                    + ((SystemClock.uptimeMillis()-startTime)/1000f)
                    + " seconds");
}
```

**boot_progress_pms_ready**

```java
// frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java
 public PackageManagerService(Injector injector, boolean onlyCore, boolean factoryTest) {
        t.traceBegin("write settings");
        mSettings.writeLPr();
        t.traceEnd();
        EventLog.writeEvent(EventLogTags.BOOT_PROGRESS_PMS_READY, SystemClock.uptimeMillis());
    }
```

**boot_progress_ams_ready**

```java
// frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java
public void systemReady(final Runnable goingCallback, @NonNull TimingsTraceAndSlog t) {
        t.traceEnd();
        EventLog.writeBootProgressAmsReady(SystemClock.uptimeMillis());
    }
```

**boot_progress_enable_screen**

```java
// frameworks/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java
 @Override
public void enableScreenAfterBoot(boolean booted) {
        writeBootProgressEnableScreen(SystemClock.uptimeMillis());
        mWindowManager.enableScreenAfterBoot();
        synchronized (mGlobalLock) {
            updateEventDispatchingLocked(booted);
        }
 }
```

**sf_stop_bootanim**

```c
// frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp
void SurfaceFlinger::bootFinished()
{
         property_set("service.bootanim.exit", "1");
         LOG_EVENT_LONG(LOGTAG_SF_STOP_BOOTANIM, ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));
}
```

**wm_boot_animation_done**

```java
// frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java
private void performEnableScreen() {
        EventLogTags.writeWmBootAnimationDone(SystemClock.uptimeMillis());
        Trace.asyncTraceEnd(TRACE_TAG_WINDOW_MANAGER,   "Stop bootanim");
}
```

可以将测试机与对比机抓取的此log数据制作成表格，制作成折线图，可以更加直观的观察到耗时异常的流程。

  

![](https://upload-images.jianshu.io/upload_images/26874665-353c9b01a806e6de.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

boot_time_chart_1.png

  

通过"boot_progress_"关键字分析日志，粒度较大，只能定位出大概的耗时流程，之后还需分析流程内部具体的耗时情况。开机各流程内部也有相应的日志，可以进行更加细致的分析。例如在SystemServiceManager.java类中启动服务时，会打印启动某项服务的日志。通过查看某个服务A与下一个服务的日志时间，可以计算出启动服务A的耗时。

  

![](https://upload-images.jianshu.io/upload_images/26874665-01a7cb19e3b1b2aa.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

a0ca34f607cf9c51134adcfebc515333.png

```java
// frameworks/base/services/core/java/com/android/server/SystemServiceManager.java
    public <T extends SystemService> T startService(Class<T> serviceClass)
    {
        try {
            final String name = serviceClass.getName();
            Slog.i(TAG, "Starting " + name);
            Trace.traceBegin(Trace.TRACE_TAG_SYSTEM_SERVER, "StartService " + name);
            try {
                Constructor<T> constructor = serviceClass . getConstructor (Context.class);
                service = constructor.newInstance(mContext);
            } catch ...
                startService(service);
            return service;
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
        }
    }

```

使用bootchart工具可以进行更加直观的分析。

## 2.2 使用bootchart工具

bootchart是一个能对GNU/Linux boot过程进行性能分析并把结果直观化的开源工具，在系统启动过程中自动收集 CPU 占用率、磁盘吞吐率、进程等信息，并以图形方式显示分析结果，可用作指导优化系统启动过程。BootChart包含数据收集工具和图像产生工具，数据收集工具在原始的BootChart中是独立的shell程序，但在Android中，数据收集工具被集成到了init程序中。  
以下涉及到的命令，请自行应该参数的path。

- 抓取bootchart数据  
    bootchart开始生成数据的源码

```c
// system/core/init/bootchart.cpp
static int do_bootchart_start() {
 // 只要存在/data/bootchart/enabled文件，即抓取bootchart数据
   std::string start;
   if (!android::base::ReadFileToString("/data/bootchart/enabled", &start)) {
       LOG(VERBOSE) << "Not bootcharting";
       return 0;
 }
   g_bootcharting_thread = new std::thread(bootchart_thread_main);
   return 0;
}
```

所以只要生成/data/bootchart/enabled文件即可

```shell
    adb shell touch /data/bootchart/enabled
    adb reboot
```

在设备启动后，提取启动图表：

```shell
    adb  pull /data/bootchart
```

获取到bootchart数据之后进行打包

```shell
    tar -czf bootchart.tgz *
```

- 分析数据  
    下载[Boot Chart](https://links.jianshu.com/go?to=https%3A%2F%2Fdownload.cnet.com%2FBoot-Chart%2F3000-2094_4-75956709.html)包并解压，使用bootchart.jar解析生成的文件输出图片

```shell
    java -jar bootchart.jar bootchart.tgz
```

![](https://upload-images.jianshu.io/upload_images/26874665-b84047ed7bcbf036.png?imageMogr2/auto-orient/strip|imageView2/2/w/396/format/webp)

Bootchart.png

  

从生成的图片可以更加直观详细的看到开机耗时以及硬件使用情况。个人认为，bootchart的分析应该是以PIXEL或者开机速度正常机子的bootchart为参考来对照分析。使用完之后，记得删除enabled文件以防每次开机都收集启动数据。

## 2.3 抓取boottrace

抓取开机阶段的trace，也就是boottrace，是一种重要的分析开机的手段。抓取方式如下：

1. 将手机中的atrace.rc拉取下来，并备份；

```shell
  adb pull /system/etc/init/atrace.rc
```

2. 在文件atrace.rc末尾添加

```shell
    on property:persist.debug.atrace.boottrace=1
    start boottrace
    service boottrace /system/bin/atrace --async_start -b 30720 gfx input view webview wm am sm audio video binder_lock binder_driver camera hal res dalvik rs bionic power pm ss database network adb vibrator aidl sched  
    disabled
    oneshot
```

3. 将修改后的atrace.rc文件push到手机里面

```shell
  adb push atrace.rc /system/etc/init/
```

4. 打开抓取boottrace的属性开关

```shell
    adb  shell setprop persist.debug.atrace.boottrace 1
```

5. 重启手机
6. 手机启动完成之后等待几秒，关闭boottrace属性开关

```shell
    adb  shell setprop persist.debug.atrace.boottrace 0
```

8. 生成boottrace文件

```shell
     adb shell atrace --async_stop -z -c -o /data/local/tmp/boot_trace
```

10. 拉取boottrace日志文件

```shell
    adb  pull /data/local/tmp/boot_trace
```

之后就可以通过分析boot_trace文件来分析了。

# 3. 开机速度优化实践

在Android S升级Android T的过程中，遇到一个开机速度的问题。同一个项目的手机，在升级Android T之后，比之前的开机时间大概要多18秒左右。  
首先在手机开机之后，查看boot_progress相关日志如下

```java
07-15 04:13:35.244 I/boot_progress_start( 1059): 4040
07-15 04:13:35.934 I/boot_progress_preload_start( 1059): 4730
07-15 04:13:37.255 I/boot_progress_preload_end( 1059): 6051
07-15 04:13:37.636 I/boot_progress_system_run( 2221): 6432
07-15 04:13:38.260 I/boot_progress_pms_start( 2221): 7056
07-15 04:13:38.473 I/boot_progress_pms_system_scan_start( 2221): 7269
07-15 04:13:38.797 I/boot_progress_pms_data_scan_start( 2221): 7593
07-15 04:13:38.803 I/boot_progress_pms_scan_end( 2221): 7599
07-15 04:13:38.894 I/boot_progress_pms_ready( 2221): 7690
07-15 04:13:58.006 I/boot_progress_ams_ready( 2221): 26802
07-15 04:13:59.164 I/boot_progress_enable_screen( 2221): 27960
```

发现，在boot_progress_pms_ready到boot_progress_ams_ready之间，耗时近20秒，严重超时。  
确定了大体的耗时异常点之后，我们抓取boottrace再进行进一步的分析。  
由于PMS和AMS服务军运行再SystemServer进程当中，所以我们重点关注SystemServer进程的运行情况。

  

![](https://upload-images.jianshu.io/upload_images/26874665-d3a15bb23b0b0c47.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

9d055601ad4ebe5eddec1aff0249da2c.png

  

查看trace文件发现，主要是因为在启动AudioService的时候耗时较长。startService相关的日志也表明了这一点。

  

![](https://upload-images.jianshu.io/upload_images/26874665-4682feeeb54288bc.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

be264267394792edc34e531924092587.png

  
可见，启动AudioService耗时15.5秒左右。通过在AudioService相关的代码里面添加log，最终定位到为AudioService驱动在Android T上的问题。转交Audio模块处理之后，开机时间正常。

## 参考

[Boot ROM](https://links.jianshu.com/go?to=https%3A%2F%2Fen.wikipedia.org%2Fwiki%2FBoot_ROM)  
[Booting process of Android devices](https://links.jianshu.com/go?to=https%3A%2F%2Fen.wikipedia.org%2Fwiki%2FBooting_process_of_Android_devices)  
[Android Boot Process](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.geeksforgeeks.org%2Fandroid-boot-process%2F)  
[Android白话启动篇（Android booting process）](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.csdn.net%2Feliot_shao%2Farticle%2Fdetails%2F51800310)  
[Booting process of Android devices](https://links.jianshu.com/go?to=https%3A%2F%2Fen.wikipedia.org%2Fwiki%2FBooting_process_of_Android_devices)  
[Android Boot Sequence](https://links.jianshu.com/go?to=https%3A%2F%2Flearnlinuxconcepts.blogspot.com%2F2014%2F02%2Fandroid-boot-sequence.html)  
[Android init 启动](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.bilibili.com%2Fread%2Fcv9263357%2F)  
[深入研究源码：Android10.0系统启动流程（二）init进程](https://links.jianshu.com/go?to=https%3A%2F%2Fzhuanlan.zhihu.com%2Fp%2F350204603)  
[Android Zygote Startup](https://links.jianshu.com/go?to=https%3A%2F%2Felinux.org%2FAndroid_Zygote_Startup)  
[安卓10开机时间优化分析](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.csdn.net%2Fmafei852213034%2Farticle%2Fdetails%2F109265538)  
[Android开机各个阶段(Android R)](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.cnblogs.com%2Fforrest-lin%2Fp%2F14528341.html)  
[优化启动时间](https://links.jianshu.com/go?to=https%3A%2F%2Fsource.android.com%2Fdevices%2Ftech%2Fperf%2Fboot-times)

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/3f23e027b591  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。