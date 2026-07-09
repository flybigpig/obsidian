#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "binder.h"

// ==================== 测试统计 ====================
static int test_count = 0;
static int passed_count = 0;

#define TEST_START(name) do { \
    test_count++; \
    printf("[TEST %d] %s... ", test_count, name); \
} while(0)

#define TEST_PASS() do { \
    passed_count++; \
    printf("PASSED ✅\n"); \
} while(0)

#define TEST_FAIL(reason) do { \
    printf("FAILED ❌ (%s)\n", reason); \
} while(0)

// ==================== 测试用例 ====================

// 测试 1: Binder Open/Close
void test_binder_open_close(void) {
    TEST_START("Binder Open/Close");

    BinderHandle *handle = binder_open("/dev/binder");
    assert(handle != NULL);
    assert(handle->is_initialized == true);
    assert(strlen(handle->device_name) > 0);

    int result = binder_close(handle);
    assert(result == BINDER_OK);

    TEST_PASS();
}

// 测试 2: Binder Open 无效参数
void test_binder_open_invalid(void) {
    TEST_START("Binder Open Invalid Params");

    BinderHandle *handle = binder_open(NULL);
    assert(handle == NULL);

    TEST_PASS();
}

// 测试 3: Binder Close 无效参数
void test_binder_close_invalid(void) {
    TEST_START("Binder Close Invalid Params");

    int result = binder_close(NULL);
    assert(result == BINDER_ERR_INVALID_PARAM);

    TEST_PASS();
}

// 测试 4: Binder Transaction
void test_binder_transact(void) {
    TEST_START("Binder Transaction");

    BinderHandle *handle = binder_open("/dev/binder");
    assert(handle != NULL);

    const char *test_data = "Hello Binder!";
    int result = binder_transact(handle, 1, 100, test_data, strlen(test_data));
    assert(result == BINDER_OK);
    assert(handle->transaction_id > 0);

    binder_close(handle);
    TEST_PASS();
}

// 测试 5: Binder Transaction 无效参数
void test_binder_transact_invalid(void) {
    TEST_START("Binder Transaction Invalid Params");

    int result = binder_transact(NULL, 1, 100, "test", 4);
    assert(result == BINDER_ERR_INVALID_PARAM);

    BinderHandle *handle = binder_open("/dev/binder");
    assert(handle != NULL);

    // 数据指针为空但大小不为空
    result = binder_transact(handle, 1, 100, NULL, 10);
    assert(result == BINDER_ERR_INVALID_PARAM);

    // 数据过大
    result = binder_transact(handle, 1, 100, "test", BINDER_MAX_DATA_LEN + 1);
    assert(result == BINDER_ERR_BUFFER_OVERFLOW);

    binder_close(handle);
    TEST_PASS();
}

// 测试 6: Binder Reply
void test_binder_reply(void) {
    TEST_START("Binder Reply");

    BinderHandle *handle = binder_open("/dev/binder");
    assert(handle != NULL);

    const char *reply_data = "OK";
    int result = binder_reply(handle, reply_data, strlen(reply_data));
    assert(result == BINDER_OK);

    binder_close(handle);
    TEST_PASS();
}

// 测试 7: Service Manager 注册服务
void test_binder_register_service(void) {
    TEST_START("Service Manager Register");

    BinderHandle *handle = binder_open("/dev/binder");
    assert(handle != NULL);

    uint32_t service_handle = binder_generate_handle();
    int result = binder_register_service(handle, "test_service", service_handle);
    assert(result == BINDER_OK);

    result = binder_register_service(handle, "another_service", service_handle + 1);
    assert(result == BINDER_OK);

    // 重复注册应失败
    result = binder_register_service(handle, "test_service", service_handle);
    assert(result == BINDER_ERR_SERVICE_EXISTS);

    binder_close(handle);
    TEST_PASS();
}

// 测试 8: Service Manager 查询服务
void test_binder_get_service(void) {
    TEST_START("Service Manager Get Service");

    BinderHandle *handle = binder_open("/dev/binder");
    assert(handle != NULL);

    uint32_t service_handle = binder_generate_handle();
    int result = binder_register_service(handle, "query_service", service_handle);
    assert(result == BINDER_OK);

    uint32_t found_handle = binder_get_service(handle, "query_service");
    assert(found_handle == service_handle);

    // 查询不存在的服务
    found_handle = binder_get_service(handle, "non_existent");
    assert(found_handle == 0);

    binder_close(handle);
    TEST_PASS();
}

