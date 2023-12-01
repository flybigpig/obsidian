# InputManagerService



IMS由system_server启动

```java
private void startOtherServices() {
    //创建InputManagerService
    inputManager = new InputManagerService(context);
    //创建WindowManagerService 传入IMS 进行绑定
    wm = WindowManagerService.main(context, inputManager, !mFirstBoot, mOnlyCore,
                    new PhoneWindowManager(), mActivityManagerService.mActivityTaskManager);
    //添加WindowManagerService
   ServiceManager.addService(Context.WINDOW_SERVICE, wm, /* allowIsolated= */ false,
                    DUMP_FLAG_PRIORITY_CRITICAL | DUMP_FLAG_PROTO);
   //添加InputManagerService
   ServiceManager.addService(Context.INPUT_SERVICE, inputManager,
                    /* allowIsolated= */ false, DUMP_FLAG_PRIORITY_CRITICAL);
   //设置WindowManagerCallbacks
   inputManager.setWindowManagerCallbacks(wm.getInputManagerCallback());
   //开启InputManagerService
   inputManager.start();
}

```





```
public InputManagerService(Context context) {
        this.mContext = context;
        //创建handler
        this.mHandler = new InputManagerHandler(DisplayThread.get().getLooper());
        //调用nativeInit创建Native层的InputManagerService 传入了MessageQueue
        mPtr = nativeInit(this, mContext, mHandler.getLooper().getQueue());
        //添加到本地的服务集合中
        LocalServices.addService(InputManagerInternal.class, new LocalService());
    }

```

