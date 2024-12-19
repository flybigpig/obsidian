本片文章的主要内容下：

-   1、Message和MessageQueue类注释
-   2、获取Message成员变量解析
-   3、获取Message对象
-   4、Message的消息对象池和无参的obtain()方法
-   5、obtain()有参函数解析
-   6、Message的 浅拷贝

### 一、 Message和MessageQueue类注释

> 为了让大家更好的理解谷歌团队设计这个两个类Message和MessageQueue的意图，我们还是从这个两个类的类注释开始

##### (一) Message.java

[Message](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fdeveloper.android.com%2Freference%2Fandroid%2Fos%2FMessage.html&objectId=1199403&objectType=1&isNewArticle=undefined)

> Defines a message containing a description and arbitrary data object that can be sent to a Handler. This object contains two extra int fields and an extra object field that allow you to not do allocations in many cases. While the constructor of Message is public, the best way to get one of these is to call Message.obtain() or one of the Handler.obtainMessage() methods, which will pull them from a pool of recycled objects.

翻译一下：

> 定义一个可以发送给Handler的描述和任意数据对象的消息。此对象包含两个额外的int字段和一个额外的对象字段，这样就可以使用在很多情况下不用做分配工作。 尽管Message的构造器是公开的，但是获取Message对象的最好方法是调用Message.obtain()或者Handler.obtainMessage()，这样是从一个可回收的对象池中获取Message对象。

至此Java层面Handler机制中最重要的四个类大家有了一个初步印象。下面咱们源码跟踪一下

### 二、获取Message成员变量解析

##### (一) 成员变量 what

> Message用一个标志来区分不同消息的身份，不同的Handler使用相同的消息不会弄混，一般使用16进制形式来表示，阅读起来比较容易

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 39行

```
    /**
     * User-defined message code so that the recipient can identify 
     * what this message is about. Each {@link Handler} has its own name-space
     * for message codes, so you do not need to worry about yours conflicting
     * with other handlers.
     */
    public int what;
```

注释翻译：

> 用户定义的Message的标识符用以分辨消息的内容。Hander拥有自己的消息代码的命名空间，因此你不用担心与其他的Handler冲突。

##### (二) 成员变量 arg1和arg2

> arg1和arg2都是Message类的可选变量，可以用来存放两个整数值，不用访问obj对象就能读取的变量。

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 46行

```
    /**
     * arg1 and arg2 are lower-cost alternatives to using
     * {@link #setData(Bundle) setData()} if you only need to store a
     * few integer values.
     */
    public int arg1; 

    /**
     * arg1 and arg2 are lower-cost alternatives to using
     * {@link #setData(Bundle) setData()} if you only need to store a
     * few integer values.
     */
    public int arg2;
```

因为两个注释一样，我就不重复翻译了，翻译如下：

> 如果你仅仅是保存几个整形的数值，相对于使用setData()方法，使用arg1和arg2是较低成本的替代方案。

##### (三) 成员变量 obj

> obj 用来保存对象，接受纤细后取出获得传送的对象。

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 65行

```
    /**
     * An arbitrary object to send to the recipient.  When using
     * {@link Messenger} to send the message across processes this can only
     * be non-null if it contains a Parcelable of a framework class (not one
     * implemented by the application).   For other data transfer use
     * {@link #setData}.
     * 
     * <p>Note that Parcelable objects here are not supported prior to
     * the {@link android.os.Build.VERSION_CODES#FROYO} release.
     */
    public Object obj;
```

注释翻译：

-   将一个独立的对象发送给接收者。当使用Messenger去送法消息，并且这个对象包含Parcelable类的时候，它必须是非空的。对于其他数据的传输，建议使用setData()方法
-   请注意，在Android系统版本FROYO(2.2)之前不支持Parcelable对象。

##### (四) 其他成员变量

其他变量我就不一一解释了，大家就直接看注释吧

