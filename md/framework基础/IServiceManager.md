我来为您详细解析 Android 中的 IServiceManager，这是 Binder 机制的核心组件，负责系统服务的注册与查询。
  我来为您详细解析 Android 中的 IServiceManager，这是 Binder 机制的核心组件，负责系统服务的注册与查询。

---

## 一、IServiceManager 架构定位

### 1.1 角色定义

IServiceManager 是 **Binder 机制的"服务大管家"**，运行在独立的 `servicemanager` 进程中，负责：

| 功能 | 说明 |
|------|------|
| **服务注册 (addService)** | 系统服务启动时向 SM 注册自己的名字和 Binder 对象 |
| **服务查询 (getService)** | 客户端通过名字获取服务的 Binder 代理 |
| **服务管理 (listServices)** | 列出所有已注册的服务 |
| **死亡通知 (linkToDeath)** | 监控服务进程死亡，自动清理注册表  |

### 1.2 进程架构

```
用户空间
├── servicemanager 进程 (system/bin/servicemanager)
│   └── ServiceManager (Native C++)
│       ├── mNameToService: Map<name, IBinder>  // 服务注册表
│       └── Looper: epoll 监听 /dev/binder
│
├── system_server 进程
│   ├── AMS, ATMS, WMS... (Java 服务)
│   └── 通过 IServiceManager.addService() 注册
│
└── 普通应用进程
    └── 通过 IServiceManager.getService() 获取服务代理

内核空间
└── Binder 驱动 (/dev/binder)
    └── 处理进程间通信，维护 Binder 引用计数
```

---

## 二、核心接口定义

### 2.1 AIDL 接口 (Android 10+) 

```java
// frameworks/native/libs/binder/aidl/android/os/IServiceManager.aidl
interface IServiceManager {
    // 注册服务
    void addService(@utf8InCpp String name, IBinder service,
                    boolean allowIsolated, int dumpPriority);
    
    // 查询服务（阻塞式）
    @nullable IBinder getService(@utf8InCpp String name);
    
    // 检查服务（非阻塞）
    @nullable IBinder checkService(@utf8InCpp String name);
    
    // 列出所有服务
    String[] listServices(int dumpPriority);
    
    // 注册死亡通知
    void registerForNotifications(@utf8InCpp String name, IServiceCallback callback);
    
    // 注销死亡通知
    void unregisterForNotifications(@utf8InCpp String name, IServiceCallback callback);
}
```

### 2.2 实现类层次

```cpp
// Native 层实现 (frameworks/native/libs/binder/IServiceManager.cpp)
class ServiceManagerShim : public IServiceManager {
    sp<AidlServiceManager> mTheRealServiceManager;  // AIDL 代理
    
    status_t addService(...) override {
        // 转发到 AIDL 接口
        return mTheRealServiceManager->addService(...);
    }
    
    sp<IBinder> getService(...) override {
        return checkService(name);  // 内部调用 checkService
    }
};

// Java 层代理 (frameworks/base/core/java/android/os/ServiceManagerNative.java)
class ServiceManagerProxy implements IServiceManager {
    private IBinder mRemote;  // 指向 Native ServiceManager 的 Binder 代理
    private IServiceManager mServiceManager;  // AIDL Stub
    
    public void addService(String name, IBinder service, ...) {
        // 打包参数，通过 Binder 驱动发送给 servicemanager
        mServiceManager.addService(name, service, allowIsolated, dumpPriority);
    }
}
```

---

## 三、服务注册流程 (addService)

### 3.1 调用链（以 AMS 注册为例）

