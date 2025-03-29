> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

```
int SecondStageMain(int argc, char** argv) {
    ...省略代码 

    am.QueueBuiltinAction(SetupCgroupsAction, "SetupCgroups");

    am.QueueEventTrigger("early-init");

    // Queue an action that waits for coldboot done so we know ueventd has set up all of /dev...
    am.QueueBuiltinAction(wait_for_coldboot_done_action, "wait_for_coldboot_done");
    // ... so that we can start queuing up actions that require stuff from /dev.
    am.QueueBuiltinAction(MixHwrngIntoLinuxRngAction, "MixHwrngIntoLinuxRng");
    am.QueueBuiltinAction(SetMmapRndBitsAction, "SetMmapRndBits");
    am.QueueBuiltinAction(SetKptrRestrictAction, "SetKptrRestrict");
    Keychords keychords;
    am.QueueBuiltinAction(
        [&epoll, &keychords](const BuiltinArguments& args) -> Result<Success> {
            for (const auto& svc : ServiceList::GetInstance()) {
                keychords.Register(svc->keycodes());
            }
            keychords.Start(&epoll, HandleKeychord);
            return Success();
        },
        "KeychordInit");
    am.QueueBuiltinAction(console_init_action, "console_init");

    // Trigger all the boot actions to get us started.
    am.QueueEventTrigger("init");

    // Starting the BoringSSL self test, for NIAP certification compliance.
    am.QueueBuiltinAction(StartBoringSslSelfTest, "StartBoringSslSelfTest");

    // Repeat mix_hwrng_into_linux_rng in case /dev/hw_random or /dev/random
    // wasn't ready immediately after wait_for_coldboot_done
    am.QueueBuiltinAction(MixHwrngIntoLinuxRngAction, "MixHwrngIntoLinuxRng");

    // Initialize binder before bringing up other system services
    am.QueueBuiltinAction(InitBinder, "InitBinder");

    // Don't mount filesystems or start core system services in charger mode.
    std::string bootmode = GetProperty("ro.bootmode", "");
    if (bootmode == "charger") {
        am.QueueEventTrigger("charger");
    } else {
        am.QueueEventTrigger("late-init");
    }

    // Run all property triggers based on current state of the properties.
    am.QueueBuiltinAction(queue_property_triggers_action, "queue_property_triggers");

    ...省略代码 
} 
```

以上代码对am进行了 2 个操作

-   QueueBuiltinAction向am队列中添加一个Action
    -   Action会被添加到actions\_管理
    -   Action的事件添加到event\_queue\_队列
-   QueueEventTrigger向event\_queue\_队列中添加trigger 代码中QueueEventTrigger调用的trigger顺序如下：

1.  early-init
2.  init
3.  late-init

event\_queue\_对象的定义需要我们注意一下

```
std::queue<std::variant<EventTrigger, PropertyChange, BuiltinAction>> event_queue_;
```

它是一个队列，但支持 3 中类型，QueueEventTrigger最后添加的EventTrigger，这个我们后面会看到其妙用。

这个trigger实际上就是rc文件中Action中定义的trigger

```
on <trigger> [&& <trigger>]*
    <command>
    <command>
    <command>
```

但QueueBuiltinAction调用只是将要执行的Action顺序添加到队列中，Action还并未开始执行，在如下循环中才开始执行Action，进而启动目标服务。

## 服务启动

这里首先明确一下我们的分析目标：init通过rc脚本启动的是zygote，因此我们围绕这一目标进行分析。

首先我们看看在rc脚本中zygote服务是如何被启动的，在init.rc的late-init中触发zygote-start

```
//system/core/rootdir/init.rc
on late-init
    # Now we can start zygote for devices with file based encryption
    trigger zygote-start
```

zygote-start定义了多种条件，但是它里面都调用了start zygote去启动zygote服务

