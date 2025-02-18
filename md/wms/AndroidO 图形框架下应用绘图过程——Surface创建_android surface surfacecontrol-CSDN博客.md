Android图形[框架](https://so.csdn.net/so/search?q=%E6%A1%86%E6%9E%B6&spm=1001.2101.3001.7020)在前几年已经分析过了，不过，随着Android版本的升级，虽然框架主体未变，但有些细节变动还是比较大的，应网友要求，今天再次以AndroidO为基础，重新介绍图形框架实现，Android图形框架包括以下三大部分：

1\. 应用绘图；

2\. SurfaceFlinger混合图层；

3\. Hwc Hal实现

本文首先介绍AndroidO下应用的绘图过程，应用绘图也分为以下几个步骤：

```
sp<SurfaceComposerClient> client = new SurfaceComposerClient();  
sp<SurfaceControl> surfaceControl = client->createSurface(String8("resize"),  160, 240, PIXEL_FORMAT_RGB_565, 0);  
sp<Surface> surface = surfaceControl->getSurface();  
SurfaceComposerClient::openGlobalTransaction();  
surfaceControl->setLayer(100000);  
SurfaceComposerClient::closeGlobalTransaction();  
ANativeWindow_Buffer outBuffer;  
surface->lock(&outBuffer, NULL);  
ssize_t bpr = outBuffer.stride * bytesPerPixel(outBuffer.format);  
android_memset16((uint16_t*)outBuffer.bits, 0xF800, bpr*outBuffer.height);  
surface->unlockAndPost();  
```

1\. 应用创建Surface过程

2\. 应用申请图形buffer过程

3\. 应用画图过程

4\. 应用归还图形buffer过程

接下来详细分析这三个步骤，尽量把Android应用绘图整个脉络捋清楚。

## 应用创建Surface过程

我们知道，画图需要画纸，画笔，那么对于Android系统来说，应用程序画图同样需要画纸和画笔，在上一节博文中已经介绍了，在Android系统中，画纸其实就是Surface，而画笔却有多种，不同角色的画图者采用的画笔也不相同，对应应用程序而言，如果是画2D图形，那么画笔采用Skia，如果是画3D图形，那么画笔采用OpenGL，而对应SurfaceFlinger而言，他需要使用OpenGL来混合图形，这里先介绍画纸，在Android4.2之前，画纸也有两种，一种是Surface，另一种是FramebufferNativeWindow，但是从Android4.2版本之后，无论是应用程序还是SurfaceFlinger，画纸统一使用Surface，因此这里首先介绍Surface的创建过程，当然应用程序和SurfaceFlinger创建画纸Surface的过程不经相同，这里首先介绍应用程序创建Surface的过程。

![](https://i-blog.csdnimg.cn/blog_migrate/9c49bfef6158eb011a5d7b235748f343.png)

### 应用程序请求创建Surface

我们知道应用程序启动的所有窗口都需要通过WindowManager.addView注册到WMS维护的数据结构中，在这里将创建ViewRootImpl对象，也就是说每个应用程序窗口都对应有一个ViewRootImpl对象，该对象是与WMS通信的接口。

![](https://i-blog.csdnimg.cn/blog_migrate/b8043091594b9abf8156f0529e4ab9bf.png)

在应用程序的ViewRootImpl类中定义了成员变量mSurface，

```
private final Surface mSurface = new Surface();
```

由此可知应用程序进程为每个窗口对象都创建了一个Surface对象。

![](https://i-blog.csdnimg.cn/blog_migrate/6ff82379e7ef47ed695f9c5a76706ecc.png)

Surface有3种构造函数，这里采用的是第一种构造函数，即创建一个空的Surface对象，并没有初始化该Surface的native层，其实应用程序进程的Surface创建过程是由WMS服务来完成，WMS服务通过Binder跨进程方式将创建好Surface返回给应用程序进程。我们知道，在Android系统中，如果一个对象需要在不同进程间传输，必须实现Parcelable接口，Surface类正好实现了Parcelable接口。

![](https://i-blog.csdnimg.cn/blog_migrate/0718cdb5bf8609521323513d364bf324.png)

ViewRootImpl通过IWindowSession接口请求WMS的完整过程如下：

![](https://i-blog.csdnimg.cn/blog_migrate/c5d61f14d0e2e5b0267ee50e1b5a734a.png)

这是什么意思呢？其实应用程序这边的窗口只是创建了一个空的Surface，而真正的Surface是由WMS创建的，WMS最终会将创建好的Surface传递给应用程序。因此这里就涉及到应用程序和WMS之间的RPC通信，这个通信过程是通过IWindowSession完成的，应用程序调用relayoutWindow来请求WMS为其创建一个Surface对象。frameworks\\base\\core\\Java\\android\\view\\ViewRootImpl.Java

```java
private int relayoutWindow(
    WindowManager.LayoutParams params,
    int viewVisibility,
    boolean insetsPending
) throws RemoteException {
    float appScale = mAttachInfo.mApplicationScale;
    boolean restore = false;
    if (params != null && mTranslator != null) {
        restore = true;
        params.backup();
        mTranslator.translateWindowLayout(params);
    }
    if (params != null) {
        if (DBG) Log.d(mTag, "WindowLayout in layoutWindow:" + params);
    }
    mPendingMergedConfiguration.getMergedConfiguration().seq = 0;
    if (params != null && mOrigWindowType != params.type) {
        if (mTargetSdkVersion < Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
            Slog.w(
                mTag,
                "Window type can not be changed after " +
                "the window is added; ignoring change of " +
                mView
            );
            params.type = mOrigWindowType;
        }
    }
    int relayoutResult = mWindowSession.relayout(
        mWindow,
        mSeq,
        params,
        (int) (mView.getMeasuredWidth() * appScale + 0.5f),
        (int) (mView.getMeasuredHeight() * appScale + 0.5f),
        viewVisibility,
        insetsPending ? WindowManagerGlobal.RELAYOUT_INSETS_PENDING : 0,
        mWinFrame,
        mPendingOverscanInsets,
        mPendingContentInsets,
        mPendingVisibleInsets,
        mPendingStableInsets,
        mPendingOutsets,
        mPendingBackDropFrame,
        mPendingMergedConfiguration,
        mSurface
    );
    mPendingAlwaysConsumeNavBar = (relayoutResult &
        WindowManagerGlobal.RELAYOUT_RES_CONSUME_ALWAYS_NAV_BAR) !=
    0;
    if (restore) {
        params.restore();
    }
    if (mTranslator != null) {
        mTranslator.translateRectInScreenToAppWinFrame(mWinFrame);
        mTranslator.translateRectInScreenToAppWindow(mPendingOverscanInsets);
        mTranslator.translateRectInScreenToAppWindow(mPendingContentInsets);
        mTranslator.translateRectInScreenToAppWindow(mPendingVisibleInsets);
        mTranslator.translateRectInScreenToAppWindow(mPendingStableInsets);
    }
    return relayoutResult;
}

```

这里将变量mSurface以参数的形式传递给WMS，在RPC调用过程中，会完成这个变量的赋值。

framework\_intermediates\\src\\core\\Java\\android\\view\\IWindowSession.Java$ Proxy

```java
public int relayout(android.view.IWindow window, int seq,
		android.view.WindowManager.LayoutParams attrs,
		int requestedWidth, int requestedHeight,
		...
		android.view.Surface outSurface)
		throws android.os.RemoteException {
	android.os.Parcel _data = android.os.Parcel.obtain();
	android.os.Parcel _reply = android.os.Parcel.obtain();
	int _result;
	try {
		_data.writeInterfaceToken(DESCRIPTOR);
		_data.writeStrongBinder((((window != null)) ? (window.asBinder()) : (null)));
		...
		_data.writeInt(viewVisibility);
		_data.writeInt(flags);
		mRemote.transact(Stub.TRANSACTION_relayout, _data, _reply,0);
		_reply.readException();
		_result = _reply.readInt();
		…
         //读取WMS中创建的Surface对象
		if ((0 != _reply.readInt())) {
			outSurface.readFromParcel(_reply);
		}
	} finally {
		_reply.recycle();
		_data.recycle();
	}
	return _result;
}
```

从IWindowSession代理类Proxy的relayout函数实现可以看出，应用程序进程中创建的Surface对象并没有传递到WMS服务进程，只是读取WMS服务进程返回来的Surface。那么WMS服务是如何响应应用程序进程窗口布局请求的呢？

### WMS创建Surface过程

framework\_intermediates\\src\\core\\Java\\android\\view\\IWindowSession.Java$Stub

```java
public boolean onTransact(int code, android.os.Parcel data,android.os.Parcel reply, int flags)throws android.os.RemoteException {
	switch (code) {
	case TRANSACTION_relayout: {
		data.enforceInterface(DESCRIPTOR);
		android.view.IWindow _arg0;
		_arg0 = android.view.IWindow.Stub.asInterface(data.readStrongBinder());
		…
		android.view.Surface _arg13;
		_arg13 = new android.view.Surface();
		int _result = this.relayout(_arg0, _arg1, _arg2, _arg3, _arg4,
				_arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11,_arg12, _arg13);
		reply.writeNoException();
		reply.writeInt(_result);
		….
		if ((_arg13 != null)) {
			reply.writeInt(1);
			_arg13.writeToParcel(reply,android.os.Parcelable.PARCELABLE_WRITE_RETURN_VALUE);
		} else {
			reply.writeInt(0);
		}
		return true;
	}
	}
	return super.onTransact(code, data, reply, flags);
}
```

WMS服务在响应应用程序进程请求relayout调用时，首先在当前进程空间创建一个Surface对象，然后调用Session的relayout()函数进一步完成窗口布局过程，最后将WMS服务中创建的Surface返回给应用程序。

![](https://i-blog.csdnimg.cn/blog_migrate/374b26e9d8f9d8291e0a0e9f799ceb93.png)

到目前为止，在应用程序进程和WMS服务进程分别创建了一个Java层的Surface对象，但是他们调用的都是Surface的无参构造函数，创建的都是空Surface对象。那么WMS服务这边的Java层Surface是如何被初始化的呢？ native层的Surface是在那里创建的呢？

frameworks\\base\\services\\core\\Java\\com\\android\\server\\wm\\Session.Java

```
public int relayout(IWindow window, int seq, WindowManager.LayoutParams attrs,
        int requestedWidth, int requestedHeight, int viewFlags,
        int flags, Rect outFrame, Rect outOverscanInsets, Rect outContentInsets,
        Rect outVisibleInsets, Rect outStableInsets, Rect outsets, Rect outBackdropFrame,
        MergedConfiguration mergedConfiguration, Surface outSurface) {
    if (false) Slog.d(TAG_WM, ">>>>>> ENTERED relayout from "
            + Binder.getCallingPid());
    Trace.traceBegin(TRACE_TAG_WINDOW_MANAGER, mRelayoutTag);
    int res = mService.relayoutWindow(this, window, seq, attrs,
            requestedWidth, requestedHeight, viewFlags, flags,
            outFrame, outOverscanInsets, outContentInsets, outVisibleInsets,
            outStableInsets, outsets, outBackdropFrame, mergedConfiguration, outSurface);
    Trace.traceEnd(TRACE_TAG_WINDOW_MANAGER);
    if (false) Slog.d(TAG_WM, "<<<<<< EXITING relayout to "
            + Binder.getCallingPid());
    return res;
}
```

Session又将调用WMS的relayoutWindow函数来布局窗口。

frameworks\\base\\services\\core\\Java\\com\\android\\server\\wm\\WindowManagerService.Java

![](https://i-blog.csdnimg.cn/blog_migrate/8c7ab936cca500b4029efb606e6d4cac.png)

![](https://i-blog.csdnimg.cn/blog_migrate/a2101028598e814c8cd6f708ef832b90.png)

WMS在布局窗口过程中，首先调用WindowStateAnimator类的createSurfaceLocked()函数来创建一个SurfaceControl对象，然后调用Surface类的copyFrom函数将SurfaceControl中持有的native层Surface关联到前面由WMS服务创建的Java层Surface对象中。

frameworks\\base\\services\\core\\Java\\com\\android\\server\\wm\\WindowStateAnimator.Java

![](https://i-blog.csdnimg.cn/blog_migrate/5a61a51a3a1b85aa1e5276072fc69631.png)

createSurfaceLocked()函数根据图像的宽、高、格式等信息创建并初始化Java层SurfaceControl对象。

frameworks\\base\\services\\core\\java\\com\\android\\server\\wm\\WindowSurfaceController.java

![](https://i-blog.csdnimg.cn/blog_migrate/7915b93b209eede5ff5dde0298144659.png)

frameworks\\base\\core\\Java\\android\\view\\SurfaceControl.Java

![](https://i-blog.csdnimg.cn/blog_migrate/eabdf81d5e49f6f5d3db077bc90f0c9e.png)

在SurfaceControl的构造函数中通过调用JNI函数nativeCreate将为当前的Java层SurfaceControl对象创建相关的native对象。

frameworks\\base\\core\\jni\\android\_view\_SurfaceControl.cpp

![](https://i-blog.csdnimg.cn/blog_migrate/34134289e5b2bd2987c413b331ad0bcd.png)

首先调用android\_view\_SurfaceSession\_getClient函数从Java层的SurfaceSession对象的成员变量mNativeClient中取出SurfaceComposerClient对象指针，通过该对象向SurfaceFlinger端的Client对象发送创建Surface的请求，然后创建一个native的SurfaceControl对象。

frameworks\\native\\libs\\gui\\SurfaceComposerClient.cpp

![](https://i-blog.csdnimg.cn/blog_migrate/b2f1e8f7ea3d52e3c6ebea73df90a265.png)

通过前面的分析，我们已经很清晰地知道应用程序进程和WMS服务两边都会创建Surface，但请求SurfaceFlinger为当前Surface创建对应的Layer的工作是由WMS来完成的，WMS服务在构造Java层的SurfaceControl对象过程中使用ISurfaceComposerClient的代理对象BpSurfaceComposerClient请求SurfaceFlinger进程中专为当前进程服务的Client为当前Surface创建对应的Layer。

![](https://i-blog.csdnimg.cn/blog_migrate/41074996fef2e337721f98e30b18a728.png)

frameworks\\native\\libs\\gui\\ISurfaceComposerClient.cpp$BpSurfaceComposerClient

```c

virtual status_t createSurface(const String8& name, uint32_t w,
		uint32_t h, PixelFormat format, uint32_t flags,
		sp<IBinder>* handle,sp<IGraphicBufferProducer>* gbp) {
	Parcel data, reply;
	data.writeInterfaceToken(ISurfaceComposerClient::getInterfaceDescriptor());
	data.writeString8(name);
	data.writeInt32(w);
	data.writeInt32(h);
	data.writeInt32(format);
	data.writeInt32(flags);
	remote()->transact(CREATE_SURFACE, data, &reply);
	*handle = reply.readStrongBinder();
	*gbp = interface_cast<IGraphicBufferProducer>(reply.readStrongBinder());
	return reply.readInt32();
}
————————————————

                            版权声明：本文为博主原创文章，遵循 CC 4.0 BY-SA 版权协议，转载请附上原文出处链接和本声明。
                        
原文链接：https://blog.csdn.net/yangwen123/article/details/80674965
```

在SurfaceFlinger端的Client本地对象会返回一个IGraphicBufferProducer代理对象给WMS，WMS通过该代理对象为当前创建的Surface创建一个SurfaceControl对象。Native层的SurfaceControl对象的构造过程如下：

frameworks\\native\\libs\\gui\\SurfaceControl.cpp

![](https://i-blog.csdnimg.cn/blog_migrate/5c697a57451c4ad00174c6c01b8a1d22.png)

到此WMS服务创建Surface的任务就完成了，在为应用程序进程创建Surface过程中，创建的对象类型越来越多，在此总结一下:  
1.  WMS服务在接收应用程序进程窗口布局请求时创建了一个空的Java层Surface对象；  
2.  WMS服务在布局窗口过程中，创建了一个Java层的SurfaceControl对象；  
3.  在构造Java层的SurfaceControl对象时，请求SurfaceFlinger为应用程序进程创建的Surface创建对应的Layer对象。4.   创建了一个native的SurfaceControl对象，并且关联到Java层的SurfaceControl对象中。

![](https://i-blog.csdnimg.cn/blog_migrate/fdbd4cf80436ae9e50c812c20f600b6f.png)

到目前为止，应用程序这边创建了一个Java层的Surface，在WMS服务这边也创建了一个Java层的Surface对象、一个Java层的SurfaceControl对象和一个native层的SurfaceControl对象，接着WMS服务通过执行surfaceController.getSurface(outSurface);

frameworks\\base\\services\\core\\java\\com\\android\\server\\wm\\WindowSurfaceController.java

![](https://i-blog.csdnimg.cn/blog_migrate/3196d06463d273c34babb30265b8c6c1.png)

frameworks\\base\\core\\java\\android\\view\\Surface.java

![](https://i-blog.csdnimg.cn/blog_migrate/86670481de6225d27c2d121d00e0cc25.png)

frameworks\\base\\core\\jni\\android\_view\_Surface.cpp

![](https://i-blog.csdnimg.cn/blog_migrate/bda0625f1bffa25a99ebdfaadd65b578.png)

frameworks\\native\\libs\\gui\\SurfaceControl.cpp

![](https://i-blog.csdnimg.cn/blog_migrate/f8b68dd12276aad4c3f73390826a4cab.png)

函数将创建一个native层的Surface对象，并将该对象指针返回给Java层的Surface，从而建立Java层Surface和native层Surface的关联关系。

frameworks\\native\\libs\\gui\\Surface.cpp

![](https://i-blog.csdnimg.cn/blog_migrate/534f2353ee2a103a90593137acb9303f.png)

到此才算真正创建了一个可用于绘图的Surface，整个Surface的创建过程都是由WMS服务来完成。

![](https://i-blog.csdnimg.cn/blog_migrate/fde6b4ef48e6679adcc1f6551dbe0663.png)

Android4.4之前版本中，在WMS服务进程端会创建两个Java层的Surface对象。第一个Surface使用了无参构造函数，仅仅构造一个Surface对象而已，而第二个Surface却使用了有参构造函数，参数指定了图象宽高等信息，这个Java层Surface对象还会在native层创建一个真正能用于绘制图象的native层Surface，同时请求SurfaceFlinger为该Surface创建一个Layer对象。最后通过浅拷贝的方式将第二个Surface复制到第一个Surface中，最后通过writeToParcel方式写回到应用程序进程。

![](https://i-blog.csdnimg.cn/blog_migrate/49dc2fa66980349bc3210a2007f4699a.png)

从上图可以看到，Android4.4版本开始对Surface创建进行了整理，结构更加清晰。在应用程序这边创建Surface对象，该Surface用于画图，而在WMS这边创建了一个SurfaceControl对象，用于控制图层信息，无论是应用程序这边的Surface还是WMS这边的SurfaceControl，他们都持有IGraphicBufferProducer接口，用于操作位于SurfaceFlinger进程中的图形buffer。

### WMS返回Surface过程

WMS服务为应用程序创建好Surface后，通过writeToParcel方式写回到应用程序进程。

```
if ((_arg13 != null)) {reply.writeInt(1);_arg13.writeToParcel(reply,android.os.Parcelable.PARCELABLE_WRITE_RETURN_VALUE);}
```

在给应用程序返回结果时，先向应用程序进程发送数字1，然后将WMS这边创建的Java层Surface对象返回给应用程序进程。

frameworks\\base\\core\\Java\\android\\view\\Surface.Java![](https://i-blog.csdnimg.cn/blog_migrate/645ca1c8a6bb7b44c30c026cd4512387.png)

frameworks\\base\\core\\jni\\android\_view\_Surface.cpp

![](https://i-blog.csdnimg.cn/blog_migrate/8fbaaeaae216773a24bc444f57001d3c.png)

从WMS服务向应用程序进程返回Surface过程可知，WMS返回的并不是Surface对象，而是IGraphicBufferProducer的代理对象。WMS服务请求SurfaceFlinger为Surface创建Layer对象时，SurfaceFlinger不仅在自身进程空间创建了一个Layer对象，同时还为这个Layer对象创建了一个IGraphicBufferProducer本地Binder对象，用于管理当前创建的Surface的图形buffer。同时将其代理对象返回给WMS服务，WMS得到IGraphicBufferProducer代理对象后，将其保存在自身进程空间的native的SurfaceControl中，同时也将该IGraphicBufferProducer代理对象返回给应用程序进程。

![](https://i-blog.csdnimg.cn/blog_migrate/337819d7496cb649ac8696ba07dee515.png)

### 应用读取Surface过程

frameworks\\base\\core\\java\\android\\view\\Surface.java

![](https://i-blog.csdnimg.cn/blog_migrate/06d5f9a0f9848924e5af3733cf2dd859.png)

通过前面的分析，我们知道WMS服务通过writeToParcel函数将IGraphicBufferProducer代理对象写回给了应用程序进程，应用程序在请求WMS服务执行完窗口布局之后，将通过readFromParcel函数读取WMS服务返回来的结果。变量mNativeObject记录的是应用程序这边的Java层Surface关联的native层Surface对象指针，应用程序进程首先通过nativeReadFromParcel函数在应用程序进程这边创建一个native的Surface对象，因为此时应用程序进程这边只创建了一个Java层的Surface，同时该Surface并未关联任何native层 的Surface对象，然后调用setNativeObjectLocked函数将创建的native层Surface和Java层Surface对象建立关联关系，也就是将native层的Surface对象指针保存到Java层Surface对象的成员变量mNativeObject中。

frameworks\\base\\core\\jni\\android\_view\_Surface.cpp

![](https://i-blog.csdnimg.cn/blog_migrate/719ffbdb528b1f74fb69bab1302b6fff.png)

到此，应用程序进程和WMS服务都分别创建了一个Java层的Surface和一个native的Surface对象，同时WMS服务还创建了SurfaceControl对象来调整Surface的属性，我们来对比一下应用程序进程和WMS服务创建Java层Surface的方法：  
1.    应用程序进程  
mSurface = new Surface();  
2.    WMS服务  
\_arg13 = new android.view.Surface();  
由此可以看出，无论是应用程序进程还是WMS服务，都只是创建了一个空的Java层Surface对象。  
接下来比较一下native层Surface的创建过程：  
1.    应用程序进程  
sur = new Surface(gbp, true);  
2.    WMS服务  
mSurfaceData = new Surface(mGraphicBufferProducer, false);  
其中Surface构造函数的第一个参数都引用的是IGraphicBufferProducer代理对象，第二个参数描述当前Surface是属于应用程序进程的呢还是WMS服务的，如果为true，则表示这是应用程序端的Surface对象，反之亦然。

![](https://i-blog.csdnimg.cn/blog_migrate/a4cb2d59665b5c99ac08f45162c8e2a1.png)

### SurfaceFlinger创建图层过程

frameworks\\native\\services\\surfaceflinger\\Client.cpp

![](https://i-blog.csdnimg.cn/blog_migrate/dcd7d333a4dfbfcc9d172ffe6db11c47.png)

Client将应用程序创建Surface的请求转换为异步消息投递到SurfaceFlinger的消息队列中，将创建Surface的任务转交给SurfaceFlinger，因为同一时刻可以有多个应用程序请求SurfaceFlinger为其创建Surface，通过消息队列可以实现请求排队，然后SurfaceFlinger依次为应用程序创建Surface。

![](https://i-blog.csdnimg.cn/blog_migrate/104ae94e193b70fdcedda9b465cb8fb4.png)

![](https://i-blog.csdnimg.cn/blog_migrate/2b6839b4c68d336c3c83d95fee6fd91f.png)

frameworks\\native\\services\\surfaceflinger\\SurfaceFlinger.cpp

![](https://i-blog.csdnimg.cn/blog_migrate/9d3cd05048ce484ac311f805bf96d143.png)

该函数中根据flag创建不同的Layer，Layer用于标示一个图层。SurfaceFlinger为应用程序创建好Layer后，需要统一管理这些Layer对象，因此通过函数addClientLayer将创建的Layer保存到当前State的Z秩序列表layersSortedByZ中，同时将这个Layer所对应的IGraphicBufferProducer本地Binder对象gbp保存到SurfaceFlinger的成员变量mGraphicBufferProducerList中。除了SurfaceFlinger需要统一管理系统中创建的所有Layer对象外，专门为每个应用程序进程服务的Client也需要统一管理当前应用程序进程所创建的Layer，因此在addClientLayer函数里还会通过Client::attachLayer将创建的Layer和该类对应的handle以键值对的方式保存到Client的成员变量mLayers表中。

![](https://i-blog.csdnimg.cn/blog_migrate/ba32b8c3143814948ed7ae91357ef36c.png)

![](https://i-blog.csdnimg.cn/blog_migrate/ca5a77ab34a89eeca33482f567d2eeac.png)

frameworks\\native\\services\\surfaceflinger\\Layer.cpp

![](https://i-blog.csdnimg.cn/blog_migrate/d541b1421cdbab0d53d0cadd45c104c2.png)

frameworks\\native\\libs\\gui\\BufferQueue.cpp

![](https://i-blog.csdnimg.cn/blog_migrate/ac515dc5ceb4c9fe130852012ade0394.png)

![](https://i-blog.csdnimg.cn/blog_migrate/76deb7e4cab4bccc154be6557a2dde21.png)

1.    BufferQueue  
可以认为BufferQueue是一个服务中心，IGraphicBufferProducer和IGraphicBufferConsumer  
所需要使用的buffer必须要通过它来管理。比如说当IGraphicBufferProducer想要获取一个buffer时，它不能越过BufferQueue直接与IGraphicBufferConsumer进行联系，反之亦然。这有点像房产中介一样，房主与买方的任何交易都需要经过中介的同意，私自达成的协议都是违反规定的  
2.    IGraphicBufferProducer  
IGraphicBufferProducer就是“填充”buffer空间的人，通常情况下是应用程序。因为应用程序不断地刷新UI，从而将产生的显示数据源源不断地写到buffer中。当IGraphicBufferProducer需要使用一块buffer时，它首先会向中介BufferQueue发起dequeueBuffer申请，然后才能对指定的buffer进行操作。此时buffer就只属于IGraphicBufferProducer一个人的了，它可以对buffer进行任何必要的操作，而IGraphicBufferConsumer此刻绝不能操作这块buffer。当IGraphicBufferProducer认为一块buffer已经写入完成后，它进一步调用queueBuffer函数。从字面上看这个函数是“入列”的意思，形象地表达了buffer此时的操作，把buffer归还到BufferQueue的队列中。一旦queue成功后，buffer的owner也就随之改变为BufferQueue了。  
3.    IGraphicBufferConsumer

IGraphicBufferConsumer是与IGraphicBufferProducer相对应的，它的操作同样受到BufferQueue的管控。当一块buffer已经就绪后，IGraphicBufferConsumer就可以开始工作了。

![](https://i-blog.csdnimg.cn/blog_migrate/06f54e923c061f25ea2972221a810c49.png)

![](https://i-blog.csdnimg.cn/blog_migrate/4fb706c243598aa9a1b929e116d33c00.png)

总结一下应用程序创建Surface的过程：  
1.    每一个应用程序窗口都有且只有一个ViewRootImpl对象，在该对象中会创建一个Java层的Surface；  
2.    当应用程序向WMS服务请求布局一个窗口时，WMS服务在接收请求时会创建一个Java层的Surface；  
3.    WMS服务在布局窗口过程中，在自身进程空间创建一个Java层的SurfaceControl对象及native的SurfaceControl对象。  
4.    在创建native的SurfaceControl对象时，WMS将请求SurfaceFlinger在它的进程空间为当前创建的Surface创建对应的Layer对象，并向WMS返回IGraphicBufferProducer代理对象。  
5.    WMS使用SurfaceFlinger返回过来的IGraphicBufferProducer代理对象在WMS服务端创建一个native的Surface；

6.    WMS服务将IGraphicBufferProducer代理对象以窗口布局结果返回给应用程序进程，应用程序进程得到该代理对象后，在自身进程空间创建一个native的Surface。

![](https://i-blog.csdnimg.cn/blog_migrate/b9c2073351d3865cabdc0f7b4713b5bc.png)

无论是应用程序进程还是WMS服务进程，它们拥有的IGraphicBufferProducer代理对象引用SurfaceFlinger进程中的同一个IGraphicBufferProducer本地对象。