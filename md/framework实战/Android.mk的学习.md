


##### Android Framework(二) – Android.mk的学习

```java
author:朱红喜 2020-10-11 
notice:该文件一直会更新->未完待续
```

###### 1\. `Android.mk`是什么

```java
+ Android.mk构建系统解析一次或多次的微小GNU makefile 片段，
+ 用于描述源代码中的构建规则，
+ 例如，要描述要构建的源文件、构建的输出格式、描述文件之间的依赖、库和包的导入等其他的业务。
```

###### 2\. 最简单的`Android.mk`组成

```shell
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := demo
LOCAL_SRC_FILES := demo.c
include $(BUILD_SHARED_LIBRARY)
```

+   **`LOCAL_PATH := $(call my-dir)`**  
    `Android.mk`文件必须最先声明这个变量，此变量表示源文件在开发树中的位置。调用构建系统提供的宏函数 `my-dir` 将返回当前目录（`Android.mk` 文件本身所在的目录）的路径。
    
+   **`include $(CLEAR_VARS)`**  
    声明`CLEAR_VARS`变量，其值由构建系统提供，用于清除许多 LOCAL\_XXX 变量，例如 `LOCAL_MODULE、LOCAL_SRC_FILES`和 `LOCAL_STATIC_LIBRARIES`。但`GNU Makefile`不会清除`LOCAL_PATH`。因为系统在单一`GNU Make` 执行上下文（其中的所有变量都是全局变量）中解析所有构建控制文件,如果清除，则上下文将不一定会指到当前文件。在描述每个模块之前，您必须重新声明此变量。
    
+   **`LOCAL_MODULE := demo`**  
    `LOCAL_MODULE` 变量存储您要构建的模块的名称。**请在应用的每个模块中使用一次此变量**,每个模块名称必须**唯一**，且不含任何空格,如果是构建共享库(`.so`),构建系统会自动在这个名称加上前缀`lib`和后缀`.so`(前提是这个名称原先没有前缀`lib`或者没有后缀`.so`)
    
