---
link: https://blog.csdn.net/qq_40587575/article/details/121424634
title: 【安卓 R 源码】从源码角度看Context
description: 文章浏览阅读2.1k次。1. Context 是什么？Context，中文直译为“上下文”，它描述的是一个应用程序环境的信息，从程序的角度上来理解：Context是个抽象类，而Activity、Service、Application等都是该类的一个实现。SDK中对其说明如下：Interface to global information about an application environment. This is an abstract class whose implementation is provided _context.getcontentresolver().query无权
keywords: context.getcontentresolver().query无权
author: 蜘蛛侠不会飞 Csdn认证博客专家 Csdn认证企业博客 码龄6年 暂无认证
date: 2023-08-22T01:53:37.000Z
publisher: null
stats: paragraph=164 sentences=324, words=2544
---
## 1. Context 是什么？

Context，中文直译为"上下文"，它描述的是一个应用程序环境的信息，从程序的角度上来理解：Context是个抽象类，而Activity、Service、Application等都是该类的一个实现。SDK中对其说明如下：

> Interface to global information about an application environment. This is an abstract class whose implementation is provided by the Android system. It allows access to application-specific resources and classes, as well as up-calls for application-level operations such as launching activities, broadcasting and receiving intents, etc

中文意思为：

> 1、它描述的是一个应用程序环境的信息，即上下文。
2、该类是一个抽象(abstract class)类，android提供了该抽象类的具体实现类(ContextIml类)。
3、通过它我们可以获取应用程序的资源和类，也包括一些应用级别操作，例如：启动一个Activity，发送广播，接受Intent信息 等，有如下功能：
• 启动Activity
• 启动和停止Service
• 发送广播消息(Intent)
• 注册广播消息(Intent)接收者
• 可以访问APK中各种资源(如Resources和AssetManager等)
• 可以访问Package的相关信息
• APK的各种权限管理

```
在android中context可以作很多操作，但是最主要的功能是加载和访问资源。在android中有两种context，一种是 application context，一种是activity context，通常我们在各种类和方法间传递的是activity context。
```

有些函数调用时需要一个Context参数，比如Toast.makeText， **因为函数需要知道是在哪个界面中显示的Toast**。一般在Activity中我们直接用this代替，代表调用者的实例为Activity。再比如，Button myButton = new Button(this); **这里也需要Context参数（this），表示这个按钮是在"this"这个屏幕中显示的**。

Context体现到代码上来说，是个抽象类，其实调用的是 ContextImpl，其主要方法如下：

