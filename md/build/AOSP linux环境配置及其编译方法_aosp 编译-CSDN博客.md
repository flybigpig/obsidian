## 安装依赖libs

```csharp
sudo apt-get install openjdk-11-jdk;#安装jdk

sudo apt-get install libx11-dev:i386 libreadline6-dev:i386 libgl1-mesa-dev
sudo apt-get install -y git flex bison gperf build-essential libncurses5-dev:i386
sudo apt-get install tofrodos python-markdown libxml2-utils xsltproc zlib1g-dev:i386
sudo apt-get install dpkg-dev libsdl1.2-dev
sudo apt-get install git-core gnupg flex bison gperf build-essential
sudo apt-get install zip curl zlib1g-dev gcc-multilib
sudo apt-get install libc6-dev-i386
sudo apt-get install lib32ncurses5-dev x11proto-core-dev libx11-dev
sudo apt-get install libgl1-mesa-dev libxml2-utils xsltproc unzip m4
sudo apt-get install lib32z-dev ccache;
sudo apt-get install libssl-dev libncurses5;bash12345678910111213
```

## 下载 repo 工具

```bash
sudo apt install curl

mkdir ~/bin
PATH=~/bin:$PATH
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repobash123456
```

## 换源

1.修改还源文件权限

```bash
sudo chmod 777 /etc/apt/sources.listbash1
```

2.备份文件

```bash
sudo cp /etc/apt/sources.list /etc/apt/sources.list.bakbash1
```

3.修改  
清华源  
地址：`https://mirrors.tuna.tsinghua.edu.cn/help/ubuntu/`

```cpp
# 默认注释了源码镜像以提高 apt update 速度，如有需要可自行取消注释
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy main restricted universe multiverse
# deb-src https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy-updates main restricted universe multiverse
# deb-src https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy-updates main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy-backports main restricted universe multiverse
# deb-src https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy-backports main restricted universe multiverse

# deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy-security main restricted universe multiverse
# # deb-src https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy-security main restricted universe multiverse

deb http://security.ubuntu.com/ubuntu/ jammy-security main restricted universe multiverse
# deb-src http://security.ubuntu.com/ubuntu/ jammy-security main restricted universe multiverse

# 预发布软件源，不建议启用
# deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy-proposed main restricted universe multiverse
# # deb-src https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ jammy-proposed main restricted universe multiversebash1234567891011121314151617
```

中国科学技术大学源  
地址：`https://lug.ustc.edu.cn/wiki/mirrors/help/ubuntu/`

```cpp
# 默认注释了源码镜像以提高 apt update 速度，如有需要可自行取消注释
deb https://mirrors.ustc.edu.cn/ubuntu/ xenial main restricted universe multiverse
# deb-src https://mirrors.ustc.edu.cn/ubuntu/ xenial main main restricted universe multiverse
deb https://mirrors.ustc.edu.cn/ubuntu/ xenial-updates main restricted universe multiverse
# deb-src https://mirrors.ustc.edu.cn/ubuntu/ xenial-updates main restricted universe multiverse
deb https://mirrors.ustc.edu.cn/ubuntu/ xenial-backports main restricted universe multiverse
# deb-src https://mirrors.ustc.edu.cn/ubuntu/ xenial-backports main restricted universe multiverse
deb https://mirrors.ustc.edu.cn/ubuntu/ xenial-security main restricted universe multiverse
# deb-src https://mirrors.ustc.edu.cn/ubuntu/ xenial-security main restricted universe multiverse

# 预发布软件源，不建议启用
# deb https://mirrors.ustc.edu.cn/ubuntu/ xenial-proposed main restricted universe multiverse
# deb-src https://mirrors.ustc.edu.cn/ubuntu/ xenial-proposed main restricted universe multiversebash12345678910111213
```

北京外国语大学  
地址：`https://mirrors.bfsu.edu.cn/help/ubuntu/`

```cpp
# 默认注释了源码镜像以提高 apt update 速度，如有需要可自行取消注释
deb https://mirrors.bfsu.edu.cn/ubuntu/ jammy main restricted universe multiverse
# deb-src https://mirrors.bfsu.edu.cn/ubuntu/ jammy main restricted universe multiverse
deb https://mirrors.bfsu.edu.cn/ubuntu/ jammy-updates main restricted universe multiverse
# deb-src https://mirrors.bfsu.edu.cn/ubuntu/ jammy-updates main restricted universe multiverse
deb https://mirrors.bfsu.edu.cn/ubuntu/ jammy-backports main restricted universe multiverse
# deb-src https://mirrors.bfsu.edu.cn/ubuntu/ jammy-backports main restricted universe multiverse

# 以下安全更新软件源为官方源配置
deb http://security.ubuntu.com/ubuntu/ jammy-security main restricted universe multiverse
# deb-src http://security.ubuntu.com/ubuntu/ jammy-security main restricted universe multiverse

# 预发布软件源，不建议启用
# deb https://mirrors.bfsu.edu.cn/ubuntu/ jammy-proposed main restricted universe multiverse
# # deb-src https://mirrors.bfsu.edu.cn/ubuntu/ jammy-proposed main restricted universe multiverse
```

阿里源  
地址：`https://developer.aliyun.com/mirror/ubuntu`

```cpp
deb http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiverse
deb http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiverse
deb http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiverse
deb http://mirrors.aliyun.com/ubuntu/ focal-proposed main restricted universe multiverse
deb http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse
deb-src http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiverse
deb-src http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiverse
deb-src http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiverse
deb-src http://mirrors.aliyun.com/ubuntu/ focal-proposed main restricted universe multiverse
deb-src http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse
```

