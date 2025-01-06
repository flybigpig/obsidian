## 一、引言

Android里面内存泄漏问题最突出的就是Activity的泄漏，而泄漏的根源大多在于因为生命周期较长的对象去引用生命周期较短的Activity实例，也就会造成在Activity生命周期结束后，还被引用导致无法被系统回收释放。

Activity导致内存泄漏有两种情况：

1. 应用级：应用程序代码实现的activity没有很好的管理其生命周期，导致Activity退出后仍然被引用。
2. 系统级：Android系统级实现的对activity管理不太友好，被应用调用导致内存泄漏。

本文主要讲的是最近发现的系统级shouldShowRequestPermissionRationale方法使用导致的内存泄漏问题。

## 二、背景

Android 6.0 (API 23) 之前应用的权限在安装时全部授予，运行时应用不再需要询问用户。在 Android 6.0 或更高版本对权限进行了分类，对某些涉及到用户隐私的权限可在运行时根据用户的需要动态获取。主要流程如下：

![](https://upload-images.jianshu.io/upload_images/26874665-f5cbfd6ccfebf14e.png?imageMogr2/auto-orient/strip|imageView2/2/w/1006/format/webp)

权限申请流程

这个过程中会遇到这么几个方法：

- ContextCompat.checkSelfPermission，检查应用是否有权限
    
- ActivityCompat.requestPermissions，请求某个或某几个权限
    
- onRequestPermissionsResult，请求权限之后的授权结果回调
    
- shouldShowRequestPermissionRationale
    

前三个方法的用途都非常清楚，使用也很简单，这里不做过多解释，今天主要看下shouldShowRequestPermissionRationale，看下它是干什么用的：

当APP调用一个需要权限的函数时，如果用户拒绝某个授权，下一次弹框时将会有一个“禁止后不再询问”的选项，来防止APP以后继续请求授权。如果这个选项在拒绝授权前被用户勾选了，下次为这个权限请求requestPermissions时，对话框就不弹出来了，结果就是app啥都不干。遇到这种情况需要在请求requestPermissions前，检查是否需要展示请求权限的提示，这时候用的就是shouldShowRequestPermissionRationale方法。

shouldShowRequestPermissionRationale字面解释是“应不应该解释下请求这个权限的目的”，下面列举了此方法使用时的4种情况及相应情况下的返回值：

1. 都没有请求过这个权限，用户不一定会拒绝你，所以你不用解释，故返回false;
2. 请求了但是被用户拒绝了，此时返回true，意思是你该向用户好好解释下了；
3. 用户选择了拒绝并且不再提示，也不给你弹窗提醒了，所以你也不用解释了，故返回fasle;
4. 已经允许了，不需要申请也不需要提示，故返回false。

## 三、调用案例及内存泄漏隐患

### 3.1正常权限申请流程

通常我们申请权限时先调用checkSelfPermission方法检验应用是否有需要使用的权限，没有相应权限时调用shouldShowRequestPermissionRationale方法检查是否需要展示请求权限的提示。不需要展示提示时再调用requestPermissions方法进行权限请求。代码如下：

![](https://upload-images.jianshu.io/upload_images/26874665-3ff4b16943f10fb1.png?imageMogr2/auto-orient/strip|imageView2/2/w/1076/format/webp)

Demo源码

### 3.2内存泄漏隐患发现

在Android S上，我们使用上述方式进行权限获取时会发现只要你调用了shouldShowRequestPermissionRationale方法，当MainActivity生命周期结束后MainActivity都不会被回收。我们可以不给予所需权限多次进入退出此应用运行一段时间（保证每次都会调用到shouldShowRequestPermissionRationale方法）。使用adb命令dump meminfo查看内存情况。可以看到activity实例数为25，表明acticity虽然被销毁但是因为被其他对象持有所以并没有被GC。 注意：此处测试是通过返回键退出activity的，我们的Demo在返回键的监听有调用finish方法确保结束activity。每次重新进入都会重新执行onCreate方法。因为Android S上使用返回键退出应用并不会直接销毁activity，而只有当应用主动调用finish或者非启动类型的activity才会去销毁。

![](https://upload-images.jianshu.io/upload_images/26874665-f57f048bc1408dfd.png?imageMogr2/auto-orient/strip|imageView2/2/w/993/format/webp)

Demo内存占用情况

此时我们已经发现此Demo存在明显的内存泄露问题。下面我们使用Memory Profiler工具抓取hprof文件进行内存分析，看看是哪里发生了内存泄漏。

![](https://upload-images.jianshu.io/upload_images/26874665-b9dec1c42d356e0c.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

使用Memory Profiler工具进行内存泄漏分析

1. 可以看到Memory Profiler工具已经提示我们有com.nubia.application包下MainActivity有24个对象发生了内存泄露。
    
2. 可以看到当前MainActivity总共实例有25个和adb命令查询出来吻合。其他24个实例没有被GC导致内存泄露。
    
3. References标签页可以看到其他MainActivity实例被AppOpsManager持有导致无法被GC。
    
4. 我们可以看到Memory Profiler工具提示共有48个对象产生内存泄露那么其他24个是哪里产生的呢？点击Leaks进行查看如下图。
    
    ![](https://upload-images.jianshu.io/upload_images/26874665-24fe5c34c7e64767.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)
    
    使用Memory Profiler工具进行内存泄漏分析
    
5. 可以看到除了MainActivity产生了内存泄露，ReportFragment也产生了内存泄露。查看ReportFragment的Instance Details标签页可以看到ReportFragment的实例被MainActivity持有而MainActivity被AppOpsManager持有所以产生了内存泄露。
    
6. 至此Demo中所有内存泄露问题分析完成。
    

### 3.3内存泄漏隐患分析

在上一节中我们已经发现Demo在使用过程中存在内存泄漏问题。只要我们调用shouldShowRequestPermissionRationale方法，当Activity生命周期结束时就会发生Activity内存泄漏。那么这一节我们来具体分析下为什么调用shouldShowRequestPermissionRationale方法会发生内存泄漏。

我们先来看一下shouldShowRequestPermissionRationale方法调用整个过程的时序图：

![](https://upload-images.jianshu.io/upload_images/26874665-972ee05ad321f23b.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

shouldShowRequestPermissionRationale方法调用时序图

看完调用流程图后，我们再来一步一步分析shouldShowRequestPermissionRationale具体是怎么调用的，以及为什么会产生内存泄漏？

先从Activity的shouldShowRequestPermissionRationale方法开始，Activity调用的是packageManager的shouldShowRequestPermissionRationale方法。

```java
public boolean shouldShowRequestPermissionRationale(@NonNull String permission) {
    //调用ContextImpl的getPackageManager()方法获取PackageManager实例
    //然后调用PackageManager的shouldShowRequestPermissionRationale方法
    return getPackageManager().shouldShowRequestPermissionRationale(permission);
}
```

我们知道Activity是从ContextWrapper继承而来的，ContextWrapper中持有一个mBase实例，这个实例指向一个contextImpl对象，Activity的getPackageManager这个方法调用的就是contextImpl的getPackageManager方法。

```java
private PackageManager mPackageManager;
...
public PackageManager getPackageManager() {
    if (mPackageManager != null) {
        return mPackageManager;
    }
    //获取PackageManagerService代理对象
    final IPackageManager pm = ActivityThread.getPackageManager();
    if (pm != null) {
        // Doesn't matter if we make more than one instance.
        //创建ApplicationPackageManager实例,传入contextImpl对象
        return (mPackageManager = new ApplicationPackageManager(this, pm));
    }
    return null;
}
```

可以看到contextImpl的getPackageManager方法中会创建ApplicationPackageManager实例同时传入contextImpl对象，然后调用ApplicationPackageManager的shouldShowRequestPermissionRationale方法。

```java
private PermissionManager mPermissionManager;
protected ApplicationPackageManager(ContextImpl context, IPackageManager pm) {
    //传入的contextImpl和pm实例 
    mContext = context;
    mPM = pm;
}
private PermissionManager getPermissionManager() {
    synchronized (mLock) {
        if (mPermissionManager == null) {
            //获取PermissionManager对象
            //contextImpl.getSystemService实现
            mPermissionManager = mContext.getSystemService(PermissionManager.class);
        }
        return mPermissionManager;
    }
}
public boolean shouldShowRequestPermissionRationale(String permName) {
    //调用PermissionManager的shouldShowRequestPermissionRationale方法
    return getPermissionManager().shouldShowRequestPermissionRationale(permName);
} 
```

ApplicationPackageManager中调用的是PermissionManager的shouldShowRequestPermissionRationale方法。获取PermissionManager时会调用contextImpl的getSystemService方法。getSystemService方法调用由SystemServiceRegistry来完成。

```java
public final @Nullable <T> T getSystemService(@NonNull Class<T> serviceClass) {
    String serviceName = getSystemServiceName(serviceClass);
    return serviceName != null ? (T)getSystemService(serviceName) : null;
}
public String getSystemServiceName(Class<?> serviceClass) {
    //通过class对象获取服务名
    return SystemServiceRegistry.getSystemServiceName(serviceClass);
}
public Object getSystemService(String name) {
    //通过服务名获取服务，传入contextImpl实例对象
    return SystemServiceRegistry.getSystemService(this, name);
}
```

SystemServiceRegistry提供PermissionManager的实例。SystemServiceRegistry在静态注册PermissionManager会传入contextImpl的outerContext对象，这个outerContext就是Activity对象。

```java
public static Object getSystemService(ContextImpl ctx, String name) {
    if (name == null) {
        return null;
    }
    //通过服务名获取对应服务
    final ServiceFetcher<?> fetcher = SYSTEM_SERVICE_FETCHERS.get(name);
    ...
    //返回对应服务
    final Object ret = fetcher.getService(ctx);
    ...
    return ret;
}
static{
    //静态注册PermissionManager
    registerService(Context.PERMISSION_SERVICE, PermissionManager.class,
                    new CachedServiceFetcher<PermissionManager>() {
                        @Override
                        public PermissionManager createService(ContextImpl ctx)
                            throws ServiceNotFoundException {
                            //创建PermissionManager实例 传入activity实例对象
                            return new PermissionManager(ctx.getOuterContext());
                        }});
}
```

精彩的部分来了，在PermissionManager的构造方法中会创建PermissionUsageHelper对象并传入context，这个context是SystemServiceRegistry中contextImpl对象持有的outerContext对象，就是一开始的Activity对象，所以PermissionUsageHelper的单实例持有了Activity的实例引用。

看到这里再结合上节的内存泄漏隐患发现时hprof文件中的GC Root我们可以知道，内存泄漏就是这个PermissionUsageHelper构造时持有Activity对象最终在Acitvity生命周期结束时没有被释放导致的。

```java
public PermissionManager(@NonNull Context context) throws ServiceManager.ServiceNotFoundException {
    mContext = context;
    mPackageManager = AppGlobals.getPackageManager();
    //获取PermissionManagerService代理对象
    mPermissionManager = IPermissionManager.Stub.asInterface(ServiceManager.getServiceOrThrow("permissionmgr"));
    mLegacyPermissionManager = context.getSystemService(LegacyPermissionManager.class);
    //TODO ntmyren: there should be a way to only enable the watcher when requested
    //Android S新增,初始化 PermissionUsageHelper 引发内存泄漏问题
    mUsageHelper = new PermissionUsageHelper(context);
}
```

我们再来看PermissionManager中的shouldShowRequestPermissionRationale方法，最终调用的是PermissionManagerService代理对象的shouldShowRequestPermissionRationale方法。

到这里我们基本走完了shouldShowRequestPermissionRationale方法的调用流程。也知道调用shouldShowRequestPermissionRationale方法产生的内存泄漏是因为在获取PermissionManager时创建PermissionUsageHelper导致的。这个问题仅在Android S上出现，在Android_R上PermissionManager构造方法并没有去创建PermissionUsageHelper所以也不会有内存泄露问题。下面我们再来看为什么创建PermissionUsageHelper时传入Activity对象会导致内存泄漏？Activity又是被谁一直持有的？

```java
public boolean shouldShowRequestPermissionRationale(@NonNull String permissionName) {
    try {
        final String packageName = mContext.getPackageName();
        //调用PermissionManagerService的shouldShowRequestPermissionRationale方法
        return mPermissionManager.shouldShowRequestPermissionRationale(packageName, permissionName, mContext.getUserId());
    } catch (RemoteException e) {
        throw e.rethrowFromSystemServer();
    }
}
```

在PermissionUsageHelper构造方法中会把Activity对象赋值给mContext这个成员变量，然后获取AppOpsManager对象，并调用AppOpsManager的startWatchingActive和startWatchingStarted方法传入this作为回调用来监听应用程序状态和权限状态监听。

这里的startWatchingActive和startWatchingStarted方法最终会调用AppOpsService的startWatchingActive和startWatchingStarted方法并把传入的this也就是PermissionUsageHelper保存起来。而PermissionUsageHelper又持有Activivty实例，导致AppOpsService间接保存Acitivty实例。当应用程序主动调用destroy方法时，AppOpsService并没有移除应用监听和权限状态监听，仍然保存着Acitivty实例导致Acitivty无法释放产生内存泄漏。

```java
public PermissionUsageHelper(@NonNull Context context) {
    //Activity上下文对象
    mContext = context;
    mPkgManager = context.getPackageManager();
    //获取AppOpsManager
    mAppOpsManager = context.getSystemService(AppOpsManager.class);
    mUserContexts = new ArrayMap<>();
    mUserContexts.put(Process.myUserHandle(), mContext);
    // TODO ntmyren: make this listen for flag enable/disable changes
    String[] opStrs = { OPSTR_CAMERA, OPSTR_RECORD_AUDIO };
    // 监听应用程序状态，此处this实现了OnOpActiveChangedListener接口作为callbackck传入
    mAppOpsManager.startWatchingActive(opStrs, context.getMainExecutor(), this);
    int[] ops = { OP_CAMERA, OP_RECORD_AUDIO };
    // 监听权限状态，此处this实现了OnOpStartedListener接口作为callbackck传入
    mAppOpsManager.startWatchingStarted(ops, this);
}
```

## 四、总结

Android S上增加权限指示器功能PermissionUsageHelper，这个类它获取所有使用过麦克风、相机和可能的位置许可的应用程序，在特定的时间范围内，以及可能的特殊属性，监听应用程序使用此类权限的状态。调用shouldShowRequestPermissionRationale方法产生内存泄露的根本原因是获取PermissionManager时会创建PermissionUsageHelper对象并监听应用程序状态和权限状态。

只有应用进程退出或者手机进入IDLE状态，才释放activity并且未使用camera、audio和location权限的应用同样会去做监听，在Android S上重构权限模块明显导致了activity泄漏问题。不主动finish应用的actiivity虽然不会导致这个问题，但是应用场景使用情况很多，从框架原生代码逻辑来说是不合理的。

这个问题目前仅在Android S上出现，其他会使用PermissionManager的场景没认真研究，不确定有没有这个问题。不过为了避免类似的情况发生，最好的解决办法就是：

对PermissionUsageHelper进行修改，判断应用是否有camera、location、audio权限，应用程序activity退出时，主动调用stop方法，移除监听。

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/b0de542204f8  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。