```java
// 1. AMS 注册入口
public class ActivityManagerService {
    public void setSystemProcess() {
        ServiceManager.addService(Context.ACTIVITY_SERVICE, this, 
                                  true, DUMP_FLAG_PRIORITY_CRITICAL);
    }
}

// 2. ServiceManager Java 封装
public final class ServiceManager {
    private static IServiceManager sServiceManager;
    
    public static void addService(String name, IBinder service, 
                                  boolean allowIsolated, int dumpPriority) {
        try {
            // 获取 IServiceManager 代理，调用 addService
            getIServiceManager().addService(name, service, allowIsolated, dumpPriority);
        } catch (RemoteException e) {
            Log.e(TAG, "error in addService", e);
        }
    }
    
    private static IServiceManager getIServiceManager() {
        if (sServiceManager != null) return sServiceManager;
        
        // 关键：获取 ServiceManager 的 Binder 代理
        sServiceManager = ServiceManagerNative.asInterface(
            Binder.allowBlocking(BinderInternal.getContextObject()));
        return sServiceManager;
    }
}

// 3. 获取 ContextObject（Native 调用）
public class BinderInternal {
    @UnsupportedAppUsage
    public static final native IBinder getContextObject();
    // 对应 JNI: android_os_BinderInternal_getContextObject()
}

// 4. JNI 实现 (android_util_Binder.cpp)
static jobject android_os_BinderInternal_getContextObject(JNIEnv* env, jobject clazz) {
    // 获取 ProcessState 单例，打开 /dev/binder
    sp<ProcessState> proc = ProcessState::self();
    
    // 获取 handle 为 0 的 Binder（即 ServiceManager）
    sp<IBinder> b = proc->getContextObject(NULL);
    
    // 转换为 Java 层的 BinderProxy 对象
    return javaObjectForIBinder(env, b);
}
```

### 3.2 Native 层处理（servicemanager）

```cpp
// frameworks/native/cmds/servicemanager/ServiceManager.cpp
class ServiceManager : public os::BnServiceManager {
    std::map<std::string, sp<IBinder>> mNameToService;  // 服务注册表
    std::map<std::string, sp<IBinder>> mNameToDeathRecipient;  // 死亡通知
    
    // 处理 addService 请求
    Status addService(const std::string& name, 
                      const sp<IBinder>& binder,
                      bool allowIsolated,
                      int32_t dumpPriority) override {
        // 1. 权限检查（SELinux）
        if (!mAccess->canAdd(name)) {
            return Status::fromExceptionCode(Status::EX_SECURITY);
        }
        
        // 2. 检查服务是否已存在
        auto it = mNameToService.find(name);
        if (it != mNameToService.end()) {
            // 服务已存在，替换（或返回错误）
        }
        
        // 3. 注册死亡通知（服务进程崩溃时自动清理）
        sp<DeathRecipient> deathRecipient = new ServiceDeathRecipient(name);
        binder->linkToDeath(deathRecipient);
        mNameToDeathRecipient[name] = deathRecipient;
        
        // 4. 存入注册表
        mNameToService[name] = binder;
        
        return Status::ok();
    }
};
```

---

## 四、服务查询流程 (getService)

### 4.1 客户端调用链 

```java
// 1. 应用获取 WindowManager 服务
WindowManager wm = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
// 底层调用:
// ServiceManager.getService(Context.WINDOW_SERVICE);

// 2. ServiceManager.getService 实现
public static IBinder getService(String name) {
    try {
        // 先查缓存（Java 层缓存）
        IBinder service = sCache.get(name);
        if (service != null) return service;
        
        // 远程调用 ServiceManager
        return Binder.allowBlocking(rawGetService(name));
    } catch (RemoteException e) {
        Log.e(TAG, "error in getService", e);
    }
    return null;
}

private static IBinder rawGetService(String name) throws RemoteException {
    // 通过 IServiceManager 查询
    final IBinder binder = getIServiceManager().getService(name);
    return binder;
}

// 3. ServiceManagerProxy.getService -> 最终调用 checkService
public IBinder getService(String name) throws RemoteException {
    return mServiceManager.checkService(name);  // AIDL 调用
}
```

### 4.2 Native 层查询处理 

```cpp
// ServiceManager.cpp
Status ServiceManager::checkService(const std::string& name, sp<IBinder>* outBinder) {
    // 1. 权限检查
    if (!mAccess->canFind(name)) {
        return Status::fromExceptionCode(Status::EX_SECURITY);
    }
    
    // 2. 查找服务
    auto it = mNameToService.find(name);
    if (it == mNameToService.end()) {
        *outBinder = nullptr;  // 服务未找到
        return Status::ok();
    }
    
    // 3. 返回 Binder 引用（驱动处理引用计数）
    *outBinder = it->second;
    return Status::ok();
}
```

### 4.3 服务引用转换流程

