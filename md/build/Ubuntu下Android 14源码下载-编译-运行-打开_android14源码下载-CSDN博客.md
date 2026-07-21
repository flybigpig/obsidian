> 想让Android Studio更易用，推荐阅读：[Android Studio 安装-配置-优化](https://blog.csdn.net/xunyan6234/article/details/132189026?spm=1001.2014.3001.5501)

## 1\. 背景

安卓14源码在线网站：[http://aospxref.com/android-14.0.0\_r2/](http://aospxref.com/android-14.0.0_r2/)

直接查看源码有很多不方便，比如国内、不能跳转、无法调试。Android Studio有很多实用功能，直接下载源码可以更方便的查看和调试，更加人性化。本文以Android14为例，以源码下载-编译-运行-打开顺序逐步讲解，主要是用于记录步骤和遇到问题的解决，建议先通读，再按需尝试。

## 2\. 环境

> 如果遇到在过程中遇到java或者python版本不对时，可以使用命令切换到需要的版本（前提是已经有几个版本可以换）：  
> sudo update-alternatives --config python或者sudo update-alternatives --config java  
> 如果是Ubuntu虚拟机，那么虚拟机的内存应该小于window，比如window32G，给Ubuntu24G

| 设备 | Ubuntu 22.04.3 LTS |
|--------|----------------------|
| 内存 | 32.0 GiB |
| 磁盘 | 2.0 TB |
| java | 开始是java8，后来换成了java17 |
| git | 2.34.1 |
| Python | 3.10.12 |
| Python | v2.40 (后来安装的) |

### 2.1 安装repo

#### 2.1.1 repo安装方法

```bash
mkdir ~/bin   # 文件夹可能本身就存在，可以cd看看
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc  # 立即生效
curl -sSL  'https://gerrit-googlesource.proxy.ustclug.org/git-repo/+/master/repo?format=TEXT' |base64 -d > ~/bin/repo
chmod a+x ~/bin/reposhell12345
```

> 注意:默认的 repo 使用的地址是 REPO\_URL = ‘https://gerrit.googlesource.com/git-repo’ ，即从谷歌下载更新repo，这里我们需要修改 REPO\_URL，否则会出现无法下载的情况。博主这里使用的是 方法2 。如果你在北方，可以优先考虑清华源；如果你在南方，中科大源可能会给你更好的体验。

-   **方法1**：在你的 rc 文件里面，加入一条配置即可：REPO\_URL=”https://gerrit-googlesource.proxy.ustclug.org/git-repo"
-   **方法2**：直接打开sudo gedit ~/bin/repo, 把 REPO\_URL`REPO_URL = 'https://gerrit.googlesource.com/git-repo'` 一行替换成下面的： `REPO_URL = 'https://gerrit-googlesource.proxy.ustclug.org/git-repo'`是中科大的镜像源，国内访问速度快。）  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/4a92b30e6cdca9afb59246ad1d7a0f03.png)
-   **方法3**：替换源（注意需要先将源备份），可参考[AOSP linux环境配置及其编译方法](https://blog.csdn.net/yimelancholy/article/details/130462470?csdn_share_tail=%7B%22type%22:%22blog%22,%22rType%22:%22article%22,%22rId%22:%22130462470%22,%22source%22:%22yimelancholy%22%7D#t4)

#### 2.1.2 repo验证方法

输入`repo --vertion`，若如图所示，则说明安装成功  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/09952c4a626632892ca940078738d2e3.png)

-   若报错如下：

```ruby
File "/home/xxx/bin/repo", line 51
    def print(self, *args, **kwargs):
            ^
SyntaxError: invalid syntaxshell1234
```

则运行如下指令，参考[repo引导脚本报错问题SyntaxError: invalid syntax](https://blog.csdn.net/alang_/article/details/134686462)：

```bash
curl -sSL  'https://gerrit-googlesource.proxy.ustclug.org/git-repo/+/master/repo?format=TEXT' |base64 -d > ~/bin/reposhell1
```

-   若报错如下：

```python
repo --version
/usr/bin/env: ‘python’: No such file or directoryshell12
```

则将 python 软链接

```bash
# 查看 Python3 版本
python3 --version
# 创建软链接，将 python 指向 python3
sudo ln -s /usr/bin/python3 /usr/bin/python
# 验证
python --versionshell123456
```

### 2.2 依赖安装

[建立构建环境](https://source.android.google.cn/docs/setup/build/initializing?hl=zh-cn)  
安装所需的程序包（Ubuntu的18.04及以上）

```css
sudo apt-get install git-core gnupg flex bison build-essential zip curl zlib1g-dev gcc-multilib g++-multilib libc6-dev-i386 libncurses5 lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z1-dev libgl1-mesa-dev libxml2-utils xsltproc unzip fontconfigshell1
```

## 3\. Android14 源码下载

[源代码标记和 build](https://source.android.google.cn/docs/setup/about/build-numbers?hl=zh-cn) — 虽然不可见，但是能下载

> android-14.0.0\_r1：Android 14 的初始版本，发布于 2023 年 10 月 4 日  
> android-14.0.0\_r2：Android 14 的第一个月度安全更新，发布于 2023 年 11 月 1 日  
> ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5c268dfcb425f2e488384ae115b64abd.png)

```bash
mkdir code/android14    # 创建一个目录来装源码
cd code/android14
# 改用https 这里之前使用的是git://mirrors.ustc.edu.cn/aosp/platform/manifest，但是被防火墙屏蔽，导致代码一直同步失败
repo init -u https://mirrors.ustc.edu.cn/aosp/platform/manifest -b android-14.0.0_r2  # 初始化Android14源码
repo sync -j4   # 同步源码，j4是四线程，我的电脑最大是32线程，只要不卡死，就可以按需求弄。
repo sync -c -d --no-tags -j4   # 同步源码（占用小），-c 只下载当前分支，能减少约 40% 下载。--no-tags 能再省一点。-d 主要确保版本一致性，不影响大小。shell123456
```

下载完成如图所示说明成功：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/dde00d8696da2c0efdd00cfea45be645.png)

### 3.1 交换空间swap配置

> 在repo sync 时，如果内存爆了，电脑卡死，或者FAILED: out/soong/build.ninja这类报错，则需要swap交换空间，相当于手机的内存融合。

```bash
free -k     # 查看内存情况，以KB为单位，也可以使用-h,以合适单位，此处显示的是GB

# 创建要作为swap分区的文件:增加count大小的交换分区，则命令写法如下，其中的count等于想要的块的数量（bs*count=文件大小）。
# 这里count一般和电脑内存一致，我这里是1024 x 1024 x 16 = 16G，如果是32就是16777216 x 2 = 33554432
dd if=/dev/zero of=/var/swapfile bs=1024 count=16777216

mkswap /var/swapfile  # 格式化为交换分区文件,建立swap的文件系统
chmod -R 0600 /var/swapfile  # 修改权限
swapon /var/swapfile     # 启用交换分区文件，启用swap文件

# 使系统开机时自启用
vi /etc/fstab
/var/swapfile swap swap defaults 0 0   # 进入文件，在文件 /etc/fstab 中添加一行

#  如果启用swap文件报错时，提示：swapon 失败：设备或资源忙，可以先使用该命令关闭swap文件，再swapon /var/swapfile
swapoff /var/swapfileshell12345678910111213141516
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/1d97fdd618a5af3221a01a6fc41d7892.png)

## 4\. 源码全编译

> 可以直接在代码根目录通过以下命令打开模拟器，也可以使用替换img方式，在Android Studio中使用生成的版本打开模拟器。

```bash
. build/envsetup.sh       # 使用source build/envsetup.sh是一样的，用于初始化编译环境
make clobber              # 如果你之前编译过，使用此命令进行清除操作，以避免之前进行的build干扰到接下来的build
lunch sdk_phone_x86_64    # 如果编译模拟器的话, must this
make                      # 全仓编译，可能有大量warning，可忽略shell1234
```

编译完成如图所示说明成功：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/553f809378f75f0993c33d7e709dfb7b.png)

## 5\. 模拟器运行

```perl
# 使用可写入方式打开模拟器
emulator -writable-system
# 也可以直接输入emulator命令打开模拟器
emulatorshell1234
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/daa6229e14aab683ed8240b095e86faa.png)

### 5.1 编译完重启报错ERROR: No AVD specified.

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b35a765caead274d80d06c1f9ac29c3a.png)

