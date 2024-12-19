###### 本片文章的主要内容如下：

-   1、消息的取出
-   2、消息(Message)的移除
-   3、关闭[消息队列](https://cloud.tencent.com/product/message-queue-catalog?from_column=20065&from=20065)
-   4、查看消息是否存在
-   5、阻塞非安全执行

### 一、消息的取出

##### (一)、消息的取出主要是通过Looper的loop方法

代码如下[Looper.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FLooper.java&objectId=1199418&objectType=1&isNewArticle=undefined) 122行

```
  /**
     * Run the message queue in this thread. Be sure to call
     * {@link #quit()} to end the loop.
     */
    public static void loop() {
         //第1步
        final Looper me = myLooper();
        if (me == null) {
            throw new RuntimeException("No Looper; Looper.prepare() wasn't called on this thread.");
        }
        //第2步
        final MessageQueue queue = me.mQueue;

        // Make sure the identity of this thread is that of the local process,
        // and keep track of what that identity token actually is.
        Binder.clearCallingIdentity();
        final long ident = Binder.clearCallingIdentity();

         //第3步
        for (;;) {
            //第四步
            Message msg = queue.next(); // might block
            if (msg == null) {
                // No message indicates that the message queue is quitting.
                return;
            }

            // This must be in a local variable, in case a UI event sets the logger
            final Printer logging = me.mLogging;
            if (logging != null) {
                logging.println(">>>>> Dispatching to " + msg.target + " " +
                        msg.callback + ": " + msg.what);
            }

            final long traceTag = me.mTraceTag;
            if (traceTag != 0 && Trace.isTagEnabled(traceTag)) {
                Trace.traceBegin(traceTag, msg.target.getTraceName(msg));
            }
            try {
                // 第5步
                msg.target.dispatchMessage(msg);
            } finally {
                if (traceTag != 0) {
                    Trace.traceEnd(traceTag);
                }
            }

            if (logging != null) {
                logging.println("<<<<< Finished to " + msg.target + " " + msg.callback);
            }

            // Make sure that during the course of dispatching the
            // identity of the thread wasn't corrupted.
            final long newIdent = Binder.clearCallingIdentity();
            if (ident != newIdent) {
                Log.wtf(TAG, "Thread identity changed from 0x"
                        + Long.toHexString(ident) + " to 0x"
                        + Long.toHexString(newIdent) + " while dispatching to "
                        + msg.target.getClass().getName() + " "
                        + msg.callback + " what=" + msg.what);
            }
            // 第6步
            msg.recycleUnchecked();
        }
    }
```

这个方法已经在[Android Handler机制4之Looper与Handler简介](https://cloud.tencent.com/developer/article/1199313?from_column=20421&from=20421)中说过了，我就重点说下流程，大体上分为6步

-   **第1步** 获取Looper对象
-   **第2步** 获取MessageQueue消息队列对象
-   **第3步** while()死循环遍历
-   **第4步** 通过queue.next()来从MessageQueue的消息队列中获取一个Message msg对象
-   **第5步** 通过msg.target. dispatchMessage(msg)来处理消息
-   **第6步** 通过msg.recycleUnchecked()方来回收Message到消息对象池中

由于**第1步**、**第2步**和**第3步**比较简单就不讲解了，而**第6步**在y已经讲解过，也不讲解了，下面我们来重点说下**第4步**和**第5步**

##### (二)、Message next()方法

> 从消息队列中提取Message交给Looper来处，这个步骤应该是MessageQueue乃至整个线程消息机制的核心了，所以我们将这部分放到最后来将，因为其内部的代码逻辑比较复杂，涉及到了障栅如何拦截同步消息、如何阻塞线程、如何在空闲的时候执行IdleHandler以及如何关闭Looper等内容，在源码已经做了详细的注释，不过由于逻辑比较复杂所以想要看明白，大家还要花费一定时间的。

PS:在Looper.loop()中获取消息的方式就是调用**next()**方法。

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199418&objectType=1&isNewArticle=undefined)

307行

```
    Message next() {
        // Return here if the message loop has already quit and been disposed.
        // This can happen if the application tries to restart a looper after quit
        // which is not supported.
        // 如果消息循环已经退出了。则直接在这里return。因为调用disposed()方法后mPtr=0
        final long ptr = mPtr;
        if (ptr == 0) {
            return null;
        }
        //记录空闲时处理的IdlerHandler的数量
        int pendingIdleHandlerCount = -1; // -1 only during first iteration
        // native层用到的变量 ，如果消息尚未到达处理时间，则表示为距离该消息处理事件的总时长，
        // 表明Native Looper只需要block到消息需要处理的时间就行了。 所以nextPollTimeoutMillis>0表示还有消息待处理
        int nextPollTimeoutMillis = 0;
        for (;;) {
            if (nextPollTimeoutMillis != 0) {
                //刷新下Binder命令，一般在阻塞前调用
                Binder.flushPendingCommands();
            }
            // 调用native层进行消息标示，nextPollTimeoutMillis 为0立即返回，为-1则阻塞等待。
            nativePollOnce(ptr, nextPollTimeoutMillis);
            //加上同步锁
            synchronized (this) {
                // Try to retrieve the next message.  Return if found.
                // 获取开机到现在的时间
                final long now = SystemClock.uptimeMillis();
                Message prevMsg = null;
                // 获取MessageQueue的链表表头的第一个元素
                Message msg = mMessages;
                 // 判断Message是否是障栅，如果是则执行循环，拦截所有同步消息，直到取到第一个异步消息为止
                if (msg != null && msg.target == null) {
                     // 如果能进入这个if，则表面MessageQueue的第一个元素就是障栅(barrier)
                    // Stalled by a barrier.  Find the next asynchronous message in the queue.
                    // 循环遍历出第一个异步消息，这段代码可以看出障栅会拦截所有同步消息
                    do {
                        prevMsg = msg;
                        msg = msg.next;
                       //如果msg==null或者msg是异步消息则退出循环，msg==null则意味着已经循环结束
                    } while (msg != null && !msg.isAsynchronous());
                }
                 // 判断是否有可执行的Message
                if (msg != null) {  
                    // 判断该Mesage是否到了被执行的时间。
                    if (now < msg.when) {
                        // Next message is not ready.  Set a timeout to wake up when it is ready.
                        // 当Message还没有到被执行时间的时候，记录下一次要执行的Message的时间点
                        nextPollTimeoutMillis = (int) Math.min(msg.when - now, Integer.MAX_VALUE);
                    } else {
                        // Message的被执行时间已到
                        // Got a message.
                        // 从队列中取出该Message，并重新构建原来队列的链接
                        // 刺客说明说有消息，所以不能阻塞
                        mBlocked = false;
                        // 如果还有上一个元素
                        if (prevMsg != null) {
                            //上一个元素的next(越过自己)直接指向下一个元素
                            prevMsg.next = msg.next;
                        } else {
                           //如果没有上一个元素，则说明是消息队列中的头元素，直接让第二个元素变成头元素
                            mMessages = msg.next;
                        }
                        // 因为要取出msg，所以msg的next不能指向链表的任何元素，所以next要置为null
                        msg.next = null;
                        if (DEBUG) Log.v(TAG, "Returning message: " + msg);
                        // 标记该Message为正处于使用状态，然后返回Message
                        msg.markInUse();
                        return msg;
                    }
                } else {
                    // No more messages.
                    // 没有任何可执行的Message，重置时间
                    nextPollTimeoutMillis = -1;
                }

                // Process the quit message now that all pending messages have been handled.
                // 关闭消息队列，返回null，通知Looper停止循环
                if (mQuitting) {
                    dispose();
                    return null;
                }

                // If first time idle, then get the number of idlers to run.
                // Idle handles only run if the queue is empty or if the first message
                // in the queue (possibly a barrier) is due to be handled in the future.
                // 当第一次循环的时候才会在空闲的时候去执行IdleHandler，从代码可以看出所谓的空闲状态
                // 指的就是当队列中没有任何可执行的Message，这里的可执行有两要求，
                // 即该Message不会被障栅拦截，且Message.when到达了执行时间点
                if (pendingIdleHandlerCount < 0
                        && (mMessages == null || now < mMessages.when)) {
                    pendingIdleHandlerCount = mIdleHandlers.size();
                }
                
                // 这里是消息队列阻塞( 死循环) 的重点，消息队列在阻塞的标示是消息队列中没有任何消息，
                // 并且所有的 IdleHandler 都已经执行过一次了
                if (pendingIdleHandlerCount <= 0) {
                    // No idle handlers to run.  Loop and wait some more.
                    mBlocked = true;
                    continue;
                }
    
                // 初始化要被执行的IdleHandler，最少4个
                if (mPendingIdleHandlers == null) {
                    mPendingIdleHandlers = new IdleHandler[Math.max(pendingIdleHandlerCount, 4)];
                }
                mPendingIdleHandlers = mIdleHandlers.toArray(mPendingIdleHandlers);
            }

            // Run the idle handlers.
            // We only ever reach this code block during the first iteration.
            // 开始循环执行所有的IdleHandler，并且根据返回值判断是否保留IdleHandler
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
            // 重点代码，IdleHandler只会在消息队列阻塞之前执行一次，执行之后改标示设置为0，
            // 之后就不会再执行，一直到下一次调用MessageQueue.next() 方法。
            pendingIdleHandlerCount = 0;

            // While calling an idle handler, a new message could have been delivered
            // so go back and look again for a pending message without waiting.
            // 当执行了IdleHandler 的 处理之后，会消耗一段时间，这时候消息队列里的可能有消息已经到达 
             // 可执行时间，所以重置该变量回去重新检查消息队列。
            nextPollTimeoutMillis = 0;
        }
    }
```

总的来说当我们试图产品从MessageQueue中获取一个Message的时候，会分为以下几步

-   **首先**、MessageQueue会先判断队列中是否有障栅的存在，如果有的话，只会返回异步消息，否则就逐个返回。
-   **其次**、当MessageQueue没有任何消息可以处理的时候，它会进度阻塞状态等待新的消息到来(无线循环)，在阻塞之前它会执行以便 IdleHandler，所谓的阻塞其实就是不断的循环查看是否有新的消息进入队列中。
-   **再次**、当MessageQueue被关闭的时候，其成员变量mQuitting会被标记为true，然后在Looper视图从队列中取出Message的时候返回null，而Message==null就是告诉Looper消息队列已经关闭，应该停止循环了，这一点可以在Looper.loop()房源中看出。
-   **最后**、如果大家细心一定会发现，Handler线程里面实际上有两个无线循环体，Looper循环体和MessageQueue循环体，真正阻塞的地方是MessageQueue的next()方法里。

这里有个难点，我简单说下

```
//****************  第一部分  ***************
                if (msg != null && msg.target == null) {
                    // Stalled by a barrier.  Find the next asynchronous message in the queue.
                    do {
                        prevMsg = msg;
                        msg = msg.next;
                    } while (msg != null && !msg.isAsynchronous());
                }

//============分割线==============


//****************  第二部分  ***************
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
```

-   第一种情况：**第一部分**主要是判断链表的第一个元素是否是障栅，如果是障栅，则进入if，内部区域，然后进行while循环，如果在链表中有一个元素是异步的，则跳出循环，然后进入**第二部分**，其中**第二部分**就是取出这个异步消息
-   第二种情况：没进入进入**第一部分**的if，则说明头部元素不是障栅(barrier)，则直接进入**第二部分**，这时候取出的就是当前的头部元素。

PS:

> nativePollOnce是阻塞操作，其中nextPollTimeoutMillis代表下一个消息到来前，需要等待的时长，当nextPollTimeoutMillis=-1时，表示消息队列无消息，会一直等待下去。nativePollOnce()是在native做了大量的工作。

##### (三)、msg.target.dispatchMessage(msg);方法

我们知道这个方法其实是Handler的dispatchMessage(Message)方法，那我们就来详细看下

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199418&objectType=1&isNewArticle=undefined) 93行