```cpp
// 从 ServiceManager 获取的是 Binder 代理（BpBinder）
// 需要转换为 Java 层的 BinderProxy

// android_util_Binder.cpp
jobject javaObjectForIBinder(JNIEnv* env, const sp<IBinder>& val) {
    if (val == nullptr) return nullptr;
    
    // 检查是否已有对应的 Java 对象（缓存）
    BinderProxyNativeData* nativeData = gNativeDataCache.get(val);
    if (nativeData != nullptr) {
        return env->NewLocalRef(nativeData->mObject);
    }
    
    // 创建新的 BinderProxy
    jobject object = env->CallStaticObjectMethod(
        gBinderProxyClass, gBinderProxyGetInstanceMethod,
        reinterpret_cast<jlong>(nativeData),  // Native 数据指针
        reinterpret_cast<jlong>(val.get())  // Binder 地址
    );
    
    // 建立 Java 对象与 Native Binder 的映射
    nativeData->mObject = env->NewGlobalRef(object);
    gNativeDataCache.insert(val, nativeData);
    
    return object;
}
```

---

## 五、ServiceManager 启动流程 

### 5.1 启动脚本

```bash
# frameworks/native/cmds/servicemanager/servicemanager.rc
service servicemanager /system/bin/servicemanager
    class core animation    # 核心服务，优先启动
    user system
    group system readproc
    critical                # 关键服务，崩溃则系统重启
    onrestart restart apexd # 重启时联动重启其他服务
    onrestart restart audioserver
    onrestart class_restart main
```

### 5.2 Native 主函数

```cpp
// frameworks/native/cmds/servicemanager/main.cpp
int main(int argc, char** argv) {
    // 1. 打开 Binder 驱动
    const char* driver = argc == 2 ? argv[1] : "/dev/binder";
    sp<ProcessState> ps = ProcessState::initWithDriver(driver);
    
    // 2. 设置线程池（ServiceManager 单线程即可）
    ps->setThreadPoolMaxThreadCount(0);
    
    // 3. 创建 ServiceManager 对象
    sp<ServiceManager> manager = sp<ServiceManager>::make(std::make_unique<Access>());
    
    // 4. 【关键】注册自己为 "manager" 服务
    if (!manager->addService("manager", manager, false, 
                             IServiceManager::DUMP_FLAG_PRIORITY_DEFAULT).isOk()) {
        LOG(ERROR) << "Could not self register servicemanager";
    }
    
    // 5. 设置上下文对象（处理 Binder 事务）
    IPCThreadState::self()->setTheContextObject(manager);
    
    // 6. 【关键】成为 Binder 上下文管理者（handle 0）
    ps->becomeContextManager();  // ioctl(BINDER_SET_CONTEXT_MGR)
    
    // 7. 准备 Looper，监听 Binder FD
    sp<Looper> looper = Looper::prepare(false);
    BinderCallback::setupTo(looper);  // 注册 epoll 回调
    
    // 8. 进入事件循环
    while (true) {
        looper->pollAll(-1);  // 阻塞等待 Binder 事务
    }
    
    return EXIT_FAILURE;
}
```

### 5.3 becomeContextManager 实现

```cpp
// frameworks/native/libs/binder/ProcessState.cpp
bool ProcessState::becomeContextManager() {
    AutoMutex _l(mLock);
    
    flat_binder_object obj {
        .flags = FLAT_BINDER_FLAG_TXN_SECURITY_CTX,
    };
    
    // 告诉 Binder 驱动：我是全局的 ServiceManager
    int result = ioctl(mDriverFD, BINDER_SET_CONTEXT_MGR_EXT, &obj);
    
    if (result != 0) {
        // 降级到旧版本命令
        int unused = 0;
        result = ioctl(mDriverFD, BINDER_SET_CONTEXT_MGR, &unused);
    }
    
    return result == 0;
}
```

---

## 六、Binder 事务处理循环

### 6.1 事件处理流程 