```bash
# 再次lunch即可
. build/envsetup.sh
lunch sdk_phone_x86_64
emulator -writable-systemshell1234
```

## 6\. Android Studio打开

> 全编完成后，再单编译idegen，运行idegen.sh脚本，生成android.ipr和android.iml。使用Android Studio打开生成的android.ipr即可。

```bash
. build/envsetup.sh   # 或者 source build/envsetup.sh
make idegen   # 或者  mmm development/tools/idegen/
development/tools/idegen/idegen.sh # 可能有权限相关问题，无中断则继续
sudo chmod 777 android.iml android.iprshell1234
```

### 6.1 更换默认java版本为java 17

> 测试java 8和 java 11运行development/tools/idegen/idegen.sh都会有如下报错，java 17正常运行，但是有些时候又需要切换为老的java版本，使用sudo update-alternatives --config java 即可轻松管理。
> 
> -   当你执行 sudo update-alternatives --config java 时，系统会读取“注册表”，列出所有已注册的Java版本（候选人）并编号。  
>     你输入编号选择其中一个。  
>     update-alternatives 命令会立即修改 /etc/alternatives/java 这个符号链接，让它指向你选中的那个JDK的 java 程序。  
>     因为 /usr/bin/java 一直指向 /etc/alternatives/java，所以整个切换在瞬间完成，系统中的所有程序（包括你的终端、Android编译工具）之后调用的 java 命令，就会自动变成你新选中的版本。

