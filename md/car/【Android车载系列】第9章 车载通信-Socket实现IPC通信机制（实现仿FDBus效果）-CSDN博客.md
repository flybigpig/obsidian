## 1 FDBus简介

  FDBus 基于 Socket (TCP 和 Unix domain) 之上的[IPC](https://so.csdn.net/so/search?q=IPC&spm=1001.2101.3001.7020)机制, 采用 Google protobuf 做序列化和反序列化。 FDBus还支持字符串形式的名字作为server地址。通过 name server 自动为 server 分配Unix domain 地址和 TCP 端口号, 实现 client 和server 之间用服务名字寻址。**一句话描述：FDBus (Fast Distributed Bus) 是一种 IPC 机制, 用于进程间通信。**

> 特点：  
> **分布式**：基于TCP socket和Unix Domain socket（UDS），既可用于本地IPC，也支持网络主机之间的IPC；  
> **跨平台**：目前已在Windows，Linux和QNX上验证；  
> **高性能**：点对点直接通信，不通过中央Hub或Broker转发；  
> **安全性**：能够为server的方法调用也事件广播配置不同等级的访问权限，只有权限足够高的client才能特点方法和接收特定事件；  
> **服务名解析**：server地址用名字标识，通过name server注册服务和解析名字，从而server可以在全网络任意部署。

## 2 车载Android设备间通讯

  我们知道车载Android设备不止一台，如果我们要在中控和仪表盘这两个Android系统设备之间要进行通信怎么实现呢？同学们思考一下  
  不同Android设备间的通信已经无法通过Binder、Broadcast、Service来完成，所以我们是否可以参考FDBus通过Socket来进行不同Android设备的通讯呢？下面我们来看看大致思路：

**通过`Socket+动态代理`实现简单的`跨设备通讯`**:

> 1.YFDBus库中分为IPCNameManager类：实现客户端Socket的管理，动态代理的相关的逻辑；NameServerManager类：服务端的Socket管理。  
> 2.Server模块为服务端，依赖YFDBus库，初始化时**服务注册**到IPCNameManager中，提供自定义的服务。接收到客户端Socket发送过来的消息后，将该消息解析做出响应返回对应的数据给客户端。  
> 3.Client模块为客户端，依赖YFDBus库，通过Socket实现对Server端的**服务发现**，确实是数据传输后使用动态代理解析来自Server的响应数据，得到的服务对象。然后通过该对象进行**服务调用**，确实就是Socket发送数据到Server做处理然后给出响应，来实现服务的发现和调用。

**最简单的理解就是：  
  1.通过Socket客户端向服务端发送消息，实现`服务注册`、`服务发现`、`服务调用`。  
  2.通过动态代理的方式获取到服务端的类及方法在客户端进行调用。**

## 3 代码实现

### 3.1 思路图

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/44c36bdf1c2160076c63d842547f57f0.png)

### 3.2 核心代码

#### 3.2.1 项目结构

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/75511f745d131ec0d343233dd6db7332.png)

下面我们来看看关键的代码：

#### 3.2.2 Client客户端模块

客户端INameServer.java，同服务端的`INameServer`目的是两边保持一致，在对应模块直接调用。

```
package com.yvan.nameserver.interfaces;

import com.yvan.yfdbus.annotion.ClassId;

/**
 * @author yvan
 * @date 2023/4/22
 * @description 客户端的服务类（对应服务端com.yvan.nameserver.NameServer类）
 */
@ClassId("com.yvan.nameserver.NameServer")
public interface INameServer {
    /**
     * 速度，能源 信息获取
     *
     * @param userId
     * @param token
     * @return
     */
    SpeedState getSpeed(String userId, String token);
}
```

客户端SpeedState.java类，是`传输的数据类`

