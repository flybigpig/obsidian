### ProcessState

  ProcessState表示进程状态，一个进程就一个ProcessState对象。它的职责：1、打开Binder驱动；2、建立线程池。

```c
 //frameworks/native/include/binder/ProcessState.h
 class ProcessState: public virtual RefBase {
     public: static sp < ProcessState > self();
     void startThreadPool();
     void spawnPooledThread(bool isMain);
     private: int mDriverFD;
     void * mVMStart;
     // Maximum number for binder threads allowed for this process.
     size_t mMaxThreads;
     bool mThreadPoolStarted;
     volatile int32_t mThreadPoolSeq;
 }
```

  以上是ProcessState中Binder相关的接口和成员变量。

#### 1）获取ProcessState实例，并打开Binder驱动

```c
//单例模式
sp<ProcessState> ProcessState::self()
{
    Mutex::Autolock _l(gProcessMutex);
    if (gProcess != NULL) {
        return gProcess;
    }
    gProcess = new ProcessState;
    return gProcess;
}

//构造函数
ProcessState::ProcessState()
    : mDriverFD(open_driver())//在创建实例时就打开了Binder驱动
    , mVMStart(MAP_FAILED)
   ……
    , mMaxThreads(DEFAULT_MAX_BINDER_THREADS)
   ……
    , mThreadPoolStarted(false)
    , mThreadPoolSeq(1)
{
    if (mDriverFD >= 0) {
        // mmap the binder, providing a chunk of virtual address space to receive transactions.
        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
        if (mVMStart == MAP_FAILED) {
            // *sigh*
            ALOGE("Using /dev/binder failed: unable to mmap transaction memory.\n");
            close(mDriverFD);
            mDriverFD = -1;
        }
    }

    LOG_ALWAYS_FATAL_IF(mDriverFD < 0, "Binder driver could not be opened.  Terminating.");
}
```

  这个构造函数里面调用open\_driver()打开了/dev/binder设备驱动文件，返回了Binder驱动的文件描述符。在Linux里一切皆文件，而文件描述符就相当于这个文件的句柄，可以对这个文件进行读写操作。有了Binder驱动的文件描述符就可以对Binder驱动进行读写。

#### 2）创建线程池`ProcessState::self()->startThreadPool();`

```c
void ProcessState::startThreadPool()
{
    AutoMutex _l(mLock);
    if (!mThreadPoolStarted) {//从这里可以看出线程池只能被创建一次
        mThreadPoolStarted = true;
        spawnPooledThread(true);//创建线程池时就会马上创建一个主线程
    }
}

void ProcessState::spawnPooledThread(bool isMain)
{
    if (mThreadPoolStarted) {
        String8 name = makeBinderThreadName();
        ALOGV("Spawning new pooled thread, name=%s\n", name.string());
        sp<Thread> t = new PoolThread(isMain);
        t->run(name.string());
    }
}
//PoolThread是一个继承于Thread的类，创建了PoolThread其实就是创建了一个线程。
//调用t->run()之后相当于调用PoolThread类的threadLoop()函数。
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
        // 这里线程函数调用了一次IPCThreadState::self()->joinThreadPool()后就退出了
        return false;
    }

    const bool mIsMain;
};
```

### IPCThreadState

  IPCThreadState表示进程间通信的线程状态，一个负责进程间通信的线程有且仅有一个IPCThreadState对象。它提供了与Binder驱动交互的函数：sendReply、waitForResponse、joinThreadPool、transact、talkWithDriver、writeTransactionData、getAndExecuteCommand。

#### 1）创建IPCThreadState对象，并赋值给当前线程

```c
static pthread_mutex_t gTLSMutex = PTHREAD_MUTEX_INITIALIZER;
static bool gHaveTLS = false;//记录线程私有数据对应的key创建与否
static pthread_key_t gTLS = 0;

IPCThreadState* IPCThreadState::self()
{
    if (gHaveTLS) {//判断线程私有数据的key创建与否，
    //若没有创建就先创建，然后再走下面的方法体
restart:
        const pthread_key_t k = gTLS;
        //取出key对应的当前线程的私有数据，
        //这里将该key对应的线程私有数据设置成了IPCThreadState对象
        IPCThreadState* st = (IPCThreadState*)pthread_getspecific(k);
        if (st) return st;
        return new IPCThreadState;//若当前线程的key对应的IPCThreadState对象不存在，则创建IPCThreadState对象
    }

    if (gShutdown) {
        ALOGW("Calling IPCThreadState::self() during shutdown is dangerous, expect a crash.\n");
        return NULL;
    }

    pthread_mutex_lock(&gTLSMutex);
    if (!gHaveTLS) {
    //创建线程私有数据的key，并赋值给gTLS
        int key_create_value = pthread_key_create(&gTLS, threadDestructor);
        if (key_create_value != 0) {
            pthread_mutex_unlock(&gTLSMutex);
            ALOGW("IPCThreadState::self() unable to create TLS key, expect a crash: %s\n",
                    strerror(key_create_value));
            return NULL;
        }
        gHaveTLS = true;
    }
    pthread_mutex_unlock(&gTLSMutex);
    goto restart;
}
```

  线程私有数据的有关内容可以查阅以下文章：

