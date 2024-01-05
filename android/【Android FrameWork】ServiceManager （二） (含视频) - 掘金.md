文章太长了，超过字数限制了不得不分开，非常抱歉。😂 我们接上回。addService执行完毕了客户端回到了等待状态，我们回到`service_manager`中看看唤醒之后的处理: 可能大家都忘记了，我们服务端在哪里等我们了。在`binder_thread_read`中。我们看看。

```
 while (1) {
      //找到需要处理的todo队列
      if (!binder_worklist_empty_ilocked(&thread->todo))
         list = &thread->todo;
      else if (!binder_worklist_empty_ilocked(&proc->todo) &&
            wait_for_proc_work)
         list = &proc->todo;
      else {
         binder_inner_proc_unlock(proc);
      }
      //从todo队列里面取出来一个binder_work
      w = binder_dequeue_work_head_ilocked(list);
      if (binder_worklist_empty_ilocked(&thread->todo))
         thread->process_todo = false;

      switch (w->type) {
      case BINDER_WORK_TRANSACTION: {
       binder_inner_proc_unlock(proc);
       //根据binder_work找到binder_transaction结构  写入的data code = ADD_SERVICE_TRANSACTION  data=AMS
       t = container_of(w, struct binder_transaction, work);
      }
      break;
      }
      
if (!t)//t!=null
   continue;

if (t->buffer->target_node) {//target_node!=null 是service_manager
   struct binder_node *target_node = t->buffer->target_node;
   struct binder_priority node_prio;
   trd->target.ptr = target_node->ptr;//设置binder在用户空间的地址
   trd->cookie =  target_node->cookie;
   //设置优先级
   node_prio.sched_policy = target_node->sched_policy;
   node_prio.prio = target_node->min_priority;
   binder_transaction_priority(current, t, node_prio,
                target_node->inherit_rt);
   cmd = BR_TRANSACTION;//设置响应码为BR_TRANSACTION
}


trd->code = t->code;//记录code也就是ADD_SERVICE_TRANSACTION
trd->flags = t->flags;
trd->sender_euid = from_kuid(current_user_ns(), t->sender_euid);
//记录from也就是发起进程 并且引用计数+1
t_from = binder_get_txn_from(t);

if (t_from) {
   struct task_struct *sender = t_from->proc->tsk;//记录客户端的进程信息
   trd->sender_pid =
      task_tgid_nr_ns(sender,
            task_active_pid_ns(current));
}
//设置数据大小 偏移量 设置数据区的首地址 也就是通过内核空间和用户空间的偏移量算出来的
trd->data_size = t->buffer->data_size;
trd->offsets_size = t->buffer->offsets_size;
trd->data.ptr.buffer = (binder_uintptr_t)
   ((uintptr_t)t->buffer->data +
   binder_alloc_get_user_buffer_offset(&proc->alloc));
trd->data.ptr.offsets = trd->data.ptr.buffer +
         ALIGN(t->buffer->data_size,
             sizeof(void *));

tr.secctx = t->security_ctx;
      if (t->security_ctx) {
            cmd = BR_TRANSACTION_SEC_CTX;
            trsize = sizeof(tr);
        }
        //回复给用户的响应码是BR_TRANSACTION 放到用户空间 也就是bwr.read_buffer在binder_ioctl_write_read
      if (put_user(cmd, (uint32_t __user *)ptr)) {
         if (t_from)
            binder_thread_dec_tmpref(t_from);

         binder_cleanup_transaction(t, "put_user failed",
                     BR_FAILED_REPLY);

         return -EFAULT;
      }
      ptr += sizeof(uint32_t);
      //把数据拷贝到用户空间 也就是bwr中去
      if (copy_to_user(ptr, &tr, trsize)) {
         if (t_from)
            binder_thread_dec_tmpref(t_from);

         binder_cleanup_transaction(t, "copy_to_user failed",
                     BR_FAILED_REPLY);

         return -EFAULT;
      }
      ptr += trsize;

      trace_binder_transaction_received(t);
      binder_stat_br(proc, thread, cmd);


      if (t_from)//临时引用计数器+1
         binder_thread_dec_tmpref(t_from);
      t->buffer->allow_user_free = 1;
      if (cmd != BR_REPLY && !(t->flags & TF_ONE_WAY)) {//cmd = BR_TRANSACTION 所以是true
         binder_inner_proc_lock(thread->proc);
         t->to_parent = thread->transaction_stack;//插入到事务栈中
         t->to_thread = thread;//设置目标处理进程
         thread->transaction_stack = t;
         binder_inner_proc_unlock(thread->proc);
      } else {
         binder_free_transaction(t);
      }
      break;
   }
```

