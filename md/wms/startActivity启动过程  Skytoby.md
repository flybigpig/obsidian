> 基于Android10.0，分析startActivity的启动过程

## [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#%E4%B8%80%E3%80%81%E6%A6%82%E8%BF%B0 "一、概述")一、概述

startActivity的整体流程和startService相近，启动后都是通过AMS来完成的。但相比service启动更加复杂，多了任务栈、UI、生命周期。其启动流程如下：

[![startActivity](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/startActivity.jpg)](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/startActivity.jpg)

## [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#%E4%BA%8C%E3%80%81%E5%90%AF%E5%8A%A8%E6%B5%81%E7%A8%8B "二、启动流程")二、启动流程

启动Activity，一般是用startActivity。

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-1-Activity-startActivity "2.1 Activity.startActivity")2.1 Activity.startActivity

\[->Activity.java\]

```
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
```

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-2-startActivityForResult "2.2  startActivityForResult")2.2 startActivityForResult

\[->ContextImpl.java\]

```
/**  
   * @hide  
   */  
  @Override  
  @UnsupportedAppUsage  
  public void startActivityForResult(  
          String who, Intent intent, int requestCode, @Nullable Bundle options) {  
      Uri referrer = onProvideReferrer();  
      if (referrer != null) {  
          intent.putExtra(Intent.EXTRA_REFERRER, referrer);  
      }  
      options = transferSpringboardActivityOptions(options);  
      Instrumentation.ActivityResult ar =  
          mInstrumentation.execStartActivity(  
              this, mMainThread.getApplicationThread(), mToken, who,  
              intent, requestCode, options);  
      if (ar != null) {  
          mMainThread.sendActivityResult(  
              mToken, who, requestCode,  
              ar.getResultCode(), ar.getResultData());  
      }  
      cancelInputsAndStartExitTransition(options);  
  }
```


execStartActivity方法参数：

mAppThread:类型为ApplicationThread,通过mMainThread.getApplicationThread()获取。

mToken：为Binder类型

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-3-execStartActivity "2.3  execStartActivity")2.3 execStartActivity

\[->Instrumentation.java\]

```
public ActivityResult execStartActivity(  
      Context who, IBinder contextThread, IBinder token, String target,  
      Intent intent, int requestCode, Bundle options) {  
      IApplicationThread whoThread = (IApplicationThread) contextThread;  
      if (mActivityMonitors != null) {  
          synchronized (mSync) {  
              final int N = mActivityMonitors.size();  
              for (int i=0; i<N; i++) {  
                  final ActivityMonitor am = mActivityMonitors.get(i);  
                  ActivityResult result = null;  
                  if (am.ignoreMatchingSpecificIntents()) {  
                      result = am.onStartActivity(intent);  
                  }  
                  if (result != null) {  
                      am.mHits++;  
                      return result;  
                  } else if (am.match(who, null, intent)) {  
                      am.mHits++;  
                      //如果am阻塞activity启动，则返回  
                      if (am.isBlocking()) {  
                          return requestCode >= 0 ? am.getResult() : null;  
                      }  
                      break;  
                  }  
              }  
          }  
      }  
      try {  
          intent.migrateExtraStreamToClipData();  
          intent.prepareToLeaveProcess(who);  
          int result = ActivityManager.getService()  
              .startActivity(whoThread, who.getBasePackageName(), intent,  
                      intent.resolveTypeIfNeeded(who.getContentResolver()),  
                      token, target, requestCode, 0, null, options);  
          checkStartActivityResult(result, intent);  
      } catch (RemoteException e) {  
          throw new RuntimeException("Failure from system", e);  
      }  
      return null;  
  }
```

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-3-1-AM-getService "2.3.1  AM.getService")2.3.1 AM.getService

和Android6.0不同的是Android10.0直接通过AIDL的方式生成了AMS的代理。

```
/**  
    * @hide  
    */  
   @UnsupportedAppUsage  
   public static IActivityManager getService() {  
       return IActivityManagerSingleton.get();  
   }  
   public abstract class Singleton<T> {  
   @UnsupportedAppUsage  
   private T mInstance;  
  
   protected abstract T create();  
  
   @UnsupportedAppUsage  
   public final T get() {  
       synchronized (this) {  
           if (mInstance == null) {  
               mInstance = create();  
           }  
           return mInstance;  
       }  
   }  
 }    
   @UnsupportedAppUsage  
   private static final Singleton<IActivityManager> IActivityManagerSingleton =  
           new Singleton<IActivityManager>() {  
               @Override  
               protected IActivityManager create() {  
                   final IBinder b = ServiceManager.getService(Context.ACTIVITY_SERVICE);  
                   //获取AMS的代理  
                   final IActivityManager am = IActivityManager.Stub.asInterface(b);  
                   return am;  
               }  
           };
```


### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-4-IActivityManager-startActivity "2.4  IActivityManager.startActivity")2.4 IActivityManager.startActivity

通过AIDL生成的代理类调用AMS的startActivity，其代理类在编译的时候，会自动生成。

startActivity共有10个参数，参数对应值如下：

-   caller：当前应用的Application对象mAppThread
-   callingPackage:当前Activity所在的包名
-   intent：启动Activity传过来的参数
-   resolvedType：调用intent.resolveTypeIfNeeded获取
-   resultTo：来自当前Activity.mToken
-   resultWho： 来自当前Activity.mEmbeddedID
-   requestCode:-1
-   startFlags:0
-   profilerInfo：null
-   bOptions:null

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-5-AMS-startActivity "2.5 AMS.startActivity")2.5 AMS.startActivity

\[->ActivityManagerService.java\]

```
@Override  
 public final int startActivity(IApplicationThread caller, String callingPackage,  
         Intent intent, String resolvedType, IBinder resultTo, String resultWho, int requestCode,  
         int startFlags, ProfilerInfo profilerInfo, Bundle bOptions) {  
     return startActivityAsUser(caller, callingPackage, intent, resolvedType, resultTo,  
             resultWho, requestCode, startFlags, profilerInfo, bOptions,  
             UserHandle.getCallingUserId());  
 }  
  @Override  
 public final int startActivityAsUser(IApplicationThread caller, String callingPackage,  
         Intent intent, String resolvedType, IBinder resultTo, String resultWho, int requestCode,  
         int startFlags, ProfilerInfo profilerInfo, Bundle bOptions, int userId) {  
     return startActivityAsUser(caller, callingPackage, intent, resolvedType, resultTo,  
             resultWho, requestCode, startFlags, profilerInfo, bOptions, userId,  
             true /*validateIncomingUser*/);  
 }
```


### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-6-AMS-startActivityAsUser "2.6 AMS.startActivityAsUser")2.6 AMS.startActivityAsUser

\[->ActivityManagerService.java\]

```
public final int startActivityAsUser(IApplicationThread caller, String callingPackage,  
           Intent intent, String resolvedType, IBinder resultTo, String resultWho, int requestCode,  
           int startFlags, ProfilerInfo profilerInfo, Bundle bOptions, int userId,  
           boolean validateIncomingUser) {  
       enforceNotIsolatedCaller("startActivity");  
  
       userId = mActivityStartController.checkTargetUser(userId, validateIncomingUser,  
               Binder.getCallingPid(), Binder.getCallingUid(), "startActivityAsUser");  
  
       // TODO: Switch to user app stacks here.  
       return mActivityStartController.obtainStarter(intent, "startActivityAsUser")  
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

通过建造者模式，来设置参数，其参数在2.4节有介绍，通过execute方法最后执行。

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-6-1-ASC-obtainStarter "2.6.1  ASC.obtainStarter")2.6.1 ASC.obtainStarter

\[->ActivityStartController.java\]

```
/**  
    * @return A starter to configure and execute starting an activity. It is valid until after  
    *         {@link ActivityStarter#execute} is invoked. At that point, the starter should be  
    *         considered invalid and no longer modified or used.  
    */  
   ActivityStarter obtainStarter(Intent intent, String reason) {  
       return mFactory.obtain().setIntent(intent).setReason(reason);  
   }
```



#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-6-2-AS-setMayWait "2.6.2  AS.setMayWait")2.6.2 AS.setMayWait

这个方法在2.6.3节中用到，可以看到这里将mRequest.mayWait设置为true

```
ActivityStarter setMayWait(int userId) {  
     mRequest.mayWait = true;  
     mRequest.userId = userId;  
     return this;  
 }
```
#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-6-3-AS-execute "2.6.3  AS.execute")2.6.3 AS.execute

\[->ActivityStarter.java\]


```
/**  
    * Starts an activity based on the request parameters provided earlier.  
    * @return The starter result.  
    */  
   int execute() {  
       try {  
           // TODO(b/64750076): Look into passing request directly to these methods to allow  
           // for transactional diffs and preprocessing.  
           //经过该方法，前面已经设置为true  
           if (mRequest.mayWait) {  
               return startActivityMayWait(mRequest.caller, mRequest.callingUid,  
                       mRequest.callingPackage, mRequest.intent, mRequest.resolvedType,  
                       mRequest.voiceSession, mRequest.voiceInteractor, mRequest.resultTo,  
                       mRequest.resultWho, mRequest.requestCode, mRequest.startFlags,  
                       mRequest.profilerInfo, mRequest.waitResult, mRequest.globalConfig,  
                       mRequest.activityOptions, mRequest.ignoreTargetSecurity, mRequest.userId,  
                       mRequest.inTask, mRequest.reason,  
                       mRequest.allowPendingRemoteAnimationRegistryLookup,  
                       mRequest.originatingPendingIntent);  
           } else {  
               return startActivity(mRequest.caller, mRequest.intent, mRequest.ephemeralIntent,  
                       mRequest.resolvedType, mRequest.activityInfo, mRequest.resolveInfo,  
                       mRequest.voiceSession, mRequest.voiceInteractor, mRequest.resultTo,  
                       mRequest.resultWho, mRequest.requestCode, mRequest.callingPid,  
                       mRequest.callingUid, mRequest.callingPackage, mRequest.realCallingPid,  
                       mRequest.realCallingUid, mRequest.startFlags, mRequest.activityOptions,  
                       mRequest.ignoreTargetSecurity, mRequest.componentSpecified,  
                       mRequest.outActivity, mRequest.inTask, mRequest.reason,  
                       mRequest.allowPendingRemoteAnimationRegistryLookup,  
                       mRequest.originatingPendingIntent);  
           }  
       } finally {  
           onExecutionComplete();  
       }  
   }
```
### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-7-AS-startActivityMayWait "2.7  AS.startActivityMayWait")2.7 AS.startActivityMayWait

这个方法的参数有21个，具体重要的几个参数2.4节已经介绍过, inTask = null。

\[->ActivityStarter.java\]

```
private int startActivityMayWait(IApplicationThread caller, int callingUid,  
           String callingPackage, Intent intent, String resolvedType,  
           IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,  
           IBinder resultTo, String resultWho, int requestCode, int startFlags,  
           ProfilerInfo profilerInfo, WaitResult outResult,  
           Configuration globalConfig, SafeActivityOptions options, boolean ignoreTargetSecurity,  
           int userId, TaskRecord inTask, String reason,  
           boolean allowPendingRemoteAnimationRegistryLookup,  
           PendingIntentRecord originatingPendingIntent) {  
       // Refuse possible leaked file descriptors  
       if (intent != null && intent.hasFileDescriptors()) {  
           throw new IllegalArgumentException("File descriptors passed in Intent");  
       }  
       //activity开始启动日志  
       mSupervisor.getActivityMetricsLogger().notifyActivityLaunching();  
       boolean componentSpecified = intent.getComponent() != null;  
  
       final int realCallingPid = Binder.getCallingPid();  
       final int realCallingUid = Binder.getCallingUid();  
  
       int callingPid;  
       if (callingUid >= 0) {  
           callingPid = -1;  
       } else if (caller == null) {  
           callingPid = realCallingPid;  
           callingUid = realCallingUid;  
       } else {  
           callingPid = callingUid = -1;  
       }  
  
       // Save a copy in case ephemeral needs it  
       final Intent ephemeralIntent = new Intent(intent);  
       // Don't modify the client's object!  
       intent = new Intent(intent);  
       //对一些特殊的intent做处理  
       if (componentSpecified  
               && !(Intent.ACTION_VIEW.equals(intent.getAction()) && intent.getData() == null)  
               && !Intent.ACTION_INSTALL_INSTANT_APP_PACKAGE.equals(intent.getAction())  
               && !Intent.ACTION_RESOLVE_INSTANT_APP_PACKAGE.equals(intent.getAction())  
               && mService.getPackageManagerInternalLocked()  
                       .isInstantAppInstallerComponent(intent.getComponent())) {  
           // intercept intents targeted directly to the ephemeral installer the  
           // ephemeral installer should never be started with a raw Intent; instead  
           // adjust the intent so it looks like a "normal" instant app launch  
           intent.setComponent(null /*component*/);  
           componentSpecified = false;  
       }  
       //处理intent信息，当存在多个activity时，弹出resolverAcitvity  
       ResolveInfo rInfo = mSupervisor.resolveIntent(intent, resolvedType, userId,  
               0 /* matchFlags */,  
                       computeResolveFilterUid(  
                               callingUid, realCallingUid, mRequest.filterCallingUid));  
       if (rInfo == null) {  
           UserInfo userInfo = mSupervisor.getUserInfo(userId);  
           if (userInfo != null && userInfo.isManagedProfile()) {  
               // Special case for managed profiles, if attempting to launch non-cryto aware  
               // app in a locked managed profile from an unlocked parent allow it to resolve  
               // as user will be sent via confirm credentials to unlock the profile.  
               UserManager userManager = UserManager.get(mService.mContext);  
               boolean profileLockedAndParentUnlockingOrUnlocked = false;  
               long token = Binder.clearCallingIdentity();  
               try {  
                   UserInfo parent = userManager.getProfileParent(userId);  
                   profileLockedAndParentUnlockingOrUnlocked = (parent != null)  
                           && userManager.isUserUnlockingOrUnlocked(parent.id)  
                           && !userManager.isUserUnlockingOrUnlocked(userId);  
               } finally {  
                   Binder.restoreCallingIdentity(token);  
               }  
               if (profileLockedAndParentUnlockingOrUnlocked) {  
                   rInfo = mSupervisor.resolveIntent(intent, resolvedType, userId,  
                           PackageManager.MATCH_DIRECT_BOOT_AWARE  
                                   | PackageManager.MATCH_DIRECT_BOOT_UNAWARE,  
                           computeResolveFilterUid(  
                                   callingUid, realCallingUid, mRequest.filterCallingUid));  
               }  
           }  
       }  
       //收集intent所指向的activity信息  
       // Collect information about the target of the Intent.  
       ActivityInfo aInfo = mSupervisor.resolveActivity(intent, rInfo, startFlags, profilerInfo);  
  
       synchronized (mService) {  
           final ActivityStack stack = mSupervisor.mFocusedStack;  
           stack.mConfigWillChange = globalConfig != null  
                   && mService.getGlobalConfiguration().diff(globalConfig) != 0;  
           if (DEBUG_CONFIGURATION) Slog.v(TAG_CONFIGURATION,  
                   "Starting activity when config will change = " + stack.mConfigWillChange);  
  
           final long origId = Binder.clearCallingIdentity();  
  
           if (aInfo != null &&  
                   (aInfo.applicationInfo.privateFlags  
                           & ApplicationInfo.PRIVATE_FLAG_CANT_SAVE_STATE) != 0 &&  
                   mService.mHasHeavyWeightFeature) {  
               //heavy-weight进程处理流程  
               // This may be a heavy-weight process!  Check to see if we already  
               // have another, different heavy-weight process running.  
               if (aInfo.processName.equals(aInfo.applicationInfo.packageName)) {  
                   final ProcessRecord heavy = mService.mHeavyWeightProcess;  
                   if (heavy != null && (heavy.info.uid != aInfo.applicationInfo.uid  
                           || !heavy.processName.equals(aInfo.processName))) {  
                       int appCallingUid = callingUid;  
                       if (caller != null) {  
                           ProcessRecord callerApp = mService.getRecordForAppLocked(caller);  
                           if (callerApp != null) {  
                               appCallingUid = callerApp.info.uid;  
                           } else {  
                               Slog.w(TAG, "Unable to find app for caller " + caller  
                                       + " (pid=" + callingPid + ") when starting: "  
                                       + intent.toString());  
                               SafeActivityOptions.abort(options);  
                               return ActivityManager.START_PERMISSION_DENIED;  
                           }  
                       }  
  
                       IIntentSender target = mService.getIntentSenderLocked(  
                               ActivityManager.INTENT_SENDER_ACTIVITY, "android",  
                               appCallingUid, userId, null, null, 0, new Intent[] { intent },  
                               new String[] { resolvedType }, PendingIntent.FLAG_CANCEL_CURRENT  
                                       | PendingIntent.FLAG_ONE_SHOT, null);  
  
                       Intent newIntent = new Intent();  
                       if (requestCode >= 0) {  
                           // Caller is requesting a result.  
                           newIntent.putExtra(HeavyWeightSwitcherActivity.KEY_HAS_RESULT, true);  
                       }  
                       newIntent.putExtra(HeavyWeightSwitcherActivity.KEY_INTENT,  
                               new IntentSender(target));  
                       if (heavy.activities.size() > 0) {  
                           ActivityRecord hist = heavy.activities.get(0);  
                           newIntent.putExtra(HeavyWeightSwitcherActivity.KEY_CUR_APP,  
                                   hist.packageName);  
                           newIntent.putExtra(HeavyWeightSwitcherActivity.KEY_CUR_TASK,  
                                   hist.getTask().taskId);  
                       }  
                       newIntent.putExtra(HeavyWeightSwitcherActivity.KEY_NEW_APP,  
                               aInfo.packageName);  
                       newIntent.setFlags(intent.getFlags());  
                       newIntent.setClassName("android",  
                               HeavyWeightSwitcherActivity.class.getName());  
                       intent = newIntent;  
                       resolvedType = null;  
                       caller = null;  
                       callingUid = Binder.getCallingUid();  
                       callingPid = Binder.getCallingPid();  
                       componentSpecified = true;  
                       rInfo = mSupervisor.resolveIntent(intent, null /*resolvedType*/, userId,  
                               0 /* matchFlags */, computeResolveFilterUid(  
                                       callingUid, realCallingUid, mRequest.filterCallingUid));  
                       aInfo = rInfo != null ? rInfo.activityInfo : null;  
                       if (aInfo != null) {  
                           aInfo = mService.getActivityInfoForUser(aInfo, userId);  
                       }  
                   }  
               }  
           }  
  
           final ActivityRecord[] outRecord = new ActivityRecord[1];  
           //见2.8节  
           int res = startActivity(caller, intent, ephemeralIntent, resolvedType, aInfo, rInfo,  
                   voiceSession, voiceInteractor, resultTo, resultWho, requestCode, callingPid,  
                   callingUid, callingPackage, realCallingPid, realCallingUid, startFlags, options,  
                   ignoreTargetSecurity, componentSpecified, outRecord, inTask, reason,  
                   allowPendingRemoteAnimationRegistryLookup, originatingPendingIntent);  
  
           Binder.restoreCallingIdentity(origId);  
  
           if (stack.mConfigWillChange) {  
               // If the caller also wants to switch to a new configuration,  
               // do so now.  This allows a clean switch, as we are waiting  
               // for the current activity to pause (so we will not destroy  
               // it), and have not yet started the next activity.  
               mService.enforceCallingPermission(android.Manifest.permission.CHANGE_CONFIGURATION,  
                       "updateConfiguration()");  
               stack.mConfigWillChange = false;  
               if (DEBUG_CONFIGURATION) Slog.v(TAG_CONFIGURATION,  
                       "Updating to new configuration after starting activity.");  
               mService.updateConfigurationLocked(globalConfig, null, false);  
           }  
  
           // Notify ActivityMetricsLogger that the activity has launched. ActivityMetricsLogger  
           // will then wait for the windows to be drawn and populate WaitResult.  
           mSupervisor.getActivityMetricsLogger().notifyActivityLaunched(res, outRecord[0]);  
           if (outResult != null) {  
               outResult.result = res;  
  
               final ActivityRecord r = outRecord[0];  
  
               switch(res) {  
                   case START_SUCCESS: {  
                       mSupervisor.mWaitingActivityLaunched.add(outResult);  
                       do {  
                           try {  
                               mService.wait();  
                           } catch (InterruptedException e) {  
                           }  
                       } while (outResult.result != START_TASK_TO_FRONT  
                               && !outResult.timeout && outResult.who == null);  
                       if (outResult.result == START_TASK_TO_FRONT) {  
                           res = START_TASK_TO_FRONT;  
                       }  
                       break;  
                   }  
                   case START_DELIVERED_TO_TOP: {  
                       outResult.timeout = false;  
                       outResult.who = r.realActivity;  
                       outResult.totalTime = 0;  
                       break;  
                   }  
                   case START_TASK_TO_FRONT: {  
                       // ActivityRecord may represent a different activity, but it should not be  
                       // in the resumed state.  
                       if (r.nowVisible && r.isState(RESUMED)) {  
                           outResult.timeout = false;  
                           outResult.who = r.realActivity;  
                           outResult.totalTime = 0;  
                       } else {  
                           final long startTimeMs = SystemClock.uptimeMillis();  
                           mSupervisor.waitActivityVisible(r.realActivity, outResult, startTimeMs);  
                           // Note: the timeout variable is not currently not ever set.  
                           do {  
                               try {  
                                   mService.wait();  
                               } catch (InterruptedException e) {  
                               }  
                           } while (!outResult.timeout && outResult.who == null);  
                       }  
                       break;  
                   }  
               }  
           }  
  
           return res;  
       }  
   }
```



#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-7-1-PKMS-resolveIntent "2.7.1 PKMS.resolveIntent")2.7.1 PKMS.resolveIntent

\[->PackageManagerService.java\]

mSupervisor.resolveInten经过层层调用，通过IPC最后会调用PKMS对象中的resolveIntent。

```
/**  
    * Normally instant apps can only be resolved when they're visible to the caller.  
    * However, if {@code resolveForStart} is {@code true}, all instant apps are visible  
    * since we need to allow the system to start any installed application.  
    */  
   private ResolveInfo resolveIntentInternal(Intent intent, String resolvedType,  
           int flags, int userId, boolean resolveForStart, int filterCallingUid) {  
       try {  
           Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "resolveIntent");  
  
           if (!sUserManager.exists(userId)) return null;  
           final int callingUid = Binder.getCallingUid();  
           flags = updateFlagsForResolve(flags, userId, intent, filterCallingUid, resolveForStart);  
           mPermissionManager.enforceCrossUserPermission(callingUid, userId,  
                   false /*requireFullPermission*/, false /*checkShell*/, "resolve intent");  
  
           Trace.traceBegin(TRACE_TAG_PACKAGE_MANAGER, "queryIntentActivities");  
           //找到相应的activity组件，并保存intent对象  
           final List<ResolveInfo> query = queryIntentActivitiesInternal(intent, resolvedType,  
                   flags, filterCallingUid, userId, resolveForStart, true /*allowDynamicSplits*/);  
           Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);  
           //根据priority选择最佳的activity  
           final ResolveInfo bestChoice =  
                   chooseBestActivity(intent, resolvedType, flags, query, userId);  
           return bestChoice;  
       } finally {  
           Trace.traceEnd(TRACE_TAG_PACKAGE_MANAGER);  
       }  
   }
```



#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-7-2-ASS-resolveActivity "2.7.2 ASS.resolveActivity")2.7.2 ASS.resolveActivity

\[->ActivityStackSupervisor.java\]

```
ActivityInfo resolveActivity(Intent intent, ResolveInfo rInfo, int startFlags,  
           ProfilerInfo profilerInfo) {  
       final ActivityInfo aInfo = rInfo != null ? rInfo.activityInfo : null;  
       if (aInfo != null) {  
           // Store the found target back into the intent, because now that  
           // we have it we never want to do this again.  For example, if the  
           // user navigates back to this point in the history, we should  
           // always restart the exact same activity.  
           intent.setComponent(new ComponentName(  
                   aInfo.applicationInfo.packageName, aInfo.name));  
  
           // Don't debug things in the system process  
           if (!aInfo.processName.equals("system")) {  
               if ((startFlags & ActivityManager.START_FLAG_DEBUG) != 0) {  
                   mService.setDebugApp(aInfo.processName, true, false);  
               }  
  
               if ((startFlags & ActivityManager.START_FLAG_NATIVE_DEBUGGING) != 0) {  
                   mService.setNativeDebuggingAppLocked(aInfo.applicationInfo, aInfo.processName);  
               }  
  
               if ((startFlags & ActivityManager.START_FLAG_TRACK_ALLOCATION) != 0) {  
                   mService.setTrackAllocationApp(aInfo.applicationInfo, aInfo.processName);  
               }  
  
               if (profilerInfo != null) {  
                   mService.setProfileApp(aInfo.applicationInfo, aInfo.processName, profilerInfo);  
               }  
           }  
           final String intentLaunchToken = intent.getLaunchToken();  
           if (aInfo.launchToken == null && intentLaunchToken != null) {  
               aInfo.launchToken = intentLaunchToken;  
           }  
       }  
       return aInfo;  
   }
