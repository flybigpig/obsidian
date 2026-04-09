GitHub 网页端**无法直接修改文件编码**，必须在**本地修改编码 → 提交 → 推送**到 GitHub，才能让远程仓库的文件以 UTF‑8 正常显示中文。

### 一、核心前提（必看）

- GitHub 网页**只按 UTF‑8 解析文本文件**，非 UTF‑8（如 GBK）会显示为 `\uXXXX` 数字串或乱码。
- Git 不存储编码信息，仅存二进制内容，**必须本地转码后提交**。
- 推荐统一用 **UTF‑8 无 BOM**（Windows 下最稳妥）。

---

### 二、本地修改编码（3 种常用方法）

#### 方法 1：VS Code（推荐，可视化操作）

1. 用 VS Code 打开目标文件。
2. 看右下角：显示当前编码（如 GBK）。
3. 点击编码 → 选择 **通过编码保存** → 选 **UTF‑8**（无 BOM）。
4. 保存文件。

#### 方法 2：Notepad++（Windows 常用）

1. 打开文件 → 菜单 **编码**。
2. 查看当前编码 → 选择 **转为 UTF‑8 无 BOM 格式**。
3. 保存。

#### 方法 3：命令行批量转换（适合多文件）

bash

运行

```
# macOS/Linux（GBK → UTF‑8）
iconv -f GBK -t UTF-8 原文件 > 新文件 && mv 新文件 原文件

# Windows Git Bash（批量转 .txt/.md）
for f in *.txt *.md; do iconv -f GBK -t UTF-8 "$f" > "$f.tmp" && mv "$f.tmp" "$f"; done
```

---

### 三、提交并推送到 GitHub

bash

运行

```
# 1. 添加修改
git add 文件名
# 或批量添加所有文件
git add --renormalize .  # 按当前编码配置重新规范化

# 2. 提交
git commit -m "fix: 编码转为 UTF‑8"

# 3. 推送
git push origin 分支名
```

---

### 四、仓库级规范（避免后续问题）

在仓库根目录创建 `.gitattributes`，强制 Git 按 UTF‑8 处理文本文件：

plaintext

```
# 所有文本文件用 UTF‑8，LF 换行
* text=auto working-tree-encoding=UTF-8 eol=lf

# 特定类型（如 .md/.java）
*.md text working-tree-encoding=UTF-8
*.java text working-tree-encoding=UTF-8
```

提交并推送 `.gitattributes`，确保全仓库编码一致。

---

### 五、验证效果

1. 刷新 GitHub 对应文件页面。
2. 中文正常显示 → 成功；仍为 `\uXXXX` → 本地未正确转码（检查是否为 UTF‑8 无 BOM）。
