       ![](https://lf-web-assets.juejin.cn/obj/juejin-web/xitu_juejin_web/img/banner.a5c9f88.jpg)

> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

`IdleHandler`是`android.os.MessageQueue`的一个内部接口，其定义如下

```
/**
 * Callback interface for discovering when a thread is going to block
 * waiting for more messages.
 */
public static interface IdleHandler {
    /**
     * Called when the message queue has run out of messages and will now
     * wait for more.  Return true to keep your idle handler active, false
     * to have it removed.  This may be called if there are still messages
     * pending in the queue, but they are all scheduled to be dispatched
     * after the current time.
     */
    //false：表示只监听一次idle状态；ture：一直监听队列的idle状态
    boolean queueIdle();
}
```

可以把它理解为`Runnable`。`IdleHandler`会在`MessageQueue`中没有`Message`要处理或者要处理的`Message`都是延时任务的时候得到执行，这种特性很重要，因为当`MessageQueue`中没有`Message`要处理或者要处理的`Message`都是延时任务的时候，就表明当前线程为空闲状态，如果是在主线程，则表明当前UI没有绘制动作，所以可以根据监听`IdleHandler`是否执行来判断UI是否绘制完成，从而避免在UI绘制的时候进行耗时操作，影响UI绘制效率。

## 添加和移除IdleHandler

//MessageQueue.java

```
public void addIdleHandler(@NonNull IdleHandler handler) {
    if (handler == null) {
        throw new NullPointerException("Can't add a null IdleHandler");
    }
    synchronized (this) {
        mIdleHandlers.add(handler);
    }
}

public void removeIdleHandler(@NonNull IdleHandler handler) {
    synchronized (this) {
        mIdleHandlers.remove(handler);
    }
}
```

然后看看MessageQueue.next方法

```
Message next() {
    //pendingIdleHandlerCount保证了空闲时只会调用一次idler.queueIdle()
    int pendingIdleHandlerCount = -1; // -1 only during first iteration
    int nextPollTimeoutMillis = 0;
    for (;;) {
        if (nextPollTimeoutMillis != 0) {
            Binder.flushPendingCommands();
        }

        nativePollOnce(ptr, nextPollTimeoutMillis);

        synchronized (this) {
            // If first time idle, then get the number of idlers to run.
            // Idle handles only run if the queue is empty or if the first message
            // in the queue (possibly a barrier) is due to be handled in the future.
            //mMessages == null || now < mMessages.when：表示队列中没有消息或者需要执行的第一个Message还未到时间
            if (pendingIdleHandlerCount < 0
                    && (mMessages == null || now < mMessages.when)) {
                pendingIdleHandlerCount = mIdleHandlers.size();
            }
            if (pendingIdleHandlerCount <= 0) {
                // No idle handlers to run.  Loop and wait some more.
                mBlocked = true;
                continue;
            }

            if (mPendingIdleHandlers == null) {
                mPendingIdleHandlers = new IdleHandler[Math.max(pendingIdleHandlerCount, 4)];
            }
            mPendingIdleHandlers = mIdleHandlers.toArray(mPendingIdleHandlers);
        }

        // Run the idle handlers.
        // We only ever reach this code block during the first iteration.
        //遍历mPendingIdleHandlers
        for (int i = 0; i < pendingIdleHandlerCount; i++) {
            final IdleHandler idler = mPendingIdleHandlers[i];
            mPendingIdleHandlers[i] = null; // release the reference to the handler

            boolean keep = false;
            try {
                //回调queueIdle
                keep = idler.queueIdle();
            } catch (Throwable t) {
                Log.wtf(TAG, "IdleHandler threw exception", t);
            }
            //如果queueIdle返回false，则从队列中移除，后续当线程空闲时就不会回调了
            if (!keep) {
                synchronized (this) {
                    mIdleHandlers.remove(idler);
                }
            }
        }

        nextPollTimeoutMillis = 0;
    }
}
```

## IdleHandler应用场景

在应用启动时我们可能希望把一些优先级没那么高的操作延迟一点处理，一般会使用 Handler.postDelayed(Runnable r, long delayMillis)来实现，但是又不知道该延迟多少时间比较合适，因为手机性能不同，有的性能较差可能需要延迟较多，有的性能较好可以允许较少的延迟时间。所以在做项目性能优化的时候可以使用 IdleHandler，它在主线程空闲时执行任务，而不影响其他任务的执行。