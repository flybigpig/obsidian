

[![](https://upload.jianshu.io/users/upload_avatars/26874665/2a40e37f-0985-4482-b744-158567be55c4.PNG?imageMogr2/auto-orient/strip|imageView2/1/w/96/h/96/format/webp)](https://www.jianshu.com/u/167b54662111)

[努比亚技术团队](https://www.jianshu.com/u/167b54662111)关注IP属地: 贵州

122021.09.13 18:16:33字数 8,760阅读 49,958

_努比亚技术团队原创内容，转载请务必注明出处。_

# 1.前言

从用户手指点击桌面上的应用图标到屏幕上显示出应用主Activity界面而完成应用启动，快的话往往都不需要一秒钟，但是这整个过程却是十分复杂的，其中涉及了Android系统的几乎所有核心知识点。同时应用的启动速度也绝对是系统的核心用户体验指标之一，多少年来，无论是谷歌或是手机系统厂商们还是各个Android应用开发者，都在为实现应用打开速度更快一点的目标而不断努力。但是要想真正做好应用启动速度优化这件事情，我想是必须要对应用启动的整个流程有充分的认识和理解的，所以无论作为Android系统或应用开发者，都有必要好好的学习和了解一下这个过程的。网上有很多介绍应用启动流程源码的文章，但是总感觉大多数都不太全面，很多只是介绍了应用启动过程中的部分流程，而没有总体清晰的认识应用启动过程的总体脉络与系统架构设计思想。所以本文将结合笔者多年来的工作经历，结合systrace分析工具，基于最新Android R AOSP源码完整的分析一下这个从用户手指触控点击屏幕应用图标到应用界面展示到屏幕上的整个应用启动过程，也是对之前所做所学的一个总结与归纳。

# 2.大纲

- **Android触控事件处理机制**
    
- **Zygote进程启动和应用进程创建流程**
    
- **Handler消息机制**
    
- **AMS的Activity组件管理**
    
- **应用Application和Activity组件创建与初始化**
    
- **应用UI布局与绘制**
    
- **RenderThread渲染**
    
- **SurfaceFlinger合成显示**
    
- **写在最后**
    
- **参考**
    

# 3. Input触控事件处理流程

## 3.1 系统机制分析

`Android` 系统是由事件驱动的，而 `input` 是最常见的事件之一，用户的点击、滑动、长按等操作，都属于 `input` 事件驱动，其中的核心就是 `InputReader` 和 `InputDispatcher`。`InputReader` 和 `InputDispatcher` 是跑在 `SystemServer`进程中的两个 `native` 循环线程，负责读取和分发 `Input` 事件。整个处理过程大致流程如下：

1. `InputReader`负责从`EventHub`里面把`Input`事件读取出来，然后交给 `InputDispatcher` 进行事件分发；
2. `InputDispatcher`在拿到 `InputReader`获取的事件之后，对事件进行包装后，寻找并分发到目标窗口;
3. `InboundQueue`队列（“iq”）中放着`InputDispatcher`从`InputReader`中拿到的`input`事件；
4. `OutboundQueue`（“oq”）队列里面放的是即将要被派发给各个目标窗口App的事件；
5. `WaitQueue`队列里面记录的是已经派发给 `App`（“wq”），但是 `App`还在处理没有返回处理成功的事件；
6. `PendingInputEventQueue`队列（“aq”）中记录的是应用需要处理的`Input`事件，这里可以看到`input`事件已经传递到了应用进程；
7. `deliverInputEvent` 标识 `App` `UI Thread` 被 `Input` 事件唤醒；
8. `InputResponse` 标识 `Input` 事件区域，这里可以看到一个 `Input_Down` 事件 + 若干个 `Input_Move` 事件 + 一个 `Input_Up` 事件的处理阶段都被算到了这里；
9. `App` 响应处理`Input` 事件，内部会在其界面`View`树中传递处理。

用一张图描述整个过程大致如下：

  

![](https://upload-images.jianshu.io/upload_images/26874665-9a6f99f4f9970bb6.PNG?imageMogr2/auto-orient/strip|imageView2/2/w/1159/format/webp)

input模型.PNG

## 3.2 结合Systrace分析

从桌面点击应用图标启动应用，`system_server`的`native`线程`InputReader`首先负责从`EventHub`中利用`linux`的`epolle`机制监听并从屏幕驱动读取上报的触控事件，然后唤醒另外一条native线程`InputDispatcher`负责进行进一步事件分发。`InputDispatcher`中会先将事件放到`InboundQueue`也就是“iq”队列中，然后寻找具体处理`input`事件的目标应用窗口，并将事件放入对应的目标窗口`OutboundQueue`也就是“oq”队列中等待通过`SocketPair`双工信道发送到应用目标窗口中。最后当事件发送给具体的应用目标窗口后，会将事件移动到`WaitQueue`也就是“wq”中等待目标应用处理事件完成，并开启倒计时，**如果目标应用窗口在5S内没有处理完成此次触控事件，就会向`system_server`报应用ANR异常事件**。以上整个过程在`Android`系统源码中都加有相应的systrace tag，如下systrace截图所示：  

![](https://upload-images.jianshu.io/upload_images/26874665-e0a5a7ddd33e8c08.png?imageMogr2/auto-orient/strip|imageView2/2/w/900/format/webp)

input事件处理1.png

  
接着上面的流程继续往下分析：当`input`触控事件传递到桌面应用进程后，`Input`事件到来后先通过`enqueueInputEvent`函数放入“aq”本地待处理队列中，并唤醒应用的UI线程在`deliverInputEvent`的流程中进行`input`事件的具体分发与处理。具体会先交给在应用界面`Window`创建时的`ViewRootImpl#setView`流程中创建的多个不同类型的`InputStage`中依次进行处理（比如对输入法处理逻辑的封装`ImeInputStage`），**整个处理流程是按照责任链的设计模式进行**。最后会交给`ViewPostImeInputStage`中具体进行处理，这里面会从`View`布局树的根节点`DecorView`开始遍历整个`View`树上的每一个子`View`或`ViewGroup`界面进行事件的分发、拦截、处理的逻辑。最后触控事件处理完成后会调用`finishInputEvent`结束应用对触控事件处理逻辑，这里面会通过`JNI`调用到`native`层`InputConsumer`的`sendFinishedSignal`函数通知`InputDispatcher`事件处理完成，从触发从"wq"队列中及时移除待处理事件以免报`ANR`异常。  

![](https://upload-images.jianshu.io/upload_images/26874665-91c5c6d65d79417b.png?imageMogr2/auto-orient/strip|imageView2/2/w/887/format/webp)

Input事件处理2.png

  
桌面应用界面View中在连续处理一个`ACTION_DOWN`的`TouchEvent`触控事件和多个`ACTION_MOVE`，直到最后出现一个`ACTION_UP`的`TouchEvent`事件后，判断属于`onClick`点击事件，然后透过`ActivityManager` `Binder`调用`AMS`的`startActivity`服务接口触发启动应用的逻辑。从systrace上看如下图所示：

![](https://upload-images.jianshu.io/upload_images/26874665-0f0a499e1b1211de.png?imageMogr2/auto-orient/strip|imageView2/2/w/856/format/webp)

input事件处理3.png

# 4. 应用进程的创建与启动

## 4.1 Pause桌面应用

接着上一节继续往下看，桌面进程收到input触控事件并处理后`binder`调用框架`AMS`的的`startActivity`接口启动应用，相关简化代码如下：

```java
  /*frameworks/base/services/core/java/com/android/server/wm/ActivityStarter.java*/
  private int startActivityUnchecked(final ActivityRecord r, ActivityRecord sourceRecord,
                IVoiceInteractionSession voiceSession, IVoiceInteractor voiceInteractor,
                int startFlags, boolean doResume, ActivityOptions options, Task inTask,
                boolean restrictedBgActivity, NeededUriGrants intentGrants) {
        ...
        try {
            ...
            // 添加“startActivityInner”的systrace tag
            Trace.traceBegin(Trace.TRACE_TAG_WINDOW_MANAGER, "startActivityInner");
            // 执行startActivityInner启动应用的逻辑
            result = startActivityInner(r, sourceRecord, voiceSession, voiceInteractor,
                    startFlags, doResume, options, inTask, restrictedBgActivity, intentGrants);
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_WINDOW_MANAGER);
            ...
        }
        ...
    }
```

在执行`startActivityInner`启动应用逻辑中，`AMS`中的`Activity`栈管理的逻辑，**检查发现当前处于前台`Resume`状态的`Activity`是桌面应用，所以第一步需要通知桌面应用的`Activity`进入`Paused`状态**，相关简化代码逻辑如下：

```java
/*frameworks/base/services/core/java/com/android/server/wm/ActivityStack.java*/
private boolean resumeTopActivityInnerLocked(ActivityRecord prev, ActivityOptions options) {
   ...
   // mResumedActivity不为null，说明当前存在处于resume状态的Activity且不是新需要启动的应用
   if (mResumedActivity != null) {
      // 执行startPausingLocked通知桌面应用进入paused状态
      pausing |= startPausingLocked(userLeaving, false /* uiSleeping */, next);
   }
   ...
}

final boolean startPausingLocked(boolean userLeaving, boolean uiSleeping,
            ActivityRecord resuming) {
    ...
    ActivityRecord prev = mResumedActivity;
    ...
    if (prev.attachedToProcess()) {
        try {
             ...
             // 相关执行动作封装事务，binder通知mResumedActivity也就是桌面执行pause动作
             mAtmService.getLifecycleManager().scheduleTransaction(prev.app.getThread(),
                        prev.appToken, PauseActivityItem.obtain(prev.finishing, userLeaving,
                        prev.configChangeFlags, pauseImmediately));
        } catch (Exception e) {
           ...
        }
     }
     ...
}
```

桌面应用进程这边执行收到`pause`消息后执行`Activity`的`onPause`生命周期，并在执行完成后，会`binder`调用`AMS`的`activityPaused`接口通知系统执行完`activity`的`pause`动作，相关代码如下：

```java
  /*frameworks/base/core/java/android/app/servertransaction/PauseActivityItem.java*/
  @Override
  public void postExecute(ClientTransactionHandler client, IBinder token,
            PendingTransactionActions pendingActions) {
        ...
        try {
            // binder通知AMS当前应用activity已经执行完pause的流程
            ActivityTaskManager.getService().activityPaused(token);
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    }
```

`AMS`这边收到应用的`activityPaused`调用后，继续执行启动应用的逻辑，**判断需要启动的应用`Activity`所在的进程不存在，所以接下来需要先`startProcessAsync`创建应用进程**，相关简化代码如下：

```java
/*frameworks/base/services/core/java/com/android/server/wm/ActivityStackSupervisor.java*/
 void startSpecificActivity(ActivityRecord r, boolean andResume, boolean checkConfig) {
        final WindowProcessController wpc =
                mService.getProcessController(r.processName, r.info.applicationInfo.uid);
        ...
        // 1.如果wpc不为null且hasThread表示应用Activity所属进程存在，直接realStartActivityLocked启动Activity
        if (wpc != null && wpc.hasThread()) {
            try {
                realStartActivityLocked(r, wpc, andResume, checkConfig);
                return;
            } catch (RemoteException e) {
                Slog.w(TAG, "Exception when starting activity "
                        + r.intent.getComponent().flattenToShortString(), e);
            }
           ...
        }
        ...
        // 2.否则，调用AMS的startProcessAsync正式开始创建应用进程 
        mService.startProcessAsync(r, knownToBeDead, isTop, isTop ? "top-activity" : "activity");
    }
```

以上过程从systrace上看，如下图所示：

1. 通知pause桌面应用：  
    
    ![](https://upload-images.jianshu.io/upload_images/26874665-5609e542da7f72c6.png?imageMogr2/auto-orient/strip|imageView2/2/w/900/format/webp)
    
    launcher_paused.png
    
      
    2.确认桌面`activityPaused`状态之后，开始创建应用进程：  
    
    ![](https://upload-images.jianshu.io/upload_images/26874665-8dc22f45c77ec522.png?imageMogr2/auto-orient/strip|imageView2/2/w/886/format/webp)
    
    start_app_proc.png
    

## 4.2 创建应用进程

接上一小节的分析可以知道，`Android`应用进程的启动是**被动式**的，在桌面点击图标启动一个应用的组件如`Activity`时，如果`Activity`所在的进程不存在，就会创建并启动进程。**`Android`系统中一般应用进程的创建都是统一由`zygote`进程`fork`创建的，`AMS`在需要创建应用进程时，会通过`socket`连接并通知到到`zygote`进程在开机阶段就创建好的`socket`服务端，然后由`zygote`进程`fork`创建出应用进程。**整体架构如下图所示：  

![](https://upload-images.jianshu.io/upload_images/26874665-d35e3ceb51181b5a.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

应用进程创建流程图.png

  
我们接着上节中的分析，继续从`AMS#startProcessAsync`创建进程函数入手，继续看一下应用进程创建相关简化流程代码：

### 4.2.1 AMS 发送socket请求

```java
  /*frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java*/  
   @GuardedBy("this")
    final ProcessRecord startProcessLocked(...) {
        return mProcessList.startProcessLocked(...);
   }
   
   /*frameworks/base/services/core/java/com/android/server/am/ProcessList.java*/
   private Process.ProcessStartResult startProcess(HostingRecord hostingRecord, String entryPoint,
            ProcessRecord app, int uid, int[] gids, int runtimeFlags, int zygotePolicyFlags,
            int mountExternal, String seInfo, String requiredAbi, String instructionSet,
            String invokeWith, long startTime) {
        try {
            // 原生标识应用进程创建所加的systrace tag
            Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "Start proc: " +
                    app.processName);
            ...
            // 调用Process的start方法创建进程
            startResult = Process.start(...);
            ...
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
        }
    }
    
    /*frameworks/base/core/java/android/os/Process.java*/
    public static ProcessStartResult start(...) {
        // 调用ZygoteProcess的start函数
        return ZYGOTE_PROCESS.start(...);
    }
    
    /*frameworks/base/core/java/android/os/ZygoteProcess.java*/
    public final Process.ProcessStartResult start(...){
        try {
            return startViaZygote(...);
        } catch (ZygoteStartFailedEx ex) {
           ...
        }
    }
    
    private Process.ProcessStartResult startViaZygote(...){
        ArrayList<String> argsForZygote = new ArrayList<String>();
        ...
        return zygoteSendArgsAndGetResult(openZygoteSocketIfNeeded(abi), argsForZygote);
    }

```

在`ZygoteProcess#startViaZygote`中，最后创建应用进程的逻辑：

1. **`openZygoteSocketIfNeeded`函数中打开本地`socket`客户端连接到`zygote`进程的`socket`服务端**；
2. **`zygoteSendArgsAndGetResult`发送`socket`请求参数，带上了创建的应用进程参数信息**；
3. **`return`返回的数据结构`ProcessStartResult`中会有新创建的进程的`pid`字段**。

从systrace上看这个过程如下：

  

![](https://upload-images.jianshu.io/upload_images/26874665-836153b3703e3e11.png?imageMogr2/auto-orient/strip|imageView2/2/w/1154/format/webp)

start_proc.png

### 4.2.2 Zygote 处理socket请求

其实早在系统开机阶段，`zygote`进程创建时，就会在`ZygoteInit#main`入口函数中创建服务端`socket`，**并预加载系统资源和框架类（加速应用进程启动速度）**，代码如下：

```csharp
 /*frameworks/base/core/java/com/android/internal/os/ZygoteInit.java*/
 public static void main(String[] argv) {
        ZygoteServer zygoteServer = null;
         ...
        try {
            ...
            // 1.preload提前加载框架通用类和系统资源到进程，加速进程启动
            preload(bootTimingsTraceLog);
            ...
            // 2.创建zygote进程的socket server服务端对象
            zygoteServer = new ZygoteServer(isPrimaryZygote);
            ...
            // 3.进入死循环，等待AMS发请求过来
            caller = zygoteServer.runSelectLoop(abiList);
        } catch (Throwable ex) {
            ...
        } finally {
            ...
        }
        ...
    }
```

继续往下看`ZygoteServer#runSelectLoop`如何监听并处理AMS客户端的请求：

```java
 /*frameworks/base/core/java/com/android/internal/os/ZygoteServer.java*/
 Runnable runSelectLoop(String abiList) {
     // 进入死循环监听
     while (true) {
        while (--pollIndex >= 0) {
           if (pollIndex == 0) {
             ...
           } else if (pollIndex < usapPoolEventFDIndex) {
             // Session socket accepted from the Zygote server socket
             // 得到一个请求连接封装对象ZygoteConnection
             ZygoteConnection connection = peers.get(pollIndex);
             // processCommand函数中处理AMS客户端请求
             final Runnable command = connection.processCommand(this, multipleForksOK);
           }
        }
     }
 }
 
 Runnable processCommand(ZygoteServer zygoteServer, boolean multipleOK) {
         ...
         // 1.fork创建应用子进程
         pid = Zygote.forkAndSpecialize(...);
         try {
             if (pid == 0) {
                 ...
                 // 2.pid为0，当前处于新创建的子应用进程中，处理请求参数
                 return handleChildProc(parsedArgs, childPipeFd, parsedArgs.mStartChildZygote);
             } else {
                 ...
                 handleParentProc(pid, serverPipeFd);
             }
          } finally {
             ...
          }
 }
 
  private Runnable handleChildProc(ZygoteArguments parsedArgs,
            FileDescriptor pipeFd, boolean isZygote) {
        ...
        // 关闭从父进程zygote继承过来的ZygoteServer服务端地址
        closeSocket();
        ...
        if (parsedArgs.mInvokeWith != null) {
           ...
        } else {
            if (!isZygote) {
                // 继续进入ZygoteInit#zygoteInit继续完成子应用进程的相关初始化工作
                return ZygoteInit.zygoteInit(parsedArgs.mTargetSdkVersion,
                        parsedArgs.mDisabledCompatChanges,
                        parsedArgs.mRemainingArgs, null /* classLoader */);
            } else {
                ...
            }
        }
    }
```

以上过程从systrace上看如下图所示：

  

![](https://upload-images.jianshu.io/upload_images/26874665-29c16b48721bb074.png?imageMogr2/auto-orient/strip|imageView2/2/w/1092/format/webp)

zygote_fork.png

### 4.2.3 应用进程初始化

接上一节中的分析，`zygote`进程监听接收`AMS`的请求，`fork`创建子应用进程，然后`pid`为0时进入子进程空间，然后在 `ZygoteInit#zygoteInit`中完成进程的初始化动作，相关简化代码如下：

```cpp
/*frameworks/base/core/java/com/android/internal/os/ZygoteInit.java*/
public static Runnable zygoteInit(int targetSdkVersion, long[] disabledCompatChanges,
            String[] argv, ClassLoader classLoader) {
        ...
        // 原生添加名为“ZygoteInit ”的systrace tag以标识进程初始化流程
        Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "ZygoteInit");
        RuntimeInit.redirectLogStreams();
        // 1.RuntimeInit#commonInit中设置应用进程默认的java异常处理机制
        RuntimeInit.commonInit();
        // 2.ZygoteInit#nativeZygoteInit函数中JNI调用启动进程的binder线程池
        ZygoteInit.nativeZygoteInit();
        // 3.RuntimeInit#applicationInit中反射创建ActivityThread对象并调用其“main”入口方法
        return RuntimeInit.applicationInit(targetSdkVersion, disabledCompatChanges, argv,
                classLoader);
 }
```

应用进程启动后，初始化过程中主要依次完成以下几件事情：

1. **应用进程默认的`java`异常处理机制（可以实现监听、拦截应用进程所有的`Java crash`的逻辑）；**
2. **`JNI`调用启动进程的`binder`线程池（注意应用进程的`binder`线程池资源是自己创建的并非从`zygote`父进程继承的）；**
3. **通过反射创建`ActivityThread`对象并调用其“`main`”入口方法。**

我们继续看`RuntimeInit#applicationInit`简化的代码流程：

```java
 /*frameworks/base/core/java/com/android/internal/os/RuntimeInit.java*/
 protected static Runnable applicationInit(int targetSdkVersion, long[] disabledCompatChanges,
            String[] argv, ClassLoader classLoader) {
        ...
        // 结束“ZygoteInit ”的systrace tag
        Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
        // Remaining arguments are passed to the start class's static main
        return findStaticMain(args.startClass, args.startArgs, classLoader);
  }
  
  protected static Runnable findStaticMain(String className, String[] argv,
            ClassLoader classLoader) {
        Class<?> cl;
        try {
            // 1.反射加载创建ActivityThread类对象
            cl = Class.forName(className, true, classLoader);
        } catch (ClassNotFoundException ex) {
            ...
        }
        Method m;
        try {
            // 2.反射调用其main方法
            m = cl.getMethod("main", new Class[] { String[].class });
        } catch (NoSuchMethodException ex) {
            ...
        } catch (SecurityException ex) {
            ...
        }
        ...
        // 3.触发执行以上逻辑
        return new MethodAndArgsCaller(m, argv);
    }
```

我们继续往下看`ActivityThread`的`main`函数中又干了什么：

```java
/*frameworks/base/core/java/android/app/ActivityThread.java*/
public static void main(String[] args) {
     // 原生添加的标识进程ActivityThread初始化过程的systrace tag，名为“ActivityThreadMain”
     Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "ActivityThreadMain");
     ...
     // 1.创建并启动主线程的loop消息循环
     Looper.prepareMainLooper();
     ...
     // 2.attachApplication注册到系统AMS中
     ActivityThread thread = new ActivityThread();
     thread.attach(false, startSeq);
     ...
     Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
     Looper.loop();
     ...
}

private void attach(boolean system, long startSeq) {
    ...
    if (!system) {
       ...
       final IActivityManager mgr = ActivityManager.getService();
       try {
          // 通过binder调用AMS的attachApplication接口将自己注册到AMS中
          mgr.attachApplication(mAppThread, startSeq);
       } catch (RemoteException ex) {
                throw ex.rethrowFromSystemServer();
       }
    }
}
```

可以看到进程`ActivityThread#main`函数初始化的主要逻辑是：

1. **创建并启动主线程的`loop`消息循环；**
2. **通过`binder`调用`AMS`的`attachApplication`接口将自己`attach`注册到`AMS`中。**

以上初始化过程。从systrace上看如下图所示：

  

![](https://upload-images.jianshu.io/upload_images/26874665-b3b3d924cf050777.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

activitythread_main.png

# 5. 应用主线程消息循环机制建立

接上一节的分析，我们知道应用进程创建后会通过反射创建`ActivityThread`对象并执行其`main`函数，进行主线程的初始化工作：

```java
/*frameworks/base/core/java/android/app/ActivityThread.java*/
public static void main(String[] args) {
     ...
     // 1.创建Looper、MessageQueue
     Looper.prepareMainLooper();
     ...
     // 2.启动loop消息循环，开始准备接收消息
     Looper.loop();
     ...
}

// 3.创建主线程Handler对象
final H mH = new H();

class H extends Handler {
  ...
}

/*frameworks/base/core/java/android/os/Looper.java*/
public static void prepareMainLooper() {
     // 准备主线程的Looper
     prepare(false);
     synchronized (Looper.class) {
          if (sMainLooper != null) {
              throw new IllegalStateException("The main Looper has already been prepared.");
          }
          sMainLooper = myLooper();
     }
}

private static void prepare(boolean quitAllowed) {
      if (sThreadLocal.get() != null) {
          throw new RuntimeException("Only one Looper may be created per thread");
      }
      // 创建主线程的Looper对象，并通过ThreadLocal机制实现与主线程的一对一绑定
      sThreadLocal.set(new Looper(quitAllowed));
}

private Looper(boolean quitAllowed) {
      // 创建MessageQueue消息队列
      mQueue = new MessageQueue(quitAllowed);
      mThread = Thread.currentThread();
}
```

主线程初始化完成后，**主线程就有了完整的 `Looper`、`MessageQueue`、`Handler`，此时 `ActivityThread` 的 `Handler` 就可以开始处理 `Message`，包括 `Application`、`Activity`、`ContentProvider`、`Service`、`Broadcast` 等组件的生命周期函数，都会以 `Message` 的形式，在主线程按照顺序处理**，这就是 `App` 主线程的初始化和运行原理，部分处理的 `Message` 如下

```java
/*frameworks/base/core/java/android/app/ActivityThread.java*/
class H extends Handler {
        public static final int BIND_APPLICATION        = 110;
        @UnsupportedAppUsage
        public static final int RECEIVER                = 113;
        @UnsupportedAppUsage
        public static final int CREATE_SERVICE          = 114;
        @UnsupportedAppUsage
        public static final int BIND_SERVICE            = 121;
        
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case BIND_APPLICATION:
                    Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "bindApplication");
                    AppBindData data = (AppBindData)msg.obj;
                    handleBindApplication(data);
                    Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
                    break;
                    ...
            }
         }
         ...
}
```

主线程初始化完成后，主线程就进入阻塞状态，等待 `Message`，一旦有 `Message` 发过来，主线程就会被唤醒，处理 `Message`，处理完成之后，如果没有其他的 `Message` 需要处理，那么主线程就会进入休眠阻塞状态继续等待。可以说`Android`系统的运行是受消息机制驱动的，而整个消息机制是由上面所说的四个关键角色相互配合实现的（**`Handler`**、**`Looper`**、**`MessageQueue`**、**`Message`**），其运行原理如下图所示：

![](https://upload-images.jianshu.io/upload_images/26874665-816bcf754eef9a06.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/618/format/webp)

Android消息机制.jpg

1. **`Handler`** : `Handler` 主要是用来处理 `Message`，应用可以在任何线程创建 `Handler`，只要在创建的时候指定对应的 `Looper` 即可，如果不指定，默认是在当前 `Thread` 对应的 `Looper`。
2. **`Looper` :** `Looper` 可以看成是一个循环器，**其 `loop` 方法开启后，不断地从 `MessageQueue` 中获取 `Message`**，对 `Message` 进行 `Delivery` 和 `Dispatch`，最终发给对应的 `Handler` 去处理。
3. `**MessageQueue**：MessageQueue` 就是一个 `Message` 管理器，队列中是 `Message`，在没有 `Message` 的时候，**`MessageQueue` 借助 `Linux` 的 `ePoll`机制，阻塞休眠等待，直到有 `Message` 进入队列将其唤醒**。
4. `**Message**：Message` 是传递消息的对象，其内部包含了要传递的内容，最常用的包括 `what`、`arg`、`callback` 等。

# 6. 应用Application和Activity组件创建与初始化

## 6.1 Application的创建与初始化

从前面4.2.3小结中的分析我们知道，应用进程启动初始化执行`ActivityThread#main`函数过程中，在开启主线程`loop`消息循环之前，会通过`Binder`调用系统核心服务`AMS`的`attachApplication`接口将自己注册到`AMS`中。下面我们接着这个流程往下看，我们先从systrace上看看`AMS`服务的`attachApplication`接口是如何处理应用进程的attach注册请求的：  

![](https://upload-images.jianshu.io/upload_images/26874665-c306af21df6dbeef.png?imageMogr2/auto-orient/strip|imageView2/2/w/904/format/webp)

attachApplication.png

  
我们继续来看相关代码的简化流程：

```java
/*frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java*/
@GuardedBy("this")
private boolean attachApplicationLocked(@NonNull IApplicationThread thread,
            int pid, int callingUid, long startSeq) {
     ...
     if (app.isolatedEntryPoint != null) {
           ...
     } else if (instr2 != null) {
           // 1.通过oneway异步类型的binder调用应用进程ActivityThread#IApplicationThread#bindApplication接口
           thread.bindApplication(...);
     } else {
           thread.bindApplication(...);
     }
     ...
     // See if the top visible activity is waiting to run in this process...
     if (normalMode) {
          try {
            // 2.继续执行启动应用Activity的流程
            didSomething = mAtmInternal.attachApplication(app.getWindowProcessController());
          } catch (Exception e) {
                Slog.wtf(TAG, "Exception thrown launching activities in " + app, e);
                badApp = true;
          }
      }
}

/*frameworks/base/core/java/android/app/ActivityThread.java*/
private class ApplicationThread extends IApplicationThread.Stub {
      @Override
      public final void bindApplication(...) {
            ...
            AppBindData data = new AppBindData();
            data.processName = processName;
            data.appInfo = appInfo;
            ...
            // 向应用进程主线程Handler发送BIND_APPLICATION消息，触发在应用主线程执行handleBindApplication初始化动作
            sendMessage(H.BIND_APPLICATION, data);
      }
      ...
}

class H extends Handler {
      ...
      public void handleMessage(Message msg) {
           switch (msg.what) {
                case BIND_APPLICATION:
                    Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "bindApplication");
                    AppBindData data = (AppBindData)msg.obj;
                    // 在应用主线程执行handleBindApplication初始化动作
                    handleBindApplication(data);
                    Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
                    break;
                    ...
           }
      }
      ...
}

@UnsupportedAppUsage
private void handleBindApplication(AppBindData data) {
    ...
}
```

从上面的代码流程可以看出：**`AMS`服务在执行应用的`attachApplication`注册请求过程中，会通过`oneway`类型的`binder`调用应用进程`ActivityThread#IApplicationThread`的`bindApplication`接口，而`bindApplication`接口函数实现中又会通过往应用主线程消息队列post `BIND_APPLICATION`消息触发执行`handleBindApplication`初始化函数**，从systrace看如下图所示：  

![](https://upload-images.jianshu.io/upload_images/26874665-adfb82322a9b2e44.png?imageMogr2/auto-orient/strip|imageView2/2/w/1193/format/webp)

handleBindApplication.png

  
我们继续结合代码看看handleBindApplication的简化关键流程：

```kotlin
/*frameworks/base/core/java/android/app/ActivityThread.java*/
@UnsupportedAppUsage
private void handleBindApplication(AppBindData data) {
    ...
    // 1.创建应用的LoadedApk对象
    data.info = getPackageInfoNoCheck(data.appInfo, data.compatInfo);
    ...
    // 2.创建应用Application的Context、触发Art虚拟机加载应用APK的Dex文件到内存中，并加载应用APK的Resource资源
    final ContextImpl appContext = ContextImpl.createAppContext(this, data.info);
    ...
    // 3.调用LoadedApk的makeApplication函数，实现创建应用的Application对象
    app = data.info.makeApplication(data.restrictedBackupMode, null);
    ...
    // 4.执行应用Application#onCreate生命周期函数
    mInstrumentation.onCreate(data.instrumentationArgs);
    ...
}
```

在`ActivityThread#**handleBindApplication`初始化过程中在应用主线程中主要完成如下几件事件**：

1. 根据框架传入的`ApplicationInfo`信息创建应用`APK`对应的`LoadedApk`对象;
2. 创建应用`Application`的`Context`对象；
3. **创建类加载器`ClassLoader`对象并触发`Art`虚拟机执行`OpenDexFilesFromOat`动作加载应用`APK`的`Dex`文件**；
4. **通过`LoadedApk`加载应用`APK`的`Resource`资源**；
5. 调用`LoadedApk`的`makeApplication`函数，创建应用的`Application`对象;
6. **执行应用`Application#onCreate`生命周期函数**（`APP`应用开发者能控制的第一行代码）;

下面我们结合代码重点看看`APK Dex`文件的加载和`Resource`资源的加载流程。

### 6.1.1 应用APK的Dex文件加载

```java
/*frameworks/base/core/java/android/app/ContextImpl.java*/
static ContextImpl createAppContext(ActivityThread mainThread, LoadedApk packageInfo,
            String opPackageName) {
    if (packageInfo == null) throw new IllegalArgumentException("packageInfo");
    // 1.创建应用Application的Context对象
    ContextImpl context = new ContextImpl(null, mainThread, packageInfo, null, null, null, null,
                0, null, opPackageName);
    // 2.触发加载APK的DEX文件和Resource资源
    context.setResources(packageInfo.getResources());
    context.mIsSystemOrSystemUiContext = isSystemOrSystemUI(context);
    return context;
}

/*frameworks/base/core/java/android/app/LoadedApk.java*/
@UnsupportedAppUsage
public Resources getResources() {
     if (mResources == null) {
         ...
         // 加载APK的Resource资源
         mResources = ResourcesManager.getInstance().getResources(null, mResDir,
                    splitPaths, mOverlayDirs, mApplicationInfo.sharedLibraryFiles,
                    Display.DEFAULT_DISPLAY, null, getCompatibilityInfo(),
                    getClassLoader()/*触发加载APK的DEX文件*/, null);
      }
      return mResources;
}

@UnsupportedAppUsage
public ClassLoader getClassLoader() {
     synchronized (this) {
         if (mClassLoader == null) {
             createOrUpdateClassLoaderLocked(null /*addedPaths*/);
          }
          return mClassLoader;
     }
}

private void createOrUpdateClassLoaderLocked(List<String> addedPaths) {
     ...
     if (mDefaultClassLoader == null) {
          ...
          // 创建默认的mDefaultClassLoader对象，触发art虚拟机加载dex文件
          mDefaultClassLoader = ApplicationLoaders.getDefault().getClassLoaderWithSharedLibraries(
                    zip, mApplicationInfo.targetSdkVersion, isBundledApp, librarySearchPath,
                    libraryPermittedPath, mBaseClassLoader,
                    mApplicationInfo.classLoaderName, sharedLibraries);
          ...
     }
     ...
     if (mClassLoader == null) {
         // 赋值给mClassLoader对象
         mClassLoader = mAppComponentFactory.instantiateClassLoader(mDefaultClassLoader,
                    new ApplicationInfo(mApplicationInfo));
     }
}

/*frameworks/base/core/java/android/app/ApplicationLoaders.java*/
ClassLoader getClassLoaderWithSharedLibraries(...) {
    // For normal usage the cache key used is the same as the zip path.
    return getClassLoader(zip, targetSdkVersion, isBundled, librarySearchPath,
                              libraryPermittedPath, parent, zip, classLoaderName, sharedLibraries);
}

private ClassLoader getClassLoader(String zip, ...) {
        ...
        synchronized (mLoaders) {
            ...
            if (parent == baseParent) {
                ...
                // 1.创建BootClassLoader加载系统框架类，并增加相应的systrace tag
                Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, zip);
                ClassLoader classloader = ClassLoaderFactory.createClassLoader(
                        zip,  librarySearchPath, libraryPermittedPath, parent,
                        targetSdkVersion, isBundled, classLoaderName, sharedLibraries);
                Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
                ...
                return classloader;
            }
            // 2.创建PathClassLoader加载应用APK的Dex类，并增加相应的systrace tag
            Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, zip);
            ClassLoader loader = ClassLoaderFactory.createClassLoader(
                    zip, null, parent, classLoaderName, sharedLibraries);
            Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
            return loader;
        }
}

/*frameworks/base/core/java/com/android/internal/os/ClassLoaderFactory.java*/
public static ClassLoader createClassLoader(...) {
        // 通过new的方式创建ClassLoader对象，最终会触发art虚拟机加载APK的dex文件
        ClassLoader[] arrayOfSharedLibraries = (sharedLibraries == null)
                ? null
                : sharedLibraries.toArray(new ClassLoader[sharedLibraries.size()]);
        if (isPathClassLoaderName(classloaderName)) {
            return new PathClassLoader(dexPath, librarySearchPath, parent, arrayOfSharedLibraries);
        }
        ...
}
```

从以上代码可以看出：在创建`Application`的`Context`对象后会立马尝试去加载`APK`的`Resource`资源，而在这之前需要通过`LoadedApk`去创建类加载器`ClassLoader`对象，而这个过程最终就会触发`Art`虚拟机加载应用`APK`的`dex`文件，从systrace上看如下图所示：  

![](https://upload-images.jianshu.io/upload_images/26874665-7161620d25eabba2.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

OpenDexFilesFromOat.png

  
具体art虚拟机加载dex文件的流程由于篇幅所限这里就不展开讲了，这边画了一张流程图可以参考一下，感兴趣的读者可以对照追一下源码流程：  

![](https://upload-images.jianshu.io/upload_images/26874665-e4c79f7c401c356e.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

Art虚拟机Dex加载流程.jpg

### 6.1.2 应用APK的Resource资源加载

```java
/*frameworks/base/core/java/android/app/LoadedApk.java*/
@UnsupportedAppUsage
public Resources getResources() {
     if (mResources == null) {
         ...
         // 加载APK的Resource资源
         mResources = ResourcesManager.getInstance().getResources(null, mResDir,
                    splitPaths, mOverlayDirs, mApplicationInfo.sharedLibraryFiles,
                    Display.DEFAULT_DISPLAY, null, getCompatibilityInfo(),
                    getClassLoader()/*触发加载APK的DEX文件*/, null);
      }
      return mResources;
}

/*frameworks/base/core/java/android/app/ResourcesManager.java*/
public @Nullable Resources getResources(...) {
      try {
          // 原生Resource资源加载的systrace tag
          Trace.traceBegin(Trace.TRACE_TAG_RESOURCES, "ResourcesManager#getResources");
          ...
          return createResources(activityToken, key, classLoader, assetsSupplier);
      } finally {
          Trace.traceEnd(Trace.TRACE_TAG_RESOURCES);
      }
}

private @Nullable Resources createResources(...) {
      synchronized (this) {
            ...
            // 执行创建Resources资源对象
            ResourcesImpl resourcesImpl = findOrCreateResourcesImplForKeyLocked(key, apkSupplier);
            if (resourcesImpl == null) {
                return null;
            }
            ...
     }
}

private @Nullable ResourcesImpl findOrCreateResourcesImplForKeyLocked(
            @NonNull ResourcesKey key, @Nullable ApkAssetsSupplier apkSupplier) {
      ...
      impl = createResourcesImpl(key, apkSupplier);
      ...
}

private @Nullable ResourcesImpl createResourcesImpl(@NonNull ResourcesKey key,
            @Nullable ApkAssetsSupplier apkSupplier) {
        ...
        // 创建AssetManager对象，真正实现的APK文件加载解析动作
        final AssetManager assets = createAssetManager(key, apkSupplier);
        ...
}

private @Nullable AssetManager createAssetManager(@NonNull final ResourcesKey key,
            @Nullable ApkAssetsSupplier apkSupplier) {
        ...
        for (int i = 0, n = apkKeys.size(); i < n; i++) {
            final ApkKey apkKey = apkKeys.get(i);
            try {
                // 通过loadApkAssets实现应用APK文件的加载
                builder.addApkAssets(
                        (apkSupplier != null) ? apkSupplier.load(apkKey) : loadApkAssets(apkKey));
            } catch (IOException e) {
                ...
            }
        }
        ...   
}

private @NonNull ApkAssets loadApkAssets(@NonNull final ApkKey key) throws IOException {
        ...
        if (key.overlay) {
            ...
        } else {
            // 通过ApkAssets从APK文件所在的路径去加载
            apkAssets = ApkAssets.loadFromPath(key.path,
                    key.sharedLib ? ApkAssets.PROPERTY_DYNAMIC : 0);
        }
        ...
    }

/*frameworks/base/core/java/android/content/res/ApkAssets.java*/
public static @NonNull ApkAssets loadFromPath(@NonNull String path, @PropertyFlags int flags)
            throws IOException {
        return new ApkAssets(FORMAT_APK, path, flags, null /* assets */);
}

private ApkAssets(@FormatType int format, @NonNull String path, @PropertyFlags int flags,
            @Nullable AssetsProvider assets) throws IOException {
        ...
        // 通过JNI调用Native层的系统system/lib/libandroidfw.so库中的相关C函数实现对APK文件压缩包的解析与加载
        mNativePtr = nativeLoad(format, path, flags, assets);
        ...
}
```

从以上代码可以看出：**系统对于应用`APK`文件资源的加载过程其实就是创建应用进程中的`Resources`资源对象的过程，其中真正实现`APK`资源文件的`I/O`解析作，最终是借助于`AssetManager`中通过JNI调用系统`Native`层的相关`C`函数实现。**整个过程从systrace上看如下图所示：  

![](https://upload-images.jianshu.io/upload_images/26874665-10a05df711ae2020.png?imageMogr2/auto-orient/strip|imageView2/2/w/1199/format/webp)

getResources.png

## 6.2 Activity的创建与初始化

我们回到6.1小结中，看看`AMS`在收到应用进程的`attachApplication`注册请求后，先通过oneway类型的binder调用应用及进程的`IApplicationThread`#`bindApplication`接口，触发应用进程在主线程执行`handleBindeApplication`初始化操作，然后继续执行启动应用`Activity`的操作，下面我们来看看系统是如何启动创建应用`Activity`的，简化代码流程如下：

```java
/*frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java*/
@GuardedBy("this")
private boolean attachApplicationLocked(...) {
     ...
     if (app.isolatedEntryPoint != null) {
           ...
     } else if (instr2 != null) {
           // 1.通过oneway异步类型的binder调用应用进程ActivityThread#IApplicationThread#bindApplication接口
           thread.bindApplication(...);
     } else {
           thread.bindApplication(...);
     }
     ...
     // See if the top visible activity is waiting to run in this process...
     if (normalMode) {
          try {
            // 2.继续执行启动应用Activity的流程
            didSomething = mAtmInternal.attachApplication(app.getWindowProcessController());
          } catch (Exception e) {
                Slog.wtf(TAG, "Exception thrown launching activities in " + app, e);
                badApp = true;
          }
      }
}

/*frameworks/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java*/
public boolean attachApplication(WindowProcessController wpc) throws RemoteException {
       synchronized (mGlobalLockWithoutBoost) {
            if (Trace.isTagEnabled(TRACE_TAG_WINDOW_MANAGER)) {
                // 原生标识attachApplication过程的systrace tag
                Trace.traceBegin(TRACE_TAG_WINDOW_MANAGER, "attachApplication:" + wpc.mName);
            }
            try {
                return mRootWindowContainer.attachApplication(wpc);
            } finally {
                Trace.traceEnd(TRACE_TAG_WINDOW_MANAGER);
            }
       }
}

/*frameworks/base/services/core/java/com/android/server/wm/RootWindowContainer.java*/
boolean attachApplication(WindowProcessController app) throws RemoteException {
       ...
       final PooledFunction c = PooledLambda.obtainFunction(
                // startActivityForAttachedApplicationIfNeeded执行启动应用Activity流程
                RootWindowContainer::startActivityForAttachedApplicationIfNeeded, this,
                PooledLambda.__(ActivityRecord.class), app,
                rootTask.topRunningActivity());
       ...
}
 
private boolean startActivityForAttachedApplicationIfNeeded(ActivityRecord r,
            WindowProcessController app, ActivityRecord top) {
        ...
        try {
            // ActivityStackSupervisor的realStartActivityLocked真正实现启动应用Activity流程
            if (mStackSupervisor.realStartActivityLocked(r, app,
                    top == r && r.isFocusable() /*andResume*/, true /*checkConfig*/)) {
                ...
            }
        } catch (RemoteException e) {
            ..
        }
}

/*frameworks/base/services/core/java/com/android/server/wm/ActivityStackSupervisor.java*/
boolean realStartActivityLocked(ActivityRecord r, WindowProcessController proc,
            boolean andResume, boolean checkConfig) throws RemoteException {
         ...
        // 1.先通过LaunchActivityItem封装Binder通知应用进程执行Launch Activity动作       
         clientTransaction.addCallback(LaunchActivityItem.obtain(...);
         // Set desired final state.
         final ActivityLifecycleItem lifecycleItem;
         if (andResume) {
                // 2.再通过ResumeActivityItem封装Binder通知应用进程执行Launch Resume动作        
                lifecycleItem = ResumeActivityItem.obtain(dc.isNextTransitionForward());
         }
         ...
         clientTransaction.setLifecycleStateRequest(lifecycleItem);
         // 执行以上封装的Binder调用
         mService.getLifecycleManager().scheduleTransaction(clientTransaction);
         ...
}
```

从以上代码分析可以看到，框架`system_server`进程最终是通过`ActivityStackSupervisor`#`realStartActivityLocked`函数中，通过`LaunchActivityItem`和`ResumeActivityItem`两个类的封装，依次实现binder调用通知应用进程这边执行`Activity`的Launch和Resume动作的，我们继续往下看相关代码流程：

### 6.2.1 Activity Create

```java
/*frameworks/base/core/java/android/app/servertransaction/LaunchActivityItem.java*/
@Override
public void execute(ClientTransactionHandler client, IBinder token,
            PendingTransactionActions pendingActions) {
     // 原生标识Activity Launch的systrace tag
     Trace.traceBegin(TRACE_TAG_ACTIVITY_MANAGER, "activityStart");
     ActivityClientRecord r = new ActivityClientRecord(token, mIntent, mIdent, mInfo,
                mOverrideConfig, mCompatInfo, mReferrer, mVoiceInteractor, mState, mPersistentState,
                mPendingResults, mPendingNewIntents, mIsForward,
                mProfilerInfo, client, mAssistToken, mFixedRotationAdjustments);
     // 调用到ActivityThread的handleLaunchActivity函数在主线程执行应用Activity的Launch创建动作
     client.handleLaunchActivity(r, pendingActions, null /* customIntent */);
     Trace.traceEnd(TRACE_TAG_ACTIVITY_MANAGER);
}

/*frameworks/base/core/java/android/app/ActivityThread.java*/
@Override
public Activity handleLaunchActivity(ActivityClientRecord r,
            PendingTransactionActions pendingActions, Intent customIntent) {
     ...
     final Activity a = performLaunchActivity(r, customIntent);
     ...
}

/**  Core implementation of activity launch. */
private Activity performLaunchActivity(ActivityClientRecord r, Intent customIntent) {
        ...
        // 1.创建Activity的Context
        ContextImpl appContext = createBaseContextForActivity(r);
        try {
            //2.反射创建Activity对象
            activity = mInstrumentation.newActivity(
                    cl, component.getClassName(), r.intent);
            ...
        } catch (Exception e) {
            ...
        }
        try {
            ...
            if (activity != null) {
                ...
                // 3.执行Activity的attach动作
                activity.attach(...);
                ...
                // 4.执行应用Activity的onCreate生命周期函数,并在setContentView调用中创建DecorView对象
                mInstrumentation.callActivityOnCreate(activity, r.state);
                ...
            }
            ...
        } catch (SuperNotCalledException e) {
            ...
        }
}

/*frameworks/base/core/java/android/app/Activity.java*/
 @UnsupportedAppUsage
 final void attach(...) {
        ...
        // 1.创建表示应用窗口的PhoneWindow对象
        mWindow = new PhoneWindow(this, window, activityConfigCallback);
        ...
        // 2.为PhoneWindow配置WindowManager
        mWindow.setWindowManager(
                (WindowManager)context.getSystemService(Context.WINDOW_SERVICE),
                mToken, mComponent.flattenToString(),
                (info.flags & ActivityInfo.FLAG_HARDWARE_ACCELERATED) != 0);
        ...
}
```

从上面代码可以看出，应用进程这边在收到系统binder调用后，**在主线程中创建`Activiy`的流程主要步骤如下**：

1. 创建`Activity`的`Context`；
2. 通过反射创建`Activity`对象；
3. 执行`Activity`的`attach`动作，**其中会创建应用窗口的`PhoneWindow`对象并设置`WindowManage`**；
4. **执行应用`Activity`的`onCreate`生命周期函数，并在`setContentView`中创建窗口的`DecorView`对象**；

从systrace上看整个过程如下图所示：

  

![](https://upload-images.jianshu.io/upload_images/26874665-9bd20c4b27a4c010.png?imageMogr2/auto-orient/strip|imageView2/2/w/1044/format/webp)

ActivityStart.png

### 6.2.2 Activity Resume

```java
/*frameworks/base/core/java/android/app/servertransaction/ResumeActivityItem.java*/
@Override
public void execute(ClientTransactionHandler client, IBinder token,
            PendingTransactionActions pendingActions) {
   // 原生标识Activity Resume的systrace tag
   Trace.traceBegin(TRACE_TAG_ACTIVITY_MANAGER, "activityResume");
   client.handleResumeActivity(token, true /* finalStateRequest */, mIsForward,
                "RESUME_ACTIVITY");
   Trace.traceEnd(TRACE_TAG_ACTIVITY_MANAGER);
}

/*frameworks/base/core/java/android/app/ActivityThread.java*/
 @Override
public void handleResumeActivity(...){
    ...
    // 1.执行performResumeActivity流程,执行应用Activity的onResume生命周期函数
    final ActivityClientRecord r = performResumeActivity(token, finalStateRequest, reason);
    ...
    if (r.window == null && !a.mFinished && willBeVisible) {
            ...
            if (a.mVisibleFromClient) {
                if (!a.mWindowAdded) {
                    ...
                    // 2.执行WindowManager#addView动作开启视图绘制逻辑
                    wm.addView(decor, l);
                } else {
                  ...
                }
            }
     }
    ...
}

public ActivityClientRecord performResumeActivity(...) {
    ...
    // 执行应用Activity的onResume生命周期函数
    r.activity.performResume(r.startsNotResumed, reason);
    ...
}

/*frameworks/base/core/java/android/view/WindowManagerGlobal.java*/
public void addView(...) {
     // 创建ViewRootImpl对象
     root = new ViewRootImpl(view.getContext(), display);
     ...
     try {
         // 执行ViewRootImpl的setView函数
         root.setView(view, wparams, panelParentView, userId);
     } catch (RuntimeException e) {
         ...
     } 
}
```

从上面代码可以看出，应用进程这边在接收到系统Binder调用请求后，**在主线程中`Activiy` `Resume`的流程主要步骤如下**：

1. **执行应用`Activity`的`onResume`生命周期函数**;
2. 执行`WindowManager`的`addView`动作开启视图绘制逻辑;
3. 创建`Activity`的`ViewRootImpl`对象;
4. **执行`ViewRootImpl`的`setView`函数开启UI界面绘制动作**；

从systrace上看整个过程如下图所示：

  

![](https://upload-images.jianshu.io/upload_images/26874665-c2aa7d40fb9c7e5d.png?imageMogr2/auto-orient/strip|imageView2/2/w/1127/format/webp)

activityResume.png

# 7. 应用UI布局与绘制

接上一节的分析，应用主线程中在执行`Activity`的Resume流程的最后，会创建`ViewRootImpl`对象并调用其setView函数，从此并开启了应用界面UI布局与绘制的流程。在开始讲解这个过程之前，我们先来整理一下前面代码中讲到的这些概念，如`Activity`、`PhoneWindow`、`DecorView`、`ViewRootImpl`、`WindowManager`它们之间的关系与职责，因为这些核心类基本构成了Android系统的GUI显示系统在应用进程侧的核心架构，其整体架构如下图所示：  

![](https://upload-images.jianshu.io/upload_images/26874665-ab588cdefb9a311f.png?imageMogr2/auto-orient/strip|imageView2/2/w/804/format/webp)

GUI_APP.png

- `Window`是一个抽象类，**通过控制`DecorView`提供了一些标准的UI方案，比如`背景、标题、虚拟按键等`**，而`PhoneWindow`是`Window`的唯一实现类，在`Activity`创建后的attach流程中创建，应用启动显示的内容装载到其内部的`mDecor`（`DecorView`）；
- `DecorView`是整个界面布局View控件树的根节点，通过它可以遍历访问到整个View控件树上的任意节点；
- `WindowManager`是一个接口，继承自`ViewManager`接口，提供了`View`的基本操作方法；`WindowManagerImp`实现了`WindowManager`接口，内部通过`组合`方式持有`WindowManagerGlobal`，用来操作`View`；**`WindowManagerGlobal`是一个全局单例，内部可以通过`ViewRootImpl`将`View`添加至`窗口`中**；
- **`ViewRootImpl`是所有`View`的`Parent`，用来总体管理`View`的绘制以及与系统`WMS`窗口管理服务的IPC交互从而实现`窗口`的开辟**；`ViewRootImpl`是应用进程运转的发动机，可以看到`ViewRootImpl`内部包含`mView`（就是`DecorView`）、`mSurface`、`Choregrapher`，`mView`代表整个控件树，`mSurfacce`代表画布，应用的UI渲染会直接放到`mSurface`中，`Choregorapher`使得应用请求`vsync`信号，接收信号后开始渲染流程；  
    我们从`ViewRootImpl`的setView流程继续结合代码往下看：

```java
/*frameworks/base/core/java/android/view/ViewRootImpl.java*/
public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView,
            int userId) {
      synchronized (this) {
         if (mView == null) {
             mView = view;
         }
         ...
         // 开启绘制硬件加速，初始化RenderThread渲染线程运行环境
         enableHardwareAcceleration(attrs);
         ...
         // 1.触发绘制动作
         requestLayout();
         ...
         inputChannel = new InputChannel();
         ...
         // 2.Binder调用访问系统窗口管理服务WMS接口，实现addWindow添加注册应用窗口的操作,并传入inputChannel用于接收触控事件
         res = mWindowSession.addToDisplayAsUser(mWindow, mSeq, mWindowAttributes,
                            getHostVisibility(), mDisplay.getDisplayId(), userId, mTmpFrame,
                            mAttachInfo.mContentInsets, mAttachInfo.mStableInsets,
                            mAttachInfo.mDisplayCutout, inputChannel,
                            mTempInsets, mTempControls);
         ...
         // 3.创建WindowInputEventReceiver对象，实现应用窗口接收触控事件
         mInputEventReceiver = new WindowInputEventReceiver(inputChannel,
                            Looper.myLooper());
         ...
         // 4.设置DecorView的mParent为ViewRootImpl
         view.assignParent(this);
         ...
      }
}
```

从以上代码可以看出`ViewRootImpl`的setView内部关键流程如下：

1. **requestLayout()通过一系列调用触发界面绘制（measure、layout、draw）动作**，下文会详细展开分析；
2. **通过Binder调用访问系统窗口管理服务`WMS`的`addWindow`接口**，**实现添加、注册应用窗口的操作**，并传入本地创建inputChannel对象用于后续接收系统的触控事件，这一步执行完我们的`View`就可以显示到屏幕上了。关于`WMS`的内部实现流程也非常复杂，由于篇幅有限本文就不详细展开分析了。
3. 创建WindowInputEventReceiver对象，封装实现应用窗口接收系统触控事件的逻辑；
4. 执行view.assignParent(this)，设置`DecorView`的mParent为`ViewRootImpl`。所以，**虽然`ViewRootImpl`不是一个`View`,但它是所有`View`的顶层`Parent`。**

我们顺着`ViewRootImpl`的`requestLayout`动作继续往下看界面绘制的流程代码：

```java
/*frameworks/base/core/java/android/view/ViewRootImpl.java*/
public void requestLayout() {
    if (!mHandlingLayoutInLayoutRequest) {
         // 检查当前UI绘制操作是否发生在主线程，如果发生在子线程则会抛出异常
         checkThread();
         mLayoutRequested = true;
         // 触发绘制操作
         scheduleTraversals();
    }
}

@UnsupportedAppUsage
void scheduleTraversals() {
    if (!mTraversalScheduled) {
         ...
         // 注意此处会往主线程的MessageQueue消息队列中添加同步栏删，因为系统绘制消息属于异步消息，需要更高优先级的处理
         mTraversalBarrier = mHandler.getLooper().getQueue().postSyncBarrier();
         // 通过Choreographer往主线程消息队列添加CALLBACK_TRAVERSAL绘制类型的待执行消息，用于触发后续UI线程真正实现绘制动作
         mChoreographer.postCallback(
                    Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null);
         ...
     }
}
```

`Choreographer` 的引入，主要是配合系统`Vsync`垂直同步机制（Android“黄油计划”中引入的机制之一，协调APP生成UI数据和`SurfaceFlinger`合成图像，避免Tearing画面撕裂的现象），给上层 App 的渲染提供一个稳定的 `Message` 处理的时机，也就是 `Vsync` 到来的时候 ，系统通过对 `Vsync` 信号周期的调整，来控制每一帧绘制操作的时机。**`Choreographer` 扮演 Android 渲染链路中承上启下的角色**：

1. **承上**：负责接收和处理 App 的各种更新消息和回调，等到 `Vsync` 到来的时候统一处理。比如集中处理 Input(主要是 Input 事件的处理) 、Animation(动画相关)、Traversal(包括 `measure、layout、draw` 等操作) ，判断卡顿掉帧情况，记录 CallBack 耗时等；
2. **启下**：负责请求和接收 Vsync 信号。接收 Vsync 事件回调(通过 `FrameDisplayEventReceiver`.`onVsync` )，请求 `Vsync`(`FrameDisplayEventReceiver`.`scheduleVsync`) 。

`Choreographer`在收到`CALLBACK_TRAVERSAL`类型的绘制任务后，其内部的工作流程如下图所示：  

![](https://upload-images.jianshu.io/upload_images/26874665-a44a2100a0f46092.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

Choreographer工作原理.jpg

  
从以上流程图可以看出：`ViewRootImpl`调用`Choreographer`的`postCallback`接口放入待执行的绘制消息后，`Choreographer`会先向系统申请`APP` 类型的`vsync`信号，然后等待系统`vsync`信号到来后，去回调到`ViewRootImpl`的`doTraversal`函数中执行真正的绘制动作（measure、layout、draw）。这个绘制过程从systrace上看如下图所示：  

![](https://upload-images.jianshu.io/upload_images/26874665-2c6e1bf64f739540.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

UI绘制任务.png

  
我们接着`ViewRootImpl`的`doTraversal`函数的简化代码流程往下看：

```java
/*frameworks/base/core/java/android/view/ViewRootImpl.java*/
void doTraversal() {
     if (mTraversalScheduled) {
         mTraversalScheduled = false;
         // 调用removeSyncBarrier及时移除主线程MessageQueue中的Barrier同步栏删，以避免主线程发生“假死”
         mHandler.getLooper().getQueue().removeSyncBarrier(mTraversalBarrier);
         ...
         // 执行具体的绘制任务
         performTraversals();
         ...
    }
}

private void performTraversals() {
     ...
     // 1.从DecorView根节点出发，遍历整个View控件树，完成整个View控件树的measure测量操作
     windowSizeMayChange |= measureHierarchy(...);
     ...
     if (mFirst...) {
    // 2.第一次执行traversals绘制任务时，Binder调用访问系统窗口管理服务WMS的relayoutWindow接口，实现WMS计算应用窗口尺寸并向系统surfaceflinger正式申请Surface“画布”操作
         relayoutResult = relayoutWindow(params, viewVisibility, insetsPending);
     }
     ...
     // 3.从DecorView根节点出发，遍历整个View控件树，完成整个View控件树的layout测量操作
     performLayout(lp, mWidth, mHeight);
     ...
     // 4.从DecorView根节点出发，遍历整个View控件树，完成整个View控件树的draw测量操作
     performDraw();
     ...
}

private int relayoutWindow(WindowManager.LayoutParams params, int viewVisibility,
            boolean insetsPending) throws RemoteException {
        ...
        // 通过Binder IPC访问系统WMS服务的relayout接口，申请Surface“画布”操作
        int relayoutResult = mWindowSession.relayout(mWindow, mSeq, params,
                (int) (mView.getMeasuredWidth() * appScale + 0.5f),
                (int) (mView.getMeasuredHeight() * appScale + 0.5f), viewVisibility,
                insetsPending ? WindowManagerGlobal.RELAYOUT_INSETS_PENDING : 0, frameNumber,
                mTmpFrame, mTmpRect, mTmpRect, mTmpRect, mPendingBackDropFrame,
                mPendingDisplayCutout, mPendingMergedConfiguration, mSurfaceControl, mTempInsets,
                mTempControls, mSurfaceSize, mBlastSurfaceControl);
        if (mSurfaceControl.isValid()) {
            if (!useBLAST()) {
                // 本地Surface对象获取指向远端分配的Surface的引用
                mSurface.copyFrom(mSurfaceControl);
            } else {
               ...
            }
        }
        ...
}

private void performMeasure(int childWidthMeasureSpec, int childHeightMeasureSpec) {
        ...
        // 原生标识View树的measure测量过程的trace tag
        Trace.traceBegin(Trace.TRACE_TAG_VIEW, "measure");
        try {
            // 从mView指向的View控件树的根节点DecorView出发，遍历访问整个View树，并完成整个布局View树的测量工作
            mView.measure(childWidthMeasureSpec, childHeightMeasureSpec);
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_VIEW);
        }
}

private void performDraw() {
     ...
     boolean canUseAsync = draw(fullRedrawNeeded);
     ...
}

private boolean draw(boolean fullRedrawNeeded) {
    ...
    if (mAttachInfo.mThreadedRenderer != null && mAttachInfo.mThreadedRenderer.isEnabled()) {
        ...
        // 如果开启并支持硬件绘制加速，则走硬件绘制的流程（从Android 4.+开始，默认情况下都是支持跟开启了硬件加速的）
        mAttachInfo.mThreadedRenderer.draw(mView, mAttachInfo, this);
    } else {
        // 否则走drawSoftware软件绘制的流程
        if (!drawSoftware(surface, mAttachInfo, xOffset, yOffset,
                        scalingRequired, dirty, surfaceInsets)) {
                    return false;
         }
    }
}
```

从上面的代码流程可以看出，**`ViewRootImpl`中负责的整个应用界面绘制的主要流程如下**：

1. 从界面View控件树的根节点`DecorView`出发，递归遍历整个View控件树，完成对整个`View`控件树的`measure`测量操作，由于篇幅所限，本文就不展开分析这块的详细流程；
2. 界面第一次执行绘制任务时，会通过`Binder` `IPC`访问系统窗口管理服务WMS的relayout接口，实现窗口尺寸的计算并向系统申请用于本地绘制渲染的Surface“画布”的操作（**具体由`SurfaceFlinger`负责创建应用界面对应的`BufferQueueLayer`对象，并通过内存共享的方式通过`Binder`将地址引用透过WMS回传给应用进程这边**），由于篇幅所限，本文就不展开分析这块的详细流程；
3. 从界面View控件树的根节点`DecorView`出发，递归遍历整个View控件树，完成对整个`View`控件树的`layout`测量操作；
4. 从界面View控件树的根节点`DecorView`出发，递归遍历整个`View`控件树，完成对整个`View`控件树的`draw`测量操作，**如果开启并支持硬件绘制加速（从Android 4.X开始谷歌已经默认开启硬件加速），则走`GPU`硬件绘制的流程，否则走`CPU`软件绘制的流程**；

以上绘制过程从systrace上看如下图所示：

  

![](https://upload-images.jianshu.io/upload_images/26874665-a18d405f6837cf38.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

performTraversal绘制.png

  

![](https://upload-images.jianshu.io/upload_images/26874665-1c521ee5320be3a7.png?imageMogr2/auto-orient/strip|imageView2/2/w/1199/format/webp)

relayoutWindow.png

  

借用一张图来总结应用UI绘制的流程，如下所示：

  

![](https://upload-images.jianshu.io/upload_images/26874665-79225531199ba7ce.png?imageMogr2/auto-orient/strip|imageView2/2/w/1111/format/webp)

UI绘制流程.png

# 8. RenderThread渲染

截止到目前，在`ViewRootImpl`中完成了对界面的measure、layout和draw等绘制流程后，用户依然还是看不到屏幕上显示的应用界面内容，因为整个`Android`系统的显示流程除了前面讲到的UI线程的绘制外，界面还需要经过`RenderThread`线程的渲染处理，渲染完成后，还需要通过`Binder`调用“上帧”交给`surfaceflinger`进程中进行合成后送显才能最终显示到屏幕上。本小节中，我们将接上一节中`ViewRootImpl`中最后draw的流程继续往下分析开启硬件加速情况下，`RenderThread`渲染线程的工作流程。由于目前Android 4.X之后系统默认界面是开启硬件加速的，所以本文我们重点分析硬件加速条件下的界面渲染流程，我们先分析一下简化的代码流程：

```java
/*frameworks/base/core/java/android/view/ViewRootImpl.java*/
private boolean draw(boolean fullRedrawNeeded) {
    ...
    if (mAttachInfo.mThreadedRenderer != null && mAttachInfo.mThreadedRenderer.isEnabled()) {
        ...
        // 硬件加速条件下的界面渲染流程
        mAttachInfo.mThreadedRenderer.draw(mView, mAttachInfo, this);
    } else {
        ...
    }
}

/*frameworks/base/core/java/android/view/ThreadedRenderer.java*/
void draw(View view, AttachInfo attachInfo, DrawCallbacks callbacks) {
    ...
    // 1.从DecorView根节点出发，递归遍历View控件树，记录每个View节点的绘制操作命令，完成绘制操作命令树的构建
    updateRootDisplayList(view, callbacks);
    ...
    // 2.JNI调用同步Java层构建的绘制命令树到Native层的RenderThread渲染线程，并唤醒渲染线程利用OpenGL执行渲染任务；
    int syncResult = syncAndDrawFrame(choreographer.mFrameInfo);
    ...
}
```

从上面的代码可以看出，**硬件加速绘制主要包括两个阶段**：

1. 从`DecorView`根节点出发，递归遍历`View`控件树，记录每个`View`节点的`drawOp`绘制操作命令，完成绘制操作命令树的构建；
2. `JNI`调用同步`Java`层构建的绘制命令树到`Native`层的`RenderThread`渲染线程，并唤醒渲染线程利用`OpenGL`执行渲染任务；

## 8.1 构建绘制命令树

我们先来看看第一阶段构建绘制命令树的代码简化流程：

```java
/*frameworks/base/core/java/android/view/ThreadedRenderer.java*/
private void updateRootDisplayList(View view, DrawCallbacks callbacks) {
        // 原生标记构建View绘制操作命令树过程的systrace tag
        Trace.traceBegin(Trace.TRACE_TAG_VIEW, "Record View#draw()");
        // 递归子View的updateDisplayListIfDirty实现构建DisplayListOp
        updateViewTreeDisplayList(view);
        ...
        if (mRootNodeNeedsUpdate || !mRootNode.hasDisplayList()) {
            // 获取根View的SkiaRecordingCanvas
            RecordingCanvas canvas = mRootNode.beginRecording(mSurfaceWidth, mSurfaceHeight);
            try {
                ...
                // 利用canvas缓存DisplayListOp绘制命令
                canvas.drawRenderNode(view.updateDisplayListIfDirty());
                ...
            } finally {
                // 将所有DisplayListOp绘制命令填充到RootRenderNode中
                mRootNode.endRecording();
            }
        }
        Trace.traceEnd(Trace.TRACE_TAG_VIEW);
}

private void updateViewTreeDisplayList(View view) {
        ...
        // 从DecorView根节点出发，开始递归调用每个View树节点的updateDisplayListIfDirty函数
        view.updateDisplayListIfDirty();
        ...
}

/*frameworks/base/core/java/android/view/View.java*/
public RenderNode updateDisplayListIfDirty() {
     ...
     // 1.利用`View`对象构造时创建的`RenderNode`获取一个`SkiaRecordingCanvas`“画布”；
     final RecordingCanvas canvas = renderNode.beginRecording(width, height);
     try {
         ...
         if ((mPrivateFlags & PFLAG_SKIP_DRAW) == PFLAG_SKIP_DRAW) {
              // 如果仅仅是ViewGroup，并且自身不用绘制，直接递归子View
              dispatchDraw(canvas);
              ...
         } else {
              // 2.利用SkiaRecordingCanvas，在每个子View控件的onDraw绘制函数中调用drawLine、drawRect等绘制操作时，创建对应的DisplayListOp绘制命令，并缓存记录到其内部的SkiaDisplayList持有的DisplayListData中；
              draw(canvas);
         }
     } finally {
         // 3.将包含有`DisplayListOp`绘制命令缓存的`SkiaDisplayList`对象设置填充到`RenderNode`中；
         renderNode.endRecording();
         ...
     }
     ...
}

public void draw(Canvas canvas) {
    ...
    // draw the content(View自己实现的onDraw绘制，由应用开发者自己实现)
    onDraw(canvas);
    ...
    // draw the children
    dispatchDraw(canvas);
    ...
}

/*frameworks/base/graphics/java/android/graphics/RenderNode.java*/
public void endRecording() {
        ...
        // 从SkiaRecordingCanvas中获取SkiaDisplayList对象
        long displayList = canvas.finishRecording();
        // 将SkiaDisplayList对象填充到RenderNode中
        nSetDisplayList(mNativeRenderNode, displayList);
        canvas.recycle();
}
```

从以上代码可以看出，**构建绘制命令树的过程是从`View`控件树的根节点`DecorView`触发，递归调用每个子`View`节点的`updateDisplayListIfDirty`函数，最终完成绘制树的创建，简述流程如下**：

1. 利用`View`对象构造时创建的`RenderNode`获取一个`SkiaRecordingCanvas`“画布”；
2. 利用`SkiaRecordingCanvas`，**在每个子`View`控件的`onDraw`绘制函数中调用`drawLine`、`drawRect`等绘制操作时，创建对应的`DisplayListOp`绘制命令，并缓存记录到其内部的`SkiaDisplayList`持有的`DisplayListData`中**；
3. 将包含有`DisplayListOp`绘制命令缓存的`SkiaDisplayList`对象设置填充到`RenderNode`中；
4. 最后将根`View`的缓存`DisplayListOp`设置到`RootRenderNode`中，完成构建。

以上整个构建绘制命令树的过程可以用如下流程图表示：

  

![](https://upload-images.jianshu.io/upload_images/26874665-1195656a32dbec9e.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

硬件加速绘制之绘制命令树构建.png

  

硬件加速下的整个界面的View树的结构如下图所示：

  

![](https://upload-images.jianshu.io/upload_images/26874665-a951aa2dfda7c791.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

硬件绘制下的View树结构.png

  
最后从systrace上看这个过程如下图所示：  

![](https://upload-images.jianshu.io/upload_images/26874665-fbe918b3968154ea.png?imageMogr2/auto-orient/strip|imageView2/2/w/1144/format/webp)

构建View绘制命令树.png

## 8.2 执行渲染绘制任务

经过上一小节中的分析，应用在`UI`线程中从根节点`DecorView`出发，递归遍历每个子`View`节点，搜集其`drawXXX`绘制动作并转换成`DisplayListOp`命令，将其记录到`DisplayListData`并填充到`RenderNode`中，最终完成整个`View`绘制命令树的构建。从此UI线程的绘制任务就完成了。下一步`UI`线程将唤醒`RenderThread`渲染线程，触发其利用`OpenGL`执行界面的渲染任务，本小节中我们将重点分析这个流程。我们还是先看看这块代码的简化流程：

```cpp
/*frameworks/base/graphics/java/android/graphics/HardwareRenderer.java*/
public int syncAndDrawFrame(@NonNull FrameInfo frameInfo) {
    // JNI调用native层的相关函数
    return nSyncAndDrawFrame(mNativeProxy, frameInfo.frameInfo, frameInfo.frameInfo.length);
}

/*frameworks/base/libs/hwui/jni/android_graphics_HardwareRenderer.cpp*/
static int android_view_ThreadedRenderer_syncAndDrawFrame(JNIEnv* env, jobject clazz,
        jlong proxyPtr, jlongArray frameInfo, jint frameInfoSize) {
    ...
    RenderProxy* proxy = reinterpret_cast<RenderProxy*>(proxyPtr);
    env->GetLongArrayRegion(frameInfo, 0, frameInfoSize, proxy->frameInfo());
    return proxy->syncAndDrawFrame();
}

/*frameworks/base/libs/hwui/renderthread/RenderProxy.cpp*/
int RenderProxy::syncAndDrawFrame() {
    // 唤醒RenderThread渲染线程，执行DrawFrame绘制任务
    return mDrawFrameTask.drawFrame();
}

/*frameworks/base/libs/hwui/renderthread/DrawFrameTask.cpp*/
int DrawFrameTask::drawFrame() {
    ...
    postAndWait();
    ...
}

void DrawFrameTask::postAndWait() {
    AutoMutex _lock(mLock);
    // 向RenderThread渲染线程的MessageQueue消息队列放入一个待执行任务，以将其唤醒执行run函数
    mRenderThread->queue().post([this]() { run(); });
    // UI线程暂时进入wait等待状态
    mSignal.wait(mLock);
}

void DrawFrameTask::run() {
    // 原生标识一帧渲染绘制任务的systrace tag
    ATRACE_NAME("DrawFrame");
    ...
    {
        TreeInfo info(TreeInfo::MODE_FULL, *mContext);
        //1.将UI线程构建的DisplayListOp绘制命令树同步到RenderThread渲染线程
        canUnblockUiThread = syncFrameState(info);
        ...
    }
    ...
    // 同步完成后则可以唤醒UI线程
    if (canUnblockUiThread) {
        unblockUiThread();
    }
    ...
    if (CC_LIKELY(canDrawThisFrame)) {
        // 2.执行draw渲染绘制动作
        context->draw();
    } else {
        ...
    }
    ...
}

bool DrawFrameTask::syncFrameState(TreeInfo& info) {
    ATRACE_CALL();
    ...
    // 调用CanvasContext的prepareTree函数实现绘制命令树同步的流程
    mContext->prepareTree(info, mFrameInfo, mSyncQueued, mTargetNode);
    ...
}

/*frameworks/base/libs/hwui/renderthread/CanvasContext.cpp*/
void CanvasContext::prepareTree(TreeInfo& info, int64_t* uiFrameInfo, int64_t syncQueued,
                                RenderNode* target) {
     ...
     for (const sp<RenderNode>& node : mRenderNodes) {
        ...
        // 递归调用各个子View对应的RenderNode执行prepareTree动作
        node->prepareTree(info);
        ...
    }
    ...
}

/*frameworks/base/libs/hwui/RenderNode.cpp*/
void RenderNode::prepareTree(TreeInfo& info) {
    ATRACE_CALL();
    ...
    prepareTreeImpl(observer, info, false);
    ...
}

void RenderNode::prepareTreeImpl(TreeObserver& observer, TreeInfo& info, bool functorsNeedLayer) {
    ...
    if (info.mode == TreeInfo::MODE_FULL) {
        // 同步绘制命令树
        pushStagingDisplayListChanges(observer, info);
    }
    if (mDisplayList) {
        // 遍历调用各个子View对应的RenderNode的prepareTreeImpl
        bool isDirty = mDisplayList->prepareListAndChildren(
                observer, info, childFunctorsNeedLayer,
                [](RenderNode* child, TreeObserver& observer, TreeInfo& info,
                   bool functorsNeedLayer) {
                    child->prepareTreeImpl(observer, info, functorsNeedLayer);
                });
        ...
    }
    ...
}

void RenderNode::pushStagingDisplayListChanges(TreeObserver& observer, TreeInfo& info) {
    ...
    syncDisplayList(observer, &info);
    ...
}

void RenderNode::syncDisplayList(TreeObserver& observer, TreeInfo* info) {
    ...
    // 完成赋值同步DisplayList对象
    mDisplayList = mStagingDisplayList;
    mStagingDisplayList = nullptr;
    ...
}

void CanvasContext::draw() {
    ...
    // 1.调用OpenGL库使用GPU，按照构建好的绘制命令完成界面的渲染
    bool drew = mRenderPipeline->draw(frame, windowDirty, dirty, mLightGeometry, &mLayerUpdateQueue,
                                      mContentDrawBounds, mOpaque, mLightInfo, mRenderNodes,
                                      &(profiler()));
    ...
    // 2.将前面已经绘制渲染好的图形缓冲区Binder上帧给SurfaceFlinger合成和显示
    bool didSwap =
            mRenderPipeline->swapBuffers(frame, drew, windowDirty, mCurrentFrameInfo, &requireSwap);
    ...
}
```

从以上代码可以看出：`UI`线程利用`RenderProxy`向`RenderThread`线程发送一个`DrawFrameTask`任务请求，**`RenderThread`被唤醒，开始渲染，大致流程如下**：

1. `syncFrameState`中遍历`View`树上每一个`RenderNode`，执行`prepareTreeImpl`函数，实现同步绘制命令树的操作；
2. 调用`OpenGL`库`API`使用`GPU`，按照构建好的绘制命令完成界面的渲染（具体过程，由于本文篇幅所限，暂不展开分析）；
3. 将前面已经绘制渲染好的图形缓冲区`Binder`上帧给`SurfaceFlinger`合成和显示；

整个过程可以用如下流程图表示：

  

![](https://upload-images.jianshu.io/upload_images/26874665-7218830c7bb346ae.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

RenderThread线程渲染流程.png

  

从systrace上这个过程如下图所示：

  

![](https://upload-images.jianshu.io/upload_images/26874665-d8d47f1418e9f21a.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

RenderThread实现界面渲染.png

# 9. SurfaceFlinger合成显示

`SurfaceFlinger`合成显示部分完全属于`Android`系统`GUI`中图形显示的内容，逻辑结构也比较复杂，但不属于本文介绍内容的重点。所以本小节中只是总体上介绍一下其工作原理与思想，不再详细分析源码，感兴趣的读者可以关注笔者后续的文章再来详细分析讲解。简单的说`SurfaceFlinger`作为系统中独立运行的一个`Native`进程，**借用`Android`官网的描述，其职责就是负责接受来自多个来源的数据缓冲区，对它们进行合成，然后发送到显示设备。**如下图所示：  

![](https://upload-images.jianshu.io/upload_images/26874665-cb33efbd47f23d22.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/537/format/webp)

SurfaceFlinger工作原理.jpg

  
从上图可以看出，其实`SurfaceFlinger`在`Android`系统的整个图形显示系统中是起到一个**承上启下的作用**：

- **对上**：通过Surface与不同的应用进程建立联系，接收它们写入Surface中的绘制缓冲数据，对它们进行统一合成。
- **对下**：通过屏幕的后缓存区与屏幕建立联系，发送合成好的数据到屏幕显示设备。

图形的传递是通过`Buffer`作为载体，`Surface`是对`Buffer`的进一步封装，也就是说`Surface`内部具有多个`Buffer`供上层使用，如何管理这些`Buffer`呢？答案就是`BufferQueue` ，下面我们来看看`BufferQueue`的工作原理：

## 9.1 BufferQueue机制

借用一张经典的图来描述`BufferQueue`的工作原理：  

![](https://upload-images.jianshu.io/upload_images/26874665-05c18df7fb448c79.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/481/format/webp)

BufferQueue状态转换图.jpg

  
`BufferQueue`是一个**典型的生产者-消费者模型中的数据结构**。在`Android`应用的渲染流程中，应用扮演的就是“生产者”的角色，而`SurfaceFlinger`扮演的则是“消费者”的角色，**其配合工作的流程如下**：

1. 应用进程中在开始界面的绘制渲染之前，需要通过`Binder`调用`dequeueBuffer`接口从`SurfaceFlinger`进程中管理的`BufferQueue` 中申请一张处于`free`状态的可用`Buffer`，如果此时没有可用`Buffer`则阻塞等待；
2. 应用进程中拿到这张可用的`Buffer`之后，选择使用`CPU`软件绘制渲染或`GPU`硬件加速绘制渲染，渲染完成后再通过`Binder`调用`queueBuffer`接口将缓存数据返回给应用进程对应的`BufferQueue`（如果是 `GPU` 渲染的话，这里还有个 `GPU`处理的过程，所以这个 `Buffer` 不会马上可用，需要等 `GPU` 渲染完成的`Fence`信号），并申请`sf`类型的`Vsync`以便唤醒“消费者”`SurfaceFlinger`进行消费；
3. `SurfaceFlinger` 在收到 `Vsync` 信号之后，开始准备合成，使用 `acquireBuffer`获取应用对应的 `BufferQueue` 中的 `Buffer` 并进行合成操作；
4. 合成结束后，`SurfaceFlinger` 将通过调用 `releaseBuffer`将 `Buffer` 置为可用的`free`状态，返回到应用对应的 `BufferQueue`中。

## 9.2 Vsync同步机制

`Vysnc`垂直同步是`Android`在“黄油计划”中引入的一个重要机制，本质上是为了协调`BufferQueue`的应用生产者生成UI数据动作和`SurfaceFlinger`消费者的合成消费动作，避免出现画面撕裂的`Tearing`现象。`Vysnc`信号分为两种类型：

1. `app`类型的`Vsync`：**`app`类型的`Vysnc`信号由上层应用中的`Choreographer`根据绘制需求进行注册和接收，用于控制应用UI绘制上帧的生产节奏**。根据第7小结中的分析：应用在UI线程中调用invalidate刷新界面绘制时，需要先透过`Choreographer`向系统申请注册app类型的`Vsync`信号，待`Vsync`信号到来后，才能往主线程的消息队列放入待绘制任务进行真正UI的绘制动作；
2. `sf`类型的`Vsync`:**`sf`类型的`Vsync`是用于控制`SurfaceFlinger`的合成消费节奏**。应用完成界面的绘制渲染后，通过`Binder`调用`queueBuffer`接口将缓存数据返还给应用对应的`BufferQueue`时，会申请`sf`类型的`Vsync`，待`SurfaceFlinger` 在其UI线程中收到 `Vsync` 信号之后，便开始进行界面的合成操作。

`Vsync`信号的生成是参考屏幕硬件的刷新周期的，其架构如下图所示：  

![](https://upload-images.jianshu.io/upload_images/26874665-7a7e75039d05d786.png?imageMogr2/auto-orient/strip|imageView2/2/w/582/format/webp)

vsync.png

  
本小节所描述的流程，从systrace上看`SurfaceFlinger`处理应用上帧工作的流程如下图所示：  

![](https://upload-images.jianshu.io/upload_images/26874665-9d5cefb49aa75c16.png?imageMogr2/auto-orient/strip|imageView2/2/w/856/format/webp)

requestVsync.png

  

![](https://upload-images.jianshu.io/upload_images/26874665-d7e5bebe790e020e.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

SurfaceFlinger处理.png

# 10.写在最后

至此，本文结合源码和systrace完整的分析了从用户手指点击桌面上的应用图标到屏幕上显示出应用主`Activity`界面第一帧画面的完整流程，这其中涉及了`App`应用、`system_server`框架、`Art`虚拟机、`surfaceflinger`等一系列`Android`系统核心模块的相互配合，有很多的细节也由于篇幅所限无法完全展开分析，感兴趣的读者可以结合AOSP源码继续深入分析。而优化应用启动打开的速度这个系统核心用户体验的指标，也是多少年来谷歌、`SOC`芯片厂商、`ODM`手机厂商以及各个应用开发者共同努力优化的方向：

- **对于`SOC`芯片厂商而言**：需要不断升级`CPU`和`GPU`的硬件算力；
- **对于`Android`系统的维护者谷歌而言**：在Android系统大版本升级过程中，不断的优化应用启动过程上的各个系统流程，比如进程创建的速度优化、`Art`虚拟机的引入与性能优化、View绘制流程的简化、硬件绘制加速机制的引入、系统核心AMS、WMS等核心服务的锁优化等；
- **对于各个`ODM`手机厂商而言**：会开发识别应用启动的场景，进行针对性的CPU主频的拉升调节、触控响应速度的优化等机制；
- **对于各个应用开发者而言**：会结合自己的业务对应用启动的场景进行优化，比如尽量减少或推迟在`Application`、`Activity`生命周期函数中的初始化逻辑、去除界面布局的过度绘制、异步化的布局`XML`文件解析等机制。

本文只是分析了应用启动一般性流程，至于如何去优化应用启动的速度，可以关注笔者后续文章的更新，而本文则可以作为应用启动优化课题的一个基础认知。最后用一张流程图来概述一下应用启动流程的全貌：

  

![](https://upload-images.jianshu.io/upload_images/26874665-3228fe250c4f1092.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200)

应用冷启动流程.png

# 11.参考

Systrace 流畅性实战 1 ：了解卡顿原理 [https://www.androidperformance.com/2021/04/24/android-systrace-smooth-in-action-1/](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.androidperformance.com%2F2021%2F04%2F24%2Fandroid-systrace-smooth-in-action-1%2F)  
史上最全Android渲染机制讲解（长文源码深度剖析）[https://mp.weixin.qq.com/s?__biz=MzU2MTk0ODUxOQ==&mid=2247483782&idx=1&sn=f9eae167b217c83036b3a24cd4182cd1&chksm=fc71b38ecb063a9847f4518802fc541091d7f708b112399ec39827e68a6f590249748d643747&mpshare=1&scene=1&srcid=0224RGsfWeG5GyMpxLwEhx7N&sharer_sharetime=1582507745901&sharer_shareid=2d76fc4769fc55b6ca84ec3820ba5821#rd](https://links.jianshu.com/go?to=https%3A%2F%2Fmp.weixin.qq.com%2Fs%3F__biz%3DMzU2MTk0ODUxOQ%3D%3D%26mid%3D2247483782%26idx%3D1%26sn%3Df9eae167b217c83036b3a24cd4182cd1%26chksm%3Dfc71b38ecb063a9847f4518802fc541091d7f708b112399ec39827e68a6f590249748d643747%26mpshare%3D1%26scene%3D1%26srcid%3D0224RGsfWeG5GyMpxLwEhx7N%26sharer_sharetime%3D1582507745901%26sharer_shareid%3D2d76fc4769fc55b6ca84ec3820ba5821%23rd)  
理解Android硬件加速原理的小白文 [https://www.jianshu.com/p/40f660e17a73](https://www.jianshu.com/p/40f660e17a73)