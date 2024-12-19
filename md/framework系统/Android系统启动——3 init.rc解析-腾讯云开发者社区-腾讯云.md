

本篇文章的主要内容如下：

-   1、init.rc文件格式
-   2、init.rc脚本语法简介
-   3、init.rc
-   4、init.rc文件的解析
-   5、init.rc脚本语法简介
-   6、init总结

### 一、init.rc文件格式

init.rc文件是以“块”(section)为单位服务的，，一个“块”(section)可以包含多行。“块”(section)分成两大类：一类称为"动作(action)"，另一类称为“服务(service)”。

-   动作(action)：以关键字"on" 开头，表示一堆命令
-   服务(service)：以关键字“service”开头，表示启动某个进程的方式和参数

"块"(section)以关键字"on"或者"service"开始，直到下一个"on"或者"service"结束，中间所有行都属于这个"块"(section)

> PS：空行或注释行没有分割作用，注释用'#'开始

**无论是“动作(action)”块还是“服务(service)”块，并不是按照文件中的编码排序逐一执行的**

### 二、init.rc脚本语法简介

> 上面说到了init.rc的脚本语法很简答，那我们就来简单的了解下

为了让我们更好的理解init.rc脚本，我们先来回顾下init.rc脚本的语法。关于init.rc的脚本介绍比较少，目前网络的主流推荐的文档就是[init/readme.txt](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Freadme.txt&objectId=1199504&objectType=1&isNewArticle=undefined)，大家可以点击进去看下。

一个init.rc脚本由4个类型的声明组成，即

-   Action——行为/动作
-   commands——命令/启动
-   services—— 服务
-   Options—— 选项

Action(动作)和service(服务)暗示着一个新语句的开始，这两个关键字后面跟着commands(命令)或者options(选项)都属于这个新语句。

> PS:如果Action(动作)和services(服务)有唯一的名字，如果出现和已有动作或服务重名的，将会被当成错误忽略掉。

这里补充一下语法：

-   C语言风格的反斜杠转义字符("")可以用来为参数添加空格。
-   关键字和参数以空格分隔，每个语句以行为单位。
-   行尾的反斜杠用来表示下面一行是同一行
-   为了防止字符串中的空格把其切割成多个部分，我们需要对其使用双引号
-   注释以“#”开头

下面我们就来依次简单介绍下

##### (一)、Action(动作)

动作的一般格式如下：

```
on  <trigger>           ## 触发条件
      <command>     ##执行命令
      <command1>   ##可以执行多个命令
```

从上面，我们可以知道，一个Action其实就是响应某事件的过程。即当<trigger>所描述的触发事件产生时，依次执行各种command(同一事件可以对应多个命令)。从源码实现的角度来说，就会把它加入"命令执行队列"的尾部(除非这个Action在队列中已经存在了)，然后系统再对这些命令按顺序执行。

###### 1、触发器(trigger)

在"动作"(action)里面的，on后面跟着的字符串是触发器(trigger)，trigger是一个用于匹配某种事件类型的字符串，它将对应的Action的执行。

触发器(trigger)有几种格式：

-   1、最简单的一种是一个单纯的字符串。比如“on boot”。这种简单的格式可以使用命令"trigger"来触发。
-   2、还有一种常见的格式是"on property<属性>=<值>"。如果属性值在运行时设成了指定的值，则"块"(action)中的命令列表就会执行。

常见的如下：

-   on on early-init：在初始化早期阶段触发
-   on init：在初始化阶段触发
-   on late-init：在初始化晚期阶段触发
-   on boot/charger：当系统启动/充电时触发
-   on property：当属性值满足条件时触发

##### (二)、commands(命令)

command是action的命令列表中的命令，或者是service中的选项 onrestart 的参数命令 命令将在所属事件发生时被一个个地执行 下面罗列出init中定义的一些常见事件



