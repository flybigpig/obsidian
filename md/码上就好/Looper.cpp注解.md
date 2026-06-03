
用户希望我为 Looper.cpp 的核心函数添加详细的中文注释。我需要为每个核心函数添加清晰的中文注释，解释其功能、参数、返回值、设计意图等。由于这是 ask mode，我只能提供注释后的代码供用户参考，无法直接修改文件。

让我为所有核心函数编写带详细中文注释的版本。
# Looper.cpp 核心函数完整注释版

以下是对每个核心函数的逐行深度注释：

---

## 一、辅助类

### 1.1 WeakMessageHandler — 弱引用消息处理器（第 21-35 行）

```startLine:21:35:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
// --- WeakMessageHandler ---
// 【作用】使用弱引用包装 MessageHandler，避免循环引用导致的内存泄漏
// 【场景】当 handler 的生命周期不由 Looper 管理时，使用弱引用可以安全地
//         在 handler 被外部释放后自动失效，而不是阻止其释放

WeakMessageHandler::WeakMessageHandler(const wp<MessageHandler>& handler) :
        mHandler(handler) {
    // wp = weak pointer（弱指针），不增加引用计数
}

WeakMessageHandler::~WeakMessageHandler() {
    // 析构时无需特殊处理，wp 会自动释放
}

void WeakMessageHandler::handleMessage(const Message& message) {
    // 【关键】尝试将弱引用升级为强引用（sp）
    sp<MessageHandler> handler = mHandler.promote();
    if (handler != nullptr) {
        // promote 成功 → 原对象仍然存活，安全调用
        handler->handleMessage(message);
    }
    // promote 失败 → 原对象已被释放，静默丢弃消息（不会崩溃）
}
```

### 1.2 SimpleLooperCallback — 函数指针回调适配器（第 38-49 行）

```startLine:38:49:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
// --- SimpleLooperCallback ---
// 【作用】将 C 风格函数指针适配为 LooperCallback 接口
// 【设计模式】适配器模式（Adapter Pattern）
// 【用途】允许用户传入简单的函数指针而非必须实现 LooperCallback 类

SimpleLooperCallback::SimpleLooperCallback(Looper_callbackFunc callback) :
        mCallback(callback) {
    // 保存用户提供的回调函数指针
}

SimpleLooperCallback::~SimpleLooperCallback() {
}

int SimpleLooperCallback::handleEvent(int fd, int events, void* data) {
    // 直接委托给用户函数，参数透传
    return mCallback(fd, events, data);
}
```

---

## 二、构造与析构

### 2.1 Looper 构造函数（第 60-73 行）

```startLine:60:73:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
// --- Looper ---

// Maximum number of file descriptors for which to retrieve poll events each iteration.
static const int EPOLL_MAX_EVENTS = 16;
// 【含义】每次 epoll_wait 最多返回的事件数量
// 【为什么是 16？】Android UI 线程通常监听的 fd 数量有限（输入事件、传感器等），
//                  16 是经验值，平衡内存占用和系统调用次数

static pthread_once_t gTLSOnce = PTHREAD_ONCE_INIT;   // TLS key 一次性初始化控制
static pthread_key_t gTLSKey = 0;                      // 线程本地存储键（存储 Looper*）

Looper::Looper(bool allowNonCallbacks)
    : mAllowNonCallbacks(allowNonCallbacks),     // 是否允许无 callback 的 fd 注册
      mSendingMessage(false),                    // 是否正在分发消息中（优化标志）
      mPolling(false),                           // 是否正在 epoll_wait 阻塞中
      mEpollRebuildRequired(false),              // 是否需要重建 epoll 实例
      mNextRequestSeq(0),                        // 下一个请求序列号（用于版本校验）
      mResponseIndex(0),                         // 当前 Response 消费位置
      mNextMessageUptime(LLONG_MAX) {            // 下一条消息的执行时间（初始=无穷大）
    
    // ====== 第一步：创建唤醒机制（eventfd）======
    // eventfd 是 Linux 特有的 IPC 机制，比 pipe 更轻量
    // 参数1: initval=0（计数器初始值）
    // 参数2: EFD_NONBLOCK（非阻塞）| EFD_CLOEXEC（fork 后自动关闭）
    mWakeEventFd.reset(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
    LOG_ALWAYS_FATAL_IF(mWakeEventFd.get() < 0, "Could not make wake event fd: %s", strerror(errno));
    // FATAL：eventfd 创建失败意味着 Looper 完全无法工作，只能终止进程

    // ====== 第二步：初始化 epoll 实例 ======
    AutoMutex _l(mLock);        // RAII 锁，作用域结束自动解锁
    rebuildEpollLocked();       // 在锁保护下创建 epoll 并注册 wake fd
}
```

**成员初始化详解：**

| 成员 | 初始值 | 含义 |
|------|--------|------|
| `mAllowNonCallbacks` | 参数传入 | 兼容旧 API，允许注册不带 callback 的 fd |
| `mSendingMessage` | `false` | 优化：正在发消息时跳过 `wake()` |
| `mPolling` | `false` | 供 `isPolling()` 外部查询线程状态 |
| `mEpollRebuildRequired` | `false` | 延迟重建标记 |
| `mNextRequestSeq` | `0` | 单调递增，`-1` 保留作特殊值 |
| `mResponseIndex` | `0` | `pollOnce` 两阶段消费的位置游标 |
| `mNextMessageUptime` | `LLONG_MAX` | 无消息时设为最大值，表示"没有待处理消息" |

### 2.2 析构函数（第 75-76 行）

