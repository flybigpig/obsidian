## 1\. 分屏窗口尺寸计算

### 1.1 窗口添加到WMS

Activity首次启动之后，在其resume阶段会将自己的Window添加到WMS：

```
    void makeVisible() {
        if (!mWindowAdded) {
            ViewManager wm = getWindowManager();
           //顶层DecorView
            wm.addView(mDecor, getWindow().getAttributes());
            mWindowAdded = true;
        }
        mDecor.setVisibility(View.VISIBLE);
    }
```

ViewRootImpl.setView

```
public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
          synchronized (this) {
            if (mView == null) {
                //顶层DecorView
                mView = view;
                      .......
                       //添加窗口到WMS，mWindow（Binder类型W，传给WMS以便WMS可以调用应用进程方法）
                      res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
                            getHostVisibility(), mDisplay.getDisplayId(), mTmpFrame,
                            mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
                            mAttachInfo.mOutsets, mAttachInfo.mDisplayCutout, mInputChannel,
                            mTempInsets);
                    //mTmpFrame为WMS计算得到的窗口尺寸
                    setFrame(mTmpFrame);
                      ......
                }
           }
}
```

窗口通过`addToDisplay`添加到WMS之后，WMS会粗略计算当前窗口的尺寸

WMS.addWindow

```
public int addWindow(Session session, IWindow client, int seq,
            LayoutParams attrs, int viewVisibility, int displayId, Rect outFrame,
            Rect outContentInsets, Rect outStableInsets, Rect outOutsets,
            DisplayCutout.ParcelableWrapper outDisplayCutout, InputChannel outInputChannel,
            InsetsState outInsetsState) {
            ......
            final Rect taskBounds;
            final boolean floatingStack;
            if (atoken != null && atoken.getTask() != null) {
                taskBounds = mTmpRect;
                //重点：这里getBounds得到的尺寸是在Activity启动阶段调用setBounds设置的
                atoken.getTask().getBounds(mTmpRect);
                floatingStack = atoken.getTask().isFloating();
            } else {
                taskBounds = null;
                floatingStack = false;
            }
             //重点方法getLayoutHintLw
            if (displayPolicy.getLayoutHintLw(win.mAttrs, taskBounds, displayFrames, floatingStack,
                    outFrame, outContentInsets, outStableInsets, outOutsets, outDisplayCutout)) {
                res |= WindowManagerGlobal.ADD_FLAG_ALWAYS_CONSUME_SYSTEM_BARS;
            }
            ......
}
```

以上同样只关注和窗口尺寸相关的代码，上述最重要的点是获取`taskBounds`，这个值会获取Activity启动阶段调用`setBounds`设置的值，对于分屏窗口来说当分屏窗口启动之后会调用`resizeDockedStack`设置分屏栈边界，这个方法最终调用的就是`setBounds`方法设置边界的值，WMS此处就是获取分屏窗口自己设置的值，接着在看`getLayoutHintLw`方法之前，先来看看`DisplayFrames`中的几个边界`Rect`：

```
//DisplayFrames.java
//以模拟器尺寸Rect(0, 0 - 800, 480)为例
 /**
     * 真实屏幕的尺寸  Rect(0, 0 - 800, 480)
     */
        public final Rect mOverscan = new Rect();

   /**
     * 除开状态栏，导航栏，输入法的内容区域  Rect(0, 57 - 800, 396)
     */
    public final Rect mCurrent = new Rect();

/**
     * 包含状态栏导航栏的屏幕可见区域  Rect(0, 0 - 800, 480)
     */
    public final Rect mUnrestricted = new Rect();

 /**
     * 除开导航栏的内容区域  Rect(0, 0 - 800, 396)
     */
    public final Rect mRestricted = new Rect();

/** 除开状态栏，导航栏的内容区域  Rect(0, 57 - 800, 396) */
    public final Rect mStable = new Rect();

  /**
     * 除开导航栏的内容区域  Rect(0, 0 - 800, 396)
     */
    public final Rect mStableFullscreen = new Rect();
```