```


Activity类有3个flags用于调试

-   START\_FLAG\_DEBUG：用于调试debug app
-   START\_FLAG\_NATIVE\_DEBUGGING：用于调试native
-   START\_FLAG\_TRACK\_ALLOCATION：用于调试allocation tracking

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-8-AS-startActivity "2.8  AS.startActivity")2.8 AS.startActivity

\[->ActivityStarter.java\]

```
private int startActivity(IApplicationThread caller, Intent intent, Intent ephemeralIntent,  
          String resolvedType, ActivityInfo aInfo, ResolveInfo rInfo,  
          IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,  
          IBinder resultTo, String resultWho, int requestCode, int callingPid, int callingUid,  
          String callingPackage, int realCallingPid, int realCallingUid, int startFlags,  
          SafeActivityOptions options, boolean ignoreTargetSecurity, boolean componentSpecified,  
          ActivityRecord[] outActivity, TaskRecord inTask, String reason,  
          boolean allowPendingRemoteAnimationRegistryLookup,  
          PendingIntentRecord originatingPendingIntent) {  
  
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
              inTask, allowPendingRemoteAnimationRegistryLookup, originatingPendingIntent);  
  
      if (outActivity != null) {  
          // mLastStartActivityRecord[0] is set in the call to startActivity above.  
          outActivity[0] = mLastStartActivityRecord[0];  
      }  
  
      return getExternalResult(mLastStartActivityResult);  
  }
```



下面才正式进入startActivity具体内容

```
private int startActivity(IApplicationThread caller, Intent intent, Intent ephemeralIntent,  
           String resolvedType, ActivityInfo aInfo, ResolveInfo rInfo,  
           IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,  
           IBinder resultTo, String resultWho, int requestCode, int callingPid, int callingUid,  
           String callingPackage, int realCallingPid, int realCallingUid, int startFlags,  
           SafeActivityOptions options,  
           boolean ignoreTargetSecurity, boolean componentSpecified, ActivityRecord[] outActivity,  
           TaskRecord inTask, boolean allowPendingRemoteAnimationRegistryLookup,  
           PendingIntentRecord originatingPendingIntent) {  
       int err = ActivityManager.START_SUCCESS;  
       // Pull the optional Ephemeral Installer-only bundle out of the options early.  
       final Bundle verificationBundle  
               = options != null ? options.popAppVerificationBundle() : null;  
       //获取调用者的进程记录对象  
       ProcessRecord callerApp = null;  
       if (caller != null) {  
           callerApp = mService.getRecordForAppLocked(caller);  
           if (callerApp != null) {  
               callingPid = callerApp.pid;  
               callingUid = callerApp.info.uid;  
           } else {  
               Slog.w(TAG, "Unable to find app for caller " + caller  
                       + " (pid=" + callingPid + ") when starting: "  
                       + intent.toString());  
               err = ActivityManager.START_PERMISSION_DENIED;  
           }  
       }  
  
       final int userId = aInfo != null && aInfo.applicationInfo != null  
               ? UserHandle.getUserId(aInfo.applicationInfo.uid) : 0;  
  
       if (err == ActivityManager.START_SUCCESS) {  
           Slog.i(TAG, "START u" + userId + " {" + intent.toShortString(true, true, true, false)  
                   + "} from uid " + callingUid);  
       }  
  
       //获取调用者所在的activity  
       ActivityRecord sourceRecord = null;  
       ActivityRecord resultRecord = null;  
       if (resultTo != null) {  
           sourceRecord = mSupervisor.isInAnyStackLocked(resultTo);  
           if (DEBUG_RESULTS) Slog.v(TAG_RESULTS,  
                   "Will send result to " + resultTo + " " + sourceRecord);  
           if (sourceRecord != null) {  
               //requestCode = -1 不会进入  
               if (requestCode >= 0 && !sourceRecord.finishing) {  
                   resultRecord = sourceRecord;  
               }  
           }  
       }  
  
       final int launchFlags = intent.getFlags();  
  
       if ((launchFlags & Intent.FLAG_ACTIVITY_FORWARD_RESULT) != 0 && sourceRecord != null) {  
           //activity执行结果的返回由源activity切换到新activity，不需要返回结果则不会进该分支    
           // Transfer the result target from the source activity to the new  
           // one being started, including any failures.  
           if (requestCode >= 0) {  
               SafeActivityOptions.abort(options);  
               return ActivityManager.START_FORWARD_AND_REQUEST_CONFLICT;  
           }  
           resultRecord = sourceRecord.resultTo;  
           if (resultRecord != null && !resultRecord.isInStackLocked()) {  
               resultRecord = null;  
           }  
           resultWho = sourceRecord.resultWho;  
           requestCode = sourceRecord.requestCode;  
           sourceRecord.resultTo = null;  
           if (resultRecord != null) {  
               resultRecord.removeResultsLocked(sourceRecord, resultWho, requestCode);  
           }  
           if (sourceRecord.launchedFromUid == callingUid) {  
               // The new activity is being launched from the same uid as the previous  
               // activity in the flow, and asking to forward its result back to the  
               // previous.  In this case the activity is serving as a trampoline between  
               // the two, so we also want to update its launchedFromPackage to be the  
               // same as the previous activity.  Note that this is safe, since we know  
               // these two packages come from the same uid; the caller could just as  
               // well have supplied that same package name itself.  This specifially  
               // deals with the case of an intent picker/chooser being launched in the app  
               // flow to redirect to an activity picked by the user, where we want the final  
               // activity to consider it to have been launched by the previous app activity.  
               callingPackage = sourceRecord.launchedFromPackage;  
           }  
       }  
   
       if (err == ActivityManager.START_SUCCESS && intent.getComponent() == null) {  
           //从intent中无法找到相应的component  
           // We couldn't find a class that can handle the given Intent.  
           // That's the end of that!  
           err = ActivityManager.START_INTENT_NOT_RESOLVED;  
       }  
  
       if (err == ActivityManager.START_SUCCESS && aInfo == null) {  
           //从intent中无法找到相应的ActivityInfo  
           // We couldn't find the specific class specified in the Intent.  
           // Also the end of the line.  
           err = ActivityManager.START_CLASS_NOT_FOUND;  
       }  
  
       if (err == ActivityManager.START_SUCCESS && sourceRecord != null  
               && sourceRecord.getTask().voiceSession != null) {  
            //启动的activity是voice session一部分  
           // If this activity is being launched as part of a voice session, we need  
           // to ensure that it is safe to do so.  If the upcoming activity will also  
           // be part of the voice session, we can only launch it if it has explicitly  
           // said it supports the VOICE category, or it is a part of the calling app.  
           if ((launchFlags & FLAG_ACTIVITY_NEW_TASK) == 0  
                   && sourceRecord.info.applicationInfo.uid != aInfo.applicationInfo.uid) {  
               try {  
                   intent.addCategory(Intent.CATEGORY_VOICE);  
                   if (!mService.getPackageManager().activitySupportsIntent(  
                           intent.getComponent(), intent, resolvedType)) {  
                       Slog.w(TAG,  
                               "Activity being started in current voice task does not support voice: "  
                                       + intent);  
                       err = ActivityManager.START_NOT_VOICE_COMPATIBLE;  
                   }  
               } catch (RemoteException e) {  
                   Slog.w(TAG, "Failure checking voice capabilities", e);  
                   err = ActivityManager.START_NOT_VOICE_COMPATIBLE;  
               }  
           }  
       }  
  
       if (err == ActivityManager.START_SUCCESS && voiceSession != null) {  
           //启动是是voice session  
           // If the caller is starting a new voice session, just make sure the target  
           // is actually allowing it to run this way.  
           try {  
               if (!mService.getPackageManager().activitySupportsIntent(intent.getComponent(),  
                       intent, resolvedType)) {  
                   Slog.w(TAG,  
                           "Activity being started in new voice task does not support: "  
                                   + intent);  
                   err = ActivityManager.START_NOT_VOICE_COMPATIBLE;  
               }  
           } catch (RemoteException e) {  
               Slog.w(TAG, "Failure checking voice capabilities", e);  
               err = ActivityManager.START_NOT_VOICE_COMPATIBLE;  
           }  
       }  
  
       final ActivityStack resultStack = resultRecord == null ? null : resultRecord.getStack();  
       //错误则返回  
       if (err != START_SUCCESS) {  
           if (resultRecord != null) {  
               resultStack.sendActivityResultLocked(  
                       -1, resultRecord, resultWho, requestCode, RESULT_CANCELED, null);  
           }  
           SafeActivityOptions.abort(options);  
           return err;  
       }  
  
       //检查权限  
       boolean abort = !mSupervisor.checkStartAnyActivityPermission(intent, aInfo, resultWho,  
               requestCode, callingPid, callingUid, callingPackage, ignoreTargetSecurity,  
               inTask != null, callerApp, resultRecord, resultStack);  
       abort |= !mService.mIntentFirewall.checkStartActivity(intent, callingUid,  
               callingPid, resolvedType, aInfo.applicationInfo);  
  
       // Merge the two options bundles, while realCallerOptions takes precedence.  
       ActivityOptions checkedOptions = options != null  
               ? options.getOptions(intent, aInfo, callerApp, mSupervisor)  
               : null;  
       if (allowPendingRemoteAnimationRegistryLookup) {  
           checkedOptions = mService.getActivityStartController()  
                   .getPendingRemoteAnimationRegistry()  
                   .overrideOptionsIfNeeded(callingPackage, checkedOptions);  
       }  
       //ActivityController不为空的情况，比如monkey测试过程  
       if (mService.mController != null) {  
           try {  
               // The Intent we give to the watcher has the extra data  
               // stripped off, since it can contain private information.  
               Intent watchIntent = intent.cloneFilter();  
               abort |= !mService.mController.activityStarting(watchIntent,  
                       aInfo.applicationInfo.packageName);  
           } catch (RemoteException e) {  
               mService.mController = null;  
           }  
       }  
  
       mInterceptor.setStates(userId, realCallingPid, realCallingUid, startFlags, callingPackage);  
       if (mInterceptor.intercept(intent, rInfo, aInfo, resolvedType, inTask, callingPid,  
               callingUid, checkedOptions)) {  
           // activity被拦截  
           // activity start was intercepted, e.g. because the target user is currently in quiet  
           // mode (turn off work) or the target application is suspended  
           intent = mInterceptor.mIntent;  
           rInfo = mInterceptor.mRInfo;  
           aInfo = mInterceptor.mAInfo;  
           resolvedType = mInterceptor.mResolvedType;  
           inTask = mInterceptor.mInTask;  
           callingPid = mInterceptor.mCallingPid;  
           callingUid = mInterceptor.mCallingUid;  
           checkedOptions = mInterceptor.mActivityOptions;  
       }  
  
       //终止则返回  
       if (abort) {  
           if (resultRecord != null) {  
               resultStack.sendActivityResultLocked(-1, resultRecord, resultWho, requestCode,  
                       RESULT_CANCELED, null);  
           }  
           // We pretend to the caller that it was really started, but  
           // they will just get a cancel result.  
           ActivityOptions.abort(checkedOptions);  
           return START_ABORTED;  
       }  
       //如果需要再检查权限，则启动检查activity  
       // If permissions need a review before any of the app components can run, we  
       // launch the review activity and pass a pending intent to start the activity  
       // we are to launching now after the review is completed.  
       if (mService.mPermissionReviewRequired && aInfo != null) {  
           if (mService.getPackageManagerInternalLocked().isPermissionsReviewRequired(  
                   aInfo.packageName, userId)) {  
               IIntentSender target = mService.getIntentSenderLocked(  
                       ActivityManager.INTENT_SENDER_ACTIVITY, callingPackage,  
                       callingUid, userId, null, null, 0, new Intent[]{intent},  
                       new String[]{resolvedType}, PendingIntent.FLAG_CANCEL_CURRENT  
                               | PendingIntent.FLAG_ONE_SHOT, null);  
  
               final int flags = intent.getFlags();  
               Intent newIntent = new Intent(Intent.ACTION_REVIEW_PERMISSIONS);  
               newIntent.setFlags(flags  
                       | Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);  
               newIntent.putExtra(Intent.EXTRA_PACKAGE_NAME, aInfo.packageName);  
               newIntent.putExtra(Intent.EXTRA_INTENT, new IntentSender(target));  
               if (resultRecord != null) {  
                   newIntent.putExtra(Intent.EXTRA_RESULT_NEEDED, true);  
               }  
               intent = newIntent;  
  
               resolvedType = null;  
               callingUid = realCallingUid;  
               callingPid = realCallingPid;  
  
               rInfo = mSupervisor.resolveIntent(intent, resolvedType, userId, 0,  
                       computeResolveFilterUid(  
                               callingUid, realCallingUid, mRequest.filterCallingUid));  
               aInfo = mSupervisor.resolveActivity(intent, rInfo, startFlags,  
                       null /*profilerInfo*/);  
  
               if (DEBUG_PERMISSIONS_REVIEW) {  
                   Slog.i(TAG, "START u" + userId + " {" + intent.toShortString(true, true,  
                           true, false) + "} from uid " + callingUid + " on display "  
                           + (mSupervisor.mFocusedStack == null  
                           ? DEFAULT_DISPLAY : mSupervisor.mFocusedStack.mDisplayId));  
               }  
           }  
       }  
  
       // If we have an ephemeral app, abort the process of launching the resolved intent.  
       // Instead, launch the ephemeral installer. Once the installer is finished, it  
       // starts either the intent we resolved here [on install error] or the ephemeral  
       // app [on install success].  
       if (rInfo != null && rInfo.auxiliaryInfo != null) {  
           intent = createLaunchIntent(rInfo.auxiliaryInfo, ephemeralIntent,  
                   callingPackage, verificationBundle, resolvedType, userId);  
           resolvedType = null;  
           callingUid = realCallingUid;  
           callingPid = realCallingPid;  
  
           aInfo = mSupervisor.resolveActivity(intent, rInfo, startFlags, null /*profilerInfo*/);  
       }  
  
       //创建activity记录对象  
       ActivityRecord r = new ActivityRecord(mService, callerApp, callingPid, callingUid,  
               callingPackage, intent, resolvedType, aInfo, mService.getGlobalConfiguration(),  
               resultRecord, resultWho, requestCode, componentSpecified, voiceSession != null,  
               mSupervisor, checkedOptions, sourceRecord);  
       if (outActivity != null) {  
           outActivity[0] = r;  
       }  
  
       if (r.appTimeTracker == null && sourceRecord != null) {  
           // If the caller didn't specify an explicit time tracker, we want to continue  
           // tracking under any it has.  
           r.appTimeTracker = sourceRecord.appTimeTracker;  
       }  
  
       final ActivityStack stack = mSupervisor.mFocusedStack;  
  
        
       // If we are starting an activity that is not from the same uid as the currently resumed  
       // one, check whether app switches are allowed.  
       if (voiceSession == null && (stack.getResumedActivity() == null  
               || stack.getResumedActivity().info.applicationInfo.uid != realCallingUid)) {  
           //如果前台stack还没有resume状态的activity，则检查app是否允许切换，见2.8.1  
           if (!mService.checkAppSwitchAllowedLocked(callingPid, callingUid,  
                   realCallingPid, realCallingUid, "Activity start")) {  
                //如果不允许切换，则把要启动的activity添加到PendingActivity，并且返回  
               mController.addPendingActivityLaunch(new PendingActivityLaunch(r,  
                       sourceRecord, startFlags, stack, callerApp));  
               ActivityOptions.abort(checkedOptions);  
               return ActivityManager.START_SWITCHES_CANCELED;  
           }  
       }  
          
       if (mService.mDidAppSwitch) {  
           //从第一次app切换到第二次允许app,允许切换时间设置为0，则表示可以任意切换app  
           // This is the second allowed switch since we stopped switches,  
           // so now just generally allow switches.  Use case: user presses  
           // home (switches disabled, switch to home, mDidAppSwitch now true);  
           // user taps a home icon (coming from home so allowed, we hit here  
           // and now allow anyone to switch again).  
           mService.mAppSwitchesAllowedTime = 0;  
       } else {  
           mService.mDidAppSwitch = true;  
       }  
  
       //处理PendingActivity的启动，由于app switch禁用从而被hold的等待的activity，见2.8.2  
       mController.doPendingActivityLaunches(false);  
  
       maybeLogActivityStart(callingUid, callingPackage, realCallingUid, intent, callerApp, r,  
               originatingPendingIntent);  
       //再走startActivity  
       return startActivity(r, sourceRecord, voiceSession, voiceInteractor, startFlags,  
               true /* doResume */, checkedOptions, inTask, outActivity);  
   }
```

在上面这三个返回值表示启动activity失败

START\_INTENT\_NOT\_RESOLVED：从intent中无法找到相应的component  
START\_CLASS\_NOT\_FOUND ：从intent中无法找到相应的ActivityInfo  
START\_NOT\_VOICE\_COMPATIBLE :不支持voice task

```
private int startActivity(final ActivityRecord r, ActivityRecord sourceRecord,  
                IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,  
                int startFlags, boolean doResume, ActivityOptions options, TaskRecord inTask,  
                ActivityRecord[] outActivity) {  
        int result = START_CANCELED;  
        try {  
            mService.mWindowManager.deferSurfaceLayout();  
            //见2.9节  
            result = startActivityUnchecked(r, sourceRecord, voiceSession, voiceInteractor,  
                    startFlags, doResume, options, inTask, outActivity);  
        } finally {  
            //不能启动则取消task关联  
            // If we are not able to proceed, disassociate the activity from the task. Leaving an  
            // activity in an incomplete state can lead to issues, such as performing operations  
            // without a window container.  
            final ActivityStack stack = mStartActivity.getStack();  
            if (!ActivityManager.isStartResultSuccessful(result) && stack != null) {  
                stack.finishActivityLocked(mStartActivity, RESULT_CANCELED,  
                        null /* intentResultData */, "startActivity", true /* oomAdj */);  
            }  
            mService.mWindowManager.continueSurfaceLayout();  
        }  
        postStartActivityProcessing(r, result, mTargetStack);  
        return result;  
    }
```

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-8-1-AMS-checkAppSwitchAllowedLocked "2.8.1 AMS.checkAppSwitchAllowedLocked")2.8.1 AMS.checkAppSwitchAllowedLocked

\[->ActivityManagerService.java\]

```
boolean checkAppSwitchAllowedLocked(int sourcePid, int sourceUid,  
          int callingPid, int callingUid, String name) {  
      if (mAppSwitchesAllowedTime < SystemClock.uptimeMillis()) {  
          return true;  
      }  
      if (mRecentTasks.isCallerRecents(sourceUid)) {  
          return true;  
      }  
      int perm = checkComponentPermission(STOP_APP_SWITCHES, sourcePid, sourceUid, -1, true);  
      if (perm == PackageManager.PERMISSION_GRANTED) {  
          return true;  
      }  
      if (checkAllowAppSwitchUid(sourceUid)) {  
          return true;  
      }  
      // If the actual IPC caller is different from the logical source, then  
      // also see if they are allowed to control app switches.  
      if (callingUid != -1 && callingUid != sourceUid) {  
          perm = checkComponentPermission(STOP_APP_SWITCHES, callingPid, callingUid, -1, true);  
          if (perm == PackageManager.PERMISSION_GRANTED) {  
              return true;  
          }  
          if (checkAllowAppSwitchUid(callingUid)) {  
              return true;  
          }  
      }  
      Slog.w(TAG, name + " request from " + sourceUid + " stopped");  
      return false;  
  }
```


当mAppSwitchesAllowedTime时间小于当前时间或者具有STOP\_APP\_SWITCHES的权限，则允许app发生切换操作。 其中mAppSwitchesAllowedTime在AMS.stopAppSwitches的过程中会设置， mAppSwitchesAllowedTime = SystemClock.uptimeMillis()+APP\_SWITCH\_DELAY\_TIME(=5s); 禁止app切换的timeout时间为5s。

当发送5s超时或者执行ASM.resumeAppSwitches过程会将mAppSwitchesAllowedTime 设置为0，都会开启允许app执行切换的操作。禁止app切换的操作，对于同一个app是不受影响的，可查看AMS.checkComponentPermission

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-8-1-ASC-doPendingActivityLaunches "2.8.1 ASC.doPendingActivityLaunches")2.8.1 ASC.doPendingActivityLaunches

\[->ActivityStartController.java\]

```
void doPendingActivityLaunches(boolean doResume) {  
      while (!mPendingActivityLaunches.isEmpty()) {  
          final PendingActivityLaunch pal = mPendingActivityLaunches.remove(0);  
          final boolean resume = doResume && mPendingActivityLaunches.isEmpty();  
          final ActivityStarter starter = obtainStarter(null /* intent */,  
                  "pendingActivityLaunch");  
          try {  
              starter.startResolvedActivity(pal.r, pal.sourceRecord, null, null, pal.startFlags,  
                      resume, null, null, null /* outRecords */);  
          } catch (Exception e) {  
              Slog.e(TAG, "Exception during pending activity launch pal=" + pal, e);  
              pal.sendErrorResult(e.getMessage());  
          }  
      }  
  }