```
# It is recommended to put unnecessary data/ initialization from post-fs-data
# to start-zygote in device's init.rc to unblock zygote start.
on zygote-start && property:ro.crypto.state=unencrypted
    # A/B update verifier that marks a successful boot.
    exec_start update_verifier_nonencrypted
    start netd
    start zygote
    start zygote_secondary

on zygote-start && property:ro.crypto.state=unsupported
    # A/B update verifier that marks a successful boot.
    exec_start update_verifier_nonencrypted
    start netd
    start zygote
    start zygote_secondary

on zygote-start && property:ro.crypto.state=encrypted && property:ro.crypto.type=file
    # A/B update verifier that marks a successful boot.
    exec_start update_verifier_nonencrypted
    start netd
    start zygote
    start zygote_secondary
```

zygote服务完成定义如下

```
//system/core/rootdir/init.zygote64.rc

service zygote /system/bin/app_process64 -Xzygote /system/bin --zygote --start-system-server
    class main
    priority -20
    user root
    group root readproc reserved_disk
    socket zygote stream 660 root system
    socket usap_pool_primary stream 660 root system
    onrestart write /sys/android_power/request_state wake
    onrestart write /sys/power/state on
    onrestart restart audioserver
    onrestart restart cameraserver
    onrestart restart media
    onrestart restart netd
    onrestart restart wificond
    writepid /dev/cpuset/foreground/tasks
```

因此我们需要分析代码中是如何触发late-init，最后是如何启动zygote服务的。

```
int SecondStageMain(int argc, char** argv) {
    ...省略代码
    
    while (true) {
        if (!(waiting_for_prop || Service::is_exec_service_running())) {
            am.ExecuteOneCommand();
        }
        
        if (!(waiting_for_prop || Service::is_exec_service_running())) {
            ...省略代码
            
            // If there's more work to do, wake up again immediately.
            if (am.HasMoreCommands()) epoll_timeout = 0ms;
        }
    }
    
    ...省略代码 
} 
```

上面循环中调用ExecuteOneCommand开始执行Action

```
void ActionManager::ExecuteOneCommand() {
    // Loop through the event queue until we have an action to execute
    while (current_executing_actions_.empty() && !event_queue_.empty()) {
        for (const auto& action : actions_) {
            if (std::visit([&action](const auto& event) { return action->CheckEvent(event); },
                           event_queue_.front())) {
                current_executing_actions_.emplace(action.get());
            }
        }
        event_queue_.pop();
    }

    if (current_executing_actions_.empty()) {
        return;
    }

    auto action = current_executing_actions_.front();

    if (current_command_ == 0) {
        std::string trigger_name = action->BuildTriggersString();
        LOG(INFO) << "processing action (" << trigger_name << ") from (" << action->filename()
                  << ":" << action->line() << ")";
    }

    action->ExecuteOneCommand(current_command_);

    // If this was the last command in the current action, then remove
    // the action from the executing list.
    // If this action was oneshot, then also remove it from actions_.
    ++current_command_;
    if (current_command_ == action->NumCommands()) {
        current_executing_actions_.pop();
        current_command_ = 0;
        if (action->oneshot()) {
            auto eraser = [&action](std::unique_ptr<Action>& a) { return a.get() == action; };
            actions_.erase(std::remove_if(actions_.begin(), actions_.end(), eraser));
        }
    }
}
```

-   当前没有正在执行的Action且event\_queue\_队列不为空，取出event\_queue\_.front，在我们当前分析的地方这个front返回的类型为EventTrigger（后面还有返回PropertyChange的场景，因此需要注意）
-   遍历actions\_，然后根据EventTrigger查找对应的Action，然后添加到current\_executing\_actions\_队列中

```
bool Action::CheckEvent(const EventTrigger& event_trigger) const {
    return event_trigger == event_trigger_ && CheckPropertyTriggers();
}
```

-   然后执行此Action的ExecuteOneCommand

```
//调用
action->ExecuteOneCommand(current_command_);

//实现
void Action::ExecuteOneCommand(std::size_t command) const {
    // We need a copy here since some Command execution may result in
    // changing commands_ vector by importing .rc files through parser
    Command cmd = commands_[command];
    ExecuteCommand(cmd);
}
```

-   current\_command\_：记录执行的是当前Action下面的第几个command
-   commands\_\[command\]：取出当前对应的command，按照前面的分析这个Action是late-init，command对应是

```
trigger zygote-start
```

