本篇文章的内容如下：

-   1 AIDL简介
-   2 为什么要设置AIDL
-   3 AIDL的注意事项
-   4 AIDL的使用
-   5 源码跟踪
-   6 AIDL的设计给我们的思考
-   7 总结

### 一、AIDL简介

> AIDL是一个缩写，全程是Android Interface Definition Language，也是android接口定义语言。准确的来说，它是用于定义客户端/[服务器](https://cloud.tencent.com/product/cvm/?from_column=20065&from=20065)通信接口的一种描述语言。它其实一种IDL语言，可以拿来生成用于IPC的代码。从某种意义上说它其实是一个模板。为什么这么说？因为在我们使用中，实际起作用的并不是我们写的AIDL代码，而是系统根据它生成的一个IInterface的实例的代码。而如果大家都生成这样的几个实例，然后它们拿来比较，你会发现它们都是有套路的——都是一样的流程，一样的结构，只是根据具体的AIDL文件的不同由细微变动。所以其实AIDL就是为了避免我们一遍遍的写一些前篇一律的代码而出现的一个模板

在详解讲解AIDL之前，大家想一想下面几个问题？

-   1、那么为什么安卓团队要定义这一种语言。
-   2、如果使用AIDL
-   3、AIDL的原理

那我们开始围绕这三个问题开始一次接待

### 二、为什么要设置AIDL

两个维度来看待这个问题：

##### (一) IPC的角度

设计这门语言的目的是为了实现进程间通信，尤其是在涉及多进程并发情况的下的进程间通信IPC。每一个进程都有自己的Dalvik VM实例，都有自己的一块独立的内存，都在自己的内存上存储自己的数据，执行着自己的操作，都在自己的那个空间里操作。每个进程都是独立的，你不知我，我不知你。就像两座小岛之间的桥梁。通过这个桥梁，两个小岛可以进行交流，进行信息的交互。

> 通过AIDL，可以让本地调用远程服务器的接口就像调用本地接口那么接单，让用户无需关注内部细节，只需要实现自己的业务逻辑接口，内部复杂的参数序列化发送、接收、客户端调用服务端的逻辑，你都不需要去关心了。

##### (二)方便角度

> 在Android process 之间不能用通常的方式去访问彼此的内存数据。他们把需要传递的数据解析成基础对象，使得系统能够识别并处理这些对象。因为这个处理过程很难写，所以Android使用AIDL来解决这个问题

### 三、ADIL的注意事项

在定义AIDL之前，请意识到调用这些接口是direct function call。请不要认为call这些接口的行为是发生在另外一个线程里面的。具体的不同因为这个调用是发生在local process还是 remote process而异。

-   发生在local process 里面的调用会跑在这个local process的thread里面。如果这是你的UI主线程，那么AIDL接口的调用也会发生在这个UIthread里面。如果这是发生在另外一个thread，那么调用会发生在service里面。因此，如果仅仅是发生在local process的调用，则你可以完全控制这些调用，当然这样的话，就不需要AIDL了。因为你完全可以使用Bound Service的第一种方式去实现。
-   发生在remote process 里面调用的会跑在你自己的process所维护的thread pool里面。那么你需要注意可能会在同一时刻接收到多个请求。所以AIDL的操作需要做到thread-safe。(每次请求，都交给Service，在线程池里面启动一个thread去执行哪些请求，所以那些方法需要是线程安全的)
-   oneway关键字改变了remote cal的行为。当使用这个关键字时，remote call 不会被阻塞住，它仅仅是发送交互数据后再立即返回。IBinder thread pool之后会把它当做一个通常的remote call。

### 四、AIDL的使用

##### (一)、什么时候使用AIDL

前面我们介绍了，Binder机制，还有后面要讲解的Messager，以及现在说的AIDL等，Android系统中有事先IPC的很多中方式，到底什么情况下应该使用AIDL？Android官方文档给出了答案：

> Note: Using AIDL is necessary only if you allow clients from different applications to access your service for IPC and want to handle multithreading in your service. If you do not need to perform concurrent IPC across different applications, you should create your interface by implementing a Binder or, if you want to perform IPC, but do not need to handle multithreading, implement your interface using a Messenger. Regardless, be sure that you understand Bound Services before implementing an AIDL.

所以使用AIDL只有在你先允许来自不同应用的客户端跨进程通信访问你的Service，并且想要在你的Service处理多线程的时候才是必要的。简单的来说，就是多个客户端，多个线程并发的情况下要使用AIDL。官方文档还之处，如果你的IPC不需要适用多个客户端，就用Binder。如果你想要IPC，但是不需要多线程，那就选择Messager。

##### (二)、建立AIDL

我们以在Android Studio为例进行讲解

###### 1、创建AIDL文件夹

如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/19pizfov1f.png)

创建AIDL文件夹.png

###### 2、创建AIDL文件

如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/nhu0u18u5i.png)

创建AIDL文件.png

文件名为 **"IMyAidlInterface"**

###### 3、编辑和生成AIDL文件

