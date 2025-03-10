## 背景：

前面[分享过一篇文章](https://mp.weixin.qq.com/s?__biz=MzkzOTQ4NDUyNg==&mid=2247489185&idx=1&sn=c0f18ee5006c31de53bc30c4718d05b8&scene=21#wechat_redirect)已经针对Handler和Message进行了详细的源码分析，从上到下对安卓的消息机制进行讲解。在了解了整个消息机制的原理后，可能大家对消息机制都有了一个新的高度。但是想要进一步的深刻理解消息机制的作用，那么还是需要针对消息机制进行相关的实战使用。  
大家在java层面经常都会使用到Hander，Message，Looper，但是学习了前面的消息机制后，其实native层面也是完全可以和java层一样使用消息机制，所以本文主要在native层面来使用Looper，Message，Handler组成的消息机制。  

![在这里插入图片描述](https://mmbiz.qpic.cn/sz_mmbiz_png/DYicOkJDdA2o8u236OUAictBgZUxVyHeVgmeQuuqkibzcD41DGpqUVvibE4SN2gFxYrY69qHYZgiabYM9r7yyuYC7oQ/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1 "null")

在这里插入图片描述

## 回顾java层面消息机制

一般java层一个子线程需要使用消息循环一般会有如下代码：  

![在这里插入图片描述](https://mmbiz.qpic.cn/sz_mmbiz_png/DYicOkJDdA2o8u236OUAictBgZUxVyHeVgvjeHLPVwIxSYMRbRdn3oqxSJnib9Ey5BNafF7t3cytON2lEnjvn3iacA/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1 "null")

在这里插入图片描述

  
主要分为如下几步：  
1、初始化Loop，通过调用Looper.prepare方法  
2、构造基于初始化的Loop构造出Handler  
3、Loop启动循环等待消息  
4、Handler接收处理消息

回顾了Java端的消息机制使用流程后，再来写native的就方便多了。

## native层面消息机制实战代码

其实native端使用消息机制步骤和java端基本上一样，也是如下几步  
1、初始化Loop，通过调用Looper.prepare方法  
2、构造基于初始化的Loop构造出Handler  
3、Loop启动循环等待消息  
4、Handler接收处理消息

其实关于native层面的消息机制使用aosp原生也有相关的测试文件可以参考，本文的实战demo就是基于这个测试案例进行的改造：  
测试文件案例参考地址：

```
system/core/libutils/Looper_test.cpp
```

下面展示改造后的实战demo：

```
#define LOG_TAG "LooperTest"  
#include "Pipe.h"  
#include <stdio.h>  
#include <utils/Looper.h>  
#include <unistd.h>  
#include <time.h>  
#include <utils/threads.h>  
  
  
using namespace android;  
using namespace std;  
Pipe pipe1;//管道对象，主要就是搞个管道fd，这样可以不使用Looper自带fd  
  
class CallbackHandler {  
public:  
    CallbackHandler(){}  
    void setCallback(const sp<Looper>& looper, int fd, int events) {  
        looper->addFd(fd, 0, events, staticHandler, this);//本质也是调用Looper的addFd进行fd监听  
    }  
  
protected:  
    int handler(int fd, int events) {  
        printf("[Thread=%d] %s fd=%d, events=%d,  pipe1.readSignal() = %s\n", gettid(), __func__, fd, events, pipe1.readSignal() == OK ? "OK":"NOT OK");  
        return 0;  
    }  
  
private:  
    static int staticHandler(int fd, int events, void* data) {  
        return static_cast<CallbackHandler*>(data)->handler(fd, events);  
    }  
};  
  
class LooperEventCallback : public LooperCallback {  
  public:  
    using Callback = std::function<int(int fd, int events)>;  
    explicit LooperEventCallback(Callback callback) : mCallback(std::move(callback)) {}  
    //这里handleEvent其实直接调用了mCallback，当然也可以handleEvent直接处理相关业务  
    int handleEvent(int fd, int events, void* /*data*/) override { return mCallback(fd, events); }  
  
  private:  
    Callback mCallback;  
};  
  
class MyMessageHandler : public MessageHandler {  
public:  
    Vector<Message> messages;  
  
    virtual void handleMessage(const Message& message) {  
        printf("[Thread=%d] %s message.what=%d \n", gettid(), __func__, message.what);  
        messages.push(message);  
    }  
};  
  
struct LooperThread : public Thread {  
public:  
    LooperThread(Looper *looper)  
        : mLooper(looper) {  
    }  
  
    virtual bool threadLoop() {  
        if(mLooper == NULL)  
            return false;  
        mLooper->pollOnce(-1);//进行Looper循环  
        return true;  
    }  
  
protected:  
    virtual ~LooperThread() {}  
  
private:  
    Looper *mLooper;  
};  
  
int main()  
{  
    printf("process %s  pid = %d\n", LOG_TAG,getpid());  
     
    sp<Looper> mLooper = new Looper(true);//构建Looper对象  
    sp<LooperThread> mLooperThread = new LooperThread(mLooper.get());//使用该Looper构建对应的Thread  
    mLooperThread->run("LooperThreadTest");//让Thread运行，让Looper进行循环  
  
    //------------------开始使用自带Message进行消息传递，也就是使用的fd其实是Looper自带的mWakeEventFd  
    sp<MyMessageHandler> handler = new MyMessageHandler();  
    printf("[Thread=%d] sendMessage message.what=%d \n", gettid(), 1);  
    mLooper->sendMessage(handler, Message(1));//主线程发送消息  
    sleep(1);  
      
    //-------------------开始使用Looper监听用户自定义fd方式,这里演示两种回调方式   
    //1.直接函数指针回调法  
     
    CallbackHandler mCallbackHandler;  
    //注意这里使用的是 pipe1.receiveFd管道fd  
    mCallbackHandler.setCallback(mLooper, pipe1.receiveFd, Looper::EVENT_INPUT);  
    printf("[Thread=%d] writeSignal 1\n", gettid());  
    pipe1.writeSignal();   
    sleep(1);  
      
      
    //2.使用LooperCallback这种回调方法  
    auto testCallback = [](int fd, int events) {  
  
        if ((events & Looper::EVENT_INPUT) == 0) {  
            return 1;    
        }  
        char buf[1];  
        int size = (int)::read(fd, buf, 1);  
        printf("[Thread %d]  testCallback EVENT_INPUT buf = %c size = %d\n", gettid(),buf[0],size);  
        return 0;    
    };  
    mLooper->addFd( pipe1.receiveFd, 0, Looper::EVENT_INPUT, new LooperEventCallback(std::move(testCallback)), nullptr);  
    printf("[Thread=%d] writeSignal 2\n", gettid());  
    pipe1.writeSignal();  
      
    sleep(2);  
    mLooperThread->requestExit();  
    mLooper.clear();  
}
```

整体实战demo案例设计图如下：  

![在这里插入图片描述](https://mmbiz.qpic.cn/sz_mmbiz_png/DYicOkJDdA2o8u236OUAictBgZUxVyHeVg8WGzbRAaRDsKV529SvSGrhr9Futyh63PEBianp6AOksQ6t1hD0ZY5AA/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1 "null")

在这里插入图片描述

  
代码中本身已经有详细的注释进行解释，这里也就不需要多说，大家看代码既可以。

这里再补充一下Pipe.h的源码：

![图片](https://mmbiz.qpic.cn/sz_mmbiz_png/DYicOkJDdA2o8u236OUAictBgZUxVyHeVgPqH4kGR44uiaPMONCRia7W86Sn2mJ3IQasdjxWoT8KzP5iaSFVP2A8PBg/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

  

实战成果展示：

执行后打印输出如下

```
process LooperTest  pid = 2711  ---进程号，主线程号  
 -------默认Looper自带fd部分  
[Thread=2711] sendMessage message.what=1  ---主线程发送默认消息，传递what为1  
[Thread=2712] handleMessage message.what=1 ---子线程handler中接受到进行处理  
  
 -------自定义Looper的fd部分  
[Thread=2711] writeSignal 1   ---主线程给管道fd写入数据  
[Thread=2712] handler fd=3, events=1,  pipe1.readSignal() = OK  ---子线程通过函数指针方式回调接受到  
  
[Thread=2711] writeSignal 2  ---主线程给管道fd写入数据  
[Thread 2712]  testCallback EVENT_INPUT buf = a size = 1  ---子线程通过LooperCallback方式回调接受到
```

其他framework实战技术干货相关手把手课程资料：

[https://mp.weixin.qq.com/s/Qv8zjgQ0CkalKmvi8tMGaw](https://mp.weixin.qq.com/s?__biz=MzkzOTQ4NDUyNg==&mid=2247484186&idx=1&sn=328a6efaf16b78b1029b3595be03268b&scene=21#wechat_redirect)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_png/DYicOkJDdA2q80uKrooibR0XibaPIIaEMhuuDaxA1NFlug1h29icLU1KtQFTib41IL7wQGDoXpK6baC7F3jB3jhq4pQ/640?wx_fmt=other&from=appmsg&wxfrom=5&wx_lazy=1&wx_co=1&tp=webp)

具体优惠购买方式可以私聊马哥wx号：androidframework007

![图片](data:image/svg+xml,%3C%3Fxml version='1.0' encoding='UTF-8'%3F%3E%3Csvg width='1px' height='1px' viewBox='0 0 1 1' version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink'%3E%3Ctitle%3E%3C/title%3E%3Cg stroke='none' stroke-width='1' fill='none' fill-rule='evenodd' fill-opacity='0'%3E%3Cg transform='translate(-249.000000, -126.000000)' fill='%23FFFFFF'%3E%3Crect x='249' y='126' width='1' height='1'%3E%3C/rect%3E%3C/g%3E%3C/g%3E%3C/svg%3E)