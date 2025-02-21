---
tags:
  - 性能优化
---
文章内容根据《性能优化入门与实战》总结而来



## 为什么要做内存优化

内存作为App运行必需的资源，对用户体验的影响非常明显。如果内存使用不当，会导致如下的问题：

-   App 因为 OOM 而崩溃
-   应用后台存活时间短，被系统强制“杀掉”（物理内存不足）
-   频繁执行GC，导致应用启动变慢、流畅性变差、耗电更快（频繁垃圾回收）

## 内存问题造成的影响

### OOM

常见的 OOM 异常类型有 `Java OOM`、`Native OOM`、`Graphics OOM` 三种。

#### Java OOM

Java OOM 指的是App使用的Java内存超出了App可以使用的Java Heap上限。常见的 Java OOM 的报错信息如下：

```
// 在发生OOM前，App想要分配516252字节（约504 KB）内存，由于内存不足，
// 系统先尝试做了一次GC（Garbage Collection，垃圾回收），但只释放了50816字节（约49KB）内存
// ，这时实在没有更多内存可供使用，只好抛出java.lang.OutOfMemoryError。
java.lang.OutOfMemoryError : Failed to allocate a 516252 byte allocation with 50816 free bytes and 49KB until OOM

// 在创建一个线程时，需要分配1040KB内存，但是现在可用内存不足，创建失败，
// 抛出java.lang.OutOfMemoryError
java.lang.OutOfMemoryError: pthread_create (1040KB stack) failed: Try again
```

要分析 Java OOM，可以使用 HPROF 或者 JVMTI。目前市面上的主流内存分析工具基本都是基于这两者实现的，比如MAT、LeakCanary、Android Studio Memory Profiler等

#### Native OOM

Native OOM 是指C/C++代码使用的内存过多，导致App无法再分配内存。一种典型的Native OOM异常如下所示：

```
// App使用了4008912KB（约3.82GB）虚拟内存，在32位设备上，
// 这个使用量已经基本达到上限，因此在下一次分配内存时，发生异常。
Signal 6 (SIGABRT), code -1 (SI_QUEUE), fault addr --- abort message: 'App vss abnormal VmSize: 4008912 kB'
```

发生Native OOM时，异常信息会更复杂一些，包含信号(SIGABRT)、fault addr（错误地址）、主动抛出的异常消息(abort message)等。在不同的场景下，abort message可能会略有区别，但基本上都会携带out of memory、abnormal VmSize、mmap failed等关键字。

相对分析Java OOM，分析Native OOM要复杂一些，需要通过maps/hook等方式，获取到崩溃时App的具体内存使用情况进行分析

#### Graphics OOM

Graphics OOM主要是指在通过OpenGL渲染图形（比如直播、拍摄、图像处理等场景）时，分配内存失败而抛出的异常。典型的Graphics OOM异常如下所示：

```
Signal 6(SIGABRT), Code -6(SI_TKILL) abort message: GL error: Out of memory! Signal 6(SIGABRT), Code -6(SI_TKILL) abort message: 'glTexImage2D error! GL_OUT_OF_MEMORY (0x505)'
```

报错信息看起来和Native OOM异常的比较相似，主要通过观察abort message中是否包含GL关键字进行区分。

### 物理内存不足导致App后台存活时间短

应用后台存活时间短的主要原因是App退到后台后优先级会变低，LMK机制在“杀掉”进程时，会先拿内存占用高的下手。如下图所示，LMK 先杀死内存最多的应用逻辑在 `find_and_kill_process` 方法中，可以看 [lmkd.cpp](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2Fmain%2F%2B%2Fmain%3Asystem%2Fmemory%2Flmkd%2Flmkd.cpp%3Bl%3D2534%3Fq%3Dfind_and_kill_process "https://cs.android.com/android/platform/superproject/main/+/main:system/memory/lmkd/lmkd.cpp;l=2534?q=find_and_kill_process")源码。

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/83c799ec87d0445aaac3fe6d50e3d755~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=EzRI8alTuy0EpptH2nH0Mix2udQ%3D)

可以看到，Android系统会在设备物理内存不足时，根据通过oom\_adj计算出的oom\_score\_adj和App占用内存，按照从高到低的顺序“杀掉”App。因此我们的App当使用比较多内存时，退到后台后很容易被列入“先杀名单”。

我们可以通过 `adb shell cat /proc/{pid}/oom_score_adj` 查看进程的分数。

```
127|HWAGS2:/ $ cat /proc/19892/oom_score_adj -1000 //最小值，表示优先级最高 HWAGS2:/ $ cat /proc/19892/oom_adj -17
```

当App被强制“杀掉”时，我们可以从Logcat看到相关日志：

```
ActivityManager: Force Stopping top.shixinzhang.example appid=10251 user=-1 ActivityManager: Killing 3281:top.shixinzhang.example (adj 900):stop top.shixinzhang.example
```

### 频繁执行GC

频繁的GC还是会对App造成不少影响。主要原因如下:

1.  GC的类型有多种，有些类型的GC会阻塞线程执行，这无疑会影响线程执行速度。
2.  异步执行GC的线程（线程名为HeapTaskDaemon）常常会占用大量CPU时间片或抢占大核，导致主线程无法被及时调度（CPU时间片变少、线程状态频繁切换、从大核切换到小核），从而影响应用启动速度、页面流畅性。
3.  部分版本的GC采用复制算法，会将数据复制到另外一块内存，导致CPU缓存失效，代码执行效率降低。
4.  GC过程中会获取一些锁，导致主线程锁等待。

下面是从Atrace看到的被GC阻塞的日志

```
// 主线程和GC线程有锁竞争，处于阻塞状态。
20277,955343210876,B|20277|Lock contention on GC barrier lock (owner tid: 0)
```

异步执行GC线程和主线程的CPU使用时间，如下图所示：

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/8fe5768c650b41a58775277d6c009f07~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=yD%2FxvzCt4olYtwyqkvPPvhS8OLA%3D)

> 在为App分配内存时，如果申请的内存超过一定数值，系统会先尝试进行GC(kGcCauseForAlloc)，如果GC后内存还是不够，就会增加Java heap大小，申请额外空间然后分配内存。分配完成后，还会判断是否需要执行后台GC(kGcCauseBackground)。在执行后台GC时，我们可以从Logcat看到相关日志如下：
> 
> ```
> Background concurrent copying GC freed 148318(7207KB) AllocSpace objects, 
> 0(0B) LOS objects, 49% free, 24MB/48MB, paused 664us total 199.730ms
> ```

## 内存监控

就崩溃、后台存活时间短、卡顿这3个问题，讲述线上内存监控方案

### 线上监控