增加一行代码如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/ogokktnd5k.png)

编辑.png

这时候可以点击"同步"按钮，或者rebuild以下项目。然后去看下是否生成了相应的文件

如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/5f1504mxew.png)

生成文件.png

###### 4、编写客户端代码

设置一个单例模式的PushManager，代码如下：

```
public class PushManager {

    private static final String TAG = "GEBILAOLITOU";
    private int id=1;

    //定义为单例模式
    private PushManager() {
    }

    private IMyAidlInterface iMyAidlInterface;

    private static PushManager instance = new PushManager();

    public static PushManager getInstance() {
        return instance;
    }


    public void init(Context context){
        //定义intent
        Intent intent = new Intent(context,PushService.class);
        //绑定服务
        context.bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
    }

    public void connect(){
        try {
            //通过AIDL远程调用
            Log.d(TAG,"pushManager ***************start Remote***************");
            iMyAidlInterface.connect();
        } catch (RemoteException e) {
            e.printStackTrace();
        }

    }


    private ServiceConnection serviceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            //成功连接
            Log.d(TAG,"pushManager ***************成功连接***************");
            iMyAidlInterface = IMyAidlInterface.Stub.asInterface(service);

        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            //断开连接调用
            Log.d(TAG,"pushManager ***************连接已经断开***************");
        }
    };
}
```

还有一个Server

```
public class PushService extends Service{

    private MyServer myServer=new MyServer();


    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return myServer;
    }
}
```

###### 5、编写服务器代码

> 编写服务端实现connect()、sendInMessage()方法

我自己写一个MyServer.java类，代码如下:

```
public class MyServer extends IMyAidlInterface.Stub {

    private static final String TAG = "GEBILAOLITOU";


    @Override
    public void connect() throws RemoteException {
        Log.i(TAG,"connect");
    }

    @Override
    public void connect() throws RemoteException {
        Log.i(TAG,"MyServer connect");
    }

    @Override
    public void sendInMessage(Message message) throws RemoteException {
        Log.i(TAG,"MyServer ** sendInMessage **"+message.toString());
    }
}
```

再写一个Service

```
public class PushService extends Service{

    private MyServer myServer=new MyServer();


    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return myServer;
    }
}
```

###### 6、编写传递的"数据"

> 之前讲解Binder的时候，说过，跨进程调试的时候需要实现Parcelable接口。因为AIDL默认支持的类型保罗Java基本类型（int、long等）和（String、List、Map、CharSequence），如果要传递自定义的类型要实现android.os.Parcelable接口。

假设在两个进程中传递的“数据”一般都是“消息”，我们编写具体的载体，写一个实体类public class Message implements Parcelable。代码如下：

```
public class Message implements Parcelable {

    public long id;  //消息的id
    public String content; //消息的内容
    public long time;  //时间

    @Override
    public String toString() {
        return "Message{" +
                "id=" + id +
                ", content='" + content + '\'' +
                ", time=" + time +
                '}';
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
          dest.writeLong(id);
          dest.writeString(content);
          dest.writeLong(time);
    }

    public Message(Parcel source){
          id=source.readLong();
          content=source.readString();
          time=source.readLong();
    }

    public Message(){

    }

    public void readFromParcel(Parcel in){
        id = in.readLong();
        content = in.readString();
        time=in.readLong();
    }


    public static final Creator<Message> CREATOR=new Creator<Message>() {
        @Override
        public Message createFromParcel(Parcel source) {
            return new Message(source);
        }
        @Override
        public Message[] newArray(int size) {
            return new Message[size];
        }
    };
}
```

修改IMyAidlInterface.aidl，增加一个方法

```
// IMyAidlInterface.aidl
package com.gebilaolitou.android.aidl;

// Declare any non-default types here with import statements
import com.gebilaolitou.android.aidl.Message;

interface IMyAidlInterface {

    //连接
    void connect();

    void sendMessage(Message message);
}
```

###### 7、进行调试

这时候我们rebuild项目，或者同步，会报错

```
Information:Gradle tasks [:app:generateDebugSources, :app:generateDebugAndroidTestSources, :app:mockableAndroidJar, :app:prepareDebugUnitTestDependencies]
/Users/gebilaolitou/Downloads/AIDLDemo/app/src/main/aidl/com/gebilaolitou/android/aidl/IMyAidlInterface.aidl
Error:(5) couldn't find import for class com.gebilaolitou.android.aidl.Message
```

这是因为自定义类型不仅要定义实现android.os.Parcelable接口类，还的为该实现类定义个aidl文件。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/qka3r19nkl.png)

Message.AIDL.png

代码如下：

```
// Message.aidl.aidl
package com.gebilaolitou.android.aidl;

// Declare any non-default types here with import statements
import com.gebilaolitou.android.aidl.Message;

parcelable Message ;
```

> 注意:自定义类型aidl文件名字、路径需要和自定义类名字，路径保持一致。

编译项目或者同步，还是报错

