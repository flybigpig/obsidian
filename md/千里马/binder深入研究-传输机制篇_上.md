## 1 Binder 是如何做到精确打击的？

```
我们先问一个问题，binder 机制到底是如何从代理对象找到其对应的 binder 实体呢？难道它有某种制导装置吗？要回答这个问题，我们只能静下心来研究 binder 驱动的代码。在本系列文档的初始篇中，我们曾经介绍过 ProcessState，这个结构是属于应用层次的东西，仅靠它当然无法完成精确打击。其实，在 binder 驱动层，还有个与之相对的结构，叫做 binder_proc。
```

```
为了说明问题，我修改了初始篇中的示意图，得到下图：
```

![在这里插入图片描述](https://mmbiz.qpic.cn/sz_mmbiz_png/DYicOkJDdA2qlA9kz6Nictkr1gbmkQS25Es5fJKm6g0pkMODbDApicacm5a81Aef5On1IFk2BNMNUIMRrckOSvQng/640?wx_fmt=png&from=appmsg&randomid=an4oztq6&watermark=1&tp=webp&wxfrom=5&wx_lazy=1)

在这里插入图片描述

传输机制篇\_上001

#### 1.1 创建 binder\_proc

```scss
当构造 ProcessState 并打开 binder 驱动之时，会调用到驱动层的 binder_open () 函数，而 binder_proc 就是在 binder_open () 函数中创建的。新创建的 binder_proc 会作为一个节点，插入一个总链表（binder_procs）中。
```

```
具体代码可参考
```

```swift
kernel/drivers/staging/android/Binder.c。驱动层的 binder_open () 的代码如下：
```

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg" data-fail="0"></span><code id="code-lang-rust">static&nbsp;int&nbsp;binder_open(struct inode *nodp, struct file *filp){&nbsp; &nbsp;&nbsp;struct&nbsp;binder_proc&nbsp;*proc;&nbsp;&nbsp; &nbsp; . . . . . .&nbsp; &nbsp; proc = kzalloc(sizeof(*proc), GFP_KERNEL);&nbsp; &nbsp;&nbsp; &nbsp; get_task_struct(current);&nbsp; &nbsp; proc-&gt;tsk = current;&nbsp; &nbsp; . . . . . .&nbsp; &nbsp; hlist_add_head(&amp;proc-&gt;proc_node, &amp;binder_procs);&nbsp; &nbsp; proc-&gt;pid = current-&gt;group_leader-&gt;pid;&nbsp; &nbsp; . . . . . .&nbsp; &nbsp; filp-&gt;private_data = proc;&nbsp; &nbsp; . . . . . .}</code>
```

注意，新创建的 binder\_proc 会被记录在参数 filp 的 private\_data 域中，以后每次执行 binder\_ioctl ()，都会从 filp->private\_data 域重新读取 binder\_proc 的。

binder\_procs 总表的定义如下：

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg"></span><code id="code-lang-scss">static&nbsp;HLIST_HEAD(binder_procs);</code>
```

我们可以在 List.h 中看到 HLIST\_HEAD 的定义：

【kernel/include/linux/List.h】

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg"></span><code id="code-lang-cpp">#define&nbsp;HLIST_HEAD(name) struct hlist_head name = { &nbsp;.first = NULL }</code>
```

于是 binder\_procs 的定义相当于：

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg"></span><code id="code-lang-cpp">struct&nbsp;hlist_head&nbsp;binder_procs&nbsp; = {&nbsp;.first =&nbsp;NULL&nbsp;};</code>
```

随着后续不断向 binder\_procs 表中添加节点，这个表会不断加长，示意图如下：![在这里插入图片描述](data:image/svg+xml,%3C%3Fxml version='1.0' encoding='UTF-8'%3F%3E%3Csvg width='1px' height='1px' viewBox='0 0 1 1' version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'%3E%3Ctitle%3E%3C/title%3E%3Cg stroke='none' stroke-width='1' fill='none' fill-rule='evenodd' fill-opacity='0'%3E%3Cg transform='translate(-249.000000, -126.000000)' fill='%23FFFFFF'%3E%3Crect x='249' y='126' width='1' height='1'%3E%3C/rect%3E%3C/g%3E%3C/g%3E%3C/svg%3E)

传输机制篇\_上002

#### 1.2 binder\_proc 中的 4 棵红黑树

binder\_proc 里含有很多重要内容，不过目前我们只需关心其中的几个域：

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg"></span><code id="code-lang-cpp">struct&nbsp;binder_proc{&nbsp; &nbsp;&nbsp;struct&nbsp;hlist_node&nbsp;proc_node;&nbsp; &nbsp;&nbsp;struct&nbsp;rb_root&nbsp;threads;&nbsp; &nbsp;&nbsp;struct&nbsp;rb_root&nbsp;nodes;&nbsp; &nbsp;&nbsp;struct&nbsp;rb_root&nbsp;refs_by_desc;&nbsp; &nbsp;&nbsp;struct&nbsp;rb_root&nbsp;refs_by_node;&nbsp; &nbsp;&nbsp;int&nbsp;pid;&nbsp; &nbsp; . . . . . .&nbsp; &nbsp; . . . . . .};</code>
```

注意其中的那 4 个 rb\_root 域，“rb” 的意思是 “red black”，可见 binder\_proc 里搞出了 4 个红黑树。![在这里插入图片描述](data:image/svg+xml,%3C%3Fxml version='1.0' encoding='UTF-8'%3F%3E%3Csvg width='1px' height='1px' viewBox='0 0 1 1' version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'%3E%3Ctitle%3E%3C/title%3E%3Cg stroke='none' stroke-width='1' fill='none' fill-rule='evenodd' fill-opacity='0'%3E%3Cg transform='translate(-249.000000, -126.000000)' fill='%23FFFFFF'%3E%3Crect x='249' y='126' width='1' height='1'%3E%3C/rect%3E%3C/g%3E%3C/g%3E%3C/svg%3E)

传输机制篇\_上003

其中，nodes 树用于记录 binder 实体，refs\_by\_desc 树和 refs\_by\_node 树则用于记录 binder 代理。之所以会有两个代理树，是为了便于快速查找，我们暂时只关心其中之一就可以了。threads 树用于记录执行传输动作的线程信息。

在一个进程中，有多少 “被其他进程进行跨进程调用的” binder 实体，就会在该进程对应的 nodes 树中生成多少个红黑树节点。另一方面，一个进程要访问多少其他进程的 binder 实体，则必须在其 refs\_by\_desc 树中拥有对应的引用节点。

这 4 棵树的节点类型是不同的，threads 树的节点类型为 binder\_thread，nodes 树的节点类型为 binder\_node，refs\_by\_desc 树和 refs\_by\_node 树的节点类型相同，为 binder\_ref。这些节点内部都会包含 rb\_node 子结构，该结构专门负责连接节点的工作，和前文的 hlist\_node 有点儿异曲同工，这也是 linux 上一个常用的小技巧。我们以 nodes 树为例，其示意图如下：

![在这里插入图片描述](data:image/svg+xml,%3C%3Fxml version='1.0' encoding='UTF-8'%3F%3E%3Csvg width='1px' height='1px' viewBox='0 0 1 1' version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'%3E%3Ctitle%3E%3C/title%3E%3Cg stroke='none' stroke-width='1' fill='none' fill-rule='evenodd' fill-opacity='0'%3E%3Cg transform='translate(-249.000000, -126.000000)' fill='%23FFFFFF'%3E%3Crect x='249' y='126' width='1' height='1'%3E%3C/rect%3E%3C/g%3E%3C/g%3E%3C/svg%3E)

在这里插入图片描述

传输机制篇\_上004

rb\_node 和 rb\_root 的定义如下：

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg"></span><code id="code-lang-cpp">struct&nbsp;rb_node{&nbsp; &nbsp;&nbsp;unsigned&nbsp;long&nbsp; rb_parent_color;#define&nbsp;RB_RED &nbsp; &nbsp; &nbsp;0#define&nbsp;RB_BLACK &nbsp; &nbsp;1&nbsp; &nbsp;&nbsp;struct&nbsp;rb_node&nbsp;*rb_right;&nbsp; &nbsp;&nbsp;struct&nbsp;rb_node&nbsp;*rb_left;} __attribute__((aligned(sizeof(long))));&nbsp; &nbsp;&nbsp;/* The alignment might seem pointless, but allegedly CRIS needs it */&nbsp;struct&nbsp;rb_root{&nbsp; &nbsp;&nbsp;struct&nbsp;rb_node&nbsp;*rb_node;};</code>
```

binder\_node 的定义如下：

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg"></span><code id="code-lang-cpp">struct&nbsp;binder_node{&nbsp; &nbsp;&nbsp;int&nbsp;debug_id;&nbsp; &nbsp;&nbsp;struct&nbsp;binder_work&nbsp;work;&nbsp; &nbsp;&nbsp;union&nbsp;{&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;struct&nbsp;rb_node&nbsp;rb_node;&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;struct&nbsp;hlist_node&nbsp;dead_node;&nbsp; &nbsp; };&nbsp; &nbsp;&nbsp;struct&nbsp;binder_proc&nbsp;*proc;&nbsp; &nbsp;&nbsp;struct&nbsp;hlist_head&nbsp;refs;&nbsp; &nbsp;&nbsp;int&nbsp;internal_strong_refs;&nbsp; &nbsp;&nbsp;int&nbsp;local_weak_refs;&nbsp; &nbsp;&nbsp;int&nbsp;local_strong_refs;&nbsp; &nbsp;&nbsp;void&nbsp;__user *ptr; &nbsp; &nbsp; &nbsp;&nbsp;// 注意这个域！&nbsp; &nbsp;&nbsp;void&nbsp;__user *cookie; &nbsp; &nbsp;// 注意这个域！&nbsp; &nbsp;&nbsp;unsigned&nbsp;has_strong_ref:1;&nbsp; &nbsp;&nbsp;unsigned&nbsp;pending_strong_ref:1;&nbsp; &nbsp;&nbsp;unsigned&nbsp;has_weak_ref:1;&nbsp; &nbsp;&nbsp;unsigned&nbsp;pending_weak_ref:1;&nbsp; &nbsp;&nbsp;unsigned&nbsp;has_async_transaction:1;&nbsp; &nbsp;&nbsp;unsigned&nbsp;accept_fds:1;&nbsp; &nbsp;&nbsp;unsigned&nbsp;min_priority:8;&nbsp; &nbsp;&nbsp;struct&nbsp;list_head&nbsp;async_todo;};</code>
```

我们前文已经说过，nodes 树是用于记录 binder 实体的，所以 nodes 树中的每个 binder\_node 节点，必须能够记录下相应 binder 实体的信息。因此请大家注意 binder\_node 的 ptr 域和 cookie 域。

另一方面，refs\_by\_desc 树和 refs\_by\_node 树的每个 binder\_ref 节点则和上层的一个 BpBinder 对应，而且更重要的是，它必须具有和 “目标 binder 实体的 binder\_node” 进行关联的信息。binder\_ref 的定义如下：

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg"></span><code id="code-lang-cpp">struct&nbsp;binder_ref{&nbsp; &nbsp;&nbsp;int&nbsp;debug_id;&nbsp; &nbsp;&nbsp;struct&nbsp;rb_node&nbsp;rb_node_desc;&nbsp; &nbsp;&nbsp;struct&nbsp;rb_node&nbsp;rb_node_node;&nbsp; &nbsp;&nbsp;struct&nbsp;hlist_node&nbsp;node_entry;&nbsp; &nbsp;&nbsp;struct&nbsp;binder_proc&nbsp;*proc;&nbsp; &nbsp;&nbsp;struct&nbsp;binder_node&nbsp;*node;&nbsp; &nbsp;// 注意这个node域&nbsp; &nbsp;&nbsp;uint32_t&nbsp;desc;&nbsp; &nbsp;&nbsp;int&nbsp;strong;&nbsp; &nbsp;&nbsp;int&nbsp;weak;&nbsp; &nbsp;&nbsp;struct&nbsp;binder_ref_death&nbsp;*death;};</code>
```

请注意那个 node 域，它负责和 binder\_node 关联。另外，binder\_ref 中有两个类型为 rb\_node 的域：rb\_node\_desc 域和 rb\_node\_node 域，它们分别用于连接 refs\_by\_desc 树和 refs\_by\_node。也就是说虽然 binder\_proc 中有两棵引用树，但这两棵树用到的具体 binder\_ref 节点其实是复用的。

大家应该还记得，在《初始篇》中我是这样表达 BpBinder 和 BBinder 关系的：![在这里插入图片描述](data:image/svg+xml,%3C%3Fxml version='1.0' encoding='UTF-8'%3F%3E%3Csvg width='1px' height='1px' viewBox='0 0 1 1' version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'%3E%3Ctitle%3E%3C/title%3E%3Cg stroke='none' stroke-width='1' fill='none' fill-rule='evenodd' fill-opacity='0'%3E%3Cg transform='translate(-249.000000, -126.000000)' fill='%23FFFFFF'%3E%3Crect x='249' y='126' width='1' height='1'%3E%3C/rect%3E%3C/g%3E%3C/g%3E%3C/svg%3E)

传输机制篇\_上005

现在，我们有了 binder\_ref 和 binder\_node 知识，可以再画一张图，来解释 BpBinder 到底是如何和 BBinder 联系上的：![在这里插入图片描述](data:image/svg+xml,%3C%3Fxml version='1.0' encoding='UTF-8'%3F%3E%3Csvg width='1px' height='1px' viewBox='0 0 1 1' version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'%3E%3Ctitle%3E%3C/title%3E%3Cg stroke='none' stroke-width='1' fill='none' fill-rule='evenodd' fill-opacity='0'%3E%3Cg transform='translate(-249.000000, -126.000000)' fill='%23FFFFFF'%3E%3Crect x='249' y='126' width='1' height='1'%3E%3C/rect%3E%3C/g%3E%3C/g%3E%3C/svg%3E)

传输机制篇\_上006

上图只表示了从进程 1 向进程 2 发起跨进程传输的意思，其实反过来也是可以的，即进程 2 也可以通过自己的 “引用树” 节点找到进程 1 的 “实体树” 节点，并进行跨进程传输。大家可以自己补充上图。

OK，现在我们可以更深入地说明 binder 句柄的作用了，比如进程 1 的 BpBinder 在发起跨进程调用时，向 binder 驱动传入了自己记录的句柄值，binder 驱动就会在 “进程 1 对应的 binder\_proc 结构” 的引用树中查找和句柄值相符的 binder\_ref 节点，一旦找到 binder\_ref 节点，就可以通过该节点的 node 域找到对应的 binder\_node 节点，这个目标 binder\_node 当然是从属于进程 2 的 binder\_proc 啦，不过不要紧，因为 binder\_ref 和 binder\_node 都处于 binder 驱动的地址空间中，所以是可以用指针直接指向的。目标 binder\_node 节点的 cookie 域，记录的其实是进程 2 中 BBinder 的地址，binder 驱动只需把这个值反映给应用层，应用层就可以直接拿到 BBinder 了。这就是 Binder 完成精确打击的大体过程。

## 2 BpBinder 和 IPCThreadState

接下来我们来谈谈 Binder 传输机制。

在《[初始篇](https://mp.weixin.qq.com/s?__biz=MzkzOTQ4NDUyNg==&mid=2247490073&idx=1&sn=026de57bb3625aaba449730d99432548&scene=21#wechat_redirect)》中，我们已经提到了 BpBinder 和 ProcessState。当时只是说 BpBinder 是代理端的核心，主要负责跨进程传输，并且不关心所传输的内容。而 ProcessState 则是进程状态的记录器，它里面记录着打开 binder 驱动后得到的句柄值。因为我们并没有进一步展开来讨论 BpBinder 和 ProcessState，所以也就没有进一步打通 BpBinder 和 ProcessState 之间的关系。现在，我们试着补充一些内容。

作为代理端的核心，BpBinder 总要通过某种方式和 binder 驱动打交道，才可能完成跨进程传递语义的工作。既然 binder 驱动对应的句柄在 ProcessState 中记着，那么现在就要看 BpBinder 如何和 ProcessState 联系了。此时，我们需要提到 IPCThreadState。

从名字上看，IPCThreadState 是 “和跨进程通信（IPC）相关的线程状态”。那么很显然，一个具有多个线程的进程里应该会有多个 IPCThreadState 对象了，只不过每个线程只需一个 IPCThreadState 对象而已。这有点儿 “局部单例” 的意思。所以，在实际的代码中，IPCThreadState 对象是存放在线程的局部存储区（TLS）里的。

#### 2.1 BpBinder 的 transact () 动作

每当我们利用 BpBinder 的 transact () 函数发起一次跨进程事务时，其内部其实是调用 IPCThreadState 对象的 transact ()。BpBinder 的 transact () 代码如下：

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg"></span><code id="code-lang-cpp">status_t&nbsp;BpBinder::transact(uint32_t&nbsp;code,&nbsp;const&nbsp;Parcel&amp; data,Parcel* reply,&nbsp;uint32_t&nbsp;flags){&nbsp; &nbsp;&nbsp;// Once a binder has died, it will never come back to life.&nbsp; &nbsp;&nbsp;if&nbsp;(mAlive)&nbsp; &nbsp; {&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;status_t&nbsp;status = IPCThreadState::self()-&gt;transact(mHandle, code, data, reply, flags);&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;if&nbsp;(status == DEAD_OBJECT) mAlive =&nbsp;0;&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;return&nbsp;status;&nbsp; &nbsp; }&nbsp;&nbsp; &nbsp;&nbsp;return&nbsp;DEAD_OBJECT;}</code>
```

当然，进程中的一个 BpBinder 有可能被多个线程使用，所以发起传输的 IPCThreadState 对象可能并不是同一个对象，但这没有关系，因为这些 IPCThreadState 对象最终使用的是同一个 ProcessState 对象。

#### 2.1.1 调用 IPCThreadState 的 transact ()

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg"></span><code id="code-lang-cpp">status_t&nbsp;IPCThreadState::transact(int32_t&nbsp;handle,&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;uint32_t&nbsp;code,&nbsp;const&nbsp;Parcel&amp; data,&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; Parcel* reply,&nbsp;uint32_t&nbsp;flags){&nbsp; &nbsp; . . . . . .&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;// 把data数据整理进内部的mOut包中&nbsp; &nbsp; &nbsp; &nbsp; err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data,&nbsp;NULL);&nbsp; &nbsp; . . . . . .&nbsp; &nbsp;&nbsp; &nbsp;&nbsp;if&nbsp;((flags &amp; TF_ONE_WAY) ==&nbsp;0)&nbsp; &nbsp; {&nbsp; &nbsp; &nbsp; &nbsp; . . . . . .&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;if&nbsp;(reply)&nbsp; &nbsp; &nbsp; &nbsp; {&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; err = waitForResponse(reply);&nbsp; &nbsp; &nbsp; &nbsp; }&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;else&nbsp; &nbsp; &nbsp; &nbsp; {&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; Parcel fakeReply;&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; err = waitForResponse(&amp;fakeReply);&nbsp; &nbsp; &nbsp; &nbsp; }&nbsp; &nbsp; &nbsp; &nbsp; . . . . . .&nbsp; &nbsp; }&nbsp; &nbsp;&nbsp;else&nbsp; &nbsp; {&nbsp; &nbsp; &nbsp; &nbsp; err = waitForResponse(NULL,&nbsp;NULL);&nbsp; &nbsp; }&nbsp; &nbsp;&nbsp; &nbsp;&nbsp;return&nbsp;err;}</code>
```

IPCThreadState::transact () 会先调用 writeTransactionData () 函数将 data 数据整理进内部的 mOut 包中，这个函数的代码如下：

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg"></span><code id="code-lang-cpp">status_t&nbsp;IPCThreadState::writeTransactionData(int32_t&nbsp;cmd,&nbsp;uint32_t&nbsp;binderFlags,&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;int32_t&nbsp;handle,&nbsp;uint32_t&nbsp;code,&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;const&nbsp;Parcel&amp; data,&nbsp;status_t* statusBuffer){&nbsp; &nbsp; binder_transaction_data tr;&nbsp;&nbsp; &nbsp; tr.target.handle = handle;&nbsp; &nbsp; tr.code = code;&nbsp; &nbsp; tr.flags = binderFlags;&nbsp; &nbsp; tr.cookie =&nbsp;0;&nbsp; &nbsp; tr.sender_pid =&nbsp;0;&nbsp; &nbsp; tr.sender_euid =&nbsp;0;&nbsp; &nbsp;&nbsp; &nbsp; . . . . . .&nbsp; &nbsp; &nbsp; &nbsp; tr.data_size = data.ipcDataSize();&nbsp; &nbsp; &nbsp; &nbsp; tr.data.ptr.buffer = data.ipcData();&nbsp; &nbsp; &nbsp; &nbsp; tr.offsets_size = data.ipcObjectsCount()*sizeof(size_t);&nbsp; &nbsp; &nbsp; &nbsp; tr.data.ptr.offsets = data.ipcObjects();&nbsp; &nbsp; . . . . . .&nbsp; &nbsp;&nbsp; &nbsp; mOut.writeInt32(cmd);&nbsp; &nbsp; mOut.write(&amp;tr,&nbsp;sizeof(tr));&nbsp; &nbsp;&nbsp; &nbsp;&nbsp;return&nbsp;NO_ERROR;}</code>
```

接着 IPCThreadState::transact () 会考虑本次发起的事务是否需要回复。“不需要等待回复的” 事务，在其 flag 标志中会含有 TF\_ONE\_WAY，表示一去不回头。而 “需要等待回复的”，则需要在传递时提供记录回复信息的 Parcel 对象，一般发起 transact () 的用户会提供这个 Parcel 对象，如果不提供，transact () 函数内部会临时构造一个假的 Parcel 对象。

上面代码中，实际完成跨进程事务的是 waitForResponse () 函数，这个函数的命名不太好，但我们也不必太在意，反正 Android 中写得不好的代码多了去了，又不只多这一处。waitForResponse () 的代码截选如下：

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg"></span><code id="code-lang-perl">status_t&nbsp;IPCThreadState::waitForResponse(Parcel *reply,&nbsp;status_t&nbsp;*acquireResult){&nbsp; &nbsp;&nbsp;int32_t&nbsp;cmd;&nbsp; &nbsp;&nbsp;int32_t&nbsp;err;&nbsp;&nbsp; &nbsp;&nbsp;while&nbsp;(1)&nbsp; &nbsp; {&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;// talkWithDriver()内部会完成跨进程事务&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;if&nbsp;((err = talkWithDriver()) &lt; NO_ERROR)&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;break;&nbsp; &nbsp; &nbsp; &nbsp;&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;// 事务的回复信息被记录在mIn中，所以需要进一步分析这个回复&nbsp; &nbsp; &nbsp; &nbsp; . . . . . .&nbsp; &nbsp; &nbsp; &nbsp; cmd = mIn.readInt32();&nbsp; &nbsp; &nbsp; &nbsp; . . . . . .&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;switch&nbsp;(cmd)&nbsp; &nbsp; &nbsp; &nbsp; {&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;case&nbsp;BR_TRANSACTION_COMPLETE:&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;if&nbsp;(!reply &amp;&amp; !acquireResult)&nbsp;goto&nbsp;finish;&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;break;&nbsp; &nbsp; &nbsp; &nbsp;&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;case&nbsp;BR_DEAD_REPLY:&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; err = DEAD_OBJECT;&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;goto&nbsp;finish;&nbsp;&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;case&nbsp;BR_FAILED_REPLY:&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; err = FAILED_TRANSACTION;&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;goto&nbsp;finish;&nbsp; &nbsp; &nbsp; &nbsp; . . . . . .&nbsp; &nbsp; &nbsp; &nbsp; . . . . . .&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;default:&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;// 注意这个executeCommand()噢，它会处理BR_TRANSACTION的。&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; err = executeCommand(cmd);&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;if&nbsp;(err != NO_ERROR)&nbsp;goto&nbsp;finish;&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp;break;&nbsp; &nbsp; &nbsp; &nbsp; }&nbsp; &nbsp; }&nbsp;finish:&nbsp; &nbsp; . . . . . .&nbsp; &nbsp;&nbsp;return&nbsp;err;}</code>
```

#### 2.1.2 talkWithDriver()

waitForResponse () 中是通过调用 talkWithDriver () 来和 binder 驱动打交道的，说到底会调用 ioctl () 函数。因为 ioctl () 函数在传递 BINDER\_WRITE\_READ 语义时，既会使用 “输入 buffer”，也会使用 “输出 buffer”，所以 IPCThreadState 专门搞了两个 Parcel 类型的成员变量：mIn 和 mOut。总之就是，mOut 中的内容发出去，发送后的回复写进 mIn。

talkWithDriver () 的代码截选如下：

```
<span data-cacheurl="" data-remoteid="" data-lazy-bgimg="https://mmbiz.qpic.cn/mmbiz_svg/LIND77SSex9jzkGpyR1iaCaoCL9vmz1vgPVx3tSBeESyvOBsDTpUPwck1Ns2Uxic5QuYyTmmxpv4gX8NSqzZVjgt6xkg2JYyyk/640?wx_fmt=svg&amp;from=appmsg"></span><code id="code-lang-cpp">status_t&nbsp;IPCThreadState::talkWithDriver(bool&nbsp;doReceive){&nbsp; &nbsp; . . . . . .&nbsp; &nbsp; binder_write_read bwr;&nbsp; &nbsp;&nbsp; &nbsp; . . . . . .&nbsp; &nbsp; bwr.write_size = outAvail;&nbsp; &nbsp; bwr.write_buffer = (long&nbsp;unsigned&nbsp;int)mOut.data();&nbsp; &nbsp; . . . . . .&nbsp; &nbsp; &nbsp; &nbsp; bwr.read_size = mIn.dataCapacity();&nbsp; &nbsp; &nbsp; &nbsp; bwr.read_buffer = (long&nbsp;unsigned&nbsp;int)mIn.data();&nbsp; &nbsp; . . . . . .&nbsp; &nbsp; . . . . . .&nbsp; &nbsp;&nbsp;do&nbsp; &nbsp; {&nbsp; &nbsp; &nbsp; &nbsp; . . . . . .&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;if&nbsp;(ioctl(mProcess-&gt;mDriverFD, BINDER_WRITE_READ, &amp;bwr) &gt;=&nbsp;0)&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; err = NO_ERROR;&nbsp; &nbsp; &nbsp; &nbsp; . . . . . .&nbsp; &nbsp; }&nbsp;while&nbsp;(err == -EINTR);&nbsp;&nbsp; &nbsp; . . . . . .&nbsp; &nbsp; . . . . . .&nbsp; &nbsp;&nbsp;return&nbsp;err;}</code>
```

看到了吗？mIn 和 mOut 的 data 会先整理进一个 binder\_write\_read 结构，然后再传给 ioctl () 函数。而最关键的一句，当然就是那句 ioctl () 了。此时使用的文件描述符就是前文我们说的 ProcessState 中记录的 mDriverFD，说明是向 binder 驱动传递语义。BINDER\_WRITE\_READ 表示我们希望读写一些数据。

至此，应用程序通过 BpBinder 向远端发起传输的过程就交代完了，数据传到了 binder 驱动，一切就看 binder 驱动怎么做了。至于驱动层又做了哪些动作，我们留在下一篇文章再介绍。

原文地址:

https://my.oschina.net/youranhongcha/blog/149575

其他framework实战技术干货相关手把手课程资料：

[Android Framework开发rom实战合集课表/车载车机手机高级系统开发工程必会技能](https://mp.weixin.qq.com/s?__biz=MzkzOTQ4NDUyNg==&mid=2247484186&idx=1&sn=328a6efaf16b78b1029b3595be03268b&scene=21#wechat_redirect)

![图片](data:image/svg+xml,%3C%3Fxml version='1.0' encoding='UTF-8'%3F%3E%3Csvg width='1px' height='1px' viewBox='0 0 1 1' version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'%3E%3Ctitle%3E%3C/title%3E%3Cg stroke='none' stroke-width='1' fill='none' fill-rule='evenodd' fill-opacity='0'%3E%3Cg transform='translate(-249.000000, -126.000000)' fill='%23FFFFFF'%3E%3Crect x='249' y='126' width='1' height='1'%3E%3C/rect%3E%3C/g%3E%3C/g%3E%3C/svg%3E)

具体优惠购买和成为vip学员加入vip群可以私聊马哥微信号：

androidframework007

![图片](data:image/svg+xml,%3C%3Fxml version='1.0' encoding='UTF-8'%3F%3E%3Csvg width='1px' height='1px' viewBox='0 0 1 1' version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'%3E%3Ctitle%3E%3C/title%3E%3Cg stroke='none' stroke-width='1' fill='none' fill-rule='evenodd' fill-opacity='0'%3E%3Cg transform='translate(-249.000000, -126.000000)' fill='%23FFFFFF'%3E%3Crect x='249' y='126' width='1' height='1'%3E%3C/rect%3E%3C/g%3E%3C/g%3E%3C/svg%3E)