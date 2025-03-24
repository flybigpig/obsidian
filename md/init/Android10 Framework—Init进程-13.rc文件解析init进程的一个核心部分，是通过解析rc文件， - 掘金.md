> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

init进程的一个核心部分，是通过解析rc文件，执行Action和启动Service，配置文件解析相关代码如下：

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

首先这里创建了一个function\_map，然后将其赋值给Action对象的function\_map\_变量；那么function\_map是什么呢，它其实是个工具类

```
//system/core/init/keyword_map.h
class KeywordMap {
  public:
    using FunctionInfo = std::tuple<std::size_t, std::size_t, Function>;
    using Map = std::map<std::string, FunctionInfo>;

    virtual ~KeywordMap() {
    }

    const Result<Function> FindFunction(const std::vector<std::string>& args) const {
        using android::base::StringPrintf;

        if (args.empty()) return Error() << "Keyword needed, but not provided";

        auto& keyword = args[0];
        auto num_args = args.size() - 1;

        auto function_info_it = map().find(keyword);
        if (function_info_it == map().end()) {
            return Error() << StringPrintf("Invalid keyword '%s'", keyword.c_str());
        }

        auto function_info = function_info_it->second;

        auto min_args = std::get<0>(function_info);
        auto max_args = std::get<1>(function_info);
        if (min_args == max_args && num_args != min_args) {
            return Error() << StringPrintf("%s requires %zu argument%s", keyword.c_str(), min_args,
                                           (min_args > 1 || min_args == 0) ? "s" : "");
        }

        if (num_args < min_args || num_args > max_args) {
            if (max_args == std::numeric_limits<decltype(max_args)>::max()) {
                return Error() << StringPrintf("%s requires at least %zu argument%s",
                                               keyword.c_str(), min_args, min_args > 1 ? "s" : "");
            } else {
                return Error() << StringPrintf("%s requires between %zu and %zu arguments",
                                               keyword.c_str(), min_args, max_args);
            }
        }

        return std::get<Function>(function_info);
    }

  private:
    // Map of keyword ->
    // (minimum number of arguments, maximum number of arguments, function pointer)
    virtual const Map& map() const = 0;
};

//system/core/init/builtins.h
using KeywordFunctionMap = KeywordMap<std::pair<bool, BuiltinFunction>>;
class BuiltinFunctionMap : public KeywordFunctionMap {
  public:
    BuiltinFunctionMap() {}

  private:
    const Map& map() const override;
};

//system/core/init/service.cpp
class Service::OptionParserMap : public KeywordMap<OptionParser> {
  public:
    OptionParserMap() {}

  private:
    const Map& map() const override;
};
```

如下 2 个类比较重要

-   BuiltinFunctionMap
-   OptionParserMap

它们都继承KeywordMap,KeywordMap中FindFunction和map两个方法比较重要

-   FindFunction在KeywordMap中有实现，对于Action而言，FindFunction返回Action中commond对应功能实现函数；对Service而言，FindFunction返回Service中option对应解析函数
-   map的实现KeywordMap中，它根据Acton和Service不同，在各自实现中返回不同的函数描述

在这里将这些内容提前进行说明，后续会涉及到。

```
ActionManager& am = ActionManager::GetInstance();
ServiceList& sm = ServiceList::GetInstance();
```

创建了用于管理从rc配置文件解析的Action和Service，实际的解析是调用LoadBootScripts开始进行的。

```
static void LoadBootScripts(ActionManager& action_manager, ServiceList& service_list) {
    Parser parser = CreateParser(action_manager, service_list);

    std::string bootscript = GetProperty("ro.boot.init_rc", "");
    if (bootscript.empty()) {
        parser.ParseConfig("/init.rc");
        if (!parser.ParseConfig("/system/etc/init")) {
            late_import_paths.emplace_back("/system/etc/init");
        }
        if (!parser.ParseConfig("/product/etc/init")) {
            late_import_paths.emplace_back("/product/etc/init");
        }
        if (!parser.ParseConfig("/product_services/etc/init")) {
            late_import_paths.emplace_back("/product_services/etc/init");
        }
        if (!parser.ParseConfig("/odm/etc/init")) {
            late_import_paths.emplace_back("/odm/etc/init");
        }
        if (!parser.ParseConfig("/vendor/etc/init")) {
            late_import_paths.emplace_back("/vendor/etc/init");
        }
    } else {
        parser.ParseConfig(bootscript);
    }
}
```