#### 内存不足导致的崩溃如何监控

监控内存不足导致的崩溃所需数据，如下图所示：

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/1879123944c044baae040feccf4ef926~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=khQqSEigfRhzGkTQu8C%2BqTs2x7Y%3D)

-   OOM次数

```
//1.自定义崩溃处理器
public class JavaCrashHandler implements Thread.UncaughtExceptionHandler {
    @Override    
    public void uncaughtException(@NonNull Thread t, @NonNull Throwable e) {
        //2.判断崩溃类型
        if (e instanceof OutOfMemoryError) {
              //发生了 OutOfMemory          
              //3.记录当前内存使用数据并上报        
        }
    }
}
　
//4.为线程注册崩溃处理器
Thread.currentThread().setUncaughtExceptionHandler(new JavaCrashHandler());
```

使用 `Thread.UncaughtExceptionHandler` 来监控App的内存不足导致的崩溃的次数

-   内存使用情况

通过Runtime我们可以获取App的Java内存的上限和当前已使用的内存，代码示例如下：

```
/** * 通过 Runtime 获取 Java 内存使用情况*/
private static void getByRuntime() {
     //dalvik 堆最大可用内存
     long maxMemory = Runtime.getRuntime().maxMemory();
     long freeMemory = Runtime.getRuntime().freeMemory();
     long totalMemory = Runtime.getRuntime().totalMemory();
　
     //已使用的内存
     double memoryUsedPercent = (totalMemory - freeMemory) * 1.0f / maxMemory * 100;
　
     Log.w(TAG, "memoryUsedPercent: " + memoryUsedPercent + " %");
     Log.d(TAG, "maxMemory: " + formatMB(maxMemory)+ " ,
           totalMemory: " + formatMB(totalMemory)+ " ,
           used: " + formatMB(totalMemory - freeMemory));
}
```

通过Debug.MemoryInfo#getMemoryStats()，我们可以获取到Java、Native、Graphics等类型的物理内存使用情况，代码示例如下：

```
// android.os.Debug.MemoryInfo的getMemoryStats方法
public Map<String, String> getMemoryStats() {
     Map<String, String> stats = new HashMap<String, String>();
     //Java 堆内存实际映射的物理内存
     stats.put("summary.java-heap", Integer.toString(getSummaryJavaHeap()));
     //Native 堆内存实际映射的物理内存
     stats.put("summary.native-heap", Integer.toString(getSummaryNativeHeap()));
     //.dex文件 .so文件 .art文件 .ttf文件等映射的物理内存
     stats.put("summary.code", Integer.toString(getSummaryCode()));
     //运行时栈空间映射的物理内存
     stats.put("summary.stack", Integer.toString(getSummaryStack()));
     //Graphics 相关映射的物理内存
     stats.put("summary.graphics", Integer.toString(getSummaryGraphics()));
     //…
     return stats;
}

// 使用方法如下
Debug.MemoryInfo memoryInfo = new Debug.MemoryInfo();
Debug.getMemoryInfo(memoryInfo);
Map<String, String> memoryStats = memoryInfo.getMemoryStats();
Set<Map.Entry<String, String>> entries = memoryStats.entrySet();
                               for (Map.Entry<String, String> entry : entries) {
   Log.d(TAG, "getByDebugMemoryInfo: " + entry.getKey() + " : " + entry.getValue());
}
```

#### 后台被强制“杀掉”的问题如何监控

要判断App是否有在后台被强制“杀掉”的问题，需要获取下图所示的数据

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/3100a687a0a643afa11a3773dd9e3063~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=yxqypp7sayhrbcwZjwYb5NDmygI%3D)

-   获取 LMK 的次数

从Android 11开始，Android系统为我们提供了可以直接获取App上次退出的信息的API(ActivityManager.getHistoricalProcessExitReasons)，通过它我们可以获取到App退出的原因、当时进程的优先级和物理内存等信息。如果App被LMK强制“杀掉”，下次启动应用时就能查询到被强制“杀掉”的信息。代码示例如下：

```
@RequiresApi(api = Build.VERSION_CODES.R)
                   private static void getApplicationExitInfo() {
    if (sContext == null) {
         return;
    }
　
    String packageName = sContext.getPackageName();
    ActivityManager activityManager = (ActivityManager)sContext.
                         getSystemService(Context.ACTIVITY_SERVICE);
    List<ApplicationExitInfo> historicalProcessExitReasons = activityManager.
                                  getHistoricalProcessExitReasons(packageName, 0, 0);
    for (ApplicationExitInfo info : historicalProcessExitReasons) {
         int importance = info.getImportance();
         int reason = info.getReason();
         String processName = info.getProcessName();
         Log.d(TAG, "ApplicationExitInfo:  processName: " + processName + " ,
                   reason: " + reason + " , importance: " + importance);
    }
}

```

当进程被LMK强制“杀掉”后，进程的退出原因是REASON\_LOW\_MEMORY。因此每次启动App后查询退出记录，我们就能获取到App不同版本的LMK次数。

另外，App被LMK强制“杀掉”时，也会有对应的Logcat日志：

```
ActivityManager: Killing 3281:top.shixinzhang.example (adj 900):stop top. 
shixinzhang.example
```

因此在系统低于Android 11的手机上，我们可以通过Logcat日志数据判断App是否被强制“杀掉”。具体方式：在崩溃时上报最近的Logcat日志数据，分析其中是否有 `Killing {pid}:{package Name} (adj ×××)`等关键字，如果有则证明App被强制“杀掉”了。

-   是否为低物理内存

ActivityManager为我们提供了查询当前设备是否为低物理内存设备的API，当设备物理内存小于等于1GB时这个API返回true。代码示例如下:

```
boolean lowRamDevice = activityManager.isLowRamDevice();
```

更详细的内存信息，我们可以通过ActivityManager.getMemoryInfo查询设备的物理内存总数及剩余可用内存。代码示例如下：

```
ActivityManager activityManager = (ActivityManager) sContext.
                                   getSystemService(Context.ACTIVITY_SERVICE);
ActivityManager.MemoryInfo memoryInfo = new ActivityManager.MemoryInfo();
                           activityManager.getMemoryInfo(memoryInfo);
printSection("手机操作系统的物理内存是否够用 (ActivityManager.getMemoryInfo)：");
Log.d(TAG, "RAM size of the device: " + formatMB(memoryInfo.totalMem)+ " ,
      availMem: " +formatMB(memoryInfo.availMem)+ ",
      lowMemory:" + memoryInfo.lowMemory  + " ,
      threshold: " + formatMB(memoryInfo.threshold));

```

