接下来我们仿造振动器写一个简单的 AIDL HAL 模块。

## AIDL 文件编写

首先，在 `hardware/interfaces/` 路径下创建 aidl hal 项目目录：

```bash
cd hardware/interfaces
mkdir hello_aidl_hal
cd hello_aidl_hal
mkdir -p android/hardware/hello
```

接着我们在 `hardware/interfaces/hello_aidl_hal/aidl/android/hardware/hello` 目录下创建 IHelloHal.aidl 文件：

```java
// hardware/interfaces/hello_aidl_hal/aidl/android/hardware/hello/IHelloHal.aidl
package android.hardware.hello;

@VintfStability 
interface IHelloHal {
    void hello_write(String str);
    String hello_read();
}
```

接着编写最外层的 Android.bp：

```yaml
// hardware/interfaces/hello_aidl_hal/aidl/Android.bp
aidl_interface {
    name: "android.hardware.hello",
    vendor_available: true,
    srcs: ["android/hardware/hello/*.aidl"],
    stability: "vintf",
    owner:"xxx.hello",
    backend: {
        cpp: {
            enabled: false,
        },
        java: {
            sdk_version: "module_current",
        },
    },
}
```

aidl 会生成 java c++ ndk rust 四种库，一般 java 和 ndk 使用会比较多。这部分内容的细节可以在 backend 中配置。

接着就可以编译了：

```bash
mmm hardware/interfaces/hello_aidl_hal/
```

报错：

```bash
[  7% 186/2361] echo "API dump for the current version of AIDL interface android.hardware.hello does not exist." && echo "Run the co
FAILED: out/soong/.intermediates/hardware/interfaces/hello_aidl_hal/aidl/android.hardware.hello-api/checkapi_current.timestamp
echo "API dump for the current version of AIDL interface android.hardware.hello does not exist." && echo "Run the command \"m android.hardware.hello-update-api\" or add \"unstable: true\" to the build rule for the interface if it does not need to be versioned" && false # hash of input list: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
API dump for the current version of AIDL interface android.hardware.hello does not exist.
Run the command "m android.hardware.hello-update-api" or add "unstable: true" to the build rule for the interface if it does not need to be versioned
15:28:12 ninja failed with: exit status 1
```

按提示执行：

```sql
m android.hardware.hello-update-api
```

执行完成后，生成的代码结构如下：

```ruby
zzh0838@zzh0838-2204:~/Project/aosp/android-14.0.0_r15$ tree hardware/interfaces/hello_aidl_hal/
hardware/interfaces/hello_aidl_hal/
└── aidl
    ├── aidl_api
    │   └── android.hardware.hello
    │       └── current
    │           └── android
    │               └── hardware
    │                   └── hello
    │                       └── IHelloHal.aidl
    ├── android
    │   └── hardware
    │       └── hello
    │           └── IHelloHal.aidl
    └── Android.bp
```

接着再编译：

```bash
mmm hardware/interfaces/hello_aidl_hal/
```

执行完成后，就会在 `out/soong/.intermediates/hardware/interfaces/hello_aidl_hal/aidl` 目录下生成一下的一堆库：

```kotlin
android.hardware.hello-api        android.hardware.hello-V1-java-source  default
android.hardware.hello_interface  android.hardware.hello-V1-ndk
android.hardware.hello-V1-java    android.hardware.hello-V1-ndk-source
```

## 服务端程序代码编写

接下来在 `hardware/interfaces/hello_aidl_hal/aidl/default` 目录下创建如下的文件：

![20240414095841](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7ed3d98aa6c54f7889d184868039549e~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=570&h=281&s=22133&e=png&b=191919)

`hardware/interfaces/hello_aidl_hal/aidl/default/HelloHalImpl.h` 实现如下：

```cpp
#ifndef ANDROID_HARDWARE_HELLO_H
#define ANDROID_HARDWARE_HELLO_H

#include <aidl/android/hardware/hello/BnHelloHal.h>

namespace aidl::android::hardware::hello {

class HelloHalImpl : public BnHelloHal {
  public:
        ::ndk::ScopedAStatus hello_write(const std::string& in_str) override;

        ::ndk::ScopedAStatus hello_read(std::string* _aidl_return) override;
};

}  

#endif
```

`hardware/interfaces/hello_aidl_hal/aidl/default/HelloHalImpl.cpp` 的实现如下：