```
    /**
     * Handle system messages here.
     */
    public void dispatchMessage(Message msg) {
        if (msg.callback != null) {
            //当Message存在回调方法，回调msg.callback.run()方法；
            handleCallback(msg);
        } else {
            if (mCallback != null) {
                //当Handler存在Callback成员变量时，回调方法handleMessage()；
                if (mCallback.handleMessage(msg)) {
                    return;
                }
            }
            //Handler自身的回调方法handleMessage()
            handleMessage(msg);
        }
    }
```

这个方法很简单就是二个条件，三种情况

-   情况1：如果msg.callback 不为空，则执行handleCallback(Message)，而handleCallback(Message)的内部最终调用的是message.callback.run();，所以最终是msg.callback.run()。
-   情况2：如果msg.callback 为空，且mCallback不为空，则执行mCallback.handleMessage(msg)。
-   情况3：如果msg.callback 为空，且mCallback也为空，则执行handleMessage()方法

这里我们可以看到，在分发消息时三个方法的优先级分别如下：

-   Message的回调方法优先级最高，即message.callback.run()；
-   Handler的回调方法优先级次之，即Handler.mCallback.handleMessage(msg)；
-   Handler的默认方法优先级最低，即Handler.handleMessage(msg)。

对于很多情况下，消息分发后的处理情况是第3种情况，即Handler.handleMessage()，一般地往往是通过覆写该方法从而实现自己的业务逻辑。

