---
link: https://blog.csdn.net/qq_40587575/article/details/130394731
title: 【安卓源码】Binder机制3 -- Binder线程池
description: 文章浏览阅读1.4k次。Binder本身是C/S架构，就可能存在多个Client会同时访问Server的情况。在这种情况下，如果Server只有一个线程处理响应，就会导致客户端的请求可能需要排队而导致响应过慢的现象发生。解决这个问题的方法就是引入多线程。【多个客户端不同线程去请求，服务端需要使用多线程机制，binder线程池，创建多个线程去回复多个客户端的请求】Binder机制的设计从最底层–驱动层，就考虑到了对于多线程的支持。_binder线程池
keywords: binder线程池
author: 蜘蛛侠不会飞 Csdn认证博客专家 Csdn认证企业博客 码龄6年 暂无认证
date: 2024-01-01T16:27:53.000Z
publisher: null
stats: paragraph=175 sentences=73, words=2541
---
Binder本身是C/S架构，就可能存在多个Client会同时访问Server的情况。 在这种情况下，如果Server只有一个线程处理响应，就会导致客户端的请求可能需要排队而导致响应过慢的现象发生。解决这个问题的方法就是引入多线程。【多个客户端不同线程去请求，服务端需要使用多线程机制，binder线程池，创建多个线程去回复多个客户端的请求】

Binder机制的设计从最底层–驱动层，就考虑到了对于多线程的支持。具体内容如下：

> a. 使用 Binder 的进程在启动之后，通过 BINDER_SET_MAX_THREADS 告知驱动其支持的最大线程数量
b. 驱动会对线程进行管理。在 binder_proc 结构中，这些字段记录了进程中线程的信息：max_threads，requested_threads，requested_threads_started，ready_threads
c. binder_thread 结构对应了 Binder 进程中的线程
d. 驱动通过 BR_SPAWN_LOOPER 命令告知进程需要创建一个新的线程
c. 进程通过 BC_ENTER_LOOPER 命令告知驱动其主线程已经ready
d. 进程通过 BC_REGISTER_LOOPER 命令告知驱动其子线程（非主线程）已经ready
e. 进程通过 BC_EXIT_LOOPER 命令告知驱动其线程将要退出
f. 在线程退出之后，通过 BINDER_THREAD_EXIT 告知Binder驱动。驱动将对应的 binder_thread 对象销毁

## 1. 最大的binder 数量

在每个进程启动时候，都会创建 ProcessState 对象，获得ProcessState对象是单例模式，从而保证每一个进程只有一个ProcessState对象。因此一个进程只打开binder设备一次,其中ProcessState的成员变量mDriverFD记录binder驱动的fd，用于访问binder设备。

```
// &#x5728;&#x521B;&#x5EFA; ProcessState &#x5BF9;&#x8C61;&#x7684;&#x65F6;&#x5019;&#xFF0C;&#x4F1A;&#x53BB;&#x6253;&#x5F00;driver
sp<processstate> ProcessState::self()
{
    Mutex::Autolock _l(gProcessMutex);
    if (gProcess != nullptr) {
        return gProcess;
    }
    gProcess = new ProcessState(kDefaultDriver);
    return gProcess;
}

// &#x6253;&#x5F00;&#x9A71;&#x52A8;&#x8BBE;&#x5907;
static int open_driver(const char *driver)
{
    int fd = open(driver, O_RDWR | O_CLOEXEC);
    if (fd >= 0) {
        int vers = 0;
        status_t result = ioctl(fd, BINDER_VERSION, &vers);
        if (result == -1) {
            ALOGE("Binder ioctl to obtain version failed: %s", strerror(errno));
            close(fd);
            fd = -1;
        }
        if (result != 0 || vers != BINDER_CURRENT_PROTOCOL_VERSION) {
          ALOGE("Binder driver protocol(%d) does not match user space protocol(%d)! ioctl() return value: %d",
                vers, BINDER_CURRENT_PROTOCOL_VERSION, result);
            close(fd);
            fd = -1;
        }

// &#x8BBE;&#x7F6E;&#x6700;&#x5927;&#x7684;&#x7EBF;&#x7A0B;&#x6570;&#x91CF;&#x4E3A;&#xFF1A;15
// #define DEFAULT_MAX_BINDER_THREADS 15
        size_t maxThreads = DEFAULT_MAX_BINDER_THREADS;

// &#x4E0E;binder &#x9A71;&#x52A8;&#x4EA4;&#x4E92;&#xFF0C;&#x8BBE;&#x7F6E;&#x9A71;&#x52A8;&#x7684;&#x7EBF;&#x7A0B;&#x6570;&#x91CF;&#x4E3A; 15&#x4E2A;
        result = ioctl(fd, BINDER_SET_MAX_THREADS, &maxThreads);
        if (result == -1) {
            ALOGE("Binder ioctl to set max threads failed: %s", strerror(errno));
        }
    } else {
        ALOGW("Opening '%s' failed: %s\n", driver, strerror(errno));
    }
    return fd;
}</processstate>
```

