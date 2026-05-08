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

---

### 使用方法

1. 打开记事本，粘贴上面的代码
2. 保存为 `C:\sync.bat`（注意选择"所有文件"，编码选 UTF-8）
3. 双击运行 `sync.bat`

---

### 增强版（带日志和错误检查）

```bat
@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

set SOURCE=C:\wsl_work
set DEST=C:\D\otherproject\obsidian\project
set LOGFILE=C:\sync_log_%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%.txt

echo ============================================
echo   同步工具
echo   源: %SOURCE%
echo   目标: %DEST%
echo ============================================
echo.

if not exist "%SOURCE%" (
    echo [错误] 源目录不存在: %SOURCE%
    pause
    exit /b 1
)

if not exist "%DEST%" (
    echo [提示] 目标目录不存在，正在创建...
    mkdir "%DEST%"
)

echo [开始同步...]
robocopy "%SOURCE%" "%DEST%" /E /MT:8 /R:3 /W:5 /LOG:"%LOGFILE%"

echo.
echo ============================================
if %ERRORLEVEL% LEQ 7 (
    echo   同步成功完成
    echo   日志: %LOGFILE%
) else (
    echo   同步出现错误，错误码: %ERRORLEVEL%
)
echo ============================================
pause
```

---

### Robocopy 错误码说明

| 错误码 | 含义 |
|--------|------|
| 0 | 无错误，无文件复制 |
| 1 | 无错误，有文件复制 |
| 2 | 有额外文件在目标目录 |
| 3 | 有文件复制且有额外文件 |
| 4 | 有文件不匹配 |
| 5 | 有文件复制且有不匹配 |
| 6 | 有额外文件和不匹配 |
| 7 | 有文件复制、额外文件和不匹配 |
| 8+ | 发生错误 |

`LEQ 7` 表示成功。

---

你想用简洁版还是增强版？