### 二、消息(Message)的移除

##### (一) Handler的消息移除

> 消息(Message)的移除，其实就是根据身份what、消息Runnable或msg.obj移除队列中对应的消息。例如发送msg，用同一个msg.what作为参数。所有方法最终调用MessageQueue.removeMessages，来进行时机操作的。

代码如下，因为不复杂，我就合并在一起了

```
// Handler.java
public final void removeCallbacks(Runnable r) {
    mQueue.removeMessages(this, r, null);
}

public final void removeCallbacks(Runnable r, Object token) {
    mQueue.removeMessages(this, r, token);
}

public final void removeMessages(int what) {
    mQueue.removeMessages(this, what, null);
}

public final void removeMessages(int what, Object object) {
    mQueue.removeMessages(this, what, object);
}

public final void removeCallbacksAndMessages(Object token) {
    mQueue.removeCallbacksAndMessages(this, token);
}
```

所以我们知道，Handler里面的删除工作，其实本地都是调用MessageQueue来操作的。

下面我们就来看下MessageQueue是怎么操作的？

##### (二) MessageQueue的消息移除

MessageQueue的消息移除在其类类的方法如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/dlc4nlnfgi.png)

MessageQueue的消息移除.png

一共有5个方法如下：

-   移除方法1：void removeMessages(Handler , int , Object )
-   移除方法2：void removeMessages(Handler, Runnable,Object)
-   移除方法3：void removeCallbacksAndMessages(Handler, Object)
-   移除方法4：void removeAllMessagesLocked()
-   移除方法5：void removeAllFutureMessagesLocked()

