---
created: 2025-06-06T08:52:58 (UTC +08:00)
tags: []
source: 
author: 成就一亿技术人!
---


栏目: [编程语言](https://www.yisu.com/ask/bcjs/)

开发者测试专用服务器限时活动，0元免费领，库存有限，领完即止！ 点击查看>>

要删除一个commit中的文件，可以使用以下命令：

1. 首先找到要删除文件的commit的哈希值，可以通过以下命令查看commit历史记录：

```
git log
```

复制代码

2. 找到要删除文件的commit的哈希值后，使用以下命令将该文件从commit中移除：

```
git rebase -i <commit的哈希值>~1
```

复制代码

3. 在弹出的文本编辑器中，找到要删除文件的commit，并将其前面的单词改为`edit`。
    
4. 保存并关闭编辑器，Git会自动进入rebase操作，在rebase操作中，使用以下命令删除文件：
    

```
git rm <要删除的文件>
```

复制代码

5. 完成删除后，继续rebase操作，使用以下命令保存并退出：

```
git rebase --continue
```

复制代码

6. 提交更改：

```
git commit --amend
```

复制代码

7. 最后将更改推送到远程仓库：

```
git push origin <分支名> --force
```

复制代码

这样就成功删除了指定commit中的文件。请注意，使用`--force`选项可能会导致远程仓库的历史被改变，谨慎操作。