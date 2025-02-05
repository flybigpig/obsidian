## RecycleView

主要部件

 `ViewHolder`、`Adapter`、`AdapterDataObservable`、`RecyclerViewDataObserver`、`LayoutManager`、、`Recycler`、`RecyclerPool`

理解RecycleView实现原理

![](pic/2934684-1b8fadc84223ea0a.png)

## ViewHolder

对于`Adapter`来说，一个`ViewHolder`就对应一个`data`。它也是`Recycler缓存池`的基本单元。



```java
class ViewHolder {
    public final View itemView;
    int mPosition = NO_POSITION;
    int mItemViewType = INVALID_TYPE;
    int mFlags;
    ...
}
```

上面我列出了`ViewHolder`最重要的4个属性:

- itemView : 会被当做`child view`来`add`到`RecyclerView`中。
- mPosition : 标记当前的`ViewHolder`在`Adapter`中所处的位置。
- mItemViewType : 这个`ViewHolder`的`Type`，在`ViewHolder`保存到`RecyclerPool`时，主要靠这个类型来对`ViewHolder`做复用。
- mFlags : 标记`ViewHolder`的状态，比如 `FLAG_BOUND(显示在屏幕上)`、`FLAG_INVALID(无效，想要使用必须rebound)`、`FLAG_REMOVED(已被移除)`等。

## Adapter

它的工作是把`data`和`View`绑定，即上面说的一个`data`对应一个`ViewHolder`。主要负责`ViewHolder`的创建以及数据变化时通知`RecycledView`。比如下面这个Adapter:



```kotlin
class SimpleStringAdapter(val dataSource: List<String>, val context: Context) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        if (holder.itemView is ViewHolderRenderProtocol) {
            (holder.itemView as ViewHolderRenderProtocol).render(dataSource[position], position)
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) = SimpleVH(SimpleStringView(context))

    override fun getItemCount() = dataSource.size

    override fun getItemViewType(position: Int) = 1

    override fun notifyDataSetChanged() {   //super的实现
        mObservable.notifyChanged();
    }  
}
```

即:

1. 它引用着一个数据源集合`dataSource`
2. `getItemCount()`用来告诉`RecyclerView`展示的总条目
3. 它并不是直接映射`data -> ViewHolder`， 而是 `data position -> data type -> viewholder`。 所以对于`ViewHolder`来说，它知道的只是它的`view type`

## AdapterDataObservable

`Adapter`是数据源的直接接触者，当数据源发生变化时，它需要通知给`RecyclerView`。这里使用的模式是`观察者模式`。`AdapterDataObservable`是数据源变化时的被观察者。`RecyclerViewDataObserver`是观察者。
 在开发中我们通常使用`adapter.notifyXX()`来刷新UI,实际上`Adapter`会调用`AdapterDataObservable`的`notifyChanged()`:



```csharp
    public void notifyChanged() {
        for (int i = mObservers.size() - 1; i >= 0; i--) {
            mObservers.get(i).onChanged();
        }
    }
```

逻辑很简单，即通知`Observer`数据发生变化。

## RecyclerViewDataObserver

它是`RecycledView`用来监听`Adapter`数据变化的观察者:



```cpp
    public void onChanged() {
        mState.mStructureChanged = true; // RecycledView每一次UI的更新都会有一个State
        processDataSetCompletelyChanged(true);
        if (!mAdapterHelper.hasPendingUpdates()) {
            requestLayout();
        }
    }
```

## LayoutManager

它是`RecyclerView`的布局管理者，`RecyclerView`在`onLayout`时，会利用它来`layoutChildren`,它决定了`RecyclerView`中的子View的摆放规则。但不止如此, 它做的工作还有:

1. 测量子View
2. 对子View进行布局
3. 对子View进行回收
4. 子View动画的调度
5. 负责`RecyclerView`滚动的实现
6. ...

## Recycler

对于`LayoutManager`来说，它是`ViewHolder`的提供者。对于`RecyclerView`来说，它是`ViewHolder`的管理者，是`RecyclerView`最核心的实现。下面这张图大致描述了它的组成:

![img](https:////upload-images.jianshu.io/upload_images/2934684-0978416753d58872.png?imageMogr2/auto-orient/strip|imageView2/2/w/694/format/webp)

### AttachedScrapList



```dart
final ArrayList<ViewHolder> mAttachedScrap = new ArrayList<>();
ArrayList<ViewHolder> mChangedScrap = null;
```

- `View Scrap状态`

相信你在许多`RecyclerView`的`crash log`中都看到过这个单词。它是指`View`在`RecyclerView`布局期间进入分离状态的子视图。即它已经被`deatach`(标记为`FLAG_TMP_DETACHED`状态)了。这种`View`是可以被立即复用的。它在复用时，如果数据没有更新，是不需要调用`onBindViewHolder`方法的。如果数据更新了，那么需要重新调用`onBindViewHolder`。

`mAttachedScrap`和`mChangedScrap`中的View复用主要作用在`adapter.notifyXXX`时。这时候就会产生很多`scrap`状态的`view`。 也可以把它理解为一个`ViewHolder`的缓存。不过在从这里获取`ViewHolder`时完全是根据`ViewHolder`的`position`而不是`item type`。如果在`notifyXX`时data已经被移除掉你，那么其中对应的`ViewHolder`也会被移除掉。

### mCacheViews

可以把它理解为`RecyclerView`的一级缓存。它的默认大小是3, 从中可以根据`item type`或者`position`来获取`ViewHolder`。可以通过`RecyclerView.setItemViewCacheSize()`来改变它的大小。

### RecycledViewPool

它是一个可以被复用的`ViewHolder`缓存池。即可以给多个`RecycledView`来设置统一个`RecycledViewPool`。这个对于`多tab feed流`应用可能会有很显著的效果。它内部利用一个`ScrapData`来保存`ViewHolder`集合:



```java
class ScrapData {
    final ArrayList<ViewHolder> mScrapHeap = new ArrayList<>();
    int mMaxScrap = DEFAULT_MAX_SCRAP;   //最多缓存5个
    long mCreateRunningAverageNs = 0;
    long mBindRunningAverageNs = 0;
}

SparseArray<ScrapData> mScrap = new SparseArray<>();  //RecycledViewPool 用来保存ViewHolder的容器
```

一个`ScrapData`对应一种`type`的`ViewHolder`集合。看一下它的获取`ViewHolder`和保存`ViewHolder`的方法:



```csharp
//存
public void putRecycledView(ViewHolder scrap) {
    final int viewType = scrap.getItemViewType();
    final ArrayList<ViewHolder> scrapHeap = getScrapDataForType(viewType).mScrapHeap;
    if (mScrap.get(viewType).mMaxScrap <= scrapHeap.size())  return; //到最大极限就不能放了
    scrap.resetInternal();  //放到里面，这个view就相当于和原来的信息完全隔离了，只记得他的type，清除其相关状态
    scrapHeap.add(scrap);
}

//取
private ScrapData getScrapDataForType(int viewType) {
    ScrapData scrapData = mScrap.get(viewType);
    if (scrapData == null) {
        scrapData = new ScrapData();
        mScrap.put(viewType, scrapData);
    }
    return scrapData;
}
```

以上所述，是`RecycledView`最核心的组成部分(本文并没有描述动画的部分)。

下一篇文章会分析[RecyclerView的刷新机制