```cpp
#define LOG_TAG "HELLO-HAL"
#define LOG_NDEBUG 0
#include <iostream>

#include <log/log.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <aidl/android/hardware/hello/IHelloHal.h>

#include <stdio.h>
using aidl::android::hardware::hello::IHelloHal;

int main() {
    std::shared_ptr<IHelloHal> service = IHelloHal::fromBinder(ndk::SpAIBinder(AServiceManager_getService("android.hardware.hello.IHelloHal/default")));
    ALOGD("get service = %p\n",service.get());

    if (service == nullptr) {
        return -1;
    }
    service->hello_write("hello");
    fflush(stdout);
    return EXIT_FAILURE;  // should not reach
}
```

可以看出，这里的 HelloHalImpl 就是 binder 服务端类的具体实现。

接着写一个主程序，来注册这个 binder 服务:

`hardware/interfaces/hello_aidl_hal/aidl/default/main.cpp`

```cpp
AIDL 文件编写#include "HelloHalImpl.h"
#define LOG_TAG "HelloHalImpl"
#define LOG_NDEBUG 0
#include <iostream>

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <stdio.h>
#include <log/log.h>

using aidl::android::hardware::hello::HelloHalImpl;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<HelloHalImpl> hello = ::ndk::SharedRefBase::make<HelloHalImpl>();

    const std::string instance = std::string() + HelloHalImpl::descriptor + "/default";

    ALOGD("HelloHalImpl instance =%s  sde =%p \n", instance.c_str(), hello->asBinder().get());

    binder_status_t status =
          AServiceManager_addService(hello->asBinder().get(), instance.c_str());

    CHECK_EQ(status, STATUS_OK);
    ALOGD("HelloHalImpl AServiceManager_addService status=%d \n", status);

    fflush(stdout);
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
```

接着还要写一个 hal 的 vintf：

`hardware/interfaces/hello_aidl_hal/aidl/default/hellohal-default.xml`

```php-template
<manifest version="1.0" type="device">
    <hal format="aidl" optional="true">
        <name>android.hardware.hello</name>
        <version>1</version>
        <interface>
        <name>IHelloHal</name>
        <instance>default</instance>
    </interface>
    </hal>
</manifest>
```

需要开机启动，添加:

`hardware/interfaces/hello_aidl_hal/aidl/default/android.hardware.hello.rc`

```swift
service vendor.hellohal-default /vendor/bin/hw/android.hardware.hello.example
    class hal
    user root
    group root
```

接着编写 `hardware/interfaces/hello_aidl_hal/aidl/default/Android.bp`

```bash
cc_binary {
    name: "android.hardware.hello.example",
    relative_install_path: "hw",
    vendor: true,
    init_rc: ["android.hardware.hello.rc"],
    vintf_fragments: ["hellohal-default.xml"],
    shared_libs: [
        "android.hardware.hello-V1-ndk", 
        "liblog",
        "libbase",
        "libcutils",
        "libutils",
        "libbinder_ndk",
    ],
    srcs: [
        "main.cpp",
        "HelloHalImpl.cpp",
    ],
}
```

接着还需要将 hal 加入到兼容性矩阵中：

`hardware/interfaces/compatibility_matrices/compatibility_matrix.8.xml`

```php-template
<hal format="aidl" optional="true">
        <name>android.hardware.hello</name>
        <version>1</version>
        <interface>
        <name>IHelloHal</name>
        <instance>default</instance>
    </interface>
    </hal>
```

在 Android14 中还会多一个叫 fuzzer 的东西，暂时还没搞清楚作用，看源码大部分 hal 都没有做支持，这里我们写的 hal 也不做支持，修改 `system/sepolicy/build/soong/service_fuzzer_bindings.go` :

```csharp
"android.hardware.vibrator.IVibrator/default":                             EXCEPTION_NO_FUZZER,
"android.hardware.vibrator.IVibratorManager/default":                      []string{"android.hardware.vibrator-service.example_fuzzer"},
        // 添加的内容
"android.hardware.hello.IHelloHal/default":                         EXCEPTION_NO_FUZZER,
"android.hardware.weaver.IWeaver/default":                                 EXCEPTION_NO_FUZZER,
```

## Selinux

接下来配置 Selinux 权限：

添加 `system/sepolicy/vendor/hal_hello.te`：

