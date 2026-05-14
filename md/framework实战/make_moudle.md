用户想要了解如何在 AOSP 中单独编译某个模块。这是 Android 开发中非常常见的需求，我需要提供完整的指南，包括各种编译命令、技巧和示例。
我来为你提供 **AOSP 单独编译模块** 的完整指南！⚡

## 🎯 AOSP 单模块编译完全手册

---

### **一、基础命令速查表**

| **命令** | **用途** | **适用场景** |
|---------|---------|-------------|
| `m <模块名>` | 编译指定模块 | 最常用 ✅ |
| `mm` | 编译当前目录模块 | 在模块目录下使用 |
| `mmm <路径>` | 编译指定路径的模块 | 批量或远程目录 |
| `mma` | 编译当前目录+依赖 | 模块有更新时 |
| `mmma <路径>` | 编译指定路径+依赖 | 完整重建 |
| `make <模块名>` | 同 `m` | 传统方式 |
| `one-shot <模块>` | 临时编译不安装 | 测试用 |

---

### **二、核心命令详解**

#### **1️⃣ `m` - 最推荐的模块编译**

```bash
# 基本语法
m <module_name> [选项]

# ==================== 示例：编译系统应用 ====================
# 编译 Settings 应用
m Settings

# 编译 SystemUI (状态栏/导航栏)
m SystemUI

# 编译 Launcher3 (桌面启动器)
m Launcher3QuickStep

# 编译 Framework (整个框架层 - 耗时较长)
m framework

# ==================== 示例：编译 Native 库 ====================
# 编译你的自定义 native 库
m libmyapp_jni
m libmyapp_core

# 编译 ART 运行时相关
m art
m libart

# ==================== 示例：编译 HAL 服务 ====================
# 硬件抽象层
m android.hardware.biometrics.fingerprint@2.1-service

# ==================== 带选项的编译 ====================
# 显示详细日志
m MySystemApp -j1 2>&1 | tee build.log

# 使用所有 CPU 核心 (默认行为)
m MySystemApp -j$(nproc)

# 只分析依赖不实际编译 (dry-run)
m nothing --make-mode showcommands MySystemApp
```

#### **2️⃣ `mm` - 当前目录快速编译**

```bash
# 用法：先 cd 到模块目录，然后运行 mm
cd packages/apps/MySystemApp
mm

# 等价于
cd packages/apps/Settings
mm

# 输出只包含该模块（更快）
```

**优点：**
- 不需要记住完整的模块名
- 自动检测当前目录的模块
- 通常比 `m` 更快（范围更小）

#### **3️⃣ `mmm` - 指定路径编译**

```bash
# 编译指定路径下的模块 (不需要 cd)
mmm packages/apps/MySystemApp

# 批量编译多个模块
mmm packages/apps/Settings packages/apps/SystemUI

# 编译整个子目录的所有模块
mmm frameworks/base/core/java

# 结合环境变量
mmm packages/apps/MySystemApp -j8
```

---

### **三、高级编译选项**

#### **1️⃣ 并行编译控制**

```bash
# 自动检测 CPU 核心数
m MySystemApp -j$(nproc)

# 手动指定并行数
m MySystemApp -j4        # 使用4个线程
m MySystemApp -j1         # 单线程 (方便看错误信息)
m MySystemApp -j16        # 高性能机器

# 根据内存调整 (每线程约需 2GB RAM)
# 16GB RAM -> j8
# 32GB RAM -> j16
# 64GB RAM -> j32
```

#### **2️⃣ 增量编译 vs 全量编译**

```bash
# ===== 增量编译 (快速，推荐日常使用) =====
m MySystemApp          # 只重新编译修改过的文件
mm                     # 当前目录增量编译

# ===== 全量编译 (干净重建) =====
# 先清理再编译
m clean-MySystemApp && m MySystemApp

# 或使用 make clean + 重编
make clean
m MySystemApp

# 强制重编所有依赖 (包括上游变化)
mma                    # 当前目录 + 所有依赖
mmma packages/apps/MySystemApp   # 指定路径 + 依赖
```

#### **3️⃣ 安装到设备**

