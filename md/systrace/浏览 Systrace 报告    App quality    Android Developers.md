---
created: 2025-06-04T10:32:07 (UTC +08:00)
tags: []
source: https://developer.android.google.cn/topic/performance/tracing?hl=cs
author: 
---

# 浏览 Systrace 报告  |  App quality  |  Android Developers

> ## Excerpt
> 本指南介绍了如何浏览和解读 Systrace 报告。如需解读 Perfetto 报告，请参阅跟踪处理器文档。

---
本指南介绍了如何浏览和解读 [Systrace](https://developer.android.google.cn/topic/performance/tracing?hl=cs) 报告。如需解读 Perfetto 报告，请参阅[跟踪处理器](https://perfetto.dev/#/trace-processor.md)文档。

## 典型报告的元素

Systrace 会生成包含多个部分的输出 HTML 文件。该报告列出了每个进程的线程。如果给定线程会渲染界面帧，该报告还会沿时间轴指明所渲染的帧。当您在报告中从左向右移动时，时间会向前推移。

报告从上到下包含以下几个部分。

### 用户互动

第一部分包含表示应用或游戏中的具体用户互动（例如点按设备屏幕）的条形图。这些互动可用作有用的时间标记。

### CPU 活动

下一部分显示了表示每个 CPU 中的线程活动的条形图。长条显示所有应用（包括应用或游戏）的 CPU 活动。

CPU 活动部分可展开，以便您查看每个 CPU 的时钟频率。图 1 显示了收起的 CPU 活动部分示例，图 2 显示了显示时钟频率的展开版本：

![Systrace 报告的屏幕截图](https://developer.android.google.cn/static/topic/performance/images/tracing/cpu-activity-collapsed.svg?hl=cs)

**图 1.** Systrace 报告中的 CPU 活动示例（收起后的视图）

___

![Systrace 报告的屏幕截图](https://developer.android.google.cn/static/topic/performance/images/tracing/cpu-activity-expanded.svg?hl=cs)

**图 2.** Systrace 报告中显示 CPU 时钟频率的 CPU 活动示例（展开后的视图）

### 系统事件

此部分中的直方图会显示特定的系统级事件，例如特定对象的纹理计数和总大小。

值得仔细检查的直方图是标记为 **SurfaceView** 的直方图。计数表示已传递到显示管道并等待显示在设备屏幕上的组合帧缓冲区的数量。由于大多数设备都会进行双重或三重缓冲，因此该计数几乎总为 0、1 或 2。

描绘 Surface Flinger 进程（包括 VSync 事件和界面线程交换工作）的其他直方图，如图 3 所示：

![Systrace 报告的屏幕截图](https://developer.android.google.cn/static/topic/performance/images/tracing/surface-flinger.svg?hl=cs)

**图 3.** Systrace 报告中的 Surface Flinger 图表示例

### 显示帧

这一部分通常是报告中最顶部的部分，描绘了一条多色线条，后面是成堆的条形。这些形状表示已创建的特定线程的状态和帧堆栈。堆栈的每个层级代表对 `beginSection()` 的一次调用，或您为应用或游戏定义的[自定义跟踪事件](https://developer.android.google.cn/topic/performance/tracing/custom-events?hl=cs)的开头。

每个条形堆上方的多色线条表示特定线程随时间变化的一组状态。每段线条可以包含以下一种颜色：

绿色：正在运行

线程正在完成与某个进程相关的工作或正在响应中断。

蓝色：可运行

线程可以运行但目前未进行调度。

白色：休眠

线程没有可执行的任务，可能是因为线程在遇到互斥锁定时被阻止。

橙色：不可中断的休眠

线程在遇到 I/O 操作时被阻止或正在等待磁盘操作完成。

紫色：可中断的休眠

线程在遇到另一项内核操作（通常是内存管理）时被阻止。

## 键盘快捷键

下表列出了查看 Systrace 报告时可以使用的键盘快捷键：

## 调查性能问题

与 Systrace 报告互动时，您可以检查记录期间的设备 CPU 使用情况。如需浏览 HTML 报告方面的帮助，请查看[键盘快捷键](https://developer.android.google.cn/topic/performance/tracing?hl=cs#keyboard-shortcuts)部分，或点击报告右上角的 **?** 按钮。

以下各部分介绍了如何检查报告中的信息以查找和修复性能问题。

### 识别性能问题

浏览 Systrace 报告时，您可以通过执行以下一项或多项操作来更轻松地识别性能问题：

-   通过在时间间隔周围绘制一个矩形来选择所需的时间间隔。
-   使用标尺工具标记或高亮显示问题区域。
-   依次点击 **View Options > Highlight VSync**，显示每项屏幕刷新操作。

### 检查界面帧和提醒

如图 4 所示，Systrace 报告列出了渲染界面帧的每个进程，并指明了沿时间轴渲染的每个帧。在 16.6 毫秒内渲染的必须保持每秒 60 帧稳定帧速率的帧以绿色圆圈表示。渲染时间超过 16.6 毫秒的帧以黄色或红色帧圆圈表示。

![已放大帧视图](https://developer.android.google.cn/static/topic/performance/images/tracing/frame-unselected.png?hl=cs)

**图 4.** 放大长时间运行的帧后的 Systrace 显示界面

点击某个帧圆圈可将其高亮显示，并提供有关系统为渲染该帧所做工作的其他信息，包括提醒。此报告还会显示系统在渲染该帧时执行的方法。您可以调查这些方法以确定界面卡顿的可能原因。

![已选择有问题的帧](https://developer.android.google.cn/static/topic/performance/images/tracing/frame-selected.png?hl=cs)

**图 5.** 选择有问题的帧后，跟踪报告下方会显示一条提醒，用于指明问题所在

选择运行速度慢的帧后，您可能会在报告的底部窗格中看到一条提醒。图 5 中显示的提醒指明帧的主要问题是在 [`ListView`](https://developer.android.google.cn/reference/android/widget/ListView?hl=cs) 回收和重新绑定上花费了太多时间。指向跟踪记录中相关事件的链接可详细说明系统在此期间执行的操作。

如需查看此工具在您的跟踪记录中发现的每条提醒以及设备触发每条提醒的次数，请点击窗口最右侧的 **Alerts** 标签页，如图 6 所示。**Alerts** 面板可帮助您了解跟踪记录中出现的问题以及这些问题导致出现卡顿的频率。您可以将此面板视为要修正的 bug 列表。通常情况下，只需对一个区域进行细微改动或改进即可移除整组提醒。

![显示的“Alert”标签页](https://developer.android.google.cn/static/topic/performance/images/tracing/frame-selected-alert-tab.png?hl=cs)

**图 6.** 点击 **Alert** 按钮可显示提醒标签页

如果您发现在界面线程上执行的工作太多，请使用以下方法之一来帮助确定哪些方法占用了过多的 CPU 时间：

-   如果您想了解哪些方法可能会导致瓶颈，请在这些方法中添加跟踪标记。如需了解详情，请参阅有关如何[在代码中定义自定义事件](https://developer.android.google.cn/topic/performance/tracing/custom-events?hl=cs)的指南。
-   如果您不确定界面瓶颈的来源，请使用 Android Studio 中提供的 [CPU 分析器](https://developer.android.google.cn/studio/profile/cpu-profiler?hl=cs)。您可以[生成跟踪日志](https://developer.android.google.cn/studio/profile/generate-trace-logs?hl=cs)，然后使用 CPU 分析器导入和检查这些日志。
