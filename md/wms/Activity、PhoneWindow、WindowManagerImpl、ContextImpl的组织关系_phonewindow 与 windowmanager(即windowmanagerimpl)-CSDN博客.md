```
ActivityThread.java

private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
    
    ContextImpl appContext = createBaseContextForActivity(r);
    
    activity.attach(appContext, this, getInstrumentation(), r.token,
        r.ident, app, r.intent, r.activityInfo, title, r.parent,
        r.embeddedID, r.lastNonConfigurationInstances, config,
        r.referrer, r.voiceInteractor, window, r.activityConfigCallback,
        r.assistToken, r.shareableActivityToken);
}


private ContextImpl createBaseContextForActivity(ActivityClientRecord r) {
    ContextImpl appContext = ContextImpl.createActivityContext(
        this, r.packageInfo, r.activityInfo, r.token, displayId, r.overrideConfig);
        
    return appContext;
}
```

```
ContextImpl.java

static ContextImpl createActivityContext(ActivityThread mainThread,
        LoadedApk packageInfo, ActivityInfo activityInfo, IBinder activityToken, int displayId,
        Configuration overrideConfiguration) {
        
    ContextImpl context = new ContextImpl(null, mainThread, packageInfo, ContextParams.EMPTY,
        attributionTag, null, activityInfo.splitName, activityToken, null, 0, classLoader,
        null);
        
    return context;
            
}
```

综上，appContext本质上是个ContextImpl实例。

```
Activity.java

final void attach(Context context, ActivityThread aThread,
        Instrumentation instr, IBinder token, int ident,
        Application application, Intent intent, ActivityInfo info,
        CharSequence title, Activity parent, String id,
        NonConfigurationInstances lastNonConfigurationInstances,
        Configuration config, String referrer, IVoiceInteractor voiceInteractor,
        Window window, ActivityConfigCallback activityConfigCallback, IBinder assistToken,
        IBinder shareableActivityToken) {
        
    attachBaseContext(context);
        
    mWindow = new PhoneWindow(this, window, activityConfigCallback);         
        
    mWindow.setWindowManager(
        (WindowManager)context.getSystemService(Context.WINDOW_SERVICE),
        mToken, mComponent.flattenToString(),
        (info.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED) != 0);
}       
 

protected void attachBaseContext(Context newBase) {
    super.attachBaseContext(newBase);
    if (newBase != null) {
        newBase.setAutofillClient(getAutofillClient());
        newBase.setContentCaptureOptions(getContentCaptureOptions());
    }
}
```

```
ContextThemeWrapper.java

protected void attachBaseContext(Context newBase) {
    super.attachBaseContext(newBase);
}
```

```
ContextWrapper.java

protected void attachBaseContext(Context base) {
    if (mBase != null) {
        throw new IllegalStateException("Base context already set");
    }
    mBase = base;
}
```