```
package com.yvan.nameserver.interfaces;

/**
 * @author yvan
 * @date 2023/4/22
 * @description 客户端速度，能源容量信息类（对应服务端SpeedState类）
 */
public class SpeedState {

    int speed;
    int energy;

    public SpeedState() {
    }

    public SpeedState(int speed, int energy) {
        this.speed = speed;
        this.energy = energy;
    }

    public int getSpeed() {
        return speed;
    }

    public void setSpeed(int speed) {
        this.speed = speed;
    }

    public int getEnergy() {
        return energy;
    }

    public void setEnergy(int energy) {
        this.energy = energy;
    }
}

```

客户端页面展示ClientActivity.java，主要有：`连接服务端`、`服务发现`、`服务调用`

```
package com.yvan.client;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;
import android.view.View;

import com.yvan.clent.R;
import com.yvan.nameserver.interfaces.INameServer;
import com.yvan.nameserver.interfaces.SpeedState;
import com.yvan.yfdbus.IPCNameManager;
import com.yvan.yfdbus.NameConfig;

/**
 * @author yvan
 * @date 2023/4/22
 * @description 客户端页面
 */
public class ClientActivity extends AppCompatActivity {

    private static final String TAG = ClientActivity.class.getSimpleName();
    private INameServer iNameServer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        IPCNameManager.getInstance().init(this);
    }

    /**
     * 连接服务端
     *
     * @param view
     */
    public void connect(View view) {
        IPCNameManager.getInstance().connect(NameConfig.IP);
    }

    /**
     * 发现服务
     *
     * @param view
     */
    public void findServer(View view) {
        iNameServer = IPCNameManager.getInstance().getInstance(INameServer.class, "小明的车", "token666");
        Log.i(TAG, "findServer: 服务发现  发现远端设备的服务  ");
    }

    /**
     * 服务调用
     *
     * @param view
     */
    public void invekeServer(View view) {
        Log.i(TAG, "invekeServer: 服务调用 " + iNameServer);
        new Thread() {
            @Override
            public void run() {
                SpeedState speedState = iNameServer.getSpeed("小明的车", "token666");
                Log.i(TAG, "invekeServer: 结果远端设备的速度: " + speedState.getSpeed()
                        + " 远端设备的电池容量: " + speedState.getEnergy());
            }
        }.start();
    }
}
```

#### 3.2.3 Server服务端模块

INameServer.java、SpeedState.java同客户端，这里不重复贴出来了。  
服务端NameServer.java，主要是对`INameServer的实现类`。

```
package com.yvan.nameserver;

import android.util.Log;

import com.yvan.nameserver.interfaces.INameServer;
import com.yvan.nameserver.interfaces.SpeedState;

/**
 * @author yvan
 * @date 2023/4/22
 * @description 服务端的服务实现类
 */
public class NameServer implements INameServer {

    private final static String TAG = NameServer.class.getSimpleName();
    private static NameServer sInstance = null;

    public static synchronized NameServer getInstance(String userId, String token) {
        Log.i(TAG, "getInstance:userId  " + userId + " token  " + token);
        if (sInstance == null) {
            sInstance = new NameServer();
        }
        return sInstance;
    }

    @Override
    public SpeedState getSpeed(String userId, String token) {
        return new SpeedState(180, 55);
    }
}

```

服务端页面展示ServerActivity.java，用于`管理服务的启动`、`服务注册`

```
package com.yvan;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;

import com.yvan.nameserver.NameServer;
import com.yvan.nameserver.R;
import com.yvan.yfdbus.IPCNameManager;
import com.yvan.yfdbus.NameServerManager;

/**
 * @author yvan
 * @date 2023/4/22
 * @description 服务端页面
 */
public class ServerActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        // 管理服务的启动
        Intent intent = new Intent(this, NameServerManager.class);
        startService(intent);
        // 服务注册
        IPCNameManager.getInstance().register(NameServer.class);
    }
}
```

#### 3.2.4 YFDBus模块

重点类IPCNameManager.java用于IPC通讯，服务注册、服务发现、服务调用的入口，管理着服务及动态代理。

