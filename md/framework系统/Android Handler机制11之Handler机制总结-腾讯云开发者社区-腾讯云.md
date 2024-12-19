###### 本片文章的主要内容如下：

-   1、Handler机制的思考
-   2、Handler消息机制
-   3、享元模式
-   4、HandlerThread
-   5、Handler的内存泄露
-   6、Handler的面试题

### 一、Handler机制的思考

-   先提一个问题哈，如果让你设计一个操作系统，你会怎么设计？

> 我们一般操作系统都会有一个消息系统，里面有个死循环，不断的轮训处理其他各种输入设备输入的信息，比如你在在键盘的输入，鼠标的移动等。这些输入信息最终都会进入你的操作系统，然后由操作系统的内部轮询机制挨个处理这些信息。

-   那Android系统那？它内部是怎么实现的? 如果让你设计，你会怎么设计？

> 答：如果让我设计，肯定和上面一样：

-   1 设计一个类，里面有一个死循环去做循环操作；
-   2 用一个类来抽象代表各种输入信息/消息；这个信息/消息应该还有一个唯一标识符字段；如果这个信息里面有个对象来保存对应的键值对；方便其他人往这个信息/消息 存放信息；这个信息/消息应该有个字段标明消息产生的时间；
-   3 而上面的这些 信息/消息 又组成一个集合。常用集合很多，那是用ArrayList好还是LinkedList或者Map好那？因为前面说了是一个死循环去处理，所以这个集合最好是"线性和排序的"比较好，因为输入有先后，一般都是按照输入的时间先后来构成。既然这样就排除了Map，那么就剩下来了ArrayList和LinkedList。我们知道一个操作系统的事件是很多的，也就是说对应的信息/消息很多，所以这个集合肯定会面临大量的"插入"操作，而在"插入"效能这块，LinkedList有着明显的优势，所以这个集合应该是一个链表，但是链表又可以分为很多种，因为是线性排序的，所以只剩下"双向链表"和"单向链表”，但是由于考虑下手机的性能问题，大部分人肯定会倾向于选择"单向链表"，因为"单项链表"在增加和删除上面的复杂度明显低于"双向链表"。
-   4、最后还应该有两个类，一个负责生产这个输入信息，一个负责消费这些信息。因为涉及到消费端，所以上面2中说的信息/消息应该有一个字段负责指向消费端。

**经过上面的思考，大家是不是发现和其实我们Handler的机制基本上一致。Looper负责轮询；Message代表消息，为了区别对待，用what来做为标识符，when表示时间，data负责存放键值对；MessageQueue则代表Message的集合，Message内部同时也是单项链表的。通过上面的分析，希望大家对Handler机制的总体设计有不一样的感悟。**

### 二、Handler消息机制

> 如果你想要让一个Android的应用程序反应灵敏，那么你必须防止它的UI线程被阻塞。同样地，将这些阻塞的或者计算密集型的任务转到工作线程去执行也会提高程序的响应灵敏性。然而，这些任务的执行结果通常需要重新更新UI组件的显示，但该操作只能在UI线程中去执行。有一些方法解决了UI线程的阻塞问题，例如阻塞对象，共享内存以及管道技术。Android为了解决这个问题，提供了一种自有的消息传递机制——Handler。Handler是Android Framework架构中的一个基础组件，它实现了一种非阻塞的消息传递机制，在消息转换的过程中，消息的生产者和消费者都不会阻塞。

Handler由以下部分组成：

-   Handler
-   Message
-   MessageQueue
-   Looper

下面我们来了解下它们及它们之间的交互。

##### (一)、Handler

> Handler 是线程间传递消息的即时接口，生产线程和消费线程用以下操作来使用Handler