从`cmd` = `BINDER_WORK_TRANSACTION`把数据取出来放入bwr.read\_buffer中去，把`cmd`修改成`BR_TRANSACTION`并 `中断循环`。 返回`binder_ioctl_write_read`

```
//把数据拷贝到ubuf中去  code = ADD_SERVICE_TRANSACTION  data=AMS
if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {
   ret = -EFAULT;
   goto out;
}
也就是拷贝到binder_ioctl的arg中 返回到service_manager层的bwr中
```

把数据拷贝到service\_manager中的bwr中去了，我们看看`service_manager`的`binder_loop`拿到数据怎么处理的

```
for (;;) {
    bwr.read_size = sizeof(readbuf);
    bwr.read_consumed = 0;
    bwr.read_buffer = (uintptr_t) readbuf;

    res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);

    if (res < 0) {
        ALOGE("binder_loop: ioctl failed (%s)\n", strerror(errno));
        break;
    }
    //调用binder_parse 对数据进行解析
    res = binder_parse(bs, 0, (uintptr_t) readbuf, bwr.read_consumed, func);
    if (res == 0) {
        ALOGE("binder_loop: unexpected reply?!\n");
        break;
    }
    if (res < 0) {
        ALOGE("binder_loop: io error %d %s\n", res, strerror(errno));
        break;
    }
}


//解析传递过来的数据
int binder_parse(struct binder_state *bs, struct binder_io *bio,
                 uintptr_t ptr, size_t size, binder_handler func)
{
    int r = 1;
    uintptr_t end = ptr + (uintptr_t) size;

    while (ptr < end) {
        uint32_t cmd = *(uint32_t *) ptr;
        ptr += sizeof(uint32_t);
        switch(cmd) {
       
        case BR_TRANSACTION: {//到这里来了
            struct binder_transaction_data_secctx txn;
            if (cmd == BR_TRANSACTION_SEC_CTX) {//这里不走
                if ((end - ptr) < sizeof(struct binder_transaction_data_secctx)) {
                    ALOGE("parse: txn too small (binder_transaction_data_secctx)!\n");
                    return -1;
                }
                memcpy(&txn, (void*) ptr, sizeof(struct binder_transaction_data_secctx));
                ptr += sizeof(struct binder_transaction_data_secctx);
            } else /* BR_TRANSACTION */ {
                if ((end - ptr) < sizeof(struct binder_transaction_data)) {
                    ALOGE("parse: txn too small (binder_transaction_data)!\n");
                    return -1;
                }
               //数据拷贝到txn的transaction_data
                memcpy(&txn.transaction_data, (void*) ptr, sizeof(struct binder_transaction_data));
                ptr += sizeof(struct binder_transaction_data);

                txn.secctx = 0;
            }

            binder_dump_txn(&txn.transaction_data);
            if (func) {//回调函数不为null 回调函数是 svcmgr_handle
                unsigned rdata[256/4];
                struct binder_io msg;
                struct binder_io reply;
                int res;
                //初始化reply
                bio_init(&reply, rdata, sizeof(rdata), 4);
                //解析数据
                bio_init_from_txn(&msg, &txn.transaction_data);
                res = func(bs, &txn, &msg, &reply);
                if (txn.transaction_data.flags & TF_ONE_WAY) {//这里不会来
                    binder_free_buffer(bs, txn.transaction_data.data.ptr.buffer);
                } else {
                //把处理结果返回
                    binder_send_reply(bs, &reply, txn.transaction_data.data.ptr.buffer, res);
                }
            }
            break;
        }
    }

    return r;
}

```

返回的`cmd`是`BR_TRANSACTION`通过`binder_parse`拿到返回的数据msg 返回给`service_manager`的`svcmgr_handler`。

