[VMware Workstation15 安装ubuntu-22.04](https://blog.csdn.net/w690333243/article/details/104766628)  
[ubuntu安装后配置](https://blog.csdn.net/w690333243/article/details/133969762)  
这篇主要介绍安装后环境搭建，Android源码的下载与编译。小编当前下载的是当前最新的代码，是主干分支代码，20200301.tar，后又同步，查看系统版本是R系统、即Android 11([获取 Android 11](https://developer.android.google.cn/preview/get))。解释 aosp（Android Open Source Project）。

windows查看ubuntu中的android源码 [ubuntu22.04配置samba实现ubuntu与windows共享文件夹](https://blog.csdn.net/w690333243/article/details/121449203)  
或 [FileZilla连接VMWare实现ubuntu与windows互传文件](https://blog.csdn.net/w690333243/article/details/122768215)  
为了便于网友针对性查看阅读，调整这篇文档结构，前半部分只写下载android aosp源码用到的，其他遇到的问题还有ubuntu配置相关的放到文档后半部分，由于不同的时间不同的环境配置不太一样，可能遇到的问题解决方式也不一样，建议网友阅读的时候，如果按照这里的介绍没有成功，多在网上搜索搜索或分析解决问题 2021.12.4 14:10 sh ylxy 50210  
同步完代码后，~/bin/aosp/ cat build/core/version\_default.xml可以看源码的版本信息  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/7b7ff576c43867592d3a9df1ca89c40c.png#pic_center)  
![请添加图片描述](https://i-blog.csdnimg.cn/blog_migrate/b4be8a9f48884e0e16341445e623a749.png)

## 一、Android系统源码 下载

### 1、迅雷下载aosp-latest.tar

从 [https://mirrors.tuna.tsinghua.edu.cn/aosp-monthly/](https://mirrors.tuna.tsinghua.edu.cn/aosp-monthly/) 下载当前最新的aosp jar包，也就是 [aosp-20210701.tar](https://mirrors.tuna.tsinghua.edu.cn/aosp-monthly/aosp-20210701.tar)，现在最新的就是20210701.tar 这个tar包，所以它和 aosp-latest.tar是同一个压缩包，从迅雷下载这个jar包(126G)，迅雷会使用云下载，真的超级快  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d83268ef92b5f1d1be04a2041f27dac8.png)  
下载完后，windows文件共享到ubuntu中，1右键按照下图开启共享  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c8256fc99f57f2ccc0cc5b0174a78c2b.png)

## 2、配置PATH环境变量

添加~/bin 到环境变量PATH里面  
从android的源码下载网站可以看到,是repo是建议我们在home下配置一个bin目录的，然后把bin目录path添加在配置文件中，笔者用的是虚拟机上的ubuntu，又想原本的操作系统可以看到下载好的源码，所以我是做了一个共享文件夹android，然后在共享文件夹中创建的bin目录，再把bin目录软连到home下。最后还是没有使用共享文件夹，担心不同的文件系统，会影响文件  
具体命令如下:

```bash
cd ~
mkdir ~/bin  # 1 。要么执行2语句，要么执行这条语句  
# ln -s /mnt/hgfs/android/bin/ ~   # 2
echo "PATH=~/bin:\$PATH" >> ~/.bashrc
source ~/.bashrcbash12345
```

此处创建软链接的效果等同于 主目录下有一个 bin/ 目录，并且该目录包含在PATH中  
软连建立：ln -s 源文件 软链接文件  
android是ubuntu共享windows下的文件夹，ln是创建了一个软链接

将下载好的aosp-latest.tar copy到bin目录下

```bash
cp /mnt/hgfs/xunlei/aosp-latest.tar ./bin/
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/2fddfc05e8e6c857d68e210cabe11612.png)  
如果你在ubuntu里面使用  
wget -c https://mirrors.tuna.tsinghua.edu.cn/aosp-monthly/aosp-latest.tar下载也快的话，你可以使用这种方式下载,就省了windows往ubuntu copy的过程

## 3、解压tar包，下载repo文件，repo sync

下载 repo 工具，修改它可执行

```bash
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
## 或者使用 curl https://mirrors.tuna.tsinghua.edu.cn/git/git-repo -o repo
chmod a+x ~/bin/repo
```

解压aosp-20200301.tar

```bash
cd ~/bin/
#wget -c https://mirrors.tuna.tsinghua.edu.cn/aosp-monthly/aosp-20200301.tar  不要下载aosp-latest.tar的，最好下载离aosp-latest.tar最近打包的，因为aosp-latest.tar一直在更新，你下载过程中服务器一旦更新，下载的信息可能发生错误，导致下载进度归零
##wget -c https://mirrors.tuna.tsinghua.edu.cn/aosp-monthly/aosp-latest.tar # 下载初始化包
tar xvf aosp-latest.tar # 解压得到的 aosp 工程目录
cd aosp
## repo init -u https://mirrors.tuna.tsinghua.edu.cn/git/AOSP/platform/manifest -b android-12.0.0_r15 //你可以使用此命令下载android12分支的代码，默认repo sync同步下来的是master分支的代码
repo sync # 正常同步一遍即可得到完整目录
```

查看分支信息，指定分支后(如下命令) 同步

```bash
xxx:~/bin/aosp$cd .repo/manifests
xxx:~/bin/aosp/.repo/manifests$ git branch -a 
xxx:~/bin/aosp/.repo/manifests$ git branch -a | grep "android-12"
xxx:~/bin/aosp$ repo init -u https://android.googlesource.com/platform/manifest -b android-13.0.0_r9
xxx:~/bin/aosp$ repo init -u https://android.googlesource.com/platform/manifest
xxx:~/bin/aosp$ repo init -u https://mirrors.tuna.tsinghua.edu.cn/git/AOSP/platform/manifest -b android-13.0.0_r9
```

如果init的时候未指定分支名称，在代码下载完成后，可以通过命令repo start android-13.0.0\_r9 --all 切换分支  
repo forall -c“ git checkout android\_4.2.2\_r1”这会将所有git存储库切换到所需的分支.  
更新于2022年10月30日 11:28 sh ylxy 50288168  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d01883cb9a55c65e2a3af27180aaff70.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/aa6c2ac674ca27f21d36aaa98d9c3f2b.png)  
[Repo下载AOSP源码：基于ubuntu22.04 环境配置](https://blog.csdn.net/yyzsyx/article/details/124738623)  
[Android AOSP 下载和编译](https://blog.csdn.net/yin13753884368/article/details/126756769)  
[\[aosp\]\[Android\] 如何查看当前分支](https://blog.csdn.net/kuniao/article/details/119911770)

### 4、repo sync

过程中如果遇到类似的问题  
info: A new version of repo is available  
warning: repo is not tracking a remote branch, so it will not receive updates  
repo reset: error: Entry ‘SUBMITTING\_PATCHES.md’ not uptodate. Cannot merge.  
fatal: Could not reset index file to revision ‘v2.18^0’.  
参考[Android系统源码AOSP repo sync报错:info: A new version of repo is available](https://blog.csdn.net/w690333243/article/details/121712454)  
[2020-12-09 Android源码里面查看版本号的方法](https://blog.csdn.net/qq_37858386/article/details/110918906)  
如下时间 2021.12.11 11:17

## 二、编译

安装jdk  
`sudo apt-get install openjdk-11-jdk`  
javac -version//查看安装是否成功

```csharp
//不知道哪些有用，哪些没用，网上都是这么写的，索性都安装了
sudo apt-get install libx11-dev:i386 libreadline6-dev:i386 libgl1-mesa-dev g++-multilib
sudo apt-get install -y git flex bison gperf build-essential libncurses5-dev:i386
sudo apt-get install tofrodos python-markdown libxml2-utils xsltproc zlib1g-dev:i386
sudo apt-get install dpkg-dev libsdl1.2-dev libesd0-dev
sudo apt-get install git-core gnupg flex bison gperf build-essential
sudo apt-get install zip curl zlib1g-dev gcc-multilib g++-multilib
sudo apt-get install libc6-dev-i386
sudo apt-get install lib32ncurses5-dev x11proto-core-dev libx11-dev
sudo apt-get install libgl1-mesa-dev libxml2-utils xsltproc unzip m4
sudo apt-get install lib32z-dev ccache
sudo apt-get install libssl-dev
```

小编在安装输入第一条命令的时候，ubuntu提示libreadline6-dev:i386 建议安装libreadline-dev:i386，于是又安装了一次libreadline-dev:i386

```go
source build/envsetup.sh
make clobber //如果你之前编译过，使用此命令进行清除操作，以避免之前进行的build干扰到接下来的build
//选择编译开发工程师的版本，方便debug
lunch aosp_x86_64-eng //如果你想编译aosp_x86_64-eng，可以直接这样写。你也可以只输入  lunch，然后下面再选版本，选版本时输入版本序号即可
make -j4
```

lunch命令查看可以编译的镜像类型  
![请添加图片描述](https://i-blog.csdnimg.cn/blog_migrate/c1039b79ff51fbd5fd91d6cd43d1183e.png)  
输入上面列出的镜像类型对应的序号，编译你需要的  
![请添加图片描述](https://i-blog.csdnimg.cn/blog_migrate/45129b677239d10107c9243df5cb58bb.png)

```markdown
You're building on Linux

Lunch menu... pick a combo:
     1. aosp_arm-eng
     2. aosp_arm64-eng
     3. aosp_barbet-userdebug
     4. aosp_blueline-userdebug
     5. aosp_blueline_car-userdebug
     6. aosp_bonito-userdebug
     7. aosp_bonito_car-userdebug
     8. aosp_bramble-userdebug
     9. aosp_bramble_car-userdebug
     10. aosp_car_arm-userdebug
     11. aosp_car_arm64-userdebug
     12. aosp_car_x86-userdebug
     13. aosp_car_x86_64-userdebug
     14. aosp_cf_arm64_auto-userdebug
     15. aosp_cf_arm64_phone-userdebug
     16. aosp_cf_x86_64_foldable-userdebug
     17. aosp_cf_x86_64_pc-userdebug
     18. aosp_cf_x86_64_phone-userdebug
     19. aosp_cf_x86_64_tv-userdebug
     20. aosp_cf_x86_auto-userdebug
     21. aosp_cf_x86_phone-userdebug
     22. aosp_cf_x86_tv-userdebug
     23. aosp_coral-userdebug
     24. aosp_coral_car-userdebug
     25. aosp_crosshatch-userdebug
     26. aosp_crosshatch_car-userdebug
     27. aosp_crosshatch_vf-userdebug
     28. aosp_flame-userdebug
     29. aosp_flame_car-userdebug
     30. aosp_redfin-userdebug
     31. aosp_redfin_car-userdebug
     32. aosp_redfin_vf-userdebug
     33. aosp_sargo-userdebug
     34. aosp_sargo_car-userdebug
     35. aosp_slider-userdebug
     36. aosp_sunfish-userdebug
     37. aosp_sunfish_car-userdebug
     38. aosp_trout_arm64-userdebug
     39. aosp_trout_x86-userdebug
     40. aosp_whitefin-userdebug
     41. aosp_x86-eng
     42. aosp_x86_64-eng
     43. arm_krait-eng
     44. arm_v7_v8-eng
     45. armv8-eng
     46. armv8_cortex_a55-eng
     47. armv8_kryo385-eng
     48. beagle_x15-userdebug
     49. beagle_x15_auto-userdebug
     50. car_x86_64-userdebug
     51. db845c-userdebug
     52. fuchsia_arm64-eng
     53. fuchsia_x86_64-eng
     54. gsi_car_arm64-userdebug
     55. gsi_car_x86_64-userdebug
     56. hikey-userdebug
     57. hikey64_only-userdebug
     58. hikey960-userdebug
     59. hikey960_tv-userdebug
     60. hikey_tv-userdebug
     61. pixel3_mainline-userdebug
     62. poplar-eng
     63. poplar-user
     64. poplar-userdebug
     65. qemu_trusty_arm64-userdebug
     66. sdk_car_arm-userdebug
     67. sdk_car_arm64-userdebug
     68. sdk_car_x86-userdebug
     69. sdk_car_x86_64-userdebug
     70. silvermont-eng
     71. uml-userdebug
     72. yukawa-userdebug
     73. yukawa_sei510-userdebug

Which would you like? [aosp_arm-eng]
```

要求电脑(虚拟机)可用的内存为16G，不然会很容易失败  
![请添加图片描述](https://i-blog.csdnimg.cn/blog_migrate/c19ac96af96731aed394fd1eb340b46b.png)

![请添加图片描述](https://i-blog.csdnimg.cn/blog_migrate/20ded9319a3b051909ee418f28cc3d46.png)  
![请添加图片描述](https://i-blog.csdnimg.cn/blog_migrate/61c5412b82e03b4d66ca57527f0a0938.png)  
失败，报错

```swift
[100% 250/250] out/soong/.bootstrap/bin/soong_build out/soong/build.ninja
FAILED: out/soong/build.ninja
cd "$(dirname "out/soong/.bootstrap/bin/soong_build")" && BUILDER="$PWD/$(basen
ame "out/soong/.bootstrap/bin/soong_build")" && cd / && env -i "$BUILDER"     -
-top "$TOP"     --out "out/soong"     -n "out"     -d "out/soong/build.ninja.d"
     -t -l out/.module_paths/Android.bp.list -globFile out/soong/.bootstrap/bui
ld-globs.ninja -o out/soong/build.ninja --available_env out/soong/soong.environ
ment.available --used_env out/soong/soong.environment.used Android.bp
Killed
13:38:51 soong bootstrap failed with: exit status 1
FAILED: ninja fifo didn't finish after 5s
ninja: build stopped: subcommand failed.

#### failed to build some targets (53:53 (mm:ss)) ####


============================================
[100% 1/1] out/soong/.bootstrap/bin/soong_build out/soong/build.ninja
FAILED: out/soong/build.ninja
cd "$(dirname "out/soong/.bootstrap/bin/soong_build")" && BUILDER="$PWD/$(basename "out/soong/.bo
otstrap/bin/soong_build")" && cd / && env -i "$BUILDER"     --top "$TOP"     --out "out/soong"   
  -n "out"     -d "out/soong/build.ninja.d"     -t -l out/.module_paths/Android.bp.list -globFile
 out/soong/.bootstrap/build-globs.ninja -o out/soong/build.ninja --available_env out/soong/soong.
environment.available --used_env out/soong/soong.environment.used Android.bp
Killed
16:21:28 soong bootstrap failed with: exit status 1
ninja: build stopped: subcommand failed.

#### failed to build some targets (01:26:03 (hh:mm:ss)) ####
```

如果报以上错误，就说明是内存不够。可以增加页交换大小(方式见下面截图)，来尝试解决(并不一定能解决，小编之前用的ubuntu18，后来有改用ubuntu20.04下载的，尝试修改了7、8次吧，每次编译12个小时以上，后面总是卡住不动了，直接卡死，不清楚是不是哪修改的不对，没成功，准备再换成ubuntu18编译试试)。页交换，即使用物理存储当内存用  
注意：虚拟机重新开机后，新增加的页交换会失效

```swift
free -m //查看内存情况
//创建交换分区的文件：增加16G大小的交换分区，则命令写法如下，其中的 count 等于想要的块大小
dd if=/dev/zero of=/home/swapfile bs=1M count=16384
//设置交换分区文件，建立swap的文件系统
mkswap /home/swapfile
//立即启用交换分区文件
swapon /home/swapfile
```

![请添加图片描述](https://i-blog.csdnimg.cn/blog_migrate/bc0fdaba309fdcdd640d9b0a51bd122c.png)  
![请添加图片描述](https://i-blog.csdnimg.cn/blog_migrate/6e6fa24c835ab0c3efdde5b04aec65c3.png)  
每次编译一夜，或者长时间后就会各种卡死，还没成功  
![请添加图片描述](https://i-blog.csdnimg.cn/blog_migrate/3e4c00a20b08ea7b2631352607b1d692.png)  
2021.12.11 11:51 sh ylxy yyds 5210  
参考：[vmware和ubuntu编译运行aosp](https://www.jianshu.com/p/6dee223ac93e)

下面部分的内容是小编20年到今天(2021.12.04 14:52)下载代码的一些过程(并不是下了两年，是有时候会更新，或者从新搞)，遇到的问题及解决方法，如果有需要的，可以继续看下面部分的内容。

[自己动手编译Android源码(超详细)](https://www.jianshu.com/p/367f0886e62b)  
[Ubuntu 18.04 配置android 源码开发/编译环境](https://blog.csdn.net/aaa111/article/details/93841285)

#### 安装 repo

```bash
sudo apt install repo
```

安装依赖库

```bash
sudo apt-get install libx11-dev:i386 libreadline6-dev:i386 libgl1-mesa-dev g++-multilib
sudo apt-get install -y git flex bison gperf build-essential libncurses5-dev:i386
sudo apt-get install tofrodos python-markdown libxml2-utils xsltproc zlib1g-dev:i386
sudo apt-get install dpkg-dev libsdl1.2-dev libesd0-dev
sudo apt-get install git-core gnupg flex bison gperf build-essential
sudo apt-get install zip curl zlib1g-dev gcc-multilib g++-multilib
sudo apt-get install libc6-dev-i386
sudo apt-get install lib32ncurses5-dev x11proto-core-dev libx11-dev
sudo apt-get install libgl1-mesa-dev libxml2-utils xsltproc unzip m4
sudo apt-get install lib32z-dev ccache
sudo apt-get install libssl-dev
```

如果报错 ，有可能当时服务器网络不稳定，过一会重新执行  
unable to locate package libesd0-dev

```swift
sudo vim /etc/apt/sources.list  //在行尾添加如下两行的内容
deb http://archive.ubuntu.com/ubuntu/ trusty main universe restricted multiverse
```

更新软件源并重新安装:

```sql
sudo apt-get update && sudo apt-get install libesd0-dev
```

[ubuntu18.04无法安装libesd0-dev完美解决办法](https://blog.csdn.net/panda_8/article/details/103925942)  
下载 Repo 工具，并确保它可执行

```bash
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
## 或者使用 curl https://mirrors.tuna.tsinghua.edu.cn/git/git-repo -o repo
chmod a+x ~/bin/repo
```

## 下载代码

mkdir bin  
cd bin

```shell
wget -c https://mirrors.tuna.tsinghua.edu.cn/aosp-monthly/aosp-20200301.tar  不要下载aosp-latest.tar的，最好下载离aosp-latest.tar最近打包的，因为aosp-latest.tar一直在更新，你下载过程中服务器一旦更新，下载的信息可能发生错误，导致下载进度归零
##wget -c https://mirrors.tuna.tsinghua.edu.cn/aosp-monthly/aosp-latest.tar # 下载初始化包
tar xvf aosp-latest.tar # 解压得到的 AOSP 工程目录//如果提示没权限，可以使用 sudo  tar xvf  xxx
cd AOSP   
repo sync # 正常同步一遍即可得到完整目录
# ../repo sync  小编后来使用的这个命令，意思是使用自己下载的repo命令，即bin/repo，当前所在的目录是aosp目录，所以用../repo sync
```

小编用ubuntu下载，一直没下载成功，从这周二就开始倒腾这个，时不时的和服务器就断了，不知为何，此时下载的是aosp-latest.tar。用迅雷也在下载，好在迅雷下载的稳定些  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/a8955be76d4bd0638272a0114f1242bb.png)  
已经下了3天了。后来突然归零了。又从新下载了20190801号的tar包。  
执行repo sync之后，我的ubuntu 提示了一句

```ruby
warning: redirecting to https://aosp.tuna.tsinghua.edu.cn/platform/manifest/
```

然后一直不动，从昨天到现在有20个小时了，我一怒之下 安装ctrl + c，提示  
aborted by user  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b01ee3da9cc354e890390f5f400b7db8.png)  
[Android 8.1.0 AOSP源码下载及编译](https://www.jianshu.com/p/6fed8af8d11e)  
[Another infinity](http://mirrors.ustc.edu.cn/)  
[Android 9、10 -源码下载编译](https://blog.csdn.net/wangwei890702/article/details/86380477)  
[Android源码 半小时下完Android系统源码](https://blog.csdn.net/ynztlxdeai/article/details/65936140)  
[下载源代码](https://source.android.google.cn/source/downloading)  
[Android源代码下载与编译 - 2019](https://www.jianshu.com/p/302718c6fe3b)  
2020年3月11日 20:48 yuxinjy

[Android 镜像使用帮助](https://mirrors.tuna.tsinghua.edu.cn/help/AOSP/)  
[Git Repo 镜像使用帮助](https://mirrors.tuna.tsinghua.edu.cn/help/git-repo/)  
[AOSP(Android) 镜像使用帮助](https://lug.ustc.edu.cn/wiki/mirrors/help/aosp)  
[Index of /aosp-monthly/](https://mirrors.tuna.tsinghua.edu.cn/aosp-monthly/)  
[代号、标记和细分版本号](https://source.android.google.cn/setup/start/build-numbers.html#source-code-tags-and-builds)  
[Android 系统源码——下载到编译](https://blog.csdn.net/xx326664162/article/details/86354616)  
[自己动手编译Android源码(超详细)](https://www.jianshu.com/p/367f0886e62b)

很是倒腾，快弄一个周了，镜像下载也是特别慢，而且总是中断，不支持断点续传。镜像比较大  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/bba013c7ccfa2db12fa1538dc8b10573.png)

2020年3月13日 13:27 yuxinjy sh

很遗憾，源码编译问题还没搞定，准确的来说是进行到了源码同步阶段  
即repo sync，我下载的是20190801日的tar包，同步的时候老是出错，应该是网络问题吧，正在下载20200301日的tar包，因为下载aosp-latest.tar下载了两次，第一次到70%，归零了，第二次10% 又归零了，搞了10天了有，很头疼，继续吧  
2020年3月18日 18:29 yuxingjiayuan sh  
看看repo sync期间的错误  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/4b786b015031a41188798105849abc1b.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5792c8c521b4030eba9e5826fbea2784.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/75156c14c23a761e8cc91caefeef5699.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/dadebe2decbe7207fdc371ebd94d09b9.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c17f17ebfc9a422f867dd75ead4af60e.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5262c8776ca8dec82c924b3e92838b8b.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/91d8491e5342167a4184671e5adcaee0.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/1c11d85556d0e1497b0be60a9733f891.png)  
小编在同步过程中，时不时的就会卡着不动，也不知道是没反应了，还是一直在后台运行，只是界面没有更新。于是小编时不时的要 ctrl + c 一下，然后重新执行 …/repo sync命令，有时半夜也要起来看一下，看迅雷下载是不是还在继续，同步是不是还在执行。好在今天起来看到了 repo successfully的字样，从搭建虚拟机环境到现在前前后后已经快2周了，下载代码才基本搞定。为了避免同步过程中有丢失的文件，所以小编在提示repo successfully后又多同步了几次，还在同步。网上很多博客说的同步这一块很简单，不清楚他们是怎么做到的，小编在虚拟机上所有的操作，都在这个博客里进行了记录。希望对大家有所帮助，在你怀疑网络问题的时候(有墙)，只能能做一步是一步吧，坚持，如果实在不行，也没办法。  
2020年3月21日 7:31 yuxingjiay sh

## 编译

首先cd到应的源码目路，初始化编译环境。小编的目录是 aosp

```bash
source build/envsetup.sh
```

初始化编译环境后，引入了一些执行脚本，其中就包括马上要使用的lunch指令。通过lunch指令可以设置编译目标，所谓的编译目标就是生成的镜像要运行在什么样的设备上。这里我们设置的编译目标是aosp\_arm64-eng，因此执行指令

```
lunch aosp_arm-eng
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/ee6e7243cd5e6514d277891415e00365.png)  
aosp表示Android Open Source Project，arm64表示是使用arm64 cpu的设备，eng表示engineer版本，其直接开放了一些root等权限。当然直接使用lunch命令会列出所有可选的编译目标。  
我们可通过如下命令来开始编译andriod源码

```go
make -j4
```

这里的j4表示可以开启4个线程来参与编译源码，这里指定的线程数一般应该遵从cpu内核数的2倍这个规律，可以通过cat /proc/cpuinfo查看相关cpu信息。  
[Ubantu18.04环境下编译android源码](https://blog.csdn.net/Yoryky/article/details/81813717)  
2020年3月23日11:12 ylxy sq  
编译出错

```swift
============================================
[100% 55/55] out/soong/.bootstrap/bin/soong_build out/soong/build.ninja
FAILED: out/soong/build.ninja
out/soong/.bootstrap/bin/soong_build -t -l out/.module_paths/Android.bp.list -b out/soong -n out -d out/soong/build.ninja.d -globFile out/soong/.bootstrap/build-globs.ninja -o out/soong/build.ninja Android.bp
Killed
16:40:26 soong bootstrap failed with: exit status 1
FAILED: ninja fifo didn't finish after 5s
ninja: build stopped: subcommand failed.
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f3eb6a14174d0e1d6a1019eabf1cdde8.png)  
由于没有具体报错信息，于是就网上搜索乱试一通

```bash
export LC_ALL=C
```

[Ubuntu18.04编译Android源码报\[run\_soong\_ui\] Error 1错误](https://www.jianshu.com/p/c1a726adc4a1)  
可以将export LC\_ALL=C添加到~/.bashrc文件。LC\_ALL=C 是为了去除所有本地化的设置，让命令能正确执行。  
[Ubuntu18.04下编译Android源码笔记](https://www.jianshu.com/p/1d3321ad36a2)

vim /etc/security/limits.conf 添加如下的行

```markdown
# 解除服务器所有用户文件数 进程限制
*             soft       nofile      10240
*             hard       nofile      10240
*             soft       noproc      10240
*             hard       noproc      10240
```

保存后，再输入ulimit -n 10240回车即可生效。

```bash
sudo sh -c "ulimit -n 65535 && exec su $LOGNAME"
```

[编译错误-build stopped: subcommand failed. 解决方法](https://blog.csdn.net/touxiong/article/details/86233805)  
[Android soong build系统介绍](https://www.jianshu.com/p/80013a768a45)

上面命令执行后有一些用处，之前编译了不到一个小时就报错了，现在编译到1个多小时后报错。最好看官方编译文档，但是没找到  
准备换成 lunch aosp\_arm64-eng 试试

[Ubuntu 18.04（虚拟机）环境下编译Android 源码](https://www.jianshu.com/p/2beae17c9b12)  
[Android 系统源码——下载到编译](https://blog.csdn.net/xx326664162/article/details/86354616)  
[自己动手编译Android源码(超详细)](https://www.jianshu.com/p/367f0886e62b)  
2020年3月24日 21:26 ylxysq sh  
由于没有官方文档做参考，同时网上也没有找到Android R系统源码的编译相关资料，也不清楚是不是代码没有同步完整的问题引起的，一直没编译成功，很耗费时间，故边搁置边编译。在同步代码的时候，这个platform，你把它当成即当前的aosp目录即可，platform后面的目录即是你aosp下的同步到本地的文件夹  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/1b1d0a720cb20ebf2de11c1c866a4289.png)  
2020年3月26日 10:21 yyjy

#### aosp repo reset: error: Entry ‘command.py’ not uptodate. Cannot merge.

fatal: Could not reset index file to revision ‘v2.17.3^0’.

下载之后按照官网[https://mirrors.tuna.tsinghua.edu.cn/help/AOSP/](https://mirrors.tuna.tsinghua.edu.cn/help/AOSP/)说的 repo同步方法

```bash
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
```

报错

```vbnet
aosp repo reset: error: Entry 'command.py' not uptodate. Cannot merge.
fatal: Could not reset index file to revision 'v2.17.3^0'.
```

网上没找到此问题的解决方案，找个类似的没有解决问题  
一手下载6月1号的aosp jar包，一手查看代码，结果发现  
.repo/repo路径下有 repo文件，  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/be64e1f888dcd04b43aba728ea08330e.png)  
使用此文件同步  
bin/aosp路径下  
./.repo/repo/repo sync  
同步成功  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d999b0810d19433c35c3cf7753bf7662.png)  
2021.11.23 19:18 还未吃饭 ylxy sh

如何平时自己使用，可以直接修改账户管理员权限  
[Ubuntu创建用户及用户配置为管理员](https://blog.csdn.net/u010417914/article/details/96028894)  
sudo vi /etc/sudoers  
wrr ALL=(ALL:ALL) ALL //用户名后面的空格是tab键  
wq! 强制保存

ubuntu20.04执行的时候，解压aosp-latest.jar时老是报权限问题，原来原因是bin文件夹是在root账户创建的，而解压操作是在wrr账户执行的，重新在wrr账户创建bin文件夹即可

###### /usr/bin/env: ‘python’: No such file or directory

sudo ln -s /usr/bin/python3 /usr/bin/python  
[解决：/usr/bin/env: ‘python’: No such file or directory](https://blog.csdn.net/qq_41550190/article/details/119804102)

如果出现此错误  
info: A new version of repo is available  
warning: repo is not tracking a remote branch, so it will not receive updates  
repo reset: error: Entry ‘SUBMITTING\_PATCHES.md’ not uptodate. Cannot merge.  
fatal: Could not reset index file to revision ‘v2.18^0’.  
参考[Android系统源码AOSP repo sync报错:info: A new version of repo is available](https://blog.csdn.net/w690333243/article/details/121712454) 解决  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/53ffcc15e7deed9af6ef1515e5d0436e.png)  
继续重新同步  
2021.12.02 19:25

##### ubuntu 无法联网

打开终端窗口，在终端窗口输入如下命令：

```swift
sudo service network-manager stop
#cp /var/lib/NetworkManager/NetworkManager.state  /var/lib/NetworkManager/NetworkManager.state.bak
sudo rm /var/lib/NetworkManager/NetworkManager.state  //注意先备份
sudo service network-manager start
```

重新打开 Firefox浏览器，即可联网  
[Ubuntu 20.04.1 无法联网](https://www.cnblogs.com/hyzhu-lucky/p/13809415.html)

[Ubuntu 20.04桌面很卡的解决方案–亲测有效](https://blog.csdn.net/woody218/article/details/119532283)

##### 将ubuntu中的文件copy到共享目录报错问题

```lua
cp: error writing '/mnt/hgfs/xunlei/frameworks.tar': Input/output error
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5da3b9f5c36fa6684dd6342d7557887f.png)  
2021.12.04 15:03

![请添加图片描述](https://i-blog.csdnimg.cn/blog_migrate/aab10aa9050c98a9e894ac15efdd9ff7.png)

ubuntu开机密码卡死，或者命令终端界面卡死

```csharp
sudo dpkg --get-selections | grep linux  //查看安装的linux内核
sudo apt remove linux-image-5.11.0-27-generic  //卸载多余的内核
```

![请添加图片描述](https://i-blog.csdnimg.cn/blog_migrate/7bb81359b3304e84be462ee62af0abfc.png)

[ubuntu开机输入密码卡死或者无法进入命令行的解决方法](https://blog.csdn.net/da_ge_chen/article/details/105483819)  
20211211 xx:xx

[Windows 环境下载 Android 源码](https://www.jianshu.com/p/28ea6268f50b)  
[windows环境android源码下载](https://blog.csdn.net/tracydragonlxy/article/details/76178409)  
20220320 16:55