那我们就依次讲解下:

###### 移除方法1：void removeMessages(Handler , int , Object )方法

> 从消息队列中删除所有符合指定条件的Message

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199418&objectType=1&isNewArticle=undefined) 587行

```
  void removeMessages(Handler h, int what, Object object) {
        // 第1步
        if (h == null) {
            return;
        }
         // 第2步
        synchronized (this) {
           // 第3步
            Message p = mMessages;

            // Remove all messages at front.

          
            //第4步
            while (p != null && p.target == h && p.what == what
                   && (object == null || p.obj == object)) {
                Message n = p.next;
                mMessages = n;
                p.recycleUnchecked();
                p = n;
            }

            // Remove all messages after front.
            //第5步
            while (p != null) {
                Message n = p.next;
                if (n != null) {
                    if (n.target == h && n.what == what
                        && (object == null || n.obj == object)) {
                        Message nn = n.next;
                        n.recycleUnchecked();
                        p.next = nn;
                        continue;
                    }
                }
                p = n;
            }
        }
    }
```

上面的代码大体可以分为5个步骤如下：

-   **第1步、**，对传递进来的Handler做非空判断，如果传递进来的Handler为空，则直接返回
-   **第2步、**，加同步锁
-   **第3步、**，获取消息队列链表的头元素
-   **第4步、**，如果从消息队列的头部就有符合删除条件的Message，就从头开始遍历删除所有符合条件的Message,并不端更新mMessages指向的Message。
-   **第5步、**，因为有了**第4步、**，前面的的情况不会发生，也就是我们不需要关心指向的问题，现在处理的问题就是删除剩下的符合删除条件的Message。

