从这篇文章开始准备研究应用层到HAL层的一整套流程，目标是写一个APP调用HAL的一个函数，在AOSP源码环境下进行开发，大概流程是：  
APP---->Framework service---->[native](https://so.csdn.net/so/search?q=native&spm=1001.2101.3001.7020)\----->HAL

### 什么是HAL

HAL全称Hardware [Abstract](https://so.csdn.net/so/search?q=Abstract&spm=1001.2101.3001.7020) Layer，硬件抽象层，它向下屏蔽了硬件的实现细节，向上提供了抽象接口，HAL是底层硬件和上层框架直接的接口，框架层通过HAL可以操作硬件设备，HAL的实现在用户空间

### 为什么需要HAL

我们知道Android是基于Linux进行开发的，传统的Linux对硬件的操作基本上都是在内核中，而Android把对硬件的操作分为了两部分，HAL和内核驱动，HAL实现在用户空间，驱动在内核空间，这是因为Linux内核中的代码是需要开源的，如果把对硬件的操作放在内核这会损害硬件厂商的利益，因为这是别人厂商的商业机密，而现在有了HAL层位于用户空间，硬件厂商就可以将自己的核心算法之类的放在HAL层，保护了自己的利益，这样Android系统才能得到更多厂商的支持

### HAL实现的一般规则

每种硬件都对应了一个HAL模块，要向实现自己的HAL，必须满足HAL的相关规则，规则定义在源码hardward目录下，头文件hardward.h，C文件hardward.c  
hardward.h中定义了三个重要的结构体：

```
struct hw_module_t;
struct hw_module_methods_t;
struct hw_device_t;
```

其实HAL的实现使用了C中结构体继承的技巧，当然这种继承并不是真正意义的继承，而是一种结构体强制转换，我们可以理解为继承

结构体hw\_module\_t代表HAL模块，自己定义的HAL模块必须包含一个自定义struct，且必须”继承” hw\_module\_t（即第一个变量必须为 hw\_module\_t），且模块的tag必须指定为HARDWARE\_MODULE\_TAG，代表这是HAL模块的结构体

结构体hw\_module\_methods\_t代表模块的操作方法列表，它内部只有一个函数指针open，用来打开该模块下的设备

结构体hw\_device\_t代表该模块下的设备，自己定义的HAL模块必须包含一个结构体，且必须“继承” hw\_device\_t（即第一个变量必须为hw\_device\_t）

每个自定义HAL模块还有一个模块名和N个设备名（标识模块下的设备个数，一个模块可以有多个设备），

最后这个模块定义好之后还必须导出符号HAL\_MODULE\_INFO\_SYM，指向这个模块，HAL\_MODULE\_INFO\_SYM定义在hardware.h中值为“HMI”

接下来就手动实现一个HAL模块，这个HAL提供一个加法函数  
首先在hardware/libhardware/include/hardware目录下创建hello.h文件

```
#include <sys/cdefs.h>
#include <sys/types.h>
#include <hardware/hardware.h>
//HAL模块名
#define HELLO_HARDWARE_MODULE_ID "hello"
//HAL版本号
#define HELLO_MODULE_API_VERSION_1_0 HARDWARE_MODULE_API_VERSION(0, 1)
//设备名
#define HARDWARE_HELLO "hello"

//自定义HAL模块结构体
typedef struct hello_module {
    struct hw_module_t common;
} hello_module_t;

//自定义HAL设备结构体
typedef struct hello_device {
    struct hw_device_t common;
    //加法函数
    int (*additionTest)(const struct hello_device *dev,int a,int b,int* total);
} hello_device_t;

//给外部调用提供打开设备的函数
static inline int _hello_open(const struct hw_module_t *module,
        hello_device_t **device) {
    return module->methods->open(module, HARDWARE_HELLO,
            (struct hw_device_t **) device);
}

```

我们可以看到在hello.h中自定义了两个结构体，分别继承hw\_module\_t和hw\_device\_t且在第一个变量，满足前面说的规则，另外定义在device结构体中的函数的第一个参数也必须是device结构体

接着在hardware/libhardware/modules/目录下创建一个hello的文件夹，里面包含一个hello.c和Android.bp，我们来看hello.c文件

```
#define LOG_TAG "HelloHal"

#include <malloc.h>
#include <stdint.h>
#include <string.h>

#include <log/log.h>

#include <hardware/hello.h>
#include <hardware/hardware.h>

//加法函数实现
static int additionTest(const struct hello_device *dev,int a,int b,int *total)
{
if(!dev){
return -1;
}
    *total = a + b;
    return 0;
}

//关闭设备函数
static int hello_close(hw_device_t *dev)
{
    if (dev) {
        free(dev);
        return 0;
    } else {
        return -1;
    }
}
//打开设备函数
static int hello_open(const hw_module_t* module,const char __unused *id,
                            hw_device_t** device)
{
    if (device == NULL) {
        ALOGE("NULL device on open");
        return -1;
    }

    hello_device_t *dev = malloc(sizeof(hello_device_t));
    memset(dev, 0, sizeof(hello_device_t));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = HELLO_MODULE_API_VERSION_1_0;
    dev->common.module = (struct hw_module_t*) module;
    dev->common.close = hello_close;
    dev->additionTest = additionTest;

    *device = &(dev->common);
    return 0;
}

static struct hw_module_methods_t hello_module_methods = {
    .open = hello_open,
};
//导出符号HAL_MODULE_INFO_SYM，指向自定义模块
hello_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag                = HARDWARE_MODULE_TAG,
        .module_api_version = HELLO_MODULE_API_VERSION_1_0,
        .hal_api_version    = HARDWARE_HAL_API_VERSION,
        .id                 = HELLO_HARDWARE_MODULE_ID,
        .name               = "Demo Hello HAL",
        .author             = "dongjiao.tang@tcl.com",
        .methods            = &hello_module_methods,
    },
};

```

前面我们说过，要想自定义的HAL模块被外部使用需要导出符号HAL\_MODULE\_INFO\_SYM，导出的这个模块结构体最重要的其实就是tag，id和methods，tag必须定义为HARDWARE\_MODULE\_TAG，id就是在hello.h中定义的模块名，methods指向hello\_module\_methods地址，hello\_module\_methods指向结构体hw\_module\_methods\_t，它里面的open函数指针作用是打开模块下的设备，它指向hello\_open函数

### hello\_open

```
static int hello_open(const hw_module_t* module,const char __unused *id,
                            hw_device_t** device)
{
    if (device == NULL) {
        ALOGE("NULL device on open");
        return -1;
    }
    hello_device_t *dev = malloc(sizeof(hello_device_t));
    memset(dev, 0, sizeof(hello_device_t));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = HELLO_MODULE_API_VERSION_1_0;
    dev->common.module = (struct hw_module_t*) module;
    dev->common.close = hello_close;
    dev->additionTest = additionTest;

    *device = &(dev->common);
    return 0;
}
```

其实hello\_open函数也很简单，就是创建一个hello\_device\_t结构体，这是我们自定义HAL模块下的设备结构体，然后给这个结构体赋值，我们定义的加法函数additionTest赋值给设备结构体的函数指针，外部就可以使用，因为这个HAL模块导出了符号HAL\_MODULE\_INFO\_SYM，所以我们能通过模块id找到这个HAL模块，有了模块之后就可以调用它的hello\_open函数打开设备，hello\_open接收三个参数，module代表这个HAL模块，id代表要打开的是这个模块下哪一个设备，最后一个参数是一个二级指针，其实是我们自定义设备结构体指针的地址，传个地址过来就可以给这个设备结构体赋值

这样最简单的一个HAL模块就定义好了，它仅仅提供了一个加法函数，对于Android.bp如下：最终编译成一个共享lib库

```
cc_library_shared {
    name: "hello.default",
    relative_install_path: "hw",
    proprietary: true,
    srcs: ["hello.c"],
    header_libs: ["libhardware_headers"],
    shared_libs: ["liblog"],
}
```

我们在根目录执行如下命令来编译一下：

```
mmm hardware/libhardware/modules/hello/
```

编译成功后生成了一个hello.default.so共享lib库  
![hello.default.so](https://i-blog.csdnimg.cn/blog_migrate/2940e181bdc7fbaf7ed998efaad5ba2d.png)

我们将这个so库push进手机/system/lib64路径下，接着需要写一个简单测试程序来测一下

我们在HAL模块目录下再创建一个test目录，test目录下创建一个HelloTest.cpp测试文件和一个Android.bp文件，目录结构如下：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/da76997e8453e2f86ea2cf722543bcb8.png)  
HelloTest.cpp源码如下：

```
#include <hardware/hardware.h>
#include <hardware/hello.h>
#include <log/log.h>
#define TAG "HelloHal"
int main(){
    hello_device_t* hello_device = NULL;

    const hello_module_t * module = NULL;

    int ret = hw_get_module(HELLO_HARDWARE_MODULE_ID,(const struct hw_module_t**)&module);

    if (!ret) {
        ret = _hello_open((const struct hw_module_t*)module,&hello_device);
    }

   
    if (ret < 0) {
          ALOGD("get hello-hal failed.......");
          return -1;
    }

    int total = 0;
    hello_device->additionTest(hello_device,3,5,&total);
    ALOGD("success start hello-hal....total = %d",total);
    return 0;
}
```

通过hardware.c提供的hw\_get\_module函数，传递模块名获取对应的HAL模块，接着调用我们自定义的\_hello\_open函数通过获取到的模块打开设备，获取到设备之后，我们调用定义在设备结构体中的加法函数

Android.bp定义如下：

```
cc_binary {
    name: "hello_hal",
    srcs: ["HelloTest.cpp"],
    shared_libs: [
        "liblog",
        "libhardware",
    ],
}
```

这个test会被编译成二进制可执行文件：hello\_hal  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/52694c0d5941e62db3001cbaa527dbf7.png)

我们将这个可执行文件push进手机system/bin目录，然后执行  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b0172c69ae2783dd71ea81a9c3b70268.png)

可以看到log已经成功打印：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/85c6062dfb1a5b3c1dbe1411cadfbf5e.png)  
到此我们最简单的HAL就已经实现了，这个HAL并不设计底层驱动，实际开发中自己可以在设备结构体中定义函数访问底层硬件

后续文章会接着写从native层通过HIDL访问HAL，以及framework层通过JNI访问native层，以及通过应用层APK通过AIDL访问framework层，打通应用层到HAL层的整个开发框架