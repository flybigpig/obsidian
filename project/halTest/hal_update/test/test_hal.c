#include "hal_uart.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    HAL_UART_HandleTypeDef huart = {
        .baudrate = 115200,
        .data_bits = 8,
        .parity = 'N',
        .stop_bits = 1,
        .is_initialized = false,
#ifdef HAL_UART_LINUX_MODE
        .fd = -1,
        .device = "/dev/ttyUSB0"
#endif
    };

    printf("=== HAL UART Test ===\n");
    printf("Mode: %s\n\n",
#ifdef HAL_UART_MOCK_MODE
           "MOCK"
#else
           "LINUX HARDWARE"
#endif
    );

    // 初始化
    printf("1. Initializing UART...\n");
    int ret = HAL_UART_Init(&huart);
    if (ret != HAL_UART_OK) {
        printf("   Failed! ret=%d\n", ret);
        return 1;
    }
    printf("   OK\n\n");

    // 发送测试
    printf("2. Transmit test...\n");
    const char *tx_data = "Hello, UART!\n";
    ret = HAL_UART_Transmit(&huart, (uint8_t *)tx_data, strlen(tx_data), 1000);
    if (ret < 0) {
        printf("   Failed! ret=%d\n", ret);
    } else {
        printf("   Sent %d bytes\n", ret);
    }
    printf("\n");

    // 接收测试
    printf("3. Receive test (waiting for data)...\n");
    uint8_t rx_buffer[64] = {0};
    printf("   Waiting up to 5 seconds...\n");
    ret = HAL_UART_Receive(&huart, rx_buffer, sizeof(rx_buffer) - 1, 5000);
    if (ret > 0) {
        printf("   Received %d bytes: %s\n", ret, rx_buffer);
    } else if (ret == HAL_UART_TIMEOUT) {
        printf("   Timeout (no data received)\n");
    } else {
        printf("   Error! ret=%d\n", ret);
    }
    printf("\n");

    // 非阻塞检查测试
    printf("4. Non-blocking check test...\n");
    ret = HAL_UART_RxAvailable(&huart);
    printf("   Data available: %s\n", ret > 0 ? "YES" : "NO");
    printf("\n");

    // 反初始化
    printf("5. Deinitializing UART...\n");
    HAL_UART_DeInit(&huart);
    printf("   OK\n");

    printf("\n=== Test Complete ===\n");
    return 0;
}
