# upStairsMore

### HandlerThread 

是Thread 的一个子类。HandlerThread自带Looper使它可以通过消息队列来重复使用当前线程。节省系统资源开销。这是它的优点和缺点。每一个任务都将以队列的方式逐个被执行到。一旦队列中某个任务执行时间过长。那么就导致后续的任务被延迟处理。它的使用也就比较简单。

```
HandlerThread thread = new HandlerThread("MyHandlerThread");
thread.start();
//Looper在子线程中的handler
mHandler = new Handler(thread.getLooper());
//mHandler.post(new Runnable(){...});
```



```
public class MainActivity extends AppCompatActivity {
    HandlerThread mCheckMsgThread;
    boolean isUpdateInfo = true;
    private int mCount = 0;
    private static final int MSG_UPDATE_INFO = 0x110;

    //与UI线程管理的handler
    private Handler mHandler = new Handler();
    private Handler mCheckMsgHandler;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        initBackThread();
    }

    private void initBackThread()
    {
    	// 新建HandlerThread并启动
        mCheckMsgThread = new HandlerThread("check-message-coming");
        mCheckMsgThread.start();  //1 
     
        
        
        // 创建Handler
        mCheckMsgHandler = new Handler( mCheckMsgThread.getLooper())  // 2
        {
            @Override
            public void handleMessage(Message msg) {
                //Thread.sleep(1000) 模拟耗时操作
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        ((TextView)findViewById(R.id.id_txt)).setText((mCount++)+"");
                    }
                });
                if(isUpdateInfo)
                    mCheckMsgHandler.sendEmptyMessage(MSG_UPDATE_INFO);

            }
        };


    }

    @Override
    protected void onPause() {
        super.onPause();
        isUpdateInfo = false;
        mCheckMsgHandler.removeMessages(MSG_UPDATE_INFO);
    }

    @Override
    protected void onResume() {
        super.onResume();
        isUpdateInfo = true;
        mCheckMsgHandler.sendEmptyMessage(MSG_UPDATE_INFO);

    }
    @Override
    protected void onDestroy() {
        super.onDestroy();
        mCheckMsgThread.quit();
    }
}
```

###### 1

> 看到了什么，其实我们就是初始化和启动了一个线程；然后我们看run()方法，可以看到该方法中调用了Looper.prepare()，Loop.loop();
>
> prepare()中创建了一个Looper对象，并且把该对象放到了该线程范围内的变量中（sThreadLocal），在Looper对象的构造过程中，初始化了一个MessageQueue，作为该Looper对象成员变量。
>
> loop()就开启了，不断的循环从MessageQueue中取消息处理了，当没有消息的时候会阻塞，有消息的到来的时候会唤醒。

###### 2

> mCheckMsgThread.getLooper()返回的就是我们在run方法中创建的mLooper。
>
> 那么Handler的构造呢，其实就是在Handler中持有一个指向该Looper.mQueue对象，当handler调用sendMessage方法时，其实就是往该mQueue中去插入一个message，然后Looper.loop()就会取出执行。
>
> 好了，到这我们就分析完了，其实就几行代码；不过有一点我想提一下：
>
> 如果你够细心你会发现，run方法里面当mLooper创建完成后有个notifyAll()，getLooper()中有个wait()，这是为什么呢？因为的mLooper在一个线程中执行，而我们的handler是在UI线程初始化的，也就是说，我们必须等到mLooper创建完成，才能正确的返回getLooper();wait(),notify()就是为了解决这两个线程的同步问题。

## **整体流程**

