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

TARGET   = libhal.a        # 静态库
TEST_TARGET = test_runner  # 测试程序

# ==================== 构建目标 ====================
.PHONY: all test clean run_test

all: $(TARGET)

# 构建静态库
$(TARGET): $(OBJS)
	ar rcs $@ $^

# 编译源文件
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 编译测试文件
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 构建测试程序
$(TEST_TARGET): $(TEST_OBJS) $(TARGET)
	$(CC) $(LDFLAGS) $(TEST_OBJS) -o $@ -L. -lhal -I$(INC_DIR)

# 运行测试
test: $(TEST_TARGET)
	@echo "========================================"
	@echo "Running HAL Tests..."
	@echo "========================================"
	./$(TEST_TARGET)
	@echo "========================================"
	@echo "All tests passed! ✅"
	@echo "========================================"

# 创建构建目录
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 清理
clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TEST_TARGET)
