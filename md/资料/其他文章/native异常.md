基于[google/breakpad](https://link.juejin.cn?target=https%3A%2F%2Fgithub.com%2Fgoogle%2Fbreakpad)的Android Native 异常捕获库，在native层发生异常时java层能得到相关异常信息。



# 现状

- 发生native异常时，安卓系统会将native异常信息输出到logcat中，但是java层无法感知到native异常的发生，进而无法获取这些异常信息并上报到业务的异常监控系统。
- 业务部门可以快速实现java层的异常监控系统（java层全局异常捕获的实现很简单），又或者业务部门已经实现了java层的异常监控系统，但没有覆盖到native层的异常捕获。
- 安卓还可以接入Breakpad，其导出的minidump文件不仅体积小信息还全，但有两个问题：
  - 1.和现状第1点的问题相同。
  - 2.：需要拉取minidump文件并经过比较繁琐的步骤才可以得出有用的信息：
    - 启动时检测Breakpad是否有导出过minidump文件，有则说明发生过native异常。
    - 到客户现场，或者远程拉取minidump文件。
    - 编译出自己电脑的操作系统的minidump_stackwalk工具。
    - 使用minidump_stackwalk工具翻译minidump文件内容，例如拿到崩溃时的程序计数器寄存器内的值（下文称为pc值）。
    - 找到对应崩溃so库ABI的add2line工具，并根据上一步拿到的pc值定位出发生异常的代码行数。

整个步骤十分复杂和繁琐，且没有java层的crash线程栈信息，不利于java开发者快速定位调用native的代码。

# 设计意图

1. 让java层有知悉native异常的通道：
   - java开发者可以在java代码中得到native异常的情况，进而对native异常做出反应，而不是再次启动后去检测Breakpad是否有导出过minidump文件。
2. 增加信息的可用性，进而提升问题分析的效率：
   - 回调中提供naive异常信息、naive和java调用栈信息和minidump文件文件路径，这些信息可以直接通过业务部门的异常监控系统上报。
   - 划分为两个阶段解决问题，我预想是大部分都在阶段一解决了问题，而不需要再对minidump文件进行分析，总体来讲是提升了分析效率的：
     - 阶段一：有了java的调用栈和native的调用栈信息，大部分异常原因都可以快速定位并分析出来。
     - 阶段二：回调中也会提供minidump文件的存储路径，业务部门可以按需拉取。（这一步需要业务部门本身有拉取日志的功能，且需要按上文”现状部分进行操作”，较费时费力）
3. 最少改动：
   - 让接入方不因为引入新功能而大量改动现有代码。例如：在native崩溃回调处,使用现有的java层异常监控系统上报native异常信息。
4. 单一职责：
   - 只做native的crash捕获，不做系统内存情况、cpu使用率、系统日志等信息的采集功能。

# 整体流程

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/87876e4f5fff41b9aeaccfd11ae569fc~tplv-k3u1fbpfcp-zoom-in-crop-mark:1304:0:0:0.awebp?)

# 功能介绍

- 保留breakpad导出minidump文件功能 （可选择是否启用）
- 发生native异常时将异常信息、native层调用栈、java层的调用栈通过回调提供给开发者，将这些信息输出到控制台的效果如下：