// 测试 9: Service Manager 注销服务
void test_binder_unregister_service(void) {
    TEST_START("Service Manager Unregister");

    BinderHandle *handle = binder_open("/dev/binder");
    assert(handle != NULL);

    uint32_t service_handle = binder_generate_handle();
    int result = binder_register_service(handle, "temp_service", service_handle);
    assert(result == BINDER_OK);

    result = binder_unregister_service(handle, "temp_service");
    assert(result == BINDER_OK);

    // 注销后查询应失败
    uint32_t found_handle = binder_get_service(handle, "temp_service");
    assert(found_handle == 0);

    // 重复注销应失败
    result = binder_unregister_service(handle, "temp_service");
    assert(result == BINDER_ERR_SERVICE_NOT_FOUND);

    binder_close(handle);
    TEST_PASS();
}

// 测试 10: Service Manager 检查服务存在
void test_binder_service_exists(void) {
    TEST_START("Service Manager Service Exists");

    BinderHandle *handle = binder_open("/dev/binder");
    assert(handle != NULL);

    uint32_t service_handle = binder_generate_handle();
    int result = binder_register_service(handle, "exists_service", service_handle);
    assert(result == BINDER_OK);

    assert(binder_service_exists(handle, "exists_service") == true);
    assert(binder_service_exists(handle, "non_existent") == false);

    binder_close(handle);
    TEST_PASS();
}

// 测试 11: 数据序列化 - Int32
void test_binder_serialize_int32(void) {
    TEST_START("Data Serialization Int32");

    BinderBuffer *buffer = binder_buffer_create(1024);
    assert(buffer != NULL);

    int32_t test_value = 0x12345678;
    int result = binder_buffer_write_int32(buffer, test_value);
    assert(result == BINDER_OK);
    assert(buffer->position == sizeof(int32_t));

    // 重置位置并读取
    buffer->position = 0;
    int32_t read_value = binder_buffer_read_int32(buffer);
    assert(read_value == test_value);

    binder_buffer_destroy(buffer);
    TEST_PASS();
}

// 测试 12: 数据序列化 - String
void test_binder_serialize_string(void) {
    TEST_START("Data Serialization String");

    BinderBuffer *buffer = binder_buffer_create(1024);
    assert(buffer != NULL);

    const char *test_str = "Hello Binder Serialization!";
    int result = binder_buffer_write_string(buffer, test_str);
    assert(result == BINDER_OK);

    // 重置位置并读取
    buffer->position = 0;
    const char *read_str = binder_buffer_read_string(buffer);
    assert(strcmp(read_str, test_str) == 0);

    binder_buffer_destroy(buffer);
    TEST_PASS();
}

// 测试 13: 数据序列化 - 混合数据
void test_binder_serialize_mixed(void) {
    TEST_START("Data Serialization Mixed");

    BinderBuffer *buffer = binder_buffer_create(1024);
    assert(buffer != NULL);

    // 写入混合数据
    int32_t int_val = 12345;
    const char *str_val = "test";
    uint8_t raw_data[] = {0xDE, 0xAD, 0xBE, 0xEF};

    assert(binder_buffer_write_int32(buffer, int_val) == BINDER_OK);
    assert(binder_buffer_write_string(buffer, str_val) == BINDER_OK);
    assert(binder_buffer_write_data(buffer, raw_data, sizeof(raw_data)) == BINDER_OK);

    // 读取并验证
    buffer->position = 0;
    assert(binder_buffer_read_int32(buffer) == int_val);
    assert(strcmp(binder_buffer_read_string(buffer), str_val) == 0);

    uint8_t read_raw[sizeof(raw_data)];
    assert(binder_buffer_read_data(buffer, read_raw, sizeof(read_raw)) == sizeof(raw_data));
    assert(memcmp(read_raw, raw_data, sizeof(raw_data)) == 0);

    binder_buffer_destroy(buffer);
    TEST_PASS();
}

// 测试 14: 数据序列化 - 缓冲区溢出
void test_binder_serialize_overflow(void) {
    TEST_START("Data Serialization Buffer Overflow");

    BinderBuffer *buffer = binder_buffer_create(10);  // 小缓冲区
    assert(buffer != NULL);

    // 尝试写入超过缓冲区大小的数据
    int32_t large_data[4];  // 16 字节
    int result = binder_buffer_write_data(buffer, large_data, sizeof(large_data));
    assert(result == BINDER_ERR_BUFFER_OVERFLOW);

    binder_buffer_destroy(buffer);
    TEST_PASS();
}

// 测试 15: Acquire/Release
void test_binder_acquire_release(void) {
    TEST_START("Binder Acquire/Release");

    BinderHandle *handle = binder_open("/dev/binder");
    assert(handle != NULL);

    uint32_t service_handle = binder_generate_handle();
    int result = binder_register_service(handle, "ref_service", service_handle);
    assert(result == BINDER_OK);

    // Acquire
    result = binder_acquire(handle, service_handle);
    assert(result == BINDER_OK);

    result = binder_acquire(handle, service_handle);
    assert(result == BINDER_OK);

    // Release
    result = binder_release(handle, service_handle);
    assert(result == BINDER_OK);

    // Release 不存在的服务
    result = binder_release(handle, 999);
    assert(result == BINDER_ERR_SERVICE_NOT_FOUND);

    binder_close(handle);
    TEST_PASS();
}

