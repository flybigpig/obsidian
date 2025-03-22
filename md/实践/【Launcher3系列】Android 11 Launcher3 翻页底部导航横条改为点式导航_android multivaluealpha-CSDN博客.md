###### 1\. 概述

> Android系统的[Launcher](https://so.csdn.net/so/search?q=Launcher&spm=1001.2101.3001.7020)改造在国内算是一个不算很低频的需求，尤其是相当多的三方硬件设备以及部分手机厂商的个性化ROM等，受限于国内整体的开源环境，网上能找到相关的开发资源并不多，最近公司有相关业务需求的开发，就自己涉及到的改造点作一些分享。

源码中默认展示的翻页是类似于Android常见的[tab切换](https://so.csdn.net/so/search?q=tab%E5%88%87%E6%8D%A2&spm=1001.2101.3001.7020)样式，不过国内的定制OS似乎都更倾向于点状的切换显示，这个实际上源码里本就支持，仅需要稍作改动切换一下即可。

###### 2\. 涉及到的类及接口等

主要是改动Workspace继承的PagedView泛型，以及PageIndicatorDot更改

```
com.android.launcher3.Workspace
com.android.launcher3.Launcher
com.android.launcher3.pageindicators.PageIndicatorDots
com.android.launcher3.QuickstepAppTransitionManagerImpl
launcher.xml
dimens.xml
```

###### 3\. Show the code

首先替换Workspace中的父类泛型

Workspace.java

```
public class Workspace extends PagedView<PageIndicatorDots>
        //这里原本为PagedView<WorkspacePageIndicator>
        implements DropTarget, DragSource, View.OnTouchListener,
        DragController.DragListener, Insettable, StateHandler<LauncherState>,
        WorkspaceLayoutManager {
    //...//
}
```

修正或删除相关引用

Launcher.java

```
    @Override
    public void onStateSetStart(LauncherState state) {
        super.onStateSetStart(state);
        if (mDeferOverlayCallbacks) {
            scheduleDeferredCheck();
        }
        addActivityFlags(ACTIVITY_STATE_TRANSITION_ACTIVE);

        if (state.hasFlag(FLAG_CLOSE_POPUPS)) {
            AbstractFloatingView.closeAllOpenViews(this, !state.hasFlag(FLAG_NON_INTERACTIVE));
        }

        if (state == SPRING_LOADED) {
            // Prevent any Un/InstallShortcutReceivers from updating the db while we are
            // not on homescreen
            InstallShortcutReceiver.enableInstallQueue(FLAG_DRAG_AND_DROP);
            getRotationHelper().setCurrentStateRequest(REQUEST_LOCK);

            mWorkspace.showPageIndicatorAtCurrentScroll();
            mWorkspace.setClipChildren(false);
        }
        // When multiple pages are visible, show persistent page indicator
//====修改start=== 改为引导点时，无该方法
        //mWorkspace.getPageIndicator().setShouldAutoHide(!state.hasFlag(FLAG_MULTI_PAGE));
//====修改end====
    }
```

QuickstepAppTransitionManagerImpl.java

```
private Pair<AnimatorSet, Runnable> getLauncherContentAnimator(boolean isAppOpening,
            float[] trans) {
        AnimatorSet launcherAnimator = new AnimatorSet();
        Runnable endListener;

        float[] alphas = isAppOpening
                ? new float[] {1, 0}
                : new float[] {0, 1};

        if (mLauncher.isInState(ALL_APPS)) {
            //...//
        } else {
            mDragLayerAlpha.setValue(alphas[0]);
            ObjectAnimator alpha =
                    ObjectAnimator.ofFloat(mDragLayerAlpha, MultiValueAlpha.VALUE, alphas);
            alpha.setDuration(CONTENT_ALPHA_DURATION);
            alpha.setInterpolator(LINEAR);
            launcherAnimator.play(alpha);

            Workspace workspace = mLauncher.getWorkspace();
            View currentPage = ((CellLayout) workspace.getChildAt(workspace.getCurrentPage()))
                    .getShortcutsAndWidgets();
            View hotseat = mLauncher.getHotseat();
            View qsb = mLauncher.findViewById(R.id.search_container_all_apps);

            currentPage.setLayerType(View.LAYER_TYPE_HARDWARE, null);
            hotseat.setLayerType(View.LAYER_TYPE_HARDWARE, null);
            qsb.setLayerType(View.LAYER_TYPE_HARDWARE, null);

            launcherAnimator.play(ObjectAnimator.ofFloat(currentPage, View.TRANSLATION_Y, trans));
            launcherAnimator.play(ObjectAnimator.ofFloat(hotseat, View.TRANSLATION_Y, trans));
            launcherAnimator.play(ObjectAnimator.ofFloat(qsb, View.TRANSLATION_Y, trans));

            // Pause page indicator animations as they lead to layer trashing.
//            mLauncher.getWorkspace().getPageIndicator().pauseAnimations();

            endListener = () -> {
                currentPage.setTranslationY(0);
                hotseat.setTranslationY(0);
                qsb.setTranslationY(0);

                currentPage.setLayerType(View.LAYER_TYPE_NONE, null);
                hotseat.setLayerType(View.LAYER_TYPE_NONE, null);
                qsb.setLayerType(View.LAYER_TYPE_NONE, null);

                mDragLayerAlpha.setValue(1f);
//                mLauncher.getWorkspace().getPageIndicator().skipAnimationsToEnd();
            };
        }
        return new Pair<>(launcherAnimator, endListener);
    }
```

给PageIndicatorDots添加setInsets处理， 此处直接将WorkspacePageIndicator中的setInsets方法拷贝过来

PageIndicatorDots.java

```
public class PageIndicatorDots extends View implements PageIndicator, Insettable {
    //此处的新增对Insettable接口的实现
    pprivate static final float SHIFT_PER_ANIMATION = 0.5f;
    private static final float SHIFT_THRESHOLD = 0.1f;
    private static final long ANIMATION_DURATION = 150;

    private static final int ENTER_ANIMATION_START_DELAY = 300;
    private static final int ENTER_ANIMATION_STAGGERED_DELAY = 150;
    private static final int ENTER_ANIMATION_DURATION = 400;

    // This value approximately overshoots to 1.5 times the original size.
    private static final float ENTER_ANIMATION_OVERSHOOT_TENSION = 4.9f;

    private static final RectF sTempRect = new RectF();

    private static final Property<PageIndicatorDots, Float> CURRENT_POSITION
            = new Property<PageIndicatorDots, Float>(float.class, "current_position") {
        @Override
        public Float get(PageIndicatorDots obj) {
            return obj.mCurrentPosition;
        }

        @Override
        public void set(PageIndicatorDots obj, Float pos) {
            obj.mCurrentPosition = pos;
            obj.invalidate();
            obj.invalidateOutline();
        }
    };
//新增mLauncher ====修改start====
    private final Launcher mLauncher;
//====修改end====
    private final Paint mCirclePaint;
    private final float mDotRadius;
    private final int mActiveColor;
    private final int mInActiveColor;
    private final boolean mIsRtl;

    private int mNumPages;
    private int mActivePage;

    //...//

    public PageIndicatorDots(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
//新增mLauncher ====修改start====
        mLauncher = Launcher.getLauncher(context);
//====修改end====
        mCirclePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mCirclePaint.setStyle(Style.FILL);
        mDotRadius = getResources().getDimension(R.dimen.page_indicator_dot_size) / 2;
        setOutlineProvider(new MyOutlineProver());

        mActiveColor = Themes.getColorAccent(context);
        mInActiveColor = Themes.getAttrColor(context, android.R.attr.colorControlHighlight);

        mIsRtl = Utilities.isRtl(getResources());
    }

       @Override
    public void setScroll(int currentScroll, int totalScroll) {
        if (mNumPages > 1) {
            if (mIsRtl) {
                currentScroll = totalScroll - currentScroll;
            }
            int scrollPerPage = totalScroll / (mNumPages - 1);
//====修改start====
            //调试时有时会发生崩溃，检查发现是totalscroll有时会为0，导致scrollPerPage会有分母为0情况
            if(scrollPerPage == 0){
                return;
            }
//====修改end====
            int pageToLeft = currentScroll / scrollPerPage;
            int pageToLeftScroll = pageToLeft * scrollPerPage;
            int pageToRightScroll = pageToLeftScroll + scrollPerPage;

            //...//

    }

    //...//

//====增加setInsets实现====
    @Override
    public void setInsets(Rect insets) {
        DeviceProfile grid = mLauncher.getDeviceProfile();
        FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) getLayoutParams();

        if (grid.isVerticalBarLayout()) {
            Rect padding = grid.workspacePadding;
            lp.leftMargin = padding.left + grid.workspaceCellPaddingXPx;
            lp.rightMargin = padding.right + grid.workspaceCellPaddingXPx;
            lp.bottomMargin = padding.bottom;
        } else {
            lp.leftMargin = lp.rightMargin = 0;
            lp.gravity = Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM;
            lp.bottomMargin = grid.hotseatBarSizePx + insets.bottom;
        }
        setLayoutParams(lp);
    }
}
```

launcher.xml

```
<com.android.launcher3.LauncherRootView
    xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:launcher="http://schemas.android.com/apk/res-auto"
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

        <!--省略-->

        <include
            android:id="@+id/overview_panel"
            layout="@layout/overview_panel" />


        <!-- Keep these behind the workspace so that they are not visible when
         we go into AllApps -->
        <com.android.launcher3.pageindicators.PageIndicatorDots
            android:id="@+id/page_indicator"
            android:layout_width="match_parent"
            android:layout_height="@dimen/workspace_page_indicator_dot_height"
            android:layout_gravity="bottom|center_horizontal"
            android:theme="@style/HomeScreenElementTheme" />
<!--====更改PageIndicatorDots的layout_height ====-->
        <include
            android:id="@+id/drop_target_bar"
            layout="@layout/drop_target_bar" />

        <!--省略--->
    </com.android.launcher3.dragndrop.DragLayer>

</com.android.launcher3.LauncherRootView>
```

相应在dimens中添加

4dp

###### 4\. 结语

以上为翻页底部导航条改为导航点涉及到的全部改动，供参考。