```
Information:Gradle tasks [:app:generateDebugSources, :app:generateDebugAndroidTestSources, :app:mockableAndroidJar, :app:prepareDebugUnitTestDependencies]
Error:Execution failed for task ':app:compileDebugAidl'.
> java.lang.RuntimeException: com.android.ide.common.process.ProcessException: Error while executing process /Users/gebilaolitou/Library/Android/sdk/build-tools/25.0.2/aidl with arguments {-p/Users/gebilaolitou/Library/Android/sdk/platforms/android-25/framework.aidl -o/Users/gebilaolitou/Downloads/AIDLDemo/app/build/generated/source/aidl/debug -I/Users/gebilaolitou/Downloads/AIDLDemo/app/src/main/aidl -I/Users/gebilaolitou/Downloads/AIDLDemo/app/src/debug/aidl -I/Users/gebilaolitou/.android/build-cache/a1586d1e8ebbe7e662fad3571bc225fc0194aa31/output/aidl -I/Users/gebilaolitou/.android/build-cache/d52918cebdc5dd2a94851191b48afd011ca2b5c1/output/aidl -I/Users/gebilaolitou/.android/build-cache/a04d8c3e429534f503f1d5bbe860d0472d27613b/output/aidl -I/Users/gebilaolitou/.android/build-cache/83996cff7d35ba84168d1ed30e788df3aae82edd/output/aidl -I/Users/gebilaolitou/.android/build-cache/59c7b347cfeea50e258b0409d1f3d4eb91f58927/output/aidl -I/Users/gebilaolitou/.android/build-cache/7127c78b885aaac60b2b0712034851136a252f90/output/aidl -I/Users/gebilaolitou/.android/build-cache/d501962f472162456967742c491558ed0737c27c/output/aidl -I/Users/guochenli/.android/build-cache/13936966f3097ecab148b88871eeb79b0a9fe984/output/aidl -I/Users/gebilaolitou/.android/build-cache/fb883931c2e88ee11d0e77773aa01a2e67652940/output/aidl -I/Users/gebilaolitou/.android/build-cache/a0568698ab34df5d8fa827b197c443b9e1747f8e/output/aidl -d/var/folders/7d/z3snv_1n33xbf7l4vr2956fh0000gn/T/aidl2942334890792150517.d /Users/gebilaolitou/Downloads/AIDLDemo/app/src/main/aidl/com/gebilaolitou/android/aidl/IMyAidlInterface.aidl}
```

貌似什么看不出来，这时候我们看下Gradle Console，会发现

```
Executing tasks: [:app:generateDebugSources, :app:generateDebugAndroidTestSources, :app:mockableAndroidJar, :app:prepareDebugUnitTestDependencies]

Configuration on demand is an incubating feature.
Incremental java compilation is an incubating feature.
:app:preBuild UP-TO-DATE
:app:preDebugBuild UP-TO-DATE
:app:checkDebugManifest
:app:preReleaseBuild UP-TO-DATE
:app:prepareComAndroidSupportAnimatedVectorDrawable2531Library
:app:prepareComAndroidSupportAppcompatV72531Library
:app:prepareComAndroidSupportConstraintConstraintLayout102Library
:app:prepareComAndroidSupportSupportCompat2531Library
:app:prepareComAndroidSupportSupportCoreUi2531Library
:app:prepareComAndroidSupportSupportCoreUtils2531Library
:app:prepareComAndroidSupportSupportFragment2531Library
:app:prepareComAndroidSupportSupportMediaCompat2531Library
:app:prepareComAndroidSupportSupportV42531Library
:app:prepareComAndroidSupportSupportVectorDrawable2531Library
:app:prepareDebugDependencies
:app:compileDebugAidl
aidl E 19367 21417307 type_namespace.cpp:130] In file /Users/guochenli/Downloads/AIDLDemo/app/src/main/aidl/com/gebilaolitou/android/aidl/IMyAidlInterface.aidl line 12 parameter message (argument 1):
aidl E 19367 21417307 type_namespace.cpp:130]     'Message' can be an out type, so you must declare it as in, out or inout.
```

看重点

> aidl E 19367 21417307 type\_namespace.cpp:130\] In file /Users/gebilaolitou/Downloads/AIDLDemo/app/src/main/aidl/com/gebilaolitou/android/aidl/IMyAidlInterface.aidl line 12 parameter message (argument 1): **aidl E 19367 21417307 type\_namespace.cpp:130\] 'Message' can be an out type, so you must declare it as in, out or inout.**

这是因为AIDL不是Java。它是真的很接近，但是它不是Java。Java参数是没有方向的概念，AIDL参数是有方向，参数可以从客户端传到服务端，再返回来。

> 所以如果sendMessage()方法的message参数是纯粹的输入参，这意味着是从客户端到服务器的数据，你需要在ADIL声明：

```
void sendMessage(in Message message);
```

> 如果sendMessage方法的message参数是纯粹的输出，这意味它的数据是通过服务器到客户端的，使用：

```
void sendMessage(out Message message);
```

> 如果sendMessage方法的message参数是输入也是输出，客户端的值在服务器可能会被修改，使用

