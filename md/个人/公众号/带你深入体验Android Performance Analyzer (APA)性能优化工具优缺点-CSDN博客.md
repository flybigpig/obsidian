### Android Performance Analyzer (APA)介绍

Optimize your app or game with Android’s new profiler and performance analysis tool for the Android ecosystem.  
使用Android 生态系统推出的全新性能分析工具，优化您的应用或游戏。

简单说就是google专门为了 性能优化 专门做了一个界面类似android studio的软件，大家可能第一反应是不是有网页版本的Perfetto网站么为啥还要这个APA，整体体验目前来看确实是有一些

### 使用APA体验感受



这里针对APA的使用体验主要就是和Perfetto网页版本进行对比，因为除非我们APA相比Perfetto网页版本有更多的优点我们才可能选择APA。

##### 体验比Perfetto好的部分

**1、良好的抓取Perfetto trace体验**

这里因为APA是独立的一个软件，相比Perfetto网页的抓trace方式自然要方便很多。  
在APA上可以实现一键抓取分析trace，但是在浏览器Perfetto网页抓取要配置一些辅助脚本。

Perfetto抓取要进行辅助配置![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/e88cce6b60c240d89626e301a6f97c32.png)  
APA基本上只需要一键点击就可以抓取（当然APA会自动帮我们安装对应的apk到手机）  
![APA的抓取方式](https://i-blog.csdnimg.cn/direct/2c3c1d670d434c34a3df7d04da0bfa69.png)

**2、查看trace部分优点**

2.1 有明显展示cpu大小核  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/7b6ff82dd44f493abe261fe33897e4bb.png)2.2 有自动置顶核心的一些track 轨道

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/9c98d6f46b0448db949389cb36715ce5.png)  
2.3 sf显示相关buf的轨道有单独列出  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/7c6add2ca24046a1ab99d327f5d18399.png)  
2.4 多个trace文件可以同时对比查看  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/4d86cebf90734c0dbe9b958e44f26fe2.png)  
其他，，，，省略优点，暂时还没有体验，欢迎留言补充。

##### 体验比Perfetto不好的部分

最大致命缺点，就是无法跟踪跳转线程之间的唤醒流程，这个功能如果后续不加上，基本上没有必要用这个APA。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/ff50a71d4626420aa2726153e4092f73.png)

##### 目前APA还无法体验到的部及相关疑问（如有解惑的欢迎留言联系）

**ai部分**

原本以为是这个apa自带了一个ai agent 这种特性，瞬间感觉原来在Perfetto网页版本上自己做ai助手瞬间感觉没有意义。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/ad9a578b1acb4a7694aec982672980e4.png)但是其实仔细阅读后发现这里只是说可以借助android Perfetto sql的skill，让ai 工具可以编写对应sql语句，然后再是使用这些ai 工具生成的sql到APA中运行方式来辅助我们分析。

**Perfetto分析时候无法抓取到截图**

在看官网的相关截图时候，发现官网截图展示，可以在Perfetto工具查看trace时候，附带每一帧截图的情况。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/2cb9f1ab5e834f6a96c23aed54e0917a.png)但是马哥自己抓取的trace中根本没有这个，具体抓取附带截图方式等和抓取条件目前还不清楚。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/b540313c780047fe885d9f8b26b91e08.png)

### APA的 下载

下载地址：  
[https://developer.android.com/android-performance-analyzer?authuser=1](https://developer.android.com/android-performance-analyzer?authuser=1)

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/e2ce000df5bb40b5a1ffc0443131d307.png)基本上的主流操作系统都支持，大家自行根据需要进行下载。

### APA快速入门

如果您已经熟悉系统性能分析，本指南将提供满足要求、安装软件以及开始使用 Android 性能分析器中的系统性能分析器运行和查看跟踪信息所需的所有信息。否则，请参阅后续步骤以获取更深入的指导链接。

### 要求

为了成功使用系统分析器记录系统跟踪，运行该软件的计算机、运行被测应用程序的设备以及被测应用程序本身必须满足一些要求。

#### 计算机要求

运行系统分析器的计算机必须满足以下要求：

