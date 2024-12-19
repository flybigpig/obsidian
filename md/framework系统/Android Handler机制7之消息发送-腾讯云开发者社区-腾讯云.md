###### 本片文章的主要内容如下：

-   1、Handler发送消息
-   2、Handler的send方案
-   3、Handler的post方案

### 一 、Handler发送消息

大家平时发送消息主要是调用的两大类方法 如下两图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/ms07gbhclb.png)

send方案.png

![](https://ask.qcloudimg.com/http-save/yehe-2957818/oeg7w3fp65.png)

send方案.png

光看上面这些API你可能会觉得handler能法两种消息，一种是Runnable对象，一种是message对象，这是直观的理解，但其实post发出的Runnable对象最后都封装成message对象了。

-   send方案发送消息(需要回调才能接收消息)
    -   1、sendMessage(Message) 立即发送Message到[消息队列](https://cloud.tencent.com/product/message-queue-catalog?from_column=20065&from=20065)
    -   2、sendMessageAtFrontOfQueue(Message) 立即发送Message到队列，而且是放在队列的最前面
    -   3、sendMessageAtTime(Message,long) 设置时间，发送Message到队列
    -   4、sendMessageDelayed(Message,long) 延时若干毫秒后，发送Message到队列
-   post方案 立即发送Message到消息队列
    -   1、post(Runnable) 立即发送Message到消息队列
    -   2、postAtFrontOfQueue(Runnable) 立即发送Message到队列，而且是放在队列的最前面
    -   3、postAtTime(Runnable,long) 设置时间，发送Message到队列
    -   4、postDelayed(Runnable,long) 在延时若干毫秒后，发送Message到队列

下面我们就先从send方案中的第一个sendMessage() 开始源码跟踪下：

### 二、 Handler的send方案

我们以Handler的sendMessage(Message msg)为例子。

##### (一)、boolean sendMessage(Message msg)方法

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 505行

```
    /**
     * Pushes a message onto the end of the message queue after all pending messages
     * before the current time. It will be received in {@link #handleMessage},
     * in the thread attached to this handler.
     *  
     * @return Returns true if the message was successfully placed in to the 
     *         message queue.  Returns false on failure, usually because the
     *         looper processing the message queue is exiting.
     */
    public final boolean sendMessage(Message msg)
    {
        return sendMessageDelayed(msg, 0);
    }
```

老规矩先翻译一下注释：

> 在当前时间，在所有待处理消息之后，将消息推送到消息队列的末尾。在和当前线程关联的的Handler里面的handleMessage将收到这条消息，

我们看到sendMessage(Message)里面代码很简单，就是调用了sendMessageDelayed(msg,0)

###### 1、boolean sendMessageDelayed(Message msg, long delayMillis)

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 565行

```
    /**
     * Enqueue a message into the message queue after all pending messages
     * before (current time + delayMillis). You will receive it in
     * {@link #handleMessage}, in the thread attached to this handler.
     *  
     * @return Returns true if the message was successfully placed in to the 
     *         message queue.  Returns false on failure, usually because the
     *         looper processing the message queue is exiting.  Note that a
     *         result of true does not mean the message will be processed -- if
     *         the looper is quit before the delivery time of the message
     *         occurs then the message will be dropped.
     */
    public final boolean sendMessageDelayed(Message msg, long delayMillis)
    {
        if (delayMillis < 0) {
            delayMillis = 0;
        }
        return sendMessageAtTime(msg, SystemClock.uptimeMillis() + delayMillis);
    }
```

注释和boolean sendMessage(Message msg)方法差不多，我就不翻译了

> 该方法内部就做了两件事

-   1、判断delayMillis是否小于0
-   2、调用了public boolean sendMessageAtTime(Message msg, long uptimeMillis)方法

###### 2、boolean sendMessageAtTime(Message msg, long uptimeMillis)

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 592行

```
    /**
     * Enqueue a message into the message queue after all pending messages
     * before the absolute time (in milliseconds) <var>uptimeMillis</var>.
     * <b>The time-base is {@link android.os.SystemClock#uptimeMillis}.</b>
     * Time spent in deep sleep will add an additional delay to execution.
     * You will receive it in {@link #handleMessage}, in the thread attached
     * to this handler.
     * 
     * @param uptimeMillis The absolute time at which the message should be
     *         delivered, using the
     *         {@link android.os.SystemClock#uptimeMillis} time-base.
     *         
     * @return Returns true if the message was successfully placed in to the 
     *         message queue.  Returns false on failure, usually because the
     *         looper processing the message queue is exiting.  Note that a
     *         result of true does not mean the message will be processed -- if
     *         the looper is quit before the delivery time of the message
     *         occurs then the message will be dropped.
     */
    public boolean sendMessageAtTime(Message msg, long uptimeMillis) {
        MessageQueue queue = mQueue;
        if (queue == null) {
            RuntimeException e = new RuntimeException(
                    this + " sendMessageAtTime() called with no mQueue");
            Log.w("Looper", e.getMessage(), e);
            return false;
        }
        return enqueueMessage(queue, msg, uptimeMillis);
    }
```

老规矩先翻译一下注释：

> 以android系统的SystemClock的uptimeMillis()为基准，以毫秒为基本单位的绝对时间下，在所有待处理消息后，将消息放到消息队列中。深度睡眠中的时间将会延迟执行的时间，你将在和当前线程办的规定的Handler中的handleMessage中收到该消息。

这里顺便提一下异步的作用，因为通常我们理解的异步是指新开一个线程，但是这里不是，因为异步的也是发送到looper所绑定的消息队列中，这里的异步主要是针对Message中的障栅(Barrier)而言的，当出现障栅(Barrier)的时候，同步的会被阻塞，而异步的则不会。所以这个异步仅仅是一个标记而已。

> 该方法内部就做了两件事

-   1、获取消息队列，并对该消息队列做非空判断，如果为null，直接返回false，表示消息发送失败
-   2、调用了boolean enqueueMessage(MessageQueue queue, Message msg, long uptimeMillis)方法

###### 3、boolean enqueueMessage(MessageQueue queue, Message msg, long uptimeMillis)

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 626行

```
    private boolean enqueueMessage(MessageQueue queue, Message msg, long uptimeMillis) {
        msg.target = this;
        if (mAsynchronous) {
            msg.setAsynchronous(true);
        }
        return queue.enqueueMessage(msg, uptimeMillis);
    }
```

本方法内部做了三件事

-   1、设置msg的target变量，并将target指向自己
-   2、如果Handler的mAsynchronous值为true(默认为false，即不设置)，则设置msg的flags值，让是否异步在Handler和Message达成统一。
-   3、调用MessageQueue的enqueueMessage()方法

###### 4、boolean enqueueMessage(Message msg, long when)方法

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199417&objectType=1&isNewArticle=undefined) 533行

```
    boolean enqueueMessage(Message msg, long when) {
        //第一步
        if (msg.target == null) {
            throw new IllegalArgumentException("Message must have a target.");
        }
        // 第二步
        if (msg.isInUse()) {
            throw new IllegalStateException(msg + " This message is already in use.");
        }
        // 第三步
        synchronized (this) {
             // 第四步
            //判断消息队列是否正在关闭
            if (mQuitting) {
                IllegalStateException e = new IllegalStateException(
                        msg.target + " sending message to a Handler on a dead thread");
                Log.w(TAG, e.getMessage(), e);
                msg.recycle();
                return false;
            }
             // 第五步
            msg.markInUse();
            msg.when = when;
            Message p = mMessages;
            boolean needWake;
             // 第六步
            //根据when的比较来判断要添加的Message是否应该放在队列头部，当第一个添加消息的时候，
            // 测试队列为空，所以该Message也应该位于头部。
            if (p == null || when == 0 || when < p.when) {
                // New head, wake up the event queue if blocked.
                // 把msg的下一个元素设置为p
                msg.next = p;
                // 把msg设置为链表的头部元素
                mMessages = msg;
                 // 如果有阻塞，则需要唤醒
                needWake = mBlocked;
            } else {
                 // 第七步
                // Inserted within the middle of the queue.  Usually we don't have to wake
                // up the event queue unless there is a barrier at the head of the queue
                // and the message is the earliest asynchronous message in the queue.
                //除非消息队列的头部是障栅(barrier)，或者消息队列的第一个消息是异步消息，
                //否则如果是插入到中间位置，我们通常不唤醒消息队列，
                 // 第八步
                needWake = mBlocked && p.target == null && msg.isAsynchronous();
                Message prev;
                  // 第九步
                 // 不断遍历消息队列，根据when的比较找到合适的插入Message的位置。
                for (;;) {
                    prev = p;
                    p = p.next;
                    // 第十步
                    if (p == null || when < p.when) {
                        break;
                    }
                    // 第十一 步
                    if (needWake && p.isAsynchronous()) {
                        needWake = false;
                    }
                }
                // 第十二步
                msg.next = p; // invariant: p == prev.next
                prev.next = msg;
            }
              // 第十三步
            // We can assume mPtr != 0 because mQuitting is false.
            if (needWake) {
                nativeWake(mPtr);
            }
        }
         // 第十四步
        return true;
    }
```

因为这是消息入队的流程，为了让大家更好的理解，我将上面的流程大致分为12步骤

-   **第1步骤、** 判断msg的target变量是否为null，如果为null，则为障栅(barrier)，而障栅(barrier)入队则是通过postSyncBarrier()方法入队，所以msg的target一定有值
-   **第2步骤、** 判断msg的标志位，因为此时的msg应该是要入队，意味着msg的标志位应该显示还未被使用。如果显示已使用，明显有问题，直接抛异常。
-   **第3步骤、** 加入同步锁。
-   **第4步骤、** 判断消息队列是否正在被关闭，如果是正在被关闭，则return false告诉消息入队是失败，并且回收消息
-   **第5步骤、** 设置msg的when并且修改msg的标志位，msg标志位显示为已使用
-   **第6步骤、** 如果p==null则说明消息队列中的链表的头部元素为null；when == 0 表示立即执行；when< p.when 表示 msg的执行时间早与链表中的头部元素的时间，所以上面三个条件，那个条件成立，都要把msg设置成消息队列中链表的头部是元素
-   **第7步骤、** 如果上面三个条件都不满足则说明要把msg插入到中间的位置，不需要插入到头部
-   **第8步骤、** 如果头部元素不是障栅(barrier)或者异步消息，而且还是插入中间的位置，我们是不唤醒消息队列的。
-   **第9步骤、** 进入一个死循环，将p的值赋值给prev，前面的带我们知道，p指向的是mMessage，所以这里是将prev指向了mMessage，在下一次循环的时候，prev则指向了第一个message，一次类推。接着讲p指向了p.next也就是mMessage.next，也就是消息队列链表中的第二个元素。这一步骤实现了消息指针的移动，此时p表示的消息队列中第二个元素。
-   **第10步骤、** p==null，则说明没有下一个元素，即消息队列到头了，跳出循环；p!=null&&when < p.when 则说明当前需要入队的这个message的执行时间是小于队列中这个任务的执行时间的，也就是说这个需要入队的message需要比队列中这个message先执行，则说明这个位置刚刚是适合这个message的，所以跳出循环。 如果上面的两个条件都不满足，则说明这个位置还不是放置这个需要入队的message，则继续跟链表中后面的元素，也就是继续跟消息队列中的下一个消息进行对比，直到满足条件或者到达队列的末尾。
-   **第11步骤、** 因为没有满足条件，说明队列中还有消息，不需要唤醒。
-   **第12步骤、** 跳出循环后主要做了两件事：事件A，将入队的这个消息的next指向循环中获取到的应该排在这个消息之后message。事件B，将msg前面的message.next指向了msg。这样就将一个message完成了入队。
-   **第13步骤、** 如果需要唤醒，则唤醒，具体请看后面的Handler中的Native详解。
-   **第14步骤、** 返回true，告知入队成功。

提供两张图，让大家更好的理解入队

![](https://ask.qcloudimg.com/http-save/yehe-2957818/gmw3x5kiwi.png)

入队1.png

![](https://ask.qcloudimg.com/http-save/yehe-2957818/09c771kgmh.png)

入队2.png

总结一句话就是：就是遍历消息队列中所有的消息，根据when的比较找到合适添加Message的位置。

上面是sendMessage(Message msg)发送消息机制，这样再来看下其他的send方案

##### (二) boolean sendMessageAtFrontOfQueue(Message msg)

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 615行

```
    /**
     * Enqueue a message at the front of the message queue, to be processed on
     * the next iteration of the message loop.  You will receive it in
     * {@link #handleMessage}, in the thread attached to this handler.
     * <b>This method is only for use in very special circumstances -- it
     * can easily starve the message queue, cause ordering problems, or have
     * other unexpected side-effects.</b>
     *  
     * @return Returns true if the message was successfully placed in to the 
     *         message queue.  Returns false on failure, usually because the
     *         looper processing the message queue is exiting.
     */
    public final boolean sendMessageAtFrontOfQueue(Message msg) {
        MessageQueue queue = mQueue;
        if (queue == null) {
            RuntimeException e = new RuntimeException(
                this + " sendMessageAtTime() called with no mQueue");
            Log.w("Looper", e.getMessage(), e);
            return false;
        }
        return enqueueMessage(queue, msg, 0);
    }
```

老规矩先翻译一下注释：

> 在消息队列的最前面插入一个消息，在消息循环的下一次迭代中进行处理。你将在当前线程关联的Handler的handleMessage()中收到这个消息。由于它可以轻松的解决消息队列的排序问题和其他的意外副作用。

方法内部的实现和boolean sendMessageAtTime(Message msg, long uptimeMillis)大体上一致，唯一的区别就是该方法在调用enqueueMessage(MessageQueue, Message, long)方法的时候，最后一个参数是0而已。

##### (三) boolean sendEmptyMessage(int what)

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 517行

```
   /**
     * Sends a Message containing only the what value.
     *  
     * @return Returns true if the message was successfully placed in to the 
     *         message queue.  Returns false on failure, usually because the
     *         looper processing the message queue is exiting.
     */
    public final boolean sendEmptyMessage(int what)
    {
        return sendEmptyMessageDelayed(what, 0);
    }
```

老规矩先翻译一下注释：

> 发送一个仅有what的Message

内容很简单，也就是调用sendEmptyMessageDelayed(int,long)而已，那么下面我们来看下sendEmptyMessageDelayed(int,long)的具体实现。

###### 1、boolean sendEmptyMessageDelayed(int what, long delayMillis)

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 531行

```
    /**
     * Sends a Message containing only the what value, to be delivered
     * after the specified amount of time elapses.
     * @see #sendMessageDelayed(android.os.Message, long) 
     * 
     * @return Returns true if the message was successfully placed in to the 
     *         message queue.  Returns false on failure, usually because the
     *         looper processing the message queue is exiting.
     */
    public final boolean sendEmptyMessageDelayed(int what, long delayMillis) {
        Message msg = Message.obtain();
        msg.what = what;
        return sendMessageDelayed(msg, delayMillis);
    }
```

老规矩先翻译一下注释：

> 发送一个仅有what的Message，并且延迟特定的时间发送

这个方法内部主要就是做了3件事

-   1、调用Message.obtain();从消息对象池中获取一个空的Message。
-   2、设置这个Message的what值
-   3、调用sendMessageDelayed(Message,long) 将这个消息方法

sendMessageDelayed(Message,long) 这个方法上面有讲解过，这里就不详细说了

##### (四) boolean sendEmptyMessageAtTime(int what, long uptimeMillis)

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 547行

```
    /**
     * Sends a Message containing only the what value, to be delivered 
     * at a specific time.
     * @see #sendMessageAtTime(android.os.Message, long)
     *  
     * @return Returns true if the message was successfully placed in to the 
     *         message queue.  Returns false on failure, usually because the
     *         looper processing the message queue is exiting.
     */

    public final boolean sendEmptyMessageAtTime(int what, long uptimeMillis) {
        Message msg = Message.obtain();
        msg.what = what;
        return sendMessageAtTime(msg, uptimeMillis);
    }
```

老规矩先翻译一下注释：

> 发送一个仅有what的Message，并且在特定的时间发送

这个方法内部主要就是做了3件事

-   1、调用Message.obtain();从消息对象池中获取一个空的Message。
-   2、设置这个Message的what值
-   3、调用sendMessageAtTime(Message,long) 将这个消息方法

##### (五) 小结

综上所述

-   public final boolean sendMessage(Message msg)
-   public final boolean sendEmptyMessage(int what)
-   public final boolean sendEmptyMessageDelayed(int what, long delayMillis)
-   public final boolean sendEmptyMessageAtTime(int what, long uptimeMillis)
-   public final boolean sendMessageDelayed(Message msg, long delayMillis)
-   public boolean sendMessageAtTime(Message msg, long uptimeMillis)
-   public boolean sendMessageAtTime(Message msg, long uptimeMillis)
-   public final boolean sendMessageAtFrontOfQueue(Message msg)

> 以上这些send方案都会从这里或者那里最终走到**boolean enqueueMessage(MessageQueue queue, Message msg, long uptimeMillis)**如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/9oktkn4ot9.png)

send小结.png

### 三、 Handler的post方案

##### (一)、boolean post(Runnable r)方法

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 324行

```
    /**
     * Causes the Runnable r to be added to the message queue.
     * The runnable will be run on the thread to which this handler is 
     * attached. 
     *  
     * @param r The Runnable that will be executed.
     * 
     * @return Returns true if the Runnable was successfully placed in to the 
     *         message queue.  Returns false on failure, usually because the
     *         looper processing the message queue is exiting.
     */
    public final boolean post(Runnable r)
    {
       return  sendMessageDelayed(getPostMessage(r), 0);
    }
```

老规矩先翻译一下注释：

> 将一个Runnable添加到消息队列中，这个runnable将会在和当前Handler关联的线程中被执行。

这个方法内部很简单，就是调用了sendMessageDelayed(Message, long);这个方法，所以可见**boolean post(Runnable r)**这个方法最终还是走到上面说到的send的流程中，最终调用**boolean enqueueMessage(MessageQueue queue, Message msg, long uptimeMillis)**。

这里面调用了**Message getPostMessage(Runnable r)**，我们来看一下。

###### 1、Message getPostMessage(Runnable r)方法

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 725行

```
    private static Message getPostMessage(Runnable r) {
        Message m = Message.obtain();
        m.callback = r;
        return m;
    }
```

代码很简单，主要是做了两件事

-   通过Message.obtain()从消息对象池中获取一个空的Message
-   将这空的Message的callback变量指向Runnable

最后返回这个Message m。

###### 2、小结

> 所以我们知道**boolean post(Runnable r)方法**的内置也是通过Message.obtain()来获取一个Message对象m，然后仅仅把m的callback指向参数r而已。最后最终通过调用send方案的某个流程最终调用到**boolean enqueueMessage(MessageQueue queue, Message msg, long uptimeMillis)**

这时候聪明的同学一定会想，那么post方案的其他方法是不是也是这样的？**是的，的确都是这样**，这都是“套路”，那我们就用一一去检验。

##### (二)、boolean postAtTime(Runnable r, long uptimeMillis)方法

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 347行

```
    /**
     * Causes the Runnable r to be added to the message queue, to be run
     * at a specific time given by <var>uptimeMillis</var>.
     * <b>The time-base is {@link android.os.SystemClock#uptimeMillis}.</b>
     * Time spent in deep sleep will add an additional delay to execution.
     * The runnable will be run on the thread to which this handler is attached.
     *
     * @param r The Runnable that will be executed.
     * @param uptimeMillis The absolute time at which the callback should run,
     *         using the {@link android.os.SystemClock#uptimeMillis} time-base.
     *  
     * @return Returns true if the Runnable was successfully placed in to the 
     *         message queue.  Returns false on failure, usually because the
     *         looper processing the message queue is exiting.  Note that a
     *         result of true does not mean the Runnable will be processed -- if
     *         the looper is quit before the delivery time of the message
     *         occurs then the message will be dropped.
     */
    public final boolean postAtTime(Runnable r, long uptimeMillis)
    {
        return sendMessageAtTime(getPostMessage(r), uptimeMillis);
    }
```

老规矩先翻译一下注释：

> 将一个Runnable添加到消息队列中，这个runnable将会一个特定的时间被执行，这个时间是以android.os.SystemClock.uptimeMillis()为基准。如果在深度睡眠下，会推迟执行的时间，这个Runnable将会在和当前Hander关联的线程中被执行。

方法内部也是先是调用getPostMessage(Runnable)来获取一个Message，这个Message的callback字段指向了这个Runnable，然后调用sendMessageAtTime(Message,long)。

##### (三)、boolean postAtTime(Runnable r, Object token, long uptimeMillis)方法

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 372行

```
    /**
     * Causes the Runnable r to be added to the message queue, to be run
     * at a specific time given by <var>uptimeMillis</var>.
     * <b>The time-base is {@link android.os.SystemClock#uptimeMillis}.</b>
     * Time spent in deep sleep will add an additional delay to execution.
     * The runnable will be run on the thread to which this handler is attached.
     *
     * @param r The Runnable that will be executed.
     * @param uptimeMillis The absolute time at which the callback should run,
     *         using the {@link android.os.SystemClock#uptimeMillis} time-base.
     * 
     * @return Returns true if the Runnable was successfully placed in to the 
     *         message queue.  Returns false on failure, usually because the
     *         looper processing the message queue is exiting.  Note that a
     *         result of true does not mean the Runnable will be processed -- if
     *         the looper is quit before the delivery time of the message
     *         occurs then the message will be dropped.
     *         
     * @see android.os.SystemClock#uptimeMillis
     */
    public final boolean postAtTime(Runnable r, Object token, long uptimeMillis)
    {
        return sendMessageAtTime(getPostMessage(r, token), uptimeMillis);
    }
```

这个方法的翻译同上，这个方法和上个方法唯一不同就是多了Object参数，而这个参数仅仅是把Message.obtain();获取的Message的obj字段的指向第二个入参token而已。最后也是调用sendMessageAtTime(Message,long)。

##### (四)、boolean postDelayed(Runnable r, long delayMillis)方法

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 396行

```
    /**
     * Causes the Runnable r to be added to the message queue, to be run
     * after the specified amount of time elapses.
     * The runnable will be run on the thread to which this handler
     * is attached.
     * <b>The time-base is {@link android.os.SystemClock#uptimeMillis}.</b>
     * Time spent in deep sleep will add an additional delay to execution.
     *  
     * @param r The Runnable that will be executed.
     * @param delayMillis The delay (in milliseconds) until the Runnable
     *        will be executed.
     *        
     * @return Returns true if the Runnable was successfully placed in to the 
     *         message queue.  Returns false on failure, usually because the
     *         looper processing the message queue is exiting.  Note that a
     *         result of true does not mean the Runnable will be processed --
     *         if the looper is quit before the delivery time of the message
     *         occurs then the message will be dropped.
     */
    public final boolean postDelayed(Runnable r, long delayMillis)
    {
        return sendMessageDelayed(getPostMessage(r), delayMillis);
    }
```

这个方法也很简单，就是依旧是通过getPostMessage(Runnable)来获取一个Message，最后调用sendMessageDelayed(Message,long)而已。

##### (五)、boolean postDelayed(Runnable r, long delayMillis)方法

代码在[Handler.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FHandler.java&objectId=1199417&objectType=1&isNewArticle=undefined) 416行

```
    /**
     * Posts a message to an object that implements Runnable.
     * Causes the Runnable r to executed on the next iteration through the
     * message queue. The runnable will be run on the thread to which this
     * handler is attached.
     * <b>This method is only for use in very special circumstances -- it
     * can easily starve the message queue, cause ordering problems, or have
     * other unexpected side-effects.</b>
     *  
     * @param r The Runnable that will be executed.
     * 
     * @return Returns true if the message was successfully placed in to the 
     *         message queue.  Returns false on failure, usually because the
     *         looper processing the message queue is exiting.
     */
    public final boolean postAtFrontOfQueue(Runnable r)
    {
        return sendMessageAtFrontOfQueue(getPostMessage(r));
    }
```

这个方法也很简单，就是依旧是通过getPostMessage(Runnable)来获取一个Message，最后调用sendMessageAtFrontOfQueue(Message)而已。

##### (六)、小结

Handler的post方案的如下方法

-   boolean post(Runnable r)
-   postAtTime(Runnable r, long uptimeMillis)
-   boolean postAtTime(Runnable r, Object token, long uptimeMillis)
-   boolean postDelayed(Runnable r, long delayMillis)
-   boolean postAtFrontOfQueue(Runnable r) 都是通过**Message getPostMessage(Runnable )**中调用Message m = Message.obtain();来获取一个空的Message，然后把这个Message的callback变量指向了Runnable，最终调用相应的send方案而已。

所以我们可以这样说:

> Handler的发送消息(障栅除外)，无论是通过send方案还是pos方案最终都会做走到 **_boolean enqueueMessage(MessageQueue queue, Message msg, long uptimeMillis)_** 中

![](https://ask.qcloudimg.com/http-save/yehe-2957818/cwcakjuu7j.png)

消息发送.png

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2017.09.14 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除