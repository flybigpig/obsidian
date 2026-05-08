# 编译器和标志
CC = gcc
CFLAGS = -Wall -Wextra -g -O2
TARGET = hello

# 默认目标
all: $(TARGET)

# 链接
$(TARGET): hello.o
	$(CC) $^ -o $@

# 编译：.o 依赖于 .c（跳过手动预处理和汇编步骤）
hello.o: hello.c
	$(CC) $(CFLAGS) -c $< -o $@

# 或者更简单，让 make 自动推导
# 只需保留以下规则即可：
# $(TARGET): hello.o
#     $(CC) $^ -o $@

# 清理
.PHONY: all clean
clean:
	rm -f *.i *.s *.o $(TARGET)
