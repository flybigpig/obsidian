systrace 是分析 Android 设备性能的主要工具。不过，它实际上是多种其他工具的封装容器：它是 atrace 的主机端封装容器。atrace 是用于控制用户空间跟踪和设置 ftrace 的设备端可执行文件，也是 Linux 内核中的主要跟踪机制。systrace 使用 atrace 来启用跟踪，然后读取 ftrace 缓冲区并将其全部封装到一个独立的 HTML 查看器中。（虽然较新的内核支持 Linux 增强型柏克莱封包过滤器 (eBPF)，但以下文档内容仅适用于 3.18 内核（无 eFPF），因为 Pixel/Pixel XL 上使用的是 3.18 内核）。

systrace 归 Google Android 和 Google Chrome 团队所有，并作为 [Catapult 项目](https://github.com/catapult-project/catapult)的一部分实现开源。除 systrace 之外，Catapult 还包含其他多种实用程序。例如，除了可由 systrace 或 atrace 直接实现的功能之外，ftrace 还提供了其他功能，并且包含一些对调试性能问题至关重要的高级功能（这些功能需要 root 访问权限，通常也需要新的内核）。

## 运行 systrace

要在 Pixel/Pixel XL 上调试抖动问题，请从以下命令开始：

```
./systrace.py sched freq idle am wm gfx view sync binder_driver irq workq input -b 96000
```

当将该命令用于 GPU 活动和显示管道活动所需的附加跟踪点时，您将能够跟踪从用户输入直到屏幕上显示的帧。将缓冲区空间设置为较大的值，以避免丢失事件（因为如果没有较大的缓冲区，一些 CPU 在跟踪记录中的某个点之后将没有任何事件）。

在使用 systrace 的过程中，请记住，**每个事件都是由 CPU 上的活动触发的**。

因为 systrace 构建于 ftrace 之上，而 ftrace 在 CPU 上运行，所以 CPU 上的活动必须写入用于记录硬件变化情况的 ftrace 缓冲区。这意味着，如果您想知道显示栅栏状态变更的原因，则可以查看在确切的状态转换点上， CPU 在进行哪些活动（在 CPU 上进行的某些活动触发了日志中记录的这项变更）。此概念是使用 systrace 分析性能的基础。

## 示例：工作帧

该示例介绍了正常界面管道的 systrace。如需按照示例操作，请[下载跟踪记录的 .zip 文件](https://source.android.google.cn/static/docs/core/tests/debug/perf_traces.zip?hl=cs)（该文件还包含本部分中提及的其他跟踪记录），解压缩该文件，然后在浏览器中打开 `systrace_tutorial.html` 文件。注意，该 systrace 是一个大型文件；除非您在日常工作中有使用 systrace，否则相比您过去所见的单个跟踪记录，这个跟踪记录的体积很可能要大得多，包含的信息也多得多。

对于稳定的周期性工作负载（例如 TouchLatency），界面管道包含以下元素：

1.  SurfaceFlinger 中的 EventThread 会唤醒应用的界面线程，表明该渲染新帧了。
2.  应用使用 CPU 和 GPU 资源在界面线程、RenderThread 和 hwuiTask 中渲染帧。这是界面占用的大部分容量。
3.  应用使用 binder 将渲染的帧发送到 SurfaceFlinger，然后 SurfaceFlinger 进入休眠状态。
4.  SurfaceFlinger 中的第二个 EventThread 唤醒 SurfaceFlinger，以触发构图和显示输出。如果 SurfaceFlinger 确定没有任何任务需要执行，将返回休眠状态。
5.  SurfaceFlinger 利用硬件混合渲染器 (HWC)/硬件混合渲染器 2 (HWC2) 或 GL 处理构图。HWC/HWC2 构图的速度更快且功率更低，但存在限制，具体取决于系统芯片 (SoC)。这个步骤通常需要约 4-6 毫秒，但是可以与第 2 步同步进行，因为 Android 应用始终会进行三重缓冲（虽然应用始终会进行三重缓冲，但 SurfaceFlinger 中可能只有一个待处理帧在等待，这样它就看起来与双重缓冲一样）。
6.  SurfaceFlinger 将通过供应商驱动程序将最终输出调度到显示部分，然后返回休眠状态，等待 EventThread 唤醒。

下面我们详细介绍一下从 15409 毫秒开始的帧：

[![正常界面管道（EventThread 正在运行）](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_normal_pipeline.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_normal_pipeline.png?hl=cs "点击即可放大")

**图 1.** 正常界面管道（EventThread 正在运行）

图 1 显示了一个由多个正常帧围绕的正常帧，因此对于理解界面管道的工作原理来说，它是一个很好的切入点。TouchLatency 的界面线程所在行在不同的时间显示为不同的颜色。柱形表示线程的不同状态：

+   **灰色**：正在休眠。
+   **蓝色**：可运行（它可以运行，但是调度程序尚未选择让它运行）。
+   **绿色**：正在运行（调度程序认为它正在运行）。
+   **红色**：不可中断休眠（通常在内核中处于休眠锁定状态）。可以指示 I/O 负载，在调试性能问题时非常有用。
+   **橙色**：由于 I/O 负载而不可中断休眠。

如需查看不可中断休眠的原因（可从 `sched_blocked_reason` 跟踪点获取），请选择红色的不可中断休眠图块。

当 EventThread 运行时，TouchLatency 的界面线程会变为可运行。如需查看是什么唤醒的它，请点击蓝色部分：

[![TouchLatency 的界面线程](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_tl.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_tl.png?hl=cs "点击即可放大")

**图 2.** TouchLatency 的界面线程

图 2 显示了 TouchLatency 的界面线程被与 EventThread 对应的 tid 6843 唤醒。界面线程被唤醒、渲染一个帧，并将其加入队列以供 SurfaceFlinger 使用。

[![界面线程被唤醒、渲染一个帧，并将其加入队列以供 SurfaceFlinger 使用](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_wake_render_enqueue.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_wake_render_enqueue.png?hl=cs "点击即可放大")

**图 3.** 界面线程被唤醒、渲染一个帧，并将其加入队列以供 SurfaceFlinger 使用

如果跟踪记录中的 `binder_driver` 标记已启用，您可以选择一个 Binder 事务，并查看该事务涉及的所有进程的相关信息：

[![](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_binder_trans.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_binder_trans.png?hl=cs "点击即可放大")

**图 4.** Binder 事务

图 4 显示在 15423.65 毫秒，SurfaceFlinger 中的 Binder:6832\_1 由于 tid 9579（即 TouchLatency 的 RenderThread）变为可运行。此外，您还可以在 Binder 事务的两侧看到 queueBuffer。

在 SurfaceFlinger 端的 queueBuffer 期间，TouchLatency 中待处理帧的数量从 1 变为 2。

[![待处理帧从 1 个变为 2 个](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_pending_frames.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_pending_frames.png?hl=cs "点击即可放大")

**图 5.** 待处理帧从 1 个变为 2 个

图 5 表示三重缓冲，其中两个帧已完成渲染，应用将很快开始渲染第三个帧。这是因为一些帧已经丢弃，所以应用保留两个待处理的帧而不是一个，以避免以后再丢帧。

稍后，SurfaceFlinger 的主线程被第二个 EventThread 唤醒，以便它可以将较早的待处理帧输出到显示部分：

[![SurfaceFlinger 的主线程被第二个 EventThread 唤醒](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_sf_woken_et.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_sf_woken_et.png?hl=cs "点击即可放大")

**图 6.** SurfaceFlinger 的主线程被第二个 EventThread 唤醒

SurfaceFlinger 首先锁定较早的待处理缓冲区，而这将导致待处理缓冲区的数量从 2 减为 1。

[![SurfaceFlinger 首先锁定较早的待处理缓冲区](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_sf_latches_pend.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_sf_latches_pend.png?hl=cs "点击即可放大")

**图 7.** SurfaceFlinger 首先锁定较早的待处理缓冲区

锁定缓冲区后，SurfaceFlinger 会设置构图并将最终帧提交给显示部分（其中某些区段作为 `mdss` 跟踪点的一部分启用，因此它们可能不在您 SoC 的相应位置上）。

[![SurfaceFlinger 设置构图并提交最终帧](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_sf_comp_submit.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_sf_comp_submit.png?hl=cs "点击即可放大")

**图 8.** SurfaceFlinger 设置构图并提交最终帧

接下来，`mdss_fb0` 在 CPU 0 上被唤醒。`mdss_fb0` 是显示管道的内核线程，用于将渲染过的帧输出到显示部分。我们可以看到 `mdss_fb0` 位于跟踪记录中其自己所在行中的情形（向下滚动即可查看）。

[![mdss_fb0 在 CPU 0 上被唤醒](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_wake_cpu0.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_wake_cpu0.png?hl=cs "点击即可放大")

**图 9.** mdss\_fb0 在 CPU 0 上被唤醒

`mdss_fb0` 被唤醒，短暂运行，进入不可中断休眠状态，然后再次被唤醒。

## 示例：非工作帧

该示例介绍了用于调试 Pixel/Pixel XL 抖动问题的 systrace。如需按照示例操作，请[下载跟踪记录的 .zip 文件](https://source.android.google.cn/static/docs/core/tests/debug/perf_traces.zip?hl=cs)（该文件包含本部分中提及的其他跟踪记录），解压缩该文件，然后在浏览器中打开 `systrace_tutorial.html` 文件。

打开 systrace 时，您会看到如下内容：

[![在 Pixel XL 上运行的 TouchLatency（大多数选项已启用）](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_tl_pxl.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_tl_pxl.png?hl=cs "点击即可放大")

**图 10.** 在 Pixel XL 上运行的 TouchLatency（大多数选项已启用，包括 mdss 和 kgsl 跟踪点）

查找卡顿时，请检查 SurfaceFlinger 下的 FrameMissed 行。FrameMissed 是一项可提升用户体验的改进，由 HWC2 提供。当您查看其他设备的 systrace 时，如果设备未使用 HWC2，您可能不会看到 FrameMissed 行。在任一情况下，FrameMissed 都会与具有以下特点的 SurfaceFlinger 相关联：缺少一个很常用的运行时，以及在 vsync 时应用 (`com.prefabulated.touchlatency`) 的待处理缓冲区计数未发生变化：

[![FrameMissed 与 SurfaceFlinger 相关联](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_fm_sf.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_fm_sf.png?hl=cs "点击即可放大")

**图 11.** FrameMissed 与 SurfaceFlinger 相关联

图 11 显示了 15598.29 毫秒处的已丢失帧。SurfaceFlinger 在 vsync 间隔时间被短暂唤醒，并在未执行任何任务的情况下返回休眠状态，这意味着 SurfaceFlinger 确定不值得再次向显示部分发送帧。为什么？

为了了解在渲染此帧时管道是如何出现故障的，请先回顾上面的正常帧示例，了解正常界面管道在 systrace 中的效果如何。准备就绪后，回到丢失的帧并进行反推。请注意，SurfaceFlinger 被唤醒后立即进入休眠状态。查看来自 TouchLatency 的待处理帧的数量时，可以看到有两个帧（这是一条很好的线索，可帮助弄清楚发生的实际情况）。

[![SurfaceFlinger 被唤醒并立即进入休眠状态](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_sf_wake_sleep.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_sf_wake_sleep.png?hl=cs "点击即可放大")

**图 12.** SurfaceFlinger 被唤醒并立即进入休眠状态

因为我们在 SurfaceFlinger 中有一些帧，所以这不是一个应用问题。此外，SurfaceFlinger 在正确的时间被唤醒，所以这也不是一个 SurfaceFlinger 问题。如果 SurfaceFlinger 和应用看起来都正常，那么这可能是一个驱动程序问题。

因为启用了 `mdss` 和 `sync` 跟踪点，所以我们可以获得有关相应栅栏（在显示驱动程序和 SurfaceFlinger 之间共享，控制帧实际提交到显示部分的时间）的信息。这些栅栏列于 `mdss_fb0_retire` 下，它会指示帧实际在显示部分出现的时间。这些栅栏作为 `sync` 跟踪类别的一部分提供。哪些栅栏与 SurfaceFlinger 中的特定事件相对应，取决于您的 SOC 和驱动程序堆栈，因此请与您的 SOC 供应商联系，以了解您的跟踪记录中栅栏类别的含义。

[![mdss_fb0_retire 栅栏](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_fences.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_fences.png?hl=cs "点击即可放大")

**图 13.** mdss\_fb0\_retire 栅栏

图 13 中的帧显示了 33 毫秒，而不是预期的 16.7 毫秒。图块前进到中途时，该帧应该被新帧替换，但实际上没有。查看上一帧，看看是否可以找到任何内容：

[![被损坏帧的上一帧](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_frame_previous.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_frame_previous.png?hl=cs "点击即可放大")

**图 14.** 被损坏帧的上一帧

图 14 显示了时长为 14.482 毫秒的帧。被损坏的两帧区段是 33.6 毫秒，这和我们预期的两帧时长大致接近（我们以 60Hz 渲染帧，每帧 16.7 毫秒，很接近）。但是 14.482 毫秒与 16.7 毫秒相去甚远，这表明显示管道存在严重错误。

调查栅栏的确切结束位置，以确定是什么在控制它。

[![调查栅栏的结束位置](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_fence_end.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_fence_end.png?hl=cs "点击即可放大")

**图 15.** 调查栅栏的结束位置

工作队列包含一个 `__vsync_retire_work_handler`，它在栅栏发生变化时运行。通过浏览内核源代码，您可以看到它是显示驱动程序的一部分。它看起来位于显示管道的关键路径上，所以必须以尽可能快的速度运行。它可运行约 70 微秒（不是很长的调度延迟），但它是一个工作队列，系统可能无法准确地排定其运行时间。

检查上一帧，以确定它是否会导致该问题；有时抖动会随时间而累加，最终导致我们错过最后期限。

[![上一帧](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_previous_frame.png?hl=cs)](https://source.android.google.cn/static/docs/core/tests/debug/images/perf_trace_previous_frame.png?hl=cs "点击即可放大")

**图 16.** 上一帧

kworker 上的可运行行不可见，因为当该行被选择时，查看器会将它变为白色，不过，通过统计数据，我们可以了解到问题所在：部分显示管道关键路径存在 2.3 毫秒的调度程序延迟，这**很糟糕**。在继续操作之前，请先解决该延迟问题，方法是：将显示管道关键路径的这一部分从工作队列（作为 `SCHED_OTHER` CFS 线程运行）移动到专用的 `SCHED_FIFO` kthread。这一功能需要时间保证，而工作队列不能（也不打算）提供这一保证。

这是产生卡顿的原因吗？很难说这就是最终结论。在不易于诊断的情况下（如内核锁争用导致显示关键线程进入休眠状态），您通常无法通过跟踪记录了解问题的起因。这种抖动有可能是丢帧的原因吗？当然可以。栅栏时间应该是 16.7 毫秒，但是在丢帧前的一系列帧中，栅栏时间都与该值相去甚远鉴于显示管道的耦合紧密程度，栅栏时间的上下抖动有可能导致丢帧。

在此示例中，解决方案涉及将 `__vsync_retire_work_handler` 从工作队列转换为专用 kthread。这样一来，弹力球测试中的抖动情况得到显著改善，且卡顿问题也会明显减轻。随后的跟踪记录显示了非常接近 16.7 毫秒的栅栏时间。