### 一、消息种类

**关于Handler机制的基本原理不了解可以看这里**： [Handler机制源码解析](https://blog.csdn.net/start_mao/article/details/82466672)。

Message分为3种：普通消息（同步消息）、屏障消息（同步屏障）和异步消息。我们通常使用的都是普通消息，而屏障消息就是在消息队列中插入一个屏障，在屏障之后的所有普通消息都会被挡着，不能被处理。不过异步消息却例外，屏障不会挡住异步消息，因此可以这样认为：屏障消息就是为了确保异步消息的优先级，设置了屏障后，只能处理其后的异步消息，同步消息会被挡住，除非撤销屏障。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b3b58b44da3ddcbe018fcb0a477fd37c.png)

### 二、什么是屏障消息

同步屏障是通过MessageQueue的postSyncBarrie[r方](https://so.csdn.net/so/search?q=r%E6%96%B9&spm=1001.2101.3001.7020)法插入到消息队列的。

```
MessageQueue#postSyncBarrier
 private int postSyncBarrier(long when) {
        synchronized (this) {
            final int token = mNextBarrierToken++;
            //1、屏障消息和普通消息的区别是屏障消息没有tartget。
            final Message msg = Message.obtain();
            msg.markInUse();
            msg.when = when;
            msg.arg1 = token;

            Message prev = null;
            Message p = mMessages;
            //2、根据时间顺序将屏障插入到消息链表中适当的位置
            if (when != 0) {
                while (p != null && p.when <= when) {
                    prev = p;
                    p = p.next;
                }
            }
            if (prev != null) { // invariant: p == prev.next
                msg.next = p;
                prev.next = msg;
            } else {
                msg.next = p;
                mMessages = msg;
            }
            //3、返回一个序号，通过这个序号可以撤销屏障
            return token;
        }
    }
```

postSyncBarrier方法就是用来插入一个屏障到消息队列的，可以看到它很简单，从这个方法我们可以知道如下：

- 屏障消息和普通消息的区别在于屏障没有tartget，普通消息有target是因为它需要将消息分发给对应的target，而屏障不需要被分发，它就是用来挡住普通消息来保证异步消息优先处理的。
- 屏障和普通消息一样可以根据时间来插入到消息队列中的适当位置，并且只会挡住它后面的同步消息的分发。
- postSyncBarrier返回一个int类型的数值，通过这个数值可以撤销屏障。
- 插入普通消息会唤醒消息队列，但是插入屏障不会。

### 三、屏障消息的工作原理

通过postSyncBarrier方法屏障就被插入到消息队列中了，那么屏障是如何挡住普通消息只允许异步消息通过的呢？我们知道MessageQueue是通过next方法来获取消息的。

```
Message next() {
			//1、如果有消息被插入到消息队列或者超时时间到，就被唤醒，否则阻塞在这。
            nativePollOnce(ptr, nextPollTimeoutMillis);

            synchronized (this) {
                Message prevMsg = null;
                Message msg = mMessages;
                if (msg != null && msg.target == null) {//2、遇到屏障  msg.target == null
                    do {
                        prevMsg = msg;
                        msg = msg.next;
                    } while (msg != null && !msg.isAsynchronous());//3、遍历消息链表找到最近的一条异步消息
                }
                if (msg != null) {
                	//4、如果找到异步消息
                    if (now < msg.when) {//异步消息还没到处理时间，就在等会（超时时间）
                        nextPollTimeoutMillis = (int) Math.min(msg.when - now, Integer.MAX_VALUE);
                    } else {
                        //异步消息到了处理时间，就从链表移除，返回它。
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
                    // 如果没有异步消息就一直休眠，等待被唤醒。
                    nextPollTimeoutMillis = -1;
                }
			//。。。。
        }
    }
```

可以看到，在注释2如果碰到屏障就遍历整个消息链表找到最近的一条异步消息，在遍历的过程中只有异步消息才会被处理执行到 if (msg != null){}中的代码。可以看到通过这种方式就挡住了所有的普通消息。

### 四、如何发送异步消息

Handler有几个构造方法，可以传入async标志为true，这样构造的Handler发送的消息就是异步消息。不过可以看到，这些构造函数都是hide的。

```markdown
     /**
      * @hide
      */
    public Handler(boolean async) {}

    /**
     * @hide
     */
    public Handler(Callback callback, boolean async) { }

    /**
     * @hide
     */
    public Handler(Looper looper, Callback callback, boolean async) {}

```

当调用handler.sendMessage(msg)发送消息，最终会走到：

```
private boolean enqueueMessage(MessageQueue queue, Message msg, long uptimeMillis) {
        msg.target = this;
        if (mAsynchronous) {
            msg.setAsynchronous(true);//把消息设置为异步消息
        }
        return queue.enqueueMessage(msg, uptimeMillis);
    }
```

可以看到如果这个handler的mAsynchronous为true就把消息设置为异步消息，设置异步消息其实也就是设置msg内部的一个标志。而这个mAsynchronous就是构造handler时传入的async。除此之外，还有一个公开的方法：

```
	    Message message=Message.obtain();
        message.setAsynchronous(true);
        handler.sendMessage(message);
123
```

在发送消息时通过 message.setAsynchronous(true)将消息设为异步的，这个方法是公开的，我们可以正常使用。

### 五、移除屏障

移除屏障可以通过MessageQueue的removeSyncBarrier方法：

``` java
//注释已经写的很清楚了，就是通过插入同步屏障时返回的token 来移除屏障
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
            Message p = mMessages;
            //找到token对应的屏障
            while (p != null && (p.target != null || p.arg1 != token)) {
                prev = p;
                p = p.next;
            }
            final boolean needWake;
            //从消息链表中移除
            if (prev != null) {
                prev.next = p.next;
                needWake = false;
            } else {
                mMessages = p.next;
                needWake = mMessages == null || mMessages.target != null;
            }
            //回收这个Message到对象池中。
            p.recycleUnchecked();
			// If the loop is quitting then it is already awake.
            // We can assume mPtr != 0 when mQuitting is false.
            if (needWake && !mQuitting) {
                nativeWake(mPtr);//唤醒消息队列
            }
    }

```

### 六、实战

1、当点击同步消息会发送一个延时1秒执行普通消息，执行的结果打印log。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/72aabd8024a8f98c30c448e22e3ac878.gif)  
2、同步屏障会挡住同步消息。通过点击发送同步屏障->发送同步消息->移除同步消息测试  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/cbbcafb59c2618cc5357c51d2fb14772.gif)  
3、当点击发送同步屏障，会挡住同步消息，但是不会挡住异步消息。通过点击插入同步屏障->插入同步消息->插入异步消息->移除同步屏障 来测试（需要注意不要通过弹土司来测试，通过打印log。不然看不出效果）  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b319aba0741111d7a91495cba2839a3c.gif)  
测试代码如下（省略布局文件）：

