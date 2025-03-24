
最后总结一下，Android 启动过程中重要的 trigger 事件如下：

```
- early-init
  +--- start ueventd
- init
- charger                       # 当 ro.bootmode 为 charger 执行
- late-init
  +--- early-fs
  +--- fs                       # 挂载系统分区
     +--- mount_all /vendor/etc/fstab.${ro.hardware}
  +--- post-fs                  # 执行依赖文件系统的命令
     +--- load_system_props
        +--- load_properties_from_file("/system/build.prop", NULL);
        +--- load_properties_from_file("/odm/build.prop", NULL);
        +--- load_properties_from_file("/vendor/build.prop", NULL);
        +--- load_properties_from_file("/factory/factory.prop", "ro.*");
        +--- load_recovery_id_prop();
     +--- start logd
     +--- start servicemanager
     +--- start hwservicemanager
     +--- start vndservicemanager
  +--- late-fs
     +--- class_start early_hal
  +--- post-fs-data              # data分区初始化
     +--- start vold
     +--- installkey /data
     +--- bootchart start
     +--- init_user0
  +--- zygote-start
     +--- exec_start update_verifier_nonencrypted
     +--- start netd
     +--- start zygote
     +--- start zygote_secondary
  +--- load_persist_props_action
     +--- load_persist_props
     +--- LoadPersistentProperties()
     +--- start logd
     +--- start logd-reinit
  +--- firmware_mounts_complete
     +--- rm /dev/.booting
  +--- early-boot
  +--- boot

  +--- property:sys.boot_completed=1
     +--- bootchart stop
```


## 常见命令
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