4.更新

```sql
sudo apt-get update
sudo apt-get upgrade
```

## 初始化与同步

清华：`https://mirrors.tuna.tsinghua.edu.cn/help/AOSP/`  
中科大：`https://mirrors.ustc.edu.cn/help/aosp.html`

### 操作方法

方法一：

```bash
curl -OC - https://mirrors.tuna.tsinghua.edu.cn/aosp-monthly/aosp-latest.tar # 下载初始化包
tar xf aosp-latest.tar
cd AOSP   # 解压得到的 AOSP 工程目录
# 这时 ls 的话什么也看不到，因为只有一个隐藏的 .repo 目录
repo sync # 正常同步一遍即可得到完整目录
# 或 repo sync -l 仅checkout代码bash123456
```

使用`curl` 下载不了可以直接网站上下载`https://mirrors.ustc.edu.cn/aosp-monthly/`

方法二：

```bash
mkdir WORKING_DIRECTORY
cd WORKING_DIRECTORY
repo init -u https://mirrors.tuna.tsinghua.edu.cn/git/AOSP/mirror/manifest#默认最新

# 选择特定版本 版本列表https://source.android.com/setup/start/build-numbers#source-code-tags-and-builds
repo init -u https://mirrors.tuna.tsinghua.edu.cn/git/AOSP/platform/manifest -b android-10.0.0_r1
repo init -u https://mirrors.tuna.tsinghua.edu.cn/git/AOSP/platform/manifest -b android-13.0.0_r6
repo init -u https://mirrors.tuna.tsinghua.edu.cn/git/AOSP/platform/manifest -b android-13.0.0_r43
# --partial-clone 部分克隆 / 浅克隆 ，不拉取完整 Git 历史，只拉取当前版本的代码
repo init --partial-clone -u https://mirrors.ustc.edu.cn/aosp/platform/manifest -b android-16.0.0_r4 

repo sync#同步（全仓下载）
#repo sync -c -j$(nproc) --optimized-fetch --prune --no-tags
repo sync -c -j$(nproc) --optimized-fetch --prune
# 参数说明：
# -c：仅同步当前分支，跳过无关历史
# -j$(nproc)：按 CPU 核心数并行下载（建议 8~16 线程，避免内存溢出）
# --optimized-fetch：复用已有对象，加速增量同步
# --prune：自动清理远程已删除的引用


# 单仓下载方式
repo sync -c platform/frameworks/base
repo sync -c platform/frameworks/native
repo sync -c system/corebash1234567891011121314151617181920212223242526
```

