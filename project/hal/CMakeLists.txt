cmake_minimum_required(VERSION 3.10)
project(HAL_UART C)

# 设置 C 标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 包含目录
include_directories(include)

# 收集源文件
file(GLOB HAL_SOURCES "src/*.c")
file(GLOB TEST_SOURCES "test/*.c")

# 创建静态库
add_library(hal ${HAL_SOURCES})

# 创建测试程序
add_executable(test_runner ${TEST_SOURCES})
target_link_libraries(test_runner hal)

# 编译选项
target_compile_options(hal PRIVATE -Wall -Wextra -g)
target_compile_options(test_runner PRIVATE -Wall -Wextra -g)

# 测试目标
enable_testing()
add_test(NAME HAL_Test COMMAND test_runner)