```bash
# 编译并推送到连接的设备
m MySystemApp && adb install -r out/target/product/generic/system_ext/app/MySystemApp/MySystemApp.apk

# 或使用 adb sync (同步整个 system 分区)
adb root
adb remount
m MySystemApp
adb sync
adb reboot
```

---

### **四、查找模块名称**

#### **方法 1：使用 `get_build_var` 查询**

```bash
# 列出所有可编译的模块 (过滤关键字)
source build/envsetup.sh
lunch aosp_arm64-eng

# 搜索包含 "setting" 的模块
get_build_var ALL_MODULES | tr ' ' '\n' | grep -i setting

# 搜索包含 "systemui" 的模块
get_build_var ALL_MODULES | tr ' ' '\n' | grep -i systemui

# 搜索你的应用
get_build_var ALL_MODULES | tr ' ' '\n' | grep -i myapp
```

#### **方法 2：查看 Android.bp 中的 name 字段**

```bash
# 模块名就是 Android.bp 中定义的 name
grep '^    name:' packages/apps/MySystemApp/Android.bp

# 输出:
#     name: "MySystemApp",      ← 这就是模块名!
```

#### **方法 3：使用 soong_ui 工具**

```bash
# 列出特定目录下的模块
source build/envsetup.sh

# 查看 Soong 解析的模块信息
m nothing --module-info MySystemApp

# 查看模块依赖关系
m --make-mode showcommands MySystemApp 2>&1 | grep -E "(depend|require)"
```

---

### **五、常用模块名称速查表**

| **模块类型** | **模块名称** | **说明** |
|-------------|-------------|---------|
| **系统应用** | `Settings` | 设置 |
| | `SystemUI` | 系统界面 |
| | `Launcher3QuickStep` | 启动器 (手势导航版) |
| | `Calculator` | 计算器 |
| | `Camera2` | 相机 |
| **Framework** | `framework` | Java 框架 jar |
| | `framework-res` | 资源框架 |
| | `services` | 系统服务 |
| **Native 库** | `libbinder` | Binder IPC |
| | `liblog` | 日志库 |
| | `libcutils` | C 工具库 |
| | `surfaceflinger` | SurfaceFlinger |
| **ART 运行时** | `art` | ART 全部 |
| | `dex2oat` | DEX/AOT 编译器 |
| **HAL** | `android.hardware.camera.provider@2.5-service` | Camera HAL |
| **内核** | `kernel` | 内核镜像 |

---

### **六、实战脚本：智能单模块编译器**

