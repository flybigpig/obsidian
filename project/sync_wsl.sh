#!/bin/bash

# 直接使用真实路径（避免符号链接问题）
SOURCE="/home/fly/work"  # WSL 内的真实路径
DEST=$(wslpath -u "C:\\D\\otherproject\\obsidian\\project")

echo "源: $SOURCE"
echo "目标: $DEST"

rsync -avh --progress --update --delete "$SOURCE/" "$DEST/"
rsync -avh --progress --update  --delete "$DEST/" "$SOURCE/"


# chmod +x ~/work/sync_wsl.sh

# ~/work/sync_wsl.sh

