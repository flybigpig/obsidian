#  ------------单文件 ------------          
# # 编译器
# CXX = g++

# # 编译选项
# CXXFLAGS = -Wall -g -std=c++17

# # 目标文件
# TARGET = hello

# # 源文件
# SRCS = hello.cpp

# # 默认目标
# all: $(TARGET)

# # 链接规则
# $(TARGET): $(SRCS)
# 	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

# # 清理
# clean:
# 	rm -f $(TARGET)

# # 运行
# run: $(TARGET)
# 	./$(TARGET)

#  ------------多文件 ------------     

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
