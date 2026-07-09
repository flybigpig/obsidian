#include "driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ==================== 全局状态 ====================
static DriverDevice g_devices[DRIVER_MAX_DEVICES];
static int g_device_count = 0;
static int g_next_major = 240;  // 动态主设备号起始

// ==================== 内部辅助函数 ====================
static int find_device_by_name(const char *name) {
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].is_registered &&
            strcmp(g_devices[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_free_device_slot(void) {
    for (int i = 0; i < DRIVER_MAX_DEVICES; i++) {
        if (!g_devices[i].is_registered) {
            return i;
        }
    }
    return -1;
}

static int allocate_major(void) {
    return g_next_major++;
}

// ==================== 默认操作实现 ====================
static int default_open(DriverHandle *handle) {
    (void)handle;
    return DRIVER_OK;
}

static int default_close(DriverHandle *handle) {
    (void)handle;
    return DRIVER_OK;
}

static ssize_t default_read(DriverHandle *handle, void *buf, size_t count) {
    if (handle == NULL || buf == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    size_t available = handle->buffer_size - handle->position;
    size_t to_read = available < count ? available : count;

    if (to_read == 0) {
        return 0;  // EOF
    }

    memcpy(buf, handle->buffer + handle->position, to_read);
    handle->position += to_read;

    return (ssize_t)to_read;
}

static ssize_t default_write(DriverHandle *handle, const void *buf, size_t count) {
    if (handle == NULL || buf == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    if (handle->position + count > handle->buffer_size) {
        // 自动扩展缓冲区
        uint8_t *new_buf = realloc(handle->buffer, handle->position + count);
        if (new_buf == NULL) {
            return DRIVER_ERR_NO_MEMORY;
        }
        handle->buffer = new_buf;
        handle->buffer_size = handle->position + count;
    }

    memcpy(handle->buffer + handle->position, buf, count);
    handle->position += count;

    return (ssize_t)count;
}

static int default_ioctl(DriverHandle *handle, unsigned int cmd, unsigned long arg) {
    (void)arg;

    switch (cmd) {
        case DRIVER_IOCTL_RESET:
            if (handle != NULL) {
                handle->position = 0;
            }
            return DRIVER_OK;
        default:
            return DRIVER_ERR_INVALID_IOCTL;
    }
}

static int default_flush(DriverHandle *handle) {
    (void)handle;
    return DRIVER_OK;
}

// ==================== 设备管理实现 ====================
int driver_register_device(const char *name,
                          int major,
                          int minor_start,
                          int minor_count,
                          DriverOperations *ops) {
    if (name == NULL || strlen(name) == 0) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    if (minor_count <= 0 || minor_count > 256) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    // 检查设备是否已注册
    if (find_device_by_name(name) >= 0) {
        return DRIVER_ERR_ALREADY_REGISTERED;
    }

    // 查找空闲槽位
    int slot = find_free_device_slot();
    if (slot < 0) {
        return DRIVER_ERR_NO_MEMORY;
    }

    // 分配主设备号
    if (major <= 0) {
        major = allocate_major();
    }

    // 注册设备
    strncpy(g_devices[slot].name, name, DRIVER_MAX_NAME_LEN - 1);
    g_devices[slot].name[DRIVER_MAX_NAME_LEN - 1] = '\0';
    g_devices[slot].major = major;
    g_devices[slot].minor_start = minor_start;
    g_devices[slot].minor_count = minor_count;
    g_devices[slot].is_registered = true;
    g_devices[slot].is_active = true;
    g_devices[slot].open_count = 0;

    // 设置操作函数
    if (ops != NULL) {
        memcpy(&g_devices[slot].ops, ops, sizeof(DriverOperations));
    } else {
        // 使用默认操作
        g_devices[slot].ops.open = default_open;
        g_devices[slot].ops.close = default_close;
        g_devices[slot].ops.read = default_read;
        g_devices[slot].ops.write = default_write;
        g_devices[slot].ops.ioctl = default_ioctl;
        g_devices[slot].ops.flush = default_flush;
    }

    if (slot >= g_device_count) {
        g_device_count = slot + 1;
    }

    printf("[DRIVER] Device registered: name=%s, major=%d, minor=%d-%d\n",
           name, major, minor_start, minor_start + minor_count - 1);

    return DRIVER_OK;
}

int driver_unregister_device(const char *name) {
    if (name == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    int index = find_device_by_name(name);
    if (index < 0) {
        return DRIVER_ERR_DEVICE_NOT_FOUND;
    }

    // 检查设备是否还在使用
    if (g_devices[index].open_count > 0) {
        return DRIVER_ERR_DEVICE_BUSY;
    }

    printf("[DRIVER] Device unregistered: name=%s\n", name);

    g_devices[index].is_registered = false;
    g_devices[index].is_active = false;
    g_devices[index].name[0] = '\0';
    g_devices[index].open_count = 0;

    return DRIVER_OK;
}

DriverDevice* driver_find_device(const char *name) {
    if (name == NULL) {
        return NULL;
    }

    int index = find_device_by_name(name);
    if (index < 0) {
        return NULL;
    }

    return &g_devices[index];
}

bool driver_device_exists(const char *name) {
    return find_device_by_name(name) >= 0;
}

void driver_list_devices(void) {
    printf("[DRIVER] === Device List ===\n");
    int count = 0;
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].is_registered) {
            printf("  [%d] name=%s, major=%d, minor=%d-%d, opens=%u\n",
                   count,
                   g_devices[i].name,
                   g_devices[i].major,
                   g_devices[i].minor_start,
                   g_devices[i].minor_start + g_devices[i].minor_count - 1,
                   g_devices[i].open_count);
            count++;
        }
    }
    if (count == 0) {
        printf("  (no devices registered)\n");
    }
    printf("[DRIVER] =====================\n");
}

// ==================== 设备打开/关闭实现 ====================
DriverHandle* driver_open(const char *name, int minor, uint32_t flags) {
    if (name == NULL) {
        return NULL;
    }

    int index = find_device_by_name(name);
    if (index < 0) {
        return NULL;
    }

    DriverDevice *dev = &g_devices[index];

    // 检查次设备号是否有效
    if (minor < dev->minor_start ||
        minor >= dev->minor_start + dev->minor_count) {
        return NULL;
    }

    // 创建句柄
    DriverHandle *handle = (DriverHandle*)malloc(sizeof(DriverHandle));
    if (handle == NULL) {
        return NULL;
    }

    memset(handle, 0, sizeof(DriverHandle));
    handle->major = dev->major;
    handle->minor = minor;
    handle->open_count = 1;
    handle->is_opened = true;
    handle->flags = flags;
    handle->position = 0;
    handle->buffer_size = 1024;  // 默认缓冲区大小

    // 分配缓冲区
    handle->buffer = (uint8_t*)malloc(handle->buffer_size);
    if (handle->buffer == NULL) {
        free(handle);
        return NULL;
    }
    memset(handle->buffer, 0, handle->buffer_size);

    // 调用设备的 open 回调
    if (dev->ops.open != NULL) {
        if (dev->ops.open(handle) != DRIVER_OK) {
            free(handle->buffer);
            free(handle);
            return NULL;
        }
    }

    dev->open_count++;

    printf("[DRIVER] Device opened: name=%s, minor=%d, flags=0x%x\n",
           name, minor, flags);

    return handle;
}

int driver_close(DriverHandle *handle) {
    if (handle == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    if (!handle->is_opened) {
        return DRIVER_ERR_NOT_OPENED;
    }

    // 查找设备
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].is_registered &&
            g_devices[i].major == handle->major) {
            g_devices[i].open_count--;

            // 调用设备的 close 回调
            if (g_devices[i].ops.close != NULL) {
                g_devices[i].ops.close(handle);
            }

            break;
        }
    }

    printf("[DRIVER] Device closed: major=%d, minor=%d\n",
           handle->major, handle->minor);

    handle->is_opened = false;

    if (handle->buffer != NULL) {
        free(handle->buffer);
    }

    if (handle->private_data != NULL) {
        free(handle->private_data);
    }

    free(handle);

    return DRIVER_OK;
}

