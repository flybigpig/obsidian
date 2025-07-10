---
created: 2025-07-10T13:44:40 (UTC +08:00)
tags: [selinux权限里面type什么意思android]
source: https://blog.csdn.net/qq_40731414/article/details/126919088?spm=1001.2014.3001.5502
author: 成就一亿技术人!
---

# SELinux for Android的基本知识和实战_selinux权限里面type什么意思android-CSDN博客

> ## Excerpt
> 文章浏览阅读2.5k次。本文介绍了SELinux在Android系统中的基本原理及其实战应用。详细解释了SELinux的背景、对象、域、类型等核心概念，并提供了快速解决权限问题的方法。

---
![](https://csdnimg.cn/release/blogv2/dist/pc/img/original.png)

[坂田民工](https://blog.csdn.net/qq_40731414 "坂田民工") ![](https://csdnimg.cn/release/blogv2/dist/pc/img/newUpTime2.png) 已于 2022-09-18 17:06:32 修改

于 2022-09-18 17:04:09 首次发布

版权声明：本文为博主原创文章，遵循 [CC 4.0 BY-SA](http://creativecommons.org/licenses/by-sa/4.0/) 版权协议，转载请附上原文出处链接和本声明。

## SELinux for Android的基本知识和实战

#### 1\. 背景

1.  SELinux出现之前，Linux上的安全模型叫DAC。Linux DAC采用了一种非常简单的策略, 只是将资源和访问对象分类，根据访问者的分组和组对应权限决定来确定是否可以访问，**但是这样就会出现只要取得root权限就可以为所欲为的情况**。针对DAC的不足，所以在DAC之外设计了一个新的安全模型，叫MAC，**MAC的做法就是任何进程想在SELinux系统中干任何事情，都必须先在安全策略配置文件中赋予权限。凡是没有出现在安全策略配置文件中的权限，进程就没有该权限**。SELinux也属于MAC机制的一种，在selinux的约束下，即使你有root权限，如果无法通过MAC验证，那么一样的无法真正执行相关的操作。另外对每一项权限进行了更加完整的细化，可限制用户对资源的访问行为。
2.  Android是建立在标准的Linux Kernel基础上，Google为了进一步增强Android的安全性，经过多个版本的努力将SELinux引入了Android中，作为 Android 安全模型的一部分(SEAndroid)，对所有进程强制执行强制访问控制 (MAC)，甚至包括以 Root/超级用户权限运行的进程。借助 SELinux，Android 可以更好地保护和限制系统服务、控制对应用数据和系统日志的访问、降低恶意软件的影响，并保护用户免遭移动设备上的代码可能存在的缺陷的影响。SELinux 按照默认拒绝的原则运行：**任何未经明确允许的行为都会被拒绝**。SELinux 可按两种全局模式运行：
    
    -   **Permissive**，宽容模式：权限拒绝事件会被记录下来，但不会被强制执行。
    -   **Enforce**，强制模式：权限拒绝事件会被记录下来并强制执行。
    
    ```
    查看设备当前的SELinux方法
    adb shell getenforce
    ```
    
    ```
    临时切换SELinux模式:
    Permissive: adb shell setenforce 0
    Enforce: adb shell setenforce 1
    ```
    

在选择强制执行级别时只能二择其一，一般出厂的设备都要求强制模式，开发时在宽容模式解决完所有的权限问题后将会切换到强制模式。

#### 2\. selinux的对象、域、类型等概念

SELinux是一种基于域-类型（domain-type）模型的强制[访问控制](https://so.csdn.net/so/search?q=%E8%AE%BF%E9%97%AE%E6%8E%A7%E5%88%B6&spm=1001.2101.3001.7020)（MAC）安全系统，它有几个概念我们先了解下：

-   对象(object)：例如文件、进程、[套接字](https://so.csdn.net/so/search?q=%E5%A5%97%E6%8E%A5%E5%AD%97&spm=1001.2101.3001.7020)、系统属性、IPC等系统资源
    
-   类型(type)：SELinux中所有的对象都会有对应的类型，这样才能根据类型实施不同的策略。举例, 默认情况下，应用的类型为 untrusted\_app.
    
-   域(daemain)：对于进程而言，其类型也称为域
    
-   属性(attribute)：属性是多个具有共性的类型的集合
    
-   上下文(context)：也称为标签，SELinux给Linux的所有对象都分配一个安全上下文(Security Context)， 描述成一个标准的字符串
    
    > 通过ps -Z和ls -Z命令看到的所有的进程和文件的安全上下文  
    > 格式： user:role:type\[:range\]  
    > 举例： u:r:untrusted\_app:s0  
    > 说明：  
    > 1、u为user的意思。SEAndroid中只定义了一个SELinux用户，值为u。  
    > 2、r为role的意思。role是角色之意，它是SELinux中一种比较高层次，更方便的权限管理思路，一个u可以属于多个role，不同的role具有不同的权限，SEAndroid也只定义了一个SELinux角色r。  
    > 3、untrusted\_app，代表该进程所属的类型untrusted\_app, 进程的类型称为Domain  
    > 4、S0和SELinux为了满足军用和教育行业，MLS将系统的进程和文件进行了分级，不同级别的资源需要对应级别的进程才能访问。
    
-   策略(policy)： SEAndroid安全机制主要是使用对象安全上下文中的类型来定义安全策略,策略是具体实施的规则，策略文件位于各级sepolicy目录下，后缀为".te"的文件
    

##### 2.1 安全上下文(Context)

SELinux 上下文标签，为了支持平台和供应商 sepolicy 之间的区别，系统以不同的方式构建上下文文件以使它们分开, 使用这些上下文给系统对象分配标签。SEAndroid主要的上下文有以下几种

-   文件上下文file\_contexts：  
    作用：用于为文件分配标签，并且可供多种用户空间组件使用
    
-   属性上下文property\_contexts  
    作用：用于声明属性的安全上下文，以便控制哪些进程可以设置这些属性
    
-   服务上下文service\_contexts  
    作用：用于为 Android Binder 服务分配标签，以便控制哪些进程可以为相应服务添加（注册）和查找（查询）Binder 引用，service\_contexts对应使用servicemanager(binder)的服务上下文，hwservice\_contexts对应使用hwservicemanager(hwBinder)的服务上下文
    
-   Seapp 上下文 seapp\_contexts  
    作用：用于为应用进程和 /data/data 目录分配标签。在每次应用启动时，zygote 进程都会读取此配置；在启动期间，installd 会读取此配置。
    
-   mac\_permissions.xml  
    作用：用于根据应用签名和应用软件包名称（后者可选）为应用分配 seinfo 标记
    

值得注意的是vendor定制的目录下对应的上下文的名字一般都不会加前缀，和平台的对应文件保持一致，system的上下文和vendor的上下文会在init进程中一起加载。

##### 2.2 策略文件

系统为我们预定于了很对规则，可分类为：

1.  针对attribute的策略制定，例如：domain.te、file.te等
2.  针对daemon domain的策略制定，例如：adbd.te、servicemanager.te、surfaceflinger.te等
3.  针对系统的其他模块进行策略制定，例如：app.te、system.te、init.te等

##### 2.3 SEAndroid sepolicy的各个文件目录

-   system/sepolicy/public  
    平台的公共策略，平台的sepolicy API，对vendor可见，vendor可以引用
-   system/sepolicy/private  
    平台的专用策略，对vendor不可见
-   system/sepolicy/vendor  
    供应商可以使用的政策和上下文文件（供应商可以根据情况忽略）
-   /device/manufacturer/device-name/sepolicy  
    在设备对应的BoardConfig.mk 通过BOARD\_SEPOLICY\_DIRS添加的自定义供应商sepolicy路径
-   /vendor/manufacturer/device-name/sepolicy 同上

#### 3\. 实战

##### 3.1 快速添加avc denied权限的万能公式

这里主要是解决我们在开发中在日志获取到的avc denied的解决，因为selinux权限问题90%的场景都是在补足缺少的权限，这里有一个通用的方法帮我们解决此类问题，步骤如下：

1.  首先获取avc的打印信息，可以通过logcat或者dmsg获取，一般logcat |grep avc就可以获取  
    举例：
    
    ```
    type=1400 audit(0.0:1639): avc: denied { read } for name="u:object_r:hwservicemanager_prop:s0" dev="tmpfs" ino=2257 scontext=u:r:tcl_factory_service_default:s0 tcontext=u:object_r:hwservicemanager_prop:s0 tclass=file permissive=1
    ```
    
    这里我们先介绍下对应的字段的概念：  
    操作权限 - action 具体缺少的权限，这个权限属于tclass组  
    源上下文 - scontext 发起者对象的类型  
    目前上下文 - tcontext 正在操作的对象类型  
    目标类 - tclass 正在操作的对象的类型
    
    说的简单一点就是：  
    缺少什么权限： { read }权限，  
    谁缺少权限： scontext=u:r:tcl\_factory\_service\_default:s0  
    对谁操作时缺少权限： tcontext=u:object\_r:hwservicemanager\_prop:s0  
    什么类型的权限： tclass=file
    
    得出的权限解决语句：
    
    ```
    # tcl_factory_service_default.te
    allow tcl_factory_service_default hwservicemanager_prop:file { read };
    ```
    
    总结出的通用公式：
    
    ```
    要添加的权限语句： allow scontext tcontext:tclass { action };
    要添加的目标文件： {scontext}.te
    ```
    

##### 3.2 Audit2allow工具

audit2allow是一个开源的工具，可帮助我们从avc日志中生成需要添加的权限语句。但是这个工具只是辅助，因为有时候它会给很泛的权限，虽然解决了问题，但是范围给的太大了，不安全。  
使用前记得source lunch下环境就可以在根目录下使用。可执行文件的位置：

```
external/selinux/prebuilts/bin/audit2allow
```

1.  使用方法1，从dmsg中提取：  
    (1) 提取所有的avc LOG.
    
    ```
    adb shell "cat /proc/kmsg | grep avc" > avc_log.txt
    ```
    
    (2) 使用 audit2allow tool直接生成policy.
    
    ```
    audit2allow -i avc_log.txt  即可自动输出生成的policy
    ```
    
2.  使用方法2，从系统专门的文件中提取(google推荐)
    
    ```
    adb pull /sys/fs/selinux/policy
    adb logcat -b events -d | audit2allow -p policy
    ```
    
3.  使用方法3，从logcat提取
    
    ```
    adb logcat -b all -d | audit2allow -p policy
    ```
    

##### 3.4 为新的服务添加权限

1.  例子1：为一个system分区的native服务添加权限(google的例子)
    
    1.  在对应的init.device.rc添加服务启动脚本
    
    ```
    service foo /system/bin/foo
    class core
    ```
    
    2.  为服务创建一个新的域  
        在对应的device sepolicy下创建foo.te, 例如：device/manufacturer/device-name/sepolicy/foo.te
    
    ```
    # foo service
    type foo, domain;
    type foo_exec, exec_type, file_type;
    
    init_daemon_domain(foo)
    ```
    
    3.  给服务这个bin文件添加标签(上下文)  
        在对应的device sepolicy下的file\_contexts添加，例如：device/manufacturer/device- name/sepolicy/file\_contexts
    
    ```
    /system/bin/foo   u:object_r:foo_exec:s0
    ```
    
    4.  在device.mk中添加这个文件，或者拷贝对应的可执行文件（确保刷机起来后/system/bin/foo存在）
        
    5.  编译并烧录
        
    6.  根据avc denied提示，使用上面的通用公式添加缺少的权限
        
2.  例子2： (HIDL|AIDL) HAL新服务添加selinux权限
    
3.  例子3： JAVA 新服务添加selinux权限
    
4.  例子4： 设备节点添加selinux服务
    

##### 3.5 neverallow的场景和绕开

SELinux neverallow规则用于禁止在任何情况下都不应该发生的行为, 即会影响安全性的绝不允许的规则，删除neverallow节点将无法通过CTS认证。但是有时候我们的定制需求又需要解决某个权限被neverallow的情况，那么我们就得尝试如何绕开这个规则限制，只是争取绕开，没有什么万能的方法。

1.  关于文件路径访问的neverallow绕开

```
例子：logpersist访问data/p1路径

由于我们要访问的目录path为/data/p1，将该目录定义成自己的Type，可以自定义Type，如下：

在file.te中自定义一个type：
type p1_data_file, file_type, data_file_type, core_data_file_type;

在file_contexts中定义安全上下文：
/data/p1(/.*)?            u:object_r:p1_data_file:s0

在logpersist.te将allow语句改为：
allow  logpersist  p1_data_file:dir write;

然后在logpersist.te中单独将自定义的p1_data_file减去即可。
neverallow logpersist {
  file_type
  userdebug_or_eng(`-misc_logd_file -coredump_file')
  with_native_coverage(`-method_trace_data_file')
  -p1_data_file
}:file { create write append };
```

2.  例子2：  
    后续补充

经常困扰的问题：

1.  Q: 有些SOC目录下分为private、public、vendor，里面文件都相同，我们到底应该改private、public、vendor哪个路径呢？  
    A: 在device对应的BoardConfig.mk定义了不同版本所要编译包含的目录，以MK里配置的目录为准
2.  如何快速验证策略是否通过编译,检查添加的权限是否合法，是否存在neverallow的现象  
    A: make sepolicy -jxxx
