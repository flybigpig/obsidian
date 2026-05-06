cmake_minimum_required(VERSION 3.10)
project(Binder_IPC C)

# 设置 C 标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 打印项目信息
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "C Standard: ${CMAKE_C_STANDARD}")

# 包含目录
include_directories(include)

# 收集源文件
file(GLOB BINDER_SOURCES "src/*.c")
file(GLOB TEST_SOURCES "test/*.c")

# 创建静态库
add_library(binder ${BINDER_SOURCES})

# 创建测试程序
add_executable(test_runner ${TEST_SOURCES})
target_link_libraries(test_runner binder)

# 编译选项
target_compile_options(binder PRIVATE -Wall -Wextra -g)
target_compile_options(test_runner PRIVATE -Wall -Wextra -g)

# 定义输出目录
set_target_properties(binder PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(test_runner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 测试目标
enable_testing()
add_test(NAME Binder_Test COMMAND test_runner)

# 自定义目标
add_custom_target(run_tests
    COMMAND ${CMAKE_BINARY_DIR}/bin/test_runner
    DEPENDS test_runner
    COMMENT "Running Binder tests..."
)

add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning all build artifacts..."
)

# 打印配置信息
message(STATUS "Source files: ${BINDER_SOURCES}")
message(STATUS "Test files: ${TEST_SOURCES}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