```
int svcmgr_handler(struct binder_state *bs,
                   struct binder_transaction_data_secctx *txn_secctx,
                   struct binder_io *msg,
                   struct binder_io *reply)
{
    struct svcinfo *si;//创建svcinfo结构
    struct binder_transaction_data *txn = &txn_secctx->transaction_data;
    strict_policy = bio_get_uint32(msg);
    bio_get_uint32(msg);  // Ignore worksource header.
    s = bio_get_string16(msg, &len);//获取到服务的名字

    switch(txn->code) {//code就是 ADD_SERVICE_TRANSACTION 但是里面并没有ADD_SERVICE_TRANSACTION 我们看看它的值是多少? 在IServiceMananger.java中定义的值是3而SVC_MGR_ADD_SERVICE值也是3
 
    case SVC_MGR_ADD_SERVICE://进入到这里来
    //获取服务的名字
        s = bio_get_string16(msg, &len);
        if (s == NULL) {
            return -1;
        }
        handle = bio_get_ref(msg);//获取到AMS的handle
        allow_isolated = bio_get_uint32(msg) ? 1 : 0;
        dumpsys_priority = bio_get_uint32(msg);
        //调用do_add_service
        if (do_add_service(bs, s, len, handle, txn->sender_euid, allow_isolated, dumpsys_priority,
                           txn->sender_pid, (const char*) txn_secctx->secctx))
            return -1;
        break;


//添加服务
int do_add_service(struct binder_state *bs, const uint16_t *s, size_t len, uint32_t handle,
                   uid_t uid, int allow_isolated, uint32_t dumpsys_priority, pid_t spid, const char* sid) {
    struct svcinfo *si;

    si = find_svc(s, len);//从服务中查找
    if (si) {//如果存在
        if (si->handle) {//非ServiceManager就先death掉
            ALOGE("add_service('%s',%x) uid=%d - ALREADY REGISTERED, OVERRIDE\n",
                 str8(s, len), handle, uid);
            svcinfo_death(bs, si);
        }
        si->handle = handle;
    } else {//不存在的画就在堆上开辟svcinfo的空间
        si = malloc(sizeof(*si) + (len + 1) * sizeof(uint16_t));
         //指定handle为ams的handle
        si->handle = handle;
        si->len = len;
        memcpy(si->name, s, (len + 1) * sizeof(uint16_t));
        si->name[len] = '\0';
        si->death.func = (void*) svcinfo_death;
        si->death.ptr = si;
        si->allow_isolated = allow_isolated;
        si->dumpsys_priority = dumpsys_priority;
        //链接到链表后边 添加到svclist 
        si->next = svclist;
        svclist = si;
    }

    binder_acquire(bs, handle);
    binder_link_to_death(bs, handle, &si->death);
    return 0;
}


//获取到handle
uint32_t bio_get_ref(struct binder_io *bio)
{
    struct flat_binder_object *obj;

    obj = _bio_get_obj(bio);
    if (!obj)
        return 0;

    if (obj->hdr.type == BINDER_TYPE_HANDLE)
        return obj->handle;

    return 0;
}
```

最终执行到`service_manager`的`do_add_service`把AMS添加到svclist中去。然后调用`binder_send_reply`把结果返回.

```
//返回结果 让客户端释放buffer和执行bc_replay
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

    data.cmd_free = BC_FREE_BUFFER;//释放buffer
    data.buffer = buffer_to_free;
    data.cmd_reply = BC_REPLY;//设置cmd_reply为BC_REPLY
    data.txn.target.ptr = 0;//回复的ptr 都是0
    data.txn.cookie = 0;//cookie 都是0
    data.txn.code = 0;//code 都是0
    if (status) {//status == 0
        data.txn.flags = TF_STATUS_CODE;
        data.txn.data_size = sizeof(int);
        data.txn.offsets_size = 0;
        data.txn.data.ptr.buffer = (uintptr_t)&status;
        data.txn.data.ptr.offsets = 0;
    } else {
        data.txn.flags = 0;
        data.txn.data_size = reply->data - reply->data0;
        data.txn.offsets_size = ((char*) reply->offs) - ((char*) reply->offs0);
        data.txn.data.ptr.buffer = (uintptr_t)reply->data0;
        data.txn.data.ptr.offsets = (uintptr_t)reply->offs0;
    }
    binder_write(bs, &data, sizeof(data));
}
```

然后执行到Binder驱动的`binder_thread_read`中命令是`BINDER_WRITE_READ`嘛。执行到`binder_thread_write`命令是`BC_FREE_BUFFER`

```
case BC_FREE_BUFFER: {
   binder_uintptr_t data_ptr;
   struct binder_buffer *buffer;

   if (get_user(data_ptr, (binder_uintptr_t __user *)ptr))
      return -EFAULT;
   ptr += sizeof(binder_uintptr_t);
    //释放内存空间
   buffer = binder_alloc_prepare_to_free(&proc->alloc,
                     data_ptr);
      break;
   }
   释放完成之后执行BC_REPLY
   
case BC_REPLY: {
   struct binder_transaction_data tr;

   if (copy_from_user(&tr, ptr, sizeof(tr)))
      return -EFAULT;
   ptr += sizeof(tr);
   binder_transaction(proc, thread, &tr,
            cmd == BC_REPLY, 0);
   break;
}
```

释放完成之后接着执行`BC_REPLY`,调用`binder_transaction` `cmd`是`BC_REPLY`所以是`true`。 进入`binder_transaction`之前reply是false 现在是`true`

