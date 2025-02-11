本文详解了 DiffUtils、Myers 算法和 Jetpack Compose。DiffUtils 是 Android 中优化列表更新的工具，依赖 Myers 算法计算最小更改。文中介绍了其工作原理、应用示例和在 RecyclerView 中的作用。Jetpack Compose 基于声明性 UI 原则，由状态驱动 UI 自动重组，内置优化且智能跳过未变元素重组，无需 DiffUtils，可通过带 Keys 的 LazyColumn 等工具实现类似优化，还列举了相关面试问题和结论。

关联问题: Compose 如何管理状态 DiffUtils 适用场景有哪些 Myers 算法复杂度怎样

> 本文译自[《DiffUtils, Myers’ Algorithm and Jetpack Compose》](https://link.juejin.cn/?target=https%3A%2F%2Fproandroiddev.com%2Fdiffutils-myers-algorithm-and-jetpack-compose-028c726e574d "https://proandroiddev.com/diffutils-myers-algorithm-and-jetpack-compose-028c726e574d")，原文发布于2024年12月27日。

在 Android UI 开发领域，DiffUtils 是优化 RecyclerView 中列表更新的必备工具。DiffUtils 是一个实用程序类，它计算将一个列表转换为另一个列表所需的**最小更改**，并仅更新 UI 中已更改的部分，从而**节省性能并减少不必要的重绘**。这个强大的工具依赖于**Myers算法**，这是一种在两个序列之间找到\*\*最短编辑脚本（Shortest edit script）\*\*的有效方法。

![banner](https://p9-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/23fcdf8b44144548b2d8c8253ea7c9ef~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg56iA5pyJ54y_6K-J:q75.awebp?rk3s=f64ab15b&x-expires=1739266107&x-signature=tqfoc6oJ2iB1Y0Um4%2FAa7TCHsRw%3D)

但在 Jetpack Compose 中，是没有DiffUtils的，这又是为什么呢？

在这篇博客中，我将详细分析 DiffUtils 的工作原理、它与 Myers 算法的联系、为什么它是现代 Android 开发的重要组成部分，并探讨为什么 DiffUtil 在 Compose 中是不必要的、Compose 如何优化 UI 更新，以及你应该使用什么来代替。

让我们开始吧！

## DiffUtils 是什么？

DiffUtils 是 Android 中的一个实用程序，它可以比较两个列表并生成一系列更新操作，例如：

-   插入：添加新项目。
-   删除：移除过时的项目。
-   移动：重新排序现有项目。

然后可以应用这些操作来有效地更新列表，最大限度地减少不必要的重绘或重新计算。这在 RecyclerView 等性能至关重要的组件中特别有用。

### DiffUtils 的工作原理

> DiffUtil 使用 Eugene W. Myers 的差异算法来计算将一个列表转换为另一个列表所需的最少更新次数。Myers 的算法不处理移动的项目，因此 DiffUtil 对结果进行第二次遍历以检测移动的项目。-- [developer.android.com/reference/a…](https://link.juejin.cn/?target=https%3A%2F%2Fdeveloper.android.com%2Freference%2Fandroidx%2Frecyclerview%2Fwidget%2FDiffUtil "https://developer.android.com/reference/androidx/recyclerview/widget/DiffUtil")

Android 中的 DiffUtils 基于 Myers 算法，用于比较列表并找出它们之间的差异。Myers 算法和 DiffUtils 的目标是相同的：确定将一个序列（列表）转换为另一个序列（列表）所需的最小更改次数，包括插入、删除和移动。从本质上讲，DiffUtils 通过识别以下内容来计算两个列表之间的差异：

1.  最长公共子序列 (LCS)：新旧列表之间保持不变的元素。
2.  编辑操作：将旧列表转换为新列表所需的插入、删除和移动。

**关键见解：** DiffUtils使用 LCS 的概念来最小化更改（插入和删除），并且还针对移动进行了优化——这是基于列表的比较所特有的功能，其中元素不仅被删除和插入，而且还被重新定位。

### Myers 算法：DiffUtils 的基础

Myers 算法于 1986 年推出，旨在计算两个序列之间的**最短编辑脚本 (SES，Shortest Edit Script)**。它找到将一个序列转换为另一个序列所需的最少操作数。这些操作包括：

-   插入
-   删除
-   匹配（按顺序排列的公共元素）

#### Myers 算法的关键概念

Myers 算法旨在找到将一个序列（例如，旧列表）转换为另一个序列（新列表）所需的最少**插入、删除和移动**次数。该算法基于**编辑距离（Edit Distance）** 的概念，并专门计算将一个序列转换为另一个序列的一系列操作。

Myers 算法在计算**最短的编辑操作序列**方面特别有效，从而最小化所需的总更改次数。核心思想是找到两个序列之间的**最长公共子序列 (LCS)**，然后确定将旧序列转换为新序列的最小操作。

最长公共子序列 (LCS)：Myers 算法首先确定两个列表之间的 LCS。LCS 表示不需要修改的元素。

**编辑图：**

-   该算法将转换可视化为一个图，其中每条路径 代表一系列操作（插入、删除或匹配）。
-   通过该图的最短路径对应于最短编辑脚本 （SES）。

**优化：**

-   Myers 算法使用动态规划来减少计算开销，实现高效的 O(ND) 时间复杂度，其中 N 和 D 是序列的长度和它们之间的距离。

### DiffUtils 实际应用示例

让我们考虑两个列表：

```
// Old list
["a", "b", "c", "d"]
// New List
["a", "d", "c", "b"]
```

1.  **确定 LCS**：此处的 LCS 为 \["a"\] 。
2.  **计算编辑脚本**：

-   删除“b”（旧列表）。
-   将“d”移到“c”之前。
-   在“c”之后插入“b”。

3.  **应用更改**：使用这些最少的操作将旧列表转换为新列表。

### DiffUtils 在 RecyclerView 中的作用

在 RecyclerView 中，每次更新都涉及计算哪些项目发生了变化、哪些项目被添加以及哪些项目被删除。单纯地更新整个列表可能会导致卡顿或无响应等性能问题。DiffUtils 通过以下方式解决此问题：

-   最小化更改：仅执行必要的更新。
-   优化性能：实现流畅的动画和高效的列表更新。
-   减少重绘：仅重新渲染受影响的项目，从而提高整体 UI 响应能力。

```
// 第1步: 创建一个DiffUtil.Callback
public class MyDiffCallback extends DiffUtil.Callback {
    private final List<String> oldList;
    private final List<String> newList;

    public MyDiffCallback(List<String> oldList, List<String> newList) {
        this.oldList = oldList;
        this.newList = newList;
    }

    @Override
    public int getOldListSize() {
        return oldList.size();
    }

    @Override
    public int getNewListSize() {
        return newList.size();
    }

    @Override
    public boolean areItemsTheSame(int oldItemPosition, int newItemPosition) {
        return oldList.get(oldItemPosition).equals(newList.get(newItemPosition));
    }

    @Override
    public boolean areContentsTheSame(int oldItemPosition, int newItemPosition) {
        return oldList.get(oldItemPosition).equals(newList.get(newItemPosition));
    }
}

// 第2步: 计算差异
DiffUtil.DiffResult diffResult = DiffUtil.calculateDiff(
    new MyDiffCallback(oldList, newList)
);

// 第3步: 派发更新
myAdapter.submitList(newList);
diffResult.dispatchUpdatesTo(myAdapter);
```

DiffUtils 是一款功能强大的工具，用于处理 Android 中的列表更新，确保高效且最小的更改。通过利用 Myers 算法，它可以计算出将一个列表转换为另一个列表的最短编辑脚本。了解其工作原理不仅可以提高你对 Android 开发的掌握，还可以帮助你优化 RecyclerView 的性能。

## 为什么 Compose 不需要 DiffUtils

Jetpack Compose 建立在**声明性 UI 原则**之上，这意味着你可以根据当前状态**描述 UI 应该是**什么样子，而 Compose 会处理其余的事情。

**命令式 UI（视图）：**

-   通过确定需要应用哪些更改，你可以手动更新 UI 组件。
-   DiffUtil 等工具对于计算列表的最小更新以控制性能必不可少。

**声明式 UI（Compose）：**

-   你描述给定状态下的 UI 应该是什么样子，而不是如何更改它。
-   Compose 会自动重新组合受状态更改影响的 UI 部分。

在 Compose 中，**状态驱动 UI，重组处理更新**。DiffUtils 无需计算增量，因为系统会自动优化要重新渲染的内容。

以下是 Compose 不再需要 DiffUtils 的主要原因：

1.  **状态驱动的UI**

在 Compose 中，当状态发生变化时，UI 会自动重组。你无需手动计算列表之间的差异；Compose 会为你处理。

```
val items = remember { mutableStateListOf("Apple", "Banana", "Cherry") }

LazyColumn {
    items(items) { item ->
        Text(text = item)
    }
}
```

如果你从 items 中添加或删除项目，Compose 将仅重组 UI 中受影响的部分。无需 DiffUtils！

2.  **内置优化**

Compose 使用 LazyColumn 和 LazyRow 中的键来优化项目渲染。通过为每个项目指定一个唯一键，Compose 可以识别哪些项目已更改、已添加或已移除。

```
LazyColumn {
    items(items = yourList, key = { item -> item.id }) { item ->
        Text(text = item.name)
    }
}
```

该key确保 Compose 有效地仅更新受影响的项目，类似于 DiffUtils 所做的。

3.  **智能重组**

Compose 可以智能地跳过未发生改变的 UI 元素的重组。使用 Remember 和 RememberSaveable 等工具，你可以进一步优化重组行为。

```
@Composable
fun RememberExample() {
    val count = remember { mutableStateOf(0) }

    Button(onClick = { count.value++ }) {
        Text("Clicked ${count.value} times")
    }
}
// 在这里，当状态发生变化时，只有Button中的Text，而不是整个组件，会发生重组
```

**Compose 中的重组：**

-   Compose 会观察状态变化。当特定 UI 元素的状态发生变化时，只会重组该元素（及其依赖项）。
-   系统会完全跳过未更改的 UI 元素。

**Views 中的 DiffUtils：**

-   需要明确计算列表的新旧状态之间的变化。
-   然后分派计算出的更改以更新 RecyclerView。

### 在 Compose 中用什么来代替 DiffUtils

虽然你不需要 DiffUtils ，但 Compose 提供了实现类似优化的工具：

1.  带 Keys 的 LazyColumn

使用 key 可以有效地识别和管理列表中的更改。

```
LazyColumn {
    items(items = yourList, key = { item -> item.id }) { item ->
        Text(text = item.name)
    }
}
```

2.  SnapshotStateList

若要以被动方式管理列表，请使用 SnapshotStateList 。

```
val items = remember { mutableStateListOf("Apple", "Banana", "Cherry") }
Button(onClick = { items.add("Date") }) {
    Text("Add Item")
}
LazyColumn {
    items(items) { item ->
        Text(text = item)
    }
}
```

3.  SubcomposeLayout

对于复杂的场景，SubcomposeLayout 可以精确控制要重组的内容。

## 相关的面试问题

1.  为什么 Jetpack Compose 不需要 DiffUtils ？

Compose 依赖于声明性 UI 模型。它会根据状态变化自动更新 UI，无需像 DiffUtil 那样手动计算列表差异。使用 LazyColumn 中的键可确保高效更新，而无需使用外部工具。

2.  Jetpack Compose 处理列表更新的方式与 RecyclerView 有何不同？

Compose 不依赖手动差异计算 (DiffUtils)，而是观察状态变化并仅重组受影响的组件。这是通过使用键和 Compose 的重组逻辑在内部管理的。

3.  在 Android 开发中，声明式 UI 与命令式 UI 相比有哪些优势？

-   代码更简单：声明式 UI 通过关注内容而不是方式来减少样板代码。
-   自动状态管理：Compose 根据状态变化自动更新 UI。
-   可测试性提高：无状态可组合项可以独立测试。
-   一致性：重组确保 UI 始终反映当前状态。

4.  Compose 中的重组是什么？它与 RecyclerView 中的传统视图失效有何不同？

-   重组：当 Compose 检测到状态变化时发生。它仅重新生成受变化影响的 UI 部分。
-   视图失效：在 RecyclerView 中，失效会触发视图重绘，如果没有 DiffUtil 之类的工具，这可能会很低效。

5.  何时应在 Compose 中使用 Remember 和 RememberSaveable？

-   使用 Remember 在单个组合生命周期内存储状态。
-   使用 RememberSaveable 在配置更改（如屏幕旋转）期间保留状态。

6.  Compose 如何决定要重组 UI 的哪些部分？

Compose 会跟踪每个可组合项中的状态读取。当状态发生变化时，只有读取已更改状态的可组合项才会被重组。

7.  在 Compose 中实现 LazyColumn 以显示项目列表并添加按钮来更新列表。确保它能够高效更新。

```
@Composable
fun LazyColumnExample() {
    val items = remember { mutableStateListOf("Apple", "Banana", "Cherry") }

    Column {
        Button(onClick = { items.add("Date") }) {
            Text("Add Item")
        }

        LazyColumn {
            items(items, key = { it }) { item ->
                Text(text = item)
            }
        }
    }
}
```

8.  识别并修复 Compose 组件中不必要的重组。

```
LazyColumn {
    items(items = list, key = { item -> item.id }) { item ->
        Text(text = item.name)
    }
}
```

9.  诊断 LazyColumn 中的滞后：

-   检查关键参数：确保每个项目都有唯一的键。
-   使用分析工具：使用 Android Studio 的Compose 调试器分析重组计数。
-   优化项目渲染：避免在 LazyColumn 中使用的可组合函数中进行大量计算。

10.  调试列表更新中的不一致行为：

确保数据源稳定并符合 UI 预期。使用SnapshotStateList 可以帮助保持反应性。

11.  SnapshotStateList 和 ArrayList 之间的区别：

SnapshotStateList 是被动的；更改会自动触发 Compose 中的重组。ArrayList 不是被动的，需要手动通知 UI 更新。

12.  使用 SubcomposeLayout

SubcomposeLayout 是 Jetpack Compose 中一个强大的布局工具，可让你按需组合布局的各个部分。这对于 UI 的某些部分占用大量资源或可能无法立即使用的情况尤其有用，例如从网络或数据库加载图像。SubcomposeLayout 允许按需组合布局的各个部分。

示例：动态加载图像并显示占位符，直到准备好为止。

```
@Composable
fun ImageWithPlaceholder(imageUrl: String, placeholder: Painter) {
    Box(modifier = Modifier.fillMaxSize()) {
        SubcomposeLayout { constraints ->
            // First, compose the placeholder
            val placeholderLayout = subcompose(0) {
                Image(painter = placeholder, contentDescription = null, modifier = Modifier.fillMaxSize())
            }

            // Compose the image once it's loaded
            val imageLayout = subcompose(1) {
                AsyncImage(
                    model = imageUrl,
                    contentDescription = null,
                    modifier = Modifier.fillMaxSize(),
                    contentScale = ContentScale.Crop
                )你
            }

            // Return the max size for the layout
            layout(constraints.maxWidth, constraints.maxHeight) {
                placeholderLayout[0].measure(constraints).placeRelative(0, 0)
                imageLayout[0].measure(constraints).placeRelative(0, 0)
            }
        }
    }
}

@Preview
@Composable
fun ImageWithPlaceholderPreview() {
    ImageWithPlaceholder(
        imageUrl = "https://www.example.com/image.jpg",
        placeholder = painterResource(id = R.drawable.placeholder_image)
    )
}
```

-   SubcomposeLayout：此布局允许你根据需要组合布局的各个部分。在这里，我们首先组合占位符，然后在图像准备好后组合图像。
-   subcompose()：此函数用于组合布局的各个部分。subcompose 函数返回 MeasureResult 对象列表，然后你可以测量这些对象并将其放置在屏幕上。
-   AsyncImage：我们使用 coil-compose 库中的 AsyncImage 异步加载图像。加载时，会显示占位符。
-   占位符：首先显示占位符。图像准备好后，它会接管。

此方法可帮助你通过减少不必要的重新组合和更优雅地处理图像或数据等动态内容来创建更高效的 UI。

13.  将旧版 RecyclerView 迁移到 Compose：

-   用 LazyColumn 替换 RecyclerView。
-   将适配器逻辑移至可组合函数。
-   使用 Remember 或 SnapshotStateList 进行状态管理。
-   使用 Keys 进行优化。

## 结论

-   Compose 不需要 DiffUtils，因为它建立在声明性和状态驱动的架构上。
-   带有 key 的 LazyColumn 和 SnapshotStateList 提供类似的优化。
-   智能重组可确保高效的 UI 更新，从而减少手动优化的需要。

通过拥抱 Compose 的声明性特性，你可以专注于构建美观、响应迅速的 UI，而无需担心列表更新的复杂性。

Happy Composing！

## 参考文献

1.  Myers, E. (1986)。O(ND) 差分算法及其变体。ACM编程语言和系统事务，1(2)，251–266。
2.  [Android 开发者文档 — DiffUtils](https://link.juejin.cn/?target=https%3A%2F%2Fdeveloper.android.com%2Freference%2Fandroidx%2Frecyclerview%2Fwidget%2FDiffUtil "https://developer.android.com/reference/androidx/recyclerview/widget/DiffUtil")

> 欢迎搜索并关注 _公众号[「稀有猿诉」](https://link.juejin.cn/?target=https%3A%2F%2Fmp.weixin.qq.com%2Fmp%2Fqrcode%3Fscene%3D10000004%26size%3D512%26__biz%3DMzU1MDg0NDczNA%3D%3D%26mid%3D2247484417%26idx%3D1%26sn%3D60c15f0d3c4be75e37748c2472a74102 "https://mp.weixin.qq.com/mp/qrcode?scene=10000004&size=512&__biz=MzU1MDg0NDczNA==&mid=2247484417&idx=1&sn=60c15f0d3c4be75e37748c2472a74102")_ 获取更多的优质文章！
> 
> 保护原创，请勿转载！