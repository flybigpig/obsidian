## 1.简介

-   默认的launcher就是launcher3这个app了，手机启动以后自动启动的app，就是我们常说的桌面。
-   点击home键会返回桌面app，如果手机上装有多个桌面app，那么点击home键会提示让你选择一个。

### 1.1.launcher.xml

布局简单分析下

```
    <com.android.launcher3.LauncherRootView
        android:id="@+id/launcher"
        android:layout_width="match_parent"
        android:layout_height="match_parent"
        android:fitsSystemWindows="true">

        <com.android.launcher3.dragndrop.DragLayer
            android:id="@+id/drag_layer"
            android:layout_width="match_parent"
            android:layout_height="match_parent"
            android:clipChildren="false"
            android:clipToPadding="false"
            android:importantForAccessibility="no">

            <com.android.launcher3.views.AccessibilityActionsView
                android:layout_width="match_parent"
                android:layout_height="match_parent"
                android:contentDescription="@string/home_screen"
                />

            <!-- The workspace contains 5 screens of cells -->
            <!-- 左右滑动的控件 DO NOT CHANGE THE ID -->
            <com.android.launcher3.Workspace
                android:id="@+id/workspace"
                android:layout_width="match_parent"
                android:layout_height="match_parent"
                android:layout_gravity="center"
                android:theme="@style/HomeScreenElementTheme"
                launcher:pageIndicator="@+id/page_indicator" />

            <!-- home页底部那几个快捷方式，DO NOT CHANGE THE ID -->
            <include
                android:id="@+id/hotseat"
                layout="@layout/hotseat" />

            <!-- 对应workspace的指示器，只有一页的话不显示。Keep these behind the workspace so that they are not visible when
             we go into AllApps -->
            <com.android.launcher3.pageindicators.WorkspacePageIndicator
                android:id="@+id/page_indicator"
                android:layout_width="match_parent"
                android:layout_height="@dimen/workspace_page_indicator_height"
                android:layout_gravity="bottom|center_horizontal"
                android:theme="@style/HomeScreenElementTheme" />

            <!-- 这个是长按app快捷方式的时候，屏幕顶部出现的操作按钮，取消,卸载等按钮-->
            <include
                android:id="@+id/drop_target_bar"
                layout="@layout/drop_target_bar" />

            <com.android.launcher3.views.ScrimView
                android:layout_width="match_parent"
                android:layout_height="match_parent"
                android:id="@+id/scrim_view"
                android:background="@android:color/transparent" />

            <!--这个是手势上划以后看到的页面，就是一个搜索框，下边是所有已安装的app-->
            <include
                android:id="@+id/apps_view"
                layout="@layout/all_apps"
                android:layout_width="match_parent"
                android:layout_height="match_parent" />
            <!--用来显示recents内容的,具体布局在quickStep目录下重写了-->
            <include
                android:id="@+id/overview_panel"
                layout="@layout/overview_panel" />

        </com.android.launcher3.dragndrop.DragLayer>

    </com.android.launcher3.LauncherRootView>
```

下图:1是id/workspace ，2是id/page\_indicator ，3是id/hotseat ![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/13fe7d2e71524a2893642c39ce77d324~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=337&h=689&s=380630&e=png&b=050116)

下图是布局里的id://apps\_views ![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/6ecf1b4046524bb6bd897596b06af90e~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=337&h=510&s=54218&e=png&b=f1eff3)

#### \>1.hotseat.xml

```
    <com.android.launcher3.Hotseat
        android:id="@+id/hotseat"
        android:layout_width="match_parent"
        android:layout_height="match_parent"
        android:theme="@style/HomeScreenElementTheme"
        android:importantForAccessibility="no"
        android:preferKeepClear="true"
        launcher:containerType="hotseat" />
```

#### \>2.all\_apps.xml

```
<com.android.launcher3.allapps.LauncherAllAppsContainerView 
    android:id="@+id/apps_view"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:clipChildren="true"
    android:clipToPadding="false"
    android:focusable="false"
    android:saveEnabled="false" />
```

#### \>3.drop\_target\_bar.xml

```
<com.android.launcher3.DropTargetBar 
    android:layout_width="match_parent"
    android:layout_height="@dimen/dynamic_grid_drop_target_size"
    android:layout_gravity="center_horizontal|top"
    android:focusable="false"
    android:alpha="0"
    android:theme="@style/HomeScreenElementTheme"
    android:visibility="invisible">

    <!-- Delete target -->
    <com.android.launcher3.DeleteDropTarget
        android:id="@+id/delete_target_text"
        style="@style/DropTargetButton"
        android:layout_width="wrap_content"
        android:layout_height="wrap_content"
        android:layout_gravity="center"
        android:gravity="center"
        android:text="@string/remove_drop_target_label" />

    <!-- Uninstall target -->
    <com.android.launcher3.SecondaryDropTarget
        android:id="@+id/uninstall_target_text"
        style="@style/DropTargetButton"
        android:layout_width="wrap_content"
        android:layout_height="wrap_content"
        android:layout_gravity="center"
        android:gravity="center"
        android:text="@string/uninstall_drop_target_label" />

</com.android.launcher3.DropTargetBar>
```

## 2.DeviceProfile.java

DeviceProfile是通过Builder来创建的，如下

```
    public static class Builder {
    
        public DeviceProfile build() {
//...
            return new DeviceProfile(mContext, mInv, mInfo, mWindowBounds, mDotRendererCache,
                    mIsMultiWindowMode, mTransposeLayoutWithOrientation, mIsMultiDisplay,
                    mIsGestureMode, mViewScaleProvider);
        }
```

### 2.1.构造方法

-   inlineNavButtonsEndSpacing读取的是device\_profile.xml里的值，没有的话用的是默认值 taskbar\_button\_margin\_default=48dp
-   hotseatBarEndOffset就是3个导航按钮占用的位置,包括其他间隔

```
//taskbar显示，非手势模式，非手机
        if (areNavButtonsInline && !isPhone) {
            inlineNavButtonsEndSpacing = res.getDimensionPixelSize(inv.inlineNavButtonsEndSpacing);
            /*
             * 3 nav buttons +
             * Spacing between nav buttons +
             * Space at the end for contextual buttons
             */
            hotseatBarEndOffset = 3 * res.getDimensionPixelSize(R.dimen.taskbar_nav_buttons_size)
                    + 2 * res.getDimensionPixelSize(R.dimen.taskbar_button_space_inbetween)
                    + inlineNavButtonsEndSpacing;
        } else {
            inlineNavButtonsEndSpacing = 0;
            hotseatBarEndOffset = 0;
        }
```

### 2.2.updateWorkspacePadding

```
    private void updateWorkspacePadding() {
        Rect padding = workspacePadding;
        if (isVerticalBarLayout()) {
        //这个是hotseat显示在两边的，不看
            padding.top = 0;
            padding.bottom = edgeMarginPx;
            if (isSeascape()) {
                padding.left = hotseatBarSizePx;
                padding.right = hotseatBarSidePaddingStartPx;
            } else {
                padding.left = hotseatBarSidePaddingStartPx;
                padding.right = hotseatBarSizePx;
            }
        } else {
            // Pad the bottom of the workspace with hotseat bar
            // and leave a bit of space in case a widget go all the way down
            //底部的margin就是 hotseat的高度 + workspace的底部padding + 指示器的高度
            int paddingBottom = hotseatBarSizePx + workspaceBottomPadding
                    + workspacePageIndicatorHeight - mWorkspacePageIndicatorOverlapWorkspace
                    - mInsets.bottom;
            int paddingTop = workspaceTopPadding + (isScalableGrid ? 0 : edgeMarginPx);
            int paddingSide = desiredWorkspaceHorizontalMarginPx;

            padding.set(paddingSide, paddingTop, paddingSide, paddingBottom);
        }
        insetPadding(workspacePadding, cellLayoutPaddingPx);
    }
```

### 2.3.getHotseatLayoutPadding

计算hotseat容器的4个padding

```
    public Rect getHotseatLayoutPadding(Context context) {
        Rect hotseatBarPadding = new Rect();
        if (isVerticalBarLayout()) {
            //两边显示的，不看，平板一般不是
        } else if (isTaskbarPresent) {//3个导航按钮显示的时候
            // Center the QSB vertically with hotseat
            int hotseatBarBottomPadding = getHotseatBarBottomPadding();
            int hotseatBarTopPadding =
                    hotseatBarSizePx - hotseatBarBottomPadding - hotseatCellHeightPx;
            //获取hotseat需要的高度，见补充1
            int hotseatWidth = getHotseatRequiredWidth();
            //availableWidthPx正常就是屏幕的宽，这里取差值一半，就是说hotseat默认居中显示
            int leftSpacing = (availableWidthPx - hotseatWidth) / 2;
            int rightSpacing = leftSpacing;
            // Hotseat aligns to the left with nav buttons
            //hotseatBarEndOffset就是右侧3个导航按钮要占用的大小，数据见补充5
            if (hotseatBarEndOffset > 0) {
                leftSpacing = inlineNavButtonsEndSpacing;
                //居左显示的，宽度减去左边界，hotseat占用大小，最后
                rightSpacing = availableWidthPx - hotseatWidth - leftSpacing + hotseatBorderSpace;
            }

            hotseatBarPadding.set(leftSpacing, hotseatBarTopPadding, rightSpacing,
                    hotseatBarBottomPadding);

            boolean isRtl = Utilities.isRtl(context.getResources());
            if (isRtl) {
                hotseatBarPadding.right += getAdditionalQsbSpace();
            } else {
                hotseatBarPadding.left += getAdditionalQsbSpace();
            }
        } else if (isScalableGrid) {
            int sideSpacing = (availableWidthPx - hotseatQsbWidth) / 2;
            hotseatBarPadding.set(sideSpacing,
                    0,
                    sideSpacing,
                    getHotseatBarBottomPadding());
        } else {
            // We want the edges of the hotseat to line up with the edges of the workspace, but the
            // icons in the hotseat are a different size, and so don't line up perfectly. To account
            // for this, we pad the left and right of the hotseat with half of the difference of a
            // workspace cell vs a hotseat cell.
            float workspaceCellWidth = (float) widthPx / inv.numColumns;
            float hotseatCellWidth = (float) widthPx / numShownHotseatIcons;
            int hotseatAdjustment = Math.round((workspaceCellWidth - hotseatCellWidth) / 2);
            hotseatBarPadding.set(
                    hotseatAdjustment + workspacePadding.left + cellLayoutPaddingPx.left
                            + mInsets.left,
                    0,
                    hotseatAdjustment + workspacePadding.right + cellLayoutPaddingPx.right
                            + mInsets.right,
                    getHotseatBarBottomPadding());
        }
        return hotseatBarPadding;
    }
```