它必须安装以下操作系统之一：  
Windows。64位 Windows 10 或更高版本。  
macOS 。macOS 12 或更高版本。  
必须使用基于 ARM 架构的芯片。不支持使用 Intel 芯片的 Mac 电脑。  
Linux。  
64 位机器必须安装64 位机器所需的库。  
必须安装 Android SDK，包括 Platform-Tools软件包。  
ANDROID\_HOME必须设置环境变量。

#### 测试设备要求

运行被测应用程序的设备必须满足以下要求：

支持的Android设备，运行Android 12或更高版本。  
USB 数据线。  
必须启用Android 调试桥 ( adb)调试，并且必须可以通过 访问设备adb。如果存在“通过 USB 安装”选项，请启用它。  
注意：所列要求是使用系统分析器测试特定设备的最低要求。不同的设备和 GPU 会暴露不同的数据，这可能导致某些跟踪数据不可用。

#### 设备验证

为确保系统跟踪有效，系统分析器会在您首次连接新设备时运行验证检查。验证过程中请勿干扰设备，否则可能导致设备验证失败。如果设备验证失败但设置正确，您可以单击“设备”下拉菜单中的“重试”按钮或断开设备连接并重新连接来重试验证。

设备通过验证后，在“配置录制”窗口中，设备名称旁边会出现一个绿色对勾。

应用要求  
虽然这不是硬性要求，但我们建议您采取以下措施，以使用户画像尽可能有用和准确：

使用应用或游戏的正式发布版本，或者使用启用了性能选项（例如编译器标志或打包优化）的版本。  
如果您正在分析使用Vulkan进行图形处理的应用或游戏，请将Android 清单文件中的debuggable 属性设置为true。这样就可以将 Vulkan 特有的数据包含在系统跟踪中。  
对于 Java 和 Kotlin 应用，请将debuggable 属性设置 为false，以使 Android 运行时能够以最高优化效率运行。这有助于系统跟踪结果反映真实世界的性能。对于纯 C/C++ 应用或原生游戏循环来说，这影响不大，但托管代码应用需要这样做才能生成准确的性能分析数据。

### 基本工作流程

执行以下步骤以捕获分析数据并打开生成的跟踪文件进行分析：

打开 Android Performance Analyzer，然后选择现有项目或单击“新建项目”创建新项目。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/b0dd2d8a29e84992a5f88f09eee86892.png)  
图 1 ： Android 性能分析器启动窗口的屏幕截图。

输入新项目的名称和目录位置。Android Performance Analyzer 会自动打开您的空项目。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/f259d4aacf854bb49e05bddc5450580e.png)

图 2：一个空白项目的屏幕截图。

点击标题栏左侧的“录制轨迹”按钮，打开“配置录制”窗口。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/8919452c27c84c9faf96d34ff230972a.png)

图 3 ：配置录制窗口的屏幕截图。  
“配置录制”窗口初始打开时会显示默认的跟踪配置。请根据需要调整选项，然后单击“确定”。这将打开“控制录制”窗口，并自动在您的测试设备上启动应用程序。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/2c3c1d670d434c34a3df7d04da0bfa69.png)

图 4 ：控制录制窗口的屏幕截图 。  
运行测试。跟踪过程会一直运行，直到您点击“停止”按钮或预设的持续时间结束（如果您已设置）。Android 性能分析器会检索跟踪数据，然后自动在跟踪视图中打开结果。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/90a132cb242a43df9ace33b2aeb38a2b.png)

图 5：示例跟踪视图的屏幕截图。  
使用跟踪视图与收集的数据进行交互和分析。

### 查看trace部分

您可以在 Android 性能分析器的系统分析器中查看之前记录的所有系统跟踪信息。本指南演示如何使用跟踪视图与记录的数据进行交互。有关跟踪视图中显示的数据的详细说明，请参阅“了解跟踪数据”。  
https:// developer .android.com/android-performance-analyzer/view/data?authuser=1

### 浏览trace视图

跟踪视图提供了多种与记录数据交互的方式，以便进行快速、直观的分析，并支持自定义查询。单击 跟踪视图右上角的“？号图标 查看键盘鼠标快捷键”，即可查看键盘和鼠标快捷键列表。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/89b1eb72bef649a19b85f738512136d3.png)

图 1：示例跟踪视图的屏幕截图。

##### 按名称筛选曲目

您可以在轨迹视图左上角的“按名称筛选轨迹”字段中输入内容，将展开的轨迹筛选为名称与搜索字符串匹配的轨迹。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/f49bb8c0d04b4aa795f4035a63e76d74.png)