[pthread\_key\_create()–创建线程私有数据|pthread\_key\_delete()–注销线程私有数据](https://blog.csdn.net/rain_qingtian/article/details/11486651)

```c
IPCThreadState::IPCThreadState()
    : mProcess(ProcessState::self()),//获取当前进程的ProcessState对象并赋值给成员变量mProcess
      mMyThreadId(gettid()),
      mStrictModePolicy(0),
      mLastTransactionBinderFlags(0)
{
    pthread_setspecific(gTLS, this);//将当前线程的key（即gTLS）对应的私有数据设置成自己
    clearCaller();
    mIn.setDataCapacity(256);//mIn:输入缓冲区
    mOut.setDataCapacity(256);//mOut:输出缓冲区
}
```

#### 2）IPCThreadState既作为客户端，又作为服务端

作为客户端：

```c
transact() -- writeTransactionData() -- waitForResponse() -- talkWithDriver() -- reply->ipcSetDataReference()

status_t IPCThreadState::transact(int32_t handle, uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
……
// 发送ADD_SERVICE_TRANSACTION请求
writeTransactionData(BC_TRANSACTION, flags, handle, code, data, NULL);
if(reply)// 等待响应
waitForResponse(NULL, reply);
……
}

/**
* 将data的数据写入输出缓冲区
*/
status_t IPCThreadState::writeTransactionData(int32_t cmd, uint32_t binderFlags,
    int32_t handle, uint32_t code, const Parcel& data, status_t* statusBuffer)
{
    binder_transaction_data tr;//创建一个结构体binder_transaction_data
    tr.target.ptr = 0; /* Don't pass uninitialized stack data to a remote process */
    tr.target.handle = handle;
    tr.code = code;
    tr.flags = binderFlags;
……
    const status_t err = data.errorCheck();
    if (err == NO_ERROR) {
    //将Parcel类的data中的数据写入结构体tr中
        tr.data_size = data.ipcDataSize();
        tr.data.ptr.buffer = data.ipcData();
        tr.offsets_size = data.ipcObjectsCount()*sizeof(binder_size_t);
        tr.data.ptr.offsets = data.ipcObjects();
    } 
……
    mOut.writeInt32(cmd);
    mOut.write(&tr, sizeof(tr));//最后将结构体tr写入输出缓冲区
    return NO_ERROR;
}

status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    uint32_t cmd;
    int32_t err;

    while (1) {
        talkWithDriver();//与binder驱动交互
……
        cmd = (uint32_t)mIn.readInt32();
        ……
        switch (cmd) {
        case BR_TRANSACTION_COMPLETE:
            ……
            break;

        case BR_REPLY:
            {
                binder_transaction_data tr;//创建一个结构体binder_transaction_data
                err = mIn.read(&tr, sizeof(tr));//从输入缓冲区中将数据写入结构体tr中
                ……
                if (reply) {
                    if ((tr.flags & TF_STATUS_CODE) == 0) {
                    //最后将结构体tr中的数据写入reply
                        reply->ipcSetDataReference(
                            reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                            tr.data_size,
                            reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                            tr.offsets_size/sizeof(binder_size_t),
                            freeBuffer, this);
                    } 
                }
                ……
            }
            goto finish;

        ……
        }
    }

finish:
    if (err != NO_ERROR) {
        ……
        if (reply) reply->setError(err);
        ……
    }

    return err;
}

status_t IPCThreadState::talkWithDriver(bool doReceive)
{
binder_write_read bwr;

bwr.write_size = outAvail;
bwr.write_buf = (long unsigned int)mOut.data();// 取出输出缓冲区
bwr.read_size = mIn.dataCapacity;
bwr.read_buffer = (long unsigned int)mIn.data();//取出输入缓冲区
ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr);// 把mOut写到Binder，并将返回数据读入mIn
……
if (bwr.read_consumed > 0) {
           mIn.setDataSize(bwr.read_consumed);
           mIn.setDataPosition(0);
    }
    ……
}
```

作为服务端：

```c
joinThreadPool() -- getAndExecuteCommand() -- talkWithDriver() -- executeCommand() -- sendReply() -- writeTransactionData() -- waitForResponse()

void IPCThreadState::joinThreadPool(bool isMain) {
mOut.writeInt32(isMain ? BC_ENTER_LOOPER : BC_REGISTER_LOOPER);
status_t result;
do {
result = getAndExecuteCommand();
} while (result != -ECONNREFUSED && result != -EBADF);
mOut.writeInt32(BC_EXIT_LOOPER);
}

status_t IPCThreadState::getAndExecuteCommand() {
status_t result;
    int32_t cmd;

    result = talkWithDriver();//将输出缓冲区mOut的数据写入Binder驱动，并从Binder驱动把返回数据读入输入缓冲区mIn
    cmd = mIn.readInt32();
    result = executeCommand(cmd);
    return result;
}

status_t IPCThreadState::executeCommand(int32_t cmd) {
status_t result = NO_ERROR;
switch ((uint32_t)cmd) {
case BR_TRANSACTION:
binder_transaction_data tr;//创建结构体binder_transaction_data
            result = mIn.read(&tr, sizeof(tr));//将输入缓冲区中的数据读入结构体tr中
            Parcel buffer;
            //将结构体tr的数据写入buffer
            buffer.ipcSetDataReference(
                reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                tr.data_size,
                reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                tr.offsets_size/sizeof(binder_size_t), freeBuffer, this);

Parcel reply;
//调用BBinder的transact()，将参数buffer传入，将返回值写入reply
reinterpret_cast<BBinder*>(tr.cookie)->transact(tr.code, buffer,
                           &reply, tr.flags);
             ……
             sendReply(reply, 0);
             ……
}
}

status_t IPCThreadState::sendReply(const Parcel& reply, uint32_t flags)
{
    status_t err;
    status_t statusBuffer;
    //将返回数据reply写入输出缓冲区mOut
    err = writeTransactionData(BC_REPLY, flags, -1, 0, reply, &statusBuffer);
    if (err < NO_ERROR) return err;

    return waitForResponse(NULL, NULL);
}

status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    uint32_t cmd;
    int32_t err;

    while (1) {
        talkWithDriver();
……
        cmd = (uint32_t)mIn.readInt32();
        ……
        switch (cmd) {
        case BR_TRANSACTION_COMPLETE:
            if (!reply && !acquireResult) goto finish;
            break;
        }
    }
    
finish:
    if (err != NO_ERROR) {
        ……
        if (reply) reply->setError(err);
        ……
    }

    return err;
}
```

小结：IPCThreadState与Binder驱动交互可以简化为如下的读写过程：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/e10716a0861036ad726e6a49afb5ebb9.png)