#### \>1..getHotseatRequiredWidth

```
    private int getHotseatRequiredWidth() {
    //hotseat里显示qsb（device_profiles.xml配置的）的时候才有值，我们没有这个，后续都无视
        int additionalQsbSpace = getAdditionalQsbSpace();
        return iconSizePx * numShownHotseatIcons
                + hotseatBorderSpace * (numShownHotseatIcons - (areNavButtonsInline ? 0 : 1))
                + additionalQsbSpace;
    }
```

### 2.4.calculateHotseatBorderSpace

计算hotseat里icon之间的间隔

```
    private int calculateHotseatBorderSpace(float hotseatWidthPx, int numExtraBorder) {
    //icon占用的总大小
        float hotseatIconsTotalPx = iconSizePx * numShownHotseatIcons;
        //总尺寸减去icon的大小，除以间隔数
        int hotseatBorderSpace =
                (int) (hotseatWidthPx - hotseatIconsTotalPx)
                        / (numShownHotseatIcons - 1 + numExtraBorder);
        return Math.min(hotseatBorderSpace, maxHotseatIconSpacePx);
    }
```

### 2.5.InvariantDeviceProfile.java

```
    private void initGrid(Context context, Info displayInfo, DisplayOption displayOption,
            @DeviceType int deviceType) {
    //...
    for (WindowBounds bounds : displayInfo.supportedBounds) {
        localSupportedProfiles.add(new DeviceProfile.Builder(context, this, displayInfo)
                .setIsMultiDisplay(deviceType == TYPE_MULTI_DISPLAY)
                .setWindowBounds(bounds)
                .setDotRendererCache(dotRendererCache)
                .build());
```

这里的bounds有3种，根据结果猜测应该是竖屏加两种横屏，模拟器日志如下

```
//bounds and inset
Rect(0, 0 - 1440, 2960)==Rect(0, 84 - 0, 168)
Rect(0, 0 - 2960, 1440)==Rect(0, 84 - 168, 0)
Rect(0, 0 - 2960, 1440)==Rect(168, 84 - 0, 0)
```

#### \>1.invDistWeightedInterpolate

-   bounds.availableSize ,这个东西和原本的bounds有个inset的区别。
-   y值有个状态栏的差异区别，所以最终的width和height是差一点的。
-   比如设备宽600px，高1000px，平板模式，最终minWidthPx可能是600px，minHeightPx可能是590px

```
    private static DisplayOption invDistWeightedInterpolate(
            Info displayInfo, ArrayList<DisplayOption> points, @DeviceType int deviceType) {
        int minWidthPx = Integer.MAX_VALUE;
        int minHeightPx = Integer.MAX_VALUE;
        for (WindowBounds bounds : displayInfo.supportedBounds) {
            boolean isTablet = displayInfo.isTablet(bounds);
            if (isTablet && deviceType == TYPE_MULTI_DISPLAY) {
                // For split displays, take half width per page
                minWidthPx = Math.min(minWidthPx, bounds.availableSize.x / 2);
                minHeightPx = Math.min(minHeightPx, bounds.availableSize.y);

            } else if (!isTablet && bounds.isLandscape()) {
                // We will use transposed layout in this case
                minWidthPx = Math.min(minWidthPx, bounds.availableSize.y);
                minHeightPx = Math.min(minHeightPx, bounds.availableSize.x);
            } else {
                minWidthPx = Math.min(minWidthPx, bounds.availableSize.x);
                minHeightPx = Math.min(minHeightPx, bounds.availableSize.y);
            }
        }

```

1200\*1920的设备平板模式日志：

```
# bounds.availableSize的大小如下
Point(1200, 1872)
Point(1200, 1728)
Point(1920, 1152)
## 最终minWidthPx和minHeightPx的值
1200/1152
```

1200\*1920的设备手机模式日志：（就是把launcher里isTablet的判断方法返回false）

```
Point(1200, 1872)
Point(1920, 1152)
Point(1776, 1152)
## 最终minWidthPx和minHeightPx的值
1152/1776
```

## 3.View

### 3.1.CellLayout

#### \>1.构造方法

构造方法里默认添加了一个自定义的ShortcutAndWidgetContainer容器(我们的图标其实最终都是添加到这个容器里的)

```
public class CellLayout extends ViewGroup {
//...
public @interface ContainerType{}
public static final int WORKSPACE = 0;
public static final int HOTSEAT = 1;
public static final int FOLDER = 2;

    public CellLayout(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.CellLayout, defStyle, 0);
        //容器的类型，有3种，默认是workspace
        mContainerType = a.getInteger(R.styleable.CellLayout_containerType, WORKSPACE);
        a.recycle();

        // A ViewGroup usually does not draw, but CellLayout needs to draw a rectangle to show
        // the user where a dragged item will land when dropped.
        setWillNotDraw(false);
        setClipToPadding(false);
        mActivity = ActivityContext.lookupContext(context);
        DeviceProfile deviceProfile = mActivity.getDeviceProfile();

        resetCellSizeInternal(deviceProfile);
    //每页分割的行数和列数
        mCountX = deviceProfile.inv.numColumns;
        mCountY = deviceProfile.inv.numRows;
        mOccupied =  new GridOccupancy(mCountX, mCountY);
        mTmpOccupied = new GridOccupancy(mCountX, mCountY);

        mFolderLeaveBehind.mDelegateCellX = -1;
        mFolderLeaveBehind.mDelegateCellY = -1;

        setAlwaysDrawnWithCacheEnabled(false);

        Resources res = getResources();

        mBackground = getContext().getDrawable(R.drawable.bg_celllayout);
        mBackground.setCallback(this);
        mBackground.setAlpha(0);
    //就是长按图标的时候，图标底部会出现一个椭圆的边框，就是那个颜色
        mGridColor = Themes.getAttrColor(getContext(), R.attr.workspaceAccentColor);
        mGridVisualizationRoundingRadius =
                res.getDimensionPixelSize(R.dimen.grid_visualization_rounding_radius);
        mReorderPreviewAnimationMagnitude = (REORDER_PREVIEW_MAGNITUDE * deviceProfile.iconSizePx);

        // Initialize the data structures used for the drag visualization.
        mEaseOutInterpolator = Interpolators.DEACCEL_2_5; // Quint ease out
        mDragCell[0] = mDragCell[1] = -1;
        mDragCellSpan[0] = mDragCellSpan[1] = -1;
        for (int i = 0; i < mDragOutlines.length; i++) {
            mDragOutlines[i] = new CellLayoutLayoutParams(0, 0, 0, 0, -1);
        }
        mDragOutlinePaint.setColor(Themes.getAttrColor(context, R.attr.workspaceTextColor));
    //...

        mShortcutsAndWidgets = new ShortcutAndWidgetContainer(context, mContainerType);
        mShortcutsAndWidgets.setCellDimensions(mCellWidth, mCellHeight, mCountX, mCountY,
                mBorderSpace);
      //默认添加了一个容器
        addView(mShortcutsAndWidgets);
    }
```

#### \>2.resetCellSize

```
    public void resetCellSize(DeviceProfile deviceProfile) {
        resetCellSizeInternal(deviceProfile);//见补充3
        requestLayout();
    }
```

#### \>3.resetCellSizeInternal

```
    private void resetCellSizeInternal(DeviceProfile deviceProfile) {
    //根据3种容器类型，加载不同的borderSpace
        switch (mContainerType) {
            case FOLDER:
                mBorderSpace = new Point(deviceProfile.folderCellLayoutBorderSpacePx,
                        deviceProfile.folderCellLayoutBorderSpacePx);
                break;
            case HOTSEAT:
                mBorderSpace = new Point(deviceProfile.hotseatBorderSpace,
                        deviceProfile.hotseatBorderSpace);
                break;
            case WORKSPACE:
            default:
                mBorderSpace = new Point(deviceProfile.cellLayoutBorderSpacePx);
                break;
        }

        mCellWidth = mCellHeight = -1;
        mFixedCellWidth = mFixedCellHeight = -1;
    }
```

#### \>4.setGridSize

设置网格大小

```
    public void setGridSize(int x, int y) {
        mCountX = x;
        mCountY = y;
        mOccupied = new GridOccupancy(mCountX, mCountY);
        mTmpOccupied = new GridOccupancy(mCountX, mCountY);
        //最终都设置给构造方法里添加的这个容器里了。毕竟最终的child都是添加到它里边。
        mShortcutsAndWidgets.setCellDimensions(mCellWidth, mCellHeight, mCountX, mCountY,
                mBorderSpace);
        requestLayout();
    }
```

#### \>5.addViewToCellLayout

外部获取数据以后会调用。