```


mPendingActivityLaunches记录所有将要启动的Activity，由于在startActivity过程中时app切换功能被禁止，也就是不运行切换的Activity，就会将该Activity加入到mPendingActivityLaunches队列，该队列执行完doPendingActivityLaunches会清空。启动doPendingActivityLaunches的所有Activity，由于doResume=false，那么activity不会进入resume，而是设置delayedResume = true,延迟resume。

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-9-AS-startActivityUnchecked "2.9  AS.startActivityUnchecked")2.9 AS.startActivityUnchecked

\[->ActivityStarter.java\]

```
//r是本次要启动的activity，sourceRecord是调用者  
// Note: This method should only be called from {@link startActivity}.  
  private int startActivityUnchecked(final ActivityRecord r, ActivityRecord sourceRecord,  
          IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,  
          int startFlags, boolean doResume, ActivityOptions options, TaskRecord inTask,  
          ActivityRecord[] outActivity) {  
      //设置初始化状态，见2.9.1  
      setInitialState(r, options, inTask, doResume, startFlags, sourceRecord, voiceSession,  
              voiceInteractor);  
      //确定启动taskflag，见2.9.2  
      computeLaunchingTaskFlags();  
      //确定调用者栈，见2.9.3  
      computeSourceStack();  
  
      mIntent.setFlags(mLaunchFlags);  
  
      //得到可用的ActivityRecord  
      ActivityRecord reusedActivity = getReusableIntentActivity();  
  
      int preferredWindowingMode = WINDOWING_MODE_UNDEFINED;  
      int preferredLaunchDisplayId = DEFAULT_DISPLAY;  
      if (mOptions != null) {  
          preferredWindowingMode = mOptions.getLaunchWindowingMode();  
          preferredLaunchDisplayId = mOptions.getLaunchDisplayId();  
      }  
  
      // windowing mode and preferred launch display values from {@link LaunchParams} take  
      // priority over those specified in {@link ActivityOptions}.  
      if (!mLaunchParams.isEmpty()) {  
          if (mLaunchParams.hasPreferredDisplay()) {  
              preferredLaunchDisplayId = mLaunchParams.mPreferredDisplayId;  
          }  
  
          if (mLaunchParams.hasWindowingMode()) {  
              preferredWindowingMode = mLaunchParams.mWindowingMode;  
          }  
      }  
  
      if (reusedActivity != null) {  
          //LockTask mode 且设置了NEW_TASK and CLEAR_TASK则返回  
          // When the flags NEW_TASK and CLEAR_TASK are set, then the task gets reused but  
          // still needs to be a lock task mode violation since the task gets cleared out and  
          // the device would otherwise leave the locked task.  
          if (mService.getLockTaskController().isLockTaskModeViolation(reusedActivity.getTask(),  
                  (mLaunchFlags & (FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_CLEAR_TASK))  
                          == (FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_CLEAR_TASK))) {  
              Slog.e(TAG, "startActivityUnchecked: Attempt to violate Lock Task Mode");  
              return START_RETURN_LOCK_TASK_MODE_VIOLATION;  
          }  
  
          // True if we are clearing top and resetting of a standard (default) launch mode  
          // ({@code LAUNCH_MULTIPLE}) activity. The existing activity will be finished.  
          final boolean clearTopAndResetStandardLaunchMode =  
                  (mLaunchFlags & (FLAG_ACTIVITY_CLEAR_TOP | FLAG_ACTIVITY_RESET_TASK_IF_NEEDED))  
                          == (FLAG_ACTIVITY_CLEAR_TOP | FLAG_ACTIVITY_RESET_TASK_IF_NEEDED)  
                  && mLaunchMode == LAUNCH_MULTIPLE;  
  
          //如果启动的activity没有管理task，则用存在activity的task  
          // If mStartActivity does not have a task associated with it, associate it with the  
          // reused activity's task. Do not do so if we're clearing top and resetting for a  
          // standard launchMode activity.  
          if (mStartActivity.getTask() == null && !clearTopAndResetStandardLaunchMode) {  
              mStartActivity.setTask(reusedActivity.getTask());  
          }  
  
          if (reusedActivity.getTask().intent == null) {  
              //设置mStartActivity  
              // This task was started because of movement of the activity based on affinity...  
              // Now that we are actually launching it, we can assign the base intent.  
              reusedActivity.getTask().setIntent(mStartActivity);  
          }  
  
          // This code path leads to delivering a new intent, we want to make sure we schedule it  
          // as the first operation, in case the activity will be resumed as a result of later  
          // operations.  
          if ((mLaunchFlags & FLAG_ACTIVITY_CLEAR_TOP) != 0  
                  || isDocumentLaunchesIntoExisting(mLaunchFlags)  
                  || isLaunchModeOneOf(LAUNCH_SINGLE_INSTANCE, LAUNCH_SINGLE_TASK)) {  
              final TaskRecord task = reusedActivity.getTask();  
              //LAUNCH_SINGLE_INSTANCE,LAUNCH_SINGLE_TASK模式下，栈移除所有的activity  
              // In this situation we want to remove all activities from the task up to the one  
              // being started. In most cases this means we are resetting the task to its initial  
              // state.  
              final ActivityRecord top = task.performClearTaskForReuseLocked(mStartActivity,  
                      mLaunchFlags);  
  
              // The above code can remove {@code reusedActivity} from the task, leading to the  
              // the {@code ActivityRecord} removing its reference to the {@code TaskRecord}. The  
              // task reference is needed in the call below to  
              // {@link setTargetStackAndMoveToFrontIfNeeded}.  
              if (reusedActivity.getTask() == null) {  
                  reusedActivity.setTask(task);  
              }  
  
              if (top != null) {  
                  //在前台  
                  if (top.frontOfTask) {  
                      // Activity aliases may mean we use different intents for the top activity,  
                      // so make sure the task now has the identity of the new intent.  
                      top.getTask().setIntent(mStartActivity);  
                  }  
                  deliverNewIntent(top);  
              }  
          }  
  
          mSupervisor.sendPowerHintForLaunchStartIfNeeded(false /* forceSend */, reusedActivity);  
  
          reusedActivity = setTargetStackAndMoveToFrontIfNeeded(reusedActivity);  
  
          final ActivityRecord outResult =  
                  outActivity != null && outActivity.length > 0 ? outActivity[0] : null;  
  
          // When there is a reused activity and the current result is a trampoline activity,  
          // set the reused activity as the result.  
          if (outResult != null && (outResult.finishing || outResult.noDisplay)) {  
              outActivity[0] = reusedActivity;  
          }  
  
          if ((mStartFlags & START_FLAG_ONLY_IF_NEEDED) != 0) {  
              // We don't need to start a new activity, and the client said not to do anything  
              // if that is the case, so this is it!  And for paranoia, make sure we have  
              // correctly resumed the top activity.  
              resumeTargetStackIfNeeded();  
              return START_RETURN_INTENT_TO_CALLER;  
          }  
  
          if (reusedActivity != null) {  
              setTaskFromIntentActivity(reusedActivity);  
  
              if (!mAddingToTask && mReuseTask == null) {  
                  // We didn't do anything...  but it was needed (a.k.a., client don't use that  
                  // intent!)  And for paranoia, make sure we have correctly resumed the top activity.  
  
                  resumeTargetStackIfNeeded();  
                  if (outActivity != null && outActivity.length > 0) {  
                      outActivity[0] = reusedActivity;  
                  }  
  
                  return mMovedToFront ? START_TASK_TO_FRONT : START_DELIVERED_TO_TOP;  
              }  
          }  
      }  
        
      //启动的activity没有包名，直接返回  
      if (mStartActivity.packageName == null) {  
          final ActivityStack sourceStack = mStartActivity.resultTo != null  
                  ? mStartActivity.resultTo.getStack() : null;  
          if (sourceStack != null) {  
              sourceStack.sendActivityResultLocked(-1 /* callingUid */, mStartActivity.resultTo,  
                      mStartActivity.resultWho, mStartActivity.requestCode, RESULT_CANCELED,  
                      null /* data */);  
          }  
          ActivityOptions.abort(mOptions);  
          return START_CLASS_NOT_FOUND;  
      }  
  
      // If the activity being launched is the same as the one currently at the top, then  
      // we need to check if it should only be launched once.  
      final ActivityStack topStack = mSupervisor.mFocusedStack;  
      final ActivityRecord topFocused = topStack.getTopActivity();  
      final ActivityRecord top = topStack.topRunningNonDelayedActivityLocked(mNotTop);  
      final boolean dontStart = top != null && mStartActivity.resultTo == null  
              && top.realActivity.equals(mStartActivity.realActivity)  
              && top.userId == mStartActivity.userId  
              && top.app != null && top.app.thread != null  
              && ((mLaunchFlags & FLAG_ACTIVITY_SINGLE_TOP) != 0  
              || isLaunchModeOneOf(LAUNCH_SINGLE_TOP, LAUNCH_SINGLE_TASK));          
      if (dontStart) {  
          // For paranoia, make sure we have correctly resumed the top activity.  
          topStack.mLastPausedActivity = null;  
          if (mDoResume) {  
              mSupervisor.resumeFocusedStackTopActivityLocked();  
          }  
          ActivityOptions.abort(mOptions);  
          if ((mStartFlags & START_FLAG_ONLY_IF_NEEDED) != 0) {  
              // We don't need to start a new activity, and the client said not to do  
              // anything if that is the case, so this is it!  
              return START_RETURN_INTENT_TO_CALLER;  
          }  
          //触发onNewIntent  
          deliverNewIntent(top);  
  
          // Don't use mStartActivity.task to show the toast. We're not starting a new activity  
          // but reusing 'top'. Fields in mStartActivity may not be fully initialized.  
          mSupervisor.handleNonResizableTaskIfNeeded(top.getTask(), preferredWindowingMode,  
                  preferredLaunchDisplayId, topStack);  
  
          return START_DELIVERED_TO_TOP;  
      }  
  
      boolean newTask = false;  
      final TaskRecord taskToAffiliate = (mLaunchTaskBehind && mSourceRecord != null)  
              ? mSourceRecord.getTask() : null;  
  
      // Should this be considered a new task?  
      int result = START_SUCCESS;  
      if (mStartActivity.resultTo == null && mInTask == null && !mAddingToTask  
              && (mLaunchFlags & FLAG_ACTIVITY_NEW_TASK) != 0) {  
          newTask = true;  
          result = setTaskFromReuseOrCreateNewTask(taskToAffiliate, topStack);  
      } else if (mSourceRecord != null) {  
          result = setTaskFromSourceRecord();  
      } else if (mInTask != null) {  
          result = setTaskFromInTask();  
      } else {  
          // This not being started from an existing activity, and not part of a new task...  
          // just put it in the top task, though these days this case should never happen.  
          setTaskToCurrentTopOrCreateNewTask();  
      }  
      if (result != START_SUCCESS) {  
          return result;  
      }  
  
      mService.grantUriPermissionFromIntentLocked(mCallingUid, mStartActivity.packageName,  
              mIntent, mStartActivity.getUriPermissionsLocked(), mStartActivity.userId);  
      mService.grantEphemeralAccessLocked(mStartActivity.userId, mIntent,  
              mStartActivity.appInfo.uid, UserHandle.getAppId(mCallingUid));  
      if (newTask) {  
          EventLog.writeEvent(EventLogTags.AM_CREATE_TASK, mStartActivity.userId,  
                  mStartActivity.getTask().taskId);  
      }  
      ActivityStack.logStartActivity(  
              EventLogTags.AM_CREATE_ACTIVITY, mStartActivity, mStartActivity.getTask());  
      mTargetStack.mLastPausedActivity = null;  
  
      mSupervisor.sendPowerHintForLaunchStartIfNeeded(false /* forceSend */, mStartActivity);  
      //见2.10节  
      mTargetStack.startActivityLocked(mStartActivity, topFocused, newTask, mKeepCurTransition,  
              mOptions);  
      if (mDoResume) {  
          final ActivityRecord topTaskActivity =  
                  mStartActivity.getTask().topRunningActivityLocked();  
          if (!mTargetStack.isFocusable()  
                  || (topTaskActivity != null && topTaskActivity.mTaskOverlay  
                  && mStartActivity != topTaskActivity)) {  
              // 没有获取焦点，不能resume     
              // If the activity is not focusable, we can't resume it, but still would like to  
              // make sure it becomes visible as it starts (this will also trigger entry  
              // animation). An example of this are PIP activities.  
              // Also, we don't want to resume activities in a task that currently has an overlay  
              // as the starting activity just needs to be in the visible paused state until the  
              // over is removed.  
              mTargetStack.ensureActivitiesVisibleLocked(null, 0, !PRESERVE_WINDOWS);  
              // Go ahead and tell window manager to execute app transition for this activity  
              // since the app transition will not be triggered through the resume channel.  
              mService.mWindowManager.executeAppTransition();  
          } else {  
              // If the target stack was not previously focusable (previous top running activity  
              // on that stack was not visible) then any prior calls to move the stack to the  
              // will not update the focused stack.  If starting the new activity now allows the  
              // task stack to be focusable, then ensure that we now update the focused stack  
              // accordingly.  
              if (mTargetStack.isFocusable() && !mSupervisor.isFocusedStack(mTargetStack)) {  
                  mTargetStack.moveToFront("startActivityUnchecked");  
              }  
              //见2.11节  
              mSupervisor.resumeFocusedStackTopActivityLocked(mTargetStack, mStartActivity,  
                      mOptions);  
          }  
      } else if (mStartActivity != null) {  
          mSupervisor.mRecentTasks.add(mStartActivity.getTask());  
      }  
      mSupervisor.updateUserStackLocked(mStartActivity.userId, mTargetStack);  
  
      mSupervisor.handleNonResizableTaskIfNeeded(mStartActivity.getTask(), preferredWindowingMode,  
              preferredLaunchDisplayId, mTargetStack);  
  
      return START_SUCCESS;  
  }
```

找到或者创建新的Activity所属的Task对象，之后调用AS.startActivityLocked

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-9-1-AS-setInitialState "2.9.1 AS.setInitialState")2.9.1 AS.setInitialState

\[->ActivityStarter.java\]

```
private void setInitialState(ActivityRecord r, ActivityOptions options, TaskRecord inTask,  
            boolean doResume, int startFlags, ActivityRecord sourceRecord,  
            IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor) {  
        reset(false /* clearRequest */);  
  
        mStartActivity = r;  
        mIntent = r.intent;  
        mOptions = options;  
        mCallingUid = r.launchedFromUid;  
        mSourceRecord = sourceRecord;  
        mVoiceSession = voiceSession;  
        mVoiceInteractor = voiceInteractor;  
  
        mPreferredDisplayId = getPreferedDisplayId(mSourceRecord, mStartActivity, options);  
  
        mLaunchParams.reset();  
  
        mSupervisor.getLaunchParamsController().calculate(inTask, null /*layout*/, r, sourceRecord,  
                options, mLaunchParams);  
  
        mLaunchMode = r.launchMode;  
        //当intent和Activity manifest存在冲突，则manifest优先  
        mLaunchFlags = adjustLaunchFlagsToDocumentMode(  
                r, LAUNCH_SINGLE_INSTANCE == mLaunchMode,  
                LAUNCH_SINGLE_TASK == mLaunchMode, mIntent.getFlags());  
        mLaunchTaskBehind = r.mLaunchTaskBehind  
                && !isLaunchModeOneOf(LAUNCH_SINGLE_TASK, LAUNCH_SINGLE_INSTANCE)  
                && (mLaunchFlags & FLAG_ACTIVITY_NEW_DOCUMENT) != 0;  
  
        sendNewTaskResultRequestIfNeeded();  
     
        if ((mLaunchFlags & FLAG_ACTIVITY_NEW_DOCUMENT) != 0 && r.resultTo == null) {  
            mLaunchFlags |= FLAG_ACTIVITY_NEW_TASK;  
        }  
  
        // If we are actually going to launch in to a new task, there are some cases where  
        // we further want to do multiple task.  
        if ((mLaunchFlags & FLAG_ACTIVITY_NEW_TASK) != 0) {  
            if (mLaunchTaskBehind  
                    || r.info.documentLaunchMode == DOCUMENT_LAUNCH_ALWAYS) {  
                mLaunchFlags |= FLAG_ACTIVITY_MULTIPLE_TASK;  
            }  
        }  
  
        // We'll invoke onUserLeaving before onPause only if the launching  
        // activity did not explicitly state that this is an automated launch.  
        mSupervisor.mUserLeaving = (mLaunchFlags & FLAG_ACTIVITY_NO_USER_ACTION) == 0;  
        if (DEBUG_USER_LEAVING) Slog.v(TAG_USER_LEAVING,  
                "startActivity() => mUserLeaving=" + mSupervisor.mUserLeaving);  
        //当本次不需要resume时，则设置为延迟resume的状态  
        // If the caller has asked not to resume at this point, we make note  
        // of this in the record so that we can skip it when trying to find  
        // the top running activity.  
        mDoResume = doResume;  
        if (!doResume || !r.okToShowLocked()) {  
            r.delayedResume = true;  
            mDoResume = false;  
        }  
  
        if (mOptions != null) {  
            if (mOptions.getLaunchTaskId() != -1 && mOptions.getTaskOverlay()) {  
                r.mTaskOverlay = true;  
                if (!mOptions.canTaskOverlayResume()) {  
                    final TaskRecord task = mSupervisor.anyTaskForIdLocked(  
                            mOptions.getLaunchTaskId());  
                    final ActivityRecord top = task != null ? task.getTopActivity() : null;  
                    if (top != null && !top.isState(RESUMED)) {  
  
                        // The caller specifies that we'd like to be avoided to be moved to the  
                        // front, so be it!  
                        mDoResume = false;  
                        mAvoidMoveToFront = true;  
                    }  
                }  
            } else if (mOptions.getAvoidMoveToFront()) {  
                mDoResume = false;  
                mAvoidMoveToFront = true;  
            }  
        }  
  
        mNotTop = (mLaunchFlags & FLAG_ACTIVITY_PREVIOUS_IS_TOP) != 0 ? r : null;  
  
        mInTask = inTask;  
        // In some flows in to this function, we retrieve the task record and hold on to it  
        // without a lock before calling back in to here...  so the task at this point may  
        // not actually be in recents.  Check for that, and if it isn't in recents just  
        // consider it invalid.  
        if (inTask != null && !inTask.inRecents) {  
            Slog.w(TAG, "Starting activity in task not in recents: " + inTask);  
            mInTask = null;  
        }  
  
        mStartFlags = startFlags;  
        // If the onlyIfNeeded flag is set, then we can do this if the activity being launched  
        // is the same as the one making the call...  or, as a special case, if we do not know  
        // the caller then we count the current top activity as the caller.  
        if ((startFlags & START_FLAG_ONLY_IF_NEEDED) != 0) {  
            ActivityRecord checkedCaller = sourceRecord;  
            if (checkedCaller == null) {  
                checkedCaller = mSupervisor.mFocusedStack.topRunningNonDelayedActivityLocked(  
                        mNotTop);  
            }  
            if (!checkedCaller.realActivity.equals(r.realActivity)) {  
                //调用者与将要启动的activity不相同时今日该分支  
                // Caller is not the same as launcher, so always needed.  
                mStartFlags &= ~START_FLAG_ONLY_IF_NEEDED;  
            }  
        }  
  
        mNoAnimation = (mLaunchFlags & FLAG_ACTIVITY_NO_ANIMATION) != 0;  
    }
