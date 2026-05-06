好的！我来详细介绍如何使用 CMake 编译项目。

## 使用 CMake 编译

### 1. 什么是 CMake？

**CMake** 是一个跨平台的构建工具，可以生成各种构建系统（如 Makefile、Visual Studio、Xcode 等）。它是现代 C/C++ 项目最常用的构建系统。

```
CMake 工作流程：
源代码 → CMakeLists.txt → (cmake) → 构建文件 → (make) → 可执行程序
```

### 2. 基础目录结构

```
project/
├── hello.cpp          # 源代码
└── CMakeLists.txt     # CMake 配置文件（必须）
```

**hello.cpp**
```cpp
#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
```

### 3. 创建 CMakeLists.txt

#### 方案一：基础版（单文件）

```cmake
# 设置 CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称和语言
project(Hello LANGUAGES CXX)

# 添加可执行目标
add_executable(hello hello.cpp)
```

#### 方案二：推荐版（带选项配置）

```cmake
# ========== 基本设置 ==========
cmake_minimum_required(VERSION 3.16)
project(
    HelloProject
    VERSION 1.0.0
    DESCRIPTION "Hello World Example"
    LANGUAGES CXX
)

# ========== C++ 标准设置 ==========
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ========== 编译选项 ==========
option(BUILD_TESTS "Build tests" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug mode - enabling debug info")
    add_compile_options(-g -O0 -DDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release mode - optimizing")
    add_compile_options(-O2 -DNDEBUG)
endif()

# ========== 构建目标 ==========
add_executable(hello hello.cpp)

# 链接库（示例）
# target_link_libraries(hello PRIVATE pthread)

# ========== 安装规则 ==========
install(TARGETS hello DESTINATION bin)
```

### 4. 编译步骤

```bash
# 进入项目目录
cd /path/to/project

# ====== 方法一：Out-of-source 构建（推荐）======
mkdir build && cd build

# 生成构建文件
cmake ..

# 编译
cmake --build .

# 运行
./hello

# ====== 完整命令（一行搞定）======
mkdir build && cd build && cmake .. && make && ./hello

# ====== 或使用 cmake --build ======
cmake -B build                    # 创建并配置 build 目录
cmake --build build               # 编译
./build/hello                     # 运行
```

### 5. 执行过程演示

```bash
$ ls
CMakeLists.txt  hello.cpp

$ mkdir build && cd build

$ cmake ..
-- The C compiler identification is GNU 11.x
-- The CXX compiler identification is GNU 11.x
-- Configuring done
-- Generating done
-- Build files have been written to: /path/to/project/build

$ cmake --build .
[ 50%] Building CXX object CMakeFiles/hello.dir/hello.cpp.o
[100%] Linking CXX executable hello
[100%] Built target hello

$ ./hello
Hello, World!
```

### 6. 常用 CMake 选项

```bash
# ====== 指定编译类型 ======
cmake .. -DCMAKE_BUILD_TYPE=Debug       # 调试版本
cmake .. -DCMAKE_BUILD_TYPE=Release     # 发布版本
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo # 带调试信息的发布版本

# ====== 指定安装路径 ======
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local

# ====== 指定编译器 ======
cmake .. -DCMAKE_C_COMPILER=gcc
cmake .. -DCMAKE_CXX_COMPILER=g++

# ====== 开启/关闭选项 ======
cmake .. -DBUILD_TESTS=ON

# ====== 详细输出 ======
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
```

### 7. 多文件项目示例

**目录结构**
```
project/
├── CMakeLists.txt          # 根 CMakeLists
├── include/
│   └── utils.h
├── src/
│   ├── main.cpp
│   └── utils.cpp
└── README.md
```

**根 CMakeLists.txt**
```cmake
cmake_minimum_required(VERSION 3.16)
project(MyApp VERSION 1.0.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 头文件搜索路径
include_directories(include)

# 收集所有源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 创建可执行文件
add_executable(myapp ${SOURCES})

# Windows 下链接控制台子系统（可选）
if(WIN32)
    set_target_properties(myapp PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()
```

### 8. 多目录/多模块项目示例

```
project/
├── CMakeLists.txt
├── src/
│   ├── CMakeLists.txt        # 子模块 CMakeLists
│   ├── main.cpp
│   └── lib/
│       ├── CMakeLists.txt    # 库的 CMakeLists
│       ├── mathlib.cpp
│       └── mathlib.h
```

**根 CMakeLists.txt**
```cmake
cmake_minimum_required(VERSION 3.16)
project(MyApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# 添加子目录
add_subdirectory(src/lib)   # 先编译库
add_subdirectory(src)       # 再编译主程序
```

**src/lib/CMakeLists.txt**（静态库）
```cmake
add_library(mathlib STATIC mathlib.cpp mathlib.h)
target_include_directories(mathlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
```

**src/CMakeLists.txt**
```cmake
add_executable(main main.cpp)
target_link_libraries(main PRIVATE mathlib)
```

### 9. CMake 常用命令速查表

| 命令 | 说明 | 示例 |
|------|------|------|
| `cmake_minimum_required` | 设置最低版本 | `cmake_minimum_required(VERSION 3.10)` |
| `project` | 定义项目 | `project(MyApp LANGUAGES CXX)` |
| `add_executable` | 创建可执行文件 | `add_executable(app main.cpp)` |
| `add_library` | 创建库 | `add_library(mylib STATIC lib.cpp)` |
| `add_subdirectory` | 添加子目录 | `add_subdirectory(src)` |
| `target_link_libraries` | 链接库 | `target_link_libraries(app PRIVATE mylib)` |
| `target_include_directories` | 添加头文件路径 | `target_include_directories(app PUBLIC include)` |
| `target_compile_options` | 添加编译选项 | `target_compile_options(app PRIVATE -Wall)` |
| `find_package` | 查找外部包 | `find_package(OpenCV REQUIRED)` |
| `install` | 安装规则 | `install(TARGETS app DESTINATION bin)` |
| `message` | 打印信息 | `message(STATUS "Building...")` |
| `option` | 定义选项 | `option(USE_CUDA "Enable CUDA" OFF)` |

### 10. 清理与重新构建

```bash
# 清理（删除整个build目录）
rm -rf build

# 重新构建
mkdir build && cd build && cmake .. && cmake --build .

# 或使用 cmake 3.15+ 的便捷命令
cmake --build build --clean-first   # 先清理再编译
```

### 11. 快速参考卡

```
┌─────────────────────────────────────────────────────┐
│              CMake 常用命令                           │
├─────────────────────────────────────────────────────┤
│ mkdir build && cd build                             │
│ cmake ..              配置项目                        │
│ cmake --build .       编译项目                        │
│ cmake .. -DCMAKE_BUILD_TYPE=Debug                   │
│ cmake .. -DCMAKE_BUILD_TYPE=Release                 │
│ cmake --install .     安装到指定位置                  │
│ rm -rf build          完全清理                       │
└─────────────────────────────────────────────────────┘
```

---

如果你需要我帮你创建实际的 `CMakeLists.txt` 和 `hello.cpp` 文件，请切换到 **CRAFT 模式**！