返回值的含义如下图所示：

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/ac4de79a5270441b850b687decddbb78~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=cQPqiWMvmqmnTOyQnEuYuxwsQGw%3D)

-   进程的oom\_score和优先级

什么是oom\_score呢？可以理解为LMK机制对不同进程的评分，根据应用的优先级动态调整。LMK在执行进程清理时会根据这个分数决定先清理谁：oom\_score越大，进程越容易被杀。我们可以通过读取/proc/{pid}/oom\_score\_adj来获取App的oom\_score。App的oom\_score\_adj范围为\[-1000, 1000\]。

```
try {
    String scoreAdjPath = String.format(Locale.CHINA, "/proc/%d/oom_score_adj",
                          Process.myPid());
    String adjPath = String.format(Locale.CHINA, "/proc/%d/oom_adj",
                     Process. myPid());
    String content = FileUtils.file2String(scoreAdjPath);
    Log.d(TAG, "oom_score_adj path: " + scoreAdjPath + " : " + content);
} catch (Exception e) {
    e.printStackTrace();
}
```

上面的经过测试，在部分使用较旧的操作系统的机型上，App没有权限读取这个节点的数据。除此之外还可以通过ActivityManager.getMyMemoryState获取到App的优先级（也称重要性，本书统称“优先级”）数据，对于LMK机制，它的概念和oom\_score很接近。代码示例如下：

```
ActivityManager.RunningAppProcessInfo processInfo = new ActivityManager.
                                                    RunningAppProcessInfo();
ActivityManager.getMyMemoryState(processInfo);
//importance 可以用于判断是否前后台
//这个值可以结合 oom_score_adj一起判断APP的优先级
Log.d(TAG, "process importance: " + processInfo.importance + " ,
      lastTrimLevel: " + processInfo.lastTrimLevel);
```

Android系统定义的优先级有这些：

```
@IntDef(prefix = { "IMPORTANCE_" }, value = {
        IMPORTANCE_FOREGROUND,
        IMPORTANCE_FOREGROUND_SERVICE,
        IMPORTANCE_TOP_SLEEPING,
        IMPORTANCE_VISIBLE,
        IMPORTANCE_PERCEPTIBLE,
        IMPORTANCE_CANT_SAVE_STATE,
        IMPORTANCE_SERVICE,
        IMPORTANCE_CACHED,
        IMPORTANCE_GONE,
})
@Retention(RetentionPolicy.SOURCE)
public @interface Importance {}
```

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/d9deec9259624723824f5939e4bf7302~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=sRNietAR9wK2xJ6JYeYLj2plnGY%3D)

通过查看进程上报的优先级，我们就可以知道进程被“杀掉”时所处的状态。另外，我们也可以根据系统对优先级的判断标准，通过一些手段提升进程的优先级，降低进程被强制“杀掉”的概率。

#### GC对流畅性的影响如何监控

要衡量App是否因为GC而卡顿，需要获取的数据如下图所示:

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/e29eac35a7844cedb9786d04d2814563~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=SCJ5aKu1zew6pO5YWPaNB7Yz1D8%3D)

-   GC次数和耗时

GC可以分为两种类型：阻塞式、非阻塞式。

1.  阻塞式GC是指在进行GC时，会阻塞GC发起线程。
2.  非阻塞式GC是指并发执行的GC，不会显式阻塞其他线程。

我们可以通过Debug.getRuntimeStat获取到App当前的GC次数和耗时：

```
public static long getGCInfoSafely(String info) {
    try {
         return Long.parseLong(Debug.getRuntimeStat(info));
    } catch (Throwable throwable) {
         throwable.printStackTrace();
         return -1;
    }
}private static void getGCInfo() {
     long gcCount = getGcInfoSafely("art.gc.gc-count");
     long gcTime = getGcInfoSafely("art.gc.gc-time");
     long blockGcCount = getGcInfoSafely("art.gc.blocking-gc-count");
     long blockGcTime = getGcInfoSafely("art.gc.blocking-gc-time");
}
```

上面的返回值中，blockGcCount和blockGcTime是App从启动到被查询时阻塞式GC的次数和耗时；gcCount和gcTime是App从启动到被查询时的非阻塞式GC的次数和耗时。

通过对比不同场景的FPS、GC次数、耗时差值，我们可以得出不同场景下的GC对流畅性的影响，从而决定是否需要对当前业务进行GC优化

-   GC 是否频繁

除了GC次数，并发执行的GC线程HeapTaskDaemon的繁忙程度，也可以用于衡量GC的影响。如果在启动、页面加载等核心场景，HeapTaskDaemon线程的CPU使用时间比主线程的还长，就说明GC对App的性能有严重影响。所以我们需要追踪HeapTaskDaemon的CPU使用时间。

在运行时我们可以通过遍历进程的 `/proc/{pid}/task`，找到名称为 HeapTaskDaemon 的 tid，然后从`/proc/{pid}/task/{tid}/stat` 中读取CPU使用时间相关数据。分别找到表示HeapTaskDaemon线程的用户态时长和内核态时长，把它们累加起来，就是线程从创建到被查询时的CPU使用时间。通过将其与主线程的CPU使用时间对比，我们可以判断出HeapTaskDaemon是否执行太频繁，从而决定是否需要对其进行优先级降低。

### 线下监控

线下内存测试的主要目的如下。

1.  获取App整体和各个场景的内存指标，包括虚拟内存、物理内存。
2.  在内存异常时，明确具体是哪种内存异常，比如Java内存、Native内存或者哪个动态库异常。
3.  初步分析导致问题的原因。

#### 获取App的内存指标

获取整体内存指标的方式如下：

1.  获取虚拟内存总大小及swap值：`/proc/${pid}/status`。
2.  获取进程的各类型内存使用量：`dumpsys meminfo --local ${pid}`。
3.  获取较为详细的内存数据：`/proc/${pid}/maps`。

-   通过`adb shell cat /proc/${pid}/status`获取进程总的虚拟内存大小

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/dc91b3189f6846809d802f8c7ea23f3b~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=4zceoKTiauVamKAsWa26BAivMIc%3D)

如上图所示，可以看到，通过`/proc/${pid}/status`，我们可以获取到很多有用的信息，如下所示。

-   FDSize：文件描述符数量，部分设备在其文件描述符数量超出上限后会崩溃。
    
-   VmPeak：虚拟内存的峰值。
    
-   VmSize：当前虚拟内存大小。
    
-   VmSwap：交换虚拟内存大小。
    
-   Threads：线程数。
    
-   通过`adb shell dumpsys meminfo --local`获取到当前App的整体物理内存和详细分类的物理内存的数据
    

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/70e0d54c37e8427a89090f8b6c2ad028~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=LIF5so3L9SrLKQ1pvpLBoYJuldo%3D)