这里可以看出Context是[装饰器模式](https://so.csdn.net/so/search?q=%E8%A3%85%E9%A5%B0%E5%99%A8%E6%A8%A1%E5%BC%8F&spm=1001.2101.3001.7020)。并且Activity的mContext字段，被赋值为一个新建的ContextImpl实例。

```
PhoneWindow.java

public PhoneWindow(@UiContext Context context, Window preservedWindow,
        ActivityConfigCallback activityConfigCallback) {
    this(context);
    
}

public PhoneWindow(@UiContext Context context) {
    super(context);
}
```

```
Window.java

public Window(@UiContext Context context) {
    mContext = context;
    mFeatures = mLocalFeatures = getDefaultFeatures(context);
}


public void setWindowManager(WindowManager wm, IBinder appToken, String appName,
        boolean hardwareAccelerated) {
    mAppToken = appToken;
    mAppName = appName;
    mHardwareAccelerated = hardwareAccelerated;
    if (wm == null) {
        wm = (WindowManager)mContext.getSystemService(Context.WINDOW_SERVICE);
    }
    mWindowManager = ((WindowManagerImpl)wm).createLocalWindowManager(this);
}
```

跟Activity一一对应的PhoneWindow，其mContext字段，用的就是Activity[向上转型](https://so.csdn.net/so/search?q=%E5%90%91%E4%B8%8A%E8%BD%AC%E5%9E%8B&spm=1001.2101.3001.7020)后的Context实例。  
Activity自身是Context的子类，可以向上转型为Context，从装饰器模式角度看，是Context的封装类；  
Activity继承得到的mBase字段，本质上是ContextImpl实例，从装饰器模式角度看，是Context的具体实现类。  
PhoneWindow的mContext字段，本质上是Activity自身向上转型后的Context类型实例。  
进一步，PhoneWindow、Activity、ContextImpl是一一对应的。  
A持有PW的引用，A持有CI的引用。

```
mWindow.setWindowManager(
        (WindowManager)context.getSystemService(Context.WINDOW_SERVICE),
        mToken, mComponent.flattenToString(),
        (info.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED) != 0);
```

接下来揭秘，Activity对应一个PhoneWindow，PW对应一个WindowManager，WindowManager本质上是WindowManagerImpl。  
上述已经表明，此context本质上是ContextImpl实例。

```
ContextImpl.java

// 调用Context构造函数的时候，会调用SystemServiceRegistry的createServiceCache()
// 该方法是静态方法，经过测试发现。
// 调用该静态方法前，会按照代码顺序，依次执行SystemServiceRegistry中的静态代码段。
// 执行静态片段，就会在SystemServiceRegistry中register各种服务
class ContextImpl extends Context {

    // 每个ContextyImpld都会缓存所有的系统服务
    // The system service cache for the system services that are cached per-ContextImpl.
    final Object[] mServiceCache = SystemServiceRegistry.createServiceCache();

}

// 每个ContextyImpld都会缓存所有的系统服务
// 用Object类的实例，表示每个服务
//Creates an array which is used to cache per-Context service instances.
public static Object[] createServiceCache() {
    return new Object[sServiceCacheSize];
}

public Object getSystemService(String name) {
    
    //  ContextImpl再取系统服务的时候，要将自身作为参数传进去
    return SystemServiceRegistry.getSystemService(this, name);
}
```

```
SystemServiceRegistry.java

public static Object getSystemService(ContextImpl ctx, String name) {
    if (name == null) {
        return null;
    }
    final ServiceFetcher<?> fetcher = SYSTEM_SERVICE_FETCHERS.get(name);
    
    // 下面会调用register服务时，重写的CachedServiceFetcher的createService(ContextImpl ctx)
    final Object ret = fetcher.getService(ctx);
    
    return ret;
    
}

static <T> void registerService(@NonNull String serviceName,
        @NonNull Class<T> serviceClass, @NonNull ServiceFetcher<T> serviceFetcher) {
    SYSTEM_SERVICE_NAMES.put(serviceClass, serviceName);
    SYSTEM_SERVICE_FETCHERS.put(serviceName, serviceFetcher);
    SYSTEM_SERVICE_CLASS_NAMES.put(serviceName, serviceClass.getSimpleName());
}

registerService(Context.WINDOW_SERVICE, WindowManager.class,
        new CachedServiceFetcher<WindowManager>() {
    @Override
    public WindowManager createService(ContextImpl ctx) {
        return new WindowManagerImpl(ctx);
}});
```

```
static abstract class CachedServiceFetcher<T> implements ServiceFetcher<T> {
    private final int mCacheIndex;

    CachedServiceFetcher() {
        // Note this class must be instantiated only by the static initializer of the
        // outer class (SystemServiceRegistry), which already does the synchronization,
        // so bare access to sServiceCacheSize is okay here.
        mCacheIndex = sServiceCacheSize++;
    }

    @Override
    @SuppressWarnings("unchecked")
    public final T getService(ContextImpl ctx) {
        final Object[] cache = ctx.mServiceCache;
        final int[] gates = ctx.mServiceInitializationStateArray;
        boolean interrupted = false;

        T ret = null;

        for (;;) {
            boolean doInitialize = false;
            synchronized (cache) {
                // Return it if we already have a cached instance.
                T service = (T) cache[mCacheIndex];
                if (service != null) {
                    ret = service;
                    break; // exit the for (;;)
                }

                // If we get here, there's no cached instance.

                // Grr... if gate is STATE_READY, then this means we initialized the service
                // once but someone cleared it.
                // We start over from STATE_UNINITIALIZED.
                // Similarly, if the previous attempt returned null, we'll retry again.
                if (gates[mCacheIndex] == ContextImpl.STATE_READY
                        || gates[mCacheIndex] == ContextImpl.STATE_NOT_FOUND) {
                    gates[mCacheIndex] = ContextImpl.STATE_UNINITIALIZED;
                }

                // It's possible for multiple threads to get here at the same time, so
                // use the "gate" to make sure only the first thread will call createService().

                // At this point, the gate must be either UNINITIALIZED or INITIALIZING.
                if (gates[mCacheIndex] == ContextImpl.STATE_UNINITIALIZED) {
                    doInitialize = true;
                    gates[mCacheIndex] = ContextImpl.STATE_INITIALIZING;
                }
            }

            if (doInitialize) {
                // Only the first thread gets here.

                T service = null;
                @ServiceInitializationState int newState = ContextImpl.STATE_NOT_FOUND;
                try {
                    // This thread is the first one to get here. Instantiate the service
                    // *without* the cache lock held.
                    
                    // 结合上面的重写，WMI跟ContextImpl，也是一一对应的。
                    service = createService(ctx);
                    newState = ContextImpl.STATE_READY;

                } catch (ServiceNotFoundException e) {
                    onServiceNotFound(e);

                } finally {
                    synchronized (cache) {
                        cache[mCacheIndex] = service;
                        gates[mCacheIndex] = newState;
                        cache.notifyAll();
                    }
                }
                ret = service;
                break; // exit the for (;;)
            }
            // The other threads will wait for the first thread to call notifyAll(),
            // and go back to the top and retry.
            synchronized (cache) {
                // Repeat until the state becomes STATE_READY or STATE_NOT_FOUND.
                // We can't respond to interrupts here; just like we can't in the "doInitialize"
                // path, so we remember the interrupt state here and re-interrupt later.
                while (gates[mCacheIndex] < ContextImpl.STATE_READY) {
                    try {
                        // Clear the interrupt state.
                        interrupted |= Thread.interrupted();
                        cache.wait();
                    } catch (InterruptedException e) {
                        // This shouldn't normally happen, but if someone interrupts the
                        // thread, it will.
                        Slog.w(TAG, "getService() interrupted");
                        interrupted = true;
                    }
                }
            }
        }
        if (interrupted) {
            Thread.currentThread().interrupt();
        }
        return ret;
    }

    public abstract T createService(ContextImpl ctx) throws ServiceNotFoundException;
}
```

可知，PhoneWindow的mWindowManager字段，本质上是WindowManagerImpl(ctx)。  
并且，WindowManagerImpl()的mContext，跟Activity继承父类ContextWrapper的mBase都是ContextImpl实例。

## 小结

A持有PW的引用，A持有CI的引用，PW持有PWI的引用，PWI持有CI的引用。  
每个Activity对应一个WindowManagerImpl。  
PW继承W的mContext，是Activity的向上转型；  
Activity继承父类ContextWrapper的mBase字段，本质上是ContextImpl实例。  
[WMI](https://so.csdn.net/so/search?q=WMI&spm=1001.2101.3001.7020)中的mContext也是Activity中的mBase；

每个ContextImpl都有属于自己的所有系统服务的缓存，这些缓存的服务，并不是所有ContextImpl共享的。每个ContextImpl在拿wm服务的时候，都会调用重写的CachedServiceFetcher的createService(ContextImpl ctx)，将自己当参数传进去。创建独属于自己的WMI。