##### 滚动、平移和缩放

您可以使用以下键盘快捷键在跟踪视图中进行导航：

A和D（或Left和Right）在时间轴上前后平移。  
W以及S放大或缩小。  
Up并可Down垂直滚动。  
按住Shift这些快捷键可以加快导航速度。  
您还可以通过点击和拖动来导航，使用跟踪视图右侧和底部的滚动条，或者使用触控板或鼠标滚轮进行水平和垂直滚动。

首次打开跟踪文件时，页面会默认以全缩小状态显示，以便查看整个时间线。要放大或缩小，请使用 键盘上的W和键。S

##### 查看详情

点击任意赛道上的某个事件，即可打开详情面板，查看为该事件收集的更详细的数据。

要关闭详细信息面板，可以单击面板右上角的折叠图标，或者按Esc键。

##### 选择时间范围

您可以通过单击并拖动跟踪视图顶部的时间线栏来选择时间范围，或者按住Ctrl(Cmd在 macOS 上) 并单击并拖动跟踪视图中的任意位置来选择时间范围。

要取消选择时间范围，请单击跟踪视图中选定切片之外的任意位置。

##### 框选项目

您可以点击并拖动鼠标，在多个轨道上选择一个时间范围，按住 Shift鼠标左键即可框选该时间范围内所有包含的轨道中的项目。

这还会打开一个详细信息面板，其中包含一个选项卡式的表格，其中包含所有选定的项目。

##### 同时查看多条trace文件

Android Performance Analyzer 继承了 IntelliJ 平台的 UI 功能，这意味着您可以使用选项卡、窗口或拆分视图同时打开多个跟踪文件。

此功能的一个特别有用的用例是在垂直分割视图中打开两个跟踪文件。这样可以对齐时间线，以便进行直接的视觉比较。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/235eefd9e5d74cfaae455e32484bed25.png)

图 2：以垂直分割视图打开的多个跟踪文件的屏幕截图。

### 自定义一些Trace查看操作

跟踪视图还提供了自定义选项，以便您在工作流程中最大限度地发挥作用。

##### 固定和断开轨道

已固定的轨道会显示在轨迹视图的顶部，并且在您向下滚动浏览可用轨道时始终可见。您可以将鼠标悬停在轨道名称栏上，然后点击出现的图钉图标来固定轨道。要取消固定轨道，请再次点击图钉图标。

##### 展开和折叠轨道

有些轨道可以展开以提供更精细的细节，也可以折叠以占用更少的空间。

##### 添加或删除书签

您可以通过点击轨迹视图顶部时间轴栏中的特定点来标记它们。每个标记都会显示一条贯穿轨迹视图中所有轨道的线条。您可以通过点击时间轴栏上相应的标记来切换每个标记的可见性，或者通过右键单击标记来完全删除该标记。

##### 状态持久性

关闭跟踪视图时，系统分析器会保存您固定的跟踪路径、书签、缩放级别和滚动位置。下次打开同一个跟踪文件时，跟踪视图的状态将会恢复。

要清除书签、将固定曲目重置为默认值或两者都重置，请单击 “重置选项”。

### 运行自定义查询

您可以点击 跟踪视图左上角的 SQL图标来打开SQL选项卡。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/12658b2c0997435cbdb754937214131b.png)

图 3 ： SQL选项卡的屏幕截图。  
在SQL选项卡中，您可以编写自定义PerfettoSQL查询以进行个性化分析。

点击 “运行查询”或按Ctrl+ Enter （ macOS 上为Cmd+ Enter）运行查询。  
如果查询结果跨越多个页面，请使用底部的导航按钮。  
单击 “复制查询”将查询窗口的内容复制到剪贴板。  
点击 “历史记录”可查看之前执行的查询的下拉列表（可滚动）。查询历史记录在跟踪文件和项目之间共享。  
点击 左下角的 “追踪”按钮，即可返回追踪视图。

文章部分参考：  
https://developer.android.com/android-performance-analyzer?authuser=1

原文地址：  
[https://mp.weixin.qq.com/s/s5iV8kuaKRO5L4mGMAVU2w](https://mp.weixin.qq.com/s/s5iV8kuaKRO5L4mGMAVU2w)