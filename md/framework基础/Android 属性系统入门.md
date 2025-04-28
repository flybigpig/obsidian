## Android 属性系统入门

*阿豪* *10/18/2023*

这是一个介绍 Android 属性系统的系列文章:

+   Android 属性系统入门（本文）
+   属性文件生成过程分析
+   如何添加系统属性
+   属性与 Selinux
+   属性系统整体框架与启动过程分析
+   属性读写过程源码分析

本文基于 AOSP android-10.0.0\_r41 版本讲解

在 Android 系统中，为统一管理系统的属性，设计了一个统一的属性系统，每个属性都是一个 key-value 对。 我们可以通过 shell 命令，Native 函数接口，Java 函数接口的方式来读写这些 key-vaule 对。

## [#](#属性在哪里) 属性在哪里？

init 进程在启动会去加载后缀为 .prop 的属性文件， 将属性文件中的属性加载到共享内存中， 这样系统就有了默认的一些属性。

属性文件都在哪里呢？

属性文件的后缀绝大部分都是 prop，我们可以在 Android 模拟器的 shell 环境下搜索：

```bash
find . -name "*.prop"

/default.prop
/data/local.prop
/system/build.prop
/system/product/build.prop
/vendor/build.prop
/vendor/odm/etc/build.prop
/vendor/default.prop
/data/property/
```



我们看看 `/default.prop` 属性文件的内容：

```bash
cat /default.prop

#
# ADDITIONAL_DEFAULT_PROPERTIES
#
ro.actionable_compatible_property.enabled=true
ro.postinstall.fstab.prefix=/system
ro.secure=0
ro.allow.mock.location=1
ro.debuggable=1
debug.atrace.tags.enableflags=0
dalvik.vm.image-dex2oat-Xms=64m
dalvik.vm.image-dex2oat-Xmx=64m
dalvik.vm.dex2oat-Xms=64m
dalvik.vm.dex2oat-Xmx=512m
dalvik.vm.usejit=true
dalvik.vm.usejitprofiles=true
dalvik.vm.dexopt.secondary=true
dalvik.vm.appimageformat=lz4
ro.dalvik.vm.native.bridge=0
pm.dexopt.first-boot=extract
pm.dexopt.boot=extract
pm.dexopt.install=speed-profile
pm.dexopt.bg-dexopt=speed-profile
pm.dexopt.ab-ota=speed-profile
pm.dexopt.inactive=verify
pm.dexopt.shared=speed
dalvik.vm.dex2oat-resolve-startup-strings=true
dalvik.vm.dex2oat-max-image-block-size=524288
dalvik.vm.minidebuginfo=true
dalvik.vm.dex2oat-minidebuginfo=true
ro.iorapd.enable=false
tombstoned.max_tombstone_count=50
persist.traced.enable=1
ro.com.google.locationfeatures=1
ro.setupwizard.mode=DISABLED
persist.sys.usb.config=adb
```



可以看出属性确实是一些 key-value 对。

init 进程会调用 `property_load_boot_defaults` 函数来加载属性文件：

```cpp
void property_load_boot_defaults(bool load_debug_prop) {
    // TODO(b/117892318): merge prop.default and build.prop files into one
    // We read the properties and their values into a map, in order to always allow properties
    // loaded in the later property files to override the properties in loaded in the earlier
    // property files, regardless of if they are "ro." properties or not.
    std::map<std::string, std::string> properties;
    if (!load_properties_from_file("/system/etc/prop.default", nullptr, &properties)) {
        // Try recovery path
        if (!load_properties_from_file("/prop.default", nullptr, &properties)) {
            // Try legacy path
            load_properties_from_file("/default.prop", nullptr, &properties);
        }
    }
    load_properties_from_file("/system/build.prop", nullptr, &properties);
    load_properties_from_file("/vendor/default.prop", nullptr, &properties);
    load_properties_from_file("/vendor/build.prop", nullptr, &properties);
    if (SelinuxGetVendorAndroidVersion() >= __ANDROID_API_Q__) {
        load_properties_from_file("/odm/etc/build.prop", nullptr, &properties);
    } else {
        load_properties_from_file("/odm/default.prop", nullptr, &properties);
        load_properties_from_file("/odm/build.prop", nullptr, &properties);
    }
    load_properties_from_file("/product/build.prop", nullptr, &properties);
    load_properties_from_file("/product_services/build.prop", nullptr, &properties);
    load_properties_from_file("/factory/factory.prop", "ro.*", &properties);

    if (load_debug_prop) {
        LOG(INFO) << "Loading " << kDebugRamdiskProp;
        load_properties_from_file(kDebugRamdiskProp, nullptr, &properties);
    }

    for (const auto& [name, value] : properties) {
        std::string error;
        if (PropertySet(name, value, &error) != PROP_SUCCESS) {
            LOG(ERROR) << "Could not set '" << name << "' to '" << value
                       << "' while loading .prop files" << error;
        }
    }

    property_initialize_ro_product_props();
    property_derive_build_fingerprint();

    update_sys_usb_config();
}
```