```bash
# 编译development/tools/idegen/idegen.sh报错：
Error: A JNI error has occurred, please check your installation and try again
Exception in thread "main" java.lang.UnsupportedClassVersionError: Main has been compiled by a more recent version of the Java Runtime (class file version 61.0), this version of the Java Runtime only recognizes class file versions up to 52.0
at java.lang.ClassLoader.defineClass1(Native Method)
at java.lang.ClassLoader.defineClass(ClassLoader.java:756)
at java.security.SecureClassLoader.defineClass(SecureClassLoader.java:142)
at java.net.URLClassLoader.defineClass(URLClassLoader.java:473)
at java.net.URLClassLoader.access$100(URLClassLoader.java:74)
at java.net.URLClassLoader$1.run(URLClassLoader.java:369)
at java.net.URLClassLoader$1.run(URLClassLoader.java:363)
at java.security.AccessController.doPrivileged(Native Method)
at java.net.URLClassLoader.findClass(URLClassLoader.java:362)
at java.lang.ClassLoader.loadClass(ClassLoader.java:418)
at sun.misc.Launcher$AppClassLoader.loadClass(Launcher.java:352)
at java.lang.ClassLoader.loadClass(ClassLoader.java:351)
at sun.launcher.LauncherHelper.checkAndLoadMain(LauncherHelper.java:621)

# 有说java和javac版本不同的，有说版本过低的，我属于后者。参考：https://blog.csdn.net/yunna520/article/details/83346394
# 建议直接使用apt-get命令安装jdk，如果使用jar解压安装，可能update-alternatives识别不了。
sudo apt-get install openjdk-8-jdk   # 安装jdk 8
sudo apt-get install openjdk-11-jdk   # 安装jdk 11
sudo apt-get install openjdk-17-jdk    # 编译14的ipr需要17

sudo update-alternatives --config java  # 实测执行这一个就够了
sudo update-alternatives --config javacshell1234567891011121314151617181920212223242526
```

### 6.2 打开IDE卡死、重启依旧卡死