```bash
#!/bin/bash
# ============================================================
#   smart_build.sh - AOSP 智能模块编译助手
#   支持: 快速编译、自动清理、设备部署、日志记录
# ============================================================

set -e

# 配置区
MODULE_NAME="${1:-MySystemApp}"
LOG_DIR="$HOME/aosp_build_logs"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="${LOG_DIR}/build_${MODULE_NAME}_${TIMESTAMP}.log"
CORES=$(nproc)

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}"
echo "╔════════════════════════════════════════╗"
echo "║   AOSP 智能模块编译工具 v2.0           ║"
echo "╠════════════════════════════════════════╣"
echo "║  模块: ${MODULE_NAME}                         ║"
echo "║  核心: ${CORES}                              ║"
echo "║  日志: ${LOG_FILE}            ║"
echo "╚════════════════════════════════════════╝"
echo -e "${NC}"

# 创建日志目录
mkdir -p "$LOG_DIR"

# 检查环境
check_environment() {
    if [ ! -f "build/envsetup.sh" ]; then
        echo -e "${RED}[错误] 请在 AOSP 根目录运行此脚本${NC}"
        exit 1
    fi
    
    echo "[信息] 初始化构建环境..."
    source build/envsetup.sh > /dev/null 2>&1
    
    # 检查是否已 lunch
    if [ -z "$TARGET_PRODUCT" ]; then
        echo "[信息] 未检测到 lunch 配置，使用默认目标..."
        lunch aosp_arm64-eng > /dev/null 2>&1
    fi
    
    echo -e "${GREEN}[✓] 环境: ${TARGET_PRODUCT}-${TARGET_BUILD_VARIANT}${NC}"
}

# 查找模块
find_module() {
    echo ""
    echo "[信息] 查找模块 '${MODULE_NAME}'..."
    
    # 尝试获取模块信息
    if m nothing --module-info "$MODULE_NAME" &>/dev/null; then
        echo -e "${GREEN}[✓] 模块已找到!${NC}"
        
        # 显示模块详细信息
        echo ""
        echo "--- 模块信息 ---"
        m nothing --module-info "$MODULE_NAME" 2>/dev/null || true
        return 0
    else
        echo -e "${RED}[✗] 未找到模块: ${MODULE_NAME}${NC}"
        echo ""
        echo "提示:"
        echo "  1. 检查模块名称拼写"
        echo "  2. 运行 'get_build_var ALL_MODULES' 搜索"
        echo "  3. 检查 Android.bp 中 name 字段"
        return 1
    fi
}

# 编译模块
build_module() {
    local BUILD_CMD="m ${MODULE_NAME} -j${CORES}"
    
    echo ""
    echo "=========================================="
    echo "  开始编译: ${MODULE_NAME}"
    echo "  命令: ${BUILD_CMD}"
    echo "  开始时间: $(date)"
    echo "=========================================="
    echo ""
    
    # 记录开始时间
    START_TIME=$(date +%s)
    
    # 执行编译 (同时输出到屏幕和日志文件)
    if ${BUILD_CMD} 2>&1 | tee "$LOG_FILE"; then
        END_TIME=$(date +%s)
        DURATION=$((END_TIME - START_TIME))
        
        echo ""
        echo "=========================================="
        echo -e "${GREEN}  ✅ 编译成功!${NC}"
        echo "  耗时: ${DURATION} 秒 ($(( DURATION / 60 )) 分 ${DURATION % 60 } 秒)"
        echo "  结束时间: $(date)"
        echo "  日志文件: ${LOG_FILE}"
        echo "=========================================="
        
        # 显示产物位置
        show_artifacts
        
        return 0
    else
        echo ""
        echo "=========================================="
        echo -e "${RED}  ❌ 编译失败!${NC}"
        echo "  请查看上方错误信息或日志:"
        echo "  ${LOG_FILE}"
        echo "=========================================="
        
        # 提供故障排除建议
        suggest_fixes
        
        return 1
    fi
}

# 显示编译产物
show_artifacts() {
    echo ""
    echo "--- 编译产物 ---"
    
    # 查找常见的产物类型
    find out/target/product/*/ \
        \( -name "*${MODULE_NAME}*.apk" -o \
           -name "*${MODULE_NAME}*.so" -o \
           -name "*${MODULE_NAME}*.jar" -o \
           -name "*${MODULE_NAME}*.a" \) \
        2>/dev/null | head -10 | while read f; do
        echo "  📦 $f"
    done
    
    echo ""
}

# 故障排除建议
suggest_fixes() {
    echo ""
    echo "--- 可能的解决方案 ---"
    echo ""
    echo "  1. 清理后重试:"
    echo "     make clean-${MODULE_NAME}"
    echo "     m ${MODULE_NAME}"
    echo ""
    echo "  2. 检查语法错误:"
    echo "     m ${MODULE_NAME} -j1 2>&1 | less"
    echo ""
    echo "  3. 查看详细依赖:"
    echo "     m nothing --make-mode showallcommands ${MODULE_NAME}"
    echo ""
    echo "  4. 更新 Soong 缓存:"
    echo "     m nothing --mkbp"
}

# 可选：推送到设备
push_to_device() {
    if command -v adb &> /dev/null && adb devices | grep -q "device$"; then
        echo ""
        read -p "是否要推送到连接的设备? [y/N]: " -n 1 -r
        echo
        
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo "[信息] 推送到设备..."
            
            # 查找 APK
            APK=$(find out/target/product/*/system*/app/*${MODULE_NAME}* \
                  -name "*.apk" 2>/dev/null | head -1)
            
            if [ -n "$APK" ]; then
                adb root
                adb remount
                adb install -r "$APK"
                echo -e "${GREEN}[✓] 已安装到设备${NC}"
            else
                echo -e "${YELLOW}[!] 未找到 APK 文件${NC}"
            fi
        fi
    fi
}

# ==================== 主流程 ====================
main() {
    check_environment
    
    if find_module; then
        if build_module; then
            push_to_device
            
            echo ""
            echo -e "${GREEN}🎉 全部完成!${NC}"
        fi
    fi
}

# 运行
main "$@"
```

