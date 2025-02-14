[AndroidQ 打通应用层到HAL层—(HAL模块实现)](https://blog.csdn.net/qq_34211365/article/details/105492973)这篇文章中我们已经实现了自己的HAL，本篇我们实现一个HIDL服务，通过这个服务来调用HAL模块的函数

### 什么是HIDL

HIDL 全称为HAL interface definition language（发音为“hide-l”）是用于指定 HAL 和其用户之间的接口的一种接口描述语言 (IDL)，Android O开始引入了HIDL这个概念，HIDL和应用层AIDL差不多，AIDL常用于连接App和Framework，HIDL则是用来连接Framework和HAL，AIDL使用Binder通信，HIDL则使用HwBinder通信，他们都是通过Binder驱动完成通信，只不过两个Binder域不一样

### 为什么需要HIDL

目前Android系统生态是几乎每年google都会出一个Android大版本，而普通手机用户一部手机一般要用两三年，所以你会发现尽管Android系统已经升级到了10，马上11出来了，然后还是有很多用户依然使用的是Android 5，6，7等版本，对普通用户来说如果不更换手机就很难跟上Android版本，这是因为OEM厂商在同一设备上进行系统升级需要花费时间金钱成本很高，导致他们不愿意升级，成本高的原因是Android O之前Android Framework的升级需要OEM将HAL也进行对应升级，Framework和HAL是一起被编译成system.img，它们存在高耦合，针对这种情况google在Android O中引入了Treble计划，Treble的目的就是解耦Framework和HAL，就是通过HIDL来实现，Framework不再直接调用HAL，而是通过HIDL来间接使用HAL模块，每个HAL模块都可以对应一个HIDL服务，Framework层通过HwBinder创建HIDL服务，通过HIDL服务来获取HAL相关模块继而打开HAL下的设备，而最终HAL也从system.img中分离，被编进一个单独的分区vendor.img，从而简化了Android系统升级的影响与难度

### HIDL的使用

HIDL可以分为：HIDL C++(C++实现)、HIDL Java(Java 实现)，并且还主要分为直通式和绑定式，本篇文章使用的C++和直通式的HIDL，HIDL用起来非常简单，AOSP的hardware/interfaces/目录下有很多的HIDL，我们仿照其他HIDL创建自己的HIDL目录：**hardware/interfaces/hello\_hidl/1.0**

并在此目录下创建一个IHello.hal文件：

```
package android.hardware.hello_hidl@1.0;

interface IHello {

    addition_hidl(uint32_t a,uint32_t b) generates (uint32_t total);
    
};
```

这个文件定义了一个addition\_hidl函数，这个函数用来调用HAL的加法函数

然后就可以使用Android提供的工具hidl-gen来生成HIDL框架，执行如下命令：

```
 PACKAGE=android.hardware.hello_hidl@1.0
 LOC=hardware/interfaces/hello_hidl/1.0/default/
 hidl-gen -o $LOC -Lc++-impl -randroid.hardware:hardware/interfaces -randroid.hidl:system/libhidl/transport $PACKAGE
 hidl-gen -o $LOC -Landroidbp-impl -randroid.hardware:hardware/interfaces -randroid.hidl:system/libhidl/transport $PACKAGE
```

执行命令成功之后我们会发现hardware/interfaces/hello\_hidl/1.0下多了一个default目录，进入default目录，里面有三个文件Android.bp，Hello.cpp，Hello.h  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6944b84d0fdf2ad343699604129b64e3.png)  
之后再在执行./hardware/interfaces/update-makefiles.sh这个命令，update-makefiles.sh这个脚本目的是为HIDL生成对应Android.bp文件

最后目录结构为：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/358a7dfcb0cbf56010f32aa9f3e3b615.png)

接着我们还需要在default目录下增加一个空文件service.cpp，用作注册HIDL服务，我们采用直通式的HIDL，所以service.cpp的内容为：

```
#include <android/hardware/hello_hidl/1.0/IHello.h>
#include <hidl/LegacySupport.h>
// Generated HIDL files
using android::hardware::hello_hidl::V1_0::IHello;
using android::hardware::defaultPassthroughServiceImplementation;

int main() {
    return defaultPassthroughServiceImplementation<IHello>();
}
```

defaultPassthroughServiceImplementation函数最终会向HwServiceManager注册HIDL服务

接着我们来看看之前生成的文件，首先看Hello.h

```
// FIXME: your file license if you have one

#pragma once

#include <android/hardware/hello_hidl/1.0/IHello.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

namespace android {
namespace hardware {
namespace hello_hidl {
namespace V1_0 {
namespace implementation {
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

struct Hello : public IHello {
    // Methods from ::android::hardware::hello_hidl::V1_0::IHello follow.
    Return<uint32_t> addition_hidl(uint32_t a, uint32_t b) override;

    // Methods from ::android::hidl::base::V1_0::IBase follow.

};

// FIXME: most likely delete, this is only for passthrough implementations
//去掉注释
extern "C" IHello* HIDL_FETCH_IHello(const char* name);

}  // namespace implementation
}  // namespace V1_0
}  // namespace hello_hidl
}  // namespace hardware
}  // namespace android
```