```
    public boolean addViewToCellLayout(View child, int index, int childId,
            CellLayoutLayoutParams params, boolean markCells) {
        final CellLayoutLayoutParams lp = params;

        // Hotseat icons - remove text
        if (child instanceof BubbleTextView) {
            BubbleTextView bubbleChild = (BubbleTextView) child;
            //hotseat不显示文字的，只显示图标
            bubbleChild.setTextVisibility(mContainerType != HOTSEAT);
        }

        child.setScaleX(mChildScale);
        child.setScaleY(mChildScale);

        //网格索引在有效范围里 
        if (lp.cellX >= 0 && lp.cellX <= mCountX - 1 && lp.cellY >= 0 && lp.cellY <= mCountY - 1) {
            //如果跨度是负的，那么就认为是铺满的，也就是最大网格数
            if (lp.cellHSpan < 0) lp.cellHSpan = mCountX;
            if (lp.cellVSpan < 0) lp.cellVSpan = mCountY;

            child.setId(childId);
            //可以看到，最终添加到这个容器里了
            mShortcutsAndWidgets.addView(child, index, lp);

            if (markCells) markCellsAsOccupiedForView(child);

            return true;
        }
        return false;
    }
```

### 3.2.Hotseat.java

```
public class Hotseat extends CellLayout implements Insettable {
//...
    public Hotseat(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
//这个应该是以前用的，以前的qsb是在hotseat里，现在的是在顶部显示的，所以这个可以不关注，宽高都是0
        mQsb = LayoutInflater.from(context).inflate(R.layout.search_container_hotseat, this, false);
        addView(mQsb);
    }
    
```

#### \>1.resetLayout

```
        public void resetLayout(boolean hasVerticalHotseat) {
            removeAllViewsInLayout();
            mHasVerticalHotseat = hasVerticalHotseat;
            DeviceProfile dp = mActivity.getDeviceProfile();
            //见3.1.2，主要是重新设置了border space
            resetCellSize(dp);
            //设置几行几列
            if (hasVerticalHotseat) {//垂直方向的hotseat是左右两边显示的，所以宽是1，高度是number
                setGridSize(1, dp.numShownHotseatIcons);
            } else {//水平方向的，底部显示的，高度固定1
            //见3.1.4
                setGridSize(dp.numShownHotseatIcons, 1);
            }
        }
```

#### \>2.setInsets

设置padding

```
        public void setInsets(Rect insets) {
            FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) getLayoutParams();
            DeviceProfile grid = mActivity.getDeviceProfile();

            if (grid.isVerticalBarLayout()) {
            //左右两侧显示的，高度铺满，宽度限定，根据seascape决定显示在左边还是右边
                mQsb.setVisibility(View.GONE);
                lp.height = ViewGroup.LayoutParams.MATCH_PARENT;
                if (grid.isSeascape()) {
                    lp.gravity = Gravity.LEFT;
                    lp.width = grid.hotseatBarSizePx + insets.left;
                } else {
                    lp.gravity = Gravity.RIGHT;
                    lp.width = grid.hotseatBarSizePx + insets.right;
                }
            } else {
            //底部显示，宽度铺满，高度限定
                mQsb.setVisibility(View.VISIBLE);
                lp.gravity = Gravity.BOTTOM;
                lp.width = ViewGroup.LayoutParams.MATCH_PARENT;
                lp.height = grid.hotseatBarSizePx;
            }
            //计算padding,见2.2.3
            Rect padding = grid.getHotseatLayoutPadding(getContext());
            //设置padding，毕竟它是match_parent的，设置了padding就相当于决定了child显示的位置
            setPadding(padding.left, padding.top, padding.right, padding.bottom);
            setLayoutParams(lp);
            InsettableFrameLayout.dispatchInsets(this, insets);
        }
```

#### \>3.onInterceptTouchEvent

```
    public boolean onInterceptTouchEvent(MotionEvent ev) {
    //触摸事件交给workspace处理了
        int yThreshold = getMeasuredHeight() - getPaddingBottom();
        if (mWorkspace != null && ev.getY() <= yThreshold) {
            mSendTouchToWorkspace = mWorkspace.onInterceptTouchEvent(ev);
            return mSendTouchToWorkspace;
        }
        return false;
    }
```

### 3.3.WorkspacePageIndicator.java

与workspace翻页对应的一个自定义指示器，不论横屏还是竖屏，都是在底部的，当然了，竖屏的时候是在hotseat上边的。

这里主要说明的是，workspace里有个方法【setPageIndicatorInset()】，设置了这个indicator的margin。

```
public class WorkspacePageIndicator extends View implements Insettable, PageIndicator {
//
    public WorkspacePageIndicator(Context context, AttributeSet attrs, int defStyle) {
    super(context, attrs, defStyle);

    Resources res = context.getResources();
    mLinePaint = new Paint();
    mLinePaint.setAlpha(0);

    mLauncher = Launcher.getLauncher(context);
    mLineHeight = res.getDimensionPixelSize(R.dimen.workspace_page_indicator_line_height);

    boolean darkText = Themes.getAttrBoolean(mLauncher, R.attr.isWorkspaceDarkText);
    mActiveAlpha = darkText ? BLACK_ALPHA : WHITE_ALPHA;
    mLinePaint.setColor(darkText ? Color.BLACK : Color.WHITE);
}
protected void onDraw(Canvas canvas) {
    if (mTotalScroll == 0 || mNumPagesFloat == 0) {
        return;
    }
//根据当前所在的页面，画条钱
    canvas.drawRoundRect(lineLeft, getHeight() / 2 - mLineHeight / 2, lineRight,
            getHeight() / 2 + mLineHeight / 2, mLineHeight, mLineHeight, mLinePaint);
}

//自动隐藏
    public void setShouldAutoHide(boolean shouldAutoHide) {
    mShouldAutoHide = shouldAutoHide;
    if (shouldAutoHide && mLinePaint.getAlpha() > 0) {
        hideAfterDelay();
    } else if (!shouldAutoHide) {
        mDelayedLineFadeHandler.removeCallbacksAndMessages(null);
    }
}
```

### 3.4.Workspace.java

-   区域很大的，带壁纸显示的，有限数量的页面，每个页面都包含一些图标，文件夹，小组件与用户交互
-   里边的child是CellLayout

```
    public class Workspace<T extends View & PageIndicator> extends PagedView<T>
            implements DropTarget, DragSource, View.OnTouchListener,
            DragController.DragListener, Insettable, StateHandler<LauncherState>,
            WorkspaceLayoutManager, LauncherBindableItemsContainer, LauncherOverlayCallbacks {
       //...

        public Workspace(Context context, AttributeSet attrs, int defStyle) {
            super(context, attrs, defStyle);

            mLauncher = Launcher.getLauncher(context);
            mStateTransitionAnimation = new WorkspaceStateTransitionAnimation(mLauncher, this);
            mWallpaperManager = WallpaperManager.getInstance(context);
            mAllAppsIconSize = mLauncher.getDeviceProfile().allAppsIconSizePx;
            mWallpaperOffset = new WallpaperOffsetInterpolator(this);

            setHapticFeedbackEnabled(false);
            initWorkspace();

            // Disable multitouch across the workspace/all apps/customize tray
            setMotionEventSplittingEnabled(true);
            //触摸事件的处理
            setOnTouchListener(new WorkspaceTouchListener(mLauncher, this));
            mStatsLogManager = StatsLogManager.newInstance(context);
        }
```

#### \>1.setInsets

-   从布局看，Workspace控件是铺满全屏的，可实际效果，很明显，距离底部有一段距离。
-   WorkspacePageIndicator也是，布局上看，它就是底部居中的，可实际上的位置，明显和底部有一段距离。
-   查看了下这两控件的onlayout方法，并未进行处理，这里处理的

```
    public void setInsets(Rect insets) {
        DeviceProfile grid = mLauncher.getDeviceProfile();

        mWorkspaceFadeInAdjacentScreens = grid.shouldFadeAdjacentWorkspaceScreens();
        //数据来源见2.1.2
        Rect padding = grid.workspacePadding;
        //设置padding
        setPadding(padding.left, padding.top, padding.right, padding.bottom);
        mInsets.set(insets);

        if (mWorkspaceFadeInAdjacentScreens) {
            // In landscape mode the page spacing is set to the default.
            setPageSpacing(grid.edgeMarginPx);
        } else {
            // In portrait, we want the pages spaced such that there is no
            // overhang of the previous / next page into the current page viewport.
            // We assume symmetrical padding in portrait mode.
            int maxInsets = Math.max(insets.left, insets.right);
            int maxPadding = Math.max(grid.edgeMarginPx, padding.left + 1);
            setPageSpacing(Math.max(maxInsets, maxPadding));
        }
        //celllayout就是workspace里的child了，设置下padding
        updateCellLayoutPadding();
        //设置里边的widget的大小，又是套在ShortcutAndWidgetContainer容器里的child
        updateWorkspaceWidgetsSizes();
        //给indicator设置margin，确保竖屏的话indicator在hotseat上边，横屏的话在右边或者左边
        setPageIndicatorInset();
    }

        //这个是给indicator设置margin
            private void setPageIndicatorInset() {
                DeviceProfile grid = mLauncher.getDeviceProfile();

                FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) mPageIndicator.getLayoutParams();

                // Set insets for page indicator
                Rect padding = grid.workspacePadding;
                if (grid.isVerticalBarLayout()) {
                    lp.leftMargin = padding.left + grid.workspaceCellPaddingXPx;
                    lp.rightMargin = padding.right + grid.workspaceCellPaddingXPx;
                    lp.bottomMargin = padding.bottom;
                } else {
                //我们平板走这里，底部显示的
                    lp.leftMargin = lp.rightMargin = 0;
                    lp.gravity = Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM;
                    //底部margin就是hotseat的高度
                    lp.bottomMargin = grid.hotseatBarSizePx;
                }
                //Workspace的布局里有指明indicator的id，所以这里拿到了对应的view
                mPageIndicator.setLayoutParams(lp);
            }
```

