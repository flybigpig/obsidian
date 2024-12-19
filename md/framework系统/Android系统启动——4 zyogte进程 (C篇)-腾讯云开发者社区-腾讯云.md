本篇文章的主要内容如下：

-   1、为什么要研究 zygote？
-   2、Zygote进程(C层)的启动
-   3、关于虚拟机简介
-   4、启动虚拟机
-   5、Runtime

我们大家都是知道**"一鼎三足"**和**"三角形的稳定性"**，那么支撑Android系统的三个"足"是什么？即**init进程**、**SystemServer进程**和**Zygote进程**。本篇文章我们就好好来研究下Zygote进程

### 一、为什么要研究 zygote？

> Linux的进程是通过系统调用fork产生的，fork出的子进程除了内核中的一些核心的数据结构和父进程不同之外，其余的内存映像都是和父进程共享的。只有当子进程需要去改写这些共享的内存时，操作系统才会为子进程分配一个新的页面，并将老的页面上的数据复制一份到新的页面，这就是所谓的"写拷贝"。

通常，子进程被fork出来后，会继续执行系统调用exec，exec将用一个新的可执行文件的内容替换当前进程的代码段、数据段、堆和栈段。Fork加exec 是Linux启动应用的标准做法，init进程也是这样来启动的各种服务的。

-   Zygote创建应用程序时却只使用了fork，没有调用exec。Android应用中执行的是Java代码，Java代码的不同才造成了应用的区别，而对于运行Java的环境，要求却是一样的。
-   Zygote初始化时会创建创建虚拟机，同时把需要的系统类库和资源文件加载到内存里面。Zygote fork出子进程后，这个子进程也继承了能正常工作的虚拟机和各类系统资源，接下来子进程只需要装载APK文件的字节码文件就可以运行了。这样应用程序的启动时间就会大大缩短。

如下图所示

![](https://ask.qcloudimg.com/http-save/yehe-2957818/5b8t977hun.png)

image.png

### 二、Zygote进程(C层)的启动

Zygote进程在init进程中以**service**的方式启动的。从Android 5.0开始，Zygote还是有变动的，之前是直接放入init.rc中的代码块中，现在是放到了单独的文件中，通过init.rc中通过"import"的方式引入文件。如下:

代码在[init.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Frootdir%25252Finit.rc&objectId=1199508&objectType=1&isNewArticle=undefined) 11行

```
import /init.${ro.zygote}.rc
```

从上面的语句可以看到，init.rc并不是直接引入某个固定的文件，而是根据属性ro.zygote的内容来引入不同的文件。这是因为从Android 5.0开始，Android系统开始支持64位的编译，Zygote进程本身也会有32位和64位版本的区别，因此，这里通过ro.zygote属性来控制启动不同版本的Zyogte进程。

属性rozyoget 可能取值有可能为如下：

-   zygote32
-   zygote32\_64
-   zygote64
-   zygote64\_32

所以在init.rc的同级目录下一共有4个和zygote相关的rc文件：

-   init.zygote32
-   init.zygote64
-   init.zygote32\_64
-   init.zygote64\_32

如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/13lvpu7x1w.png)

init.zygote.png

这里我们看下他们的区别

##### (一)、init.zygote32与init.zygote64的区别

因为init.zygote64和init.zygote64\_32的结果和 init.zygote32与init.zygote64 差不多，我就只讲解 init.zygote32与init.zygote64的区别了。

直接看代码

###### 1、init.zygote32.rc

[init.zygote32.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Frootdir%25252Finit.zygote32.rc&objectId=1199508&objectType=1&isNewArticle=undefined)

```
service zygote /system/bin/app_process -Xzygote /system/bin --zygote --start-system-server
    class main
    socket zygote stream 660 root system
    onrestart write /sys/android_power/request_state wake
    onrestart write /sys/power/state on
    onrestart restart media
    onrestart restart netd
    writepid /dev/cpuset/foreground/tasks
```

init.zygote32.rc文件的内容和Android 5.0以前的init.rc中启动zygote的内容是一致的。

###### 2、init.zygote32\_64.rc

[init.zygote32\_64.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Frootdir%25252Finit.zygote32_64.rc&objectId=1199508&objectType=1&isNewArticle=undefined)

```
service zygote /system/bin/app_process32 -Xzygote /system/bin --zygote --start-system-server --socket-name=zygote
    class main
    socket zygote stream 660 root system
    onrestart write /sys/android_power/request_state wake
    onrestart write /sys/power/state on
    onrestart restart media
    onrestart restart netd
    writepid /dev/cpuset/foreground/tasks

service zygote_secondary /system/bin/app_process64 -Xzygote /system/bin --zygote --socket-name=zygote_secondary
    class main
    socket zygote_secondary stream 660 root system
    onrestart restart zygote
    writepid /dev/cpuset/foreground/tasks
```

从init.zygote32\_64.rc 文件可以看到，这种情况下系统定义了两个zygote 服务：**zygote**和**zygote\_secodary**。这两个服务最大的区别是启动的可执行文件不同，一个是app\_process32，一个是app\_process64。所以从这里我们可以知道，Andorid系统支持4种运行模式：

-   纯32位模式：属性ro.zygote的值为zygote32
-   混32位模式(即32位为主，64位为辅)模式：属性ro.zygote的值为zygote32\_64
-   纯64位模式：属性ro.zygote的值为zygote64
-   混64位模式(即 64位为主，32位为辅)模式：属性ro.zygote值为zygote64\_32

##### (二)、zygote与service\_start()函数

以[init.zygote32.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Frootdir%25252Finit.zygote32.rc&objectId=1199508&objectType=1&isNewArticle=undefined)为例，

```
    socket zygote stream 660 root system
```

上面这行代码表示这个zygote的socket选项表明，它需要一个名字为"zgyote"的"流(stream)"的socket。当init进程真的启动zygote服务的时候，会走到会走到service\_start()函数，那我们来看下service\_start()函数的具体执行。

代码在[init.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit.cpp&objectId=1199508&objectType=1&isNewArticle=undefined) 187行

```
void service_start(struct service *svc, const char *dynamic_args)
{
    //*****************  第1步 ***********************
    // Starting a service removes it from the disabled or reset state and
    // immediately takes it out of the restarting state if it was in there.
     // 状态重置
    svc->flags &= (~(SVC_DISABLED|SVC_RESTARTING|SVC_RESET|SVC_RESTART|SVC_DISABLED_START));

    // 初始化启动时间，开始计时
    svc->time_started = 0;

    // Running processes require no additional work --- if they're in the
    // process of exiting, we've ensured that they will immediately restart
    // on exit, unless they are ONESHOT.
    // 排除正在运行的service
    if (svc->flags & SVC_RUNNING) {
        return;
    }

    //*****************  第2步 ***********************
    // 如果是需要控制台的环境，但是还没有控制台，则设置SVC_DISABLED标志，后返回
    bool needs_console = (svc->flags & SVC_CONSOLE);
    if (needs_console && !have_console) {
        ERROR("service '%s' requires console\n", svc->name);
        svc->flags |= SVC_DISABLED;
        return;
    }

   //*****************  第3步 ***********************
    struct stat s;

     // 在parse_service中构建service的过程中，会将配置文件中的service文件路径名赋值给svc->args  
     //然后这个李判断可执行文件是否存在
    if (stat(svc->args[0], &s) != 0) {
        ERROR("cannot find '%s', disabling '%s'\n", svc->args[0], svc->name);
        svc->flags |= SVC_DISABLED;
        return;
    }

   //*****************  第4步 ***********************
    // 如果是SVC_ONESHOT 则表示该service退出后，不能再次运行
    if ((!(svc->flags & SVC_ONESHOT)) && dynamic_args) {
        ERROR("service '%s' must be one-shot to use dynamic args, disabling\n",
               svc->args[0]);
        svc->flags |= SVC_DISABLED;
        return;
    }

   //*****************  第5步 ***********************
    char* scon = NULL;

   // 是否开启了selinux
    if (is_selinux_enabled() > 0) {
       // 如果 init.rc中已经制定了安全上下文
        if (svc->seclabel) {
            // 复制一份
            scon = strdup(svc->seclabel);
            if (!scon) {
                ERROR("Out of memory while starting '%s'\n", svc->name);
                return;
            }
        } else {
            char *mycon = NULL, *fcon = NULL;

            INFO("computing context for service '%s'\n", svc->args[0]);

            // 获取当前init进程的安全上下文
            int rc = getcon(&mycon);
            if (rc < 0) {
                ERROR("could not get context while starting '%s'\n", svc->name);
                return;
            }

            // 获取可执行文件的安全上下文
            rc = getfilecon(svc->args[0], &fcon);
            if (rc < 0) {
                ERROR("could not get context while starting '%s'\n", svc->name);
                freecon(mycon);
                return;
            }

            // 计算守护进程的安全上下文
            rc = security_compute_create(mycon, fcon, string_to_security_class("process"), &scon);
            if (rc == 0 && !strcmp(scon, mycon)) {
                ERROR("Warning!  Service %s needs a SELinux domain defined; please fix!\n", svc->name);
            }
            freecon(mycon);
            freecon(fcon);
            if (rc < 0) {
                ERROR("could not get context while starting '%s'\n", svc->name);
                return;
            }
        }
    }

    NOTICE("Starting service '%s'...\n", svc->name);

   //*****************  第6步 ***********************
    // fork一个新进程 
    pid_t pid = fork();

    // pid为0 表示子进程运行空间
    if (pid == 0) {
        struct socketinfo *si;
        struct svcenvinfo *ei;
        char tmp[32];
        int fd, sz;

        umask(077);

         //*****************  第7步 ***********************
        //如果 属性已经被初始化了
        if (properties_initialized()) {
 
            // 增加一个property属性，用文件描述符做key，大小做value
            get_property_workspace(&fd, &sz);
            snprintf(tmp, sizeof(tmp), "%d,%d", dup(fd), sz);

            // 增加一个环境变量
            add_environment("ANDROID_PROPERTY_WORKSPACE", tmp);
        }

        // 在配置文件中，service 通过setenv设置envvars
        for (ei = svc->envvars; ei; ei = ei->next)
            add_environment(ei->name, ei->value);

         //*****************  第8步 ***********************

        // 在init.rc配置文件中，通过sokcet关键字 设置service的socket
        for (si = svc->sockets; si; si = si->next) {
             int socket_type = (
                    !strcmp(si->type, "stream") ? SOCK_STREAM :
                        (!strcmp(si->type, "dgram") ? SOCK_DGRAM : SOCK_SEQPACKET));

            // 创建socket
            int s = create_socket(si->name, socket_type,
                                  si->perm, si->uid, si->gid, si->socketcon ?: scon);
            if (s >= 0) {
                // 调用add_environment，ANDROID_SOCKET_%s(name)，s为文件描述符
                 // Zygote的socket的key 为ANDROID_SOCKET_zygote，就是这么创建的
                publish_socket(si->name, s);
            }
        }

        freecon(scon);
        scon = NULL;

        if (svc->writepid_files_) {
            std::string pid_str = android::base::StringPrintf("%d", pid);
            for (auto& file : *svc->writepid_files_) {
                if (!android::base::WriteStringToFile(pid_str, file)) {
                    ERROR("couldn't write %s to %s: %s\n",
                          pid_str.c_str(), file.c_str(), strerror(errno));
                }
            }
        }

        if (svc->ioprio_class != IoSchedClass_NONE) {
            if (android_set_ioprio(getpid(), svc->ioprio_class, svc->ioprio_pri)) {
                ERROR("Failed to set pid %d ioprio = %d,%d: %s\n",
                      getpid(), svc->ioprio_class, svc->ioprio_pri, strerror(errno));
            }
        }

         //*****************  第9步 ***********************
       // 如果还是用新的控制台
        if (needs_console) {
            setsid();
            // 打开设备文件"/dev/sonsole"
            open_console();
        } else {
            //重定向到设备文件"/dev/null"
            zap_stdio();
        }

        if (false) {
            for (size_t n = 0; svc->args[n]; n++) {
                INFO("args[%zu] = '%s'\n", n, svc->args[n]);
            }
            for (size_t n = 0; ENV[n]; n++) {
                INFO("env[%zu] = '%s'\n", n, ENV[n]);
            }
        }

        // 因为第一个参数为0，所以将当前进程的pgid设置成自己的pid
        setpgid(0, getpid());

        // As requested, set our gid, supplemental gids, and uid.
        if (svc->gid) {
            if (setgid(svc->gid) != 0) {
                ERROR("setgid failed: %s\n", strerror(errno));
                _exit(127);
            }
        }
        if (svc->nr_supp_gids) {
            if (setgroups(svc->nr_supp_gids, svc->supp_gids) != 0) {
                ERROR("setgroups failed: %s\n", strerror(errno));
                _exit(127);
            }
        }
        if (svc->uid) {
            if (setuid(svc->uid) != 0) {
                ERROR("setuid failed: %s\n", strerror(errno));
                _exit(127);
            }
        }
        if (svc->seclabel) {
            if (is_selinux_enabled() > 0 && setexeccon(svc->seclabel) < 0) {
                ERROR("cannot setexeccon('%s'): %s\n", svc->seclabel, strerror(errno));
                _exit(127);
            }
        }

       //*****************  第10步 ***********************
        if (!dynamic_args) {
            // 静态调用execve函数
            if (execve(svc->args[0], (char**) svc->args, (char**) ENV) < 0) {
                ERROR("cannot execve('%s'): %s\n", svc->args[0], strerror(errno));
            }
        } else {
            // 动态调用execve函数
            char *arg_ptrs[INIT_PARSER_MAXARGS+1];
            int arg_idx = svc->nargs;
            char *tmp = strdup(dynamic_args);
            char *next = tmp;
            char *bword;

            /* Copy the static arguments */
            memcpy(arg_ptrs, svc->args, (svc->nargs * sizeof(char *)));

            while((bword = strsep(&next, " "))) {
                arg_ptrs[arg_idx++] = bword;
                if (arg_idx == INIT_PARSER_MAXARGS)
                    break;
            }
            arg_ptrs[arg_idx] = NULL;
            execve(svc->args[0], (char**) arg_ptrs, (char**) ENV);
        }
        _exit(127);
    }

    freecon(scon);

    // 如果子进程创建失败
    if (pid < 0) {
        ERROR("failed to start '%s'\n", svc->name);
        svc->pid = 0;
        return;
    }

    //*****************  第11步 ***********************
    // init程序作为父进程，则设置相应service的几个属性：启动时间、pid、RUNNING标志
    svc->time_started = gettime();
    svc->pid = pid;
    svc->flags |= SVC_RUNNING;

    if ((svc->flags & SVC_EXEC) != 0) {
        INFO("SVC_EXEC pid %d (uid %d gid %d+%zu context %s) started; waiting...\n",
             svc->pid, svc->uid, svc->gid, svc->nr_supp_gids,
             svc->seclabel ? : "default");
        waiting_for_exec = true;
    }

    // 设置状态为running
    svc->NotifyStateChange("running");
}
```

