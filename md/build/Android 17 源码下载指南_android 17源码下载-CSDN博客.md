## Android 17 源码下载指南

> **适用环境**: Ubuntu 20.04+ / Debian 系 Linux  
> **日期**: 2026-06-11  
> **目标**: 下载 Android 17（主干分支 `main`）完整 AOSP 源码

___

### 一、前置依赖

#### 1.1 系统依赖包

```bash
sudo apt update
sudo apt install -y \
    git \
    curl \
    python3 \
    python3-pip \
    openssh-clientbash1234567
```

#### 1.2 Git 全局配置

```csharp
git config --global user.name  "你的名字"
git config --global user.email "你的邮箱"bash12
```

___

### 二、安装 repo 工具

`repo` 是 Google 为 Android 源码管理开发的工具，用于管理几百个 git 仓库。

#### 2.1 标准安装方式（需科学上网）

```bash
mkdir -p ~/bin
curl https://storage.googleapis.com/git-repo-downloads/repo -o ~/bin/repo
chmod a+x ~/bin/repobash123
```

#### 2.2 国内镜像安装方式（推荐）

由于 `storage.googleapis.com` 和 `gerrit.googlesource.com` 在国内无法直接访问，需要用镜像：

```bash
mkdir -p ~/bin

# 从清华大学 TUNA 镜像克隆 git-repo 仓库
git clone https://mirrors.tuna.tsinghua.edu.cn/git/git-repo ~/bin/git-repo

# 创建软链接
ln -sf ~/bin/git-repo/repo ~/bin/repo
chmod a+x ~/bin/repobash12345678
```

#### 2.3 配置环境变量

```bash
# 将以下内容追加到 ~/.bashrc
cat >> ~/.bashrc << 'EOF'

# Android repo 工具
export PATH=~/bin:$PATH

# repo 本体下载镜像（避免从 Google 服务器下载）
export REPO_URL=https://mirrors.tuna.tsinghua.edu.cn/git/git-repo
EOF

# 使配置生效
source ~/.bashrcbash123456789101112
```

#### 2.4 验证安装

```
repo versionbash1
```

正确输出类似：

```python
repo launcher version 2.54
       (from /home/xxx/bin/repo)
git 2.25.1
Python 3.9.51234
```

___

### 三、下载 Android 17 源码

#### 3.1 关于 Android 17 分支的说明

Google 自 2026 年起调整了 AOSP 发布策略：**从每季度一次改为每年两次（Q2 和 Q4）**。

当前（2026年6月）AOSP 上 **尚未有 `android-17` 的正式分支或标签**，最新代码在 `main` 分支上。`main` 分支就是 Android 17 的开发主干，架构和 API 已是 Android 17 级别。

等 Q2 正式推送后，可通过以下命令 切换 到正式分支：

```
repo init -b android-17.0.0_r1
repo syncbash12
```

#### 3.2 初始化仓库

```bash
# 进入工作目录
cd /home/will/lmr/android17

# 初始化（使用 TUNA 镜像）
repo init -u https://mirrors.tuna.tsinghua.edu.cn/git/AOSP/platform/manifest -b mainbash12345
```

初始化成功提示：

```
repo has been initialized in /home/xxx/android171
```

#### 3.3 开始同步源码

```bash
# -c   : 只同步当前分支，减少下载量
# -j4  : 4 个并发下载任务（根据网络和机器性能调整，建议 2~8）
repo sync -c -j4bash123
```

**整个源码约 60~80GB**，取决于网络速度，同步时间从几十分钟到几小时不等。

#### 3.4 查看下载进度

`repo sync` 使用终端进度条实时显示。也可以通过以下命令手动查看：

```bash
# 查看已占用的磁盘空间
du -sh /home/will/lmr/android17/

# 查看已同步的项目数量（每个 .git 目录代表一个子项目）
find /home/will/lmr/android17/.repo/projects -type d -name "*.git" | wc -l

# 查看活跃的下载进程
ps aux | grep -E "git fetch|git-remote" | grep -v grepbash12345678
```

___

### 四、国内镜像源汇总

| 用途 | 镜像地址 |
|---------------|-----------------------------------------------------------------|
| repo 工具 | `https://mirrors.tuna.tsinghua.edu.cn/git/git-repo` |
| AOSP manifest | `https://mirrors.tuna.tsinghua.edu.cn/git/AOSP/platform/manifest` |
| 环境变量 | `REPO_URL=https://mirrors.tuna.tsinghua.edu.cn/git/git-repo` |

> 其他可用镜像：中科大 USTC `https://mirrors.ustc.edu.cn/aosp/`

___

### 五、常见问题与解决方案

#### 5.1 `fatal: Cannot get https://gerrit.googlesource.com/git-repo/clone.bundle`

