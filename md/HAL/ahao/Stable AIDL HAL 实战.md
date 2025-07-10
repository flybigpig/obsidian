

#### 背景：

> 官方：[https://source.android.com/devices/architecture/aidl/aidl-hals.](https://source.android.com/devices/architecture/aidl/aidl-hals.)

Google 在Android 11引入了`AIDL for HALs`，旨在代替HIDL原先的作用。在之后的Android版本推荐使用AIDL 实现Hal层的访问。  
这样做的原因，应该有以下几点：

1.  AIDL比HIDL存在的时间更长（仅从Android 8到Android 10），并在许多其他地方使用，如Android框架组件之间或应用程序中。既然AIDL具有稳定性支持，就可以用单一的IPC方式从HAL到框架进程或者应用进程。
2.  AIDL还有一个比HIDL更好的版本控制系统。

再详细的展开说就是：

1.  AIDL 更成熟，使用更广泛，如果HAL层也使用了AIDL的方式，那么就可以直接从应用进程调用到HAL 进程，以前使用HIDL的时候实现应用进程访问HAL的服务，需要在system server进程的中介。来个图：  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/fa8929e4faff0ef5220e58b97e7ad390.png#pic_center)
2.  以前使用HIDL的方式，如果后续vendor HAL version需要迭代升级，那么就需要再创建一个子目录，过程中实际上做很多的重复工作，冗余而效率不高。

值得注意的是：在HAL 层使用AIDL必须使用`Stable AIDL`, 和我们在应用层或者框架层稍微不同，因为和vendor的接口设计要兼顾稳定性，system和vendor的更新速率不一样。

> HALs using AIDL to communicate between framework components must use Stable AIDL.

#### 使用AIDL for HALs

##### 1\. 定义HAL接口

创建对应的模块目录：`/hardware/interfaces/hongxi/aidl/`  
创建aidl文件：`/hardware/interfaces/hongxi/aidl/android/hardware/hongxi/IHongxi.aidl`

```shell
package android.hardware.hongxi;

@VintfStability
interface IHongxi {
    String getName();
    
    void setName(in String msg);
}
```

**每个类型定义都必须使用@VintfStability进行注释。**  
如果想要定义类型，参考同用的AIDL的定义就行，同时比通用的AIDL多了枚举、结构体、parcelable类型（注意这些类型跟Android版本有关，13以下的版本不一定有全）

##### 2\. 配置Android.bp

创建顶层Android.bp：`/hardware/interfaces/hongxi/aidl/Android.bp`

```shell
aidl_interface {
   name: "android.hardware.hongxi",
   vendor: true,
   srcs: ["android/hardware/hongxi/*.aidl"],
   stability: "vintf",
   owner: "hongxi.zhu",
   backend: {
       cpp: {
           enabled: false,
       },
       java: {
       	   enabled: false,
       },
   },
}
```

1.  backend: 服务的后端，AIDL支持四种后端，分别是C++/JAVA/NDK/RUST, 我们将使用NDK（谷歌推荐），因此将CPP和JAVA后端声明为false(实际上我试了这两个，编译有问题，还没解决，后续解决了更新出来)。
    
2.  为了方便测试，设置vendor:true并删除vendor\_available，因为这是一个自定义供应商HAL，删除vndk部分，因此这个HAL仅位于vendor分区，不受VNDK限制，真正开发中，如需开启需要自己解决VNDK的限制问题，这里就不单独列出。
    

##### 3\. 编译模块

```shell
mmm hardware/interfaces/hongxi/
```

然后就会报错：

```shell
[ 74% 227/303] echo "API dump for the current version of AIDL interface android.hardware.hongxi does not exist." && echo Run "m android.hardware.hongxi-update-api", or add "unstable: true" to the build ru
FAILED: out/soong/.intermediates/hardware/interfaces/hongxi/aidl/android.hardware.hongxi-api/checkapi_current.timestamp
echo "API dump for the current version of AIDL interface android.hardware.hongxi does not exist." && echo Run "m android.hardware.hongxi-update-api", or add "unstable: true" to the build rule for the inte
rface if it does not need to be versioned && false
API dump for the current version of AIDL interface android.hardware.hongxi does not exist.
Run m android.hardware.hongxi-update-api, or add unstable: true to the build rule for the interface if it does not need to be versioned
19:29:13 ninja failed with: exit status 1
```

原因是当前版本没有这个接口，需要更新下API，按照提示来：

```shell
m android.hardware.hongxi-update-api
```

然后再重新编译模块：

```shell
mmm hardware/interfaces/hongxi/
```

##### 4\. 实现HAL 接口

We will use the ndk\_platfrom library, therefore, let check the generated code for ndk\_platform.我们需要在实现的接口编译脚本中引用模块的`ndk_platfrom`, 且我们要实现的接口在编译时都生成了对应的源码，我们只需要拷贝出来并实现，所以先看下，刚才的编译都生成了什么：

```shell
cd out/soong/.intermediates/hardware/interfaces/hongxi/aidl/android.hardware.hongxi-ndk_platform-source
find .
```

> .  
> ./gen  
> ./gen/android  
> ./gen/android/hardware  
> ./gen/android/hardware/hongxi  
> ./gen/android/hardware/hongxi/IHongxi.cpp.d  
> ./gen/android/hardware/hongxi/IHongxi.cpp  
> ./gen/include  
> ./gen/include/aidl  
> ./gen/include/aidl/android  
> ./gen/include/aidl/android/hardware  
> ./gen/include/aidl/android/hardware/hongxi  
> ./gen/include/aidl/android/hardware/hongxi/BpHongxi.h  
> ./gen/include/aidl/android/hardware/hongxi/BnHongxi.h  
> ./gen/include/aidl/android/hardware/hongxi/IHongxi.h  
> ./gen/timestamp

在IHongxi.h头文件中找到我们要实现的接口：

```c++
  virtual ::ndk::ScopedAStatus getName(std::string* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus setName(const std::string& in_msg) = 0;
```

接下来就需要创建后端源码文件，来实现这些接口：  
/hardware/interfaces/hongxi/aidl/default/Hongxi.h

```c++
#pragma once

#include <aidl/android/hardware/hongxi/BnHongxi.h>

namespace aidl {
namespace android {
namespace hardware {
namespace hongxi {

class Hongxi : public BnHongxi {
    public:
        //String getName();
        ndk::ScopedAStatus getName(std::string* _aidl_return);

        //void setName(in String msg);
        ndk::ScopedAStatus setName(const std::string& in_msg);

    private:
        std::string name = "";
};

}  // namespace hongxi
}  // namespace hardware
}  // namespace android
}  // namespace aidl
```

/hardware/interfaces/hongxi/aidl/default/Hongxi.cpp

```c++
#define LOG_TAG "Hongxi"

#include <utils/Log.h>
#include <iostream>
#include "Hongxi.h"

namespace aidl {
namespace android {
namespace hardware {
namespace hongxi {

ndk::ScopedAStatus Hongxi::getName(std::string* _aidl_return) {

    *_aidl_return = name;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Hongxi::setName(const std::string& in_msg) {

    name = in_msg;

    return ndk::ScopedAStatus::ok();
}

}  // namespace hongxi
}  // namespace hardware
}  // namespace android
}  // namespace aidl
```

##### 5\. 实现服务：

/hardware/interfaces/hongxi/aidl/default/main.cpp

```c++
#define LOG_TAG "Hongxi"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include "Hongxi.h"

using aidl::android::hardware::hongxi::Hongxi;
using std::string_literals::operator""s;

int main() {
    // Enable vndbinder to allow vendor-to-venfor binder call
    android::ProcessState::initWithDriver("/dev/vndbinder"); //使用vnbinder的配置

    ABinderProcess_setThreadPoolMaxThreadCount(0); // vnbinder的线程池独立，需要单独配置 
    ABinderProcess_startThreadPool();

    std::shared_ptr<Hongxi> hongxi = ndk::SharedRefBase::make<Hongxi>();
    const std::string desc = Hongxi::descriptor + "/default"s;

    if (hongxi != nullptr) {
        if(AServiceManager_addService(hongxi->asBinder().get(), desc.c_str()) != STATUS_OK) {
            ALOGE("Failed to register IHongxi service");
            return -1;
        }
    } else {
        ALOGE("Failed to get IHongxi instance");
        return -1;
    }

    ALOGD("IHongxi service starts to join service pool");
    ABinderProcess_joinThreadPool();

    return EXIT_FAILURE;  // should not reached
}
```

##### 8\. 编写服务启动的rc脚本

/hardware/interfaces/hongxi/aidl/default/android.hardware.hongxi-service.rc

```shell
service android.hardware.hongxi-service /vendor/bin/hw/android.hardware.hongxi-service
        interface aidl android.hardware.hongxi.IHongxi/default
        class hal
        user system
        group system
```

##### 9\. 声明VINTF AIDL 接口

/hardware/interfaces/hongxi/aidl/default/android.hardware.hongxi-service.xml

```shell
<manifest version="1.0" type="device">
    <hal format="aidl">
        <name>android.hardware.hongxi</name>
        <fqname>IHongxi/default</fqname>
    </hal>
</manifest>
```

##### 7\. 编写服务构建脚本

/hardware/interfaces/hongxi/aidl/default/Android.bp

```shell
cc_binary {
    name: "android.hardware.hongxi-service",
    vendor: true,
    relative_install_path: "hw",
    init_rc: ["android.hardware.hongxi-service.rc"],
    vintf_fragments: ["android.hardware.hongxi-service.xml"],

    srcs: [
        "Hongxi.cpp",
        "main.cpp",
    ],

    cflags: [
        "-Wall",
        "-Werror",
    ],

    shared_libs: [
        "libbase",
        "liblog",
        "libhardware",
        "libbinder_ndk",
        "libbinder",
        "libutils",
        "android.hardware.hongxi-ndk_platform",
    ],
}
```

#### 将模块加入系统中

/build/target/product/base\_vendor.mk

```shell
# add for Hongxi
PRODUCT_PACKAGES += \
    android.hardware.hongxi \
    android.hardware.hongxi-service \

```

#### 将模块添加到兼容性矩阵中

```shell
# (选下标最新的那个)
hardware/interfaces/compatibility_matrices/compatibility_matrix.5.xml
#（这个不一定有，如果没有就不加）
hardware/interfaces/compatibility_matrices/compatibility_matrix.current.xml
```

```shell
    <hal format="aidl" optional="true">
        <name>android.hardware.hongxi</name>
        <version>1.0</version>
        <interface>
            <name>IHongxi</name>
            <instance>default</instance>
        </interface>
    </hal>
```

#### 解决Selinux权限

这个后续补充，测试中，会有宏版本和直接添加的版本

#### 客户端测试

1.  cpp-client(user process)
2.  apk-client(user process)
3.  SystemService-client(system server process)