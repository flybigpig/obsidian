好的！我来详细介绍 G++ 编译 C++ 的各种命令和用法。

## G++ 编译 C++ 命令

### 1. 基础编译

```bash
# 基本语法：g++ [选项] 源文件 -o 输出文件

# 最简单的编译（生成 a.out）
g++ hello.cpp

# 指定输出文件名
g++ hello.cpp -o hello

# 运行
./hello
```

### 2. 指定 C++ 标准

| 标准 | 选项 | 特性 |
|------|------|------|
| C++98 | `-std=c++98` 或 `-std=c++03` | 传统标准 |
| C++11 | `-std=c++11` | auto、lambda、智能指针、右值引用 |
| C++14 | `-std=c++14` | 泛型lambda、变量模板 |
| C++17 | `-std=c++17` | 结构化绑定、optional、filesystem |
| C++20 | `-std=c++20` | concepts、modules、协程、ranges |

```bash
g++ -std=c++11 hello.cpp -o hello
g++ -std=c++17 hello.cpp -o hello    # 推荐使用
g++ -std=c++20 hello.cpp -o hello    # 最新特性
```

### 3. 常用编译选项

| 选项 | 说明 | 示例 |
|------|------|------|
| `-o <file>` | 指定输出文件名 | `g++ hello.cpp -o app` |
| `-c` | 只编译不链接，生成 `.o` | `g++ -c hello.cpp` |
| `-g` | 生成调试信息 | `g++ -g hello.cpp` |
| `-O0` ~ `-O3` | 优化等级 | `g++ -O2 hello.cpp` |
| `-Wall` | 显示所有警告 | `g++ -Wall hello.cpp` |
| `-Wextra` | 显示额外警告 | `g++ -Wextra hello.cpp` |
| `-I<dir>` | 头文件搜索路径 | `g++ -I./include main.cpp` |
| `-L<dir>` | 库文件搜索路径 | `g++ -L./lib main.cpp` |
| `-l<lib>` | 链接库（如 pthread） | `g++ main.cpp -lpthread` |

### 4. 实用编译示例

```bash
# ==================== 调试版本 ====================
g++ -g -Wall -Wextra -O0 -std=c++17 hello.cpp -o hello_debug

# ==================== 发布版本（优化）====================
g++ -Wall -O2 -std=c++17 hello.cpp -o hello_release

# ==================== 多文件编译 ====================
# 一次性编译
g++ main.cpp utils.cpp helper.cpp -o myapp

# 分别编译再链接（推荐，大型项目）
g++ -c main.cpp -o main.o
g++ -c utils.cpp -o utils.o
g++ main.o utils.o -o myapp

# ==================== 链接第三方库 ====================
# 链接线程库
g++ main.cpp -o main -lpthread

# 链接多个库
g++ main.cpp -o app -lcurl -lssl -lcrypto

# 链接静态库/动态库
g++ main.cpp -L./lib -lmylib -o app
```

### 5. 编译带外部头文件的项目

```
project/
├── include/
│   └── myheader.h
├── src/
│   └── main.cpp
└── build/
```

```bash
# 指定头文件目录
g++ -I./include src/main.cpp -o build/app
```

### 6. 生成预处理/汇编/目标文件

```bash
# 预处理（查看宏展开结果）
g++ -E hello.cpp -o hello.i

# 生成汇编代码
g++ -S hello.cpp -o hello.s

# 生成目标文件 (.o)
g++ -c hello.cpp -o hello.o
```

### 7. Makefile 示例（简化编译流程）

```makefile
CXX = g++
CXXFLAGS = -Wall -g -std=c++17
TARGET = hello
SRCS = main.cpp utils.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET)

run: $(TARGET)
	./$(TARGET)
```

使用：
```bash
make          # 编译
make run      # 编译并运行
make clean    # 清理
```

### 8. CMake 现代构建方式（推荐大型项目）

**CMakeLists.txt**
```cmake
cmake_minimum_required(VERSION 3.10)
project(MyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(hello main.cpp utils.cpp)
```

使用：
```bash
mkdir build && cd build
cmake ..
make
./hello
```

---

## 推荐的常用命令组合

### 开发阶段
```bash
g++ -g -Wall -Wextra -pedantic -std=c++17 \
    -DDEBUG hello.cpp -o hello_debug
```

### 发布阶段
```bash
g++ -Wall -O2 -std=c++17 hello.cpp -o hello_release
```

### 生产环境（最高优化）
```bash
g++ -Wall -O3 -DNDEBUG -march=native -std=c++17 \
    hello.cpp -o hello_production
```

---

## 快速参考卡

```
┌─────────────────────────────────────────────────────┐
│              G++ 快速命令参考                         │
├─────────────────────────────────────────────────────┤
│ g++ file.cpp -o out        基本编译                   │
│ g++ -std=c++17             使用C++17标准              │
│ g++ -g                     生成调试信息               │
│ g++ -O2                    二级优化                   │
│ g++ -Wall                  显示所有警告               │
│ g++ -c file.cpp            只编译不链接                │
│ g++ file1.cpp file2.cpp    多文件编译                 │
│ g++ -I/path                添加头文件路径              │
│ g++ -llib                  链接库                     │
└─────────────────────────────────────────────────────┘
```

如果你有具体的编译问题或需要帮助解决编译错误，请告诉我！