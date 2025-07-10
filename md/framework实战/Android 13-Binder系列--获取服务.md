> 获取服务  
> hongxi.zhu 2023-7-8

### 一、客户端发起获取服务

以`SurfaceFlinger`进程中获取`InputFlinger`服务为例  
frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp

```cpp
    sp<IBinder> input(defaultServiceManager()->getService(String16("inputflinger")));

    static_cast<void>(mScheduler->schedule([=] {
        if (input == nullptr) {
            ALOGE("Failed to link to input service");
        } else {
            mInputFlinger = interface_cast<os::IInputFlinger>(input);
        }
```

### 二、获取ServiceManager

获取到SM的代理对象之前的文章已经分析过，请参考前面的：Binder系列–获取ServiceManager

### 三、向ServiceManager获取服务

#### BpServiceManager::getService

```cpp
::android::binder::Status BpServiceManager::getService(const ::std::string& name, ::android::sp<::android::IBinder>* _aidl_return) {
  ::android::Parcel _aidl_data;
  _aidl_data.markForBinder(remoteStrong());
  ::android::Parcel _aidl_reply;
  ::android::status_t _aidl_ret_status = ::android::OK;
  ::android::binder::Status _aidl_status;
  _aidl_ret_status = _aidl_data.writeInterfaceToken(getInterfaceDescriptor());  //服务接口描述

  _aidl_ret_status = _aidl_data.writeUtf8AsUtf16(name);  //服务名称

  _aidl_ret_status = remote()->transact(BnServiceManager::TRANSACTION_getService, _aidl_data, &_aidl_reply, 0);
	//reply
  _aidl_ret_status = _aidl_status.readFromParcel(_aidl_reply);
	//return the service bpbinder
  _aidl_ret_status = _aidl_reply.readNullableStrongBinder(_aidl_return);
	...
  return _aidl_status;
}
```

remote()从前面可知是SM的BpBinder对象，获取到SM的BpBinder对象后，调用它的`getService`方法发起跨进程请求，向`ServiceManager`进程获取我们的目标服务

#### BpBinder::transact

```cpp
// NOLINTNEXTLINE(google-default-arguments)
status_t BpBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    // Once a binder has died, it will never come back to life.
    if (mAlive) {
        bool privateVendor = flags & FLAG_PRIVATE_VENDOR;
        // don't send userspace flags to the kernel
        flags = flags & ~FLAG_PRIVATE_VENDOR;
        ...
        status_t status;
        if (CC_UNLIKELY(isRpcBinder())) { //RpcBinder(socket)
			...
        } else {  //traditional binder
            status = IPCThreadState::self()->transact(binderHandle(), code, data, reply, flags);  //调用ipcthreadstate的transact向驱动交互, binderHandle() = 0
        }
		...
        return status;
    }

    return DEAD_OBJECT;
}
```

调用ipcthreadstate的transact向驱动交互

#### IPCThreadState::transact

```cpp
status_t IPCThreadState::transact(int32_t handle,
                                  uint32_t code, const Parcel& data,
                                  Parcel* reply, uint32_t flags)
{
    LOG_ALWAYS_FATAL_IF(data.isForRpc(), "Parcel constructed for RPC, but being used with binder.");

    status_t err;

    flags |= TF_ACCEPT_FDS;
	
    err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, nullptr);  //将数据写入mOut缓冲区，交给驱动读取 cmd = BC_TRANSACTION

    if ((flags & TF_ONE_WAY) == 0) {  //同步请求
    	...
        if (reply) {
            err = waitForResponse(reply);  //向驱动写并等待响应
        } else {
            Parcel fakeReply;
            err = waitForResponse(&fakeReply);
        }

    } else {
        err = waitForResponse(nullptr, nullptr);  //oneway的方式不等待驱动的响应直接返回
    }

    return err;
}
```

1.  通过`writeTransactionData`，构建`binder_transaction_data`结构体，并写入`mOut`缓冲区
2.  向驱动读写并阻塞等待驱动返回

#### IPCThreadState::writeTransactionData

```cpp
status_t IPCThreadState::writeTransactionData(int32_t cmd, uint32_t binderFlags,
    int32_t handle, uint32_t code, const Parcel& data, status_t* statusBuffer)
{
    binder_transaction_data tr;

    tr.target.ptr = 0; /* Don't pass uninitialized stack data to a remote process */
    tr.target.handle = handle;
    tr.code = code;
    tr.flags = binderFlags;
    tr.cookie = 0;
    tr.sender_pid = 0;
    tr.sender_euid = 0;

    const status_t err = data.errorCheck();
    if (err == NO_ERROR) {
        tr.data_size = data.ipcDataSize();  //跨进程传输的数据的实际大小
        tr.data.ptr.buffer = data.ipcData();  //跨进程的数据地址
        tr.offsets_size = data.ipcObjectsCount()*sizeof(binder_size_t);  //data中包含的对象大小
        tr.data.ptr.offsets = data.ipcObjects();  //对象的偏移地址
    } else if (statusBuffer) { //statusBuffer = nullptr
		//
    } else {
        return (mLastError = err);
    }

    mOut.writeInt32(cmd);  //cmd = BC_TRANSACTION
    mOut.write(&tr, sizeof(tr));  //将tr结构体写入（tr结构体是驱动和用户进程共同使用的数据格式）

    return NO_ERROR;
}
```

#### IPCThreadState::waitForResponse

```cpp
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    uint32_t cmd;
    int32_t err;

    while (1) {
        if ((err=talkWithDriver()) < NO_ERROR) break;  //向驱动读写数据
        
		//从驱动中返回，读取驱动写给用户进程的数据
		
        cmd = (uint32_t)mIn.readInt32();  //获取协议请求码cmd

        switch (cmd) {
		...
        case BR_REPLY:  //处理驱动给用户进程的BR_REPLY
            {
                binder_transaction_data tr;
                err = mIn.read(&tr, sizeof(tr));
                ALOG_ASSERT(err == NO_ERROR, "Not enough command data for brREPLY");
                if (err != NO_ERROR) goto finish;

                if (reply) {
                    if ((tr.flags & TF_STATUS_CODE) == 0) {
                        reply->ipcSetDataReference(
                            reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                            tr.data_size,
                            reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                            tr.offsets_size/sizeof(binder_size_t),
                            freeBuffer);
                    } else {
                        err = *reinterpret_cast<const status_t*>(tr.data.ptr.buffer);
                        freeBuffer(nullptr,
                            reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                            tr.data_size,
                            reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                            tr.offsets_size/sizeof(binder_size_t));
                    }
                } else {
                    freeBuffer(nullptr,
                        reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                        tr.data_size,
                        reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                        tr.offsets_size/sizeof(binder_size_t));
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
	...
    return err;
}
```

IPCThreadState::talkWithDriver()