**原因**: `repo init` 会尝试从 Google Gerrit 下载 repo 本体，国内网络无法访问。

**解决**:

```bash
export REPO_URL=https://mirrors.tuna.tsinghua.edu.cn/git/git-repobash1
```

#### 5.2 `fatal: error [Errno 104] Connection reset by peer`

**原因**: Google 服务器连接被重置。

**解决**: 同样设置 `REPO_URL` 环境变量指向镜像。

#### 5.3 `curl: (35)` SSL/TLS 握手失败

**原因**: 直接 `curl` 下载 Google Storage 上的 repo 工具时网络不通。

**解决**: 使用 `git clone` 镜像方式安装 repo（见 2.2 节）。

#### 5.4 `repo sync` 长时间卡在某个项目不动

**原因**: 某些仓库（如 `platform/build/bazel`、`platform/prebuilts/clang`）非常大（几个 GB），下载需要较长时间。

**解决**:

```bash
# 增加并发数
repo sync -c -j8

# 或跳过某些不需要的仓库（编辑 .repo/manifests/default.xml 注释掉对应 project 行）bash1234
```

#### 5.5 `repo sync` 中途失败 / 网络中断

**解决**: 直接重新运行 `repo sync`，它是**断点续传**的，已下载的部分不会丢失。

```bash
repo sync -c -j4  # 继续下载bash1
```

#### 5.6 Git 内存 不足 / oom-killer

大仓库（如 `prebuilts/clang`）可能消耗大量内存。

**解决**:

```lua
# 限制 Git 内存使用
git config --global core.packedGitLimit 128m
git config --global core.packedGitWindowSize 128m
git config --global pack.deltaCacheSize 128m
git config --global pack.packSizeLimit 128m

# 减少并发
repo sync -c -j1bash12345678
```

#### 5.7 磁盘空间不足

**解决**: 确保目标磁盘有至少 **150GB** 空闲空间。

```bash
df -h /home/will/lmr/android17/  # 查看可用空间bash1
```

#### 5.8 想切换到正式 Android 17 分支

当 Google 推送 `android-17.0.0_r1` 后：

```bash
cd /home/will/lmr/android17
repo init -b android-17.0.0_r1
repo sync -c -j4bash123
```

___

### 六、下载完成后的目录结构

同步完成后，目录结构大致如下：

```perl
android17/
├── art/                    # Android Runtime (ART 虚拟机)
├── bionic/                 # C 标准库 (libc, libm, linker)
├── bootable/               # bootloader / recovery
├── build/                  # 构建系统 (soong, blueprint, bazel)
├── cts/                    # Compatibility Test Suite
├── dalvik/                 # Dalvik 虚拟机（遗留）
├── developers/             # 开发者工具
├── development/            # 开发辅助工具
├── device/                 # 设备配置
├── external/               # 第三方开源库
├── frameworks/             # Android 核心框架 (base, native, av)
├── hardware/               # HAL 硬件抽象层
├── kernel/                 # Linux 内核
├── packages/               # 系统应用
├── prebuilts/              # 预编译工具链
├── sdk/                    # SDK 工具
├── system/                 # 系统核心组件 (init, core, sepolicy)
├── tools/                  # 开发工具
├── vendor/                 # 厂商相关
└── .repo/                  # repo 元数据（不要手动修改）123456789101112131415161718192021
```

___

### 七、系统要求总结

| 要求 | 最低配置 | 推荐配置 |
|--------|--------------|----------------------|
| **操作系统** | Ubuntu 20.04 | Ubuntu 22.04 / 24.04 |
| **磁盘空间** | 150GB | 250GB+ |
| **内存** | 8GB | 16GB+ |
| **CPU** | 4 核 | 8 核+ |
| **网络** | 稳定宽带 | 100Mbps+ |
| **Python** | 3.8+ | 3.9+ |
| **Git** | 2.20+ | 2.30+ |

___

### 八、速查命令

```bash
# === 安装 repo ===
git clone https://mirrors.tuna.tsinghua.edu.cn/git/git-repo ~/bin/git-repo
ln -sf ~/bin/git-repo/repo ~/bin/repo
echo 'export PATH=~/bin:$PATH' >> ~/.bashrc
echo 'export REPO_URL=https://mirrors.tuna.tsinghua.edu.cn/git/git-repo' >> ~/.bashrc
source ~/.bashrc

# === 下载源码 ===
cd /home/will/lmr/android17
repo init -u https://mirrors.tuna.tsinghua.edu.cn/git/AOSP/platform/manifest -b main
repo sync -c -j4

# === 查看进度 ===
du -sh .                                          # 磁盘占用
find .repo/projects -type d -name "*.git" | wc -l # 项目数

# === 中途恢复 ===
repo sync -c -j4   # 断点续传，直接重新运行即可bash123456789101112131415161718
```