```cpp
// BinderCallback 处理 Binder 驱动事件
class BinderCallback : public LooperCallback {
public:
    int handleEvent(int fd, int events, void* data) override {
        // 读取并处理 Binder 事务
        IPCThreadState::self()->handlePolledCommands();
        return 1;  // 继续监听
    }
    
    static void setupTo(const sp<Looper>& looper) {
        // 将 Binder FD 加入 epoll
        int binderFd = -1;
        IPCThreadState::self()->setupPolling(&binderFd);
        looper->addFd(binderFd, Looper::POLL_CALLBACK, 
                      Looper::EVENT_INPUT, 
                      sp<BinderCallback>::make(), nullptr);
    }
};

// IPCThreadState 处理命令
void IPCThreadState::handlePolledCommands() {
    while (mIn.dataPosition() < mIn.dataSize()) {
        // 读取 BR 命令（Binder Return）
        int32_t cmd = mIn.readInt32();
        
        switch (cmd) {
            case BR_TRANSACTION: {
                // 处理事务请求（addService/getService 等）
                binder_transaction_data tr;
                mIn.read(&tr, sizeof(tr));
                
                // 分发到 ServiceManager 的 onTransact
                mContextObject->transact(tr.code, buffer, &reply, 0);
                break;
            }
            case BR_DEAD_BINDER: {
                // 处理服务死亡通知
                handleDeadBinder();
                break;
            }
            // ... 其他命令
        }
    }
}
```

---

## 七、调试与监控

```bash
# 查看 ServiceManager 状态
adb shell dumpsys servicemanager

# 列出所有注册的服务
adb shell service list

# 查看特定服务
adb shell service check activity  # 检查 AMS 是否存在

# 监控 Binder 调用
adb shell logcat -s "ServiceManager" -d

# 获取服务的 Binder 引用（调试用）
adb shell service call activity 1  # 调用 AMS 的 code 1
```

---

## 八、总结

| 维度 | 说明 |
|------|------|
| **核心作用** | Binder 机制的"DNS 服务器"，服务名到 Binder 对象的映射 |
| **进程模型** | 独立进程（system/bin/servicemanager），早于 Zygote 启动 |
| **数据结构** | `Map<String, IBinder>` + 死亡通知回调 |
| **通信机制** | 通过 Binder 驱动（/dev/binder），handle 0 固定为 SM |
| **安全机制** | SELinux 权限检查（canAdd/canFind） |
| **关键优化** | Java 层缓存（sCache）、Native 层 BinderProxy 缓存 |

**设计精髓**：ServiceManager 本身是一个普通的 Binder 服务，但通过 `BINDER_SET_CONTEXT_MGR` 成为特殊节点，使得所有进程都能通过 **handle 0** 找到它，进而找到其他所有系统服务，构成 Android 系统服务架构的基石 。







我来为您详细解析 Android Binder 机制中的 `DECLARE_META_INTERFACE` 和 `IMPLEMENT_META_INTERFACE` 宏，这是实现 C++ 层 Binder 接口的核心机制。

---

## 一、宏的定位与作用

这两个宏定义在 `frameworks/native/libs/binder/include/binder/IInterface.h` ，用于**自动化生成 Binder 接口的标准化代码**，解决以下问题：

| 问题 | 解决方案 |
|------|---------|
| 每个接口都需要重复定义描述符、asInterface 等样板代码 | 宏自动生成 |
| 需要统一接口标识（字符串描述符） | 宏自动声明/定义 `descriptor` 静态成员 |
| 需要将 `IBinder` 转换为具体的接口类型（BpXXX 或 BnXXX） | 宏自动生成 `asInterface` 工厂方法 |
| 需要支持同一进程内直接调用（无需代理） | 宏内嵌 `queryLocalInterface` 判断逻辑 |

---

## 二、DECLARE_META_INTERFACE 宏详解

### 2.1 宏定义展开 

```cpp
// frameworks/native/include/binder/IInterface.h
#define DECLARE_META_INTERFACE(INTERFACE)                               \
public:                                                                 \
    static const ::android::String16 descriptor;                        \
    static ::android::sp<I##INTERFACE> asInterface(                     \
            const ::android::sp<::android::IBinder>& obj);              \
    virtual const ::android::String16& getInterfaceDescriptor() const; \
    I##INTERFACE();                                                     \
    virtual ~I##INTERFACE();                                            \
    static bool setDefaultImpl(::android::sp<I##INTERFACE> impl);      \
    static const ::android::sp<I##INTERFACE>& getDefaultImpl();         \
```

