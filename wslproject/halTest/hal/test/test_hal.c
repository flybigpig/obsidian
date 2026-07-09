#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "hal_uart.h"

void test_uart_init(void) {
    printf("[TEST] UART Init... ");

    HAL_UART_HandleTypeDef uart;
    uart.baudrate = 115200;
    uart.data_bits = 8;
    uart.parity = 'N';
    uart.stop_bits = 1;
    uart.is_initialized = false;

    int result = HAL_UART_Init(&uart);

    assert(result == 0);
    assert(uart.is_initialized == true);
    printf("PASSED ✅\n");
}

void test_uart_init_invalid_params(void) {
    printf("[TEST] UART Init Invalid Params... ");

    int result = HAL_UART_Init(NULL);
    assert(result == -1);

    HAL_UART_HandleTypeDef uart;
    memset(&uart, 0, sizeof(uart));
    result = HAL_UART_Init(&uart);
    assert(result == -2);

    printf("PASSED ✅\n");
}

void test_uart_transmit(void) {
    printf("[TEST] UART Transmit... ");

    HAL_UART_HandleTypeDef uart;
    uart.baudrate = 115200;
    uart.data_bits = 8;
    uart.parity = 'N';
    uart.stop_bits = 1;
    HAL_UART_Init(&uart);

    const uint8_t data[] = "Hello HAL!";
    int result = HAL_UART_Transmit(&uart, data, sizeof(data) - 1, 1000);

    assert(result > 0);
    printf("PASSED ✅\n");
}

void test_uart_transmit_not_initialized(void) {
    printf("[TEST] UART Transmit Not Initialized... ");

    HAL_UART_HandleTypeDef uart;
    memset(&uart, 0, sizeof(uart));

    const uint8_t data[] = "Test";
    int result = HAL_UART_Transmit(&uart, data, sizeof(data) - 1, 1000);

    assert(result == -2);
    printf("PASSED ✅\n");
}

void test_uart_receive(void) {
    printf("[TEST] UART Receive... ");

    HAL_UART_HandleTypeDef uart;
    uart.baudrate = 115200;
    uart.data_bits = 8;
    uart.parity = 'N';
    uart.stop_bits = 1;
    HAL_UART_Init(&uart);

    uint8_t buffer[10];
    int result = HAL_UART_Receive(&uart, buffer, sizeof(buffer), 1000);

    assert(result > 0);
    printf("PASSED ✅\n");
}

void test_uart_deinit(void) {
    printf("[TEST] UART DeInit... ");

    HAL_UART_HandleTypeDef uart;
    uart.baudrate = 115200;
    uart.data_bits = 8;
    uart.parity = 'N';
    uart.stop_bits = 1;
    HAL_UART_Init(&uart);

    HAL_UART_DeInit(&uart);

    assert(uart.is_initialized == false);
    printf("PASSED ✅\n");
}

// 主测试入口
int main(void) {
    printf("\n===== HAL Test Suite =====\n\n");

    test_uart_init();
    test_uart_init_invalid_params();
    test_uart_transmit();
    test_uart_transmit_not_initialized();
    test_uart_receive();
    test_uart_deinit();

    printf("\n===== All Tests Complete =====\n\n");
    return 0;
}
