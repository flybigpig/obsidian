linux 安装gcc(编译安装)

https://blog.csdn.net/qq_46662753/article/details/135371472

ubuntu
使用的ubuntu版本22
gcc 版本：12.1.0
各个版本的安装过程类似。
gcc各个版本的下载目录地址：gcc各个版本
gcc 12.1.0 下载地址
首选确保构建工具全部安装完毕

```
sudo apt install build-essential manpages-dev software-properties-common gcc g++ make bison binutils gcc-multilib flex
```




```
cd software # 这列切换到自己的下载目录
wget https://ftp.tsukuba.wide.ad.jp/software/gcc/releases/gcc-12.1.0/gcc-12.1.0.tar.gz

tar xf gcc-12.1.0.tar.gz # 解压包
cd gcc-12.1.0 # 进入解压的目录
./contrib/download_prerequisites
mkdir build # 这里创建build 目录以进行编译
cd build
../configure # 这里使用默认安装
make -j4 # 可以省略-j8,数字参考机器的核数（过大内存溢出）, 这里需要时间很长
make install # 安装到了/usr/local/bin/
gcc --version # 查看版本号
```




配置非默认安装和环境变量（默认安装环境变量可不配置，其已经在/usr/local/bin下不需要再配置）参考 …/configure参数配置