```bash
2022-02-14 11:33:08.598 30228-30253/com.babyte.banativecrash E/crash:  
    /data/user/0/com.babyte.banativecrash/cache/f1474006-60ca-40f4-c9d8e89a-47e90c2e.dmp
2022-02-14 11:33:08.599 30228-30253/com.babyte.banativecrash E/crash:  
    Operating system: Android 28 Linux 4.4.146 #37 SMP PREEMPT Wed Jan 20 18:26:59 CST 2021
    CPU: aarch64 (8 core)
    
    Crash reason: signal 11(SIGSEGV) Invalid address
    Crash address: 0000000000000000
    Crash pc: 0000000000000650
    Crash so: /data/app/com.babyte.banativecrash-ptLzOQ_6UYz-W3Vgyact8A==/lib/arm64/libnative-lib.so(arm64)
    Crash method: _Z5Crashv
2022-02-14 11:33:08.602 30228-30253/com.babyte.banativecrash E/crash:  
    Thread[name:DefaultDispatch] (NOTE: linux thread name length limit is 15 characters)
    #00 pc 0000000000000650  /data/app/com.babyte.banativecrash-ptLzOQ_6UYz-W3Vgyact8A==/lib/arm64/libnative-lib.so (Crash()+20)
    #01 pc 0000000000000670  /data/app/com.babyte.banativecrash-ptLzOQ_6UYz-W3Vgyact8A==/lib/arm64/libnative-lib.so (Java_com_babyte_banativecrash_MainActivity_nativeCrash+20)
    #02 pc 0000000000565de0  /system/lib64/libart.so (offset 0xc1000) (art_quick_generic_jni_trampoline+144)
    #03 pc 000000000055cd88  /system/lib64/libart.so (offset 0xc1000) (art_quick_invoke_stub+584)
    #04 pc 00000000000cf740  /system/lib64/libart.so (art::ArtMethod::Invoke(art::Thread*, unsigned int*, unsigned int, art::JValue*, char const*)+200)
    #05 pc 00000000002823b8  /system/lib64/libart.so (offset 0xc1000) 
...
2022-02-14 11:33:08.603 30228-30253/com.babyte.banativecrash E/crash:  
    Thread[DefaultDispatcher-worker-1,5,main]
        at com.babyte.banativecrash.MainActivity.nativeCrash(Native Method)
        at com.babyte.banativecrash.MainActivity$onCreate$2$1.invokeSuspend(MainActivity.kt:39)
        at kotlin.coroutines.jvm.internal.BaseContinuationImpl.resumeWith(ContinuationImpl.kt:33)
        at kotlinx.coroutines.DispatchedTask.run(DispatchedTask.kt:106)
        at kotlinx.coroutines.scheduling.CoroutineScheduler.runSafely(CoroutineScheduler.kt:571)
        at kotlinx.coroutines.scheduling.CoroutineScheduler$Worker.executeTask(CoroutineScheduler.kt:750)
        at kotlinx.coroutines.scheduling.CoroutineScheduler$Worker.runWorker(CoroutineScheduler.kt:678)
        at kotlinx.coroutines.scheduling.CoroutineScheduler$Worker.run(CoroutineScheduler.kt:665)
    Thread[DefaultDispatcher-worker-2,5,main]
        at java.lang.Object.wait(Native Method)
        at java.lang.Thread.parkFor$(Thread.java:2137)
        at sun.misc.Unsafe.park(Unsafe.java:358)
        at java.util.concurrent.locks.LockSupport.parkNanos(LockSupport.java:353)
        at kotlinx.coroutines.scheduling.CoroutineScheduler$Worker.park(CoroutineScheduler.kt:795)
        at kotlinx.coroutines.scheduling.CoroutineScheduler$Worker.tryPark(CoroutineScheduler.kt:740)
        at kotlinx.coroutines.scheduling.CoroutineScheduler$Worker.runWorker(CoroutineScheduler.kt:711)
        at kotlinx.coroutines.scheduling.CoroutineScheduler$Worker.run(CoroutineScheduler.kt:665)
...
复制代码
```

## 定位到so中具体代码行示例

可以使用ndk中的add2line工具根据pc值和带符号信息的so库，定位出具体代码行数。

例：从上文的异常信息中可以看到abi是aarch64，对应的so库abi是arm64，所以add2line的使用如下：

```shell
$ ./ndk/android-ndk-r16b/toolchains/aarch64-linux-android-4.9/prebuilt/darwin-x86_64/bin/aarch64-linux-android-addr2line -Cfe ~/arm64-v8a/libnative-lib.so 0000000000000650
复制代码
```

输出结果如下：

```shell
Crash()
/Users/ba/AndroidStudioProjects/NativeCrash2Java/app/.cxx/cmake/debug/arm64-v8a/../../../../src/main/cpp/native-lib.cpp:6
复制代码
```