bool driver_is_opened(DriverHandle *handle) {
    if (handle == NULL) {
        return false;
    }
    return handle->is_opened;
}

// ==================== 数据读写实现 ====================
ssize_t driver_read(DriverHandle *handle, void *buf, size_t count) {
    if (handle == NULL || buf == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    if (!handle->is_opened) {
        return DRIVER_ERR_NOT_OPENED;
    }

    if (!(handle->flags & DRIVER_FLAG_READ)) {
        return DRIVER_ERR_PERMISSION_DENIED;
    }

    // 查找设备并调用 read 回调
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].is_registered &&
            g_devices[i].major == handle->major &&
            g_devices[i].ops.read != NULL) {
            return g_devices[i].ops.read(handle, buf, count);
        }
    }

    return DRIVER_ERR_DEVICE_NOT_FOUND;
}

ssize_t driver_write(DriverHandle *handle, const void *buf, size_t count) {
    if (handle == NULL || buf == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    if (!handle->is_opened) {
        return DRIVER_ERR_NOT_OPENED;
    }

    if (!(handle->flags & DRIVER_FLAG_WRITE)) {
        return DRIVER_ERR_PERMISSION_DENIED;
    }

    // 查找设备并调用 write 回调
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].is_registered &&
            g_devices[i].major == handle->major &&
            g_devices[i].ops.write != NULL) {
            return g_devices[i].ops.write(handle, buf, count);
        }
    }

    return DRIVER_ERR_DEVICE_NOT_FOUND;
}

