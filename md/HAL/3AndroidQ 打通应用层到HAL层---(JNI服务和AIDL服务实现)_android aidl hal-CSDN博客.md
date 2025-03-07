前面两篇文章实现了自定义HAL和HIDL服务，本篇接着往上层实现，这篇文章要写的是JNI服务和framework层AIDL服务实现，由AIDL服务调用JNI层的服务的函数，为了提供给上层APP使用

同样我们参照系统其他服务的方式来写，来到frameworks/base/services/core/jni目录下，这下面有许多JNI的服务，创建cpp文件com\_android\_server\_am\_HelloService.cpp，为什么要叫这个名字，因为等下我们实现的AIDL服务包名为"com.android.server.am"

```
#include <jni.h>
#include <nativehelper/JNIHelp.h>
#include <binder/IServiceManager.h>
#include <android/hardware/hello_hidl/1.0/IHello.h>
#include <log/log.h>

using android::sp;
using android::hardware::hello_hidl::V1_0::IHello;

namespace android {

sp<IHello> hw_device;

static jint android_server_am_HelloService_nativeAdd(JNIEnv* env, jobject /* clazz */,jint a,jint b) {
      ALOGW("dongjiao...android_server_am_HelloService_nativeAdd.....");
      uint32_t total = hw_device->addition_hidl(a,b);
    return reinterpret_cast<jlong>(total);
}

static void android_server_am_HelloService_nativeInit(JNIEnv* /* env */,jobject /* clazz */) {
      ALOGW("dongjiao...android_server_am_HelloService_nativeInit.....");
      hw_device = IHello::getService();
        if (hw_device == nullptr) {
              ALOGW("dongjiao...failed to get IHello service");
              return;
     }
     ALOGW("dongjiao...success to get IHello service");
}

/*
 * JNI registration.
 */
static const JNINativeMethod gMethods[] = {
    { "nativeAdd", "(II)I", (void*) android_server_am_HelloService_nativeAdd },
    { "nativeInit", "()V", (void*) android_server_am_HelloService_nativeInit },
};

int register_android_server_am_HelloService(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, "com/android/server/am/HelloService",
            gMethods, NELEM(gMethods));
}

}; // namespace android

```

这个JNI服务中定义两个函数，android\_server\_am\_HelloService\_nativeAdd和android\_server\_am\_HelloService\_nativeInit，这两个函数是提供给framework层AIDL服务调用的，添加了一些log方便后面验证，对应等下要实现的AIDL服务中的nativeAdd和nativeInit

android\_server\_am\_HelloService\_nativeInit函数作用是获取我们上一篇文章实现的HIDL服务IHello

android\_server\_am\_HelloService\_nativeAdd函数作用是调用HIDL服务中定义的addition\_hidl函数

JNI服务中的函数想要被framework调用还需要通过register\_android\_server\_am\_HelloService函数进行注册，"com/android/server/am/HelloService"这个是等下我们要实现的framework层的AIDL服务

接着需要将这个自定义JNI服务添加到onload.cpp中开机注册，打开frameworks/base/services/core/jni/onload.cpp，添加如下代码：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3f29fe6b4561038bc67542552b2b37c8.png)

接着需要修改Android.bp文件，打开frameworks/base/services/core/jni/Android.bp，添加如下代码：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3ec94bf7e8ddf76b361a8c46f2a0f98b.png)  
主要就是将新增文件添加进编译和添加hello\_hidl的依赖库，JNI服务已经创建好了，接着，需要创建framework层AIDL服务

首先到frameworks/base/core/java/android/app/目录下创建IHelloService.aidl文件：

```
package android.app;

interface IHelloService {
    int add(int a,int b);
}

```

想要编译这个文件还需要修改Android.bp，在frameworks/base/Android.bp中添加如下代码：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/79570a2236f552213e8bdf4d60cf3cc9.png)  
然后就可以编译了，mmm frameworks/base  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/812c06987420e373ccff50113d634991.png)  
编译成功后我们可以去out目录下看看IHelloService.aidl编出来的IHelloService.java文件：