```startLine:75:76:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
Looper::~Looper() {
    // 【注意】析构函数体为空！
    // 所有资源通过 RAII 自动释放：
    //   - mWakeEventFd (unique_fd) → 自动 close
    //   - mEpollFd (unique_fd) → 自动 close（同时清理 epoll 内的所有监听）
    //   - mRequests 中的 callback (sp<>) → 引用计数归零时自动 delete
    //
    // 【设计哲学】C++ RAII > 手动清理，避免资源泄漏
}
```

---

## 三、线程本地存储（TLS）机制

### 3.1 TLS 初始化（第 78-81 行）

```startLine:78:81:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
void Looper::initTLSKey() {
    // 创建线程私有数据的 key
    // pthread_key_create 参数：
    //   - &gTLSKey: 输出参数，返回新创建的 key
    //   - threadDestructor: 线程退出时的清理回调函数
    int error = pthread_key_create(&gTLSKey, threadDestructor);
    LOG_ALWAYS_FATAL_IF(error != 0, "Could not allocate TLS key: %s", strerror(error));
    // TLS key 创建失败是致命错误，因为整个 Looper-per-thread 机制依赖它
}
```

### 3.2 线程析构函数（第 83-88 行）

```startLine:83:88:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
void Looper::threadDestructor(void *st) {
    // 【调用时机】线程退出时自动被 pthread 调用
    // 【参数 st】该线程存储在 TLS 中的值（即 Looper*）
    
    Looper* const self = static_cast<Looper*>(st);
    if (self != nullptr) {
        // decStrong: 减少强引用计数
        // (void*)threadDestructor: 标记是谁持有的这个引用（用于调试追踪）
        // 如果引用计数归零，Lo 对象将被删除
        self->decStrong((void*)threadDestructor);
    }
}
```

**引用计数的完整生命周期：**

```
setForThread(): incStrong (+1)  ← 线程持有
用户代码:       可能持有额外的 sp<Looper> 引用
threadExit:     decStrong (-1) ← 线程释放
如果计数=0:     ~Looper() 执行
```

### 3.3 设置/获取线程绑定（第 90-109 行）

```startLine:90:109:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
void Looper::setForThread(const sp<Looper>& looper) {
    // 【副作用】getForThread() 会触发 pthread_once 初始化 TLS key
    sp<Looper> old = getForThread();

    // 新 looper: 先增加引用（确保不会被提前销毁）
    if (looper != nullptr) {
        looper->incStrong((void*)threadDestructor);
    }

    // 将 looper 指针存入当前线程的 TLS
    pthread_setspecific(gTLSKey, looper.get());

    // 旧 looper: 减少引用（可能触发析构）
    if (old != nullptr) {
        old->decStrong((void*)threadDestructor);
    }
}

sp<Looper> Looper::getForThread() {
    // pthread_once: 保证 initTLSKey 只被执行一次（线程安全）
    int result = pthread_once(&gTLSOnce, initTLSKey);
    LOG_ALWAYS_FATAL_IF(result != 0, "pthread_once failed");

    // 从当前线程的 TLS 取出 Looper 指针（可能为 nullptr）
    return (Looper*)pthread_getspecific(gTLSKey);
}
```

### 3.4 prepare — 工厂方法（第 111-123 行）

```startLine:111:123:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
sp<Looper> Looper::prepare(int opts) {
    // 解析选项：是否允许非 callback 模式（兼容旧 Android 版本）
    bool allowNonCallbacks = opts & PREPARE_ALLOW_NON_CALLBACKS;
    
    // 尝试获取当前线程已绑定的 Looper
    sp<Looper> looper = Looper::getForThread();
    if (looper == nullptr) {
        // 首次调用：创建新 Looper 并绑定到当前线程
        looper = new Looper(allowNonCallbacks);
        Looper::setForThread(looper);
    }
    // 已存在：检查配置一致性（warn 但不阻塞）
    if (looper->getAllowNonCallbacks() != allowNonCallbacks) {
        ALOGW("Looper already prepared for this thread with a different value for the "
                "LOOPER_PREPARE_ALLOW_NON_CALLBACKS option.");
    }
    return looper;
}
```

---

## 四、Epoll 管理

### 4.1 rebuildEpollLocked — 重建 epoll 实例（第 129-161 行）

```startLine:129:161:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
void Looper::rebuildEpollLocked() {
    // 【前置条件】调用者必须持有 mLock
    // 【调用时机】① 构造时初始化 ② pollInner 中检测到重建标记
    
    // ---- 步骤 1：关闭旧的 epoll 实例 ----
    if (mEpollFd >= 0) {
        mEpollFd.reset();
        // reset() 调用 close(old_fd)，内核会自动清除该 epoll 上注册的所有 fd
    }

    // ---- 步骤 2：创建新的 epoll 实例 ----
    // EPOLL_CLOEXEC: fork+exec 时自动关闭，防止 fd 泄漏到子进程
    mEpollFd.reset(epoll_create1(EPOLL_CLOEXEC));
    LOG_ALWAYS_FATAL_IF(mEpollFd < 0, "Could not create epoll instance: %s", strerror(errno));

    // ---- 步骤 3：注册唤醒 fd（必须第一个注册）----
    struct epoll_event eventItem;
    memset(&eventItem, 0, sizeof(epoll_event));  // 清零 union 的脏数据
    eventItem.events = EPOLLIN;                   // 只关心可读事件（有唤醒信号写入）
    eventItem.data.fd = mWakeEventFd.get();       // 用 data.fd 存储原始 fd 编号
    int result = epoll_ctl(mEpollFd.get(), EPOLL_CTL_ADD, mWakeEventFd.get(), &eventItem);
    // wake fd 注册失败是致命的（Looper 无法被唤醒）
    LOG_ALWAYS_FATAL_IF(result != 0, "Could not add wake event fd to epoll instance: %s",
                        strerror(errno));

    // ---- 步骤 4：批量重注册所有用户 fd ----
    for (size_t i = 0; i < mRequests.size(); i++) {
        const Request& request = mRequests.valueAt(i);  // 从映射表取出请求
        struct epoll_event eventItem;
        request.initEventItem(&eventItem);              // 转换为 epoll_event 格式

        int epollResult = epoll_ctl(mEpollFd.get(), EPOLL_CTL_ADD, request.fd, &eventItem);
        if (epollResult < 0) {
            // 用户 fd 注册失败仅记录日志（fd 可能已被关闭），不终止
            ALOGE("Error adding epoll events for fd %d while rebuilding epoll set: %s",
                  request.fd, strerror(errno));
        }
    }
}
```

