> hongxi.zhu 2023-6-16  
> pixel2 XL Lineageos\_20

#### 目录

+   +   [1\. 处理属性控制信息](#1__4)
    +   [2\. 什么时候唤醒主线程来处理](#2___249)
    +   [3\. 查找属性写入端](#3__459)

### 1\. 处理属性控制信息

以`setprop ctl.start bootanim`为例子探索  
从init进程的学习可以知道，当init进程完成开机初始化等一系列事情后会主线程会进入loop中，然后等待从`epoll.Wait()`中唤醒

```cpp
int SecondStageMain(int argc, char** argv) {
    if (REBOOT_BOOTLOADER_ON_PANIC) {
        InstallRebootSignalHandlers();
    }
	//init进程需要在开机做的各种事情
	...
    while (true) {  //完成上面的事情后，init进程进入loop, 通过epoll等待关心的事件的发生
        // By default, sleep until something happens.
        auto epoll_timeout = std::optional<std::chrono::milliseconds>{kDiagnosticTimeout};

        auto shutdown_command = shutdown_state.CheckShutdown();
        if (shutdown_command) {
            LOG(INFO) << "Got shutdown_command '" << *shutdown_command
                      << "' Calling HandlePowerctlMessage()";
            HandlePowerctlMessage(*shutdown_command);
            shutdown_state.set_do_shutdown(false);
        }

        if (!(prop_waiter_state.MightBeWaiting() || Service::is_exec_service_running())) {
            am.ExecuteOneCommand();
        }
        if (!IsShuttingDown()) {
            auto next_process_action_time = HandleProcessActions();

            // If there's a process that needs restarting, wake up in time for that.
            if (next_process_action_time) {
                epoll_timeout = std::chrono::ceil<std::chrono::milliseconds>(
                        *next_process_action_time - boot_clock::now());
                if (*epoll_timeout < 0ms) epoll_timeout = 0ms;
            }
        }

        if (!(prop_waiter_state.MightBeWaiting() || Service::is_exec_service_running())) {
            // If there's more work to do, wake up again immediately.
            if (am.HasMoreCommands()) epoll_timeout = 0ms;
        }

        auto pending_functions = epoll.Wait(epoll_timeout);  //主线程会block在这里，除非timeout或者wake from event fd才会往下走
        if (!pending_functions.ok()) {
            LOG(ERROR) << pending_functions.error();
        } else if (!pending_functions->empty()) {
            // We always reap children before responding to the other pending functions. This is to
            // prevent a race where other daemons see that a service has exited and ask init to
            // start it again via ctl.start before init has reaped it.
            ReapAnyOutstandingChildren();
            for (const auto& function : *pending_functions) {
                (*function)();
            }
        } else if (Service::is_exec_service_running()) {
            static bool dumped_diagnostics = false;
            std::chrono::duration<double> waited =
                    std::chrono::steady_clock::now() - Service::exec_service_started();
            if (waited >= kDiagnosticTimeout) {
                LOG(ERROR) << "Exec service is hung? Waited " << waited.count()
                           << " without SIGCHLD";
                if (!dumped_diagnostics) {
                    DumpPidFds("exec service opened: ", Service::exec_service_pid());

                    std::string status_file =
                            "/proc/" + std::to_string(Service::exec_service_pid()) + "/status";
                    DumpFile("exec service: ", status_file);
                    dumped_diagnostics = true;

                    LOG(INFO) << "Attempting to handle any stuck SIGCHLDs...";
                    HandleSignalFd(true);
                }
            }
        }
        if (!IsShuttingDown()) {
            HandleControlMessages();  //处理
            SetUsbController();
        }
    }

    return 0;
}
```

那到这里我们就需要弄明白几个问题：

1.  属性控制事件怎么来的，谁发送的
2.  什么时候唤醒主线程来处理
3.  怎么处理ctl.start此类信息  
    我们这里反向来找答案，因为我们最容易找到第三点，因为上面我们在init的主线程loop中已经看到了

system/core/init/init.cpp

```cpp
static void HandleControlMessages() {
    auto lock = std::unique_lock{pending_control_messages_lock};
    // Init historically would only execute handle one property message, including control messages
    // in each iteration of its main loop.  We retain this behavior here to prevent starvation of
    // other actions in the main loop.
    if (!pending_control_messages.empty()) {  //关注这个消息队列，主线程处理时是从这个队列拿消息
        auto control_message = pending_control_messages.front();
        pending_control_messages.pop();
        lock.unlock();

        bool success = HandleControlMessage(control_message.message, control_message.name,
                                            control_message.pid);

        uint32_t response = success ? PROP_SUCCESS : PROP_ERROR_HANDLE_CONTROL_MESSAGE;
        if (control_message.fd != -1) {
            TEMP_FAILURE_RETRY(send(control_message.fd, &response, sizeof(response), 0));
            close(control_message.fd);
        }
        lock.lock();
    }
    // If we still have items to process, make sure we wake back up to do so.
    if (!pending_control_messages.empty()) {
        WakeMainInitThread();
    }
}

static bool HandleControlMessage(std::string_view message, const std::string& name,
                                 pid_t from_pid) {
    std::string cmdline_path = StringPrintf("proc/%d/cmdline", from_pid);
    std::string process_cmdline;
    if (ReadFileToString(cmdline_path, &process_cmdline)) {
        std::replace(process_cmdline.begin(), process_cmdline.end(), '\0', ' ');
        process_cmdline = Trim(process_cmdline);
    } else {
        process_cmdline = "unknown process";
    }

    Service* service = nullptr;
    auto action = message;
    if (ConsumePrefix(&action, "interface_")) {  //命令是否包含`interface_`
        service = ServiceList::GetInstance().FindInterface(name);  //有些服务是以接口形式向外提供的，而不是服务名称本身,例如：ctl.interface_start xxx
    } else {
        service = ServiceList::GetInstance().FindService(name);  //查询服务，init进程启动时解析rc文件会将其中所有声明的service对象保存下来，这样就可以根据name去获取对应的service对象
    }
	...
    const auto& map = GetControlMessageMap();  //获取整个action map
    const auto it = map.find(action);  //从map中找到action对应的pair对（key, value）
    if (it == map.end()) {
        LOG(ERROR) << "Unknown control msg '" << message << "'";
        return false;
    }
    const auto& function = it->second;  //获取value值->即真正的action对应的执行方法

    if (auto result = function(service); !result.ok()) {  //调用这个方法，这个方法实际上就是调用service对象的Start()
        LOG(ERROR) << "Control message: Could not ctl." << message << " for '" << name
                   << "' from pid: " << from_pid << " (" << process_cmdline
                   << "): " << result.error();
        return false;
    }

    LOG(INFO) << "Control message: Processed ctl." << message << " for '" << name
              << "' from pid: " << from_pid << " (" << process_cmdline << ")";
    return true;
}

using ControlMessageFunction = std::function<Result<void>(Service*)>;

static const std::map<std::string, ControlMessageFunction, std::less<>>& GetControlMessageMap() {
    // clang-format off
    static const std::map<std::string, ControlMessageFunction, std::less<>> control_message_functions = {
        {"sigstop_on",        [](auto* service) { service->set_sigstop(true); return Result<void>{}; }},
        {"sigstop_off",       [](auto* service) { service->set_sigstop(false); return Result<void>{}; }},
        {"oneshot_on",        [](auto* service) { service->set_oneshot(true); return Result<void>{}; }},
        {"oneshot_off",       [](auto* service) { service->set_oneshot(false); return Result<void>{}; }},
        {"start",             DoControlStart},  //真正的执行方法，也就是second()
        {"stop",              DoControlStop},
        {"restart",           DoControlRestart},
    };
    // clang-format on

    return control_message_functions;
}

static Result<void> DoControlStart(Service* service) {
    return service->Start();  //action对应的方法实际上是service的Start(),从这里就回去启动service
}
```

system/core/init/service.cpp

```cpp
Result<void> Service::Start() {
   auto reboot_on_failure = make_scope_guard([this] {
       if (on_failure_reboot_target_) {
           trigger_shutdown(*on_failure_reboot_target_);
       }
   });
 	...
 	
   pid_t pid = -1;
   if (namespaces_.flags) {  //如果配置了namespaces_.flags
       pid = clone(nullptr, nullptr, namespaces_.flags | SIGCHLD, nullptr);
   } else {  //例子bootanim服务并没有配置namespaces_.flags，所以用的是fork
       pid = fork();
   }

   if (pid == 0) {  //子进程-> 启动的服务进程
       umask(077);
       RunService(override_mount_namespace, descriptors, std::move(pipefd)); //启动服务的逻辑
       _exit(127);
   }
   ... //父进程往下做一些收尾工作，比如调整子进程adj，cgroup等
}
```

执行execv

```cpp
// Enters namespaces, sets environment variables, writes PID files and runs the service executable.
void Service::RunService(const std::optional<MountNamespace>& override_mount_namespace,
                         const std::vector<Descriptor>& descriptors,
                         std::unique_ptr<std::array<int, 2>, decltype(&ClosePipe)> pipefd) {
	...

    if (!ExpandArgsAndExecv(args_, sigstop_)) {
        PLOG(ERROR) << "cannot execv('" << args_[0]
                    << "'). See the 'Debugging init' section of init's README.md for tips";
    }
}

static bool ExpandArgsAndExecv(const std::vector<std::string>& args, bool sigstop) {
    std::vector<std::string> expanded_args;
    std::vector<char*> c_strings;

	// 启动参数组装
    expanded_args.resize(args.size());
    c_strings.push_back(const_cast<char*>(args[0].data()));
    for (std::size_t i = 1; i < args.size(); ++i) {
        auto expanded_arg = ExpandProps(args[i]);
        if (!expanded_arg.ok()) {
            LOG(FATAL) << args[0] << ": cannot expand arguments': " << expanded_arg.error();
        }
        expanded_args[i] = *expanded_arg;
        c_strings.push_back(expanded_args[i].data());
    }
    c_strings.push_back(nullptr);

	...

    return execv(c_strings[0], c_strings.data()) == 0;  //执行execv，就会找到main方法,让服务跑起来。
}
```

上面我们就知道了init主线程如何处理ctl.start的消息来启动服务进程了，我们接下来反回去找第二个问题的答案，什么时候唤醒主线程来处理？ 也就是说，事件是什么时候会被加到`pending_control_messages`这个队列中的，查找这个队列的流程，得到第二个问题的流程。

### 2\. 什么时候唤醒主线程来处理

system/core/init/init.cpp

```cpp
int SecondStageMain(int argc, char** argv) {
	...
    Epoll epoll;
    if (auto result = epoll.Open(); !result.ok()) {
        PLOG(FATAL) << result.error();
    }

    InstallSignalFdHandler(&epoll);
    InstallInitNotifier(&epoll);
    StartPropertyService(&property_fd);  //启动属性服务，也就是启动一个线程去处理属性控制相关的业务

	...

    // Restore prio before main loop
    setpriority(PRIO_PROCESS, 0, 0);
    while (true) {
        // By default, sleep until something happens.
		...
        if (!IsShuttingDown()) {
            HandleControlMessages();
            SetUsbController();
        }
    }

    return 0;
}
```

在main loop前，init进程会启动一个线程单独处理属性控制相关的业务  
system/core/init/property\_service.cpp

```cpp
void StartPropertyService(int* epoll_socket) {
    InitPropertySet("ro.property_service.version", "2");  //这个很重要，属性写入端会判断这个走不一样的逻辑，例如是否支持long key-value类型等

    int sockets[2];  //创建一对socketpair用于init主线程和property_service子线程通信
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
        PLOG(FATAL) << "Failed to socketpair() between property_service and init";
    }
    *epoll_socket = from_init_socket = sockets[0];  //写端给init
    init_socket = sockets[1];  //读端给自己
    StartSendingMessages();  //设置标志位，告诉init，准备好了，可以发消息了

	//这个socket是用来和属性写入端通信的，当属性写入时通过这个socket通知property_service
    if (auto result = CreateSocket(PROP_SERVICE_NAME, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
                                   /*passcred=*/false, /*should_listen=*/false, 0666, /*uid=*/0,
                                   /*gid=*/0, /*socketcon=*/{});
        result.ok()) {
        property_set_fd = *result;  //将socket fd保存下来
    } else {
        LOG(FATAL) << "start_property_service socket creation failed: " << result.error();
    }

    listen(property_set_fd, 8);  //作为socket服务端监听property_set_fd

    auto new_thread = std::thread{PropertyServiceThread};  //启动线程，threadLoop->PropertyServiceThread()
    property_service_thread.swap(new_thread);
}
```

property\_service主要业务来源于于init的socketpair，和属性写入端的socket，这里会去创建并初始化，然后启动线程

```cpp
static void PropertyServiceThread() {
    Epoll epoll;
    if (auto result = epoll.Open(); !result.ok()) {
        LOG(FATAL) << result.error();
    }
	//把property_set_fd注册到epoll中监听，当属性写入端往socket写入消息，fd有事件就回调handle_property_set_fd()
    if (auto result = epoll.RegisterHandler(property_set_fd, handle_property_set_fd);
        !result.ok()) {
        LOG(FATAL) << result.error();
    }
	//同上，把init_socket注册到epoll中监听，当init端通知，fd有事件就回调HandleInitSocket()
    if (auto result = epoll.RegisterHandler(init_socket, HandleInitSocket); !result.ok()) {
        LOG(FATAL) << result.error();
    }

    while (true) {
        auto pending_functions = epoll.Wait(std::nullopt);  //property_service线程在这里sleep等待事件，一旦有事件到来就唤醒并执行回调方法。
        if (!pending_functions.ok()) {
            LOG(ERROR) << pending_functions.error();
        } else {
            for (const auto& function : *pending_functions) {
                (*function)();//执行回调方法
            }
        }
    }
}
```

将两个socket fd都加入epoll的监听池子中，并等待事件的到来。这里我们主要关注`handle_property_set_fd()`, 属性事件到来时的回调

```cpp
static void handle_property_set_fd() {
    static constexpr uint32_t kDefaultSocketTimeout = 2000; /* ms */

    int s = accept4(property_set_fd, nullptr, nullptr, SOCK_CLOEXEC);  //允许property_set_fd对端连接，返回对应的socket
    if (s == -1) {
        return;
    }

    ucred cr;
    socklen_t cr_size = sizeof(cr);
    if (getsockopt(s, SOL_SOCKET, SO_PEERCRED, &cr, &cr_size) < 0) {
        close(s);
        PLOG(ERROR) << "sys_prop: unable to get SO_PEERCRED";
        return;
    }

    SocketConnection socket(s, cr);
    uint32_t timeout_ms = kDefaultSocketTimeout;

    uint32_t cmd = 0;
    if (!socket.RecvUint32(&cmd, &timeout_ms)) {  //接收property_set_fd对端的数据
        PLOG(ERROR) << "sys_prop: error while reading command from the socket";
        socket.SendUint32(PROP_ERROR_READ_CMD);
        return;
    }

    switch (cmd) {
    case PROP_MSG_SETPROP: {
 		...
        break;
      }

    case PROP_MSG_SETPROP2: {  //从打印看走的是这里
        std::string name;  //属性的key
        std::string value;  //属性的value
        if (!socket.RecvString(&name, &timeout_ms) ||
            !socket.RecvString(&value, &timeout_ms)) {
          PLOG(ERROR) << "sys_prop(PROP_MSG_SETPROP2): error while reading name/value from the socket";
          socket.SendUint32(PROP_ERROR_READ_DATA);
          return;
        }

        std::string source_context;
        if (!socket.GetSourceContext(&source_context)) {
            PLOG(ERROR) << "Unable to set property '" << name << "': getpeercon() failed";
            socket.SendUint32(PROP_ERROR_PERMISSION_DENIED);
            return;
        }

        const auto& cr = socket.cred();
        std::string error;
        uint32_t result = HandlePropertySet(name, value, source_context, cr, &socket, &error);  //处理接收的事件
        if (result != PROP_SUCCESS) {
            LOG(ERROR) << "Unable to set property '" << name << "' from uid:" << cr.uid
                       << " gid:" << cr.gid << " pid:" << cr.pid << ": " << error;
        }
        socket.SendUint32(result);
        break;
      }

    default:
        LOG(ERROR) << "sys_prop: invalid command " << cmd;
        socket.SendUint32(PROP_ERROR_INVALID_CMD);
        break;
    }
}
```

当属性写入端socket发来消息，那就根据标准Linux socket消息处理流程接收并处理, 最后获取到对应内容，根据内容类型调用`HandlePropertySet`

```cpp
// This returns one of the enum of PROP_SUCCESS or PROP_ERROR*.
uint32_t HandlePropertySet(const std::string& name, const std::string& value,
                           const std::string& source_context, const ucred& cr,
                           SocketConnection* socket, std::string* error) {
	...

    if (StartsWith(name, "ctl.")) {  //如果是ctl.开头的控制信息走这里
        return SendControlMessage(name.c_str() + 4, value, cr.pid, socket, error);
    }

	//如果是其他的属性走下面处理
	...

    return PropertySet(name, value, error);
}
```

根据属性的前缀，走不同的分支，我们例子看的是`ctl.`开头的, 其他的同理

```cpp
static uint32_t SendControlMessage(const std::string& msg, const std::string& name, pid_t pid,
                                   SocketConnection* socket, std::string* error) {
	...
    bool queue_success = QueueControlMessage(msg, name, pid, fd);  //从这里就可以知道消息入队的操作了
    if (!queue_success && fd != -1) {
        uint32_t response = PROP_ERROR_HANDLE_CONTROL_MESSAGE;
        TEMP_FAILURE_RETRY(send(fd, &response, sizeof(response), 0));  //处理完，回复给socket写端，并关闭fd
        close(fd);
    }

    return PROP_SUCCESS;
}

bool QueueControlMessage(const std::string& message, const std::string& name, pid_t pid, int fd) {
    auto lock = std::lock_guard{pending_control_messages_lock};
	...
    pending_control_messages.push({message, name, pid, fd});  //将消息入队
    WakeMainInitThread();  //唤醒主线程处理
    return true;
}

static void WakeMainInitThread() {
    uint64_t counter = 1;
    TEMP_FAILURE_RETRY(write(wake_main_thread_fd, &counter, sizeof(counter)));  //往主线程申请的fd写入任意数据，唤醒主线程
}
```

从上面可知到当socket对端，也就是属性写入端发来数据时唤醒property-service线程，然后将消息入队，唤醒init主线程处理，第二个问题找到答案了，最后一个问题，属性控制事件怎么来的，谁发送的？我们从第二个问题中可知，第三个问题实际上就是找到socket对端在哪里。

### 3\. 查找属性写入端

根据socket的路径节点`"/dev/socket/" PROP_SERVICE_NAME;`搜索，实际是在bionic/libc/bionic/system\_property\_set.cpp中，属性写入是被libc实现为标准API了，所以每个地方写入属性都会调用到这里

```cpp
__BIONIC_WEAK_FOR_NATIVE_BRIDGE
int __system_property_set(const char* key, const char* value) {
	...
  if (g_propservice_protocol_version == kProtocolVersion1) {
    // Old protocol does not support long names or values
    ...
  } else {
    // New protocol only allows long values for ro. properties only.
    if (strlen(value) >= PROP_VALUE_MAX && strncmp(key, "ro.", 3) != 0) return -1;
    // Use proper protocol
    PropertyServiceConnection connection;  //这个里面就是封装了socket对应的信息
    if (!connection.IsValid()) {
      errno = connection.GetLastError();
      async_safe_format_log(
          ANDROID_LOG_WARN, "libc",
          "Unable to set property \"%s\" to \"%s\": connection failed; errno=%d (%s)", key, value,
          errno, strerror(errno));
      return -1;
    }

    SocketWriter writer(&connection);
    if (!writer.WriteUint32(PROP_MSG_SETPROP2).WriteString(key).WriteString(value).Send()) {  //往init进程的property-service的socket写入数据, 包括cmd = PROP_MSG_SETPROP2, key, value
      errno = connection.GetLastError();
      async_safe_format_log(ANDROID_LOG_WARN, "libc",
                            "Unable to set property \"%s\" to \"%s\": write failed; errno=%d (%s)",
                            key, value, errno, strerror(errno));
      return -1;
    }

    int result = -1;
    if (!connection.RecvInt32(&result)) {  
      errno = connection.GetLastError();
      async_safe_format_log(ANDROID_LOG_WARN, "libc",
                            "Unable to set property \"%s\" to \"%s\": recv failed; errno=%d (%s)",
                            key, value, errno, strerror(errno));
      return -1;
    }
    ...

    return 0;
  }
}

```

libc中`__system_property_set`中，当调用该方法写入属性时都会通过socket通知init进程中的property service

```cpp
static const char property_service_socket[] = "/dev/socket/" PROP_SERVICE_NAME;
static const char* kServiceVersionPropertyName = "ro.property_service.version";

class PropertyServiceConnection {
 public:
  PropertyServiceConnection() : last_error_(0) {
    socket_.reset(::socket(AF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (socket_.get() == -1) {
      last_error_ = errno;
      return;
    }

    const size_t namelen = strlen(property_service_socket);
    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    strlcpy(addr.sun_path, property_service_socket, sizeof(addr.sun_path)); //addr
    addr.sun_family = AF_LOCAL;
    socklen_t alen = namelen + offsetof(sockaddr_un, sun_path) + 1;
	// connect对应的socket
    if (TEMP_FAILURE_RETRY(connect(socket_.get(),
                                   reinterpret_cast<sockaddr*>(&addr), alen)) == -1) {
      last_error_ = errno;
      socket_.reset();
    }
  }
  ...
}

class SocketWriter {
 public:
  explicit SocketWriter(PropertyServiceConnection* connection)
      : connection_(connection), iov_index_(0), uint_buf_index_(0) {
  }

  SocketWriter& WriteUint32(uint32_t value) {
    CHECK(uint_buf_index_ < kUintBufSize);
    CHECK(iov_index_ < kIovSize);
    uint32_t* ptr = uint_buf_ + uint_buf_index_;
    uint_buf_[uint_buf_index_++] = value;
    iov_[iov_index_].iov_base = ptr;
    iov_[iov_index_].iov_len = sizeof(*ptr);
    ++iov_index_;
    return *this;
  }

  SocketWriter& WriteString(const char* value) {
    uint32_t valuelen = strlen(value);
    WriteUint32(valuelen);
    if (valuelen == 0) {
      return *this;
    }

    CHECK(iov_index_ < kIovSize);
    iov_[iov_index_].iov_base = const_cast<char*>(value);
    iov_[iov_index_].iov_len = valuelen;
    ++iov_index_;

    return *this;
  }

  bool Send() {
    if (!connection_->IsValid()) {
      return false;
    }

    if (writev(connection_->socket(), iov_, iov_index_) == -1) {
      connection_->last_error_ = errno;
      return false;
    }

    iov_index_ = uint_buf_index_ = 0;
    return true;
  }
  ...
}
```

到这里，基本就串起来解答了`ctl.*`的属性控制对应服务如何实现。