调用CreateParser构建解析器

```
Parser CreateParser(ActionManager& action_manager, ServiceList& service_list) {
    Parser parser;

    parser.AddSectionParser("service", std::make_unique<ServiceParser>(&service_list, subcontexts));
    parser.AddSectionParser("on", std::make_unique<ActionParser>(&action_manager, subcontexts));
    parser.AddSectionParser("import", std::make_unique<ImportParser>(&parser));

    return parser;
}
```

这里实际上和前面讲的rc文件格式是对应的，三种关键字srvice、on、import都有其对应的解析器ServiceParser、ActionParser、ImportParser，其中ServiceParser、ActionParser分别又传入了service\_list和action\_manager。这三个都是section解析器，会被添加到section\_parsers\_变量中保存。

```
parser.ParseConfig("")

bool Parser::ParseConfig(const std::string& path) {
    if (is_dir(path.c_str())) {
        return ParseConfigDir(path);
    }
    return ParseConfigFile(path);
}
```

然后调用ParseConfig解析文件，可以看到其支持目录下遍历查找文件解析的，其中最重要的就是解析根目录下的/init.rc，我们以此作为主线进行分析

```
parser.ParseConfig("/init.rc");

bool Parser::ParseConfigFile(const std::string& path) {
    LOG(INFO) << "Parsing file " << path << "...";
    android::base::Timer t;
    auto config_contents = ReadFile(path);
    if (!config_contents) {
        LOG(INFO) << "Unable to read config file '" << path << "': " << config_contents.error();
        return false;
    }

    ParseData(path, &config_contents.value());

    LOG(VERBOSE) << "(Parsing " << path << " took " << t << ".)";
    return true;
}
```

读取文件内容调用ParseData

```
void Parser::ParseData(const std::string& filename, std::string* data) {
    ...省略代码

    for (;;) {
        switch (next_token(&state)) {
            case T_EOF:
                end_section();

                for (const auto& [section_name, section_parser] : section_parsers_) {
                    section_parser->EndFile();
                }

                return;
            case T_NEWLINE: {
                state.line++;
                if (args.empty()) break;
                // If we have a line matching a prefix we recognize, call its callback and unset any
                // current section parsers.  This is meant for /sys/ and /dev/ line entries for
                // uevent.
                auto line_callback = std::find_if(
                    line_callbacks_.begin(), line_callbacks_.end(),
                    [&args](const auto& c) { return android::base::StartsWith(args[0], c.first); });
                if (line_callback != line_callbacks_.end()) {
                    end_section();

                    if (auto result = line_callback->second(std::move(args)); !result) {
                        parse_error_count_++;
                        LOG(ERROR) << filename << ": " << state.line << ": " << result.error();
                    }
                } else if (section_parsers_.count(args[0])) {
                    end_section();
                    section_parser = section_parsers_[args[0]].get();
                    section_start_line = state.line;
                    if (auto result =
                            section_parser->ParseSection(std::move(args), filename, state.line);
                        !result) {
                        parse_error_count_++;
                        LOG(ERROR) << filename << ": " << state.line << ": " << result.error();
                        section_parser = nullptr;
                        bad_section_found = true;
                    }
                } else if (section_parser) {
                    if (auto result = section_parser->ParseLineSection(std::move(args), state.line);
                        !result) {
                        parse_error_count_++;
                        LOG(ERROR) << filename << ": " << state.line << ": " << result.error();
                    }
                } else if (!bad_section_found) {
                    parse_error_count_++;
                    LOG(ERROR) << filename << ": " << state.line
                               << ": Invalid section keyword found";
                }
                args.clear();
                break;
            }
            case T_TEXT:
                args.emplace_back(state.text);
                break;
        }
    }
}
```

整个文本的解析是按section来解析的（以 import, on, service 开头的， 都表示一个 section(段落)， 意味着一个关键词出现之后到出现第二个关键词 之前， 关键词后面的内容都属于一个 section）

-   T\_NEWLINE：新行
-   T\_TEXT：一行（除去关键字）中的参数
-   T\_EOF：section结束

## Action解析

以如下片段看看其解析流程

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

    exec_start apexd-bootstrap
