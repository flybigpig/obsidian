

于 2022-09-26 00:33:32 首次发布

版权声明：本文为博主原创文章，遵循 [CC 4.0 BY-SA](http://creativecommons.org/licenses/by-sa/4.0/) 版权协议，转载请附上原文出处链接和本声明。

## Android 12 添加系统服务

#### 1\. 添加服务AIDL文件

模仿其他的系统服务，也放在app包下。

```java
// IDemoManager.aidl

package android.app;

// Declare any non-default types here with import statements

interface IDemoManager {
    int getValue();
}
```

#### 2\. 定义服务的标签

```java
//Context.java

public static final String DEMO_SERVICE = "demo";
```

#### 3\. 新建服务管理类

```java
// DemoManager.java

package android.app;

import android.annotation.SystemService;

import android.content.Context;
import android.compat.annotation.UnsupportedAppUsage;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;
import android.util.Singleton;

@SystemService(Context.DEMO_SERVICE)
public class DemoManager {
    private static String TAG = "DemoManager";

    /**
     * @hide
     */
    @UnsupportedAppUsage
    public static IDemoManager getService() {
        return IDemoManagerSingleton.get();
    }

    @UnsupportedAppUsage
    private static final Singleton<IDemoManager> IDemoManagerSingleton =
            new Singleton<IDemoManager>() {
                @Override
                protected IDemoManager create() {
                    final IBinder b = ServiceManager.getService(Context.DEMO_SERVICE);
                    final IDemoManager am = IDemoManager.Stub.asInterface(b);
                    return am;
                }
            };

    public int getValue() throws RemoteException {
        return getService().getValue();
    }

    /**
     * @hide
     */
    public DemoManager() {
    }
}
```

#### 4\. 新建服务实现端

```java
// DemoManagerService.java

package com.android.server;

import android.app.IDemoManager;
import android.content.Context;
import android.os.RemoteException;
import android.util.Log;

import com.android.server.SystemService;

import androidx.annotation.NonNull;

public class DemoManagerService extends IDemoManager.Stub{
    private static String TAG = "DemoManagerService";

    @Override
    public int getValue() throws RemoteException {
        Log.d(TAG, "return value to client.");
        return 1024;
    }

    public static final class Lifecycle extends SystemService {
        private final DemoManagerService mService;

        public Lifecycle(Context context) {
            super(context);
            mService = new DemoManagerService();
            Log.d(TAG, "Lifecycle create");
        }

        @Override
        public void onStart() {
            publishBinderService(Context.DEMO_SERVICE, mService);
            mService.start();
        }

        @Override
        public void onBootPhase(int phase) {

        }

        public DemoManagerService getService() {
            return mService;
        }
    }

    private void start() {
        //todo
    }
}
```

继承IDemoManager.Stub，实现接口；内部静态类 Lifecycle 继承了 SystemService,则可由系统管理它的生命周期(启动、添加到SericeManager等);

#### 5\. 注册服务

向servicemanager注册服务，SystemServer进程会缓存该进程所有系统服务的实例。

```java
// SystemServiceRegistry.java

@SystemApi
public final class SystemServiceRegistry {
    ...
    static {
      ...
       registerService(Context.DEMO_SERVICE, DemoManager.class,
                new CachedServiceFetcher<DemoManager>() {
            @Override
            public DemoManager createService(ContextImpl ctx) {
                return new DemoManager();
            }});
            ...
```

#### 6\. 启动服务

在startOtherServices()方法中，启动服务

```java
//SystemServer.java

private void startOtherServices(@NonNull TimingsTraceAndSlog t) {
        ...
        t.traceBegin("StartDemoManagerService");
        try {
            mSystemServiceManager.startService(DemoManagerService.Lifecycle.class);
        } catch (Throwable e) {
            reportWtf("starting DemoManagerService", e);
        }
        t.traceEnd();
```

#### 7\. 解决编译报错

##### 7.1 更新系统API

```java
make update-api
```

后续补充，主要是Lint注解类的问题解决, 有可能会有api list version的问题。

#### 8\. SELinux权限添加

后续补充  
涉及：file\_contexts、service\_contexts、service.te、system\_server.te等

参考：https://www.jianshu.com/p/74971ee85a8b