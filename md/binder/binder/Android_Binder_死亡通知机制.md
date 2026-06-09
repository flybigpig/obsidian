# Android Binder 死亡通知机制（Death Notification）

## 目录

1. [概述](#一概述)
2. [数据结构](#二数据结构)
3. [注册死亡通知流程](#三注册死亡通知流程)
4. [触发死亡通知流程](#四触发死亡通知流程)
5. [清理死亡通知流程](#五清理死亡通知流程)
6. [内核源码解析](#六内核源码解析)
7. [用户空间使用示例](#七用户空间使用示例)
8. [总结](#八总结)

---

## 一、概述

Binder 死亡通知机制用于**监控远程 Binder 服务端的存活状态**。当客户端持有某个远程服务的句柄（handle）时，如果服务端进程崩溃或被杀死，内核会自动通知客户端，让客户端可以做出相应处理（如重连、释放资源、提示用户）。

### 关键角色

| 角色 | 说明 |
|------|------|
| **服务端** | 提供 Binder 服务的进程，持有 `binder_node` |
| **客户端** | 调用远程服务的进程，持有 `binder_ref` |
| **内核** | 监控 `binder_node` 所在进程存活状态，死亡时生成 `BR_DEAD_BINDER` |
| **Death Recipient** | 客户端注册的死亡通知回调对象 |

### 触发条件

```
服务端进程:
  正常退出  → binder_release() → binder_deferred_release()
  被杀死      → 内核清理进程资源 → binder_release()
  崩溃        → 同上

内核检测到服务端进程死亡:
  1. 遍历所有引用此 node 的 binder_ref
  2. 为每个注册了死亡通知的引用生成 BR_DEAD_BINDER
  3. 将死亡通知挂到客户端线程的 todo 队列
  4. 客户端下次 ioctl 时收到 BR_DEAD_BINDER
```

---

## 二、数据结构

### 2.1 binder_ref_death — 死亡通知核心结构

```c
// drivers/android/binder_internal.h
struct binder_ref_death {
    // 死亡通知工作项（挂到线程 todo 队列）
    struct binder_work work;

    // 用户空间传入的死亡通知 cookie
    // 在收到 BR_DEAD_BINDER 时原样返回给用户
    binder_uintptr_t cookie;
};
```

### 2.2 binder_ref 中的 death 字段

```c
struct binder_ref {
    // ...
    struct binder_ref_data data;
    // ...

    // ── 死亡通知 ──
    // 如果 != NULL，表示此 ref 注册了死亡通知
    struct binder_ref_death *death;
};
```

---

## 三、注册死亡通知流程

### 3.1 总体流程

```
客户端进程                         内核                        服务端进程
   │                               │                              │
   │ linkToDeath(recipient)        │                              │
   │                               │                              │
   │── BC_REQUEST_DEATH_NOTIFICATION                              │
   │   target.handle = X           │                              │
   │   cookie = recipient_ptr      │                              │
   │──────────────────────────────►│                              │
   │                               │                              │
   │                               │  1. 查找 ref(desc=X)        │
   │                               │  2. 创建 binder_ref_death   │
   │                               │     death->cookie = cookie  │
   │                               │     death->work.type =      │
   │                               │     BINDER_WORK_DEAD_BINDER │
   │                               │  3. ref->death = death      │
   │                               │  4. 增加 node 的弱引用计数   │
   │                               │     （防止 node 先被释放）   │
   │                               │                              │
   │◄── BR_DEAD_BINDER ───────────│                              │
   │   (如果没有立即返回 OK，       │                              │
   │    则后续服务死亡时才会收到)   │                              │
```

### 3.2 用户空间 API

```cpp
// frameworks/native/libs/binder/BpBinder.cpp

status_t BpBinder::linkToDeath(const sp<DeathRecipient>& recipient,
                               void* cookie, uint32_t flags)
{
    // ── 1. 构造死亡通知参数 ──
    Parcel data;
    data.writeStrongBinder(
        IInterface::asBinder(recipient));

    // ── 2. 发送 BC_REQUEST_DEATH_NOTIFICATION ──
    //     handle 是 BpBinder 持有的句柄号
    status_t status = IPCThreadState::self()->requestDeathNotification(
        mHandle, recipient);

    // ── 3. 存储 recipient（用于回调时查找） ──
    AutoMutex _l(mDeathRecipientsLock);
    if (mDeathRecipients == nullptr)
        mDeathRecipients = new DeathRecipientList;
    mDeathRecipients->add(recipient);
    // ...

    return status;
}
```

### 3.3 IPCThreadState 发送请求

```cpp
// frameworks/native/libs/binder/IPCThreadState.cpp

status_t IPCThreadState::requestDeathNotification(int32_t handle,
                                                   const sp<DeathRecipient>& recipient)
{
    status_t result = -EINTR;

    // ── 写入 BC_REQUEST_DEATH_NOTIFICATION 命令 ──
    mOut.writeInt32(BC_REQUEST_DEATH_NOTIFICATION);
    mOut.writeInt32((int32_t)handle);        // 目标句柄
    mOut.writePointer((uintptr_t)recipient.get());  // cookie = recipient 指针

    // ── 发送给驱动 ──
    result = talkWithDriver();
    return result;
}
```

---

## 四、触发死亡通知流程

### 4.1 服务端进程死亡时内核处理

```
服务端进程死亡（进程被 kill/崩溃）
   │
   │── 进程退出 → close(fd) → binder_release()
   │
   ▼
binder_deferred_release(proc)
   │
   ├── 遍历 proc->nodes 红黑树
   │    │
   │    ├── 对每个 node:
   │    │    │
   │    │    ├── 遍历 node->refs 链表
   │    │    │    │
   │    │    │    ├── 对于每个 ref:
   │    │    │    │    │
   │    │    │    │    ├── if (ref->death != NULL)
   │    │    │    │    │    │
   │    │    │    │    │    ├── 创建 binder_work(BR_DEAD_BINDER)
   │    │    │    │    │    ├── work->cookie = ref->death->cookie
   │    │    │    │    │    ├── 挂入 ref->proc->todo
   │    │    │    │    │    │    (即客户端进程的待处理队列)
   │    │    │    │    │    ├── 唤醒客户端进程
   │    │    │    │    │    └── wake_up_interruptible()
   │    │    │    │    │
   │    │    │    │    └── 清理 ref（删除 ref->death）
   │    │    │    │
   │    │    │    └── ...
   │    │    │
   │    │    └── 清理 node
   │    │
   │    └── ...
   │
   ├── 清理 proc->refs_by_desc
   ├── 清理 proc->refs_by_node
   └── 清理 proc->buffer（释放 mmap 内存）
```

### 4.2 客户端收到 BR_DEAD_BINDER

```
客户端进程
   │
   │── ioctl(BINDER_WRITE_READ) → 阻塞等待
   │
   ▼
binder_thread_read() → 发现 thread->todo 或 proc->todo 有工作
   │
   ├── 取出 binder_work
   │    type = BINDER_WORK_DEAD_BINDER
   │
   ├── 生成 BR_DEAD_BINDER
   │    data = ref->death->cookie (即 recipient 指针)
   │
   ▼
返回用户空间
   │
   ├── executeCommand(BR_DEAD_BINDER)
   │    │
   │    ├── cookie 即为 DeathRecipient* 指针
   │    ├── 调用 recipient->binderDied(who)
   │    │    (who 是死亡的 BpBinder)
   │    │
   │    └── 发送 BC_DEAD_BINDER_DONE
   │         (告知内核，死亡通知已处理，可以释放相关资源)
```

### 4.3 IPCThreadState 执行命令

```cpp
// IPCThreadState::executeCommand()

case BR_DEAD_BINDER:
{
    // ── 1. 从回复中读取 cookie ──
    //     即注册时传入的 recipient 指针
    BpBinder *proxy = (BpBinder*)mIn.readPointer();
    sp<IBinder::DeathRecipient> recipient;
    // ... 从缓存的 DeathRecipientList 中找到对应 recipient ...

    // ── 2. 回调用户注册的 binderDied() ──
    //     这通常触发客户端重连或释放资源
    recipient->binderDied(proxy);

    // ── 3. 发送 BC_DEAD_BINDER_DONE ──
    //     告知内核可以清理死亡通知相关资源了
    mOut.writeInt32(BC_DEAD_BINDER_DONE);
    mOut.writePointer((uintptr_t)cookie);
    talkWithDriver(false);

    break;
}
```

---

## 五、清理死亡通知流程

### 5.1 BC_CLEAR_DEATH_NOTIFICATION

客户端也可以在服务端死亡前主动取消死亡通知：

```cpp
status_t BpBinder::unlinkToDeath(const sp<DeathRecipient>& recipient,
                                 void* cookie, uint32_t flags)
{
    // ── 1. 从列表中移除 recipient ──
    mDeathRecipients->remove(recipient);

    // ── 2. 发送 BC_CLEAR_DEATH_NOTIFICATION ──
    status_t status = IPCThreadState::self()->clearDeathNotification(
        mHandle, recipient.get());

    // ── 3. 确认清理完成 ──
    //     等待 BR_CLEAR_DEATH_NOTIFICATION_DONE

    return status;
}
```

### 5.2 死亡通知完整生命周期

```
注册死亡通知
   │
   │── BC_REQUEST_DEATH_NOTIFICATION
   │── 内核: ref->death = death
   │── 内核: node->local_weak_refs++（防止 node 提前释放）
   │
   ├── 服务端一直存活
   │    └── 客户端主动取消:
   │         BC_CLEAR_DEATH_NOTIFICATION
   │         内核: ref->death = NULL
   │         内核: 清理 death 结构体
   │
   └── 服务端死亡
        │
        ├── 内核: 遍历 refs → 生成 BR_DEAD_BINDER
        ├── 内核: 挂入 client->todo 队列
        │
        ├── 客户端收到 BR_DEAD_BINDER
        │    ├── recipient->binderDied()
        │    └── BC_DEAD_BINDER_DONE
        │
        └── 内核收到 BC_DEAD_BINDER_DONE
             ├── 释放 death 结构体
             └── 释放 node 弱引用
```

---

## 六、内核源码解析

### 6.1 BC_REQUEST_DEATH_NOTIFICATION 处理

```c
// drivers/android/binder.c — binder_thread_write()

case BC_REQUEST_DEATH_NOTIFICATION:
{
    struct binder_ref *ref;
    struct binder_ref_death *death;
    binder_uintptr_t cookie;

    // ── 1. 读取参数 ──
    target_handle = *(u32 *)(ptr); ptr += sizeof(u32);
    cookie = *(binder_uintptr_t *)(ptr); ptr += sizeof(binder_uintptr_t);

    // ── 2. 查找 ref ──
    ref = binder_get_ref(proc, target_handle);

    if (ref == NULL) {
        // 句柄无效
        break;
    }

    // ── 3. 只能注册一次死亡通知 ──
    if (ref->death) {
        // 已经注册过 → 返回 BR_OK
        break;
    }

    // ── 4. 创建 death 结构体 ──
    death = kzalloc(sizeof(*death), GFP_KERNEL);
    if (death == NULL) {
        // 返回 BR_ERROR
        break;
    }

    // ── 5. 初始化 ──
    INIT_LIST_HEAD(&death->work.entry);
    death->work.type = BINDER_WORK_DEAD_BINDER;
    death->cookie = cookie;

    // ── 6. 绑定到 ref ──
    ref->death = death;

    // ── 7. 增加 node 弱引用计数（防止 node 提前释放） ──
    //     确保服务端死亡时我们仍然能找到 node
    ref->node->local_weak_refs++;

    // ── 8. 如果目标 node 已经死亡（服务端已先于注册挂了） ──
    if (ref->node->proc == NULL) {
        // node 已死亡，立即触发死亡通知
        // 将 work 挂到当前线程的 todo 队列
        binder_enqueue_work(&death->work, &thread->todo);
    }

    break;
}
```

### 6.2 服务端死亡时的内核处理

```c
// drivers/android/binder.c — binder_deferred_release()

static void binder_deferred_release(struct binder_proc *proc)
{
    struct hlist_node *tmp;
    struct binder_node *node;
    struct binder_ref *ref;
    struct rb_node *n;

    // ── 1. 释放所有节点 ──
    while ((n = rb_first(&proc->nodes))) {
        node = rb_entry(n, struct binder_node, rb_node);

        // ── 2. 遍历引用此 node 的所有 ref ──
        while (!list_empty(&node->refs)) {
            ref = list_first_entry(&node->refs,
                                   struct binder_ref, node_entry);
            // ── 3. 如果有死亡通知 ──
            if (ref->death) {
                // 将死亡通知挂到客户端 todo 队列
                binder_enqueue_work(
                    &ref->death->work,
                    &ref->proc->todo);
                // 唤醒客户端进程
                wake_up_interruptible(&ref->proc->wait);
            }
            // ── 4. 删除引用 ──
            binder_cleanup_ref(ref);
        }

        // ── 5. 删除 node ──
        rb_erase(&node->rb_node, &proc->nodes);
        binder_free_node(node);
    }

    // ── 6. 释放缓冲区 ──
    binder_alloc_deferred_release(&proc->alloc);
    // ...
}
```

### 6.3 BC_DEAD_BINDER_DONE 处理

```c
// drivers/android/binder.c — binder_thread_write()

case BC_DEAD_BINDER_DONE:
{
    struct binder_work *w;
    binder_uintptr_t cookie;

    // ── 1. 读取 cookie ──
    cookie = *(binder_uintptr_t *)(ptr); ptr += sizeof(binder_uintptr_t);

    // ── 2. 查找对应的 death work ──
    list_for_each_entry(w, &proc->delivered_death, entry) {
        struct binder_ref_death *death =
            container_of(w, struct binder_ref_death, work);

        if (death->cookie == cookie) {
            // ── 3. 从已送达列表中移除 ──
            list_del_init(&death->work.entry);

            // ── 4. 释放死亡通知结构体 ──
            kfree(death);
            break;
        }
    }
    break;
}
```

---

## 七、用户空间使用示例

### 7.1 Java 层（Android Framework）

```java
// 客户端使用示例
public class MyActivity extends Activity {

    private IRemoteService mService;
    private ServiceConnection mConnection = ...;

    // 死亡通知回调
    private IBinder.DeathRecipient mDeathRecipient =
        new IBinder.DeathRecipient() {
            @Override
            public void binderDied() {
                Log.e(TAG, "Remote service died!");

                // ── 死亡后的处理 ──
                // 1. 移除死亡通知
                if (mService != null) {
                    mService.asBinder().unlinkToDeath(this, 0);
                    mService = null;
                }

                // 2. 尝试重连
                bindService(new Intent(MyActivity.this,
                                       RemoteService.class),
                            mConnection, Context.BIND_AUTO_CREATE);
            }
        };

    void connectToService() {
        Intent intent = new Intent(this, RemoteService.class);
        bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
    }

    // ServiceConnection 回调
    private ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mService = IRemoteService.Stub.asInterface(service);

            try {
                // ── 注册死亡通知 ──
                service.linkToDeath(mDeathRecipient, 0);
            } catch (RemoteException e) {
                // 服务已死
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            // 服务断开（不一定死亡，也可能是正常 unbind）
            mService = null;
        }
    };
}
```

### 7.2 Native 层（C++）

```cpp
// Native 层使用示例
class MyDeathRecipient : public IBinder::DeathRecipient {
public:
    void binderDied(const wp<IBinder>& who) override {
        ALOGE("Remote service died!");

        // ── 重连逻辑 ──
        sp<IServiceManager> sm = defaultServiceManager();
        sp<IBinder> binder = sm->getService(String16("my_service"));

        if (binder != nullptr) {
            // 重新注册死亡通知
            binder->linkToDeath(this);
            mService = interface_cast<IMyService>(binder);
        }
    }
};

void connect() {
    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder = sm->getService(String16("my_service"));

    if (binder != nullptr) {
        // ── 注册死亡通知 ──
        sp<MyDeathRecipient> recipient = new MyDeathRecipient();
        binder->linkToDeath(recipient);

        mService = interface_cast<IMyService>(binder);
    }
}
```

---

## 八、总结

```
死亡通知 = Binder 的"心跳检测"机制
      │
      ├── 注册: linkToDeath()
      │     └── BC_REQUEST_DEATH_NOTIFICATION
      │          └── 内核: ref->death = death
      │               node->local_weak_refs++
      │
      ├── 触发: 服务端进程死亡
      │     └── binder_deferred_release()
      │          └── 遍历 node->refs
      │               └── 对有 death 的 ref:
      │                    BR_DEAD_BINDER → 挂入 client->todo
      │                    wake_up_interruptible()
      │
      ├── 处理: 客户端收到 BR_DEAD_BINDER
      │     ├── recipient->binderDied()
      │     └── BC_DEAD_BINDER_DONE
      │          └── 内核释放 death 结构体
      │
      └── 取消: unlinkToDeath()
            └── BC_CLEAR_DEATH_NOTIFICATION
                 └── 内核: ref->death = NULL
                      node->local_weak_refs--
```

| 命令 | 方向 | 作用 |
|------|------|------|
| `BC_REQUEST_DEATH_NOTIFICATION` | 用户 → 内核 | 注册死亡通知 |
| `BC_CLEAR_DEATH_NOTIFICATION` | 用户 → 内核 | 取消死亡通知 |
| `BC_DEAD_BINDER_DONE` | 用户 → 内核 | 确认已处理死亡通知 |
| `BR_DEAD_BINDER` | 内核 → 用户 | 通知服务端已死亡 |
| `BR_CLEAR_DEATH_NOTIFICATION_DONE` | 内核 → 用户 | 确认取消完成 |

**文件**：
- `drivers/android/binder.c`
- `frameworks/native/libs/binder/BpBinder.cpp`
- `frameworks/native/libs/binder/IPCThreadState.cpp`