```

首先解析

```
on early-init
```

line\_callback此时为空（后面会简单讲解一下line\_callback），会执行如下代码

```
if (section_parsers_.count(args[0])) {
    end_section();
    section_parser = section_parsers_[args[0]].get();
    section_start_line = state.line;
    if (auto result =
            section_parser->ParseSection(std::move(args), filename, state.line);
        !result) {
        parse_error_count_++;
        LOG(ERROR) << filename << ": " << state.line << ": " << result.error();
        section_parser = nullptr;
        bad_section_found = true;
    }
}
```

-   此时args\[0\]为字符串"on"，因此获取的section解析器，这个解析器就是ActionParser
-   当section\_parser被赋值后，那就说明一个section要开始解析了
-   调用ParseSection解析关键字on所在的行

```
//system/core/init/action_parser.cpp

Result<Success> ActionParser::ParseSection(std::vector<std::string>&& args,
                                           const std::string& filename, int line) {
    std::string event_trigger;
    std::map<std::string, std::string> property_triggers;

    if (auto result = ParseTriggers(triggers, action_subcontext, &event_trigger, &property_triggers);
        !result) {
        return Error() << "ParseTriggers() failed: " << result.error();
    }

    auto action = std::make_unique<Action>(false, action_subcontext, filename, line, event_trigger,
                                           property_triggers);

    action_ = std::move(action);
    return Success();
}
```

action的定义如下

```
on <trigger> [&& <trigger>]*
    <command>
    <command>
    <command>
```

-   trigger 作为触发器，trigger 有两种， event 和 proptery。

显然上面的代码将字符串中的event 和 proptery两种trigger解析到event\_trigger和property\_triggers变量中，然后创建一个Action对象，最后赋值给action\_。

接下来继续解析command，因为section\_parser变量已被赋值，则会调用如下代码进行command解析

```
if (section_parser) {
    if (auto result = section_parser->ParseLineSection(std::move(args), state.line);
        !result) {
        parse_error_count_++;
        LOG(ERROR) << filename << ": " << state.line << ": " << result.error();
    }
}


//system/core/init/action_parser.cpp
Result<Success> ActionParser::ParseLineSection(std::vector<std::string>&& args, int line) {
    return action_ ? action_->AddCommand(std::move(args), line) : Success();
}
```

ParseLineSection最终调用Action类的AddCommand

```
Result<Success> Action::AddCommand(std::vector<std::string>&& args, int line) {
    if (!function_map_) {
        return Error() << "no function map available";
    }

    auto function = function_map_->FindFunction(args);
    if (!function) return Error() << function.error();

    commands_.emplace_back(function->second, function->first, std::move(args), line);
    return Success();
}
```

FindFunction前面已经讲过是怎么来的了，其中调用了map方法，map方法实现如下

```
//system/core/init/builtins.cpp

