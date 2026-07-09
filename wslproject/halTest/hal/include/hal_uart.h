#ifndef __HAL_UART_H
#define __HAL_UART_H

#include <stdint.h>
#include <stdbool.h>

// UART 句柄结构体
typedef struct {
    uint32_t baudrate;    // 波特率
    uint8_t  data_bits;   // 数据位
    char     parity;      // 校验位: N/O/E
    uint8_t  stop_bits;   // 停止位
    bool     is_initialized; // 初始化状态
} HAL_UART_HandleTypeDef;

// 初始化 UART
int HAL_UART_Init(HAL_UART_HandleTypeDef *huart);

// 发送数据
int HAL_UART_Transmit(HAL_UART_HandleTypeDef *huart,
                      const uint8_t *data,
                      uint16_t size,
                      uint32_t timeout);

// 接收数据
int HAL_UART_Receive(HAL_UART_HandleTypeDef *huart,
                     uint8_t *data,
                     uint16_t size,
                     uint32_t timeout);

// 反初始化
void HAL_UART_DeInit(HAL_UART_HandleTypeDef *huart);

#endif // __HAL_UART_H
