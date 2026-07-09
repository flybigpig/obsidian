我来帮你了解 Makefile 的高级用法。先看看你当前的 makefile 内容。
你已经添加了 `.PHONY: clean` 目标！很好。让我为你介绍 Makefile 的 **高级用法**，你可以基于现有的 makefile 进行升级：

---

# Makefile 高级指南

## 📌 你当前的基础版本 vs 高级版本对比

### 1. 使用变量（Variables）

```makefile
# 定义变量
CC = gcc
CFLAGS = -Wall -g -O2
TARGET = hello
SRCS = hello.c

# 最终目标
$(TARGET): $(TARGET).o
	$(CC) $(TARGET).o -o $(TARGET)

# 编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
```

| 变量 | 含义 |
|------|------|
| `CC` | C 编译器 |
| `CFLAGS` | 编译选项（`-Wall` 警告、`-g` 调试信息、`-O2` 优化） |
| `TARGET` | 目标名称 |
| `$<` | 第一个依赖文件 |
| `$@` | 目标文件 |

---

### 2. 使用自动变量与模式规则

```makefile
CC = gcc
CFLAGS = -Wall -g -std=c11
TARGET = hello

$(TARGET): $(TARGET).o
	$(CC) $^ -o $@

# 模式规则：所有 .o 文件都从对应的 .c 文件生成
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o $(TARGET)
```

**自动变量速查表：**

| 变量 | 含义 | 示例 |
|------|------|------|
| `$@` | **目标文件名** | `hello.o` |
| `$^` | **所有依赖文件** | `hello.c` |
| `$<` | **第一个依赖文件** | `hello.c` |
| `$*` | **目标的主干名**（无后缀） | `hello` |

---

### 3. 添加函数（Functions）

```makefile
CC = gcc
CFLAGS = -Wall -g

# 自动查找当前目录下所有 .c 文件
SRCS = $(wildcard *.c)

# 将 .c 替换为 .o 得到目标文件列表
OBJS = $(patsubst %.c, %.o, $(SRCS))

TARGET = app

$(TARGET): $(OBJS)
	$(CC) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean
all: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
```

**常用函数：**

| 函数 | 作用 | 示例结果 |
|------|------|---------|
| `$(wildcard *.c)` | 查找匹配的文件 | `a.c b.c c.c` |
| `$(patsubst %.c,%.o,$(SRCS))` | 替换后缀 | `a.o b.o c.o` |
| `$(notdir path)` | 去掉路径部分 | `hello.c` |
| `$(dir src/hello.c)` | 取目录部分 | `src/` |

---

### 4. 完整高级版 Makefile 模板

```makefile
# ==================== 编译器配置 ====================
CC       = gcc
CFLAGS   = -Wall -Wextra -g -std=c11
LDFLAGS  =
LIBS     =

# ==================== 项目配置 ====================
TARGET   = hello
BUILD_DIR= ./build
SRC_DIR  = .
SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# ==================== 目标规则 ====================
.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

# 创建 build 目录并编译
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 运行程序
run: $(TARGET)
	./$(TARGET)

# 清理
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
```

**这个版本的优势：**
- ✅ 自动扫描源文件（无需手动维护）
- ✅ 分离构建目录（保持源码整洁）
- ✅ 支持 `make run` 一键运行
- ✅ 只需顺序依赖（`| $(BUILD_DIR)`）

---

## 🎯 推荐升级路径

根据你的学习进度，建议按以下步骤升级：

1. **初级** → 添加变量和 `.PHONY`
2. **中级** → 使用模式规则 `%` 和自动变量 `$@ $^ $<`
3. **高级** → 使用函数 `wildcard`、`patsubst`、构建目录分离
4. **专家级** → 条件编译、多平台支持、自动化测试

你想尝试哪个版本的改进？我可以帮你具体修改 makefile！