```
if (reply) {
   binder_inner_proc_lock(proc);
  //拿到此时service_manager的transaction_stack
   in_reply_to = thread->transaction_stack;
   if (in_reply_to == NULL) {//不是null
   }
   if (in_reply_to->to_thread != thread) {
   }
   //将这个transaction_stack移出链表
   thread->transaction_stack = in_reply_to->to_parent;
   binder_inner_proc_unlock(proc);
   //得到回应的进程
   target_thread = binder_get_txn_from_and_acq_inner(in_reply_to);
   if (target_thread == NULL) {
   }
   if (target_thread->transaction_stack != in_reply_to) {
   }
   //通过target_thread得到proc binder_proc
   target_proc = target_thread->proc;
   atomic_inc(&target_proc->tmp_ref);
   binder_inner_proc_unlock(target_thread->proc);
}

```

找到目标进城之后处理逻辑和之前service\_manager一样了会开辟内存拷贝数据，创建`tcomplete`和`t`。

```
if (reply) {
//把tcomplete插入到service_manager的todo中
   binder_enqueue_thread_work(thread, tcomplete);
   binder_inner_proc_lock(target_proc);
   if (target_thread->is_dead) {
      binder_inner_proc_unlock(target_proc);
      goto err_dead_proc_or_thread;
   }
   BUG_ON(t->buffer->async_transaction != 0);
   //把service_manager从目标thread的transaction中删除
   binder_pop_transaction_ilocked(target_thread, in_reply_to);
   binder_enqueue_thread_work_ilocked(target_thread, &t->work);
   binder_inner_proc_unlock(target_proc);
   //唤起AMS的wait队列
   wake_up_interruptible_sync(&target_thread->wait);
   binder_restore_priority(current, in_reply_to->saved_priority);
   binder_free_transaction(in_reply_to);
}


binder_transaction(
case BINDER_TYPE_HANDLE://之前修改成了binder_type_hadnle
case BINDER_TYPE_WEAK_HANDLE: {
   struct flat_binder_object *fp;

   fp = to_flat_binder_object(hdr);
   //调用binder_translate_handle
   ret = binder_translate_handle(fp, t, thread);
   if (ret < 0) {
      return_error = BR_FAILED_REPLY;
      return_error_param = ret;
      return_error_line = __LINE__;
      goto err_translate_failed;
   }
} break;



static int binder_translate_handle(struct flat_binder_object *fp,
               struct binder_transaction *t,
               struct binder_thread *thread)
{
   struct binder_proc *proc = thread->proc;
   struct binder_proc *target_proc = t->to_proc;
   struct binder_node *node;
   struct binder_ref_data src_rdata;
   int ret = 0;

   node = binder_get_node_from_ref(proc, fp->handle,
         fp->hdr.type == BINDER_TYPE_HANDLE, &src_rdata);
   if (!node) {
      binder_user_error("%d:%d got transaction with invalid handle, %d\n",
              proc->pid, thread->pid, fp->handle);
      return -EINVAL;
   }
   if (security_binder_transfer_binder(proc->tsk, target_proc->tsk)) {
      ret = -EPERM;
      goto done;
   }

   binder_node_lock(node);
   if (node->proc == target_proc) {//不走这里 是本进程 服务端 因为也有可能自己和自己ipc通信  node->proc就是service_manager target_proc就是ServiceManagerProxy(system_server)
   } else {//走这里
      struct binder_ref_data dest_rdata;

      binder_node_unlock(node);
      //把servciemanager进程的binder_node拷贝到本进程来 
      ret = binder_inc_ref_for_node(target_proc, node,
            fp->hdr.type == BINDER_TYPE_HANDLE,
            NULL, &dest_rdata);
      if (ret)
         goto done;

      fp->binder = 0;
      //替换成自己进程的handle值
      fp->handle = dest_rdata.desc;
      fp->cookie = 0;
      trace_binder_transaction_ref_to_ref(t, node, &src_rdata,
                      &dest_rdata);
      binder_debug(BINDER_DEBUG_TRANSACTION,
              "        ref %d desc %d -> ref %d desc %d (node %d)\n",
              src_rdata.debug_id, src_rdata.desc,
              dest_rdata.debug_id, dest_rdata.desc,
              node->debug_id);
   }
}



再看看service_mananger接收到
binder.c
case BINDER_WORK_TRANSACTION_COMPLETE: {
   binder_inner_proc_unlock(proc);
   cmd = BR_TRANSACTION_COMPLETE;
   if (put_user(cmd, (uint32_t __user *)ptr))
      return -EFAULT;
   ptr += sizeof(uint32_t);

   binder_stat_br(proc, thread, cmd);
   binder_debug(BINDER_DEBUG_TRANSACTION_COMPLETE,
           "%d:%d BR_TRANSACTION_COMPLETE\n",
           proc->pid, thread->pid);
   kfree(w);
   binder_stats_deleted(BINDER_STAT_TRANSACTION_COMPLETE);
} 

service_manager.c
case BR_TRANSACTION_COMPLETE break;
`
```

至此`service_mananger`的处理和返回完成。

再看看客户端怎么处理的？唤醒客户端后从`binder_thread_read`继续执行，首先拿到的是`todo`中的`cmd` = `BINDER_WORK_TRANSACTION`以及之前和服务端通信之前加入的`BINDER_WORK_TRANSACTION_COMPLETE`

```
case BINDER_WORK_TRANSACTION_COMPLETE: {
   binder_inner_proc_unlock(proc);
   cmd = BR_TRANSACTION_COMPLETE;
   //把BR_TRANSACTION_COMPLETE存到用户空间
   if (put_user(cmd, (uint32_t __user *)ptr))
      return -EFAULT;
   ptr += sizeof(uint32_t);

   binder_stat_br(proc, thread, cmd);
   binder_debug(BINDER_DEBUG_TRANSACTION_COMPLETE,
           "%d:%d BR_TRANSACTION_COMPLETE\n",
           proc->pid, thread->pid);
   kfree(w);
   binder_stats_deleted(BINDER_STAT_TRANSACTION_COMPLETE);
} break;