const BuiltinFunctionMap::Map& BuiltinFunctionMap::map() const {
    constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
    // clang-format off
    static const Map builtin_functions = {
        {"bootchart",               {1,     1,    {false,  do_bootchart}}},
        {"chmod",                   {2,     2,    {true,   do_chmod}}},
        {"chown",                   {2,     3,    {true,   do_chown}}},
        {"class_reset",             {1,     1,    {false,  do_class_reset}}},
        {"class_reset_post_data",   {1,     1,    {false,  do_class_reset_post_data}}},
        {"class_restart",           {1,     1,    {false,  do_class_restart}}},
        {"class_start",             {1,     1,    {false,  do_class_start}}},
        {"class_start_post_data",   {1,     1,    {false,  do_class_start_post_data}}},
        {"class_stop",              {1,     1,    {false,  do_class_stop}}},
        {"copy",                    {2,     2,    {true,   do_copy}}},
        {"domainname",              {1,     1,    {true,   do_domainname}}},
        {"enable",                  {1,     1,    {false,  do_enable}}},
        {"exec",                    {1,     kMax, {false,  do_exec}}},
        {"exec_background",         {1,     kMax, {false,  do_exec_background}}},
        {"exec_start",              {1,     1,    {false,  do_exec_start}}},
        {"export",                  {2,     2,    {false,  do_export}}},
        {"hostname",                {1,     1,    {true,   do_hostname}}},
        {"ifup",                    {1,     1,    {true,   do_ifup}}},
        {"init_user0",              {0,     0,    {false,  do_init_user0}}},
        {"insmod",                  {1,     kMax, {true,   do_insmod}}},
        {"installkey",              {1,     1,    {false,  do_installkey}}},
        {"interface_restart",       {1,     1,    {false,  do_interface_restart}}},
        {"interface_start",         {1,     1,    {false,  do_interface_start}}},
        {"interface_stop",          {1,     1,    {false,  do_interface_stop}}},
        {"load_persist_props",      {0,     0,    {false,  do_load_persist_props}}},
        {"load_system_props",       {0,     0,    {false,  do_load_system_props}}},
        {"loglevel",                {1,     1,    {false,  do_loglevel}}},
        {"mark_post_data",          {0,     0,    {false,  do_mark_post_data}}},
        {"mkdir",                   {1,     4,    {true,   do_mkdir}}},
        // TODO: Do mount operations in vendor_init.
        // mount_all is currently too complex to run in vendor_init as it queues action triggers,
        // imports rc scripts, etc.  It should be simplified and run in vendor_init context.
        // mount and umount are run in the same context as mount_all for symmetry.
        {"mount_all",               {1,     kMax, {false,  do_mount_all}}},
        {"mount",                   {3,     kMax, {false,  do_mount}}},
        {"parse_apex_configs",      {0,     0,    {false,  do_parse_apex_configs}}},
        {"umount",                  {1,     1,    {false,  do_umount}}},
        {"umount_all",              {1,     1,    {false,  do_umount_all}}},
        {"readahead",               {1,     2,    {true,   do_readahead}}},
        {"restart",                 {1,     1,    {false,  do_restart}}},
        {"restorecon",              {1,     kMax, {true,   do_restorecon}}},
        {"restorecon_recursive",    {1,     kMax, {true,   do_restorecon_recursive}}},
        {"rm",                      {1,     1,    {true,   do_rm}}},
        {"rmdir",                   {1,     1,    {true,   do_rmdir}}},
        {"setprop",                 {2,     2,    {true,   do_setprop}}},
        {"setrlimit",               {3,     3,    {false,  do_setrlimit}}},
        {"start",                   {1,     1,    {false,  do_start}}},
        {"stop",                    {1,     1,    {false,  do_stop}}},
        {"swapon_all",              {1,     1,    {false,  do_swapon_all}}},
        {"enter_default_mount_ns",  {0,     0,    {false,  do_enter_default_mount_ns}}},
        {"symlink",                 {2,     2,    {true,   do_symlink}}},
        {"sysclktz",                {1,     1,    {false,  do_sysclktz}}},
        {"trigger",                 {1,     1,    {false,  do_trigger}}},
        {"verity_load_state",       {0,     0,    {false,  do_verity_load_state}}},
        {"verity_update_state",     {0,     0,    {false,  do_verity_update_state}}},
        {"wait",                    {1,     2,    {true,   do_wait}}},
        {"wait_for_prop",           {2,     2,    {false,  do_wait_for_prop}}},
        {"write",                   {2,     2,    {true,   do_write}}},
    };
    // clang-format on
    return builtin_functions;
}
```

Action中所有commond命令都有其对应实现。

```
const Result<Function> FindFunction(const std::vector<std::string>& args) const {
        using android::base::StringPrintf;

        if (args.empty()) return Error() << "Keyword needed, but not provided";

        auto& keyword = args[0];
        auto num_args = args.size() - 1;

        auto function_info_it = map().find(keyword);
        if (function_info_it == map().end()) {
            return Error() << StringPrintf("Invalid keyword '%s'", keyword.c_str());
        }

        auto function_info = function_info_it->second;

        auto min_args = std::get<0>(function_info);
        auto max_args = std::get<1>(function_info);
        if (min_args == max_args && num_args != min_args) {
            return Error() << StringPrintf("%s requires %zu argument%s", keyword.c_str(), min_args,
                                           (min_args > 1 || min_args == 0) ? "s" : "");
        }

        if (num_args < min_args || num_args > max_args) {
            if (max_args == std::numeric_limits<decltype(max_args)>::max()) {
                return Error() << StringPrintf("%s requires at least %zu argument%s",
                                               keyword.c_str(), min_args, min_args > 1 ? "s" : "");
            } else {
                return Error() << StringPrintf("%s requires between %zu and %zu arguments",
                                               keyword.c_str(), min_args, max_args);
            }
        }

        return std::get<Function>(function_info);
    }
