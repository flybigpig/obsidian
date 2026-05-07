#include "hal_uart.h"
#include <stdio.h>
#include <string.h>

// 初始化 UART
int HAL_UART_Init(HAL_UART_HandleTypeDef *huart) {
    if (huart == NULL) {
        return -1;
    }

    // 参数校验
    if (huart->baudrate == 0) {
        return -2;
    }

    // 模拟硬件初始化
    printf("[HAL] UART initialized: baudrate=%lu, data_bits=%u, parity=%c, stop_bits=%u\n",
           (unsigned long)huart->baudrate,
           huart->data_bits,
           huart->parity,
           huart->stop_bits);

    huart->is_initialized = true;
    return 0;
}

// 发送数据
int HAL_UART_Transmit(HAL_UART_HandleTypeDef *huart,
                      const uint8_t *data,
                      uint16_t size,
                      uint32_t timeout) {
    if (huart == NULL || data == NULL || size == 0) {
        return -1;
    }

    if (!huart->is_initialized) {
        return -2;
    }

    // 模拟数据发送
    printf("[HAL] UART transmit: size=%u, timeout=%lu\n",
           size, (unsigned long)timeout);
    printf("[HAL] Data: ");
    for (uint16_t i = 0; i < size; i++) {
        printf("%c", data[i]);
    }
    printf("\n");

    return size;  // 返回发送的字节数
}

// 接收数据
int HAL_UART_Receive(HAL_UART_HandleTypeDef *huart,
                     uint8_t *data,
                     uint16_t size,
                     uint32_t timeout) {
    if (huart == NULL || data == NULL || size == 0) {
        return -1;
    }

    if (!huart->is_initialized) {
        return -2;
    }

    // 模拟数据接收（这里返回0表示超时无数据）
    printf("[HAL] UART receive: size=%u, timeout=%lu\n",
           size, (unsigned long)timeout);

    // 模拟接收到数据
    const char *mock_response = "OK";
    uint16_t copy_len = size < 2 ? size : 2;
    memcpy(data, mock_response, copy_len);

    return copy_len;
}

// 反初始化
void HAL_UART_DeInit(HAL_UART_HandleTypeDef *huart) {
    if (huart == NULL) {
        return;
    }

    printf("[HAL] UART deinitialized\n");
    huart->is_initialized = false;
}
