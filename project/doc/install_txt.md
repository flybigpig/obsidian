# Fedora 常用文本编辑器（WSL/桌面都能用）
## 终端常用（无界面，轻量）
### 1. nano（最简单，新手首选）
自带一般都有，不用装：
```bash
nano 文件名.txt
```
保存：`Ctrl+O` → 回车 → 退出 `Ctrl+X`

### 2. vim / vi（高手编辑器）
```bash
vim 文件名.txt
```
没有就装：
```bash
sudo dnf install -y vim
```

## 图形界面编辑器（WSLg 桌面可用）
### 1. gedit（系统默认记事本，最简单）
安装：
```bash
sudo dnf install -y gedit
```
运行：
```bash
gedit
```

### 2. mousepad（轻量简洁，XFCE 常用）
```bash
sudo dnf install -y mousepad
```

### 3. VS Code（最强代码/文本编辑）
```bash
sudo dnf install -y code
```

## 快速总结
- 终端临时改配置：用 **nano**
- 桌面点点开txt：用 **gedit**
- 写代码：用 **VS Code**

你要我给你装一个**默认好用、右键直接打开txt**的编辑器吗？