系统自动生成了Hello结构体（当然也可以自己改为class），继承IHello接口，addition\_hidl函数就需要在Hello.cpp中去实现了，因为我们采用直通式HIDL，所以需要将// extern “C” IHello\* HIDL\_FETCH\_IHello(const char\* name);的注释去掉

接着来看看Hello.cpp：

```
// FIXME: your file license if you have one

#include "Hello.h"
#include <utils/Log.h>
namespace android {
namespace hardware {
namespace hello_hidl {
namespace V1_0 {
namespace implementation {

// Methods from ::android::hardware::hello_hidl::V1_0::IHello follow.
Return<uint32_t> Hello::addition_hidl(uint32_t a, uint32_t b) {
    // TODO implement
    ALOGE("hello_hidl service is init success....a :%d,b:%d",a,b);
    return uint32_t {};
}
// Methods from ::android::hidl::base::V1_0::IBase follow.

IHello* HIDL_FETCH_IHello(const char* /* name */) {
    return new Hello();
}
}  // namespace implementation
}  // namespace V1_0
}  // namespace hello_hidl
}  // namespace hardware
}  // namespace android
```

同样需要去掉HIDL\_FETCH\_IHello函数的注释，采用直通式HIDL时，通过前面service.cpp中的defaultPassthroughServiceImplementation函数注册HIDL服务时，内部原理就是通过“HIDL\_FETCH\_”字串拼接defaultPassthroughServiceImplementation传递的IHello，找到HIDL\_FETCH\_IHello函数并获取IHello对象，我们可以看到HIDL\_FETCH\_IHello初始代码就是创建了一个Hello对象

在接着看default目录下的Android.bp：

```
cc_library_shared {
    name: "android.hardware.hello_hidl@1.0-impl",
    relative_install_path: "hw",
    proprietary: true,
    srcs: [
        "Hello.cpp",
    ],
    shared_libs: [
        "libhidlbase",
        "libhidltransport",
        "libutils",
        "android.hardware.hello_hidl@1.0",
        "liblog",
        "libutils",
    ],
}
```

这个Android.bp会将Hello这个HIDL服务编译成一个android.hardware.hello\_hidl@1.0-impl.so，它还依赖一个android.hardware.hello\_hidl@1.0.so，这个so哪来的呢？

再接着看1.0目录下的Android.bp：

```
// This file is autogenerated by hidl-gen -Landroidbp.
hidl_interface {
    name: "android.hardware.hello_hidl@1.0",
    root: "android.hardware",
    vndk: {
        enabled: true,
    },
    srcs: [
        "IHello.hal",
    ],
    interfaces: [
        "android.hidl.base@1.0",
    ],
    gen_java: true,
}
```

这个Android.bp会将hardware/interfaces/hello\_hidl/1.0这个HIDL编译成一个android.hardware.hello\_hidl@1.0.so，到这里我们发现service.cpp没有用到，所以我们还需要修改default目录下的Android.bp：

```
// FIXME: your file license if you have one

cc_library_shared {
    name: "android.hardware.hello_hidl@1.0-impl",
    relative_install_path: "hw",
    proprietary: true,
    srcs: [
        "Hello.cpp",
    ],
    shared_libs: [
        "libhidlbase",
        "libhidltransport",
        "libutils",
        "liblog",
        "libhardware",
        "android.hardware.hello_hidl@1.0",
        "liblog",
        "libutils",
    ],
}
cc_binary {
    name: "android.hardware.hello_hidl@1.0-service",
    defaults: ["hidl_defaults"],
    relative_install_path: "hw",
    vendor: true,
    srcs: ["service.cpp"],
    shared_libs: [
        "android.hardware.hello_hidl@1.0",
        "libhardware",
        "libhidlbase",
        "libhidltransport",
        "libutils",
        "liblog",
    ],

}

```

新增加对service.cpp的编译，我们将service.cpp编译成一个二进制可执行文件android.hardware.hello\_hidl@1.0-service.so，用来启动HIDL服务，好了，最终我们这个HIDL会编译出来如下三个so:  
android.hardware.hello\_hidl@1.0-impl.so  
android.hardware.hello\_hidl@1.0.so  
android.hardware.hello\_hidl@1.0-service.so

还有一点需要注意的是，这个HIDL想要被Framework获取使用还需要在manifest.xml中注册，  
manifest.xml在手机vendor/etc/vintf/manifest.xml下，我们将这个文件pull出来然后添加如下代码：

```
   <hal format="hidl">
        <name>android.hardware.hello_hidl</name>
        <transport>hwbinder</transport>
        <version>1.0</version>
        <interface>
            <name>IHello</name>
            <instance>default</instance>
        </interface>
        <fqname>@1.0::IHello/default</fqname>
    </hal>
```