总结一下：

> 从消息队列中删除Message的操作也是遍历消息队列然后删除所有符合条件的Message，但是这里有连个小细节需要注意，从代码中可以看出删除Message分为两次操作，第一次是先判断符合删除条件的Message是不是从消息队列的头部就开始有了，这时候会设计修改mMessage指向的问题，而mMessage代表的就是整个消息队列，在排除了第一种情况之后，剩下的就是继续遍历队列删除剩余的符合删除条件的Message。其他重载方法也是同样的操作，唯一条件就是条件不同而已，

###### 移除方法2：void removeMessages(Handler, Runnable,Object)方法

> 从消息队列中删除所有符合指定条件的Message

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199418&objectType=1&isNewArticle=undefined) 604行

```
    void removeMessages(Handler h, Runnable r, Object object) {
        if (h == null || r == null) {
            return;
        }

        synchronized (this) {
            Message p = mMessages;

            // Remove all messages at front.
            while (p != null && p.target == h && p.callback == r
                   && (object == null || p.obj == object)) {
                Message n = p.next;
                mMessages = n;
                p.recycleUnchecked();
                p = n;
            }

            // Remove all messages after front.
            while (p != null) {
                Message n = p.next;
                if (n != null) {
                    if (n.target == h && n.callback == r
                        && (object == null || n.obj == object)) {
                        Message nn = n.next;
                        n.recycleUnchecked();
                        p.next = nn;
                        continue;
                    }
                }
                p = n;
            }
        }
    }
```

里面代码和移除方法1：void removeMessages(Handler , int , Object )基本一致，唯一不同就是筛选条件不同而已。

###### 移除方法3：void removeMessages(Handler, Runnable,Object)方法

> 从消息队列中删除所有符合指定条件的Message

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199418&objectType=1&isNewArticle=undefined) 689行

```
    void removeCallbacksAndMessages(Handler h, Object object) {
        if (h == null) {
            return;
        }

        synchronized (this) {
            Message p = mMessages;

            // Remove all messages at front.
            while (p != null && p.target == h
                    && (object == null || p.obj == object)) {
                Message n = p.next;
                mMessages = n;
                p.recycleUnchecked();
                p = n;
            }

            // Remove all messages after front.
            while (p != null) {
                Message n = p.next;
                if (n != null) {
                    if (n.target == h && (object == null || n.obj == object)) {
                        Message nn = n.next;
                        n.recycleUnchecked();
                        p.next = nn;
                        continue;
                    }
                }
                p = n;
            }
        }
    }
```

里面代码和移除方法1：void removeMessages(Handler , int , Object )基本一致，唯一不同就是筛选条件不同而已。

###### 移除方法4：void removeAllMessagesLocked()方法

> 删除所有的消息

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199418&objectType=1&isNewArticle=undefined) 722行

```
   private void removeAllMessagesLocked() {
        Message p = mMessages;
        while (p != null) {
            Message n = p.next;
            p.recycleUnchecked();
            p = n;
        }
        mMessages = null;
    }
```

这个方法很简单，就是删除所有的消息

###### 移除方法5：void removeAllFutureMessagesLocked()

