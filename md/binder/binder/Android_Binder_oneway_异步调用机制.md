# Android Binder oneway 异步调用机制

## 目录

1. [概述](#一概述)
2. [oneway 的定义](#二oneway-的定义)
3. [内核处理流程](#三内核处理流程)
4. [异步空间管理](#四异步空间管理)
5. [用户空间行为差异](#五用户空间行为差异)
6. [典型使用场景](#六典型使用场景)
7. [总结](#七总结)

---

## 一、概述

在 Binder IPC 中，oneway 是一种**异步调用模式**：

- **同步调用**：Client 发送事务后阻塞等待 Server 回复
- **oneway 调用**：Client 发送事务后立即返回，不需要 Server 回复

```
同步: Client ─── BC_TRANSACTION ──► Server
                  等待...           处理...
              ◄── BC_REPLY ───────
              （Client 此时才返回）

oneway: Client ─── BC_TRANSACTION ──► Server
              ◄── BR_TRANSACTION_COMPLETE
              （立即返回，不等待）
```

---

## 二、oneway 的定义

### 2.1 AIDL 中的 oneway

```java
// 接口级别 oneway — 接口中所有方法都是异步
oneway interface IRemoteService {
    void doSomething(int a);       // 自动 oneway
    void doSomethingElse(int b);   // 自动 oneway
}

// 方法级别 oneway — 只有特定方法是异步
interface IRemoteService {
    oneway void fireAndForget(int a);  // 异步
    int getResult();                    // 同步（默认）
}
```

### 2.2 事务标志

```c
// frameworks/native/libs/binder/IBinder.h
enum {
    FLAG_ONEWAY = 0x01  // 事务标志：oneway 调用
};
```

### 2.3 Parcel 中的设置

```cpp
// 框架层在使用 oneway 时设置标志
status_t BpBinder::transact(uint32_t code, const Parcel& data,
                            Parcel* reply, uint32_t flags)
{
    // flags 中包含 FLAG_ONEWAY
    status_t status = IPCThreadState::self()->transact(
        mHandle, code, data, reply, flags);
    return status;
}
```

---

## 三、内核处理流程

### 3.1 oneway 事务的内核分发

```c
// binder_transaction() 中的关键差异

static void binder_transaction(struct binder_proc *proc,
                               struct binder_thread *thread,
                               struct binder_transaction_data *tr,
                               int reply, struct binder_work *tcomplete)
{
    struct binder_transaction *t;
    struct binder_work *tcomplete;

    t = kzalloc(sizeof(*t), GFP_KERNEL);
    // ...

    // ── 关键：need_reply 标志 ──
    if (tr->flags & TF_ONE_WAY)
        t->need_reply = 0;    // oneway：不需要回复
    else
        t->need_reply = 1;    // 同步：需要回复

    // ── 目标线程选择策略不同 ──
    if (t->need_reply) {
        // 同步：找到调用栈上的目标线程
        // 必须由特定线程回复
        target_thread = thread->transaction_stack->from;
        target_proc = target_thread->proc;
    } else {
        // oneway：任意空闲线程都可以处理
        // 通常挂到 proc->todo，由线程池调度
        target_thread = NULL;
        target_proc = target_node->proc;
    }

    // ── 缓冲区分配 ──
    // oneway 使用独立的空间配额
    buffer = binder_alloc_new_buf(&target_proc->alloc,
                                  tr->data_size,
                                  tr->offsets_size,
                                  extra_buffers_size,
                                  !t->need_reply  // async 标志
                                  );

    // ── 挂入目标队列 ──
    if (target_thread &&
        target_thread->looper & BINDER_LOOPER_STATE_WAITING) {
        // 同步：挂到特定线程
        binder_enqueue_work(&t->work, &target_thread->todo);
        wake_up_interruptible(&target_thread->wait);
    } else {
        // oneway：挂到进程队列，由线程池竞争处理
        binder_enqueue_work(&t->work, &target_proc->todo);
        wake_up_interruptible(&target_proc->wait);
    }

    // ── 给发送方一个完成确认 ──
    tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
    binder_enqueue_work(tcomplete, &thread->todo);
}
```

### 3.2 oneway 的回复路径

```
同步事务的回复路径:
Client                    内核                    Server
   │                       │                       │
   │── BC_TRANSACTION ────►│                       │
   │                       │── 挂入 thread->todo   │
   │                       ├──────────────────────►│
   │                       │                       │── onTransact()
   │                       │◄── BC_REPLY ──────────│
   │                       │── 挂入 Client thread  │
   │◄── BR_REPLY ──────────│                       │
   │  (返回给 Client)       │                       │

oneway 事务的回复路径:
Client                    内核                    Server
   │                       │                       │
   │── BC_TRANSACTION ────►│                       │
   │   (FLAG_ONEWAY)       │── 挂入 proc->todo     │
   │                       ├──────────────────────►│
   │◄── BR_TRANSACTION_COMPLETE                    │
   │  (立即返回)            │                       │── onTransact()
   │                       │                       │  (不需要回复)
```

### 3.3 BR_TRANSACTION_COMPLETE 的生成

```c
// binder_thread_read() 中生成 BR_TRANSACTION_COMPLETE

struct binder_work *w = list_first_entry(&thread->todo,
                                         struct binder_work, entry);

switch (w->type) {

case BINDER_WORK_TRANSACTION_COMPLETE:
{
    // ── 发送 BR_TRANSACTION_COMPLETE 给用户空间 ──
    cmd = BR_TRANSACTION_COMPLETE;
    if (copy_to_user(ptr, &cmd, sizeof(cmd))) {
        return -EFAULT;
    }
    ptr += sizeof(cmd);

    // ── 从 todo 队列中移除 ──
    list_del(&w->entry);
    kfree(w);
    break;
}

case BINDER_WORK_TRANSACTION:
{
    // ── 发送 BR_TRANSACTION（包含数据） ──
    // ...
    break;
}
// ...
}
```

---

## 四、异步空间管理

### 4.1 空间配额

oneway 事务使用**独立的异步空间配额**，防止异步事务无限堆积淹没目标进程：

```c
// binder_mmap() 中初始化
proc->free_async_space = proc->buffer_size / 2;  // 异步空间 = mmap 大小的一半
```

### 4.2 分配时检查

```c
// binder_alloc_new_buf() 中
if (async) {
    // ── 检查异步空间是否充足 ──
    if (alloc->free_async_space < size) {
        binder_alloc_debug(BINDER_DEBUG_BUFFER_ALLOC,
                           "%d: binder_alloc_buf size %zd failed, no async space left\n",
                           alloc->pid, size);
        return ERR_PTR(-ENOSPC);
    }

    // ── 扣除异步空间 ──
    alloc->free_async_space -= size;
    alloc->allocated_async_space += size;
}
```

### 4.3 释放时归还

```c
void binder_alloc_free_buf(struct binder_alloc *alloc,
                           struct binder_buffer *buffer)
{
    if (buffer->async) {
        // ── 归还异步空间 ──
        alloc->free_async_space += buffer->free_space +
                                   sizeof(struct binder_buffer);
        alloc->allocated_async_space -= buffer->free_space +
                                        sizeof(struct binder_buffer);

        binder_alloc_debug(BINDER_DEBUG_BUFFER_ALLOC,
                           "%d: binder_alloc_buf buf %p "
                           "async free space size %zu\n",
                           alloc->pid, buffer,
                           alloc->free_async_space);
    }
    // ...
}
```

### 4.4 异步空间耗尽时的行为

当异步空间耗尽时：

```c
// binder_transaction() 中处理异步空间不足

if (oneway) {
    // ── 异步空间不足 → 返回 BR_FAILED_REPLY ──
    return_error = BR_FAILED_REPLY;
    return_error_param = -ENOSPC;
    goto err_alloc_buf_struct_failed;
}
```

用户空间收到 `BR_FAILED_REPLY` 后，`transact()` 返回 `FAILED_TRANSACTION` 错误。

### 4.5 异步空间监控

```c
// binder_thread_read() 中检查异步事务堆积

if (proc->outstanding_txns > 0) {
    // 等待异步事务完成
    // 防止进程退出时还有未完成的异步事务
    wait_event(proc->freeze_wait,
               !atomic_read(&proc->outstanding_txns));
}
```

---

## 五、用户空间行为差异

### 5.1 transact() 的行为

```cpp
// IPCThreadState::transact()

status_t IPCThreadState::transact(int32_t handle,
                                  uint32_t code, const Parcel& data,
                                  Parcel* reply, uint32_t flags)
{
    status_t err;

    // ── 写入 BC_TRANSACTION ──
    err = writeTransactionData(BC_TRANSACTION, flags,
                               handle, code, data, nullptr);

    if (err != NO_ERROR) {
        if (reply) reply->setError(err);
        return (mLastError = err);
    }

    if ((flags & TF_ONE_WAY) != 0) {
        // ── oneway：不需要 reply ──
        // 发送后立即返回，不等待
        err = waitForResponse(nullptr, nullptr);
    } else {
        // ── 同步：需要 reply ──
        // 阻塞等待回复
        err = waitForResponse(reply, nullptr);
    }

    return err;
}
```

### 5.2 waitForResponse 的差异

```cpp
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    uint32_t cmd;
    int32_t err;

    while (1) {
        // ── 与驱动通信 ──
        if ((err = talkWithDriver()) < NO_ERROR) break;

        // ── 处理返回的命令 ──
        cmd = (uint32_t)mIn.readInt32();

        switch (cmd) {

        case BR_TRANSACTION_COMPLETE:
            // ── oneway：收到这个就完成了 ──
            if (!reply && !acquireResult)
                goto finish;  // oneway → 直接返回
            break;

        case BR_REPLY:
            // ── 同步：收到回复数据 ──
            binder_transaction_data tr;
            mIn.read(&tr, sizeof(tr));
            if (reply) {
                // 填充 reply 并返回
            }
            goto finish;

        case BR_DEAD_BINDER:
            // ── 服务端已死 ──
            err = DEAD_OBJECT;
            goto finish;

        // ...
        }
    }

finish:
    // oneway 从这里跳出循环
    // 同步从这里跳出循环
    return err;
}
```

### 5.3 超时行为

```cpp
// oneway 调用没有超时概念（不会阻塞）
// 但是 talkWithDriver() 本身可能被信号中断

// 同步调用有隐含的"超时"机制：
// 如果服务端一直不回复，Client 会一直阻塞
// 在 ANR 检测中，如果 Binder 调用超过 5 秒
// 系统会弹出 ANR 对话框
```

---

## 六、典型使用场景

### 6.1 适用的场景

| 场景 | 示例 | 说明 |
|------|------|------|
| 通知/事件 | `onSensorChanged()` | 传感器事件通知，不需要确认 |
| 日志 | `logToSystem()` | 写入日志，不需要知道何时完成 |
| 广播 | `sendBroadcast()` | 发送广播，不等待接收者处理完 |
| 快速返回 | `scheduleBackgroundWork()` | 通知后台执行，调用方继续运行 |

### 6.2 不适用的场景

| 场景 | 示例 | 原因 |
|------|------|------|
| 获取数据 | `getServiceName()` | 需要返回值 |
| 关键操作 | `writeDatabase()` | 需要确保写入成功 |
| 顺序依赖 | `startActivity()` | 需要知道启动结果 |

### 6.3 SystemServer 中的 oneway 使用

```java
// ActivityManagerService 中的 oneway 调用
// 通过 schedule 机制将同步调用转为 oneway

// 用户空间调用:
// 前台线程调用 ams.startActivity(...) ← 同步

// AMS 内部:
// → ActivityManagerService.startActivity()
//   → 验证权限等快速操作（同步）
//   → mHandler.post(() -> {
//         // 耗时操作通过 oneway 异步执行
//         mBinder.scheduleStartActivity(...)  // oneway
//     })
//   → 立即返回给调用方

// 这样既保证了关键权限检查是同步的
// 又避免了调用方长时间阻塞
```

### 6.4 Binder 调用中 oneway 的使用建议

```java
public class MyService extends IService.Stub {

    // ✅ 适合用 oneway：通知类型
    @Override
    public void onEvent(int eventType, Bundle data) {
        // 快速处理，不阻塞
    }

    // ❌ 不能用 oneway：需要返回值
    @Override
    public int queryData(String key) {
        // 必须同步返回结果
        return mCache.get(key);
    }

    // ✅ 适合 oneway：耗时操作分离
    @Override
    public void scheduleHeavyWork(int taskId) {
        // 放入后台线程执行
        mExecutor.execute(() -> {
            doHeavyWork(taskId);
        });
        // 立即返回
    }
}
```

---

## 七、总结

```
oneway 异步调用
      │
      ├── 同步 vs oneway
      │     ├── 同步: BC_TRANSACTION → 等待 → BR_REPLY
      │     └── oneway: BC_TRANSACTION → BR_TRANSACTION_COMPLETE（立即返回）
      │
      ├── 内核差异
      │     ├── need_reply = 0（不需要回复）
      │     ├── 挂入 proc->todo（线程池调度，不特定线程）
      │     ├── 使用异步空间配额（buffer_size / 2）
      │     └── BR_TRANSACTION_COMPLETE 确认
      │
      ├── 内存保护
      │     ├── free_async_space 独立配额
      │     ├── 耗尽时返回 BR_FAILED_REPLY
      │     └── 防止异步事务淹没目标进程
      │
      └── 使用指南
            ├── ✅ 适合: 通知、事件、日志、后台调度
            └── ❌ 不适合: 需要返回值、关键操作、顺序依赖
```

| 对比项 | 同步调用 | oneway 调用 |
|--------|---------|------------|
| **标志** | `flags=0` | `flags=FLAG_ONEWAY` |
| **阻塞** | 发送方阻塞等待回复 | 发送方立即返回 |
| **回复** | 必须有 `BC_REPLY` | 只有 `BR_TRANSACTION_COMPLETE` |
| **目标线程** | 挂到特定线程（调用栈上的） | 挂到进程队列（任意线程） |
| **内存配额** | 共享全部 mmap 空间 | 最大 mmap 的一半 |
| **超时** | 可能触发 ANR（5s） | 不会阻塞，无 ANR |
| **AIDL 定义** | 默认 | `oneway` 关键字 |
| **返回值** | 可以有返回值 | 只能是 `void` |

**文件**：
- `drivers/android/binder.c` — `binder_transaction()` 中的 oneway 处理
- `drivers/android/binder_alloc.c` — 异步空间管理
- `frameworks/native/libs/binder/IPCThreadState.cpp` — `waitForResponse()`
