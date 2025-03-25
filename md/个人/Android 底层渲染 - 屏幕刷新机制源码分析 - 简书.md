## Android 底层渲染 - 屏幕刷新机制源码分析

[![](https://upload.jianshu.io/users/upload_avatars/4314397/39de1eb1-5b12-461e-9fa1-ec88ce1d90a6.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/96/h/96/format/webp)](https://www.jianshu.com/u/35083fcb7747)

12020.03.18 21:30:28字数 555阅读 3,163

相关文章链接：

[1\. Android Framework - 学习启动篇](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.csdn.net%2Fz240336124%2Farticle%2Fdetails%2F98117999)  
[2\. 源码阅读分析 - Window底层原理与系统架构](https://www.jianshu.com/p/3f2510a38a91)

相关源码文件：

```
/frameworks/base/core/java/android/view/ViewRootImpl.java
/frameworks/base/core/java/android/view/Choreographer.java
/frameworks/base/core/java/android/view/DisplayEventReceiver.java

/frameworks/base/core/jni/android_view_DisplayEventReceiver.cpp
/frameworks/native/libs/gui/DisplayEventReceiver.cpp
/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp
/frameworks/native/services/surfaceflinger/EventThread.cpp
/frameworks/native/libs/gui/BitTube.cpp
```

### 1\. 梳理概述

在开始阅读文章前，希望大家能认真思考几个问题：

-   界面卡顿的原理是怎样的？
-   ViewRootImpl 与 SurfaceFlinger 是怎么通信的？
-   invalidate / requestLayout 会不会立马刷新屏幕？
-   SurfaceView / GLSurfaceView 的底层实现原理？

搞 Android 搞了几年，我们对 VSync 信号应该会有一些了解，但是未必真正能理解其具体原理。比如 VSync 信号是从哪里来的？发到哪里去？有什么作用？本文主要讲解发到哪里去，至于从哪里来的大家可以看看之前的内容。

### 2\. 请求 VSync 信号

如果我们的界面需要发生变化，一般都会来到 ViewRootImpl 的 requestLayout 方法，有可能是手动触发的也有可能是被动触发的，在这个方法里面我们会主动去请求接收 VSync 信号，当下一次 VSync 信号的来的时候会主动回掉回来，然后才开始真正的绘制流程。

```
    @Override
    public void requestLayout() {
      if (!mHandlingLayoutInLayoutRequest) {
        checkThread();
        mLayoutRequested = true;
        scheduleTraversals();
      }
    }

    void scheduleTraversals() {
      if (!mTraversalScheduled) {
        mTraversalScheduled = true;
        // 插入一条消息屏障
        mTraversalBarrier = mHandler.getLooper().getQueue().postSyncBarrier();
        // post 一个 Callback
        mChoreographer.postCallback(Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null);
      }
    }

    private void postCallbackDelayedInternal(int callbackType, Object action, Object token, long delayMillis) {
      synchronized (mLock) {
        ...
        if (dueTime <= now) {
          scheduleFrameLocked(now);
        } else {
          ...
        }
      }
    }

    private void scheduleFrameLocked(long now) {
      if (!mFrameScheduled) {
        mFrameScheduled = true;
        if (USE_VSYNC) {
          // 是否在 Choreographer 的工作线程
          if (isRunningOnLooperThreadLocked()) {
            scheduleVsyncLocked();
          } else {
            Message msg = mHandler.obtainMessage(MSG_DO_SCHEDULE_VSYNC);
            msg.setAsynchronous(true);
            mHandler.sendMessageAtFrontOfQueue(msg);
          }
        } else {
          ...
        }
      }
    }
    // 请求接收下一次 VSync 信号
    private void scheduleVsyncLocked() {
      mDisplayEventReceiver.scheduleVsync();
    }
```

由上面的源码可以看出，每一次调用 requestLayout 方法，都会主动调用 scheduleVsync 方法来接收下一次的 VSync 信号。也就是说在下一次 VSync 信号来之前，就算连续调用 n 次的 requestLayout 方法，也并不会触发刷新绘制流程。

### 3\. 接收 VSync 信号

应用 App 请求了要接收下一次的 VSync 信号，那么 SurfaceFlinger 服务怎么把 VSync 信号，发给我们的应用 App ？这个得从 DisplayEventReceiver 的初始化入手，涉及到跨进程通信也涉及到 Native 层源码。

```
    public DisplayEventReceiver(Looper looper) {
      ...
      // nativeInit 
      mReceiverPtr = nativeInit(new WeakReference<DisplayEventReceiver>(this), mMessageQueue);
    }

    static jlong nativeInit(JNIEnv* env, jclass clazz, jobject receiverWeak, jobject messageQueueObj) {
      sp<NativeDisplayEventReceiver> receiver = new NativeDisplayEventReceiver(env, receiverWeak, messageQueue);
      // 初始化方法
      status_t status = receiver->initialize();
      receiver->incStrong(gDisplayEventReceiverClassInfo.clazz); // retain a reference for the object
      return reinterpret_cast<jlong>(receiver.get());
    }

    status_t NativeDisplayEventReceiver::initialize() {
      // 接收端的 fd 添加到 Looper
      int rc = mMessageQueue->getLooper()->addFd(mReceiver.getFd(), 0, Looper::EVENT_INPUT,this, NULL);
      return OK;
    }
    // 跨进程创建一个 mEventConnection 对象
    DisplayEventReceiver::DisplayEventReceiver() {
      sp<ISurfaceComposer> sf(ComposerService::getComposerService());
      if (sf != NULL) {
        mEventConnection = sf->createDisplayEventConnection();
        if (mEventConnection != NULL) {
            mDataChannel = mEventConnection->getDataChannel();
        }
      }
    }
    // 获取接收端的 fd
    int DisplayEventReceiver::getFd() const {
      if (mDataChannel == NULL)
          return NO_INIT;
      return mDataChannel->getFd();
    }
    // VSync 信号来会回调到这个方法
    int NativeDisplayEventReceiver::handleEvent(int receiveFd, int events, void* data) {
      // Drain all pending events, keep the last vsync.
      nsecs_t vsyncTimestamp;
      int32_t vsyncDisplayId;
      uint32_t vsyncCount;
      if (processPendingEvents(&vsyncTimestamp, &vsyncDisplayId, &vsyncCount)) {
        mWaitingForVsync = false;
        dispatchVsync(vsyncTimestamp, vsyncDisplayId, vsyncCount);
      }
      return 1; // keep the callback
    }

    void NativeDisplayEventReceiver::dispatchVsync(nsecs_t timestamp, int32_t id, uint32_t count) {
      JNIEnv* env = AndroidRuntime::getJNIEnv();
      ScopedLocalRef<jobject> receiverObj(env, jniGetReferent(env, mReceiverWeakGlobal));
      if (receiverObj.get()) {
        // 回掉到 Java 层的 dispatchVsync 方法
        env->CallVoidMethod(receiverObj.get(),
                gDisplayEventReceiverClassInfo.dispatchVsync, timestamp, id, count);
      }
    }

    @Override
    public void onVsync(long timestampNanos, int builtInDisplayId, int frame) {
      // 发消息执行 doFrame 方法，真正开始刷新绘制流程
      mTimestampNanos = timestampNanos;
      mFrame = frame;
      Message msg = Message.obtain(mHandler, this);
      msg.setAsynchronous(true);
      mHandler.sendMessageAtTime(msg, timestampNanos / TimeUtils.NANOS_PER_MS);
    }
```

DisplayEventReceiver 在初始化时会创建与 SurfaceFlinger 的 Connection 连接，当应用 App 主动发起 requestNextVsync 后，SurfaceFlinger 会在下一个 VSync 信号来的时候，主动通知我们的应用 App ，回掉到 Java 层的 onVsync 方法，开始真正的刷新绘制流程。

视频地址：[https://pan.baidu.com/s/1tQ7omRNg8BgldnkjdlBPlw](https://links.jianshu.com/go?to=https%3A%2F%2Fpan.baidu.com%2Fs%2F1tQ7omRNg8BgldnkjdlBPlw)  
视频密码：6hlc

最后编辑于

：2020.03.21 22:10:13

更多精彩内容，就在简书APP

![](https://upload.jianshu.io/images/js-qrc.png)

"小礼物走一走，来简书关注我"

还没有人赞赏，支持一下

[![  ](https://upload.jianshu.io/users/upload_avatars/4314397/39de1eb1-5b12-461e-9fa1-ec88ce1d90a6.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/100/h/100/format/webp)](https://www.jianshu.com/u/35083fcb7747)

[红橙Darren](https://www.jianshu.com/u/35083fcb7747 "红橙Darren")目前就职于腾讯微信，腾讯面试官，专注移动端开发，热爱技术，乐于分享！

总资产84共写了23.0W字获得7,282个赞共8,197个粉丝

-   序言：七十年代末，一起剥皮案震惊了整个滨河市，随后出现的几起案子，更是在滨河造成了极大的恐慌，老刑警刘岩，带你破解...
    
    [![](https://upload.jianshu.io/users/upload_avatars/15878160/783c64db-45e5-48d7-82e4-95736f50533e.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)沈念sama](https://www.jianshu.com/u/dcd395522934)阅读 224,014评论 6赞 522
    
-   序言：滨河连续发生了三起死亡事件，死亡现场离奇诡异，居然都是意外死亡，警方通过查阅死者的电脑和手机，发现死者居然都...
    
-   文/潘晓璐 我一进店门，熙熙楼的掌柜王于贵愁眉苦脸地迎上来，“玉大人，你说我怎么就摊上这事。” “怎么了？”我有些...
    
-   文/不坏的土叔 我叫张陵，是天一观的道长。 经常有香客问我，道长，这世上最难降的妖魔是什么？ 我笑而不...
    
-   正文 为了忘掉前任，我火速办了婚礼，结果婚礼上，老公的妹妹穿的比我还像新娘。我一直安慰自己，他们只是感情好，可当我...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 69,636评论 6赞 399
    
-   文/花漫 我一把揭开白布。 她就那样静静地躺着，像睡着了一般。 火红的嫁衣衬着肌肤如雪。 梳的纹丝不乱的头发上，一...
    
-   那天，我揣着相机与录音，去河边找鬼。 笑死，一个胖子当着我的面吹牛，可吹牛的内容都是我干的。 我是一名探鬼主播，决...
    
-   文/苍兰香墨 我猛地睁开眼，长吁一口气：“原来是场噩梦啊……” “哼！你这毒妇竟也来了？” 一声冷哼从身侧响起，我...
    
-   序言：老挝万荣一对情侣失踪，失踪者是张志新（化名）和其女友刘颖，没想到半个月后，有当地人在树林里发现了一具尸体，经...
    
-   正文 独居荒郊野岭守林人离奇死亡，尸身上长有42处带血的脓包…… 初始之章·张勋 以下内容为张勋视角 年9月15日...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 39,069评论 3赞 344
    
-   正文 我和宋清朗相恋三年，在试婚纱的时候发现自己被绿了。 大学时的朋友给我发了我未婚夫和他白月光在一起吃饭的照片。...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 41,211评论 1赞 354
    
-   序言：一个原本活蹦乱跳的男人离奇死亡，死状恐怖，灵堂内的尸体忽然破棺而出，到底是诈尸还是另有隐情，我是刑警宁泽，带...
    
-   正文 年R本政府宣布，位于F岛的核电站，受9级特大地震影响，放射性物质发生泄漏。R本人自食恶果不足惜，却给世界环境...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 42,541评论 3赞 336
    
-   文/蒙蒙 一、第九天 我趴在偏房一处隐蔽的房顶上张望。 院中可真热闹，春花似锦、人声如沸。这庄子的主人今日做“春日...
    
-   文/苍兰香墨 我抬头看了看天上的太阳。三九已至，却和暖如春，着一层夹袄步出监牢的瞬间，已是汗流浃背。 一阵脚步声响...
    
-   我被黑心中介骗来泰国打工， 没想到刚下飞机就差点儿被人妖公主榨干…… 1. 我叫王不留，地道东北人。 一个月前我还...
    
-   正文 我出身青楼，却偏偏与公主长得像，于是被迫代替她去往敌国和亲。 传闻我的和亲对象是个残疾皇子，可洞房花烛夜当晚...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 46,240评论 2赞 363
    

### 推荐阅读[更多精彩内容](https://www.jianshu.com/)

-   这篇博文是参考别人的博客，结合源码自己又走了一遍，仅供自己记录学习。 我们要掌握android，那么关于andro...
    
-   1、概述 不论电脑，电视，手机，我们看到的画面都是由一帧帧的画面组成的。FPS是图像领域中的定义，是指画面每秒传输...
    
    [![](https://upload.jianshu.io/users/upload_avatars/8398510/252ba685-b7b3-4e7e-9310-9ccef88b7281?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)高丕基](https://www.jianshu.com/u/0febfe6c679c)阅读 12,107评论 6赞 34
    
-   转载请注明出处：http://blog.csdn.net/a740169405/article/details/7...
    
    [![](https://cdn2.jianshu.io/assets/default_avatar/8-a356878e44b45ab268a3b0bbaaadeeb7.jpg)良秋](https://www.jianshu.com/u/46823256ecfc)阅读 14,141评论 8赞 81
    
-   从本篇文章开始，我将对Android比较复杂的图形系统进行分析，开篇我们先对图形系统做个概览，先不对代码做具体分析...
    
-   上一篇文章面试官：说说多线程并发问题阅读量和点赞数超出我的想象，这周带来这个系列第二篇。 故事开始 面试官：平时开...