#!/bin/bash
set -e

# 固定文件名 + 你当前的真实路径
PKG="Typora-linux-x64-1.13.4.tar.gz"
INST_PATH="/home/fly/work/Typora"

# 检查当前目录是否有压缩包
if [ ! -f "$PKG" ]; then
    echo -e "\033[31m错误：当前目录没有找到 $PKG\033[0m"
    echo "当前目录：$(pwd)"
    ls
    exit 1
fi

# 解压
echo "→ 正在解压..."
tar -zxf "$PKG" --strip-components=1 -C "$INST_PATH"

# 配置命令
echo "alias typora='/home/fly/work/Typora/Typora'" >> ~/.bashrc
source ~/.bashrc

echo -e "\033[32m✅ 安装完成！\033[0m"
echo "输入：typora   即可启动"