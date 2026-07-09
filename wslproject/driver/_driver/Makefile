
# ==================== 编译器配置 ====================
CC       = gcc
CFLAGS   = -Wall -Wextra -g -I./include
LDFLAGS  =

# ==================== 目录配置 ====================
SRC_DIR  = ./src
TEST_DIR = ./test
BUILD_DIR= ./build
INC_DIR  = ./include

# ==================== 文件列表 ====================
SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

TEST_SRCS= $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS = $(patsubst $(TEST_DIR)/%.c, $(BUILD_DIR)/%.o, $(TEST_SRCS))

TARGET   = libdriver.a      # 静态库
TEST_TARGET = test_runner   # 测试程序

# ==================== 构建目标 ====================
.PHONY: all test clean run_test help


all: $(TARGET)

# 构建静态库
$(TARGET): $(OBJS)
	@echo "[BUILD] Creating static library: $@"
	ar rcs $@ $^

# 编译源文件
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "[BUILD] Compiling: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# 编译测试文件
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	@echo "[BUILD] Compiling test: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# 构建测试程序
$(TEST_TARGET): $(TEST_OBJS) $(TARGET)
	@echo "[BUILD] Linking test program: $@"
	$(CC) $(LDFLAGS) $(TEST_OBJS) -o $@ -L. -ldriver -I$(INC_DIR)

# 运行测试
test: $(TEST_TARGET)
	@echo "================================================"
	@echo "           Running Driver Tests..."
	@echo "================================================"
	./$(TEST_TARGET)
	@echo "================================================"

# 快速测试（不重新编译）
run_test: $(TEST_TARGET)
	./$(TEST_TARGET)

# 创建构建目录
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 清理
clean:
	@echo "[CLEAN] Removing build artifacts..."
	rm -rf $(BUILD_DIR) $(TARGET) $(TEST_TARGET)
	@echo "[CLEAN] Done!"

# 帮助信息
help:
	@echo "Driver Test Framework - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build static library (default)"
	@echo "  test       - Build and run tests"
	@echo "  run_test   - Run tests without rebuilding"
	@echo "  clean      - Remove build artifacts"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Directories:"
	@echo "  Source:     $(SRC_DIR)"
	@echo "  Tests:      $(TEST_DIR)"
	@echo "  Includes:   $(INC_DIR)"
	@echo "  Build:      $(BUILD_DIR)"

# 调试目标
debug: CFLAGS += -DDEBUG -O0
debug: clean all

# 优化目标
optimized: CFLAGS += -O3
optimized: clean all


# # ============================================================
# # Linux Kernel Driver Module Makefile
# # 生成 .ko 可加载内核驱动模块
# # ============================================================

# # ==================== 模块配置 ====================
# MODULE_NAME := driver

# # 声明为可加载内核模块 (生成 driver.ko)
# obj-m += $(MODULE_NAME).o

# # 如果有多个源文件, 取消注释并修改:
# # driver-objs := src/main.o src/init.o src/io.o

# # ==================== 内核构建环境 ====================
# KDIR := /lib/modules/$(shell uname -r)/build
# PWD  := $(shell pwd)

# # ========================================
# # 构建目标
# # ========================================

# .PHONY: all modules install clean load unload reload info log help

# all: modules

# # ----- 编译内核模块 -----
# modules:
# 	@echo "============================================="
# 	@echo " Building kernel module: $(MODULE_NAME).ko"
# 	@echo " Kernel version: $(shell uname -r)"
# 	@echo "============================================="
# 	$(MAKE) -C $(KDIR) M=$(PWD) modules

# # ----- 安装到系统 -----
# install:
# 	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
# 	depmod -a
# 	@echo "Module installed successfully!"
# 	@echo "Run 'sudo modprobe $(MODULE_NAME)' to load."

# # ----- 清理 -----
# clean:
# 	$(MAKE) -C $(KDIR) M=$(PWD) clean
# 	@echo "[CLEAN] Done!"

# # ----- 运行时操作 -----
# load:
# 	@if [ ! -f "$(MODULE_NAME).ko" ]; then \
# 		echo "Module not found! Run 'make' first."; \
# 		exit 1; \
# 	fi
# 	sudo insmod ./$(MODULE_NAME).ko
# 	@echo "--- Kernel log (last 10 lines) ---"
# 	dmesg | tail -10

# unload:
# 	sudo rmmod $(MODULE_NAME)
# 	@echo "Module unloaded."
# 	@echo "--- Kernel log (last 5 lines) ---"
# 	dmesg | tail -5

# reload: unload load

# # ----- 查询/调试 -----
# info:
# 	modinfo ./$(MODULE_NAME).ko

# log:
# 	@echo "=== Last 30 lines of kernel log ==="
# 	dmesg | tail -30

# # ----- 帮助 -----
# help:
# 	@echo ""
# 	@echo "============================================="
# 	@echo "  Linux Kernel Module Build System"
# 	@echo "  Module: $(MODULE_NAME).ko"
# 	@echo "============================================="
# 	@echo ""
# 	@echo "Usage: make [target]"
# 	@echo ""
# 	@echo " BUILD:"
# 	@echo "   make          - Build .ko module"
# 	@echo "   make clean    - Remove build artifacts"
# 	@echo "   make install  - Install to system (need root)"
# 	@echo ""
# 	@echo " RUNTIME:"
# 	@echo "   make load     - insmod the module"
# 	@echo "   make unload   - rmmod the module"
# 	@echo "   make reload   - rmmod + insmod"
# 	@echo "   make info     - modinfo output"
# 	@echo "   make log      - show dmesg"
# 	@echo ""
# 	@echo " WORKFLOW:"
# 	@echo "   make && make load     # build + load"
# 	@echo "   make reload           # quick rebuild cycle"
# 	@echo ""