现在 Android 日常开发中，多多少少会用到 so 动态库，特别是一些第三方的 so 比如（地图 SDK，音视频 SDK）还有自研 SDK，不知道大家有没有想过这样的一个问题，用户反馈我们的 APP 崩溃，这个时候后台也没有收到客服端上报的具体日志，我们也不知道从哪里分析，这是最可怕的。如果有日志，一切就好办了，下面我们就来分析 Android 端怎么来捕获 native 崩溃信息。

## [框架使用](https://link..im/?target=https%3A%2F%2Fgithub.com%2Fyangkun19921001%2FYKCrash)

```
//1. 配置 project/build.gradle

allprojects {
		repositories {
			...
			maven { url 'https://jitpack.io' }
		}
	}
复制代码
//2. app/build.gradle

	dependencies {
	  implementation 'com.github.yangkun19921001:YKCrash:1.0.2'
	}
复制代码
//3. application 中初始化
//nativePath: 保存的 dmp 日志
//javaPath: 保存的 java 崩溃日志
//onCrashListener:  java 崩溃监听回调
//框架初始化
new Crash.CrashBuild(getApplicationContext())
                .nativeCrashPath(nativePath)
                .javaCrashPath(javaPath, this)
                .build();
复制代码
```

## 产生崩溃的原因

### 哪些情况会崩溃

1. Java 崩溃；

   Java 崩溃就是在 Java 代码中，出现了未捕获异常，导致程序异常退出。

2. native 崩溃；

   一般都是因为在 Native 代码中访问非法地址，也可能是地址对齐出现了问题，，或者发生了程序主动 abort , 这些都会产生相应的 signal 信号，导致程序异常退出。

3. ANR；

   1. 死锁；
   2. IO 问题；
   3. 主线程耗时操作；
   4. 频繁大量 GC.

## breakpad 编译及使用(MAC)

### 介绍

