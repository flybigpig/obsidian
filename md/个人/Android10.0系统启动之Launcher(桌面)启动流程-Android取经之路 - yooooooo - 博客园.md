**阅读目录**

-   [1\. 概述](https://www.cnblogs.com/linhaostudy/p/18286051#_label0)
-   [2.核心源码](https://www.cnblogs.com/linhaostudy/p/18286051#_label1)
-   [3.架构](https://www.cnblogs.com/linhaostudy/p/18286051#_label2)
-   [4.源码分析](https://www.cnblogs.com/linhaostudy/p/18286051#_label3)
    -   [4.1 第一阶段SystemServer 启动HomeActivity的调用阶段](https://www.cnblogs.com/linhaostudy/p/18286051#_label3_0)
    -   [4.2 \[RootActivityContainer.java\] startHomeOnDisplay()](https://www.cnblogs.com/linhaostudy/p/18286051#_label3_1)
    -   [4.3 \[ActivityStartController.java \] startHomeActivity()](https://www.cnblogs.com/linhaostudy/p/18286051#_label3_2)
    -   [4.4  第二阶段Zygote fork一个Launcher进程的阶段](https://www.cnblogs.com/linhaostudy/p/18286051#_label3_3)
    -   [4.5 第三个阶段，Launcher在自己的进程中进行onCreate等后面的动作](https://www.cnblogs.com/linhaostudy/p/18286051#_label3_4)
-   [5.总结](https://www.cnblogs.com/linhaostudy/p/18286051#_label4)

**正文**

Launcher的启动经过了三个阶段：

第一个阶段：SystemServer完成启动Launcher Activity的调用

第二个阶段：Zygote()进行Launcher进程的Fork操作

第三个阶段：进入ActivityThread的main()，完成最终Launcher的onCreate操作

## 1\. 概述

上一节我们学习了AMS\\ATM的启动流程，这一节主要来学习Launcher的启动流程。

在Android的中，桌面应用Launcher由Launcher演变到Launcher2，再到现在的Launcher3，Google也做了很多改动。

Launcher不支持桌面小工具动画效果，Launcher2添加了动画效果和3D初步效果支持，从Android 4.4 (KK)开始Launcher默认使用Launcher3，    Launcher3加入了透明状态栏，增加overview模式，可以调整workspace上页面的前后顺序，可以动态管理屏幕数量，widget列表与app list分开显示等功能。

我们主要研究Launcher3的启动过程。

## 2.核心源码

```
/frameworks/base/core/java/com/android/internal/os/ZygoteInit.java
/frameworks/base/core/java/com/android/internal/os/ZygoteServer.java
/frameworks/base/core/java/com/android/internal/os/ZygoteConnection.java
/frameworks/base/core/java/com/android/internal/os/Zygote.java
/frameworks/base/core/java/com/android/internal/os/RuntimeInit.java
/frameworks/base/services/java/com/android/server/SystemServer.java
/frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java
/frameworks/base/services/core/java/com/android/server/am/ProcessList.java
/frameworks/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java
/frameworks/base/services/core/java/com/android/server/wm/ActivityStartController.java
/frameworks/base/services/core/java/com/android/server/wm/ActivityStarter.java
/frameworks/base/services/core/java/com/android/server/wm/ActivityStack.java
/frameworks/base/services/core/java/com/android/server/wm/ActivityStackSupervisor.java
/frameworks/base/services/core/java/com/android/server/wm/RootActivityContainer.java
/frameworks/base/services/core/java/com/android/server/wm/ClientLifecycleManager.java
/frameworks/base/core/java/android/os/Process.java
/frameworks/base/core/java/android/os/ZygoteProcess.java
/frameworks/base/core/java/android/app/ActivityThread.java
/frameworks/base/core/java/android/app/Activity.java
/frameworks/base/core/java/android/app/ActivityManagerInternal.java
/frameworks/base/core/java/android/app/servertransaction/ClientTransaction.java
/frameworks/base/core/java/android/app/servertransaction/ClientTransaction.aidl
/frameworks/base/core/java/android/app/ClientTransactionHandler.java
/frameworks/base/core/java/android/app/servertransaction/TransactionExecutor.java
/frameworks/base/core/java/android/app/servertransaction/LaunchActivityItem.java
/frameworks/base/core/java/android/app/Instrumentation.java
/frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java
```

从上面的代码路径可以看出，Android10.0中 Activity的相关功能被放到了wm的目录中，在Android9.0中是在am目录中，Google 最终的目的是把activity 和window融合，在Android10中只是做了简单的代码路径的变更，正在的功能还要到后面的版本才能慢慢融合。

主要代码作用：

-   Instrumentation：负责调用Activity和Application生命周期。
    
-   ActivityTaskManagerService：负责Activity管理和调度等工作。 ATM是Android10中新增内容
    
-   ActivityManagerService：负责管理四大组件和进程，包括生命周期和状态切换。
    
-   ActivityTaskManagerInternal：是由ActivityTaskManagerService对外提供的一个抽象类，真正的实现是在 ActivityTaskManagerService#LocalService
    
-   ActivityThread：管理应用程序进程中主线程的执行
    
-   ActivityStackSupervisor：负责所有Activity栈的管理
    
-   TransactionExecutor：主要作用是执行ClientTransaction
    
-   ClientLifecycleManager：生命周期的管理调用
    

## 3.架构

**Android启动流程图：**

![](https://img-blog.csdnimg.cn/20191229115243250.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3lpcmFuZmVuZw==,size_16,color_FFFFFF,t_70)

**Launcher启动序列图：**

内容较多，例如Zygote的fork流程，realStartActivityLocked启动Activity的中间过程，都没有列出，下一个章节会单独来讲这部分内容

![](https://img-blog.csdnimg.cn/20191229115305398.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3lpcmFuZmVuZw==,size_16,color_FFFFFF,t_70)

## 4.源码分析

上一节在AMS启动过程中，我们知道了AMS启动完成前，在systemReady()中会去调用startHomeOnAllDisplays()来启动Launcher，本次就从startHomeOnAllDisplays()函数入口，来看看Launcher是如何被启动起来的。

```
[ActivityManagerService.java]
public void systemReady(final Runnable goingCallback, TimingsTraceLog 
traceLog) {
    ...
    //启动Home Activity
    mAtmInternal.startHomeOnAllDisplays(currentUserId, "systemReady");
    ...
}
```

Launcher的启动由三部分启动：

-   SystemServer完成启动Launcher Activity的调用
    
-   Zygote()进行Launcher进程的Fork操作
    
-   进入ActivityThread的main()，完成最终Launcher的onCreate操作
    

接下来我们分别从源码部分来分析这三个启动过程。

### 4.1 第一阶段SystemServer 启动HomeActivity的调用阶段

调用栈：

![](https://img-blog.csdnimg.cn/201912291153587.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3lpcmFuZmVuZw==,size_16,color_FFFFFF,t_70)

**\[ActivityTaskManagerService.java\] startHomeOnAllDisplays()**

说明：ActivityTaskManagerInternal是 ActivityTaskManagerService的一个抽象类，正在的实现是在ActivityTaskManagerService的LocalService，所以mAtmInternal.startHomeOnAllDisplays()最终调用的是ActivityTaskManagerService的startHomeOnAllDisplays()方法

源码：

```
public boolean startHomeOnAllDisplays(int userId, String reason) {
          synchronized (mGlobalLock) {
         //一路调用到 RootActivityContainer 的startHomeOnDisplay()方法，参考[4.2]
        return mRootActivityContainer.startHomeOnAllDisplays(userId, reason);
     }
}
```

### 4.2 \[RootActivityContainer.java\] startHomeOnDisplay()

说明：在\[4.1\]中，获取的displayId为DEFAULT\_DISPLAY， 首先通过getHomeIntent 来构建一个category为CATEGORY\_HOME的Intent，表明是Home Activity；然后通过resolveHomeActivity()从系统所用已安装的引用中，找到一个符合HomeItent的Activity，最终调用startHomeActivity()来启动Activity

源码：

```
boolean startHomeOnDisplay(int userId, String reason, int displayId, boolean allowInstrumenting,
    boolean fromHomeKey) {
    ...
     if (displayId == DEFAULT_DISPLAY) {
        //构建一个category为CATEGORY_HOME的Intent，表明是Home Activity，参考[4.2.1]
        homeIntent = mService.getHomeIntent();
        //通过PKMS从系统所用已安装的引用中，找到一个符合HomeItent的Activity参考[4.2.2]
        aInfo = resolveHomeActivity(userId, homeIntent); 
    } 
    ...
    //启动Home Activity，参考[4.3]
    mService.getActivityStartController().startHomeActivity(homeIntent, aInfo, myReason,
        displayId);
    return true;
}
```

#### 4.2.1 \[ActivityTaskManagerService.java\] getHomeIntent()

说明：构建一个category为CATEGORY\_HOME的Intent，表明是Home Activity。

Intent.CATEGORY\_HOME = "android.intent.category.HOME"

这个category会在Launcher3的 AndroidManifest.xml中配置，表明是Home Acivity

源码：

```
int execute() {
    ...
    if (mRequest.mayWait) {
        return startActivityMayWait(...)
    } else {
        return startActivity(...) //参考[4.3.2]
    }
    ...
}
```

#### 4.2.2 \[RootActivityContainer.java\] resolveHomeActivity()

**说明**: 通过Binder跨进程通知PackageManagerService从系统所用已安装的引用中，找到一个符合HomeItent的Activity

**源码：**

```
ActivityInfo resolveHomeActivity(int userId, Intent homeIntent) {
    final int flags = ActivityManagerService.STOCK_PM_FLAGS;
    final ComponentName comp = homeIntent.getComponent(); //系统正常启动时，component为null
    ActivityInfo aInfo = null;
    ...
        if (comp != null) {
            // Factory test.
            aInfo = AppGlobals.getPackageManager().getActivityInfo(comp, flags, userId);
        } else {
            //系统正常启动时，走该流程
            final String resolvedType =
                    homeIntent.resolveTypeIfNeeded(mService.mContext.getContentResolver());

            //resolveIntent做了两件事：1.通过queryIntentActivities来查找符合HomeIntent需求Activities
            //            2.通过chooseBestActivity找到最符合Intent需求的Activity信息
            final ResolveInfo info = AppGlobals.getPackageManager()
                    .resolveIntent(homeIntent, resolvedType, flags, userId);
            if (info != null) {
                aInfo = info.activityInfo;
            }
        }
    ...
    aInfo = new ActivityInfo(aInfo);
    aInfo.applicationInfo = mService.getAppInfoForUser(aInfo.applicationInfo, userId);
    return aInfo;
}
```

### 4.3 \[ActivityStartController.java \] startHomeActivity()

说明：正在的启动Home Activity入口。obtainStarter() 方法返回的是 ActivityStarter 对象，它负责 Activity 的启动，一系列 setXXX() 方法传入启动所需的各种参数，最后的 execute() 是真正的启动逻辑。另外如果home activity处于顶层的resume activity中，则Home Activity 将被初始化，但不会被恢复。并将保持这种状态，直到有东西再次触发它。我们需要进行另一次恢复。

源码：

```
void startHomeActivity(Intent intent, ActivityInfo aInfo, String reason, int displayId) {
    ....
    //返回一个 ActivityStarter 对象，它负责 Activity 的启动
    //一系列 setXXX() 方法传入启动所需的各种参数，最后的 execute() 是真正的启动逻辑
    //最后执行 ActivityStarter的execute方法
    mLastHomeActivityStartResult = obtainStarter(intent, "startHomeActivity: " + reason)
            .setOutActivity(tmpOutRecord)
            .setCallingUid(0)
            .setActivityInfo(aInfo)
            .setActivityOptions(options.toBundle())
            .execute();  //参考[4.3.1]
    mLastHomeActivityStartRecord = tmpOutRecord[0];
    final ActivityDisplay display =
            mService.mRootActivityContainer.getActivityDisplay(displayId);
    final ActivityStack homeStack = display != null ? display.getHomeStack() : null;

    if (homeStack != null && homeStack.mInResumeTopActivity) {
        //如果home activity 处于顶层的resume activity中，则Home Activity 将被初始化，但不会被恢复（以避免递归恢复），
        //并将保持这种状态，直到有东西再次触发它。我们需要进行另一次恢复。
        mSupervisor.scheduleResumeTopActivities();
    }
}
```

#### 4.3.1 \[ActivityStarter.java\] execute()

**说明**: 在\[4.3\]中obtainStarter没有调用setMayWait的方法，因此mRequest.mayWait为false，走startActivity流程

**源码：**

```
int execute() {
    ...
    if (mRequest.mayWait) {
        return startActivityMayWait(...)
    } else {
         return startActivity(...) //参考[4.3.2]
    }
    ...
}
```

#### 4.3.2 \[ActivityStarter.java\] startActivity()

**说明**: 延时布局，然后通过startActivityUnchecked()来处理启动标记 flag ，要启动的任务栈等，最后恢复布局

**源码：**

```
private int startActivity(final ActivityRecord r, ActivityRecord sourceRecord,
            IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
            int startFlags, boolean doResume, ActivityOptions options, TaskRecord inTask,
            ActivityRecord[] outActivity, boolean restrictedBgActivity) {
    ...
    try {
        //延时布局
        mService.mWindowManager.deferSurfaceLayout();
        //调用 startActivityUnchecked ,一路调用到resumeFocusedStacksTopActivities()，参考[4.3.4]
        result = startActivityUnchecked(r, sourceRecord, voiceSession, voiceInteractor,
                startFlags, doResume, options, inTask, outActivity, restrictedBgActivity);
    } finally {
        //恢复布局
        mService.mWindowManager.continueSurfaceLayout();
    }
    ...
}
```

#### 4.3.3 \[RootActivityContainer.java\]  resumeFocusedStacksTopActivities()

说明：获取栈顶的Activity，恢复它

**源码：**

```
boolean resumeFocusedStacksTopActivities(
        ActivityStack targetStack, ActivityRecord target, ActivityOptions targetOptions) {
    ...
    //如果秒表栈就是栈顶Activity，启动resumeTopActivityUncheckedLocked()
    if (targetStack != null && (targetStack.isTopStackOnDisplay()
        || getTopDisplayFocusedStack() == targetStack)) {
    result = targetStack.resumeTopActivityUncheckedLocked(target, targetOptions);
    ...
    if (!resumedOnDisplay) {
        // 获取  栈顶的 ActivityRecord
        final ActivityStack focusedStack = display.getFocusedStack();
        if (focusedStack != null) {
            //最终调用startSpecificActivityLocked(),参考[4.3.4]
            focusedStack.resumeTopActivityUncheckedLocked(target, targetOptions);
        }
    }
  }
}
```

#### 4.3.4 \[ActivityStackSupervisor.java\] startSpecificActivityLocked()

**说明**: 发布消息以启动进程，以避免在ATM锁保持的情况下调用AMS时可能出现死锁,最终调用到ATM的startProcess()

**源码：**

```
void startSpecificActivityLocked(ActivityRecord r, boolean andResume, boolean checkConfig) {
    ...
    //发布消息以启动进程，以避免在ATM锁保持的情况下调用AMS时可能出现死锁
    //最终调用到AMS的startProcess()，参考[4.3.5]
    final Message msg = PooledLambda.obtainMessage(
            ActivityManagerInternal::startProcess, mService.mAmInternal, r.processName,
            r.info.applicationInfo, knownToBeDead, "activity", r.intent.getComponent());
    mService.mH.sendMessage(msg);
    ...
}
```

#### 4.3.5 \[ActivityManagerService.java\] startProcess()

说明：一路调用Process start()，最终到ZygoteProcess的attemptUsapSendArgsAndGetResult()，用来fork一个新的Launcher的进程

源码：

```
public void startProcess(String processName, ApplicationInfo info,
        boolean knownToBeDead, String hostingType, ComponentName hostingName) {
        ..
        //同步操作，避免死锁
        synchronized (ActivityManagerService.this) {
            //调用startProcessLocked,然后到 Process的start，最终到ZygoteProcess的attemptUsapSendArgsAndGetResult()
            //用来fork一个新的Launcher的进程，参考[4.3.6]
            startProcessLocked(processName, info, knownToBeDead, 0 /* intentFlags */,
                    new HostingRecord(hostingType, hostingName),
                    false /* allowWhileBooting */, false /* isolated */,
                    true /* keepIfLarge */);
        }
        ...
}
```

**调用栈如下:**

![](https://img-blog.csdnimg.cn/20191229120148881.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3lpcmFuZmVuZw==,size_16,color_FFFFFF,t_70)

#### 4.3.6 \[ZygoteProcess.java\]  attemptZygoteSendArgsAndGetResult()

说明：通过Socket连接Zygote进程，把之前组装的msg发给Zygote，其中processClass ="android.app.ActivityThread"，通过Zygote进程来Fork出一个新的进程，并执行 "android.app.ActivityThread"的main方法

源码：

```
private Process.ProcessStartResult attemptZygoteSendArgsAndGetResult(
        ZygoteState zygoteState, String msgStr) throws ZygoteStartFailedEx {
    try {
        //传入的zygoteState为openZygoteSocketIfNeeded()，里面会通过abi来检查是第一个zygote还是第二个
        final BufferedWriter zygoteWriter = zygoteState.mZygoteOutputWriter;
        final DataInputStream zygoteInputStream = zygoteState.mZygoteInputStream;

        zygoteWriter.write(msgStr);  //把应用进程的一些参数写给前面连接的zygote进程，包括前面的processClass ="android.app.ActivityThread"
        zygoteWriter.flush(); //进入Zygote进程，处于阻塞状态， 参考[4.4]

         //从socket中得到zygote创建的应用pid，赋值给 ProcessStartResult的对象
        Process.ProcessStartResult result = new Process.ProcessStartResult();
        result.pid = zygoteInputStream.readInt();
        result.usingWrapper = zygoteInputStream.readBoolean();

        if (result.pid < 0) {
            throw new ZygoteStartFailedEx("fork() failed");
        }

        return result;
    } catch (IOException ex) {
        zygoteState.close();
        Log.e(LOG_TAG, "IO Exception while communicating with Zygote - "
                + ex.toString());
        throw new ZygoteStartFailedEx(ex);
    }
}
```

### 4.4  第二阶段Zygote fork一个Launcher进程的阶段

说明：Zygote的启动过程我们前面有详细讲到过。SystemServer的AMS服务向启动Home Activity发起一个fork请求，Zygote进程通过Linux的fork函数，孵化出一个新的进程。

由于Zygote进程在启动时会创建Java虚拟机，因此通过fork而创建的Launcher程序进程可以在内部获取一个Java虚拟机的实例拷贝。fork采用copy-on-write机制，有些类如果不做改变，甚至都不用复制，子进程可以和父进程共享这部分数据，从而省去不少内存的占用。

fork过程，参考下图：

![](https://img-blog.csdnimg.cn/20191229120247407.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3lpcmFuZmVuZw==,size_16,color_FFFFFF,t_70)

**Zygote的调用栈如下：**

![](https://img-blog.csdnimg.cn/20191229120258467.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3lpcmFuZmVuZw==,size_16,color_FFFFFF,t_70)

#### 4.4.1 \[ZygoteInit.java\] main()

**说明**Zygote先fork出SystemServer进程，接着进入循环等待，用来接收Socket发来的消息，用来fork出其他应用进程，比如Launcher

**源码：**

```
public static void main(String argv[]) {
    ...
    Runnable caller;
    ....
    if (startSystemServer) {
        //Zygote Fork出的第一个进程 SystmeServer
        Runnable r = forkSystemServer(abiList, zygoteSocketName, zygoteServer);

        if (r != null) {
            r.run();
            return;
        }
    }
    ...
    //循环等待fork出其他的应用进程，比如Launcher
    //最终通过调用processOneCommand()来进行进程的处理，参考[4.4.2]
    caller = zygoteServer.runSelectLoop(abiList);
    ...
    if (caller != null) {
        caller.run(); //执行返回的Runnable对象，进入子进程
    }
}
```

#### 4.4.2 \[ZygoteConnection.java\] processOneCommand()

**说明**通过forkAndSpecialize()来fork出Launcher的子进程，并执行handleChildProc，进入子进程的处理

**源码：**

```
Runnable processOneCommand(ZygoteServer zygoteServer) {
    int pid = -1;
    ...
    //Fork子进程，得到一个新的pid
    /fork子进程,采用copy on write方式，这里执行一次，会返回两次
    ///pid=0 表示Zygote  fork子进程成功
    //pid > 0 表示子进程 的真正的PID
    pid = Zygote.forkAndSpecialize(parsedArgs.mUid, parsedArgs.mGid, parsedArgs.mGids,
            parsedArgs.mRuntimeFlags, rlimits, parsedArgs.mMountExternal, parsedArgs.mSeInfo,
            parsedArgs.mNiceName, fdsToClose, fdsToIgnore, parsedArgs.mStartChildZygote,
            parsedArgs.mInstructionSet, parsedArgs.mAppDataDir, parsedArgs.mTargetSdkVersion);
    ...
    if (pid == 0) {
        // in child, fork成功，第一次返回的pid = 0
        ...
        //参考[4.4.3]
        return handleChildProc(parsedArgs, descriptors, childPipeFd,
                parsedArgs.mStartChildZygote);
    } else {
        //in parent
        ...
        childPipeFd = null;
        handleParentProc(pid, descriptors, serverPipeFd);
        return null;
    }
}
```

#### 4.4.3 \[ZygoteConnection.java\] handleChildProc()

说明：进行子进程的操作，最终获得需要执行的ActivityThread的main()

源码：

```
private Runnable handleChildProc(ZygoteArguments parsedArgs, FileDescriptor[] descriptors,
        FileDescriptor pipeFd, boolean isZygote) {
    ...
    if (parsedArgs.mInvokeWith != null) {
        ...
        throw new IllegalStateException("WrapperInit.execApplication unexpectedly returned");
    } else {
        if (!isZygote) {
            // App进程将会调用到这里，执行目标类的main()方法
            return ZygoteInit.zygoteInit(parsedArgs.mTargetSdkVersion,
                    parsedArgs.mRemainingArgs, null /* classLoader */);
        } else {
            return ZygoteInit.childZygoteInit(parsedArgs.mTargetSdkVersion,
                    parsedArgs.mRemainingArgs, null /* classLoader */);
        }
    }
}
```

zygoteInit 进行一些环境的初始化、启动Binder进程等操作：

```
public static final Runnable zygoteInit(int targetSdkVersion, String[] argv,
        ClassLoader classLoader) {
    RuntimeInit.commonInit(); //初始化运行环境 
    ZygoteInit.nativeZygoteInit(); //启动Binder线程池 
     //调用程序入口函数  
    return RuntimeInit.applicationInit(targetSdkVersion, argv, classLoader);
}
```

把之前传来的"android.app.ActivityThread" 传递给findStaticMain：

```
protected static Runnable applicationInit(int targetSdkVersion, String[] argv,
        ClassLoader classLoader) {
    ...
    // startClass: 如果AMS通过socket传递过来的是 ActivityThread
    return findStaticMain(args.startClass, args.startArgs, classLoader);
}
```

把之前传来的"android.app.ActivityThread" 传递给findStaticMain：

```
protected static Runnable applicationInit(int targetSdkVersion, String[] argv,
        ClassLoader classLoader) {
    ...
    // startClass: 如果AMS通过socket传递过来的是 ActivityThread
    return findStaticMain(args.startClass, args.startArgs, classLoader);
}
```

通过反射，拿到ActivityThread的main()方法：

```
protected static Runnable findStaticMain(String className, String[] argv,
        ClassLoader classLoader) {
    Class<?> cl;

    try {
        cl = Class.forName(className, true, classLoader);
    } catch (ClassNotFoundException ex) {
        throw new RuntimeException(
                "Missing class when invoking static main " + className,
                ex);
    }

    Method m;
    try {
        m = cl.getMethod("main", new Class[] { String[].class });
    } catch (NoSuchMethodException ex) {
        throw new RuntimeException(
                "Missing static main on " + className, ex);
    } catch (SecurityException ex) {
        throw new RuntimeException(
                "Problem getting static main on " + className, ex);
    }

    int modifiers = m.getModifiers();
    if (! (Modifier.isStatic(modifiers) && Modifier.isPublic(modifiers))) {
        throw new RuntimeException(
                "Main method is not public and static on " + className);
    }
    return new MethodAndArgsCaller(m, argv);
}
```

把反射得来的ActivityThread main()入口返回给ZygoteInit的main，通过caller.run()进行调用：

```
static class MethodAndArgsCaller implements Runnable {
    /** method to call */
    private final Method mMethod;

    /** argument array */
    private final String[] mArgs;

    public MethodAndArgsCaller(Method method, String[] args) {
        mMethod = method;
        mArgs = args;
    }

    //调用ActivityThread的main()
    public void run() {
        try {
            mMethod.invoke(null, new Object[] { mArgs });
        } catch (IllegalAccessException ex) {
            throw new RuntimeException(ex);
        } catch (InvocationTargetException ex) {
            Throwable cause = ex.getCause();
            if (cause instanceof RuntimeException) {
                throw (RuntimeException) cause;
            } else if (cause instanceof Error) {
                throw (Error) cause;
            }
            throw new RuntimeException(ex);
        }
    }
}
```

### 4.5 第三个阶段，Launcher在自己的进程中进行onCreate等后面的动作

从\[4.4\]可以看到，Zygote fork出了Launcher的进程，并把接下来的Launcher启动任务交给了ActivityThread来进行，接下来我们就从ActivityThread main()来分析Launcher的创建过程。

调用栈如下：

![](https://img-blog.csdnimg.cn/20191229120537586.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3lpcmFuZmVuZw==,size_16,color_FFFFFF,t_70)

#### 4.5.1 \[ActivityThread.java\] main()

**说明**主线程处理， 创建ActivityThread对象，调用attach进行处理，最终进入Looper循环

**源码：**

```
p}ublic static void main(String[] args) {
    // 安装选择性的系统调用拦截
    AndroidOs.install();
  ...
  //主线程处理
    Looper.prepareMainLooper();
  ...

  //之前SystemServer调用attach传入的是true，这里到应用进程传入false就行
    ActivityThread thread = new ActivityThread();
    thread.attach(false, startSeq);
  ...
  //一直循环，如果退出，说明程序关闭
    Looper.loop();

    throw new RuntimeException("Main thread loop unexpectedly exited");
}
```

调用ActivityThread的attach进行处理

```
private void attach(boolean system, long startSeq) {
  sCurrentActivityThread = this;
  mSystemThread = system;
  if (!system) {
    //应用进程启动，走该流程
    ...
    RuntimeInit.setApplicationObject(mAppThread.asBinder());
     //获取AMS的本地代理类
    final IActivityManager mgr = ActivityManager.getService();
    try {
      //通过Binder调用AMS的attachApplication方法，参考[4.5.2]
      mgr.attachApplication(mAppThread, startSeq);
    } catch (RemoteException ex) {
      throw ex.rethrowFromSystemServer();
    }
    ...
  } else {
    //通过system_server启动ActivityThread对象
    ...
  }

  // 为 ViewRootImpl 设置配置更新回调，
  当系统资源配置（如：系统字体）发生变化时，通知系统配置发生变化
  ViewRootImpl.ConfigChangedCallback configChangedCallback
      = (Configuration globalConfig) -> {
    synchronized (mResourcesManager) {
      ...
    }
  };
  ViewRootImpl.addConfigCallback(configChangedCallback);
}
```

#### 4.5.2 \[ActivityManagerService.java\] attachApplication()

说明：清除一些无用的记录，最终调用ActivityStackSupervisor.java的 realStartActivityLocked()，进行Activity的启动

源码：

```
public final void attachApplication(IApplicationThread thread, long startSeq) {
    synchronized (this) {
    //通过Binder获取传入的pid信息
        int callingPid = Binder.getCallingPid();
        final int callingUid = Binder.getCallingUid();
        final long origId = Binder.clearCallingIdentity();
        attachApplicationLocked(thread, callingPid, callingUid, startSeq);
        Binder.restoreCallingIdentity(origId);
    }
}

private final boolean attachApplicationLocked(IApplicationThread thread,
        int pid, int callingUid, long startSeq) {
  ...
    //如果当前的Application记录仍然依附到之前的进程中，则清理掉
    if (app.thread != null) {
        handleAppDiedLocked(app, true, true);
    }·

    //mProcessesReady这个变量在AMS的 systemReady 中被赋值为true，
    //所以这里的normalMode也为true
    boolean normalMode = mProcessesReady || isAllowedWhileBooting(app.info);
  ...
    //上面说到，这里为true，进入StackSupervisor的attachApplication方法
    //去真正启动Activity
    if (normalMode) {
    ...
      //调用ATM的attachApplication()，最终层层调用到ActivityStackSupervisor.java的 realStartActivityLocked()
      //参考[4.5.3]
            didSomething = mAtmInternal.attachApplication(app.getWindowProcessController());
    ...
    }
  ...
    return true;
}
```

#### 4.5.3 \[ActivityStackSupervisor.java\] realStartActivityLocked()

说明：真正准备去启动Activity，通过clientTransaction.addCallback把LaunchActivityItem的obtain作为回调参数加进去，再调用

ClientLifecycleManager.scheduleTransaction()得到

LaunchActivityItem的execute()方法进行最终的执行 参考上面的第三阶段的调用栈流程 调用栈如下：

```
boolean realStartActivityLocked(ActivityRecord r, WindowProcessController proc,
        boolean andResume, boolean checkConfig) throws RemoteException {
     // 直到所有的 onPause() 执行结束才会去启动新的 activity
    if (!mRootActivityContainer.allPausedActivitiesComplete()) {
    ...
        return false;
    }
  try {
            // Create activity launch transaction.
            // 添加 LaunchActivityItem
            final ClientTransaction clientTransaction = ClientTransaction.obtain(
                    proc.getThread(), r.appToken);
      //LaunchActivityItem.obtain(new Intent(r.intent)作为回调参数
            clientTransaction.addCallback(LaunchActivityItem.obtain(new Intent(r.intent),
                    System.identityHashCode(r), r.info,
                    // TODO: Have this take the merged configuration instead of separate global
                    // and override configs.
                    mergedConfiguration.getGlobalConfiguration(),
                    mergedConfiguration.getOverrideConfiguration(), r.compat,
                    r.launchedFromPackage, task.voiceInteractor, proc.getReportedProcState(),
                    r.icicle, r.persistentState, results, newIntents,
                    dc.isNextTransitionForward(), proc.createProfilerInfoIfNeeded(),
                            r.assistToken));
 
      ...
      // 设置生命周期状态
            final ActivityLifecycleItem lifecycleItem;
            if (andResume) {
                lifecycleItem = ResumeActivityItem.obtain(dc.isNextTransitionForward());
            } else {
                lifecycleItem = PauseActivityItem.obtain();
            }
            clientTransaction.setLifecycleStateRequest(lifecycleItem);
 
            // Schedule transaction.
            // 重点关注：调用 ClientLifecycleManager.scheduleTransaction()，得到上面addCallback的LaunchActivityItem的execute()方法
      //参考[4.5.4]
            mService.getLifecycleManager().scheduleTransaction(clientTransaction);
 
        } catch (RemoteException e) {
            if (r.launchFailed) {
                 // 第二次启动失败，finish activity
                stack.requestFinishActivityLocked(r.appToken, Activity.RESULT_CANCELED, null,
                        "2nd-crash", false);
                return false;
            }
            // 第一次失败，重启进程并重试
            r.launchFailed = true;
            proc.removeActivity(r);
            throw e;
        }
    } finally {
        endDeferResume();
    }
  ...
    return true;
}
```

#### 4.5.4 \[TransactionExecutor.java\] execute()

**说明**：执行之前realStartActivityLocked()中的clientTransaction.addCallback

clientTransaction.addCallback

![](https://img-blog.csdnimg.cn/20191229120752710.png)

**源码：**

```
 [TransactionExecutor.java] 
public void execute(ClientTransaction transaction) {
  ...
     // 执行 callBack，参考上面的调用栈，执行回调方法，
   //最终调用到ActivityThread的handleLaunchActivity()参考[4.5.5]
    executeCallbacks(transaction);
 
     // 执行生命周期状态
    executeLifecycleState(transaction);
    mPendingActions.clear();
}
```

#### 4.5.5 \[ActivityThread.java\]  handleLaunchActivity()

**说明**主要干了两件事，第一件：初始化WindowManagerGlobal；第二件：调用performLaunchActivity方法

**源码：**

```
[ActivityThread.java] 
public Activity handleLaunchActivity(ActivityClientRecord r,
        PendingTransactionActions pendingActions, Intent customIntent) {
  ...
  //初始化WindowManagerGlobal
    WindowManagerGlobal.initialize();
  ...
  //调用performLaunchActivity，来处理Activity，参考[4.5.6]
    final Activity a = performLaunchActivity(r, customIntent);
  ..
    return a;
}
```

#### 4.5.6 \[ActivityThread.java\] performLaunchActivity()

说明：获取ComponentName、Context，反射创建Activity，设置Activity的一些内容，比如主题等； 最终调用callActivityOnCreate()来执行Activity的onCreate()方法

源码

```
private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
     // 获取 ComponentName
    ComponentName component = r.intent.getComponent();
  ...
     // 获取 Context
    ContextImpl appContext = createBaseContextForActivity(r);
    Activity activity = null;
    try {
         // 反射创建 Activity
        java.lang.ClassLoader cl = appContext.getClassLoader();
        activity = mInstrumentation.newActivity(
                cl, component.getClassName(), r.intent);
        StrictMode.incrementExpectedActivityCount(activity.getClass());
        r.intent.setExtrasClassLoader(cl);
        r.intent.prepareToEnterProcess();
        if (r.state != null) {
            r.state.setClassLoader(cl);
        }
    } catch (Exception e) {
    ...
    }
 
    try {
        // 获取 Application
        Application app = r.packageInfo.makeApplication(false, mInstrumentation);
        if (activity != null) {
      ...
      //Activity的一些处理
            activity.attach(appContext, this, getInstrumentation(), r.token,
                    r.ident, app, r.intent, r.activityInfo, title, r.parent,
                    r.embeddedID, r.lastNonConfigurationInstances, config,
                    r.referrer, r.voiceInteractor, window, r.configCallback,
                    r.assistToken);
 
            if (customIntent != null) {
                activity.mIntent = customIntent;
            }
      ...
            int theme = r.activityInfo.getThemeResource();
            if (theme != 0) {
              // 设置主题
                activity.setTheme(theme);
            }
 
            activity.mCalled = false;
            // 执行 onCreate()
            if (r.isPersistable()) {
                mInstrumentation.callActivityOnCreate(activity, r.state, r.persistentState);
            } else {
                mInstrumentation.callActivityOnCreate(activity, r.state);
            }
      ...
            r.activity = activity;
        }
    //当前状态为ON_CREATE
        r.setState(ON_CREATE);
    ...
    } catch (SuperNotCalledException e) {
        throw e;
    } catch (Exception e) {
    ...
    }
    return activity;
}
```

callActivityOnCreate先执行activity onCreate的预处理，再去调用Activity的onCreate，最终完成Create创建后的内容处理

```
public void callActivityOnCreate(Activity activity, Bundle icicle,
        PersistableBundle persistentState) {
    prePerformCreate(activity); //activity onCreate的预处理
    activity.performCreate(icicle, persistentState);//执行onCreate()
    postPerformCreate(activity); //activity onCreate创建后的一些信息处理
}

public void callActivityOnCreate(Activity activity, Bundle icicle,
        PersistableBundle persistentState) {
    prePerformCreate(activity); //activity onCreate的预处理
    activity.performCreate(icicle, persistentState);//执行onCreate()
    postPerformCreate(activity); //activity onCreate创建后的一些信息处理
}
```

performCreate()主要调用Activity的onCreate()

```
final void performCreate(Bundle icicle, PersistableBundle persistentState) {
  ...
    if (persistentState != null) {
        onCreate(icicle, persistentState);
    } else {
        onCreate(icicle);
    }
  ...
}
```

至此，看到了我们最熟悉的Activity的onCreate()，Launcher的启动完成，Launcher被真正创建起来。

## 5.总结

看到onCreate()后，进入到我们最熟悉的Activity的入口，Launcher的启动告一段落。整个Android的启动流程，我们也完整的分析完成。

Launcher的启动经过了三个阶段：

第一个阶段：SystemServer完成启动Launcher Activity的调用

第二个阶段：Zygote()进行Launcher进程的Fork操作

第三个阶段：进入ActivityThread的main()，完成最终Launcher的onCreate操作

  

___

```
如果您觉得阅读本文对您有帮助，请点一下“推荐”按钮，您的“推荐”将是我最大的写作动力！
```