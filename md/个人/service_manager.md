在 Android 系统中，**`ServiceManager`** 是 Binder 机制的核心组件，负责管理系统中的 Binder 服务（如 `ActivityManagerService`、`WindowManagerService` 等）。它的启动过程涉及 **用户空间** 和 **内核空间** 的交互，是 Android 系统启动流程中的关键环节。以下是其启动流程的详细解析：

### **一、ServiceManager 的本质与作用**

- **本质**：
    - 是一个运行在独立进程中的 **C/S 架构服务**，基于 Binder 机制实现。
    - 自身是 Binder 机制中的 “**上下文管理者**”（Context Manager），持有系统中所有 Binder 服务的引用。
- **作用**：
    - 提供 **服务注册**（`addService`）、**查询**（`getService`）、**检查服务是否存在**（`checkService`）等功能。
    - 是客户端获取服务端 Binder 代理对象（`BpBinder`）的唯一入口。

### **二、ServiceManager 的启动流程**

#### **1. 系统启动阶段（Zygote 之前）**

`ServiceManager` 的启动早于 Android 的 Java 层服务（如 `SystemServer`），属于 **Native 层启动流程**，主要步骤如下：

##### **(1) 从 init 进程启动**

- **配置文件**：  
    Android 系统通过 init 进程的配置文件（通常为 `/init.rc` 或 `/init.<device>.rc`）启动 `servicemanager` 进程。
    
    rc
    
    ```rc
    service servicemanager /system/bin/servicemanager
        class core
        user system
        group system readproc
        critical
        onrestart restart healthd
        onrestart restart zygote
        onrestart restart media
        onrestart restart netd
    ```
    
      
    - `class core`：属于核心服务，确保优先启动。
    - `critical`：标记为关键服务，若崩溃则重启系统。

##### **(2) 启动 servicemanager 可执行文件**

- **入口函数**：  
    `servicemanager` 的代码位于 Android 源码的 `frameworks/native/cmds/servicemanager` 目录，入口为 `main()` 函数。
    
    c
    
    ```c
    int main(int argc, char** argv) {
        // 1. 创建设备节点访问权限
        umask(0);
        
        // 2. 打开 Binder 驱动
        sp<ProcessState> ps(ProcessState::self());
        
        // 3. 启动 Binder 线程池
        ps->startThreadPool();
        
        // 4. 成为 Binder 上下文管理者（关键步骤）
        sp<IServiceManager> sm = defaultServiceManager();
        
        // 5. 进入循环，等待 Binder 请求
        ProcessState::self()->joinThreadPool();
        
        return 0;
    }
    ```
    
      
    

#### **2. 关键初始化步骤**

##### **(1) 打开 Binder 驱动**

- **调用 `open("/dev/binder", O_RDWR)`**：  
    通过系统调用打开 Binder 驱动设备文件，获取文件描述符（`fd`）。
- **内存映射（mmap）**：  
    通过 `mmap()` 在内核空间分配一块缓冲区，用于跨进程通信的数据传输，避免用户空间与内核空间的多次拷贝。

##### **(2) 创建设备节点访问权限**

- **设置 `/dev/binder` 权限**：  
    通过 `ioctl` 命令设置 Binder 驱动的访问权限，确保只有特定用户（如 `system` 组）可访问。

##### **(3) 成为上下文管理者（`defaultServiceManager`）**

- **核心函数**：
    
    c
    
    ```c
    sp<IServiceManager> defaultServiceManager() {
        if (gDefaultServiceManager != nullptr) return gDefaultServiceManager;
        
        // 打开 Binder 驱动（若未打开）
        {
            AutoMutex _l(gDefaultServiceManagerLock);
            if (gDefaultServiceManager == nullptr) {
                // 创建 BpBinder(0)，对应 ServiceManager 的 Binder 句柄为 0
                sp<IBinder> b = interface_cast<IBinder>(
                    ProcessState::self()->getContextObject(nullptr));
                
                // 创建 ServiceManager 的代理对象（BpServiceManager）
                gDefaultServiceManager = interface_cast<IServiceManager>(b);
            }
        }
        return gDefaultServiceManager;
    }
    ```
    
      
    - **核心逻辑**：  
        通过 `ProcessState::getContextObject(nullptr)` 获取 Binder 句柄为 `0` 的对象（即 `ServiceManager` 自身），并创建其代理对象 `BpServiceManager`。

