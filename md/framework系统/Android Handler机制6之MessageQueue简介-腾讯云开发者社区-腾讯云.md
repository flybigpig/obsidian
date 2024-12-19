###### 本片文章的主要内容如下：

-   1、MessageQueue简介
-   2、MessageQueue类注释
-   3、MessageQueue成员变量
-   4、MessageQueue的构造函数
-   5、native层代码的初始化
-   6、IdleHandler简介
-   7、MessageQueue中的Message分类

[MessageQueue官网](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fdeveloper.android.com%2Freference%2Fandroid%2Fos%2FMessageQueue.html&objectId=1199413&objectType=1&isNewArticle=undefined)

### 一、MessageQueue简介

> MessageQueue即[消息队列](https://cloud.tencent.com/product/message-queue-catalog?from_column=20065&from=20065)，这个消息队列和上篇文章里面的[Android Handler机制5之Message简介与消息对象对象池](https://cloud.tencent.com/developer/article/1199403?from_column=20421&from=20421)里面的 _**消息对象池**_ 可**不是**同一个东西。MessageQueue是一个消息队列，Handler将Message发送到消息队列中，消息队列会按照一定的规则取出要执行的Message。需要注意的是Java层的MessageQueue负责处理Java的消息，native也有一个MessageQueue负责处理native的消息，本文重点是Java层，所以暂时不分析native源码。

### 二、MessageQueue类注释

[MessageQueue.java源码地址](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199413&objectType=1&isNewArticle=undefined)

> Low-level class holding the list of messages to be dispatched by a Looper. Messages are not added directly to a MessageQueue, but rather through Handler objects associated with the Looper. You can retrieve the MessageQueue for the current thread with Looper.myQueue().

翻译一下：

> 它是一个被Looper分发、低等级的持有Message集合的类。Message并不是直接加到MessageQueue的，而是通过Handler对象和Looper关联到一起。 我们可以通过Looper.myQueue()方法来检索当前线程的MessageQueue 它是一个低等级的持有Messages集合的类，被Looper分发。Messages并不是直接加到MessageQueue的，而是通过Handler对象和Looper关联到一起。我们可以通过Looper.myQueue()方法来检索当前线程的

### 三、MessageQueue成员变量

```
    // True if the message queue can be quit.
    //用于标示消息队列是否可以被关闭，主线程的消息队列不可关闭
    private final boolean mQuitAllowed;

    @SuppressWarnings("unused")
    // 该变量用于保存native代码中的MessageQueue的指针
    private long mPtr; // used by native code

    //在MessageQueue中，所有的Message是以链表的形式组织在一起的，该变量保存了链表的第一个元素，也可以说它就是链表的本身
    Message mMessages;

    //当Handler线程处于空闲状态的时候(MessageQueue没有其他Message时)，可以利用它来处理一些事物，该变量就是用于保存这些空闲时候要处理的事务
    private final ArrayList<IdleHandler> mIdleHandlers = new ArrayList<IdleHandler>();

    // 注册FileDescriptor以及感兴趣的Events，例如文件输入、输出和错误，设置回调函数，最后
    // 调用nativeSetFileDescriptorEvent注册到C++层中，
    // 当产生相应事件时，由C++层调用Java的DispathEvents，激活相应的回调函数
    private SparseArray<FileDescriptorRecord> mFileDescriptorRecords;

     // 用于保存将要被执行的IdleHandler
    private IdleHandler[] mPendingIdleHandlers;

    //标示MessageQueue是否正在关闭。
    private boolean mQuitting;

    // Indicates whether next() is blocked waiting in pollOnce() with a non-zero timeout.
    // 标示 MessageQueue是否阻塞
    private boolean mBlocked;

    // The next barrier token.
    // Barriers are indicated by messages with a null target whose arg1 field carries the token.
    // 在MessageQueue里面有一个概念叫做障栅，它用于拦截同步的Message，阻止这些消息被执行，
    // 只有异步Message才会放行。障栅本身也是一个Message，只是它的target为null并且arg1用于区分不同的障栅，
     // 所以该变量就是用于不断累加生成不同的障栅。
    private int mNextBarrierToken;
```

### 四、MessageQueue的构造函数

通过分析下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/l1tia3mjtd.png)

MessageQueue构造函数.png

我们知道MessageQueue就一个构造函数

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199413&objectType=1&isNewArticle=undefined) 68行

```
    MessageQueue(boolean quitAllowed) {
        mQuitAllowed = quitAllowed;
        mPtr = nativeInit();
    }
```

> MessageQueue只是有一个构造函数，该构造函数是包内可见的，其内部就两行代码，分别是设置了MessageQueue是否可以退出和native层代码的相关初始化

### 五、native层代码的初始化

