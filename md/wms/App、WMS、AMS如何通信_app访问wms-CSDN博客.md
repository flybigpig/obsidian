## 

## ![](https://i-blog.csdnimg.cn/blog_migrate/90747e7d7ec1c58cc65deea88ae7abf6.webp?x-image-process=image/format,png)

## 

## 1\. APP通过Token访问[AMS](https://so.csdn.net/so/search?q=AMS&spm=1001.2101.3001.7020)，将Activity、PhoneWindow同ActivityRecord对应

AMS中的ActivityRecord有继承了Binder的Token[内部类](https://so.csdn.net/so/search?q=%E5%86%85%E9%83%A8%E7%B1%BB&spm=1001.2101.3001.7020)，Binder是IBiner接口的实现类。

ActivityRecord继承了WindowToken，WindowToken有类型为`IBinder`的成员`token` ； 所以ActivityRecord也继承到了`IBinder`的成员`token`。

WindowToken的成员`token` ，在WindowToken的构造函数中被赋值。

WindowToken的构造，在ActivityRecord的构造中通过super关键字被调用，因为存在继承关系。

在ActivityRecord的构造中，new出一个其内部类Token的对象，给WindowToken的`token` 成员赋值。

在新建Activity的流程中，ActivityTaskSupervisor.`realStartActivityLocked` 会创建`ClientTransaction` ，进而跟应用进行交互。

给`ClientTransaction`添加`LaunchActivityItem` 类型的Callback。

AMS主导被创建的Activity执行Callback:

ClientLifecycleManager.`scheduleTransaction` → ClientTransaction.`schedule` → ActivityThread.`scheduleTransaction` → TransactionExecutor → `execute` → `executeCallbacks` →

LaunchActivityItem.`execute` → ActivityThread.ActivityClientRecord.`constructor` → 将ActivityRecord的成员`token`给到应用侧的ActivityClientRecord → ActivityThread.`handleLaunchActivity` →

`performLaunchActivity` →Activity.`attach` → Activity的成员`mToken` 被赋值 → PhoneWindow.`setWindowManager` →PhoneWindow、Window的成员`mAppToken` 被赋值 →

Window的成员`mWindowAttributes` 的成员`token` 会在调用`adjustLayoutParamsForSubWindow` 时被赋值

至此，App可以通过Token向AMS发起调用。

比如：start新Activity的时候， 旧Activity在应用进程执行onPaused后，会通过token调用AMS中的ActivityRecord.addToStopping。

ClientLifecycleManager.`scheduleTransaction` → ClientTransaction.`schedule` → ActivityThread.`scheduleTransaction` → TransactionExecutor → `execute` → `executeLifecycleState` →

PauseActivityItem.`postExecute` → ActivityClient.`activityPaused` →ActivityClientController.`activityPaused` →ActivityRecord.`activityPaused` → … →ActivityRecord.`addToStopping`

## 2\. AMS通过ApplicationThread访问App

ActivityThread的内部类`ApplicationThread` ，继承`IApplicationThread.Stub` ，因此ActivityThread是这个Binder的服务端，客户端就是AMS

ActivityThread代表应用侧，应用进程启动时，会执行ActivityThread的main方法，进而执行ActivityThread.`attach` ，进而执行AMS.`attachApplication`

此时，AMS就可以主动向应用侧发起调用了。

AMS通过`ClientTransaction` 控制应用的生命周期，就是通过ActivityThread来调用应用侧。

## 3\. VRI通过Session同WMS通信

## 4\. WMS通过VRI的成员mWindow，将WindowState、VRI、PhoneWindow对应

AMS调用ActivityThread.`handleResumeActivity` →WindowManagerImpl.`addView` →WindowManagerGlobal.`addView` → ViewRootImpl.`setView` →Session`.addToDisplayAsUser(mWindow...)` →

WMS.`addWindow`

`mWindow` 是VRI的成员，类型为VRI的内部类W。

WMS.`addWindow` ，主要完成以下几项工作：1. 新建`WindowToken` 2. 新建`WindowState` 3. 将`IWindow` 类型的`client` 放到`mWindowMap` 保存。

`WindowToken` 的`token`成员是Window；`WindowState` 的`mClient` 是Window。

## 5\. AMS、WMS通过ActivityRecord、WindowToken的继承关系联系起来