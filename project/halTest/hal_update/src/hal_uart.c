#include "hal_uart.h"
#include <stdio.h>
#include <string.h>

#ifdef HAL_UART_LINUX_MODE
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#endif

// 初始化 UART
int HAL_UART_Init(HAL_UART_HandleTypeDef *huart) {
    if (huart == NULL) {
        return HAL_UART_ERROR;
    }

    // 参数校验
    if (huart->baudrate == 0) {
        return HAL_UART_ERROR;
    }

#ifdef HAL_UART_MOCK_MODE
    // 模拟模式初始化
    printf("[HAL] UART initialized (MOCK): baudrate=%lu, data_bits=%u, parity=%c, stop_bits=%u\n",
           (unsigned long)huart->baudrate,
           huart->data_bits,
           huart->parity,
           huart->stop_bits);
    huart->is_initialized = true;
    return HAL_UART_OK;

#elif defined(HAL_UART_LINUX_MODE)
    // Linux 真实串口初始化
    if (strlen(huart->device) == 0) {
        strcpy(huart->device, "/dev/ttyUSB0");
    }

    huart->fd = open(huart->device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (huart->fd < 0) {
        perror("[HAL] Failed to open UART device");
        return HAL_UART_IO_ERROR;
    }

    // 清除非阻塞标志
    fcntl(huart->fd, F_SETFL, 0);

    struct termios options;
    tcgetattr(huart->fd, &options);

    // 设置波特率
    speed_t baud;
    switch (huart->baudrate) {
        case 9600:   baud = B9600;   break;
        case 19200:  baud = B19200;  break;
        case 38400:  baud = B38400;  break;
        case 57600:  baud = B57600;  break;
        case 115200: baud = B115200; break;
        default:     baud = B115200; break;
    }
    cfsetispeed(&options, baud);
    cfsetospeed(&options, baud);

    // 设置数据位
    options.c_cflag &= ~CSIZE;
    switch (huart->data_bits) {
        case 5: options.c_cflag |= CS5; break;
        case 6: options.c_cflag |= CS6; break;
        case 7: options.c_cflag |= CS7; break;
        default: options.c_cflag |= CS8; break;
    }

    // 设置校验位
    switch (huart->parity) {
        case 'O': // 奇校验
            options.c_cflag |= PARENB;
            options.c_cflag |= PARODD;
            break;
        case 'E': // 偶校验
            options.c_cflag |= PARENB;
            options.c_cflag &= ~PARODD;
            break;
        default:  // 无校验
            options.c_cflag &= ~PARENB;
            break;
    }

    // 设置停止位
    if (huart->stop_bits == 2) {
        options.c_cflag |= CSTOPB;
    } else {
        options.c_cflag &= ~CSTOPB;
    }

    // 启用接收，本地模式
    options.c_cflag |= (CLOCAL | CREAD);

    // 原始输入模式
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;

    tcsetattr(huart->fd, TCSANOW, &options);
    tcflush(huart->fd, TCIOFLUSH);

    huart->is_initialized = true;
    printf("[HAL] UART initialized (LINUX): device=%s, baudrate=%lu\n",
           huart->device, (unsigned long)huart->baudrate);
    return HAL_UART_OK;
#else
    #error "Please define HAL_UART_MOCK_MODE or HAL_UART_LINUX_MODE"
#endif
}

// 发送数据
int HAL_UART_Transmit(HAL_UART_HandleTypeDef *huart,
                      const uint8_t *data,
                      uint16_t size,
                      uint32_t timeout) {
    if (huart == NULL || data == NULL || size == 0) {
        return HAL_UART_ERROR;
    }

    if (!huart->is_initialized) {
        return HAL_UART_NOT_INIT;
    }

#ifdef HAL_UART_MOCK_MODE
    // 模拟发送
    printf("[HAL] UART transmit (MOCK): size=%u\n", size);
    printf("[HAL] Data: ");
    for (uint16_t i = 0; i < size; i++) {
        printf("%c", data[i]);
    }
    printf("\n");
    return size;

#elif defined(HAL_UART_LINUX_MODE)
    // Linux 真实发送
    (void)timeout; // timeout 在 write 中不直接使用

    ssize_t written = write(huart->fd, data, size);
    if (written < 0) {
        perror("[HAL] UART write failed");
        return HAL_UART_IO_ERROR;
    }
    tcdrain(huart->fd); // 等待发送完成
    return (int)written;
#endif
}

// 接收数据
int HAL_UART_Receive(HAL_UART_HandleTypeDef *huart,
                     uint8_t *data,
                     uint16_t size,
                     uint32_t timeout) {
    if (huart == NULL || data == NULL || size == 0) {
        return HAL_UART_ERROR;
    }

    if (!huart->is_initialized) {
        return HAL_UART_NOT_INIT;
    }

#ifdef HAL_UART_MOCK_MODE
    // 模拟接收 - 返回 "OK"
    printf("[HAL] UART receive (MOCK): size=%u, timeout=%lu\n", size, (unsigned long)timeout);
    const char *mock_response = "OK";
    uint16_t copy_len = size < 2 ? size : 2;
    memcpy(data, mock_response, copy_len);
    return copy_len;

#elif defined(HAL_UART_LINUX_MODE)
    // Linux 真实接收 - 使用 select 实现超时
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(huart->fd, &readfds);

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    int ret = select(huart->fd + 1, &readfds, NULL, NULL, &tv);
    if (ret < 0) {
        perror("[HAL] UART select failed");
        return HAL_UART_IO_ERROR;
    } else if (ret == 0) {
        return HAL_UART_TIMEOUT; // 超时
    }

    // 有数据可读
    ssize_t received = read(huart->fd, data, size);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return HAL_UART_TIMEOUT;
        }
        perror("[HAL] UART read failed");
        return HAL_UART_IO_ERROR;
    }

    return (int)received;
#endif
}

// 反初始化
void HAL_UART_DeInit(HAL_UART_HandleTypeDef *huart) {
    if (huart == NULL) {
        return;
    }

#ifdef HAL_UART_LINUX_MODE
    if (huart->fd >= 0) {
        close(huart->fd);
        huart->fd = -1;
    }
#endif

    printf("[HAL] UART deinitialized\n");
    huart->is_initialized = false;
}

// 检查是否有数据可读（非阻塞）
int HAL_UART_RxAvailable(HAL_UART_HandleTypeDef *huart) {
    if (huart == NULL || !huart->is_initialized) {
        return HAL_UART_ERROR;
    }

#ifdef HAL_UART_MOCK_MODE
    return 1; // 模拟模式始终有数据

#elif defined(HAL_UART_LINUX_MODE)
    fd_set readfds;
    struct timeval tv = {0, 0}; // 立即返回

    FD_ZERO(&readfds);
    FD_SET(huart->fd, &readfds);

    int ret = select(huart->fd + 1, &readfds, NULL, NULL, &tv);
    if (ret < 0) {
        return HAL_UART_IO_ERROR;
    }
    return ret; // 0 = 无数据, 1 = 有数据
#endif
}