```

-   通过keyword，调用map找到对应的commond处理对象，也就是如下内容

```
{1,     1,    {false,  do_start}}
```

-   然后校验其参数个数是否合法
-   最后返回std::get(function\_info)，实际上是如下内容

```
{false,  do_start}
```

这样就找到了每个commond对应实现的方法是哪个，最后解析出来数据会构建出一个commond对象，这个对象被保存到commands\_变量中。

```
commands_.emplace_back(function->second, function->first, std::move(args), line);
```

除了commond关键字的解析外，commond命令可能还会有参数，这些参数就是通过如下代码保存到args中的。

```
case T_TEXT:
    args.emplace_back(state.text);
    break;
```

最后，当section全部解析完成后，会调用如下代码结束section的解析

```
case T_EOF:
    end_section();

    for (const auto& [section_name, section_parser] : section_parsers_) {
        section_parser->EndFile();
    }

    return;
```

end\_section最后调用对应解析器的ActionParser类的EndSection

```
Result<Success> ActionParser::EndSection() {
    if (action_ && action_->NumCommands() > 0) {
        action_manager_->AddAction(std::move(action_));
    }

    return Success();
}
```

将action\_添加到action\_manager\_的actions\_集合对象中进行管理。

**最后，所有rc文件中的Action都会按照如下数据结构方式进行管理：**

![ActionManager.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/cd04ef4510cd433486dc7087d77ed5c3~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743251493&x-signature=k2mqzMd3zorSvEvuwmeOwRlihcY%3D)

## Service解析

Service与Action解析根本的不同点就在于如下代码

```
if (section_parsers_.count(args[0])) {
    end_section();
    section_parser = section_parsers_[args[0]].get();
    section_start_line = state.line;
    if (auto result =
            section_parser->ParseSection(std::move(args), filename, state.line);
        !result) {
        parse_error_count_++;
        LOG(ERROR) << filename << ": " << state.line << ": " << result.error();
        section_parser = nullptr;
        bad_section_found = true;
    }
}
```

对于Action，section\_parser为ActionParser;对于service，section\_parser为ServiceParser；这也就导致后续调用的三个核心函数ParseSection、ParseLineSection、EndSection实现代码有所不同。

以如下service解析为例

```
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

```
//system/core/init/service.cpp
    
Result<Success> ServiceParser::ParseSection(std::vector<std::string>&& args,
                                            const std::string& filename, int line) {
    if (args.size() < 3) {
        return Error() << "services must have a name and a program";
    }

    const std::string& name = args[1];
    if (!IsValidName(name)) {
        return Error() << "invalid service name '" << name << "'";
    }

    filename_ = filename;

    ...

    std::vector<std::string> str_args(args.begin() + 2, args.end());


    service_ = std::make_unique<Service>(name, restart_action_subcontext, str_args);
    return Success();
}
```

-   获取服务名name=“zygote”
-   然后将后续参数放到str\_args中(/system/bin/app\_process64 -Xzygote /system/bin --zygote --start-system-server)
-   最后使用name和str\_args创建Service，并赋值给service\_

```
//system/core/init/service.cpp
    
Result<Success> ServiceParser::ParseLineSection(std::vector<std::string>&& args, int line) {
    return service_ ? service_->ParseLine(std::move(args)) : Success();
}
    
Result<Success> Service::ParseLine(std::vector<std::string>&& args) {
    static const OptionParserMap parser_map;
    auto parser = parser_map.FindFunction(args);

    if (!parser) return parser.error();

    return std::invoke(*parser, this, std::move(args));
}
```

ParseLineSection最终是调用Service中的ParseLine：

-   OptionParserMap在前面有提到，它继承至KeywordMap
-   parser\_map.FindFunction实际就是调用的KeywordMap.FindFunction
-   KeywordMap.FindFunction中会调用map()方法，这个方法被OptionParserMap所覆盖，实现如下：