当我们使用HandlerThread创建一个线程，它statr()之后会在它的线程创建一个Looper对象且初始化了一个MessageQueue(消息队列），通过Looper对象在他的线程构建一个Handler对象，然后我们通过Handler发送消息的形式将任务发送到MessageQueue中，因为Looper是顺序处理消息的，所以当有多个任务存在时就会顺序的排队执行。当我们不使用的时候我们应该调用它的quit()或者quitSafely()来终止它的循环。





### 为什么重写equals 和 hashCode 方法

```

import java.util.HashMap;

class Key {
    private Integer id;

    public Integer getId() {
        return id;
    }

    public Key(Integer id) {
        this.id = id;
    }

    //故意先注释掉equals和hashCode方法//
    public boolean equals(Object o) {
        if (o == null || !(o instanceof Key)) {
            return false;
        } else {
            return this.getId().equals(((Key) o).getId());
        }
    }

    public int hashCode() {
        return id.hashCode();
    }
}

public class WithoutHashCode {
    public static void main(String[] args) {
        Key k1 = new Key(1);
        Key k2 = new Key(1);
        HashMap<Key, String> hm = new HashMap<Key, String>();
        hm.put(k1, "Key with id is 1");
//        hm.put(k2, "Key with id is 1");
        /**
         *    原因有两个—没有重写。第一是没有重写hashCode方法，第二是没有重写equals方法。
         *    这里调用ibject 的 hascode 方法 ，返回hash 是k1 k2 的内存地址
         *    由于k1和k2是两个不同的对象，所以它们的内存地址一定不会相同，也就是说它们的hash值一定不同
         *    结果输出null
         */
        System.out.println(hm.get(k2).hashCode());

        /**
         *   我们再来更正一下存k1和取k2的动作。存k1时，是根据它id的hash值，
         *   假设这里是100，把k1对象放入到对应的位置。而取k2时，
         *   是先计算它的hash值（由于k2的id也是1，这个值也是100），随后到这个位置去找。
         *
         *   存k1时，是根据它id的hash值，假设这里是100，把k1对象放入到对应的位置。
         *   而取k2时，是先计算它的hash值（由于k2的id也是1，这个值也是100），随后到这个位置去
         */
        System.out.println(hm.get(k1).hashCode());
    }
}
```





### java 多线程

1 可重入锁

> 第一，当一个线程在外层函数得到可重入锁后，能直接递归地调用该函数，第二，同一线程在外层函数获得可重入锁后，内层函数可以直接获取该锁对应其它代码的控制权。之前我们提到的synchronized和ReentrantLock都是可重入锁。

2 公平锁 非公平锁 

>  在创建Semaphore对象时，我们可以通过第2个参数，来指定该Semaphore对象是否以公平锁的方式来调度资源。
>
>   公平锁会维护一个等待队列，多个在阻塞状态等待的线程会被插入到这个等待队列，在调度时是按它们所发请求的时间顺序获取锁，而对于非公平锁，当一个线程请求非公平锁时，如果此时该锁变成可用状态，那么这个线程会跳过等待队列中所有的等待线程而获得锁。
>
>   我们在创建可重入锁时，也可以通过调用带布尔类型参数的构造函数来指定该锁是否是公平锁。ReentrantLock(boolean fair)。
>
>   在项目里，如果请求锁的平均时间间隔较长，建议使用公平锁，反之建议使用非公平锁。

3 读写锁

>   之前我们通过synchronized和ReentrantLock来管理临界资源时，只要是一个线程得到锁，其它线程不能操作这个临界资源，这种锁可以叫做“互斥锁”。
>
>   和这种管理方式相比，ReentrantReadWriteLock对象会使用两把锁来管理临界资源，一个是“读锁“，另一个是“写锁“。
>
>   如果一个线程获得了某资源上的“读锁“，那么其它对该资源执行“读操作“的线程还是可以继续获得该锁，也就是说，“读操作“可以并发执行，但执行“写操作“的线程会被阻塞。如果一个线程获得了某资源的“写锁“，那么其它任何企图获得该资源“读锁“和“写锁“的线程都将被阻塞。
>
>   和互斥锁相比，读写锁在保证并发时数据准确性的同时，允许多个线程同时“读“某资源，从而能提升效率。





### 多线程

7.2.10.1有T1、T2、T3三个线程，如何保证T2在T1执行完后执行，T3在T2执行完后执行？

  用join语句，在t3开始前join t2，在t2开始前join t1。

  不过，这会破坏多线程的并发性，不建议这样做。

7.2.10.2 wait和sleep方法有什么不同？

  对于sleep()方法，我们首先要知道该方法是属于Thread类中的。而wait()方法，则是属于Object类中的。

  sleep()方法导致了程序暂停执行指定的时间，让出cpu该其他线程，但是他的监控状态依然保持者，当指定的时间到了又会自动恢复运行状态。在调用sleep()方法的过程中，线程不会释放对象锁。

  而当调用wait()方法的时候，线程会放弃对象锁，进入等待此对象的等待锁定池，只有针对此对象调用notify()方法后本线程才获取对象锁进入运行状态。

7.2.10.3 用Java多线程的思路解决生产者和消费者问题。

  在本书7.2.4部分里，给出了通过wait和notify解决生产者消费者问题的相关代码，大家可以读下。

7.2.10.4 如何在多个线程里共享资源。

  本书提到了synchronized，Lock和volatile三种方法，通过它们，可以控制多线程的并发，大家可以参考下。

7.2.10.5 notify和notifyAll方法有什么差别和联系？

  通过wait和notify方法，我们同样可以通过控制“锁”的方式来实现多线程的并发管理。这两个方法需要放置在synchronized的作用域里（比如synchronized作用的方法或代码块里），一旦一个线程执行wait方法后，该线程就会释放synchronized所关联的锁，进入阻塞状态，所以该线程一般无法再次主动地回到可执行状态，一定要通过其它线程的notify（或notifyAll）方法唤醒它。

  一旦一个线程执行了notify方法，则会通知那些可能因调用wait方法而等待对象锁的其他线程，如果有多个线程等待，则会任意挑选其中的一个线程来发出唤醒通知，这样得到通知的线程就能继续得到对象锁从而继续执行下去。

  对于notifyAll，该方法会让所有因wait方法进入到阻塞状态的线程退出阻塞状态，但由于这些线程此时还没有获取到对象锁，因此还不能继续往下执行，它们会去竞争对象锁，如果其中一个线程获得了对象锁，则会继续执行，在它退出synchronized代码释放锁后，其它的已经被唤醒的线程则会继续竞争，以此类推，直到所有被唤醒的线程都执行完毕。

7.2.10.6 synchronized 和 ReentrantLock有什么不同？它们各自的适用场景是什么？

  synchronized可以作用在方法和代码块上，但无法锁住由多个方法组成的业务块，而ReentrantLock可以保证业务层面的并发。

7.2.10.7 synchronized如果作用在一段代码上，那么是锁什么？

  是锁方法所在的对象





