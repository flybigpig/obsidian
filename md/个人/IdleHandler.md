### 一. 序

Handler 机制算是 Android 基本功，面试常客。但现在面试，多数已经不会直接让你讲讲 Handler 的机制，Looper 是如何循环的，MessageQueue 是如何管理 Message 等，而是基于场景去提问，看看你对 Handler 机制的掌握是否扎实。

本文就来聊聊 Handler 中的 IdleHandler，这个我们比较少用的功能。它能干什么？怎么使用？有什么合适的使用场景？哪些不是合适的使用场景？在 Android Framework 中有哪些地方用到了它？

### 二. IdleHandler

#### 2.1 简单说说 Handler 机制

在说 IdleHandler 之前，先简单了解一下 Handler 机制。

Handler 是标准的事件驱动模型，存在一个消息队列 MessageQueue，它是一个基于消息触发时间的优先级队列，还有一个基于此消息队列的事件循环 Looper，Looper 通过循环，不断的从 MessageQueue 中取出待处理的 Message，再交由对应的事件处理器 Handler/callback 来处理。

其中 MessageQueue 被 Looper 管理，Looper 在构造时同步会创建 MessageQueue，并利用 ThreadLocal 这种 TLS，将其与当前线程绑定。而 App 的主线程在启动时，已经构造并准备好主线程的 Looper 对象，开发者只需要直接使用即可。

Handler 类中封装了大部分「Handler 机制」对外的操作接口，可以通过它的 send/post 相关的方法，向消息队列 MessageQueue 中插入一条 Message。在 Looper 循环中，又会不断的从 MessageQueue 取出下一条待处理的 Message 进行处理。

IdleHandler 使用相关的逻辑，就在 MessageQueue 取消息的 `next()` 方法中。

#### 2.2 IdleHandler 是什么？怎么用？

**IdleHandler 说白了，就是 Handler 机制提供的一种，可以在 Looper 事件循环的过程中，当出现空闲的时候，允许我们执行任务的一种机制。**

IdleHandler 被定义在 MessageQueue 中，它是一个接口。

```
// MessageQueue.java
public static interface IdleHandler {
  boolean queueIdle();
}
复制代码
```

可以看到，定义时需要实现其 `queueIdle()` 方法。同时返回值为 true 表示是一个持久的 IdleHandler 会重复使用，返回 false 表示是一个一次性的 IdleHandler。