如上图所示，可以看到，通过dumpsys meminfo，我们可以获取到当前App的整体物理内存情况和各个类型的物理内存信息：

-   Java Heap：Java物理内存，在Java/Kotlin代码中分配的内存。
-   Native Heap：Native物理内存，在C/C++ 代码中分配的内存。
-   Code：文件映射的物理内存。
-   Graphics：Graphics物理内存，如图片、纹理等的内存。
-   TOTAL：总的物理内存

标注1处我们可以获取到更加具体的信息：

-   Java内存可以分为Dalvik Heap和Dalvik other。
-   Graphics内存可以分为Gfx dev、EGL mtrack、GL mtrack。
-   Code内存可以分为.so文件、.jar文件、.apk文件、.ttf文件、.dex文件、.oat文件、.art文件等的内存。

Android Studio Profiler提供的内存信息，与dumpsys meminfo结果一致的

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/b011abd82c584d82a298eb4e38727057~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=gRvxDCg0jfa9m02LHtj6rpqQ3jY%3D)

#### 获取进程的内存空间数据

/proc/${pid}/maps可以为我们提供某个进程的虚拟内存空间的详细数据，如下图所示：

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/14b265954157433b8a9bb0c1f46facf6~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=gg3D9ZiFEdxWm0PTIdnezAmJeXU%3D)

maps会返回多行结果，以第一行为例，其内容的含义如下：

-   12c0000-2ac0000：这块内存的开始地址和结束地址，二者相减可以得到这段内存的大小。
-   rw-p：这块内存的权限，r表示可读、w表示可写、x表示可执行、p表示私有。
-   00000000：偏移地址，这块内存在文件中的偏移，匿名映射为0。
-   00:00：主设备号和次设备号，匿名映射为00:00。
-   0：索引节点的节点号，匿名映射为0。
-   \[anon:dalvik-main space (region space)\]：映射的文件名或者代码中设置的内存名称。

通过maps信息我们可以看到进程的文件映射、匿名映射等数据。不过它的结果还比较笼统，有时候我们想知道某个动态库占据的内存是共享的还是私有的，可以通过 /proc/${pid}/smaps进行查看。如下图所示：

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/08813dfd30be46d5b9bdad2aa9a3bca9~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=y%2BF5uaVRlFZrUim3fleVkv02sb8%3D)

-   Size：虚拟内存大小。
-   KernelPageSize/MMUPageSize：页大小，一般为4KB。
-   Rss：实际分配的物理内存，等于Shared\_Clean +Shared\_Dirty + Private\_Clean + Private\_Dirty。
-   Shared：多个进程共享的内存，比如系统库。
-   Private：进程内部私有的内存。
-   Clean/Dirty：内存是否和文件一致，基于文件映射的内存如果被修改后未同步到文件就是Dirty的。
-   Pss：按照比例分配共享内存后的物理内存，其大小小于等于Rss的大小，等于Private\_Clean + Private\_Dirty + Shared\_Clean/共享进程数 +Shared\_Dirty/共享进程数。
-   Referenced：被访问过的内存。
-   Anonymous：匿名内存。

如果觉得统计起来比较麻烦，在Android高版本上，可以使用showmap命令，查看进程的内存使用情况，比如showmap -a 20027 （-a表示展示虚拟地址，20027是进程ID），如下图所示:

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/1449a2de5f794913b5c0eb89cc33b9d1~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=eHT9dK75jHnuvdPcP8qQpfQoDaw%3D)

showmap会将smaps的结果进行聚合，给出某个文件映射或者匿名内存的整体虚拟内存大小和物理内存大小。我们可以对结果做二次加工，根据virtual size（即虚拟内存大小）或者PSS（Proportional Set Size，物理内存大小）进行排序，即可得到占用内存最大的文件映射或者匿名内存

#### 分析内存的使用详情

要分析Java内存问题，我们可以先在终端中执行adb shell am dumpheap生成hprof文件。

```
% adb shell am dumpheap top.shixinzhang.performance
File: /data/local/tmp/heapdump-20220831-115045.prof
Waiting for dump to finish…
% cd hprof
% adb pull /data/local/tmp/heapdump-20220831-115045.prof .
/data/local/tmp/heapdump-20220831-115045.prof: 1 file pulled, 0 skipped. 213.3 MB/s (129206570 bytes in 0.578s)
```

如果觉得手动生成hprof文件比较麻烦，可以使用Android Studio Profiler的实时内存分配分析功能（基于JVMTI实现）。一种是使用栈视图，它通过表格的方式罗列每个函数分配和释放的内存及其调用次数；一种是使用火焰图，通过图形化的方式展示，剩余内存越多的函数，区块越长

## 内存优化

根据线上常见的内存问题的类型，主要分为以下3种。

1.  Java内存问题。
2.  Native内存问题。
3.  图片内存问题。

### Java内存问题分析、定位

#### 什么时候分析 Java 内存问题

Java内存，是指在Java/Kotlin代码中申请，由Android虚拟机完成分配的内存。当通过监控发现线上有如下问题时，说明需要进行Java内存优化。

1.  java.lang.OutOfMemoryError较多。
2.  Java Heap内存使用较高。
3.  卡顿发生前，GC执行较频繁

#### 常见的 Java 内存问题

导致Java内存异常的问题一般如下：

-   对象引用泄漏。
-   存在内存占用很大的对象。
-   创建线程过多。
-   存在大量重复的对象。
-   频繁创建、回收对象。

要获取这些信息，在线下，我们可以通过LeakCanary、Android Studio Profiler、MAT(Memory Analyzer Tool)等工具；在线上，我们可以使用KOOM、Tailor等开源库或自研工具.了解目前使用较多的Java内存分析开源库KOOM的原理

#### KOOM 的原理

定位Java内存问题，最核心的就是获取到hprof文件。在线下内存测试时，我们可以通过`adb shell am dumpheap`获取hprof文件，比如 `adb shell am dumpheap top.shixinzhang.performance` ；而在线上内存测试时，最简单的方式是通过`Debug.dumpHprofData`获取。

```
//android.os.Debug
public static void dumpHprofData(String fileName) throws IOException {
      VMDebug.dumpHprofData(fileName);
}
```

调用这个方法并传递一个文件路径，就可以在这个路径下生成hprof文件。内存泄漏检测开源库LeakCanary就是通过这种方式获取hprof文件的。Debug.dumpHprofData的好处是使用简单，仅用一行代码就可以实现获取hprof文件。但它的缺点也很明显：调用后会导致进程卡顿数秒甚至数十秒！这对用户体验的影响是非常大的。让人惊喜的是，开源的KOOM库解决了Debug.dumpHprofData执行时的卡顿问题，使得线上获取hprof文件的性能损耗变得可以接受。