与binder 驱动交互，设置最大线程数量

```
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct binder_proc *proc = filp->private_data;
	struct binder_thread *thread;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;

	trace_binder_ioctl(cmd, arg);

	ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
	if (ret)
		goto err_unlocked;

	binder_lock(__func__);

// &#x8FD9;&#x91CC;&#x53EF;&#x4EE5;&#x901A;&#x8FC7;&#x8FDB;&#x7A0B;&#x83B7;&#x53D6;&#x5BF9;&#x5E94;&#x7684;&#x7EBF;&#x7A0B;
	thread = binder_get_thread(proc);
	if (thread == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	switch (cmd) {

&#x3002;&#x3002;&#x3002;
	case BINDER_SET_MAX_THREADS:

// &#x4FDD;&#x5B58;&#x5230; proc->max_threads
		if (copy_from_user(&proc->max_threads, ubuf, sizeof(proc->max_threads))) {
			ret = -EINVAL;
			goto err;
		}
		break;
```

设置 binder_proc 结构体的 max_threads 为 15，结构体为如下：

```
struct binder_proc {
	struct hlist_node proc_node;

// &#x4F7F;&#x7528;&#x7EA2;&#x9ED1;&#x6811;&#x4FDD;&#x5B58; threads &#x7EBF;&#x7A0B;
	struct rb_root threads;

// &#x8FDB;&#x7A0B;&#x7684;pid &#x53F7;
	int pid;

// &#x8FDB;&#x7A0B;&#x9700;&#x8981;&#x505A;&#x7684;&#x4E8B;&#x9879;
	struct list_head todo;

// &#x4FDD;&#x5B58;&#x6700;&#x5927;&#x7684;&#x7EBF;&#x7A0B;&#x6570;&#x91CF;
	int max_threads;

// &#x8BF7;&#x6C42;&#x7684;&#x7EBF;&#x7A0B;&#x6570;&#x91CF;
	int requested_threads;
	int requested_threads_started;
	int ready_threads;

};
```

## 2. binder 主线程的创建

进程调用下列 startThreadPool 方法，去启动binder 主线程

ProcessState::self()->startThreadPool();

```
void ProcessState::startThreadPool()
{
    AutoMutex _l(mLock);

// mThreadPoolStarted &#x521D;&#x59CB;&#x503C;&#x4E3A; false
    if (!mThreadPoolStarted) {

// &#x8BBE;&#x7F6E;&#x4E3A;true&#xFF0C;&#x8D70;&#x5230; spawnPooledThread(true)
        mThreadPoolStarted = true;
        spawnPooledThread(true);
    }
}
```

走到 spawnPooledThread(true)