-   生产线程：在[消息队列](https://cloud.tencent.com/product/message-queue-catalog?from_column=20065&from=20065)中创建、插入或移除消息
-   消费线程：处理消息

![](https://ask.qcloudimg.com/http-save/yehe-2957818/ydhgowh822.png)

Handler.png

> 每个Handler都有一个与之关联的Looper和消息队列。有两种创建Handler方式(这里不是说只有两个构造函数，而是说把它的构造函数分为两类)

-   通过默认的构造方法，使用当前线程中关联的Looper
-   显式地指定使用Looper

如果没有指定Looper的Handler是无法工作的，因为它无法将消息放到消息队列中。同样地，它无法获取要处理的消息。

```
    public Handler(Callback callback, boolean async) {
        if (FIND_POTENTIAL_LEAKS) {
            final Class<? extends Handler> klass = getClass();
            if ((klass.isAnonymousClass() || klass.isMemberClass() || klass.isLocalClass()) &&
                    (klass.getModifiers() & Modifier.STATIC) == 0) {
                Log.w(TAG, "The following Handler class should be static or leaks might occur: " +
                    klass.getCanonicalName());
            }
        }

        mLooper = Looper.myLooper();
        if (mLooper == null) {
            throw new RuntimeException(
                "Can't create handler inside thread that has not called Looper.prepare()");
        }
        mQueue = mLooper.mQueue;
        mCallback = callback;
        mAsynchronous = async;
    }
```

如果是使用上面Handler的构造函数，它会检查当前线程有没有可用的Looper对象，如果没有，它会抛出一个运行时的异常，如果正常的话，Handler会持有Looper中的消息队列对象的引用。

> PS：同一个线程中多的Handler分享一个同样的消息队列，因为他们分享的是同一个Looper对象

Callback参数是一个可选的参数，如果提供的话，它将会处理由Looper分发的过来的消息。

##### (二)、Message

> Message 是容纳任意数据的容器。生产线程发送消息给Handler，Handler将消息加入到消息队列中。消息提供了三种额外的信息，以供Handler和消息队列处理时使用：

-   what：一种标识符，Handler能使用它来区分不同的消息，从而采取不同的处理方法
-   time：告诉消息队列合适处理消息
-   target：表示那一个Handler应该处理消息

android.os.Message 消息一般是通过Handler中以下方法来创建的

```
public final Message obtainMessage()
public final Message obtainMessage(int what)
public final Message obtainMessage(int what, Object obj)
public final Message obtainMessage(int what, int arg1, int arg2)
public final Message obtainMessage(int what, int arg1, int arg2, Object obj)
```

消息从消息池中获取得到，方法中提供的参数会放到消息体对应的字段中。Handler同样可以设置消息的目标为其自身，这允许我们进行链式调用，比如：

```
mHandler.obtainMessage(MSG_SHOW_IMAGE, mBitmap).sendToTarget();
```

消息池是一个消息对象的单项链表集合，它的最大长度是50。在Handler处理完这条消息之后，消息队列把这个对象返回到消息池中，并且重置其所有字段。

当使用Handler调用post方法来执行一个Runnable时，Handler隐式地创建了一个新的消息，并且设置callback参数来存储这个Runnable。

```
Message m = Message.obtain();
m.callback = r;
```

![](https://ask.qcloudimg.com/http-save/yehe-2957818/e5tavhyztc.png)

Handler与Message.png

生产线程发送消息给 Handler 的交互

> 在上图中，我们能看到生产线程和 Handler 的交互。生产者创建了一个消息，并且发送给Handler，随后Handler 将这个消息加入消息队列中，在未来某个时间，Handler 会在消费小城中处理这个消息。

##### (三)、MessageQueue

> MessageQueue是一个消息体对象的无界的单向链表集合，它按照时序将消息插入队列，最小的时间戳将会被首先处理。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/jr0i7gs08o.png)

MessageQueue.png

消息队列也通过SystemClock.uptimeMillis获取当前时间，维护一个阻塞阀值(dispatch barrier)。当一个消息体的时间戳低于这个值的时候，消息就会分发给Handler进行处理

Handler 提供了三种方式来发送消息：

```
public final boolean sendMessageDelayed(Message msg, long delayMillis)
public final boolean sendMessageAtFrontOfQueue(Message msg)
public boolean sendMessageAtTime(Message msg, long uptimeMillis)
```

以延迟的方式发送消息，是设置了消息体的time字段为SystemClock.uptimeMillis()+delayMillis。然而，通过sendMessageAtFontOfQueue方法是把消息插入到队首，会将其时间字段设置为0，消息会在下一次轮训时被处理。需要谨慎使用这个方法，因为它可能会英系那个消息队列，造成顺序问题，或是其他不可预料的副作用。

##### (四)、消息队列、Handler、生产线程的交互

现在我们可以概括消息队列、Handler、生产线程的交互：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/lrg0z2598a.png)

消息队列、Handler、生产线程的交互.png

> 上图中，多个生产线程提交消息到不同的Handler中，然而，不同的Handler都与同一个Looper对象关联，因此所有的消息都加入到同一个消息队列中。这一点非常重要，Android中创建的许多不同的Handler都关联到主线程的Looper。

比如：

-   The Choreographer：处理垂直同步与帧更新
-   The ViewRoot：处理输入和窗口时间，配置修改等等
-   The InputMethodManager不会大量生成消息，因为这可能会抑制处理系统

##### (五)、Looper

> Looper 从消息队列中读取消息，然后分发给对应的Handler处理。一旦消息超过阻塞阀，那么Looper就会在下一轮读取过程中读取到它。Looper在没有消息分发的时候变成阻塞状态，当有消息可用时会继续轮询。

每个线程只能关联一个Looper，给线程附加的另外的Looper会导致运行时的异常。通过使用Looper的Threadlocal对象可以保证线程只关联一个Looper对象。

调用Looper.quit()方法会立即终止Looper，并且丢弃消息队列中的已经通过阻塞阀的所有消息。调用Looper.quitSafely()方法能够保证所有待分发的消息在队列中等待的消息被丢弃前得到处理。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/e2vz7m0r6v.png)