#### 3）BBinder与BpBinder

  当IPCThreadState作为服务端时出现了BBinder，它是Binder的本地对象，相对应地存在一个BpBinder，它表示Binder的代理对象。

```c
status_t IPCThreadState::executeCommand(int32_t cmd) {
    switch ((uint32_t) cmd) {
        case BR_TRANSACTION:
            ……
            Parcel reply;
            //调用BBinder的transact()，将参数buffer传入，将返回值写入reply
            reinterpret_cast < BBinder * > (tr.cookie) - > transact(tr.code, buffer, &
                reply, tr.flags);……
    }
}

status_t BpBinder::transact(uint32_t code,
    const Parcel & data, Parcel * reply, uint32_t flags) {
    IPCThreadState::self() - > transact(mHandle, code, data, reply, flags);
}
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/765cd789d977ff5e49aaa491e77120ba.png)

#### 4）示例MediaPlayerService

  可以通过具体示例MediaPlayerService来分析我们应用程序中怎么通过Binder通信的。关于MediaPlayerService的分析可以参考下面文章：[Android开发之MediaPlayerService详解](https://blog.csdn.net/llping2011/article/details/9715971)。  
  下面是MediaPlayerService中Binder通信相关的类图。  
![MediaPlayerService中Binder通信相关的类图](https://i-blog.csdnimg.cn/blog_migrate/db99796f3d043720bc97d139f8f872fa.png)

#### 5）binder\_proc和binder\_thread

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/90a081e1976ea7b2713c2451c87a5e49.png)  
  在用户空间中一个进程一个ProcessState，一个用于进程间通信的线程一个IPCThreadState。而在内核空间中也提供了对应的数据结构：binder\_proc和binder\_thread。binder\_proc与ProcessState一一对应，binder\_thread与IPCThreadState一一对应。  
  binder\_thread与IPCThreadState构建了用户空间和内核空间的的通信通道。IPCThreadState将handle和参数传给了binder\_thread。如下代码：

```c
status_t BpBinder::transact(uint32_t code,
    const Parcel & data, Parcel * reply, uint32_t flags) {
    IPCThreadState::self() - > transact(mHandle, code, data, reply, flags);
}
```

  Binder驱动从binder\_thread中取出handle和参数，根据handle找到当前进程对应的binder\_proc中的binder\_ref，再由binder\_ref找到服务进程对应的binder\_proc中binder\_node。接着从binder\_node从取出BBinder的地址，把该地址和参数传给服务进程的binder\_thread，最后交给该binder\_thread对应的IPCThreadState执行如下代码：

```c
status_t IPCThreadState::executeCommand(int32_t cmd) {
    switch ((uint32_t) cmd) {
        case BR_TRANSACTION:
            ……
            Parcel reply;
            //调用BBinder的transact()，将参数buffer传入，将返回值写入reply
            reinterpret_cast < BBinder * > (tr.cookie) - > transact(tr.code, buffer, &
                reply, tr.flags);……
    }
}
```

  最后执行BBinder的onTransact方法，具体怎么执行那就要看BBinder的实际对象类型，这就涉及到面向对象的多态性。