> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

## 权限控制介绍

SEAndroid是android中一种权限控制机制，android中的权限控制机制发展经历MAC和DAC。

### DAC 权限控制

SElinux 之前， linux 采用的是 DAC (Discretionary Access Control，自主访问控制系统)基本概念就是 系统只提供基本的验证,完整的访问控制由开发者自己控制(直接 chmod/chmod 就可以修改权限)，DAC 采用了一 种非常简单的策略(policy)，存在资源访问者，资源，资源的访问权限：

-   资源访问者分别：Owner, Group，Other，访问者包含各种用户(uid/gid)，各种进程；
-   资源一般都是文件；
-   资源的访问权限分成 read、 write、 execute；

### MAC 权限控制

在DAC中，每个用户都有uid/gid, 进程理论上所拥有的权限与执行它的用户的权限相同，如果进程是由root 超级用户启动，该进程就会拥有 root 权限, 该进程就可以在系统上干任何事情，其实是比较危险的。所以就出 现了 MAC（Mandatory Access control 强制性访问控制），即要求系统对每一项访问都进行严格的限制和验证， 具体的限制和验证策略(policy)由开发者给出，即需要明确的指明某个进程可以访问哪些资源(文件、网络端口， 属性等)。

MAC 可以明显弥补 DAC 的缺陷。如可限制 Root 权限，即使你有 root 权限(将军)，如果无法通过 MAC 验证(圣 旨授权)，那么一样的无法真正执行相关的操作。另外 MAC 对每一项权限进行了更加完整的细化，可限制用户对 资源的访问行为。在 Linux Kernel，所有的 MAC 机制都是搭建在 Linux Security Modules(LSM)基础上,目前 SELinux 已经成了事实上的行业标准。

![DAC和MAC.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/0d6ed3209292469191c1ad9d9d1703c9~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240357&x-signature=77oDUfVqr7sLqhjbskF6lJA%2BEpE%3D)

注意：Linux系统先做DAC检查。如果没有通过DAC权限检查，则操作直接失败。通过DAC检查之后，再做MAC权限检查。

### SEAndroid 和 SELinux 关系

SELinux(Security Enhanced Linux)就是 linux 系统中对 MAC 的实现。由于 Android 系统有着独特的用户空 间运行时，因此 SELinux 不能完全适用于 Android 系统。为此，NSA(美国国家安全局)同 Google 一起针对 Android 系统，在 SELinux 基础上开发了 SEAndroid(Security Enhancements (SE) for Android), Android 从 5.0 开始 普遍使用 SEAndroid。

## SEAndroid框架

参考罗升阳大神的这张图 ![image.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/c256a19915a947c19d36b19b91272185~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240357&x-signature=abVsO7PcMT%2BmVxnIIS8AIpxD9HE%3D) SEAndroid 框架是由用户空间和内核空间组成， 用户空间安全保护机制中主要包含：

-   安全上下文（Security Context）
-   安全策略（Security Policy）
-   安全服务（Security Server）等模块

内核空间由 LSM，AVC 组成，LSM(linux security module)负责内核资源的安全访问控, 由它来决定一个主体访问一个客体是否是合法的，这个主体一般是指进程，而客体主要指资源， 而 selinux 是属于 LSM 中实现的一种。

用户空间的 Security Server(一种统称)主要根据用户空间提供的安全上下文对资源进行配置，它由4个部分构成：

-   应用程序安装服务 PackageManagerService
-   应用安装守护进程 installd
-   应用程序进程孵化器 Zygote 进程
-   init进程

其中PackageManagerService 和 installd 负责创建 App 数据目录的安全上下文，Zygote 进程负责创建 App 进程的安全上下文，而 init 进程负责控制系统属性的安全访问。

上面这个框架图说明了大体结构，但是整个机制的工作流描述的还不太清晰，因此使用下面这张流程图讲其工作流阐述清楚。

![SEAndroid工作流程.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/a4306d01e8a142fcae61446dc822d9c7~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240357&x-signature=66zHJYSWQLCKq568lVpQFvJp5Xc%3D) 如上图所示，当一个进程想要访问一个文件时（读写操作等），需要解决 3 个问题：

-   谁来做裁判，评判进程是否有权限访问这个文件，这个裁判就是LSM
-   裁判是依据什么来做判断，判断依据就是“安全策略”
-   “安全策略”中如何才能对号入座的描述清楚进程对文件的访问权限呢？就比如大家的身份证号码，只有有了身份标识，做相关检查时才能对号入座；在SEAndroid中这个身份标识就是“安全上下文”。

整个流程大致如下：

-   LSM如何监控操作？

LSM 会在相应的内核对象子系统中会加入一些 Hook 代 码(内核 LSM 驱动代码:security/selinux.c),例如我们调用系统接口 open 函数来读取一个文件的时候，就会进入到内 核的文件子系统中。在文件子系统中负责读取文件函数 vfs\_open 就会调用 LSM 加入的 Hook 代码。这些 Hook 代码就会调用之前 SELinux 注册进来的回调函数（security/selinux/hooks.c），以便后者可以进行安全检查。

-   进程、文件等的安全上下文哪儿来？安全上下在源码中有统一的定义，由用户空间的 Security Server进行统一分配的。
-   安全策略怎么来的？安全策略在源码中有统一的定义，然后在源码编译阶段和init进程在初始化阶段将整个安全策略组织成LSM所需要的格式，写入内核空间，这样一来LSM就有了判断依据

当一个进程访问文件时，首先会进行系统调用调用的内核空间，经过一些检查后，会调用LSM的Hook代码，在Hook代码中会依据“安全策略”进行权限访问判断。

## 安全上下文

可以通过ls -Z 可以查看文件上下文：

