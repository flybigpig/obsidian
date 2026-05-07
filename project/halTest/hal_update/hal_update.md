# HAL UART Update 项目文档

## 项目概述

`hal_update` 是 HAL (Hardware Abstraction Layer) UART 模块的升级版本，支持**模拟模式**和**Linux 真实硬件模式**双模式运行。

## 项目结构

```
hal_update/
├── include/
│   └── hal_uart.h      # 头文件：接口定义与平台配置
├── src/
│   └── hal_uart.c      # 源文件：双模式实现
├── test/
│   └── test_hal.c      # 测试程序
├── build/              # 构建输出目录
└── Makefile            # 构建脚本
```

## 核心特性

### 1. 双模式架构

| 模式 | 宏定义 | 说明 |
|------|--------|------|
| 模拟模式 | `HAL_UART_MOCK_MODE` | 软件模拟，无需硬件 |
| Linux 硬件模式 | `HAL_UART_LINUX_MODE` | 真实串口通信 |

### 2. 模式切换方式

**方式一：Makefile 编译时切换**
```bash
make mock    # 编译模拟模式
make linux   # 编译 Linux 硬件模式（默认）
```

**方式二：修改头文件**（`include/hal_uart.h`）
```c
// 取消注释使用模拟模式
// #define HAL_UART_MOCK_MODE

// Linux 真实串口模式（默认启用）
#define HAL_UART_LINUX_MODE
```

## API 接口

### 数据结构

```c
typedef struct {
    uint32_t baudrate;       // 波特率：9600/19200/38400/57600/115200
    uint8_t  data_bits;      // 数据位：5/6/7/8
    char     parity;         // 校验位：'N'(无)/'O'(奇)/'E'(偶)
    uint8_t  stop_bits;      // 停止位：1/2
    bool     is_initialized; // 初始化状态

#ifdef HAL_UART_LINUX_MODE
    int      fd;             // Linux 串口文件描述符
    char     device[32];     // 设备路径，如 "/dev/ttyUSB0"
#endif
} HAL_UART_HandleTypeDef;
```

### 函数列表

| 函数 | 功能 | 返回值 |
|------|------|--------|
| `HAL_UART_Init()` | 初始化 UART | 0=成功, <0=错误 |
| `HAL_UART_Transmit()` | 发送数据 | 发送字节数/错误码 |
| `HAL_UART_Receive()` | 接收数据（带超时） | 接收字节数/错误码 |
| `HAL_UART_DeInit()` | 反初始化 | 无 |
| `HAL_UART_RxAvailable()` | 检查数据可读（非阻塞） | 0=无数据, 1=有数据, <0=错误 |

### 错误码定义

```c
#define HAL_UART_OK         0   // 成功
#define HAL_UART_ERROR     -1   // 通用错误
#define HAL_UART_NOT_INIT  -2   // 未初始化
#define HAL_UART_TIMEOUT   -3   // 超时
#define HAL_UART_IO_ERROR  -4   // IO 错误
```

## 使用示例

### 模拟模式示例

```c
#include "hal_uart.h"

int main(void) {
    HAL_UART_HandleTypeDef huart = {
        .baudrate = 115200,
        .data_bits = 8,
        .parity = 'N',
        .stop_bits = 1
    };

    // 初始化
    HAL_UART_Init(&huart);

    // 发送数据
    const char *tx = "Hello";
    HAL_UART_Transmit(&huart, (uint8_t *)tx, strlen(tx), 1000);

    // 接收数据（模拟返回 "OK"）
    uint8_t rx[64];
    int len = HAL_UART_Receive(&huart, rx, sizeof(rx), 5000);

    // 反初始化
    HAL_UART_DeInit(&huart);
    return 0;
}
```

### Linux 硬件模式示例