+   **`LOCAL_SRC_FILES := demo.c`**  
    `LOCAL_SRC_FILES`变量是声明包含要构建到模块中的 `C`和`C++`源文件列表,如果包含多个文件 可用`\`分行声明
    
+   **`include $(BUILD_SHARED_LIBRARY)`**  
    此变量声明将源代码构建成什么形式的文件。`BUILD_SHARED_LIBRARY`表明声明是动态库(`.so`)
    

###### 3\. 常用的`include`变量

+   `CLEAR_VARS`  
    清除当前全局的LOCAL\_XXX变量，单不包含LOCAL\_PATH，在声明每个模块前必须声明这个变量，用法：
    
    ```shell
    include $(CLEAR_VARS)
    ```
    
+   `BUILD_EXECUTABLE`  
    将列出的源文件构建成目标可执行文件，用法：
    
    ```shell
    include $(BUILD_EXECUTABLE)
    ```
    
+   `BUILD_SHARED_LIBRARY`  
    根据您列出的源文件构建目标共享库。共享库变量会导致构建系统生成扩展名为 .so 的库文件。用法：
    
    ```shell
    include $(BUILD_SHARED_LIBRARY)
    ```
    
+   `BUILD_STATIC_LIBRARY`  
    根据列出的源文件构建目标静态库，构建系统生成扩展名为 .a 的静态库。用法：
    
    ```shell
    include $(BUILD_STATIC_LIBRARY)
    ```
    
+   `PREBUILT_SHARED_LIBRARY`  
    指向用于指定预构建共享库的构建脚本。与 BUILD\_SHARED\_LIBRARY 和 BUILD\_STATIC\_LIBRARY 的情况不同，这里的 LOCAL\_SRC\_FILES 值不能是源文件，而必须是指向预构建共享库的单一路径，例如 foo/libfoo.so。使用此变量的语法为：
    
    ```shell
    include $(PREBUILT_SHARED_LIBRARY)
    ```
    
+   `PREBUILT_STATIC_LIBRARY`  
    指向用于指定预构建静态库的构建脚本。
    

###### 4\. 模块描述变量

+   `LOCAL_PATH`  
    此变量用于指定当前文件的路径。必须在 Android.mk 文件开头定义此变量。CLEAR\_VARS 所指向的脚本不会清除此变量。因此，即使 Android.mk 文件描述了多个模块，您也只需定义此变量一次。用法：
    
    ```shell
    LOCAL_PATH := $(call my-dir)
    ```
    
+   `LOCAL_MODULE`  
    此变量用于指定模块名称。指定的名称在所有模块名称中必须唯一，并且不得包含任何空格。必须先定义该名称，然后才能添加任何脚本（`CLEAR_VARS`的脚本除外）。**无需添加 lib 前缀或 .so 或 .a 文件扩展名**；构建系统会自动执行这些修改。例如，以下行会导致生成名为`libfoo.so`的共享库模块：
    
    ```shell
    LOCAL_MODULE := "foo"
    ```
    
+   `LOCAL_MODULE_FILENAME`  
    此可选变量使您能够替换构建系统为其生成的文件默认使用的名称。例如，如果`LOCAL_MODULE`的名称为 `foo`，您可以强制系统将其生成的文件命名为 `libnewfoo`。用法：
    
    ```shell
    LOCAL_MODULE := foo
    LOCAL_MODULE_FILENAME := libnewfoo
    ```
    
    对于共享库模块，此示例将生成一个名为`libnewfoo.so` 的文件。  
    **注意：您无法替换文件路径或文件扩展名。**
    
+   `LOCAL_SRC_FILES`  
    此变量包含构建系统生成模块时所用的源文件列表。只列出构建系统实际传递到编译器的文件，因为构建系统会自动计算所有相关的依赖项。请注意，您可以使用相对（相对于 `LOCAL_PATH`）和绝对文件路径。
    
    ```shell
    LOCAL_SRC_FILES := a.c \
    					b.c
    ```
    
    建议避免使用绝对文件路径；相对路径可以提高 `Android.mk`文件的移植性。
    
+   `LOCAL_C_INCLUDES`  
    您可使用此可选变量指定相对于 root 目录的路径列表，以便在编译所有源文件（C、C++ 和 Assembly）时添加到 include 搜索路径中。例如：
    
    ```shell
    LOCAL_C_INCLUDES := sources/foo
    或：
    LOCAL_C_INCLUDES := $(LOCAL_PATH)/<subdirectory>/foo
    ```
    
+   `LOCAL_STATIC_LIBRARIES`  
    此变量用于存储当前模块依赖的静态库模块列表。  
    如果当前模块是共享库或可执行文件，此变量将强制这些库链接到生成的二进制文件。  
    如果当前模块是静态库，此变量只是指出依赖于当前模块的其他模块也会依赖于列出的库。
    
+   `LOCAL_SHARED_LIBRARIES`  
    此变量会列出此模块在运行时依赖的共享库模块。此信息是链接时必需的信息，用于将相应的信息嵌入到生成的文件中。
    
+   `LOCAL_LDLIBS`  
    此变量列出了在构建共享库或可执行文件时使用的额外链接器标记。利用此变量，您可使用 -l 前缀传递特定系统库的名称。例如，以下示例指示链接器生成在加载时链接到`/system/lib/libz.so` 的模块：
    
    ```shell
    LOCAL_LDLIBS := -lz
    ```
    
+   `LOCAL_STATIC_JAVA_LIBRARIES := static-library`  
    这个表示的这个`APK`所依赖的`jar`包，从上面的代码中可以看出jar包的名字是`static-library.jar`。
    
+   `LOCAL_SRC_FILES := $(call all-subdir-java-files)`  
    这个就是这个源文件，`all-subdir-java-files`说明编译这个目录下的所有`.java`文件，
    
+   `LOCAL_MODULE_TAGS := optional`  
    模块在什么版本的才编译的条件
    
    ```shell
    user:  指该模块只在user版本下才编译
    eng:  指该模块只在eng版本下才编译
    tests: 指该模块只在tests版本下才编译
    optional:指该模块在所有版本下都编译
    ```
    
+   `LOCAL_PACKAGE_NAME := Settings`  
    `package`的名字，这个名字在脚本中将标识这个`app`或`package`。
    
+   `LOCAL_CERTIFICATE := platform`  
    用于指定签名时使用的KEY，如果不指定，默认使用testkey，`LOCAL_CERTIFICATE`可设置的值如下：
    
    ```shell
     LOCAL_CERTIFICATE := platform
     LOCAL_CERTIFICATE := shared
     LOCAL_CERTIFICATE := media
    ```
    

###### 5\. `GNU Make` 构建系统提供的常用函数宏

+   `my-dir`  
    这个宏返回最后包括的 `makefile` 的路径，通常是当前 `Android.mk`的路径。`my-dir`可用于在 `Android.mk`文件开头定义`LOCAL_PATH`。例如：
    
    ```shell
    LOCAL_PATH := $(call my-dir)
    ```
    
    由于`GNU Make`的工作方式，这个宏实际返回的是构建系统解析构建脚本时包含的最后一个 `makefile` 的路径。因此，包括其他文件后就不应调用 `my-dir`。
    
+   `all-subdir-makefiles`  
    返回位于当前`my-dir` 路径所有子目录中的 `Android.mk` 文件列表。利用此函数，您可以为构建系统提供深度嵌套的源目录层次结构。
    
+   `this-makefile`  
    返回当前 `makefile`（构建系统从中调用函数）的路径。
    
+   `parent-makefile`  
    返回包含树中父 `makefile`的路径（包含当前 `makefile` 的 `makefile` 的路径）。
    
+   `grand-parent-makefile`  
    返回包含树中祖父 `makefile`的路径（包含当前父`makefile` 的 `makefile`的路径）。
    
+   `import-module`  
    此函数用于按模块名称来查找和包含模块的`Android.mk` 文件。典型的示例如下所示：
    
    ```shell
    $(call import-module,<name>)
    ```
    
    在此示例中，构建系统在当前环境变量所引用的目录列表中查找具有`<name>`标记的模块，并且自动包括其 `Android.mk` 文件。
    

###### 6.常用

```shell
#编译静态库    
LOCAL_PATH := $(call my-dir)    
include $(CLEAR_VARS)    
LOCAL_MODULE = libhellos    
LOCAL_CFLAGS = $(L_CFLAGS)    
LOCAL_SRC_FILES = hellos.c    
LOCAL_C_INCLUDES = $(INCLUDES)    
LOCAL_SHARED_LIBRARIES := libcutils    
LOCAL_COPY_HEADERS_TO := libhellos    
LOCAL_COPY_HEADERS := hellos.h    
include $(BUILD_STATIC_LIBRARY)    
    