##### **(4) 启动 Binder 线程池**

- **调用 `startThreadPool()`**：  
    为 `ServiceManager` 进程创建 Binder 线程池，用于处理客户端的注册、查询等请求。
- **线程循环**：  
    通过 `joinThreadPool()` 使主线程加入线程池，进入循环，等待 Binder 驱动的事件通知（如 `ioctl(BINDER_WRITE_READ)`）。

#### **3. Binder 驱动中的注册**

- **唯一标识**：  
    `ServiceManager` 在 Binder 驱动中通过 **固定句柄 `0`** 标识，是所有 Binder 服务的 “根节点”。
- **驱动层逻辑**：  
    当 `ServiceManager` 进程启动并打开 Binder 驱动后，驱动会为其分配一个 **`BINDER_SERVICE_MANAGER`** 类型的节点，并将其句柄固定为 `0`。  
    其他进程通过句柄 `0` 即可访问 `ServiceManager` 的 `transact` 方法。

### **三、ServiceManager 与其他服务的交互**

#### **1. 服务注册（如 SystemServer 注册服务）**

1. **Java 层调用**：
    
    java
    
    ```java
    ServiceManager.addService("activity.mgr", activityManagerService);
    ```
    
      
    
2. **JNI 层转换**：  
    调用 `android_os_ServiceManager_addService` 方法，将 Java `Binder` 对象转换为 Native `BBinder` 对象。
3. **Native 层通信**：  
    通过 `BpServiceManager`（`ServiceManager` 的代理对象）调用 `transact` 方法，向句柄 `0` 发送 `ADD_SERVICE_TRANSACTION` 事务。
4. **ServiceManager 处理**：  
    在 `onTransact` 中解析请求，将服务名与对应的 `BBinder` 对象存入内部映射表（如 `Vector<Service> mServices`）。

#### **2. 服务查询（如客户端获取服务）**

1. **Java 层调用**：
    
    java
    
    ```java
    ServiceManager.getService("activity.mgr");
    ```
    
      
    
2. **Native 层通信**：  
    通过 `BpServiceManager` 发送 `GET_SERVICE_TRANSACTION` 事务。
3. **ServiceManager 响应**：  
    从映射表中查找服务对应的 `BBinder` 对象，返回其句柄给客户端，客户端通过句柄创建 `BpBinder` 代理对象。

### **四、关键总结**

1. **启动时机**：  
    `ServiceManager` 是 Android 系统中 **第一个启动的 Binder 服务**，早于 `Zygote` 和 `SystemServer`，为后续服务的注册和查询奠定基础。
2. **唯一性**：  
    全系统只有一个 `ServiceManager` 实例，通过固定句柄 `0` 访问，是 Binder 机制的 “服务目录”。
3. **跨进程核心**：  
    所有系统服务（如 `AMS`、`WMS`）和应用服务都需通过 `ServiceManager` 注册和发现，是跨进程通信的核心枢纽。

### **附：启动时序图**

plaintext

```plaintext
系统启动流程：
init 进程 → 解析 init.rc → 启动 servicemanager 进程 → 打开 Binder 驱动 → 注册为上下文管理者 → 启动线程池 → 等待请求
       ↓
     Zygote 进程 → SystemServer 进程 → 注册系统服务（如 AMS）到 ServiceManager
       ↓
     应用进程 → 通过 ServiceManager 获取服务代理对象 → 跨进程通信
```

  

  

通过这一流程，`ServiceManager` 构建了 Android 系统中跨进程通信的基础架构，使得各个服务和应用能够通过统一的接口进行交互。