# Android ServiceManager 注册与查询流程

## 目录

1. [概述](#一概述)
2. [ServiceManager 的启动](#二servicemanager-的启动)
3. [服务注册流程](#三服务注册流程)
4. [服务查询流程](#四服务查询流程)
5. [Context Manager 机制](#五context-manager-机制)
6. [完整时序图](#六完整时序图)
7. [总结](#七总结)

---

## 一、概述

ServiceManager 是 Android Binder 架构中的**中央注册中心**（俗称"黄页"），负责：

- **注册**：服务端进程将 Binder 服务以（名称, 句柄）的形式注册到 ServiceManager
- **查询**：客户端进程通过服务名称查询，获取服务对应的句柄
- **管理**：监控服务进程存活状态，服务死亡时自动清理

### 关键特点

- ServiceManager 自身是一个 Binder 服务，其 **handle 固定为 0**
- 运行在独立的进程 `/system/bin/servicemanager` 中
- 使用 Binder 协议的特殊命令 `SVC_MGR_`（非 AIDL，而是自定义协议）

---

## 二、ServiceManager 的启动

### 2.1 init.rc 启动

```ini
# /init.rc
service servicemanager /system/bin/servicemanager
    class core animation
    user system
    group system readproc
    critical
    onrestart restart healthd
    onrestart restart zygote
    onrestart restart audioserver
    onrestart restart media
    onrestart restart surfaceflinger
    onrestart restart inputflinger
    onrestart restart drm
    onrestart restart cameraserver
    onrestart restart keystore
    onrestart restart gatekeeperd
    writepid /dev/cpuset/system-background/tasks
```

ServiceManager 崩溃会导致一系列核心服务重启。

### 2.2 ServiceManager 内部初始化

```cpp
// frameworks/native/cmds/servicemanager/service_manager.c

int main(int argc, char** argv)
{
    struct binder_state *bs;
    // ...

    // ── 1. 打开 Binder 驱动 ──
    bs = binder_open(driver, 128*1024);
    // binder_open 内部：open("/dev/binder") + mmap(128KB)

    // ── 2. 将自身注册为 Context Manager ──
    //     告诉内核：本进程就是 ServiceManager
    if (binder_become_context_manager(bs)) {
        ALOGE("cannot become context manager (%s)\n", strerror(errno));
        return -1;
    }

    // ── 3. 进入 Binder 事件循环 ──
    //     不断读取和处理 Binder 事务
    binder_loop(bs, svcmgr_handler);

    return 0;
}
```

### 2.3 binder_become_context_manager

```cpp
// frameworks/native/cmds/servicemanager/binder.c

int binder_become_context_manager(struct binder_state *bs)
{
    // 发送 ioctl BINDER_SET_CONTEXT_MGR
    // 内核将当前进程标记为 context_mgr
    return ioctl(bs->fd, BINDER_SET_CONTEXT_MGR, 0);
}
```

### 2.4 内核 BINDER_SET_CONTEXT_MGR 处理

```c
// drivers/android/binder.c
static int binder_ioctl_set_ctx_mgr(struct file *filp, struct binder_proc *proc)
{
    struct binder_context *context = proc->context;
    struct binder_node *node;

    // ── 1. 只能有一个 Context Manager ──
    if (context->binder_context_mgr_node) {
        return -EBUSY;  // 已经注册过了
    }

    // ── 2. 创建 ServiceManager 的 binder_node ──
    //     ptr = 0, cookie = 0
    //     这是一个特殊的 node，handle 固定为 0
    node = binder_new_node(proc, 0, 0);
    if (!node)
        return -ENOMEM;

    // ── 3. 标记为 context_mgr_node ──
    node->local_strong_refs++;
    node->local_weak_refs++;
    context->binder_context_mgr_node = node;
    node->has_strong_ref = 1;
    node->has_weak_ref = 1;

    return 0;
}
```

---

## 三、服务注册流程

### 3.1 总体流程

```
服务端进程（如 SystemServer）        内核                    ServiceManager
   │                                 │                         │
   │ 创建 Binder 服务对象            │                         │
   │ (BBinder*)                      │                         │
   │                                 │                         │
   │ ServiceManager::addService()    │                         │
   │ 填充 Parcel:                    │                         │
   │   SVC_MGR_ADD_SERVICE           │                         │
   │   服务名称 ("activity")         │                         │
   │   flat_binder_object            │                         │
   │   (type=BINDER_TYPE_BINDER      │                         │
   │    binder=BBinder地址)          │                         │
   │                                 │                         │
   │── BC_TRANSACTION ──────────────►│                         │
   │   target.handle = 0             │                         │
   │   （handle=0 固定指向 SM）      │                         │
   │                                 │                         │
   │                                 │                          │
   │                                 │  1. 查找 ref(desc=0)    │
   │                                 │     → context_mgr_node   │
   │                                 │     → node->proc (SM)   │
   │                                 │                          │
   │                                 │  2. 分配 buffer          │
   │                                 │  3. 拷贝数据             │
   │                                 │  4. 转换 flat_binder_    │
   │                                 │     object:              │
   │                                 │     BINDER → HANDLE     │
   │                                 │                          │
   │                                 ├────────────────────────►│
   │                                 │                          │
   │                                 │                          │── BR_TRANSACTION
   │                                 │                          │── svcmgr_handler()
   │                                 │                          │
   │                                 │                          │  1. 解析服务名称
   │                                 │                          │  2. 存储到 svclist 链表
   │                                 │                          │     { name, handle }
   │                                 │                          │
   │                                 │                          │── BC_REPLY
   │                                 │◄─────────────────────────│
   │◄── BR_REPLY ───────────────────│                          │
   │   (OK)                         │                          │
```

### 3.2 用户空间调用

```cpp
// frameworks/native/libs/binder/IServiceManager.cpp

status_t ServiceManagerShim::addService(const String16& name,
                                        const sp<IBinder>& service,
                                        bool allowIsolated)
{
    Parcel data, reply;

    // ── 1. 写入 SVC_MGR_ADD_SERVICE 命令码 ──
    data.writeInt32(IPCThreadState::self()->getStrictModePolicy()
                    | STRICT_MODE_PENALTY_GATHER);
    data.writeStrongBinder(service);     // 写入 flat_binder_object（BINDER_TYPE_BINDER）
    data.writeString16(name);            // 写入服务名称
    data.writeInt32(allowIsolated ? 1 : 0);

    // ── 2. 发送给 ServiceManager（handle=0） ──
    status_t err = remote()->transact(ADD_SERVICE_TRANSACTION, data, &reply);
    return err;
}
```

### 3.3 BpBinder 的 transact

```cpp
// frameworks/native/libs/binder/BpBinder.cpp

status_t BpBinder::transact(uint32_t code, const Parcel& data,
                            Parcel* reply, uint32_t flags)
{
    // BpBinder 持有 handle（对于 SM 是 0）
    // 调用 IPCThreadState 发送 BC_TRANSACTION
    status_t status = IPCThreadState::self()->transact(
        mHandle, code, data, reply, flags);
    return status;
}
```

### 3.4 ServiceManager 处理注册

```cpp
// frameworks/native/cmds/servicemanager/service_manager.c

int svcmgr_handler(struct binder_state *bs,
                   struct binder_transaction_data *txn,
                   struct binder_io *msg,
                   struct binder_io *reply)
{
    struct svcinfo *si;
    uint16_t *s;
    size_t len;
    uint32_t handle;

    // ...

    switch(txn->code) {
    case SVC_MGR_GET_SERVICE:
    case SVC_MGR_CHECK_SERVICE:
        // 查询服务（见下一节）
        s = bio_get_string16(msg, &len);
        // 查找 svclist
        handle = do_find_service(bs, s, len, txn->sender_euid, txn->sender_pid);
        if (handle)
            bio_put_ref(reply, handle);
        else
            bio_put_uint32(reply, 0);
        return 0;

    case SVC_MGR_ADD_SERVICE:
        // ── 注册服务 ──
        s = bio_get_string16(msg, &len);        // 服务名称
        if (s == NULL) return -1;

        handle = bio_get_ref(msg);              // 获取 handle（内核已转换好）
        if (handle) {
            // 将服务信息保存到 svclist
            si = find_svc(s, len);
            if (si) {
                // 已存在 → 更新 handle
                si->handle = handle;
            } else {
                // 不存在 → 创建新条目
                si = malloc(sizeof(*si) + (len + 1) * sizeof(uint16_t));
                si->handle = handle;
                si->len = len;
                memcpy(si->name, s, (len + 1) * sizeof(uint16_t));
                si->name[len] = '\0';
                si->death.func = svcinfo_death;
                si->death.ptr = si;
                // 插入链表头
                si->next = svclist;
                svclist = si;
            }
        }

        // 回复 OK
        bio_put_uint32(reply, 0);
        return 0;

    case SVC_MGR_LIST_SERVICES:
        // 列出所有已注册服务
        // ...
        return 0;
    }

    return -1;
}
```

### 3.5 svclist 链表结构

```
svclist (全局链表头)
   │
   ├── svcinfo { name="activity",     handle=1,  death.func=svcinfo_death }
   ├── svcinfo { name="package",      handle=2,  death.func=svcinfo_death }
   ├── svcinfo { name="window",       handle=3,  death.func=svcinfo_death }
   ├── svcinfo { name="power",        handle=4,  death.func=svcinfo_death }
   ├── svcinfo { name="sensorservice",handle=5,  death.func=svcinfo_death }
   ├── svcinfo { name="batterystats", handle=6,  death.func=svcinfo_death }
   └── ... (80+ 系统服务)
```

### 3.6 服务死亡自动清理

```cpp
// 当服务进程死亡时，ServiceManager 收到 BR_DEAD_BINDER
void svcinfo_death(struct binder_state *bs, void *ptr)
{
    struct svcinfo *si = (struct svcinfo *)ptr;

    // 从链表中移除此服务
    if (si == svclist) {
        svclist = si->next;
    } else {
        struct svcinfo *prev = svclist;
        while (prev && prev->next != si)
            prev = prev->next;
        if (prev)
            prev->next = si->next;
    }

    // 释放内存
    free(si);
}
```

---

## 四、服务查询流程

### 4.1 总体流程

```
客户端进程（App）                 内核                    ServiceManager
   │                                 │                         │
   │ getService("activity")          │                         │
   │ 填充 Parcel:                    │                         │
   │   SVC_MGR_GET_SERVICE           │                         │
   │   服务名称 ("activity")         │                         │
   │                                 │                         │
   │── BC_TRANSACTION ──────────────►│                         │
   │   target.handle = 0             │                         │
   │                                 │                         │
   │                                 ├────────────────────────►│
   │                                 │                         │
   │                                 │                         │── svcmgr_handler()
   │                                 │                         │   do_find_service()
   │                                 │                         │   遍历 svclist
   │                                 │                         │   找到 handle=1
   │                                 │                         │
   │                                 │                         │── bio_put_ref(reply, 1)
   │                                 │                         │   写入 flat_binder_object
   │                                 │                         │   type=BINDER_TYPE_HANDLE
   │                                 │                         │   handle=1
   │                                 │                         │
   │                                 │◄── BC_REPLY ───────────│
   │                                 │                         │
   │                                 │  转换 flat_binder_object │
   │                                 │  HANDLE → HANDLE        │
   │                                 │  （在客户端创建/查找    │
   │                                 │   ref，分配新句柄号）    │
   │                                 │                         │
   │◄── BR_REPLY ───────────────────│                         │
   │   拿到 handle                   │                         │
   │   创建 BpBinder(handle)         │                         │
   │   返回 IBinder                  │                         │
```

### 4.2 用户空间调用

```cpp
// frameworks/native/libs/binder/IServiceManager.cpp

sp<IBinder> ServiceManagerShim::getService(const String16& name) const
{
    Parcel data, reply;
    sp<IBinder> b;

    // ── 1. 写入查询参数 ──
    data.writeString16(name);

    // ── 2. 发送给 ServiceManager ──
    //     使用 CHECK_SERVICE_TRANSACTION 先检查
    status_t err = remote()->transact(GET_SERVICE_TRANSACTION,
                                      data, &reply);

    // ── 3. 从回复中取出 Binder 句柄 ──
    if (err == NO_ERROR) {
        // readStrongBinder() 从 Parcel 中读取 flat_binder_object
        // 将其包装为 BpBinder(handle)
        b = reply.readStrongBinder();
    }

    return b;
}
```

### 4.3 readStrongBinder 解析

```cpp
// frameworks/native/libs/binder/Parcel.cpp

sp<IBinder> Parcel::readStrongBinder() const
{
    sp<IBinder> val;

    // 从 Parcel 中读取 flat_binder_object
    // 对于从 SM 返回的回复，type = BINDER_TYPE_HANDLE
    unflatten_binder(ProcessState::self(), *this, &val);

    return val;
}

status_t unflatten_binder(const sp<ProcessState>& proc,
                          const Parcel& in, sp<IBinder>* out)
{
    const flat_binder_object *flat = in.readObject(false);

    if (flat) {
        switch (flat->hdr.type) {
        case BINDER_TYPE_BINDER:
            // 本地 Binder 对象 → 直接转 BBinder*
            *out = reinterpret_cast<IBinder*>(flat->cookie);
            return NO_ERROR;

        case BINDER_TYPE_HANDLE:
            // 远程句柄 → 创建 BpBinder
            *out = proc->getStrongProxyForHandle(flat->handle);
            return NO_ERROR;
        }
    }
    return BAD_TYPE;
}
```

### 4.4 do_find_service — ServiceManager 查询

```cpp
// frameworks/native/cmds/servicemanager/service_manager.c

uint32_t do_find_service(struct binder_state *bs,
                         const uint16_t *s, size_t len,
                         uid_t caller_uid, pid_t caller_pid)
{
    struct svcinfo *si;

    // ── 1. 遍历 svclist 链表 ──
    si = find_svc(s, len);

    if (si && si->handle) {
        // ── 2. 找到服务 → 返回句柄 ──
        //     如果服务进程已死，handle 会被设为 0
        return si->handle;
    }

    // ── 3. 未找到 → 返回 0 ──
    return 0;
}

struct svcinfo *find_svc(const uint16_t *s, size_t len)
{
    struct svcinfo *si;

    // 遍历 svclist 链表，按名称匹配
    for (si = svclist; si; si = si->next) {
        if (si->len == len &&
            !memcmp(s, si->name, len * sizeof(uint16_t))) {
            return si;
        }
    }

    return NULL;
}
```

### 4.5 getStrongProxyForHandle — 创建 BpBinder

```cpp
// frameworks/native/libs/binder/ProcessState.cpp

sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    // ── 1. 查找或创建 handle_entry ──
    handle_entry *e = lookupHandleLocked(handle);
    if (e == nullptr) return result;

    // ── 2. 如果还没有 BpBinder，创建新的 ──
    if (e->binder == nullptr) {
        // 创建 BpBinder，持有 handle
        // 并通过 BC_INCREFS/BC_ACQUIRE 增加引用计数
        e->binder = new BpBinder(handle);
        result = e->binder;

        // 通知内核增加此句柄的引用计数
        IPCThreadState::self()->incWeakHandle(handle);
        IPCThreadState::self()->decWeakHandle(handle);

    } else {
        result = e->binder;
    }

    return result;
}
```

---

## 五、Context Manager 机制

### 5.1 为什么 handle=0 总是指向 ServiceManager？

内核在初始化时创建了 `context_mgr_node`，并确保这个 node 的引用永不释放：

```
binder_proc (ServiceManager)
   │
   └── binder_node (ptr=0, cookie=0)
        │
        └── context->binder_context_mgr_node (内核全局唯一)
               │
               └── 所有进程的 ref(desc=0) → 此 node
```

### 5.2 其他进程如何获得 handle=0

```cpp
// ProcessState.cpp 中自动获取

ProcessState::ProcessState(const char *driver)
{
    // ...
    if (mDriverFD >= 0) {
        // ── 自动为 handle=0 创建 BpBinder ──
        //     用于后续所有对 ServiceManager 的调用
        if (mManager == nullptr) {
            // 获取 SM 的 BpBinder(0)
            sp<BpBinder> b = getStrongProxyForHandle(0);
            mManager = new BpServiceManager(b);
        }
    }
}
```

### 5.3 ServiceManager 的 BpServiceManager

```cpp
// frameworks/native/libs/binder/IServiceManager.cpp

class BpServiceManager : public BpInterface<IServiceManager>
{
public:
    explicit BpServiceManager(const sp<IBinder>& impl)
        : BpInterface<IServiceManager>(impl)
    {}

    virtual sp<IBinder> getService(const String16& name) const
    {
        return getServiceInternal(name);
    }

    virtual status_t addService(const String16& name,
                                const sp<IBinder>& service,
                                bool allowIsolated)
    {
        Parcel data, reply;
        data.writeStrongBinder(service);   // 写入本地 Binder
        data.writeString16(name);
        // ...
        return remote()->transact(ADD_SERVICE_TRANSACTION, data, &reply);
    }
};
```

---

## 六、完整时序图

### 6.1 完整生命周期

```
SystemServer                     ServiceManager                    App
    │                                  │                           │
    │  1. 启动                          │                           │
    │  ProcessState::self()             │                           │
    │  IPCThreadState::self()           │                           │
    │  joinThreadPool(true)             │                           │
    │                                  │                           │
    │  2. 注册 ActivityManager          │                           │
    │  addService("activity", svc)     │                           │
    │─────────────────────────────────►│                           │
    │                                  │ 存储到 svclist            │
    │◄─────────────────────────────────│                           │
    │                                  │                           │
    │  3. 其他服务...(80+)             │                           │
    │                                  │                           │
    │                                  │                           │  4. App 启动
    │                                  │                           │  getService("activity")
    │                                  │◄──────────────────────────│
    │                                  │ 查找 svclist              │
    │                                  │ 返回 handle=1             │
    │                                  ├──────────────────────────►│
    │                                  │                           │
    │                                  │                           │  5. 通过 handle=1
    │                                  │                           │  调用 ActivityManager
    │◄─────────────────────────────────────────────────────────────│
    │  BR_TRANSACTION                  │                           │
    │  onTransact()                    │                           │
    ├─────────────────────────────────────────────────────────────►│
    │  BC_REPLY                        │                           │
    │                                  │                           │
    │  6. SystemServer 崩溃            │                           │
    │  (binder_release)                │                           │
    │                                  │                           │
    │  kernel 发 BR_DEAD_BINDER        │                           │
    │─────────────────────────────────►│                           │
    │                                  │ svcinfo_death()           │
    │                                  │ 从 svclist 删除           │
    │                                  │                           │
    │                                  │                           │  7. App 再次调用
    │                                  │                           │  → 得到 DEAD_BINDER
```

---

## 七、总结

```
ServiceManager = Binder 黄页服务
      │
      ├── 注册流程:
      │     addService(name, service)
      │       │
      │       ├── 写入 SVC_MGR_ADD_SERVICE + 服务名 + flat_binder_object
      │       ├── transact(handle=0) → 内核 → ServiceManager
      │       ├── ServiceManager 将 {name, handle} 存入 svclist
      │       └── 返回 OK
      │
      ├── 查询流程:
      │     getService(name)
      │       │
      │       ├── 写入 SVC_MGR_GET_SERVICE + 服务名
      │       ├── transact(handle=0) → 内核 → ServiceManager
      │       ├── ServiceManager 遍历 svclist 找到 handle
      │       ├── 返回 flat_binder_object(type=HANDLE, handle=X)
      │       └── 客户端创建 BpBinder(X)
      │
      └── 特点:
            ├── handle=0 固定指向 ServiceManager（内核保证）
            ├── ServiceManager 是唯一 Context Manager
            ├── 使用自定义 SVC_MGR 协议（非 AIDL）
            └── 服务死亡时自动清理 svclist
```

**文件**：
- `frameworks/native/cmds/servicemanager/service_manager.c`
- `frameworks/native/libs/binder/IServiceManager.cpp`
- `drivers/android/binder.c` — `binder_ioctl_set_ctx_mgr()`
