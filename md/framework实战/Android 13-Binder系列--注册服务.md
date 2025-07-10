> 基于Android T 分析客户端进程如何通过Binder向ServiceManager进程注册服务的过程  
> hongxi.zhu 2023-6-29

以注册SurfaceFlinger为例，分析客户端进程如何通过Binder向ServiceManager进程注册服务的过程。

### 一、 客户端进程发起注册服务请求

frameworks/native/services/surfaceflinger/main\_surfaceflinger.cpp

```cpp
    // publish surface flinger
    sp<IServiceManager> sm(defaultServiceManager());
    sm->addService(String16(SurfaceFlinger::getServiceName()), flinger, false,
                   IServiceManager::DUMP_FLAG_PRIORITY_CRITICAL | IServiceManager::DUMP_FLAG_PROTO);

    // publish gui::ISurfaceComposer, the new AIDL interface
    sp<SurfaceComposerAIDL> composerAIDL = new SurfaceComposerAIDL(flinger);
    sm->addService(String16("SurfaceFlingerAIDL"), composerAIDL, false,
                   IServiceManager::DUMP_FLAG_PRIORITY_CRITICAL | IServiceManager::DUMP_FLAG_PROTO);
```

在`SurfaceFlinger`的`main`函数里，首先是获取`IServiceManager`的对象，其实也就是`BpServiceManager`对象，然后再通过`BpServiceManager->addService()`注册相关SurfaceFlinger相关的两个服务。那么看下这个IServiceManager的对象是怎么获取的, 从上面可以看到是`defaultServiceManager()`中返回的。

### 二、获取ServiceManager

获取到SM的代理对象之前的文章已经分析过，请参考前面的：Binder系列–获取ServiceManager

### 三、向ServiceManager发起注册服务请求

前面获取到SM的代理对象后，就调用它的`addService`方法向远程SM发起注册服务请求。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/77916210b9fe5edb386f58909fc955b8.png#pic_center)

#### BpServiceManager::addService

BpServiceManager已经由AIDL的方式实现  
out/soong/.intermediates/frameworks/native/libs/binder/libbinder/android\_x86\_64\_shared/gen/aidl/android/os/IServiceManager.cpp

```cpp
::android::binder::Status BpServiceManager::addService(const ::std::string& name, const ::android::sp<::android::IBinder>& service, bool allowIsolated, int32_t dumpPriority) {
  ::android::Parcel _aidl_data;
  _aidl_data.markForBinder(remoteStrong());
  ::android::Parcel _aidl_reply;
  ::android::status_t _aidl_ret_status = ::android::OK;
  ::android::binder::Status _aidl_status;
  _aidl_ret_status = _aidl_data.writeInterfaceToken(getInterfaceDescriptor());  //服务接口描述
	...
  _aidl_ret_status = _aidl_data.writeUtf8AsUtf16(name);  //服务名
	...
  _aidl_ret_status = _aidl_data.writeStrongBinder(service);  //服务的BBinder
	...
  _aidl_ret_status = _aidl_data.writeBool(allowIsolated);
	...
  _aidl_ret_status = _aidl_data.writeInt32(dumpPriority);
	...
  _aidl_ret_status = remote()->transact(BnServiceManager::TRANSACTION_addService, _aidl_data, &_aidl_reply, 0);  //remote()实际上获取BpBinder, 在BpServiceManager初始化时会初始化他的父类，然后将父类mRemote指向handle对应的BpBinder，而remote()获取到的就是这个BpBinder，流程比较复杂
	...
  _aidl_ret_status = _aidl_status.readFromParcel(_aidl_reply);
	...
  return _aidl_status;
}
```

将服务相关的信息写入`Parcel`中，然后`remote()->transact`调用传输，其中比较重要的就是`writeStrongBinder`、`remote()->transact`,

#### writeStrongBinder(service)

frameworks/native/libs/binder/Parcel.cpp

```cpp
status_t Parcel::writeStrongBinder(const sp<IBinder>& val)
{
    return flattenBinder(val);
}

```

调用的是`flattenBinder`, 意思为压扁Binder对象

#### flattenBinder

```cpp
status_t Parcel::flattenBinder(const sp<IBinder>& binder) {
    BBinder* local = nullptr;
    if (binder) local = binder->localBinder(); //我们是注册服务，之后是作为服务端运行，所以是BBinder，也就是localBinder
    if (local) local->setParceled();
	...
    flat_binder_object obj;
	...
    if (binder != nullptr) {
        if (!local) {
            BpBinder *proxy = binder->remoteBinder();
            if (proxy == nullptr) {
                ALOGE("null proxy");
            } else {
                if (proxy->isRpcBinder()) {
                    ALOGE("Sending a socket binder over kernel binder is prohibited");
                    return INVALID_OPERATION;
                }
            }
            const int32_t handle = proxy ? proxy->getPrivateAccessor().binderHandle() : 0;
            obj.hdr.type = BINDER_TYPE_HANDLE;
            obj.binder = 0; /* Don't pass uninitialized stack data to a remote process */
            obj.flags = 0;
            obj.handle = handle;   //如果是BpBinder，赋值handle字段为BpBinder的handle值
            obj.cookie = 0;
        } else {  //注册服务走这里
            int policy = local->getMinSchedulerPolicy();
            int priority = local->getMinSchedulerPriority();

            if (policy != 0 || priority != 0) {
                // override value, since it is set explicitly
                schedBits = schedPolicyMask(policy, priority);
            }
            obj.flags = FLAT_BINDER_FLAG_ACCEPTS_FDS;
            if (local->isRequestingSid()) {
                obj.flags |= FLAT_BINDER_FLAG_TXN_SECURITY_CTX;
            }
            if (local->isInheritRt()) {
                obj.flags |= FLAT_BINDER_FLAG_INHERIT_RT;
            }
            obj.hdr.type = BINDER_TYPE_BINDER;
            obj.binder = reinterpret_cast<uintptr_t>(local->getWeakRefs()); //如果是BBinder，赋值BBinder本身的弱引用到binder字段
            obj.cookie = reinterpret_cast<uintptr_t>(local); //如果是BBinder，赋值BBinder本身到cookie字段
        }
    } else {
        obj.hdr.type = BINDER_TYPE_BINDER;
        obj.flags = 0;
        obj.binder = 0;
        obj.cookie = 0;
    }

    obj.flags |= schedBits;

    status_t status = writeObject(obj, false);  //将flat_binder_object写入parcel中
    if (status != OK) return status;

    return finishFlattenBinder(binder);
}
```

#### remote()->transact

frameworks/native/libs/binder/BpBinder.cpp

```cpp
// NOLINTNEXTLINE(google-default-arguments)
status_t BpBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    // Once a binder has died, it will never come back to life.
    if (mAlive) {
		...
        status_t status;
        if (CC_UNLIKELY(isRpcBinder())) {  //这个是socket binder,是Google的新东西，使用RpcHandle构造的BpBinder走这里
            status = rpcSession()->transact(sp<IBinder>::fromExisting(this), code, data, reply,
                                            flags);
        } else {  //handle构造的BpBinder走这里，传统binder是handle构造的BpBinder
            status = IPCThreadState::self()->transact(binderHandle(), code, data, reply, flags);
        }
		...
        if (status == DEAD_OBJECT) mAlive = 0;

        return status;
    }

    return DEAD_OBJECT;
}
```

