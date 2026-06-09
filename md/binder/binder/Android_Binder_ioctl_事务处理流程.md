# Android Binder ioctl 事务处理流程

## 目录

1. [概述](#一概述)
2. [支持的命令码](#二支持的命令码)
3. [binder_ioctl 入口函数](#三binder_ioctl-入口函数源码解析)
4. [BINDER_WRITE_READ 核心事务流程](#四binder_write_read--核心事务流程)
5. [binder_transaction 事务处理核心](#五binder_transaction--事务处理核心)
6. [事务处理完整时序图](#六事务处理流程完整时序图)
7. [BC/BR 命令完整列表](#七bcbr-命令完整列表)
8. [binder_thread 与 Looper 状态机](#八binder_thread-与-looper-状态机)
9. [关键设计要点](#九关键设计要点)
10. [完整函数调用链](#十完整函数调用链)
11. [总结](#十一总结)

---

## 一、概述

`binder_ioctl` 是 Binder 驱动的**命令分发入口**。用户空间所有的 Binder 操作 —— 发送事务、接收回复、注册死亡通知、进入/退出 Looper —— 全部通过 `ioctl(fd, cmd, arg)` 进入内核，由 `binder_ioctl` 统一派发。

### 函数原型

```c
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
```

| 参数 | 说明 |
|------|------|
| `filp` | binder 设备文件，通过 `filp->private_data` 可获取 `binder_proc` |
| `cmd` | ioctl 命令码 |
| `arg` | 用户空间传入的参数指针 |
| **返回值** | 0 成功，负数失败 |

---

## 二、支持的命令码

| 命令 | 值 | 说明 |
|------|-----|------|
| **`BINDER_WRITE_READ`** | `_IOWR('b', 1, struct binder_write_read)` | **核心命令**：发送/接收事务 |
| `BINDER_SET_MAX_THREADS` | `_IOW('b', 5, __u32)` | 设置进程最大 Binder 线程数 |
| `BINDER_SET_CONTEXT_MGR` | `_IOW('b', 7, __s32)` | 将当前进程注册为 ServiceManager |
| `BINDER_THREAD_EXIT` | `_IOW('b', 8, __s32)` | 通知内核 Binder 线程退出 |
| `BINDER_VERSION` | `_IOR('b', 9, struct binder_version)` | 获取 Binder 驱动版本 |
| `BINDER_GET_NODE_DEBUG_INFO` | `_IOWR('b', 11, ...)` | 获取 Binder 节点调试信息 |

---

## 三、binder_ioctl 入口函数源码解析

```c
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct binder_proc *proc = filp->private_data;
    struct binder_thread *thread;
    unsigned int size = _IOC_SIZE(cmd);
    void __user *ubuf = (void __user *)arg;
    int ret;

    // ── 1. 进入 Binder 上下文（跟踪锁状态） ──
    binder_inc_use(proc);

    // ── 2. 查找或创建当前线程的 binder_thread ──
    thread = binder_get_thread(proc);
    if (IS_ERR(thread)) {
        ret = PTR_ERR(thread);
        goto err;
    }

    // ── 3. 命令分发 ──
    switch (cmd) {

    case BINDER_WRITE_READ:
        ret = binder_ioctl_write_read(filp, cmd, arg, thread);
        if (ret)
            goto err;
        break;

    case BINDER_SET_MAX_THREADS: {
        int max_threads;
        if (copy_from_user(&max_threads, ubuf, sizeof(max_threads))) {
            ret = -EFAULT;
            goto err;
        }
        proc->max_threads = max_threads;
        break;
    }

    case BINDER_SET_CONTEXT_MGR:
        ret = binder_ioctl_set_ctx_mgr(filp, proc);
        if (ret)
            goto err;
        break;

    case BINDER_THREAD_EXIT:
        binder_free_thread(proc, thread);
        thread = NULL;
        break;

    case BINDER_VERSION: {
        struct binder_version vers;
        vers.protocol_version = BINDER_CURRENT_PROTOCOL_VERSION;
        if (copy_to_user(ubuf, &vers, sizeof(vers))) {
            ret = -EFAULT;
            goto err;
        }
        break;
    }

    default:
        ret = -EINVAL;
        goto err;
    }

    ret = 0;

err:
    if (thread)
        thread->looper_need_return = false;
    // 等待异步事务完成（调试追踪用）
    wait_event(proc->freeze_wait, proc->outstanding_txns == 0);
    binder_dec_use(proc);
    return ret;
}
```

---

## 四、`BINDER_WRITE_READ` — 核心事务流程

这是**最重要的命令**，所有 Binder 跨进程调用都通过它完成。它由两个部分组成：`binder_write`（发送）和 `binder_read`（接收）。

### 4.1 数据结构

```c
struct binder_write_read {
    // 写入（发送）
    unsigned long write_size;     // 写入缓冲区总大小
    unsigned long write_consumed; // 已消耗的写入字节数
    binder_uintptr_t write_buffer;// 用户空间写入缓冲区的地址

    // 读取（接收）
    unsigned long read_size;      // 读取缓冲区总大小
    unsigned long read_consumed;  // 已读取的字节数
    binder_uintptr_t read_buffer; // 用户空间读取缓冲区的地址
};
```

用户空间一次可以同时发送多个命令和接收多个回复。

### 4.2 命令流分层

```
用户空间                 内核                 远程进程
   │                     │                     │
   │ ioctl(BINDER_WRITE_READ, &bwr)           │
   │────────────────────►│                     │
   │                     │                     │
   │                     ├── binder_write():   │
   │                     │  BC_TRANSACTION     │── binder_transaction() ──►
   │                     │  BC_REPLY           │                         │
   │                     │  BC_FREE_BUFFER     │                         │
   │                     │  BC_INCREFS         │                         │
   │                     │  BC_ACQUIRE         │                         │
   │                     │  BC_RELEASE         │                         │
   │                     │  BC_REGISTER_LOOPER │                         │
   │                     │                     │                         │
   │                     ├── binder_read():    │◄── 接收回复或请求 ──────│
   │                     │  BR_TRANSACTION     │                         │
   │                     │  BR_REPLY           │                         │
   │                     │  BR_TRANSACTION_COMPLETE                     │
   │                     │  BR_DEAD_BINDER     │                         │
   │                     │  BR_SPAWN_LOOPER    │                         │
   │◄────────────────────┤                     │                         │
   │  (read_consumed > 0)│                     │
```

### 4.3 `binder_ioctl_write_read` 实现

```c
static int binder_ioctl_write_read(struct file *filp,
                                   unsigned int cmd, unsigned long arg,
                                   struct binder_thread *thread)
{
    struct binder_proc *proc = filp->private_data;
    struct binder_write_read bwr;
    int ret;

    // ── 1. 从用户空间拷贝 binder_write_read 结构 ──
    if (copy_from_user(&bwr, (void __user *)arg, sizeof(bwr))) {
        ret = -EFAULT;
        goto out;
    }

    // ── 2. 如果有待写入的命令 → 先处理写入 ──
    if (bwr.write_size > bwr.write_consumed) {
        ret = binder_write(proc, thread,
                          (const void __user *)(uintptr_t)bwr.write_buffer,
                          bwr.write_size - bwr.write_consumed,
                          &bwr.write_consumed);
        if (ret < 0 && ret != -EINTR) {
            // 严重错误，直接返回
            goto out;
        }
    }

    // ── 3. 如果有待读取的数据 → 处理读取 ──
    if (bwr.read_size > bwr.read_consumed) {
        ret = binder_read(proc, thread,
                         (void __user *)(uintptr_t)bwr.read_buffer,
                         bwr.read_size,
                         &bwr.read_consumed);
        if (ret < 0 && ret != -EINTR) {
            goto out;
        }
    }

    // ── 4. 将更新后的 bwr 写回用户空间 ──
    if (copy_to_user((void __user *)arg, &bwr, sizeof(bwr))) {
        ret = -EFAULT;
        goto out;
    }

out:
    return ret;
}
```

---

## 五、`binder_transaction` — 事务处理核心

这是 Binder 中**最复杂的函数**，负责将 BC_TRANSACTION 命令处理为一次完整的跨进程调用。

### 5.1 流程总览

```
binder_transaction()
    │
    ├── 1. 参数解析 ─────────────────────────
    │   从用户空间拷贝 binder_transaction_data
    │   获取目标 handle、code、数据等
    │
    ├── 2. 查找目标 ─────────────────────────
    │   handle == 0 → ServiceManager
    │   handle > 0  → proc->refs_by_desc 红黑树查找 binder_ref
    │                 ref->node 找到 binder_node
    │                 node->proc 找到目标进程
    │
    ├── 3. 安全校验 ─────────────────────────
    │   检查发送方 UID/PID
    │   检查目标节点是否存活
    │   检查上下文是否匹配（binder/hwbinder/vndbinder）
    │
    ├── 4. 分配缓冲区 ───────────────────────
    │   在目标进程的 mmap 映射区分配 binder_buffer
    │   binder_alloc_new_buf()
    │
    ├── 5. 数据拷贝 ─────────────────────────
    │   copy_from_user()
    │   将用户数据从发送方拷贝到目标进程的缓冲区
    │   （这是唯一的一次拷贝！）
    │
    ├── 6. 处理 Binder 对象 ────────────────
    │   遍历偏移数组，将 flat_binder_object 中的
    │   本地指针/句柄 转换为 远程引用/节点
    │
    ├── 7. 处理文件描述符 ──────────────────
    │   如果事务中包含 FD，执行 SCM 文件传递
    │
    ├── 8. 挂入目标队列 ────────────────────
    │   创建 binder_work 挂入 target_proc->todo
    │   或 target_thread->todo
    │
    └── 9. 唤醒目标线程 ────────────────────
        wake_up_interruptible(target_wait)
```

### 5.2 `binder_transaction_data` 结构

用户空间通过这个结构与内核交换事务数据：

```c
struct binder_transaction_data {
    union {
        __u32 handle;           // 目标服务句柄（Client 发送时使用）
        binder_uintptr_t ptr;   // Binder 对象地址（Server 回复时使用）
    } target;
    binder_uintptr_t cookie;    // 用户空间附加数据
    __u32 code;                 // 方法调用码（AIDL 方法 ID）

    // 权限
    __u32 sender_pid;           // 发送方 PID（内核填充）
    __u32 sender_euid;          // 发送方 UID（内核填充）

    __u32 data_size;            // 数据大小（bytes）
    __u32 offsets_size;         // 偏移数组大小（用于定位 flat_binder_object）

    union {
        struct {
            const void __user *buffer;     // 数据缓冲区
            const void __user *offsets;    // 偏移数组
        } ptr;
        __u8 buf[8];            // 小数据内联
    } data;
};
```

### 5.3 简化版源码流程

```c
static void binder_transaction(struct binder_proc *proc,
                               struct binder_thread *thread,
                               struct binder_transaction_data *tr,
                               int reply)
{
    struct binder_transaction *t;
    struct binder_work *tcomplete;
    struct binder_proc *target_proc;
    struct binder_thread *target_thread = NULL;
    struct binder_node *target_node = NULL;
    struct binder_ref *ref;
    struct binder_buffer *buffer;

    // ── 1. 查找目标进程和线程 ──
    if (reply) {
        // BC_REPLY：回复给调用方
        target_thread = thread->transaction_stack->from;
        target_proc = target_thread->proc;
    } else {
        // BC_TRANSACTION：查找目标服务
        ref = binder_get_ref(proc, tr->target.handle);
        target_node = ref->node;
        target_proc = target_node->proc;
    }

    // ── 2. 分配事务结构 ──
    t = kzalloc(sizeof(*t), GFP_KERNEL);
    tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);

    t->from = thread;
    t->to_proc = target_proc;
    t->need_reply = !tr->oneway;
    t->oneway = tr->oneway;
    t->sender_pid = proc->pid;
    t->sender_euid = current_euid().val;

    // ── 3. 在目标进程的 mmap 区分配缓冲区 ──
    buffer = binder_alloc_new_buf(&target_proc->alloc,
                                  tr->data_size,
                                  tr->offsets_size,
                                  extra_buffers_size);
    t->buffer = buffer;

    // ── 4. 拷贝数据（唯一一次拷贝！） ──
    copy_from_user(buffer->data, tr->data.ptr.buffer, tr->data_size);

    // ── 5. 处理 Binder 对象（句柄 ↔ 节点转换） ──
    off = (const size_t *)tr->data.ptr.offsets;
    for (int i = 0; i < tr->offsets_size / sizeof(size_t); i++) {
        struct flat_binder_object *fbo;
        fbo = buffer->data + off[i];
        switch (fbo->hdr.type) {
            case BINDER_TYPE_BINDER:
                // 发送方传过来的本地 Binder 对象
                // → 在目标进程创建 binder_ref
                break;
            case BINDER_TYPE_HANDLE:
                // 发送方传过来的远程句柄
                // → 转换为目标进程可用的句柄
                break;
            case BINDER_TYPE_FD:
                // 文件描述符 → SCM 传递
                break;
        }
    }

    // ── 6. 将工作项挂入队列 ──
    if (target_thread && target_thread->looper & BINDER_LOOPER_STATE_WAITING) {
        // 目标线程正在等待 → 直接挂到线程队列
        binder_enqueue_work(&t->work, &target_thread->todo);
        wake_up_interruptible(&target_thread->wait);
    } else {
        // 挂到进程队列，由线程池调度
        binder_enqueue_work(&t->work, &target_proc->todo);
        wake_up_interruptible(&target_proc->wait);
    }

    // ── 7. 在发送方线程挂一个 BR_TRANSACTION_COMPLETE ──
    binder_enqueue_work(tcomplete, &thread->todo);

    // ── 8. 对于 oneway 调用，追踪异步空间 ──
    if (tr->oneway)
        target_proc->outstanding_txns++;
}
```

---

## 六、事务处理流程完整时序图

### 6.1 同步调用（需要回复）

```
Client 线程                       内核                           Server 线程
   │                               │                               │
   │── ioctl(BWR, write=BC_TRANSACTION, target.handle)            │
   │──────────────────────────────►│                               │
   │                               │── binder_transaction()        │
   │                               │   - 查 ref → node → proc     │
   │                               │   - 分配 buffer（mmap）       │
   │                               │   - copy_from_user（一次拷贝） │
   │                               │   - 处理 Binder 对象转换      │
   │                               │                               │
   │                               │── 挂入 target_proc->todo     │
   │                               │── wake_up_interruptible()     │
   │                               │───────────────────────────►   │
   │                               │                               │── 线程被唤醒
   │  ioctl 阻塞等待               │                               │── BR_TRANSACTION
   │                               │                               │── 执行 onTransact()
   │                               │                               │── ioctl(write=BC_REPLY)
   │                               │◄────────────────────────────  │
   │                               │── binder_transaction(reply)   │
   │                               │   - 分配 buffer               │
   │                               │   - 拷贝回复数据              │
   │                               │   - 挂入 Client thread->todo │
   │                               │── wake_up_interruptible()      │
   │◄──────────────────────────────│                               │
   │  ioctl 返回                    │                               │
   │  BR_REPLY                      │                               │
   │  读取回复数据                  │                               │
```

### 6.2 异步调用（oneway，不需要回复）

```
Client 线程                       内核                           Server 线程
   │                               │                               │
   │── ioctl(BC_TRANSACTION, oneway=1)                             │
   │──────────────────────────────►│                               │
   │                               │── binder_transaction(oneway)  │
   │                               │   - 不需要回复                │
   │                               │   - 不阻塞                    │
   │                               │   - 挂入 proc->todo           │
   │                               │── wake_up_interruptible()      │
   │◄── BR_TRANSACTION_COMPLETE    │───────────────────────────►   │
   │  ioctl 立即返回               │                               │
   │                               │                               │── 处理后不回复
```

---

## 七、BC/BR 命令完整列表

### 7.1 `binder_write` 处理的 BC 命令（用户 → 内核）

| 命令 | 说明 |
|------|------|
| `BC_TRANSACTION` | 发起一次事务调用 |
| `BC_REPLY` | 回复一次事务 |
| `BC_FREE_BUFFER` | 释放 mmap 缓冲区 |
| `BC_INCREFS` | 增加弱引用计数 |
| `BC_ACQUIRE` | 增加强引用计数 |
| `BC_RELEASE` | 释放强引用 |
| `BC_DECREFS` | 释放弱引用 |
| `BC_ACQUIRE_NODE` | 通过节点指针增加引用 |
| `BC_RELEASE_NODE` | 通过节点指针释放引用 |
| `BC_REGISTER_LOOPER` | 注册一个 Binder 线程 |
| `BC_ENTER_LOOPER` | 进入 Binder Looper |
| `BC_EXIT_LOOPER` | 退出 Binder Looper |
| `BC_REQUEST_DEATH_NOTIFICATION` | 注册死亡通知 |
| `BC_CLEAR_DEATH_NOTIFICATION` | 清除死亡通知 |
| `BC_DEAD_BINDER_DONE` | 确认死亡通知已处理 |

### 7.2 `binder_read` 产生的 BR 命令（内核 → 用户）

| 命令 | 说明 |
|------|------|
| `BR_TRANSACTION` | 有传入事务需要处理 |
| `BR_REPLY` | 同步调用的回复已到达 |
| `BR_TRANSACTION_COMPLETE` | 事务已提交完成 |
| `BR_DEAD_BINDER` | Binder 服务端已死 |
| `BR_SPAWN_LOOPER` | 通知进程创建新 Binder 线程 |
| `BR_INCREFS` | 引用计数变化（已废弃） |
| `BR_ACQUIRE` | 引用计数变化（已废弃） |
| `BR_RELEASE` | 引用计数变化（已废弃） |
| `BR_DECREFS` | 引用计数变化（已废弃） |
| `BR_OK` | 操作成功 |
| `BR_ERROR` | 操作失败 |
| `BR_NOOP` | 无操作 |
| `BR_FAILURE_REPLY` | 回复失败 |

---

## 八、binder_thread 与 Looper 状态机

### 8.1 Looper 状态

```c
enum {
    BINDER_LOOPER_STATE_REGISTERED = 1 << 0,   // 已通过 BC_REGISTER_LOOPER 注册
    BINDER_LOOPER_STATE_ENTERED   = 1 << 1,     // 已通过 BC_ENTER_LOOPER 进入
    BINDER_LOOPER_STATE_WAITING   = 1 << 2,     // 正在等待事务
    BINDER_LOOPER_STATE_NEED_RETURN = 1 << 3,   // 需要返回（用于线程池管理）
};
```

### 8.2 线程状态转换

```
线程创建
  │
  │── BC_REGISTER_LOOPER   (手动注册线程)
  │      │
  │      ▼
  │  REGISTERED
  │      │
  │      ▼
  │  binder_read() 中等待 → WAITING
  │      │
  │      ├── BR_TRANSACTION → 处理事务 → 继续等待
  │      └── BR_SPAWN_LOOPER → 通知进程创建新线程
  │
  │── BC_ENTER_LOOPER   (主线程 / binder 自管理线程)
  │      │
  │      ▼
  │  ENTERED
  │      │
  │      ▼
  │  binder_read() 中等待 → WAITING
  │
  │── BC_EXIT_LOOPER / 线程退出
```

### 8.3 用户空间（C++）的线程池管理

```cpp
// frameworks/native/libs/binder/IPCThreadState.cpp

void IPCThreadState::joinThreadPool(bool isMain)
{
    // 1. 注册当前线程到内核
    if (isMain)
        ioctl(mProcess->mDriverFD, BC_ENTER_LOOPER, 0);
    else
        ioctl(mProcess->mDriverFD, BC_REGISTER_LOOPER, 0);

    // 2. 进入循环
    while (true) {
        // 3. 执行 ioctl BINDER_WRITE_READ，阻塞等待
        result = getAndExecuteCommand();

        // 4. 收到 BR_SPAWN_LOOPER → 创建新线程
        if (result == TIMED_OUT && !isMain)
            break;
    }

    // 5. 退出
    ioctl(mProcess->mDriverFD, BC_EXIT_LOOPER, 0);
}

status_t IPCThreadState::getAndExecuteCommand()
{
    // 发送 BC_ENTER_LOOPER / BC_REGISTER_LOOPER
    // 调用 talkWithDriver() → ioctl(BINDER_WRITE_READ)
    // 收到 BR_TRANSACTION → executeCommand()
    // 收到 BR_SPAWN_LOOPER → spawnPooledThread()
    // 收到 BR_REPLY → 唤醒等待的发起线程
}
```

---

## 九、关键设计要点

### 9.1 事务栈（transaction_stack）

`binder_thread` 中的 `transaction_stack` 是一个**单向链表**，用于追踪嵌套调用：

```
ServiceManager              SystemServer              App
    │                          │                      │
    │◄──── context.encode() ───│                      │
    │   (BC_TRANSACTION)       │                      │
    │                          │                      │
    │── BC_REPLY ────────────► │                      │
    │                          │── BC_TRANSACTION ──► │
    │                          │   (startActivity)    │
    │                          │                      │
    │                          │◄──── BC_REPLY ────── │
    │◄── BC_REPLY ────────────│                      │
```

事务栈结构（`from` / `from_parent` / `to_parent` 指针构成链表）。

### 9.2 缓存池管理

- **分配策略**：最佳适应（best-fit），从 `free_buffers` 红黑树查找最合适的空闲块
- **事务大小**：受 mmap 大小限制（默认 1MB - 2 page），超过返回 `TransactionTooLargeException`
- **异步空间**：独立分配，不超过 mmap 大小的一半（防止异步事务淹没目标）

### 9.3 安全性

- **sender_pid**：内核自动填充，用户空间不可伪造
- **sender_euid**：内核自动填充，即使用户空间传 0 也会被覆盖
- **权限校验**：Server 端通过 `Binder.getCallingPid()` / `Binder.getCallingUid()` 校验

---

## 十、完整函数调用链

```
ioctl(fd, BINDER_WRITE_READ, &bwr)
  └─ binder_ioctl()
       └─ binder_get_thread()            ← 查找/创建当前线程的 binder_thread
       └─ binder_ioctl_write_read()
            ├─ binder_write()
            │    └─ binder_thread_write()
            │         ├─ BC_TRANSACTION → binder_transaction()   ← 核心
            │         ├─ BC_REPLY       → binder_transaction()   ← 核心
            │         ├─ BC_FREE_BUFFER → binder_free_buffer()
            │         ├─ BC_REGISTER_LOOPER / BC_ENTER_LOOPER
            │         ├─ BC_REQUEST_DEATH_NOTIFICATION
            │         └─ BC_CLEAR_DEATH_NOTIFICATION
            │
            └─ binder_read()
                 └─ binder_thread_read()
                      ├─ BR_TRANSACTION  → 给用户处理
                      ├─ BR_REPLY        → 给用户处理
                      ├─ BR_DEAD_BINDER  → 通知用户
                      ├─ BR_SPAWN_LOOPER → 请求创建线程
                      └─ (等待) wait_event_interruptible()
```

---

## 十一、总结

```
binder_ioctl = Binder 驱动的命令总入口
                   │
                   ├── BINDER_WRITE_READ ← 核心命令
                   │       ├── binder_write()  → BC_TRANSACTION / BC_REPLY
                   │       │                      └── binder_transaction()
                   │       │                           ├── 查找目标进程
                   │       │                           ├── 分配 mmap buffer
                   │       │                           ├── 一次数据拷贝
                   │       │                           ├── Binder 对象转换
                   │       │                           └── 唤醒目标线程
                   │       │
                   │       └── binder_read()   → BR_TRANSACTION / BR_REPLY
                   │                               └── 阻塞等待 → 处理 → 返回
                   │
                   ├── BINDER_SET_MAX_THREADS
                   ├── BINDER_SET_CONTEXT_MGR  ← 注册 ServiceManager
                   ├── BINDER_THREAD_EXIT
                   └── BINDER_VERSION
```
