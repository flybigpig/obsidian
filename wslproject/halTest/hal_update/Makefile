# HAL UART Update - Makefile
# 支持模拟模式和 Linux 真实硬件模式

CC = gcc
CFLAGS = -Wall -Wextra -I./include

# 默认使用 Linux 真实硬件模式
# 使用 make MOCK=1 切换到模拟模式
ifdef MOCK
    CFLAGS += -DHAL_UART_MOCK_MODE -UHAL_UART_LINUX_MODE
    MODE = MOCK
else
    MODE = LINUX
endif

SRC_DIR = src
TEST_DIR = test
BUILD_DIR = build

SOURCES = $(SRC_DIR)/hal_uart.c
TEST_SRC = $(TEST_DIR)/test_hal.c

TARGET = $(BUILD_DIR)/test_runner

.PHONY: all clean test mock linux

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SOURCES) $(TEST_SRC)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Build complete: $(MODE) mode"

mock:
	$(MAKE) MOCK=1

linux:
	$(MAKE) MOCK=0

test: all
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

# 显示当前模式
info:
	@echo "Current mode: $(MODE)"
	@echo "Use 'make mock' for mock mode"
	@echo "Use 'make linux' or 'make' for Linux hardware mode"