```c
#include "hal_uart.h"

int main(void) {
    HAL_UART_HandleTypeDef huart = {
        .baudrate = 115200,
        .data_bits = 8,
        .parity = 'N',
        .stop_bits = 1,
        .device = "/dev/ttyUSB0"  // 指定串口设备
    };

    // 初始化（打开真实串口）
    if (HAL_UART_Init(&huart) != HAL_UART_OK) {
        printf("串口初始化失败\n");
        return 1;
    }

    // 发送数据到硬件
    const char *tx = "AT\r\n";
    HAL_UART_Transmit(&huart, (uint8_t *)tx, strlen(tx), 1000);

    // 从硬件接收数据（带 5 秒超时）
    uint8_t rx[256];
    int len = HAL_UART_Receive(&huart, rx, sizeof(rx), 5000);
    if (len > 0) {
        printf("收到: %s\n", rx);
    } else if (len == HAL_UART_TIMEOUT) {
        printf("接收超时\n");
    }

    HAL_UART_DeInit(&huart);
    return 0;
}
```

## 关键实现细节

### 1. 模拟模式实现

**接收数据**（`src/hal_uart.c:172-180`）：
```c
// 模拟接收 - 返回固定字符串 "OK"
const char *mock_response = "OK";
uint16_t copy_len = size < 2 ? size : 2;
memcpy(data, mock_response, copy_len);
return copy_len;
```

### 2. Linux 硬件模式实现

**串口初始化**（`src/hal_uart.c:46-108`）：
- 使用 `open()` 打开设备（如 `/dev/ttyUSB0`）
- 使用 `termios` 配置波特率、数据位、校验位、停止位
- 支持波特率：9600/19200/38400/57600/115200

**接收数据**（`src/hal_uart.c:183-215`）：
```c
// 使用 select() 实现超时控制
fd_set readfds;
struct timeval tv;
tv.tv_sec = timeout / 1000;
tv.tv_usec = (timeout % 1000) * 1000;

int ret = select(huart->fd + 1, &readfds, NULL, NULL, &tv);
if (ret == 0) return HAL_UART_TIMEOUT;  // 超时

// 读取数据
ssize_t received = read(huart->fd, data, size);
```

**非阻塞检查**（`src/hal_uart.c:230-247`）：
```c
// select 超时设为 0，立即返回
struct timeval tv = {0, 0};
int ret = select(huart->fd + 1, &readfds, NULL, NULL, &tv);
return ret;  // 0=无数据, 1=有数据
```

## 编译与运行

### 编译

```bash
# 进入项目目录
cd hal_update

# 编译（默认 Linux 模式）
make

# 或显式指定模式
make linux   # Linux 硬件模式
make mock    # 模拟模式
```

### 运行测试

```bash
# 运行测试程序
./build/test_runner
```

### 清理构建

```bash
make clean
```

## 测试程序说明

`test/test_hal.c` 包含以下测试项：

1. **初始化测试** - 验证 UART 初始化
2. **发送测试** - 发送字符串 `Hello, UART!`
3. **接收测试** - 等待数据接收（超时 5 秒）
4. **非阻塞检查测试** - 检查数据可读性
5. **反初始化测试** - 清理资源

## 注意事项

### Linux 硬件模式

1. **权限问题**：访问串口设备需要权限
   ```bash
   sudo chmod 666 /dev/ttyUSB0
   # 或将用户加入 dialout 组
   sudo usermod -a -G dialout $USER
   ```

2. **设备路径**：根据实际硬件修改 `huart.device`
   - USB 转串口：`/dev/ttyUSB0`, `/dev/ttyUSB1`
   - 板载串口：`/dev/ttyS0`, `/dev/ttyS1`
   - 虚拟串口：`/dev/pts/0`（用于测试）

3. **波特率支持**：实际支持的波特率取决于硬件

### 模拟模式

- 发送数据仅打印到控制台
- 接收数据固定返回 `"OK"`
- 适用于单元测试和无硬件环境开发

## 扩展指南

### 添加新的硬件平台

1. 在 `hal_uart.h` 添加新的平台宏：
   ```c
   #define HAL_UART_STM32_MODE
   ```

2. 在 `hal_uart.c` 中添加平台相关代码：
   ```c
   #elif defined(HAL_UART_STM32_MODE)
       // STM32 HAL 实现
       HAL_UART_Receive(&huart->handle, data, size, timeout);
   ```

3. 更新 Makefile 支持新平台编译选项

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2024 | 初始版本，支持 MOCK / LINUX 双模式 |

## 相关文件

- 原始 HAL 项目：`/home/fly/fly/project/hal/`
- 设计文档：`/home/fly/fly/project/hal/hal.md`