```
    // 回复跨进程的Messenger 
    public Messenger replyTo;

    // Messager发送这的Uid
    public int sendingUid = -1;

    // 正在使用的标志值 表示当前Message 正处于使用状态，当Message处于消息队列中、处于消息池中或者Handler正在处理Message的时候，它就处于使用状态。
    /*package*/ static final int FLAG_IN_USE = 1 << 0;

    // 异步标志值 表示当前Message是异步的。
    /*package*/ static final int FLAG_ASYNCHRONOUS = 1 << 1;

    // 消息标志值 在调用copyFrom()方法时，该常量将会被设置，其值其实和FLAG_IN_USE一样
    /*package*/ static final int FLAGS_TO_CLEAR_ON_COPY_FROM = FLAG_IN_USE;

    // 消息标志，上面三个常量 FLAG 用在这里
    /*package*/ int flags;

    // 用于存储发送消息的时间点，以毫秒为单位
    /*package*/ long when;

    // 用于存储比较复杂的数据
    /*package*/ Bundle data;
    
    // 用于存储发送当前Message的Handler对象，前面提到过Handler其实和Message相互持有引用的
    /*package*/ Handler target;
    
    // 用于存储将会执行的Runnable对象，前面提到过除了handlerMessage(Message msg)方法，你也可以使用Runnable执行操作，要注意的是这种方法并不会创建新的线程。
    /*package*/ Runnable callback;
 
    // 指向下一个Message，也就是线程池其实是一个链表结构
    /*package*/ Message next;

    // 该静态变量仅仅是为了给同步块提供一个锁而已
    private static final Object sPoolSync = new Object();

    //该静态的Message是整个线程池链表的头部，通过它才能够逐个取出对象池的Message
    private static Message sPool;

    // 该静态变量用于记录对象池中的Message的数量，也就是链表的长度
    private static int sPoolSize = 0;
  
    // 设置了对象池中的Message的最大数量，也就是链表的最大长度
    private static final int MAX_POOL_SIZE = 50;

     //该版本系统是否支持回收标志位
    private static boolean gCheckRecycle = true;
```

### 三、获取Message对象

##### (一)、Message构造函数

如果想获取Message对象，大家第一印象肯定是找Message的构造函数，那我们就来看下Message的构造函数。代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 475行

```
    /** Constructor (but the preferred way to get a Message is to call {@link #obtain() Message.obtain()}).
    */
    public Message() {
    }
```

发现代码里面什么都没有

那我们看下注释，简单翻译一下：

> 构造函数，但是获取Message的首选方法是通过Message.obtain()来调用

其实在上面解释Message的注释时也是这样说的，说明Android官方团队是推荐使用Message.obtain()方法来获取Message对象的，那我们就来看下Message.obtain()

##### (二)、Message.obtain()方法

我们来看下[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 类的结构图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/78b87urx7j.png)

Message结构图.png

Message.居然有8个obtain函数 为了方便后续的跟踪，也将这8个方法编号，分别如下

-   ① public static Message obtain()
-   ② public static Message obtain(Message orig)
-   ③ public static Message obtain(Handler h)
-   ④ public static Message obtain(Handler h, Runnable callback)
-   ⑤ public static Message obtain(Handler h, int what)
-   ⑥ public static Message obtain(Handler h, int what, Object obj)
-   ⑦ public static Message obtain(Handler h, int what, int arg1, int arg2)
-   ⑧ public static Message obtain(Handler h, int what, int arg1, int arg2, Object obj)

这里我们也像Handler一样，分为两大类

-   **无参的obtain()方法**
-   **有参的obtain()方法**

在讲解无参的obtain()的时候很有必要先了会涉及一个概念**“Message对象池”**，所以我们就合并一起讲解了

### 四、Message的消息对象池和无参的obtain()方法

先来看一下下面 无参的obtain()方法的代码

###### 1、① public static Message obtain(Message orig)

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 122行

```
    /**
     * Return a new Message instance from the global pool. Allows us to
     * avoid allocating new objects in many cases.
     */
    public static Message obtain() {
       // 保证线程安全
        synchronized (sPoolSync) {
            if (sPool != null) {
                Message m = sPool;
                sPool = m.next;
                m.next = null;
                 // flags为移除使用标志
                m.flags = 0; // clear in-use flag
                sPoolSize--;
                return m;
            }
        }
        return new Message();
    }
```

老规矩，先看下注释，翻译如下：

> 从全局的pool返回一个实例化的Message对象。这样可以避免我们重新创建冗余的对象。

等等上面提到了一个名词pool，我们通常理解为"池"，我们看到源代码的里面有一个变量是"sPool"，那么"sPool"，这里面就涉及到了Message的设计原理了，在Message里面是有一个"对象池"，下面我们就详细了解下

###### 2、sPool

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 111行

```
   private static Message sPool;
```

-   好像也没什么嘛？就是一个Message对象而已，所以sPool默认是null。
-   这时候我们再来看上面**public static Message obtain()**方法，我们发现**public static Message obtain()**就是直接new了Message 直接返回而已。好像很简单的样子，大家心里肯定感觉"咱们是不是忽略了什么？"
-   是的，既然官方是不推荐使用new Message的，因为这样可能会重新创建冗余的对象。所以我们推测大部分情况下sPool是不为null的。那我们就反过来看下，来全局找下sPool什么时候被赋值的

