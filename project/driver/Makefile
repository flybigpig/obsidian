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