-   Debug.dumpHprofData为什么会导致卡顿?

执行时会暂停当前进程的所有线程。

-   KOOM是如何解决卡顿问题的?

KOOM高性能获取hprof文件的核心方案：fork子进程执行Debug.dumpHprofData，把耗时操作转移到子进程。我们还需要了解这个核心方案背后的两点。

1.  为什么在子进程中获取的hprof文件和在主进程中获取的一样？

之所以在子进程中获取的hprof文件和在主进程中获取的一样，是因为在Linux中，为了提升进程创建的速度和减少内存使用，创建子进程时不会立刻分配新的内存地址空间，而是先让其和父进程共享同一块内存。只有在子进程发生写操作时，才分配新的内存。这样在刚创建的子进程中访问的内存数据其实就是父进程的内存数据

2.  为什么在创建子进程之前需要先暂停主进程的所有线程？

而在创建子进程之前需要先暂停主进程的所有线程，有如下两个原因:

-   获取hprof文件需要从所有GC Root出发进行遍历，如果不暂停线程执行，结果会有问题（这就是Debug.dumpHprofData中会执行暂停线程的原因）。
-   在子进程创建后才执行暂停逻辑，修改的就是子进程的数据，会触发内存分配等操作，无法获取到主进程执行dump时的真实数据。

#### KOOM如何检测线程泄漏

在Android系统中，主线程默认占用内存为8192KB，即8MB，其他线程默认占用内存为1MB左右.在ANR的trace文件里我们可以在每个线程的stackSize值里看到对应的值内存。示例如下：

```
"main" prio=5 tid=1 Blocked
  | group="main" sCount=1 dsCount=0 flags=1 obj=0x713dcb88 self=0x7e27006c00
  | sysTid=24602 nice=-10 cgrp=default sched=0/0 handle=0x7e28574ed0
  | state=S schedstat=( 8406034979 1095165571 4956 ) utm=740 stm=100 core=7 HZ=100
  | stack=0x7fcb05f000-0x7fcb061000 stackSize=8192KB
　
"HeapTaskDaemon" prio=5 tid=101 TimedWaiting
  | group="main" sCount=1 dsCount=0 flags=1 obj=0x13cc1f70 self=0x7d37632c00
  | sysTid=24781 nice=0 cgrp=default sched=0/0 handle=0x7cd10ffd50
  | state=S schedstat=( 51265214 45234581 362 ) utm=3 stm=2 core=7 HZ=100
  | stack=0x7cd0ffd000-0x7cd0fff000 stackSize=1040KB
```

Java线程没有及时退出的典型例子如下。

-   使用HandlerThread，没有及时调用quit方法。HandlerThread在启动后会执行Looper.loop，这会导致run方法始终不会结束。
-   使用Thread.java，在run方法里有循环操作。
-   项目里线程池过多，线程池的核心线程数都不为0。

Native 线程结束后无法退出的例子：创建pthread时没有设置detach状态（Linux线程的一个属性，默认值为joinable），导致线程执行完后无法退出。之所以会这样，是因为Linux线程在退出时，只有进入detach状态才会释放内存，所以我们需要在创建线程时主动将线程的detach状态设置为PTHREAD\_CREATE\_DETACHED，否则会导致线程泄漏。

常见的线程监控方式有两种:

1.  编译时对new Thread和new HandlerThread等代码进行AOP切面hook，在其中增加统计代码或者直接把创建线程替换成统一使用线程池。
2.  运行时在hook线程创建的底层实现pthread\_create方法，在其中增加统计代码，根据创建堆栈进行优化。

当我们通过new Thread创建一个线程时，最终会执行到Native层的pthread\_create方法，也就是说，一个Java线程对应一个Native线程。

我们可以通过拦截Linux线程创建的关键函数pthread\_create，在其中获取调用堆栈，实现监控线程的创建信息。在KOOM中，实现线程监控的是thread\_hook.cpp

```
//koom-thread-leak/src/main/cpp/src/thread/thread_hook.cpp
void ThreadHooker::InitHook() {
  //获取当前加载的所有库
  std::set<std::string> libs;
  DlopenCb::GetInstance().GetLoadedLibs(libs);
  //执行 hook
  HookLibs(libs, Constant::kDlopenSourceInit);
  DlopenCb::GetInstance().AddCallback(DlopenCallback);
}
　
bool ThreadHooker::RegisterSo(const std::string &lib, int source) {
  if (IsLibIgnored(lib)) {
    return false;
  }
  auto lib_ctr = lib.c_str();
  xhook_register(lib_ctr, "pthread_create",
                 reinterpret_cast<void *>(HookThreadCreate), nullptr);
  xhook_register(lib_ctr, “pthread_detach”,
                 reinterpret_cast<void *>(HookThreadDetach), nullptr);
  xhook_register(lib_ctr, "pthread_join",
                 reinterpret_cast<void *>(HookThreadJoin), nullptr);
  xhook_register(lib_ctr, "pthread_exit",
                 reinterpret_cast<void *>(HookThreadExit), nullptr);
　
  return true;
}
```

可以看到，thread\_hook.cpp拦截了当前加载的所有.so文件的pthread\_create、pthread\_detach、pthread\_join和pthread\_exit函数，这几个函数是线程创建、退出、被修改状态的核心方法。在线程创建的代理函数中，会获取当前函数的调用堆栈，将其保存到记录中。

```
//线程创建的拦截函数
//koom-thread-leak/src/main/cpp/src/thread/thread_hook.cpp
int ThreadHooker::HookThreadCreate(pthread_t *tidp, const pthread_attr_t *attr,
                                   void *(*start_rtn)(void *), void *arg) {
   if (hookEnabled() && start_rtn != nullptr) {
       //…
       void *thread = koom::CallStack::GetCurrentThread();
       if (thread != nullptr) {
           //Java 栈回溯
           koom::CallStack::JavaStackTrace(thread,
                            hook_arg->thread_create_arg->java_stack);
       }
       //Native 栈回溯
       koom::CallStack::FastUnwind(thread_create_arg->pc,
                                   koom::Constant::kMaxCallStackDepth);
       thread_create_arg->stack_time = Util::CurrentTimeNs() - time;
                          return pthread_create(tidp, attr,
                          reinterpret_cast<void *(*)(void *)>(HookThreadStart),
                          reinterpret_cast<void *>(hook_arg));
       }
       return pthread_create(tidp, attr, start_rtn, arg);
 }

```

在线程退出的代理函数中，会在之前的缓存中查找当前线程ID对应的记录信息，查看当前线程记录的thread\_detached状态是否为true。

