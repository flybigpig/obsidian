---
created: 2025-04-10T10:01:05 (UTC +08:00)
tags: []
source: https://www.sunofbeach.net/a/1861571601200447490
author: 关于作者
         春风不识路 摸鱼工程师@摸鱼免责有限公司  文章 14   阅读 50232  粉丝 8
---

# 安卓在Recent界面清理app同时也清理掉关于此app的所有服务

> ## Excerpt
> 安卓在Recent界面清理app同时也清理掉关于此app的所有服务

---
在此网站上冲浪的大佬基本都是做安卓的，什么是服务我就不多赘述了，在安卓系统里有很多服务在后台运行，不管是app的还是系统的都有，比较重要的systemservice，有些app在多任务后台清理掉以后一些服务还在后台一直运行，比如QQ音乐，后台清理掉，后台播放着音乐一样不会停止掉，这样会导致占用系统资源，时间久了后台运行的服务多了，系统就会变卡，但是原生的系统是不会管这个的，所以需要厂商自己客制，最近一个客户也是要做这个功能，顾客是上帝（客户纯byd，天天提抽象又难做的需求，公司又不加工资，纯牛马），没办法只能给他做了，顺便也写一篇文章（好久在这上面写了）。 我会把低版本和高版本的方法都会写出来，主要是安卓8.1，安卓11和安卓十三，中间的版本需要改的也是大同小异，主要挑差异大的写，要想清理服务，需要借助ActivityManager.java里面的forceStopPackage方法，因为我在安卓八点一、安卓七改的地方context是没有定义的，我自己加的context不能用，用ActivityManager编译一直报错，怎么改都有问题，所以我用了IActivityManager.aidl里面的forceStopPackage接口方法，下面先看一下安卓八点一的forceStopPackage方法源码：

```
void forceStopPackage(in String packageName, int userId);
```

因为IActivityManager.aidl是一个接口类，实现是在ActivityManager.java，所以还是要看ActivityManager.java里面的forceStopPackage方法：

```
/**
     * @see #forceStopPackageAsUser(String, int)
     * @hide
     */
    @SystemApi
    @RequiresPermission(Manifest.permission.FORCE_STOP_PACKAGES)
    public void forceStopPackage(String packageName) {
        forceStopPackageAsUser(packageName, UserHandle.myUserId());
    }
```

forceStopPackage方法在各个版本的写法基本相同的，我就不贴出来了，作用就是用于强行停止指定应用包方法，注意点是要注意在AndroidManifest里面加一个FORCE\_STOP\_PACKAGES权限，不然就会报错。 知道了怎么杀服务的方法以后，下面就要怎么实现在recent界面划掉app的时候加上这个方法，在recent界面划掉app的时候会调用一个方法，这个方法是removeTask，这个方法在安卓九以下是在systemui里面，因为从安卓九开始，recent的一些功能被放在launcher3里面的quickstep，首先看安卓八点一，安卓八点一的removeTask方法是在frameworks/base/packages/SystemUI/src/com/android/systemui/recents/model/TaskStack.java里面

```
    /**
     * Removes a task from the stack, with an additional {@param animation} hint to the callbacks on
     * how they should update themselves.
     */
    public void removeTask(Task t, AnimationProps animation, boolean fromDockGesture,
            boolean dismissRecentsIfAllRemoved) {
        if (mStackTaskList.contains(t)) {
            removeTaskImpl(mStackTaskList, t);
            Task newFrontMostTask = getStackFrontMostTask(false  /* includeFreeform */);
            if (mCb != null) {
                // Notify that a task has been removed
                mCb.onStackTaskRemoved(this, t, newFrontMostTask, animation,
                        fromDockGesture, dismissRecentsIfAllRemoved);
            }
        }
        mRawTaskList.remove(t);
    }

    /**
     * Removes all tasks from the stack.
     */
    public void removeAllTasks(boolean notifyStackChanges) {
        ArrayList<Task> tasks = mStackTaskList.getTasks();
        for (int i = tasks.size() - 1; i >= 0; i--) {
            Task t = tasks.get(i);
            removeTaskImpl(mStackTaskList, t);
            mRawTaskList.remove(t);
        }
        if (mCb != null && notifyStackChanges) {
            // Notify that all tasks have been removed
            mCb.onStackTasksRemoved(this);
        }
    }
```

单个划掉app用的是removeTask，全部清理是removeAllTasks，下面直接看解决办法：

