@echo off
echo ══════════════════════════════════════════
echo        Obsidian Git 自动提交工具
echo ══════════════════════════════════════════
echo.

:: 显示当前状态
echo [信息] 检查当前状态...
echo [信息] 正在添加文件...

git add -A

echo [信息] 正在提交...
git commit -m "obidian update bat"

echo [信息] 正在推送到远程仓库...
git push 

if errorlevel 1 (
    echo.
    echo [警告] 推送到 master 失败，尝试推送到 main...
    git push origin main
)

echo.
echo ══════════════════════════════════════════
echo [✓] 提交完成！时间：%date% %time%
echo ══════════════════════════════════════════

:end
echo.
pause