#### \>2.drag start/end

```
    public void onDragStart(DragObject dragObject, DragOptions options) {

        if (mDragInfo != null && mDragInfo.cell != null) {
            CellLayout layout = (CellLayout) (mDragInfo.cell instanceof LauncherAppWidgetHostView
                    ? dragObject.dragView.getContentViewParent().getParent()
                    : mDragInfo.cell.getParent().getParent());
            layout.markCellsAsUnoccupiedForView(mDragInfo.cell);
        }

        updateChildrenLayersEnabled();
//判断下是否需要添加新的页面 
        boolean addNewPage = !(options.isAccessibleDrag && dragObject.dragSource != this);
        if (addNewPage) {
            mDeferRemoveExtraEmptyScreen = false;
            //添加一个空白页面
            addExtraEmptyScreenOnDrag(dragObject);

            if (dragObject.dragInfo.itemType == LauncherSettings.Favorites.ITEM_TYPE_APPWIDGET
                    && dragObject.dragSource != this) {
                int currentPage = getDestinationPage();
                for (int pageIndex = currentPage; pageIndex < getPageCount(); pageIndex++) {
                    CellLayout page = (CellLayout) getPageAt(pageIndex);
                    if (page.hasReorderSolution(dragObject.dragInfo)) {
                        setCurrentPage(pageIndex);
                        break;
                    }
                }
            }
        }

        // Always enter the spring loaded mode
        mLauncher.getStateManager().goToState(SPRING_LOADED);

    }

public void onDragEnd() {
    updateChildrenLayersEnabled();
    StateManager<LauncherState> stateManager = mLauncher.getStateManager();
    stateManager.addStateListener(new StateManager.StateListener<LauncherState>() {
        @Override
        public void onStateTransitionComplete(LauncherState finalState) {
            if (finalState == NORMAL) {
                if (!mDeferRemoveExtraEmptyScreen) {
                    removeExtraEmptyScreen(true /* stripEmptyScreens */);
                }
                stateManager.removeStateListener(this);
            }
        }
    });

    mDragInfo = null;
    mDragSourceInternal = null;
}
```

#### \>3.onViewAdded

```
public void onViewAdded(View child) {
//可以看到，只能添加CellLayout
    if (!(child instanceof CellLayout)) {
        throw new IllegalArgumentException("A Workspace can only have CellLayout children.");
    }
    CellLayout cl = ((CellLayout) child);
    cl.setOnInterceptTouchListener(this);//child是否拦截触摸事件，由父类来处理
    cl.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
    super.onViewAdded(child);
}
```

#### \>4.bindAndInitFirstWorkspaceScreen

一个是launcher类里调用，一个是下边removeAll的时候调用

```
public void bindAndInitFirstWorkspaceScreen() {
    if (!FeatureFlags.QSB_ON_FIRST_SCREEN) {
        return;
    }

    // 添加第一个页面
    CellLayout firstPage = insertNewWorkspaceScreen(Workspace.FIRST_SCREEN_ID, getChildCount());
    // Always add a first page pinned widget on the first screen.
    if (mFirstPagePinnedItem == null) {
    //这个就是那个顶部的search bar
        mFirstPagePinnedItem = LayoutInflater.from(getContext())
                .inflate(R.layout.search_container_workspace, firstPage, false);
    }

    int cellHSpan = mLauncher.getDeviceProfile().inv.numSearchContainerColumns;
    CellLayoutLayoutParams lp = new CellLayoutLayoutParams(0, 0, cellHSpan, 1, FIRST_SCREEN_ID);
    lp.canReorder = false;
    //把search bar 添加到第一个cellLayout页面里去
    if (!firstPage.addViewToCellLayout(
            mFirstPagePinnedItem, 0, R.id.search_container_workspace, lp, true)) {
        Log.e(TAG, "Failed to add to item at (0, 0) to CellLayout");
        mFirstPagePinnedItem = null;
    }
}
```

##### +search\_container\_workspace.xml

```
<com.android.launcher3.qsb.QsbContainerView
        xmlns:android="http://schemas.android.com/apk/res/android"
        android:orientation="vertical"
        android:layout_width="match_parent"
        android:layout_height="0dp"
        android:id="@id/search_container_workspace"
        android:padding="0dp" >

    <fragment
        android:name="com.android.launcher3.qsb.QsbContainerView$QsbFragment"
        android:layout_width="match_parent"
        android:tag="qsb_view"
        android:layout_height="match_parent"/>
</com.android.launcher3.qsb.QsbContainerView>
```

#### \>5.insertNewWorkspaceScreen

```
    public CellLayout insertNewWorkspaceScreen(int screenId, int insertIndex) {

        CellLayout newScreen = (CellLayout) LayoutInflater.from(getContext()).inflate(
                        R.layout.workspace_screen, this, false /* attachToRoot */);

        mWorkspaceScreens.put(screenId, newScreen);
        mScreenOrder.add(insertIndex, screenId);
        addView(newScreen, insertIndex);//添加到workspace里
        mStateTransitionAnimation.applyChildState(
                mLauncher.getStateManager().getState(), newScreen, insertIndex);

        updatePageScrollValues();
        updateCellLayoutPadding();
        return newScreen;
    }
```

外部调用的是下边的方法

```
    public void insertNewWorkspaceScreenBeforeEmptyScreen(int screenId) {
        // 有空白页的话插在空白页之前，没有的话插在容器末尾
        int insertIndex = mScreenOrder.indexOf(EXTRA_EMPTY_SCREEN_ID);
        if (insertIndex < 0) {
            insertIndex = mScreenOrder.size();
        }
        insertNewWorkspaceScreen(screenId, insertIndex);
    }

    public void insertNewWorkspaceScreen(int screenId) {
    //默认插入在末尾
        insertNewWorkspaceScreen(screenId, getChildCount());
    }
```

#### \>6.removeAllWorkspaceScreens

在launcher里会调用这个方法，主要是清空workspace内容，并初始化一个默认的页面

```
    public void removeAllWorkspaceScreens() {

        disableLayoutTransitions();

        // 移除首页那个search bar
        if (mFirstPagePinnedItem != null) {
            ((ViewGroup) mFirstPagePinnedItem.getParent()).removeView(mFirstPagePinnedItem);
        }

        // 清空view以及集合里保存的引用
        removeFolderListeners();
        removeAllViews();
        mScreenOrder.clear();
        mWorkspaceScreens.clear();

        // Remove any deferred refresh callbacks
        mLauncher.mHandler.removeCallbacksAndMessages(DeferredWidgetRefresh.class);

        // 重新初始化第一个默认的页面
        bindAndInitFirstWorkspaceScreen();

        // Re-enable the layout transitions
        enableLayoutTransitions();
    }
```

##### +workspace\_screen.xml

```
<com.android.launcher3.CellLayout
    xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:launcher="http://schemas.android.com/apk/res-auto"
    android:layout_width="wrap_content"
    android:layout_height="wrap_content"
    android:hapticFeedbackEnabled="false"
    launcher:containerType="workspace" />
```

#### \>7.workspace元素介绍

如下图，有3种，

-   第一种是LauncherAppWidgetHostView，也就是常说的（某个app的）小部件
-   第二种是文件夹FolderIcon，其实拖拽的时候也是当个图标处理的
-   第三种就是app的快捷图标了DoubleShadowBubbleTextView ![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/69209562332a4aed8868ed73d6d1c768~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=332&h=500&s=282748&e=png&b=17161d)

#### \>8.addExtraEmptyScreenOnDrag

处理下拖拽的时候，需不需要添加新的空白页。

变量说明：

-   dragSourceChildCount----开始拖动以后，页面上元素的个数，如果是小部件的话，开始拖动的时候这个小部件就自动从页面移除了，所以拖动小部件的时候这个count会少一，举个列子，如果页面上就只有一个小部件，你拖动以后返回的count就是0了。
    
    ```
      private void addExtraEmptyScreenOnDrag(DragObject dragObject) {
          boolean lastChildOnScreen = false;
          boolean childOnFinalScreen = false;
    
          if (mDragSourceInternal != null) {
          //看上边说明
              int dragSourceChildCount = mDragSourceInternal.getChildCount();
    
              //这玩意判断的是折叠屏吗？ If the icon was dragged from Hotseat, there is no page pair
              if (isTwoPanelEnabled() && !(mDragSourceInternal.getParent() instanceof Hotseat)) {
                  int pagePairScreenId = getScreenPair(dragObject.dragInfo.screenId);
                  CellLayout pagePair = mWorkspaceScreens.get(pagePairScreenId);
                  dragSourceChildCount += pagePair.getShortcutsAndWidgets().getChildCount();
              }
    
    //拖动的是小部件的话，这个count数少一个，这里需要加回来
              if (dragObject.dragView.getContentView() instanceof LauncherAppWidgetHostView) {
                  dragSourceChildCount++;
              }
    
      //说明我们拖动的是页面上的唯一一个元素
              if (dragSourceChildCount == 1) {
                  lastChildOnScreen = true;
              }
              CellLayout cl = (CellLayout) mDragSourceInternal.getParent();
              //这个拖动的图标所在的页面是workspace的最后一个页面
              if (getLeftmostVisiblePageForIndex(indexOfChild(cl))
                      == getLeftmostVisiblePageForIndex(getPageCount() - 1)) {
                  childOnFinalScreen = true;
              }
          }
    
          // 拖动的是最后一个页面的唯一一个元素，那么不用创建新的空白页面
          if (lastChildOnScreen && childOnFinalScreen) {
              return;
          }
          
          forEachExtraEmptyPageId(extraEmptyPageId -> {
          //没有空白页的话，创建一个
              if (!mWorkspaceScreens.containsKey(extraEmptyPageId)) {
                  insertNewWorkspaceScreen(extraEmptyPageId);
              }
          });
      }
    ```
    

