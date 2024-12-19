## 回顾

回顾一下: 1.第一个启动的就是`init`进程，它解析了`init.rc`文件，启动各种`service`:`zygote`,`surfaceflinger`,`service_manager`。

2.接着就讲了`Zygote`，Zygote就是一个孵化器，它开启了`system_server`以及开启了`ZygoteServer`用来接收客户端的请求，当客户端请求来了之后就会`fork`出来子进程，并且初始化`binder` 和`进程信息`，为了加速`Zygote`还会预加载一些`class`和`Drawable`、`color`等系统资源。

3.接下来讲了`system_server`，它是系统启动管理`service`的`入口`，比如`AMS`、`PMS`、`WMS`等等，它加载了`framework-res.apk`,接着调用`startBootstrapService`,`startCoreService`,`startOtherService`开启非常多的服务，还开启了`WatchDog`，来监控service。

4.接着讲了`service_manager`，他是一个独立的进程，它存储了系统各种服务的`Binder`，我们经常通过`ServiceMananger`来获取，其中还详细说了`Binder`机制，`C/S`架构，大家要记住`客户端`、`Binder`、`Server`三端的工作流程。

5.之后讲了`Launcher`，它由`system_server`启动，通过`LauncherModel`进行`Binder`通信 通过`PMS`来查询所有的应用信息，然后绑定到`RecyclerView`中，它的点击事件是通过`ItemClickHandler`来处理。

6.接着讲了`AMS`是如何开启应用进程的,首先我们从`Launcher`的点击开始，调用到`Activity`的`startActivity`函数，通过`Instrumentation`的`execStartActivity`经过两次`IPC`(1.通过ServiceManager获取到ATMS 2.调用ATMS的startActivity) 调用到`AMS`端在AMS端进行了一系列的信息处理，会判断进程是否存在，`没有存在的话就会开启进程(通过Socket，给ZygoteServer发送信息)`,传入`entryPoint`为`ActivityThread`,通过`Zygote`来`fork`出来子进程（应用进程）调用`ActivityThread.main`，应用进程创建之后会调用到`AMS`，由`AMS`来`attachApplication`存储进程信息,然后告诉`客户端`，让客户端来创建`Application`，并在客户端创建成功之后 继续执行开启`Activity`的流程。客户端接收到`AMS`的数据之后会创建`loadedApk`,`Instrumentation` 以及`Application调用attach(attachBaseContext)`，调用`Instrumentation`的`callApplicationOncreate`执行`Application`的`Oncreate`周期.

7.应用执行完`Application`的`OnCreate`之后 回到`ATMS`的`attachApplication` 接着调用 `realStartActivityLocked` 创建了`ClientTransaction`，设置`callBack`为`LaunchActivityItem`添加了`stateRequest` 为`ResumeActivityItem`，然后通过`IApplicationThread` 回到客户端执行这两个事务,调用了`ActivityThread`的`scheduleTransaction` 函数，调用`executeCallBack` 执行了`LaunchActivityItem`的execute 他会调用ActivityThread的 `handleLaunchActivity`，会创建Activity Context，通过`Instrumentation.newActivity 反射创建Activity` 并调用`attach 绑定window` 再通过`Instrumentation`的`callActivityOnCreate`执行Activity的`onCreate`，在Activity的onCreate中分发监听给`ActivityLifecycleCallbacks`。最后设置`ActivityClientRecord`的state为`ON_CREATE`。 接着执行`executeLifecycleState`，调用了`cycleToPath`，之前设置了state为ON\_CREATE，所以会返回一个Int数组`{2}` 调用`performLifecycleSequence`会执行到ActivityThread的`handleStartActivity`分发`ActivityLifecycleCallbacks`，并且分发给Fragments，调用`Instrumentation`的`callActivityOnStart` 执行Activity的`onStart`并设置state为`ON_START`，接着执行`ResumeActivityItem`的`execute`，会调用到ActivityThread的`handleResumeActivity`，调用`performResume` 分发resume事件给ActivityLifecycleCallbacks，分发Fragments，调用Instrumentation的`callActivityOnResume` 执行Activity的onResume。 最后会调用`ActivityClientRecord.activity.makeVisible` 通过`WindowManager` 添加当前View 和 WMS(IPC) 通信 绘制UI，接着postResume 会执行 `ATMS`的`activityresume` 设置 AMS的Activity的状态。

8.接着讲了Service，介绍了Service是如何开启的，生命周期是怎么执行的，ANR 是如何弹出的。Service的启动分两种:`startService`和`bindService`。先回忆下startService:startService会调用到ContextImpl的startService，它会直接调用AMS的startService。在`AMS`这里会先检查Service是否可以执行(常驻内存、蓝牙、电源白名单允许直接启动服务)，接着调用`bringUpServiceLocked` 判断是否需要隔离进程如果非隔离 就看是否已经启动进程 执行`realStartServiceLocked`，否则是隔离进程 直接开启新进程。开启成功之后会将ServiceRecord添加到`mPendingServices`中去。进程创建之后，会调用`AMS`的`attachApplication` 接着处理`service(ActiveService)`，之前创建之后会添加到`mPendingServices`中，现在继续处理调用`realStartServiceLocked`来开启Service，在开启的过程中会埋入一个炸弹(给Handler发送一个`SERVICE_TIMEOUT_MSG`) 如果超时未处理会弹出`ANR`，然后调用`app.thread.scheduleCreateService`通知客户端创建反射`Service、Context` 调用`onCreate` 把`Service`存入到`mService`中，调用`AMS`的`serviceDoneExecuting`进行炸弹的拆除。 然后再埋炸弹 调用`app.thread.scheduleServiceArgs` 调用`service.onStartCommand`再通知AMS 拆除炸弹。 这样Service就运行起来了。

