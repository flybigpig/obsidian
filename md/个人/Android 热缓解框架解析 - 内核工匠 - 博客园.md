---
created: 2025-04-15T13:10:45 (UTC +08:00)
tags: []
source: https://www.cnblogs.com/Linux-tech/p/13873891.html
author: 内核工匠
---

# Android 热缓解框架解析 - 内核工匠 - 博客园

> ## Excerpt
> 随着手机功能的不断丰富，算法复杂性、系统核心频率和集成水平不断提高，而设备的形制和尺寸不断缩小，手机热缓解的重要性日益凸显。 为了在手机开始过热时进行有效的热缓解，Android 引入了热系统，用于将热子系统硬件设备的接口抽象化,硬件接口包括设备表面、电...

---
随着手机功能的不断丰富，算法复杂性、系统核心频率和集成水平不断提高，而设备的形制和尺寸不断缩小，手机热缓解的重要性日益凸显。

为了在手机开始过热时进行有效的热缓解，Android 引入了热系统，用于将热子系统硬件设备的接口抽象化,硬件接口包括设备表面、电池、GPU、CPU 和 USB 端口的温度传感器和热敏电阻。借助该框架，设备制造商和应用开发者可以主动获取这些系统硬件设备的温度数据,或者通过注册的回调函数（位于 PowerManager 类中）接收高温通知，进而在设备开始过热时调整系统及应用执行策略，降低系统负载。例如，当系统温度较高时，jobscheduler 作业会受到限制等，从而可以有效缓解手机发热问题。

**一、 Android热服务上层可调用接口**

Android 10 中的热服务利用 Thermal HAL 2.0 的监控底层设备各个节点温度等信息，系统内部组件和三方应用可通过调用相关接口获取这些反馈信息。

**1\. 三方应用可调用接口**

  
对于上层Android应用来说，可通过调用 PowerManager 类来添加监听器回调或主动获取获取热状态等信息，方法如下：

-   getCurrentThermalStatus()；//以整数形式返回设备的当前热状态等级。
    
-   addThermalStatusListener()；//添加监听器。
    
-   removeThermalStatusListener()；//移除之前添加的监听器。
    

以上方法获取的热状态等级为当前热状态码值。在ThermalHal 2.0中，热状态码分为如下几个级别：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSlFndjZFTzJzM2ZpYXZSTTJQTU9pYnpORVFRT25FWEdZSDlvWEV6ZFVRRGlhNDFkS1ZBRXlhSEFRUS82NDA?x-oss-process=image/format,png)

因此应用层调用以上接口方法，假如收到返回值为0x2，说明当前手机热等级为MODERATE (0x2) 中等限制，应用可以根据对应的热等级建议，调整自身逻辑，降低系统负载，缓解手机发热，进而增强在高温状态下的用户体验。

**2\. Android系统应用可调用接口**

  
对于Android系统内部组件来说，还可以注册IThermalEventListener回调接口等方法进行监控访问更加详细的热传感器和热事件信息；例如，Android系统内部组件可注册IThermalEventListener事件，监听手机表面温度变化，例如:

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSk83VUZvRUdwZ0VFUFVRbWxpYzdhejExRmlhRjcyaWJNbUoxMXE0cElycjRWTXRtcENKYU95YmliMHcvNjQw?x-oss-process=image/format,png)

当外壳温度超过系统底层设置的阈值档位时就会触发回调事件，进而弹出 Android 系统界面的警告消息提醒用户。此外，Android系统内部组件还可以通过调用HardwarePropertiesManager获取thermalHal的其他功能接口：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSng1YmFaejNTREV3Zm9JUlhJNjJoZDJVSUtYcmhWWnNySEFUd2hYTkFaOFhVZVVkeW1oOENxQS82NDA?x-oss-process=image/format,png)

**二、 Android热服务处理流程**

在 Android 10 中，框架中的热服务利用来自 Thermal HAL 2.0 监控各个设备节点温度信息，并向上层提供有关限制严重级别的反馈及当前温度等信息，下图为 Android 10 中热缓解处理流程的模型。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSlNGakM2NGo4UjNhM2JFVlBmQVJVcEQ4WWlhT0dqS01JT2s0cUJLaHFzdjZPamRhOWNxYkRaREEvNjQw?x-oss-process=image/format,png)

下面以其中一个接口方法（上层应用注册监听系统温度触发等级）的调用流程为例，探究整个调用流程的实现机制:

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSlRiYVNXWDVGekhLMzlrQnN1RlJjOExsbGJvZU02Q2FXQ2tNSFNpYkZDYmRlUWlibEV6dm9kTVBBLzY0MA?x-oss-process=image/format,png)

