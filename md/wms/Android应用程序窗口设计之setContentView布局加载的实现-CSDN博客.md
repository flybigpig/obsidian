##  Android应用程序窗口设计之setContentView布局加载的实现

Android应用程序窗口设计系列博客:

> [Android应用程序窗口设计之Window及WindowManager的创建](https://blog.csdn.net/tkwxty/article/details/109597570)  
> [Android应用程序窗口设计之setContentView布局加载的实现](https://blog.csdn.net/tkwxty/article/details/110005502)  
> [普法Android的Token前世今生以及在APP,AMS,WMS之间传递](https://blog.csdn.net/tkwxty/article/details/110563212)  
> [Android应用程序窗口设计之窗口的添加](https://blog.csdn.net/tkwxty/article/details/110524244)  
> [Android应用程序窗口设计之建立与WMS服务之间的通信过程](https://blog.csdn.net/tkwxty/article/details/111152011)  
> [Android窗口设计之Dialog、PopupWindow、系统窗口的实现](https://blog.csdn.net/tkwxty/article/details/111311014)  
> [Android应用程序建立与AMS服务之间的通信过程](https://blog.csdn.net/tkwxty/article/details/110817983)

___

本篇博客编写思路总结和关键点说明:

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/7dc9052fae7ad9fd8072837010b1fecd.png)  
为了更加方便的读者阅读博客，通过导读思维图的形式将本博客的关键点列举出来，从而方便读者取舍和阅读！

___

  

### 引言

  如果我们将Android应用程序窗口设计的实现比喻为一场接力赛的话，那么通过前面的博客[Android应用程序窗口设计之窗口以及窗口管理器的创建](https://blog.csdn.net/tkwxty/article/details/109597570)我们已经很好的完成了接力赛的第一棒的交接了，通过前面的博客我们主要完成了Android应用程序窗口设计中的Activity布局加载前期准备的准备工作了，即:

-   Activity启动的前期获取到Activity启动需要的信息，然后通过反射创建了我们的目标Activity
-   然后调用Activity实例的的attach方法，将启动Activity的相关信息初始化它的成员变量
-   然后在attach方法中，创建Activiyt对应的窗口Window(这里建议读者重点理解理解Window窗口的概念)
-   接着为创建的Window窗口设置窗口管理器WindowManagerImpl

至此，我们的Activity布局加载的前期准备已经就绪，窗口和窗口管理器已经建立，只待我们的Activity在目标窗口上挥斥方遒，指点江山做出美丽的画来了。而这也是我们本章博客要分析的。

> 注意：本篇的介绍是基于Android 7.xx平台为基础的，并且涉及的源码路径在前面的博客有放出来了就不重复了

在正式开始本篇博客Activity中界面布局的加载流程分析之前，这里有必要先将Activity、View、Window、WindowManager之间的类图关系先放出来，以便能够方便后续的相关阅读，使读者能够更加清楚的知道这几者之间在源码中的关系。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/485ed5355986e304bd850cb900ca8798.png)

___

### 一. 前期知识准备工作

  俗话诚不欺我啊，工欲利其事必先利其器！所以在开始本篇博客的分析之前我们必须引入几个国之重器来提升我们源码分析的质量和速度。

#### 1.1. Android视图层级工具的使用

我们知道Android的视图是按照层级呈现的，并且在后续的Activity布局加载中也会涉及到Android的视图的层级关系，那么有没有什么工具能比较直观的帮助我们查看Android视图的层级关系呢？这个答案是肯定的，并且我们常见的Android开发IDE工具eclipse和Android stuido都有，这里我对二者都简单介绍一下。

##### 1.1.1 Eclipse下通过Hierachy View查看Activity布局视图层级

  也许很多新时代的读者看到这个标题会很不屑的说，都啥年代了还有人在用Eclipse进行Android开发！能不拆台吗，虽然是如此，但是秉着不抛弃不放弃的原则我们还是说说Eclipse下通过Hierachy View查看Activity布局视图层级的使用方法。Hierachy View是Android SDK下tool自带的一个视图分析工具，并且我们的eclipse也默认将其集成了。

Eclipse下使用Hierachy View工具非常简单，点击工具中的Window—>Open Perspective—> Hierachy View即可打开该工具的分析窗口，如下：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f155c81e72b4f6a1789cf229f1005355.png#pic_center)

> 这里有一点需要注意的就是在打开该工具的过程中可能会报如下的连接提示错误:  
> Unable to get the focused window from device 0820733287
> 
> 上述错误我们可以通过如下命令关闭重启adb一把即可解决：
> 
> λ adb kill-server  
> λ adb start-server\*  
> daemon not running. starting it now on port 5037 \*\*  
> daemon started successfully \*

最终呈现在我们眼前的视图效果如下所示：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/509229e6655815ef27c67c75396a07d3.png#pic_center)  
注意这里我们只是引出该工具，至于具体的使用的一些细节不是本篇博客的重点，大伙可以参见Android官方文档[使用Hierarchy Viewer分析您的布局](https://developer.android.google.cn/studio/profile/hierarchy-viewer)！

##### 1.1.2 AS下通过Layout Inspector查看Activity布局视图层级

如果说现阶段还在使用eclipse进行Android的开发代表的是老态龙钟的话，那么AS则是一颗冉冉升起的新星，代表着活力，对应的它也为我们提供了布局视图层级工具Layout Inspector，它的使用步骤也比较简单如下：

-   在连接的设备或模拟器上运行应用。
-   依次点击 Tools > Layout Inspector。

最终呈现在我们眼前的视图效果如下所示：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/7ba0767614877aae825e649c76a92c67.png#pic_center)

注意这里我们只是引出该工具，至于具体的使用的一些细节不是本篇博客的重点，大伙可以参见Android官方文档[使用布局检查器和布局验证工具调试布局](https://developer.android.google.cn/studio/debug/layout-inspector)以及博客[视图层级实时分析](https://www.jianshu.com/p/a8850e7cbac2)。

  

#### 1.2 Activity布局视图层级命令查看工具使用

  如果你对以上的安排还不满意，嫌麻烦的话，没有关系！Android为我们提供了很多命令，其中我最最喜欢的就是dumpsy命令，它非常的强大(这里不是随口一说，而是真的dumpsys命令很好很强大，你值得拥有)，提供了很多的扩展功能。通过它也可以查看我们的Activity的布局视图层级，使用方式如下:

```
XXX:/ # dumpsys activity top
TASK com.example.jnitest id=73
  ACTIVITY com.example.jnitest/.MainActivity ff7173 pid=11935
    Local Activity b238720 State:
      mResumed=false mStopped=true mFinished=false
      mChangingConfigurations=false
      mCurrentConfig={1.0 ?mcc?mnc [zh_CN] ldltr sw360dp w360dp h568dp 320dpi nrml port finger -keyb/v/h -nav/h s.5}
      mLoadersStarted=true
      FragmentManager misc state:
        mHost=android.app.Activity$HostCallbacks@40c78d9
        mContainer=android.app.Activity$HostCallbacks@40c78d9
        mCurState=3 mStateSaved=true mDestroyed=false
    ViewRoot:
      mAdded=true mRemoved=false
      mConsumeBatchedInputScheduled=false
      mConsumeBatchedInputImmediatelyScheduled=false
      mPendingInputEventCount=0
      mProcessInputEventsScheduled=false
      mTraversalScheduled=false      mIsAmbientMode=false
      android.view.ViewRootImpl$NativePreImeInputStage: mQueueLength=0
      android.view.ViewRootImpl$ImeInputStage: mQueueLength=0
      android.view.ViewRootImpl$NativePostImeInputStage: mQueueLength=0
    Choreographer:
      mFrameScheduled=false
      mLastFrameTime=29948713 (163658 ms ago)
    View Hierarchy:
      DecorView@ad14ee2[MainActivity]
        com.android.internal.widget.ActionBarOverlayLayout{92b04af V.E...... ........ 0,0-720,1184 #1020427 android:id/decor_content_parent}
          android.widget.FrameLayout{d4aeca8 V.E...... ........ 0,160-720,1184 #1020002 android:id/content}
            android.widget.LinearLayout{c443766 V.E...... ........ 0,0-720,1024}
          com.android.internal.widget.ActionBarContainer{ecc63f2 V.ED..... ........ 0,48-720,160 #1020428 android:id/action_bar_container}
            android.widget.Toolbar{41689f9 V.E...... ........ 0,0-720,112 #1020429 android:id/action_bar}
              android.widget.TextView{6db90ec V.ED..... ........ 32,29-170,83}
              android.widget.ActionMenuView{5cfbb97 V.E...... ........ 640,0-720,112}
                android.widget.ActionMenuPresenter$OverflowMenuButton{961ada2 VFED..CL. ........ 0,8-80,104}
            com.android.internal.widget.ActionBarContextView{580b887 G.E...... ......I. 0,0-0,0 #102042a android:id/action_context_bar}
        android.view.View{c74c352 V.ED..... ........ 0,1184-720,1280 #1020030 android:id/navigationBarBackground}
        android.view.View{4a65623 V.ED..... ........ 0,0-720,48 #102002f android:id/statusBarBackground}
    Looper (main, tid 1) {6f7c29e}
      (Total messages: 0, polling=false, quitting=false)

```

> 这里我们只需重点关注View Hierarchy显示的内容  
> 并且这里的0,0-720,1184表示的是Activity布局中控件的X,Y坐标值，以及长宽！

  

#### 1.3 Activity布局视图示意图

前面无论是工具的介绍，还是命令的介绍都是为了让读者能够了解Activity布局视图，从而为后面的源码分析打下基础，这点非常重要。这里我们简单的给出一个Activity的布局视图示意图！

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/eed682a366fe4b9ddcbe1e51515e421c.png#pic_center)

> 注意上述的布局示意图是在一切默认的情况下，没有加上任何WindowFeature和设置相关的flag标志！
> 
> 其中的状态栏背景通常被用来实现沉浸式状态栏，而导航栏背景则会在不同主题下呈现不同的背景色.
> 
> 并且上述的示意图会根据不同的WindowFeature和flags标志有所不同，这个最好在阅读源码时借助工具或者命令自己分析得出结论！

___

### 二. setContentView正式开始Activity布局加载

  前面的前期知识准备整了这么久，估计很多读者会有点不耐烦了(前戏这么久，还不入正题是不)，这不就来了吗！对于应用开发者来说布局的加载就是从Activity的onCrate()方法中调用setContentView()开始，当然我们也不能免俗的从此处开始了！

  

#### 2.1 setContentView(…)设置Activity布局

  通常在我们的Activity实例的onCreate()里面会通过如下方式调用setContentView()方法(当然这里只是为了演示使用)，如下：

```
@Override
protected void onCreate(Bundle savedInstanceState) {
super.onCreate(savedInstanceState);
setContentView(R.layout.activity_main);
}
```

> 这里有一点需要提醒读者的是通常在Activity的子类中，重写它的生命周期的相关方法一定不要忘记调用super它的父类方法，否则会抛出如下的异常错误:  
> android.util.SuperNotCalledException: Activity {com.example.jnitest/com.example.jnitest.MainActivity} did not call through to super.onCreate()
> 
> 上述也很好理解吗，因为必须调用父类的方法做一些初始化的操作，所以子类没有搭理老子，不尊敬长者就必须给点颜色看看不是！

这里的setContentView方法最终调用到了Activity中去了，我们一探究竟，如下:

```
//[Activity.java]
    public void setContentView(@LayoutRes int layoutResID) {
        getWindow().setContentView(layoutResID);
        initWindowDecorActionBar();//这里是ActionBar的相关处理，忽略
    }

    public Window getWindow() {
        return mWindow;
    }
```

这里getWindow()返回的是Acitivity类中的mWindow变量，就是我们前面博客重点分析的Activity在创建时一起创建的PhoneWindow对象(即我们的Activity的窗口)。并且我们可以看到Activity中很多关于View或者布局的相关操作都不是由Activity直接处理的而是交由mWindow窗口代为处理的，此时的mWindow就是我们Activity和View之间的代理和桥梁将二者之间关联起来了。

> 并且在后续的分析中我们会知道Activity通过setContentView将自定义布局View设置到了PhoneWindow上，而View通过WindowManagerImpl的addView()、removeView()、updateViewLayout()对View进行管理。

在继续往下源码分析之前我们来回顾下PhoneWindow的类图关系图，如下:

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9fb9b56f770884b3176112cc574bc69e.png#pic_center)

我们接着来看看实现类PhoneWindow中的setContentView()方法：

```
//[PhoneWindow.java]
    @Override
    public void setContentView(int layoutResID) {
        if (mContentParent == null) {//在Activity没有调用setContentView之前，它对应的Window中的mContentParent为NULL
            installDecor();//完成DecorView的创建工作和初始化mContentParent,详见2.2
        } else if (!hasFeature(FEATURE_CONTENT_TRANSITIONS)) {
            mContentParent.removeAllViews();
        }

        if (hasFeature(FEATURE_CONTENT_TRANSITIONS)) {//FEATURE_CONTENT_TRANSITIONS的属性，这里是Android的过渡动画相关机制，这里我们不再展开详述
            final Scene newScene = Scene.getSceneForLayout(mContentParent, layoutResID,
                    getContext());
            transitionTo(newScene);
        } else {
        // 这里将我们的自定义的布局文件inflate到mContentParent中
            // 那这里的mContentParent表示什么呢，是我们contentView的父布局
            mLayoutInflater.inflate(layoutResID, mContentParent);
        }
        mContentParent.requestApplyInsets();
        final Callback cb = getCallback();
        if (cb != null && !isDestroyed()) {
            cb.onContentChanged();
        }
        mContentParentExplicitlySet = true;
    }
```

> 这里插一嘴，分析上面的源码中涉及的布局之间的关系，如果有整不清楚的读者前面的介绍的工具和命令就可以排上用场了！

上面的setContentView方法，归纳起来核心的只有两点(简洁却并不简单，因为扩展开来会很多很多):

-   调用installDecor()方法，完成DecorView的创建工作和初始化mContentParent
-   接着通过LayoutInflater.inflate方法将将我们传入的资源文件转换为view树，装载到mContentParent中(对于LayoutInflater.inflate我想应用开发，特别是经常自定义View的开发者应该是非常的熟悉了),这个不是本篇博客的重点

  

#### 2.2 installDecor(…)开始DecorView的创建工作和初始化mContentParent

  在开始本节的分析前，我还是要不厌其烦的先来说说DecorView实例mDecor和ContentParent实例mContentParent，通过前面的类图我们可以看到都是PhoneWindow中两个成员变量，如下:

```
    // This is the top-level view of the window, containing the window decor.
    private DecorView mDecor;
    
    // This is the view in which the window contents are placed. It is either
    // mDecor itself, or a child of mDecor where the contents go.
    ViewGroup mContentParent;
```

从字面意思描述来看可以概括为 mDector是窗体的顶级视图，mContentParent 是放置窗体内容的容器，也就是我们 setContentView() 时所加入的 View 视图树，从包含关系来看可以如下表示：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/2b01cf2c8afcf1b6e752bcea718582e6.png#pic_center)

> 上述的mDecorContentParent比较特别，它在不同的Activity的feature和sytel下有可能有，有可能没有，需要根据实际情况来确定。

并且虽然随着Android的版本的迭代，该方法以及相关调用方法中有关 Feature/Style/Transition等发生了很大的细节方面的变化(当然这种变化是Android有意而为之，无论是处于Android的优化亦或是谷歌的工程师为了KPI考核)，但是它的整体框架还是基本保持不变的，我们来详细分析一把！

```
//[PhoneWindow.java]
    private void installDecor() {
        mForceDecorInstall = false;
        if (mDecor == null) {//在没有进行相关的初始化之前mDecor肯定为null
            mDecor = generateDecor(-1);//创建DecorView,可以认为是我们Activity的根布局，详见章节2.3
            mDecor.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
            mDecor.setIsRootNamespace(true);
            if (!mInvalidatePanelMenuPosted && mInvalidatePanelMenuFeatures != 0) {
                mDecor.postOnAnimation(mInvalidatePanelMenuRunnable);
            }
        } else {
            mDecor.setWindow(this);//将mDecor和Windo关联起来
        }  
        if (mContentParent == null) {
            mContentParent = generateLayout(mDecor);//这个也是重点，根据Activity的相关配置信息加载具体的mContentParent父布局，详见章节2.4

            // Set up decor part of UI to ignore fitsSystemWindows if appropriate.
            mDecor.makeOptionalFitsSystemWindows();

            final DecorContentParent decorContentParent = (DecorContentParent) mDecor.findViewById(
                    R.id.decor_content_parent);

//注意这里为啥会判断是否为null呢，因为根据Activity的不同配置信息加载的Activity的
            if (decorContentParent != null) {   
            mDecorContentParent = decorContentParent;
            ...
            mDecorContentParent.setUiOptions(mUiOptions);
....
            }else {
...
            }
            ...
            /*
            这里省略了与分析无关的代码，主要是对feature和style属性的一些判断和设置
            并不是说明它们不重要，只是影响我们对整体框架的理解罢了
            Activity呈现出五彩缤纷的世界，离不开各种feature和style的协助
            */
        } 
    }

```

由于此时Activity是第一次调用setContentView方法，所以此时的mDecor 和mContentParent肯定为null了，此时在installDecor()方法中会开始DecorView的创建工作和初始化mContentParent，并且有可能的会还会初始化构建加载decorContentParent (这个需要根据实际的场景来决定，如果是采用Activity的默认配置，不设置任何的feature和style是有的)！

> 这里省略了与分析无关的代码，主要是对feature和style属性的一些判断和设置  
> 并不是说明它们不重要，只是影响我们对整体框架的理解罢了  
> Activity呈现出五彩缤纷的世界，离不开各种feature和style的协助

  在开始详细分析mDecor和mContentParent之前，我们先来先看看decorContentParent的加载流程(这里本应该放在前两者分析之后，但是为了博客编写所以放在了这里)：

```
            final DecorContentParent decorContentParent = (DecorContentParent) mDecor.findViewById(
                    R.id.decor_content_parent);
```

decor\_content\_parent是不是有点眼熟，在我们的1.2的章节中有看到了，从前面介绍的Activity的布局层级关系可以看到，它是我们的mDecor的直接子布局！这里我们应该怎么确定我们的mDecor的直接布局文件呢(在generateLayout中会进行选择)，这里我们可以通过如下的方式确定:

```
XXX$ grep -nr "decor_content_parent"  frameworks/base/core/res/
frameworks/base/core/res/res/layout/screen_action_bar.xml:23:    android:id="@+id/decor_content_parent"
frameworks/base/core/res/res/layout/screen_toolbar.xml:23:    android:id="@+id/decor_content_parent"
frameworks/base/core/res/res/values/symbols.xml:58:  <java-symbol type="id" name="decor_content_parent" />
```

可以看到这里有两个xml中定义了对应的id，那具体是那个呢，其实具体我们这里加载的是screen\_toolbar.xml文件，我们来看看：

> 注意为什么最终选择的是screen\_action\_bar.xml文件呢，这里我们是通过工具和命令来配合使用来确认的，当然也可以直接分析源码！

这里我们来看下screen\_toolbar.xml的布局文件，如下：

```
//[screen_toolbar.xml]
<com.android.internal.widget.ActionBarOverlayLayout
    xmlns:android="http://schemas.android.com/apk/res/android"
    android:id="@+id/decor_content_parent"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:splitMotionEvents="false"
    android:theme="?attr/actionBarTheme">
    <FrameLayout android:id="@android:id/content"
                 android:layout_width="match_parent"
                 android:layout_height="match_parent" />
    <com.android.internal.widget.ActionBarContainer
        android:id="@+id/action_bar_container"
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:layout_alignParentTop="true"
        style="?attr/actionBarStyle"
        android:transitionName="android:action_bar"
        android:touchscreenBlocksFocus="true"
        android:gravity="top">
        <Toolbar
            android:id="@+id/action_bar"
            android:layout_width="match_parent"
            android:layout_height="wrap_content"
            android:navigationContentDescription="@string/action_bar_up_description"
            style="?attr/toolbarStyle" />
        <com.android.internal.widget.ActionBarContextView
            android:id="@+id/action_context_bar"
            android:layout_width="match_parent"
            android:layout_height="wrap_content"
            android:visibility="gone"
            style="?attr/actionModeStyle" />
    </com.android.internal.widget.ActionBarContainer>
</com.android.internal.widget.ActionBarOverlayLayout>
```

  这里我们通过工具和命令分别验证一下，我们先来个布局查看器验证一下(这里为了展示的直观我们选择Hierachy View，真的我不是老古董不选择AS，是为了更加的直观演示而已)：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/613a6357e2216f46ed4679b2f3aa7e06.png#pic_center)  
通过工具可以看到 ActionBarOverlayLayout的布局文件的id正是 decor\_content\_parent ，并且我们也可以看到布局文件中的每个View节点的名称和 id 都与Hierarchy Viewer视图中的一一对应，再看其中的FrameLayout的id为content，我们自然而然的猜测它就是我们根布局 LinearLayout的父布局!

当然也可以通过命令查看，更加的直观简介明了:

```
    View Hierarchy:
      DecorView@91d2db0[MainActivity]
        com.android.internal.widget.ActionBarOverlayLayout{ffc4ec5 V.E...... ........ 0,0-720,1184 #1020427 android:id/decor_content_parent}
          android.widget.FrameLayout{3d5c3e6 V.E...... ........ 0,160-720,1184 #1020002 android:id/content}
            android.widget.LinearLayout{1edcbd4 V.E...... ........ 0,0-720,1024}
          com.android.internal.widget.ActionBarContainer{e45d140 V.ED..... ........ 0,48-720,160 #1020428 android:id/action_bar_container}
            android.widget.Toolbar{4c1a1f V.E...... ........ 0,0-720,112 #1020429 android:id/action_bar}
              android.widget.TextView{12463ca V.ED..... ........ 32,29-170,83}
              android.widget.ActionMenuView{78fd3ed V.E...... ........ 640,0-720,112}
                android.widget.ActionMenuPresenter$OverflowMenuButton{a100a70 VFED..CL. ........ 0,8-80,104}
            com.android.internal.widget.ActionBarContextView{49a925d G.E...... ......I. 0,0-0,0 #102042a android:id/action_context_bar}
        android.view.View{332efa0 V.ED..... ........ 0,1184-720,1280 #1020030 android:id/navigationBarBackground}
        android.view.View{772f759 V.ED..... ........ 0,0-720,48 #102002f android:id/statusBarBackground}
```

> 这里细心的读者会发现一点就是我们的screen\_toolbar.xml中并没有navigationBarBackground和statusBarBackground的子View，那么它是怎么来的呢！这里留一个悬念给读者自行阅读源码查找答案！
> 
> 其实也没有神秘的是mDecor通过动态方式加载的！

  

#### 2.3 创建DecorView

  感觉前面有点啰嗦啊，但是为了将整个流程讲透彻，明白不得而已为之(当然如果是想碎片化时间获取相关知识，可能这篇博客不适合您！)!好了我们接着继续分析看看是怎么创建的：

```
//[PhoneWindow.java]
    protected DecorView generateDecor(int featureId) {
        Context context;
        if (mUseDecorContext) {//注意此时在PhoneWindow被构建的时候，mUseDecorContext已经被初始化为true了，可以回看前面的博客
            Context applicationContext = getContext().getApplicationContext();
            if (applicationContext == null) {
                context = getContext();
            } else {
                context = new DecorContext(applicationContext, getContext().getResources());
                if (mTheme != -1) {
                    context.setTheme(mTheme);
                }
            }
        } else {
            context = getContext();
        }
        return new DecorView(context, featureId, this, getAttributes());
    }
```

这个没什么可多说的，就是依据我们Actiiviyt相关的feature和上下文当相关信息直接new出一个实例对象而已，这里我们只需要知道的是DecorView是FrameLayout子类(那么就肯定具有View的相关功能了)，它的简单类图继承关系如下:

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/ae1606377d1d68a3d5b35295efa6ded0.png#pic_center)

  

#### 2.4 初始化mContentParent并加载contentView

  开心否，一大块硬骨头终于被我们啃下来了，我们继续开啃generateLayout()方法，它比我们前面的generateDecor()方法复杂那么一点一点，不过没有关系一步步分解，蚂蚁都能把大象干倒不是！

```
//[PhoneWindow.java]
    protected ViewGroup generateLayout(DecorView decor) {
//获取窗口的style
        TypedArray a = getWindowStyle();

...//省略很多关于style处理的源码，各位感兴趣的可以自行阅读

        int layoutResource;
        int features = getLocalFeatures();
//根据feathres给layoutResource赋值
if ((features & (1 << FEATURE_SWIPE_TO_DISMISS)) != 0) {
...
}else if(...){
...
}else if ((features & (1 << FEATURE_NO_TITLE)) == 0) {
            if (mIsFloating) {

            } else if ((features & (1 << FEATURE_ACTION_BAR)) != 0) {//我们的加载的就是此分支
                layoutResource = a.getResourceId(
                        R.styleable.Window_windowActionBarFullscreenDecorLayout,
                        R.layout.screen_action_bar);
            } else {
...
            }
        } else if ((features & (1 << FEATURE_ACTION_MODE_OVERLAY)) != 0) {
...
        } else {
            layoutResource = R.layout.screen_simple;
        }

//mDecor要改变的标记位
        mDecor.startChanging();
//此处将layoutResource布局文件解析成 View 添加到了 DecorView 之中
        mDecor.onResourcesLoaded(mLayoutInflater, layoutResource);


//通过findViewById给contentParent赋值
ViewGroup contentParent = (ViewGroup)findViewById(ID_ANDROID_CONTENT);
        if (contentParent == null) {
            throw new RuntimeException("Window couldn't find content container view");
        }
        ...
        //mDecor改变结束的标记位
mDecor.finishChanging();

        return contentParent;
    }
```

此方法的代码逻辑比较多(在这里我将其中的一些非关键的代码给省略了)，上述方法整体可以分为三部分:

-   根据不同的主题样式的属性值选择不同的布局文件，然后将其ID赋给layoutResource，最后加载进mDecor视图中(在View的树状结构下，DecorView即是整个Window显示的视图的根节点)。

> 并且此处蕴含着一个知识点就是我们要在setContentView() 之前执行 requestWindowFeature()才可以生效的原因，过时不候吗

-   接着在的mDecor以及子节点中查找id为ID\_ANDROID\_CONTENT的一块区域作为mContentParent变量用于加载用户Activity的布局。(与mContentParent平级的视图有ActionBar视图和Title的视图等，这个根据实际的layoutResource 用户自行分析)。

> 这里有第一点我们需要注意的是，无论layoutResource的id或者说对应的xml文件怎么变，它一定要有一个id为ID\_ANDROID\_CONTENT的字段，否则会抛出异常。  
> 这个也可以理解，如果没有这个contentParent 我们怎么加载我们的Activity对应的布局呢

-   最后将找到的contentParent返回给最外层PhoeWindow的成员mContentParent.

这里我们对mDecor.onResourcesLoaded来简单复盘分析下！

##### 2.4.1 mDecor.onResourcesLoaded()将layoutResource布局文件解析成View添加到DecorView之中

```
//[DecorView.java]
    void onResourcesLoaded(LayoutInflater inflater, int layoutResource) {
        mStackId = getStackId();

        if (mBackdropFrameRenderer != null) {
            loadBackgroundDrawablesIfNeeded();
            mBackdropFrameRenderer.onResourcesLoaded(
                    this, mResizingBackgroundDrawable, mCaptionBackgroundDrawable,
                    mUserCaptionBackgroundDrawable, getCurrentColor(mStatusColorViewState),
                    getCurrentColor(mNavigationColorViewState));
        }
/* 
英文翻译过来的意思是Decor的标题View, 这个类表示用于控制自由格式窗口的特殊屏幕元素环境
这个比较特殊，和多窗口有关系
对于多窗口的Activity这两这个子View(mContentRoot mDecorCaptionView都有，
对于没有多窗口的，则只有mContentRoot)
*/
        mDecorCaptionView = createDecorCaptionView(inflater);//关于这个方法就不展开分析了，感兴趣的读者自行分析
        final View root = inflater.inflate(layoutResource, null);
//若不为null则先添加mDecorCaptionView, 再向mDecorCaptionView中添加root
        if (mDecorCaptionView != null) {
            if (mDecorCaptionView.getParent() == null) {
                addView(mDecorCaptionView,
                        new ViewGroup.LayoutParams(MATCH_PARENT, MATCH_PARENT));
            }
 //将从系统的资源XML文件得到的root加到这个mDecorCaptionView中
            mDecorCaptionView.addView(root,
                    new ViewGroup.MarginLayoutParams(MATCH_PARENT, MATCH_PARENT));
        } else {

            // Put it below the color views.
//若mDecorCaptionView为null，则直接将root添加到DecorView中
            addView(root, 0, new ViewGroup.LayoutParams(MATCH_PARENT, MATCH_PARENT));
        }
//将root强制转换成ViewGroup，传递给mContentRoot
        mContentRoot = (ViewGroup) root;
        initializeElevation();
    }
```

这里我们可以看到 mDecor.onResourcesLoaded方法主要做了如下的三件事情：

-   通过 LayoutInflate装载器来创建这个layoutResource 的View实例对象root
-   若mDecorCaptionView nul 则先添 mDecorCaptionView, 再向mDecorCaptionView添加root  
    若mDecorCaptionView为null,则直接将root加到DecorView中

> 此处的mDecorCaptionView在Android 5版本我没有见到，这个特性应该是Android版本新加的

-   将加载系统资源文件创建的 View 的实例赋给 mContentRoot

通过上述源码的分析我们可以知道, 系统创建的 DecorView中只会存在一个子 View(注意是子View而不是孙View)，如下：

-   mDecorCaptionView(它内部也包含了mContentRoot)，此种情况比较少见，我们只需要了解
-   mContentRoot(我们绝大部分的Activiyt布局层级属于这种)

##### 2.4.2 generateLayout小结

如上就是整个generateLayout()的流程，在generateLayout执行完之后，会返回setContentView()方法中到继续执行执行mLayoutInflater.inflate(layoutResID, mContentParent)方法将用户传入的布局转化为view再加入到mContentParent上。

  

#### 2.5 setContentView()小结

  至此setContentView()的整个流程到这里就告一段落了了，Activity加载布局的初始阶段也宣告完成(放在整个Android的加载，绘制，渲染中真的仅仅是一个开始)！而从小的来说我们Activity的窗口对应的mDecore也创建完成，我们的自定义布局也已经加载OK了，这里我们从时序图上来整体概括一下！

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/dc880edeca8f888f643fbd3f0f59e9b7.png)  
如上是时序图的调用搞懂了，那么我们的Activity的布局层级示意图得来也就完全不费功夫了，再来一个锦上添花，看看我们的Activity布局层级关系图如下:

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/459808ae8e66ceb58551ee584855cbca.png)

___

### 三. 继续handleResumeActivity()处理流程

  通过前面章节setContentView的努力我们的Activiyt对应的DecorView也创建完成，自定义布局也加载到DecorView中去了，Activity加载布局的初始阶段也宣告完成但是这距离将上述相关的各种VIew呈现在我们的Android设备终端上还有很远的路要走，而我们也必须继续前进的脚步一步一个脚印的进行分析，通过前面的博客[Android四大组件之Activity布局加载流程实现之窗口以及窗口管理器的创建](https://blog.csdn.net/tkwxty/article/details/109597570)分析我们知道了在AcitityThread.handleLauncherActivity()方法的代码执行完performLaunchAcitity()创建好Acitivity并完成onCreate()方法的调用后，便会执行到handleResumeActivity()方法，该方法代码方法如下：

```
//[ActivityThread.java]
    final void handleResumeActivity(IBinder token,
            boolean clearHide, boolean isForward, boolean reallyResume, int seq, String reason) {
        ActivityClientRecord r = mActivities.get(token);

...

        //该方法执行过程中会调用到Acitity的onResume()方法，
        //返回的ActivityClientRecord对象对应的已经创建好并且初始化好的Activity
        r = performResumeActivity(token, clearHide, reason);

        if (r != null) {
            final Activity a = r.activity;//得到前面创建好的Activity
/*
           判断该Acitivity是否可见，mStartedAcitity记录的是一个Activity是否还处于启动状态
　　　　　　如果还处于启动状态则mStartedAcitity为true，表示该activity还未启动好，则该Activity还不可见

注意mStartedActivity的值在performLaunchActivity中会被至于false
*/
            boolean willBeVisible = !a.mStartedActivity;

/*
此处再次向AMS查询，目标Activity是否应该可见
*/
            if (!willBeVisible) {
                try {
                    willBeVisible = ActivityManagerNative.getDefault().willActivityBeVisible(
                            a.getActivityToken());
                } catch (RemoteException e) {
                    throw e.rethrowFromSystemServer();
                }
            }
            if (r.window == null && !a.mFinished && willBeVisible) {
/*
获取前面为目标Activity创建好的::
窗口Window
窗口管理器
DecorView

*/
                r.window = r.activity.getWindow();
                View decor = r.window.getDecorView();
                decor.setVisibility(View.INVISIBLE);
                ViewManager wm = a.getWindowManager();
                WindowManager.LayoutParams l = r.window.getAttributes();
                a.mDecor = decor;
                l.type = WindowManager.LayoutParams.TYPE_BASE_APPLICATION;
                l.softInputMode |= forwardBit;
//PreserverWindow，一般指主题换了或者configuration变了情况下的Acitity快速重启机制
                if (r.mPreserveWindow) {
...//此处忽略
                }

//调用了WindowManagerImpl的addView方法
                if (a.mVisibleFromClient && !a.mWindowAdded) {
                    a.mWindowAdded = true;
                    wm.addView(decor, l);//这个是重点
                }

            } else if (!willBeVisible) {
...
            }

...
            if (!r.activity.mFinished && willBeVisible
                    && r.activity.mDecor != null && !r.hideForNow) {
。。。
//设置目标Activity可见
                if (r.activity.mVisibleFromClient) {
                    r.activity.makeVisible();
                }
            }

            if (!r.onlyLocalRequest) {
                r.nextIdle = mNewActivities;
                mNewActivities = r;
                if (localLOGV) Slog.v(
                    TAG, "Scheduling idle handler for " + r);
                Looper.myQueue().addIdleHandler(new Idler());
            }
            r.onlyLocalRequest = false;

            //此处是通知AMS目标Activity已经执行完onResume完毕了
            if (reallyResume) {
                try {
                    ActivityManagerNative.getDefault().activityResumed(token);
                } catch (RemoteException ex) {
                    throw ex.rethrowFromSystemServer();
                }
            }

        } else {
...
        }
    }
```

handleResumeActivity方法逻辑有如下几点:

-   执行目标Activity的onResume()方法
    
-   获取前面为目标Activity构建的相关的成员变量信息，譬如对应的窗口啊，窗口管理器啊，DecorView等，它们之间对应的关系如下所示  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5321284826579531cb65dd85bf224c21.png)
    
-   接着调用WindowManagerImpl实例对象的addView()方法对Activity对应的DecorView做进一步的处理
    
-   最后将目标Activiyt设置为可见
    

这里我们需要重点来关注wm.addView()方法，该方法中的decor参数为Acitity对应的Window中的视图DecorView，wm为在创建PhoneWindow是创建的WindowManagerImpl对象，该对象的addView方法实际调用到的是它的mGlobal成员变量指向的是单例对象WindowManagerGlobal的addView方法（这个在前面博客已经有提到了）。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/13e15864e2760931902555f7e4afd65c.png)

至此Activity布局加载的的实现就基本完结了，我们的Activity对应的视图对象也已经创建完成了。在这里我们重点介绍了通过setContentView方法将自定义布局视图layout设置到了PhoneWindow的DecorView上面,而后又将继续会通过WindowManagerImpl的addView()、removeView()、updateViewLayout()对DecorView视图进行相关的管理进而完成绘制渲染和显示的目的。

> 如果说Activity的启动更多的是通过AMS的交互来完成的，那么Activiyt的显示更多的就是和WMS的交互来完成的，这个在我们的系列博客后续分析中会有体现的。

虽然Activity布局加载到这里就已经完结了，但是这在Android应用程序窗口整个的设计流程中仅仅是一个开始而已！这里限于篇幅关于关于Android应用程序窗口设计系列博客之Activity布局加载的的实现就到这里了！

___

### 写在最后

  [Android应用程序窗口设计之布局加载的的实现](https://editor.csdn.net/md?articleId=110005502)到这里就告一段落了，此时我们已经完成应用程序Activiyt相关DecoreView的创建以及对应的contentView的加载，在接下来我将会重点分析应用程序Activiyt对应的DecorView是如何添加到窗口管理器之中，完成相关绘制并将相关的窗口添加到WMS服务中到最终显示在我们的终端界面上面的，如果有感兴趣的亲请期待。好了，青山不改绿水长流先到这里了。如果本博客对你有所帮助，麻烦关注或者点个赞，如果觉得很烂也可以踩一脚！谢谢各位了！！