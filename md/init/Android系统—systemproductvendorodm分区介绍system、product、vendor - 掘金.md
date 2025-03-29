> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

> 基于Android10 源码分析

## system、product、vendor、odm分区介绍

![vendor.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/f77950439c27414b9d02225081632f9a~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743238336&x-signature=bWjy6wKnjlJlGJ5P5J%2B76rmNz5I%3D) Android系统中的分区对于设备的构建和管理至关重要。在Android系统中，我们常常听到vendor、ODM、system、product等分区，它们各自有不同的作用和职责。以下是对这些分区的简要介绍：

-   vendor分区：vendor分区包含了设备厂商定制的底层驱动程序和系统库。这些定制化的驱动程序和库使得设备能够支持特定的硬件组件，如相机、音频芯片等。vendor分区通常是设备制造商根据Android开源项目的源代码进行定制化的结果。这个分区的存在使得设备厂商可以对设备的硬件功能进行更细致的控制，同时也使得设备的硬件特性得以充分发挥。
-   ODM分区：ODM（Original Design Manufacturer）分区是指设备制造商为满足不同客户需求而进行的定制化分区。ODM分区通常包含了客户定制的软件功能和配置文件。这个分区的存在使得设备制造商能够为客户提供更加个性化的服务，满足客户的特定需求。
-   system分区：system分区是Android系统的基础分区，包含了Android系统的核心文件和应用程序。这个分区中的文件是Android系统运行所必需的，任何对system分区的修改都可能导致系统不稳定或者功能失效。因此，通常情况下，我们不建议对system分区进行修改。
-   product分区：product分区是Android系统中的一个高级定制分区。它通常包含了针对某一特定设备系列进行优化的系统配置和应用程序。product分区使得设备制造商可以在保持系统稳定性的前提下，对设备的性能和功能进行进一步的提升。此外，通过对product分区的定制，设备制造商还可以实现对设备的外观、主题等方面的个性化设置。

总之，Android系统中的vendor、ODM、system、product等分区各自承担着不同的职责，共同构成了设备的系统架构。这些分区的合理使用和管理，有助于提高设备的性能和用户体验，同时也为设备制造商和客户提供了更大的灵活性和定制空间。

### 输出到system分区

Android.bp语法

```
cc_binary {
    name: "test",

    srcs: ["test.cpp"],
    shared_libs: [
        "libutils",
        "liblog",
        "libcutils",
    ],
    cflags: [
    "-Wno-multichar",
    "-Werror",
    "-Wall",
"-Wno-implicit-fallthrough",
"-Wno-missing-field-initializers",
"-Wno-missing-braces",
"-Wno-unused-variable",
    ],
}
```

默认输出到:system分区  
默认输出到:out/target/product/redfin/system/bin/test目录

### 输出到product分区

Android.bp语法

```
cc_binary {
    name: "test",

    srcs: ["test.cpp"],
    shared_libs: [
        "libutils",
        "liblog",
        "libcutils",
    ],

    //v2.0 输出到:product分区
    product_specific: true,

    cflags: [
        "-Wno-multichar",
        "-Werror",
        "-Wall",
"-Wno-implicit-fallthrough",
"-Wno-missing-field-initializers",
"-Wno-missing-braces",
"-Wno-unused-variable",
    ],
}

```

Android.mk语法

```
LOCAL_PRODUCT_MODULE := true
```

输出到:out/target/product/redfin/product/bin/test

### 输出到odm分区

Android.bp语法

```
cc_binary {
    name: "test",

    srcs: ["test.cpp"],
    shared_libs: [
        "libutils",
        "liblog",
        "libcutils",
    ],

    //v3.0 输出到odm分区
    device_specific: true, 

    cflags: [
        "-Wno-multichar",
        "-Werror",
        "-Wall",
"-Wno-implicit-fallthrough",
"-Wno-missing-field-initializers",
"-Wno-missing-braces",
"-Wno-unused-variable",
    ],
}
```

Android.mk语法

```
LOCAL_ODM_MODULE := true
```

输出到:out/target/product/redfin/vendor/odm/bin/test

### 输出到vendor分区

Android.bp语法

```
cc_binary {
    name: "test",

    srcs: ["test.cpp"],
    shared_libs: [
        "libutils",
        "liblog",
        "libcutils",
    ],
    //v4.0 输出到:vendor分区
    vendor: true,

    cflags: [
        "-Wno-multichar",
        "-Werror",
        "-Wall",
"-Wno-implicit-fallthrough",
"-Wno-missing-field-initializers",
"-Wno-missing-braces",
"-Wno-unused-variable",
    ],
}
```

Android.mk语法

```
LOCAL_VENDOR_MODULE := true
```

输出到:out/target/product/redfin/vendor/bin/test

参考文章：

[blog.csdn.net/u010164190/…](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fu010164190%2Farticle%2Fdetails%2F136570819 "https://blog.csdn.net/u010164190/article/details/136570819") [cloud.tencent.com/developer/a…](https://link.juejin.cn/?target=https%3A%2F%2Fcloud.tencent.com%2Fdeveloper%2Farticle%2F1170083 "https://cloud.tencent.com/developer/article/1170083")