上述的几个边界值在窗口计算中非常重要，它们的值是在`DisplayPolicy.beginLayoutLw`中完成初始化的，Android窗口尺寸计算非常依赖这四个边界值，屏幕区域，状态栏区域，导航栏区域，输入法区域。

有了上述几个尺寸接下来看`DisplayPolicy.getLayoutHintLw`：

```
public boolean getLayoutHintLw(LayoutParams attrs, Rect taskBounds,
            DisplayFrames displayFrames, boolean floatingStack, Rect outFrame,
            Rect outContentInsets, Rect outStableInsets,
            Rect outOutsets, DisplayCutout.ParcelableWrapper outDisplayCutout) {
        //这里获取和窗口尺寸计算相关的flag，
        //如WindowManager.LayoutParams.FLAG_FULLSCREEN，全屏
        //WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS等，半透明状态栏
        final int fl = PolicyControl.getWindowFlags(null, attrs);
        final int pfl = attrs.privateFlags;
       //获取和SystemUI相关 flag，
       //这些flag定义在View中，大多和是否全屏显示，是否隐藏状态栏，是否隐藏导航栏相关
        final int requestedSysUiVis = PolicyControl.getSystemUiVisibility(null, attrs);
       
        final int sysUiVis = requestedSysUiVis | getImpliedSysUiFlagsForLayout(attrs);
       //屏幕旋转角度
        final int displayRotation = displayFrames.mRotation;
    
       //是否使用超出真实屏幕的底部像素值
        final boolean useOutsets = outOutsets != null && shouldUseOutsets(attrs, fl);
        if (useOutsets) {
            //这个值定义在configs.xml中（config_windowOutsetBottom），默认为0，
            int outset = mWindowOutsetBottom;
            if (outset > 0) {
                if (displayRotation == Surface.ROTATION_0) {
                    outOutsets.bottom += outset;
                } else if (displayRotation == Surface.ROTATION_90) {
                    outOutsets.right += outset;
                } else if (displayRotation == Surface.ROTATION_180) {
                    outOutsets.top += outset;
                } else if (displayRotation == Surface.ROTATION_270) {
                    outOutsets.left += outset;
                }
            }
        }

        final boolean layoutInScreen = (fl & FLAG_LAYOUT_IN_SCREEN) != 0;
        final boolean layoutInScreenAndInsetDecor = layoutInScreen
                && (fl & FLAG_LAYOUT_INSET_DECOR) != 0;
        final boolean screenDecor = (pfl & PRIVATE_FLAG_IS_SCREEN_DECOR) != 0;

        if (layoutInScreenAndInsetDecor && !screenDecor) {
            if ((sysUiVis & SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION) != 0) {
                //包含状态栏导航栏的屏幕可见区域  Rect(0, 0 - 800, 480)
                outFrame.set(displayFrames.mUnrestricted);
            } else {
                //除开导航栏的内容区域  Rect(0, 0 - 800, 396)
                outFrame.set(displayFrames.mRestricted);
            }

            final Rect sf;
            
            //悬浮栈，窗口模式为自由窗口或者画中画的栈
            if (floatingStack) {
                sf = null;
            } else {
               //除开状态栏，导航栏的内容区域  Rect(0, 57 - 800, 396)
                sf = displayFrames.mStable;
            }

            final Rect cf;
            if (floatingStack) {
                cf = null;
            } else if ((sysUiVis & View.SYSTEM_UI_FLAG_LAYOUT_STABLE) != 0) {
                if ((fl & FLAG_FULLSCREEN) != 0) {
                    //除开导航栏的内容区域  Rect(0, 0 - 800, 396)
                    cf = displayFrames.mStableFullscreen;
                } else {
                    //除开状态栏，导航栏的内容区域  Rect(0, 57 - 800, 396)
                    cf = displayFrames.mStable;
                }
            } else if ((fl & FLAG_FULLSCREEN) != 0 || (fl & FLAG_LAYOUT_IN_OVERSCAN) != 0) {
                //真实屏幕的尺寸  Rect(0, 0 - 800, 480)
                cf = displayFrames.mOverscan;
            } else {
                //除开状态栏，导航栏，输入法的内容区域  Rect(0, 57 - 800, 396)
                cf = displayFrames.mCurrent;
            }
     
            if (taskBounds != null) {
                //taskBounds为Activity自己设置的大小，Rect(400, 57 - 800, 396)
                outFrame.intersect(taskBounds);
            }
        
            InsetUtils.insetsBetweenFrames(outFrame, cf, outContentInsets);
            InsetUtils.insetsBetweenFrames(outFrame, sf, outStableInsets);
            outDisplayCutout.set(displayFrames.mDisplayCutout.calculateRelativeTo(outFrame)
                    .getDisplayCutout());
            return mForceShowSystemBars;
        } else {
            if (layoutInScreen) {
                //包含状态栏导航栏的屏幕可见区域  Rect(0, 0 - 800, 480)
                outFrame.set(displayFrames.mUnrestricted);
            } else {
                  //除开状态栏，导航栏的内容区域  Rect(0, 57 - 800, 396)
                outFrame.set(displayFrames.mStable);
            }
            if (taskBounds != null) {
                //taskBounds为Activity自己设置的大小，Rect(400, 57 - 800, 396)
                outFrame.intersect(taskBounds);
            }

            outContentInsets.setEmpty();
            outStableInsets.setEmpty();
            outDisplayCutout.set(DisplayCutout.NO_CUTOUT);
            return mForceShowSystemBars;
        }
    }
```

