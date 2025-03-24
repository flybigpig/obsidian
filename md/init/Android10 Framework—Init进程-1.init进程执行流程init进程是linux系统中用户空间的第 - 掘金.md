> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

init进程是linux系统中用户空间的第一个进程，进程号为1。当bootloader启动后，启动kernel，kernel启动完后，在用户空间启动init进程，再通过init进程，来读取init.rc中的相关配置，从而来启动其他相关进程以及其他操作。

## 内核启动init

```
// init/main.c(linux内核源码)

static int __ref kernel_init(void *unused)
{
...省略代码

if (ramdisk_execute_command) {
ret = run_init_process(ramdisk_execute_command);
if (!ret)
return 0;
pr_err("Failed to execute %s (error %d)\n",
       ramdisk_execute_command, ret);
}

/*
 * We try each of these until one succeeds.
 *
 * The Bourne shell can be used instead of init if we are
 * trying to recover a really broken machine.
 */
if (execute_command) {
ret = run_init_process(execute_command);
if (!ret)
return 0;
panic("Requested init %s failed (error %d).",
      execute_command, ret);
}

if (CONFIG_DEFAULT_INIT[0] != '\0') {
ret = run_init_process(CONFIG_DEFAULT_INIT);
if (ret)
pr_err("Default init %s failed (error %d)\n",
       CONFIG_DEFAULT_INIT, ret);
else
return 0;
}

if (!try_to_run_init_process("/sbin/init") ||
    !try_to_run_init_process("/etc/init") ||
    !try_to_run_init_process("/bin/init") ||
    !try_to_run_init_process("/bin/sh"))
return 0;

panic("No working init found.  Try passing init= option to kernel. "
      "See Linux Documentation/admin-guide/init.rst for guidance.");
}
```

-   这个函数会去根文件系统中寻找init程序，具体路径由U-boot的环境变量bootargs提供，一旦init程序被找到，就会启动init进程（该可执行程序的文件名不一定叫init），然后操作系统正式运行，至此一去不复返
-   如果U-boot的环境变量bootargs没有传过来路径，或者路径中找不到，或者执行出错，那么kernel还留了一手以防万一。init\_post函数尾部有四个run\_init\_process函数，它们会去4个地方看看有没有init程序，如果以上都不成功，就启动失败了

## init相关源文件

```
system/core/init/main.cpp
system/core/init/init.cpp
system/core/init/ueventd.cpp
system/core/init/selinux.cpp
system/core/init.cpp
system/core/base/logging.cpp
system/core/init/first_stage_init.cpp
system/core/init/first_stage_main.cpp
system/core/init/first_stage_mount.cpp
system/core/init/keyutils.h
system/core/init/property_service.cpp
external/selinux/libselinux/src/label.c
system/core/init/signal_handler.cpp
system/core/init/service.cpp
```

## init进程

```
// system/core/init/main.cpp

int main(int argc, char** argv) {
#if __has_feature(address_sanitizer)
    __asan_set_error_report_callback(AsanReportCallback);
#endif

    if (!strcmp(basename(argv[0]), "ueventd")) {
        return ueventd_main(argc, argv);
    }

    if (argc > 1) {
        if (!strcmp(argv[1], "subcontext")) {
            android::base::InitLogging(argv, &android::base::KernelLogger);
            const BuiltinFunctionMap function_map;

            return SubcontextMain(argc, argv, &function_map);
        }

        if (!strcmp(argv[1], "selinux_setup")) {
            return SetupSelinux(argv);
        }

        if (!strcmp(argv[1], "second_stage")) {
            return SecondStageMain(argc, argv);
        }
    }

    return FirstStageMain(argc, argv);
}
```

init中启动代码是比较复杂的，这里我们首先搞清楚 2 个概念

-   init可执行程序，/init、/sbin/init、/etc/init、/bin/init文件表示可执行程序
-   init进程（进程的名为init）

一般来说init可执行程序启动后就是init进程，但是android中的init可执行程序是一个复合体，init可执行程序会被调用多次，在这个过程中init可执行程序会创建三个进程：

-   init进程
-   subcontext进程
-   ueventd进程

我们通过模拟器可以窥见一二，并且ueventd和subcontext的父进程都是init进程。

```
emulator64_arm64:/ # ps -elf
UID            PID  PPID C STIME TTY          TIME CMD
root             1     0 0 34:51 ?        00:00:00 init second_stage
root           114     1 0 34:51 ?        00:00:00 ueventd
root           112     1 0 34:51 ?        00:00:00 init subcontext u:r:vendor_init:s0 11
```