```
void ProcessState::spawnPooledThread(bool isMain)
{

// &#x5982;&#x679C;&#x6CA1;&#x6709;&#x6267;&#x884C;&#xFF1A;startThreadPool&#xFF0C;&#x5219; mThreadPoolStarted &#x4E3A;false&#xFF0C;&#x4E0D;&#x8D70;&#x4E0B;&#x5217;&#x7684;&#x4EE3;&#x7801;
// &#x6B64;&#x65F6;&#x662F;&#x4E3A; true &#x7684;
    if (mThreadPoolStarted) {

// &#x8BBE;&#x7F6E;binder thread &#x7684;&#x540D;&#x5B57;
        String8 name = makeBinderThreadName();
        ALOGV("Spawning new pooled thread, name=%s\n", name.string());

// &#x521B;&#x5EFA;&#x4E00;&#x4E2A;&#x7EBF;&#x7A0B;PoolThread&#xFF0C;isMain &#x4E3A;true &#x8868;&#x793A;&#x662F;&#x4E3B;&#x7EBF;&#x7A0B;
        sp<thread> t = new PoolThread(isMain);
// run &#x8FD9;&#x4E2A;&#x7EBF;&#x7A0B;
        t->run(name.string());
    }
}

==========
String8 ProcessState::makeBinderThreadName() {
    int32_t s = android_atomic_add(1, &mThreadPoolSeq);
    pid_t pid = getpid();
    String8 name;

// &#x4E3B;&#x7EBF;&#x7A0B;&#x7684;binder &#x540D;&#x5B57;&#x4E3A;&#xFF1A;Binder:pid&#x53F7;_1&#xFF0C;&#x5982;&#xFF1A;Binder:9320_1
    name.appendFormat("Binder:%d_%X", pid, s);
    return name;
}</thread>
```

创建一个线程PoolThread，isMain 为true 表示是主线程

PoolThread 继承了 Thread：

```
28  #include <utils androidthreads.h>
29
30  #ifdef __cplusplus
31  #include <utils condition.h>
32  #include <utils errors.h>
33  #include <utils mutex.h>
34  #include <utils rwlock.h>
35  #include <utils thread.h>
36  #endif
37
38  #endif // _LIBS_UTILS_THREADS_H</utils></utils></utils></utils></utils></utils>
```

```
status_t Thread::run(const char* name, int32_t priority, size_t stack)
{
    LOG_ALWAYS_FATAL_IF(name == nullptr, "thread name not provided to Thread::run");

    Mutex::Autolock _l(mLock);

    if (mRunning) {
        // thread already started
        return INVALID_OPERATION;
    }

    // reset status and exitPending to their default value, so we can
    // try again after an error happened (either below, or in readyToRun())
    mStatus = OK;
    mExitPending = false;
    mThread = thread_id_t(-1);

    // hold a strong reference on ourself
    mHoldSelf = this;

    mRunning = true;

    bool res;
    if (mCanCallJava) {
        res = createThreadEtc(_threadLoop,
                this, name, priority, stack, &mThread);
    } else {
        res = androidCreateRawThreadEtc(_threadLoop,
                this, name, priority, stack, &mThread);
    }

======
int Thread::_threadLoop(void* user)
{
    Thread* const self = static_cast<thread*>(user);

    sp<thread> strong(self->mHoldSelf);
    wp<thread> weak(strong);
    self->mHoldSelf.clear();

#if defined(__ANDROID__)
    // this is very useful for debugging with gdb
    self->mTid = gettid();
#endif

    bool first = true;

    do {
        bool result;
        if (first) {
            first = false;
            self->mStatus = self->readyToRun();
            result = (self->mStatus == OK);

            if (result && !self->exitPending()) {
                // Binder threads (and maybe others) rely on threadLoop
                // running at least once after a successful ::readyToRun()
                // (unless, of course, the thread has already been asked to exit
                // at that point).

                // This is because threads are essentially used like this:
                //   (new ThreadSubclass())->run();
                // The caller therefore does not retain a strong reference to
                // the thread and the thread would simply disappear after the
                // successful ::readyToRun() call instead of entering the
                // threadLoop at least once.

                result = self->threadLoop();
            }
        } else {
            result = self->threadLoop();
        }
</thread></thread></thread*>
```

执行run 方法，循环回调 threadLoop 方法