```
#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-9-2-AS-computeLaunchingTaskFlags "2.9.2 AS.computeLaunchingTaskFlags")2.9.2 AS.computeLaunchingTaskFlags

\[->ActivityStarter.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br><span>40</span><br><span>41</span><br><span>42</span><br><span>43</span><br><span>44</span><br><span>45</span><br><span>46</span><br><span>47</span><br><span>48</span><br><span>49</span><br><span>50</span><br><span>51</span><br><span>52</span><br><span>53</span><br><span>54</span><br><span>55</span><br><span>56</span><br><span>57</span><br><span>58</span><br><span>59</span><br><span>60</span><br><span>61</span><br><span>62</span><br><span>63</span><br><span>64</span><br><span>65</span><br><span>66</span><br><span>67</span><br><span>68</span><br><span>69</span><br><span>70</span><br><span>71</span><br><span>72</span><br><span>73</span><br><span>74</span><br><span>75</span><br><span>76</span><br><span>77</span><br><span>78</span><br><span>79</span><br><span>80</span><br><span>81</span><br><span>82</span><br><span>83</span><br><span>84</span><br><span>85</span><br><span>86</span><br><span>87</span><br></pre></td><td><pre><span>private void computeLaunchingTaskFlags() {</span><br><span>        //当调用者不是来自于activity，而是指定明确task的情况</span><br><span>        // If the caller is not coming from another activity, but has given us an explicit task into</span><br><span>        // which they would like us to launch the new activity, then let's see about doing that.</span><br><span>        if (mSourceRecord == null &amp;&amp; mInTask != null &amp;&amp; mInTask.getStack() != null) {</span><br><span>            final Intent baseIntent = mInTask.getBaseIntent();</span><br><span>            final ActivityRecord root = mInTask.getRootActivity();</span><br><span>            if (baseIntent == null) {</span><br><span>                ActivityOptions.abort(mOptions);</span><br><span>                throw new IllegalArgumentException("Launching into task without base intent: "</span><br><span>                        + mInTask);</span><br><span>            }</span><br><span></span><br><span>            // If this task is empty, then we are adding the first activity -- it</span><br><span>            // determines the root, and must be launching as a NEW_TASK.</span><br><span>            if (isLaunchModeOneOf(LAUNCH_SINGLE_INSTANCE, LAUNCH_SINGLE_TASK)) {</span><br><span>                if (!baseIntent.getComponent().equals(mStartActivity.intent.getComponent())) {</span><br><span>                    ActivityOptions.abort(mOptions);</span><br><span>                    throw new IllegalArgumentException("Trying to launch singleInstance/Task "</span><br><span>                            + mStartActivity + " into different task " + mInTask);</span><br><span>                }</span><br><span>                if (root != null) {</span><br><span>                    ActivityOptions.abort(mOptions);</span><br><span>                    throw new IllegalArgumentException("Caller with mInTask " + mInTask</span><br><span>                            + " has root " + root + " but target is singleInstance/Task");</span><br><span>                }</span><br><span>            }</span><br><span></span><br><span>            // If task is empty, then adopt the interesting intent launch flags in to the</span><br><span>            // activity being started.</span><br><span>            if (root == null) {</span><br><span>                final int flagsOfInterest = FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK</span><br><span>                        | FLAG_ACTIVITY_NEW_DOCUMENT | FLAG_ACTIVITY_RETAIN_IN_RECENTS;</span><br><span>                mLaunchFlags = (mLaunchFlags &amp; ~flagsOfInterest)</span><br><span>                        | (baseIntent.getFlags() &amp; flagsOfInterest);</span><br><span>                mIntent.setFlags(mLaunchFlags);</span><br><span>                mInTask.setIntent(mStartActivity);</span><br><span>                mAddingToTask = true;</span><br><span></span><br><span>                // If the task is not empty and the caller is asking to start it as the root of</span><br><span>                // a new task, then we don't actually want to start this on the task. We will</span><br><span>                // bring the task to the front, and possibly give it a new intent.</span><br><span>            } else if ((mLaunchFlags &amp; FLAG_ACTIVITY_NEW_TASK) != 0) {</span><br><span>                mAddingToTask = false;</span><br><span></span><br><span>            } else {</span><br><span>                mAddingToTask = true;</span><br><span>            }</span><br><span></span><br><span>            mReuseTask = mInTask;</span><br><span>        } else {</span><br><span>            mInTask = null;</span><br><span>            // Launch ResolverActivity in the source task, so that it stays in the task bounds</span><br><span>            // when in freeform workspace.</span><br><span>            // Also put noDisplay activities in the source task. These by itself can be placed</span><br><span>            // in any task/stack, however it could launch other activities like ResolverActivity,</span><br><span>            // and we want those to stay in the original task.</span><br><span>            if ((mStartActivity.isResolverActivity() || mStartActivity.noDisplay) &amp;&amp; mSourceRecord != null</span><br><span>                    &amp;&amp; mSourceRecord.inFreeformWindowingMode())  {</span><br><span>                mAddingToTask = true;</span><br><span>            }</span><br><span>        }</span><br><span></span><br><span>        if (mInTask == null) {</span><br><span>            if (mSourceRecord == null) {</span><br><span>                //调用者不是Activity context,则强制创建新task</span><br><span>                // This activity is not being started from another...  in this</span><br><span>                // case we -always- start a new task.</span><br><span>                if ((mLaunchFlags &amp; FLAG_ACTIVITY_NEW_TASK) == 0 &amp;&amp; mInTask == null) {</span><br><span>                    Slog.w(TAG, "startActivity called from non-Activity context; forcing " +</span><br><span>                            "Intent.FLAG_ACTIVITY_NEW_TASK for: " + mIntent);</span><br><span>                    mLaunchFlags |= FLAG_ACTIVITY_NEW_TASK;</span><br><span>                }</span><br><span>            } else if (mSourceRecord.launchMode == LAUNCH_SINGLE_INSTANCE) {</span><br><span>                 //调用者启动模式是single instance，则创建新task</span><br><span>                // The original activity who is starting us is running as a single</span><br><span>                // instance...  this new activity it is starting must go on its</span><br><span>                // own task.</span><br><span>                mLaunchFlags |= FLAG_ACTIVITY_NEW_TASK;</span><br><span>            } else if (isLaunchModeOneOf(LAUNCH_SINGLE_INSTANCE, LAUNCH_SINGLE_TASK)) {</span><br><span>                //目标activity带有single instance或者single task则创建新的task</span><br><span>                // The activity being started is a single instance...  it always</span><br><span>                // gets launched into its own task.</span><br><span>                mLaunchFlags |= FLAG_ACTIVITY_NEW_TASK;</span><br><span>            }</span><br><span>        }</span><br><span>    }</span><br></pre></td></tr></tbody></table>

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-9-3-AS-computeSourceStack "2.9.3 AS.computeSourceStack")2.9.3 AS.computeSourceStack

\[->ActivityStarter.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br></pre></td><td><pre><span>private void computeSourceStack() {</span><br><span>       if (mSourceRecord == null) {</span><br><span>           mSourceStack = null;</span><br><span>           return;</span><br><span>       }</span><br><span>       if (!mSourceRecord.finishing) {</span><br><span>          //当调用者activity不为空，且不在finishing状态，则其所在的栈赋于sourceStack</span><br><span>           mSourceStack = mSourceRecord.getStack();</span><br><span>           return;</span><br><span>       }</span><br><span>       //当调用者处于finishing状态，则创建新的task</span><br><span>       // If the source is finishing, we can't further count it as our source. This is because the</span><br><span>       // task it is associated with may now be empty and on its way out, so we don't want to</span><br><span>       // blindly throw it in to that task.  Instead we will take the NEW_TASK flow and try to find</span><br><span>       // a task for it. But save the task information so it can be used when creating the new task.</span><br><span>       if ((mLaunchFlags &amp; FLAG_ACTIVITY_NEW_TASK) == 0) {</span><br><span>           Slog.w(TAG, "startActivity called from finishing " + mSourceRecord</span><br><span>                   + "; forcing " + "Intent.FLAG_ACTIVITY_NEW_TASK for: " + mIntent);</span><br><span>           mLaunchFlags |= FLAG_ACTIVITY_NEW_TASK;</span><br><span>           mNewTaskInfo = mSourceRecord.info;</span><br><span></span><br><span>           // It is not guaranteed that the source record will have a task associated with it. For,</span><br><span>           // example, if this method is being called for processing a pending activity launch, it</span><br><span>           // is possible that the activity has been removed from the task after the launch was</span><br><span>           // enqueued.</span><br><span>           final TaskRecord sourceTask = mSourceRecord.getTask();</span><br><span>           mNewTaskIntent = sourceTask != null ? sourceTask.intent : null;</span><br><span>       }</span><br><span>       mSourceRecord = null;</span><br><span>       mSourceStack = null;</span><br><span>   }</span><br></pre></td></tr></tbody></table>

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-9-4-AS-getReusableIntentActivity "2.9.4 AS.getReusableIntentActivity")2.9.4 AS.getReusableIntentActivity

\[->ActivityStarter.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br></pre></td><td><pre><span>/**</span><br><span>     * Decide whether the new activity should be inserted into an existing task. Returns null</span><br><span>     * if not or an ActivityRecord with the task into which the new activity should be added.</span><br><span>     */</span><br><span>    private ActivityRecord getReusableIntentActivity() {</span><br><span>        // We may want to try to place the new activity in to an existing task.  We always</span><br><span>        // do this if the target activity is singleTask or singleInstance; we will also do</span><br><span>        // this if NEW_TASK has been requested, and there is not an additional qualifier telling</span><br><span>        // us to still place it in a new task: multi task, always doc mode, or being asked to</span><br><span>        // launch this as a new task behind the current one.</span><br><span>        boolean putIntoExistingTask = ((mLaunchFlags &amp; FLAG_ACTIVITY_NEW_TASK) != 0 &amp;&amp;</span><br><span>                (mLaunchFlags &amp; FLAG_ACTIVITY_MULTIPLE_TASK) == 0)</span><br><span>                || isLaunchModeOneOf(LAUNCH_SINGLE_INSTANCE, LAUNCH_SINGLE_TASK);</span><br><span>        // If bring to front is requested, and no result is requested and we have not been given</span><br><span>        // an explicit task to launch in to, and we can find a task that was started with this</span><br><span>        // same component, then instead of launching bring that one to the front.</span><br><span>        putIntoExistingTask &amp;= mInTask == null &amp;&amp; mStartActivity.resultTo == null;</span><br><span>        ActivityRecord intentActivity = null;</span><br><span>        if (mOptions != null &amp;&amp; mOptions.getLaunchTaskId() != -1) {</span><br><span>            final TaskRecord task = mSupervisor.anyTaskForIdLocked(mOptions.getLaunchTaskId());</span><br><span>            intentActivity = task != null ? task.getTopActivity() : null;</span><br><span>        } else if (putIntoExistingTask) {</span><br><span>            if (LAUNCH_SINGLE_INSTANCE == mLaunchMode) {</span><br><span>                // There can be one and only one instance of single instance activity in the</span><br><span>                // history, and it is always in its own unique task, so we do a special search.</span><br><span>               intentActivity = mSupervisor.findActivityLocked(mIntent, mStartActivity.info,</span><br><span>                       mStartActivity.isActivityTypeHome());</span><br><span>            } else if ((mLaunchFlags &amp; FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0) {</span><br><span>                // For the launch adjacent case we only want to put the activity in an existing</span><br><span>                // task if the activity already exists in the history.</span><br><span>                intentActivity = mSupervisor.findActivityLocked(mIntent, mStartActivity.info,</span><br><span>                        !(LAUNCH_SINGLE_TASK == mLaunchMode));</span><br><span>            } else {</span><br><span>                // Otherwise find the best task to put the activity in.</span><br><span>                intentActivity = mSupervisor.findTaskLocked(mStartActivity, mPreferredDisplayId);</span><br><span>            }</span><br><span>        }</span><br><span>        return intentActivity;</span><br><span>    }</span><br></pre></td></tr></tbody></table>

上面是根据不同的启动模式，来获取ActivityRecord信息，来决定将要启动的activity所在的栈。

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-9-5-Launch-Mode "2.9.5 Launch Mode")2.9.5 Launch Mode

AcitivityInfo.java定义了四类Launch Mode:

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br></pre></td><td><pre><span>/**</span><br><span> * Constant corresponding to &lt;code&gt;standard&lt;/code&gt; in</span><br><span> * the {@link android.R.attr#launchMode} attribute.</span><br><span> */</span><br><span>//每次启动新Activity，都会创建新的Activity，这是最常见标准情形</span><br><span>public static final int LAUNCH_MULTIPLE = 0;</span><br><span>/**</span><br><span> * Constant corresponding to &lt;code&gt;singleTop&lt;/code&gt; in</span><br><span> * the {@link android.R.attr#launchMode} attribute.</span><br><span> */</span><br><span>//当启动新Activity，如果栈顶存在相同Activity，则不会创建新的Activity  </span><br><span>public static final int LAUNCH_SINGLE_TOP = 1;</span><br><span>/**</span><br><span> * Constant corresponding to &lt;code&gt;singleTask&lt;/code&gt; in</span><br><span> * the {@link android.R.attr#launchMode} attribute.</span><br><span> */</span><br><span>//当启动Activity，在栈中存在相同Activity，则不会创建新的Activity</span><br><span>//而是移除该Activity之上的所有Activity</span><br><span>public static final int LAUNCH_SINGLE_TASK = 2;</span><br><span>/**</span><br><span> * Constant corresponding to &lt;code&gt;singleInstance&lt;/code&gt; in</span><br><span> * the {@link android.R.attr#launchMode} attribute.</span><br><span> */</span><br><span> //每个Task栈只有一个Activity</span><br><span>public static final int LAUNCH_SINGLE_INSTANCE = 3;</span><br></pre></td></tr></tbody></table>

常见的flag含义

FLAG\_ACTIVITY\_NEW\_TASK

将新Activity放入新启动的task。

FLAG\_ACTIVITY\_CLEAR\_TASK

启动Activity时，将目标Activity关联的task清除，再启动新task,将该Activity放入该Task。这个flag一般配置FLAG\_ACTIVITY\_NEW\_TASK使用。

FLAG\_ACTIVITY\_CLEAR\_TOP

启动非栈顶Activity时，先清除该Activity之上的Activity. 例如Task已有A,B,C,D，启动A,则需要先清除B,C,D，类似SingleTop。

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-10-AS-startActivityLocked "2.10 AS.startActivityLocked")2.10 AS.startActivityLocked

\[->ActivityStack.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br><span>40</span><br><span>41</span><br><span>42</span><br><span>43</span><br><span>44</span><br><span>45</span><br><span>46</span><br><span>47</span><br><span>48</span><br><span>49</span><br><span>50</span><br><span>51</span><br><span>52</span><br><span>53</span><br><span>54</span><br><span>55</span><br><span>56</span><br><span>57</span><br><span>58</span><br><span>59</span><br><span>60</span><br><span>61</span><br><span>62</span><br><span>63</span><br><span>64</span><br><span>65</span><br><span>66</span><br><span>67</span><br><span>68</span><br><span>69</span><br><span>70</span><br><span>71</span><br><span>72</span><br><span>73</span><br><span>74</span><br><span>75</span><br><span>76</span><br><span>77</span><br><span>78</span><br><span>79</span><br><span>80</span><br><span>81</span><br><span>82</span><br><span>83</span><br><span>84</span><br><span>85</span><br><span>86</span><br><span>87</span><br><span>88</span><br><span>89</span><br><span>90</span><br><span>91</span><br><span>92</span><br><span>93</span><br><span>94</span><br><span>95</span><br><span>96</span><br><span>97</span><br><span>98</span><br><span>99</span><br><span>100</span><br><span>101</span><br><span>102</span><br><span>103</span><br><span>104</span><br><span>105</span><br><span>106</span><br><span>107</span><br><span>108</span><br><span>109</span><br><span>110</span><br><span>111</span><br><span>112</span><br><span>113</span><br><span>114</span><br><span>115</span><br><span>116</span><br><span>117</span><br><span>118</span><br><span>119</span><br><span>120</span><br><span>121</span><br><span>122</span><br><span>123</span><br><span>124</span><br><span>125</span><br><span>126</span><br><span>127</span><br><span>128</span><br><span>129</span><br><span>130</span><br><span>131</span><br><span>132</span><br><span>133</span><br><span>134</span><br><span>135</span><br><span>136</span><br><span>137</span><br><span>138</span><br><span>139</span><br></pre></td><td><pre><span>void startActivityLocked(ActivityRecord r, ActivityRecord focusedTopActivity,</span><br><span>           boolean newTask, boolean keepCurTransition, ActivityOptions options) {</span><br><span>       TaskRecord rTask = r.getTask();</span><br><span>       final int taskId = rTask.taskId;</span><br><span>       // mLaunchTaskBehind tasks get placed at the back of the task stack.</span><br><span>       if (!r.mLaunchTaskBehind &amp;&amp; (taskForIdLocked(taskId) == null || newTask)) {</span><br><span>           // Last activity in task had been removed or ActivityManagerService is reusing task.</span><br><span>           // Insert or replace.</span><br><span>           // Might not even be in.</span><br><span>           //task中上一个activity被移除，或者ams重用task,则将该task移到顶部</span><br><span>           insertTaskAtTop(rTask, r);</span><br><span>       }</span><br><span>       TaskRecord task = null;</span><br><span>       if (!newTask) {</span><br><span>           // If starting in an existing task, find where that is...</span><br><span>           boolean startIt = true;</span><br><span>           for (int taskNdx = mTaskHistory.size() - 1; taskNdx &gt;= 0; --taskNdx) {</span><br><span>               task = mTaskHistory.get(taskNdx);</span><br><span>               if (task.getTopActivity() == null) {</span><br><span>                   // All activities in task are finishing.</span><br><span>                   //该task所有activity都finishing</span><br><span>                   continue;</span><br><span>               }</span><br><span>               if (task == rTask) {</span><br><span>                   // Here it is!  Now, if this is not yet visible to the</span><br><span>                   // user, then just add it without starting; it will</span><br><span>                   // get started when the user navigates back to it.</span><br><span>                   if (!startIt) {</span><br><span>                       if (DEBUG_ADD_REMOVE) Slog.i(TAG, "Adding activity " + r + " to task "</span><br><span>                               + task, new RuntimeException("here").fillInStackTrace());</span><br><span>                       r.createWindowContainer();</span><br><span>                       ActivityOptions.abort(options);</span><br><span>                       return;</span><br><span>                   }</span><br><span>                   break;</span><br><span>               } else if (task.numFullscreen &gt; 0) {</span><br><span>                   startIt = false;</span><br><span>               }</span><br><span>           }</span><br><span>       }</span><br><span></span><br><span>       // Place a new activity at top of stack, so it is next to interact with the user.</span><br><span></span><br><span>       // If we are not placing the new activity frontmost, we do not want to deliver the</span><br><span>       // onUserLeaving callback to the actual frontmost activity</span><br><span>       final TaskRecord activityTask = r.getTask();</span><br><span>       if (task == activityTask &amp;&amp; mTaskHistory.indexOf(task) != (mTaskHistory.size() - 1)) {</span><br><span>           mStackSupervisor.mUserLeaving = false;</span><br><span>           if (DEBUG_USER_LEAVING) Slog.v(TAG_USER_LEAVING,</span><br><span>                   "startActivity() behind front, mUserLeaving=false");</span><br><span>       }</span><br><span></span><br><span>       task = activityTask;</span><br><span></span><br><span>       // Slot the activity into the history stack and proceed</span><br><span>       if (DEBUG_ADD_REMOVE) Slog.i(TAG, "Adding activity " + r + " to stack to task " + task,</span><br><span>               new RuntimeException("here").fillInStackTrace());</span><br><span>       // TODO: Need to investigate if it is okay for the controller to already be created by the</span><br><span>       // time we get to this point. I think it is, but need to double check.</span><br><span>       // Use test in b/34179495 to trace the call path.</span><br><span>       if (r.getWindowContainerController() == null) {</span><br><span>           r.createWindowContainer();</span><br><span>       }</span><br><span>       task.setFrontOfTask();</span><br><span>      //当切换到新的task或者下一个activity进程目前没有运行</span><br><span>       if (!isHomeOrRecentsStack() || numActivities() &gt; 0) {</span><br><span>           if (DEBUG_TRANSITION) Slog.v(TAG_TRANSITION,</span><br><span>                   "Prepare open transition: starting " + r);</span><br><span>           if ((r.intent.getFlags() &amp; Intent.FLAG_ACTIVITY_NO_ANIMATION) != 0) {</span><br><span>               mWindowManager.prepareAppTransition(TRANSIT_NONE, keepCurTransition);</span><br><span>               mStackSupervisor.mNoAnimActivities.add(r);</span><br><span>           } else {</span><br><span>               int transit = TRANSIT_ACTIVITY_OPEN;</span><br><span>               if (newTask) {</span><br><span>                   if (r.mLaunchTaskBehind) {</span><br><span>                       transit = TRANSIT_TASK_OPEN_BEHIND;</span><br><span>                   } else {</span><br><span>                       // If a new task is being launched, then mark the existing top activity as</span><br><span>                       // supporting picture-in-picture while pausing only if the starting activity</span><br><span>                       // would not be considered an overlay on top of the current activity</span><br><span>                       // (eg. not fullscreen, or the assistant)</span><br><span>                       if (canEnterPipOnTaskSwitch(focusedTopActivity,</span><br><span>                               null /* toFrontTask */, r, options)) {</span><br><span>                           focusedTopActivity.supportsEnterPipOnTaskSwitch = true;</span><br><span>                       }</span><br><span>                       transit = TRANSIT_TASK_OPEN;</span><br><span>                   }</span><br><span>               }</span><br><span>               mWindowManager.prepareAppTransition(transit, keepCurTransition);</span><br><span>               mStackSupervisor.mNoAnimActivities.remove(r);</span><br><span>           }</span><br><span>           boolean doShow = true;</span><br><span>           if (newTask) {</span><br><span>               // Even though this activity is starting fresh, we still need</span><br><span>               // to reset it to make sure we apply affinities to move any</span><br><span>               // existing activities from other tasks in to it.</span><br><span>               // If the caller has requested that the target task be</span><br><span>               // reset, then do so.</span><br><span>               if ((r.intent.getFlags() &amp; Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED) != 0) {</span><br><span>                   resetTaskIfNeededLocked(r, r);</span><br><span>                   doShow = topRunningNonDelayedActivityLocked(null) == r;</span><br><span>               }</span><br><span>           } else if (options != null &amp;&amp; options.getAnimationType()</span><br><span>                   == ActivityOptions.ANIM_SCENE_TRANSITION) {</span><br><span>               doShow = false;</span><br><span>           }</span><br><span>           if (r.mLaunchTaskBehind) {</span><br><span>               // Don't do a starting window for mLaunchTaskBehind. More importantly make sure we</span><br><span>               // tell WindowManager that r is visible even though it is at the back of the stack.</span><br><span>               r.setVisibility(true);</span><br><span>               ensureActivitiesVisibleLocked(null, 0, !PRESERVE_WINDOWS);</span><br><span>           } else if (SHOW_APP_STARTING_PREVIEW &amp;&amp; doShow) {</span><br><span>               // Figure out if we are transitioning from another activity that is</span><br><span>               // "has the same starting icon" as the next one.  This allows the</span><br><span>               // window manager to keep the previous window it had previously</span><br><span>               // created, if it still had one.</span><br><span>               TaskRecord prevTask = r.getTask();</span><br><span>               ActivityRecord prev = prevTask.topRunningActivityWithStartingWindowLocked();</span><br><span>               if (prev != null) {</span><br><span>                  //当前activity属于不同的task</span><br><span>                   // We don't want to reuse the previous starting preview if:</span><br><span>                   // (1) The current activity is in a different task.</span><br><span>                   if (prev.getTask() != prevTask) {</span><br><span>                       prev = null;</span><br><span>                   }</span><br><span>                   //当前activity已经display</span><br><span>                   // (2) The current activity is already displayed.</span><br><span>                   else if (prev.nowVisible) {</span><br><span>                       prev = null;</span><br><span>                   }</span><br><span>               }</span><br><span>               r.showStartingWindow(prev, newTask, isTaskSwitch(r, focusedTopActivity));</span><br><span>           }</span><br><span>       } else {</span><br><span>           // If this is the first activity, don't do any fancy animations,</span><br><span>           // because there is nothing for it to animate on top of.</span><br><span>           ActivityOptions.abort(options);</span><br><span>       }</span><br><span>   }</span><br></pre></td></tr></tbody></table>

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-11-ASS-resumeFocusedStackTopActivityLocked "2.11  ASS.resumeFocusedStackTopActivityLocked")2.11 ASS.resumeFocusedStackTopActivityLocked

\[->ActivityStackSupervisor.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br></pre></td><td><pre><span>boolean resumeFocusedStackTopActivityLocked(</span><br><span>           ActivityStack targetStack, ActivityRecord target, ActivityOptions targetOptions) {</span><br><span></span><br><span>       if (!readyToResume()) {</span><br><span>           return false;</span><br><span>       }</span><br><span></span><br><span>       if (targetStack != null &amp;&amp; isFocusedStack(targetStack)) {</span><br><span>           return targetStack.resumeTopActivityUncheckedLocked(target, targetOptions);</span><br><span>       }</span><br><span></span><br><span>       final ActivityRecord r = mFocusedStack.topRunningActivityLocked();</span><br><span>       if (r == null || !r.isState(RESUMED)) {</span><br><span>           mFocusedStack.resumeTopActivityUncheckedLocked(null, null);</span><br><span>       } else if (r.isState(RESUMED)) {</span><br><span>           // Kick off any lingering app transitions form the MoveTaskToFront operation.</span><br><span>           mFocusedStack.executeAppTransition(targetOptions);</span><br><span>       }</span><br><span></span><br><span>       return false;</span><br><span>   }</span><br></pre></td></tr></tbody></table>

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-12-AS-resumeTopActivityUncheckedLocked "2.12 AS.resumeTopActivityUncheckedLocked")2.12 AS.resumeTopActivityUncheckedLocked

\[->ActivityStack.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br></pre></td><td><pre><span>boolean resumeTopActivityUncheckedLocked(ActivityRecord prev, ActivityOptions options) {</span><br><span>      //防止递归启动</span><br><span>      if (mStackSupervisor.inResumeTopActivity) {</span><br><span>          // Don't even start recursing.</span><br><span>          return false;</span><br><span>      }</span><br><span></span><br><span>      boolean result = false;</span><br><span>      try {</span><br><span>          // Protect against recursion.</span><br><span>          mStackSupervisor.inResumeTopActivity = true;</span><br><span>          //见2.13节</span><br><span>          result = resumeTopActivityInnerLocked(prev, options);</span><br><span></span><br><span>          // When resuming the top activity, it may be necessary to pause the top activity (for</span><br><span>          // example, returning to the lock screen. We suppress the normal pause logic in</span><br><span>          // {@link #resumeTopActivityUncheckedLocked}, since the top activity is resumed at the</span><br><span>          // end. We call the {@link ActivityStackSupervisor#checkReadyForSleepLocked} again here</span><br><span>          // to ensure any necessary pause logic occurs. In the case where the Activity will be</span><br><span>          // shown regardless of the lock screen, the call to</span><br><span>          // {@link ActivityStackSupervisor#checkReadyForSleepLocked} is skipped.</span><br><span>          final ActivityRecord next = topRunningActivityLocked(true /* focusableOnly */);</span><br><span>          if (next == null || !next.canTurnScreenOn()) {</span><br><span>              checkReadyForSleep();</span><br><span>          }</span><br><span>      } finally {</span><br><span>          mStackSupervisor.inResumeTopActivity = false;</span><br><span>      }</span><br><span></span><br><span>      return result;</span><br><span>  }</span><br></pre></td></tr></tbody></table>

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-13-AS-resumeTopActivityInnerLocked "2.13 AS.resumeTopActivityInnerLocked")2.13 AS.resumeTopActivityInnerLocked

\[->ActivityStack.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br><span>40</span><br><span>41</span><br><span>42</span><br><span>43</span><br><span>44</span><br><span>45</span><br><span>46</span><br><span>47</span><br><span>48</span><br><span>49</span><br><span>50</span><br><span>51</span><br><span>52</span><br><span>53</span><br><span>54</span><br><span>55</span><br><span>56</span><br><span>57</span><br><span>58</span><br><span>59</span><br><span>60</span><br><span>61</span><br><span>62</span><br><span>63</span><br><span>64</span><br><span>65</span><br><span>66</span><br><span>67</span><br><span>68</span><br><span>69</span><br><span>70</span><br><span>71</span><br><span>72</span><br><span>73</span><br><span>74</span><br><span>75</span><br><span>76</span><br><span>77</span><br><span>78</span><br><span>79</span><br><span>80</span><br><span>81</span><br><span>82</span><br><span>83</span><br><span>84</span><br><span>85</span><br><span>86</span><br><span>87</span><br><span>88</span><br><span>89</span><br><span>90</span><br><span>91</span><br><span>92</span><br><span>93</span><br><span>94</span><br><span>95</span><br><span>96</span><br><span>97</span><br><span>98</span><br><span>99</span><br><span>100</span><br><span>101</span><br><span>102</span><br><span>103</span><br><span>104</span><br><span>105</span><br><span>106</span><br><span>107</span><br><span>108</span><br><span>109</span><br><span>110</span><br><span>111</span><br><span>112</span><br><span>113</span><br><span>114</span><br><span>115</span><br><span>116</span><br><span>117</span><br><span>118</span><br><span>119</span><br><span>120</span><br><span>121</span><br><span>122</span><br><span>123</span><br><span>124</span><br><span>125</span><br><span>126</span><br><span>127</span><br><span>128</span><br><span>129</span><br><span>130</span><br><span>131</span><br><span>132</span><br><span>133</span><br><span>134</span><br><span>135</span><br><span>136</span><br><span>137</span><br><span>138</span><br><span>139</span><br><span>140</span><br><span>141</span><br><span>142</span><br><span>143</span><br><span>144</span><br><span>145</span><br><span>146</span><br><span>147</span><br><span>148</span><br><span>149</span><br><span>150</span><br><span>151</span><br><span>152</span><br><span>153</span><br><span>154</span><br><span>155</span><br><span>156</span><br><span>157</span><br><span>158</span><br><span>159</span><br><span>160</span><br><span>161</span><br><span>162</span><br><span>163</span><br><span>164</span><br><span>165</span><br><span>166</span><br><span>167</span><br><span>168</span><br><span>169</span><br><span>170</span><br><span>171</span><br><span>172</span><br><span>173</span><br><span>174</span><br><span>175</span><br><span>176</span><br><span>177</span><br><span>178</span><br><span>179</span><br><span>180</span><br><span>181</span><br><span>182</span><br><span>183</span><br><span>184</span><br><span>185</span><br><span>186</span><br><span>187</span><br><span>188</span><br><span>189</span><br><span>190</span><br><span>191</span><br><span>192</span><br><span>193</span><br><span>194</span><br><span>195</span><br><span>196</span><br><span>197</span><br><span>198</span><br><span>199</span><br><span>200</span><br><span>201</span><br><span>202</span><br><span>203</span><br><span>204</span><br><span>205</span><br><span>206</span><br><span>207</span><br><span>208</span><br><span>209</span><br><span>210</span><br><span>211</span><br><span>212</span><br><span>213</span><br><span>214</span><br><span>215</span><br><span>216</span><br><span>217</span><br><span>218</span><br><span>219</span><br><span>220</span><br><span>221</span><br><span>222</span><br><span>223</span><br><span>224</span><br><span>225</span><br><span>226</span><br><span>227</span><br><span>228</span><br><span>229</span><br><span>230</span><br><span>231</span><br><span>232</span><br><span>233</span><br><span>234</span><br><span>235</span><br><span>236</span><br><span>237</span><br><span>238</span><br><span>239</span><br><span>240</span><br><span>241</span><br><span>242</span><br><span>243</span><br><span>244</span><br><span>245</span><br><span>246</span><br><span>247</span><br><span>248</span><br><span>249</span><br><span>250</span><br><span>251</span><br><span>252</span><br><span>253</span><br><span>254</span><br><span>255</span><br><span>256</span><br><span>257</span><br><span>258</span><br><span>259</span><br><span>260</span><br><span>261</span><br><span>262</span><br><span>263</span><br><span>264</span><br><span>265</span><br><span>266</span><br><span>267</span><br><span>268</span><br><span>269</span><br><span>270</span><br><span>271</span><br><span>272</span><br><span>273</span><br><span>274</span><br><span>275</span><br><span>276</span><br><span>277</span><br><span>278</span><br><span>279</span><br><span>280</span><br><span>281</span><br><span>282</span><br><span>283</span><br><span>284</span><br><span>285</span><br><span>286</span><br><span>287</span><br><span>288</span><br><span>289</span><br><span>290</span><br><span>291</span><br><span>292</span><br><span>293</span><br><span>294</span><br><span>295</span><br><span>296</span><br><span>297</span><br><span>298</span><br><span>299</span><br><span>300</span><br><span>301</span><br><span>302</span><br><span>303</span><br><span>304</span><br><span>305</span><br><span>306</span><br><span>307</span><br><span>308</span><br><span>309</span><br><span>310</span><br><span>311</span><br><span>312</span><br><span>313</span><br><span>314</span><br><span>315</span><br><span>316</span><br><span>317</span><br><span>318</span><br><span>319</span><br><span>320</span><br><span>321</span><br><span>322</span><br><span>323</span><br><span>324</span><br><span>325</span><br><span>326</span><br><span>327</span><br><span>328</span><br><span>329</span><br><span>330</span><br><span>331</span><br><span>332</span><br><span>333</span><br><span>334</span><br><span>335</span><br><span>336</span><br><span>337</span><br><span>338</span><br><span>339</span><br><span>340</span><br><span>341</span><br><span>342</span><br><span>343</span><br><span>344</span><br><span>345</span><br><span>346</span><br><span>347</span><br><span>348</span><br><span>349</span><br><span>350</span><br><span>351</span><br><span>352</span><br><span>353</span><br><span>354</span><br><span>355</span><br><span>356</span><br><span>357</span><br><span>358</span><br><span>359</span><br><span>360</span><br><span>361</span><br><span>362</span><br><span>363</span><br><span>364</span><br><span>365</span><br><span>366</span><br><span>367</span><br><span>368</span><br><span>369</span><br><span>370</span><br><span>371</span><br><span>372</span><br><span>373</span><br><span>374</span><br><span>375</span><br><span>376</span><br><span>377</span><br><span>378</span><br><span>379</span><br><span>380</span><br><span>381</span><br><span>382</span><br><span>383</span><br><span>384</span><br><span>385</span><br><span>386</span><br><span>387</span><br><span>388</span><br><span>389</span><br><span>390</span><br><span>391</span><br><span>392</span><br><span>393</span><br><span>394</span><br><span>395</span><br><span>396</span><br><span>397</span><br><span>398</span><br><span>399</span><br><span>400</span><br><span>401</span><br><span>402</span><br><span>403</span><br><span>404</span><br><span>405</span><br><span>406</span><br><span>407</span><br><span>408</span><br><span>409</span><br><span>410</span><br><span>411</span><br><span>412</span><br><span>413</span><br><span>414</span><br><span>415</span><br><span>416</span><br><span>417</span><br><span>418</span><br><span>419</span><br><span>420</span><br><span>421</span><br><span>422</span><br><span>423</span><br><span>424</span><br><span>425</span><br><span>426</span><br><span>427</span><br><span>428</span><br><span>429</span><br><span>430</span><br><span>431</span><br><span>432</span><br><span>433</span><br><span>434</span><br><span>435</span><br><span>436</span><br><span>437</span><br><span>438</span><br><span>439</span><br><span>440</span><br><span>441</span><br><span>442</span><br><span>443</span><br><span>444</span><br><span>445</span><br></pre></td><td><pre><span>@GuardedBy("mService")</span><br><span>   private boolean resumeTopActivityInnerLocked(ActivityRecord prev, ActivityOptions options) {</span><br><span>       //系统没有进入booting或者booted状态被，则不允许启动Activity</span><br><span>       if (!mService.mBooting &amp;&amp; !mService.mBooted) {</span><br><span>           // Not ready yet!</span><br><span>           return false;</span><br><span>       }</span><br><span></span><br><span>       //找到top-most activity没有finishing的栈顶activity</span><br><span>       // Find the next top-most activity to resume in this stack that is not finishing and is</span><br><span>       // focusable. If it is not focusable, we will fall into the case below to resume the</span><br><span>       // top activity in the next focusable task.</span><br><span>       final ActivityRecord next = topRunningActivityLocked(true /* focusableOnly */);</span><br><span></span><br><span>       final boolean hasRunningActivity = next != null;</span><br><span></span><br><span>       // TODO: Maybe this entire condition can get removed?</span><br><span>       if (hasRunningActivity &amp;&amp; !isAttached()) {</span><br><span>           return false;</span><br><span>       }</span><br><span>       //top running之后的任意处于初始化状态且有显示startingWindow,则移除startingWindow</span><br><span>       mStackSupervisor.cancelInitializingActivities();</span><br><span></span><br><span>       // Remember how we'll process this pause/resume situation, and ensure</span><br><span>       // that the state is reset however we wind up proceeding.</span><br><span>       boolean userLeaving = mStackSupervisor.mUserLeaving;</span><br><span>       mStackSupervisor.mUserLeaving = false;</span><br><span></span><br><span>       if (!hasRunningActivity) {</span><br><span>           //见2.13.1节</span><br><span>           // There are no activities left in the stack, let's look somewhere else.</span><br><span>           return resumeTopActivityInNextFocusableStack(prev, options, "noMoreActivities");</span><br><span>       }</span><br><span></span><br><span>       next.delayedResume = false;</span><br><span>       //已经resume的情况</span><br><span>       // If the top activity is the resumed one, nothing to do.</span><br><span>       if (mResumedActivity == next &amp;&amp; next.isState(RESUMED)</span><br><span>               &amp;&amp; mStackSupervisor.allResumedActivitiesComplete()) {</span><br><span>           // Make sure we have executed any pending transitions, since there</span><br><span>           // should be nothing left to do at this point.</span><br><span>           executeAppTransition(options);</span><br><span>           if (DEBUG_STATES) Slog.d(TAG_STATES,</span><br><span>                   "resumeTopActivityLocked: Top activity resumed " + next);</span><br><span>           if (DEBUG_STACK) mStackSupervisor.validateTopActivitiesLocked();</span><br><span>           return false;</span><br><span>       }</span><br><span>       //处于睡眠或者关机状态，top activity已经暂停的情况</span><br><span>       // If we are sleeping, and there is no resumed activity, and the top</span><br><span>       // activity is paused, well that is the state we want.</span><br><span>       if (shouldSleepOrShutDownActivities()</span><br><span>               &amp;&amp; mLastPausedActivity == next</span><br><span>               &amp;&amp; mStackSupervisor.allPausedActivitiesComplete()) {</span><br><span>           // Make sure we have executed any pending transitions, since there</span><br><span>           // should be nothing left to do at this point.</span><br><span>           executeAppTransition(options);</span><br><span>           if (DEBUG_STATES) Slog.d(TAG_STATES,</span><br><span>                   "resumeTopActivityLocked: Going to sleep and all paused");</span><br><span>           if (DEBUG_STACK) mStackSupervisor.validateTopActivitiesLocked();</span><br><span>           return false;</span><br><span>       }</span><br><span>       //拥有该activity的用户没有启动则直接返回</span><br><span>       // Make sure that the user who owns this activity is started.  If not,</span><br><span>       // we will just leave it as is because someone should be bringing</span><br><span>       // another user's activities to the top of the stack.</span><br><span>       if (!mService.mUserController.hasStartedUserState(next.userId)) {</span><br><span>           Slog.w(TAG, "Skipping resume of top activity " + next</span><br><span>                   + ": user " + next.userId + " is stopped");</span><br><span>           if (DEBUG_STACK) mStackSupervisor.validateTopActivitiesLocked();</span><br><span>           return false;</span><br><span>       }</span><br><span>       </span><br><span>       // The activity may be waiting for stop, but that is no longer</span><br><span>       // appropriate for it.</span><br><span>       mStackSupervisor.mStoppingActivities.remove(next);</span><br><span>       mStackSupervisor.mGoingToSleepActivities.remove(next);</span><br><span>       next.sleeping = false;</span><br><span>       mStackSupervisor.mActivitiesWaitingForVisibleActivity.remove(next);</span><br><span></span><br><span>       if (DEBUG_SWITCH) Slog.v(TAG_SWITCH, "Resuming " + next);</span><br><span>       //当处于暂停activity，则直接返回</span><br><span>       // If we are currently pausing an activity, then don't do anything until that is done.</span><br><span>       if (!mStackSupervisor.allPausedActivitiesComplete()) {</span><br><span>           if (DEBUG_SWITCH || DEBUG_PAUSE || DEBUG_STATES) Slog.v(TAG_PAUSE,</span><br><span>                   "resumeTopActivityLocked: Skip resume: some activity pausing.");</span><br><span>           if (DEBUG_STACK) mStackSupervisor.validateTopActivitiesLocked();</span><br><span>           return false;</span><br><span>       }</span><br><span></span><br><span>       mStackSupervisor.setLaunchSource(next.info.applicationInfo.uid);</span><br><span></span><br><span>       boolean lastResumedCanPip = false;</span><br><span>       ActivityRecord lastResumed = null;</span><br><span>       final ActivityStack lastFocusedStack = mStackSupervisor.getLastStack();</span><br><span>       if (lastFocusedStack != null &amp;&amp; lastFocusedStack != this) {</span><br><span>           // So, why aren't we using prev here??? See the param comment on the method. prev doesn't</span><br><span>           // represent the last resumed activity. However, the last focus stack does if it isn't null.</span><br><span>           lastResumed = lastFocusedStack.mResumedActivity;</span><br><span>           //多窗口模式判断</span><br><span>           if (userLeaving &amp;&amp; inMultiWindowMode() &amp;&amp; lastFocusedStack.shouldBeVisible(next)) {</span><br><span>               // The user isn't leaving if this stack is the multi-window mode and the last</span><br><span>               // focused stack should still be visible.</span><br><span>               if(DEBUG_USER_LEAVING) Slog.i(TAG_USER_LEAVING, "Overriding userLeaving to false"</span><br><span>                       + " next=" + next + " lastResumed=" + lastResumed);</span><br><span>               userLeaving = false;</span><br><span>           }</span><br><span>           lastResumedCanPip = lastResumed != null &amp;&amp; lastResumed.checkEnterPictureInPictureState(</span><br><span>                   "resumeTopActivity", userLeaving /* beforeStopping */);</span><br><span>       }</span><br><span>       //要等待暂停当前activity完成，再resume top activity</span><br><span>       // If the flag RESUME_WHILE_PAUSING is set, then continue to schedule the previous activity</span><br><span>       // to be paused, while at the same time resuming the new resume activity only if the</span><br><span>       // previous activity can't go into Pip since we want to give Pip activities a chance to</span><br><span>       // enter Pip before resuming the next activity.</span><br><span>       final boolean resumeWhilePausing = (next.info.flags &amp; FLAG_RESUME_WHILE_PAUSING) != 0</span><br><span>               &amp;&amp; !lastResumedCanPip;</span><br><span>       </span><br><span>       //暂停其他Activity，见13.2节</span><br><span>       boolean pausing = mStackSupervisor.pauseBackStacks(userLeaving, next, false);</span><br><span>       if (mResumedActivity != null) {</span><br><span>           if (DEBUG_STATES) Slog.d(TAG_STATES,</span><br><span>                   "resumeTopActivityLocked: Pausing " + mResumedActivity);</span><br><span>           //当resume状态activity不为空，则需要暂停该Activity</span><br><span>           pausing |= startPausingLocked(userLeaving, false, next, false);</span><br><span>       }</span><br><span>       if (pausing &amp;&amp; !resumeWhilePausing) {</span><br><span>           if (DEBUG_SWITCH || DEBUG_STATES) Slog.v(TAG_STATES,</span><br><span>                   "resumeTopActivityLocked: Skip resume: need to start pausing");</span><br><span>           // At this point we want to put the upcoming activity's process</span><br><span>           // at the top of the LRU list, since we know we will be needing it</span><br><span>           // very soon and it would be a waste to let it get killed if it</span><br><span>           // happens to be sitting towards the end.</span><br><span>           if (next.app != null &amp;&amp; next.app.thread != null) {</span><br><span>               mService.updateLruProcessLocked(next.app, true, null);</span><br><span>           }</span><br><span>           if (DEBUG_STACK) mStackSupervisor.validateTopActivitiesLocked();</span><br><span>           if (lastResumed != null) {</span><br><span>               lastResumed.setWillCloseOrEnterPip(true);</span><br><span>           }</span><br><span>           return true;</span><br><span>       } else if (mResumedActivity == next &amp;&amp; next.isState(RESUMED)</span><br><span>               &amp;&amp; mStackSupervisor.allResumedActivitiesComplete()) {</span><br><span>           // It is possible for the activity to be resumed when we paused back stacks above if the</span><br><span>           // next activity doesn't have to wait for pause to complete.</span><br><span>           // So, nothing else to-do except:</span><br><span>           // Make sure we have executed any pending transitions, since there</span><br><span>           // should be nothing left to do at this point.</span><br><span>           executeAppTransition(options);</span><br><span>           if (DEBUG_STATES) Slog.d(TAG_STATES,</span><br><span>                   "resumeTopActivityLocked: Top activity resumed (dontWaitForPause) " + next);</span><br><span>           if (DEBUG_STACK) mStackSupervisor.validateTopActivitiesLocked();</span><br><span>           return true;</span><br><span>       }</span><br><span></span><br><span>       // If the most recent activity was noHistory but was only stopped rather</span><br><span>       // than stopped+finished because the device went to sleep, we need to make</span><br><span>       // sure to finish it as we're making a new activity topmost.</span><br><span>       if (shouldSleepActivities() &amp;&amp; mLastNoHistoryActivity != null &amp;&amp;</span><br><span>               !mLastNoHistoryActivity.finishing) {</span><br><span>           if (DEBUG_STATES) Slog.d(TAG_STATES,</span><br><span>                   "no-history finish of " + mLastNoHistoryActivity + " on new resume");</span><br><span>           requestFinishActivityLocked(mLastNoHistoryActivity.appToken, Activity.RESULT_CANCELED,</span><br><span>                   null, "resume-no-history", false);</span><br><span>           mLastNoHistoryActivity = null;</span><br><span>       }</span><br><span></span><br><span>       if (prev != null &amp;&amp; prev != next) {</span><br><span>           if (!mStackSupervisor.mActivitiesWaitingForVisibleActivity.contains(prev)</span><br><span>                   &amp;&amp; next != null &amp;&amp; !next.nowVisible</span><br><span>                   &amp;&amp; checkKeyguardVisibility(next, true /* shouldBeVisible */,</span><br><span>                           next.isTopRunningActivity())) {</span><br><span>               mStackSupervisor.mActivitiesWaitingForVisibleActivity.add(prev);</span><br><span>               if (DEBUG_SWITCH) Slog.v(TAG_SWITCH,</span><br><span>                       "Resuming top, waiting visible to hide: " + prev);</span><br><span>           } else {</span><br><span>               // The next activity is already visible, so hide the previous</span><br><span>               // activity's windows right now so we can show the new one ASAP.</span><br><span>               // We only do this if the previous is finishing, which should mean</span><br><span>               // it is on top of the one being resumed so hiding it quickly</span><br><span>               // is good.  Otherwise, we want to do the normal route of allowing</span><br><span>               // the resumed activity to be shown so we can decide if the</span><br><span>               // previous should actually be hidden depending on whether the</span><br><span>               // new one is found to be full-screen or not.</span><br><span>               if (prev.finishing) {</span><br><span>                   prev.setVisibility(false);</span><br><span>                   if (DEBUG_SWITCH) Slog.v(TAG_SWITCH,</span><br><span>                           "Not waiting for visible to hide: " + prev + ", waitingVisible="</span><br><span>                           + mStackSupervisor.mActivitiesWaitingForVisibleActivity.contains(prev)</span><br><span>                           + ", nowVisible=" + next.nowVisible);</span><br><span>               } else {</span><br><span>                   if (DEBUG_SWITCH) Slog.v(TAG_SWITCH,</span><br><span>                           "Previous already visible but still waiting to hide: " + prev</span><br><span>                           + ", waitingVisible="</span><br><span>                           + mStackSupervisor.mActivitiesWaitingForVisibleActivity.contains(prev)</span><br><span>                           + ", nowVisible=" + next.nowVisible);</span><br><span>               }</span><br><span>           }</span><br><span>       }</span><br><span></span><br><span>       // Launching this app's activity, make sure the app is no longer</span><br><span>       // considered stopped.</span><br><span>       try {</span><br><span>           AppGlobals.getPackageManager().setPackageStoppedState(</span><br><span>                   next.packageName, false, next.userId); /* TODO: Verify if correct userid */</span><br><span>       } catch (RemoteException e1) {</span><br><span>       } catch (IllegalArgumentException e) {</span><br><span>           Slog.w(TAG, "Failed trying to unstop package "</span><br><span>                   + next.packageName + ": " + e);</span><br><span>       }</span><br><span></span><br><span>       // We are starting up the next activity, so tell the window manager</span><br><span>       // that the previous one will be hidden soon.  This way it can know</span><br><span>       // to ignore it when computing the desired screen orientation.</span><br><span>       boolean anim = true;</span><br><span>       if (prev != null) {</span><br><span>           if (prev.finishing) {</span><br><span>               if (DEBUG_TRANSITION) Slog.v(TAG_TRANSITION,</span><br><span>                       "Prepare close transition: prev=" + prev);</span><br><span>               if (mStackSupervisor.mNoAnimActivities.contains(prev)) {</span><br><span>                   anim = false;</span><br><span>                   mWindowManager.prepareAppTransition(TRANSIT_NONE, false);</span><br><span>               } else {</span><br><span>                   mWindowManager.prepareAppTransition(prev.getTask() == next.getTask()</span><br><span>                           ? TRANSIT_ACTIVITY_CLOSE</span><br><span>                           : TRANSIT_TASK_CLOSE, false);</span><br><span>               }</span><br><span>               prev.setVisibility(false);</span><br><span>           } else {</span><br><span>               if (DEBUG_TRANSITION) Slog.v(TAG_TRANSITION,</span><br><span>                       "Prepare open transition: prev=" + prev);</span><br><span>               if (mStackSupervisor.mNoAnimActivities.contains(next)) {</span><br><span>                   anim = false;</span><br><span>                   mWindowManager.prepareAppTransition(TRANSIT_NONE, false);</span><br><span>               } else {</span><br><span>                   mWindowManager.prepareAppTransition(prev.getTask() == next.getTask()</span><br><span>                           ? TRANSIT_ACTIVITY_OPEN</span><br><span>                           : next.mLaunchTaskBehind</span><br><span>                                   ? TRANSIT_TASK_OPEN_BEHIND</span><br><span>                                   : TRANSIT_TASK_OPEN, false);</span><br><span>               }</span><br><span>           }</span><br><span>       } else {</span><br><span>           if (DEBUG_TRANSITION) Slog.v(TAG_TRANSITION, "Prepare open transition: no previous");</span><br><span>           if (mStackSupervisor.mNoAnimActivities.contains(next)) {</span><br><span>               anim = false;</span><br><span>               mWindowManager.prepareAppTransition(TRANSIT_NONE, false);</span><br><span>           } else {</span><br><span>               mWindowManager.prepareAppTransition(TRANSIT_ACTIVITY_OPEN, false);</span><br><span>           }</span><br><span>       }</span><br><span></span><br><span>       if (anim) {</span><br><span>           next.applyOptionsLocked();</span><br><span>       } else {</span><br><span>           next.clearOptionsLocked();</span><br><span>       }</span><br><span></span><br><span>       mStackSupervisor.mNoAnimActivities.clear();</span><br><span>       ActivityStack lastStack = mStackSupervisor.getLastStack();</span><br><span>       //进程存在的情况</span><br><span>       if (next.app != null &amp;&amp; next.app.thread != null) {</span><br><span>           if (DEBUG_SWITCH) Slog.v(TAG_SWITCH, "Resume running: " + next</span><br><span>                   + " stopped=" + next.stopped + " visible=" + next.visible);</span><br><span></span><br><span>           // If the previous activity is translucent, force a visibility update of</span><br><span>           // the next activity, so that it's added to WM's opening app list, and</span><br><span>           // transition animation can be set up properly.</span><br><span>           // For example, pressing Home button with a translucent activity in focus.</span><br><span>           // Launcher is already visible in this case. If we don't add it to opening</span><br><span>           // apps, maybeUpdateTransitToWallpaper() will fail to identify this as a</span><br><span>           // TRANSIT_WALLPAPER_OPEN animation, and run some funny animation.</span><br><span>           final boolean lastActivityTranslucent = lastStack != null</span><br><span>                   &amp;&amp; (lastStack.inMultiWindowMode()</span><br><span>                   || (lastStack.mLastPausedActivity != null</span><br><span>                   &amp;&amp; !lastStack.mLastPausedActivity.fullscreen));</span><br><span></span><br><span>           // The contained logic must be synchronized, since we are both changing the visibility</span><br><span>           // and updating the {@link Configuration}. {@link ActivityRecord#setVisibility} will</span><br><span>           // ultimately cause the client code to schedule a layout. Since layouts retrieve the</span><br><span>           // current {@link Configuration}, we must ensure that the below code updates it before</span><br><span>           // the layout can occur.</span><br><span>           synchronized(mWindowManager.getWindowManagerLock()) {</span><br><span>               //设置activity可见</span><br><span>               // This activity is now becoming visible.</span><br><span>               if (!next.visible || next.stopped || lastActivityTranslucent) {</span><br><span>                   next.setVisibility(true);</span><br><span>               }</span><br><span></span><br><span>               // schedule launch ticks to collect information about slow apps.</span><br><span>               next.startLaunchTickingLocked();</span><br><span></span><br><span>               ActivityRecord lastResumedActivity =</span><br><span>                       lastStack == null ? null :lastStack.mResumedActivity;</span><br><span>               final ActivityState lastState = next.getState();</span><br><span></span><br><span>               mService.updateCpuStats();</span><br><span></span><br><span>               if (DEBUG_STATES) Slog.v(TAG_STATES, "Moving to RESUMED: " + next</span><br><span>                       + " (in existing)");</span><br><span>              //设置activity resume</span><br><span>               next.setState(RESUMED, "resumeTopActivityInnerLocked");</span><br><span></span><br><span>               mService.updateLruProcessLocked(next.app, true, null);</span><br><span>               updateLRUListLocked(next);</span><br><span>               mService.updateOomAdjLocked();</span><br><span></span><br><span>               // Have the window manager re-evaluate the orientation of</span><br><span>               // the screen based on the new activity order.</span><br><span>               boolean notUpdated = true;</span><br><span></span><br><span>               if (mStackSupervisor.isFocusedStack(this)) {</span><br><span>                   // We have special rotation behavior when here is some active activity that</span><br><span>                   // requests specific orientation or Keyguard is locked. Make sure all activity</span><br><span>                   // visibilities are set correctly as well as the transition is updated if needed</span><br><span>                   // to get the correct rotation behavior. Otherwise the following call to update</span><br><span>                   // the orientation may cause incorrect configurations delivered to client as a</span><br><span>                   // result of invisible window resize.</span><br><span>                   // TODO: Remove this once visibilities are set correctly immediately when</span><br><span>                   // starting an activity.</span><br><span>                   notUpdated = !mStackSupervisor.ensureVisibilityAndConfig(next, mDisplayId,</span><br><span>                           true /* markFrozenIfConfigChanged */, false /* deferResume */);</span><br><span>               }</span><br><span></span><br><span>               if (notUpdated) {</span><br><span>                   // The configuration update wasn't able to keep the existing</span><br><span>                   // instance of the activity, and instead started a new one.</span><br><span>                   // We should be all done, but let's just make sure our activity</span><br><span>                   // is still at the top and schedule another run if something</span><br><span>                   // weird happened.</span><br><span>                   ActivityRecord nextNext = topRunningActivityLocked();</span><br><span>                   if (DEBUG_SWITCH || DEBUG_STATES) Slog.i(TAG_STATES,</span><br><span>                           "Activity config changed during resume: " + next</span><br><span>                                   + ", new next: " + nextNext);</span><br><span>                   if (nextNext != next) {</span><br><span>                       // Do over!</span><br><span>                       mStackSupervisor.scheduleResumeTopActivities();</span><br><span>                   }</span><br><span>                   if (!next.visible || next.stopped) {</span><br><span>                       next.setVisibility(true);</span><br><span>                   }</span><br><span>                   next.completeResumeLocked();</span><br><span>                   if (DEBUG_STACK) mStackSupervisor.validateTopActivitiesLocked();</span><br><span>                   return true;</span><br><span>               }</span><br><span></span><br><span>               try {</span><br><span>                   //分发所有pending结果</span><br><span>                   final ClientTransaction transaction = ClientTransaction.obtain(next.app.thread,</span><br><span>                           next.appToken);</span><br><span>                   // Deliver all pending results.</span><br><span>                   ArrayList&lt;ResultInfo&gt; a = next.results;</span><br><span>                   if (a != null) {</span><br><span>                       final int N = a.size();</span><br><span>                       if (!next.finishing &amp;&amp; N &gt; 0) {</span><br><span>                           if (DEBUG_RESULTS) Slog.v(TAG_RESULTS,</span><br><span>                                   "Delivering results to " + next + ": " + a);</span><br><span>                           transaction.addCallback(ActivityResultItem.obtain(a));</span><br><span>                       }</span><br><span>                   }</span><br><span></span><br><span>                   if (next.newIntents != null) {</span><br><span>                       transaction.addCallback(NewIntentItem.obtain(next.newIntents,</span><br><span>                               false /* andPause */));</span><br><span>                   }</span><br><span></span><br><span>                   // Well the app will no longer be stopped.</span><br><span>                   // Clear app token stopped state in window manager if needed.</span><br><span>                   next.notifyAppResumed(next.stopped);</span><br><span></span><br><span>                   EventLog.writeEvent(EventLogTags.AM_RESUME_ACTIVITY, next.userId,</span><br><span>                           System.identityHashCode(next), next.getTask().taskId,</span><br><span>                           next.shortComponentName);</span><br><span></span><br><span>                   next.sleeping = false;</span><br><span>                   mService.getAppWarningsLocked().onResumeActivity(next);</span><br><span>                   mService.showAskCompatModeDialogLocked(next);</span><br><span>                   next.app.pendingUiClean = true;</span><br><span>                   next.app.forceProcessStateUpTo(mService.mTopProcessState);</span><br><span>                   next.clearOptionsLocked();</span><br><span>                   //处罚onResume</span><br><span>                   transaction.setLifecycleStateRequest(</span><br><span>                           ResumeActivityItem.obtain(next.app.repProcState,</span><br><span>                                   mService.isNextTransitionForward()));</span><br><span>                   mService.getLifecycleManager().scheduleTransaction(transaction);</span><br><span></span><br><span>                   if (DEBUG_STATES) Slog.d(TAG_STATES, "resumeTopActivityLocked: Resumed "</span><br><span>                           + next);</span><br><span>               } catch (Exception e) {</span><br><span>                   // Whoops, need to restart this activity!</span><br><span>                   if (DEBUG_STATES) Slog.v(TAG_STATES, "Resume failed; resetting state to "</span><br><span>                           + lastState + ": " + next);</span><br><span>                   next.setState(lastState, "resumeTopActivityInnerLocked");</span><br><span></span><br><span>                   // lastResumedActivity being non-null implies there is a lastStack present.</span><br><span>                   if (lastResumedActivity != null) {</span><br><span>                       lastResumedActivity.setState(RESUMED, "resumeTopActivityInnerLocked");</span><br><span>                   }</span><br><span></span><br><span>                   Slog.i(TAG, "Restarting because process died: " + next);</span><br><span>                   if (!next.hasBeenLaunched) {</span><br><span>                       next.hasBeenLaunched = true;</span><br><span>                   } else  if (SHOW_APP_STARTING_PREVIEW &amp;&amp; lastStack != null</span><br><span>                           &amp;&amp; lastStack.isTopStackOnDisplay()) {</span><br><span>                       next.showStartingWindow(null /* prev */, false /* newTask */,</span><br><span>                               false /* taskSwitch */);</span><br><span>                   }</span><br><span>                   mStackSupervisor.startSpecificActivityLocked(next, true, false);</span><br><span>                   if (DEBUG_STACK) mStackSupervisor.validateTopActivitiesLocked();</span><br><span>                   return true;</span><br><span>               }</span><br><span>           }</span><br><span></span><br><span>           // From this point on, if something goes wrong there is no way</span><br><span>           // to recover the activity.</span><br><span>           try {</span><br><span>               next.completeResumeLocked();</span><br><span>           } catch (Exception e) {</span><br><span>               // If any exception gets thrown, toss away this</span><br><span>               // activity and try the next one.</span><br><span>               Slog.w(TAG, "Exception thrown during resume of " + next, e);</span><br><span>               requestFinishActivityLocked(next.appToken, Activity.RESULT_CANCELED, null,</span><br><span>                       "resume-exception", true);</span><br><span>               if (DEBUG_STACK) mStackSupervisor.validateTopActivitiesLocked();</span><br><span>               return true;</span><br><span>           }</span><br><span>       } else {</span><br><span>           //需要重新启动Activity</span><br><span>           // Whoops, need to restart this activity!</span><br><span>           if (!next.hasBeenLaunched) {</span><br><span>               next.hasBeenLaunched = true;</span><br><span>           } else {</span><br><span>               if (SHOW_APP_STARTING_PREVIEW) {</span><br><span>                   next.showStartingWindow(null /* prev */, false /* newTask */,</span><br><span>                           false /* taskSwich */);</span><br><span>               }</span><br><span>               if (DEBUG_SWITCH) Slog.v(TAG_SWITCH, "Restarting: " + next);</span><br><span>           }</span><br><span>           if (DEBUG_STATES) Slog.d(TAG_STATES, "resumeTopActivityLocked: Restarting " + next);</span><br><span>           //见2.14节</span><br><span>           mStackSupervisor.startSpecificActivityLocked(next, true, true);</span><br><span>       }</span><br><span></span><br><span>       if (DEBUG_STACK) mStackSupervisor.validateTopActivitiesLocked();</span><br><span>       return true;</span><br><span>   }</span><br></pre></td></tr></tbody></table>

主要工作如下：

-   当找不到resume的activity时，则直接回到桌面
-   当resume状态activity不为空,则执行startPausingLocked,暂停该Activity
-   当Activity之前启动过，则直接resume，否则执行startSpecificActivityLocked，2.14节将继续讨论。

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-13-1-AS-resumeTopActivityInNextFocusableStack "2.13.1 AS.resumeTopActivityInNextFocusableStack")2.13.1 AS.resumeTopActivityInNextFocusableStack

\[->ActivityStack.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br></pre></td><td><pre><span>private boolean resumeTopActivityInNextFocusableStack(ActivityRecord prev,</span><br><span>           ActivityOptions options, String reason) {</span><br><span>       if (adjustFocusToNextFocusableStack(reason)) {</span><br><span>           //如果该栈没有全屏，则尝试下一个可见的stack</span><br><span>           // Try to move focus to the next visible stack with a running activity if this</span><br><span>           // stack is not covering the entire screen or is on a secondary display (with no home</span><br><span>           // stack).</span><br><span>           return mStackSupervisor.resumeFocusedStackTopActivityLocked(</span><br><span>                   mStackSupervisor.getFocusedStack(), prev, null);</span><br><span>       }</span><br><span></span><br><span>       // Let's just start up the Launcher...</span><br><span>       ActivityOptions.abort(options);</span><br><span>       if (DEBUG_STATES) Slog.d(TAG_STATES,</span><br><span>               "resumeTopActivityInNextFocusableStack: " + reason + ", go home");</span><br><span>       if (DEBUG_STACK) mStackSupervisor.validateTopActivitiesLocked();</span><br><span>       // Only resume home if on home display</span><br><span>       //启动桌面activity</span><br><span>       return isOnHomeDisplay() &amp;&amp;</span><br><span>               mStackSupervisor.resumeHomeStackTask(prev, reason);</span><br><span>   }</span><br></pre></td></tr></tbody></table>

当找不到需要的resume的Activity时，直接回到桌面

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-13-2-AS-pauseBackStacks "2.13.2 AS.pauseBackStacks")2.13.2 AS.pauseBackStacks

\[->ActivityStack.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br></pre></td><td><pre><span>/**</span><br><span>    * Pause all activities in either all of the stacks or just the back stacks.</span><br><span>    * @param userLeaving Passed to pauseActivity() to indicate whether to call onUserLeaving().</span><br><span>    * @param resuming The resuming activity.</span><br><span>    * @param dontWait The resuming activity isn't going to wait for all activities to be paused</span><br><span>    *                 before resuming.</span><br><span>    * @return true if any activity was paused as a result of this call.</span><br><span>    */</span><br><span>   boolean pauseBackStacks(boolean userLeaving, ActivityRecord resuming, boolean dontWait) {</span><br><span>       boolean someActivityPaused = false;</span><br><span>       for (int displayNdx = mActivityDisplays.size() - 1; displayNdx &gt;= 0; --displayNdx) {</span><br><span>           final ActivityDisplay display = mActivityDisplays.valueAt(displayNdx);</span><br><span>           for (int stackNdx = display.getChildCount() - 1; stackNdx &gt;= 0; --stackNdx) {</span><br><span>               final ActivityStack stack = display.getChildAt(stackNdx);</span><br><span>               if (!isFocusedStack(stack) &amp;&amp; stack.getResumedActivity() != null) {</span><br><span>                   if (DEBUG_STATES) Slog.d(TAG_STATES, "pauseBackStacks: stack=" + stack +</span><br><span>                           " mResumedActivity=" + stack.getResumedActivity());</span><br><span>                   //见2.13.2节</span><br><span>                   someActivityPaused |= stack.startPausingLocked(userLeaving, false, resuming,</span><br><span>                           dontWait);</span><br><span>               }</span><br><span>           }</span><br><span>       }</span><br><span>       return someActivityPaused;</span><br><span>   }</span><br></pre></td></tr></tbody></table>

暂停所有处于后台栈的所有Activity

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-13-3-AS-startPausingLocked "2.13.3 AS.startPausingLocked")2.13.3 AS.startPausingLocked

\[->ActivityStack.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br><span>40</span><br><span>41</span><br><span>42</span><br><span>43</span><br><span>44</span><br><span>45</span><br><span>46</span><br><span>47</span><br><span>48</span><br><span>49</span><br><span>50</span><br><span>51</span><br><span>52</span><br><span>53</span><br><span>54</span><br><span>55</span><br><span>56</span><br><span>57</span><br><span>58</span><br><span>59</span><br><span>60</span><br><span>61</span><br><span>62</span><br><span>63</span><br><span>64</span><br><span>65</span><br><span>66</span><br><span>67</span><br><span>68</span><br><span>69</span><br><span>70</span><br><span>71</span><br><span>72</span><br><span>73</span><br><span>74</span><br><span>75</span><br><span>76</span><br><span>77</span><br><span>78</span><br><span>79</span><br><span>80</span><br><span>81</span><br><span>82</span><br><span>83</span><br><span>84</span><br><span>85</span><br><span>86</span><br><span>87</span><br><span>88</span><br><span>89</span><br><span>90</span><br><span>91</span><br><span>92</span><br><span>93</span><br><span>94</span><br><span>95</span><br><span>96</span><br><span>97</span><br><span>98</span><br><span>99</span><br><span>100</span><br><span>101</span><br><span>102</span><br><span>103</span><br></pre></td><td><pre><span>final boolean startPausingLocked(boolean userLeaving, boolean uiSleeping,</span><br><span>            ActivityRecord resuming, boolean pauseImmediately) {</span><br><span>        if (mPausingActivity != null) {</span><br><span>            Slog.wtf(TAG, "Going to pause when pause is already pending for " + mPausingActivity</span><br><span>                    + " state=" + mPausingActivity.getState());</span><br><span>            if (!shouldSleepActivities()) {</span><br><span>                // Avoid recursion among check for sleep and complete pause during sleeping.</span><br><span>                // Because activity will be paused immediately after resume, just let pause</span><br><span>                // be completed by the order of activity paused from clients.</span><br><span>                completePauseLocked(false, resuming);</span><br><span>            }</span><br><span>        }</span><br><span>        ActivityRecord prev = mResumedActivity;</span><br><span></span><br><span>        if (prev == null) {</span><br><span>            if (resuming == null) {</span><br><span>                Slog.wtf(TAG, "Trying to pause when nothing is resumed");</span><br><span>                mStackSupervisor.resumeFocusedStackTopActivityLocked();</span><br><span>            }</span><br><span>            return false;</span><br><span>        }</span><br><span></span><br><span>        if (prev == resuming) {</span><br><span>            Slog.wtf(TAG, "Trying to pause activity that is in process of being resumed");</span><br><span>            return false;</span><br><span>        }</span><br><span></span><br><span>        if (DEBUG_STATES) Slog.v(TAG_STATES, "Moving to PAUSING: " + prev);</span><br><span>        else if (DEBUG_PAUSE) Slog.v(TAG_PAUSE, "Start pausing: " + prev);</span><br><span>        mPausingActivity = prev;</span><br><span>        mLastPausedActivity = prev;</span><br><span>        mLastNoHistoryActivity = (prev.intent.getFlags() &amp; Intent.FLAG_ACTIVITY_NO_HISTORY) != 0</span><br><span>                || (prev.info.flags &amp; ActivityInfo.FLAG_NO_HISTORY) != 0 ? prev : null;</span><br><span>        prev.setState(PAUSING, "startPausingLocked");</span><br><span>        prev.getTask().touchActiveTime();</span><br><span>        clearLaunchTime(prev);</span><br><span></span><br><span>        mStackSupervisor.getActivityMetricsLogger().stopFullyDrawnTraceIfNeeded();</span><br><span></span><br><span>        mService.updateCpuStats();</span><br><span></span><br><span>        if (prev.app != null &amp;&amp; prev.app.thread != null) {</span><br><span>            if (DEBUG_PAUSE) Slog.v(TAG_PAUSE, "Enqueueing pending pause: " + prev);</span><br><span>            try {</span><br><span>                EventLogTags.writeAmPauseActivity(prev.userId, System.identityHashCode(prev),</span><br><span>                        prev.shortComponentName, "userLeaving=" + userLeaving);</span><br><span>                mService.updateUsageStats(prev, false);</span><br><span>                //暂停目标Activity</span><br><span>                mService.getLifecycleManager().scheduleTransaction(prev.app.thread, prev.appToken,</span><br><span>                        PauseActivityItem.obtain(prev.finishing, userLeaving,</span><br><span>                                prev.configChangeFlags, pauseImmediately));</span><br><span>            } catch (Exception e) {</span><br><span>                // Ignore exception, if process died other code will cleanup.</span><br><span>                Slog.w(TAG, "Exception thrown during pause", e);</span><br><span>                mPausingActivity = null;</span><br><span>                mLastPausedActivity = null;</span><br><span>                mLastNoHistoryActivity = null;</span><br><span>            }</span><br><span>        } else {</span><br><span>            mPausingActivity = null;</span><br><span>            mLastPausedActivity = null;</span><br><span>            mLastNoHistoryActivity = null;</span><br><span>        }</span><br><span></span><br><span>        // If we are not going to sleep, we want to ensure the device is</span><br><span>        // awake until the next activity is started.</span><br><span>        if (!uiSleeping &amp;&amp; !mService.isSleepingOrShuttingDownLocked()) {</span><br><span>            mStackSupervisor.acquireLaunchWakelock();</span><br><span>        }</span><br><span></span><br><span>        if (mPausingActivity != null) {</span><br><span>            // Have the window manager pause its key dispatching until the new</span><br><span>            // activity has started.  If we're pausing the activity just because</span><br><span>            // the screen is being turned off and the UI is sleeping, don't interrupt</span><br><span>            // key dispatch; the same activity will pick it up again on wakeup.</span><br><span>            if (!uiSleeping) {</span><br><span>                prev.pauseKeyDispatchingLocked();</span><br><span>            } else if (DEBUG_PAUSE) {</span><br><span>                 Slog.v(TAG_PAUSE, "Key dispatch not paused for screen off");</span><br><span>            }</span><br><span></span><br><span>            if (pauseImmediately) {</span><br><span>                // If the caller said they don't want to wait for the pause, then complete</span><br><span>                // the pause now.</span><br><span>                completePauseLocked(false, resuming);</span><br><span>                return false;</span><br><span></span><br><span>            } else {</span><br><span>                //500ms,执行暂停超时的消息</span><br><span>                schedulePauseTimeout(prev);</span><br><span>                return true;</span><br><span>            }</span><br><span></span><br><span>        } else {</span><br><span>            // This activity failed to schedule the</span><br><span>            // pause, so just treat it as being paused now.</span><br><span>            if (DEBUG_PAUSE) Slog.v(TAG_PAUSE, "Activity not running, resuming next.");</span><br><span>            if (resuming == null) {  //调度失败，则认为暂停结束开始执行resume操作</span><br><span>                mStackSupervisor.resumeFocusedStackTopActivityLocked();</span><br><span>            }</span><br><span>            return false;</span><br><span>        }</span><br><span>    }</span><br></pre></td></tr></tbody></table>

通过LifecycleManager的方式来暂停Activity操作。对于pauseImmediately= true则执行completePauseLocked操作，否则等待app通知500ms超时再执行该方法。

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-13-4-AS-completePauseLocked "2.13.4 AS.completePauseLocked")2.13.4 AS.completePauseLocked

\[->ActivityStack.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br><span>40</span><br><span>41</span><br><span>42</span><br><span>43</span><br><span>44</span><br><span>45</span><br><span>46</span><br><span>47</span><br><span>48</span><br><span>49</span><br><span>50</span><br><span>51</span><br><span>52</span><br><span>53</span><br><span>54</span><br><span>55</span><br><span>56</span><br><span>57</span><br><span>58</span><br><span>59</span><br><span>60</span><br><span>61</span><br><span>62</span><br><span>63</span><br><span>64</span><br><span>65</span><br><span>66</span><br><span>67</span><br><span>68</span><br><span>69</span><br><span>70</span><br><span>71</span><br><span>72</span><br><span>73</span><br><span>74</span><br><span>75</span><br><span>76</span><br><span>77</span><br><span>78</span><br><span>79</span><br><span>80</span><br><span>81</span><br><span>82</span><br><span>83</span><br><span>84</span><br><span>85</span><br><span>86</span><br><span>87</span><br><span>88</span><br><span>89</span><br><span>90</span><br><span>91</span><br><span>92</span><br><span>93</span><br><span>94</span><br><span>95</span><br><span>96</span><br><span>97</span><br><span>98</span><br><span>99</span><br></pre></td><td><pre><span>private void completePauseLocked(boolean resumeNext, ActivityRecord resuming) {</span><br><span>       ActivityRecord prev = mPausingActivity;</span><br><span>       if (DEBUG_PAUSE) Slog.v(TAG_PAUSE, "Complete pause: " + prev);</span><br><span></span><br><span>       if (prev != null) {</span><br><span>           prev.setWillCloseOrEnterPip(false);</span><br><span>           final boolean wasStopping = prev.isState(STOPPING);</span><br><span>           prev.setState(PAUSED, "completePausedLocked");</span><br><span>           if (prev.finishing) {</span><br><span>               if (DEBUG_PAUSE) Slog.v(TAG_PAUSE, "Executing finish of activity: " + prev);</span><br><span>               prev = finishCurrentActivityLocked(prev, FINISH_AFTER_VISIBLE, false,</span><br><span>                       "completedPausedLocked");</span><br><span>           } else if (prev.app != null) {</span><br><span>               if (DEBUG_PAUSE) Slog.v(TAG_PAUSE, "Enqueue pending stop if needed: " + prev</span><br><span>                       + " wasStopping=" + wasStopping + " visible=" + prev.visible);</span><br><span>               if (mStackSupervisor.mActivitiesWaitingForVisibleActivity.remove(prev)) {</span><br><span>                   if (DEBUG_SWITCH || DEBUG_PAUSE) Slog.v(TAG_PAUSE,</span><br><span>                           "Complete pause, no longer waiting: " + prev);</span><br><span>               }</span><br><span>               if (prev.deferRelaunchUntilPaused) {</span><br><span>                   // Complete the deferred relaunch that was waiting for pause to complete.</span><br><span>                   if (DEBUG_PAUSE) Slog.v(TAG_PAUSE, "Re-launching after pause: " + prev);</span><br><span>                   prev.relaunchActivityLocked(false /* andResume */,</span><br><span>                           prev.preserveWindowOnDeferredRelaunch);</span><br><span>               } else if (wasStopping) {</span><br><span>                   // We are also stopping, the stop request must have gone soon after the pause.</span><br><span>                   // We can't clobber it, because the stop confirmation will not be handled.</span><br><span>                   // We don't need to schedule another stop, we only need to let it happen.</span><br><span>                   prev.setState(STOPPING, "completePausedLocked");</span><br><span>               } else if (!prev.visible || shouldSleepOrShutDownActivities()) {</span><br><span>                   // Clear out any deferred client hide we might currently have.</span><br><span>                   prev.setDeferHidingClient(false);</span><br><span>                   // If we were visible then resumeTopActivities will release resources before</span><br><span>                   // stopping.</span><br><span>                   addToStopping(prev, true /* scheduleIdle */, false /* idleDelayed */);</span><br><span>               }</span><br><span>           } else {</span><br><span>               if (DEBUG_PAUSE) Slog.v(TAG_PAUSE, "App died during pause, not stopping: " + prev);</span><br><span>               prev = null;</span><br><span>           }</span><br><span>           // It is possible the activity was freezing the screen before it was paused.</span><br><span>           // In that case go ahead and remove the freeze this activity has on the screen</span><br><span>           // since it is no longer visible.</span><br><span>           if (prev != null) {</span><br><span>               prev.stopFreezingScreenLocked(true /*force*/);</span><br><span>           }</span><br><span>           mPausingActivity = null;</span><br><span>       }</span><br><span></span><br><span>       if (resumeNext) {</span><br><span>           final ActivityStack topStack = mStackSupervisor.getFocusedStack();</span><br><span>           if (!topStack.shouldSleepOrShutDownActivities()) {</span><br><span>               mStackSupervisor.resumeFocusedStackTopActivityLocked(topStack, prev, null);</span><br><span>           } else {</span><br><span>               checkReadyForSleep();</span><br><span>               ActivityRecord top = topStack.topRunningActivityLocked();</span><br><span>               if (top == null || (prev != null &amp;&amp; top != prev)) {</span><br><span>                   // If there are no more activities available to run, do resume anyway to start</span><br><span>                   // something. Also if the top activity on the stack is not the just paused</span><br><span>                   // activity, we need to go ahead and resume it to ensure we complete an</span><br><span>                   // in-flight app switch.</span><br><span>                   mStackSupervisor.resumeFocusedStackTopActivityLocked();</span><br><span>               }</span><br><span>           }</span><br><span>       }</span><br><span></span><br><span>       if (prev != null) {</span><br><span>           prev.resumeKeyDispatchingLocked();</span><br><span></span><br><span>           if (prev.app != null &amp;&amp; prev.cpuTimeAtResume &gt; 0</span><br><span>                   &amp;&amp; mService.mBatteryStatsService.isOnBattery()) {</span><br><span>               long diff = mService.mProcessCpuTracker.getCpuTimeForPid(prev.app.pid)</span><br><span>                       - prev.cpuTimeAtResume;</span><br><span>               if (diff &gt; 0) {</span><br><span>                   BatteryStatsImpl bsi = mService.mBatteryStatsService.getActiveStatistics();</span><br><span>                   synchronized (bsi) {</span><br><span>                       BatteryStatsImpl.Uid.Proc ps =</span><br><span>                               bsi.getProcessStatsLocked(prev.info.applicationInfo.uid,</span><br><span>                                       prev.info.packageName);</span><br><span>                       if (ps != null) {</span><br><span>                           ps.addForegroundTimeLocked(diff);</span><br><span>                       }</span><br><span>                   }</span><br><span>               }</span><br><span>           }</span><br><span>           prev.cpuTimeAtResume = 0; // reset it</span><br><span>       }</span><br><span></span><br><span>       // Notify when the task stack has changed, but only if visibilities changed (not just</span><br><span>       // focus). Also if there is an active pinned stack - we always want to notify it about</span><br><span>       // task stack changes, because its positioning may depend on it.</span><br><span>       if (mStackSupervisor.mAppVisibilitiesChangedSinceLastPause</span><br><span>               || getDisplay().hasPinnedStack()) {</span><br><span>           mService.mTaskChangeNotificationController.notifyTaskStackChanged();</span><br><span>           mStackSupervisor.mAppVisibilitiesChangedSinceLastPause = false;</span><br><span>       }</span><br><span></span><br><span>       mStackSupervisor.ensureActivitiesVisibleLocked(resuming, 0, !PRESERVE_WINDOWS);</span><br><span>   }</span><br></pre></td></tr></tbody></table>

暂停Activity完成后，修改暂停activity状态

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-14-ASS-startSpecificActivityLocked "2.14  ASS.startSpecificActivityLocked")2.14 ASS.startSpecificActivityLocked

\[->ActivityStackSupervisor.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br></pre></td><td><pre><span>void startSpecificActivityLocked(ActivityRecord r,</span><br><span>           boolean andResume, boolean checkConfig) {</span><br><span>       // Is this activity's application already running?</span><br><span>       ProcessRecord app = mService.getProcessRecordLocked(r.processName,</span><br><span>               r.info.applicationInfo.uid, true);</span><br><span></span><br><span>       if (app != null &amp;&amp; app.thread != null) {</span><br><span>           try {</span><br><span>               if ((r.info.flags&amp;ActivityInfo.FLAG_MULTIPROCESS) == 0</span><br><span>                       || !"android".equals(r.info.packageName)) {</span><br><span>                   // Don't add this if it is a platform component that is marked</span><br><span>                   // to run in multiple processes, because this is actually</span><br><span>                   // part of the framework so doesn't make sense to track as a</span><br><span>                   // separate apk in the process.</span><br><span>                   app.addPackage(r.info.packageName, r.info.applicationInfo.longVersionCode,</span><br><span>                           mService.mProcessStats);</span><br><span>               }</span><br><span>               //真正启动Activity，见2.18节</span><br><span>               realStartActivityLocked(r, app, andResume, checkConfig);</span><br><span>               return;</span><br><span>           } catch (RemoteException e) {</span><br><span>               Slog.w(TAG, "Exception when starting activity "</span><br><span>                       + r.intent.getComponent().flattenToShortString(), e);</span><br><span>           }</span><br><span></span><br><span>           // If a dead object exception was thrown -- fall through to</span><br><span>           // restart the application.</span><br><span>       }</span><br><span>       //当进程不存在，则创建进程</span><br><span>       mService.startProcessLocked(r.processName, r.info.applicationInfo, true, 0,</span><br><span>               "activity", r.intent.getComponent(), false, false, true);</span><br><span>   }</span><br></pre></td></tr></tbody></table>

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-15-AMS-startProcessLocked "2.15 AMS.startProcessLocked")2.15 AMS.startProcessLocked

这个过程为启动Android进程的过程，在文章Android进程启动过程解析，详细描述了startProcessLocked整个过程，创建完成新进程之后，在新进程中通过binder ipc方式后调用到AMS.attachApplicationLocked。

\[->ActivityManagerService.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br><span>40</span><br><span>41</span><br><span>42</span><br></pre></td><td><pre><span>private final boolean attachApplicationLocked(IApplicationThread thread,</span><br><span>         int pid, int callingUid, long startSeq) {</span><br><span>        ...</span><br><span>         if (app.isolatedEntryPoint != null) {</span><br><span>             // This is an isolated process which should just call an entry point instead of</span><br><span>             // being bound to an application.</span><br><span>             thread.runIsolatedEntryPoint(app.isolatedEntryPoint, app.isolatedEntryPointArgs);</span><br><span>         } else if (app.instr != null) {</span><br><span>             thread.bindApplication(processName, appInfo, providers,</span><br><span>                     app.instr.mClass,</span><br><span>                     profilerInfo, app.instr.mArguments,</span><br><span>                     app.instr.mWatcher,</span><br><span>                     app.instr.mUiAutomationConnection, testMode,</span><br><span>                     mBinderTransactionTrackingEnabled, enableTrackAllocation,</span><br><span>                     isRestrictedBackupMode || !normalMode, app.persistent,</span><br><span>                     new Configuration(getGlobalConfiguration()), app.compat,</span><br><span>                     getCommonServicesLocked(app.isolated),</span><br><span>                     mCoreSettingsObserver.getCoreSettingsLocked(),</span><br><span>                     buildSerial, isAutofillCompatEnabled);</span><br><span>         } else {</span><br><span>             thread.bindApplication(processName, appInfo, providers, null, profilerInfo,</span><br><span>                     null, null, null, testMode,</span><br><span>                     mBinderTransactionTrackingEnabled, enableTrackAllocation,</span><br><span>                     isRestrictedBackupMode || !normalMode, app.persistent,</span><br><span>                     new Configuration(getGlobalConfiguration()), app.compat,</span><br><span>                     getCommonServicesLocked(app.isolated),</span><br><span>                     mCoreSettingsObserver.getCoreSettingsLocked(),</span><br><span>                     buildSerial, isAutofillCompatEnabled);</span><br><span>         }</span><br><span>         ...</span><br><span>     // See if the top visible activity is waiting to run in this process...</span><br><span>     if (normalMode) {</span><br><span>         try {</span><br><span>             if (mStackSupervisor.attachApplicationLocked(app)) {</span><br><span>                 didSomething = true;</span><br><span>             }</span><br><span>         } catch (Exception e) {</span><br><span>             Slog.wtf(TAG, "Exception thrown launching activities in " + app, e);</span><br><span>             badApp = true;</span><br><span>         }</span><br><span>     }</span><br><span>}</span><br></pre></td></tr></tbody></table>

在bindApplication后，调用了ASS.attachApplicationLocked。

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-16-ASS-attachApplicationLocked "2.16 ASS.attachApplicationLocked")2.16 ASS.attachApplicationLocked

\[->ActivityStackSupervisor.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br></pre></td><td><pre><span>boolean attachApplicationLocked(ProcessRecord app) throws RemoteException {</span><br><span>       final String processName = app.processName;</span><br><span>       boolean didSomething = false;</span><br><span>       for (int displayNdx = mActivityDisplays.size() - 1; displayNdx &gt;= 0; --displayNdx) {</span><br><span>           final ActivityDisplay display = mActivityDisplays.valueAt(displayNdx);</span><br><span>           for (int stackNdx = display.getChildCount() - 1; stackNdx &gt;= 0; --stackNdx) {</span><br><span>               final ActivityStack stack = display.getChildAt(stackNdx);</span><br><span>               if (!isFocusedStack(stack)) {</span><br><span>                   continue;</span><br><span>               }</span><br><span>               stack.getAllRunningVisibleActivitiesLocked(mTmpActivityList);</span><br><span>               //获取前台栈顶第一个非finishing的Activity</span><br><span>               final ActivityRecord top = stack.topRunningActivityLocked();</span><br><span>               final int size = mTmpActivityList.size();</span><br><span>               for (int i = 0; i &lt; size; i++) {</span><br><span>                   final ActivityRecord activity = mTmpActivityList.get(i);</span><br><span>                   if (activity.app == null &amp;&amp; app.uid == activity.info.applicationInfo.uid</span><br><span>                           &amp;&amp; processName.equals(activity.processName)) {</span><br><span>                       try {</span><br><span>                           //真正启动Activity，见2.15节</span><br><span>                           if (realStartActivityLocked(activity, app,</span><br><span>                                   top == activity /* andResume */, true /* checkConfig */)) {</span><br><span>                               didSomething = true;</span><br><span>                           }</span><br><span>                       } catch (RemoteException e) {</span><br><span>                           Slog.w(TAG, "Exception in new application when starting activity "</span><br><span>                                   + top.intent.getComponent().flattenToShortString(), e);</span><br><span>                           throw e;</span><br><span>                       }</span><br><span>                   }</span><br><span>               }</span><br><span>           }</span><br><span>       }</span><br><span>       if (!didSomething) {</span><br><span>           //启动Activity不成功，确保有可见的Activity</span><br><span>           ensureActivitiesVisibleLocked(null, 0, !PRESERVE_WINDOWS);</span><br><span>       }</span><br><span>       return didSomething;</span><br><span>   }</span><br></pre></td></tr></tbody></table>

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-17-ASS-realStartActivityLocked "2.17 ASS.realStartActivityLocked")2.17 ASS.realStartActivityLocked

\[->ActivityStackSupervisor.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br><span>40</span><br><span>41</span><br><span>42</span><br><span>43</span><br><span>44</span><br><span>45</span><br><span>46</span><br><span>47</span><br><span>48</span><br><span>49</span><br><span>50</span><br><span>51</span><br><span>52</span><br><span>53</span><br><span>54</span><br><span>55</span><br><span>56</span><br><span>57</span><br><span>58</span><br><span>59</span><br><span>60</span><br><span>61</span><br><span>62</span><br><span>63</span><br><span>64</span><br><span>65</span><br><span>66</span><br><span>67</span><br><span>68</span><br><span>69</span><br><span>70</span><br><span>71</span><br><span>72</span><br><span>73</span><br><span>74</span><br><span>75</span><br><span>76</span><br><span>77</span><br><span>78</span><br><span>79</span><br><span>80</span><br><span>81</span><br><span>82</span><br><span>83</span><br><span>84</span><br><span>85</span><br><span>86</span><br><span>87</span><br><span>88</span><br><span>89</span><br><span>90</span><br><span>91</span><br><span>92</span><br><span>93</span><br><span>94</span><br><span>95</span><br><span>96</span><br><span>97</span><br><span>98</span><br><span>99</span><br><span>100</span><br><span>101</span><br><span>102</span><br><span>103</span><br><span>104</span><br><span>105</span><br><span>106</span><br><span>107</span><br><span>108</span><br><span>109</span><br><span>110</span><br><span>111</span><br><span>112</span><br><span>113</span><br><span>114</span><br><span>115</span><br><span>116</span><br><span>117</span><br><span>118</span><br><span>119</span><br><span>120</span><br><span>121</span><br><span>122</span><br><span>123</span><br><span>124</span><br><span>125</span><br><span>126</span><br><span>127</span><br><span>128</span><br><span>129</span><br><span>130</span><br><span>131</span><br><span>132</span><br><span>133</span><br><span>134</span><br><span>135</span><br><span>136</span><br><span>137</span><br><span>138</span><br><span>139</span><br><span>140</span><br><span>141</span><br><span>142</span><br><span>143</span><br><span>144</span><br><span>145</span><br><span>146</span><br><span>147</span><br><span>148</span><br><span>149</span><br><span>150</span><br><span>151</span><br><span>152</span><br><span>153</span><br><span>154</span><br><span>155</span><br><span>156</span><br><span>157</span><br><span>158</span><br><span>159</span><br><span>160</span><br><span>161</span><br><span>162</span><br><span>163</span><br><span>164</span><br><span>165</span><br><span>166</span><br><span>167</span><br><span>168</span><br><span>169</span><br><span>170</span><br><span>171</span><br><span>172</span><br><span>173</span><br><span>174</span><br><span>175</span><br><span>176</span><br><span>177</span><br><span>178</span><br><span>179</span><br><span>180</span><br><span>181</span><br><span>182</span><br><span>183</span><br><span>184</span><br><span>185</span><br><span>186</span><br><span>187</span><br><span>188</span><br><span>189</span><br><span>190</span><br><span>191</span><br><span>192</span><br><span>193</span><br><span>194</span><br><span>195</span><br><span>196</span><br><span>197</span><br><span>198</span><br><span>199</span><br><span>200</span><br><span>201</span><br><span>202</span><br><span>203</span><br><span>204</span><br><span>205</span><br><span>206</span><br><span>207</span><br><span>208</span><br><span>209</span><br><span>210</span><br><span>211</span><br><span>212</span><br><span>213</span><br><span>214</span><br><span>215</span><br><span>216</span><br><span>217</span><br><span>218</span><br><span>219</span><br><span>220</span><br><span>221</span><br><span>222</span><br><span>223</span><br><span>224</span><br><span>225</span><br><span>226</span><br><span>227</span><br><span>228</span><br><span>229</span><br><span>230</span><br><span>231</span><br><span>232</span><br><span>233</span><br><span>234</span><br><span>235</span><br><span>236</span><br><span>237</span><br><span>238</span><br><span>239</span><br><span>240</span><br><span>241</span><br><span>242</span><br><span>243</span><br><span>244</span><br><span>245</span><br><span>246</span><br><span>247</span><br><span>248</span><br><span>249</span><br><span>250</span><br><span>251</span><br><span>252</span><br><span>253</span><br><span>254</span><br><span>255</span><br><span>256</span><br><span>257</span><br><span>258</span><br><span>259</span><br><span>260</span><br><span>261</span><br><span>262</span><br></pre></td><td><pre><span>final boolean realStartActivityLocked(ActivityRecord r, ProcessRecord app,</span><br><span>          boolean andResume, boolean checkConfig) throws RemoteException {</span><br><span></span><br><span>      if (!allPausedActivitiesComplete()) {</span><br><span>          //如果Activity没有pausing完成则返回</span><br><span>          // While there are activities pausing we skipping starting any new activities until</span><br><span>          // pauses are complete. NOTE: that we also do this for activities that are starting in</span><br><span>          // the paused state because they will first be resumed then paused on the client side.</span><br><span>          if (DEBUG_SWITCH || DEBUG_PAUSE || DEBUG_STATES) Slog.v(TAG_PAUSE,</span><br><span>                  "realStartActivityLocked: Skipping start of r=" + r</span><br><span>                  + " some activities pausing...");</span><br><span>          return false;</span><br><span>      }</span><br><span></span><br><span>      final TaskRecord task = r.getTask();</span><br><span>      final ActivityStack stack = task.getStack();</span><br><span></span><br><span>      beginDeferResume();</span><br><span></span><br><span>      try {</span><br><span>          r.startFreezingScreenLocked(app, 0);</span><br><span>          //启动tick，收集应用启动慢的信息</span><br><span>          // schedule launch ticks to collect information about slow apps.</span><br><span>          r.startLaunchTickingLocked();</span><br><span></span><br><span>          r.setProcess(app);</span><br><span></span><br><span>          if (getKeyguardController().isKeyguardLocked()) {</span><br><span>              r.notifyUnknownVisibilityLaunched();</span><br><span>          }</span><br><span>         </span><br><span>          // Have the window manager re-evaluate the orientation of the screen based on the new</span><br><span>          // activity order.  Note that as a result of this, it can call back into the activity</span><br><span>          // manager with a new orientation.  We don't care about that, because the activity is</span><br><span>          // not currently running so we are just restarting it anyway.</span><br><span>          if (checkConfig) {</span><br><span>              // Deferring resume here because we're going to launch new activity shortly.</span><br><span>              // We don't want to perform a redundant launch of the same record while ensuring</span><br><span>              // configurations and trying to resume top activity of focused stack.</span><br><span>              ensureVisibilityAndConfig(r, r.getDisplayId(),</span><br><span>                      false /* markFrozenIfConfigChanged */, true /* deferResume */);</span><br><span>          }</span><br><span></span><br><span>          if (r.getStack().checkKeyguardVisibility(r, true /* shouldBeVisible */,</span><br><span>                  true /* isTop */)) {</span><br><span>              // We only set the visibility to true if the activity is allowed to be visible</span><br><span>              // based on</span><br><span>              // keyguard state. This avoids setting this into motion in window manager that is</span><br><span>              // later cancelled due to later calls to ensure visible activities that set</span><br><span>              // visibility back to false.</span><br><span>              r.setVisibility(true);</span><br><span>          }</span><br><span></span><br><span>          final int applicationInfoUid =</span><br><span>                  (r.info.applicationInfo != null) ? r.info.applicationInfo.uid : -1;</span><br><span>          if ((r.userId != app.userId) || (r.appInfo.uid != applicationInfoUid)) {</span><br><span>              Slog.wtf(TAG,</span><br><span>                      "User ID for activity changing for " + r</span><br><span>                              + " appInfo.uid=" + r.appInfo.uid</span><br><span>                              + " info.ai.uid=" + applicationInfoUid</span><br><span>                              + " old=" + r.app + " new=" + app);</span><br><span>          }</span><br><span></span><br><span>          app.waitingToKill = null;</span><br><span>          r.launchCount++;</span><br><span>          r.lastLaunchTime = SystemClock.uptimeMillis();</span><br><span></span><br><span>          if (DEBUG_ALL) Slog.v(TAG, "Launching: " + r);</span><br><span></span><br><span>          int idx = app.activities.indexOf(r);</span><br><span>          if (idx &lt; 0) {</span><br><span>              app.activities.add(r);</span><br><span>          }</span><br><span>          </span><br><span>          mService.updateLruProcessLocked(app, true, null);</span><br><span>          mService.updateOomAdjLocked();</span><br><span></span><br><span>          final LockTaskController lockTaskController = mService.getLockTaskController();</span><br><span>          if (task.mLockTaskAuth == LOCK_TASK_AUTH_LAUNCHABLE</span><br><span>                  || task.mLockTaskAuth == LOCK_TASK_AUTH_LAUNCHABLE_PRIV</span><br><span>                  || (task.mLockTaskAuth == LOCK_TASK_AUTH_WHITELISTED</span><br><span>                          &amp;&amp; lockTaskController.getLockTaskModeState()</span><br><span>                                  == LOCK_TASK_MODE_LOCKED)) {</span><br><span>              lockTaskController.startLockTaskMode(task, false, 0 /* blank UID */);</span><br><span>          }</span><br><span></span><br><span>          try {</span><br><span>              if (app.thread == null) {</span><br><span>                  throw new RemoteException();</span><br><span>              }</span><br><span>              List&lt;ResultInfo&gt; results = null;</span><br><span>              List&lt;ReferrerIntent&gt; newIntents = null;</span><br><span>              if (andResume) {</span><br><span>                  // We don't need to deliver new intents and/or set results if activity is going</span><br><span>                  // to pause immediately after launch.</span><br><span>                  results = r.results;</span><br><span>                  newIntents = r.newIntents;</span><br><span>              }</span><br><span>              if (DEBUG_SWITCH) Slog.v(TAG_SWITCH,</span><br><span>                      "Launching: " + r + " icicle=" + r.icicle + " with results=" + results</span><br><span>                              + " newIntents=" + newIntents + " andResume=" + andResume);</span><br><span>              EventLog.writeEvent(EventLogTags.AM_RESTART_ACTIVITY, r.userId,</span><br><span>                      System.identityHashCode(r), task.taskId, r.shortComponentName);</span><br><span>              if (r.isActivityTypeHome()) {</span><br><span>                  //home进程是该栈的根进程</span><br><span>                  // Home process is the root process of the task.</span><br><span>                  mService.mHomeProcess = task.mActivities.get(0).app;</span><br><span>              }</span><br><span>              mService.notifyPackageUse(r.intent.getComponent().getPackageName(),</span><br><span>                      PackageManager.NOTIFY_PACKAGE_USE_ACTIVITY);</span><br><span>              r.sleeping = false;</span><br><span>              r.forceNewConfig = false;</span><br><span>              mService.getAppWarningsLocked().onStartActivity(r);</span><br><span>              mService.showAskCompatModeDialogLocked(r);</span><br><span>              r.compat = mService.compatibilityInfoForPackageLocked(r.info.applicationInfo);</span><br><span>              ProfilerInfo profilerInfo = null;</span><br><span>              if (mService.mProfileApp != null &amp;&amp; mService.mProfileApp.equals(app.processName)) {</span><br><span>                  if (mService.mProfileProc == null || mService.mProfileProc == app) {</span><br><span>                      mService.mProfileProc = app;</span><br><span>                      ProfilerInfo profilerInfoSvc = mService.mProfilerInfo;</span><br><span>                      if (profilerInfoSvc != null &amp;&amp; profilerInfoSvc.profileFile != null) {</span><br><span>                          if (profilerInfoSvc.profileFd != null) {</span><br><span>                              try {</span><br><span>                                  profilerInfoSvc.profileFd = profilerInfoSvc.profileFd.dup();</span><br><span>                              } catch (IOException e) {</span><br><span>                                  profilerInfoSvc.closeFd();</span><br><span>                              }</span><br><span>                          }</span><br><span></span><br><span>                          profilerInfo = new ProfilerInfo(profilerInfoSvc);</span><br><span>                      }</span><br><span>                  }</span><br><span>              }</span><br><span></span><br><span>              app.hasShownUi = true;</span><br><span>              app.pendingUiClean = true;</span><br><span>              //将该进程设置为前台进程PROCESS_STATE_TOP</span><br><span>              app.forceProcessStateUpTo(mService.mTopProcessState);</span><br><span>              // Because we could be starting an Activity in the system process this may not go</span><br><span>              // across a Binder interface which would create a new Configuration. Consequently</span><br><span>              // we have to always create a new Configuration here.</span><br><span></span><br><span>              final MergedConfiguration mergedConfiguration = new MergedConfiguration(</span><br><span>                      mService.getGlobalConfiguration(), r.getMergedOverrideConfiguration());</span><br><span>              r.setLastReportedConfiguration(mergedConfiguration);</span><br><span></span><br><span>              logIfTransactionTooLarge(r.intent, r.icicle);</span><br><span></span><br><span>              //创建Activity启动事务</span><br><span>              // Create activity launch transaction.</span><br><span>              final ClientTransaction clientTransaction = ClientTransaction.obtain(app.thread,</span><br><span>                      r.appToken);</span><br><span>              clientTransaction.addCallback(LaunchActivityItem.obtain(new Intent(r.intent),</span><br><span>                      System.identityHashCode(r), r.info,</span><br><span>                      // TODO: Have this take the merged configuration instead of separate global</span><br><span>                      // and override configs.</span><br><span>                      mergedConfiguration.getGlobalConfiguration(),</span><br><span>                      mergedConfiguration.getOverrideConfiguration(), r.compat,</span><br><span>                      r.launchedFromPackage, task.voiceInteractor, app.repProcState, r.icicle,</span><br><span>                      r.persistentState, results, newIntents, mService.isNextTransitionForward(),</span><br><span>                      profilerInfo));</span><br><span></span><br><span>              //设置目标事务的状态为onResume</span><br><span>              // Set desired final state.</span><br><span>              final ActivityLifecycleItem lifecycleItem;</span><br><span>              if (andResume) {</span><br><span>                  lifecycleItem = ResumeActivityItem.obtain(mService.isNextTransitionForward());</span><br><span>              } else {</span><br><span>                  lifecycleItem = PauseActivityItem.obtain();</span><br><span>              }</span><br><span>              clientTransaction.setLifecycleStateRequest(lifecycleItem);</span><br><span></span><br><span>              //通过transaciton方式开始activity生命周期，onCreate,onStart,onResume</span><br><span>              // Schedule transaction.</span><br><span>              mService.getLifecycleManager().scheduleTransaction(clientTransaction);</span><br><span></span><br><span></span><br><span>              if ((app.info.privateFlags &amp; ApplicationInfo.PRIVATE_FLAG_CANT_SAVE_STATE) != 0</span><br><span>                      &amp;&amp; mService.mHasHeavyWeightFeature) {</span><br><span>                  //处理heavy-weight进程</span><br><span>                  // This may be a heavy-weight process!  Note that the package</span><br><span>                  // manager will ensure that only activity can run in the main</span><br><span>                  // process of the .apk, which is the only thing that will be</span><br><span>                  // considered heavy-weight.</span><br><span>                  if (app.processName.equals(app.info.packageName)) {</span><br><span>                      if (mService.mHeavyWeightProcess != null</span><br><span>                              &amp;&amp; mService.mHeavyWeightProcess != app) {</span><br><span>                          Slog.w(TAG, "Starting new heavy weight process " + app</span><br><span>                                  + " when already running "</span><br><span>                                  + mService.mHeavyWeightProcess);</span><br><span>                      }</span><br><span>                      mService.mHeavyWeightProcess = app;</span><br><span>                      Message msg = mService.mHandler.obtainMessage(</span><br><span>                              ActivityManagerService.POST_HEAVY_NOTIFICATION_MSG);</span><br><span>                      msg.obj = r;</span><br><span>                      mService.mHandler.sendMessage(msg);</span><br><span>                  }</span><br><span>              }</span><br><span></span><br><span>          } catch (RemoteException e) {</span><br><span>              if (r.launchFailed) {</span><br><span>                  //第二次启动失败，则结束该Activity</span><br><span>                  // This is the second time we failed -- finish activity</span><br><span>                  // and give up.</span><br><span>                  Slog.e(TAG, "Second failure launching "</span><br><span>                          + r.intent.getComponent().flattenToShortString()</span><br><span>                          + ", giving up", e);</span><br><span>                  mService.appDiedLocked(app);</span><br><span>                  stack.requestFinishActivityLocked(r.appToken, Activity.RESULT_CANCELED, null,</span><br><span>                          "2nd-crash", false);</span><br><span>                  return false;</span><br><span>              }</span><br><span>              //第一次启动失败，则重启进程</span><br><span>              // This is the first time we failed -- restart process and</span><br><span>              // retry.</span><br><span>              r.launchFailed = true;</span><br><span>              app.activities.remove(r);</span><br><span>              throw e;</span><br><span>          }</span><br><span>      } finally {</span><br><span>          endDeferResume();</span><br><span>      }</span><br><span></span><br><span>      r.launchFailed = false;</span><br><span>       //将该进程加入到mLruActivity队列顶部</span><br><span>      if (stack.updateLRUListLocked(r)) {</span><br><span>          Slog.w(TAG, "Activity " + r + " being launched, but already in LRU list");</span><br><span>      }</span><br><span></span><br><span>      // TODO(lifecycler): Resume or pause requests are done as part of launch transaction,</span><br><span>      // so updating the state should be done accordingly.</span><br><span>      if (andResume &amp;&amp; readyToResume()) {</span><br><span>          // As part of the process of launching, ActivityThread also performs</span><br><span>          // a resume.</span><br><span>          stack.minimalResumeActivityLocked(r);</span><br><span>      } else {</span><br><span>          // This activity is not starting in the resumed state... which should look like we asked</span><br><span>          // it to pause+stop (but remain visible), and it has done so and reported back the</span><br><span>          // current icicle and other state.</span><br><span>          if (DEBUG_STATES) Slog.v(TAG_STATES,</span><br><span>                  "Moving to PAUSED: " + r + " (starting in paused state)");</span><br><span>          r.setState(PAUSED, "realStartActivityLocked");</span><br><span>      }</span><br><span></span><br><span>      // Launch the new version setup screen if needed.  We do this -after-</span><br><span>      // launching the initial activity (that is, home), so that it can have</span><br><span>      // a chance to initialize itself while in the background, making the</span><br><span>      // switch back to it faster and look better.</span><br><span>      if (isFocusedStack(stack)) {</span><br><span>          //当系统发生更新时，只会执行一次的用户向导</span><br><span>          mService.getActivityStartController().startSetupActivity();</span><br><span>      }</span><br><span></span><br><span>      // Update any services we are bound to that might care about whether</span><br><span>      // their client may have activities.</span><br><span>      if (r.app != null) {</span><br><span>          //更新所有与该Activity具有绑定关系的Service连接</span><br><span>          mService.mServices.updateServiceConnectionActivitiesLocked(r.app);</span><br><span>      }</span><br><span></span><br><span>      return true;</span><br><span>  }</span><br></pre></td></tr></tbody></table>

Android9.0之后，引入了ClientLifecycleManager和ClientTransactionHandler来辅助管理Activity的生命周期。

一个生命周期都抽象出了一个对象。

onCreate (LaunchActivityItem),onResume(ResumeActivityItem),

onPause(PauseActivityItem)，onStop(StopActivityItem),onDestrory(DestroyActivityItem)

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-18-CLM-scheduleTransaction "2.18  CLM.scheduleTransaction")2.18 CLM.scheduleTransaction

\[->ClientLifecycleManager.java\]

将2.17节启动Activity生命周期的代码单独分析一下启动的过程

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br></pre></td><td><pre><span> //创建Activity启动事务</span><br><span>final ClientTransaction clientTransaction = ClientTransaction.obtain(app.thread,</span><br><span>        r.appToken);</span><br><span>//设置事务callback，状态为onCreate ---&gt;1        </span><br><span>clientTransaction.addCallback(LaunchActivityItem.obtain(new Intent(r.intent),</span><br><span>        System.identityHashCode(r), r.info,</span><br><span>        mergedConfiguration.getGlobalConfiguration(),</span><br><span>        mergedConfiguration.getOverrideConfiguration(), r.compat,</span><br><span>        r.launchedFromPackage, task.voiceInteractor, app.repProcState, r.icicle,</span><br><span>        r.persistentState, results, newIntents, mService.isNextTransitionForward(),</span><br><span>        profilerInfo));</span><br><span></span><br><span>//设置目标事务的状态为onResume   ----&gt;2</span><br><span>final ActivityLifecycleItem lifecycleItem;</span><br><span>lifecycleItem = ResumeActivityItem.obtain(mService.isNextTransitionForward());</span><br><span>clientTransaction.setLifecycleStateRequest(lifecycleItem);</span><br><span></span><br><span>//通过事务方式开始activity生命周期，onCreate,onStart,onResume   ----&gt;3</span><br><span>mService.getLifecycleManager().scheduleTransaction(clientTransaction);</span><br></pre></td></tr></tbody></table>

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-18-1-AMS-getLifecycleManager "2.18.1 AMS.getLifecycleManager")2.18.1 AMS.getLifecycleManager

\[->ActivityManagerService.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br></pre></td><td><pre><span>ClientLifecycleManager getLifecycleManager() {</span><br><span>       return mLifecycleManager;</span><br><span>   }</span><br></pre></td></tr></tbody></table>

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-18-2-CLM-scheduleTransaction "2.18.2 CLM.scheduleTransaction")2.18.2 CLM.scheduleTransaction

\[->ClientLifecycleManager.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br></pre></td><td><pre><span>void scheduleTransaction(ClientTransaction transaction) throws RemoteException {</span><br><span>      final IApplicationThread client = transaction.getClient();</span><br><span>      transaction.schedule();</span><br><span>      if (!(client instanceof Binder)) {</span><br><span>          // If client is not an instance of Binder - it's a remote call and at this point it is</span><br><span>          // safe to recycle the object. All objects used for local calls will be recycled after</span><br><span>          // the transaction is executed on client in ActivityThread.</span><br><span>          transaction.recycle();</span><br><span>      }</span><br><span>  }</span><br></pre></td></tr></tbody></table>

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-18-3-CT-schedule "2.18.3 CT.schedule")2.18.3 CT.schedule

\[->ClientTransaction.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br></pre></td><td><pre><span>public IApplicationThread getClient() {</span><br><span>       return mClient;</span><br><span>}</span><br><span>public void schedule() throws RemoteException {</span><br><span>       mClient.scheduleTransaction(this);</span><br><span>}</span><br></pre></td></tr></tbody></table>

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-18-4-AT-scheduleTransaction "2.18.4 AT.scheduleTransaction")2.18.4 AT.scheduleTransaction

\[->ActivityThread.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br></pre></td><td><pre><span>@Override</span><br><span>public void scheduleTransaction(ClientTransaction transaction) throws RemoteException {</span><br><span>      ActivityThread.this.scheduleTransaction(transaction);</span><br><span>}</span><br></pre></td></tr></tbody></table>

ActivityThread继承ClientTransactionHandler

\[->ClientTransactionHandler.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br></pre></td><td><pre><span>/** Prepare and schedule transaction for execution. */</span><br><span>void scheduleTransaction(ClientTransaction transaction) {</span><br><span>     transaction.preExecute(this);</span><br><span>     sendMessage(ActivityThread.H.EXECUTE_TRANSACTION, transaction);</span><br><span>}</span><br></pre></td></tr></tbody></table>

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-18-5-AT-handleMessage "2.18.5 AT.handleMessage")2.18.5 AT.handleMessage

\[->ActivityThread.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br></pre></td><td><pre><span>public void handleMessage(Message msg) {</span><br><span>         ...</span><br><span>         case EXECUTE_TRANSACTION:</span><br><span>                    final ClientTransaction transaction = (ClientTransaction) msg.obj;</span><br><span>                    mTransactionExecutor.execute(transaction);</span><br><span>                    if (isSystem()) {</span><br><span>                        // Client transactions inside system process are recycled on the client side</span><br><span>                        // instead of ClientLifecycleManager to avoid being cleared before this</span><br><span>                        // message is handled.</span><br><span>                        transaction.recycle();</span><br><span>                    }</span><br><span>                    // TODO(lifecycler): Recycle locally scheduled transactions.</span><br><span>                    break;</span><br><span>           ....</span><br><span>     }</span><br></pre></td></tr></tbody></table>

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-18-6-TE-execute "2.18.6 TE.execute")2.18.6 TE.execute

\[->TransactionExecutor.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br></pre></td><td><pre><span>/**</span><br><span>    * Resolve transaction.</span><br><span>    * First all callbacks will be executed in the order they appear in the list. If a callback</span><br><span>    * requires a certain pre- or post-execution state, the client will be transitioned accordingly.</span><br><span>    * Then the client will cycle to the final lifecycle state if provided. Otherwise, it will</span><br><span>    * either remain in the initial state, or last state needed by a callback.</span><br><span>    */</span><br><span>   public void execute(ClientTransaction transaction) {</span><br><span>       final IBinder token = transaction.getActivityToken();</span><br><span>       log("Start resolving transaction for client: " + mTransactionHandler + ", token: " + token);</span><br><span>       //2.18节开始方法的第一步，状态为onCreate</span><br><span>       executeCallbacks(transaction);</span><br><span>       //2.18节开始方法的第二步，最后的状态为onResume</span><br><span>       executeLifecycleState(transaction);</span><br><span>       mPendingActions.clear();</span><br><span>       log("End resolving transaction");</span><br><span>   }</span><br></pre></td></tr></tbody></table>

\[->TransactionExecutor.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br><span>40</span><br><span>41</span><br><span>42</span><br><span>43</span><br><span>44</span><br><span>45</span><br><span>46</span><br></pre></td><td><pre><span>public void executeCallbacks(ClientTransaction transaction) {</span><br><span>       final List&lt;ClientTransactionItem&gt; callbacks = transaction.getCallbacks();</span><br><span>       if (callbacks == null) {</span><br><span>           // No callbacks to execute, return early.</span><br><span>           return;</span><br><span>       }</span><br><span>       log("Resolving callbacks");</span><br><span></span><br><span>       final IBinder token = transaction.getActivityToken();</span><br><span>       ActivityClientRecord r = mTransactionHandler.getActivityClient(token);</span><br><span></span><br><span>       // In case when post-execution state of the last callback matches the final state requested</span><br><span>       // for the activity in this transaction, we won't do the last transition here and do it when</span><br><span>       // moving to final state instead (because it may contain additional parameters from server).</span><br><span>       final ActivityLifecycleItem finalStateRequest = transaction.getLifecycleStateRequest();</span><br><span>       final int finalState = finalStateRequest != null ? finalStateRequest.getTargetState()</span><br><span>               : UNDEFINED;</span><br><span>       // Index of the last callback that requests some post-execution state.</span><br><span>       final int lastCallbackRequestingState = lastCallbackRequestingState(transaction);</span><br><span></span><br><span>       final int size = callbacks.size();</span><br><span>       for (int i = 0; i &lt; size; ++i) {</span><br><span>           final ClientTransactionItem item = callbacks.get(i);</span><br><span>           log("Resolving callback: " + item);</span><br><span>           final int postExecutionState = item.getPostExecutionState();</span><br><span>           final int closestPreExecutionState = mHelper.getClosestPreExecutionState(r,</span><br><span>                   item.getPostExecutionState());</span><br><span>           if (closestPreExecutionState != UNDEFINED) {</span><br><span>               cycleToPath(r, closestPreExecutionState);</span><br><span>           }</span><br><span>           //将会执行execute方法</span><br><span>           item.execute(mTransactionHandler, token, mPendingActions);</span><br><span>           item.postExecute(mTransactionHandler, token, mPendingActions);</span><br><span>           if (r == null) {</span><br><span>               // Launch activity request will create an activity record.</span><br><span>               r = mTransactionHandler.getActivityClient(token);</span><br><span>           }</span><br><span></span><br><span>           if (postExecutionState != UNDEFINED &amp;&amp; r != null) {</span><br><span>               // Skip the very last transition and perform it by explicit state request instead.</span><br><span>               final boolean shouldExcludeLastTransition =</span><br><span>                       i == lastCallbackRequestingState &amp;&amp; finalState == postExecutionState;</span><br><span>               cycleToPath(r, postExecutionState, shouldExcludeLastTransition);</span><br><span>           }</span><br><span>       }</span><br><span>   }</span><br></pre></td></tr></tbody></table>

\[->LaunchActivityItem.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br></pre></td><td><pre><span>@Override</span><br><span>   public void execute(ClientTransactionHandler client, IBinder token,</span><br><span>           PendingTransactionActions pendingActions) {</span><br><span>       Trace.traceBegin(TRACE_TAG_ACTIVITY_MANAGER, "activityStart");</span><br><span>       ActivityClientRecord r = new ActivityClientRecord(token, mIntent, mIdent, mInfo,</span><br><span>               mOverrideConfig, mCompatInfo, mReferrer, mVoiceInteractor, mState, mPersistentState,</span><br><span>               mPendingResults, mPendingNewIntents, mIsForward,</span><br><span>               mProfilerInfo, client);</span><br><span>        //见2.19节，ActivityThread继承ClientTransactionHandler</span><br><span>       client.handleLaunchActivity(r, pendingActions, null /* customIntent */);</span><br><span>       Trace.traceEnd(TRACE_TAG_ACTIVITY_MANAGER);</span><br><span>   }</span><br></pre></td></tr></tbody></table>

#### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-18-7-TE-cycleToPath "2.18.7 TE.cycleToPath")2.18.7 TE.cycleToPath

\[->TransactionExecutor.java\]

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br><span>40</span><br><span>41</span><br><span>42</span><br><span>43</span><br><span>44</span><br><span>45</span><br><span>46</span><br><span>47</span><br><span>48</span><br><span>49</span><br><span>50</span><br><span>51</span><br><span>52</span><br><span>53</span><br><span>54</span><br></pre></td><td><pre><span>/**</span><br><span>    * Transition the client between states with an option not to perform the last hop in the</span><br><span>    * sequence. This is used when resolving lifecycle state request, when the last transition must</span><br><span>    * be performed with some specific parameters.</span><br><span>    */</span><br><span>   private void cycleToPath(ActivityClientRecord r, int finish,</span><br><span>           boolean excludeLastState) {</span><br><span>       final int start = r.getLifecycleState();</span><br><span>       log("Cycle from: " + start + " to: " + finish + " excludeLastState:" + excludeLastState);</span><br><span>       final IntArray path = mHelper.getLifecyclePath(start, finish, excludeLastState);</span><br><span>       performLifecycleSequence(r, path);</span><br><span>   }</span><br><span></span><br><span>   /** Transition the client through previously initialized state sequence. */</span><br><span>   private void performLifecycleSequence(ActivityClientRecord r, IntArray path) {</span><br><span>       final int size = path.size();</span><br><span>       for (int i = 0, state; i &lt; size; i++) {</span><br><span>           state = path.get(i);</span><br><span>           log("Transitioning to state: " + state);</span><br><span>           switch (state) {</span><br><span>               case ON_CREATE:</span><br><span>                   mTransactionHandler.handleLaunchActivity(r, mPendingActions,</span><br><span>                           null /* customIntent */);</span><br><span>                   break;</span><br><span>               case ON_START:</span><br><span>                   mTransactionHandler.handleStartActivity(r, mPendingActions);</span><br><span>                   break;</span><br><span>               case ON_RESUME:</span><br><span>                   mTransactionHandler.handleResumeActivity(r.token, false /* finalStateRequest */,</span><br><span>                           r.isForward, "LIFECYCLER_RESUME_ACTIVITY");</span><br><span>                   break;</span><br><span>               case ON_PAUSE:</span><br><span>                   mTransactionHandler.handlePauseActivity(r.token, false /* finished */,</span><br><span>                           false /* userLeaving */, 0 /* configChanges */, mPendingActions,</span><br><span>                           "LIFECYCLER_PAUSE_ACTIVITY");</span><br><span>                   break;</span><br><span>               case ON_STOP:</span><br><span>                   mTransactionHandler.handleStopActivity(r.token, false /* show */,</span><br><span>                           0 /* configChanges */, mPendingActions, false /* finalStateRequest */,</span><br><span>                           "LIFECYCLER_STOP_ACTIVITY");</span><br><span>                   break;</span><br><span>               case ON_DESTROY:</span><br><span>                   mTransactionHandler.handleDestroyActivity(r.token, false /* finishing */,</span><br><span>                           0 /* configChanges */, false /* getNonConfigInstance */,</span><br><span>                           "performLifecycleSequence. cycling to:" + path.get(size - 1));</span><br><span>                   break;</span><br><span>               case ON_RESTART:</span><br><span>                   mTransactionHandler.performRestartActivity(r.token, false /* start */);</span><br><span>                   break;</span><br><span>               default:</span><br><span>                   throw new IllegalArgumentException("Unexpected lifecycle state: " + state);</span><br><span>           }</span><br><span>       }</span><br><span>   }</span><br></pre></td></tr></tbody></table>

cycleToPath将会执行从start,finish之间的周期方法。

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-19-AT-handleLaunchActivity "2.19 AT.handleLaunchActivity")2.19 AT.handleLaunchActivity

\[->ActivityThread.java\]

```
/**  
 * Extended implementation of activity launch. Used when server requests a launch or relaunch.  
 */  