在PowerManager.java 中该方法内部实现如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSkE5MkJ3NXdvVzZwdzMwU1RwWHp6dmtJaENpYXpNYzVjdDd0NEdjaHZHZmxTUGE3UWg2bUFuSUEvNjQw?x-oss-process=image/format,png)

以上可看出会调用到ThermalService进行注册监听，接下来在 ThermalManagerService 中看下 registerThermalStatusListener 的实现过程：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSnd2R25qaWNuUTdVSE1KRkFEa1JmYnJBVmxBT0lpY2Q4b2VnekpNTkVydWJvOWx6YW11Mnlld0JRLzY0MA?x-oss-process=image/format,png)

从上可以看到上层的注册监听者会被存放在在mThermalStatusListeners中，它是一个RemoteCallbackList类:

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSkNRNVA2T0VvUHM2djVtdjRTTnN4d0NVWXNmc2hiMjd6Y29RcmZwdjFkV1ZkNk1YbkhvbEVHQS82NDA?x-oss-process=image/format,png)

如何触发并调用到存储在mThermalStatusListeners这些注册过来的binder监听者呢，继续看 ThermalManagerService 中的以下调用链：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSm9wMlRybmlhUWR2NlpzT3dRMzRjQ2hyV21wYzlXOUYzQkJwMU42WDNHY2JQbGZEWVM3Z2tLTFEvNjQw?x-oss-process=image/format,png)

从以上调用关系看，onTemperatureChangedCallback 函数最终会调用到 mThermalStatusListeners 触发上层应用注册的监听者回调；那么谁会又调用到onTemperatureChangedCallback函数呢？在ThermalManagerService 的初始化启动过程中会执行如下代码：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSjg3T1BSRkJEaGEyd2JOeWhzcGI3Z3ZxaWFpYTBGR1dyMWFxN29ETmJoNkZnM2lhOWpzYjlQVDdmdy82NDA?x-oss-process=image/format,png)

以上可知，ThermalHal20Wrapper 在thermalService服务初始化完成后会设置onTemperatureChangedCallback 回调函数，ThermalHal20Wrapper类实现如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSnFpY1pUbkJLYTRuTjFya1VHT2hRcDhnSWljdnlNaDdibm45aWFhWU5EN044UFNoMWpGQ01aZEFpYkEvNjQw?x-oss-process=image/format,png)

以上可看出 ThermalHal20Wrapper 会获取Thermal HAL 2.0 的 service服务，并注册调用其接口 registerThermalChangedCallback监听温度等级变化，当温度等级触发时会回调到 mThermalCallback20 的实现，进而经过以上层层调用，最终上层应用注册的监听者可以获取到触发返回的状态值信息。

此外需要注意，以上ThermalService中监听获取hal层返回的温度变化过程中，会检测当前温度是否超过关机阈值，如果超过就会触发手机直接关机：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSmlheDhScHNYWmhLWGEyWFppYkltellDUmI4ZEhrZlJiZVNyd2ljcXJ4RXY4OGxJVVNDbnRjT25Rdy82NDA?x-oss-process=image/format,png)

**三、 Thermal HAL 2.0 实现机制**

对于 Thermal HAL 2.0，它是从 Android 10 开始引入的，设备制造商必须实现 Thermal HAL 2.0 的 HIDL 中的方法（如 IThermal.hal 中所提供）。它获取设备温度传感器和限制状态，温度状态变化超过设定阈值时，将这些信息通过IThermalChangedCallback传递给thermalservice，触发以上调用流程，进而上层应用监听者能够获取这些状态信息，从而做出相应决策。

**1.Thermal HAL 2.0 和 1.0的区别**

  
以下是Thermal HAL  1.0的接口功能：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSlVlSG5kMjZBRGVSMEhNZXZNZjZjY3JLaWM0anI5ZnJ2YnBOOEJVZUU3SWliT28wU3ZXYjdkNVZBLzY0MA?x-oss-process=image/format,png)

Thermal HAL 2.0 是在继承1.0的基础上增加了监听设备节点温度回调监听接口，以下是thermal HAL 2.0的 IThermal.hal 部分声明：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSlMxTnlzd1FDTVp2cFNMYnVLbkdiZlV0V3pBOWMxd0lpY0dlWTF6SmliM3M3T2pmZVk1TGljaWNvMVEvNjQw?x-oss-process=image/format,png)

**2.Thermal HAL 2.0 实现案例**