```
/*
 * This file is auto-generated.  DO NOT MODIFY.
 */
package android.app;
public interface IHelloService extends android.os.IInterface
{
  /** Default implementation for IHelloService. */
  public static class Default implements android.app.IHelloService
  {
    @Override public int add(int a, int b) throws android.os.RemoteException
    {
      return 0;
    }
    @Override
    public android.os.IBinder asBinder() {
      return null;
    }
  }
  /** Local-side IPC implementation stub class. */
  public static abstract class Stub extends android.os.Binder implements android.app.IHelloService
  {
    private static final java.lang.String DESCRIPTOR = "android.app.IHelloService";
    /** Construct the stub at attach it to the interface. */
    public Stub()
    {
      this.attachInterface(this, DESCRIPTOR);
    }
    /**
     * Cast an IBinder object into an android.app.IHelloService interface,
     * generating a proxy if needed.
     */
    public static android.app.IHelloService asInterface(android.os.IBinder obj)
    {
      if ((obj==null)) {
        return null;
      }
      android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
      if (((iin!=null)&&(iin instanceof android.app.IHelloService))) {
        return ((android.app.IHelloService)iin);
      }
      return new android.app.IHelloService.Stub.Proxy(obj);
    }
    @Override public android.os.IBinder asBinder()
    {
      return this;
    }
    /** @hide */
    public static java.lang.String getDefaultTransactionName(int transactionCode)
    {
      switch (transactionCode)
      {
        case TRANSACTION_add:
        {
          return "add";
        }
        default:
        {
          return null;
        }
      }
    }
    /** @hide */
    public java.lang.String getTransactionName(int transactionCode)
    {
      return this.getDefaultTransactionName(transactionCode);
    }
    @Override public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException
    {
      java.lang.String descriptor = DESCRIPTOR;
      switch (code)
      {
        case INTERFACE_TRANSACTION:
        {
          reply.writeString(descriptor);
          return true;
        }
        case TRANSACTION_add:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          int _arg1;
          _arg1 = data.readInt();
          int _result = this.add(_arg0, _arg1);
          reply.writeNoException();
          reply.writeInt(_result);
          return true;
        }
        default:
        {
          return super.onTransact(code, data, reply, flags);
        }
      }
    }
    private static class Proxy implements android.app.IHelloService
    {
      private android.os.IBinder mRemote;
      Proxy(android.os.IBinder remote)
      {
        mRemote = remote;
      }
      @Override public android.os.IBinder asBinder()
      {
        return mRemote;
      }
      public java.lang.String getInterfaceDescriptor()
      {
        return DESCRIPTOR;
      }
      @Override public int add(int a, int b) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        int _result;
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(a);
          _data.writeInt(b);
          boolean _status = mRemote.transact(Stub.TRANSACTION_add, _data, _reply, 0);
          if (!_status && getDefaultImpl() != null) {
            return getDefaultImpl().add(a, b);
          }
          _reply.readException();
          _result = _reply.readInt();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
        return _result;
      }
      public static android.app.IHelloService sDefaultImpl;
    }
    static final int TRANSACTION_add = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
    public static boolean setDefaultImpl(android.app.IHelloService impl) {
      if (Stub.Proxy.sDefaultImpl == null && impl != null) {
        Stub.Proxy.sDefaultImpl = impl;
        return true;
      }
      return false;
    }
    public static android.app.IHelloService getDefaultImpl() {
      return Stub.Proxy.sDefaultImpl;
    }
  }
  public int add(int a, int b) throws android.os.RemoteException;
}
```

其实这个文件和我们用Android Studio创建AIDL服务生成的中间文件差不多的，都是统一的AIDL框架：有一个Stub抽象类，继承IBinder，实现IHelloService，还有一个代理类Proxy继承IHelloService，通过asInterface方法来获取

了解了IHelloService.aidl生成的一个中间文件，我们再实现HelloService.java的时候就清晰了，在frameworks/base/services/core/java/com/android/server/am/目录下创建HelloService.java文件：

