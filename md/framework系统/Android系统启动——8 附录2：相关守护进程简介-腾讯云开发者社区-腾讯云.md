[社区首页](https://cloud.tencent.com/developer) >[专栏](https://cloud.tencent.com/developer/column) >Android系统启动——8 附录2：相关守护进程简介

在`init.rc`中定义了很多系统的守护进程，这里主要是做一些简单的介绍

### 一、uevent

负责相应uevent事件，创建设备节点文件： 代码在[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252Finit.rc&objectId=1199517&objectType=1&isNewArticle=undefined) 550行

```
550service ueventd /sbin/ueventd
551    class core
552    critical
553    seclabel u:r:ueventd:s0
```

### 二、console

包含常用的shell命令、如ls、cd等 代码在[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252Finit.rc&objectId=1199517&objectType=1&isNewArticle=undefined) 574行

```
574service console /system/bin/sh
575    class core
576    console
577    disabled
578    user shell
579    group shell log
580    seclabel u:r:shell:s0
```

### 三、adbd

abd的守护进程： 代码在[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252Finit.rc&objectId=1199517&objectType=1&isNewArticle=undefined) 585行

```
585# adbd is controlled via property triggers in init.<platform>.usb.rc
586service adbd /sbin/adbd --root_seclabel=u:r:su:s0
587    class core
588    socket adbd stream 660 system system
589    disabled
590    seclabel u:r:adbd:s0
```

### 四、servicemanager

binder的服务总管，负责binder服务的注册和查找 代码在[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252Finit.rc&objectId=1199517&objectType=1&isNewArticle=undefined) 602行

```
602service servicemanager /system/bin/servicemanager
603    class core
604    user system
605    group system
606    critical
607    onrestart restart healthd
608    onrestart restart zygote
609    onrestart restart media
610    onrestart restart surfaceflinger
611    onrestart restart drm
```

### 五、vold

负责完成系统USB存储卡等扩展存储自动挂载的守护进程 代码在[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252Finit.rc&objectId=1199517&objectType=1&isNewArticle=undefined) 613行

```
613service vold /system/bin/vold \
614        --blkid_context=u:r:blkid:s0 --blkid_untrusted_context=u:r:blkid_untrusted:s0 \
615        --fsck_context=u:r:fsck:s0 --fsck_untrusted_context=u:r:fsck_untrusted:s0
616    class core
617    socket vold stream 0660 root mount
618    socket cryptd stream 0660 root mount
619    ioprio be 2
```

### 六、netd

Android 网络守护进程 代码在[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252Finit.rc&objectId=1199517&objectType=1&isNewArticle=undefined) 621行

```
621service netd /system/bin/netd
622    class main
623    socket netd stream 0660 root system
624    socket dnsproxyd stream 0660 root inet
625    socket mdns stream 0660 root system
626    socket fwmarkd stream 0660 root inet
```

### 七、debuggerd

负责异常退出的诊断。如果侦测到程序崩溃，debuggerd将把崩溃时的进程状态信息输出到文件和串口中，供开发人员分析和调试使用： 代码在[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252Finit.rc&objectId=1199517&objectType=1&isNewArticle=undefined) 628行

```
628service debuggerd /system/bin/debuggerd
629    class main
630    writepid /dev/cpuset/system-background/tasks
631
632service debuggerd64 /system/bin/debuggerd64
633    class main
634    writepid /dev/cpuset/system-background/tasks
```

### 八、ril-deamon

手机底层的通信系统的守护进程 代码在[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252Finit.rc&objectId=1199517&objectType=1&isNewArticle=undefined) 636行

```
636service ril-daemon /system/bin/rild
637    class main
638    socket rild stream 660 root radio
639    socket sap_uim_socket1 stream 660 bluetooth bluetooth
640    socket rild-debug stream 660 radio system
641    user root
642    group radio cache inet misc audio log
```

### 九、surfaceflinger：

负责合成系统所有显示图层的服务进程 代码在[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252Finit.rc&objectId=1199517&objectType=1&isNewArticle=undefined) 644行

```
644service surfaceflinger /system/bin/surfaceflinger
645    class core
646    user system
647    group graphics drmrpc
648    onrestart restart zygote
649    writepid /dev/cpuset/system-background/tasks
```

### 十、media：

系统多媒体部分的守护进程，包含了audio、mediaplayer以及camera 等系统服务 代码在[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252Finit.rc&objectId=1199517&objectType=1&isNewArticle=undefined) 656行

```
656service media /system/bin/mediaserver
657    class main
658    user media
659    group audio camera inet net_bt net_bt_admin net_bw_acct drmrpc mediadrm
660    ioprio rt 4
```

### 十一、bootanim：

播放开机动画的进程 代码在[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252Finit.rc&objectId=1199517&objectType=1&isNewArticle=undefined) 644行

```
676service bootanim /system/bin/bootanimation
677    class core
678    user graphics
679    group graphics audio
680    disabled
681    oneshot
```

### 十二、installd：

Android的安装服务守护进程

```
687service installd /system/bin/installd
688    class main
689    socket installd stream 600 system system
```

### 官人\[飞吻\]，你都把臣妾从头看到尾了，喜欢就点个赞呗(眉眼)！！！！

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2018.03.08 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除