### 4.2 scheduleEpollRebuildLocked — 延迟调度重建（第 163-171 行）

```startLine:163:171:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
void Looper::scheduleEpollRebuildLocked() {
    // 【设计】延迟执行模式（Deferred Execution）
    // 不立即重建，而是设置标记 + 唤醒 poll 循环，让 pollInner 在安全的时机执行
    
    if (!mEpollRebuildRequired) {          // 防止重复调度（幂等性保证）
        mEpollRebuildRequired = true;      // 设置重建标记
        wake();                             // 唤醒 epoll_wait（如果正在阻塞的话）
        // wake 后，pollInner 会从 epoll_wait 返回，然后检测到标记并执行重建
    }
}
```

---

## 五、轮询机制（核心中的核心 ⭐⭐⭐）

### 5.1 pollOnce — 分发入口（第 173-207 行）

```startLine:173:207:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
int Looper::pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
    // 【参数说明】
    //   timeoutMillis: 超时时间（毫秒），-1 表示无限等待，0 表示立即返回
    //   outFd: [输出] 触发事件的文件描述符
    //   outEvents: [输出] 事件类型（EVENT_INPUT/EVENT_OUTPUT/EVENT_ERROR/EVENT_HANGUP）
    //   outData: [输出] 用户注册时传入的自定义数据指针
    // 【返回值】
    //   >= 0: 用户注册时的 ident 值
    //   POLL_WAKE (-1): 被 wake() 唤醒
    //   POLL_TIMEOUT (-2): 超时
    //   POLL_ERROR (-3): 发生错误
    //   POLL_CALLBACK (-4): 处理了 callback（需要继续轮询以获取最终结果）

    int result = 0;
    for (;;) {                                    // 无限循环直到有可返回的结果
        // ========== 第一层：消费 Response 缓冲区中 ident >= 0 的事件 ==========
        // Response 缓冲区由上一次 pollInner 填充
        // ident == POLL_CALLBACK (-4) 的事件不在此层返回，留到后面处理
        while (mResponseIndex < mResponses.size()) {
            const Response& response = mResponses.itemAt(mResponseIndex++);
            int ident = response.request.ident;
            if (ident >= 0) {                     // 只返回非 CALLBACK 类型
                // 填充输出参数
                if (outFd != nullptr) *outFd = fd;
                if (outEvents != nullptr) *outEvents = events;
                if (outData != nullptr) *outData = data;
                return ident;                      // 返回用户的自定义标识符
            }
        }

        // ========== 第二层：如果上轮 pollInner 有非 CALLBACK 结果，直接返回 ==========
        if (result != 0) {
            // result != 0 意味着上轮 pollInner 返回了 POLL_WAKE/TIMEOUT/ERROR
            // 且 Response 缓冲区已经全部消费完毕
            if (outFd != nullptr) *outFd = 0;
            if (outEvents != nullptr) *outEvents = 0;
            if (outData != nullptr) *outData = nullptr;
            return result;
        }

        // ========== 第三层：进入真正的 I/O 多路复用等待 ==========
        result = pollInner(timeoutMillis);
        // pollInner 返回后会填充 mResponses 缓冲区
        // 下一轮循环的第一层会先检查缓冲区
    }
}
```

**三层过滤模型图解：**

```
pollOnce 进入 ──────────────────────────────────────┐
                                                   │
  ┌── 第一层 ──┐  匹配(ident>=0)? ──Yes──→ 返回 ident │
  │ 消费缓存区  │                                   │
  │ mResponses │  No                                │
  └─────┬──────┘                                   │
        ▼                                           │
  ┌── 第二层 ──┐  result!=0? ──Yes──→ 返回 result   │←┘
  │ 上次结果   │                                   │
  │ 非CALLBACK │  No (首次或result==CALLBACK)         │
  └─────┬──────┘                                   │
        ▼                                           │
  ┌── 第三层 ──┐  调用 epoll_wait + 处理事件          │
  │ pollInner  │  返回结果存入 result                 │
  └─────┬──────┘  填充 mResponses                    │
        │                                           │
        └──────────→ 回到第一层 ────────────────────┘
```

### 5.2 pollInner — 事件等待引擎（第 209-365 行）⭐⭐⭐