上述代码的核心其实就是`DisplayFrames`中的几个边界`Rect`和Activity类型窗口的栈边界，无非就是根据窗口添加的flag选用不同的`Rect`，然后做一下`Rect`的加减，

`outFrame`就是最终WMS计算得到的Activity的窗口尺寸，上述`getLayoutHintLw`方法计算的到的尺寸并非最终的尺寸，这里的尺寸更多可以理解为父窗口尺寸，例如如果添加的是一个Activity中的Dialog，这里`outFrame`得到的就是Dialog所属的Activity所在栈的边界值而并非Dialog自己的尺寸，后续将`outFrame`返回给应用进程之后Dialog拿到这个尺寸进行`measure`，此时才能得到Dialog的所要求的尺寸，但这任然可能不是最终尺寸，真正尺寸计算会在后续的WMS.relayout中进行。

这个值粗略计算得到的值会返回给Activity所在应用进程：

```
public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
          synchronized (this) {
            if (mView == null) {
                //顶层DecorView
                mView = view;
                      .......
                       //添加窗口到WMS，mWindow（Binder类型W，传给WMS以便WMS可以调用应用进程方法）
                      res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
                            getHostVisibility(), mDisplay.getDisplayId(), mTmpFrame,
                            mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
                            mAttachInfo.mOutsets, mAttachInfo.mDisplayCutout, mInputChannel,
                            mTempInsets);
                    //mTmpFrame为WMS计算得到的窗口尺寸
                    setFrame(mTmpFrame);
                      ......
                }
           }
}
```

`setFrame`将WMS计算得到的Activity窗口尺寸保存在了`ViewRootImpl`的成员变量`mWinFrame`中：

```
   private void setFrame(Rect frame) {
        mWinFrame.set(frame);
        mInsetsController.onFrameChanged(frame);
    }
```

有了Activity窗口尺寸，接下来Activity要进行自己的测量工作了：

### 1.2 performTraversals

在添加窗口到WMS成功之后会执行View的三大流程，`onMeasure`，`onLayout`，`onDraw`，这三大流程在`ViewRootImpl`的`performTraversals`中执行，这个方法非常复杂，这里重点关注和窗口尺寸计算相关的代码：

