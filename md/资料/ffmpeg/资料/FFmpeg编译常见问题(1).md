# Clang编译FFmpeg常见问题

> 系统：Centos
> ffmpeg版本：4.2.3
> ndk版本：r19c
> 编译器：clang



## 1、命令找不到

错误信息：

```
./build_android.sh: line 18: --enable-shared: command not found
./build_android.sh: line 20: --disable-static: command not found
./build_android.sh: line 22: --disable-doc: command not found
./build_android.sh: line 24: --disable-ffmpeg: command not found
./build_android.sh: line 26: --disable-ffplay: command not found
./build_android.sh: line 28: --disable-ffprobe: command not found
./build_android.sh: line 30: --disable-ffserver: command not found
./build_android.sh: line 32: --disable-avdevice: command not found
```

解决：
**如果是直接copy网上的shell脚本，可能会是dos格式，请使用dos2unix build_android.sh 转换一下，删掉多余空格(这一点非常重要dos2unix 是一个工具，如果没有安装的话请先安装一下：brew install dos2unix ，很快就完事。**

## 2、xmakefile 文件没有生成

错误信息：

```
./android_config.sh: line 36: --enable-shared: command not found
Makefile:2: ffbuild/config.mak: No such file or directory
Makefile:40: /tools/Makefile: No such file or directory
Makefile:41: /ffbuild/common.mak: No such file or directory
Makefile:91: /libavutil/Makefile: No such file or directory
Makefile:91: /ffbuild/library.mak: No such file or directory
Makefile:93: /fftools/Makefile: No such file or directory
Makefile:94: /doc/Makefile: No such file or directory
Makefile:95: /doc/examples/Makefile: No such file or directory
Makefile:160: /tests/Makefile: No such file or directory
make: *** No rule to make target `/tests/Makefile'. Stop.
Makefile:2: ffbuild/config.mak: No such file or directory
Makefile:40: /tools/Makefile: No such file or directory
Makefile:41: /ffbuild/common.mak: No such file or directory
Makefile:91: /libavutil/Makefile: No such file or directory
Makefile:91: /ffbuild/library.mak: No such file or directory
Makefile:93: /fftools/Makefile: No such file or directory
Makefile:94: /doc/Makefile: No such file or directory
Makefile:95: /doc/examples/Makefile: No such file or directory
Makefile:160: /tests/Makefile: No such file or directory
```

解决：
**执行`./configure --disable-x86asm` 生成config.mak文件**

## 3、`arm-linxu-androideabi-gcc is unable to create an executable file`

错误信息：

```
/Users/aria/dev/android/sdk/ndk-bundle/toolchains/arm-linux-androideabi-4.9/prebuilt/darwin-x86_64/bin/arm-linux-androideabi-gcc is unable to create an executable file.
```

原因：
**检查ndk版本，android官方从`r18b`开始，已经移除了gcc这个编译工具详情见[ndk r18b修订内容](https://developer.android.com/ndk/downloads/revision_history?hl=zh-Cn)**
**解决：**
**使用clang进行编译，详情见：https://medium.com/@ilja.kosynkin/building-ffmpeg-4-0-for-android-with-clang-642e4911c31e**

## 4、`/android_config.sh: line 32: xxxxx No such file or directory`

原因：
`.configure`后面的命令不能有注释
![img](FFmpeg编译常见问题.assets/36152987.png)

解决：
删除注释的哪一行代码

## 5、`static declaration of 'xxx' follows non-static declaration`

解决：
config.h 搜索 lrint、lrintf、round、roundf 等对于的字符，将0修改为1

```
#define HAVE_LLRINT 1
#define HAVE_LLRINTF 1
#define HAVE_LRINT 1
#define HAVE_LRINTF 1
#define HAVE_ROUND 1
#define HAVE_ROUNDF 1
#define HAVE_CBRT 1
#define HAVE_CBRTF 1
#define HAVE_COPYSIGN 1
#define HAVE_TRUNC 1
#define HAVE_TRUNCF 1
#define HAVE_RINT 1
#define HAVE_HYPOT 1
#define HAVE_ERF 1
```

**或直接使用**`sed`来修改`config.h`文件

```
sed -i -e 's/#define HAVE_LLRINT 0/#define HAVE_LLRINT 1/g' config.h
sed -i -e 's/#define HAVE_LLRINTF 0/#define HAVE_LLRINTF 1/g' config.h
sed -i -e 's/#define HAVE_LRINT 0/#define HAVE_LRINT 1/g' config.h
sed -i -e 's/#define HAVE_LRINTF 0/#define HAVE_LRINTF 1/g' config.h
sed -i -e 's/#define HAVE_ROUND 0/#define HAVE_ROUND 1/g' config.h
sed -i -e 's/#define HAVE_ROUNDF 0/#define HAVE_ROUNDF 1/g' config.h
sed -i -e 's/#define HAVE_CBRT 0/#define HAVE_CBRT 1/g' config.h
sed -i -e 's/#define HAVE_CBRTF 0/#define HAVE_CBRTF 1/g' config.h
sed -i -e 's/#define HAVE_COPYSIGN 0/#define HAVE_COPYSIGN 1/g' config.h
sed -i -e 's/#define HAVE_TRUNC 0/#define HAVE_TRUNC 1/g' config.h
sed -i -e 's/#define HAVE_TRUNCF 0/#define HAVE_TRUNCF 1/g' config.h
sed -i -e 's/#define HAVE_RINT 0/#define HAVE_RINT 1/g' config.h
sed -i -e 's/#define HAVE_HYPOT 0/#define HAVE_HYPOT 1/g' config.h
sed -i -e 's/#define HAVE_ERF 0/#define HAVE_ERF 1/g' config.h
sed -i -e 's/#define HAVE_GMTIME_R 0/#define HAVE_GMTIME_R 1/g' config.h
sed -i -e 's/#define HAVE_LOCALTIME_R 0/#define HAVE_LOCALTIME_R 1/g' config.h
sed -i -e 's/#define HAVE_INET_ATON 0/#define HAVE_INET_ATON 1/g' config.h
```

## 6、` xxxxxxxxxx error: expected ')'`

错误信息：

```
#define getenv(x) NULL
^
/home/cd008/diska/android-ndk-r9/platforms/android-18/arch-arm/usr/include/stdlib.h:54:14: note: in expansion of macro 'getenv'
extern char *getenv(const char *);
^
./config.h:17:19: error: expected ')' before numeric constant
#define getenv(x) NULL
^
/home/cd008/diska/android-ndk-r9/platforms/android-18/arch-arm/usr/include/stdlib.h:54:14: note: in expansion of macro 'getenv'
extern char *getenv(const char *);
```

解决：
在config.h中注释掉#define getenv(x) NULL
/*#define getenv(x) NULL*/

```
sed -i -e 's/#define getenv(x) NULL/\/\*#define getenv(x) NULL\*\//g' config.h
```

## 7、`arm-linux-androideabi-ld -Wl,-soname,libavutil.so unknown option`

错误信息：

```
Users/aria/dev/android/sdk/ndk-bundle/toolchains/arm-linux-androideabi-4.9/prebuilt/darwin-x86_64/bin/arm-linux-androideabi-ld -Wl,-soname
```

原因：
gcc 构建 `.so` 的命令是 `-shared -wl,soname,xxxx.so` 而 clang 的是 `-shared -soname xxx.so`
![img](FFmpeg编译常见问题.assets/49102592.png)

解决：
修改`ffbuild/config.mak`文件，将`SHFLAGS=-shared -Wl,-soname,$(SLIBNAME)`修改为`SHFLAGS=-shared -soname $(SLIBNAME)`