我们发现除了**public static Message obtain()**里面的**if (sPool != null) {}**里面外，还有**recycleUnchecked**给sPool赋值了。那我们就来看下这个方法

###### 3、recycleUnchecked()

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 291行

```
   /**
     * Recycles a Message that may be in-use.
     * Used internally by the MessageQueue and Looper when disposing of queued Messages.
     */
    void recycleUnchecked() {
        // Mark the message as in use while it remains in the recycled object pool.
        // Clear out all other details.
        // 添加正在使用标志位，其他情况就除掉
        flags = FLAG_IN_USE;
        what = 0;
        arg1 = 0;
        arg2 = 0;
        obj = null;
        replyTo = null;
        sendingUid = -1;
        when = 0;
        target = null;
        callback = null;
        data = null;
        //拿到同步锁，以避免线程不安全
        synchronized (sPoolSync) {
            if (sPoolSize < MAX_POOL_SIZE) {
                next = sPool;
                sPool = this;
                sPoolSize++;
            }
        }
    }
```

###### 4、消息对象池的理解

为了让大家更好的理解，我把recycleUnchecked()和obtain()合在一起，省略一些不重要的代码 代码如下：

```
void recycleUnchecked() {
                ...
        if (sPoolSize < MAX_POOL_SIZE) {
                // 第一步
                next = sPool;
                 // 第二步
                sPool = this;
                 // 第三步
                sPoolSize++;
                 ...
         }
    }

public static Message obtain() {
    synchronized (sPoolSync) {
        //第一步
        if (sPool != null) {
            // 第二步
            Message m = sPool;
            // 第三步
            sPool = m.next;
            // 第四步
            m.next = null;
            // 第五步
            m.flags = 0; 
            // 第六步
            sPoolSize--;
            return m;
        }
    }
}
```

###### 4.1、recycleUnchecked()的理解

假设消息对象池为空，从new message开始，到这个message被取出使用后，准备回收 先来看**recycleUnchecked()**方法

-   第一步，**next=sPool**，因为消息对象池为空，所以此时sPool为null，同时next也为null。
-   第二步，**spool = this**，将当前这个message作为消息对象池中下一个被复用的对象。
-   第三步，**sPoolSize++**，默认为0，此时为1，将消息对象池的数量+1，这个数量依然是全系统共共享的。

这时候假设又调用了，这个方法，之前的原来的第一个Message对象我假设定位以为**message1**，依旧走到上面的循环。

-   第一步，**next=sPool**，因为消息对象池为message1，所以此时sPool为message1，同时next也为message1。
-   第二步，**sPool = this**，将当前这个message作为消息对象池中下一个被复用的对象。
-   第三步，**sPoolSize++**，此时为1，将消息对象池的数量+1，sPoolSize为2，这个数量依然是全系统共共享的。

以此类推，直到sPoolSize=50(MAX\_POOL\_SIZE = 50)

###### 4.2、obtain()的理解

假设上面已经回收了一个Message对象，又从这里获取一个message，看看会发生什么？

-   第一步，判断**sPool**是否为空，如果消息对象池为空，则直接new Message并返回
-   第二步，**Message m = sPool**，将消息对象池中的对象取出来，为m。
-   第三步，**sPool = m.next**，将消息对象池中的下一个可以复用的Message对象(m.next)赋值为消息对象池中的当前对象。(如果消息对象池就之前就一个，则此时sPool=null)
-   第四步，将m.next置为null，因为之前已经把这个对象取出来了，所以无所谓了。
-   第五步，**m.flags = 0**，设置m的标记位，标记位正在被使用
-   第六步，**sPoolSize--**，因为已经把m取出了，这时候要把消息对象池的容量减一。

###### 4.3、深入理解消息对象池

> 上面的过程主要讨论只有一个message的情况，详细解释一下sPool和next，将sPool看成一个指针，通过next来将对象组成一个链表，因为每次只需要从池子里拿出一个对象，所以不需要关心池子里具体有多少个对象，而是拿出当前这个sPool所指向的这个对象就可以了，sPool从思路上理解就是通过左右移动来完成复用和回收

###### 4.3.1、obtain()复用

如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/su2evb9kdy.png)

obtain()复用1.png