ExecuteCommand调用command.InvokeFunc

```
void Action::ExecuteCommand(const Command& command) const {
    ...

    auto result = command.InvokeFunc(subcontext_);

    ...
}

Result<Success> Command::InvokeFunc(Subcontext* subcontext) const {
    if (subcontext) {
        if (execute_in_subcontext_) {
            return subcontext->Execute(args_);
        }

        auto expanded_args = subcontext->ExpandArgs(args_);
        if (!expanded_args) {
            return expanded_args.error();
        }
        return RunBuiltinFunction(func_, *expanded_args, subcontext->context());
    }

    return RunBuiltinFunction(func_, args_, kInitContext);
}
```

-   Commond中的func\_前面已经讲过了，这里它对应trigger命令的函数实现do\_trigger
-   RunBuiltinFunction里面其实就是执行调用do\_trigger函数

```
static Result<Success> do_trigger(const BuiltinArguments& args) {
    ActionManager::GetInstance().QueueEventTrigger(args[1]);
    return Success();
}
```

do\_trigger调用QueueEventTrigger向队列中添加了事件zygote-start，最后代码在循环中会触发zygote-start事件

```
int SecondStageMain(int argc, char** argv) {
    ...省略代码
    
    while (true) {
        if (!(waiting_for_prop || Service::is_exec_service_running())) {
            am.ExecuteOneCommand();
        }
        
        if (!(waiting_for_prop || Service::is_exec_service_running())) {
            ...省略代码
            
            // If there's more work to do, wake up again immediately.
            if (am.HasMoreCommands()) epoll_timeout = 0ms;
        }
    }
    
    ...省略代码 
} 
```

整个过程和触发late-init是一样的，不同的是zygote-start中会启动zygote服务

```
on zygote-start
    start zygote
```

根据前面的分析start对应的实现函数是do\_start

```
//system/core/init/builtins.cpp

static Result<Success> do_start(const BuiltinArguments& args) {
    Service* svc = ServiceList::GetInstance().FindService(args[1]);
    if (!svc) return Error() << "service " << args[1] << " not found";
    if (auto result = svc->Start(); !result) {
        return Error() << "Could not start service: " << result.error();
    }
    return Success();
}
```

-   ServiceList中保存的是从rc解析出来的所有服务列表
-   args\[1\]为zygote
-   FindService(args\[1\])返回zygote对应的Service对象
-   调用Service对象的Start方法启动服务