```
class PoolThread : public Thread
{
public:
    explicit PoolThread(bool isMain)
        : mIsMain(isMain)
    {
    }

protected:
    virtual bool threadLoop()
    {

// &#x4E00;&#x4E2A;&#x7EBF;&#x7A0B;&#x53EA;&#x6709;&#x4E00;&#x4E2A; IPCThreadState &#x5B9E;&#x4F8B;&#xFF0C;&#x8C03;&#x7528; joinThreadPool &#x65B9;&#x6CD5;
        IPCThreadState::self()->joinThreadPool(mIsMain);
        return false;
    }

    const bool mIsMain;
};
```

调用 joinThreadPool 方法

```
void IPCThreadState::joinThreadPool(bool isMain)
{
    LOG_THREADPOOL("**** THREAD %p (PID %d) IS JOINING THE THREAD POOL\n", (void*)pthread_self(), getpid());

// &#x5982;&#x679C; isMain &#x4E3A;true &#xFF0C;&#x5219;&#x4E3A; BC_ENTER_LOOPER
// false &#x4E3A;&#xFF1A;BC_REGISTER_LOOPER
    mOut.writeInt32(isMain ? BC_ENTER_LOOPER : BC_REGISTER_LOOPER);

    status_t result;
    do {
        processPendingDerefs();
        // now get the next command to be processed, waiting if necessary

// talkwithdriver &#x53BB;&#x9A71;&#x52A8;&#x8BBE;&#x5907;&#x4EA4;&#x4E92;&#xFF0C;&#x6267;&#x884C;&#x547D;&#x4EE4;
        result = getAndExecuteCommand();

        if (result < NO_ERROR && result != TIMED_OUT && result != -ECONNREFUSED && result != -EBADF) {
            ALOGE("getAndExecuteCommand(fd=%d) returned unexpected error %d, aborting",
                  mProcess->mDriverFD, result);
            abort();
        }

        if(result == TIMED_OUT && !isMain) {
            break;
        }
    } while (result != -ECONNREFUSED && result != -EBADF);
```