```
void sendMessage(inout Message message);
```

> 所以，最终我们修改IMyAidlInterface.aidl文件如下

```
// IMyAidlInterface.aidl
package com.gebilaolitou.android.aidl;

// Declare any non-default types here with import statements
import com.gebilaolitou.android.aidl.Message;

interface IMyAidlInterface {
    //连接
    void connect();
    //发送消息  客户端——> 服务器
    void sendInMessage(in Message message);
}
```

这时候重新编译就可以了

###### 8、补充完善

设计一个Activity，如下

```
public class MainActivity extends AppCompatActivity {

    private Button   btn,btnSend;
    private EditText et;
    private boolean isConnected=false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        btn=(Button)this.findViewById(R.id.btn);
        et=(EditText)this.findViewById(R.id.et);
        btnSend=(Button)this.findViewById(R.id.send);
        btn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                PushManager.getInstance().connect();
                isConnected=true;
            }
        });
        btnSend.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(!isConnected){
                    Toast.makeText(MainActivity.this,"请连接",Toast.LENGTH_LONG).show();
                }
                if(et.getText().toString().trim().length()==0){
                    Toast.makeText(MainActivity.this,"请输入",Toast.LENGTH_LONG).show();
                }
                PushManager.getInstance().sendString(et.getText().toString());
            }
        });
    }
}
```

对应的xml如下

```
<?xml version="1.0" encoding="utf-8"?>
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:orientation="vertical"
    >

    <Button
        android:id="@+id/btn"
        android:layout_width="368dp"
        android:layout_height="50dp"
        android:text="连接"
        />

    <LinearLayout
        android:layout_width="match_parent"
        android:layout_height="match_parent"
        android:orientation="horizontal">
        <EditText
            android:layout_weight="1"
            android:id="@+id/et"
            android:layout_width="wrap_content"
            android:layout_height="50dp"
            />
        <Button
            android:id="@+id/send"
            android:text="发送"
            android:layout_width="50dp"
            android:layout_height="50dp" />
    </LinearLayout>

</LinearLayout>
```

别忘记manifest文件

```
    <application
        android:allowBackup="true"
        android:icon="@mipmap/ic_launcher"
        android:label="@string/app_name"
        android:roundIcon="@mipmap/ic_launcher_round"
        android:name=".App"
        android:supportsRtl="true"
        android:theme="@style/AppTheme">
        <activity android:name=".MainActivity">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />

                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
        <service
            android:name=".PushService"
            android:enabled="true"
            android:process=":push"
            android:exported="true" />
    </application>
```

###### 9、效果显示

###### (1) 没有任何操作下显示如下

无操作情况下，**主进程**log输出如下

```
08-14 15:57:57.491 9575-9575/com.gebilaolitou.android.aidl I/GEBILAOLITOU: PushManager ***************成功连接***************
```

###### (2) 先点击**"连接"**按钮，**主进程**log输出如下

```
08-14 16:00:20.394 9575-9575/com.gebilaolitou.android.aidl I/GEBILAOLITOU: PushManager ***************start Remote***************
```

这时候看下**push进程**log输出如下

```
08-14 16:00:20.394 9605-9627/com.gebilaolitou.android.aidl:push I/GEBILAOLITOU: MyServer connect
```

###### (3) 在**输入框** 输入**"123456"**，然后点击**"发送"**按钮,**主进程**log输出如下

```
08-14 16:02:23.042 9575-9575/com.gebilaolitou.android.aidl I/GEBILAOLITOU: PushManager ***************sendString***************Message{id=2, content='123456', time=1502697743041}
```

这时候看下**push进程**log输出如下

```
08-14 16:02:23.045 9605-9625/com.gebilaolitou.android.aidl:push I/GEBILAOLITOU: MyServer ** sendInMessage **Message{id=2, content='123456', time=1502697743041}
```

至此我们成功把一个Message对象通过AIDL传递到另外一个进程中。

### 五、源码跟踪

通过上面的内容，我们已经学会了AIDL的全部用法，接下来让我们透过现象看本质，研究一下究竟AIDL是如何帮助我们进行跨进程通信的。

> 我们上文已经提到，在写完AIDL文件后，编译器会帮我们自动生成一个同名的.java文件，大家已经发现了，在我们实际编写客户端和服务端代码的过程中，真正协助我们工作的其实就是这个文件，而.aidl文件从头到尾都没有出现过。大家会问：我们为什么要写这个.aidl文件。其实我们写这个.aidl文件就是为了生成这个对应的.java文件。事实上，就算我们不写AIDL文件，直接按照它生成的.java文件这样写一个.java文件出来。在服务端和客户端也可以照常使用这个.java类进行跨进程通信。所以说AIDL语言只是在简化我们写这个.java文件而已，而要研究AIDL是符合帮助我们进行跨境进程通信的，其实就是研究这个生成的.java文件是如何工作的

##### (一) .java文件位置

位置如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/359joy0q8s.png)

位置.png