```scss
hal_attribute(hellohal);
type hal_hellohal_default, domain, mlstrustedsubject;
hal_server_domain(hal_hellohal_default, hal_hellohal);
type hal_hellohal_default_exec, exec_type, vendor_file_type, file_type;
init_daemon_domain(hal_hellohal_default);

binder_call(hal_hellohal_client, hal_hellohal_default)

type hal_hellohal_service, service_manager_type;
add_service(hal_hellohal_default, hal_hellohal_service)
allow hal_hellohal_client hal_hellohal_service:service_manager find;
hal_client_domain(system_server, hal_hellohal)

allow hal_hellohal_default servicemanager:binder { call transfer };
allow {  platform_app shell } hal_hellohal:binder {call};

allow hal_hellohal_default hello_dev_t:chr_file { open read write };
```

在 `system/sepolicy/vendor/file_contexts` 中添加：

```bash
/(vendor|system/vendor)/bin/hw/android\.hardware\.hello\.example          u:object_r:hal_hellohal_default_exec:s0
```

修改 `system/sepolicy/private/service_contexts` 和 `system/sepolicy/prebuilts/api/34.0/private/service_contexts`，注意保持两个文件一致：

```bash
android.hardware.hello.IHelloHal/default                           u:object_r:hal_hellohal_service:s0
```

## 编写测试程序

在 `hardware/interfaces/hello_aidl_hal` 目录下创建：

![20240414104625](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a398926baacd4ec188723ca665396143~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=295&h=131&s=7116&e=png&b=181818)

`hardware/interfaces/hello_aidl_hal/test_aidl_hal/main.cpp`:

```cpp
#define LOG_TAG "Test-HAL"
#define LOG_NDEBUG 0
#include <iostream>

#include <log/log.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <aidl/android/hardware/hello/IHelloHal.h>

#include <stdio.h>
using aidl::android::hardware::hello::IHelloHal;

int main() {
    std::shared_ptr<IHelloHal> service = IHelloHal::fromBinder(ndk::SpAIBinder(AServiceManager_getService("android.hardware.hello.IHelloHal/default")));
    ALOGD("get service = %p\n",service.get());

    if (service == nullptr) {
        return -1;
    }
    service->hello_write("hello");
    fflush(stdout);
    return EXIT_FAILURE;  // should not reach
}
```

`hardware/interfaces/hello_aidl_hal/test_aidl_hal/Android.bp`:

```bash
cc_binary {
    name: "test_aidl_hal",
    vendor: true,
    shared_libs: [
        "android.hardware.hello-V1-ndk", 
        "liblog",
        "libbase",
        "libcutils",
        "libutils",
        "libbinder_ndk",
    ],
    srcs: [
        "main.cpp",
    ],
}
```

## 编译与测试

```bash
# 编译
source build/envsetup.sh
lunch aosp_cf_x86_phone-eng
m
# 启动模拟器
cvd start  -kernel_path=/home/zzh0838/Project/aosp/kernel/out/virtual_device_x86_64/dist/bzImage  -initramfs_path=/home/zzh0838/Project/aosp/kernel/out/virtual_device_x86_64/dist/initramfs.img
```

最后测试，

运行测试程序：

```
adb shell
test_aidl_hal
```

查看 log：

```csharp
adb logcat | grep hello
04-11 11:13:31.414     0     0 W         : drivers/char/hello_driver.c hello_init line 69
04-11 11:13:32.803     0     0 I init    : Parsing file /vendor/etc/init/android.hardware.hello.rc...
04-11 11:13:37.533     0     0 I init    : starting service 'vendor.hellohal-default'...
04-11 11:13:37.537     0     0 I init    : ... started service 'vendor.hellohal-default' has pid 339
04-11 11:13:37.604     0     0 I servicemanager: Found android.hardware.hello.IHelloHal/default in device VINTF manifest.
04-11 11:20:11.512   339   339 D HelloHalImpl: write hello
```

## 参考资料

-   [Type Statements](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2FSELinuxProject%2Fselinux-notebook%2Fblob%2Fmain%2Fsrc%2Ftype_statements.md%23expandattribute "https://github.com/SELinuxProject/selinux-notebook/blob/main/src/type_statements.md#expandattribute")
-   [hal深入剖析之aidl实战-android framework车机车载手机系统开发](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Flearnframework%2Farticle%2Fdetails%2F134945726 "https://blog.csdn.net/learnframework/article/details/134945726")
-   [AIDL for HALs实战](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fqq_40731414%2Farticle%2Fdetails%2F126823262 "https://blog.csdn.net/qq_40731414/article/details/126823262")