> 删除所有未来消息

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199418&objectType=1&isNewArticle=undefined) 732行

```
    private void removeAllFutureMessagesLocked() {
         // 第1步
        final long now = SystemClock.uptimeMillis();
         // 第2步
        Message p = mMessages;
        if (p != null) {
            // 第3步
            if (p.when > now) {
                removeAllMessagesLocked();
            } else {
                // 第4步
                Message n;
                for (;;) {
                    n = p.next;
                    if (n == null) {
                        return;
                    }
                    if (n.when > now) {
                        break;
                    }
                    p = n;
                }
               // 第5步
                p.next = null;
                do {
                    p = n;
                    n = p.next;
                    p.recycleUnchecked();
                } while (n != null);
            }
        }
    }
```

这个方法大体上分为5个步骤，具体解释如下：

-   第1步：获取当前时间(其实从手机开机到现在的时间)
-   第2步：获取消息队列链表的的头元素
-   第3步：如果头元素的执行的时间就大于当前时间，因为我们知道链表的排序其实有从当前到未来的顺序排列的，所以但如果头元素大于当前时间，意味着这个链表的所有元素的执行时间都大于当前，则删除链表中的全部元素。
-   第4步：如果消息队列中的头元素小于或等于当前时间，则说明要从消息队列中截取，从中间的某个未知的位置截取到消息队列链表的队尾。这个时候就需要找到这个具体的位置，这个步骤主要就是做这个事情。通过对比时间，找到合适的位置
-   第5步：找到合适的位置后，就开始删除这个位置到消息队列队尾的所有元素

### 三、关闭消息队列

> 通过前面的文章，我们知道Handler消息机制的停止，本质上是停止Looper的循环，在[Android Handler机制4之Looper与Handler简介](https://cloud.tencent.com/developer/article/1199313?from_column=20421&from=20421)文章中我们知道Looper的停止实际上是关闭消息队列的关闭，现在我们来揭示MessageQueue是如何关闭的

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199418&objectType=1&isNewArticle=undefined) 413行

```
    void quit(boolean safe) {
         // 第1步
        if (!mQuitAllowed) {
            throw new IllegalStateException("Main thread not allowed to quit.");
        }
        // 第2步
        synchronized (this) {
            // 第3步
            if (mQuitting) {
                return;
            }
            mQuitting = true;
            // 第4步
            if (safe) {
                removeAllFutureMessagesLocked();
            } else {
                removeAllMessagesLocked();
            }

            // We can assume mPtr != 0 because mQuitting was previously false.
            // 第5步
            nativeWake(mPtr);
        }
    }
```

这个方法内部大概分为5个步骤

-   第1步：判断是否允许退出，因为在构造MessageQueue对象的时候传入了一个boolean参数，来表示该MessageQueue是否允许退出。而这个boolean参数在Looper里面设置，Loooper.prepare()方法里面是true，在Looper.prepareMainLooper()是false，由此可见我们知道：**主线程的MessageQueue是不能退出**。其他工作线程的MessageQueue是可以退出的。
-   第2步：加上同步锁
-   第3步：主要防止重复退出，加入一个mQuitting变量表示是否退出
-   第4步：如果该方法的变量safe为true，则删除以当前时间为分界线，删除未来的所有消息，如果该方法的变量safe为false，则删除当前消息队列的所有消息。
-   第5步：删除小时后nativeWake函数，以触发nativePollOnce函数，结束等待，这个块内容请在Android Handler机制9之Native的实现中，这里就不详细描述了

### 四、查看消息是否存在

Handler机制也存在查找是否存在某条消息的机制，代码如下：

```
// Handler.java
public final boolean hasMessages(int what) {
    return mQueue.hasMessages(this, what, null);
}

public final boolean hasMessages(int what, Object object) {
    return mQueue.hasMessages(this, what, object);
}

public final boolean hasCallbacks(Runnable r) {
    return mQueue.hasMessages(this, r, null);
}
```

