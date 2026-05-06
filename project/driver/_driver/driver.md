# Driver 测试框架

## 项目简介

这是一个模拟 Linux 字符设备驱动程序的测试框架。该框架模拟了 Linux 驱动的核心功能，包括设备注册、打开/关闭、数据读写、IOCTL 操作、缓冲区管理等。

## 项目结构

```
driver/
├── driver.md              # 项目文档
├── include/
│   └── driver.h          # 驱动接口头文件
├── src/
│   └── driver.c         # 驱动实现
├── test/
│   └── test_driver.c    # 测试用例（25 个测试）
├── build/                # 编译输出目录
├── Makefile              # Make 构建文件
├── CMakeLists.txt        # CMake 配置
├── libdriver.a          # 生成的静态库
└── test_runner          # 测试程序
```

## 功能特性

### 核心功能

1. **设备管理**
   - 设备注册/注销
   - 设备打开/关闭
   - 多设备支持
   - 设备列表查看

2. **数据读写**
   - 顺序读写
   - 定位读写（at 位置）
   - 权限检查（读/写/读写）
   - 缓冲区自动扩展

3. **IOCTL 操作**
   - 获取/设置设备信息
   - 重置设备
   - 获取/设置缓冲区大小
   - 自定义 IOCTL 命令

4. **高级功能**
   - 私有数据管理
   - 回调机制（模拟中断）
   - 并发锁模拟
   - 缓冲区管理

5. **错误处理**
   - 参数校验
   - 权限检查
   - 设备状态检查
   - 错误码返回

## API 说明

### 设备管理

```c
// 注册设备
int driver_register_device(const char *name,
                          int major,
                          int minor_start,
                          int minor_count,
                          DriverOperations *ops);

// 注销设备
int driver_unregister_device(const char *name);

// 查找设备
DriverDevice* driver_find_device(const char *name);

// 检查设备是否存在
bool driver_device_exists(const char *name);

// 列出所有设备
void driver_list_devices(void);
```

### 设备打开/关闭

```c
// 打开设备
DriverHandle* driver_open(const char *name, int minor, uint32_t flags);

// 关闭设备
int driver_close(DriverHandle *handle);

// 检查设备是否已打开
bool driver_is_opened(DriverHandle *handle);
```

### 数据读写

```c
// 读取数据
ssize_t driver_read(DriverHandle *handle, void *buf, size_t count);

// 写入数据
ssize_t driver_write(DriverHandle *handle, const void *buf, size_t count);

// 在指定位置读取
ssize_t driver_read_at(DriverHandle *handle, void *buf, size_t count, size_t offset);

// 在指定位置写入
ssize_t driver_write_at(DriverHandle *handle, const void *buf, size_t count, size_t offset);
```

### IOCTL 操作

```c
// IOCTL 操作
int driver_ioctl(DriverHandle *handle, unsigned int cmd, unsigned long arg);

// 预定义的 IOCTL 命令
#define DRIVER_IOCTL_GET_INFO    (0xDF + 1)
#define DRIVER_IOCTL_SET_INFO    (0xDF + 2)
#define DRIVER_IOCTL_RESET       (0xDF + 3)
#define DRIVER_IOCTL_GET_SIZE    (0xDF + 4)
#define DRIVER_IOCTL_SET_SIZE    (0xDF + 5)
```

### 缓冲区管理

```c
// 刷新缓冲区
int driver_flush(DriverHandle *handle);

// 重置缓冲区位置
int driver_reset_buffer(DriverHandle *handle);

// 获取缓冲区大小
size_t driver_get_buffer_size(DriverHandle *handle);

// 设置缓冲区大小
int driver_set_buffer_size(DriverHandle *handle, size_t size);
```

### 高级功能

```c
// 设置/获取私有数据
void driver_set_private_data(DriverHandle *handle, void *data);
void* driver_get_private_data(DriverHandle *handle);

// 注册回调（模拟中断）
int driver_register_callback(const char *name, DriverCallback cb, void *data);
int driver_trigger_callback(const char *name);

// 并发锁
int driver_lock(DriverHandle *handle);
int driver_unlock(DriverHandle *handle);
bool driver_try_lock(DriverHandle *handle);
```

### 工具函数

```c
// 获取错误信息字符串
const char* driver_strerror(int error_code);

// 获取设备数量
int driver_get_device_count(void);

// 获取打开计数
uint32_t driver_get_open_count(DriverHandle *handle);
```

### 设备操作结构体

