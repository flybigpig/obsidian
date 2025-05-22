---
created: 2025-05-22T14:56:58 (UTC +08:00)
tags: [Android,Linux中文技术社区,前端开发社区,前端技术交流,前端框架教程,JavaScript 学习资源,CSS 技巧与最佳实践,HTML5 最新动态,前端工程师职业发展,开源前端项目,前端技术趋势]
source: https://juejin.cn/post/7245518707581124666
author: 满嘴跑火车的小土匪
---

# 图解 Binder：AIDLAIDL 是一种支持跨进程通信 (IPC) 的接口定义语言，作用就是为我们生成一些 Bind - 掘金

> ## Excerpt
> AIDL 是一种支持跨进程通信 (IPC) 的接口定义语言，作用就是为我们生成一些 Binder 通信的模板代码，减轻开发者的工作量。本文将介绍 AIDL 的使用及工作原理。

---
> 这是一系列的 Binder 文章，会从内核层到 Framework 层，再到应用层，深入浅出，介绍整个 Binder 的设计。详见《[图解 Binder：概述](https://juejin.cn/post/7244018340880007226 "https://juejin.cn/post/7244018340880007226")》。
> 
> 本文基于 Android platform 分支 android-13.0.0\_r1 和内核分支 common-android13-5.15 解析。
> 
> 一些关键代码的链接，可能会因为源码的变动，发生位置偏移、丢失等现象。可以搜索函数名，重新进行定位。

[AIDL](https://link.juejin.cn/?target=https%3A%2F%2Fdeveloper.android.com%2Fguide%2Fcomponents%2Faidl%3Fhl%3Dzh-cn "https://developer.android.com/guide/components/aidl?hl=zh-cn")（Android Interface Definition Language）是一种支持跨进程通信 (IPC) 的接口定义语言。在 Android 系统中，各个进程通常使用 Binder 通信。Android 提供的 AIDL 也是基于 Binder 的。通过 AIDL，我们可以利用其他进程的 Service，调用其他进程的函数。

AIDL 的作用，其实就是为开发者生成两个类：Stub 和 Proxy。这两个类都是一些模板代码，主要目的除了减轻开发者的工作量以外，也是为了隐藏跨进程通信 (IPC) 的各种细节在这两个类中，使得开发者只需要关注实现具体的服务逻辑，而无需关心底层通信的细节。

**Stub 通常用于服务端，Proxy 通常用于客户端。**

## AIDL 的使用

要在 Java 中使用 AIDL，我们需要执行以下步骤：

1.  **无论是客户端，还是服务端**，都要定义 AIDL 接口：创建一个后缀为 .aidl 的文件，并在其中定义一个接口。接口中可以声明需要跨进程调用的函数。例如，创建一个名为 IExampleService.aidl 的文件，其内容如下：
    
    ```
    package com.example;
    
    interface IExampleService {
          String getMessage();
    }
    ```
    
2.  编译生成代码：编译项目时，AIDL 编译器会自动为定义的 AIDL 接口生成对应的 Java 文件：[IExampleService.java](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2Fqiao-coder%2FAIDL%2Fblob%2Fmaster%2Fapp%2Fsrc%2Fmain%2Fjava%2Fcom%2Fexample%2FIExampleService.java "https://github.com/qiao-coder/AIDL/blob/master/app/src/main/java/com/example/IExampleService.java")。生成的文件里包括了 IExampleService 接口、Stub 类和 Proxy 类。
    
3.  在服务端里提供一个 Service，由 Service 提供 Binder 实体：Stub 类继承自 Binder，这意味它本身就是个 Binder 实体，但是它是个抽象类，不提供 getMessage() 的实现。所以，需要我们继承 Stub，然后实现 getMessage()。最后，再创建一个 ExampleService（继承自 Service 类），然后在它的 onBind 里，返回我们的 Binder 实体。
    
    ```
    package com.example;
    
    public class ExampleService extends Service {
        private final IExampleService.Stub mBinder = new IExampleService.Stub() {
            @Override
            public String getMessage() {
                return "Hello from ExampleService!";
            }
        };
    
        @Override
        public IBinder onBind(Intent intent) {
            return mBinder;
        }
    }
    ```
    
    当然，不要忘了在服务端的 AndroidManifest.xml 中注册服务：
    
    ```
    <service android:name=".ExampleService"
        android:exported="true"
        android:enabled="true">
        <intent-filter>
            <action android:name="com.example.IExampleService"/>
        </intent-filter>
    </service>
    ```
    
4.  客户端通过 AIDL ，调用服务端里的 getMessage()：
    
    先在客户端进程中，bindService() 绑定服务端的 ExampleService 服务。例如：
    
    ```
    private IExampleService mExampleService;
    private ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            mExampleService = IExampleService.Stub.asInterface(service);
        }
    
        @Override
        public void onServiceDisconnected(ComponentName className) {
            mExampleService = null;
        }
    };
    
    void bindService() {
        Intent intent = new Intent("com.example.IExampleService");
        intent.setPackage("com.example");
        bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
    }
    
    void unbindService() {
        if (mExampleService != null) {
            unbindService(mConnection);
            mExampleService = null;
        }
    }
    ```
    
    这样，在客户端进程可以通过 AIDL 生成的 Proxy 的 getMessage()，调用服务端的 getMessage()：
    
    ```
    try {
        String message = mExampleService.getMessage();
    } catch (RemoteException e) {
        e.printStackTrace();
    }
    ```
    

## Stub 和 Proxy

在 AIDL 自动生成的 [IExampleService.java](https://link.juejin.cn/?target=https%3A%2F%2Fgithub.com%2Fqiao-coder%2FAIDL%2Fblob%2Fmaster%2Fapp%2Fsrc%2Fmain%2Fjava%2Fcom%2Fexample%2FIExampleService.java "https://github.com/qiao-coder/AIDL/blob/master/app/src/main/java/com/example/IExampleService.java")里，最关键的是 Stub 和 Proxy。它们之间的关系如下图：

![](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f74c021fb5c84720b1578b03a9d484ab~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

Proxy 通常用于客户端，最关键的是它的 getMessage() ：

```
public String getMessage() throws android.os.RemoteException {
    // _data 是写缓冲区
    android.os.Parcel _data = android.os.Parcel.obtain();
    // _reply 是读缓冲区
    android.os.Parcel _reply = android.os.Parcel.obtain();
    String _result;
    try {
        _data.writeInterfaceToken(DESCRIPTOR);
        // TRANSACTION_getMessage 是函数索引
        // mRemote 是 BinderProxy 的实例
        boolean _status = mRemote.transact(Stub.TRANSACTION_getMessage, _data, _reply, 0);
        if (!_status && getDefaultImpl() != null) {
            return getDefaultImpl().getMessage();
        }
        _reply.readException();
        // 从读缓冲区读取远程调用的结果
        _result = _reply.readString();
    } finally {
        _reply.recycle();
        _data.recycle();
    }
    return _result;
}
```

Stub 通常用于服务端。Stub 继承自 Binder，代表它就是一个 Binder 实体。它是一个抽象类，不会实现 getMessage()，交给子类去实现。我们需要留意一下它的 onTransact()：

```
public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags)
throws android.os.RemoteException {
    String descriptor = DESCRIPTOR;
    switch (code) {
        case INTERFACE_TRANSACTION: {
            reply.writeString(descriptor);
            return true;
        }
        // 目标函数的索引
        case TRANSACTION_getMessage: {
            data.enforceInterface(descriptor);
            // 调用 getMessage()
            String _result = this.getMessage();
            reply.writeNoException();
            // 返回结果给客户端
            reply.writeString(_result);
            return true;
        }
        default: {
            return super.onTransact(code, data, reply, flags);
        }
    }
}
```

在一次 getMessage() 的跨进程调用里，它们的作用如下：

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/58b040c354904dc6881da2d28a7a8720~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## transact()

transact() 会向 Binder 驱动发起事务，大致的调用链路是：

BinderProxy.java

└─[transact()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FBinderProxy.java%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D526 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/android/os/BinderProxy.java;bpv=1;bpt=0;l=526")

└──[transactNative()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FBinderProxy.java%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D614 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/android/os/BinderProxy.java;bpv=1;bpt=0;l=614")

└───android\_util\_Binder.cpp.cpp

└────[android\_os\_BinderProxy\_transact()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D1387 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/jni/android_util_Binder.cpp;bpv=1;bpt=0;l=1387")

└─────BpBinder.cpp

└──────[transact()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FBpBinder.cpp%3Bl%3D275%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/BpBinder.cpp;l=275;bpv=1;bpt=0")

└───────IPCThreadState.cpp

└────────[self()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D287 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=287")

└─────────[transact()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D706 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=706")

└──────────[writeTransactionData()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D1104 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=1104")

└──────────[waitForResponse()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D897 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=0;bpt=0;l=897")

└───────────[talkWithDriver()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D0%3Bbpt%3D0%3Bl%3D997 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=0;bpt=0;l=997")

└────────────ioctl.cpp

└─────────────[ioctl()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Abionic%2Flibc%2Fbionic%2Fioctl.cpp%3Bl%3D34%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:bionic/libc/bionic/ioctl.cpp;l=34;bpv=1;bpt=0")

注意：从 transact() 到 ioctl() ，这个过程都是同步调用。即当我们通过 AIDL 调用远程服务时，客户端会等待服务端完成处理并返回结果。整个过程中，客户端线程会阻塞，直到收到来自服务端的回应。所以，我们千万别在主线程发起调用。

## onTransact()

服务端的 Binder 线程池的线程接收到 BR\_TRANSACTION 消息后，会一路调用到 onTransact()：

IPCThreadState.cpp

└─[executeCommand()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FIPCThreadState.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D1147 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp;bpv=1;bpt=0;l=1147") // 读取服务端的读缓冲区数据，处理 BR\_TRANSACTION 消息

└──Binder.cpp

└───[BBinder::transact()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fnative%2Flibs%2Fbinder%2FBinder.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D270 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/native/libs/binder/Binder.cpp;bpv=1;bpt=0;l=270") // 根据事务数据里的函数索引、参数，完成函数调用。

└────JavaBBinder.cpp（继承自 BBinder）

└─────[onTransact()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D394 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/jni/android_util_Binder.cpp;bpv=1;bpt=0;l=394")

└──────Binder.java

└───────[execTransact()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FBinder.java%3Bl%3D1237%3Bbpv%3D1%3Bbpt%3D0 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/android/os/Binder.java;l=1237;bpv=1;bpt=0")

└────────[execTransactInternal()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Aframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FBinder.java%3Bbpv%3D1%3Bbpt%3D0%3Bl%3D1250 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:frameworks/base/core/java/android/os/Binder.java;bpv=1;bpt=0;l=1250")

└─────────onTransact() // Stub 会覆盖父类的 onTransact()

## BpBinder 与 BinderProxy、BBinder 与 Binder

transact() 和 onTransact() 的调用链路里出现了 BpBinder、BinderProxy、BBinder（JavaBBinder）和 Binder。

在 Binder 事务一文，已经提到过，BBinder 即 Binder 实体，而 BpBinder 即 Binder 代理，持有着 Binder 引用。它们两个都是 C++ 里的类。

而 Binder、BinderProxy 则是它们的 Java 层表示。如下图：

![](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/9b6583c585fa4898b4206bdabf20698c~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 应用级服务

服务端提供的 ExampleService 属于应用级服务。在前面的示例中，我们在客户端里会通过 bindService() 启动它，并在 ServiceConnection 回调里，拿到了所需的 Binder 引用。

应用级别的服务通常不会注册到 ServiceManager，而是由 AMS 进行管理。

当客户端发起 bindService() 调用后，会通过 Binder 调用到 AMS 的 bindServiceLocked()，进行服务的绑定。如果服务还没启动，AMS 会通过 Binder 通信，通知服务端创建、启动和绑定服务。服务端执行绑定逻辑后，会传递 Binder 实体给 Binder 驱动，Binder 驱动转换为 Binder 引用后，再传给 AMS。最后由 AMS 转交给客户端，即最终会执行 ServiceConnection 回调的 onServiceConnected()。

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f4c9ca943c4e4c279f1e988007c468a0~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

-   Binder 实体是服务端里的 IExampleService.Stub mBinder。
-   Binder 引用均是经过 Binder 驱动转换的，上图没有画出 Binder 驱动。

## aidl-cpp

C++ 层也可以通过 [aidl-cpp](https://link.juejin.cn/?target=https%3A%2F%2Fandroid.googlesource.com%2Fplatform%2Fsystem%2Ftools%2Faidl%2F%2B%2Fbrillo-m10-dev%2Fdocs%2Faidl-cpp.md "https://android.googlesource.com/platform/system/tools/aidl/+/brillo-m10-dev/docs/aidl-cpp.md") 来使用 AIDL。