Looper.png

> Handler 与消息队列和Looper 直接交互的整体流程 Looper 应在线程的run方法中初始化。调用静态方法Looper.prepare()会检查线程是否与一个已存在的Looper关联。这个过程的实现是通过Looper类中的ThreadLocal对象来检查Looper对象是否存在。如果Looper不存在，将会创建一个新的Looper对象和一个新的消息队列。如下代码展示了这个过程

**PS: 公有的prepare方法会默认调用prepare(true)**

```
private static void prepare(boolean quitAllowed) {
    if (sThreadLocal.get() != null) {
        throw new RuntimeException(“Only one Looper may be created per thread”);
    }
    sThreadLocal.set(new Looper(quitAllowed));
}
```

Handler 现在能接收到消息并加入到消息队列中，执行静态方法Looper.loop()方法会开始将消息从消息队列中出队。每次轮训迭代器指向下一条消息，接着分发消息对应目标地的Handler，然后回收消息到消息池中。Looper.looper()方法循环执行这个过程，直到Looper终止。下面代码片段展示这个过程：

```
public static void loop() {
    if (me == null) {
        throw new RuntimeException("No Looper; Looper.prepare() wasn't called on this thread.");
    }
    final MessageQueue queue = me.mQueue;
    for (;;) {
        Message msg = queue.next(); // might block
        if (msg == null) {
            // No message indicates that the message queue is quitting.
            return;
        }
        msg.target.dispatchMessage(msg);
        msg.recycleUnchecked();
    }
}
```

##### (六)、整体流程图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/4grp6humel.gif)

整体流程图.gif

##### 三、享元模式

> 享元模式是对象池的一种实现，尽可能减少内存的使用，使用缓存来共享可用的对象，避免创建过多的对象。Android中Message使用的设计模式就是享元模式，将获取Message通过obtain方法从对象池获取，Message使用结束通过recyle将Message归还给对象池，达到循环利用对象，避免重复创建的目的

##### (一)概念

> **享元模式(Flywight Pattern)** 是一种软件设计模式。它使用共享物件，用来尽可能减少内存使用量以及分享咨询给尽可能多的相似物件；它适用于只是因重复而导致无法使用无法令人接受的大量内存的大量物件。通常物件中的部分状态时可以分享的。常见的做法是把他们放到外部数据结构，当需要使用时将他们传递给享元。

##### (二) 为什么要用享元模式

-   当一个软件系统在运行时产生的对象数量太多，将导致运行代价过高，带来系统性能下降的问题。所以需要采用一个共享来避免大量拥有相同内容对象的开销。在Java中，String类型就是使用享元模式。String对象是final类型，对象一旦创建就不可改变。在Java字符串常量都是存在常量池中的，Java会确保一个字符串常量在常量池只有一个拷贝。
-   是对对象池的一种实现，共享对象，避免重复的创建，采用一个共享来避免大量拥有相同内容对象的开销。使用享元模式可以有效支持大量的细粒度对象。

Flyweight，如果很多很小的对象它们有很多相同的东西，并且在很多地方用到，那就可以把它们抽取成一个对象，把不同的东西变成外部属性，作为方法的参数传入。