```startLine:209:365:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
int Looper::pollInner(int timeoutMillis) {
    // ════════════════ 阶段 0：调整超时时间 ════════════════
    // 如果有待处理的定时消息，将超时时间缩短为「下条消息到期时间」
    // 这样即使没有 IO 事件，也能及时分发到期的消息
    if (timeoutMillis != 0 && mNextMessageUptime != LLONG_MAX) {
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);  // 使用单调时钟（不受系统时间调整影响）
        int messageTimeoutMillis = toMillisecondTimeoutDelay(now, mNextMessageUptime);
        // 取较小值：用户超时 vs 消息超时
        if (messageTimeoutMillis >= 0
                && (timeoutMillis < 0 || messageTimeoutMillis < timeoutMillis)) {
            timeoutMillis = messageTimeoutMillis;
        }
    }

    // ════════════════ 阶段 1：准备 polling ════════════════
    int result = POLL_WAKE;           // 默认结果：被唤醒
    mResponses.clear();               // 清空上次遗留的响应缓冲区
    mResponseIndex = 0;               // 重置消费游标

    mPolling = true;                  // ★ 标记进入阻塞状态（isPolling() 可查询）

    // ════════════════ 阶段 2：epoll_wait 阻塞等待 ════════════════
    struct epoll_event eventItems[EPOLL_MAX_EVENTS];  // 栈上分配事件数组
    int eventCount = epoll_wait(
        mEpollFd.get(),             // epoll 实例 fd
        eventItems,                 // 输出事件数组
        EPOLL_MAX_EVENTS,           // 最大事件数 (16)
        timeoutMillis               // 超时时间（毫秒）
    );
    // 【阻塞点】线程在这里暂停，直到以下任一条件发生：
    //   1. 有 fd 就绪（IO 事件 / wake 信号）
    //   2. 超时时间到达
    //   3. 被信号中断（返回 -1, errno=EINTR）

    mPolling = false;                 // ★ 标记退出阻塞状态

    // ════════════════ 阶段 3：加锁并检查重建标记 ════════════════
    mLock.lock();

    if (mEpollRebuildRequired) {      // 其他线程设置了重建标记？
        mEpollRebuildRequired = false;
        rebuildEpollLocked();         // 重建 epoll 实例
        goto Done;                    // 跳过本轮事件（基于旧 epoll 的数据不可靠）
    }

    // ════════════════ 阶段 4：错误处理 ════════════════
    if (eventCount < 0) {
        if (errno == EINTR) {
            goto Done;                // 被信号中断是正常情况，静默忽略
        }
        ALOGW("Poll failed with an unexpected error: %s", strerror(errno));
        result = POLL_ERROR;
        goto Done;
    }

    // ════════════════ 阶段 5：超时处理 ════════════════
    if (eventCount == 0) {
        result = POLL_TIMEOUT;
        goto Done;
    }

    // ════════════════ 阶段 6：事件分发（★ 核心）════════════════
    for (int i = 0; i < eventCount; i++) {
        int fd = eventItems[i].data.fd;           // 从 epoll_event.data.fd 取出 fd 编号
        uint32_t epollEvents = eventItems[i].events;  // 原生 epoll 事件掩码

        if (fd == mWakeEventFd.get()) {
            // ──── 情况 A：唤醒事件 ────
            if (epollEvents & EPOLLIN) {
                awoken();                          // 读取 eventfd 清空计数器
            } else {
                ALOGW("Ignoring unexpected epoll events 0x%x on wake event fd.", epollEvents);
            }
            // 注意：wake 事件不产生 Response，不进入后续回调流程
        } else {
            // ──── 情况 B：普通 IO 事件 ────
            ssize_t requestIndex = mRequests.indexOfKey(fd);  // 反向查找对应的 Request
            if (requestIndex >= 0) {
                // fd 仍在注册表中 → 有效事件
                int events = 0;
                // 将 epoll 事件转换为 Looper 抽象事件
                if (epollEvents & EPOLLIN)  events |= EVENT_INPUT;    // 可读
                if (epollEvents & EPOLLOUT) events |= EVENT_OUTPUT;   // 可写
                if (epollEvents & EPOLLERR) events |= EVENT_ERROR;    // 错误
                if (epollEvents & EPOLLHUP) events |= EVENT_HANGUP;   // 挂断
                pushResponse(events, mRequests.valueAt(requestIndex));  // 存入 Response 缓冲区
            } else {
                // fd 不在注册表中 → 可能刚被移除，忽略
                ALOGW("Ignoring unexpected epoll events 0x%x on fd %d that is "
                        "no longer registered.", epollEvents, fd);
            }
        }
    }
Done: ;   // 空语句（C++ 标签后必须有语句）

    // ════════════════ 阶段 7：消息分发（持有锁）════════════════
    mNextMessageUptime = LLONG_MAX;     // 重置为"无消息"
    while (mMessageEnvelopes.size() != 0) {
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
        const MessageEnvelope& envelope = mMessageEnvelopes.itemAt(0);  // 取队首（最早的）
        
        if (envelope.uptime <= now) {
            // 消息到期 → 执行分发
            
            // ──── 加锁取消息，解锁后执行 ────
            { // obtain handler（花括号控制 sp<> 的生命周期）
                sp<MessageHandler> handler = envelope.handler;   // 强引用 +1
                Message message = envelope.message;              // 拷贝消息
                mMessageEnvelopes.removeAt(0);                   // 从队列移除
                mSendingMessage = true;                          // 标记正在发送
                mLock.unlock();                                  // ★ 关键：解锁后再执行！

                // ★ 此时无锁，handler->handleMessage 可以：
                //   - 安全地 sendMessage（重新获取锁添加新消息）
                //   - 耗时操作不会阻塞其他线程的 addFd/removeFd
                handler->handleMessage(message);                 // 调用用户代码
            } // release handler（sp<> 析构，引用 -1，handler 可被删除）

            mLock.lock();                                        // 重新加锁继续
            mSendingMessage = false;                             // 清除标记
            result = POLL_CALLBACK;                              // 标记"处理了回调"
        } else {
            // 消息未到期 → 记录最近的消息时间，停止遍历
            mNextMessageUptime = envelope.uptime;
            break;                                               // 队列按时间有序，后面的更晚
        }
    }

    // ════════════════ 阶段 8：释放锁 ════════════════
    mLock.unlock();

    // ════════════════ 阶段 9：Response 回调执行（★ 无锁状态）════════════════
    for (size_t i = 0; i < mResponses.size(); i++) {
        Response& response = mResponses.editItemAt(i);
        if (response.request.ident == POLL_CALLBACK) {
            // 只处理 ident == POLL_CALLBACK 的 Response
            // （ident >= 0 的已在 pollOnce 第一层返回了）
            
            int fd = response.request.fd;
            int events = response.events;
            void* data = response.request.data;

            // 调用用户注册的回调函数
            int callbackResult = response.request.callback->handleEvent(fd, events, data);
            
            if (callbackResult == 0) {
                // 返回 0 表示："不再监听此 fd"
                removeFd(fd, response.request.seq);   // seq 用于版本校验防止误删
            }

            // 及时释放 callback 引用（mResponses 本身要到下次 poll 才清空）
            response.request.callback.clear();
            result = POLL_CALLBACK;
        }
    }
    return result;
}
```

