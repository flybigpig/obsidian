       ![](https://lf-web-assets.juejin.cn/obj/juejin-web/xitu_juejin_web/img/banner.a5c9f88.jpg)

> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

## 属性是什么

在 Android 系统中，为统一管理系统的属性，设计了一个统一的属性系统，每个属性都是一个 key-value 对。 我们可以通过 shell 命令，Native 函数接口，Java 函数接口的方式来读写这些 key-vaule 对。 例如：可以在命令行通过命令获取属性

```
//命令
getprop dalvik.vm.dex2oat-Xmx
//结果
512m
```

## 属性服务架构

在 Android 平台中，为了让运行中的所有进程共享系统运行时所需要的各种设置值，系统开辟了属性存储区域，并提供了访问该区域的 API。属性由键(key)与值(value)构成,其表现形式为“键=值”在 Linux 系统中，属性服务主要用来设置环境变量，提供各进程访问设定的环境变量值'。在 Android 平台中，属性服务得到更系统地应用，在访问属性值时，添加了访问权限控制，增强了访问的安全性。系统中所有运行中的进程都可以访问属性值，但仅有 init 进程才能修改属性值。其他进程修改属性值时，必须向 init 进程提出请求，最终由 init 进程负责修改属性值。在此过程中，init 进程会先检查各属性的访问权限，而后再修改属性值。当属性值更改后，若定义在 init.rc 文件中的某个特定条件得到满足，则与此条件相匹配的动作就会发生。每个动作都有一个“触发器”(trigger)，它决定动作的执行时间，记录在“on property”关键字后的命令即被执行。

下图图简单地描述了 init 进程与其它进程在访问并修改属性值的大致情形:

![架构图.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/1feaf69ec1f04251832adc4146dcff83~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240014&x-signature=CHh%2Fz7waKHA1gxzwzo8eLP4a0RA%3D)

## 属性服务相关代码

封装的API接口

```
//c语言接口
system/core/include/cutils/properties.h
system/core/libcutils/properties.cpp

int8_t property_get_bool(const char *key, int8_t default_value);
int64_t property_get_int64(const char *key, int64_t default_value);
int property_get(const char* key, char* value, const char* default_value);
int32_t property_get_int32(const char *key, int32_t default_value);
int property_set(const char *key, const char *value);

//c++语言接口
system/core/base/include/android-base/properties.h
system/core/base/properties.cpp

// 获取 bool 类型的属性值,如果属性为空或者不存在， 返回参数 2 的默认值
bool GetBoolProperty(const std::string& key, bool default_value);
template <typename T>
//获取 int 类型的属性值， 如果属性为空或不存在，并且不在最小值和最大值之间，返回参数 2 的默认值
T GetIntProperty(const std::string& key, T default_value, T min, T max);
//获取 unsignd int 类型的属性值，如果属性为空或不存在，并且不在 0 和最大值之间，返回参数 2 的默认值
template <typename T>
T GetUintProperty(const std::string& key, T default_value, T max);
// 获取默认类型( 字符串)属性值
std::string GetProperty(const std::string& key, const std::string& default_value);
//设置属性值
bool SetProperty(const std::string& key, const std::string& value);
#if defined(__BIONIC__)
// 等待属性的值变成预定值， relative_timeout 参数设置超时时间， 成功返回 true。
bool WaitForProperty(const std::string& key, const std::string& expected_value, std::chrono::milliseconds relative_timeout = std::chrono::milliseconds::max());

//Java接口
frameworks/base/core/java/android/os/SystemProperties.java

//核心接口实现
bionic/libc/bionic/system_property_api.cpp //服务端和客户端共同使用
bionic/libc/bionic/system_property_set.cpp //客户端使用
```

属性服务实现核心代码

```
//init进程中属性服务相关代码
system/core/init/property_service.cpp

//属性服务实现相关的工具模块
system/core/property_service/
├── Android.bp
├── OWNERS
├── libpropertyinfoparser //属性安全上下文解析
├── libpropertyinfoserializer//属性序列化工具
└── property_info_checker//属性格式等检查

//android标准C库中与properties相关实现
bionic/libc/include/sys/_system_properties.h
bionic/libc/system_properties/
```

## 属性服务初始化

属性服务的初始化过程非常复杂，之所以比较复杂是因为整个流程涉及到很多文件、描述数据的类，多种数据结构等，如果不能将这些文件、类、数据结构之间的关系搞清楚，分析代码的时候就会变得非常混乱，我绘制了如下图帮助大家简单高效理解属性服务初始化。

![数据流程架构.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/e9d8068b35d34aee84220e9bd96336fa~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240014&x-signature=GZW%2BYg7n3DCDk5HoUPB23vXEsEQ%3D)

属性服务初始化代码如下：

```
//system/core/init/init.cpp

int SecondStageMain(int argc, char** argv){
//创建/dev/__properties__/property_info，并对内容进行序列化
// 构建和映射所有属性的共享内存
property_init();
// 读取特定设备树中的信息，并设置 ro.boot 开头的属性， RK3399 上没有相关信息。
process_kernel_dt();
//将内核中 cmdline 中所有的有=号的参数，以及特殊的 androidboot.xx=xxx 的形式设置成相应的属性
//如果是模拟器, bootargs 中包含： androidboot.hardware=ranchu 和 init=/init
// 就会设置 ro.kernel.androidboot.hardware= ranchu ro.kernel.init=/init 和 ro.boot.hardware=ranchu, //如果是真机， 只针对 androidboot.xx=xx 的参数设置，
//如 androidboot.storagemedia=emmc, 就会有 root.boot.storagemedia=emmc
process_kernel_cmdline();
// 将列表中特定属性的值设置到另外一个属性的值中去
//如将 ro.boot.serialno 的值设置到 ro.serialno 中去
//一般 ro.boot 开头的属性很多都是来自内核的 cmdline
export_kernel_boot_props();
//加载各个分区中的属性文件，如 prop.default, build.pro， default.prop，之前详细讲过属性文件
property_load_boot_defaults(load_debug_prop);
//创建本地套接字， 用于接收客户端的设置请求，并将套接字文件描述符加入到 epoll 监控机制
//套接字有数据之后的处理函数是： handle_property_set_fd()
StartPropertyService(&epoll);
}
```

我将整个初始化内容分为如下几个部分:

-   数据模型构建阶段
    -   属性安全上下文序列化，上图所示左侧绿色框
    -   属性文件创建和mmap映射，上图所示右侧橙色框
-   属性值初始化阶段
-   属性服务socket创建

其中property\_init函数完成了整个数据模型构建过程

```
void property_init() {
    mkdir("/dev/__properties__", S_IRWXU | S_IXGRP | S_IXOTH);
    //实现属性安全上下文序列化
    CreateSerializedPropertyInfo();
    //属性文件创建和mmap映射
    if (__system_property_area_init()) {
        LOG(FATAL) << "Failed to initialize property area";
    }
}
```

属性值初始化阶段由如下几个函数实现

```
process_kernel_dt();
process_kernel_cmdline();
export_kernel_boot_props();
property_load_boot_defaults(load_debug_prop);
```

最后StartPropertyService创建属性服务socket。

参考：  
[blog.csdn.net/GDUYT\_gduyt…](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2FGDUYT_gduyt%2Farticle%2Fdetails%2F115526835 "https://blog.csdn.net/GDUYT_gduyt/article/details/115526835") [blog.canyie.top/2022/04/09/…](https://link.juejin.cn/?target=https%3A%2F%2Fblog.canyie.top%2F2022%2F04%2F09%2Fproperty-implementation-and-isolation%2F "https://blog.canyie.top/2022/04/09/property-implementation-and-isolation/")

![avatar](https://p26-passport.byteacctimg.com/img/user-avatar/bde1ef02645bf5d95c45e5b30bef749f~40x40.awebp)