![](https://img-blog.csdnimg.cn/e5fd1d7b68924212a539d4827261d614.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBA6JyY6Jub5L6g5LiN5Lya6aOe,size_20,color_FFFFFF,t_70,g_se,x_16)

常用的一些方法：

```
TextView tv = new TextView(getContext());

ListAdapter adapter = new SimpleCursorAdapter(getApplicationContext(), &#x2026;);

AudioManager am = (AudioManager) getContext().

                        getSystemService(Context.AUDIO_SERVICE);
getApplicationContext().getSharedPreferences(name, mode);

getApplicationContext().getContentResolver().query(uri, &#x2026;);

getContext().getResources().getDisplayMetrics().widthPixels * 5 / 8;

getContext().startActivity(intent);

getContext().startService(intent);

getContext().sendBroadcast(intent);
```

## 2. 获取 Context 的 3 种方式

* **1. mContext = getApplicationContext();*

这种方式获得的context是全局context，整个项目的生命中期中是唯一的且一直存在的，代表了所有activities的context

* **2. mContext = getContext()*

这种方式获得的context当activity销毁时，context也会跟着销毁了

* **3. mContext = getBaseContext();*

获取的是 ContextImpl

![](https://img-blog.csdnimg.cn/img_convert/38be546292a4708d88dc22af76f4c451.png)

应用程序创建Context实例的情况有如下几种情况：

> 1、创建Application 对象时， 而且 **整个App共一个Application对象**
2、创建Service对象时
3、创建Activity对象时

因此应用程序App共有的Context数目公式为： **总Context实例个数 = Service个数 + Activity个数 + 1（Application对应的Context实例）**
Broadcast Receiver，Content Provider并不是Context的子类，他们所持有的Context都是其他地方传过去的，所以并不计入Context总数。

## 3. Context 及其相关类源码

Context相关类的继承关系如下：

![](https://img-blog.csdnimg.cn/20190119155319676.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2dlZHVvXzgz,size_16,color_FFFFFF,t_70)

Context的直接子类是ContextImpl和ContextWrapper（后文分析二者的作用），Service和Application二者相似，都是继承于ContextWrapper，而Activity是继承于ContextThemeWrapper，因为Activity是有主题的

### 1）. Context相关的类

* **frameworks/base/core/java/android/content/Context.java*

```
// Context &#x662F;&#x62BD;&#x8C61;&#x7C7B;
public abstract class Context {
     ...

     public abstract Object getSystemService(String name);  //&#x83B7;&#x5F97;&#x7CFB;&#x7EDF;&#x7EA7;&#x670D;&#x52A1;
     public abstract void startActivity(Intent intent);     //&#x901A;&#x8FC7;&#x4E00;&#x4E2A;Intent&#x542F;&#x52A8;Activity
     public abstract boolean bindService(@RequiresPermission Intent service,
            @NonNull ServiceConnection conn, @BindServiceFlags int flags);  //&#x7ED1;&#x5B9A;Service
     //&#x6839;&#x636E;&#x6587;&#x4EF6;&#x540D;&#x5F97;&#x5230;SharedPreferences&#x5BF9;&#x8C61;
     public abstract SharedPreferences getSharedPreferences(String name,int mode);
     ...

}
```

* **frameworks/base/core/java/android/app/ContextImpl.java*

**Context 的具体的实现类ContextImpl ：**

```
class ContextImpl extends Context {
    private final static String TAG = "ContextImpl";
    private final static boolean DEBUG = false;

// &#x7ED1;&#x5B9A;&#x670D;&#x52A1;&#xFF0C;&#x5177;&#x4F53;&#x7684;&#x5B9E;&#x73B0;
    @Override
    public boolean bindService(Intent service, ServiceConnection conn, int flags) {
        warnIfCallingFromSystemProcess();
        return bindServiceCommon(service, conn, flags, null, mMainThread.getHandler(), null,
                getUser());
    }

// &#x542F;&#x52A8;&#x4E00;&#x4E2A;activity
    @Override
    public void startActivity(Intent intent) {
        warnIfCallingFromSystemProcess();
        startActivity(intent, null);
    }
```

* **frameworks/base/core/java/android/content/ContextWrapper.java*

ContextWrapper 对Context类的一种包装，该类的构造函数包含了一个真正的Context引用，即ContextIml对象

```
public class ContextWrapper extends Context {
    @UnsupportedAppUsage
    Context mBase; // Context &#x5B9E;&#x9645;&#x662F;&#x5B9E;&#x73B0;&#x7C7B; ContextImpl

    public ContextWrapper(Context base) {
        mBase = base;
    }

// &#x4F20;&#x5165; Context &#x5BF9;&#x8C61;
    protected void attachBaseContext(Context base) {
        if (mBase != null) {
            throw new IllegalStateException("Base context already set");
        }
        mBase = base;
    }

// &#x83B7;&#x53D6;Context &#x5BF9;&#x8C61;
    /**
     * @return the base context as set by the constructor or setBaseContext
     */
    public Context getBaseContext() {
        return mBase;
    }

// &#x8C03;&#x7528; ContextImpl&#x65B9;&#x6CD5;&#x53BB;&#x542F;&#x52A8; activity
    @Override
    public void startActivity(Intent intent) {
        mBase.startActivity(intent);
    }
```

* **frameworks/base/core/java/android/view/ContextThemeWrapper.java*

包含了主题(Theme)相关的接口，即android:theme属性指定的。只有Activity需要主题

```
public class ContextThemeWrapper extends ContextWrapper {
    @UnsupportedAppUsage
    private int mThemeResource;
    @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.P, trackingBug = 123768723)
    private Resources.Theme mTheme;

    public ContextThemeWrapper(Context base, @StyleRes int themeResId) {
        super(base);
        mThemeResource = themeResId;
    }

    public ContextThemeWrapper(Context base, Resources.Theme theme) {
        super(base);
        mTheme = theme;
    }

    @Override
    protected void attachBaseContext(Context newBase) {
        super.attachBaseContext(newBase);
    }
```

### 2. Activity的 Context 创建

Activity 的启动流程可以看这篇文章：[【安卓 R 源码】Activity 启动流程及其生命周期源码分析](https://mikejun.blog.csdn.net/article/details/121354976)

* **frameworks/base/core/java/android/app/ActivityThread.java*

```
    /**  Core implementation of activity launch. */
    private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
        ActivityInfo aInfo = r.activityInfo;
        if (r.packageInfo == null) {
            r.packageInfo = getPackageInfo(aInfo.applicationInfo, r.compatInfo,
                    Context.CONTEXT_INCLUDE_CODE);
        }

        ComponentName component = r.intent.getComponent();
        if (component == null) {
            component = r.intent.resolveActivity(
                mInitialApplication.getPackageManager());
            r.intent.setComponent(component);
        }

        if (r.activityInfo.targetActivity != null) {
            component = new ComponentName(r.activityInfo.packageName,
                    r.activityInfo.targetActivity);
        }

// 1. &#x6B64;&#x5904;&#x521B;&#x5EFA;&#x4E86; context &#x5BF9;&#x8C61;&#xFF0C;ContextImpl
        ContextImpl appContext = createBaseContextForActivity(r);
        Activity activity = null;
        try {
            java.lang.ClassLoader cl = appContext.getClassLoader();

// 2. &#x901A;&#x8FC7;&#x53CD;&#x5C04;&#x521B;&#x5EFA; activity &#x5BF9;&#x8C61;
            activity = mInstrumentation.newActivity(
                    cl, component.getClassName(), r.intent);
            StrictMode.incrementExpectedActivityCount(activity.getClass());
            r.intent.setExtrasClassLoader(cl);
            r.intent.prepareToEnterProcess(isProtectedComponent(r.activityInfo),
                    appContext.getAttributionSource());
            if (r.state != null) {
                r.state.setClassLoader(cl);
            }
        } catch (Exception e) {
            if (!mInstrumentation.onException(activity, e)) {
                throw new RuntimeException(
                    "Unable to instantiate activity " + component
                    + ": " + e.toString(), e);
            }
        }

        try {

// 3. &#x521B;&#x5EFA; Application&#x5BF9;&#x8C61;
            Application app = r.packageInfo.makeApplication(false, mInstrumentation);

            if (localLOGV) Slog.v(TAG, "Performing launch of " + r);
            if (localLOGV) Slog.v(
                    TAG, r + ": app=" + app
                    + ", appName=" + app.getPackageName()
                    + ", pkg=" + r.packageInfo.getPackageName()
                    + ", comp=" + r.intent.getComponent().toShortString()
                    + ", dir=" + r.packageInfo.getAppDir());

            if (activity != null) {
                CharSequence title = r.activityInfo.loadLabel(appContext.getPackageManager());
                Configuration config =
                        new Configuration(mConfigurationController.getCompatConfiguration());
                if (r.overrideConfig != null) {
                    config.updateFrom(r.overrideConfig);
                }
                if (DEBUG_CONFIGURATION) Slog.v(TAG, "Launching activity "
                        + r.activityInfo.name + " with config " + config);
                Window window = null;
                if (r.mPendingRemoveWindow != null && r.mPreserveWindow) {
                    window = r.mPendingRemoveWindow;
                    r.mPendingRemoveWindow = null;
                    r.mPendingRemoveWindowManager = null;
                }

                // Activity resources must be initialized with the same loaders as the
                // application context.

                appContext.getResources().addLoaders(
                        app.getResources().getLoaders().toArray(new ResourcesLoader[0]));

// 4. &#x8BBE;&#x7F6E;&#x5916;&#x90E8;context&#xFF0C;setOuterContext&#xFF0C;&#x53C2;&#x6570;&#x4E3A;activity
                appContext.setOuterContext(activity);

// 5. appContext&#x4F5C;&#x4E3A;&#x53C2;&#x6570;&#x4F20;&#x5165;&#x5230; activity &#x65B9;&#x6CD5;attach
                activity.attach(appContext, this, getInstrumentation(), r.token,
                        r.ident, app, r.intent, r.activityInfo, title, r.parent,
                        r.embeddedID, r.lastNonConfigurationInstances, config,
                        r.referrer, r.voiceInteractor, window, r.configCallback,
                        r.assistToken, r.shareableActivityToken);
```

**1. 此处创建了 context 对象，ContextImpl appContext = createBaseContextForActivity**

```
// &#x8C03;&#x7528;&#x5176;&#x79C1;&#x6709;&#x65B9;&#x6CD5;&#x8BBE;&#x7F6E;
    private ContextImpl createBaseContextForActivity(ActivityClientRecord r) {
        final int displayId = ActivityClient.getInstance().getDisplayId(r.token);
        ContextImpl appContext = ContextImpl.createActivityContext(
                this, r.packageInfo, r.activityInfo, r.token, displayId, r.overrideConfig);

```

* frameworks/base/core/java/android/app/ContextImpl.java

```
    @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.R, trackingBug = 170729553)
    static ContextImpl createActivityContext(ActivityThread mainThread,
            LoadedApk packageInfo, ActivityInfo activityInfo, IBinder activityToken, int displayId,
            Configuration overrideConfiguration) {
        if (packageInfo == null) throw new IllegalArgumentException("packageInfo");

        String[] splitDirs = packageInfo.getSplitResDirs();
        ClassLoader classLoader = packageInfo.getClassLoader();

// &#x521B;&#x5EFA;&#x4E86;ContextImpl &#x5BF9;&#x8C61;
        ContextImpl context = new ContextImpl(null, mainThread, packageInfo, ContextParams.EMPTY,
                attributionTag, null, activityInfo.splitName, activityToken, null, 0, classLoader,
                null);
// &#x8BBE;&#x7F6E;&#x4E86;resource
        context.setResources(resourcesManager.createBaseTokenResources(activityToken,
                packageInfo.getResDir(),
                splitDirs,
                packageInfo.getOverlayDirs(),
                packageInfo.getOverlayPaths(),
                packageInfo.getApplicationInfo().sharedLibraryFiles,
                displayId,
                overrideConfiguration,
                compatInfo,
                classLoader,
                packageInfo.getApplication() == null ? null
                        : packageInfo.getApplication().getResources().getLoaders()));
        context.mDisplay = resourcesManager.getAdjustedDisplay(displayId,
                context.getResources());
        return context;
    }

// &#x53EF;&#x4EE5;&#x901A;&#x8FC7; getResources&#x83B7;&#x53D6;&#x5BF9;&#x5E94; Resources
    @Override
    public Resources getResources() {
        return mResources;
    }
```

注释4中， 设置外部context，setOuterContext，参数为activity

* 作用：把Activity的实例传递给ContextImpl，这样ContextImpl中的mOuterContext就可以调用Activity的变量和方法

```
    @UnsupportedAppUsage
    final void setOuterContext(@NonNull Context context) {
        mOuterContext = context;
        // TODO(b/149463653): check if we still need this method after migrating IMS to
        //  WindowContext.

        if (mOuterContext.isUiContext() && mContextType <= context_type_display_context) { mcontexttype="CONTEXT_TYPE_WINDOW_CONTEXT;" misconfigurationbasedcontext="true;" } @unsupportedappusage final context getoutercontext() return moutercontext; }< code></=>
```

**5. appContext作为参数传入到 activity 方法attach**

新创建的ContextImpl对象传递到Activity的attach方法

* frameworks/base/core/java/android/app/Activity.java

```
    @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.R, trackingBug = 170729553)
    final void attach(Context context, ActivityThread aThread,
            Instrumentation instr, IBinder token, int ident,
            Application application, Intent intent, ActivityInfo info,
            CharSequence title, Activity parent, String id,
            NonConfigurationInstances lastNonConfigurationInstances,
            Configuration config, String referrer, IVoiceInteractor voiceInteractor,
            Window window, ActivityConfigCallback activityConfigCallback, IBinder assistToken,
            IBinder shareableActivityToken) {
        attachBaseContext(context);

        mFragments.attachHost(null /*parent*/);

        mWindow = new PhoneWindow(this, window, activityConfigCallback);
        mWindow.setWindowControllerCallback(mWindowControllerCallback);
        mWindow.setCallback(this);
        mWindow.setOnWindowDismissedCallback(this);
        mWindow.getLayoutInflater().setPrivateFactory(this);
        if (info.softInputMode != WindowManager.LayoutParams.SOFT_INPUT_STATE_UNSPECIFIED) {
            mWindow.setSoftInputMode(info.softInputMode);
        }
        if (info.uiOptions != 0) {
            mWindow.setUiOptions(info.uiOptions);
        }
        mUiThread = Thread.currentThread();

        mMainThread = aThread;
        mInstrumentation = instr;
        mToken = token;
        mAssistToken = assistToken;
        mShareableActivityToken = shareableActivityToken;
        mIdent = ident;
        mApplication = application;
        mIntent = intent;
```

此处调用父类ContextThemeWrapper的attachBaseContext方法并最终调用ContextWrapper类的attachBaseContext方法，将新创建的ContextImpl对象赋值给ContextWrapper的成员变量mBase，这样ContextWrapper及其子类的mBase成员变量就被实例化为ContextImpl对象。

```
public class Activity extends ContextThemeWrapper

    @Override
    protected void attachBaseContext(Context newBase) {
// &#x8C03;&#x7528;&#x5176;&#x7236;&#x7C7B;&#x7684;&#x65B9;&#x6CD5;
        super.attachBaseContext(newBase);
        if (newBase != null) {
            newBase.setAutofillClient(this);
            newBase.setContentCaptureOptions(getContentCaptureOptions());
        }
    }

frameworks/base/core/java/android/view/ContextThemeWrapper.java
public class ContextThemeWrapper extends ContextWrapper {
// &#x4E5F;&#x662F;&#x8C03;&#x7528;&#x7236;&#x7C7B;&#x65B9;&#x6CD5;
    @Override
    protected void attachBaseContext(Context newBase) {
        super.attachBaseContext(newBase);
    }
```

Activity通过ContextWrapper的成员mBase来引用ContextImpl对象，即Activity组件可通过这个ContextImpl对象来执行一些具体的操作（启动Service等）。同时ContextImpl类又通过自己的成员变量mOuterContext引用了与它关联的Activity，这样ContextImpl类也可以操作Activity。

因此说明一个Activity就有一个Context，而且生命周期和Activity类相同。

### 3. Application 的 Context 创建

如上述代码注释 3.中，创建 Application对象，Application app = r.packageInfo.makeApplication(false, mInstrumentation);

* **frameworks/base/core/java/android/app/LoadedApk.java*

```
    @UnsupportedAppUsage
    public Application makeApplication(boolean forceDefaultAppClass,
            Instrumentation instrumentation) {
        if (mApplication != null) {
            return mApplication;
        }

        Application app = null;

        String appClass = mApplicationInfo.className;
        if (forceDefaultAppClass || (appClass == null)) {
            appClass = "android.app.Application";
        }

        try {
            final java.lang.ClassLoader cl = getClassLoader();
            if (!mPackageName.equals("android")) {
                Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER,
                        "initializeJavaContextClassLoader");
                initializeJavaContextClassLoader();
                Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
            }

            // Rewrite the R 'constants' for all library apks.

            SparseArray<string> packageIdentifiers = getAssets().getAssignedPackageIdentifiers(
                    false, false);
            for (int i = 0, n = packageIdentifiers.size(); i < n; i++) {
                final int id = packageIdentifiers.keyAt(i);
                if (id == 0x01 || id == 0x7f) {
                    continue;
                }

                rewriteRValues(cl, packageIdentifiers.valueAt(i), id);
            }
// 1. &#x521B;&#x5EFA; ContextImpl &#x5BF9;&#x8C61;
            ContextImpl appContext = ContextImpl.createAppContext(mActivityThread, this);
            // The network security config needs to be aware of multiple
            // applications in the same process to handle discrepancies
            NetworkSecurityConfigProvider.handleNewApplication(appContext);
// 2. &#x521B;&#x5EFA; Application &#x5BF9;&#x8C61;
            app = mActivityThread.mInstrumentation.newApplication(
                    cl, appClass, appContext);
// 3. &#x8BBE;&#x7F6E; &#x5916;&#x90E8;&#x7684;context
            appContext.setOuterContext(app);
        } catch (Exception e) {
            if (!mActivityThread.mInstrumentation.onException(app, e)) {
                Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
                throw new RuntimeException(
                    "Unable to instantiate application " + appClass
                    + " package " + mPackageName + ": " + e.toString(), e);
            }
        }
        mActivityThread.mAllApplications.add(app);
        mApplication = app;</string>
```

**// 1. 创建 ContextImpl 对象**

```
    @UnsupportedAppUsage
    static ContextImpl createAppContext(ActivityThread mainThread, LoadedApk packageInfo) {
        return createAppContext(mainThread, packageInfo, null);
    }

    static ContextImpl createAppContext(ActivityThread mainThread, LoadedApk packageInfo,
            String opPackageName) {
        if (packageInfo == null) throw new IllegalArgumentException("packageInfo");
        ContextImpl context = new ContextImpl(null, mainThread, packageInfo,
            ContextParams.EMPTY, null, null, null, null, null, 0, null, opPackageName);
        context.setResources(packageInfo.getResources());
        context.mContextType = isSystemOrSystemUI(context) ? CONTEXT_TYPE_SYSTEM_OR_SYSTEM_UI
                : CONTEXT_TYPE_NON_UI;
        return context;
    }
```

**// 2. 创建 Application 对象**

调用Instrumentation对象newApplication方法来创建Application对象

* frameworks/base/core/java/android/app/Instrumentation.java

```
    public Application newApplication(ClassLoader cl, String className, Context context)
            throws InstantiationException, IllegalAccessException,
            ClassNotFoundException {
        Application app = getFactory(context.getPackageName())
                .instantiateApplication(cl, className);
        app.attach(context);
        return app;
    }

frameworks/base/core/java/android/app/Application.java

public class Application extends ContextWrapper implements ComponentCallbacks2 {
    private static final String TAG = "Application";

    @UnsupportedAppUsage
    /* package */ final void attach(Context context) {
// &#x8C03;&#x7528;&#x7236;&#x7C7B;&#x7684; attachBaseContext&#x65B9;&#x6CD5;
        attachBaseContext(context);
        mLoadedApk = ContextImpl.getImpl(context).mPackageInfo;
    }

frameworks/base/core/java/android/content/ContextWrapper.java

    protected void attachBaseContext(Context base) {
        if (mBase != null) {
            throw new IllegalStateException("Base context already set");
        }
        mBase = base;
    }

```

和之前一样，通过种种方法的传递，调用Application的父类方法attachBaseContext将ContextImpl对象赋值给ContextWrapper类的成员变量mBase，最终使ContextWrapper及其子类包含ContextImpl对象的引用。

注释：3. 设置 外部的context，将新创建的Application对象赋值给ContextImpl对象的成员变量mOuterContext，既让ContextImpl内部持有Application对象的引用。

这样就让二者之间都持有互相的对象，另外可说明一个Application就包含一个Context，而且生命周期和Application类相同，且于应用程序的生命周期相同。

### 4. Service 的 Context 创建

启动Service一般是通过startService和bindService方式，但是无论通过哪种方式来创建新的Service其最终都是通过ActivityThread类的handleCreateService()方法来执行操作

绑定服务的流程可以看：[【安卓 R 源码】 bindService 源码分析](https://mikejun.blog.csdn.net/article/details/121242181)

* **frameworks/base/core/java/android/app/ActivityThread.java*

```

    private void handleCreateService(CreateServiceData data) {
        // If we are getting ready to gc after going to the background, well
        // we are back active so skip it.

        unscheduleGcIdler();

        LoadedApk packageInfo = getPackageInfoNoCheck(
                data.info.applicationInfo, data.compatInfo);
        Service service = null;
        try {
            if (localLOGV) Slog.v(TAG, "Creating service " + data.info.name);

// &#x521B;&#x5EFA; Application &#x5BF9;&#x8C61;
            Application app = packageInfo.makeApplication(false, mInstrumentation);

            final java.lang.ClassLoader cl;
            if (data.info.splitName != null) {
                cl = packageInfo.getSplitClassLoader(data.info.splitName);
            } else {
                cl = packageInfo.getClassLoader();
            }
// 1. &#x901A;&#x8FC7;&#x53CD;&#x5C04;&#x83B7;&#x53D6;&#x5230; service &#x5BF9;&#x8C61;
            service = packageInfo.getAppFactory()
                    .instantiateService(cl, data.info.name, data.intent);

// 2. &#x83B7;&#x53D6; context&#x5BF9;&#x8C61;
            ContextImpl context = ContextImpl.getImpl(service
                    .createServiceBaseContext(this, packageInfo));
            if (data.info.splitName != null) {
                context = (ContextImpl) context.createContextForSplit(data.info.splitName);
            }
            if (data.info.attributionTags != null && data.info.attributionTags.length > 0) {
                final String attributionTag = data.info.attributionTags[0];
                context = (ContextImpl) context.createAttributionContext(attributionTag);
            }
            // Service resources must be initialized with the same loaders as the application
            // context.

            context.getResources().addLoaders(
                    app.getResources().getLoaders().toArray(new ResourcesLoader[0]));
 // &#x53CC;&#x5411;&#x7ED1;&#x5B9A;context &#x548C;service
            context.setOuterContext(service);
// 3. &#x4E0E; context &#x7ED1;&#x5B9A;
            service.attach(context, this, data.info.name, data.token, app,
                    ActivityManager.getService());
// 4. &#x56DE;&#x8C03; onCreate &#x65B9;&#x6CD5;
            service.onCreate();
            mServicesData.put(data.token, data);
            mServices.put(data.token, service);
            try {
                ActivityManager.getService().serviceDoneExecuting(
                        data.token, SERVICE_DONE_EXECUTING_ANON, 0, 0);
            } catch (RemoteException e) {
                throw e.rethrowFromSystemServer();
```

**2. 获取 context对象**

和 【 Application 的 Context 创建】，也是调用 createAppContext创建 ContextImpl 对象

```
    @UnsupportedAppUsage
    static ContextImpl createAppContext(ActivityThread mainThread, LoadedApk packageInfo) {
        return createAppContext(mainThread, packageInfo, null);
    }

    static ContextImpl createAppContext(ActivityThread mainThread, LoadedApk packageInfo,
            String opPackageName) {
        if (packageInfo == null) throw new IllegalArgumentException("packageInfo");
        ContextImpl context = new ContextImpl(null, mainThread, packageInfo,
            ContextParams.EMPTY, null, null, null, null, null, 0, null, opPackageName);
        context.setResources(packageInfo.getResources());
        context.mContextType = isSystemOrSystemUI(context) ? CONTEXT_TYPE_SYSTEM_OR_SYSTEM_UI
                : CONTEXT_TYPE_NON_UI;
        return context;
    }
```

3. 与 context 绑定

* frameworks/base/core/java/android/app/Service.java

```
public abstract class Service extends ContextWrapper implements ComponentCallbacks2,
        ContentCaptureManager.ContentCaptureClient {
    private static final String TAG = "Service";

    public final void attach(
            Context context,
            ActivityThread thread, String className, IBinder token,
            Application application, Object activityManager) {
        attachBaseContext(context);
        mThread = thread;           // NOTE:  unused - remove?

        mClassName = className;
        mToken = token;
        mApplication = application;
        mActivityManager = (IActivityManager)activityManager;
        mStartCompatibility = getApplicationInfo().targetSdkVersion
                < Build.VERSION_CODES.ECLAIR;

        setContentCaptureOptions(application.getContentCaptureOptions());
    }

// &#x76F8;&#x540C;&#x5730;&#xFF0C;&#x4E5F;&#x662F;&#x8C03;&#x7528;&#x7236;&#x7C7B;&#x65B9;&#x6CD5;
    @Override
    protected void attachBaseContext(Context newBase) {
        super.attachBaseContext(newBase);
        if (newBase != null) {
            newBase.setContentCaptureOptions(getContentCaptureOptions());
        }
    }
```

## 4. Context 导致内存泄漏问题：

**1. 错误方式获取 Context**

```

public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        TextView tv = new TextView(MainActivity.this);
        TextView tv2 = new TextView(getApplicationContext()); //&#x8FD9;&#x91CC;&#x4E0D;&#x80FD;&#x4F7F;&#x7528;getApplicationContext()
    }

```

TextView tv = new TextView(MainActivity.this)；tv 这个view 是依赖Activity（界面而存在的）；Activity销毁，tv也会销毁

如果使用TextView tv = new TextView(getApplicationContext())，可能Activity销毁了，但是整个应用程序还没有销毁，这样这个tv会变成空指针，导致内存泄露。

**2. View持有activity 的引用：**

```
public class MainActivity extends Activity {
    private static Drawable mDrawable;

    @Override
    protected void onCreate(Bundle saveInstanceState) {
        super.onCreate(saveInstanceState);
        setContentView(R.layout.activity_main);
        ImageView iv = new ImageView(this);
        mDrawable = getResources().getDrawable(R.drawable.ic_launcher);
        iv.setImageDrawable(mDrawable);
    }
}
```

Drawable是一个静态的对象，当ImageView设置这个Drawable时，ImageView保存了mDrawable的引用，而ImageView传入的this是MainActivity的mContext，因为被static修饰的mDrawable是常驻内存的，MainActivity是它的间接引用，MainActivity被销毁时，也不能被GC掉，所以造成内存泄漏。

**在使用 Context应该尽量注意如下几点：**

1. 如果不涉及Activity的主题样式，尽量使用Application的Context。
2. 不要让生命周期长于Activity的对象持有其的引用。
3. 尽量不要在Activity中使用非静态内部类，因为非静态内部类会隐式持有外部类示例的引用，如果使用静态内部类，将外部实例引用作为弱引用持有。

参考：

[Android中Context源码分析](https://blog.csdn.net/dongxianfei/article/details/52303847?ops_request_misc=%257B%2522request%255Fid%2522%253A%2522163730505116780271567868%2522%252C%2522scm%2522%253A%252220140713.130102334.pc%255Fall.%2522%257D&request_id=163730505116780271567868&biz_id=0&utm_medium=distribute.pc_search_result.none-task-blog-2~all~first_rank_ecpm_v1~rank_v31_ecpm-3-52303847.first_rank_v2_pc_rank_v29&utm_term=%E5%AE%89%E5%8D%93Context%E6%BA%90%E7%A0%81&spm=1018.2226.3001.4187)

[Android 10 源码理解上下文Context](https://blog.csdn.net/qq_30359699/article/details/108282355?ops_request_misc=&request_id=&biz_id=102&utm_term=%E5%AE%89%E5%8D%93Context%E6%BA%90%E7%A0%81&utm_medium=distribute.pc_search_result.none-task-blog-2~all~sobaiduweb~default-2-108282355.first_rank_v2_pc_rank_v29&spm=1018.2226.3001.4187)

[Android Context的设计思想及源码分析](https://blog.csdn.net/u011897062/article/details/109334192?ops_request_misc=&request_id=&biz_id=102&utm_term=%E5%AE%89%E5%8D%93Context%E6%BA%90%E7%A0%81&utm_medium=distribute.pc_search_result.none-task-blog-2~all~sobaiduweb~default-5-109334192.first_rank_v2_pc_rank_v29&spm=1018.2226.3001.4187)

[Android源码解析--Context详解](https://blog.csdn.net/xmxkf/article/details/107962926?ops_request_misc=&request_id=&biz_id=102&utm_term=%E5%AE%89%E5%8D%93Context%E6%BA%90%E7%A0%81&utm_medium=distribute.pc_search_result.none-task-blog-2~all~sobaiduweb~default-4-107962926.first_rank_v2_pc_rank_v29&spm=1018.2226.3001.4187)
