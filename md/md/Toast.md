# Toast



#### 源码

```
    public static Toast makeText(Context context, CharSequence text, @Duration int duration) {
        // 获取Toast对象
        Toast result = new Toast(context);
        LayoutInflater inflate = (LayoutInflater)
            context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);    
        // 填充布局
        View v = inflate.inflate(com.android.internal.R.layout.transient_notification, null);
        TextView tv = (TextView)v.findViewById(com.android.internal.R.id.message);
        tv.setText(text);
        // 设置View和duration属性
        result.mNextView = v;
        result.mDuration = duration;
        return result;
    }
```



```
    public void show() {
        if (mNextView == null) {
            throw new RuntimeException("setView must have been called");
        }
        INotificationManager service = getService();
        String pkg = mContext.getOpPackageName();
        TN tn = mTN;
        tn.mNextView = mNextView;
        try {
            service.enqueueToast(pkg, tn, mDuration);
        } catch (RemoteException e) {
            // Empty
        }
    }
```



这里有三个问题。

1. **通过`getService()`怎么就获得一个`INotificationManager`对象？**
2. **`TN`类是个什么鬼**？
3. **方法最后只有一个`service.enqueueToast()`，显示和隐藏在哪里？**



#### 1

```
    static private INotificationManager getService() {
        if (sService != null) {
            return sService;
        }
        sService = INotificationManager
        	.Stub.asInterface(ServiceManager.getService("notification"));
        return sService;
    }
```

对`Binder`机制了解的同学看见`XXX.Stub.asInterface`肯定会很熟悉，这不就是`AIDL`中获取`client`嘛！确实是这样。

