#ifndef __DRIVER_H
#define __DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>

// ==================== 常量定义 ====================
#define DRIVER_MAX_NAME_LEN      64
#define DRIVER_MAX_DEVICES       32
#define DRIVER_MAX_DATA_SIZE     4096
#define DRIVER_IOCTL_BASE        0xDF

// ==================== 错误码定义 ====================
typedef enum {
    DRIVER_OK = 0,
    DRIVER_ERR_INVALID_PARAM = -1,
    DRIVER_ERR_NO_MEMORY = -2,
    DRIVER_ERR_DEVICE_NOT_FOUND = -3,
    DRIVER_ERR_DEVICE_BUSY = -4,
    DRIVER_ERR_ALREADY_REGISTERED = -5,
    DRIVER_ERR_NOT_OPENED = -6,
    DRIVER_ERR_PERMISSION_DENIED = -7,
    DRIVER_ERR_INVALID_IOCTL = -8,
    DRIVER_ERR_BUFFER_OVERFLOW = -9,
    DRIVER_ERR_TIMEOUT = -10
} DriverError;

// ==================== 设备标志 ====================
typedef enum {
    DRIVER_FLAG_READ = 0x01,
    DRIVER_FLAG_WRITE = 0x02,
    DRIVER_FLAG_READ_WRITE = 0x03,
    DRIVER_FLAG_NONBLOCK = 0x04,
    DRIVER_FLAG_EXCLUSIVE = 0x08
} DriverFlags;

// ==================== IOCTL 命令 ====================
#define DRIVER_IOCTL_GET_INFO    (DRIVER_IOCTL_BASE + 1)
#define DRIVER_IOCTL_SET_INFO    (DRIVER_IOCTL_BASE + 2)
#define DRIVER_IOCTL_RESET       (DRIVER_IOCTL_BASE + 3)
#define DRIVER_IOCTL_GET_SIZE    (DRIVER_IOCTL_BASE + 4)
#define DRIVER_IOCTL_SET_SIZE    (DRIVER_IOCTL_BASE + 5)

// ==================== 设备句柄 ====================
typedef struct {
    int major;                   // 主设备号
    int minor;                   // 次设备号
    uint32_t open_count;         // 打开计数
    bool is_opened;              // 是否已打开
    uint8_t *buffer;             // 数据缓冲区
    size_t buffer_size;          // 缓冲区大小
    size_t position;             // 当前位置
    uint32_t flags;              // 打开标志
    void *private_data;          // 私有数据
} DriverHandle;

// ==================== 设备操作结构体 ====================
typedef struct DriverOperations {
    int (*open)(DriverHandle *handle);
    int (*close)(DriverHandle *handle);
    ssize_t (*read)(DriverHandle *handle, void *buf, size_t count);
    ssize_t (*write)(DriverHandle *handle, const void *buf, size_t count);
    int (*ioctl)(DriverHandle *handle, unsigned int cmd, unsigned long arg);
    int (*flush)(DriverHandle *handle);
} DriverOperations;

// ==================== 设备信息 ====================
typedef struct {
    char name[DRIVER_MAX_NAME_LEN];  // 设备名称
    int major;                        // 主设备号
    int minor_start;                  // 起始次设备号
    int minor_count;                  // 次设备号数量
    bool is_registered;               // 是否已注册
    bool is_active;                   // 是否激活
    uint32_t open_count;              // 总打开次数
    DriverOperations ops;             // 设备操作
} DriverDevice;

// ==================== 核心 API ====================

// 设备管理
int driver_register_device(const char *name,
                          int major,
                          int minor_start,
                          int minor_count,
                          DriverOperations *ops);
int driver_unregister_device(const char *name);
DriverDevice* driver_find_device(const char *name);
bool driver_device_exists(const char *name);
void driver_list_devices(void);

// 设备打开/关闭
DriverHandle* driver_open(const char *name, int minor, uint32_t flags);
int driver_close(DriverHandle *handle);
bool driver_is_opened(DriverHandle *handle);

// 数据读写
ssize_t driver_read(DriverHandle *handle, void *buf, size_t count);
ssize_t driver_write(DriverHandle *handle, const void *buf, size_t count);
ssize_t driver_read_at(DriverHandle *handle, void *buf, size_t count, size_t offset);
ssize_t driver_write_at(DriverHandle *handle, const void *buf, size_t count, size_t offset);

// IOCTL 操作
int driver_ioctl(DriverHandle *handle, unsigned int cmd, unsigned long arg);

// 缓冲区管理
int driver_flush(DriverHandle *handle);
int driver_reset_buffer(DriverHandle *handle);
size_t driver_get_buffer_size(DriverHandle *handle);
int driver_set_buffer_size(DriverHandle *handle, size_t size);

// 工具函数
const char* driver_strerror(int error_code);
int driver_get_device_count(void);
uint32_t driver_get_open_count(DriverHandle *handle);
void driver_set_private_data(DriverHandle *handle, void *data);
void* driver_get_private_data(DriverHandle *handle);

// 模拟中断和回调
typedef void (*DriverCallback)(DriverHandle *handle, void *data);
int driver_register_callback(const char *name, DriverCallback cb, void *data);
int driver_trigger_callback(const char *name);

// 并发控制模拟
int driver_lock(DriverHandle *handle);
int driver_unlock(DriverHandle *handle);
bool driver_try_lock(DriverHandle *handle);

#endif // __DRIVER_H