```
package com.yvan.yfdbus;

import android.content.Context;
import android.text.TextUtils;
import android.util.Log;

import com.google.gson.Gson;
import com.yvan.yfdbus.annotion.ClassId;
import com.yvan.yfdbus.request.RequestBean;
import com.yvan.yfdbus.request.RequestParamter;

import org.java_websocket.client.WebSocketClient;
import org.java_websocket.handshake.ServerHandshake;

import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.net.URI;
import java.nio.ByteBuffer;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * @author yvan
 * @date 2023/4/22
 * @description IPC通讯管理类
 */
public class IPCNameManager {

    private static final String TAG = IPCNameManager.class.getSimpleName();
    private NameCenter nameCenter = new NameCenter();
    private static final Gson GSON = new Gson();
    private static final IPCNameManager ourInstance = new IPCNameManager();
    private Context sContext;

    public static IPCNameManager getInstance() {
        return ourInstance;
    }

    public NameCenter getNameCenter() {
        return nameCenter;
    }

    public final Lock lock = new ReentrantLock();
    private String responce = null;

    /**
     * 初始化（客户端）
     *
     * @param context
     */
    public void init(Context context) {
        sContext = context.getApplicationContext();
    }

    /**
     * 服务注册（服务端）
     *
     * @param clazz
     */
    public void register(Class<?> clazz) {
        nameCenter.register(clazz);
    }

    /**
     * WebSocket（客户端给服务端发消息用）
     */
    MyWebSocketClient myWebSocketClient;

    public void connect(String ip) {
        new Thread() {
            @Override
            public void run() {
                connectSocketServer(ip);
            }
        }.start();
    }

    /**
     * 服务发现（客户端）
     *
     * @param clazz
     * @param parameters
     * @param <T>
     * @return
     */
    public <T> T getInstance(Class<T> clazz, Object... parameters) {
        // 实例化  服务发现
        sendRequest(clazz, null, parameters, NameServerManager.TYPE_GET);
        return getProxy(clazz);
    }

    /**
     * 向服务端发送消息（客户端）
     * @param clazz
     * @param method
     * @param parameters
     * @param type
     * @param <T>
     * @return
     */
    public <T> String sendRequest(Class<T> clazz, Method method, Object[] parameters, int type) {
        // socket  协议
        RequestParamter[] requestParamters = null;
        if (parameters != null && parameters.length > 0) {
            requestParamters = new RequestParamter[parameters.length];
            for (int i = 0; i < parameters.length; i++) {
                Object parameter = parameters[i];
                String parameterClassName = parameter.getClass().getName();
                String parameterValue = GSON.toJson(parameter);
                RequestParamter requestParamter = new RequestParamter(parameterClassName, parameterValue);
                requestParamters[i] = requestParamter;
            }
        }
        String className = clazz.getAnnotation(ClassId.class).value();
        String methodName = method == null ? "getInstance" : method.getName();
        RequestBean requestBean = new RequestBean(type, className, methodName, requestParamters);
        String request = GSON.toJson(requestBean);

        synchronized (lock) {
            try {
                // 客户端给服务端发消息
                myWebSocketClient.send(request.getBytes());
                Log.i(TAG, "sendRequest: 锁住线程:" + Thread.currentThread().getName());
                lock.wait();
                Log.i(TAG, "sendRequest: 锁住成功");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        Log.i(TAG, "sendRequest: 唤醒线程");
        if (!TextUtils.isEmpty(responce)) {
            String data1 = responce;
            responce = null;
            return data1;
        }
        return null;
    }

    /**
     * 动态代理
     * @param clazz
     * @param <T>
     * @return
     */
    private <T> T getProxy(Class<T> clazz) {
        ClassLoader classLoader = sContext.getClassLoader();
        return (T) Proxy.newProxyInstance(classLoader, new Class<?>[]{clazz},
                new NameServerInvokeHandler(clazz));
    }

    /**
     * 连接服务端
     * @param ip
     */
    private void connectSocketServer(String ip) {
        try {
            URI url = new URI(ip + NameConfig.IP_PORT);
            myWebSocketClient = new MyWebSocketClient(url);
            myWebSocketClient.connect();
            Log.i(TAG, "connect: 连接 ");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    /**
     * 客户端Socket
     */
    class MyWebSocketClient extends WebSocketClient {
        public MyWebSocketClient(URI serverUri) {
            super(serverUri);
            Log.i(TAG, "MyWebSocketClient create serverUri:" + serverUri);
        }

        @Override
        public void onOpen(ServerHandshake handshakedata) {
            Log.i(TAG, "onOpen: ");
        }

        @Override
        public void onMessage(String message) {
            Log.i(TAG, "onMessage: " + message);
        }

        public void onMessage(ByteBuffer message) {
            byte[] buf = new byte[message.remaining()];
            message.get(buf);
            String data = new String(buf);
            Log.i(TAG, "客户端收到信息 onMessage: " + data);
            responce = data;
            synchronized (lock) {
                try {
                    lock.notify();
                } catch (Exception e) {
                    Log.e(TAG, "onMessage: 锁异常: " + e.getMessage());
                }
            }
        }

        @Override
        public void onClose(int code, String reason, boolean remote) {
            Log.i(TAG, "onClose: code: " + code + ", reason: " + reason + ", remote: " + remote);
        }

        @Override
        public void onError(Exception ex) {
            Log.e(TAG, "onError: " + ex.getMessage());
        }
    }
}

```