**pollInner 完整时序图：**

```
线程进入 pollInner
    │
    ├─[无锁] 调整超时时间
    ├─[无锁] mPolling = true
    ├─[内核] epoll_wait() ◄════ 阻塞点
    ├─[无锁] mPolling = false
    │
    ├─[加锁] mLock.lock()
    │   ├─ 检查 epoll 重建标记 → 需要则重建 + goto Done
    │   ├─ 错误处理 (EINTR/其他)
    │   ├─── 遍历 epoll_events[] → 填充 mResponses[]
    │   ├─── 遍历 mMessageEnvelopes[] → 分发到期消息
    │   │      └─ [临时解锁] handler->handleMessage()
    │   │      └─ [重新加锁] 继续下一消息
    │   └─[解锁] mLock.unlock()
    │
    └─[无锁] 遍历 mResponses[] → 执行 callback handleEvent()
        └─ return result
```

### 5.3 pollAll — 便捷封装（第 367-391 行）

```startLine:367:391:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
int Looper::pollAll(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
    // 【作用】持续轮询直到获得非 CALLBACK 结果
    // 【与 pollOnce 的区别】pollOnce 返回 CALLBACK 后由调用者决定是否继续；
    //                       pollAll 自动循环直到得到最终结果

    if (timeoutMillis <= 0) {
        // 非正超时：简单循环直到非 CALLBACK 结果
        int result;
        do {
            result = pollOnce(timeoutMillis, outFd, outEvents, outData);
        } while (result == POLL_CALLBACK);
        return result;
    } else {
        // 正数超时：需要考虑时间流逝（每轮 pollOnce 都消耗了时间）
        nsecs_t endTime = systemTime(SYSTEM_TIME_MONOTONIC)
                + milliseconds_to_nanoseconds(timeoutMillis);

        for (;;) {
            int result = pollOnce(timeoutMillis, outFd, outEvents, outData);
            if (result != POLL_CALLBACK) {
                return result;              // 最终结果
            }

            // 重新计算剩余超时时间
            nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
            timeoutMillis = toMillisecondTimeoutDelay(now, endTime);
            if (timeoutMillis == 0) {
                return POLL_TIMEOUT;        // 时间耗尽
            }
        }
    }
}
```

---

## 六、唤醒机制

### 6.1 wake — 唤醒 epoll_wait（第 393-406 行）

```startLine:393:406:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
void Looper::wake() {
    // 【作用】从任意线程唤醒目标线程的 epoll_wait
    // 【原理】向 eventfd 写入数据，使 epoll 监听到 EPOLLIN 事件
    
    uint64_t inc = 1;
    // TEMP_FAILURE_RETRY: 自动重试被信号中断的系统调用
    ssize_t nWrite = TEMP_FAILURE_RETRY(
        write(mWakeEventFd.get(), &inc, sizeof(uint64_t))
    );
    // write 到 eventfd 的效果：内部计数器 += inc (即 +1)
    // 这会使 epoll_wait 因为 EPOLLIN 事件而返回
    
    if (nWrite != sizeof(uint64_t)) {
        if (errno != EAGAIN) {
            // EAGAIN: 非缓冲模式下正常情况（计数器溢出等）
            // 其他错误：致命问题（写唤醒信号失败意味着无法唤醒 Looper）
            LOG_ALWAYS_FATAL("Could not write wake signal to fd %d (returned %zd): %s",
                             mWakeEventFd.get(), nWrite, strerror(errno));
        }
    }
}
```

**eventfd 工作原理：**
```
write(fd, &1, 8)  →  counter += 1  →  epoll 检测到可读  →  epoll_wait 返回
read(fd, &val, 8) →  val = counter  →  counter = 0       →  消费完成
```

### 6.2 awoken — 消费唤醒信号（第 408-415 行）

```startLine:408:415:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
void Looper::awoken() {
    // 【作用】读取并清空 eventfd 的计数器
    // 【调用时机】pollInner 中检测到 wake eventfd 的 EPOLLIN 事件时
    // 【为什么不检查返回值？】只关心清空计数器，具体值不重要
    
    uint64_t counter;
    TEMP_FAILURE_RETRY(read(mWakeEventFd.get(), &counter, sizeof(uint64_t)));
    // read 返回后 counter 包含自上次 read 后累积的所有 write 值的总和
    // 同时计数器被重置为 0
}
```