我将的service\_start函数分为11步

那我们来一一介绍下

##### 1、第1步——重置Service中的标志

**service**在启动的时候，需要重置标志，其中SVC\_DISABLED、SVC\_RESTARTING、SVC\_RESET、SVC\_DISABLED\_START这4个标志都是和启动进程相关，需要先清除掉。如果**service**带有SVC\_RUNNING标志，说明服务已经运行了，所以就不用重复启动了。

##### 2、第2步——控制台检查

这里主要是判断**service**是否需要有控制台，如果需要控制台，但是还没有控制台，就前后矛盾了，则直接退出。

> 代码中的\*\* have\_console\*\* 是一个全局变量，在函数console\_init\_action() (在[init.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Finit%25252Finit.cpp&objectId=1199508&objectType=1&isNewArticle=undefined) 726行)中，如果能打开设备文件"/dev/console"，则它会被置为1。

##### 3、第3步——文件检查

这里主要是检查**service**二进制文件是否存在

> PS：restart\_service\_need()函数也会使用service\_start()函数使用的参数 dynamic\_args为NULL，因此这段不会执行

##### 4、第4步——检查SVC\_ONESHOT参数

SVC\_ONESHOT标志表示service只启动一次，一旦退出后，就不能再启动了

##### 5、第5步——设置SELinux的安全上下文

这块代码的作用，就是获取服务进程的安全上下文。

##### 6、第6步——fork子进程

没什么好讲的就是fork子进程

##### 7、第7步——准备环境变量

在**service**选项中，如果有setev选项，会将setenv的参数设置为服务进程环境变量。

> PS：这里把"属性"共享区域文件句柄fd执行dup后的结果放到了ANDROID\_PROPERTY\_WORKSPACE环境变量中。这个fd在服务进程不能打开属性共享区的设备文件时使用。

##### 8、第8步——创建sokcet。

在**service**选项中，如果有socket选项，则这里会为服务进程创建参数中的socke。这里创建的socket是AF\_UNIX类型的，这类的socket的创建需要有root权限，因此，要在执行exec调用之前创建出来。但是执行exec后服务进程不知道文件描述符也无法使用它。Android解决方法是将描述符放到了一个环境变量中。环境变量的名字被定义为ANDROID\_SOCKET\_XXX，XXX是socket选项的名字。这样服务进程就能通过这个特殊名字的环境变量来找到socket的文件描述符了。为了让socket的文件描述符在执行exec后不被关闭，还需要对文件描述符执行fcntl(fd,F\_SETFD,0)调用。这些都是在函数**publish\_socket()**中完成的。这里就不深入研究**publish\_socket()**函数了。

##### 9、第9步——处理标准输出、标准输入、标准错误

-   如果需要使用控制台console。则调用\*\* open\_console()函数\*\*打开设备文件"/dev/sonsole"，然后把标准输出、标准输入、标准错误重定向到该设备文件上。
-   如果不需要控制台，则调用\*\* zap\_stdio()函数\*\*，把标准输出、标注你输入、标准错误重定向到设备文件"/dev/null"。

##### 10、第10步——处理标准输出、标准输入、标准错误

调用execve函数，这里分两种情况，一种是静态的，一种是动态的，通过dynamic\_args参数来决定

##### 10、第11步——更新属性

该快代码，就是更新service的相关属性

从前面的定义可以都看到Zygote进程可以执行文件是app\_process。app\_process模块的源文件在frameworks/base/cmds/app\_process下，只有一个文件app\_main.cpp

![](https://ask.qcloudimg.com/http-save/yehe-2957818/juhqunqipc.png)

image.png

##### (三)、zygote的启动流程

那我们就整理一下zygote的启动流程，大致如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/p6luk8kiol.png)

image.png

[大图链接](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%25253A%25252F%25252Fpostimg.org%25252Fimage%25252Fx1z0eeni3%25252F&objectId=1199508&objectType=1&isNewArticle=undefined)

关于init.cpp里面的流程前面已经讲解了，我这里就不再说了，我们从app\_main.cpp开始

###### 1、app\_main.cpp的main函数

代码在[app\_process/app\_main.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fframeworks%25252Fbase%25252Fcmds%25252Fapp_process%25252Fapp_main.cpp&objectId=1199508&objectType=1&isNewArticle=undefined) 186行

```
int main(int argc, char* const argv[])
{

    // ******************** 第一部分 *********************
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        // Older kernels don't understand PR_SET_NO_NEW_PRIVS and return
        // EINVAL. Don't die on such kernels.
        if (errno != EINVAL) {
            LOG_ALWAYS_FATAL("PR_SET_NO_NEW_PRIVS failed: %s", strerror(errno));
            return 12;
        }
    }

    // ******************** 第二部分 *********************
    AppRuntime runtime(argv[0], computeArgBlockSize(argc, argv));
    // Process command line arguments
    // ignore argv[0]
    argc--;
    argv++;

    // Everything up to '--' or first non '-' arg goes to the vm.
    //
    // The first argument after the VM args is the "parent dir", which
    // is currently unused.
    //
    // After the parent dir, we expect one or more the following internal
    // arguments :
    //
    // --zygote : Start in zygote mode
    // --start-system-server : Start the system server.
    // --application : Start in application (stand alone, non zygote) mode.
    // --nice-name : The nice name for this process.
    //
    // For non zygote starts, these arguments will be followed by
    // the main class name. All remaining arguments are passed to
    // the main method of this class.
    //
    // For zygote starts, all remaining arguments are passed to the zygote.
    // main function.
    //
    // Note that we must copy argument string values since we will rewrite the
    // entire argument block when we apply the nice name to argv0.


     // ******************** 第三部分 *********************  
    int i;
    for (i = 0; i < argc; i++) {
        if (argv[i][0] != '-') {
            break;
        }
        if (argv[i][1] == '-' && argv[i][2] == 0) {
            ++i; // Skip --.
            break;
        }
        runtime.addOption(strdup(argv[i]));
    }

    // Parse runtime arguments.  Stop at first unrecognized option.
    bool zygote = false;
    bool startSystemServer = false;
    bool application = false;
    String8 niceName;
    String8 className;

     // ******************** 第四部分 *********************  
    ++i;  // Skip unused "parent dir" argument.
    while (i < argc) {
        const char* arg = argv[i++];
        if (strcmp(arg, "--zygote") == 0) {
            zygote = true;
            niceName = ZYGOTE_NICE_NAME;
        } else if (strcmp(arg, "--start-system-server") == 0) {
            startSystemServer = true;
        } else if (strcmp(arg, "--application") == 0) {
            application = true;
        } else if (strncmp(arg, "--nice-name=", 12) == 0) {
            niceName.setTo(arg + 12);
        } else if (strncmp(arg, "--", 2) != 0) {
            className.setTo(arg);
            break;
        } else {
            --i;
            break;
        }
    }


     // ******************** 第五部分 *********************  
    Vector<String8> args;
    if (!className.isEmpty()) {
        // We're not in zygote mode, the only argument we need to pass
        // to RuntimeInit is the application argument.
        //
        // The Remainder of args get passed to startup class main(). Make
        // copies of them before we overwrite them with the process name.
        args.add(application ? String8("application") : String8("tool"));
        runtime.setClassNameAndArgs(className, argc - i, argv + i);
    } else {
        // We're in zygote mode.
        maybeCreateDalvikCache();

        if (startSystemServer) {
            args.add(String8("start-system-server"));
        }

        char prop[PROP_VALUE_MAX];
        if (property_get(ABI_LIST_PROPERTY, prop, NULL) == 0) {
            LOG_ALWAYS_FATAL("app_process: Unable to determine ABI list from property %s.",
                ABI_LIST_PROPERTY);
            return 11;
        }

        String8 abiFlag("--abi-list=");
        abiFlag.append(prop);
        args.add(abiFlag);

        // In zygote mode, pass all remaining arguments to the zygote
        // main() method.
        for (; i < argc; ++i) {
            args.add(String8(argv[i]));
        }
    }

     // ******************** 第六部分 *********************  
    if (!niceName.isEmpty()) {
        runtime.setArgv0(niceName.string());
        set_process_name(niceName.string());
    }

     // ******************** 第七部分 *********************  
    if (zygote) {
        runtime.start("com.android.internal.os.ZygoteInit", args, zygote);
    } else if (className) {
        runtime.start("com.android.internal.os.RuntimeInit", args, zygote);
    } else {
        fprintf(stderr, "Error: no class name or --zygote supplied.\n");
        app_usage();
        LOG_ALWAYS_FATAL("app_process: no class name or --zygote supplied.");
        return 10;
    }
}
```

我将上述代码分为七个部分，下面我们一一讲解下

###### 1.1、第一部分

这段代码与系统安全机制有关，prctl(PR\_SET\_NO\_NEW\_PRIVS) 好像是禁止改变权限，是SEAndroid的内容。Android在4.4上正式推出基于SELinux的系统安全机制

> 在Linux中，PR\_SET\_NO\_NEW\_PRIVS 当一个进程或者子进程设置了**PR\_SET\_NO\_NEW\_PRIVS**属性，则其不能访问一些无法share的操作，这是kernel 3.5后加的feature。

###### 1.2、第二部分

这部分主要是创建了AppRuntime对象，AppRuntime类继承自AndroidRuntime。关于这块，我后面会详细讲解的。

接着从命令行参数中找到虚拟机相关的参数，添加到runtime对象

###### 1.3、第三部分

从init.rc传入的参数 为**\-Xzygote /system/bin --zygote --start-system-server**

参考[init.zygote32.rc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fsystem%25252Fcore%25252Frootdir%25252Finit.zygote32.rc&objectId=1199508&objectType=1&isNewArticle=undefined)

```
service zygote /system/bin/app_process -Xzygote /system/bin --zygote --start-system-server
    class main
    socket zygote stream 660 root system
    onrestart write /sys/android_power/request_state wake
    onrestart write /sys/power/state on
    onrestart restart media
    onrestart restart netd
    writepid /dev/cpuset/foreground/tasks
```

-   **\-Xzygote**是传递给虚拟机的参数
-   **/system/bin** 是 parent dir(程序运行目录)

后面跟着的是一些**internal** 参数，其中**\--zygote**表示以zygote模式启动

###### 1.4、第四部分

将上面的内容赋给相应的变量

以**\-Xzygote /system/bin --zygote --start-system-server**为例，结果如下：