从源码中我们也可以看到 init 进程加载了哪些属性文件以及加载的顺序。

## [#](#属性长什么样) 属性长什么样？

每一个属性是一个 key-value 对：

```bash
ro.actionable_compatible_property.enabled=true
ro.postinstall.fstab.prefix=/system
ro.secure=0
ro.allow.mock.location=1
ro.debuggable=1
debug.atrace.tags.enableflags=0
dalvik.vm.image-dex2oat-Xms=64m
dalvik.vm.image-dex2oat-Xmx=64m
```


等号左边是属性的名字，等号右边是属性的值



属性的分类：

+   一般属性：普通的 key-value 对，没有其他功能，系统启动后，如果修改了某个属性值(仅修改了内存中的值，未写入到文件)，再重启系统，修改的值不会被保存下来，读取到的仍是修改前的值
+   特殊属性
    +   属性名称以 `ro` 开头，那么这个属性被视为只读属性。一旦设置，属性值不能改变。
    +   `net` 开头的属性，顾名思义，就是与网络相关的属性，`net` 属性中有一个特殊的属性：`net.change`，它记录了每一次最新设置和更新的 `net` 属性，也就是每次设置和更新 `net`,属性时则会自动的更新 `net.change` 属性，`net.change` 属性的 value 就是这个被设置或者更新的 `net` 属性的 name。例如我们更新了属性 `net.bt.name` 的值，由于 `net` 有属性发生了变化，那么属性服务就会自动更新 `net.change`，将其值设置为 `net.bt.name`。
    +   以 `persist` 为开头的属性值，当在系统中通过 setprop 命令设置这个属性时，就会在 `/data/property/` 目录下会保存一个副本。这样在系统重启后，按照加载流程这些 `persist` 属性的值就不会消失了。
    +   属性 `ctrl.start` 和 `ctrl.stop` 是用来启动和停止服务。这里的服务是指定义在 rc 后缀文件中的服务。当我们向 `ctrl.start` 属性写入一个值时，属性服务将使用该属性值作为服务名找到该服务，启动该服务。这项服务的启动结果将会放入 `init.svc.<服务名>` 属性中，可以通过查询这个属性值，以确定服务是否已经启动。

## [#](#如何读写属性) 如何读写属性：

命令行：

```bash
getprop "wlan.driver.status"
setprop "wlan.driver.status"  "timeout"
```

 

Native 代码：

```bash
char buf[20]="qqqqqq";
char tempbuf[PROPERTY_VALUE_MAX];
property_set("type_value",buf);
property_get("type_value",tempbuf,"0");
```


Java 代码：

```bash
String navBarOverride = SystemProperties.get("qemu.hw.mainkeys");
SystemProperties.set("service.bootanim.exit", "0");
```

 

## [#](#属性的作用) 属性的作用

常见的属性文件的作用如下：
![](https://cdn.jsdelivr.net/gh/zzh0838/MyImages/img/20231018121100.png)
## [#](#参考资料) 参考资料

+   [Android系统10 RK3399 init进程启动(三十三) property属性系统框架 (opens new window)](https://blog.csdn.net/ldswfun/article/details/126129569?spm=1001.2014.3001.5501)
+   [Android属性系统简介 (opens new window)](https://www.cnblogs.com/l2rf/p/6610348.html)
+   [Android系统中属性值的设置和使用 (opens new window)](https://blog.csdn.net/qq_30624591/article/details/102679377)
+   [使用persist属性来调用脚本文件 (opens new window)](https://www.cnblogs.com/peter-chen/p/3946129.html)
+   [Android系统10 RK3399 init进程启动(三十五) 属性文件介绍和生成过程 (opens new window)](https://blog.csdn.net/ldswfun/article/details/126325567?spm=1001.2014.3001.5502)