```
const Service::OptionParserMap::Map& Service::OptionParserMap::map() const {
    constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
    // clang-format off
    static const Map option_parsers = {
        {"capabilities",
                        {0,     kMax, &Service::ParseCapabilities}},
        {"class",       {1,     kMax, &Service::ParseClass}},
        {"console",     {0,     1,    &Service::ParseConsole}},
        {"critical",    {0,     0,    &Service::ParseCritical}},
        {"disabled",    {0,     0,    &Service::ParseDisabled}},
        {"enter_namespace",
                        {2,     2,    &Service::ParseEnterNamespace}},
        {"file",        {2,     2,    &Service::ParseFile}},
        {"group",       {1,     NR_SVC_SUPP_GIDS + 1, &Service::ParseGroup}},
        {"interface",   {2,     2,    &Service::ParseInterface}},
        {"ioprio",      {2,     2,    &Service::ParseIoprio}},
        {"keycodes",    {1,     kMax, &Service::ParseKeycodes}},
        {"memcg.limit_in_bytes",
                        {1,     1,    &Service::ParseMemcgLimitInBytes}},
        {"memcg.limit_percent",
                        {1,     1,    &Service::ParseMemcgLimitPercent}},
        {"memcg.limit_property",
                        {1,     1,    &Service::ParseMemcgLimitProperty}},
        {"memcg.soft_limit_in_bytes",
                        {1,     1,    &Service::ParseMemcgSoftLimitInBytes}},
        {"memcg.swappiness",
                        {1,     1,    &Service::ParseMemcgSwappiness}},
        {"namespace",   {1,     2,    &Service::ParseNamespace}},
        {"oneshot",     {0,     0,    &Service::ParseOneshot}},
        {"onrestart",   {1,     kMax, &Service::ParseOnrestart}},
        {"oom_score_adjust",
                        {1,     1,    &Service::ParseOomScoreAdjust}},
        {"override",    {0,     0,    &Service::ParseOverride}},
        {"priority",    {1,     1,    &Service::ParsePriority}},
        {"restart_period",
                        {1,     1,    &Service::ParseRestartPeriod}},
        {"rlimit",      {3,     3,    &Service::ParseProcessRlimit}},
        {"seclabel",    {1,     1,    &Service::ParseSeclabel}},
        {"setenv",      {2,     2,    &Service::ParseSetenv}},
        {"shutdown",    {1,     1,    &Service::ParseShutdown}},
        {"sigstop",     {0,     0,    &Service::ParseSigstop}},
        {"socket",      {3,     6,    &Service::ParseSocket}},
        {"timeout_period",
                        {1,     1,    &Service::ParseTimeoutPeriod}},
        {"updatable",   {0,     0,    &Service::ParseUpdatable}},
        {"user",        {1,     1,    &Service::ParseUser}},
        {"writepid",    {1,     kMax, &Service::ParseWritepid}},
    };
    // clang-format on
    return option_parsers;
}
```

FindFunction实际返回的就是&Service::\*，这里Service中所实现的方法主要是用来解析rc脚本中Service的Option选项，例如：

```
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

-   std::invoke(\*parser, this, std::move(args))就是调用&Service::\*方法，在这里方法中会将Option中解析的数据填充到前面创建的\_service对象中

```
Result<Success> ServiceParser::EndSection() {
    if (service_) {
        Service* old_service = service_list_->FindService(service_->name());
        if (old_service) {
            if (!service_->is_override()) {
                return Error() << "ignored duplicate definition of service '" << service_->name()
                               << "'";
            }

            if (StartsWith(filename_, "/apex/") && !old_service->is_updatable()) {
                return Error() << "cannot update a non-updatable service '" << service_->name()
                               << "' with a config in APEX";
            }

            service_list_->RemoveService(*old_service);
            old_service = nullptr;
        }

        service_list_->AddService(std::move(service_));
    }

    return Success();
}
```

最后当一个Service section解析完后就被添加到service\_list\_中管理，整个过程构建的数据结构如下：

![ServiceList.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/6d6e1b8069684320b55aa05469dea9ce~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743251493&x-signature=edSP6SYUOjlU11QCEnJBQdojzAk%3D)

## line\_callback

line\_callback这个不是一个重要的内容，在这里简单提一下即可，它并不是init进程在使用，而是ueventd进程使用的；在前面init进程执行流程中讲解了ueventd进程是如何启动的，启动入口ueventd\_main如下

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

在ueventd\_main会解析与ueventd相关的rc文件

```
int ueventd_main(int argc, char** argv) {
    ...

    auto ueventd_configuration = ParseConfig({"/ueventd.rc", "/vendor/ueventd.rc",
                                              "/odm/ueventd.rc", "/ueventd." + hardware + ".rc"});

    ...
    return 0;
}
```

在这里面会调用AddSingleLineParser添加一个行解析器，前面讲解init.rc都是调用AddSectionParser添加的段解析器

```
UeventdConfiguration ParseConfig(const std::vector<std::string>& configs) {
    Parser parser;
    UeventdConfiguration ueventd_configuration;

    parser.调用AddSectionParser添加的段解析器("subsystem",
                            std::make_unique<SubsystemParser>(&ueventd_configuration.subsystems));

    using namespace std::placeholders;
    parser.AddSingleLineParser(
            "/sys/",
            std::bind(ParsePermissionsLine, _1, &ueventd_configuration.sysfs_permissions, nullptr));
    parser.AddSingleLineParser("/dev/", std::bind(ParsePermissionsLine, _1, nullptr,
                                                  &ueventd_configuration.dev_permissions));
    parser.AddSingleLineParser("firmware_directories",
                               std::bind(ParseFirmwareDirectoriesLine, _1,
                                         &ueventd_configuration.firmware_directories));
    parser.AddSingleLineParser("modalias_handling",
                               std::bind(ParseModaliasHandlingLine, _1,
                                         &ueventd_configuration.enable_modalias_handling));
    parser.AddSingleLineParser("uevent_socket_rcvbuf_size",
                               std::bind(ParseUeventSocketRcvbufSizeLine, _1,
                                         &ueventd_configuration.uevent_socket_rcvbuf_size));

    for (const auto& config : configs) {
        parser.ParseConfig(config);
    }

    return ueventd_configuration;
}
```

AddSingleLineParser就是将解析器添加到了line\_callbacks\_中

```
void Parser::AddSingleLineParser(const std::string& prefix, LineCallback callback) {
    line_callbacks_.emplace_back(prefix, callback);
}
```

到这里，我们就明白了为什么前面分析init.rc解析时，直接忽略line\_callbacks\_了。

为什么需要这个行解析器呢，我们看一下ueventd.rc文件内容就明白了

```
firmware_directories /etc/firmware/ /odm/firmware/ /vendor/firmware/ /firmware/image/
uevent_socket_rcvbuf_size 16M

