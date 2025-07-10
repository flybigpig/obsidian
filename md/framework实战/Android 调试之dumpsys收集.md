

本文介绍了Android调试中的一些实用技巧，包括使用dumpsys命令来获取当前获取焦点的控件、查看系统广播发送记录以及获取TV输入信源信息。通过\`dumpsys window | grep imfocus\`可以找到当前焦点控件，\`dumpsys | grep BroadcastRecord\`则用于查看广播记录，这些记录按时间顺序从新到旧排列。此外，\`dumpsys tv\_input\`则用于获取TV输入的相关信息，帮助开发者深入理解Android系统的运行状态。

摘要生成于 [C知道](https://ai.csdn.net/?utm_source=cknow_pc_ai_abstract) ，由 DeepSeek-R1 满血版支持， [前往体验 >](https://ai.csdn.net/?utm_source=cknow_pc_ai_abstract)

## Android 调试之dumpsys收集

+   找到当前获取焦点的控件

```shell
dumpsys window | grep -i mfocus
```

+   查看Android系统广播发送记录

```shell
dumpsys |grep BroadcastRecord
```

> 从上到下，是按照从新到旧的顺序排列的

+   获取TV输入信源信息

```shell
dumpsys tv_input
```