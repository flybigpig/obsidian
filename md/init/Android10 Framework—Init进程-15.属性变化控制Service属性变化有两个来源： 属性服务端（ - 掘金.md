       ![](https://lf-web-assets.juejin.cn/obj/juejin-web/xitu_juejin_web/img/banner.a5c9f88.jpg)

> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

属性变化有两个来源：

-   属性服务端（也就是init进程）自身修改属性值，例如上一篇文章“启动服务”中最后调用NotifyStateChange，它里面会调用property\_set修改属性，而property\_set实际上就是InitPropertySet
-   客户端修改属性（参考“Android Framework—Init进程—9.客户端操作属性”）它最近也是通过socket调用到init进程，在init进程中调用PropertySet完成属性的修改

```
//system/core/init/property_service.cpp

uint32_t (*property_set)(const std::string& name, const std::string& value) = InitPropertySet;

static uint32_t PropertySet(const std::string& name, const std::string& value, std::string* error) {
    size_t valuelen = value.size();

    if (!IsLegalPropertyName(name)) {
        *error = "Illegal property name";
        return PROP_ERROR_INVALID_NAME;
    }

    if (valuelen >= PROP_VALUE_MAX && !StartsWith(name, "ro.")) {
        *error = "Property value too long";
        return PROP_ERROR_INVALID_VALUE;
    }

    if (mbstowcs(nullptr, value.data(), 0) == static_cast<std::size_t>(-1)) {
        *error = "Value is not a UTF8 encoded string";
        return PROP_ERROR_INVALID_VALUE;
    }

    //更新或添加属性值
    prop_info* pi = (prop_info*) __system_property_find(name.c_str());
    if (pi != nullptr) {
        // ro.* properties are actually "write-once".
        if (StartsWith(name, "ro.")) {
            *error = "Read-only property was already set";
            return PROP_ERROR_READ_ONLY_PROPERTY;
        }

        __system_property_update(pi, value.c_str(), valuelen);
    } else {
        int rc = __system_property_add(name.c_str(), name.size(), value.c_str(), valuelen);
        if (rc < 0) {
            *error = "__system_property_add failed";
            return PROP_ERROR_SET_FAILED;
        }
    }

    // Don't write properties to disk until after we have read all default
    // properties to prevent them from being overwritten by default values.
    if (persistent_properties_loaded && StartsWith(name, "persist.")) {
        WritePersistentProperty(name, value);
    }
    //通知属性变化
    property_changed(name, value);
    return PROP_SUCCESS;
}
```

-   更新或添加属性值（这个在属性服务系列文章分析过了）
-   通知属性变化

```
void property_changed(const std::string& name, const std::string& value) {
    // If the property is sys.powerctl, we bypass the event queue and immediately handle it.
    // This is to ensure that init will always and immediately shutdown/reboot, regardless of
    // if there are other pending events to process or if init is waiting on an exec service or
    // waiting on a property.
    // In non-thermal-shutdown case, 'shutdown' trigger will be fired to let device specific
    // commands to be executed.
    //检查是否是关机事件
    if (name == "sys.powerctl") {
        // Despite the above comment, we can't call HandlePowerctlMessage() in this function,
        // because it modifies the contents of the action queue, which can cause the action queue
        // to get into a bad state if this function is called from a command being executed by the
        // action queue.  Instead we set this flag and ensure that shutdown happens before the next
        // command is run in the main init loop.
        // TODO: once property service is removed from init, this will never happen from a builtin,
        // but rather from a callback from the property service socket, in which case this hack can
        // go away.
        shutdown_command = value;
        do_shutdown = true;
    }
    //属性变化事件入队
    if (property_triggers_enabled) ActionManager::GetInstance().QueuePropertyChange(name, value);

    if (waiting_for_prop) {
        if (wait_prop_name == name && wait_prop_value == value) {
            LOG(INFO) << "Wait for property '" << wait_prop_name << "=" << wait_prop_value
                      << "' took " << *waiting_for_prop;
            ResetWaitForProp();
        }
    }
}
```

-   检查是否是关机事件，如果是do\_shutdown设置为true，后面会使用到这个变量
-   调用QueuePropertyChange事件入队，最终事件添加到event\_queue\_中，需要注意的是现在添加到event\_queue\_中的是PropertyChange对象，在“服务启动”文章中分析时添加的是EventTrigger对象

```
void ActionManager::QueuePropertyChange(const std::string& name, const std::string& value) {
    event_queue_.emplace(std::make_pair(name, value));
}
```

当有需要执行的事件时，wake就会被唤醒

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

又开始执行ExecuteOneCommand

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

    ....
}
```

-   前面我们分析了事件触发的过程
    -   遍历actions\_，然后根据EventTrigger查找对应的Action，然后添加到current\_executing\_actions\_队列中
        
        ```
        bool Action::CheckEvent(const EventTrigger& event_trigger) const {
            return event_trigger == event_trigger_ && CheckPropertyTriggers();
        }
        ```
        
-   这里分析属性触发的过程
    -   此时调用的action->CheckEvent就是如下函数了，显然这里是根据属性去查找Action，找到所有与该属性变化有关的Action，然后都添加到current\_executing\_actions\_队列中，后续会将这些Action都再执行一次。
        
        ```
        bool Action::CheckEvent(const PropertyChange& property_change) const {
            const auto& [name, value] = property_change;
            return event_trigger_.empty() && CheckPropertyTriggers(name, value);
        }
        ```
        

property\_triggers\_保存的就是该Action的PropertyTrigger，遍历property\_triggers\_然后找到返回true，否则返回false

```
bool Action::CheckPropertyTriggers(const std::string& name,
                                   const std::string& value) const {
    if (property_triggers_.empty()) {
        return true;
    }

    bool found = name.empty();
    for (const auto& [trigger_name, trigger_value] : property_triggers_) {
        if (trigger_name == name) {
            if (trigger_value != "*" && trigger_value != value) {
                return false;
            } else {
                found = true;
            }
        } else {
            std::string prop_val = android::base::GetProperty(trigger_name, "");
            if (prop_val.empty() || (trigger_value != "*" && trigger_value != prop_val)) {
                return false;
            }
        }
    }
    return found;
}
```

![avatar](https://p26-passport.byteacctimg.com/img/user-avatar/bde1ef02645bf5d95c45e5b30bef749f~40x40.awebp)