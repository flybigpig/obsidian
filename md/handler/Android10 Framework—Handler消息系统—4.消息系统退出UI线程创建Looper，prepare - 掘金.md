       ![](https://lf-web-assets.juejin.cn/obj/juejin-web/xitu_juejin_web/img/banner.a5c9f88.jpg)

> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

UI线程创建Looper，prepare传入参数为false

```
public static void prepareMainLooper() {
    prepare(false);
    synchronized (Looper.class) {
        if (sMainLooper != null) {
            throw new IllegalStateException("The main Looper has already been prepared.");
        }
        sMainLooper = myLooper();
    }
}
```

子线程创建Looper，prepare传入参数为true

```
public static void prepare() {
    prepare(true);
}
```

这样很好理解，因为UI线程不允许消息系统退出，退出了APP就无法工作了；而子线程消息系统是可以退出的；可以调用Looper的如下方法退出

```
//非安全退出
public void quit()
//安全退出
public void quitSafely()
```

他们都会调用MessageQueue的quit方法

```
void quit(boolean safe) {
    if (!mQuitAllowed) {
        throw new IllegalStateException("Main thread not allowed to quit.");
    }

    synchronized (this) {
        if (mQuitting) {
            return;
        }
        mQuitting = true;

        if (safe) {
            removeAllFutureMessagesLocked();
        } else {
            removeAllMessagesLocked();
        }

        // We can assume mPtr != 0 because mQuitting was previously false.
        nativeWake(mPtr);
    }
}
```

非安全退出会直接调用removeAllMessagesLocked,它将mMessages队列中所有消息都进行回收；

```
private void removeAllMessagesLocked() {
    Message p = mMessages;
    while (p != null) {
        Message n = p.next;
        p.recycleUnchecked();
        p = n;
    }
    mMessages = null;
}
```

安全退出会直接调用removeAllFutureMessagesLocked

```
private void removeAllFutureMessagesLocked() {
    final long now = SystemClock.uptimeMillis();
    Message p = mMessages;
    if (p != null) {
        if (p.when > now) {
            removeAllMessagesLocked();
        } else {
            Message n;
            //从队列中找到大于当前时间的第一个节点
            for (;;) {
                n = p.next;
                if (n == null) {
                    return;
                }
                if (n.when > now) {
                    break;
                }
                p = n;
            }
            p.next = null;
            //回收找到的节点开始后面所有节点
            do {
                p = n;
                n = p.next;
                p.recycleUnchecked();
            } while (n != null);
        }
    }
}
```

removeAllFutureMessagesLocked回收队列中所有大于当前时间的节点，其它节点依然会被分发；

![avatar](https://p26-passport.byteacctimg.com/img/user-avatar/bde1ef02645bf5d95c45e5b30bef749f~40x40.awebp)