9.再回忆下bindService:首先`ServiceConnection`是无法跨进程通信的，所以在`LoadedApk.java`中帮我们封装了`InnerConnection` 它继承自Stub，也就是native层的JavaBBinder,然后通过`bindIsolatedService`将`sd(InnerConnection)`写入到Parcel中，调用`AMS(BinderProxy)`的`transact`进行IPC 通信，把InnerConnection存入到客户端的`nodes`中,以及给`AMS`的`refs_by_node`和`refs_by_desc`挂上`InnerConnection` 接着唤醒`AMS`,唤醒之后调用`onTransact`读取到传过来的`sd`并且包装成`BpBinder(BinderProxy)`返回,这样AMS就在`bindIsolatedService`的时候拿到了InnerConnection的BpBinder，接着AMS 通过`pkms`通过intent查找到服务端进程,创建`AppBindRecord`和`ConnectionRecord`,接着调用`bringUpServiceLocked` 看是否开启进程 如果没有开启就先开启进程，开启了进程之后 调用`realStartServiceLocked` 也埋了炸弹 接着调用 `app.thread.scheduleCreateService`通知客户端创建反射`Service、Context` 调用`onCreate` 把`Service`存入到`mService`中,调用`serviceDoneExecutingLocked`把炸弹拆除，再调用`requestServiceBindingsLocked` 去执行`r.app.thread.scheduleBindService` 执行服务端的onBind函数拿到了返回的`Binder`再调用AMS的`publishService`把服务发布到AMS（在服务端的进程创建`binder_proc(server)` 在AMS的`refs_by_node` 和`refs_by_desc 挂上server）`,`AMS`的`onTransact` 中读取到服务端返回的`server(Binder)`包装成`BpBinder`，然后调用`c.conn.connected(从refs_by_desc 和 refs_by_node 找到InnerConnection)`调用connect 所以到了客户端进程会把server的BpBinder 挂在`refs_by_desc`和`refs_by_node`上边。再调用`Servcie.onServiceConnected` 之后拆除炸弹。

具体的细节可以参考之前写的文章和视频：

