> 作者：孝之请回答

## 前言

相信做应用层业务开发的同学，都跟我一样，对Framework”深恶痛绝“。确实如此，如果平日里都在做应用层的开发，那么基本上我们很少会去碰Framework的知识。但生活所迫，面试总是逃不过这一关的，所以作为一名合格的打工人，我们还是必须得具备一些Framework的基本知识。

网上有很多文章或浅或深地讲了Framework，有很多源码大家肯定也都看过不止一两次，但过一段时间就忘记了。我这篇文章将会从应用层开发的视角，罗列下我认为需要掌握的Framework基本知识。为了更方便大家去记忆与巩固，我更多的是从流程上进行讲解，而不会对源码进行非常深入的解读，文章会比较长，大家可以当做一个知识小册，根据目录针对性地进行浏览（基于Android 8.0/9.0）。

那话不多说，我们冲！

## 一、系统启动流程

## [Zygote进程](https://zhida.zhihu.com/search?content_id=231089876&content_type=Article&match_order=1&q=Zygote%E8%BF%9B%E7%A8%8B&zhida_source=entity)

Zygote进程是非常重要的一个进程，它负责Android系统中虚拟机（DVM或者ART）的创建、应用进程的创建以及[SystemServer进程](https://zhida.zhihu.com/search?content_id=231089876&content_type=Article&match_order=1&q=SystemServer%E8%BF%9B%E7%A8%8B&zhida_source=entity)的创建。

系统在开机后会启动init进程，init进程进而会fork出Zygote进程。Zygote进程在启动时，主要做了这么几件事情：

1.  创建虚拟机。当Zygote以fork自身的方式去创建应用进程和SystemServer进程时，它们就能拿到虚拟机的一个副本。
2.  作为Socket服务端监听[AMS](https://zhida.zhihu.com/search?content_id=231089876&content_type=Article&match_order=1&q=AMS&zhida_source=entity)请求创建进程
3.  启动SystemServer进程

## SystemServer进程

SystemServer进程是我们学习Framework中最重要的一个系统级进程，我们熟知的很多服务（AMS、[WMS](https://zhida.zhihu.com/search?content_id=231089876&content_type=Article&match_order=1&q=WMS&zhida_source=entity)、[PMS](https://zhida.zhihu.com/search?content_id=231089876&content_type=Article&match_order=1&q=PMS&zhida_source=entity)等）都属于它提供的一个服务，是与我们APP进程通信最频繁的进程。

SystemServer进程在启动时，主要做了这么几件事情：

1.  启动Binder线程池，为进程间通信做准备
2.  启动各种系统服务，比如AMS、WMS、PMS

## Launcher进程

Launcher进程实际上就是我们的桌面进程，熟悉它的同学都知道一页桌面本质上就是一个Grid类型的RecylerView，那它是怎么创建的呢？

这就是系统启动的最后一步，当SystemServer进程启动了AMS进程后，AMS进程会启动Launcher进程。Launcher进程通过与PMS进程通信，获取到机器上所有的安装包信息后，将数据（APP图标、APP名称等）渲染到桌面上。

## 小结：

系统启动流程可以用下图概括：

![](https://pic4.zhimg.com/v2-d9fba8389c043713c2c02c414c745123_1440w.jpg)

## 二、应用进程启动流程

前面我们讲了SystemServer进程与桌面进程是如何启动的，等它们启动好了之后，就可以从桌面启动我们的应用进程了。

在桌面点击APP图标后，Launcher进程将会向AMS请求创建APP的启动页面。AMS识别到APP进程不存在之后，就会用Socket的方式向Zygote进程请求创建APP进程。Zygote通过fork的方式创建APP进程之后，会反射调用`android.app.[ActivityThread](https://zhida.zhihu.com/search?content_id=231089876&content_type=Article&match_order=1&q=ActivityThread&zhida_source=entity)`的`main()`方法，初始化ActivityThread。

ActivityThread，可以理解为管理APP主线程任务的一个类。在`main()`方法中初始化时，我们会初始化主线程的消息循环，从而接收主线程需要处理的所有消息，保证UI在主线程的渲染（消息机制后文会详细介绍）。

```
public static void main(String[] args) {
    ...
    ActivityThread thread = new ActivityThread();
    thread.attach(false, startSeq);//注释1

    if (sMainThreadHandler == null) {
        sMainThreadHandler = thread.getHandler(); //注释2
    }

    if (false) {
        Looper.myLooper().setMessageLogging(new
                LogPrinter(Log.DEBUG, "ActivityThread"));
    }

    // End of event ActivityThreadMain.
    Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
    Looper.loop();
}

private void attach(boolean system, long startSeq) {
    ...
    if (!system) {
        ...
        final IActivityManager mgr = ActivityManager.getService();
        try {
            mgr.attachApplication(mAppThread, startSeq); // 注释3
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
        ...
    } else {
        ...
        try {
            mInstrumentation = new Instrumentation();
            mInstrumentation.basicInit(this);
            ContextImpl context = ContextImpl.createAppContext(
                    this, getSystemContext().mPackageInfo);
            mInitialApplication = context.mPackageInfo.makeApplication(true, null);
            mInitialApplication.onCreate();
        } catch (Exception e) {
            throw new RuntimeException(
                    "Unable to instantiate Application():" + e.toString(), e);
        }
    }
    ...
}
```

可以看到注释2处会创建主线程Handler，这里的Handler类名为`H`，属于ActivityThread的内部类。后面会多次提到它。

注释1处调用`attach`方法时，传入的`system参数`为false，所以是会到注释3处的代码的，这里又回到了AMS，调用AMS的`attachApplication`方法。`attachApplication`方法又会一直调用到`ApplicationThread`的`bindApplication`方法，最终一直到`ActivityThread.handleBindApplication`方法：

```
private void handleBindApplication(AppBindData data) {
    ...
    try {
        final ClassLoader cl = instrContext.getClassLoader();
        // 注释1
        mInstrumentation = (Instrumentation)
            cl.loadClass(data.instrumentationName.getClassName()).newInstance();
    } catch (Exception e) {
        throw new RuntimeException(
            "Unable to instantiate instrumentation "
            + data.instrumentationName + ": " + e.toString(), e);
    }

    final ComponentName component = new ComponentName(ii.packageName, ii.name);
    mInstrumentation.init(this, instrContext, appContext, component,
            data.instrumentationWatcher, data.instrumentationUiAutomationConnection);
    ...
    
    try {
        ...
        try {
            // 注释2
            app = data.info.makeApplication(data.restrictedBackupMode, null);
            ...
            if (!data.restrictedBackupMode) {
                if (!ArrayUtils.isEmpty(data.providers)) {
                    // 注释3
                    installContentProviders(app, data.providers);
                    ...
                }
            }
            // 注释4
            mInstrumentation.onCreate(data.instrumentationArgs);
        } catch (Exception e) {
            throw new RuntimeException(
                "Exception thrown in onCreate() of "
                + data.instrumentationName + ": " + e.toString(), e);
        }
        try {
            // 注释5
            mInstrumentation.callApplicationOnCreate(app);
        } catch (Exception e) {
            if (!mInstrumentation.onException(app, e)) {
                throw new RuntimeException(
                  "Unable to create application " + app.getClass().getName()
                  + ": " + e.toString(), e);
            }
        }
    }
    ...
}
```

注释1处会初始化`Instrumentation`，Instrumentation类非常重要，后文会介绍到。这里主要是用它在注释2、注释4、注释5处创建Application并调用Application的onCreate生命周期。创建Application时会先调用Application的`attachBaseContext`方法。另外，**注释3处我们可以看到，随着应用进程的启动，ContentProviders也会被启动**。

## 小结：

应用进程启动流程可以用下图概括：

![](https://pica.zhimg.com/v2-ea60d779544d38470687b4ec7638aa66_1440w.jpg)

## 三、Activity启动过程

上一节我们已经将APP进程成功启动，但我们的页面还没有起来，这一节我们就讲一下Activity是如何启动的。

让我们回忆一下，上一节最开始是Launcher进程请求AMS创建APP的启动页面，那么Launcher进程的桌面实际上也是一个Activtiy，它启动我们的启动页面，也是调用的Activity#startActivity()方法。一层层进去后，我们可以发现，实际上是调用的`Instrumentation`的`execStartActivity()`方法：

```
#Activity

public void startActivityForResult(@RequiresPermission Intent intent, int requestCode,
        @Nullable Bundle options) {
    if (mParent == null) { //注释1
        options = transferSpringboardActivityOptions(options);
        Instrumentation.ActivityResult ar =
            mInstrumentation.execStartActivity(
                this, mMainThread.getApplicationThread(), mToken, this,
                intent, requestCode, options);  //注释2
        ……
    } else {
        if (options != null) {
            mParent.startActivityFromChild(this, intent, requestCode, options);
        } else {
            // Note we want to go through this method for compatibility with
            // existing applications that may have overridden it.
            mParent.startActivityFromChild(this, intent, requestCode);
        }
    }
}
```

注释1处可以看到，mParent代表着上一个页面，打开启动页面时mParent为null，调用注释2处Instrumentation的execStartActivity()方法。 Instrumentation不止执行startActivity，它还负责了所有Activity生命周期的调用：

![](https://pic3.zhimg.com/v2-32b9c4f626017545653baf472be55a06_1440w.jpg)

但它也不是真正的执行者，它只是包装了一下，为什么要这样包装呢？个人理解是因为Instrumentation需要对这些行为加一下监控，它的成员变量`mActivityMonitors`就是这个作用。

我们再进去看Instrumentation的`execStartActivity()`方法：

```
public ActivityResult execStartActivity(
        Context who, IBinder contextThread, IBinder token, Activity target,
        Intent intent, int requestCode, Bundle options) {
    ……
    try {
        intent.migrateExtraStreamToClipData();
        intent.prepareToLeaveProcess(who);
        int result = ActivityManager.getService()
            .startActivity(whoThread, who.getBasePackageName(), intent,
                    intent.resolveTypeIfNeeded(who.getContentResolver()),
                    token, target != null ? target.mEmbeddedID : null,
                    requestCode, 0, null, options); //注释1
        checkStartActivityResult(result, intent);
    } catch (RemoteException e) {
        throw new RuntimeException("Failure from system", e);
    }
    return null;
}
```

注释1处实际上是获取了`ActivityManager.getService()`去调用的startActivity。那这个`ActivityManager.getService()`又是何方神圣呢？再往下走

```
private static final Singleton<IActivityManager> IActivityManagerSingleton =
        new Singleton<IActivityManager>() {
            @Override
            protected IActivityManager create() {
                final IBinder b = ServiceManager.getService(Context.ACTIVITY_SERVICE);//注释1
                final IActivityManager am = IActivityManager.Stub.asInterface(b);
                return am;
            }
        };
```

注释1处我们可以看到`ActivityManager.getService()`最终拿到的就是`IActivityManager`。这段代码采用的是[AIDL](https://zhida.zhihu.com/search?content_id=231089876&content_type=Article&match_order=1&q=AIDL&zhida_source=entity)（不了解的同学可以自行学习一下），拿到AMS在APP进程的代理对象`IActivityManager`。那么最终调用的startActivity也就是AMS的startActivity了。

AMS在startActivity的过程中也会进行一个比较长的链路，主要是校验权限、处理ActivityRecord、Activity任务栈等，这里不细说，最终会调用到`app.thread.scheduleLaunchActivity`方法。`app.thread`实际上是`IApplicationThread`，跟之前的IActivityManager一样，它也是一个代理对象，代理的是app进程的`ApplicationThread`。`ApplicationThread`属于ActivityThread的内部类，我们可以看它的`scheduleLaunchActivity`方法：

```
@Override

public final void scheduleLaunchActivity(Intent intent, IBinder token, int ident,

    ActivityInfo info, Configuration curConfig, Configuration overrideConfig,

    CompatibilityInfo compatInfo, String referrer, IVoiceInteractor voiceInteractor,

    int procState, Bundle state, PersistableBundle persistentState,

    List<ResultInfo> pendingResults, List<ReferrerIntent> pendingNewIntents,

    boolean notResumed, boolean isForward, ProfilerInfo profilerInfo) {

    updateProcessState(procState, false);

    ActivityClientRecord r = new ActivityClientRecord(); // 注释1

    r.token = token;

    r.ident = ident;

    ...

    updatePendingConfiguration(curConfig);

    sendMessage(H.LAUNCH_ACTIVITY, r); // 注释2

}
```

注释1处会将启动Activity的参数封装成ActivityClientRecord。注释2处发送消息给`H`。之所以发给`H`是因为ApplicationThread本身是一个Binder对象，它执行`scheduleLaunchActivity`时处于Binder线程中，所以我们需要通过`H`转换到主线程中来。

`H`接收到`LAUNCH_ACTIVITY`消息后，会调用`handleLaunchActivity`方法：

```
private void handleLaunchActivity(ActivityClientRecord r, Intent customIntent, String reason) {

    ...

    Activity a = performLaunchActivity(r, customIntent);//注释1

    if (a != null) {

        r.createdConfig = new Configuration(mConfiguration);

        reportSizeConfigurations(r);

        Bundle oldState = r.state;

        handleResumeActivity(r.token, false, r.isForward,//注释2

        !r.activity.mFinished && !r.startsNotResumed, r.lastProcessedSeq, reason);

    if (!r.activity.mFinished && r.startsNotResumed) {
        ...

        performPauseActivityIfNeeded(r, reason);//注释3

    } else {

       ...

    }

}
```

看注释2与注释3处的代码，可以联想到它们必然与Activity生命周期有关，并且这里也解释了为什么上一个页面的onPause生命周期是在下一个页面的onResume之后调用的。注释1处`performLaunchActivity`方法很重要，我们来看一下：

```
private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {

...

    try {
        // 注释1

        java.lang.ClassLoader cl = appContext.getClassLoader();

        activity = mInstrumentation.newActivity(

        cl, component.getClassName(), r.intent);

        ...

    } catch (Exception e) {

        ...

    }

    try {

        Application app = r.packageInfo.makeApplication(false, mInstrumentation); // 注释2

        ...
        // 注释3

        activity.attach(appContext, this, getInstrumentation(), r.token,

        r.ident, app, r.intent, r.activityInfo, title, r.parent,

        r.embeddedID, r.lastNonConfigurationInstances, config,

        r.referrer, r.voiceInteractor, window, r.configCallback);

        ...

        int theme = r.activityInfo.getThemeResource();

        if (theme != 0) {

            activity.setTheme(theme); // 注释4

        }

        activity.mCalled = false;
        
        // 注释5
        if (r.isPersistable()) {

            mInstrumentation.callActivityOnCreate(activity, r.state, r.persistentState);

        } else {

            mInstrumentation.callActivityOnCreate(activity, r.state);

        }

        ...
        // 注释6

        if (!r.activity.mFinished) {

            activity.performStart();

            r.stopped = false;

        }

        ...
        // 注释7

        if (r.state != null || r.persistentState != null) {

            mInstrumentation.callActivityOnRestoreInstanceState(activity, r.state,

        r.persistentState);

        }

        } else if (r.state != null) {

            mInstrumentation.callActivityOnRestoreInstanceState(activity, r.state);

        }

    }

    ...

}
```

注释1处通过反射创建了Activity实例。注释2处调用`r.packageInfo.makeApplication`创建Application,如果这里Application已经在`ActivityThread#handleBindApplication()`阶段创建了，就会直接返回。

注释3处调用了Activity的`attach`方法，内部会创建`PhoneWindow`绑定Activity。注释4设置Activity的主题，注释5调用Activity的onCreate()生命周期，注释6处调用onStart()生命周期，注释7当Activity恢复时调用OnRestoreInstanceState()生命周期。再结合上面有提到的onResume()生命周期，我们Activity的启动过程就已经讲完了。当然，这里走的是启动Activity的链路，非启动Activity的逻辑其实大差不差，大家有兴趣可以自行看一下源码。

## 小结

启动页面启动过程中各进程的交互关系：

![](https://pic2.zhimg.com/v2-ae9196194a5170ccee64b8c9a9f2a0d3_1440w.jpg)

启动页面启动过程可用下图概括：

![](https://pic2.zhimg.com/v2-d58cddefecdcb6ab7eb170648add542d_1440w.jpg)

## 四、Context

APP中到底有多少个Context呢？答案是Activity的数量+Service的数量+1，1是指Application。

![](https://pic4.zhimg.com/v2-52ca8fe190b185fdc2b6810f19df9d77_1440w.jpg)

如上图所示，`ContextImpl`与`ContextWrapper`都继承于Context，并且Context具体的实现类是`ContextImpl`，作为`ContextWrapper`的成员变量`mBase`存在。Activity、Service、Application都继承于`ContextWrapper`，都属于Context，但它们都是靠`ContextImpl`去实现Context相关的功能的。

ContextImpl具体的创建时机是在Activity、Service、Application创建的时候，调用`attachBaseContext()`方法，具体代码这边就不贴了，感兴趣的同学可以自行查阅一下。

`Application#getApplicationContext()`方法获取到的是什么呢？

```
# ContextImpl

@Override
public Context getApplicationContext() {
    return (mPackageInfo != null) ?
            mPackageInfo.getApplication() : mMainThread.getApplication();
}
```

最终调用到`ContextImpl#getApplicationContext()`方法，可以看到最终返回Context的是`Application`对象。而`fragment#getContext()`返回的Context是`Activity`对象。

## 五、View的工作原理

View与Window是相辅相成的两个存在，本节先介绍View，涉及到Window的知识点可以先不管，下一节会详细介绍。可以先把Window理解成画布，View理解成画，画总是要在画布上才能渲染的，也就是说View必须要借助于Window才能展示出来。

## View的绘制流程

每一个View都会有一个`ViewRootImpl`对象，View绘制的起点就是`ViewRootImpl的perfromTraversals`方法，在`perfromTraversals`内部会调用`measure、layout、draw`这三大流程。

`measure`是为了测量出View的尺寸大小，measure又会调用`onMeasure`，在onMeasure中又会先对子View进行测量，最终得到整个View的大小。measure过程之后，我们就已经可以调用`View#getMeasuredWidth()`与`View#getMeasuredHeight()`获取到View的测量宽高了。

`layout`过程决定了View在父容器中的位置与实际宽高，也即是View的四个顶点坐标的位置。layout与measure相反，layout是先得到父View的位置，再`onLayout`递归下去得到子View的位置的。layout过程后，`View#getWidth()`与`View#getHeight()`才有值，是View的实际宽高。

`draw`是绘制的最后一步，调用`onDraw`方法将View绘制在屏幕上。当然，子View也是会一层一层递归（通过`dispatchDraw`方法）调用`onDraw`方法往下走的。

### Measure

绘制流程中比较重要的个人感觉是`measure`方法，所以单拿出来说一说。我们在看measure相关方法的时候一定会看到`MeasureSpec`这个参数，它是什么东西呢？

`MeasureSpec`是一个32位的int值，高2位代表`SpecMode`，低30位代表`SpecSize`。前者表示模式，后者表示大小，用1个值包含了两种含义。我们可以通过`MeasureSpec#makeMeasureSpec(size, mode)`方法合成`MeasureSpec`，也可以通过`MeasureSpec#getMode(spec)`与`MeasureSpec#getSize(spec)`获取到`MeasureSpec`中的`Mode`或者`Size`。**只有生成了子View的MeasureSpec，我们才能够通过调用子View的measure方法测量出子View的大小。**

那么MeasureSpec是怎么创建的呢？

SpecMode有三类，分别是`UNSPECIFIED、EXACTLY、AT_MOST`三种。UNSPECIFIED不用管，是系统内部使用的。那么什么时候是EXACTLY，什么时候是AT\_MOST呢？有些同学可能知道，它的取值跟`LayoutParams`是有关系的。**但需要注意的是，它不是完全由自身的LayoutParams决定的，LayoutParams需要和父容器一起才能决定View本身的SpecMode**。当View是DecorView时，MeasureSpec根据窗口尺寸和自身的LayoutParams就能确定：

```
# ViewRootImpl

private boolean measureHierarchy(final View host, final WindowManager.LayoutParams lp,
        final Resources res, final int desiredWindowWidth, final int desiredWindowHeight) {
    ...
    childWidthMeasureSpec = getRootMeasureSpec(baseSize, lp.width); // 注释1
    childHeightMeasureSpec = getRootMeasureSpec(desiredWindowHeight, lp.height); // 注释2
    performMeasure(childWidthMeasureSpec, childHeightMeasureSpec);
    ...
}
```

注释1与注释2处获取的就是窗口尺寸与自身LayoutParams的尺寸。**当顶层View的`MeasureSpec`确认后，在`onMeasure`方法中就会对下一层的View进行测量，获取子View的MeasureSpec**。我们可以看一下ViewGroup的`measureChildWithMargins`方法，它在很多ViewGroup的onMeasure中都会被调用到：

```
protected void measureChildWithMargins(View child,
        int parentWidthMeasureSpec, int widthUsed,
        int parentHeightMeasureSpec, int heightUsed) {
    final MarginLayoutParams lp = (MarginLayoutParams) child.getLayoutParams(); // 注释1

    final int childWidthMeasureSpec = getChildMeasureSpec(parentWidthMeasureSpec,
            mPaddingLeft + mPaddingRight + lp.leftMargin + lp.rightMargin
                    + widthUsed, lp.width);  // 注释2
    final int childHeightMeasureSpec = getChildMeasureSpec(parentHeightMeasureSpec,
            mPaddingTop + mPaddingBottom + lp.topMargin + lp.bottomMargin
                    + heightUsed, lp.height);

    child.measure(childWidthMeasureSpec, childHeightMeasureSpec); // 注释3
}
```

我们可以看到注释1处首先是获取到子View自身的`LayoutParams`。然后再在注释2处根据父View的`MeasureSpec`以及`padding`、`margin`，生成了子View的`MeasureSpec`。最后再通过注释3处调用`measure`测量出了子View的长度。那么父View的MeasureSpec传进去有什么用呢？后面代码有点长，我直接写结论：

```
当父View的MeasureMode是EXACTLY时，子View的LayoutParams如果是MATCH_PARENT或者写死的值，子View的MeasureMode是EXACTLY；子View的LayoutParams如果是WRAP_CONTENT，子View的MeasureMode是AT_MOST。

当父View的MeasureMode是AT_MOST时，子View的LayoutParams只有是写死的值时，子View的MeasureMode才会是EXACTLY，不然这种情况都是AT_MOST。
```

有一个比较容易理解的方法是，`AT_MOST`通常对应的LayoutParams是`WRAP_CONTENT`。我们可以想想，父View如果是`WRAP_CONTENT`，子View即使是`MATCH_CONTENT`，那子View还不是相当于尺寸不确定吗？所以子View这种情况下的MeasureMode仍然是`AT_MOST`。**只有在尺寸确定的情况下，View的MeasureMode才会是EXACTLY**。如果这里混乱的同学，可以再好好琢磨一下。

至于padding和margin，我们大概想一想就知道肯定是在测量长度时用到的。比如在计算子View长度时，肯定是需要把它的padding和margin都去掉才能准确。

另外，我们观察View的onMeasure方法，发现它提供了默认的实现：

```
protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
    setMeasuredDimension(getDefaultSize(getSuggestedMinimumWidth(), widthMeasureSpec),
            getDefaultSize(getSuggestedMinimumHeight(), heightMeasureSpec));
}
```

通过`setMeasuredDimension(width, height)`设置了View的宽高。但这里的getDefaultSize()是准确的吗？我们可以看一下：

```
public static int getDefaultSize(int size, int measureSpec) {
    int result = size;
    int specMode = MeasureSpec.getMode(measureSpec);
    int specSize = MeasureSpec.getSize(measureSpec);

    switch (specMode) {
    case MeasureSpec.UNSPECIFIED:
        result = size;
        break;
    case MeasureSpec.AT_MOST:
    case MeasureSpec.EXACTLY:
        result = specSize; // 注释1
        break;
    }
    return result;
}
```

注释1处我们可以看到，当SpecMode为AT\_MOST时，默认直接用的是specSize。**所以这里是有问题的**，因为这个specSize代表父View的size，**这样会造成LayoutParams为WRAP\_CONTENT的效果是MATCH\_PARENT的效果**。所以很多官方的自定义View都是重写了onMeasure方法，自己去计算尺寸。我们在写自定义View时也需要特别注意这一点。

而ViewGroup是一个抽象类，它并没有实现View的onMeasure方法，那是因为每个ViewGroup的布局规则都不一样，自然测量的方式也会不同，需要各个子类自己去实现onMeasure。

### layout过程

`layout`过程只需要知道，**layout主要是父容器用来确定容易中子View位置的一个过程**。当父容器确定位置后，在父容器的`onLayout`中会遍历其所有子元素调用它们的`layout`方法。而**layout方法实际上就是确定四个顶点坐标的位置**。

我们可以看到`onLayout`与`onMeasure`方法类似，View和ViewGroup都没有实现它，因为每个View布局方式不同，需要自己实现。但因为在确定坐标过程中需要用到长和宽，所以layout的顺序排在measure的后面。

观察`View#getWidth()`与`View#getHeight()`方法：

```
public final int getWidth() {
    return mRight - mLeft;
}

public final int getWidth() {
    return mRight - mLeft;
}
```

可以看到实际上它们就是坐标之间做了减法，所以这两个方法要在onLayout方法之后才能获取到真正的值。也就是View的实际宽高。一般情况下measureWidth与width是会相等的，除非我们特意重写了layout方法（layout方法与measure方法不一样，它是可以被重写的）：

```
public void layout(int l, int t, int r, int b) {
    super.layout(l, t, r+11, b-11)
}
```

这样，实际宽高与测量宽高就会不一致了。

## 自定义View

自定义View一共分为四种类型：

-   继承View

-   这种类型常见于要绘制一些不规则的图形，需要重写它的`onDraw`方法。需要的注意的是，**View的Padding属性在绘制过程中默认是不起作用的**，如果要使Padding属性起作用，就需要自己在`onDraw`中获取Padding后绘制。另外，`onMeasure`时需要考虑wrap\_content和padding的情况，上文也有提到过。

-   继承ViewGroup

-   这种类型比较少，一般用于实现自定义的布局。那就意味着要自定义布局规则，也就是自定义`onMeasure`、`onLayout`。在`onMeasure`中，也需要处理wrap\_content和padding的情况。另外在`onMeasure`和`onLayout`中，还需要考虑padding与子元素margin共同作用的场景。

-   继承某个特定的View，比如TextView

-   这种类型一般用于扩展已有的某个View的功能，比较容易实现，不需要重写`onMeasure`或者`onLayout`方法。

-   继承某个特定的ViewGroup，比如FrameLayout

-   这种类型也很常见，一般用于将几个View组合到一起，但相对第二种方式会简单许多，也不需要重写`onMeasure`或者`onLayout`方法。

在自定义View中还需要额外注意的是，**如果View中有额外开线程或者是动画的话，需要在合适的时机（比如onDetachedFromWindow生命周期）进行回收或者暂停，否则容易引起内存泄漏**。

## 六、Window与WindowManager

## Window相关类

Window是一个抽象类，它的具体实现是`PhoneWindow`。`PhoneWindow`在`Activity#attach`方法中被创建：

```
#Activity

final void attach(Context context...) {
    ...
    mWindow = new PhoneWindow(this, window, activityConfigCallback); // 注释1
    ...
    mWindow.setWindowManager(
        (WindowManager)context.getSystemService(Context.WINDOW_SERVICE),
        mToken, mComponent.flattenToString(),
        (info.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED) != 0); // 注释2

}
```

注释1处初始化了`PhoneWindow`，注释2处给Window设置了`WindowManager`。`WindowManager`，顾名思义就是用来管理Window的，它继承了`ViewManager`接口：

```
public interface ViewManager
{
    public void addView(View view, ViewGroup.LayoutParams params);
    public void updateViewLayout(View view, ViewGroup.LayoutParams params);
    public void removeView(View view);
}
```

从上面的代码我们可以看出，管理Window，实际上就是管理View。`context.getSystemService(Context.WINDOW_SERVICE)`方法最终获取的是`WindowManager`的实现类`WindowManagerImpl`，我们可以观察`WindowManagerImpl`中的这三个方法，如`addView`:

```
@Override
public void addView(@NonNull View view, @NonNull ViewGroup.LayoutParams params) {
    applyDefaultToken(params);
    mGlobal.addView(view, params, mContext.getDisplay(), mParentWindow);
}
```

可以看到实际上`addView`是由`mGlobal`去实现的，`WindowManagerImpl`只是桥接了一下。这个`mGlobal`是`WindowManagerGlobal`，是个单例，全局唯一：

```
#WindowManagerImpl

private final WindowManagerGlobal mGlobal = WindowManagerGlobal.getInstance();
```

## Window类型

Window类型具体为Window的`Type`属性。`Type`属性其实是一个int指，分为三大类型：

-   应用程序窗口

-   常见的Activity的Window就属于应用程序窗口，它的int值范围是1-99。

-   子窗口

-   子窗口代表着要依附于其它窗口才能存在，比如PopupWindow就属于一个子窗口，它的int值范围是1000-1999。

-   系统窗口

-   Toast、音量条、输入法的窗口就属于系统窗口，int值范围是2000-2999。当我们要创建一个系统窗口时，需要申请系统权限android.permission.SYSTEM\_ALERT\_WINDOW才可以。

一般来说，type属性值越大，那么Z-Order的排序就越靠前，窗口越接近用户。

## Window的操作

### 添加View

我们延续上方的`addView`代码，看一下这里面的过程：

```
# WindowManagerImpl
@Override
public void addView(@NonNull View view, @NonNull ViewGroup.LayoutParams params) {
    applyDefaultToken(params); // 注释1
    mGlobal.addView(view, params, mContext.getDisplay(), mParentWindow);
}

private void applyDefaultToken(@NonNull ViewGroup.LayoutParams params) {
    // Only use the default token if we don't have a parent window.
    if (mDefaultToken != null && mParentWindow == null) {
        if (!(params instanceof WindowManager.LayoutParams)) {
            throw new IllegalArgumentException("Params must be WindowManager.LayoutParams");
        }

        // Only use the default token if we don't already have a token.
        final WindowManager.LayoutParams wparams = (WindowManager.LayoutParams) params;
        if (wparams.token == null) {
            wparams.token = mDefaultToken;
        }
    }
}

# WindowManagerGlobal
public void addView(View view, ViewGroup.LayoutParams params,
        Display display, Window parentWindow) {
    ...
    root = new ViewRootImpl(view.getContext(), display); // 注释2

    view.setLayoutParams(wparams);

    // 注释3
    mViews.add(view);
    mRoots.add(root);
    mParams.add(wparams);

    // do this last because it fires off messages to start doing things
    try {
        // 注释4
        root.setView(view, wparams, panelParentView);
    } catch (RuntimeException e) {
        // BadTokenException or InvalidDisplayException, clean up.
        if (index >= 0) {
            removeViewLocked(index, true);
        }
        throw e;
    }
}
```

注释1调用了`applyDefaultToken`方法，将`token`放在了`LayoutParams`中。我们可以发现Window的很多方法中都有这个token，这个token到底是什么，有什么作用呢？实际上这个token是一个`IBinder`对象，是AMS创建Activity时就会创建的，用来唯一标识一个Activity，创建后WMS也会拿到一份储存起来。当AMS调用`scheduleLauncherActivity`方法转回到APP进程时，会将这个token传到APP进程中。当我们在APP进程对Window进行操作时，就会用到这个token。因为最终Window的操作是在WMS，所以我们调用WMS方法时会将这个token传过去，WMS就会比对这个token和最开始存储的token，以此来找到这个Window是属于哪个Activity。、

再继续回来，我们看注释2处`WindowManagerGlobal#addView`方法中每次都会new一个`ViewRootImpl`对象。`ViewRootImpl`我们很熟悉，上一节有讲到，它负责View的绘制工作。这里它不仅负责View的绘制，还负责与最终的WMS进行通信。具体可以看到注释4处调用了`ViewRootImpl#setView`方法，内部再调用`IWindowSession#addToDisplay`方法。

```
public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
    ...
    res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
        getHostVisibility(), mDisplay.getDisplayId(), mWinFrame,
        mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
        mAttachInfo.mOutsets, mAttachInfo.mDisplayCutout, mInputChannel);
    ...
}
```

`IWindowSession`实际上是WMS的`Session对象`在APP进程的代理，这样我们的逻辑就到了WMS那边了，WMS会完成剩下的addView操作，包括为添加的窗口分配Surface，确定窗口显示次序，最后将Surface交给`SurfaceFlinger`处理，合成到屏幕上进行显示。并且，`addToDisplay`方法的第一个参数mWindow，是`ViewRootImpl`的内部类`W`，它是app进程的Binder实现类，WMS可以通过它调用app进程的方法。

另外我们可以看到`WindowManagerGlobal`在注释3处维护了三个列表，一个是`View`，一个是`ViewRootImpl`，一个是`Params`布局参数，在更新Window/移除Window时会用到。

### 小结

添加View的过程可用下图概括：

![](https://pic2.zhimg.com/v2-4d7466fdad72ce1a6bd210d138d02c59_1440w.jpg)

### 更新Window

更新Window的过程与添加的过程是类似的，所经过的类关系一模一样。主要的区别在`WindowManagerGlobal`中需要从列表中获取到ViewRootImpl，并更新布局参数：

```
public void updateViewLayout(View view, ViewGroup.LayoutParams params) {
    ...
    synchronized (mLock) {
        int index = findViewLocked(view, true);
        ViewRootImpl root = mRoots.get(index);
        mParams.remove(index);
        mParams.add(index, wparams);
        root.setLayoutParams(wparams, false);
    }
}
```

ViewRootImpl会调用`scheduleTraversals`方法重新绘制页面，最终在`performTraversals`方法中调用`IWindowSession#relayout`方法更新Window，并重新触发View的三大绘制流程。

### Activity渲染

在第三节我们讲了Activity的启动过程，但其实还没有完全讲完，因为Activity实际上是没有渲染出来的。那Activity是怎么渲染出来的呢？当然也是靠Window。

当界面可与用户进行交互时，AMS会调用`ActivityThread`的`handleResumeActivity`方法:

```
public void handleResumeActivity(IBinder token, boolean finalStateRequest, boolean isForward, String reason) {
    ...
    final ActivityClientRecord r = performResumeActivity(token, finalStateRequest, reason); // 注释1
    ...
    if (r.window == null && !a.mFinished && willBeVisible) {
        r.window = r.activity.getWindow();
        View decor = r.window.getDecorView();
        decor.setVisibility(View.INVISIBLE);
        ViewManager wm = a.getWindowManager(); 
        WindowManager.LayoutParams l = r.window.getAttributes();
        a.mDecor = decor;
        l.type = WindowManager.LayoutParams.TYPE_BASE_APPLICATION;
        l.softInputMode |= forwardBit;
        ...
        if (a.mVisibleFromClient) {
            if (!a.mWindowAdded) {
                a.mWindowAdded = true;
                wm.addView(decor, l); // 注释2
            } else {
            ...
        }
        ...
```

注释1处调用`performResumeActivity`方法，内部会触发Activity的`onResume`生命周期。注释2处用`WindowManager#addView`方法将decorView绘制到了Window上，这样整个Activity就渲染出来了。这也是Activity在onResume生命周期才会显示出来的原因。

## Dialog、PopupWindow、Toast

最后，还是觉得有必要搞明白Dialog、PopupWindow、Toast这三个东西到底是什么玩意儿。毫无疑问，它们都是通过Window渲染的，但Dialog属于应用程序窗口，PopupWindow属于子窗口，Toast属于系统级窗口。

Dialog的创建时，跟Activity一样会创建一个PhoneWindow：

```
Dialog(@NonNull Context context, @StyleRes int themeResId, boolean      createContextThemeWrapper) {
    ···
    mWindowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);

    final Window w = new PhoneWindow(mContext); // 注释1
    mWindow = w;
    w.setCallback(this);
    w.setOnWindowDismissedCallback(this);
    w.setOnWindowSwipeDismissedCallback(() -> {
        if (mCancelable) {
            cancel();
        }
    });
    w.setWindowManager(mWindowManager, null, null);
    w.setGravity(Gravity.CENTER);

    mListenersHandler = new ListenersHandler(this);
}
```

我们看到注释1处Dialog初始化时是创建了自己的Window，所以它不依附于其它Window存在，不属于子窗口，而是一个应用程序窗口。

在`Dialog#show()`时，就会用WindowManager将DecorView添加到窗口上，移除时同样是将DecorView从窗口上移除。**有一个特殊之处是，普通的dialog的context必须是Activity，否则show时会报错，因为添加窗口时需要校验Activity的token**，除非我们把这个dialog的window设置成系统窗口，就不需要了。

而PopupWindow为什么就属于子窗口呢？我们可以查阅PopupWindow的源码：

```
# PopupWindow

private int mWindowLayoutType = WindowManager.LayoutParams.TYPE_APPLICATION_PANEL;
public PopupWindow(View contentView, int width, int height, boolean focusable) {
    if (contentView != null) {
        mContext = contentView.getContext();
        // 注释1
        mWindowManager = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE); 
    }

    setContentView(contentView);
    setWidth(width);
    setHeight(height);
    setFocusable(focusable);
}

public void showAtLocation(IBinder token, int gravity, int x, int y) {
    ...
    final WindowManager.LayoutParams p = createPopupLayoutParams(token); // 注释2
    preparePopup(p);

    ...
    invokePopup(p);
}
   
protected final WindowManager.LayoutParams createPopupLayoutParams(IBinder token) {
    final WindowManager.LayoutParams p = new WindowManager.LayoutParams();
    p.gravity = computeGravity();
    p.flags = computeFlags(p.flags);
    p.type = mWindowLayoutType; // 注释3
    p.token = token; // 注释4
    p.softInputMode = mSoftInputMode;
    p.windowAnimations = computeAnimationResource();
    ...
}
    
private void invokePopup(WindowManager.LayoutParams p) {
    ...
    final PopupDecorView decorView = mDecorView;
    decorView.setFitsSystemWindows(mLayoutInsetDecor);

    setLayoutDirectionFromAnchor();

    mWindowManager.addView(decorView, p); // 注释5
    ...
}
```

在源码中，我们可以看到PopupWindow中不会有任何新建Window的操作，因为它依赖的是别人的Window。在注释1处拿到WindowNManager，注释2处调用`createPopupLayoutParams`方法给Window参数赋值。尤其注意注释3和注释4处两个字段，`type`字段即代表了窗口的类型，我们可以看到mWindowLayoutType的值是`WindowManager.LayoutParams.TYPE_APPLICATION_PANEL`，这就代表了是子窗口。且注释4处的`token`，不再是Activity的token了，而是show的时候传进来的View的token。

Toast也是类似，我们可以看到它的源码中`WindowParams.LayoutParams.type`的值是`WindowManager.LayoutParams.TYPE_TOAST`，对应着系统窗口。具体内部机制这里就不分析了，感兴趣的同学可以自行查阅。

## 七、Handler消息机制

Handler消息机制，最主要的作用就是在多线程的背景下，通过消息机制，可以从子线程切换到主线程进行UI的更新操作，并且保证了线程安全。

首先介绍其中的**主要成员以及他们之间的关系**。

-   Message

-   消息，可用来存放数据，通过Message.obtain()可从缓存池中获取一个消息对象。

-   MessageQueue

-   用于存储消息的队列。

-   Looper

-   loop方法会持续监听MessageQueue，当MessageQueue中有消息时，将消息拿出来进行分发。
-   一个线程只会有一个Looper，一个Looper对应一个MessageQueue，在Loope创建时，对应的MessageQueue就会创建。一个Handler也只能绑定一个Looper，但是一个Looper可以绑定多个Handler，**Looper是以线程为单位的**。
-   Looper线程隔离，是通过**Threadlocal**实现的。Looper下有个静态变量sThreadLocal，每个线程下调用Looper.myLooper()时就是从这个变量中拿，获取当前线程下的Looper。
-   Looper分为子线程Looper与MainLooper。MainLooper在ActivityThread的`main方`法中就会通过`Looper.prepareMainLooper()`方法准备好。它们的区别一个是可以quit的，一个是不能quit的。所谓的quit就是调用`looper.quit`方法，实际上是在MessageQueue中进行quit，将其中的消息给移除。quit又分是否安全quit。safequit下，会先将消息队列中已有的消息先发完再quit。

-   Handler

-   用于发送Message与接收Message，作为Message中的**target**。

-   Message.Callback

-   **当Message.callback存在时，Handler将接收不到Message，而是直接回调到callback中**。 Handler在post一个Runnable时，实际上就是发送一个消息并将这个Runnable作为这个消息的callback存在。

那整个**消息通信的流程**又是什么样的呢？我们可以串起来说一下。

1.  首先通信准备阶段，`Looper.prepare()`给当前线程创建一个Looper，同时创建一个MessageQueue。 new一个Handler。
2.  `handler.post`或者`handler.sendMessage`发送一个消息，入队到MessageQueue。
3.  Looper将MessageQueue中的队列按优先级分发给Handler。
4.  Handler获取到Message，并根据之中的数据处理消息。

在写业务的时候我们常常会`post/send`一个延时消息，这个延时消息是怎么实现的呢？每个Message都有一个`when`字段，对应着它什么时候需要被分发。在入队到MessageQueue时，就会根据`when`的先后进行排列。当loop取消息时，会判断这个消息的分发时间是否到了，如果还没到就先不分发，等时间到了再分发。

所以在消息机制中，并不是发一个消息就能够保证它马上会被处理的，因为机制内部就有优先级排列。那怎么能够在我们需要的时候提高消息的优先级呢？我们可以调用`postAtFrontOfQueue`以及`sendMessageAtFrontOfQueue`将消息放到队头，其实是将这个消息的when设置为0。或者，使用异步消息与同步屏障。

## 异步消息与同步屏障

异步消息与同步屏障其实我们开发时很少会用到，一般都是系统自己使用，同步屏障的接口也是hide的，我们要调用只能反射调用。但因为面试很常见，这里也顺带提一下。

**异步消息的创建**我们可以在创建Message时调用`setAsynchronous`方法将Message设置为异步消息，或者Handler创建时也可以传参选择是否为异步，如果是异步的话，Handler发送的所有消息都为异步消息。

那什么又是同步屏障呢？**同步屏障的本质**其实就是特殊的Message，**特殊在于这个Message的Target不是Handler，而是null**，以此与其它Message区分。我们可以通过反射调用`MessageQueue的postSyncBarrier`方法发送一个同步屏障，以及`removeSyncBarrier`方法进行移除。

**我们可以将同步屏障与异步消息结合，以此来提高异步消息是被优先执行的**。具体原理是MessageQueue在Loop取出Message时，会判断Message是否为同步屏障。若为同步屏障，则需要在队列中找到第一个异步消息进行优先处理，而不是处理排在前面的同步消息：

```
#MessageQueue

Message next() {
    ...
    synchronized (this) {
        // Try to retrieve the next message.  Return if found.
        final long now = SystemClock.uptimeMillis();
        Message prevMsg = null;
        Message msg = mMessages;
        if (msg != null && msg.target == null) {
            // Stalled by a barrier.  Find the next asynchronous message in the queue.
            do {
                prevMsg = msg;
                msg = msg.next;
            } while (msg != null && !msg.isAsynchronous()); // 注释1
        }
    ...
}
```

注释1处当发现`msg.target`为null时，代表着这个消息是同步屏障，就会去拿队列中第一个异步消息。

上文中讲到过view的更新操作，其中在最后的绘制阶段ViewRootImpl会通过`requestLayout()`进行布局的更新，`requestLayout()`内部又调用`scheduleTraversals()`方法从而重新走一遍三大绘制流程。

```
# ViewRootImpl

void scheduleTraversals() {
    if (!mTraversalScheduled) {
        mTraversalScheduled = true;
        mTraversalBarrier = mHandler.getLooper().getQueue().postSyncBarrier(); // 注释1
        mChoreographer.postCallback(
                Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null); //注释2
        if (!mUnbufferedInputDispatch) {
            scheduleConsumeBatchedInput();
        }
        notifyRendererOfFramePending();
        pokeDrawLockIfNeeded();
    }
}
```

在这个方法中我们没有看到三大绘制流程的触发，而是在注释1处发了一个同步屏障，阻塞主线程消息队列。同时在注释2处监听VSYNC信号。当VSYNC信号来时，Choreographer会发一个异步消息，这个异步消息在同步屏障的帮助下将会得到执行，并且触发监听回调。监听回调中ViewRootImpl将移除同步屏障，并调用`performTraversals()`执行三大绘制流程刷新UI。

## 总结

本篇文章一共介绍了七点Framework知识，分别是：**系统启动流程、应用进程启动流程、Activity启动过程、Context、View的工作原理、Window与WindowManager、Handler消息机制**

### Android 学习笔录

**Android 性能优化篇：`[https://qr18.cn/FVlo89](https://link.zhihu.com/?target=https%3A//qr18.cn/FVlo89)`**  
**Android 车载篇：`[https://qr18.cn/F05ZCM](https://link.zhihu.com/?target=https%3A//qr18.cn/F05ZCM)`**  
**Android 逆向安全学习笔记：`[https://qr18.cn/CQ5TcL](https://link.zhihu.com/?target=https%3A//qr18.cn/CQ5TcL)`**  
**Android Framework底层原理篇：`[https://qr18.cn/AQpN4J](https://link.zhihu.com/?target=https%3A//qr18.cn/AQpN4J)`**  
**Android 音视频篇：`[https://qr18.cn/Ei3VPD](https://link.zhihu.com/?target=https%3A//qr18.cn/Ei3VPD)`**  
**Jetpack全家桶篇（内含Compose）：`[https://qr18.cn/A0gajp](https://link.zhihu.com/?target=https%3A//qr18.cn/A0gajp)`**  
**Kotlin 篇：`[https://qr18.cn/CdjtAF](https://link.zhihu.com/?target=https%3A//qr18.cn/CdjtAF)`**  
**Gradle 篇：`[https://qr18.cn/DzrmMB](https://link.zhihu.com/?target=https%3A//qr18.cn/DzrmMB)`**  
**OkHttp 源码解析笔记：`[https://qr18.cn/Cw0pBD](https://link.zhihu.com/?target=https%3A//qr18.cn/Cw0pBD)`**  
**Flutter 篇：`[https://qr18.cn/DIvKma](https://link.zhihu.com/?target=https%3A//qr18.cn/DIvKma)`**  
**Android 八大知识体：`[https://qr18.cn/CyxarU](https://link.zhihu.com/?target=https%3A//qr18.cn/CyxarU)`**  
**Android 核心笔记：`[https://qr21.cn/CaZQLo](https://link.zhihu.com/?target=https%3A//qr21.cn/CaZQLo)`**  
**Android 往年面试题锦：`[https://qr18.cn/CKV8OZ](https://link.zhihu.com/?target=https%3A//qr18.cn/CKV8OZ)`**  
**2023年最新Android 面试题集：`[https://qr18.cn/CgxrRy](https://link.zhihu.com/?target=https%3A//qr18.cn/CgxrRy)`**  
**Android 车载开发岗位面试习题：`[https://qr18.cn/FTlyCJ](https://link.zhihu.com/?target=https%3A//qr18.cn/FTlyCJ)`**  
**音视频面试题锦：`[https://qr18.cn/AcV6Ap](https://link.zhihu.com/?target=https%3A//qr18.cn/AcV6Ap)`**


### Android 学习笔录

**Android 性能优化篇：`[https://qr18.cn/FVlo89](https://link.zhihu.com/?target=https%3A//qr18.cn/FVlo89)`**  
**Android 车载篇：`[https://qr18.cn/F05ZCM](https://link.zhihu.com/?target=https%3A//qr18.cn/F05ZCM)`**  
**Android 逆向安全学习笔记：`[https://qr18.cn/CQ5TcL](https://link.zhihu.com/?target=https%3A//qr18.cn/CQ5TcL)`**  
**Android Framework底层原理篇：`[https://qr18.cn/AQpN4J](https://link.zhihu.com/?target=https%3A//qr18.cn/AQpN4J)`**  
**Android 音视频篇：`[https://qr18.cn/Ei3VPD](https://link.zhihu.com/?target=https%3A//qr18.cn/Ei3VPD)`**  
**Jetpack全家桶篇（内含Compose）：`[https://qr18.cn/A0gajp](https://link.zhihu.com/?target=https%3A//qr18.cn/A0gajp)`**  
**Kotlin 篇：`[https://qr18.cn/CdjtAF](https://link.zhihu.com/?target=https%3A//qr18.cn/CdjtAF)`**  
**Gradle 篇：`[https://qr18.cn/DzrmMB](https://link.zhihu.com/?target=https%3A//qr18.cn/DzrmMB)`**  
**OkHttp 源码解析笔记：`[https://qr18.cn/Cw0pBD](https://link.zhihu.com/?target=https%3A//qr18.cn/Cw0pBD)`**  
**Flutter 篇：`[https://qr18.cn/DIvKma](https://link.zhihu.com/?target=https%3A//qr18.cn/DIvKma)`**  
**Android 八大知识体：`[https://qr18.cn/CyxarU](https://link.zhihu.com/?target=https%3A//qr18.cn/CyxarU)`**  
**Android 核心笔记：`[https://qr21.cn/CaZQLo](https://link.zhihu.com/?target=https%3A//qr21.cn/CaZQLo)`**  
**Android 往年面试题锦：`[https://qr18.cn/CKV8OZ](https://link.zhihu.com/?target=https%3A//qr18.cn/CKV8OZ)`**  
**2023年最新Android 面试题集：`[https://qr18.cn/CgxrRy](https://link.zhihu.com/?target=https%3A//qr18.cn/CgxrRy)`**  
**Android 车载开发岗位面试习题：`[https://qr18.cn/FTlyCJ](https://link.zhihu.com/?target=https%3A//qr18.cn/FTlyCJ)`**  
**音视频面试题锦：`[https://qr18.cn/AcV6Ap](https://link.zhihu.com/?target=https%3A//qr18.cn/AcV6Ap)`**