```
u:object_r:cgroup:s0           acct            
u:object_r:adb_keys_file:s0    adb_keys        
u:object_r:apex_mnt_dir:s0     apex            
u:object_r:rootfs:s0           bin             
u:object_r:rootfs:s0           bugreports      
u:object_r:cache_file:s0       cache           
u:object_r:rootfs:s0           charger         
u:object_r:configfs:s0         config          
u:object_r:rootfs:s0           d               
u:object_r:system_data_file:s0 data            
u:object_r:tmpfs:s0            debug_ramdisk   
u:object_r:rootfs:s0           default.prop    
u:object_r:device:s0           dev             
u:object_r:rootfs:s0           dsp             
u:object_r:rootfs:s0           etc             
u:object_r:firmware_file:s0    firmware        
u:object_r:init_exec:s0        init            
u:object_r:rootfs:s0           init.environ.rc 
```

通过 ps -Z 查看进程上下文

```
LABEL                          UID            PID  PPID C STIME TTY          TIME CMD
u:r:init:s0                    root             1     0 0 52:25 ?        00:00:00 init second_stage
u:r:qemu_props:s0              root           204     1 0 52:26 ?        00:00:00 qemu-props
u:r:ashmemd:s0                 nobody         211     1 0 52:26 ?        00:00:00 ashmemd
u:r:audioserver:s0             audioserver    212     1 0 52:26 ?        00:00:00 audioserver
u:r:gpuservice:s0              gpu_service    213     1 0 52:26 ?        00:00:00 gpuservice
u:r:lmkd:s0                    lmkd           214     1 0 52:26 ?        00:00:00 lmkd
u:r:kernel:s0                  root           218     2 0 52:26 ?        00:00:00 [psimon]
u:r:surfaceflinger:s0          system         219     1 2 52:26 ?        00:19:48 surfaceflinger
u:r:logpersist:s0              logd           221     1 0 52:26 ?        00:00:00 logcat -Q
u:r:adbd:s0                    shell          224     1 0 52:26 ?        00:00:06 adbd --root_secl+
u:r:traced:s0                  nobody         225     1 0 52:26 ?        00:00:00 traced
u:r:traced_probes:s0           nobody         227     1 0 52:26 ?        00:00:00 traced_probes
u:r:cameraserver:s0            cameraserver   229     1 0 52:26 ?        00:00:00 cameraserver
u:r:drmserver:s0               drm            230     1 0 52:26 ?        00:00:00 drmserver
u:r:idmap:s0                   system         231     1 0 52:26 ?        00:00:00 idmap2d
u:r:incidentd:s0               incidentd      232     1 0 52:26 ?        00:00:00 incidentd
u:r:installd:s0                root           233     1 0 52:26 ?        00:00:00 installd
u:r:keystore:s0                keystore       235     1 0 52:26 ?        00:00:00 keystore /data/m+
u:r:mdnsd:s0                   mdnsr          237     1 0 52:26 ?        00:00:00 mdnsd
u:r:mediadrmserver:s0          media          238     1 0 52:26 ?        00:00:00 mediadrmserver
u:r:mediaextractor:s0          mediaex        239     1 0 52:26 ?        00:00:00 media.extractor +
u:r:mediametrics:s0            media          240     1 0 52:26 ?        00:00:00 media.metrics di+
u:r:mediaserver:s0             media          241     1 0 52:26 ?        00:00:00 mediaserver
u:r:statsd:s0                  statsd         242     1 0 52:26 ?        00:00:15 statsd
u:r:storaged:s0                root           243     1 0 52:26 ?        00:00:01 storaged
u:r:wificond:s0                wifi           244     1 0 52:26 ?        00:00:03 wificond
u:r:mediacodec:s0              mediacodec     245     1 0 52:26 ?        00:00:00 media.codec hw/a+
u:r:rild:s0                    radio          246     1 0 52:26 ?        00:00:01 rild
```

实际由四部分内容组成：SELinux 用户、SELinux 角色、类型、安全级别。

| 格式 | user:role:type:mls |
| --- | --- |
| User 用户 | 代表创建这个文件的 SELinux user, Android 不使用 SELinux 用户。唯一定义的用户是 u， android 源码中有定义：system/sepolicy/private/users |
| Role 角色 | 为角色 role 的意思， 不同角色有不同的权限， Android 不使用 SELinux 角色，定义并使用了两个默认角色：如果是进程(活的), 会定义为 r, 如果是文件(死的)，会被定义为 object\_r。Android 源码中有定义：system/sepolicy/private/roles\_decl |
| Type 类型 | 如果是进程的话， 这一栏就是 domain（域），表示进程属于某某 Domain 并具有某某权限， 在策略文件中，domain 可以通过 allow 语句来赋予不同的权限， 当某个进程属于这个域，也就有了这个域的权限， 比如 zygote 域拥有对 init 进程发送 SIGCHLD信号等权限， 而属于 zygote 的所有进程也就有了相同权限， 这栏如果是文件的话，那这一栏就是 type(文件类型,如 device 类型，camera 类型等)，文件有了类型， 这样进程才能对知道对哪些类型文件拥有什么访问权限， 对进程来说，Type 就是 Domain |
| Mls 多级别安全 | MLS 将系统的进程和文件进行了分级，不同级别的资源需要对应级别的进程才能访问，主要是为了满足军用和教育行业而设计的， Android 不使用 SELinux 敏感度,只有 s0 敏感度。 |

其中“Type 类型”最为重要，在 Android 源码中会存在几个重要的上下文文件， 用来描述系统和属性，服务等的上下文信息， 路径在system/sepolicy

