## 前言

相信 Android 开发都知道，View 是树形结构，一组 View 的集合就是 `ViewGroup`，而 `ViewGroup` 中又可以包含 View 和其他 `ViewGroup`，从而构成了树结构。那么问题来了，这棵树的根又是什么呢？接下来就让我们一起来探究一下 Android 的顶级 View——`DecorView`。

## 1\. 从 Activity 探究 View 的布局

之所以从 `Activity` 来开始看，是因为一个 App 的界面都是由 `Activity` 加载各种各样的布局得到的，这里的布局当然就是由 View 组成的啦，`Activity` 加载布局的代码相信大家都很熟悉：

```
public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.zmq_test);
}
```

进去 `setContentView` 的源码来康康它究竟做了些什么：

```
public void setContentView(int layoutResID) {
    // 1.getWindow()
    getWindow().setContentView(layoutResID);
    initWindowDecorActionBar();
}
```

先看看注释 1 处，我们看看 `getWindow` 是什么东东：

```
public Window getWindow() {
    return mWindow;
}
```

`getWindow` 直接返回了一个私有变量 `mWindow`，那我们就全局找一下 `mWindow` 是在哪里被初始化的吧？

```
final void attach(Context context, ActivityThread aThread,
        Instrumentation instr, IBinder token, int ident,
        Application application, Intent intent, ActivityInfo info,
        CharSequence title, Activity parent, String id,
        NonConfigurationInstances lastNonConfigurationInstances,
        Configuration config, String referrer, IVoiceInteractor voiceInteractor,
        Window window, ActivityConfigCallback activityConfigCallback, IBinder assistToken,
        IBinder shareableActivityToken) {
        
    attachBaseContext(context);
    mFragments.attachHost(null);
    // 初始化 mWindow
    mWindow = new PhoneWindow(this, window, activityConfigCallback);
    ...
}
```

我们在 `Activity` 的 `attch` 方法中找到了 `mWindow` 初始化的踪迹。原来 `mWindow` 是 `PhoneWindow` 类，继承自 `Window`，既然知道了 `getWindow` 返回的其实是 `PhoneWindow`，那就去看看 `PhoneWindow` 的 `setContentView` 方法都做了些什么吧：

```
public void setContentView(int layoutResID) {
    if (mContentParent == null) {
        // 1.初始化
        installDecor();
    } else if (!hasFeature(FEATURE_CONTENT_TRANSITIONS)) {
        mContentParent.removeAllViews();
    }
    ...
}
```

这段代码很清晰，如果放置内容的视图为空，就需要去执行 `installDecor` 方法：

```
private void installDecor() {
    if (mDecor == null) {
        // 1. 初始化 decor
        mDecor = generateDecor(-1);
        ...
    } else {
        mDecor.setWindow(this);
    }
    if (mContentParent == null) {
        // 2. 生成布局
        mContentParent = generateLayout(mDecor);
        ...
    }
}
```

`installDecor` 方法的代码太长啦，我们挑重点部分来看一看，这里已经能看到 `DecorView` 相关的蛛丝马迹了，`mDecor` 为空的时候就去生成一个对象，这个对象是不是就是我们要寻找的 `DecorView` 呢？

```
protected DecorView generateDecor(int featureId) {
    ...
    return new DecorView(context, featureId, this, getAttributes());
}
```

果然！在 `generateDecor` 方法中找到了创建 `DecorView` 的代码，`DecorView` 继承自 `FrameLayout`，也就是 View 的子类，更是 `Activity` 中的根 View。那么 `Activity` 到底是如何把 `DecorView` 作为根 View 布局的呢？我们接着往下看，注释 2 处的 `generateLayout` 使用创建的 `mDecor` 做了什么吧：

