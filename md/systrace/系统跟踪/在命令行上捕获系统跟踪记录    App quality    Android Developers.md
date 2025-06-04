---
created: 2025-06-04T10:33:05 (UTC +08:00)
tags: []
source: https://developer.android.google.cn/topic/performance/tracing?hl=cs
author: 
---

# 在命令行上捕获系统跟踪记录  |  App quality  |  Android Developers

> ## Excerpt
> 了解 Systrace 工具；您可以借助该工具收集和检查设备上在系统一级运行的所有进程的时间信息。

---
## 在命令行上捕获系统跟踪记录

-   On this page
-   [语法](https://developer.android.google.cn/topic/performance/tracing?hl=cs#syntax)
    -   [全局选项](https://developer.android.google.cn/topic/performance/tracing?hl=cs#global_options)
    -   [命令和命令选项](https://developer.android.google.cn/topic/performance/tracing?hl=cs#command_options)

`systrace` 命令会调用 [Systrace 工具](https://developer.android.google.cn/topic/performance/tracing?hl=cs)，您可以借助该工具收集和检查设备上在系统一级运行的所有进程的时间信息。

本文档说明了如何从命令行生成 Systrace 报告。 在搭载 Android 9（API 级别 28）或更高版本的设备上，您还可以使用 [System Tracing 系统应用](https://developer.android.google.cn/topic/performance/tracing/on-device?hl=cs)生成 Systrace 报告。

如需运行 `systrace`，请完成以下步骤：

1.  从 Android Studio [下载并安装最新的 Android SDK 工具](https://developer.android.google.cn/studio/intro/update?hl=cs#sdk-manager)。
2.  安装 [Python](http://www.python.org/) 并将其添加到工作站的 `PATH` 环境变量中。
3.  将 `android-sdk/platform-tools/` 添加到 `PATH` 环境变量。此目录包含由 `systrace` 程序调用的 Android 调试桥二进制文件 (adb)。
4.  使用 [USB 调试连接](https://developer.android.google.cn/tools/device?hl=cs#setting-up)将搭载 Android 4.3（API 级别 18）或更高版本的设备连接到开发系统。

`systrace` 命令在 Android SDK 工具软件包中提供，并且可以在 `android-sdk/platform-tools/systrace/` 中找到。

## 语法

如需为应用生成 HTML 报告，您需要使用以下语法通过命令行运行 `systrace`：

```
python systrace.py [options] [categories]
```

例如，以下命令会调用 `systrace` 来记录设备活动，并生成一个名为 `mynewtrace.html` 的 HTML 报告。此类别列表是大多数设备的合理默认列表。

```
$ python systrace.py -o mynewtrace.html sched freq idle am wm gfx view \    binder_driver hal dalvik camera input res memory
```

**提示**：如果要在跟踪输出中查看任务名称，必须在命令参数中添加 `sched` 类别。

如需查看已连接设备支持的类别列表，请运行以下命令：

```
$ python systrace.py --list-categories
```

如果您未指定任何类别或选项，`systrace` 会生成包含所有可用类别的报告，并使用默认设置。可用类别取决于您所使用的已连接设备。

### 全局选项

| 全局选项 | 说明 |
| --- | --- |
| `-h | --help` | 显示帮助消息。 |
| `-l | --list-categories` | 列出您的已连接设备可用的跟踪类别。 |

### 命令和命令选项

| 命令和选项 | 说明 |
| --- | --- |
| `-o file` | 将 HTML 跟踪报告写入指定的 file。如果您未指定此选项，`systrace` 会将报告保存到 `systrace.py` 所在的目录中，并将其命名为 `trace.html`。 |
| `-t N | --time=N` | 跟踪设备活动 N 秒。如果您未指定此选项，`systrace` 会提示您在命令行中按 Enter 键结束跟踪。 |
| `-b N | --buf-size=N` | 使用 N 千字节的跟踪缓冲区大小。使用此选项，您可以限制跟踪期间收集到的数据的总大小。 |
| `-k functions   | --ktrace=functions` | 跟踪逗号分隔列表中指定的特定内核函数的活动。 |
| `-a app-name   | --app=app-name` | 启用对应用的跟踪，指定为包含[进程名称](https://developer.android.google.cn/guide/topics/manifest/application-element?hl=cs#proc)的逗号分隔列表。 这些应用必须包含 `[Trace](https://developer.android.google.cn/reference/android/os/Trace?hl=cs)` 类中的跟踪插桩调用。您应在分析应用时指定此选项。很多库（例如 `[RecyclerView](https://developer.android.google.cn/reference/androidx/recyclerview/widget/RecyclerView?hl=cs)`）都包括跟踪插桩调用，这些调用可在您启用应用级跟踪时提供有用的信息。如需了解详情，请参阅[定义自定义事件](https://developer.android.google.cn/topic/performance/tracing/custom-events?hl=cs)。
如需跟踪搭载 Android 9（API 级别 28）或更高版本的设备上的所有应用，请传递用添加引号的通配符字符 `"*"`。

 |
| `--from-file=file-path` | 根据文件（例如包含原始跟踪数据的 TXT 文件）创建交互式 HTML 报告，而不是运行实时跟踪。 |
| `-e device-serial   | --serial=device-serial` | 在已连接的特定设备（由对应的[设备序列号](https://developer.android.google.cn/studio/command-line/adb?hl=cs#devicestatus)标识）上进行跟踪。 |
| `categories` | 包含您指定的系统进程的跟踪信息，如 `gfx` 表示用于渲染图形的系统进程。您可以使用 `-l` 命令运行 `systrace`，以查看已连接设备可用的服务列表。 |

No recommendations at this time.

Try [signing in](https://developer.android.google.cn/topic/performance/tracing?hl=cs#) to your Google account.

本页面上的内容和代码示例受[内容许可](https://developer.android.google.cn/license?hl=cs)部分所述许可的限制。Java 和 OpenJDK 是 Oracle 和/或其关联公司的注册商标。

最后更新时间 (UTC)：2024-09-20。
