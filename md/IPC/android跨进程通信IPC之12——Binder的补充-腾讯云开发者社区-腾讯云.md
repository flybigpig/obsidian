本篇文章的主要内容

-   Binder中的线程池
-   Binder的权限
-   Binder的死亡通知机制

### 一 、Binder中的线程池

> 客户端在使用Binder可以调用服务端的方法，这里面有一些隐含的问题，如果我们服务端的方法是一个耗时的操作，那么对于我们客户端和服务端都存在风险，如果有很多客户端都来调用它的方法，那么是否会造成ANR那？多个客户端调用，是否会有同步问题？如果客户端在UI线程中调用的这个是耗时方法，那么是不是它也会造成ANR？这些问题都是真实存在的，首先第一个问题是不会出现，因为服务端所有这些被调用方法都是在一个线程池中执行的，不在服务端的UI线程中，因此服务端不会ANR，但是服务端会有同步问题，因此我们提供的服务端接口方法应该注意同步问题。客户端会ANR很容易解决，就是我们不要在UI线程中就可以避免了。那我们一起来看下Binder的线程池

##### (一) Binder线程池简述

> Android系统启动完成后，ActivityManager、PackageManager等各大服务都运行在system\_server进程，app应用需要使用系统服务都是通过Binder来完成进程间的通信，那么对于Binder线程是如何管理的？又是如何创建的？其实无论是system\_server进程还是app进程，都是在fork完成后，便会在新进程中执行onZygoteInit()的过程，启动Binder线程池。

从整体架构以及通信协议的角度来阐述了Binder架构。那对于binder线程是如何管理的呢，又是如何创建的呢？其实无论是system\_server进程，还是app进程，都是在进程fork完成后，便会在新进程中执行onZygoteInit()的过程中，启动binder线程池。

##### (二) Binder线程池创建

Binder 线程创建与其坐在进程的创建中产生，Java层进程的创建都是通过**Process.start()**方法，向Zygote进程发出创建进程的socket消息，Zygote收到消息后会调用Zygote.forkAndSpecialize()来fork出新进程，在新进程中调用**RuntimeInit.nativeZygoteInit()**方法，该方法经过JNI映射，最终会调用app\_main.cpp中的onZygoteInit，那么接下来从这个方法开始。

###### 1、onZygoteInit()

