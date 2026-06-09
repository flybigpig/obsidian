# Android Parcel 序列化机制

## 目录

1. [概述](#一概述)
2. [Parcel 数据结构](#二parcel-数据结构)
3. [基本数据类型序列化](#三基本数据类型序列化)
4. [Binder 对象序列化](#四binder-对象序列化)
5. [Parcel 的内存管理](#五parcel-的内存管理)
6. [Java 层 Parcel](#六java-层-parcel)
7. [性能优化技巧](#七性能优化技巧)
8. [总结](#八总结)

---

## 一、概述

Parcel 是 Android Binder 中**跨进程数据传输的容器**。它将数据序列化为扁平化的字节流，包含：

- **数据区**：存储实际的数据内容
- **偏移数组**：存储 `flat_binder_object` 和 `binder_fd_array_object` 等特殊对象在数据区中的偏移位置

### 数据布局

```
Parcel 内存布局
┌──────────────────────────────────────────────┐
│  Parcel 内部对象                             │
│  ┌────────────────────────────────────────┐  │
│  │  mData  (数据区)                      │  │
│  │  ┌──────┬──────┬──────┬──────┐       │  │
│  │  │ int  │string│ fbo  │ int  │       │  │
│  │  └──────┴──────┴──────┴──────┘       │  │
│  └────────────────────────────────────────┘  │
│                                              │
│  ┌────────────────────────────────────────┐  │
│  │  mObjects (偏移数组)                   │  │
│  │  ┌──────┐                             │  │
│  │  │off=8 │  ← 指向 fbo 在 mData 中的位置 │  │
│  │  └──────┘                             │  │
│  └────────────────────────────────────────┘  │
│                                              │
│  mDataSize  = 写入的数据总大小               │
│  mDataCapacity = 已分配的容量                │
│  mObjectsSize  = Binder 对象数量              │
│  mObjectsCapacity = 偏移数组容量              │
└──────────────────────────────────────────────┘
```

---

## 二、Parcel 数据结构

### 2.1 C++ 层 Parcel

```cpp
// frameworks/native/libs/binder/Parcel.h
class Parcel {
    // ── 数据区 ──
    uint8_t* mData;             // 数据缓冲区（序列化的字节流）
    size_t mDataSize;           // 已写入的数据大小
    size_t mDataCapacity;       // 数据缓冲区容量
    mutable size_t mDataPos;    // 当前读写位置

    // ── 偏移数组 ──
    size_t* mObjects;           // 特殊对象偏移量数组
    size_t mObjectsSize;        // 已写入的偏移量数量
    size_t mObjectsCapacity;    // 偏移数组容量

    // ── 所有者信息 ──
    status_t mError;            // 错误状态
    int32_t mOwnerCookie;       // 所有者 cookie

    // ── FD 管理 ──
    int32_t mFdsCaptured;       // 已捕获的 FD 数量（用于调试）

    // ── 辅助数据 ──
    binder_size_t* mOffsets;    // 内核使用的偏移指针
    size_t mOffsetsSize;        // 内核使用的偏移数量
};
```

### 2.2 Java 层 Parcel

```java
// frameworks/base/core/java/android/os/Parcel.java
public final class Parcel {
    // ── Native 指针（指向 C++ Parcel 对象） ──
    private long mNativePtr;

    // ── 读写位置 ──
    private long mNativeSize;

    // ── 使用标志 ──
    private boolean mOwnsNativeParcelObject;
    private boolean mRecycled;

    // ── 跨进程相关 ──
    private long mNativeReadWritePosition;
}
```

### 2.3 BINDER 内核的 Parcel 传输结构

```c
// 内核在 binder_transaction 中看到的 Parcel 数据
struct binder_transaction_data {
    // ...
    __u32 data_size;            // Parcel 数据区大小
    __u32 offsets_size;         // Parcel 偏移数组大小

    union {
        struct {
            const void __user *buffer;     // 数据区指针
            const void __user *offsets;    // 偏移数组指针
        } ptr;
        __u8 buf[8];
    } data;
};
```

---

## 三、基本数据类型序列化

### 3.1 写入和读取

```cpp
// ── 写入基本类型 ──
status_t Parcel::writeInt32(int32_t val)
{
    return writeAligned(val);    // 对齐写入 4 字节
}

status_t Parcel::writeInt64(int64_t val)
{
    return writeAligned(val);    // 对齐写入 8 字节
}

status_t Parcel::writeFloat(float val)
{
    return writeAligned(val);    // 对齐写入 4 字节
}

status_t Parcel::writeDouble(double val)
{
    return writeAligned(val);    // 对齐写入 8 字节
}

// ── 读取基本类型 ──
int32_t Parcel::readInt32() const
{
    int32_t result;
    readAligned(&result);        // 对齐读取 4 字节
    return result;
}
```

### 3.2 内存对齐规则

```cpp
// Parcel 中的所有数据都按 sizeof(void*) 对齐
// 32位: 4 字节对齐
// 64位: 8 字节对齐

static size_t alignSize(size_t size) {
    return (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
}

// 写入时自动填充 padding
status_t Parcel::writeAligned(const T& val) {
    // 1. 确保容量足够
    // 2. 对齐当前写入位置
    // 3. 写入数据
    // 4. 更新 mDataSize
}
```

### 3.3 字符串序列化

```cpp
// ── 写入 String16 ──
status_t Parcel::writeString16(const String16& str)
{
    // 格式: [长度][数据...]
    // 长度 = 字符数（不是字节数）
    writeInt32(str.size());              // 写入字符数
    writeAligned(str.string(),           // 写入 UTF-16 数据
                 str.size() * sizeof(char16_t));
    return NO_ERROR;
}

// ── 读取 String16 ──
String16 Parcel::readString16() const
{
    size_t len = readInt32();            // 读取字符数
    const char16_t* data =
        (const char16_t*)readInplace(len * sizeof(char16_t));
    return String16(data, len);
}
```

### 3.4 序列化后的内存布局示例

```
写入: writeInt32(42) writeString16("Hi") writeFloat(3.14f)

Parcel mData 布局:
┌──────────┬──────────┬──────────┬──────────┐
│ 0x2A000000│ 0x00000002│ 0x00480069│ 0x4048F5C3│
│ int32=42  │ len=2    │ "Hi"      │ float=3.14│
│ offset=0  │ offset=4 │ offset=8  │ offset=16 │
└──────────┘          ↑           ↑
                  pad(4→8)    pad(12→16)
```

---

## 四、Binder 对象序列化

### 4.1 writeStrongBinder — 写入 Binder 对象

```cpp
// frameworks/native/libs/binder/Parcel.cpp

status_t Parcel::writeStrongBinder(const sp<IBinder>& val)
{
    return flatten_binder(ProcessState::self(), val, this);
}

status_t flatten_binder(const sp<ProcessState>& proc,
                        const sp<IBinder>& binder, Parcel* out)
{
    flat_binder_object obj;

    if (binder != nullptr) {
        // ── 获取 Binder 的本地指针 ──
        BBinder *local = binder->localBinder();
        if (local) {
            // ── 本地 Binder 对象 ──
            obj.hdr.type = BINDER_TYPE_BINDER;
            obj.flags = 0;
            obj.binder = (uintptr_t)local->getWeakRefs();
            obj.cookie = (uintptr_t)local;
        } else {
            // ── 远程句柄（BpBinder） ──
            BpBinder *proxy = binder->remoteBinder();
            const int32_t handle = proxy->handle();
            obj.hdr.type = BINDER_TYPE_HANDLE;
            obj.flags = 0;
            obj.handle = handle;
            obj.cookie = 0;
        }
    } else {
        // ── 空 Binder ──
        obj.hdr.type = BINDER_TYPE_BINDER;
        obj.flags = 0;
        obj.binder = 0;
        obj.cookie = 0;
    }

    // ── 将 flat_binder_object 写入数据区 ──
    // ── 并在偏移数组中记录其位置 ──
    return out->writeObject(obj, true);
}
```

### 4.2 writeObject — 写入偏移数组

```cpp
status_t Parcel::writeObject(const flat_binder_object& val, bool nullMetaData)
{
    // ── 1. 写入 flat_binder_object 到数据区 ──
    const size_t objsize = sizeof(val);
    void* buffer = writeInplace(objsize);
    if (buffer == nullptr)
        return NO_MEMORY;

    memcpy(buffer, &val, objsize);

    // ── 2. 如果是 Binder 对象或 FD，记录偏移 ──
    if (nullMetaData || val.hdr.type != BINDER_TYPE_BINDER) {
        // 将偏移量添加到 mObjects 数组
        if (mObjectsSize >= mObjectsCapacity) {
            // 扩容偏移数组
            size_t newSize = mObjectsCapacity == 0 ? 8 : mObjectsCapacity * 2;
            size_t* newArray = (size_t*)realloc(mObjects, newSize * sizeof(size_t));
            mObjects = newArray;
            mObjectsCapacity = newSize;
        }

        // ── 记录偏移：从 mData 起始到当前对象位置 ──
        mObjects[mObjectsSize] = (uint8_t*)buffer - mData;
        mObjectsSize++;
    }

    return NO_ERROR;
}
```

### 4.3 readStrongBinder — 读取 Binder 对象

```cpp
sp<IBinder> Parcel::readStrongBinder() const
{
    sp<IBinder> val;
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
            // ── 本地 Binder：直接返回 BBinder 指针 ──
            *out = reinterpret_cast<IBinder*>(flat->cookie);
            return NO_ERROR;

        case BINDER_TYPE_HANDLE:
            // ── 远程句柄：创建 BpBinder ──
            *out = proc->getStrongProxyForHandle(flat->handle);
            return NO_ERROR;
        }
    }
    return BAD_TYPE;
}
```

### 4.4 readObject — 读取并验证偏移

```cpp
const flat_binder_object* Parcel::readObject(bool nullMetaData) const
{
    // ── 1. 从偏移数组中获取下一个偏移量 ──
    if (mObjectsPos >= mObjectsSize)
        return nullptr;

    size_t offset = mObjects[mObjectsPos];

    // ── 2. 校验偏移是否在数据区范围内 ──
    if (offset + sizeof(flat_binder_object) > mDataSize)
        return nullptr;

    // ── 3. 检查偏移是否对齐 ──
    if ((offset & (sizeof(void*)-1)) != 0)
        return nullptr;

    // ── 4. 移动偏移数组读取位置 ──
    mObjectsPos++;

    // ── 5. 返回数据区中的 flat_binder_object 指针 ──
    return reinterpret_cast<const flat_binder_object*>(mData + offset);
}
```

### 4.5 内核中的对象处理

当 Parcel 通过 `binder_transaction()` 传递给内核时，内核遍历偏移数组，找到每个 `flat_binder_object` 并执行**对象转换**（详见《flat_binder_object 对象转换》文档）：

```c
// 内核遍历偏移数组
off = (const size_t *)tr->data.ptr.offsets;
for (int i = 0; i < tr->offsets_size / sizeof(size_t); i++) {
    struct flat_binder_object *fbo;
    fbo = (struct flat_binder_object *)(buffer->data + off[i]);

    switch (fbo->hdr.type) {
        case BINDER_TYPE_BINDER:
            // BINDER → HANDLE 转换
            break;
        case BINDER_TYPE_HANDLE:
            // HANDLE 重新映射
            break;
        case BINDER_TYPE_FD:
            // FD 复制
            break;
    }
}
```

---

## 五、Parcel 的内存管理

### 5.1 扩容策略

```cpp
// 数据区扩容
uint8_t* Parcel::growData(size_t len)
{
    // ── 计算新容量（至少增长 1.5 倍） ──
    size_t newSize = ((mDataSize + len) * 3) / 2;
    // 或使用最小可用容量
    return (uint8_t*)realloc(mData, newSize);
}

// 偏移数组扩容
void Parcel::ensureObjectCapacity(size_t count)
{
    if (mObjectsCapacity >= count)
        return;

    size_t newSize = count;  // 或 count * 2
    mObjects = (size_t*)realloc(mObjects, newSize * sizeof(size_t));
    mObjectsCapacity = newSize;
}
```

### 5.2 数据所有权转移

```cpp
// 当 Parcel 传给内核后，所有权转移
// Parcel 会标记 mOwner，不再自行释放

void Parcel::ipcSetDataReference(const uint8_t* data, size_t dataSize,
                                  const binder_size_t* objects, size_t objectsCount,
                                  release_func relFunc)
{
    // ── 释放原有数据 ──
    freeDataNoInit();

    // ── 接管内核返回的数据 ──
    mData = const_cast<uint8_t*>(data);
    mDataSize = dataSize;
    mDataCapacity = dataSize;
    mObjects = const_cast<binder_size_t*>(objects);
    mObjectsSize = objectsCount;
    mObjectsCapacity = objectsCount;

    // ── 设置释放回调 ──
    mOwner = relFunc;
}
```

### 5.3 BC_FREE_BUFFER

当用户空间处理完内核发来的事务数据后，通过 `BC_FREE_BUFFER` 告知内核释放 mmap 缓冲区：

```cpp
// IPCThreadState.cpp — 事务处理完成后的清理

void IPCThreadState::freeBuffer(const uint8_t* data, size_t dataSize,
                                 const binder_size_t* objects, size_t objectsSize)
{
    // ── 通知内核释放 mmap 缓冲区 ──
    mOut.writeInt32(BC_FREE_BUFFER);
    mOut.writePointer((uintptr_t)data);
    talkWithDriver(false);
}
```

---

## 六、Java 层 Parcel

### 6.1 Java 与 Native 的映射

Java 层的 `Parcel` 是对 C++ `Parcel` 的 JNI 封装：

```java
// android/os/Parcel.java
public final class Parcel {
    // 持有 Native 对象指针
    private long mNativePtr;

    // Native 方法
    private static native long nativeCreate();
    private native void nativeWriteInt(long nativePtr, int val);
    private native int nativeReadInt(long nativePtr);
    // ...

    // Java API
    public void writeInt(int val) {
        nativeWriteInt(mNativePtr, val);
    }

    public int readInt() {
        return nativeReadInt(mNativePtr);
    }
}
```

### 6.2 Java 层序列化接口

```java
// 实现 Parcelable 接口的类可以序列化到 Parcel
public class MyData implements Parcelable {
    int id;
    String name;

    protected MyData(Parcel in) {
        id = in.readInt();           // 反序列化
        name = in.readString();
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(id);           // 序列化
        dest.writeString(name);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    public static final Creator<MyData> CREATOR = new Creator<MyData>() {
        @Override
        public MyData createFromParcel(Parcel in) {
            return new MyData(in);
        }

        @Override
        public MyData[] newArray(int size) {
            return new MyData[size];
        }
    };
}
```

### 6.3 Java 层 Binder 对象读写

```java
// 写入 Binder 对象
public final void writeStrongBinder(IBinder val) {
    nativeWriteStrongBinder(mNativePtr, val);
}

// 读取 Binder 对象
public final IBinder readStrongBinder() {
    return nativeReadStrongBinder(mNativePtr);
}

// Native 实现（JNI）
// → android_os_Parcel.cpp
// → 调用 C++ Parcel 的 writeStrongBinder() / readStrongBinder()
```

---

## 七、性能优化技巧

### 7.1 常用优化方法

```cpp
// 1. 预分配容量（避免多次扩容）
Parcel data;
data.setDataCapacity(4096);  // 预分配 4KB

// 2. 重用 Parcel（避免频繁分配/释放）
static Parcel cachedParcel;
cachedParcel.setDataPosition(0);  // 重置写入位置
cachedParcel.setDataSize(0);      // 重置数据大小

// 3. 批量写入（减少函数调用开销）
Parcel data;
data.writeInt32(count);
for (int i = 0; i < count; i++) {
    data.writeInt32(values[i]);  // 批量写入
}

// 4. 使用 writeInplace 直接写入结构体
struct Header {
    int32_t version;
    int32_t count;
    int64_t timestamp;
};
Header header = {1, 100, systemTime()};
data.writeInplace(sizeof(header));  // 直接拷贝结构体
memcpy(data.data(), &header, sizeof(header));
```

### 7.2 大数据传输优化

```cpp
// 小于 1MB: 可直接使用 Binder Parcel 传输
// 大于 1MB: 必须使用共享内存

// ❌ 超过 1MB 会抛出 TransactionTooLargeException
Parcel data;
data.writeBlob(largeData, 2 * 1024 * 1024);  // 2MB → 异常

// ✅ 使用共享内存 + FileDescriptor
// 1. 创建 ashmem / MemFd
// 2. 写入共享内存
// 3. 将 FD 通过 Binder 传递
int fd = ashmem_create_region("shared", 2 * 1024 * 1024);
void* ptr = mmap(nullptr, 2 * 1024 * 1024, PROT_READ|PROT_WRITE,
                 MAP_SHARED, fd, 0);
memcpy(ptr, largeData, 2 * 1024 * 1024);
data.writeFileDescriptor(fd);  // 只传 FD，不传数据
```

### 7.3 小数据优化

内核支持**小数据内联**（`binder_transaction_data.data.buf[8]`）：

```c
// 当数据大小 <= 8 字节时，不需要额外分配 mmap 缓冲区
// 直接内联在 binder_transaction_data 中
// 减少了 buffer 分配和拷贝的开销
```

---

## 八、总结

```
Parcel = Binder 跨进程数据传输容器
      │
      ├── 数据区 (mData): 扁平化字节流
      │     ├── 基本类型: int, float, String（对齐写入）
      │     ├── Binder 对象: flat_binder_object
      │     └── 文件描述符: flat_binder_object(BINDER_TYPE_FD)
      │
      ├── 偏移数组 (mObjects): 特殊对象的位置索引
      │     ├── 每个 flat_binder_object 记录一个偏移
      │     └── 内核遍历此数组进行对象转换
      │
      └── 核心 API
            ├── writeInt32 / readInt32        (基本类型)
            ├── writeString16 / readString16  (字符串)
            ├── writeStrongBinder / readStrongBinder (Binder 对象)
            ├── writeFileDescriptor / readFileDescriptor (FD)
            ├── writeBlob / readBlob          (大块数据)
            └── writeParcelable / createTypedParcelable (Java Parcelable)
```

| 类型 | C++ 写入 | C++ 读取 | 对齐 |
|------|---------|---------|------|
| int32_t | `writeInt32(v)` | `readInt32()` | 4 字节 |
| int64_t | `writeInt64(v)` | `readInt64()` | 8 字节 |
| float | `writeFloat(v)` | `readFloat()` | 4 字节 |
| double | `writeDouble(v)` | `readDouble()` | 8 字节 |
| String16 | `writeString16(s)` | `readString16()` | 任意+填充 |
| IBinder | `writeStrongBinder(b)` | `readStrongBinder()` | 8 字节 |
| FD | `writeFileDescriptor(fd)` | `readFileDescriptor()` | 8 字节 |
| 原生数据 | `writeInplace(len)` | `readInplace(len)` | 8 字节 |

**文件**：
- `frameworks/native/libs/binder/Parcel.cpp`
- `frameworks/native/libs/binder/Parcel.h`
- `frameworks/base/core/java/android/os/Parcel.java`
- `frameworks/base/core/jni/android_os_Parcel.cpp`
