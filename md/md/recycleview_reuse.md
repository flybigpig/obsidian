## 复用



###### ListView  or  Recycleview



RecyclerView与 ListView 的缓存机制原理大致相似， 滑动的时候，离屏的 ItemView 被回收至缓存，入屏的 ItemView 则会优先从缓存中获取。是 ListView 与 RecyclerView 的实现细节有差异。





> **Tips：**ListView只有两级缓存，RecyclerView名义上是有四级缓存，但三级缓存是空实现，需自定义，实质是有三级缓存。



### ListView

listview继承与ablistview,  RecycleBin 是AbListView的内部类。其作用是通过两级缓存来缓存view

##### 一级缓存 :mActiveViews

```
/**
 * Views that were on screen at the start of layout. This array is populated at the start of
 * layout, and at the end of layout all view in mActiveViews are moved to mScrapViews.
 * Views in mActiveViews represent a contiguous range of Views, with position of the first
 * view store in mFirstActivePosition.
 */
private View[] mActiveViews = new View[0];
```



第一级缓存，这些view  是布局过程开始时屏幕上的view ,layout开始时这个数组被填充，layout结束。mActiveViews 的view 移动到mScrapView 意义在于快速重用屏幕上可见的列表项itemview,不需要重新createview  和 bindview.



##### 二级缓存  mScrapView

```
/**
 * Unsorted views that can be used by the adapter as a convert view.
 */
private ArrayList<View>[] mScrapViews;
```



第二级缓存，mScrapView 是多个 List 组成的数据，数组的长度为 viewTypeCount，每个 List 缓存不同类型 Item 布局的 View，其意义在于缓存离开屏幕的 ItemView，目的是让即将进入屏幕的 itemView 重用，当 mAdapter 被更换时，mScrapViews 则被清空。



### Recycleiew四级缓存



RecyclerView 也有一个类专门来管理缓存，不过与 ListView 不同的是，RecylerView 缓存的是 ViewHolder，而且实现的是四级缓存。

#### 一级：Scrap

> 对应ListView 的一级缓存，快速重用屏幕上可见的 ViewHolder。
>
> **简而言之，屏幕内显示的。**

#### 二级：Cached

> 对应ListView的二级缓存，如果仍依赖于 RecyclerView(比如已经滑出可视范围，但还没有被移除掉)，但已经被标记移除的 ItemView 集合被添加到 mAttachedScrap 中。然后如果 mAttachedScrap 中不再依赖时会被加入到 mCachedViews 中，默认缓存 2 个 ItemView，RecycleView 从这里获取的缓存时，如果数据源不变的情况下，无需重新 bindView。
>
> **简而言之，linearlayoutmanager来说cached缓存默认大小为2，起到的作用就是rv滑动时刚被移出屏幕的viewholer的收容所。**

#### 三级：CacheExtension

> 第三级缓存，其实是一个抽象静态类，用于充当附加的缓存池，当 RecyclerView 从 mCacheViews 找不到需要的 View 时，将会从 ViewCacheExtension 中寻找。不过这个缓存是由开发者维护的，如果没有设置它，则不会启用。通常我们也不会设置它，除非有特殊需求，比如要在调用系统的缓存池之前，返回一个特定的视图，才会用到它。
>
> **简而言之，这是一个自定义的缓存，没错rv是可以自定义缓存行为的。
>  目前来说这还只是个空实现而已，从这点来看其实rv所说的四级缓存本质上还只是三级缓存。**

#### 四级：RecycledViewPool（最强大）

> 它是一个缓存池，实现上，是通过一个默认为 5 大小的 ArrayList 实现的。这一点，同 ListView 的 RecyclerBin 这个类一样。每一个 ArrayList 又都是放在一个 Map 里面的，SparseArray 用两个数组用来替代 Map。
>
> 把所有的 ArrayList 放在一个 Map 里面，这也是 RecyclerView 最大的亮点，这样根据 itemType 来取不同的缓存 Holder，每一个 Holder 都有对应的缓存，而只需要为这些不同 RecyclerView 设置同一个 Pool 就可以了。
>
> **简而言之，  pool一般会和cached配合使用，这么来说，cached存不下的会被保存到pool中毕竟cached默认容量大小只有2，但是pool容量       也是有限的当保存满之后再有viewholder到来的话就只能会无情抛弃掉，它也有一个默认的容量大小**



> ```undefined
>   privatestaticfinalintDEFAULT_MAX_SCRAP = 5;
> ```













![](C:\Users\YT_FLY\Desktop\farmework\md\pic\2934684-0978416753d58872.png)