| 文件名 | 描述 |
| --- | --- |
| file\_contexts | 根系统中所有文件的安全上下文， 如/system/bin， /system/etc 等文件，源码路径如： system/sepolicy/private/file\_contexts |
| property\_contexts | 属性的安全上下文，源码路径如:system/sepolicy/private/property\_contexts |
| genfs\_contexts | Generalized filesystem 虚拟文件系统安全上下文，如 proc, sysfs,debugfs 文件系统，源码路径如： system/sepolicy/private/genfs\_contexts |
| service\_contexts | service 文件安全上下文，用于为 Android Binder 服务分配标签，以便控制哪些进程可以为相应服务添加（注册）和查找（查询）Binder 引用。在启动期间，servicemanager进程会读取此配置，HIDL 机制出现后，就有两个，源码路径：system/sepolicy/private/hwservice\_contextssystem/sepolicy/private/service\_contexts |
| mac\_permission.xml | 用于根据应用签名和应用软件包名称（后者可选）为应用分配 seinfo 标记。随后，分配的 seinfo 标记可在 seapp\_contexts 文件中用作密钥，以便为带有该 seinfo 标记的所有应用分配特定标签。在启动期间，system\_server 会读取此配置， 源码路径为： system/sepolicy/private/mac\_permissions.xml |
| seapp\_contexts | app 安全上下文，用于描述 apk 安装之后的目录文件和 /data/data 目录分配标签。在每次应用启动时，zygote 进程都会读取此配置；在启动期间，installd 会读取此配置。源码路径如： system/sepolicy/private/seapp\_contexts |

有了上述文件，系统中的文件、属性、进程等就有了对应的安全上下文。

如下为file\_contexts中部分内容示例，可以看到其对android根文件系统中每一个文件和目录都定义了对应的安全上下文

```
###########################################
# Root
/                   u:object_r:rootfs:s0

# Data files
/adb_keys           u:object_r:adb_keys_file:s0
/build.prop        u:object_r:rootfs:s0
/default.prop      u:object_r:rootfs:s0
/fstab..*          u:object_r:rootfs:s0
/init..*           u:object_r:rootfs:s0
/res(/.*)?          u:object_r:rootfs:s0
/selinux_version    u:object_r:rootfs:s0
/ueventd..*        u:object_r:rootfs:s0
/verity_key         u:object_r:rootfs:s0

# Executables
/init               u:object_r:init_exec:s0
/sbin(/.*)?         u:object_r:rootfs:s0

# For kernel modules
/lib(/.*)?          u:object_r:rootfs:s0

# Empty directories
/lost+found        u:object_r:rootfs:s0
/acct               u:object_r:cgroup:s0
/config             u:object_r:rootfs:s0
/debug_ramdisk      u:object_r:tmpfs:s0
/mnt                u:object_r:tmpfs:s0
/postinstall        u:object_r:postinstall_mnt_dir:s0
/postinstall/apex   u:object_r:postinstall_apex_mnt_dir:s0
/proc               u:object_r:rootfs:s0
/sys                u:object_r:sysfs:s0
/apex               u:object_r:apex_mnt_dir:s0
```

如下为service\_contexts中部分内容示例，其对系统中binder服务定义了对应的安全上下文

```
accessibility                             u:object_r:accessibility_service:s0
account                                   u:object_r:account_service:s0
activity                                  u:object_r:activity_service:s0
activity_task                             u:object_r:activity_task_service:s0
adb                                       u:object_r:adb_service:s0
alarm                                     u:object_r:alarm_service:s0
android.os.UpdateEngineService            u:object_r:update_engine_service:s0
android.security.keystore                 u:object_r:keystore_service:s0
android.service.gatekeeper.IGateKeeperService    u:object_r:gatekeeper_service:s0
app_binding                               u:object_r:app_binding_service:s0
app_prediction                            u:object_r:app_prediction_service:s0
apexservice                               u:object_r:apex_service:s0
ashmem_device_service                     u:object_r:ashmem_device_service:s0
gsiservice                                u:object_r:gsi_service:s0
appops                                    u:object_r:appops_service:s0
appwidget                                 u:object_r:appwidget_service:s0
assetatlas                                u:object_r:assetatlas_service:s0
attention                                 u:object_r:attention_service:s0
audio                                     u:object_r:audio_service:s0
```

在file\_contexts、service\_contexts等文件中定义的安全上下文的“Type 类型”字段，例如：rootfs、sysfs、tmpfs、accessibility\_service、activity\_service等也是有统一定义的地方的，它们在system/sepolicy下面的.te文件中（后面会具体讲解）。

## 安全策略

为了让大家快速理解， 来看一个 SEAndroid 中设置安全策略(设置权限)的例子：

```
/*system/sepolicy/public/vold.te */
allow vold tmpfs:dir create_dir_perms
```

解释： 允许 vold 进程在 tmpfs 文件系统(如/mnt 目录下)中创建文件夹。如果没有在 vold.te 中使用上例中的权限配置 allow 语句，则 vold 进程就无法往 tmpfs 文件系统中创建目录，即使 vold 具有 root 权限。

完整的 allow 相关的语句格式为：

```
rule_name source_type target_type :object_class perm_set
```

-   rule\_name：规则名，分别有 allow，dontaudit，auditallow，neverallow 等
-   source\_type：源类型，主要作用是用来填写一个域(domain),一般都是一个进程, 一般也叫做 scotonext
-   target\_type：目标的类型，一般都是客体的上下文标签,当然进程也是可以做为客体，比如一个进程给另外一个进 程发送信号，获取另外一个进程 pid 等, object\_class：类别，目标（客体）是哪种类别，主要有 file,dir,socket, process, SEAndroid 还有 binder 等，在这些 基础上又细分出设备字符类型（chr\_file），链接文件（lnk\_file）等。
-   perm\_set：权限集合， 如 read， write 等。

更加复杂的格式为：

```
rule_name {source_type1 source_type2 ...} { target_type1 target_type2 ... } :object_class {perm_set}
```

安全策略语句格式涉及的相关内容定义在如下文件中：

