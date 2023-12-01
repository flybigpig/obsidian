## RecycleView 刷新

1. `adapter.notifyXXX()`时RecyclerView的UI刷新的逻辑,即`子View`是如何添加到`RecyclerView`中的。
2. 在数据存在的情况下，滑动`RecyclerView`时`子View`是如何添加到`RecyclerView`并滑动的。

###  adapter.notifyDataSetChanged()

上篇介绍到`adapter.notifyDataSetChanged()`时，会引起`RecyclerView`重新布局(`requestLayout`)，



adapter.notifyDataSetChanged()会调用到adapter.notifyChanged() 

![](C:\Users\YT_FLY\Desktop\farmework\md\pic\微信图片_20210511163044.png)

而mObserves 指的是 AdapterDataObservable

mObserves.get(i)指的是RecycleviewDataObserver

是在setadapter的时候注册，通过mObservers.add(observer) 添加进去

![](../blog/blog/source/_posts/md/pic/image-20210602190153275.png)

#### 界面刷新顺序

在ViewRootImpl中，重写了requestLayout方法，我们看看这个方法，

```
@Override
public void requestLayout() {
    if (!mHandlingLayoutInLayoutRequest) {
        checkThread();
        mLayoutRequested = true;
        scheduleTraversals();
    }
}
```



> **小结**：子View调用requestLayout方法，会标记当前View及父容器，同时逐层向上提交，直到ViewRootImpl处理该事件，ViewRootImpl会调用三大流程，从measure开始，对于每一个含有标记位的view及其子View都会进行测量、布局、绘制