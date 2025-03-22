![](https://csdnimg.cn/release/blogv2/dist/pc/img/original.png)

[humanGetup](https://blog.csdn.net/qq_75158598 "humanGetup") ![](https://csdnimg.cn/release/blogv2/dist/pc/img/newUpTime2.png) 已于 2025-02-24 11:22:56 修改

于 2025-02-10 16:57:57 首次发布

版权声明：本文为博主原创文章，遵循 [CC 4.0 BY-SA](http://creativecommons.org/licenses/by-sa/4.0/) 版权协议，转载请附上原文出处链接和本声明。

___

## 前言

为了方便对虚拟机进行文件管理，程序员通常会通过[Samba](https://so.csdn.net/so/search?q=Samba&spm=1001.2101.3001.7020)配置将虚拟机内存映射到主机盘符

[0voice · GitHub](https://github.com/0voice "0voice · GitHub")

## 一、确保[虚拟机安装](https://so.csdn.net/so/search?q=%E8%99%9A%E6%8B%9F%E6%9C%BA%E5%AE%89%E8%A3%85&spm=1001.2101.3001.7020)了samba和vim

##  1.安装samba的指令：

```
sudo apt install samba
```

## 2.[安装vim](https://so.csdn.net/so/search?q=%E5%AE%89%E8%A3%85vim&spm=1001.2101.3001.7020)的指令：

```
sudo apt install vim
```

## 3.如果碰到下面的情况

```
Do you want to continue? [Y/n] Y
```

一律输入Y即可

## 二、操作步骤

### 1.查看当前目录

输入：

```
ls /etc/samba
```

输出：

```
dhcp.conf  gdbcommands  smb.conf  tls
```

### 2.用vim修改文件

#### (1)修改什么？

```
sudo vim /etc/samba/smb.conf
```

输入这个指令后课进入文件中，下移光标至文件底部并输入以下内容：

（注意：path是文件路径，home表示虚拟机本机，human0sheng是我的虚拟机用户名，读者应输入自己的相应用户名，share是等会要创建的文件）

```
[Share] comment = My Samba path = /home/human0sheng/share browseable = yes writeable = yes
```

![](https://i-blog.csdnimg.cn/direct/86530a49045144a5b7a8a80cf1b9c2d6.png)

上图为输入后的示例图。

#### (2)如何用vim输入

按i进入输入状态,可以看到如下图的标识

![](https://i-blog.csdnimg.cn/direct/e77070011255491d92b3f619abe2dbad.png)

然后就可以正常输入了。

输入完成后，按Esc键，会退出输入模式，标识消失。输入:wq，再按一下Enter（回车键），就会保存退出。这里的`:`是帮助`vim`进入命令模式，`w`是保存，相当于记事本里边快捷键Ctrl+s，`q`就是关闭`vim`的命令。

### 3.创建share文件并创建一个 Samba 用户

#### (1)首先输入：

```
mkdir share
```

这个命令的作用是创建新文件夹share

#### (2)接着输入：

```
sudo chmod 777 share/ -R
```

这个命令的作用是递归地更改 `share/` 目录及其内部所有文件和子目录的权限。执行该命令后，`share/` 目录及其所有子文件和子目录的权限将被设置为：

-   **所有者**：读、写、执行权限。
-   **同组用户**：读、写、执行权限。
-   **其他用户**：读、写、执行权限。

这意味着任何人都可以对该目录及其文件进行读取、修改和执行操作。

#### （3)接着输入:

```
sudo smbpasswd -a human0sheng
```

添加一个新的 Samba 用户 `human0sheng`，并为其设置密码。此命令会提示用户输入新密码并确认密码：

![](https://i-blog.csdnimg.cn/direct/13cfafe0e8494756a32a05fdb2fe7e68.png)

输入两次密码即可

#### (4)输入输出完整的反馈如下图：

![](https://i-blog.csdnimg.cn/direct/a756b74d917040a48c3fcc48c5735eab.png)

### 4.创建共享的映射盘符

#### (1)在文件资源管理器中输入\\\\+虚拟机主机号![](https://i-blog.csdnimg.cn/direct/d957c85cd8f34b4dbd6c42f4d9648ada.png)

输入后进入这个界面，输入前面注册的Samba用户名密码：

![](https://i-blog.csdnimg.cn/direct/fcc61495e64d4cbd9c7084f450fea04b.png)

#### (2)点击映射网络驱动器

![](https://i-blog.csdnimg.cn/direct/904d46c7901840988ce8e16cfa3cebee.jpeg)

#### （3）点击确定即可

![](https://i-blog.csdnimg.cn/direct/e0bc60eb8dfc43fba7eef53e0193ca33.png)

___

## 总结

这篇文章给出了完整的操作步骤，快去尝试吧！