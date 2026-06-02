                Android Framework 开发必备技能

## 一、Gerrit 是什么？

### 1.1 定义

Gerrit 是 Google 开发的一款基于 Web 的代码审查工具，专门为 Git 设计。它是 Android 开源项目（AOSP）的官方代码审查平台，也被众多企业用于管理大型代码库。

### 1.2 核心理念

📦 传统 Git 工作流：

| 👨💻 开发者 | → git push → | ✅ 直接合入主分支 |
|-----------|--------------|-----------|

🔍 Gerrit 工作流：

| 👨💻 开发者 | → | 📋 Gerrit 暂存区 | → | 🔍 代码审查 | → | ✅ 合入主分支 |
|-----------|-----|---------------|-----|---------|-----|---------|

|  | ↓ 审查不通过 |  |
|-----|---------------|-----|
|  | ❌ 返回修改 → 重新提交 |  |

💡 **核心思想**：每一个提交在合入主分支前，都必须经过至少一人的代码审查。

### 1.3 为什么 Android 开发需要 Gerrit？

| 原因 | 说明 |
|--------|-------------------------|
| 代码质量保证 | 每个修改都经过审查，减少 bug |
| 知识共享 | 审查过程中团队成员互相学习 |
| 责任追溯 | 每个提交都有审查记录 |
| 防止误操作 | 避免未经检验的代码直接进入主分支 |
| 符合大厂规范 | Google、华为、小米等都使用 Gerrit |

## 二、Gerrit 核心概念详解

### 2.1 Change（变更）

Change 是 Gerrit 中最核心的概念，代表一个待审查的代码修改。

📦 一个 Change 包含：

| Change-Id | 唯一标识符（以 I 开头的40位字符） |
|-------------|-------------------------------|
| Subject | 提交标题 |
| Description | 详细描述 |
| Owner | 提交者 |
| Patchsets | 修改版本集合（可能有多个版本） |
| Labels | 审查评分（Code-Review, Verified 等） |
| Comments | 审查意见 |

🔑 Change-Id 示例：

Change-Id: I1234567890abcdef1234567890abcdef12345678

⚠️ **重要**：Change-Id 是 Gerrit 追踪修改的关键。即使你多次修改代码重新提交，只要 Change-Id 相同，Gerrit 就知道这是对同一个 Change 的更新。

### 2.2 Patchset（补丁集）

一个 Change 可以有多个 Patchset，代表同一个修改的不同版本。

| 📋 Change #12345 |
|-----------------------------------------------------------------------------------------------------------------------------------------|
|  |
| Patchset 1 - 首次提交

💬 审查意见："请修复变量命名"

▼

Patchset 2 - 根据意见修改后重新提交

💬 审查意见："还需要添加单元测试"

▼

Patchset 3 - 添加测试后重新提交

✅ 审查通过！

▼

🎉 合入主分支 |

💻 工作流程：

\# 首次提交 -> 创建 Patchset 1  
git commit -m "Add new feature"  
git push origin HEAD:refs/for/main  
  
\# 收到审查意见后修改代码  
\# 使用 --amend 保持相同的 Change-Id -> 创建 Patchset 2  
git add .  
git commit --amend  
git push origin HEAD:refs/for/main

### 2.3 Label（标签/评分）

Gerrit 使用 Label 来表示审查状态，常见的 Label 有：

📝 Code-Review（代码审查）

| 分数 | 含义 | 说明 |
|-----|---------------|----------------|
| +2 | Approved | 批准合入，代码完全没问题 |
| +1 | Looks good | 看起来不错，但我没有合入权限 |
| 0 | No score | 没有评分 |
| -1 | Needs work | 需要修改 |
| -2 | Do not submit | 绝对不能合入（一票否决） |

✅ Verified（验证）

| 分数 | 含义 | 说明 |
|-----|----------|-----------|
| +1 | Verified | 编译通过，测试通过 |
| 0 | No score | 未验证 |
| -1 | Fails | 编译失败或测试失败 |

🎯 合入条件（典型配置）：

✅ Code-Review: 至少一个 +2  
✅ Verified: 至少一个 +1  
✅ 没有任何 -2  
✅ 没有未解决的评论

### 2.4 refs/for 和 refs/changes

这是 Gerrit 的特殊 Git 引用：

\# refs/for/分支名 - 用于提交代码到审查  
git push origin HEAD:refs/for/main  
git push origin HEAD:refs/for/android-14  
  