// 测试 16: 错误处理 - 未初始化
void test_binder_not_initialized(void) {
    TEST_START("Error Handling Not Initialized");

    BinderHandle handle;
    handle.is_initialized = false;

    int result = binder_transact(&handle, 1, 100, "test", 4);
    assert(result == BINDER_ERR_NOT_INITIALIZED);

    result = binder_reply(&handle, "test", 4);
    assert(result == BINDER_ERR_NOT_INITIALIZED);

    TEST_PASS();
}

// 测试 17: 列出服务
void test_binder_list_services(void) {
    TEST_START("List Services");

    BinderHandle *handle = binder_open("/dev/binder");
    assert(handle != NULL);

    // 注册几个服务
    binder_register_service(handle, "service1", binder_generate_handle());
    binder_register_service(handle, "service2", binder_generate_handle());

    // 列出服务（主要验证不崩溃）
    binder_list_services(handle);

    binder_close(handle);
    TEST_PASS();
}

// 测试 18: 边界条件 - 空字符串
void test_binder_edge_cases(void) {
    TEST_START("Edge Cases");

    BinderHandle *handle = binder_open("/dev/binder");
    assert(handle != NULL);

    // 注册空名称（应失败）
    int result = binder_register_service(handle, "", binder_generate_handle());
    assert(result == BINDER_ERR_INVALID_PARAM);

    // 查询空名称
    uint32_t h = binder_get_service(handle, "");
    assert(h == 0);

    // 事务数据为空但大小为 0（应成功）
    result = binder_transact(handle, 1, 100, NULL, 0);
    assert(result == BINDER_OK);

    binder_close(handle);
    TEST_PASS();
}

// 测试 19: 多服务注册压力测试
void test_binder_stress_services(void) {
    TEST_START("Stress Test Multiple Services");

    BinderHandle *handle = binder_open("/dev/binder");
    assert(handle != NULL);

    // 注册多个服务
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "stress_service_%d", i);
        int result = binder_register_service(handle, name, binder_generate_handle());
        assert(result == BINDER_OK);
    }

    // 验证所有服务都可以查询到
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "stress_service_%d", i);
        assert(binder_service_exists(handle, name) == true);
    }

    binder_close(handle);
    TEST_PASS();
}

// 测试 20: 错误字符串转换
void test_binder_strerror(void) {
    TEST_START("Binder Strerror");

    assert(strcmp(binder_strerror(BINDER_OK), "Success") == 0);
    assert(strcmp(binder_strerror(BINDER_ERR_INVALID_PARAM), "Invalid parameter") == 0);
    assert(strcmp(binder_strerror(BINDER_ERR_NOT_INITIALIZED), "Not initialized") == 0);
    assert(strcmp(binder_strerror(BINDER_ERR_SERVICE_NOT_FOUND), "Service not found") == 0);
    assert(strcmp(binder_strerror(999), "Unknown error") == 0);  // 未知错误

    TEST_PASS();
}

// ==================== 测试主函数 ====================
int main(void) {
    printf("\n");
    printf("================================================\n");
    printf("       Binder Test Suite - 20 Test Cases        \n");
    printf("================================================\n\n");

    // 基础功能测试
    test_binder_open_close();
    test_binder_open_invalid();
    test_binder_close_invalid();

    // 事务测试
    test_binder_transact();
    test_binder_transact_invalid();
    test_binder_reply();

    // Service Manager 测试
    test_binder_register_service();
    test_binder_get_service();
    test_binder_unregister_service();
    test_binder_service_exists();

    // 数据序列化测试
    test_binder_serialize_int32();
    test_binder_serialize_string();
    test_binder_serialize_mixed();
    test_binder_serialize_overflow();

    // 引用计数测试
    test_binder_acquire_release();

    // 错误处理测试
    test_binder_not_initialized();

    // 辅助功能测试
    test_binder_list_services();
    test_binder_edge_cases();
    test_binder_stress_services();
    test_binder_strerror();

    // 测试结果
    printf("\n");
    printf("================================================\n");
    printf("           Test Results Summary                 \n");
    printf("================================================\n");
    printf("  Total Tests:  %d\n", test_count);
    printf("  Passed:       %d ✅\n", passed_count);
    printf("  Failed:       %d\n", test_count - passed_count);
    printf("  Success Rate: %.1f%%\n",
           test_count > 0 ? (float)passed_count / test_count * 100.0 : 0);
    printf("================================================\n\n");

    if (passed_count == test_count) {
        printf("🎉 All tests passed!\n\n");
        return 0;
    } else {
        printf("⚠️  Some tests failed!\n\n");
        return 1;
    }
}