```

然后就到了`talkWithDriver`,没干啥,接着调用`talkWithDriver`

```
//清空mOut,把数据写入mIn
if (err >= NO_ERROR) {
    if (bwr.write_consumed > 0) {
        if (bwr.write_consumed < mOut.dataSize())
            mOut.remove(0, bwr.write_consumed);
        else {
            mOut.setDataSize(0);
            processPostWriteDerefs();
        }
    }
    if (bwr.read_consumed > 0) {
        mIn.setDataSize(bwr.read_consumed);
        mIn.setDataPosition(0);
    }
    return NO_ERROR;
}
```

把`mOut`数据清空，把返回的数据写入`mIn`,然后继续调用`talkWithDriver`,注意此时的命令是 `BR_TRANSACTION_COMPLETE`。

```

const bool needRead = mIn.dataPosition() >= mIn.dataSize();//false

const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0; //返回0

if (doReceive && needRead) {
    bwr.read_size = mIn.dataCapacity();
    bwr.read_buffer = (uintptr_t)mIn.data();
} else {
    bwr.read_size = 0;
    bwr.read_buffer = 0;
}

//读取到了这条命令 
case BR_TRANSACTION_COMPLETE:
    if (!reply && !acquireResult) goto finish;
    break;
```

然后继续调用`talkWithDriver再次进去 if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)`继续进入`binder_thread_read` 客户端 进入`休眠`等待

接着收到了服务端的 `BINDER_WORK_TRANSACTION` 唤醒客户端的binder驱动:

```
case BINDER_WORK_TRANSACTION: {
       binder_inner_proc_unlock(proc);
       //根据binder_work找到binder_transaction结构 
       t = container_of(w, struct binder_transaction, work);
      }
      break;
      }

if (!t)//t!=null
   continue;

if (t->buffer->target_node) {//这里是null 所以走下边
   struct binder_node *target_node = t->buffer->target_node;
   struct binder_priority node_prio;
   trd->target.ptr = target_node->ptr;
   trd->cookie =  target_node->cookie;
   node_prio.sched_policy = target_node->sched_policy;
   node_prio.prio = target_node->min_priority;
   binder_transaction_priority(current, t, node_prio,
                target_node->inherit_rt);
   cmd = BR_TRANSACTION;
}else{
trd->target.ptr = 0;
trd->cookie = 0;
cmd = BR_REPLY; 给用户空间存放了BR_REPLY 执行到BR_REPLY
}


trd->code = t->code;//记录code也就是ADD_SERVICE_TRANSACTION
trd->flags = t->flags;
trd->sender_euid = from_kuid(current_user_ns(), t->sender_euid);
//记录from也就是发起进程 并且引用计数+1
t_from = binder_get_txn_from(t);

if (t_from) {
   struct task_struct *sender = t_from->proc->tsk;//记录客户端的进程信息
   trd->sender_pid =
      task_tgid_nr_ns(sender,
            task_active_pid_ns(current));
}
//设置数据大小 偏移量 设置数据区的首地址 也就是通过内核空间和用户空间的偏移量算出来的
trd->data_size = t->buffer->data_size;
trd->offsets_size = t->buffer->offsets_size;
trd->data.ptr.buffer = (binder_uintptr_t)
   ((uintptr_t)t->buffer->data +
   binder_alloc_get_user_buffer_offset(&proc->alloc));
trd->data.ptr.offsets = trd->data.ptr.buffer +
         ALIGN(t->buffer->data_size,
             sizeof(void *));