```
private void performTraversals() {
        //DecorView
        final View host = mView;
       ......

        WindowManager.LayoutParams lp = mWindowAttributes;
        //这两个变量用来记录Activity窗口的宽高尺寸
        int desiredWindowWidth;
        int desiredWindowHeight;
      
      ......
      //mWinFrame用来记录Activity窗口的尺寸，这个值是WMS计算的到的Rect(400, 57 - 800, 396)
      Rect frame = mWinFrame;
    //首次进入
        if (mFirst) {
            mFullRedrawNeeded = true;
            mLayoutRequested = true;

            final Configuration config = mContext.getResources().getConfiguration();
            //是否使用屏幕的尺寸，TYPE_STATUS_BAR_PANEL，TYPE_INPUT_METHOD，TYPE_VOLUME_OVERLAY这三种类型窗口返回true
            if (shouldUseDisplaySize(lp)) {
                Point size = new Point();
                mDisplay.getRealSize(size);
                desiredWindowWidth = size.x;
                desiredWindowHeight = size.y;
            } else {
                //将Activity宽高保存在这两个变量中，w = 400，h = 339
                desiredWindowWidth = mWinFrame.width();
                desiredWindowHeight = mWinFrame.height();
            }
            ......
                //此方法用来处理可能出现的系统窗口，状态栏，导航栏，输入法，罗升阳老师文章中叫做边衬区域，暂时略过
            dispatchApplyInsets(host);
        } else {
            desiredWindowWidth = frame.width();
            desiredWindowHeight = frame.height();
            //mWidth和mHeight记录上次WMS为Activity计算得到的窗口宽高
            if (desiredWindowWidth != mWidth || desiredWindowHeight != mHeight) {
                //不等说明窗口尺寸发生了变化
                mFullRedrawNeeded = true;
                mLayoutRequested = true;
                windowSizeMayChange = true;
            }
        }
          ...
          //对DecorView进行测量，宽高使用WMS.addWindow计算出来的尺寸
            windowSizeMayChange |= measureHierarchy(host, lp, res,
                    desiredWindowWidth, desiredWindowHeight);
       ......
           //这里有六个条件，满足其中之一就会对窗口进行再次计算
           if (mFirst || windowShouldResize || insetsChanged ||
                viewVisibilityChanged || params != null || mForceNextWindowRelayout) {
               ......
             //针对窗口发生变化的情况进行再次计算
             relayoutResult = relayoutWindow(params, viewVisibility, insetsPending);
               ......
           }

    .....
           
}

```

上面代码关于尺寸计算的代码中最重要的代码就是`relayoutWindow`，此方法用于计算窗口尺寸，`insetsPending`代表是否指定额外的边衬区域，默认为false：

relayoutWindow

```
private int relayoutWindow(WindowManager.LayoutParams params, int viewVisibility,
            boolean insetsPending) throws RemoteException {
       
    //缩放系数 等于1
    float appScale = mAttachInfo.mApplicationScale;
         ......
        //调用WMS的relayout方法
        int relayoutResult = mWindowSession.relayout(mWindow, mSeq, params,
                (int) (mView.getMeasuredWidth() * appScale + 0.5f),
                (int) (mView.getMeasuredHeight() * appScale + 0.5f), viewVisibility,
                insetsPending ? WindowManagerGlobal.RELAYOUT_INSETS_PENDING : 0, frameNumber,
                mTmpFrame, mPendingOverscanInsets, mPendingContentInsets, mPendingVisibleInsets,
                mPendingStableInsets, mPendingOutsets, mPendingBackDropFrame, mPendingDisplayCutout,
                mPendingMergedConfiguration, mSurfaceControl, mTempInsets);
        if (mSurfaceControl.isValid()) {
            mSurface.copyFrom(mSurfaceControl);
        } else {
            destroySurface();
        }
       ......
        //设置WMS再次计算得到窗口尺寸
        setFrame(mTmpFrame);
        mInsetsController.onStateChanged(mTempInsets);
        return relayoutResult;
    }
```

### 1.3 WMS.relayoutWindow

