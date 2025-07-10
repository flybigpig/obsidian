---
created: 2025-07-10T14:40:06 (UTC +08:00)
tags: []
source: https://www.jianshu.com/p/996bca12eb1d
author: 
---

# Android Choreographer 源码分析 - 简书

> ## Excerpt
> Android系统从4.1(API 16)开始加入Choreographer这个类来控制同步处理输入(Input)、动画(Animation)、绘制(Draw)三个UI操作。...

---
## Android Choreographer 源码分析

[![](https://cdn2.jianshu.io/assets/default_avatar/8-a356878e44b45ab268a3b0bbaaadeeb7.jpg)](https://www.jianshu.com/u/d0dd7ce127b8)

92016.05.30 11:43:28字数 2,386阅读 35,658

Android系统从4.1(API 16)开始加入Choreographer这个类来控制同步处理输入(Input)、动画(Animation)、绘制(Draw)三个UI操作。其实UI显示的时候每一帧要完成的事情只有这三种。如下图是官网的相关说明：  

![](https://upload-images.jianshu.io/upload_images/1688934-6a44233ae5764492.png?imageMogr2/auto-orient/strip|imageView2/2/w/931/format/webp)

Choreographer

  
Choreographer接收显示系统的时间脉冲(垂直同步信号-VSync信号)，在下一个frame渲染时控制执行这些操作。  
Choreographer中文翻译过来是**"舞蹈指挥"**，字面上的意思就是优雅地指挥以上三个UI操作一起跳一支舞。这个词可以概括这个类的工作，如果android系统是一场芭蕾舞，他就是Android UI显示这出精彩舞剧的编舞，指挥台上的演员们相互合作，精彩演出。Google的工程师看来挺喜欢舞蹈的！  
好了废话不多说，下面让我们来看看剧本是怎么设计的，Let's Read the fucking source code!  
Choreographer的源码位于android.view这个pakage中，是view层框架的一部分，Android studio里面搜一下就可以看到源码了。  
首先看看头部的一些说明，大体了解一下这个类是干嘛的，有助于我们理解接下来的源码。 和官网的文档是一样的，应该就是用这个生成的，和上面一部分相比介绍了Choreographer的使用接口。                                                                                                                                                                              

开发者可以使用Choreographer#postFrameCallback设置自己的callback与Choreographer交互，你设置的callCack会在下一个frame被渲染时触发。Callback有4种类型，Input、Animation、Draw，还有一种是用来解决动画启动问题的，将在下文介绍。这四种操作都是这么触发的。

如下图：  

![](https://upload-images.jianshu.io/upload_images/1688934-c016950ceaa989df.png?imageMogr2/auto-orient/strip|imageView2/2/w/821/format/webp)

Choreographer工作流程

  
收到VSync信号后，顺序执行3个操作，然后等待下一个信号，再次顺序执行3个操作。假设在第二个信号到来之前，所有的操作都执行完成了，即Draw操作完成了，那么第二个信号来到时，此时界面将会更新为第一frame的内容，因为Draw操作已经完成了。否则界面将不会更新，还是显示上一个frame的内容，表示你丢帧了。丢帧是造成卡顿的原因。如下图：  

![](https://upload-images.jianshu.io/upload_images/1688934-9324b8eb156bdc5c.png?imageMogr2/auto-orient/strip|imageView2/2/w/820/format/webp)

丢帧

  
第二个信号到来时，Draw操作没有按时完成，导致第三个时钟周期内显示的还是第一帧的内容。  
注意文档的最后一段话：  
__Each Looper thread has its own choreographer. Other threads can_ post callbacks to run on the choreographer but they will run on the Looper to which the choreographer belongs._\*  
每个线程都有自己的choreographer。

基本上的原理就是上面这样，那么接下来我们通过源码详细地看一下细节是怎么实现的。  
首先先看看构造函数。

## 构造函数

```
private Choreographer(Looper looper) {    
  mLooper = looper;    
  mHandler = new FrameHandler(looper);    
  mDisplayEventReceiver = USE_VSYNC ? new FrameDisplayEventReceiver(looper) : null;    
  mLastFrameTimeNanos = Long.MIN_VALUE;    
  mFrameIntervalNanos = (long)(1000000000 / getRefreshRate());    
  mCallbackQueues = new CallbackQueue[CALLBACK_LAST + 1];   
  for (int i = 0; i <= CALLBACK_LAST; i++) {        
   mCallbackQueues[i] = new CallbackQueue();    
  }
}
```

这里做了几个初始化操作，根据Looper对象生成，Looper和线程是一对一的关系，对应上面说明里的每个线程对应一个Choreographer。

1.初始化FrameHandler。接收处理消息。

2.初始化FrameDisplayEventReceiver。FrameDisplayEventReceiver用来接收垂直同步脉冲，就是VSync信号，VSync信号是一个时间脉冲，一般为60HZ，用来控制系统同步操作，怎么同ChoreoGrapher一起工作的，将在下文介绍。

3.初始化mLastFrameTimeNanos(标记上一个frame的渲染时间)以及mFrameIntervalNanos(帧率,fps，一般手机上为1s/60)。

4.初始化CallbackQueue，callback队列，将在下一帧开始渲染时回调。

我们首先看看FrameHandler和FrameDisplayEventReceiver的结构。

## FrameHandler

```
private final class FrameHandler extends Handler {    
  public FrameHandler(Looper looper) {        
    super(looper);  
  }    
  @Override    
  public void handleMessage(Message msg) {        
    switch (msg.what) {            
      case MSG_DO_FRAME:  
        doFrame(System.nanoTime(), 0);                
      break;            
      case MSG_DO_SCHEDULE_VSYNC:                  
        doScheduleVsync();                
      break;            
      case MSG_DO_SCHEDULE_CALLBACK:                
        doScheduleCallback(msg.arg1);                
      break;        
  }    
  }
}
```

看上面的代码，就是一个简单的Handler。处理3个类型的消息。

MSG\_DO\_FRAME：开始渲染下一帧的操作

MSG\_DO\_SCHEDULE\_VSYNC：请求Vsync信号

MSG\_DO\_SCHEDULE\_CALLBACK：请求执行callback

额，下面再细分一下，分别详细看一下这三个步骤是怎么实现的。继续看源码吧。。。

## FrameDisplayEventReceiver

```
private final class FrameDisplayEventReceiver extends DisplayEventReceiver        
implements Runnable {    
  public FrameDisplayEventReceiver(Looper looper) {    
    super(looper);
  }
  @Override 
  public void onVsync(long timestampNanos, int  builtInDisplayId, int frame) {
  ...     
  mTimestampNanos = timestampNanos;        
  mFrame = frame;        
  Message msg = Message.obtain(mHandler, this);        
  msg.setAsynchronous(true);        
  mHandler.sendMessageAtTime(msg, timestampNanos / TimeUtils.NANOS_PER_MS);    
  }    
  @Override    
  public void run() {        
    mHavePendingVsync = false;        
    doFrame(mTimestampNanos, mFrame);    
  }
}
```

FrameDisplayEventReceiver继承自DisplayEventReceiver接收底层的VSync信号开始处理UI过程。VSync信号由SurfaceFlinger实现并定时发送。FrameDisplayEventReceiver收到信号后，调用onVsync方法组织消息发送到主线程处理。这个消息主要内容就是run方法里面的doFrame了，这里mTimestampNanos是信号到来的时间参数。

FrameHandler和FrameDisplayEventReceiver是怎么工作的呢？ChoreoGrapher的总体流程图如下图：

## 流程图

![](https://upload-images.jianshu.io/upload_images/1688934-a4a5281df5b2bc14.png?imageMogr2/auto-orient/strip|imageView2/2/w/520/format/webp)

choreographer流程图

以上是总体的流程图：  
1.PostCallBack,发起添加回调，这个FrameCallBack将在下一帧被渲染时执行。

2.AddToCallBackQueue,将FrameCallBack添加到回调队列里面，等待时机执行回调。每种类型的callback按照设置的执行时间(dueTime)顺序排序分别保存在一个单链表中。

3.判断FrameCallBack设定的执行时间是否在当前时间之后，若是，发送MSG\_DO\_SCHEDULE\_CALLBACK消息到主线程，安排执行doScheduleCallback，安排执行CallBack。否则直接跳到第4步。

4.执行scheduleFrameLocked，安排执行下一帧。

5.判断上一帧是否已经执行，若未执行，当前操作直接结束。若已经执行，根据情况执行以下6、7步。

6.若使用垂直同步信号进行同步，则执行7.否则，直接跳到9。

7.若当前线程是UI线程，则通过执行scheduleVsyncLocked请求垂直同步信号。否则，送MSG\_DO\_SCHEDULE\_VSYNC消息到主线程，安排执行doScheduleVsync，在主线程调用scheduleVsyncLocked。

8.收到垂直同步信号，调用FrameDisplayEventReceiver.onVsync()，发送消息到主线程，请求执行doFrame。

9.执行doFrame，渲染下一帧。

主要的工作在doFrame中，接下来我们具体看看doFrame函数都干了些什么。  
从名字看很容易理解doFrame函数就是开始进行下一帧的显示工作。好了以下源代码又来了，我们一行一行分析一下吧。

### doFrame

```
void doFrame(long frameTimeNanos, int frame) {    
  final long startNanos;    
  synchronized (mLock) {        
    if (!mFrameScheduled) { //判断是否有callback需要执行，mFrameScheduled会在postCallBack的时候置为true,一次frame执行时置为false       
      return; // no work to do        
    }
    \\\\打印跳frame时间        
    if (DEBUG_JANK && mDebugPrintNextFrameTimeDelta) {            
      mDebugPrintNextFrameTimeDelta = false;            
      Log.d(TAG, "Frame time delta: "                    
              + ((frameTimeNanos - mLastFrameTimeNanos) *  0.000001f) + " ms");        
    }
    //设置当前frame的Vsync信号到来时间        
    long intendedFrameTimeNanos = frameTimeNanos;        
    startNanos = System.nanoTime();//实际开始执行当前frame的时间
    //时间差        
    final long jitterNanos = startNanos - frameTimeNanos;        
    if (jitterNanos >= mFrameIntervalNanos) {
      //时间差大于一个时钟周期，认为跳frame            
      final long skippedFrames = jitterNanos / mFrameIntervalNanos;
      //跳frame数大于默认值，打印警告信息，默认值为30            
      if (skippedFrames >= SKIPPED_FRAME_WARNING_LIMIT) {                
         Log.i(TAG, "Skipped " + skippedFrames + " frames!  "                        
                    + "The application may be doing too much work on its main thread.");            
      }
      //计算实际开始当前frame与时钟信号的偏差值            
      final long lastFrameOffset = jitterNanos % mFrameIntervalNanos; 
      //打印偏差及跳帧信息           
      if (DEBUG_JANK) {                
        Log.d(TAG, "Missed vsync by " + (jitterNanos * 0.000001f) + " ms "                        
                  + "which is more than the frame interval of "                        
                  + (mFrameIntervalNanos * 0.000001f) + " ms!  "                        
                  + "Skipping " + skippedFrames + " frames and setting frame "                        
                  + "time to " + (lastFrameOffset * 0.000001f) + " ms in the past.");            
       }
       //修正偏差值，忽略偏差，为了后续更好地同步工作            
       frameTimeNanos = startNanos - lastFrameOffset;        
    }
    //若时间回溯，则不进行任何工作，等待下一个时钟信号的到来
    //这里为什么会发生时间回溯我没搞明白，大概是未知时钟错误引起？注释里说的maybe 好像不太对        
    if (frameTimeNanos < mLastFrameTimeNanos) {            
    if (DEBUG_JANK) {                
      Log.d(TAG, "Frame time appears to be going backwards.  May be due to a "                        
                + "previously skipped frame.  Waiting for next vsync.");            
   }
   //请求下一次时钟信号            
   scheduleVsyncLocked();            
   return;        
  }
 //记录当前frame信息        
 mFrameInfo.setVsync(intendedFrameTimeNanos,frameTimeNanos);        
 mFrameScheduled = false;
 //记录上一次frame开始时间，修正后的        
 mLastFrameTimeNanos = frameTimeNanos;    
 }    
  try {
    //执行相关callBack        
    Trace.traceBegin(Trace.TRACE_TAG_VIEW, "Choreographer#doFrame");        
    mFrameInfo.markInputHandlingStart();        
    doCallbacks(Choreographer.CALLBACK_INPUT, frameTimeNanos);        
    mFrameInfo.markAnimationsStart();        
    doCallbacks(Choreographer.CALLBACK_ANIMATION, frameTimeNanos);        
    mFrameInfo.markPerformTraversalsStart();        
    doCallbacks(Choreographer.CALLBACK_TRAVERSAL, frameTimeNanos);        
    doCallbacks(Choreographer.CALLBACK_COMMIT, frameTimeNanos);    
  } finally {        
    Trace.traceEnd(Trace.TRACE_TAG_VIEW);    
  }    
  if (DEBUG_FRAMES) {        
    final long endNanos = System.nanoTime();        
    Log.d(TAG, "Frame " + frame + ": Finished, took "                
              + (endNanos - startNanos) * 0.000001f + " ms, latency "                
              + (startNanos - frameTimeNanos) * 0.000001f + " ms.");    
   }
}
```

大部分内容都在上面的注释中说明了，大概是以下的流程:

  

![](https://upload-images.jianshu.io/upload_images/1688934-1badf0d804c3631b.png?imageMogr2/auto-orient/strip|imageView2/2/w/314/format/webp)

doFrame流程图

总结起来其实主要是两个操作：

1.设置当前frame的启动时间。  
判断是否跳帧，若跳帧修正当前frame的启动时间到最近的VSync信号时间。如果没跳帧，当前frame启动时间直接设置为当前VSync信号时间。修正完时间后，无论当前frame是否跳帧，使得当前frame的启动时间与VSync信号还是在一个节奏上的，可能可能延后了一到几个周期，但是节奏点还是吻合的。  
如下图所示是时间修正的一个例子，

  

![](https://upload-images.jianshu.io/upload_images/1688934-77ecfd7adb3f0015.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/651/format/webp)

没有跳帧但延迟

由于第二个frame执行超时，第三个frame实际启动时间比第三个VSync信号到来时间要晚，因为这时候延时比较小，没有超过一个时钟周期，系统还是将frameTimeNanos3传给回调，回调拿到的时间和VSync信号同步。  
再来看看下图：

  

![](https://upload-images.jianshu.io/upload_images/1688934-3bacde7e73429d73.png)

跳帧

由于第二个frame执行时间超过2个时钟周期，导致第三个frame延后执行时间大于一个时钟周期，系统认为这时候影响较大，判定为跳帧了，将第三个frame的时间修正为frameTimeNanos4,比VSync真正到来的时间晚了一个时钟周期。  
时间修正，既保证了doFrame操作和VSync保持同步节奏，又保证实际启动时间与记录的时间点相差不会太大，便于同步及分析。

2.顺序执行callBack队列里面的callback.

然后接下来看看doCallbacks的执行过程:

```
void doCallbacks(int callbackType, long frameTimeNanos) {    
  CallbackRecord callbacks;    
  synchronized (mLock) {        
        // We use "now" to determine when callbacks become due because it's possible        
        // for earlier processing phases in a frame to post callbacks that should run        
        // in a following phase, such as an input event that causes an animation to start.        
        final long now = System.nanoTime();        
        callbacks =  mCallbackQueues[callbackType].extractDueCallbacksLocked(                now / TimeUtils.NANOS_PER_MS);        
        if (callbacks == null) {            
              return;        
        }        
        mCallbacksRunning = true;        
        // Update the frame time if necessary when committing the frame.
        // We only update the frame time if we are more than 2 frames late reaching
        // the commit phase.  This ensures that the frame time which is observed by the
        // callbacks will always increase from one frame to the next and never repeat.
        // We never want the next frame's starting frame time to end up being less than
        // or equal to the previous frame's commit frame time.  Keep in mind that the
        // next frame has most likely already been scheduled by now so we play it
        // safe by ensuring the commit time is always at least one frame behind.
        if (callbackType == Choreographer.CALLBACK_COMMIT) {
            final long jitterNanos = now - frameTimeNanos;
            Trace.traceCounter(Trace.TRACE_TAG_VIEW, "jitterNanos", (int) jitterNanos);
            if (jitterNanos >= 2 * mFrameIntervalNanos) {
                final long lastFrameOffset = jitterNanos % mFrameIntervalNanos
                        + mFrameIntervalNanos;
                if (DEBUG_JANK) {
                    Log.d(TAG, "Commit callback delayed by " + (jitterNanos * 0.000001f)
                            + " ms which is more than twice the frame interval of "
                            + (mFrameIntervalNanos * 0.000001f) + " ms!  "
                            + "Setting frame time to " +(lastFrameOffset * 0.000001f)
                            + " ms in the past.");
                    mDebugPrintNextFrameTimeDelta = true;
                }
                frameTimeNanos = now - lastFrameOffset;
                mLastFrameTimeNanos = frameTimeNanos;
            }
        }
    }
    try {
        Trace.traceBegin(Trace.TRACE_TAG_VIEW, CALLBACK_TRACE_TITLES[callbackType]);
        for (CallbackRecord c = callbacks; c != null; c = c.next) {
            if (DEBUG_FRAMES) {
                Log.d(TAG, "RunCallback: type=" + callbackType
                        + ", action=" + c.action + ", token=" + c.token
                        + ", latencyMillis=" + (SystemClock.uptimeMillis() - c.dueTime));
            }
            c.run(frameTimeNanos);
        }
    } finally {
        synchronized (mLock) {
            mCallbacksRunning = false;
            do {
                final CallbackRecord next = callbacks.next;
                recycleCallbackLocked(callbacks);
                callbacks = next;
            } while (callbacks != null);
        }
        Trace.traceEnd(Trace.TRACE_TAG_VIEW);
    }
}
```

callback的类型有以下4种，除了文章一开始提到的3中外，还有一个CALLBACK\_COMMIT。

CALLBACK\_INPUT：输入  
CALLBACK\_ANIMATION:动画  
CALLBACK\_TRAVERSAL:遍历，执行measure、layout、draw  
CALLBACK\_COMMIT：遍历完成的提交操作，用来修正动画启动时间

然后看上面的源码，分析一下每个callback的执行过程：

1.callbacks = mCallbackQueues\[callbackType\].extractDueCallbacksLocked( now / TimeUtils.NANOS\_PER\_MS);得到执行时间在当前时间之前的所有CallBack，保存在单链表中。每种类型的callback按执行时间先后顺序排序分别存在一个单链表里面。为了保证当前callback执行时新post进来的callback在下一个frame时才被执行，这个地方extractDueCallbacksLocked会将需要执行的callback和以后执行的callback断开变成两个链表，新post进来的callback会被放到后面一个链表中。当前frame只会执行前一个链表中的callback，保证了在执行callback时，如果callback中Post相同类型的callback，这些新加的callback将在下一个frame启动后才会被执行。

2.接下来，看一大段注释，如果类型是CALLBACK\_COMMIT，并且当前frame渲染时间超过了两个时钟周期，则将当前提交时间修正为上一个垂直同步信号时间。为了保证下一个frame的提交时间和当前frame时间相差为一且不重复。  
这个地方注释挺难看懂，实际上这个地方CALLBACK\_COMMIT是为了解决ValueAnimator的一个问题而引入的，主要是解决因为遍历时间过长导致动画时间启动过长，时间缩短，导致跳帧，这里修正动画第一个frame开始时间延后来改善，这时候才表示动画真正启动。为什么不直接设置当前时间而是回溯一个时钟周期之前的时间呢？看注释，这里如果设置为当前frame时间，因为动画的第一个frame其实已经绘制完成，第二个frame这时候已经开始了，设置为当前时间会导致这两个frame时间一样，导致冲突。详细情况请看官方针对这个问题的修改。[Fix animation start jank due to expensive layout operations.](https://link.jianshu.com/?t=https://android.googlesource.com/platform/frameworks/base/+/c42b28d%5E!/)

如下图所示：

  

![](https://upload-images.jianshu.io/upload_images/1688934-146921f79d6f5935.jpg)

修正commit时间

比如说在第二个frame开始执行时，开始渲染动画的第一个画面，第二个frame执行时间超过了两个时钟周期，Draw操作执行结束后，这时候完成了动画第一帧的渲染，动画实际上还没开始，但是时间已经过了两个时钟周期，后面动画实际执行时间将会缩短一个时钟周期。这时候系统通过修正commit时间到frameTimeNanos的上一个VSync信号时间，即完成动画第一帧渲染之前的VSync信号到来时间，修正了动画启动时间，保证动画执行时间的正确性。

3.接下来就是调用c.run(frameTimeNanos);执行回调。  
例如，你可以写一个自定义的FPSFrameCallback继承自Choreographer.FrameCallback，实现里面的doFrame方法。

```
public class FPSFrameCallback implements Choreographer.FrameCallback{
@Override
  public void doFrame(long frameTimeNanos){
      //do something
  }
}
```

通过  
Choreographer.getInstance().postFrameCallback(new FPSFrameCallback());  
把你的回调添加到Choreographer之中，那么在下一个frame被渲染的时候就会回调你的callback,执行你定义的doFrame操作，这时候你就可以获取到这一帧的开始渲染时间并做一些自己想做的事情了。  
开源组件Tiny Dancer就是根据这个原理获取每一帧的渲染时间，继而分析实现获取设备的当前帧率的。有兴趣的人可以查看。  
[Tiny Dancer](https://link.jianshu.com/?t=https://github.com/friendlyrobotnyc/TinyDancer)

好了，关于Choreographer的分析到此结束。希望对你有帮助。

最后编辑于

：2017.12.03 06:04:03

©

著作权归作者所有,转载或内容合作请联系作者  
平台声明：文章内容（如有图片或视频亦包括在内）由作者上传并发布，文章内容仅代表作者本人观点，简书系信息发布平台，仅提供信息存储服务。

更多精彩内容，就在简书APP

"小礼物走一走，来简书关注我"

![  ](https://cdn2.jianshu.io/assets/default_avatar/wechat-771371d5741b0c32cd805caeb48ad6c0.png)共1人赞赏

[![  ](https://cdn2.jianshu.io/assets/default_avatar/8-a356878e44b45ab268a3b0bbaaadeeb7.jpg)](https://www.jianshu.com/u/d0dd7ce127b8)

总资产10共写了1.1W字获得407个赞共404个粉丝

### 推荐阅读[更多精彩内容](https://www.jianshu.com/)

-   Android系统从4.1(API 16)开始加入Choreographer这个类来控制同步处理输入(Input)...
    
-   本文章转载于搜狗测试 前言：那些年我们用过的显示性能指标 相对其他 Android 性能指标（如内存、CPU、功耗...
    
    [![](https://cdn2.jianshu.io/assets/default_avatar/13-394c31a9cb492fcb39c27422ca7d2815.jpg)夜境](https://www.jianshu.com/u/6c0c0b1fb77a)阅读 1,410评论 0赞 4
    
-   前言：那些年我们用过的显示性能指标 注：Google 在自己文章中用了 Display Performance 来...
    
-   1\. 前言 上一篇文章《Android Animation运行原理详解》介绍了插间动画的原理，而Android3....
    
-   有什么料？ 从这篇文章中你能获得这些料： 知道setContentView()之后发生了什么？ 知道Android...
