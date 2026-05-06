好的！我来详细介绍 GCC 编译命令的各种用法。

## GCC 基础编译命令

### 1. 基本编译流程

```bash
# 基本语法：gcc [选项] 源文件 -o 输出文件

# 最简单的编译（生成 a.out）
gcc hello.c

# 指定输出文件名
gcc hello.c -o hello

# 运行生成的程序
./hello        # Linux/Mac
hello.exe      # Windows (CMD)
```

### 2. 完整编译过程（4个阶段）

```bash
# 预处理 (.c -> .i) - 展开宏、处理头文件
gcc -E hello.c -o hello.i

# 编译 (.i -> .s) - 生成汇编代码
gcc -S hello.c -o hello.s

# 汇编 (.s -> .o) - 生成目标文件
gcc -c hello.c -o hello.o

# 链接 (.o -> 可执行文件) - 链接库函数
gcc hello.o -o hello
```

### 3. 常用编译选项

| 选项 | 说明 | 示例 |
|------|------|------|
| `-o <file>` | 指定输出文件名 | `gcc hello.c -o myapp` |
| `-c` | 只编译不链接，生成 `.o` 文件 | `gcc -c hello.c` |
| `-E` | 只进行预处理 | `gcc -E hello.c` |
| `-S` | 只编译到汇编语言 | `gcc -S hello.c` |
| `-g` | 生成调试信息（GDB用） | `gcc -g hello.c -o debug_hello` |
| `-O0` ~ `-O3` | 优化等级（0=无，3=最高） | `gcc -O2 hello.c -o fast_hello` |
| `-Wall` | 显示所有警告信息 | `gcc -Wall hello.c` |
| `-Werror` | 将警告视为错误 | `gcc -Werror hello.c` |
| `-std=c11` | 指定 C 语言标准 | `gcc -std=c11 hello.c` |
| `-I<dir>` | 添加头文件搜索路径 | `gcc -I./include hello.c` |
| `-L<dir>` | 添加库文件搜索路径 | `gcc -L./lib hello.c` |
| `-l<lib>` | 链接指定库 | `gcc hello.c -lm` |

### 4. 实用编译示例

```bash
# ==================== 调试版本 ====================
gcc -g -Wall -O0 hello.c -o hello_debug

# ==================== 发布版本（带警告和优化）====================
gcc -Wall -Wextra -O2 hello.c -o hello_release

# ==================== 使用 C99/C11 标准 ====================
gcc -std=c99   program.c -o program    # C99
gcc -std=c11   program.c -o program    # C11
gcc -std=c17   program.c -o program    # C17

# ==================== 多文件编译 ====================
# 方法1：一次性编译
gcc main.c utils.c helper.c -o myapp

# 方法2：分别编译再链接（推荐，修改时只重编改动的文件）
gcc -c main.c -o main.o
gcc -c utils.c -o utils.o
gcc -c helper.c -o helper.o
gcc main.o utils.o helper.o -o myapp

# ==================== 包含数学库 (-lm) ====================
gcc calc.c -o calc -lm

# ==================== 定义宏 ====================
gcc -DDEBUG hello.c -o hello_debug       # 定义 DEBUG 宏
gcc -DVERSION=\"1.0\" hello.c -o hello   # 定义带值的宏
```

### 5. G++ (C++编译器)

```bash
# 基本用法
g++ hello.cpp -o hello

# 指定 C++ 标准
g++ -std=c++11  hello.cpp -o hello
g++ -std=c++14  hello.cpp -o hello
g++ -std=c++17  hello.cpp -o hello
g++ -std=c++20  hello.cpp -o hello

# 带调试信息和警告
g++ -g -Wall -std=c++17 hello.cpp -o hello
```

### 6. 常用 Makefile 简化编译

```makefile
CC = gcc
CFLAGS = -Wall -g
TARGET = hello
SRCS = main.c utils.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET)
```

使用：
```bash
make          # 编译
make clean    # 清理
```

### 7. 查看帮助

```bash
gcc --help                    # 查看所有选项
gcc --version                 # 查看 GCC 版本
man gcc                       # 详细手册
```

---

## 推荐的常用组合命令

```bash
# 开发时使用（带完整警告 + 调试信息）
gcc -g -Wall -Wextra -pedantic -std=c11 hello.c -o hello

# 发布时使用（高优化 + 警告）
gcc -Wall -Wextra -O2 -std=c11 hello.c -o hello
```

如果你有具体的编译需求或遇到编译问题，请告诉我！