### 2.2 以 IServiceManager 为例展开 

```cpp
// 原始代码（IServiceManager.h）
class IServiceManager : public IInterface {
public:
    DECLARE_META_INTERFACE(ServiceManager);  // 使用宏
    virtual status_t addService(...) = 0;
    virtual sp<IBinder> getService(...) const = 0;
    // ...
};

// 宏展开后（等效代码）
class IServiceManager : public IInterface {
public:
    // 1. 接口描述符（唯一标识）
    static const ::android::String16 descriptor;
    
    // 2. 工厂方法：将 IBinder 转换为 IServiceManager 智能指针
    static ::android::sp<IServiceManager> asInterface(
            const ::android::sp<::android::IBinder>& obj);
    
    // 3. 获取描述符（虚函数，运行时多态）
    virtual const ::android::String16& getInterfaceDescriptor() const;
    
    // 4. 构造与析构
    IServiceManager();
    virtual ~IServiceManager();
    
    // 5. 默认实现（用于测试或模拟）
    static bool setDefaultImpl(::android::sp<IServiceManager> impl);
    static const ::android::sp<IServiceManager>& getDefaultImpl();
    
    // 业务方法...
    virtual status_t addService(...) = 0;
    virtual sp<IBinder> getService(...) const = 0;
};
```

### 2.3 关键符号说明

| 符号 | 含义 | 示例 |
|------|------|------|
| `I##INTERFACE` | 字符串拼接 | `I` + `ServiceManager` = `IServiceManager` |
| `descriptor` | 接口唯一标识字符串 | `"android.os.IServiceManager"` |
| `asInterface` | 类型转换工厂方法 | 实现 `IBinder` → `IServiceManager` 的智能转换 |

---

## 三、IMPLEMENT_META_INTERFACE 宏详解

### 3.1 宏定义展开 

```cpp
// frameworks/native/include/binder/IInterface.h
#define IMPLEMENT_META_INTERFACE(INTERFACE, NAME)                       \
    DO_NOT_DIRECTLY_USE_ME_IMPLEMENT_META_INTERFACE(INTERFACE, NAME)

#define DO_NOT_DIRECTLY_USE_ME_IMPLEMENT_META_INTERFACE(INTERFACE, NAME) \
    const ::android::String16 I##INTERFACE::descriptor(NAME);         \
    const ::android::String16&                                        \
            I##INTERFACE::getInterfaceDescriptor() const {            \
        return I##INTERFACE::descriptor;                              \
    }                                                                 \
    ::android::sp<I##INTERFACE> I##INTERFACE::asInterface(              \
            const ::android::sp<::android::IBinder>& obj)             \
    {                                                                 \
        ::android::sp<I##INTERFACE> intr;                             \
        if (obj != nullptr) {                                         \
            intr = static_cast<I##INTERFACE*>(                        \
                obj->queryLocalInterface(                               \
                        I##INTERFACE::descriptor).get());             \
            if (intr == nullptr) {                                    \
                intr = new Bp##INTERFACE(obj);                        \
            }                                                         \
        }                                                             \
        return intr;                                                  \
    }                                                                 \
    I##INTERFACE::I##INTERFACE() { }                                  \
    I##INTERFACE::~I##INTERFACE() { }                                 \
    static bool setDefaultImpl(::android::sp<I##INTERFACE> impl) {    \
        // ... 实现省略                                               \
    }                                                                 \
    static const ::android::sp<I##INTERFACE>& getDefaultImpl() {      \
        // ... 实现省略                                               \
    }
```

### 3.2 以 IServiceManager 为例展开 

