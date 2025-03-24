
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