[【Android FrameWork】第一个启动的程序--init](https://juejin.cn/post/7213733567606685753 "https://juejin.cn/post/7213733567606685753")

[【Android FrameWork】Zygote](https://juejin.cn/post/7214068367607119933 "https://juejin.cn/post/7214068367607119933")

[【Android FrameWork】SystemServer](https://juejin.cn/post/7214493929566945340 "https://juejin.cn/post/7214493929566945340")

[【Android FrameWork】ServiceManager(一)](https://juejin.cn/post/7216537771448598585#heading-4 "https://juejin.cn/post/7216537771448598585#heading-4")

[【Android FrameWork】ServiceManager(二)](https://juejin.cn/post/7216536069285675045#heading-9 "https://juejin.cn/post/7216536069285675045#heading-9")

[【Android Framework】Launcher3](https://juejin.cn/post/7218129062744227896#heading-1 "https://juejin.cn/post/7218129062744227896#heading-1")

[【Android Framework】ActivityManagerService(一)](https://juejin.cn/post/7219130999685808188 "https://juejin.cn/post/7219130999685808188")

[【Android Framework】ActivityManagerService(二)](https://juejin.cn/post/7220439797565767741 "https://juejin.cn/post/7220439797565767741")

[【Android Framework】# Service](https://juejin.cn/post/7223687532067471420 "https://juejin.cn/post/7223687532067471420")

[【Android Framework】# Broadcast](https://juejin.cn/post/7224433107775471653 "https://juejin.cn/post/7224433107775471653")

## 介绍

ContentProvider是Android的四大组件之一，虽然他没有Broadcast和Service用的频繁。ContentProvider的作用是不同应用之间数据共享，提供一个统一的接口。例如我们我想让其他应用使用自己的数据 就需要使用ContentProvider。

注册：

```
<provider
    android:multiprocess="false"
    android:name=".StudentContentProvider"
    android:exported="true"
    android:process=":provider"
    android:authorities="com.iehshx.student.provider" />
```

`process`:指定ContentProvider的运行进程。 `authorities`：鉴权字符串列表。多个以;分割。必须至少指定一个。 `multiprocess`:该ContentProvider是否在其他应用中创建。默认是false。如果是true，每个进程都有自己的内容提供者对象，可以减少IPC通信的开销。

ContentProvider实现类。

```
//这里我使用了room
class StudentContentProvider : ContentProvider() {

    override fun onCreate(): Boolean {
        return true
    }


    companion object {
        private const val AUTHORITY = "com.iehshx.student.provider"
        //URI的前缀必选是content://xxxx/table
        val URI_STUDENT: Uri = Uri.parse(
            "content://$AUTHORITY" + "/" + Student.TABLE_NAME
        )
    }


    override fun query(
        uri: Uri,
        projection: Array<out String>?,
        selection: String?,
        selectionArgs: Array<out String>?,
        sortOrder: String?
    ): Cursor? {
        val context = context ?: return null
        var cursor: Cursor =
            StudentDataBase.getInstance(context).getStudentDao()!!.getAll()
        cursor.setNotificationUri(context.contentResolver, uri)
        return cursor;
    }

    override fun getType(uri: Uri): String? {
        return null
    }

    override fun insert(uri: Uri, values: ContentValues?): Uri? {
        val context = context ?: return null
        val id = StudentDataBase.getInstance(context).getStudentDao()
            .insert(Student.fromContentValues(values))
        context.contentResolver.notifyChange(uri, null)
        return ContentUris.withAppendedId(uri, id)
    }

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int {
        val context = context ?: return 0
        val count = StudentDataBase.getInstance(context).getStudentDao()
            .deleteById(ContentUris.parseId(uri))
        context.contentResolver.notifyChange(uri, null)
        return count
    }

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<out String>?
    ): Int {
        val context = context ?: return 0
        val student = Student.fromContentValues(values);
        student.id = ContentUris.parseId(uri);
        val count = StudentDataBase.getInstance(context).getStudentDao()
            .update(student)
        context.contentResolver.notifyChange(uri, null);
        return count;
    }

    override fun applyBatch(operations: ArrayList<ContentProviderOperation>): Array<ContentProviderResult> {
        val context = context ?: return Array<ContentProviderResult>(1) { ContentProviderResult(0) }
        return StudentDataBase.getInstance(context)
            .runInTransaction(Callable<Array<ContentProviderResult>> {
                return@Callable super.applyBatch(
                    operations
                )
            })
    }


    override fun bulkInsert(uri: Uri, values: Array<ContentValues?>): Int {
        val context = context ?: return 0
        val studentDao = StudentDataBase.getInstance(context).getStudentDao()
        val notes = arrayOfNulls<Student>(values.size)
        var i = 0
        while (i < values.size) {
            notes[i] = Student.fromContentValues(values[i])
            i++
        }
        return studentDao.insertAll(notes).size
    }


}

```

使用方:

```
val studentValues = ContentValues()
studentValues.put("name", name)
studentValues.put("age", age)
requireActivity().contentResolver.insert(
    Uri.parse("content://com.iehshx.student.provider/student"),
    studentValues
)
```

注册Observer

```

requireActivity().contentResolver.registerContentObserver(
    Uri.parse("content://com.iehshx.student.provider/student"),true,object:ContentObserver(object:Handler(){}){
        override fun onChange(selfChange: Boolean, uri: Uri?) {
            super.onChange(selfChange, uri)
        }
    })
```

## 正文

1.获取ContentProvider

```
ApplicationContentResolver mContentResolver = new ApplicationContentResolver(this, mainThread);
//获取到ContentResolver
    public ContentResolver getContentResolver() {
        return mContentResolver;
    }
    
    //insert插入数据
    public final @Nullable Uri insert(@RequiresPermission.Write @NonNull Uri url,
                @Nullable ContentValues values) {
        Preconditions.checkNotNull(url, "url");

        try {//这里默认是null
            if (mWrapped != null) return mWrapped.insert(url, values);
        } catch (RemoteException e) {
            return null;
        }
        //获取到ContentProvider
        IContentProvider provider = acquireProvider(url);
        if (provider == null) {
            throw new IllegalArgumentException("Unknown URL " + url);
        }
        try {
            long startTime = SystemClock.uptimeMillis();
            Uri createdRow = provider.insert(mPackageName, url, values);
            long durationMillis = SystemClock.uptimeMillis() - startTime;
            maybeLogUpdateToEventLog(durationMillis, url, "insert", null /* where */);
            return createdRow;
        } catch (RemoteException e) {
            // Arbitrary and not worth documenting, as Activity
            // Manager will kill this process shortly anyway.
            return null;
        } finally {
            releaseProvider(provider);
        }
    }
    
   
   public final IContentProvider acquireProvider(String name) {
        if (name == null) {
            return null;
        }
        //调用到ApplicationContentResolver
        return acquireProvider(mContext, name);
    }
    
    
    //调用到ActivityThread
        protected IContentProvider acquireProvider(Context context, String auth) {
            return mMainThread.acquireProvider(context,
                    ContentProvider.getAuthorityWithoutUserId(auth),
                    resolveUserIdFromAuthority(auth), true);
        }



//ActivityThread.java
    public final IContentProvider acquireProvider(
            Context c, String auth, int userId, boolean stable) {
            //从本地找
        final IContentProvider provider = acquireExistingProvider(c, auth, userId, stable);
        if (provider != null) {//找到就返回
            return provider;
        }

        ContentProviderHolder holder = null;
        try {
            synchronized (getGetProviderLock(auth, userId)) {
            //通过AMS来找ContentProviderHolder
                holder = ActivityManager.getService().getContentProvider(
                        getApplicationThread(), c.getOpPackageName(), auth, userId, stable);
            }
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
        if (holder == null) {
            return null;
        }
        holder = installProvider(c, holder, holder.info,
                true /*noisy*/, holder.noReleaseNeeded, stable);
        return holder.provider;
    }
    
    //从本地找ContentProvider
    public final IContentProvider acquireExistingProvider(
            Context c, String auth, int userId, boolean stable) {
        synchronized (mProviderMap) {
            final ProviderKey key = new ProviderKey(auth, userId);
            //从map找 如果没有就返回null 第一次肯定是没有的
            final ProviderClientRecord pr = mProviderMap.get(key);
            if (pr == null) {
                return null;
            }

            IContentProvider provider = pr.mProvider;
            IBinder jBinder = provider.asBinder();
            if (!jBinder.isBinderAlive()) {
                handleUnstableProviderDiedLocked(jBinder, true);
                return null;
            }
            ProviderRefCount prc = mProviderRefCountMap.get(jBinder);
            if (prc != null) {
                incProviderRefLocked(prc, stable);
            }
            return provider;
        }
    }
    
    
    
    public final ContentProviderHolder getContentProvider(
            IApplicationThread caller, String callingPackage, String name, int userId,
            boolean stable) {
            //隔离进程的判断
        enforceNotIsolatedCaller("getContentProvider");
        if (caller == null) {
            throw new SecurityException(msg);
        }
        //获取到调用者的uid
        final int callingUid = Binder.getCallingUid();
        if (callingPackage != null && mAppOpsService.checkPackage(callingUid, callingPackage)
                != AppOpsManager.MODE_ALLOWED) {//权限校验
            throw new SecurityException("Given calling package " + callingPackage
                    + " does not match caller's uid " + callingUid);
        }
        //调用getContentProviderImpl
        return getContentProviderImpl(caller, name, null, callingUid, callingPackage,
                null, stable, userId);
    }
    
    
    
    private ContentProviderHolder getContentProviderImpl(IApplicationThread caller,
            String name, IBinder token, int callingUid, String callingPackage, String callingTag,
            boolean stable, int userId) {
        ContentProviderRecord cpr;
        ContentProviderConnection conn = null;
        ProviderInfo cpi = null;
        boolean providerRunning = false;

        synchronized(this) {
            long startTime = SystemClock.uptimeMillis();

            ProcessRecord r = null;
            if (caller != null) {
                r = getRecordForAppLocked(caller);
                if (r == null) {//如果调用者进程不存在 抛出异常
                    throw new SecurityException(
                            "Unable to find app for caller " + caller
                          + " (pid=" + Binder.getCallingPid()
                          + ") when getting content provider " + name);
                }
            }

            boolean checkCrossUser = true;

            checkTime(startTime, "getContentProviderImpl: getProviderByName");
            //从mProviderMap中获取 第一次是null的 
            cpr = mProviderMap.getProviderByName(name, userId);
            if (cpr == null && userId != UserHandle.USER_SYSTEM) {
                cpr = mProviderMap.getProviderByName(name, UserHandle.USER_SYSTEM);
                if (cpr != null) {
                    cpi = cpr.info;
                    if (isSingleton(cpi.processName, cpi.applicationInfo,
                            cpi.name, cpi.flags)
                            && isValidSingletonCall(r.uid, cpi.applicationInfo.uid)) {
                        userId = UserHandle.USER_SYSTEM;
                        checkCrossUser = false;
                    } else {
                        cpr = null;
                        cpi = null;
                    }
                }
            }
//providerRunning 默认是false
            if (providerRunning) {//不走这里 第一次是null 所以不会修改providerRunning
                cpi = cpr.info;
                String msg;

                if (r != null && cpr.canRunHere(r)) {
                    if ((msg = checkContentProviderAssociation(r, callingUid, cpi)) != null) {
                        throw new SecurityException("Content provider lookup "
                                + cpr.name.flattenToShortString()
                                + " failed: association not allowed with package " + msg);
                    }
                    checkTime(startTime,
                            "getContentProviderImpl: before checkContentProviderPermission");
                    if ((msg = checkContentProviderPermissionLocked(cpi, r, userId, checkCrossUser))
                            != null) {
                        throw new SecurityException(msg);
                    }
                    checkTime(startTime,
                            "getContentProviderImpl: after checkContentProviderPermission");
                    //ContentProvider 所在的进程已经存在，并且设置了可以在调用者进程创建  创建ContentProviderHolder
                    ContentProviderHolder holder = cpr.newHolder(null);
                    //设置一个空的provider
                    holder.provider = null;
                    return holder;
                }

                try {
                //从pkms中查找 差找不到返回null
                    if (AppGlobals.getPackageManager()
                            .resolveContentProvider(name, 0 /*flags*/, userId) == null) {
                        return null;
                    }
                } catch (RemoteException e) {
                }

               
                final long origId = Binder.clearCallingIdentity();

                checkTime(startTime, "getContentProviderImpl: incProviderCountLocked");
        //provider已经运行的情况下 增加ContentProvider的引用计数
                conn = incProviderCountLocked(r, cpr, token, callingUid, callingPackage, callingTag,
                        stable);
                if (conn != null && (conn.stableCount+conn.unstableCount) == 1) {
                    if (cpr.proc != null && r.setAdj <= ProcessList.PERCEPTIBLE_LOW_APP_ADJ) {
                        checkTime(startTime, "getContentProviderImpl: before updateLruProcess");
                        mProcessList.updateLruProcessLocked(cpr.proc, false, null);
                        checkTime(startTime, "getContentProviderImpl: after updateLruProcess");
                    }
                }

                checkTime(startTime, "getContentProviderImpl: before updateOomAdj");
                final int verifiedAdj = cpr.proc.verifiedAdj;
                boolean success = updateOomAdjLocked(cpr.proc, true,
                        OomAdjuster.OOM_ADJ_REASON_GET_PROVIDER);
                if (success && verifiedAdj != cpr.proc.setAdj && !isProcessAliveLocked(cpr.proc)) {
                    success = false;
                }
                maybeUpdateProviderUsageStatsLocked(r, cpr.info.packageName, name);
                checkTime(startTime, "getContentProviderImpl: after updateOomAdj");
                if (!success) {
                    boolean lastRef = decProviderCountLocked(conn, cpr, token, stable);
                    checkTime(startTime, "getContentProviderImpl: before appDied");
                    appDiedLocked(cpr.proc);
                    checkTime(startTime, "getContentProviderImpl: after appDied");
                    if (!lastRef) {
                        return null;
                    }
                    providerRunning = false;
                    conn = null;
                } else {
                    cpr.proc.verifiedAdj = cpr.proc.setAdj;
                }

                Binder.restoreCallingIdentity(origId);
            }

            if (!providerRunning) {//如果没有运行
                try {
                    //从pkms中查到ContentProvider的信息
                    cpi = AppGlobals.getPackageManager().
                        resolveContentProvider(name,
                            STOCK_PM_FLAGS | PackageManager.GET_URI_PERMISSION_PATTERNS, userId);
                } catch (RemoteException ex) {
                }
                //找不到返回
                if (cpi == null) {
                    return null;
                }
                boolean singleton = isSingleton(cpi.processName, cpi.applicationInfo,
                        cpi.name, cpi.flags)
                        && isValidSingletonCall(r.uid, cpi.applicationInfo.uid);
                if (singleton) {
                    userId = UserHandle.USER_SYSTEM;
                }
                cpi.applicationInfo = getAppInfoForUser(cpi.applicationInfo, userId);
                checkTime(startTime, "getContentProviderImpl: got app info for user");

                String msg;
                if ((msg = checkContentProviderAssociation(r, callingUid, cpi)) != null) {
                    throw new SecurityException("Content provider lookup " + name
                            + " failed: association not allowed with package " + msg);
                }
                checkTime(startTime, "getContentProviderImpl: before checkContentProviderPermission");
                if ((msg = checkContentProviderPermissionLocked(cpi, r, userId, !singleton))
                        != null) {
                    throw new SecurityException(msg);
                }
                checkTime(startTime, "getContentProviderImpl: after checkContentProviderPermission");
                //AMS没有准备好 并且非系统进程
                if (!mProcessesReady
                        && !cpi.processName.equals("system")) {
                    throw new IllegalArgumentException(
                            "Attempt to launch content provider before system ready");
                }
                if (!mSystemProvidersInstalled && cpi.applicationInfo.isSystemApp()
                        && "system".equals(cpi.processName)) {
                    throw new IllegalStateException("Cannot access system provider: '"
                            + cpi.authority + "' before system providers are installed!");
                }

                if (!mUserController.isUserRunning(userId, 0)) {
                    return null;
                }
                //创建ComponentName
                ComponentName comp = new ComponentName(cpi.packageName, cpi.name);
                //根据comp 查找ContentProviderRecord 第一次肯定找不到
                cpr = mProviderMap.getProviderByClass(comp, userId);
                checkTime(startTime, "getContentProviderImpl: after getProviderByClass");
                final boolean firstClass = cpr == null;
                if (firstClass) {//cpt为null 所以这里是true
                    final long ident = Binder.clearCallingIdentity();

                    try {
                    //从pkms获取到ApplicationInfo
                        ApplicationInfo ai =
                            AppGlobals.getPackageManager().
                                getApplicationInfo(
                                        cpi.applicationInfo.packageName,
                                        STOCK_PM_FLAGS, userId);
                        if (ai == null) {
                            return null;
                        }
                        //创建新的ApplicationInfo
                        ai = getAppInfoForUser(ai, userId);
                        //创建ContentProviderRecord对象
                        cpr = new ContentProviderRecord(this, cpi, ai, comp, singleton);
                    } catch (RemoteException ex) {
                    } finally {
                        Binder.restoreCallingIdentity(ident);
                    }
                }
                
                //查看ContentProvider能否安装在调用者进程，如果可以返回一个空的ContentProvider
                if (r != null && cpr.canRunHere(r)) {
                    return cpr.newHolder(null);
                }
               
                final int N = mLaunchingProviders.size();
                int i;
                //看mLaunchingProviders中是否有我们需要的ContentProvider
                for (i = 0; i < N; i++) {
                    if (mLaunchingProviders.get(i) == cpr) {
                        break;
                    }
                }
            
                if (i >= N) {
                    final long origId = Binder.clearCallingIdentity();

                    try {
                        try {
                        //设置PackageStopState = false
                            AppGlobals.getPackageManager().setPackageStoppedState(
                                    cpr.appInfo.packageName, false, userId);
                        } catch (RemoteException e) {
                        } catch (IllegalArgumentException e) {
                        }
                        //获取ContentProvider运行的进程信息 ProcessRecord 
                        ProcessRecord proc = getProcessRecordLocked(
                                cpi.processName, cpr.appInfo.uid, false);
                        if (proc != null && proc.thread != null && !proc.killed) {//如果进程存在
                            if (!proc.pubProviders.containsKey(cpi.name)) {//进程中发布的Providers没有就发布（存入map）
                                proc.pubProviders.put(cpi.name, cpr);
                                try {
                                //到ContentProvider的运行进程中执行scheduleInstallProvider
                                    proc.thread.scheduleInstallProvider(cpi);
                                } catch (RemoteException e) {
                                }
                            }
                        } else {//如果不存在
                        //开启进程
                            proc = startProcessLocked(cpi.processName,
                                    cpr.appInfo, false, 0,
                                    new HostingRecord("content provider",
                                    new ComponentName(cpi.applicationInfo.packageName,
                                            cpi.name)), false, false, false);
                            if (proc == null) {
                                return null;
                            }
                        }
                        //设置launchingApp
                        cpr.launchingApp = proc;
                        //把当前的ContentProviderRecord添加到mLaunchingProviders中
                        mLaunchingProviders.add(cpr);
                    } finally {
                        Binder.restoreCallingIdentity(origId);
                    }
                }

                if (firstClass) {//存入mProviderMap 以comp为key
                    mProviderMap.putProviderByClass(comp, cpr);
                }
                //以name为kay
                mProviderMap.putProviderByName(name, cpr);
                //引用计数+1
                conn = incProviderCountLocked(r, cpr, token, callingUid, callingPackage, callingTag,
                        stable);
                if (conn != null) {
                //设置为等待状态
                    conn.waiting = true;
                }
            }
            grantEphemeralAccessLocked(userId, null /*intent*/,
                    UserHandle.getAppId(cpi.applicationInfo.uid),
                    UserHandle.getAppId(Binder.getCallingUid()));
        }
       
        final long timeout = SystemClock.uptimeMillis() + CONTENT_PROVIDER_WAIT_TIMEOUT;
        boolean timedOut = false;
        synchronized (cpr) {
            while (cpr.provider == null) {
                if (cpr.launchingApp == null) {
                    return null;
                }
                try {
                    final long wait = Math.max(0L, timeout - SystemClock.uptimeMillis());
                    if (conn != null) {
                        conn.waiting = true;
                    }
                   //挂起调用者线程 直到ContentProvider安装完成
                    cpr.wait(wait);
                    if (cpr.provider == null) {
                        timedOut = true;
                        break;
                    }
                } catch (InterruptedException ex) {
                } finally {
                    if (conn != null) {
                        conn.waiting = false;
                    }
                }
            }
        }
        if (timedOut) {//超时
            String callerName = "unknown";
            synchronized (this) {
                final ProcessRecord record = mProcessList.getLRURecordForAppLocked(caller);
                if (record != null) {
                    callerName = record.processName;
                }
            }
            return null;
        }
        //返回ContentProviderHolder
        return cpr.newHolder(conn);
    }

```

由调用者进程调用到`AMS`中，如果`ContentProvider`已经发布了 通过`canRunHere`判断`ContentProvider`是否可以运行在调用者的进程中，如果`允许` 不会把已经发布的ContentProvider返回，而是返回新的ContentProviderHoder 但是会把`provider`设置成`null`。 如果`不允许` 设置OOMAdj 和 更新进程LRU 最终调用cpr.newHolder(conn)。如果`没有发布`还是会检查是否可以在调用者进程来创建，接下来看ContentProvider的进程是否创建，如果`没有创建`调用`startProcessLocked`启动进程。如果已经创建进程 调用ContentProvider进程的`proc.thread.scheduleInstallProvider`。 两种情况都会把ContentProvider添加到`mLaunchingProviders`和`mProviderMap`中。然后等待ContentProvider安装完成 唤醒。

## 2.安装ContentProvider

```

        public void scheduleInstallProvider(ProviderInfo provider) {
            sendMessage(H.INSTALL_PROVIDER, provider);
        }
        
        
        case INSTALL_PROVIDER:  
            handleInstallProvider((ProviderInfo) msg.obj);  
            break;


    public void handleInstallProvider(ProviderInfo info) {
        final StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            installContentProviders(mInitialApplication, Arrays.asList(info));
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }
    
    
    
    private void installContentProviders(
            Context context, List<ProviderInfo> providers) {
        final ArrayList<ContentProviderHolder> results = new ArrayList<>();
        //从本进程中遍历providers 调用installProvider
        for (ProviderInfo cpi : providers) {
            ContentProviderHolder cph = installProvider(context, null, cpi,
                    false /*noisy*/, true /*noReleaseNeeded*/, true /*stable*/);
            if (cph != null) {
                cph.noReleaseNeeded = true;
                results.add(cph);
            }
        }

        try {
        //告诉AMS发布ContentProvider
            ActivityManager.getService().publishContentProviders(
                getApplicationThread(), results);
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    }
    
//ContentProvider进程安装Provider
    private ContentProviderHolder installProvider(Context context,
            ContentProviderHolder holder, ProviderInfo info,
            boolean noisy, boolean noReleaseNeeded, boolean stable) {
        ContentProvider localProvider = null;
        IContentProvider provider;
        if (holder == null || holder.provider == null) {//传递的holder是null
            Context c = null;
            //获取到ApplicationInfo 得到Context
            ApplicationInfo ai = info.applicationInfo;
            if (context.getPackageName().equals(ai.packageName)) {
                c = context;
            } else if (mInitialApplication != null &&
                    mInitialApplication.getPackageName().equals(ai.packageName)) {
                c = mInitialApplication;
            } else {
                try {
                    c = context.createPackageContext(ai.packageName,
                            Context.CONTEXT_INCLUDE_CODE);
                } catch (PackageManager.NameNotFoundException e) {
                }
            }
            if (c == null) {
                return null;
            }

            if (info.splitName != null) {
                try {
                    c = c.createContextForSplit(info.splitName);
                } catch (NameNotFoundException e) {
                    throw new RuntimeException(e);
                }
            }

            try {
            //获取到ClassLoader
                final java.lang.ClassLoader cl = c.getClassLoader();
                //获取到loadedApk
                LoadedApk packageInfo = peekPackageInfo(ai.packageName, true);
                if (packageInfo == null) {
                    packageInfo = getSystemContext().mPackageInfo;
                }
                //反射创建Provider
                localProvider = packageInfo.getAppFactory()
                        .instantiateProvider(cl, info.name);
                //调用getIContentProvider 获取到Transport 也就是ContentProviderNative 其实是一个BinderProxy 后续的通信都是通过他
                provider = localProvider.getIContentProvider();
                if (provider == null) {
                    return null;
                }
                //调用attach 最后会执行onCreate
                localProvider.attachInfo(c, info);
            } catch (java.lang.Exception e) {
                if (!mInstrumentation.onException(null, e)) {
                    throw new RuntimeException(
                            "Unable to get provider " + info.name
                            + ": " + e.toString(), e);
                }
                return null;
            }
        } else {//holder部位null的情况 可以直接使用
            provider = holder.provider;
        }

        ContentProviderHolder retHolder;

        synchronized (mProviderMap) {
            IBinder jBinder = provider.asBinder();
            if (localProvider != null) {//之前反射创建了 不是null
                ComponentName cname = new ComponentName(info.packageName, info.name);
                //获取ProviderClientRecord 第一次是null
                ProviderClientRecord pr = mLocalProvidersByName.get(cname);
                if (pr != null) {
                    provider = pr.mProvider;
                } else {//创建 ContentProviderHolder 设置provider为反射创建的provider
                    holder = new ContentProviderHolder(info);
                    holder.provider = provider;
                    holder.noReleaseNeeded = true;
                    //通过installProviderAuthoritiesLocked创建ProviderClientRecord
                    pr = installProviderAuthoritiesLocked(provider, localProvider, holder);
                    //存入mLocalProviders 和 mLocalProvidersByName
                    mLocalProviders.put(jBinder, pr);
                    mLocalProvidersByName.put(cname, pr);
                }
                //拿到ContentProviderHolder
                retHolder = pr.mHolder;
            } else {//为null 就是之前就有了
                ProviderRefCount prc = mProviderRefCountMap.get(jBinder);
                if (prc != null) {
                    if (!noReleaseNeeded) {
                        incProviderRefLocked(prc, stable);
                        try {
                            ActivityManager.getService().removeContentProvider(
                                    holder.connection, stable);
                        } catch (RemoteException e) {
                            //do nothing content provider object is dead any way
                        }
                    }
                } else {
                //创建ProviderClientRecord
                    ProviderClientRecord client = installProviderAuthoritiesLocked(
                            provider, localProvider, holder);
                    if (noReleaseNeeded) {
                        prc = new ProviderRefCount(holder, client, 1000, 1000);
                    } else {
                        prc = stable
                                ? new ProviderRefCount(holder, client, 1, 0)
                                : new ProviderRefCount(holder, client, 0, 1);
                    }
                    mProviderRefCountMap.put(jBinder, prc);
                }
                retHolder = prc.holder;
            }
        }
        //返回ContentProviderHolder
        return retHolder;
    }


//创建ProviderClientRecord
    private ProviderClientRecord installProviderAuthoritiesLocked(IContentProvider provider,
            ContentProvider localProvider, ContentProviderHolder holder) {
        final String auths[] = holder.info.authority.split(";");
        final int userId = UserHandle.getUserId(holder.info.applicationInfo.uid);

        if (provider != null) {
            for (String auth : auths) {
                switch (auth) {
                    case ContactsContract.AUTHORITY:
                    case CallLog.AUTHORITY:
                    case CallLog.SHADOW_AUTHORITY:
                    case BlockedNumberContract.AUTHORITY:
                    case CalendarContract.AUTHORITY:
                    case Downloads.Impl.AUTHORITY:
                    case "telephony":
                        Binder.allowBlocking(provider.asBinder());
                }
            }
        }

        final ProviderClientRecord pcr = new ProviderClientRecord(
                auths, provider, localProvider, holder);
        for (String auth : auths) {
            final ProviderKey key = new ProviderKey(auth, userId);
            final ProviderClientRecord existing = mProviderMap.get(key);
            if (existing != null) {
            } else {
                mProviderMap.put(key, pcr);
            }
        }
        return pcr;
    }
```

遍历AMS传递过来的`providers`调用`installProvider`反射创建`ContentProvider`调用`attachInfo` 执行`onCreate` 创建`ProviderClientRecord(持有provider和ContentProviderInfo 以及ContentProviderHolder)` 存入`mLocalProviders`和`mLocalProvidersByName` 返回`ContentProviderHolder`.然后调用AMS的publishContentProviders进行发布。

```
    public final void publishContentProviders(IApplicationThread caller,
            List<ContentProviderHolder> providers) {
        if (providers == null) {
            return;
        }
        //判断隔离进程
        enforceNotIsolatedCaller("publishContentProviders");
        synchronized (this) {
        //获取到调用者的ProcessRecord
            final ProcessRecord r = getRecordForAppLocked(caller);
            if (r == null) {
                throw new SecurityException(
                        "Unable to find app for caller " + caller
                      + " (pid=" + Binder.getCallingPid()
                      + ") when publishing content providers");
            }

            final long origId = Binder.clearCallingIdentity();
            //遍历反射创建的providers
            final int N = providers.size();
            for (int i = 0; i < N; i++) {
                ContentProviderHolder src = providers.get(i);
                if (src == null || src.info == null || src.provider == null) {
                    continue;
                }
                //获取到ContentProviderRecord
                ContentProviderRecord dst = r.pubProviders.get(src.info.name);
                if (dst != null) {
                //创建ComponentName
                    ComponentName comp = new ComponentName(dst.info.packageName, dst.info.name);
                    //存入 mProviderMap
                    mProviderMap.putProviderByClass(comp, dst);
                    //获取到authority
                    String names[] = dst.info.authority.split(";");
                    //存入mProviderMap
                    for (int j = 0; j < names.length; j++) {
                        mProviderMap.putProviderByName(names[j], dst);
                    }
                    int launchingCount = mLaunchingProviders.size();
                    int j;
                    boolean wasInLaunchingProviders = false;
                    for (j = 0; j < launchingCount; j++) {
                        //将ContentProvider从mLaunchingProviders中移除
                        if (mLaunchingProviders.get(j) == dst) {
                            mLaunchingProviders.remove(j);
                            wasInLaunchingProviders = true;
                            j--;
                            launchingCount--;
                        }
                    }
                    //移除超时
                    if (wasInLaunchingProviders) {
                        mHandler.removeMessages(CONTENT_PROVIDER_PUBLISH_TIMEOUT_MSG, r);
                    }
                    r.addPackage(dst.info.applicationInfo.packageName,
                            dst.info.applicationInfo.longVersionCode, mProcessStats);
                    synchronized (dst) {
                        dst.provider = src.provider;
                        dst.setProcess(r);
                        //唤醒 唤醒之后返回cpt.newHolder
                        dst.notifyAll();
                    }
                    //更新OOMAdj
                    updateOomAdjLocked(r, true, OomAdjuster.OOM_ADJ_REASON_GET_PROVIDER);
                    maybeUpdateProviderUsageStatsLocked(r, src.info.packageName,
                            src.info.authority);
                }
            }

            Binder.restoreCallingIdentity(origId);
        }
    }
```

存入mProviderMap中 一个是以ComponentName为key 一个是以authority为key(authority可以是多个;分割 但是dst是同一个)。然后唤醒 之前的`getContentProviderImpl`返回ContentProviderHolder。

既然multiprocess设置为true可以不拉起ContentProvider进程，直接在自己进程创建，那么他是怎么在自己进程创建ContentProvider的呢？有一个需要关注的地方：

```
//在ActivityThread创建ContentProvider的Context的时候会走这里
c = context.createPackageContext(ai.packageName, Context.CONTEXT_INCLUDE_CODE);

 public Context createPackageContext(String packageName, int flags)
            throws NameNotFoundException {
        return createPackageContextAsUser(packageName, flags, mUser);
    }
    
    
    public Context createPackageContextAsUser(String packageName, int flags, UserHandle user)
            throws NameNotFoundException {
        if (packageName.equals("system") || packageName.equals("android")) {//这里不走
            return new ContextImpl(this, mMainThread, mPackageInfo, null, mActivityToken, user,
                    flags, null, null);
        }
        //拿到loadedApk 从pkms中查询
        LoadedApk pi = mMainThread.getPackageInfo(packageName, mResources.getCompatibilityInfo(),
                flags | CONTEXT_REGISTER_PACKAGE, user.getIdentifier());
        if (pi != null) {
        //创建ContentProvider进程的ContextImpl
            ContextImpl c = new ContextImpl(this, mMainThread, pi, null, mActivityToken, user,
                    flags, null, null);

            final int displayId = getDisplayId();

            c.setResources(createResources(mActivityToken, pi, null, displayId, null,
                    getDisplayAdjustments(displayId).getCompatibilityInfo()));
            if (c.mResources != null) {
                return c;
            }
        }

        throw new PackageManager.NameNotFoundException(
                "Application package " + packageName + " not found");
    }





    public final LoadedApk getPackageInfo(String packageName, CompatibilityInfo compatInfo,
            int flags, int userId) {
        final boolean differentUser = (UserHandle.myUserId() != userId);
        ApplicationInfo ai;
        try {
        //从pkms中拿到ApplicationInfo
            ai = getPackageManager().getApplicationInfo(packageName,
                    PackageManager.GET_SHARED_LIBRARY_FILES
                            | PackageManager.MATCH_DEBUG_TRIAGED_MISSING,
                    (userId < 0) ? UserHandle.myUserId() : userId);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }

        synchronized (mResourcesManager) {
            WeakReference<LoadedApk> ref;
            if (differentUser) {
                ref = null;
            } else if ((flags & Context.CONTEXT_INCLUDE_CODE) != 0) {
                ref = mPackages.get(packageName);
            } else {
                ref = mResourcePackages.get(packageName);
            }

            LoadedApk packageInfo = ref != null ? ref.get() : null;
            if (ai != null && packageInfo != null) {
                if (!isLoadedApkResourceDirsUpToDate(packageInfo, ai)) {
                    packageInfo.updateApplicationInfo(ai, null);
                }

                if (packageInfo.isSecurityViolation()
                        && (flags&Context.CONTEXT_IGNORE_SECURITY) == 0) {
                    throw new SecurityException(
                            "Requesting code from " + packageName
                            + " to be run in process "
                            + mBoundApplication.processName
                            + "/" + mBoundApplication.appInfo.uid);
                }
                return packageInfo;
            }
        }

        if (ai != null) {
            return getPackageInfo(ai, compatInfo, flags);
        }

        return null;
    }

//这样getClassLoader就获取到了ContentProvider应用的ClassLoader就可以通过反射创建调用了
final java.lang.ClassLoader cl = c.getClassLoader(); //获取到loadedApk LoadedApk packageInfo = peekPackageInfo(ai.packageName, true); if (packageInfo == null) { packageInfo = getSystemContext().mPackageInfo; } //反射创建Provider localProvider = packageInfo.getAppFactory() .instantiateProvider(cl, info.name);
   

```

## 总结

## 1.文字总结

1.获取ContentProvider 调用者进程调用到`AMS`中，如果`ContentProvider`已经发布了 通过`canRunHere`判断`ContentProvider`是否可以运行在调用者的进程中，如果`允许` 不会把已经发布的ContentProvider返回，而是返回新的ContentProviderHoder 但是会把`provider`设置成`null`。 如果`不允许` 设置OOMAdj 和 更新进程LRU 最终调用cpr.newHolder(conn)。如果`没有发布`还是会检查是否可以在调用者进程来创建，接下来看ContentProvider的进程是否创建，如果`没有创建`调用`startProcessLocked`启动进程。如果已经创建进程 调用ContentProvider进程的`proc.thread.scheduleInstallProvider`。 两种情况都会把ContentProvider添加到`mLaunchingProviders`和`mProviderMap`中。然后等待ContentProvider安装完成 唤醒。唤醒之后返回`ContentProviderHolder`。

2.安装ContentProvider 遍历AMS传递过来的`providers`调用`installProvider`反射创建`ContentProvider`调用`attachInfo` 执行`onCreate` 创建`ProviderClientRecord(持有provider和ContentProviderInfo 以及ContentProviderHolder)` 存入`mLocalProviders`和`mLocalProvidersByName` 返回`ContentProviderHolder`.然后调用AMS的publishContentProviders进行发布:存入mProviderMap中 一个是以ComponentName为key 一个是以authority为key(authority可以是多个;分割 但是dst是同一个)。然后唤醒 之前的`getContentProviderImpl`返回ContentProviderHolder， ，通过provider(IContentProvider BinderProxy) 再去进行ipc通信。 当然也可以在本进程创建。

## 2.流程图

![content_provider.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ad9749b21ffb410e9256c267ed1c3752~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

ContentProvider也就讲完了，那我们在WMS见啦~

在线视频：

[www.bilibili.com/video/BV1fh…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1fh411j7s1%2F "https://www.bilibili.com/video/BV1fh411j7s1/")