\# refs/changes/XX/YYYY/Z - Gerrit 内部存储  
\# XX: Change 编号的后两位  
\# YYYY: Change 编号  
\# Z: Patchset 编号  
\# 例如：refs/changes/45/12345/3 表示 Change 12345 的 Patchset 3  
  
\# 下载特定的 Change 进行本地测试  
git fetch origin refs/changes/45/12345/3  
git checkout FETCH\_HEAD

## 三、Gerrit 安装与配置

### 3.1 服务器端安装（管理员操作）

\# 1. 下载 Gerrit WAR 文件  
wget https://gerrit-releases.storage.googleapis.com/gerrit-3.9.1.war  
  
\# 2. 初始化 Gerrit 站点  
java -jar gerrit-3.9.1.war init -d /opt/gerrit  
  
\# 3. 启动 Gerrit  
/opt/gerrit/bin/gerrit.sh start  
  
\# 4. 访问 Web 界面  
\# http://your-server:8080

### 3.2 客户端配置（开发者操作）

1️⃣ 配置 Git 用户信息

\# 必须与 Gerrit 账户邮箱一致  
git config --global user.name "你的名字"  
git config --global user.email "your.email@company.com"

2️⃣ 生成并配置 SSH 密钥

\# 生成 SSH 密钥  
ssh-keygen -t ed25519 -C "your.email@company.com"  
  
\# 查看公钥（添加到 Gerrit）  
cat ~/.ssh/id\_ed25519.pub  
\# 复制输出内容粘贴到 Gerrit Web 界面  
\# Settings -> SSH Keys -> Add Key

3️⃣ 配置 SSH 别名（可选）

cat >> ~/.ssh/config << 'EOF'  
Host gerrit  
    HostName gerrit.mycompany.com  
    Port 29418  
    User your\_username  
    IdentityFile ~/.ssh/id\_ed25519  
EOF

4️⃣ 测试 SSH 连接

ssh -p 29418 your\_username@gerrit.mycompany.com  
  
\# 成功输出：  
\*\*\*\* Welcome to Gerrit Code Review \*\*\*\*  
Hi your\_username, you have successfully connected over SSH.

5️⃣ 安装 commit-msg hook（自动添加 Change-Id）

\# 方法一：使用 scp  
scp -p -P 29418 your\_username@gerrit.mycompany.com:hooks/commit-msg \\  
    .git/hooks/  
  
\# 方法二：使用 curl  
curl -Lo .git/hooks/commit-msg \\  
    http://gerrit.mycompany.com/tools/hooks/commit-msg  
chmod +x .git/hooks/commit-msg

## 四、Gerrit 日常使用详解

### 4.1 提交代码到审查

📋 标准流程：

| Step 1-2：确保代码最新

git fetch origin<br>git rebase origin/main |
|-------------------------------------------------------------------|
| ▼ |
| Step 3-4：创建分支并修改代码

git checkout -b feature-xxx<br># ... 编辑文件 ... |
| ▼ |
| Step 5-6：暂存并提交

git add -A<br>git commit |
| ▼ |
| Step 7：推送到 Gerrit 审查

git push origin HEAD:refs/for/main |

📝 Commit Message 示例：

SystemUI: Fix status bar icon alignment  
  
The status bar icons were misaligned on devices with  
notch displays. This change adjusts the padding calculation  
to account for the display cutout.  
  
Bug: 123456  
Test: Manual test on Pixel 6, verified icons are centered

👥 指定审查者：

\# 推送时指定审查者  
git push origin HEAD:refs/for/main%r=reviewer1@company.com,r=reviewer2@company.com  
  
\# 设置为 Work In Progress（草稿状态）  
git push origin HEAD:refs/for/main%wip  
  
\# 设置为 Private  
git push origin HEAD:refs/for/main%private

### 4.2 修改已提交的 Change

💡 当审查者提出修改意见后

\# 1. 确保在正确的提交上  
git log -1  \# 查看当前提交，确认 Change-Id  
  
\# 2. 修改代码  
\# ... 编辑文件 ...  
  
\# 3. 追加到原提交（关键：使用 --amend）  
git add -A  
git commit \--amend  
  
\# 4. 重新推送（会创建新的 Patchset）  
git push origin HEAD:refs/for/main  
  
\# Gerrit 会根据 Change-Id 识别这是对同一个 Change 的更新

### 4.3 下载他人的 Change

\# 方法1：使用 git-review  
git review -d 12345  
  
\# 方法2：直接 fetch  
git fetch origin refs/changes/45/12345/3 && git checkout FETCH\_HEAD  
  