> 当移动Obtain()的时候，让sPool=next，因此第一个message.next就等于第二个message，从上图看相当于指针向后移动了一位，随后会将第一个message.next的值置为空。如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/i3fabx3050.png)

obtain()复用2.png

现在这个链表看上去就断了，如果in-use这个message使用完毕了，怎么回到链表中？这就是recycleUnchecked() – 回收了

###### 4.3.2、recycleUnchecked()回收

> 这时候在看下recycleUnchecked()里面的代码，next ＝ sPool，将当前sPool所指向的message对象赋值给in－use的next，然后sPool=this，将sPool指向第一个message对象。如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/4u91yyql64.png)

ecycleUnchecked()回收.png

这样，就将链表恢复了，而且不管是复用还是回收大欧式保证线程同步的，所以始终会形成一条链式结构。

如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/1zh8fgo41q.png)

回收.png

### 五、obtain()有参函数解析

##### (一)、② public static Message obtain(Message orig)

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 142行

```
   /**
     * Same as {@link #obtain()}, but copies the values of an existing
     * message (including its target) into the new one.
     * @param orig Original message to copy.
     * @return A Message object from the global pool.
     */
    public static Message obtain(Message orig) {
        Message m = obtain();
        m.what = orig.what;
        m.arg1 = orig.arg1;
        m.arg2 = orig.arg2;
        m.obj = orig.obj;
        m.replyTo = orig.replyTo;
        m.sendingUid = orig.sendingUid;
        if (orig.data != null) {
            m.data = new Bundle(orig.data);
        }
        m.target = orig.target;
        m.callback = orig.callback;

        return m;
    }
```

先翻译注释：

> 和obtain()一样，但是将message的所有内容复制一份到新的消息中。

看代码我们知道首先调用obtain()从消息对象池中获取一个Message对象m，然后把orig中的所有属性赋值给m。

##### (二)、③ public static Message obtain(Handler h)

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 164行

```
     /**
     * Same as {@link #obtain()}, but sets the value for the <em>target</em> member on the Message returned.
     * @param h  Handler to assign to the returned Message object's <em>target</em> member.
     * @return A Message object from the global pool.
     */
    public static Message obtain(Handler h) {
        Message m = obtain();
        m.target = h;

        return m;
    }
```

先翻译注释：

> 和obtain()一样，但是成员变量中的target的值用以指定的值(入参)来替换。

代码很简单就是调用obtain()从消息对象池中获取一个Message对象m，然后将m的target重新赋值而已。

##### (三)、④ public static Message obtain(Handler h, Runnable callback)

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 178行

```
    /**
     * Same as {@link #obtain(Handler)}, but assigns a callback Runnable on
     * the Message that is returned.
     * @param h  Handler to assign to the returned Message object's <em>target</em> member.
     * @param callback Runnable that will execute when the message is handled.
     * @return A Message object from the global pool.
     */
    public static Message obtain(Handler h, Runnable callback) {
        Message m = obtain();
        m.target = h;
        m.callback = callback;

        return m;
    }
```

先翻译注释：

> 和obtain()一样，但是成员变量中的target的值用以指定的值(入参)来替换，并且添加一个回调的Runnable

代码很简单就是调用obtain()从消息对象池中获取一个Message对象m，然后将m的target和m的callback重新赋值而已。

##### (四)、⑤ public static Message obtain(Handler h, int what)

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 193行

```
    /**
     * Same as {@link #obtain()}, but sets the values for both <em>target</em> and
     * <em>what</em> members on the Message.
     * @param h  Value to assign to the <em>target</em> member.
     * @param what  Value to assign to the <em>what</em> member.
     * @return A Message object from the global pool.
     */
    public static Message obtain(Handler h, int what) {
        Message m = obtain();
        m.target = h;
        m.what = what;

        return m;
    }
```

先翻译注释：

> 和obtain()一样，但是重置了成员变量target和what的值。

代码很简单就是调用obtain()从消息对象池中获取一个Message对象m，然后将m的target和m的what重新赋值而已。

##### (五)、public static Message obtain(Handler h, int what, Object obj)

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 209行

```
    /**
     * Same as {@link #obtain()}, but sets the values of the <em>target</em>, <em>what</em>, and <em>obj</em>
     * members.
     * @param h  The <em>target</em> value to set.
     * @param what  The <em>what</em> value to set.
     * @param obj  The <em>object</em> method to set.
     * @return  A Message object from the global pool.
     */
    public static Message obtain(Handler h, int what, Object obj) {
        Message m = obtain();
        m.target = h;
        m.what = what;
        m.obj = obj;

        return m;
    }
```

