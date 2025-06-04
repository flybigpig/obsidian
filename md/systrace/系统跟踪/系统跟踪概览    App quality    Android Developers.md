---
created: 2025-06-04T10:28:48 (UTC +08:00)
tags: []
source: https://developer.android.google.cn/topic/performance/tracing?hl=cs
author: 
---

# 系统跟踪概览  |  App quality  |  Android Developers

> ## Excerpt
> “系统跟踪”就是记录短时间内的设备活动。系统跟踪会生成轨迹文件，该文件可用于生成系统报告。此报告有助于您了解如何最有效地提升应用或游戏的性能。

---
## 系统跟踪概览

-   On this page
-   [系统跟踪指南](https://developer.android.google.cn/topic/performance/tracing?hl=cs#guides)

“系统跟踪”就是记录短时间内的设备活动。系统跟踪会生成轨迹文件，该文件可用于生成系统报告。此报告有助于您了解如何最有效地提升应用或游戏的性能。

有关进行跟踪和性能分析的全面介绍，请参阅 Perfetto 文档中的[跟踪 101](https://perfetto.dev/docs/tracing-101) 页面。

Android 平台提供了多种不同的跟踪记录获取途径：

-   Android Studio CPU 和内存分析器
-   Perfetto 命令行工具（Android 10 及更高版本）
-   系统跟踪实用程序
-   Systrace 命令行工具

在您与应用互动时，Android Studio CPU 性能分析器可实时检查应用的 CPU 使用情况和线程活动。您也可以在录制的方法轨迹、函数轨迹和系统轨迹中检查该详细信息。内存分析器可让您大致了解与触摸事件、[`Activity`](https://developer.android.google.cn/reference/android/app/Activity?hl=cs) 更改和垃圾回收事件相关的内存用量。

Perfetto 是 Android 10 中引入的平台级跟踪工具。这是适用于 Android、Linux 和 Chrome 的成熟开源跟踪项目。与 Systrace 不同，它提供数据源超集，可让您以协议缓冲区二进制流形式录制任意长度的轨迹。您可以在 [Perfetto 界面](https://ui.perfetto.dev/#!/)中打开这些轨迹。

系统跟踪实用程序是一款 Android 工具，用于将设备活动保存到轨迹文件中。在搭载 Android 10（API 级别 29）或更高版本的设备上，轨迹文件会以 Perfetto 格式保存，如本文档后面部分所示。在搭载较低版本 Android 系统的设备上，轨迹文件会以 Systrace 格式保存。

Systrace 是由旧版平台提供的命令行工具，可录制短时间内的设备活动并将结果保存在压缩的文本文件中。该工具会生成一份报告，其中汇总了 Android 内核中的数据，例如 CPU 调度程序、磁盘活动和应用线程。Systrace 适用于所有 Android 平台版本，但我们建议将 Perfetto 用于搭载 Android 10 及更高版本的设备。

![Perfetto 轨迹视图的屏幕截图](https://developer.android.google.cn/static/topic/performance/images/tracing/perfetto.svg?hl=cs)

**图 1.** Perfetto 轨迹视图示例，其中显示了与某个应用之间大约 20 秒的交互情况。

![Systrace 报告的屏幕截图](https://developer.android.google.cn/static/images/systrace/overview.png?hl=cs)

**图 2.** Systrace HTML 报告示例，其中显示了与某个应用之间时长为 5 秒的交互情况。

这两份报告都提供在给定时间段内，Android 设备的系统进程总体情况。该报告还检查了捕获到的跟踪信息，以突出显示发现的问题（例如界面卡顿或耗电量高）。

Perfetto 和 Systrace 可交互使用：

-   在 Perfetto 界面中打开 Perfetto 文件和 Systrace 文件。在 Perfetto 界面中使用旧版 Systrace 查看器打开 Systrace 文件（点击 **Open with legacy UI** 链接）。
-   使用 `traceconv` 工具[将 Perfetto 轨迹转换为旧版 Systrace 文本格式](https://docs.perfetto.dev/#/traceconv.md)。

## 系统跟踪指南

如需详细了解系统跟踪工具，请参阅以下指南：

[**使用 CPU 性能分析器检查 CPU 活动**](https://developer.android.google.cn/studio/profile/cpu-profiler?hl=cs)

展示如何在 Android Studio 中分析应用的 CPU 使用情况和线程活动。

[**在设备上捕获系统轨迹**](https://developer.android.google.cn/topic/performance/tracing/on-device?hl=cs)

介绍如何在任何搭载 Android 9（API 级别 28）或更高版本的设备上直接捕获系统轨迹。

[**在命令行上捕获系统轨迹**](https://developer.android.google.cn/topic/performance/tracing/command-line?hl=cs)

定义可传递到 Systrace 命令行界面的不同选项和标志。

[**使用 adb 运行 Perfetto**](https://developer.android.google.cn/studio/command-line/perfetto?hl=cs)

介绍如何运行 `perfetto` 命令行工具来捕获轨迹。

[**快速入门：在 Android 设备上录制轨迹**](https://perfetto.dev/#/running.md)

外部文档，介绍如何构建和运行 `perfetto` 命令行工具来捕获轨迹。

[**快速入门：在 Android 设备上录制轨迹**](https://perfetto.dev/#/running.md)

Perfetto 网页版轨迹查看器可打开 Perfetto 轨迹并显示完整报告。您还可以使用旧版界面选项在此查看器中打开 Systrace 轨迹。

[**浏览 Systrace 报告**](https://developer.android.google.cn/topic/performance/tracing/navigate-report?hl=cs)

列出典型报告的各个元素，提供用于浏览报告的键盘快捷键，并介绍如何识别性能问题的类型。

[**定义自定义事件**](https://developer.android.google.cn/topic/performance/tracing/custom-events?hl=cs)

介绍如何对代码的特定部分应用自定义标签，以便更轻松地在 Systrace 或 Perfetto 中分析根本原因。

本页面上的内容和代码示例受[内容许可](https://developer.android.google.cn/license?hl=cs)部分所述许可的限制。Java 和 OpenJDK 是 Oracle 和/或其关联公司的注册商标。

最后更新时间 (UTC)：2025-05-23。
