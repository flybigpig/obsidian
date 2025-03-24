> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

## rc文件说明

早期的rc文件的内容都集中在/init.rc中， 大部分的service定义也是集中在这个文件中， 同时有可能会出现service对应的可执行程序没有编译进系统， 导致service执行失败的情形， 现在各种rc文件是分散在不同分区中进行模块化，并且service对应代码和rc文件是一起编译的， 这样保证模块化的完整性。

-   /init.rc：最主要和最早被加载的rc文件， 一般都是做系统最初初始化的。
-   /init.${ro.hardware}.rc和/vendor/etc/init/hw/init.${ro.hardware}.rc：Soc对应的定制的初始化命令和服务。
-   /system/etc/init/：启动系统核心的服务，比如 SurfaceFlinger, MediaService, and logcatd。
-   /vendor/etc/init/：SOC厂商定制开机自启动的serviced对应的rc脚本，比如gnss，camera，sensor，drm等HAL相关服务
-   /odm/etc/init/：外设设备厂商定制自启动的serviced对应的rc脚本， 如运动传感器或其他外围设备所需的命令和服务。

## rc文件内容说明

Init.rc 中的语法都是 Android 自定义的， system/core/init/README.md 有具体语法说明 ，整个 init.rc 其实有以 下几个部分组成：

| 关键字 | 描述 |
| --- | --- |
| Imports | 导入其他 rc 文件 |
| Actions | 命令的集合， 形式为 on trigger, 其中 trigger 有两种， event 和 proptery。 |
| Commands | 隶属于 Action 中的命令 |
| Services | 后台服务， 一般都是 C/C++的守护进程，或者是 shell 脚本 |
| Options | 隶属于 Service 中选项， 比如守护进程的用户 id， 组 id， 类别 , 是否执行一次， 创建额外套接字等。 |
| 其它规则： |  |

-   #号作为注释。
-   一行为单位， 以空格作为分隔符, 行尾可以用反斜杠''表示连接下一行。
-   字符串中可以使用双引号(如“ ”)来表示空格。
-   ${property.name} 可以展开属性。
-   以 import, on, service 开头的， 都表示一个 section(段落)， 意味着一个关键词出现之后到出现第二个关键词 之前， 关键词后面的内容都属于一个 section。
-   Service 的关键词在所有的 rc 文件中都必须是唯一的， 如果出现第二个相同的 service 名字， 将会忽略和显 示日志。

init.rc部分内容示例：

```
import /init.environ.rc
import /init.usb.rc
import /init.${ro.hardware}.rc
import /vendor/etc/init/hw/init.${ro.hardware}.rc
import /init.usb.configfs.rc
import /init.${ro.zygote}.rc

# Cgroups are mounted right before early-init using list from /etc/cgroups.json

on early-init
    # Disable sysrq from keyboard
    write /proc/sys/kernel/sysrq 0
    # Set the security context of /adb_keys if present. 
    restorecon /adb_keys

service ueventd /system/bin/ueventd
    class core
    critical
    seclabel u:r:ueventd:s0
    shutdown critical

on property:ro.debuggable=1
    # Give writes to anyone for the trace folder on debug builds. 
    # The folder is used to store method traces. 
    chmod 0773 /data/misc/trace
    # Give reads to anyone for the window trace folder on debug builds. 
    chmod 0775 /data/misc/wmtrace
    start console
```

### Action

Action 其实就是一组命令的集合， 它有一个 trigger 触发器，当系统中出现与动作的触发器匹配的事件，这 个 Action 就会被加入到执行队列的尾部， 等到这个 Action 执行时， Action 中包含的所有命令将会按照先后顺序执行， 格式：

```
on <trigger> [&& <trigger>]*
    <command>
    <command>
    <command>
```

-   trigger 作为触发器，trigger 有两种， event 和 proptery。
-   command: 类似 shell 命令，但是并不是 shell 命令， 因为这些命令是在 init 进程中单独实现的， 而命令行我们执行的 shell 命令， 是在 toolbox 中实现的， 并且 rc 脚本中的 command 的数量有限。

Action 的示例：

```
on early-init
    # Disable sysrq from keyboard
    write /proc/sys/kernel/sysrq 0
    # Set the security context of /adb_keys if present. 
    restorecon /adb_keys

on property:ro.debuggable=1
    # Give writes to anyone for the trace folder on debug builds. 
    # The folder is used to store method traces. 
    chmod 0773 /data/misc/trace
    # Give reads to anyone for the window trace folder on debug builds. 
    chmod 0775 /data/misc/wmtrace
    start console
```

