Linux 内核是一个模块化、分层的操作系统内核，它充当硬件和用户空间程序之间的桥梁。其架构旨在提供高效、稳定和可扩展的操作系统。

![](https://developer.qcloudimg.com/http-save/audit-11218869/025f6f43eb15077b751b9a3b71a6b33e.png)

内核的核心组件包括：

-   **系统调用接口：** 应用程序与内核通信的接口。
    
-   **调度程序：** 管理进程执行并分配 CPU 时间。
    
-   **内存管理单元 (MMU)：** 管理虚拟内存和物理内存之间的映射。
    
-   **虚拟文件系统 (VFS)：** 提供对不同文件系统的一致访问。
    
-   **网络堆栈：** 处理网络通信。
    

![](https://developer.qcloudimg.com/http-save/audit-11218869/b957f9aa43daa2a252501765309b37a9.png)

内核还包含一系列子系统，它们提供特定的功能：

-   **设备驱动程序：** 与硬件设备交互。
    
-   **文件系统：** 提供对文件和目录的访问。
    
-   **网络协议栈：** 实现特定网络协议，如 TCP/IP。
    
-   **安全模块：** 处理安全功能，如访问控制和加密。
    
-   **系统服务：** 提供系统级服务，如计时器和进程间通信。
    

用户空间程序通过系统调用与内核交互。系统调用是内核提供的特殊函数，允许用户空间程序执行受保护的操作，例如访问文件或分配内存。

内核通过中断与硬件设备交互。当硬件设备需要内核的关注时，它会引发中断。内核然后处理中断并采取适当的行动，例如处理网络数据包或更新设备状态。

## 二、进程基础知识

Linux 内核把进程称为任务(task)，进程的虚拟地址空间分为用户虚拟地址空间和内核虚拟地址空间，所有进程共享内核虚拟地址空间，每个进程有独立的用户虚拟地址空间。

进程有两种特殊形式：

-   没有用户虚拟地址空间的进程称为内核线程。
    
-   共享用户虚拟地址空间的进程称为用户线程。
    

通常在不会引起混淆的情况下把用户线程简称为线程。共享同一个用户虚拟地址空间的所有用户线程组成一个线程组。

C 标准库进程术语和 Linux 内核进程术语对应关系如下：

| C 标准库进程术语 | Linux 内核进程术语  |
|-----------|---------------|
| 包含多个线程的进程 |      线程组      |
| 只有一个线程的进程 |     进程或任务     |
|    线程     | 共享用户虚拟地址空间的进程 |

## 三、Linux 进程四要素

1.  有一段程序供其执行。
    
2.  有进程专用的系统堆栈空间。
    
3.  在内核有 task\_struct 数据结构。
    
4.  有独立的存储空间，拥有专有的用户空间。
    

## 四、task\_struct 数据结构主要成员

(include/linux/sched.h)

```cpp
struct task_struct {//进程描述符
#ifdef CONFIG_THREAD_INFO_IN_TASK
/*
 * For reasons of header soup (see current_thread_info()), this
 * must be the first element of task_struct.
 */
struct thread_infothread_info;
#endif
unsigned int__state;//指向进程状态

#ifdef CONFIG_PREEMPT_RT
/* saved state for "spinlock sleepers" */
unsigned intsaved_state;
#endif

/*
 * This begins the randomizable portion of task_struct. Only
 * scheduling-critical items should be added above here.
 */
randomized_struct_fields_start

void*stack;//指向内核栈
refcount_tusage;
/* Per task flags (PF_*), defined further below: */
unsigned intflags;
unsigned intptrace;

       // ...... 
};
```

-   task\_struct：进程描述符。
    
-   \_\_state：指向进程状态。
    
-   \*stack：指向内核栈。
    
-   pid：指向全局的进程号。
    
-   tgid：指向全局的线程组的标识符。
    
-   \*real\_parent：指向真实的父进程
    
-   \*parent：指向当前的父进程。比如一个进程被另外的进程使用系统调用进行跟踪（ptrace），那么此时的父进程就是跟踪进程。
    
-   进程调度策略的优先级：prio、static\_prio、normal\_prio、rt\_priority。
    
-   nr\_cpus\_allowed：允许进程在哪些处理器上执行。
    
-   \*mm：指向内存描述符，内核线程此项为NULL。
    
-   \*active\_mm：指向内存描述符，内核线程运行时从进程借用。
    

-   \*fs：文件系统信息。
    

还有很多成员，这里就不一一列举。

## 五、创建新进程分析

在 Linux 内核中，新进程是从一个已经存在的进程复制出来的，内核使用静态数据结构造出 0 号内核线程，0 号内核线程分叉生成 1 号内核线程和 2 号内核线程（kthreadd 线程）。1 号内核线程完成初始化以后装载用户程序，变成 1 号进程，其他进程都是 1 号进程或者它的子孙进程分叉生成的；其他内核线程是 kthreadd 线程分叉生成的。

**Linux 内核3 个系统调用创建新的进程：**

-   fork(分叉)：子进程是父进程的一个副本，采用写时复制技术。
    
-   vfork：用于创建子进程，之后子进程立即调用 execve 以装载新程序的情况，为了避免复制物理页，父进程会睡眠等待子进程装载新程序。现在 fork 采用了写时复制技术，vfork 失去了速度优势，已经被废弃。
    
-   clone（克隆）：可以精确地控制子进程和父进程共享哪些资源。这个系统调用的主要用处是可供 pthread 库用来创建线程。
    

clone 是功能最齐全的函数，参数多、使用复杂，fork 是 clone 的简化函数。（kernel/fork.c）

```objectivec
#ifdef __ARCH_WANT_SYS_FORK
SYSCALL_DEFINE0(fork)
{
#ifdef CONFIG_MMU
struct kernel_clone_args args = {
.exit_signal = SIGCHLD,
};

return _do_fork(&args);
#else
/* can not support in nommu mode */
return -EINVAL;
#endif
}
#endif

#ifdef __ARCH_WANT_SYS_VFORK
SYSCALL_DEFINE0(vfork)
{
struct kernel_clone_args args = {
.flags= CLONE_VFORK | CLONE_VM,
.exit_signal= SIGCHLD,
};

return _do_fork(&args);
}
#endif


#ifdef __ARCH_WANT_SYS_CLONE
#ifdef CONFIG_CLONE_BACKWARDS
SYSCALL_DEFINE5(clone, unsigned long, clone_flags, unsigned long, newsp,
 int __user *, parent_tidptr,
 unsigned long, tls,
 int __user *, child_tidptr)
#elif defined(CONFIG_CLONE_BACKWARDS2)
SYSCALL_DEFINE5(clone, unsigned long, newsp, unsigned long, clone_flags,
 int __user *, parent_tidptr,
 int __user *, child_tidptr,
 unsigned long, tls)
#elif defined(CONFIG_CLONE_BACKWARDS3)
SYSCALL_DEFINE6(clone, unsigned long, clone_flags, unsigned long, newsp,
int, stack_size,
int __user *, parent_tidptr,
int __user *, child_tidptr,
unsigned long, tls)
#else
SYSCALL_DEFINE5(clone, unsigned long, clone_flags, unsigned long, newsp,
 int __user *, parent_tidptr,
 int __user *, child_tidptr,
 unsigned long, tls)
#endif
{
struct kernel_clone_args args = {
.flags= (lower_32_bits(clone_flags) & ~CSIGNAL),
.pidfd= parent_tidptr,
.child_tid= child_tidptr,
.parent_tid= parent_tidptr,
.exit_signal= (lower_32_bits(clone_flags) & CSIGNAL),
.stack= newsp,
.tls= tls,
};

if (!legacy_clone_args_valid(&args))
return -EINVAL;

return _do_fork(&args);
}
#endif
```

Linux 内核定义系统调用的独特方式，目前以系统调用 fork 为例：创建新进程的 3 个系统调用在文件kernel/fork.c中，它们把工作委托给函数\_do\_fork（从6.0开始，更名为kernel\_clone）。具体源码分析如下：

```csharp
long _do_fork(struct kernel_clone_args *args)
{
u64 clone_flags = args->flags;
struct completion vfork;
struct pid *pid;
struct task_struct *p;
int trace = 0;
long nr;

// ......

}
```

Linux 内核函数\_do\_fork()执行流程如下图所示：

![](https://developer.qcloudimg.com/http-save/audit-11218869/9692eba0fd51f5f7f05d1b506dde2760.png)

具体核心处理函数为 copy\_process()内核源码如下：

```csharp
/*
 * This creates a new process as a copy of the old one,
 * but does not actually start it yet.
 *
 * It copies the registers, and all the appropriate
 * parts of the process environment (as per the clone
 * flags). The actual kick-off is left to the caller.
 */
static __latent_entropy struct task_struct *copy_process(
struct pid *pid,
int trace,
int node,
struct kernel_clone_args *args)
{
int pidfd = -1, retval;
struct task_struct *p;
struct multiprocess_signals delayed;
struct file *pidfile = NULL;
u64 clone_flags = args->flags;
struct nsproxy *nsp = current->nsproxy;

// ......

}
```

函数 copy\_process()：创建新进程的主要工作由此函数完成， 具体处理流程如下图所示：

![](https://developer.qcloudimg.com/http-save/audit-11218869/c31c2b536ed5796c739443efc879699f.png)

同一个线程组的所有线程必须属于相同的用户命名空间和进程号命名空间。

## 六、剖析进程状态迁移

进程主要有 7 种状态：

-   就绪状态。
    
-   **运行状态：** 进程正在执行指令。
    
-   轻度睡眠。
    
-   中度睡眠。
    
-   深度睡眠。
    
-   **僵尸状态：** 进程已终止，但其父进程尚未收集其资源。
    
-   死亡状态。
    

它们之间状态变迁如下：

![](https://developer.qcloudimg.com/http-save/audit-11218869/e333363ce7f44c921fd12f2905e22e51.png)

就绪：state是TASK\_RUNING（没有严格区别就绪和运行），正在运行队列中等待调度器调度。

运行：state是TASK\_RUNING，证明调度器选中，正在CPU上执行。

僵尸：state是TASK\_DEAD，进程退出并且父进程关注子进程退出事件。

死亡：state是exit\_state。

## 七、写时复制技术

写时复制核心思想：只有在不得不复制数据内容时才去复制数据内容；降低资源浪费。

申请新进程的步骤：

1.  申请一块空的PCB（进程控制块）。
    
2.  为 新进程分配数据资源（这里使用写时复制技术）。
    
3.  初始化PCB。
    
4.  把刚才申请的新进程插入到就绪队列中。state是task\_running，被调度器调度，进入运行状态。
    

应用程序（进程 1）修改页面 C 之前：

![](https://developer.qcloudimg.com/http-save/audit-11218869/9c90a6714d7134ba5cf41cc87513cd53.png)

应用程序（进程 1）修改页面 C 之后：

![](https://developer.qcloudimg.com/http-save/audit-11218869/6d59bff1a77f8e2aa8669075403fe45d.png)

注意：只有可修改的页面才需要标记为写时复制，不能修改的页面可以由父进程和子进程共享。

## 八、总结

-   **进程基础：** 进程生命周期、进程状态、进程控制块 (PCB)。
    
-   **进程创建：** `fork()` 系统调用、内核如何复制进程。
    
-   **进程调度：** 调度算法、内核如何根据优先级和公平性分配 CPU 时间。
    
-   **进程同步：** 锁、信号量、条件变量，以及内核如何防止竞争条件。
    
-   **进程终止：** `exit()` 系统调用、内核如何回收进程资源。
    

![](https://developer.qcloudimg.com/http-save/audit-11218869/644fa32828519229a3e500dacf82bb41.png)