```
diff --git a/frameworks/base/packages/SystemUI/AndroidManifest.xml b/frameworks/base/packages/SystemUI/AndroidManifest.xml
index 2f7f0bb..6e83978 100755
--- a/frameworks/base/packages/SystemUI/AndroidManifest.xml
+++ b/frameworks/base/packages/SystemUI/AndroidManifest.xml
@@ -33,6 +33,7 @@
     <!-- Used to read storage for all users -->
     <uses-permission android:name="android.permission.WRITE_MEDIA_STORAGE" />
     <uses-permission android:name="android.permission.WAKE_LOCK" />
+    <uses-permission android:name="android.permission.FORCE_STOP_PACKAGES"/>
 
     <uses-permission android:name="android.permission.INJECT_EVENTS" />
     <uses-permission android:name="android.permission.DUMP" />
diff --git a/frameworks/base/packages/SystemUI/src/com/android/systemui/recents/model/TaskStack.java b/frameworks/base/packages/SystemUI/src/com/android/systemui/recents/model/TaskStack.java
old mode 100644
new mode 100755
index 6e3be09..f63c785
--- a/frameworks/base/packages/SystemUI/src/com/android/systemui/recents/model/TaskStack.java
+++ b/frameworks/base/packages/SystemUI/src/com/android/systemui/recents/model/TaskStack.java
@@ -642,6 +642,14 @@ public class TaskStack {
     public void removeTask(Task t, AnimationProps animation, boolean fromDockGesture,
             boolean dismissRecentsIfAllRemoved) {
         if (mStackTaskList.contains(t)) {
+            IActivityManager activityManager = ActivityManagerNative.getDefault();
+            String pkgName = t.key.getComponent().getPackageName();
+            Log.d("swl", "Stopping pkgName " + pkgName);
+            try {
+                activityManager.forceStopPackage(pkgName, UserHandle.myUserId());
+            } catch (RemoteException e) {
+                Log.e("swl", "Failed to force stop package " + pkgName, e);
+            }
             removeTaskImpl(mStackTaskList, t);
             Task newFrontMostTask = getStackFrontMostTask(false  /* includeFreeform */);
             if (mCb != null) {
@@ -658,8 +666,16 @@ public class TaskStack {
      */
     public void removeAllTasks(boolean notifyStackChanges) {
         ArrayList<Task> tasks = mStackTaskList.getTasks();
+        IActivityManager activityManager = ActivityManagerNative.getDefault();
         for (int i = tasks.size() - 1; i >= 0; i--) {
             Task t = tasks.get(i);
+            String pkgName = t.key.getComponent().getPackageName();
+            Log.d("swl", "Stopping allpkgName " + pkgName);  
+            try {
+                activityManager.forceStopPackage(pkgName, UserHandle.myUserId());
+            } catch (RemoteException e) {
+                Log.e("swl", "Failed to force stop allpackage " + pkgName, e);
+            }
             removeTaskImpl(mStackTaskList, t);
             mRawTaskList.remove(t);
         }
```

这是安卓八点一的方法，下面看安卓十一的方法，安卓十一的removeTask方法是在packages\\apps\\Launcher3\\quickstep\\recents\_ui\_overrides\\src\\com\\android\\quickstep\\views\\RecentsView.java中，源码如下：

```
private void removeTask(TaskView taskView, int index, EndState endState) {
        if (taskView.getTask() != null) {
            ActivityManagerWrapper.getInstance().removeTask(taskView.getTask().key.id);
            ComponentKey compKey = TaskUtils.getLaunchComponentKeyForTask(taskView.getTask().key);
            mActivity.getUserEventDispatcher().logTaskLaunchOrDismiss(
                    endState.logAction, Direction.UP, index, compKey);
            mActivity.getStatsLogManager().logger().withItemInfo(taskView.getItemInfo())
                    .log(LAUNCHER_TASK_DISMISS_SWIPE_UP);
        }
    }
```

具体的改法我就不写了，这里可以直接使用ActivityManager.java里面的forceStopPackage方法，此方案在（9-11都可适用）。 还有安卓十三的方法：