它的完整路径是：app->build->generated->source->aidl->debug->com->gebilaolitou->android->aidl->IMyAidlInterface.java（其中 com.gebilaolitou.android.aidl

是包名，相对应的AIDL文件为 IMyAidlInterface.aidl ）。在AndroidStudio里面目录组织方式由默认的 android改为 Project 就可以直接按照文件夹结构访问到它。

##### (二) IMyAidlInterface .java类分析

IMyAidlInterface .java 里面代码如下：

```
/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Original file: /Users/gebilaolitou/Downloads/AIDLDemo/app/src/main/aidl/com/gebilaolitou/android/aidl/IMyAidlInterface.aidl
 */
package com.gebilaolitou.android.aidl;

public interface IMyAidlInterface extends android.os.IInterface {
    /**
     * Local-side IPC implementation stub class.
     */
    public static abstract class Stub extends android.os.Binder implements com.gebilaolitou.android.aidl.IMyAidlInterface {
        private static final java.lang.String DESCRIPTOR = "com.gebilaolitou.android.aidl.IMyAidlInterface";

        /**
         * Construct the stub at attach it to the interface.
         */
        public Stub() {
            this.attachInterface(this, DESCRIPTOR);
        }

        /**
         * Cast an IBinder object into an com.gebilaolitou.android.aidl.IMyAidlInterface interface,
         * generating a proxy if needed.
         */
        public static com.gebilaolitou.android.aidl.IMyAidlInterface asInterface(android.os.IBinder obj) {
            if ((obj == null)) {
                return null;
            }
            android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
            if (((iin != null) && (iin instanceof com.gebilaolitou.android.aidl.IMyAidlInterface))) {
                return ((com.gebilaolitou.android.aidl.IMyAidlInterface) iin);
            }
            return new com.gebilaolitou.android.aidl.IMyAidlInterface.Stub.Proxy(obj);
        }

        @Override
        public android.os.IBinder asBinder() {
            return this;
        }

        @Override
        public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException {
            switch (code) {
                case INTERFACE_TRANSACTION: {
                    reply.writeString(DESCRIPTOR);
                    return true;
                }
                case TRANSACTION_connect: {
                    data.enforceInterface(DESCRIPTOR);
                    this.connect();
                    reply.writeNoException();
                    return true;
                }
                case TRANSACTION_sendInMessage: {
                    data.enforceInterface(DESCRIPTOR);
                    com.gebilaolitou.android.aidl.Message _arg0;
                    if ((0 != data.readInt())) {
                        _arg0 = com.gebilaolitou.android.aidl.Message.CREATOR.createFromParcel(data);
                    } else {
                        _arg0 = null;
                    }
                    this.sendInMessage(_arg0);
                    reply.writeNoException();
                    return true;
                }
            }
            return super.onTransact(code, data, reply, flags);
        }

        private static class Proxy implements com.gebilaolitou.android.aidl.IMyAidlInterface {
            private android.os.IBinder mRemote;

            Proxy(android.os.IBinder remote) {
                mRemote = remote;
            }

            @Override
            public android.os.IBinder asBinder() {
                return mRemote;
            }

            public java.lang.String getInterfaceDescriptor() {
                return DESCRIPTOR;
            }
//连接

            @Override
            public void connect() throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    mRemote.transact(Stub.TRANSACTION_connect, _data, _reply, 0);
                    _reply.readException();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
            }
//发送消息  客户端——> 服务器

            @Override
            public void sendInMessage(com.gebilaolitou.android.aidl.Message message) throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    if ((message != null)) {
                        _data.writeInt(1);
                        message.writeToParcel(_data, 0);
                    } else {
                        _data.writeInt(0);
                    }
                    mRemote.transact(Stub.TRANSACTION_sendInMessage, _data, _reply, 0);
                    _reply.readException();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
            }
        }

        static final int TRANSACTION_connect = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
        static final int TRANSACTION_sendInMessage = (android.os.IBinder.FIRST_CALL_TRANSACTION + 1);
    }
//连接

    public void connect() throws android.os.RemoteException;
//发送消息  客户端——> 服务器

    public void sendInMessage(com.gebilaolitou.android.aidl.Message message) throws android.os.RemoteException;
}
```

> 们可以看到编译的后**IMyAidlInterface.java**文件是一个接口，继承自**android.os.IInterface**，仔细观察IMyAidlInterface接口我们可以发现IMyAidlInterface内部代码主要分成两部分，一个是**抽象类Stub** 和 **原来aidl声明的connect()和sendInMessage()方法**

如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/y8uc6ic8am.png)

IMyAidlInterface结构.png

> 重点在于**Stub类**，下面我们来分析一下。从**Stub类**中我们可以看到是继承自Binder，并且实现了IMyAidlInterface接口。如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/vb7hd89n84.png)

Stub类.png

Stub类基本结构如下：

-   静态方法 asInterface(android.os.IBinder obj)
-   静态内部类 Proxy
-   方法 onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags)
-   方法 asBinder()
-   private的String类型常量DESCRIPTOR
-   private的int类型常量TRANSACTION\_connect
-   private的int类型常量TRANSACTION\_sendInMessage