```c
typedef struct DriverOperations {
    int (*open)(DriverHandle *handle);
    int (*close)(DriverHandle *handle);
    ssize_t (*read)(DriverHandle *handle, void *buf, size_t count);
    ssize_t (*write)(DriverHandle *handle, const void *buf, size_t count);
    int (*ioctl)(DriverHandle *handle, unsigned int cmd, unsigned long arg);
    int (*flush)(DriverHandle *handle);
} DriverOperations;
```

### 设备标志

```c
typedef enum {
    DRIVER_FLAG_READ = 0x01,         // 只读
    DRIVER_FLAG_WRITE = 0x02,        // 只写
    DRIVER_FLAG_READ_WRITE = 0x03,   // 读写
    DRIVER_FLAG_NONBLOCK = 0x04,    // 非阻塞
    DRIVER_FLAG_EXCLUSIVE = 0x08    // 独占
} DriverFlags;
```

## 测试用例

本项目包含 **25 个测试用例**，覆盖以下功能：

| 测试编号 | 测试名称 | 测试内容 |
|---------|---------|---------|
| 1 | Driver Register/Unregister | 设备注册和注销 |
| 2 | Driver Duplicate Register | 重复注册检测 |
| 3 | Driver Unregister Nonexistent | 注销不存在的设备 |
| 4 | Driver Open/Close | 设备打开和关闭 |
| 5 | Driver Open Nonexistent | 打开不存在的设备 |
| 6 | Driver Close Invalid | 关闭无效句柄 |
| 7 | Driver Read/Write | 基础数据读写 |
| 8 | Driver Read Permission | 读取权限检查 |
| 9 | Driver Write Permission | 写入权限检查 |
| 10 | Driver IOCTL | IOCTL 操作 |
| 11 | Driver Buffer Management | 缓冲区管理 |
| 12 | Driver Read/Write At Position | 定位读写 |
| 13 | Driver Multiple Devices | 多设备管理 |
| 14 | Driver Open Count | 打开计数 |
| 15 | Driver Private Data | 私有数据管理 |
| 16 | Driver List Devices | 设备列表 |
| 17 | Driver Edge Cases | 边界条件测试 |
| 18 | Driver Strerror | 错误信息转换 |
| 19 | Driver Custom Operations | 自定义操作函数 |
| 20 | Driver Callback | 回调机制 |
| 21 | Driver Lock | 并发锁模拟 |
| 22 | Driver Buffer Overflow | 缓冲区溢出测试 |
| 23 | Driver Not Initialized | 未初始化错误处理 |
| 24 | Driver Stress Large Data | 大量数据压力测试 |
| 25 | Driver Device Busy | 设备忙检测 |

## 构建和运行

### 使用 Make

```bash
# 构建静态库
make all

# 构建并运行测试
make test

# 运行测试（不重新编译）
make run_test

# 清理构建产物
make clean

# 查看帮助
make help

# 调试版本
make debug

# 优化版本
make optimized
```

### 使用 CMake

```bash
# 创建构建目录
mkdir -p build && cd build

# 配置
cmake ..

# 编译
make

# 运行测试
./bin/test_runner

# 或使用 CTest
ctest

# 清理
make clean
```

## 错误码说明

| 错误码 | 值 | 说明 |
|-------|-----|------|
| DRIVER_OK | 0 | 成功 |
| DRIVER_ERR_INVALID_PARAM | -1 | 无效参数 |
| DRIVER_ERR_NO_MEMORY | -2 | 内存不足 |
| DRIVER_ERR_DEVICE_NOT_FOUND | -3 | 设备未找到 |
| DRIVER_ERR_DEVICE_BUSY | -4 | 设备忙 |
| DRIVER_ERR_ALREADY_REGISTERED | -5 | 已注册 |
| DRIVER_ERR_NOT_OPENED | -6 | 未打开 |
| DRIVER_ERR_PERMISSION_DENIED | -7 | 权限拒绝 |
| DRIVER_ERR_INVALID_IOCTL | -8 | 无效 IOCTL 命令 |
| DRIVER_ERR_BUFFER_OVERFLOW | -9 | 缓冲区溢出 |
| DRIVER_ERR_TIMEOUT | -10 | 超时 |

## 使用示例

### 基础使用

```c
#include "driver.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    // 1. 注册设备（使用默认操作）
    int result = driver_register_device("my_device", 0, 0, 1, NULL);
    if (result != DRIVER_OK) {
        printf("Failed to register device: %s\n", driver_strerror(result));
        return 1;
    }

    // 2. 打开设备
    DriverHandle *handle = driver_open("my_device", 0, DRIVER_FLAG_READ_WRITE);
    if (handle == NULL) {
        printf("Failed to open device\n");
        driver_unregister_device("my_device");
        return 1;
    }

    // 3. 写入数据
    const char *data = "Hello Driver!";
    ssize_t written = driver_write(handle, data, strlen(data));
    printf("Written %zd bytes\n", written);

    // 4. 重置位置并读取
    driver_reset_buffer(handle);
    char buffer[64] = {0};
    ssize_t read = driver_read(handle, buffer, sizeof(buffer) - 1);
    printf("Read %zd bytes: %s\n", read, buffer);

    // 5. 关闭设备
    driver_close(handle);

    // 6. 注销设备
    driver_unregister_device("my_device");

    return 0;
}
```