```
    private void forEachExtraEmptyPageId(Consumer<Integer> callback) {
        callback.accept(EXTRA_EMPTY_SCREEN_ID);//空屏的id，固定的
        if (isTwoPanelEnabled()) {//双屏的话，再加一个空的
            callback.accept(EXTRA_EMPTY_SCREEN_SECOND_ID);
        }
    }
```

#### \>9.removeExtraEmptyScreen

```
    public void removeExtraEmptyScreen(boolean stripEmptyScreens) {
        removeExtraEmptyScreenDelayed(0, stripEmptyScreens, null);
    }
    
    public void removeExtraEmptyScreenDelayed(
        int delay, boolean stripEmptyScreens, Runnable onComplete) {
    if (mLauncher.isWorkspaceLoading()) {
        // Don't strip empty screens if the workspace is still loading
        return;
    }

    if (delay > 0) {
        postDelayed(
                () -> removeExtraEmptyScreenDelayed(0, stripEmptyScreens, onComplete), delay);
        return;
    }

    // First we convert the last page to an extra page if the last page is empty
    // and we don't already have an extra page.
    convertFinalScreenToEmptyScreenIfNecessary();
    // Then we remove the extra page(s) if they are not the only pages left in Workspace.
    if (hasExtraEmptyScreens()) {
        forEachExtraEmptyPageId(extraEmptyPageId -> {
            removeView(mWorkspaceScreens.get(extraEmptyPageId));
            mWorkspaceScreens.remove(extraEmptyPageId);
            mScreenOrder.removeValue(extraEmptyPageId);
        });

        setCurrentPage(getNextPage());

        // Update the page indicator to reflect the removed page.
        showPageIndicatorAtCurrentScroll();
    }

    if (stripEmptyScreens) {
        // This will remove all empty pages from the Workspace. If there are no more pages left,
        // it will add extra page(s) so that users can put items on at least one page.
        stripEmptyScreens();
    }

    if (onComplete != null) {
        onComplete.run();
    }
}    
```

### 3.5.WorkspaceTouchListener

用来处理workspace的空白区域的点击事件的，主要就是弹一个如下弹框选项

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7907224931504cf4b4cbcf63119156f3~tplv-k3u1fbpfcp-jj-mark:3024:0:0:0:q75.awebp#?w=295&h=257&s=114354&e=png&b=eaebf8)

```

    //处理workspace空白区域的touch事件，并显示一个选项弹框
    public class WorkspaceTouchListener extends GestureDetector.SimpleOnGestureListener
    implements OnTouchListener {
    //...
    private int mLongPressState = STATE\_CANCELLED;

    private final GestureDetector mGestureDetector;//手势监听，这里就处理下长按事件

    public WorkspaceTouchListener(Launcher launcher, Workspace\<?> workspace) {
    //..
    mGestureDetector = new GestureDetector(workspace.getContext(), this);
    }

    @Override
    public boolean onTouch(View view, MotionEvent ev) {
    mGestureDetector.onTouchEvent(ev);//这里就简单的监听了长按事件

         int action = ev.getActionMasked();
         if (action == ACTION_DOWN) {
             // 是否可以处理长按操作
             boolean handleLongPress = canHandleLongPress();

             if (handleLongPress) {
                 // Check if the event is not near the edges
                 DeviceProfile dp = mLauncher.getDeviceProfile();
                 DragLayer dl = mLauncher.getDragLayer();
                 Rect insets = dp.getInsets();

                 mTempRect.set(insets.left, insets.top, dl.getWidth() - insets.right,
                         dl.getHeight() - insets.bottom);
                 mTempRect.inset(dp.edgeMarginPx, dp.edgeMarginPx);
                 //判断下点击是否在可操作范围
                 handleLongPress = mTempRect.contains((int) ev.getX(), (int) ev.getY());
             }

             if (handleLongPress) {
                 mLongPressState = STATE_REQUESTED;//修改状态，后边用到
                 mTouchDownPoint.set(ev.getX(), ev.getY());
                 // Mouse right button's ACTION_DOWN should immediately show menu
                 if (TouchUtil.isMouseRightClickDownOrMove(ev)) {
                     maybeShowMenu();//鼠标右键的话这里就直接弹框了
                     return true;
                 }
             }

             mWorkspace.onTouchEvent(ev);
             // Return true to keep receiving touch events
             return true;
         }

         if (mLongPressState == STATE_PENDING_PARENT_INFORM) {
             // Inform the workspace to cancel touch handling
             ev.setAction(ACTION_CANCEL);
             mWorkspace.onTouchEvent(ev);

             ev.setAction(action);
             mLongPressState = STATE_COMPLETED;
         }

         boolean isInAllAppsBottomSheet = mLauncher.isInState(ALL_APPS)
                 && mLauncher.getDeviceProfile().isTablet;

         final boolean result;
         if (mLongPressState == STATE_COMPLETED) {
             // We have handled the touch, so workspace does not need to know anything anymore.
             result = true;
         } else if (mLongPressState == STATE_REQUESTED) {
             mWorkspace.onTouchEvent(ev);
             //正在拖拽或者移动的话，取消长按事件
             if (mWorkspace.isHandlingTouch()) {
                 cancelLongPress();
             } else if (action == ACTION_MOVE && PointF.length(
                     mTouchDownPoint.x - ev.getX(), mTouchDownPoint.y - ev.getY()) > mTouchSlop) {
                 cancelLongPress();
             }

             result = true;
         } else {
             // We don't want to handle touch unless we're in AllApps bottom sheet, let workspace
             // handle it as usual.
             result = isInAllAppsBottomSheet;
         }

         if (action == ACTION_UP || action == ACTION_POINTER_UP) {
             if (!mWorkspace.isHandlingTouch()) {
                 final CellLayout currentPage =
                         (CellLayout) mWorkspace.getChildAt(mWorkspace.getCurrentPage());
                 if (currentPage != null) {
                 //手指抬起的话，把位置发送给壁纸处理
                     mWorkspace.onWallpaperTap(ev);
                 }
             }
         }

         if (action == ACTION_UP || action == ACTION_CANCEL) {
             cancelLongPress();
         }
         if (action == ACTION_UP && isInAllAppsBottomSheet) {
             mLauncher.getStateManager().goToState(NORMAL);
         }

         return result;

    }
    //是否支持长按事件
    private boolean canHandleLongPress() {
    return AbstractFloatingView\.getTopOpenView(mLauncher) == null
    && mLauncher.isInState(NORMAL);
    }

    private void cancelLongPress() {
    mLongPressState = STATE\_CANCELLED;
    }

    @Override
    public void onLongPress(MotionEvent event) {
    maybeShowMenu(); //手势的长按事件回调
    }

    private void maybeShowMenu() {
    if (mLongPressState == STATE\_REQUESTED) {//这个state前边有分析，在可点击范围即可
    if (canHandleLongPress()) {//再次判断是否可以长按
    mLongPressState = STATE\_PENDING\_PARENT\_INFORM;
    //这里就是显示弹框的操作了
    mLauncher.showDefaultOptions(mTouchDownPoint.x, mTouchDownPoint.y);
    } else {
    cancelLongPress();
    }
    }
    }
    }

```

### 3.6.OptionsPopupView

```
    public void showDefaultOptions(float x, float y) {
        OptionsPopupView.show(this, getPopupTarget(x, y), OptionsPopupView.getOptions(this),
                false);
    }
```

```
    public static OptionsPopupView show(ActivityContext launcher, RectF targetRect,
            List<OptionItem> items, boolean shouldAddArrow, int width) {
            //这玩意就是个线性布局
        OptionsPopupView popup = (OptionsPopupView) launcher.getLayoutInflater()
                .inflate(R.layout.longpress_options_menu, launcher.getDragLayer(), false);
        popup.mTargetRect = targetRect;
        popup.setShouldAddArrow(shouldAddArrow);

        for (OptionItem item : items) {
            DeepShortcutView view =
                    (DeepShortcutView) popup.inflateAndAdd(R.layout.system_shortcut, popup);
            if (width > 0) {
                view.getLayoutParams().width = width;
            }
            view.getIconView().setBackgroundDrawable(item.icon);
            view.getBubbleText().setText(item.label);
            view.setOnClickListener(popup);
            view.setOnLongClickListener(popup);
            popup.mItemMap.put(view, item);
        }

        popup.show();
        return popup;
    }
```

### 3.7.ShortcutAndWidgetContainer

```
   public ShortcutAndWidgetContainer(Context context, @ContainerType int containerType) {
        super(context);
        mActivity = ActivityContext.lookupContext(context);
        mWallpaperManager = WallpaperManager.getInstance(context);
        mContainerType = containerType;
    }

    public void setCellDimensions(int cellWidth, int cellHeight, int countX, int countY,
            Point borderSpace) {
        mCellWidth = cellWidth;
        mCellHeight = cellHeight;
        mCountX = countX;
        mCountY = countY;
        mBorderSpace = borderSpace;
    }
```

#### \>1.getChildAt

根据传进来的索引，对比布局参数，判断点击的是哪个

```
    public View getChildAt(int cellX, int cellY) {
        final int count = getChildCount();
        for (int i = 0; i < count; i++) {
            View child = getChildAt(i);
            CellLayoutLayoutParams lp = (CellLayoutLayoutParams) child.getLayoutParams();

            if ((lp.cellX <= cellX) && (cellX < lp.cellX + lp.cellHSpan)
                    && (lp.cellY <= cellY) && (cellY < lp.cellY + lp.cellVSpan)) {
                return child;
            }
        }
        return null;
    }
```