那我们就依次来分析下，我们先从 asInterface(android.os.IBinder obj) 方法入手

###### 1、静态方法 asInterface(android.os.IBinder obj)

```
        /**
         * Cast an IBinder object into an com.gebilaolitou.android.aidl.IMyAidlInterface interface,
         * generating a proxy if needed.
         */
        public static com.gebilaolitou.android.aidl.IMyAidlInterface asInterface(android.os.IBinder obj) {
            //非空判断
            if ((obj == null)) {
                return null;
            }
            // DESCRIPTOR是常量为"com.gebilaolitou.android.aidl.IMyAidlInterface"
            // queryLocalInterface是Binder的方法，搜索本地是否有可用的对象
            android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
            //如果有，则强制类型转换并返回
            if (((iin != null) && (iin instanceof com.gebilaolitou.android.aidl.IMyAidlInterface))) {
                return ((com.gebilaolitou.android.aidl.IMyAidlInterface) iin);
            }
            //如果没有，则构造一个IMyAidlInterface.Stub.Proxy对象
            return new com.gebilaolitou.android.aidl.IMyAidlInterface.Stub.Proxy(obj);
        }
```

> 上面的代码可以看到，主要的作用就是根据传入的Binder对象转换成客户端需要的IMyAidlInterface接口。通过之前学过的Binder内容，我们知道：如果客户端和服务端处于同一进程，那么queryLocalInterface()方法返回就是服务端Stub对象本身；如果是跨进程，则返回一个封装过的Stub.Proxy，也是一个代理类，在这个代理中实现跨进程通信。那么让我们来看下Stub.Proxy类

###### 2、onTransact()方法解析

onTransact()方法是根据code参数来处理，这里面会调用真正的业务实现类

> 在onTransact()方法中，根据传入的code值回去执行服务端相应的方法。其中常量TRANSACTION\_connect和常量TRANSACTION\_sendInMessage就是code值(在AIDL文件中声明了多少个方法就有多少个对应的code)。其中data就是服务端方法需要的的参数，执行完，最后把方法的返回结果放入reply中传递给客户端。如果该方法返回false，那么客户端请求失败。

###### 3、静态类Stub.Proxy

代码如下：

```
private static class Proxy implements com.gebilaolitou.android.aidl.IMyAidlInterface {
            private android.os.IBinder mRemote;

            Proxy(android.os.IBinder remote) {
                mRemote = remote;
            }
            //Proxy的asBinder()返回位于本地接口的远程代理
            @Override
            public android.os.IBinder asBinder() {
                return mRemote;
            }

            public java.lang.String getInterfaceDescriptor() {
                return DESCRIPTOR;
            }
//连接
            
            @Override
            public void connect() throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    mRemote.transact(Stub.TRANSACTION_connect, _data, _reply, 0);
                    _reply.readException();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
            }
//发送消息  客户端——> 服务器

            @Override
            public void sendInMessage(com.gebilaolitou.android.aidl.Message message) throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    if ((message != null)) {
                        _data.writeInt(1);
                        message.writeToParcel(_data, 0);
                    } else {
                        _data.writeInt(0);
                    }
                    mRemote.transact(Stub.TRANSACTION_sendInMessage, _data, _reply, 0);
                    _reply.readException();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
            }
        }
```

通过上面的代码，我们知道了几个重点

-   1、Proxy 实现了 com.gebilaolitou.android.aidl.IMyAidlInterfac接口，所以他内部有IMyAidlInterface接口的两个抽象方法
-   2、Proxy的asBinder()方法返回的mRemote，而这个mRemote是什么时候被赋值的？是在构造函数里面被赋值的。

###### 3.1、静态类Stub.Proxy的connect()方法和sendInMessage()方法

###### 3.1.1connect()方法解析

-   1、 通过阅读静态类Stub.Proxy的connect()方法，我们容易分析出来里面的两个android.os.Parcel**\_data**和**\_reply**是用来进行跨进程传输的"载体"。而且通过字面的意思，很容易猜到，**\_data**用来存储 **客户端流向服务端** 的数据，**\_reply**用来存储 **服务端流向客户端** 的数据。
-   2、通过mRemote. transact()方法，将**\_data**和**\_reply**传过去
-   3、通过\_reply.readException()来读取服务端执行方法的结果。
-   4、最后通过finally回收l**\_data**和**\_reply**

###### 3.1.2的相关参数