-   变量 parentDir 等于/system/bin
-   变量 niceName 等于 zyoget
-   变量 startSystemServer 等于 true
-   变量 zygote 等于 true

###### 1.5、第五部分

这部分主要是准备启动ZygoteInit类或者RuntimeInit类，这里根据类名是否存在为空，来区别是**非zyoget模式**和**zyoget模式**.

###### 1.6、第六部分

如果niceName不为空，则将本进程的名称修改为参数\*\* --nice-name\*\* 指定的字符串。缺省的情况下，niceName 的值为"zygote"或者"zygote64"。其中set\_process\_name函数用来改变进程的mi9ngcheng。setArgv0函数是用来替换启动参数串中的"app\_process"为参数。

###### 1.7、第七部分

启动Java类，如果启动参数带有 "--zygote"。则执行ZygoteInit。

这里面主要分为三种情况

-   1、zygote 模式，即启动ZygoteInit
-   2、非zygote 模式，即启动RuntimeInit
-   3、以上两种均不是

> PS：app\_process 除了 能启动Zyoget进程。

###### 2、AppRuntime类简介

上面我们提到了在app\_main.cpp的main函数里面创建了AppRuntime对象

那我们先来看下AppRuntime这个类

代码在[app\_main.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fframeworks%25252Fbase%25252Fcmds%25252Fapp_process%25252Fapp_main.cpp&objectId=1199508&objectType=1&isNewArticle=undefined) 34行

```
class AppRuntime : public AndroidRuntime
{
public:
    AppRuntime(char* argBlockStart, const size_t argBlockLength)
        : AndroidRuntime(argBlockStart, argBlockLength)
        , mClass(NULL)
    {
    }

    void setClassNameAndArgs(const String8& className, int argc, char * const *argv) {
        mClassName = className;
        for (int i = 0; i < argc; ++i) {
             mArgs.add(String8(argv[i]));
        }
    }

    virtual void onVmCreated(JNIEnv* env)
    {
        if (mClassName.isEmpty()) {
            return; // Zygote. Nothing to do here.
        }

        /*
         * This is a little awkward because the JNI FindClass call uses the
         * class loader associated with the native method we're executing in.
         * If called in onStarted (from RuntimeInit.finishInit because we're
         * launching "am", for example), FindClass would see that we're calling
         * from a boot class' native method, and so wouldn't look for the class
         * we're trying to look up in CLASSPATH. Unfortunately it needs to,
         * because the "am" classes are not boot classes.
         *
         * The easiest fix is to call FindClass here, early on before we start
         * executing boot class Java code and thereby deny ourselves access to
         * non-boot classes.
         */
        char* slashClassName = toSlashClassName(mClassName.string());
        mClass = env->FindClass(slashClassName);
        if (mClass == NULL) {
            ALOGE("ERROR: could not find class '%s'\n", mClassName.string());
        }
        free(slashClassName);

        mClass = reinterpret_cast<jclass>(env->NewGlobalRef(mClass));
    }

    virtual void onStarted()
    {
        sp<ProcessState> proc = ProcessState::self();
        ALOGV("App process: starting thread pool.\n");
        proc->startThreadPool();

        AndroidRuntime* ar = AndroidRuntime::getRuntime();
        ar->callMain(mClassName, mClass, mArgs);

        IPCThreadState::self()->stopProcess();
    }

    virtual void onZygoteInit()
    {
        sp<ProcessState> proc = ProcessState::self();
        ALOGV("App process: starting thread pool.\n");
        proc->startThreadPool();
    }

    virtual void onExit(int code)
    {
        if (mClassName.isEmpty()) {
            // if zygote
            IPCThreadState::self()->stopProcess();
        }

        AndroidRuntime::onExit(code);
    }


    String8 mClassName;
    Vector<String8> mArgs;
    jclass mClass;
};
```

通过上面代码，我们知道AppRuntime继承自AndroidRuntime类，并且重载了onVmCreated 、onStarted、onZygoteInit和onExit函数。我们发现并没有重载start函数，而在app\_main.cpp的main()函数的最后runtime.start函数，所以具体执行在AndroidRuntime类的start函数。那我们就来研究下AndroidRuntime类

###### 3、AndroidRuntime类简介

[AndroidRuntime源码地址](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fframeworks%25252Fbase%25252Fcore%25252Fjni%25252FAndroidRuntime.cpp&objectId=1199508&objectType=1&isNewArticle=undefined)

> AndroidRuntime类是安卓底层系统超级重要的一个类，它负责启动虚拟机以及Java线程。AndroidRuntime类是在一个进程中只有一个实例对象，并将其保存在全局变量gCurRuntime中。

下面我们就来看下它的构造函数，代码在[AndroidRuntime.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fframeworks%25252Fbase%25252Fcore%25252Fjni%25252FAndroidRuntime.cpp&objectId=1199508&objectType=1&isNewArticle=undefined) 238行

```
AndroidRuntime::AndroidRuntime(char* argBlockStart, const size_t argBlockLength) :
        mExitWithoutCleanup(false),
        mArgBlockStart(argBlockStart),
        mArgBlockLength(argBlockLength)
{
    SkGraphics::Init();
    // There is also a global font cache, but its budget is specified by
    // SK_DEFAULT_FONT_CACHE_COUNT_LIMIT and SK_DEFAULT_FONT_CACHE_LIMIT.

    // Pre-allocate enough space to hold a fair number of options.    
    mOptions.setCapacity(20);
    

    assert(gCurRuntime == NULL);        // one per process
    gCurRuntime = this;
}
```

构造函数，代码很少，一共就4行，我将其分为3个部分

-   **SkGraphics::Init();**： 这里主要是初始化skia图形系统。skia是google的第一个底层的图形、图像、动画、SVG、文本等多方面的图形图，是Android图形系统的引擎。skia作为第三方软件放在external目录下： external/skia/。后面附了一个skia结构图
-   **mOptions.setCapacity(20);**： 预先分配空间来存放传入虚拟机的参数
-   **gCurRuntime = this;**： 首先通过的断言判断gCurRuntime是否为空，保证只能被初始化一次