我们发现其内部都是调用MessageQueue的hasMessages函数，那我们就来看下

##### (一) boolean hasMessages(Handler h, int what, Object object) 方法

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199418&objectType=1&isNewArticle=undefined) 587行

```
    boolean hasMessages(Handler h, int what, Object object) {
        //第1步
        if (h == null) {
            return false;
        }
        //第2步
        synchronized (this) {
            //第3步
            Message p = mMessages;
            //第4步
            while (p != null) {
                if (p.target == h && p.what == what && (object == null || p.obj == object)) {
                    return true;
                }
                p = p.next;
            }
            return false;
        }
    }
```

该方法的主要内容可以分为4个步骤

-   第1步：判断传入进来的Handler是否为空，如果传入的Handler为空，直接返回false，表示没有找到
-   第2步：加上同步锁
-   第3步：取出消息队列链表中的头部元素
-   第4步：遍历消息队里链表中的所有元素，如果有元素消息符合指定条件则return false，如果遍历完毕还没有则返回false

boolean hasMessages(Handler h, Runnable r, Object object)方法和本方法基本一致，唯一不同就是筛选条件不同而已。我就说讲解了。

### 五、阻塞非安全执行

> 如果当前执行线程是Handler的线程，Runnable会被立刻执行。否则把它放在消息队列中一直等待执行完毕或者超时，超时后这个任务还在队列中，在后面的某个时刻它仍然会执行，很有可能造成死锁，所以尽量不要用它。

这个方法使用场景是Android初始化一个WindowManagerService，应为WindowManagerService不成功，其他组件就不允许继续，所以使用阻塞的方式直到完成。

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199418&objectType=1&isNewArticle=undefined) 461行

```
    /**
     * Runs the specified task synchronously.
     * <p>
     * If the current thread is the same as the handler thread, then the runnable
     * runs immediately without being enqueued.  Otherwise, posts the runnable
     * to the handler and waits for it to complete before returning.
     * </p><p>
     * This method is dangerous!  Improper use can result in deadlocks.
     * Never call this method while any locks are held or use it in a
     * possibly re-entrant manner.
     * </p><p>
     * This method is occasionally useful in situations where a background thread
     * must synchronously await completion of a task that must run on the
     * handler's thread.  However, this problem is often a symptom of bad design.
     * Consider improving the design (if possible) before resorting to this method.
     * </p><p>
     * One example of where you might want to use this method is when you just
     * set up a Handler thread and need to perform some initialization steps on
     * it before continuing execution.
     * </p><p>
     * If timeout occurs then this method returns <code>false</code> but the runnable
     * will remain posted on the handler and may already be in progress or
     * complete at a later time.
     * </p><p>
     * When using this method, be sure to use {@link Looper#quitSafely} when
     * quitting the looper.  Otherwise {@link #runWithScissors} may hang indefinitely.
     * (TODO: We should fix this by making MessageQueue aware of blocking runnables.)
     * </p>
     *
     * @param r The Runnable that will be executed synchronously.
     * @param timeout The timeout in milliseconds, or 0 to wait indefinitely.
     *
     * @return Returns true if the Runnable was successfully executed.
     *         Returns false on failure, usually because the
     *         looper processing the message queue is exiting.
     *
     * @hide This method is prone to abuse and should probably not be in the API.
     * If we ever do make it part of the API, we might want to rename it to something
     * less funny like runUnsafe().
     */
    public final boolean runWithScissors(final Runnable r, long timeout) {
         // 第1步
        if (r == null) {
            throw new IllegalArgumentException("runnable must not be null");
        }
         // 第2步
        if (timeout < 0) {
            throw new IllegalArgumentException("timeout must be non-negative");
        }

          // 第3步
         // 如果为同一个线程，则直接执行runnable，而不需要加入到消息队列。
        if (Looper.myLooper() == mLooper) {
            r.run();
            return true;
        }
 
        // 第4步
        BlockingRunnable br = new BlockingRunnable(r);
        return br.postAndWait(this, timeout);
    }
```

