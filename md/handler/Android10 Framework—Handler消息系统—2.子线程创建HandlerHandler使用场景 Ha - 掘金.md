       ![](https://lf-web-assets.juejin.cn/obj/juejin-web/xitu_juejin_web/img/banner.a5c9f88.jpg)

> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

## Handler使用场景

Handler消息系统使用分为 2 种场景

-   子线程A中创建Handler，此场景是其它子线程和UI与子线程A通信

```
Handler mThreadHandler = null;

new Thread(new Runnable() {
    @Override
    public void run() {
        Looper.prepare();
        mThreadHandler = new Handler(new Handler.Callback() {
            @Override
            public boolean handleMessage(Message message) {
                return false;
            }
        });
        Looper.loop();
    }
}).start();

```

-   UI线程创建Handler，其它子线程和UI线程通信

```
mMainHandler = new Handler(new Handler.Callback() {
    @Override
    public boolean handleMessage(Message message) {
        return false;
    }这个代码
});
```

子线程比UI线程中创建Handler多了如下 2 处代码，因为UI线程的创建是由JVM发起的，UI线程的这个代码细节被系统隐藏了，后续分析“UI线程创建Handler”会将其剖析出来，本片文章重点分析“子线程创建Handler”。

```
Looper.prepare();
Looper.loop();
```

## 消息发送

消息队列的构建是从发送消息开始的，因此我们先讲一下发送消息，接口如下：

```
boolean sendMessage(@NonNull Message msg)
boolean sendEmptyMessageDelayed(int what, long delayMillis)
```

最终会调用如下方法

```
private boolean enqueueMessage(@NonNull MessageQueue queue, @NonNull Message msg,
        long uptimeMillis) {
    msg.target = this;
    msg.workSourceUid = ThreadLocalWorkSource.getUid();

    if (mAsynchronous) {
        msg.setAsynchronous(true);
    }
    return queue.enqueueMessage(msg, uptimeMillis);
}   
```

-   uptimeMillis：表示消息在消息队列中等到什么时间是被分发处理
-   target:指向当前的Handler实例，当uptimeMillis时间到时会通过target进行回调
-   queue：Handler被创建时绑定的MessageQueue

```
boolean enqueueMessage(Message msg, long when) {
    synchronized (this) {
        msg.markInUse();
        msg.when = when;
        Message p = mMessages;
        boolean needWake;
        if (p == null || when == 0 || when < p.when) {
            // New head, wake up the event queue if blocked.
            msg.next = p;
            mMessages = msg;
            needWake = mBlocked;
        } else {
            // Inserted within the middle of the queue.  Usually we don't have to wake
            // up the event queue unless there is a barrier at the head of the queue
            // and the message is the earliest asynchronous message in the queue.
            needWake = mBlocked && p.target == null && msg.isAsynchronous();
            Message prev;
            for (;;) {
                prev = p;
                p = p.next;
                if (p == null || when < p.when) {
                    break;
                }
                if (needWake && p.isAsynchronous()) {
                    needWake = false;
                }
            }
            msg.next = p; // invariant: p == prev.next
            prev.next = msg;
        }

        // We can assume mPtr != 0 because mQuitting is false.
        if (needWake) {
            nativeWake(mPtr);
        }
    }
    return true;
}
```

-   p == null || when == 0 || when < p.when：满足三个条件之一，则将消息插入消息队列头部
    -   p == null：表示mMessages消息队列为空；
    -   when == 0：当前发送的消息需要立刻执行
    -   when < p.when：当前发送的消息执行时间比消息队列头还要早
-   else分支：遍历消息队列从队列中找到第一个消息节点的时间>当前发送的消息执行时间，然后将其插入到队列中
-   当有需要处理的消息时会调用nativeWake去唤醒阻塞在mMessages队列上的线程（消息监听与分发处理会讲）

简化代码如下

```
Message prev;
for (;;) {
    prev = p;
    p = p.next;
    if (p == null || when < p.when) {
        break;
    }
}
msg.next = p; // invariant: p == prev.next
prev.next = msg;
```

整个过程其实就是一个链表插入，链表的排序是按照消息执行时间从小到大进行排序，mMessages是链表头指针，最终构建出如下消息队列

![消息队列.png](https://p9-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/0723c8e96f304398a5e57a3be1dd2ef5~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743239991&x-signature=mWReqU0O3U6a3QVxy5fWj16hTfY%3D)

## 消息监听与分发处理

下面代码执行完成后子线程就会阻塞监听消息队列，当消息队列中有消息时子线程会被唤醒分发消息给mThreadHandler处理。 我们先看看这段代码的背后都做了什么

```
new Thread(new Runnable() {
    @Override
    public void run() {
        Looper.prepare();
        Looper.loop();
    }
}).start();
```

上面的代码我们分三步来分析：  
**①Looper.prepare()**  
**②new Handler()**  
**③Looper.loop()**

### Looper.prepare

底层调用代码如下

```
public static void prepare() {  
    prepare(true);  
}  
  
private static void prepare(boolean quitAllowed) {  
    if (sThreadLocal.get() != null) {  
        throw new RuntimeException("Only one Looper may be created per thread");  
    }  
    sThreadLocal.set(new Looper(quitAllowed));  
}

private Looper(boolean quitAllowed) {  
    mQueue = new MessageQueue(quitAllowed);  
    mThread = Thread.currentThread();  
}
```

创建了一个Looper实例，并将其存储到sThreadLocal中，这样就保证了子线程-Looper实例-MessageQueue实例的唯一关系。

**注意：这里的quitAllowed=ture，标识Looper是可以退出的，这点是和UI线程有差异的，后续也会专门说明。**

### new Handler

```
public Handler(@Nullable Callback callback, boolean async) {
    mLooper = Looper.myLooper();
    
    mQueue = mLooper.mQueue;
    mCallback = callback;
}
```

重点在于Looper.myLooper()

```
public static @Nullable Looper myLooper() {
    return sThreadLocal.get();
}
```

显而易见，Handler在哪个线程实例化，就会与该线程的Looper和MessageQueue进行绑定。

### Looper.loop

```
public static void loop() {
    final Looper me = myLooper();
    final MessageQueue queue = me.mQueue;

    for (;;) {
        //当，如果没有消息线程会一直在next()方法调用上休眠，直到有消息时被唤醒
        Message msg = queue.next(); // might block 
        //消息分发
        msg.target.dispatchMessage(msg);
    }
}  
```

再进行看next方法

```
Message next() {
    int pendingIdleHandlerCount = -1; // -1 only during first iteration
    int nextPollTimeoutMillis = 0;
    for (;;) {
        nativePollOnce(ptr, nextPollTimeoutMillis);

        synchronized (this) {
            // Try to retrieve the next message.  Return if found.
            final long now = SystemClock.uptimeMillis();
            Message prevMsg = null;
            Message msg = mMessages;
            
            if (msg != null) {
                if (now < msg.when) {
                    // Next message is not ready.  Set a timeout to wake up when it is ready.
                    nextPollTimeoutMillis = (int) Math.min(msg.when - now, Integer.MAX_VALUE);
                } else {
                    // Got a message.
                    mBlocked = false;
                    if (prevMsg != null) {
                        prevMsg.next = msg.next;
                    } else {
                        mMessages = msg.next;
                    }
                    msg.next = null;
                    if (DEBUG) Log.v(TAG, "Returning message: " + msg);
                    msg.markInUse();
                    return msg;
                }
            } else {
                // No more messages.
                nextPollTimeoutMillis = -1;
            }
        }
    }
}
```

nativePollOnce是一个native函数

-   当nextPollTimeoutMillis=-1时，调用此方法的线程会一直休眠直到调用nativeWake才被唤醒；
-   当nextPollTimeoutMillis>0时，调用此方法的线程会休眠nextPollTimeoutMillis时间后被系统唤醒；

next()方法会从mMessages队列中循环获取消息

-   消息队列中没有消息nextPollTimeoutMillis = -1，则会一直阻塞；
-   消息队列有消息
    -   当`now < msg.when`时表示队列头的消息还未到执行它的时间，那么就会计算时间差并进行休眠以等待直到时间到来;
    -   否则标识执行消息的时间到了，从队列中取出消息，并将其返回； 消息返回后就会调用如下方法进行分发了

```
msg.target.dispatchMessage(msg);
```

target指向的是发送此msg的Handler

```
public void dispatchMessage(@NonNull Message msg) {
    if (msg.callback != null) {
        handleCallback(msg);
    } else {
        if (mCallback != null) {
            if (mCallback.handleMessage(msg)) {
                return;
            }
        }
        handleMessage(msg);
    }
}   
```

到此为止就完成了消息的消费。