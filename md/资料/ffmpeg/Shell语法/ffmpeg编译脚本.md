#### 问题1：ERROR:srt>=1.3.0 not found using pkg-config

![生成编译文件报错](https://img-blog.csdnimg.cn/20200715111123275.png)

##### 问题描述：

无法根据配置生成[Makefile](https://so.csdn.net/so/search?q=Makefile&spm=1001.2101.3001.7020)文件

查看 ffbuild/config.log文件，发现编译配置使能了libsrt，libsrt需要用到OpenSSL相关动态库文件，未指定库文件，编译时找不到OpenSSL库;另外可能的原因是找不到对应的库的路径，要确保指定的库的相关路径正确，检查核对–extra-cflags="-I…" --extra-ldflags="-L/…"这两条是否正确。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20200715111656201.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3dlaXhpbl80MjI2NTUxMQ==,size_16,color_FFFFFF,t_70)


解决方案
通过–extra-libs 指定相关库文件


问题2：C compiler test failed.
 

#### 问题2：C compiler test failed.

![在这里插入图片描述](https://img-blog.csdnimg.cn/20200827211935531.png#pic_center)

##### 问题描述：

./configure 配置Makefile失败，无法生成Makefile

原因
通过ffbuild/config.log日志确认，原因为编译器找不到x264的库文件导致；

解决方案
1、找到库路径
2 /etc/ld.so.conf.d/文件夹下新增一个文件 文件名.conf
3、新增路径
4、保存退出
5、输入命令sudo ldconfig

#### 通过以上操作，系统编译时就会增加相应路径下检索相关库文件

问题3："/usr/local/include" is unsafe for cross-compilation![编译告警](https://img-blog.csdnimg.cn/20200715110218678.png)

问题描述：
编译过程中，一直提示连接不安全

原因
生成的ffbuild/config.mak文件中 指定头文件（.h文件）的路径CFLAGS里包含/usr/local/include。
之后深究其中问题，发现是自己之前为主机配置环境变量CFLAGS包含了/usr/local/include相关路径;
 