---
created: 2025-06-27T10:21:36 (UTC +08:00)
tags: [Android中文技术社区,前端开发社区,前端技术交流,前端框架教程,JavaScript 学习资源,CSS 技巧与最佳实践,HTML5 最新动态,前端工程师职业发展,开源前端项目,前端技术趋势]
source: https://juejin.cn/post/7294924154720239643
author: 阿豪讲Framework
---

# IdleHanlder 原理与使用本文摘自写给应用开发的 Android Framework 教程，完整教程请查阅 ht - 掘金

> ## Excerpt
> 本文摘自写给应用开发的 Android Framework 教程，完整教程请查阅 https://yuandaimaahao.gitee.io/androidframeworktutorialpage

---
       ![](https://p3-piu.byteimg.com/tos-cn-i-8jisjyls3a/796c19f610c146ffac65db71d7329490~tplv-8jisjyls3a-2:0:0:q75.image)

本文摘自**写给应用开发的 Android Framework 教程**，完整教程请查阅

[yuandaimaahao.gitee.io/androidfram…](https://link.juejin.cn/?target=https%3A%2F%2Fyuandaimaahao.gitee.io%2Fandroidframeworktutorialpages%2F "https://yuandaimaahao.gitee.io/androidframeworktutorialpages/")

更为详细的视频教程与答疑服务，请联系微信 zzh0838

IdleHandler 是 Handler 机制提供的一种，在 Looper 事件循环的过程中，当出现空闲的时候，允许我们执行任务的一种机制。

## 如何使用 IdleHanlder

IdleHandler 被定义在 MessageQueue 中，它是一个接口：

```
public static interface IdleHandler {
    boolean queueIdle();
}
```

自定义 IdleHandler 时，需要实现其 queueIdle() 方法，并返回一个布尔值。

在 MessageQueue 中定义了对应的 add 和 remove 方法，用于添加和删除 IdleHandler：

```
// MessageQueue.java
public void addIdleHandler(@NonNull IdleHandler handler) {
// ...
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

当 IdleHandler 添加好以后，Looper 出现空闲时，就会执行 IdleHandler 中定义的回调方法了，接下来我们从源码中分析 IdleHandler 的执行过程。

## IdleHandler 源码分析

在 Message.next() 这个获取消息队列下一个待执行消息的方法中，会进行 IdleHandler 的处理：

```
 Message next() {
        
        final long ptr = mPtr;
        if (ptr == 0) {
            return null;
        }

        // 初始化为 -1
        int pendingIdleHandlerCount = -1; // -1 only during first iteration
        int nextPollTimeoutMillis = 0;
        for (;;) {
            if (nextPollTimeoutMillis != 0) {
                Binder.flushPendingCommands();
            }

            nativePollOnce(ptr, nextPollTimeoutMillis);

            synchronized (this) {
                // Try to retrieve the next message.  Return if found.
                final long now = SystemClock.uptimeMillis();
                Message prevMsg = null;
                Message msg = mMessages;

                // 获取 Message

                // 这里是屏障 Message 处理，暂时不管
                if (msg != null && msg.target == null) {
                    // Stalled by a barrier.  Find the next asynchronous message in the queue.
                    do {
                        prevMsg = msg;
                        msg = msg.next;
                    } while (msg != null && !msg.isAsynchronous());
                }

                if (msg != null) {
                    if (now < msg.when) { // message 的触发时间未到
                        
                        nextPollTimeoutMillis = (int) Math.min(msg.when - now, Integer.MAX_VALUE);
                    } else {    //找到合适的 message 并返回
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
                } else { // 没有 Message 了
                    nextPollTimeoutMillis = -1;
                }


                // Looper 空闲
                // 情况1 Message 的触发时间未到
                // 情况2 没有 Message 了
               
                // 开始处理 IdleHandler
                // 第一次 for 循环 pendingIdleHandlerCount 等于 -1
                if (pendingIdleHandlerCount < 0
                        && (mMessages == null || now < mMessages.when)) { // 进入 if
                    pendingIdleHandlerCount = mIdleHandlers.size(); 
                }
                if (pendingIdleHandlerCount <= 0) { // 不进入
                    // No idle handlers to run.  Loop and wait some more.
                    mBlocked = true;
                    continue;
                }

                // new 一个 IdleHandler 数组
                if (mPendingIdleHandlers == null) {
                    mPendingIdleHandlers = new IdleHandler[Math.max(pendingIdleHandlerCount, 4)];
                }
                
                // 把 mIdleHandlers 拷贝到 mPendingIdleHandlers 中
                mPendingIdleHandlers = mIdleHandlers.toArray(mPendingIdleHandlers);
            }


            // 循环依次处理 IdleHandler
            // Run the idle handlers.
            // We only ever reach this code block during the first iteration.
            for (int i = 0; i < pendingIdleHandlerCount; i++) {
                final IdleHandler idler = mPendingIdleHandlers[i];
                mPendingIdleHandlers[i] = null; // release the reference to the handler

                boolean keep = false;
                try {
                    keep = idler.queueIdle();
                } catch (Throwable t) {
                    Log.wtf(TAG, "IdleHandler threw exception", t);
                }

                if (!keep) {
                    synchronized (this) {
                        mIdleHandlers.remove(idler);
                    }
                }
            }

            // Reset the idle handler count to 0 so we do not run them again.
            pendingIdleHandlerCount = 0; //下一次 for 循环置为 0，下次循环就不会处理 IdleHandler 了

            // While calling an idle handler, a new message could have been delivered
            // so go back and look again for a pending message without waiting.
            nextPollTimeoutMillis = 0;
        }
    }
```

-   当 Message 的触发时间未到或者没有 Message 了，Looper 进入空闲状态，开始处理 IdleHandler
-   接着初始化一个 IdleHandler\[\] mPendingIdleHandlers 数组，并把 mIdleHandlers 中的 IdleHandler 拷贝到 mPendingIdleHandlers 数组中
-   接着在循环中从数组中取出 IdleHandler，并调用其 queueIdle() 记录返回值存到 keep 中； 当 keep 为 false 时，从 mIdleHandler 中移除当前循环的 IdleHandler，反之则保留；
-   最后 pendingIdleHandlerCount = 0，下一次 for 循环置为 0，下次循环就不会处理 IdleHandler 了

## IdleHandler 应用场景

在 ActivityThread.java 中有一个 H 类，继承自 Handler，当它收到 GC\_WHEN\_IDLE 消息后，会执行 scheduleGcIdler 方法：

```
void scheduleGcIdler() {
    if (!mGcIdlerScheduled) {
        mGcIdlerScheduled = true;
        //添加GC任务
        Looper.myQueue().addIdleHandler(mGcIdler);
    }
    mH.removeMessages(H.GC_WHEN_IDLE);
}
```

这里会添加一个 mGcIdler：

```
   final class GcIdler implements MessageQueue.IdleHandler {
        @Override
        public final boolean queueIdle() {
            doGcIfNeeded();
            //执行后，就直接删除
            return false;
        }
    }

    // 判断是否需要执行垃圾回收。
    void doGcIfNeeded() {
        mGcIdlerScheduled = false;
        final long now = SystemClock.uptimeMillis();
        //获取上次GC的时间
        if ((BinderInternal.getLastGcTime()+MIN_TIME_BETWEEN_GCS) < now) {
            //Slog.i(TAG, "**** WE DO, WE DO WANT TO GC!");
            BinderInternal.forceGc("bg");  //执行 GC 任务
        }
    }
```

GcIdler 方法理解起来很简单、就是获取上次 GC 的时间，判断是否需要 GC 操作。如果需要则进行 GC 操作。

何时会收到 GC\_WHEN\_IDLE 消息？当 AMS(ActivityManagerService) 中的这两个方法被调用之后：

-   doLowMemReportIfNeededLocked， 这个方法看名字就知道是不够内存的时候调用的了。
-   当 ActivityThread 的 handleResumeActivity 方法被调用时。

IdleHandler 其他使用场景：

-   Activity 启动优化（加快App启动速度）：onCreate，onStart，onResume中耗时较短但非必要的代码可以放到 IdleHandler 中执行，减少启动时间
-   想要在一个 View 绘制完成之后添加其他依赖于这个 View 的 View，当然这个用 View#post() 也能实现，区别就是前者会在消息队列空闲时执行
-   发送一个返回 true 的 IdleHandler，在里面让某个 View 不停闪烁，这样当用户发呆时就可以诱导用户点击这个 View，这也是种很酷的操作
-   一些第三方库中有使用，比如 LeakCanary，Glide 中有使用到，具体可以自行去查看

## 参考资料

-   [Android 避坑指南：实际经历来说说IdleHandler的坑](https://link.juejin.cn/?target=https%3A%2F%2Fmp.weixin.qq.com%2Fs%2Fdh_71i8J5ShpgxgWN5SPEw "https://mp.weixin.qq.com/s/dh_71i8J5ShpgxgWN5SPEw")
-   [IdleHandler基本使用及应用案例分析](https://juejin.cn/post/7089946584057659399 "https://juejin.cn/post/7089946584057659399")
-   [临时抱佛脚：IdleHandler 的原理分析和妙用](https://link.juejin.cn/?target=https%3A%2F%2Fzhuanlan.zhihu.com%2Fp%2F345819916 "https://zhuanlan.zhihu.com/p/345819916")
-   [IdleHandler 是什么？怎么使用，能解决什么问题？](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fjdsjlzx%2Farticle%2Fdetails%2F110532500 "https://blog.csdn.net/jdsjlzx/article/details/110532500")

![avatar](https://p9-passport.byteacctimg.com/img/user-avatar/e4025d31a2e5f38cf89f17f38e7eec87~40x40.awebp)