---

## 七、辅助方法

### 7.1 pushResponse — 缓存事件响应（第 417-422 行）

```startLine:417:422:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
void Looper::pushResponse(int events, const Request& request) {
    // 【作用】将 epoll 事件转换为 Response 并加入缓冲区
    // 【为什么需要缓冲？】此时持有 mLock，不适合直接执行 callback
    //                   延迟到 pollInner 阶段 9（无锁状态）再执行
    
    Response response;
    response.events = events;              // 转换后的事件类型
    response.request = request;            // 完整拷贝 Request（含 fd/ident/callback/data）
    mResponses.push(response);             // 追加到缓冲区尾部
}
```

### 7.2 isPolling — 查询状态（第 653-655 行）

```startLine:653:655:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
bool Looper::isPolling() const {
    return mPolling;
    // 【用途】外部可查询 Looper 线程是否处于空闲阻塞状态
    // 【典型场景】PowerManager 判断是否可以休眠 CPU
    //              如果 isPolling()==true，说明没有待处理的事件，可以降频/睡眠
}
```

### 7.3 Request::initEventItem — 事件转换（第 657-665 行）

```startLine:657:665:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
void Looper::Request::initEventItem(struct epoll_event* eventItem) const {
    // 【作用】将 Looper 抽象的事件类型转换为 Linux epoll 原生事件类型
    // 这是 Looper 抽象层与内核 epoll 之间的桥梁
    
    int epollEvents = 0;
    if (events & EVENT_INPUT)  epollEvents |= EPOLLIN;    // 可读 → epoll 可读
    if (events & EVENT_OUTPUT) epollEvents |= EPOLLOUT;   // 可写 → epoll 可写
    // 注意：EVENT_ERROR 和 EVENT_HANGUP 由 epoll 自动报告，不需要显式注册

    memset(eventItem, 0, sizeof(epoll_event));  // 清零联合体的脏数据！
    eventItem->events = epollEvents;            // 设置关注的事件掩码
    eventItem->data.fd = fd;                    // 用 fd 作为回调时的查找键
    // 为什么不用 data.ptr 存储 Request*？
    // 因为 fd 更通用，且 pollOnce 需要区分 wake fd 和普通 fd
}
```

**事件类型映射表：**

| Looper 层 | epoll 层 | 含义 |
|-----------|----------|------|
| `EVENT_INPUT` (1 << 0) | `EPOLLIN` | 文件描述符可读（包括普通读和优先级带外数据） |
| `EVENT_OUTPUT` (1 << 1) | `EPOLLOUT` | 文件描述符可写 |
| `EVENT_ERROR` (1 << 2) | `EPOLLERR` | 关联的 fd 发生错误（始终被报告，无法屏蔽） |
| `EVENT_HANGUP` (1 << 3) | `EPOLLHUP` | 挂断（如管道写端关闭） |

---

## 八、FD 注册管理

### 8.1 addFd 重载（第 424-509 行）

```startLine:424:509:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
// ┌─────────────────────────────────────────────────────────────────┐
// │  重载 1：接受 C 函数指针（便捷接口）                               │
// └─────────────────────────────────────────────────────────────────┘
int Looper::addFd(int fd, int ident, int events, 
                  Looper_callbackFunc callback, void* data) {
    // 如果 callback 非空，将其包装为 SimpleLooperCallback 对象
    return addFd(fd, ident, events, 
                 callback ? new SimpleLooperCallback(callback) : nullptr, data);
}

// ┌─────────────────────────────────────────────────────────────────┐
// │  重载 2：核心实现（接受 LooperCallback 智能指针）                   │
// └─────────────────────────────────────────────────────────────────┘
int Looper::addFd(int fd, int ident, int events, 
                  const sp<LooperCallback>& callback, void* data) {
    // 【参数验证】
    if (!callback.get()) {
        // 无 callback 模式（旧 API 兼容）
        if (!mAllowNonCallbacks) {
            ALOGE("Invalid attempt to set NULL callback but not allowed for this looper.");
            return -1;                                     // 此 Looper 不允许
        }
        if (ident < 0) {
            ALOGE("Invalid attempt to set NULL callback with ident < 0.");
            return -1;                                     // 无 callback 必须有合法 ident
        }
    } else {
        // 有 callback：强制覆盖 ident 为 POLL_CALLBACK
        ident = POLL_CALLBACK;                            // (-4)
    }

    { // acquire lock
        AutoMutex _l(mLock);

        // 构建 Request 结构体
        Request request;
        request.fd = fd;                                  // 要监听的 fd
        request.ident = ident;                            // 标识符（POLL_CALLBACK 或用户指定）
        request.events = events;                          // 关注的事件
        request.seq = mNextRequestSeq++;                  // 单调递增序列号（防竞态）
        request.callback = callback;                      // 回调对象
        request.data = data;                              // 用户自定义数据
        if (mNextRequestSeq == -1) mNextRequestSeq = 0;   // -1 保留作特殊值，跳过

        // 准备 epoll_event
        struct epoll_event eventItem;
        request.initEventItem(&eventItem);

        // 检查 fd 是否已存在
        ssize_t requestIndex = mRequests.indexOfKey(fd);
        if (requestIndex < 0) {
            // ──── 新 fd：ADD 操作 ────
            int epollResult = epoll_ctl(mEpollFd.get(), EPOLL_CTL_ADD, fd, &eventItem);
            if (epollResult < 0) {
                ALOGE("Error adding epoll events for fd %d: %s", fd, strerror(errno));
                return -1;
            }
            mRequests.add(fd, request);                   // 加入映射表
        } else {
            // ──── 已存在的 fd：MOD 操作 ────
            int epollResult = epoll_ctl(mEpollFd.get(), EPOLL_CTL_MOD, fd, &eventItem);
            if (epollResult < 0) {
                if (errno == ENOENT) {
                    // ★ FD 回收竞争！
                    // 旧 fd 已关闭但未从 epoll 移除，同编号的新 fd 正在被注册
                    // 解决：回退到 ADD + 标记需重建 epoll（见 rebuildEpollLocked 分析）
                    epollResult = epoll_ctl(mEpollFd.get(), EPOLL_CTL_ADD, fd, &eventItem);
                    if (epollResult < 0) { /* ... error ... */ return -1; }
                    scheduleEpollRebuildLocked();          // 延迟重建
                } else {
                    ALOGE("Error modifying epoll events for fd %d: %s", fd, strerror(errno));
                    return -1;
                }
            }
            mRequests.replaceValueAt(requestIndex, request);  // 更新映射表
        }
    } // release lock (AutoMutex 析构)
    return 1;                                             // 成功
}
```