这里推荐大家使用 Google 开源 的 [Breakpad](https://link..im/?target=https%3A%2F%2Fgithub.com%2Fgoogle%2Fbreakpad), 为什么呢？因为它是目前 Native 崩溃捕获中最为成熟的方案，但是很多人都觉得 Breakpad 过于复杂。其实我认为 Native 崩溃捕获这件事儿就本来不容易。

如果你对 Native 崩溃机制的一些基本知识还不是很熟悉，可以看一下[Android 平台 Native 代码的崩溃捕获机制及实现](https://link..im/?target=https%3A%2F%2Fmp.weixin.qq.com%2Fs%2Fg-WzYF3wWAljok1XjPoo7w)

### **1. 获取 breakpad 源码**

- [官网获取](https://link..im/?target=https%3A%2F%2Fchromium.googlesource.com%2Fbreakpad%2Fbreakpad%2F%2B%2Fmaster)

  

  [![breakpad.jpg](https://user-gold-cdn.xitu.io/2019/9/18/16d4062f5ed08c23?imageView2/0/w/1280/h/960/format/webp/ignore-error/1)](https://link..im/?target=https%3A%2F%2Ffree.imgsha.com%2Fi%2F1EAoP)

  

- [GitHub 下载](https://link..im/?target=https%3A%2F%2Fgithub.com%2Fgoogle%2Fbreakpad)

### **2. 执行安装 breakpad**

```
1. cd breakpad 目录
2. 直接命令窗口输入：

./configure && make

复制代码
```

执行完之后会生成 src/processor/minidump_stackwalk 等文件，待会 dmp -> txt 会用到这个文件。

### **3. CMake** 编译源码



[![breakpad_build.jpg](https://user-gold-cdn.xitu.io/2019/9/18/16d4062f5e6876a9?imageView2/0/w/1280/h/960/format/webp/ignore-error/1)](https://link..im/?target=https%3A%2F%2Ffree.imgsha.com%2Fi%2F1Ewg1)



**build 配置**

```
apply plugin: 'com.android.library'

android {
		.....

    defaultConfig {
			....
        externalNativeBuild {
            cmake {
                cppFlags "-std=c++11"
            }
        }

        ndk {
            abiFilters "armeabi-v7a", "arm64-v8a", "x86"
        }
    }
		....

    externalNativeBuild {
        cmake {
            path "src/main/cpp/CMakeLists.txt"
        }
    }
}

dependencies {
    implementation fileTree(dir: 'libs', include: ['*.jar'])
}
复制代码
```

把 breakpad/src 源码导致 AS 中 [CMake](https://link..im/?target=https%3A%2F%2Fdeveloper.android.com%2Fndk%2Fguides%2Fcmake) 配置好之后直接在 AS/Build/make break-build 之后就能生成动态 so 库了。

注意：下载的源码缺少 `lss` 目录，可以点击[下载](https://link..im/?target=https%3A%2F%2Fgithub.com%2Fyangkun19921001%2FYKCrash)获取

### **4. 编写初始化 breakpad**

```
#include <stdio.h>
#include <jni.h>
#include <android/log.h>

#include "client/linux/handler/exception_handler.h"
#include "client/linux/handler/minidump_descriptor.h"

#define LOG_TAG "dodoodla_crash"

#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


bool DumpCallback(const google_breakpad::MinidumpDescriptor &descriptor,
                  void *context,
                  bool succeeded) {
    ALOGD("===============crrrrash================");
    ALOGD("Dump path: %s\n", descriptor.path());
    return succeeded;
}

/** java 代码中调用*/
extern "C"
JNIEXPORT void JNICALL
Java_com_devyk_crash_1module_CrashUtils_initBreakpadNative(JNIEnv *env, jclass type,
                                                           jstring path_) {
    const char *path = env->GetStringUTFChars(path_, 0);

    // TODO
    google_breakpad::MinidumpDescriptor descriptor(path);
    static google_breakpad::ExceptionHandler eh(descriptor, NULL, DumpCallback, NULL, true, -1);

    env->ReleaseStringUTFChars(path_, path);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}


复制代码
```

## 生成 dmp 文件并定位 crash

### 1. 在 app 模块中故意编写崩溃代码

```
/**
 * 引起 crash
 */
void Crash() {
    volatile int *a = (int *) (NULL);
    *a = 1;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_devyk_ykcrash_MainActivity_testCrash(JNIEnv *env, jclass type) {

    // TODO
    Crash();
}
复制代码
```

### 2. init native crash 捕获

```
//配置 native 崩溃捕获
CrashUtils.initCrash(String nativeCrashPath);
复制代码
```

### 3. 制造 Crash 并生成 xxx.dmp 文件



[![no1Exe.gif](https://user-gold-cdn.xitu.io/2019/9/18/16d4063030476706?imageslim)](https://link..im/?target=https%3A%2F%2Fimgchr.com%2Fi%2Fno1Exe)



这里看到了生成了的 xxx.dmp 文件，下面就需要将 dmp to txt 文件稍微我们能看的懂的。

### 4. dmp to txt

1. 将 breakpad/src/processor/minidump_stackwalk copy 到一个单独的文件下

2. 执行命令 to txt

   ```
   //格式 
   ./minidump_stackwalk xxx.dmp >xxx.txt
   
   //例子
   ./minidump_stackwalk /Users/devyk/Data/Project/sample/tempFile/nativeCrash.dmp >crashLog2.txt
   复制代码
   ```

   

   [![no3qcF.gif](https://user-gold-cdn.xitu.io/2019/9/18/16d406303fcc0310?imageslim)](https://link..im/?target=https%3A%2F%2Fimgchr.com%2Fi%2Fno3qcF)

   

3. 查看 txt 文件到底是什么？

   ```
   Operating system: Android
                     0.0.0 Linux 4.9.148 #1 SMP PREEMPT Wed Jun 26 04:38:26 CST 2019 aarch64
   CPU: arm64
        8 CPUs
   
   GPU: UNKNOWN
   
   Crash reason:  SIGSEGV /SEGV_MAPERR
   Crash address: 0x0
   Process uptime: not available
   
   //crash 发生线程
   Thread 0 (crashed)
     //这里的 libcrash-lib.so + 0x5f0 很重要。告诉了我们在哪个 so 发生崩溃，在具体哪个位置发生崩溃。这里先记住 0x5f0 这个值。
    0  libcrash-lib.so + 0x5f0 
        x0 = 0x00000078d4ac5380    x1 = 0x0000007fe01fd9d4
        x2 = 0x0000007fe01fda00    x3 = 0x00000078d453ecb8
        x4 = 0x0000000000000000    x5 = 0x00000078d4586b94
        x6 = 0x0000000000000001    x7 = 0x0000000000000001
        x8 = 0x0000000000000001    x9 = 0x0000000000000000
       x10 = 0x0000000000430000   x11 = 0x00000078d49396d8
       x12 = 0x000000795afcb630   x13 = 0x0ef1a811d0863271
       x14 = 0x000000795aede000   x15 = 0xffffffffffffffff
       x16 = 0x00000078b8cb5fe8   x17 = 0x00000078b8ca55dc
       x18 = 0x0000000000000000   x19 = 0x00000078d4a15c00
       x20 = 0x0000000000000000   x21 = 0x00000078d4a15c00
       x22 = 0x0000007fe01fdc90   x23 = 0x00000079552cb12a
       x24 = 0x0000000000000000   x25 = 0x000000795b3125e0
       x26 = 0x00000078d4a15ca0   x27 = 0x0000000000000000
       x28 = 0x0000007fe01fd9d0    fp = 0x0000007fe01fd9a0
        lr = 0x00000078b8ca5614    sp = 0x0000007fe01fd980
   复制代码
   ```

4. 基于 dmp to txt 里面的 libcrash-lib.so + 0x5f0 信息，转换为具体哪个函数，哪行报的错。

   1. 根据 txt 提示的信息 aarch64 CPU: arm64 那么我们就在当前使用的 NDK 版本找到 `/Users/devyk/Data/Android/NDK/android-ndk-r17c/toolchains/aarch64-linux-android-4.9/prebuilt/darwin-x86_64/bin/aarch64-linux-android-addr2line` 这个路径，使用下面的命令找到具体报错的地方

      ```
      //1. 格式
      aarch64-linux-android-addr2line -f -C -e [根据 txt 信息拿到具体报错的 so] [根据 txt 文件信息拿到具体报错的值]
      
      //2. 示例
      /Users/devyk/Data/Android/NDK/android-ndk-r17c/toolchains/aarch64-linux-android-4.9/prebuilt/darwin-x86_64/bin/aarch64-linux-android-addr2line -f -C -e /Users/devyk/Data/Project/sample/github_code/YKCrash/app/build/intermediates/transforms/mergeJniLibs/debug/0/lib/arm64-v8a/libcrash-lib.so 0x5f0
      复制代码
      ```

      下面录像就是输入命令之后的效果。

   

   [![noG7yF.gif](https://user-gold-cdn.xitu.io/2019/9/18/16d406302ed90da5?imageslim)](https://link..im/?target=https%3A%2F%2Fimgchr.com%2Fi%2FnoG7yF)

   

   根据上面录像我们拿到了重要信息如下：

   ```
   //报错的函数
   Crash()
   //当前 cpp 报错的行数
   /Users/devyk/Data/Project/sample/github_code/YKCrash/app/src/main/cpp/crash.cpp:10
   复制代码
   ```

   我们看下 cpp 10 行具体是什么。

   

   [![noJh0H.jpg](https://user-gold-cdn.xitu.io/2019/9/18/16d4062ffde2d785?imageView2/0/w/1280/h/960/format/webp/ignore-error/1)](https://link..im/?target=https%3A%2F%2Fimgchr.com%2Fi%2FnoJh0H)

   

   这里不用想，肯定会 crash。

   到这里我们已经捕获到了 native 层崩溃日志，相信大家已经会了。

## 总结

在之前我是真不知道 [breakpad](https://link..im/?target=https%3A%2F%2Fgithub.com%2Fgoogle%2Fbreakpad) 这个开源库，而且还是 Google 开源的。因为项目中用到 C++ 代码比较多，崩溃捕获是必须的。在不知道 [breakpad](https://link..im/?target=https%3A%2F%2Fgithub.com%2Fgoogle%2Fbreakpad) 之前一直用的 [腾讯 Bugly](https://link..im/?target=https%3A%2F%2Fbugly.qq.com%2Fv2%2F) ，由于Bugly 只支持互联网环境，最后我也就止步了，为什么呢？因为公司开发出来的产品是给政府部分用的（政府部门有专门的网络属于局域网)。