tr.secctx = t->security_ctx;
      if (t->security_ctx) {
            cmd = BR_TRANSACTION_SEC_CTX;
            trsize = sizeof(tr);
        }
        //回复给用户的响应码是BR_TRANSACTION 放到用户空间 也就是bwr.read_buffer在binder_ioctl_write_read
      if (put_user(cmd, (uint32_t __user *)ptr)) {
         if (t_from)
            binder_thread_dec_tmpref(t_from);

         binder_cleanup_transaction(t, "put_user failed",
                     BR_FAILED_REPLY);

         return -EFAULT;
      }
      ptr += sizeof(uint32_t);
      //把数据拷贝到用户空间 也就是bwr中去
      if (copy_to_user(ptr, &tr, trsize)) {
         if (t_from)
            binder_thread_dec_tmpref(t_from);

         binder_cleanup_transaction(t, "copy_to_user failed",
                     BR_FAILED_REPLY);

         return -EFAULT;
      }
      ptr += trsize;

      trace_binder_transaction_received(t);
      binder_stat_br(proc, thread, cmd);


      if (t_from)//临时引用计数器+1
         binder_thread_dec_tmpref(t_from);
      t->buffer->allow_user_free = 1;
      if (cmd != BR_REPLY && !(t->flags & TF_ONE_WAY)) {//cmd = BR_TRANSACTION 所以是true
         binder_inner_proc_lock(thread->proc);
         t->to_parent = thread->transaction_stack;//插入到事务栈中
         t->to_thread = thread;//设置目标处理进程
         thread->transaction_stack = t;
         binder_inner_proc_unlock(thread->proc);
      } else {
         binder_free_transaction(t);
      }
      break;
   }

//客户端拿到
//拿到服务端的返回结果
case BR_REPLY:
    {
        binder_transaction_data tr;
        err = mIn.read(&tr, sizeof(tr));//从in中获取数据
        ALOG_ASSERT(err == NO_ERROR, "Not enough command data for brREPLY");
        if (err != NO_ERROR) goto finish;

        if (reply) {
            if ((tr.flags & TF_STATUS_CODE) == 0) {//在这里执行
                reply->ipcSetDataReference(//执行
                    reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                    tr.data_size,
                    reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                    tr.offsets_size/sizeof(binder_size_t),
                    freeBuffer, this);
            } else {
                err = *reinterpret_cast<const status_t*>(tr.data.ptr.buffer);
                freeBuffer(nullptr,
                    reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                    tr.data_size,
                    reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                    tr.offsets_size/sizeof(binder_size_t), this);
            }
        } else {
            freeBuffer(nullptr,
                reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                tr.data_size,
                reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                tr.offsets_size/sizeof(binder_size_t), this);
            continue;
        }
    }
    goto finish;
    
    
    
    //把数据拷贝到parcel中
void Parcel::ipcSetDataReference(const uint8_t* data, size_t dataSize,
    const binder_size_t* objects, size_t objectsCount, release_func relFunc, void* relCookie)
{
    binder_size_t minOffset = 0;
    freeDataNoInit();
    mError = NO_ERROR;
    mData = const_cast<uint8_t*>(data);
    mDataSize = mDataCapacity = dataSize;
    //ALOGI("setDataReference Setting data size of %p to %lu (pid=%d)", this, mDataSize, getpid());
    mDataPos = 0;
    ALOGV("setDataReference Setting data pos of %p to %zu", this, mDataPos);
    mObjects = const_cast<binder_size_t*>(objects);
    mObjectsSize = mObjectsCapacity = objectsCount;
    mNextObjectHint = 0;
    mObjectsSorted = false;
    mOwner = relFunc;
    mOwnerCookie = relCookie;
    for (size_t i = 0; i < mObjectsSize; i++) {
        binder_size_t offset = mObjects[i];
        if (offset < minOffset) {
            ALOGE("%s: bad object offset %" PRIu64 " < %" PRIu64 "\n",
                  __func__, (uint64_t)offset, (uint64_t)minOffset);
            mObjectsSize = 0;
            break;
        }
        minOffset = offset + sizeof(flat_binder_object);
    }
    scanForFds();
}


```

把返回的数据拷贝到parcel中，然后返回`IPCThreadState::transact`返回到`BpBinder::transact` 再返回到 `android_os_BinderProxy_transact`返回到java层BinderProxy.java的 `transact`函数,最后返回到`ServiceManager`对应的`addService`中,没有返回值 执行完毕。

## 服务的获取

我们再看看有返回值的.

`服务端`的处理:

```
case SVC_MGR_GET_SERVICE:
case SVC_MGR_CHECK_SERVICE:
    s = bio_get_string16(msg, &len);
    if (s == NULL) {
        return -1;
    }
    handle = do_find_service(s, len, txn->sender_euid, txn->sender_pid,
                             (const char*) txn_secctx->secctx);
    if (!handle)
        break;
        //给当前reply 设置了handle值
    bio_put_ref(reply, handle);
    return 0;