首先先简单翻译一下注释：

-   同步运行指定的任务。
-   如果当前线程就是Handler的处理线程，则可以不用排队，直接运行这个runnable。否则如果当前线程和Handler的处理编程不是同一个线程则需要发送这个runnable到Handler线程，并且等待它完成后再返回。
-   使用这个方法是有风险的，使用不当可能会导致死锁。不要在有锁或者可能有锁的代码区域调用这个方法。
-   这个方法的使用场景通常是，一个后台线程必须等待Handler线程中的一个任务的完成。但是，这往往是不优雅设计才会出现的问题。所以在使用这个方法的时候，请首先考虑改进设计方案。
-   这个方法的使用场景是：在你建立Handler线程之前，你需要执行一些初始化操作。
-   如果发生超时，虽然该方法还是会返回false，但是该 如果超时发生，那么该方法返回<code> false </ code>，但是runnable仍是会保留在Handler中，并且在一段时间以后会在被执行。
-   在使用这个方法的时候，并且要退出一个Looper的时候，请一定要调用quitSafely()这个方法。否则runWithScissors()这个方法可能会无限期挂起。(TODO：我们应该通知MessageQueue去阻止runnable来解决这个问题)

该方法内部的执行流程主要分为4个步骤，如下：

-   **第1步、**：Runnable非空判断
-   **第2步、**：timeout是否小于0判断
-   **第3步、**：如果Looper的线程和Handler的线程是同一个线程
-   **第4步、**，构造一个BlockingRunnable对象，并调用该对象的postAndWait(Handler,long)方法

上面涉及到一个咱们之前没有讲解过的类：BlockingRunnable，他是Handler的静态内部类，我们来研究下

##### (一)、Handler的静态内部类BlockingRunnable

BlockingRunnable是Handler的一个私有内部静态类，利用Object的wait和notifyAll方法实现。

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199418&objectType=1&isNewArticle=undefined)

```
  private static final class BlockingRunnable implements Runnable {
        private final Runnable mTask;
        private boolean mDone;

        public BlockingRunnable(Runnable task) {
            mTask = task;
        }

        @Override
        public void run() {
            try {
                mTask.run();
            } finally {
                synchronized (this) {
                    mDone = true;
                     // runnable 执行完之后，会通知wait的线程不再wait
                    notifyAll();
                }
            }
        }

        public boolean postAndWait(Handler handler, long timeout) {
            if (!handler.post(this)) {
                return false;
            }

            synchronized (this) {
                if (timeout > 0) {
                    final long expirationTime = SystemClock.uptimeMillis() + timeout;
                    while (!mDone) {
                        long delay = expirationTime - SystemClock.uptimeMillis();
                        if (delay <= 0) {
                            return false; // timeout
                        }
                       // post runnable 之后，将调用线程变为wait状态
                        try {
                            wait(delay);
                        } catch (InterruptedException ex) {
                        }
                    }
                } else {
                    while (!mDone) {
                         // post runnable 之后，将调用线程变为wait状态
                        try {
                            wait();
                        } catch (InterruptedException ex) {
                        }
                    }
                }
            }
            return true;
        }
    }
```

通过分析源码我们获取的了如下信息：

-   1、该类实现了Runnable接口
-   2、构造函数：接受一个Runnable作为参数的构造函数，包含了真正要执行的Task。
-   3、run函数很简单，直接调用mTask.run()，一个finally内会同步对象本身(因为mDone涉及到多线程，而notifyAll()则需要synchronized配合)
-   4、postAndWait(Handler, long)：首先尝试将BlockingRunnable自己post到handler上，如果post失败，则直接返回false；其次如果上一步的post成功，就需要同步对象本身(为了使用wait())；此时，如果timeout>0，那么就一个while循环+wait(long)，中间有任何的interrupt都直接catch重新结算wait的时间，只有在任务完成(mDone=true，另外线程的run函数会设置此值)或者任何超时才会返回(true/false)；如果imeout <=0，也就无限等待了

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2017.09.14 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除