ssize_t driver_read_at(DriverHandle *handle, void *buf, size_t count, size_t offset) {
    if (handle == NULL || buf == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    if (offset >= handle->buffer_size) {
        return 0;  // EOF
    }

    size_t old_pos = handle->position;
    handle->position = offset;

    ssize_t result = driver_read(handle, buf, count);

    handle->position = old_pos;

    return result;
}

ssize_t driver_write_at(DriverHandle *handle, const void *buf, size_t count, size_t offset) {
    if (handle == NULL || buf == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    size_t old_pos = handle->position;
    handle->position = offset;

    ssize_t result = driver_write(handle, buf, count);

    handle->position = old_pos;

    return result;
}

// ==================== IOCTL 操作实现 ====================
int driver_ioctl(DriverHandle *handle, unsigned int cmd, unsigned long arg) {
    if (handle == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    if (!handle->is_opened) {
        return DRIVER_ERR_NOT_OPENED;
    }

    // 查找设备并调用 ioctl 回调
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].is_registered &&
            g_devices[i].major == handle->major &&
            g_devices[i].ops.ioctl != NULL) {
            return g_devices[i].ops.ioctl(handle, cmd, arg);
        }
    }

    return DRIVER_ERR_INVALID_IOCTL;
}

// ==================== 缓冲区管理实现 ====================
int driver_flush(DriverHandle *handle) {
    if (handle == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    if (!handle->is_opened) {
        return DRIVER_ERR_NOT_OPENED;
    }

    // 查找设备并调用 flush 回调
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].is_registered &&
            g_devices[i].major == handle->major &&
            g_devices[i].ops.flush != NULL) {
            return g_devices[i].ops.flush(handle);
        }
    }

    return DRIVER_OK;
}