触发器在哪里触发呢： 一般在 init 的代码中，如：

```
system/core/init/init.cpp

am.QueueEventTrigger("early-init");
am.QueueEventTrigger("late-init");
```

或者在 rc 文件中， 如：

```
on late-init
    trigger early-fs
    trigger fs
on post-fs
    exec - system system -- /system/bin/vdc checkpoint markBootAttempt
```

常见的trigger：

```
on early-init #早期初始化
on boot #系统启动触发
on init #在初始化时触发
on late-init #在初始化晚期阶段触发
on charger #当充电时触发
on property:<key>=<value> #当属性值满足条件时触发
on post-fs #挂载文件系统
on post-fs-data #挂载 data
```

常见的 command： 大部分的命令都在 system/core/init/builtins.cpp 代码中实现：

```
// Builtin-function-map start
const BuiltinFunctionMap::Map& BuiltinFunctionMap::map() const {
    constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
    // clang-format off
    static const Map builtin_functions = {
        {"bootchart",               {1,     1,    {false,  do_bootchart}}},
        {"chmod",                   {2,     2,    {true,   do_chmod}}},
        {"chown",                   {2,     3,    {true,   do_chown}}},
        {"class_reset",             {1,     1,    {false,  do_class_reset}}},
        {"class_reset_post_data",   {1,     1,    {false,  do_class_reset_post_data}}},
        {"class_restart",           {1,     1,    {false,  do_class_restart}}},
        {"class_start",             {1,     1,    {false,  do_class_start}}},
        {"class_start_post_data",   {1,     1,    {false,  do_class_start_post_data}}},
        {"class_stop",              {1,     1,    {false,  do_class_stop}}},
        {"copy",                    {2,     2,    {true,   do_copy}}},
        {"domainname",              {1,     1,    {true,   do_domainname}}},
        {"enable",                  {1,     1,    {false,  do_enable}}},
        {"exec",                    {1,     kMax, {false,  do_exec}}},
        {"exec_background",         {1,     kMax, {false,  do_exec_background}}},
        {"exec_start",              {1,     1,    {false,  do_exec_start}}},
        {"export",                  {2,     2,    {false,  do_export}}},
        {"hostname",                {1,     1,    {true,   do_hostname}}},
        {"ifup",                    {1,     1,    {true,   do_ifup}}},
        {"init_user0",              {0,     0,    {false,  do_init_user0}}},
        {"insmod",                  {1,     kMax, {true,   do_insmod}}},
        {"installkey",              {1,     1,    {false,  do_installkey}}},
        {"interface_restart",       {1,     1,    {false,  do_interface_restart}}},
        {"interface_start",         {1,     1,    {false,  do_interface_start}}},
        {"interface_stop",          {1,     1,    {false,  do_interface_stop}}},
        {"load_persist_props",      {0,     0,    {false,  do_load_persist_props}}},
        {"load_system_props",       {0,     0,    {false,  do_load_system_props}}},
        {"loglevel",                {1,     1,    {false,  do_loglevel}}},
        {"mark_post_data",          {0,     0,    {false,  do_mark_post_data}}},
        {"mkdir",                   {1,     4,    {true,   do_mkdir}}},
        // TODO: Do mount operations in vendor_init.
        // mount_all is currently too complex to run in vendor_init as it queues action triggers,
        // imports rc scripts, etc.  It should be simplified and run in vendor_init context.
        // mount and umount are run in the same context as mount_all for symmetry.
        {"mount_all",               {1,     kMax, {false,  do_mount_all}}},
        {"mount",                   {3,     kMax, {false,  do_mount}}},
        {"parse_apex_configs",      {0,     0,    {false,  do_parse_apex_configs}}},
        {"umount",                  {1,     1,    {false,  do_umount}}},
        {"umount_all",              {1,     1,    {false,  do_umount_all}}},
        {"readahead",               {1,     2,    {true,   do_readahead}}},
        {"restart",                 {1,     1,    {false,  do_restart}}},
        {"restorecon",              {1,     kMax, {true,   do_restorecon}}},
        {"restorecon_recursive",    {1,     kMax, {true,   do_restorecon_recursive}}},
        {"rm",                      {1,     1,    {true,   do_rm}}},
        {"rmdir",                   {1,     1,    {true,   do_rmdir}}},
        {"setprop",                 {2,     2,    {true,   do_setprop}}},
        {"setrlimit",               {3,     3,    {false,  do_setrlimit}}},
        {"start",                   {1,     1,    {false,  do_start}}},
        {"stop",                    {1,     1,    {false,  do_stop}}},
        {"swapon_all",              {1,     1,    {false,  do_swapon_all}}},
        {"enter_default_mount_ns",  {0,     0,    {false,  do_enter_default_mount_ns}}},
        {"symlink",                 {2,     2,    {true,   do_symlink}}},
        {"sysclktz",                {1,     1,    {false,  do_sysclktz}}},
        {"trigger",                 {1,     1,    {false,  do_trigger}}},
        {"verity_load_state",       {0,     0,    {false,  do_verity_load_state}}},
        {"verity_update_state",     {0,     0,    {false,  do_verity_update_state}}},
        {"wait",                    {1,     2,    {true,   do_wait}}},
        {"wait_for_prop",           {2,     2,    {false,  do_wait_for_prop}}},
        {"write",                   {2,     2,    {true,   do_write}}},
    };
    // clang-format on
    return builtin_functions;
}
```