```
protected ViewGroup generateLayout(DecorView decor) {
    // 1.根据当前的 Activity 主题来设置一些属性.
    TypedArray a = getWindowStyle();
    ...
    if (a.getBoolean(R.styleable.Window_windowNoTitle, false)) {
        requestFeature(FEATURE_NO_TITLE);
    } else if (a.getBoolean(R.styleable.Window_windowActionBar, false)) {
        // Don't allow an action bar if there is no title.
        requestFeature(FEATURE_ACTION_BAR);
    }
    ...
    
    // 2.给 layoutResource 赋值不同的资源 id ，加载不同的布局。
    int layoutResource;
    int features = getLocalFeatures();
    if ((features & ((1 << FEATURE_LEFT_ICON) | (1 << FEATURE_RIGHT_ICON))) != 0) {
        if (mIsFloating) {
            TypedValue res = new TypedValue();
            getContext().getTheme().resolveAttribute(
                    R.attr.dialogTitleIconsDecorLayout, res, true);
            layoutResource = res.resourceId;
        } else {
            layoutResource = R.layout.screen_title_icons;
        }
        removeFeature(FEATURE_ACTION_BAR);
    } else if ((features & ((1 << FEATURE_PROGRESS) | (1 << FEATURE_INDETERMINATE_PROGRESS))) != 0
            && (features & (1 << FEATURE_ACTION_BAR)) == 0) {
        layoutResource = R.layout.screen_progress;
    } else if ((features & (1 << FEATURE_CUSTOM_TITLE)) != 0) {
        if (mIsFloating) {
            TypedValue res = new TypedValue();
            getContext().getTheme().resolveAttribute(
                    R.attr.dialogCustomTitleDecorLayout, res, true);
            layoutResource = res.resourceId;
        } else {
            layoutResource = R.layout.screen_custom_title;
        }
        removeFeature(FEATURE_ACTION_BAR);
    } else if ((features & (1 << FEATURE_NO_TITLE)) == 0) {
        if (mIsFloating) {
            TypedValue res = new TypedValue();
            getContext().getTheme().resolveAttribute(
                    R.attr.dialogTitleDecorLayout, res, true);
            layoutResource = res.resourceId;
        } else if ((features & (1 << FEATURE_ACTION_BAR)) != 0) {
            layoutResource = a.getResourceId(
                    R.styleable.Window_windowActionBarFullscreenDecorLayout,
                    R.layout.screen_action_bar);
        } else {
            layoutResource = R.layout.screen_title;
        }
    } else if ((features & (1 << FEATURE_ACTION_MODE_OVERLAY)) != 0) {
        layoutResource = R.layout.screen_simple_overlay_action_mode;
    } else {
        layoutResource = R.layout.screen_simple;
    }
  
    mDecor.startChanging();
    // 3.加载资源
    mDecor.onResourcesLoaded(mLayoutInflater, layoutResource);
    ...
    return contentParent;
}
```

`generateLayout` 方法中的代码有足足几百行，其实从方法名字也能看出来，这个方法是用来生成布局的。首先会根据开发设置的一些主题来调整我们的布局，然后就是给我们的窗口 `Window` 进行装饰啦。可以看到，根据不同的情况会使用不同的布局。最后在注释 3 处，`mDecor` 对象会根据选定的 `layoutResource` 来加载布局，我们随便点一个 layout 进去康康布局长什么样？

```
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:orientation="vertical"
    android:fitsSystemWindows="true">
    <!-- Popout bar for action modes -->
    <ViewStub android:id="@+id/action_mode_bar_stub"
              android:inflatedId="@+id/action_mode_bar"
              android:layout="@layout/action_mode_bar"
              android:layout_width="match_parent"
              android:layout_height="wrap_content"
              android:theme="?attr/actionBarTheme" />

    <FrameLayout android:id="@android:id/title_container" 
        android:layout_width="match_parent" 
        android:layout_height="?android:attr/windowTitleSize"
        android:transitionName="android:title"
        style="?android:attr/windowTitleBackgroundStyle">
    </FrameLayout>
    <FrameLayout android:id="@android:id/content"
        android:layout_width="match_parent" 
        android:layout_height="0dip"
        android:layout_weight="1"
        android:foregroundGravity="fill_horizontal|top"
        android:foreground="?android:attr/windowContentOverlay" />
</LinearLayout>
```

我这里进入的是 `R.layout.screen_custom_title`，可以看到布局中提供了一个 `ActionBar`，两个 `FrameLayout` 分别用来显示 `title` 和 `content` 部分的。实际上我们在开发时所写的布局就是展示在`content` 中的，一般我们不会使用 `ActionBar` 或者 `title`，而是在应用中自己去实现标题。这也是为什么 Activity 中叫 `setContentView`，因为我们操作的是 `content` 部分。

我们可以用一张图来表示 `Activity` 和 `DecorView` 之间的关系：

![View 坐标系](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/6498e5ee9deb469a8828612ee8cf64ab~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 2\. 总结

根据上面的分析，相信你已经明白了 `DecorView` 作为根 `View` 是如何被创建以及加载的，为了更清晰简单用流程图表达下：

![View 坐标系](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/6ebf2d5f13494a3c8b432fa4d4cec767~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

1.  `Activity` 在 `attch` 时，会创建 `PhoneWindow` 对象，在 `onCreate` 执行其 `setContentView` 方法；
2.  `setContentView` 中会使用 `installDecor` 来创建一个 `DecorView` 对象作为根 `View`；
3.  得到了 `DecorView` 对象后，会通过 `generateLayout` 方法获取对应的资源 id，`DecorView` 会根据该 id 来加载不同的布局；
4.  `DecorView` 作为一个顶级 View，一般情况下内部会包含一个 `LinearLayout`，并将屏幕划分成 `TitleView` 和 `ContentView` 两部分。