#编译动态库    
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
    
#使用静态库    
LOCAL_PATH := $(call my-dir)    
include $(CLEAR_VARS)    
LOCAL_MODULE := hellos    
LOCAL_STATIC_LIBRARIES := libhellos    
LOCAL_SHARED_LIBRARIES :=    
LOCAL_LDLIBS += -ldl    
LOCAL_CFLAGS := $(L_CFLAGS)    
LOCAL_SRC_FILES := mains.c    
LOCAL_C_INCLUDES := $(INCLUDES)    
include $(BUILD_EXECUTABLE)    
    
#使用动态库    
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
  
  
#拷贝文件到指定目录  
LOCAL_PATH := $(call my-dir)  
include $(CLEAR_VARS)  
LOCAL_MODULE := bt_vendor.conf  
LOCAL_MODULE_CLASS := ETC  
LOCAL_MODULE_PATH := $(TARGET_OUT)/etc/bluetooth  
LOCAL_MODULE_TAGS := eng  
LOCAL_SRC_FILES := $(LOCAL_MODULE)  
include $(BUILD_PREBUILT)  
  
  
#拷贝动态库到指定目录  
LOCAL_PATH := $(call my-dir)  
include $(CLEAR_VARS)  
#the data or lib you want to copy  
LOCAL_MODULE := libxxx.so  
LOCAL_MODULE_CLASS := SHARED_LIBRARIES  
LOCAL_MODULE_PATH := $(ANDROID_OUT_SHARED_LIBRARIES)  
LOCAL_SRC_FILES := lib/$(LOCAL_MODULE )  
OVERRIDE_BUILD_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)  
include $(BUILD_PREBUILT) 

# 装载一个普通的第三方APK
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
# Module name should match apk name to be installed.
LOCAL_MODULE := Demo
LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_CERTIFICATE := platform
include $( BUILD_PREBUILT ) 
  
# 装载 需要.so（动态库）的第三方apk
LOCAL_PATH := $(my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := Demo
LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_CERTIFICATE := platform
include $( BUILD_PREBUILT )

# copy the library to /system/lib
include $(CLEAR_VARS)
LOCAL_MODULE := libinputcore.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := lib/$(LOCAL_MODULE)
OVERRIDE_BUILD_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $( BUILD_PREBUILT )
```

参考：[https://www.cnblogs.com/reality-soul/p/6893248.html](https://www.cnblogs.com/reality-soul/p/6893248.html)  
[https://developer.android.google.cn/ndk/guides/cpp-support](https://developer.android.google.cn/ndk/guides/cpp-support)