既然 IdleHandler 被定义在 MessageQueue 中，使用它也需要借助 MessageQueue。在 MessageQueue 中定义了对应的 add 和 remove 方法。

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
复制代码
```

可以看到 add 或 remove 其实操作的都是 `mIdleHandlers`，它的类型是一个 ArrayList。

**既然 IdleHandler 主要是在 MessageQueue 出现空闲的时候被执行，那么何时出现空闲？**

MessageQueue 是一个基于消息触发时间的优先级队列，所以队列出现空闲存在两种场景。

1.  MessageQueue 为空，没有消息；
2.  MessageQueue 中最近需要处理的消息，是一个延迟消息（when>currentTime），需要滞后执行；

这两个场景，都会尝试执行 IdleHandler。

处理 IdleHandler 的场景，就在 `Message.next()` 这个获取消息队列下一个待执行消息的方法中，我们跟一下具体的逻辑。

```
Message next() {
	// ...
  int pendingIdleHandlerCount = -1; 
  int nextPollTimeoutMillis = 0;
  for (;;) {
    nativePollOnce(ptr, nextPollTimeoutMillis);

    synchronized (this) {
      // ...
      if (msg != null) {
        if (now < msg.when) {
          // 计算休眠的时间
          nextPollTimeoutMillis = (int) Math.min(msg.when - now, Integer.MAX_VALUE);
        } else {
          // Other code
          // 找到消息处理后返回
          return msg;
        }
      } else {
        // 没有更多的消息
        nextPollTimeoutMillis = -1;
      }
      
      if (pendingIdleHandlerCount < 0
          && (mMessages == null || now < mMessages.when)) {
        pendingIdleHandlerCount = mIdleHandlers.size();
      }
      if (pendingIdleHandlerCount <= 0) {
        mBlocked = true;
        continue;
      }

      if (mPendingIdleHandlers == null) {
        mPendingIdleHandlers = new IdleHandler[Math.max(pendingIdleHandlerCount, 4)];
      }
      mPendingIdleHandlers = mIdleHandlers.toArray(mPendingIdleHandlers);
    }

    for (int i = 0; i < pendingIdleHandlerCount; i++) {
      final IdleHandler idler = mPendingIdleHandlers[i];
      mPendingIdleHandlers[i] = null; 

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

    pendingIdleHandlerCount = 0;
    nextPollTimeoutMillis = 0;
  }
}
复制代码
```

我们先解释一下 `next()` 中关于 IdleHandler 执行的主逻辑：

1.  准备执行 IdleHandler 时，说明当前待执行的消息为 null，或者这条消息的执行时间未到；
2.  当 `pendingIdleHandlerCount < 0` 时，根据 `mIdleHandlers.size()` 赋值给 `pendingIdleHandlerCount`，它是后期循环的基础；
3.  将 `mIdleHandlers` 中的 IdleHandler 拷贝到 `mPendingIdleHandlers` 数组中，这个数组是临时的，之后进入 for 循环；
4.  循环中从数组中取出 IdleHandler，并调用其 `queueIdle()` 记录返回值存到 `keep` 中；
5.  当 `keep` 为 false 时，从 `mIdleHandler` 中移除当前循环的 IdleHandler，反之则保留；

可以看到 IdleHandler 机制中，最核心的就是在 `next()` 中，当队列空闲的时候，循环 mIdleHandler 中记录的 IdleHandler 对象，如果其 `queueIdle()` 返回值为 `false` 时，将其从 `mIdleHander` 中移除。

需要注意的是，对 `mIdleHandler` 这个 List 的所有操作，都通过 synchronized 来保证线程安全，这一点无需担心。

#### 2.3 IdleHander 是如何保证不进入死循环的？

当队列空闲时，会循环执行一遍 `mIdleHandlers` 数组并执行 `IdleHandler.queueIdle()` 方法。而如果数组中有一些 IdleHander 的 `queueIdle()` 返回了 `true`，则会保留在 `mIdleHanders` 数组中，下次依然会再执行一遍。

注意现在代码逻辑还在 `MessageQueue.next()` 的循环中，在这个场景下 IdleHandler 机制是如何保证不会进入死循环的？

有些文章会说 IdleHandler 不会死循环，是因为下次循环调用了 `nativePollOnce()` 借助 epoll 机制进入休眠状态，下次有新消息入队的时候会重新唤醒，但这是不对的。

注意看前面 `next()` 中的代码，在方法的末尾会重置 pendingIdleHandlerCount 和 nextPollTimeoutMillis。

```
Message next() {
	// ...
  int pendingIdleHandlerCount = -1; 
  int nextPollTimeoutMillis = 0;
  for (;;) {
		nativePollOnce(ptr, nextPollTimeoutMillis);
    // ...
    // 循环执行 mIdleHandlers
    // ...
    pendingIdleHandlerCount = 0;
    nextPollTimeoutMillis = 0;
  }
}
复制代码
```

nextPollTimeoutMillis 决定了下次进入 `nativePollOnce()` 超时的时间，它传递 0 的时候等于不会进入休眠，所以说 `natievPollOnce()` 进入休眠所以不会死循环是不对的。

这很好理解，毕竟 `IdleHandler.queueIdle()` 运行在主线程，它执行的时间是不可控的，那么 MessageQueue 中的消息情况可能会变化，所以需要再处理一遍。

实际不会死循环的关键是在于 pendingIdleHandlerCount，我们看看下面的代码。

```
Message next() {
	// ...
  // Step 1
  int pendingIdleHandlerCount = -1; 
  int nextPollTimeoutMillis = 0;
  for (;;) {
    nativePollOnce(ptr, nextPollTimeoutMillis);

    synchronized (this) {
      // ...
      // Step 2
      if (pendingIdleHandlerCount < 0
          && (mMessages == null || now < mMessages.when)) {
        pendingIdleHandlerCount = mIdleHandlers.size();
      }
     	// Step 3
      if (pendingIdleHandlerCount <= 0) {
        mBlocked = true;
        continue;
      }
      // ...
    }
		// Step 4
    pendingIdleHandlerCount = 0;
    nextPollTimeoutMillis = 0;
  }
}
复制代码
```

我们梳理一下：

+   Step 1，循环开始前，`pendingIdleHandlerCount` 的初始值为 -1；
+   Step 2，在 `pendingIdleHandlerCount<0` 时，才会通过 `mIdleHandlers.size()` 赋值。也就是说只有第一次循环才会改变 `pendingIdleHandlerCount` 的值；
+   Step 3，如果 `pendingIdleHandlerCount<=0` 时，则循环 continus；
+   Step 4，重置 `pendingIdleHandlerCount` 为 0；

在第二次循环时，`pendingIdleHandlerCount` 等于 0，在 Step 2 不会改变它的值，那么在 Step 3 中会直接 continus 继续下一次循环，此时没有机会修改 `nextPollTimeoutMillis`。

那么 `nextPollTimeoutMillis` 有两种可能：-1 或者下次唤醒的等待间隔时间，在执行到 `nativePollOnce()` 时就会进入休眠，等待再次被唤醒。

下次唤醒时，`mMessage` 必然会有一个待执行的 Message，则 `MessageQueue.next()` 返回到 `Looper.loop()` 的循环中，分发处理这个 Message，之后又是一轮新的 `next()` 中去循环。

#### 2.4 framework 中如何使用 IdleHander？

到这里基本上就讲清楚 IdleHandler 如何使用以及一些细节，接下来我们来看看，在系统中，有哪些地方会用到 IdleHandler 机制。

在 AS 中搜索一下 IdleHandler。

![](https://i-blog.csdnimg.cn/blog_migrate/b2f3acaebde6443bcc99e10f2bc7c783.png)

简单解释一下：

1.  ActivityThread.Idler 在 `ActivityThread.handleResumeActivity()` 中调用。
2.  ActivityThread.GcIdler 是在内存不足时，强行 GC；
3.  Instrumentation.ActivityGoing 在 Activity onCreate() 执行前添加；
4.  Instrumentation.Idler 调用的时机就比较多了，是键盘相关的调用；
5.  TextToSpeechService.SynthThread 是在 TTS 合成完成之后发送广播；

有兴趣可以自己追一下源码，这些都是使用的场景，具体用 IdleHander 干什么，还是要看业务。

### 三.一些面试问题

到这里我们就讲清楚 IdleHandler 干什么？怎么用？有什么问题？以及使用中一些原理的讲解。

下面准备一些基本的问题，供大家理解。

**Q：IdleHandler 有什么用？**

1.  IdleHandler 是 Handler 提供的一种在消息队列空闲时，执行任务的时机；
2.  当 MessageQueue 当前没有立即需要处理的消息时，会执行 IdleHandler；

**Q：MessageQueue 提供了 add/remove IdleHandler 的方法，是否需要成对使用？**

1.  不是必须；
2.  IdleHandler.queueIdle() 的返回值，可以移除加入 MessageQueue 的 IdleHandler；

**Q：当 mIdleHanders 一直不为空时，为什么不会进入死循环？**

1.  只有在 pendingIdleHandlerCount 为 -1 时，才会尝试执行 mIdleHander；
2.  pendingIdlehanderCount 在 next() 中初始时为 -1，执行一遍后被置为 0，所以不会重复执行；

**Q：是否可以将一些不重要的启动服务，搬移到 IdleHandler 中去处理？**

1.  不建议；
2.  IdleHandler 的处理时机不可控，如果 MessageQueue 一直有待处理的消息，那么 IdleHander 的执行时机会很靠后；

**Q：IdleHandler 的 queueIdle() 运行在那个线程？**

1.  陷进问题，queueIdle() 运行的线程，只和当前 MessageQueue 的 Looper 所在的线程有关；
2.  子线程一样可以构造 Looper，并添加 IdleHandler；

### 三. 小结时刻

到这里就把 IdleHandler 的使用和原理说清除了。

IdleHandler 是 Handler 提供的一种在消息队列空闲时，执行任务的时机。但它执行的时机依赖消息队列的情况，那么如果 MessageQueue 一直有待执行的消息时，IdleHandler 就一直得不到执行，也就是它的执行时机是不可控的，不适合执行一些对时机要求比较高的任务。

* * *

IdleHandler 在API上面的解释如下：

public final void addIdleHandler (MessageQueue.IdleHandler handler)

向消息队列中添加一个新的MessageQueue.IdleHandler。当调用IdleHandler.queueIdle()返回false时，此MessageQueue.IdleHandler会自动的从消息队列中移除。或者调用removeIdleHandler(MessageQueue.IdleHandler)也可以从消息队列中移除MessageQueue.IdleHandler。

此方法是线程安全的。

参数 ：handler 要添加的IdleHandler。

> 注意：在主线程中使用时queueIdle中不能执行太耗时的任务。

使用场景：

1.用在 android初始化activty界面时。如果想用android做一个播放器，如果下面包括播放进度条，暂停、停止等按钮的控件用popWindow实现的话。就是在程序一运行起来就需要将下面的popWindow显示在activty上。用这个是比较好的，当然你可以用myHandler.sendEmptyMessage() 去你想要的操作。

把IdleHandler用在onCreate ( )里面，用法很简单如下：

```java
Looper.myQueue().addIdleHandler(new IdleHandler()
{

@Override
public boolean queueIdle()
{

// TODO Auto-generated method stub
//你想做的任何事情
//........
//........
return false;
}
});
```

2.很多人在Android项目中都会遇到希望一些操作延迟一点处理，一般会使用Handler.postDelayed(Runnable r, long delayMillis)来实现，但是又不知道该延迟多少时间比较合适，因为手机性能不同，有的性能较差可能需要延迟较多，有的性能较好可以允许较少的延迟时间。

之前在项目中对启动过程进行优化，用到了IdleHandler，它可以在主线程空闲时执行任务，而不影响其他任务的执行。

对于多个任务的延迟加载，如果addIdleHandler()调用多次明显不太优雅，而且也不要把所有要延迟的任务都一起放到queueIdle()方法内。根据queueIdle返回true时可以执行多次的特点，可以实现一个任务列表，然后从这个任务列表中取任务执行。

下面给出具体实现方案：

```java
import android.os.Looper;
import android.os.MessageQueue;
 
import java.util.LinkedList;
import java.util.Queue;
 
public class DelayTaskDispatcher {
    private Queue<Task> delayTasks = new LinkedList<>();
 
    private MessageQueue.IdleHandler idleHandler = new MessageQueue.IdleHandler() {
        @Override
        public boolean queueIdle() {
            if (delayTasks.size() > 0) {
                Task task = delayTasks.poll();
                if (task != null) {
                    task.run();
                }
            }
            return !delayTasks.isEmpty(); //delayTasks非空时返回ture表示下次继续执行，为空时返回false系统会移除该IdleHandler不再执行
        }
    };
 
    public DelayTaskDispatcher addTask(Task task) {
        delayTasks.add(task);
        return this;
    }
 
    public void start() {
        Looper.myQueue().addIdleHandler(idleHandler);
    }
}
```

```java
//使用系统Runnable接口自定义Task接口
public interface Task extends Runnable {
 
}
```

使用方法：

```java
    new DelayTaskDispatcher().addTask(new Task() {
            @Override
            public void run() {
                Log.d(TAG, "DelayTaskDispatcher one task");
            }
        }).addTask(new Task() {
            @Override
            public void run() {
                Log.d(TAG, "DelayTaskDispatcher two task");
            }
        }).start();
```

使用上述方式可以添加多个任务，在线程空闲时分别执行。

* * *

```java
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

注释中很明确地指出当消息队列空闲时会执行IdleHandler的queueIdle()方法，该方法返回一个boolean值，

如果为false则执行完毕之后移除这条消息（一次性完事），

如果为true则保留，等到下次空闲时会再次执行（重复执行）。

看MessageQueue的源码可以发现有两处关于IdleHandler的声明，分明是：

+   存放IdleHandler的ArrayList(mIdleHandlers)，
+   还有一个IdleHandler数组(mPendingIdleHandlers)。

后面的数组，它里面放的IdleHandler实例都是临时的，也就是每次使用完（调用了queueIdle方法）之后，都会置空（mPendingIdleHandlers\[i\] = null）

下面看下MessageQueue的next()方法可以发现确实是这样

```java
Message next() {
        ......
        for (;;) {
            ......
            synchronized (this) {
        // 此处为正常消息队列的处理
                ......
                if (mQuitting) {
                    dispose();
                    return null;
                }
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
            pendingIdleHandlerCount = 0;
            nextPollTimeoutMillis = 0;
        }
    }
```

就在MessageQueue的next方法里面。

大概流程是这样的：

1.  如果本次循环拿到的Message为空，或者！这个Message是一个延时的消息而且还没到指定的触发时间，那么，就认定当前的队列为空闲状态，
2.  接着就会遍历mPendingIdleHandlers数组(这个数组里面的元素每次都会到mIdleHandlers中去拿)来调用每一个IdleHandler实例的queueIdle方法，
3.  如果这个方法返回false的话，那么这个实例就会从mIdleHandlers中移除，也就是当下次队列空闲的时候，不会继续回调它的queueIdle方法了。

处理完IdleHandler后会将nextPollTimeoutMillis设置为0，也就是不阻塞消息队列，当然要注意这里执行的代码同样不能太耗时，因为它是同步执行的，如果太耗时肯定会影响后面的message执行。

能力大概就是上面讲的那样，那么能力决定用处，用处从本质上讲就是趁着消息队列空闲的时候干点事情，当然具体的用处还是要看具体的处理。

### IdleHandler返回true和false带来的不同结果

queueIdle() 方法如果返回 false，那么这个 IdleHandler 只会执行一次，执行完后会从队列中删除，如果返回 true，那么执行完后不会被删除，只要执行 MessageQueue.next 时消息队列中没有可执行的消息，即为空闲时间，那么 IdleHandler 队列中的 IdleHandler 还会继续被执行。

```java
public void clickTestIdleHandler(View view) {
    // 添加第一个 IdleHandler
    Looper.myQueue().addIdleHandler(new MessageQueue.IdleHandler() {
        @Override
        public boolean queueIdle() {
            try {
                Thread.sleep(2000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            Log.e("Test","IdleHandler1 queueIdle");
            return false;
        }
    });
    // 添加第二个 IdleHandler
    Looper.myQueue().addIdleHandler(new MessageQueue.IdleHandler() {
        @Override
        public boolean queueIdle() {
            try {
                Thread.sleep(2000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            Log.e("Test","IdleHandler2 queueIdle");
            return false;
        }
    });
    Handler handler = new Handler();
    // 添加第一个Message
    Message message1 = Message.obtain(handler, new Runnable() {
        @Override
        public void run() {
            try {
                Thread.sleep(2000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            Log.e("Test","message1");
        }
    });
    message1.sendToTarget();
    // 添加第二个Message
    Message message2 = Message.obtain(handler, new Runnable() {
        @Override
        public void run() {
            try {
                Thread.sleep(2000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            Log.e("Test","message2");
        }
    });
    message2.sendToTarget();
}
```

上面的例子中分别向消息队列中添加来两个 IdleHandler 和两个 Message，其中 IdleHandler 的 queueIdle() 方法返回 false，下面来看一下结果：

```bash
16:23:07.726 E/Test: message1
16:23:09.727 E/Test: message2
16:23:11.732 E/Test: IdleHandler1 queueIdle
16:23:13.733 E/Test: IdleHandler2 queueIdle
```

可以看到 IdleHandler 是在消息队列的其它消息执行完后才执行的，而且只执行来一次。  
再来看一下 queueIdle() 方法返回 true 的情况：

```bash
16:27:02.083  E/Test: message1
16:27:04.084  E/Test: message2
16:27:06.090  E/Test: IdleHandler1 queueIdle
16:27:08.090  E/Test: IdleHandler2 queueIdle
16:27:10.095  E/Test: IdleHandler1 queueIdle
16:27:12.096  E/Test: IdleHandler2 queueIdle
16:27:14.099  E/Test: IdleHandler1 queueIdle
16:27:16.099  E/Test: IdleHandler2 queueIdle
16:27:43.788  E/Test: IdleHandler1 queueIdle
16:27:45.788  E/Test: IdleHandler2 queueIdle
16:27:47.792  E/Test: IdleHandler1 queueIdle
16:27:49.793  E/Test: IdleHandler2 queueIdle
```

可以看到，当 queueIdle() 方法返回 true时会多次执行，即 IdleHandler 执行一次后不会从 IdleHandler 的队列中删除，等下次空闲时间到来时还会继续执行。

### IdleHandler它在源码里的使用场景：

比如在ActivityThread中，就有一个名叫GcIdler的内部类，实现了IdleHandler接口。

它在queueIdle方法被回调时，会做强行GC的操作（即调用BinderInternal的forceGc方法），但强行GC的前提是，与上一次强行GC至少相隔5秒以上。 

### ActivityThread中GcIdler

那么我们就看看最为明显的ActivityThread中声明的GcIdler。在ActivityThread中的**H**收到**GC\_WHEN\_IDLE**消息后，会执行scheduleGcIdler，将GcIdler添加到MessageQueue中的空闲任务集合中。具体如下：

```java
 void scheduleGcIdler() {
        if (!mGcIdlerScheduled) {
            mGcIdlerScheduled = true;
            //添加GC任务
            Looper.myQueue().addIdleHandler(mGcIdler);
        }
        mH.removeMessages(H.GC_WHEN_IDLE);
    }
```

ActivityThread中GcIdler的详细声明：

```java
//GC任务
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
            BinderInternal.forceGc("bg");
        }
    }
```

GcIdler方法理解起来很简单、就是获取上次GC的时间，判断是否需要GC操作。如果需要则进行GC操作。这里ActivityThread中还声明了其他空闲时的任务。如果大家对其他空闲任务感兴趣，可以自行研究。

那这个GcIdler会在什么时候使用呢？

当ActivityThread的mH(Handler)收到GC\_WHEN\_IDLE消息之后。

何时会收到GC\_WHEN\_IDLE消息？

当AMS(ActivityManagerService)中的这两个方法被调用之后：

+   doLowMemReportIfNeededLocked， 这个方法看名字就知道是不够内存的时候调用的了。
+   activityIdle这个方法呢，就是当ActivityThread的handleResumeActivity方法被调用时(Activity的onResume方法也是在这方法里回调)调用的。

其他使用场景：

+   Activity启动优化（加快App启动速度）：onCreate，onStart，onResume中耗时较短但非必要的代码可以放到IdleHandler中执行，减少启动时间
+   想要在一个View绘制完成之后添加其他依赖于这个View的View，当然这个用View#post()也能实现，区别就是前者会在消息队列空闲时执行
+   发送一个返回true的IdleHandler，在里面让某个View不停闪烁，这样当用户发呆时就可以诱导用户点击这个View，这也是种很酷的操作
+   一些第三方库中有使用，比如LeakCanary，Glide中有使用到，具体可以自行去查看

参考链接：https://juejin.cn/post/6844903916958662669

* * *

Q:主线程的IdleHandler 如果进行耗时操作会怎样？

1.Thread.sleep(n) n > 10 页面会卡死，但不会崩溃，如果页面有动图，则动图变为静态图（视频未尝试，猜测也会处于静止状态） 但此时如果点击页面按钮，则会无响应进入anr，如果不点击，n秒过后恢复正常。

2.网络请求： 不会崩溃，但会报错 IdleHandler threw exception android.os.NetworkOnMainThreadException

3.文件写入本地： 成功。测试所用文件为小文件，大文件猜测也一样，同sleep，中途不点击页面无事，点击anr。

Q:onCreate中Mainlooper添加idle后，先执行哪个？

先执行oncreate其他代码，然后执行idleHandler