```
public int relayoutWindow(Session session, IWindow client, int seq, LayoutParams attrs,
            int requestedWidth, int requestedHeight, int viewVisibility, int flags,
            long frameNumber, Rect outFrame, Rect outOverscanInsets, Rect outContentInsets,
            Rect outVisibleInsets, Rect outStableInsets, Rect outOutsets, Rect outBackdropFrame,
            DisplayCutout.ParcelableWrapper outCutout, MergedConfiguration mergedConfiguration,
            SurfaceControl outSurfaceControl, InsetsState outInsetsState) {
            
                ......
                if (viewVisibility != View.GONE) {
                    //requestedWidth和requestedHeight是Activity窗口经过测量后得到的自己的想要的宽高
                win.setRequestedSize(requestedWidth, requestedHeight);
            }
                ......
                    //WMS核心功能，刷新系统UI，这里面会去计算窗口尺寸，
                    //遍历系统所有窗口调用其WindowState的computeFrameLw方法
                    mWindowPlacerLocked.performSurfacePlacement(true /* force */);
                ......
                    win.getCompatFrame(outFrame);
                 ......
            }
```

`WMS.relayoutWindow`将Activity窗口经过测量后得到的自己的想要的宽高保存在了其对应的`WindowState`中， `mWindowPlacerLocked.performSurfacePlacement`最终回调到`WindowState`的`computeFrameLw`方法去计算窗口的尺寸：

### 1.4 WindowState.computeFrameLw

每个`WindowState`内部都有一个类`WindowFrames`，这个类中提供了众多`Rect`来描述不同的区域边界，这些`Rect`的值有的来自`DisplayFrames`中，有的来自`computeFrameLw`的计算，`DisplayFrames`中的`Rect`描述的是窗口的基础边界，例如，屏幕宽高，状态栏，导航栏宽高，而一个窗口的真正尺寸还是需要自己经过计算得到，例如有的窗口没有状态栏或者导航栏，有的窗口有输入法等，这些情况都是要实际计算时才能知道。

先看`WindowFrames`中的窗口基础边界如何赋值的，Android系统中任意一个窗口的添加都会触发系统中所有窗口尺寸重新计算，每个窗口都会调用`DisplayPolicy`的`layoutWindowLw`方法，

```
public void layoutWindowLw(WindowState win, WindowState attached, DisplayFrames displayFrames) {
    ......
     final WindowFrames windowFrames = win.getWindowFrames();
    
        final Rect pf = windowFrames.mParentFrame;
        final Rect df = windowFrames.mDisplayFrame;
        final Rect of = windowFrames.mOverscanFrame;
        final Rect cf = windowFrames.mContentFrame;
        final Rect vf = windowFrames.mVisibleFrame;
        final Rect dcf = windowFrames.mDecorFrame;
        final Rect sf = windowFrames.mStableFrame;
    ......
           //经过各种条件判断，最后会对上述Rect赋值，赋的值全部来自DisplayFrames中
           
        .....
        //有了上述窗口的基础边界之后便开始窗口自己尺寸的计算了
       win.computeFrameLw();
    .....
}
```

上述赋值过程省略了，给一张最后赋值完成的图：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/01dd23081785cab58df74c8ff6e66ac8.png)

关注重点是窗口自己尺寸的计算：

