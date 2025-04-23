---
created: 2025-04-23T10:26:26 (UTC +08:00)
tags: [android.bp]
source: https://blog.csdn.net/wenzhi20102321/article/details/135631671
author: 成就一亿技术人!
---

# Android.bp详解+入门必备-CSDN博客

> ## Excerpt
> 文章浏览阅读1w次，点赞37次，收藏64次。Android.bp 就是为了用来替换 Android.mk 一个脚本语言文件。如果学习过Android.mk，在学习一下Android.bp应该是很easy的，如果不是很熟悉可以学习了解一下。Android.bp和Android.mk作用都是一样的，在系统源码中用来编译出类库.jar,应用文件.apk，动态库.so，静态库.a作用。其中关键的就是模块类型定义和不同的属性定义。Android.bp文件用类似json的简洁声明来描述需要构建的模块。_android.bp

---
## Android.bp详解+入门必备

#### 文章目录

-   [Android.bp详解+入门必备](https://blog.csdn.net/wenzhi20102321/article/details/135631671#Androidbp_0)
-   -   [一、前言](https://blog.csdn.net/wenzhi20102321/article/details/135631671#_3)
    -   [二、Android.bp 文件模版](https://blog.csdn.net/wenzhi20102321/article/details/135631671#Androidbp__24)
    -   -   [1、编译.jar包](https://blog.csdn.net/wenzhi20102321/article/details/135631671#1jar_26)
        -   [2、编译apk](https://blog.csdn.net/wenzhi20102321/article/details/135631671#2apk_49)
        -   -   [（1）以apk编译apk？](https://blog.csdn.net/wenzhi20102321/article/details/135631671#1apkapk_51)
            -   [（2）以java源码编译apk](https://blog.csdn.net/wenzhi20102321/article/details/135631671#2javaapk_128)
        -   [3、编译动态库.so](https://blog.csdn.net/wenzhi20102321/article/details/135631671#3so_157)
        -   [4、编译静态库.a](https://blog.csdn.net/wenzhi20102321/article/details/135631671#4a_202)
        -   [5、Android.mk 编译文件小结](https://blog.csdn.net/wenzhi20102321/article/details/135631671#5Androidmk__231)
        -   [编译类型的BuildType总结：](https://blog.csdn.net/wenzhi20102321/article/details/135631671#BuildType_267)
        -   [Android系统源码编译Android.bp文件方式：](https://blog.csdn.net/wenzhi20102321/article/details/135631671#AndroidAndroidbp_283)
    -   [三、Android.bp 具体示例](https://blog.csdn.net/wenzhi20102321/article/details/135631671#Androidbp__291)
    -   [四、Android.bp 主要属性](https://blog.csdn.net/wenzhi20102321/article/details/135631671#Androidbp__348)
    -   [五、总结](https://blog.csdn.net/wenzhi20102321/article/details/135631671#_431)
    -   -   [1、Android.bp 的简单使用总结](https://blog.csdn.net/wenzhi20102321/article/details/135631671#1Androidbp__433)
        -   [2、Android.bp 详解](https://blog.csdn.net/wenzhi20102321/article/details/135631671#2Androidbp__451)
        -   [3、Android.bp的其他知识](https://blog.csdn.net/wenzhi20102321/article/details/135631671#3Androidbp_474)
        -   -   [（1）Android所有bp属性和mk属性的对照关系完整文件：](https://blog.csdn.net/wenzhi20102321/article/details/135631671#1Androidbpmk_478)
            -   [（2）Android.bp文件编译的模块类型和属性定义字符串都是大写的](https://blog.csdn.net/wenzhi20102321/article/details/135631671#2Androidbp_486)
            -   [（3）其他](https://blog.csdn.net/wenzhi20102321/article/details/135631671#3_515)

### 一、前言

Android.bp 就是为了用来替换 Android.mk 一个[脚本语言](https://so.csdn.net/so/search?q=%E8%84%9A%E6%9C%AC%E8%AF%AD%E8%A8%80&spm=1001.2101.3001.7020)文件。

如果学习过Android.mk，在学习一下Android.bp应该是很easy的，如果不是很熟悉可以学习了解一下。

Android.bp和Android.mk作用都是一样的，在系统源码中用来编译出类库.jar,应用文件.apk，[动态库](https://so.csdn.net/so/search?q=%E5%8A%A8%E6%80%81%E5%BA%93&spm=1001.2101.3001.7020).so，静态库.a作用。其中关键的就是模块类型定义和不同的属性定义。

Android.bp文件用类似json的简洁声明来描述需要构建的模块。

Android.bp文件是Android 7.0及更高版本中引入的一种构建脚本文件，用于描述和管理项目的编译过程。

Android.bp文件是使用Starlark语法编写的，它是一种基于Python的轻量级脚本语言。

Android.bp文件用于定义模块和构建规则，与之前的Android.mk文件相比，更加灵活和易于维护。

对Android.mk-入门必备的详细介绍：

[https://blog.csdn.net/wenzhi20102321/article/details/135631544](https://blog.csdn.net/wenzhi20102321/article/details/135631544)

### 二、Android.bp 文件模版

#### 1、编译.jar包

```
java_library {
    name: "mylibrary",
    srcs: ["src/**/*.java"],
    static_libs: ["lib1", "lib2"],
    javac_flags: ["-source 1.8", "-target 1.8"],
    aaptflags: ["--auto-add-overlay"],
    manifest: "AndroidManifest.xml",
    resource_dirs: ["res"],
    dex_preopt: {
        enabled: true,
    },
    dex_preopt_config: "dexpreopt.config",
    dex_preopt_uses_art: true,
    dex_preopt_omit_shared_libs: true,
    optional: true,
}
```

#### 2、编译apk

##### （1）以apk编译apk？

就是给普通apk加上系统签名变成系统apk的作用。

从最新的Android13 源码看bp文件未看到与mk文件对应的编译类型。

具体可以查看：Android.mk 与 Android.bp 属性转换对应字符串关系文件：

release\\build\\soong\\androidmk\\androidmk\\android.go

下面是bp文件中编译模块类型定义

```
//mk和bp文件编译类型对应关系
var moduleTypes = map[string]string{
    "BUILD_SHARED_LIBRARY":        "cc_library_shared",
    "BUILD_STATIC_LIBRARY":        "cc_library_static",
    "BUILD_HOST_SHARED_LIBRARY":   "cc_library_host_shared",
    "BUILD_HOST_STATIC_LIBRARY":   "cc_library_host_static",
    "BUILD_HEADER_LIBRARY":        "cc_library_headers",
    "BUILD_EXECUTABLE":            "cc_binary",
    "BUILD_HOST_EXECUTABLE":       "cc_binary_host",
    "BUILD_NATIVE_TEST":           "cc_test",
    "BUILD_HOST_NATIVE_TEST":      "cc_test_host",
    "BUILD_NATIVE_BENCHMARK":      "cc_benchmark",
    "BUILD_HOST_NATIVE_BENCHMARK": "cc_benchmark_host",

    "BUILD_JAVA_LIBRARY":             "java_library_installable", // will be rewritten to java_library by bpfix
    "BUILD_STATIC_JAVA_LIBRARY":      "java_library",
    "BUILD_HOST_JAVA_LIBRARY":        "java_library_host",
    "BUILD_HOST_DALVIK_JAVA_LIBRARY": "java_library_host_dalvik",
    "BUILD_PACKAGE":                  "android_app",
    "BUILD_RRO_PACKAGE":              "runtime_resource_overlay",

    "BUILD_CTS_EXECUTABLE":          "cc_binary",               // will be further massaged by bpfix depending on the output path
    "BUILD_CTS_SUPPORT_PACKAGE":     "cts_support_package",     // will be rewritten to android_test by bpfix
    "BUILD_CTS_PACKAGE":             "cts_package",             // will be rewritten to android_test by bpfix
    "BUILD_CTS_TARGET_JAVA_LIBRARY": "cts_target_java_library", // will be rewritten to java_library by bpfix
    "BUILD_CTS_HOST_JAVA_LIBRARY":   "cts_host_java_library",   // will be rewritten to java_library_host by bpfix
}

//预编译部分
var prebuiltTypes = map[string]string{
    "SHARED_LIBRARIES": "cc_prebuilt_library_shared",
    "STATIC_LIBRARIES": "cc_prebuilt_library_static",
    "EXECUTABLES":      "cc_prebuilt_binary",
    "JAVA_LIBRARIES":   "java_import",
    "APPS":             "android_app_import",
    "ETC":              "prebuilt_etc",
}
```

前面是Android.mk中的编译类型定义，右边是Android.bp中编译模块类型的定义。

这个功能Android.mk用到 BUILD\_PREBUILT 类型，在这里未查询到可以替换的类型。

通过一定的摸索发现Android.bp可以使用 “android\_app\_import” 这个关键字的类型进行预编译apk。

编译代码示例：

```
android_app_import {
    name: "FileManager",
    apk: "FileManager.apk",
    privileged: true,
    //使用系统签名
    certificate: "platform",
    //不用覆盖签名,使用原本打包的签名，和上面的只能2选一
    //presigned:true, 

}
```

##### （2）以java源码编译apk

```
android_app {
    name: "myapp",
    srcs: [
        "path/to/your/app/src/**/*.java",
    ],
    resource_dirs: [
        "path/to/your/app/res",
    ],
    manifest: "path/to/your/app/AndroidManifest.xml",
    
    //加载类库
    static_libs: [
        "mylibrary",
    ],
    
    //是否系统签名
    certificate: "platform",
    platform_apis: true,
    //是否生成到priv-app目录
    privileged: true,
    
}
```

#### 3、编译动态库.so

```
cc_library_shared {
    name: "mylibrary",
    srcs: [
        "path/to/your/library.c",
    ],
    shared_libs: [
        "lib1",
        "lib2",
    ],
    include_dirs: [
        "path/to/your/include",
    ],
    export_include_dirs: [
        "path/to/your/export_include",
    ],
    target: {
        android: {
            shared_libs: [
                "lib3",
                "lib4",
            ],
        },
    },
}
```

上述示例中，定义了一个名为"mylibrary"的cc\_library\_shared模块，它包含了要编译的动态库源文件（library.c）。

使用srcs属性指定源文件的路径。

使用shared\_libs属性指定该动态库依赖的其他动态库（lib1、lib2）。这些依赖的库可以是系统库或其他自定义库。

使用include\_dirs属性指定用于编译的头文件目录的路径。

使用export\_include\_dirs属性指定用于在其他模块中使用该库时所需的头文件目录的路径。

通过target.android.shared\_libs属性指定在Android平台上链接该库时所需的其他系统库（lib3、lib4）。

请根据您的实际项目路径和配置需求，修改和配置相关路径和属性，确保与您的项目一致。

#### 4、编译[静态库](https://so.csdn.net/so/search?q=%E9%9D%99%E6%80%81%E5%BA%93&spm=1001.2101.3001.7020).a

```
cc_library_static {
    name: "mylibrary",
    srcs: [
        "path/to/your/library.c",
    ],
    include_dirs: [
        "path/to/your/include",
    ],
    export_include_dirs: [
        "path/to/your/export_include",
    ],
}
```

上述示例中，定义了一个名为"mylibrary"的cc\_library\_static模块，它包含了要编译的静态库源文件（library.c）。

使用srcs属性指定源文件的路径。

使用include\_dirs属性指定用于编译的头文件目录的路径。

使用export\_include\_dirs属性指定用于在其他模块中使用该库时所需的头文件目录的路径。

请根据您的实际项目路径和配置需求，修改和配置相关路径和属性，确保与您的项目一致。

#### 5、Android.mk 编译文件小结

编译模版：

```
BuildType { //（1）编译类型
    name: "mylibrary", // （2）模块名称
    srcs: [
        "path/XXX", //（2）编译的源码
    ],
    //根据类型设置不同的属性和不同的值
}

BuildType2 { //（1）编译类型
    name: "mylibrary2", // （2）模块名称
    srcs: [
        "path/XXX", //（2）编译的源码
    ],
}
。。。//同一个bp文件可以编译多个模块。
```

所有的Android.bp都是基于上面的模版进行适配的。

编译某个类型模块，中间添加属性定义和相关值即可，属性的设置使用就是冒号 “:”,

代码注释，同Java代码一样，使用// 或者 /\*\* 注释 \*\*/ 表示注释标识的。

在实际的bp文件中可能有几百行，这一般是多个模块编译的情况，

不用头晕，只看自己相关模块那段代码，

每个模块都有一个模块名称 name，找到开始和结束的部分适配修改即可。

#### 编译类型的BuildType总结：

```
编码类型和Android.mk和Android.bp中对应的关键字
1、apk文件,mk：BUILD_PREBUILT，bp:android_app_import（与之前不是对应关系！）
2、app代码,mk：BUILD_PACKAGE，bp:android_app
3、动态库,mk：BUILD_SHARED_LIBRARY，bp:cc_library_shared
4、静态库,mk：BUILD_STATIC_LIBRARY，bp:cc_library_static
5、代码编译成Jar包,mk：BUILD_JAVA_LIBRARY，bp:java_library_installable
6、jar包编译到系统,mk：BUILD_MULTI_PREBUILT，bp:java_import（与之前不是对应关系！）
```

其他的编译类型可以看android.go文件查看。

#### Android系统源码编译Android.bp文件方式：

（1）在源码release目录，输入 make -j64 “name名称”

（2） cd 到Android.bp模块目录，输入"mm"

### 三、Android.bp 具体示例

```

//（1）预编译jar包，加载jar包
java_import {
  name: "libs",
  jars: ["app/libs/rxjava.jar", "app/libs/rxandroid.jar"","app/libs/zxing.jar"],
}


android_app {

    name: "SkgSettings",
    platform_apis: true,
    privileged: true,

    kotlincflags: ["-Xjvm-default=enable"],
    optimize: {
        proguard_flags_files: ["app/src/main/proguard.flags"],
    },



    libs: [
       // Soong fails to automatically add this dependency because all the
       // *.kt sources are inside a filegroup.
       "kotlin-annotations",
    ],

    static_libs: [
        "androidx.appcompat_appcompat",
        "kotlin-stdlib",
        "libs",
    ],

    manifest: "app/src/main/AndroidManifest.xml",

    srcs: ["app/src/main/java/**/*.java",
           "app/src/main/java/**/*.kt",
       "SystemUpdaterSDK/src/main/java/**/*.java",
       "SystemUpdaterSDK/src/main/aidl/**/*.aidl"],
    aidl: {
        local_include_dirs: ["SystemUpdaterSDK/src/main/aidl/"],
    },
    resource_dirs: ["app/src/main/res"],

}

```

### 四、Android.bp 主要属性

以下是一些常见的预定义属性（以下没有定义模块属性）：

1、name：定义模块的名称，通常是唯一标识符。

```
name: "my_module",
```

2、 srcs：指定模块的源文件，可以是一个文件列表。

```
srcs: ["file1.java", "file2.java"],
```

3、deps：指定模块的依赖关系，即依赖于其他模块的模块列表。

```
deps: ["dependency_module1", "dependency_module2"],
```

4、visibility：指定模块的可见性，确定哪些模块可以访问它。

```
visibility: ["//my/module:visible_module"],
```

5、cflags、cppflags、ldflags：用于指定C/C++编译和链接的标志。

```
cflags: ["-Wall", "-O2"],
cppflags: ["-DDEBUG"],
ldflags: ["-L/path/to/lib", "-lmylib"],
```

6、shared\_libs、static\_libs：指定模块的动态链接库和静态链接库的依赖关系。

```
shared_libs: ["lib1", "lib2"],  //编译依赖的动态库lib1和lib2
static_libs: ["lib3", "lib4"],  //编译依赖的静态库lib3和lib4
```

7、host\_supported、device\_supported：指定模块是否支持主机构建和目标设备构建。

```
host_supported: true,
device_supported: true,
```

8、installable：指定模块是否可以被安装到系统镜像中。

```
installable: true,
```

9、product\_specific: 指定编译出来放在/product/目录下(默认是放在/system目录下)

```
product_specific: true
```

10、vendor: 指定编译出来放在/vendor/目录下(默认是放在/system目录下)

```
vendor: true, 
```

这些是Android.bp文件中一些常见的预制属性。每个属性用于不同的目的，开发者可以根据模块的类型和需求来使用它们。此外，Android构建系统还支持许多其他属性，这些属性可以根据具体的构建任务和模块类型进行自定义。

有关更多属性和其详细说明，请参阅Android构建系统的官方文档:

[https://source.android.google.cn/docs/setup/build?hl=zh-cn](https://source.android.google.cn/docs/setup/build?hl=zh-cn)

### 五、总结

#### 1、Android.bp 的简单使用总结

```
BuildType { //（1）编译类型
    name: "mylibrary", // （2）模块名称
    srcs: [
        "path/XXX", //（2）编译的源码
    ],
    //根据类型设置不同的属性和不同的值
}
```

关键就是编译类型的确定和各属性的定义。

注释使用和Java代码一样，使用双斜线//或者/\*\* 注释\*\*/

#### 2、Android.bp 详解

下面是Android.bp一些比较官方的介绍，有兴趣可以详情看看。

Android.bp文件是Android 7.0及更高版本中引入的一种构建脚本文件，用于描述和管理项目的编译过程。下面是Android.bp文件的一些详解：

概述：Android.bp文件是使用Starlark语法编写的，它是一种基于Python的轻量级脚本语言。Android.bp文件用于定义模块和构建规则，与之前的Android.mk文件相比，更加灵活和易于维护。

模块定义：Android.bp文件中可以定义多个模块，每个模块都有一个唯一的模块名。模块可以是可执行文件、静态库、共享库等。通过cc\_library、cc\_binary等函数来定义模块。

源文件定义：Android.bp文件中使用srcs参数来指定模块的源文件。可以使用通配符来匹配多个文件，如\[“\*.cpp”\]表示所有的cpp文件。

编译选项：Android.bp文件中可以指定编译选项，如指定编译器、编译标志等。通过cflags、cppflags、ldflags等参数来设置。

依赖关系：Android.bp文件中可以指定模块的依赖关系，即一个模块依赖于其他模块。通过shared\_libs、static\_libs等参数来指定依赖的共享库或静态库。

目标文件生成：Android.bp文件中可以指定生成的目标文件的名称和路径。通过name、installable等参数来设置。

其他功能：Android.bp文件还支持其他一些功能，如指定需要编译的源文件、排除某些源文件、指定编译器、链接器等。可以通过查阅Android.bp文件的官方文档或相关教程来了解更多功能和用法。

总之，Android.bp文件是Android 7.0及更高版本中引入的一种构建脚本文件，用于管理项目的编译过程。通过编写Android.bp文件，可以定义模块的编译选项、依赖关系和生成的目标文件等。这样可以更灵活地管理和组织项目的代码和资源。

#### 3、Android.bp的其他知识

##### （1）Android所有bp属性和mk属性的对照关系完整文件：

系统源码目录：build/soong/androidmk/androidmk/android.go

[http://aospxref.com/android-13.0.0\_r3/xref/build/soong/androidmk/androidmk/android.go](http://aospxref.com/android-13.0.0_r3/xref/build/soong/androidmk/androidmk/android.go)

##### （2）Android.bp文件编译的模块类型和属性定义字符串都是大写的

Android.mk 文件编译的模块类型和属性定义都是大写的。

示例对比如下：

```
//Android.mk 示例
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
# 设置模块名或者apk名称
LOCAL_MODULE := mylibrary
#根据类型设置不同的属性和不同的值
...

//Android.bp 示例
BuildType { //（1）编译类型
    name: "mylibrary", // （2）模块名称
    srcs: [
        "path/XXX", //（2）编译的源码
    ],
    ...//根据类型设置不同的属性和不同的值
}


```

##### （3）其他

总的来说，这是一篇Android.bp入门级别和相关内容比较全面介绍。

具体的开发中，可能比上面的复杂，比如动态库的编译那些差别是很大的。

之前编译复杂的动态库，依赖的东西太多，会出现非常多问题，

可能会用到一些不常用的属性，并且有些属性在网上也查不到，官方也没有介绍！

如果是系统源码编译出现这个情况，可以全局搜索整个系统的代码，可以参考其他模块使用这个属性的具体定义和值。比如上面的预编译apk的情况，百度和普通搜索可能是无法搜索到相关内容的，通过关键字全局搜索package目录下的代码，就能找到相关类型和具体参数设置使用进行参考。