![](https://ask.qcloudimg.com/http-save/yehe-2957818/fabxqjzwb9.png)



image.png

针对这些事件，如下表命令可供使用



![](https://ask.qcloudimg.com/http-save/yehe-2957818/rwkl5jf69b.png)



image.png

##### (三)、services 服务

"服务"(service)的关键字 service后面跟着的是服务名称。我们可以使用"start"命令加"服务名称"来启动一个服务。关键字以下的行称为"选项"，没一个选项占用一行，选项也有多种，常见的"class"表示服务所属的类别。

services 其实是可执行程序，它们在特定选项的约束下是被init程序运行或者重启(service可以在配置中指定是否需要退出重启，这样当service出现异常crash时就可以有机会复原)

```
service <name><pathname> [ <argument> ]*
    <option>
    <option>
```

我们简单的解释上面的参数

-   <name>：表示此service的名称
-   <pathname>：此service所在路径。因为是可执行文件，所以一定有存储路径
-   <argument>：启动service所带的参数
-   <option>：对此service的约束选项

##### (四)、options(选项)

options是Service的修订项。它们决定一个Service何时以及如何运行。 services中可用选项如下表



![](https://ask.qcloudimg.com/http-save/yehe-2957818/om9p3aizfq.png)



image.png

default: 意味着disabled=false，oneshot=false，critical=false。

### 三、init.rc

[init.rc地址](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Frootdir%25252Finit.rc&objectId=1199504&objectType=1&isNewArticle=undefined)

[[md/个人/init.rc|init.rc]]

```
# Copyright (C) 2012 The Android Open Source Project
#
# IMPORTANT: Do not create world writable files or directories.
# This is a common source of Android security bugs.
#

import /init.environ.rc
import /init.usb.rc
import /init.${ro.hardware}.rc
import /init.usb.configfs.rc
import /init.${ro.zygote}.rc
import /init.trace.rc

on early-init
    # Set init and its forked children's oom_adj.
    write /proc/1/oom_score_adj -1000

    # Set the security context of /adb_keys if present.
    restorecon /adb_keys

    start ueventd

on init
    sysclktz 0

    # Backward compatibility.
    symlink /system/etc /etc
    symlink /sys/kernel/debug /d

    # Link /vendor to /system/vendor for devices without a vendor partition.
    symlink /system/vendor /vendor

    # Create cgroup mount point for cpu accounting
    mkdir /acct
    mount cgroup none /acct cpuacct
    mkdir /acct/uid

    # Create cgroup mount point for memory
    mount tmpfs none /sys/fs/cgroup mode=0750,uid=0,gid=1000
    mkdir /sys/fs/cgroup/memory 0750 root system
    mount cgroup none /sys/fs/cgroup/memory memory
    write /sys/fs/cgroup/memory/memory.move_charge_at_immigrate 1
    chown root system /sys/fs/cgroup/memory/tasks
    chmod 0660 /sys/fs/cgroup/memory/tasks
    mkdir /sys/fs/cgroup/memory/sw 0750 root system
    write /sys/fs/cgroup/memory/sw/memory.swappiness 100
    write /sys/fs/cgroup/memory/sw/memory.move_charge_at_immigrate 1
    chown root system /sys/fs/cgroup/memory/sw/tasks
    chmod 0660 /sys/fs/cgroup/memory/sw/tasks

    mkdir /system
    mkdir /data 0771 system system
    mkdir /cache 0770 system cache
    mkdir /config 0500 root root

    # Mount staging areas for devices managed by vold
    # See storage config details at http://source.android.com/tech/storage/
    mkdir /mnt 0755 root system
    mount tmpfs tmpfs /mnt mode=0755,uid=0,gid=1000
    restorecon_recursive /mnt

    mkdir /mnt/secure 0700 root root
    mkdir /mnt/secure/asec 0700 root root
    mkdir /mnt/asec 0755 root system
    mkdir /mnt/obb 0755 root system
    mkdir /mnt/media_rw 0750 root media_rw
    mkdir /mnt/user 0755 root root
    mkdir /mnt/user/0 0755 root root
    mkdir /mnt/expand 0771 system system

    # Storage views to support runtime permissions
    mkdir /storage 0755 root root
    mkdir /mnt/runtime 0700 root root
    mkdir /mnt/runtime/default 0755 root root
    mkdir /mnt/runtime/default/self 0755 root root
    mkdir /mnt/runtime/read 0755 root root
    mkdir /mnt/runtime/read/self 0755 root root
    mkdir /mnt/runtime/write 0755 root root
    mkdir /mnt/runtime/write/self 0755 root root

    # Symlink to keep legacy apps working in multi-user world
    symlink /storage/self/primary /sdcard
    symlink /mnt/user/0/primary /mnt/runtime/default/self/primary

    # memory control cgroup
    mkdir /dev/memcg 0700 root system
    mount cgroup none /dev/memcg memory

    write /proc/sys/kernel/panic_on_oops 1
    write /proc/sys/kernel/hung_task_timeout_secs 0
    write /proc/cpu/alignment 4

    # scheduler tunables
    # Disable auto-scaling of scheduler tunables with hotplug. The tunables
    # will vary across devices in unpredictable ways if allowed to scale with
    # cpu cores.
    write /proc/sys/kernel/sched_tunable_scaling 0
    write /proc/sys/kernel/sched_latency_ns 10000000
    write /proc/sys/kernel/sched_wakeup_granularity_ns 2000000
    write /proc/sys/kernel/sched_compat_yield 1
    write /proc/sys/kernel/sched_child_runs_first 0

    write /proc/sys/kernel/randomize_va_space 2
    write /proc/sys/kernel/kptr_restrict 2
    write /proc/sys/vm/mmap_min_addr 32768
    write /proc/sys/net/ipv4/ping_group_range "0 2147483647"
    write /proc/sys/net/unix/max_dgram_qlen 300
    write /proc/sys/kernel/sched_rt_runtime_us 950000
    write /proc/sys/kernel/sched_rt_period_us 1000000

    # reflect fwmark from incoming packets onto generated replies
    write /proc/sys/net/ipv4/fwmark_reflect 1
    write /proc/sys/net/ipv6/fwmark_reflect 1

    # set fwmark on accepted sockets
    write /proc/sys/net/ipv4/tcp_fwmark_accept 1

    # disable icmp redirects
    write /proc/sys/net/ipv4/conf/all/accept_redirects 0
    write /proc/sys/net/ipv6/conf/all/accept_redirects 0

    # Create cgroup mount points for process groups
    mkdir /dev/cpuctl
    mount cgroup none /dev/cpuctl cpu
    chown system system /dev/cpuctl
    chown system system /dev/cpuctl/tasks
    chmod 0666 /dev/cpuctl/tasks
    write /dev/cpuctl/cpu.shares 1024
    write /dev/cpuctl/cpu.rt_runtime_us 800000
    write /dev/cpuctl/cpu.rt_period_us 1000000

    mkdir /dev/cpuctl/bg_non_interactive
    chown system system /dev/cpuctl/bg_non_interactive/tasks
    chmod 0666 /dev/cpuctl/bg_non_interactive/tasks
    # 5.0 %
    write /dev/cpuctl/bg_non_interactive/cpu.shares 52
    write /dev/cpuctl/bg_non_interactive/cpu.rt_runtime_us 700000
    write /dev/cpuctl/bg_non_interactive/cpu.rt_period_us 1000000

    # sets up initial cpusets for ActivityManager
    mkdir /dev/cpuset
    mount cpuset none /dev/cpuset

    # this ensures that the cpusets are present and usable, but the device's
    # init.rc must actually set the correct cpus
    mkdir /dev/cpuset/foreground
    write /dev/cpuset/foreground/cpus 0
    write /dev/cpuset/foreground/mems 0
    mkdir /dev/cpuset/foreground/boost
    write /dev/cpuset/foreground/boost/cpus 0
    write /dev/cpuset/foreground/boost/mems 0
    mkdir /dev/cpuset/background
    write /dev/cpuset/background/cpus 0
    write /dev/cpuset/background/mems 0

    # system-background is for system tasks that should only run on
    # little cores, not on bigs
    # to be used only by init, so don't change system-bg permissions
    mkdir /dev/cpuset/system-background
    write /dev/cpuset/system-background/cpus 0
    write /dev/cpuset/system-background/mems 0

    # change permissions for all cpusets we'll touch at runtime
    chown system system /dev/cpuset
    chown system system /dev/cpuset/foreground
    chown system system /dev/cpuset/foreground/boost
    chown system system /dev/cpuset/background
    chown system system /dev/cpuset/tasks
    chown system system /dev/cpuset/foreground/tasks
    chown system system /dev/cpuset/foreground/boost/tasks
    chown system system /dev/cpuset/background/tasks
    chmod 0664 /dev/cpuset/foreground/tasks
    chmod 0664 /dev/cpuset/foreground/boost/tasks
    chmod 0664 /dev/cpuset/background/tasks
    chmod 0664 /dev/cpuset/tasks


    # qtaguid will limit access to specific data based on group memberships.
    #   net_bw_acct grants impersonation of socket owners.
    #   net_bw_stats grants access to other apps' detailed tagged-socket stats.
    chown root net_bw_acct /proc/net/xt_qtaguid/ctrl
    chown root net_bw_stats /proc/net/xt_qtaguid/stats

    # Allow everybody to read the xt_qtaguid resource tracking misc dev.
    # This is needed by any process that uses socket tagging.
    chmod 0644 /dev/xt_qtaguid

    # Create location for fs_mgr to store abbreviated output from filesystem
    # checker programs.
    mkdir /dev/fscklogs 0770 root system

    # pstore/ramoops previous console log
    mount pstore pstore /sys/fs/pstore
    chown system log /sys/fs/pstore/console-ramoops
    chmod 0440 /sys/fs/pstore/console-ramoops
    chown system log /sys/fs/pstore/pmsg-ramoops-0
    chmod 0440 /sys/fs/pstore/pmsg-ramoops-0

    # enable armv8_deprecated instruction hooks
    write /proc/sys/abi/swp 1

# Healthd can trigger a full boot from charger mode by signaling this
# property when the power button is held.
on property:sys.boot_from_charger_mode=1
    class_stop charger
    trigger late-init

# Load properties from /system/ + /factory after fs mount.
on load_system_props_action
    load_system_props

on load_persist_props_action
    load_persist_props
    start logd
    start logd-reinit

# Indicate to fw loaders that the relevant mounts are up.
on firmware_mounts_complete
    rm /dev/.booting

# Mount filesystems and start core system services.
on late-init
    trigger early-fs
    trigger fs
    trigger post-fs

    # Load properties from /system/ + /factory after fs mount. Place
    # this in another action so that the load will be scheduled after the prior
    # issued fs triggers have completed.
    trigger load_system_props_action

    # Now we can mount /data. File encryption requires keymaster to decrypt
    # /data, which in turn can only be loaded when system properties are present
    trigger post-fs-data
    trigger load_persist_props_action

    # Remove a file to wake up anything waiting for firmware.
    trigger firmware_mounts_complete

    trigger early-boot
    trigger boot


on post-fs
    start logd
    # once everything is setup, no need to modify /
    mount rootfs rootfs / ro remount
    # Mount shared so changes propagate into child namespaces
    mount rootfs rootfs / shared rec
    # Mount default storage into root namespace
    mount none /mnt/runtime/default /storage slave bind rec

    # We chown/chmod /cache again so because mount is run as root + defaults
    chown system cache /cache
    chmod 0770 /cache
    # We restorecon /cache in case the cache partition has been reset.
    restorecon_recursive /cache

    # Create /cache/recovery in case it's not there. It'll also fix the odd
    # permissions if created by the recovery system.
    mkdir /cache/recovery 0770 system cache

    #change permissions on vmallocinfo so we can grab it from bugreports
    chown root log /proc/vmallocinfo
    chmod 0440 /proc/vmallocinfo

    chown root log /proc/slabinfo
    chmod 0440 /proc/slabinfo

    #change permissions on kmsg & sysrq-trigger so bugreports can grab kthread stacks
    chown root system /proc/kmsg
    chmod 0440 /proc/kmsg
    chown root system /proc/sysrq-trigger
    chmod 0220 /proc/sysrq-trigger
    chown system log /proc/last_kmsg
    chmod 0440 /proc/last_kmsg

    # make the selinux kernel policy world-readable
    chmod 0444 /sys/fs/selinux/policy

    # create the lost+found directories, so as to enforce our permissions
    mkdir /cache/lost+found 0770 root root

on post-fs-data
    # We chown/chmod /data again so because mount is run as root + defaults
    chown system system /data
    chmod 0771 /data
    # We restorecon /data in case the userdata partition has been reset.
    restorecon /data

    # Emulated internal storage area
    mkdir /data/media 0770 media_rw media_rw

    # Make sure we have the device encryption key
    start logd
    start vold
    installkey /data

    # Start bootcharting as soon as possible after the data partition is
    # mounted to collect more data.
    mkdir /data/bootchart 0755 shell shell
    bootchart_init

    # Avoid predictable entropy pool. Carry over entropy from previous boot.
    copy /data/system/entropy.dat /dev/urandom

    # create basic filesystem structure
    mkdir /data/misc 01771 system misc
    mkdir /data/misc/adb 02750 system shell
    mkdir /data/misc/bluedroid 02770 bluetooth net_bt_stack
    # Fix the access permissions and group ownership for 'bt_config.conf'
    chmod 0660 /data/misc/bluedroid/bt_config.conf
    chown bluetooth net_bt_stack /data/misc/bluedroid/bt_config.conf
    mkdir /data/misc/bluetooth 0770 system system
    mkdir /data/misc/keystore 0700 keystore keystore
    mkdir /data/misc/gatekeeper 0700 system system
    mkdir /data/misc/keychain 0771 system system
    mkdir /data/misc/net 0750 root shell
    mkdir /data/misc/radio 0770 system radio
    mkdir /data/misc/sms 0770 system radio
    mkdir /data/misc/zoneinfo 0775 system system
    mkdir /data/misc/V** 0770 system V**
    mkdir /data/misc/shared_relro 0771 shared_relro shared_relro
    mkdir /data/misc/systemkeys 0700 system system
    mkdir /data/misc/wifi 0770 wifi wifi
    mkdir /data/misc/wifi/sockets 0770 wifi wifi
    mkdir /data/misc/wifi/wpa_supplicant 0770 wifi wifi
    mkdir /data/misc/ethernet 0770 system system
    mkdir /data/misc/dhcp 0770 dhcp dhcp
    mkdir /data/misc/user 0771 root root
    mkdir /data/misc/perfprofd 0775 root root
    # give system access to wpa_supplicant.conf for backup and restore
    chmod 0660 /data/misc/wifi/wpa_supplicant.conf
    mkdir /data/local 0751 root root
    mkdir /data/misc/media 0700 media media
    mkdir /data/misc/vold 0700 root root

    # For security reasons, /data/local/tmp should always be empty.
    # Do not place files or directories in /data/local/tmp
    mkdir /data/local/tmp 0771 shell shell
    mkdir /data/data 0771 system system
    mkdir /data/app-private 0771 system system
    mkdir /data/app-asec 0700 root root
    mkdir /data/app-lib 0771 system system
    mkdir /data/app 0771 system system
    mkdir /data/property 0700 root root
    mkdir /data/tombstones 0771 system system

    # create dalvik-cache, so as to enforce our permissions
    mkdir /data/dalvik-cache 0771 root root
    mkdir /data/dalvik-cache/profiles 0711 system system

    # create resource-cache and double-check the perms
    mkdir /data/resource-cache 0771 system system
    chown system system /data/resource-cache
    chmod 0771 /data/resource-cache

    # create the lost+found directories, so as to enforce our permissions
    mkdir /data/lost+found 0770 root root

    # create directory for DRM plug-ins - give drm the read/write access to
    # the following directory.
    mkdir /data/drm 0770 drm drm

    # create directory for MediaDrm plug-ins - give drm the read/write access to
    # the following directory.
    mkdir /data/mediadrm 0770 mediadrm mediadrm

    mkdir /data/adb 0700 root root

    # symlink to bugreport storage location
    symlink /data/data/com.android.shell/files/bugreports /data/bugreports

    # Separate location for storing security policy files on data
    mkdir /data/security 0711 system system

    # Create all remaining /data root dirs so that they are made through init
    # and get proper encryption policy installed
    mkdir /data/backup 0700 system system
    mkdir /data/media 0770 media_rw media_rw
    mkdir /data/ss 0700 system system
    mkdir /data/system 0775 system system
    mkdir /data/system/heapdump 0700 system system
    mkdir /data/user 0711 system system

    setusercryptopolicies /data/user

    # Reload policy from /data/security if present.
    setprop selinux.reload_policy 1

    # Set SELinux security contexts on upgrade or policy update.
    restorecon_recursive /data

    # Check any timezone data in /data is newer than the copy in /system, delete if not.
    exec - system system -- /system/bin/tzdatacheck /system/usr/share/zoneinfo /data/misc/zoneinfo

    # If there is no fs-post-data action in the init.<device>.rc file, you
    # must uncomment this line, otherwise encrypted filesystems
    # won't work.
    # Set indication (checked by vold) that we have finished this action
    #setprop vold.post_fs_data_done 1

on boot
    # basic network init
    ifup lo
    hostname localhost
    domainname localdomain

    # set RLIMIT_NICE to allow priorities from 19 to -20
    setrlimit 13 40 40

    # Memory management.  Basic kernel parameters, and allow the high
    # level system server to be able to adjust the kernel OOM driver
    # parameters to match how it is managing things.
    write /proc/sys/vm/overcommit_memory 1
    write /proc/sys/vm/min_free_order_shift 4
    chown root system /sys/module/lowmemorykiller/parameters/adj
    chmod 0664 /sys/module/lowmemorykiller/parameters/adj
    chown root system /sys/module/lowmemorykiller/parameters/minfree
    chmod 0664 /sys/module/lowmemorykiller/parameters/minfree

    # Tweak background writeout
    write /proc/sys/vm/dirty_expire_centisecs 200
    write /proc/sys/vm/dirty_background_ratio  5

    # Permissions for System Server and daemons.
    chown radio system /sys/android_power/state
    chown radio system /sys/android_power/request_state
    chown radio system /sys/android_power/acquire_full_wake_lock
    chown radio system /sys/android_power/acquire_partial_wake_lock
    chown radio system /sys/android_power/release_wake_lock
    chown system system /sys/power/autosleep
    chown system system /sys/power/state
    chown system system /sys/power/wakeup_count
    chown radio system /sys/power/wake_lock
    chown radio system /sys/power/wake_unlock
    chmod 0660 /sys/power/state
    chmod 0660 /sys/power/wake_lock
    chmod 0660 /sys/power/wake_unlock

    chown system system /sys/devices/system/cpu/cpufreq/interactive/timer_rate
    chmod 0660 /sys/devices/system/cpu/cpufreq/interactive/timer_rate
    chown system system /sys/devices/system/cpu/cpufreq/interactive/timer_slack
    chmod 0660 /sys/devices/system/cpu/cpufreq/interactive/timer_slack
    chown system system /sys/devices/system/cpu/cpufreq/interactive/min_sample_time
    chmod 0660 /sys/devices/system/cpu/cpufreq/interactive/min_sample_time
    chown system system /sys/devices/system/cpu/cpufreq/interactive/hispeed_freq
    chmod 0660 /sys/devices/system/cpu/cpufreq/interactive/hispeed_freq
    chown system system /sys/devices/system/cpu/cpufreq/interactive/target_loads
    chmod 0660 /sys/devices/system/cpu/cpufreq/interactive/target_loads
    chown system system /sys/devices/system/cpu/cpufreq/interactive/go_hispeed_load
    chmod 0660 /sys/devices/system/cpu/cpufreq/interactive/go_hispeed_load
    chown system system /sys/devices/system/cpu/cpufreq/interactive/above_hispeed_delay
    chmod 0660 /sys/devices/system/cpu/cpufreq/interactive/above_hispeed_delay
    chown system system /sys/devices/system/cpu/cpufreq/interactive/boost
    chmod 0660 /sys/devices/system/cpu/cpufreq/interactive/boost
    chown system system /sys/devices/system/cpu/cpufreq/interactive/boostpulse
    chown system system /sys/devices/system/cpu/cpufreq/interactive/input_boost
    chmod 0660 /sys/devices/system/cpu/cpufreq/interactive/input_boost
    chown system system /sys/devices/system/cpu/cpufreq/interactive/boostpulse_duration
    chmod 0660 /sys/devices/system/cpu/cpufreq/interactive/boostpulse_duration
    chown system system /sys/devices/system/cpu/cpufreq/interactive/io_is_busy
    chmod 0660 /sys/devices/system/cpu/cpufreq/interactive/io_is_busy

    # Assume SMP uses shared cpufreq policy for all CPUs
    chown system system /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq
    chmod 0660 /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq

    chown system system /sys/class/timed_output/vibrator/enable
    chown system system /sys/class/leds/keyboard-backlight/brightness
    chown system system /sys/class/leds/lcd-backlight/brightness
    chown system system /sys/class/leds/button-backlight/brightness
    chown system system /sys/class/leds/jogball-backlight/brightness
    chown system system /sys/class/leds/red/brightness
    chown system system /sys/class/leds/green/brightness
    chown system system /sys/class/leds/blue/brightness
    chown system system /sys/class/leds/red/device/grpfreq
    chown system system /sys/class/leds/red/device/grppwm
    chown system system /sys/class/leds/red/device/blink
    chown system system /sys/class/timed_output/vibrator/enable
    chown system system /sys/module/sco/parameters/disable_esco
    chown system system /sys/kernel/ipv4/tcp_wmem_min
    chown system system /sys/kernel/ipv4/tcp_wmem_def
    chown system system /sys/kernel/ipv4/tcp_wmem_max
    chown system system /sys/kernel/ipv4/tcp_rmem_min
    chown system system /sys/kernel/ipv4/tcp_rmem_def
    chown system system /sys/kernel/ipv4/tcp_rmem_max
    chown root radio /proc/cmdline

    # Define default initial receive window size in segments.
    setprop net.tcp.default_init_rwnd 60

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
    start logd
    start logd-reinit

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


## Daemon processes to be run by init.
##
service ueventd /sbin/ueventd
    class core
    critical
    seclabel u:r:ueventd:s0

service logd /system/bin/logd
    class core
    socket logd stream 0666 logd logd
    socket logdr seqpacket 0666 logd logd
    socket logdw dgram 0222 logd logd
    group root system
     writepid /dev/cpuset/system-background/tasks

service logd-reinit /system/bin/logd --reinit
    oneshot
    writepid /dev/cpuset/system-background/tasks
    disabled

service healthd /sbin/healthd
    class core
    critical
    seclabel u:r:healthd:s0
    group root system

service console /system/bin/sh
    class core
    console
    disabled
    user shell
    group shell log
    seclabel u:r:shell:s0

on property:ro.debuggable=1
    start console

# adbd is controlled via property triggers in init.<platform>.usb.rc
service adbd /sbin/adbd --root_seclabel=u:r:su:s0
    class core
    socket adbd stream 660 system system
    disabled
    seclabel u:r:adbd:s0

# adbd on at boot in emulator
on property:ro.kernel.qemu=1
    start adbd

service lmkd /system/bin/lmkd
    class core
    critical
    socket lmkd seqpacket 0660 system system
    writepid /dev/cpuset/system-background/tasks

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

service vold /system/bin/vold \
        --blkid_context=u:r:blkid:s0 --blkid_untrusted_context=u:r:blkid_untrusted:s0 \
        --fsck_context=u:r:fsck:s0 --fsck_untrusted_context=u:r:fsck_untrusted:s0
    class core
    socket vold stream 0660 root mount
    socket cryptd stream 0660 root mount
    ioprio be 2

service netd /system/bin/netd
    class main
    socket netd stream 0660 root system
    socket dnsproxyd stream 0660 root inet
    socket mdns stream 0660 root system
    socket fwmarkd stream 0660 root inet

service debuggerd /system/bin/debuggerd
    class main
    writepid /dev/cpuset/system-background/tasks

service debuggerd64 /system/bin/debuggerd64
    class main
    writepid /dev/cpuset/system-background/tasks

service ril-daemon /system/bin/rild
    class main
    socket rild stream 660 root radio
    socket sap_uim_socket1 stream 660 bluetooth bluetooth
    socket rild-debug stream 660 radio system
    user root
    group radio cache inet misc audio log

service surfaceflinger /system/bin/surfaceflinger
    class core
    user system
    group graphics drmrpc
    onrestart restart zygote
    writepid /dev/cpuset/system-background/tasks

service drm /system/bin/drmserver
    class main
    user drm
    group drm system inet drmrpc

service media /system/bin/mediaserver
    class main
    user media
    group audio camera inet net_bt net_bt_admin net_bw_acct drmrpc mediadrm
    ioprio rt 4

# One shot invocation to deal with encrypted volume.
service defaultcrypto /system/bin/vdc --wait cryptfs mountdefaultencrypted
    disabled
    oneshot
    # vold will set vold.decrypt to trigger_restart_framework (default
    # encryption) or trigger_restart_min_framework (other encryption)

# One shot invocation to encrypt unencrypted volumes
service encrypt /system/bin/vdc --wait cryptfs enablecrypto inplace default noui
    disabled
    oneshot
    # vold will set vold.decrypt to trigger_restart_framework (default
    # encryption)

service bootanim /system/bin/bootanimation
    class core
    user graphics
    group graphics audio
    disabled
    oneshot

service gatekeeperd /system/bin/gatekeeperd /data/misc/gatekeeper
    class late_start
    user system

service installd /system/bin/installd
    class main
    socket installd stream 600 system system

service flash_recovery /system/bin/install-recovery.sh
    class main
    oneshot

service racoon /system/bin/racoon
    class main
    socket racoon stream 600 system system
    # IKE uses UDP port 500. Racoon will setuid to V** after binding the port.
    group V** net_admin inet
    disabled
    oneshot

service mtpd /system/bin/mtpd
    class main
    socket mtpd stream 600 system system
    user V**
    group V** net_admin inet net_raw
    disabled
    oneshot

service keystore /system/bin/keystore /data/misc/keystore
    class main
    user keystore
    group keystore drmrpc

service dumpstate /system/bin/dumpstate -s
    class main
    socket dumpstate stream 0660 shell log
    disabled
    oneshot

service mdnsd /system/bin/mdnsd
    class main
    user mdnsr
    group inet net_raw
    socket mdnsd stream 0660 mdnsr inet
    disabled
    oneshot

service uncrypt /system/bin/uncrypt
    class main
    disabled
    oneshot

service pre-recovery /system/bin/uncrypt --reboot
    class main
    disabled
    oneshot

service perfprofd /system/xbin/perfprofd
    class late_start
    user root
    oneshot
    writepid /dev/cpuset/system-background/tasks

on property:persist.logd.logpersistd=logcatd
    # all exec/services are called with umask(077), so no gain beyond 0700
    mkdir /data/misc/logd 0700 logd log
    # logd for write to /data/misc/logd, log group for read from pstore (-L)
    exec - logd log -- /system/bin/logcat -L -b all -v threadtime -v usec -v printable -D -f /data/misc/logd/logcat -r 64 -n 256
    start logcatd

service logcatd /system/bin/logcat -b all -v threadtime -v usec -v printable -D -f /data/misc/logd/logcat -r 64 -n 256
    class late_start
    disabled
    # logd for write to /data/misc/logd, log group for read from log daemon
    user logd
    group log
    writepid /dev/cpuset/system-background/tasks
```

下面我就把init.rc粘贴过来，然后加上一定的注释让大家很好的理解 按照上面的"块"(section)来举例说明，我们就用最前面的

##### 1、on early-init 块

```
on early-init

    # 调用 do_write函数 ，将oom_score_adj写为-1000  
    # Set init and its forked children's oom_adj.
    write /proc/1/oom_score_adj -1000
 
    # 为adb_keys重置安全上下文
    # Set the security context of /adb_keys if present.
    restorecon /adb_keys

     #调用函数do_start 启动服务do_start
    start ueventd
```

上面代码的 trigger为**early-init**，在[init.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit.cpp&objectId=1199504&objectType=1&isNewArticle=undefined)的main函数(1082行)设置过。

下面对所有的命令进行下讲解

-   bootchart\_init：初始化bootchart，用于获取开机过程的系统信息
-   chmod <octal-mode> <path>：改变文件的权限
-   chown <owner> <group> <path>：改变文件的群组
-   class\_start <serviceclass>：启动所有具有特定class的services
-   class\_stop <serviceclass>：将具有特定的class所有运行中的services给停止或者disable
-   class\_reset <serviceclass>：先将services stop掉，然后再重新启动
-   copy <src> <dst>：复制文件
-   domainname <name>：设置[域名](https://dnspod.cloud.tencent.com/?from_column=20065&from=20065)
-   enable <servicename>：如果service没有disable，就将他设为enable
-   exec \[ <seclabel> \[ <user> <group> \* \] \] -- <command> <argument> \* ：创建执行程序，比较重要，后面启动service要用到
-   export <name> <value>：在全局设置环境变量
-   hostname <name>：设置主机名称
-   ifup <interface>：启动网络接口
-   insmod <path>：在某个路径安装一个模块
-   load\_all\_props：加载所有的配置
-   load\_persist\_props：当data加密时加载一些配置
-   loglevel <level>：设置kernel log level
-   mkdir <path> mode group：创建文件夹
-   mount\_all <fstab> <path> ：挂载
-   mout <type> <device> <dir> <flag> \* <options> ：在dir文件夹下面挂载设备
-   restart <service>：重启服务
-   restorecon <path> <path> ：重置文件的安全上下文
-   restorecon\_recursive <path> <path> \*：一般都是selinux完成初始化之后又创建、或者改变的目录
-   rm <path>：删除文件
-   rmdir <path>：删除文件夹
-   setprop <name> <value>：设置属性
-   setrlimit <resource> <cur> <max>：设置进程资源限制
-   start <service>：如果service没有启动，就将他启动起来
-   stop <service>：将运行的服务停掉
-   swapon\_all <fstab>：在指定文件上调用 fs\_mgr\_swapon\_all
-   symlink <target> <path>：创建一个指向 <path>的符号链接
-   sysclktz <mins\_west\_of\_gmt>：指定系统时钟基准，比如0代表GMT，即以格林尼治时间为准
-   trigger <event>：触发一个事件
-   verity\_load\_state：用于加载dm-verity状态的内部实现
-   verity\_update\_state <mount\_point>：用于更新dm-verity状态并设置分区的内部实现细节，由于fs\_mgsr不能直接设置它们本身，所以通过使用adb remount来验证属性
-   wait <path> <timeout> ：等待指定路径的文件创建出来，创建完成就停止等待，或者等到超时时间到。如果未指定超时时间，缺省是5秒。
-   write <path> <content> <string> \*：打开指定的文件，并写入一个或多个字符串。

### 四、init.rc文件的解析

上面的一片文章介绍了，在init.cpp里面会调用**init\_parse\_config\_file("/init.rc");**函数来解析**init.rc**，而**init\_parse\_config\_file("/init.rc");**函数其实是**init\_parser.cpp**里面的，那我们就从这里开始。

##### (一)、init\_parse\_config\_file函数解析

代码在[init\_parser.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit_parser.cpp&objectId=1199504&objectType=1&isNewArticle=undefined) 441行

```
int init_parse_config_file(const char* path) {
    INFO("Parsing %s...\n", path);
    Timer t;
    std::string data;
    if (!read_file(path, &data)) {
        return -1;
    }

    data.push_back('\n'); // TODO: fix parse_config.

    // 实际解析init.rc文件的代码
    parse_config(path, data);
    dump_parser_state();

    NOTICE("(Parsing %s took %.2fs.)\n", path, t.duration());
    return 0;
}
```

init\_parse\_config\_file()通过read\_file()函数把整个配置文件读入内存后，调用parse\_config()函数来解析配置文件。那我们先来看下read\_file()函数的解析。

###### 1、read\_file函数解析

代码在[util.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Futil.cpp&objectId=1199504&objectType=1&isNewArticle=undefined)152行

```
bool read_file(const char* path, std::string* content) {
    content->clear();

    //打开path的文件夹，即打开了init.rc
    int fd = TEMP_FAILURE_RETRY(open(path, O_RDONLY|O_NOFOLLOW|O_CLOEXEC));

    // 如果打开失败，则返回
    if (fd == -1) {
        return false;
    }

   // For security reasons, disallow world-writable
    // or group-writable files.
    struct stat sb;

    // 得到fd文件描述符所指向文件的文件信息，并填充sb的状态结构体。如果获取失败的话，会返回-1
    if (fstat(fd, &sb) == -1) {
        ERROR("fstat failed for '%s': %s\n", path, strerror(errno));
        return false;
    }

    // 如果其他用户，或者用户组有可以写入这个文件的权限的话，则error
    if ((sb.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
        ERROR("skipping insecure file '%s'\n", path);
        return false;
    }

    // 从文件fd中，读取其内容，然后保存在content里面，
    bool okay = android::base::ReadFdToString(fd, content); 

    // 读取完了文件，则要关闭这个文件的描述符
    close(fd);
    return okay;
}
```

通过上面代码的分析，我们知道，readfile函数里面将init.rc内部全部读取到content里面，然后进行返回。下面我们来看下parse\_config函数的解析

###### 2、parse\_config函数解析

代码在[init\_parser.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit_parser.cpp&objectId=1199504&objectType=1&isNewArticle=undefined)385行

```
// 针对init.rc来说，fn指向的内容为init.rc这个文件，data就是init.rc里面的内容映射到内存的数据
static void parse_config(const char *fn, const std::string& data)
{

    struct listnode import_list;
    struct listnode *node;
    char *args[INIT_PARSER_MAXARGS];

    int nargs = 0;
    parse_state state;
 
    // 表明解析的是init.rc的文件
    state.filename = fn;

    // 将初始化的line设为0
    state.line = 0;

    // ptr指向s的第一个元素
    state.ptr = strdup(data.c_str());  // TODO: fix this code!

    // 设置nexttoken为0
    state.nexttoken = 0;

    // 设置解析的方式为parse_line_no_op，即为不需要处理。需要注意的为parse_line是一个函数指针。
    state.parse_line = parse_line_no_op;

   // 初始化前面的import的链表
    list_init(&import_list);

    // 将priv指向了初始化后的import的链表
    state.priv = &import_list;

   // 开始解析文件
    for (;;) {
        // next _token的函数原理是，针对state->ptr的指针进行解析，依次向后读取data数组中的内容
       // 如果读取到"\n"，"0"的话，返回T_EOF和T_NEWLINE
       // 如果读取出来的是一个词的话，则将内容保存在args的数组中，内容依次向后
        switch (next_token(&state)) {
 
        // 如果是文件读取结束
        case T_EOF:

            // 如果文件是空的，那么执行的function是parse_line_no_op
             // 如果不是空的，则执行parse_line_action或者service
             // 如果nargs是0的话，都会返回掉
            state.parse_line(&state, 0, 0);
            goto parser_done;

        case T_NEWLINE:

            // 如果遇到"\n"的话，state.line会+1行
            state.line++;

            // 如果nargs有值的话，说明这一行需要解析了
            if (nargs) {
                //获取这一行的第一个关键字，即args[0]，获取kw
                int kw = lookup_keyword(args[0]);

                // 如果这个kw是一个SECTION的话，怎会返回true，如果不是，则反之
                if (kw_is(kw, SECTION)) {

                    // 清楚掉现在的parse_line，开启一个新的
                    state.parse_line(&state, 0, 0);
                    parse_new_section(&state, kw, nargs, args);
                } else {

                    // 如果不是一个section的话，则将nargs与args作为参数传递到parse_line对应的函数中区
                    state.parse_line(&state, nargs, args);
                }
                // 在执行完一行之后，由于有新的内容需要读取到args中，所以将nargs设置为0
                nargs = 0;
            }
            break;
        case T_TEXT:
            if (nargs < INIT_PARSER_MAXARGS) {
 
                // 每取出来一个token，就会将其放入到args的数组中，且nargs会自动+1
                args[nargs++] = state.text;
            }
            break;
        }
    }


// 文件结束的时候，会去执行parse_done
parser_done:

    // 这里会去遍历所有import_list的节点
    list_for_each(node, &import_list) {
 
         // 取出这些import的节点
         struct import *import = node_to_item(node, struct import, list);
         int ret;

         // 继续对这些文件进行解析
         ret = init_parse_config_file(import->filename);
         if (ret)
             ERROR("could not import file '%s' from '%s'\n",
                   import->filename, fn);
    }
}
```

parse\_config()函数解析脚本文件的逻辑过程可以用一张流程图来表示，如下图所示。通过调用next\_token()函数的作用就是寻找单词结束标志或行结束标志。如果是单词结束符，就先存放在数组args中，如果找到的是行结束符，则根据行中的第一个单词来判断是否是一个"section"，"section"的标志有3个，关键字"on","service","import"。如果是"section"则调用函数**parse\_new\_section**来开启一个新"section"的处理，否则把这一行继续作为当前"section"所属的行来处理。



![](https://ask.qcloudimg.com/http-save/yehe-2957818/n6iw6pex3r.png)



流程图.png

##### (二)、init.rc具体解析

上面的代码看到了，主要解析是的函数是parse\_new\_section函数，那我们就来看下这个函数 代码在[init\_parser.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit_parser.cpp&objectId=1199504&objectType=1&isNewArticle=undefined)358行

```
static void parse_new_section(struct parse_state *state, int kw,
                       int nargs, char **args)
{
    printf("[ %s %s ]\n", args[0],
           nargs > 1 ? args[1] : "");
    switch(kw) {

    // 如果是 service
    case K_service:
        state->context = parse_service(state, nargs, args);
        if (state->context) {
            state->parse_line = parse_line_service;
            return;
        }
        break;
   // 如果是 on
    case K_on:
        state->context = parse_action(state, nargs, args);
        if (state->context) {
            state->parse_line = parse_line_action;
            return;
        }
        break;

    // 如果是 import
    case K_import:
        parse_import(state, nargs, args);
        break;
    }
    state->parse_line = parse_line_no_op;
}
```

parse\_new\_section()函数，这个函数根据3个关键字来分别处理。即**"on"**，**"service"**，**"import"**。分别对应**parse\_action**函数、**parse\_service**函数和**parse\_import**函数。下面我们就依次来看下这个三个函数的具体实现

###### 1、解析action

我们看到上面代码在**case K\_on**里面首先是调用parse\_action函数。那我们就来先看下parse\_action函数

代码在[init\_parser.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit_parser.cpp&objectId=1199504&objectType=1&isNewArticle=undefined)946行

```
static void *parse_action(struct parse_state *state, int nargs, char **args)
{
    struct trigger *cur_trigger;
    int i;

    // 检查是否存在 trgger
    if (nargs < 2) {
        parse_error(state, "actions must have a trigger\n");
        return 0;
    }
 
    // 初始化 结构体 action
    action* act = (action*) calloc(1, sizeof(*act));
    list_init(&act->triggers);

    for (i = 1; i < nargs; i++) {
        if (!(i % 2)) {
            if (strcmp(args[i], "&&")) {
                struct listnode *node;
                struct listnode *node2;
                parse_error(state, "& is the only symbol allowed to concatenate actions\n");
                list_for_each_safe(node, node2, &act->triggers) {
                    struct trigger *trigger = node_to_item(node, struct trigger, nlist);
                    free(trigger);
                }
                free(act);
                return 0;
            } else
                continue;
        }
        cur_trigger = (trigger*) calloc(1, sizeof(*cur_trigger));
        cur_trigger->name = args[i];
        list_add_tail(&act->triggers, &cur_trigger->nlist);
    }

    // 初始化action的commands这条结构体的内部链表
    list_init(&act->commands);

    //  初始化qlist这条结构体内部的链表
    list_init(&act->qlist);

    //将当前的这个结构体加入到以action_list为哨兵节点的链表中
    list_add_tail(&action_list, &act->alist);
        /* XXX add to hash */
    return act;
}
```

这里初始化的内容全部都是链表的操作。为了更好的理解，我们来看下action的结构体。

###### 1.1、action结构体

代码在[init.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit.h&objectId=1199504&objectType=1&isNewArticle=undefined)47行

```
struct action {

        /* node in list of all actions */
    // 使用这个listnode将其加入"action_list"
    struct listnode alist;

    // 挂接全局执行列表 "action_queue"节点
        /* node in the queue of pending actions */
    struct listnode qlist;

        /* node in list of actions for a trigger */
    struct listnode tlist;

    unsigned hash;

    // action的trigger字符串
        /* list of actions which triggers the commands*/
    struct listnode triggers;

    // action的命令列表的表头
    struct listnode commands;
    struct command *current;
};
```

好的，总结一下，经过parse\_action之后，当前的action会被加入到action\_list为节点的链表中。并且初始化了commands以及qlist这两条结构内部的链表。在parse\_new\_section中，我们看到，在初始化完之后，会将当前state的parse\_line设置为parse\_line\_action。

###### 1.2、listnode结构体

这里简单的说一下listnode这个数据结构 代码在[list.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finclude%25252Fcutils%25252Flist.h&objectId=1199504&objectType=1&isNewArticle=undefined)

```
struct listnode
{
    struct listnode *next;
    struct listnode *prev;
};
```

listnode的定义和我们一般理解的队列节点有点不同，一般的节点都有指向节点的数据的指针，但是listnode的定义居然只有两个用于队列连接的指针，如果把这样一个节点插入了队列，将来又如何从队列中找到对应的的action呢？其实只要明白了脚本的解析中所使用的工作原理，就容易理解了。

在init\_parser.cpp中定义了3个全局列表service\_list、action\_list和action\_queue。 代码[init\_parser.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit_parser.cpp&objectId=1199504&objectType=1&isNewArticle=undefined) 38行

```
static list_declare(service_list);
static list_declare(action_list);
static list_declare(action_queue);
```

其中service\_list列表包括了启动脚本所有"service"，action\_list列表包括了启动脚本中所有"action"，init脚本的解析结果就是生成这两个列表。action\_queue列表则保存正在执行中的"action"，init的main()函数会把需要执行的action插入到action\_queue中。

list\_declare是一个宏，定义并初始化了列表的头节点 代码在[list.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finclude%25252Fcutils%25252Flist.h&objectId=1199504&objectType=1&isNewArticle=undefined) 35行

```
#define list_declare(name) \
    struct listnode name = { \
        .next = &name, \
        .prev = &name, \
    }
```

初始化后，头节点的next指针和prev指针都指向自身。这说明通过listnode组成的队列是一个双向链表。service\_list、action\_list和action\_queue都是头节点。

列表的插入，是通过list\_add\_tail函数来完成的，代码如下： 代码在[list.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finclude%25252Fcutils%25252Flist.h&objectId=1199504&objectType=1&isNewArticle=undefined) 58行

```
static inline void list_add_tail(struct listnode *head, struct listnode *item)
{
    item->next = head;
    item->prev = head->prev;
    head->prev->next = item;
    head->prev = item;
}
```

结构如下：



![](https://ask.qcloudimg.com/http-save/yehe-2957818/req5xhdrnf.png)



action\_list结构.png

然后在接下来的action的comand的时候，就回去去执行parse\_line\_action()的函数

代码在[init\_parser.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit_parser.cpp&objectId=1199504&objectType=1&isNewArticle=undefined)

```
                if (kw_is(kw, SECTION)) {
                    state.parse_line(&state, 0, 0);
                    parse_new_section(&state, kw, nargs, args);
                } else {
                     //实质是调用parse_line_action函数 
                    state.parse_line(&state, nargs, args);
                }
```

那我们来看下parse\_line\_action函数的实现。

###### 1.3、parse\_line\_action()函数

解析完action关键字之后，接下来就是对每个命令行进行解析。命令行解析函数是parse\_line\_action() 代码在[init\_parser.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit_parser.cpp&objectId=1199504&objectType=1&isNewArticle=undefined) 985行

```
static void parse_line_action(struct parse_state* state, int nargs, char **args)
{

    // 通过 state ->context 得到了刚才正在解析的action
    struct action *act = (action*) state->context;
    int kw, n;

    // 如果判断为空，则没有要执行的command的话，就会直接返回
    if (nargs == 0) {
        return;
    }

    // 得到ke，原理和得到SECTION的一致
    kw = lookup_keyword(args[0]);
    if (!kw_is(kw, COMMAND)) {

        // 如果这个命令 不是一个command的话，则返回error
        parse_error(state, "invalid command '%s'\n", args[0]);
        return;
    }

    // 从keywords里面得到这个command需要几个参数，是在初始化数组的第三项目 nargs
    n = kw_nargs(kw);
    if (nargs < n) {

         // 如果需要的参数没有满足的话，则会返回错误
        parse_error(state, "%s requires %d %s\n", args[0], n - 1,
            n > 2 ? "arguments" : "argument");
        return;
    }

    // 对action的结构体中的cmmand结构体进行初始化
    command* cmd = (command*) malloc(sizeof(*cmd) + sizeof(char*) * nargs);
 
     //得到这个command需要执行的函数，并将其放在func的这个指针里面
    cmd->func = kw_func(kw);

    // 得到这个command是在文件中的哪一样
    cmd->line = state->line;

     // 是哪个文件的commands
    cmd->filename = state->filename;

    //  这个commands的参数有几个
    cmd->nargs = nargs;

    // 将这几个参数都copy到commands的数组里面
    memcpy(cmd->args, args, sizeof(char*) * nargs);

    // 将当前要执行的commands，加入到action的结构体中，listnode为commands的链表中
    list_add_tail(&act->commands, &cmd->clist);
}
```

###### 2、解析service

上面分析解析init.rc的action之后，剩下的一部分就是解析service了。按照我们前面解析action的流程。我们还是要回到**parse\_config()函数**里面来。根据前面的知识，我们知道，在关键字为**“service”**的时候，会进入到**parse\_new\_section**函数，然后将service以及后面的option设置为执行**"parse\_line"**，又会执行**"parse\_line：parse\_line\_service"**。为了让大家更好的理解，我们先来service的结构体。

###### 2.1、service结构体

代码在[init.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit.h&objectId=1199504&objectType=1&isNewArticle=undefined) 95行

```
struct service {
    void NotifyStateChange(const char* new_state);

   // listnode的节点
        /* list of all services */
    struct listnode slist;

    // 服务的名字
    char *name;

    // 服务的类名
    const char *classname;

    unsigned flags;

    // service 所在进程的pid
    pid_t pid;

   
    time_t time_started;    /* time of last start */
    time_t time_crashed;    /* first crash within inspection window */
    int nr_crashed;         /* number of times crashed within window */

    uid_t uid;
    gid_t gid;
    gid_t supp_gids[NR_SVC_SUPP_GIDS];
    size_t nr_supp_gids;

    const char* seclabel;

    // 为service 创建的Sockets
    struct socketinfo *sockets;

    // 为service设置的环境变量
    struct svcenvinfo *envvars;

    struct action onrestart;  /* Actions to execute on restart. */

    std::vector<std::string>* writepid_files_;

    /* keycodes for triggering this service via /dev/keychord */
    int *keycodes;
    int nkeycodes;
    int keychord_id;

    IoSchedClass ioprio_class;
    int ioprio_pri;

    int nargs;
    /* "MUST BE AT THE END OF THE STRUCT" */
    char *args[1];
}; /*     ^-------'args' MUST be at the end of this struct! */
```

这个结构体相比较而言就比较简单了，除了service的本身属性之外，对于数据结构方面就只有一个listnode了。 关于listnode上面已经讲解，我这里就不讲解了。

下面我们来看下parse\_service函数的解析

###### 2.2、parse\_service()函数

代码在[init\_parser.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit_parser.cpp&objectId=1199504&objectType=1&isNewArticle=undefined) 728行

```
static void *parse_service(struct parse_state *state, int nargs, char **args)
{
    // 如果service的参数小于3个的话，我们会认为service是一个不正常的service。
     // service最少的nargs也是3，分别是service 关键字、service的名字、service启动的时候要执行的命令
    if (nargs < 3) {
        parse_error(state, "services must have a name and a program\n");
        return 0;
    }

    // 如果service的name为不标准的名字的话，含有其他的符号的话，我们认为这个service是不规范的service。
    if (!valid_name(args[1])) {
        parse_error(state, "invalid service name '%s'\n", args[1]);
        return 0;
    }
   
    // 会从已经存在的service_list里面去查找，是否已经有同名的service的存在
    service* svc = (service*) service_find_by_name(args[1]);
    if (svc) {

        // 如果发现有同名的service存在的话，则会返回errror
        parse_error(state, "ignored duplicate definition of service '%s'\n", args[1]);
        return 0;
    }

    // 去除service关键字 与 service的name
    nargs -= 2;
    svc = (service*) calloc(1, sizeof(*svc) + sizeof(char*) * nargs);
    if (!svc) {

        // 如果分配失败，就提示out of memory
        parse_error(state, "out of memory\n");
        return 0;
    }

    // 设置service的name为service关键字 后的第一个参数
    svc->name = strdup(args[1]);

    // 默认的classname为default
    svc->classname = "default";

    // 将args剩余的参数复制到svc的args里面
    memcpy(svc->args, args + 2, sizeof(char*) * nargs);


    trigger* cur_trigger = (trigger*) calloc(1, sizeof(*cur_trigger));

    // 给 args的最后一项设置为0
    svc->args[nargs] = 0;

    // 参数的数目等于传进来的参数的数目
    svc->nargs = nargs;

    list_init(&svc->onrestart.triggers);

    // 设置 onrestart.name为onrestart
    cur_trigger->name = "onrestart";


    list_add_tail(&svc->onrestart.triggers, &cur_trigger->nlist);

    // 初始化onrestart的链表
    list_init(&svc->onrestart.commands);

    // 将当前的service的结构体加入到service_list的链表里面
    list_add_tail(&service_list, &svc->slist);

    return svc;
}
```

从上面我们知道，在执行完parse\_service之后，会初始化service的一些属性，将servicename作为args进行保存。然后将这个解析出来的service加入到service\_list的链表里面。

具体解析service中的每一行的函数是**parse\_line\_service**函数，代码在[init\_parser.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit_parser.cpp&objectId=1199504&objectType=1&isNewArticle=undefined) 765行，因为代码有点多，粘贴过来，就满了，我就简单说下其内容，其函数内部，就是解析service的每一行，将其对应进不同的case里面，进行service结构体的填充。在service的解析后，会生成一条链表保存service的结构体，然后service的结构体里面自己运行维护一个action。

###### 3、解析import

import的解析，只是把文件名插入了import\_list列表中。对import列表的处理是在parse\_config()函数里面的结尾部分

代码在[init\_parser.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit_parser.cpp&objectId=1199504&objectType=1&isNewArticle=undefined) 431行

```
         struct import *import = node_to_item(node, struct import, list);
         int ret;
```

这段代码的作用是取出import\_list每个节点的文件夹名，然后递归调用init\_parse\_config\_file()函数。init\_parse\_config\_file()函数就是解析配置文件的入口函数，前面已经讲解了，这里就不说了。

> PS：无论import语句在脚本文件的哪一行，都是在所属文件解析完之后才开始解析它引入的文件。

###### 4、执行

上面讲了很多，主要就是解析，我们知道解析过程，只不过是把脚本文件中的"action"和"service"解析出来放到各自的队列中。action的执行是在init代码中指定的。通过前面的内容，我们知道， init进程的main()函数中会通过调用action\_for\_each\_trigger()函数来把需要执行的"action"加入到执行列表的**action\_queue**中。

### 五、init.rc命令执行的顺序

我们知道解析init.rc会把一条条命令映射到内存中，然后依次启动。那启动顺序是什么？ 即是按照init.rc里面的顺序大致顺序如下：

-   on early-init
-   on init
-   on late-init //挂载文件系统，启动核心服务 trigger post-fs trigger load\_system\_props\_action trigger post-fs-data //挂载data trigger load\_persist\_props\_action trigger firmware\_mounts\_complete trigger boot
-   on post-fs start logd mount rootfs rootfs / ro remount mount rootfs rootfs / shared rec mount none /mnt/runtime/default /storage slave bind rec ...
-   on post-fs-data start logd start vold //启动vold ...
-   on boot ... class\_start core //启动core class

即如下顺序 首先是 **on early-init** -> **init** -> **late -init** -> **boot**

### 六、init总结

这里里面总结下init里面main方法做的事情如下：

-   first stage 初始化环境变量和各种文件系统目录，klog初始化等
-   selinux相关初始化完成，然后切换second stage 重启init进程
-   属性服务初始化，将各种系统属性默认值填充到属性Map中
-   创建epoll描述符结合注册socket监听，处理显示启动进程和挂掉的子进程重启
-   解析init.rc。把各种action、service等解析出来的填充到相应链表[容器](https://cloud.tencent.com/product/tke?from_column=20065&from=20065)管理
-   有序将early-init、init等各种cmd加入到执行队列action\_queue链表中
-   进入while()循环依次取出执行队列action\_queue中的command执行，fork包括app\_process在内的各种进程，epoll阻塞监听处理来自挂掉的子进程的消息，根据设定策略restart子进程。

流程图如下：



![](https://ask.qcloudimg.com/http-save/yehe-2957818/g5fxvpux3t.png)



image.png

上一篇文章 [Android系统启动——2 init进程](https://cloud.tencent.com/developer/article/1199318?from_column=20421&from=20421) 下一篇文章 [Android系统启动——4 zyogte进程 (C篇)](https://cloud.tencent.com/developer/article/1199508?from_column=20421&from=20421)

### 官人飞吻，你都把臣妾从头看到尾了，喜欢就点个赞呗(眉眼)！！！！



本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2018.03.05 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除