\# 方法3：使用 repo（Android 开发）  
repo download myproject 12345/3

### 4.4 Rebase 和解决冲突

⚠️ 当 Gerrit 显示 "Merge Conflict" 时

\# 1. 获取最新代码  
git fetch origin  
  
\# 2. Rebase 到最新  
git rebase origin/main  
  
\# 3. 如果有冲突，解决冲突文件  
\# 编辑 <<<<<<< ======= >>>>>>> 标记  
  
\# 4. 标记冲突已解决  
git add 冲突文件  
  
\# 5. 继续 rebase  
git rebase --continue  
  
\# 6. 重新推送  
git push origin HEAD:refs/for/main

## 五、Gerrit Web 界面操作

### 5.1 界面概览

| 顶部导航栏  [CHANGES] [YOUR] [OPEN] [MERGED] [ABANDONED] [搜索框] |
|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Change #12345

**Subject:** SystemUI: Fix status bar icon alignment

**Owner:** developer@company.com

**Project:** platform/frameworks/base

**Branch:** main

Code-Review: +1 (reviewer1)

   Verified: +1 (Jenkins)

Reply

 Submit Abandon |

### 5.2 常用搜索语法

| 搜索语法 | 说明 |
|----------------------------------|--------------------------|
| owner:self status:open | 搜索你的待审查提交 |
| reviewer:self status:open | 搜索分配给你审查的提交 |
| project:platform/frameworks/base | 搜索特定项目的提交 |
| branch:android-14 | 搜索特定分支 |
| status:merged owner:self | 搜索已合入的提交 |
| file:StatusBar.java | 搜索包含特定文件的提交 |
| message:"fix crash" | 搜索 commit message 包含特定内容 |

### 5.3 代码审查操作

📝 添加评论方式：

| 行内评论 | 点击代码行号，输入评论 |
|-------|----------------|
| 文件级评论 | 点击文件名旁的评论图标 |
| 总体评论 | 在 Reply 对话框中输入 |

### 5.4 合入代码

🎯 合入条件检查：

✅ Code-Review: +2 (至少一个)  
✅ Verified: +1 (CI 通过)  
✅ 无 -2 评分  
✅ 无未解决的评论  
✅ 无合并冲突  
  
满足条件后，\[Submit\] 按钮变为可点击状态

## 六、Gerrit 与 CI/CD 集成

### 6.1 Jenkins 集成

📋 Jenkins Pipeline 示例（简化版）：

pipeline {  
agentany  
  
triggers{  
    gerrit(  
      triggerOnEvents: \[patchsetCreated()\],  
      serverName:'MyGerrit'  
    )  
  }  
  
stages{  
    stage('Build') { steps { sh'make -j$(nproc)'} }  
    stage('Test') { steps { sh'./run\_tests.sh'} }  
  }  
  
post{  
    success{ gerritReview labels: \[Verified: 1\] }  
    failure{ gerritReview labels: \[Verified: -1\] }  
  }  
}

### 6.2 SSH 命令给 Gerrit 评分

\# 构建成功后给 +1 Verified  
ssh -p 29418 jenkins@gerrit.mycompany.com \\  
    gerrit review $GERRIT\_CHANGE\_NUMBER,$GERRIT\_PATCHSET\_NUMBER \\  
    --verified+1\\  
    --message'"Build succeeded"'  
  
\# 构建失败后给 -1 Verified  
ssh -p 29418 jenkins@gerrit.mycompany.com \\  
    gerrit review $GERRIT\_CHANGE\_NUMBER,$GERRIT\_PATCHSET\_NUMBER \\  
    --verified\-1\\  
    --message '"Build failed"'

## 七、Gerrit 高级功能

### 7.1 Topic（主题）

Topic 用于关联多个相关的 Change：

\# 推送时指定 Topic  
git push origin HEAD:refs/for/main%topic=feature-dark-mode

💡 同一 Topic 的所有 Change 可以一起显示、一起提交、方便跟踪相关修改

### 7.2 Hashtag（标签）

\# 推送时添加 Hashtag  
git push origin HEAD:refs/for/main%hashtag=urgent,hashtag=security-fix  
  
\# 在 Web 界面可以通过 Hashtag 搜索  
\# hashtag:security-fix

### 7.3 依赖关系

| Change A (父提交) |
|-----------------|
| ↓ |
| Change B (基于 A) |
| ↓ |
| Change C (基于 B) |

当 A 未合入时，B 和 C 显示为 "Depends on"  
A 合入后，B 变为可提交状态