`https://source.android.com/setup/start/build-numbers#source-code-tags-and-builds`查看版本列表，如果网站打不开可以下载附件：[android版本列表以及其适用机型](https://download.csdn.net/download/yimelancholy/88745633)

其他参考：[https://mirrors.ustc.edu.cn/help/aosp.html](https://mirrors.ustc.edu.cn/help/aosp.html)

### 常见报错

-   下载期间出现问题  
    常见的场景有某个单仓一直拉不下，或者checkout异常等类似如下报错：
    
    ```bash
    Aborting                                                                                                                
    error: prebuilts/rust/: platform/prebuilts/rust checkout eaf5f0747eaafadf4c75af7354aae3e59a7adfe5                       
    Syncing: 100% (1011/1011), done in 45.365s                                                                              
    error: Unable to fully sync the tree                                                                                    
    error: Checking out local projects failed.                                                                              
    Failing repos (checkout):                                                                                               
    external/opencl/llvm-project                                                                                            
    prebuilts/clang/host/linux-x86                                                                                          
    prebuilts/rust                                                                                                          
    Try re-running with "-j1 --fail-fast" to exit at the first error.                                                       
    ================================================================================                                        
    Repo command failed due to the following `SyncError` errors:                                                            
    platform/external/opencl/llvm-project checkout c18204eb35271c426b554b22301e34728b361d51                                 
    platform/prebuilts/clang/host/linux-x86 checkout cd12ddc5fb6457b221a38245ccb87203f44e09cc                               
    platform/prebuilts/rust checkout eaf5f0747eaafadf4c75af7354aae3e59a7adfe5bash123456789101112131415
    ```
    
    可以尝试运行以下指令，后重新`repo sync`  
    1.清理冲突  
    `repo forall -c 'git reset --hard HEAD; git clean -dfx'`  
    2.下载大文件  
    `repo forall -c "git lfs pull"`
    
-   如果提示无法连接到 `gerrit.googlesource.com`  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/259bbc80dadc8e763bf2c74593276079.png#pic_center)
    
    ```bash
    curl https://mirrors.tuna.tsinghua.edu.cn/git/git-repo -o repo
    chmod +x repo
    export REPO_URL='https://mirrors.tuna.tsinghua.edu.cn/git/git-repo'bash123
    ```
    
-   如果提示身份未知  
    按照提示设置email和name即可  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/e8f30d96c1a629d09f9222b3277b38b6.png#pic_center)
    
-   repo脚本问题  
    在初始化代码操作或者使用`repo --version`命令检查repo时，可能会有以下报错：
    
    ```ruby
    File "/home/xxx/bin/repo", line 51
        def print(self, *args, **kwargs):
                ^
    SyntaxError: invalid syntaxbash1234
    ```
    
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/4bb3eb7ab5d18f7a838cd6bcbab7c84a.png)  
    改为使用以下引导脚本:
    
    ```bash
    curl -sSL  'https://gerrit-googlesource.proxy.ustclug.org/git-repo/+/master/repo?format=TEXT' |base64 -d > ~/bin/repobash1
    ```
    

## 编译

### 全仓编译

```bash
. build/envsetup.sh
lunch sdk_phone_x86_64 #must this

 # android 16
lunch sdk_phone64_x86_64 # userdebug
lunch sdk_phone64_x86_64-trunk_staging-eng # eng工程版

# 车载
# 基准单屏配置，最基础的车载模拟器版本，默认只配置一个中控主屏幕，适合绝大多数单屏车机应用的常规开发与调试
lunch sdk_car_x86_64 
# 多屏（Multi-Display）配置，额外配置了副屏（如仪表盘、后排娱乐屏）相关的多屏布局、触摸输入（multi-touch）以及音频设置，专门用于多屏互联场景的开发测试
lunch sdk_car_md_x86_64 

make -j16#全仓编译bash1234567891011121314
```

编译完成后,输入命令`emulator`运行模拟器  
注：第一次编译一定要全仓编译

### 模拟器运行

第一次编译之后如果不想再编译直接运行模拟器，可以用以下命令

```bash
. build/envsetup.sh
lunch sdk_phone_x86_64  # must this

 # android 16
lunch sdk_phone64_x86_64 # userdebug
lunch sdk_phone64_x86_64-trunk_staging-eng # eng工程版

# 车载
lunch sdk_car_x86_64 # 单屏
lunch sdk_car_md_x86_64 # 多屏

# 以可写的形式打开模拟器，否则模拟器文件是只读状态
emulator -writable-system

# 清空/data目录下模拟器数据（恢复出厂设置）
emulator -wipe-databash12345678910111213141516
```

### 常见的单模块编译

使用`mm`和`mmm`命令或者`make`命令编译模块代码

-   `mm`和`mmm`命令编译模块代码  
    mm（make module）命令用于**编译当前目录下的模块**。当你使用mm时，它会找到当前目录对应的Android.mk文件，并且只编译这个目录下定义的模块。  
    例如：进入到`frameworks\base\services\`目录，直接运行`mm`命令编译模块
    
    mmm（make module matches）命令与mm类似，也是用于编译特定的模块，但它允许你指定一个路径。这意味着你可以从任何地方执行mmm命令，并提供要编译模块的路径作为参数。  
    例如，你可以在**源码的根目录下运行**`mmm frameworks\base\services\`来编译位于`frameworks\base\services\`目录下的模块。
    
-   `make`命令编译模块代码  
    **在源码根目录下运行命令**
    
    模块：`make Launcher3`  
    代码路径： packages/apps/Launcher3  
    生成包目录：out/target/product/emulator\_x86\_64/system\_ext/priv-app/Launcher3  
    adb install -t -d Launcher3.apk
    
    模块：`make Launcher3QuickStep`  
    代码路径： packages/apps/Launcher3/quickstep  
    生成包目录：out/target/product/emulator\_x86\_64/system\_ext/priv-app/Launcher3QuickStep  
    adb install -t -d Launcher3QuickStep.apk
    
    模块：`make SystemUI`  
    代码路径：frameworks\\base\\packages\\SystemUI  
    代码路径：frameworks\\base\\libs\\WindowManager\\Shell（wmshell模块在SystemUI进程）  
    生成包目录：out/target/product/emulator\_x86\_64/system\_ext/priv-app/SystemUI  
    adb push SystemUI.apk /system\_ext/priv-app/SystemUI  
    注：有的公司SystemUI在 packages/apps/目录下集成，生成apk的位置也会不同，需要看对应目录下是否有Android.bp或者mk之类的文件。比如原生代码在frameworks\\base\\packages\\SystemUI目录有Android.bp文件，说明SystemUI就是在这个目录下集成。
    
    模块：`make services`  
    代码路径：frameworks\\base\\services  
    生成包目录：out/target/product/emulator\_x86\_64/system/framework/services.jar  
    adb push services.jar /system/framework/
    
    模块：`make framework` （android 11之前的代码使用）  
    模块：`make framework-minus-apex`（android 11及其之后的代码使用）  
    代码路径：frameworks\\base\\  
    生成包目录：out/target/product/emulator\_x86\_64/system/framework/framework.jar  
    adb push framework.jar /system/framework/
    
    模块：`make framework-res`  
    代码路径：frameworks\\base\\core\\res  
    生成包目录：out/target/product/emulator\_x86\_64/system/framework/framework-res.apk  
    adb push framework-res.apk /system/framework/  
    **注**：建议先编译framework.jar，再编译该模块，因为framework-res.apk依赖framework.jar
    
    模块：`make inputflinger`  
    代码路径：frameworks/native/services/inputflinger  
    生成包目录：  
    out/target/product/emulator\_x86\_64/system/lib/libinputflinger.so  
    out/target/product/emulator\_x86\_64/system/lib64/libinputflinger.so  
    adb push libinputflinger.so system/lib  
    adb push libinputflinger.so system/lib64
    

**注**：单仓编译后记得push到对应得文件目录中，重启生效

### image镜像文件编译

生成手机镜像  
命令格式：`make 需要生成的镜像模块image`  
快速生成手机镜像  
命令格式：`make 需要生成的镜像模块image-nodeps`

生成 系统镜像  
`make systemimage`  
快速生成系统镜像  
`make systemimage-nodeps`  
生成镜像目录：out/target/product/emulator\_x86\_64

从Makefile中可以看出出了会生成基本 system .image镜像文件而且还会生成system-qeme.image镜像文件，system-qeme.image是适用于模拟器的镜像文件  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/0fc4f33e33e5b27edb0633a6562e46a3.png)  
关于其他镜像的的生成Makefile中也有写，文件路径在  
`AOSP源码根目录/build/core/Makefile`和`AOSP源码根目录/build/core/main.mk`

**注**：`make systemimage`本质是拷贝已经生成的文件打包成system.img文件。如果代码有修改，则需先编译修改代码的模块，再去生成镜像文件

### 其他编译命令

```swift
init 
make init 
mmm system/core/init

zygote 
make app_process 
mmm frameworks/base/cmds/app_process

system_server 
make services 
mmm frameworks/base/services

java framework 
make framework 
mmm frameworks/base

framework资源
make framework-res 
mmm frameworks/base/core/res

jni framework 
make libandroid_runtime
mmm frameworks/base/core/jni

binder
make libbinder
mmm frameworks/native/libs/binderbash123456789101112131415161718192021222324252627
```

## 编译报错

-   lunch 命令报错  
    运行  
    `source build/envsetup.sh`  
    `lunch sdk_phone_x86_64`
    
    ```sql
    Invalid lunch combo: sdk_phone_x86-userdebug                                                                            
    Valid combos must be of the form <product>-<release>-<variant> when using                                               
    the legacy format.  Run 'lunch --help' for usage.bash123
    ```
    
    或者直接运行lunch不显示列表
    
    运行  
    `source build/envsetup.sh`  
    `list_products`  
    查看对应的`lunch`  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/d22e70f4f79c41f9957b90580ecd3b10.png)  
    模拟器一般选择 `sdk_phone64_x86_64` 即可。
    
-   内存不足导致报错
    
    ```bash
    FAILED: out/soong/build.ninja                                                                                           
    cd "$(dirname "out/host/linux-x86/bin/soong_build")" && BUILDER="$PWD/$(basename "out/host/linux-x86/bin/soong_build")" && cd / && env -i  "$BUILDER"     --top "$TOP"     --soong_out "out/soong"     --out "out"     -o out/soong/build.ninja --bazel-mode --globListDir build --globFile out/soong/globs-build.ninja -t -l out/.module_paths/Android.bp.list --available_env out/soong/soong.environment.available --used_env out/soong/soong.environment.used.build Android.bp              
    Killed                                                                                                                  
    19:47:22 soong bootstrap failed with: exit status 1                                                                     
    
    #### failed to build some targets (03:13 (mm:ss)) ####bash123456
    ```
    
    增大交换空间即可，例如:
    
    ```bash
    sudo swapoff /swapfile          # deactivate if it's swap
    sudo rm /swapfile               # delete the old file
    sudo fallocate -l 8G /swapfile  # create new 8GB file
    sudo chmod 600 /swapfile
    sudo mkswap /swapfile
    sudo swapon /swapfile
    echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstabbash1234567
    ```
    
-   提示：`ninja failed with: exit status 137`
    
    查看文件`gedit build/soong/java/config/config.go`  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/446ff7b2ba09bf94aba9767740f65a50.png)  
    javacheap的值太小会被系统直接killed掉  
    `export MAVEN_OPTS="-Xms8192m -Xmx8192m"`  
    一般配置自身电脑内存值的一半
    
    修改后，记得清空out目录再编译，且是必须清空 ：`rm -rf out`
    
    编译后要查看配置是否有生效，可以查看此文件：`out/soong/build.ninja` ，在此文件内搜索**JavacHeapSize**，看此值是否为你设置的值
    
-   提示：`FAILED: out/target/product/emu64x/super.img......Not enough space on device for partition vendor with size 104181760`
    
    ```ruby
    FAILED: out/target/product/emu64x/super.img
    /bin/bash -c "(mkdir -p out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/ ) && (rm -rf  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"use_dynamic_partitions=true\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"lpmake=lpmake\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"build_super_partition=true\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"build_super_empty_partition=true\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"super_metadata_device=super\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"super_block_devices=super\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"super_super_device_size=1895825408\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"dynamic_partition_list=product system system_dlkm system_ext vendor\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"super_partition_groups=emulator_dynamic_partitions\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"super_emulator_dynamic_partitions_group_size=1887436800\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt;  echo \"super_emulator_dynamic_partitions_partition_list=system system_dlkm system_ext product vendor\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"build_non_sparse_super_partition=true\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"super_partition_size=1895825408\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"virtual_ab_cow_version=3\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (echo \"system_image=out/target/product/emu64x/system.img\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt;  echo \"system_dlkm_image=out/target/product/emu64x/system_dlkm.img\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt;  echo \"system_ext_image=out/target/product/emu64x/system_ext.img\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt;  echo \"product_image=out/target/product/emu64x/product.img\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt;  echo \"vendor_image=out/target/product/emu64x/vendor.img\" >>  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt ) && (mkdir -p out/target/product/emu64x/ ) && (PATH=out/host/linux-x86/bin/:\$PATH out/host/linux-x86/bin/build_super_image -v  out/target/product/emu64x/obj/PACKAGING/superimage_debug_intermediates/misc_info.txt out/target/product/emu64x/super.img )"
    2026-05-23 15:13:36 - build_super_image.py - INFO    : Building super image from info dict...
    2026-05-23 15:13:36 - common.py - INFO    :   Running: "/home/ts/AOSP16_R4/aosp/out/host/linux-x86/bin/lpmake --metadata-size 65536 --super-name super --metadata-slots 2 --device super:1895825408 --group emulator_dynamic_partitions:1887436800 --partition system:readonly:989376512:emulator_dynamic_partitions --image system=out/target/product/emu64x/system.img --partition system_dlkm:readonly:8077312:emulator_dynamic_partitions --image system_dlkm=out/target/product/emu64x/system_dlkm.img --partition system_ext:readonly:385454080:emulator_dynamic_partitions --image system_ext=out/target/product/emu64x/system_ext.img --partition product:readonly:451473408:emulator_dynamic_partitions --image product=out/target/product/emu64x/product.img --partition vendor:readonly:104181760:emulator_dynamic_partitions --image vendor=out/target/product/emu64x/vendor.img --output out/target/product/emu64x/super.img"
    2026-05-23 15:13:36 - common.py - INFO    : 05-23 15:13:36.273 1468898 1468898 I lpmake  : builder.cpp:1027 [liblp] Partition system will resize from 0 bytes to 989376512 bytes
    05-23 15:13:36.273 1468898 1468898 I lpmake  : builder.cpp:1027 [liblp] Partition system_dlkm will resize from 0 bytes to 8077312 bytes
    05-23 15:13:36.273 1468898 1468898 I lpmake  : builder.cpp:1027 [liblp] Partition system_ext will resize from 0 bytes to 385454080 bytes
    05-23 15:13:36.273 1468898 1468898 I lpmake  : builder.cpp:1027 [liblp] Partition product will resize from 0 bytes to 451473408 bytes
    05-23 15:13:36.273 1468898 1468898 E lpmake  : builder.cpp:609 [liblp] Partition vendor is part of group emulator_dynamic_partitions which does not have enough space free (104181760 requested, 1834381312 used out of 1887436800)
    Not enough space on device for partition vendor with size 104181760
    2026-05-23 15:13:36 - build_super_image.py - ERROR   :
       ERROR:
    Traceback (most recent call last):
      File "build_super_image.py", line 206, in <module>
      File "build_super_image.py", line 200, in main
      File "build_super_image.py", line 185, in BuildSuperImage
      File "build_super_image.py", line 132, in BuildSuperImageFromDict
      File "common.py", line 328, in RunAndCheckOutput
    common.ExternalError: Failed to run command '['lpmake', '--metadata-size', '65536', '--super-name', 'super', '--metadata-slots', '2', '--device', 'super:1895825408', '--group', 'emulator_dynamic_partitions:1887436800', '--partition', 'system:readonly:989376512:emulator_dynamic_partitions', '--image', 'system=out/target/product/emu64x/system.img', '--partition', 'system_dlkm:readonly:8077312:emulator_dynamic_partitions', '--image', 'system_dlkm=out/target/product/emu64x/system_dlkm.img', '--partition', 'system_ext:readonly:385454080:emulator_dynamic_partitions', '--image', 'system_ext=out/target/product/emu64x/system_ext.img', '--partition', 'product:readonly:451473408:emulator_dynamic_partitions', '--image', 'product=out/target/product/emu64x/product.img', '--partition', 'vendor:readonly:104181760:emulator_dynamic_partitions', '--image', 'vendor=out/target/product/emu64x/vendor.img', '--output', 'out/target/product/emu64x/super.img']' (exit code 70):
    05-23 15:13:36.273 1468898 1468898 I lpmake  : builder.cpp:1027 [liblp] Partition system will resize from 0 bytes to 989376512 bytes
    05-23 15:13:36.273 1468898 1468898 I lpmake  : builder.cpp:1027 [liblp] Partition system_dlkm will resize from 0 bytes to 8077312 bytes
    05-23 15:13:36.273 1468898 1468898 I lpmake  : builder.cpp:1027 [liblp] Partition system_ext will resize from 0 bytes to 385454080 bytes
    05-23 15:13:36.273 1468898 1468898 I lpmake  : builder.cpp:1027 [liblp] Partition product will resize from 0 bytes to 451473408 bytes
    05-23 15:13:36.273 1468898 1468898 E lpmake  : builder.cpp:609 [liblp] Partition vendor is part of group emulator_dynamic_partitions which does not have enough space free (104181760 requested, 1834381312 used out of 1887436800)
    Not enough space on device for partition vendor with size 104181760bash12345678910111213141516171819202122232425
    ```
    
    这里以`lunch sdk_phone64_x86_64` 为例子  
    找到相关配置`grep -R "BOARD_SUPER_PARTITION_SIZE" device/`  
    修改`device/generic/goldfish/64bitonly/product/sdk_phone64_x86_64.mk`中的配置
    
    ```shell
    #BOARD_EMULATOR_DYNAMIC_PARTITIONS_SIZE ?= $(shell expr 1800 \* 1048576 )
    #BOARD_SUPER_PARTITION_SIZE := $(shell expr $(BOARD_EMULATOR_DYNAMIC_PARTITIONS_SIZE) + 8388608 )  # +8M
    BOARD_EMULATOR_DYNAMIC_PARTITIONS_SIZE := 8472496640
    BOARD_SUPER_PARTITION_SIZE := 8589934592
    ```
    
    之后删除缓存
    
    ```bash
    rm -rf out/target/product/emu64x/super.img
    rm -rf out/target/product/emu64x/obj/PACKAGING/superimage*
    rm -rf out/target/product/emu64x/obj/PACKAGING/misc_info.txt
    ```
    
    最后重新编译（`make`）即可。
    
-   提示：`ninja: no work to do.`  
    当编译结束后，却提示ninja: no work to do. ，且编译out目录没有编译产物，则可以执行make clean后再重新编译
    
-   编译时，命令提示框突然闪退  
    可能是由于电脑内存不足导致，可以尝试清理系统内存的缓存在编译试试
    
    ```bash
    sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
    ```
    
-   车载命令`lunch sdk_car_md_x86_64`编译报错
    
    ```vbnet
    # 出现如下错误
    internal error: Device makefile has PRODUCT_SYSTEM_PROPERTIES or PRODUCT_SYSTEM_
    DEFAULT_PROPERTIES that add properties to the 'system' partition, which is again
    st packages/services/Car/car_product/build/car_generic_system.mk's artifact path
     requirement.
    Please use PRODUCT_PRODUCT_PROPERTIES or PRODUCT_SYSTEM_EXT_PROPERTIES to add th
    em to a different partition instead.
    Offending entries:
        com.android.car.internal.debug.num_auto_populated_users=1
        cppd.connectvhal.Timeoutmillis=60000
        persist.sys.max_profiles=5
        ro.vendor.simulateMultiZoneAudio=true
    
    09:26:36 soong bootstrap failed with: exit status 1
    ```
    
    AOSP 16 的车机构建脚本 (car\_generic\_system.mk) 强制禁止向 system 分区写入属性。设备配置文件中使用了 PRODUCT\_SYSTEM\_PROPERTIES 或 PRODUCT\_SYSTEM\_DEFAULT\_PROPERTIES 添加了 cppd.connectvhal.Timeoutmillis=60000，触发了分区隔离策略校验。  
    需要把 `PRODUCT_SYSTEM_PROPERTIES 改为 PRODUCT_PRODUCT_PROPERTIES 或者 PRODUCT_SYSTEM_EXT_PROPERTIES，以及PRODUCT_SYSTEM_DEFAULT_PROPERTIES 改为 PRODUCT_PRODUCT_PROPERTIES 或者 PRODUCT_SYSTEM_EXT_PROPERTIES`
    
    可以通过`grep -R "PRODUCT_SYSTEM_PROPERTIES" device/generic/car` 或者 `grep -R "PRODUCT_SYSTEM_DEFAULT_PROPERTIES" device/generic/car`找到需要修改的文件，修改如下：
    
    ```shell
    # 需要对如下位置进行修改
    # device/generic/car/emulator/car_emulator_vendor.mk:62:# PRODUCT_SYSTEM_PROPERTIES += cppd.connectvhal.Timeoutmillis=60000 ==>PRODUCT_PRODUCT_PROPERTIES
    # device/generic/car/common/car.mk:26:# PRODUCT_SYSTEM_PROPERTIES += cppd.connectvhal.Timeoutmillis=60000 ==>PRODUCT_PRODUCT_PROPERTIES
    # device/generic/car/common/car_md.mk:43:# PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \ ==>PRODUCT_PRODUCT_PROPERTIES
    # 不使用cuttlefish模拟器可以不改
    # device/google/cuttlefish/shared/auto/device_vendor.mk:94:# PRODUCT_SYSTEM_PROPERTIES += cppd.connectvhal.Timeoutmillis=60000 ==>PRODUCT_PRODUCT_PROPERTIES
    # device/google/cuttlefish/vsoc_x86_64_only/auto_md/aosp_cf.mk:38:# PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \ ==>PRODUCT_PRODUCT_PROPERTIES
    ```
    
-   直接make全仓编译报错
    
    1.  `Out of space? Out of inodes? The tree size of out/target/product/emulator_x86_64/data is 1145001984 bytes (1091 M B), with reserved space of 0 bytes (0 MB). The max image size for filesystem files is 576716800 bytes (550 MB), out of a total partition size`
        
        我们需要在boardconfig.mk修改这个BOARD\_SYSTEMIMAGE\_PARTITION\_SIZE参数的数值。  
        因此在 项目根目录中的`device`目录下运行命令`find . -iname "boardconfig.mk"`
        
        ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/85a528ccf0b41982dcd4418eed374395.png)
        
        这里有很多我们要找到我们需要修改的那个文件  
        运行 你的编译初始化命令，例如：
        
        ```bash
        . build/envsetup.sh
        lunch sdk_phone_x86_64
        ```
        
        ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/7d28f74eb73c6d83bc21b6acbfa2e261.png)  
        发现`device/generic/goldfish`目录对应的就是上面我们查找的boardconfig.mk中所存放的目录，由于我们编译的是使用模拟器且为x86\_64，因此选择`generic/goldfish/emulator64_x86_64/BoardConfig.mk`  
        在文件中把数值调整为较大的值即可  
        ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/66c0760e7e4eef38c47f2be2ff4cd8f0.png)  
        如果没有BOARD\_SYSTEMIMAGE\_PARTITION\_SIZE参数，则需要添加一句`BOARD_SYSTEMIMAGE_PARTITION_SIZE := 2147483648`，之后重新编译。  
        如果还有报错，可以先`make clean`之后再编译。
        

2.  make -j8报错
    
    ```markdown
    error: libcrypto's ABI has EXTENDING CHANGES. Please check compatibility report at: out/soong/.intermediates/external/boringssl/libcrypto/android_vendor.34_x86_64_shared/libcrypto.so.abidiff                                                  
    ******************************************************                                                                  
    error: Please update ABI references with: $ANDROID_BUILD_TOP/development/vndk/tools/header-checker/utils/create_reference_dumps.py -l libcrypto                                                                                                 
    13:34:20 ninja failed with: exit status 1
    ```
    
    运行命令`$ANDROID_BUILD_TOP/development/vndk/tools/header-checker/utils/create_reference_dumps.py -l libcrypto`后  
    直接重新`make -j8`即可

## 模拟器报错

-   模拟器运行报错
    
    ```vbnet
    ERROR   | Running multiple emulators with the same AVD 
    ERROR   | is an experimental feature.
    ERROR   | Please use -read-only flag to enable this feature
    ```
    
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/01ac84b8125871e5a32d6c4042ba7c76.png)在源码目录下android-13.0.0\_r43/out/target/product/emulator\_x86\_64找到cache.img和cache.img.qcow2，删除即可。  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/660d090fffe9ea746fb40175de50c0b7.png)或者直接杀死对应进程  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/6c21ee572c615b1fc5659f63945094b4.png)
    
-   用户组报错
    
    ```swift
    ERROR        | x86_64 emulation currently requires hardware acceleration!
    CPU acceleration status: This user doesn't have permissions to use KVM (/dev/kvm).
    The KVM line in /etc/group is: [kvm:x:109:ts]
    
    If the current user has KVM permissions,
    the KVM line in /etc/group should end with ":" followed by your username.
    
    If we see LINE_NOT_FOUND, the kvm
    More info on configuring VM acceleration on Linux:
    https://developer.android.com/studio/run/emulator-acceleration#vm-linux
    General information on acceleration: https://developer.android.com/studio/run/emulator-acceleration.
    ```
    
    把当前用户加入到**kvm**用户组即可，`sudo usermod -aG kvm $USER`，之后重启，使用`groups`命令就可以看到**kvm**
    

## 导入源码至android studio

可以不看1-5步，直接解压文件【[idegen生成的工程文件](https://download.csdn.net/download/yimelancholy/89517374)  
】，放至源码根目录下使用

1.  进入源码根目录，初始化系统环境  
    `source build/envsetup.sh`
    
2.  源码根目录执行如下命令 ，生成idegen.jar  
    `mmm development/tools/idegen/`
    
3.  源码根目录下执行  
    `sudo development/tools/idegen/idegen.sh`
    
4.  修改权限  
    `sudo chmod 777 android.iml`  
    `sudo chmod 777 android.ipr`
    
5.  过滤模块  
    使用文本编辑器打开**android.iml**文件，搜索关键字"excludeFolder "，把不需要加载的模块添加到此处  
    我这里保留了**fraweworks、vender、packages、system、sdk**模块，其他过滤掉，参考如下：
    
    ```kotlin
    <excludeFolder url="file://$MODULE_DIR$/./external/emma"/>
    <excludeFolder url="file://$MODULE_DIR$/./external/jdiff"/>
    <excludeFolder url="file://$MODULE_DIR$/out/eclipse"/>
    <excludeFolder url="file://$MODULE_DIR$/.repo"/>
    <excludeFolder url="file://$MODULE_DIR$/external/bluetooth"/>
    <excludeFolder url="file://$MODULE_DIR$/external/chromium"/>
    <excludeFolder url="file://$MODULE_DIR$/external/icu4c"/>
    <excludeFolder url="file://$MODULE_DIR$/external/webkit"/>
    <excludeFolder url="file://$MODULE_DIR$/frameworks/base/docs"/>
    <excludeFolder url="file://$MODULE_DIR$/out/host"/>
    <excludeFolder url="file://$MODULE_DIR$/out/target/common/docs"/>
    <excludeFolder url="file://$MODULE_DIR$/out/target/common/obj/JAVA_LIBRARIES/android_stubs_current_intermediates"/>
    <excludeFolder url="file://$MODULE_DIR$/out/target/product"/>
    <excludeFolder url="file://$MODULE_DIR$/prebuilt"/>
    <excludeFolder url="file://$MODULE_DIR$/art" />
    <excludeFolder url="file://$MODULE_DIR$/bionic" />
    <excludeFolder url="file://$MODULE_DIR$/bootable" />
    <excludeFolder url="file://$MODULE_DIR$/build" />
    <excludeFolder url="file://$MODULE_DIR$/compatibility" />
    <excludeFolder url="file://$MODULE_DIR$/dalvik" />
    <excludeFolder url="file://$MODULE_DIR$/cts" />
    <excludeFolder url="file://$MODULE_DIR$/developers" />
    <excludeFolder url="file://$MODULE_DIR$/developers/samples" />
    <excludeFolder url="file://$MODULE_DIR$/development" />
    <excludeFolder url="file://$MODULE_DIR$/device" />
    <excludeFolder url="file://$MODULE_DIR$/devices" />
    <excludeFolder url="file://$MODULE_DIR$/docs" />
    <excludeFolder url="file://$MODULE_DIR$/external" />
    <excludeFolder url="file://$MODULE_DIR$/flashing-files" />
    <excludeFolder url="file://$MODULE_DIR$/hardware" />
    <excludeFolder url="file://$MODULE_DIR$/kernel" />
    <excludeFolder url="file://$MODULE_DIR$/libcore" />
    <excludeFolder url="file://$MODULE_DIR$/libnativehelper" />
    <excludeFolder url="file://$MODULE_DIR$/out" />
    <excludeFolder url="file://$MODULE_DIR$/pdk" />
    <excludeFolder url="file://$MODULE_DIR$/platform_testing" />
    <excludeFolder url="file://$MODULE_DIR$/prebuilt" />
    <excludeFolder url="file://$MODULE_DIR$/prebuilts" />
    <excludeFolder url="file://$MODULE_DIR$/shortcut-fe" />
    <excludeFolder url="file://$MODULE_DIR$/test" />
    <excludeFolder url="file://$MODULE_DIR$/toolchain" />
    <excludeFolder url="file://$MODULE_DIR$/tools" />
    ```
    
6.  导入源码  
    Android Studio 打开工程源码下的 **android.ipr** 文件，此时就开始加载代码  
    如果导入有异常，需要删除根目录下的缓存文件**android.iws**，直接重新加载**android.ipr** 文件，或者按上面的步骤重新生成**android.ipr** 文件加载即可。
    
7.  调整Modules  
    打开源码跳转时可能会出现加载异常的情况  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b67bd9d1c8d0cd976b72ec80a12a25b2.png)我们打开File->Project Structure  
    ![List item](https://i-blog.csdnimg.cn/blog_migrate/016e208b98d2dd933a916257abca598e.png)选中Module source，按住Alt+↑，使其到最顶部
    

## repo 切换分支

1.  查看可切换的分支  
    例如查看android14相关分支
    
    ```bash
    cd .repo/manifests
    git branch -a |cut -d / -f 3 | grep android14
    ```
    
    效果如图  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/3783cace6c8e480f881eca4a8f71cd40.png)
    
2.  选择切换的版本  
    例如选择android-14.0.0\_r7
    
    ```shell
    #切换版本
    repo init -b android-14.0.0_r7
    # 本地是最新代码可以不用同步repo sync 
    repo start android-14.0.0_r7 --all
    # 如果上面命令有报错就直接repo sync
    repo sync
    # 查看当前的分支   
    repo branches
    ```
    

## 交换空间swap配置（可选）

编译过程中可能会有FAILED: out/soong/ build .ninja这类报错，可以配置swap之后编译

1.创建要作为swap分区的文件:增加count大小的交换分区，则命令写法如下，其中的count等于想要的块的数量（bs\*count=文件大小）。  
`dd if=/dev/zero of=/var/swapfile bs=1024 count=16777216`  
这里count的值一般和自身电脑内存一致，我这里是1024 x 1024 x 16 = 16G

2.格式化为交换分区文件:  
`mkswap /var/swapfile`  
建立swap的文件系统

3.修改权限  
`chmod -R 0600 /var/swapfile`

4.启用交换分区文件:  
`swapon /var/swapfile`  
启用swap文件

如果启用swap文件报错时，提示：_swapon 失败：设备或资源忙_  
可以先关闭swap文件`swapoff /var/swapfile`，之后在用swapon开启

5.使系统开机时自启用，在文件/etc/fstab中添加一行：  
`vi /etc/fstab`  
进入文件  
`/var/swapfile swap swap defaults 0 0`  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/ff497482c35bbaf303dd095e85ea3250.png#pic_center)  
注：  
1.权限不足记得在命令前面加上**sudo**  
2.其中 `/var/swapfile`为自定义交换分区路径,可以按照自己喜好命名

## 网络工具（可选）

[网络工具](https://ikuuu.co/)  
[备用](https://ikuuu.win/)

其中 Linux 教程的第三步  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/69cf18da64b24c279ebe316a920f0ed2.png)  
可能出现如下保报错  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/5d1a2633faf94af18681c40c5462f5f4.png)  
这是网络原因导致无法下载**Country.mmdb**文件  
可以下载离线文件[ikuuu vpn相关资源](https://download.csdn.net/download/yimelancholy/90477096) 包含Country.mmdb

## 其他

### 1.截图

截图工具安装`sudo apt install flameshot` ,运行`flameshot gui` 截图

### 2.解决**apt-get**安装中的`E: Sub-process /usr/bin/dpkg returned an error code (1)`问题

```bash
cd /var/lib/dpkg/
sudo mv info/ info_bak          # 现将info文件夹更名
sudo mkdir info                 # 再新建一个新的info文件夹
sudo apt-get update             # 更新
sudo apt-get -f install         # 修复
sudo mv info/* info_bak/        # 执行完上一步操作后会在新的info文件夹下生成一些文件，现将这些文件全部移到info_bak文件夹下
sudo rm -rf info                # 把自己新建的info文件夹删掉
sudo mv info_bak info           # 把以前的info文件夹重新改回名
```

### 3.安装SQLite3

```bash
sudo apt-get install sqlite3 #安装数据库系统Sqlite3
sudo apt-get install libsqlite3-dev #安装Sqlite3数据库开发支持库
sudo apt-get install sqlitebrowser #安装Sqlite3图形化管理界面DB Browser for SQLite
sqlite3 --version #检查是否安装成功，如果成功，输入以下命令可显示SQLite3版本信息
```

### 4.关于Android Studio中的模拟器

想要把文件push到模拟器中，但是设备无法remount

-   检查环境变量是否配置

```bash
sudo vi /etc/profile
ANDROID_HOME="/Users/Android/sdk"
export PATH="$ANDROID_HOME/emulator:$ANDROID_HOME/tools:$ANDROID_HOME/tools/bin:$ANDROID_HOME/cmdline-tools/latest:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH"

source /etc/profile
echo $ANDROID_HOME
```

-   运行模拟器

```bash
# 进入到到模拟器的目录
cd /Users/Android/sdk/emulator/
# 查看模拟器名字
./emulator -list-avds
./emulator '@模拟器名字' -writable-system
```

## 参考

`https://mirrors.tuna.tsinghua.edu.cn/help/AOSP/`  
`https://lug.ustc.edu.cn/wiki/mirrors/help/aosp/`  
`https://blog.csdn.net/longji/article/details/160203502`