-   源文件

| 文件路径 | 描述 |
| --- | --- |
| system/sepolicy/private/security\_classes | 描述 te 策略文件中的 object\_class 目标类型，如 class file，class dir |
| system/sepolicy/private/access\_vectors | 描述 te 策略文件中的 perm\_set 权限集合.如 common file{ioctl read ...} |
| system/sepolicy/public/te\_macros | 包含了 Selinux 所有的宏定义文件，如最常用的 init\_daemon\_domain() |
| system/sepolicy/public/global\_macros | 定义了各种 object\_class 和 permissions 的集合名，如 devfile\_class\_set，ipc\_class\_set 等， 权限集合如 x\_file\_perms，里面都是宏定义 |
| system/sepolicy/public/attributes | 定义系统中各种属性，属性是一种可进行关联的, 如 attribute domain;attribute exec\_type; |
| system/sepolicy/\*/file.te | 定义特定 type 的文件， 目录 vendor，private，public,prebuilts 都有该文件， |

-   根文件系统文件

| 文件路径 | 描述 |
| --- | --- |
| /system/etc/selinux | 平台策略文件编译之后生成到根文件系统中存放目录 |
| /vendor(odm/product)/etc/selinux | 厂商(odm/product)定制策略文件编译之后生成到根文件系统中存放目录 |
| /sys/fs/selinux | 内核为用户空间提供了 selinux 文件系统的挂载路径 |

### rule\_name

| 内容 | 描述 |
| --- | --- |
| allow | 允许权限操作，必须显示说明 |
| allowaudit | 允许记录，重点是记录， 允许权限规则必须使用 allow，此处允许记录是指允许对权限检查成功和失败的结果进行记录 |
| dontaudit | 对权限检查失败的操作不做记录 |
| neverallow | logd dev\_type:blk\_file { read write };此处意思是在其他策略文件中就不要出现 allow logd dev\_type:blk\_file { read write }的语句，否则编译会报错。类似于在考试出题时，上级要求作文中不允许写议论文， 而在下级自主出题时，就出现了允许写议论文的要求， 这样这个”允许”就明显不合规。 |

### object\_class

object\_class 类别作用是用于描述目标（客体）的类别，如 dir, file 类别， 不同的文件都可以设置相同的上下文，加上 object\_class 类别之后，可以更精准的去描述目标(客体)， 比如/data/test/(目录)和 /data/testfile(普通文件)，都是可以设置安全上下文为 u:object\_r:test:s0, 假如 allow 语句只想给定访问 testfile 普通文件时权限，而不想放开目录的权限， 就可以在在 allow 语句中将 object\_class 设置进来， 如:

```
allow myprocess test:file read;
```

一般 object\_class 都需要在 system/sepolicy/private/security\_classes 中声明， 而对应 classs 涉及到的具体权限项目是在 system/sepolicy/private/access\_vectors 中文件中声明的，如：

```
# file-related classes
class filesystem
class file
class dir
class fd
class lnk_file
class chr_file
class blk_file
class sock_file
class fifo_file
# network-related classes
class socket
class tcp_socket
class udp_socket
class rawip_socket
class node
class netif
.... class binder
.... # Property service
class property_service # userspace
# Service manager
class service_manager # userspace
```

```
common file
{
    ioctl
    read
    write
    create
    getattr
    setattr
    lock
    relabelfrom
    relabelto
    append
    map
    unlink
    link
    rename
    execute
    quotaon
    mounton
}


#
# Define a common prefix for socket access vectors.
#

common socket
{
# inherited from file
    ioctl
    read
    write
    create
    getattr
    setattr
    lock
    relabelfrom
    relabelto
    append
    map
# socket-specific
    bind
    connect
    listen
    accept
    getopt
    setopt
    shutdown
    recvfrom
    sendto
    name_bind
}
```

### perm\_set

Perm set 表示操作权限集合，system/sepolicy/public/global\_macros 会定义权限集宏定义, 如例子：