然后在Hello.cpp中添加一行log，之后进行编译

```
IHello* HIDL_FETCH_IHello(const char* /* name */) {

    ALOGE("hello_hidl service is init success....");
    
    return new Hello();
}
```

执行mmm hardware/interfaces/hello\_hidl/1.0/  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b7f99f4501a7b6cff849077ecb55c64f.png)  
编译成功后我们将生成的三个so分别push到手机vendor/lib64/hw/，vendor/lib64/，vendor/bin/hw/目录下

[adb](https://so.csdn.net/so/search?q=adb&spm=1001.2101.3001.7020) push vendor/lib64/hw/android.hardware.hello\_hidl@1.0-impl.so vendor/lib64/hw/

adb push system/lib64/android.hardware.hello\_hidl@1.0.so vendor/lib64/

adb push vendor/bin/hw/android.hardware.hello\_hidl@1.0-service vendor/bin/hw/

接着我们到手机vendor/bin/hw/目录下去执行android.hardware.hello\_hidl@1.0-service这个二进制可执行文件，这个文件就会执行service.cpp的代码，调用defaultPassthroughServiceImplementation注册我们的HIDL服务

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f952a59b8447ce0b8739853ca2578a57.png)

再看看log输出：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/ada5947cd6f04d65d3f2c85dd0d847e4.png)  
在执行android.hardware.hello\_hidl@1.0-service时就会输入这句log，代表我们这个HIDL服务已经实现，其实通常的HIDL服务都是通过rc文件来开机启动的，我这里为了方便演示就没有写

再执行adb shell ps -A|grep -i --color "hello\_hidl"命令看下这个服务状态  
我们发现HIDL服务启动之后就会一直在后台，这个其实和AMS，WMS这种服务是类似的，启动之后在后台会等待client端访问  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/56de1333339e0d57a670bf9ff2f4de86.png)

HIDL这个服务已经能够正常启动了，接着写一个测试程序看能否获取这个服务，并且调用该服务的函数，我在Hello.cpp的addition\_hidl函数中添加了一句log：

```
Return<uint32_t> Hello::addition_hidl(uint32_t a, uint32_t b) {
    // TODO implement
    ALOGD("dongjiao...Hello::addition_hidl a = %d,b = %d",a,b);
    return uint32_t {};
}
```

测试程序写在hardware/interfaces/hello\_hidl/1.0/default目录下：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/a9b300112042edb40d391226c230e016.png)  
Hello\_hidl\_test.cpp：

```
#include <android/hardware/hello_hidl/1.0/IHello.h>
#include <hidl/LegacySupport.h> 
#include <log/log.h>

using android::sp;
using android::hardware::hello_hidl::V1_0::IHello;
using android::hardware::Return;
int main(){
    android::sp<IHello> hw_device = IHello::getService();
    if (hw_device == nullptr) {
              ALOGD("dongjiao...failed to get hello-hidl");
              return -1;
        }
    ALOGD("dongjiao...success to get hello-hidl....");
    Return<uint32_t> total = hw_device->addition_hidl(3,4);
    return 0;
}
```

测试程序代码也比较简单，获取IHello的服务，然后调用addition\_hidl函数

看一下Android.bp：

```
cc_binary {
    name: "Hello_hidl_test",
    srcs: ["Hello_hidl_test.cpp"],
    shared_libs: [
        "liblog",
        "android.hardware.hello_hidl@1.0",
        "libhidlbase",
        "libhidltransport",
        "libhwbinder",
        "libutils",
    ],
}

```

我们再编译这个测试程序，它会被编译成一个可执行二进制文件Hello\_hidl\_test  
编译命令：  
**mmm hardware/interfaces/hello\_hidl/1.0/default/test/**  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3fec22f3b461538917989814a99d2665.png)  
编译成功了，将这个可执行文件push到手机/system/bin/目录下

在执行Hello\_hidl\_test之前别忘了把HIDL服务启动起来  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d7b44c0574677b937bdd182480a74a65.png)  
接着执行Hello\_hidl\_test  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/96c8dbc4ed2921070d501cceb30b082e.png)  
然后看log输出  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/1f4004c28a566053fdcd050226eddedd.png)  
可以看到成功了，成功获取到了HIDL服务并且调用了该服务的addition\_hidl函数，将我们的参数传递了过去，实际开发中就可以在获取HIDL服务时打开HAL模块，然后可以直接调用HAL的函数，上一篇文章其实也写了测试程序测自定义的HAL模块，我们只需要将上一篇文章中的测试程序中的代码copy到HIDL初始化代码中就能够调用HAL的那个加法函数了，具体就不测试了

到这里HIDL服务已经成功实现并且能够正常使用，HIDL这个框架用起来还是比较简单的，大部分代码都是通过工具生成的

下一篇文章我将在native层定义一个JNI服务，然后在JNI服务中调用HIDL服务定义的addition\_hidl函数