subsystem graphics
    devname uevent_devpath
    dirname /dev/graphics

subsystem drm
    devname uevent_devpath
    dirname /dev/dri

subsystem input
    devname uevent_devpath
    dirname /dev/input

subsystem sound
    devname uevent_devpath
    dirname /dev/snd

# ueventd can only set permissions on device nodes and their associated
# sysfs attributes, not on arbitrary paths.
#
# format for /dev rules: devname mode uid gid
# format for /sys rules: nodename attr mode uid gid
# shortcut: "mtd@NN" expands to "/dev/mtd/mtdNN"

/dev/null                 0666   root       root
/dev/zero                 0666   root       root
/dev/full                 0666   root       root
/dev/ptmx                 0666   root       root
/dev/tty                  0666   root       root
/dev/random               0666   root       root
/dev/urandom              0666   root       root
# Make HW RNG readable by group system to let EntropyMixer read it.
/dev/hw_random            0440   root       system
/dev/ashmem               0666   root       root
/dev/binder               0666   root       root
/dev/hwbinder             0666   root       root
/dev/vndbinder            0666   root       root

/dev/pmsg0                0222   root       log

# kms driver for drm based gpu
/dev/dri/*                0666   root       graphics

# these should not be world writable
/dev/uhid                 0660   uhid       uhid
/dev/uinput               0660   uhid       uhid
/dev/rtc0                 0640   system     system
/dev/tty0                 0660   root       system
/dev/graphics/*           0660   root       graphics
/dev/input/*              0660   root       input
/dev/v4l-touch*           0660   root       input
/dev/snd/*                0660   system     audio
/dev/bus/usb/*            0660   root       usb
/dev/mtp_usb              0660   root       mtp
/dev/usb_accessory        0660   root       usb
/dev/tun                  0660   system     vpn

# CDMA radio interface MUX
/dev/ppp                  0660   radio      vpn

# sysfs properties
/sys/devices/platform/trusty.*      trusty_version        0440  root   log
/sys/devices/virtual/input/input*   enable      0660  root   input
/sys/devices/virtual/input/input*   poll_delay  0660  root   input
/sys/devices/virtual/usb_composite/*   enable      0664  root   system
/sys/devices/system/cpu/cpu*   cpufreq/scaling_max_freq   0664  system system
/sys/devices/system/cpu/cpu*   cpufreq/scaling_min_freq   0664  system system
```

因为它里面有很多独立的行，所以定义了一个专门的解析器用于解析这些独立行。