### 8.2 removeFd（第 511-571 行）

```startLine:511:571:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
// ┌─────────────────────────────────────────────────────────────────┐
// │  重载 1：简单移除（不做序列号校验）                                 │
// └─────────────────────────────────────────────────────────────────┘
int Looper::removeFd(int fd) {
    return removeFd(fd, -1);                              // seq=-1 表示跳过校验
}

// ┌─────────────────────────────────────────────────────────────────┐
// │  重载 2：带序列号校验的安全移除                                     │
// └─────────────────────────────────────────────────────────────────┘
int Looper::removeFd(int fd, int seq) {
    // 【seq 参数的作用】防止误删：
    //   场景：callback A 注册了 fd=5(seq=100)，执行时关闭了 fd=5
    //         然后 callback B 新注册了 fd=5(seq=200)
    //         如果 A 的清理代码想 removeFd(5)，不应该删除 B 的注册
    //   解决：A 调用 removeFd(5, 100)，发现当前 seq=200 ≠ 100，拒绝操作

    { // acquire lock
        AutoMutex _l(mLock);

        // 查找 fd
        ssize_t requestIndex = mRequests.indexOfKey(fd);
        if (requestIndex < 0) {
            return 0;                                      // 未找到（已移除或从未注册）
        }

        // 序列号校验
        if (seq != -1 && mRequests.valueAt(requestIndex).seq != seq) {
            return 0;                                      // 序列号不匹配，静默忽略
        }

        // ★ 无论 epoll 操作是否成功，都从映射表中移除 ★
        // 避免回调泄漏（callback 的 sp<> 保持在 mRequests 中会导致对象不被释放）
        mRequests.removeItemsAt(requestIndex);

        // 从 epoll 实例中移除
        int epollResult = epoll_ctl(mEpollFd.get(), EPOLL_CTL_DEL, fd, nullptr);
        if (epollResult < 0) {
            if (seq != -1 && (errno == EBADF || errno == ENOENT)) {
                // fd 已关闭（EBADF）或不在 epoll 中（ENOENT）
                // 且调用者提供了正确的 seq → 说明是正常的竞争场景
                scheduleEpollRebuildLocked();              // 安排重建
            } else {
                // 未知错误 → 映射表和 epoll 不同步了 → 防御性重建
                ALOGE("Error removing epoll events for fd %d: %s", fd, strerror(errno));
                scheduleEpollRebuildLocked();
                return -1;
            }
        }
    } // release lock
    return 1;                                             // 成功
}
```

---

## 九、消息队列机制

### 9.1 三层 sendMessage（第 573-616 行）

```startLine:573:616:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
// ┌─────────────────────────────────────────────────────────────────┐
// │  sendMessage: 立即投递（uptime = now）                             │
// └─────────────────────────────────────────────────────────────────┘
void Looper::sendMessage(const sp<MessageHandler>& handler, const Message& message) {
    nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
    sendMessageAtTime(now, handler, message);              // 设定执行时间为"现在"
}

// ┌─────────────────────────────────────────────────────────────────┐
// │  sendMessageDelayed: 延迟投递（uptime = now + delay）             │
// └─────────────────────────────────────────────────────────────────┘
void Looper::sendMessageDelayed(nsecs_t uptimeDelay, 
                                const sp<MessageHandler>& handler,
                                const Message& message) {
    nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
    sendMessageAtTime(now + uptimeDelay, handler, message); // 延迟指定的纳秒数
}

// ┌─────────────────────────────────────────────────────────────────┐
// │  sendMessageAtTime: 核心实现（按时间插入有序队列）                    │
// └─────────────────────────────────────────────────────────────────┘
void Looper::sendMessageAtTime(nsecs_t uptime, 
                               const sp<MessageHandler>& handler,
                               const Message& message) {
    size_t i = 0;
    { // acquire lock
        AutoMutex _l(mLock);

        // ──── 步骤 1：找到插入位置（保持队列按 uptime 升序有序）────
        size_t messageCount = mMessageEnvelopes.size();
        while (i < messageCount && uptime >= mMessageEnvelopes.itemAt(i).uptime) {
            i += 1;                                       // 跳过所有 <= uptime 的消息
        }
        // 循环结束后，i 就是要插入的位置（第一个 uptime > 目标值的元素之前）

        // ──── 步骤 2：插入消息 ────
        MessageEnvelope envelope(uptime, handler, message);
        mMessageEnvelopes.insertAt(envelope, i, 1);       // 插入到位置 i

        // ──── 步骤 3：优化判断 ────
        if (mSendingMessage) {
            // ★ 当前正在 handleMessage 中 ★
            // 发送者可能是 handleMessage 本身（嵌套调用），
            // 也可能是其他线程。无论如何，当前消息循环结束后会自然检查新消息，
            // 无需额外 wake()，减少不必要的系统调用
            return;
        }
    } // release lock

    // ──── 步骤 4：条件唤醒 ────
    if (i == 0) {
        // ★ 只有插入到队头才需要唤醒 ★
        // 原因：新消息的执行时间早于之前的队头，
        // epoll_wait 的超时是基于"之前队头的时间"计算的，
        // 必须唤醒以重新计算超时时间
        wake();
    }
    // 如果 i > 0：新消息插在中间或队尾，不影响最近的执行时间，无需唤醒
}
```