```
//koom-thread-leak/src/main/cpp/src/thread/thread_holder.cpp
void ThreadHolder::ExitThread(pthread_t threadId, std::string &threadName,
                              long long int time) {
  bool valid = threadMap.count(threadId) > 0;
  if (!valid) return;
　
  //从记录 map 中获取数据
  auto &item = threadMap[threadId];
　
  item.exitTime = time;
  item.name.assign(threadName);
  if (!item.thread_detached) {
      // 泄漏了
      koom::Log::error(holder_tag,
                        "Exited thread Leak! Not joined or detached!\n tid:%p",
                        threadId);
      leakThreadMap[threadId] = item;
    }
    threadMap.erase(threadId);
    koom::Log::info(holder_tag, "ExitThread finish");
 }

```

通过拦截pthread\_create、pthread\_exit、pthread\_join、pthread\_detach这4个方法，KOOM就实现了记录一个线程从创建、状态修改到退出的整个过程，当线程退出时，如果没有被“join”或“detach”就会上报泄漏记录。

### Native内存问题分析、定位

#### 什么时候进行Native内存优化

Native内存指在C/C++代码中申请，由Linux Kernel完成分配的内存，包括创建匿名内存和文件映射。当通过监控发现线上有如下问题时，说明需要进行Native内存优化：

-   Native OOM较多。
-   进程后台存活时间短，容易被系统强制关闭。

#### 常见的 Native 内存问题

Native OOM一般发生在32位App（目前主流设备基本都支持64位App）上，由于使用的内存过多或者存在内存泄漏，导致使用的虚拟内存超出了3GB（32位设备）或者4GB（64位设备）。

虽然64位App的虚拟内存上限很高，但使用的虚拟内存最终还是要分配到物理内存才能使用。目前主流设备的物理内存在8GB到16GB范围内，在物理内存剩余不多时，会触发kswapd和LMK等机制回收内存，如果我们的App在后台时使用的内存仍然很多，很容易被系统强制关闭。

在一些设备上，App可以打开的FD（File Descriptor，文件描述符）的上限比较低，如果App打开的FD过多也会导致崩溃。

在线下我们可以使用Android Studio Memory Profiler、.proc文件等；在线上我们可以使用MemoryLeakDetector、KOOM和Matrix等开源库或自研工具。我们来看一下性能和稳定性较好的MemoryLeakDetector是如何检测Native内存泄漏的。通过理解它的实现细节，我们可以掌握Native内存泄漏分析的理论和方法。

#### MemoryLeakDetector 的使用

MemoryLeakDetector是字节跳动开源的一款Android Native内存泄漏监控工具，它可以监控在C/C++ 代码中申请的Native内存和创建线程时使用的内存。因为接入简单、稳定性好、监控范围广，它在很多有亿级用户的产品上都被使用。

添加依赖

```
dependencies {
    implementation 'com.github.bytedance:memory-leak-detector:0.1.8'
}
```

调用Raphael.start开始检测内存分配

```
String space = getExternalFilesDir("native_leak").getAbsolutePath(); 
Raphael.start(Raphael.MAP64_MODE|Raphael.ALLOC_MODE|0x0F0000|1024, space,null);
```

在程序运行起来后，通过本地广播输出当前的内存分配记录。

```
adb shell am broadcast -a com.bytedance.raphael.ACTION_PRINT -f 0x01000000
```

这一步后，就会在调用Raphael.start时设置的目录下生成当前的内存分配记录文件，我们可以通过adb pull导出该文件到计算机上。

```
adb pull /storage/emulated/0/Android/data/top.shixinzhang.performance/files/native_leak .
```

然后使用MemoryLeakDetector提供的Python脚本进行数据聚合分析，脚本的参数如下。

```
##   -r：日志路径, 必需，手机端生成的report文件
##   -o：输出文件名，非必需，默认为 leak-doubts.txt
##  -s：符号表目录，非必需，有符号化需求时可传入，符号表文件需跟.so文件同名，如lib×××.so，多个文件需放在同一目录下
python3 memory-leak-detector/library/src/main/python/raphael.py -r native_leak/report -o leak-result-256.txt
```

执行完这个脚本，我们就可以得到APP当前的内存分配记录数据。

```
    513,187,022   totals
268,984,320  libtest_leak.so
 31,034,050  libhwui.so
  1,822,644  libandroid_runtime.so
 211,346,008  extras
　
0xb400006f0e498000, 268435456, 1
0x0000000000017704
/data/app/~~mNBCozUcn8AWo8zkLp0rRw==/top.shixinzhang.performance-i7RI4VOWN2-1qlVXpsDFnQ==/base.apk!/lib/arm64-v8a/libtest_leak.so (unknown)
0x00000000000175b0
/data/app/~~mNBCozUcn8AWo8zkLp0rRw==/top.shixinzhang.performance-i7RI4VOWN2-1qlVXpsDFnQ==/base.apk!/lib/arm64-v8a/libtest_leak.so (unknown)
0x00000000000b1814 /apex/com.android.runtime/lib64/bionic/libc.so (__pthread_start(void*) + 268)
0x00000000000512f4 /apex/com.android.runtime/lib64/bionic/libc.so (__start_thread + 68)
```

#### MemoryLeakDetector 的原理

为什么这样就会导致Native内存泄漏呢？这是因为在C/C++程序中，没有Java程序那样的自动GC机制，内存的分配和释放需要由程序员自己完成。我们只通过malloc分配了内存却没有释放，就会导致这部分内存始终处于被使用状态，无法被再次使用，这就是我们常说的“Native内存泄漏”。

要检测Native内存泄漏，就需要对调用内存分配和释放的API的代码做监控，找到分配了内存后却始终没有释放内存的代码。MemoryLeakDetector监控了App对这些API的调用。

```
  static const void *sPltGot[][2] = {{
                "malloc",(void *) malloc_proxy
        },{
                "calloc",(void *) calloc_proxy
        },{
                "realloc",(void *) realloc_proxy
        },{
                "memalign",(void *) memalign_proxy
        },{
                "free",(void *) free_proxy
        },{
                "mmap",(void *) mmap_proxy
        },{
                "mmap64",(void *) mmap64_proxy
        },{
                "munmap",(void *) munmap_proxy
        },{
                "pthread_exit",(void *) pthread_exit_proxy
        }
};

```

Android上常见的Native内存分配API的名称和优缺点

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/d67b54bc90b248e4a4602a835d515d68~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=BocNK9U2WvR62Vk2m4j678uOWlA%3D)

-   malloc

