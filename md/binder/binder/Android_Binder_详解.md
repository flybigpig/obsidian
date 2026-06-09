# Android Binder 详解

## 目录

1. [什么是 Binder](#一什么是-binder)
2. [为什么用 Binder](#二为什么用-binder而不是-linux-传统-ipc)
3. [Binder 架构](#三binder-架构)
4. [关键技术点](#四关键技术点)
5. [AIDL](#五aidlandroid-interface-definition-language)
6. [Binder HAL](#六binder-halhardware-abstraction-layer)
7. [Binder 安全性](#七binder-安全性)
8. [相关问题排查](#八相关问题排查)
9. [binder_open 源码解析](#九binder_open-源码解析)
10. [总结](#十总结)

---

## 一、什么是 Binder？

Binder 是 Android 系统**核心的 IPC 机制**，用于在不同进程之间传递数据和方法调用。它基于 **C/S（Client-Server）架构**，性能高、安全性好，是 Android 四大组件（Activity、Service、BroadcastReceiver、ContentProvider）跨进程通信的底层基础。

---

## 二、为什么用 Binder，而不是 Linux 传统 IPC？

| 对比项 | Binder | Linux 传统 IPC（Socket/共享内存/消息队列） |
|--------|--------|------------------------------------------|
| **性能** | 高 — 一次拷贝（mmap + 内核缓冲池） | Socket/管道需两次拷贝 |
| **安全** | 内建 UID/PID 校验，每个调用可验证身份 | 需手动实现校验 |
| **易用性** | 框架层封装了 AIDL，开发者只需定义接口 | 需自行处理序列化/反序列化 |
| **面向对象** | 原生支持远程对象调用（像本地调用一样） | 仅支持字节流/数据块 |

---

## 三、Binder 架构

### 3.1 整体分层

```
┌─────────────────────────────────────┐
│   Application Framework (Java)      │  ← AIDL / Binder 类
├─────────────────────────────────────┤
│   Native Framework (C++)            │  ← libbinder (IPCThreadState, ProcessState)
├─────────────────────────────────────┤
│   Binder Driver (内核)               │  ← /dev/binder
├─────────────────────────────────────┤
│   物理硬件 / Linux Kernel            │
└─────────────────────────────────────┘
```

### 3.2 核心角色

| 角色 | 说明 |
|------|------|
| **Client** | 发起 IPC 调用的进程 |
| **Server** | 提供服务的进程（如 SystemServer） |
| **ServiceManager** | 注册和查询服务的"黄页"，Binder 上下文管理者 |
| **Binder Driver** | 内核模块，负责跨进程数据传递和线程管理 |

### 3.3 工作流程

```
Client 进程                  Binder Driver              Server 进程
   │                            │                          │
   │── transact() ──────────►   │                          │
   │                            │── 将调用数据拷贝到内核   │
   │                            │── 找到目标服务所在进程   │
   │                            │── 唤醒 Server 线程      │
   │                            │── 通过 mmap 映射区       │
   │                            │   完成一次数据拷贝        │
   │                            ├──────────────────────►   │
   │                            │                          │── onTransact()
   │                            │                          │── 处理请求
   │                            │◄───────────────────────  │
   │                            │── 回复数据返回           │
   │◄─────────── reply ───────  │                          │
```

---

## 四、关键技术点

### 4.1 一次拷贝（mmap 优化）

传统 IPC（如 Socket）需要两次数据拷贝：用户 → 内核 → 用户。

Binder 通过 **mmap** 在内核空间和接收进程的用户空间之间建立共享内存映射，只需**一次拷贝**：发送方 → 内核缓冲区。接收方通过映射直接读取，性能大幅提升。

### 4.2 Binder 协议

| 命令 | 说明 |
|------|------|
| `BC_TRANSACTION` | Client → Driver：发送调用请求 |
| `BC_REPLY` | Server → Driver：发送回复 |
| `BR_TRANSACTION` | Driver → Server：传递调用请求给目标 |
| `BR_REPLY` | Driver → Client：传递回复给调用方 |

### 4.3 Binder 线程模型

- 每个进程有一个 **Binder 线程池**
- 默认最大 16 个线程（`BINDER_MAX_THREADS`）
- 通过 `BR_SPAWN_LOOPER` 驱动自动扩缩线程

---

## 五、AIDL（Android Interface Definition Language）

AIDL 是 Binder 的上层封装，让开发者声明接口即可生成跨进程通信代码。

```java
// IRemoteService.aidl
interface IRemoteService {
    int add(int a, int b);
    String getMessage();
}
```

编译后自动生成：
- `IRemoteService.java`（接口 + Stub 类）
- **Stub**：服务端基类，处理 `onTransact()`
- **Proxy**：客户端代理类，处理 `transact()`

---

## 六、Binder HAL（Hardware Abstraction Layer）

Binder HAL 是指 Android 中通过 Binder 暴露给上层 Framework 的硬件服务接口。典型代表：

### 主要 Binder HAL 服务

| 服务 | Binder 名称 | 功能 |
|------|------------|------|
| **WindowManager** | `window` | 窗口管理、屏幕旋转、输入事件分发 |
| **ActivityManager** | `activity` | Activity 生命周期、任务栈管理 |
| **PackageManager** | `package` | 应用安装卸载、权限查询 |
| **PowerManager** | `power` | 电源管理、唤醒锁 |
| **AudioFlinger** | `media.audio_flinger` | 音频混音和输出 |
| **SurfaceFlinger** | `SurfaceFlinger` | 图形合成和显示 |
| **SensorService** | `sensorservice` | 传感器数据管理 |
| **CameraService** | `media.camera` | 相机控制 |
| **ConnectivityService** | `connectivity` | 网络连接管理 |

这些服务都运行在 **SystemServer** 进程中，通过 ServiceManager 注册，各应用作为 Client 通过 Binder IPC 调用。

---

## 七、Binder 安全性

- **UID/PID 校验**：Binder Driver 自动为每个事务附加发送方的 UID 和 PID，Server 端可以检查调用方权限
- **实名 Binder**：通过 ServiceManager 注册，名称全局唯一
- **匿名 Binder**：随事务传递，只对参与双方可见

```java
// 在服务端检查调用方 PID
public boolean onTransact(int code, Parcel data, Parcel reply, int flags) {
    int callerPid = Binder.getCallingPid();
    int callerUid = Binder.getCallingUid();
    // 权限校验...
    return super.onTransact(code, data, reply, flags);
}
```

---

## 八、相关问题排查

### 常见 Binder 错误

| 错误 | 含义 |
|------|------|
| **DeadObjectException** | 服务端进程已死或 Binder 连接断开 |
| **TransactionTooLargeException** | 传输数据超过 1MB 限制 |
| **SecurityException** | 权限校验失败 |
| `binder: cannot open /dev/binder` | 内核未加载 Binder 驱动（通常模拟器/Houdini 问题） |

### 查看 Binder 状态

```bash
# 查看所有 Binder 节点和引用
adb shell cat /sys/kernel/debug/binder/state

# 查看 Binder 事务统计
adb shell cat /sys/kernel/debug/binder/stats

# 查看进程 Binder 线程池
adb shell ls -la /proc/<pid>/fd/ | grep binder
```

---

## 九、binder_open 源码解析

### 9.1 概述

`binder_open` 是 Binder 内核驱动中**处理设备打开操作**的函数。当用户空间进程执行 `open("/dev/binder", O_RDWR)` 时，最终会触发此函数。

**所属文件**：`drivers/android/binder.c`（主线内核）

### 9.2 函数原型

```c
static int binder_open(struct inode *nodp, struct file *filp)
```

| 参数 | 说明 |
|------|------|
| `nodp` | inode 节点（对应 `/dev/binder`） |
| `filp` | 文件结构体，内核用它跟踪打开实例 |
| **返回值** | 0 成功，负数失败 |

### 9.3 完整源码解析

```c
static int binder_open(struct inode *nodp, struct file *filp)
{
    struct binder_proc *proc;      // 进程级 Binder 上下文
    struct binder_device *binder_dev;

    binder_dev = container_of(nodp->i_rdev, struct binder_device, rdev);

    // 1. 分配 binder_proc 结构体
    proc = kzalloc(sizeof(*proc), GFP_KERNEL);
    if (!proc)
        return -ENOMEM;

    // 2. 获取当前进程的 tgid（线程组 ID）
    get_task_struct(current->group_leader);
    proc->tsk = current->group_leader;

    // 3. 初始化各种链表和锁
    mutex_init(&proc->files_lock);
    INIT_LIST_HEAD(&proc->todo);           // 待处理事务队列
    INIT_LIST_HEAD(&proc->delivered_death); // 已送达的死亡通知
    INIT_LIST_HEAD(&proc->waiting_threads); // 等待中的线程
    proc->pid = current->group_leader->pid;
    proc->default_priority = task_nice(current);

    // 4. 设置异步冻结等待队列
    atomic_set(&proc->async_todo, 0);      // 异步事务计数
    binder_dev->proc_count++;              // 设备打开计数
    proc->context = &binder_dev->context;  // 绑定 Binder 上下文

    // 5. 绑定到文件结构体（关键！后续所有 Binder 操作通过 filp->private_data 找到 proc）
    filp->private_data = proc;

    // 6. 初始化 ID 分配器（用于分配 Binder 引用句柄）
    binder_alloc_init(&proc->alloc);

    // 7. 将 proc 加入全局链表
    mutex_lock(&binder_procs_lock);
    hlist_add_head(&proc->proc_node, &binder_procs);
    mutex_unlock(&binder_procs_lock);

    // 8. mmap 还没有发生，此时 proc->buffer 为 NULL

    return 0;
}
```

### 9.4 binder_dev 的数据结构关联

```
binder_device
├── rdev           (设备号, 用于 container_of 反查)
├── context        (Binder 上下文, 如 "binder" / "hwbinder" / "vndbinder")
├── proc_count     (打开此设备的进程数)
└── miscdev        (misc 设备结构体)

binder_proc
├── tsk            (指向打开设备的线程组 leader)
├── pid            (进程 PID)
├── todo           (待处理事务链表)
├── proc_node      (全局 binder_procs 链表节点)
├── alloc          (binder_alloc — 内存分配器, 管理 mmap 缓冲区)
├── default_priority
├── delivered_death
├── waiting_threads
├── context        (指向 binder_device.context)
├── files_lock
├── refs_by_node   (红黑树, 按 node 查找引用)
├── refs_by_desc   (红黑树, 按句柄查找引用)
├── nodes          (红黑树, 本进程创建的 Binder Node)
└── threads        (红黑树, 本进程的 Binder 线程)
```

### 9.5 执行流程图

```
用户空间               内核空间
────────             ─────────
fd = open("/dev/binder", O_RDWR)
    │
    ▼
VFS (虚拟文件系统)               binder_fops = { .open = binder_open, ... }
    │
    ▼
binder_open(nodp, filp)
    │
    ├── kzalloc(proc)           ─── 分配进程 Binder 上下文
    ├── get_task_struct()       ─── 引用当前进程
    ├── INIT_LIST_HEAD()        ─── 初始化链表
    ├── proc->pid = pid         ─── 记录 PID
    ├── filp->private_data = proc ─── 绑定到 fd
    ├── hlist_add_head(&binder_procs) ── 加入全局列表
    │
    ▼
返回 0 (成功)
```

### 9.6 关键设计要点

#### 一个进程只打开一次

虽然可以多次 `open("/dev/binder")`，但每个进程**通常只打开一次**。`binder_proc` 是按线程组（tgid）分配的，`ProcessState`（Native 层 C++ 代码）会保证同一个进程只打开一次。

```cpp
// frameworks/native/libs/binder/ProcessState.cpp
ProcessState::ProcessState(const char *driver)
    : mDriverName(String8(driver))
    , mDriverFD(-1)
    // ...
{
    if (mDriverFD >= 0) {
        // 已经打开过，直接返回
        return;
    }
    mDriverFD = open_driver(driver);  // 最终调用 open("/dev/binder")
}
```

#### 此时还没做 mmap

`binder_open` **只分配了数据结构**，真正的内存映射在后续 `binder_mmap` 中完成。在 mmap 完成前，proc->buffer 为 NULL。

#### binder_procs 全局链表

所有打开 `/dev/binder` 的进程都挂在 `binder_procs` 全局哈希链表上，用于调试和跟踪：

```bash
# 查看所有 Binder 进程
adb shell cat /sys/kernel/debug/binder/proc
# 或
adb shell cat /d/binder/proc
```

### 9.7 多 Binder 上下文

Android 8.0+（Treble）引入**多个 Binder 设备**，每个设备对应一个 `binder_device`：

| 设备节点 | 上下文 | 用途 |
|----------|--------|------|
| `/dev/binder` | `binder` | Framework 层 IPC |
| `/dev/hwbinder` | `hwbinder` | HAL 层 IPC（Treble） |
| `/dev/vndbinder` | `vndbinder` | Vendor 进程间 IPC |

每个设备有自己的 `binder_procs` 列表，互不干扰。`binder_open` 通过 `container_of` 从 `inode` 反查到对应的 `binder_device`。

### 9.8 错误场景

| 返回值 | 原因 |
|--------|------|
| `-ENOMEM` | 内核内存不足，`kzalloc` 分配 `binder_proc` 失败 |
| `-ENODEV` | Binder 驱动未注册或设备节点不存在 |
| `-EPERM` |（某些安全加固内核）非 root/非 system 进程被禁止打开 |

### 9.9 相关函数链

```
open("/dev/binder")
  └─ binder_open()            ← 当前讨论
       └─ binder_alloc_init()
       └─ hlist_add_head(&binder_procs)

mmap(...)
  └─ binder_mmap()            ← 分配 Binder 缓冲区

ioctl(BC_ENTER_LOOPER)
  └─ binder_ioctl()           ← 进入 Binder 事件循环

close(fd)
  └─ binder_release()         ← 清理 binder_proc
       └─ binder_deferred_release()
```

---

## 十、binder_mmap 内存映射机制

### 10.1 概述

`binder_mmap` 是 Binder 驱动中**分配和建立共享内存映射**的函数。用户空间进程在 `open("/dev/binder")` 后，紧接着会调用 `mmap()`，最终触发内核中的 `binder_mmap`。

这是 Binder **一次拷贝（one-copy）** 性能优势的关键技术基础。

#### 函数原型

```c
static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
```

| 参数 | 说明 |
|------|------|
| `filp` | 已打开的 binder 设备文件 |
| `vma` | 虚拟内存区域结构体，描述用户空间地址区间和映射属性 |
| **返回值** | 0 成功，负数失败 |

---

### 10.2 完整源码解析

```c
static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct binder_proc *proc = filp->private_data;  // 从 binder_open 获取
    struct binder_buffer *buffer;
    int ret;

    // ── 1. 参数校验 ──
    if (proc->tsk != current->group_leader)
        return -EINVAL;

    // 映射大小不能超过 4MB（SVE = BINDER_VM_SIZE）
    if ((vma->vm_end - vma->vm_start) > SZ_4M)
        vma->vm_end = vma->vm_start + SZ_4M;

    // 只允许 MAP_SHARED 模式（必须与其他进程共享）
    if (!(vma->vm_flags & VM_SHARED))
        return -EINVAL;

    // ── 2. 防止重复 mmap ──
    if (proc->buffer)
        return -EBUSY;  // 一个进程只能 mmap 一次

    // ── 3. 在内核空间分配连续的物理内存页（allocator 管理） ──
    proc->buffer = kzalloc(alloc_size, GFP_KERNEL);
    if (!proc->buffer)
        return -ENOMEM;

    // ── 4. 将内核分配的物理内存映射到用户空间（关键！） ──
    ret = remap_vmalloc_range(vma, proc->buffer, vma->vm_pgoff);
    if (ret) {
        kfree(proc->buffer);
        proc->buffer = NULL;
        return ret;
    }

    // ── 5. 初始化缓冲区管理 ──
    proc->buffer_size = vma->vm_end - vma->vm_start;
    proc->pages = kzalloc(sizeof(proc->pages[0]) *
                          ((proc->buffer_size + PAGE_SIZE - 1) / PAGE_SIZE),
                          GFP_KERNEL);

    // ── 6. 创建初始的空闲 buffer ──
    buffer = proc->buffer;
    INIT_LIST_HEAD(&proc->buffers);
    list_add(&buffer->entry, &proc->buffers);
    buffer->free = 1;
    buffer->data = proc->buffer + ALIGN(offset, sizeof(void *));
    buffer->free_space = proc->buffer_size - ALIGN(offset, sizeof(void *));

    // ── 7. 空闲 buffer 链表初始化 ──
    list_add(&buffer->entry, &proc->free_buffers);

    proc->free_async_space = proc->buffer_size / 2;  // 异步事务的空间上限

    return 0;
}
```

---

### 10.3 核心机制详解

#### 10.3.1 一次拷贝（One-Copy）原理

Binder 一次拷贝的关键在于 **`remap_vmalloc_range`**：

```
        进程 A（Client）                   进程 B（Server）
      ┌─────────────────┐             ┌─────────────────┐
      │   用户空间       │             │   用户空间       │
      │                 │             │                 │
      │  send data      │             │  read data      │◄── 直接读取，无需拷贝
      │      │          │             │      ▲          │
      └──────┼──────────┘             └──────┼──────────┘
             │ ① copy_from_user()            │
             ▼                               │
      ┌──────────────────────────────────────┼─────────┐
      │         内核空间                       │         │
      │                                      │         │
      │  ┌───────────────────────────────────┘         │
      │  │  ② binder_buffer（内核分配的物理内存）           │
      │  │  ← 进程 B 通过 mmap 直接映射这个区域             │
      │  └──────────────────────────────────────────────│
      │                                                 │
      └─────────────────────────────────────────────────┘
```

**传统 IPC（如 Socket）**：
1. 进程 A 用户空间 → 内核缓冲区（一次拷贝）
2. 内核缓冲区 → 进程 B 用户空间（二次拷贝）

**Binder IPC**：
1. 进程 A 用户空间 → 内核 binder_buffer（**一次拷贝**）
2. 进程 B 通过 mmap 映射直接读取同一块物理内存（**零拷贝**）

#### 10.3.2 内存布局

```
进程 B 的用户空间
┌─────────────────────────────────────────────┐
│  mmap 映射区 (vma)                          │
│  ┌───────────────────────────────────────┐  │
│  │ binder_buffer 缓冲池                 │  │
│  │  ┌─────┐ ┌─────┐ ┌─────┐            │  │
│  │  │空闲 │ │使用 │ │空闲 │  ...        │  │
│  │  └─────┘ └─────┘ └─────┘            │  │
│  │  proc->buffers 链表管理              │  │
│  └───────────────────────────────────────┘  │
│  (其他映射区)                                │
└─────────────────────────────────────────────┘

内核空间
┌─────────────────────────────────────────────┐
│  binder_proc                                 │
│  ├── buffer  → 指向同一物理地址             │
│  ├── pages[] → 跟踪每个物理页的引用         │
│  └── free_buffers → 空闲缓冲区链表          │
└─────────────────────────────────────────────┘

物理内存（同一组物理页框）
┌─────┬─────┬─────┬─────┬─────┬─────┐
│ Pg0 │ Pg1 │ Pg2 │ Pg3 │ ... │ PgN │
└─────┴─────┴─────┴─────┴─────┴─────┘
  ↑                          ↑
  └── 被进程 A 和进程 B 同时映射
```

#### 10.3.3 缓冲区分配策略

`binder_alloc` 管理着缓冲池，采用**最佳适应（best-fit）** 策略：

```c
struct binder_buffer *binder_alloc_new_buf(struct binder_alloc *alloc,
                                           size_t data_size,
                                           size_t offsets_size,
                                           size_t extra_buffers_size)
{
    // 1. 遍历 free_buffers 链表，找到 size >= 需求的最佳空闲块
    // 2. 从空闲块中切出所需大小
    // 3. 剩余部分作为新的空闲块重新加入 free_buffers
    // 4. 标记这块 buffer 为已使用
    // 5. 分配物理页（如果尚未分配）
}
```

---

### 10.4 与 binder_open 的区别和关联

| 函数 | 时机 | 作用 | 执行次数 |
|------|------|------|---------|
| `binder_open` | open("/dev/binder") | 分配 `binder_proc`、初始化链表、加入全局列表 | 每个进程一次 |
| `binder_mmap` | mmap(...) | 分配物理内存、建立用户态到内核态的映射 | 每个进程一次 |

**调用顺序**：
```
fd = open("/dev/binder")      → binder_open
map = mmap(fd, ...)           → binder_mmap
```

#### ProcessState 中的实际调用

```cpp
// frameworks/native/libs/binder/ProcessState.cpp
#define BINDER_VM_SIZE ((1 * 1024 * 1024) - sysconf(_SC_PAGE_SIZE) * 2)

ProcessState::ProcessState(const char *driver)
{
    mDriverFD = open_driver(driver);  // → binder_open

    if (mDriverFD >= 0) {
        // mmap the binder, providing a chunk of virtual address space
        // to receive transactions.
        mVMStart = mmap(nullptr, BINDER_VM_SIZE,
                        PROT_READ, MAP_PRIVATE | MAP_NORESERVE,
                        mDriverFD, 0);  // → binder_mmap
    }
}
```

注意这里 `mmap` 使用 `PROT_READ` **只读**标志——这意味着**用户空间只能从映射区读取数据**，不能直接写入。写入必须通过 `ioctl(BINDER_WRITE_READ)` 经由内核完成，保证了安全性。

---

### 10.5 性能对比

| 场景 | 拷贝次数 | 上下文切换 |
|------|---------|-----------|
| **传统 IPC（Unix Socket）** | 2 次拷贝 | 2 次（send → recv） |
| **Binder IPC（有 mmap）** | **1 次拷贝** | **1 次**（ioctl 进入内核，目标线程被唤醒） |
| **共享内存（ashmem）** | 0 次拷贝 | 需配合同步机制 |

对于大数据传输（如一张图片 2MB），Binder 的 `TransactionTooLargeException` 上限是 1MB，超过此阈值应使用 `FileDescriptor` + 共享内存。

---

### 10.6 限制与安全

| 限制项 | 值 | 原因 |
|--------|----|------|
| 最大映射大小 | **4MB**（SVE） | 防止进程耗尽内核低端内存 |
| 异步事务空间上限 | **映射大小的一半** | 防止异步事务淹没目标进程 |
| 映射权限 | **只读 + MAP_SHARED** | 用户空间不可写入，必须通过 ioctl |
| 重复 mmap | 返回 **-EBUSY** | 一个 proc 只能映射一次 |
| 每个事务最大数据 | **1MB** | Binder 内核强校验 |

---

### 10.7 调试与观测

```bash
# 查看每个进程的 Binder mmap 信息
adb shell cat /d/binder/proc

# 输出示例（每个进程一行）：
# proc 1234                         ← PID
# context binder                    ← Binder 上下文
# thread 1: l 12 need_return 0
#     buffer 0x7f8b4000 - 0x7f8b8000  ← mmap 地址范围
#     total_page 256                 ← 已分配的物理页数
#     pid: 1234                      ← 线程组 leader PID

# 查看内存映射
adb shell cat /proc/<pid>/maps | grep binder
# 输出示例：
# 7f8b400000-7f8b800000 r--p 00000000 00:0c 1234    /dev/binder
```

---

### 10.8 关键数据结构

```c
struct binder_proc {
    // ...
    struct binder_alloc alloc;       // 内存分配器

    // mmap 相关字段（新版内核已移到 binder_alloc 中）
    void *buffer;                    // 内核空间虚拟地址
    ptrdiff_t buffer_size;           // 映射大小（最大 4MB）
    struct list_head buffers;        // 所有 buffer 的链表
    struct list_head free_buffers;   // 空闲 buffer 链表
    struct page **pages;             // 物理页指针数组
    size_t free_async_space;         // 剩余异步空间
};

struct binder_buffer {
    struct list_head entry;          // 链表节点（链接到 proc->buffers 或 proc->free_buffers）
    struct rb_node rb_node;          // 红黑树节点（按地址排序）
    unsigned free : 1;               // 是否空闲
    unsigned allow_user_free : 1;
    unsigned async : 1;              // 是否为异步事务
    struct binder_transaction *transaction; // 关联的事务
    struct binder_node *target_node; // 目标 Binder 节点
    size_t data_size;                // 数据大小
    size_t offsets_size;             // 偏移数组大小
    void *data;                      // 数据起始地址
};
```

---

### 10.9 总结

```
binder_mmap  = 一次物理分配 + 双映射（内核态 + 用户态）
                    ↓
             一次拷贝优化（发送方 → 内核）
                    ↓
             接收方直接从映射区读取（零拷贝）
                    ↓
             性能 ≈ Socket 的 2 倍
             安全性 ≈ IOCTL 隔离
```

---

## 十一、binder_procs 数据结构

### 11.1 概述

`binder_procs` 是一个**全局哈希链表**，挂载了系统中所有打开了 `/dev/binder`（或 `/dev/hwbinder`、`/dev/vndbinder`）的进程的 `binder_proc` 节点。它是内核遍历和管理所有 Binder 进程的入口。

```c
// drivers/android/binder.c
static HLIST_HEAD(binder_procs);
```

这是一个**静态定义的全局变量**，使用 Linux 内核的哈希链表（`hlist_head`）实现。

---

### 11.2 数据结构层级全景

```
binder_procs（全局哈希链表头）
    │
    ├── proc_node → binder_proc A（PID=1234）
    │                   │
    │                   ├── tsk (task_struct*)
    │                   ├── pid = 1234
    │                   ├── todo (事务链表)
    │                   ├── nodes  (红黑树, 本进程导出的 Binder 节点)
    │                   ├── refs_by_desc (红黑树, 按句柄查找引用)
    │                   ├── refs_by_node (红黑树, 按 node 查找引用)
    │                   ├── threads (红黑树, Binder 线程)
    │                   ├── waiting_threads (链表)
    │                   ├── delivered_death (链表)
    │                   ├── alloc (binder_alloc, mmap 内存分配器)
    │                   ├── buffer (mmap 内核地址)
    │                   ├── buffers (所有 buffer 链表)
    │                   ├── free_buffers (空闲 buffer 链表)
    │                   ├── context (binder_context*)
    │                   └── default_priority
    │
    ├── proc_node → binder_proc B（PID=5678）
    │                   ...
    │
    └── proc_node → binder_proc C（PID=9012）
                        ...
```

---

### 11.3 binder_proc 结构体完整定义

```c
// drivers/android/binder_internal.h
struct binder_proc {
    struct hlist_node proc_node;          // 挂在 binder_procs 全局链表的节点
    struct rb_root threads;              // 红黑树，本进程的所有 Binder 线程（binder_thread）
    struct rb_root nodes;                // 红黑树，本进程导出的 Binder 节点（binder_node）
    struct rb_root refs_by_desc;         // 红黑树，按句柄（handle）查找引用（binder_ref）
    struct rb_root refs_by_node;         // 红黑树，按远程 node 查找引用（binder_ref）
    struct list_head waiting_threads;    // 等待工作的线程链表
    int pid;                             // 进程 ID
    struct task_struct *tsk;             // 线程组 leader 的 task_struct
    struct files_struct *files;          // 文件描述符表（用于 SCM 文件传递）
    struct mutex files_lock;             // files 的读写锁
    struct mutex inner_lock;             // 内部锁，保护 proc 自身数据

    struct binder_alloc alloc;           // mmap 内存分配器

    struct binder_context *context;      // Binder 上下文（binder/hwbinder/vndbinder）

    struct list_head todo;               // 待处理事务队列（binder_work 链表）
    struct list_head delivered_death;    // 已送达的死亡通知
    int outstanding_txns;                // 正在处理中的事务计数
    int pid_has_been_used;               // PID 是否已被重用
    bool has_incoming_transactions;      // 是否有传入的事务

    struct list_head buffers;            // 所有 binder_buffer 的链表
    struct list_head free_buffers;       // 空闲 binder_buffer 的链表

    // 调试 / 统计
    int max_threads;                     // 最大线程数（BINDER_MAX_THREADS）
    int requested_threads;               // 已请求的线程数
    int requested_threads_started;       // 已启动的线程数
    int ready_threads;                   // 当前空闲线程数
    long default_priority;               // 默认线程优先级（nice 值）

    struct dentry *debugfs_entry;        // debugfs 条目（用于 /d/binder/proc/<pid>）
};
```

---

### 11.4 关键子数据结构

#### 11.4.1 binder_node — 导出的 Binder 对象

当进程注册一个 Binder Service 时，内核创建一个 `binder_node`，挂入 `proc->nodes` 红黑树。

```c
struct binder_node {
    struct rb_node rb_node;              // 红黑树节点 → proc->nodes
    struct hlist_node dead_node;         // 死亡节点链表
    struct binder_proc *proc;            // 所属进程
    struct binder_work work;             // 死亡通知工作项
    struct list_head async_todo;         // 待处理的异步事务（用于 async node）
    binder_uintptr_t ptr;                // 用户空间的 Binder 对象地址
    binder_uintptr_t cookie;             // 用户空间的附加数据
    binder_uintptr_t cb_offs;            // 回调偏移
    binder_uintptr_t cb_offsets_size;    // 回调偏移大小
    int pid;                             // 创建此 node 的进程 PID
    int sched_policy;                    // 调度策略
    int min_priority;                    // 最小优先级
    bool has_async_transaction;          // 是否有未处理的异步事务
    u8 accept_fds;                       // 是否接受文件描述符
    u8 min_priority;                     // 最小优先级
    bool txns_pending;                   // 是否有事务未完成
    struct list_head refs;               // 引用此 node 的 binder_ref 链表
};
```

#### 11.4.2 binder_ref — 远程引用（句柄）

进程 A 要调用进程 B 的服务时，内核创建一个 `binder_ref`，挂入 `proc->refs_by_desc`（按键值查找）和 `proc->refs_by_node`（按 node 查找）两棵红黑树。

```c
struct binder_ref {
    struct rb_node rb_node_desc;         // 红黑树节点 → proc->refs_by_desc
    struct rb_node rb_node_node;         // 红黑树节点 → proc->refs_by_node
    struct binder_proc *proc;            // 持有此引用的进程
    struct binder_node *node;            // 引用的 Binder 节点
    struct binder_ref_data {
        int debug_id;                    // 调试 ID
        u32 desc;                        // 句柄（handle，int32 类型）
        int strong;                      // 强引用计数
        int weak;                        // 弱引用计数
    } data;
    struct list_head node_entry;         // 挂在 node->refs 链表上
};
```

#### 11.4.3 binder_thread — Binder 工作线程

```c
struct binder_thread {
    struct rb_node rb_node;              // 红黑树节点 → proc->threads
    struct binder_proc *proc;            // 所属进程
    struct binder_buffer *return_buffer; // 当前事务使用的 buffer
    int pid;                             // 线程 ID（tid）
    int looper;                          // Looper 状态（已注册/已退出等）
    bool looper_need_return;             // 是否需要返回
    struct binder_transaction *transaction_stack; // 调用栈（处理嵌套事务）
    struct list_head todo;               // 线程级待处理事务
    bool process_todo;                   // 是否有待处理事务
    struct {
        bool looper_control:1;           // 是否允许 looper 控制
    } flags;
};
```

#### 11.4.4 binder_transaction — 事务描述

```c
struct binder_transaction {
    int debug_id;                        // 调试 ID
    struct binder_work work;             // 工作项（挂在 proc->todo 或 thread->todo）
    struct binder_thread *from;          // 发起事务的线程
    struct binder_transaction *from_parent; // 父事务（嵌套调用）
    struct binder_proc *to_proc;         // 目标进程
    struct binder_thread *to_thread;     // 目标线程
    struct binder_transaction *to_parent;   // 目标父事务
    unsigned need_reply:1;               // 是否需要回复
    unsigned oneway:1;                   // 是否为单向（oneway）调用
    struct binder_buffer *buffer;        // 存放数据的内核 buffer
    binder_uintptr_t sender_pid;         // 发送方 PID
    binder_uintptr_t sender_euid;        // 发送方 UID
    struct list_head fd_fixups;          // 文件描述符修正列表
};
```

#### 11.4.5 binder_work — 工作项

```c
struct binder_work {
    struct list_head entry;              // 链表节点
    enum binder_work_type type;          // 工作类型
    // BINDER_WORK_TRANSACTION
    // BINDER_WORK_RETURN_ERROR
    // BINDER_WORK_TRANSACTION_COMPLETE
    // BINDER_WORK_DEAD_BINDER
    // BINDER_WORK_DEAD_BINDER_AND_CLEAR
    // BINDER_WORK_CLEAR_DEATH_NOTIFICATION
};
```

#### 11.4.6 binder_buffer — mmap 缓冲区

```c
struct binder_buffer {
    struct list_head entry;              // 链表节点（buffers / free_buffers）
    struct rb_node rb_node;              // 红黑树节点（按地址排序）
    unsigned free:1;                     // 是否空闲
    unsigned allow_user_free:1;          // 是否允许用户空间释放
    unsigned async:1;                    // 是否为异步事务
    struct binder_transaction *transaction; // 关联的事务
    struct binder_node *target_node;     // 目标 Binder 节点
    size_t data_size;                    // 数据大小
    size_t offsets_size;                 // 偏移数组大小
    size_t extra_buffers_size;           // 额外缓冲区大小
    void *data;                          // 数据起始地址
};
```

---

### 11.5 binder_procs 的操作

#### 11.5.1 插入（binder_open 中）

```c
// binder_open() 中
mutex_lock(&binder_procs_lock);
hlist_add_head(&proc->proc_node, &binder_procs);
mutex_unlock(&binder_procs_lock);
```

#### 11.5.2 删除（binder_release / binder_deferred_release 中）

```c
// binder_deferred_release() 中
mutex_lock(&binder_procs_lock);
hlist_del(&proc->proc_node);
mutex_unlock(&binder_procs_lock);
```

#### 11.5.3 遍历（各种场景）

```c
// 示例：遍历所有 Binder 进程
struct binder_proc *proc;
struct hlist_node *tmp;

mutex_lock(&binder_procs_lock);
hlist_for_each_entry(proc, &binder_procs, proc_node) {
    // 处理每个 proc
    pr_info("binder proc: pid=%d\n", proc->pid);
}
mutex_unlock(&binder_procs_lock);
```

---

### 11.6 调试视图

#### 11.6.1 /d/binder/proc 目录

内核通过 debugfs 为每个 `binder_proc` 创建条目：

```bash
adb shell ls -la /d/binder/proc/
# drwxr-xr-x  root     root             2026-06-02 10:00 .
# drwxr-xr-x  root     root             2026-06-02 10:00 ..
# drwxr-xr-x  root     root             2026-06-02 10:00 1234   ← PID=1234 的进程
# drwxr-xr-x  root     root             2026-06-02 10:00 5678   ← PID=5678 的进程

adb shell cat /d/binder/proc/1234
# proc 1234
# context binder
# threads: 5
# requested threads: 3
# ready threads: 1
# free async space: 524288
# nodes: 12
# refs: 45
# buffers: 8
# pending transactions: 2
```

#### 11.6.2 /d/binder/state — 全局状态

```bash
adb shell cat /d/binder/state
# Binder state:
# proc 1234
#   thread 456: l 12
#   thread 457: l 12 need_return 0
#   node 1: u00000000ffffff00 c00000000aabbccdd pri 120
#   ref 1: desc 1 node 1 s 1 w 1
#   buffer 0x7f8b4010: 256 bytes
# proc 5678
#   thread 789: l 12
#   node 2: u00000000ffee0000 c00000000ddeeff00 pri 120
#   ref 2: desc 1 node 2 s 1 w 1
```

---

### 11.7 完整的查找流程

以 ServiceManager 查找服务为例：

```
Client 进程                             内核
   │                                     │
   │── ioctl(BINDER_WRITE_READ,          │
   │     tx.target.ptr = 0               │
   │     tx.code = SVC_MGR_GET_SERVICE)  │
   │                                     │
   ▼                                     ▼
binder_ioctl() → binder_transaction()
   │                                     │
   │  1. 创建 binder_transaction         │
   │  2. 在 proc->refs_by_desc 中查找     │
   │     目标服务对应的 binder_ref        │
   │  3. 通过 ref->node 找到 binder_node │
   │  4. 通过 node->proc 找到目标进程     │
   │     (从 binder_procs 全局链表中可    │
   │      反查到的某个 proc)              │
   │  5. 分配 binder_buffer               │
   │  6. copy_from_user 数据到 buffer     │
   │  7. 将工作项挂入目标 proc->todo      │
   │  8. 唤醒目标进程的 Binder 线程       │
   │                                     │
   ▼                                     ▼
目标 Server 进程
   │
   │── 线程从 todo 取出工作项
   │── 执行 onTransact()
   │── ioctl 返回结果给 Client
```

---

### 11.8 数据结构的关联关系总结

```
binder_procs (全局哈希链表)
   │
   │── proc_node
   ▼
binder_proc (进程)
   │
   ├── threads → binder_thread (线程)
   │               │
   │               └── transaction_stack → binder_transaction (调用栈)
   │
   ├── nodes → binder_node (导出的 Binder 对象)
   │             │
   │             └── refs → binder_ref (引用链表)
   │
   ├── refs_by_desc → binder_ref (按句柄查找)
   ├── refs_by_node → binder_ref (按 node 查找)
   │
   ├── todo → binder_work (待处理事务)
   │
   ├── alloc → binder_alloc (mmap 内存分配器)
   │
   ├── buffers → binder_buffer (所有缓冲区)
   └── free_buffers → binder_buffer (空闲缓冲区)
```

---

## 十二、总结

```
Binder = IPC 机制 + RPC 框架 + 安全校验 + 一次拷贝优化
```

- **Binder Driver** — 内核模块，核心引擎
- **ServiceManager** — Android 服务的"DNS"
- **AIDL** — 开发者接口定义语言
- **Binder HAL** — 硬件抽象层通过 Binder 暴露给 Framework
- **binder_open** — 每个 Binder 进程的入口，分配和初始化 `binder_proc` 结构体
- **binder_mmap** — 实现一次拷贝优化的核心，通过 mmap 建立内核-用户共享内存映射
- **binder_procs** — 全局哈希链表，挂载所有 Binder 进程的 `binder_proc`