> 众所周知，`Android Studio`默认给的内存配置很小，打开一些大项目经常卡死，这时候就是想点击关闭得等很久根本没用。然后Window用户就会打开任务管理器杀死应用，linux用户也会使用xkill指令来结束应用。但是当再开启`Android Studio`时，它默认又会开启最后一次关闭的项目，导致卡死循环。这里强烈建议修改一些默认配置。

-   启动时不打开上一个项目：“2.1.3 启动首选项 & 自动保存” – 参考：[Android Studio 安装-配置-优化](https://blog.csdn.net/xunyan6234/article/details/132189026)
-   增大内存：“2.1.6 内存设置与显示” – 参考：[Android Studio 安装-配置-优化](https://blog.csdn.net/xunyan6234/article/details/132189026)
-   快捷杀死应用：“ubuntu配置快捷键xkill杀死应用” --参考：[4种强制关闭Ubuntu中无响应应用程序的方法](https://www.linuxidc.com/Linux/2019-02/156735.htm)
-   IDE多开，一个看源码一个跑demo，一个卡死另一个还能用：“3.2 多Android Studio共存” --参考：[Android Studio 安装-配置-优化](https://blog.csdn.net/xunyan6234/article/details/132189026)

### 6.3 源码打开索引很慢、卡顿问题

> 直接打开`android.ipr`，可能出现`Scanning Files to Index`过久的问题，如果电脑内存不大，且只需要查看`framework`相关代码。  
> 可以更改`android.iml`，建议先备份，然后搜索`excludeFolder`，在最后添加以下配置，排除掉除`framework`外的代码，让索引更快（如果只看其他模块，也是同理）。  
> 被`excludeFolder`的文件夹Android Studio打开将是橙色。

```kotlin
<excludeFolder url="file://$MODULE_DIR$/.repo" />
<excludeFolder url="file://$MODULE_DIR$/abi" />
<excludeFolder url="file://$MODULE_DIR$/art" />
<excludeFolder url="file://$MODULE_DIR$/bionic" />
<excludeFolder url="file://$MODULE_DIR$/bootable" />
<excludeFolder url="file://$MODULE_DIR$/build" />
<excludeFolder url="file://$MODULE_DIR$/cts" />
<excludeFolder url="file://$MODULE_DIR$/dalvik" />
<excludeFolder url="file://$MODULE_DIR$/developers" />
<excludeFolder url="file://$MODULE_DIR$/development" />
<excludeFolder url="file://$MODULE_DIR$/device" />
<excludeFolder url="file://$MODULE_DIR$/docs" />
<excludeFolder url="file://$MODULE_DIR$/external" />
<excludeFolder url="file://$MODULE_DIR$/hardware" />
<excludeFolder url="file://$MODULE_DIR$/libcore" />
<excludeFolder url="file://$MODULE_DIR$/libnativehelper" />
<excludeFolder url="file://$MODULE_DIR$/ndk" />
<excludeFolder url="file://$MODULE_DIR$/out" />
<excludeFolder url="file://$MODULE_DIR$/packages" />
<excludeFolder url="file://$MODULE_DIR$/pdk" />
<excludeFolder url="file://$MODULE_DIR$/prebuilt" />
<excludeFolder url="file://$MODULE_DIR$/prebuilts" />
<excludeFolder url="file://$MODULE_DIR$/sdk" />
<excludeFolder url="file://$MODULE_DIR$/system" />
<excludeFolder url="file://$MODULE_DIR$/tools" />xml12345678910111213141516171819202122232425
```

### 6.4 源码无法正常跳转

> Ctrl+鼠标左键进行源码跳转时，跳转到了SDK，而不是源码。此时就需要更改项目依赖的顺序了。SDK和JDK也需要和项目匹配，如图所示：  
> File -> Project Structure或者 Ctrl+Shift+Alt+S打开 Project Structure， 点击Modules -> Dependencies，将`<Module source>`和`Android API`移动到最上方。Dependencies中点击+，选择JARs or Directories，导入`framework`和`external`使用Alt+T将其上移到最前面，并apply。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9a1b8bb3f804b0413ba5367050952a0a.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/51fb42075a716b3f202494a4f0409800.png)

### 6.4 编译报错 microfactory\_test.go:191: Output timestamp should be different

```sql
FAILED: out/host/linux-x86/bin/go/blueprint-microfactory/test/test.passed
out/host/linux-x86/bin/gotestrunner -p build/blueprint/microfactory -f out/host/linux-x86/bin/go/blueprint-microfactory/test/test.passed -- out/host/linux-x86/bin/go/blueprint-microfactory/test/test -test
.short
--- FAIL: TestRebuildAfterRemoveOut (0.19s)
    microfactory_test.go:191: Output timestamp should be different, but both were 2024-08-15 02:05:58 +0000 UTC
--- FAIL: TestRebuildAfterMainChange (0.20s)
    microfactory_test.go:191: Output timestamp should be different, but both were 2024-08-15 02:05:58 +0000 UTC
--- FAIL: TestRebuildAfterPartialBuild (0.20s)
    microfactory_test.go:191: Output timestamp should be different, but both were 2024-08-15 02:05:58 +0000 UTC
--- FAIL: TestRebuildAfterGoChange (0.22s)
    microfactory_test.go:191: Output timestamp should be different, but both were 2024-08-15 02:05:58 +0000 UTC
FAIL
02:06:03 ninja failed with: exit status 1

#### failed to build some targets (03:59 (mm:ss)) ####


real        3m59.320s
user        23m57.294s
sys        8m41.755s
============================================
[build.sh]: FAILED: make -j16 ENABLE_AB=true SYSTEMEXT_SEPARATE_PARTITION_ENABLE=true BOARD_DYNAMIC_PARTITION_ENABLE=true ENABLE_VIRTUAL_AB=false SHIPPING_API_LEVEL=291234567891011121314151617181920212223
```

这个错误日志表明在构建过程中，blueprint-microfactory 的几个 测试用例 失败了。具体来说，这些测试用例期望在修改某些文件或条件后，构建的输出时间戳会有所不同，但实际上它们得到了相同的时间戳（2024-09-11 08:45:50 +0000 UTC）。这通常意味着构建系统可能没有正确地识别到这些变化，或者时间戳的更新逻辑存在问题。  
处理方法：没找到问题原因，但是可以规避一下。编辑 build/blueprint/microfactory/microfactory.go 文件，强制休息一秒：time.Sleep(1100 \* time.Millisecond)

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/9d66de978637419183570d1f6a730614.png)

## 参考

-   [aosp下载、编译、刷机和单编framework（android 12）](https://blog.csdn.net/weixin_44714875/article/details/125580601)
-   [AOSP linux环境配置及其编译方法](https://blog.csdn.net/yimelancholy/article/details/130462470?csdn_share_tail=%7B%22type%22:%22blog%22,%22rType%22:%22article%22,%22rId%22:%22130462470%22,%22source%22:%22yimelancholy%22%7D#t4)
-   [ubuntu 安装与切换Java版本以及javac](https://blog.csdn.net/yunna520/article/details/83346394)
-   [ubuntu22.04 多个　python　 版本切换](https://www.cnblogs.com/lshan/p/16579671.html)
-   [Android版本与SDK/API版本、JDK对应关系](https://blog.csdn.net/qq_42690281/article/details/131640670)
-   [repo引导脚本报错问题SyntaxError: invalid syntax](https://blog.csdn.net/alang_/article/details/134686462)
-   [android14源码下载备注](https://zhuanlan.zhihu.com/p/677191350)
-   [ubuntu-22.04 Android系统源码TP1A(Android 13)下载及编译](https://blog.csdn.net/w690333243/article/details/104779330)
-   [4种强制关闭Ubuntu中无响应应用程序的方法](https://www.linuxidc.com/Linux/2019-02/156735.htm)
-   [自己动手调试Android源码(超简单)](https://www.jianshu.com/p/30a628335114)