Android Native内存分配使用得比较多的是malloc.malloc的作用是在堆上分配内存，参数是要分配的字节数，返回分配的地址。在使用前，需要通过memset初始化分配的内存，否则可能会出现未知的问题。

为了提升内存分配的性能，malloc会提供一个内存缓存池，调用malloc时会先从内存缓存池里查找是否有剩余可用内存，如果没有才申请内存，并且会分配较大的空间。

-   calloc

calloc主要用于在堆上为数组分配内存，会把分配的所有字节设置为0（其他API一般需要手动设置为0）。

-   realloc

realloc用于调整已分配内存空间的大小，如果要扩张内存容量，并且当前的地址后没有足够的连续内存，就会重新分配一块内存空间，再把之前的数据复制过来，这时会返回新内存地址，否则返回的还是之前的内存地址

-   memalign

memalign主要用于调整内存地址的基数，一般内存地址是8的整数倍，通过memalign可以调整地址边界，但要求边界需要是2的次方。业务开发中使用不太多。可实现内存对齐

-   alloca

会在栈上分配内存，在函数执行完栈退出后会自动释放内存

-   mmap（mmap64是用于64位系统的）

mmap（mmap64是用于64位系统的）的功能非常强大，可以用于创建匿名内存、文件映射、共享内存等。它分配的内存在堆外的内存映射区，主要用于大内存分配，因为分配的最小单位是page（页

需要注意的是，calloc和realloc底层会调用malloc实现，而当我们使用malloc时，如果分配的内存大于指定的阈值（默认值是128KB或者256KB，取决于内存分配器的实现，比如dlmalloc中MMAP\_THRESHOLD为256KB ，这个阈值可以通过mallopt修改），就会使用mmap实现，否则会使用brk（通过调整数据段的大小分配内存，业务代码一般很少直接调用）实现

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/549854f02f2948649e3d53d6e146cc9d~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=BNrdjrpsFvbcJMHxlJUGXPWrvdA%3D)

除了监控分配内存的API，MemoryLeakDetector还监控了两个释放内存的API

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/fbb80d8aa945449880c3771b12dcd24b~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=v8iz0wLoQ7xlrNO5QkjHilZwdkg%3D)

free用于释放malloc、calloc、realloc等API在堆上分配的内存，在调用后不会立刻释放这部分内存，而是会通过一个缓存列表将其保存起来，后面调用malloc等API时会先从这里找是否有可用的内存

munmap用于释放mmap映射的内存，如果是基于文件的映射并且内存中的数据和文件不一致，会先同步数据到文件上。释放后再访问这部分内存，会导致SIGSEGV异常

#### MemoryLeakDetector内存分配代理函数原理

MemoryLeakDetector通过PLT Hook和Inline Hook技术代理了App对内存分配和内存释放函数的调用，在内存分配的代理函数中做了如下事。

1.  判断要分配的内存是否超出监控阈值，未超出就直接调用原始函数。
2.  若超出监控阈值，则触发代理逻辑。
3.  通过pthread\_setspecific设置一个线程独享的标记，避免malloc底层调用到mmap后多次统计数据。
4.  调用原始函数分配内存，保存返回的内存地址。
5.  抓取调用堆栈。
6.  将分配的地址、调用堆栈和内存大小保存到分配记录缓存中。

#### MemoryLeakDetector获取Native调用堆栈

在Linux中，每个线程会有自己的栈区，在函数执行时会创建栈帧，在其中保存着每一帧执行的函数地址、局部变量、寄存器状态等数据，其中包括当前指向的指令地址和caller（调用当前函数的函数）的地址等信息.

在函数执行时如果调用其他函数，会将当前fp和lr寄存器的值保存到函数的栈帧中并将栈帧压入线程栈。等函数执行完，会把caller从栈中出栈

MemoryLeakDetector首先通过GetStackRange函数获取当前线程的栈空间起始地址和结束地址

```
library/src/main/unwind64/backtrace_64.h
static inline void GetStackRange(uintptr_t *st, uintptr_t *sb) {
    void *address;
    size_t size;
　
    pthread_attr_t attr;
    //获取当前线程的属性
    pthread_getattr_np(pthread_self(), &attr);
    //获取当前线程的栈地址和栈大小
    pthread_attr_getstack(&attr, &address, &size);
　
    pthread_attr_destroy(&attr);
　
    //计算栈顶地址和栈底地址
    *st = (uintptr_t) address + size;
    *sb = (uintptr_t) address;
}
```

因此，栈回溯的核心就是通过sp和fp确定当前栈帧的起始地址和结束地址，通过lr获取到每一帧执行的函数的名称。

#### MemoryLeakDetector内存释放代理函数原理

内存泄漏的判断标准就是内存在分配后有没有释放。MemoryLeakDetector在内存分配代理函数中把分配的地址、堆栈和内存大小保存到了记录中。内存释放代理函数做了如下事。

1.  判断线程独享的标记，避免重入。
2.  调用原始函数释放内存。
3.  通过释放的内存地址，移除内存分配记录中相应的记录。

### 图片内存问题分析、定位

#### 什么时候分析图片内存问题

当通过监控发现线上有如下问题时，说明可能需要进行图片内存优化：

-   创建图片导致的OOM较多（堆栈包含android.graphics.Bitmap.nativeCreate）。
-   OOM增多，调用dump hprof后发现Bitmap对象占用内存很高。
-   某个业务使用图片较多，近期GC次数增多，内存指标抖动（升高后又降低）严重。

#### 常见的图片内存问题

导致图片内存异常的问题一般如下：

-   加载的图片过大：如一次性加载多张200MB左右的图片，瞬间导致内存溢出。
-   图片对象泄漏：被图片缓存或者其他强引用持有过久，导致内存中的图片过多。
-   频繁创建新图片对象：缓存失效，或者图片创建逻辑异常（比如列表页快速滑动的同时加载大量图片）

#### 图片内存监控与分析的常见方案

图片内存监控与分析的常见方案如下所示：

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/dbb9acc7f0d94dee81c17b19f44bb21f~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=UfTO4q6laTMTNeVPQRfAPxCYg1U%3D)

编译时AOP（Aspect Oriented Programming，面向切面编程）拦截是指在打包时修改所有调用Android SDK图片创建API的代码，替换为代理实现，在其中记录图片的大小、堆栈，同时可以结合布局的尺寸做图片是否过大的判断。优点是可以获取的信息比较多，能结合布局信息判断；缺点是需要拦截的API过多，在运行时如果有相关问题，无法直接从源码看出问题，需要反编译App