在MessageQueue的构造函数里面调用 nativeInit()，我们来看下

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199413&objectType=1&isNewArticle=undefined) 61行

```
    private native static long nativeInit();
```

根据[Android跨进程通信IPC之3——关于"JNI"的那些事](https://cloud.tencent.com/developer/article/1199091?from_column=20421&from=20421)中知道，nativeInit这个native方法对应的是[android\_os\_MessageQueue.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_os_MessageQueue.cpp&objectId=1199413&objectType=1&isNewArticle=undefined)里面的android\_os\_MessageQueue\_nativeInit(JNIEnv\* , jclass )函数

代码在[android\_os\_MessageQueue.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_os_MessageQueue.cpp&objectId=1199413&objectType=1&isNewArticle=undefined) 172 行

```
static jlong android_os_MessageQueue_nativeInit(JNIEnv* env, jclass clazz) {
    // 初始化native消息队列
    NativeMessageQueue* nativeMessageQueue = new NativeMessageQueue();
    if (!nativeMessageQueue) {
        jniThrowRuntimeException(env, "Unable to allocate native queue");
        return 0;
    }

    nativeMessageQueue->incStrong(env);
    return reinterpret_cast<jlong>(nativeMessageQueue);
}
```

后面在讲解流程时候的会详细讲解，这里就不如深入了。

### 六、IdleHandler简介

> 作为Android开发者我们知道，Handler除了用于发送Message，其本身也承载着执行具体业务逻辑的责任handlerMessage(Message msg)，而IdleHandler在处理业务逻辑方面和Handler一样，不过它只会在线程空闲的时候才执行业务逻辑的处理，这些业务经常是哪些不是很紧要或者不可预期的，比如GC。

##### (一) IdleHandler接口

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199413&objectType=1&isNewArticle=undefined) 777行

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
        boolean queueIdle();
    }
```

老规矩 先来翻译一下接口的注释：

> 回调的接口，当线程空闲的时候可以利用它来处理一些业务员

这个IdleHandler接口就一个抽象方法queueIdle，我也看一下抽象方法的注释

> 当消息队内所有的Message都执行完之后，该方法会被调用。该返回值为True的时候，IdleHandler会一直保持在消息队列中，False则会执行完该方法后移除IdleHandler。需要注意的是，当消息队列中还有其他Delay Message并且这些Message还没到被执行的时间的时候，由于线程是空闲的，所以IdleHandler也可能会被执行，

从源码可以看出IdleHandler其实就是一个简单的回调接口，内部就一个带返回值的方法**boolean queueIdle()**，在使用的时候只需要实现该接口并加入到MessageQueue中就可以了，例如

从源码可以看出IdleHandler其实就是一个简单的回调接口，内部就一个带返回值的方法boolean queueIdle()，在使用的时候只需要实现该接口并加入到MessageQueue中就可以了，例如下面简答的代码所示

```
    MessageQueue messageQueue = Looper.myQueue();
    messageQueue.addIdleHandler(new MessageQueue.IdleHandler() {
        @Override
        public boolean queueIdle() {
            // do something.
            return false;
        }
    });
```

##### (二) 添加IdleHandler：addIdleHandler(IdleHandler handler)

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199413&objectType=1&isNewArticle=undefined) 115行

```
    /**
     * Add a new {@link IdleHandler} to this message queue.  This may be
     * removed automatically for you by returning false from
     * {@link IdleHandler#queueIdle IdleHandler.queueIdle()} when it is
     * invoked, or explicitly removing it with {@link #removeIdleHandler}.
     *
     * <p>This method is safe to call from any thread.
     *
     * @param handler The IdleHandler to be added.
     */
    public void addIdleHandler(@NonNull IdleHandler handler) {
        if (handler == null) {
            throw new NullPointerException("Can't add a null IdleHandler");
        }
        synchronized (this) {
            mIdleHandlers.add(handler);
        }
    }
```

看先注释：

-   添加一个新的IdleHanlder到消息队列中，当IdleHandler的回调方法返回False的时候，该IdleHanlder在被执行后会被立即移除，你也可以通过调用removeIdleHandler(IdleHandler handler)方法来移除指定的IdleHandler。
-   在任何线程中调用该方法都是安全的。

方法内部很简单，就是三步

-   第一步，做非空判断
-   第二步，加一个同步锁
-   第三步，调用mIdleHandlers.add(handler);添加 (PS:mIdleHandlers是一个ArrayList)

##### (四) 删除IdleHandler：removeIdleHandler(IdleHandler handler)

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199413&objectType=1&isNewArticle=undefined)

133行

```
    /**
     * Remove an {@link IdleHandler} from the queue that was previously added
     * with {@link #addIdleHandler}.  If the given object is not currently
     * in the idle list, nothing is done.
     *
     * <p>This method is safe to call from any thread.
     *
     * @param handler The IdleHandler to be removed.
     */
    public void removeIdleHandler(@NonNull IdleHandler handler) {
        synchronized (this) {
            mIdleHandlers.remove(handler);
        }
    }
