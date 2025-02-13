## 概述

该篇基于Android 10的代码。在 [AMS之AMS的启动---Android Framework(Android 10)](https://www.cnblogs.com/fanglongxiang/p/13594986.html) 中已经介绍了，在Android 10中，activity的调度和管理已经从AMS移到了ActivityTaskManagerService中(这里简称ATMS)，这在下面关于应用第一次的启动的过程中也能清楚看到。  
这个详细过程有点复杂，所以可能有不准确的地方欢迎指出交流。

应用的启动过程有点复杂，该部分简单概述下。  
第二部分是跟着代码流程一步一步看的。这里涉及多次跨进程，Binder大致了解下是什么，可以参考 [Binder机制](https://www.cnblogs.com/fanglongxiang/p/13466204.html)。  
第三部分“附上调用栈”，有助于查看源码以及理解, 这个方法很好用的。

## 启动过程总结

1.  Launcher点击应用图标：这个过程是Launcher进程中进行的，去启动应用的第一个activity。
2.  通过binder进入ATMS：在ATMS中，为应用的第一个activity创建了ActivityRecord，找到其ActivityStack，将ActivityRecord插入到所在的TaskRecord的合适位置。最后执行到`ActivityManagerInternal::startProcess`。
3.  进入AMS，请求创建应用进程：这个过程创建了ProcessRecord对象并处理保存了进程所需的各种信息。最后通过Process.start()请求创建应用进程。
4.  Zygote创建应用进程：通过socket与zygote进程通信，fork出应用进程。
5.  应用进程主线程-执行ActivityThread的main()：在应用主线程中，执行ActivityThread的main()。
6.  进入系统进程，绑定应用进程：创建了应用的Application，将应用进程绑定到ATMS中，由它们管理。
7.  回到应用进程：这里主要是创建出应用第一个Activity，并执行了attach()和onCreate()。

注：AMS和ATMS服务都是在系统进程-system server进程中。  
所以，整个过程 进程变化是：Launcher进程(binder)->systemserver进程(socket)->zygote进程(socket)->应用进程(binder)->systemserver进程(binder)->应用进程。

## Activity的启动过程详述

**下面部分代码添加了注释，阅读源码时查看下原本英文注释是很有用的。**  
在最后部分“附上调用栈”，附上了部分调用栈的过程，更有助于查看和理解，这个方法很好用的。

这个根据理解画的下面流程的一个简单图示，供参考：  
![ams_app_launcher](https://git.oschina.net/flx_fdbm/BlogPictures/raw/master/ams/ams_app_launcher.png)

## Launcher点击应用图标

Launcher点击图标，BaseDraggingActivity.java中startActivitySafely()调用了startActivity()。  
这是的activity是Launcher的activity，在Launcher的进程中。

Activity中各种startActivity()调用，最终都是走到的startActivityForResult(@RequiresPermission Intent intent, int requestCode, @Nullable Bundle options)方法的。

```
//Activity.java:
@Override
public void startActivity(Intent intent) {
    this.startActivity(intent, null);
}

@Override
public void startActivity(Intent intent, @Nullable Bundle options) {
    if (options != null) {
        startActivityForResult(intent, -1, options);
    } else {
        // Note we want to go through this call for compatibility with
        // applications that may have overridden the method.
        startActivityForResult(intent, -1);
    }
}

public void startActivityForResult(@RequiresPermission Intent intent, int requestCode) {
    startActivityForResult(intent, requestCode, null);
}
```

所以Launcher点击图标后这里直接从startActivityForResult()开始看，从上面可以看到`requestCode值为-1`。

```
//Activity.java:
public void startActivityForResult(@RequiresPermission Intent intent, int requestCode,
        @Nullable Bundle options) {
    //mParent一般为null
    if (mParent == null) {
        options = transferSpringboardActivityOptions(options);
        //调用mInstrumentation.execStartActivity()
        //mMainThread、mInstrumentation、mToken在attach()被赋值
        Instrumentation.ActivityResult ar =
            mInstrumentation.execStartActivity(
                this, mMainThread.getApplicationThread(), mToken, this,
                intent, requestCode, options);
        ......
    } else {
        //调用mParent.startActivityFromChild()，和上面逻辑类似
    }
}
```

目前还是在Launcher进程，这里mMainThread、mInstrumentation、mToken都是在attach()被赋值。  
Instrumentation监控system(主要是AM，Activity Manager)与application之间的所有交互。  
mToken是IBinder对象，是Binder代理，是Android中IPC重要部分。  
mMainThread是ActivityThread对象，应用的主线程。这里是Launhcer的主线程。  
**那么什么时候被赋值的呢？attach()在哪被调用的？看到最后就知道了^v^**

这里**关键代码 调用了mInstrumentation.execStartActivity()**。

```
//Instrumentation.java
@UnsupportedAppUsage
public ActivityResult execStartActivity(
        Context who, IBinder contextThread, IBinder token, Activity target,
        Intent intent, int requestCode, Bundle options) {
    IApplicationThread whoThread = (IApplicationThread) contextThread;
    ......
    try {
        intent.migrateExtraStreamToClipData();
        intent.prepareToLeaveProcess(who);
        int result = ActivityTaskManager.getService()
            .startActivity(whoThread, who.getBasePackageName(), intent,
                    intent.resolveTypeIfNeeded(who.getContentResolver()),
                    token, target != null ? target.mEmbeddedID : null,
                    requestCode, 0, null, options);
        checkStartActivityResult(result, intent);
    } catch (RemoteException e) {
        throw new RuntimeException("Failure from system", e);
    }
    return null;
}
```

这里主要看`ActivityTaskManager.getService().startActivity()`这个方法。  
`ActivityTaskManager.getService()`这个涉及Binder机制，可以参考 [Binder机制](https://www.cnblogs.com/fanglongxiang/p/13466204.html) 。 这里获取的服务名是`Context.ACTIVITY_TASK_SERVICE(=activity_task)`,最终调用的是`ActivityTaskManagerService.startActivity()`方法。

## 进入ATMS

通过Binder机制，由Launcher进程进入到ATMS。  
ActivityTaskManagerService中的startActivity()

```
//ActivityTaskManagerService.java：
@Override
public final int startActivity(IApplicationThread caller, String callingPackage,
        Intent intent, String resolvedType, IBinder resultTo, String resultWho, int requestCode,
        int startFlags, ProfilerInfo profilerInfo, Bundle bOptions) {
    return startActivityAsUser(caller, callingPackage, intent, resolvedType, resultTo,
            resultWho, requestCode, startFlags, profilerInfo, bOptions,
            UserHandle.getCallingUserId());
}

    @Override
public int startActivityAsUser(IApplicationThread caller, String callingPackage,
        Intent intent, String resolvedType, IBinder resultTo, String resultWho, int requestCode,
        int startFlags, ProfilerInfo profilerInfo, Bundle bOptions, int userId) {
    return startActivityAsUser(caller, callingPackage, intent, resolvedType, resultTo,
            resultWho, requestCode, startFlags, profilerInfo, bOptions, userId,
            true /*validateIncomingUser*/);
}

int startActivityAsUser(IApplicationThread caller, String callingPackage,
        Intent intent, String resolvedType, IBinder resultTo, String resultWho, int requestCode,
        int startFlags, ProfilerInfo profilerInfo, Bundle bOptions, int userId,
        boolean validateIncomingUser) {
    enforceNotIsolatedCaller("startActivityAsUser");
    userId = getActivityStartController().checkTargetUser(userId, validateIncomingUser,
            Binder.getCallingPid(), Binder.getCallingUid(), "startActivityAsUser");
    // TODO: Switch to user app stacks here.
    return getActivityStartController().obtainStarter(intent, "startActivityAsUser")
            .setCaller(caller)
            .setCallingPackage(callingPackage)
            .setResolvedType(resolvedType)
            .setResultTo(resultTo)
            .setResultWho(resultWho)
            .setRequestCode(requestCode)
            .setStartFlags(startFlags)
            .setProfilerInfo(profilerInfo)
            .setActivityOptions(bOptions)
            .setMayWait(userId)
            .execute();

}
```

ATMS中的startActivity()，最终调用了ActivityStarter中的execute()。

```
//ActivityStarter.java：
ActivityStarter setMayWait(int userId) {
    mRequest.mayWait = true;
    mRequest.userId = userId;
    return this;
}

int execute() {
    try {
        // TODO(b/64750076): Look into passing request directly to these methods to allow
        // for transactional diffs and preprocessing.
        if (mRequest.mayWait) {
            return startActivityMayWait(mRequest.caller, mRequest.callingUid,
                    mRequest.callingPackage, mRequest.realCallingPid, mRequest.realCallingUid,
                    mRequest.intent, mRequest.resolvedType,
                    mRequest.voiceSession, mRequest.voiceInteractor, mRequest.resultTo,
                    mRequest.resultWho, mRequest.requestCode, mRequest.startFlags,
                    mRequest.profilerInfo, mRequest.waitResult, mRequest.globalConfig,
                    mRequest.activityOptions, mRequest.ignoreTargetSecurity, mRequest.userId,
                    mRequest.inTask, mRequest.reason,
                    mRequest.allowPendingRemoteAnimationRegistryLookup,
                    mRequest.originatingPendingIntent, mRequest.allowBackgroundActivityStart);
        } else {
            ......
        }
    } finally {
        onExecutionComplete();
    }
}
```

在setMayWait()中，`mRequest.mayWait = true;`，因此走到startActivityMayWait()。

```
//ActivityStarter.java：
private int startActivityMayWait(IApplicationThread caller, int callingUid,
        String callingPackage, int requestRealCallingPid, int requestRealCallingUid,
        Intent intent, String resolvedType, IVoiceInteractionSession voiceSession,
        IVoiceInteractor voiceInteractor, IBinder resultTo, String resultWho, int requestCode,
        int startFlags, ProfilerInfo profilerInfo, WaitResult outResult,
        Configuration globalConfig, SafeActivityOptions options, boolean ignoreTargetSecurity,
        int userId, TaskRecord inTask, String reason,
        boolean allowPendingRemoteAnimationRegistryLookup,
        PendingIntentRecord originatingPendingIntent, boolean allowBackgroundActivityStart) {
    ......
    ActivityInfo aInfo = mSupervisor.resolveActivity(intent, rInfo, startFlags, profilerInfo);
    synchronized (mService.mGlobalLock) {
        final ActivityStack stack = mRootActivityContainer.getTopDisplayFocusedStack();
        ......
        final ActivityRecord[] outRecord = new ActivityRecord[1];
        int res = startActivity(caller, intent, ephemeralIntent, resolvedType, aInfo, rInfo,
                voiceSession, voiceInteractor, resultTo, resultWho, requestCode, callingPid,
                callingUid, callingPackage, realCallingPid, realCallingUid, startFlags, options,
                ignoreTargetSecurity, componentSpecified, outRecord, inTask, reason,
                allowPendingRemoteAnimationRegistryLookup, originatingPendingIntent,
                allowBackgroundActivityStart);
        ......
        mSupervisor.getActivityMetricsLogger().notifyActivityLaunched(res, outRecord[0]);
        if (outResult != null) {
            outResult.result = res;
            final ActivityRecord r = outRecord[0];
            ......
        }
        return res;
    }
}

//startActivity 1
private int startActivity(IApplicationThread caller, Intent intent, Intent ephemeralIntent,
        String resolvedType, ActivityInfo aInfo, ResolveInfo rInfo,
        IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
        IBinder resultTo, String resultWho, int requestCode, int callingPid, int callingUid,
        String callingPackage, int realCallingPid, int realCallingUid, int startFlags,
        SafeActivityOptions options, boolean ignoreTargetSecurity, boolean componentSpecified,
        ActivityRecord[] outActivity, TaskRecord inTask, String reason,
        boolean allowPendingRemoteAnimationRegistryLookup,
        PendingIntentRecord originatingPendingIntent, boolean allowBackgroundActivityStart) {

    if (TextUtils.isEmpty(reason)) {
        throw new IllegalArgumentException("Need to specify a reason.");
    }
    mLastStartReason = reason;
    mLastStartActivityTimeMs = System.currentTimeMillis();
    mLastStartActivityRecord[0] = null;

    mLastStartActivityResult = startActivity(caller, intent, ephemeralIntent, resolvedType,
            aInfo, rInfo, voiceSession, voiceInteractor, resultTo, resultWho, requestCode,
            callingPid, callingUid, callingPackage, realCallingPid, realCallingUid, startFlags,
            options, ignoreTargetSecurity, componentSpecified, mLastStartActivityRecord,
            inTask, allowPendingRemoteAnimationRegistryLookup, originatingPendingIntent,
            allowBackgroundActivityStart);

    if (outActivity != null) {
        // mLastStartActivityRecord[0] is set in the call to startActivity above.
        outActivity[0] = mLastStartActivityRecord[0];
    }

    return getExternalResult(mLastStartActivityResult);
}

//startActivity 2
private int startActivity(IApplicationThread caller, Intent intent, Intent ephemeralIntent,
        String resolvedType, ActivityInfo aInfo, ResolveInfo rInfo,
        IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
        IBinder resultTo, String resultWho, int requestCode, int callingPid, int callingUid,
        String callingPackage, int realCallingPid, int realCallingUid, int startFlags,
        SafeActivityOptions options,
        boolean ignoreTargetSecurity, boolean componentSpecified, ActivityRecord[] outActivity,
        TaskRecord inTask, boolean allowPendingRemoteAnimationRegistryLookup,
        PendingIntentRecord originatingPendingIntent, boolean allowBackgroundActivityStart) {
    .......
    ActivityRecord r = new ActivityRecord(mService, callerApp, callingPid, callingUid,
            callingPackage, intent, resolvedType, aInfo, mService.getGlobalConfiguration(),
            resultRecord, resultWho, requestCode, componentSpecified, voiceSession != null,
            mSupervisor, checkedOptions, sourceRecord);
    if (outActivity != null) {
        outActivity[0] = r;
    }
    ......
    final ActivityStack stack = mRootActivityContainer.getTopDisplayFocusedStack();
    ......
    final int res = startActivity(r, sourceRecord, voiceSession, voiceInteractor, startFlags,
            true /* doResume */, checkedOptions, inTask, outActivity, restrictedBgActivity);
    mSupervisor.getActivityMetricsLogger().notifyActivityLaunched(res, outActivity[0]);
    return res;
}
```

这里主要的代码关注：`startActivityMayWait()->startActivity()->startActivity()->startActivity()`。  
这里3个startActivity()的同名方法，参数是不一样的。参数很多得注意点看。 这几个方法内容都很多，这里主要注意几个地方：

-   ActivityRecord在对应一个activity，是activity的信息记录管理的类。这在 [AMS之AMS的启动](https://www.cnblogs.com/fanglongxiang/p/13594986.html) 都是提到过的。
-   startActivityMayWait()方法中new的ActivityRecord对象 outRecord\[0\]，在第二个startActivity()中被赋值，指向的创建的ActivityRecord对象r。

**\-----这里在第二个startActivity()中创建了应用第一个activity的ActivityRecord对象**

第二个startActivity()同样调用了startActivity()，也是ActivityStarter最后一个同名startActivity方法，这里标记为第三个startActivity()。

```
//ActivityStarter.java：
//第三个：startActivity 3
private int startActivity(final ActivityRecord r, ActivityRecord sourceRecord,
            IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
            int startFlags, boolean doResume, ActivityOptions options, TaskRecord inTask,
            ActivityRecord[] outActivity, boolean restrictedBgActivity) {
    int result = START_CANCELED;
    final ActivityStack startedActivityStack;
    try {
        mService.mWindowManager.deferSurfaceLayout();
        result = startActivityUnchecked(r, sourceRecord, voiceSession, voiceInteractor,
                startFlags, doResume, options, inTask, outActivity, restrictedBgActivity);
    } finally {
        final ActivityStack currentStack = r.getActivityStack();
        startedActivityStack = currentStack != null ? currentStack : mTargetStack;
        ......      
    }
    ......
    return result;
}
```

第三个startActivity()中获取Activity所在的ActivityStack - startedActivityStack。

这里看下startActivityUnchecked()。

```
//ActivityStarter.java：
// Note: This method should only be called from {@link startActivity}.
private int startActivityUnchecked(final ActivityRecord r, ActivityRecord sourceRecord,
        IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
        int startFlags, boolean doResume, ActivityOptions options, TaskRecord inTask,
        ActivityRecord[] outActivity, boolean restrictedBgActivity) {
    ......
    mTargetStack.startActivityLocked(mStartActivity, topFocused, newTask, mKeepCurTransition,
            mOptions);
    if (mDoResume) {
        final ActivityRecord topTaskActivity =
                mStartActivity.getTaskRecord().topRunningActivityLocked();
        if (!mTargetStack.isFocusable()
                || (topTaskActivity != null && topTaskActivity.mTaskOverlay
                && mStartActivity != topTaskActivity)) {
            // If the activity is not focusable, we can't resume it, but still would like to
            // make sure it becomes visible as it starts (this will also trigger entry
            // animation). An example of this are PIP activities.
            // Also, we don't want to resume activities in a task that currently has an overlay
            // as the starting activity just needs to be in the visible paused state until the
            // over is removed.
            mTargetStack.ensureActivitiesVisibleLocked(mStartActivity, 0, !PRESERVE_WINDOWS);
            // Go ahead and tell window manager to execute app transition for this activity
            // since the app transition will not be triggered through the resume channel.
            mTargetStack.getDisplay().mDisplayContent.executeAppTransition();
        } else {
            // If the target stack was not previously focusable (previous top running activity
            // on that stack was not visible) then any prior calls to move the stack to the
            // will not update the focused stack.  If starting the new activity now allows the
            // task stack to be focusable, then ensure that we now update the focused stack
            // accordingly.
            if (mTargetStack.isFocusable()
                    && !mRootActivityContainer.isTopDisplayFocusedStack(mTargetStack)) {
                mTargetStack.moveToFront("startActivityUnchecked");
            }
            mRootActivityContainer.resumeFocusedStacksTopActivities(
                    mTargetStack, mStartActivity, mOptions);
        }
    } else if (mStartActivity != null) {
        mSupervisor.mRecentTasks.add(mStartActivity.getTaskRecord());
    }
    mRootActivityContainer.updateUserStack(mStartActivity.mUserId, mTargetStack);

    mSupervisor.handleNonResizableTaskIfNeeded(mStartActivity.getTaskRecord(),
            preferredWindowingMode, mPreferredDisplayId, mTargetStack);

    return START_SUCCESS;
}
```

-   startActivityUnchecked()找到目标ActivityStack - mTargetStack，然后调用了mTargetStack.startActivityLocked()找到所在TaskRecord，并将activity插入到合适位置。 这样activity对应ActivityRecord就加入到对应ActivityStack中的对应的TaskRecord中了。
-   `mRootActivityContainer.resumeFocusedStacksTopActivities();`获取栈顶activity并恢复，即将设置成resume状态。

这里看下这两个方法  
**1\. startActivityLocked()**

```
//ActivityStack.java:
void startActivityLocked(ActivityRecord r, ActivityRecord focusedTopActivity,
        boolean newTask, boolean keepCurTransition, ActivityOptions options) {
    TaskRecord rTask = r.getTaskRecord();
    final int taskId = rTask.taskId;
    final boolean allowMoveToFront = options == null || !options.getAvoidMoveToFront();
    // mLaunchTaskBehind tasks get placed at the back of the task stack.
    if (!r.mLaunchTaskBehind && allowMoveToFront
            && (taskForIdLocked(taskId) == null || newTask)) {
        // Last activity in task had been removed or ActivityManagerService is reusing task.
        // Insert or replace.
        // Might not even be in.
        insertTaskAtTop(rTask, r);
    }
    TaskRecord task = null;
    ......
    task = activityTask;
    ......
    task.setFrontOfTask();

    // The transition animation and starting window are not needed if {@code allowMoveToFront}
    // is false, because the activity won't be visible.
    if ((!isHomeOrRecentsStack() || numActivities() > 0) && allowMoveToFront) {
        ......
    } else {
        // If this is the first activity, don't do any fancy animations,
        // because there is nothing for it to animate on top of.
        ActivityOptions.abort(options);
    }
}
```

这里找到activity的TaskRecord，并将activity对应的ActivityRecord插入到TaskRecord的合适位置。然后进行过渡动画相关判断处理。

**\-----到此，应用第一个activity的ActivityRecord已创建，并找到其ActivityStack。最后在ActivityStack中将ActivityRecord插入到所在的TaskRecord的合适位置。**  
参考 [AMS之AMS的启动](https://www.cnblogs.com/fanglongxiang/p/13594986.html) 最后面那个图理解下，ActivityStack、ActivityStackSupervisor、TaskRecord、ActivityRecord、ProcessRecord之间的关系。

**2.resumeFocusedStacksTopActivities()**

```
/**
 * Root node for activity containers.
 * TODO: This class is mostly temporary to separate things out of ActivityStackSupervisor.java. The
 * intention is to have this merged with RootWindowContainer.java as part of unifying the hierarchy.
 */
//RootActivityContainer.java：
boolean resumeFocusedStacksTopActivities(
        ActivityStack targetStack, ActivityRecord target, ActivityOptions targetOptions) {

    if (!mStackSupervisor.readyToResume()) {
        return false;
    }

    boolean result = false;
    if (targetStack != null && (targetStack.isTopStackOnDisplay()
            || getTopDisplayFocusedStack() == targetStack)) {
        result = targetStack.resumeTopActivityUncheckedLocked(target, targetOptions);
    }
    ......
    return result;
}
```

这里看`targetStack.resumeTopActivityUncheckedLocked()`。

```
//ActivityStack.java:
@GuardedBy("mService")
boolean resumeTopActivityUncheckedLocked(ActivityRecord prev, ActivityOptions options) {
    if (mInResumeTopActivity) {
        // Don't even start recursing.
        return false;
    }

    boolean result = false;
    try {
        // Protect against recursion.
        mInResumeTopActivity = true;
        result = resumeTopActivityInnerLocked(prev, options);

        // When resuming the top activity, it may be necessary to pause the top activity (for
        // example, returning to the lock screen. We suppress the normal pause logic in
        // {@link #resumeTopActivityUncheckedLocked}, since the top activity is resumed at the
        // end. We call the {@link ActivityStackSupervisor#checkReadyForSleepLocked} again here
        // to ensure any necessary pause logic occurs. In the case where the Activity will be
        // shown regardless of the lock screen, the call to
        // {@link ActivityStackSupervisor#checkReadyForSleepLocked} is skipped.
        final ActivityRecord next = topRunningActivityLocked(true /* focusableOnly */);
        if (next == null || !next.canTurnScreenOn()) {
            checkReadyForSleep();
        }
    } finally {
        mInResumeTopActivity = false;
    }

    return result;
}

//ActivityStack.java:
@GuardedBy("mService")
private boolean resumeTopActivityInnerLocked(ActivityRecord prev, ActivityOptions options) {
    // Find the next top-most activity to resume in this stack that is not finishing and is
    // focusable. If it is not focusable, we will fall into the case below to resume the
    // top activity in the next focusable task.
    ActivityRecord next = topRunningActivityLocked(true /* focusableOnly */);
    ......
    boolean pausing = getDisplay().pauseBackStacks(userLeaving, next, false);
    if (mResumedActivity != null) {
        pausing |= startPausingLocked(userLeaving, false, next, false);
    }
    ......
    if (next.attachedToProcess()) {
        ......
    } else {
        ......
        mStackSupervisor.startSpecificActivityLocked(next, true, true);
    }

    return true;
}
```

由于activity所在应用的进程还未生成，此时next.attachedToProcess()明显是false的。直接看`mStackSupervisor.startSpecificActivityLocked(next, true, true);`

```
//ActivityStackSupervisor.java：
void startSpecificActivityLocked(ActivityRecord r, boolean andResume, boolean checkConfig) {
    // Is this activity's application already running?
    final WindowProcessController wpc =
            mService.getProcessController(r.processName, r.info.applicationInfo.uid);
    if (wpc != null && wpc.hasThread()) {
        try {
            realStartActivityLocked(r, wpc, andResume, checkConfig);
            return;
        } catch (RemoteException e) {
        }
    }
    ......
    try {
        if (Trace.isTagEnabled(TRACE_TAG_ACTIVITY_MANAGER)) {
            Trace.traceBegin(TRACE_TAG_ACTIVITY_MANAGER, "dispatchingStartProcess:"
                    + r.processName);
        }
        // Post message to start process to avoid possible deadlock of calling into AMS with the
        // ATMS lock held.
        final Message msg = PooledLambda.obtainMessage(
                ActivityManagerInternal::startProcess, mService.mAmInternal, r.processName,
                r.info.applicationInfo, knownToBeDead, "activity", r.intent.getComponent());
        mService.mH.sendMessage(msg);
    } finally {
        Trace.traceEnd(TRACE_TAG_ACTIVITY_MANAGER);
    }
}
```

如果已有进程会走realStartActivityLocked()。 这里走startProcess()，这里的mService即ActivityTaskManagerService(ATMS)，mService.mH是ATMS的内部类 自定义的Handler。  
关于这个Handler：它是ATMS initialize()时创建的，它的Looper是AMS中`mActivityTaskManager.initialize(mIntentFirewall, mPendingIntentController, DisplayThread.get().getLooper());`传入的，即DisplayThread.get().getLooper()。这个DisplayThread比较特别，可以看下注释。  
Hanler相关可以参考下[消息机制](https://www.cnblogs.com/fanglongxiang/p/13285896.html)。  
接着直接查看ActivityManagerInternal::startProcess。

```
/**
 * Shared singleton foreground thread for the system.  This is a thread for
 * operations that affect what's on the display, which needs to have a minimum
 * of latency.  This thread should pretty much only be used by the WindowManager,
 * DisplayManager, and InputManager to perform quick operations in real time.
 */
public final class DisplayThread extends ServiceThread {
    private DisplayThread() {
        // DisplayThread runs important stuff, but these are not as important as things running in
        // AnimationThread. Thus, set the priority to one lower.
        super("android.display", Process.THREAD_PRIORITY_DISPLAY + 1, false /*allowIo*/);
    }
```

`ActivityManagerInternal::startProcess`，ActivityManagerInternal是抽象类，实现是AMS中的内部类LocalService。

## 进入AMS，请求创建应用进程

这里直接看startProcess。

```
//ActivityManagerService.java:
@Override
public void startProcess(String processName, ApplicationInfo info,
        boolean knownToBeDead, String hostingType, ComponentName hostingName) {
    try {
        synchronized (ActivityManagerService.this) {
            startProcessLocked(processName, info, knownToBeDead, 0 /* intentFlags */,
                    new HostingRecord(hostingType, hostingName),
                    false /* allowWhileBooting */, false /* isolated */,
                    true /* keepIfLarge */);
        }
    } finally {
        ......
    }
}

@GuardedBy("this")
final ProcessRecord startProcessLocked(String processName,
        ApplicationInfo info, boolean knownToBeDead, int intentFlags,
        HostingRecord hostingRecord, boolean allowWhileBooting,
        boolean isolated, boolean keepIfLarge) {
    return mProcessList.startProcessLocked(processName, info, knownToBeDead, intentFlags,
            hostingRecord, allowWhileBooting, isolated, 0 /* isolatedUid */, keepIfLarge,
            null /* ABI override */, null /* entryPoint */, null /* entryPointArgs */,
            null /* crashHandler */);
}
```

很清晰，AMS的startProcess()调用了startProcessLocked()，而startProcessLocked()又调用了ProcessList的startProcessLocked()。

```
//ProcessList.java:
@GuardedBy("mService")
final ProcessRecord startProcessLocked(String processName, ApplicationInfo info,
        boolean knownToBeDead, int intentFlags, HostingRecord hostingRecord,
        boolean allowWhileBooting, boolean isolated, int isolatedUid, boolean keepIfLarge,
        String abiOverride, String entryPoint, String[] entryPointArgs, Runnable crashHandler) {
    ProcessRecord app;
    if (!isolated) {
        app = getProcessRecordLocked(processName, info.uid, keepIfLarge);
        ......
    } else {
        ......
    }
    ......
    final boolean success = startProcessLocked(app, hostingRecord, abiOverride);
    return success ? app : null;
}

@GuardedBy("mService")
final boolean startProcessLocked(ProcessRecord app, HostingRecord hostingRecord,
        String abiOverride) {
    return startProcessLocked(app, hostingRecord,
            false /* disableHiddenApiChecks */, false /* mountExtStorageFull */, abiOverride);
}

@GuardedBy("mService")
boolean startProcessLocked(ProcessRecord app, HostingRecord hostingRecord,
        boolean disableHiddenApiChecks, boolean mountExtStorageFull,
        String abiOverride) {
    ......
    try {
        ......
        if ("1".equals(SystemProperties.get("debug.checkjni"))) {
            runtimeFlags |= Zygote.DEBUG_ENABLE_CHECKJNI;
        }
        ......
        final String seInfo = app.info.seInfo
                + (TextUtils.isEmpty(app.info.seInfoUser) ? "" : app.info.seInfoUser);
        // Start the process.  It will either succeed and return a result containing
        // the PID of the new process, or else throw a RuntimeException.
        final String entryPoint = "android.app.ActivityThread";
        return startProcessLocked(hostingRecord, entryPoint, app, uid, gids,
                runtimeFlags, mountExternal, seInfo, requiredAbi, instructionSet, invokeWith,
                startTime);
    } catch (RuntimeException e) {
        ......
    }
}

@GuardedBy("mService")
boolean startProcessLocked(HostingRecord hostingRecord,
        String entryPoint,
        ProcessRecord app, int uid, int[] gids, int runtimeFlags, int mountExternal,
        String seInfo, String requiredAbi, String instructionSet, String invokeWith,
        long startTime) {
    app.pendingStart = true;
    app.killedByAm = false;
    app.removed = false;
    app.killed = false;
    ......
    if (mService.mConstants.FLAG_PROCESS_START_ASYNC) {
        mService.mProcStartHandler.post(() -> {
            try {
                final Process.ProcessStartResult startResult = startProcess(app.hostingRecord,
                        entryPoint, app, app.startUid, gids, runtimeFlags, mountExternal,
                        app.seInfo, requiredAbi, instructionSet, invokeWith, app.startTime);
                synchronized (mService) {
                    handleProcessStartedLocked(app, startResult, startSeq);
                }
            } catch (RuntimeException e) {
                ......
            }
        });
        return true;
    } else {
        try {
            final Process.ProcessStartResult startResult = startProcess(hostingRecord,
                    entryPoint, app,
                    uid, gids, runtimeFlags, mountExternal, seInfo, requiredAbi, instructionSet,
                    invokeWith, startTime);
            handleProcessStartedLocked(app, startResult.pid, startResult.usingWrapper,
                    startSeq, false);
        } catch (RuntimeException e) {
            ......
        }
        return app.pid > 0;
    }
}
```

从上面可以看到，经过了多次同名方法 startProcessLocked() 调用，在调用过程创建了ProcessRecord对象并处理保存了进程所需的各种信息。  
最终调用的是startProcess()。

```
//ProcessList.java:
private Process.ProcessStartResult startProcess(HostingRecord hostingRecord, String entryPoint,
        ProcessRecord app, int uid, int[] gids, int runtimeFlags, int mountExternal,
        String seInfo, String requiredAbi, String instructionSet, String invokeWith,
        long startTime) {
    try {
        checkSlow(startTime, "startProcess: asking zygote to start proc");
        final Process.ProcessStartResult startResult;
        if (hostingRecord.usesWebviewZygote()) {
            startResult = startWebView(entryPoint,
                    ......
        } else if (hostingRecord.usesAppZygote()) {
            final AppZygote appZygote = createAppZygoteForProcessIfNeeded(app);

            startResult = appZygote.getProcess().start(entryPoint,
                    ......
        } else {
            startResult = Process.start(entryPoint,
                    app.processName, uid, uid, gids, runtimeFlags, mountExternal,
                    app.info.targetSdkVersion, seInfo, requiredAbi, instructionSet,
                    app.info.dataDir, invokeWith, app.info.packageName,
                    new String[] {PROC_START_SEQ_IDENT + app.startSeq});
        }
        checkSlow(startTime, "startProcess: returned from zygote!");
        return startResult;
    } finally {
        ......
    }
}
```

这里的hostingRecord是在startProcess()调用时传入的参数`new HostingRecord(hostingType, hostingName)`，跟踪可以看到其mHostingZygote=REGULAR\_ZYGOTE。所以走的Process.start()。

## Zygote创建应用进程

```
//Process.java:
public static final ZygoteProcess ZYGOTE_PROCESS = new ZygoteProcess();

public static ProcessStartResult start(@NonNull final String processClass,
                                           @Nullable final String niceName,
                                           int uid, int gid, @Nullable int[] gids,
                                           int runtimeFlags,
                                           int mountExternal,
                                           int targetSdkVersion,
                                           @Nullable String seInfo,
                                           @NonNull String abi,
                                           @Nullable String instructionSet,
                                           @Nullable String appDataDir,
                                           @Nullable String invokeWith,
                                           @Nullable String packageName,
                                           @Nullable String[] zygoteArgs) {
        return ZYGOTE_PROCESS.start(processClass, niceName, uid, gid, gids,
                    runtimeFlags, mountExternal, targetSdkVersion, seInfo,
                    abi, instructionSet, appDataDir, invokeWith, packageName,
                    /*useUsapPool=*/ true, zygoteArgs);
    }
```

ZYGOTE\_PROCESS是新建的ZygoteProcess对象，在不带参数构造中定义了4中socket的地址。这里直接看ZygoteProcess.start()。

```
//ZygoteProcess.java:
public final Process.ProcessStartResult start(@NonNull final String processClass,
                                              ......) {
    ......
    try {
        return startViaZygote(processClass, niceName, uid, gid, gids,
                runtimeFlags, mountExternal, targetSdkVersion, seInfo,
                abi, instructionSet, appDataDir, invokeWith, /*startChildZygote=*/ false,
                packageName, useUsapPool, zygoteArgs);
    } catch (ZygoteStartFailedEx ex) {
    }
}

private Process.ProcessStartResult startViaZygote(@NonNull final String processClass,
                                                  ......)
                                                  throws ZygoteStartFailedEx {
    ArrayList<String> argsForZygote = new ArrayList<>();

    // --runtime-args, --setuid=, --setgid=,
    // and --setgroups= must go first
    argsForZygote.add("--runtime-args");
    argsForZygote.add("--setuid=" + uid);
    argsForZygote.add("--setgid=" + gid);
    argsForZygote.add("--runtime-flags=" + runtimeFlags);
    if (mountExternal == Zygote.MOUNT_EXTERNAL_DEFAULT) {
        argsForZygote.add("--mount-external-default");
    } 
    ......
    synchronized(mLock) {
        // The USAP pool can not be used if the application will not use the systems graphics
        // driver.  If that driver is requested use the Zygote application start path.
        return zygoteSendArgsAndGetResult(openZygoteSocketIfNeeded(abi),
                                          useUsapPool,
                                          argsForZygote);
    }
}
```

ZygoteProcess.start()调用了startViaZygote，argsForZygote保存了启动的应用进程的完整参数。最后调用zygoteSendArgsAndGetResult()发送参数 通过socket进行通信，完成应用进程的fork，并获取结果Process.ProcessStartResult。  
这里主要看下 openZygoteSocketIfNeeded()，这个是打开Zygote socket的过程。

```
//ZygoteProcess.java:
@GuardedBy("mLock")
private ZygoteState openZygoteSocketIfNeeded(String abi) throws ZygoteStartFailedEx {
    try {
        attemptConnectionToPrimaryZygote();

        if (primaryZygoteState.matches(abi)) {
            return primaryZygoteState;
        }
        if (mZygoteSecondarySocketAddress != null) {
            // The primary zygote didn't match. Try the secondary.
            attemptConnectionToSecondaryZygote();

            if (secondaryZygoteState.matches(abi)) {
                return secondaryZygoteState;
            }
        }
    } catch (IOException ioe) {
    }
}

@GuardedBy("mLock")
private void attemptConnectionToPrimaryZygote() throws IOException {
    if (primaryZygoteState == null || primaryZygoteState.isClosed()) {
        primaryZygoteState =
                ZygoteState.connect(mZygoteSocketAddress, mUsapPoolSocketAddress);

        maybeSetApiBlacklistExemptions(primaryZygoteState, false);
        maybeSetHiddenApiAccessLogSampleRate(primaryZygoteState);
        maybeSetHiddenApiAccessStatslogSampleRate(primaryZygoteState);
    }
}

private static class ZygoteState implements AutoCloseable {
   static ZygoteState connect(@NonNull LocalSocketAddress zygoteSocketAddress,
            @Nullable LocalSocketAddress usapSocketAddress)
            throws IOException {
        ......
        final LocalSocket zygoteSessionSocket = new LocalSocket();
        .......
        try {
            zygoteSessionSocket.connect(zygoteSocketAddress);
            zygoteInputStream = new DataInputStream(zygoteSessionSocket.getInputStream());
            zygoteOutputWriter =
                    new BufferedWriter(
                            new OutputStreamWriter(zygoteSessionSocket.getOutputStream()),
                            Zygote.SOCKET_BUFFER_SIZE);
        } catch (IOException ex) {
            ......
        }

        return new ZygoteState(zygoteSocketAddress, usapSocketAddress,
                               zygoteSessionSocket, zygoteInputStream, zygoteOutputWriter,
                               getAbiList(zygoteOutputWriter, zygoteInputStream));
    }
}
```

openZygoteSocketIfNeeded()返回一个 ZygoteState，这个是ZygoteProcess类的内部类，是保存与zygote进程进行通信时的状态信息。  
attemptConnectionToPrimaryZygote()和attemptConnectionToSecondaryZygote()类似。通过connect()打开socket并通过ZygoteState保存状态信息。

关于进入connect()后通过socket与zygote进程通信fork出应用进程的过程 个人也需进一步查看学习 在这里不说了。

## 应用进程主线程-执行ActivityThread的main()

当应用进程fork出来后，最终会执行到ActivityThread的main()方法。这个类在 [AMS之AMS的启动](https://www.cnblogs.com/fanglongxiang/p/13594986.html) 中说的比较多，那时主要是系统进程(system server)。关于应用进程相关的略过，这里主要是应用进程。

```
//ActivityThread.java:
public static void main(String[] args) {
    // Install selective syscall interception
    AndroidOs.install();

    // CloseGuard defaults to true and can be quite spammy.  We
    // disable it here, but selectively enable it later (via
    // StrictMode) on debug builds, but using DropBox, not logs.
    CloseGuard.setEnabled(false);

    Environment.initForCurrentUser();

    // Make sure TrustedCertificateStore looks in the right place for CA certificates
    final File configDir = Environment.getUserConfigDirectory(UserHandle.myUserId());
    TrustedCertificateStore.setDefaultUserDirectory(configDir);

    Process.setArgV0("<pre-initialized>");

    Looper.prepareMainLooper();

    ......
    ActivityThread thread = new ActivityThread();
    thread.attach(false, startSeq);

    if (sMainThreadHandler == null) {
        sMainThreadHandler = thread.getHandler();
    }
    ......
    Looper.loop();

    throw new RuntimeException("Main thread loop unexpectedly exited");
}
```

这里很多熟悉的地方：  
主线程默认创建的Looper，且不可退出。在 [Android消息机制(Handler)详述](https://www.cnblogs.com/fanglongxiang/p/13285896.html) 详细说过。  
创建了ActivityThread对象，执行了attach()，attach()中这里第一个参数是false，即非系统进程。 [AMS之AMS的启动](https://www.cnblogs.com/fanglongxiang/p/13594986.html) 则是true，是系统进程。  
下面直接看下ActivityThread.attach()。  
注：此时activity还未创建，activity的attach()还在后面。

```
@UnsupportedAppUsage
//ActivityThread.java:
private void attach(boolean system, long startSeq) {
    sCurrentActivityThread = this;
    mSystemThread = system;
    if (!system) {
        android.ddm.DdmHandleAppName.setAppName("<pre-initialized>",
                                                UserHandle.myUserId());
        RuntimeInit.setApplicationObject(mAppThread.asBinder());
        final IActivityManager mgr = ActivityManager.getService();
        try {
            mgr.attachApplication(mAppThread, startSeq);
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
        // Watch for getting close to heap limit.
        BinderInternal.addGcWatcher(new Runnable() {
            @Override public void run() {
                if (!mSomeActivitiesChanged) {
                    return;
                }
                Runtime runtime = Runtime.getRuntime();
                long dalvikMax = runtime.maxMemory();
                long dalvikUsed = runtime.totalMemory() - runtime.freeMemory();
                if (dalvikUsed > ((3*dalvikMax)/4)) {
                    if (DEBUG_MEMORY_TRIM) Slog.d(TAG, "Dalvik max=" + (dalvikMax/1024)
                            + " total=" + (runtime.totalMemory()/1024)
                            + " used=" + (dalvikUsed/1024));
                    mSomeActivitiesChanged = false;
                    try {
                        ActivityTaskManager.getService().releaseSomeActivities(mAppThread);
                    } catch (RemoteException e) {
                        throw e.rethrowFromSystemServer();
                    }
                }
            }
        });
    } else {
        ......
    }
    ......
}
```

主要来看下mgr.attachApplication()。很明显这个也是binder机制进行跨进程的，调用的是AMS的attachApplication()。

## 进入系统进程，将应用进程绑定到ATMS中

```
//ActivityManagerService.java：
@Override
public final void attachApplication(IApplicationThread thread, long startSeq) {
    if (thread == null) {
        throw new SecurityException("Invalid application interface");
    }
    synchronized (this) {
        int callingPid = Binder.getCallingPid();
        final int callingUid = Binder.getCallingUid();
        final long origId = Binder.clearCallingIdentity();
        attachApplicationLocked(thread, callingPid, callingUid, startSeq);
        Binder.restoreCallingIdentity(origId);
    }
}

@GuardedBy("this")
private boolean attachApplicationLocked(@NonNull IApplicationThread thread,
        int pid, int callingUid, long startSeq) {

    // Find the application record that is being attached...  either via
    // the pid if we are running in multiple processes, or just pull the
    // next app record if we are emulating process with anonymous threads.
    ProcessRecord app;
    long startTime = SystemClock.uptimeMillis();
    long bindApplicationTimeMillis;
    if (pid != MY_PID && pid >= 0) {
        synchronized (mPidsSelfLocked) {
            app = mPidsSelfLocked.get(pid);
        }
    ......
    final String processName = app.processName;
    ......
    final BackupRecord backupTarget = mBackupTargets.get(app.userId);
    try {
        ......
        mAtmInternal.preBindApplication(app.getWindowProcessController());
        final ActiveInstrumentation instr2 = app.getActiveInstrumentation();
        if (app.isolatedEntryPoint != null) {
            // This is an isolated process which should just call an entry point instead of
            // being bound to an application.
            thread.runIsolatedEntryPoint(app.isolatedEntryPoint, app.isolatedEntryPointArgs);
        } else if (instr2 != null) {
            thread.bindApplication(processName, appInfo, providers,
                    instr2.mClass,
                    profilerInfo, instr2.mArguments,
                    instr2.mWatcher,
                    instr2.mUiAutomationConnection, testMode,
                    mBinderTransactionTrackingEnabled, enableTrackAllocation,
                    isRestrictedBackupMode || !normalMode, app.isPersistent(),
                    new Configuration(app.getWindowProcessController().getConfiguration()),
                    app.compat, getCommonServicesLocked(app.isolated),
                    mCoreSettingsObserver.getCoreSettingsLocked(),
                    buildSerial, autofillOptions, contentCaptureOptions);
        } else {
            thread.bindApplication(processName, appInfo, providers, null, profilerInfo,
                    null, null, null, testMode,
                    mBinderTransactionTrackingEnabled, enableTrackAllocation,
                    isRestrictedBackupMode || !normalMode, app.isPersistent(),
                    new Configuration(app.getWindowProcessController().getConfiguration()),
                    app.compat, getCommonServicesLocked(app.isolated),
                    mCoreSettingsObserver.getCoreSettingsLocked(),
                    buildSerial, autofillOptions, contentCaptureOptions);
        }
        .......
    } catch (Exception e) {
        ......
    // See if the top visible activity is waiting to run in this process...
    if (normalMode) {
        try {
            didSomething = mAtmInternal.attachApplication(app.getWindowProcessController());
        } catch (Exception e) {
            Slog.wtf(TAG, "Exception thrown launching activities in " + app, e);
            badApp = true;
        }
    }
    ......
    return true;
}
```

`attachApplication()->attachApplicationLocked()`，主要看下thread.bindApplication()和mAtmInternal.attachApplication()。  
thread.bindApplication()实际调用的是 ApplicationThread下的 bindApplication()。ApplicationThread是ActivityThread的内部类，`private class ApplicationThread extends IApplicationThread.Stub`。注意当前是在AMS进程，其中的thread是传入的 是应用进程主线程。  
mAtmInternal.attachApplication()最终调用的是ATMS中的 attachApplication()。

**先来看下thread.bindApplication()**

```
//ActivityThread.java->ApplicationThread class：
public final void bindApplication(String processName, ApplicationInfo appInfo,
        ......) {
    ......
    data.contentCaptureOptions = contentCaptureOptions;
    sendMessage(H.BIND_APPLICATION, data);
}

//ActivityThread.java->H class：
class H extends Handler {
    public static final int BIND_APPLICATION        = 110;
    public void handleMessage(Message msg) {
        switch (msg.what) {
            case BIND_APPLICATION:
                Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "bindApplication");
                AppBindData data = (AppBindData)msg.obj;
                handleBindApplication(data);
                Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
                break;
        }
    }
}
//ActivityThread.java:
private void handleBindApplication(AppBindData data) {
    ......
    data.info = getPackageInfoNoCheck(data.appInfo, data.compatInfo);
    ......
    // Continue loading instrumentation.
    if (ii != null) {
        ......
        try {
            final ClassLoader cl = instrContext.getClassLoader();
            mInstrumentation = (Instrumentation)
                cl.loadClass(data.instrumentationName.getClassName()).newInstance();
        ......
    }
    ......
    // Allow disk access during application and provider setup. This could
    // block processing ordered broadcasts, but later processing would
    // probably end up doing the same disk access.
    Application app;

    try {
        app = data.info.makeApplication(data.restrictedBackupMode, null);
        ......
        try {
            mInstrumentation.callApplicationOnCreate(app);
        } catch (Exception e) {
            ......
        }
    }
    ......
}

public final LoadedApk getPackageInfoNoCheck(ApplicationInfo ai,
        CompatibilityInfo compatInfo) {
    return getPackageInfo(ai, compatInfo, null, false, true, false);
}

//Application.java:
/**
* Called when the application is starting, before any activity, service,
* or receiver objects (excluding content providers) have been created.
*/
@CallSuper
public void onCreate() {
}
```

这时ApplicationThread和ActivityThread在同一进程中，所以bindApplication()通过handler通信，发送message(BIND\_APPLICATION)，直接看到处理部分handleBindApplication()。  
通过 `cl.loadClass(data.instrumentationName.getClassName()).newInstance()` 反射创建了Instrumentation对象，这个在 [AMS之AMS的启动](https://www.cnblogs.com/fanglongxiang/p/13594986.html) 也说过。  
通过 `getPackageInfoNoCheck()` 创建LoadedApk对象并保存在data.info。代码流程：`getPackageInfoNoCheck()->getPackageInfo()->new LoadedApk()`，都在ActivityThread中。  
通过`data.info.makeApplication(data.restrictedBackupMode, null)`创建了Application。关键代码看下面。  
最后通过`mInstrumentation.callApplicationOnCreate(app)`调用了`app.onCreate();`，Application创建完成。  
这些操作是thread下的，前面说过是传入的 应用进程主线程。所以创建Application是在应用进程中的。

```
//LoadedApk.java:   
@UnsupportedAppUsage
public Application makeApplication(boolean forceDefaultAppClass,
        Instrumentation instrumentation) {
    Application app = null;
    String appClass = mApplicationInfo.className;
    try {            
        java.lang.ClassLoader cl = getClassLoader();
        ......
        ContextImpl appContext = ContextImpl.createAppContext(mActivityThread, this);
        app = mActivityThread.mInstrumentation.newApplication(
                cl, appClass, appContext);
        appContext.setOuterContext(app);
    } catch (Exception e) {
        
    }
    mActivityThread.mAllApplications.add(app);
    mApplication = app;  
    return app;
}

//Instrumentation.java:
public Application newApplication(ClassLoader cl, String className, Context context)
        throws InstantiationException, IllegalAccessException, 
        ClassNotFoundException {
    Application app = getFactory(context.getPackageName())
            .instantiateApplication(cl, className);
    app.attach(context);
    return app;
}

private AppComponentFactory getFactory(String pkg) {
    ......
    return apk.getAppFactory();
}

//AppComponentFactory.java:
public @NonNull Application instantiateApplication(@NonNull ClassLoader cl,
        @NonNull String className)
        throws InstantiationException, IllegalAccessException, ClassNotFoundException {
    return (Application) cl.loadClass(className).newInstance();
}

//Application.java:
/* package */ final void attach(Context context) {
    attachBaseContext(context);
    mLoadedApk = ContextImpl.getImpl(context).mPackageInfo;
}
//
```

这就是makeApplication()方法创建Application的过程。注意 makeApplication()传入的instrumentation为null，Application的实例化也是通过反射。

**接着看第二点mAtmInternal.attachApplication()**

```
//ActivityTaskManagerService.java:
@HotPath(caller = HotPath.PROCESS_CHANGE)
@Override
public boolean attachApplication(WindowProcessController wpc) throws RemoteException {
    synchronized (mGlobalLockWithoutBoost) {
        return mRootActivityContainer.attachApplication(wpc);
    }
}
//RootActivityContainer.java:
boolean attachApplication(WindowProcessController app) throws RemoteException {
    final String processName = app.mName;
    boolean didSomething = false;
    for (int displayNdx = mActivityDisplays.size() - 1; displayNdx >= 0; --displayNdx) {
        final ActivityDisplay display = mActivityDisplays.get(displayNdx);
        final ActivityStack stack = display.getFocusedStack();
        if (stack != null) {
            stack.getAllRunningVisibleActivitiesLocked(mTmpActivityList);
            final ActivityRecord top = stack.topRunningActivityLocked();
            final int size = mTmpActivityList.size();
            for (int i = 0; i < size; i++) {
                final ActivityRecord activity = mTmpActivityList.get(i);
                if (activity.app == null && app.mUid == activity.info.applicationInfo.uid
                        && processName.equals(activity.processName)) {
                    try {
                        if (mStackSupervisor.realStartActivityLocked(activity, app,
                                top == activity /* andResume */, true /* checkConfig */)) {
                            didSomething = true;
                        }
                    } catch (RemoteException e) {
                        ......
                    }
                }
            }
        }
    }
    ......
    return didSomething;
}

//ActivityStackSupervisor.java：
boolean realStartActivityLocked(ActivityRecord r, WindowProcessController proc,
        boolean andResume, boolean checkConfig) throws RemoteException {
    ......
    try {
        r.startFreezingScreenLocked(proc, 0);
        // schedule launch ticks to collect information about slow apps.
        r.startLaunchTickingLocked();
        r.setProcess(proc);
        ......
        try {
            // Create activity launch transaction.
            final ClientTransaction clientTransaction = ClientTransaction.obtain(
                    proc.getThread(), r.appToken);

            final DisplayContent dc = r.getDisplay().mDisplayContent;
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

            // Set desired final state.
            final ActivityLifecycleItem lifecycleItem;
            if (andResume) {
                lifecycleItem = ResumeActivityItem.obtain(dc.isNextTransitionForward());
            } else {
                lifecycleItem = PauseActivityItem.obtain();
            }
            clientTransaction.setLifecycleStateRequest(lifecycleItem);

            // Schedule transaction.
            mService.getLifecycleManager().scheduleTransaction(clientTransaction);
            ......

        } catch (RemoteException e) {
            
        }
    } finally {
    }
    ......
    return true;
}
```

attachApplication()关键代码 一直调用到realStartActivityLocked()。这里有几点注意。

-   这里完善了ActivityRecord，设置了进程等信息。总体上可以理解，应为ActivityRecord、ProcessRecord等由AMS/ATMS管理，这里将application绑定到了ATMS。
-   创建了ClientTransaction对象
-   设置了ClientTransaction的callback，为一个创建的LaunchActivityItem对象
-   设置生命周期，为创建的lifecycleItem对象
-   通过`mService.getLifecycleManager().scheduleTransaction(clientTransaction)`发送请求。这里的mService为ATMS，这里的mService.getLifecycleManager()即ClientLifecycleManager。
-   创建Application后，通过attachApplication()绑定到ATMS。当前还是在系统进程中。

创建了ClientTransaction对象和设置callback的相关代码，可以了解下。

```
//ClientTransaction.java
public void addCallback(ClientTransactionItem activityCallback) {
    if (mActivityCallbacks == null) {
        mActivityCallbacks = new ArrayList<>();
    }
    mActivityCallbacks.add(activityCallback);
}

public static ClientTransaction obtain(IApplicationThread client, IBinder activityToken) {
    ClientTransaction instance = ObjectPool.obtain(ClientTransaction.class);
    if (instance == null) {
        instance = new ClientTransaction();
    }
    instance.mClient = client;
    instance.mActivityToken = activityToken;

    return instance;
}
//WindowProcessController.java:
IApplicationThread getThread() {
    return mThread;
}
```

clientTransaction相关有大致了解后，直接看最后发送请求代码。

```
//ClientLifecycleManager.java:
void scheduleTransaction(ClientTransaction transaction) throws RemoteException {
    final IApplicationThread client = transaction.getClient();
    transaction.schedule();
    if (!(client instanceof Binder)) {
        // If client is not an instance of Binder - it's a remote call and at this point it is
        // safe to recycle the object. All objects used for local calls will be recycled after
        // the transaction is executed on client in ActivityThread.
        transaction.recycle();
    }
}

//ClientTransaction.java
public void schedule() throws RemoteException {
    mClient.scheduleTransaction(this);
}
```

mClient是啥呢？在创建ClientTransaction对象时赋值的。mClient在obtain()时传入的，即proc.getThread()，`final ClientTransaction clientTransaction = ClientTransaction.obtain(proc.getThread(), r.appToken)`。  
ApplicationThread是ActivityThread的内部类，继承IApplicationThread.Stub，即是一个Binder。`private class ApplicationThread extends IApplicationThread.Stub`。  
ApplicationThread是作为Activitythread和AMS/ATMS通信的桥梁。它与ActivityThread之间通过handler通信，AMS获取ApplicationThread的binder进行通信。  
**这里开始，实际就是从系统进程回到了应用进程。**  
这里的过程是应用进程通过binder(IPC)执行mgr.attachApplication()进入系统进程，ATMS通过回调ApplicationThread.scheduleTransaction()，然后通过handler回到应用进程的主线程。

> ATMS回调ApplicationThread的方法，该方法在Binder线程池中的线程执行，所以需要使用Handler来切换线程到ActivityThread所在线程。

所以这里实际调用的就是ApplicationThread的 scheduleTransaction 方法。下面来看下。

```
//ActivityThread.java->ApplicationThread class：
@Override
public void scheduleTransaction(ClientTransaction transaction) throws RemoteException {
    ActivityThread.this.scheduleTransaction(transaction);
}

//ClientTransactionHandler.java:
/** Prepare and schedule transaction for execution. */
void scheduleTransaction(ClientTransaction transaction) {
    transaction.preExecute(this);
    sendMessage(ActivityThread.H.EXECUTE_TRANSACTION, transaction);
}

//ActivityThread.java:
//public final class ActivityThread extends ClientTransactionHandler
private final TransactionExecutor mTransactionExecutor = new TransactionExecutor(this);
//TransactionExecutor.java:
/** Initialize an instance with transaction handler, that will execute all requested actions. */
public TransactionExecutor(ClientTransactionHandler clientTransactionHandler) {
    mTransactionHandler = clientTransactionHandler;
}


//ActivityThread.java->H class：
class H extends Handler {
    public void handleMessage(Message msg) {
        switch (msg.what) {
            case EXECUTE_TRANSACTION:
                final ClientTransaction transaction = (ClientTransaction) msg.obj;
                mTransactionExecutor.execute(transaction);
                if (isSystem()) {
                    // Client transactions inside system process are recycled on the client side
                    // instead of ClientLifecycleManager to avoid being cleared before this
                    // message is handled.
                    transaction.recycle();
                }
                // TODO(lifecycler): Recycle locally scheduled transactions.
                break;
        }
    }
}
```

可以看到scheduleTransaction() 最终通过hanlder进行处理的，执行到TransactionExecutor的execute()。  
注意上面关于TransactionExecutor的创建，this是ActivityThread 作为参数闯入到构造函数中，ActivityThread是继承了ClientTransactionHandler的。mTransactionHandler即ActivityThread，这是应用进程的主线程，后面出现要知道。

```
//TransactionExecutor.java:
public void execute(ClientTransaction transaction) {
    ......
    executeCallbacks(transaction);

    executeLifecycleState(transaction);
    mPendingActions.clear();
}

/** Cycle through all states requested by callbacks and execute them at proper times. */
@VisibleForTesting
public void executeCallbacks(ClientTransaction transaction) {
    final List<ClientTransactionItem> callbacks = transaction.getCallbacks();
    ......
    final int size = callbacks.size();
    for (int i = 0; i < size; ++i) {
        final ClientTransactionItem item = callbacks.get(i);
        ......
        item.execute(mTransactionHandler, token, mPendingActions);
        item.postExecute(mTransactionHandler, token, mPendingActions);
        ......
    }
}
```

在上面realStartActivityLocked()中，设置的callback是LaunchActivityItem对象。这里execute()最终执行到LaunchActivityItem的execute()。 继续看

```
//LaunchActivityItem.java:
@Override
public void execute(ClientTransactionHandler client, IBinder token,
        PendingTransactionActions pendingActions) {
    Trace.traceBegin(TRACE_TAG_ACTIVITY_MANAGER, "activityStart");
    ActivityClientRecord r = new ActivityClientRecord(token, mIntent, mIdent, mInfo,
            mOverrideConfig, mCompatInfo, mReferrer, mVoiceInteractor, mState, mPersistentState,
            mPendingResults, mPendingNewIntents, mIsForward,
            mProfilerInfo, client, mAssistToken);
    client.handleLaunchActivity(r, pendingActions, null /* customIntent */);
    Trace.traceEnd(TRACE_TAG_ACTIVITY_MANAGER);
}
```

上面有说过的，这里的client就是ActivityThread。所以走到了ActivityThread的handleLaunchActivity()。

## 回到应用进程，创建activity并执行attach()和onCreate()

这里的client就是ActivityThread，上面也说过，其实是应用进程的主线程。  
这里主要是创建出应用第一个Activity，并执行了attach()和onCreate()。

```
//ActivityThread.java:
public Activity handleLaunchActivity(ActivityClientRecord r,
        PendingTransactionActions pendingActions, Intent customIntent) {
    ......
    final Activity a = performLaunchActivity(r, customIntent);
    ......
    return a;
}
    
private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
    ActivityInfo aInfo = r.activityInfo;
    ......
    ComponentName component = r.intent.getComponent();
    ......
    ContextImpl appContext = createBaseContextForActivity(r);
    Activity activity = null;
    try {
        java.lang.ClassLoader cl = appContext.getClassLoader();
        activity = mInstrumentation.newActivity(
                cl, component.getClassName(), r.intent);
        ......
    }
    ......
    try {
        Application app = r.packageInfo.makeApplication(false, mInstrumentation);
        ......
        if (activity != null) {
            ......
            appContext.setOuterContext(activity);
            activity.attach(appContext, this, getInstrumentation(), r.token,
                    r.ident, app, r.intent, r.activityInfo, title, r.parent,
                    r.embeddedID, r.lastNonConfigurationInstances, config,
                    r.referrer, r.voiceInteractor, window, r.configCallback,
                    r.assistToken);
            ......
            if (r.isPersistable()) {
                mInstrumentation.callActivityOnCreate(activity, r.state, r.persistentState);
            } else {
                mInstrumentation.callActivityOnCreate(activity, r.state);
            }
            ......
            r.activity = activity;
        }
        r.setState(ON_CREATE);
    }
    .......
    return activity;
}
```

这里是app启动中activity创建起来的最后一个阶段了。上面主要看3点

-   mInstrumentation.newActivity()创建了一个activity
-   activity.attach()，这个就是Activity中attach()的地方，将Context、ActivityThread、Instrumentation、Application、Window等等重要信息关联到了activity中。这里代码不列出了。
-   mInstrumentation.callActivityOnCreate()。执行了Activity的onCreate()方法。  
    下面来看下activity的创建和如何执行到onCreate()的。

**创建activity**

```
//Instrumentation.java:
public Activity newActivity(ClassLoader cl, String className,
        Intent intent)
        throws InstantiationException, IllegalAccessException,
        ClassNotFoundException {
    String pkg = intent != null && intent.getComponent() != null
            ? intent.getComponent().getPackageName() : null;
    return getFactory(pkg).instantiateActivity(cl, className, intent);
}

private AppComponentFactory getFactory(String pkg) {
    ......
}

//AppComponentFactory.java:
public @NonNull Activity instantiateActivity(@NonNull ClassLoader cl, @NonNull String className,
        @Nullable Intent intent)
        throws InstantiationException, IllegalAccessException, ClassNotFoundException {
    return (Activity) cl.loadClass(className).newInstance();
}
```

getFactory()获取的是AppComponentFactory对象。通过反射生成了Activity。

**执行Activity.onCreate()**

```
// Instrumentation.java
public void callActivityOnCreate(Activity activity, Bundle icicle) {
    prePerformCreate(activity);
    activity.performCreate(icicle);
    postPerformCreate(activity);
}

// Activity.java
final void performCreate(Bundle icicle) {
    performCreate(icicle, null);
}

@UnsupportedAppUsage
final void performCreate(Bundle icicle, PersistableBundle persistentState) {
    ......
    if (persistentState != null) {
        onCreate(icicle, persistentState);
    } else {
        onCreate(icicle);
    }
    ......
}
```

## 附上调用栈

**从抛出异常中 获取方法调用的堆栈信息。在需要的方法中添加如下一行即可：**

```
//android.util.Log
Log.d(TAG, Log.getStackTraceString(new Throwable()));
```

系统中已经添加了一些类似下面的Trace标记，使用Systrace获取后进一步查看分析。

```
Trace.traceBegin(long traceTag, String methodName);
Trace.traceEnd(long traceTag);
```

下面是几个方法的调用堆栈信息，供参考了解下这个方法，可以自己尝试下 会发现很好用<sup>v</sup>。  
注意下部分方法有两次不同的内容，这是差异。文章第二部分的流程都是按首次启动进行的，即与下面两次中的第一次调用堆栈内容一致。  
**添加在：Instrumentation.java->execStartActivity()**

```
09-16 10:10:16.899  2520  2520 D flx_ams : java.lang.Throwable
09-16 10:10:16.899  2520  2520 D flx_ams : at android.app.Instrumentation.execStartActivity(Instrumentation.java:1681)
09-16 10:10:16.899  2520  2520 D flx_ams : at android.app.Activity.startActivityForResult(Activity.java:5255)
09-16 10:10:16.899  2520  2520 D flx_ams : at com.android.launcher3.Launcher.startActivityForResult(Launcher.java:1609)
09-16 10:10:16.899  2520  2520 D flx_ams : at android.app.Activity.startActivity(Activity.java:5580)
09-16 10:10:16.899  2520  2520 D flx_ams : at com.android.launcher3.BaseDraggingActivity.startActivitySafely(BaseDraggingActivity.java:176)
09-16 10:10:16.899  2520  2520 D flx_ams : at com.android.launcher3.Launcher.startActivitySafely(Launcher.java:1933)
09-16 10:10:16.899  2520  2520 D flx_ams : at com.android.launcher3.touch.ItemClickHandler.startAppShortcutOrInfoActivity(ItemClickHandler.java:267)
09-16 10:10:16.899  2520  2520 D flx_ams : at com.android.launcher3.touch.ItemClickHandler.onClickAppShortcut(ItemClickHandler.java:232)
09-16 10:10:16.899  2520  2520 D flx_ams : at com.android.launcher3.touch.ItemClickHandler.onClick(ItemClickHandler.java:96)
09-16 10:10:16.899  2520  2520 D flx_ams : at com.android.launcher3.touch.ItemClickHandler.lambda$getInstance$0(ItemClickHandler.java:67)
09-16 10:10:16.899  2520  2520 D flx_ams : at com.android.launcher3.touch.-$$Lambda$ItemClickHandler$_rHy-J5yxnvGlFKWSh6CdrSf-lA.onClick(Unknown Source:2)
09-16 10:10:16.899  2520  2520 D flx_ams : at android.view.View.performClick(View.java:7171)
09-16 10:10:16.899  2520  2520 D flx_ams : at android.view.View.performClickInternal(View.java:7148)
09-16 10:10:16.899  2520  2520 D flx_ams : at android.view.View.access$3500(View.java:802)
09-16 10:10:16.899  2520  2520 D flx_ams : at android.view.View$PerformClick.run(View.java:27409)
09-16 10:10:16.899  2520  2520 D flx_ams : at android.os.Handler.handleCallback(Handler.java:883)
09-16 10:10:16.899  2520  2520 D flx_ams : at android.os.Handler.dispatchMessage(Handler.java:100)
09-16 10:10:16.899  2520  2520 D flx_ams : at android.os.Looper.loop(Looper.java:214)
09-16 10:10:16.899  2520  2520 D flx_ams : at android.app.ActivityThread.main(ActivityThread.java:7643)
09-16 10:10:16.899  2520  2520 D flx_ams : at java.lang.reflect.Method.invoke(Native Method)
09-16 10:10:16.899  2520  2520 D flx_ams : at com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run(RuntimeInit.java:503)
09-16 10:10:16.899  2520  2520 D flx_ams : at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:936)
```

execStartActivity()：点击launcher图标后的，进入ATMS前的。

**添加在：ActivityStack.java->startActivityLocked()**

```
09-16 10:10:16.994   877  1956 D flx_ams : java.lang.Throwable
09-16 10:10:16.994   877  1956 D flx_ams : at com.android.server.wm.ActivityStack.startActivityLocked(ActivityStack.java:3163)
09-16 10:10:16.994   877  1956 D flx_ams : at com.android.server.wm.ActivityStarter.startActivityUnchecked(ActivityStarter.java:1755)
09-16 10:10:16.994   877  1956 D flx_ams : at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:1417)
09-16 10:10:16.994   877  1956 D flx_ams : at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:956)
09-16 10:10:16.994   877  1956 D flx_ams : at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:586)
09-16 10:10:16.994   877  1956 D flx_ams : at com.android.server.wm.ActivityStarter.startActivityMayWait(ActivityStarter.java:1311)
09-16 10:10:16.994   877  1956 D flx_ams : at com.android.server.wm.ActivityStarter.execute(ActivityStarter.java:517)
09-16 10:10:16.994   877  1956 D flx_ams : at com.android.server.wm.ActivityTaskManagerService.startActivityAsUser(ActivityTaskManagerService.java:1075)
09-16 10:10:16.994   877  1956 D flx_ams : at com.android.server.wm.ActivityTaskManagerService.startActivityAsUser(ActivityTaskManagerService.java:1049)
09-16 10:10:16.994   877  1956 D flx_ams : at com.android.server.wm.ActivityTaskManagerService.startActivity(ActivityTaskManagerService.java:1026)
09-16 10:10:16.994   877  1956 D flx_ams : at android.app.IActivityTaskManager$Stub.onTransact(IActivityTaskManager.java:1486)
09-16 10:10:16.994   877  1956 D flx_ams : at android.os.Binder.execTransactInternal(Binder.java:1045)
09-16 10:10:16.994   877  1956 D flx_ams : at android.os.Binder.execTransact(Binder.java:1018)
```

**添加在：RootActivityContainer.java->resumeFocusedStacksTopActivities()**

```
09-16 10:10:17.020   877  1956 D flx_ams : java.lang.Throwable
09-16 10:10:17.020   877  1956 D flx_ams : at com.android.server.wm.RootActivityContainer.resumeFocusedStacksTopActivities(RootActivityContainer.java:1162)
09-16 10:10:17.020   877  1956 D flx_ams : at com.android.server.wm.ActivityStarter.startActivityUnchecked(ActivityStarter.java:1783)
09-16 10:10:17.020   877  1956 D flx_ams : at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:1417)
09-16 10:10:17.020   877  1956 D flx_ams : at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:956)
09-16 10:10:17.020   877  1956 D flx_ams : at com.android.server.wm.ActivityStarter.startActivity(ActivityStarter.java:586)
09-16 10:10:17.020   877  1956 D flx_ams : at com.android.server.wm.ActivityStarter.startActivityMayWait(ActivityStarter.java:1311)
09-16 10:10:17.020   877  1956 D flx_ams : at com.android.server.wm.ActivityStarter.execute(ActivityStarter.java:517)
09-16 10:10:17.020   877  1956 D flx_ams : at com.android.server.wm.ActivityTaskManagerService.startActivityAsUser(ActivityTaskManagerService.java:1075)
09-16 10:10:17.020   877  1956 D flx_ams : at com.android.server.wm.ActivityTaskManagerService.startActivityAsUser(ActivityTaskManagerService.java:1049)
09-16 10:10:17.020   877  1956 D flx_ams : at com.android.server.wm.ActivityTaskManagerService.startActivity(ActivityTaskManagerService.java:1026)
09-16 10:10:17.020   877  1956 D flx_ams : at android.app.IActivityTaskManager$Stub.onTransact(IActivityTaskManager.java:1486)
09-16 10:10:17.020   877  1956 D flx_ams : at android.os.Binder.execTransactInternal(Binder.java:1045)
09-16 10:10:17.020   877  1956 D flx_ams : at android.os.Binder.execTransact(Binder.java:1018)

09-16 10:10:17.039   877   894 D flx_ams : java.lang.Throwable
09-16 10:10:17.039   877   894 D flx_ams : at com.android.server.wm.RootActivityContainer.resumeFocusedStacksTopActivities(RootActivityContainer.java:1162)
09-16 10:10:17.039   877   894 D flx_ams : at com.android.server.wm.ActivityStack.completePauseLocked(ActivityStack.java:1852)
09-16 10:10:17.039   877   894 D flx_ams : at com.android.server.wm.ActivityStack.activityPausedLocked(ActivityStack.java:1773)
09-16 10:10:17.039   877   894 D flx_ams : at com.android.server.wm.ActivityTaskManagerService.activityPaused(ActivityTaskManagerService.java:1756)
09-16 10:10:17.039   877   894 D flx_ams : at android.app.IActivityTaskManager$Stub.onTransact(IActivityTaskManager.java:1981)
09-16 10:10:17.039   877   894 D flx_ams : at android.os.Binder.execTransactInternal(Binder.java:1045)
09-16 10:10:17.039   877   894 D flx_ams : at android.os.Binder.execTransact(Binder.java:1018)
```

注意，这就有两份不同的，具体情况可以跟踪看看。之前讲到的与第一份一致。

**添加在：ActivityThread.java->attach()**

```
09-16 10:10:17.496  6896  6896 D flx_ams : java.lang.Throwable
09-16 10:10:17.496  6896  6896 D flx_ams : at android.app.ActivityThread.attach(ActivityThread.java:7317)
09-16 10:10:17.496  6896  6896 D flx_ams : at android.app.ActivityThread.main(ActivityThread.java:7627)
09-16 10:10:17.496  6896  6896 D flx_ams : at java.lang.reflect.Method.invoke(Native Method)
09-16 10:10:17.496  6896  6896 D flx_ams : at com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run(RuntimeInit.java:503)
09-16 10:10:17.496  6896  6896 D flx_ams : at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:936)
```

**添加在：LaunchActivityItem.java->execute()**

```
09-16 10:10:17.850  6896  6896 D flx_ams : java.lang.Throwable
09-16 10:10:17.850  6896  6896 D flx_ams : at android.app.servertransaction.LaunchActivityItem.execute(LaunchActivityItem.java:78)
09-16 10:10:17.850  6896  6896 D flx_ams : at android.app.servertransaction.TransactionExecutor.executeCallbacks(TransactionExecutor.java:135)
09-16 10:10:17.850  6896  6896 D flx_ams : at android.app.servertransaction.TransactionExecutor.execute(TransactionExecutor.java:95)
09-16 10:10:17.850  6896  6896 D flx_ams : at android.app.ActivityThread$H.handleMessage(ActivityThread.java:2055)
09-16 10:10:17.850  6896  6896 D flx_ams : at android.os.Handler.dispatchMessage(Handler.java:107)
09-16 10:10:17.850  6896  6896 D flx_ams : at android.os.Looper.loop(Looper.java:214)
09-16 10:10:17.850  6896  6896 D flx_ams : at android.app.ActivityThread.main(ActivityThread.java:7643)
09-16 10:10:17.850  6896  6896 D flx_ams : at java.lang.reflect.Method.invoke(Native Method)
09-16 10:10:17.850  6896  6896 D flx_ams : at com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run(RuntimeInit.java:503)
09-16 10:10:17.850  6896  6896 D flx_ams : at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:936)
```

点击的camera，execStartActivity()获取到的过程

```
09-16 10:10:18.169  6896  6896 D flx_ams : java.lang.Throwable
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.Instrumentation.execStartActivity(Instrumentation.java:1681)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.Activity.startActivityForResult(Activity.java:5255)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.Activity.startActivityForResult(Activity.java:5213)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.Activity.startActivity(Activity.java:5584)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.Activity.startActivity(Activity.java:5552)
09-16 10:10:18.169  6896  6896 D flx_ams : at com.android.camera.CameraActivity.checkPermissions(CameraActivity.java:2522)
09-16 10:10:18.169  6896  6896 D flx_ams : at com.android.camera.CameraActivity.checkCameraPermission(CameraActivity.java:1297)
09-16 10:10:18.169  6896  6896 D flx_ams : at com.android.camera.CameraActivity.checkPermissionOnCreate(CameraActivity.java:4874)
09-16 10:10:18.169  6896  6896 D flx_ams : at com.android.camera.CameraActivity.onCreateTasks(CameraActivity.java:1722)
09-16 10:10:18.169  6896  6896 D flx_ams : at com.android.camera.util.QuickActivity.onCreate(QuickActivity.java:115)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.Activity.performCreate(Activity.java:7873)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.Activity.performCreate(Activity.java:7861)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.Instrumentation.callActivityOnCreate(Instrumentation.java:1306)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.ActivityThread.performLaunchActivity(ActivityThread.java:3310)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.ActivityThread.handleLaunchActivity(ActivityThread.java:3491)
9-16 10:10:18.169  6896  6896 D flx_ams : at android.app.servertransaction.LaunchActivityItem.execute(LaunchActivityItem.java:84)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.servertransaction.TransactionExecutor.executeCallbacks(TransactionExecutor.java:135)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.servertransaction.TransactionExecutor.execute(TransactionExecutor.java:95)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.ActivityThread$H.handleMessage(ActivityThread.java:2055)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.os.Handler.dispatchMessage(Handler.java:107)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.os.Looper.loop(Looper.java:214)
09-16 10:10:18.169  6896  6896 D flx_ams : at android.app.ActivityThread.main(ActivityThread.java:7643)
09-16 10:10:18.169  6896  6896 D flx_ams : at java.lang.reflect.Method.invoke(Native Method)
09-16 10:10:18.169  6896  6896 D flx_ams : at com.android.internal.os.RuntimeInit$MethodAndArgsCaller.run(RuntimeInit.java:503)
09-16 10:10:18.169  6896  6896 D flx_ams : at com.android.internal.os.ZygoteInit.main(ZygoteInit.java:936)
```