```
Result<Success> Service::Start() {
    ...省略代码

    LOG(INFO) << "starting service '" << name_ << "'...";

    pid_t pid = -1;
    if (namespace_flags_) {
        pid = clone(nullptr, nullptr, namespace_flags_ | SIGCHLD, nullptr);
    } else {
        pid = fork();
    }

    if (pid == 0) {//子进程执行
        umask(077);

        if (auto result = EnterNamespaces(); !result) {
            LOG(FATAL) << "Service '" << name_ << "' could not enter namespaces: " << result.error();
        }

#if defined(__ANDROID__)
        if (pre_apexd_) {
            if (!SwitchToBootstrapMountNamespaceIfNeeded()) {
                LOG(FATAL) << "Service '" << name_ << "' could not enter "
                           << "into the bootstrap mount namespace";
            }
        }
#endif

        if (namespace_flags_ & CLONE_NEWNS) {
            if (auto result = SetUpMountNamespace(); !result) {
                LOG(FATAL) << "Service '" << name_
                           << "' could not set up mount namespace: " << result.error();
            }
        }

        if (namespace_flags_ & CLONE_NEWPID) {
            // This will fork again to run an init process inside the PID
            // namespace.
            if (auto result = SetUpPidNamespace(); !result) {
                LOG(FATAL) << "Service '" << name_
                           << "' could not set up PID namespace: " << result.error();
            }
        }

        for (const auto& [key, value] : environment_vars_) {
            setenv(key.c_str(), value.c_str(), 1);
        }

        std::for_each(descriptors_.begin(), descriptors_.end(),
                      std::bind(&DescriptorInfo::CreateAndPublish, std::placeholders::_1, scon));

        // See if there were "writepid" instructions to write to files under cpuset path.
        std::string cpuset_path;
        if (CgroupGetControllerPath("cpuset", &cpuset_path)) {
            auto cpuset_predicate = [&cpuset_path](const std::string& path) {
                return StartsWith(path, cpuset_path + "/");
            };
            auto iter =
                    std::find_if(writepid_files_.begin(), writepid_files_.end(), cpuset_predicate);
            if (iter == writepid_files_.end()) {
                // There were no "writepid" instructions for cpusets, check if the system default
                // cpuset is specified to be used for the process.
                std::string default_cpuset = GetProperty("ro.cpuset.default", "");
                if (!default_cpuset.empty()) {
                    // Make sure the cpuset name starts and ends with '/'.
                    // A single '/' means the 'root' cpuset.
                    if (default_cpuset.front() != '/') {
                        default_cpuset.insert(0, 1, '/');
                    }
                    if (default_cpuset.back() != '/') {
                        default_cpuset.push_back('/');
                    }
                    writepid_files_.push_back(
                            StringPrintf("%s%stasks", cpuset_path.c_str(), default_cpuset.c_str()));
                }
            }
        } else {
            LOG(ERROR) << "cpuset cgroup controller is not mounted!";
        }
        std::string pid_str = std::to_string(getpid());
        for (const auto& file : writepid_files_) {
            if (!WriteStringToFile(pid_str, file)) {
                PLOG(ERROR) << "couldn't write " << pid_str << " to " << file;
            }
        }

        if (ioprio_class_ != IoSchedClass_NONE) {
            if (android_set_ioprio(getpid(), ioprio_class_, ioprio_pri_)) {
                PLOG(ERROR) << "failed to set pid " << getpid()
                            << " ioprio=" << ioprio_class_ << "," << ioprio_pri_;
            }
        }

        if (needs_console) {
            setsid();
            OpenConsole();
        } else {
            ZapStdio();
        }

        // As requested, set our gid, supplemental gids, uid, context, and
        // priority. Aborts on failure.
        SetProcessAttributes();

        if (!ExpandArgsAndExecv(args_, sigstop_)) {
            PLOG(ERROR) << "cannot execve('" << args_[0] << "')";
        }

        _exit(127);
    }

    if (pid < 0) {
        pid_ = 0;
        return ErrnoError() << "Failed to fork";
    }
    //主进程执行
    if (oom_score_adjust_ != -1000) {
        std::string oom_str = std::to_string(oom_score_adjust_);
        std::string oom_file = StringPrintf("/proc/%d/oom_score_adj", pid);
        if (!WriteStringToFile(oom_str, oom_file)) {
            PLOG(ERROR) << "couldn't write oom_score_adj";
        }
    }

    time_started_ = boot_clock::now();
    pid_ = pid;
    flags_ |= SVC_RUNNING;
    start_order_ = next_start_order_++;
    process_cgroup_empty_ = false;

    bool use_memcg = swappiness_ != -1 || soft_limit_in_bytes_ != -1 || limit_in_bytes_ != -1 ||
                      limit_percent_ != -1 || !limit_property_.empty();
    errno = -createProcessGroup(uid_, pid_, use_memcg);
    if (errno != 0) {
        PLOG(ERROR) << "createProcessGroup(" << uid_ << ", " << pid_ << ") failed for service '"
                    << name_ << "'";
    } else if (use_memcg) {
        if (swappiness_ != -1) {
            if (!setProcessGroupSwappiness(uid_, pid_, swappiness_)) {
                PLOG(ERROR) << "setProcessGroupSwappiness failed";
            }
        }

        if (soft_limit_in_bytes_ != -1) {
            if (!setProcessGroupSoftLimit(uid_, pid_, soft_limit_in_bytes_)) {
                PLOG(ERROR) << "setProcessGroupSoftLimit failed";
            }
        }

        size_t computed_limit_in_bytes = limit_in_bytes_;
        if (limit_percent_ != -1) {
            long page_size = sysconf(_SC_PAGESIZE);
            long num_pages = sysconf(_SC_PHYS_PAGES);
            if (page_size > 0 && num_pages > 0) {
                size_t max_mem = SIZE_MAX;
                if (size_t(num_pages) < SIZE_MAX / size_t(page_size)) {
                    max_mem = size_t(num_pages) * size_t(page_size);
                }
                computed_limit_in_bytes =
                        std::min(computed_limit_in_bytes, max_mem / 100 * limit_percent_);
            }
        }

        if (!limit_property_.empty()) {
            // This ends up overwriting computed_limit_in_bytes but only if the
            // property is defined.
            computed_limit_in_bytes = android::base::GetUintProperty(
                    limit_property_, computed_limit_in_bytes, SIZE_MAX);
        }

        if (computed_limit_in_bytes != size_t(-1)) {
            if (!setProcessGroupLimit(uid_, pid_, computed_limit_in_bytes)) {
                PLOG(ERROR) << "setProcessGroupLimit failed";
            }
        }
    }

    NotifyStateChange("running");
    return Success();
}
```