```
@Override
    public void computeFrameLw() {
        if (mWillReplaceWindow && (mAnimatingExit || !mReplacingRemoveRequested)) {
            return;
        }
        mHaveFrame = true;
        
        //获取窗口对应的task，非Activity窗口为空
        final Task task = getTask();
        //是否为全屏，非多窗口模式并且getBounds等于Display的getBounds，我们分析的是分屏窗口，所以这里为false
        final boolean isFullscreenAndFillsDisplay = !inMultiWindowMode() && matchesDisplayBounds();
        //分屏不是悬浮窗口，此处为false
        final boolean windowsAreFloating = task != null && task.isFloating();
        
        final DisplayContent dc = getDisplayContent();
        //这里getBounds返回分屏窗口启动是所设置的边界值，即为Rect(400, 57 - 800, 396)
        mInsetFrame.set(getBounds());


        final Rect layoutContainingFrame;
        final Rect layoutDisplayFrame;

        final int layoutXDiff;
        final int layoutYDiff;
        //是否有输入法窗口
        final WindowState imeWin = mWmService.mRoot.getCurrentInputMethodWindow();
        //当前窗口是否为输入法窗口的目标窗口
        final boolean isImeTarget =
                imeWin != null && imeWin.isVisibleNow() && isInputMethodTarget();
        
        if (isFullscreenAndFillsDisplay || layoutInParentFrame()) {
            //分屏窗口不走这里，省略
            ......
        } else {
            //这里的getDisplayedBounds就等于task.getBounds，即为Rect(400, 57 - 800, 396)
            mWindowFrames.mContainingFrame.set(getDisplayedBounds());
            if (mAppToken != null && !mAppToken.mFrozenBounds.isEmpty()) {
                  //冻屏窗口的情况，省略
                  ......
            }
            // 当前窗口是否为输入法窗口的目标窗口
            if (isImeTarget) {
                 //需要计算输入法的情况，省略
                ......
            }
           
            if (windowsAreFloating) {
                //悬浮窗口的情况，省略
                ......
            }
            //获取窗口所在的栈，
            final TaskStack stack = getStack();
            
            if (inPinnedWindowingMode() && stack != null
                    && stack.lastAnimatingBoundsWasToFullscreen()) {
                //画中画模式的窗口，省略
                ......
            }
            //mWindowFrames.mDisplayFrame值为 Rect(0, 0 - 800, 480)
            layoutDisplayFrame = new Rect(mWindowFrames.mDisplayFrame);
            //mWindowFrames.mContainingFrame值为 Rect(400, 57 - 800, 396)
            mWindowFrames.mDisplayFrame.set(mWindowFrames.mContainingFrame);
            //layout得到的尺寸和实际尺寸的偏移量，大多数情况为0
            layoutXDiff = mInsetFrame.left - mWindowFrames.mContainingFrame.left;
            layoutYDiff = mInsetFrame.top - mWindowFrames.mContainingFrame.top;
            layoutContainingFrame = mInsetFrame;
            //mTmpRect保存屏幕尺寸，为 Rect(0, 0 - 800, 480)
            mTmpRect.set(0, 0, dc.getDisplayInfo().logicalWidth, dc.getDisplayInfo().logicalHeight);
            
            subtractInsets(mWindowFrames.mDisplayFrame, layoutContainingFrame, layoutDisplayFrame,
                    mTmpRect);
            //layoutInParentFrame代表当前计算尺寸的是否为子窗口
            if (!layoutInParentFrame()) {
                subtractInsets(mWindowFrames.mContainingFrame, layoutContainingFrame,
                        mWindowFrames.mParentFrame, mTmpRect);
                subtractInsets(mInsetFrame, layoutContainingFrame, mWindowFrames.mParentFrame,
                        mTmpRect);
            }
            layoutDisplayFrame.intersect(layoutContainingFrame);
        }
       //对于当前窗口为子窗口或者全屏的情况mWindowFrames.mContainingFrame保存的是父窗口的尺寸，
        //否则mWindowFrames.mContainingFrame保存的就是自己的尺寸
        final int pw = mWindowFrames.mContainingFrame.width();
        final int ph = mWindowFrames.mContainingFrame.height();
        //mRequestedWidth和mRequestedHeight是Activity自己测量出来的自己的DecorView的宽高
        //WMS要结合这个应用自己请求的宽高来计算窗口的尺寸
        if (mRequestedWidth != mLastRequestedWidth || mRequestedHeight != mLastRequestedHeight) {
            mLastRequestedWidth = mRequestedWidth;
            mLastRequestedHeight = mRequestedHeight;
            mWindowFrames.setContentChanged(true);
        }
        
        //mWindowFrames.mFrame保存的是最终窗口计算出来的实际尺寸，computeFrameLw方法最终要计算的就是它的值
        //目前这里还是0
        final int fw = mWindowFrames.mFrame.width();
        final int fh = mWindowFrames.mFrame.height();
        
        //计算mFrame的核心方法，layoutContainingFrame代表的是父窗口尺寸区域，layoutDisplayFrame代表当前窗口栈所在区域
        //大多数情况下这两个值都相等
        applyGravityAndUpdateFrame(layoutContainingFrame, layoutDisplayFrame);

        // 计算超出屏幕区域的部分，省略
          .....

        
        if (windowsAreFloating && !mWindowFrames.mFrame.isEmpty()) {
              //悬浮窗口的情况，省略
            ......
        } else if (mAttrs.type == TYPE_DOCK_DIVIDER) {
             //窗口类型为TYPE_DOCK_DIVIDER，省略
            ......
        } else {
            //mContentFrame的值来自DisplayFrames，为除开状态栏，导航栏的区域 Rect(0, 57 - 800, 396)
            //这里mContentFrame取值为mContentFrame和mFrame中较小的区域
            mWindowFrames.mContentFrame.set(
                    Math.max(mWindowFrames.mContentFrame.left, mWindowFrames.mFrame.left),
                    Math.max(mWindowFrames.mContentFrame.top, mWindowFrames.mFrame.top),
                    Math.min(mWindowFrames.mContentFrame.right, mWindowFrames.mFrame.right),
                    Math.min(mWindowFrames.mContentFrame.bottom, mWindowFrames.mFrame.bottom));
            
            //mVisibleFrame的值来自DisplayFrames，为除开状态栏，导航栏的区域 Rect(0, 57 - 800, 396)
            //这里mVisibleFrame取值为mVisibleFrame和mFrame中较小的区域
            mWindowFrames.mVisibleFrame.set(
                    Math.max(mWindowFrames.mVisibleFrame.left, mWindowFrames.mFrame.left),
                    Math.max(mWindowFrames.mVisibleFrame.top, mWindowFrames.mFrame.top),
                    Math.min(mWindowFrames.mVisibleFrame.right, mWindowFrames.mFrame.right),
                    Math.min(mWindowFrames.mVisibleFrame.bottom, mWindowFrames.mFrame.bottom));
            
            //mStableFrame的值来自DisplayFrames，为除开状态栏，导航栏的区域 Rect(0, 57 - 800, 396)
            //这里mStableFrame取值为mStableFrame和mFrame中较小的区域
            mWindowFrames.mStableFrame.set(
                    Math.max(mWindowFrames.mStableFrame.left, mWindowFrames.mFrame.left),
                    Math.max(mWindowFrames.mStableFrame.top, mWindowFrames.mFrame.top),
                    Math.min(mWindowFrames.mStableFrame.right, mWindowFrames.mFrame.right),
                    Math.min(mWindowFrames.mStableFrame.bottom, mWindowFrames.mFrame.bottom));
            
            //上述三个区域mContentFrame，mVisibleFrame，mStableFrame最后得到的值相等
            //mFrame为计算出来的窗口实际尺寸
        }

        if (isFullscreenAndFillsDisplay && !windowsAreFloating) {
            //全屏并且非悬浮窗口，省略
            ......
        }

        if (mAttrs.type == TYPE_DOCK_DIVIDER) {
          //类型为TYPE_DOCK_DIVIDER的窗口，省略
            ......
        } else {
            //mTmpRect保存了屏幕的尺寸，Rect(0, 0 - 800, 480)，将这个尺寸保存到DisplayContent中去
            getDisplayContent().getBounds(mTmpRect);
            mWindowFrames.calculateInsets(
                    windowsAreFloating, isFullscreenAndFillsDisplay, mTmpRect);
        }

         ......
       //将mFrame保存到mCompatFrame
        mWindowFrames.mCompatFrame.set(mWindowFrames.mFrame);
        ......
            
        if (mIsWallpaper && (fw != mWindowFrames.mFrame.width()
                || fh != mWindowFrames.mFrame.height())) {
                //壁纸窗口
            ......
        }

        ......
    }
```