```cpp
status_t IPCThreadState::talkWithDriver(bool doReceive)  //doReceive默认为true
{

    binder_write_read bwr;

    // Is the read buffer empty?
    //用户进程已经处理完上一次读取发过来的数据
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();

    // We don't want to write anything if we are still reading
    // from data left in the input buffer and the caller
    // has requested to read the next data.
    //只有当用户进程处理完上一次驱动发过来的数据才会去进行下一次向驱动写操作，否则mOut.dataSize = 0
    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;

    bwr.write_size = outAvail;
    bwr.write_buffer = (uintptr_t)mOut.data();

	//如果需要读，bwr.read_size = mIn.dataCapacity()，  bwr.write_size = 0
	//如果需要写， bwr.read_size = 0，bwr.write_size = mOut.dataSize()
	//两者都为0，则不读不写，直接返回
    // This is what we'll read.
    if (doReceive && needRead) {
        bwr.read_size = mIn.dataCapacity();
        bwr.read_buffer = (uintptr_t)mIn.data();
    } else {
        bwr.read_size = 0;
        bwr.read_buffer = 0;
    }

    }

    // Return immediately if there is nothing to do.
    if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;

    bwr.write_consumed = 0; //驱动已经处理的写操作数据字节数
    bwr.read_consumed = 0;  //驱动已经处理的读操作数据字节数
    status_t err;
    do {
		...
	  	// 通过ioctl向驱动读、写数据
		// ioctl cmd = BINDER_WRITE_READ
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
        else
            err = -errno;
		...
    } while (err == -EINTR);


    if (err >= NO_ERROR) {
        if (bwr.write_consumed > 0) {
            if (bwr.write_consumed < mOut.dataSize())
				...
            else {   //驱动已经处理完我们写入的数据，可以将输出缓冲区重置
                mOut.setDataSize(0);
                processPostWriteDerefs();
            }
        }
        if (bwr.read_consumed > 0) {
          	//从驱动读到数据，设置本次需要用户进程处理的字节数为bwr.read_consumed
            mIn.setDataSize(bwr.read_consumed);
            //指针从头开始读
            mIn.setDataPosition(0);
        }
        return NO_ERROR;
    }

    return err;
}
```

上面主要是

1.  构建`binder_write_read`结构体`bwr`, 这个结构体是用户进程和驱动交互的数据格式  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/e0f97e7a06bd2b7cbde52c660b6d97ec.png#pic_center)
2.  通过`ioctl`向驱动写入`bwr`

### 三、驱动部分

用户空间调用`ioctl`系统调用时会调到具体模块的实现中，也就是binder驱动的`binder_ioctl`中, 此时仍处于请求端用户进程上下文

#### BC\_TRANSACTION

#### binder\_ioctl

```cpp
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct binder_proc *proc = filp->private_data;
	struct binder_thread *thread;
	unsigned int size = _IOC_SIZE(cmd);  //获取ioctl cmd 
	void __user *ubuf = (void __user *)arg;

	thread = binder_get_thread(proc);  //获取当前请求端进程
	
	switch (cmd) {
	case BINDER_WRITE_READ:  //用户进程最主要的行为就是读写操作
		ret = binder_ioctl_write_read(filp, cmd, arg, thread);
		if (ret)
			goto err;
		break;
	...
	}
	ret = 0;
err:
	...
}
```

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
	
	//从用户空间拷贝bwr结构体数据
	if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {
		ret = -EFAULT;
		goto out;
	}

	if (bwr.write_size > 0) {  //要写
		ret = binder_thread_write(proc, thread,
					  bwr.write_buffer,
					  bwr.write_size,
					  &bwr.write_consumed);
	}
	if (bwr.read_size > 0) {  //要读
		ret = binder_thread_read(proc, thread, bwr.read_buffer,
					 bwr.read_size,
					 &bwr.read_consumed,
					 filp->f_flags & O_NONBLOCK);
		trace_binder_read_done(ret);
		binder_inner_proc_lock(proc);
		if (!binder_worklist_empty_ilocked(&proc->todo))
			binder_wakeup_proc_ilocked(proc);
		binder_inner_proc_unlock(proc);
	}

	if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {
		ret = -EFAULT;
		goto out;
	}
out:
	return ret;
}
```

将用户进程的数据拷贝到内核空间，并调用`binder_thread_write`处理

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

		  //从用户空间缓冲区拷贝cmd请求码到内核空间
		  //这个ptr就是指向用户空间那块数据内存的
		if (get_user(cmd, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t); //上面拷贝了cmd，数据指针起始地址要往后偏移，一个cmd大小，就能接着往后读数据

		switch (cmd) {
		...
		case BC_TRANSACTION:  //BC_TRANSACTION
		case BC_REPLY: {
			struct binder_transaction_data tr;

			//从用户空间拷贝binder_transaction_data结构体到内核空间
			//bwr = cmd + tr,因为上面更新了偏移地址，所以这里直接拷贝即可
			if (copy_from_user(&tr, ptr, sizeof(tr)))  
				return -EFAULT;
			ptr += sizeof(tr);  //更新ptr数据起始地址
			binder_transaction(proc, thread, &tr,
					   cmd == BC_REPLY, 0);
			break;
		}
		...
		*consumed = ptr - buffer;
	}
	return 0;
}
```

从用户空间拷贝`binder_transaction_data`到内核空间，然后调用`binder_transaction` 处理

#### binder\_transaction

```cpp
static void binder_transaction(struct binder_proc *proc,
			       struct binder_thread *thread,
			       struct binder_transaction_data *tr, int reply,
			       binder_size_t extra_buffers_size)
{
	int ret;
	struct binder_transaction *t;
	struct binder_work *tcomplete;
	binder_size_t *offp, *off_end, *off_start;
	binder_size_t off_min;
	u8 *sg_bufp, *sg_buf_end;
	struct binder_proc *target_proc = NULL;
	struct binder_thread *target_thread = NULL;
	struct binder_node *target_node = NULL;
	struct binder_transaction *in_reply_to = NULL;
	struct binder_buffer_object *last_fixup_obj = NULL;
	binder_size_t last_fixup_min_off = 0;
	struct binder_context *context = proc->context;
	int t_debug_id = atomic_inc_return(&binder_last_id);
	char *secctx = NULL;
	u32 secctx_sz = 0;

	if (reply) {  //BC_REPLY
		...
	} else {  //BC_TRANSACTION
		if (tr->target.handle) { 
			...
		} else {  //tr->target.handle = 0
			mutex_lock(&context->context_mgr_node_lock);
			target_node = context->binder_context_mgr_node;  //SM服务binder_node
			if (target_node)
				target_node = binder_get_node_refs_for_txn(
						target_node, &target_proc,
						&return_error);
			else
				return_error = BR_DEAD_REPLY;
			mutex_unlock(&context->context_mgr_node_lock);
		}

		e->to_node = target_node->debug_id;

		binder_inner_proc_lock(proc);
		if (!(tr->flags & TF_ONE_WAY) && thread->transaction_stack) {
			...
		}
		binder_inner_proc_unlock(proc);
	}

	/* TODO: reuse incoming transaction for reply */
	t = kzalloc(sizeof(*t), GFP_KERNEL);  //binder_transaction
	
	spin_lock_init(&t->lock);

	tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);  //binder_work

	//往目前进程的是binder_transaction，往当前线程的是binder_work
	//binder_transaction包含binder_work、binder_buffer等
	t->debug_id = t_debug_id;

	if (!reply && !(tr->flags & TF_ONE_WAY))
		t->from = thread;  //同步请求时记录下当前客户端的请求线程
	else
		t->from = NULL;   //oneway的方式不需要记录
		
	t->sender_euid = task_euid(proc->tsk);
	t->to_proc = target_proc;  //SM进程
	t->to_thread = target_thread;  //target_thread没有指定，为null
	t->code = tr->code;  //getService
	t->flags = tr->flags;
	...
	t->buffer = binder_alloc_new_buf(&target_proc->alloc, tr->data_size,
		tr->offsets_size, extra_buffers_size,
		!reply && (t->flags & TF_ONE_WAY));  //从buffer红黑树上查询合适的buffer，找不到就创建并加入红黑树
	...
	t->buffer->debug_id = t->debug_id;
	t->buffer->transaction = t;
	t->buffer->target_node = target_node;

	off_start = (binder_size_t *)(t->buffer->data +
				      ALIGN(tr->data_size, sizeof(void *)));
	offp = off_start;

	if (copy_from_user(t->buffer->data, (const void __user *)(uintptr_t)
			   tr->data.ptr.buffer, tr->data_size)) {  //将tr结构体中data.ptr.buffer(ipcData)拷贝到内核空间binder_transaction的binder_buffer中
	}fhf
	if (copy_from_user(offp, (const void __user *)(uintptr_t)
			   tr->data.ptr.offsets, tr->offsets_size)) {  //将tr中的中data.ptr.offsets(ipcObjects)对象起始地址拷贝到内核空间
	}

	off_end = (void *)off_start + tr->offsets_size;
	sg_bufp = (u8 *)(PTR_ALIGN(off_end, sizeof(void *)));
	sg_buf_end = sg_bufp + extra_buffers_size -
		ALIGN(secctx_sz, sizeof(u64));
	off_min = 0;
	for (; offp < off_end; offp++) {
		//因为data里面并没有binder对象，所以tr->offsets_size = 0, 所以off_end = off_start = offp，所以不会进入这个循环里
	}
	//往当前线程type，意味着本次客户端向驱动这次单向通信完成，驱动需要回复给当前线程通信成功
	tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
	//往目标进程的type，驱动向目标进程的一次单向通信
	t->work.type = BINDER_WORK_TRANSACTION;

	if (reply) {
		...
	} else if (!(t->flags & TF_ONE_WAY)) {
		BUG_ON(t->buffer->async_transaction != 0);
		binder_inner_proc_lock(proc);
		//将tcomplete加入当前线程的todo工作队列，通过list_add_tail(&tcomplete->entry, &thread->todo);
		binder_enqueue_deferred_thread_work_ilocked(thread, tcomplete);
		t->need_reply = 1;  //需要目的进程回复
		t->from_parent = thread->transaction_stack;  //当前线程事务栈为null，所以t->from_parent = null
		thread->transaction_stack = t;  //将t加入当前线程的事务栈
		binder_inner_proc_unlock(proc);
		if (!binder_proc_transaction(t, target_proc, target_thread)) {
			...
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
	return;
...
}
```