### 自定义操作函数

```c
#include "driver.h"
#include <stdio.h>

// 自定义打开函数
static int my_open(DriverHandle *handle) {
    printf("Device opened\n");
    return DRIVER_OK;
}

// 自定义读取函数
static ssize_t my_read(DriverHandle *handle, void *buf, size_t count) {
    const char *msg = "Hello from custom driver!";
    size_t len = strlen(msg);
    size_t to_copy = len < count ? len : count;
    memcpy(buf, msg, to_copy);
    return (ssize_t)to_copy;
}

int main(void) {
    // 定义自定义操作
    DriverOperations ops;
    ops.open = my_open;
    ops.close = NULL;  // 使用默认
    ops.read = my_read;
    ops.write = NULL; // 使用默认
    ops.ioctl = NULL;  // 使用默认
    ops.flush = NULL;  // 使用默认

    // 注册设备（使用自定义操作）
    int result = driver_register_device("custom_dev", 0, 0, 1, &ops);
    if (result != DRIVER_OK) {
        printf("Failed to register device: %s\n", driver_strerror(result));
        return 1;
    }

    // 打开并读取
    DriverHandle *handle = driver_open("custom_dev", 0, DRIVER_FLAG_READ);
    if (handle != NULL) {
        char buf[64] = {0};
        driver_read(handle, buf, sizeof(buf) - 1);
        printf("Read: %s\n", buf);
        driver_close(handle);
    }

    driver_unregister_device("custom_dev");
    return 0;
}
```

### 使用 IOCTL

```c
#include "driver.h"
#include <stdio.h>

int main(void) {
    // 注册并打开设备
    driver_register_device("ioctl_dev", 0, 0, 1, NULL);
    DriverHandle *handle = driver_open("ioctl_dev", 0, DRIVER_FLAG_READ_WRITE);

    if (handle != NULL) {
        // 重置设备
        int result = driver_ioctl(handle, DRIVER_IOCTL_RESET, 0);
        if (result == DRIVER_OK) {
            printf("Device reset successfully\n");
        }

        // 获取缓冲区大小
        driver_ioctl(handle, DRIVER_IOCTL_GET_SIZE, 0);

        // 设置缓冲区大小
        int new_size = 2048;
        driver_ioctl(handle, DRIVER_IOCTL_SET_SIZE, new_size);

        driver_close(handle);
    }

    driver_unregister_device("ioctl_dev");
    return 0;
}
```

## 技术要点

### 驱动机制模拟

本框架模拟了 Linux 字符设备驱动的核心概念：

1. **设备注册** - 向系统注册设备
2. **设备号** - 主设备号和次设备号
3. **文件操作** - open、close、read、write、ioctl
4. **缓冲区管理** - 内核缓冲区的用户空间模拟
5. **权限控制** - 读/写权限检查

### 默认操作

当注册设备时传递 `NULL` 作为操作参数，框架会使用默认操作：

- `default_open()` - 空操作，返回成功
- `default_close()` - 空操作，返回成功
- `default_read()` - 从缓冲区读取数据
- `default_write()` - 向缓冲区写入数据（自动扩展）
- `default_ioctl()` - 支持 RESET 命令
- `default_flush()` - 空操作，返回成功

### 内存管理

- 使用 `malloc/free` 管理动态内存
- 缓冲区自动扩展
- 防止内存泄漏

## 扩展功能

### 已添加的特定测试场景

✅ **多设备并发管理测试** - 测试同时注册多个设备  
✅ **缓冲区边界测试** - 测试读写操作的边界条件  
✅ **权限控制测试** - 测试读写权限检查  
✅ **定位读写测试** - 测试在指定位置读写数据  
✅ **并发锁模拟测试** - 测试锁的获取和释放  
✅ **回调机制测试** - 测试中断回调的注册和触发  
✅ **压力测试** - 测试大量数据的读写  
✅ **错误处理全覆盖** - 测试所有错误路径  

## 参考资源

- Linux 字符设备驱动
- Linux 内核文件系统
- POSIX 文件 I/O
- 设备驱动程序开发

## 许可证

本项目的所有代码均为测试框架，仅供参考和学习使用。
