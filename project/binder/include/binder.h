#ifndef __BINDER_H
#define __BINDER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ==================== 常量定义 ====================
#define BINDER_MAX_NAME_LEN     128
#define BINDER_MAX_DATA_LEN     4096
#define BINDER_MAX_SERVICES     32
#define BINDER_MAX_TRANSACTIONS 64

// ==================== 错误码定义 ====================
typedef enum {
    BINDER_OK = 0,
    BINDER_ERR_INVALID_PARAM = -1,
    BINDER_ERR_NOT_INITIALIZED = -2,
    BINDER_ERR_ALREADY_INITIALIZED = -3,
    BINDER_ERR_SERVICE_NOT_FOUND = -4,
    BINDER_ERR_SERVICE_EXISTS = -5,
    BINDER_ERR_TRANSACTION_FAILED = -6,
    BINDER_ERR_BUFFER_OVERFLOW = -7,
    BINDER_ERR_TIMEOUT = -8,
    BINDER_ERR_NO_MEMORY = -9
} BinderError;

// ==================== Binder 句柄 ====================
typedef struct {
    int fd;                      // 文件描述符（模拟）
    uint32_t transaction_id;      // 事务 ID 计数器
    bool is_initialized;          // 初始化状态
    char device_name[32];         // 设备名称
} BinderHandle;

// ==================== Binder 事务数据 ====================
typedef struct {
    uint32_t target;              // 目标句柄
    uint32_t code;                // 事务代码
    void *data;                   // 数据指针
    size_t data_size;             // 数据大小
    uint32_t flags;               // 标志位
    uint32_t pid;                 // 进程 ID
    uint32_t tid;                 // 线程 ID
} BinderTransactionData;

// ==================== Service 信息 ====================
typedef struct {
    char name[BINDER_MAX_NAME_LEN];  // 服务名称
    uint32_t handle;                 // 服务句柄
    bool is_active;                  // 是否激活
    uint32_t ref_count;             // 引用计数
} BinderService;

// ==================== 序列化缓冲区 ====================
typedef struct {
    uint8_t *buffer;             // 缓冲区指针
    size_t capacity;              // 缓冲区容量
    size_t position;              // 当前位置
} BinderBuffer;

// ==================== 核心 API ====================

// 设备操作
BinderHandle* binder_open(const char *device_name);
int binder_close(BinderHandle *handle);
bool binder_is_initialized(BinderHandle *handle);

// 事务操作
int binder_transact(BinderHandle *handle,
                    uint32_t target,
                    uint32_t code,
                    const void *data,
                    size_t data_size);
int binder_reply(BinderHandle *handle,
                const void *data,
                size_t data_size);
int binder_acquire(BinderHandle *handle, uint32_t target);
int binder_release(BinderHandle *handle, uint32_t target);

// Service Manager 操作
int binder_register_service(BinderHandle *handle,
                           const char *name,
                           uint32_t service_handle);
int binder_unregister_service(BinderHandle *handle,
                              const char *name);
uint32_t binder_get_service(BinderHandle *handle,
                           const char *name);
bool binder_service_exists(BinderHandle *handle,
                          const char *name);

// 数据序列化/反序列化
BinderBuffer* binder_buffer_create(size_t capacity);
void binder_buffer_destroy(BinderBuffer *buffer);
int binder_buffer_write_int32(BinderBuffer *buffer, int32_t value);
int binder_buffer_write_string(BinderBuffer *buffer, const char *str);
int binder_buffer_write_data(BinderBuffer *buffer,
                            const void *data,
                            size_t size);
int32_t binder_buffer_read_int32(BinderBuffer *buffer);
const char* binder_buffer_read_string(BinderBuffer *buffer);
size_t binder_buffer_read_data(BinderBuffer *buffer,
                              void *data,
                              size_t max_size);

// 工具函数
const char* binder_strerror(int error_code);
uint32_t binder_generate_handle(void);
void binder_list_services(BinderHandle *handle);

// 多进程模拟
typedef void (*BinderHandler)(BinderHandle *handle,
                             BinderTransactionData *data);
int binder_start_server(BinderHandle *handle,
                       BinderHandler handler);
int binder_stop_server(BinderHandle *handle);

#endif // __BINDER_H