tips: 本着追本溯源的精神，先看下ServiceManager.getService("notification")。在上上上上篇博客[SystemServer启动流程源码解析](https://blog.csdn.net/qq_17250009/article/details/52143652)中startOtherServices()涉及到NotificationManagerService的启动，

```
mSystemServiceManager.startService(NotificationManagerService.class);
```

> 辅助服务
>
> 辅助功能（AccessibilityService）其实是一个Android系统提供给的一种服务，本身是继承Service类的。这个服务提供了增强的用户界面，旨在帮助残障人士或者可能暂时无法与设备充分交互的人们。
>
>   从开发者的角度看，其实就是提供两种功能：查找界面元素，实现模拟点击。实现一个辅助功能服务要求继承AccessibilityService类并实现它的抽象方法。自定义一个服务类AccessibilitySampleService（这个命名可以随意），继承系统的AccessibilityService并覆写onAccessibilityEvent和onInterrupt方法。编写好服务类之后，在系统配置文件（AndroidManifest.xml）中注册服务。完成前面两个步骤就完成了基本发辅助功能服务注册与配置，具体的功能实现需要在onAccessibilityEvent中完成，根据onAccessibilityEvent回调方法传递过来的AccessibilityEvent对象可以对事件进行过滤，结合AccessibilitySampleService本身提供的查找节点与模拟点击相关的接口即可实现权限节点的查找与点击。



> Toast中AIDL对应文件的位置。
>
> 源码位置：frameworks/base/core/java/android/app/INotificationManager.aidl
>
> Server端：NotificationManagerService.java
> 源码位置：frameworks/base/services/core/java/com/android/server/notification/NotificationManagerService.java

可以理解为：经过进程间通信（`AIDL`方式），最后调用`NotificationManagerService#enqueueToast()`。可以看下[这篇](http://blog.csdn.net/qq_17250009/article/details/49720795)



#### 2 TN

```
    public Toast(Context context) {
        mContext = context;
        mTN = new TN();
        mTN.mY = context.getResources().getDimensionPixelSize(
                com.android.internal.R.dimen.toast_y_offset);
        mTN.mGravity = context.getResources().getInteger(
                com.android.internal.R.integer.config_toastDefaultGravity);
    }
```

源码位置：frameworks/base/core/java/android/widght/Toast$TN.java

    private static class TN extends ITransientNotification.Stub {
        ...
        TN() {
            final WindowManager.LayoutParams params = mParams;
            params.height = WindowManager.LayoutParams.WRAP_CONTENT;
            params.width = WindowManager.LayoutParams.WRAP_CONTENT;
            ...
        }
        ...
    }

`TN`类除了设置参数的作用之外，更大的作用是`Toast`显示与隐藏的回调。`TN`类在这里作为`Server`端。`NotificationManagerService$NotificationListeners`类作为`client`端









------



##### 自定义Toast

```
    Toast customToast = new Toast(MainActivity.this.getApplicationContext());
    View customView = LayoutInflater.from(MainActivity.this).inflate(R.layout.custom_toast,null);
    ImageView img = (ImageView) customView.findViewById(R.id.img);
    TextView tv = (TextView) customView.findViewById(R.id.tv);
    img.setBackgroundResource(R.drawable.daima);
    tv.setText("沉迷学习，日渐消瘦");
    customToast.setView(customView);
    customToast.setDuration(Toast.LENGTH_SHORT);
    customToast.setGravity(Gravity.CENTER,0,0);
    customToast.show();
```



##### 单例实现，解决多次点击不显示问题

```
public class SingleToast {

    private static Toast mToast;

    /**双重锁定，使用同一个Toast实例*/
    public static Toast getInstance(Context context){
        if (mToast == null){
            synchronized (SingleToast.class){
                if (mToast == null){
                    mToast = new Toast(context);
                }
            }
        }
        return mToast;
    }
}
```

#### 3 show()

frameworks/base/services/core/java/com/android/server/notification/NotificationManagerService.java

    private final IBinder mService = new INotificationManager.Stub() {
    
        @Override
        public void enqueueToast(String pkg, ITransientNotification callback, int duration)
        {
            if (pkg == null || callback == null) {
                Slog.e(TAG, "Not doing toast. pkg=" + pkg + " callback=" + callback);
                return ;
            }
            final boolean isSystemToast = isCallerSystem() || ("android".equals(pkg));
            ...
            synchronized (mToastQueue) {
                int callingPid = Binder.getCallingPid();
                long callingId = Binder.clearCallingIdentity();
                try {
                    ToastRecord record;
                    int index = indexOfToastLocked(pkg, callback);
                    if (index >= 0) {
                        record = mToastQueue.get(index);
                        record.update(duration);
                    } else {
                        if (!isSystemToast) {
                            int count = 0;
                            final int N = mToastQueue.size();
                            for (int i=0; i<N; i++) {
                                 final ToastRecord r = mToastQueue.get(i);
                                 if (r.pkg.equals(pkg)) {
                                     count++;
                                     if (count >= MAX_PACKAGE_NOTIFICATIONS) {
                                         Slog.e(TAG, "Package has already posted " + count
                                                + " toasts. Not showing more. Package=" + pkg);
                                         return;
                                     }
                                 }
                            }
                        }
    
                        record = new ToastRecord(callingPid, pkg, callback, duration);
                        mToastQueue.add(record);
                        index = mToastQueue.size() - 1;
                        // 将Toast所在的进程设置为前台进程
                        keepProcessAliveLocked(callingPid);
                    }
                    if (index == 0) {
                        showNextToastLocked();
                    }
                } finally {
                    Binder.restoreCallingIdentity(callingId);
                }
            }
        }
        ...
    }

在Toast#show()最终会进入到这个方法。首先通过indexOfToastLocked()方法获取应用程序对应的ToastRecord在mToastQueue中的位置，Toast消失后返回-1，否则返回对应的位置。mToastQueue明明是个ArratList对象，却命名Queue，猜测后面会遵循“后进先出”的原则移除对应的ToastRecord对象～。这里先以返回index=-1查看，也就是进入到else分支。如果不是系统程序，也就是应用程序。那么同一个应用程序瞬时在mToastQueue中存在的消息不能超过50条（Toast对象不能超过50个）。否则直接return。这也是上文中为什么快速点击50次之后无法继续显示的原因。既然瞬时Toast不能超过50个，那么运用单例模式使用同一个Toast对象不就可以了嘛？答案是：可行。消息用完了就移除，瞬时存在50个以上的Toast对象相信在正常的程序中也用不上。而且注释中也说这样做是为了放置DOS攻击和防止泄露。其实从这里也可以看出：为了防止内存泄露，创建Toast最好使用getApplicationContext，不建议使用Activity、Service等。

回归主题。接下来创建了一个ToastRecord对象并添加进mToastQueue。接下来调用showNextToastLocked()方法显示一个Toast。

**显示Toast**

```
    void showNextToastLocked() {
        ToastRecord record = mToastQueue.get(0);
        while (record != null) {
            if (DBG) Slog.d(TAG, "Show pkg=" + record.pkg + " callback=" + record.callback);
            try {
                record.callback.show();
                scheduleTimeoutLocked(record);
                return;
            } catch (RemoteException e) {
                int index = mToastQueue.indexOf(record);
                if (index >= 0) {
                    mToastQueue.remove(index);
                }
                keepProcessAliveLocked(record.pid);
                if (mToastQueue.size() > 0) {
                    record = mToastQueue.get(0);
                } else {
                    record = null;
                }
            }
        }
    }
```

这里首先调用`record.callback.show()`，这里的`record.callback`其实就是`TN`类。接下来调用`scheduleTimeoutLocked()`方法，我们知道`Toast`显示一段时间后会自己消失，所以这个方法肯定是定时让`Toast`消失

```
    private void scheduleTimeoutLocked(ToastRecord r)
    {
        mHandler.removeCallbacksAndMessages(r);
        Message m = Message.obtain(mHandler, MESSAGE_TIMEOUT, r);
        long delay = r.duration == Toast.LENGTH_LONG ? LONG_DELAY : SHORT_DELAY;
        mHandler.sendMessageDelayed(m, delay);
    }  
```

果然如此。重点在于使用mHandler.sendMessageDelayed(m, delay)延迟发送消息。这里的delay只有两种值，要么等于LENGTH_LONG，其余统统的等于SHORT_DELAY，setDuration为其他值用正常手段是没有用的（可以反射，不在重点范围内）。

handler收到MESSAGE_TIMEOUT消息后会调用handleTimeout((ToastRecord)msg.obj)。

```
    private void handleTimeout(ToastRecord record)
    {
        if (DBG) Slog.d(TAG, "Timeout pkg=" + record.pkg + " callback=" + record.callback);
        synchronized (mToastQueue) {
            int index = indexOfToastLocked(record.pkg, record.callback);
            if (index >= 0) {
                cancelToastLocked(index);
            }
        }
    }
```

```
    void cancelToastLocked(int index) {
        ToastRecord record = mToastQueue.get(index);
        try {
            record.callback.hide();
        } catch (RemoteException e) {
            ...
        }
        mToastQueue.remove(index);
        keepProcessAliveLocked(record.pid);
        if (mToastQueue.size() > 0) {
            showNextToastLocked();
        }
    }
```

延时调用record.callback.hide() 隐藏Toast.前文也提到record.callback就是TN对象。

frameworks/base/core/java/android/widght/ToastTN.javaToastTN.javaToastTN#show()

        final Handler mHandler = new Handler(); 
    
        @Override
        public void show() {
            if (localLOGV) Log.v(TAG, "SHOW: " + this);
            mHandler.post(mShow);
        }
    
        final Runnable mShow = new Runnable() {
            @Override
            public void run() {
                handleShow();
            }
        };

**注意下这里直接使用new Handler获取Handler对象，这也是为什么在子线程中不用Looper弹出Toast会出错的原因。跟进handleShow()。**

        public void handleShow() {
            if (mView != mNextView) {
                // remove the old view if necessary
                handleHide();
                mView = mNextView;
                Context context = mView.getContext().getApplicationContext();
                String packageName = mView.getContext().getOpPackageName();
                if (context == null) {
                    context = mView.getContext();
                }
                mWM = (WindowManager)context.getSystemService(Context.WINDOW_SERVICE);
                ...
                mParams.packageName = packageName;
                if (mView.getParent() != null) {
                    mWM.removeView(mView);
                }
                mWM.addView(mView, mParams);
                trySendAccessibilityEvent();
            }
        }

原来`addView`到`WindowManager`。这样就完成了`Toast`的显示。至于隐藏就更简单了。

```
        public void handleHide() {
            if (localLOGV) Log.v(TAG, "HANDLE HIDE: " + this + " mView=" + mView);
            if (mView != null) {
                // note: checking parent() just to make sure the view has
                // been added...  i have seen cases where we get here when
                // the view isn't yet added, so let's try not to crash.
                if (mView.getParent() != null) {
                    if (localLOGV) Log.v(TAG, "REMOVE! " + mView + " in " + this);
                    mWM.removeView(mView);
                }

                mView = null;
            }
        }
```





