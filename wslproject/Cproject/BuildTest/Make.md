好的！我来教你如何使用 Make 和 Makefile 编译 `hello.cpp`。

## 使用 Make 编译 hello.cpp

### 1. 准备源文件

**hello.cpp**
```cpp
#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
```

### 2. 创建 Makefile

在 `hello.cpp` 同目录下创建 `Makefile` 文件（**注意：无扩展名，M 可大写可小写**）

#### 方案一：简单 Makefile（推荐入门使用）

```makefile
# 编译器
CXX = g++

# 编译选项
CXXFLAGS = -Wall -g -std=c++17

# 目标文件
TARGET = hello

# 源文件
SRCS = hello.cpp

# 默认目标
all: $(TARGET)

# 链接规则
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

# 清理
clean:
	rm -f $(TARGET)

# 运行
run: $(TARGET)
	./$(TARGET)
```

> **注意**：Makefile 中缩进必须使用 **Tab 键**，不能用空格！

#### 方案二：完整 Makefile（推荐多文件项目）

```makefile
# ========== 编译器配置 ==========
CXX = g++
CXXFLAGS = -Wall -Wextra -g -std=c++17
LDFLAGS =

# ========== 项目文件 ==========
TARGET = hello
SRCS = hello.cpp
OBJS = $(SRCS:.cpp=.o)

# ========== 构建规则 ==========
.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^
	@echo "构建完成: $(TARGET)"

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ========== 辅助命令 ==========
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "清理完成"

run: $(TARGET)
	./$(TARGET)

debug: CXXFLAGS += -DDEBUG
debug: clean all
	@echo "调试版本已构建"
```

### 3. 目录结构

```
project/
├── hello.cpp      # 源代码
└── Makefile       # 构建脚本
```

### 4. 使用 Make 命令

```bash
# 进入项目目录
cd /path/to/project

# ====== 编译 ======
make              # 或 make all，编译生成 hello
make run          # 编译并运行

# ====== 运行 ======
./hello           # 直接运行

# ====== 清理 ======
make clean        # 删除生成的文件

# ====== 其他目标 ======
make debug        # 构建调试版本（带 DEBUG 宏）
```

### 5. 执行效果演示

```bash
$ make
g++ -Wall -Wextra -g -std=c++17 -c hello.cpp -o hello.o
g++  -o hello hello.o
构建完成: hello

$ ./hello
Hello, World!

$ make clean
rm -f hello.o hello
清理完成

$ make run
g++ -Wall -Wextra -g -std=c++17 -c hello.cpp -o hello.o
g++  -o hello hello.o
构建完成: hello
Hello, World!
```

### 6. Makefile 语法说明

| 符号 | 含义 | 示例 |
|------|------|------|
| `=` | 赋值（延迟展开） | `CXX = g++` |
| `:=` | 赋值（立即展开） | `CC := gcc` |
| `+=` | 追加值 | `CXXFLAGS += -O2` |
| `$()` | 变量引用 | `$(CXX)` |
| `$@` | 当前目标名 | |
| `$<` | 第一个依赖文件 | |
| `$^` | 所有依赖文件 | |
| `.PHONY:` | 声明伪目标 | |

### 7. 进阶：多文件项目示例

```
project/
├── src/
│   ├── main.cpp
│   ├── utils.cpp
│   └── math.cpp
├── include/
│   ├── utils.h
│   └── math.h
└── Makefile
```

**Makefile**
```makefile
CXX = g++
CXXFLAGS = -Wall -g -std=c++17 -I./include
TARGET = myapp
SRCS = src/main.cpp src/utils.cpp src/math.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^
	@echo "构建成功: $(TARGET)"

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)
```

### 8. 快速参考卡

```
┌─────────────────────────────────────────────┐
│         Make 常用命令                         │
├─────────────────────────────────────────────┤
│ make            编译默认目标                   │
│ make all        同上                          │
│ make clean      清理生成文件                   │
│ make run        编译并运行                     │
│ make -n         模拟执行（不真正运行）          │
│ make -f file    指定Makefile                  │
│ make -j4        并行编译（4线程）              │
└─────────────────────────────────────────────┘
```

---

如果你需要我帮你创建实际的文件，请切换到 **CRAFT 模式**，我可以帮你创建 `hello.cpp` 和 `Makefile`！