```

看先注释：

-   从消息队列中移除一个之前添加的IdleHandler。如果该IdleHandler不存在，则什么也不做。

移除IdleHandler的方法同样很简单，下一步同步处理然后直接mIdleHandlers.reomve(handler)就可以了。

### 七、MessageQueue中的Message分类

在MessageQueue中，Message被分成3类，分别是

-   同步消息
-   异步消息
-   障栅

那我们就一次来看下：

##### (一) 同步消息：

> 正常情况下我们通过Handler发送的Message都属于同步消息，除非我们在发送的时候执行该消息是一个异步消息。同步消息会按顺序排列在队列中，除非指定Message的执行时间，否咋Message会按顺序执行。

##### (二) 异步消息：

> 想要往消息队列中发送异步消息，我们必须在初始化Handler的时候通过构造函数public Handler(boolean async)中指定Handler是异步的，这样Handler在讲Message加入消息队列的时候就会将Message设置为异步的。

##### (三) 障栅(Barrier)：

> 障栅(Barrier) 是一种特殊的Message，它的target为null(只有障栅的target可以为null，如果我们自己视图设置Message的target为null的话会报异常)，并且arg1属性被用作障栅的标识符来区别不同的障栅。障栅的作用是用于拦截队列中同步消息，放行异步消息。就好像交警一样，在道路拥挤的时候会决定哪些车辆可以先通过，这些可以通过的车辆就是异步消息。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/0etd045spa.png)

同步和异步.png

###### 1、添加障栅：postSyncBarrier()

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199413&objectType=1&isNewArticle=undefined)

458行

```
    /**
     * Posts a synchronization barrier to the Looper's message queue.
     *
     * Message processing occurs as usual until the message queue encounters the
     * synchronization barrier that has been posted.  When the barrier is encountered,
     * later synchronous messages in the queue are stalled (prevented from being executed)
     * until the barrier is released by calling {@link #removeSyncBarrier} and specifying
     * the token that identifies the synchronization barrier.
     *
     * This method is used to immediately postpone execution of all subsequently posted
     * synchronous messages until a condition is met that releases the barrier.
     * Asynchronous messages (see {@link Message#isAsynchronous} are exempt from the barrier
     * and continue to be processed as usual.
     *
     * This call must be always matched by a call to {@link #removeSyncBarrier} with
     * the same token to ensure that the message queue resumes normal operation.
     * Otherwise the application will probably hang!
     *
     * @return A token that uniquely identifies the barrier.  This token must be
     * passed to {@link #removeSyncBarrier} to release the barrier.
     *
     * @hide
     */
    public int postSyncBarrier() {
        return postSyncBarrier(SystemClock.uptimeMillis());
    }
```

看先注释：

-   向Looper的消息队列中发送一个同步的障栅(barrier)
-   如果没有发送同步的障栅(barrier)，消息处理像往常一样该怎么处理就怎么处理。当发现遇到障栅(barrier)后，队列中后续的同步消息会被阻塞，直到通过调用removeSyncBarrier()释放指定的障栅(barrier)。
-   这个方法会导致立即推迟所有后续发布的同步消息，知道满足释放指定的障栅(barrier)。而异步消息则不受障栅(barrier)的影响，并按照之前的流程继续处理。
-   必须使用相同的token去调用removeSyncBarrier()，来保证插入的障栅(barrier)和移除的是一个同一个，这样可以确保消息队列可以正常运行，否则应用程序可能会挂起。
-   返回值是障栅(barrier)的唯一标识符，持有个token去调用removeSyncBarrier()方法才能达到真正的释放障栅(barrier)

方法内部很简单就是调用了postSyncBarrier(SystemClock.uptimeMillis())，通过[Android Handler机制3之SystemClock类](https://cloud.tencent.com/developer/article/1199311?from_column=20421&from=20421)，我们知道SystemClock.uptimeMillis()是手机开机到现在的时间。那我们来看下这个postSyncBarrier(long)方法

###### 1.1、postSyncBarrier(long when)

###### 在讲解之前先补充一个知识点：MessageQueue里面的所有Message是按照时间从前往后有序排列的。后面我们讲解Handler机制流程的时候会详细说明

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199413&objectType=1&isNewArticle=undefined)

462行

```
 private int postSyncBarrier(long when) {
        // Enqueue a new sync barrier token.
        // We don't need to wake the queue because the purpose of a barrier is to stall it.
        synchronized (this) {
              /** 第一步 */
            final int token = mNextBarrierToken++;
             /** 第二步 */
            final Message msg = Message.obtain();
            msg.markInUse();
            msg.when = when;
            msg.arg1 = token;

              /** 第三步 */
            Message prev = null;
            //把消息队列的第一个元素指向p
            Message p = mMessages;
            if (when != 0) {
             /** 第四步 */
                while (p != null && p.when <= when) {
                    //通过p的时间点和障栅的时间点的比较，如果比障栅的小，就把消息队列中的消息向后移动一位(因为消息队列中所有元素是按照时间排序的)
                    prev = p;
                    p = p.next;
                }
            }
              /** 第五步 */
             //prev != null 代表不是消息队列的头部，则需要考虑前面一个消息和后面的一个消息
            if (prev != null) { // invariant: p == prev.next
                //msg的下一个消息是p 
                msg.next = p;
                 //msg的上一个消息是msg
                prev.next = msg;
            } else {
                //prev == null 代表是消息队列的头部，则只需要负责下一个消息即可
                msg.next = p;
                //设置自己是消息队列的头部
                mMessages = msg;
            }
            /** 第六步 */
            return token;
        }
    }