首先从内核启动init程序时是没有参数的，因此首先执行的是FirstStageMain（此时创建的就是init进程，毕竟它是祖先嘛）

```
//system/core/init/first_stage_init.cpp
int FirstStageMain(int argc, char** argv) {
    ...

    const char* path = "/system/bin/init";
    const char* args[] = {path, "selinux_setup", nullptr};
    execv(path, const_cast<char**>(args));

    return 1;
}
```

这里在init进程中又执行init可执行程序，传参selinux\_setup，显然接下来就是调用SetupSelinux方法了

```
//system/core/init/selinux.cpp
int SetupSelinux(char** argv) {
    ...

    const char* path = "/system/bin/init";
    const char* args[] = {path, "second_stage", nullptr};
    execv(path, const_cast<char**>(args));

    return 1;
}
```

这里在init进程中又执行init可执行程序，传参second\_stage，显然接下来就是调用SecondStageMain方法了

```
// system/core/init.cpp
int SecondStageMain(int argc, char** argv) {
    //创建subcontext进程
    subcontexts = InitializeSubcontexts();
    
    ...

    //创建ueventd进程
    am.QueueEventTrigger("early-init");
}
```

SecondStageMain中上面 2 处的调用会分别创建subcontext进程和ueventd进程

```
std::vector<Subcontext>* InitializeSubcontexts() {
    if (SelinuxGetVendorAndroidVersion() >= __ANDROID_API_P__) {
        for (const auto& [path_prefix, secontext] : paths_and_secontexts) {
            //创建Subcontext对象，调用其构造函数
            subcontexts.emplace_back(path_prefix, secontext);
        }
    }
    return &subcontexts;
}
```

Subcontext构造函数

```
Subcontext(std::string path_prefix, std::string context)
    : path_prefix_(std::move(path_prefix)), context_(std::move(context)), pid_(0) {
    Fork();
}

void Subcontext::Fork() {
    auto result = fork();

    if (result == -1) {
        LOG(FATAL) << "Could not fork subcontext";
    } else if (result == 0) {
        //subcontext子进程
        socket_.reset();

        // We explicitly do not use O_CLOEXEC here, such that we can reference this FD by number
        // in the subcontext process after we exec.
        int child_fd = dup(subcontext_socket);
        if (child_fd < 0) {
            PLOG(FATAL) << "Could not dup child_fd";
        }

        if (setexeccon(context_.c_str()) < 0) {
            PLOG(FATAL) << "Could not set execcon for '" << context_ << "'";
        }

        //init可执行程序
        auto init_path = GetExecutablePath();
        auto child_fd_string = std::to_string(child_fd);
        //参数为subcontext
        const char* args[] = {init_path.c_str(), "subcontext", context_.c_str(),
                              child_fd_string.c_str(), nullptr};
        //子进程中执行init可执行程序
        execv(init_path.data(), const_cast<char**>(args));

        PLOG(FATAL) << "Could not execv subcontext init";
    } else {
        subcontext_socket.reset();
        pid_ = result;
        LOG(INFO) << "Forked subcontext for '" << context_ << "' with pid " << pid_;
    }
}
```

通过fork创建子进程（创建的进程就是subcontext进程），在subcontext进程中执行init可执行程序并且传参subcontext，最后调用SubcontextMain方法。

am.QueueEventTrigger("early-init")调用会触发执行rc脚本内容（rc执行脚本后续会讲解），early-init定义在/init.rc中内容如下：

```
on early-init
    # Disable sysrq from keyboard
    write /proc/sys/kernel/sysrq 0

    # Set the security context of /adb_keys if present.
    restorecon /adb_keys

    # Set the security context of /postinstall if present.
    restorecon /postinstall

    mkdir /acct/uid

    # memory.pressure_level used by lmkd
    chown root system /dev/memcg/memory.pressure_level
    chmod 0040 /dev/memcg/memory.pressure_level
    # app mem cgroups, used by activity manager, lmkd and zygote
    mkdir /dev/memcg/apps/ 0755 system system
    # cgroup for system_server and surfaceflinger
    mkdir /dev/memcg/system 0550 system system

    start ueventd

    # Run apexd-bootstrap so that APEXes that provide critical libraries
    # become available. Note that this is executed as exec_start to ensure that
    # the libraries are available to the processes started after this statement.
    exec_start apexd-bootstrap
```

start ueventd命令启动ueventd服务, start命令会调用如下的do\_start方法（这个过程在后面解析init.rc并启动服务会讲解）