```cpp
// 原始代码（IServiceManager.cpp）
IMPLEMENT_META_INTERFACE(ServiceManager, "android.os.IServiceManager");

// 宏展开后（等效代码）
const ::android::String16 IServiceManager::descriptor("android.os.IServiceManager");

const ::android::String16& IServiceManager::getInterfaceDescriptor() const {
    return IServiceManager::descriptor;
}

::android::sp<IServiceManager> IServiceManager::asInterface(
        const ::android::sp<::android::IBinder>& obj) {
    ::android::sp<IServiceManager> intr;
    if (obj != nullptr) {
        // 【关键】尝试获取本地实现（同一进程内）
        intr = static_cast<IServiceManager*>(
            obj->queryLocalInterface(IServiceManager::descriptor).get());
        
        // 【关键】如果不是本地实现，创建代理对象
        if (intr == nullptr) {
            intr = new BpServiceManager(obj);  // 创建客户端代理
        }
    }
    return intr;
}

IServiceManager::IServiceManager() { }
IServiceManager::~IServiceManager() { }

// 默认实现方法（省略）
```

---

## 四、asInterface 的核心逻辑：智能类型转换

### 4.1 调用场景 

```cpp
// 典型调用：获取 ServiceManager 代理
gDefaultServiceManager = interface_cast<IServiceManager>(
    ProcessState::self()->getContextObject(nullptr)  // 返回 BpBinder(0)
);

// interface_cast 模板函数
template<typename INTERFACE>
inline sp<INTERFACE> interface_cast(const sp<IBinder>& obj) {
    return INTERFACE::asInterface(obj);  // 调用宏生成的 asInterface
}
```

### 4.2 转换逻辑流程图 

```
输入: IBinder 对象 (可能是 BBinder 或 BpBinder)
    │
    ▼
┌─────────────────────────────────────┐
│ 调用 asInterface(obj)               │
│ 1. 检查 obj 是否为 null             │
└─────────────────────────────────────┘
    │
    ▼ 非 null
┌─────────────────────────────────────┐
│ 2. 调用 obj->queryLocalInterface(   │
│    IServiceManager::descriptor)     │
│                                     │
│    • 如果是 BBinder（服务端本地）   │
│      → 返回 BnServiceManager 指针   │
│      → static_cast 成功，intr 非空  │
│                                     │
│    • 如果是 BpBinder（客户端代理）  │
│      → BpBinder 无本地实现，返回 NULL│
└─────────────────────────────────────┘
    │
    ├──┬─────────────────────────────┐
    │  │ 返回非空（同一进程）          │  返回 NULL（跨进程）
    │  ▼                              │  ▼
    │  intr = 本地实现                 │  intr = new BpServiceManager(obj)
    │  （直接调用，无 IPC 开销）        │  （创建代理，后续通过 IPC 通信）
    │                                  │
    └──────────────────────────────────┘
                              │
                              ▼
                    返回 sp<IServiceManager>
```

### 4.3 关键代码解析 

```cpp
// BnInterface 实现 queryLocalInterface（服务端）
template<typename INTERFACE>
class BnInterface : public INTERFACE, public BBinder {
public:
    virtual sp<IInterface> queryLocalInterface(const String16& _descriptor) {
        // 描述符匹配，返回自身（this）
        if (_descriptor == INTERFACE::descriptor) {
            return sp<IInterface>(static_cast<INTERFACE*>(this));
        }
        return nullptr;
    }
};

// BpRefBase 实现 queryLocalInterface（客户端）
class BpRefBase : public virtual RefBase {
public:
    // 默认返回 NULL，表示无本地实现
    virtual sp<IInterface> queryLocalInterface(const String16& /*_descriptor*/) {
        return nullptr;
    }
};
```

---

## 五、完整使用示例：自定义 Binder 服务

### 5.1 接口定义（.h 文件）

```cpp
// IMyService.h
#ifndef IMYSERVICE_H
#define IMYSERVICE_H

#include <binder/IInterface.h>
#include <binder/Parcel.h>

namespace android {

class IMyService : public IInterface {
public:
    // 【声明宏】自动生成标准 Binder 接口代码
    DECLARE_META_INTERFACE(MyService)
    
    // 业务方法
    virtual void sayHello() = 0;
    virtual int add(int a, int b) = 0;
};

// 命令码（用于 onTransact 分发）
enum {
    HELLO = IBinder::FIRST_CALL_TRANSACTION,
    ADD,
};

// 客户端代理声明
class BpMyService : public BpInterface<IMyService> {
public:
    explicit BpMyService(const sp<IBinder>& impl);
    virtual void sayHello();
    virtual int add(int a, int b);
};

// 服务端实现声明
class BnMyService : public BnInterface<IMyService> {
public:
    virtual status_t onTransact(uint32_t code, const Parcel& data,
                                Parcel* reply, uint32_t flags = 0);
    // 业务方法实现（子类需要实现）
};

} // namespace android

#endif // IMYSERVICE_H
```

