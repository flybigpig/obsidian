![](https://csdnimg.cn/release/blogv2/dist/pc/img/original.png)

[xhBruce](https://xhbruce.blog.csdn.net/ "xhBruce") ![](https://csdnimg.cn/release/blogv2/dist/pc/img/newUpTime2.png) 已于 2023-07-06 08:28:02 修改

于 2023-07-06 04:03:11 首次发布

#### [Systrace](https://so.csdn.net/so/search?q=Systrace&spm=1001.2101.3001.7020)入门

-   [1、观看优秀Blog文章](https://blog.csdn.net/qq_23452385/article/details/131566907#1Blog_3)
-   [2、Systrace命令](https://blog.csdn.net/qq_23452385/article/details/131566907#2Systrace_12)
-   -   [2.1 Systrace工具位置](https://blog.csdn.net/qq_23452385/article/details/131566907#21_Systrace_15)
    -   [2.2 Strace.py脚本](https://blog.csdn.net/qq_23452385/article/details/131566907#22_Stracepy_17)
    -   -   [2.2.1 全局选项](https://blog.csdn.net/qq_23452385/article/details/131566907#221__26)
        -   [2.2.2 命令和命令选项](https://blog.csdn.net/qq_23452385/article/details/131566907#222__31)
    -   [2.3 其他方式](https://blog.csdn.net/qq_23452385/article/details/131566907#23__42)
-   [3、浏览 Systrace 报告](https://blog.csdn.net/qq_23452385/article/details/131566907#3_Systrace__46)
-   -   [3.1 键盘快捷键](https://blog.csdn.net/qq_23452385/article/details/131566907#31__48)
    -   [3.2 典型报告的元素](https://blog.csdn.net/qq_23452385/article/details/131566907#32__66)
    -   [3.3 调查性能问题(着手地方)](https://blog.csdn.net/qq_23452385/article/details/131566907#33__90)
    -   -   [3.3.1 其他方法帮助确定](https://blog.csdn.net/qq_23452385/article/details/131566907#331__103)
        -   [3.3.2 Perfetto 界面打开文件](https://blog.csdn.net/qq_23452385/article/details/131566907#332_Perfetto__108)

___

## 1、观看优秀Blog文章

## 2、Systrace命令

> Systrace 是平台提供的旧版命令行工具，可记录短时间内的设备活动，并保存在压缩的文本文件中。该工具会生成一份报告，其中汇总了 Android 内核中的数据，例如 CPU 调度程序、磁盘活动和应用线程。Systrace 适用于 Android 4.3（API 级别 18）及更高版本的所有平台版本，但建议将 Perfetto 用于运行 Android 10 及更高版本的设备。  
> Perfetto 是 Android 10 中引入的平台级跟踪工具。这是适用于 Android、Linux 和 Chrome 的成熟开源跟踪项目。与 Systrace 不同，它提供数据源超集，可让您以协议缓冲区二进制流形式记录任意长度的跟踪记录。您可以在 [Perfetto 界面](https://ui.perfetto.dev/#!/) 中打开这些跟踪记录。

### 2.1 Systrace工具位置

> `systrace` 命令在 `Android SDK` 工具软件包中提供，并且可以在 `android-sdk/platform-tools/systrace/` 中找到

### 2.2 Strace.py脚本

`python systrace.py [options] [categories]`

```
$ python systrace.py -o mynewtrace.html sched freq idle am wm gfx view binder_driver hal dalvik camera input res memory
```

生成一个名为 `mynewtrace.html` 的 HTML 报告，其中tag参数查看`python systrace.py --list-categories`  
或 `adb shell atrace --list_categories`  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3573cf9ed623ed4f21030a880a86e028.png)

#### 2.2.1 全局选项

| 全局选项 | 说明 |
| --- | --- |
| \-h | –help 显示帮助消息。 |
| \-l | –list-categories 列出您的已连接设备可用的跟踪类别。 |

#### 2.2.2 命令和命令选项

| 命令和选项 | 说明 |
| --- | --- |
| \-o file | 将 HTML 跟踪报告写入指定的 file。如果您未指定此选项，systrace 会将报告保存到 systrace.py 所在的目录中，并将其命名为 trace.html。 |
| \-t N | –time=N 跟踪设备活动 N 秒。如果您未指定此选项，systrace 会提示您在命令行中按 Enter 键结束跟踪。 |
| \-b N | –buf-size=N 使用 N 千字节的跟踪缓冲区大小。使用此选项，您可以限制跟踪期间收集到的数据的总大小。 |
| \-k functions  
|–ktrace=functions | 跟踪逗号分隔列表中指定的特定内核函数的活动。 |
| \-a app-name  
|–app=app-name | 启用对应用的跟踪，指定为包含进程名称的逗号分隔列表。 这些应用必须包含 Trace 类中的跟踪插桩调用。您应在分析应用时指定此选项。很多库（例如 RecyclerView）都包括跟踪插桩调用，这些调用可在您启用应用级跟踪时提供有用的信息。如需了解详情，请参阅定义自定义事件。  
如需跟踪搭载 Android 9（API 级别 28）或更高版本的设备上的所有应用，请传递用添加引号的通配符字符 “\*”。 |
| –from-file=file-path | 根据文件（例如包含原始跟踪数据的 TXT 文件）创建交互式 HTML 报告，而不是运行实时跟踪。 |
| \-e device-serial  
|–serial=device-serial | 在已连接的特定设备（由对应的设备序列号标识）上进行跟踪。 |
| categories | 包含您指定的系统进程的跟踪信息，如 gfx 表示用于渲染图形的系统进程。您可以使用 -l 命令运行 systrace，以查看已连接设备可用的服务列表。 |

### 2.3 其他方式

-   在搭载 Android 9（API 级别 28）或更高版本的设备上，您还可以使用 [“系统跟踪”系统应用](https://developer.android.google.cn/topic/performance/tracing/on-device?hl=zh-cn) 生成 Systrace 报告
-   **monitor 程序**: 进入Android SDK根目录下的 tools 目录，运行 monitor 程序。

## 3、浏览 Systrace 报告

**`chrome://tracing/`** 浏览器中打开HTML报告

### 3.1 键盘快捷键

| 键 | 说明 |
| --- | --- |
| W | 放大跟踪时间轴。 |
| A | 在跟踪时间轴上向左平移。 |
| S | 缩小跟踪时间轴。 |
| D | 在跟踪时间轴上向右平移。 |
| E | 以当前鼠标位置为中定位跟踪时间轴。 |
| M | 高亮当前选区。 |
| 1 | 将当前正在使用中的选择模型更改为“选择”模式。 对应于鼠标选择器工具栏中显示的第 1 个按钮（请参见右图）。![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/7aa4b3236542fac3361776291ea609a2.png) |
| 2 | 将当前正在使用中的选择模型更改为“平移”模式。 对应于鼠标选择器工具栏中显示的第 2 个按钮 |
| 3 | 将当前正在使用中的选择模型更改为“缩放”模式。 对应于鼠标选择器工具栏中显示的第 3 个按钮 |
| 4 | 将当前正在使用中的选择模型更改为“计时”模式。 对应于鼠标选择器工具栏中显示的第 4 个按钮 |
| G | 在当前所选任务的开头显示网格。 |
| Shift + G | 在当前所选任务的末尾显示网格。 |
| 向左箭头 | 在当前选定的时间轴上选择上一个事件。 |
| 向右箭头 | 在当前选定的时间轴上选择下一个事件。 |

### 3.2 典型报告的元素

> 每个条形堆上方的多色线条表示特定线程随时间变化的一组状态。每段线条可以包含以下一种颜色：
> 
> **`绿色：正在运行`**  
> 线程正在完成与某个进程相关的工作或正在响应中断。  
> **`蓝色：可运行`**  
> 线程可以运行但目前未进行调度。  
> **`白色：休眠`**  
> 线程没有可执行的任务，可能是因为线程在遇到[互斥锁](https://so.csdn.net/so/search?q=%E4%BA%92%E6%96%A5%E9%94%81&spm=1001.2101.3001.7020)定时被阻止。  
> **`橙色：不可中断的休眠`**  
> 线程在遇到 I/O 操作时被阻止或正在等待磁盘操作完成。  
> **`紫色：可中断的休眠`**  
> 线程在遇到另一项内核操作（通常是内存管理）时被阻止。

### 3.3 调查性能问题(着手地方)

> 浏览 Systrace 报告时，您可以通过执行以下一项或多项操作来更轻松地识别性能问题：
> 
> 1.  通过在时间间隔周围绘制一个矩形来选择所需的时间间隔。
> 2.  使用标尺工具标记或高亮显示问题区域。
> 3.  依次点击 `View Options > Highlight VSync`，显示每项屏幕刷新操作。

Systrace 报告列出了渲染界面帧的每个进程，并指明了沿时间轴渲染的每个帧。在 16.6 毫秒内渲染的必须保持每秒 60 帧稳定帧速率的帧以绿色圆圈表示。渲染时间超过 16.6 毫秒的帧以黄色或红色帧圆圈表示。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/a5001b65c252112724b5ef02dd00d42b.png)  
点击某个帧圆圈可将其高亮显示，并提供有关系统为渲染该帧所做工作的其他信息，包括提醒。此报告还会显示系统在渲染该帧时执行的方法。您可以调查这些方法以确定界面卡顿的可能原因。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f906b23ffb65a85912c00f3f48831e81.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/8644dd0e24ab67e3e00ef1b280d1eaa8.png)

#### 3.3.1 其他方法帮助确定

如果您发现在界面线程上执行的工作太多，请使用以下方法之一来帮助确定哪些方法占用了过多的 CPU 时间：

-   如果您想了解哪些方法可能会导致瓶颈，请在这些方法中添加跟踪标记。如需了解详情，请参阅有关如何 [在代码中定义自定义事件](https://developer.android.google.cn/topic/performance/tracing/custom-events?hl=zh-cn) 的指南。
-   如果您不确定界面瓶颈的来源，请使用 Android Studio 中提供的 [CPU 分析器](https://developer.android.google.cn/studio/profile/cpu-profiler?hl=zh-cn)。您可以 [生成跟踪日志](https://developer.android.google.cn/studio/profile/generate-trace-logs?hl=zh-cn)，然后使用 CPU 分析器导入和检查这些日志。

#### 3.3.2 Perfetto 界面打开文件

-   [Perfetto 界面](https://ui.perfetto.dev/#!/) ：在 Perfetto 界面中打开 Perfetto 文件和 Systrace 文件。在 Perfetto 界面中使用旧版 Systrace 查看器打开 Systrace 文件（使用 Open with legacy UI 链接）。
-   使用 traceconv 工具将 Perfetto 跟踪记录转换为旧版 Systrace 文本格式