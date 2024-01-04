在 Android 平台，native crash 我们可能关注得比较少，记得在长沙做开发那会，基本不会用到自己写的 so 库，集成第三方功能像地图也就会拷贝几个 so 到目录下，当时连 so 是什么都不知道。后来渐渐的由于项目的特殊性，不能直接集成 bugly 和 qapm 这些，因此后面就被逼着学会了 Native 层的崩溃捕获。虽然实现起来相对要比 java 层更难一些，但也并不是很复杂，我们可以查一些资料或者借鉴一些第三方的开源库，总结起来只需要从以下几个方面入手即可：

-   了解 native 层的崩溃处理机制
-   捕捉到 native crash 信号
-   处理各种特殊情况
-   解析 native 层的 crash 堆栈

### 1\. 了解 native 层的崩溃处理机制

开源库有 [coffeecatch](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2Fxroche%2Fcoffeecatch "https://github.com/xroche/coffeecatch") 、 [breakpad](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2Fgoogle%2Fbreakpad "https://github.com/google/breakpad") 等，普通项目中我们可以直接集成 [bugly](https://link.juejin.cn/?target=https%3A%2F%2Fbugly.qq.com%2Fv2%2F "https://bugly.qq.com/v2/") ，由于 bugly 不开源所以借鉴的意义并不大。breakpad 是 google 开源的比较权威但是代码体积量大，coffeecatch 实现简洁但存在兼容性问题。其实无论是 coffeecatch 还是 bugly 又或是我们自己写，其内部的实现原理肯定都是一致的， 只要我们了解 native 层的崩溃处理机制，一切便能迎刃而解。

在 Unix-like 系统中，所有的崩溃都是编程错误或者硬件错误相关的，系统遇到不可恢复的错误时会触发崩溃机制让程序退出，如除零、段地址错误等。异常发生时，CPU 通过异常中断的方式，触发异常处理流程。不同的处理器，有不同的异常中断类型和中断处理方式。linux 把这些中断处理，统一为信号量，可以注册信号量向量进行处理。信号机制是进程之间相互传递消息的一种方法，信号全称为软中断信号。

函数运行在用户态，当遇到系统调用、中断或是异常的情况时，程序会进入内核态。信号涉及到了这两种状态之间的转换。

![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/db23e4bcbb4545ab8c71d43d38e9ecc9~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp)

接收信号的任务是由内核代理的，当内核接收到信号后，会将其放到对应进程的信号队列中，同时向进程发送一个中断，使其陷入内核态。注意，此时信号还只是在队列中，对进程来说暂时是不知道有信号到来的。进程陷入内核态后，有两种场景会对信号进行检测：

-   进程从内核态返回到用户态前进行信号检测
-   进程在内核态中，从睡眠状态被唤醒的时候进行信号检测

当发现有新信号时，便会进入信号的处理。信号处理函数是运行在用户态的，调用处理函数前，内核会将当前内核栈的内容备份拷贝到用户栈上，并且修改指令寄存器（eip）将其指向信号处理函数。接下来进程返回到用户态中，执行相应的信号处理函数。信号处理函数执行完成后，还需要返回内核态，检查是否还有其它信号未处理。如果所有信号都处理完成，就会将内核栈恢复（从用户栈的备份拷贝回来），同时恢复指令寄存器（eip）将其指向中断前的运行位置，最后回到用户态继续执行进程。至此，一个完整的信号处理流程便结束了，如果同时有多个信号到达，会不断的检测和处理信号。

### 2\. 捕捉到 native crash 信号

了解 native 层的崩溃处理机制，那么我们的实现方案便是注册信号处理函数，在 native 层可以用 sigaction()：

```
#include <signal.h> 

// signum：代表信号编码，可以是除SIGKILL及SIGSTOP外的任何一个特定有效的信号，如果为这两个信号定义自己的处理函数，将导致信号安装错误。
// act：指向结构体sigaction的一个实例的指针，该实例指定了对特定信号的处理，如果设置为空，进程会执行默认处理。
// oldact：和参数act类似，只不过保存的是原来对相应信号的处理，也可设置为NULL。
// int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact));

void signal_pass(int code, siginfo_t *si, void *sc) {
    LOGD("捕捉到了 native crash 信号.");
}

bool installHandlersLocked() {
    if (handlers_installed)
        return false;

    // Fail if unable to store all the old handlers.
    for (int i = 0; i < kNumHandledSignals; ++i) {
        if (sigaction(kExceptionSignals[i], NULL, &old_handlers[i]) == -1) {
            return false;
        } else {
            handlerMaps->insert(
                    std::pair<int, struct sigaction *>(kExceptionSignals[i], &old_handlers[i]));
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

    // Mask all exception signals when we're handling one of them.
    for (int i = 0; i < kNumHandledSignals; ++i)
        sigaddset(&sa.sa_mask, kExceptionSignals[i]);

    sa.sa_sigaction = signal_pass;
    sa.sa_flags = SA_ONSTACK | SA_SIGINFO;

    for (int i = 0; i < kNumHandledSignals; ++i) {
        if (sigaction(kExceptionSignals[i], &sa, NULL) == -1) {
            // At this point it is impractical to back out changes, and so failure to
            // install a signal is intentionally ignored.
        }
    }
    handlers_installed = true;
    return true;
}
```

### 3\. 处理各种特殊情况

Native 层的崩溃捕获复杂就复杂在需要处理各种特殊情况，虽然一个函数就能监听到崩溃信号回调，但是需要预防各种其他异常情况的出现，我们一一来看下：

##### 3.1 设置额外栈空间

SIGSEGV 很有可能是栈溢出引起的，如果在默认的栈上运行很有可能会破坏程序运行的现场，无法获取到正确的上下文。而且当栈满了（太多次递归，栈上太多对象），系统会在同一个已经满了的栈上调用 SIGSEGV 的信号处理函数，又再一次引起同样的信号。我们应该开辟一块新的空间作为运行信号处理函数的栈。可以使用 sigaltstack 在任意线程注册一个可选的栈，保留一下在紧急情况下使用的空间。（系统会在危险情况下把栈指针指向这个地方，使得可以在一个新的栈上运行信号处理函数）

```
/**
 * 先创建一块 sigaltstack ，因为有可能是由堆栈溢出发出的信号
 */
static void installAlternateStackLocked() {
    if (stack_installed)
        return;

    memset(&old_stack, 0, sizeof(old_stack));
    memset(&new_stack, 0, sizeof(new_stack));

    // SIGSTKSZ may be too small to prevent the signal handlers from overrunning
    // the alternative stack. Ensure that the size of the alternative stack is
    // large enough.
    static const unsigned kSigStackSize = std::max(16384, SIGSTKSZ);

    // Only set an alternative stack if there isn't already one, or if the current
    // one is too small.
    if (sigaltstack(NULL, &old_stack) == -1 || !old_stack.ss_sp ||
        old_stack.ss_size < kSigStackSize) {
        new_stack.ss_sp = calloc(1, kSigStackSize);
        new_stack.ss_size = kSigStackSize;

        if (sigaltstack(&new_stack, NULL) == -1) {
            free(new_stack.ss_sp);
            return;
        }
        stack_installed = true;
    }
}
```

##### 3.2 兼容其他 signal 处理

某些信号可能在之前已经被安装过信号处理函数，而 sigaction 一个信号量只能注册一个处理函数，这意味着我们的处理函数会覆盖其他人的处理信号。保存旧的处理函数，在处理完我们的信号处理函数后，在重新运行老的处理函数就能完成兼容。

```
/* Call the old handler. */
void call_old_signal_handler(const int sig, siginfo_t *const info, void *const sc) {
    // 恢复默认应该也行吧
    LOGD("sig -> %d", sig);
    handlerMaps->at(sig)->sa_sigaction(sig, info, sc);
}
```

##### 3.3 防止死锁或者死循环

```
void signal_pass(int code, siginfo_t *si, void *sc) {
    /* Ensure we do not deadlock. Default of ALRM is to die.
    * (signal() and alarm() are signal-safe) */
    // 这里要考虑用非信号方式防止死锁
    signal(code, SIG_DFL);
    signal(SIGALRM, SIG_DFL);
    /* Ensure we do not deadlock. Default of ALRM is to die.
     * (signal() and alarm() are signal-safe) */
    (void) alarm(8);

    /* Available context ? */
    notifyCaughtSignal();

    call_old_signal_handler(code, si, sc);

    LOGD("at the end of signal_pass");
}
```

### 4\. 解析 native 层的 crash 堆栈

关于解析 native 层的 crash 堆栈解析，并不是一两句话能说清楚的，因此我们打算单独拿一次课来跟大家讲。

视频地址：[pan.baidu.com/s/1FeZjyrnv…](https://link.juejin.cn/?target=https%3A%2F%2Fpan.baidu.com%2Fs%2F1FeZjyrnvHO8AkEJH29Vesg "https://pan.baidu.com/s/1FeZjyrnvHO8AkEJH29Vesg") 视频密码：gr11