`BpBinder->transact`最终还是要调`IPCThreadState::self()->transact`  
frameworks/native/libs/binder/IPCThreadState.cpp

```cpp
status_t IPCThreadState::transact(int32_t handle,
                                  uint32_t code, const Parcel& data,
                                  Parcel* reply, uint32_t flags)
{
    status_t err;

    flags |= TF_ACCEPT_FDS;
	//将数据写入mOut(binder_transaction_data)
    err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, nullptr);
	...
    if ((flags & TF_ONE_WAY) == 0) {  //同步请求，需要等待服务端回复reply
		...
        if (reply) {
            err = waitForResponse(reply);  //同步请求，
        } else {
            Parcel fakeReply;
            err = waitForResponse(&fakeReply);
        }
		...
    } else {  //异步请求，不需要reply，直接返回
        err = waitForResponse(nullptr, nullptr);  //异步请求
    }

    return err;
}
```

#### writeTransactionData

```cpp
status_t IPCThreadState::writeTransactionData(int32_t cmd, uint32_t binderFlags,
    int32_t handle, uint32_t code, const Parcel& data, status_t* statusBuffer)
{
    binder_transaction_data tr;

    tr.target.ptr = 0; /* Don't pass uninitialized stack data to a remote process */
    tr.target.handle = handle; //目标服务端的BpBinder的handle，这里目标服务端是SM进程，handle = 0
    tr.code = code;  //TRANSACTION_addService  = FIRST_CALL_TRANSACTION + 2
    tr.flags = binderFlags;
    tr.cookie = 0;
    tr.sender_pid = 0;
    tr.sender_euid = 0;

    const status_t err = data.errorCheck();
    if (err == NO_ERROR) {  //走这里
        tr.data_size = data.ipcDataSize();  //mDataSize
        tr.data.ptr.buffer = data.ipcData();  //mData
        tr.offsets_size = data.ipcObjectsCount()*sizeof(binder_size_t);  //mObjectsSize * binder_size_t
        tr.data.ptr.offsets = data.ipcObjects();  //mObjects
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

    mOut.writeInt32(cmd);   //BC_TRANSACTION
    mOut.write(&tr, sizeof(tr));  //将cmd和binder_transaction_data写入mOut

    return NO_ERROR;
}
```

`binder_transaction_data`是用户进程与驱动之间传输的数据结构， `writeTransactionData`主要是将数据封装到`binder_transaction_data`，然后将`cmd`和`binder_transaction_data`写入`mOut(Parcel)`

#### waitForResponse(reply)

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

        cmd = (uint32_t)mIn.readInt32();  //从输入缓冲区读取cmd操作数
        
        switch (cmd) {
        case BR_ONEWAY_SPAM_SUSPECT:
		...
        case BR_TRANSACTION_COMPLETE:
		...
        case BR_DEAD_REPLY:
		...
        case BR_FAILED_REPLY:
		...
        case BR_FROZEN_REPLY:
		...
        case BR_ACQUIRE_RESULT:
		...
        case BR_REPLY:
            {
    			...
            }
            goto finish;

        default:
            err = executeCommand(cmd);  //执行驱动返回的命令
            if (err != NO_ERROR) goto finish;
            break;
        }
    }