当前是向SM服务获取服务，所以我们并没有携带binder对象过来（data里只有服务的name），这里跳过了binder相关的转化，然后就是构建往目标进程的binder\_transaction和往当前线程的binder\_work，并加入各自的线程（或进程）的todo队列，最后唤醒目标线程（进程）。

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
	...
	if (oneway) {
		...
	}

	binder_inner_proc_lock(proc);

	if (!thread && !pending_async)
		//从目标进程中获取一个可用的线程
		thread = binder_select_thread_ilocked(proc);

	if (thread) {
		binder_transaction_priority(thread->task, t, node_prio,
					    node->inherit_rt);
	    //如果目标进程中能找到可用的binder线程，就把事务binder_work放入线程的todo队列
		binder_enqueue_thread_work_ilocked(thread, &t->work);
	} else if (!pending_async) {
		//如果目标进程没有可用的进程，就把binder_work放入进程的todo队列
		binder_enqueue_work_ilocked(&t->work, &proc->todo);
	} else {
		//如果没找到可用的进程且是异步任务，就把binder_work放入进程的异步工作队列async_todo
		binder_enqueue_work_ilocked(&t->work, &node->async_todo);
	}

	if (!pending_async)  //如果不是异步任务，就唤醒线程/进程
		binder_wakeup_thread_ilocked(proc, thread, !oneway /* sync */);

	binder_inner_proc_unlock(proc);
	binder_node_unlock(node);

	return true;
}
```

这里就是尝试从目标进程找一个可用的线程来处理binder\_transaction, 如果找到某个线程可用就将t->work加入线程的todo队列，找不到就加入进程的todo的队列，然后唤醒目标线程（进程）

#### binder\_wakeup\_thread\_ilocked

```cpp
tatic void binder_wakeup_thread_ilocked(struct binder_proc *proc,
					 struct binder_thread *thread,
					 bool sync)
{
	assert_spin_locked(&proc->inner_lock);

	if (thread) {
		if (sync)
			//如果目标进程的某个线程可用，就从它等待队列上唤醒它
			wake_up_interruptible_sync(&thread->wait);
		else
			wake_up_interruptible(&thread->wait);
		return;
	}
	
	//如果找不到可用的线程，就唤醒目标进程全部binder线程
	binder_wakeup_poll_threads_ilocked(proc, sync);
}
```

这里就是重要的，唤醒目标进程（线程）的地方，从这里会进入目标进程的上下文中(SM进程启动后会阻塞在驱动中等待唤醒)

#### 进入目标进程中

SM进程唤醒位置

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

	if (*consumed == 0) {  //如果read_consumed == 0就返回一个BR_NOOP事件到用户空间
		if (put_user(BR_NOOP, (uint32_t __user *)ptr))
			return -EFAULT;
		ptr += sizeof(uint32_t);
	}

retry:
	binder_inner_proc_lock(proc);
	//检测当前线程是否空闲（todo队列为空&事务栈为空）
	wait_for_proc_work = binder_available_for_proc_work_ilocked(thread);
	binder_inner_proc_unlock(proc);

	thread->looper |= BINDER_LOOPER_STATE_WAITING;  //如果当前线程处于空间就设置它的状态为等待
	if (non_block) {
		//如果用户进程是通过非阻塞方式访问驱动（open时传入的NONBLOCK）
		if (!binder_has_work(thread, wait_for_proc_work))
			ret = -EAGAIN;
	} else {
		//阻塞方式
		ret = binder_wait_for_work(thread, wait_for_proc_work);
	}

	//当线程（进程）被唤醒时，要做的事情
	while (1) {
	...
	}
done：
	...
}
```

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
		////将线程加入等待队列，并更改当前线程的状态
		prepare_to_wait(&thread->wait, &wait, TASK_INTERRUPTIBLE);
		//退出睡眠的条件，被唤醒也要符合条件才能退出循环，否则再次进入睡眠
		if (binder_has_work_ilocked(thread, do_proc_work))
			break;//如果线程工作队列上有活干了，就退出循环，处理todo队列上的事务
	    //如果线程空闲，就将线程加入waiting_threads
		if (do_proc_work)
			list_add(&thread->waiting_thread_node,
				 &proc->waiting_threads);
		binder_inner_proc_unlock(proc);
		schedule();  //让出调度，进入睡眠，线程从此等待被唤醒
		binder_inner_proc_lock(proc);
		list_del_init(&thread->waiting_thread_node);
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
	}
	//将线程等待队列中删除,更改当前线程的状态，将当前线程置于TASK_RUNNING状态
	finish_wait(&thread->wait, &wait);
	binder_inner_proc_unlock(proc);
	freezer_count();

	return ret;
}
```

接着上面唤醒后往下执行：

```cpp
static int binder_thread_read(struct binder_proc *proc,
			      struct binder_thread *thread,
			      binder_uintptr_t binder_buffer, size_t size,
			      binder_size_t *consumed, int non_block)
{
	....
	if (non_block) {
		if (!binder_has_work(thread, wait_for_proc_work))
			ret = -EAGAIN;
	} else {
		ret = binder_wait_for_work(thread, wait_for_proc_work);
	}
	//从上面wait状态唤醒，移除线程的wait状态
	thread->looper &= ~BINDER_LOOPER_STATE_WAITING;
	//线程唤醒后要做的事情
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
		//如果当前线程todo队列不为空，工作队列就从是线程todo，否则就是进程的todo
		if (!binder_worklist_empty_ilocked(&thread->todo))
			list = &thread->todo;
		else if (!binder_worklist_empty_ilocked(&proc->todo) &&
			   wait_for_proc_work)
			list = &proc->todo;
		else {
			binder_inner_proc_unlock(proc);
		
		//从工作队列中获取binder_work（出队）
		w = binder_dequeue_work_head_ilocked(list);
		if (binder_worklist_empty_ilocked(&thread->todo))
			thread->process_todo = false;

		switch (w->type) {
		case BINDER_WORK_TRANSACTION: {
			binder_inner_proc_unlock(proc);
			//根据binder_work构建binder_transaction
			t = container_of(w, struct binder_transaction, work);
		} break;
		...
		}

		BUG_ON(t->buffer == NULL);
		if (t->buffer->target_node) {
			// //从binder_buffer获取相关参数并构建binder_transaction_data
			struct binder_node *target_node = t->buffer->target_node; //SM进程
			struct binder_priority node_prio;

			trd->target.ptr = target_node->ptr;  //data指针
			trd->cookie =  target_node->cookie;  //获取服务被没有携带binder，所以cookie这里为null
			node_prio.sched_policy = target_node->sched_policy;
			node_prio.prio = target_node->min_priority;
			binder_transaction_priority(current, t, node_prio,
						    target_node->inherit_rt);
			cmd = BR_TRANSACTION;  // cmd为BR_TRANSACTION，为驱动向目标进程发起请求
		} else {
			...
		}
		trd->code = t->code;  //code 为getservice对应的协议码
		trd->flags = t->flags;  //flag
		trd->sender_euid = from_kuid(current_user_ns(), t->sender_euid);

		t_from = binder_get_txn_from(t);  //获取事务从哪个客户端线程发过来的（客户端请求线程）
		if (t_from) {
			struct task_struct *sender = t_from->proc->tsk;

			trd->sender_pid =
				task_tgid_nr_ns(sender,
						task_active_pid_ns(current));  //客户端请求进程
		} else {
			trd->sender_pid = 0;
		}

		//获取服务时，data里只有想要获取服务的name字段。没有binder对象
		trd->data_size = t->buffer->data_size;
		trd->offsets_size = t->buffer->offsets_size;
		trd->data.ptr.buffer = (binder_uintptr_t)
			((uintptr_t)t->buffer->data +
			binder_alloc_get_user_buffer_offset(&proc->alloc));
		trd->data.ptr.offsets = trd->data.ptr.buffer +
					ALIGN(t->buffer->data_size,
					    sizeof(void *));

		//将cmd拷贝到用户空间输入缓冲区（）
		if (put_user(cmd, (uint32_t __user *)ptr)) {
			...
		}
		ptr += sizeof(uint32_t);  //更新内存偏移
		////将tr结构体拷贝到用户空间输入缓冲区（）
		if (copy_to_user(ptr, &tr, trsize)) {
			...
		}
		ptr += trsize;  //更新内存偏移
		//到这里用户空间输入缓冲区就可以收到一个完整的bwr结构体数据：
		//bwr = cmd + tr

		if (t_from)
			binder_thread_dec_tmpref(t_from);
		t->buffer->allow_user_free = 1;
		if (cmd != BR_REPLY && !(t->flags & TF_ONE_WAY)) {
			//更新事务链条和当前线程的事务栈
			binder_inner_proc_lock(thread->proc);
			t->to_parent = thread->transaction_stack;  //null 因为前面thread并没有事务
			t->to_thread = thread; //更新事务的目标线程
			thread->transaction_stack = t;  //更新当前thread的事务栈（压栈）
			binder_inner_proc_unlock(thread->proc);
		} else {
			binder_free_transaction(t);
		}
		break;
	}

done:

	*consumed = ptr - buffer;  //更新已读取的字节数
	binder_inner_proc_lock(proc);
	//检测是否需要向用户进程请求新建binder线程池
	//对于ServiceManager进程，它在启动时就设置只有一个binder线程（主线程）
	//ps->setThreadPoolMaxThreadCount(0);
	if (proc->requested_threads == 0 &&
	    list_empty(&thread->proc->waiting_threads) &&
	    proc->requested_threads_started < proc->max_threads &&
	    (thread->looper & (BINDER_LOOPER_STATE_REGISTERED |
	     BINDER_LOOPER_STATE_ENTERED)) /* the user-space code fails to */
	     /*spawn a new thread if we leave this out */) {
		proc->requested_threads++;
		binder_inner_proc_unlock(proc);

		if (put_user(BR_SPAWN_LOOPER, (uint32_t __user *)buffer))
			return -EFAULT;
		binder_stat_br(proc, thread, BR_SPAWN_LOOPER);
	} else
		binder_inner_proc_unlock(proc);
	return 0;
}
```

到这里线程（进程）唤醒后会将客户端传过来的`binder_transaction`解析并写入用户空间输入缓冲区，驱动向SM用户进程发起一个`BR_TRANSACTION`请求，用户进程从ioctl中返回，处理输入缓冲区数据。

#### ServiceManager进程用户空间

Servicemanager进程只有一个binder线程，当Servicemanager进程启动后，就通过Looper监听驱动的fd，当驱动从ioctl中返回，就会回调`handleEvent()`方法

#### handleEvent

frameworks/native/cmds/servicemanager/main.cpp

```cpp
    int handleEvent(int /* fd */, int /* events */, void* /* data */) override {
        IPCThreadState::self()->handlePolledCommands();
        return 1;  // Continue receiving callbacks.
    }
