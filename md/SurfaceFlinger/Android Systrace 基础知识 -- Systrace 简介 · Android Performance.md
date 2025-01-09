本文是 Systrace 系列文章的第一篇，主要是对 Systrace 进行简单介绍，介绍其简单使用方法；如何去看 Systrace；如何结合其他工具对 Systrace 中的现象进行分析。

本系列的目的是通过 Systrace 这个工具，从另外一个角度来看待 Android 系统整体的运行，同时也从另外一个角度来对 Framework 进行学习。也许你看了很多讲 Framework 的文章，但是总是记不住代码，或者不清楚其运行的流程，也许从 Systrace 这个图形化的角度，你可以理解的更深入一些。

## [](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/#%E7%B3%BB%E5%88%97%E6%96%87%E7%AB%A0%E7%9B%AE%E5%BD%95 "系列文章目录")系列文章目录

1.  [Systrace 简介](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/)
2.  [Systrace 基础知识 - Systrace 预备知识](https://www.androidperformance.com/2019/07/23/Android-Systrace-Pre/)
3.  [Systrace 基础知识 - Why 60 fps ？](https://www.androidperformance.com/2019/05/27/why-60-fps/)
4.  [Systrace 基础知识 - SystemServer 解读](https://www.androidperformance.com/2019/06/29/Android-Systrace-SystemServer/)
5.  [Systrace 基础知识 - SurfaceFlinger 解读](https://www.androidperformance.com/2020/02/14/Android-Systrace-SurfaceFlinger/)
6.  [Systrace 基础知识 - Input 解读](https://www.androidperformance.com/2019/11/04/Android-Systrace-Input/)
7.  [Systrace 基础知识 - Vsync 解读](https://www.androidperformance.com/2019/12/01/Android-Systrace-Vsync/)
8.  [Systrace 基础知识 - Vsync-App ：基于 Choreographer 的渲染机制详解](https://androidperformance.com/2019/10/22/Android-Choreographer/)
9.  [Systrace 基础知识 - MainThread 和 RenderThread 解读](https://www.androidperformance.com/2019/11/06/Android-Systrace-MainThread-And-RenderThread/)
10.  [Systrace 基础知识 - Binder 和锁竞争解读](https://www.androidperformance.com/2019/12/06/Android-Systrace-Binder/)
11.  [Systrace 基础知识 - Triple Buffer 解读](https://www.androidperformance.com/2019/12/15/Android-Systrace-Triple-Buffer)
12.  [Systrace 基础知识 - CPU Info 解读](https://www.androidperformance.com/2019/12/21/Android-Systrace-CPU)
13.  [Systrace 流畅性实战 1 ：了解卡顿原理](https://www.androidperformance.com/2021/04/24/android-systrace-smooth-in-action-1/)
14.  [Systrace 流畅性实战 2 ：案例分析: MIUI 桌面滑动卡顿分析](https://www.androidperformance.com/2021/04/24/android-systrace-smooth-in-action-2/)
15.  [Systrace 流畅性实战 3 ：卡顿分析过程中的一些疑问](https://www.androidperformance.com/2021/04/24/android-systrace-smooth-in-action-3/)
16.  [Systrace 响应速度实战 1 ：了解响应速度原理](https://www.androidperformance.com/2021/09/13/android-systrace-Responsiveness-in-action-1/)
17.  [Systrace 响应速度实战 2 ：响应速度实战分析-以启动速度为例](https://www.androidperformance.com/2021/09/13/android-systrace-Responsiveness-in-action-2/)
18.  [Systrace 响应速度实战 3 ：响应速度延伸知识](https://www.androidperformance.com/2021/09/13/android-systrace-Responsiveness-in-action-3/)
19.  [Systrace 线程 CPU 运行状态分析技巧 - Runnable 篇](https://www.androidperformance.com/2022/01/21/android-systrace-cpu-state-runnable/)
20.  [Systrace 线程 CPU 运行状态分析技巧 - Running 篇](https://www.androidperformance.com/2022/03/13/android-systrace-cpu-state-running/)
21.  [Systrace 线程 CPU 运行状态分析技巧 - Sleep 和 Uninterruptible Sleep 篇](https://www.androidperformance.com/2022/03/13/android-systrace-cpu-state-sleep/)

## [](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/#%E6%AD%A3%E6%96%87 "正文")正文

Systrace 是 Android4.1 中新增的性能数据采样和分析工具。它可帮助开发者收集 Android 关键子系统（如 SurfaceFlinger/SystemServer/Kernel/Input/Display 等 Framework 部分关键模块、服务，View系统等）的运行信息，从而帮助开发者更直观的分析系统瓶颈，改进性能。

Systrace 的功能包括跟踪系统的 I/O 操作、内核工作队列、CPU 负载以及 Android 各个子系统的运行状况等。在 Android 平台中，它主要由3部分组成：

-   **内核部分**：Systrace 利用了 Linux Kernel 中的 ftrace 功能。所以，如果要使用 Systrace 的话，必须开启 kernel 中和 ftrace 相关的模块。
-   **数据采集部分**：Android 定义了一个 Trace 类。应用程序可利用该类把统计信息输出给ftrace。同时，Android 还有一个 atrace 程序，它可以从 ftrace 中读取统计信息然后交给数据分析工具来处理。
-   **数据分析工具**：Android 提供一个 systrace.py（ python 脚本文件，位于 Android SDK目录/platform-tools/systrace 中，其内部将调用 atrace 程序）用来配置数据采集的方式（如采集数据的标签、输出文件名等）和收集 ftrace 统计数据并生成一个结果网页文件供用户查看。 从本质上说，Systrace 是对 Linux Kernel中 ftrace 的封装。应用进程需要利用 Android 提供的 Trace 类来使用 Systrace.  
    关于 Systrace 的官方介绍和使用可以看这里：[Systrace](http://developer.android.com/tools/help/systrace.html "SysTrace官方介绍")

> Note : 最新版本的 platform-tools 里面已经移除了 Systrace 工具，Google 推荐使用 [Perfetto](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/(https://perfetto.dev/docs/)) 来抓 Trace
> 
> 个人建议：Systrace 和 Perfetto 都可以用，哪个用着顺手就用哪个，不过最终 Google 是要用 Perfetto 来替代 Systrace 的，所以可以把默认的 Trace 打开工具切换成 [Perfetto UI](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/(https://ui.perfetto.dev/))
> 
> Perfetto 相比 Systrace 最大的改进是可以支持长时间数据抓取，这是得益于它有一个可在后台运行的服务，通过它实现了对收集上来的数据进行 Protobuf 的编码并存盘。从数据来源来看，核心原理与 Systrace 是一致的，也都是基于 Linux 内核的 Ftrace 机制实现了用户空间与内核空间关键事件的记录（ATRACE、CPU 调度）。Systrace 提供的功能 Perfetto 都支持，由此才说 Systrace 最终会被 Perfetto 替代。
> 
> Perfetto 所支持的数据类型、获取方法，以及分析方式上看也是前所未有的全面，它几乎支持所有的类型与方法。数据类型上通过 ATRACE 实现了 Trace 类型支持，通过可定制的节点读取机制实现了 Metric 类型的支持，在 UserDebug 版本上通过获取 Logd 数据实现了 Log 类型的支持。
> 
> 你可以通过 Perfetto.dev 网页、命令行工具手动触发抓取与结束，通过设置中的开发者选项触发长时间抓取，甚至你可以通过框架中提供的 Perfetto Trigger API 来动态开启数据抓取，基本上涵盖了我们在项目上能遇到的所有的情境。
> 
> 在数据分析层面，Perfetto 提供了类似 Systrace 操作的数据可视化分析网页，但底层实现机制完全不同，最大的好处是可以支持超大文件的渲染，这是 Systrace 做不到的（超过 300M 以上时可能会崩溃、可能会超卡）。在这个可视化网页上，可以看到各种二次处理的数据、可以执行 SQL 查询命令、甚至还可以看到 logcat 的内容。Perfetto Trace 文件可以转换成基于 SQLite 的数据库文件，既可以现场敲 SQL 也可以把已经写好的 SQL 形成执行文件。甚至你可以把他导入到 Jupyter 等数据科学工具栈，将你的分析思路分享给其他伙伴。
> 
> 比如你想要计算 SurfaceFlinger 线程消耗 CPU 的总量，或者运行在大核中的线程都有哪一些等等，可以与领域专家合作，把他们的经验转成 SQL 指令。如果这个还不满足你的需求， Perfetto 也提供了 Python API，将数据导出成 DataFrame 格式近乎可以实现任意你想要的数据分析效果。
> 
> 可以在 [Android 性能优化的术、道、器](https://www.androidperformance.com/2022/01/07/The-Performace-1-Performance-Tools/) 这篇文章中查看各个工具的介绍和使用，以及他们的优劣比。
> 
> 笔者后续会持续更新 Perfetto 系统，后续所有的图例也都会用 Perfetto 来演示。

### [](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/#Systrace-%E7%9A%84%E8%AE%BE%E8%AE%A1%E6%80%9D%E8%B7%AF "Systrace 的设计思路")**Systrace 的设计思路**[](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/#Systrace-%E7%9A%84%E8%AE%BE%E8%AE%A1%E6%80%9D%E8%B7%AF)

在**系统的一些关键操作**（比如 Touch 操作、Power 按钮、滑动操作等）、**系统机制**（input 分发、View 绘制、进程间通信、进程管理机制等）、**软硬件信息**（CPU 频率信息、CPU 调度信息、磁盘信息、内存信息等）的关键流程上，插入类似 Log 的信息，我们称之为 TracePoint（本质是 Ftrace 信息），通过这些 TracePoint 来展示一个核心操作过程的执行时间、某些变量的值等信息。然后 Android 系统把这些散布在各个进程中的 TracePoint 收集起来，写入到一个文件中。导出这个文件后，Systrace 通过解析这些 TracePoint 的信息，得到一段时间内整个系统的运行信息。

[![](https://www.androidperformance.com/images/Android-Systrace-About/56bebe5b-e5fd-4b69-8e36-3197fe205034.png)](https://www.androidperformance.com/images/Android-Systrace-About/56bebe5b-e5fd-4b69-8e36-3197fe205034.png)

Android 系统中，一些重要的模块都已经默认插入了一些 TracePoint，通过 TraceTag 来分类，其中信息来源如下

1.  Framework Java 层的 TracePoint 通过 android.os.Trace 类完成
2.  Framework Native 层的 TracePoint 通过 ATrace 宏完成
3.  App 开发者可以通过 android.os.Trace 类自定义 Trace

这样 Systrace 就可以把 Android 上下层的所有信息都收集起来并集中展示，对于 Android 开发者来说，Systrace 最大的作用就是把整个 Android 系统的运行状态，从黑盒变成了白盒。全局性和可视化使得 Systrace 成为 Android 开发者在分析复杂的性能问题的时候的首选。

### [](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/#%E5%AE%9E%E8%B7%B5%E4%B8%AD%E7%9A%84%E5%BA%94%E7%94%A8%E6%83%85%E5%86%B5 "实践中的应用情况")实践中的应用情况[](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/#%E5%AE%9E%E8%B7%B5%E4%B8%AD%E7%9A%84%E5%BA%94%E7%94%A8%E6%83%85%E5%86%B5)

解析后的 Systrace 由于有大量的系统信息，天然适合分析 Android App 和 Android 系统的性能问题， Android 的 App 开发者、系统开发者、Kernel 开发者都可以使用 Systrace 来分析性能问题。

1.  从技术角度来说，Systrace 可覆盖性能涉及到的 **响应速度** 、**卡顿丢帧**、 **ANR** 这几个大类。
2.  从用户角度来说，Systrace 可以分析用户遇到的性能问题，包括但不限于:
    1.  应用启动速度问题，包括冷启动、热启动、温启动
    2.  界面跳转速度慢、跳转动画卡顿
    3.  其他非跳转的点击操作慢（开关、弹窗、长按、选择等）
    4.  亮灭屏速度慢、开关机慢、解锁慢、人脸识别慢等
    5.  列表滑动卡顿
    6.  窗口动画卡顿
    7.  界面加载卡顿
    8.  整机卡顿
    9.  App 点击无响应、卡死闪退

在遇到上述问题后，可以使用多种方式抓取 Systrace ，将解析后的文件在 Chrome 打开，然后就可以进行分析

## [](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/#Systrace-%E7%AE%80%E5%8D%95%E4%BD%BF%E7%94%A8 "Systrace 简单使用")Systrace 简单使用[](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/#Systrace-%E7%AE%80%E5%8D%95%E4%BD%BF%E7%94%A8)

使用 Systrace 前，要先了解一下 Systrace 在各个平台上的使用方法，鉴于大家使用Eclipse 和 Android Studio 的居多，所以直接摘抄官网关于这个的使用方法，不过不管是什么工具，流程是一样的：

-   手机准备好你要进行抓取的界面
-   点击开始抓取(命令行的话就是开始执行命令)
-   手机上开始操作(不要太长时间)
-   设定好的时间到了之后，会将生成 Trace.html 文件，使用 **Chrome** 将这个文件打开进行分析

一般抓到的 Systrace 文件如下  
[![](https://www.androidperformance.com/images/15618018036720.jpg)](https://www.androidperformance.com/images/15618018036720.jpg)

## [](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/#%E4%BD%BF%E7%94%A8%E5%91%BD%E4%BB%A4%E8%A1%8C%E5%B7%A5%E5%85%B7%E6%8A%93%E5%8F%96-Systrace "使用命令行工具抓取 Systrace")使用命令行工具抓取 Systrace[](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/#%E4%BD%BF%E7%94%A8%E5%91%BD%E4%BB%A4%E8%A1%8C%E5%B7%A5%E5%85%B7%E6%8A%93%E5%8F%96-Systrace)

命令行形式比较灵活，速度也比较快，一次性配置好之后，以后再使用的时候就会很快就出结果（**强烈推荐**）  
Systrace 工具在 Android-SDK 目录下的 platform-tools 里面（**最新版本的 platform-tools 里面已经移除了 systrace 工具，需要下载老版本的 platform-tools ，33 之前的版本**，可以在这里下载：[https://androidsdkmanager.azurewebsites.net/Platformtools](https://androidsdkmanager.azurewebsites.net/Platformtools) ）,下面是简单的使用方法

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br></pre></td><td><pre><span><span>$ </span><span><span>cd</span> android-sdk/platform-tools/systrace</span></span><br><span><span>$ </span><span>python systrace.py</span></span><br></pre></td></tr></tbody></table>

可以在 Bash 中配置好对应的路径和 Alias，使用起来还是很快速的。另外 User 版本所抓的 Systrce 文件所包含的信息,是比 eng 版本或者 Userdebug 版本要少的,建议使用 Userdebug 版本的机器来进行 debug,这样既保证了性能,又能有比较详细的输出结果.

抓取结束后，会生成对应的 Trace.html 文件，注意这个文件只能被 Chrome 打开。关于如何分析 Trace 文件，我们下面的章节会讲。不论使用那种工具，在抓取之前都可以选择参数，下面说一下这些参数的意思：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br></pre></td><td><pre><span> -a appname      enable app-level tracing for a comma separated list of cmdlines; * is a wildcard matching any process</span><br><span> -b N            use a trace buffer size of N KB</span><br><span> -c              trace into a circular buffer</span><br><span> -f filename     use the categories written in a file as space-separated</span><br><span>                   values in a line</span><br><span> -k fname,...    trace the listed kernel functions</span><br><span> -n              ignore signals</span><br><span> -s N            sleep for N seconds before tracing [default 0]</span><br><span> -t N            trace for N seconds [default 5]</span><br><span> -z              compress the trace dump</span><br><span> --async_start   start circular trace and return immediately</span><br><span> --async_dump    dump the current contents of circular trace buffer</span><br><span> --async_stop    stop tracing and dump the current contents of circular</span><br><span>                   trace buffer</span><br><span> --stream        stream trace to stdout as it enters the trace buffer</span><br><span>                   Note: this can take significant CPU time, and is best</span><br><span>                   used for measuring things that are not affected by</span><br><span>                   CPU performance, like pagecache usage.</span><br><span> --list_categories</span><br><span>                 list the available tracing categories</span><br><span>-o filename      write the trace to the specified file instead</span><br><span>                   of stdout.</span><br></pre></td></tr></tbody></table>

上面的参数虽然比较多，但使用工具的时候不需考虑这么多，在对应的项目前打钩即可，命令行的时候才会去手动加参数，我们一般会把这个命令配置成 Alias，比如（下面列出的 am，binder\_driver 这些，不同的手机、root 和非 root，会有一些不同，可以查看下一节，使用 adb shell atrace –list\_categories 来查看你的手机支持的 tag）：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br></pre></td><td><pre><span>alias <span>st</span>-start=<span>'python /path-to-sdk/platform-tools/systrace/systrace.py'</span>  </span><br><span>alias <span>st</span>-start-gfx-trace = ‘<span>st</span>-start -t <span>8</span> <span>am</span>,binder_driver,camera,dalvik,freq,gfx,hal,idle,<span>input</span>,memory,memreclaim,<span>res</span>,sched,<span>sync</span>,<span>view</span>,webview,wm,workq,binder’</span><br></pre></td></tr></tbody></table>

这样在使用的时候，可以直接敲 **st-start** 即可，当然为了区分和保持各个文件，还需要加上 **\-o xxx.html** .上面的命令和参数不必一次就理解，只需要记住如何简单使用即可，在分析的过程中，这些东西都会慢慢熟悉的。

一般来说比较常用的是

1.  \-o : 指示输出文件的路径和名字
2.  \-t : 抓取时间(最新版本可以不用指定, 按 Enter 即可结束)
3.  \-b : 指定 buffer 大小 (一般情况下,默认的 Buffer 是够用的,如果你要抓很长的 Trae , 那么建议调大 Buffer )
4.  \-a : 指定 app 包名 (如果要 Debug 自定义的 Trace 点, 记得要加这个)

## [](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/#%E6%9F%A5%E7%9C%8B%E6%94%AF%E6%8C%81%E7%9A%84-TAG "查看支持的 TAG")查看支持的 TAG

Systrace 默认支持的 TAG，可以通过下面的命令来进行抓取，不同厂商的机器可能有不同的配置，在使用的时候可以根据自己的需求来进行选择和配置，TAG 选的少的话，Trace 文件的体积也会相应的变小，但是抓取的内容也会相应变少。Trace 文件大小会影响其在 Chrome 中打开后的操作性能，所以这个需要自己取舍

以我手上的 Android 12 的机器为例，可以看到这台机器支持下面的 tag（左边是 tag 名，右边是解释）

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br><span>40</span><br><span>41</span><br><span>42</span><br><span>43</span><br><span>44</span><br><span>45</span><br><span>46</span><br><span>47</span><br><span>48</span><br></pre></td><td><pre><span><span>$ </span><span>adb shell atrace --list_categories</span></span><br><span>         gfx - Graphics</span><br><span>       input - Input</span><br><span>        view - View System</span><br><span>     webview - WebView</span><br><span>          wm - Window Manager</span><br><span>          am - Activity Manager</span><br><span>          sm - Sync Manager</span><br><span>       audio - Audio</span><br><span>       video - Video</span><br><span>      camera - Camera</span><br><span>         hal - Hardware Modules</span><br><span>         res - Resource Loading</span><br><span>      dalvik - Dalvik VM</span><br><span>          rs - RenderScript</span><br><span>      bionic - Bionic C Library</span><br><span>       power - Power Management</span><br><span>          pm - Package Manager</span><br><span>          ss - System Server</span><br><span>    database - Database</span><br><span>     network - Network</span><br><span>         adb - ADB</span><br><span>    vibrator - Vibrator</span><br><span>        aidl - AIDL calls</span><br><span>       nnapi - NNAPI</span><br><span>         rro - Runtime Resource Overlay</span><br><span>         pdx - PDX services</span><br><span>       sched - CPU Scheduling</span><br><span>         irq - IRQ Events</span><br><span>         i2c - I2C Events</span><br><span>        freq - CPU Frequency</span><br><span>        idle - CPU Idle</span><br><span>        disk - Disk I/O</span><br><span>        sync - Synchronization</span><br><span>       workq - Kernel Workqueues</span><br><span>  memreclaim - Kernel Memory Reclaim</span><br><span>  regulators - Voltage and Current Regulators</span><br><span>  binder_driver - Binder Kernel driver</span><br><span>  binder_lock - Binder global lock trace</span><br><span>   pagecache - Page cache</span><br><span>      memory - Memory</span><br><span>     thermal - Thermal event</span><br><span>        freq - CPU Frequency and System Clock (HAL)</span><br><span>         gfx - Graphics (HAL)</span><br><span>         ion - ION Allocation (HAL)</span><br><span>      memory - Memory (HAL)</span><br><span>       sched - CPU Scheduling and Trustzone (HAL)</span><br><span>  thermal_tj - Tj power limits and frequency (HAL)</span><br></pre></td></tr></tbody></table>

## [](https://www.androidperformance.com/2019/05/28/Android-Systrace-About/#%E5%85%B3%E4%BA%8E%E6%88%91-amp-amp-%E5%8D%9A%E5%AE%A2 "关于我 && 博客")关于我 && 博客

下面是个人的介绍和相关的链接，期望与同行的各位多多交流，三人行，则必有我师!

1.  [博主个人介绍](https://www.androidperformance.com/about/) ：里面有个人的微信和微信群链接。
2.  [本博客内容导航](https://androidperformance.com/2019/12/01/BlogMap/) ：个人博客内容的一个导航。
3.  [个人整理和搜集的优秀博客文章 - Android 性能优化必知必会](https://androidperformance.com/2018/05/07/Android-performance-optimization-skills-and-tools/) ：欢迎大家自荐和推荐 （微信私聊即可）
4.  [Android性能优化知识星球](https://www.androidperformance.com/2023/12/30/the-performance/) ： 欢迎加入，多谢支持～

> **一个人可以走的更快 , 一群人可以走的更远**

[![微信扫一扫](https://www.androidperformance.com/images/WechatIMG581.png)](https://www.androidperformance.com/images/WechatIMG581.png)