> 为了减少图片对App内存的影响，Android系统在不同版本上做过如下调整。1.在Android 8.0以前，图片的宽高数据和像素数据都保存在Java层。2.从Android 8.0开始，Java层只保存图片的宽高数据，图片的像素数据保存在Native层，不再占用Java Heap内存。带来的好处是Android 8.0及以后的设备上出现Java OOM的概率大大降低。不过OOM只是众多内存问题中最明显的一个，Android 8.0及以后的设备使用图片不当还是会存在物理内存使用过多、位于后台时容易被LMK强制关闭的问题。

## 减少内存的有效方法

### 提升App的可用内存

减少内存问题，最简单、有效的方法是提升App的可用内存，主要有如下两种思路。 1.提升单进程的内存上限。 2.拆分业务逻辑到多进程。

提升单进程的内存上限，可以从提升Java内存上限和提升虚拟内存上限两部分出发。

-   提升Java内存上限

对于单进程的Java内存的上限，我们可以通过在AndroidManifest.xml的application中设置largeHeap ="true"来提升，这个设置会让App创建的所有进程使用更大的Java堆

```
<application
    //…
    android:largeHeap="true">
    //…
</application>
```

我们可以通过ActivityManager的getMemoryClass和getLargeMemoryClass获取单个进程的常规Java堆内存上限和设置largeHeap后的Java堆内存上限，也可以通过Runtime.getRuntime().maxMemory方法查看当前App运行时的最大Java堆内存.

```
ActivityManager activityManager = (ActivityManager) sContext.
getSystemService(Context.ACTIVITY_SERVICE);
Log.d(TAG, "getMemoryClass: " + activityManager.getMemoryClass() + " ,
      getLargeMemoryClass: " + activityManager.getLargeMemoryClass());
　
long maxMemory = Runtime.getRuntime().maxMemory();
Log.d(TAG, "maxMemory: " + formatMB(maxMemory));

```

运行结果如下：

```
//设置Large Heap前
Meminfo: getMemoryClass: 256 ,getLargeMemoryClass: 512
Meminfo: maxMemory: 256.0MB
　
//设置 largeHeap 后
Meminfo: getMemoryClass: 256 ,getLargeMemoryClass: 512
Meminfo: maxMemory: 512.0MB
```

-   提升虚拟内存上限

对于单进程的虚拟内存上限，我们可以通过适配64位架构，让App在64位设备上的虚拟内存的可用上限有极大的提升。目前新出厂的设备大都支持64位，因此为了提供更好的用户体验，各App商店都要求发布App时必须支持64位架构。如果你的App还没有适配64位，需要做以下几件事。1.首先在自己开发的native库的build.gradle中设置abiFilters，添加对arm64-v8a的支持，比如这样：

```
android {
    defaultConfig {
        ndk {
            abiFilters 'armeabi-v7a', 'arm64-v8a'
        }
    }
}
```

2.然后在build.gradle的android中增加splits和abi代码块，在其中配置需要针对不同的CPU架构生成不同的APK，在每个APK中仅包含一套架构的native库，以减少最终分发的App体积。

```
android {
  …
  splits {
    abi {
      enable true
      reset()
      include armeabi-v7a", "arm64-v8a
      universalApk false
    }
  }
}

```

上面的配置中，enable表示是否根据ABI（Application Binary Interface，应用程序二进制接口）生成不同的APK，默认为false，我们需要将其设置为true；reset是一个方法，和include配合使用，用于强制指定要为哪些ABI单独生成APK；universalApk表示是否要打一个包含所有ABI的包，这里我们要将其设置为false。

-   分离业务到子进程

通过以上两种方法提升单进程的可用内存上限后，我们还可以采用多进程的方法，把核心且耗费内存的业务放到子进程进行，这样既可以减少单个进程使用的内存，还可以在主进程意外崩溃时，保证核心业务继续进行

不过需要小心的是，在主进程中和其他进程频繁地进行Binder通信可能会导致卡顿

### 认识所有会持有对象引用的GC Root

提升可用内存后，接下来要做的就是尽可能地合理使用内存，减少内存泄漏。常见的GC Root名称及其含义如下图所示：

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/fa8146afe2d14885a8fedb44a9889a55~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=27gDyjKtd8E69gBZ692FoB1OgXg%3D)

> 除了常见的Java类静态属性引用的对象、常量引用的对象、栈帧中的局部变量引用的对象，我们还需要关注调用JNI代码时传递的参数、在JNI代码中创建的全局Java对象、调用synchronized和wait的对象，它们是日常开发中可能造成内存泄漏的典型例子

### 不滥用软引用和弱引用

软引用/弱引用不会导致内存泄漏的前提是：虚拟机在进行GC时，这个对象的引用者只有软对象/弱对象。注意加粗的两个字“只有”。如果这个对象会被其他强引用持有，那就还是无法回收。比如正在GC执行时，调用WeakReference#get的方法正在执行，那这个方法里的输入参数、局部变量、返回值都会在虚拟机的栈桢中，这样原始的对象就会有一个局部变量作为强引用，导致它无法被回收。

Android系统源代码里的确存在频繁调用WeakReference#get导致的内存泄漏，一个典型的例子是属性动画ObjectAnimator.ObjectAnimator中使用WeakReference保存要执行动画的View，动画开始后每帧刷新时都会执行animateValue方法，直到动画结束。由于animateValue中会调用WeakReference#get方法创建局部引用，因此在它执行期间，View对象会被局部变量强引用，导致GC无法回收这个View对象。

```
    //frameworks/base/core/java/android/animation/ObjectAnimator.java
    private WeakReference<Object> mTarget;
　
    public Object getTarget() {
        return mTarget == null ? null : mTarget.get();
    }
　
    void animateValue(float fraction) {
        final Object target = getTarget();
        if (mTarget != null && target == null) {
             cancel();
             return;
        }
　
        // …
}
```

由于ObjectAnimator的这个问题，App中一旦创建了无限循环的动画且退出页面后没有停止，就会导致View泄漏

这个bug在Android 12及以下的设备上都存在，所以我们在使用属性动画时需要注意及时调用cancel。此外我们还需要记住：使用软引用/弱引用后也可能会有内存泄漏，需要保证它不被频繁调用！

### 单例对象统一管理

单例模式方便的原因是有一个静态对象始终存在于内存中，如果没有及时释放，很容易导致内存泄漏。有一种比较好的避免单例对象内存泄漏的方式：给单例对象增加生命周期托管

### 根据内存情况进行业务降级

我们可以根据设备的配置，采取不同的内存使用策略。一个具体业务的设备分级内存策略

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/ed32e60f3eeb4b67bc116c079b638d32~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5bCP5aKZ56iL5bqP5ZGY:q75.awebp?rk3s=f64ab15b&x-expires=1740706555&x-signature=ALftRxPvi8Cge7evD718nVqldKg%3D)