常见命令有以下这些：

```
exec <path> [ <argument> ]*：运行指定路径下的程序，并传递参数
export <name> <value>：设置全局环境参数。此参数被设置后对全部进程都有效
ifup <interface>：使指定的网络接口"上线",相当激活指定的网络接口
import <filename>：导入一个额外的 rc 配置文件
hostname <name>：设置主机名
chdir <directory>：改变工作文件夹
chmod <octal-mode> <path>：设置指定文件的读取权限
chown <owner> <group> <path>：设置文件所有者和文件关联组
chroot <directory>：设置根文件夹
class_start <serviceclass>：启动指定类属的全部服务，假设服务已经启动，则不再反复启动
class_stop <serviceclass>：停止指定类属的全部服务
domainname <name>：设置域名
insmod <path>：安装模块到指定路径
mkdir <path> [mode] [owner] [group]：用指定参数创建一个文件夹
mount <type> <device> <dir> [ <mountoption> ]*：类似于linux的mount指令
setprop <name> <value>：设置属性及相应的值
setrlimit <resource> <cur> <max>：设置资源的rlimit
start <service>：假设指定的服务未启动，则启动它
stop <service>：假设指定的服务当前正在执行。则停止它
symlink <target> <path>：创建一个符号链接
sysclktz <mins_west_of_gmt>：设置系统基准时间
trigger <event>：触发另一个时间
write <path> <string> [ <string> ]*：往指定的文件写字符串
```

### Service

Service 表示一个开机自启动服务， 服务可以常驻型(不死型)， 也可以是只执行一次， 可以选择开机自启动或 者在特定的时候启动，语法：

```
service <name> <pathname> [ <argument> ]*
       <option>
       <option>
       ...
```

-   service的名字，要保证唯一
-   service对应的可执行程序路径
-   启动service所带的参数
-   启动service时约束和附加选项，它影响着服务以什么方式和什么时候启动，

服务的选项主要有：

-   class <name> \[ <name>\* \]：为服务指定 class 名字。 同一个 class 名字的服务会被一起启动或退出, 默认值是 default
-   console \[<console>\]：这个选项表明服务需要一个控制台。 第二个参数 console 的意思是可以设置你想要的控制台类型，默认控制台是 /dev/console, /dev 这个前缀通常是被省略的， 比如你要设置控制台 /dev/tty0, 那么只需要设置为console tty0 即可。
-   critical：表示服务是严格模式。 如果这个服务在4分钟内或者启动完成前退出超过4次，那么设备将重启进入 bootloader 模式
-   disabled：这个服务不会随着 class 一起启动。只能通过服务名来显式启动。比如 foobar 服务的 class 是 core, 且是 disabled 的，当执行 class\_start core 时，foobar 服务是不会被启动的。 foobar 服务只能通过 start foobar 这种方法来启动。
-   file <path> <type> 根据文件路径 path 来打开文件，然后把文件描述符 fd 传递给服务进程。type 表示打工文件的方式，只有三种取值 r, w, rw。对于 native 程序来说，可以通过 libcutils 库提供的 android\_get\_control\_file() 函数来获取传递过来的文件描述符。举个例子， logd.rc 部分内容如下:

```
service logd /system/bin/logd
    socket logd stream 0666 logd logd
    socket logdr seqpacket 0666 logd logd
    socket logdw dgram+passcred 0222 logd logd
    file /proc/kmsg r
    file /dev/kmsg w
    user logd
```

其中通过 file /proc/kmsg r 以只读方式打开了设备文件 /proc/kmsg, 然后在代码中这么获取打开的文件, 见 sytem/core/logd/main.cpp main 函数：

