# Android Binder FD 传递机制（SCM 文件传递）

## 目录

1. [概述](#一概述)
2. [FD 传递的数据结构](#二fd-传递的数据结构)
3. [FD 传递流程](#三fd-传递流程)
4. [BINDER_TYPE_FD 内核处理](#四binder_type_fd-内核处理)
5. [BINDER_TYPE_FDA 数组传递](#五binder_type_fda-数组传递)
6. [FD 传递的安全与限制](#六fd-传递的安全与限制)
7. [总结](#七总结)

---

## 一、概述

Binder 支持将**文件描述符（FD）** 从一个进程传递到另一个进程。这是通过 SCM（Service Control Message）机制和内核的 `struct file` 引用计数实现的。

### 核心原理

```
进程 A                         内核                       进程 B
   │                            │                            │
   │  打开文件 fd=7             │                            │
   │  struct file *f            │                            │
   │                            │                            │
   │── BC_TRANSACTION ─────────►│                            │
   │  flat_binder_object        │                            │
   │  type=BINDER_TYPE_FD       │                            │
   │  handle=7                  │                            │
   │                            │                            │
   │                            │  1. fget(fd) → file        │
   │                            │     (增加 file 引用计数)   │
   │                            │                            │
   │                            │  2. get_unused_fd()        │
   │                            │     在进程 B 中分配新 fd   │
   │                            │                            │
   │                            │  3. fd_install(fd, file)   │
   │                            │     将 file 安装到进程 B   │
   │                            │                            │
   │                            │  4. fbo->handle = new_fd   │
   │                            │                            │
   │                            ├───────────────────────────►│
   │                            │                            │
   │  BR_TRANSACTION            │                            │── fd=32（新 fd 号）
   │  handle=32                 │                            │── 可以直接操作
```

**关键点**：传递后两个进程的 FD 指向**同一个 `struct file`**（共享同一个文件描述、偏移量、状态）。

---

## 二、FD 传递的数据结构

### 2.1 flat_binder_object — 单个 FD

```c
struct flat_binder_object {
    struct binder_object_header hdr;  // type = BINDER_TYPE_FD
    __u32 flags;                      // 标志位
    union {
        binder_uintptr_t binder;      // 未使用
        __u32 handle;                 // 发送方：原始 FD；接收方：新 FD
    };
    binder_uintptr_t cookie;          // 未使用
};
```

### 2.2 binder_fd_array_object — FD 数组

```c
// 批量传递多个 FD
struct binder_fd_array_object {
    struct binder_object_header hdr;  // type = BINDER_TYPE_FDA
    __u32 num_fds;                    // FD 数量
    __u32 parent;                     // 父缓冲区索引
    __u32 parent_offset;              // 父缓冲区中 FD 数组的偏移
};
```

### 2.3 内核使用的辅助结构

```c
// 内核中追踪 FD 修正（转换 FD 号）
struct binder_fd_fixup {
    struct list_head fixup_list;     // 链表节点
    struct file *file;               // 目标进程的 struct file
    int target_fd;                   // 目标进程的 FD 号
};
```

---

## 三、FD 传递流程

### 3.1 发送方写入 FD

```cpp
// Parcel.cpp — writeFileDescriptor()

status_t Parcel::writeFileDescriptor(int fd, bool takeOwnership)
{
    // ── 写入 flat_binder_object(BINDER_TYPE_FD) ──
    flat_binder_object obj;

    obj.hdr.type = BINDER_TYPE_FD;
    obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    obj.handle = fd;              // 发送方的原始 FD
    obj.cookie = 0;

    return writeObject(obj, true);  // 写入 Parcel 并记录偏移
}
```

### 3.2 内核转换（发送时）

```c
// binder_transaction() 中的 BINDER_TYPE_FD 处理

case BINDER_TYPE_FD:
{
    int target_fd;
    struct file *file;

    // ── 1. 从发送方进程获取 struct file ──
    //     fget() 增加 struct file 的引用计数
    file = fget(proc->files, fbo->handle);
    if (!file) {
        return_error = BR_FAILED_REPLY;
        goto err;
    }

    // ── 2. 在目标进程中分配一个新的未使用的 FD ──
    //     O_CLOEXEC 标志自动设置
    target_fd = task_get_unused_fd_flags(target_proc, O_CLOEXEC);
    if (target_fd < 0) {
        fput(file);
        return_error = BR_FAILED_REPLY;
        goto err;
    }

    // ── 3. 记录 FD 修正信息（用于后续可能的错误回滚） ──
    //     添加到事务的 fd_fixups 链表
    fixup = kzalloc(sizeof(*fixup), GFP_KERNEL);
    fixup->file = file;
    fixup->target_fd = target_fd;
    list_add_tail(&fixup->fixup_list, &t->fd_fixups);

    // ── 4. 在目标进程的 FD 表中安装 ──
    //     fd_install() 使 FD 在目标进程中可用
    //     注意：还没执行，等到事务提交时
    //     出错时可以回滚（不安装 FD）

    // ── 5. 修改 fbo：handle 替换为目标进程新 FD ──
    fbo->handle = target_fd;
    fbo->flags = 0;

    break;
}
```

### 3.3 FD 安装（事务提交时）

```c
// binder_transaction() 末尾，所有对象转换成功后

// ── 安装所有 FD ──
list_for_each_entry(fixup, &t->fd_fixups, fixup_list) {
    // 将 file 安装到目标进程的 FD 表
    task_fd_install(target_proc, fixup->target_fd, fixup->file);
}
```

### 3.4 错误回滚

```c
// 如果事务处理过程中出错，需要回滚已经分配的 FD

err_fd_fixup:
    // ── 回滚：释放已分配但未安装的 FD ──
    list_for_each_entry_safe(fixup, tmp, &t->fd_fixups, fixup_list) {
        // 在目标进程中释放这个 FD 槽位
        task_fd_uninstall(target_proc, fixup->target_fd);
        // 减少 file 引用计数（之前 fget 增加的）
        fput(fixup->file);
        list_del(&fixup->fixup_list);
        kfree(fixup);
    }
    goto err;
```

### 3.5 接收方读取 FD

```cpp
// Parcel.cpp — readFileDescriptor()

int Parcel::readFileDescriptor() const
{
    // ── 读取 flat_binder_object ──
    const flat_binder_object *flat = readObject(false);
    if (!flat) return -1;

    // ── 类型检查 ──
    if (flat->hdr.type == BINDER_TYPE_FD) {
        // ── 返回新的 FD 号 ──
        // 此时 handle 已经是目标进程的 FD 号
        return flat->handle;
    }

    return -1;
}
```

### 3.6 FD 的生命周期

```
发送方进程                        内核                          接收方进程
   │                              │                              │
   │ open() → fd=7                │                              │
   │ struct file ref=1            │                              │
   │                              │                              │
   │ writeFileDescriptor(7)       │                              │
   │── BC_TRANSACTION ───────────►│                              │
   │                              │  fget(7) → ref=2            │
   │                              │  get_unused_fd() → fd=32     │
   │                              │  fd_install(32)              │
   │                              │                              │
   │                              ├─────────────────────────────►│
   │                              │                              │── BR_TRANSACTION
   │                              │                              │── handle=32
   │                              │                              │   struct file ref=2
   │                              │                              │
   │ close(7)                     │                              │
   │── fput → ref=1               │                              │
   │                              │                              │
   │                              │                              │ readFileDescriptor()
   │                              │                              │── 使用 fd=32
   │                              │                              │
   │                              │                              │ close(32)
   │                              │                              │── fput → ref=0
   │                              │                              │── 释放 file
   │                              │                              │   （真正的文件关闭）
```

---

## 四、BINDER_TYPE_FD 内核处理

### 4.1 完整源码解析

```c
// drivers/android/binder.c — binder_transaction()

case BINDER_TYPE_FD:
{
    struct binder_fd_fixup *fixup;
    int target_fd;
    struct file *file;

    // ── 1. 安全检查：校验 flags ──
    if (!(fbo->flags & FLAT_BINDER_FLAG_ACCEPTS_FDS)) {
        // 接收方没有声明接受 FD
        // 但这不是硬性错误，只是警告
        pr_warn_once("binder: type %d flags 0x%x\n",
                     fbo->hdr.type, fbo->flags);
    }

    // ── 2. 获取发送方 FD 对应的 struct file ──
    //     fget() 会做边界检查（避免 FD 越界）
    file = fget(proc->files, fbo->handle);
    if (!file) {
        // 发送方的 FD 无效
        return_error_param = -EBADF;
        return_error = BR_FAILED_REPLY;
        goto err_fd_fixup;
    }

    // ── 3. 在目标进程中分配新 FD ──
    //     自动设置 O_CLOEXEC
    //     如果目标进程 FD 用完，会返回 -EMFILE
    target_fd = task_get_unused_fd_flags(target_proc, O_CLOEXEC);
    if (target_fd < 0) {
        fput(file);
        return_error_param = -EMFILE;
        return_error = BR_FAILED_REPLY;
        goto err_fd_fixup;
    }

    // ── 4. 创建修正记录（延迟安装） ──
    fixup = kzalloc(sizeof(*fixup), GFP_KERNEL);
    if (!fixup) {
        // 目标进程的 FD 槽位被浪费了（难以回滚）
        // 这种情况极少发生
        task_fd_uninstall(target_proc, target_fd);
        fput(file);
        return_error_param = -ENOMEM;
        return_error = BR_FAILED_REPLY;
        goto err_fd_fixup;
    }

    fixup->file = file;
    fixup->target_fd = target_fd;

    // ── 5. 添加到事务的修正链表 ──
    list_add_tail(&fixup->fixup_list, &t->fd_fixups);

    // ── 6. 修改 fbo ──
    fbo->binder = 0;
    fbo->handle = target_fd;    // 替换为目标进程的 FD
    fbo->cookie = 0;

    break;
}
```

### 4.2 FD 安装的时机

```c
// binder_transaction() 最后阶段

// ── 所有 Binder 对象转换完成，所有 FD 分配完成 ──
// ── 此时才能安装 FD（因为之前如果出错可以回滚）──

if (!list_empty(&t->fd_fixups)) {
    list_for_each_entry(fixup, &t->fd_fixups, fixup_list) {
        // ── 将 file 安装到目标进程 ──
        task_fd_install(target_proc, fixup->target_fd, fixup->file);
    }
}

// ── 然后才挂入目标队列 ──
// ── 确保 FD 安装完成后再唤醒目标线程 ──
binder_enqueue_work(&t->work, &target_proc->todo);
wake_up_interruptible(&target_proc->wait);
```

### 4.3 接收方如何使用传递来的 FD

```cpp
// 服务端收到包含 FD 的事务

void BnMyService::onTransact(uint32_t code,
                             const Parcel& data,
                             Parcel* reply, uint32_t flags)
{
    switch (code) {
    case TRANSACTION_OPEN_FILE:
    {
        // ── 读取 FD ──
        int fd = data.readFileDescriptor();

        // ── 使用 FD ──
        // 这个 FD 在接收方进程中有效
        char buf[256];
        read(fd, buf, sizeof(buf));

        // ── 可以通过 dup() 复制 ──
        int new_fd = dup(fd);

        // ── 使用完后关闭 ──
        close(fd);

        reply->writeInt32(0);
        break;
    }
    }
}
```

---

## 五、BINDER_TYPE_FDA 数组传递

### 5.1 Parcel 写入 FD 数组

```cpp
// Parcel.cpp
status_t Parcel::writeFileDescriptorVector(const std::vector<int>& fds)
{
    for (size_t i = 0; i < fds.size(); i++) {
        // ── 写入每个 FD ──
        writeFileDescriptor(fds[i], false);
    }

    // ── 写入描述 FD 数组的 binder_fd_array_object ──
    binder_fd_array_object fda;
    fda.hdr.type = BINDER_TYPE_FDA;
    fda.num_fds = fds.size();
    fda.parent = ...;        // 父缓冲区索引
    fda.parent_offset = ...; // 父缓冲区中 FD 偏移

    return writeObject(fda, true);
}
```

### 5.2 内核处理 FD 数组

```c
// binder_transaction() 中的 BINDER_TYPE_FDA 处理

case BINDER_TYPE_FDA:
{
    struct binder_fd_array_object *fda = ...;
    size_t num_fds = fda->num_fds;

    // ── 遍历 FD 数组，逐个处理 ──
    for (size_t i = 0; i < num_fds; i++) {
        // 定位到数组中的第 i 个 fd
        int *fd_ptr = parent_buffer + fda->parent_offset + i * sizeof(int);
        int fd = *fd_ptr;

        // ── 同单个 FD 的处理逻辑 ──
        struct file *file = fget(proc->files, fd);
        int target_fd = task_get_unused_fd_flags(target_proc, O_CLOEXEC);

        // 记录修正
        struct binder_fd_fixup *fixup = kzalloc(...);
        fixup->file = file;
        fixup->target_fd = target_fd;
        list_add_tail(&fixup->fixup_list, &t->fd_fixups);

        // ── 原地替换 FD 号 ──
        *fd_ptr = target_fd;
    }
    break;
}
```

---

## 六、FD 传递的安全与限制

### 6.1 安全特性

| 特性 | 说明 |
|------|------|
| **自动 O_CLOEXEC** | 传递的 FD 自动设置 close-on-exec，防止子进程继承 |
| **引用计数安全** | `fget()`/`fput()` 保证 `struct file` 生命周期 |
| **事务回滚** | 出错时释放已分配的 FD 槽位 |
| **无权限提升** | 接收方获得的 FD 指向同一个 `struct file`，权限相同 |

### 6.2 限制

| 限制 | 说明 |
|------|------|
| **FD 号不同** | 发送方 fd=7，接收方可能是 fd=32 |
| **无法传递目录 FD** | Binder 未实现 `O_PATH` 类型 FD 的传递 |
| **FD 总数限制** | 受 `RLIMIT_NOFILE` 限制（通常 1024） |
| **无所有权转移语义** | 发送方仍然可以继续使用原始 FD |
| **大数据量不适用** | 批量传递大量 FD 时，每个 FD 需要内核对象分配 |

### 6.3 典型使用场景

```java
// 1. SurfaceFlinger 传递图形缓冲区 FD
// App 通过 Binder 将 graphic buffer 的 FD 传给 SurfaceFlinger
Parcel data;
data.writeFileDescriptor(dma_buf_fd);
mSurfaceFlinger.transact(TRANSACTION_SET_BUFFER, data, ...);

// 2. MediaServer 传递解码器输出 FD
// MediaCodec 输出 MediaFormat 中包含 ashmem FD

// 3. CameraService 传递拍照结果
// Camera HAL 输出 JPEG 数据的共享内存 FD

// 4. ContentProvider 传递文件
// ContentProvider 返回打开文件的 ParcelFileDescriptor
ParcelFileDescriptor pfd = contentResolver.openFileDescriptor(uri, "r");
// 内部通过 Binder FD 传递实现
```

### 6.4 ParcelFileDescriptor 封装

```java
// Java 层封装了 Binder FD 传递
// frameworks/base/core/java/android/os/ParcelFileDescriptor.java

public class ParcelFileDescriptor implements Parcelable {
    private final FileDescriptor mFd;

    // 写 Parcel 时
    @Override
    public void writeToParcel(Parcel dest, int flags) {
        // 调用 native 写入 BINDER_TYPE_FD
        dest.writeFileDescriptor(mFd);
    }

    // 读 Parcel 时
    public static final Parcelable.Creator<ParcelFileDescriptor> CREATOR =
        new Parcelable.Creator<ParcelFileDescriptor>() {
        public ParcelFileDescriptor createFromParcel(Parcel in) {
            // 调用 native 读取 FD
            FileDescriptor fd = in.readFileDescriptor();
            return new ParcelFileDescriptor(fd);
        }
    };
}
```

---

## 七、总结

```
Binder FD 传递 = 跨进程文件描述符共享
      │
      ├── 原理: struct file 引用计数 +1，在目标进程安装新 FD
      │
      ├── 类型:
      │     ├── BINDER_TYPE_FD  — 单个 FD
      │     └── BINDER_TYPE_FDA — FD 数组
      │
      ├── 流程:
      │     发送方 fget() → 目标 get_unused_fd() → fd_install()
      │     → 原地替换 handle → 接收方直接使用
      │
      └── 特点:
            ├── 共享同一个 struct file（偏移量、状态同步）
            ├── 自动 O_CLOEXEC
            ├── 支持事务回滚
            ├── 无所有权转移（发送方可继续使用）
            └── 受 RLIMIT_NOFILE 限制
```

**文件**：
- `drivers/android/binder.c` — `binder_transaction()` 中的 BINDER_TYPE_FD/BINDER_TYPE_FDA 处理
- `frameworks/native/libs/binder/Parcel.cpp` — `writeFileDescriptor()` / `readFileDescriptor()`
- `frameworks/base/core/java/android/os/ParcelFileDescriptor.java`