关于Parcel，这部分我已经在前面，讲解过了，如果有不明白的，请看[Android跨进程通信IPC之4——AndroidIPC基础1](https://cloud.tencent.com/developer/article/1199093?from_column=20421&from=20421)中的第四部分**Parcel类详解**。

关于 transact()方法：这是客户端和和服务端通信的核心方法，也是IMyAidlInterface.Stub继承android.os.Binder而重写的一个方法。调起这个方法之后，客户端将会挂起当前线程，等候服务端执行完相关任务后，通知并接收返回的**\_reply**数据流。关于这个方法的传参，有注意两点

-   1 方法ID：transact()方法第一个参数是一个方法ID，这个是客户端和服务端约定好的给方法的编码，彼此一一对应。在AIDL文件转话为.java时候，系统会自动给AIDL里面的每一个方法自动分配一个方法ID。而这个ID就是咱们说的常量**TRANSACTION\_connect**和**TRANSACTION\_sendInMessage**这些常量生成了递增的ID,是根据你在aidl文件的方法顺序而来，然后在IMyAidlInterface.Stub中的onTransact()方法里面switch根据第一个参数code即我们说的ID而来。
-   2最后的一个参数：transact()方法最后一个参数是一个int值，代表是单向的还是双向的。具体大家请参考我们前面的文章[Android跨进程通信IPC之5——Binder的三大接口](https://cloud.tencent.com/developer/article/1199097?from_column=20421&from=20421)中关于IBinder部分。我这里直接说结论：0表示双向流通，即**\_reply**可以正常的携带数据回来。如果为1的话，那么数据只能单向流程，从服务端回来的数据**\_reply**不携带任何数据。注意：AIDL生成的.java文件，这个参数均为0

sendInMessage()方法和connect()方法大同小异，这里就不详细说明了。

###### 3.2 AIDL中关于定向tag的理解

定向tag是AIDL语法的一部分，而in，out，intout，是三个定向的tag

###### 3.2.1 android官方文档中关于AIDL中定向tag的介绍

[https://developer.android.com/guide/components/aidl.html](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fdeveloper.android.com%2Fguide%2Fcomponents%2Faidl.html&objectId=1199109&objectType=1&isNewArticle=undefined)中**1\. Create the .aidl file** 里面，原文内容截图了一部分如下:

> All non-primitive parameters require a directional tag indicating which way the data goes . Either in , out , or inout . Primitives are in by default , and connot be otherwise .

翻译过来就是：**所有非基本参数都需要一个定向tag来指出数据流通的方向，不管是in，out，inout。基本参数的定向tag默认是并且只能是in**

所以按照上面翻译的理解，in和out代表客户端与服务端两条单向的数据流向，而inout则表示两端可双向流通数据的。

###### 3.2.2 总结

> AIDL中的定向tag表示在跨进程通信中数据 流向，其中in表示数据只能由客户端流向服务端，out表示数据只能由服务端流行客户端，而inout则表示数据可在服务端与客户端之间双向流通。其中，数据流向是针对客户端中的那个传入方法的对象而言。in为定向tag的话，表现为服务端将会接受到一个那个对象的完整数据，但是客户端的那个对象不会因为服务端传参修改而发生变动；out的话表现为服务端将会接收到那个对象的参数为空的对象，但是在服务端对接收到的空对象有任何修改之后客户端将会同步变动；inout为定向tag的情况下，服务端将会接收到客户端传来对象的完整信息，并且客户端将会同步服务对该对象的任何变动。

###### 4.asBinder()方法

该方法就是返回当前的Binder方法

##### (三) IMyAidlInterface .java流程分析

通过阅读上面源码，我们来看下AIDL的流程，以上面的例子为例，那么先从客户端开始

###### 1.客户端流程

###### 1、获取IMyAidlInterface对象

```
    public void init(Context context){
        //定义intent
        Intent intent = new Intent(context,PushService.class);
        //绑定服务
        context.bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
    }

    private ServiceConnection serviceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            //成功连接
            Log.i(TAG,"PushManager ***************成功连接***************");
            iMyAidlInterface = IMyAidlInterface.Stub.asInterface(service);

        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            //断开连接调用
            Log.i(TAG,"PushManager ***************连接已经断开***************");
        }
    };
```

客户端中通过Intent去绑定一个服务端的Service。在\*\* onServiceConnected(ComponentName name, IBinder service)\*\*方法中通过返回service可以得到AIDL接口的实例。这是调用了asInterface(android.os.IBinder) 方法完成的。

###### 1.1、asInterface(android.os.IBinder)方法

```
        /**
         * Cast an IBinder object into an com.gebilaolitou.android.aidl.IMyAidlInterface interface,
         * generating a proxy if needed.
         */
        public static com.gebilaolitou.android.aidl.IMyAidlInterface asInterface(android.os.IBinder obj) {
            if ((obj == null)) {
                return null;
            }
            android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
            if (((iin != null) && (iin instanceof com.gebilaolitou.android.aidl.IMyAidlInterface))) {
                return ((com.gebilaolitou.android.aidl.IMyAidlInterface) iin);
            }
            return new com.gebilaolitou.android.aidl.IMyAidlInterface.Stub.Proxy(obj);
        }
```

在asInterface(android.os.IBinder)我们知道是调用的new com.gebilaolitou.android.aidl.IMyAidlInterface.Stub.Proxy(obj)构造的一个Proxy对象。

所以可以这么说在PushManager中的变量IMyAidlInterface其实是一个IMyAidlInterface.Stub.Proxy对象。

###### 2、调用connect()方法

上文我们说过了PushManager类中的iMyAidlInterface其实IMyAidlInterface.Stub.Proxy对象，所以调用connect()方法其实是IMyAidlInterface.Stub.Proxy的connect()方法。代码如下：

```
            @Override
            public void connect() throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    mRemote.transact(Stub.TRANSACTION_connect, _data, _reply, 0);
                    _reply.readException();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
            }
```

-   1、这里面主要是生成了\_data和\_reply数据流，并向\_data中存入客户端的数据。
-   2、通过 transact()方法将他们传递给服务端，并请求服务指定的方法
-   3、接收\_reply数据，并且从中读取服务端传回的数据。

通过上面客户端的所有行为，我们会发现，其实通过ServiceConnection类中onServiceConnected(ComponentName name, IBinder service)中第二个参数service很重要，因为我们最后是滴啊用它的transact() 方法，将客户端的数据和请求发送给服务端去。从这个角度来看，这个service就像是服务端在客户端的代理一样，而IMyAidlInterface.Stub.Proxy对象更像一个二级代理，我们在外部通过调用这个二级代理来间接调用service这个一级代理

###### 2服务端流程

在前面几篇文章中我们知道Binder传输中，客户端调用transact()对应的是服务端的onTransact()函数，我们在IMyAidlInterface.java中看到

###### 1、获取IMyAidlInterface.Stub的transact()方法

```
        @Override
        public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException {
            switch (code) {
                case INTERFACE_TRANSACTION: {
                    reply.writeString(DESCRIPTOR);
                    return true;
                }
                case TRANSACTION_connect: {
                    data.enforceInterface(DESCRIPTOR);
                    this.connect();
                    reply.writeNoException();
                    return true;
                }
                case TRANSACTION_sendInMessage: {
                    data.enforceInterface(DESCRIPTOR);
                    com.gebilaolitou.android.aidl.Message _arg0;
                    if ((0 != data.readInt())) {
                        _arg0 = com.gebilaolitou.android.aidl.Message.CREATOR.createFromParcel(data);
                    } else {
                        _arg0 = null;
                    }
                    this.sendInMessage(_arg0);
                    reply.writeNoException();
                    return true;
                }
            }
            return super.onTransact(code, data, reply, flags);
        }
```

可以看到，他在收到客户端的 transact()方法后，直接调用了switch选择，根据ID执行不同操作，因为我们知道是调用的connect()方法，所以对应的code是TRANSACTION\_connect，所以我们下**case TRANSACTION\_connect:**的内容，如下：

```
                case TRANSACTION_connect: {
                    data.enforceInterface(DESCRIPTOR);
                    this.connect();
                    reply.writeNoException();
                    return true;
                }
```

这里面十分简单了，就是直接调用服务端的connect()方法。

整体流程如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/4b5ecw83wn.png)

AIDL流程.png

### 六、AIDL的设计给我们的思考

通过研究我们知道AIDL是基于Binder实现的跨进程通信，但是为什么要设计成AIDL，这么麻烦？

> 在程序的设计领域，任何的解决方案，无非都是需求和性能两方面的综合考量。性能又包括可维护性和可拓展性等。

我们知道android已经有了Binder实现跨进程通信，但是里面涉及很多Service在启动的时候在ServiceManager中进行注册，但是在application层面做到这一步基本是无法实现的。所以很有以必要研究一套application层的IPC通信机制。因为android已经提供了Binder机制，如果能重复利用Binder机制岂不是更好，所以就有了现在的AIDL。android是操作系统，要利于开发者去方便操作，所以就应该是设计出一套模板。这样就很方便了。

> 由于是跨进程通信，所以我们就需要有一种途径去访问它们，在这时候，**代理—桩**的设计理念就初步成型了。为了达到我们的目的，我们可以在客户端建立一个服务端的代理，在服务端建立一个客户端的桩，这样一来，客户端有什么需求可以直接和代理通信，代理说你等下，代理就把信息发送给桩，桩把信息给服务端进行处理，处理结束后，服务器告诉桩，桩告诉代理，代理告诉客户端。这样一来，客户端以为代理就是服务端，并且事实是它也只是和代理进行交互，服务端也是也是如此。

类似的跨进程通信机制，我知道还有一个是[Hermes](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2FXiaofei-it%2FHermes&objectId=1199109&objectType=1&isNewArticle=undefined)，大家有空可以去了解下。

### 七、总结

> AIDL是Android IPC机制中很重要的一部分，AIDL主要是通过Binder来实现进程通信的，其实另一种IPC方式的Message底层也是通过AIDL来实现的。所以AIDL的重要性就不言而喻了。后面我将讲解Message.

为了让大家更好的理解AIDL，我下面补上了三张图，分别是类图、流程图和原理图。

设计的类图如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/ee4afsgqns.png)

类图.png

![](https://ask.qcloudimg.com/http-save/yehe-2957818/15kebnlbty.png)

AIDL流程图.png

![](https://ask.qcloudimg.com/http-save/yehe-2957818/wqf51dkp83.png)

原理图.png

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2017.08.22 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除