int driver_reset_buffer(DriverHandle *handle) {
    if (handle == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    handle->position = 0;

    return DRIVER_OK;
}

size_t driver_get_buffer_size(DriverHandle *handle) {
    if (handle == NULL) {
        return 0;
    }

    return handle->buffer_size;
}

int driver_set_buffer_size(DriverHandle *handle, size_t size) {
    if (handle == NULL || size == 0) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    if (size > DRIVER_MAX_DATA_SIZE) {
        return DRIVER_ERR_BUFFER_OVERFLOW;
    }

    uint8_t *new_buf = realloc(handle->buffer, size);
    if (new_buf == NULL) {
        return DRIVER_ERR_NO_MEMORY;
    }

    handle->buffer = new_buf;
    handle->buffer_size = size;

    if (handle->position > size) {
        handle->position = size;
    }

    return DRIVER_OK;
}

// ==================== 工具函数实现 ====================
const char* driver_strerror(int error_code) {
    switch (error_code) {
        case DRIVER_OK:
            return "Success";
        case DRIVER_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case DRIVER_ERR_NO_MEMORY:
            return "No memory";
        case DRIVER_ERR_DEVICE_NOT_FOUND:
            return "Device not found";
        case DRIVER_ERR_DEVICE_BUSY:
            return "Device busy";
        case DRIVER_ERR_ALREADY_REGISTERED:
            return "Already registered";
        case DRIVER_ERR_NOT_OPENED:
            return "Not opened";
        case DRIVER_ERR_PERMISSION_DENIED:
            return "Permission denied";
        case DRIVER_ERR_INVALID_IOCTL:
            return "Invalid ioctl command";
        case DRIVER_ERR_BUFFER_OVERFLOW:
            return "Buffer overflow";
        case DRIVER_ERR_TIMEOUT:
            return "Timeout";
        default:
            return "Unknown error";
    }
}

int driver_get_device_count(void) {
    int count = 0;
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].is_registered) {
            count++;
        }
    }
    return count;
}

uint32_t driver_get_open_count(DriverHandle *handle) {
    if (handle == NULL) {
        return 0;
    }
    return handle->open_count;
}

void driver_set_private_data(DriverHandle *handle, void *data) {
    if (handle != NULL) {
        handle->private_data = data;
    }
}

void* driver_get_private_data(DriverHandle *handle) {
    if (handle == NULL) {
        return NULL;
    }
    return handle->private_data;
}

// ==================== 模拟中断和回调实现 ====================
typedef struct {
    DriverCallback callback;
    void *data;
} CallbackInfo;

static CallbackInfo g_callbacks[DRIVER_MAX_DEVICES];

int driver_register_callback(const char *name, DriverCallback cb, void *data) {
    if (name == NULL || cb == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    int index = find_device_by_name(name);
    if (index < 0) {
        return DRIVER_ERR_DEVICE_NOT_FOUND;
    }

    g_callbacks[index].callback = cb;
    g_callbacks[index].data = data;

    return DRIVER_OK;
}

int driver_trigger_callback(const char *name) {
    if (name == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    int index = find_device_by_name(name);
    if (index < 0) {
        return DRIVER_ERR_DEVICE_NOT_FOUND;
    }

    if (g_callbacks[index].callback != NULL) {
        // 创建一个虚拟句柄
        DriverHandle handle;
        memset(&handle, 0, sizeof(handle));
        handle.major = g_devices[index].major;
        handle.minor = g_devices[index].minor_start;

        g_callbacks[index].callback(&handle, g_callbacks[index].data);
    }

    return DRIVER_OK;
}

// ==================== 并发控制模拟实现 ====================
typedef struct {
    bool is_locked;
    uint32_t lock_count;
} DriverLock;

static DriverLock g_locks[DRIVER_MAX_DEVICES];

int driver_lock(DriverHandle *handle) {
    if (handle == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].is_registered &&
            g_devices[i].major == handle->major) {
            g_locks[i].is_locked = true;
            g_locks[i].lock_count++;
            return DRIVER_OK;
        }
    }

    return DRIVER_ERR_DEVICE_NOT_FOUND;
}

int driver_unlock(DriverHandle *handle) {
    if (handle == NULL) {
        return DRIVER_ERR_INVALID_PARAM;
    }

    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].is_registered &&
            g_devices[i].major == handle->major) {
            if (g_locks[i].lock_count > 0) {
                g_locks[i].lock_count--;
                if (g_locks[i].lock_count == 0) {
                    g_locks[i].is_locked = false;
                }
            }
            return DRIVER_OK;
        }
    }

    return DRIVER_ERR_DEVICE_NOT_FOUND;
}

bool driver_try_lock(DriverHandle *handle) {
    if (handle == NULL) {
        return false;
    }

    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].is_registered &&
            g_devices[i].major == handle->major) {
            if (!g_locks[i].is_locked) {
                g_locks[i].is_locked = true;
                g_locks[i].lock_count++;
                return true;
            }
            return false;
        }
    }

    return false;
}