@Override  
public Activity handleLaunchActivity(ActivityClientRecord r,  
        PendingTransactionActions pendingActions, Intent customIntent) {  
    // If we are getting ready to gc after going to the background, well  
    // we are back active so skip it.  
    unscheduleGcIdler();  
    mSomeActivitiesChanged = true;  
  
    if (r.profilerInfo != null) {  
        mProfiler.setProfiler(r.profilerInfo);  
        mProfiler.startProfiling();  
    }  
    //回调目标Activity的onConfigurationChanged  
    // Make sure we are running with the most recent config.  
    handleConfigurationChanged(null, null);  
  
    if (localLOGV) Slog.v(  
        TAG, "Handling launch of " + r);  
   
    // Initialize before creating the activity  
    if (!ThreadedRenderer.sRendererDisabled) {  
        GraphicsEnvironment.earlyInitEGL();  
    }  
    WindowManagerGlobal.initialize();  
     //回调目标Activity的onCreate，正式开始Activity的生命周期  
    final Activity a = performLaunchActivity(r, customIntent);  
  
    if (a != null) {  
        r.createdConfig = new Configuration(mConfiguration);  
        reportSizeConfigurations(r);  
        if (!r.activity.mFinished && pendingActions != null) {  
            pendingActions.setOldState(r.state);  
            pendingActions.setRestoreInstanceState(true);  
            pendingActions.setCallOnPostCreate(true);  
        }  
    } else {  
        //存在error,则停止该Activity  
        // If there was an error, for any reason, tell the activity manager to stop us.  
        try {  
            ActivityManager.getService()  
                    .finishActivity(r.token, Activity.RESULT_CANCELED, null,  
                            Activity.DONT_FINISH_TASK_WITH_ACTIVITY);  
        } catch (RemoteException ex) {  
            throw ex.rethrowFromSystemServer();  
        }  
    }  
  
    return a;  
}
```

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-20-AT-performLaunchActivity "2.20 AT.performLaunchActivity")2.20 AT.performLaunchActivity

\[->ActivityThread.java\]

```
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
  
      ContextImpl appContext = createBaseContextForActivity(r);  
      Activity activity = null;  
      try {  
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
          if (!mInstrumentation.onException(activity, e)) {  
              throw new RuntimeException(  
                  "Unable to instantiate activity " + component  
                  + ": " + e.toString(), e);  
          }  
      }  
  
      try {  
          //创建Application对象  
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
              Configuration config = new Configuration(mCompatConfiguration);  
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
              appContext.setOuterContext(activity);  
              //attach方法  
              activity.attach(appContext, this, getInstrumentation(), r.token,  
                      r.ident, app, r.intent, r.activityInfo, title, r.parent,  
                      r.embeddedID, r.lastNonConfigurationInstances, config,  
                      r.referrer, r.voiceInteractor, window, r.configCallback);  
  
              if (customIntent != null) {  
                  activity.mIntent = customIntent;  
              }  
              r.lastNonConfigurationInstances = null;  
              checkAndBlockForNetworkAccess();  
              activity.mStartedActivity = false;  
              int theme = r.activityInfo.getThemeResource();  
              if (theme != 0) {  
                  activity.setTheme(theme);  
              }  
  
              activity.mCalled = false;  
              //进入生命周期的onCreate  
              if (r.isPersistable()) {  
                  mInstrumentation.callActivityOnCreate(activity, r.state, r.persistentState);  
              } else {  
                  mInstrumentation.callActivityOnCreate(activity, r.state);  
              }  
              if (!activity.mCalled) {  
                  throw new SuperNotCalledException(  
                      "Activity " + r.intent.getComponent().toShortString() +  
                      " did not call through to super.onCreate()");  
              }  
              r.activity = activity;  
          }  
          r.setState(ON_CREATE);  
  
          mActivities.put(r.token, r);  
  
      } catch (SuperNotCalledException e) {  
          throw e;  
  
      } catch (Exception e) {  
          if (!mInstrumentation.onException(activity, e)) {  
              throw new RuntimeException(  
                  "Unable to start activity " + component  
                  + ": " + e.toString(), e);  
          }  
      }  
  
      return activity;  
  }
