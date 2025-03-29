       ![](https://lf-web-assets.juejin.cn/obj/juejin-web/xitu_juejin_web/img/banner.a5c9f88.jpg)

> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

## 类图

![类图.jpg](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/d60836c9b01446359b8188384a423fd8~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240029&x-signature=jESEMGnZxKOHOUOubwVgj1dScDw%3D) Handler消息系统通过线程间消息通信达到线程切换的目的。

### Looper

消息系统的核心管理类，事件循环、线程隔离、消息分发等都在此类中完成，Looper和线程息息相关，每个线程只有唯一一个Looper，每个Looper也只有唯一一个MessageQueue，因此，**线程-Looper-MessageQueue是一对一的关系**。

-   sMainLooper：UI线程Looper实例，因为一个应用只有一个UI线程，所以它是static实例
-   sThreadLocal：用于保存每个线程对应的Looper，它也是static实例
-   mQueue：消息队列
-   mThread：当前Looper所对应的线程

### Handler

消息生产和消费工具类，一个线程中可以创建多个Handler,但同一个线程无论创建多少个Hanlder，它的mLooper指向的都是同一个Looper实例（这是ThreadLocal所保证的）

-   mLooper：线程所对应的Looper对象
-   mQueue：线程所对应的Looper对象绑定的MessageQueue
-   mCallback：事件消费者，也叫回调
-   mAsynchronous：异步标识；默认创建的Handler，使用sendMessage发送的都是同步消息，同步消息在队列中按照执行时间排序，只有执行时间到时才会被执行；创建的Handler如果开启此设置后，发送的消息都是异步消息，异步消息会被优先执行，这个后面会专门用一节讲“同步屏障”。

### MessageQueue

-   mMessages：消息队列链表头
-   mIdleHandler：空闲Handler，可以添加IdleHandler，当消息队列空闲时会回调mIdleHandler，这样就可以监听消息队列中事件是否已处理完成，有些场景会使用；

### Message

-   data：数据
-   when：消息执行时间戳
-   target：指向发送此消息的Hander，当达到消息处理时间时，会根据target进行消息分发
-   next：指向下一个Message，用于构建MessageQueue链表
-   sPool和sPoolSize：消息回收池子，用于提升消息复用，减少JVM短时间大量对象的创建,这个问题后面也会专门讲解

## 消息系统工作流

![消息系统工作流.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/35049690ba514681a7670be28d7efbf2~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240029&x-signature=RKlUt%2B6m1nZIGTY9wRGeXkM3OI8%3D) 消息系统是一个生产消费者模型，核心有 2 个

-   消息生产者
-   消息消费者 以UI线程向子线程发送消息为例

```
Handler mThreadHandler = null;

new Thread(new Runnable() {
    @Override
    public void run() {
        Looper.prepare();
        mThreadHandler = new Handler(new Handler.Callback() {
            @Override
            public boolean handleMessage(Message message) {
                return false;
            }
        });
        Looper.loop();
    }
}).start();

//这个调用应该是在某个方法中，这里只是个示例
threadHandler.sendMessage()
```

-   代码中创建的threadHandler与子线程的中的Looper、MessageQueue绑定；
-   子线程调用Looper.loop()后从MessageQueue读取消息并处理，如果没有消息则会阻塞，有消息时被唤醒；
-   UI线程通过threadHandler.sendMessage创建消息，添加的MessageQueue中；
-   然后Looper被唤醒，并从中获取消息，最后根据target分发到对应的handler进行消费；

> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 公众号：沐言知识圈  
> 技术交流群（备注：Android），技术交流、程序员搞钱探索 ![Android1.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/a580836e88b94ff89f711894843d556c~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743240029&x-signature=txbtXjgtGtB8YtuO5OZUNrC0Xpw%3D)

![avatar](https://p26-passport.byteacctimg.com/img/user-avatar/bde1ef02645bf5d95c45e5b30bef749f~40x40.awebp)