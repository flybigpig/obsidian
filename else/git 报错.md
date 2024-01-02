
## git pull 报错：fatal: Exiting because of unfinished merge.

### 报错内容：

```makefile
error: You have not concluded your merge (MERGE_HEAD exists).
hint: Please, commit your changes before merging.
fatal: Exiting because of unfinished merge.
```

### 解决方法（放弃本地修改）：

```bash
git reset --hard origin/master
```