### 7.4 Cherry-pick & Revert

\# Cherry-pick 到其他分支  
ssh -p 29418 gerrit.mycompany.com \\  
    gerrit cherry-pick 12345,1 android-13  
  
\# Revert 已合入的 Change  
\# 在 Web 界面点击 "Revert"  
\# 这会创建一个新的逆向 Change

## 八、Gerrit 权限管理

### 8.1 权限模型

| 🌐 全局权限 (All-Projects)

适用于所有项目的默认权限 |
|--------------------------------------|
| ↓ 继承 |
| 📂 父项目权限

可被子项目继承 |
| ↓ 继承 |
| 📁 项目权限

特定项目的权限设置 |

### 8.2 常见用户组

| 用户组 | 权限说明 |
|----------------|----------------------------|
| Administrators | 系统管理员，拥有所有权限 |
| Project-Leads | 项目负责人，可以合入代码 |
| Core-Reviewers | 核心审查者，可以 +2 Code-Review |
| Developers | 开发者，可以提交代码和 +1 Code-Review |
| CI-Bots | CI 机器人账户，可以 Verified |

## 九、Gerrit 最佳实践

### 9.1 提交规范

| ✅ 正确做法 | ✅ 正确做法 |
|--------|----------------------------------------|
| • | 每个 Change 只做一件事 |
| • | commit message 清晰描述修改目的 |
| • | 提交前本地测试通过 |
| • | 小步提交，方便审查 |
| ❌ 错误做法 | ❌ 错误做法 |
| • | 一个 Change 包含多个不相关的修改 |
| • | commit message 只写 "fix bug" 或 "update" |
| • | 提交未经测试的代码 |
| • | 积攒很多修改一次提交 |

### 9.2 审查规范

| 🔍 作为审查者 | 🔍 作为审查者 |
|-------------|----------------------|
| • | 及时审查，不要让 Change 等待太久 |
| • | 提供建设性的意见，不只是说"不好" |
| • | 区分"必须修改"和"建议修改" |
| 👨💻 作为提交者 | 👨💻 作为提交者 |
| • | 认真对待审查意见 |
| • | 及时回复评论 |
| • | 解释你的设计决策 |

## 十、常见问题与解决方案

❌ **问题**：git push 失败 - "missing Change-Id in commit message footer"

✅ **解决**：安装 commit-msg hook

scp -p -P 29418 username@gerrit:hooks/commit-msg .git/hooks/  
git commit --amend  
git push origin HEAD:refs/for/main

❌ **问题**：same Change-Id in multiple changes

✅ **解决**：生成新的 Change-Id

git commit --amend  
\# 在编辑器中删除 Change-Id 行  
\# 保存后 hook 会生成新的 Change-Id

❌ **问题**：Merge Conflict

✅ **解决**：Rebase 到最新

git fetch origin  
git rebase origin/main  
\# 解决冲突后  
git add .  
git rebase --continue  
git push origin HEAD:refs/for/main

❌ **问题**：审查通过但无法提交

✅ **可能原因**：

| 1. | 缺少 Verified +1（需要 CI 验证） |
|-----|--------------------------|
| 2. | 有未解决的评论 |
| 3. | 有依赖的 Change 未合入 |
| 4. | 你没有 Submit 权限 |

## 十一、Gerrit 命令速查表

### SSH 命令

| 命令 | 说明 |
|--------------------------------------|-----------|
| ssh -p 29418 user@gerrit | 连接测试 |
| gerrit query status:open owner:self | 查询 Change |
| gerrit review 12345 --code-review +2 | 审查 Change |
| gerrit review 12345 --abandon | 放弃 Change |

### Git 命令

| 命令 | 说明 |
|-------------------------------------------|-----------|
| git push origin HEAD:refs/for/main | 推送到审查 |
| git push origin HEAD:refs/for/main%r=user | 推送并指定审查者 |
| git push origin HEAD:refs/for/main%wip | 推送为草稿 |
| git fetch origin refs/changes/45/12345/3 | 下载 Change |
| git commit --amend | 修改并重新提交 |

💡 新手建议  
  
1️⃣ **先熟悉基本流程**：提交 → 审查 → 修改 → 合入  
2️⃣ **养成好习惯**：小步提交、清晰的 commit message  
3️⃣ **善用 Web 界面**：大部分操作都可以在 Web 界面完成  
4️⃣ **遇到问题先搜索**：Gerrit 有完善的文档和社区支持

— 掌握 Gerrit，更好地参与 Android 开源项目协作开发！ —