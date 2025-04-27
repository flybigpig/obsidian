原文地址：[https://blog.csdn.net/qq\_30993595/article/details/82714409](https://blog.csdn.net/qq_30993595/article/details/82714409)

Android系统启动流程

Android系统启动过程往细了说可以分为5步：  
Loader --》Kernel --》Native --》Framework --》Application

![](https://i-blog.csdnimg.cn/blog_migrate/4a54d636ff02642973270910ce4e56f6.jpeg)

#### Loader

Boot ROM: 当手机处于关机状态时，长按Power键开机，引导芯片开始从固化在ROM里的预设出代码开始执行，然后加载引导程序到RAM  
Boot Loader：这是启动Android系统之前的引导程序，主要是检查RAM，初始化硬件参数等功能

#### Kernel

Kernel层是指Android内核层，到这里才刚刚开始进入Android系统

启动Kernel的swapper进程(pid=0)：该进程又称为idle进程, 系统初始化过程Kernel由无到有开创的第一个进程, 用于初始化进程管理、内存管理，加载Display,Camera Driver，Binder Driver等相关工作  
启动kthreadd进程（pid=2）：是Linux系统的内核进程，会创建内核工作线程kworkder，软中断线程ksoftirqd，thermal等内核守护进程。kthreadd进程是所有内核进程的鼻祖

#### Native

这里的Native层主要包括init孵化来的用户空间的守护进程、HAL层以及开机动画等。启动init进程(pid=1),是Linux系统的用户进程，init进程是所有用户进程的鼻祖

init进程会孵化出ueventd、logd、healthd、installd、adbd、lmkd等用户守护进程  
init进程还启动servicemanager(binder服务管家)、bootanim(开机动画)等重要服务  
init进程孵化出Zygote进程，Zygote进程是Android系统的第一个Java进程(即虚拟机进程)，Zygote是所有Java进程的父进程

#### Framework

Zygote进程启动后，加载ZygoteInit类，注册Zygote Socket服务端套接字；加载虚拟机；加载类，加载系统资源  
System Server进程，是由Zygote进程fork而来，System Server是Zygote孵化的第一个进程，System Server负责启动和管理整个Java framework，包含ActivityManager，PowerManager等服务  
Media Server进程，是由init进程fork而来，负责启动和管理整个C++ framework，包含AudioFlinger，Camera Service等服务  
Application

Zygote进程孵化出的第一个App进程是Launcher，即手机桌面APP，没错，手机桌面就是跟我们平时使用的APP一样，它也是一个应用  
所有APP进程都是由zygote进程fork出来的  
这些层之间，有的并不能直接交流，比如Native与Kernel之间要经过系统调用才能访问，Java层和Native层需要通过JNI进行调用

严格来说，Android系统实际上是运行于Linux内核上的一系列服务进程，这些进程是维持设备正常运行的关键，而这些进程的老祖宗就是init进程

#### init进程

上面也介绍到了，当内核启动完成后，就会创建用户空间的第一个进程，即init进程；后面所有的进程，比如Binder机制中的ServiceManager，Zygote都是由init进程孵化出来的

启动

当init进程启动后会调用/system/core/init/Init.cpp的main()方法  
 

```cpp
int main(int argc, char** argv) {
    ...
    
    klog_init();  //初始化kernel log，位于设备节点/dev/kmsg
    klog_set_level(KLOG_NOTICE_LEVEL); //设置输出的log级别
    // 输出init启动阶段的log
    NOTICE("init%s started!\n", is_first_stage ? "" : " second stage");
    
    property_init(); //创建一块共享的内存空间，用于属性服务
    signal_handler_init();  //初始化子进程退出的信号处理过程

    property_load_boot_defaults(); //加载default.prop文件
    start_property_service();   //启动属性服务器(通过socket通信)
    init_parse_config_file("/init.rc"); //解析init.rc文件

    //执行rc文件中触发器为 on early-init的语句
    action_for_each_trigger("early-init", action_add_queue_tail);
    //等冷插拔设备初始化完成
    queue_builtin_action(wait_for_coldboot_done_action, "wait_for_coldboot_done");
    queue_builtin_action(mix_hwrng_into_linux_rng_action, "mix_hwrng_into_linux_rng");
    //设备组合键的初始化操作
    queue_builtin_action(keychord_init_action, "keychord_init");
    // 屏幕上显示Android静态Logo 
    queue_builtin_action(console_init_action, "console_init");
    
    //执行rc文件中触发器为 on init的语句
    action_for_each_trigger("init", action_add_queue_tail);
    queue_builtin_action(mix_hwrng_into_linux_rng_action, "mix_hwrng_into_linux_rng");
    
    char bootmode[PROP_VALUE_MAX];
    //当处于充电模式，则charger加入执行队列；否则late-init加入队列。
    if (property_get("ro.bootmode", bootmode) > 0 && strcmp(bootmode, "charger") == 0) {
       action_for_each_trigger("charger", action_add_queue_tail);
    } else {
       action_for_each_trigger("late-init", action_add_queue_tail);
    }
    //触发器为属性是否设置
    queue_builtin_action(queue_property_triggers_action, "queue_property_triggers");
     
    while (true) {
        if (!waiting_for_exec) {
            execute_one_command();
            restart_processes(); 
        }
        int timeout = -1;
        if (process_needs_restart) {
            timeout = (process_needs_restart - gettime()) * 1000;
            if (timeout < 0)
                timeout = 0;
        }
        if (!action_queue_empty() || cur_action) {
            timeout = 0;
        }

        epoll_event ev;
        //循环 等待事件发生
        int nr = TEMP_FAILURE_RETRY(epoll_wait(epoll_fd, &ev, 1, timeout));
        if (nr == -1) {
            ERROR("epoll_wait failed: %s\n", strerror(errno));
        } else if (nr == 1) {
            ((void (*)()) ev.data.ptr)();
        }
    }
    return 0;
}
```

这里很重要的一句话就是init\_parse\_config\_file，然后去解析init.rc文件，init.rc位于/bootable/recovery/etc/init.rc；需要注意的是这就是一个脚本文件，就像Android打包用到的gradle脚本一样

rc文件规则  
这个文件解析具体实现在init\_parser.cpp文件中，一个完整的init.rc脚本由四种类型的声明组成

Action（动作）  
Commands（命令）  
Service（服务）  
Options（选项）  
rc文件有一些通用的语法规则

注释以 # 开头  
关键字和参数以空格分割，每个语句以行为单位  
C语言风格的反斜杠转义字符（""）可以用来为参数添加空格  
为了防止字符串中的空格把其切割成多个部分，我们需要对其使用双引号  
行尾的反斜杠用来表示下面一行是同一行  
Actions和Service暗示着下一个新语句的开始，这两个关键字后面跟着的commands或者options都属于这个新语句  
ActionsService有唯一的名字，如果出现和已有Actions和Service重名的会被当做错误而忽略  
Actions  
Actions代表一些Action，Action代表一组命令（Commands），每个Action都有一个trigger（触发器），这个触发器决定了在什么情况下执行该Action中定义的命令；当一些条件满足触发器的条件时，该Action中定义的命令会被添加到“命令执行队列”的尾部，如果命令已经存在了就不会再添加了

Action的格式如下

```bash
on <trgger> ## on后面接触发条件
   <command1> ## 命令1
   <command2> ## 命令2
   <command3> ## 命令3
   ...
```

不同的脚本用【on】来区分，on后面跟一个触发器，当被触发时，下面的命令就会以此执行

常用的有以下几种事件触发器

```bash
类型                      说明
-------------------------------------------------
boot                    init.rc被装载后触发
device-added-<path>     当设备节点添加时触发
device-removed-<path>   当设备节点移除时触发
service-exited-<name>   在指定的服务(service)退出时触发
early-init              init程序初始化之前触发
late-init               init程序初始化之后触发
init                    初始化时触发（在 /init.conf （启动配置文件）被装载之后）
```

#### Commands

  
exec< path>\[< argument>\]\* ：fork并执行一个程序，其路径为< path>，这条命令将阻塞直到该程序启动完成  
ifup< interface>：使网络接口< interface>成功连接  
chdir< directory>：更改工作目录< directory>  
chmod< octal-mode>< path>：更改文件访问权限  
chroot< directory>：更改根目录位置  
class\_start< serviceclass>：启动由< serviceclass>类名指定的所有相关服务，如果它们不在运行状态的话  
class\_stop< serviceclass>：停止所有由< serviceclass>类名指定的服务，如果它们还在运行的话  
domainname< name>：设置域名  
start< service>：启动一个服务，如果没有运行  
stop< service>：停止一个服务，如果还在运行  
sysclktz< mins\_west\_of\_gmt>：设置基准时间  
write< path>< String>\[< String>\]\*：打开一个文件，写入一个或者多个String  
Service  
Service其实是一个可执行程序，以service开头的脚本，在特定选项的约束下会被init程序启动或者重启（Service可以在配置中指定是否需要在退出时重启，这样当Service出现crash时就可以有机会复原）

脚本格式如下

```bash
service <name> <pathname> [ <argument> ]*
    <option>
    <option>
    ...
```

+   service开头表明这是一个可执行程序
+   < name> 表示服务名
+   < pathname> 此服务所在路径，因为是可执行文件，所以肯定有存储路径
+   < argument> 启动Service所带的参数
+   < option> 对service的设置的选项

#### Options

  
可用选项如下，也就是上面Service所用到的< option>

critical：表明这是对设备至关重要的一个服务，如果它在四分钟内退出超过四次，则设备进入恢复模式  
disabled：此服务不会自动启动，而是需要显示调用服务名来启动  
socket< name>< type>< perm>\[< user>\[< group>\] \]：创建一个名为/dev/socket/< name>的unix domain socket，然后将它的fd值传给启动它的进程；有效的< type>值包括dgram，stream，seqpacket，而user和group默认值是0  
user< username>：在启动服务前将用户切换至< username>，默认情况下用户都是root  
group< groupname>\[< groupname>\]\*：在启动服务前将用户组切换至< groupname>\]  
oneshot：当该服务退出时，不要主动去重启它  
class< name>：为该服务指定一个class名，同一个class的所有服务必须同时启动或者停止，默认情况下< name>值是default  
onrestart：当此服务重启时执行某些命令

init.rc文件  
 

```bash
# Copyright (C) 2012 The Android Open Source Project
#
# IMPORTANT: Do not create world writable files or directories.
# This is a common source of Android security bugs.
#

"【import <filename>一个init配置文件，扩展当前配置。】"
import /init.environ.rc
import /init.usb.rc
import /init.${ro.hardware}.rc
import /init.${ro.zygote}.rc
import /init.trace.rc

"【触发条件early-init，在early-init阶段调用以下行】"
on early-init
    # Set init and its forked children's oom_adj.
    write /proc/1/oom_score_adj -1000
    "【打开路径为<path>的一个文件，并写入一个或多个字符串】"
    # Apply strict SELinux checking of PROT_EXEC on mmap/mprotect calls.
    write /sys/fs/selinux/checkreqprot 0

    # Set the security context for the init process.
    # This should occur before anything else (e.g. ueventd) is started.
    "【这段脚本的意思是init进程启动之后就马上调用函数setcon将自己的安全上下文设置为“u:r:init:s0”，即将init进程的domain指定为init。】"
    setcon u:r:init:s0

    # Set the security context of /adb_keys if present.
    "【恢复指定文件到file_contexts配置中指定的安全上线文环境】"
    restorecon /adb_keys

    "【执行start ueventd的命令。ueventd是一个service后面有定义】 "
    start ueventd

    "【mkdir <path> [mode] [owner] [group]   //创建一个目录<path>，可以选择性地指定mode、owner以及group。如果没有指定，默认的权限为755，并属于root用户和root组。】"
    # create mountpoints
    mkdir /mnt 0775 root system

on init
    "【设置系统时钟的基准,比如0代表GMT,即以格林尼治时间为准】"
    sysclktz 0

"【设置kernel日志等级】"
loglevel 6 ####
    write /proc/bootprof "INIT: on init start" ####

    "【symlink <target> <path>    //创建一个指向<path>的软连接<target>。】"
    # Backward compatibility
    symlink /system/etc /etc
    symlink /sys/kernel/debug /d

    # Right now vendor lives on the same filesystem as system,
    # but someday that may change.
    symlink /system/vendor /vendor

    "【创建一个目录<path>，可以选择性地指定mode、owner以及group。】"
    # Create cgroup mount point for cpu accounting
    mkdir /acct
    mount cgroup none /acct cpuacct
    mkdir /acct/uid

    "【mount <type> <device> <dir> [ <mountoption> ]   //在目录<dir>挂载指定的设备。<device> 可以是以 mtd@name 的形式指定一个mtd块设备。<mountoption>包括 ro、rw、remount、noatime、 ...】"
    # Create cgroup mount point for memory
    mount tmpfs none /sys/fs/cgroup mode=0750,uid=0,gid=1000
    mkdir /sys/fs/cgroup/memory 0750 root system
    mount cgroup none /sys/fs/cgroup/memory memory
    write /sys/fs/cgroup/memory/memory.move_charge_at_immigrate 1
    "【chown <owner> <group> <path>   //改变文件的所有者和组。】"

    "【后面的一些行因为类似，就省略了】"
    .....

# Healthd can trigger a full boot from charger mode by signaling this
# property when the power button is held.
on property:sys.boot_from_charger_mode=1
    "【停止指定类别服务类下的所有已运行的服务】"
    class_stop charger
    "【触发一个事件,将该action排在某个action之后(用于Action排队)】"
    trigger late-init

# Load properties from /system/ + /factory after fs mount.
on load_all_props_action
    "【从/system，/vendor加载属性。默认包含在init.rc】"
    load_all_props

# Indicate to fw loaders that the relevant mounts are up.
on firmware_mounts_complete
    "【删除指定路径下的文件】"
    rm /dev/.booting

# Mount filesystems and start core system services.
on late-init
    "【触发一个事件。用于将一个action与另一个 action排列。】"
    trigger early-fs
    trigger fs
    trigger post-fs
    trigger post-fs-data

    # Load properties from /system/ + /factory after fs mount. Place
    # this in another action so that the load will be scheduled after the prior
    # issued fs triggers have completed.
    trigger load_all_props_action

    # Remove a file to wake up anything waiting for firmware.
    trigger firmware_mounts_complete

    trigger early-boot
    trigger boot


on post-fs
    ...
    "【一些创造目录，建立链接，更改权限的操作，这里省略】"

on post-fs-data
    ...
    "【一些创造目录，建立链接，更改权限的操作，这里省略】"

    "【恢复指定文件到file_contexts配置中指定的安全上线文环境】"
    restorecon /data/mediaserver

    "【将系统属性<name>的值设置为<value>,即以键值对的方式设置系统属性】"
    # Reload policy from /data/security if present.
    setprop selinux.reload_policy 1

    "【以递归的方式恢复指定目录到file_contexts配置中指定的安全上下文中】"
    # Set SELinux security contexts on upgrade or policy update.
    restorecon_recursive /data

    # If there is no fs-post-data action in the init.<device>.rc file, you
    # must uncomment this line, otherwise encrypted filesystems
    # won't work.
    # Set indication (checked by vold) that we have finished this action
    #setprop vold.post_fs_data_done 1

on boot
    "【初始化网络】"
    # basic network init
    ifup lo
    "【设置主机名为localhost】"
    hostname localhost
    "【设置域名localdomain】"
    domainname localdomain

    "【设置资源限制】"
    # set RLIMIT_NICE to allow priorities from 19 to -20
    setrlimit 13 40 40

    "【这里省略了一些chmod,chown,等操作，不多解释】"
   ...


    # Define default initial receive window size in segments.
    setprop net.tcp.default_init_rwnd 60

    "【重启core服务】"
    class_start core

on nonencrypted
    class_start main
    class_start late_start

on property:vold.decrypt=trigger_default_encryption
    start defaultcrypto

on property:vold.decrypt=trigger_encryption
    start surfaceflinger
    start encrypt

on property:sys.init_log_level=*
    loglevel ${sys.init_log_level}

on charger
    class_start charger

on property:vold.decrypt=trigger_reset_main
    class_reset main

on property:vold.decrypt=trigger_load_persist_props
    load_persist_props

on property:vold.decrypt=trigger_post_fs_data
    trigger post-fs-data

on property:vold.decrypt=trigger_restart_min_framework
    class_start main

on property:vold.decrypt=trigger_restart_framework
    class_start main
    class_start late_start

on property:vold.decrypt=trigger_shutdown_framework
    class_reset late_start
    class_reset main

on property:sys.powerctl=*
    powerctl ${sys.powerctl}

# system server cannot write to /proc/sys files,
# and chown/chmod does not work for /proc/sys/ entries.
# So proxy writes through init.
on property:sys.sysctl.extra_free_kbytes=*
    write /proc/sys/vm/extra_free_kbytes ${sys.sysctl.extra_free_kbytes}

# "tcp_default_init_rwnd" Is too long!
on property:sys.sysctl.tcp_def_init_rwnd=*
    write /proc/sys/net/ipv4/tcp_default_init_rwnd ${sys.sysctl.tcp_def_init_rwnd}

"【守护进程】"
## Daemon processes to be run by init.
##
service ueventd /sbin/ueventd
    class core
    critical
    seclabel u:r:ueventd:s0

"【日志服务进程】"
service logd /system/bin/logd
    class core
    socket logd stream 0666 logd logd
    socket logdr seqpacket 0666 logd logd
    socket logdw dgram 0222 logd logd
    seclabel u:r:logd:s0

"【Healthd是android4.4之后提出来的一种中介模型，该模型向下监听来自底层的电池事件，向上传递电池数据信息给Framework层的BatteryService用以计算电池电量相关状态信息】"
service healthd /sbin/healthd
    class core
    critical
    seclabel u:r:healthd:s0

"【控制台进程】"
service console /system/bin/sh
    "【为当前service设定一个类别.相同类别的服务将会同时启动或者停止,默认类名是default】"
    class core
    "【服务需要一个控制台】"
    console
    "【服务不会自动启动,必须通过服务名显式启动】"
    disabled
    "【在执行此服务之前切换用户名,当前默认的是root.自Android M开始,即使它要求linux capabilities,也应该使用该选项.很明显,为了获得该功能,进程需要以root用户运行】"
    user shell
    seclabel u:r:shell:s0

on property:ro.debuggable=1
    start console

# 启动adbd服务进程
service adbd /sbin/adbd --root_seclabel=u:r:su:s0
    class core
    "【创建一个unix域下的socket,其被命名/dev/socket/<name>. 并将其文件描述符fd返回给服务进程.其中,type必须为dgram,stream或者seqpacke,user和group默认是0.seclabel是该socket的SELLinux的安全上下文环境,默认是当前service的上下文环境,通过seclabel指定】"
    socket adbd stream 660 system system
    disabled
    seclabel u:r:adbd:s0

# adbd on at boot in emulator
on property:ro.kernel.qemu=1
    start adbd

"【内存管理服务，内存不够释放内存】"
service lmkd /system/bin/lmkd
    class core
    critical
    socket lmkd seqpacket 0660 system system

"【ServiceManager是一个守护进程，它维护着系统服务和客户端的binder通信。
在Android系统中用到最多的通信机制就是Binder，Binder主要由Client、Server、ServiceManager和Binder驱动程序组成。其中Client、Service和ServiceManager运行在用户空间，而Binder驱动程序运行在内核空间。核心组件就是Binder驱动程序了，而ServiceManager提供辅助管理的功能，无论是Client还是Service进行通信前首先要和ServiceManager取得联系。而ServiceManager是一个守护进程，负责管理Server并向Client提供查询Server的功能。】"
service servicemanager /system/bin/servicemanager
    class core
    user system
    group system
    critical
    onrestart restart healthd
    "【servicemanager 服务启动时会重启zygote服务】"
    onrestart restart zygote
    onrestart restart media
    onrestart restart surfaceflinger
    onrestart restart drm

"【Vold是Volume Daemon的缩写,它是Android平台中外部存储系统的管控中心,是管理和控制Android平台外部存储设备的后台进程】"
service vold /system/bin/vold
    class core
    socket vold stream 0660 root mount
    ioprio be 2

"【Netd是Android系统中专门负责网络管理和控制的后台daemon程序】"
service netd /system/bin/netd
    class main
    socket netd stream 0660 root system
    socket dnsproxyd stream 0660 root inet
    socket mdns stream 0660 root system
    socket fwmarkd stream 0660 root inet

"【debuggerd是一个daemon进程，在系统启动时随着init进程启动。主要负责将进程运行时的信息dump到文件或者控制台中】"
service debuggerd /system/bin/debuggerd
    class main

service debuggerd64 /system/bin/debuggerd64
    class main

"【Android RIL (Radio Interface Layer)提供了Telephony服务和Radio硬件之间的抽象层】"
# for using TK init.modem.rc rild-daemon setting
#service ril-daemon /system/bin/rild
#    class main
#    socket rild stream 660 root radio
#    socket rild-debug stream 660 radio system
#    user root
#    group radio cache inet misc audio log

"【提供系统 范围内的surface composer功能，它能够将各种应用 程序的2D、3D surface进行组合。】"
service surfaceflinger /system/bin/surfaceflinger
    class core
    user system
    group graphics drmrpc
    onrestart restart zygote

"【DRM可以直接访问DRM clients的硬件。DRM驱动用来处理DMA，内存管理，资源锁以及安全硬件访问。为了同时支持多个3D应用，3D图形卡硬件必须作为一个共享资源，因此需要锁来提供互斥访问。DMA传输和AGP接口用来发送图形操作的buffers到显卡硬件，因此要防止客户端越权访问显卡硬件。】"
#make sure drm server has rights to read and write sdcard ####
service drm /system/bin/drmserver
    class main
    user drm
    # group drm system inet drmrpc ####
    group drm system inet drmrpc sdcard_r ####

"【媒体服务，无需多说】"
service media /system/bin/mediaserver
    class main
    user root ####
#   google default ####
#   user media    ####
    group audio camera inet net_bt net_bt_admin net_bw_acct drmrpc mediadrm media sdcard_r system net_bt_stack ####
#   google default ####
#   group audio camera inet net_bt net_bt_admin net_bw_acct drmrpc mediadrm ####

    ioprio rt 4

"【设备加密相关服务】"
# One shot invocation to deal with encrypted volume.
service defaultcrypto /system/bin/vdc --wait cryptfs mountdefaultencrypted
    disabled
    "【当服务退出时,不重启该服务】"
    oneshot
    # vold will set vold.decrypt to trigger_restart_framework (default
    # encryption) or trigger_restart_min_framework (other encryption)

# One shot invocation to encrypt unencrypted volumes
service encrypt /system/bin/vdc --wait cryptfs enablecrypto inplace default
    disabled
    oneshot
    # vold will set vold.decrypt to trigger_restart_framework (default
    # encryption)

"【开机动画服务】"
service bootanim /system/bin/bootanimation
    class core
    user graphics
#    group graphics audio ####
    group graphics media audio ####
    disabled
    oneshot

"【在Android系统中，PackageManagerService用于管理系统中的所有安装包信息及应用程序的安装卸载，但是应用程序的安装与卸载并非PackageManagerService来完成，而是通过PackageManagerService来访问installd服务来执行程序包的安装与卸载的。】"
service installd /system/bin/installd
    class main
    socket installd stream 600 system system

service flash_recovery /system/bin/install-recovery.sh
    class main
    seclabel u:r:install_recovery:s0
    oneshot

"【vpn相关的服务】"
service racoon /system/bin/racoon
    class main
    socket racoon stream 600 system system
    # IKE uses UDP port 500. Racoon will setuid to vpn after binding the port.
    group vpn net_admin inet
    disabled
    oneshot

"【android中有mtpd命令可以连接vpn】"
service mtpd /system/bin/mtpd
    class main
    socket mtpd stream 600 system system
    user vpn
    group vpn net_admin inet net_raw
    disabled
    oneshot

service keystore /system/bin/keystore /data/misc/keystore
    class main
    user keystore
    group keystore drmrpc

"【可以用dumpstate 获取设备的各种信息】"
service dumpstate /system/bin/dumpstate -s
    class main
    socket dumpstate stream 0660 shell log
    disabled
    oneshot

"【mdnsd 是多播 DNS 和 DNS 服务发现的守护程序。】"
service mdnsd /system/bin/mdnsd
    class main
    user mdnsr
    group inet net_raw
    socket mdnsd stream 0660 mdnsr inet
    disabled
    oneshot

"【触发关机流程继续往下走】"
service pre-recovery /system/bin/uncrypt
    class main
    disabled
    "【当服务退出时,不重启该服务】"
    oneshot
```

启动顺序是on early-init -> init -> late-init -> boot，接下来就是各种服务的启动

init进程的功能

+   分析和运行所有的init.rc文件
+   生成设备驱动节点（通过rc文件创建）
+   处理子进程的终止(signal方式)
+   提供属性服务

####   
Zygote进程

  
在Android中，zygote是整个系统创建新进程的核心进程。在init进程启动后就会创建zygote进程；zygote进程在内部会先启动Dalvik虚拟机，继而加载一些必要的系统资源和系统类，最后进入一种监听状态。在之后的运作中，当其他系统模块（比如AMS）希望创建新进程时，只需向zygote进程发出请求，zygote进程监听到该请求后，会相应地fork出新的进程，于是这个新进程在初生之时，就先天具有了自己的Dalvik虚拟机以及系统资源

其实在早期的Android版本中，Zygote的启动命令直接是写在init.rc中的，但是随着硬件的不断升级换代，Android系统也要面对32位和64位机器同时存在的情况，所以对Zygote启动也需要根据不同情况对待

在init.rc顶部可以看到有这么一句话

> import /init.${ro.zygote}.rc

这里会根据系统属性ro.zygote的值去加载不同的描述Zygote的rc脚本，比如

+   init.zygote32.rc
+   init.zygote64.rc
+   init.zygote32\_64.rc
+   init.zygote64\_32.rc

以init.zygote64\_32.rc为例

```bash
service zygote /system/bin/app_process64 -Xzygote /system/bin --zygote --start-system-server --socket-name=zygote
    class main
    socket zygote stream 660 root system
    onrestart write /sys/android_power/request_state wake
    onrestart write /sys/power/state on
    onrestart restart audioserver
    onrestart restart cameraserver
    onrestart restart media
    onrestart restart netd
    writepid /dev/cpuset/foreground/tasks
 
service zygote_secondary /system/bin/app_process32 -Xzygote /system/bin --zygote --socket-name=zygote_secondary
    class main
    socket zygote_secondary stream 660 root system
    onrestart restart zygote
    writepid /dev/cpuset/foreground/tasks
```

如上，可以看到服务名（进程名）是zygote，对应的可执行程序是app\_processXX，而且还创建了一个名为zygote的unix domain socket，类型是stream，这个socket是为了后面IPC所用；上面可以看到还有一个zygote\_secondary的进程，其实这是为了适配不同的abi型号

其中Zygote进程能够重启的地方有

+   servicemanager进程被杀
+   surfaceflinger进程被杀
+   Zygote进程自己被杀
+   system\_server进程被杀

  
接下来看看zygote启动过程，zygote对应的可执行文件就是/system/bin/app\_processXX，也就是说系统启动时会执行到这个可执行文件的main()函数里

#### main  
 

```cpp
int main(int argc, char* const argv[])
{
    //Android运行时环境，传到的参数argv为“-Xzygote /system/bin --zygote --start-system-server”
    AppRuntime runtime(argv[0], computeArgBlockSize(argc, argv));
    argc--; argv++; //忽略第一个参数

    int i;
    for (i = 0; i < argc; i++) {
        if (argv[i][0] != '-') {
            break;
        }
        if (argv[i][1] == '-' && argv[i][2] == 0) {
            ++i;
            break;
        }
        runtime.addOption(strdup(argv[i]));
    }
    //参数解析
    bool zygote = false;
    bool startSystemServer = false;
    bool application = false;
    String8 niceName;
    String8 className;
    ++i;
    while (i < argc) {
        const char* arg = argv[i++];
        if (strcmp(arg, "--zygote") == 0) {
            zygote = true;
            //--zygote表示当前进程用于承载zygote
            //对于64位系统nice_name为zygote64; 32位系统为zygote
            niceName = ZYGOTE_NICE_NAME;
        } else if (strcmp(arg, "--start-system-server") == 0) {
	        //是否需要启动system server
            startSystemServer = true;
        } else if (strcmp(arg, "--application") == 0) {
	        //启动进入独立的程序模式
            application = true;
        } else if (strncmp(arg, "--nice-name=", 12) == 0) {
	        //niceName 为当前进程别名，区别abi型号
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
        // 运行application或tool程序
        args.add(application ? String8("application") : String8("tool"));
        runtime.setClassNameAndArgs(className, argc - i, argv + i);
    } else {
        //进入zygote模式，创建 /data/dalvik-cache路径
        maybeCreateDalvikCache();
        if (startSystemServer) {
            args.add(String8("start-system-server"));
        }
        char prop[PROP_VALUE_MAX];
        if (property_get(ABI_LIST_PROPERTY, prop, NULL) == 0) {
            return 11;
        }
        String8 abiFlag("--abi-list=");
        abiFlag.append(prop);
        args.add(abiFlag);

        for (; i < argc; ++i) {
            args.add(String8(argv[i]));
        }
    }

    //设置进程名
    if (!niceName.isEmpty()) {
        runtime.setArgv0(niceName.string());
        set_process_name(niceName.string());
    }
    if (zygote) {
        runtime.start("com.android.internal.os.ZygoteInit", args, zygote);
    } else if (className) {
        runtime.start("com.android.internal.os.RuntimeInit", args, zygote);
    } else {
        //没有指定类名或zygote，参数错误
        return 10;
    }
}

```

根据传入参数的不同可以有两种启动方式，一个是 “com.android.internal.os.RuntimeInit”, 另一个是 ”com.android.internal.os.ZygoteInit", 对应RuntimeInit 和 ZygoteInit 两个类， 这两个类的主要区别在于Java端，可以明显看出，ZygoteInit 相比 RuntimeInit 多做了很多事情，比如说 “preload", “gc” 等等。但是在Native端，他们都做了相同的事， startVM() 和 startReg()

在当前场景中，init.rc指定了–zygote选项，并且args有添加start-system-server值，所以接下来执行

#### AndroidRuntime::start

```cpp
void AndroidRuntime::start(const char* className, const Vector<String8>& options, bool zygote)
{
    static const String8 startSystemServer("start-system-server");

    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == startSystemServer) {
           const int LOG_BOOT_PROGRESS_START = 3000;
        }
    }
    const char* rootDir = getenv("ANDROID_ROOT");
    if (rootDir == NULL) {
        rootDir = "/system";
        if (!hasDir("/system")) {
            return;
        }
        setenv("ANDROID_ROOT", rootDir, 1);
    }
    JniInvocation jni_invocation;
    jni_invocation.Init(NULL);
    JNIEnv* env;
    // 虚拟机创建，主要篇幅是关于虚拟机参数的设置
    if (startVm(&mJavaVM, &env, zygote) != 0) {
        return;
    }
    onVmCreated(env);
    // JNI方法注册
    if (startReg(env) < 0) {
        return;
    }

    jclass stringClass;
    jobjectArray strArray;
    jstring classNameStr;

    //等价 strArray= new String[options.size() + 1];
    stringClass = env->FindClass("java/lang/String");
    strArray = env->NewObjectArray(options.size() + 1, stringClass, NULL);

    //等价 strArray[0] = "com.android.internal.os.ZygoteInit"
    classNameStr = env->NewStringUTF(className);
    env->SetObjectArrayElement(strArray, 0, classNameStr);

    //等价 strArray[1] = "start-system-server"；
    //    strArray[2] = "--abi-list=xxx"；
    //其中xxx为系统响应的cpu架构类型，比如arm64-v8a.
    for (size_t i = 0; i < options.size(); ++i) {
        jstring optionsStr = env->NewStringUTF(options.itemAt(i).string());
        env->SetObjectArrayElement(strArray, i + 1, optionsStr);
    }

    //将"com.android.internal.os.ZygoteInit"转换为"com/android/internal/os/ZygoteInit"
    char* slashClassName = toSlashClassName(className);
    //找到Zygoteinit类
    jclass startClass = env->FindClass(slashClassName);
    if (startClass == NULL) {
        ...
    } else {
    //找到这个类后就继续找成员函数main方法的Mehtod ID
        jmethodID startMeth = env->GetStaticMethodID(startClass, "main",
            "([Ljava/lang/String;)V");
        // 通过反射调用ZygoteInit.main()方法
        env->CallStaticVoidMethod(startClass, startMeth, strArray);
    }
    //释放相应对象的内存空间
    free(slashClassName);
    mJavaVM->DetachCurrentThread();
    mJavaVM->DestroyJavaVM();
}
```

#### AndroidRuntime::startVm

```cpp
int AndroidRuntime::startVm(JavaVM** pJavaVM, JNIEnv** pEnv, bool zygote)
{
    // JNI检测功能，用于native层调用jni函数时进行常规检测，比较弱字符串格式是否符合要求，资源是否正确释放。该功能一般用于早期系统调试或手机Eng版，对于User版往往不会开启，引用该功能比较消耗系统CPU资源，降低系统性能。
    bool checkJni = false;
    property_get("dalvik.vm.checkjni", propBuf, "");
    if (strcmp(propBuf, "true") == 0) {
        checkJni = true;
    } else if (strcmp(propBuf, "false") != 0) {
        property_get("ro.kernel.android.checkjni", propBuf, "");
        if (propBuf[0] == '1') {
            checkJni = true;
        }
    }
    if (checkJni) {
        addOption("-Xcheck:jni");
    }

    //虚拟机产生的trace文件，主要用于分析系统问题，路径默认为/data/anr/traces.txt
    parseRuntimeOption("dalvik.vm.stack-trace-file", stackTraceFileBuf, "-Xstacktracefile:");

    //对于不同的软硬件环境，这些参数往往需要调整、优化，从而使系统达到最佳性能
    parseRuntimeOption("dalvik.vm.heapstartsize", heapstartsizeOptsBuf, "-Xms", "4m");
    parseRuntimeOption("dalvik.vm.heapsize", heapsizeOptsBuf, "-Xmx", "16m");
    parseRuntimeOption("dalvik.vm.heapgrowthlimit", heapgrowthlimitOptsBuf, "-XX:HeapGrowthLimit=");
    parseRuntimeOption("dalvik.vm.heapminfree", heapminfreeOptsBuf, "-XX:HeapMinFree=");
    parseRuntimeOption("dalvik.vm.heapmaxfree", heapmaxfreeOptsBuf, "-XX:HeapMaxFree=");
    parseRuntimeOption("dalvik.vm.heaptargetutilization",
                       heaptargetutilizationOptsBuf, "-XX:HeapTargetUtilization=");
    ...

    //preloaded-classes文件内容是由WritePreloadedClassFile.java生成的，
    //在ZygoteInit类中会预加载工作将其中的classes提前加载到内存，以提高系统性能
    if (!hasFile("/system/etc/preloaded-classes")) {
        return -1;
    }

    //初始化虚拟机
    if (JNI_CreateJavaVM(pJavaVM, pEnv, &initArgs) < 0) {
        ALOGE("JNI_CreateJavaVM failed\n");
        return -1;
    }
}
```

#### AndroidRuntime::startReg

```cpp
int AndroidRuntime::startReg(JNIEnv* env)
{
    //设置线程创建方法为javaCreateThreadEtc 
    androidSetCreateThreadFunc((android_create_thread_fn) javaCreateThreadEtc);

    env->PushLocalFrame(200);
    //进程JNI方法的注册
    if (register_jni_procs(gRegJNI, NELEM(gRegJNI), env) < 0) {
        env->PopLocalFrame(NULL);
        return -1;
    }
    env->PopLocalFrame(NULL);
    return 0;
}
```

总结一下Zygote native 进程做了哪些主要工作：

+   创建虚拟机–startVM
    
+   注册JNI函数–startReg
    
+   通过JNI知道Java层的com.android.internal.os.ZygoteInit 类，调用main 函数，进入java 世界
    

这就开始进入java层了

/frameworks/base/core/java/com/android/internal/os/ZygoteInit.java

####   
ZygoteInit.main

```java
public static void main(String argv[]) {
    try {
        RuntimeInit.enableDdms(); //开启DDMS功能
        SamplingProfilerIntegration.start();
        boolean startSystemServer = false;
        String socketName = "zygote";
        String abiList = null;
        for (int i = 1; i < argv.length; i++) {
            if ("start-system-server".equals(argv[i])) {
                startSystemServer = true;
            } else if (argv[i].startsWith(ABI_LIST_ARG)) {
                abiList = argv[i].substring(ABI_LIST_ARG.length());
            } else if (argv[i].startsWith(SOCKET_NAME_ARG)) {
                socketName = argv[i].substring(SOCKET_NAME_ARG.length());
            } else {
                throw new RuntimeException("Unknown command line argument: " + argv[i]);
            }
        }
        ...

        registerZygoteSocket(socketName); //为Zygote注册socket
        preload(); // 预加载类和资源
        SamplingProfilerIntegration.writeZygoteSnapshot();
        gcAndFinalize(); //GC操作
        if (startSystemServer) {
            startSystemServer(abiList, socketName);//启动system_server
        }
        runSelectLoop(abiList); //进入循环模式
        closeServerSocket();
    } catch (MethodAndArgsCaller caller) {
        caller.run(); //启动system_server中会讲到。
    } catch (RuntimeException ex) {
        closeServerSocket();
        throw ex;
    }
}

```

### ZygoteInit.registerZygoteSocket

```java
private static void registerZygoteSocket(String socketName) {
    if (sServerSocket == null) {
        int fileDesc;
        final String fullSocketName = ANDROID_SOCKET_PREFIX + socketName;
        try {
            String env = System.getenv(fullSocketName);
            fileDesc = Integer.parseInt(env);
        } catch (RuntimeException ex) {
            ...
        }

        try {
            FileDescriptor fd = new FileDescriptor();
            fd.setInt$(fileDesc); //设置文件描述符
            sServerSocket = new LocalServerSocket(fd); //创建Socket的本地服务端
        } catch (IOException ex) {
            ...
        }
    }
}
```

在这里就是实例化一个LocalServerSocket，这样zygote就可以作为服务端，不断的获取其它进程发送过来的请求

### ZygoteInit.preload

```java
static void preload() {
    //预加载位于/system/etc/preloaded-classes文件中的类
    preloadClasses();

    //预加载资源，包含drawable和color资源
    preloadResources();

    //预加载OpenGL
    preloadOpenGL();

    //通过System.loadLibrary()方法，
    //预加载"android","compiler_rt","jnigraphics"这3个共享库
    preloadSharedLibraries();

    //预加载  文本连接符资源
    preloadTextResources();

    //仅用于zygote进程，用于内存共享的进程
    WebViewFactory.prepareWebViewInZygote();
}
```

执行Zygote进程的初始化,对于类加载，采用反射机制Class.forName()方法来加载。对于资源加载，主要是 com.android.internal.R.array.preloaded\_drawables和com.android.internal.R.array.preloaded\_color\_state\_lists，在应用程序中以com.android.internal.R.xxx开头的资源，便是此时由Zygote加载到内存的

#### ZygoteInit.runSelectLoop

```java
private static void runSelectLoop(String abiList) throws MethodAndArgsCaller {
    ArrayList<FileDescriptor> fds = new ArrayList<FileDescriptor>();
    ArrayList<ZygoteConnection> peers = new ArrayList<ZygoteConnection>();
    //sServerSocket是socket通信中的服务端，即zygote进程。保存到fds[0]
    fds.add(sServerSocket.getFileDescriptor());
    peers.add(null);

    while (true) {
        StructPollfd[] pollFds = new StructPollfd[fds.size()];
        for (int i = 0; i < pollFds.length; ++i) {
            pollFds[i] = new StructPollfd();
            pollFds[i].fd = fds.get(i);
            pollFds[i].events = (short) POLLIN;
        }
        try {
             //处理轮询状态，当pollFds有事件到来则往下执行，否则阻塞在这里
            Os.poll(pollFds, -1);
        } catch (ErrnoException ex) {
            ...
        }
        
        for (int i = pollFds.length - 1; i >= 0; --i) {
            //采用I/O多路复用机制，当接收到客户端发出连接请求 或者数据处理请求到来，则往下执行；
            // 否则进入continue，跳出本次循环。
            if ((pollFds[i].revents & POLLIN) == 0) {
                continue;
            }
            if (i == 0) {
                //即fds[0]，代表的是sServerSocket，则意味着有客户端连接请求；
                // 则创建ZygoteConnection对象,并添加到fds。
                ZygoteConnection newPeer = acceptCommandPeer(abiList);
                peers.add(newPeer);
                fds.add(newPeer.getFileDesciptor()); //添加到fds.
            } else {
                //i>0，则代表通过socket接收来自对端的数据，并执行相应操作
                boolean done = peers.get(i).runOnce();
                if (done) {
                    peers.remove(i);
                    fds.remove(i); //处理完则从fds中移除该文件描述符
                }
            }
        }
    }
}

```

### ZygoteInit.acceptCommandPeer

```java
private static ZygoteConnection acceptCommandPeer(String abiList) {
    try {
        return new ZygoteConnection(sServerSocket.accept(), abiList);
    } catch (IOException ex) {
        ...
    }
}
```

### ZygoteConnection.runOnce

```java
boolean runOnce() throws ZygoteInit.MethodAndArgsCaller {

    String args[];
    Arguments parsedArgs = null;
    FileDescriptor[] descriptors;

    try {
        //读取socket客户端发送过来的参数列表
        args = readArgumentList();
        descriptors = mSocket.getAncillaryFileDescriptors();
    } catch (IOException ex) {
        ...
        return true;
    }
    ...

    try {
        //将binder客户端传递过来的参数，解析成Arguments对象格式
        parsedArgs = new Arguments(args);
        ...
        pid = Zygote.forkAndSpecialize(parsedArgs.uid, parsedArgs.gid, parsedArgs.gids,
                parsedArgs.debugFlags, rlimits, parsedArgs.mountExternal, parsedArgs.seInfo,
                parsedArgs.niceName, fdsToClose, parsedArgs.instructionSet,
                parsedArgs.appDataDir);
    } catch (Exception e) {
        ...
    }

    try {
        if (pid == 0) {
            //子进程执行
            IoUtils.closeQuietly(serverPipeFd);
            serverPipeFd = null;
            //进入子进程流程
            handleChildProc(parsedArgs, descriptors, childPipeFd, newStderr);
            return true;
        } else {
            //父进程执行
            IoUtils.closeQuietly(childPipeFd);
            childPipeFd = null;
            return handleParentProc(pid, descriptors, serverPipeFd, parsedArgs);
        }
    } finally {
        IoUtils.closeQuietly(childPipeFd);
        IoUtils.closeQuietly(serverPipeFd);
    }
}
```

接收客户端发送过来的connect()操作，Zygote作为服务端执行accept()操作。 再后面客户端调用write()写数据，Zygote进程调用read()读数据。

没有连接请求时会进入休眠状态，当有创建新进程的连接请求时，唤醒Zygote进程，创建Socket通道ZygoteConnection，然后执行ZygoteConnection的runOnce()方法。  
 

### Zygote总结：

+   解析init.zygote.rc中的参数，创建AppRuntime并调用AppRuntime.start()方法
+   调用AndroidRuntime的startVM()方法创建虚拟机，再调用startReg()注册JNI函数
+   通过JNI方式调用ZygoteInit.main()，第一次进入Java世界
+   registerZygoteSocket()建立socket通道，zygote作为通信的服务端，用于响应客户端请求
+   preload()预加载通用类、drawable和color资源、openGL以及共享库以及WebView，用于提高app启动效率
+   通过startSystemServer()，fork得力帮手system\_server进程，也是Java Framework的运行载体（下面讲到system server再详细讲解）
+   调用runSelectLoop()，随时待命，当接收到请求创建新进程请求时立即唤醒并执行相应工作

####   
System Server进程

  
system server进程和zygote进程可以说是Android世界中的两大最重要的进程，离开其中之一基本上系统就玩完了；基本上在Java Framework中的大多数服务都是在system server进程中一个线程的方式存在的，如下：

+   EntropyService    提供伪随机数
+   PowerManagerService    电源管理服务
+   ActivityManagerService    最核心的服务之一，管理 四大组件
+   TelephonyRegistry    通过该服务注册电话模块的事件响应，比如重启、关闭、启动等
+   PackageManagerService    程序包管理服务
+   AccountManagerService    账户管理服务，是指联系人账户，而不是 Linux 系统的账户
+   ContentService    ContentProvider 服务，提供跨进程数据交换
+   BatteryService    电池管理服务
+   LightsService    自然光强度感应传感器服务
+   VibratorService    震动器服务
+   AlarmManagerService    定时器管理服务，提供定时提醒服务
+   WindowManagerService    Framework 最核心的服务之一，负责窗口管理
+   BluetoothService    蓝牙服务
+   DevicePolicyManagerService    提供一些系统级别的设置及属性
+   StatusBarManagerService    状态栏管理服务
+   ClipboardService    系统剪切板服务
+   InputMethodManagerService    输入法管理服务
+   NetStatService    网络状态服务
+   NetworkManagementService    网络管理服务
+   ConnectivityService    网络连接管理服务
+   ThrottleService    暂不清楚其作用
+   AccessibilityManagerService    辅助管理程序截获所有的用户输入，并根据这些输入给用户一些额外的反馈，起到辅助的效果
+   MountService    挂载服务，可通过该服务调用 Linux 层面的 mount 程序
+   NotificationManagerService    通知栏管理服务， Android 中的通知栏和状态栏在一起，只是界面上前者在左边，后者在右边
+   DeviceStorageMonitorService    磁盘空间状态检测服务
+   LocationManagerService    地理位置服务
+   SearchManagerService    搜索管理服务
+   DropBoxManagerService    通过该服务访问 Linux 层面的 Dropbox 程序
+   WallpaperManagerService    墙纸管理服务，墙纸不等同于桌面背景，在 View 系统内部，墙纸可以作为任何窗口的背景
+   AudioService    音频管理服务
+   BackupManagerService    系统备份服务
+   AppWidgetService    Widget 服务
+   RecognitionManagerService    身份识别服务
+   DiskStatsService    磁盘统计服务

system server进程也是由zygote进程fork出来的，在上面的ZygoteInit.main方法中有如下代码  
 

### ZygoteInit.main

```java
public static void main(String argv[]) {
    try {
        if (startSystemServer) {
            startSystemServer(abiList, socketName);//启动system_server
        }
    } catch (MethodAndArgsCaller caller) {
        caller.run(); //这一步很重要，接下来会讲到
    } catch (RuntimeException ex) {
        closeServerSocket();
        throw ex;
    }
}
```

### ZygoteInit.startSystemServer

```java
private static boolean startSystemServer(String abiList, String socketName)
        throws MethodAndArgsCaller, RuntimeException {
    ...
    //参数准备
    String args[] = {
        "--setuid=1000",
        "--setgid=1000",
        "--setgroups=1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1018,1021,1032,3001,3002,3003,3006,3007",
        "--capabilities=" + capabilities + "," + capabilities,
        "--nice-name=system_server",
        "--runtime-args",
        "com.android.server.SystemServer",
    };

    ZygoteConnection.Arguments parsedArgs = null;
    int pid;
    try {
        //用于解析参数，生成目标格式
        parsedArgs = new ZygoteConnection.Arguments(args);
        ZygoteConnection.applyDebuggerSystemProperty(parsedArgs);
        ZygoteConnection.applyInvokeWithSystemProperty(parsedArgs);

        // fork子进程，该进程是system_server进程
        pid = Zygote.forkSystemServer(
                parsedArgs.uid, parsedArgs.gid,
                parsedArgs.gids,
                parsedArgs.debugFlags,
                null,
                parsedArgs.permittedCapabilities,
                parsedArgs.effectiveCapabilities);
    } catch (IllegalArgumentException ex) {
        throw new RuntimeException(ex);
    }

    //进入子进程system_server
    if (pid == 0) {
	    //第二个zygote进程
        if (hasSecondZygote(abiList)) {
            waitForSecondaryZygote(socketName);
        }
        // 完成system_server进程剩余的工作 
        handleSystemServerProcess(parsedArgs);
    }
    return true;
}

```

这个方法先准备参数，然后fork新进程，对于有两个zygote进程情况，需等待第2个zygote创建完成

### Zygote.forkSystemServer

```java
public static int forkSystemServer(int uid, int gid, int[] gids, int debugFlags,
        int[][] rlimits, long permittedCapabilities, long effectiveCapabilities) {
    VM_HOOKS.preFork();
    // 调用native方法fork system_server进程
    int pid = nativeForkSystemServer(
            uid, gid, gids, debugFlags, rlimits, permittedCapabilities, effectiveCapabilities);
    if (pid == 0) {
        Trace.setTracingEnabled(true);
    }
    VM_HOOKS.postForkCommon();
    return pid;
}
```

nativeForkSystemServer()方法在AndroidRuntime.cpp中注册的，调用com\_android\_internal\_os\_Zygote.cpp中的register\_com\_android\_internal\_os\_Zygote()方法建立native方法的映射关系，所以接下来进入如下方法

### com\_android\_internal\_os\_Zygote.nativeForkSystemServer

```cpp
static jint com_android_internal_os_Zygote_nativeForkSystemServer(
        JNIEnv* env, jclass, uid_t uid, gid_t gid, jintArray gids,
        jint debug_flags, jobjectArray rlimits, jlong permittedCapabilities,
        jlong effectiveCapabilities) {
  //fork子进程
  pid_t pid = ForkAndSpecializeCommon(env, uid, gid, gids,
                                      debug_flags, rlimits,
                                      permittedCapabilities, effectiveCapabilities,
                                      MOUNT_EXTERNAL_DEFAULT, NULL, NULL, true, NULL,
                                      NULL, NULL);
  if (pid > 0) {
      // zygote进程，检测system_server进程是否创建
      gSystemServerPid = pid;
      int status;
      if (waitpid(pid, &status, WNOHANG) == pid) {
          //当system_server进程死亡后，重启zygote进程
          RuntimeAbort(env);
      }
  }
  return pid;
}
```

当system\_server进程创建失败时，将会重启zygote进程。这里需要注意，对于Android 5.0以上系统，有两个zygote进程，分别是zygote、zygote64两个进程，system\_server的父进程，一般来说64位系统其父进程是zygote64进程

+   当kill system\_server进程后，只重启zygote64和system\_server，不重启zygote;
+   当kill zygote64进程后，只重启zygote64和system\_server，也不重启zygote；
+   当kill zygote进程，则重启zygote、zygote64以及system\_server。

#### com\_android\_internal\_os\_Zygote.ForkAndSpecializeCommon

```cpp
static pid_t ForkAndSpecializeCommon(JNIEnv* env, uid_t uid, gid_t gid, jintArray javaGids,
                                     jint debug_flags, jobjectArray javaRlimits,
                                     jlong permittedCapabilities, jlong effectiveCapabilities,
                                     jint mount_external,
                                     jstring java_se_info, jstring java_se_name,
                                     bool is_system_server, jintArray fdsToClose,
                                     jstring instructionSet, jstring dataDir) {
  SetSigChldHandler(); //设置子进程的signal信号处理函数
  pid_t pid = fork(); //fork子进程
  if (pid == 0) {
    //进入子进程
    DetachDescriptors(env, fdsToClose); //关闭并清除文件描述符

    if (!is_system_server) {
        //对于非system_server子进程，则创建进程组
        int rc = createProcessGroup(uid, getpid());
    }
    SetGids(env, javaGids); //设置设置group
    SetRLimits(env, javaRlimits); //设置资源limit

    int rc = setresgid(gid, gid, gid);
    rc = setresuid(uid, uid, uid);

    SetCapabilities(env, permittedCapabilities, effectiveCapabilities);
    SetSchedulerPolicy(env); //设置调度策略

     //selinux上下文
    rc = selinux_android_setcontext(uid, is_system_server, se_info_c_str, se_name_c_str);

    if (se_info_c_str == NULL && is_system_server) {
      se_name_c_str = "system_server";
    }
    if (se_info_c_str != NULL) {
      SetThreadName(se_name_c_str); //设置线程名为system_server，方便调试
    }
    UnsetSigChldHandler(); //设置子进程的signal信号处理函数为默认函数
    //等价于调用zygote.callPostForkChildHooks()
    env->CallStaticVoidMethod(gZygoteClass, gCallPostForkChildHooks, debug_flags,
                              is_system_server ? NULL : instructionSet);
    ...

  } else if (pid > 0) {
    //进入父进程，即zygote进程
  }
  return pid;
}

int fork() {
  __bionic_atfork_run_prepare(); 

  pthread_internal_t* self = __get_thread();

  //fork期间，获取父进程pid，并使其缓存值无效
  pid_t parent_pid = self->invalidate_cached_pid();
  //系统调用
  int result = syscall(__NR_clone, FORK_FLAGS, NULL, NULL, NULL, &(self->tid));
  if (result == 0) {
    self->set_cached_pid(gettid());
    __bionic_atfork_run_child(); //fork完成执行子进程回调方法
  } else {
    self->set_cached_pid(parent_pid);
    __bionic_atfork_run_parent(); //fork完成执行父进程回调方法
  }
  return result;
}
```

#### fork

fork()采用copy on write技术，这是linux创建进程的标准方法，调用一次，返回两次，返回值有3种类型

1.  父进程中，fork返回新创建的子进程的pid;
2.  子进程中，fork返回0；
3.  当出现错误时，fork返回负数。（当进程数超过上限或者系统内存不足时会出错）

fork()的主要工作是寻找空闲的进程号pid，然后从父进程拷贝进程信息，例如数据段和代码段，fork()后子进程要执行的代码等。 Zygote进程是所有Android进程的母体，包括system\_server和各个App进程。zygote利用fork()方法生成新进程，对于新进程A复用Zygote进程本身的资源，再加上新进程A相关的资源，构成新的应用进程A

  
![](https://i-blog.csdnimg.cn/blog_migrate/03610ec3e980feab3174ef10ab2fbccf.jpeg)

fork之后，操作系统会复制一个与父进程完全相同的子进程，虽说是父子关系，但是在操作系统看来，他们更像兄弟关系，这2个进程共享代码空间，但是数据空间是互相独立的，子进程数据空间中的内容是父进程的完整拷贝，指令指针也完全相同，子进程拥有父进程当前运行到的位置（两进程的程序计数器pc值相同，也就是说，子进程是从fork返回处开始执行的），但有一点不同，如果fork成功，子进程中fork的返回值是0，父进程中fork的返回值是子进程的进程号，如果fork不成功，父进程会返回错误。

可以这样想象，2个进程一直同时运行，而且步调一致，在fork之后，他们就开始分别作不同的工作，正如fork原意【分支】一样

到此system\_server进程已完成了创建的所有工作，接下来开始了system\_server进程的真正工作。在前面startSystemServer()方法中，zygote进程执行完forkSystemServer()后，新创建出来的system\_server进程便进入handleSystemServerProcess()方法  
 

### ZygoteInit.handleSystemServerProcess

```java
private static void handleSystemServerProcess(
        ZygoteConnection.Arguments parsedArgs)
        throws ZygoteInit.MethodAndArgsCaller {

    closeServerSocket(); //关闭父进程zygote复制而来的Socket

    Os.umask(S_IRWXG | S_IRWXO);

    if (parsedArgs.niceName != null) {
        Process.setArgV0(parsedArgs.niceName); //设置当前进程名为"system_server"
    }

    final String systemServerClasspath = Os.getenv("SYSTEMSERVERCLASSPATH");
    if (systemServerClasspath != null) {
        //执行dex优化操作
        performSystemServerDexOpt(systemServerClasspath);
    }

    if (parsedArgs.invokeWith != null) {
        String[] args = parsedArgs.remainingArgs;

        if (systemServerClasspath != null) {
            String[] amendedArgs = new String[args.length + 2];
            amendedArgs[0] = "-cp";
            amendedArgs[1] = systemServerClasspath;
            System.arraycopy(parsedArgs.remainingArgs, 0, amendedArgs, 2, parsedArgs.remainingArgs.length);
        }
        //启动应用进程
        WrapperInit.execApplication(parsedArgs.invokeWith,
                parsedArgs.niceName, parsedArgs.targetSdkVersion,
                VMRuntime.getCurrentInstructionSet(), null, args);
    } else {
        ClassLoader cl = null;
        if (systemServerClasspath != null) {
            创建类加载器，并赋予当前线程
            cl = new PathClassLoader(systemServerClasspath, ClassLoader.getSystemClassLoader());
            Thread.currentThread().setContextClassLoader(cl);
        }

        //system_server故进入此分支
        RuntimeInit.zygoteInit(parsedArgs.targetSdkVersion, parsedArgs.remainingArgs, cl);
    }
}

```

### RuntimeInit.zygoteInit

```java
public static final void zygoteInit(int targetSdkVersion, String[] argv, ClassLoader classLoader)
        throws ZygoteInit.MethodAndArgsCaller {

    Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "RuntimeInit");
    redirectLogStreams(); //重定向log输出

    commonInit(); // 通用的一些初始化
    nativeZygoteInit(); // zygote初始化 
    applicationInit(targetSdkVersion, argv, classLoader); // 应用初始化
}

private static final void commonInit() {
    // 设置默认的未捕捉异常处理方法
    Thread.setDefaultUncaughtExceptionHandler(new UncaughtHandler());

    // 设置市区，中国时区为"Asia/Shanghai"
    TimezoneGetter.setInstance(new TimezoneGetter() {
        @Override
        public String getId() {
            return SystemProperties.get("persist.sys.timezone");
        }
    });
    TimeZone.setDefault(null);

    //重置log配置
    LogManager.getLogManager().reset();
    new AndroidConfig();

    // 设置默认的HTTP User-agent格式( "Dalvik/1.1.0 (Linux; U; Android 6.0.1；LenovoX3c70 Build/LMY47V)"),用于 HttpURLConnection
    String userAgent = getDefaultUserAgent();
    System.setProperty("http.agent", userAgent);

    // 设置socket的tag，用于网络流量统计
    NetworkManagementSocketTagger.install();
}

private static void applicationInit(int targetSdkVersion, String[] argv, ClassLoader classLoader)
        throws ZygoteInit.MethodAndArgsCaller {
    //true代表应用程序退出时不调用AppRuntime.onExit()，否则会在退出前调用
    nativeSetExitWithoutCleanup(true);

    //设置虚拟机的内存利用率参数值为0.75
    VMRuntime.getRuntime().setTargetHeapUtilization(0.75f);
    VMRuntime.getRuntime().setTargetSdkVersion(targetSdkVersion);

    final Arguments args;
    try {
        args = new Arguments(argv); //解析参数
    } catch (IllegalArgumentException ex) {
        return;
    }

    Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);

    //调用startClass的static方法 main() 此处args.startClass为”com.android.server.SystemServer”
    invokeStaticMain(args.startClass, args.startArgs, classLoader);
}

private static void invokeStaticMain(String className, String[] argv, ClassLoader classLoader)
        throws ZygoteInit.MethodAndArgsCaller {
    Class<?> cl = Class.forName(className, true, classLoader);
    ...

    Method m;
    try {
        m = cl.getMethod("main", new Class[] { String[].class });
    } catch (NoSuchMethodException ex) {
        ...
    } catch (SecurityException ex) {
        ...
    }

    //通过抛出异常，回到ZygoteInit.main()。这样做好处是能清空栈帧，提高栈帧利用率
    throw new ZygoteInit.MethodAndArgsCaller(m, argv);
}

```

重点看最后一个方法，通过反射获取SystemServer类的main方法参数，然后抛出MethodAndArgsCaller异常；但是抛出异常后怎么弄呢，我们知道一个方法抛异常，会一直走到调用方法，直到一个方法捕获了异常，这里就是开头讲的，在ZygoteInit.main方法捕获了异常然后去执行  
 

### ZygoteInit.main

```java
public static void main(String argv[]) {
    try {
        if (startSystemServer) {
            startSystemServer(abiList, socketName);//启动system_server
        }
    } catch (MethodAndArgsCaller caller) {
        caller.run(); //这一步很重要，接下来会讲到
    } catch (RuntimeException ex) {
        closeServerSocket();
        throw ex;
    }
}
```

但是为啥没有直接在startSystemServer()或者上面的方法中直接调用SystemServer类的main方法，而是通过抛异常的方式处理呢？

我们知道，当一个函数抛出异常后，这个异常会依次传递给调用它的函数，直到这个异常被捕获，如果这个异常一直没有被处理，最终就会引起程序的崩溃。

程序都是由一个个函数组成的(除了汇编程序)，c/c++/java/…等高级语言编写的应用程序，在执行的时候，他们都拥有自己的栈空间（是一种先进后出的内存区域），用于存放函数的返回地址和函数的临时数据，每调用一个函数时，就会把函数的返回地址和相关数据压入栈中，当一个函数执行完后，就会从栈中弹出，cpu会根据函数的返回地址，执行上一个调用函数的下一条指令。

所以，在抛出异常后，如果异常没有在当前的函数中捕获，那么当前的函数执行就会异常的退出，从应用程序的栈弹出，并将这个异常传递给上一个函数，直到异常被捕获处理，否则，就会引起程序的崩溃。

因此，这里通过抛异常的方式启动主要是清理应用程序栈中ZygoteInit.main以上的函数栈帧，以实现当相应的main函数退出时，能直接退出整个应用程序  
 

### ZygoteInit.MethodAndArgsCaller.run

```java
public static class MethodAndArgsCaller extends Exception
            implements Runnable {
        /** 调用的方法 在上面的方法可知是main方法 */
        private final Method mMethod;

        /** 参数列表 */
        private final String[] mArgs;

        public MethodAndArgsCaller(Method method, String[] args) {
            mMethod = method;
            mArgs = args;
        }

        public void run() {
            try {
                mMethod.invoke(null, new Object[] { mArgs });
            } catch (IllegalAccessException ex) {
                throw new RuntimeException(ex);
            } catch (InvocationTargetException ex) {
                Throwable cause = ex.getCause();
                if (cause instanceof RuntimeException) {
                    throw (RuntimeException) cause;
                } else if (cause instanceof Error) {
                    throw (Error) cause;
                }
                throw new RuntimeException(ex);
            }
        }
    }
```

可以看到这里根据传递过来的参数，可知此处通过反射机制调用的是SystemServer.main()方法；到此，总算是进入到了SystemServer类的main()方法

### SystemServer.main

```java
public static void main(String[] args) {
        new SystemServer().run();
}

private void run() {
        try {
           	//如果系统时间比1970年早，那就设置为1970
            if (System.currentTimeMillis() < EARLIEST_SUPPORTED_TIME) {
                SystemClock.setCurrentTimeMillis(EARLIEST_SUPPORTED_TIME);
            }

            //变更虚拟机的库文件，对于Android 6.0默认采用的是libart.so
            SystemProperties.set("persist.sys.dalvik.vm.lib.2", VMRuntime.getRuntime().vmLibrary());

            //清除vm内存增长上限，由于启动过程需要较多的虚拟机内存空间
            VMRuntime.getRuntime().clearGrowthLimit();

            //设置内存的可能有效使用率为0.8
            VMRuntime.getRuntime().setTargetHeapUtilization(0.8f);

            // 针对部分设备依赖于运行时就产生指纹信息，因此需要在开机完成前已经定义
            Build.ensureFingerprintProperty();

            //访问环境变量前，需要明确地指定用户
            Environment.setUserRequired(true);

            // Within the system server, any incoming Bundles should be defused
            // to avoid throwing BadParcelableException.
            BaseBundle.setShouldDefuse(true);

            //确保当前系统进程的binder调用，总是运行在前台优先级(foreground priority)
            BinderInternal.disableBackgroundScheduling(true);

            // 增加system_server中的binder线程数
            BinderInternal.setMaxThreads(sMaxBinderThreads);

            // 创建主线程looper 在当前线程运行
            android.os.Process.setThreadPriority(
                android.os.Process.THREAD_PRIORITY_FOREGROUND);
            android.os.Process.setCanSelfBackground(false);
            Looper.prepareMainLooper();

            //初始化android_servers库
            System.loadLibrary("android_servers");

            //检测上次关机过程是否失败
            performPendingShutdown();

            //初始化系统上下文
            //初始化系统上下文对象mSystemContext，并设置默认的主题,mSystemContext实际上是一个ContextImpl对象。
            //调用ActivityThread.systemMain()的时候，会调用ActivityThread.attach(true)，而在attach()里面，则创建了Application对象，并调用了Application.onCreate()。
            createSystemContext();

            //创建系统服务管理
            mSystemServiceManager = new SystemServiceManager(mSystemContext);
            LocalServices.addService(SystemServiceManager.class, mSystemServiceManager);
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
        }

        //启动各种系统服务
        try {
	        //引导服务
            startBootstrapServices();
            //核心服务
            startCoreServices();
            //其它服务
            startOtherServices();
        } catch (Throwable ex) {
            throw ex;
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_SYSTEM_SERVER);
        }

        //开启消息循环
        Looper.loop();
        throw new RuntimeException("Main thread loop unexpectedly exited");
    }
```

#### system server进程总结

+   zygote进程通过Zygote.forkSystemServer —> fork.fork()创建system server进程
+   调用ZygoteInit.handleSystemServerProcess方法设置当前进程名为"system\_server"，执行dex优化操作，一些属性的初始化，设置binder线程
+   通过抛出MethodAndArgsCaller异常，回到ZygoteInit.main()，在try catch中执行MethodAndArgsCaller.run；在这里通过反射执行SystemServer.main()方法
+   在SystemServer.run方法中做一些设置，比如初始化系统上下文 ，创建SystemServiceManager，启动引导服务，启动核心服务，启动其他服务
+   最后调用Looper.loop()，不断的从消息队列取出消息处理