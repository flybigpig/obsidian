       ![](https://lf-web-assets.juejin.cn/obj/juejin-web/xitu_juejin_web/img/banner.a5c9f88.jpg)

> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

属性服务的本质就是init进程作为服务端创建并监听socket，其它进程作为客户端通过socket添加或更新属性。

```
// system/core/init/init.cpp
int SecondStageMain(int argc, char** argv) {
    StartPropertyService(&epoll);
}

void StartPropertyService(Epoll* epoll) {
    selinux_callback cb;
    cb.func_audit = SelinuxAuditCallback;
    selinux_set_callback(SELINUX_CB_AUDIT, cb);

    property_set("ro.property_service.version", "2");

    property_set_fd = CreateSocket(PROP_SERVICE_NAME, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
                                   false, 0666, 0, 0, nullptr);
    if (property_set_fd == -1) {
        PLOG(FATAL) << "start_property_service socket creation failed";
    }

    listen(property_set_fd, 8);

    if (auto result = epoll->RegisterHandler(property_set_fd, handle_property_set_fd); !result) {
        PLOG(FATAL) << result.error();
    }
}

// system/core/libcutils/include/cutils/sockets.h
#define ANDROID_SOCKET_DIR"/dev/socket"

// bionic/libc/include/sys/_system_properties.h
#define PROP_SERVICE_NAME "property_service"
```

-   创建并监听Socket，"/dev/socket/property\_service"
-   将property\_set\_fd添加到epoll中，监听客户端发过来的请求
-   handle\_property\_set\_fd处理客户端请求

```
static void handle_property_set_fd() {
    static constexpr uint32_t kDefaultSocketTimeout = 2000; /* ms */

    int s = accept4(property_set_fd, nullptr, nullptr, SOCK_CLOEXEC);
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
    if (!socket.RecvUint32(&cmd, &timeout_ms)) {
        PLOG(ERROR) << "sys_prop: error while reading command from the socket";
        socket.SendUint32(PROP_ERROR_READ_CMD);
        return;
    }

    switch (cmd) {
    case PROP_MSG_SETPROP: {
        char prop_name[PROP_NAME_MAX];
        char prop_value[PROP_VALUE_MAX];

        if (!socket.RecvChars(prop_name, PROP_NAME_MAX, &timeout_ms) ||
            !socket.RecvChars(prop_value, PROP_VALUE_MAX, &timeout_ms)) {
          PLOG(ERROR) << "sys_prop(PROP_MSG_SETPROP): error while reading name/value from the socket";
          return;
        }

        prop_name[PROP_NAME_MAX-1] = 0;
        prop_value[PROP_VALUE_MAX-1] = 0;

        const auto& cr = socket.cred();
        std::string error;
        uint32_t result =
            HandlePropertySet(prop_name, prop_value, socket.source_context(), cr, &error);
        if (result != PROP_SUCCESS) {
            LOG(ERROR) << "Unable to set property '" << prop_name << "' to '" << prop_value
                       << "' from uid:" << cr.uid << " gid:" << cr.gid << " pid:" << cr.pid << ": "
                       << error;
        }

        break;
      }

    case PROP_MSG_SETPROP2: {
        std::string name;
        std::string value;
        if (!socket.RecvString(&name, &timeout_ms) ||
            !socket.RecvString(&value, &timeout_ms)) {
          PLOG(ERROR) << "sys_prop(PROP_MSG_SETPROP2): error while reading name/value from the socket";
          socket.SendUint32(PROP_ERROR_READ_DATA);
          return;
        }

        const auto& cr = socket.cred();
        std::string error;
        uint32_t result = HandlePropertySet(name, value, socket.source_context(), cr, &error);
        if (result != PROP_SUCCESS) {
            LOG(ERROR) << "Unable to set property '" << name << "' to '" << value
                       << "' from uid:" << cr.uid << " gid:" << cr.gid << " pid:" << cr.pid << ": "
                       << error;
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

-   客户端发送过来的数据分为 3 部分：cmd、属性name、属性value
-   根据cmd不同，对属性name、属性value做不同的解析，但是最后都是调用HandlePropertySet方法

```
uint32_t HandlePropertySet(const std::string& name, const std::string& value,
                           const std::string& source_context, const ucred& cr, std::string* error) {
    if (auto ret = CheckPermissions(name, value, source_context, cr, error); ret != PROP_SUCCESS) {
        return ret;
    }

    ....省略代码

    return PropertySet(name, value, error);
}
```

最终就是调用PropertySet完成属性设置，关于PropertySet在“属性值初始化”章已讲解过了。

到此为止属性服务启动也讲清楚了，它的流程还是比较简单的。