String类型的对象创建后就不可改变，如果两个String对象所包含的内容相同时，JVM只创建一个String对象对应这两个不同的对象引用。字符串常量池。

##### (三) 核心思想

###### 1、概念

> 运行共享技术有效地支持大量细粒度对象的复用。系统只使用少量的对象，而这些对象都很相似，状态变化很小，可以实现对象的多次复用。由于享元模式要求能够共享对象必须是细颗粒对象，因此它又称为轻量级模式，它是一种对象结构模式。

享元对象共享的关键是区分了内部状态(Intrinsic State)和外部状态(Extrinsic State)。

###### 2、内部状态(Intrinsic State)

> 存储在享元对象内部并且不会随环境改变而改变的状态，内部状态可以共享。

###### 3、外部状态(Extrinsic State)

> 享元对象的外部状态通常由客户端保存，并在享元对象被创建之后，需要使用的时候再传入到享元对象内部。随环境改变而改变的、不可以共享的状态。一个外部状态与另一个外状态是相互独立的。

由于区分了内部状态和外部状态，我们可以将具有相同内部状态的[对象存储](https://cloud.tencent.com/product/cos?from_column=20065&from=20065)在享元池中，享元池中的对象是可以实现共享的，需要的时候将对象从享元池中取出，实现对象的复用。通过向取出的对象注入不同的外部状态，可以得到一系列相似的对象，而这些对象在内存中实际上只存储一份。

##### (四) 享元模式分类

-   单纯享元模式
-   复合享元模式

###### 1、单纯享元模式结构重要核心模块

-   **抽象享元角色**：为具体享元角色规定了必须实现的方法，而外部状态时以参数的行贿通过此方法传入。在Java中可以由抽象类、接口担当
-   **具体享元角色**：实现抽象橘色规定的方法。如果存在内部状态，就负责为内部状态提供存储空间。
-   **享元端角色**\*：负责创建和管理享元角色。想要达到共享目的，这个角色的实现是关键。
-   **客户端角色**：维护对所有享元对象的引用，而且还需要存储对应的外部状态。

> 单纯享元模式和创新型的简单工厂模式实际上非常相似，但是它的重点或者用意却和工厂模式截然不同。工厂模式的使用主要是为了使用系统不依赖于实现的细节；而在享元模式的主要目的是避免大量拥有相同内容对象的开销。

###### 2、复合享元模式

-   **抽象享元角色**：为了具体享元角色规定了必须实现的方法，而外部状态就是以参数的形式听过此方法传入。在Java中可以由抽象类、接口来担当。
-   **具体享元角色**：实现抽象角色规定的方法。如果存在内部状态，就负责为内部状态提供存储空间。
-   **复合享元角色**：它所代表的对象是不可以共享的，并且可以分解为多个单纯享元对象的组合。
-   **享元工厂角色**：负责创建和管理享元角色。想要达到共享的目的，这个角色的实现是关键！
-   **客户端角色**：维护对所有享元对象的引用，而且还需要存储对应的外部状态。

##### (五) 享元模式的使用场景

一般在如下场景中使用享元模式

-   1 一个系统有大量相同或相似的对象，造成内存大量耗费。
-   2 对象大部分状态都可以外部化，可以将这些外部状态传入对象中。
-   3 再使用享元模式时需要维护一个存储享元对象的享元池，而这需要耗费一定的系统资源，因此，应该在需要多次重复使用享元对象时才值得使用享元模式。

##### 四、HandlerThread

[HandlerThread官网](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%253A%252F%252Fdeveloper.android.com%252Freference%252Fandroid%252Fos%252FHandlerThread.html&objectId=1199425&objectType=1&isNewArticle=undefined)

###### (一)、HandlerThread 简介

> 我们看到HandlerThread很快就会联想到Handler。Android中Handler的使用，一般都在UI线程中执行，因此在Handler接受消息后，处理消息时，不能做一些很耗时的操作，否则将出现ANR错误。Android中专门提供了HandlerThread类，来解决该类问题。HandlerThread是一个线程专门处理Handler的消息，依次从Handler的队列中获取信息，逐个进行处理，保证安全，不会出现混乱引发的异常。HandlerThread继承于Thread，所以它卑职就是一个Thread。与普通Thread的差别就在于，它有个Looper成员变量。

我们看下官方对它的简介：

> Handy class for starting a new thread that has a looper. The looper can then be used to create handler classes. Note that start() must still be called.

翻译一下:

> HandlerThread可以创建一个带有looper的线程。Looper对象可以用于创建Handler类来进行调度。

###### (二)、类源码解析

代码在[HandlerThread.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fandroid%252Fos%252FHandlerThread.java&objectId=1199425&objectType=1&isNewArticle=undefined)

```
/**
 * Handy class for starting a new thread that has a looper. The looper can then be 
 * used to create handler classes. Note that start() must still be called.
 */
public class HandlerThread extends Thread {
    // 线程的优先级 
    int mPriority;

    // 线程id
    int mTid = -1;

    // Looper对象，消息对象以及循环
    Looper mLooper;

    public HandlerThread(String name) {
        super(name);
        //设置默认的线程优先级
        mPriority = Process.THREAD_PRIORITY_DEFAULT;
    }
    
    /**
     * Constructs a HandlerThread.
     * @param name
     * @param priority The priority to run the thread at. The value supplied must be from 
     * {@link android.os.Process} and not from java.lang.Thread.
     */
    // 自定义设置线程优先级
    public HandlerThread(String name, int priority) {
        super(name);
        mPriority = priority;
    }
    
    /**
     * Call back method that can be explicitly overridden if needed to execute some
     * setup before Looper loops.
     */
    //如果有需要这个方法可以重写，例如可以在这里声明这个Handler关联此线程
    protected void onLooperPrepared() {
    }
    //Thread线程的run方法
    @Override
    public void run() {
        //获取当前线程的id
        mTid = Process.myTid();

        // 一旦调用这句代码，就在此线程中创建了Looper对象，这就是为什么我们要在调用线程start()方法后，才能得到Looper对象，即当调用Looper.myLooper()时不为null
        Looper.prepare();

        // 同步代码块，意思就是当获取mLooper对象后对象后，唤醒所有线程
        synchronized (this) {
            mLooper = Looper.myLooper();
            notifyAll();
        }
   
        // 设置线程优先级
        Process.setThreadPriority(mPriority);

        //调用上面的方法，需要用户重写
        onLooperPrepared();
        
        // 开启消息循环
        Looper.loop();
        mTid = -1;
    }
    
    /**
     * This method returns the Looper associated with this thread. If this thread not been started
     * or for any reason is isAlive() returns false, this method will return null. If this thread 
     * has been started, this method will block until the looper has been initialized.  
     * @return The looper.
     */
    public Looper getLooper() {
        // 如果线程已经死了，所以返回null
        if (!isAlive()) {
            return null;
        }
        
        // If the thread has been started, wait until the looper has been created.
        // 同步代码块，正好和上面(run()方法里面的)形成对应，就是说，只要线程活着并且我的looper为null，那么我就让你一直等
        synchronized (this) {
            while (isAlive() && mLooper == null) {
                try {
                    wait();
                } catch (InterruptedException e) {
                }
            }
        }
        return mLooper;
    }

    /**
     * Quits the handler thread's looper.
     * <p>
     * Causes the handler thread's looper to terminate without processing any
     * more messages in the message queue.
     * </p><p>
     * Any attempt to post messages to the queue after the looper is asked to quit will fail.
     * For example, the {@link Handler#sendMessage(Message)} method will return false.
     * </p><p class="note">
     * Using this method may be unsafe because some messages may not be delivered
     * before the looper terminates.  Consider using {@link #quitSafely} instead to ensure
     * that all pending work is completed in an orderly manner.
     * </p>
     *
     * @return True if the looper looper has been asked to quit or false if the
     * thread had not yet started running.
     *
     * @see #quitSafely
     */
    public boolean quit() {
        Looper looper = getLooper();
        if (looper != null) {
            // 退出消息循环
            looper.quit();
            return true;
        }
        return false;
    }

    /**
     * Quits the handler thread's looper safely.
     * <p>
     * Causes the handler thread's looper to terminate as soon as all remaining messages
     * in the message queue that are already due to be delivered have been handled.
     * Pending delayed messages with due times in the future will not be delivered.
     * </p><p>
     * Any attempt to post messages to the queue after the looper is asked to quit will fail.
     * For example, the {@link Handler#sendMessage(Message)} method will return false.
     * </p><p>
     * If the thread has not been started or has finished (that is if
     * {@link #getLooper} returns null), then false is returned.
     * Otherwise the looper is asked to quit and true is returned.
     * </p>
     *
     * @return True if the looper looper has been asked to quit or false if the
     * thread had not yet started running.
     */
    public boolean quitSafely() {
        Looper looper = getLooper();
        if (looper != null) {
            looper.quitSafely();
            return true;
        }
        return false;
    }

    /**
     * Returns the identifier of this thread. See Process.myTid().
     */
    // 返回线程ID
    public int getThreadId() {
        return mTid;
    }
}
```

> 整体来说上面代码还是比较浅显易懂的。主要作用是建立了一个线程，并且创建了消息队列，有自己的Looper，可以让我们在自己线程中分发和处理消息。

quit和quitSafely都是退出HandlerThread的消息循环，其分别调用Looper的quit和quitSafely方法。我们在这里简单说下区别:

-   1 quit方法会将消息队列中的所有消息移除(延迟消息和非延迟消息)。
-   2 quitSafely 会将消息队列所有延迟消息移除。非延迟消息则派发出去让Handler去处理。
-   quitSafely相比于quit方法安全之处在于清空消息之前会派发所有的非延迟消息。

###### (三)、HandlerThread的使用

通过上面的源码，我们大概也能推测出HandlerThread的使用步骤，它的使用步骤如下：

-   第一步：创建一个HandlerThread实例，本质是创建一个包含Looper的线程。比如

```
  HandlerThread handlerThread=new HandlerThread("name")
```

-   第二步：开启线程

-   第三步：获得线程Looper

```
 Looper looper=handlerThread.getLooper();
```

-   第四步：创建Handler，并用looper初始化

```
Handler  handler=new Handler(looper);
```

-   第五步：利用handle进行一些操作
-   第六步：调用quit()或者quitSafely()来终止它的循环

所以说整体流程如下：

> 当我们使用HandlerThrad创建一个线程，它start()之后会在它的线程创建一个Looper对象且初始化一个MessageQueue，通过Looper对象在他的线程构建一个Handler对象，然后我们通过Handler发送消息的形式将任务发送到MessaegQueue中，因为Looper是顺序处理消息的，所以当有多个任务存在时会顺序的排队执行。但我们不使用的时候我们应该调用它的quit()或者quitSafely()来终止它的循环。

###### (四)、HandlerThread和普通Thread的比较

> HandlerThread继承自Thread，当线程开启时，也就是它run方法运行起来后，线程同时创建了了一个含有消息队列的Looper，并对外提供自己这个Looper对象的get方法。

###### (五)、HandlerThread的使用场景

-   1、开发中如果多次使用类似new Thread(){...}.start()。这种方式开启一个子线程，会创建多个匿名线程，使得程序运行起来越来越慢，而HandlerThread自带Looper使他可以通过消息来多次重复使用当前线程，节省开支。
-   2、android系统提供的Handler类内部的Looper默认绑定的是UI线程的消息队列，对于非UI线程又想使用消息机制，那么HandlerThread内部的Looper是最合适的，它不会干扰或阻塞UI线程。
-   3、HandlerThread适合处理本地I/O读写操作(比如数据库)，因为本地I/O操作大多数的耗时属于毫秒级别的，对于单线程+异步队列的形式不会产生较大的阻塞。而网络操作相对于比较耗时，容易阻塞后面的请求，因此在这个HandlerThread中不合适加入网络操作。

###### (五) 小结：

-   1、HandlerThread将loop转到子线程中去处理，说白了就是分担MainLooper的工作量，降低了主线程压力，使主界面更流程。
-   2、开启一个线程起到多个线程的作用。处理任务是串行执行，按消息发送顺序进行处理。HandlerThread本质是一个线程，在线程内部，代码是串行处理的。但是由于每一个任务都将以队列的方式逐个被执行到，一旦队列中某个任务执行时间过长，那么就会导致后续的任务都会被延迟处理。HandlerThread拥有自己的消息队列，它不会干扰或阻塞UI线程。
-   3、对于网络I/O操作，HandlerThread并不合适，因为它只有一个线程，还得排队一个一个等着。

### 五、Handler的内存泄露

##### (一)、概述

android使用Java作为开发环境，Java的跨平台和垃圾回收机制已经帮助我们解决了底层的一些问题。但是尽管有了垃圾回收机制，在开发android的时候仍然时不时遇到out of memory的问题，这个时候我们不禁要问，垃圾回收器去哪里了？这里我们主要讲解handler引起的泄露，并且给出了几种解决方案，并且最后提供一个第三方库WeakHandler库。

可能导致泄漏问题的handler一般会被提示 Lint警告如下：

```
This Handler class should be static or leaks might occur 
```

意思是Handler class应该使用静态声明，否则可能会出现内存泄露 下面是更详细的说明(Android Studio，现在应该没人用Eclipse了吧)

> Since this Handler is declared as an inner class, it may prevent the outer class from being garbage collected. If the Handler is using a Looper or MessageQueue for a thread other than the main thread, then there is no issue. If the Handler is using the Looper or MessageQueue of the main thread, you need to fix your Handler declaration, as follows: Declare the Handler as a static class; In the outer class, instantiate a WeakReference to the outer class and pass this object to your Handler when you instantiate the Handler; Make all references to members of the outer class using the WeakReference object.

大概意思是：

> 一旦Handler 被声明为内部类，那么可能导致它的外部类不能够被垃圾回收，如果Handler在其他线程(我们通常称为工作线程(worker thread))使用Looper或MessageQueue(消息队列)，而不是main线程(UI线程)，那么久没有有这个问题。如果Handler使用Looper或MessageQueue在主线程(main thread)，你需要对Handler的声明做如下修改： 声明Handler为static类；在外部类实例化一个外部类的WeakReferernce(弱引用)并且在Handler初始化时传入这个对象给你的Handler；将所有引用的外部类成员使用WeakReference对象。

##### (二)、什么是内存泄露

> Java使用有向图机制，通过GC自动检查内存中的对象(什么时候检查由虚拟机决定)，如果GC发现一个或一组对象为不可到达状态，则将该对象从内存中回收。也就是说，一个对象不被任何应用所指向，则该对象会在被GC发现的时候被回收；另外，如果一组对象只包含相互的引用，没没有来自他们外部的引用(例如有两个对象A和B相互持有引用，但没有任何外部对象持有指向A或B的引用)，这让然属于不可叨叨，同样会被GC回收。

##### (三)、为什么会内存泄露

原因：

-   当Android应用启动的时候，会先创建一个应用主线程的Looper对象，Looper实现了一个简单的消息队列，一个一个的处理里面的Message对象。主线程Looper对象在整个应用生命周期中存在。
-   当在主线程中初始化Handler时，该Handler和Looper的消息队列关联。发送到消息的队列的Message会应用发送该消息的Handler对象，这样系统可以调用Handler.handleMessage(Message)来分发处理该消息。
-   在Java中，非静态(匿名)内部类会引用外部类对象。而静态内部类不会引用外部类对象。 -垃圾回收机制中约定，当内存中的一个对象的引用计数为0时，将会被回收。
-   如果外部类是Activity，则会引起Activity泄露。当Acitivity finish后，延时消息会继续存在主线程消息队列中1分钟，然后处理消息。而该消息引用了Activity的Handler对象，然后这个Handler又引用了这个Activity。这些引用对象会保持到该消息被处理完，这样就导致了该Activity对象无法被回收，从而导致了上面所说的Activity泄露。

所以说如果要修改该问题，只需要按照Lint提示那样，把Handler类定义为静态即可，然后通过WeakReference拉埃保持外部的Activity对象。

##### (四)、什么是WeakReference？

> WeakReference弱引用，与强引用(即我们常说的引用)相对，它的特点是，GC在回收时会忽略掉弱引用，即就算有弱引用指向某对象，但只要该对象没有被强引用所指向(实际上多数时候还要求没有软引用，但此处软件用的概念可以忽略)，该对象就会在被GC检查到时回收掉。对于上面的代码，用户在关闭Activity之后，就算后台线程还没有结束，但由于仅有一条来自Handler的弱引用指向Activity，所以GC仍然会在检查的时候把Activity回收掉。这样内存泄露的问题就不会出现了。

##### (五)、Handler内存泄露使用弱引用的补充

> 一般将Handler声明为static就不必造成内存泄露，声明成弱引用Activity的话，虽然也不会造成内存泄露，但是需要等到handler中没有执行任务后才会回收，因此性能不高。

所以说使用弱引用可以解决内存泄露，但是需要等到Handler中任务都执行完，才会释放activity内存，不如直接static释放的快。所以说单独使用弱引用性能不是太高。

##### (六)、WeakHandler

> **WeakHandler**使用起来和handler一样，但它是线程安全的，WeakHandler使用如下：

###### 1、WeakHandler的使用

```
public class ExampleActivity extends Activity {

    private WeakHandler mHandler; // We still need at least one hard reference to WeakHandler
    
    protected void onCreate(Bundle savedInstanceState) {
        mHandler = new WeakHandler();
        ...
    }
    
    private void onClick(View view) {
        mHandler.postDelayed(new Runnable() {
            view.setVisibility(View.INVISIBLE);
        }, 5000);
    }
}
```

你只需要将在以前的Handler替换成WeakHandler就行了。

###### 2、WeakHandler的原理

> WeakHandler的思想是将Handler和Runnable做一次封装，我们使用的是封装后的WeakHandler，但其实真正起到Handler作用的是封装的内部，而封装的内部对handler和runnable都是用的弱引用。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/mnkdnx05c7.png)