```
diff --git a/frameworks/base/data/etc/com.android.launcher3.xml b/frameworks/base/data/etc/com.android.launcher3.xml
old mode 100644
new mode 100755
index 36a51341f52..9d6b004ccb6
--- a/frameworks/base/data/etc/com.android.launcher3.xml
+++ b/frameworks/base/data/etc/com.android.launcher3.xml
@@ -25,5 +25,6 @@
         <permission name="android.permission.START_TASKS_FROM_RECENTS"/>
         <permission name="android.permission.STATUS_BAR"/>
         <permission name="android.permission.STOP_APP_SWITCHES"/>
+               <permission name="android.permission.FORCE_STOP_PACKAGES"/>
     </privapp-permissions>
 </permissions>
diff --git a/vendor/mediatek/proprietary/packages/apps/Launcher3/quickstep/AndroidManifest.xml b/vendor/mediatek/proprietary/packages/apps/Launcher3/quickstep/AndroidManifest.xml
old mode 100644
new mode 100755
index 352cd3e7b62..1267682a5e3
--- a/vendor/mediatek/proprietary/packages/apps/Launcher3/quickstep/AndroidManifest.xml
+++ b/vendor/mediatek/proprietary/packages/apps/Launcher3/quickstep/AndroidManifest.xml
@@ -37,6 +37,7 @@
     <uses-permission android:name="android.permission.MANAGE_ACCESSIBILITY"/>
     <uses-permission android:name="android.permission.MONITOR_INPUT"/>
     <uses-permission android:name="android.permission.ALLOW_SLIPPERY_TOUCHES"/>
+    <uses-permission android:name="android.permission.FORCE_STOP_PACKAGES"/>
 
     <uses-permission android:name="android.permission.SYSTEM_APPLICATION_OVERLAY" />
 
diff --git a/vendor/mediatek/proprietary/packages/apps/Launcher3/quickstep/src/com/android/quickstep/views/RecentsView.java b/vendor/mediatek/proprietary/packages/apps/Launcher3/quickstep/src/com/android/quickstep/views/RecentsView.java
old mode 100644
new mode 100755
index 2360396cf70..d2d260c64de
--- a/vendor/mediatek/proprietary/packages/apps/Launcher3/quickstep/src/com/android/quickstep/views/RecentsView.java
+++ b/vendor/mediatek/proprietary/packages/apps/Launcher3/quickstep/src/com/android/quickstep/views/RecentsView.java
@@ -189,6 +189,9 @@ import java.util.List;
 import java.util.Objects;
 import java.util.function.Consumer;
 
+import android.app.ActivityManager;
+import android.app.ActivityOptions;
+
 /**
  * A list of recent tasks.
  */
@@ -3127,12 +3130,16 @@ public abstract class RecentsView<ACTIVITY_TYPE extends StatefulActivity<STATE_T
                if (success) {
                    if (shouldRemoveTask) {
                        if (dismissedTaskView.getTask() != null) {
+                       tring pkgname = dismissedTaskView.getTask().key.getPackageName();
+                       Log.d("swl","pkgname: "+pkgname);
                        if (ENABLE_QUICKSTEP_LIVE_TILE.get()&& dismissedTaskView.isRunningTask()) {
                            finishRecentsAnimation(true /* toRecents */, false /* shouldPip */,
-                           () -> removeTaskInternal(dismissedTaskViewId));
+                           () -> removeTaskInternal(dismissedTaskViewId,pkgname));
                        } else {
-                           removeTaskInternal(dismissedTaskViewId);
+                           removeTaskInternal(dismissedTaskViewId,pkgname);
                             }
                             mActivity.getStatsLogManager().logger()
                                     .withItemInfo(dismissedTaskView.getItemInfo())
@@ -3401,7 +3408,7 @@ public abstract class RecentsView<ACTIVITY_TYPE extends StatefulActivity<STATE_T
         return lastVisibleIndex;
     }
 
-    private void removeTaskInternal(int dismissedTaskViewId) {
+    private void removeTaskInternal(int dismissedTaskViewId, String pkgname) {
         int[] taskIds = getTaskIdsForTaskViewId(dismissedTaskViewId);
         int primaryTaskId = taskIds[0];
         int secondaryTaskId = taskIds[1];
@@ -3411,6 +3418,10 @@ public abstract class RecentsView<ACTIVITY_TYPE extends StatefulActivity<STATE_T
                     if (secondaryTaskId != -1) {
                         ActivityManagerWrapper.getInstance().removeTask(secondaryTaskId);
                     }
+                                       ActivityManager am = (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
+                                       Log.d("swl","pkgname: "+pkgname);
+                                       am.forceStopPackage(pkgname);                                   
                 },
                 REMOVE_TASK_WAIT_FOR_APP_STOP_MS);
     }
@@ -3436,6 +3447,12 @@ public abstract class RecentsView<ACTIVITY_TYPE extends StatefulActivity<STATE_T
         int count = getTaskViewCount();
         for (int i = 0; i < count; i++) {
             addDismissedTaskAnimations(requireTaskViewAt(i), duration, anim);
+                       //andy
+                       String pkgname = requireTaskViewAt(i).getTask().key.getPackageName();
+                       Log.d("swl","forceStop: "+pkgname););
+                       ActivityManager am = (ActivityManager) mActivity.getSystemService(Context.ACTIVITY_SERVICE);
+                       am.forceStopPackage(pkgname);                                   
+                       
         }
 
         mPendingAnimation = anim;
```

核心思路基本都是一样的，只是改的方式有点差异，我这个也是参考了一个MTK网站上几年前一个同事提的安卓九的case改出来的，安卓CV工程师哈哈哈，有大佬还有更好的方式，欢迎评论留言
