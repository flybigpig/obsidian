![](https://csdnimg.cn/release/blogv2/dist/pc/img/original.png)

[m0\_hehua](https://blog.csdn.net/m0_46586467 "m0_hehua") ![](https://csdnimg.cn/release/blogv2/dist/pc/img/newUpTime2.png) 已于 2023-01-13 17:05:57 修改

于 2023-01-13 16:57:32 首次发布

版权声明：本文为博主原创文章，遵循 [CC 4.0 BY-SA](http://creativecommons.org/licenses/by-sa/4.0/) 版权协议，转载请附上原文出处链接和本声明。

> 提示：文章写完后，目录可以自动生成，如何生成可参考右边的帮助文档

___

## 前言

1.init简介(本文基于android12)  
init进程是Android系统中用户空间的第一个进程，作为第一个进程，它被赋予了很多极其重要的工作职责，比如创建zygote(孵化器)和属性服务等。init进程是由多个源文件共同组成的，这些文件位于源码目录system/core/init。本文将基于Android12源码来分析Init进程。  
2.引入init进程  
说到init进程，首先要提到Android系统启动流程的前几步：  
1.启动电源以及系统启动  
当电源按下时引导芯片代码开始从预定义的地方（固化在ROM）开始执行。加载引导程序Bootloader到RAM，然后执行。  
2.引导程序Bootloader  
引导程序是在Android操作系统开始运行前的一个小程序，它的主要作用是把系统OS拉起来并运行。  
3.linux内核启动  
内核启动时，设置缓存、被保护存储器、计划列表，加载驱动。当内核完成系统设置，它首先在系统文件中寻找”init”文件，然后启动root进程或者系统的第一个进程。  
4.init进程启动

___

## 一、1.解析init进程启动过程

init进程启动过程  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/674df6bf26a3fe6c541cabc705944ec5.png)

```
android12流程和之前的有所区别首先进入main.cpp 只选取主要的过程
/system/core/init/main.cpp
51  int main(int argc, char** argv) {
61      if (argc > 1) {
...
73          if (!strcmp(argv[1], "second_stage")) {
74              return SecondStageMain(argc, argv);2
75          }
76      }
78      return FirstStageMain(argc, argv);1
79  }

/system/core/init/first_stage_init.cpp FirstStageMain
//创建文件并挂载
202      CHECKCALL(mkdir("/dev/pts", 0755));
203      CHECKCALL(mkdir("/dev/socket", 0755));
204      CHECKCALL(mkdir("/dev/dm-user", 0755));

/system/core/init/init.cpp SecondStageMain
787      PropertyInit();1 初始化属性相关资源
805      Epoll epoll;
806      if (auto result = epoll.Open(); !result.ok()) {
807          PLOG(FATAL) << result.error();
808      }
810      InstallSignalFdHandler(&epoll);2 通过epoll监听signal 
811      InstallInitNotifier(&epoll);
812      StartPropertyService(&property_fd);3 启动属性服务
840      LoadBootScripts(am, sm);4 解析init.rc
854      am.QueueBuiltinAction(TestPerfEventSelinuxAction, "TestPerfEventSelinux");
855      am.QueueEventTrigger("early-init");5 解析完的服务等等加入队列
888      while (true) {
900          if (!(prop_waiter_state.MightBeWaiting() || Service::is_exec_service_running())) {
901              am.ExecuteOneCommand();6 执行解析完的命令

6最后回调到/system/core/init/action.cpp的ExecuteCommand()然后result = command.InvokeFunc(subcontext_);调用对应的func函数，下面进入LoadBootScripts看下是如何解析init.rc文件的

首先先了解下init.rc的文件格式
/system/core/rootdir/init.rc
15 on early-init
119 on init
468 on late-init
496     trigger zygote-start
948 on zygote-start && property:ro.crypto.state=unencrypted
949     wait_for_prop odsign.verification.done 1
950     # A/B update verifier that marks a successful boot.
951     exec_start update_verifier_nonencrypted
952     start statsd
953     start netd
954     start zygote
955     start zygote_secondary
1086 on nonencrypted
1087     class_start main//这里的main指zygote
1088     class_start late_start

/system/core/init/init.cpp
303  static void LoadBootScripts(ActionManager& action_manager, ServiceList& service_list) {
304      Parser parser = CreateParser(action_manager, service_list);
306      std::string bootscript = GetProperty("ro.boot.init_rc", "");
307      if (bootscript.empty()) {
308          parser.ParseConfig("/system/etc/init/hw/init.rc");
309          if (!parser.ParseConfig("/system/etc/init")) {
310              late_import_paths.emplace_back("/system/etc/init");
311          }
...
324      } else {
325          parser.ParseConfig(bootscript);
326      }
327  }

/system/core/init/parser.cpp
184  bool Parser::ParseConfig(const std::string& path) {
185      if (is_dir(path.c_str())) {
186          return ParseConfigDir(path);
187      }
188      return ParseConfigFile(path);
189  }

43  void Parser::ParseData(const std::string& filename, std::string* data) {
73      for (;;) {
74          switch (next_token(&state)) {
75              case T_EOF:
83              case T_NEWLINE: {
...         ParseSection() //这个方法会走不同的cpp的ParseSection方法根据之前传入的类型
                        EndSection();
124              }
125              case T_TEXT:
126                  args.emplace_back(state.text);
127                  break;
128          }
129      }
130  }

就不详细展开了有兴趣可以自己看下源码，如果学过编译原理看起来不难下面拿service 的parse举例
628  Result<void> ServiceParser::EndSection() {
672      service_list_->AddService(std::move(service_)); //解析完加入队列中
674      return {};
675  }
```

## 二、2.解析service服务

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/4f8a4b35ce1bfd1368ae8fbde283362c.png)

```
前面说了会调用do_class_start方法
/system/core/init/builtins.cpp
161  static Result<void> do_class_start(const BuiltinArguments& args) {
167      for (const auto& service : ServiceList::GetInstance()) {
168          if (service->classnames().count(args[1])) {
169              if (auto result = service->StartIfNotDisabled(); !result.ok()) {
170                  LOG(ERROR) << "Could not start service '" << service->name()
171                             << "' as part of class '" << args[1] << "': " << result.error();
172              }
173          }
174      }
175      return {};
176  }

/system/core/init/service.cpp
625  Result<void> Service::StartIfNotDisabled() {
626      if (!(flags_ & SVC_DISABLED)) {
627          return Start();
628      } 
631      return {};
632  }

/system/core/init/service.cpp  Start()
506      pid_t pid = -1;
507      if (namespaces_.flags) {
508          pid = clone(nullptr, nullptr, namespaces_.flags | SIGCHLD, nullptr);
509      } else {
510          pid = fork();//创建一个新的进程，fork
511      }
513      if (pid == 0) {//子线程
514          umask(077);//设置权限
542          if (!ExpandArgsAndExecv(args_, sigstop_)) {
543              PLOG(ERROR) << "cannot execv('" << args_[0]
544                          << "'). See the 'Debugging init' section of init's README.md for tips";
545          }
547          _exit(127);
548      }

105  static bool ExpandArgsAndExecv(const std::vector<std::string>& args, bool sigstop) {
125      return execv(c_strings[0], c_strings.data()) == 0;//exec 函数值去执行那个可执行文件
126  }
```

fork 创建一个新的进程，系统调用，fork 了之后子进程会继承父进程的资源，exec 是不会的，fork读时共享写时复制,简单来说fork出来的子进程读时共享父进程的资源，但是写入时会复制一份新的不会影响父进程的资源。  
execv zygote的func会进入到/frameworks/base/cmds/app\_process/app\_main.cpp  
main函数这个我们下次再说。

## 总结

总结起来init进程主要做了几件事：  
1.创建一些文件夹并挂载设备  
2.初始化和启动属性服务  
3.解析init.rc配置文件  
4.启动相关服务

有时候看源码是很痛苦的事情，我感觉这是在锻炼我的内心，心够不够静是不是很烦躁。因为很多东西你是看不懂的。就像我几年看的数据结构与算法当时根本不知道他在说什么，看的时候很容易分心，也不知道学它有什么意义，就是单纯的自己感兴趣，后来看了多次也敲了代码，很多东西也都理解了。  
我的意思就是第一遍看不懂没关系，多看几遍，积累[linux内核](https://so.csdn.net/so/search?q=linux%E5%86%85%E6%A0%B8&spm=1001.2101.3001.7020) c++ 等等相关知识过一段时间就会觉得不难，修心提升自己。我们不要抱怨环境怎么样，努力提升自己，对人对事也都是如此。


---

加载并解析init.rc 根据rc文件创建Service对象，最终将service添加到service_List中，然后解析到on 执行 class_start的时候会开启服务调用Service::Start函数,fork子进程，并且执行app_process文件 开启了zygote，也就到了zygote的main函数中了
