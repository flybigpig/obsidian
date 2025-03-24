       ![](https://lf-web-assets.juejin.cn/obj/juejin-web/xitu_juejin_web/img/banner.a5c9f88.jpg)

> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

同步屏障这个概念有点不好理解，为了搞清楚同步屏障，首先要聊聊消息系统中消息的类型，消息类型实际分为 2 种：

-   同步消息：应用层开发使用的都是此类型消息
-   异步消息：Framework层使用

异步消息创建

-   Handler创建时设置为异步，通过此handler发送的消息都会被设置为异步

```
public Handler(boolean async) {
    this(null, async);
}
```

-   创建Message时调用如下方法设置为异步消息

```
public void setAsynchronous(boolean async)
```

除了上诉情况外，创建的消息都是同步消息。

**同步屏障作用：阻碍同步消息，让异步消息优先被执行**，这就是同步屏障的含义。

## 开启同步屏障

```
public int postSyncBarrier() {
    return postSyncBarrier(SystemClock.uptimeMillis());
}

private int postSyncBarrier(long when) {
    // Enqueue a new sync barrier token.
    // We don't need to wake the queue because the purpose of a barrier is to stall it.
    synchronized (this) {
        final int token = mNextBarrierToken++;
        final Message msg = Message.obtain();
        msg.markInUse();
        //获取同步屏障msg，但是没有设置target，这个时同步屏障的标识
        msg.when = when;
        msg.arg1 = token;

        Message prev = null;
        Message p = mMessages;
        if (when != 0) {
            //在mMessages队列中查找大于SystemClock.uptimeMillis()的第一个消息节点
            while (p != null && p.when <= when) {
                prev = p;
                p = p.next;
            }
        }
        //然后将同步屏障msg插入到查找到的节点前面
        if (prev != null) { // invariant: p == prev.next
            msg.next = p;
            prev.next = msg;
        } else {
            msg.next = p;
            mMessages = msg;
        }
        return token;
    }
}
```

调用MessageQueue.postSyncBarrier开始同步屏障，过程如下：

-   获取同步屏障msg，但是没有设置target，这个时同步屏障的标识；
-   将同步屏障msg按执行时间顺序插入到mMessages中（需要注意的时，此时队列中可能有到时间需要进行分发的消息了，如下图所示）

![同步屏障.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/7305cd04b26042e2b86ba3759e4141db~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743239872&x-signature=4uDjlerIDgJ7OgpxkG77%2Fr8xYlo%3D)

-   when=0 表示此同步消息已经到需要处理的时间了
-   同步屏障插入到第二个节点

按正常情况消息应该是安装队列when时间一次执行，当时插入同步屏障后就变得不一样了。

## 同步屏障如何工作

同步屏障会影响消息的获取，也就是MessageQueue.next方法

```
Message next() {
    for (;;) {
        nativePollOnce(ptr, nextPollTimeoutMillis);

        synchronized (this) {
            // Try to retrieve the next message.  Return if found.
            final long now = SystemClock.uptimeMillis();
            Message prevMsg = null;
            Message msg = mMessages;
            if (msg != null && msg.target == null) {
                // Stalled by a barrier.  Find the next asynchronous message in the queue.
                do {
                    prevMsg = msg;
                    msg = msg.next;
                } while (msg != null && !msg.isAsynchronous());
            }
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

-   检查消息是否同步屏障msg.target == null
    -   找到同步屏障后，遍历队列查找异步消息msg.isAsynchronous()，这个过程中when=10000 的同步消息被调过去了，优先执行异步消息when=10010
    -   一直循环处理队列中所有异步消息，直到处理完毕后再处理同步消息
-   非同步屏障则按照正常流程执行，返回when=0 的msg

## 异步消息的应用场景

似乎在日常的应用开发中，很少会用到同步屏障。那么，同步屏障在系统源码中有哪些使用场景呢？

Android系统中的UI更新相关的消息即为异步消息，需要优先处理。 比如，在 View 更新时，draw、requestLayout、invalidate 等很多地方都调用了ViewRootImpl#scheduleTraversals()，如下：

```
//ViewRootImpl.java 
void scheduleTraversals() {
    if (!mTraversalScheduled) {
        mTraversalScheduled = true;
        //开启同步屏障
        mTraversalBarrier = mHandler.getLooper().getQueue().postSyncBarrier();
        //发送异步消息
        mChoreographer.postCallback(
            Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null);
        if (!mUnbufferedInputDispatch) {
            scheduleConsumeBatchedInput();
        }
        notifyRendererOfFramePending();
        pokeDrawLockIfNeeded();
    }
}
```

postCallback()最终走到了ChoreographerpostCallbackDelayedInternal()：

```
private void postCallbackDelayedInternal(int callbackType,
        Object action, Object token, long delayMillis) {
    synchronized (mLock) {
        final long now = SystemClock.uptimeMillis();
        final long dueTime = now + delayMillis;
        mCallbackQueues[callbackType].addCallbackLocked(dueTime, action, token);

        if (dueTime <= now) {
            scheduleFrameLocked(now);
        } else {
            Message msg = mHandler.obtainMessage(MSG_DO_SCHEDULE_CALLBACK, action);
            msg.arg1 = callbackType;
            //这里将消息设置为异步消息
            msg.setAsynchronous(true);
            mHandler.sendMessageAtTime(msg, dueTime);
        }
    }
}
```

这里就开启了同步屏障，并发送异步消息，由于 UI 更新相关的消息是优先级最高的，这样系统就会优先处理这些异步消息。 最后，当要移除同步屏障的时候需要调用ViewRootImpl#unscheduleTraversals()。

```
void unscheduleTraversals() {
    if (mTraversalScheduled) {
        mTraversalScheduled = false;
        //移除同步屏障
        mHandler.getLooper().getQueue().removeSyncBarrier(mTraversalBarrier);
        mChoreographer.removeCallbacks(
        Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null);
    }
}
```

## 总结

**同步屏障作用：阻碍同步消息，让异步消息优先被执行**，但是需要注意的是：即便时开启了同步屏障，也是要先执行到期的同步消息，然后执行异步消息，最后才执行未到期的同步消息。