**消息队列有序性示例：**

```
插入前:  [t=100] [t=500] [t=1000]
插入 t=300:
         查找: 300 >= 100? yes(i=1), 300 >= 500? no → 停在 i=1
插入后:  [t=100] [t=300] [t=500] [t=1000]  ✅ 保持有序
         
插入 t=50:
         查找: 50 >= 100? no → 停在 i=0
插入后:  [t=50] [t=100] [t=500] [t=1000]   ✅ i==0 → 触发 wake()
```

### 9.2 removeMessages（第 618-651 行）

```startLine:618:651:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
// ┌─────────────────────────────────────────────────────────────────┐
// │  removeMessages(handler): 移除指定 handler 的所有消息               │
// └─────────────────────────────────────────────────────────────────┘
void Looper::removeMessages(const sp<MessageHandler>& handler) {
    { // acquire lock
        AutoMutex _l(mLock);

        // 从后向前遍历（删除元素不影响前面的索引）
        for (size_t i = mMessageEnvelopes.size(); i != 0; ) {
            const MessageEnvelope& envelope = mMessageEnvelopes.itemAt(--i);
            if (envelope.handler == handler) {
                mMessageEnvelopes.removeAt(i);            // 匹配则移除
            }
        }
    } // release lock
}

// ┌─────────────────────────────────────────────────────────────────┐
// │  removeMessages(handler, what): 移除指定 handler + what 的消息     │
// └─────────────────────────────────────────────────────────────────┘
void Looper::removeMessages(const sp<MessageHandler>& handler, int what) {
    { // acquire lock
        AutoMutex _l(mLock);

        for (size_t i = mMessageEnvelopes.size(); i != 0; ) {
            const MessageEnvelope& envelope = mMessageEnvelopes.itemAt(--i);
            if (envelope.handler == handler
                    && envelope.message.what == what) {
                mMessageEnvelopes.removeAt(i);            // 双条件匹配
            }
        }
    } // release lock
}
```

---

## 十、虚析构函数（第 667-669 行）

```startLine:667:669:c:/D/android_project/cells-android10/system/core/libutils/Looper.cpp
MessageHandler::~MessageHandler() { }    // 空实现（基类析构必须存在以确保正确释放）
LooperCallback::~LooperCallback() { }    // 同上
```

---

## 十一、整体架构总结图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Looper 整体架构                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────── 外部入口 ───────────────┐                             │
│  │  prepare() → 绑定线程 (TLS)             │                             │
│  │  pollOnce() → 两阶段消费 Response      │◄──── 用户循环调用            │
│  │  pollAll() → 持续轮询到最终结果         │                             │
│  └───────────────┬────────────────────────┘                             │
│                  │                                                      │
│                  ▼                                                      │
│  ┌─────────────── pollInner (引擎) ──────────┐                          │
│  │                                          │                          │
│  │  ┌──────────────────────────────────┐   │                          │
│  │  │  epoll_wait() ◄── 阻塞等待 ──┐  │   │                          │
│  │  └────────────────────────────────┘   │   │                          │
│  │              │                        │   │                          │
│  │              ▼                        │   │                          │
│  │  ┌────────────────────────────────┐   │   │                          │
│  │  │ wake fd? → awoken()            │   │   │                          │
│  │  │ other fd? → pushResponse()     │   │   │                          │
│  │  └──────────────┬─────────────────┘   │   │                          │
│  │                 │                     │   │                          │
│  │                 ▼                     │   │                          │
│  │  ┌────────────────────────────────┐   │   │                          │
│  │  │ 分发到期消息 (unlock→call→lock) │   │   │                          │
│  │  └──────────────┬─────────────────┘   │   │                          │
│  │                 │                     │   │                          │
│  │                 ▼ unlock             │   │                          │
│  │  ┌────────────────────────────────┐   │   │                          │
│  │  │ 执行 Response callbacks (无锁)  │   │   │                          │
│  │  └────────────────────────────────┘   │   │                          │
│  └──────────────────────────────────────┘   │                          │
│                                             │                          │
│  ┌────────────── 辅助接口 ──────────────┐   │                          │
│  │  addFd()    → 注册 IO 监听           ├───┘                          │
│  │  removeFd() → 取消 IO 监听           │                              │
│  │  sendMessage() → 投递定时消息        │                              │
│  │  wake()      → 唤醒阻塞              │                              │
│  │  isPolling() → 查询状态              │                              │
│  └──────────────────────────────────────┘                              │
│                                                                         │
│  底层支撑: eventfd (唤醒) + epoll (多路复用) + mutex (线程安全)          │
└─────────────────────────────────────────────────────────────────────────┘
```

如果你希望我将这些注释实际写入文件（需要切换到 craft mode），或者对某个具体函数还想深入了解，随时告诉我！