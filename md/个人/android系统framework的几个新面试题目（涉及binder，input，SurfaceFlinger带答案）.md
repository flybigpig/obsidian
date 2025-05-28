---
created: 2025-05-28T14:32:42 (UTC +08:00)
tags: []
source: https://mp.weixin.qq.com/s/5XtuKElaCcmIaj1Svolqbw
author: 千里马
---

# android系统framework的几个新面试题目（涉及binder，input，SurfaceFlinger带答案）

> ## Excerpt
> 今天给大家分享几个学员朋友面试过程中带回来的几个新面试题，这些面试题目属于比较独特一些，一些不属于1+1=2直接有标准答案，但是需要对模块熟悉后有一些自己的理解和思考答出的开放性题目，比如问题1和问题2就属于这种，所以这种题目可能面试官自己也没有明确的面试答案哈，我这里也整理了一些答案，有的也有ai一些整理归纳功劳哈。

---
## 背景：

今天给大家分享几个学员朋友面试过程中带回来的几个新面试题，这些面试题目属于比较独特一些，一些不属于1+1=2直接有标准答案，但是需要对模块熟悉后有一些自己的理解和思考答出的开放性题目，比如问题1和问题2就属于这种，所以这种题目可能面试官自己也没有明确的面试答案哈，我这里也整理了一些答案，有的也有ai一些整理归纳功劳哈。