#### \>2.onMeasure

```
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int count = getChildCount();

        int widthSpecSize = MeasureSpec.getSize(widthMeasureSpec);
        int heightSpecSize = MeasureSpec.getSize(heightMeasureSpec);
        setMeasuredDimension(widthSpecSize, heightSpecSize);

        for (int i = 0; i < count; i++) {
            View child = getChildAt(i);
            if (child.getVisibility() != GONE) {
            //最终测量的是child，见补充3
                measureChild(child);
            }
        }
    }

```

#### \>3.measureChild

主要就是通过lp的setup方法，计算child的x,y位置以及宽高

```
    public void measureChild(View child) {
        CellLayoutLayoutParams lp = (CellLayoutLayoutParams) child.getLayoutParams();
        final DeviceProfile dp = mActivity.getDeviceProfile();

        if (child instanceof NavigableAppWidgetHostView) {
            ((NavigableAppWidgetHostView) child).getWidgetInset(dp, mTempRect);
            final PointF appWidgetScale = dp.getAppWidgetScale((ItemInfo) child.getTag());
            lp.setup(mCellWidth, mCellHeight, invertLayoutHorizontally(), mCountX, mCountY,
                    appWidgetScale.x, appWidgetScale.y, mBorderSpace, mTempRect);
        } else {
        //根据每个网格的大小，以及自己占用的网格大小，计算要显示的尺寸大小
            lp.setup(mCellWidth, mCellHeight, invertLayoutHorizontally(), mCountX, mCountY,
                    mBorderSpace, null);
    //...
            child.setPadding(cellPaddingX, cellPaddingY, cellPaddingX, 0);
        }
        //根据上边计算的宽高，设置尺寸
        int childWidthMeasureSpec = MeasureSpec.makeMeasureSpec(lp.width, MeasureSpec.EXACTLY);
        int childheightMeasureSpec = MeasureSpec.makeMeasureSpec(lp.height, MeasureSpec.EXACTLY);
        child.measure(childWidthMeasureSpec, childheightMeasureSpec);
    }
```

#### \>4.onLayout

```
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        int count = getChildCount();
        for (int i = 0; i < count; i++) {
            final View child = getChildAt(i);
            if (child.getVisibility() != GONE) {
                layoutChild(child);
            }
        }
    }
    
    
public void layoutChild(View child) {
    CellLayoutLayoutParams lp = (CellLayoutLayoutParams) child.getLayoutParams();
    if (child instanceof NavigableAppWidgetHostView) {
        NavigableAppWidgetHostView nahv = (NavigableAppWidgetHostView) child;
    //...
    }

    int childLeft = lp.x;
    int childTop = lp.y;
    //measure方法的时候已经通过lp的setup方法计算了位置以及宽高，这里直接使用即可
    child.layout(childLeft, childTop, childLeft + lp.width, childTop + lp.height);

    //...
}
```

### 3.8.CellLayoutLayoutParams

celllayout类似网格那种，所以这里的数据都是网格的索引

```
public CellLayoutLayoutParams(int cellX, int cellY, int cellHSpan, int cellVSpan,
            int screenId) {
        super(CellLayoutLayoutParams.MATCH_PARENT, CellLayoutLayoutParams.MATCH_PARENT);
        this.cellX = cellX;//水平方向索引
        this.cellY = cellY;//垂直方向索引
        this.cellHSpan = cellHSpan;//水平方向跨度，就是占几个格子
        this.cellVSpan = cellVSpan;//垂直方向跨度
        this.screenId = screenId;//显示在哪个屏幕上，多页的情况下
    }
```

#### \>1.setup

根据网格的索引，每个网格的宽高，以及自己横向和纵向占几个网格，就可以计算出位置宽高了。

```
    public void setup(int cellWidth, int cellHeight, boolean invertHorizontally, int colCount,
            int rowCount, float cellScaleX, float cellScaleY, Point borderSpace,
            @Nullable Rect inset) {
        if (isLockedToGrid) {
            final int myCellHSpan = cellHSpan;
            final int myCellVSpan = cellVSpan;
            int myCellX = useTmpCoords ? tmpCellX : cellX;
            int myCellY = useTmpCoords ? tmpCellY : cellY;

            if (invertHorizontally) {
                myCellX = colCount - myCellX - cellHSpan;
            }

            int hBorderSpacing = (myCellHSpan - 1) * borderSpace.x;
            int vBorderSpacing = (myCellVSpan - 1) * borderSpace.y;

            float myCellWidth = ((myCellHSpan * cellWidth) + hBorderSpacing) / cellScaleX;
            float myCellHeight = ((myCellVSpan * cellHeight) + vBorderSpacing) / cellScaleY;

            width = Math.round(myCellWidth) - leftMargin - rightMargin;
            height = Math.round(myCellHeight) - topMargin - bottomMargin;
            x = leftMargin + (myCellX * cellWidth) + (myCellX * borderSpace.x);
            y = topMargin + (myCellY * cellHeight) + (myCellY * borderSpace.y);

            if (inset != null) {
                x -= inset.left;
                y -= inset.top;
                width += inset.left + inset.right;
                height += inset.top + inset.bottom;
            }
        }
    }
```

### 3.9.FolderIcon

把多个icon图标放到一起就会有个文件夹了

#### \>1.folder\_icon.xml

```
<com.android.launcher3.folder.FolderIcon
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:orientation="vertical"
    android:focusable="true" >
    <com.android.launcher3.views.DoubleShadowBubbleTextView
        style="@style/BaseIcon.Workspace"
        android:id="@+id/folder_icon_name"
        android:focusable="false"
        android:layout_gravity="top"
        android:layout_width="match_parent"
        android:layout_height="match_parent" />
</com.android.launcher3.folder.FolderIcon>
```

#### \>2.init

构造方法里会初始化一些工具类，

```
    public FolderIcon(Context context) {
        super(context);
        init();
    }

    private void init() {
        mLongPressHelper = new CheckLongPressHelper(this);//长按事件帮助类
        mPreviewLayoutRule = new ClippedFolderIconLayoutRule();
        mPreviewItemManager = new PreviewItemManager(this);//预览效果这个来控制
        mDotParams = new DotRenderer.DrawParams();
    }
    
public boolean onTouchEvent(MotionEvent event) {
    if (event.getAction() == MotionEvent.ACTION_DOWN
            && shouldIgnoreTouchDown(event.getX(), event.getY())) {
        return false;
    }

    // Call the superclass onTouchEvent first, because sometimes it changes the state to
    // isPressed() on an ACTION_UP
    super.onTouchEvent(event);
    //触摸事件交给helper类来处理长按事件
    mLongPressHelper.onTouchEvent(event);
    // Keep receiving the rest of the events
    return true;
}
```

看一下helper类里长按事件的处理逻辑

```
    private void triggerLongPress() {
        if ((mView.getParent() != null)
                && mView.hasWindowFocus()
                && (!mView.isPressed() || mListener != null)
                && !mHasPerformedLongPress) {
            boolean handled;
            if (mListener != null) {
                handled = mListener.onLongClick(mView);
            } else {
            //我们没有设置listener，所以最后交给FolderIcon自己处理了
                handled = mView.performLongClick();
            }
            if (handled) {
                mView.setPressed(false);
                mHasPerformedLongPress = true;
            }
            clearCallbacks();
        }
    }
```

这里讲一下，长按事件是在workspace添加元素的时候统一添加的,用的下边这个

```
    default View.OnLongClickListener getWorkspaceChildOnLongClickListener() {
        return ItemLongClickListener.INSTANCE_WORKSPACE;
    }
```

#### \>3.获取对象

```
    public static <T extends Context & ActivityContext> FolderIcon inflateFolderAndIcon(int resId,
            T activityContext, ViewGroup group, FolderInfo folderInfo) {
            //创建folder，也就是foldericon展开的控件
        Folder folder = Folder.fromXml(activityContext);
        //创建foldericon并设置数据
        FolderIcon icon = inflateIcon(resId, activityContext, group, folderInfo);
        folder.setFolderIcon(icon);
        folder.bind(folderInfo);
        icon.setFolder(folder);
        return icon;
    }

    public static FolderIcon inflateIcon(int resId, ActivityContext activity, ViewGroup group,
            FolderInfo folderInfo) {

        DeviceProfile grid = activity.getDeviceProfile();
        FolderIcon icon = (FolderIcon) LayoutInflater.from(group.getContext())
                .inflate(resId, group, false);

        icon.mFolderName = icon.findViewById(R.id.folder_icon_name);//文件夹名字
        //...
        FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) icon.mFolderName.getLayoutParams();
        //设置topMargin，这样就显示在图标下边了。
        lp.topMargin = grid.iconSizePx + grid.iconDrawablePaddingPx;

        icon.setTag(folderInfo);
        //点击事件
        icon.setOnClickListener(ItemClickHandler.INSTANCE);
        //...
        icon.setDotInfo(folderDotInfo);

    //预览图数据设置
        icon.mPreviewVerifier = new FolderGridOrganizer(activity.getDeviceProfile().inv);
        icon.mPreviewVerifier.setFolderInfo(folderInfo);
        icon.updatePreviewItems(false);

        folderInfo.addListener(icon);

        return icon;
    }
```

#### \>4.点击事件

ItemClickHandler.INSTANCE

