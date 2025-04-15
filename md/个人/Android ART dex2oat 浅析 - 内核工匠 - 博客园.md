---
created: 2025-04-15T13:11:42 (UTC +08:00)
tags: []
source: https://www.cnblogs.com/Linux-tech/p/12961285.html
author: 内核工匠
---

# Android ART dex2oat 浅析 - 内核工匠 - 博客园

> ## Excerpt
> 一、什么是dex2oatDex2oat (dalvik excutable file to optimized art file) ，是一个对 dex 文件进行编译优化的程序，在我们的 Android 手机中的位...

---
**一、什么是dex2oat**

Dex2oat (dalvik excutable file to optimized art file) ，是一个对 dex 文件进行编译优化的程序，在我们的 Android 手机中的位置是 /system/bin/dex2oat，对应的源码路径为 android/art/dex2oat/dex2oat.cc，通过编译优化，可以提升用户日常的使用体验（包含安装速度、启动速度、应用使用过程中的流畅度等），是 Android Art Runtime 中的一个重要的模块， 本文我们一起来了解下 dex2oat 的功能以及常用的场景。

**二、为什么要进行dex2oat转换？**

众所周知， Android 虚拟机可以识别的是dex文件，应用使用过程中如果每次将dex文件加载进行内存，解释性执行字节码，效率会很低， 严重影响用户体验。通过dex2oat 优化后， 可以在系统运行之前利用合适的时机将dex文件字节码提前转化为虚拟机可以执行运行的机器码，后续直接从效率更高的机器码中运行，则运行阶段更加流畅，优化用户体验。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qT3FEWEZoNjhLaFFpY3NpYlVNVXJhVzFMdTJRTXZkaWJGeEtnR2VHVTBxc05WeHgwUThoNkRnalhGM2lhaWMyVlZ0WUNsSnFwcW44bHdxVFNRLzY0MA?x-oss-process=image/format,png)

Dex2oat的主要触发场景

**三、几种dex2oat 相关的文件**

**Dex文件：**Dex文件是Android 虚拟机识别的一种可执行文件，我们可以解压一个apk, 获取其中的class.dex文件， 通过dexdump 命令工具对dex 文件进行解析，查看文件内容，更多格式说明查看参考资料 1。

**Oat文件：**art执行的文件，dex2oat程序编译dex文件的产物。我们可以通过oatdump 查看oat文件具体内容。

**Odex文件:** Optimizied dexfile, dex文件已经dexopt操作优化后的产物，和dex文件类似，使用了一些优化操作码。

**Art文件：**Image文件，记录应用启动热点函数相关地址，方便寻址。

**Vdex文件：**Verified dex，主要包含dex和quicken info信息。Andorid 8.0新增机制产生的文件，其目的主要是为了跳过verified流程，减少dex2oat执行时间。

**四、如何使用Dex2oat**

**4.1  Dex2oat用法**

Dex2oat工具的常用参数如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X2pwZy9kNGhvWUpseE9qT3FEWEZoNjhLaFFpY3NpYlVNVXJhVzFMR2tNZDR2YXpSY2ozRDV2eUNGaWNtUng3akNkNnNJRzFpYmlhMmlhS3ZpYUlzQVN5ZHpKbkx2VzVrWEEvNjQw?x-oss-process=image/format,png)

**4.2  Dex2oat日志解析**

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTVRPekw3MDAwSHlBOTJBV0hZYmxlWVpuNHJQdUljOUVxTTZmZGliQXFuTDdCQWlhNVgyMjJpYTNnb0tGSW1QZVdOOTdkbWdKMVUyTjNvUS82NDA?x-oss-process=image/format,png)

从日志中可以看出，在dex2oat发起时具体的编译类型、线程数以及编译原因等等。

常见的编译类型：verify、quicken、space-profile、space、speed-profile、speed、everything， 具体效果从字面上比较好理解， 越后面的类型编译时间越长，占用的空间也越大，运行时打开速度也越快，典型空间换时间思路的体现，其中profile类型的编译方式主要是根据JIT运行过程中热点函数的情况进行编译，JIT机制不进行展开，可以查阅相关资料。

**4.3  和dex2oat相关的系统配置**

\[pm.dexopt.ab-ota\]: \[speed-profile\]

\[pm.dexopt.bg-dexopt\]: \[speed-profile\]

\[pm.dexopt.boot\]: \[verify\]

\[pm.dexopt.first-boot\]: \[quicken\]

\[pm.dexopt.inactive\]: \[verify\]

\[pm.dexopt.install\]: \[speed-profile\]

\[pm.dexopt.shared\]: \[speed\]

**4.4  如何手动发起dex2oat操作**

通过以上介绍参数含义后，我们可以在adb shell 下通过命令行方式直接发起dex2oat操作，例如强制编译微信：

_adb shell cmd package compile -m speed-profile -f com.tencent.mm_

清除配置文件数据并移除经过编译的代码：

adb shell cmd package compile --reset com.tencent.mm

下面对具体调用的流程进行分析。

**五、Dex2oat 流程分析**

本次分析基于Android Q 代码。上述触发场景主要涉及PackageManagerService , 所以从该服务作为入口，分析dex2oat的相关流程。

