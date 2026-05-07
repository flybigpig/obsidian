#ifndef __HAL_UART_H
#define __HAL_UART_H

#include <stdint.h>
#include <stdbool.h>

// 平台选择宏 - 定义使用真实硬件还是模拟
// #define HAL_UART_MOCK_MODE    // 取消注释使用模拟模式
#define HAL_UART_LINUX_MODE     // Linux 真实串口模式

// UART 句柄结构体
typedef struct {
    uint32_t baudrate;       // 波特率
    uint8_t  data_bits;      // 数据位
    char     parity;         // 校验位: N/O/E
    uint8_t  stop_bits;      // 停止位
    bool     is_initialized; // 初始化状态

    // 平台相关字段
#ifdef HAL_UART_LINUX_MODE
    int      fd;             // Linux 串口文件描述符
    char     device[32];     // 设备路径，如 "/dev/ttyUSB0"
#endif
} HAL_UART_HandleTypeDef;

// 错误码定义
#define HAL_UART_OK         0
#define HAL_UART_ERROR     -1
#define HAL_UART_NOT_INIT  -2
#define HAL_UART_TIMEOUT   -3
#define HAL_UART_IO_ERROR  -4

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

// 检查是否有数据可读（非阻塞）
int HAL_UART_RxAvailable(HAL_UART_HandleTypeDef *huart);

#endif // __HAL_UART_H
