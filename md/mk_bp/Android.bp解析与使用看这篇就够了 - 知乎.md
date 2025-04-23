---
created: 2025-04-23T10:26:53 (UTC +08:00)
tags: [Android 开发,Android 开发入门,Android]
source: https://zhuanlan.zhihu.com/p/680171333
author: 关于作者Aliff即时通讯，软件设计与开发者，物联网，移动互联网，AIVRVI兴趣者回答89文章161关注者1,291关注他发私信
---

# Android.bp解析与使用看这篇就够了 - 知乎

> ## Excerpt
> Android.bp解析与使用看这篇就够了写在前面一、概念1、Android.bp和Androd.mk区别以及宏变量对应关系2、Android.mk转换Android.bp2.1、Android.mk自动转换Android.bp2.2 Android.mk手动转换Android.bp二、Android.…

---
-   [Android.bp解析与使用看这篇就够了](https://zhuanlan.zhihu.com/p/680171333/edit#androidbp%E8%A7%A3%E6%9E%90%E4%B8%8E%E4%BD%BF%E7%94%A8%E7%9C%8B%E8%BF%99%E7%AF%87%E5%B0%B1%E5%A4%9F%E4%BA%86)
-   [写在前面](https://zhuanlan.zhihu.com/p/680171333/edit#%E5%86%99%E5%9C%A8%E5%89%8D%E9%9D%A2)
-   [一、概念](https://zhuanlan.zhihu.com/p/680171333/edit#%E4%B8%80%E6%A6%82%E5%BF%B5)
-   [1、Android.bp和Androd.mk区别以及宏变量对应关系](https://zhuanlan.zhihu.com/p/680171333/edit#1androidbp%E5%92%8Candrodmk%E5%8C%BA%E5%88%AB%E4%BB%A5%E5%8F%8A%E5%AE%8F%E5%8F%98%E9%87%8F%E5%AF%B9%E5%BA%94%E5%85%B3%E7%B3%BB)
-   [2、Android.mk转换Android.bp](https://zhuanlan.zhihu.com/p/680171333/edit#2androidmk%E8%BD%AC%E6%8D%A2androidbp)

-   [2.1、Android.mk自动转换Android.bp](https://zhuanlan.zhihu.com/p/680171333/edit#21androidmk%E8%87%AA%E5%8A%A8%E8%BD%AC%E6%8D%A2androidbp)
-   [2.2 Android.mk手动转换Android.bp](https://zhuanlan.zhihu.com/p/680171333/edit#22-androidmk%E6%89%8B%E5%8A%A8%E8%BD%AC%E6%8D%A2androidbp)

-   [二、Android.bp详细解析](https://zhuanlan.zhihu.com/p/680171333/edit#%E4%BA%8Candroidbp%E8%AF%A6%E7%BB%86%E8%A7%A3%E6%9E%90)
-   [1、几个常用的库函数](https://zhuanlan.zhihu.com/p/680171333/edit#1%E5%87%A0%E4%B8%AA%E5%B8%B8%E7%94%A8%E7%9A%84%E5%BA%93%E5%87%BD%E6%95%B0)
-   [1、编译不同类型的模块](https://zhuanlan.zhihu.com/p/680171333/edit#1%E7%BC%96%E8%AF%91%E4%B8%8D%E5%90%8C%E7%B1%BB%E5%9E%8B%E7%9A%84%E6%A8%A1%E5%9D%97)

-   [1.1、编译成Java库](https://zhuanlan.zhihu.com/p/680171333/edit#11%E7%BC%96%E8%AF%91%E6%88%90java%E5%BA%93)
-   [1.2、编译成Java静态库](https://zhuanlan.zhihu.com/p/680171333/edit#12%E7%BC%96%E8%AF%91%E6%88%90java%E9%9D%99%E6%80%81%E5%BA%93)
-   [1.3、编译成App应用](https://zhuanlan.zhihu.com/p/680171333/edit#13%E7%BC%96%E8%AF%91%E6%88%90app%E5%BA%94%E7%94%A8)
-   [1.4、编译成Native动态库](https://zhuanlan.zhihu.com/p/680171333/edit#14%E7%BC%96%E8%AF%91%E6%88%90native%E5%8A%A8%E6%80%81%E5%BA%93)
-   [1.5、编译成Native静态库](https://zhuanlan.zhihu.com/p/680171333/edit#15%E7%BC%96%E8%AF%91%E6%88%90native%E9%9D%99%E6%80%81%E5%BA%93)
-   [1.6、编译成Native执行程序](https://zhuanlan.zhihu.com/p/680171333/edit#16%E7%BC%96%E8%AF%91%E6%88%90native%E6%89%A7%E8%A1%8C%E7%A8%8B%E5%BA%8F)
-   [1.7、编译成头文件库](https://zhuanlan.zhihu.com/p/680171333/edit#17%E7%BC%96%E8%AF%91%E6%88%90%E5%A4%B4%E6%96%87%E4%BB%B6%E5%BA%93)

-   [2、文件路径](https://zhuanlan.zhihu.com/p/680171333/edit#2%E6%96%87%E4%BB%B6%E8%B7%AF%E5%BE%84)

-   [2.1、本地头文件路径](https://zhuanlan.zhihu.com/p/680171333/edit#21%E6%9C%AC%E5%9C%B0%E5%A4%B4%E6%96%87%E4%BB%B6%E8%B7%AF%E5%BE%84)
-   [2.2、导出的头文件路径](https://zhuanlan.zhihu.com/p/680171333/edit#22%E5%AF%BC%E5%87%BA%E7%9A%84%E5%A4%B4%E6%96%87%E4%BB%B6%E8%B7%AF%E5%BE%84)
-   [2.3、资源文件路径](https://zhuanlan.zhihu.com/p/680171333/edit#23%E8%B5%84%E6%BA%90%E6%96%87%E4%BB%B6%E8%B7%AF%E5%BE%84)

-   [3、库依赖](https://zhuanlan.zhihu.com/p/680171333/edit#3%E5%BA%93%E4%BE%9D%E8%B5%96)

-   [3.1、依赖的静态库](https://zhuanlan.zhihu.com/p/680171333/edit#31%E4%BE%9D%E8%B5%96%E7%9A%84%E9%9D%99%E6%80%81%E5%BA%93)
-   [3.2、依赖的动态库](https://zhuanlan.zhihu.com/p/680171333/edit#32%E4%BE%9D%E8%B5%96%E7%9A%84%E5%8A%A8%E6%80%81%E5%BA%93)
-   [3.3、依赖的头文件库](https://zhuanlan.zhihu.com/p/680171333/edit#33%E4%BE%9D%E8%B5%96%E7%9A%84%E5%A4%B4%E6%96%87%E4%BB%B6%E5%BA%93)
-   [3.4、依赖的Java库](https://zhuanlan.zhihu.com/p/680171333/edit#34%E4%BE%9D%E8%B5%96%E7%9A%84java%E5%BA%93)

-   [4、安装到不同分区中](https://zhuanlan.zhihu.com/p/680171333/edit#4%E5%AE%89%E8%A3%85%E5%88%B0%E4%B8%8D%E5%90%8C%E5%88%86%E5%8C%BA%E4%B8%AD)

-   [4.1、安装到vendor中](https://zhuanlan.zhihu.com/p/680171333/edit#41%E5%AE%89%E8%A3%85%E5%88%B0vendor%E4%B8%AD)
-   [4.2、安装到product中](https://zhuanlan.zhihu.com/p/680171333/edit#42%E5%AE%89%E8%A3%85%E5%88%B0product%E4%B8%AD)
-   [4.3、安装到odm中](https://zhuanlan.zhihu.com/p/680171333/edit#43%E5%AE%89%E8%A3%85%E5%88%B0odm%E4%B8%AD)

-   [5、编译参数](https://zhuanlan.zhihu.com/p/680171333/edit#5%E7%BC%96%E8%AF%91%E5%8F%82%E6%95%B0)

-   [5.1、C flags](https://zhuanlan.zhihu.com/p/680171333/edit#51c-flags)
-   [5.2、Cpp flags](https://zhuanlan.zhihu.com/p/680171333/edit#52cpp-flags)
-   [5.3、Java flags](https://zhuanlan.zhihu.com/p/680171333/edit#53java-flags)

-   [三、Android.bp案例实战](https://zhuanlan.zhihu.com/p/680171333/edit#%E4%B8%89androidbp%E6%A1%88%E4%BE%8B%E5%AE%9E%E6%88%98)
-   [1、Android.bp 文件中引入aar](https://zhuanlan.zhihu.com/p/680171333/edit#1androidbp-%E6%96%87%E4%BB%B6%E4%B8%AD%E5%BC%95%E5%85%A5aar)
-   [2、编译APK](https://zhuanlan.zhihu.com/p/680171333/edit#2%E7%BC%96%E8%AF%91apk)

-   [2.1、编译含有源码的APK](https://zhuanlan.zhihu.com/p/680171333/edit#21%E7%BC%96%E8%AF%91%E5%90%AB%E6%9C%89%E6%BA%90%E7%A0%81%E7%9A%84apk)

-   [3、引入 so](https://zhuanlan.zhihu.com/p/680171333/edit#3%E5%BC%95%E5%85%A5-so)
-   [4、一个完整的包含 aar/jar/so Android.bp](https://zhuanlan.zhihu.com/p/680171333/edit#4%E4%B8%80%E4%B8%AA%E5%AE%8C%E6%95%B4%E7%9A%84%E5%8C%85%E5%90%AB-aarjarso-androidbp)

-   [4.1、libs](https://zhuanlan.zhihu.com/p/680171333/edit#41libs)
-   [4.2、libs/Android.bp](https://zhuanlan.zhihu.com/p/680171333/edit#42libsandroidbp)
-   [4.3、模块根路径Android.bp](https://zhuanlan.zhihu.com/p/680171333/edit#43%E6%A8%A1%E5%9D%97%E6%A0%B9%E8%B7%AF%E5%BE%84androidbp)

-   [5、其它](https://zhuanlan.zhihu.com/p/680171333/edit#5%E5%85%B6%E5%AE%83)
-   [四、AOSP编译错误汇总](https://zhuanlan.zhihu.com/p/680171333/edit#%E5%9B%9Baosp%E7%BC%96%E8%AF%91%E9%94%99%E8%AF%AF%E6%B1%87%E6%80%BB)
-   href="[https://zhuanlan.zhihu.com/p/680171333/edit#1%E9%87%8D%E8%A6%81%E7%9A%84%E6%B3%A8%E6%84%8F%E4%BA%8B%E9%A1%B9androidmk%E5%8F%AF%E4%BB%A5%E5%BC%95%E7%94%A8androidbp%E4%B8%AD%E7%9A%84%E6%A8%A1%E5%9D%97%E5%8F%8D%E4%B9%8Bandroidbp%E4%B8%8D%E8%83%BD%E5%BC%95%E7%94%A8androidmk%E4%B8%AD%E7%9A%84%E6%A8%A1%E5%9D%97](https://zhuanlan.zhihu.com/p/680171333/edit#1%E9%87%8D%E8%A6%81%E7%9A%84%E6%B3%A8%E6%84%8F%E4%BA%8B%E9%A1%B9androidmk%E5%8F%AF%E4%BB%A5%E5%BC%95%E7%94%A8androidbp%E4%B8%AD%E7%9A%84%E6%A8%A1%E5%9D%97%E5%8F%8D%E4%B9%8Bandroidbp%E4%B8%8D%E8%83%BD%E5%BC%95%E7%94%A8androidmk%E4%B8%AD%E7%9A%84%E6%A8%A1%E5%9D%97)">1、重要的注意事项，Android.mk可以引用Android.bp中的模块，反之Android.bp不能引用Android.mk中的模块
-   href="[https://zhuanlan.zhihu.com/p/680171333/edit#2androidbp%E6%A8%A1%E5%9D%97%E4%B8%8D%E6%94%AF%E6%8C%81%E5%8E%BB%E5%AF%BB%E6%89%BE%E4%B8%8A%E5%B1%82%E8%B7%AF%E5%BE%84%E7%9A%84%E6%96%87%E4%BB%B6%E5%8F%AA%E6%94%AF%E6%8C%81%E6%9C%AC%E7%9B%AE%E5%BD%95%E4%B8%8B%E7%9A%84folder%E9%87%8C%E9%9D%A2%E7%9A%84%E6%96%87%E4%BB%B6%E5%A6%82libsbgwanaar](https://zhuanlan.zhihu.com/p/680171333/edit#2androidbp%E6%A8%A1%E5%9D%97%E4%B8%8D%E6%94%AF%E6%8C%81%E5%8E%BB%E5%AF%BB%E6%89%BE%E4%B8%8A%E5%B1%82%E8%B7%AF%E5%BE%84%E7%9A%84%E6%96%87%E4%BB%B6%E5%8F%AA%E6%94%AF%E6%8C%81%E6%9C%AC%E7%9B%AE%E5%BD%95%E4%B8%8B%E7%9A%84folder%E9%87%8C%E9%9D%A2%E7%9A%84%E6%96%87%E4%BB%B6%E5%A6%82libsbgwanaar)">2、Android.bp模块，不支持../../去寻找上层路径的文件，只支持本目录下的folder里面的文件，如libs/Bgwan.aar
-   [3、unkonw type namespace](https://zhuanlan.zhihu.com/p/680171333/edit#3unkonw-type-namespace)
-   [4、can not link against](https://zhuanlan.zhihu.com/p/680171333/edit#4can-not-link-against)
-   [5、missing dependencies](https://zhuanlan.zhihu.com/p/680171333/edit#5missing-dependencies)
-   [6、Compilation can’t be completed because some library classes are missing](https://zhuanlan.zhihu.com/p/680171333/edit#6compilation-cant-be-completed-because-some-library-classes-are-missing)
-   [7、编译时会提示找不到资源，运行时会报错](https://zhuanlan.zhihu.com/p/680171333/edit#7%E7%BC%96%E8%AF%91%E6%97%B6%E4%BC%9A%E6%8F%90%E7%A4%BA%E6%89%BE%E4%B8%8D%E5%88%B0%E8%B5%84%E6%BA%90%E8%BF%90%E8%A1%8C%E6%97%B6%E4%BC%9A%E6%8A%A5%E9%94%99)
-   [五、附录：mk与bp映射表](https://zhuanlan.zhihu.com/p/680171333/edit#%E4%BA%94%E9%99%84%E5%BD%95mk%E4%B8%8Ebp%E6%98%A0%E5%B0%84%E8%A1%A8)
-   [致谢（引用和推荐）（可选）](https://zhuanlan.zhihu.com/p/680171333/edit#%E8%87%B4%E8%B0%A2%E5%BC%95%E7%94%A8%E5%92%8C%E6%8E%A8%E8%8D%90%E5%8F%AF%E9%80%89)
-   [@©LICENSE（版权和更新记录）](https://zhuanlan.zhihu.com/p/680171333/edit#%C2%A9license%E7%89%88%E6%9D%83%E5%92%8C%E6%9B%B4%E6%96%B0%E8%AE%B0%E5%BD%95)

![](https://pic1.zhimg.com/v2-1d90d773d6ebb67ad1556ed6e96d429e_1440w.jpg)

Android.bp解析与使用看这篇就够了

背景图来源：<科技：科幻背景[(点我跳转)](https://link.zhihu.com/?target=https%3A//616pic.com/sucai/1xgf7r0nz.html)\>

_争取每一篇文章都是精华，每一篇文章都能做到后期维护，_若本文存在排版问题，可通过本人唯一 **〖阿里云地址[(点我跳转)](https://link.zhihu.com/?target=https%3A//bgwan.oss-cn-shanghai.aliyuncs.com/sunst0069/Company_Car/Framework/Android.bp%25E8%25A7%25A3%25E6%259E%2590%25E4%25B8%258E%25E4%25BD%25BF%25E7%2594%25A8%25E7%259C%258B%25E8%25BF%2599%25E7%25AF%2587%25E5%25B0%25B1%25E5%25A4%259F%25E4%25BA%2586.html)**查看

## 写在前面

如果要理解Android.bp，还是需要对Android.mk有一定理解，好在我在前面一篇内容<**Android.mk解析与使用看这篇就够了[(点我跳转)](https://zhuanlan.zhihu.com/p/680173022)**\>中进行了详细分析

## 一、概念

随着 Android 越来越庞大，module 越来越多，编译时间也越来越久，而使用ninja在编译并发处理上较 make 有很大的提升。[Ninja](https://zhida.zhihu.com/search?content_id=239279048&content_type=Article&match_order=1&q=Ninja&zhida_source=entity) 的配置文件就是Android.bp

![](https://pic1.zhimg.com/Makefile_1440w.jpg)

-   ninja是一个编译框架，根据相应的ninja格式的配置文件进行编译，但是ninja文件一般不会手动修改，而是通过将 Android.bp文件转换成ninja格式文件来编译
-   [Soong](https://zhida.zhihu.com/search?content_id=239279048&content_type=Article&match_order=1&q=Soong&zhida_source=entity)类似于之前的Makefile编译系统的核心，负责提供Android.bp语义解析，并将之转换成Ninja文件。
-   [Blueprint](https://zhida.zhihu.com/search?content_id=239279048&content_type=Article&match_order=1&q=Blueprint&zhida_source=entity)是生成、解析Android.bp的工具，是Soong的一部分。Blueprint只是解析文件格式，Soong解析内容的具体含义 Blueprint和Soong都是由[Golang](https://zhida.zhihu.com/search?content_id=239279048&content_type=Article&match_order=1&q=Golang&zhida_source=entity)写的项目 从Android 7.0，prebuilts/go/目录下新增Golang所需的运行环境，在编译时使用  
    
-   [kati](https://zhida.zhihu.com/search?content_id=239279048&content_type=Article&match_order=1&q=kati&zhida_source=entity)是为了兼容老的mk配置文件，专为Android开发的一个基于Golang和C++的工具，主要功能是把Android.mk文件转换成Ninja文件。代码路径是build/kati/，编译后的产物是ckati

**⚠️ 注意**

> 目前Android 10（Android Q）里边，还是支持Android.mk方式。但是相信将来的版本中，以mk文件编辑的方式会成为历史，同时Kati也会被淘汰，只保留bp配置方式

### 1、Android.bp和Androd.mk区别以及宏变量对应关系

> Android.bp --> Soong --> Ninja Makefile or Android.mk --> kati --> Ninja

最终都是生成Ninja格式文件进行编译。2023-08-29在实际案例中，其实bp中支持条件判断的宏定义（可以不用go语言实现，但稍微复杂一些）

### 2、Android.mk转换Android.bp

Android.bp的出现就是为了替换Android.mk文件。bp跟mk文件不同，它是纯粹的配置，没有分支、循环等流程控制，不能做算数逻辑运算。如果需要控制逻辑，那么只能通过Go语言编写

旧的mk可以转换为bp，Soong会编译生成一个androidmk命令，用于将Android.mk文件转换为Android.bp文件。即：

> Android.mk --> Soong中androidmk --> Android.bp

如果是Android.mk有分支或循环等流程控制那就没办法做到Android.mk自动转Android.bp，那么就需要进行手动转换

### 2.1、Android.mk自动转换Android.bp

-   在工程源码中：

```
a. source build/envsetup.sh
b. lunch xxx
c. make androidmk
```

生成androidmk转换工具，路径为：/out/soong/host/linux-x86/bin/androidmk \* 直接把你要转换的Android.mk 文件放置到此目录下，然后执行命令：

> androidmk Android.mk > Android.bp

### 2.2 Android.mk手动转换Android.bp

要想会手动转换，那就必须知道它们之间变量名的对应关系

源码路径：附录mk与bp映射表**[(点我跳转)](https://link.zhihu.com/?target=https%3A//cs.android.com/android/platform/superproject/main/%2B/main%3Abuild/soong/androidmk/androidmk/android.go)**

**⚠️ 注意**

> 在后面《五、附录：mk与bp映射表》单独说明

为了便于理解，把Android.mk和 Android.bp的语法放在一起说明。再次提醒前面一篇内容<**Android.mk解析与使用看这篇就够了[(点我跳转)](https://zhuanlan.zhihu.com/p/22242264/)**\>本神进行了详细分析，如果不理解Andoid.mk的可以先了解后再回来阅读本内容

### 1、几个常用的库函数

-   java\_library 会把aidl java 等文件编译成 .jar 库
-   android\_library 会把 xml 资源文件， aidl java 等文件 编译成 .aar 库
-   java\_import 预编译 .jar 库 （引用 第三方 jar 库）
-   [android\_library\_import](https://zhida.zhihu.com/search?content_id=239279048&content_type=Article&match_order=1&q=android_library_import&zhida_source=entity) 这是预编译 .aar 库 (引用第三方aar库)
-   [android\_app\_import](https://zhida.zhihu.com/search?content_id=239279048&content_type=Article&match_order=1&q=android_app_import&zhida_source=entity) 这是 预编译 apk，相当于 BUILD\_PREBUILT
-   android\_app 编译成apk，相当于 BUILD\_PACKAGE

默认模块可用于在多个模块中重复使用相同的属性 :

```
defaults  
cc_defaults
java_defaults
如：
cc_defaults {
    name: "gzip_defaults",
    shared_libs: ["libz"],
    stl: "none",
}
cc_binary {
    name: "gzip",
    **defaults**: ["gzip_defaults"],
    srcs: ["src/test/minigzip.c"],
}

hostdex: true                for hiddenapi check . Hostdex is only for ART testing on host: 

privileged: true,    // this needs to be a privileged application
dex_preopt: {     //Make sure the build system doesn't try to resign the APK
       enabled: false,
},
```

### 1、编译不同类型的模块

### 1.1、编译成Java库

会把aidl java 等文件编译成 .jar 库

```
Android.mk
include $(BUILD_JAVA_LIBRARY)

Android.bp
java_library {
......
}
```

### 1.2、编译成Java静态库

```
Android.mk
include $(BUILD_STATIC_JAVA_LIBRARY)

Android.bp
java_library_static {
......
}
```

### 1.3、编译成App应用

```
Android.mk
include $(BUILD_PACKAGE)

Android.bp
android_app {
......
}
```

### 1.4、编译成Native动态库

```
Android.mk
include $(BUILD_SHARED_LIBRARY)

Android.bp
cc_library_shared {
......
}
```

### 1.5、编译成Native静态库

```
Android.mk
include $(BUILD_STATIC_LIBRARY)

Android.bp
cc_library_static {
......
}
```

### 1.6、编译成Native执行程序

```
Android.mk
include $(BUILD_EXECUTABLE)

Android.bp
cc_binary {
......
}
```

### 1.7、编译成头文件库

```
Android.mk
include $(BUILD_HEADER_LIBRARY)

Android.bp
cc_library_headers {
......
}
```

### 2、文件路径

### 2.1、本地头文件路径

```
Android.mk
LOCAL_C_INCLUDES := 

Android.bp
local_include_dirs: ["xxx", ...]
```

### 2.2、导出的头文件路径

```
Android.mk
LOCAL_EXPORT_C_INCLUDE_DIRS := 

Android.bp
export_include_dirs: ["xxx", ...]
```

### 2.3、资源文件路径

```
Android.mk
LOCAL_RESOURCE_DIR := 

Android.bp
resource_dirs: ["xxx", ...]
```

### 3、库依赖

### 3.1、依赖的静态库

```
Android.mk
LOCAL_STATIC_LIBRARIES := 

Android.bp
static_libs: ["xxx", "xxx", ...]
```

### 3.2、依赖的动态库

```
Android.mk
LOCAL_SHARED_LIBRARIES := 

Android.bp
shared_libs: ["xxx", "xxx", ...]
```

### 3.3、依赖的头文件库

```
Android.mk
LOCAL_HEADER_LIBRARIES := 

Android.bp
header_libs: ["xxx", "xxx", ...]
```

### 3.4、依赖的Java库

```
Android.mk
LOCAL_STATIC_JAVA_LIBRARIES := 

Android.bp
static_libs: ["xxx", "xxx", ...]
```

### 4、安装到不同分区中

### 4.1、安装到vendor中

```
Android.mk
LOCAL_VENDOR_MODULE := true
        or
LOCAL_PROPRIETARY_MODULE := true

Android.bp
proprietary: true
    or
vendor: true
```

**⚠️ 注意**

> LOCAL\_PROPRIETARY\_MODUL，true控制生成路径到vendor/lib，false就是system/lib LOCAL\_CLANG，clang：true来指定默认编译器为Clang，Android 8.0后不需要指定，默认是Clang

[版权声明CopyRight：](https://zhuanlan.zhihu.com/p/80668416)

> 本内容作者：sunst0069，转载或引用请[标明出处](https://www.zhihu.com/people/qydq)，违者追究法律责任！！！

### 4.2、安装到product中

```
Android.mk
LOCAL_PRODUCT_MODULE := true

Android.bp
product_specific: true
```

### 4.3、安装到odm中

```
Android.mk
LOCAL_ODM_MODULE := true

Android.bp
device_specific: true
```

### 5、编译参数

### 5.1、C flags

```
Android.mk
LOCAL_CFLAGS := 

Android.bp
cflags: ["xxx", "xxx", ...]
```

### 5.2、Cpp flags

```
Android.mk
LOCAL_CPPFLAGS := 

Android.bp
cppflags: ["xxx", "xxx", ...]
```

### 5.3、Java flags

```
Android.mk
LOCAL_JAVACFLAGS := 

Android.bp
javacflags: ["xxx", "xxx", ...]
```

未完待续。。。

## 三、Android.bp案例实战

**项目目录结构**

为了便于内容理解，新增本内容；该目录结构同样适用于本神的<**Android.mk解析与使用看这篇就够了[(点我跳转)](https://zhuanlan.zhihu.com/p/22242264/)**\>这篇文章

```
Android.bp
 AndroidManifest.xml
 Android.mk
 assets
 libs
 res
 src
```

aar包 li.aar aar包 ：ba.aar 三方jar包 audioeffectservice.jar

**⚠️ 注意**

-   **Android应用级项目中目录结构是有区别的，应用级APP目录结构如下图**

![](https://pic4.zhimg.com/v2-d90b68fb7756e7727790c0d080420fb5_1440w.jpg)

-   li,ba为个人[livery框架](https://zhuanlan.zhihu.com/p/599703996)aar库（这里只是示例作用，并不适用于车载或其它智能设备）
-   audioeffectservice为我们公司的一个恢复音效的jar

### 1、Android.bp 文件中引入aar

```
android_library_import {  // 预编译 aar 包
   name: "phglib",
   aars: ["libs/phg_lib.aar"],
   sdk_version: "current",
}

java_import {  // 预编译 jar 包
    name: "phgtest",
    jars: ["libs/phg_test.jar"],
    sdk_version: "current",
}

android_app {
    name: "PHG_APP",
    certificate: "platform",

    srcs: ["src/**/*.java"],

    static_libs: [
        "phglib",  // 引用 aar 包
        "phgtest", //引用 jar 包
        "android-support-v7-appcompat",
        "android-support-constraint-layout",
        "android-support-design",        
        "android.hidl.base-V1.0-java",
                "lib-lottie",
        "android-support-annotations",
        "android-support-compat",
        "android-support-core-ui",
        "androidx.tvprovider_tvprovider",
        "android-support-v4",
    ],

    resource_dirs: ["res"],
    asset_dirs: ["assets"],

    optimize: {  // 有时候编译会出错，需要加这个变量
        enabled: false,
    },

    dex_preopt: {  // 有时候编译会出错，需要加这个变量
        enabled: false,
    },

    platform_apis: true,

    aaptflags: [
       "--auto-add-overlay",
       "--extra-packages",  
       "com.test.phg.service", // 这里要加上 aar 包里面的包名，可以多个
    ],
}
```

### 2、编译APK

如我们公司的一个恢复音效的模块

```
android_library_import {
   name: "readytuneclientapi",
   aars: ["readytuneclientapi/readytuneclientapi.aar"],
   sdk_version: "current",
}

android_app_import {
    name: "ReadyTuneApp",
    apk: "ReadyTuneApp.apk",
    certificate: "platform",
    privileged: true,

    dex_preopt: {
        enabled: false,
    },
    required: [
         "privapp-permissions-com.harman.ode.readytune.xml",
         "android.harman.caraudio.readytune.xml"
     ],
}

prebuilt_etc {
    name: "privapp-permissions-com.harman.ode.readytune.xml",
    src: "xml/privapp-permissions-com.harman.ode.readytune.xml",
    sub_dir: "permissions",
}
prebuilt_etc {
     name: "android.harman.caraudio.readytune.xml",
     sub_dir: "permissions",
     src: "xml/android.harman.caraudio.readytune.xml",
}
```

### 2.1、编译含有源码的APK

src/Android.bp

```
android_library_import {
   name: "readytuneclientapi",
   aars: ["libs/readytuneclientapi.aar"],
   sdk_version: "current",
}

java_library {
    name: "mediamodeservice_lastmediasource",
    srcs: ["main/java/com/harman/mediamodeservice/lastmediasource/**/*.java"],
    libs: ["android.car"],
    static_libs: [
        "mediamodeservice_utility",
        "mediamodeservice_carserviceproxy",
        "mediamodeservice_persistence"
    ],
        android_library {
         "dagger2-2.19",
         "jsr330"
         "jsr330",
         "testlib_jar",
     ],
     manifest: "AndroidManifest.xml",
}
```

src/main/Android.bp

```
android_app {
    name: "MediaModeService",

    platform_apis: true,
    certificate: "platform",
    privileged: true,
    system_ext_specific: true,

    srcs: [
        "java/com/harman/mediamodeservice/service/MediaModeService.java",
        "java/com/harman/mediamodeservice/service/**/*.aidl",
    ],
    aidl: {
        local_include_dirs: ["java"],
    },
    resource_dirs: ["res"],

    libs: ["android.car","com.harman.effect"],
    static_libs: [
        "mediamodeservice_lastmediasource",
        "mediamodeservice_persistence",
        "mediamodeservice_s2r",
        "mediamodeservice_utility",
        "mediamodeservice_carserviceproxy",
        "mediamodeservice_lastaudiosetting",
        "readytuneclientapi",
        "kotlin-stdlib"
    ],
}
```

### 3、引入 so

在模块源码根文件下新建文件夹 armeabi，复制要引入的 so 至此，在libs中新建 Android.bp

新增如下语句,这里以 libjniopencv\_face.so 为例, arm 和 arm64 分别对应32/64的so库,针对源码环境

位数都是确定的，所以我们就写成一样了

```
cc_prebuilt_library_shared {
    name: "libjniopencv_face",
    arch: {
        arm: {
            srcs: ["armeabi/libjniopencv_face.so"],
        },
        arm64: {
            srcs: ["armeabi/libjniopencv_face.so"],
        },
    },
}
```

然后在模块目录下 Android.bp 文件中的 android\_app {} 中 jni\_libs 引入 “libjniopencv\_face”,

```
android_app {
    name: "LiveTv",

     jni_libs: [
        "libjniopencv_face",
    ]

]
```

### 4、一个完整的包含 aar/jar/so Android.bp

### 4.1、libs

```
----Android.bp
----lottie-2.8.0.aar
----face-opencv-jar
----armeabi
    -----libjniopencv_face.so
    -----libopencv_text.so
```

### 4.2、libs/Android.bp

```
android_library_import {
    name: "lib-lottie",
    aars: ["lottie-2.8.0.aar"],
    sdk_version: "current",
}

java_import {
    name: "face-opencv-jar",
    jars: ["opencv.jar"],
    sdk_version: "current",
}

cc_prebuilt_library_shared {
    name: "libjniopencv_face",
    arch: {
        arm: {
            srcs: ["armeabi/libjniopencv_face.so"],
        },
        arm64: {
            srcs: ["armeabi/libjniopencv_face.so"],
        },
    },
}

cc_prebuilt_library_shared {
    name: "libopencv_text",
    arch: {
        arm: {
            srcs: ["armeabi/libopencv_text.so"],
        },
        arm64: {
            srcs: ["armeabi/libopencv_text.so"],
        },
    },
}
```

### 4.3、模块根路径Android.bp

```
android_app {
    name: "LiveTv",

    srcs: ["src/**/*.java"],

    // TODO(b/122608868) turn proguard back on
    optimize: {
        enabled: false,
    },

    // It is required for com.android.providers.tv.permission.ALL_EPG_DATA
    privileged: true,

    sdk_version: "system_current",
    min_sdk_version: "23", // M

    resource_dirs: [
        "res",
        "material_res",
        "res-lottie",

    ],

    libs: [
        "face-opencv-jar",
    ],

    static_libs: [
        "android-support-compat",
        "android-support-core-ui",
        "androidx.tvprovider_tvprovider",
        "android-support-v4",
        "android-support-v7-appcompat",
        "android-support-v7-palette",
        "android-support-v7-preference",
        "android-support-v7-recyclerview",
        "android-support-v14-preference",
        "android-support-v17-leanback",
        "android-support-v17-preference-leanback",
        "lib-lottie",
    ],

     jni_libs: [
        "libjniopencv_face",
        "libopencv_text",
    ]

    javacflags: [
        "-Xlint:deprecation",
        "-Xlint:unchecked",
    ],

    aaptflags: [
        "--version-name",
        version_name,

        "--version-code",
        version_code,

        "--extra-packages",
        "com.android.tv.tuner",

        "--extra-packages",
        "com.airbnb.lottie",
    ],
}
```

一般需要引入的so库都会是几十个，每一个都需要在Android.bp配置对应的 cc\_prebuilt\_library\_shared

挨个复制会很浪费时间，据观察格式都是固定的，我们可以通过遍历文件夹来生成这个json串

将所有 so 库文件拷贝至 sdcard/Android/armeabi/,遍历读取文件名，按默认格式写入txt文件即可

```
private void getSoJson() {
        String fileAbsolutePath = Environment.getExternalStorageDirectory().getPath() + "/Android/armeabi/";
        Log.e("eee", "fileAbsolutePath ： " + fileAbsolutePath);
        File file = new File(fileAbsolutePath);
        File[] subFile = file.listFiles();
        String json = "";
        String soName = "";

        for (int iFileLength = 0; iFileLength < subFile.length; iFileLength++) {
            if (!subFile[iFileLength].isDirectory()) {
                String filename = subFile[iFileLength].getName();
                String name = filename.split("\\.")[0];
                Log.e("eee", "filename ： " + filename + "  name=" + name);

                json += "cc_prebuilt_library_shared {\n" +
                        "    name: \"" + name + "\",\n" +
                        "    arch: {\n" +
                        "        arm: {\n" +
                        "            srcs: [\"armeabi/" + filename + "\"],\n" +
                        "        },\n" +
                        "        arm64: {\n" +
                        "            srcs: [\"armeabi/" + filename + "\"],\n" +
                        "        },\n" +
                        "    },\n" +
                        "}" + "\r\n";

                soName += "\""+name+"\"," + "\r\n";
            }
        }

        Log.e("eee", "soJson =" + json);
        write2File("so.txt", json);
        write2File("libs.txt", soName);
    }

    private void write2File(String fileName, String data) {
        String strFilePath = Environment.getExternalStorageDirectory().getPath() + "/Android/" + fileName;
        String strContent = data + "\r\n";
        try {
            File sfile = new File(strFilePath);
            if (!sfile.exists()) {
                Log.d("TestFile", "Create the file:" + strFilePath);
                sfile.getParentFile().mkdirs();
                sfile.createNewFile();
            }
            RandomAccessFile raf = new RandomAccessFile(sfile, "rwd");
            raf.seek(sfile.length());
            raf.write(strContent.getBytes());
            raf.close();
        } catch (Exception e) {
            Log.e("TestFile", "Error on write File:" + e);
        }
    }
```

### 5、其它

未完待续【关机】

[https://blog.csdn.net/tkwxty/article/details/104395820](https://link.zhihu.com/?target=https%3A//blog.csdn.net/tkwxty/article/details/104395820)

## 四、AOSP编译错误汇总

请根据实际项目使用和理解，因为比较容易出错，所以单独拎出来

### 1、**重要的注意事项，Android.mk可以引用Android.bp中的模块，反之Android.bp不能引用Android.mk中的模块**

### 2、**Android.bp模块，不支持../../去寻找上层路径的文件，只支持本目录下的folder里面的文件，如libs/Bgwan.aar**

### 3、unkonw type namespace

namespace不对.解决方法 将 c 文件改成 cpp

### 4、can not link against

因为编译的模块的测试模块依赖的共享库是vendor的, 所以测试模块页必须是vendor的

在测试模块加上:

> LOCAL\_VENDOR\_MODULE := true

### 5、missing dependencies

这个是找不到应用的模块, 原因是模块用Android.mk构建编译,而编译的测试模块是Android.bp构建的

全部改成`Android.bp`（用 androidmk 将 Android.mk 生成为 Android.bp）

**⚠️ 注意**

> 但是bp是无法条件编译的, 如果里面有条件编译，你可能需要修改你的Android.mk, 同时你的模块如果引用了其他模块，其他模块也要改成Android.mk, 这里麻烦一些，这里又要全部改成 Android.mk

下面是例子: _BUILD\_NATIVE\_TEST 自动包含依赖项 gtest_ LOCAL\_MODULE\_TAGS 标志测试模块

```
include $(CLEAR_VARS)
LOCAL_MODULE := config-test
LOCAL_SRC_FILES := config_test.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_CFLAGS = -Wall -Werror -Wunused
#LOCAL_CLANG_CFLAGS += -Wno-error=unused-const-variable -Wno-error=unused-private-field
LOCAL_MODULE_TAGS := tests
LOCAL_SHARED_LIBRARIES := libconfig
LOCAL_VENDOR_MODULE := true
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
include $(BUILD_NATIVE_TEST)
```

### 6、Compilation can’t be completed because some library classes are missing

是因为你的aar库没有放到libs库中，或者文件目录结构错误，可以尝试将 “xxx.aar”或“xxx.jar”, 移动到 libs: \[\] 中再次尝试

### 7、编译时会提示找不到资源，运行时会报错

如果aar中带资源文件，需要将aar解压拷贝资源文件，不然编译时会提示找不到资源，运行时会报错

解压 bgwanxxx.aar，在模块源码根文件夹下新建 res-bgwanxxx 文件夹，将资源文件拷贝到此目录

模块目录下 Android.bp 文件中的 resource\_dirs: \[\] 引入

```
android_app {
    name: "LiveTv",

    srcs: ["src/**/*.java"],

    resource_dirs: [
        "res",
        "res_ext",
        "res-bgwanxxx",
    ],

    static_libs: [
        "lib-bgwan",
        "android-support-annotations",
        "android-support-compat",
        "android-support-core-ui",
        "androidx.tvprovider_tvprovider",
        "android-support-v4",
        ....
    ],

    aaptflags: [
        "--extra-packages",
        "com.sunst.ol",
    ],
```

同时增加aar对应的包名 aaptflags，以便生成对应包名 R 文件（多个aar增按照如上操作配置多个即可） aaptflags --extra-packages com.sunst.ol, 编译时会生成 com.sunst.ol.R

## 五、附录：mk与bp映射表

Android.mk 和Android.bp 的对应关系，可以查看Android源码下，为了方便查看本神把它列到下面

> /build/soong/androidmk/cmd/androidmk/android.go

```
package main
import (
    mkparser "android/soong/androidmk/parser"
    "fmt"
    "sort"
    "strings"

    bpparser "github.com/google/blueprint/parser"
)

const (
    clear_vars      = "__android_mk_clear_vars"
    include_ignored = "__android_mk_include_ignored"
)

type bpVariable struct {
    name         string
    variableType bpparser.Type
}

type variableAssignmentContext struct {
    file    *bpFile
    prefix  string
    mkvalue *mkparser.MakeString
    append  bool
}

var rewriteProperties = map[string](func(variableAssignmentContext) error){
    // custom functions
    "LOCAL_AIDL_INCLUDES":         localAidlIncludes,
    "LOCAL_C_INCLUDES":            localIncludeDirs,
    "LOCAL_EXPORT_C_INCLUDE_DIRS": exportIncludeDirs,
    "LOCAL_LDFLAGS":               ldflags,
    "LOCAL_MODULE_CLASS":          prebuiltClass,
    "LOCAL_MODULE_STEM":           stem,
    "LOCAL_MODULE_HOST_OS":        hostOs,
    "LOCAL_SANITIZE":              sanitize(""),
    "LOCAL_SANITIZE_DIAG":         sanitize("diag."),
    "LOCAL_CFLAGS":                cflags,
    "LOCAL_UNINSTALLABLE_MODULE":  invert("installable"),
    "LOCAL_PROGUARD_ENABLED":      proguardEnabled,

    // composite functions
    "LOCAL_MODULE_TAGS": includeVariableIf(bpVariable{"tags", bpparser.ListType}, not(valueDumpEquals("optional"))),

    // skip functions
    "LOCAL_ADDITIONAL_DEPENDENCIES": skip, // TODO: check for only .mk files?
    "LOCAL_CPP_EXTENSION":           skip,
    "LOCAL_MODULE_SUFFIX":           skip, // TODO
    "LOCAL_PATH":                    skip, // Nothing to do, except maybe avoid the "./" in paths?
    "LOCAL_PRELINK_MODULE":          skip, // Already phased out
    "LOCAL_BUILT_MODULE_STEM":       skip,
    "LOCAL_USE_AAPT2":               skip, // Always enabled in Soong
    "LOCAL_JAR_EXCLUDE_FILES":       skip, // Soong never excludes files from jars
}

// adds a group of properties all having the same type
func addStandardProperties(propertyType bpparser.Type, properties map[string]string) {
    for key, val := range properties {
        rewriteProperties[key] = includeVariable(bpVariable{val, propertyType})
    }
}

func init() {
    addStandardProperties(bpparser.StringType,
        map[string]string{
            "LOCAL_MODULE":                  "name",
            "LOCAL_CXX_STL":                 "stl",
            "LOCAL_STRIP_MODULE":            "strip",
            "LOCAL_MULTILIB":                "compile_multilib",
            "LOCAL_ARM_MODE_HACK":           "instruction_set",
            "LOCAL_SDK_VERSION":             "sdk_version",
            "LOCAL_NDK_STL_VARIANT":         "stl",
            "LOCAL_JAR_MANIFEST":            "manifest",
            "LOCAL_JARJAR_RULES":            "jarjar_rules",
            "LOCAL_CERTIFICATE":             "certificate",
            "LOCAL_PACKAGE_NAME":            "name",
            "LOCAL_MODULE_RELATIVE_PATH":    "relative_install_path",
            "LOCAL_PROTOC_OPTIMIZE_TYPE":    "proto.type",
            "LOCAL_MODULE_OWNER":            "owner",
            "LOCAL_RENDERSCRIPT_TARGET_API": "renderscript.target_api",
            "LOCAL_NOTICE_FILE":             "notice",
            "LOCAL_JAVA_LANGUAGE_VERSION":   "java_version",
            "LOCAL_INSTRUMENTATION_FOR":     "instrumentation_for",
            "LOCAL_MANIFEST_FILE":           "manifest",

            "LOCAL_DEX_PREOPT_PROFILE_CLASS_LISTING": "dex_preopt.profile",
        })
    addStandardProperties(bpparser.ListType,
        map[string]string{
            "LOCAL_SRC_FILES":                     "srcs",
            "LOCAL_SRC_FILES_EXCLUDE":             "exclude_srcs",
            "LOCAL_HEADER_LIBRARIES":              "header_libs",
            "LOCAL_SHARED_LIBRARIES":              "shared_libs",//【引用 C/C++ 动态库(也叫共享库）】
            "LOCAL_STATIC_LIBRARIES":              "static_libs",//【引用 C/C++ 静态库 】
            "LOCAL_WHOLE_STATIC_LIBRARIES":        "whole_static_libs",
            "LOCAL_SYSTEM_SHARED_LIBRARIES":       "system_shared_libs",
            "LOCAL_ASFLAGS":                       "asflags",
            "LOCAL_CLANG_ASFLAGS":                 "clang_asflags",
            "LOCAL_CONLYFLAGS":                    "conlyflags",
            "LOCAL_CPPFLAGS":                      "cppflags",
            "LOCAL_REQUIRED_MODULES":              "required",
            "LOCAL_OVERRIDES_MODULES":             "overrides",
            "LOCAL_LDLIBS":                        "host_ldlibs",
            "LOCAL_CLANG_CFLAGS":                  "clang_cflags",
            "LOCAL_YACCFLAGS":                     "yaccflags",
            "LOCAL_SANITIZE_RECOVER":              "sanitize.recover",
            "LOCAL_LOGTAGS_FILES":                 "logtags",
            "LOCAL_EXPORT_HEADER_LIBRARY_HEADERS": "export_header_lib_headers",
            "LOCAL_EXPORT_SHARED_LIBRARY_HEADERS": "export_shared_lib_headers",
            "LOCAL_EXPORT_STATIC_LIBRARY_HEADERS": "export_static_lib_headers",
            "LOCAL_INIT_RC":                       "init_rc",
            "LOCAL_TIDY_FLAGS":                    "tidy_flags",
            // TODO: This is comma-separated, not space-separated
            "LOCAL_TIDY_CHECKS":           "tidy_checks",
            "LOCAL_RENDERSCRIPT_INCLUDES": "renderscript.include_dirs",
            "LOCAL_RENDERSCRIPT_FLAGS":    "renderscript.flags",

            "LOCAL_JAVA_RESOURCE_DIRS":    "java_resource_dirs",
            "LOCAL_RESOURCE_DIR":          "resource_dirs",
            "LOCAL_JAVACFLAGS":            "javacflags",
            "LOCAL_ERROR_PRONE_FLAGS":     "errorprone.javacflags",
            "LOCAL_DX_FLAGS":              "dxflags",
            "LOCAL_JAVA_LIBRARIES":        "libs",//【引用JAVA 动态库】
            "LOCAL_STATIC_JAVA_LIBRARIES": "static_libs",//【引用JAVA 静态库】 
            "LOCAL_AAPT_FLAGS":            "aaptflags",
            "LOCAL_PACKAGE_SPLITS":        "package_splits",
            "LOCAL_COMPATIBILITY_SUITE":   "test_suites",

            "LOCAL_ANNOTATION_PROCESSORS":        "annotation_processors",
            "LOCAL_ANNOTATION_PROCESSOR_CLASSES": "annotation_processor_classes",

            "LOCAL_PROGUARD_FLAGS":      "optimize.proguard_flags",
            "LOCAL_PROGUARD_FLAG_FILES": "optimize.proguard_flag_files",

            // These will be rewritten to libs/static_libs by bpfix, after their presence is used to convert
            // java_library_static to android_library.
            "LOCAL_SHARED_ANDROID_LIBRARIES": "android_libs",
            "LOCAL_STATIC_ANDROID_LIBRARIES": "android_static_libs",
        })

    addStandardProperties(bpparser.BoolType,
        map[string]string{
            // Bool properties
            "LOCAL_IS_HOST_MODULE":           "host",
            "LOCAL_CLANG":                    "clang",
            "LOCAL_FORCE_STATIC_EXECUTABLE":  "static_executable",
            "LOCAL_NATIVE_COVERAGE":          "native_coverage",
            "LOCAL_NO_CRT":                   "nocrt",
            "LOCAL_ALLOW_UNDEFINED_SYMBOLS":  "allow_undefined_symbols",
            "LOCAL_RTTI_FLAG":                "rtti",
            "LOCAL_NO_STANDARD_LIBRARIES":    "no_standard_libs",
            "LOCAL_PACK_MODULE_RELOCATIONS":  "pack_relocations",
            "LOCAL_TIDY":                     "tidy",
            "LOCAL_PROPRIETARY_MODULE":       "proprietary",
            "LOCAL_VENDOR_MODULE":            "vendor",
            "LOCAL_ODM_MODULE":               "device_specific",
            "LOCAL_PRODUCT_MODULE":           "product_specific",
            "LOCAL_EXPORT_PACKAGE_RESOURCES": "export_package_resources",
            "LOCAL_PRIVILEGED_MODULE":        "privileged",

            "LOCAL_DEX_PREOPT":                  "dex_preopt.enabled",
            "LOCAL_DEX_PREOPT_APP_IMAGE":        "dex_preopt.app_image",
            "LOCAL_DEX_PREOPT_GENERATE_PROFILE": "dex_preopt.profile_guided",
        })
}

type listSplitFunc func(bpparser.Expression) (string, bpparser.Expression, error)

func emptyList(value bpparser.Expression) bool {
    if list, ok := value.(*bpparser.List); ok {
        return len(list.Values) == 0
    }
    return false
}

func splitBpList(val bpparser.Expression, keyFunc listSplitFunc) (lists map[string]bpparser.Expression, err error) {
    lists = make(map[string]bpparser.Expression)

    switch val := val.(type) {
    case *bpparser.Operator:
        listsA, err := splitBpList(val.Args[0], keyFunc)
        if err != nil {
            return nil, err
        }

        listsB, err := splitBpList(val.Args[1], keyFunc)
        if err != nil {
            return nil, err
        }

        for k, v := range listsA {
            if !emptyList(v) {
                lists[k] = v
            }
        }

        for k, vB := range listsB {
            if emptyList(vB) {
                continue
            }

            if vA, ok := lists[k]; ok {
                expression := val.Copy().(*bpparser.Operator)
                expression.Args = [2]bpparser.Expression{vA, vB}
                lists[k] = expression
            } else {
                lists[k] = vB
            }
        }
    case *bpparser.Variable:
        key, value, err := keyFunc(val)
        if err != nil {
            return nil, err
        }
        if value.Type() == bpparser.ListType {
            lists[key] = value
        } else {
            lists[key] = &bpparser.List{
                Values: []bpparser.Expression{value},
            }
        }
    case *bpparser.List:
        for _, v := range val.Values {
            key, value, err := keyFunc(v)
            if err != nil {
                return nil, err
            }
            l := lists[key]
            if l == nil {
                l = &bpparser.List{}
            }
            l.(*bpparser.List).Values = append(l.(*bpparser.List).Values, value)
            lists[key] = l
        }
    default:
        panic(fmt.Errorf("unexpected type %t", val))
    }

    return lists, nil
}

// classifyLocalOrGlobalPath tells whether a file path should be interpreted relative to the current module (local)
// or relative to the root of the source checkout (global)
func classifyLocalOrGlobalPath(value bpparser.Expression) (string, bpparser.Expression, error) {
    switch v := value.(type) {
    case *bpparser.Variable:
        if v.Name == "LOCAL_PATH" {
            return "local", &bpparser.String{
                Value: ".",
            }, nil
        } else {
            // TODO: Should we split variables?
            return "global", value, nil
        }
    case *bpparser.Operator:
        if v.Type() != bpparser.StringType {
            return "", nil, fmt.Errorf("classifyLocalOrGlobalPath expected a string, got %s", v.Type())
        }

        if v.Operator != '+' {
            return "global", value, nil
        }

        firstOperand := v.Args[0]
        secondOperand := v.Args[1]
        if firstOperand.Type() != bpparser.StringType {
            return "global", value, nil
        }

        if _, ok := firstOperand.(*bpparser.Operator); ok {
            return "global", value, nil
        }

        if variable, ok := firstOperand.(*bpparser.Variable); !ok || variable.Name != "LOCAL_PATH" {
            return "global", value, nil
        }

        local := secondOperand
        if s, ok := secondOperand.(*bpparser.String); ok {
            if strings.HasPrefix(s.Value, "/") {
                s.Value = s.Value[1:]
            }
        }
        return "local", local, nil
    case *bpparser.String:
        return "global", value, nil
    default:
        return "", nil, fmt.Errorf("classifyLocalOrGlobalPath expected a string, got %s", v.Type())

    }
}

func sortedMapKeys(inputMap map[string]string) (sortedKeys []string) {
    keys := make([]string, 0, len(inputMap))
    for key := range inputMap {
        keys = append(keys, key)
    }
    sort.Strings(keys)
    return keys
}

// splitAndAssign splits a Make list into components and then
// creates the corresponding variable assignments.
func splitAndAssign(ctx variableAssignmentContext, splitFunc listSplitFunc, namesByClassification map[string]string) error {
    val, err := makeVariableToBlueprint(ctx.file, ctx.mkvalue, bpparser.ListType)
    if err != nil {
        return err
    }

    lists, err := splitBpList(val, splitFunc)
    if err != nil {
        return err
    }

    for _, nameClassification := range sortedMapKeys(namesByClassification) {
        name := namesByClassification[nameClassification]
        if component, ok := lists[nameClassification]; ok && !emptyList(component) {
            err = setVariable(ctx.file, ctx.append, ctx.prefix, name, component, true)
            if err != nil {
                return err
            }
        }
    }
    return nil
}

func localIncludeDirs(ctx variableAssignmentContext) error {
    return splitAndAssign(ctx, classifyLocalOrGlobalPath, map[string]string{"global": "include_dirs", "local": "local_include_dirs"})
}

func exportIncludeDirs(ctx variableAssignmentContext) error {
    // Add any paths that could not be converted to local relative paths to export_include_dirs
    // anyways, they will cause an error if they don't exist and can be fixed manually.
    return splitAndAssign(ctx, classifyLocalOrGlobalPath, map[string]string{"global": "export_include_dirs", "local": "export_include_dirs"})
}

func localAidlIncludes(ctx variableAssignmentContext) error {
    return splitAndAssign(ctx, classifyLocalOrGlobalPath, map[string]string{"global": "aidl.include_dirs", "local": "aidl.local_include_dirs"})
}

func stem(ctx variableAssignmentContext) error {
    val, err := makeVariableToBlueprint(ctx.file, ctx.mkvalue, bpparser.StringType)
    if err != nil {
        return err
    }
    varName := "stem"

    if exp, ok := val.(*bpparser.Operator); ok && exp.Operator == '+' {
        if variable, ok := exp.Args[0].(*bpparser.Variable); ok && variable.Name == "LOCAL_MODULE" {
            varName = "suffix"
            val = exp.Args[1]
        }
    }

    return setVariable(ctx.file, ctx.append, ctx.prefix, varName, val, true)
}

func hostOs(ctx variableAssignmentContext) error {
    val, err := makeVariableToBlueprint(ctx.file, ctx.mkvalue, bpparser.ListType)
    if err != nil {
        return err
    }

    inList := func(s string) bool {
        for _, v := range val.(*bpparser.List).Values {
            if v.(*bpparser.String).Value == s {
                return true
            }
        }
        return false
    }

    falseValue := &bpparser.Bool{
        Value: false,
    }

    trueValue := &bpparser.Bool{
        Value: true,
    }

    if inList("windows") {
        err = setVariable(ctx.file, ctx.append, "target.windows", "enabled", trueValue, true)
    }

    if !inList("linux") && err == nil {
        err = setVariable(ctx.file, ctx.append, "target.linux_glibc", "enabled", falseValue, true)
    }

    if !inList("darwin") && err == nil {
        err = setVariable(ctx.file, ctx.append, "target.darwin", "enabled", falseValue, true)
    }

    return err
}

func sanitize(sub string) func(ctx variableAssignmentContext) error {
    return func(ctx variableAssignmentContext) error {
        val, err := makeVariableToBlueprint(ctx.file, ctx.mkvalue, bpparser.ListType)
        if err != nil {
            return err
        }

        if _, ok := val.(*bpparser.List); !ok {
            return fmt.Errorf("unsupported sanitize expression")
        }

        misc := &bpparser.List{}

        for _, v := range val.(*bpparser.List).Values {
            switch v := v.(type) {
            case *bpparser.Variable, *bpparser.Operator:
                ctx.file.errorf(ctx.mkvalue, "unsupported sanitize expression")
            case *bpparser.String:
                switch v.Value {
                case "never", "address", "coverage", "thread", "undefined", "cfi":
                    bpTrue := &bpparser.Bool{
                        Value: true,
                    }
                    err = setVariable(ctx.file, false, ctx.prefix, "sanitize."+sub+v.Value, bpTrue, true)
                    if err != nil {
                        return err
                    }
                default:
                    misc.Values = append(misc.Values, v)
                }
            default:
                return fmt.Errorf("sanitize expected a string, got %s", v.Type())
            }
        }

        if len(misc.Values) > 0 {
            err = setVariable(ctx.file, false, ctx.prefix, "sanitize."+sub+"misc_undefined", misc, true)
            if err != nil {
                return err
            }
        }

        return err
    }
}

func prebuiltClass(ctx variableAssignmentContext) error {
    class := ctx.mkvalue.Value(ctx.file.scope)
    if v, ok := prebuiltTypes[class]; ok {
        ctx.file.scope.Set("BUILD_PREBUILT", v)
    } else {
        // reset to default
        ctx.file.scope.Set("BUILD_PREBUILT", "prebuilt")
    }
    return nil
}

func ldflags(ctx variableAssignmentContext) error {
    val, err := makeVariableToBlueprint(ctx.file, ctx.mkvalue, bpparser.ListType)
    if err != nil {
        return err
    }

    lists, err := splitBpList(val, func(value bpparser.Expression) (string, bpparser.Expression, error) {
        // Anything other than "-Wl,--version_script," + LOCAL_PATH + "<path>" matches ldflags
        exp1, ok := value.(*bpparser.Operator)
        if !ok {
            return "ldflags", value, nil
        }

        exp2, ok := exp1.Args[0].(*bpparser.Operator)
        if !ok {
            return "ldflags", value, nil
        }

        if s, ok := exp2.Args[0].(*bpparser.String); !ok || s.Value != "-Wl,--version-script," {
            return "ldflags", value, nil
        }

        if v, ok := exp2.Args[1].(*bpparser.Variable); !ok || v.Name != "LOCAL_PATH" {
            ctx.file.errorf(ctx.mkvalue, "Unrecognized version-script")
            return "ldflags", value, nil
        }

        s, ok := exp1.Args[1].(*bpparser.String)
        if !ok {
            ctx.file.errorf(ctx.mkvalue, "Unrecognized version-script")
            return "ldflags", value, nil
        }

        s.Value = strings.TrimPrefix(s.Value, "/")

        return "version", s, nil
    })
    if err != nil {
        return err
    }

    if ldflags, ok := lists["ldflags"]; ok && !emptyList(ldflags) {
        err = setVariable(ctx.file, ctx.append, ctx.prefix, "ldflags", ldflags, true)
        if err != nil {
            return err
        }
    }

    if version_script, ok := lists["version"]; ok && !emptyList(version_script) {
        if len(version_script.(*bpparser.List).Values) > 1 {
            ctx.file.errorf(ctx.mkvalue, "multiple version scripts found?")
        }
        err = setVariable(ctx.file, false, ctx.prefix, "version_script", version_script.(*bpparser.List).Values[0], true)
        if err != nil {
            return err
        }
    }

    return nil
}

func cflags(ctx variableAssignmentContext) error {
    // The Soong replacement for CFLAGS doesn't need the same extra escaped quotes that were present in Make
    ctx.mkvalue = ctx.mkvalue.Clone()
    ctx.mkvalue.ReplaceLiteral(`\"`, `"`)
    return includeVariableNow(bpVariable{"cflags", bpparser.ListType}, ctx)
}

func proguardEnabled(ctx variableAssignmentContext) error {
    val, err := makeVariableToBlueprint(ctx.file, ctx.mkvalue, bpparser.ListType)
    if err != nil {
        return err
    }

    list, ok := val.(*bpparser.List)
    if !ok {
        return fmt.Errorf("unsupported proguard expression")
    }

    set := func(prop string, value bool) {
        bpValue := &bpparser.Bool{
            Value: value,
        }
        setVariable(ctx.file, false, ctx.prefix, prop, bpValue, true)
    }

    enable := false

    for _, v := range list.Values {
        s, ok := v.(*bpparser.String)
        if !ok {
            return fmt.Errorf("unsupported proguard expression")
        }

        switch s.Value {
        case "disabled":
            set("optimize.enabled", false)
        case "obfuscation":
            enable = true
            set("optimize.obfuscate", true)
        case "optimization":
            enable = true
            set("optimize.optimize", true)
        case "full":
            enable = true
        case "custom":
            set("optimize.no_aapt_flags", true)
            enable = true
        default:
            return fmt.Errorf("unsupported proguard value %q", s)
        }
    }

    if enable {
        // This is only necessary for libraries which default to false, but we can't
        // tell the difference between a library and an app here.
        set("optimize.enabled", true)
    }

    return nil
}

func invert(name string) func(ctx variableAssignmentContext) error {
    return func(ctx variableAssignmentContext) error {
        val, err := makeVariableToBlueprint(ctx.file, ctx.mkvalue, bpparser.BoolType)
        if err != nil {
            return err
        }

        val.(*bpparser.Bool).Value = !val.(*bpparser.Bool).Value

        return setVariable(ctx.file, ctx.append, ctx.prefix, name, val, true)
    }
}

// given a conditional, returns a function that will insert a variable assignment or not, based on the conditional
func includeVariableIf(bpVar bpVariable, conditional func(ctx variableAssignmentContext) bool) func(ctx variableAssignmentContext) error {
    return func(ctx variableAssignmentContext) error {
        var err error
        if conditional(ctx) {
            err = includeVariableNow(bpVar, ctx)
        }
        return err
    }
}

// given a variable, returns a function that will always insert a variable assignment
func includeVariable(bpVar bpVariable) func(ctx variableAssignmentContext) error {
    return includeVariableIf(bpVar, always)
}

func includeVariableNow(bpVar bpVariable, ctx variableAssignmentContext) error {
    var val bpparser.Expression
    var err error
    val, err = makeVariableToBlueprint(ctx.file, ctx.mkvalue, bpVar.variableType)
    if err == nil {
        err = setVariable(ctx.file, ctx.append, ctx.prefix, bpVar.name, val, true)
    }
    return err
}

// given a function that returns a bool, returns a function that returns the opposite
func not(conditional func(ctx variableAssignmentContext) bool) func(ctx variableAssignmentContext) bool {
    return func(ctx variableAssignmentContext) bool {
        return !conditional(ctx)
    }
}

// returns a function that tells whether mkvalue.Dump equals the given query string
func valueDumpEquals(textToMatch string) func(ctx variableAssignmentContext) bool {
    return func(ctx variableAssignmentContext) bool {
        return (ctx.mkvalue.Dump() == textToMatch)
    }
}

func always(ctx variableAssignmentContext) bool {
    return true
}

func skip(ctx variableAssignmentContext) error {
    return nil
}

// Shorter suffixes of other suffixes must be at the end of the list
var propertyPrefixes = []struct{ mk, bp string }{
    {"arm", "arch.arm"},
    {"arm64", "arch.arm64"},
    {"mips", "arch.mips"},
    {"mips64", "arch.mips64"},
    {"x86", "arch.x86"},
    {"x86_64", "arch.x86_64"},
    {"32", "multilib.lib32"},
    // 64 must be after x86_64
    {"64", "multilib.lib64"},
    {"darwin", "target.darwin"},
    {"linux", "target.linux_glibc"},
    {"windows", "target.windows"},
}

var conditionalTranslations = map[string]map[bool]string{
    "($(HOST_OS),darwin)": {
        true:  "target.darwin",
        false: "target.not_darwin"},
    "($(HOST_OS), darwin)": {
        true:  "target.darwin",
        false: "target.not_darwin"},
    "($(HOST_OS),windows)": {
        true:  "target.windows",
        false: "target.not_windows"},
    "($(HOST_OS), windows)": {
        true:  "target.windows",
        false: "target.not_windows"},
    "($(HOST_OS),linux)": {
        true:  "target.linux_glibc",
        false: "target.not_linux_glibc"},
    "($(HOST_OS), linux)": {
        true:  "target.linux_glibc",
        false: "target.not_linux_glibc"},
    "($(BUILD_OS),darwin)": {
        true:  "target.darwin",
        false: "target.not_darwin"},
    "($(BUILD_OS), darwin)": {
        true:  "target.darwin",
        false: "target.not_darwin"},
    "($(BUILD_OS),linux)": {
        true:  "target.linux_glibc",
        false: "target.not_linux_glibc"},
    "($(BUILD_OS), linux)": {
        true:  "target.linux_glibc",
        false: "target.not_linux_glibc"},
    "(,$(TARGET_BUILD_APPS))": {
        false: "product_variables.unbundled_build"},
    "($(TARGET_BUILD_APPS),)": {
        false: "product_variables.unbundled_build"},
    "($(TARGET_BUILD_PDK),true)": {
        true: "product_variables.pdk"},
    "($(TARGET_BUILD_PDK), true)": {
        true: "product_variables.pdk"},
}

func mydir(args []string) string {
    return "."
}

func allFilesUnder(wildcard string) func(args []string) string {
    return func(args []string) string {
        dir := ""
        if len(args) > 0 {
            dir = strings.TrimSpace(args[0])
        }

        return fmt.Sprintf("%s/**/"+wildcard, dir)
    }
}

func allSubdirJavaFiles(args []string) string {
    return "**/*.java"
}

func includeIgnored(args []string) string {
    return include_ignored
}

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

    "BUILD_JAVA_LIBRARY":             "java_library",
    "BUILD_STATIC_JAVA_LIBRARY":      "java_library_static",
    "BUILD_HOST_JAVA_LIBRARY":        "java_library_host",
    "BUILD_HOST_DALVIK_JAVA_LIBRARY": "java_library_host_dalvik",
    "BUILD_PACKAGE":                  "android_app",
}

var prebuiltTypes = map[string]string{
    "SHARED_LIBRARIES": "cc_prebuilt_library_shared",
    "STATIC_LIBRARIES": "cc_prebuilt_library_static",
    "EXECUTABLES":      "cc_prebuilt_binary",
    "JAVA_LIBRARIES":   "java_import",
}

var soongModuleTypes = map[string]bool{}

func androidScope() mkparser.Scope {
    globalScope := mkparser.NewScope(nil)
    globalScope.Set("CLEAR_VARS", clear_vars)
    globalScope.SetFunc("my-dir", mydir)
    globalScope.SetFunc("all-java-files-under", allFilesUnder("*.java"))
    globalScope.SetFunc("all-proto-files-under", allFilesUnder("*.proto"))
    globalScope.SetFunc("all-aidl-files-under", allFilesUnder("*.aidl"))
    globalScope.SetFunc("all-Iaidl-files-under", allFilesUnder("I*.aidl"))
    globalScope.SetFunc("all-logtags-files-under", allFilesUnder("*.logtags"))
    globalScope.SetFunc("all-subdir-java-files", allSubdirJavaFiles)
    globalScope.SetFunc("all-makefiles-under", includeIgnored)
    globalScope.SetFunc("first-makefiles-under", includeIgnored)
    globalScope.SetFunc("all-named-subdir-makefiles", includeIgnored)
    globalScope.SetFunc("all-subdir-makefiles", includeIgnored)

    for k, v := range moduleTypes {
        globalScope.Set(k, v)
        soongModuleTypes[v] = true
    }
    for _, v := range prebuiltTypes {
        soongModuleTypes[v] = true
    }

    return globalScope
}
```

## 致谢（引用和推荐）（可选）

本文参考借鉴了以下文章部分内容，非常感谢各位前辈的开源精神，当代互联网的发展离不开你们的分享，再次感谢 .同时以下↓↓↓，也是本神推荐阅读系列

-   \-\[x\] **[\*\*Android.bp 文件中引入aar、jar、so库正确编译方法](https://link.zhihu.com/?target=https%3A//blog.csdn.net/u012932409/article/details/108119443)**
-   \-\[x\] **[\*\*APP 引用第三方aar包和jar包 Android.bp 和 Android.mk 编写](https://link.zhihu.com/?target=https%3A//blog.csdn.net/kanyou222/article/details/107846262)**
-   \-\[x\] **[\*\*Android.bp入门指南之浅析Android.bp语法](https://link.zhihu.com/?target=https%3A//blog.csdn.net/tkwxty/article/details/104395820)**

## @[©LICENSE](https://zhuanlan.zhihu.com/p/80668416)（版权和更新记录）

请尊重劳动成果，注意文中[版权声明](https://zhuanlan.zhihu.com/p/80668416)，[Android专栏](https://zhuanlan.zhihu.com/qyddai)不定时更新，☀️欢迎关注我的知乎Bgwan**[(点我跳转)](https://www.zhihu.com/people/qydq)**。也可以同时关注[人工智能专栏](https://zhuanlan.zhihu.com/sstai)，[文艺语录专栏](https://zhuanlan.zhihu.com/xiaoyue?author=qydq)，技术上沟通可以在qyddai@gmail.com或知乎上留言

> 2023-09-08：修改`《一.2.4、小节_源码路径》`**→**`<⑴、在线浏览源码的地址>`，目前Android最新版本

知乎是一个不错的平台，对于技术类的内容，不像CSDN需要付费阅读；当然整理本内容我也会花费了不少时间和精力，尤其是现在有了小baby要照顾；本着技术类分享精神，如果你觉得本文对你所帮助，你也可以分享给更多的同学，[或支持一下＃(香)](https://link.zhihu.com/?target=https%3A//bgwan.oss-cn-shanghai.aliyuncs.com/Beautiful_Life/Wealth_Manage/Images/Award_Pay.png)。授人以渔，不如授人以渔，开源和技术分享才能促进科技的进步

作者：sunst0069

发布日期：2023-11-28 14:38；更新日期：2024-01-20；维护次数：2
