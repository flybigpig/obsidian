#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "driver.h"

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

// 测试 1: 设备注册/注销
void test_driver_register_unregister(void) {
    TEST_START("Driver Register/Unregister");

    int result = driver_register_device("test_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    assert(driver_device_exists("test_dev") == true);

    result = driver_unregister_device("test_dev");
    assert(result == DRIVER_OK);

    assert(driver_device_exists("test_dev") == false);

    TEST_PASS();
}

// 测试 2: 重复注册
void test_driver_duplicate_register(void) {
    TEST_START("Driver Duplicate Register");

    int result = driver_register_device("dup_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    // 重复注册应失败
    result = driver_register_device("dup_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_ERR_ALREADY_REGISTERED);

    driver_unregister_device("dup_dev");

    TEST_PASS();
}

// 测试 3: 注销不存在的设备
void test_driver_unregister_nonexistent(void) {
    TEST_START("Driver Unregister Nonexistent");

    int result = driver_unregister_device("no_such_device");
    assert(result == DRIVER_ERR_DEVICE_NOT_FOUND);

    TEST_PASS();
}

// 测试 4: 设备打开/关闭
void test_driver_open_close(void) {
    TEST_START("Driver Open/Close");

    int result = driver_register_device("open_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    DriverHandle *handle = driver_open("open_dev", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle != NULL);
    assert(handle->is_opened == true);
    assert(handle->major > 0);
    assert(handle->minor == 0);

    result = driver_close(handle);
    assert(result == DRIVER_OK);

    driver_unregister_device("open_dev");

    TEST_PASS();
}

// 测试 5: 打开不存在的设备
void test_driver_open_nonexistent(void) {
    TEST_START("Driver Open Nonexistent");

    DriverHandle *handle = driver_open("no_such_device", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle == NULL);

    TEST_PASS();
}

// 测试 6: 关闭 NULL 句柄
void test_driver_close_invalid(void) {
    TEST_START("Driver Close Invalid");

    int result = driver_close(NULL);
    assert(result == DRIVER_ERR_INVALID_PARAM);

    TEST_PASS();
}

// 测试 7: 数据读写
void test_driver_read_write(void) {
    TEST_START("Driver Read/Write");

    int result = driver_register_device("rw_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    DriverHandle *handle = driver_open("rw_dev", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle != NULL);

    // 写入数据
    const char *write_data = "Hello Driver!";
    ssize_t written = driver_write(handle, write_data, strlen(write_data));
    assert(written == (ssize_t)strlen(write_data));

    // 重置位置并读取
    driver_reset_buffer(handle);
    char read_buf[64] = {0};
    ssize_t read = driver_read(handle, read_buf, strlen(write_data));
    assert(read == (ssize_t)strlen(write_data));
    assert(strcmp(read_buf, write_data) == 0);

    driver_close(handle);
    driver_unregister_device("rw_dev");

    TEST_PASS();
}

// 测试 8: 读取权限检查
void test_driver_read_permission(void) {
    TEST_START("Driver Read Permission");

    int result = driver_register_device("perm_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    // 只读打开
    DriverHandle *handle = driver_open("perm_dev", 0, DRIVER_FLAG_WRITE);
    assert(handle != NULL);

    // 尝试读取应失败
    char buf[16];
    ssize_t read = driver_read(handle, buf, sizeof(buf));
    assert(read == DRIVER_ERR_PERMISSION_DENIED);

    driver_close(handle);
    driver_unregister_device("perm_dev");

    TEST_PASS();
}

// 测试 9: 写入权限检查
void test_driver_write_permission(void) {
    TEST_START("Driver Write Permission");

    int result = driver_register_device("perm_dev2", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    // 只写打开
    DriverHandle *handle = driver_open("perm_dev2", 0, DRIVER_FLAG_READ);
    assert(handle != NULL);

    // 尝试写入应失败
    const char *data = "test";
    ssize_t written = driver_write(handle, data, strlen(data));
    assert(written == DRIVER_ERR_PERMISSION_DENIED);

    driver_close(handle);
    driver_unregister_device("perm_dev2");

    TEST_PASS();
}

// 测试 10: IOCTL 操作
void test_driver_ioctl(void) {
    TEST_START("Driver IOCTL");

    int result = driver_register_device("ioctl_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    DriverHandle *handle = driver_open("ioctl_dev", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle != NULL);

    // 测试 RESET IOCTL
    result = driver_ioctl(handle, DRIVER_IOCTL_RESET, 0);
    assert(result == DRIVER_OK);

    // 测试无效 IOCTL
    result = driver_ioctl(handle, 0xFFFF, 0);
    assert(result == DRIVER_ERR_INVALID_IOCTL);

    driver_close(handle);
    driver_unregister_device("ioctl_dev");

    TEST_PASS();
}

// 测试 11: 缓冲区管理
void test_driver_buffer_management(void) {
    TEST_START("Driver Buffer Management");

    int result = driver_register_device("buf_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    DriverHandle *handle = driver_open("buf_dev", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle != NULL);

    // 获取默认缓冲区大小
    size_t size = driver_get_buffer_size(handle);
    assert(size > 0);

    // 设置新缓冲区大小
    result = driver_set_buffer_size(handle, 2048);
    assert(result == DRIVER_OK);
    assert(driver_get_buffer_size(handle) == 2048);

    // 重置缓冲区
    result = driver_reset_buffer(handle);
    assert(result == DRIVER_OK);
    assert(handle->position == 0);

    // 刷新
    result = driver_flush(handle);
    assert(result == DRIVER_OK);

    driver_close(handle);
    driver_unregister_device("buf_dev");

    TEST_PASS();
}

// 测试 12: 定位读写
void test_driver_read_write_at(void) {
    TEST_START("Driver Read/Write At Position");

    int result = driver_register_device("pos_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    DriverHandle *handle = driver_open("pos_dev", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle != NULL);

    // 写入数据
    const char *data = "ABCDEFGHIJ";
    driver_write(handle, data, strlen(data));

    // 在偏移 5 处读取
    char buf[6] = {0};
    ssize_t read = driver_read_at(handle, buf, 5, 5);
    assert(read == 5);
    assert(strcmp(buf, "FGHIJ") == 0);

    // 在偏移 0 处写入
    const char *new_data = "12345";
    ssize_t written = driver_write_at(handle, new_data, strlen(new_data), 0);
    assert(written == 5);

    // 验证
    driver_reset_buffer(handle);
    char verify[11] = {0};
    driver_read(handle, verify, 10);
    assert(strcmp(verify, "12345FGHIJ") == 0);

    driver_close(handle);
    driver_unregister_device("pos_dev");

    TEST_PASS();
}

// 测试 13: 多设备管理
void test_driver_multiple_devices(void) {
    TEST_START("Driver Multiple Devices");

    int result = driver_register_device("multi_dev0", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    result = driver_register_device("multi_dev1", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    result = driver_register_device("multi_dev2", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    assert(driver_get_device_count() == 3);

    // 打开所有设备
    DriverHandle *h0 = driver_open("multi_dev0", 0, DRIVER_FLAG_READ_WRITE);
    DriverHandle *h1 = driver_open("multi_dev1", 0, DRIVER_FLAG_READ_WRITE);
    DriverHandle *h2 = driver_open("multi_dev2", 0, DRIVER_FLAG_READ_WRITE);

    assert(h0 != NULL);
    assert(h1 != NULL);
    assert(h2 != NULL);

    // 关闭并注销
    driver_close(h0);
    driver_close(h1);
    driver_close(h2);

    driver_unregister_device("multi_dev0");
    driver_unregister_device("multi_dev1");
    driver_unregister_device("multi_dev2");

    assert(driver_get_device_count() == 0);

    TEST_PASS();
}

// 测试 14: 多次打开计数
void test_driver_open_count(void) {
    TEST_START("Driver Open Count");

    int result = driver_register_device("count_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    DriverHandle *h1 = driver_open("count_dev", 0, DRIVER_FLAG_READ_WRITE);
    DriverHandle *h2 = driver_open("count_dev", 0, DRIVER_FLAG_READ_WRITE);

    assert(h1 != NULL);
    assert(h2 != NULL);

    DriverDevice *dev = driver_find_device("count_dev");
    assert(dev != NULL);
    assert(dev->open_count == 2);

    driver_close(h1);
    driver_close(h2);

    assert(dev->open_count == 0);

    driver_unregister_device("count_dev");

    TEST_PASS();
}

// 测试 15: 私有数据
void test_driver_private_data(void) {
    TEST_START("Driver Private Data");

    int result = driver_register_device("priv_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    DriverHandle *handle = driver_open("priv_dev", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle != NULL);

    // 设置私有数据
    int *private = (int*)malloc(sizeof(int));
    *private = 12345;
    driver_set_private_data(handle, private);

    // 获取并验证
    int *retrieved = (int*)driver_get_private_data(handle);
    assert(retrieved != NULL);
    assert(*retrieved == 12345);

    driver_close(handle);  // 会自动释放私有数据
    driver_unregister_device("priv_dev");

    TEST_PASS();
}

// 测试 16: 设备列表
void test_driver_list_devices(void) {
    TEST_START("Driver List Devices");

    int result = driver_register_device("list_dev1", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    result = driver_register_device("list_dev2", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    // 列出设备（主要验证不崩溃）
    driver_list_devices();

    driver_unregister_device("list_dev1");
    driver_unregister_device("list_dev2");

    TEST_PASS();
}

// 测试 17: 边界条件 - 空名称
void test_driver_edge_cases(void) {
    TEST_START("Driver Edge Cases");

    // 注册空名称（应失败）
    int result = driver_register_device("", 0, 0, 1, NULL);
    assert(result == DRIVER_ERR_INVALID_PARAM);

    // 查询空名称
    bool exists = driver_device_exists("");
    assert(exists == false);

    // 打开时次设备号无效
    result = driver_register_device("edge_dev", 0, 0, 2, NULL);
    assert(result == DRIVER_OK);

    DriverHandle *handle = driver_open("edge_dev", 5, DRIVER_FLAG_READ_WRITE);
    assert(handle == NULL);  // 次设备号 5 超出范围

    driver_unregister_device("edge_dev");

    TEST_PASS();
}

// 测试 18: 错误字符串转换
void test_driver_strerror(void) {
    TEST_START("Driver Strerror");

    assert(strcmp(driver_strerror(DRIVER_OK), "Success") == 0);
    assert(strcmp(driver_strerror(DRIVER_ERR_INVALID_PARAM), "Invalid parameter") == 0);
    assert(strcmp(driver_strerror(DRIVER_ERR_DEVICE_NOT_FOUND), "Device not found") == 0);
    assert(strcmp(driver_strerror(DRIVER_ERR_PERMISSION_DENIED), "Permission denied") == 0);
    assert(strcmp(driver_strerror(999), "Unknown error") == 0);  // 未知错误

    TEST_PASS();
}

// 测试 19: 自定义操作函数
void test_driver_custom_operations(void) {
    TEST_START("Driver Custom Operations");

    // 定义自定义操作（所有函数指针为 NULL）
    DriverOperations ops = {0};

    int result = driver_register_device("custom_dev", 0, 0, 1, &ops);
    assert(result == DRIVER_OK);

    DriverHandle *handle = driver_open("custom_dev", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle != NULL);

    // 测试自定义读取（返回错误，因为 read 是 NULL）
    char buf[16];
    ssize_t read = driver_read(handle, buf, sizeof(buf));
    assert(read == DRIVER_ERR_DEVICE_NOT_FOUND);  // 没有设置 read 操作

    driver_close(handle);
    driver_unregister_device("custom_dev");

    TEST_PASS();
}

// 测试 20: 模拟中断回调
void test_driver_callback(void) {
    TEST_START("Driver Callback");

    int result = driver_register_device("cb_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    // 注册回调
    result = driver_register_callback("cb_dev", NULL, NULL);
    assert(result == DRIVER_ERR_INVALID_PARAM);

    // 触发回调（没有注册回调，不应崩溃）
    result = driver_trigger_callback("cb_dev");
    assert(result == DRIVER_OK);

    // 触发不存在设备的回调
    result = driver_trigger_callback("no_such_device");
    assert(result == DRIVER_ERR_DEVICE_NOT_FOUND);

    driver_unregister_device("cb_dev");

    TEST_PASS();
}

// 测试 21: 并发锁模拟
void test_driver_lock(void) {
    TEST_START("Driver Lock");

    int result = driver_register_device("lock_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    DriverHandle *handle = driver_open("lock_dev", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle != NULL);

    // 加锁
    result = driver_lock(handle);
    assert(result == DRIVER_OK);

    // 尝试非阻塞加锁（应失败）
    bool locked = driver_try_lock(handle);
    assert(locked == false);

    // 解锁
    result = driver_unlock(handle);
    assert(result == DRIVER_OK);

    // 现在可以加锁
    locked = driver_try_lock(handle);
    assert(locked == true);

    driver_unlock(handle);
    driver_close(handle);
    driver_unregister_device("lock_dev");

    TEST_PASS();
}

// 测试 22: 缓冲区溢出
void test_driver_buffer_overflow(void) {
    TEST_START("Driver Buffer Overflow");

    int result = driver_register_device("overflow_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    DriverHandle *handle = driver_open("overflow_dev", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle != NULL);

    // 设置很小的缓冲区
    result = driver_set_buffer_size(handle, 10);
    assert(result == DRIVER_OK);

    // 写入超过缓冲区大小的数据（应自动扩展）
    const char *data = "This is a long string that exceeds 10 bytes";
    ssize_t written = driver_write(handle, data, strlen(data));
    assert(written == (ssize_t)strlen(data));
    assert(driver_get_buffer_size(handle) > 10);  // 缓冲区已扩展

    driver_close(handle);
    driver_unregister_device("overflow_dev");

    TEST_PASS();
}

// 测试 23: 未初始化错误
void test_driver_not_initialized(void) {
    TEST_START("Driver Not Initialized");

    // 不注册设备，直接尝试打开
    DriverHandle *handle = driver_open("never_registered", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle == NULL);

    TEST_PASS();
}

// 测试 24: 压力测试 - 大量数据
void test_driver_stress_large_data(void) {
    TEST_START("Driver Stress Large Data");

    int result = driver_register_device("stress_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    DriverHandle *handle = driver_open("stress_dev", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle != NULL);

    // 写入大量数据
    size_t data_size = 4096;
    char *data = (char*)malloc(data_size);
    assert(data != NULL);
    memset(data, 'A', data_size);

    ssize_t written = driver_write(handle, data, data_size);
    assert(written == (ssize_t)data_size);

    // 读取并验证
    driver_reset_buffer(handle);
    char *read_buf = (char*)malloc(data_size);
    assert(read_buf != NULL);
    ssize_t read = driver_read(handle, read_buf, data_size);
    assert(read == (ssize_t)data_size);
    assert(memcmp(read_buf, data, data_size) == 0);

    free(data);
    free(read_buf);
    driver_close(handle);
    driver_unregister_device("stress_dev");

    TEST_PASS();
}

// 测试 25: 设备忙（打开计数 > 0 时注销）
void test_driver_device_busy(void) {
    TEST_START("Driver Device Busy");

    int result = driver_register_device("busy_dev", 0, 0, 1, NULL);
    assert(result == DRIVER_OK);

    DriverHandle *handle = driver_open("busy_dev", 0, DRIVER_FLAG_READ_WRITE);
    assert(handle != NULL);

    // 尝试注销（应失败，设备还在使用）
    result = driver_unregister_device("busy_dev");
    assert(result == DRIVER_ERR_DEVICE_BUSY);

    driver_close(handle);

    // 现在可以注销
    result = driver_unregister_device("busy_dev");
    assert(result == DRIVER_OK);

    TEST_PASS();
}

// ==================== 测试主函数 ====================
int main(void) {
    printf("\n");
    printf("================================================\n");
    printf("        Driver Test Suite - 25 Test Cases        \n");
    printf("================================================\n\n");

    // 基础功能测试
    test_driver_register_unregister();
    test_driver_duplicate_register();
    test_driver_unregister_nonexistent();

    // 设备打开/关闭测试
    test_driver_open_close();
    test_driver_open_nonexistent();
    test_driver_close_invalid();

    // 数据读写测试
    test_driver_read_write();
    test_driver_read_permission();
    test_driver_write_permission();

    // IOCTL 测试
    test_driver_ioctl();

    // 缓冲区管理测试
    test_driver_buffer_management();

    // 定位读写测试
    test_driver_read_write_at();

    // 多设备测试
    test_driver_multiple_devices();
    test_driver_open_count();

    // 高级功能测试
    test_driver_private_data();
    test_driver_list_devices();
    test_driver_custom_operations();
    test_driver_callback();
    test_driver_lock();

    // 边界条件测试
    test_driver_edge_cases();

    // 错误处理测试
    test_driver_strerror();
    test_driver_not_initialized();

    // 压力和边界测试
    test_driver_buffer_overflow();
    test_driver_stress_large_data();
    test_driver_device_busy();

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
