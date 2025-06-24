

Native Looper 是 Android 系统底层的事件循环机制，主要负责处理跨线程通信、文件描述符监听和定时任务。其工作流程可分为 **初始化、事件注册、事件循环、事件处理** 四个核心阶段：

### **一、初始化阶段**

1. **线程绑定 Looper**
    
    - 每个线程首次调用 `Looper::getForThread()` 时，会创建并绑定一个 `Looper` 实例（单例模式）。
    - 示例：
        
        cpp
        
        运行
        
        ```cpp
        sp<Looper> looper = Looper::prepare(0); // 为当前线程创建 Looper
        ```
        
          
        
2. **底层实现**
    
    - 内部创建一个 `epoll` 实例（Linux 系统）用于监听文件描述符事件。
    - 创建 `eventfd` 用于线程间唤醒（当消息入队时触发）。

### **二、事件注册阶段**

通过 `addFd()` 方法注册需要监听的事件源：

  

cpp

运行

```cpp
int Looper::addFd(
    int fd,                     // 文件描述符（如 socket、pipe）
    int ident,                  // 事件标识（回调时返回）
    int events,                 // 监听的事件类型（如 ALOOPER_EVENT_INPUT）
    const sp<LooperCallback>& callback, // 事件回调
    void* data                  // 传递给回调的用户数据
);
```

  

**常见事件源**：

  

- **消息队列**：通过 `eventfd` 监听消息入队事件。
- **定时器**：通过 `timerfd` 实现定时触发。
- **Socket/Pipe**：监听网络或进程间通信数据。

### **三、事件循环阶段**

通过 `pollOnce()` 进入无限循环，等待并处理事件：

  

cpp

运行

```cpp
int Looper::pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
    // 1. 处理所有待处理的消息（如延迟消息到期）
    if (mMessageEnvelopes.size() != 0) {
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
        const MessageEnvelope& messageEnvelope = mMessageEnvelopes.itemAt(0);
        if (messageEnvelope.uptime <= now) {
            // 执行消息回调
            messageEnvelope.handler->handleMessage(messageEnvelope.message);
            return 1; // 表示处理了一条消息
        }
    }

    // 2. 调用 epoll_wait() 等待事件发生
    int result = epoll_wait(mEpollFd, mEpollEvents, EPOLL_MAX_EVENTS, timeoutMillis);
    
    // 3. 处理 epoll 返回的事件
    for (int i = 0; i < result; i++) {
        int fd = mEpollEvents[i].data.fd;
        uint32_t epollEvents = mEpollEvents[i].events;
        
        // 检查是否为唤醒事件（eventfd 触发）
        if (fd == mWakeEventFd) {
            if (epollEvents & EPOLLIN) {
                awoken(); // 读取 eventfd 数据，清除事件
            }
        } else {
            // 调用用户注册的回调处理其他事件
            const Request* request = mRequests.valueFor(fd);
            if (request) {
                int callbackResult = request->callback->handleEvent(
                    fd, epollEvents, request->data);
                // ...
            }
        }
    }
    
    return result; // 返回处理的事件数量
}
```

### **四、事件处理阶段**

当事件发生时，Native Looper 会：

  

1. **执行回调函数**
    
    - 对于通过 `addFd()` 注册的事件，调用对应的 `LooperCallback`。
    - 示例：
        
        cpp
        
        运行
        
        ```cpp
        class MyCallback : public LooperCallback {
        public:
            virtual int handleEvent(int fd, int events, void* data) {
                if (events & ALOOPER_EVENT_INPUT) {
                    // 读取 fd 数据并处理
                    uint8_t buffer[1024];
                    read(fd, buffer, sizeof(buffer));
                    // ...
                }
                return 1; // 返回 1 表示继续监听该 fd
            }
        };
        ```
        
          
        
2. **处理消息队列**
    
    - 对于通过 `sendMessage()` 发送的消息，按时间顺序执行 `MessageHandler`。
    - 示例：
        
        cpp
        
        运行
        
        ```cpp
        class MyHandler : public MessageHandler {
        public:
            virtual void handleMessage(const Message& message) {
                switch (message.what) {
                    case MSG_UPDATE_UI:
                        // 更新 UI 逻辑
                        break;
                }
            }
        };
        ```
        
          
        

### **五、关键机制详解**

#### 1. **线程唤醒机制**

当向 Looper 发送消息或添加事件时，会通过 `eventfd` 唤醒等待中的 `epoll_wait()`：

  

cpp

运行

```cpp
void Looper::wake() {
    uint64_t inc = 1;
    ssize_t nWrite = TEMP_FAILURE_RETRY(write(mWakeEventFd, &inc, sizeof(uint64_t)));
    // ...
}
```

#### 2. **同步屏障（Sync Barrier）**

通过 `postSyncBarrier()` 插入特殊消息，阻止后续同步消息执行，优先处理异步消息（如输入事件）：

  

cpp

运行

```cpp
int Looper::postSyncBarrier(nsecs_t when) {
    // 插入一个无 targetHandler 的消息（即同步屏障）
    Message msg;
    msg.what = 0; // 特殊值表示屏障
    msg.target = nullptr; // 无 targetHandler 表示屏障
    // ...
}
```

#### 3. **延迟消息处理**

消息按 `when` 时间戳排序，`pollOnce()` 会检查最早的消息是否到期：

  

cpp

运行

```cpp
// 在 pollOnce() 中
nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
if (messageEnvelope.uptime <= now) {
    // 执行消息
} else {
    // 计算最近到期消息的剩余时间，作为 epoll_wait 的超时参数
    timeoutMillis = (int)((messageEnvelope.uptime - now) / 1000000LL);
}
```

### **六、工作流程总结**

plaintext

```plaintext
1. 线程初始化 Looper
   ↓
2. 注册事件源（fd、定时器、消息）
   ↓
3. 进入 pollOnce() 循环
   ↓
4. 检查是否有到期消息 → 有则执行回调
   ↓
5. 调用 epoll_wait() 等待事件
   ↓
6. 事件发生 → 执行对应回调
   ↓
7. 继续循环（或退出）
```

### **七、典型应用场景**

1. **Android 系统服务**：
    
    - SurfaceFlinger 通过 Native Looper 监听来自应用的绘制请求。
    - InputManagerService 处理触摸事件。
2. **JNI 通信**：
    
    - Java 层通过 `Handler` 发送消息，经 JNI 转发到 Native Looper 处理。
3. **高性能模块**：
    
    - 视频解码线程通过 Native Looper 监听缓冲区状态，减少 Java 层开销。

### **八、与 Java Looper 的对比**

|特性|Native Looper|Java Looper|
|---|---|---|
|**事件源**|文件描述符、定时器、消息|仅消息|
|**同步机制**|epoll、eventfd|Object.wait()、notify()|
|**性能**|高（无 JNI 开销）|低（需跨层调用）|
|**适用场景**|系统底层、高性能模块|应用层 UI 线程、普通消息处理|

  

理解 Native Looper 的工作流程，有助于深入分析 Android 系统的事件处理机制，优化跨线程通信性能，并解决复杂的线程同步问题。