finish:
	...

    return err;
}
```

`waitForResponse`主要是等待驱动（服务端进程通过驱动返回的消息）的消息返回，然后根据返回的cmd进行处理。

#### talkWithDriver

```cpp
status_t IPCThreadState::talkWithDriver(bool doReceive)  //doReceive 默认为true
{
    binder_write_read bwr;  //binder_write_read是进程和驱动之间交换数据的数据结构

    // Is the read buffer empty?（判断mIn输入缓冲区是否为空）
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();  //检测mIn中是否有数据

    // We don't want to write anything if we are still reading
    // from data left in the input buffer and the caller
    // has requested to read the next data.
    // 如果doReceive = false（当前进程需要读取驱动返回的数据）且当前输入缓冲区mIn还有数据没处理完，那么就不向驱动写数据，设置outAvail = 0
    // 如果doReceive = true（当前进程不需要读取驱动返回的数据）或者当前输入缓冲区mIn数据已经处理完，那么就向驱动写数据，设置outAvail =  mOut.dataSize()
    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;

    bwr.write_size = outAvail; // outAvail = mOut.dataSize()才会写，等于0就不会写数据
    bwr.write_buffer = (uintptr_t)mOut.data();   //将mOut中的数据写入binder_write_read结构体write_buffer中

    // This is what we'll read.
    if (doReceive && needRead) {  //需要向驱动发起一次读取了
        bwr.read_size = mIn.dataCapacity();  //读取的大小为256字节(IPCThreadState构造时制定了mIn和mOut大小)
        bwr.read_buffer = (uintptr_t)mIn.data();
    } else {  //不需要读取就将read_size和read_buffer设置为0
        bwr.read_size = 0;
        bwr.read_buffer = 0;
    }
	...
    // Return immediately if there is nothing to do.
    if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;  // 不需要读也不需要写

    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    status_t err;
    do {
		...        
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)  //向驱动发起一次读/写请求，并等待驱动返回
            err = NO_ERROR;
        else
            err = -errno;
        ...
    } while (err == -EINTR);  //除非系统调用过程中发生异常-EINTR会再次循环，否则执行一次就退出这个循环
	...
    if (err >= NO_ERROR) {
        if (bwr.write_consumed > 0) {  //驱动已经处理完写入的数据
				...
                mOut.setDataSize(0);  //将输出缓冲区mOut中的数据大小重置为0
                processPostWriteDerefs();  //将已经处理的命令数据引用移除
        }
        if (bwr.read_consumed > 0) {  //驱动已经处理完读取的数据, 并将数据写入了mIn
            mIn.setDataSize(bwr.read_consumed);  //mIn.setDataSize大小设置为驱动实际上写入mIn的字节数
            mIn.setDataPosition(0);  //数据地址地址从头开始，表示数据可从头开始读取（Parcel是有序的内存，需要有序读取）
        }
		...
        return NO_ERROR;
    }

    return err;
}
```

处理流程：  
1\. 先读后写  
2\. 读的时候，设置bwr.write\_size = 0，bwr.read\_size为mIn.dataCapacity()  
3\. 写的时候，设置bwr.read\_size = 0， bwr.write\_size为mOut.dataSize()

相关变量  
doReceive = true(默认)，表示调用者调用talkWithDriver接受来自binder驱动的返回命令cmd和data  
doReceive = false，表示调用者调用talkWithDriver只关心接受来自binder驱动的返回命令cmd

needRead = true 表示mIn中的数据已经被当前进程读取完成，可以继续向驱动发起下一次读取动作了  
needRead = false 表示mIn中的数据已经被当前进程读取还没有完成，还有数据没读取完

bwr.write\_consumed 驱动已经处理的写入字节数 （驱动读取本进程写给驱动的字节数）  
bwr.read\_consumed 驱动已经处理的读取字节数（向本进程输入缓冲区写入的字节数）

上面的处理主要就是通过`ioctl`往驱动读写的过程, 读写的操作主要就是围绕`mIn`和`mOut`进行，需要熟悉Parcel的机制才能完全理解。对于注册服务这个意图来说，我们主要关心写的过程，当`mIn`已经处理完成，就可以发起一次写操作，将`mOut.dataSize()` 和`mOut.data()` 分别写入binder\_write\_read结构的`write_size` 和`write_buffer`中，通过`ioctl`的BINDER\_WRITE\_READ操作进入驱动处理。

### 四、驱动部分

客户端用户进程调用`ioctl()`后就会调到驱动的`binder_ioctl`方法中（此时仍处于客户端用户进程上下文）

#### binder\_ioctl

/kernel/google/wahoo/drivers/android/binder.c

```cpp
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct binder_proc *proc = filp->private_data;
	struct binder_thread *thread;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;

	//如果进入iocll时，用户进程调用过程发生了异常，binder_stop_on_user_error的值 >=2 则挂起当前用户进程
	ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);

	thread = binder_get_thread(proc);  //获取当前客户端的当前线程
	if (thread == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	switch (cmd) {
	case BINDER_WRITE_READ:  //和驱动进行读写操作，主要是这个case完成跨进程数据交互
		ret = binder_ioctl_write_read(filp, cmd, arg, thread);
		if (ret)
			goto err;
		break;
	case BINDER_SET_MAX_THREADS: {  //设置客户端进程最大binder工作线程数
		int max_threads;

		if (copy_from_user(&max_threads, ubuf,
				   sizeof(max_threads))) {
			ret = -EINVAL;
			goto err;
		}
		binder_inner_proc_lock(proc);
		proc->max_threads = max_threads;
		binder_inner_proc_unlock(proc);
		break;
	}

	case BINDER_SET_CONTEXT_MGR:  //成为全局唯一个ServiceManager
		ret = binder_ioctl_set_ctx_mgr(filp, NULL);
		if (ret)
			goto err;
		break;
	case BINDER_VERSION: {  //获取BINDER_VERSION
		struct binder_version __user *ver = ubuf;

		if (size != sizeof(struct binder_version)) {
			ret = -EINVAL;
			goto err;
		}
		if (put_user(BINDER_CURRENT_PROTOCOL_VERSION,
			     &ver->protocol_version)) {  //获取内核Binder版本，并通过put_user()将结果拷贝回用户进程地址空间
			ret = -EINVAL;
			goto err;
		}
		break;
	}
	...
}
```

Binder驱动`binder_ioctl`中主要是`BINDER_WRITE_READ`处理两个进程间的交互，通过`binder_ioctl_write_read`处理用户进程的读写操作

#### binder\_ioctl\_write\_read

```cpp
static int binder_ioctl_write_read(struct file *filp,
				unsigned int cmd, unsigned long arg,
				struct binder_thread *thread)
{
	int ret = 0;
	struct binder_proc *proc = filp->private_data;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;
	struct binder_write_read bwr;

	if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {  //将用户空间的ubuf缓冲区拷贝到内核空间bwr结构体变量
		ret = -EFAULT;
		goto out;
	}

	//驱动中是先写后读，如果write_size > 0就先处理写操作
	if (bwr.write_size > 0) {  
		ret = binder_thread_write(proc, thread,
					  bwr.write_buffer,
					  bwr.write_size,
					  &bwr.write_consumed);
		trace_binder_write_done(ret);
		if (ret < 0) {
			bwr.read_consumed = 0;
			if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
				ret = -EFAULT;
			goto out;
		}
	}
	
	//如果没有写操作或者写操作完成就处理读操作
	if (bwr.read_size > 0) {  
		//读是阻塞操作，会挂起进程，让出CPU，直到别的地方唤醒
		ret = binder_thread_read(proc, thread, bwr.read_buffer,
					 bwr.read_size,
					 &bwr.read_consumed,
					 filp->f_flags & O_NONBLOCK);
		trace_binder_read_done(ret);
		binder_inner_proc_lock(proc);
		if (!binder_worklist_empty_ilocked(&proc->todo))
			binder_wakeup_proc_ilocked(proc);
		binder_inner_proc_unlock(proc);
		if (ret < 0) {
			if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
				ret = -EFAULT;
			goto out;
		}
	}
	
	if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {  //将读取的数据返回到用户进程中(mIn中)
		ret = -EFAULT;
		goto out;
	}
out:
	return ret;
}
```

`binder_ioctl_write_read` 中先处理写操作(如果write\_size>0), 然后再处理读操作(如果read\_size>0)，其中读操作是阻塞操作，将会挂起当前用户进程，让出CPU资源。我们当前仍处于请求注册服务客户端进程中，为写操作, 所以进到`binder_thread_write`

#### binder\_thread\_write

```cpp
static int binder_thread_write(struct binder_proc *proc,
			struct binder_thread *thread,
			binder_uintptr_t binder_buffer, size_t size,
			binder_size_t *consumed)
{
	uint32_t cmd;
	struct binder_context *context = proc->context;
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

	while (ptr < end && thread->return_error.cmd == BR_OK) {
		int ret;

		if (get_user(cmd, (uint32_t __user *)ptr))  //从用户空间ptr中拷贝cmd到内核空间（这个ptr指向mOut那块数据缓冲区,cmd就是最前面的命令头）
			return -EFAULT;
		ptr += sizeof(uint32_t);

		switch (cmd) {
		...
		case BC_TRANSACTION:  //我们传入的为BC_TRANSACTION
		case BC_REPLY: {
			struct binder_transaction_data tr;

			if (copy_from_user(&tr, ptr, sizeof(tr)))  //从用户空间到内核空间的拷贝，赋给binder_transaction_data结构体（用户进程和内核空间相同的结构体）
				return -EFAULT;
			ptr += sizeof(tr); //偏移指针往后偏移
			binder_transaction(proc, thread, &tr,
					   cmd == BC_REPLY, 0);  //我们cmd为BC_TRANSACTION
			break;
		}
		...
		
		*consumed = ptr - buffer; //已写字节数(驱动已处理的)
	}
	return 0;
}
```

`binder_thread_write`中将用户空间的数据拷贝到内核空间，转化为`binder_transaction_data`

#### binder\_transaction

```cpp
static void binder_transaction(struct binder_proc *proc,
			       struct binder_thread *thread,
			       struct binder_transaction_data *tr, int reply,
			       binder_size_t extra_buffers_size)
{
	...
	if (reply) {  //cmd为BC_TRANSACTION, 所以这个reply = false
		...
	} else {
		if (tr->target.handle) {  //注册服务，目标进程是SM，handle值为0
			...
		} else {  //hanedle = 0 的情况
			mutex_lock(&context->context_mgr_node_lock);
			target_node = context->binder_context_mgr_node;  //hanedle = 0 的 target_node
			if (target_node)
				target_node = binder_get_node_refs_for_txn(  //将target_node和binder_proc关联（目标进程为SM进程）
						target_node, &target_proc,
						&return_error);
			else
				return_error = BR_DEAD_REPLY;
			mutex_unlock(&context->context_mgr_node_lock);
			...
		}
		...
		e->to_node = target_node->debug_id;
		...
		binder_inner_proc_lock(proc);
		if (!(tr->flags & TF_ONE_WAY) && thread->transaction_stack) {  //thread->transaction_stack 当前线程的事务栈这里应该为空, 
			...
		}
		binder_inner_proc_unlock(proc);
	}
	if (target_thread)  //我们请求端也没有指定对端的binder thread，所以应该为null， 一般来说服务端进程用那个线程处理是没区别（双向调用为例外）
		e->to_thread = target_thread->pid;
	e->to_proc = target_proc->pid;

	/* TODO: reuse incoming transaction for reply */
	t = kzalloc(sizeof(*t), GFP_KERNEL);  //创建目标进程（服务端）的binder_transaction,这个事务需要往目标进程的某个线程todo队列投递

	binder_stats_created(BINDER_STAT_TRANSACTION);
	spin_lock_init(&t->lock);

	tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);  //创建当前进程（客户端）的binder_work，这个事务需要往当前线程todo队列投递，用于回复本次事务complete

	binder_stats_created(BINDER_STAT_TRANSACTION_COMPLETE);

	t->debug_id = t_debug_id;

	if (!reply && !(tr->flags & TF_ONE_WAY))
		//同步非BC_REPLY请求
		t->from = thread;  //将往目标进程的binder_transaction的from设为当前客户端线程，后续可以根据这个from找到客户端相应线程
	else
		//oneway 方式的异步请求则不需要记录caller thread
		t->from = NULL;
		
	t->sender_euid = task_euid(proc->tsk);
	t->to_proc = target_proc;  //target_proc
	t->to_thread = target_thread;  //target_thread = null
	t->code = tr->code;  //add service action
	t->flags = tr->flags;
	...
	// 从binder_buff查找一块合适的buffer，如果没有就创建申请一块合适的buffer内存，并放入红黑树管理
	t->buffer = binder_alloc_new_buf(&target_proc->alloc, tr->data_size,  
		tr->offsets_size, extra_buffers_size,
		!reply && (t->flags & TF_ONE_WAY));
	t->buffer->debug_id = t->debug_id;
	t->buffer->transaction = t; //binder_transaction和binder_buffer关联
	t->buffer->target_node = target_node; //target_node

	off_start = (binder_size_t *)(t->buffer->data +
				      ALIGN(tr->data_size, sizeof(void *)));  //data中对象的起始地址
	offp = off_start;

	if (copy_from_user(t->buffer->data, (const void __user *)(uintptr_t)
			   tr->data.ptr.buffer, tr->data_size)) {  //拷贝data到binder_transaction中
	}
	if (copy_from_user(offp, (const void __user *)(uintptr_t)
			   tr->data.ptr.offsets, tr->offsets_size)) {  //拷贝对象偏移指针，这个指针是用于找到data的flat对象
	}
	
	off_end = (void *)off_start + tr->offsets_size;
	sg_bufp = (u8 *)(PTR_ALIGN(off_end, sizeof(void *)));
	sg_buf_end = sg_bufp + extra_buffers_size -
		ALIGN(secctx_sz, sizeof(u64));
	off_min = 0;
	for (; offp < off_end; offp++) {
		struct binder_object_header *hdr;
		size_t object_size = binder_validate_object(t->buffer, *offp);

		hdr = (struct binder_object_header *)(t->buffer->data + *offp);
		off_min = *offp + object_size;
		switch (hdr->type) {  //我们注册服务时，writeStrongBinder->flat中为BINDER_TYPE_BINDER，因为我们是要注册服务，传过来的是BBinder
		case BINDER_TYPE_BINDER:
		case BINDER_TYPE_WEAK_BINDER: {
			struct flat_binder_object *fp;

			fp = to_flat_binder_object(hdr);  //拿出一个flat_binder_object对象指针
			ret = binder_translate_binder(fp, t, thread);
			
			...
		} break;
		case BINDER_TYPE_HANDLE:  //如果是调用一般服务进程的方法，传过来的应该是Handle，为BpBinder
		case BINDER_TYPE_WEAK_HANDLE: {
			struct flat_binder_object *fp;

			fp = to_flat_binder_object(hdr);
			ret = binder_translate_handle(fp, t, thread);
			...
		} break;
		...
	}
	tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;  //发给当前进程的Binder_work类型为BINDER_WORK_TRANSACTION_COMPLETE
	t->work.type = BINDER_WORK_TRANSACTION;  //发给目标进程的binder_transaction类型为BINDER_WORK_TRANSACTION

	if (reply) {
		...
	} else if (!(t->flags & TF_ONE_WAY)) {  //同步请求
		BUG_ON(t->buffer->async_transaction != 0);
		binder_inner_proc_lock(proc);

		binder_enqueue_deferred_thread_work_ilocked(thread, tcomplete);  //list_add_tail(&work->entry, &thread->todo);  把tcomplete加入当前线程的todo队列
		t->need_reply = 1;
		t->from_parent = thread->transaction_stack;  //栈更新指向，from_parent指向上一个transaction_stack
		thread->transaction_stack = t;  //当前transaction_stack指向t
		binder_inner_proc_unlock(proc);
		//1. 将binder_transaction加入目标进程的某个线程的todo队列
		//2. 然后目标进程找出一个binder thread来处理
		//3. 接着唤醒这个线程，进入目标进程上下文
		if (!binder_proc_transaction(t, target_proc, target_thread)) {
			binder_inner_proc_lock(proc);
			binder_pop_transaction_ilocked(thread, t);
			binder_inner_proc_unlock(proc);
			goto err_dead_proc_or_thread;
		}
	} else {
		...
	}
	if (target_thread)
		binder_thread_dec_tmpref(target_thread);
	binder_proc_dec_tmpref(target_proc);
	if (target_node)
		binder_dec_node_tmpref(target_node);
	/*
	 * write barrier to synchronize with initialization
	 * of log entry
	 */
	smp_wmb();
	WRITE_ONCE(e->debug_id_done, t_debug_id);
	return;  //写操作到这里处理结束，return后就到处理读操作，然后当前进程进入休眠状态
	...  
}
```

方法很长，主要做了四件事：

1.  找到target\_node、target\_proc
2.  创建binder\_transaction、binder\_work、binder\_buffer
3.  获取flat\_binder\_object中的BBinder，创建binder\_node和binder\_ref，转化hdr->type  
    4.1 将binder\_transaction(BINDER\_WORK\_TRANSACTION)加入目标进程的某个线程的todo队列，然后找到一个空闲的目标进程线程并唤醒来处理这个事务，进入目标进程中  
    4.2 将binder\_work(BINDER\_WORK\_TRANSACTION\_COMPLETE)加入当前线程的todo队列，然后结束写操作的流程，进入读操作，阻塞在读操作中，等待唤醒

#### binder\_translate\_binder

```cpp
static int binder_translate_binder(struct flat_binder_object *fp,
				   struct binder_transaction *t,
				   struct binder_thread *thread)
{
	struct binder_node *node;
	struct binder_proc *proc = thread->proc;
	struct binder_proc *target_proc = t->to_proc;
	struct binder_ref_data rdata;
	int ret = 0;

	node = binder_get_node(proc, fp->binder);  //通过binder指针从binder_node红黑树中查找服务的binder_node，因为服务还没注册，所以这里返回空
	if (!node) {
		node = binder_new_node(proc, fp);  //创建一个binder_node，并加入binder_node红黑树中
		if (!node)
			return -ENOMEM;
	}
	...
	//从binder_ref红黑树中增加对应binder_node的引用
	//如果还没有引用指向这个node，就创建一个ref，并加入binder_ref红黑树中
	//增加node的ref引用
	ret = binder_inc_ref_for_node(target_proc, node,
			fp->hdr.type == BINDER_TYPE_BINDER,
			&thread->todo, &rdata);
	if (ret)
		goto done;

	//我们注册服务，传到驱动的是服务的BBinder对象，
	//但是这个对象跨进程无法使用，对象的地址在其他进程是不可用的，
	//因此我们到其他进程中传递的是handle值
	//
	if (fp->hdr.type == BINDER_TYPE_BINDER)
		fp->hdr.type = BINDER_TYPE_HANDLE;
	else
		fp->hdr.type = BINDER_TYPE_WEAK_HANDLE;
	fp->binder = 0;
	fp->handle = rdata.desc;
	fp->cookie = 0;
	...
}
```

#### binder\_proc\_transaction

```cpp
static bool binder_proc_transaction(struct binder_transaction *t,
				    struct binder_proc *proc,
				    struct binder_thread *thread)
{
	struct binder_node *node = t->buffer->target_node;
	struct binder_priority node_prio;
	bool oneway = !!(t->flags & TF_ONE_WAY);
	bool pending_async = false;

	BUG_ON(!node);
	binder_node_lock(node);
	node_prio.prio = node->min_priority;
	node_prio.sched_policy = node->sched_policy;

	if (oneway) {
		BUG_ON(thread);
		if (node->has_async_transaction) {
			pending_async = true;
		} else {
			node->has_async_transaction = true;
		}
	}

	binder_inner_proc_lock(proc);

	if (!thread && !pending_async)  //target_thread为空
		thread = binder_select_thread_ilocked(proc);  //目标进程中找到一个空闲的线程（如果找到则waiting_threads - 1）

	if (thread) {
		binder_transaction_priority(thread->task, t, node_prio,
					    node->inherit_rt);
		binder_enqueue_thread_work_ilocked(thread, &t->work);  //将binder_work加到找到的线程的todo队列
	} else if (!pending_async) {  //如果找不到能处理的线程，就加到进程的todo队列
		binder_enqueue_work_ilocked(&t->work, &proc->todo);
	} else {  //如果是异步任务就就加到异步todo队列
		binder_enqueue_work_ilocked(&t->work, &node->async_todo);
	}

	if (!pending_async)
		binder_wakeup_thread_ilocked(proc, thread, !oneway /* sync */);  //

	binder_inner_proc_unlock(proc);
	binder_node_unlock(node);

	return true;
}
```

1.  因为当前处于客户端进程中，我们是不需要请求是去指定哪一个目标进程的线程的，因为目标进程使用哪一个线程来处理我们的请求是没有区别的，所以target\_thread为一开始是为空的，但是我们在这里已经构建好了往目标进程处理的binder\_transaction, 这时就需要找出一个空闲的线程来处理即可
2.  将binder\_transaction加入找的线程的todo队列中，如果找不到就加到进程的todo队列
3.  唤醒这个找到的线程（如果没找到合适的线程，就唤醒所有休眠的线程）

#### binder\_select\_thread\_ilocked

```cpp
static struct binder_thread *
binder_select_thread_ilocked(struct binder_proc *proc)
{
	struct binder_thread *thread;

	assert_spin_locked(&proc->inner_lock);
	thread = list_first_entry_or_null(&proc->waiting_threads,
					  struct binder_thread,
					  waiting_thread_node);  //从waiting_threads中找到一个空闲的进程

	if (thread)
		list_del_init(&thread->waiting_thread_node);

	return thread;
}
```

从目标进程`waiting_threads`中找一个空闲的线程（不一定能找到）

#### binder\_enqueue\_thread\_work\_ilocked

```cpp
static void
binder_enqueue_thread_work_ilocked(struct binder_thread *thread,
				   struct binder_work *work)
{
	binder_enqueue_work_ilocked(work, &thread->todo);
	thread->process_todo = true;  //告知线程当前有活干了
}

static void
binder_enqueue_work_ilocked(struct binder_work *work,
			   struct list_head *target_list)
{
	BUG_ON(target_list == NULL);
	BUG_ON(work->entry.next && !list_empty(&work->entry));
	list_add_tail(&work->entry, target_list);  //将binder_work加入上面找到的线程的todo队列
}
```

`binder_enqueue_thread_work_ilocked`的作用就是将binder\_work加入上面找到的线程的todo队列

#### binder\_wakeup\_thread\_ilocked

```cpp
static void binder_wakeup_thread_ilocked(struct binder_proc *proc,
					 struct binder_thread *thread,
					 bool sync)
{
	assert_spin_locked(&proc->inner_lock);

	if (thread) {  //如果找到能够处理的线程则从直接线程的等待队列唤醒这个线程
		if (sync)
			wake_up_interruptible_sync(&thread->wait);
		else
			wake_up_interruptible(&thread->wait);
		return;
	}

	 //如果前面找不到能够处理的线程，有可能是两个原因：
	 // 1. 所有binder thread正在处理transactions事务，没有空闲
	 // 2. 没有空闲的线程，有可能有些线程因为其他的原因（不在处理binder相关的事务）在阻塞等待，例如正在epoll中阻塞
	 //那么我们只能迭代所有线程，把它们都唤醒来干活
	binder_wakeup_poll_threads_ilocked(proc, sync);
}

static void binder_wakeup_poll_threads_ilocked(struct binder_proc *proc,
					       bool sync)
{
	struct rb_node *n;
	struct binder_thread *thread;

	//遍历唤醒所有的线程
	for (n = rb_first(&proc->threads); n != NULL; n = rb_next(n)) {
		thread = rb_entry(n, struct binder_thread, rb_node);
		if (thread->looper & BINDER_LOOPER_STATE_POLL &&
		    binder_available_for_proc_work_ilocked(thread)) {
			if (sync)
				wake_up_interruptible_sync(&thread->wait);
			else
				wake_up_interruptible(&thread->wait);
		}
	}
}
```

`binder_wakeup_thread_ilocked` 主要作用就是唤醒目标进程的线程来处理前面的binder\_transaction 如果前面找不到空闲的进程处理这个事务，那么将会唤醒所有的binder线程。唤醒目标进程的线程后，将会进入目标进程的上下文中，**正式进入目标进程中**。

#### 线程如何进入等待状态

在进入目标进程前，我们回顾下服务进程是如何进入挂起状态的，从前面分析可知，`ServiceManager`服务端进程启动后进行的一些列的操作，最后阻塞在驱动的`binder_thread_read`中

```cpp
static int binder_thread_read(struct binder_proc *proc,
			      struct binder_thread *thread,
			      binder_uintptr_t binder_buffer, size_t size,
			      binder_size_t *consumed, int non_block)
{
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

	int ret = 0;
	int wait_for_proc_work;

	if (*consumed == 0) {    //consumed = 0就说明客户端进程还没处理完输入缓冲区的内容，返回个BR_NOOP到用户进程
		if (put_user(BR_NOOP, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
	}

retry:
	binder_inner_proc_lock(proc);
	wait_for_proc_work = binder_available_for_proc_work_ilocked(thread);    //是否当前线程正在等待进程来活干，因为这时候客户端还没有往这个进程的线程todo工作队列投递事务，且线程的事务栈也是空的，所以这里为true
	binder_inner_proc_unlock(proc);

	thread->looper |= BINDER_LOOPER_STATE_WAITING;

	trace_binder_wait_for_work(wait_for_proc_work,
				   !!thread->transaction_stack,
				   !binder_worklist_empty(proc, &thread->todo));
	if (wait_for_proc_work) {
		if (!(thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
			...
		}
		binder_restore_priority(current, proc->default_priority);
	}

	if (non_block) {
		if (!binder_has_work(thread, wait_for_proc_work))
			ret = -EAGAIN;
	} else { //阻塞模式打开binder驱动
		ret = binder_wait_for_work(thread, wait_for_proc_work);  //让当前线程在它的等待队列中休眠（挂起让出CPU）
	}

	...
		}
	...
	//下面的都是唤醒后的操作了
}
```

1.  检测下当前线程是否有活干
2.  进入睡眠状态等待唤醒

#### binder\_available\_for\_proc\_work\_ilocked

```cpp
static bool binder_available_for_proc_work_ilocked(struct binder_thread *thread)
{
	return !thread->transaction_stack &&
		binder_worklist_empty_ilocked(&thread->todo) &&
		(thread->looper & (BINDER_LOOPER_STATE_ENTERED |
				   BINDER_LOOPER_STATE_REGISTERED));
}
```

是否当前线程正在等待进程来活干，因为这时候客户端还没有往这个进程的线程todo工作队列投递事务，且线程的事务栈也是空的，所以这里返回true

#### binder\_wait\_for\_work

```cpp
static int binder_wait_for_work(struct binder_thread *thread,
				bool do_proc_work)
{
	DEFINE_WAIT(wait);
	struct binder_proc *proc = thread->proc;
	int ret = 0;

	freezer_do_not_count();
	binder_inner_proc_lock(proc);
	for (;;) {
		prepare_to_wait(&thread->wait, &wait, TASK_INTERRUPTIBLE);  //将线程加入等待队列，并更改当前线程的状态
		if (binder_has_work_ilocked(thread, do_proc_work)) //退出睡眠的条件，被唤醒也要符合条件才能退出循环，否则再次进入睡眠
			break;  //如果线程工作队列上有活干了，就退出循环，处理todo队列上的事务
		if (do_proc_work)
			list_add(&thread->waiting_thread_node,
				 &proc->waiting_threads);  //如果线程空闲，就将线程加入waiting_threads
		binder_inner_proc_unlock(proc);
		schedule();  //让出调度，进入睡眠，线程从此等待被唤醒
		binder_inner_proc_lock(proc);
		list_del_init(&thread->waiting_thread_node);
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
	}
	finish_wait(&thread->wait, &wait);  //将线程等待队列中删除,更改当前线程的状态，将当前线程置于TASK_RUNNING状态
	binder_inner_proc_unlock(proc);
	freezer_count();

	return ret;
}
```

1.  `prepare_to_wait`将线程加入等待队列，并更改当前线程的状态为`TASK_INTERRUPTIBLE`, 然后判断是否符合退出睡眠的状态，首次进入也要先判断，避免错过刚好符合退出条件而进入睡眠，当被唤醒时也要判断是否符合退出睡眠条件，否则再次进入睡眠
2.  schedule() 让出调度权，意味着失去调度权，进入睡眠，线程从此等待被从等待队列上唤醒
3.  当退出睡眠的状态，把线程从等待队列移除，并设置它的状态为`TASK_RUNNING`

#### 目标进程被唤醒

从前面分析可知 ，客户端进程中，binder驱动`中的binder_wakeup_thread_ilocked` 会唤醒目标进程的线程来处理投递的binder\_transaction，唤醒目标进程的线程后，将会进入目标进程的上下文中，**进入目标进程中**。上面我们也知道唤醒是在`binder_wait_for_work`中出来

```cpp
static int binder_thread_read(struct binder_proc *proc,
			      struct binder_thread *thread,
			      binder_uintptr_t binder_buffer, size_t size,
			      binder_size_t *consumed, int non_block)
{
	...
retry:
	...

	if (non_block) {
		if (!binder_has_work(thread, wait_for_proc_work))
			ret = -EAGAIN;
	} else {
		ret = binder_wait_for_work(thread, wait_for_proc_work);
	}

	thread->looper &= ~BINDER_LOOPER_STATE_WAITING;

	while (1) {
		uint32_t cmd;
		struct binder_transaction_data_secctx tr;
		struct binder_transaction_data *trd = &tr.transaction_data;
		struct binder_work *w = NULL;
		struct list_head *list = NULL;
		struct binder_transaction *t = NULL;
		struct binder_thread *t_from;
		size_t trsize = sizeof(*trd);

		binder_inner_proc_lock(proc);
		if (!binder_worklist_empty_ilocked(&thread->todo))  //如果线程todo不为空，binder_work就从线程todo拿
			list = &thread->todo;
		else if (!binder_worklist_empty_ilocked(&proc->todo) &&
			   wait_for_proc_work)  //如果线程todo为空且线程的事务栈也为空，binder_work就从进程todo拿
			list = &proc->todo;
		else {
			binder_inner_proc_unlock(proc);
		...
		w = binder_dequeue_work_head_ilocked(list);  //拿出binder_work, 并从todo队列中删除这个work
		if (binder_worklist_empty_ilocked(&thread->todo))
			thread->process_todo = false;  //如果线程的todo队列为空了，标记无事可干了，空闲

		switch (w->type) {
		case BINDER_WORK_TRANSACTION: { //根据前面投递binder_work类型为BINDER_WORK_TRANSACTION
			binder_inner_proc_unlock(proc);
			t = container_of(w, struct binder_transaction, work);  //根据这个work返回binder_transaction指针
		} break;
		...
		
		BUG_ON(t->buffer == NULL);
		if (t->buffer->target_node) {
			struct binder_node *target_node = t->buffer->target_node;  //从binder_buffer拿出target_node
			struct binder_priority node_prio;

			trd->target.ptr = target_node->ptr;  //数据指针
			trd->cookie =  target_node->cookie;  //cookie附加数据
			node_prio.sched_policy = target_node->sched_policy;
			node_prio.prio = target_node->min_priority;
			binder_transaction_priority(current, t, node_prio,
						    target_node->inherit_rt);
			cmd = BR_TRANSACTION;  //cmd -> BR_TRANSACTION
		} else {
			trd->target.ptr = 0;
			trd->cookie = 0;
			cmd = BR_REPLY;
		}
		trd->code = t->code;  //ADD SERVICE
		trd->flags = t->flags;
		trd->sender_euid = from_kuid(current_user_ns(), t->sender_euid);

		t_from = binder_get_txn_from(t);  //t_from就是客户端请求thread
		if (t_from) {
			struct task_struct *sender = t_from->proc->tsk;  //客户端进程的task_struct

			trd->sender_pid =
				task_tgid_nr_ns(sender,
						task_active_pid_ns(current)); //发起请求的客户端进程pid
		} else {
			trd->sender_pid = 0;
		}

		trd->data_size = t->buffer->data_size;  //data_size
		trd->offsets_size = t->buffer->offsets_size;  //offsets_size
		trd->data.ptr.buffer = (binder_uintptr_t)
			((uintptr_t)t->buffer->data +
			binder_alloc_get_user_buffer_offset(&proc->alloc));  //ptr.buffer
		trd->data.ptr.offsets = trd->data.ptr.buffer +
					ALIGN(t->buffer->data_size,
					    sizeof(void *));  //ptr.offsets

		tr.secctx = t->security_ctx;

		if (put_user(cmd, (uint32_t __user *)ptr)) {  //往用户空间输入缓冲区写入cmd头（内存最前面就是cmd）
			...
		}
		ptr += sizeof(uint32_t);  // 写完了cmd就ptr指针偏移cmd的大小
		if (copy_to_user(ptr, &tr, trsize)) {  //接着写binder_transaction_data 数据格式，整个ptr指针指向的内存空间就是cmd+binder_transaction_data
			...
		}
		ptr += trsize; //ptr指针更新偏移

		t->buffer->allow_user_free = 1;
		if (cmd != BR_REPLY && !(t->flags & TF_ONE_WAY)) {
			binder_inner_proc_lock(thread->proc);
			//设置这个客户端投递过来的binder_transaction的to_parent为当前事务
			t->to_parent = thread->transaction_stack; 
			//在客户端请求线程时，我们target thread还不知道具体是哪个thread，所以设置为null, 现在在目标进程真正处理的线程，可以知道了具体thread
			t->to_thread = thread; 
			//更新事务栈的指向
			thread->transaction_stack = t;
			binder_inner_proc_unlock(thread->proc);
		} else {
			binder_free_transaction(t);
		}
		break;
	}
	}

done:

	*consumed = ptr - buffer;  //更新consumed字节数
	binder_inner_proc_lock(proc);
	//上面的数据完成写入用户空间后，检查下当前是否需要请求用户进程创建新的binder工作线程(记得上面Switch还有其他case)
	//如果当前需要用户进程创建binder工作线程则向用户进程请求BR_SPAWN_LOOPER，条件：
	//requested_threads = 0  请求线程处理的数量为0
	//waiting_threads 空闲线程为0
	//requested_threads_started 已经启动的线程数小于用户进程允许的最大数量
	//当前进程设置了BINDER_LOOPER_STATE_REGISTERED和BINDER_LOOPER_STATE_ENTERED
	if (proc->requested_threads == 0 &&
	    list_empty(&thread->proc->waiting_threads) &&
	    proc->requested_threads_started < proc->max_threads &&
	    (thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
	     BINDER_LOOPER_STATE_ENTERED)) /* the user-space code fails to */
	     /*spawn a new thread if we leave this out */) {
		proc->requested_threads++;
		binder_inner_proc_unlock(proc);
		binder_debug(BINDER_DEBUG_THREADS,
			     "%d:%d BR_SPAWN_LOOPER\n",
			     proc->pid, thread->pid);
		if (put_user(BR_SPAWN_LOOPER, (uint32_t __user *)buffer))  // 向用户空间发一条BR_SPAWN_LOOPER命令
			return -EFAULT;
		binder_stat_br(proc, thread, BR_SPAWN_LOOPER);
	} else
		binder_inner_proc_unlock(proc);
	return 0;  //执行完，一级一级return，最终就会从binder_ioctl返回用户空间中处理
}
```

方法很长，我们注册服务为目的，唤醒SM服务进程后（在具体线程中），主要在做的事情：

1.  从线程`todo工作队列`中获取客户端进程投递的`binder_transaction`, 并根据它创建`binder_transaction_data`
2.  设置cmd = BR\_TRANSACTION，将cmd + binder\_transaction\_data 写入SM进程用户空间输入缓冲区
3.  然后一级一级return回到SM用户空间(从binder\_ioctl返回)，触发驱动fd事件

### 五、 ServiceManager服务端的处理addService操作

前面从驱动到用户空间ioctl中返回，进入ServiceManager服务端用户空间的逻辑处理。从Binder系列–ServiceManager的启动，我们知道ServiceManager进程启动时在`main方法`中最后是用`Looper`方式监听驱动的fd事件，当驱动有事件时，回调`handleEvent`方法  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3ee5f9da5f408d6985ebd676473900f8.png#pic_center)

```cpp
class BinderCallback : public LooperCallback {
public:
    static sp<BinderCallback> setupTo(const sp<Looper>& looper) {
        sp<BinderCallback> cb = sp<BinderCallback>::make();

        int binder_fd = -1;
        IPCThreadState::self()->setupPolling(&binder_fd);
        LOG_ALWAYS_FATAL_IF(binder_fd < 0, "Failed to setupPolling: %d", binder_fd);

        int ret = looper->addFd(binder_fd,
                                Looper::POLL_CALLBACK,
                                Looper::EVENT_INPUT,
                                cb,
                                nullptr /*data*/);
        LOG_ALWAYS_FATAL_IF(ret != 1, "Failed to add binder FD to Looper");

        return cb;
    }

    int handleEvent(int /* fd */, int /* events */, void* /* data */) override {
        IPCThreadState::self()->handlePolledCommands();
        return 1;  // Continue receiving callbacks.
    }
};
```

然后调用`IPCThreadState`的`handlePolledCommands`处理

#### handlePolledCommands

frameworks/native/libs/binder/IPCThreadState.cpp

```cpp
status_t IPCThreadState::handlePolledCommands()
{
    status_t result;

    do {
        result = getAndExecuteCommand();
    } while (mIn.dataPosition() < mIn.dataSize());

    processPendingDerefs();
    flushCommands();
    return result;
}
```

#### getAndExecuteCommand

```cpp
status_t IPCThreadState::getAndExecuteCommand()
{
    status_t result;
    int32_t cmd;

    result = talkWithDriver();
    if (result >= NO_ERROR) {
        size_t IN = mIn.dataAvail();
        cmd = mIn.readInt32();  //从mIn中读取cmd 协议操作code->BR_TRANSACTION
        IF_LOG_COMMANDS() {
            alog << "Processing top-level Command: "
                 << getReturnString(cmd) << endl;
        }

        pthread_mutex_lock(&mProcess->mThreadCountLock);
        mProcess->mExecutingThreadsCount++;  //请求用于处理命令的线程数 + 1（在干活的线程数+1）
        if (mProcess->mExecutingThreadsCount >= mProcess->mMaxThreads &&
                mProcess->mStarvationStartTimeMs == 0) {  //如果需要干活的线程数大于最大线程数且线程池枯竭时间为0（刚好用完线程情况）
            mProcess->mStarvationStartTimeMs = uptimeMillis(); //开始记录池子用完持续时间
        }
        pthread_mutex_unlock(&mProcess->mThreadCountLock);

        result = executeCommand(cmd);  //处理事务

        pthread_mutex_lock(&mProcess->mThreadCountLock);
        mProcess->mExecutingThreadsCount--;  //请求干活的线程数 - 1
        if (mProcess->mExecutingThreadsCount < mProcess->mMaxThreads &&
                mProcess->mStarvationStartTimeMs != 0) {  //请求用于处理命令线程大于最大线程数了，且还在记录线程池枯竭时间
            int64_t starvationTimeMs = uptimeMillis() - mProcess->mStarvationStartTimeMs;
            if (starvationTimeMs > 100) {
                ALOGE("binder thread pool (%zu threads) starved for %" PRId64 " ms",
                      mProcess->mMaxThreads, starvationTimeMs);  //来个骚打印，让性能的老哥紧张下
            }
            mProcess->mStarvationStartTimeMs = 0;  //重置下时间（有空闲线程了，池子应该是非枯竭了）
        }

        // Cond broadcast can be expensive, so don't send it every time a binder
        // call is processed. b/168806193
        if (mProcess->mWaitingForThreads > 0) {  //如果当前mWaitingForThreads > 0（因为没线程而有事务没处理的计数）> 0
            pthread_cond_broadcast(&mProcess->mThreadCountDecrement);  // 唤醒blockUntilThreadAvailable状态中等待的线程
        }
        pthread_mutex_unlock(&mProcess->mThreadCountLock);
    }

    return result;
}
```

#### executeCommand

```cpp
status_t IPCThreadState::executeCommand(int32_t cmd)
{
    BBinder* obj;
    RefBase::weakref_type* refs;
    status_t result = NO_ERROR;

    switch ((uint32_t)cmd) {
	...
    case BR_TRANSACTION:  //处理驱动发过来的cmd = BR_TRANSACTION请求
        {
            binder_transaction_data_secctx tr_secctx;
            binder_transaction_data& tr = tr_secctx.transaction_data;

            if (cmd == (int) BR_TRANSACTION_SEC_CTX) {
                result = mIn.read(&tr_secctx, sizeof(tr_secctx));
            } else {  //BR_TRANSACTION
                result = mIn.read(&tr, sizeof(tr));
                tr_secctx.secctx = 0;
            }
			...

            Parcel buffer;
			....

            Parcel reply;
            status_t error;

            if (tr.target.ptr) {  //发起注册服务的客户端的writeTransactionData写入的是0
				...
            } else {  //走这里
                error = the_context_object->transact(tr.code, buffer, &reply, tr.flags);  //使用SM进程启动时缓存的BBinder
            }

            if ((tr.flags & TF_ONE_WAY) == 0) {  //同步请求需要回复reply
				...
                sendReply(reply, (tr.flags & kForwardReplyFlags));
            } else {  //异步请求不需要应答
				//oneway
            }
			...
        }
        break;
	...
...
    return result;
}
```

调用 `BBinder::transact`处理，这个`BBinder`就是SM进程启动时通过`IPCThreadState::setTheContextObject`保存的

#### BBinder::transact

frameworks/native/libs/binder/Binder.cpp

```cpp
status_t BBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    data.setDataPosition(0);

    if (reply != nullptr && (flags & FLAG_CLEAR_BUF)) {
        reply->markSensitive();
    }

    status_t err = NO_ERROR;
    switch (code) {
        case PING_TRANSACTION:  //处理客户端进程首次获取SM BpBinder时发起的ping检测
		...
        case EXTENSION_TRANSACTION:
		...
        case DEBUG_PID_TRANSACTION:
		...
        case SET_RPC_CLIENT_TRANSACTION: {
		...
        default:
            err = onTransact(code, data, reply, flags);  //调用BnServiceManager实现类的onTransact
            break;
    }
	...
    return err;
}
```

调用`BnServiceManager`实现类的`onTransact`

#### BnServiceManager::onTransact

这是AIDL生成的类，BnXXX模版中间类  
out/soong/.intermediates/frameworks/native/libs/binder/libbinder/android\_x86\_64\_shared/gen/aidl/android/os/IServiceManager.cpp

```cpp
::android::status_t BnServiceManager::onTransact(uint32_t _aidl_code, const ::android::Parcel& _aidl_data, ::android::Parcel* _aidl_reply, uint32_t _aidl_flags) {
  ::android::status_t _aidl_ret_status = ::android::OK;
  switch (_aidl_code) {
    case ...
    
    case BnServiceManager::TRANSACTION_addService:
  {
    ::std::string in_name;
    ::android::sp<::android::IBinder> in_service;
    bool in_allowIsolated;
    int32_t in_dumpPriority;

    _aidl_ret_status = _aidl_data.readUtf8FromUtf16(&in_name);  //服务名

    _aidl_ret_status = _aidl_data.readStrongBinder(&in_service);  //服务Biner

    _aidl_ret_status = _aidl_data.readBool(&in_allowIsolated); 

    _aidl_ret_status = _aidl_data.readInt32(&in_dumpPriority);

    ::android::binder::Status _aidl_status(addService(in_name, in_service, in_allowIsolated, in_dumpPriority));  //调用服务ServiceManager真正的实现
    _aidl_ret_status = _aidl_status.writeToParcel(_aidl_reply);	
  }
  break;
```

调用服务ServiceManager真正的实现处理  
frameworks/native/cmds/servicemanager/ServiceManager.cpp

```cpp
Status ServiceManager::addService(const std::string& name, const sp<IBinder>& binder, bool allowIsolated, int32_t dumpPriority) {
    auto ctx = mAccess->getCallingContext();

    if (multiuser_get_app_id(ctx.uid) >= AID_APP) {  //uid 鉴权
        return Status::fromExceptionCode(Status::EX_SECURITY, "App UIDs cannot add services");
    }

    if (!mAccess->canAdd(ctx, name)) {  // selinux 鉴权
        return Status::fromExceptionCode(Status::EX_SECURITY, "SELinux denial");
    }

    if (binder == nullptr) {  //binder 非空检测
        return Status::fromExceptionCode(Status::EX_ILLEGAL_ARGUMENT, "Null binder");
    }

    if (!isValidServiceName(name)) {  //名字非法检测
        LOG(ERROR) << "Invalid service name: " << name;
        return Status::fromExceptionCode(Status::EX_ILLEGAL_ARGUMENT, "Invalid service name");
    }

#ifndef VENDORSERVICEMANAGER  //如果是hwservicemanager 还要检测VINTF declaration 
    if (!meetsDeclarationRequirements(binder, name)) {
        // already logged
        return Status::fromExceptionCode(Status::EX_ILLEGAL_ARGUMENT, "VINTF declaration error");
    }
#endif  // !VENDORSERVICEMANAGER

    // implicitly unlinked when the binder is removed
    if (binder->remoteBinder() != nullptr &&
        binder->linkToDeath(sp<ServiceManager>::fromExisting(this)) != OK) {  //add service时binder->remoteBinder() == nullptr
        LOG(ERROR) << "Could not linkToDeath when adding " << name;
        return Status::fromExceptionCode(Status::EX_ILLEGAL_STATE, "linkToDeath failure");
    }

    // Overwrite the old service if it exists
    mNameToService[name] = Service {  //添加一个<name, Service>到服务map容器中，这个就是管理所有注册的容器
        .binder = binder,  //这个是一个由注册服务的进程真正的BBinder转化BpBinder
        .allowIsolated = allowIsolated,
        .dumpPriority = dumpPriority,
        .debugPid = ctx.debugPid,
    };

    auto it = mNameToRegistrationCallback.find(name);
    if (it != mNameToRegistrationCallback.end()) {  //客户端对服务当前的客户端数的回调监听
        for (const sp<IServiceCallback>& cb : it->second) {
            mNameToService[name].guaranteeClient = true;
            // permission checked in registerForNotifications
            cb->onRegistration(name, binder);
        }
    }

    return Status::ok();
}
```

到这里服务的注册就完成了，过程中驱动部分还有很多的不了解，这个还会不断完善，最后看下一个完整的请求, 引用一个大佬的图吧  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b5389d0212ba3e6f68e447dae447178f.png#pic_center)