-   fork创建子进程
    -   子进程
        -   service中的args\_为“/system/bin/app\_process64 -Xzygote /system/bin --zygote --start-system-server”
        -   ExpandArgsAndExecv函数在新的进程中运行可执行程序/system/bin/app\_process64，启动参数为“-Xzygote /system/bin --zygote --start-system-server”
        -   到此为止zygote服务就被启动了
    -   主进程
        -   flags\_ |= SVC\_RUNNING设置service运行状态
        -   调用NotifyStateChange（这个涉及属性变化对Action的影响，放到下一篇文章专门讲解）
-   设置time\_started\_时间，表示服务启动时间，这个在后面服务重启时会用到

```
//system/core/init/service.cpp

void Service::NotifyStateChange(const std::string& new_state) const {
    if ((flags_ & SVC_TEMPORARY) != 0) {
        // Services created by 'exec' are temporary and don't have properties tracking their state.
        return;
    }

    std::string prop_name = "init.svc." + name_;
    property_set(prop_name, new_state);

    if (new_state == "running") {
        uint64_t start_ns = time_started_.time_since_epoch().count();
        std::string boottime_property = "ro.boottime." + name_;
        if (GetProperty(boottime_property, "").empty()) {
            property_set(boottime_property, std::to_string(start_ns));
        }
    }
}
```

NotifyStateChange函数执行 2 个比较重要的功能

-   调用property\_set在属性系统中记录该服务的运行状态（关于属性服务需要看前面写的属性服务系列文章）
-   通知属性变化，因为rc文件中定义的Action的trigger可以是属性，因此当属性有变化时需要通知其进行处理（这个在"属性变化如何影响Service"一文中讲解）

到此为止zygote服务已被启动，但是对于late-init的执行并没结束，我们继续看循环。

```
on late-init
    trigger early-fs

    trigger fs
    trigger post-fs

    trigger late-fs

    # Now we can mount /data. File encryption requires keymaster to decrypt
    # /data, which in turn can only be loaded when system properties are present.
    trigger post-fs-data

    # Load persist properties and override properties (if enabled) from /data.
    trigger load_persist_props_action

    # Now we can start zygote for devices with file based encryption
    trigger zygote-start

    # Remove a file to wake up anything waiting for firmware.
    trigger firmware_mounts_complete

    trigger early-boot
    trigger boot
```

```
int SecondStageMain(int argc, char** argv) {
    ...省略代码
    
    while (true) {
        if (!(waiting_for_prop || Service::is_exec_service_running())) {
            am.ExecuteOneCommand();
        }
        
        if (!(waiting_for_prop || Service::is_exec_service_running())) {
            ...省略代码
            
            // If there's more work to do, wake up again immediately.
            if (am.HasMoreCommands()) epoll_timeout = 0ms;
        }
        
        if (auto result = epoll.Wait(epoll_timeout); !result) {
            LOG(ERROR) << result.error();
        }
    }
    
    ...省略代码 
} 
```

这个循环中会调用HasMoreCommands判断late-init段是否还有commond需要执行，这样循环执行Section中的所有commond，直到所有commond被执行完，这样这个trigger就执行结束了，当init进程没有可执行的事件时，会调用Wait进入休眠，知道有事件将其唤醒。