---
title: launcher
---
## 1\. 概述

[Launcher](https://so.csdn.net/so/search?q=Launcher&spm=1001.2101.3001.7020) 是 Android 系统中负责管理主屏幕、应用图标和用户交互的核心组件。它是用户与设备交互的第一个界面。

Launcher 的启动过程可以分为两个部分：

1.  系统启动时的 Launcher 初始化；
2.  用户点击应用图标时的应用启动流程；

本篇文章我们只看第一种情况，第二种作用应用程序的启动流程，后续文章里再分析。

## 2\. Launcher 启动过程

在分析 SystemServer 启动过程时 [添加链接描述](https://blog.csdn.net/Bonnie_cat/article/details/146256561)，我们知道了startOtherServices(t) 启动其他服务中，会调用 AMS.systemReady() 启动桌面。

### 2.1 SystemServer 启动 HomeActivity

```
public final class SystemServer {
// step1 SystemServer 的入口函数
public static void main(String[] args) {
        new SystemServer().run();
    }
    
    private void run() {
    ...
    startBootstrapServices(t); // step2 启动系统引导服务，如 AMS
        startCoreServices(t); // 启动核心服务
        startOtherServices(t); // step3 启动其他服务
    }
}
 private void startOtherServices(@NonNull TimingsTraceAndSlog t) {

       ...
       // step4 调用 AMS.systemReady() 启动桌面，进入 Launcher 的启动过程
        mActivityManagerService.systemReady(() -> {
            ...
	        //  ActivityManagerService.systemReady()
	         mAtmInternal.startHomeOnAllDisplays(currentUserId, "systemReady");  
          }
           ...
    }
```

```
// frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java
public class ActivityManagerService extends IActivityManager.Stub
        implements Watchdog.Monitor, BatteryStatsImpl.BatteryCallback {
     public ActivityManagerService(Context systemContext, ActivityTaskManagerService atm) {
     ...
     // 获取 ATMS 
     mAtmInternal = LocalServices.getService(ActivityTaskManagerInternal.class);
     ...
     }
...
public void systemReady(final Runnable goingCallback, @NonNull TimingsTraceAndSlog t) {
...
if (bootingSystemUser) {
                t.traceBegin("startHomeOnAllDisplays");
                // step5 调用ActivityTaskManagerService.LocalService.startHomeOnAllDisplays
                mAtmInternal.startHomeOnAllDisplays(currentUserId, "systemReady");
                t.traceEnd();
            }
         ...
}
}
```

SystemServer 进程启动后，会启动 otherService，并通知 AMS 启动 HomeActivity。

这里的 ActivityTaskManagerInternal 是 ActivityTaskManagerService 的一个抽象类，真正的实现是在 ActivityTaskManagerService 的 LocalServices 中，所以 mAtmInternal.startHomeOnAllDisplays() 最终调用是在 ActivityTaskManagerService 的 startHomeOnAllDisplays() 方法。

```
// frameworks/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java
public class ActivityTaskManagerService extends IActivityTaskManager.Stub {
...
@Override
        public boolean startHomeOnAllDisplays(int userId, String reason) {
            synchronized (mGlobalLock) {
            // step6 调用RootWindowContainer.startHomeOnAllDisplays()
                return mRootWindowContainer.startHomeOnAllDisplays(userId, reason);
            }
        }
    ...
}
```

```
// frameworks/base/services/core/java/com/android/server/wm/RootWindowContainer.java
class RootWindowContainer extends WindowContainer<DisplayContent>
        implements DisplayManager.DisplayListener {
        ...
    boolean startHomeOnAllDisplays(int userId, String reason) {
        boolean homeStarted = false;
        for (int i = getChildCount() - 1; i >= 0; i--) {
            final int displayId = getChildAt(i).mDisplayId;
            // step7
            homeStarted |= startHomeOnDisplay(userId, reason, displayId);
        }
        return homeStarted;
    }
    boolean startHomeOnDisplay(int userId, String reason, int displayId) {
    // step8
        return startHomeOnDisplay(userId, reason, displayId, false /* allowInstrumenting */,
                false /* fromHomeKey */);
    }

    boolean startHomeOnDisplay(int userId, String reason, int displayId, boolean allowInstrumenting,
            boolean fromHomeKey) {
        // Fallback to top focused display or default display if the displayId is invalid.
        if (displayId == INVALID_DISPLAY) {
            final ActivityStack stack = getTopDisplayFocusedStack();
            displayId = stack != null ? stack.getDisplayId() : DEFAULT_DISPLAY;
        }

        final DisplayContent display = getDisplayContent(displayId);
        boolean result = false;
        for (int tcNdx = display.getTaskDisplayAreaCount() - 1; tcNdx >= 0; --tcNdx) {
            final TaskDisplayArea taskDisplayArea = display.getTaskDisplayAreaAt(tcNdx);
           // step8
            result |= startHomeOnTaskDisplayArea(userId, reason, taskDisplayArea,
                    allowInstrumenting, fromHomeKey);
        }
        return result;
    }
    
boolean startHomeOnTaskDisplayArea(int userId, String reason, TaskDisplayArea taskDisplayArea,
            boolean allowInstrumenting, boolean fromHomeKey) {
        ...
        Intent homeIntent = null;
        ActivityInfo aInfo = null;
        // 是否是默认的屏幕区域
        if (taskDisplayArea == getDefaultTaskDisplayArea()) {
        // 构建一个category为CATEGORY_HOME的Intent，表明是Home Activity
            homeIntent = mService.getHomeIntent();
            //通过PKMS从系统所用已安装的引用中，找到一个符合HomeItent的Activity
            aInfo = resolveHomeActivity(userId, homeIntent);
        } 
        ...
        // step9 调用 ActivityStartController.startHomeActivity()
        mService.getActivityStartController().startHomeActivity(homeIntent, aInfo, myReason,
                taskDisplayArea);
        return true;
    }
    ...
}
```

获取的 displayId 为 DEFAULT\_DISPLAY。通过 getHomeIntent() 来构建一个 category 为 CATEGORY\_HOME 的 intent，表明是 Home Activity；然后通过 resolveHomeActivity() 从系统所有已安装的应用中找到一个符合 HomeIntent 的 Activity（具体分析可以参考后续文章 PMS 的分析），最终调用 ActivityStartController 的 startHomeActivity() 方法来启动 Activity。

```
// frameworks/base/services/core/java/com/android/server/wm/ActivityStartController.java
public class ActivityStartController {
...
void startHomeActivity(Intent intent, ActivityInfo aInfo, String reason,
            TaskDisplayArea taskDisplayArea) {
        ...
// step10 调用 ActivityStarter.java 的 execute()
        mLastHomeActivityStartResult = obtainStarter(intent, "startHomeActivity: " + reason)
                .setOutActivity(tmpOutRecord)
                .setCallingUid(0)
                .setActivityInfo(aInfo)
                .setActivityOptions(options.toBundle())
                .execute();
        mLastHomeActivityStartRecord = tmpOutRecord[0];
        if (homeStack.mInResumeTopActivity) {
            //如果home activity 处于顶层的resume activity中，则Home Activity 将被初始化，但不会被恢复（以避免递归恢复），
        //并将保持这种状态，直到有东西再次触发它。我们需要进行另一次恢复。
            mSupervisor.scheduleResumeTopActivities();
        }
    }
...
}
```

obtainStarter() 方法返回的是 ActivityStart 对象，它负责 Activity 的启动，调用一系列 setXXX() 方法传入启动所需要的各种参数，最后执行 ActivityStarter 的 execute() 方法。另外如果 HomeActivity 处于顶层的 resume activity 中，则HomeActivity 将被初始化，但不会被恢复，并将保持这种状态，直到有东西再次触发它。我们需要进行另一次恢复。

```
// frameworks/base/services/core/java/com/android/server/wm/ActivityStarter.java
class ActivityStarter { 
...
// step11
int execute() {
       ...
       // step12
       res = executeRequest(mRequest);
       ...
    }
    private int executeRequest(Request request) {
    // 其他逻辑代码
       ...

// 通知系统当前正在发生应用切换（AppSwitch）
    // 例如，释放资源或调整优先级
        mService.onStartActivitySetDidAppSwitch();
 // 启动那些之前被延迟启动的 Activity
    // 这些 Activity 可能因为系统资源不足或其他原因未能及时启动
        mController.doPendingActivityLaunches(false); 
         // 启动当前请求的 Activity
    // 这是核心方法，用于执行 Activity 的启动逻辑
    // step13
        mLastStartActivityResult = startActivityUnchecked(r, sourceRecord, voiceSession,
                request.voiceInteractor, startFlags, true /* doResume */, checkedOptions, inTask,
                restrictedBgActivity, intentGrants);

        if (request.outActivity != null) {
            request.outActivity[0] = mLastStartActivityRecord;
        }

        return mLastStartActivityResult;
    }
    private int startActivityUnchecked(final ActivityRecord r, ActivityRecord sourceRecord,
                IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
                int startFlags, boolean doResume, ActivityOptions options, Task inTask,
                boolean restrictedBgActivity, NeededUriGrants intentGrants) {
   ...
   // step14
            result = startActivityInner(r, sourceRecord, voiceSession, voiceInteractor,
                    startFlags, doResume, options, inTask, restrictedBgActivity, intentGrants);
    
...
        return result;
    }
    int startActivityInner(......) {
        // step15 再次RootWindowContainer.java ->resumeFocusedStacksTopActivities
mRootWindowContainer.resumeFocusedStacksTopActivities( mTargetStack, mStartActivity, mOptions);
}
...
}
```

在 execute() 一路调用到 RootWindowContainer 的 resumeFocusedStacksTopActivities()。通过startActivityUnchecked()来处理启动标记 flag ，要启动的任务栈等，最后恢复布局。

```
// frameworks/base/services/core/java/com/android/server/wm/RootWindowContainer.java
class RootWindowContainer extends WindowContainer<DisplayContent>
        implements DisplayManager.DisplayListener {
        ...
       // step16
boolean resumeFocusedStacksTopActivities() {
        return resumeFocusedStacksTopActivities(null, null, null);
    }
    boolean resumeFocusedStacksTopActivities(
            ActivityStack targetStack, ActivityRecord target, ActivityOptions targetOptions) {
         ...
        boolean result = false;
        //如果是栈顶Activity，启动resumeTopActivityUncheckedLocked()
        if (targetStack != null && (targetStack.isTopStackInDisplayArea()
                || getTopDisplayFocusedStack() == targetStack)) {
                // step17 调用ActivityStack.java 的 resumeTopActivityUncheckedLocked
            result = targetStack.resumeTopActivityUncheckedLocked(target, targetOptions);
        }
...
        return result;
    }
    ...
 }
```

在 resumeFocusedStacksTopActivities() 方法中获取栈顶的 Activity，恢复它。

```
// frameworks/base/services/core/java/com/android/server/wm/ActivityStack.java
class ActivityStack extends Task {
...
boolean resumeTopActivityUncheckedLocked(ActivityRecord prev, ActivityOptions options) {
        ...
        // step18
         result = resumeTopActivityInnerLocked(prev, options);
        ... 
    }
    private boolean resumeTopActivityInnerLocked(ActivityRecord prev, ActivityOptions options) {
    ...
    // step19 调用ActivityStackSupervisor.java 的 startSpecificActivity
    mStackSupervisor.startSpecificActivity(next, true, false);
    ...
    }
    ...
}
```

```
// frameworks/base/services/core/java/com/android/server/wm/ActivityStackSupervisor.java
public class ActivityStackSupervisor implements RecentTasks.Callbacks {
...
public ActivityStackSupervisor(ActivityTaskManagerService service, Looper looper) {
        mService = service;
        mLooper = looper;
        mHandler = new ActivityStackSupervisorHandler(looper);
    }
    ...
void startSpecificActivity(ActivityRecord r, boolean andResume, boolean checkConfig) {
       ...
// 如果进程已经存在且已经创建了线程（即进程已经启动）
        if (wpc != null && wpc.hasThread()) {
            try {
            // 直接启动目标 Activity
                realStartActivityLocked(r, wpc, andResume, checkConfig);
                return;
            } catch (RemoteException e) {
                Slog.w(TAG, "Exception when starting activity "
                        + r.intent.getComponent().flattenToShortString(), e);
            }

            // If a dead object exception was thrown -- fall through to
            // restart the application.
            knownToBeDead = true;
        }
// 通知系统，目标 Activity 的可见性状态未知（例如在锁屏过渡期间启动）
        r.notifyUnknownVisibilityLaunchedForKeyguardTransition();
// 检查目标 Activity 是否是当前栈顶的 Activity
        final boolean isTop = andResume && r.isTopRunningActivity();
// 如果进程不存在，需要为应用启动一个新的进程
    // 异步启动进程，并传递相关参数
    // step20 调用 ATMS 的 startProcessAsync
        mService.startProcessAsync(r, knownToBeDead, isTop, isTop ? "top-activity" : "activity");
    }
    ...
}
```

```
// frameworks/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java
void startProcessAsync(ActivityRecord activity, boolean knownToBeDead, boolean isTop,
            String hostingType) {
        ...
            // step 21 使用消息机制异步启动进程，调用 AMS 的 startProcess
            final Message m = PooledLambda.obtainMessage(ActivityManagerInternal::startProcess,
                    mAmInternal, activity.processName, activity.info.applicationInfo, knownToBeDead,
                    isTop, hostingType, activity.intent.getComponent());
            mH.sendMessage(m);
        ...
    }
```

在 startProcessAsync() 中发布消息以启动进程，以避免在ATM锁保持的情况下调用AMS时可能出现死锁，最终调用到AMS的startProcess()。

```
// frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java
        public void startProcess(String processName, ApplicationInfo info, boolean knownToBeDead,
                boolean isTop, String hostingType, ComponentName hostingName) {
                ...
                // step 22
                    startProcessLocked(processName, info, knownToBeDead, 0 /* intentFlags */,
                            new HostingRecord(hostingType, hostingName, isTop),
                            ZYGOTE_POLICY_FLAG_LATENCY_SENSITIVE, false /* allowWhileBooting */,
                            false /* isolated */, true /* keepIfLarge */);
                }
   ...
        }
        final ProcessRecord startProcessLocked(String processName,
            ApplicationInfo info, boolean knownToBeDead, int intentFlags,
            HostingRecord hostingRecord, int zygotePolicyFlags, boolean allowWhileBooting,
            boolean isolated, boolean keepIfLarge) {
            // step 23
        return mProcessList.startProcessLocked(processName, info, knownToBeDead, intentFlags,
                hostingRecord, zygotePolicyFlags, allowWhileBooting, isolated, 0 /* isolatedUid */,
                keepIfLarge, null /* ABI override */, null /* entryPoint */,
                null /* entryPointArgs */, null /* crashHandler */);
    }
```

```
// frameworks/base/services/core/java/com/android/server/am/ProcessList.java
public final class ProcessList {
...
final ProcessRecord startProcessLocked(String processName, ApplicationInfo info,
            boolean knownToBeDead, int intentFlags, HostingRecord hostingRecord,
            int zygotePolicyFlags, boolean allowWhileBooting, boolean isolated, int isolatedUid,
            boolean keepIfLarge, String abiOverride, String entryPoint, String[] entryPointArgs,
            Runnable crashHandler) {
        // step 24
        final boolean success =
                startProcessLocked(app, hostingRecord, zygotePolicyFlags, abiOverride);
    }
    //经过一系列的重载， 调用此startProcessLocked
    boolean startProcessLocked(HostingRecord hostingRecord, String entryPoint, ProcessRecord app,
            int uid, int[] gids, int runtimeFlags, int zygotePolicyFlags, int mountExternal,
            String seInfo, String requiredAbi, String instructionSet, String invokeWith,
            long startTime) {
            // step 25
         final Process.ProcessStartResult startResult = startProcess(hostingRecord,
                        entryPoint, app,
                        uid, gids, runtimeFlags, zygotePolicyFlags, mountExternal, seInfo,
                        requiredAbi, instructionSet, invokeWith, startTime);
                handleProcessStartedLocked(app, startResult.pid, startResult.usingWrapper,
                        startSeq, false);
    }
    private Process.ProcessStartResult startProcess(HostingRecord hostingRecord, String entryPoint,
            ProcessRecord app, int uid, int[] gids, int runtimeFlags, int zygotePolicyFlags,
            int mountExternal, String seInfo, String requiredAbi, String instructionSet,
            String invokeWith, long startTime) {
         if (hostingRecord.usesWebviewZygote()) {
            } else if (hostingRecord.usesAppZygote()) {
            } else {
                // step 26,启动Launcher进程
                startResult = Process.start(entryPoint,
                        app.processName, uid, uid, gids, runtimeFlags, mountExternal,
                        app.info.targetSdkVersion, seInfo, requiredAbi, instructionSet,
                        app.info.dataDir, invokeWith, app.info.packageName, zygotePolicyFlags,
                        isTopApp, app.mDisabledCompatChanges, pkgDataInfoMap,
                        whitelistedAppDataInfoMap, bindMountAppsData, bindMountAppStorageDirs,
                        new String[]{PROC_START_SEQ_IDENT + app.startSeq});
            }
    }
...
}
```

一路调用Process start()，最终到ZygoteProcess的attemptUsapSendArgsAndGetResult()，用来fork一个新的Launcher的进程。

```
private Process.ProcessStartResult attemptZygoteSendArgsAndGetResult(
            ZygoteState zygoteState, String msgStr) throws ZygoteStartFailedEx {
        try {
        //传入的zygoteState为openZygoteSocketIfNeeded()，里面会通过abi来检查是第一个zygote还是第二个
            final BufferedWriter zygoteWriter = zygoteState.mZygoteOutputWriter;
            final DataInputStream zygoteInputStream = zygoteState.mZygoteInputStream;

            zygoteWriter.write(msgStr); //把应用进程的一些参数写给前面连接的zygote进程，包括前面的processClass ="android.app.ActivityThread"
            zygoteWriter.flush();  // socket，进入Zygote进程，处于阻塞状态

            // Always read the entire result from the input stream to avoid leaving
            // bytes in the stream for future process starts to accidentally stumble
            // upon.
            //从socket中得到zygote创建的应用pid，赋值给 ProcessStartResult的对象
            Process.ProcessStartResult result = new Process.ProcessStartResult();
            result.pid = zygoteInputStream.readInt();  // ¶Ásocket ½á¹û
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

通过 Socket 连接 Zygote 进程，把之前组装的 msg 发给 Zygote，其中 processClass =“[android](https://so.csdn.net/so/search?q=android&spm=1001.2101.3001.7020).app.ActivityThread”，通过Zygote 进程来 Fork出一个新的进程，并执行 "android.app.ActivityThread"的 main() 方法。

### 2.2 Zygote 进行 Launcher 进程的 fork 操作

Zygote 的启动过程我们前面有详细讲到过。SystemServer 的 AMS 服务向启动 Home Activity 发起一个 fork 请求，Zygote 进程通过 Linux 的 fork 函数，孵化出一个新的进程。

由于 Zygote 进程在启动时会创建 Java 虚拟机，因此通过 fork 而创建的 Launcher 程序进程可以在内部获取一个 Java 虚拟机的实例拷贝。fork 采用 copy-on-write 机制，有些类如果不做改变，甚至都不用复制，子进程可以和父进程共享这部分数据，从而省去不少内存的占用。

```
// frameworks/base/core/java/com/android/internal/os/ZygoteInit.java
public class ZygoteInit {
// step1
public static void main(String argv[]) {
 ...
 Runnable caller;
 ...
 if (startSystemServer) {
                //Zygote Fork出的第一个进程 SystmeServer
                Runnable r = forkSystemServer(abiList, zygoteSocketName, zygoteServer);

                // {@code r == null} in the parent (zygote) process, and {@code r != null} in the
                // child (system_server) process.
                if (r != null) {
                    r.run();
                    return;
                }
            }

            Log.i(TAG, "Accepting command socket connections");

            // The select loop returns early in the child process after a fork and
            // loops forever in the zygote.
            //循环等待fork出其他的应用进程，比如Launcher
    //最终通过调用processOneCommand()来进行进程的处理
    // step2
            caller = zygoteServer.runSelectLoop(abiList);
        ...

        // We're in the child process and have exited the select loop. Proceed to execute the
        // command.
        if (caller != null) {
            //执行返回的Runnable对象，进入子进程
            caller.run();
        }
}
}
```

Zygote 先 fork 出 SystemServer 进程，接着进入循环等待，用来接收 Socket 发来的消息，用来 fork 出其他应用进程，比如 Launcher。

```
// frameworks/base/core/java/com/android/internal/os/ZygoteServer.java
class ZygoteServer {
...
Runnable runSelectLoop(String abiList) {
...
// step3
final Runnable command = connection.processOneCommand(this);
...
}
...
}
```

```
// frameworks/base/core/java/com/android/internal/os/ZygoteConnection.java
class ZygoteConnection {
...
Runnable processOneCommand(ZygoteServer zygoteServer) {
int pid = -1;
    ...
    //Fork子进程，得到一个新的pid
    //fork子进程,采用copy on write方式，这里执行一次，会返回两次
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
        // step4
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
...
private Runnable handleChildProc(ZygoteArguments parsedArgs,
            FileDescriptor pipeFd, boolean isZygote) {
            ...
    if (parsedArgs.mInvokeWith != null) {
        ...
        throw new IllegalStateException("WrapperInit.execApplication unexpectedly returned");
    } else {
        if (!isZygote) {
            // App进程将会调用到这里，执行目标类的main()方法
            // step5
            return ZygoteInit.zygoteInit(parsedArgs.mTargetSdkVersion,
                    parsedArgs.mRemainingArgs, null /* classLoader */);
        } else {
            return ZygoteInit.childZygoteInit(parsedArgs.mTargetSdkVersion,
                    parsedArgs.mRemainingArgs, null /* classLoader */);
        }
    }
     }
}
```

通过 forkAndSpecialize() 来 fork 出 Launcher 的子进程，并执行 handleChildProc() ，进入子进程的处理。  
进行子进程的操作，最终获得需要执行的 ActivityThread 的 main()。

```
public class ZygoteInit {
...
public static final Runnable zygoteInit(int targetSdkVersion, long[] disabledCompatChanges,
            String[] argv, ClassLoader classLoader) {
        if (RuntimeInit.DEBUG) {
            Slog.d(RuntimeInit.TAG, "RuntimeInit: Starting application from zygote");
        }

        Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "ZygoteInit");
        RuntimeInit.redirectLogStreams();

        RuntimeInit.commonInit();//初始化运行环境 
        ZygoteInit.nativeZygoteInit();//启动Binder线程池 
        // step6 调用程序入口函数  
        return RuntimeInit.applicationInit(targetSdkVersion, disabledCompatChanges, argv,
                classLoader);
    }
...
}
```

zygoteInit 进行一些环境的初始化、启动Binder进程等操作：

```
// frameworks/base/core/java/com/android/internal/os/RuntimeInit.java
public class RuntimeInit {
...
 protected static Runnable applicationInit(int targetSdkVersion, long[] disabledCompatChanges,
            String[] argv, ClassLoader classLoader) {
        // step7 startClass: 如果AMS通过socket传递过来的是 ActivityThread
        return findStaticMain(args.startClass, args.startArgs, classLoader);
    }
    ...
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

        // step8
        return new MethodAndArgsCaller(m, argv);
    }
    static class MethodAndArgsCaller implements Runnable {
    /** method to call */
    private final Method mMethod;

    /** argument array */
    private final String[] mArgs;

    public MethodAndArgsCaller(Method method, String[] args) {
        mMethod = method;
        mArgs = args;
    }

    // step9 调用ActivityThread的main()
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
    ...
}
```

把之前传来的 “android.app.ActivityThread” 传递给 findStaticMain()，通过反射，拿到 ActivityThread 的 main() 方法，把反射得来的 ActivityThread main() 入口返回给 ZygoteInit 的 main，通过 caller.run() 进行调用。

### 2.3 Launcher 的 onCreate() 操作

从前面两部分分析得知，Zygote fork 出了 Launcher 的进程，并把接下来的 Launcher 启动任务交给了 ActivityThread 来进行，接下来我们就从 ActivityThread main() 来分析 Launcher 的创建过程。

```
// frameworks/base/core/java/android/app/ActivityThread.java
public final class ActivityThread extends ClientTransactionHandler {
...
public static void main(String[] args) {
    // 安装选择性的系统调用拦截
    AndroidOs.install();
  ...
  //主线程处理
    Looper.prepareMainLooper();
  ...

  //之前SystemServer调用attach传入的是true，这里到应用进程传入false就行
  // step1
    ActivityThread thread = new ActivityThread();
    thread.attach(false, startSeq);
  ...
  //一直循环，如果退出，说明程序关闭
    Looper.loop();

    throw new RuntimeException("Main thread loop unexpectedly exited");
}
...
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
      //step2 通过Binder调用AMS的attachApplication方法
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
  //当系统资源配置（如：系统字体）发生变化时，通知系统配置发生变化
  ViewRootImpl.ConfigChangedCallback configChangedCallback
      = (Configuration globalConfig) -> {
    synchronized (mResourcesManager) {
      ...
    }
  };
  ViewRootImpl.addConfigCallback(configChangedCallback);
}
...
}
```

主线程处理， 创建ActivityThread对象，调用attach进行处理，最终进入Looper循环。

```
...
public final void attachApplication(IApplicationThread thread, long startSeq) {
    synchronized (this) {
    //通过Binder获取传入的pid信息
        int callingPid = Binder.getCallingPid();
        final int callingUid = Binder.getCallingUid();
        final long origId = Binder.clearCallingIdentity();
        //step3
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
      //step4 调用ATM的attachApplication()，最终层层调用到ActivityStackSupervisor.java的 realStartActivityLocked()
            didSomething = mAtmInternal.attachApplication(app.getWindowProcessController());
    ...
    }
  ...
    return true;
}
...
```

清除一些无用的记录，最终调用 ActivityStackSupervisor 的 realStartActivityLocked()，进行Activity的启动。

```
...
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
            // step5 重点关注：调用 ClientLifecycleManager.scheduleTransaction()，得到上面addCallback的LaunchActivityItem的execute()方法
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
...
```

真正准备去启动 Activity，通过 clientTransaction.addCallback 把 LaunchActivityItem 的 obtain 作为回调参数加进去，再调用  
ClientLifecycleManager.scheduleTransaction() 得到。  
LaunchActivityItem的execute() 方法进行最终的执行。

```
public void execute(ClientTransaction transaction) {
  ...
     // 执行 callBack，参考上面的调用栈，执行回调方法，
   //最终调用到ActivityThread的handleLaunchActivity()
    executeCallbacks(transaction);
 
     // 执行生命周期状态
    executeLifecycleState(transaction);
    mPendingActions.clear();
}
```

执行之前 realStartActivityLocked() 中的 clientTransaction.addCallback。

```
public Activity handleLaunchActivity(ActivityClientRecord r,
        PendingTransactionActions pendingActions, Intent customIntent) {
  ...
  //初始化WindowManagerGlobal
    WindowManagerGlobal.initialize();
  ...
  //step6 调用performLaunchActivity，来处理Activity
    final Activity a = performLaunchActivity(r, customIntent);
  ..
    return a;
}
```

主要干了两件事，第一件：初始化 WindowManagerGlobal；第二件：调用 performLaunchActivity() 方法

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

获取 ComponentName、Context，反射创建 Activity，设置 Activity 的一些内容，比如主题等； 最终调用callActivityOnCreate() 来执行 Activity的 onCreate() 方法.

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

callActivityOnCreate 先执行 activity onCreate 的预处理，再去调用 Activity的onCreate，最终完成Create创建后的内容处理

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

performCreate()主要调用Activity的onCreate()。看到了我们最熟悉的Activity的onCreate()，Launcher的启动完成，Launcher被真正创建起来。

## 总结

Launcher 的启动分三部分：

1.  SystemServer 完成启动 LauncherActivity 的调用；
2.  Zygote 进行 Launcher 进程的 fork 操作；
3.  进入 ActivityThread 的 main()，完成最终 Launcher 的 onCreate() 操作；