下面以Google pixel项目为例分析其thermal HAL 2.0的实现机制。其温度注册监听回调接口实现如下，它会将注册监听者缓存在 callbacks\_ 容器中：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSklGTGVPV2VFUXIwd2IwZFBreHVwVjE3c3FSbGprVjVqaWN1UjZ4c29tQ3FoT1Q5bVlKSnVrV3cvNjQw?x-oss-process=image/format,png)

在pixel thermal HAL 2.0 服务刚初始化时，会同时开启一个线程持续地监听底层硬件设备的温度节点变化，当温度超过配置文件的设定阈值时，就会调用如下函数触发回调：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSk91RDk2VDJsanBDU1VCYThkUVV6MW9pYXFGVDFLT0V2aWFVTFgzTnd5NlpCWVliRElLdFRWWFJ3LzY0MA?x-oss-process=image/format,png)

如下是thermalhelper部分实现：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSkhMMHllaG9pY0ZIT2thZWljZ0JBTno5WDZLa0toSDdXbEJsU3pCaWNicGxNaWNKNEp1R1l2cnBxMncvNjQw?x-oss-process=image/format,png)

在 ThermalWatcher 中，它继承了Looper类，循环监听各个系统设备节点的温度变化

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSlp0T3Z5aWN6c0kxS1VkN0ZkdjhwOVM5bTZZQUpGMm8ycUF3ZXdjcWxKTDl3UEpWWWdsN2liR1RBLzY0MA?x-oss-process=image/format,png)

当ThermalWatcher监控到设备节点温度发生变化后，会回调到如下方法：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSnpUeHV6bzlPSXdBbTNEcHZTWjBGOGhZaDVlVnZMdnRWdnBZSlhrREZNVDRudXlkOUIxNko2US82NDA?x-oss-process=image/format,png)

按照如上回调函数流程，会调用到 Thermal.cpp 中的 sendThermalChangedCallback 函数：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSk42OWp6RHhFTnNPWDZxWUE3TUdkRHFlMDVGWnNvZ0hwMWIzSmVqbVMwZkRTaWFHN282SHNqbVEvNjQw?x-oss-process=image/format,png)

进而触发callbacks\_中保存的上层回调监听，通过hidl 接口回调到 thermalservice 中的 ThermalHal20Wrapper 的回调接口，进而将消息传递给上层的应用监听者，实现android系统设备节点温度监听。

如上分析，Google pixel中 thermal hal 2.0 监听的设备节点、温度触发等级阈值等信息均是通过系统内置配置文件获取的，其配置文件存放在：/vendor/etc/thermal\_info\_config.json 中， 部分配置截取说明如下：

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9kNGhvWUpseE9qTmZ4QjM5aDdhWlZWM3g0SnlDRjVUSmlhc3FlTmFSWjFXRDRma2x3TjA2TTliV200YUxidnRKVVdETW05U1lzQWw1c3hraEZQNmRFSHcvNjQw?x-oss-process=image/format,png)

通过修改配置文件，便可以调整读取或监控设备温度节点，设备节点温度触发等级等信息；

Android 热缓解框架为上层提供了主动获取或注册监听硬件设备温度的接口方法，而对于硬件设备温度过高时从系统层面如何进行热缓解，系统中有一个 thermal-engine 进程服务，它是Android系统的温度控制核心，该进程会监控系统中各个硬件节点温度，同时制定了一系列的配置策略（通过命令可导出配置策略：adb shell thermal-engine –o > thermal-engine.conf），根据这些策略及各个部件的当前温度，来控制CPU/gpu频率的升降、电池充电速度、网络信号强度、系统刷新帧率等，通过这些措施来缓解温升。上层可通过调用上述getCurrentCoolingDevices()接口获取当前被限制的部件具体信息。

**四、总结**

通过Android 10热服务框架以及thermal hal 2.0版本的引入，上层应用可以主动获取这些系统硬件设备的温度数据,或者通过注册的回调函数（位于 PowerManager 类中）接收高温通知，进而在设备开始过热时调整系统及应用执行策略。例如，当系统温度较高时，jobscheduler 作业会受到限制，降低系统负载，从而可以有效缓解手机发热问题。

![](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X2dpZi9kNGhvWUpseE9qTlNyNE9XSjlrdWtpYkZuc3h2U3pkSWlicjJqRDVZVDNqQU1mOWVrZDJDc0I3ME9HallxbjBKdFB3QjFtSXkxWlduQ216R1JMSzJFaWN5dy82NDA?x-oss-process=image/format,png)

**扫码关注**  
**“内核工匠”微信公众号**  
Linux 内核黑科技 | 技术文章 | 精选教程
