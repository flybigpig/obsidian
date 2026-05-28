你的 RecyclerView 为什么越滑越卡？聊聊四级缓存和那些年我踩过的坑

> 每个 Android 开发都被列表卡顿折磨过。今天我们从源码出发，把 RecyclerView 的缓存复用机制彻底聊透。

___

## 从一个线上 bug 说起

你有没有遇到过这种情况：列表滑动时越来越卡，打开 Profiler 一看，`onCreateViewHolder` 被疯狂调用，`onBindViewHolder` 反而没怎么走。

内存一路飙升，最后 OOM 崩溃。

我第一次碰到这个问题的时候也是一头雾水——明明 RecyclerView 的卖点就是"回收复用"，怎么一个都没复用上？后来花了不少时间翻源码，才把整个缓存机制搞明白。今天就把这些东西整理出来，希望能帮你少走点弯路。

___

## 先建个直觉：快递分拣中心

在翻源码之前，先用一个类比帮你建立直觉。

把 RecyclerView 想象成一个**快递分拣中心**。屏幕就是分拣台面，只能同时摆下几个包裹（可见的 ViewHolder）。当新包裹要上台面时，分拣员不会每次都找工厂订一个全新箱子——那太慢了。他会先看看旁边有没有同型号的旧箱子，撕掉旧标签、贴上新标签就能用。

这就是 RecyclerView 的核心思想：**复用 ViewHolder，避免重复 inflate**。

而它实现复用的方式，就是一套四级缓存系统。

___

## 四级缓存，到底是怎么回事

我把这四级缓存类比成电商仓库的四级调度，从近到远：

### 一级：Scrap——还在台面上，临时挪一下

当 RecyclerView 执行 `onLayoutChildren()` 重新布局时，会先把屏幕上所有的 ViewHolder **临时放进 scrap**，布局算完了再放回去。

```swift
final ArrayList<ViewHolder> mAttachedScrap = new ArrayList<>();
```

`mAttachedScrap` 放的是"数据没变的"，拿回来直接用，连 bind 都不需要。`mChangedScrap` 放的是"数据变了的"（比如你调了 `notifyItemChanged`），拿回来还得 rebind。

这级缓存的生命周期极短，只存在于 layout 过程中。你几乎感知不到它，但它是整个复用体系的基础。

### 二级：CachedViews——刚滑出去的，记得住

这个是跟用户体验直接相关的。当一个 item 刚被滑出屏幕，RecyclerView 不会立刻把它扔进大仓库，而是先放到一个**暂存架**上。

```swift
final ArrayList<ViewHolder> mCachedViews = new ArrayList<>();
```

重点来了：CachedViews 是**按 position 精确匹配**的。什么意思？就是你滑下去两行又滑回来，那两个刚消失的 item 能直接从暂存架拿回来，**连 onBind 都不用走**，因为数据还是对的。

默认只有 2 个位置。为什么这么少？因为大多数用户回弹的范围就 1-2 个 item。调大当然可以，但 CachedViews 里的 ViewHolder 是**带着完整数据的**，比 Pool 里的空壳重得多，吃内存。

满了之后怎么办？FIFO 淘汰——最早进来的被踢到 RecycledViewPool。

### 三级：ViewCacheExtension——几乎没人用

说实话，这一级我在实际项目中**从来没用过**。它是 Google 留给极端场景的扩展口，比如你有一个广告 Banner 永远不变，想"钉死"在缓存里不让回收。

但正常业务根本用不上。知道有这么个东西就行，跳过。

### 四级：RecycledViewPool——大仓库，按型号分区

这是最重要的一级。前面都没命中，就到这里来找了。

```perl
public static class RecycledViewPool {
```

Pool 的匹配逻辑和 CachedViews 完全不同：**它只看 viewType，不管 position**。所以从 Pool 里取出来的 ViewHolder，`resetInternal()` 清空状态后，必须走一遍 `onBindViewHolder` 重新绑数据。

每种 viewType 默认缓存 5 个。

这一级有个杀手锏功能：**可以跨 RecyclerView 共享**。后面讲实战的时候会细说。

___

## 关键流程图解

搞清楚四级缓存分别是什么之后，更重要的是理解它们**怎么协作**。下面几张图是整个机制的骨架。

### 图一：缓存查找全链路

每次 LayoutManager 需要一个 ViewHolder，都会走 `tryGetViewHolderForPositionByDeadline()` 这个方法。这张时序图展示了它的完整查找链路：