动态代理类NameServerInvokeHandler.java

```
package com.yvan.yfdbus;

import android.text.TextUtils;
import android.util.Log;

import com.google.gson.Gson;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;

/**
 * @author yvan
 * @date 2023/4/22
 * @description 服务代理方法类
 */
public class NameServerInvokeHandler implements InvocationHandler {
    private static final String TAG = NameServerInvokeHandler.class.getSimpleName();
    private static final Gson GSON = new Gson();
    private Class clazz;

    public NameServerInvokeHandler(Class clazz) {
        this.clazz = clazz;
    }

    @Override
    public Object invoke(Object o, Method method, Object[] objects) throws Throwable {
        if (method.getName().contains("toString")) {
            return "proxy";
        }
        Log.i(TAG, "invoke: " + method.getName());
        // 发送请求
        String data = IPCNameManager.getInstance().sendRequest(clazz, method, objects, NameServerManager.TYPE_INVOKE);
        Log.i(TAG, "invoke: data " + data);
        if (!TextUtils.isEmpty(data)) {
            Object object = GSON.fromJson(data, method.getReturnType());
            return object;
        }
        return null;
    }
}

```

服务管理类NameServerManager.java，用于`处理服务发现`和`处理服务调用`给客户端响应

```
package com.yvan.yfdbus;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import androidx.annotation.Nullable;

import com.google.gson.Gson;
import com.yvan.yfdbus.request.RequestBean;
import com.yvan.yfdbus.request.RequestParamter;

import org.java_websocket.WebSocket;
import org.java_websocket.handshake.ClientHandshake;
import org.java_websocket.server.WebSocketServer;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;

/**
 * @author yvan
 * @date 2023/4/22
 * @description 服务端管理类
 */
public class NameServerManager extends Service {

    private static final String TAG = NameServerManager.class.getSimpleName();

    private final Gson gson = new Gson();

    //服务发现
    public static final int TYPE_GET = 1;
    //服务调用
    public static final int TYPE_INVOKE = 2;

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "onCreate: 开始 ");
        new Thread(new TcpServer()).start();
    }

    private class TcpServer implements Runnable {
        @Override
        public void run() {
            // 服务端开启SocketServer，等待客户端的请求
            SocketServer socketServer = new SocketServer();
            socketServer.start();
        }
    }

    class SocketServer extends WebSocketServer {

        public SocketServer() {
            super(new InetSocketAddress(NameConfig.IP_PORT));
        }

        @Override
        public void onOpen(WebSocket conn, ClientHandshake handshake) {
            Log.i(TAG, "onOpen: " + conn.getRemoteSocketAddress().getAddress().getHostAddress());
        }

        @Override
        public void onClose(WebSocket conn, int code, String reason, boolean remote) {
            Log.i(TAG, "onClose: " + conn.getRemoteSocketAddress().getAddress().getHostAddress()
                    + ", code:" + code
                    + ", reason:" + reason
                    + ", remote:" + remote);
        }

        @Override
        public void onMessage(WebSocket conn, String message) {
        }

        @Override
        public void onMessage(WebSocket conn, ByteBuffer message) {
            Log.i(TAG, "onMessage: " + conn.getRemoteSocketAddress().getAddress().getHostAddress());
            byte[] buf = new byte[message.remaining()];
            message.get(buf);
            String request = new String(buf);
            Log.i(TAG, "onMessage 接收到客户端消息: " + request);
            String responce = dealRequest(request);
            Log.i(TAG, "onMessage 向客户端发送数据: " + responce);
            // 服务端 向 客户端 发送响应
            conn.send(responce.getBytes());
        }

        @Override
        public void onError(WebSocket conn, Exception ex) {
            Log.e(TAG, "onError: " + conn.getRemoteSocketAddress().getAddress().getHostAddress()
                    + ", ex:" + ex);
        }

        @Override
        public void onStart() {
            Log.i(TAG, "onStart: ");
        }
    }

    /**
     * 服务端处理客户端发送过来的消息，返回响应
     *
     * @param request
     * @return
     */
    String dealRequest(String request) {
        RequestBean requestBean = gson.fromJson(request, RequestBean.class);
        int type = requestBean.getType();
        //服务发现， 服务初始化
        switch (type) {
            case TYPE_INVOKE:
                Object object = IPCNameManager.getInstance().getNameCenter().getObject(requestBean.getClassName());
                Method tempMethod = IPCNameManager.getInstance().getNameCenter().getMethod(requestBean);
                Object[] mParameters = makeParameterObject(requestBean);
                try {
                    Object result = tempMethod.invoke(object, mParameters);
                    String data = gson.toJson(result);
                    return data;
                } catch (IllegalAccessException e) {
                    e.printStackTrace();
                } catch (InvocationTargetException e) {
                    e.printStackTrace();
                }
                break;
            case TYPE_GET:
                Method method = IPCNameManager.getInstance().getNameCenter().getMethod(requestBean);
                Object[] parameters = makeParameterObject(requestBean);
                try {
                    Object object1 = method.invoke(null, parameters);
                    IPCNameManager.getInstance().getNameCenter().putObject(requestBean.getClassName(), object1);
                } catch (IllegalAccessException e) {
                    e.printStackTrace();
                } catch (InvocationTargetException e) {
                    e.printStackTrace();
                }
                return "success";
        }
        return null;
    }

    /**
     * 获取方法参数的值
     *
     * @param requestBean
     * @return
     */
    private Object[] makeParameterObject(RequestBean requestBean) {
        Object[] mParameters = null;
        RequestParamter[] requestParamters = requestBean.getRequestParamters();
        if (requestParamters != null && requestParamters.length > 0) {
            mParameters = new Object[requestBean.getRequestParamters().length];
            for (int i = 0; i < requestParamters.length; i++) {
                RequestParamter requestParamter = requestParamters[i];
                Class<?> clazz = IPCNameManager.getInstance().getNameCenter().getClassType(requestParamter.getParameterClassName());
                mParameters[i] = gson.fromJson(requestParamter.getParameterValue(), clazz);
            }

        } else {
            mParameters = new Object[0];
        }
        return mParameters;
    }

}

```