### 5.2 接口实现（.cpp 文件）

```cpp
// IMyService.cpp
#include "IMyService.h"

namespace android {

// 【实现宏】自动生成 asInterface 等实现
IMPLEMENT_META_INTERFACE(MyService, "android.demo.IMyService");

// ========== 客户端代理实现 ==========
BpMyService::BpMyService(const sp<IBinder>& impl)
    : BpInterface<IMyService>(impl) {
}

void BpMyService::sayHello() {
    Parcel data, reply;
    // 写入接口标识（用于权限校验）
    data.writeInterfaceToken(IMyService::getInterfaceDescriptor());
    // 发送远程调用
    remote()->transact(HELLO, data, &reply);
}

int BpMyService::add(int a, int b) {
    Parcel data, reply;
    data.writeInterfaceToken(IMyService::getInterfaceDescriptor());
    data.writeInt32(a);
    data.writeInt32(b);
    remote()->transact(ADD, data, &reply);
    return reply.readInt32();
}

// ========== 服务端实现 ==========
status_t BnMyService::onTransact(uint32_t code, const Parcel& data,
                                 Parcel* reply, uint32_t flags) {
    // 校验接口标识
    CHECK_INTERFACE(IMyService, data, reply);
    
    switch (code) {
        case HELLO: {
            sayHello();  // 调用子类实现
            return NO_ERROR;
        }
        case ADD: {
            int a = data.readInt32();
            int b = data.readInt32();
            int result = add(a, b);  // 调用子类实现
            reply->writeInt32(result);
            return NO_ERROR;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

} // namespace android
```

### 5.3 服务端实现 

```cpp
// MyService.cpp（具体业务实现）
#include "IMyService.h"

namespace android {

class MyService : public BnMyService {
public:
    virtual void sayHello() {
        ALOGI("MyService::sayHello called");
    }
    
    virtual int add(int a, int b) {
        return a + b;
    }
};

} // namespace android
```

---

## 六、宏的演进：Android 新版本变化

### 6.1 Android 10+ 的变化 

```cpp
// 新版宏增加了默认实现支持（用于测试和模拟）
#define DECLARE_META_INTERFACE(INTERFACE)                               \
    // ... 原有声明 ...
    static bool setDefaultImpl(::android::sp<I##INTERFACE> impl);       \
    static const ::android::sp<I##INTERFACE>& getDefaultImpl();         \

// 对应的 IMPLEMENT 宏也增加了实现
#define DO_NOT_DIRECTLY_USE_ME_IMPLEMENT_META_INTERFACE(INTERFACE, NAME) \
    // ... 原有实现 ...
    static ::android::sp<I##INTERFACE> default_impl;                    \
    static bool setDefaultImpl(::android::sp<I##INTERFACE> impl) {      \
        default_impl = impl;                                            \
        return true;                                                    \
    }                                                                   \
    static const ::android::sp<I##INTERFACE>& getDefaultImpl() {        \
        return default_impl;                                            \
    }
```

---

## 七、总结

| 宏 | 作用 | 生成内容 |
|----|------|---------|
| `DECLARE_META_INTERFACE(INTERFACE)` | 在头文件中**声明**标准化接口 | `descriptor`、`asInterface`、`getInterfaceDescriptor`、构造/析构函数 |
| `IMPLEMENT_META_INTERFACE(INTERFACE, NAME)` | 在 cpp 文件中**实现**标准化接口 | `descriptor` 初始化、`asInterface` 智能转换逻辑、函数空实现 |

**核心设计思想**：
1. **模板化代码生成**：避免每个 Binder 接口重复编写样板代码
2. **智能类型识别**：通过 `queryLocalInterface` 自动判断同一进程（直接调用）或跨进程（创建代理）
3. **安全类型转换**：`static_cast` 确保运行时类型安全
4. **接口标识**：`descriptor` 字符串作为 Binder 接口的"身份证"，用于校验和查找 