应用在发起dex2oat时，主要通过PMS中接口调用installd触发的，相关调用函数performDexOpt ，该函数在上述应用安装以及启动的时候都会涉及，所以主要查看下这个函数的调用流程，详见代码：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X2pwZy9kNGhvWUpseE9qT3FEWEZoNjhLaFFpY3NpYlVNVXJhVzFMTFR4aGljOVhkTUpwOXpzVnpoVkJtMmIwbjJRbkRUelJXRjFraWF5QndNVXdOMFc1WXhjSmFwUlEvNjQw?x-oss-process=image/format,png)

传入参数DexoptOptions ， 可以通过该参数指定编译包名，编译类型以及标志，返回编译是否成功。常见编译标志位：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qT3FEWEZoNjhLaFFpY3NpYlVNVXJhVzFMaWNsMmljcG9IRzBiaWJJM2ZMMHlIMUZCcUpLdTdJdGtEYWxGTHplbnYyMno1eEUyMlNpYWljcmw2aWJRLzY0MA?x-oss-process=image/format,png)

接下来的调用流程：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X2pwZy9kNGhvWUpseE9qT3FEWEZoNjhLaFFpY3NpYlVNVXJhVzFMMTdnRGVFWGxjNGF6VVdzWmFIdFdnaWJWNURIMVA1aWExaWJLMXFKMHhlTTcwMEVvRXlQNFQ0WkFnLzY0MA?x-oss-process=image/format,png)

以上是Framework中Dex2oat 的调用流程， 感兴趣的同学可以跟踪代码查看具体细节。

系统经过installd 的dexopt编译，通常会利用一些关键的日志查看dex2oat相关的信息， 比如计算dex2oat运行耗时以及最终的编译状态。

**Installd dexopt代码小结：**

1.检测dexopt  classloader context 和相关的flag。

2.解析传入参数，生成dex2oat 命令， 最后通过RunDex2Oat 执行。 

常见的dalvik参数控制属性值，更多标志位参考详见参考资料2：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qT3FEWEZoNjhLaFFpY3NpYlVNVXJhVzFMNXJsd2liNWVwdUFidlVTV3NrWG1QVWJxZU1EZzc3YVE4QTlETHdUOGliTDdpYmwyNFk2WXZpYXNJQS82NDA?x-oss-process=image/format,png)

经过上述命令，最终调用到底层libart中相关代码，下面我们查看dex2oat调用的流程图， 了解dex2oat的相关流程。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qT3FEWEZoNjhLaFFpY3NpYlVNVXJhVzFMYXNpYzJKaWNDN3VTbGVDTjAxenFCRnVGWERGdUMycmxoVm1JejNMVnhrNnhxS3J2SmhPQmE5UncvNjQw?x-oss-process=image/format,png)

流程图

**Dex2oat逻辑小结：**

1\. 处理命令行参数；

2\. 判断dex2oat的setup是否完成；

3\. 根据是否为image类型，分别调用CompileImage或CompileApp的处理，CompileImage和CompileApp的主要功能逻辑类似，主要通过CompilerDriver对dexfile 进行编译。

**六、Dex2oat常见修改思路**

前面介绍了dex2oat一些优化以及相关的流程，虽然能够提高系统的流畅度，如果在不合适的时机发起，很有可能影响到其他用户操作，需要针对这一类情况进行修改。

以下是常见的修改思路：

1\. 根据场景和负载情况调整dex2oat 编译参数，如编译类型，编译线程数量等。

2\. 调整boot.img编译资源，预加载资源文件列表。

3\. 后台并行编译。在系统空闲或者首次加载dex文件的时候预先触发dex2oat流程，从而加快后续使用dex文件的速度。

举例：后台应用安装导致大量资源被dex2oat占用导致前台进程卡顿

dex2oat 优化后虽然能够增加应用运行的流畅度， 但是如果在短时间内大量发起则会影响用户界面操作， 造成负面的影响。所以发现应用是因为后台自动更新时， 则可以限制dex2oat运行的线程数量，尽可能的减少对前面进程的影响。

**七、总结**

本文从dex2oat日志输出和使用命令出发，介绍了dex2oat常用的场景以及相关的调用流程，以此为根据简单讨论了常见dex2oat优化方面的思路，希望能起到抛砖引玉的作用，加深读者对dex2oat的原理流程的了解。

**参考资料**

1.https://source.android.google.cn/devices/tech/dalvik/dex-format

2.https://source.android.com/devices/tech/dalvik/configure#runtime\_configuration

3.https://source.android.com/devices/tech/dalvik

4.https://blog.csdn.net/cosmoslhf/article/details/40380559

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X2dpZi9kNGhvWUpseE9qTlNyNE9XSjlrdWtpYkZuc3h2U3pkSWlicjJqRDVZVDNqQU1mOWVrZDJDc0I3ME9HallxbjBKdFB3QjFtSXkxWlduQ216R1JMSzJFaWN5dy82NDA?x-oss-process=image/format,png)

**扫描关注**  
**“内核工匠”微信公众号**  
Linux 内核黑科技 | 技术文章 | 精选教程