NameCenter.java类个方法管理中心

```
package com.yvan.yfdbus;

import com.yvan.yfdbus.request.RequestBean;
import com.yvan.yfdbus.request.RequestParamter;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;

/**
 * @author yvan
 * @date 2023/4/22
 * @description 类和方法管理中心
 */
public class NameCenter {

    /**
     * 类集合
     */
    private ConcurrentHashMap<String, Class<?>> mClassMap;

    /**
     * 方法集合
     */
    private ConcurrentHashMap<String, ConcurrentHashMap<String, Method>> mAllMethodMap;

    /**
     * 方法参数值
     */
    private final ConcurrentHashMap<String, Object> mInstanceObjectMap;

    public NameCenter() {
        mClassMap = new ConcurrentHashMap<String, Class<?>>();
        mAllMethodMap = new ConcurrentHashMap<String, ConcurrentHashMap<String, Method>>();
        mInstanceObjectMap = new ConcurrentHashMap<String, Object>();
    }

    public void register(Class clazz) {
        mClassMap.put(clazz.getName(), clazz);
        Method[] methods = clazz.getDeclaredMethods();
        for (Method method : methods) {
            ConcurrentHashMap<String, Method> map = mAllMethodMap.get(clazz.getName());
            if (map == null) {
                map = new ConcurrentHashMap<String, Method>();
                mAllMethodMap.put(clazz.getName(), map);
            }
            // java重载   方法名+参数
            String key = getMethodParameters(method);
            map.put(key, method);
        }
    }

    public void putObject(String className, Object instance) {
        mInstanceObjectMap.put(className, instance);
    }

    public Object getObject(String className) {
        return mInstanceObjectMap.get(className);
    }

    public Method getMethod(RequestBean requestBean) {
        ConcurrentHashMap<String, Method> map = mAllMethodMap.get(requestBean.getClassName());
        if (map != null) {
            String key = getMethodParameters(requestBean);
            return map.get(key);
        }
        return null;
    }

    public Class<?> getClassType(String parameterClassName) {
        try {
            Class clazz = Class.forName(parameterClassName);
            return clazz;
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        }
        return null;
    }

    /**
     * 拼装方法（方法名-参数类型）
     *
     * @param requestBean
     * @return
     */
    public static String getMethodParameters(RequestBean requestBean) {
        List<String> parameterClassName = new ArrayList<>();
        for (RequestParamter c : requestBean.getRequestParamters()) {
            parameterClassName.add(c.getParameterClassName());
        }
        return getMethodParameters(requestBean.getMethodName(), parameterClassName);
    }

    /**
     * 拼装方法（方法名-参数类型）
     *
     * @param method
     * @return
     */
    public static String getMethodParameters(Method method) {
        List<String> parameterClassName = new ArrayList<>();
        for (Class<?> c : method.getParameterTypes()) {
            parameterClassName.add(c.getName());
        }
        return getMethodParameters(method.getName(), parameterClassName);
    }

    /**
     * 拼装方法（方法名-参数类型）
     *
     * @param methodName
     * @param parameterClassName
     * @return
     */
    public static String getMethodParameters(String methodName, List<String> parameterClassName) {
        // 方法签名
        StringBuilder result = new StringBuilder();
        result.append(methodName);
        int size = parameterClassName.size();
        if (size == 0) {
            return result.toString();
        }
        for (int i = 0; i < size; ++i) {
            result.append("-").append(parameterClassName.get(i));
        }
        return result.toString();
    }

}
```

### 3.3 项目地址

[Github项目地址，记得点个Star喔](https://github.com/zhuyingfa/NameServer)

## 4 总结

项目实现目的：`跨设备通信`  
项目实现方案：`Socket实现IPC通讯`、`动态代理实现类方法的调用`  
如果对您有帮助，请点赞收藏加关注，别忘了Github上给个Star喔。