```
    private static void onClick(View v) {
    //根据tag拿到点击的view所属的类型，对应处理
        Object tag = v.getTag();
        if (tag instanceof WorkspaceItemInfo) {
            onClickAppShortcut(v, (WorkspaceItemInfo) tag, launcher);
        } else if (tag instanceof FolderInfo) {
            if (v instanceof FolderIcon) {
                onClickFolderIcon(v);
            }
        } else if (tag instanceof AppInfo) {
            startAppShortcutOrInfoActivity(v, (AppInfo) tag, launcher);
        } else if (tag instanceof LauncherAppWidgetInfo) {
            if (v instanceof PendingAppWidgetHostView) {
                onClickPendingWidget((PendingAppWidgetHostView) v, launcher);
            }
        } else if (tag instanceof ItemClickProxy) {
            ((ItemClickProxy) tag).onItemClicked(v);
        }
    }
```

最终是拿到folder以后调用animate方法显示

```
folder.animateOpen();
```

#### \>5.dispatchDraw

```
    protected void dispatchDraw(Canvas canvas) {
        super.dispatchDraw(canvas);

        if (!mBackgroundIsVisible) return;

        mPreviewItemManager.recomputePreviewDrawingParams();

        if (!mBackground.drawingDelegated()) {
            mBackground.drawBackground(canvas);
        }

        if (mCurrentPreviewItems.isEmpty() && !mAnimating) return;
    //通过管理类画背景，就是folder里缩小的app图标
        mPreviewItemManager.draw(canvas);

        if (!mBackground.drawingDelegated()) {
            mBackground.drawBackgroundStroke(canvas);
        }

        drawDot(canvas);
    }
```

### 3.10.Folder

这玩意是个线性布局，点击FolerIcon以后显示的浮窗

```
public abstract class AbstractFloatingView extends LinearLayout implements TouchController {
```

```
public class Folder extends AbstractFloatingView implements ClipPathView, DragSource,
        View.OnLongClickListener, DropTarget, FolderListener, TextView.OnEditorActionListener,
        View.OnFocusChangeListener, DragListener, ExtendedEditText.OnBackKeyListener {
        //...
@IntDef({STATE_CLOSED, STATE_ANIMATING, STATE_OPEN})
public @interface FolderState {}
```

#### \>1.user\_folder\_icon\_normalized.xml

FolderPagedView也是个自定义view，类似workspace，里边也是添加celllayout作为一页容器

```
<com.android.launcher3.folder.Folder 
    android:layout_width="wrap_content"
    android:layout_height="wrap_content"
    android:orientation="vertical" >

    <com.android.launcher3.folder.FolderPagedView
        android:id="@+id/folder_content"
        android:clipToPadding="false"
        android:layout_width="match_parent"
        android:layout_height="match_parent"
        launcher:pageIndicator="@+id/folder_page_indicator" />
    <!--水平方向，文件夹名字以及indicator-->
    <LinearLayout
        android:id="@+id/folder_footer"
        android:layout_width="match_parent"
        android:layout_height="@dimen/folder_footer_height_default"
        android:clipChildren="false"
        android:orientation="horizontal">

        <com.android.launcher3.folder.FolderNameEditText
            android:id="@+id/folder_name"
            android:layout_width="0dp"
            android:layout_height="wrap_content"
            android:layout_gravity="center_vertical"
            style="@style/TextHeadline"
            android:layout_weight="1"/>

        <com.android.launcher3.pageindicators.PageIndicatorDots
            android:id="@+id/folder_page_indicator"
            android:layout_gravity="center_vertical"
            android:layout_width="wrap_content"
            android:layout_height="wrap_content"
            android:elevation="1dp"
            />

    </LinearLayout>

</com.android.launcher3.folder.Folder>
```

#### \>2.onFinishInflate

获取背景图片，以及给child设置对应的属性

```
    protected void onFinishInflate() {
        super.onFinishInflate();

        mBackground = (GradientDrawable) ResourcesCompat.getDrawable(getResources(),
                R.drawable.round_rect_folder, getContext().getTheme());

        mContent = findViewById(R.id.folder_content);
        mContent.setFolder(this);

        mPageIndicator = findViewById(R.id.folder_page_indicator);
        mFolderName = findViewById(R.id.folder_name);
        //...

        mFooter = findViewById(R.id.folder_footer);
        mFooterHeight = dp.folderFooterHeightPx;
    }
```

#### \>3.animateOpen

点击FolderIcon以后会调用下边的方法显示Folder

```
    public void animateOpen() {
        animateOpen(mInfo.contents, 0);
    }
    
private void animateOpen(List<WorkspaceItemInfo> items, int pageNo) {
    Folder openFolder = getOpen(mActivityContext);
    if (openFolder != null && openFolder != this) {
        //先查下，如果有在显示的folder，关闭它
        openFolder.close(true);
    }

    mContent.bindItems(items);
    centerAboutIcon();//计算下位置，大小，设置下背景图的大小
    mItemsInvalidated = true;
    updateTextViewFocus();

    mIsOpen = true;

    BaseDragLayer dragLayer = mActivityContext.getDragLayer();
    if (getParent() == null) {
        dragLayer.addView(this);//添加到容器里
        mDragController.addDropTarget(this);
    } else {
    }

    mContent.completePendingPageChanges();
    mContent.setCurrentPage(pageNo);

    mDeleteFolderOnDropCompleted = false;

    //...anim

    // Footer animation
    if (mContent.getPageCount() > 1 && !mInfo.hasOption(FolderInfo.FLAG_MULTI_PAGE_ANIMATION)) {

    //...
    } else {
        mFolderName.setTranslationX(0);
    }

    //...
}
```

#### \>4.close

首先这个东西是添加到dragLayer层的，所以最开始的touch事件处理就是从layer层开始判断的。

BaseDragLayer.java

```
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        int action = ev.getAction();

        if (action == ACTION_UP || action == ACTION_CANCEL) {
            if (mTouchCompleteListener != null) {
                mTouchCompleteListener.onTouchComplete();
            }
            mTouchCompleteListener = null;
        } else if (action == MotionEvent.ACTION_DOWN) {
            mActivity.finishAutoCancelActionMode();
        }
        return findActiveController(ev);//这一行..
    }
```

//继续

```
    protected boolean findActiveController(MotionEvent ev) {
        mActiveController = null;
        if (canFindActiveController()) {
            mActiveController = findControllerToHandleTouch(ev);//这行..
        }
        return mActiveController != null;
    }
```

//Folder就是继承的AbstractFloatingView，所以显示的时候，最上层查到的topView就是我们的Folder了

```
    private TouchController findControllerToHandleTouch(MotionEvent ev) {
        AbstractFloatingView topView = AbstractFloatingView.getTopOpenView(mActivity);
        if (topView != null
                && (isEventInLauncher(ev) || topView.canInterceptEventsInSystemGestureRegion())
                && topView.onControllerInterceptTouchEvent(ev)) {//这行..
            return topView;
        }

        for (TouchController controller : mControllers) {
            if (controller.onControllerInterceptTouchEvent(ev)) {
                return controller;
            }
        }
        return null;
    }
```

//回到Folder.java

```
    public boolean onControllerInterceptTouchEvent(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_DOWN) {
            BaseDragLayer dl = (BaseDragLayer) getParent();

            if (isEditingName()) {
                if (!dl.isEventOverView(mFolderName, ev)) {
                    mFolderName.dispatchBackKey();
                    return true;
                }
                return false;
            } else if (!dl.isEventOverView(this, ev)
                    && mLauncherDelegate.interceptOutsideTouch(ev, dl, this)) {//这行..
                return true;
            }
        }
        return false;
    }
```

//go on,我们是点击空白区域隐藏Folder，所以很明显，走的else

```
    boolean interceptOutsideTouch(MotionEvent ev, BaseDragLayer dl, Folder folder) {
        if (mLauncher.getAccessibilityDelegate().isInAccessibleDrag()) {
            // Do not close the container if in drag and drop.
            if (!dl.isEventOverView(mLauncher.getDropTargetBar(), ev)) {
                return true;
            }
        } else {
            // 这行。
            folder.close(true);
            return true;
        }
        return false;
    }
```

// Folder.java

```
    protected void handleClose(boolean animate) {
        mIsOpen = false;
//...
        if (animate) {
            animateClosed();//带动画的，走这里
        } else {
            closeComplete(false);
            post(this::announceAccessibilityChanges);
        }

    }
```

//最终会走到这里，重新显示FolderIcon, 并从dragLayer里移除Folder

```
    private void closeComplete(boolean wasAnimated) {
        BaseDragLayer parent = (BaseDragLayer) getParent();
        if (parent != null) {
            parent.removeView(this);// 移除
        }
        mDragController.removeDropTarget(this);
        clearFocus();
        if (mFolderIcon != null) {
            mFolderIcon.setVisibility(View.VISIBLE);
            mFolderIcon.setIconVisible(true);//重新显示
            mFolderIcon.mFolderName.setTextVisibility(true);
        }
```

### 3.11.FolderPagedView

```
public class FolderPagedView extends PagedView<PageIndicatorDots> implements ClipPathView {
```

## 4.学习记录

### 4.1.ItemLongClickListener.java

hotseat，workspace，allapps页面的icon的长按事件，都会调用canStartDrag这个方法判断的，所以在这个方法里处理就行。

```
public static final OnLongClickListener INSTANCE_WORKSPACE =
        ItemLongClickListener::onWorkspaceItemLongClick;

public static final OnLongClickListener INSTANCE_ALL_APPS =
        ItemLongClickListener::onAllAppsItemLongClick;
```

#### \>1.onWorkspaceItemLongClick

```
private static boolean onWorkspaceItemLongClick(View v) {
    Launcher launcher = Launcher.getLauncher(v.getContext());
    //先判断是否支持拖拽
    if (!canStartDrag(launcher)) return false;
    if (!launcher.isInState(NORMAL) && !launcher.isInState(OVERVIEW)) return false;
    if (!(v.getTag() instanceof ItemInfo)) return false;

    launcher.setWaitingForResult(null);
    beginDrag(v, launcher, (ItemInfo) v.getTag(), launcher.getDefaultWorkspaceDragOptions());
    return true;
}
```

