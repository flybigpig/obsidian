# Android Binder 线程池启动过程详解

## 目录

1. [概述](#一概述)
2. [启动全流程图](#二启动全流程图)
3. [详细步骤源码解析](#三详细步骤源码解析)
4. [线程池动态扩缩](#四线程池动态扩缩)
5. [内核的线程管理](#五内核的线程管理)
6. [主线程 vs 辅助线程](#六主线程-vs-辅助线程)
7. [SystemServer 启动示例](#七systemserver-启动示例)
8. [线程池调优](#八线程池调优)
9. [总结](#九总结)

---

## 一、概述

Binder 线程池是 Android 每个使用 Binder 的进程中**管理 Binder 工作线程的机制**。每个进程在首次使用 Binder 时，会执行以下三步：

```
进程启动
   │
   ├── 1. ProcessState::self()                     ← 打开 /dev/binder + mmap
   │        │                                         （创建内核 binder_proc）
   │        └── open_driver() + mmap()
   │
   ├── 2. IPCThreadState::self()                   ← 初始化线程级状态
   │        │                                         （创建内核 binder_thread）
   │        └── pthread_getspecific() / 首次创建
   │
   └── 3. IPCThreadState::joinThreadPool(true)     ← 启动主 Binder 线程
                                                    （进入 Looper 循环）
```

---

## 二、启动全流程图

```
进程启动（如 SystemServer）
   │
   │── ProcessState::self()                              [C++ 层]
   │    │
   │    ├── 单例检查（已创建则直接返回）
   │    │
   │    ├── open("/dev/binder", O_RDWR)                 [kernel]
   │    │    └── binder_open() → 创建 binder_proc
   │    │
   │    ├── mmap(NULL, BINDER_VM_SIZE, PROT_READ,
   │    │         MAP_PRIVATE | MAP_NORESERVE, fd, 0)    [kernel]
   │    │    └── binder_mmap() → 分配物理内存 + 双映射
   │    │
   │    └── 设置最大线程数
   │         └── ioctl(fd, BINDER_SET_MAX_THREADS, 15)
   │
   │── IPCThreadState::self()                            [C++ 层]
   │    │
   │    ├── 调用 ProcessState::self() 确保驱动已打开
   │    │
   │    └── 线程局部存储（TLS），每个线程一个 IPCThreadState 对象
   │
   │── IPCThreadState::joinThreadPool(true)              [C++ 层]
   │    │
   │    ├── ioctl(fd, BC_ENTER_LOOPER)                  [kernel]
   │    │    └── 标记 thread->looper |= ENTERED
   │    │
   │    └── while(1) 循环：
   │         ├── talkWithDriver() → ioctl(BINDER_WRITE_READ)
   │         │    │                  └── binder_ioctl_write_read()
   │         │    │                       └── binder_thread_read()
   │         │    │                            └── 阻塞在 wait_event_interruptible()
   │         │    │
   │         │    ├── BR_TRANSACTION → executeCommand()
   │         │    │    │                  └── 调用 BBinder::transact()
   │         │    │    │                       └── onTransact()
   │         │    │    └── BC_REPLY → 回复
   │         │    │
   │         │    ├── BR_REPLY → 唤醒等待同步调用的线程
   │         │    │
   │         │    └── BR_SPAWN_LOOPER → 创建新 Binder 线程
   │         │         │
   │         │         └── spawnPooledThread()
   │         │              └── new PoolThread(isMain=false)
   │         │                   └── pthread_create() → threadLoop()
   │         │                        └── joinThreadPool(false)
   │         │
   │         └── 处理超时 / 退出条件
```

---

## 三、详细步骤源码解析

### 3.1 ProcessState::self() — 单例入口

```cpp
// frameworks/native/libs/binder/ProcessState.cpp

sp<ProcessState> ProcessState::self()
{
    // 1. 全局单例（gProcess 是原子指针）
    if (gProcess != nullptr)
        return gProcess;

    // 2. 首次创建
    sp<ProcessState> process(new ProcessState(kDefaultDriver));
    gProcess = process;
    return process;
}
```

### 3.2 ProcessState 构造函数

```cpp
ProcessState::ProcessState(const char *driver)
    : mDriverName(String8(driver))
    , mDriverFD(-1)
    , mVMStart(MAP_FAILED)
    , mManagesContexts(false)
    , mMaxThreads(DEFAULT_MAX_BINDER_THREADS)  // 默认 15
{
    // ── 1. 打开 Binder 驱动 ──
    mDriverFD = open_driver(driver);

    if (mDriverFD >= 0) {
        // ── 2. mmap 分配映射区 ──
        // BINDER_VM_SIZE = (1MB - 2*page_size)
        mVMStart = mmap(nullptr, BINDER_VM_SIZE,
                        PROT_READ,
                        MAP_PRIVATE | MAP_NORESERVE,
                        mDriverFD, 0);

        if (mVMStart == MAP_FAILED) {
            close(mDriverFD);
            mDriverFD = -1;
        }
    }

    // ── 3. 记录调用栈──
    LOG_ALWAYS_FATAL_IF(mDriverFD < 0,
                        "Binder driver could not be opened: %s", driver);
}
```

### 3.3 open_driver() — 打开设备并设置参数

```cpp
static int open_driver(const char *driver)
{
    // ── 1. 打开 Binder 设备 ──
    int fd = open(driver, O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return fd;

    // ── 2. 检查 Binder 版本 ──
    int vers;
    status_t result = ioctl(fd, BINDER_VERSION, &vers);
    if (result == -1 || vers != BINDER_CURRENT_PROTOCOL_VERSION) {
        close(fd);
        return -1;
    }

    // ── 3. 设置最大线程数 ──
    size_t maxThreads = DEFAULT_MAX_BINDER_THREADS;  // = 15
    result = ioctl(fd, BINDER_SET_MAX_THREADS, &maxThreads);

    // ── 4. 统计已打开次数 ──
    gOpenCount++;

    return fd;
}
```

### 3.4 IPCThreadState::self() — 线程局部存储

```cpp
// frameworks/native/libs/binder/IPCThreadState.cpp

IPCThreadState* IPCThreadState::self()
{
    // 1. 如果本线程已有 IPCThreadState，直接返回
    if (gHaveTLS) {
        restart:
        const pthread_key_t k = gTLS;
        IPCThreadState *st = (IPCThreadState*)pthread_getspecific(k);
        if (st)
            return st;
        return nullptr;
    }

    // 2. 首次调用：创建 TLS key
    if (gShutdown)
        return nullptr;

    // 3. 创建新的 IPCThreadState
    IPCThreadState *st = new IPCThreadState;

    // 4. 保存到线程局部存储
    pthread_setspecific(gTLS, st);
    st->mProcess->mThreadCount.fetch_add(1, std::memory_order_relaxed);
    gHaveTLS = true;

    return st;
}
```

### 3.5 IPCThreadState 构造函数

```cpp
IPCThreadState::IPCThreadState()
    : mProcess(ProcessState::self()),    // 确保 ProcessState 已初始化
      mStrictModePolicy(0),
      mLastTransactionBinderFlags(0)
{
    // 发送 BC_ENTER_LOOPER 或 BC_REGISTER_LOOPER 不是在这里
    // 而是在 joinThreadPool() 中
    pthread_setspecific(gTLS, this);
    clearCaller();
}
```

### 3.6 joinThreadPool() — 启动 Binder 线程主循环

```cpp
void IPCThreadState::joinThreadPool(bool isMain)
{
    // ── 1. 向内核注册本线程 ──
    if (isMain) {
        // 主线程使用 BC_ENTER_LOOPER
        // 表示线程已准备好进入 Binder 事件循环
        mOut.writeInt32(BC_ENTER_LOOPER);
    } else {
        // 辅助线程使用 BC_REGISTER_LOOPER
        mOut.writeInt32(BC_REGISTER_LOOPER);
    }

    // ── 2. 主循环 ──
    while (true) {
        // ── 3. 与驱动通信（核心！） ──
        result = getAndExecuteCommand();

        // ── 4. 超时退出（非主线程可退出） ──
        if (result == TIMED_OUT && !isMain)
            break;
    }

    // ── 5. 退出循环后，通知内核本线程退出 ──
    mOut.writeInt32(BC_EXIT_LOOPER);
    talkWithDriver(false);
}
```

### 3.7 getAndExecuteCommand() — 获取并执行命令

```cpp
status_t IPCThreadState::getAndExecuteCommand()
{
    status_t result;

    // ── 1. 与 Binder 驱动通信 ──
    // 发送 mOut 缓冲区的命令，接收数据到 mIn 缓冲区
    result = talkWithDriver();
    if (result >= NO_ERROR) {
        // ── 2. 有数据需要处理 ──
        size_t N = mIn.dataAvail();
        if (N > 0) {
            // ── 3. 执行命令 ──
            result = executeCommand(talkWithDriver());
        }
    }

    return result;
}
```

### 3.8 talkWithDriver() — 与内核驱动通信的核心

```cpp
status_t IPCThreadState::talkWithDriver(bool doReceive)
{
    binder_write_read bwr;

    // ── 1. 如果没有待发送数据且不需要接收，直接返回 ──
    if (mOut.dataSize() == 0 && !doReceive)
        return NO_ERROR;

    // ── 2. 填充 binder_write_read 结构 ──
    // 写入端
    bwr.write_size     = mOut.dataSize();
    bwr.write_consumed = 0;
    bwr.write_buffer   = (uintptr_t)mOut.data();

    // 读取端
    if (doReceive && mIn.dataSize() == 0) {
        // 接收数据
        bwr.read_size   = mIn.dataCapacity();
        bwr.read_consumed = 0;
        bwr.read_buffer = (uintptr_t)mIn.data();
    } else {
        // 不接收
        bwr.read_size   = 0;
        bwr.read_consumed = 0;
        bwr.read_buffer = 0;
    }

    // ── 3. 执行 ioctl — 进入内核 ──
    //      这是 Binder IPC 中最关键的一步
    //      内核 binder_ioctl() → binder_ioctl_write_read()
    //                              → binder_thread_read() 可能阻塞
    status_t result = ioctl(mProcess->mDriverFD,
                            BINDER_WRITE_READ, &bwr);

    if (result >= NO_ERROR) {
        // ── 4. 更新已消耗的写入数据 ──
        mOut.setDataSize(bwr.write_consumed);
        if (bwr.write_consumed > 0)
            mOut.flush();
        mOut.setDataSize(0);

        // ── 5. 更新接收数据 ──
        if (bwr.read_consumed > 0) {
            mIn.setDataSize(bwr.read_consumed);
            mIn.setDataOrientation(HDR);
        }
    }

    return result;
}
```

### 3.9 executeCommand() — 分发 BR_ 命令

```cpp
status_t IPCThreadState::executeCommand(int32_t cmd)
{
    switch ((uint32_t)cmd) {

    case BR_TRANSACTION:
    {
        // ── 收到远程调用请求 ──
        binder_transaction_data tr;
        mIn.read(&tr, sizeof(tr));

        // 解析 Parcel
        Parcel buffer;
        buffer.ipcSetDataReference(
            reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
            tr.data_size,
            nullptr, 0);

        // 查找目标 BBinder 并调用 onTransact()
        sp<BBinder> b = reinterpret_cast<BBinder*>(tr.cookie);
        b->transact(tr.code, buffer, &reply, tr.flags);

        // 发送回复
        sendReply(reply, 0);

        break;
    }

    case BR_REPLY:
    {
        // ── 收到同步调用的回复 ──
        // 唤醒等待的线程
        binder_transaction_data tr;
        mIn.read(&tr, sizeof(tr));
        // ... 处理回复数据 ...
        break;
    }

    case BR_SPAWN_LOOPER:
    {
        // ── 内核请求创建新的 Binder 线程！ ──
        // 这是线程池扩容的关键
        mProcess->spawnPooledThread(false);
        break;
    }

    case BR_DEAD_BINDER:
    {
        // ── 远程 Binder 服务死亡 ──
        // 触发死亡通知回调
        break;
    }

    case BR_TRANSACTION_COMPLETE:
        // 事务已提交
        break;

    // ... 其他命令 ...
    }

    return NO_ERROR;
}
```

---

## 四、线程池动态扩缩

### 4.1 BR_SPAWN_LOOPER — 创建新线程

当内核发现目标进程的线程池中没有空闲线程处理事务时，会向其中一个已注册线程返回 `BR_SPAWN_LOOPER`：

```cpp
// IPCThreadState::executeCommand()
case BR_SPAWN_LOOPER:
{
    // 检查线程池是否达到上限
    if (mProcess->mThreadCount < mProcess->mMaxThreads) {
        // 创建新线程
        mProcess->spawnPooledThread(false);
    }
    break;
}
```

```cpp
void ProcessState::spawnPooledThread(bool isMain)
{
    // ── 1. 检查线程数上限 ──
    if (mThreadCount >= mMaxThreads) {
        // 已达最大线程数，不会创建
        return;
    }

    // ── 2. 增加计数 ──
    mThreadCount.fetch_add(1, std::memory_order_relaxed);

    // ── 3. 创建新线程 ──
    sp<PoolThread> thread = new PoolThread(isMain);
    thread->run("Binder_%d", mThreadCount.load());
    // PoolThread::threadLoop() 中会调用
    // IPCThreadState::joinThreadPool(false)
}
```

### 4.2 PoolThread 类

```cpp
// ProcessState.cpp
class PoolThread : public Thread {
public:
    explicit PoolThread(bool isMain)
        : mIsMain(isMain) {}

protected:
    virtual bool threadLoop() {
        // 新线程加入 Binder 线程池
        IPCThreadState::self()->joinThreadPool(mIsMain);
        return false;  // 线程退出
    }

private:
    bool mIsMain;
};
```

### 4.3 完整线程生命周期

```
内核状态                             用户空间
─────────                          ─────────

binder_proc 创建 (binder_open)         ProcessState 构造
    │                                      │
    ├── max_threads = 15                   │
    ├── ready_threads = 0                  │
    └── threads = empty                    │
                                           │
    ◄── BC_ENTER_LOOPER (主线程) ────────── IPCThreadState::joinThreadPool(true)
    │                                      │
    ├── threads[rb]: thread_1              │
    │   looper = ENTERED | WAITING         │── talkWithDriver() → 阻塞等待
    │                                      │
    │                                      │
    │── 有事务到达                          │
    │── 检查空闲线程                        │
    │── ready_threads == 0                 │
    │── 向 thread_1 返回 BR_SPAWN_LOOPER   │
    │────────────────────────────────────► │── executeCommand(BR_SPAWN_LOOPER)
    │                                      │── spawnPooledThread(false)
    │                                      │    │
    │                                      │    └── pthread_create()
    │                                      │         └── joinThreadPool(false)
    │                                      │              │
    ◄── BC_REGISTER_LOOPER (线程2) ────────               │
    │                                      │              │
    ├── threads[rb]: thread_1, thread_2    │              │
    │   thread_2.looper = REGISTERED|WAIT  │              │
    │                                      │              │
    │── 检查空闲线程                        │              │
    │── ready_threads >= 1                 │              │
    │── 直接派发事务到 thread_2            │              │
    │────────────────────────────────────► │◄─────────────│
    │                                      │
    │── 事务太多再次 SPAWN                  │── spawnPooledThread(false)
    │── ... 直到 max_threads=15            │── pool 达到 15 个线程
    │                                      │
    │── 进程退出                            │── 所有线程退出
    ◄── BC_EXIT_LOOPER (各线程) ──────────  │── joinThreadPool() 退出循环
```

---

## 五、内核的线程管理

### 5.1 binder_thread 的创建

```c
// binder_get_thread() — 在 binder_ioctl 中调用
static struct binder_thread *binder_get_thread(struct binder_proc *proc)
{
    struct binder_thread *thread;
    struct rb_node *parent = NULL;
    struct rb_node **p = &proc->threads.rb_node;

    // ── 1. 在 proc->threads 红黑树中按 PID 查找 ──
    while (*p) {
        parent = *p;
        thread = rb_entry(parent, struct binder_thread, rb_node);

        if (current->pid < thread->pid)
            p = &(*p)->rb_left;
        else if (current->pid > thread->pid)
            p = &(*p)->rb_right;
        else
            return thread;  // 已存在
    }

    // ── 2. 未找到 → 创建新线程 ──
    thread = kzalloc(sizeof(*thread), GFP_KERNEL);
    if (!thread)
        return ERR_PTR(-ENOMEM);

    thread->proc = proc;
    thread->pid = current->pid;  // 记录线程 PID
    init_waitqueue_head(&thread->wait);
    INIT_LIST_HEAD(&thread->todo);

    // ── 3. 插入红黑树 ──
    rb_link_node(&thread->rb_node, parent, p);
    rb_insert_color(&thread->rb_node, &proc->threads);

    // ── 4. 记录到 proc ──
    proc->ready_threads++;

    return thread;
}
```

### 5.2 内核的线程调度策略

```c
// binder_thread_read() 中的调度逻辑（简化）
static int binder_thread_read(struct binder_proc *proc,
                              struct binder_thread *thread,
                              ...)
{
    // ── 1. 优先处理线程级 todo ──
    if (!list_empty(&thread->todo)) {
        // 有本线程的待处理事务 → 直接处理
        goto done;
    }

    // ── 2. 处理进程级 todo ──
    while (!list_empty(&proc->todo)) {
        // 有空闲线程 → 只取自己的任务
        if (proc->ready_threads > 1) {
            // 有多个空闲线程，竞争获取
        }
        // 唯一空闲线程 → 获取 proc->todo 中的任务
        break;
    }

    // ── 3. 没有工作 → 阻塞等待 ──
    thread->looper |= BINDER_LOOPER_STATE_WAITING;
    proc->ready_threads++;

    ret = wait_event_interruptible(
        thread->wait,
        !list_empty(&thread->todo) ||
        !list_empty(&proc->todo));

    thread->looper &= ~BINDER_LOOPER_STATE_WAITING;
    proc->ready_threads--;

    // ── 4. 检查是否需要创建更多线程 ──
    if (proc->ready_threads == 0 &&
        proc->requested_threads < proc->max_threads) {
        // 所有线程都忙 → 请求创建新线程
        // 这个 work 会触发 BR_SPAWN_LOOPER
    }
}
```

---

## 六、主线程 vs 辅助线程

| 特性 | 主线程 (isMain=true) | 辅助线程 (isMain=false) |
|------|---------------------|------------------------|
| **注册命令** | `BC_ENTER_LOOPER` | `BC_REGISTER_LOOPER` |
| **内核标记** | `BINDER_LOOPER_STATE_ENTERED` | `BINDER_LOOPER_STATE_REGISTERED` |
| **超时退出** | 永不退出（无限循环） | 超时后可以退出 |
| **创建时机** | 进程启动时 | 收到 `BR_SPAWN_LOOPER` 时 |
| **典型场景** | SystemServer 主线程 | 服务端线程池扩容 |

---

## 七、SystemServer 启动示例

```
zygote
  │
  └── fork SystemServer 进程
        │
        ├── main() 中
        │    └── ProcessState::self()
        │         ├── open("/dev/binder")        ← binder_open
        │         └── mmap(1MB)                  ← binder_mmap
        │
        ├── 启动各种 Service
        │    ├── ActivityManagerService ← 内部创建 Binder 对象
        │    ├── PowerManagerService   ← 内部创建 Binder 对象
        │    ├── WindowManagerService  ← 内部创建 Binder 对象
        │    └── ... 等 80+ 个系统服务
        │
        ├── ServiceManager::addService() ← 注册到 ServiceManager
        │
        ├── IPCThreadState::self()
        │    └── joinThreadPool(true)    ← 主线程进入 Binder 循环
        │         │
        │         ├── BC_ENTER_LOOPER
        │         ├── while(1):
        │         │    ├── talkWithDriver() → 阻塞等待
        │         │    ├── BR_TRANSACTION
        │         │    │    └── execute remote request
        │         │    ├── BR_SPAWN_LOOPER
        │         │    │    └── spawnPooledThread(false)
        │         │    │         └── 新线程 joinThreadPool(false)
        │         │    │              ├── BC_REGISTER_LOOPER
        │         │    │              └── while(1)...
        │         │    └── BR_REPLY
        │         │         └── 处理回复
        │         │
        │         └── (永不退出)
        │
        └── 其他线程也通过 IPCThreadState::self()
             └── 自动获取 IPCThreadState 实例
```

---

## 八、线程池调优

### 8.1 自定义最大线程数

```cpp
// 可以在进程初始化时设置
ProcessState::self()->setThreadPoolMaxThreadCount(32);
```

### 8.2 默认值

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `DEFAULT_MAX_BINDER_THREADS` | 15 | Binder 线程池上限 |
| 内核硬限制 `BINDER_MAX_THREADS` | 16 | 内核最大限制 |
| mmap 大小 | 1MB - 2×page | 映射区大小 |

### 8.3 查看线程池状态

```bash
adb shell cat /d/binder/proc/<pid>
# 输出：
# proc 1234
# context binder
# threads: 5           ← 当前线程数
# requested threads: 3 ← 已请求的线程数
# ready threads: 1     ← 当前空闲线程数
# max threads: 15      ← 最大线程数
```

---

## 九、总结

```
Binder 线程池启动流程:

进程启动
  ├── ProcessState::self()
  │     ├── open("/dev/binder") → binder_open       (内核: 创建 binder_proc)
  │     └── mmap() → binder_mmap                    (内核: 分配缓冲区)
  │
  ├── IPCThreadState::self()
  │     └── TLS 线程局部存储每个线程绑定一个 IPCThreadState
  │
  └── IPCThreadState::joinThreadPool(true)
        ├── BC_ENTER_LOOPER                          (内核: 注册主线程)
        └── while(1):
              ├── talkWithDriver() → ioctl(BINDER_WRITE_READ)
              │                      └── binder_thread_read()
              │                            └── wait_event_interruptible() ← 阻塞
              │
              ├── BR_TRANSACTION  → executeCommand() → onTransact()
              ├── BR_REPLY        → 触发等待结果的线程
              ├── BR_SPAWN_LOOPER → spawnPooledThread()
              │                      └── pthread_create()
              │                           └── joinThreadPool(false) ← 新线程
              │                                └── BC_REGISTER_LOOPER
              │
              └── (循环继续)
```

**文件**：`frameworks/native/libs/binder/ProcessState.cpp`、`IPCThreadState.cpp`