**使用方法：**
```bash
chmod +x smart_build.sh

# 基础用法
./smart_build.sh MySystemApp

# 编译其他模块
./smart_build.sh Settings
./smart_build.sh SystemUI
./smart_build.sh framework
```

---

### **七、常见问题解决**

#### **问题 1：模块未找到**

```bash
错误: Module 'xxx' not found in source tree.

# 解决步骤:
1. 检查名称是否正确
   grep '"name"' packages/apps/xxx/Android.bp

2. 刷新 Soong 缓存
   m nothing --mkbp

3. 查找正确的模块名
   get_build_var ALL_MODULES | tr ' ' '\n' | grep -i keyword
```

#### **问题 2：编译缓存导致的问题**

```bash
# 清理单个模块缓存
m clean-<ModuleName>

# 清理所有中间产物
make clean

# 强制重编 (忽略时间戳)
touch <changed_source_files>
m <ModuleName>

# 删除 Soong 构建缓存 (慎用!)
rm -rf out/soong/.intermediates
rm -rf out/.module_paths
```

#### **问题 3：依赖缺失**

```bash
错误: undefined reference to 'xxx'
error: module 'yyy' not found

# 解决方案:
# 1. 使用 mma/mmma 代替 m/mmm (会自动处理依赖)
mma packages/apps/MySystemApp

# 2. 手动添加缺失依赖到 PRODUCT_PACKAGES
# 在产品 .mk 文件中:
PRODUCT_PACKAGES += \
    missing_lib \
    another_dependency

# 3. 查看完整依赖树
m nothing --make-mode dumpvar=all_modules
```

#### **问题 4：内存不足 (OOM)**

```bash
# 减少并行数
m MySystemApp -j4    # 或 -j2, -j1

# 增加 swap 空间
sudo swapfile create --size 16G /swapfile
sudo swapon /swapfile

# 限制 Java 堆内存
export ANDROID_JAVA_OPTIONS="-Xmx4g"
export SOONG_UI_BUILDER_OPTIONS="--memory 4096"
```

---

### **八、最佳实践总结**

```markdown
## 日常开发工作流

### ✅ 推荐做法:
1. **首次全量编译**: 
   ```bash
   m -j$(nproc)       # 或 make -j$(nproc)
   ```

2. **日常增量编译**: 
   ```bash
   m MyModule         # 或 cd 到目录后 mm
   ```

3. **修改公共头文件后**: 
   ```bash
   mma                 # 重编当前模块及所有依赖者
   ```

4. **遇到奇怪问题时**: 
   ```bash
   m clean-MyModule && m MyModule
   ```

### ⚠️ 注意事项:
- 不要在 AOSP 目录内使用 sudo
- 保持足够的磁盘空间 (建议 >100GB)
- 定期执行 `repo sync` 更新代码
- 编译前确保 `lunch` 正确
- 大改动后考虑全量 rebuild

### 🔧 效率技巧:
- 使用 ccache 加速重复编译
- 将源码放在 SSD 上
- 关闭不必要的后台程序
- 监控编译进度:
  ```bash
  watch -n 5 'ps aux | grep ninja | wc -l'
  ```
```

---

## 📊 编译命令对比速查

| **场景** | **命令** | **速度** | **安全性** |
|---------|---------|---------|-----------|
| 小改动，确定无依赖影响 | `m Mod` | ⚡最快 | 一般 |
| 改动了接口/API | `mma` | 🚀快 | 安全 |
| 不确定影响了谁 | `mmma path` | 🐢较慢 | 最安全 |
| 遇到诡异问题 | `clean && m` | 🐌最慢 | 彻底 |
| 只想测试不想安装 | `one-shot Mod` | ⚡快 | 无副作用 |

---

需要我针对某个具体模块提供更详细的编译指导吗？比如：
- 🖥️ **Framework 编译与调试**
- 📱 **SystemUI 定制开发**
- 🔧 **HAL 层服务编译**
- 🧪 **Native 层调试技巧**
