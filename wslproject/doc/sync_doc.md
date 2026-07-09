创建一个 `sync.bat` 文件，内容如下：

```bat
@echo off
chcp 65001 >nul
echo ============================================
echo   同步 wsl_work 到 obsidian/project
echo ============================================
echo.

robocopy "C:\wsl_work" "C:\D\otherproject\obsidian\project" /E /MT:8 /R:3 /W:5

echo.
echo ============================================
echo   同步完成
echo ============================================
pause
```

```
# sh
# 双向同步
# rsync -avh --progress --update /mnt/c/wsl_work/ /mnt/c/D/otherproject/obsidian/project/
# rsync -avh --progress --update /mnt/c/D/otherproject/obsidian/project/ /mnt/c/wsl_work/  创建sh

#!/bin/bash

SOURCE="/mnt/c/wsl_work"
DEST="/mnt/c/D/otherproject/obsidian/project"
LOGFILE="/mnt/c/sync_$(date +%Y%m%d_%H%M%S).log"

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOGFILE"
}

log "========================================"
log "开始双向同步"
log "源: $SOURCE"
log "目标: $DEST"
log "========================================"

mkdir -p "$DEST"

log "[1/2] 同步: 源 -> 目标"
rsync -avh --progress --update "$SOURCE/" "$DEST/" | tee -a "$LOGFILE"

log "[2/2] 同步: 目标 -> 源"
rsync -avh --progress --update "$DEST/" "$SOURCE/" | tee -a "$LOGFILE"

log "========================================"
log "双向同步完成"
log "日志: $LOGFILE"
log "========================================"


```