```
mChangedScrap` : 用来保存`RecyclerView`做动画时，被detach的`ViewHolder`。
```

```
mAttachedScrap` : 用来保存`RecyclerView`做数据刷新(`notify`)，被detach的`ViewHolder
```

`mCacheViews` : `Recycler`的一级`ViewHolder`缓存。

`RecyclerViewPool` : `mCacheViews`集合中装满时，会放到这里。







```
/**
         * Attempts to get the ViewHolder for the given position, either from the Recycler scrap,
         * cache, the RecycledViewPool, or creating it directly.
         * <p>
         * If a deadlineNs other than {@link #FOREVER_NS} is passed, this method early return
         * rather than constructing or binding a ViewHolder if it doesn't think it has time.
         * If a ViewHolder must be constructed and not enough time remains, null is returned. If a
         * ViewHolder is aquired and must be bound but not enough time remains, an unbound holder is
         * returned. Use {@link ViewHolder#isBound()} on the returned object to check for this.
         *
         * @param position Position of ViewHolder to be returned.
         * @param dryRun True if the ViewHolder should not be removed from scrap/cache/
         * @param deadlineNs Time, relative to getNanoTime(), by which bind/create work should
         *                   complete. If FOREVER_NS is passed, this method will not fail to
         *                   create/bind the holder if needed.
         *
         * @return ViewHolder for requested position
         */
        @Nullable
        ViewHolder tryGetViewHolderForPositionByDeadline(int position,
                boolean dryRun, long deadlineNs) {
            if (position < 0 || position >= mState.getItemCount()) {
                throw new IndexOutOfBoundsException("Invalid item position " + position
                        + "(" + position + "). Item count:" + mState.getItemCount()
                        + exceptionLabel());
            }
            boolean fromScrapOrHiddenOrCache = false;
            ViewHolder holder = null;
            ///////////////////////////////////////////////////////////////////////////////////////////
            // 0) If there is a changed scrap, try to find from there
            if (mState.isPreLayout()) {
                holder = getChangedScrapViewForPosition(position);
                fromScrapOrHiddenOrCache = holder != null;
            }
             ///////////////////////////////////////////////////////////////////////////////////////////
            // 1) Find by position from scrap/hidden list/cache
            if (holder == null) {
                holder = getScrapOrHiddenOrCachedHolderForPosition(position, dryRun);
                if (holder != null) {
                    if (!validateViewHolderForOffsetPosition(holder)) {
                        // recycle holder (and unscrap if relevant) since it can't be used
                        if (!dryRun) {
                            // we would like to recycle this but need to make sure it is not used by
                            // animation logic etc.
                            holder.addFlags(ViewHolder.FLAG_INVALID);
                            if (holder.isScrap()) {
                                removeDetachedView(holder.itemView, false);
                                holder.unScrap();
                            } else if (holder.wasReturnedFromScrap()) {
                                holder.clearReturnedFromScrapFlag();
                            }
                            recycleViewHolderInternal(holder);
                        }
                        holder = null;
                    } else {
                        fromScrapOrHiddenOrCache = true;
                    }
                }
            }
            if (holder == null) {
                final int offsetPosition = mAdapterHelper.findPositionOffset(position);
                if (offsetPosition < 0 || offsetPosition >= mAdapter.getItemCount()) {
                    throw new IndexOutOfBoundsException("Inconsistency detected. Invalid item "
                            + "position " + position + "(offset:" + offsetPosition + ")."
                            + "state:" + mState.getItemCount() + exceptionLabel());
                }

                final int type = mAdapter.getItemViewType(offsetPosition);
                 ///////////////////////////////////////////////////////////////////////////////////////////
                // 2) Find from scrap/cache via stable ids, if exists
                if (mAdapter.hasStableIds()) {
                    holder = getScrapOrCachedViewForId(mAdapter.getItemId(offsetPosition),
                            type, dryRun);
                    if (holder != null) {
                        // update position
                        holder.mPosition = offsetPosition;
                        fromScrapOrHiddenOrCache = true;
                    }
                }
                if (holder == null && mViewCacheExtension != null) {
                    // We are NOT sending the offsetPosition because LayoutManager does not
                    // know it.
                    final View view = mViewCacheExtension
                            .getViewForPositionAndType(this, position, type);
                    if (view != null) {
                        holder = getChildViewHolder(view);
                        if (holder == null) {
                            throw new IllegalArgumentException("getViewForPositionAndType returned"
                                    + " a view which does not have a ViewHolder"
                                    + exceptionLabel());
                        } else if (holder.shouldIgnore()) {
                            throw new IllegalArgumentException("getViewForPositionAndType returned"
                                    + " a view that is ignored. You must call stopIgnoring before"
                                    + " returning this view." + exceptionLabel());
                        }
                    }
                }
                if (holder == null) { // fallback to pool
                    if (DEBUG) {
                        Log.d(TAG, "tryGetViewHolderForPositionByDeadline("
                                + position + ") fetching from shared pool");
                    }
                    holder = getRecycledViewPool().getRecycledView(type);
                    if (holder != null) {
                        holder.resetInternal();
                        if (FORCE_INVALIDATE_DISPLAY_LIST) {
                            invalidateDisplayListInt(holder);
                        }
                    }
                }
                if (holder == null) {
                    long start = getNanoTime();
                    if (deadlineNs != FOREVER_NS
                            && !mRecyclerPool.willCreateInTime(type, start, deadlineNs)) {
                        // abort - we have a deadline we can't meet
                        return null;
                    }
                    holder = mAdapter.createViewHolder(RecyclerView.this, type);
                    if (ALLOW_THREAD_GAP_WORK) {
                        // only bother finding nested RV if prefetching
                        RecyclerView innerView = findNestedRecyclerView(holder.itemView);
                        if (innerView != null) {
                            holder.mNestedRecyclerView = new WeakReference<>(innerView);
                        }
                    }

                    long end = getNanoTime();
                    mRecyclerPool.factorInCreateTime(type, end - start);
                    if (DEBUG) {
                        Log.d(TAG, "tryGetViewHolderForPositionByDeadline created new ViewHolder");
                    }
                }
            }

            // This is very ugly but the only place we can grab this information
            // before the View is rebound and returned to the LayoutManager for post layout ops.
            // We don't need this in pre-layout since the VH is not updated by the LM.
            if (fromScrapOrHiddenOrCache && !mState.isPreLayout() && holder
                    .hasAnyOfTheFlags(ViewHolder.FLAG_BOUNCED_FROM_HIDDEN_LIST)) {
                holder.setFlags(0, ViewHolder.FLAG_BOUNCED_FROM_HIDDEN_LIST);
                if (mState.mRunSimpleAnimations) {
                    int changeFlags = ItemAnimator
                            .buildAdapterChangeFlagsForAnimations(holder);
                    changeFlags |= ItemAnimator.FLAG_APPEARED_IN_PRE_LAYOUT;
                    final ItemHolderInfo info = mItemAnimator.recordPreLayoutInformation(mState,
                            holder, changeFlags, holder.getUnmodifiedPayloads());
                    recordAnimationInfoIfBouncedHiddenView(holder, info);
                }
            }

            boolean bound = false;
            if (mState.isPreLayout() && holder.isBound()) {
                // do not update unless we absolutely have to.
                holder.mPreLayoutPosition = position;
            } else if (!holder.isBound() || holder.needsUpdate() || holder.isInvalid()) {
                if (DEBUG && holder.isRemoved()) {
                    throw new IllegalStateException("Removed holder should be bound and it should"
                            + " come here only in pre-layout. Holder: " + holder
                            + exceptionLabel());
                }
                final int offsetPosition = mAdapterHelper.findPositionOffset(position);
                bound = tryBindViewHolderByDeadline(holder, offsetPosition, position, deadlineNs);
            }

            final ViewGroup.LayoutParams lp = holder.itemView.getLayoutParams();
            final LayoutParams rvLayoutParams;
            if (lp == null) {
                rvLayoutParams = (LayoutParams) generateDefaultLayoutParams();
                holder.itemView.setLayoutParams(rvLayoutParams);
            } else if (!checkLayoutParams(lp)) {
                rvLayoutParams = (LayoutParams) generateLayoutParams(lp);
                holder.itemView.setLayoutParams(rvLayoutParams);
            } else {
                rvLayoutParams = (LayoutParams) lp;
            }
            rvLayoutParams.mViewHolder = holder;
            rvLayoutParams.mPendingInvalidate = fromScrapOrHiddenOrCache && bound;
            return holder;
        }