binder 驱动对：BC_ENTER_LOOPER 的处理

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
		if (get_user(cmd, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
		trace_binder_command(cmd);

		switch (cmd) {

&#x3002;&#x3002;&#x3002;&#x3002;&#x3002;&#x3002;&#x3002;&#x3002;
		case BC_ENTER_LOOPER:

// &#x5982;&#x679C; thread->looper &#x6709;&#x8BBE;&#x7F6E; binder &#x666E;&#x901A;&#x7EBF;&#x7A0B;&#x7684;&#x503C;&#xFF1A;BINDER_LOOPER_STATE_REGISTERED&#xFF0C;&#x5219;&#x56DE;&#x590D;error
			if (thread->looper & BINDER_LOOPER_STATE_REGISTERED) {
				thread->looper |= BINDER_LOOPER_STATE_INVALID;
				binder_user_error("%d:%d ERROR: BC_ENTER_LOOPER called after BC_REGISTER_LOOPER\n",
					proc->pid, thread->pid);
			}

// &#x8BBE;&#x7F6E; thread->looper &#x4E3A;&#xFF1A;BINDER_LOOPER_STATE_ENTERED&#xFF1A;0x02, &#x4F4D;&#x8FD0;&#x7B97;&#x6765;&#x4FDD;&#x5B58;
			thread->looper |= BINDER_LOOPER_STATE_ENTERED;
			break;

// &#x5982;&#x679C;&#x662F;&#x9000;&#x51FA;&#x7EBF;&#x7A0B;&#x7684;&#x8BDD;&#xFF0C;&#x5219;&#x8BBE;&#x7F6E;&#x4E3A;&#xFF1A;BINDER_LOOPER_STATE_EXITED 0x04
		case BC_EXIT_LOOPER:
			binder_debug(BINDER_DEBUG_THREADS,
				     "%d:%d BC_EXIT_LOOPER\n",
				     proc->pid, thread->pid);
			thread->looper |= BINDER_LOOPER_STATE_EXITED;
			break;
```

## 3. binder 普通线程的创建

线程池是在service端，用于响应处理client端的众多请求。binder线程池中的线程都是由Binder驱动来控制创建的。

> 创建binder 普通线程是由binder 驱动控制的， **驱动通过 BR_SPAWN_LOOPER 命令告知进程需要创建一个新的线程**，然后 **进程通过 BC_REGISTER_LOOPER 命令告知驱动其子线程**（非主线程）已经ready

**service 端创建线程的2种情况：**

>

* BC_TRANSACTION：client进程向binderDriver发送IPC调用请求的时候。
* BC_REPLY：client进程收到了binderDriver的IPC调用请求，逻辑执行结束后发送返回值。

首先客户端调用 IPCThreadState::transact：

* **客户端进程*

```cpp
status_t IPCThreadState::transact(int32_t handle,
                                  uint32_t code, const Parcel& data,
                                  Parcel* reply, uint32_t flags)
{
    status_t err;

    flags |= TF_ACCEPT_FDS;

// 封装data 值
    err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, nullptr);

        if (reply) {

// 与binder 驱动交互 waitForResponse
            err = waitForResponse(reply);
        } else {
            Parcel fakeReply;
            err = waitForResponse(&fakeReply);
        }
```

// 与binder 驱动交互 waitForResponse

```cpp
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    uint32_t cmd;
    int32_t err;

    while (1) {
        if ((err=talkWithDriver()) < NO_ERROR) break;
        err = mIn.errorCheck();
        if (err < NO_ERROR) break;
        if (mIn.dataAvail() == 0) continue;

        cmd = (uint32_t)mIn.readInt32();

========================
status_t IPCThreadState::talkWithDriver(bool doReceive)
{
    if (mProcess->mDriverFD = mIn.dataSize();

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

    if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;

    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    status_t err;
    do {

#if defined(__ANDROID__)

// 与binder 驱动交互
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
```

与binder 驱动交互：BC_TRANSACTION

```cpp
static int binder_thread_write(struct binder_proc *proc,
			struct binder_thread *thread,
			binder_uintptr_t binder_buffer, size_t size,
			binder_size_t *consumed)
{
	uint32_t cmd;

。。。。。

// 只有cmd 命令是 BC_TRANSACTION 和 BC_REPLY 才会调用 binder_transaction 函数
		case BC_TRANSACTION:
		case BC_REPLY: {
			struct binder_transaction_data tr;

			if (copy_from_user(&tr, ptr, sizeof(tr)))
				return -EFAULT;
			ptr += sizeof(tr);
			binder_transaction(proc, thread, &tr, cmd == BC_REPLY);
			break;
		}
```

只有cmd 命令是 BC_TRANSACTION 和 BC_REPLY 才会调用 binder_transaction 函数

```cpp
static void binder_transaction(struct binder_proc *proc,
			       struct binder_thread *thread,
			       struct binder_transaction_data *tr, int reply)
{
	struct binder_transaction *t;
	struct binder_work *tcomplete;
	binder_size_t *offp, *off_end;
	binder_size_t off_min;

。。。。。。
	}

// 设置工作类型为 BINDER_WORK_TRANSACTION，增加到工作的双向链表中
	t->work.type = BINDER_WORK_TRANSACTION;
	list_add_tail(&t->work.entry, target_list);
	tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
	list_add_tail(&tcomplete->entry, &thread->todo);

// 唤醒对应的进程处理
	if (target_wait)
		wake_up_interruptible(target_wait);
	return;
```

* **service 服务端处理消息*

```cpp
static int binder_thread_read(struct binder_proc *proc,
			      struct binder_thread *thread,
			      binder_uintptr_t binder_buffer, size_t size,
			      binder_size_t *consumed, int non_block)
{
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

// 如果返回有 error，则有可能 执行到 done
	if (thread->return_error != BR_OK && ptr < end) {
		if (thread->return_error2 != BR_OK) {
			if (put_user(thread->return_error2, (uint32_t __user *)ptr))
				return -EFAULT;
			ptr += sizeof(uint32_t);
			binder_stat_br(proc, thread, thread->return_error2);
			if (ptr == end)
				goto done;
			thread->return_error2 = BR_OK;
		}
		if (put_user(thread->return_error, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
		binder_stat_br(proc, thread, thread->return_error);
		thread->return_error = BR_OK;
		goto done;
	}

。。。。。。
// 执行while 循环
	while (1) {
		uint32_t cmd;
		struct binder_transaction_data tr;
		struct binder_work *w;

// 初始值 t 为 null
		struct binder_transaction *t = NULL;

		if (!list_empty(&thread->todo)) {
			w = list_first_entry(&thread->todo, struct binder_work,
					     entry);
		} else if (!list_empty(&proc->todo) && wait_for_proc_work) {
			w = list_first_entry(&proc->todo, struct binder_work,
					     entry);
		} else {
			/* no data added */
			if (ptr - buffer == 4 &&
			    !(thread->looper & BINDER_LOOPER_STATE_NEED_RETURN))
				goto retry;
			break;
		}

		if (end - ptr < sizeof(tr) + 4)
			break;

		switch (w->type) {

// 执行 BINDER_WORK_TRANSACTION，t 不为空，不走cotinue，回走到 done 分支
		case BINDER_WORK_TRANSACTION: {
			t = container_of(w, struct binder_transaction, work);
		} break;
。。。。。
// 如果命令是 BR_DEAD_BINDER，也会走到 done
			if (cmd == BR_DEAD_BINDER)
				goto done; /* DEAD_BINDER notifications can cause transactions */
		} break;
		}

// 如果 t 为null，则执行 continue，不退出循环
		if (!t)
			continue;
。。。
		} else {
			t->buffer->transaction = NULL;
			kfree(t);
			binder_stats_deleted(BINDER_STAT_TRANSACTION);
		}
		break;

// 下列括号是 while 的括号
	}

done:

	*consumed = ptr - buffer;

// 需要满足 3 个条件：
// 1. 当前进程没有可请求的线程，也没有已经ready可用的线程
// 2. 启动的线程要小于 15；
// 3. 对应的client中的线程不能已经启动过。
	if (proc->requested_threads + proc->ready_threads == 0 &&
	    proc->requested_threads_started < proc->max_threads &&
	    (thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
	     BINDER_LOOPER_STATE_ENTERED)) /* the user-space code fails to */
	     /*spawn a new thread if we leave this out */) {

// 设置请求线程的数量 + 1
		proc->requested_threads++;

// 拷贝 BR_SPAWN_LOOPER 到用户空间
		if (put_user(BR_SPAWN_LOOPER, (uint32_t __user *)buffer))
			return -EFAULT;
		binder_stat_br(proc, thread, BR_SPAWN_LOOPER);
	}
	return 0;
}
```

当发生以下3种情况之一，便会进入done分支：

>

1. 当前线程的return_error发生error的情况；
2. 当Binder驱动向client端发送死亡通知的情况；
3. 当类型为BINDER_WORK_TRANSACTION(即收到命令是BC_TRANSACTION或BC_REPLY)的情况；

线程的含义：

>

1. ready_threads： 表示当前线程池中有多少可用的空闲线程。
2. requested_threads：请求开启线程的数量。
3. requested_threads_started：表示当前已经接受请求开启的线程数量。

创建 Binder 普通线程的条件有3个：

> 1. 当前进程没有可请求的线程，也没有已经ready可用的线程
2. 启动的线程要小于 15；
3. 对应的client中的线程不能已经启动过

拷贝 BR_SPAWN_LOOPER 到用户空间，执行用户空间的代码创建普通线程：

```
status_t IPCThreadState::getAndExecuteCommand()
{
    status_t result;
    int32_t cmd;

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
        if (mProcess->mExecutingThreadsCount >= mProcess->mMaxThreads &&
                mProcess->mStarvationStartTimeMs == 0) {
            mProcess->mStarvationStartTimeMs = uptimeMillis();
        }
        pthread_mutex_unlock(&mProcess->mThreadCountLock);

        result = executeCommand(cmd);
```

executeCommand

```cpp
status_t IPCThreadState::executeCommand(int32_t cmd)
{
    BBinder* obj;
    RefBase::weakref_type* refs;
    status_t result = NO_ERROR;

    switch ((uint32_t)cmd) {

    case BR_SPAWN_LOOPER:
        mProcess->spawnPooledThread(false);
        break;
```

执行 mProcess->spawnPooledThread(false)

```cpp
void ProcessState::spawnPooledThread(bool isMain)
{

// isMain 为false
    if (mThreadPoolStarted) {

// 设置binder 的名字：
        String8 name = makeBinderThreadName();
        ALOGV("Spawning new pooled thread, name=%s\n", name.string());

// 创建 PoolThread对象，指向run 方法
        sp t = new PoolThread(isMain);
        t->run(name.string());
    }
}

=======
String8 ProcessState::makeBinderThreadName() {

// 递增加1
    int32_t s = android_atomic_add(1, &mThreadPoolSeq);
    pid_t pid = getpid();
    String8 name;

// 这里为：Binder:9032_2
    name.appendFormat("Binder:%d_%X", pid, s);
    return name;
}
```

创建 PoolThread对象，指向run 方法

```cpp
class PoolThread : public Thread
{
public:
    explicit PoolThread(bool isMain)
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

又回到：IPCThreadState::self()->joinThreadPool(false)，此 IPCThreadState 对象是个新的对象，与主线程的 IPCThreadState 是不同的。

```cpp
void IPCThreadState::joinThreadPool(bool isMain)
{

// isMain  为false，与binder 驱动交互的命令是 BC_REGISTER_LOOPER
    mOut.writeInt32(isMain ? BC_ENTER_LOOPER : BC_REGISTER_LOOPER);

    status_t result;
    do {

        result = getAndExecuteCommand();

        if (result < NO_ERROR && result != TIMED_OUT && result != -ECONNREFUSED && result != -EBADF) {
            ALOGE("getAndExecuteCommand(fd=%d) returned unexpected error %d, aborting",
                  mProcess->mDriverFD, result);
            abort();
        }

        // Let this thread exit the thread pool if it is no longer
        // needed and it is not the main process thread.

// 如果普通线程返回的结果是 TIMED_OUT，则回收该普通线程
        if(result == TIMED_OUT && !isMain) {
            break;
        }
    } while (result != -ECONNREFUSED && result != -EBADF);

    LOG_THREADPOOL("**** THREAD %p (PID %d) IS LEAVING THE THREAD POOL err=%d\n",
        (void*)pthread_self(), getpid(), result);

// 通知binder 驱动线程退出了：BC_EXIT_LOOPER
    mOut.writeInt32(BC_EXIT_LOOPER);
    talkWithDriver(false);
}
```

isMain 为false，与binder 驱动交互的命令是 BC_REGISTER_LOOPER

```cpp
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
		if (get_user(cmd, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);

		switch (cmd) {

。。。。。。。
		case BC_REGISTER_LOOPER:

// 如果是主线程，则报错。
			if (thread->looper & BINDER_LOOPER_STATE_ENTERED) {
				thread->looper |= BINDER_LOOPER_STATE_INVALID;
				binder_user_error("%d:%d ERROR: BC_REGISTER_LOOPER called after BC_ENTER_LOOPER\n",
					proc->pid, thread->pid);

// 如果binder 驱动都没有请求创建线程，则报错
			} else if (proc->requested_threads == 0) {
				thread->looper |= BINDER_LOOPER_STATE_INVALID;
				binder_user_error("%d:%d ERROR: BC_REGISTER_LOOPER called without request\n",
					proc->pid, thread->pid);
			} else {

// requested_threads减1， requested_threads_started开启的线程增加为 1；
				proc->requested_threads--;
				proc->requested_threads_started++;
			}

// 设置looper 模式
			thread->looper |= BINDER_LOOPER_STATE_REGISTERED;
			break;
```
