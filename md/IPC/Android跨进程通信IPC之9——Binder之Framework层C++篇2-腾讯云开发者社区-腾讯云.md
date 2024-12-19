### 四、注册服务

##### (一) 源码位置：

```
framework/native/libs/binder/
  - Binder.cpp
  - BpBinder.cpp
  - IPCThreadState.cpp
  - ProcessState.cpp
  - IServiceManager.cpp
  - IInterface.cpp
  - Parcel.cpp

frameworks/native/include/binder/
  - IInterface.h (包括BnInterface, BpInterface)

/frameworks/av/media/mediaserver/
  - main_mediaserver.cpp

/frameworks/av/media/libmediaplayerservice/
  - MediaPlayerService.cpp
```

对应的链接为

-   [Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FBinder.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [BpBinder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FBpBinder.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [IPCThreadState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [ProcessState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [IServiceManager.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIServiceManager.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [IInterface.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIInterface.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [IInterface.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Finclude%2Fbinder%2FIInterface.h&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [main\_mediaserver.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fav%2Fmedia%2Fmediaserver%2Fmain_mediaserver.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [MediaPlayerService.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fav%2Fmedia%2Flibmediaplayerservice%2FMediaPlayerService.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)

##### (二)、概述

由于服务注册会涉及到具体的服务注册，网上大多数说的都是Media注册服务，我们也说它。

> media入口函数是 “main\_mediaserver.cpp”中的main()方法，代码如下：

```
frameworks/av/media/mediaserver/main_mediaserver.cpp      44行
int main(int argc __unused, char** argv)
{
    *** 省略部分代码  *****
    InitializeIcuOrDie();
    // 获得ProcessState实例对象 
    sp<ProcessState> proc(ProcessState::self());
    //获取 BpServiceManager
    sp<IServiceManager> sm = defaultServiceManager();
    AudioFlinger::instantiate();
    //注册多媒体服务
    MediaPlayerService::instantiate();
    ResourceManagerService::instantiate();
    CameraService::instantiate();
    AudioPolicyService::instantiate();
    SoundTriggerHwService::instantiate();
    RadioService::instantiate();
    registerExtensions();
    //启动Binder线程池
    ProcessState::self()->startThreadPool();
    //当前线程加入到线程池
    IPCThreadState::self()->joinThreadPool();
 }
```

所以在main函数里面

-   首先 获得了一个ProcessState的实例
-   其次 调用defualtServiceManager方法获取IServiceManager实例
-   再次 进行重要服务的初始化
-   最后调用startThreadPool方法和joinThreadPool方法。

###### PS: (1)获取ServiceManager：我们上篇文章讲解了defaultServiceManager()返回的是BpServiceManager对象，用于跟servicemanger进行通信。

##### (三)、类图

我们这里主要讲解的是Native层的服务，所以我们以native层的media为例，来说一说服务注册的过程，先来看看media的关系图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/4e9s5spfvs.png)

media类关系图.png

图解

-   蓝色代表的是注册MediaPlayerService
-   绿色代表的是Binder架构中与Binder驱动通信
-   紫色代表的是注册服务和获取服务的公共接口/父类

##### (四)、时序图

先通过一幅图来说说，media服务启动过程是如何向servicemanager注册服务的。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/95dx756ttz.png)

注册.png

##### (五)、流程介绍

###### 1、inistantiate()函数

```
// MediaPlayerService.cpp     269行
void MediaPlayerService::instantiate() {
    defaultServiceManager()->addService(String16("media.player"), new MediaPlayerService());
}
```

-   1 创建一个新的Service——BnMediaPlayerService，想把它告诉ServiceManager。然后调用BnServiceManager的addService的addService来向ServiceManager中添加一个Service，其他进程可以通过字符串"media.player"来向ServiceManager查询此服务。
-   2 注册服务MediaPlayerService：由defaultServiceManager()返回的是BpServiceManager，同时会创建ProcessState对象和BpBinder对象。故此处等价于调用BpServiceManager->addService。

###### 2、BpSserviceManager.addService()函数

```
/frameworks/native/libs/binder/IServiceManager.cpp   155行
virtual status_t addService(const String16& name, const sp<IBinder>& service,
        bool allowIsolated)
{
    //data是送到BnServiceManager的命令包
    Parcel data, reply; 
    //先把interface名字写进去，写入头信息"android.os.IServiceManager"
    data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());   
    // 再把新service的名字写进去 ，name为"media.player"
    data.writeString16(name);  
    // MediaPlayerService对象
    data.writeStrongBinder(service); 
     // allowIsolated= false
    data.writeInt32(allowIsolated ? 1 : 0);
    //remote()指向的BpServiceManager中保存的BpBinder
    status_t err = remote()->transact(ADD_SERVICE_TRANSACTION, data, &reply);
    return err == NO_ERROR ? reply.readExceptionCode() : err;
}
```

服务注册过程：向ServiceManager 注册服务MediaPlayerService，服务名为"media.player"。这样别的进程皆可以通过"media.player"来查询该服务

这里我们重点说下writeStrongBinder()函数和最后的transact()函数

###### 2.1、writeStrongBinder()函数

```
/frameworks/native/libs/binder/Parcel.cpp        872行
status_t Parcel::writeStrongBinder(const sp<IBinder>& val)
{
    return flatten_binder(ProcessState::self(), val, this);
}
```

里面调用flatten\_binder()函数，那我们继续跟踪

###### 2.1.1、 flatten\_binder()函数

```
/frameworks/native/libs/binder/Parcel.cpp        205行
status_t flatten_binder(const sp<ProcessState>& /*proc*/,
    const sp<IBinder>& binder, Parcel* out)
{
    flat_binder_object obj;

    obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
     //本地Binder不为空
    if (binder != NULL) {
        IBinder *local = binder->localBinder(); 
        if (!local) {
            BpBinder *proxy = binder->remoteBinder();
            const int32_t handle = proxy ? proxy->handle() : 0;
            obj.type = BINDER_TYPE_HANDLE; 
            obj.binder = 0; 
            obj.handle = handle;
            obj.cookie = 0;
        } else { 
            // 进入该分支
            obj.type = BINDER_TYPE_BINDER; 
            obj.binder = reinterpret_cast<uintptr_t>(local->getWeakRefs());
            obj.cookie = reinterpret_cast<uintptr_t>(local);
        }
    } else {
        ...
    }
    return finish_flatten_binder(binder, obj, out);
}
```

其实是将Binder对象扁平化，转换成flat\_binder\_object对象

-   对于Binder实体，则用cookie记录binder实体的指针。
-   对于Binder代理，则用handle记录Binder代理的句柄。

关于localBinder，代码如下：

```
//frameworks/native/libs/binder/Binder.cpp      191行
BBinder* BBinder::localBinder()
{
    return this;
}

//frameworks/native/libs/binder/Binder.cpp      47行
BBinder* IBinder::localBinder()
{
    return NULL;
}
```

上面 最后又调用了finish\_flatten\_binder()让我们一起来看下

###### 2.1.1、 finish\_flatten\_binder()函数

```
//frameworks/native/libs/binder/Parcel.cpp        199行
inline static status_t finish_flatten_binder(
    const sp<IBinder>& , const flat_binder_object& flat, Parcel* out)
{
    return out->writeObject(flat, false);
}
```

###### 2.2、 transact()函数

```
//frameworks/native/libs/binder/BpBinder.cpp       159行
status_t BpBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    if (mAlive) {
        // code=ADD_SERVICE_TRANSACTION
        status_t status = IPCThreadState::self()->transact(
            mHandle, code, data, reply, flags);
        if (status == DEAD_OBJECT) mAlive = 0;
        return status;
    }
    return DEAD_OBJECT;
}
```

Binder代理类调用transact()方法，真正的工作还是交给IPCThreadState来进行transact工作，先来，看见IPCThreadState:: self的过程。

Binder代理类调用transact()方法，真正工作还是交给IPCThreadState来进行transact工作。先来 看看IPCThreadState::self的过程。

###### 2.2.1、IPCThreadState::self()函数

```
//frameworks/native/libs/binder/IPCThreadState.cpp     280行
IPCThreadState* IPCThreadState::self()
{
    if (gHaveTLS) {
restart:
        const pthread_key_t k = gTLS;
        IPCThreadState* st = (IPCThreadState*)pthread_getspecific(k);
        if (st) return st;
        // new 了一个  IPCThreadState对象
        return new IPCThreadState; 
    }

    if (gShutdown) return NULL;

    pthread_mutex_lock(&gTLSMutex);
     //首次进入gHaveTLS为false
    if (!gHaveTLS) { 
        // 创建线程的TLS
        if (pthread_key_create(&gTLS, threadDestructor) != 0) { 
            pthread_mutex_unlock(&gTLSMutex);
            return NULL;
        }
        gHaveTLS = true;
    }
    pthread_mutex_unlock(&gTLSMutex);
    goto restart;
}
```

> TLS 是指Thread local storage(线程本地存储空间)，每个线程都拥有自己的TLS，并且是私有空间，线程空间是不会共享的。通过pthread\_getspecific/pthread\_setspecific函数可以设置这些空间中的内容。从线程本地存储空间中获得保存在其中的IPCThreadState对象。

说到 IPCThreadState对象，我们就来看看它的构造函数

###### 2.2.1、IPCThreadState的构造函数

```
//frameworks/native/libs/binder/IPCThreadState.cpp     686行  
IPCThreadState::IPCThreadState()
    : mProcess(ProcessState::self()),
      mMyThreadId(gettid()),
      mStrictModePolicy(0),
      mLastTransactionBinderFlags(0)
{
    pthread_setspecific(gTLS, this);
    clearCaller();
    mIn.setDataCapacity(256);
    mOut.setDataCapacity(256);
}
```

每个线程都有一个IPCThreadState，每个IPCThreadState中都有一个mIn，一个mOut。成员变量mProcess保存了ProccessState变量(每个进程只有一个)

-   mIn：用来接收来自Binder设备的数据，默认大小为256字节
-   mOut：用来存储发往Binder设备的数据，默认大小为256字节

###### 2.2.2、IPCThreadState::transact()函数

```
//frameworks/native/libs/binder/IPCThreadState.cpp     548行
status_t IPCThreadState::transact(int32_t handle,
                                  uint32_t code, const Parcel& data,
                                  Parcel* reply, uint32_t flags)
{
    //数据错误检查
    status_t err = data.errorCheck(); 
    flags |= TF_ACCEPT_FDS;
    //  ***  省略部分代码  ***
    if (err == NO_ERROR) {
        //传输数据
        err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, NULL);
    }
    //  ***  省略部分代码  ***
    if ((flags & TF_ONE_WAY) == 0) {
        if (reply) {
            //等待响应 
            err = waitForResponse(reply);
        } else {
            Parcel fakeReply;
            err = waitForResponse(&fakeReply);
        }
    } else {
        //one waitForReponse(NULL,NULL)
        err = waitForResponse(NULL, NULL);
    }
    return err;
}
```

IPCThreadState进行trancsact事物处理3部分：

-   errorCheck() ：负责 数据错误检查
-   writeTransactionData()： 负责 传输数据
-   waitForResponse()： 负责 等待响应

那我们重点看下writeTransactionData()函数与waitForResponse()函数

###### 2.2.2.1、writeTransactionData)函数

```
//frameworks/native/libs/binder/IPCThreadState.cpp      904行
status_t IPCThreadState::writeTransactionData(int32_t cmd, uint32_t binderFlags, int32_t handle, uint32_t code, const Parcel& data, status_t* statusBuffer) 
{
            binder_transaction_data tr;
            tr.target.ptr = 0; /* Don't pass uninitialized stack data to a remote process */ 
            // handle=0
            tr.target.handle = handle;
             //code=ADD_SERVICE_TRANSACTION
            tr.code = code;
            // binderFlags=0
            tr.flags = binderFlags;
            tr.cookie = 0;
            tr.sender_pid = 0;
            tr.sender_euid = 0;
            // data为记录Media服务信息的Parcel对象
            const status_t err = data.errorCheck();
            if (err == NO_ERROR) {
                    tr.data_size = data.ipcDataSize();
                    tr.data.ptr.buffer = data.ipcData();
                    tr.offsets_size = data.ipcObjectsCount()*sizeof(binder_size_t);
                    tr.data.ptr.offsets = data.ipcObjects();
               } else if (statusBuffer) {
                  tr.flags |= TF_STATUS_CODE;
                    *statusBuffer = err;
                    tr.data_size = sizeof(status_t);
                    tr.data.ptr.buffer = reinterpret_cast<uintptr_t>(statusBuffer);
                   tr.offsets_size = 0;
                    tr.data.ptr.offsets = 0;
              } else {
                   return (mLastError = err);
                }
           // cmd=BC_TRANSACTION
           mOut.writeInt32(cmd);
          // 写入binder_transaction_data数据
           mOut.write(&tr, sizeof(tr));
           return NO_ERROR;
        }
```

其中handle的值用来标示目的端，注册服务过程的目的端为service manager，此处handle=0所对应的是binder\_context\_mgr\_node对象，正是service manager所对应的binder实体对象。其中 binder\_transaction\_data结构体是binder驱动通信的数据结构，该过程最终是把Binder请求码BC\_TRANSACTION和binder\_transaction\_data写入mOut。

transact的过程，先写完binder\_transaction\_data数据，接下来执行waitForResponse。

###### 2.2.2.2、waitForResponse()函数

```
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult) {
        uint32_t cmd;
        int32_t err;

        while (1) {
            if ((err = talkWithDriver()) < NO_ERROR) break;
            err = mIn.errorCheck();
            if (err < NO_ERROR) break;
            if (mIn.dataAvail() == 0) continue;

            cmd = (uint32_t) mIn.readInt32();

            IF_LOG_COMMANDS() {
                alog << "Processing waitForResponse Command: "
                        << getReturnString(cmd) << endl;
            }

            switch (cmd) {
                case BR_TRANSACTION_COMPLETE:
                    if (!reply && !acquireResult) goto finish;
                    break;

                case BR_DEAD_REPLY:
                    err = DEAD_OBJECT;
                                goto finish;

                case BR_FAILED_REPLY:
                    err = FAILED_TRANSACTION;
                                goto finish;

                case BR_ACQUIRE_RESULT: {
                    ALOG_ASSERT(acquireResult != NULL, "Unexpected brACQUIRE_RESULT");
                                    const int32_t result = mIn.readInt32();
                    if (!acquireResult) continue;
                                   *acquireResult = result ? NO_ERROR : INVALID_OPERATION;
                }
                            goto finish;

                case BR_REPLY: {
                    binder_transaction_data tr;
                    err = mIn.read( & tr, sizeof(tr));
                    ALOG_ASSERT(err == NO_ERROR, "Not enough command data for brREPLY");
                    if (err != NO_ERROR) goto finish;

                    if (reply) {
                        if ((tr.flags & TF_STATUS_CODE) == 0) {
                            reply -> ipcSetDataReference(
                                    reinterpret_cast <const uint8_t * > (tr.data.ptr.buffer),
                                    tr.data_size,
                                    reinterpret_cast <const binder_size_t * > (tr.data.ptr.offsets),
                                    tr.offsets_size / sizeof(binder_size_t),
                                    freeBuffer, this);
                        } else {
                            err = *reinterpret_cast<const status_t * > (tr.data.ptr.buffer);
                            freeBuffer(NULL,
                                    reinterpret_cast <const uint8_t * > (tr.data.ptr.buffer),
                                    tr.data_size,
                                    reinterpret_cast <const binder_size_t * > (tr.data.ptr.offsets),
                                    tr.offsets_size / sizeof(binder_size_t), this);
                        }
                    } else {
                        freeBuffer(NULL,
                                reinterpret_cast <const uint8_t * > (tr.data.ptr.buffer),
                                tr.data_size,
                                reinterpret_cast <const binder_size_t * > (tr.data.ptr.offsets),
                                tr.offsets_size / sizeof(binder_size_t), this);
                        continue;
                    }
                }
                            goto finish;

                default:
                    err = executeCommand(cmd);
                    if (err != NO_ERROR) goto finish;
                    break;
            }
        }

        finish:
        if (err != NO_ERROR) {
            if (acquireResult) *acquireResult = err;
            if (reply) reply -> setError(err);
            mLastError = err;
        }
        return err;
    }
```

在waitForResponse过程，首先执行BR\_TRANSACTION\_COMPLETE；另外，目标进程收到事物后，处理BR\_TRANSACTION事物，然后送法给当前进程，再执行BR\_REPLY命令。

这里详细说下talkWithDriver()函数

###### 2.2.2.3、talkWithDriver()函数

```
    status_t IPCThreadState::talkWithDriver(bool doReceive) {
        if (mProcess -> mDriverFD <= 0) {
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
        bwr.write_buffer = (uintptr_t) mOut.data();

        // This is what we'll read.
        if (doReceive && needRead) {
            //接受数据缓冲区信息的填充，如果以后收到数据，就直接填在mIn中了。
            bwr.read_size = mIn.dataCapacity();
            bwr.read_buffer = (uintptr_t) mIn.data();
        } else {
            bwr.read_size = 0;
            bwr.read_buffer = 0;
        }
        IF_LOG_COMMANDS() {
            TextOutput::Bundle _b(alog);
            if (outAvail != 0) {
                alog << "Sending commands to driver: " << indent;
                            const void*cmds = (const void*)bwr.write_buffer;
                            const void*end = ((const uint8_t *)cmds)+bwr.write_size;
                alog << HexDump(cmds, bwr.write_size) << endl;
                while (cmds < end) cmds = printCommand(alog, cmds);
                alog << dedent;
            }
            alog << "Size of receive buffer: " << bwr.read_size
                    << ", needRead: " << needRead << ", doReceive: " << doReceive << endl;
        }

        // Return immediately if there is nothing to do.
        // 当读缓冲和写缓冲都为空，则直接返回
        if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;
        bwr.write_consumed = 0;
        bwr.read_consumed = 0;
        status_t err;
        do {
            IF_LOG_COMMANDS() {
                alog << "About to read/write, write size = " << mOut.dataSize() << endl;
            }
            #if defined(HAVE_ANDROID_OS)
            //通过ioctl不停的读写操作，跟Binder驱动进行通信
            if (ioctl(mProcess -> mDriverFD, BINDER_WRITE_READ, & bwr) >=0)
            err = NO_ERROR;
                    else
            err = -errno;
            #else
            err = INVALID_OPERATION;
            #endif
            if (mProcess -> mDriverFD <= 0) {
                err = -EBADF;
            }
            IF_LOG_COMMANDS() {
                alog << "Finished read/write, write size = " << mOut.dataSize() << endl;
            }
        } while (err == -EINTR);

        IF_LOG_COMMANDS() {
            alog << "Our err: " << (void*)(intptr_t) err << ", write consumed: "
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
                          const void*cmds = mIn.data();
                           const void*end = mIn.data() + mIn.dataSize();
                alog << HexDump(cmds, mIn.dataSize()) << endl;
                while (cmds < end) cmds = printReturnCommand(alog, cmds);
                alog << dedent;
            }
            return NO_ERROR;
        }
        return err;
    }
```

**binder\_write\_read结构体** 用来与Binder设备交换数据的结构，通过ioctl与mDriverFD通信，是真正与Binder驱动进行数据读写交互的过程。主要操作是mOut和mIn变量。

ioctl经过系统调用后进入Binder Driver 大体流程如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/f0n866ee5f.png)

大体流程图.png

##### (六)、Binder驱动

Binder驱动内部调用了流程

> ioctl——> binder\_ioctl ——> binder\_ioctl\_write\_read

###### 1、binder\_ioctl\_write\_read()函数处理

```
static int binder_ioctl_write_read(struct file *filp,
                unsigned int cmd, unsigned long arg,
                struct binder_thread *thread)
{
    struct binder_proc *proc = filp->private_data;
    void __user *ubuf = (void __user *)arg;
    struct binder_write_read bwr;

    //将用户空间bwr结构体拷贝到内核空间
    copy_from_user(&bwr, ubuf, sizeof(bwr));
    //  ***省略部分代码***
    if (bwr.write_size > 0) {
        //将数据放入目标进程
        ret = binder_thread_write(proc, thread,
                      bwr.write_buffer,
                      bwr.write_size,
                      &bwr.write_consumed);
        //  ***省略部分代码***
    }
    if (bwr.read_size > 0) {
        //读取自己队列的数据 
        ret = binder_thread_read(proc, thread, bwr.read_buffer,
             bwr.read_size,
             &bwr.read_consumed,
             filp->f_flags & O_NONBLOCK);
        if (!list_empty(&proc->todo))
            wake_up_interruptible(&proc->wait);
           //  ***省略部分代码***
    }

    //将内核空间bwr结构体拷贝到用户空间
    copy_to_user(ubuf, &bwr, sizeof(bwr));
     //  ***省略部分代码***
}  
```

###### 2、binder\_thread\_write()函数处理

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
        //拷贝用户空间的cmd命令，此时为BC_TRANSACTION
        if (get_user(cmd, (uint32_t __user *)ptr)) -EFAULT;
        ptr += sizeof(uint32_t);
        switch (cmd) {
        case BC_TRANSACTION:
        case BC_REPLY: {
            struct binder_transaction_data tr;
            //拷贝用户空间的binder_transaction_data
            if (copy_from_user(&tr, ptr, sizeof(tr)))   return -EFAULT;
            ptr += sizeof(tr);
            binder_transaction(proc, thread, &tr, cmd == BC_REPLY);
            break;
        }
        //  ***省略部分代码***
    }
    *consumed = ptr - buffer;
  }
  return 0;
}
```

###### 3、binder\_thread\_write()函数处理

```
static void binder_transaction(struct binder_proc *proc,
               struct binder_thread *thread,
               struct binder_transaction_data *tr, int reply){

    if (reply) {
        //  ***省略部分代码***
    }else {
        if (tr->target.handle) {
          //  ***省略部分代码***
        } else {
            // handle=0则找到servicemanager实体
            target_node = binder_context_mgr_node;
        }
        //target_proc为servicemanager进程
        target_proc = target_node->proc;
    }

    if (target_thread) {
         //  ***省略部分代码***
    } else {
        //找到servicemanager进程的todo队列
        target_list = &target_proc->todo;
        target_wait = &target_proc->wait;
    }

    t = kzalloc(sizeof(*t), GFP_KERNEL);
    tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);

    //非oneway的通信方式，把当前thread保存到transaction的from字段
    if (!reply && !(tr->flags & TF_ONE_WAY))
        t->from = thread;
    else
        t->from = NULL;

    t->sender_euid = task_euid(proc->tsk);
    t->to_proc = target_proc; //此次通信目标进程为servicemanager进程
    t->to_thread = target_thread;
    t->code = tr->code;  //此次通信code = ADD_SERVICE_TRANSACTION
    t->flags = tr->flags;  // 此次通信flags = 0
    t->priority = task_nice(current);

    //从servicemanager进程中分配buffer
    t->buffer = binder_alloc_buf(target_proc, tr->data_size,
        tr->offsets_size, !reply && (t->flags & TF_ONE_WAY));

    t->buffer->allow_user_free = 0;
    t->buffer->transaction = t;
    t->buffer->target_node = target_node;

    if (target_node)
        //引用计数+1
        binder_inc_node(target_node, 1, 0, NULL); 
    offp = (binder_size_t *)(t->buffer->data + ALIGN(tr->data_size, sizeof(void *)));

    //分别拷贝用户空间的binder_transaction_data中ptr.buffer和ptr.offsets到内核
    copy_from_user(t->buffer->data,
        (const void __user *)(uintptr_t)tr->data.ptr.buffer, tr->data_size);
    copy_from_user(offp,
        (const void __user *)(uintptr_t)tr->data.ptr.offsets, tr->offsets_size);

    off_end = (void *)offp + tr->offsets_size;

    for (; offp < off_end; offp++) {
        struct flat_binder_object *fp;
        fp = (struct flat_binder_object *)(t->buffer->data + *offp);
        off_min = *offp + sizeof(struct flat_binder_object);
        switch (fp->type) {
            case BINDER_TYPE_BINDER:
            case BINDER_TYPE_WEAK_BINDER: {
              struct binder_ref *ref;
              struct binder_node *node = binder_get_node(proc, fp->binder);
              if (node == NULL) { 
                //服务所在进程 创建binder_node实体
                node = binder_new_node(proc, fp->binder, fp->cookie);
                 //  ***省略部分代码***
              }
              //servicemanager进程binder_ref
              ref = binder_get_ref_for_node(target_proc, node);
              ...
              //调整type为HANDLE类型
              if (fp->type == BINDER_TYPE_BINDER)
                fp->type = BINDER_TYPE_HANDLE;
              else
                fp->type = BINDER_TYPE_WEAK_HANDLE;
              fp->binder = 0;
              fp->handle = ref->desc; //设置handle值
              fp->cookie = 0;
              binder_inc_ref(ref, fp->type == BINDER_TYPE_HANDLE,
                       &thread->todo);
            } break;
            case :  //  ***省略部分代码***
    }

    if (reply) {
          //  ***省略部分代码***
    } else if (!(t->flags & TF_ONE_WAY)) {
        //BC_TRANSACTION 且 非oneway,则设置事务栈信息
        t->need_reply = 1;
        t->from_parent = thread->transaction_stack;
        thread->transaction_stack = t;
    } else {
          //  ***省略部分代码***
    }
    //将BINDER_WORK_TRANSACTION添加到目标队列，本次通信的目标队列为target_proc->todo
    t->work.type = BINDER_WORK_TRANSACTION;
    list_add_tail(&t->work.entry, target_list);
    //将BINDER_WORK_TRANSACTION_COMPLETE添加到当前线程的todo队列
    tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
    list_add_tail(&tcomplete->entry, &thread->todo);
    //唤醒等待队列，本次通信的目标队列为target_proc->wait
    if (target_wait)
        wake_up_interruptible(target_wait);
    return;
}
```

-   注册服务的过程，传递的是BBinder对象，因此上面的writeStrongBinder()过程中localBinder不为空，从而flat\_binder\_object.type等于BINDER\_TYPE\_BINDER。
-   服务注册过程是在服务所在进程创建binder\_node，在servicemanager进程创建binder\_ref。对于同一个binder\_node，每个进程只会创建一个binder\_ref对象。
-   向servicemanager的binder\_proc->todo添加BINDER\_WORK\_TRANSACTION事务，接下来进入ServiceManager进程。

这里说下这个函数里面涉及的三个重要函数

-   binder\_get\_node()
-   binder\_new\_node()
-   binder\_get\_ref\_for\_node()

###### 3.1、binder\_get\_node()函数处理

```
//     /kernel/drivers/android/binder.c     904行
static struct binder_node *binder_get_node(struct binder_proc *proc,
             binder_uintptr_t ptr)
{
  struct rb_node *n = proc->nodes.rb_node;
  struct binder_node *node;

  while (n) {
    node = rb_entry(n, struct binder_node, rb_node);

    if (ptr < node->ptr)
      n = n->rb_left;
    else if (ptr > node->ptr)
      n = n->rb_right;
    else
      return node;
  }
  return NULL;
}
```

从binder\_proc来根据binder指针ptr值，查询相应的binder\_node

###### 3.2、binder\_new\_node()函数处理

```
//kernel/drivers/android/binder.c      923行
static struct binder_node *binder_new_node(struct binder_proc *proc,
                       binder_uintptr_t ptr,
                       binder_uintptr_t cookie)
{
    struct rb_node **p = &proc->nodes.rb_node;
    struct rb_node *parent = NULL;
    struct binder_node *node;
        //第一次进来是空
    while (*p) {
        parent = *p;
        node = rb_entry(parent, struct binder_node, rb_node);

        if (ptr < node->ptr)
            p = &(*p)->rb_left;
        else if (ptr > node->ptr)
            p = &(*p)->rb_right;
        else
            return NULL;
    }
        //给创建的binder_node 分配内存空间
    node = kzalloc(sizeof(*node), GFP_KERNEL);
    if (node == NULL)
        return NULL;
    binder_stats_created(BINDER_STAT_NODE);
        //将创建的node对象添加到proc红黑树
    rb_link_node(&node->rb_node, parent, p);
    rb_insert_color(&node->rb_node, &proc->nodes);
    node->debug_id = ++binder_last_id;
    node->proc = proc;
    node->ptr = ptr;
    node->cookie = cookie;
        //设置binder_work的type
    node->work.type = BINDER_WORK_NODE;
    INIT_LIST_HEAD(&node->work.entry);
    INIT_LIST_HEAD(&node->async_todo);
    binder_debug(BINDER_DEBUG_INTERNAL_REFS,
             "%d:%d node %d u%016llx c%016llx created\n",
             proc->pid, current->pid, node->debug_id,
             (u64)node->ptr, (u64)node->cookie);
    return node;
}
```

###### 3.3、binder\_get\_ref\_for\_node()函数处理

```
//    kernel/drivers/android/binder.c      1066行
static struct binder_ref *binder_get_ref_for_node(struct binder_proc *proc,
              struct binder_node *node)
{
  struct rb_node *n;
  struct rb_node **p = &proc->refs_by_node.rb_node;
  struct rb_node *parent = NULL;
  struct binder_ref *ref, *new_ref;
  //从refs_by_node红黑树，找到binder_ref则直接返回。
  while (*p) {
    parent = *p;
    ref = rb_entry(parent, struct binder_ref, rb_node_node);

    if (node < ref->node)
      p = &(*p)->rb_left;
    else if (node > ref->node)
      p = &(*p)->rb_right;
    else
      return ref;
  }
  
  //创建binder_ref
  new_ref = kzalloc_preempt_disabled(sizeof(*ref));
  
  new_ref->debug_id = ++binder_last_id;
  //记录进程信息
  new_ref->proc = proc; 
  // 记录binder节点
  new_ref->node = node; 
  rb_link_node(&new_ref->rb_node_node, parent, p);
  rb_insert_color(&new_ref->rb_node_node, &proc->refs_by_node);

  //计算binder引用的handle值，该值返回给target_proc进程
  new_ref->desc = (node == binder_context_mgr_node) ? 0 : 1;
  //从红黑树最最左边的handle对比，依次递增，直到红黑树遍历结束或者找到更大的handle则结束。
  for (n = rb_first(&proc->refs_by_desc); n != NULL; n = rb_next(n)) {
    //根据binder_ref的成员变量rb_node_desc的地址指针n，来获取binder_ref的首地址
    ref = rb_entry(n, struct binder_ref, rb_node_desc);
    if (ref->desc > new_ref->desc)
      break;
    new_ref->desc = ref->desc + 1;
  }

  // 将新创建的new_ref 插入proc->refs_by_desc红黑树
  p = &proc->refs_by_desc.rb_node;
  while (*p) {
    parent = *p;
    ref = rb_entry(parent, struct binder_ref, rb_node_desc);

    if (new_ref->desc < ref->desc)
      p = &(*p)->rb_left;
    else if (new_ref->desc > ref->desc)
      p = &(*p)->rb_right;
    else
      BUG();
  }
  rb_link_node(&new_ref->rb_node_desc, parent, p);
  rb_insert_color(&new_ref->rb_node_desc, &proc->refs_by_desc);
  if (node) {
    hlist_add_head(&new_ref->node_entry, &node->refs);
  } 
  return new_ref;
}
```

handle值计算方法规律：

-   每个进程binder\_proc所记录的binder\_ref的handle值是从1开始递增的
-   所有进程binder\_proc所记录的handle=0的binder\_ref都指向service manager
-   同一服务的binder\_node在不同进程的binder\_ref的handle值可以不同

##### (七)、ServiceManager流程

关于ServiceManager的启动流程，我这里就不详细讲解了。启动后，就会循环在binder\_loop()过程，当来消息后，会调用binder\_parse()函数

###### 1、binder\_parse()函数

```
// framework/native/cmds/servicemanager/binder.c      204行
int binder_parse(struct binder_state *bs, struct binder_io *bio,
                 uintptr_t ptr, size_t size, binder_handler func)
{
    int r = 1;
    uintptr_t end = ptr + (uintptr_t) size;

    while (ptr < end) {
        uint32_t cmd = *(uint32_t *) ptr;
        ptr += sizeof(uint32_t);
        switch(cmd) {
        case BR_TRANSACTION: {
            struct binder_transaction_data *txn = (struct binder_transaction_data *) ptr;
            // *** 省略部分源码 ***
            binder_dump_txn(txn);
            if (func) {
                unsigned rdata[256/4];
                struct binder_io msg; 
                struct binder_io reply;
                int res;

                bio_init(&reply, rdata, sizeof(rdata), 4);
                //从txn解析出binder_io信息
                bio_init_from_txn(&msg, txn); 
                 // 收到Binder事务 
                res = func(bs, txn, &msg, &reply);
                // 发送reply事件
                binder_send_reply(bs, &reply, txn->data.ptr.buffer, res);
            }
            ptr += sizeof(*txn);
            break;
        }
        case :   // *** 省略部分源码 ***
    }
    return r;
}
```

###### 2、svcmgr\_handler()函数

```
//frameworks/native/cmds/servicemanager/service_manager.c  
    244行  
int svcmgr_handler(struct binder_state *bs,
                   struct binder_transaction_data *txn,
                   struct binder_io *msg,
                   struct binder_io *reply)
{
    struct svcinfo *si;
    uint16_t *s;
    size_t len;
    uint32_t handle;
    uint32_t strict_policy;
    int allow_isolated;
    // *** 省略部分源码 ***
    strict_policy = bio_get_uint32(msg);
    s = bio_get_string16(msg, &len);
     // *** 省略部分源码 ***
    switch(txn->code) {
      case SVC_MGR_ADD_SERVICE: 
          s = bio_get_string16(msg, &len);
          ...
          handle = bio_get_ref(msg); //获取handle
          allow_isolated = bio_get_uint32(msg) ? 1 : 0;
           //注册指定服务 
          if (do_add_service(bs, s, len, handle, txn->sender_euid,
              allow_isolated, txn->sender_pid))
              return -1;
          break;
       case : // *** 省略部分源码 ***
    }

    bio_put_uint32(reply, 0);
    return 0;
}
```

###### 3、do\_add\_service()函数

```
// frameworks/native/cmds/servicemanager/service_manager.c     194行
int do_add_service(struct binder_state *bs,
                   const uint16_t *s, size_t len,
                   uint32_t handle, uid_t uid, int allow_isolated,
                   pid_t spid)
{
    struct svcinfo *si;

    if (!handle || (len == 0) || (len > 127))
        return -1;

    //权限检查
    if (!svc_can_register(s, len, spid)) {
        return -1;
    }

    //服务检索
    si = find_svc(s, len);
    if (si) {
        if (si->handle) {
            //服务已经注册时，释放相应的服务
            svcinfo_death(bs, si); 
        }
        si->handle = handle;
    } else {
        si = malloc(sizeof(*si) + (len + 1) * sizeof(uint16_t));
        //内存不足时，无法分配足够的内存
        if (!si) { 
            return -1;
        }
        si->handle = handle;
        si->len = len;
         //内存拷贝服务信息
        memcpy(si->name, s, (len + 1) * sizeof(uint16_t)); 
        si->name[len] = '\0';
        si->death.func = (void*) svcinfo_death;
        si->death.ptr = si;
        si->allow_isolated = allow_isolated;
        //svclist保存所有已注册的服务
        si->next = svclist; 
        svclist = si;
    }

    //以BC_ACQUIRE命令，handle为目标的信息，通过ioctl发送给binder驱动
    binder_acquire(bs, handle);
    //以BC_REQUEST_DEATH_NOTIFICATION命令的信息，通过ioctl发送给binder驱动，主要用于清理内存等收尾工作。
    binder_link_to_death(bs, handle, &si->death);
    return 0;
}
```

svcinfo记录着服务名和handle信息

###### 4、binder\_send\_reply()函数

```
// frameworks/native/cmds/servicemanager/binder.c   170行
void binder_send_reply(struct binder_state *bs,
                       struct binder_io *reply,
                       binder_uintptr_t buffer_to_free,
                       int status)
{
    struct {
        uint32_t cmd_free;
        binder_uintptr_t buffer;
        uint32_t cmd_reply;
        struct binder_transaction_data txn;
    } __attribute__((packed)) data;
     //free buffer命令
    data.cmd_free = BC_FREE_BUFFER; 
    data.buffer = buffer_to_free;
    // reply命令
    data.cmd_reply = BC_REPLY; 
    data.txn.target.ptr = 0;
    data.txn.cookie = 0;
    data.txn.code = 0;
    if (status) {
     // *** 省略部分源码 ***
    } else {
        data.txn.flags = 0;
        data.txn.data_size = reply->data - reply->data0;
        data.txn.offsets_size = ((char*) reply->offs) - ((char*) reply->offs0);
        data.txn.data.ptr.buffer = (uintptr_t)reply->data0;
        data.txn.data.ptr.offsets = (uintptr_t)reply->offs0;
    }
    //向Binder驱动通信
    binder_write(bs, &data, sizeof(data));
}
```

binder\_write进去binder驱动后，将BC\_FREE\_BUFFER和BC\_REPLY命令协议发送给Binder驱动，向Client端发送reply binder\_write进入binder驱动后，将BC\_FREE\_BUFFER和BC\_REPLY命令协议发送给Binder驱动， 向client端发送reply.

##### (八)、总结

服务注册过程(addService)核心功能：在服务所在进程创建的binder\_node，在servicemanager进程创建binder\_ref。其中binder\_ref的desc在同一个进程内是唯一的：

-   每个进程binder\_proc所记录的binder\_ref的handle值是从1开始递增的
-   所有进程binder\_proc所记录的bandle=0的binder\_ref指向service manager
-   同一个服务的binder\_node在不同的进程的binder\_ref的handle值可以不同

Media服务注册的过程设计到MediaPlayerService(作为Cliient进程)和Service Manager(作为Service 进程)，通信的流程图如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/2bh5khsx6o.png)

Media服务注册流程.png

过程分析：

-   1、MediaPlayerService进程调用 **ioctl()**向Binder驱动发送IPC数据，该过程可以理解成一个事物 **binder\_transaction** (记为BT1)，执行当前操作线程的binder\_thread(记为 thread1)，则BT1 ->from\_parent=NULL， BT1 ->from=thread1，thread1 ->transaction\_stack=T1。其中IPC数据内容包括：
    -   Binder协议为BC\_TRANSACTION
    -   Handle等于0
    -   PRC代码为ADD\_SERVICE
    -   PRC数据为"media.player"
-   2、Binder驱动收到该Binder请求。生成BR\_TRANSACTION命令，选择目标处理该请求的线程，即ServiceManager的binder线程(记为thread2)，则T1->to\_parent=NULL,T1 -> to\_thread=thread2，并将整个binder\_transaction数据(记为BT2)插入到目标线程的todo队列。
-   3、Service Manager的线程thread收到BT2后，调用服务注册函数将服务“media.player”注册到服务目录中。当服务注册完成，生成IPC应答数据(BC\_REPLY)，BT2->from\_parent=BT1，BT2 ->from=thread2，thread2->transaction\_stack=BT2。
-   4、Binder驱动收到该Binder应答请求，生成BR\_REPLY命令，BT2->to\_parent=BT1，BT2->to\_thread1，thread1->transaction\_stack=BT2。在MediaPlayerService收到该命令后，知道服务注册完成便可以正常使用。

### 五、获取服务

#### (一) 源码位置

```
/frameworks/av/media/libmedia/
  - IMediaDeathNotifier.cpp

framework/native/libs/binder/
  - Binder.cpp
  - BpBinder.cpp
  - IPCThreadState.cpp
  - ProcessState.cpp
  - IServiceManager.cpp
```

对应的链接为

-   [Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FBinder.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [BpBinder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FBpBinder.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [IPCThreadState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [ProcessState.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FProcessState.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [IServiceManager.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIServiceManager.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [IInterface.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIServiceManager.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [IInterface.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FIInterface.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [IInterface.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Finclude%2Fbinder%2FIInterface.h&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [main\_mediaserver.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fav%2Fmedia%2Fmediaserver%2Fmain_mediaserver.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [MediaPlayerService.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fav%2Fmedia%2Flibmediaplayerservice%2FMediaPlayerService.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)
-   [IMediaDeathNotifier.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fav%2Fmedia%2Flibmedia%2FIMediaDeathNotifier.cpp&objectId=1199106&objectType=1&isNewArticle=undefined)

在Native层的服务注册，我们依旧选择media为例展开讲解，先来看看media类关系图。

##### (二)、类图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/iojcpquoz2.png)

类图.png

图解：

-   蓝色:代表获取MediaPlayerService服务相关的类
-   绿色:代表Binder架构中与Binder驱动通信过程中的最为核心的两个雷
-   紫色:代表 **注册服务** 和 **获取服务** 的公共接口/父类

##### (二)、获取服务流程

###### 1、getMediaPlayerService()函数

```
//frameworks/av/media/libmedia/IMediaDeathNotifier.cpp   35行
sp<IMediaPlayerService>&
IMediaDeathNotifier::getMediaPlayerService()
{
    Mutex::Autolock _l(sServiceLock);
    if (sMediaPlayerService == 0) {
         // 获取 ServiceManager
        sp<IServiceManager> sm = defaultServiceManager(); 
        sp<IBinder> binder;
        do {
            //获取名为"media.player"的服务
            binder = sm->getService(String16("media.player"));
            if (binder != 0) {
                break;
            }
            usleep(500000); // 0.5s
        } while (true);

        if (sDeathNotifier == NULL) {
            // 创建死亡通知对象
            sDeathNotifier = new DeathNotifier(); 
        }

        //将死亡通知连接到binder
        binder->linkToDeath(sDeathNotifier);
        sMediaPlayerService = interface_cast<IMediaPlayerService>(binder);
    }
    return sMediaPlayerService;
}
```

其中defaultServiceManager()过程在上面已经说了，返回的是BpServiceManager

> 在请求获取名为"media.player"的服务过程中，采用不断循环获取的方法。由于MediaPlayerService服务可能还没向ServiceManager注册完成或者尚未启动完成等情况，故则binder返回NULL，休眠0.5s后继续请求，知道获取服务为止。

###### 2、BpServiceManager.getService()函数

```
//frameworks/native/libs/binder/IServiceManager.cpp       134行
virtual sp<IBinder> getService(const String16& name) const
    {
        unsigned n;
        for (n = 0; n < 5; n++){
            sp<IBinder> svc = checkService(name); 
            if (svc != NULL) return svc;
            sleep(1);
        }
        return NULL;
    }
```

通过BpServiceManager来获取MediaPlayer服务：检索服务是否存在，当服务存在则返回相应的服务，当服务不存在则休眠1s再继续检索服务。该循环进行5次。为什么循环5次？这估计和Android的ANR的时间为5s相关。如果每次都无法获取服务，循环5次，每次循环休眠1s，忽略checkService()的时间，差不多是5s的时间。

###### 3、BpSeriveManager.checkService()函数

```
//frameworks/native/libs/binder/IServiceManager.cpp    146行
virtual sp<IBinder> checkService( const String16& name) const
{
    Parcel data, reply;
    //写入RPC头
    data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());
    //写入服务名
    data.writeString16(name);
    remote()->transact(CHECK_SERVICE_TRANSACTION, data, &reply); 
    return reply.readStrongBinder(); 
}
```

检索制定服务是否存在，其中remote()为BpBinder

###### 4、BpBinder::transact()函数

```
// /frameworks/native/libs/binder/BpBinder.cpp    159行
status_t BpBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    if (mAlive) {
        status_t status = IPCThreadState::self()->transact(
            mHandle, code, data, reply, flags);
        if (status == DEAD_OBJECT) mAlive = 0;
        return status;
    }
    return DEAD_OBJECT;
}
```

Binder代理类调用transact()方法，真正工作还是交给IPCThreadState来进行transact工作。

###### 4.1、IPCThreadState::self()函数

```
IPCThreadState* IPCThreadState::self()
{
    if (gHaveTLS) {
restart:
        const pthread_key_t k = gTLS;
        IPCThreadState* st = (IPCThreadState*)pthread_getspecific(k);
        if (st) return st;
        //初始化 IPCThreadState
        return new IPCThreadState;  
    }

    if (gShutdown) return NULL;
    pthread_mutex_lock(&gTLSMutex);
     //首次进入gHaveTLS为false
    if (!gHaveTLS) {
         //创建线程的TLS
        if (pthread_key_create(&gTLS, threadDestructor) != 0) { 
            pthread_mutex_unlock(&gTLSMutex);
            return NULL;
        }
        gHaveTLS = true;
    }
    pthread_mutex_unlock(&gTLSMutex);
    goto restart;
}
```

TLS是指Thread local storage(线程本地存储空间)，每个线程都拥有自己的TLS，并且是私有空间，线程之间不会共享。通过pthread\_getspecific()/pthread\_setspecific()函数可以获取/设置这些空间中的内容。从线程本地存储空间获的保存期中的IPCThreadState对象。

###### 以后面的流程和上面的注册流程大致相同，主要流程也是 IPCThreadState:: transact()函数、IPCThreadState::writeTransactionData()函数、IPCThreadState::waitForResponse()函数和IPCThreadState.talkWithDriver()函数，由于上面已经讲解过了，这里就不详细说明了。我们从IPCThreadState.talkWithDriver() 开始继讲解

###### 4.2、IPCThreadState:: talkWithDriver()函数

```
status_t IPCThreadState::talkWithDriver(bool doReceive)
{
    ...
    binder_write_read bwr;
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();
    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;
    bwr.write_size = outAvail;
    bwr.write_buffer = (uintptr_t)mOut.data();
     //接收数据缓冲区信息的填充。如果以后收到数据，就直接填在mIn中了。
    if (doReceive && needRead) {
        bwr.read_size = mIn.dataCapacity();
        bwr.read_buffer = (uintptr_t)mIn.data();
    } else {
        bwr.read_size = 0;
        bwr.read_buffer = 0;
    }
    //当读缓冲和写缓冲都为空，则直接返回
    if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;

    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    status_t err;
    do {
        //通过ioctl不停的读写操作，跟Binder Driver进行通信
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
        ...
       //当被中断，则继续执行
    } while (err == -EINTR); 
    ...
    return err;
}
```

**binder\_write\_read结构体** 用来与Binder设备交换数据的结构，通过ioctl与mDriverFD通信，是真正的与Binder驱动进行数据读写交互的过程。先向service manager进程发送查询服务的请求(BR\_TRANSACTION)。当service manager 进程收到带命令后，会执行do\_find\_service()查询服务所对应的handle，然后再binder\_send\_reply()应发送者，发送BC\_REPLY协议，然后再调用binder\_transaction()，再向服务请求者的todo队列插入事务。接下来，再看看binder\_transaction过程。

让我们继续看下binder\_transaction的过程

###### 4.2.1、binder\_transaction()函数

```
//kernel/drivers/android/binder.c     1827行
static void binder_transaction(struct binder_proc *proc,
               struct binder_thread *thread,
               struct binder_transaction_data *tr, int reply){
    //根据各种判定，获取以下信息：
    // 目标线程
    struct binder_thread *target_thread； 
    // 目标进程
    struct binder_proc *target_proc； 
     /// 目标binder节点   
    struct binder_node *target_node；    
    // 目标 TODO队列
    struct list_head *target_list；    
     // 目标等待队列
    wait_queue_head_t *target_wait；    
    ...
    
    //分配两个结构体内存
    struct binder_transaction *t = kzalloc(sizeof(*t), GFP_KERNEL);
    struct binder_work *tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);
    //从target_proc分配一块buffer
    t->buffer = binder_alloc_buf(target_proc, tr->data_size,

    for (; offp < off_end; offp++) {
        switch (fp->type) {
        case BINDER_TYPE_BINDER: ...
        case BINDER_TYPE_WEAK_BINDER: ...
        
        case BINDER_TYPE_HANDLE: 
        case BINDER_TYPE_WEAK_HANDLE: {
          struct binder_ref *ref = binder_get_ref(proc, fp->handle,
                fp->type == BINDER_TYPE_HANDLE);
          ...
          //此时运行在servicemanager进程，故ref->node是指向服务所在进程的binder实体，
          //而target_proc为请求服务所在的进程，此时并不相等。
          if (ref->node->proc == target_proc) {
            if (fp->type == BINDER_TYPE_HANDLE)
              fp->type = BINDER_TYPE_BINDER;
            else
              fp->type = BINDER_TYPE_WEAK_BINDER;
            fp->binder = ref->node->ptr;
             // BBinder服务的地址
            fp->cookie = ref->node->cookie; 
            binder_inc_node(ref->node, fp->type == BINDER_TYPE_BINDER, 0, NULL);
            
          } else {
            struct binder_ref *new_ref;
            //请求服务所在进程并非服务所在进程，则为请求服务所在进程创建binder_ref
            new_ref = binder_get_ref_for_node(target_proc, ref->node);
            fp->binder = 0;
             //重新给handle赋值
            fp->handle = new_ref->desc; 
            fp->cookie = 0;
            binder_inc_ref(new_ref, fp->type == BINDER_TYPE_HANDLE, NULL);
          }
        } break;
        
        case BINDER_TYPE_FD: ...
        }
    }
    //分别target_list和当前线程TODO队列插入事务
    t->work.type = BINDER_WORK_TRANSACTION;
    list_add_tail(&t->work.entry, target_list);
    tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
    list_add_tail(&tcomplete->entry, &thread->todo);
    if (target_wait)
        wake_up_interruptible(target_wait);
    return;
}
```

这个过程非常重要，分两种情况来说：

-   情况1 当请求服务的进程与服务属于不同的进程，则为请求服务所在进程创建binder\_ref对象，指向服务进程中的binder\_node
-   当请求服务的进程与服务属于同一进程，则不再创建新对象，只是引用计数+1，并且修改type为BINDER\_TYPE\_BINER或BINDER\_TYPE\_WEAK\_BINDER。

###### 4.2.2、binder\_thread\_read()函数

```
//kernel/drivers/android/binder.c    2650行
binder_thread_read（struct binder_proc *proc,struct binder_thread *thread,binder_uintptr_t binder_buffer, size_t size,binder_size_t *consumed, int non_block）{
    ...
    //当线程todo队列有数据则执行往下执行；当线程todo队列没有数据，则进入休眠等待状态
    ret = wait_event_freezable(thread->wait, binder_has_thread_work(thread));
    ...
    while (1) {
        uint32_t cmd;
        struct binder_transaction_data tr;
        struct binder_work *w;
        struct binder_transaction *t = NULL;
        //先从线程todo队列获取事务数据
        if (!list_empty(&thread->todo)) {
            w = list_first_entry(&thread->todo, struct binder_work, entry);
        // 线程todo队列没有数据, 则从进程todo对获取事务数据
        } else if (!list_empty(&proc->todo) && wait_for_proc_work) {
            ...
        }
        switch (w->type) {
            case BINDER_WORK_TRANSACTION:
                //获取transaction数据
                t = container_of(w, struct binder_transaction, work);
                break;
                
            case : ...  
        }

        //只有BINDER_WORK_TRANSACTION命令才能继续往下执行
        if (!t) continue;

        if (t->buffer->target_node) {
            ...
        } else {
            tr.target.ptr = NULL;
            tr.cookie = NULL;
            //设置命令为BR_REPLY
            cmd = BR_REPLY; 
        }
        tr.code = t->code;
        tr.flags = t->flags;
        tr.sender_euid = t->sender_euid;

        if (t->from) {
            struct task_struct *sender = t->from->proc->tsk;
            //当非oneway的情况下,将调用者进程的pid保存到sender_pid
            tr.sender_pid = task_tgid_nr_ns(sender, current->nsproxy->pid_ns);
        } else {
            ...
        }

        tr.data_size = t->buffer->data_size;
        tr.offsets_size = t->buffer->offsets_size;
        tr.data.ptr.buffer = (void *)t->buffer->data +
                    proc->user_buffer_offset;
        tr.data.ptr.offsets = tr.data.ptr.buffer +
                    ALIGN(t->buffer->data_size,
                        sizeof(void *));

        //将cmd和数据写回用户空间
        put_user(cmd, (uint32_t __user *)ptr);
        ptr += sizeof(uint32_t);
        copy_to_user(ptr, &tr, sizeof(tr));
        ptr += sizeof(tr);

        list_del(&t->work.entry);
        t->buffer->allow_user_free = 1;
        if (cmd == BR_TRANSACTION && !(t->flags & TF_ONE_WAY)) {
            ...
        } else {
            t->buffer->transaction = NULL;
            //通信完成则运行释放
            kfree(t); 
        }
        break;
    }
done:
    *consumed = ptr - buffer;
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

###### 4.3、readStrongBinder()函数

```
//frameworks/native/libs/binder/Parcel.cpp   1334行
sp<IBinder> Parcel::readStrongBinder() const
{
    sp<IBinder> val;
    unflatten_binder(ProcessState::self(), *this, &val);
    return val;
}
```

里面主要是调用unflatten\_binder()函数 那我们就来详细看下

###### 4.3.1、unflatten\_binder()函数

```
status_t unflatten_binder(const sp<ProcessState>& proc,
    const Parcel& in, sp<IBinder>* out)
{
    const flat_binder_object* flat = in.readObject(false);
    if (flat) {
        switch (flat->type) {
            case BINDER_TYPE_BINDER:
                // 当请求服务的进程与服务属于同一进程
                *out = reinterpret_cast<IBinder*>(flat->cookie);
                return finish_unflatten_binder(NULL, *flat, in);
            case BINDER_TYPE_HANDLE:
                //请求服务的进程与服务属于不同进程
                *out = proc->getStrongProxyForHandle(flat->handle);
                //创建BpBinder对象
                return finish_unflatten_binder(
                    static_cast<BpBinder*>(out->get()), *flat, in);
        }
    }
    return BAD_TYPE;
}
```

如果服务的进程与服务属于不同的进程会调用getStrongProxyForHandle()函数，那我们就好好研究下

###### 4.3.2、getStrongProxyForHandle()函数

```
sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);
    //查找handle对应的资源项[2.9.3]
    handle_entry* e = lookupHandleLocked(handle);

    if (e != NULL) {
        IBinder* b = e->binder;
        if (b == NULL || !e->refs->attemptIncWeak(this)) {
            ...
            //当handle值所对应的IBinder不存在或弱引用无效时，则创建BpBinder对象
            b = new BpBinder(handle);
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }
    return result;
}
```

readStrong的功能是flat\_binder\_object解析并创建BpBinder对象

###### 4.3.2、getStrongProxyForHandle()函数

```
ProcessState::handle_entry* ProcessState::lookupHandleLocked(int32_t handle)
{
    const size_t N=mHandleToObject.size();
    //当handle大于mHandleToObject的长度时，进入该分支
    if (N <= (size_t)handle) {
        handle_entry e;
        e.binder = NULL;
        e.refs = NULL;
        //从mHandleToObject的第N个位置开始，插入(handle+1-N)个e到队列中
        status_t err = mHandleToObject.insertAt(e, N, handle+1-N);
        if (err < NO_ERROR) return NULL;
    }
    return &mHandleToObject.editItemAt(handle);
}
```

根据handle值来查找对应的handle\_entry。

##### (三)、死亡通知

死亡通知时为了让Bp端知道Bn端的生死情况

-   DeathNotifier是继承IBinder::DeathRecipient类，主要需要实现其binderDied()来进行死亡通告。
-   注册：binder->linkToDeath(sDeathNotifier)是为了将sDeathNotifier死亡通知注册到Binder上。

Bp端只需要覆写binderDied()方法，实现一些后尾清楚类的工作，则在Bn端死掉后，会回调binderDied()进行相应处理

###### 1、linkToDeath()函数

```
// frameworks/native/libs/binder/BpBinder.cpp   173行
status_t BpBinder::linkToDeath(
    const sp<DeathRecipient>& recipient, void* cookie, uint32_t flags)
{
    Obituary ob;
    ob.recipient = recipient;
    ob.cookie = cookie;
    ob.flags = flags;

    {
        AutoMutex _l(mLock);
        if (!mObitsSent) {
            if (!mObituaries) {
                mObituaries = new Vector<Obituary>;
                if (!mObituaries) {
                    return NO_MEMORY;
                }
                getWeakRefs()->incWeak(this);
                IPCThreadState* self = IPCThreadState::self();
                self->requestDeathNotification(mHandle, this);
                self->flushCommands();
            }
            ssize_t res = mObituaries->add(ob);
            return res >= (ssize_t)NO_ERROR ? (status_t)NO_ERROR : res;
        }
    }
    return DEAD_OBJECT;
}
```

里面调用了requestDeathNotification()函数

###### 2、requestDeathNotification()函数

```
//frameworks/native/libs/binder/IPCThreadState.cpp    670行
status_t IPCThreadState::requestDeathNotification(int32_t handle, BpBinder* proxy)
{
    mOut.writeInt32(BC_REQUEST_DEATH_NOTIFICATION);
    mOut.writeInt32((int32_t)handle);
    mOut.writePointer((uintptr_t)proxy);
    return NO_ERROR;
}
```

向binder driver发送 BC\_REQUEST\_DEATH\_NOTIFICATION命令。后面的流程和 **Service Manager** 里面的 \*\* binder\_link\_to\_death() \*\* 的过程。

###### 3、binderDied()函数

```
//frameworks/av/media/libmedia/IMediaDeathNotifier.cpp    78行
void IMediaDeathNotifier::DeathNotifier::binderDied(const wp<IBinder>& who __unused) {
    SortedVector< wp<IMediaDeathNotifier> > list;
    {
        Mutex::Autolock _l(sServiceLock);
        // 把Bp端的MediaPlayerService清除掉
        sMediaPlayerService.clear();   
        list = sObitRecipients;
    }

    size_t count = list.size();
    for (size_t iter = 0; iter < count; ++iter) {
        sp<IMediaDeathNotifier> notifier = list[iter].promote();
        if (notifier != 0) {
            //当MediaServer挂了则通知应用程序，应用程序回调该方法
            notifier->died(); 
        }
    }
}
```

客户端进程通过Binder驱动获得Binder的代理(BpBinder)，死亡通知注册的过程就是客户端进程向Binder驱动注册的一个死亡通知，该死亡通知关联BBinder，即与BpBinder所对应的服务端。

###### 4、unlinkToDeath()函数

当Bp在收到服务端的死亡通知之前先挂了，那么需要在对象的销毁方法内，调用unlinkToDeath()来取消死亡通知；

```
//frameworks/av/media/libmedia/IMediaDeathNotifier.cpp    101行
IMediaDeathNotifier::DeathNotifier::~DeathNotifier()
{
    Mutex::Autolock _l(sServiceLock);
    sObitRecipients.clear();
    if (sMediaPlayerService != 0) {
        IInterface::asBinder(sMediaPlayerService)->unlinkToDeath(this);
    }
}
```

###### 5、触发时机

> 每当service进程退出时，service manager 会收到来自Binder驱动的死亡通知。这项工作在启动Service Manager时通过 binder\_link\_to\_death(bs, ptr, &si->death)完成。另外，每个Bp端也可以自己注册死亡通知，能获取Binder的死亡消息，比如前面的IMediaDeathNotifier。

那Binder的死亡通知时如何被出发的？对于Binder的IPC进程都会打开/dev/binder文件，当进程异常退出的时候，Binder驱动会保证释放将要退出的进程中没有正常关闭的/dev/binder文件，实现机制是binder驱动通过调用/dev/binder文件所对应的release回调函数，执行清理工作，并且检查BBinder是否有注册死亡通知，当发现存在死亡通知时，那么久向其对应的BpBinder端发送死亡通知消息。

##### (三)总结

在请求服务(getService)的过程，当执行到binder\_transaction()时，会区分请求服务所属进程情况。

-   当请求服务的进程与服务属于不同进程，则为请求服务所在进程创binder\_ref对象，指向服务进程的binder\_noder
-   当请求服务的进程与服务属于同一进程， 则不再创建新对象，只是引用计数+1，并且修改type为BINDER\_TYPE\_BINDER或BINDER\_TYPE\_WEAK\_BINDER。
    -   最终readStrongBinder()，返回的是BB对象的真实子类