```
static const char dev_kmsg[] = "/dev/kmsg";
fdDmesg = android_get_control_file(dev_kmsg);
if (fdDmesg < 0) {
    fdDmesg = TEMP_FAILURE_RETRY(open(dev_kmsg, O_WRONLY | O_CLOEXEC));
}
```

-   group <groupname> \[ <groupname>\* \]：在启动 Service 前，将 Service 的用户组改为第一个groupname, 第一个 groupname 是必须有的， 第二个 groupname 可以不设置，用于追加组（通setgroups）。目前默认的用户组是 root 组。
-   oneshot：当服务退出的时候，不自动重启。适用于那些开机只运行一次的服务。
-   onrestart：在服务重启的时候执行一个命令
-   seclabel <seclabel>：在启动 Service 前设置指定的 seclabel，默认使用init的安全策略。 主要用于在 rootfs 上启动的 service，比如 ueventd, adbd。 在系统分区上运行的 service 有自己的 SELinux安全策略。
-   setenv <name> <value>：设置进程的环境变量
-   socket <name> <type> <perm> \[ <user> \[ <group> \[ <seclabel> \] \] \] 创建一个 unix domain socket, 路径为 /dev/socket/name , 并将 fd 返回给 Service。 type 只能是 dgram, stream or seqpacket。user 和 group 默认值是 0。 seclabel 是这个 socket 的 SELinux security context, 它的默认值是 service 的 security context 或者基于其可执行文件的 security context。
-   user 在启动 Service 前修改进程的所属用户, 默认启动时 user 为 root

常见的option： 大部分都在system/core/init/service.cpp代码中实现：

```
const Service::OptionParserMap::Map& Service::OptionParserMap::map() const {
    constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
    // clang-format off
    static const Map option_parsers = {
        {"capabilities",
                        {0,     kMax, &Service::ParseCapabilities}},
        {"class",       {1,     kMax, &Service::ParseClass}},
        {"console",     {0,     1,    &Service::ParseConsole}},
        {"critical",    {0,     0,    &Service::ParseCritical}},
        {"disabled",    {0,     0,    &Service::ParseDisabled}},
        {"enter_namespace",
                        {2,     2,    &Service::ParseEnterNamespace}},
        {"file",        {2,     2,    &Service::ParseFile}},
        {"group",       {1,     NR_SVC_SUPP_GIDS + 1, &Service::ParseGroup}},
        {"interface",   {2,     2,    &Service::ParseInterface}},
        {"ioprio",      {2,     2,    &Service::ParseIoprio}},
        {"keycodes",    {1,     kMax, &Service::ParseKeycodes}},
        {"memcg.limit_in_bytes",
                        {1,     1,    &Service::ParseMemcgLimitInBytes}},
        {"memcg.limit_percent",
                        {1,     1,    &Service::ParseMemcgLimitPercent}},
        {"memcg.limit_property",
                        {1,     1,    &Service::ParseMemcgLimitProperty}},
        {"memcg.soft_limit_in_bytes",
                        {1,     1,    &Service::ParseMemcgSoftLimitInBytes}},
        {"memcg.swappiness",
                        {1,     1,    &Service::ParseMemcgSwappiness}},
        {"namespace",   {1,     2,    &Service::ParseNamespace}},
        {"oneshot",     {0,     0,    &Service::ParseOneshot}},
        {"onrestart",   {1,     kMax, &Service::ParseOnrestart}},
        {"oom_score_adjust",
                        {1,     1,    &Service::ParseOomScoreAdjust}},
        {"override",    {0,     0,    &Service::ParseOverride}},
        {"priority",    {1,     1,    &Service::ParsePriority}},
        {"restart_period",
                        {1,     1,    &Service::ParseRestartPeriod}},
        {"rlimit",      {3,     3,    &Service::ParseProcessRlimit}},
        {"seclabel",    {1,     1,    &Service::ParseSeclabel}},
        {"setenv",      {2,     2,    &Service::ParseSetenv}},
        {"shutdown",    {1,     1,    &Service::ParseShutdown}},
        {"sigstop",     {0,     0,    &Service::ParseSigstop}},
        {"socket",      {3,     6,    &Service::ParseSocket}},
        {"timeout_period",
                        {1,     1,    &Service::ParseTimeoutPeriod}},
        {"updatable",   {0,     0,    &Service::ParseUpdatable}},
        {"user",        {1,     1,    &Service::ParseUser}},
        {"writepid",    {1,     kMax, &Service::ParseWritepid}},
    };
    // clang-format on
    return option_parsers;
}
```