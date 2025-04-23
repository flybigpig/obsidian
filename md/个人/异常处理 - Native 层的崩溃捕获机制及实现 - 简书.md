## 异常处理 - Native 层的崩溃捕获机制及实现

[![](https://upload.jianshu.io/users/upload_avatars/4314397/39de1eb1-5b12-461e-9fa1-ec88ce1d90a6.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/96/h/96/format/webp)](https://www.jianshu.com/u/35083fcb7747)

42021.02.20 16:28:34字数 1,612阅读 7,372

在 Android 平台，native crash 我们可能关注得比较少，记得在长沙做开发那会，基本不会用到自己写的 so 库，集成第三方功能像地图也就会拷贝几个 so 到目录下，当时连 so 是什么都不知道。后来渐渐的由于项目的特殊性，不能直接集成 bugly 和 qapm 这些，因此后面就被逼着学会了 Native 层的崩溃捕获。虽然实现起来相对要比 java 层更难一些，但也并不是很复杂，我们可以查一些资料或者借鉴一些第三方的开源库，总结起来只需要从以下几个方面入手即可：

-   了解 native 层的崩溃处理机制
-   捕捉到 native crash 信号
-   处理各种特殊情况
-   解析 native 层的 crash 堆栈

### 1\. 了解 native 层的崩溃处理机制

开源库有 [coffeecatch](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Fxroche%2Fcoffeecatch) 、 [breakpad](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Fgoogle%2Fbreakpad) 等，普通项目中我们可以直接集成 [bugly](https://links.jianshu.com/go?to=https%3A%2F%2Fbugly.qq.com%2Fv2%2F) ，由于 bugly 不开源所以借鉴的意义并不大。breakpad 是 google 开源的比较权威但是代码体积量大，coffeecatch 实现简洁但存在兼容性问题。其实无论是 coffeecatch 还是 bugly 又或是我们自己写，其内部的实现原理肯定都是一致的， 只要我们了解 native 层的崩溃处理机制，一切便能迎刃而解。

在 Unix-like 系统中，所有的崩溃都是编程错误或者硬件错误相关的，系统遇到不可恢复的错误时会触发崩溃机制让程序退出，如除零、段地址错误等。异常发生时，CPU 通过异常中断的方式，触发异常处理流程。不同的处理器，有不同的异常中断类型和中断处理方式。linux 把这些中断处理，统一为信号量，可以注册信号量向量进行处理。信号机制是进程之间相互传递消息的一种方法，信号全称为软中断信号。

函数运行在用户态，当遇到系统调用、中断或是异常的情况时，程序会进入内核态。信号涉及到了这两种状态之间的转换。

![](https://upload-images.jianshu.io/upload_images/4314397-abd662f7477a9811.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/720/format/webp)

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

```c
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

```c
/* Call the old handler. */
void call_old_signal_handler(const int sig, siginfo_t *const info, void *const sc) {
    // 恢复默认应该也行吧
    LOGD("sig -> %d", sig);
    handlerMaps->at(sig)->sa_sigaction(sig, info, sc);
}
```

##### 3.3 防止死锁或者死循环

```c
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

关于解析 native 层的 crash 堆栈解析，并不是一两句话能说清楚的，因此我们打算单独拿一次课来跟大家讲。视频链接地址无法发出来希望大家能够谅解，因为一粘贴视频地址文章就会被简书锁定。大家感兴趣的话，可以去我的 csdn 或者掘金找。

最后编辑于

：2021.02.20 17:28:23

更多精彩内容，就在简书APP

![](https://upload.jianshu.io/images/js-qrc.png)

"小礼物走一走，来简书关注我"

还没有人赞赏，支持一下

[![  ](https://upload.jianshu.io/users/upload_avatars/4314397/39de1eb1-5b12-461e-9fa1-ec88ce1d90a6.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/100/h/100/format/webp)](https://www.jianshu.com/u/35083fcb7747)

[红橙Darren](https://www.jianshu.com/u/35083fcb7747 "红橙Darren")目前就职于腾讯微信，腾讯面试官，专注移动端开发，热爱技术，乐于分享！

总资产84共写了23.0W字获得7,282个赞共8,197个粉丝

-   序言：七十年代末，一起剥皮案震惊了整个滨河市，随后出现的几起案子，更是在滨河造成了极大的恐慌，老刑警刘岩，带你破解...
    
    [![](https://upload.jianshu.io/users/upload_avatars/15878160/783c64db-45e5-48d7-82e4-95736f50533e.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)沈念sama](https://www.jianshu.com/u/dcd395522934)阅读 224,014评论 6赞 522
    
-   序言：滨河连续发生了三起死亡事件，死亡现场离奇诡异，居然都是意外死亡，警方通过查阅死者的电脑和手机，发现死者居然都...
    
-   文/潘晓璐 我一进店门，熙熙楼的掌柜王于贵愁眉苦脸地迎上来，“玉大人，你说我怎么就摊上这事。” “怎么了？”我有些...
    
-   文/不坏的土叔 我叫张陵，是天一观的道长。 经常有香客问我，道长，这世上最难降的妖魔是什么？ 我笑而不...
    
-   正文 为了忘掉前任，我火速办了婚礼，结果婚礼上，老公的妹妹穿的比我还像新娘。我一直安慰自己，他们只是感情好，可当我...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 69,636评论 6赞 399
    
-   文/花漫 我一把揭开白布。 她就那样静静地躺着，像睡着了一般。 火红的嫁衣衬着肌肤如雪。 梳的纹丝不乱的头发上，一...
    
-   那天，我揣着相机与录音，去河边找鬼。 笑死，一个胖子当着我的面吹牛，可吹牛的内容都是我干的。 我是一名探鬼主播，决...
    
-   文/苍兰香墨 我猛地睁开眼，长吁一口气：“原来是场噩梦啊……” “哼！你这毒妇竟也来了？” 一声冷哼从身侧响起，我...
    
-   序言：老挝万荣一对情侣失踪，失踪者是张志新（化名）和其女友刘颖，没想到半个月后，有当地人在树林里发现了一具尸体，经...
    
-   正文 独居荒郊野岭守林人离奇死亡，尸身上长有42处带血的脓包…… 初始之章·张勋 以下内容为张勋视角 年9月15日...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 39,069评论 3赞 344
    
-   正文 我和宋清朗相恋三年，在试婚纱的时候发现自己被绿了。 大学时的朋友给我发了我未婚夫和他白月光在一起吃饭的照片。...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 41,211评论 1赞 354
    
-   序言：一个原本活蹦乱跳的男人离奇死亡，死状恐怖，灵堂内的尸体忽然破棺而出，到底是诈尸还是另有隐情，我是刑警宁泽，带...
    
-   正文 年R本政府宣布，位于F岛的核电站，受9级特大地震影响，放射性物质发生泄漏。R本人自食恶果不足惜，却给世界环境...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 42,541评论 3赞 336
    
-   文/蒙蒙 一、第九天 我趴在偏房一处隐蔽的房顶上张望。 院中可真热闹，春花似锦、人声如沸。这庄子的主人今日做“春日...
    
-   文/苍兰香墨 我抬头看了看天上的太阳。三九已至，却和暖如春，着一层夹袄步出监牢的瞬间，已是汗流浃背。 一阵脚步声响...
    
-   我被黑心中介骗来泰国打工， 没想到刚下飞机就差点儿被人妖公主榨干…… 1. 我叫王不留，地道东北人。 一个月前我还...
    
-   正文 我出身青楼，却偏偏与公主长得像，于是被迫代替她去往敌国和亲。 传闻我的和亲对象是个残疾皇子，可洞房花烛夜当晚...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 46,240评论 2赞 363
    

### 被以下专题收入，发现更多相似内容

### 推荐阅读[更多精彩内容](https://www.jianshu.com/)

-   常见的应用闪退有Java Crash和Native Crash引起，基于最新的Android P源码，以下是其2者...
    
-   一、信号机制 函数运行在用户态,当遇到系统调用、中断或是异常的情况时,程序会进入内核态。信号涉及到了这两种状态之间...
    
-   Android 平台Native代码崩溃捕获机制及实现 Android 平台Native代码崩溃捕获机制及实现（来...
    
-   Java 虚拟机负责处理内存，程序员无须关心，看起来很美好。然而一旦出现内存泄漏或溢出，如果不了解虚拟机的内存管理...
    
-   今天感恩节哎，感谢一直在我身边的亲朋好友。感恩相遇！感恩不离不弃。 中午开了第一次的党会，身份的转变要...
    
    [![](https://upload.jianshu.io/users/upload_avatars/20029397/d72e5986-002a-4e04-9d29-4b14b5f11d3b.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)迷月闪星情](https://www.jianshu.com/u/3958c342d321)阅读 10,576评论 0赞 11