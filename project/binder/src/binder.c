#include "binder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ==================== 全局状态 ====================
static BinderService g_services[BINDER_MAX_SERVICES];
static int g_service_count = 0;
static uint32_t g_next_handle = 1;
static bool g_binder_initialized = false;

// ==================== 内部辅助函数 ====================
static uint32_t generate_transaction_id(void) {
    static uint32_t tid = 0;
    return ++tid;
}

static int find_service_by_name(const char *name) {
    for (int i = 0; i < g_service_count; i++) {
        if (g_services[i].is_active &&
            strcmp(g_services[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_free_service_slot(void) {
    for (int i = 0; i < BINDER_MAX_SERVICES; i++) {
        if (!g_services[i].is_active) {
            return i;
        }
    }
    return -1;
}

// ==================== 设备操作实现 ====================
BinderHandle* binder_open(const char *device_name) {
    if (device_name == NULL) {
        return NULL;
    }

    BinderHandle *handle = (BinderHandle*)malloc(sizeof(BinderHandle));
    if (handle == NULL) {
        return NULL;
    }

    handle->fd = 0;  // 模拟文件描述符
    handle->transaction_id = 0;
    handle->is_initialized = true;
    strncpy(handle->device_name, device_name, sizeof(handle->device_name) - 1);
    handle->device_name[sizeof(handle->device_name) - 1] = '\0';

    g_binder_initialized = true;

    printf("[BINDER] Device opened: %s\n", device_name);
    return handle;
}

int binder_close(BinderHandle *handle) {
    if (handle == NULL) {
        return BINDER_ERR_INVALID_PARAM;
    }

    if (!handle->is_initialized) {
        return BINDER_ERR_NOT_INITIALIZED;
    }

    printf("[BINDER] Device closed: %s\n", handle->device_name);

    handle->is_initialized = false;
    free(handle);

    g_binder_initialized = false;
    return BINDER_OK;
}

bool binder_is_initialized(BinderHandle *handle) {
    if (handle == NULL) {
        return false;
    }
    return handle->is_initialized;
}

// ==================== 事务操作实现 ====================
int binder_transact(BinderHandle *handle,
                    uint32_t target,
                    uint32_t code,
                    const void *data,
                    size_t data_size) {
    if (handle == NULL) {
        return BINDER_ERR_INVALID_PARAM;
    }

    if (!handle->is_initialized) {
        return BINDER_ERR_NOT_INITIALIZED;
    }

    if (data == NULL && data_size > 0) {
        return BINDER_ERR_INVALID_PARAM;
    }

    if (data_size > BINDER_MAX_DATA_LEN) {
        return BINDER_ERR_BUFFER_OVERFLOW;
    }

    // 生成事务 ID
    handle->transaction_id = generate_transaction_id();

    printf("[BINDER] Transact: target=%u, code=%u, data_size=%zu, tid=%u\n",
           target, code, data_size, handle->transaction_id);

    // 模拟数据传输
    if (data != NULL && data_size > 0) {
        printf("[BINDER] Data: ");
        for (size_t i = 0; i < data_size && i < 32; i++) {
            printf("%02X ", ((uint8_t*)data)[i]);
        }
        if (data_size > 32) {
            printf("...");
        }
        printf("\n");
    }

    return BINDER_OK;
}

int binder_reply(BinderHandle *handle,
                const void *data,
                size_t data_size) {
    if (handle == NULL) {
        return BINDER_ERR_INVALID_PARAM;
    }

    if (!handle->is_initialized) {
        return BINDER_ERR_NOT_INITIALIZED;
    }

    printf("[BINDER] Reply: data_size=%zu\n", data_size);

    if (data != NULL && data_size > 0) {
        printf("[BINDER] Reply Data: ");
        for (size_t i = 0; i < data_size && i < 32; i++) {
            printf("%02X ", ((uint8_t*)data)[i]);
        }
        if (data_size > 32) {
            printf("...");
        }
        printf("\n");
    }

    return BINDER_OK;
}

int binder_acquire(BinderHandle *handle, uint32_t target) {
    if (handle == NULL) {
        return BINDER_ERR_INVALID_PARAM;
    }

    if (!handle->is_initialized) {
        return BINDER_ERR_NOT_INITIALIZED;
    }

    // 查找服务
    for (int i = 0; i < g_service_count; i++) {
        if (g_services[i].handle == target && g_services[i].is_active) {
            g_services[i].ref_count++;
            printf("[BINDER] Acquire: handle=%u, ref_count=%u\n",
                   target, g_services[i].ref_count);
            return BINDER_OK;
        }
    }

    return BINDER_ERR_SERVICE_NOT_FOUND;
}

int binder_release(BinderHandle *handle, uint32_t target) {
    if (handle == NULL) {
        return BINDER_ERR_INVALID_PARAM;
    }

    if (!handle->is_initialized) {
        return BINDER_ERR_NOT_INITIALIZED;
    }

    // 查找服务
    for (int i = 0; i < g_service_count; i++) {
        if (g_services[i].handle == target && g_services[i].is_active) {
            if (g_services[i].ref_count > 0) {
                g_services[i].ref_count--;
            }
            printf("[BINDER] Release: handle=%u, ref_count=%u\n",
                   target, g_services[i].ref_count);
            return BINDER_OK;
        }
    }

    return BINDER_ERR_SERVICE_NOT_FOUND;
}

// ==================== Service Manager 操作实现 ====================
int binder_register_service(BinderHandle *handle,
                           const char *name,
                           uint32_t service_handle) {
    if (handle == NULL || name == NULL) {
        return BINDER_ERR_INVALID_PARAM;
    }

    if (!handle->is_initialized) {
        return BINDER_ERR_NOT_INITIALIZED;
    }

    if (strlen(name) == 0 || strlen(name) >= BINDER_MAX_NAME_LEN) {
        return BINDER_ERR_INVALID_PARAM;
    }

    // 检查服务是否已存在
    if (find_service_by_name(name) >= 0) {
        return BINDER_ERR_SERVICE_EXISTS;
    }

    // 查找空闲槽位
    int slot = find_free_service_slot();
    if (slot < 0) {
        return BINDER_ERR_NO_MEMORY;
    }

    // 注册服务
    strncpy(g_services[slot].name, name, BINDER_MAX_NAME_LEN - 1);
    g_services[slot].name[BINDER_MAX_NAME_LEN - 1] = '\0';
    g_services[slot].handle = service_handle;
    g_services[slot].is_active = true;
    g_services[slot].ref_count = 0;

    if (slot >= g_service_count) {
        g_service_count = slot + 1;
    }

    printf("[BINDER] Service registered: name=%s, handle=%u\n",
           name, service_handle);

    return BINDER_OK;
}

int binder_unregister_service(BinderHandle *handle,
                              const char *name) {
    if (handle == NULL || name == NULL) {
        return BINDER_ERR_INVALID_PARAM;
    }

    if (!handle->is_initialized) {
        return BINDER_ERR_NOT_INITIALIZED;
    }

    int index = find_service_by_name(name);
    if (index < 0) {
        return BINDER_ERR_SERVICE_NOT_FOUND;
    }

    g_services[index].is_active = false;
    g_services[index].name[0] = '\0';
    g_services[index].handle = 0;
    g_services[index].ref_count = 0;

    printf("[BINDER] Service unregistered: name=%s\n", name);

    return BINDER_OK;
}

uint32_t binder_get_service(BinderHandle *handle,
                           const char *name) {
    if (handle == NULL || name == NULL) {
        return 0;
    }

    if (!handle->is_initialized) {
        return 0;
    }

    int index = find_service_by_name(name);
    if (index < 0) {
        return 0;
    }

    printf("[BINDER] Service found: name=%s, handle=%u\n",
           name, g_services[index].handle);

    return g_services[index].handle;
}

bool binder_service_exists(BinderHandle *handle,
                          const char *name) {
    if (handle == NULL || name == NULL) {
        return false;
    }

    return find_service_by_name(name) >= 0;
}

// ==================== 数据序列化/反序列化实现 ====================
BinderBuffer* binder_buffer_create(size_t capacity) {
    if (capacity == 0 || capacity > BINDER_MAX_DATA_LEN) {
        return NULL;
    }

    BinderBuffer *buffer = (BinderBuffer*)malloc(sizeof(BinderBuffer));
    if (buffer == NULL) {
        return NULL;
    }

    buffer->buffer = (uint8_t*)malloc(capacity);
    if (buffer->buffer == NULL) {
        free(buffer);
        return NULL;
    }

    buffer->capacity = capacity;
    buffer->position = 0;
    memset(buffer->buffer, 0, capacity);

    return buffer;
}

void binder_buffer_destroy(BinderBuffer *buffer) {
    if (buffer == NULL) {
        return;
    }

    if (buffer->buffer != NULL) {
        free(buffer->buffer);
    }
    free(buffer);
}

int binder_buffer_write_int32(BinderBuffer *buffer, int32_t value) {
    if (buffer == NULL || buffer->buffer == NULL) {
        return BINDER_ERR_INVALID_PARAM;
    }

    if (buffer->position + sizeof(int32_t) > buffer->capacity) {
        return BINDER_ERR_BUFFER_OVERFLOW;
    }

    memcpy(buffer->buffer + buffer->position, &value, sizeof(int32_t));
    buffer->position += sizeof(int32_t);

    return BINDER_OK;
}

int binder_buffer_write_string(BinderBuffer *buffer, const char *str) {
    if (buffer == NULL || buffer->buffer == NULL || str == NULL) {
        return BINDER_ERR_INVALID_PARAM;
    }

    size_t len = strlen(str) + 1;  // 包含 null 终止符

    if (buffer->position + len > buffer->capacity) {
        return BINDER_ERR_BUFFER_OVERFLOW;
    }

    memcpy(buffer->buffer + buffer->position, str, len);
    buffer->position += len;

    return BINDER_OK;
}

int binder_buffer_write_data(BinderBuffer *buffer,
                            const void *data,
                            size_t size) {
    if (buffer == NULL || buffer->buffer == NULL || data == NULL) {
        return BINDER_ERR_INVALID_PARAM;
    }

    if (buffer->position + size > buffer->capacity) {
        return BINDER_ERR_BUFFER_OVERFLOW;
    }

    memcpy(buffer->buffer + buffer->position, data, size);
    buffer->position += size;

    return BINDER_OK;
}

int32_t binder_buffer_read_int32(BinderBuffer *buffer) {
    if (buffer == NULL || buffer->buffer == NULL) {
        return 0;
    }

    if (buffer->position + sizeof(int32_t) > buffer->capacity) {
        return 0;
    }

    int32_t value;
    memcpy(&value, buffer->buffer + buffer->position, sizeof(int32_t));
    buffer->position += sizeof(int32_t);

    return value;
}

const char* binder_buffer_read_string(BinderBuffer *buffer) {
    if (buffer == NULL || buffer->buffer == NULL) {
        return NULL;
    }

    const char *str = (const char*)(buffer->buffer + buffer->position);

    // 查找 null 终止符
    size_t len = 0;
    while (buffer->position + len < buffer->capacity) {
        if (buffer->buffer[buffer->position + len] == '\0') {
            break;
        }
        len++;
    }

    buffer->position += len + 1;

    return str;
}

size_t binder_buffer_read_data(BinderBuffer *buffer,
                              void *data,
                              size_t max_size) {
    if (buffer == NULL || buffer->buffer == NULL || data == NULL) {
        return 0;
    }

    size_t available = buffer->capacity - buffer->position;
    size_t to_read = available < max_size ? available : max_size;

    memcpy(data, buffer->buffer + buffer->position, to_read);
    buffer->position += to_read;

    return to_read;
}

// ==================== 工具函数实现 ====================
const char* binder_strerror(int error_code) {
    switch (error_code) {
        case BINDER_OK:
            return "Success";
        case BINDER_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case BINDER_ERR_NOT_INITIALIZED:
            return "Not initialized";
        case BINDER_ERR_ALREADY_INITIALIZED:
            return "Already initialized";
        case BINDER_ERR_SERVICE_NOT_FOUND:
            return "Service not found";
        case BINDER_ERR_SERVICE_EXISTS:
            return "Service already exists";
        case BINDER_ERR_TRANSACTION_FAILED:
            return "Transaction failed";
        case BINDER_ERR_BUFFER_OVERFLOW:
            return "Buffer overflow";
        case BINDER_ERR_TIMEOUT:
            return "Timeout";
        case BINDER_ERR_NO_MEMORY:
            return "No memory available";
        default:
            return "Unknown error";
    }
}

uint32_t binder_generate_handle(void) {
    return g_next_handle++;
}

void binder_list_services(BinderHandle *handle) {
    if (handle == NULL) {
        printf("[BINDER] Cannot list services: invalid handle\n");
        return;
    }

    printf("[BINDER] === Service List ===\n");
    int count = 0;
    for (int i = 0; i < g_service_count; i++) {
        if (g_services[i].is_active) {
            printf("  [%d] name=%s, handle=%u, ref_count=%u\n",
                   count, g_services[i].name, g_services[i].handle,
                   g_services[i].ref_count);
            count++;
        }
    }
    if (count == 0) {
        printf("  (no services registered)\n");
    }
    printf("[BINDER] =====================\n");
}

// ==================== 多进程模拟实现 ====================
int binder_start_server(BinderHandle *handle,
                       BinderHandler handler) {
    if (handle == NULL || handler == NULL) {
        return BINDER_ERR_INVALID_PARAM;
    }

    if (!handle->is_initialized) {
        return BINDER_ERR_NOT_INITIALIZED;
    }

    printf("[BINDER] Server started (simulated)\n");
    // 在实际实现中，这里会创建线程或进程来处理事务
    return BINDER_OK;
}

int binder_stop_server(BinderHandle *handle) {
    if (handle == NULL) {
        return BINDER_ERR_INVALID_PARAM;
    }

    if (!handle->is_initialized) {
        return BINDER_ERR_NOT_INITIALIZED;
    }

    printf("[BINDER] Server stopped (simulated)\n");
    return BINDER_OK;
}