![图片](https://mmbiz.qpic.cn/sz_mmbiz_png/DYicOkJDdA2otAG3MlmicQ3Oqent4X9qzge9xHNkFgWMUT6wiciaORHiaCALkVXZTs0O1NobsOXv2rPibcIDRf46oOKQ/640?wx_fmt=png&from=appmsg&wxfrom=5&wx_lazy=1&tp=webp)

## 问题1：

#### 系统中`audioflinger`和`surfaceflinger`作为独立的Native守护进程运行，而`inputflinger`与`system_server`进程绑定，为什么inputflinger不作为独立进程

主要基于以下设计考量：

1.  **事件分发的高实时性要求**  
    
    Input事件（如触摸、按键）的处理对延迟极其敏感，需要快速响应并分发到应用进程。若`inputflinger`独立为守护进程，需通过IPC（如Binder）与`system_server`通信，这会引入额外的进程间通信开销，可能影响事件分发的实时性。而当前集成在`system_server`中的设计，通过线程级交互（如`InputReaderThread`和`InputDispatcherThread`）直接处理事件，避免了跨进程延迟。
    
2.  **与窗口管理的紧密耦合**  
    
    Input事件的分发逻辑高度依赖窗口管理器（`WindowManagerService`，WMS）。例如，WMS需为应用进程提供`InputChannel`以接收事件，并处理焦点窗口切换、触摸事件拦截等逻辑。若`inputflinger`独立，需频繁与WMS跨进程同步状态，增加复杂性和性能损耗。集成在`system_server`中可减少这类协调成本。
    
3.  **系统服务启动与依赖管理**`   `
    
    `system_server`是Android的核心服务管理器，负责按顺序启动和依赖解析。`inputflinger`的初始化依赖于`system_server`提供的环境（如`DisplayThread`的Looper），且需在WMS启动后立即可用。独立化需额外设计进程生命周期管理，可能引入启动顺序或死锁风险。
    
4.  **资源与性能权衡**  
    
    独立进程虽能提升模块隔离性，但会占用额外内存和CPU资源。`inputflinger`作为高频调用的服务，独立后需维护常驻进程，可能得不偿失。而`audioflinger`和`surfaceflinger`因涉及硬件资源独占（如音频设备、显示合成），独立进程更利于资源调度。
    
5.  **历史架构演进**  
    
    早期Android版本中，`inputflinger`曾尝试过独立进程设计（如通过`main.cpp`启动），但最终因上述问题回归`system_server`集成。后续优化
    

## 问题2：

#### surfaceflinger为什么要设置binder线程为4个

SurfaceFlinger 自己在 main\_surfaceflinger.cpp 里主动把 Binder 线程池的上限从系统默认的 15 条降到了 4 条：

```
// 
// frameworks/native/services/surfaceflinger/main_surfaceflinger.cpp
int main(int argc, char** argv) {
    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->setThreadPoolMaxThreadCount(4);   // 关键语句
    …
}
```

之所以只开 4 条线程，而不是让 Binder 驱动自动根据负载无限制地拉起线程，主要是下面几方面的权衡。

并发度需求有限

 • SurfaceFlinger 对外只暴露 ISurfaceComposer、IDisplayEventConnection 等少量接口。

• 绝大部分事务最终都要在主线程（“VSYNC 线程”）里执行，因为它持有全局锁 mStateLock 并驱动合成管线。Binder 线程做的只是把 IPC 转成主线程消息，再多线程也无法突破这把大锁带来的串行瓶颈。

结论：多于 3-4 条线程并不能显著提升吞吐。

避免 CPU 抢占导致掉帧

• SurfaceFlinger 的主线程和合成线程都使用 SCHED\_FIFO/SCHED\_OTHER + 高优先级。 • 若 Binder 线程数量过多，它们同样是高优先级，容易在调度器里与合成关键路径「抢时间片」，造成 jank／帧间隔抖动。 • 控制线程数可以将 CPU 占用保持在一个可预测范围，降低实时渲染链路被意外打断的概率。

减少内存与上下文切换开销

• 每条 Binder 线程的用户栈默认 1 MiB，再加 Binder 驱动内核栈，线程越多越浪费。

• Binder 事务通常很短（几百微秒级），大量线程反而会导致锁竞争、cache miss 和上下文切换开销上升。

早期设备资源受限的历史包袱

• 最初做参数调优时，主流设备只有 1-2 GiB RAM、4-8 核 CPU，实验表明 4 条线程已经覆盖 99% 使用场景。这个值后来一直沿用。

Dos／恶意调用防护

• 将线程数限定得较小可防止外部 Service 使用海量并发事务拖垮 SurfaceFlinger，属于一种「背压」策略。

总结 SurfaceFlinger 作为图形栈核心进程，对实时性和确定性要求极高；Binder 请求本身对并发需求不大，却可能对调度造成负面影响。综合测试结果后，Google 把线程池上限固定为 4，正好在「满足事务峰值」与「最小化资源占用、保障帧率」之间取得平衡。

## 问题3：

#### 讲述一下binder机制中binder多线程的支持

1.  使用 Binder 的进程在启动之后，通过 BINDER\_SET\_MAX\_THREADS 告知驱动其支持的最大线程数量
    
2.  驱动会对线程进行管理。在 binder\_proc 结构中，这些字段记录了进程中线程的信息：max\_threads，requested\_threads，requested\_threads\_started
    
3.  binder\_thread 结构对应了 Binder 进程中的线程
    
4.  驱动通过 BR\_SPAWN\_LOOPER 命令告知进程需要创建一个新的线程
    
5.  进程通过 BC\_ENTER\_LOOPER 命令告知驱动其主线程已经ready
    
6.  进程通过 BC\_REGISTER\_LOOPER 命令告知驱动其子线程（非主线程）已经ready
    
7.  进程通过 BC\_EXIT\_LOOPER 命令告知驱动其线程将要退出
    
8.  在线程退出之后，通过 BINDER\_THREAD\_EXIT 告知Binder驱动。驱动将对应的 binder\_thread 对象销毁
    
    流程图如下：
    

![在这里插入图片描述](https://mmbiz.qpic.cn/sz_mmbiz_png/DYicOkJDdA2pgwsN22XmgbkDAotBtoDZfJltEyjy6alLBWgiatZnHa3icSCXno2ib48QTdYySTkbaLF9NibvvwNvHXg/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1)

其他framework实战技术干货相关手把手课程资料：

[](https://mp.weixin.qq.com/s?__biz=MzkzOTQ4NDUyNg==&mid=2247484186&idx=1&sn=328a6efaf16b78b1029b3595be03268b&scene=21#wechat_redirect)[Android Framework开发rom实战合集课表/车载车机手机高级系统开发工程必会技能](https://mp.weixin.qq.com/s?__biz=MzkzOTQ4NDUyNg==&mid=2247484186&idx=1&sn=328a6efaf16b78b1029b3595be03268b&scene=21#wechat_redirect)

![图片](data:image/svg+xml,%3C%3Fxml version='1.0' encoding='UTF-8'%3F%3E%3Csvg width='1px' height='1px' viewBox='0 0 1 1' version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'%3E%3Ctitle%3E%3C/title%3E%3Cg stroke='none' stroke-width='1' fill='none' fill-rule='evenodd' fill-opacity='0'%3E%3Cg transform='translate(-249.000000, -126.000000)' fill='%23FFFFFF'%3E%3Crect x='249' y='126' width='1' height='1'%3E%3C/rect%3E%3C/g%3E%3C/g%3E%3C/svg%3E)

具体优惠购买和成为vip学员加入vip群可以私聊马哥微信号：

androidframework007

![图片](data:image/svg+xml,%3C%3Fxml version='1.0' encoding='UTF-8'%3F%3E%3Csvg width='1px' height='1px' viewBox='0 0 1 1' version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'%3E%3Ctitle%3E%3C/title%3E%3Cg stroke='none' stroke-width='1' fill='none' fill-rule='evenodd' fill-opacity='0'%3E%3Cg transform='translate(-249.000000, -126.000000)' fill='%23FFFFFF'%3E%3Crect x='249' y='126' width='1' height='1'%3E%3C/rect%3E%3C/g%3E%3C/g%3E%3C/svg%3E)