![图片](https://mmbiz.qpic.cn/mmbiz_png/Tf49FvRjxcmGzy9E5a891Tz1WVB3LTtjria63KZZpJP4BuGXibALzCstrtvTkG70W200XtzwnkQ9CU5rtRkchhacwnepqtkYytogI6cBvOHAE/640?wx_fmt=png&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=0)

核心结论：**越靠前命中，代价越低。** Scrap 和 CachedViews 命中连 bind 都不走；Pool 命中要走 bind 但省了 create；全部 miss 才走 create，这是最贵的路径。

### 图二：回收流程——ViewHolder 滑出屏幕后去了哪里

理解了查找，再看反方向。一个 ViewHolder 被滑出屏幕后的命运：

![图片](https://mmbiz.qpic.cn/mmbiz_png/Tf49FvRjxcnwZh95aB4bjAZRxcZwHkAM0edsNnpq1U393UvtXn3wdxeJ115xPK1QGZ3MLKDWdr64D8x52TFrCyrjTqRbIxKiblQP8AyzA0vk/640?wx_fmt=png&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=1)

**如果 Pool 也满了，ViewHolder 就被直接丢弃了。** 下次需要同类型的只能重新 create。所以如果你发现 create 被频繁调用，很可能是 Pool 容量不够，或者更常见的——viewType 太多导致 Pool 里根本存不到匹配的。

___

## 对应到源码里，就是这一个方法

上面时序图对应的就是 `tryGetViewHolderForPositionByDeadline`，来看核心逻辑：

```cpp
ViewHolder tryGetViewHolderForPositionByDeadline(int position, ...) {
```

重点看 Step 2 和 Step 5 的区别：一个按 position 找，一个按 viewType 找。这个区别直接决定了命中后要不要走 bind。

___

## 踩坑实录

搞清楚了原理，来聊聊我在大型项目里实际踩过的坑。

### 坑一：viewType 爆炸——每个 item 都在 create

这是最经典的一个。我们的列表有十几种卡片类型，有段时间新来的同学写了个 Adapter，`getItemViewType()` 直接返回了 position：

```cpp
// ❌ 灾难性写法
```

后果就是每个 position 都是一种独立的 viewType。Pool 里虽然存了之前回收的 ViewHolder，但 type 对不上，全部 miss。

**它根本不知道该复用哪个 ViewHolder。**

这就是 viewType 爆炸的本质。

修复很简单——按实际布局类型分类，确保 type 数量有限：

```kotlin
override fun getItemViewType(position: Int): Int {
```

我们后来用 SectionManager 统一管理所有区块的 viewType，十几种卡片类型也不会乱。新增一种卡片就加一个 Section，职责很清晰。

### 坑二：wrap\_content 陷阱——缓存被彻底废掉

这个坑更隐蔽。一个页面里 ScrollView 嵌套了 RecyclerView，高度设成了 `wrap_content`。结果所有 item 一次性全部 create 出来，缓存形同虚设。

为什么？来看 `LinearLayoutManager.fill()` 的核心循环：

```bash
while ((layoutState.mInfinite || remainingSpace > 0)
```

重点是 `mInfinite` 这个标志。当 ScrollView 测量子 View 时，给的 heightSpec 是 `MeasureSpec.UNSPECIFIED`——无限高度。LayoutManager 一看"空间无限大"，`mInfinite = true`，while 循环停不下来，把所有数据全部 layout 出来。

**一千条数据就 create 一千个 ViewHolder。**

用时序图看会更清楚——wrap\_content 为什么导致全量创建：

![图片](https://mmbiz.qpic.cn/mmbiz_png/Tf49FvRjxcm7Y2H4TfobPu86HjwYIMCOEX16odfep8jSHibh6lSoh9nyP0ibmw9Z1xCJLQwCCiaicGiaepBZTJaWyuxic8mlBaSzNZv0tve3sbCA8/640?wx_fmt=png&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=2)

解决方案不是换成 NestedScrollView（那也是 UNSPECIFIED，一样全量创建），而是从根本上去掉外层滚动容器。把 header、banner、footer 这些东西都做成 RecyclerView 的不同 viewType，用 `ConcatAdapter` 或者 SectionManager 组合起来，让 RecyclerView 自己管滚动。

### 坑三：跨列表共享 Pool——好用，但有个前提

我们的首页有多个横向滑动列表（分类 A 的卡片、分类 B 的卡片...），卡片布局完全一样。如果每个横向列表各维护自己的 Pool，用户从 A 列表切到 B 列表时要重新 create，浪费。

解法是创建一个共享的 RecycledViewPool：

```java
val sharedPool = RecyclerView.RecycledViewPool().apply {
```

用时序图看一下共享复用的实际过程：

![图片](https://mmbiz.qpic.cn/mmbiz_png/Tf49FvRjxcm0emUkrY0ib4iaEnSiaeXGQ7XOBXsRric8VfWLSNRdyM9o5y2x5IQyibTHLVjRKl9N17LOgS9Ke8KO4hOXol2VbGYHibRRDdJU3H3BE/640?wx_fmt=png&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=3)

效果很明显，ViewHolder 创建量减少了 30% 以上。

但这里有个坑要注意：**共享的前提是同 viewType 的 ViewHolder 布局结构完全一致。** 如果 A 列表的卡片有个下载按钮而 B 的没有，那就不能共享。还有，从 Pool 里拿出来的 ViewHolder 虽然做了 `resetInternal()`，但如果你的 `onBind` 没把所有 UI 状态（比如 visibility、checked）覆盖到，就会出现"状态串位"——上一个 item 的下载进度条跑到了下一个 item 上。

### DiffUtil 精准刷新——告别 notifyDataSetChanged

很多人习惯数据一变就 `notifyDataSetChanged()`。这相当于告诉 RecyclerView："所有数据都变了，你全部重来吧。" 结果就是所有可见 ViewHolder 全部 rebind，没有动画，用户看到的是整屏闪烁。

用 DiffUtil 就好多了。它基于 Myers 差分算法，帮你算出新旧列表的最小差异，只更新变化的部分：

```kotlin
class CardDiffCallback(
```

配合 payload 做局部刷新，连 ViewHolder 整体 rebind 都不需要，只更新变化的那个进度条：

```kotlin
override fun onBindViewHolder(holder: CardViewHolder, position: Int,
```

有个提醒：DiffUtil 的时间复杂度是 O(N+D²)，列表超过 1000 条的话，一定要用 `ListAdapter` 或 `AsyncListDiffer` 放到后台线程算，别卡主线程。

___

## 混合开发的一个细节：ComposeView 进 RecyclerView

如果你在往 Compose 迁移，多半会遇到 ComposeView 作为 RecyclerView item 的场景。这里有个容易忽略的内存泄漏：

```perl
class ComposeViewHolder(val composeView: ComposeView) : ViewHolder(composeView) {
```

不设置 `ViewCompositionStrategy`，ViewHolder 被回收时 Composition 不会释放，内存就一直涨。个人推荐用 `DisposeOnViewTreeLifecycleDestroyed`，跟着宿主 Fragment/Activity 的生命周期走，最省心。

___

## 综合下来的效果

在一个十几种卡片类型的大型列表页中，把上面这些优化全部落地后：

-   卡顿率下降 **40%**
    
-   帧率稳定在 **60fps**
    

数字不骗人。列表性能这件事，原理搞清楚了，优化手段就是水到渠成的。

___

## 留几个问题给你

1.  **CachedViews 默认 2 个，调大到 10 个会怎样？** 提示：想想内存占用和数据陈旧的风险。
    
2.  **如果让你设计一个跨页面共享的 RecycledViewPool，线程安全怎么处理？** Pool 的 `getRecycledView` 和 `putRecycledView` 是同步调用吗？
    
3.  **NestedScrollView 嵌套 RecyclerView，加上 `nestedScrollingEnabled = false`，解决了什么问题？没解决什么问题？** 提示：滚动冲突和缓存失效是两回事。
    
4.  **GapWorker 预取机制和你自己做的业务预加载有什么区别？** 提示：一个是框架层优化 ViewHolder 创建，一个是业务层优化数据准备。
    
5.  **RecyclerView 和 Compose 的 LazyColumn，缓存思路有什么本质区别？** 提示：一个复用 View 实例，一个复用 Composition 状态。
    

___

## 最后总结一下

说到底，RecyclerView 性能优化就是在跟它的缓存系统打交道。四级缓存的优先级搞清楚了，大多数卡顿问题都能快速定位。

几个核心要点：Scrap 和 CachedViews 命中不走 bind，Pool 命中要走 bind 但省了 create；viewType 数量一定要可控；永远不要用 ScrollView 包 RecyclerView；共享 Pool 能省大量 create 但要确保布局一致；DiffUtil 搭配 payload 做局部刷新是标配。

___

> 关于作者：资深 Android 工程师，专注大型 App 架构与性能优化。日常在大规模工程中摸爬滚打，喜欢把复杂的东西讲简单。 欢迎关注，一起聊点有深度的 Android 技术 + AI技术。