```

> 即大致步骤是:
>
> 1. 如果执行了`RecyclerView`动画的话，尝试`根据position`从`mChangedScrap集合`中寻找一个`ViewHolder`
> 2. 尝试`根据position`从`scrap集合`、`hide的view集合`、`mCacheViews(一级缓存)`中寻找一个`ViewHolder`
> 3. 根据`LayoutManager`的`position`更新到对应的`Adapter`的`position`。 (这两个`position`在大部分情况下都是相等的，不过在`子view删除或移动`时可能产生不对应的情况)
> 4. 根据`Adapter position`,调用`Adapter.getItemViewType()`来获取`ViewType`
> 5. 根据`stable id(用来表示ViewHolder的唯一，即使位置变化了)`从`scrap集合`和`mCacheViews(一级缓存)`中寻找一个`ViewHolder`
> 6. 根据`position和viewType`尝试从用户自定义的`mViewCacheExtension`中获取一个`ViewHolder`
> 7. 根据`ViewType`尝试从`RecyclerViewPool`中获取一个`ViewHolder`
> 8. 调用`mAdapter.createViewHolder()`来创建一个`ViewHolder`
> 9. 如果需要的话调用`mAdapter.bindViewHolder`来设置`ViewHolder`。
> 10. 调整`ViewHolder.itemview`的布局参数为`Recycler.LayoutPrams`，并返回Holder
>
> 虽然步骤很多，逻辑还是很简单的,即从几个缓存集合中获取`ViewHolder`，如果实在没有就创建。但比较疑惑的可能就是上述`ViewHolder缓存集合`中什么时候会保存`ViewHolder`。接下来分几个`RecyclerView`的具体情形，来一点一点弄明白这些`ViewHolder缓存集合`的问题
>
> 