`computeFrameLw`方法的核心是计算出`mWindowFrames.mFrame`的值，这个值就是窗口的实际尺寸，计算方法`applyGravityAndUpdateFrame`.

### 1.5 mFrame计算

对于分屏窗口，这里接收的两个参数相等都是分屏栈的大小 Rect(400, 57 - 800, 396)

```
private void applyGravityAndUpdateFrame(Rect containingFrame, Rect displayFrame) {  // Rect(400, 57 - 800, 396)
        
        final int pw = containingFrame.width();
        final int ph = containingFrame.height();
        final Task task = getTask();
       //当前窗口是否占满父容器，对分屏窗口来说inNonFullscreenContainer为true，即不会占满父容器
        final boolean inNonFullscreenContainer = !inAppWindowThatMatchesParentBounds();
       //是否允许当前窗口的大小无限制，对分屏窗口来说noLimits为false
        final boolean noLimits = (mAttrs.flags & FLAG_LAYOUT_NO_LIMITS) != 0;

        //是否占满整个屏幕，对分屏窗口来说fitToDisplay为false
        final boolean fitToDisplay = (task == null || !inNonFullscreenContainer)
                || ((mAttrs.type != TYPE_BASE_APPLICATION) && !noLimits);
        float x, y;
        int w,h;
       //是否运行在兼容模式，这里为false
        final boolean inSizeCompatMode = inSizeCompatMode();
       //是否指定窗口缩放系数
        if ((mAttrs.flags & FLAG_SCALED) != 0) {
               //没有指定，省略
               .......
        } else {
            
            if (mAttrs.width == MATCH_PARENT) {
                //如果分屏窗口宽指定为MATCH_PARENT，则w等于栈宽度
                w = pw;
            } else if (inSizeCompatMode) {//兼容模式
                w = (int)(mRequestedWidth * mGlobalScale + .5f);
            } else {
                //否则w等于分屏应用请求的宽度
                w = mRequestedWidth;
            }
            if (mAttrs.height == MATCH_PARENT) {
                //如果分屏窗口高指定为MATCH_PARENT，则h等于栈高度
                h = ph;
            } else if (inSizeCompatMode) {//兼容模式
                h = (int)(mRequestedHeight * mGlobalScale + .5f);
            } else {
                //否则h等于分屏应用请求的高度
                h = mRequestedHeight;
            }
          
        }
        if (inSizeCompatMode) {
            //兼容模式
            x = mAttrs.x * mGlobalScale;
            y = mAttrs.y * mGlobalScale;
        } else {
            x = mAttrs.x;
            y = mAttrs.y;
        }
        //非占满父容器，并且非子窗口
        if (inNonFullscreenContainer && !layoutInParentFrame()) {
            //这里是确保窗口的宽高是合理的，对于Activity类型窗口，其最大宽高只能等于所在栈的宽高
            w = Math.min(w, pw);
            h = Math.min(h, ph);
        }

        // 给mFrame赋值，这里会考虑当前窗口的gravity，x，y的位置，Margin来最终计算mFrame
        Gravity.apply(mAttrs.gravity, w, h, containingFrame,
                (int) (x + mAttrs.horizontalMargin * pw),
                (int) (y + mAttrs.verticalMargin * ph), mWindowFrames.mFrame);

        if (fitToDisplay) {
            //这里是确定计算出来的窗口尺寸在屏幕区域之内
            Gravity.applyDisplay(mAttrs.gravity, displayFrame, mWindowFrames.mFrame);
        }

      //给mCompatFrame设置mFrame同样的值
        mWindowFrames.mCompatFrame.set(mWindowFrames.mFrame);
        if (inSizeCompatMode) {
           
            mWindowFrames.mCompatFrame.scale(mInvGlobalScale);
        }
    }
```

窗口的尺寸计算到此就完成了，最终的结果是保存在`mFrame`中，最后这个值会返回给APP进程，APP进程`ViewRootImpl`中调用的`relayoutWindow`方法主要目的就是请求WMS对窗口进行计算得到`mFrame`的值，最后APP将此值保存在了`ViewRootImpl`的成员变量`mWinFrame`中。

总结，窗口尺寸的计算依赖应用进程和WMS协同，应用进程首次添加到WMS时，WMS返回一个不保证正确的尺寸值（这个尺寸对于Activity窗口来说一般为其栈的边界，非Activity窗口一般为屏幕尺寸），应用进程用这个尺寸对自己的根View进行测量，测量完成之后通过`WMS.relayout`对应用请求的尺寸进行再次计算，因为WMS这边要考虑屏幕尺寸，状态栏，导航栏等，所以WMS这一步计算是保证应用请求的尺寸是合理的。