```
define(`x_file_perms', `{ getattr execute execute_no_trans map }')
define(`r_file_perms', `{ getattr open read ioctl lock map }')
define(`w_file_perms', `{ open append write lock map }')
define(`rx_file_perms', `{ r_file_perms x_file_perms }')
define(`ra_file_perms', `{ r_file_perms append }')
define(`rw_file_perms', `{ r_file_perms w_file_perms }')
define(`rwx_file_perms', `{ rw_file_perms x_file_perms }')
define(`create_file_perms', `{ create rename setattr unlink rw_file_perms }')

```

当然在 perm\_set 这个语法位置上也是可以使用上一节中的 access\_vectors 文件中出现的权限字符串。

### type

-   在安全策略格式中type为source\_type和target\_type

```
rule_name source_type target_type :object_class perm_set
```

-   在安全上下文格式中type为第三项

```
user:role:type:mls
```

其实安全策略中的source\_type和target\_type就是安全上下文中的第三项，type的定义在如下相关文件中：

| 文件 | 描述 |
| --- | --- |
| system/sepolicy/public/file.te | 系统中各种文件类型的声明， 如 type sockfs, fs\_type; |
| system/sepolicy/public/device.te | 系统中各种设备类型的声明， 如 type camera\_device, dev\_type; 很多时候我们在开发驱动的时候，会新建设备节点如一个串口设备，这个设备节点的 type 一般都习惯放在 device.te 中声明 |
| system/sepolicy/public/service.te | 声明系统中各种后台服务，如 type audioserver\_service, service\_manager\_type; |
| 各个进程的 te 文件 | 如 zygote.te，init.te 等文件， 主要是用于声明 domain, 如 type vold, domain; |

type的完整格式如下：

```
type type_id [, attribute_id,][attribute_id,] …;
方括号表示可选， attribute_id 表示和 type_id 进行关联的属性，可以是多个,之间用逗号隔开
```

例子如下：

```
type device, dev_type, fs_type;
此处表示声明(或理解新建)一个 device 类型， 该类型关联文件系统属性，以及设备属性
type graphics_device, dev_type;
此处表示声明(或理解新建)一个 graphics_device 类型， 该类型关联另外一个设备属性
```

### attribute

直接翻译过来叫属性， 主要用于在声明 type 的时候进行关联。 属性类似一个组，type 关联属性之后， 当前新 增的 type 就可以加入到这个属性组中，这个组拥有的权限， 新增的 type 也就自然拥有了这个权限)，举例：

```
type system_server, coredomain, domain;
type performanced, domain, coredomain;
{ system_server performanced }都关联(加入)了 domain 属性组
文件 public/domain.te +66
# Device accesses. allow domain device:dir search;
就可以等同于(伪代码)：
allow { system_server performanced } device:dir search;
```

attribute其实就是为了方便书写te规则，我们可以用attribute找到所有关联了属性的type集合，而不用每一个都写出来。

属性语法：

```
attribute attribute_id;
```

声明属性的文件 system/sepolicypublic/attributes:

```
attribute dev_type;
attribute domain;
```

其实 attribute 也可以理解成一种 type（集合），意味着属性也是可以用在安全上下文和安全策略中， 如：

```
u:object_r:某属性:s0
allow 某属性 device:dir search;
allow adbd 某属性:dir search;
```

不同属性所拥有的权限会在不同的 te 的文件中声明， 比如属性 domain 会在以下文件中定义 system/sepolicy/public/domain.te:

```
allow domain self:fd use;
allow domain proc:dir r_dir_perms;
allow domain proc_net_type:dir search;
r_dir_file(domain, self)
allow domain self:{ fifo_file file } rw_file_perms;
allow domain self:unix_dgram_socket { create_socket_perms sendto };
allow domain self:unix_stream_socket { create_stream_socket_perms connectto };
```

### 宏

```
#public/adbd.te文件

# adbd seclabel is specified in init.rc since
# it lives in the rootfs and has no unique file type.
type adbd, domain;
type adbd_exec, exec_type, file_type, system_file_type;

# Only init is allowed to enter the adbd domain via exec()
neverallow { domain -init } adbd:process transition;
neverallow * adbd:process dyntransition;

# Allow adbd start/stop mdnsd via ctl.start
set_prop(adbd, ctl_mdnsd_prop)
```

在如上示例的te文件中有set\_prop，这就是一个宏，其完整定义如下：

```
#public/te_macros文件

# set_prop(sourcedomain, targetproperty)
# Allows source domain to set the
# targetproperty.
#
define(`set_prop', `
unix_socket_connect($1, property, init)
allow $1 $2:property_service set;
get_prop($1, $2)
')

```

大部分宏都是在 system/sepolicy/public/te\_macros 中定义：

| 宏 | 描述 |
| --- | --- |
| type\_transition source\_type target\_type : class default\_type
例子：  
type\_transition vold storage\_file:dir storage\_stub\_file;

场景解释：  
Vold 进程会在打了 vold storage\_file 标签(上下文)的目录中创建一个文件时， 子文件的上下文应该变成 storage\_stub\_file 标签(上下文)

 | 当主体进程域source\_type对target\_type类型的客体进行 class 定义的操作时，主体或者客体会切换到 default\_type。意味着该声明是可以描述主体和客体的类型切换的， 比如客体文件类型切换， 或者主体进程域的 type 切换(上下文切换)， 注意：此时只是描述了从哪里切换到哪里的问题。 |
| domain\_trans(olddomain, target\_type, newdomain)  
缩写： DT  
例子：  
domain\_trans(vold, shell\_exec, blkid); | 定义允许在 olddomain 操作 target\_type 时，从 olddomain 到 newdomain 转化规则。主要针对进程上下文的转换，大部分进程都是从init 进程调动 fork 和 execv，此时子进程就需要从 init 的进程安全上下文切换到对应的子进程自己应有的进程安全上下文。此时就需要用该宏来切换。注意点， 该宏重点只是申请允许切换。也可以理解为 type\_transition()申请权限。 |
| domain\_auto\_trans(olddomain, target\_type, newdomain)  
例子：  
domain\_auto\_trans(init, charger\_exec, charger)当 init 进程在执行 charger\_exec 可执行程序时，自动切换到 charger进程上下文。 | 等同于 type\_transition 和 domain\_trans 两个，既描述从哪里切换到哪里， 也描述切换所需要的允许规则。 |
| file\_type\_trans(domain, dir\_type, file\_type)缩写： TT | 在 某 个 domain( 进 程 ) 在 安 全上 下 文 为dir\_type 类型的目录下创建子文件， 子文件会自动继承父目录的文件安全上下文，如果想子文件的上下文创建时变为 file\_type， 可通过 file\_type\_trans 申请权限， 注意，该宏重点是也只是申请允许切换的权限。 |
| file\_type\_auto\_trans(domain, dir\_type, file\_type)例子：一般在实际例子用的少 | 等同于 type\_transition 和 file\_type\_trans 两个，既描述从哪里切换到哪里， 也描述切换所需要的允许规则。 |
| r\_dir\_file(domain, type) | 允许 domain 访问安全上下文为 type 的目录下的文件和符号连接文件 |
| unconfined\_domain(domain) | 使指定的 doamin 获得更多的许可(这一宏只是 让 指 定 的 进 程 安 全 上 下 文 继 承mlstrustedsubject(在作为主体时， 能绕过MLS 检查相关)和 unconfineddomain 属性(在作为客体时， 能绕过 MLS 检查相关)) |
| binder\_use(domain) | 允许 domain 使用 binder 通信 |
| binder\_call(clientdomain, serverdomain) | 允许clientdomain通过binder调用serverdomain |
| binder\_service(domain) | 定义 domain 作为 binder service 的相关许可 |
| app\_domain(domain) | 为 domain 定义所有 android app 所需的基本许可(实际上是继承 appoamin 属性) |
| net\_domain(domain) | 为 doamin 定义访问网络所需的基本许可(实际上是继承 netdoamin 属性) |
| set\_prop(domain, prop) | 允许 domain 对 prop 属性有设置权限 |
| get\_prop(domain, prop) | 允许 domain 对 prop 属性有获取权限 |

## 编译

整个编译过程大致如下：

![编译.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/77afda658c4a404e92f5d0e07b3ad8b8~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240357&x-signature=Ji7X56IlqGZr5wfpHDR5g5vPJs0%3D) Android系统将源码部分分为平台和非平台，因为Android系统的开发除了google，还有硬件厂商（包括vendor、odm、oem等），google自己的代码为平台代码，硬件厂商的代码为非平台代码，如下图所示

![vendor.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/3ca19baaa5eb4a549e62877cedc2b67f~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240357&x-signature=syNjviPhww0LVczUW5QpWRa50Q8%3D) 所以SEAndroid的源码位置分为平台和非平台 2 处存放。

这些文件会经历如下编译步骤：

-   transform-policy-to-conf(m4 宏处理)---> 得到 conf 宏展开并合并文件
-   checkpolicy 编译 conf 文件--->得到 cil 通用中间语言文件
-   secilc 工具编译 cil 文件---> 得到目标文件 precompiled\_sepolicy 二进制

这个编译过程使用到的相关编译工具位于external/selinux目录：外部 SELinux 项目，用于构建主机环境中编译 SELinux 政策和标签所需的各种工具，如提供 libselinux 库函数，checkpolicy 编译器(policy.conf 编译成 cil 文件)，secilc 编译器(cil 文件编译成二进制)，getenfore/setenfore 命令。

```
external/selinux/
├── Android.bp
├── checkpolicy ： policy.conf 编译成 cil 文件
├── CleanSpec.mk
├── dbus
├── gui
├── libselinux : 提供 libselinux 库函数, getenfore/setenfore 命令
├── libsemanage
├── libsepol： 提供检查上下文语法的工具和库函数。
├── Makefile
├── mcstrans
├── MODULE_LICENSE_GPL
├── NOTICE
├── OWNERS
├── policycoreutils： 提供 load_policy，chcon 等命令
├── prebuilts : 提供 audit2allow，sediff 等命令
├── python
├── README
├── README.android
├── restorecond
├── sandbox
├── scripts
├── secilc : cil 文件编译成二进制
└── semodule-utils
```

最终这些文件会打包到根文件系统的如下目录：

-   /system/etc/selinux
-   /vendor/etc/selinux
-   /odm/etc/selinux
-   /oem/etc/selinux

![1.jpg](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/82764f55501c42d2a0d355a1dab12dae~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240357&x-signature=ci2b0j%2Fmmh9qBpi2gbaK9Bqto7g%3D) ![2.jpg](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/54d8e7cb7049415cb5b1008624dcf016~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240357&x-signature=JnTqtvL7ypvokZeVytSa95OShoc%3D)

这些文件在后续的权限检查判断过程中会被：安全服务（Security Server）相关模块以及内核LSM所使用到。

## Init初始化之SEAndroid

初始化代码位于源码目录system/core/init/selinux.cpp

```
int SetupSelinux(char** argv) {
    SelinuxInitialize();
}

void SelinuxInitialize() {
    Timer t;

    LOG(INFO) << "Loading SELinux policy";
    //SEAndroid安全策略加载
    if (!LoadPolicy()) {
        LOG(FATAL) << "Unable to load SELinux policy";
    }

    //获取selinux模式，其实就是从/sys/fs/selinux/enforce文件中去读取。
    bool kernel_enforcing = (security_getenforce() == 1);
    //从内核启动的参数中androidboot.selinux=xxx获取用户需要设置的selinux模式
    bool is_enforcing = IsEnforcing();
    //如果当前模式和用户设置的不一直， 那么就重新设置
    if (kernel_enforcing != is_enforcing) {
        if (security_setenforce(is_enforcing)) {
            PLOG(FATAL) << "security_setenforce(%s) failed" << (is_enforcing ? "true" : "false");
        }
    }
    //写入0，表示权限检查由内核来决定，1表示由用户空间来决定。默认android都写0
    if (auto result = WriteFile("/sys/fs/selinux/checkreqprot", "0"); !result) {
        LOG(FATAL) << "Unable to write to /sys/fs/selinux/checkreqprot: " << result.error();
    }

    // init's first stage can't set properties, so pass the time to the second stage.
    setenv("INIT_SELINUX_TOOK", std::to_string(t.duration().count()).c_str(), 1);
}
```

从Android8.0之后，因为Project Treble，system和vendor策略分离，所以Android 10上走的是LoadSplitPolicy

```
bool LoadPolicy() {
    return IsSplitPolicyDevice() ? LoadSplitPolicy() : LoadMonolithicPolicy();
}
```

```
bool LoadSplitPolicy() {
    const char* force_debuggable_env = getenv("INIT_FORCE_DEBUGGABLE");
    bool use_userdebug_policy =
            ((force_debuggable_env && "true"s == force_debuggable_env) &&
             AvbHandle::IsDeviceUnlocked() && access(kDebugRamdiskSEPolicy, F_OK) == 0);
    if (use_userdebug_policy) {
        LOG(WARNING) << "Using userdebug system sepolicy";
    }

    // Load precompiled policy from vendor image, if a matching policy is found there. The policy
    // must match the platform policy on the system image.
    std::string precompiled_sepolicy_file;
    // use_userdebug_policy requires compiling sepolicy with userdebug_plat_sepolicy.cil.
    // Thus it cannot use the precompiled policy from vendor image.
    if (!use_userdebug_policy && FindPrecompiledSplitPolicy(&precompiled_sepolicy_file)) {
        unique_fd fd(open(precompiled_sepolicy_file.c_str(), O_RDONLY | O_CLOEXEC | O_BINARY));
        if (fd != -1) {
            if (selinux_android_load_policy_from_fd(fd, precompiled_sepolicy_file.c_str()) < 0) {
                LOG(ERROR) << "Failed to load SELinux policy from " << precompiled_sepolicy_file;
                return false;
            }
            return true;
        }
    }
    // No suitable precompiled policy could be loaded

    LOG(INFO) << "Compiling SELinux policy";

    // We store the output of the compilation on /dev because this is the most convenient tmpfs
    // storage mount available this early in the boot sequence.
    char compiled_sepolicy[] = "/dev/sepolicy.XXXXXX";
    unique_fd compiled_sepolicy_fd(mkostemp(compiled_sepolicy, O_CLOEXEC));
    if (compiled_sepolicy_fd < 0) {
        PLOG(ERROR) << "Failed to create temporary file " << compiled_sepolicy;
        return false;
    }

    // Determine which mapping file to include
    std::string vend_plat_vers;
    if (!GetVendorMappingVersion(&vend_plat_vers)) {
        return false;
    }
    std::string plat_mapping_file("/system/etc/selinux/mapping/" + vend_plat_vers + ".cil");

    std::string product_policy_cil_file("/product/etc/selinux/product_sepolicy.cil");
    if (access(product_policy_cil_file.c_str(), F_OK) == -1) {
        product_policy_cil_file.clear();
    }

    std::string product_mapping_file("/product/etc/selinux/mapping/" + vend_plat_vers + ".cil");
    if (access(product_mapping_file.c_str(), F_OK) == -1) {
        product_mapping_file.clear();
    }

    // vendor_sepolicy.cil and plat_pub_versioned.cil are the new design to replace
    // nonplat_sepolicy.cil.
    std::string plat_pub_versioned_cil_file("/vendor/etc/selinux/plat_pub_versioned.cil");
    std::string vendor_policy_cil_file("/vendor/etc/selinux/vendor_sepolicy.cil");

    if (access(vendor_policy_cil_file.c_str(), F_OK) == -1) {
        // For backward compatibility.
        // TODO: remove this after no device is using nonplat_sepolicy.cil.
        vendor_policy_cil_file = "/vendor/etc/selinux/nonplat_sepolicy.cil";
        plat_pub_versioned_cil_file.clear();
    } else if (access(plat_pub_versioned_cil_file.c_str(), F_OK) == -1) {
        LOG(ERROR) << "Missing " << plat_pub_versioned_cil_file;
        return false;
    }

    // odm_sepolicy.cil is default but optional.
    std::string odm_policy_cil_file("/odm/etc/selinux/odm_sepolicy.cil");
    if (access(odm_policy_cil_file.c_str(), F_OK) == -1) {
        odm_policy_cil_file.clear();
    }
    const std::string version_as_string = std::to_string(SEPOLICY_VERSION);

    // clang-format off
    std::vector<const char*> compile_args {
        "/system/bin/secilc",
        use_userdebug_policy ? kDebugRamdiskSEPolicy: plat_policy_cil_file,
        "-m", "-M", "true", "-G", "-N",
        "-c", version_as_string.c_str(),
        plat_mapping_file.c_str(),
        "-o", compiled_sepolicy,
        // We don't care about file_contexts output by the compiler
        "-f", "/sys/fs/selinux/null",  // /dev/null is not yet available
    };
    // clang-format on

    if (!product_policy_cil_file.empty()) {
        compile_args.push_back(product_policy_cil_file.c_str());
    }
    if (!product_mapping_file.empty()) {
        compile_args.push_back(product_mapping_file.c_str());
    }
    if (!plat_pub_versioned_cil_file.empty()) {
        compile_args.push_back(plat_pub_versioned_cil_file.c_str());
    }
    if (!vendor_policy_cil_file.empty()) {
        compile_args.push_back(vendor_policy_cil_file.c_str());
    }
    if (!odm_policy_cil_file.empty()) {
        compile_args.push_back(odm_policy_cil_file.c_str());
    }
    compile_args.push_back(nullptr);

    if (!ForkExecveAndWaitForCompletion(compile_args[0], (char**)compile_args.data())) {
        unlink(compiled_sepolicy);
        return false;
    }
    unlink(compiled_sepolicy);

    LOG(INFO) << "Loading compiled SELinux policy";
    if (selinux_android_load_policy_from_fd(compiled_sepolicy_fd, compiled_sepolicy) < 0) {
        LOG(ERROR) << "Failed to load SELinux policy from " << compiled_sepolicy;
        return false;
    }

    return true;
}
```

LoadSplitPolicy函数功能如下：

-   查找上一节“编译”产物中是否有precompiled\_sepolicy二进制文件
-   如果没有开始Compiling SELinux policy，它会收集编译产物中contexts文件和cil文件，然后将其编译后写入/dev/sepolicy.XXXXXX

### 查找precompiled\_sepolicy

```
bool FindPrecompiledSplitPolicy(std::string* file) {
    file->clear();
    // If there is an odm partition, precompiled_sepolicy will be in
    // odm/etc/selinux. Otherwise it will be in vendor/etc/selinux.
    static constexpr const char vendor_precompiled_sepolicy[] =
        "/vendor/etc/selinux/precompiled_sepolicy";
    static constexpr const char odm_precompiled_sepolicy[] =
        "/odm/etc/selinux/precompiled_sepolicy";
    if (access(odm_precompiled_sepolicy, R_OK) == 0) {
        *file = odm_precompiled_sepolicy;
    } else if (access(vendor_precompiled_sepolicy, R_OK) == 0) {
        *file = vendor_precompiled_sepolicy;
    } else {
        PLOG(INFO) << "No precompiled sepolicy";
        return false;
    }
    std::string actual_plat_id;
    if (!ReadFirstLine("/system/etc/selinux/plat_sepolicy_and_mapping.sha256", &actual_plat_id)) {
        PLOG(INFO) << "Failed to read "
                      "/system/etc/selinux/plat_sepolicy_and_mapping.sha256";
        return false;
    }
    std::string actual_product_id;
    if (!ReadFirstLine("/product/etc/selinux/product_sepolicy_and_mapping.sha256",
                       &actual_product_id)) {
        PLOG(INFO) << "Failed to read "
                      "/product/etc/selinux/product_sepolicy_and_mapping.sha256";
        return false;
    }

    std::string precompiled_plat_id;
    std::string precompiled_plat_sha256 = *file + ".plat_sepolicy_and_mapping.sha256";
    if (!ReadFirstLine(precompiled_plat_sha256.c_str(), &precompiled_plat_id)) {
        PLOG(INFO) << "Failed to read " << precompiled_plat_sha256;
        file->clear();
        return false;
    }
    std::string precompiled_product_id;
    std::string precompiled_product_sha256 = *file + ".product_sepolicy_and_mapping.sha256";
    if (!ReadFirstLine(precompiled_product_sha256.c_str(), &precompiled_product_id)) {
        PLOG(INFO) << "Failed to read " << precompiled_product_sha256;
        file->clear();
        return false;
    }
    if (actual_plat_id.empty() || actual_plat_id != precompiled_plat_id ||
        actual_product_id.empty() || actual_product_id != precompiled_product_id) {
        file->clear();
        return false;
    }
    return true;
}
```

FindPrecompiledSplitPolicy函数功能：

-   从/odm/etc/selinux/和/vendor/etc/selinux/依次查找precompiled\_sepolicy，找到哪个就使用
-   如果找到文件，校验文件的sha256，这个校验文件在编译的时候也是会一并生成的

然后文件找到后续会调用selinux\_android\_load\_policy\_from\_fd函数（后面会讲）。

### 编译precompiled\_sepolicy

如果没有找到二进制precompiled\_sepolicy文件，就开始编译

```
LOG(INFO) << "Compiling SELinux policy";

std::vector<const char*> compile_args {
    "/system/bin/secilc",
    use_userdebug_policy ? kDebugRamdiskSEPolicy: plat_policy_cil_file,
    "-m", "-M", "true", "-G", "-N",
    "-c", version_as_string.c_str(),
    plat_mapping_file.c_str(),
    "-o", compiled_sepolicy,
    // We don't care about file_contexts output by the compiler
    "-f", "/sys/fs/selinux/null",  // /dev/null is not yet available
};
```

使用/system/bin/secilc编译工具进行编译 ![image.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/16d72a9ec06b4cc6b9b6ab137098e059~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240357&x-signature=5nqDOVEngDla0INe8idqkcXs6wM%3D) 编译所需源文件放在compile\_args中，最后会fork一个进程进行单独编译，并等待进程处理完返回

```
if (!ForkExecveAndWaitForCompletion(compile_args[0], (char**)compile_args.data())) {
    unlink(compiled_sepolicy);
    return false;
}
unlink(compiled_sepolicy);
```

当编译玩成后会调用函数selinux\_android\_load\_policy\_from\_fd。

## selinux\_android\_load\_policy\_from\_fd

```
int selinux_android_load_policy_from_fd(int fd, const char *description)
{
    set_selinuxmnt(SELINUXMNT);
    
    ....

    map = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    ....

    rc = security_load_policy(map, sb.st_size);

    return 0;
}
```

首先会设置一个SELINUXMNT宏，它定义如下

```
#/external/selinux/libselinux/src/policy.h
#define SELINUXMNT "/sys/fs/selinux"

#/external/selinux/libselinux/src/init.c
 void set_selinuxmnt(const char *mnt)
{
selinux_mnt = strdup(mnt);
}
```

结果就是selinux\_mnt="/sys/fs/selinux"

mmap将fd进行了映射，根据前面的分析fd指向如下 2 处文件之一：

-   /odm/etc/selinux/或/vendor/etc/selinux/的precompiled\_sepolicy二进制文件
-   文件/dev/sepolicy.XXXXXX 这两个文件的内容其实是一样的，都是编译后提供给内核使用的二进制数据。

```
#/external/selinux/libselinux/src/load_policy.c
int security_load_policy(void *data, size_t len)
{
    char path[PATH_MAX];
    int fd, ret;

    if (!selinux_mnt) {
       errno = ENOENT;
       return -1;
    }

    snprintf(path, sizeof path, "%s/load", selinux_mnt);
    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
       return -1;

    ret = write(fd, data, len);
    close(fd);
    if (ret < 0)
       return -1;
    return 0;
}
```

最后会将二进制数据写入/sys/fs/selinux/load文件中，后续内核会读取这些数据作为安全策略进行权限的判断工作（后内核代码就不进一步分析了）。

## 参考文章

-   [blog.csdn.net/headwind\_/a…](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fheadwind_%2Farticle%2Fdetails%2F119704755 "https://blog.csdn.net/headwind_/article/details/119704755")
-   [blog.csdn.net/headwind\_/a…](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fheadwind_%2Farticle%2Fdetails%2F119707417 "https://blog.csdn.net/headwind_/article/details/119707417")
-   [blog.csdn.net/luoshengyan…](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fluoshengyang%2Farticle%2Fdetails%2F37613135 "https://blog.csdn.net/luoshengyang/article/details/37613135")