---
created: 2025-05-12T10:15:11 (UTC +08:00)
tags: [view的工作原理]
source: https://blog.csdn.net/weixin_73891206/article/details/138926114
author: 成就一亿技术人!
---

# 第四章：View的工作原理-CSDN博客

> ## Excerpt
> 文章浏览阅读1.8k次，点赞34次，收藏44次。Android开发艺术探索第四章_view的工作原理

---
#### 这里写自定义目录标

-   -   [4.1 初始ViewRoot和DecorView](https://blog.csdn.net/weixin_73891206/article/details/138926114#41_ViewRootDecorView_1)
    -   [4.2 理解MeasureSpec](https://blog.csdn.net/weixin_73891206/article/details/138926114#42_MeasureSpec_18)
    -   -   [4.2.1 MeasureSpec](https://blog.csdn.net/weixin_73891206/article/details/138926114#421_MeasureSpec_22)
        -   [4.2.2 MeasureSpec和LayoutParams的对应关系](https://blog.csdn.net/weixin_73891206/article/details/138926114#422_MeasureSpecLayoutParams_32)
    -   [4.3 VIew的工作流程](https://blog.csdn.net/weixin_73891206/article/details/138926114#43_VIew_79)
    -   -   [4.3.1 measure过程](https://blog.csdn.net/weixin_73891206/article/details/138926114#431_measure_83)
        -   [4.3.2layout过程](https://blog.csdn.net/weixin_73891206/article/details/138926114#432layout_268)
        -   [4.3.3 draw过程](https://blog.csdn.net/weixin_73891206/article/details/138926114#433_draw_373)
    -   [4.4自定义View](https://blog.csdn.net/weixin_73891206/article/details/138926114#44View_435)
    -   -   [4.4.1 自定义View的分类](https://blog.csdn.net/weixin_73891206/article/details/138926114#441_View_437)
        -   [4.4.2自定义view须知](https://blog.csdn.net/weixin_73891206/article/details/138926114#442view_448)
        -   [4.4.3 自定义View示例](https://blog.csdn.net/weixin_73891206/article/details/138926114#443_View_462)
-   [](https://blog.csdn.net/weixin_73891206/article/details/138926114#_622)

### 4.1 初始ViewRoot和DecorView

ViewRoot对应于ViewRootImpl类，它是连接WindowManager和DecorView的纽带，View的三大流程都是通过ViewRoot来完成的。在ActivityThread中，当Activity被创建完毕后，会将DecorView添加到Window中，同时会创建ViewRootImpl对象，并将ViewRootImpl对象和DecorView建立联系。

```
root = new ViewRootImpl(view.getContext(),display);
root.setView(view,wparams,panelParentView);
```

View的绘制流程从ViewRoot的perfromTraversals方法开始，他通过measure，layout和[draw](https://so.csdn.net/so/search?q=draw&spm=1001.2101.3001.7020)三个过程才能将View画出来，**measure测量view宽高，layout确定view在父容器的位置，draw将view绘制在屏幕上**。依次调用perfromMeasure，perfromLayout，perfromDraw，他们分别完成顶级View的measure，layout和draw。  
onMeasure会对所有的子元素进行measure，完成从父容器传递到子元素的过程，接着子元素会重复父容器的measure过程，如此反复的完成了整个View[树的遍历](https://so.csdn.net/so/search?q=%E6%A0%91%E7%9A%84%E9%81%8D%E5%8E%86&spm=1001.2101.3001.7020)。

measure过程决定了View的宽高，通过getMeasureWidth和getMeasureHeight来获取View测量后的高宽；layout过程决定了view的四个顶点的坐标和实际View的宽高，一般View测量后的宽高等同于View最终的宽高，可以通过getTop getBottom getLeft getRight来拿到View四个顶点的位置，getWidth getHeight拿到View最终的宽和高；draw决定了View的显示，draw方法完成了后view会显示在屏幕上。  
![img](https://i-blog.csdnimg.cn/blog_migrate/1d8405eefda7f93b14e7947a257cf38d.png)

DecorVIew作为顶级view，一般情况下内部包含一个竖直方向的LinearLayout，这个Linearlayout中有上下两个部分，标题栏和内容栏，内容栏的id是android.R.id.content，这里我们就能理解为什么我们在[设置布局](https://so.csdn.net/so/search?q=%E8%AE%BE%E7%BD%AE%E5%B8%83%E5%B1%80&spm=1001.2101.3001.7020)的时候要调用setContentView方法了。DecorView是一个FrameLayout，所有view事件都先经过他才传递给我们的view。

### 4.2 理解MeasureSpec

MeasureSpec“测量规格””测量说明书”–描述如何测量view的规格大小。 系统会将View的LayoutParams根据父容器所施加的规则转换成对应的MeasureSpec，然后再根据这个measureSpec来测量出View的宽高。**测量高宽不应定等于最终高宽**。

#### 4.2.1 MeasureSpec

MeasureSpec代表一个32位int值，高两位代表SpecMode（测量模式），低30位代表SpecSize（规格大小）。MeasureSpec通过将SpecMode和SpecSize打包成一个int值来避免过多的对象内存分配，并提供SpecMode和SpecSize的打包和解包方法。

SpecMode分为三类：

-   UNSPECIFIED：无限制，要多大给多大；
-   EXACTLY：父容器已经检测出View所需要的精度大小，这个时候view最终的大小就是SpecSize所指定的值，对应于LayoutParams中的match\_parent和具体的数值两种。
-   AT\_MOST：父容器指定了一个可用大小（SpecSize），view的大小不能大于这个值，具体大小要看不同view的具体实现。它对应于LayoutParams中wrap\_content。

#### 4.2.2 MeasureSpec和LayoutParams的对应关系

在view测量的时候，系统会将layoutparams在父容器的约束下转换成对应的MeasureSpec，然后再根据这个MeasureSpec来确定view测量后的宽高。MeasureSpec不是唯一由layoutparams决定的，layoutparams需要和父容器一起决定view的MeasureSpec，从而进一步决定view的宽高。顶级view（DecorView）和普通的view的MeasureSpec转换过程不同：前者由窗口自身尺寸和自身的layoutparams来决定，后者由父容器的MeasureSpec和自身的layoutparams来决定，还和View的Margin、Padding有关。

```
//源码：DecorView中MeasureSpec部分用到的函数
private static int getRootMeasureSpec(int windowSize, int rootDimension){
    int measureSpec;
    switch(rootDimension){
        case ViewGroup.LayoutParams.MATCH_PARENT:
            measureSpec = MeasureSpec.makeMeasureSpec(windowSize,MeasureSpec.EXACTLY);
            break;
       case ViewGroup.LayoutParams.WRAP_CONTENT:
            measureSpec = MeasureSpec.makeMeasureSpec(windowSize,MeasureSpec.At_Most);
            break;
       default:
            measureSpec = MeasureSpec.makeMeasureSpec(rootDimension,MeasureSpec.EXACTLY);
            break;
    }
}
```

```
//源码：viewgroup#measureChildWithMargins
final MarginLayoutParams lp = (MarginLayoutParams) child.getLayoutParams();
final int childWidthMeasureSpec = getChildMeasureSpec(parentWidthMeasureSpec, mPaddingleft + mPaddingright + lp.leftMargin + lp.rightMargin + widthUsed, lp.width);
child.measure(childWidthMeasureSpec, childHeightMeasureSpec);
//在调用子元素的measure方法前会通过getChildMeasureSpec得到子元素的MeasureSpec
```

![img](https://i-blog.csdnimg.cn/blog_migrate/809a4b6e9c8237db1de958973687e548.png)

总结：

当View采用固定宽/高的时候，不管父容器的MeasureSpec是什么，View 的MeasureSpee都是精确模式，那么View也是精准模式并且其大小是父容器的剩余空间；当View是match\_parent时，要么等于父容器剩余空间，要么不大于父容器剩余空间；当View的宽/高是wrap\_content时，不管父容器的模式是精准还是最大化，View的模式总是最大化,并且大小不能超过父容器的剩余空间。

注意上面的parentSize是子元素可用的大小，即父容器尺寸减去已经用过的大小：

```
//源码：getChildMeasureSpec
public static int getChildMeasureSpec(int spec, int padding ,int childDimension){
    int specSize = MeasureSpec.getSize(spec);
int size = Math.max(0, specSize - padding);
    ...
}
```

### 4.3 VIew的工作流程

View的工作流程主要是指measure、layout、draw这三大流程，即测量、布局和绘制，其中measure确定View的**测量宽/高**，layout确定View的**最终宽/高**和四个顶点的位置，而draw将View绘制到屏幕上。

#### 4.3.1 measure过程

measure过程需要分情况来看，如果只是一个原始的View，那么通过measure方法就可以完成了其测量过程，如果是一个ViewGroup，除了完成自己的测量过程外，还会遍历去调用所有子元素的measure方法，各个子元素再递归去执行这个流程。

1.View的measure过程

在View的measure方法中（final类型的方法，子类不能重写此方法）去调用View的onMesure方法：

```
@Override
protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
//setMeasuredDimension会设置View宽/高的测量值
    setMeasuredDimension(
                getDefaultSize(getSuggestedMinimumWidth(), widthMeasureSpec),
                getDefaultSize(getSuggestedMinimumHeight(), heightMeasureSpec));
//getSuggestedMinimumWidth：如果View没有设置背景，那么返回android:minwidth这个属性所指定的值，这个值可以为0：
//如果View设置了背景，则返回 android:minwidth和背景Drawable的原始宽度这两者中的最大值.
}
 
//getDefaultSize返回的大小就是mesureSpec中的specSize，而这个specSize就是view测量后的大小，View最终的大小是在layout阶段确定的，但一般情况两者相同。
    public static int getDefaultSize(int size, int measureSpec) {
        int result = size;
        int specMode = MeasureSpec.getMode(measureSpec);
        int specSize = MeasureSpec.getSize(measureSpec);
        switch (specMode) {
        case MeasureSpec.UNSPECIFIED:
            result = size;//getSuggestedMinimumWidth返回的就是view在unspecified下的测量宽高
            break;
        case MeasureSpec.AT_MOST:
        case MeasureSpec.EXACTLY:
            result = specSize;
            break;
        }
        return result;
}

//如果view没有设置背景，那么返回mMinWidth--android:minWidth，这个属性不指定默认为0
//getMinimumWidth()返回drawable的原始宽度
protected int getSuggestedMinimumWidth(){
    return (mBackground == null) ? mMinWidth : max(mMinwidth, mBackground.getMinimumWidth());
}
```

分析getDefaultSize()我们可以得出如下结论：直接继承View的自定义控件需要重写onMeasure方法并设置wrap\_content时的自身大小，否则在布局中使用wrap\_content就相当于使用match\_parent。我们只需要给View指定一个默认的内部宽高，并在wrap\_content时设置此宽高即可。

2.ViewGroup的measure过程

对于ViewGroup而言，除了完成自己的measure过程以外，还会遍历去调用所有子元素的measure方法，各个子元素再通归去执行这个过程。和View不同的是，ViewGroup是一个抽象类，因此它没有重写View的onMeasure方法，但是它提供了一个叫measureChildren，在ViewGroup的measure时，会对每一个子元素调用measureChild进行测量。

```
   protected void measureChildren(int widthMeasureSpec, int heightMeasureSpec) {
        final int size = mChildrenCount;
        final View[] children = mChildren;
        for (int i = 0; i < size; ++i) {
            final View child = children[i];
            if ((child.mViewFlags & VISIBILITY_MASK) != GONE) {
                measureChild(child, widthMeasureSpec, heightMeasureSpec);
            }
        }
}
 
 protected void measureChild(View child, int parentWidthMeasureSpec,
            int parentHeightMeasureSpec) {

        final LayoutParams lp = child.getLayoutParams();
        final int childWidthMeasureSpec = getChildMeasureSpec(parentWidthMeasureSpec,
                mPaddingLeft + mPaddingRight, lp.width);

        final int childHeightMeasureSpec = getChildMeasureSpec(parentHeightMeasureSpec,
                mPaddingTop + mPaddingBottom, lp.height);
        //MeasureSpec直接传递给View的measure方法来进行测量
        child.measure(childWidthMeasureSpec, childHeightMeasureSpec);
}
```

ViewGroup没有定义测量的具体过程，他是一个抽象类，测量过程的onMeasure方法需要各个子类去实现，比如LinearLayout、RelativeLayout等。为什么ViewGroup不像view一样对onMeasure方法做统一的实现呢？因为不同的ViewGroup有不同的布局特性，导致他们的测量细节各不相同。下面通过LInearLayout的onMeasure方法来分析ViewGroup的measure过程。

-   在onMeasure中根据orientation选择测量过程
-   假设这里是vertical，调用measureVertical方法，在方法中会遍历子元素并对每一个子元素执行measureChildBeforeLayout方法，这个方法内部会调用子元素的measure方法；mTotalLength这个变量来存储LinearLayout在竖直方向上的高度。每测量一个元素，这个变量就会增加，增加的部分包括子元素的高度以及子元素在竖直方向上的margin等。
-   当子元素测量完毕之后，LinearLayout会根据子元素的情况来测量自己的大小，在水平方向的测量过程遵循View的测量过程；在竖直方向上，若高度采用的是match\_parent或者具体值，那么他的绘制过程和View一致，若采用warp\_content，那么它的高度是所有的子元素所占用的高度+竖直方向上的Padding，但是不能超过父容器的剩余空间

```
//三个参数分别为：测量大小，父容器传递给子容器的测量规格，子视图测量状态
public static int resolveSizeAndState(int size, int measureSpec, int childMeasuredState){
    int result = size;
    int specMode = MeasureSpec.getMode(measureSpec);
    int specSize = MeasureSpec.getSize(measureSpec);
    switch(specMode){
        case MeasureSpec.UNSPECIFIED:
            result = size;
            break;
      case MeasureSpec.AT_MOST:
            if(specSize < size){
                result = specSize | MEASURED_STATE_TOO_SMALL;
            }else{
                result = size;
   }
            break;      
      case MeasureSpec.EXACTLY:
            result = specSize;
            break;      
    }
    return result | (childMeasuredState & MEASURED_STATE_MASK);
}
```

___

现在我们需要在Activity已启动的时候做一件任务，这一件任务需要获取某个View的宽/高，在onCreate、onStart、onResume中均无法正确得View的宽/高信息，这是因为View的measure过程和Activity的生命周期方法不是同步执行的，因此无法保证Activiy执行了onCreate、onStart、onResume时某个View已经测量完毕了，如果View还没有测量完毕，那么获得的宽/高就是0。如何解决？

-   Activity/View#onWindowFocusChanged：
    
    View已经初始化完毕了，宽/高已经准备好了，这个时候去获取宽/高是没问题的。当activity的窗口得到焦点和失去焦点的时候这个方法会被调用。
    
-   view.post(runnable)
    
    通过post可以将一个runnable投递到消息队列，然后等到Lopper调用runnable的时候，View也就初始化好了，典型代码如下:
    
    ```
       @Override
        protected void onStart() {
            super.onStart();
            mTextView.post(new Runnable() {
                @Override
                public void run() {
                    int width = mTextView.getMeasuredWidth();
                    int height = mTextView.getMeasuredHeight();
                }
            });
        }
    ```
    
-   ViewTreeObserver
    
    使用ViewTreeObserver的众多回调可以完成这个功能，比如使用OnGlobalLayoutListener这个接口，当View树的状态发生改变或者View树内部的View的可见性发生改变，onGlobalLayout方法就会回调。
    
    ```
    @Override
    protected void onStart() {
        super.onStart();
        ViewTreeObserver observer = mTextView.getViewTreeObserver();
        observer.addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener(){
            @Override
            public void onGlobalLayout() {
                mTextView.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                int width = mTextView.getMeasuredWidth();
                int height = mTextView.getMeasuredHeight();
            }
        });
    }
    ```
    
-   view.measure(int widthMeasureSpec, int heightMeasureSpec)
    
    通过手动对View进行measure来得到view的宽高。这种方式比较复杂，这里需要根据View的LayoutParams进行讨论：
    
    **match\_parent**
    
    直接放弃，无法measure出具体的宽高。因为根据View的measure过程，构造此种MeasureSpec需要知道parentSize，即父容器的剩余空间，而这个时候我们无法知道parentSize。
    
    **具体的数值dp/px**
    
    比如宽高都是100dp
    
    ```
      int widthMeasureSpec = View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.
    EXACTLY);
      int heightMeasureSpec = View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.
    EXACTLY);
      mTextView.measure(widthMeasureSpec,heightMeasureSpec);
    ```
    
    **wrap\_content**
    
    View的MwasureSpec中size是三十位的二进制表示，也就是说最大是30个1（2^30-1）,也就是（1<30-1）,在最大的模式下，我们用View理论上能支持最大值去构造MwasureSpec是合理的。
    

```
int widthMeasureSpec = View.MeasureSpec.makeMeasureSpec((1<<30)-1, View.MeasureSpec.
AT_MOST);
 int heightMeasureSpec = View.MeasureSpec.makeMeasureSpec((1<<30)-1, View.MeasureSpec.
AT_MOST);
mTextView.measure(widthMeasureSpec,heightMeasureSpec);
```

#### 4.3.2layout过程

Layout的作用是ViewGroup用来确定子元素的位置，当ViewGroup的位置被确认之后，他会在onLayout中去遍历所有子元素并且其调用layout方法，在layout方法中onLayou又被调用。layout方法确定了View本身的位置，而onLayout方法则会确定所有子元素的位置，先看View的layout方法：



```
public void layout(int l, int t, int r, int b) {
        if ((mPrivateFlags3 & PFLAG3_MEASURE_NEEDED_BEFORE_LAYOUT) != 0) {
            onMeasure(mOldWidthMeasureSpec, mOldHeightMeasureSpec);
            mPrivateFlags3 &= ~PFLAG3_MEASURE_NEEDED_BEFORE_LAYOUT;
        }

        int oldL = mLeft;
        int oldT = mTop;
        int oldB = mBottom;
        int oldR = mRight;
        //setFrame方法来设定View的四个顶点的位置,四个顶点一旦确定，那么View在父容器的位置也就确定了
        boolean changed = isLayoutModeOptical(mParent) ?
                setOpticalFrame(l, t, r, b) : setFrame(l, t, r, b);

        if (changed || (mPrivateFlags & PFLAG_LAYOUT_REQUIRED) == PFLAG_LAYOUT_REQUIRED) {
            //该方法的用途是调用父容器确定子元素的位置，具体位置实现同样和具体布局有关。
            //所以view和viewgroup均没有真正实现onLayout方法
            onLayout(changed, l, t, r, b);
            mPrivateFlags &= ~PFLAG_LAYOUT_REQUIRED;

            ListenerInfo li = mListenerInfo;
            if (li != null && li.mOnLayoutChangeListeners != null) {
                ArrayList<OnLayoutChangeListener> listenersCopy =
                        (ArrayList<OnLayoutChangeListener>)li.mOnLayoutChangeListeners.clone();
                int numListeners = listenersCopy.size();
                for (int i = 0; i < numListeners; ++i) {
                    listenersCopy.get(i).onLayoutChange(this, l, t, r, b, oldL, oldT, oldR, oldB);
                }
            }
        }
        mPrivateFlags &= ~PFLAG_FORCE_LAYOUT;
        mPrivateFlags3 |= PFLAG3_IS_LAID_OUT;
    }
```

```
//LinearLayout # onLayout 
@Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        if (mOrientation == VERTICAL) {
            layoutVertical(l, t, r, b);
        } else {
            layoutHorizontal(l, t, r, b);
        }
    }
```

```
 void layoutVertical(int left, int top, int right, int bottom) {
        ...
        final int count = getVirtualChildCount();
        for (int i = 0; i < count; i++) {
            final View child = getVirtualChildAt(i);
            if (child == null) {
                childTop += measureNullChild(i);
            } else if (child.getVisibility() != GONE) {
                final int childWidth = child.getMeasuredWidth();
                final int childHeight = child.getMeasuredHeight();
                final LinearLayout.LayoutParams lp =
                        (LinearLayout.LayoutParams) child.getLayoutParams();
                ...
               if (hasDividerBeforeChildAt(i)) {
                    childTop += mDividerHeight;
                }
               childTop += lp.topMargin;
               //setChildFrame其实是调用元素的layout方法
               setChildFrame(child, childLeft, childTop + getLocationOffset(child), childWidth, childHeight);
               childTop += childHeight + lp.bottomMargin + getNextLocationOffset(child);

                i += getChildrenSkipCount(child, i);
            }
        }
    }
```

```
    //width和height实际上就是子元素测量宽高
    private void setChildFrame(View child, int left, int top, int width, int height) {        
        child.layout(left, top, left + width, top + height);
    }
```

___

View的测量宽高和最终宽高有什么区别？–即：View的getMeasuredWidth 和getWidth这两个方法有什么区别？

```
public final int getWidth(){
    return mRight - mLeft;
}
```

根据源码，getWidth返回的刚好是View测量的测量宽度，getHeight是测量高度，在View的默认实现中，View的测量宽高和最终的是一样的，只不过一个是measure过程，一个是layout过程，而最终形成的是layout过程，即两者的**赋值时机不同**，测量宽高的赋值时机，稍微早一些。一般相等。

#### 4.3.3 draw过程

Draw的作用是将View绘制到屏幕上面，主要有以下几个步骤：

1.绘制背景background.draw(canvas)；

2.绘制自己(onDraw)；

3.绘制children(dispatchDraw)；

4.绘制装饰(onDrawScrollBars)。

```
//源码：draw
   public void draw(Canvas canvas) {
        final int privateFlags = mPrivateFlags;
       //判断是否完全为脏区域--即需要重新绘制
        final boolean dirtyOpaque = (privateFlags & PFLAG_DIRTY_MASK) == PFLAG_DIRTY_OPAQUE &&
                (mAttachInfo == null || !mAttachInfo.mIgnoreDirtyState);
        mPrivateFlags = (privateFlags & ~PFLAG_DIRTY_MASK) | PFLAG_DRAWN;
        /*
         *      1. Draw the background
         *      2. If necessary, save the canvas' layers to prepare for fading
         *      3. Draw view's content
         *      4. Draw children
         *      5. If necessary, draw the fading edges and restore layers
         *      6. Draw decorations (scrollbars for instance)
         *skip step 2 & 5 if possible (common case)
         */
 
        int saveCount;
 
        if (!dirtyOpaque) {
            drawBackground(canvas);
        }
 
        // 判断是否需要绘制边缘渐变效果
        final int viewFlags = mViewFlags;
        boolean horizontalEdges = (viewFlags & FADING_EDGE_HORIZONTAL) != 0;
        boolean verticalEdges = (viewFlags & FADING_EDGE_VERTICAL) != 0;
        if (!verticalEdges && !horizontalEdges) {
           
            if (!dirtyOpaque) onDraw(canvas);
 //View绘制过程的传递是通过dispatchDraw实现的，dispatchDraw调用子元素的draw方法
            dispatchDraw(canvas);
            
            // Overlay is part of the content and draws beneath Foreground
            if (mOverlay != null && !mOverlay.isEmpty()) {
                mOverlay.getOverlayView().dispatchDraw(canvas);
            }
 
            onDrawForeground(canvas);
 
            return;
        }
     ...
    }
```

> view有一个特殊的方法： setWillNotDraw
> 
> 如果一个view不需要绘制任何内容，那么设置这个标记位后，系统会进行相应的优化。默认情况下，view没有启用这个标记位，但是viewgroup会默认启用这个标记位。当明确知道一个viewgroup需要通过ondraw来绘制内容时，我们需要显式的关闭这个标记位。

### 4.4自定义View

#### 4.4.1 自定义View的分类

-   1.继承View重写onDraw方法  
    重写了绘制，自己实现某些图形，原生控件已经满足不了你了，重写onDraw方法，采用这个方式需要自身支持warp\_content,并且padding也要自己处理。
-   2.继承ViewGroup派生出来的Layout  
    重写容器，实现自定义布局。除了LinearLayout、RelativeLayout、FrameLayout外，重新定义布局，像是几种View组合在一起。需要合理处理viewgroup测量、布局，并处理好子元素的测量和布局过程。
-   3.继承特定的View  
    用于拓展某种已有的view的功能，较为容易实现，不需要自己支持wrap\_content和padding等。
-   4.继承特定的ViewGroup  
    像是几种view组合在一起。这种方法不需要自己处理viewgroup的测量和布局。方法2能实现的效果方法4也能实现，方法二更接近于底层。

#### 4.4.2自定义view须知

1.让View支持warp\_content  
直接继承View或ViewGroup控件，需要在onMeasure中对wrap\_content进行特殊处理；  
2.如果有必要，让View支持Padding  
直接继承View控件的需要在draw方法中处理padding;另外，继承自ViewGroup的控件需要在onMeasure和onLayout中考虑padding和子元素的margin。  
3.尽量不要在View中使用Handler  
View内部本身提供了post系列方法，完全可以替代Handler的作用。  
4.View中如果有线程和动画，需要及时停止View#onDetachedFromWindow  
当包含此view的activity退出或者当前view被remove时，不停止这个线程或者动画，容易导致内存溢出，View的onDetachedFromWindow会被调用，与之相对应的是onAttachedToWindow。  
5.View代用滑动嵌套情形时，需要处理好滑动冲突

#### 4.4.3 自定义View示例

**1.继承view重写onDraw方法**

> 这里我们绘制一个简单的圆：需要重写onDraw方法；自己支持wrap\_content ，并且padding也需要处理；为了提高便捷性，还需要对外提供自定义属性

```
/**
 * 实现了一个具有圆形效果的自定义View，它会在自己的中心点以宽高的最小值为直径绘制一个红色的实心圆。
 */
public class CirecleView extends View {
    //
    private int mColor = Color.RED;
    private Paint mpaint = new Paint(Paint.ANTI_ALIAS_FLAG);
 
    public CirecleView(Context context) {
        super(context);
        init();
    }
 
    public CirecleView(Context context, AttributeSet attrs) {
        this(context,attrs,0);
    }
 
    public CirecleView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        //加载自定义属性集合CircleView
        TypedArray a = context.obtainStyledAttributes(attrs,R.styleable.CirecleView);
        //解析CircleView属性集合中的circle_color属性，它的id刚才已经定义了
        mColor = a.getColor(R.styleable.CirecleView_circle_color,Color.RED);
        //解析完成后，通过recycle来释放资源
        a.recycle();
        init();
    }
 
    private void init() {
        mpaint.setColor(mColor);
    }
 
    //解决wrap_content不起作用的问题，设置一个默认值，这里是200
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        int widthSpecMode = MeasureSpec.getMode(widthMeasureSpec);
        int widthSpecSize = MeasureSpec.getSize(widthMeasureSpec);
        int heightSpecMode = MeasureSpec.getMode(heightMeasureSpec);
        int heightSpecSize = MeasureSpec.getSize(heightMeasureSpec);
 
        if(widthSpecMode == MeasureSpec.AT_MOST && heightSpecMode== MeasureSpec.AT_MOST){
            setMeasuredDimension(200,200);
        }else if(widthSpecMode == MeasureSpec.AT_MOST){
            setMeasuredDimension(200,heightSpecSize);
        }else if(heightSpecMode == MeasureSpec.AT_MOST){
            setMeasuredDimension(widthMeasureSpec,200);
        }
    }
 
    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
//        int width = getWidth();
//        int height = getHeight();
//        int radius = Math.min(width, height) / 2;
//        canvas.drawCircle(width/2,height/2,radius,mpaint);
        //解决padding不起作用的问题：在绘制的时候考虑view四周的空白
        final int paddingLeft = getPaddingLeft();
        final int paddingRight = getPaddingRight();
        final int paddingTop = getPaddingTop();
        final int paddingBottom = getPaddingBottom();
        int width = getWidth() - paddingLeft - paddingRight;
        int height = getHeight() - paddingTop - paddingBottom;
        int radius = Math.min(width, height) / 2;
        canvas.drawCircle(paddingLeft+width/2,paddingTop+height/2,radius,mpaint);
    }
}
```

自定义属性

在values目录下面创建自定义属性的xml，比如attrs\_circle\_view.xml，一般都以attrs\_开头

```
<resources>
    <declare-styleable name="CirecleView">
        <attr name="circle_color" format="color" />
    </declare-styleable>
    <!--定义了格式为color的属性circle_color-->
</resources>
```

上面定义了一个自定义属性集合CirecleView，在这里有一个格式为“color”的属性“circle\_color”。color–颜色，自定义属性还有其他格式：reference–资源id；dimension–尺寸；string，integer，boolean等基本数据类型。在xml中使用自定义属性时，要添加schemas声明：

> xmlns:app=http://schemas.android.com/apk/res-auto

**2.继承viewgroup派生特殊的Layout**

需要合适的处理viewgroup和子元素的测量及布局过程。

这里我们实现一个水平方向的线性布局LinearLayout，它内部的子元素可以水平滑动并且子元素元素内部可以竖直滑动。假设所有子元素的宽高都是一样的。

```
@Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        int measureWidth = 0;
        int measureHeight = 0;
        //首先判断是否有子元素
        final int childrenCount = getChildCount();
        measureChildren(widthMeasureSpec, heightMeasureSpec);
        
        int widthSpecSize = MeasureSpec.getSize(widthMeasureSpec);
        int widthSpecMode = MeasureSpec.getMode(widthMeasureSpec);
        int heightSpecSize = MeasureSpec.getSize(heightMeasureSpec);
        int heightSpecMode = MeasureSpec.getMode(heightMeasureSpec);
        //如果没有子元素就直接把自己的宽高设置为零
        if (childrenCount == 0) {
            setMeasuredDimension(0, 0);
        } else if (widthSpecMode == MeasureSpec.AT_MOST && heightSpecMode == MeasureSpec.AT_MOST) {
            final View childView = getChildAt(0);
            //如果高采用了wrap_content，那么高度就是第一个元素的高度。
            measureWidth = childView.getMeasuredWidth() * childrenCount;
            //如果宽采用了wrap_content，那么宽度就是所有子元素的宽度之和。
            measureHeight = childView.getMeasuredHeight();
            setMeasuredDimension(measureWidth,measureHeight);
        }else if(heightSpecMode == MeasureSpec.AT_MOST){
            final View childView = getChildAt(0);
            measureHeight = childView.getMeasuredHeight();
            setMeasuredDimension(widthSpecSize,measureHeight);
        }else if(widthSpecMode == MeasureSpec.AT_MOST){
            final View childView = getChildAt(0);
            measureWidth = childView.getMeasuredWidth() * childrenCount;
            setMeasuredDimension(measureWidth,heightSpecSize);
        }
//        不规范之处有两点：1.没有元素时不应该把宽高设置为零，应该根据LayoutParams的宽高来处理；
//        2.未考虑它的Padding和子元素的margin。
    }
```

```
//    首先遍历所有子元素，若果不是处于GONE状态下，通过Layout将其放在合适的位置上，位置是从左往右的，
//    但是仍然没有考虑padding和子元素的margin，这个也不是很规范，
    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        int childleft = 0;
        final int childCount = getChildCount();
        childrenSize = childCount;
        for(int i= 0 ;i<childCount;i++){
            final View childView = getChildAt(i);
            if(childView.getVisibility()!=View.GONE){
                final int childWidth = childView.getMeasuredWidth();
                mChildWidth = childWidth;
                childView.layout(childleft,0,childleft+childWidth,childView.getMeasuredHeight());
                childleft+=childWidth;
            }
        }
    }
```

##
