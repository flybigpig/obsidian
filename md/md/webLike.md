android 8.0 往上 Toast不再弹出



查看Toast源码后，发现Toast是要通过INotificationManager类来实现，而当通知权限禁用后，调用此类会返回异常

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
    // 权限禁用后走这里，这里是空方法，所以会发生既不crash又无响应的情况
  }
}
```

 解决办法是 通过反射方式暴力绕过

```
public class ToastUtils {
    private static Object iNotificationManagerObj;

    /**
     * @param context
     * @param message
     */
    public static void show(Context context, String message) {
        show(context.getApplicationContext(), message, Toast.LENGTH_SHORT);
    }

    /**
     * @param context
     * @param message
     */
    public static void show(Context context, String message, int duration) {
        if (TextUtils.isEmpty(message)) {
            return;
        }
        //后setText 兼容小米默认会显示app名称的问题
        Toast toast = Toast.makeText(context, null, duration);
        toast.setText(message);
        if (isNotificationEnabled(context)) {
            toast.show();
        } else {
            showSystemToast(toast);
        }
    }

    /**
     * 显示系统Toast
     */
    private static void showSystemToast(Toast toast) {
        try {
            Method getServiceMethod = Toast.class.getDeclaredMethod("getService");
            getServiceMethod.setAccessible(true);
            //hook INotificationManager
            if (iNotificationManagerObj == null) {
                iNotificationManagerObj = getServiceMethod.invoke(null);

                Class iNotificationManagerCls = Class.forName("android.app.INotificationManager");
                Object iNotificationManagerProxy = Proxy.newProxyInstance(toast.getClass().getClassLoader(), new Class[]{iNotificationManagerCls}, new InvocationHandler() {
                    @Override
                    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
                        //强制使用系统Toast
                        if ("enqueueToast".equals(method.getName())
                                || "enqueueToastEx".equals(method.getName())) {  //华为p20 pro上为enqueueToastEx
                            args[0] = "android";
                        }
                        return method.invoke(iNotificationManagerObj, args);
                    }
                });
                Field sServiceFiled = Toast.class.getDeclaredField("sService");
                sServiceFiled.setAccessible(true);
                sServiceFiled.set(null, iNotificationManagerProxy);
            }
            toast.show();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    /**
     * 消息通知是否开启
     *
     * @return
     */
    private static boolean isNotificationEnabled(Context context) {
        NotificationManagerCompat notificationManagerCompat = NotificationManagerCompat.from(context);
        boolean areNotificationsEnabled = notificationManagerCompat.areNotificationsEnabled();
        return areNotificationsEnabled;
    }
}
```





##### 内容相同的Toast短时间不能重复弹出。

```
Toast mToast;

public void showToast(String text) {
  if (mToast == null) {
    mToast = Toast.makeText(MainActivity.this, text, Toast.LENGTH_SHORT);
  } else {
    mToast.setText(text);
    mToast.setDuration(Toast.LENGTH_SHORT);
  }
  mToast.show();
}
```



