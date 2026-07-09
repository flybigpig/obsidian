# -----------单文件  

# 设置 CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称和语言
project(Hello LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行目标
add_executable(hello hello.cpp)