![](https://ask.qcloudimg.com/http-save/yehe-2957818/26zlla4s94.png)

kia结构图.png

PS：在Android类的构造函数中，Android 5.0以前会对Skia图形系统做设置，把系统底层使用的图形格式设为RGB565，一种16位图形格式。16图像格式没有24位的图形格式颜色丰富，但是能节省内存的使用。Android 5.0以后，对skia图形的系统设置去掉了，只保留有调用 **mOptions.setCapacity(20);**来设置虚拟机的参数

###### 4、启动虚拟机即AndroidRuntime的start()函数

在前面的时候，我们知道在main()函数的最后，调用了AppRuntime类的start()函数来启动，通过上面我们知道，AppRuntime类start()函数，其实是调用的AndroidRuntime的start()函数，那我们来看下下**AndroidRuntime**的**start()**函数里面的具体实现，如下：

```
/*
 * Start the Android runtime.  This involves starting the virtual machine
 * and calling the "static void main(String[] args)" method in the class
 * named by "className".
 *
 * Passes the main function two arguments, the class name and the specified
 * options string.
 */
void AndroidRuntime::start(const char* className, const Vector<String8>& options, bool zygote)
{
    //******************* 第一部分**********************
    ALOGD(">>>>>> START %s uid %d <<<<<<\n",
            className != NULL ? className : "(unknown)", getuid());

    static const String8 startSystemServer("start-system-server");

    /*
     * 'startSystemServer == true' means runtime is obsolete and not run from
     * init.rc anymore, so we print out the boot start event here.
     */
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == startSystemServer) {
           /* track our progress through the boot sequence */
           const int LOG_BOOT_PROGRESS_START = 3000;
           LOG_EVENT_LONG(LOG_BOOT_PROGRESS_START,  ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));
        }
    }

     //******************* 第二部分**********************
    const char* rootDir = getenv("ANDROID_ROOT");
    if (rootDir == NULL) {
        rootDir = "/system";
        if (!hasDir("/system")) {
            LOG_FATAL("No root directory specified, and /android does not exist.");
            return;
        }
        setenv("ANDROID_ROOT", rootDir, 1);
    }

    //const char* kernelHack = getenv("LD_ASSUME_KERNEL");
    //ALOGD("Found LD_ASSUME_KERNEL='%s'\n", kernelHack);


    //******************* 第三部分**********************
    /* start the virtual machine */
    JniInvocation jni_invocation;
    jni_invocation.Init(NULL);
    JNIEnv* env;
    if (startVm(&mJavaVM, &env, zygote) != 0) {
        return;
    }

    //******************* 第四部分**********************
    onVmCreated(env);


    //******************* 第五部分**********************
    /*
     * Register android functions.
     */
    if (startReg(env) < 0) {
        ALOGE("Unable to register all android natives\n");
        return;
    }

   //******************* 第六部分**********************
    /*
     * We want to call main() with a String array with arguments in it.
     * At present we have two arguments, the class name and an option string.
     * Create an array to hold them.
     */
    jclass stringClass;
    jobjectArray strArray;
    jstring classNameStr;

    stringClass = env->FindClass("java/lang/String");
    assert(stringClass != NULL);
    strArray = env->NewObjectArray(options.size() + 1, stringClass, NULL);
    assert(strArray != NULL);
    classNameStr = env->NewStringUTF(className);
    assert(classNameStr != NULL);
    env->SetObjectArrayElement(strArray, 0, classNameStr);

    for (size_t i = 0; i < options.size(); ++i) {
        jstring optionsStr = env->NewStringUTF(options.itemAt(i).string());
        assert(optionsStr != NULL);
        env->SetObjectArrayElement(strArray, i + 1, optionsStr);
    }


   //******************* 第七部分**********************
    /*
     * Start VM.  This thread becomes the main thread of the VM, and will
     * not return until the VM exits.
     */
    char* slashClassName = toSlashClassName(className);
    jclass startClass = env->FindClass(slashClassName);
    if (startClass == NULL) {
        ALOGE("JavaVM unable to locate class '%s'\n", slashClassName);
        /* keep going */
    } else {
        jmethodID startMeth = env->GetStaticMethodID(startClass, "main",
            "([Ljava/lang/String;)V");
        if (startMeth == NULL) {
            ALOGE("JavaVM unable to find main() in '%s'\n", className);
            /* keep going */
        } else {
            env->CallStaticVoidMethod(startClass, startMeth, strArray);

#if 0
            if (env->ExceptionCheck())
                threadExitUncaughtException(env);
#endif
        }
    }
    free(slashClassName);

    ALOGD("Shutting down VM\n");
    if (mJavaVM->DetachCurrentThread() != JNI_OK)
        ALOGW("Warning: unable to detach main thread\n");
    if (mJavaVM->DestroyJavaVM() != 0)
        ALOGW("Warning: VM did not shut down cleanly\n");
}
```

哈哈，这个方法有注释，太好了，先来翻译一下注释，如下

> 启动Android运行时，包括启动虚拟机并调用的"className"的静态main()方法。 这个mian方法的两个入参一个是类名，一个是特定选项的字符串

这个函数里面内容不多，但是很重要，所以我将**AndroidRuntime**的**start()**函数内部的代码划分为7部分。我们来一一讲解下

###### 4.1、第一部分——打印log

首先调用了**ALOGD方法**，用来记录日内容(ALOGD记录的日志在编译的时候时候存在，但是在运行时会被提出)，标志着Android的启动。后面跟着的for循环，来判断是否是启动systemServer(即传入的参数是或否有startSystemServer)，如果是启动systemServer，同样要打印日志。

> PS：systemTime(SYSTEM\_TIME\_MONOTONIC)获取系统当前时间

###### 4.2、第二部分——获取系统目录

系统目录从环境变量**ANDROID\_ROOT**中读取。如果说去失败，则默认设置目录为**"/system"**。如果连**"/system"**也没有，则Zygote进程会退出。

> PS：系统目录是在init进程中创建出来的

###### 4.3、第三部分——启动虚拟机

通过jni\_invocation.Init(NULL)完成jni接口的初始化。接着是创建虚拟机的代码，即调用startVm函数。

> Android系统从5.0之后，启动的将是ART虚拟机。关于之前Dalvik的启动代码[4.3的AndroidRuntime.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F4.3_r2.1%25252Fxref%25252Fframeworks%25252Fbase%25252Fcore%25252Fjni%25252FAndroidRuntime.cpp&objectId=1199508&objectType=1&isNewArticle=undefined)的267 大家可以自行对比。关于启动ART虚拟机请参考本片文章**第五部分、动虚拟机**

###### 4.4、第四部分——调用虚函数onVmCreated

onVimCreate()是一个虚函数，调用它实际上调用的是继承类的AppRuntime中的重载函数。

代码在[app\_main.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fframeworks%25252Fbase%25252Fcmds%25252Fapp_process%25252Fapp_main.cpp&objectId=1199508&objectType=1&isNewArticle=undefined) 50行

```
    virtual void onVmCreated(JNIEnv* env)
    {
        if (mClassName.isEmpty()) {
            return; // Zygote. Nothing to do here.
        }

        /*
         * This is a little awkward because the JNI FindClass call uses the
         * class loader associated with the native method we're executing in.
         * If called in onStarted (from RuntimeInit.finishInit because we're
         * launching "am", for example), FindClass would see that we're calling
         * from a boot class' native method, and so wouldn't look for the class
         * we're trying to look up in CLASSPATH. Unfortunately it needs to,
         * because the "am" classes are not boot classes.
         *
         * The easiest fix is to call FindClass here, early on before we start
         * executing boot class Java code and thereby deny ourselves access to
         * non-boot classes.
         */
        char* slashClassName = toSlashClassName(mClassName.string());
        mClass = env->FindClass(slashClassName);
        if (mClass == NULL) {
            ALOGE("ERROR: could not find class '%s'\n", mClassName.string());
        }
        free(slashClassName);

        mClass = reinterpret_cast<jclass>(env->NewGlobalRef(mClass));
    }
```

从上面代码中，我们知道如果是Zygote的启动，则onVmCreated实际上什么也不做，如果不是Zygote模式，则简单的根据类名，获取类，并

只是简单根据类名获取了类，并释放了类名

###### 4.5、第五部分——注册系统的JNI函数

startReg()函数通过调用register\_jni\_procs()函数将全局的gRegJNI中的本地JNI函数在虚拟机中注册，这部分的解析请参考[3、Android跨进程通信IPC之3——关于"JNI"的那些事](https://cloud.tencent.com/developer/article/1199091?from_column=20421&from=20421)中的**4、JNI查找方式**

###### 4.6、第六部分——为启动Java类的main函数做准备

这部分代码首先调用NewObjectArray函数，来创建一个包含options.size() + 1的数组；类型是“java/lang/String”，然后通过调用SetObjectArrayElement给NewObjectArray的每个元素来赋值，这里面时特别指出，第一个元素是类名——"classNameStr"。

###### 4.7、第六部分——调用Zygoteinit类的main()函数

它首先通过GetStaticMethodI函数来获取main()方法的id。接下来就是使用CallStaticVoidMethod函数来调用Java层方法了。至此Zygoet的初始化将转移到Java。当然如果不是启动Zyogte，执行的Java将是RunnableInit

###### 总结

AndroidRuntime类的start函数其实主要就是做了3件事情：

-   1、创建了一个JniInvocation的实例，并且调用它的成员函数init来初始化JNI环境。
-   2、调用AndroidRuntime类的成员函数startVm来创建一个虚拟机即其对应的JNI接口，即创建一个JavaVM接口和一个JNIEnv接口。
-   3、有了上述的JavaVM接口和JNIEnv接口之后，就可以在Zygote进程中加载指定的class了。

至此，zyogte进程 (C篇) 已经结束了。后面是zyogte进程的Java篇了，我们会在下一篇文章xxx里面讲解后续的流程。上面我们来看

### 三、关于虚拟机简介

Android系统在4.4版本的时候，发布了一个ART运行时，来替换之前的一直使用的Dalvik虚拟机，接来解决之前Dalvik虚拟机的性能问题，关于ART的实现原理，我后续会单独讲解。这里先略过。这里先简单的讲解下虚拟机。Dalvik虚拟机实则是也算是一个Java虚拟机，只不过它的执行的不是class文件，而是dex文件。所以，ART运行时的设计的最perfect的方案肯定也是类似于一个Dalvik虚拟机的形式。其实无论是ART虚拟机还是Dalvik虚拟机在接口上与Java虚拟机基本上是一致的(但是其内部的机制是不一样的)。这样才能无缝隙的衔接。那我们来简单的看下这三个虚拟机(Java虚拟机、Dalvik虚拟机、ART运行时) 如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/wn8m966qgs.png)

image.png

上图是Java虚拟机、Dalvik虚拟机与ART运行时的关系

###### 1、相同点

通过上图我们知道，Dalvik虚拟机和ART运行时都实现了3个抽象Java虚拟机的接口，即：

-   1、JNI\_GetDefaultJavaVMInitArgs：获取虚拟机的默认是初始化参数
-   2、JNI\_CreateJavaVM：在进程中创建虚拟机实例
-   3、JNI\_GetCreatedJavaVMs：获取进程中创建虚拟机实例

在Android系统中，Davik虚拟机实现在libdvm.so中，ART运行时是现在libart.so中。也就是说，libvm.so和libart.so导出了JNI\_GetDefaultJavaVMInitArgs、JNI\_CreateJavaVM和JNI\_GetCreatedJavaVMs这三个接口，供外界调用。而且，Android系统还提供了一个系统属性在**persist.sys.dalvik.vm.lib.2**（android4.4以前是**persist.sys.dalvik.vm.lib**），它要么取值为libdvm.so，表示当前是Dalvik虚拟机；要么取值为**libdvm.so**，表示当前是ART运行时。

###### 2、不同点

以上描述的是Dalvik虚拟机与ART运行时的共同之处，当然它们之间还有不同点，最大的不通电在于，Dalvik虚拟机执行的是dex字节码，ART虚拟机执行的是本地机器吗。这意味着Dalvik虚拟机包含一个解释器，用来执行dex字节码，当前从Android2.2开始，也包含了已个JIT(Just-In-Time)，用来在运行时动态地将执行频率很高的dex字节码翻译成本地机器码，然后再执行。通过JIT，就可以有效地提高Dalvik虚拟机的执行效率。但是，dex字节翻译成本地机器码是发生在应用程序运行过程中的，并且应用程序每一次重新运行的时候，都要做重做这个翻译的工作。所以即使用了JIT，Dalvik虚拟机总体的性能还是不能与直接执行本地机器码的ART运行时相比。

###### 3、ART原理简介

那我们来看下，Android运行时从Dalvik虚拟机替换成ART运行时，并不要求开发者重新将自己的应用直接编译成目标的机器码。也就是说，其实开发者开发出来的应用程序经过编译和打包之后，仍然是一个包含dex字节码的APK文件。既然应用程序包含的仍然是dex字节码，而ART运行时需要的是本地机器码，这必然要有一个翻译的过程。这个翻译的过程。这个翻译的过程当然不能发生在应用程序运行的时候，否则的话就和Dalvik虚拟机JIT一样，并没有解决性能的问题。在计算机的世界里，与JIT相对的是AOT，即Ahead-Of-Time的简称，它发生在程序运行时之前。我们用静态语言(比如C/C++) 来开发的应用程序的时候，编译器直接就把他们翻译成目标机器码。这种静态语言的编译方式也是AOT的一种。ART运行时并不要求开发者将自己的应用直接编译成目标机器码。这样，将应用的dex字节码翻译成本地机器码的最恰当的AOT时机就是发生在应用安装的时候。在没有ART虚拟机之前，应用的安装过程，其实也会执行一次"翻译"的过程。只不过这个"翻译"过程是将dex字节码进行优化，这也就是由dex文件生成odex文件。这个过程在安装服务PackageManagerService请求守护进程installd来执行的。从这个角度来看，在应用安装的过程中将dex字节码翻译成本地机器码对原来的应用安装流程基本上就不会产生什么影响。

### 四、启动虚拟机

启动虚拟机主要包括两部分，即

-   jni\_invocation.Init(NULL)：初始化JNI环境
-   AndroidRuntime::startVm(...)函数：启动

那我们就依次来看下：

##### (一)、jni\_invocation.Init(NULL)函数解析

代码在[JniInvocation.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Flibnativehelper%25252FJniInvocation.cpp&objectId=1199508&objectType=1&isNewArticle=undefined)

```
bool JniInvocation::Init(const char* library) {
#ifdef HAVE_ANDROID_OS
  char buffer[PROPERTY_VALUE_MAX];
#else
  char* buffer = NULL;
#endif
  // 我们知道传入的library是null，所以最后应该是library是libart.so
  library = GetLibrary(library, buffer);

  // 通过dlopen加载libdvm.so，libdvm.so是dalvik vm的核心库
  handle_ = dlopen(library, RTLD_NOW);

  if (handle_ == NULL) {
    // 如果打开libart.so失败，则直接返回。如果打开其他library失败在，额尝试打开libart.so
    if (strcmp(library, kLibraryFallback) == 0) {
      // Nothing else to try.
      ALOGE("Failed to dlopen %s: %s", library, dlerror());
      return false;
    }
    // Note that this is enough to get something like the zygote
    // running, we can't property_set here to fix this for the future
    // because we are root and not the system user. See
    // RuntimeInit.commonInit for where we fix up the property to
    // avoid future fallbacks. http://b/11463182
    ALOGW("Falling back from %s to %s after dlopen error: %s",
          library, kLibraryFallback, dlerror());
    library = kLibraryFallback;
    handle_ = dlopen(library, RTLD_NOW);
    if (handle_ == NULL) {
      ALOGE("Failed to dlopen %s: %s", library, dlerror());
      return false;
    }
  }

   // 寻找并导出JNI_GetDefaultJavaVMInitArgs_
  if (!FindSymbol(reinterpret_cast<void**>(&JNI_GetDefaultJavaVMInitArgs_),
                  "JNI_GetDefaultJavaVMInitArgs")) {
    return false;
  }

   // 寻找并导出JNI_CreateJavaVM_
  if (!FindSymbol(reinterpret_cast<void**>(&JNI_CreateJavaVM_),
                  "JNI_CreateJavaVM")) {
    return false;
  }

  // 寻找并导出JNI_GetCreatedJavaVMs_
  if (!FindSymbol(reinterpret_cast<void**>(&JNI_GetCreatedJavaVMs_),
                  "JNI_GetCreatedJavaVMs")) {
    return false;
  }
  return true;
}
```

JniInvocation::Init函数主要就是通过dlopen函数打开libart.so，之后利用dlsym函数寻找并导出Java虚拟机的三个接口，这样就可以通过这三个接口创建并访问虚拟机了。从而完成初始化JNI环境。

那为什么是**libart.so**而不是**libdvm.so**，因为这里面调用了一个GetLibrary函数，我们来看下

代码在[JniInvocation.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Flibnativehelper%25252FJniInvocation.cpp&objectId=1199508&objectType=1&isNewArticle=undefined) 60行

```
#ifdef HAVE_ANDROID_OS
static const char* kLibrarySystemProperty = "persist.sys.dalvik.vm.lib.2";
static const char* kDebuggableSystemProperty = "ro.debuggable";
static const char* kDebuggableFallback = "0";  // Not debuggable.
#endif
static const char* kLibraryFallback = "libart.so";

template<typename T> void UNUSED(const T&) {}

const char* JniInvocation::GetLibrary(const char* library, char* buffer) {
#ifdef HAVE_ANDROID_OS
  const char* default_library;

  char debuggable[PROPERTY_VALUE_MAX];
  property_get(kDebuggableSystemProperty, debuggable, kDebuggableFallback);

  if (strcmp(debuggable, "1") != 0) {
    // Not a debuggable build.
    // Do not allow arbitrary library. Ignore the library parameter. This
    // will also ignore the default library, but initialize to fallback
    // for cleanliness.
    library = kLibraryFallback;
    default_library = kLibraryFallback;
  } else {
    // Debuggable build.
    // Accept the library parameter. For the case it is NULL, load the default
    // library from the system property.
    if (buffer != NULL) {
      property_get(kLibrarySystemProperty, buffer, kLibraryFallback);
      default_library = buffer;
    } else {
      // No buffer given, just use default fallback.
      default_library = kLibraryFallback;
    }
  }
#else
  UNUSED(buffer);
  const char* default_library = kLibraryFallback;
#endif
  if (library == NULL) {
    library = default_library;
  }

  return library;
}
```

这个函数很简单，就是读取系统属性**persist.sys.dalvik.vm.lib.2**的值，在我们知道在Android6.0里面这个系统属性的值为libart.so，同时它也给出了默认的系统属性的值为libart.so。所以这个方法最后就是输出libdvm.so库

所以我们可以看出JniInvocation的init函数实际上就是会根据**persist.sys.dalvik.vm.lib.2**的值来初始化ART运行时环境。接下来我们继续看AndroidRuntime类的成员函数startVm的实现：

##### (二)、startVm(JavaVM\*\* pJavaVM, JNIEnv\*\* pEnv, bool zygote)”函数解析

启动虚拟机的函数是“startVm(JavaVM\*\* pJavaVM, JNIEnv\*\* pEnv, bool zygote)”，代码在[AndroidRuntime.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fframeworks%25252Fbase%25252Fcore%25252Fjni%25252FAndroidRuntime.cpp&objectId=1199508&objectType=1&isNewArticle=undefined) 569行

```
/*
 * Start the Dalvik Virtual Machine.
 *
 * Various arguments, most determined by system properties, are passed in.
 * The "mOptions" vector is updated.
 *
 * CAUTION: when adding options in here, be careful not to put the
 * char buffer inside a nested scope.  Adding the buffer to the
 * options using mOptions.add() does not copy the buffer, so if the
 * buffer goes out of scope the option may be overwritten.  It's best
 * to put the buffer at the top of the function so that it is more
 * unlikely that someone will surround it in a scope at a later time
 * and thus introduce a bug.
 *
 * Returns 0 on success.
 */
int AndroidRuntime::startVm(JavaVM** pJavaVM, JNIEnv** pEnv, bool zygote)
{

    //******************* 第1部分**********************
    JavaVMInitArgs initArgs;
    char propBuf[PROPERTY_VALUE_MAX];
    char stackTraceFileBuf[sizeof("-Xstacktracefile:")-1 + PROPERTY_VALUE_MAX];
    char jniOptsBuf[sizeof("-Xjniopts:")-1 + PROPERTY_VALUE_MAX];
    char heapstartsizeOptsBuf[sizeof("-Xms")-1 + PROPERTY_VALUE_MAX];
    char heapsizeOptsBuf[sizeof("-Xmx")-1 + PROPERTY_VALUE_MAX];
    char heapgrowthlimitOptsBuf[sizeof("-XX:HeapGrowthLimit=")-1 + PROPERTY_VALUE_MAX];
    char heapminfreeOptsBuf[sizeof("-XX:HeapMinFree=")-1 + PROPERTY_VALUE_MAX];
    char heapmaxfreeOptsBuf[sizeof("-XX:HeapMaxFree=")-1 + PROPERTY_VALUE_MAX];
    char usejitOptsBuf[sizeof("-Xusejit:")-1 + PROPERTY_VALUE_MAX];
    char jitcodecachesizeOptsBuf[sizeof("-Xjitcodecachesize:")-1 + PROPERTY_VALUE_MAX];
    char jitthresholdOptsBuf[sizeof("-Xjitthreshold:")-1 + PROPERTY_VALUE_MAX];
    char gctypeOptsBuf[sizeof("-Xgc:")-1 + PROPERTY_VALUE_MAX];
    char backgroundgcOptsBuf[sizeof("-XX:BackgroundGC=")-1 + PROPERTY_VALUE_MAX];
    char heaptargetutilizationOptsBuf[sizeof("-XX:HeapTargetUtilization=")-1 + PROPERTY_VALUE_MAX];
    char cachePruneBuf[sizeof("-Xzygote-max-boot-retry=")-1 + PROPERTY_VALUE_MAX];
    char dex2oatXmsImageFlagsBuf[sizeof("-Xms")-1 + PROPERTY_VALUE_MAX];
    char dex2oatXmxImageFlagsBuf[sizeof("-Xmx")-1 + PROPERTY_VALUE_MAX];
    char dex2oatXmsFlagsBuf[sizeof("-Xms")-1 + PROPERTY_VALUE_MAX];
    char dex2oatXmxFlagsBuf[sizeof("-Xmx")-1 + PROPERTY_VALUE_MAX];
    char dex2oatCompilerFilterBuf[sizeof("--compiler-filter=")-1 + PROPERTY_VALUE_MAX];
    char dex2oatImageCompilerFilterBuf[sizeof("--compiler-filter=")-1 + PROPERTY_VALUE_MAX];
    char dex2oatThreadsBuf[sizeof("-j")-1 + PROPERTY_VALUE_MAX];
    char dex2oatThreadsImageBuf[sizeof("-j")-1 + PROPERTY_VALUE_MAX];
    char dex2oat_isa_variant_key[PROPERTY_KEY_MAX];
    char dex2oat_isa_variant[sizeof("--instruction-set-variant=") -1 + PROPERTY_VALUE_MAX];
    char dex2oat_isa_features_key[PROPERTY_KEY_MAX];
    char dex2oat_isa_features[sizeof("--instruction-set-features=") -1 + PROPERTY_VALUE_MAX];
    char dex2oatFlagsBuf[PROPERTY_VALUE_MAX];
    char dex2oatImageFlagsBuf[PROPERTY_VALUE_MAX];
    char extraOptsBuf[PROPERTY_VALUE_MAX];
    char voldDecryptBuf[PROPERTY_VALUE_MAX];
    enum {
      kEMDefault,
      kEMIntPortable,
      kEMIntFast,
      kEMJitCompiler,
    } executionMode = kEMDefault;
    char profilePeriod[sizeof("-Xprofile-period:")-1 + PROPERTY_VALUE_MAX];
    char profileDuration[sizeof("-Xprofile-duration:")-1 + PROPERTY_VALUE_MAX];
    char profileInterval[sizeof("-Xprofile-interval:")-1 + PROPERTY_VALUE_MAX];
    char profileBackoff[sizeof("-Xprofile-backoff:")-1 + PROPERTY_VALUE_MAX];
    char profileTopKThreshold[sizeof("-Xprofile-top-k-threshold:")-1 + PROPERTY_VALUE_MAX];
    char profileTopKChangeThreshold[sizeof("-Xprofile-top-k-change-threshold:")-1 +
                                    PROPERTY_VALUE_MAX];
    char profileType[sizeof("-Xprofile-type:")-1 + PROPERTY_VALUE_MAX];
    char profileMaxStackDepth[sizeof("-Xprofile-max-stack-depth:")-1 + PROPERTY_VALUE_MAX];
    char localeOption[sizeof("-Duser.locale=") + PROPERTY_VALUE_MAX];
    char lockProfThresholdBuf[sizeof("-Xlockprofthreshold:")-1 + PROPERTY_VALUE_MAX];
    char nativeBridgeLibrary[sizeof("-XX:NativeBridge=") + PROPERTY_VALUE_MAX];
    char cpuAbiListBuf[sizeof("--cpu-abilist=") + PROPERTY_VALUE_MAX];
    char methodTraceFileBuf[sizeof("-Xmethod-trace-file:") + PROPERTY_VALUE_MAX];
    char methodTraceFileSizeBuf[sizeof("-Xmethod-trace-file-size:") + PROPERTY_VALUE_MAX];
    char fingerprintBuf[sizeof("-Xfingerprint:") + PROPERTY_VALUE_MAX];


    //******************* 第2部分**********************
    bool checkJni = false;
    property_get("dalvik.vm.checkjni", propBuf, "");
    if (strcmp(propBuf, "true") == 0) {
        checkJni = true;
    } else if (strcmp(propBuf, "false") != 0) {
        /* property is neither true nor false; fall back on kernel parameter */
        property_get("ro.kernel.android.checkjni", propBuf, "");
        if (propBuf[0] == '1') {
            checkJni = true;
        }
    }

    ALOGD("CheckJNI is %s\n", checkJni ? "ON" : "OFF");
    if (checkJni) {
        /* extended JNI checking */
        addOption("-Xcheck:jni");

        /* with -Xcheck:jni, this provides a JNI function call trace */
        //addOption("-verbose:jni");
    }


    //******************* 第3部分**********************
    property_get("dalvik.vm.execution-mode", propBuf, "");
    if (strcmp(propBuf, "int:portable") == 0) {
        executionMode = kEMIntPortable;
    } else if (strcmp(propBuf, "int:fast") == 0) {
        executionMode = kEMIntFast;
    } else if (strcmp(propBuf, "int:jit") == 0) {
        executionMode = kEMJitCompiler;
    }


    //******************* 第4部分**********************
    parseRuntimeOption("dalvik.vm.stack-trace-file", stackTraceFileBuf, "-Xstacktracefile:");

    //******************* 第5部分**********************
    strcpy(jniOptsBuf, "-Xjniopts:");
    if (parseRuntimeOption("dalvik.vm.jniopts", jniOptsBuf, "-Xjniopts:")) {
        ALOGI("JNI options: '%s'\n", jniOptsBuf);
    }

    /* route exit() to our handler */
    addOption("exit", (void*) runtime_exit);

    /* route fprintf() to our handler */
    addOption("vfprintf", (void*) runtime_vfprintf);

    /* register the framework-specific "is sensitive thread" hook */
    addOption("sensitiveThread", (void*) runtime_isSensitiveThread);

    /* enable verbose; standard options are { jni, gc, class } */
    //addOption("-verbose:jni");
    addOption("-verbose:gc");
    //addOption("-verbose:class");


    //******************* 第6部分**********************
    /*
     * The default starting and maximum size of the heap.  Larger
     * values should be specified in a product property override.
     */
    parseRuntimeOption("dalvik.vm.heapstartsize", heapstartsizeOptsBuf, "-Xms", "4m");
    parseRuntimeOption("dalvik.vm.heapsize", heapsizeOptsBuf, "-Xmx", "16m");

    parseRuntimeOption("dalvik.vm.heapgrowthlimit", heapgrowthlimitOptsBuf, "-XX:HeapGrowthLimit=");
    parseRuntimeOption("dalvik.vm.heapminfree", heapminfreeOptsBuf, "-XX:HeapMinFree=");
    parseRuntimeOption("dalvik.vm.heapmaxfree", heapmaxfreeOptsBuf, "-XX:HeapMaxFree=");
    parseRuntimeOption("dalvik.vm.heaptargetutilization",
                       heaptargetutilizationOptsBuf,
                       "-XX:HeapTargetUtilization=");



    //******************* 第7部分**********************
    /*
     * JIT related options.
     */
    parseRuntimeOption("dalvik.vm.usejit", usejitOptsBuf, "-Xusejit:");
    parseRuntimeOption("dalvik.vm.jitcodecachesize", jitcodecachesizeOptsBuf, "-Xjitcodecachesize:");
    parseRuntimeOption("dalvik.vm.jitthreshold", jitthresholdOptsBuf, "-Xjitthreshold:");

    property_get("ro.config.low_ram", propBuf, "");
    if (strcmp(propBuf, "true") == 0) {
      addOption("-XX:LowMemoryMode");
    }

    parseRuntimeOption("dalvik.vm.gctype", gctypeOptsBuf, "-Xgc:");
    parseRuntimeOption("dalvik.vm.backgroundgctype", backgroundgcOptsBuf, "-XX:BackgroundGC=");

   //******************* 第8部分**********************
    /*
     * Enable debugging only for apps forked from zygote.
     * Set suspend=y to pause during VM init and use android ADB transport.
     */
    if (zygote) {
      addOption("-agentlib:jdwp=transport=dt_android_adb,suspend=n,server=y");
    }

    parseRuntimeOption("dalvik.vm.lockprof.threshold",
                       lockProfThresholdBuf,
                       "-Xlockprofthreshold:");

    if (executionMode == kEMIntPortable) {
        addOption("-Xint:portable");
    } else if (executionMode == kEMIntFast) {
        addOption("-Xint:fast");
    } else if (executionMode == kEMJitCompiler) {
        addOption("-Xint:jit");
    }

    // If we are booting without the real /data, don't spend time compiling.
    property_get("vold.decrypt", voldDecryptBuf, "");
    bool skip_compilation = ((strcmp(voldDecryptBuf, "trigger_restart_min_framework") == 0) ||
                             (strcmp(voldDecryptBuf, "1") == 0));

    // Extra options for boot.art/boot.oat image generation.
    parseCompilerRuntimeOption("dalvik.vm.image-dex2oat-Xms", dex2oatXmsImageFlagsBuf,
                               "-Xms", "-Ximage-compiler-option");
    parseCompilerRuntimeOption("dalvik.vm.image-dex2oat-Xmx", dex2oatXmxImageFlagsBuf,
                               "-Xmx", "-Ximage-compiler-option");
    if (skip_compilation) {
        addOption("-Ximage-compiler-option");
        addOption("--compiler-filter=verify-none");
    } else {
        parseCompilerOption("dalvik.vm.image-dex2oat-filter", dex2oatImageCompilerFilterBuf,
                            "--compiler-filter=", "-Ximage-compiler-option");
    }


   //******************* 第9部分**********************
    // Make sure there is a preloaded-classes file.
    if (!hasFile("/system/etc/preloaded-classes")) {
        ALOGE("Missing preloaded-classes file, /system/etc/preloaded-classes not found: %s\n",
              strerror(errno));
        return -1;
    }
    addOption("-Ximage-compiler-option");
    addOption("--image-classes=/system/etc/preloaded-classes");

     //******************* 第10部分**********************
    // If there is a compiled-classes file, push it.
    if (hasFile("/system/etc/compiled-classes")) {
        addOption("-Ximage-compiler-option");
        addOption("--compiled-classes=/system/etc/compiled-classes");
    }

    property_get("dalvik.vm.image-dex2oat-flags", dex2oatImageFlagsBuf, "");
    parseExtraOpts(dex2oatImageFlagsBuf, "-Ximage-compiler-option");

    // Extra options for DexClassLoader.
    parseCompilerRuntimeOption("dalvik.vm.dex2oat-Xms", dex2oatXmsFlagsBuf,
                               "-Xms", "-Xcompiler-option");
    parseCompilerRuntimeOption("dalvik.vm.dex2oat-Xmx", dex2oatXmxFlagsBuf,
                               "-Xmx", "-Xcompiler-option");
    if (skip_compilation) {
        addOption("-Xcompiler-option");
        addOption("--compiler-filter=verify-none");

        // We skip compilation when a minimal runtime is brought up for decryption. In that case
        // /data is temporarily backed by a tmpfs, which is usually small.
        // If the system image contains prebuilts, they will be relocated into the tmpfs. In this
        // specific situation it is acceptable to *not* relocate and run out of the prebuilts
        // directly instead.
        addOption("--runtime-arg");
        addOption("-Xnorelocate");
    } else {
        parseCompilerOption("dalvik.vm.dex2oat-filter", dex2oatCompilerFilterBuf,
                            "--compiler-filter=", "-Xcompiler-option");
    }
    parseCompilerOption("dalvik.vm.dex2oat-threads", dex2oatThreadsBuf, "-j", "-Xcompiler-option");
    parseCompilerOption("dalvik.vm.image-dex2oat-threads", dex2oatThreadsImageBuf, "-j",
                        "-Ximage-compiler-option");

    // The runtime will compile a boot image, when necessary, not using installd. Thus, we need to
    // pass the instruction-set-features/variant as an image-compiler-option.
    // TODO: Find a better way for the instruction-set.
#if defined(__arm__)
    constexpr const char* instruction_set = "arm";
#elif defined(__aarch64__)
    constexpr const char* instruction_set = "arm64";
#elif defined(__mips__) && !defined(__LP64__)
    constexpr const char* instruction_set = "mips";
#elif defined(__mips__) && defined(__LP64__)
    constexpr const char* instruction_set = "mips64";
#elif defined(__i386__)
    constexpr const char* instruction_set = "x86";
#elif defined(__x86_64__)
    constexpr const char* instruction_set = "x86_64";
#else
    constexpr const char* instruction_set = "unknown";
#endif
    // Note: it is OK to reuse the buffer, as the values are exactly the same between
    //       * compiler-option, used for runtime compilation (DexClassLoader)
    //       * image-compiler-option, used for boot-image compilation on device

    // Copy the variant.
    sprintf(dex2oat_isa_variant_key, "dalvik.vm.isa.%s.variant", instruction_set);
    parseCompilerOption(dex2oat_isa_variant_key, dex2oat_isa_variant,
                        "--instruction-set-variant=", "-Ximage-compiler-option");
    parseCompilerOption(dex2oat_isa_variant_key, dex2oat_isa_variant,
                        "--instruction-set-variant=", "-Xcompiler-option");
    // Copy the features.
    sprintf(dex2oat_isa_features_key, "dalvik.vm.isa.%s.features", instruction_set);
    parseCompilerOption(dex2oat_isa_features_key, dex2oat_isa_features,
                        "--instruction-set-features=", "-Ximage-compiler-option");
    parseCompilerOption(dex2oat_isa_features_key, dex2oat_isa_features,
                        "--instruction-set-features=", "-Xcompiler-option");


    property_get("dalvik.vm.dex2oat-flags", dex2oatFlagsBuf, "");
    parseExtraOpts(dex2oatFlagsBuf, "-Xcompiler-option");

    /* extra options; parse this late so it overrides others */
    property_get("dalvik.vm.extra-opts", extraOptsBuf, "");
    parseExtraOpts(extraOptsBuf, NULL);

    /* Set the properties for locale */
    {
        strcpy(localeOption, "-Duser.locale=");
        const std::string locale = readLocale();
        strncat(localeOption, locale.c_str(), PROPERTY_VALUE_MAX);
        addOption(localeOption);
    }

    /*
     * Set profiler options
     */
    // Whether or not the profiler should be enabled.
    property_get("dalvik.vm.profiler", propBuf, "0");
    if (propBuf[0] == '1') {
        addOption("-Xenable-profiler");
    }

    // Whether the profile should start upon app startup or be delayed by some random offset
    // (in seconds) that is bound between 0 and a fixed value.
    property_get("dalvik.vm.profile.start-immed", propBuf, "0");
    if (propBuf[0] == '1') {
        addOption("-Xprofile-start-immediately");
    }

    // Number of seconds during profile runs.
    parseRuntimeOption("dalvik.vm.profile.period-secs", profilePeriod, "-Xprofile-period:");

    // Length of each profile run (seconds).
    parseRuntimeOption("dalvik.vm.profile.duration-secs",
                       profileDuration,
                       "-Xprofile-duration:");

    // Polling interval during profile run (microseconds).
    parseRuntimeOption("dalvik.vm.profile.interval-us", profileInterval, "-Xprofile-interval:");

    // Coefficient for period backoff.  The the period is multiplied
    // by this value after each profile run.
    parseRuntimeOption("dalvik.vm.profile.backoff-coeff", profileBackoff, "-Xprofile-backoff:");

    // Top K% of samples that are considered relevant when
    // deciding if the app should be recompiled.
    parseRuntimeOption("dalvik.vm.profile.top-k-thr",
                       profileTopKThreshold,
                       "-Xprofile-top-k-threshold:");

    // The threshold after which a change in the structure of the
    // top K% profiled samples becomes significant and triggers
    // recompilation. A change in profile is considered
    // significant if X% (top-k-change-threshold) of the top K%
    // (top-k-threshold property) samples has changed.
    parseRuntimeOption("dalvik.vm.profile.top-k-ch-thr",
                       profileTopKChangeThreshold,
                       "-Xprofile-top-k-change-threshold:");

    // Type of profile data.
    parseRuntimeOption("dalvik.vm.profiler.type", profileType, "-Xprofile-type:");

    // Depth of bounded stack data
    parseRuntimeOption("dalvik.vm.profile.stack-depth",
                       profileMaxStackDepth,
                       "-Xprofile-max-stack-depth:");

    /*
     * Tracing options.
     */
    property_get("dalvik.vm.method-trace", propBuf, "false");
    if (strcmp(propBuf, "true") == 0) {
        addOption("-Xmethod-trace");
        parseRuntimeOption("dalvik.vm.method-trace-file",
                           methodTraceFileBuf,
                           "-Xmethod-trace-file:");
        parseRuntimeOption("dalvik.vm.method-trace-file-siz",
                           methodTraceFileSizeBuf,
                           "-Xmethod-trace-file-size:");
        property_get("dalvik.vm.method-trace-stream", propBuf, "false");
        if (strcmp(propBuf, "true") == 0) {
            addOption("-Xmethod-trace-stream");
        }
    }

    // Native bridge library. "0" means that native bridge is disabled.
    property_get("ro.dalvik.vm.native.bridge", propBuf, "");
    if (propBuf[0] == '\0') {
        ALOGW("ro.dalvik.vm.native.bridge is not expected to be empty");
    } else if (strcmp(propBuf, "0") != 0) {
        snprintf(nativeBridgeLibrary, sizeof("-XX:NativeBridge=") + PROPERTY_VALUE_MAX,
                 "-XX:NativeBridge=%s", propBuf);
        addOption(nativeBridgeLibrary);
    }

#if defined(__LP64__)
    const char* cpu_abilist_property_name = "ro.product.cpu.abilist64";
#else
    const char* cpu_abilist_property_name = "ro.product.cpu.abilist32";
#endif  // defined(__LP64__)
    property_get(cpu_abilist_property_name, propBuf, "");
    if (propBuf[0] == '\0') {
        ALOGE("%s is not expected to be empty", cpu_abilist_property_name);
        return -1;
    }
    snprintf(cpuAbiListBuf, sizeof(cpuAbiListBuf), "--cpu-abilist=%s", propBuf);
    addOption(cpuAbiListBuf);

    // Dalvik-cache pruning counter.
    parseRuntimeOption("dalvik.vm.zygote.max-boot-retry", cachePruneBuf,
                       "-Xzygote-max-boot-retry=");

    /*
     * When running with debug.generate-debug-info, add --generate-debug-info to
     * the compiler options so that the boot image, if it is compiled on device,
     * will include native debugging information.
     */
    property_get("debug.generate-debug-info", propBuf, "");
    if (strcmp(propBuf, "true") == 0) {
        addOption("-Xcompiler-option");
        addOption("--generate-debug-info");
        addOption("-Ximage-compiler-option");
        addOption("--generate-debug-info");
    }

    /*
     * Retrieve the build fingerprint and provide it to the runtime. That way, ANR dumps will
     * contain the fingerprint and can be parsed.
     */
    parseRuntimeOption("ro.build.fingerprint", fingerprintBuf, "-Xfingerprint:");

    initArgs.version = JNI_VERSION_1_4;
    initArgs.options = mOptions.editArray();
    initArgs.nOptions = mOptions.size();
    initArgs.ignoreUnrecognized = JNI_FALSE;


    //******************* 第11部分**********************
    /*
     * Initialize the VM.
     *
     * The JavaVM* is essentially per-process, and the JNIEnv* is per-thread.
     * If this call succeeds, the VM is ready, and we can start issuing
     * JNI calls.
     */
    if (JNI_CreateJavaVM(pJavaVM, pEnv, &initArgs) < 0) {
        ALOGE("JNI_CreateJavaVM failed\n");
        return -1;
    }

    return 0;
}
```

老规矩，先来翻译一下注释，如下：

> 启动Dalvik虚拟机 各种参数是在系统属性中被定义的。"mOptions"向量被更新 注意：在添加选项时，不要将char的缓冲区放到嵌套范围里面。当添加缓冲时使用" mOptions.add()"时，不要复制缓冲。是因为如果缓冲超出了范围，选项可能会被覆盖。建议将缓冲区放到函数的顶部，这样后面的就会被包含进去，从而避免减低错误的可能性。 如果成功返回0。

我将startVm函数的内容划分为11部分，下面我们就挨个讲解下：

-   第1部分：这部分主要是获取配置信息
-   第2部分：checkjni即检查jni，就是我们在C++调用jni函数的时候，会对参数或者什么的进行检查，要是不合法的话，直接exit！它还能检查资源是否正确释放。最后根据上面的判断来决定添加checkJNI参数。但是它也有副作用.所以 checkJni选项一般只在调试的eng版设置，而正是发布的user版则不设置该选项。所以该部分代码就是控制是否启动checkjni。我们再来看下它们的副作用
    -   2.1检查工作做比较耗时，所以会影响那个系统运行的速度
    -   2.2有些检查过于严格，比如字符串检查，一旦出错，则调用进程就会abort.
-   第3部分：根据属性，来决定虚拟机的执行模式。Android中用**\-Xint:portable**、**\-Xint:fast**、**\-Xint:jit** 来指定虚拟机的执行模式。虚拟机支持三种模式分别是**Portable**、**Fast**和**Jit**。
    -   Portable 是指虚拟机以可移植的方式来进行编译，也就是说，编译出来的虚拟机可以在任意平台上运行。
    -   Fast 就是针对当前平台对虚拟机进行编译，这样编译出来的Dalvik虚拟机可以进行特殊优化，从而使得它能更快地运行程序。
    -   Jit 不是解释执行代码，而是将代码动态编译成本地语言后再执行。 **所以一般通过dalvik.vm.execution-mode系统属性来指定虚拟机的模式**
-   第4部分：设置堆栈输出文件。虚拟机接收到SIGQUIT（Ctrl-\\或者kill -3）信号之后，会将所有线程的调用堆栈输出来，默认是输出到日志里面。在指定了**\-Xstacktracefile**选项之后，就可以将线程的调用堆栈输出到指定的文件中去。可以通过"dalvik.vm.stack-trace-file"系统属性来制定调用堆栈输出文件。
-   第5部分：添加一些常用的配置，注释已经很清楚了，这里就不说了
-   第6部分：添加虚拟机的堆大小，这里看见最大的heapsize，给16M。虚拟机用"-Xmx"来制定Java对象堆的最大值。我们也可以通过"dalvik.vm.heapsize"系统属性来指定为其它值。
-   第7部分：添加关于"JIT"的相关选项
-   第8部分：添加debug模式的zygote调试选项和一些其他选项，比如开机后没有挂载的选项等。
-   第9部分：确保有一个预加载的类加载文件。preloaded-classes文件的内容是由WritePreloadedClassFile.java生成的，在ZygoteInit类中会预加载工作将其中的classes提前加载到内存，以提高系统性能。
-   第10部分：添加其他虚拟机的选项。
-   第11部分：重点部分，通过调用JNI\_CreateJavaVM函数来创建及初始化一个虚拟机实例。

总结一下：其实在AndroidRuntime::startVm()函数中，主要就是做了两个件事

-   第1~10部分，设置虚拟机配置参数
-   第11部分，创建虚拟机实例

调用JNI\_CreateJavaVM函数创建虚拟机，源码的注释说明了创建虚拟机后每一个进程应该具有一个JavaVM指针，而每一个线程都具有一个JNIEnv指针，JNI\_CreateJavaVM实现在[JniInvocation.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Flibnativehelper%25252FJniInvocation.cpp&objectId=1199508&objectType=1&isNewArticle=undefined)

其主要逻辑就是返回之前从libart.so导出的JNI\_CreateJavaVM\_接口，即JNI\_CreateJavaVM的真正实现是由libart.so完成的。

因为libart.so的代码都位置与/art目录下， 我们来看下JNI\_CreateJavaVM\_的具体实现

##### (三)、JNI\_CreateJavaVM()函数解析

代码在

```
// JNI Invocation interface.

extern "C" jint JNI_CreateJavaVM(JavaVM** p_vm, JNIEnv** p_env, void* vm_args) {
  ATRACE_BEGIN(__FUNCTION__);
  const JavaVMInitArgs* args = static_cast<JavaVMInitArgs*>(vm_args);
  
  //***************  第一步 ***************
  if (IsBadJniVersion(args->version)) {
    LOG(ERROR) << "Bad JNI version passed to CreateJavaVM: " << args->version;
    ATRACE_END();
    return JNI_EVERSION;
  }

  //***************  第二步 ***************
  RuntimeOptions options;
  for (int i = 0; i < args->nOptions; ++i) {
    JavaVMOption* option = &args->options[i];
    options.push_back(std::make_pair(std::string(option->optionString), option->extraInfo));
  }
  bool ignore_unrecognized = args->ignoreUnrecognized;

  //***************  第三步 ***************
  if (!Runtime::Create(options, ignore_unrecognized)) {
    ATRACE_END();
    return JNI_ERR;
  }
  Runtime* runtime = Runtime::Current();


  //***************  第四步 ***************
  bool started = runtime->Start();

  //***************  第五步 ***************
  if (!started) {
    delete Thread::Current()->GetJniEnv();
    delete runtime->GetJavaVM();
    LOG(WARNING) << "CreateJavaVM failed";
    ATRACE_END();
    return JNI_ERR;
  }

  //***************  第六步 ***************
  *p_env = Thread::Current()->GetJniEnv();
  *p_vm = runtime->GetJavaVM();
  ATRACE_END();
  return JNI_OK;
}
```

为了方便大家理解，我将上面的不流程分为6个步骤，我依次讲解下：

-   第一步：通过调用IsBadJniVersion()函数来检查JNI版本号
-   第二步：将JavaVMOption对象转换为RuntimeOptions对象。这里简单说下RuntimeOptions，RuntimeOptions实际上只是一个Vector的定义代码在[runtime.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fart%25252Fruntime%25252Fruntime.h&objectId=1199508&objectType=1&isNewArticle=undefined)88行
-   第三步：根据上面传进来的RuntimeOptions调用Runtime::Create()函数来创建Runtime
-   第四步：根据启动上面创建的Runtime实例。
-   第五步：检查上面创建Runtime实例是否启动成功，如果启动失败，则删除相应的资源
-   第六步：返回获取相应的JNIEnv和JavaVM给上层

总结：

我们将JNI\_CreateJavaVM()函数总结一下，它主要了以下两个事情

-   初始化RuntimeOptions对象，保存参数
-   创建并启动Runtime。

至此 JNI\_CreateJavaVM()函数解析完毕，这里有个比较重要的类即Runtime，那我们一起来看下Runtime

### 五、Runtime

Runtime在ART中代表地一个Java的运行时环境。一个进程只有创建一个ATR虚拟机，一个ART虚拟机只能有一个Runtime。

关于Runtime定义在[runtime.cc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fart%25252Fruntime%25252Fruntime.cc&objectId=1199508&objectType=1&isNewArticle=undefined)中。

上面在**JNI\_CreateJavaVM()**函数里面调用了Runtime::Create(options, ignore\_unrecognized)函数

按我们就来看一下Runtime::Create(options, ignore\_unrecognized)函数

##### (一)、Runtime::Create(options, ignore\_unrecognized)函数解析

代码在[runtime.cc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%25253A%25252F%25252Fandroidxref.com%25252F6.0.1_r10%25252Fxref%25252Fart%25252Fruntime%25252Fruntime.cc&objectId=1199508&objectType=1&isNewArticle=undefined) 410行

```
bool Runtime::Create(const RuntimeOptions& options, bool ignore_unrecognized) {
  // TODO: acquire a static mutex on Runtime to avoid racing.
  // 单例判断
  if (Runtime::instance_ != nullptr) {
    return false;
  }

  //log 初始化
  InitLogging(nullptr);  // Calls Locks::Init() as a side effect.

  //创建Runtime实例
  instance_ = new Runtime;

  //调用Init函数来进行初始化
  if (!instance_->Init(options, ignore_unrecognized)) {
    // TODO: Currently deleting the instance will abort the runtime on destruction. Now This will
    // leak memory, instead. Fix the destructor. b/19100793.
    // delete instance_;
    instance_ = nullptr;
    return false;
  }
  return true;
}
```

这块代码很简单主要是调用Init函数来进行Runtime实例的初始化，那我们来看下init函数的具体实现。

##### (二)、Runtime::Init(const RuntimeOptions& raw\_options, bool ignore\_unrecognized)函数解析

```
bool Runtime::Init(const RuntimeOptions& raw_options, bool ignore_unrecognized) {
  ATRACE_BEGIN("Runtime::Init");
  CHECK_EQ(sysconf(_SC_PAGE_SIZE), kPageSize);

  //****************** 第一步 ****************** 
  MemMap::Init();

  //****************** 第二步 ****************** 
  using Opt = RuntimeArgumentMap;
  RuntimeArgumentMap runtime_options;
  std::unique_ptr<ParsedOptions> parsed_options(
      ParsedOptions::Create(raw_options, ignore_unrecognized, &runtime_options));
  if (parsed_options.get() == nullptr) {
    LOG(ERROR) << "Failed to parse options";
    ATRACE_END();
    return false;
  }
  VLOG(startup) << "Runtime::Init -verbose:startup enabled";

  //****************** 第三步 ****************** 
  QuasiAtomic::Startup();

  //****************** 第四步 ****************** 
  Monitor::Init(runtime_options.GetOrDefault(Opt::LockProfThreshold),
                runtime_options.GetOrDefault(Opt::HookIsSensitiveThread));


  //****************** 第五步 ****************** 
  boot_class_path_string_ = runtime_options.ReleaseOrDefault(Opt::BootClassPath);
  class_path_string_ = runtime_options.ReleaseOrDefault(Opt::ClassPath);
  properties_ = runtime_options.ReleaseOrDefault(Opt::PropertiesList);

  compiler_callbacks_ = runtime_options.GetOrDefault(Opt::CompilerCallbacksPtr);
  patchoat_executable_ = runtime_options.ReleaseOrDefault(Opt::PatchOat);
  must_relocate_ = runtime_options.GetOrDefault(Opt::Relocate);
  is_zygote_ = runtime_options.Exists(Opt::Zygote);
  is_explicit_gc_disabled_ = runtime_options.Exists(Opt::DisableExplicitGC);
  dex2oat_enabled_ = runtime_options.GetOrDefault(Opt::Dex2Oat);
  image_dex2oat_enabled_ = runtime_options.GetOrDefault(Opt::ImageDex2Oat);

  vfprintf_ = runtime_options.GetOrDefault(Opt::HookVfprintf);
  exit_ = runtime_options.GetOrDefault(Opt::HookExit);
  abort_ = runtime_options.GetOrDefault(Opt::HookAbort);

  default_stack_size_ = runtime_options.GetOrDefault(Opt::StackSize);
  stack_trace_file_ = runtime_options.ReleaseOrDefault(Opt::StackTraceFile);

  compiler_executable_ = runtime_options.ReleaseOrDefault(Opt::Compiler);
  compiler_options_ = runtime_options.ReleaseOrDefault(Opt::CompilerOptions);
  image_compiler_options_ = runtime_options.ReleaseOrDefault(Opt::ImageCompilerOptions);
  image_location_ = runtime_options.GetOrDefault(Opt::Image);

  max_spins_before_thin_lock_inflation_ =
      runtime_options.GetOrDefault(Opt::MaxSpinsBeforeThinLockInflation);

  monitor_list_ = new MonitorList;
  monitor_pool_ = MonitorPool::Create();
  thread_list_ = new ThreadList;
  intern_table_ = new InternTable;

  verify_ = runtime_options.GetOrDefault(Opt::Verify);
  allow_dex_file_fallback_ = !runtime_options.Exists(Opt::NoDexFileFallback);

  Split(runtime_options.GetOrDefault(Opt::CpuAbiList), ',', &cpu_abilist_);

  fingerprint_ = runtime_options.ReleaseOrDefault(Opt::Fingerprint);

  if (runtime_options.GetOrDefault(Opt::Interpret)) {
    GetInstrumentation()->ForceInterpretOnly();
  }

  zygote_max_failed_boots_ = runtime_options.GetOrDefault(Opt::ZygoteMaxFailedBoots);

  XGcOption xgc_option = runtime_options.GetOrDefault(Opt::GcOption);
  ATRACE_BEGIN("CreateHeap");

  //****************** 第六步 ****************** 
  heap_ = new gc::Heap(runtime_options.GetOrDefault(Opt::MemoryInitialSize),
                       runtime_options.GetOrDefault(Opt::HeapGrowthLimit),
                       runtime_options.GetOrDefault(Opt::HeapMinFree),
                       runtime_options.GetOrDefault(Opt::HeapMaxFree),
                       

runtime_options.GetOrDefault(Opt::HeapTargetUtilization),
                       runtime_options.GetOrDefault(Opt::ForegroundHeapGrowthMultiplier),
                       runtime_options.GetOrDefault(Opt::MemoryMaximumSize),
                       runtime_options.GetOrDefault(Opt::NonMovingSpaceCapacity),
                       runtime_options.GetOrDefault(Opt::Image),
                       runtime_options.GetOrDefault(Opt::ImageInstructionSet),
                       xgc_option.collector_type_,
                       runtime_options.GetOrDefault(Opt::BackgroundGc),
                       runtime_options.GetOrDefault(Opt::LargeObjectSpace),
                       runtime_options.GetOrDefault(Opt::LargeObjectThreshold),
                       runtime_options.GetOrDefault(Opt::ParallelGCThreads),
                       runtime_options.GetOrDefault(Opt::ConcGCThreads),
                       runtime_options.Exists(Opt::LowMemoryMode),
                       runtime_options.GetOrDefault(Opt::LongPauseLogThreshold),
                       runtime_options.GetOrDefault(Opt::LongGCLogThreshold),
                       runtime_options.Exists(Opt::IgnoreMaxFootprint),
                       runtime_options.GetOrDefault(Opt::UseTLAB),
                       xgc_option.verify_pre_gc_heap_,
                       xgc_option.verify_pre_sweeping_heap_,
                       xgc_option.verify_post_gc_heap_,
                       xgc_option.verify_pre_gc_rosalloc_,
                       xgc_option.verify_pre_sweeping_rosalloc_,
                       xgc_option.verify_post_gc_rosalloc_,
                       xgc_option.gcstress_,
                       runtime_options.GetOrDefault(Opt::EnableHSpaceCompactForOOM),
                       runtime_options.GetOrDefault(Opt::HSpaceCompactForOOMMinIntervalsMs));
  ATRACE_END();

  if (heap_->GetImageSpace() == nullptr && !allow_dex_file_fallback_) {
    LOG(ERROR) << "Dex file fallback disabled, cannot continue without image.";
    ATRACE_END();
    return false;
  }

 //****************** 第七步 ****************** 
  dump_gc_performance_on_shutdown_ = runtime_options.Exists(Opt::DumpGCPerformanceOnShutdown);

  if (runtime_options.Exists(Opt::JdwpOptions)) {
    Dbg::ConfigureJdwp(runtime_options.GetOrDefault(Opt::JdwpOptions));
  }

  jit_options_.reset(jit::JitOptions::CreateFromRuntimeArguments(runtime_options));
  if (IsAotCompiler()) {
    // If we are already the compiler at this point, we must be dex2oat. Don't create the jit in
    // this case.
    // If runtime_options doesn't have UseJIT set to true then CreateFromRuntimeArguments returns
    // null and we don't create the jit.
    jit_options_->SetUseJIT(false);
  }

  // Use MemMap arena pool for jit, malloc otherwise. Malloc arenas are faster to allocate but
  // can't be trimmed as easily.
  const bool use_malloc = IsAotCompiler();
  arena_pool_.reset(new ArenaPool(use_malloc, false));
  if (IsCompiler() && Is64BitInstructionSet(kRuntimeISA)) {
    // 4gb, no malloc. Explanation in header.
    low_4gb_arena_pool_.reset(new ArenaPool(false, true));
    linear_alloc_.reset(new LinearAlloc(low_4gb_arena_pool_.get()));
  } else {
    linear_alloc_.reset(new LinearAlloc(arena_pool_.get()));
  }


 //****************** 第八步 ****************** 
  BlockSignals();
  InitPlatformSignalHandlers();


 //****************** 第九步 ****************** 
  // Change the implicit checks flags based on runtime architecture.
  switch (kRuntimeISA) {
    case kArm:
    case kThumb2:
    case kX86:
    case kArm64:
    case kX86_64:
    case kMips:
    case kMips64:
      implicit_null_checks_ = true;
      // Installing stack protection does not play well with valgrind.
      implicit_so_checks_ = (RUNNING_ON_VALGRIND == 0);
      break;
    default:
      // Keep the defaults.
      break;
  }


 //****************** 第九步 ****************** 
  // Always initialize the signal chain so that any calls to sigaction get
  // correctly routed to the next in the chain regardless of whether we
  // have claimed the signal or not.
  InitializeSignalChain();


 //****************** 第十步 ****************** 
  if (implicit_null_checks_ || implicit_so_checks_ || implicit_suspend_checks_) {
    fault_manager.Init();

    // These need to be in a specific order.  The null point check handler must be
    // after the suspend check and stack overflow check handlers.
    //
    // Note: the instances attach themselves to the fault manager and are handled by it. The manager
    //       will delete the instance on Shutdown().
    if (implicit_suspend_checks_) {
      new SuspensionHandler(&fault_manager);
    }

    if (implicit_so_checks_) {
      new StackOverflowHandler(&fault_manager);
    }

    if (implicit_null_checks_) {
      new NullPointerHandler(&fault_manager);
    }

    if (kEnableJavaStackTraceHandler) {
      new JavaStackTraceHandler(&fault_manager);
    }
  }


 //****************** 第十一步 ****************** 
  java_vm_ = new JavaVMExt(this, runtime_options);


 //****************** 第十二步 ****************** 
  Thread::Startup();



  // ClassLinker needs an attached thread, but we can't fully attach a thread without creating
  // objects. We can't supply a thread group yet; it will be fixed later. Since we are the main
  // thread, we do not get a java peer.
  Thread* self = Thread::Attach("main", false, nullptr, false);
  CHECK_EQ(self->GetThreadId(), ThreadList::kMainThreadId);
  CHECK(self != nullptr);

  // Set us to runnable so tools using a runtime can allocate and GC by default
  self->TransitionFromSuspendedToRunnable();

 //****************** 第十三步 ****************
  // Now we're attached, we can take the heap locks and validate the heap.
  GetHeap()->EnableObjectValidation();

  CHECK_GE(GetHeap()->GetContinuousSpaces().size(), 1U);

 //****************** 第十四步 ****************
  class_linker_ = new ClassLinker(intern_table_);
  if (GetHeap()->HasImageSpace()) {
    ATRACE_BEGIN("InitFromImage");
    class_linker_->InitFromImage();
    ATRACE_END();
    if (kIsDebugBuild) {
      GetHeap()->GetImageSpace()->VerifyImageAllocations();
    }
    if (boot_class_path_string_.empty()) {
      // The bootclasspath is not explicitly specified: construct it from the loaded dex files.
      const std::vector<const DexFile*>& boot_class_path = GetClassLinker()->GetBootClassPath();
      std::vector<std::string> dex_locations;
      dex_locations.reserve(boot_class_path.size());
      for (const DexFile* dex_file : boot_class_path) {
        dex_locations.push_back(dex_file->GetLocation());
      }
      boot_class_path_string_ = Join(dex_locations, ':');
    }
  } else {
    std::vector<std::string> dex_filenames;
    Split(boot_class_path_string_, ':', &dex_filenames);

    std::vector<std::string> dex_locations;
    if (!runtime_options.Exists(Opt::BootClassPathLocations)) {
      dex_locations = dex_filenames;
    } else {
      dex_locations = runtime_options.GetOrDefault(Opt::BootClassPathLocations);
      CHECK_EQ(dex_filenames.size(), dex_locations.size());
    }

    std::vector<std::unique_ptr<const DexFile>> boot_class_path;
    OpenDexFiles(dex_filenames,
                 dex_locations,
                 runtime_options.GetOrDefault(Opt::Image),
                 &boot_class_path);
    instruction_set_ = runtime_options.GetOrDefault(Opt::ImageInstructionSet);
    class_linker_->InitWithoutImage(std::move(boot_class_path));

    // TODO: Should we move the following to InitWithoutImage?
    SetInstructionSet(instruction_set_);
    for (int i = 0; i < Runtime::kLastCalleeSaveType; i++) {
      Runtime::CalleeSaveType type = Runtime::CalleeSaveType(i);
      if (!HasCalleeSaveMethod(type)) {
        SetCalleeSaveMethod(CreateCalleeSaveMethod(), type);
      }
    }
  }

  CHECK(class_linker_ != nullptr);

  //****************** 第十五步 ****************
  // Initialize the special sentinel_ value early.
  sentinel_ = GcRoot<mirror::Object>(class_linker_->AllocObject(self));
  CHECK(sentinel_.Read() != nullptr);

  verifier::MethodVerifier::Init();

 //****************** 第十六步 ****************
  if (runtime_options.Exists(Opt::MethodTrace)) {
    trace_config_.reset(new TraceConfig());
    trace_config_->trace_file = runtime_options.ReleaseOrDefault(Opt::MethodTraceFile);
    trace_config_->trace_file_size = runtime_options.ReleaseOrDefault(Opt::MethodTraceFileSize);
    trace_config_->trace_mode = Trace::TraceMode::kMethodTracing;
    trace_config_->trace_output_mode = runtime_options.Exists(Opt::MethodTraceStreaming) ?
        Trace::TraceOutputMode::kStreaming :
        Trace::TraceOutputMode::kFile;
  }


 //****************** 第十七步 ****************
  {
    auto&& profiler_options = runtime_options.ReleaseOrDefault(Opt::ProfilerOpts);
    profile_output_filename_ = profiler_options.output_file_name_;

    // TODO: Don't do this, just change ProfilerOptions to include the output file name?
    ProfilerOptions other_options(
        profiler_options.enabled_,
        profiler_options.period_s_,
        profiler_options.duration_s_,
        profiler_options.interval_us_,
        profiler_options.backoff_coefficient_,
        profiler_options.start_immediately_,
        profiler_options.top_k_threshold_,
        profiler_options.top_k_change_threshold_,
        profiler_options.profile_type_,
        profiler_options.max_stack_depth_);

    profiler_options_ = other_options;
  }

  // TODO: move this to just be an Trace::Start argument
  Trace::SetDefaultClockSource(runtime_options.GetOrDefault(Opt::ProfileClock));


 //****************** 第十八步 ****************
  // Pre-allocate an OutOfMemoryError for the double-OOME case.
  self->ThrowNewException("Ljava/lang/OutOfMemoryError;",
                          "OutOfMemoryError thrown while trying to throw OutOfMemoryError; "
                          "no stack trace available");
  pre_allocated_OutOfMemoryError_ = GcRoot<mirror::Throwable>(self->GetException());
  self->ClearException();

  // Pre-allocate a NoClassDefFoundError for the common case of failing to find a system class
  // ahead of checking the application's class loader.
  self->ThrowNewException("Ljava/lang/NoClassDefFoundError;",
                          "Class not found using the boot class loader; no stack trace available");
  pre_allocated_NoClassDefFoundError_ = GcRoot<mirror::Throwable>(self->GetException());
  self->ClearException();

  // Look for a native bridge.
  //
  // The intended flow here is, in the case of a running system:
  //
  // Runtime::Init() (zygote):
  //   LoadNativeBridge -> dlopen from cmd line parameter.
  //  |
  //  V
  // Runtime::Start() (zygote):
  //   No-op wrt native bridge.
  //  |
  //  | start app
  //  V
  // DidForkFromZygote(action)
  //   action = kUnload -> dlclose native bridge.
  //   action = kInitialize -> initialize library
  //
  //
  // The intended flow here is, in the case of a simple dalvikvm call:
  //
  // Runtime::Init():
  //   LoadNativeBridge -> dlopen from cmd line parameter.
  //  |
  //  V
  // Runtime::Start():
  //   DidForkFromZygote(kInitialize) -> try to initialize any native bridge given.
  //   No-op wrt native bridge.


 //****************** 第十九步 ****************
  {
    std::string native_bridge_file_name = runtime_options.ReleaseOrDefault(Opt::NativeBridge);
    is_native_bridge_loaded_ = LoadNativeBridge(native_bridge_file_name);
  }

  VLOG(startup) << "Runtime::Init exiting";

  ATRACE_END();

  return true;
}
```

-   第一步：初始化内存映射模块
-   第二步：调用ParsedOptions的静态函数Create(raw\_options, ignore\_unrecognized, &runtime\_options)来解析ART运行时的启动选项，并且保存变量options指向一个ParsedOptions对象的各个成员变量中
-   第三步：原子属性库的初始化
-   第四步：Monitor初始化
-   第五步：设置runtime\_options的一些默认选项
-   第六步：创建heap对象。
-   第七步：配置一些选项
    -   获取dump\_gc\_performance\_on\_shutdown\_ 并配置，**XX:DumpGCPerformanceOnShutdown**用于传递给dalvikvm，获得应用的GC性能时序
    -   配置[JDWP](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%25253A%25252F%25252Fwww.ibm.com%25252Fdeveloperworks%25252Fcn%25252Fjava%25252Fj-lo-jpda3%25252F&objectId=1199508&objectType=1&isNewArticle=undefined)，即Java Debug Wire Protocol 的缩写，它定义了调试器(debugger)和被调试的Java 虚拟机(target vm)之间的通信协议。
    -   配置JIT选项，用来提升代码执行效率。如果是5.0之后的，不需要开启该选项，因为ART是需要AOT的，但是也兼容dalvik的dex解释器，所以JIT也是有的
    -   创建线性分配器 linear\_alloc\_
-   第八步：获取ART的架构
-   第九步：初始化信号链
-   第十步：根据ART的架构，进行对应信号的处理，所以所有信号都要交给fault\_manager来处理。
-   第十一步：创建JavaVMExt，这个JavaVMExt实例最终是要返回给调用的，使得调用者可以通过该JavaVMExt实例和ART虚拟机交互
-   第十二步：创建一个线程，并且attach线程(attach的过程实际就是创建Thread对象并初始化Thread对象的过程)
-   第十三步：attach后通过调用EnableObjectValidation函数来验证heap
-   第十四步：创建ClassLinker实例，这是一个非常重要的实例，类的加载、链接和初始化都是在这个类中完成的。如果有BootImageSpace，则调用ClassLinker::InitFromBootImage来完成ClassLinker的初始化，如果没有BootImageSpace，则调用ClassLinker::InitWithoutImage来完成初始化。前者通过ImageSpace来加载系统类；后者是通过boot\_class\_path，boot\_class\_path是系统类DexFile数组，ImageSpace的优点是加载快，通过mmap加载一个系统类的镜像文件。
-   第十五步：初始化sentinel\_的值
-   第十六步：如果有MethodTrace选项，则进行相应的配置
-   第十七步：如果有Profiler选项，则进行相应的配置
-   第十八步：提前分配一个OutOfMemoryError和NoClassDefFoundError
-   第十九步：配置NativeBridge中间模块，从Android 5.0，开始在其ART的实现中，引入了一个叫做NativeBridge的中间模块，这个模块基本上就是为了JNI调用时进行动态转码用的，自带了基本上所有的处理逻辑。

至此，Runtime::Init函数分析完毕，其中上面的每一步都可以单独列为一篇文章来进一步研究，不过毕竟不是专门研究ART，所以到此为止，有兴趣的可以自己研究。

### 官人飞吻，你都把臣妾从头看到尾了，喜欢就点个赞呗(眉眼)！！！！