```
// system/core/init/builtins.cpp
static Result<Success> do_start(const BuiltinArguments& args) {
    Service* svc = ServiceList::GetInstance().FindService(args[1]);
    if (!svc) return Error() << "service " << args[1] << " not found";
    if (auto result = svc->Start(); !result) {
        return Error() << "Could not start service: " << result.error();
    }
    return Success();
}
```

这里会找到这个服务然后去启动，ueventd服务也定义在init.rc文件中

```
## Daemon processes to be run by init.
##
service ueventd /system/bin/ueventd
    class core
    critical
    seclabel u:r:ueventd:s0
    shutdown critical
```

Start方法简化如下

```
Result<Success> Service::Start() {
    ...
    
    pid = fork();
    if (pid == 0) {
        if (!ExpandArgsAndExecv(args_, sigstop_)) {
            PLOG(ERROR) << "cannot execve('" << args_[0] << "')";
        }
    }

    ...
}

static bool ExpandArgsAndExecv(const std::vector<std::string>& args, bool sigstop) {
    std::vector<std::string> expanded_args;
    std::vector<char*> c_strings;

    expanded_args.resize(args.size());
    c_strings.push_back(const_cast<char*>(args[0].data()));
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (!expand_props(args[i], &expanded_args[i])) {
            LOG(FATAL) << args[0] << ": cannot expand '" << args[i] << "'";
        }
        c_strings.push_back(expanded_args[i].data());
    }
    c_strings.push_back(nullptr);

    if (sigstop) {
        kill(getpid(), SIGSTOP);
    }

    return execv(c_strings[0], c_strings.data()) == 0;
}
```

这里调用fork创建子进程，调用execv执行路径c\_strings\[0\]，c\_strings\[0\]就是/system/bin/ueventd可执行程序，/system/bin/ueventd其实指向的就是init。

```
emulator64_arm64:/ # ls /system/bin/ueventd -l                                         
lrwxr-xr-x 1 root shell 4 2021-11-13 03:47 /system/bin/ueventd -> init
```

这样就调用了ueventd\_main方法。

总结init代码调用的主要方法顺序如下：

1.  FirstStageMain
2.  SetupSelinux
3.  SecondStageMain
4.  SubcontextMain
5.  ueventd\_main

init进程作为Android系统的祖先进程，其功能是非常复杂的，借用网上大神的图可以看出init进程的重要性和复杂度。

![Framework系统模块启动关系图.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/f97fa14601f84ac0adb05b26e7dbe71c~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743239950&x-signature=%2FggfAG35HWbot40rp%2Bo4a3aDJ0Q%3D) 分析Android系统源码我们的思路就是抓大放小，抓住框架主要结构和流程，不必过于深究细节实现。因为，我们分析init进程核心就是看看它是如何解析解析rc文件，启动后续服务的，例如：Zygote、service\_manager等。init的其它相关内容有些也会讲解，例如：log、属性系统、selinux等，感兴趣的同学也可以了解一下。

参考：  
[www.jianshu.com/p/446682d2c…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.jianshu.com%2Fp%2F446682d2c482 "https://www.jianshu.com/p/446682d2c482")  
[www.jianshu.com/p/3e002a31d…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.jianshu.com%2Fp%2F3e002a31ddcd "https://www.jianshu.com/p/3e002a31ddcd")  
[子进程回收与服务重启分析](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fqq_28261343%2Farticle%2Fdetails%2F128280652 "https://blog.csdn.net/qq_28261343/article/details/128280652")  
[Android启动过程 - init进程对服务子进程的管理](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Ftemp7695%2Farticle%2Fdetails%2F135663621 "https://blog.csdn.net/temp7695/article/details/135663621")  
[Android ueventd浅析](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fu010164190%2Farticle%2Fdetails%2F106092896 "https://blog.csdn.net/u010164190/article/details/106092896")  
[Android 12 init(6) Subcontext进程工作过程分析](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fqq_28261343%2Farticle%2Fdetails%2F128357380 "https://blog.csdn.net/qq_28261343/article/details/128357380") [blog.csdn.net/liuwg1226/a…](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fliuwg1226%2Farticle%2Fdetails%2F109186383 "https://blog.csdn.net/liuwg1226/article/details/109186383")  
[Android系统启动流程（四）——rc文件解析](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fweixin_49274713%2Farticle%2Fdetails%2F130533998 "https://blog.csdn.net/weixin_49274713/article/details/130533998")