```

方法详解

-   **第一步** 获取障栅的唯一标示，然后自增该变量作为下一个障栅的标示，通过mNextBarrierToken ++，我们得知，这些唯一标示是从0开始，自加1的。
-   **第二步** 从Message消息对象池中获取一个Message，并重置它的when和arg1。并且arg1为token的值，通过msg.markInUse()标示msg正在被使用。这里并没有给tareget赋值。
-   **第三步** 创建变量pre和p为第四步做准备，其中p被赋值为mMessages，而mMessages未消息队列的第一个元素，所以p此时就是消息队列的第一个元素。
-   **第四步** 通过对 队列中的第一个Message的when和障栅的when作比较，决定障栅在整个消息队列中的位置，比如是放在队列的头部，还是队列中第二个位置，如果障栅在头部，则拦截后面所有的同步消息，如果在第二的位置，则会放过第一个，然后拦截剩下的消息，以此类推。
-   **第五步** 把msg插入到消息队列中
-   **第六步** 返回token

从源码中我们可以看出，在把障栅插入队列的时候先通过when的比较，根据不同的情况把障栅插入到不同的位置，具体情况如下图所示：

ps:蓝色的为Message、红色的为Barrier

当Message.when<Barrier.when，也就是第一个Message的执行时间点在障栅之前。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/hsirwh6b7r.png)

障栅插入队列1.png

当Message.when>=Barrier.when，也就是第一个Message的执行时间点在障栅之后。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/j76gz1z2k3.png)

障栅插入队列2.png

###### 大家在看上面的代码的时候有没有注意到一个事情，就是msg这个对象的target是null，因为从始至终就没有赋值过，这也是后面在移除障栅的时候通过判断条件之一：是target是否为null来判断的

###### 2、移除障栅

代码在[MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199413&objectType=1&isNewArticle=undefined)

501行

```
   /**
     * Removes a synchronization barrier.
     *
     * @param token The synchronization barrier token that was returned by
     * {@link #postSyncBarrier}.
     *
     * @throws IllegalStateException if the barrier was not found.
     *
     * @hide
     */
    public void removeSyncBarrier(int token) {
        // Remove a sync barrier token from the queue.
        // If the queue is no longer stalled by a barrier then wake it.
        synchronized (this) {
            Message prev = null;
            // 获取消息队列的第一个元素
            Message p = mMessages;
            //遍历消息队列的所有元素，直到p.targe==null并且 p.arg1==token才是我们想要的障栅
            while (p != null && (p.target != null || p.arg1 != token)) {
                prev = p;
                p = p.next;
            }
            if (p == null) {
                throw new IllegalStateException("The specified message queue synchronization "
                        + " barrier token has not been posted or has already been removed.");
            }
            //是否需要唤醒
            final boolean needWake;
            //如果是障栅是不是第一个圆度
            if (prev != null) {
                //跳过障栅，将障栅的上一个元素的next指向障栅的next
                prev.next = p.next;
                //因为有元素，所以不需要唤醒
                needWake = false;
            } else {
                //如果是第一个元素，则直接下消息队列中的第一个元素指向障栅的下一个即可
                mMessages = p.next;
                 //如果消息队列中的第一个元素是null则说明消息队列中消息，所以需要唤醒
                 //
                needWake = mMessages == null || mMessages.target != null;
            }
            p.recycleUnchecked();

            // If the loop is quitting then it is already awake.
            // We can assume mPtr != 0 when mQuitting is false.
            if (needWake && !mQuitting) {
                nativeWake(mPtr);
            }
        }
    }
```

> 删除障栅(barrier)的方法也很简单，就是不断地遍历消息队列(链表结构)，直到倒找与指定的token相匹配的障栅，然后把它从队列中移除。

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2017.09.14 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除