```
package com.android.server.am;
import android.app.IHelloService;
public class HelloService extends IHelloService.Stub {
    public HelloService(){
      android.util.Log.d("dongjiao","Start HelloService...");
      nativeInit();
    }
    @Override 
    public int add(int a, int b){
      android.util.Log.d("dongjiao","HelloService add()...a = :"+a+",b = :"+b);
      return nativeAdd(a,b);
    }
    private static native void nativeInit();
    private static native int nativeAdd(int a,int b);
}

```

这个HelloService继承自IHelloService.Stub，它作为Binder的具体实现端，里面定义了两个native方法，这两个方法和之前创建的JNI服务中的那两个函数一一对应，HelloService构造方法中调用nativeInit，add方法提供给外界访问，它里面调用nativeAdd

好了这个AIDL服务已经创建好了，接着我们到SystemServer中去添加开机注册此服务的代码，打开frameworks/base/services/java/com/android/server/SystemServer.java随便在其他某个服务下添加如下代码：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/ad4f1c45f77349a69e2cdd8c7aa63f0a.png)  
SystemServer启动时就会将HelloService添加到ServiceManager，名字自定义为”helloService“，代码已经添加完毕，总的修改就是如下部分：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b317bab2f7f0601e998ec17290c78438.png)

开始进行编译 mmm frameworks/base/

编译成功后需要将/system/framework/下所有文件push进手机  
adb push out/target/product/TOKYO\_TF\_arm64/system/framework/ /system/

另外定义的JIN服务相关代码会被编译到libandroid\_servers.so这个so中，还需push这个so  
adb push out/target/product/TOKYO\_TF\_arm64/system/lib64/libandroid\_servers.so /system/lib64/

重启手机发现了如下错误：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/de1ceb9e926024ac793288613b1bab5c.png)  
这是因为缺少了SELinux权限，实际开发中添加自定义AIDL，HIDL服务都需要SELinux权限，我们这里重点不在SELinux，所以采用规避方案，直接将SELinux关闭**adb shell setenforce 0**，这需要root权限

我们发现如下log，这是因为我的HIDL服务还没启动  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/57d860909340e11722f1fab0246a64ee.png)

启动一下前一篇文章实现的HIDL服务:  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/0ff7c70b00ce8a73d7f541fc9f2541dc.png)  
我们重新将SystemServer杀掉，为了再看一遍log：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/8ffcffc57daadf43286afa9aefac4f88.png)

04-14 23:58:58.760 9279 9279 E : hello\_hidl service is init success…  
04-14 23:58:58.761 9279 9279 I ServiceManagement: Registered android.hardware.hello\_hidl@1.0::IHello/default (start delay of 52ms)  
04-14 23:58:58.762 9279 9279 I ServiceManagement: Removing namespace from process name android.hardware.hello\_hidl@1.0-service to hello\_hidl@1.0-service.  
04-14 23:58:58.762 9279 9279 I android.hardware.hello\_hidl@1.0-service: Registration complete for android.hardware.hello\_hidl@1.0::IHello/default.  
这一段代表的是HIDL服务的启动注册

04-14 23:59:38.881 328 328 I ServiceManager: service ‘helloService’ died  
04-14 23:59:41.582 14037 14037 W : JNI\_OnLoad…hello…  
04-14 23:59:41.594 14037 14037 W : register\_android\_server\_am\_HelloService…  
04-14 23:59:44.978 14037 14037 D dongjiao: SystemServer…addService(helloService)…  
04-14 23:59:44.978 14037 14037 D dongjiao: Start HelloService…  
04-14 23:59:44.978 14037 14037 W : HelloService…nativeInit…  
04-14 23:59:44.980 14037 14037 W : success to get IHello service  
这一段代表AIDL服务的启动注册

可以看到关闭了SELinux权限之后，HIDL和AIDL服务都注册成功了，并且在AIDL服务初始化时也能成功通过JNI服务获取到HIDL服务了，后面如果调用HIDL的addition\_hidl函数也应该是很简单了

其实整个调用逻辑还是比较清晰的，从AIDL到JNI到HIDL，在AIDL服务初始化中调用JNI服务的nativeInit函数，JNI服务的nativeInit函数中获取到HIDL服务，之后就可以随意调用HIDL的函数了

下一篇文章我将写一个APP通过AIDL调用HIDL的addition\_hidl函数