void bio_put_ref(struct binder_io *bio, uint32_t handle)
{
    struct flat_binder_object *obj;

    if (handle)
        obj = bio_alloc_obj(bio);
    else
        obj = bio_alloc(bio, sizeof(*obj));

    if (!obj)
        return;

    obj->flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    obj->hdr.type = BINDER_TYPE_HANDLE;//设置type为
    obj->handle = handle;//handle值是上边查询到的
    obj->cookie = 0;
}
```

```
public static IBinder getService(String name) {
    try {
        IBinder service = sCache.get(name);//先从缓存找
        if (service != null) {
            return service;
        } else {//在从binder找
            return Binder.allowBlocking(rawGetService(name));
        }
    } catch (RemoteException e) {
        Log.e(TAG, "error in getService", e);
    }
    return null;
}



public IBinder getService(String name) throws RemoteException {
    Parcel data = Parcel.obtain();
    Parcel reply = Parcel.obtain();
    data.writeInterfaceToken(IServiceManager.descriptor);
    data.writeString(name);
    mRemote.transact(GET_SERVICE_TRANSACTION, data, reply, 0);
    IBinder binder = reply.readStrongBinder();//调用readStrongBinder
    reply.recycle();
    data.recycle();
    return binder;
}


static jobject android_os_Parcel_readStrongBinder(JNIEnv* env, jclass clazz, jlong nativePtr)
    {//拿到native的parcel对象
    Parcel* parcel = reinterpret_cast<Parcel*>(nativePtr);
    if (parcel != NULL) {
    //包装成java层的BinderProxy返回
        return javaObjectForIBinder(env, parcel->readStrongBinder());
    }
    return NULL;
}



jobject javaObjectForIBinder(JNIEnv* env, const sp<IBinder>& val)
{
    if (val == NULL) return NULL;

    if (val->checkSubclass(&gBinderOffsets)) { // == false
        // It's a JavaBBinder created by ibinderForJavaObject. Already has Java object.
        jobject object = static_cast<JavaBBinder*>(val.get())->object();
        LOGDEATH("objectForBinder %p: it's our own %p!\n", val.get(), object);
        return object;
    }

    BinderProxyNativeData* nativeData = new BinderProxyNativeData();
    nativeData->mOrgue = new DeathRecipientList;
    nativeData->mObject = val;

    //BinderProxy.getInstance mNativeData   nativeData
    jobject object = env->CallStaticObjectMethod(gBinderProxyOffsets.mClass,
            gBinderProxyOffsets.mGetInstance, (jlong) nativeData, (jlong) val.get());
    if (env->ExceptionCheck()) {
        // In the exception case, getInstance still took ownership of nativeData.
        return NULL;
    }
    BinderProxyNativeData* actualNativeData = getBPNativeData(env, object);
    if (actualNativeData == nativeData) {//是同一个创建一个新的Proxy
        // Created a new Proxy
        uint32_t numProxies = gNumProxies.fetch_add(1, std::memory_order_relaxed);
        uint32_t numLastWarned = gProxiesWarned.load(std::memory_order_relaxed);
        if (numProxies >= numLastWarned + PROXY_WARN_INTERVAL) {
            // Multiple threads can get here, make sure only one of them gets to
            // update the warn counter.
            if (gProxiesWarned.compare_exchange_strong(numLastWarned,
                        numLastWarned + PROXY_WARN_INTERVAL, std::memory_order_relaxed)) {
                ALOGW("Unexpectedly many live BinderProxies: %d\n", numProxies);
            }
        }
    } else {
        delete nativeData;
    }

    return object;
}



sp<IBinder> Parcel::readStrongBinder() const
{
    sp<IBinder> val;
    readNullableStrongBinder(&val);
    return val;
}
status_t Parcel::readNullableStrongBinder(sp<IBinder>* val) const
{
    return unflatten_binder(ProcessState::self(), *this, val);
}


status_t unflatten_binder(const sp<ProcessState>& proc,
    const Parcel& in, sp<IBinder>* out)
{
    const flat_binder_object* flat = in.readObject(false);

    if (flat) {
        switch (flat->hdr.type) {
            case BINDER_TYPE_BINDER:
                *out = reinterpret_cast<IBinder*>(flat->cookie);
                return finish_unflatten_binder(nullptr, *flat, in);
            case BINDER_TYPE_HANDLE://返回的是这个 根据handle再去找
                *out = proc->getStrongProxyForHandle(flat->handle);
                return finish_unflatten_binder(
                    static_cast<BpBinder*>(out->get()), *flat, in);
        }
    }
    return BAD_TYPE;
}

sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);

    handle_entry* e = lookupHandleLocked(handle);//肯定是没有的 所以会新建一个

    if (e != nullptr) {
        IBinder* b = e->binder;
        if (b == nullptr || !e->refs->attemptIncWeak(this)) {
            if (handle == 0) {//现在就不是0了
                Parcel data;
                status_t status = IPCThreadState::self()->transact(
                        0, IBinder::PING_TRANSACTION, data, nullptr, 0);
                if (status == DEAD_OBJECT)
                   return nullptr;
            }
            //塞进查找出来的handle值 后期调用在binder层通过handle来找
            b = BpBinder::create(handle);
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
            // This little bit of nastyness is to allow us to add a primary
            // reference to the remote proxy when this team doesn't have one
            // but another team is sending the handle to us.
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }

    return result;
}

//没干啥
inline static status_t finish_unflatten_binder(
    BpBinder* /*proxy*/, const flat_binder_object& /*flat*/,
    const Parcel& /*in*/)
{
    return NO_ERROR;
}

```

带返回值的也看完了。至此`ServiceManager`的添加获取都结束了.其中逻辑比较弯弯绕，不知道大家看没看明白，如果没看明白，可以参考视频，如果视频有讲错的地方，也欢迎大家批评指正，一起讨论，一起进步。

## 总结:

## 图片总结

### 1.service\_mananger流程

![service_manager.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c7eb216f42884406aca64c509c8f215c~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

### 2.Binder通信流程（ServiceManager.addService）

![service_manager_addService.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/879e0c4b5372499c96b7e6e60a36f390~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

### 3.Binder的cmd流程图

![命令流程图.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1956043df244497fba88b7f8ca5387be~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

### 4.关系模型图

![binder关系图.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f415ac3bd19a4e10bcc766188d4f7046~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 文字总结

### 1.service\_manager

`service_mananger`就四个重要的:

1.`binder_open`:打开`binder`驱动,对`BINDER_VERSION`进行校验

2.`binder_become_context_manager`:设置自己成为binder上下文的管理者(`binder_context_mgr_node`)

3.`binder_loop`:给`Binder`驱动发送`BC_ENTER_LOOPER`，以及循环读取内容 调用`binder_parse`解析

4.`binder_parse`:根据返回数据解析命令，调用`svcmgr_handler`回调处理

### 2.Binder

1.`binder_open`:创建`binder_proc`并且初始化`todo`,`wait`队列

2.`binder_mmap`:开辟内核空间,同时开辟物理内存空间，然后把`内核空间和物理空间进行映射`，使他们2个指向`同一个内存地址`。

3.`binder_ioctl`:根据传入的cmd执行对应的操作 例如`BINDER_VERSION`、`BINDER_WRITE_READ`、`BINDER_SET_CONTEXT_MGR_EXT`等等

关于Java层的ServiceManager 这个就是串通整个Binder流程，我们可以参考上面的流程图。

## 面试

Q1: 能不能介绍下Binder是什么？原理是什么样的？

A1:Binder是一个虚拟的硬件设备，是一个驱动。是Android中特有的一种通信方式。好处就是一次内存拷贝，安全性高。 它高效的原理是将`用户空间、内核空间、物理内存映射在了同一块地址`，这样我们就少了一次拷贝。大家也可以参考我前面列出来的管道、socket、共享内存之间的区别来讲。也可以讲讲`binder_open`、`binder_mmap`、`binder_ioct`

Q2:能否说一下Binder是如何工作的？

这个可以聊得就太多了，长篇大论，我们可以从几个方面来讲 一个从`cmd`命令流的形式来讲。 另外一个我们可以从数据流的形式来讲。参考我上边画的两个图。

Q3:Binder给每个应用分配的内存是多少？如果不够用怎么办？

A3:Binder给每个应用默认分配1M-8k,最大支持4M。如果不够用可以考虑用其他的通信方式。比如共享内存、socket等等，我们可以根据我画的几种IPC来讲下区别。

Q4:Binder线程数是多少？

A4:16个

好了，Android中复杂都Binder，ServiceManager我们讲完了，这次横跨了驱动层，FrameWork，Java层。希望打开能够看懂

在线视频:

[www.bilibili.com/video/BV1RT…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1RT411q7WQ%2F%3Fvd_source%3D689a2ec078877b4a664365bdb60362d3 "https://www.bilibili.com/video/BV1RT411q7WQ/?vd_source=689a2ec078877b4a664365bdb60362d3")