weakhandler.png

-   第一幅图是普通handler的引用关系图
-   第二幅图是使用WeakHandler的引用关系

其实原文有对WeakHandler跟多的解释，但是表述起来也是挺复杂的。

原文地址：[https://techblog.badoo.com/blog/2014/10/09/calabash-android-query/](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%253A%252F%252Ftechblog.badoo.com%252Fblog%252F2014%252F10%252F09%252Fcalabash-android-query%252F&objectId=1199425&objectType=1&isNewArticle=undefined) github项目地址：[https://github.com/badoo/android-weak-handler](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%253A%252F%252Fgithub.com%252Fbadoo%252Fandroid-weak-handler&objectId=1199425&objectType=1&isNewArticle=undefined)

### 六、Handler的面试题

###### 1、为什么安卓要使用Handler?

> 因为android更新UI只能在UI线程。为什么只能在UI线程更新UI？因为Android是单线程模型。为什么Android是单线程模型？那是因为如果任一线程都可以更新UI的话，线程安全处理起来相当麻烦，所以就规定了Android是单线程模型，只允许在UI线程更新UI

###### 2、消息机制的原理:

> 这个请参考 **本篇文章 二、Handler消息机制**

###### 3、MessageQueue是什么时候创建的？

> MessageQueue是在Looper的构造函数里面创建的，所以一个线程对应一个Looper，一个Looper对应一个MessageQueue。

###### 4、ThreadLocal在Handler机制中的作用

> ThreadLocal是一个线程内部的[数据存储](https://cloud.tencent.com/product/cdcs?from_column=20065&from=20065)类，通过它可以在制定的线程中存储数据，数据存储以后，只有在指定线程中可以获取的存储的数据，对于其他线程就获取不到数据。一般来说，当某些数据是以线程为作用域而且不同线程需要有不同的数据副本的时候，可以考虑用ThreadLocal。比如对于Handler，它要获取当前线程的Looper，很显然Looper的作用域就是线程，所以不同线程有不同的Looper。

###### 5、Looper,Handler,MessageQueue的引用关系?

> 一个Handler对象持有一个MessageQueue和它构造时所属的线程的Looper引用。也就是说一个Handler必须顶有它对应的消息队列和Looper。一个线程可能有多个Handler，但是至多有只能有一个Looper和一个消息队列。 在主线程中new了一个Handler对象后，这个Handler对象自动和主线程生成的Looper以及消息队列关联上了。子线程中拿到主线程中Handler的引用，发送消息后，消息对象就会发送到target属性对应的的那个Handler对应的消息队列中去，由对应Looper来处理(子线程msg->主线程handler->主线程messageQueue->主线程Looper->主线程Handler的handlerMessage)。而消息发送到主线程Handler，那么也就是发送到主线程的消息队列，用主线程的Looper轮询。

###### 6、MessageQueue里面的数据结构是什么类型的，为什么不是Map或者其他什么类型的?

> 这个请参考 **本片文章 一、Handler机制的思考**

###### 7、Handler引起的内存泄漏以及解决办法

> 这个请参考 **本片文章 五、Handler的内存泄露**

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2017.09.21 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除