## Ubuntu20.04换源教程：

### 1.打开终端

### 2.备份原有的[软件源](https://so.csdn.net/so/search?q=%E8%BD%AF%E4%BB%B6%E6%BA%90&spm=1001.2101.3001.7020)列表文件：

```
sudo cp /etc/apt/sources.list /etc/apt/sources.list.bak
```

### 3.打开软件源列表文件进行编辑：

```
sudo gedit /etc/apt/sources.list
```

### 4.更换新的软件源配置信息(eg:阿里云源，建议选择离您所在地区比较近的源)：

```
deb http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiversedeb-src http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiversedeb http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiversedeb-src http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiversedeb http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiversedeb-src http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiversedeb http://mirrors.aliyun.com/ubuntu/ focal-proposed main restricted universe multiversedeb-src http://mirrors.aliyun.com/ubuntu/ focal-proposed main restricted universe multiversedeb http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiversedeb-src http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse
```

### 5.更新[软件包](https://so.csdn.net/so/search?q=%E8%BD%AF%E4%BB%B6%E5%8C%85&spm=1001.2101.3001.7020)列表：

从配置的软件源中获取最新的软件包信息，并用于确定哪些软件包有可用的更新。但是，它不会实际安装任何软件包更新，只是更新本地软件包列表。

```
sudo apt update
```

### 7.升级软件包：

安装可用的软件包更新。它会检查本地软件包列表和远程软件源的最新软件包版本，并安装那些有新版本的软件包。它会升级已安装软件包的版本，并处理[依赖关系](https://so.csdn.net/so/search?q=%E4%BE%9D%E8%B5%96%E5%85%B3%E7%B3%BB&spm=1001.2101.3001.7020)。

```
sudo apt upgrade
```

PS：以下是常用的中国国内 Ubuntu 20.04 源配置信息：

1.中科大源：

```
deb https://mirrors.ustc.edu.cn/ubuntu/ focal main restricted universe multiversedeb-src https://mirrors.ustc.edu.cn/ubuntu/ focal main restricted universe multiversedeb https://mirrors.ustc.edu.cn/ubuntu/ focal-updates main restricted universe multiversedeb-src https://mirrors.ustc.edu.cn/ubuntu/ focal-updates main restricted universe multiversedeb https://mirrors.ustc.edu.cn/ubuntu/ focal-backports main restricted universe multiversedeb-src https://mirrors.ustc.edu.cn/ubuntu/ focal-backports main restricted universe multiversedeb https://mirrors.ustc.edu.cn/ubuntu/ focal-security main restricted universe multiversedeb-src https://mirrors.ustc.edu.cn/ubuntu/ focal-security main restricted universe multiversedeb https://mirrors.ustc.edu.cn/ubuntu/ focal-proposed main restricted universe multiversedeb-src https://mirrors.ustc.edu.cn/ubuntu/ focal-proposed main restricted universe multiverse
```

2.清华源：

```
deb http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal main restricted universe multiversedeb-src http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal main restricted universe multiversedeb http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-updates main restricted universe multiversedeb-src http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-updates main restricted universe multiversedeb http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-backports main restricted universe multiversedeb-src http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-backports main restricted universe multiversedeb http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-security main restricted universe multiversedeb-src http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-security main restricted universe multiversedeb http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-proposed main restricted universe multiversedeb-src http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-proposed main restricted universe multiverse
```

3.网易源：

```
# 网易源
deb http://mirrors.163.com/ubuntu/ focal main restricted universe multiversedeb http://mirrors.163.com/ubuntu/ focal-security main restricted universe multiversedeb http://mirrors.163.com/ubuntu/ focal-updates main restricted universe multiversedeb http://mirrors.163.com/ubuntu/ focal-backports main restricted universe multiversedeb-src http://mirrors.163.com/ubuntu/ focal main restricted universe multiversedeb-src http://mirrors.163.com/ubuntu/ focal-security main restricted universe multiversedeb-src http://mirrors.163.com/ubuntu/ focal-updates main restricted universe multiversedeb-src http://mirrors.163.com/ubuntu/ focal-backports main restricted universe multiverse
```

## 解决主机与虚拟机之间进行文本复制粘贴问题 ：

### 1.打开终端

### 2.安装的 Open-VM-Tools 桌面组件：

Open-VM-Tools 桌面组件提供了剪贴板共享功能，可以让您在主机和虚拟机之间复制和粘贴文本内容。

```
sudo apt-get install open-vm-tools-desktop -y
```

### 3.安装完成后，重启虚拟机以应用更改：

```
reboot
```