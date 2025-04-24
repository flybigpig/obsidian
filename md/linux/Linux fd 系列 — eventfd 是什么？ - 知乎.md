---
created: 2025-04-24T09:35:31 (UTC +08:00)
tags: [iNode,文件系统,Linux]
source: https://zhuanlan.zhihu.com/p/393748176
author: 关于作者qiya公众号：奇伢云存储。专注云存储、云计算 Linux/c/go回答51文章45关注者1,709关注他发私信
---

# Linux fd 系列 — eventfd 是什么？ - 知乎

> ## Excerpt
> 原创不易，欢迎关注：奇伢云存储一切皆文件，但 fd 区分类型？eventfd 是什么的？怎么使用它呢？句柄创建eventfd api 调用？监听 fd通常的用途磁盘的异步 IO（ libaio ）kvm 的 ioeventfd 机制还有什么朴实的用法…

---
> 原创不易，欢迎关注：奇伢云存储

-   [一切皆文件，但 fd 区分类型？](https://zhuanlan.zhihu.com/write)
-   [eventfd 是什么的？](https://zhuanlan.zhihu.com/write)
-   [怎么使用它呢？](https://zhuanlan.zhihu.com/write)

-   [句柄创建](https://zhuanlan.zhihu.com/write)
-   [eventfd api 调用？](https://zhuanlan.zhihu.com/write)
-   [监听 fd](https://zhuanlan.zhihu.com/write)

-   [通常的用途](https://zhuanlan.zhihu.com/write)

-   [磁盘的异步 IO（ libaio ）](https://zhuanlan.zhihu.com/write)
-   [kvm 的 ioeventfd 机制](https://zhuanlan.zhihu.com/write)
-   [还有什么朴实的用法？](https://zhuanlan.zhihu.com/write)

-   [总结](https://zhuanlan.zhihu.com/write)
-   [后记](https://zhuanlan.zhihu.com/write)

![](https://pic4.zhimg.com/v2-b71afb03a8e0f8a4fb9b2fdc07671dc3_1440w.jpg)

在上一期 **[深入理解 Linux Epoll 池](https://zhuanlan.zhihu.com/write)** 中随便对 eventfd 提了一嘴，这是一个很妙的 fd 。下面娓娓道来。

Linux 一切皆文件，但这个文件 fd 也是有类型的，绝大部分人都知道“文件 fd”，知道 socket fd，甚至知道 pipe fd，可能都不知道 fd 还有这么一种叫做 `eventfd` 的类型。

## **eventfd 是什么的？**

不妨拆开来看，event fd ，也就是事件 fd 类型。故名思义，就是专门用于事件通知的文件描述符（ fd ）。很多人可能没怎么用，但是用过的人都说：香 ！

哪个版本引入的？

Linux 2.6.22

代码位于：`fs/eventfd.c`

“事件传递”就是通信嘛。eventfd 不仅可以用于进程间的通信，还能用于用户态和内核态的通信。

思考一个小问题：我们知道“文件”里是保存东西的，eventfd 既然对应了一个“文件”，那么这个“文件”的内容是什么呢？

**划重点：eventfd 是一个计数相关的fd**。计数不为零是有**可读事件**发生，`read` 之后计数会清零，`write` 则会递增计数器。

这个怎么理解？

在之前自制文件系统系列中提到过：文件系统的“文件”是抽象的概念，你看到的一切知识文件系统想让你看到的东西。比如 hellofs 中我们没写过任何数据，也会返回 “hello world” 的内容。这个仅仅 hook 到 read/write 调用，然后根据逻辑返回数据而已。

eventfd 也是如此，eventfd 实现了 read/write 的调用，在调用里面实现了一套计数器的逻辑。write 仅仅是加计数，read 是读计数，并且清零。

长什么样子呢？笔者找了个进程来观摩下。

```
root@ubuntu:~# ll /proc/14168/fd
lrwx------ 1 root root 64 Jul 10 22:12 3 -> anon_inode:[eventfd]
```

在 Linux 的 `/proc` 下每个进程都会有个目录，目录名为进程 ID 号，在这个目录能看到使用的资源信息，其中有个 fd 目录，就是进程打开的所有文件。看出猫腻了不？有个叫做 `[eventfd]` 的 fd 句柄。

## **怎么使用它呢？**

### **句柄创建**

```
#include <sys/eventfd.h>
int eventfd(unsigned int initval, int flags);
```

举个栗子：

```
efd = eventfd(0, 0);
if (efd == -1)
    handle_error("eventfd");
```

这样就创建出了一个 eventfd 类型的 fd 啦。会在你的 `/proc/${pid}/fd/` 目录中有一个 eventfd 类型的句柄。

### **eventfd api 调用？**

eventfd new 出来之后，总结来说，可以对它做四个事情：

1.  可以读这个 fd；
2.  可以写这个 fd；
3.  可以监听这个 fd；
4.  可以关闭这个 fd；

我怎么知道这个知识点的？

因为在 Linux 内核代码中，我看到了呀。eventfd 就实现了这几个调用。

```
static const struct file_operations eventfd_fops = {
#ifdef CONFIG_PROC_FS
    .show_fdinfo = eventfd_show_fdinfo,
#endif
    .release = eventfd_release,
    .poll  = eventfd_poll,
    .read  = eventfd_read,
    .write  = eventfd_write,
    .llseek  = noop_llseek,
};
```

很明显就能看到以上实现的几个调用就是 eventfd 全部的内容所在。

### **读写 fd**

简单看下 eventfd 的读写究竟做了什么？

eventfd 对应的文件内容是一个 8 字节的数字，这个数字是 read/write 操作维护的计数。

首先，write 的时候，累加计数，read 的时候读取计数，并且清零。

```
uint64_t u;
ssize_t n;

// 写 eventfd，内部 buffer 必须是 8 字节大小；
n = write(efd, &u, sizeof(uint64_t));

// 读 eventfd
n = read(efd, &u, sizeof(uint64_t));
```

读写也就是 read/write，读写这个 fd 很容易理解，但是请注意了，只能 8 个字节。这个读写的内容其实是计数。

举个栗子：如下，我们连续写 3 次

```
// 写 3 次
write(efd, &u /* u = 1 */ , 8)
write(efd, &u /* u = 2 */ , 8)
write(efd, &u /* u = 3 */ , 8)
```

你猜猜读的时候，是多少？

读到的值是 6（因为 1+2+3），理解了吧。

![动图封面](https://pic1.zhimg.com/v2-1aeeec8d6878ce1c1d4502a52ff01c6e_b.jpg)

小结：

1.  写的时候，写进去一个 8 字节的整数，eventfd 实现的逻辑是累计计数；
2.  读的时候，读到总计数，并且会清零；
3.  实现在 `eventfd_write` 和 `eventfd_read` 函数中；

### **监听 fd**

在 **[深入理解 Linux Epoll 池](https://zhuanlan.zhihu.com/write)** 提到过，不是所有的 fd 类型都可用 epoll 池来监听事件的，只有实现了 `file_operation->poll` 的调用的“文件” fd 才能被 epoll 管理。eventfd 刚好就实现了这个接口。

eventfd 是专门用来传递事件的 fd ，而 epoll 池则是专门用来管理事件的池子，它们两结合就妙了。

我们知道 epoll 监听的是**可读可写事件**。那么你想过 eventfd 的可读可写事件是啥吗？

“**可读可写事件**”这是个有趣的问题，我们可以去发散下，对比思考下 socket fd，文件 fd：

-   socket fd：可以写入发送数据，那么触发可写事件，网卡数据来了，可以读，触发可读事件；
-   文件 fd：文件 fd 的可读可写事件就更有意思了，因为文件一直是可写的，所以一直都触发可写事件，文件里的数据也一直是可读的，所以一直触发可读事件。这个也是为什么类似 ext4 这种文件不实现 poll 接口的原因。**因为文件 fd 一直是可读可写的，poll 监听没有任何意义；**

回到最初问题：eventfd 呢？它的可读可写事件是什么？

我们之前说过，eventfd 实现的是计数的功能。所以 eventfd 计数不为 0 ，那么 fd 是可读的。

由于 eventfd 一直可写（可以一直累计计数），所以一直有可写事件。

所以，这里有个什么隐藏知识点呢？

**eventfd 如果用 epoll 监听事件，那么都是监听读事件，因为监听写事件无意义。**

### **关闭 fd**

关闭这个很容里理解，就是不需要这个 fd 了，主动调用一把 Close ，当没有人使用的时候，内核会释放这个 fd 的资源。

### **fd 的阻塞属性**

我们知道读写 fd 的时候，可能会遇到阻塞，对于 socket fd 来说，没有数据的时候来读，则会阻塞。写 buffer 满了的时候来写，则会阻塞。

那么对于 eventfd 呢？它的阻塞有可能是怎么样的？

read eventfd 的时候，**如果计数器的值为 0，就会阻塞**（这种就等同于没“文件”内容）。

这种可以设置 fd 的属性为非阻塞类型，这样读的时候，如果计数器为 0 ，返回 EAGAIN 即可，这样就不会阻塞整个系统。

## **通常的用途**

单独的 eventfd 看似平平无奇，但其实有非常重要的应用。下面列举几个小例子：

### **磁盘的异步 IO（ libaio ）**

我们之前说过，类似于 ext4 这种文件系统的文件 fd ，其实是不能用 epoll 来管理的，网络 fd 才可以。因为磁盘文件一直可读可写。

难道文件就自绝于此吗？用不了事件机制吗？只能同步 IO 吗？

非也。Linux 内核提供了一个叫做 libaio 的机制，能够同时提交多个 io 请求给内核（这种批量递交能提高优化的概率，大量IO堆积到设备的队列中时, 内核可以发挥 IO 调度算法的优势,比如合并 IO 等）。

aio 请求完成之后，走异步的事件通知。这个事件通知的原理就是把一个 eventfd 和这个 aio 的上下文绑定起来。aio 完成，就会往 eventfd 里面写计数，从而触发可读事件。

### **kvm 的 ioeventfd 机制**

QEMU 可以将 VM 特定地址关联一个 eventfd，对进行监听，当Guest 进行 IO 操作 exit 到 kvm 后，kvm 可以判断本次exit 是否发生在这段特定地址中，如果是则会通过使用 eventfd 进行事件通知，进行 IO 操作，这种方式对比能节省一些时间。

### **还有什么朴实的用法？**

最简单的例子，一个消费者和多个生产者，这种就可以借助 eventfd 优雅的完成事件通知。

生产者：

是多个线程，会把请求投递到一个 list 中，然后唤醒生产者。

```
producer:
    // 投递请求到链表
    list_add( global_list, request )
    // 唤醒消费者处理
    write(eventfd, &cnt /* 1 */ , 8)
```

消费者：

是一个线程，后台 loop 处理。使用 epoll 监听 eventfd 的可读事件，这样能做到一旦有请求入队，消费者就立马唤醒处理。

```
consumer 
    // 添加 eventfd 到监听池
    epoll_ctl(ep, EPOLL_CTL_ADD, eventfd, &ee);

loop:
    // 等待唤醒
    epoll_wait(ep, ... );
    
    // 读取新添加到列表里的元素个数，并且进行处理；
    n = read(eventfd, ... )
    // 遍历链表处理
    for each global_list:
        // do something
```

## **总结**

1.  Linux 一切皆文件，但 fd 各有不同；
2.  eventfd 实现了 read/write 的接口，本质是一个计数器的实现；
3.  eventfd 实现了 poll 接口，所以可以和 epoll 双剑合璧，实现事件的通知管理；
4.  eventfd 可以和 libaio & epoll 一起，实现 Linux 下的纯异步 IO；
5.  eventfd 监听可读事件才有意义；
6.  ext4 这种文件 fd 一直可读可写，所以实现 poll 毫无意义。eventfd 一直可写，所以监听可写毫无意义；
7.  eventfd 可以结合业务，做一个事件通知的通信机制，非常巧妙；

## **后记**

哈哈，奇伢想把 Linux 的句柄类型写个遍，你觉得呢？你最想看了解的是哪种类型的句柄？

> 原创不易，欢迎关注：奇伢云存储
