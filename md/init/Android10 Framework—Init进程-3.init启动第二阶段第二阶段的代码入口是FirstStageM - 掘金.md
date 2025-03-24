       ![](https://lf-web-assets.juejin.cn/obj/juejin-web/xitu_juejin_web/img/banner.a5c9f88.jpg)

> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

第二阶段的代码入口是FirstStageMain，我们使用这张图思维导图(参考网上旗浩老师的文章)总结其所做的事情

![第二阶段思维导图.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/d35ec0642b3d4b8389152fd8d3f3f14d~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240334&x-signature=VAzsZEeXHfLmhucSh1Exxg6HSak%3D) 因为这些知识点涉及内容过于庞大，如果将所有内容都在一篇文章中讲解，会导致文章过于冗长，不利于大家阅读学习；我习惯于开篇文章讲清楚结构框架，然后拆分功能点，每个功能点使用 1~N篇文章再去讲解（每篇文章尽可能将一到两个知识点讲清楚）。

我主要分为如下几部分讲解：

-   init进程日志
-   属性服务
-   Subcontext进程
-   rc文件解析
-   epoll机制
-   如何启动服务
-   子进程回收与服务重启

## init进程日志

```
int SecondStageMain(int argc, char** argv) {
    ...省略代码 

    if (REBOOT_BOOTLOADER_ON_PANIC) {
        InstallRebootSignalHandlers();
    }

    SetStdioToDevNull(argv);
    InitKernelLogging(argv);
    LOG(INFO) << "init second stage started!";

    ...省略代码 
} 
```

关于上面的代码在第一阶段中我们就讲解过了（详细内容可以参考前面的文章），它就是二阶段前的准备工作。

## 属性服务

属性服务其实说的简单些就是一堆键值对的环境变量。Android系统中，为统一管理系统的属性，设计了一个统一的属性系统，每个属性都是一个 key-value 对。 我们可以通过 shell 命令，Native 函数接口，Java 函数接口的方式来读写这些 key-vaule 对。

对于理解init进程工作机制而言，学习属性服务不是必须的，属性服务也是相对复杂的，因此大家看个人情况决定是否深入学习。

init二阶段中属性服务涉及的代码如下

```
int SecondStageMain(int argc, char** argv) {
    ...省略代码 

    property_init();

    // If arguments are passed both on the command line and in DT,
    // properties set in DT always have priority over the command-line ones.
    process_kernel_dt();
    process_kernel_cmdline();

    // Propagate the kernel variables to internal variables
    // used by init as well as the current required properties.
    export_kernel_boot_props();

    // Make the time that init started available for bootstat to log.
    property_set("ro.boottime.init", getenv("INIT_STARTED_AT"));
    property_set("ro.boottime.init.selinux", getenv("INIT_SELINUX_TOOK"));

    property_load_boot_defaults(load_debug_prop);

    StartPropertyService(&epoll);

    ...省略代码 
} 
```

上面的这些代码我将通过如下的几篇文章为大家讲解属性服务：

-   Android Framework—Init进程—6.属性服务框架
-   Android Framework—Init进程—7.服务端属性安全上下文序列化
-   Android Framework—Init进程—8.服务端属性文件创建和mmap映射
-   Android Framework—Init进程—9.服务端属性值初始化
-   Android Framework—Init进程—10.服务端属性服务启动
-   Android Framework—Init进程—11.客户端操作属性

## rc文件解析

rc文件其实就是一个配置文件，是android系统为了管理系统的服务配置而自定义的一种格式，因此也是非常重要的。 rc文件解析涉及代码如下

```
//system/core/init/init.cpp

int SecondStageMain(int argc, char** argv) {
    ...省略代码 

    const BuiltinFunctionMap function_map;
    Action::set_function_map(&function_map);

    ActionManager& am = ActionManager::GetInstance();
    ServiceList& sm = ServiceList::GetInstance();

    LoadBootScripts(am, sm);

    ...省略代码 
} 
```

这块知识内容我们也将是以如下单独文章的形式为大家讲解：

-   Android Framework—Init进程-14.rc文件格式
-   Android Framework—Init进程-15.rc文件解析

## 服务启动

rc文件定义了很多Action和Service，这些Action和Service实际上最终是为了描述系统中的服务以及服务之间的依赖关系，因此rc文件解析完成后，init进程二阶段的最终目的是启动后续服务，以保证系统启动以及后续的正常运行，这大致涉及如下内容：

-   service启动
-   init进程对service管理，包括：服务回收、重启等

这些内容我们也将是以如下单独文章的形式为大家讲解：

-   Android Framework—Init进程-16.启动服务
-   Android Framework—Init进程-17.属性变化如何影响Service

![avatar](https://p26-passport.byteacctimg.com/img/user-avatar/bde1ef02645bf5d95c45e5b30bef749f~40x40.awebp)