先翻译注释：

> 和obtain()一样，但是重置了成员变量target、what、obj的值。

代码很简单就是调用obtain()从消息对象池中获取一个Message对象m，然后将m的target、what和obj这三个成员变量重新赋值而已。

##### (六)、⑦ public static Message obtain(Handler h, int what, int arg1, int arg2)

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 228行

```
    /**
     * Same as {@link #obtain()}, but sets the values of the <em>target</em>, <em>what</em>, 
     * <em>arg1</em>, and <em>arg2</em> members.
     * 
     * @param h  The <em>target</em> value to set.
     * @param what  The <em>what</em> value to set.
     * @param arg1  The <em>arg1</em> value to set.
     * @param arg2  The <em>arg2</em> value to set.
     * @return  A Message object from the global pool.
     */
    public static Message obtain(Handler h, int what, int arg1, int arg2) {
        Message m = obtain();
        m.target = h;
        m.what = what;
        m.arg1 = arg1;
        m.arg2 = arg2;

        return m;
    }
```

先翻译注释：

> 和obtain()一样，但是重置了成员变量target、what、arg1、arg2的值。

代码很简单就是调用obtain()从消息对象池中获取一个Message对象m，然后将m的target、what、arg1、arg2这四个成员变量重新赋值而已。

##### (七)、⑧ public static Message obtain(Handler h, int what, int arg1, int arg2, Object obj)

代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 249行

```
    /**
     * Same as {@link #obtain()}, but sets the values of the <em>target</em>, <em>what</em>, 
     * <em>arg1</em>, <em>arg2</em>, and <em>obj</em> members.
     * 
     * @param h  The <em>target</em> value to set.
     * @param what  The <em>what</em> value to set.
     * @param arg1  The <em>arg1</em> value to set.
     * @param arg2  The <em>arg2</em> value to set.
     * @param obj  The <em>obj</em> value to set.
     * @return  A Message object from the global pool.
     */
    public static Message obtain(Handler h, int what, 
            int arg1, int arg2, Object obj) {
        Message m = obtain();
        m.target = h;
        m.what = what;
        m.arg1 = arg1;
        m.arg2 = arg2;
        m.obj = obj;

        return m;
    }
```

先翻译注释：

> 和obtain()一样，但是重置了成员变量target、what、arg1、arg2、obj的值。

代码很简单就是调用obtain()从消息对象池中获取一个Message对象m，然后将m的target、what、arg1、arg2、obj这五个成员变量重新赋值而已。

##### (八)、总结

> 我们发现 上面有参的obtain()方法里面第一行代码都是 Message m = obtain();，所以有参的obtain()的方法的本质都是调用无参的obtain()方法，只不过有参的obtain()可以通过入参来重置一些成员变量的值而已

### 六、Message的 浅拷贝

Message的浅拷贝 就是copyFrom(Message o)函数 代码在[Message.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessage.java&objectId=1199403&objectType=1&isNewArticle=undefined) 320行

```
    /**
     * Make this message like o.  Performs a shallow copy of the data field.
     * Does not copy the linked list fields, nor the timestamp or
     * target/callback of the original message.
     */
    public void copyFrom(Message o) {
        this.flags = o.flags & ~FLAGS_TO_CLEAR_ON_COPY_FROM;
        this.what = o.what;
        this.arg1 = o.arg1;
        this.arg2 = o.arg2;
        this.obj = o.obj;
        this.replyTo = o.replyTo;
        this.sendingUid = o.sendingUid;

        if (o.data != null) {
            this.data = (Bundle) o.data.clone();
        } else {
            this.data = null;
        }
    }
```

先翻译注释：

> 把这个Message做成像o一样。执行数据字段的浅拷贝，不复制字段连接，也不赋值目标的Handler和callback回调。

其实从本质上看就是从一个消息体复制到另一个消息体。有时候你可能需要用到Message的拷贝功能，也就是说拷贝一个和Message 一模一样的B，这时候你可以使用copyFrom(Message o) 拷贝一个Message对象。

有时候你可能需要用到Message的拷贝功能，也就是说拷贝一个和Message A一模一样的B，这时候你可以使用copyFrom(Message o)方法来浅拷贝一个Message对象。从代码中可以看出大部分数据确实都是浅拷贝，但是对于data这个Bundle类型的成员变量却进行了深拷贝，所以说该方法是一个浅拷贝方法感觉也不是很贴切。

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2017.09.14 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除