```

#### IPCThreadState::handlePolledCommands

frameworks/native/libs/binder/IPCThreadState.cpp

```cpp
status_t IPCThreadState::handlePolledCommands()
{
    status_t result;

    do {
        result = getAndExecuteCommand();  //循环获取驱动写到输入缓冲区的数据
    } while (mIn.dataPosition() < mIn.dataSize());

    processPendingDerefs();
    flushCommands(); //让线程回到内核空间中
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
        if (IN < sizeof(int32_t)) return result;
        cmd = mIn.readInt32();

        pthread_mutex_lock(&mProcess->mThreadCountLock);
        //当前在干活的线程数 + 1
        mProcess->mExecutingThreadsCount++;
        //如果当前在执行的线程数大于等于进程最大binder线程数时，
        //说明当前binder线程池已经耗尽，此时开始记录线程池耗尽的时间
        if (mProcess->mExecutingThreadsCount >= mProcess->mMaxThreads &&
                mProcess->mStarvationStartTimeMs == 0) {
            mProcess->mStarvationStartTimeMs = uptimeMillis();
        }
        pthread_mutex_unlock(&mProcess->mThreadCountLock);

        result = executeCommand(cmd);  //执行对应cmd行为

        pthread_mutex_lock(&mProcess->mThreadCountLock);
        //当前线程干完上面的活了，对当前在干活的线程数 - 1
        mProcess->mExecutingThreadsCount--;
        //如果现在在干活的线程数小于最大线程数且上次计时还没停
        //打印线程池耗尽的时间，并重置这个时间
        if (mProcess->mExecutingThreadsCount < mProcess->mMaxThreads &&
                mProcess->mStarvationStartTimeMs != 0) {
            int64_t starvationTimeMs = uptimeMillis() - mProcess->mStarvationStartTimeMs;
            if (starvationTimeMs > 100) {
                ALOGE("binder thread pool (%zu threads) starved for %" PRId64 " ms",
                      mProcess->mMaxThreads, starvationTimeMs);
            }
            mProcess->mStarvationStartTimeMs = 0;
        }

        // Cond broadcast can be expensive, so don't send it every time a binder
        // call is processed. b/168806193
        if (mProcess->mWaitingForThreads > 0) {
            pthread_cond_broadcast(&mProcess->mThreadCountDecrement);  //如果当前有等待线程处理的请求大于1则唤醒block的线程
        }
        pthread_mutex_unlock(&mProcess->mThreadCountLock);
    }

    return result;
}
```

重点是`executeCommand`, 根据驱动的BR\_XX执行对应的处理

#### executeCommand

```cpp
status_t IPCThreadState::executeCommand(int32_t cmd)
{
    BBinder* obj;
    RefBase::weakref_type* refs;
    status_t result = NO_ERROR;

    switch ((uint32_t)cmd) {
	...
    case BR_TRANSACTION:
        {
            binder_transaction_data_secctx tr_secctx;
            binder_transaction_data& tr = tr_secctx.transaction_data;

            if (cmd == (int) BR_TRANSACTION_SEC_CTX) {
                result = mIn.read(&tr_secctx, sizeof(tr_secctx));
            } else {  //cmd = BR_TRANSACTION
                result = mIn.read(&tr, sizeof(tr));
                tr_secctx.secctx = 0;
            }

            Parcel buffer;

            const void* origServingStackPointer = mServingStackPointer;
            mServingStackPointer = __builtin_frame_address(0);

            const pid_t origPid = mCallingPid;
            const char* origSid = mCallingSid;
            const uid_t origUid = mCallingUid;
            const int32_t origStrictModePolicy = mStrictModePolicy;
            const int32_t origTransactionBinderFlags = mLastTransactionBinderFlags;
            const int32_t origWorkSource = mWorkSource;
            const bool origPropagateWorkSet = mPropagateWorkSource;
            // Calling work source will be set by Parcel#enforceInterface. Parcel#enforceInterface
            // is only guaranteed to be called for AIDL-generated stubs so we reset the work source
            // here to never propagate it.
            clearCallingWorkSource();
            clearPropagateWorkSource();

            mCallingPid = tr.sender_pid;
            mCallingSid = reinterpret_cast<const char*>(tr_secctx.secctx);
            mCallingUid = tr.sender_euid;
            mLastTransactionBinderFlags = tr.flags;

            Parcel reply;
            status_t error;

            if (tr.target.ptr) {  //ptr不为空，说明是带着binder引用和cookie过来的，需要转化成本地binder实体才能调用对应的方法
                // We only have a weak reference on the target object, so we must first try to
                // safely acquire a strong reference before doing anything else with it.
                if (reinterpret_cast<RefBase::weakref_type*>(
                        tr.target.ptr)->attemptIncStrong(this)) {
                    error = reinterpret_cast<BBinder*>(tr.cookie)->transact(tr.code, buffer,
                            &reply, tr.flags);
                    reinterpret_cast<BBinder*>(tr.cookie)->decStrong(this);
                } else {
                    error = UNKNOWN_TRANSACTION;
                }

            } else { //向SM进程获取服务是不带binder对象的，所以走这里，使用SM的BBinder对象的transact（这个对象是SM进程启动时往驱动注册的）
                error = the_context_object->transact(tr.code, buffer, &reply, tr.flags);
            }
            //从transact中返回，向驱动返回BR_REPLY
            if ((tr.flags & TF_ONE_WAY) == 0) {  //同步请求
                constexpr uint32_t kForwardReplyFlags = TF_CLEAR_BUF; //请求释放buffer
                sendReply(reply, (tr.flags & kForwardReplyFlags));  //向驱动返回BR_REPLY
            } else { //异步请求, 不需要reply
 				...
            }
            ...
        }
        break;
		...
    return result;
}
```

#### BBinder::transact

frameworks/native/libs/binder/Binder.cpp

```cpp
// NOLINTNEXTLINE(google-default-arguments)
status_t BBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    data.setDataPosition(0);

    if (reply != nullptr && (flags & FLAG_CLEAR_BUF)) {
        reply->markSensitive();
    }

    status_t err = NO_ERROR;
    switch (code) {
        case PING_TRANSACTION:
            err = pingBinder();
            break;
        case EXTENSION_TRANSACTION:
            CHECK(reply != nullptr);
            err = reply->writeStrongBinder(getExtension());
            break;
        case DEBUG_PID_TRANSACTION:
            CHECK(reply != nullptr);
            err = reply->writeInt32(getDebugPid());
            break;
        case SET_RPC_CLIENT_TRANSACTION: {
            err = setRpcClientDebug(data);
            break;
        }
        default:
            err = onTransact(code, data, reply, flags);  //code是getService，上面的都不是，要调onTransact()方法，这个方法是子类实现的
            break;
    }

    // In case this is being transacted on in the same process.
    if (reply != nullptr) {
        reply->setDataPosition(0);
        if (reply->dataSize() > LOG_REPLIES_OVER_SIZE) {
            ALOGW("Large reply transaction of %zu bytes, interface descriptor %s, code %d",
                  reply->dataSize(), String8(getInterfaceDescriptor()).c_str(), code);
        }
    }

    return err;
}
```

最终还是需要往子类实现的onTransact()方法中处理, 子类就是`BnServiceManager`

#### BnServiceManager::onTransact

```cpp
::android::status_t BnServiceManager::onTransact(uint32_t _aidl_code, const ::android::Parcel& _aidl_data, ::android::Parcel* _aidl_reply, uint32_t _aidl_flags) {
  ::android::status_t _aidl_ret_status = ::android::OK;
  switch (_aidl_code) {
  case BnServiceManager::TRANSACTION_getService:
  {
    ::std::string in_name;
    ::android::sp<::android::IBinder> _aidl_return;
    
     //读取跨进程传递过来的service name
    _aidl_ret_status = _aidl_data.readUtf8FromUtf16(&in_name);
	//调用BnServiceManager的对应实现类方法getService()
    ::android::binder::Status _aidl_status(getService(in_name, &_aidl_return));
	//将获取到的service binder写入_aidl_reply
    _aidl_ret_status = _aidl_reply->writeStrongBinder(_aidl_return);

  }
  break;
  ...
}
```

#### ServiceManager::getService

```cpp
Status ServiceManager::getService(const std::string& name, sp<IBinder>* outBinder) {
    *outBinder = tryGetService(name, true);
    // returns ok regardless of result for legacy reasons
    return Status::ok();
}

sp<IBinder> ServiceManager::tryGetService(const std::string& name, bool startIfNotFound) {
    auto ctx = mAccess->getCallingContext();  //校验权限

    sp<IBinder> out;
    Service* service = nullptr;
    if (auto it = mNameToService.find(name); it != mNameToService.end()) {
        service = &(it->second);

        out = service->binder;
    }

    if (out) {
        // Setting this guarantee each time we hand out a binder ensures that the client-checking
        // loop knows about the event even if the client immediately drops the service
        service->guaranteeClient = true;
    }

    return out;
}
```

从mNameToService中查找name对应的BpBinder对象并返回到上面`IPCThreadState::executeCommand中`

```cpp
status_t IPCThreadState::executeCommand(int32_t cmd)
{
    BBinder* obj;
    RefBase::weakref_type* refs;
    status_t result = NO_ERROR;

    switch ((uint32_t)cmd) {
	...
    case BR_TRANSACTION:
        {

            if (tr.target.ptr) {  //ptr不为空，说明是带着binder引用和cookie过来的，需要转化成本地binder实体才能调用对应的方法
				...
            } else { //向SM进程获取服务是不带binder对象的，所以走这里，使用SM的BBinder对象的transact（这个对象是SM进程启动时往驱动注册的）
                error = the_context_object->transact(tr.code, buffer, &reply, tr.flags);
            }
            //从transact中返回，向驱动返回BC_REPLY
            if ((tr.flags & TF_ONE_WAY) == 0) {  //同步请求
                constexpr uint32_t kForwardReplyFlags = TF_CLEAR_BUF; //请求释放buffer
                sendReply(reply, (tr.flags & kForwardReplyFlags));  //向驱动返回BR_REPLY
            } else { //异步请求, 不需要reply
 				...
            }
            ...
        }
        break;
		...
    return result;
}
```

从`transact`中返回，调用`sendReply`向驱动返回获取到的BpBinder对象

#### IPCThreadState::sendReply

```cpp
status_t IPCThreadState::sendReply(const Parcel& reply, uint32_t flags)
{
    status_t err;
    status_t statusBuffer;
    //写入数据到输出缓冲区
    err = writeTransactionData(BC_REPLY, flags, -1, 0, reply, &statusBuffer);
	//向驱动写入
    return waitForResponse(nullptr, nullptr);
}
```

1.  在`writeTransactionData`中将ipc相关数据写入mOut输出缓冲区, 写入格式bwr = cmd + tr
2.  调用`waitForResponse`向驱动写入

#### writeTransactionData

```cpp
status_t IPCThreadState::writeTransactionData(int32_t cmd, uint32_t binderFlags,
    int32_t handle, uint32_t code, const Parcel& data, status_t* statusBuffer)
{
    binder_transaction_data tr;

    tr.target.ptr = 0; /* Don't pass uninitialized stack data to a remote process */
    tr.target.handle = handle;  //handle = -1
    tr.code = code;  //code = 0
    tr.flags = binderFlags;
    tr.cookie = 0;
    tr.sender_pid = 0;
    tr.sender_euid = 0;

    const status_t err = data.errorCheck();
    if (err == NO_ERROR) {
        tr.data_size = data.ipcDataSize();  //ipc数据大小
        tr.data.ptr.buffer = data.ipcData();  //ipc数据
        tr.offsets_size = data.ipcObjectsCount()*sizeof(binder_size_t);  //binder对象地址范围
        tr.data.ptr.offsets = data.ipcObjects();  //binder对象偏移指针
    } else if (statusBuffer) {
		...
    } else {
        return (mLastError = err);
    }

    mOut.writeInt32(cmd);  //cmd BC_REPLY
    mOut.write(&tr, sizeof(tr)); //tr结构体

    return NO_ERROR;
}
```

#### IPCThreadState::waitForResponse

```cpp
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    uint32_t cmd;
    int32_t err;

    while (1) {
    	//向读取读写
        if ((err=talkWithDriver()) < NO_ERROR) break; 

        cmd = (uint32_t)mIn.readInt32();

        switch (cmd) {
		...
        case BR_REPLY:
            {
                binder_transaction_data tr;
                err = mIn.read(&tr, sizeof(tr));
                ALOG_ASSERT(err == NO_ERROR, "Not enough command data for brREPLY");
                if (err != NO_ERROR) goto finish;

                if (reply) {  //reply = null
					...
                } else {
                    freeBuffer(nullptr,
                        reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                        tr.data_size,
                        reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                        tr.offsets_size/sizeof(binder_size_t));
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
...
    return err;
}
```

通过`talkWithDriver`方法中`ioctl`和驱动交互，将数据传给驱动

#### 再次进入内核空间

进入驱动中处理，此时仍处于SM进程上下文中

#### binder\_ioctl

```cpp
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct binder_proc *proc = filp->private_data;
	struct binder_thread *thread;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;

	thread = binder_get_thread(proc);  //获取当前线程

	switch (cmd) {
	case BINDER_WRITE_READ:
		ret = binder_ioctl_write_read(filp, cmd, arg, thread);
		if (ret)
			goto err;
		break;
	...
	return ret;
}

static int binder_ioctl_write_read(struct file *filp,
				unsigned int cmd, unsigned long arg,
				struct binder_thread *thread)
{
	int ret = 0;
	struct binder_proc *proc = filp->private_data;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;
	struct binder_write_read bwr;

	if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {
		ret = -EFAULT;
		goto out;
	}

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
	if (bwr.read_size > 0) {
		...
	}
	...
}

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

		if (get_user(cmd, (uint32_t __user *)ptr))  //将cmd从用户空间输出缓冲区拷贝到内核空间
			return -EFAULT;
		ptr += sizeof(uint32_t);  //内存指针偏移一个cmd大小

		switch (cmd) {
		...
		case BC_TRANSACTION:
		case BC_REPLY: {  //这次是BC_REPLY
			struct binder_transaction_data tr;

			if (copy_from_user(&tr, ptr, sizeof(tr))) //拷贝tr结构体到内核空间
				return -EFAULT;
			ptr += sizeof(tr);  //内存指针偏移一个tr结构体大小
			binder_transaction(proc, thread, &tr,
					   cmd == BC_REPLY, 0);  //核心方法，处理tr结构体中的数据，并投递到请求客户端进程 cmd -> BC_REPLY = true
			break;
		}
		...
		}
		*consumed = ptr - buffer;  //更新已写入的字节数，在这里已经是全部写完了
	}
	return 0;
}
```

从SM进程中拷贝输出缓冲区内容到内核空间，并调用核心方法`binder_transaction`处理

#### binder\_transaction

```cpp
static void binder_transaction(struct binder_proc *proc,
			       struct binder_thread *thread,
			       struct binder_transaction_data *tr, int reply,
			       binder_size_t extra_buffers_size)
{
	...
	if (reply) {  //这次是BC_REPLY
		binder_inner_proc_lock(proc);
		in_reply_to = thread->transaction_stack;  //前面赋值为BR_TRANSATION时对应的那个事务，

		thread->transaction_stack = in_reply_to->to_parent;  //为null
		binder_inner_proc_unlock(proc);
		target_thread = binder_get_txn_from_and_acq_inner(in_reply_to);  //获取服务请求端请求线程

		target_proc = target_thread->proc;  //请求端进程binder_proc
		atomic_inc(&target_proc->tmp_ref);
		binder_inner_proc_unlock(target_thread->proc);
	} else {
		...
	}

	/* TODO: reuse incoming transaction for reply */
	t = kzalloc(sizeof(*t), GFP_KERNEL);  //往请求端进程的binder_transaction

	binder_stats_created(BINDER_STAT_TRANSACTION);
	spin_lock_init(&t->lock);

	tcomplete = kzalloc(sizeof(*tcomplete), GFP_KERNEL);  //往当前SM进程回复complete的binder_work

	binder_stats_created(BINDER_STAT_TRANSACTION_COMPLETE);

	t->debug_id = t_debug_id;

	if (!reply && !(tr->flags & TF_ONE_WAY))
		t->from = thread;
	else
		t->from = NULL;  //不记录服务端进程的binder线程（没必要，客户端不需要再reply到服务端进程）
	t->sender_euid = task_euid(proc->tsk);
	t->to_proc = target_proc;  //客户端进程
	t->to_thread = target_thread;  //客户端请求线程
	t->code = tr->code;  //code = 0
	t->flags = tr->flags;

	t->buffer = binder_alloc_new_buf(&target_proc->alloc, tr->data_size,
		tr->offsets_size, extra_buffers_size,
		!reply && (t->flags & TF_ONE_WAY)); //从红黑树获取一个空的binder_buffer
		
	t->buffer->debug_id = t->debug_id;
	t->buffer->transaction = t;  //装入t
	t->buffer->target_node = target_node;  //reply中的target_node = null

	off_start = (binder_size_t *)(t->buffer->data +
				      ALIGN(tr->data_size, sizeof(void *)));
	offp = off_start;
	//从用户空间tr结构体中拷贝ipc data到内核空间
	if (copy_from_user(t->buffer->data, (const void __user *)(uintptr_t)
			   tr->data.ptr.buffer, tr->data_size)) {
	}
	//从用户空间tr结构体中拷贝对象偏移地址指针到内核空间
	if (copy_from_user(offp, (const void __user *)(uintptr_t)
			   tr->data.ptr.offsets, tr->offsets_size)) {
	}

	off_end = (void *)off_start + tr->offsets_size;
	sg_bufp = (u8 *)(PTR_ALIGN(off_end, sizeof(void *)));
	sg_buf_end = sg_bufp + extra_buffers_size -
		ALIGN(secctx_sz, sizeof(u64));
	off_min = 0;
	//reply中带有binder代理对象，需要进行一次循环
	for (; offp < off_end; offp++) {
		struct binder_object_header *hdr;
		size_t object_size = binder_validate_object(t->buffer, *offp);

		hdr = (struct binder_object_header *)(t->buffer->data + *offp);
		off_min = *offp + object_size;
		switch (hdr->type) {
		//SM进程reply了一个binder代理对象，但是我们要跨进程到请求进程中，binder type要转化为handle type
		//通过SM查询到的binder代理对象，使用binder代理指针在当前进程的proc->nodes.rb_node中查询对应的binder_node
		//从这个binder_node中获取binder_ref, 并增加这个binder_ref的强引用
		//把fp.hdr.type改为BINDER_TYPE_HANDLE
		//设置fp->binder = 0 和 fp->cookie = 0
		//fp->handle = rdata.desc 这个是给用户空间使用的handle值，指向这个binder_ref (handle值也是在进程范围内起作用的)
		case BINDER_TYPE_BINDER:
		case BINDER_TYPE_WEAK_BINDER: {
			struct flat_binder_object *fp;

			fp = to_flat_binder_object(hdr);
			ret = binder_translate_binder(fp, t, thread);
			if (ret < 0) {
				return_error = BR_FAILED_REPLY;
				return_error_param = ret;
				return_error_line = __LINE__;
				goto err_translate_failed;
			}
		} break;
		case BINDER_TYPE_HANDLE:
		case BINDER_TYPE_WEAK_HANDLE: {
			struct flat_binder_object *fp;

			fp = to_flat_binder_object(hdr);
			ret = binder_translate_handle(fp, t, thread);
			if (ret < 0) {
				return_error = BR_FAILED_REPLY;
				return_error_param = ret;
				return_error_line = __LINE__;
				goto err_translate_failed;
			}
		} break;
		...
		}
	}
	tcomplete->type = BINDER_WORK_TRANSACTION_COMPLETE;
	t->work.type = BINDER_WORK_TRANSACTION;

	if (reply) {
		//往当前SM进程todo队列写入tcomplete
		binder_enqueue_thread_work(thread, tcomplete);
		binder_inner_proc_lock(target_proc);
		//将目标线程的事务栈更新为当前服务端线程的事务栈，完成事务栈链条
		binder_pop_transaction_ilocked(target_thread, in_reply_to);
		//往客户端请求线程todo队列放入&t->work
		binder_enqueue_thread_work_ilocked(target_thread, &t->work);
		
		binder_inner_proc_unlock(target_proc);
		//唤醒请求线程
		wake_up_interruptible_sync(&target_thread->wait);
		//恢复SM当前线程的优先级
		binder_restore_priority(current, in_reply_to->saved_priority);
		//释放本次binder_transaction占用的buffer内存
		binder_free_transaction(in_reply_to);
	} else if (!(t->flags & TF_ONE_WAY)) {
		...
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
	return;
...
}
```

#### 唤醒客户端进程中的请求线程，进入客户端进程上下文

和前面分析的SM进程唤醒是相同的，客户端求线程唤醒也是从`binder_wait_for_work`中继续往下执行

```cpp
static int binder_thread_read(struct binder_proc *proc,
			      struct binder_thread *thread,
			      binder_uintptr_t binder_buffer, size_t size,
			      binder_size_t *consumed, int non_block)
{
	void __user *buffer = (void __user *)(uintptr_t)binder_buffer;
	void __user *ptr = buffer + *consumed;
	void __user *end = buffer + size;

	...

	int ret = 0;
	int wait_for_proc_work;
	if (non_block) {
		if (!binder_has_work(thread, wait_for_proc_work))
			ret = -EAGAIN;
	} else {
		ret = binder_wait_for_work(thread, wait_for_proc_work);
	}
	//线程唤醒后往下执行
	thread->looper &= ~BINDER_LOOPER_STATE_WAITING;  //线程移除wait状态

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
		//获取todo队列
		if (!binder_worklist_empty_ilocked(&thread->todo))
			list = &thread->todo;
		else if (!binder_worklist_empty_ilocked(&proc->todo) &&
			   wait_for_proc_work)
			list = &proc->todo;
		else {
			binder_inner_proc_unlock(proc);
		//从todo队列获取binder_work(出队)
		w = binder_dequeue_work_head_ilocked(list);
		if (binder_worklist_empty_ilocked(&thread->todo))
			thread->process_todo = false;  //出队后如果队列为空，就设置线程为当前没有处理事务需要处理

		switch (w->type) {  //
		case BINDER_WORK_TRANSACTION: {
			binder_inner_proc_unlock(proc);
			t = container_of(w, struct binder_transaction, work);  //从binder_work构建binder_transaction
		} break;
		...
		}
		BUG_ON(t->buffer == NULL);
		if (t->buffer->target_node) {  //reply的target_node为null
			...
		} else {
			trd->target.ptr = 0;  //reply的是handle，不是binder，所以ptr，cookie都是0
			trd->cookie = 0;
			cmd = BR_REPLY;  //驱动向用户进程的返回 BR_REPLY
		}
		trd->code = t->code;
		trd->flags = t->flags;
		trd->sender_euid = from_kuid(current_user_ns(), t->sender_euid);

		t_from = binder_get_txn_from(t);  //获取服务端处理线程，为null，没有赋值（不需要知道）
		if (t_from) {
			...
		} else {
			trd->sender_pid = 0;
		}

		//从t中获取ipc data 以及对象偏移等并构造binder_transation_data结构体
		trd->data_size = t->buffer->data_size;
		trd->offsets_size = t->buffer->offsets_size;
		trd->data.ptr.buffer = (binder_uintptr_t)
			((uintptr_t)t->buffer->data +
			binder_alloc_get_user_buffer_offset(&proc->alloc));
		trd->data.ptr.offsets = trd->data.ptr.buffer +
					ALIGN(t->buffer->data_size,
					    sizeof(void *));

		tr.secctx = t->security_ctx;  //null

		//向客户端进程用户空间输入缓冲区写入cmd大小
		if (put_user(cmd, (uint32_t __user *)ptr)) {
		}
		ptr += sizeof(uint32_t);  //内存指针向后偏移一个cmd大小
		//向客户端进程用户空间输入缓冲区写入tr
		if (copy_to_user(ptr, &tr, trsize)) {
		}
		ptr += trsize;  //内存指针偏移tr大小

		t->buffer->allow_user_free = 1;  //允许SM服务进程释放事务buffer
		if (cmd != BR_REPLY && !(t->flags & TF_ONE_WAY)) {
			...
		} else {
			binder_free_transaction(t);  //释放binder_buffer
		}
		break;
	}

done:
	//判断是否需要向用户空间新建binder线程处理事务
	*consumed = ptr - buffer;
	binder_inner_proc_lock(proc);
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
		if (put_user(BR_SPAWN_LOOPER, (uint32_t __user *)buffer))
			return -EFAULT;
		binder_stat_br(proc, thread, BR_SPAWN_LOOPER);
	} else
		binder_inner_proc_unlock(proc);
	return 0;
}
```

到这里驱动在内核空间已经执行完成，将返回用户空间中（ioctl中返回）

#### 回到客户端进程用户空间

然后就是从`ioclt->talkWithDriver->waitForResponse`一路返回

```cpp
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    uint32_t cmd;
    int32_t err;

    while (1) {
        if ((err=talkWithDriver()) < NO_ERROR) break;

        cmd = (uint32_t)mIn.readInt32();

        switch (cmd) {
		...
        case BR_REPLY:
            {
                binder_transaction_data tr;
                err = mIn.read(&tr, sizeof(tr));
                ALOG_ASSERT(err == NO_ERROR, "Not enough command data for brREPLY");
                if (err != NO_ERROR) goto finish;

                if (reply) {
                    if ((tr.flags & TF_STATUS_CODE) == 0) {
                    	//将tr中的内容赋值给reply
                        reply->ipcSetDataReference(
                            reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                            tr.data_size,
                            reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                            tr.offsets_size/sizeof(binder_size_t),
                            freeBuffer);
                    } else {
						...
                    }
                } else {
					...
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
	...

    return err;  //最终从这里一路返回调用的开始
}
```

在`BR_REPLY`的case中将tr数据写入reply对象中，然后return，一路回到`BpServiceManager::getService`中

#### ipcSetDataReference

```cpp
oid Parcel::ipcSetDataReference(const uint8_t* data, size_t dataSize,
    const binder_size_t* objects, size_t objectsCount, release_func relFunc)
{
    // this code uses 'mOwner == nullptr' to understand whether it owns memory
    LOG_ALWAYS_FATAL_IF(relFunc == nullptr, "must provide cleanup function");

    freeData();
	//赋值给对象的成员变量
    mData = const_cast<uint8_t*>(data);
    mDataSize = mDataCapacity = dataSize;
    mObjects = const_cast<binder_size_t*>(objects);
    mObjectsSize = mObjectsCapacity = objectsCount;
    mOwner = relFunc;

    binder_size_t minOffset = 0;
    for (size_t i = 0; i < mObjectsSize; i++) {
        binder_size_t offset = mObjects[i];

        const flat_binder_object* flat
            = reinterpret_cast<const flat_binder_object*>(mData + offset);
        uint32_t type = flat->hdr.type;
        if (!(type == BINDER_TYPE_BINDER || type == BINDER_TYPE_HANDLE ||
              type == BINDER_TYPE_FD)) {
            android_errorWriteLog(0x534e4554, "135930648");
            android_errorWriteLog(0x534e4554, "203847542");
            ALOGE("%s: unsupported type object (%" PRIu32 ") at offset %" PRIu64 "\n",
                  __func__, type, (uint64_t)offset);

            // WARNING: callers of ipcSetDataReference need to make sure they
            // don't rely on mObjectsSize in their release_func.
            mObjectsSize = 0;
            break;
        }
        minOffset = offset + sizeof(flat_binder_object);
    }
    scanForFds();
}
```

回到`BpServiceManager::getService`

#### BpServiceManager::getService

```cpp
::android::binder::Status BpServiceManager::getService(const ::std::string& name, ::android::sp<::android::IBinder>* _aidl_return) {
  ::android::Parcel _aidl_data;
  _aidl_data.markForBinder(remoteStrong());
  ::android::Parcel _aidl_reply;
  ::android::status_t _aidl_ret_status = ::android::OK;
  ::android::binder::Status _aidl_status;
  _aidl_ret_status = _aidl_data.writeInterfaceToken(getInterfaceDescriptor());

  _aidl_ret_status = _aidl_data.writeUtf8AsUtf16(name);

  _aidl_ret_status = remote()->transact(BnServiceManager::TRANSACTION_getService, _aidl_data, &_aidl_reply, 0);  //发起请求跨进程获取服务行为

  _aidl_ret_status = _aidl_status.readFromParcel(_aidl_reply);

  _aidl_ret_status = _aidl_reply.readNullableStrongBinder(_aidl_return);  //获取成功后，从_aidl_reply中读取SM进程返回的数据

  return _aidl_status;
}
```

#### Parcel::readNullableStrongBinder

frameworks/native/libs/binder/Parcel.cpp

```cpp
status_t Parcel::readNullableStrongBinder(sp<IBinder>* val) const
{
    return unflattenBinder(val);
}

status_t Parcel::unflattenBinder(sp<IBinder>* out) const
{
	...

    const flat_binder_object* flat = readObject(false);  //从mData获取flat_binder_object格式数据对象

    if (flat) {
        switch (flat->hdr.type) {
            case BINDER_TYPE_BINDER: {
                sp<IBinder> binder =
                        sp<IBinder>::fromExisting(reinterpret_cast<IBinder*>(flat->cookie));
                return finishUnflattenBinder(binder, out);
            }
            case BINDER_TYPE_HANDLE: {   //SM进程发给客户端进程的是服务对应的handle
                sp<IBinder> binder =
                    ProcessState::self()->getStrongProxyForHandle(flat->handle); //从handle转化为BpBinder
                return finishUnflattenBinder(binder, out);
            }
        }
    }
    return BAD_TYPE;
}

```

#### ProcessState::getStrongProxyForHandle

```cpp
sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);

    handle_entry* e = lookupHandleLocked(handle);  //从当前线程的mHandleToObject对象表中查找handle对应的BpBinder对象

    if (e != nullptr) {

        IBinder* b = e->binder;  //因为当前进程还没有这个服务的binder代理对象，上面找不到所以这里为null
        if (b == nullptr || !e->refs->attemptIncWeak(this)) {
            if (handle == 0) {  //ServiceManager特殊情况
				...
            }

            sp<BpBinder> b = BpBinder::PrivateAccessor::create(handle);  //通过handle构造服务在这个进程的binder代理对象
            e->binder = b.get();
            if (b) e->refs = b->getWeakRefs();  //将这个代理对象的引用赋给handle_entry的refs
            result = b;
        } else {
			...
        }
    }

    return result;
}
```

#### ProcessState::lookupHandleLocked

```cpp
ProcessState::handle_entry* ProcessState::lookupHandleLocked(int32_t handle)
{
	//从当前线程的mHandleToObject对象表中查找handle对应的BpBinder对象，
    const size_t N=mHandleToObject.size();
    if (N <= (size_t)handle) {
   		//如果不需要就创建一个handle_entry
        handle_entry e;
        e.binder = nullptr; //初始化handle_entry->binder = nullptr
        e.refs = nullptr;  //对象的弱引用计数
        //插入mHandleToObject
        status_t err = mHandleToObject.insertAt(e, N, handle+1-N);
        if (err < NO_ERROR) return nullptr;
    }
    return &mHandleToObject.editItemAt(handle);
}
```

然后这个BpBinder对象会返回到客户端调用getService的地方，也就是最开始的SurfaceFlinger的调用中，获取服务的操作到这里就完成了。