#### \>2.onAllAppsItemLongClick

```
private static boolean onAllAppsItemLongClick(View view) {
    //..
    Launcher launcher = Launcher.getLauncher(v.getContext());
    //先判断是否支持拖拽
    if (!canStartDrag(launcher)) return false;
    // When we have exited all apps or are in transition, disregard long clicks
    if (!launcher.isInState(ALL_APPS) && !launcher.isInState(OVERVIEW)) return false;
    if (launcher.getWorkspace().isSwitchingState()) return false;


    // Start the drag
    final DragController dragController = launcher.getDragController();
    dragController.addDragListener(new DragController.DragListener() {
        @Override
        public void onDragStart(DropTarget.DragObject dragObject, DragOptions options) {
        //开始拖拽，原本的view不可见
            v.setVisibility(INVISIBLE);
        }

        @Override
        public void onDragEnd() {
        //拖拽结束，恢复可见，移除监听
            v.setVisibility(VISIBLE);
            dragController.removeDragListener(this);
        }
    });

    launcher.getWorkspace().beginDragShared(v, launcher.getAppsView(), new DragOptions());
    return false;
}
```

#### \>3.canStartDrag

下边的方法返回false，所有的icon都长按都没有反应了，自然也不会拖动了。

```
//禁止元素拖拽的话，这里返回false即可
    public static boolean canStartDrag(Launcher launcher) {
        if (launcher == null) {
            return false;
        }
        if (launcher.isWorkspaceLocked()) return false;
        if (launcher.getDragController().isDragging()) return false;
        return true;
    }
```

### 4.2.WorkspaceLayoutManager.java

一个抽象类，实现了把view添加到container的逻辑，workspace，hotseat里的元素添加用的这个

```
    default void addInScreen(View child, int container, int screenId, int x, int y,
            int spanX, int spanY) {
        if (container == LauncherSettings.Favorites.CONTAINER_DESKTOP) {
        
        }

        final CellLayout layout;
        // 容器是hotseat类型
        if (container == LauncherSettings.Favorites.CONTAINER_HOTSEAT
                || container == LauncherSettings.Favorites.CONTAINER_HOTSEAT_PREDICTION) {
            layout = getHotseat();
            //hotseat里的只有图标，文本隐藏
            if (child instanceof FolderIcon) {
                ((FolderIcon) child).setTextVisible(false);
            }
        } else {
            // 其他的也就是workspace里的了，这个图标文字都显示的
            if (child instanceof FolderIcon) {
                ((FolderIcon) child).setTextVisible(true);
            }
            layout = getScreenWithId(screenId);
        }

        ViewGroup.LayoutParams genericLp = child.getLayoutParams();
        CellLayoutLayoutParams lp;
        if (genericLp == null || !(genericLp instanceof CellLayoutLayoutParams)) {
            lp = new CellLayoutLayoutParams(x, y, spanX, spanY, screenId);
        } else {
            lp = (CellLayoutLayoutParams) genericLp;
            lp.cellX = x;
            lp.cellY = y;
            lp.cellHSpan = spanX;
            lp.cellVSpan = spanY;
        }

        if (spanX < 0 && spanY < 0) {
            lp.isLockedToGrid = false;
        }

      
        ItemInfo info = (ItemInfo) child.getTag();
        int childId = info.getViewId();

        boolean markCellsAsOccupied = !(child instanceof Folder);
        //把child添加到容器里
        if (!layout.addViewToCellLayout(child, -1, childId, lp, markCellsAsOccupied)) {
        }

        child.setHapticFeedbackEnabled(false);
        //看下child的长按事件，都是这个
        child.setOnLongClickListener(getWorkspaceChildOnLongClickListener());
        if (child instanceof DropTarget) {
        //这个是拖拽到顶部丢弃的child
            onAddDropTarget((DropTarget) child);
        }
    }
        // child的长按事件用的这个
    default View.OnLongClickListener getWorkspaceChildOnLongClickListener() {
        return ItemLongClickListener.INSTANCE_WORKSPACE;
    }
```

## 5.BubbleTextView.java

-   桌面的图标基本都是这个控件，包括hotseat（把文字隐藏了）
-   主要是背景可以画个气泡

```
public class BubbleTextView extends TextView implements ItemInfoUpdateReceiver,
        IconLabelDotView, DraggableView, Reorderable {
//这里定义了8种展示类型，类型不同，图标大小样式啥的可能不一样
    private static final int DISPLAY_WORKSPACE = 0;
    private static final int DISPLAY_ALL_APPS = 1;
    private static final int DISPLAY_FOLDER = 2;
    protected static final int DISPLAY_TASKBAR = 5;
    private static final int DISPLAY_SEARCH_RESULT = 6;
    private static final int DISPLAY_SEARCH_RESULT_SMALL = 7;
```

### 5.1.构造方法

```
    public BubbleTextView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mActivity = ActivityContext.lookupContext(context);

        TypedArray a = context.obtainStyledAttributes(attrs,
                R.styleable.BubbleTextView, defStyle, 0);
        mLayoutHorizontal = a.getBoolean(R.styleable.BubbleTextView_layoutHorizontal, false);
        mIsRtl = (getResources().getConfiguration().getLayoutDirection()
                == View.LAYOUT_DIRECTION_RTL);
        DeviceProfile grid = mActivity.getDeviceProfile();
        //获取display类型，默认的是DISPLAY_WORKSPACE，跟主题有关，见5.2
        mDisplay = a.getInteger(R.styleable.BubbleTextView_iconDisplay, DISPLAY_WORKSPACE);
        final int defaultIconSize;
        if (mDisplay == DISPLAY_WORKSPACE) {
            setTextSize(TypedValue.COMPLEX_UNIT_PX, grid.iconTextSizePx);
            setCompoundDrawablePadding(grid.iconDrawablePaddingPx);
            defaultIconSize = grid.iconSizePx;
            setCenterVertically(grid.isScalableGrid);
        } else if (mDisplay == DISPLAY_ALL_APPS) {
            setTextSize(TypedValue.COMPLEX_UNIT_PX, grid.allAppsIconTextSizePx);
            setCompoundDrawablePadding(grid.allAppsIconDrawablePaddingPx);
            defaultIconSize = grid.allAppsIconSizePx;
        } else if (mDisplay == DISPLAY_FOLDER) {
            setTextSize(TypedValue.COMPLEX_UNIT_PX, grid.folderChildTextSizePx);
            setCompoundDrawablePadding(grid.folderChildDrawablePaddingPx);
            defaultIconSize = grid.folderChildIconSizePx;
        } else if (mDisplay == DISPLAY_SEARCH_RESULT) {
            defaultIconSize = getResources().getDimensionPixelSize(R.dimen.search_row_icon_size);
        } else if (mDisplay == DISPLAY_SEARCH_RESULT_SMALL) {
            defaultIconSize = getResources().getDimensionPixelSize(
                    R.dimen.search_row_small_icon_size);
        } else if (mDisplay == DISPLAY_TASKBAR) {
//我们的需求把这个改大点，结果iconSizePx是动态算的，和配置里的差距太大，害我一直搞不定
//解决办法，不用这个值了，直接用taskbar里用的R.dimen.taskbar_icon_touch_size
            defaultIconSize = grid.iconSizePx;
        } else {
            // widget_selection or shortcut_popup
            defaultIconSize = grid.iconSizePx;
        }

        mCenterVertically = a.getBoolean(R.styleable.BubbleTextView_centerVertically, false);
        //或者布局里配置iconSizeOverride
        mIconSize = a.getDimensionPixelSize(R.styleable.BubbleTextView_iconSizeOverride,
                defaultIconSize);
        a.recycle();

        mLongPressHelper = new CheckLongPressHelper(this);

        mDotParams = new DotRenderer.DrawParams();

        setEllipsize(TruncateAt.END);
        setAccessibilityDelegate(mActivity.getAccessibilityDelegate());
        setTextAlpha(1f);
    }
```

### 5.2.相关的布局以及子类

#### \>1.taskbar\_predicted\_app\_icon.xml

taskbar上hotseat用到的布局，系统推荐的app

```
<com.android.launcher3.uioverrides.PredictedAppIcon style="@style/BaseIcon.Workspace.Taskbar" />
```

主题如下，可以看到这里是设置了display

```
    <!-- Icon displayed on the taskbar -->
    <style name="BaseIcon.Workspace.Taskbar" >
        <item name="iconDisplay">taskbar</item>
    </style>
```

#### \>2.taskbar\_app\_icon.xml

主题参考补充1

```
<com.android.launcher3.views.DoubleShadowBubbleTextView style="@style/BaseIcon.Workspace.Taskbar" />
```

#### \>all\_apps\_icon.xml

这里是布局里直接生命了display的值

```
<com.android.launcher3.BubbleTextView xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:launcher="http://schemas.android.com/apk/res-auto"
    style="@style/BaseIcon.AllApps"
    android:id="@+id/icon"
    launcher:iconDisplay="all_apps"
    launcher:centerVertically="true" />
```

## 6.总结

-   桌面的布局结构
-   DevicePorfile，launcher3里各种配置相关的数据基本都在这里，比如常用的hotseat高度，workspace每页可以分成几行几列
-   cellLayout，一个可以按照网格分割显示child的容器，hotseat继承的它，workspace里添加的是它
-   workspace里每页的元素，主要有3种类型，app的图标，app的小部件，文件夹
-   3.5小节里是空白区域的长按事件，4.1小节的是元素的长按事件