```javascript
public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private Handler handler;
    private int token;

    public static final int MESSAGE_TYPE_SYNC=1;
    public static final int MESSAGE_TYPE_ASYN=2;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        initHandler();
        initListener();
    }

    private void initHandler() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                Looper.prepare();
                handler=new Handler(){
                    @Override
                    public void handleMessage(Message msg) {
                        if (msg.what == MESSAGE_TYPE_SYNC){
                            Log.d("MainActivity","收到普通消息");
                        }else if (msg.what == MESSAGE_TYPE_ASYN){
                            Log.d("MainActivity","收到异步消息");
                        }
                    }
                };
                Looper.loop();
            }
        }).start();
    }

    private void initListener() {
        findViewById(R.id.btn_postSyncBarrier).setOnClickListener(this);
        findViewById(R.id.btn_removeSyncBarrier).setOnClickListener(this);
        findViewById(R.id.btn_postSyncMessage).setOnClickListener(this);
        findViewById(R.id.btn_postAsynMessage).setOnClickListener(this);
    }

    //往消息队列插入同步屏障
    @RequiresApi(api = Build.VERSION_CODES.M)
    public void sendSyncBarrier(){
        try {
            Log.d("MainActivity","插入同步屏障");
            MessageQueue queue=handler.getLooper().getQueue();
            Method method=MessageQueue.class.getDeclaredMethod("postSyncBarrier");
            method.setAccessible(true);
            token= (int) method.invoke(queue);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    //移除屏障
    @RequiresApi(api = Build.VERSION_CODES.M)
    public void removeSyncBarrier(){
        try {
            Log.d("MainActivity","移除屏障");
            MessageQueue queue=handler.getLooper().getQueue();
            Method method=MessageQueue.class.getDeclaredMethod("removeSyncBarrier",int.class);
            method.setAccessible(true);
            method.invoke(queue,token);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    //往消息队列插入普通消息
    public void sendSyncMessage(){
        Log.d("MainActivity","插入普通消息");
        Message message= Message.obtain();
        message.what=MESSAGE_TYPE_SYNC;
        handler.sendMessageDelayed(message,1000);
    }

    //往消息队列插入异步消息
    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP_MR1)
    private void sendAsynMessage() {
        Log.d("MainActivity","插入异步消息");
        Message message=Message.obtain();
        message.what=MESSAGE_TYPE_ASYN;
        message.setAsynchronous(true);
        handler.sendMessageDelayed(message,1000);
    }

    @RequiresApi(api = Build.VERSION_CODES.M)
    @Override
    public void onClick(View v) {
        int id=v.getId();
        if (id == R.id.btn_postSyncBarrier) {
            sendSyncBarrier();
        }else if (id == R.id.btn_removeSyncBarrier) {
            removeSyncBarrier();
        }else if (id == R.id.btn_postSyncMessage) {
            sendSyncMessage();
        }else if (id == R.id.btn_postAsynMessage){
            sendAsynMessage();
        }
    }

}

123456789101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960616263646566676869707172737475767778798081828384858687888990919293949596979899100101102103104105106
```