代码在[app\_main.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcmds%2Fapp_process%2Fapp_main.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 的91行

```
    virtual void onZygoteInit()
    {
        // 获取 ProcessState对象
        sp<ProcessState> proc = ProcessState::self();
        ALOGV("App process: starting thread pool.\n");
        proc->startThreadPool();
    }
```

ProcessState主要工作是调用open()打开/dev/binder驱动设备，再利用mmap()映射内核的地址空间，将Binder驱动的fd赋值ProcessState对象的变量mDriverFD，用于交互操作。startThreadPool()是创建一个型的Binder线程，不断进行talkWithDriver()。

###### 2、ProcessState.startThreadPool()

代码在[ProcessState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 的132行

```
void ProcessState::startThreadPool()
{
     //多线程同步
    AutoMutex _l(mLock);
    if (!mThreadPoolStarted) {
        mThreadPoolStarted = true;
        spawnPooledThread(true);
    }
}
```

启动Binder线程池后，则设置mThreadPoolStarted=true，通过变量mThreadPoolStarted来保证每个应用进程只允许启动一个Binder线程池，且本次创建的是Binder主线程(isMain=true，isMain具体请看spawnPooledThread(true))。其余Binder线程池中的线程都是由Binder驱动来控制创建的。然后继续跟踪看下spawnPooledThread(true)函数

###### 3、ProcessState. spawnPooledThread()

代码在[ProcessState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 的286行

```
void ProcessState::spawnPooledThread(bool isMain)
{
    if (mThreadPoolStarted) {
        //获取Binder线程名
        String8 name = makeBinderThreadName();
        ALOGV("Spawning new pooled thread, name=%s\n", name.string());
         //这里注意isMain=true
        sp<Thread> t = new PoolThread(isMain);
        t->run(name.string());
    }
}
```

###### 3.1、ProcessState. makeBinderThreadName()

代码在[ProcessState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 的279行

```
String8 ProcessState::makeBinderThreadName() {
    int32_t s = android_atomic_add(1, &mThreadPoolSeq);
    String8 name;
    name.appendFormat("Binder_%X", s);
    return name;
}
```

获取Binder的线程名，格式为**Binder\_X**，其中X为整数，每个进程中的Binder编码是从1开始，依次递增；只有通过**makeBinderThreadName()**方法来创建线程才符合这个格式，对于直接将当前线程通过**joinThreadPool()**加入线程池的线程名则不符合这个命名规则。另外，目前Android N中Binder命令已改为**Binder:<pid> \_X**，则对于分析问题很有帮助，通过Binder名称的pid就可以很快定位到该Binder所属进程的pid

###### 3.2、PoolThread.run

代码在[ProcessState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 的52行

```
class PoolThread : public Thread
{
public:
    PoolThread(bool isMain)
        : mIsMain(isMain)
    {
    }

protected:
    virtual bool threadLoop()
    {
        IPCThreadState::self()->joinThreadPool(mIsMain); 
        return false;
    }
    const bool mIsMain;
};
```

从函数名看起来是创建线程池，其实就只是创建一个线程，该PoolThread继承Thread类，t->run()函数最终会调用PoolThread的threadLooper()方法。

###### 4、IPCThreadState. joinThreadPool()

代码在[IPCThreadState.cpp.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 的52行

```
void IPCThreadState::joinThreadPool(bool isMain)
{
    LOG_THREADPOOL("**** THREAD %p (PID %d) IS JOINING THE THREAD POOL\n", (void*)pthread_self(), getpid());
     // 创建Binder线程
    mOut.writeInt32(isMain ? BC_ENTER_LOOPER : BC_REGISTER_LOOPER);

    // This thread may have been spawned by a thread that was in the background
    // scheduling group, so first we will make sure it is in the foreground
    // one to avoid performing an initial transaction in the background.
    //设置前台调度策略
    set_sched_policy(mMyThreadId, SP_FOREGROUND);

    status_t result;
    do {
         //清楚队列的引用
        processPendingDerefs();
        // now get the next command to be processed, waiting if necessary
        // 处理下一条指令
        result = getAndExecuteCommand();

        if (result < NO_ERROR && result != TIMED_OUT && result != -ECONNREFUSED && result != -EBADF) {
            ALOGE("getAndExecuteCommand(fd=%d) returned unexpected error %d, aborting",
                  mProcess->mDriverFD, result);
            abort();
        }

        // Let this thread exit the thread pool if it is no longer
        // needed and it is not the main process thread.
        if(result == TIMED_OUT && !isMain) {
            //非主线程出现timeout则线程退出
            break;
        }
    } while (result != -ECONNREFUSED && result != -EBADF);

    LOG_THREADPOOL("**** THREAD %p (PID %d) IS LEAVING THE THREAD POOL err=%p\n",
        (void*)pthread_self(), getpid(), (void*)result);
    // 线程退出循环
    mOut.writeInt32(BC_EXIT_LOOPER);
    //false代表bwr数据的read_buffer为空
    talkWithDriver(false);
}
```

-   1 对于**isMain**\=true的情况下，command为BC\_ENTER\_LOOPER，代表的是Binder主线程，不会退出线程。
-   2 对于**isMain**\=false的情况下，command为BC\_REGISTER\_LOOPER，表示的是binder驱动创建的线程。

joinThreadLoop()里面有一个do——while循环，这个thread里面主要的调用，也就是重点，里面主要就是调用了两个函数processPendingDerefs()和getAndExecuteCommand()函数，那我们依次来看下。

###### 4.1、IPCThreadState. processPendingDerefs()

代码在[IPCThreadState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 的454行

```
// When we've cleared the incoming command queue, process any pending derefs
void IPCThreadState::processPendingDerefs()
{
    if (mIn.dataPosition() >= mIn.dataSize()) {
        size_t numPending = mPendingWeakDerefs.size();
        if (numPending > 0) {
            for (size_t i = 0; i < numPending; i++) {
                RefBase::weakref_type* refs = mPendingWeakDerefs[i];
                //弱引用减一
                refs->decWeak(mProcess.get());
            }
            mPendingWeakDerefs.clear();
        }

        numPending = mPendingStrongDerefs.size();
        if (numPending > 0) {
            for (size_t i = 0; i < numPending; i++) {
                BBinder* obj = mPendingStrongDerefs[i];
                //强引用减一
                obj->decStrong(mProcess.get());
            }
            mPendingStrongDerefs.clear();
        }
    }
}
```

我们知道了processPendingDerefs()这个函数主要是将mPendingWeakDerefs和mPendingStrongDerefs中的指针解除应用，而且他的执行结果并不影响Loop的执行，那我们主要看下getAndExecuteCommand()函数里面做了什么。

###### 4.2、IPCThreadState. getAndExecuteCommand()

代码在[IPCThreadState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 的414行

```
status_t IPCThreadState::getAndExecuteCommand()
{
    status_t result;
    int32_t cmd;
     //与binder进行交互
    result = talkWithDriver();
    if (result >= NO_ERROR) {
        size_t IN = mIn.dataAvail();
        if (IN < sizeof(int32_t)) return result;
        cmd = mIn.readInt32();
        IF_LOG_COMMANDS() {
            alog << "Processing top-level Command: "
                 << getReturnString(cmd) << endl;
        }

        pthread_mutex_lock(&mProcess->mThreadCountLock);
        mProcess->mExecutingThreadsCount++;
        pthread_mutex_unlock(&mProcess->mThreadCountLock);
        // 执行Binder响应吗
        result = executeCommand(cmd);

        pthread_mutex_lock(&mProcess->mThreadCountLock);
        mProcess->mExecutingThreadsCount--;
        pthread_cond_broadcast(&mProcess->mThreadCountDecrement);
        pthread_mutex_unlock(&mProcess->mThreadCountLock);

        // After executing the command, ensure that the thread is returned to the
        // foreground cgroup before rejoining the pool.  The driver takes care of
        // restoring the priority, but doesn't do anything with cgroups so we
        // need to take care of that here in userspace.  Note that we do make
        // sure to go in the foreground after executing a transaction, but
        // there are other callbacks into user code that could have changed
        // our group so we want to make absolutely sure it is put back.
        set_sched_policy(mMyThreadId, SP_FOREGROUND);
    }

    return result;
}
```

我们知道了getAndExecuteCommand()主要就是调用两个函数talkWithDriver()和executeCommand()，我们分别看一下

###### 4.2.1、ProcessState. talkWithDriver()

代码在[IPCThreadState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 的803行

```
status_t IPCThreadState::talkWithDriver(bool doReceive)
{
    if (mProcess->mDriverFD <= 0) {
        return -EBADF;
    }

    binder_write_read bwr;

    // Is the read buffer empty?
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();

    // We don't want to write anything if we are still reading
    // from data left in the input buffer and the caller
    // has requested to read the next data.
    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;

    bwr.write_size = outAvail;
    bwr.write_buffer = (uintptr_t)mOut.data();

    // This is what we'll read.
    if (doReceive && needRead) {
        bwr.read_size = mIn.dataCapacity();
        bwr.read_buffer = (uintptr_t)mIn.data();
    } else {
        bwr.read_size = 0;
        bwr.read_buffer = 0;
    }

    IF_LOG_COMMANDS() {
        TextOutput::Bundle _b(alog);
        if (outAvail != 0) {
            alog << "Sending commands to driver: " << indent;
            const void* cmds = (const void*)bwr.write_buffer;
            const void* end = ((const uint8_t*)cmds)+bwr.write_size;
            alog << HexDump(cmds, bwr.write_size) << endl;
            while (cmds < end) cmds = printCommand(alog, cmds);
            alog << dedent;
        }
        alog << "Size of receive buffer: " << bwr.read_size
            << ", needRead: " << needRead << ", doReceive: " << doReceive << endl;
    }

    // Return immediately if there is nothing to do.
    //如果没有输入输出数据，直接返回
    if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;

    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    status_t err;
    do {
        IF_LOG_COMMANDS() {
            alog << "About to read/write, write size = " << mOut.dataSize() << endl;
        }
#if defined(HAVE_ANDROID_OS)
        // ioctl执行binder读写操作，经过syscall，进入Binder驱动，调用Binder_ioctl
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
        else
            err = -errno;
#else
        err = INVALID_OPERATION;
#endif
        if (mProcess->mDriverFD <= 0) {
            err = -EBADF;
        }
        IF_LOG_COMMANDS() {
            alog << "Finished read/write, write size = " << mOut.dataSize() << endl;
        }
    } while (err == -EINTR);

    IF_LOG_COMMANDS() {
        alog << "Our err: " << (void*)(intptr_t)err << ", write consumed: "
            << bwr.write_consumed << " (of " << mOut.dataSize()
                        << "), read consumed: " << bwr.read_consumed << endl;
    }

    if (err >= NO_ERROR) {
        if (bwr.write_consumed > 0) {
            if (bwr.write_consumed < mOut.dataSize())
                mOut.remove(0, bwr.write_consumed);
            else
                mOut.setDataSize(0);
        }
        if (bwr.read_consumed > 0) {
            mIn.setDataSize(bwr.read_consumed);
            mIn.setDataPosition(0);
        }
        IF_LOG_COMMANDS() {
            TextOutput::Bundle _b(alog);
            alog << "Remaining data size: " << mOut.dataSize() << endl;
            alog << "Received commands from driver: " << indent;
            const void* cmds = mIn.data();
            const void* end = mIn.data() + mIn.dataSize();
            alog << HexDump(cmds, mIn.dataSize()) << endl;
            while (cmds < end) cmds = printReturnCommand(alog, cmds);
            alog << dedent;
        }
        return NO_ERROR;
    }

    return err;
}
```

在这里调用的是isMain=true，也就是向mOut写入的是便是BC\_ENTER\_LOOPER。后面就是进入Binder驱动了，具体到binder\_thread\_write()函数的BC\_ENTER\_LOOPER的处理过程。

###### 4.2.1.1、binder\_thread\_write

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 的2252行

```
static int binder_thread_write(struct binder_proc *proc,
            struct binder_thread *thread,
            binder_uintptr_t binder_buffer, size_t size,
            binder_size_t *consumed)
{
    uint32_t cmd;
    void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
    void __user *ptr = buffer + *consumed;
    void __user *end = buffer + size;
    while (ptr < end && thread->return_error == BR_OK) {
        //拷贝用户空间的cmd命令，此时为BC_ENTER_LOOPER
        if (get_user(cmd, (uint32_t __user *)ptr)) -EFAULT;
        ptr += sizeof(uint32_t);
        switch (cmd) {
          case BC_REGISTER_LOOPER:
              if (thread->looper & BINDER_LOOPER_STATE_ENTERED) {
                //出错原因：线程调用完BC_ENTER_LOOPER，不能执行该分支
                thread->looper |= BINDER_LOOPER_STATE_INVALID;

              } else if (proc->requested_threads == 0) {
                //出错原因：没有请求就创建线程
                thread->looper |= BINDER_LOOPER_STATE_INVALID;

              } else {
                proc->requested_threads--;
                proc->requested_threads_started++;
              }
              thread->looper |= BINDER_LOOPER_STATE_REGISTERED;
              break;

          case BC_ENTER_LOOPER:
              if (thread->looper & BINDER_LOOPER_STATE_REGISTERED) {
                //出错原因：线程调用完BC_REGISTER_LOOPER，不能立刻执行该分支
                thread->looper |= BINDER_LOOPER_STATE_INVALID;
              }
              //创建Binder主线程
              thread->looper |= BINDER_LOOPER_STATE_ENTERED;
              break;

          case BC_EXIT_LOOPER:
              thread->looper |= BINDER_LOOPER_STATE_EXITED;
              break;
        }
        ...
    }
    *consumed = ptr - buffer;
  }
  return 0;
}
```

> 处理完BC\_ENTER\_LOOPER命令后，一般情况下成功设置thread->looper |= BINDER\_LOOPER\_STATE\_ENTERED。那么binder线程的创建是在什么时候？那就当该线程有事务需要处理的时候，进入binder\_thread\_read()过程。

###### 4.2.1.2、binder\_thread\_read

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 的2654行

```
binder_thread_read（）{
  ...
retry:
    //当前线程todo队列为空且transaction栈为空，则代表该线程是空闲的
    wait_for_proc_work = thread->transaction_stack == NULL &&
        list_empty(&thread->todo);

    if (thread->return_error != BR_OK && ptr < end) {
        ...
        put_user(thread->return_error, (uint32_t __user *)ptr);
        ptr += sizeof(uint32_t);
        //发生error，则直接进入done
        goto done; 
    }

    thread->looper |= BINDER_LOOPER_STATE_WAITING;
    if (wait_for_proc_work)
         //可用线程个数+1
        proc->ready_threads++; 
    binder_unlock(__func__);

    if (wait_for_proc_work) {
        if (non_block) {
            ...
        } else
            //当进程todo队列没有数据,则进入休眠等待状态
            ret = wait_event_freezable_exclusive(proc->wait, binder_has_proc_work(proc, thread));
    } else {
        if (non_block) {
            ...
        } else
            //当线程todo队列没有数据，则进入休眠等待状态
            ret = wait_event_freezable(thread->wait, binder_has_thread_work(thread));
    }

    binder_lock(__func__);
    if (wait_for_proc_work)
        //可用线程个数-1
        proc->ready_threads--; 
    thread->looper &= ~BINDER_LOOPER_STATE_WAITING;

    if (ret)
        //对于非阻塞的调用，直接返回
        return ret; 

    while (1) {
        uint32_t cmd;
        struct binder_transaction_data tr;
        struct binder_work *w;
        struct binder_transaction *t = NULL;

        //先考虑从线程todo队列获取事务数据
        if (!list_empty(&thread->todo)) {
            w = list_first_entry(&thread->todo, struct binder_work, entry);
        //线程todo队列没有数据, 则从进程todo对获取事务数据
        } else if (!list_empty(&proc->todo) && wait_for_proc_work) {
            w = list_first_entry(&proc->todo, struct binder_work, entry);
        } else {
            ... //没有数据,则返回retry
        }

        switch (w->type) {
            case BINDER_WORK_TRANSACTION: ...  break;
            case BINDER_WORK_TRANSACTION_COMPLETE:...  break;
            case BINDER_WORK_NODE: ...    break;
            case BINDER_WORK_DEAD_BINDER:
            case BINDER_WORK_DEAD_BINDER_AND_CLEAR:
            case BINDER_WORK_CLEAR_DEATH_NOTIFICATION:
                struct binder_ref_death *death;
                uint32_t cmd;

                death = container_of(w, struct binder_ref_death, work);
                if (w->type == BINDER_WORK_CLEAR_DEATH_NOTIFICATION)
                  cmd = BR_CLEAR_DEATH_NOTIFICATION_DONE;
                else
                  cmd = BR_DEAD_BINDER;
                put_user(cmd, (uint32_t __user *)ptr;
                ptr += sizeof(uint32_t);
                put_user(death->cookie, (void * __user *)ptr);
                ptr += sizeof(void *);
                ...
                if (cmd == BR_DEAD_BINDER)
                  goto done; //Binder驱动向client端发送死亡通知，则进入done
                break;
        }

        if (!t)
            continue; //只有BINDER_WORK_TRANSACTION命令才能继续往下执行
        ...
        break;
    }

done:
    *consumed = ptr - buffer;
    //创建线程的条件
    if (proc->requested_threads + proc->ready_threads == 0 &&
        proc->requested_threads_started < proc->max_threads &&
        (thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
         BINDER_LOOPER_STATE_ENTERED))) {
        proc->requested_threads++;
        // 生成BR_SPAWN_LOOPER命令，用于创建新的线程
        put_user(BR_SPAWN_LOOPER, (uint32_t __user *)buffer)；
    }
    return 0;
}
```

放生一下三种情况中的任意一种，就会进入**done**

-   当前线程return\_error发生error的情况
-   当Binder驱动向client端发送死亡通知的情况
-   当类型为BINDER\_WORK\_TRANSACTION(即受到命令是BC\_TRANSACTION或BC\_REPLY)的情况

任何一个Binder线程当同事满足以下条件时，则会生成用于创建新线程的BR\_SPAWN\_LOOPER命令：

-   1、当前进程中没有请求创建binder线程，即request\_threads=0
-   2、当前进程没有空闲可用binder线程，即ready\_threads=0（线程进入休眠状态的个数就是空闲线程数）
-   3、当前线程应启动线程个数小于最大上限(默认是15)
-   4、当前线程已经接收到BC\_ENTER\_LOOPER或者BC\_REGISTER\_LOOPEE命令，即当前处于BINDER\_LOOPER\_STATE\_REGISTERED或者BINDER\_LOOPER\_STATE\_ENTERED状态。

###### 4.2.2、IPCThreadState. executeCommand()

代码在[IPCThreadState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 的947行

```
status_t IPCThreadState::executeCommand(int32_t cmd)
{
    status_t result = NO_ERROR;
    switch ((uint32_t)cmd) {
      ...
      case BR_SPAWN_LOOPER:
          //创建新的binder线程 
          mProcess->spawnPooledThread(false);
          break;
      ...
    }
    return result;
}
```

Binder主线程的创建时在其所在进程创建的过程中一起创建的，后面再创建的普通binder线程是由 spawnPooledThread(false)方法所创建的。

##### (三) Binder线程池流程

Binder设计架构中，只有第一个Binder主线程也就是Binder\_1线程是由应用程序主动创建的，Binder线程池的普通线程都是Binder驱动根据IPC通信需求而创建，Binder线程的创建流程图如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/sg670a82y5.png)

Binder线程的创建流程.png

每次由Zygote fork出新进程的过程中，伴随着创建binder线程池，调用spawnPooledThread来创建binder主线程。当线程执行binder\_thread\_read的过程中，发现当前没有空闲线程，没有请求创建线程，且没有达到上限，则创建新的binder线程。

Binder的transaction有3种类型：

> \-call：发起进程的线程不一定是Binder线程，大多数情况下，接受者只指向进程，并不确定会有两个线程来处理，所以不指定线程。

-   reply：发起者一定是binder线程，并且接收者线程便是上此call时的发起线程(该线程不一定是binder线程，可以是任意线程)
-   async：与call类型差不多，唯一不同的是async是oneway方式，不需要回复，发起进程的线程不一定是在Binder线程，接收者只指向进程，并不确定会有那个线程来处理，所以不指定线程。

Binder系统中可分为3类binder线程：

-   Binder主线程：进程创建过程会调用startThreadPool过程再进入spawnPooledThread(true)，来创建Binder主线程。编号从1开始，也就是意味着binder主线程名为binder\_1，并且主线程是不会退出的。
-   Binder普通线程：是由Binder Driver是根据是否有空闲的binder线程来决定是否创建binder线程，回调spawnPooledThread(false) ，isMain=false，该线程名格式为binder\_x
-   Binder其他线程：其他线程是指并没有调用spawnPooledThread方法，而是直接调用IPCThreadState.joinThreadPool()，将当前线程直接加入binder线程队列。例如：mediaserver和servicemanager的主线程都是binder主线程，但是system\_server的主线程并非binder主线程。

### 二、Binder的权限

##### (一) 概述

前面关于Binder的文章，讲解了Binder的IPC机制。看过Android系统源代码的读者一定看到过**Binder.clearCallingIdentity()**，**Binder.restoreCallingIdentity()**，定义在**Binder.java**文件中

```
 // Binder.java
    //清空远程调用端的uid和pid，用当前本地进程的uid和pid替代
    public static final native long clearCallingIdentity();
     // 作用是回复远程调用端的uid和pid信息，正好是"clearCallingIdentity"的饭过程
    public static final native void restoreCallingIdentity(long token);
```

这两个方法都涉及了uid和pid，每个线程都有自己独一无二**_IPCThreadState_对象，记录当前线程的pid和uid，可通过方法**Binder.getCallingPid()**和**Binder.getCallingUid()\*\*获取相应的pic和uid。

> clearCallingIdentity()，restoreCallingIdentity()这两个方法使用过程都是成对使用的，这两个方法配合使用，用于权限控制检测功能。

##### (二) 原理

从定义这两个方法是native方法，通过Binder的JNI调用，在android\_util\_Binder.cpp文件中定义了native两个方法所对应的jni方法。

###### 1、clearCallingIdentity

代码在[android\_util\_Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 771行

```
static jlong android_os_Binder_clearCallingIdentity(JNIEnv* env, jobject clazz)
{
    return IPCThreadState::self()->clearCallingIdentity();
}
```

这里面代码混简单，就是调用了IPCThreadState的clearCallingIdentity()方法

###### 1.1、IPCThreadState::clearCallingIdentity()

代码在[IPCThreadState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 356行

```
int64_t IPCThreadState::clearCallingIdentity()
{
    int64_t token = ((int64_t)mCallingUid<<32) | mCallingPid;
    clearCaller();
    return token;
}
```

UID和PID是IPCThreadState的成员变量，都是32位的int型数据，通过移动操作，将UID和PID的信息保存到token，其中高32位保存UID，低32位保存PID。然后调用clearCaller()方法将当前本地进程pid和uid分别赋值给PID和UID，这个具体的操作在**IPCThreadState::clearCaller()**里面，最后返回token

###### 1.1.1、IPCThreadState::clearCaller()

代码在[IPCThreadState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 356行

```
void IPCThreadState::clearCaller()
{
    mCallingPid = getpid();
    mCallingUid = getuid();
}
```

###### 2、JNI:restoreCallingIdentity

代码在[android\_util\_Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 776行

```
static void android_os_Binder_restoreCallingIdentity(JNIEnv* env, jobject clazz, jlong token)
{
    // XXX temporary sanity check to debug crashes.
    //token记录着uid信息，将其右移32位得到的是uid
    int uid = (int)(token>>32);
    if (uid > 0 && uid < 999) {
        // In Android currently there are no uids in this range.
        //目前android系统不存在小于999的uid，所以uid<999则抛出异常。
        char buf[128];
        sprintf(buf, "Restoring bad calling ident: 0x%" PRIx64, token);
        jniThrowException(env, "java/lang/IllegalStateException", buf);
        return;
    }
    IPCThreadState::self()->restoreCallingIdentity(token);
}
```

这个方法主要是获取uid，然后调用IPCThreadState的restoreCallingIdentity(token)方法

###### 2.1、restoreCallingIdentity

代码在[IPCThreadState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 383行

```
void IPCThreadState::restoreCallingIdentity(int64_t token)
{
    mCallingUid = (int)(token>>32);
    mCallingPid = (int)token;
}
```

从token中解析出PID和UID，并赋值给相应的变量。该方法正好是clearCallingIdentity反过程。

###### 3、JNI:getCallingPid

代码在[android\_util\_Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 761行

```
static jint android_os_Binder_getCallingPid(JNIEnv* env, jobject clazz)
{
    return IPCThreadState::self()->getCallingPid();
}
```

调用的是IPCThreadState的getCallingPid()方法

###### 3.1、IPCThreadState::getCallingPid

代码在[IPCThreadState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 346行

```
pid_t IPCThreadState::getCallingPid() const
{
    return mCallingPid;
}
```

直接返回mCallingPid

###### 4、JNI:getCallingUid

代码在[android\_util\_Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 766行

```
static jint android_os_Binder_getCallingUid(JNIEnv* env, jobject clazz)
{
    return IPCThreadState::self()->getCallingUid();
}
```

调用的是IPCThreadState的getCallingUid()方法

###### 4.1、IPCThreadState::getCallingUid

代码在[IPCThreadState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 346行

```
uid_t IPCThreadState::getCallingUid() const
{
    return mCallingUid;
}
```

直接返回mCallingUid

###### 5、远程调用

###### 5.1、binder\_thread\_read

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 的2654行

```
binder_thread_read（）{
    ...
    while (1) {
      struct binder_work *w;
      switch (w->type) {
        case BINDER_WORK_TRANSACTION:
            t = container_of(w, struct binder_transaction, work);
            break;
        case :...
      }
      if (!t)
        continue; //只有BR_TRANSACTION,BR_REPLY才会往下执行
        
      tr.code = t->code;
      tr.flags = t->flags;
      tr.sender_euid = t->sender_euid; //mCallingUid

      if (t->from) {
          struct task_struct *sender = t->from->proc->tsk;
          //当非oneway的情况下,将调用者进程的pid保存到sender_pid
          tr.sender_pid = task_tgid_nr_ns(sender,current->nsproxy->pid_ns);
      } else {
          //当oneway的的情况下,则该值为0
          tr.sender_pid = 0;
      }
      ...
}
```

###### 5.2、IPCThreadState. executeCommand()

代码在[IPCThreadState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 的947行

```
status_t IPCThreadState::executeCommand(int32_t cmd)
{
    BBinder* obj;
    RefBase::weakref_type* refs;
    status_t result = NO_ERROR;

    switch ((uint32_t)cmd) {
        case BR_TRANSACTION:
        {
            const pid_t origPid = mCallingPid;
            const uid_t origUid = mCallingUid;
            // 设置调用者pid
            mCallingPid = tr.sender_pid; 
            // 设置调用者uid
            mCallingUid = tr.sender_euid;
            ...
            reinterpret_cast<BBinder*>(tr.cookie)->transact(tr.code, buffer,
                        &reply, tr.flags);
            // 恢复原来的pid
            mCallingPid = origPid; 
             // 恢复原来的uid
            mCallingUid = origUid; 
        }
        
        case :...
    }
}
```

关于mCallingPid、mCallingUid修改过程：是在每次Binder Call的远程进程在执行binder\_thread\_read()过程中，会设置pid和uid，然后在IPCThreadState的transact收到BR\_TRANSACTION则会修改mCallingPid，mCallingUid。

> PS:当oneway的情况下：mCallingPid=0，不过mCallingUid可以拿到正确值

##### (三) 思考

###### 1、场景分析：

###### （1）场景：比如线程X通过Binder远程调用线程Y，然后线程Y通过Binder调用当前线程的另一个Service或者activity之类的组件。

###### （2）分析：

-   1 线程X通过Binder远程调用线程Y：则线程Y的IPCThreadState中的mCallingUid和mCallingPid保存的就是线程X的UID和PID。这时在线程Y中调用Binder.getCallingPid()和Binder.getCallingUid()方法便可获取线程X的UID和PID，然后利用UID和PID进行权限对比，判断线程X是否有权限调用线程Y的某个方法
-   2 线程Y通过Binder调用当前线程的某个组件：此时线程Y是线程Y某个组件的调用端，则mCallingUid和mCallingPid应该保存当前线程Y的PID和UID，故需要调用clearCallingIdentity()方法完成这个功能。当前线程Y调用完某个组件，由于线程Y仍然处于线程A的被用调用端，因此mCallingUidh和mCallingPid需要回复线程A的UID和PID，这时调用restoreCallingIdentity()即完成。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/48vh5hbeo1.png)

场景分析.png

一句话：图中过程2(调用组件2开始之前)执行clearCallingIdentity()，过程3(调用组件2结束之后)执行restoreCallingIdentity()。

###### 2、实例分析：

上述过程主要在system\_server进程各个线程中比较常见(普通app应用很少出现)，比如system\_server进程中的ActivityManagerService子线程

代码在[ActivityManagerService.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fservices%2Fcore%2Fjava%2Fcom%2Fandroid%2Fserver%2Fam%2FActivityManagerService.java&objectId=1199111&objectType=1&isNewArticle=undefined) 6246行

```
    @Override
    public final void attachApplication(IApplicationThread thread) {
        synchronized (this) {
            //获取远程Binder调用端的pid
            int callingPid = Binder.getCallingPid();
            // 清除远程Binder调用端的uid和pid信息，并保存到origId变量
            final long origId = Binder.clearCallingIdentity();
            attachApplicationLocked(thread, callingPid);
            //通过origId变量，还原远程Binder调用端的uid和pid信息
            Binder.restoreCallingIdentity(origId);
        }
    }
```

> **attachApplication()**该方法一般是system\_server进程的子线程调用远程进程时使用，而**attachApplicationLocked()**方法则在同一个线程中，故需要在调用该方法前清空远程调用该方法清空远程调用者的uid和pid，调用结束后恢复远程调用者的uid和pid。

### 三、Binder的死亡通知机制

##### (一)、概述

> 死亡通知时为了让Bp端(客户端进程)能知晓Bn端(服务端进程)的生死情况，当Bn进程死亡后能通知到Bp端。

-   定义：AppDeathRecipient是继承IBinder::DeathRecipient类，主要需要实现其binderDied()来进行死亡通告。
-   注册：binder->linkToDeath(AppDeathRecipient)是为了将AppDeathRecipient死亡通知注册到Binder上

Bn端只需要重写binderDied()方法，实现一些后尾清楚类的工作，则在Bn端死掉后，会回调binderDied()进行相应处理。

##### (二)、注册死亡通知

###### 1、Java层代码

代码在[ActivityManagerService.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fservices%2Fcore%2Fjava%2Fcom%2Fandroid%2Fserver%2Fam%2FActivityManagerService.java&objectId=1199111&objectType=1&isNewArticle=undefined) 6016行

```
public final class ActivityManagerService {
    private final boolean attachApplicationLocked(IApplicationThread thread, int pid) {
        ...
        //创建IBinder.DeathRecipient子类对象
        AppDeathRecipient adr = new AppDeathRecipient(app, pid, thread);
        //建立binder死亡回调
        thread.asBinder().linkToDeath(adr, 0);
        app.deathRecipient = adr;
        ...
        //取消binder死亡回调
        app.unlinkDeathRecipient();
    }

    private final class AppDeathRecipient implements IBinder.DeathRecipient {
        ...
        public void binderDied() {
            synchronized(ActivityManagerService.this) {
                appDiedLocked(mApp, mPid, mAppThread, true);
            }
        }
    }
}
```

这里面涉及两个方法linkToDeath和unlinkToDeath方法，实现如下：

###### 1.1、linkToDeath()与unlinkToDeath()

代码在[ActivityManagerService.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FBinder.java&objectId=1199111&objectType=1&isNewArticle=undefined) 397行

```
public class Binder implements IBinder {
    public void linkToDeath(DeathRecipient recipient, int flags) {
    }

    public boolean unlinkToDeath(DeathRecipient recipient, int flags) {
        return true;
    }
}
```

代码在[ActivityManagerService.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FBinder.java&objectId=1199111&objectType=1&isNewArticle=undefined) 509行

```
final class BinderProxy implements IBinder {
    public native void linkToDeath(DeathRecipient recipient, int flags)
            throws RemoteException;
    public native boolean unlinkToDeath(DeathRecipient recipient, int flags);
}
```

可见，以上两个方法：

-   当为Binder服务端，则相应的两个方法实现为空，没有实际功能；
-   当为BinderProxy代理端，则调用native方法来实现相应功能，这是真实使用场景

###### 2、JNI及Native层代码

native方法linkToDeath()和unlinkToDeath() 通过JNI实现，我们来依次了解。

###### 2.1 android\_os\_BinderProxy\_linkToDeath()

代码在[android\_util\_Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 397行

```
static void android_os_BinderProxy_linkToDeath(JNIEnv* env, jobject obj,
        jobject recipient, jint flags)
{
    if (recipient == NULL) {
        jniThrowNullPointerException(env, NULL);
        return;
    }

    //第一步 获取BinderProxy.mObject成员变量值, 即BpBinder对象
    IBinder* target = (IBinder*)env->GetLongField(obj, gBinderProxyOffsets.mObject);
    ...

    //只有Binder代理对象才会进入该对象
    if (!target->localBinder()) {
        DeathRecipientList* list = (DeathRecipientList*)
                env->GetLongField(obj, gBinderProxyOffsets.mOrgue);
        //第二步 创建JavaDeathRecipient对象
        sp<JavaDeathRecipient> jdr = new JavaDeathRecipient(env, recipient, list);
        //第三步 建立死亡通知
        status_t err = target->linkToDeath(jdr, NULL, flags);
        if (err != NO_ERROR) {
            //如果添加失败，第四步 ， 则从list移除引用
            jdr->clearReference();
            signalExceptionForError(env, obj, err, true /*canThrowRemoteException*/);
        }
    }
}
```

大体流程是:

-   第一步，获取BpBinder对象
-   第二步，构建JavaDeathRecipient对象
-   第三步，调用BpBinder的linkToDeath，建立死亡通知
-   第四步，如果添加死亡通知失败，则调用JavaDeathRecipient的clearReference移除

补充说明：

-   获取DeathRecipientList：其成员变量mList记录该BinderProxy的JavaDeathRecipient列表信息(一个BpBinder可以注册多个死亡回调)
-   创建JavaDeathRecipient：继承与IBinder::DeathRecipient

那我们就依照上面四个步骤依次详细了解下,获取BpBinder对象的过程和之前讲解Binder一样，这里就不详细说明了，直接从第二步开始。

###### 2.1.1 JavaDeathRecipient类

代码在[android\_util\_Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 348行

```
class JavaDeathRecipient : public IBinder::DeathRecipient
{
public:
    JavaDeathRecipient(JNIEnv* env, jobject object, const sp<DeathRecipientList>& list)
        : mVM(jnienv_to_javavm(env)), mObject(env->NewGlobalRef(object)),
          mObjectWeak(NULL), mList(list)
    {
        //将当前对象sp添加到列表DeathRecipientList
        list->add(this);
        android_atomic_inc(&gNumDeathRefs);
        incRefsCreated(env); 
    }
}
```

该方法主要功能：

-   通过env->NewGloablRef(object)，为recipient创建相应的全局引用，并保存到mObject成员变量
-   将当前对象JavaDeathRecipient强指针sp添加到DeathRecipientList

这里说下DeathRecipient关系图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/w9sh7fjwlb.png)

DeathRecipient关系图.png

> 其中Java层的BinderProxy.mOrgue 指向DeathRecipientList，而DeathRecipientList记录JavaDeathRecipient对象

最后调用了incRefsCreated()函数，让我们来看下

###### 2.1.1.1 incRefsCreated()函数

代码在[android\_util\_Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 144行

```
static void incRefsCreated(JNIEnv* env)
{
    int old = android_atomic_inc(&gNumRefsCreated);
    if (old == 200) {
        android_atomic_and(0, &gNumRefsCreated);
        //出发forceGc
        env->CallStaticVoidMethod(gBinderInternalOffsets.mClass,
                gBinderInternalOffsets.mForceGc);
    } else {
        ALOGV("Now have %d binder ops", old);
    }
}
```

该方法的主要作用是增加引用计数incRefsCreated，每计数增加200则执行一次forceGC; 会触发调用incRefsCreated()的场景有：

-   JavaBBinder 对象创建过程
-   JavaDeathRecipient对象创建过程
-   javaObjectForIBinder()方法：将native层的BpBinder对象转换为Java层BinderProxy对象的过程

###### 2.1.2 BpBinder::linkToDeath()

代码在[BpBinder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FBpBinder.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 173行

```
status_t BpBinder::linkToDeath(
    const sp<DeathRecipient>& recipient, void* cookie, uint32_t flags)
{
    Obituary ob;
    //recipient 该对象为JavaDeathRecipient
    ob.recipient = recipient;
    // cookie 为null
    ob.cookie = cookie;
    // flags=0;
    ob.flags = flags;

    LOG_ALWAYS_FATAL_IF(recipient == NULL,
                        "linkToDeath(): recipient must be non-NULL");

    {
        AutoMutex _l(mLock);
        if (!mObitsSent) {
             // 没有执行过sendObituary，则进入该方法
            if (!mObituaries) {
                mObituaries = new Vector<Obituary>;
                if (!mObituaries) {
                    return NO_MEMORY;
                }
                ALOGV("Requesting death notification: %p handle %d\n", this, mHandle);
                getWeakRefs()->incWeak(this);
                IPCThreadState* self = IPCThreadState::self();
                //具体调用步骤1
                self->requestDeathNotification(mHandle, this);
                // 具体调用步骤2
                self->flushCommands();
            }
            // 将创新的Obituary添加到mbituaries
            ssize_t res = mObituaries->add(ob);
            return res >= (ssize_t)NO_ERROR ? (status_t)NO_ERROR : res;
        }
    }
    return DEAD_OBJECT;
}
```

这里面的核心代码的就是分别调用了\*\* IPCThreadState的requestDeathNotification(mHandle, this)**函数和**flushCommands()\*\*函数，那我们就一次来看下

###### 2.1.2.1 IPCThreadState::requestDeathNotification()函数

代码在[BpBinder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 670行

```
status_t IPCThreadState::requestDeathNotification(int32_t handle, BpBinder* proxy)
{
    mOut.writeInt32(BC_REQUEST_DEATH_NOTIFICATION);
    mOut.writeInt32((int32_t)handle);
    mOut.writePointer((uintptr_t)proxy);
    return NO_ERROR;
}
```

进入Binder Driver后，直接调用后进入binder\_thread\_write处理BC\_REQUEST\_DEATH\_NOTIFICATION命令

###### 2.1.2.2 IPCThreadState::flushCommands()函数

代码在[BpBinder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 395行

```
void IPCThreadState::flushCommands()
{
    if (mProcess->mDriverFD <= 0)
        return;
    talkWithDriver(false);
}
```

flushCommands就是把命令向驱动发出，此处参数是false，则不会阻塞等待读。向Linux Kernel层的Binder Driver发送 BC\_REQEUST\_DEATH\_NOTIFACTION命令，经过ioctl执行到binder\_ioctl\_write\_read()方法。

###### 2.1.3 clearReference()函数

代码在[android\_util\_Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199111&objectType=1&isNewArticle=undefined) 412行

```
void clearReference()
 {
     sp<DeathRecipientList> list = mList.promote();
     if (list != NULL) {
         // 从列表中移除
         list->remove(this); 
     }
 }
```

###### 3、Linux Kernel层代码

###### 3.1、binder\_ioctl\_write\_read()函数

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 3138行

```
static int binder_ioctl_write_read(struct file *filp,
                unsigned int cmd, unsigned long arg,
                struct binder_thread *thread)
{
    int ret = 0;
    struct binder_proc *proc = filp->private_data;
    void __user *ubuf = (void __user *)arg;
    struct binder_write_read bwr;
    // 把用户控件数据ubuf拷贝到bwr
    if (copy_from_user(&bwr, ubuf, sizeof(bwr))) { 
        ret = -EFAULT;
        goto out;
    }
    // 此时写入缓存数据
    if (bwr.write_size > 0) { 
        ret = binder_thread_write(proc, thread,
                  bwr.write_buffer, bwr.write_size, &bwr.write_consumed);
         ...
    }
    //此时读缓存没有数据
    if (bwr.read_size > 0) {
      ...
    }
    // 将内核数据bwr拷贝到用户控件ubuf
    if (copy_to_user(ubuf, &bwr, sizeof(bwr))) { 
        ret = -EFAULT;
        goto out;
    }
out:
    return ret;
}
```

主要调用binder\_thread\_write来读写缓存数据，按我们来看下**binder\_thread\_write()**函数

###### 3.2、binder\_thread\_write()函数

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 2252行

```
static int binder_thread_write(struct binder_proc *proc,
      struct binder_thread *thread,
      binder_uintptr_t binder_buffer, size_t size,
      binder_size_t *consumed)
{
  uint32_t cmd;
  //proc, thread都是指当前发起端进程的信息
  struct binder_context *context = proc->context;
  void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
  void __user *ptr = buffer + *consumed; 
  void __user *end = buffer + size;
  while (ptr < end && thread->return_error == BR_OK) {
    get_user(cmd, (uint32_t __user *)ptr); //获取BC_REQUEST_DEATH_NOTIFICATION
    ptr += sizeof(uint32_t);
    switch (cmd) {
        case BC_REQUEST_DEATH_NOTIFICATION:{ 
           //注册死亡通知
            uint32_t target;
            void __user *cookie;
            struct binder_ref *ref;
            struct binder_ref_death *death;
            //获取targe
            get_user(target, (uint32_t __user *)ptr); t
            ptr += sizeof(uint32_t);
             //获取BpBinder
            get_user(cookie, (void __user * __user *)ptr); 
            ptr += sizeof(void *);
            //拿到目标服务的binder_ref
            ref = binder_get_ref(proc, target); 

            if (cmd == BC_REQUEST_DEATH_NOTIFICATION) {
                //native Bp可注册多个，但Kernel只允许注册一个死亡通知
                if (ref->death) {
                    break; 
                }
                death = kzalloc(sizeof(*death), GFP_KERNEL);

                INIT_LIST_HEAD(&death->work.entry);
                death->cookie = cookie;
                ref->death = death;
                //当目标binder服务所在进程已死,则直接发送死亡通知。这是非常规情况
                if (ref->node->proc == NULL) { 
                    ref->death->work.type = BINDER_WORK_DEAD_BINDER;
                    //当前线程为binder线程,则直接添加到当前线程的todo队列. 
                    if (thread->looper & (BINDER_LOOPER_STATE_REGISTERED | BINDER_LOOPER_STATE_ENTERED)) {
                        list_add_tail(&ref->death->work.entry, &thread->todo);
                    } else {
                        list_add_tail(&ref->death->work.entry, &proc->todo);
                        wake_up_interruptible(&proc->wait);
                    }
                }
            } else {
                ...
            }
        } break;
      case ...;
    }
    *consumed = ptr - buffer;
  }   
 }
```

> 该方法在处理BC\_REQUEST\_DEATH\_NOTIFACTION过程，正好遇到目标Binder进服务所在进程已死的情况，向todo队列增加BINDER\_WORK\_BINDER事务，直接发送死亡通知，但这属于非常规情况。

更常见的场景是binder服务所在进程死亡后，会调用binder\_release方法，然后调用binder\_node\_release。这个过程便会发出死亡通知的回调。

##### (三)、出发死亡通知

> 当Binder服务所在进程死亡后，会释放进程相关的资源，Binder也是一种资源。binder\_open打开binder驱动/dev/binder，这是字符设备，获取文件描述符。在进程结束的时候会有一个关闭文件系统的过程会调用驱动close方法，该方法相对应的是release()方法。当binder的fd被释放后，此处调用相应的方法是binder\_release()。

但并不是每个close系统调用都会出发调用release()方法。只有真正释放设备数据结构才调用release()，内核维持一个文件结构被使用多少次的技术，即便是应用程序没有明显地关闭它打开的文件也使用：内核在进程exit()时会释放所有内存和关闭相应的文件资源，通过使用close系统调用最终也会release binder。

###### 1、release

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 4172行

```
static const struct file_operations binder_fops = {
  .owner = THIS_MODULE,
  .poll = binder_poll,
  .unlocked_ioctl = binder_ioctl,
  .compat_ioctl = binder_ioctl,
  .mmap = binder_mmap,
  .open = binder_open,
  .flush = binder_flush,
   //对应着release的方法
  .release = binder_release, 
};
```

那我们来看下binder\_release

###### 2、binder\_release

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 3536行

```
static int binder_release(struct inode *nodp, struct file *filp)
{
    struct binder_proc *proc = filp->private_data;

    debugfs_remove(proc->debugfs_entry);
    binder_defer_work(proc, BINDER_DEFERRED_RELEASE);

    return 0;
}
```

我们看到里面调用了binder\_defer\_work()函数，那我们一起继续看下

###### 3、binder\_defer\_work

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 3739行

```
static void
binder_defer_work(struct binder_proc *proc, enum binder_deferred_state defer)
{
        //获取锁
    mutex_lock(&binder_deferred_lock);
        // 添加BINDER_DEFERRED_RELEASE
    proc->deferred_work |= defer;
    if (hlist_unhashed(&proc->deferred_work_node)) {
        hlist_add_head(&proc->deferred_work_node,
                &binder_deferred_list);
                //向工作队列添加binder_derred_work
        schedule_work(&binder_deferred_work);
    }
        // 释放锁
    mutex_unlock(&binder_deferred_lock);
}
```

这里面涉及到一个结构体**binder\_deferred\_workqueue**，那我们就来看下

###### 4、binder\_deferred\_workqueue

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 3737行

```
static DECLARE_WORK(binder_deferred_work, binder_deferred_func);
```

代码在[workqueue.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Flinux%2Fworkqueue.h&objectId=1199111&objectType=1&isNewArticle=undefined) 183行

```
#define DECLARE_WORK(n, f)                      \
    struct work_struct n = __WORK_INITIALIZER(n, f)
```

代码在[workqueue.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Flinux%2Fworkqueue.h&objectId=1199111&objectType=1&isNewArticle=undefined) 169行

```
#define __WORK_INITIALIZER(n, f) {          \
  .data = WORK_DATA_STATIC_INIT(),        \
  .entry  = { &(n).entry, &(n).entry },        \
  .func = (f),              \
  __WORK_INIT_LOCKDEP_MAP(#n, &(n))        \
  }
```

上面看起来有点凌乱，那我们合起来看

```
static DECLARE_WORK(binder_deferred_work, binder_deferred_func);

#define DECLARE_WORK(n, f)            \
  struct work_struct n = __WORK_INITIALIZER(n, f)

#define __WORK_INITIALIZER(n, f) {          \
  .data = WORK_DATA_STATIC_INIT(),        \
  .entry  = { &(n).entry, &(n).entry },        \
  .func = (f),              \
  __WORK_INIT_LOCKDEP_MAP(#n, &(n))        \
  }
```

那么 他是什么时候被初始化的？

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 4215行

```
//全局工作队列
static struct workqueue_struct *binder_deferred_workqueue;
static int __init binder_init(void)
{
  int ret;
  //创建了名叫“binder”的工作队列
  binder_deferred_workqueue = create_singlethread_workqueue("binder");
  if (!binder_deferred_workqueue)
    return -ENOMEM;
  ...
}

device_initcall(binder_init);
```

在Binder设备驱动初始化过程中执行binder\_init()方法中，调用**create\_singlethread\_workqueue(“binder”)**，创建了名叫**"binder"**的工作队列(workqueue)。workqueue是kernel提供的一种实现简单而有效的内核线程机制，可延迟执行任务。 此处的binder\_deferred\_work的func为binder\_deferred\_func,接下来看该方法。

###### 5、binder\_deferred\_work

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 2697行

```
static void binder_deferred_func(struct work_struct *work)
{
    struct binder_proc *proc;
    struct files_struct *files;

    int defer;
    do {
        binder_lock(__func__);
                // 获取binder_main_lock
        mutex_lock(&binder_deferred_lock);
        if (!hlist_empty(&binder_deferred_list)) {
            proc = hlist_entry(binder_deferred_list.first,
                    struct binder_proc, deferred_work_node);
            hlist_del_init(&proc->deferred_work_node);
            defer = proc->deferred_work;
            proc->deferred_work = 0;
        } else {
            proc = NULL;
            defer = 0;
        }
        mutex_unlock(&binder_deferred_lock);

        files = NULL;
        if (defer & BINDER_DEFERRED_PUT_FILES) {
            files = proc->files;
            if (files)
                proc->files = NULL;
        }

        if (defer & BINDER_DEFERRED_FLUSH)
            binder_deferred_flush(proc);

        if (defer & BINDER_DEFERRED_RELEASE)
                         // 核心代码，调用binder_deferred_release()
            binder_deferred_release(proc); /* frees proc */

        binder_unlock(__func__);
        if (files)
            put_files_struct(files);
    } while (proc);
}
```

可见，binder\_release最终调用的是binder\_deferred\_release；同理，binder\_flush最终调用的是binder\_deferred\_flush。

###### 6、binder\_deferred\_release

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 3590行

```
static void binder_deferred_release(struct binder_proc *proc)
{
    struct binder_transaction *t;
    struct binder_context *context = proc->context;
    struct rb_node *n;
    int threads, nodes, incoming_refs, outgoing_refs, buffers,
        active_transactions, page_count;

    BUG_ON(proc->vma);
    BUG_ON(proc->files);

        //删除proc_node节点
    hlist_del(&proc->proc_node);

    if (context->binder_context_mgr_node &&
        context->binder_context_mgr_node->proc == proc) {
        binder_debug(BINDER_DEBUG_DEAD_BINDER,
                 "%s: %d context_mgr_node gone\n",
                 __func__, proc->pid);
        context->binder_context_mgr_node = NULL;
    }
  
        //释放binder_thread
    threads = 0;
    active_transactions = 0;
    while ((n = rb_first(&proc->threads))) {
        struct binder_thread *thread;

        thread = rb_entry(n, struct binder_thread, rb_node);
        threads++;
        active_transactions += binder_free_thread(proc, thread);
    }

        //释放binder_node
    nodes = 0;
    incoming_refs = 0;
    while ((n = rb_first(&proc->nodes))) {
        struct binder_node *node;

        node = rb_entry(n, struct binder_node, rb_node);
        nodes++;
        rb_erase(&node->rb_node, &proc->nodes);
        incoming_refs = binder_node_release(node, incoming_refs);
    }

        //释放binder_ref
    outgoing_refs = 0;
    while ((n = rb_first(&proc->refs_by_desc))) {
        struct binder_ref *ref;

        ref = rb_entry(n, struct binder_ref, rb_node_desc);
        outgoing_refs++;
        binder_delete_ref(ref);
    }

        //释放binder_work
    binder_release_work(&proc->todo);
    binder_release_work(&proc->delivered_death);

    buffers = 0;
    while ((n = rb_first(&proc->allocated_buffers))) {
        struct binder_buffer *buffer;

        buffer = rb_entry(n, struct binder_buffer, rb_node);

        t = buffer->transaction;
        if (t) {
            t->buffer = NULL;
            buffer->transaction = NULL;
            pr_err("release proc %d, transaction %d, not freed\n",
                   proc->pid, t->debug_id);
            /*BUG();*/
        }

                //释放binder_buf
        binder_free_buf(proc, buffer);
        buffers++;
    }

    binder_stats_deleted(BINDER_STAT_PROC);

    page_count = 0;
    if (proc->pages) {
        int i;

        for (i = 0; i < proc->buffer_size / PAGE_SIZE; i++) {
            void *page_addr;

            if (!proc->pages[i])
                continue;

            page_addr = proc->buffer + i * PAGE_SIZE;
            binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
                     "%s: %d: page %d at %p not freed\n",
                     __func__, proc->pid, i, page_addr);
            unmap_kernel_range((unsigned long)page_addr, PAGE_SIZE);
            __free_page(proc->pages[i]);
            page_count++;
        }
        kfree(proc->pages);
        vfree(proc->buffer);
    }

    put_task_struct(proc->tsk);

    binder_debug(BINDER_DEBUG_OPEN_CLOSE,
             "%s: %d threads %d, nodes %d (ref %d), refs %d, active transactions %d, buffers %d, pages %d\n",
             __func__, proc->pid, threads, nodes, incoming_refs,
             outgoing_refs, active_transactions, buffers, page_count);

    kfree(proc);
}
```

此处proc是来自Bn端的binder\_proc. binder\_defered\_release的主要工作有：

-   binder\_free\_thread(proc,thread)
-   binder\_node\_release(node,incoming\_refs)
-   binder\_delete\_ref(ref) -binder\_release\_work(&proc->todo) -binder\_release\_work(&proc->delivered\_death) -binder\_free\_buff(proc,buffer) -以及释放各种内存信息

###### 6.1、binder\_free\_thread

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 3065行

```
static int binder_free_thread(struct binder_proc *proc,
                  struct binder_thread *thread)
{
    struct binder_transaction *t;
    struct binder_transaction *send_reply = NULL;
    int active_transactions = 0;

    rb_erase(&thread->rb_node, &proc->threads);
    t = thread->transaction_stack;
    if (t && t->to_thread == thread)
        send_reply = t;
    while (t) {
        active_transactions++;
        binder_debug(BINDER_DEBUG_DEAD_TRANSACTION,
                 "release %d:%d transaction %d %s, still active\n",
                  proc->pid, thread->pid,
                 t->debug_id,
                 (t->to_thread == thread) ? "in" : "out");

        if (t->to_thread == thread) {
            t->to_proc = NULL;
            t->to_thread = NULL;
            if (t->buffer) {
                t->buffer->transaction = NULL;
                t->buffer = NULL;
            }
            t = t->to_parent;
        } else if (t->from == thread) {
            t->from = NULL;
            t = t->from_parent;
        } else
            BUG();
    }
        //发送失败回复
    if (send_reply)
        binder_send_failed_reply(send_reply, BR_DEAD_REPLY);
    binder_release_work(&thread->todo);
    kfree(thread);
    binder_stats_deleted(BINDER_STAT_THREAD);
    return active_transactions;
}
```

###### 6.2、binder\_node\_release

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 3546行

```
static int binder_node_release(struct binder_node *node, int refs)
{
    struct binder_ref *ref;
    int death = 0;

    list_del_init(&node->work.entry);
    binder_release_work(&node->async_todo);

    if (hlist_empty(&node->refs)) {
                //引用为空，直接删除节点
        kfree(node);
        binder_stats_deleted(BINDER_STAT_NODE);

        return refs;
    }

    node->proc = NULL;
    node->local_strong_refs = 0;
    node->local_weak_refs = 0;
    hlist_add_head(&node->dead_node, &binder_dead_nodes);

    hlist_for_each_entry(ref, &node->refs, node_entry) {
        refs++;

        if (!ref->death)
            continue;

        death++;

        if (list_empty(&ref->death->work.entry)) {
                       //添加BINDER_WORK_DEAD_BINDER事务到todo队列
            ref->death->work.type = BINDER_WORK_DEAD_BINDER;
            list_add_tail(&ref->death->work.entry,
                      &ref->proc->todo);
            wake_up_interruptible(&ref->proc->wait);
        } else
            BUG();
    }
    binder_debug(BINDER_DEBUG_DEAD_BINDER,
             "node %d now dead, refs %d, death %d\n",
             node->debug_id, refs, death);
    return refs;
}
```

该方法会遍历该binder\_node所有的binder\_ref，当存在binder希望通知，则向相应的binder\_ref所在进程的todo队列添加BINDER\_WORK\_DEAD\_BINDER事务并唤醒处于proc->wait的binder线程。

###### 6.3、binder\_delete\_ref

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 1133行

```
static void binder_delete_ref(struct binder_ref *ref)
{
    binder_debug(BINDER_DEBUG_INTERNAL_REFS,
             "%d delete ref %d desc %d for node %d\n",
              ref->proc->pid, ref->debug_id, ref->desc,
              ref->node->debug_id);

    rb_erase(&ref->rb_node_desc, &ref->proc->refs_by_desc);
    rb_erase(&ref->rb_node_node, &ref->proc->refs_by_node);
    if (ref->strong)
        binder_dec_node(ref->node, 1, 1);
    hlist_del(&ref->node_entry);
    binder_dec_node(ref->node, 0, 1);
    if (ref->death) {
        binder_debug(BINDER_DEBUG_DEAD_BINDER,
                 "%d delete ref %d desc %d has death notification\n",
                  ref->proc->pid, ref->debug_id, ref->desc);
        list_del(&ref->death->work.entry);
        kfree(ref->death);
        binder_stats_deleted(BINDER_STAT_DEATH);
    }
    kfree(ref);
    binder_stats_deleted(BINDER_STAT_REF);
}
```

###### 6.4、binder\_delete\_ref

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 2980行

```
static void binder_release_work(struct list_head *list)
{
    struct binder_work *w;

    while (!list_empty(list)) {
        w = list_first_entry(list, struct binder_work, entry);
                 //删除 binder_work
        list_del_init(&w->entry);
        switch (w->type) {
        case BINDER_WORK_TRANSACTION: {
            struct binder_transaction *t;

            t = container_of(w, struct binder_transaction, work);
            if (t->buffer->target_node &&
                !(t->flags & TF_ONE_WAY)) {
                                 //发送failed回复
                binder_send_failed_reply(t, BR_DEAD_REPLY);
            } else {
                binder_debug(BINDER_DEBUG_DEAD_TRANSACTION,
                    "undelivered transaction %d\n",
                    t->debug_id);
                t->buffer->transaction = NULL;
                kfree(t);
                binder_stats_deleted(BINDER_STAT_TRANSACTION);
            }
        } break;
        case BINDER_WORK_TRANSACTION_COMPLETE: {
            binder_debug(BINDER_DEBUG_DEAD_TRANSACTION,
                "undelivered TRANSACTION_COMPLETE\n");
            kfree(w);
            binder_stats_deleted(BINDER_STAT_TRANSACTION_COMPLETE);
        } break;
        case BINDER_WORK_DEAD_BINDER_AND_CLEAR:
        case BINDER_WORK_CLEAR_DEATH_NOTIFICATION: {
            struct binder_ref_death *death;

            death = container_of(w, struct binder_ref_death, work);
            binder_debug(BINDER_DEBUG_DEAD_TRANSACTION,
                "undelivered death notification, %016llx\n",
                (u64)death->cookie);
            kfree(death);
            binder_stats_deleted(BINDER_STAT_DEATH);
        } break;
        default:
            pr_err("unexpected work type, %d, not freed\n",
                   w->type);
            break;
        }
    }
}
```

###### 6.4、binder\_delete\_ref

代码在[binder.c](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Fdrivers%2Fandroid%2Fbinder.c&objectId=1199111&objectType=1&isNewArticle=undefined) 2980行

```
static void binder_free_buf(struct binder_proc *proc,
                struct binder_buffer *buffer)
{
    size_t size, buffer_size;

    buffer_size = binder_buffer_size(proc, buffer);

    size = ALIGN(buffer->data_size, sizeof(void *)) +
        ALIGN(buffer->offsets_size, sizeof(void *)) +
        ALIGN(buffer->extra_buffers_size, sizeof(void *));

    binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
             "%d: binder_free_buf %p size %zd buffer_size %zd\n",
              proc->pid, buffer, size, buffer_size);

    BUG_ON(buffer->free);
    BUG_ON(size > buffer_size);
    BUG_ON(buffer->transaction != NULL);
    BUG_ON((void *)buffer < proc->buffer);
    BUG_ON((void *)buffer > proc->buffer + proc->buffer_size);

    if (buffer->async_transaction) {
        proc->free_async_space += size + sizeof(struct binder_buffer);

        binder_debug(BINDER_DEBUG_BUFFER_ALLOC_ASYNC,
                 "%d: binder_free_buf size %zd async free %zd\n",
                  proc->pid, size, proc->free_async_space);
    }

    binder_update_page_range(proc, 0,
        (void *)PAGE_ALIGN((uintptr_t)buffer->data),
        (void *)(((uintptr_t)buffer->data + buffer_size) & PAGE_MASK),
        NULL);
    rb_erase(&buffer->rb_node, &proc->allocated_buffers);
    buffer->free = 1;
    if (!list_is_last(&buffer->entry, &proc->buffers)) {
        struct binder_buffer *next = list_entry(buffer->entry.next,
                        struct binder_buffer, entry);

        if (next->free) {
            rb_erase(&next->rb_node, &proc->free_buffers);
            binder_delete_free_buffer(proc, next);
        }
    }
    if (proc->buffers.next != &buffer->entry) {
        struct binder_buffer *prev = list_entry(buffer->entry.prev,
                        struct binder_buffer, entry);

        if (prev->free) {
            binder_delete_free_buffer(proc, buffer);
            rb_erase(&prev->rb_node, &proc->free_buffers);
            buffer = prev;
        }
    }
    binder_insert_free_buffer(proc, buffer);
}
```

##### (四)、总结

> 对于Binder IPC进程都会打开/dev/binder文件，当进程异常退出时，Binder驱动会保证释放将要退出的进程中没有正常关闭的/dev/binder文件，实现机制是binder驱动通过调用/dev/binder文件所在对应的release回调函数，执行清理工作，并且检查BBinder是否注册死亡通知，当发现存在死亡通知时，就向其对应的BpBinder端发送死亡通知消息。

死亡回调DeathRecipient只有Bp才能正确使用，因为DeathRecipient用于监控Bn挂掉的情况，如果Bn建立跟自己的死亡通知，自己进程都挂了，就无法通知了。

清空引用，将JavaDeathRecipient从DeathRecipientList列表移除。