```


### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-21-callActivityOnCreate "2.21 callActivityOnCreate")2.21 callActivityOnCreate

\[->Instrumentation.java\]

```
public void callActivityOnCreate(Activity activity, Bundle icicle) {  
     prePerformCreate(activity);  
     activity.performCreate(icicle);  
     postPerformCreate(activity);  
 }
```

### [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#2-22-performCreate "2.22 performCreate")2.22 performCreate

\[->Activity.java\]

```
@UnsupportedAppUsage  
   final void performCreate(Bundle icicle, PersistableBundle persistentState) {  
       mCanEnterPictureInPicture = true;  
       restoreHasCurrentPermissionRequest(icicle);  
       if (persistentState != null) {  
           onCreate(icicle, persistentState);  
       } else {  
           onCreate(icicle);  
       }  
       writeEventLog(LOG_AM_ON_CREATE_CALLED, "performCreate");  
       mActivityTransitionState.readState(icicle);  
  
       mVisibleFromClient = !mWindow.getWindowStyle().getBoolean(  
               com.android.internal.R.styleable.Window_windowNoDisplay, false);  
       mFragments.dispatchActivityCreated();  
       mActivityTransitionState.setEnterActivityOptions(this, getActivityOptions());  
   }
```


到此，介绍了完成了Activity从onCreate,onStart,onResume的生命周期详细过程。

## [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#%E4%B8%89%E3%80%81%E6%80%BB%E7%BB%93 "三、总结")三、总结

本文从startActivity开始，详细分析了Activity的启动过程。

1.  流程\[2.1~2.3\]:运行在调用者的进程当中，比如桌面启动Activity，则调用者所在的进程为launcher，launcher进程通过IActivityManager.aidl生成的代理类，进入到了systemserver进程（AMS相应的Server端）。
2.  流程\[2.3~2.17\]:允许在systemserver系统进程中，这个过程为比较复杂且核心的过程，主要如下：
    -   流程\[2.7\]:调用resolveActivity，通过PackageManager查询系统中所有符合要求的Activity，当存在多个满足多个条件的Activity则会让用户来选择 。
    -   流程\[2.8\]:创建ActivityRecord对象，检查intent相关信息和权限，是否允许app切换，然后处理mPendingActivityLaunches中的Activity
    -   流程\[2.9\]:为Activity找到或创建新的Task对象，设置flags信息
    -   流程\[2.13\]:当没有处于任务栈中没有Activity时，直接回到桌面了;否则当mResumeActivity不为空是，先执行startPausingLocked方法暂停该Activity，然后进入startSpecificActivityLocked
    -   流程\[2.14\]:当目标进程已经存在则直接进入2.17，当目标进程不存在时则创建进程，经过调用最后到2.17
    -   流程\[2.17\]:systemserver进程通过IApplicationThread binder接口，进入到了目标进程。
3.  流程\[2.18~2.20\]:允许在目标进程，通过Handler消息机制，该进程中的binder线程向主线程发送EXECUTE\_TRANSACTION消息，进入事务处理阶段调用到handleLaunchActivity，最后通过发射创建的目标Activity，然后进入到onCreate生命周期。

从进程的角度分析

1.  点击桌面图标，Launcher进程采用Binder IPC向systemserver进程发送startActivity请求
2.  systemserver进程接收到请求后，向zygote进程发送创建进程的请求
3.  zygote进程fork出新的子进程即app进程
4.  App进程，通过Binder IPC向systemserver进程发起attachApplication请求
5.  systemserver进程收到请求后，进行一系列准备工作后，在通过binderIPC进程发送scheduleTransaction请求
6.  APP进程的binder进程（ApplicationThread）在收到请求后，通过handler向主线程发送EXECUTE\_TRANSACTION消息。
7.  主线程收到Message消息后，通过反射机制创建目标Activity，并回调Activity的onCreate方法。

到此应用正式启动，开始进入应用的生命周期，执行完onCreate，onStart，onResume方法，makeVisible，UI渲染结束后就可以看到App的主界面了。

## [](https://skytoby.github.io/2019/startActivity%E5%90%AF%E5%8A%A8%E8%BF%87%E7%A8%8B/#%E9%99%84%E5%BD%95 "附录")附录

源码路径



```
frameworks/base/core/java/android/content/ContextWrapper.java  
frameworks/base/core/java/android/app/ContextImpl.java  
frameworks/base/core/java/android/app/Instrumentation.java  
frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java  
frameworks/base/services/core/java/com/android/server/am/ActivityStartController.java  
frameworks/base/services/core/java/com/android/server/am/ActivityStarter.java  
frameworks/base/services/core/java/com/android/server/am/ActivityStack.java  
frameworks/base/services/core/java/com/android/server/am/ActivityStackSupervisor.java  
frameworks/base/core/java/android/app/servertransaction/TransactionExecutor.java
```