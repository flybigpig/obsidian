
我们知道WindowManagerService服务运行在SystemServer进程中，应用程序启动Activity时，需要请求WMS为启动的Activity创建对应的窗口，同时WMS也负责修改窗口属性，因此这里就涉及到应用程序进程与WMS服务之间的跨进程交互过程。在前面我们介绍了Android中的Binder通信机制，应用程序进程正是使用Binder[通信方式](https://so.csdn.net/so/search?q=%E9%80%9A%E4%BF%A1%E6%96%B9%E5%BC%8F&spm=1001.2101.3001.7020)和SystemServer进程交互的。

![](https://i-blog.csdnimg.cn/blog_migrate/8933e430d210ca5ae23a156512a6af0b.jpeg)

在应用程序进程中启动的每一个Activity都拥有一个ViewRootImpl对象：



```java
// frameworks\\base\\core\\java\\android\\view\\ViewRootImpl.java
public ViewRootImpl(Context context, Display display) {
	//在WMS服务中创建Session Binder对象，同时返回Binder代理对象给当前应用程序进程，并保存在ViewRootImpl的成员变量mWindowSession中
	mWindowSession = WindowManagerGlobal.getWindowSession();
	...
	//为每一个ViewRootImpl对象创建W Binder对象，用于WMS服务访问对应的Activity
	mWindow = new W(this);
	...
	mAttachInfo = new View.AttachInfo(mWindowSession, mWindow, display, this, mHandler, this);
	...
	mChoreographer = Choreographer.getInstance();
	...
	loadSystemProperties();
}
```

### 1.建立应用程序到WMS服务之间的连接

从上面图中可以看到，要使应用程序进程可以请求WMS服务，必须在WMS服务这边创建一个类型为Session的Binder本地对象，同时应用程序这边获取Session的代理对象，通过该代理对象，应用程序进程就可以请求WMS服务了。

frameworks\\base\\core\\java\\android\\view\\WindowManagerGlobal.java

```java
public static IWindowSession getWindowSession() {
    synchronized (WindowManagerGlobal.class) {
        if (sWindowSession == null) {
            try {
                InputMethodManager imm = InputMethodManager.getInstance();
                IWindowManager windowManager = getWindowManagerService();
                sWindowSession = windowManager.openSession(
                    imm.getClient(),
                    imm.getInputContext()
                );
                float animatorScale = windowManager.getAnimationScale(2);
                ValueAnimator.setDurationScale(animatorScale);
            } catch (RemoteException e) {
                Log.e(TAG, "Failed to open window session", e);
            }
        }
        return sWindowSession;
    }
}

```

函数首先通过getWindowManagerService来获取WMS服务的代理对象，由于WMS服务是一个有名Binder对象，即已经注册到了ServiceManager进程中，因此可以通过查询服务的方式来获取WMS的代理对象。

```java
public static IWindowManager getWindowManagerService() {
    synchronized (WindowManagerGlobal.class) {
        if (sWindowManagerService == null) {
            sWindowManagerService = IWindowManager.Stub.asInterface(
                ServiceManager.getService("window")
            );
        }
        return sWindowManagerService;
    }
}

```

得到了WMS服务的代理对象后，就可以通过该代理对象请求WMS服务创建一个Session Binder本地对象，该对象用于应用程序进程访问WMS服务。关于应用程序进程使用WMS代理对象如何请求WMS创建Session的过程这里不在介绍，这是Binder通信机制中函数远程调用流程。上面通过调用WMS代理对象的openSession函数后，WMS服务的openSession函数实现如下：

frameworks\\base\\services\\java\\com\\android\\server\\wm\\WindowManagerService.java

```java
public IWindowSession openSession(
    IInputMethodClient client,
    IInputContext inputContext
) {
    if (client == null) throw new IllegalArgumentException("null client");
    if (inputContext == null) throw new IllegalArgumentException(
        "null inputContext"
    );
    Session session = new Session(this, client, inputContext);
    return session;
}

```

这里直接构造一个Session对象，并将该对象返回给应用程序进程，这样在应用程序这边就可以得到Session的代理对象IWindowSession.Proxy，并保存在ViewRootImpl对象的成员变量mWindowSession中。

![](https://i-blog.csdnimg.cn/blog_migrate/9082cbe416cacd9f282a60de87c3ad71.jpeg)

### 2.建立WMS服务到应用程序进程之间的连接

前面介绍了应用程序进程到WMS服务之间的连接过程，虽然应用程序进程中的ViewRootImpl对象已经获取WMS服务中的Session的代理对象，也就是说应用程序进程可以请求WMS服务了，但是WMS服务还是不能请求应用程序进程，怎么在应用程序进程和WMS服务之间建立双向连接呢？在前面ViewRootImpl的构造函数中，创建了一个类型为W的对象，W实现了IWindow接口，WMS服务正是通过W的代理对象来请求应用程序进程的。那WMS服务是如何获取W的代理对象的呢？

frameworks\\base\\core\\java\\android\\view\\ViewRootImpl.java

```java
public void setView(View view, WindowManager.LayoutParams attrs, View panelParentView) {
	synchronized (this) {
		if (mView == null) {
			mView = view;
			...
			//mWindow为W本地对象
			res = mWindowSession.addToDisplay(mWindow, mSeq, mWindowAttributes,
					getHostVisibility(), mDisplay.getDisplayId(),
					mAttachInfo.mContentInsets, mInputChannel);
			...
			if (mInputChannel != null) {
				if (mInputQueueCallback != null) {
					mInputQueue = new InputQueue();
					mInputQueueCallback.onInputQueueCreated(mInputQueue);
				}
				mInputEventReceiver = new WindowInputEventReceiver(mInputChannel,
						Looper.myLooper());
			}
			...
		}
	}
}
```

在上一节中已经介绍了，ViewRootImpl对象的成员变量mWindowSession保存了Session的代理对象，向addToDisplay函数传递的第一个参数为成员变量mWindow，而mWindow中保存了W类型的Binder本地对象，因此这里通过函数addToDisplay就可以将W的代理对象传递给WMS服务。

frameworks\\base\\services\\java\\com\\android\\server\\wm\\Session.java

```java
public int addToDisplay(
    IWindow window,
    int seq,
    WindowManager.LayoutParams attrs,
    int viewVisibility,
    int displayId,
    Rect outContentInsets,
    InputChannel outInputChannel
) {
    return mService.addWindow(
        this,
        window,
        seq,
        attrs,
        viewVisibility,
        displayId,
        outContentInsets,
        outInputChannel
    );
}

```

Session接收某一个应用程序进程的函数调用请求，并将请求转交给WMS服务来完成。Session和WMS服务之间的关系如下：

![](https://i-blog.csdnimg.cn/blog_migrate/d322ffb05cef9774ac85a8aa3a1cd3fd.jpeg)

frameworks\\base\\services\\java\\com\\android\\server\\wm\\WindowManagerService.java

```java
public int addWindow(Session session, IWindow client, int seq,
		WindowManager.LayoutParams attrs, int viewVisibility, int displayId,
		Rect outContentInsets, InputChannel outInputChannel) {
		...
		WindowToken token = mTokenMap.get(attrs.token);
		if (token == null) {
			...
			token = new WindowToken(this, attrs.token, -1, false);
			addToken = true;
		//应用程序窗口
		} else if (type >= FIRST_APPLICATION_WINDOW && type <= LAST_APPLICATION_WINDOW) {
			AppWindowToken atoken = token.appWindowToken;
			...
		//输入法窗口
		} else if (type == TYPE_INPUT_METHOD) {
			...
		//墙纸窗口
		} else if (type == TYPE_WALLPAPER) {
			...
		} else if (type == TYPE_DREAM) {
			...
		}
		//在WMS服务中为窗口创建WindowState对象
		win = new WindowState(this, session, client, token,
				attachedWindow, appOp[0], seq, attrs, viewVisibility, displayContent);
		...
		win.setShowToOwnerOnlyLocked(mPolicy.checkShowToOwnerOnly(attrs));
		res = mPolicy.prepareAddWindowLw(win, attrs);
		...
		if (outInputChannel != null && (attrs.inputFeatures
				& WindowManager.LayoutParams.INPUT_FEATURE_NO_INPUT_CHANNEL) == 0) {
			String name = win.makeInputChannelName();
			InputChannel[] inputChannels = InputChannel.openInputChannelPair(name);
			win.setInputChannel(inputChannels[0]);
			inputChannels[1].transferTo(outInputChannel);
			mInputManager.registerInputChannel(win.mInputChannel, win.mInputWindowHandle);
		}
		...
		if (addToken) {
			//mTokenMap哈希表保存了所有的WindowToken对象
			mTokenMap.put(attrs.token, token);
		}
		win.attach();
		//mWindowMap哈希表以键值对的方式保存了所有的WindowState对象及W代理对象：<IWindow.Proxy，WindowState>
		mWindowMap.put(client.asBinder(), win);
		if (win.mAppOp != AppOpsManager.OP_NONE) {
			if (mAppOps.startOpNoThrow(win.mAppOp, win.getOwningUid(), win.getOwningPackage())
					!= AppOpsManager.MODE_ALLOWED) {
				win.setAppOpVisibilityLw(false);
			}
		}
		...
	}
	...
}
```

该函数首先为应用程序进程新增的窗口在WMS服务中创建对应的WindowState对象，并且将WMS，接收应用程序进程的Session，应用程序进程中的W代理对象保存到WindowState中。

```java
WindowState(WindowManagerService service, Session s, IWindow c, WindowToken token,
	   WindowState attachedWindow, int appOp, int seq, WindowManager.LayoutParams a,
	   int viewVisibility, final DisplayContent displayContent) {
	mService = service;//标识WMS服务
	mSession = s;//标识接收应用程序进程的Session本地对象
	mClient = c;//标识应用程序进程中窗口的W代理对象
	mAppOp = appOp;
	mToken = token;//标识应用程序进程中的窗口布局参数的Token
	mOwnerUid = s.mUid;
	mWindowId = new IWindowId.Stub() {
		@Override
		public void registerFocusObserver(IWindowFocusObserver observer) {
			WindowState.this.registerFocusObserver(observer);
		}
		@Override
		public void unregisterFocusObserver(IWindowFocusObserver observer) {
			WindowState.this.unregisterFocusObserver(observer);
		}
		@Override
		public boolean isFocused() {
			return WindowState.this.isFocused();
		}
	};
	...
}
```

在构造WindowState对象过程中，又创建了一个用于标识指定窗口的IWindowId本地对象，因此应用程序进程必定会获取该对象的代理对象。关于IWindowId代理对象的获取过程在接下来分析。在WMS服务中为应用程序进程中的指定窗口创建了对应的WindowState对象后，接着调用该对象的attach()函数：

frameworks\\base\\services\\java\\com\\android\\server\\wm\\WindowState.java

```java
void attach() {
    if (WindowManagerService.localLOGV) Slog.v(
        TAG,
        "Attaching " + this + " token=" + mToken + ", list=" + mToken.windows
    );
    mSession.windowAddedLocked();
}

```

在构造WindowState对象时，将用于接收应用程序进程请求的本地Session对象保存在WindowState的成员变量mSession中，这里调用Session的windowAddedLocked()函数来创建请求SurfaceFlinger的SurfaceSession对象，同时将接收应用程序进程请求的Session保存到WMS服务的mSessions数组中。  
frameworks\\base\\services\\java\\com\\android\\server\\wm\\Session.java

```java
void windowAddedLocked() {
    if (mSurfaceSession == null) {
        mSurfaceSession = new SurfaceSession();
        mService.mSessions.add(this);
    }
    mNumWindow++;
}

```

![](https://i-blog.csdnimg.cn/blog_migrate/5c752430d412ac4d9c3502ba252d687f.jpeg)

### 3. IWindowId代理对象获取过程

在前面构造WindowState对象过程中，创建了一个IWindowId本地对象，并保存在WindowState的成员变量mWindowId中，因此，应用程序进程也会获取用于标识每一个窗口对象的IWindowId的代理对象。

frameworks\\base\\core\\java\\android\\view\\View.java

```java
public WindowId getWindowId() {
	if (mAttachInfo == null) {
		return null;
	}
	if (mAttachInfo.mWindowId == null) {
		try {
			/**获取WMS服务端的IWindowId代理对象，mAttachInfo在前面创建ViewRootImpl对象时创建
			 * mWindow = new W(this);
			 * mAttachInfo = new View.AttachInfo(mWindowSession, mWindow, display, this, mHandler, this);
			 * 在AttachInfo的构造函数中：
			 * mWindow = window;
                         * mWindowToken = window.asBinder();
			 * 因此AttachInfo的成员变量mWindow和mWindowToken引用了同一对象，该对象就是类型为W的本地binder对象
			 * 下面通过W本地binder对象，来获取AMS服务端的IWindowId的代理对象
			 */
			mAttachInfo.mIWindowId = mAttachInfo.mSession.getWindowId(mAttachInfo.mWindowToken);
			WindowId类的定义主要是为了方便在进程间传输IWindowId Binder对象
			mAttachInfo.mWindowId = new WindowId(mAttachInfo.mIWindowId);
		} catch (RemoteException e) {
		}
	}
	return mAttachInfo.mWindowId;
}
```

应用程序进程还是通过Session的代理对象来获取IWindowId的代理对象，参数window在应用程序进程端为W本地binder对象，经过Binder传输到达WMS服务进程后，就转换为W的binder代理对象了。

frameworks\\base\\services\\java\\com\\android\\server\\wm\\Session.java

```java
public IWindowId getWindowId(IBinder window) {
    return mService.getWindowId(window);
}

```

在WMS服务中的Session本地对象又将IWindowId对象的查找过程交给WMS服务。

frameworks\\base\\services\\java\\com\\android\\server\\wm\\WindowManagerService.java

```java
public IWindowId getWindowId(IBinder token) {
    synchronized (mWindowMap) {
        WindowState window = mWindowMap.get(token);
        return window != null ? window.mWindowId : null;
    }
}

```

根据W的binder代理对象token在WMS中查找窗口对应的WindowState对象，再将该窗口在WindowState对象中创建的IWindowId Binder本地对象返回，这样，客户端进程就可以得到该Binder的代理对象。  
![](https://i-blog.csdnimg.cn/blog_migrate/c01bb3cccbecb7dbaceb8010448cd270.jpeg)  
查找过程如下：  
![](https://i-blog.csdnimg.cn/blog_migrate/88c40c68c12f8abf3b6f1d52d69f3604.jpeg)

通过以上介绍，可以知道应用程序进程和WMS服务之间的交互主要是通过IWindowSession，Iwindow，IWindowId三个接口来完成，IWindowSession实现应用程序进程到WMS服务间的通信，而Iwindow和IWindowId则实现WMS服 
 
  
  务到应用程序进程间的通信。

![](https://i-blog.csdnimg.cn/blog_migrate/dbf217d6fbb07dc717edcbb86a322c37.jpeg)