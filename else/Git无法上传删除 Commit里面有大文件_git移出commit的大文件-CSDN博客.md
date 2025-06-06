---
created: 2025-06-06T08:52:58 (UTC +08:00)
tags: [git移出commit的大文件]
source: https://blog.csdn.net/LoveFHM/article/details/131563696
author: 成就一亿技术人!
---
1.Bug描述

因为在一次提交中不小心把一个打包的aab文件弄到commit文件里了，于是在上传的时候push rejected 了。

![](https://i-blog.csdnimg.cn/blog_migrate/272b87aec9231c34f73285729a654888.png)

 因为GitHub的文件上限是100M，但是打的包太大了，有170M，所以是不能上传的，但是又是已经在Commit历史中了，并且已经好几次了，于是就想在历史中删除掉。

2.解决办法：

删除commit记录里的 app/release/app-release.aab

1.要删除 Git 历史记录中包含 `app/release/app-release.aab` 的 commit，您可以使用 Git 的 `filter-branch` 命令进行重写历史。请注意，这种操作会修改历史记录，因此在执行此操作之前，请确保您理解其潜在风险，并在执行操作之前备份重要数据。

2.首先，确保您在正确的 Git 仓库目录中。打开命令行或终端，并导航到该目录。

3.运行以下命令，使用 `filter-branch` 重写历史：

```bash
git filter-branch --force --index-filter "git rm --cached --ignore-unmatch app/release/app-release.aab" --prune-empty --tag-name-filter cat -- --all
```

（注意上面的文件名哦）

4.此命令会遍历整个 Git 历史，并删除包含 `app/release/app-release.aab` 的 commit。`--index-filter` 参数使用 `git rm --cached --ignore-unmatch` 命令将该文件从每个 commit 中移除。

[命令执行](https://so.csdn.net/so/search?q=%E5%91%BD%E4%BB%A4%E6%89%A7%E8%A1%8C&spm=1001.2101.3001.7020)完后，Git 会重写历史记录。**请注意，这可能需要一些时间，具体取决于您的项目历史的大小。**

![](https://i-blog.csdnimg.cn/blog_migrate/4439790582d1fb34581c7d1ff36207de.png)

 如果报错了：

![](https://i-blog.csdnimg.cn/blog_migrate/a95effc680dd29659f98758e8446e016.png)

这个是因为你本地的还有更改，你继续放到commit里面 就行了，如果还有其他的报错，根据提示修改就是了。

5.完成后，您可以运行以下命令清除 Git 的垃圾数据：

```bash
git for-each-ref --format="delete %(refname)" refs/original | git update-ref --stdin
```

这个运行命令就行了

6.上传代码

> `git push --force origin`

如果报错，根据提示去解决。

![](https://i-blog.csdnimg.cn/blog_migrate/470714d1230db4b33c8466f0773e223c.png)