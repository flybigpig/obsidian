---
created: 2025-04-23T10:27:27 (UTC +08:00)
tags: [Android 开发,Android,Android 开发入门]
source: https://zhuanlan.zhihu.com/p/680173022
author: 关于作者Aliff即时通讯，软件设计与开发者，物联网，移动互联网，AIVRVI兴趣者回答89文章161关注者1,291关注他发私信
---

# Android.mk解析与使用看这篇就够了 - 知乎

> ## Excerpt
> Android.mk解析与使用看这篇就够了背景图来源：<成都：雪山下的公园城市 (点我跳转)> 争取每一篇文章都是精华，每一篇文章都能做到后期维护，本篇内容也可通过本人唯一 〖阿里云地址(点我跳转)〗 查看写在前…

---
![](https://pic3.zhimg.com/v2-4f4e89104d1f909865fd07411e263dc4_1440w.jpg)

背景图来源：<成都：雪山下的公园城市[(点我跳转)](https://link.zhihu.com/?target=https%3A//www.sohu.com/a/505547175_120099883)\>

_争取每一篇文章都是精华，每一篇文章都能做到后期维护_，本篇内容也可通过本人唯一 **〖阿里云地址[(点我跳转)](https://link.zhihu.com/?target=https%3A//bgwan.oss-cn-shanghai.aliyuncs.com/sunst0069/Company_Car/Framework/Android.mk%25E8%25A7%25A3%25E6%259E%2590%25E4%25B8%258E%25E4%25BD%25BF%25E7%2594%25A8%25E7%259C%258B%25E8%25BF%2599%25E7%25AF%2587%25E5%25B0%25B1%25E5%25A4%259F%25E4%25BA%2586.html)〗** 查看

## 写在前面

官网对Android.mk的介绍[(点我跳转)](https://link.zhihu.com/?target=https%3A//developer.android.com/ndk/guides/android_mk%3Fhl%3Dzh-cn/)；注意新的源码中很多app已经切换到了[Android.bp](https://zhida.zhihu.com/search?content_id=239279427&content_type=Article&match_order=1&q=Android.bp&zhida_source=entity)，不过目前Android.mk还是兼容的

## 一、Android.mk理解

Android.mk是一个向Android [NDK](https://zhida.zhihu.com/search?content_id=239279427&content_type=Article&match_order=1&q=NDK&zhida_source=entity)构建系统描述NDK项目的[GNU makefile](https://zhida.zhihu.com/search?content_id=239279427&content_type=Article&match_order=1&q=GNU+makefile&zhida_source=entity)片段（可以理解为Android工程管理文件的说明书）。将源文件分组为**模块**或编译生成以下几种：

![](https://pic1.zhimg.com/Android_1440w.jpg)

-   APK程序：一般的Android应用程序，系统级别的直接push即可
-   JAVA库：JAVA类库，编译打包生成jar文件
-   C\\C++应用程序：可执行的C\\C++应用程序
-   C\\C++静态库：编译生成C\\C++静态库，并打包成.a文件
-   C\\C++共享库：编译生成共享库，并打包成.so文件

![](https://pica.zhimg.com/Forward_1440w.jpg)

**⚠️ 注意**

-   Android.mk会被编译系统解析一次或多次，所以应该尽量减少源码中声明变量，从而不会影响到后面的解析。
-   可以在每一个Android.mk文件中定义一个或多个模块，也可以多个模块使用同一个 .mk 文件。

### 1、Android.mk的基本格式

```
#每个Android.mk文件必须以定义LOCAL_PATH为开始。它用于在开发tree中查找源文件。宏my-dir 则由Build System提供。返回包含Android.mk的目录路径（即包含Android.mk file文件的目录）。
#call 是调用一个系统提供的宏函数，此处是 my-dir
# $() 是取值
#:= 是赋值
LOCAL_PATH := $(call my-dir)
#清除LOCAL_PATH变量之外的LOCAL_XXX变量（即LOCAL_PATH不会被清除）
include $(CLEAR_VARS)

#需要编译的文件
LOCAL_SRC_FILES :=$(call all-subdir-java-files)
#当前模块包含的源代码文件 多个可以用空格隔开；当某一行很长时，可以使用反斜杠 \ 换行，反斜杠可以追加多个源文件
LOCAL_SRC_FILES := hello.c \
                                     utils.c

#生成的模块名称，模块名必须唯一，不能包含空格
LOCAL_MODULE := Bgwan#这里会生成（libbgwan.so，注意前面的lib)
LOCAL_MODULE := hello  
#编译的标签
LOCAL_MODULE_TAGS := optional
#指定签名
LOCAL_CERTIFICATE := platform

#引用静态jar
LOCAL_STATIC_JAVA_LIBRARIES := jar1 jar2

#编译生成文件的类型 (即预编译文件类型，详见《三.16、预编译jar包》)
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
#(1)JAVA_LIBRARIES、(2)APPS、(3)SHARED_LIBRARIES、(4)EXECUTABLES、(5)ETC
include $(BUILD_EXECUTABLE)

#(3)编译apk
include $(BUILD_PACKAGE)

#需要进行预编译的库
include $(CLEAR_VARS)  
LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES := jar1:path1 \
                                        jar2:path2
include $(BUILD_MULTI_PREBUILT) 
#(4)生成一个动态库(表示当前模块被编译成一个共享库)
include $(BUILD_SHARED_LIBRARY)
```

### 2、静态库和动态库的理解（可选）

库是写好的现有的，成熟的，可以复用的代码。本质上来说库是一种可执行代码的二进制形式，可以被操作系统载入内存执行。库有两种：静态库（.a、.lib）和动态库（.so、.dll）

**⚠️ 注意**

> 在ubuntu下so也叫共享库

所谓静态、动态是指链接。

-   静态库（.a、.lib）：它作为程序的一个模块，在链接期间被组合到程序中。
-   动态库（.so、.dll）：它在程序运行阶段被加载进内存。

将一个程序编译成可执行程序的步骤，可以查看 **《C语言：静态库和动态库》[(点我跳转)](https://link.zhihu.com/?target=https%3A//www.cnblogs.com/LXP-Never/p/15302534.html)**\>内容

![](https://pic1.zhimg.com/v2-53a86a7aa9a29623d0bc9952979f8bf8_1440w.jpg)

### 2.1、动态链接库

### ⑴、优点：

-   (1).让编译后的目标文件比较小，因为不用每一次使用都复制一份。
-   (2).动态链接库允许在不用重新编译的情况下更换（升级）使用的动态链接库，只要更换前后的接口保持一直就行。
-   (3).由于动态库可以在运行时被程序载入，这样可以实现一个二进制插件系统（二进制插件系统的运行机制就是运行时载入插件）

### ⑵、缺点：

-   在运行时加载需要的函数以及变量需要额外的一些开销（开销比较小）

### 2.2、静态链接库

### ⑴、缺点：

-   增加了所有使用改库的程序编译后的目标文件

### ⑵、优点：

-   (1). 因为需要的代码都在编译的时候拷贝到目标文件中了，在做程序移植的时候不需要环境里有这些库
-   (2). 没有额外运行时的载入开销

**⚠️ 注意**

> 可以优先使用动态库；但在编译的目标文件中有许多可能难以满足的外部依赖项（比如c++标准库的特定版本或Boost c++库的特定版本）时，则使用静态库

## 二、Android.mk详细解析

在上面《一.1、Android.mk的基本格式》中虽然我进行了详细的注释，但是有必要继续分类说明

### 1、LOCAL\_PATH := $(call my-dir)

-   每个Android.mk文件必须以定义LOCAL\_PATH为开始。它用于在开发tree中查找源文件。宏my-dir 则由Build System提供。返回包含Android.mk的目录路径（即包含Android.mk file文件的目录）。
-   call 是调用一个系统提供的宏函数，此处是 my-dir
-   $() 是取值
-   := 是赋值

### 2、include $(CLEAR\_VARS)

CLEAR\_VARS 变量由Build System提供。并指向一个指定的GNU Makefile，由它负责清理很多LOCAL\_xxx.

例如：LOCAL\_MODULE, LOCAL\_SRC\_FILES, LOCAL\_STATIC\_LIBRARIES等等。但不清理LOCAL\_PATH.

**⚠️ 注意**

> 这个清理动作是必须的，因为所有的编译控制文件由同一个GNU Make解析和执行，其变量是全局的。所以清理后才能避免相互影响。

[版权声明CopyRight：](https://zhuanlan.zhihu.com/p/80668416)

> 本内容作者：sunst0069，转载或引用请[标明出处](https://www.zhihu.com/people/qydq)，违者追究法律责任！！！

### 3、LOCAL\_SRC\_FILES :=$(call all-subdir-java-files)

`LOCAL_SRC_FILES`变量代表需要编译的文件，`all-subdir-java-files`函数返回`LOCAL_PATH`子目录中的所有java文件。也可以直接写出需要编译的文件路径：

```
LOCAL_SRC_FILES :=src/com/sunst/hong/MainActivity.java \
                  src/com/sunst/hong/Test1.java \
                  src/com/sunst/hong/Test2.java
```

但要注意，在文件最后面加上以下语句，指明 LOCAL\_PATH 目录：

```
include $ (call all-makefiles-under,$(LOCAL_PATH))
```

或者在每个文件路径下都加上 LOCAL\_PATH：

```
LOCAL_SRC_FILES :=$(LOCAL_PATH)/src/com/sunst/hong/MainActivity.java \
                  $(LOCAL_PATH)/src/com/sunst/hong/Test1.java \
                  $(LOCAL_PATH)/src/com/sunst/hong/Test2.java
```

### 3.1、几个常用的获取源文件的方法

-   $(call all-java-files-under, src) ：获取指定目录下的所有 Java 文件
-   $(call all-c-files-under, src) ：获取指定目录下的所有 C 语言文件
-   $(call all-Iaidl-files-under, src) ：获取指定目录下的所有 AIDL 文件
-   $(call all-makefiles-under, folder)：获取指定目录下的所有 Make 文件

### 4、LOCAL\_SRC\_FILES := hello.c (针对以上《二.3、LOCAL\_SRC\_FILES》附加补充)

`LOCAL_SRC_FILES`变量代表需要编译的文件，必须包含将要打包如模块的C/C++源码。不必列出头文件，build System 会自动帮我们找出依赖文件

**⚠️ 注意**

> 缺省的C++源码的扩展名为.cpp. 也可以修改，通过LOCAL\_CPP\_EXTENSION

### 5、LOCAL\_MODULE := Bgwan

LOCAL\_MODULE模块必须定义，以表示Android.mk中的每一个模块。名字必须唯一且不包含空格。Build System会自动添加适当的前缀和后缀。

例如，Bgwan，要产生动态库，则生成libbgwan.so.

**⚠️ 注意**

> 如果模块名被定为：libbgwan.则生成libbgwan.so. 不再加前缀

### 5.1、LOCAL\_MODULE\_PATH :=$([TARGET\_ROOT\_OUT](https://zhida.zhihu.com/search?content_id=239279427&content_type=Article&match_order=1&q=TARGET_ROOT_OUT&zhida_source=entity))

`LOCAL_MODULE_PATH`用于设置指定最后生成的模块的目标路径

-   TARGET\_ROOT\_OUT:根文件系统，路径为out/target/product/generic/root
-   TARGET\_OUT（默认）:system文件系统，路径为out/target/product/generic/system
-   TARGET\_OUT\_DATA:data文件系统，路径为out/target/product/generic/data
-   [TARGET\_OUT\_DATA\_APPS](https://zhida.zhihu.com/search?content_id=239279427&content_type=Article&match_order=1&q=TARGET_OUT_DATA_APPS&zhida_source=entity)，这样配置生成的apk就会放到data/app目录下

除了以上，NDK还提供了很多其他的TARGET\_XXX\_XXX变量，用于将生成的模块拷贝到输出目录的不同路径

### 6、include $(BUILD\_SHARED\_LIBRARY)

`BUILD_SHARED_LIBRARY`是Build System提供的一个变量，指向一个GNU Makefile Script 它负责收集自从上次调用 include $(CLEAR\_VARS) 后的所有LOCAL\_XXX信息。并决定编译成什么

-   BUILD\_STATIC\_LIBRARY ：编译为静态库，静态库不会复制到的APK包中，但是能够用于编译共享库。这将会生成一个名为 lib$(LOCAL\_MODULE).a 的文件
-   BUILD\_SHARED\_LIBRARY ：编译为动态库（也叫共享库）
-   BUILD\_EXECUTABLE ：编译为Native C可执行程序
-   BUILD\_PREBUILT ：该模块已经预先编译
-   BUILD\_PACKAGE ：编译为APK

除了以上，NDK还定义了很多其他的BUILD\_XXX\_XXX变量，它们用来指定模块的生成方式。

### 7、LOCAL\_MODULE\_TAGS := optional

`LOCAL_MODULE_TAGS`用于设置编译的标签，常用的有：debug, eng, user, tests, development 或者 optional（默认）

-   user: 指该模块只在user版本下才编译
-   eng: 指该模块只在eng版本下才编译
-   tests: 指该模块只在tests版本下才编译
-   optional:指该模块在所有版本下都编译
-   development:指该模块在开发版本下编译

### 8、LOCAL\_CERTIFICATE := platform

`LOCAL_CERTIFICATE`用于设置签名属性、常用的有： _platform：该 APK 完成一些系统的核心功能。经过对系统中存在的文件夹的访问测试_ shared：该APK需要和 home/contacts 进程共享数据 \* media：该APK是 media/download 系统中的一环

### 9、LOCAL\_STATIC\_JAVA\_LIBRARIES := jar1 jar2

`LOCAL_STATIC_JAVA_LIBRARIES`用于定义需要引用静态jar库；jar1、jar2 是第三方Java包的别名，需要定义，在后面《三.16、源码环境下APK引用jar》会详细说明

**⚠️ 注意**

> 对应的是`LOCAL_JAVA_LIBRARIES`，用于引用动态jar

### 10、LOCAL\_STATIC\_JAVA\_AAR\_LIBRARIES := aar\_alias

`LOCAL_STATIC_JAVA_AAR_LIBRARIES`用于定义需要引用的静态aar库；aar和jar的区别是aar含有resource

**⚠️ 注意**

> 对应的是上面`《二.9、LOCAL_JAVA_LIBRARIES》`，用于引用静态库

### 11、需要进行预编译的库

```
LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES := jar1:path1 \
                                        jar2:path2
```

**⚠️ 注意**

> jar1、jar2 定义静态库别名，path1、path2 是静态库的路径，注意要一直写到后缀 .jar

**p.s. LOCAL\_PREBUILT\_STATIC\_JAVA\_LIBRARIES中导入aar（详见《三.1、Android.mk案例实战》中进行补充说明）：**

```
#声明AAR
include $(CLEAR_VARS)
LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES += aar_alias:libs/aar-release_1.0.aar
include $(BUILD_MULTI_PREBUILT)
#引用AAR
LOCAL_STATIC_JAVA_AAR_LIBRARIES := aar_alias
```

### 12、include $(BUILD\_MULTI\_PREBUILT)

拷贝到本地编译，将prebuild 定义的库拷到本地进行编译

### 13、GNU Make系统变量

收集一些上面没讲到的系统变量

-   TARGET\_ARCH //目标CPU平台的名字
-   TARGET\_ARCH\_ABI //暂时只支持两个 value，armeabi 和 armeabi-v7a
-   TARGET\_PLATFORM //Android.mk 解析的时候，目标 Android 平台的名字
-   TARGET\_ABI //目标平台和 ABI 的组合
-   LOCAL\_LDLIBS //编译模块时要使用的附加的链接器选项
-   LOCAL\_ARM\_MODE: 默认情况下， arm目标二进制会以 thumb 的形式生成(16 位)，你可以通过设置这个变量为 arm如果你希望你的 module 是以 32 位指令的形式
-   LOCAL\_CFLAGS: 可选的编译器选项，在编译C代码文件的时候使用

## 三、Android.mk案例实战

### 2023-09-12：新增`《项目目录结构》`使文章更清晰；（需要再修改，编译APK）

为了便于内容理解，新增本内容；该目录结构同样适用于本神的<**Android.bp解析与使用看这篇就够了[(点我跳转)](https://zhuanlan.zhihu.com/p/22242264/)**\>这篇文章

以源码Google源码：路径为：`uCar\Android\packages\apps\Gallery`的Gallery

```
Gallery
    res
    assets
    src
    tests
    libs
    Android.bp
    Android.mk
    AndroidManifest.xml
```

aar包 li.aar aar包 ：ba.aar 三方jar包 audioeffectservice.jar

**⚠️ 注意**

-   **Android应用级项目中目录结构是有区别的，应用级APP目录结构如下图**

![](https://pic4.zhimg.com/v2-d90b68fb7756e7727790c0d080420fb5_1440w.jpg)

-   li,ba为个人[livery框架](https://zhuanlan.zhihu.com/p/599703996)aar库（这里只是示例作用，并不适用于车载或其它智能设备）
-   audioeffectservice为我们公司的一个恢复音效的jar
-   当然以上目录结构只是参考，因为我见过很奇怪的目录结果，如

```
XXXServer
    app
      service
          main
            java
              com
                xxx
          test

    res
    assets
    src
    tests
    libs
    Android.bp
    Android.mk
    AndroidManifest.xml
```

### 1、Android.mk中导入AAR

aar包是Android studio下打包android工程中src、res、lib后生成的aar文件，在其它工程（如vender/目录下，或者的应用级APP项目）中引用aar后，其他工程可以方便引用源码和资源文件

### 1.1、源码环境apk引用aar

在Android源码环境下使用Android.mk的方式把aar导入apk，步骤如下：

### ⑴、先声明aar包的位置

```
include $(CLEAR_VARS)
LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES += aar_alias:libs/aar-release_1.0.aar//前面是别名，后面是路径
include $(BUILD_MULTI_PREBUILT)
```

面的代码整段都需要，而不只是中间那一行

### ⑵、引用我们声明的aar变量

```
LOCAL_STATIC_JAVA_AAR_LIBRARIES := aar_alias
```

### ⑶、添加引用的aar包里面的资源

```
LOCAL_AAPT_FLAGS += \
         --auto-add-overlay \
         --extra-packages com.sunst.bgwan
```

**⚠️ 注意**

> ⑵和⑶是添加到`include $(CLEAR_VARS)`和`include $(BUILD_MULTI_PREBUILT)`中间

### 1.2、如果遇到运行时找不到so的解决方案

源码下使用Android.mk的方式编译，编译出的apk是不含so文件的，也就意味着如果你adb install 编译出来的apk，它是不能按预期运行的；我们真实的一个案例就是（百度的CarLife aar)

**⚠️ 注意**

> 如果aar中含有so文件的话，用Android Studio构建应用级APP，so会打包到apk的lib目录下（可以反编译解压后看到）

### ⑴、推荐解决方案(预置apk + 预置aar 里的so 到/system/lib64/ 目录):

使用Android.mk的方式在源码下编译，大概率也是要预置这个apk了，可以在编译apk的mk文件中增加预置so文件的代码。当然，首先要把aar 里的so文件解压出来（集成交付可能不太友好，除了更新aar 还要再一次把aar 里的so 解压出来）

```
#  jni so
include $(CLEAR_VARS)
LOCAL_MODULE := libGet_Point-jni
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := libs/$(LOCAL_MODULE).so
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_MODULE_SUFFIX := $(suffix $(LOCAL_SRC_FILES))
LOCAL_SHARED_LIBRARIES := liblog libxt_get_point
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MULTILIB := 64
include $(BUILD_PREBUILT)
```

最后添加app 依赖的代码

```
# jni so
LOCAL_JNI_SHARED_LIBRARIES := libGet_Point-jni
```

so 文件将会被编译预置到/system/lib64/ 目录下，将apk push 到/system/app/AarTest/ 目录下验证可以运行。

**⚠️ 注意**

> so 放到 /vendor/lib64/ 下面访问不到（估计需要app 编译声明为VENDOR app），如果app 是install 安装的，那也没有权限访问/system/lib64/ 下面的so

### 1.3、Android Studio编译后AAR与JAR存储的路径（可选）

在Android Studio中对一个自己库进行生成操作时将会同时生成_.jar与_.aar文件

-   jar：库/build/intermediates/bundles/debug(release)/classes.jar
-   aar：库/build/outputs/aar/livery.aar

**⚠️ 注意**

-   关于如何打包AAR和JAR，和Android应用级APP中如何如何引用AAR的内容，可以阅读本神的**[(点我跳转)](https://zhuanlan.zhihu.com/p/22242264/)**这篇文章
-   关于如何发布AAR到maven中，作为公共库，也可以阅读本神的**[(点我跳转)](https://zhuanlan.zhihu.com/p/22351830)**这篇文章<发布库AAR至mavenCentral看这篇文章就可以了>

### 2、编译静态库

Android应用程序不能直接使用静态库，但是静态库可以用来编译成动态库。比如在将第三方代码添加到原生项目中时， 可以不用直接将第三方源码包含在原生项目中，而是将第三方源码编译成静态库，然后并入共享库

```
LOCAL_PATH := $(call my-dir) 
include $(CLEAR_VARS) 
LOCAL_MODULE = libhellos 
LOCAL_CFLAGS =$(L_CFLAGS) 
LOCAL_SRC_FILES = hellos.c 
LOCAL_C_INCLUDES = $(INCLUDES) 
LOCAL_SHARED_LIBRARIES := libcutils 
LOCAL_COPY_HEADERS_TO := libhellos 
LOCAL_COPY_HEADERS := hellos.h 
include $(BUILD_STATIC_LIBRARY)
LOCAL_PATH := $(call my-dir)

# 第三方库AVI
include $(CLEAR_VARS)
LOCAL_MODULE := AVI
LOCAL_SRC_FILES := AVI.c
include $(BUILD_STATIC_LIBRARY)

#原生模块
include $(CLEAR_VARS)
LOCAL_MODULE := module
LOCAL_SRC_FILES := module.c
#将静态库模块名添加到LOACAL_STATIC_LIBRARIES变量
LOCAL_STAITC_LIBRAYIES := AVI
include $(BUILD_SHARED_LIBRARY)
```

### 3、编译动态库

```
LOCAL_PATH := $(call my-dir) 
include $(CLEAR_VARS) 
LOCAL_MODULE = libhellod 
LOCAL_CFLAGS = $(L_CFLAGS) 
LOCAL_SRC_FILES = hellod.c 
LOCAL_C_INCLUDES = $(INCLUDES) 
LOCAL_SHARED_LIBRARIES := libcutils 
LOCAL_COPY_HEADERS_TO := libhellod 
LOCAL_COPY_HEADERS := hellod.h 
include $(BUILD_SHARED_LIBRARY)
```

### 4、编译多个共享库

一个Android.mk可能编译产生多个共享库模块，如下产生了libmodule1.so 和 libmodule2.so两个库

```
LOCAL_PATH := $(call my-dir)

#模块1
include $(CLEAR_VARS)
LOCAL_MODULE := module1
LOCAL_SRC_FILES := module1.c
include $(BUILD_SHARED_LIBRARY)

#模块2
include $(CLEAR_VARS)
LOCAL_MODULE := module2
LOCAL_SRC_FILES := module2.c
include $(BUILD_SHARED_LIBRARY)
```

### 5、使用/引用静态库

LOCAL\_STATIC\_LIBRARIES += libxxxxx

```
LOCAL_PATH := $(call my-dir) 
include $(CLEAR_VARS) 
LOCAL_MODULE := hellos 
LOCAL_STATIC_LIBRARIES := libhellos \libbgwan
LOCAL_SHARED_LIBRARIES := 
LOCAL_LDLIBS += -ldl 
LOCAL_CFLAGS := $(L_CFLAGS) 
LOCAL_SRC_FILES := mains.c 
LOCAL_C_INCLUDES := $(INCLUDES) 
include $(BUILD_EXECUTABLE)
```

### 6、使用/引用动态库

LOCAL\_SHARED\_LIBRARIES += libxxxxx

```
LOCAL_PATH := $(call my-dir) 
include $(CLEAR_VARS) 
LOCAL_MODULE := hellod 
LOCAL_MODULE_TAGS := debug 
LOCAL_SHARED_LIBRARIES := libc libcutils libhellod 
LOCAL_LDLIBS += -ldl 
LOCAL_CFLAGS := $(L_CFLAGS) 
LOCAL_SRC_FILES := maind.c 
LOCAL_C_INCLUDES := $(INCLUDES) 
include $(BUILD_EXECUTABLE)
```

### 7、使用/引用第三方文件

LOCAL\_LDFLAGS:=-L/PATH -Lxxx第三方库文件 LOCAL\_C\_INCLUDES :=path//第三方头文件

```
LOCAL_LDFLAGS := $(LOCAL_PATH)/lib/libtest.a
LOCAL_C_INCLUDES = $(INCLUDES)
```

### 8、使用共享库共享通用模块

静态库可以保证源代码模块化，但是当静态库与共享库相连时，它就变成了共享库的一部分。在多个共享库的情况下， 多个共享库与静态库连接时，需要将通用模块的多个副本与不同的共享库重复相连，这样就增加了app的大小，这种 情况，可以将通用模块作为共享库。

```
LOCAL_PATH := $(call my-dir)

# 第三方库AVI
include $(CLEAR_VARS)
LOCAL_MODULE := AVI
LOCAL_SRC_FILES := AVI.c
include $(BUILD_SHARED_LIBRARY)

#原生模块1
include $(CLEAR_VARS)
LOCAL_MODULE := module1
LOCAL_SRC_FILES := module1.c
LOCAL_SHARED_LIBRARIES := AVI
include $(BUILD_SHARED_LIBRARY)

#原生模块2
include $(CLEAR_VARS)
LOCAL_MODULE := module2
LOCAL_SRC_FILES := module2.c
LOCAL_SHARED_LIBRARIES := AVI
include $(BUILD_SHARED_LIBRARY)
```

### 9、拷贝文件到指定目录

```
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := bt_vendor.conf
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/etc/bluetooth
LOCAL_MODULE_TAGS := eng
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
```

### 10、拷贝动态库到指定目录

```
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
#the data or lib you want to copy
LOCAL_MODULE := libxxx.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(ANDROID_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := lib/$(LOCAL_MODULE )
OVERRIDE_BUILD_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)
```

### 11、多个NDK项目间共享模块

1.首先将AVI源代码移动到NDK项目以外的位置 2.作为共享模块，AVI需要有自己的Android.mk模块 3.以transcode/avilib为参数调用函数宏import-module添加到NDK项目的Android.mk文档末尾

```
#AVI模块自己的Android.mk文件
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := AVI
LOCAL_SRC_FILES := AVI.c
include $(BUILD_SHARED_LIBRARY)

#使用共享模块的NDK项目1的Android.mk文件
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := module1
LOCAL_SRC_FILES := module1.c
LOCAL_SAHRED_LIBRARIES := AVI
include $(BUILD_SHARED_LIBRARY)
$(call import-module, transcode/AVI)

#使用共享模块的NDK项目2的Android.mk文件
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := module2
LOCAL_SRC_FILES := module2.c
LOCAL_SAHRED_LIBRARIES := AVI
include $(BUILD_SHARED_LIBRARY)
$(call import-module, transcode/AVI)
```

### 12、使用预编译库

1.想在不发布源代码的情况下降模块发布给他人 2.想使用共享模块的预编译版来加速编译过程

```
#使用预编译共享模块的Android.mk文件
LOCAL_PATH := $(call my-dir)
#第三方预编译库
include $(CLEAR_VARS)
LOCAL_MODULE := AVI
LOCAL_SRC_FILES := libAVI.so
include $(PREBUILD_SHARED_LIBRARY)
```

### 13、编译独立的可执行文件

为了方便测试和进行快速开发，可以编译成可执行文件。不用打包成apk就可以复制到Android设备上直接执行

```
#独立可执行模块的Andriod.mk文件
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := module
LOCAL_SRC_FILES := module.c
LOCAL_STAITC_LIBRAYIES := AVI
include $(BUILD_EXECUTABLE)
```

### 14、编译apk

### 14.1、默认情况

不指定apk生成目录时，默认的目录为 system/app/{LOCAL\_PACKAGE\_NAME}/{LOCAL\_PACKAGE\_NAME}.apk；比如下面列子`LOCAL_PACKAGE_NAME`为Hong，这样生成的apk目录为

> system/app/Hong/Hong.apk

```
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(call all-subdir-java-files)
# 生成Hong apk
LOCAL_PACKAGE_NAME := Hong
include $(BUILD_PACKAGE)
```

### 14.2、指定目录

可以通过 LOCAL\_MODULE\_PATH 来配置，比如，我们想指定生成的 aok 目录为 system/vendor/sunst/Hong，我们可以这样配置

```
LOCAL_MODULE_PATH := $(TARGET_OUT)/vendor/sunst/Hong
LOCAL_MODULE_PATH := $(TARGET_OUT)/priv-app//这样可以放到system/priv-app下面
LOCAL_PRIVILEGED_MODULE := true//这样也可以将apk放到system/priv-app下面
```

$(TARGET\_OUT) 代表`/system`，详见上面《二、5.1、LOCAL\_MODULE\_PATH》 ,最后在system/vendor/sunst/Hong可以看到我们生成的apk

### 15、编译jar包

```
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(call all-subdir-java-files)
# 生成 hello
LOCAL_MODULE := hello
# 编译生成静态jar包
include $(BUILD_STATIC_JAVA_LIBRARY)
#编译生成共享jar
include $(BUILD_JAVA_LIBRARY)
```

**⚠️ 注意**

-   include $(BUILD\_STATIC\_JAVA\_LIBRARY) // 态jar包，使用.class文件打包而成的JAR文件，可以在任何java虚拟机运行
-   include $(BUILD\_JAVA\_LIBRARY) // 在静态jar包基础之上使用.dex打包而成的jar文件，.dex是android系统使用的文件格式

### 16、源码环境下APK引用jar

```
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
#静态jar包
LOCAL_STATIC_JAVA_LIBRARIES := static-library
#动态jar包
LOCAL_JAVA_LIBRARIES := share-library

LOCAL_SRC_FILES := $(call all-subdir-java-files)
LOCAL_PACKAGE_NAME := hello
include $(BUILD_PACKAGE)
```

### 16.1、引用单个JAR

当前目录下的libs有CommonUtil.jar jar包，引用它，需要两个步骤

### ⑴、声明我们jar包所在的目录

```
include $(CLEAR_VARS)
LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES := CommonUtil:/libs/CommonUtil.jar 
include $(BUILD_MULTI_PREBUILT)
```

### ⑵、引用我们声明jar包的变量

引用我们上面声明的 CommonUtil

```
LOCAL_STATIC_JAVA_LIBRARIES := CommonUtil
```

### 16.2、引用多个JAR

引用多个jar包的方式其实跟引用一个jar包的方式是一样的，语法有点区别而已

### ⑴、声明我们jar包所在的目录

```
include $(CLEAR_VARS)

LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES := CloudHelper:/libs/CommonUtil.jar \
                                        BaiduLBS:/libs/BaiduLBS_Android.jar \
                                        logger:/libs/logger.jar
include $(BUILD_MULTI_PREBUILT)
```

### ⑵、引用我们声明jar包的变量

```
LOCAL_STATIC_JAVA_LIBRARIES := CommonUtil \
                               BaiduLBS \
                               logger
```

### 17、预编译jar包

```
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
#指定编译生成的文件类型
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE := hello
LOCAL_SRC_FILES :=  $(call all-subdir-java-files)
# 预编译
include $(BUILD_PREBUILT)
```

**⚠️ 注意：`LOCAL_MODULE_CLASS`预编译文件类型有：**

-   JAVA\_LIBRARIES // dex归档文件
-   APPS // APK文件
-   SHARED\_LIBRARIES // 动态库文件
-   EXECUTABLES // 二进制文件
-   ETC // 其他文件格式

### 18、Android.mk中判断语句

ifeq/ifneq：根据判断条件执行相关编译

```
ifeq($(VALUE), x)   #ifneq
  do_yes
else
  do_no
endif
```

### 19、开启混淆

```
LOCAL_PROGUARD_ENABLED := disabled
```

**⚠️ 注意**

> 在应用级APP的开发中，build.gradle也有类似的配置

### 20、指定资源目录

```
#指定src目录
LOCAL_SRC_FILES := $(call all-java-files-under, src)
# 指定res目录
LOCAL_RESOURCE_DIR += $(LOCAL_PATH)/res
```

### 21、引用so库

可以结合以上《三.1、Android.mk中导入AAR》内容一起理解

假如我们当前目录下的 lib 目录下 有 armeabi-v7a，arm64-v8a 目录，里面分别有 libBaiduMapSDK\_base\_v4\_2\_1.so， libBaiduMapSDK\_base\_v4\_2\_1.so 。如果我们在编译 apk 的时候，想把这些so库打包进去，可以按照以下两种写法配置Android.mk

### 21.1、直接配置

### ⑴、直接在 mk 文件中配置以下内容，配置我们 so 库文件的所在位置

```
#(1)、配置我们的base_v4
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libBaiduMapSDK_base_v4_2_1
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES_arm :=libs/armeabi-v7a/libBaiduMapSDK_base_v4_2_1.so
LOCAL_SRC_FILES_arm64 :=libs/arm64-v8a/libBaiduMapSDK_base_v4_2_1.so
LOCAL_MODULE_TARGET_ARCHS:= arm arm64
LOCAL_MULTILIB := both
include $(BUILD_PREBUILT)

#(2)、配置我们的map_v4
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libBaiduMapSDK_map_v4_2_1
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES_arm :=libs/armeabi-v7a/libBaiduMapSDK_map_v4_2_1.so
LOCAL_SRC_FILES_arm64 :=libs/arm64-v8a/libBaiduMapSDK_map_v4_2_1.so
LOCAL_MODULE_TARGET_ARCHS:= arm arm64
LOCAL_MULTILIB := both
include $(BUILD_PREBUILT)
```

### ⑵、引用我们的so库

```
LOCAL_REQUIRED_MODULES := libBaiduMapSDK_base_v4_2_1 \
                          libBaiduMapSDK_map_v4_2_1 \

LOCAL_JNI_SHARED_LIBRARIES := libBaiduMapSDK_base_v4_2_1\
                              libBaiduMapSDK_map_v4_2_1\
include $(BUILD_PACKAGE)
```

### 21.2、声明后配置

在单独的mk（so库文件的配置独立到 mk 文件）中将so库的声明提取出来，再引用，跟第一种方法类似。这里推荐使用第二种方法，毕竟更符合面向对象的思维，以后复用以比较方便

### ⑴、baidumap.mk新文件

```
#(1)、配置我们的base_v4

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libBaiduMapSDK_base_v4_2_1
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES_arm :=libs/armeabi-v7a/libBaiduMapSDK_base_v4_2_1.so
LOCAL_SRC_FILES_arm64 :=libs/arm64-v8a/libBaiduMapSDK_base_v4_2_1.so
LOCAL_MODULE_TARGET_ARCHS:= arm arm64
LOCAL_MULTILIB := both
include $(BUILD_PREBUILT)

#(2)、配置我们的map_v4
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libBaiduMapSDK_map_v4_2_1
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES_arm :=libs/armeabi-v7a/libBaiduMapSDK_map_v4_2_1.so
LOCAL_SRC_FILES_arm64 :=libs/arm64-v8a/libBaiduMapSDK_map_v4_2_1.so
LOCAL_MODULE_TARGET_ARCHS:= arm arm64
LOCAL_MULTILIB := both
include $(BUILD_PREBUILT)
```

### ⑵、将lib/baidumap.mk文件inclue进来

在原来的 Android.mk 文件中增加以下语句，表示将 /lib/baidumap.mk 文件 include 进来

```
include $(LOCAL_PATH)/lib/baidumap.mk
```

### ⑶、引用我们的so库

```
include $(CLEAR_VARS)
# 其它省略
LOCAL_REQUIRED_MODULES := libBaiduMapSDK_base_v4_2_1 \
                          libBaiduMapSDK_map_v4_2_1 \

LOCAL_JNI_SHARED_LIBRARIES := libBaiduMapSDK_base_v4_2_1\
                              libBaiduMapSDK_map_v4_2_1\
include $(BUILD_PACKAGE)
```

### 22、Android.mk文件配置签名

build/target/product/security 目录中有四组默认签名供，Android.mk在编译APK使用

-   testkey：普通APK，默认情况下使用
-   platform：该APK完成一些系统的核心功能。经过对系统中存在的文件夹的访问测试 这种方式编译出来的APK所在进程的UID为system
-   shared：该APK需要和home/contacts进程共享数据
-   media：该APK是media/download系统中的一环

系统中所有使用android.uid.system作为共享UID的APK，都会首先在manifest节点中增加android:sharedUserId="android.uid.system"，然后在Android.mk中增加（可以参见Settings）

```
LOCAL_CERTIFICATE := platform
```

系统中所有使用android.uid.shared作为共享UID的APK，都会在manifest节点中增加android:sharedUserId="android.uid.shared"，然后在Android.mk中增加（可以参见Launcher）

```
LOCAL_CERTIFICATE := shared
```

系统中所有使用android.media作为共享UID的APK，都会在manifest节点中增加android:sharedUserId="android.media"，然后在Android.mk中增加（可以参见Gallery）

```
LOCAL_CERTIFICATE := media
```

## 四、总结：重要的注意事项

请根据实际项目使用和理解，因为比较容易出错，所以单独拎出来

### 1、**重要的注意事项，Android.mk可以引用Android.bp中的模块，反之Android.bp不能引用Android.mk中的模块**

### 2、**Android.bp模块，不支持../../去寻找上层路径的文件，只支持本目录下的folder里面的文件，如libs/Bgwan.aar**

### 3、假如我们本地库libhello-jni.so依赖于libTest.so(可以使用NDK下的ndk-depends查看so的依赖关系)

### 4、在Android 6.0版本之前，需要在加载本地库前，先加载被依赖的so

```
System.loadLibrary("Test");
System.loadLibrary("hello-jni");
```

### 5、在Android6.0版本之后，不能再使用预编译的动态库(静态库没问题)

```
System.loadLibrary("hello-jni");
```

## 致谢（引用和推荐）（可选）

本文参考借鉴了以下文章部分内容，非常感谢各位前辈的开源精神，当代互联网的发展离不开你们的分享，再次感谢 .同时以下↓↓↓，也是本神推荐阅读系列

-   \-\[x\] **[#\*官网对Android.mk的介绍](https://link.zhihu.com/?target=https%3A//developer.android.com/ndk/guides/android_mk%3Fhl%3Dzh-cn/)**
-   \-\[x\] **[\*\*Android.mk文件使用解析](https://link.zhihu.com/?target=https%3A//blog.csdn.net/wjky2014/article/details/131693042)**
-   \-\[x\] **[\*\*Android.mk介绍](https://link.zhihu.com/?target=https%3A//blog.csdn.net/reuxfhc/article/details/103288973)**
-   \-\[x\] **[\*\*Android.mk详解](https://link.zhihu.com/?target=http%3A//www.taodudu.cc/news/show-3377978.html%3Faction%3DonClick)**
-   \-\[x\] **[\*\*Android.mk解析与使用](https://link.zhihu.com/?target=https%3A//blog.csdn.net/hejnhong/article/details/120585740)**
-   \-\[x\] **[\*\*Android.mk 语法和变量介绍](https://link.zhihu.com/?target=https%3A//blog.csdn.net/tunmengsmile/article/details/118328061)**
-   \-\[x\] **[\*\*Android组件化，全面掌握](https://link.zhihu.com/?target=https%3A//juejin.cn/post/6881116198889586701)**

## @[©LICENSE](https://zhuanlan.zhihu.com/p/80668416)（版权和更新记录）

请尊重劳动成果，注意文中[版权声明](https://zhuanlan.zhihu.com/p/80668416)，[Android专栏](https://zhuanlan.zhihu.com/qyddai)不定时更新，☀️欢迎关注我的知乎Bgwan**[(点我跳转)](https://www.zhihu.com/people/qydq)**。也可以同时关注[人工智能专栏](https://zhuanlan.zhihu.com/sstai)，[文艺语录专栏](https://zhuanlan.zhihu.com/xiaoyue?author=qydq)，技术上沟通可以在qyddai@gmail.com或知乎上留言

> 2023-09-11:添加`《一.7、MakeFile文件编写补充》` 2023-09-12:新增`《三.1、Android.mk中导入AAR》`↑↑↑`**→**`《项目目录结构》`使文章更清晰</br> 2023-09-08：修改`《一.2.4、小节\_源码路径》`**→**`<⑴、在线浏览源码的地址>\`，目前Android最新版本

知乎是一个不错的平台，对于技术类的内容，不像CSDN需要付费阅读；当然整理本内容我也会花费了不少时间和精力，尤其是现在有了小baby要照顾；本着技术类分享精神，如果你觉得本文对你所帮助，你也可以分享给更多的同学，[或支持一下＃(香)](https://link.zhihu.com/?target=https%3A//bgwan.oss-cn-shanghai.aliyuncs.com/Beautiful_Life/Wealth_Manage/Images/Award_Pay.png)。授人以渔，不如授人以渔，开源和技术分享才能促进科技的进步

作者：sunst0069

发布日期：2023-06-09 14:38；更新日期：2024-01-20；维护次数：4
