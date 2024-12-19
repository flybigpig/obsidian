本篇主要分析Binder在framework中Java层的框架，相关源码

```
framework/base/core/java/android/os/
  - IInterface.java
  - IServiceManager.java
  - ServiceManager.java
  - ServiceManagerNative.java(包含内部类ServiceManagerProxy)

framework/base/core/java/android/os/
  - IBinder.java
  - Binder.java(包含内部类BinderProxy)
  - Parcel.java

framework/base/core/java/com/android/internal/os/
  - BinderInternal.java

framework/base/core/jni/
  - AndroidRuntime.cpp
  - android_os_Parcel.cpp
  - android_util_Binder.cpp
```

链接如下：

-   [IInterface.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FIInterface.java&objectId=1199107&objectType=1&isNewArticle=undefined)
-   [IServiceManager.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FIServiceManager.java&objectId=1199107&objectType=1&isNewArticle=undefined)
-   [ServiceManager.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FServiceManager.java&objectId=1199107&objectType=1&isNewArticle=undefined)
-   [ServiceManagerNative.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FServiceManagerNative.java&objectId=1199107&objectType=1&isNewArticle=undefined)
-   [IBinder.java)](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FIBinder.java&objectId=1199107&objectType=1&isNewArticle=undefined)
-   [Binder.java)](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FBinder.java&objectId=1199107&objectType=1&isNewArticle=undefined)
-   [Parcel.java)](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FParcel.java&objectId=1199107&objectType=1&isNewArticle=undefined)
-   [AndroidRuntime.cppshi](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2FAndroidRuntime.cpp&objectId=1199107&objectType=1&isNewArticle=undefined)
-   [AndroidRuntime.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2FAndroidRuntime.cpp&objectId=1199107&objectType=1&isNewArticle=undefined)
-   [android\_os\_Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_os_Parcel.cpp&objectId=1199107&objectType=1&isNewArticle=undefined)
-   [android\_util\_Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199107&objectType=1&isNewArticle=undefined)

### 一、概述

Binder在framework层，采用JNI技术来调用native(C/C++)层的binder架构，从而为上层应用程序提供服务。看过binder之前的文章，我们知道native层中，binder是C/S架构，分为Bn端(Server)和Bp端(Client)。对于Java层在命令与架构上非常相近，同时实现了一套IPC通信架构。

##### (一)架构图

framework Binder架构图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/xxtyrqbzp8.png)

Binder架构图.png

图解：

-   图中紫色色代表整个framework层binder相关组件，其中Binder代表Server端，BinderProxy代表Client端
-   图中黑色代表Native层Binder架构相关组件
-   上层framework层的Binder逻辑是建立在Native层架构基础上的，核心逻辑都是交于Native层来处理
-   framework层的ServiceManager类与Native层的功能并不完全对应，framework层的ServiceManager的实现对最终是通过BinderProxy传递给Native层来完成的。

##### (二)、类图

下面列举framework的binder类的关系图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/0ms6ezy43b.png)

类的关系图.png

图解： 其中蓝色都是interface,其余都是Class

-   ServiceManager：通过getIServiceManager方法获取的是ServiceManagerProxy对象。ServiceMnager的addService()，getService()实际工作都交给ServiceManagerProxy的相应方法来处理。
-   ServiceManagerProxy：其成员变量mRemote指向BinderProxy对象，ServiceManagerProxy的addService()，getService()方法最终是交给mRemote去完成。
-   ServiceManagerNative：其方法asInterface()返回的是ServiceManagerProxy对象，ServiceManager便是借助ServiceManagerNative类来找到ServiceManagerProxy。
-   Binder：其成员mObject和方法execTransact()用于native方法
-   BinderInternal：内部有一个GcWatcher类，用于处理和调试与Binder相关的拦击回收。
-   IBinder：接口中常量FLAG\_ONEWAY:客户端利用binder跟服务端通信是阻塞式的，但如果设置了FLAG\_ONEWAY，这成为非阻塞的调用方式，客户端能立即返回，服务端采用回调方式来通知客户端完成情况。另外IBinder接口有一个内部接口DeathDecipent(死亡通告)。

##### (三)、Binder类分层

整个Binder从kernel至native，JNI，Framework层所涉及的全部类

![](https://ask.qcloudimg.com/http-save/yehe-2957818/9vvt1p31j9.png)

Binder类分层.png

Android应用程序使用Java语言开发，Binder框架自然也少不了在Java层提供接口。前面的文章我们知道，Binder机制在C++层有了完整的实现。因此Java层完全不用重复实现，而是通过JNI衔接C++层以复用其实现。

关于Binder类中 从Binder Framework层到C++层的衔接关系如下图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/9ur8vgeokx.png)

Binder衔接关系图.png

图解：

-   IInterface(interface) ：供Java层Binder服务接口继承的接口
-   IBinder(interface)：Java层IBinder类，提供了transaction方法来调用远程服务
-   Binder(class)：实现了IBinder接口，封装了JNI的实现。Java层Binder服务的基类
-   BInderProxy(class)：实现了IBinder接口，封装了JNI的实现。提供transac()方法调用远程服务
-   JavaBBinderHolder(class) ：内部存储了JavaBBinder
-   JavaBBinder(class)：将C++端的onTransact调用传递到Java端
-   Parcel(class)：Java层的数据包装器。

这里的IInterface，IBinder和C++层的两个类是同名的。这个同名并不是巧合：它们不仅仅是同名，它们所起到的作用，以及其中包含的接口几乎都是一样的，区别仅仅是一个在C++层，一个在Java层而已。而且除了IInterface，IBinder之外，这里Binder与BinderProxy类也是与C++的类对应的，下面列出了Java层和C++层类的对应关系。

|  |  |
| --- | --- |
| 
IInterface



 | 

IInterface



 |
| 

IBinder



 | 

IBinder



 |
| 

BBinder



 | 

BBinder



 |
| 

BpProxy



 | 

BpProxy



 |
| 

Parcel



 | 

Parcel



 |

##### (四)、JNI的衔接

> JNI全称是Java Native Interface，这个是由Java虚拟机提供的机制。这个机制使得natvie代码可以和Java代码相互通讯。简单的来说就是：我们可以在C/C++端调用Java代码，也可以在Java端调用C/C++代码。

关于JNI的详细说明，可以参见Oracle的官方文档:[Java Native Interface](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fdocs.oracle.com%2Fjavase%2F8%2Fdocs%2Ftechnotes%2Fguides%2Fjni%2F&objectId=1199107&objectType=1&isNewArticle=undefined) ，这里就不详细说明了。

> 实际上，在Android中很多的服务或者机制都是在C/C++层实现的，想要将这些实现复用到Java层 就必须通过JNI进行衔接。Android Opne Source Projcet(以后简称AOSP)在源码中，/frameworks/base/core/jni/ 目录下的源码就是专门用来对接Framework层的JNI实现。其实大家看一下Binder.java的实现就会发现，这里面有不少的方法都是native 关键字修饰的，并且没有方法实现体，这些方法其实都是在C++中实现的。

以Binder为例：

###### 1、Java调用C++层代码

```
// Binder.java
public static final native int getCallingPid();

public static final native int getCallingUid();

public static final native long clearCallingIdentity();

public static final native void restoreCallingIdentity(long token);

public static final native void setThreadStrictModePolicy(int policyMask);

public static final native int getThreadStrictModePolicy();

public static final native void flushPendingCommands();

public static final native void joinThreadPool();
```

在 android\_util\_Binder.cpp文件中的下面的这段代码，设定了Java方法与C++方法对应关系

```
//android_util_Binder      843行
static const JNINativeMethod gBinderMethods[] = {
    { "getCallingPid", "()I", (void*)android_os_Binder_getCallingPid },
    { "getCallingUid", "()I", (void*)android_os_Binder_getCallingUid },
    { "clearCallingIdentity", "()J", (void*)android_os_Binder_clearCallingIdentity },
    { "restoreCallingIdentity", "(J)V", (void*)android_os_Binder_restoreCallingIdentity },
    { "setThreadStrictModePolicy", "(I)V", (void*)android_os_Binder_setThreadStrictModePolicy },
    { "getThreadStrictModePolicy", "()I", (void*)android_os_Binder_getThreadStrictModePolicy },
    { "flushPendingCommands", "()V", (void*)android_os_Binder_flushPendingCommands },
    { "init", "()V", (void*)android_os_Binder_init },
    { "destroy", "()V", (void*)android_os_Binder_destroy },
    { "blockUntilThreadAvailable", "()V", (void*)android_os_Binder_blockUntilThreadAvailable }
};
```

这种对应关系意味着：当Binder.java中的getCallingPid()方法被调用的时候，真正的实现其实是android\_os\_Binder\_getCallingPic，当getCallUid方法被调用的时候，真正的实现其实是android\_os\_Binder\_getCallingUid，其他类同。

然后我们再看一下android\_os\_Binder\_getCallingPid方法的实现就会发现，这里其实就是对街道了libbinder中了：

```
static jint android_os_Binder_getCallingPid(JNIEnv* env, jobject clazz)
{
    return IPCThreadState::self()->getCallingPid();
}
```

###### 2、C++层代码调用Java代码

上面看到了Java端代码是如何调用libbinder中的C++方法的。那么相反的方向是如何调用的？关键，libbinder中的\*\* BBinder::onTransacts \*\*是如何能能够调用到Java中的Binder:: onTransact的？

这段逻辑就是android\_util\_Binder.cpp中JavaBBinder::onTransact中处理的了。JavaBBinder是BBinder的子类，其类的结构如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/d411sjsfni.png)

JavaBBinder类结构.png

JavaBBinder:: onTransact关键代码如下：

```
//android_util_Binder      247行
virtual status_t onTransact(
   uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags = 0)
{
   JNIEnv* env = javavm_to_jnienv(mVM);

   IPCThreadState* thread_state = IPCThreadState::self();
   const int32_t strict_policy_before = thread_state->getStrictModePolicy();

   jboolean res = env->CallBooleanMethod(mObject, gBinderOffsets.mExecTransact,
       code, reinterpret_cast<jlong>(&data), reinterpret_cast<jlong>(reply), flags);
   ...
}
```

注意这一段代码：

```
jboolean res = env->CallBooleanMethod(mObject, gBinderOffsets.mExecTransact,
  code, reinterpret_cast<jlong>(&data), reinterpret_cast<jlong>(reply), flags);
```

这一行代码其实是在调用mObject上offset为mExecTransact的方法。这里的几个参数说明下：

-   mObject指向了Java端的Binder对象
-   gBinderOffsets.mExecTransact指向了Binder类的exectTransac方法
-   data调用了exectTransac方法的参数
-   code，data，reply，flag都是传递给调用方法execTransact参数

而JNIEnv.callBooleanMethod这个方法是由虚拟机实现的。即：虚拟机提供native方法来调用一个Java Object上方法。

###### 这样，就在C++层的JavaBBinder::onTransact中调用了Java层 Binder::execTransact方法。而在Binder::execTransact方法中，又调用了自身的onTransact方法，由此保证整个过程串联起来。

### 二、初始化

在Android系统开始过程中，Zygote启东时会有一个"虚拟机注册过程"，该过程调用AndroidRuntime::startReg()方法来完成jni方法的注册

##### 1、startReg()函数

```
//  frameworks/base/core/jni/AndroidRuntime.cpp   1440行
int AndroidRuntime::startReg(JNIEnv* env)
{
    androidSetCreateThreadFunc((android_create_thread_fn) javaCreateThreadEtc);
    env->PushLocalFrame(200);
    //核心函数   register_jni_procs()  注册jni方法
    if (register_jni_procs(gRegJNI, NELEM(gRegJNI), env) < 0) {
        env->PopLocalFrame(NULL);
        return -1;
    }
    env->PopLocalFrame(NULL);
    return 0;
}
```

注册 JNI方法，其中gRegJNI是一个数组，记录所有需要注册的jni方法，其中有一项便是REG\_JNI(register\_android\_os\_Binder)。

###### 1.1 gRegJNI数组

REG\_JNI(register\_android\_os\_Binder)在 \*\* frameworks/base/core/jni/AndroidRuntime.cpp \*\* 的1312行，大家自行去查看吧。

```
// frameworks/base/core/jni/AndroidRuntime.cpp   1296行。
static const RegJNIRec gRegJNI[] = {
    ......  
    REG_JNI(register_android_os_SystemProperties),
    // *****  重点部分  *****
    REG_JNI(register_android_os_Binder),
    // *****  重点部分  *****
    REG_JNI(register_android_os_Parcel),
    ......  
};
```

###### 1.2 register\_jni\_procs() 函数

```
//  frameworks/base/core/jni/AndroidRuntime.cpp   1283行
    static int register_jni_procs(const RegJNIRec array[], size_t count, JNIEnv*env) {
        for (size_t i = 0; i < count; i++) {
            if (array[i].mProc(env) < 0) {
                #ifndef NDEBUG
                ALOGD("----------!!! %s failed to load\n", array[i].mName);
                #endif
                return -1;
            }
        }
        return 0;
    }
```

那让我们继续看

###### 1.3 RegJNIRec数据结构

```
#ifdef NDEBUG
    #define REG_JNI(name)      { name }
   struct RegJNIRec {
                int (*mProc)(JNIEnv*);
            };
#else
    #define REG_JNI(name)      { name, #name }
    struct RegJNIRec {
              int (*mProc)(JNIEnv*);
              const char* mName;
            };
#endif
```

所以这里最终调用了register\_android\_os\_Binder()函数，下面说说register\_android\_os\_Binder过程。

##### 2、register\_android\_os\_Binder()函数

```
// frameworks/base/core/jni/android_util_Binder.cpp    1282行
int register_android_os_Binder(JNIEnv* env)
{
    // 注册Binder类的 jin方法
    if (int_register_android_os_Binder(env) < 0)
        return -1;

    // 注册 BinderInternal类的jni方法
    if (int_register_android_os_BinderInternal(env) < 0)
        return -1;

    // 注册BinderProxy类的jni方法
    if (int_register_android_os_BinderProxy(env) < 0)
        return -1;
    ...
    return 0;
}
```

这里面主要是三个注册方法

-   int\_register\_android\_os\_Binder()：注册Binder类的JNI方法
-   int\_register\_android\_os\_BinderInternal()：注册BinderInternal的JNI方法
-   int\_register\_android\_os\_BinderProxy()：注册BinderProxy类的JNI方法

那么就让我们依次研究下

###### 2.1 int\_register\_android\_os\_Binder()函数

```
// frameworks/base/core/jni/android_util_Binder.cpp    589行
static int int_register_android_os_Binder(JNIEnv* env)
{
    //kBinderPathName="android/os/Binder"，主要是查找kBinderPathName路径所属类
    jclass clazz = FindClassOrDie(env, kBinderPathName);
    //将Java层Binder类保存到mClass变量上
    gBinderOffsets.mClass = MakeGlobalRefOrDie(env, clazz);
    //将Java层execTransact()方法保存到mExecTransact变量；
    gBinderOffsets.mExecTransact = GetMethodIDOrDie(env, clazz, "execTransact", "(IJJI)Z");
    //将Java层的mObject属性保存到mObject变量中
    gBinderOffsets.mObject = GetFieldIDOrDie(env, clazz, "mObject", "J");
    //注册JNI方法
    return RegisterMethodsOrDie(env, kBinderPathName, gBinderMethods,
        NELEM(gBinderMethods));
}
```

PS: 注册Binder的JNI方法，其中：

-   FindClassOrDie(env, kBinderPathName) 等于 env->FindClass(kBinderPathName)
-   MakeGlobalRefOrDie() 等于 env->NewGlobalRef()
-   GetMethodIDOrDie() 等于 env->GetMethodID()
-   GetFieldIDOrDie() 等于 env->GeFieldID()
-   RegisterMethodsOrDie() 等于 Android::registerNativeMethods();

上面代码提到了gBinderOffsets，它是一个什么东西？

###### 2.1.1 gBinderOffsets：

gBinderOffsets是全局静态结构体(struct)，定义如下：

```
// frameworks/base/core/jni/android_util_Binder.cpp    65行
static struct bindernative_offsets_t
{
    // Class state.
    //记录 Binder类
    jclass mClass; 
    // 记录execTransact()方法
    jmethodID mExecTransact; 
    // Object state.
    // 记录mObject属性
    jfieldID mObject; 
} gBinderOffsets;
```

> gBinderOffsets保存了Binder.java类本身以及其成员方法execTransact()和成员属性mObject，这为JNI层访问Java层提供通道。另外通过查询获取Java层 binder信息后保存到gBinderOffsets，而不再需要每次查找binder类信息的方式能大幅提高效率，是由于每次查询需要花费较多的CPU时间，尤其是频繁访问时，但用额外的结构体来保存这些信息，是以空间换时间的方法。

###### 2.1.2 gBinderMethods：

```
// frameworks/base/core/jni/android_util_Binder.cpp    843行
static const JNINativeMethod gBinderMethods[] = {
     /* 名称, 签名, 函数指针 */
    { "getCallingPid", "()I", (void*)android_os_Binder_getCallingPid },
    { "getCallingUid", "()I", (void*)android_os_Binder_getCallingUid },
    { "clearCallingIdentity", "()J", (void*)android_os_Binder_clearCallingIdentity },
    { "restoreCallingIdentity", "(J)V", (void*)android_os_Binder_restoreCallingIdentity },
    { "setThreadStrictModePolicy", "(I)V", (void*)android_os_Binder_setThreadStrictModePolicy },
    { "getThreadStrictModePolicy", "()I", (void*)android_os_Binder_getThreadStrictModePolicy },
    { "flushPendingCommands", "()V", (void*)android_os_Binder_flushPendingCommands },
    { "init", "()V", (void*)android_os_Binder_init },
    { "destroy", "()V", (void*)android_os_Binder_destroy },
    { "blockUntilThreadAvailable", "()V", (void*)android_os_Binder_blockUntilThreadAvailable }
};
```

通过RegisterMethodsOrDie()，将为gBinderMethods数组中的方法建立了一一映射关系，从而为Java层访问JNI层提供了通道。

###### 2.1.3 总结：

总结，int\_register\_android\_os\_Binder方法的主要功能：

-   通过gBinderOffsets，保存Java层Binder类的信息，为JNI层访问Java层提供了通道
-   通过RegisterMethodsOrDie，将gBinderMethods数组完成映射关系，从而为Java层访问JNI层提供通道

也就是说该过程建立了Binder在Native层与framework之间的相互调用的桥梁。

###### 2.2 int\_register\_android\_os\_BinderInternal()函数

注册 BinderInternal

```
// frameworks/base/core/jni/android_util_Binder.cpp    935行
static int int_register_android_os_BinderInternal(JNIEnv* env)
{
    //其中kBinderInternalPathName
    jclass clazz = FindClassOrDie(env, kBinderInternalPathName);
    gBinderInternalOffsets.mClass = MakeGlobalRefOrDie(env, clazz);
    gBinderInternalOffsets.mForceGc = GetStaticMethodIDOrDie(env, clazz, "forceBinderGc", "()V");
    return RegisterMethodsOrDie(
        env, kBinderInternalPathName,
        gBinderInternalMethods, NELEM(gBinderInternalMethods));
}
```

注册 Binderinternal类的jni方法，gBinderInternaloffsets保存了BinderInternal的forceBinderGc()方法。

下面是BinderInternal类的JNI方法注册

```
// frameworks/base/core/jni/android_util_Binder.cpp    925号
static const JNINativeMethod gBinderInternalMethods[] = {
    { "getContextObject", "()Landroid/os/IBinder;", (void*)android_os_BinderInternal_getContextObject },
    { "joinThreadPool", "()V", (void*)android_os_BinderInternal_joinThreadPool },
    { "disableBackgroundScheduling", "(Z)V", (void*)android_os_BinderInternal_disableBackgroundScheduling },
    { "handleGc", "()V", (void*)android_os_BinderInternal_handleGc }
};
```

该过程 和**注册Binder类 JNI**非常类似，也就是说该过程建立了是BinderInternal类在Native层与framework层之间的相互调用的桥梁。

###### 2.3 int\_register\_android\_os\_BinderProxy()函数

```
// frameworks/base/core/jni/android_util_Binder.cpp     1254行
static int int_register_android_os_BinderProxy(JNIEnv* env)
{
    //gErrorOffsets 保存了Error类信息
    jclass clazz = FindClassOrDie(env, "java/lang/Error");
    gErrorOffsets.mClass = MakeGlobalRefOrDie(env, clazz);
    //gBinderProxyOffsets保存了BinderProxy类的信息
    //其中kBinderProxyPathName="android/os/BinderProxy"
    clazz = FindClassOrDie(env, kBinderProxyPathName);
    gBinderProxyOffsets.mClass = MakeGlobalRefOrDie(env, clazz);
    gBinderProxyOffsets.mConstructor = GetMethodIDOrDie(env, clazz, "<init>", "()V");
    gBinderProxyOffsets.mSendDeathNotice = GetStaticMethodIDOrDie(env, clazz, "sendDeathNotice", "(Landroid/os/IBinder$DeathRecipient;)V");
    gBinderProxyOffsets.mObject = GetFieldIDOrDie(env, clazz, "mObject", "J");
    gBinderProxyOffsets.mSelf = GetFieldIDOrDie(env, clazz, "mSelf", "Ljava/lang/ref/WeakReference;");
    gBinderProxyOffsets.mOrgue = GetFieldIDOrDie(env, clazz, "mOrgue", "J");
    //gClassOffsets保存了Class.getName()方法
    clazz = FindClassOrDie(env, "java/lang/Class");
    gClassOffsets.mGetName = GetMethodIDOrDie(env, clazz, "getName", "()Ljava/lang/String;");
    return RegisterMethodsOrDie(
        env, kBinderProxyPathName,
        gBinderProxyMethods, NELEM(gBinderProxyMethods));
}
```

注册BinderPoxy类的JNI方法，gBinderProxyOffsets保存了BinderProxy的构造方法，sendDeathNotice()，mObject，mSelf，mOrgue信息。

我们来看下gBinderProxyOffsets

###### 2.3.1 gBinderProxyOffsets结构体

```
// frameworks/base/core/jni/android_util_Binder.cpp     95行
static struct binderproxy_offsets_t {
        // Class state.
        // 对应的是 class对象 android.os.BinderProxy
        jclass mClass;
        // 对应的是  BinderProxy的构造函数
        jmethodID mConstructor;
        // 对应的是  BinderProxy的sendDeathNotice方法
        jmethodID mSendDeathNotice;

        // Object state.
        // 对应的是 BinderProxy的 mObject字段
        jfieldID mObject;
        // 对应的是 BinderProxy的mSelf字段
        jfieldID mSelf;
        // 对应的是 BinderProxymOrgue字段
        jfieldID mOrgue;
}   gBinderProxyOffsets;
```

PS: 这里补充下BinderProxy类是Binder类的内部类 下面BinderProxy类的JNI方法注册：

```
// frameworks/base/core/jni/android_util_Binder.cpp     1241行
static const JNINativeMethod gBinderProxyMethods[] = {
     /* 名称, 签名, 函数指针 */
    {"pingBinder",          "()Z", (void*)android_os_BinderProxy_pingBinder},
    {"isBinderAlive",       "()Z", (void*)android_os_BinderProxy_isBinderAlive},
    {"getInterfaceDescriptor", "()Ljava/lang/String;", (void*)android_os_BinderProxy_getInterfaceDescriptor},
    {"transactNative",      "(ILandroid/os/Parcel;Landroid/os/Parcel;I)Z", (void*)android_os_BinderProxy_transact},
    {"linkToDeath",         "(Landroid/os/IBinder$DeathRecipient;I)V", (void*)android_os_BinderProxy_linkToDeath},
    {"unlinkToDeath",       "(Landroid/os/IBinder$DeathRecipient;I)Z", (void*)android_os_BinderProxy_unlinkToDeath},
    {"destroy",             "()V", (void*)android_os_BinderProxy_destroy},
};
```

该过程上面非常类似，也就是说该过程建立了是BinderProxy类在Native层与framework层之间的相互调用的桥梁。

### 三、注册服务

注册服务在ServiceManager里面

```
//frameworks/base/core/java/android/os/ServiceManager.java     70行
public static void addService(String name, IBinder service, boolean allowIsolated) {
    try {
        //getIServiceManager()是获取ServiceManagerProxy对象
        // addService() 是执行注册服务操作
        getIServiceManager().addService(name, service, allowIsolated); 
    } catch (RemoteException e) {
        Log.e(TAG, "error in addService", e);
    }
}
```

##### (一) 、先来看下getIServiceManager()方法

```
//frameworks/base/core/java/android/os/ServiceManager.java     70行
    private static IServiceManager getIServiceManager() {
        if (sServiceManager != null) {
            return sServiceManager;
        }
        // Find the service manager
        sServiceManager = ServiceManagerNative.asInterface(BinderInternal.getContextObject());
        return sServiceManager;
    }
```

通过上面，大家的第一反应应该是sServiceManager是单例的。第二反映是如果想知道sServiceManager的值，必须了解**BinderInternal.getContextObject()的返回值**和 **ServiceManagerNative.asInterface()**方法的内部执行，那我们就来详细了解下

###### 1、先来看下BinderInternal.getContextObject()方法

```
//frameworks/base/core/java/com/android/internal/os/BinderInternal.java  88行
    /**
     * Return the global "context object" of the system.  This is usually
     * an implementation of IServiceManager, which you can use to find
     * other services.
     */
    public static final native IBinder getContextObject();
```

可见BinderInternal.getContextObject()最终会调用JNI通过C层来实现，那我们就继续跟踪

###### 1.1、android\_os\_BinderInternal\_getContextObject)函数

```
// frameworks/base/core/jni/android_util_Binder.cpp     899行
static jobject android_os_BinderInternal_getContextObject(JNIEnv* env, jobject clazz)
{
    sp<IBinder> b = ProcessState::self()->getContextObject(NULL);
    return javaObjectForIBinder(env, b);  
}
```

看到上面的代码 大家有没有熟悉的感觉，前面讲过了：对于ProcessState::self() -> getContextObject() 对于ProcessState::self()->getContextObject()可以理解为new BpBinder(0)，那就剩下 **javaObjectForIBinder(env, b)** 那我们就来看下这个函数

###### 1.2、javaObjectForIBinder()函数

```
// frameworks/base/core/jni/android_util_Binder.cpp         547行
jobject javaObjectForIBinder(JNIEnv* env, const sp<IBinder>& val)
{
    if (val == NULL) return NULL;
    //返回false
    if (val->checkSubclass(&gBinderOffsets)) { 
        jobject object = static_cast<JavaBBinder*>(val.get())->object();
        return object;
    }

    AutoMutex _l(mProxyLock);

    jobject object = (jobject)val->findObject(&gBinderProxyOffsets);
    //第一次 object为null
    if (object != NULL) { 
        //查找是否已经存在需要使用的BinderProxy对应，如果有，则返回引用。
        jobject res = jniGetReferent(env, object);
        if (res != NULL) {
            return res;
        }
        android_atomic_dec(&gNumProxyRefs);
        val->detachObject(&gBinderProxyOffsets);
        env->DeleteGlobalRef(object);
    }

    //创建BinderProxy对象
    object = env->NewObject(gBinderProxyOffsets.mClass, gBinderProxyOffsets.mConstructor);
    if (object != NULL) {
        // BinderProxy.mObject成员变量记录BpBinder对象
        env->SetLongField(object, gBinderProxyOffsets.mObject, (jlong)val.get());
        val->incStrong((void*)javaObjectForIBinder);

        jobject refObject = env->NewGlobalRef(
                env->GetObjectField(object, gBinderProxyOffsets.mSelf));
         //将BinderProxy对象信息附加到BpBinder的成员变量mObjects中
        val->attachObject(&gBinderProxyOffsets, refObject,
                jnienv_to_javavm(env), proxy_cleanup);

        sp<DeathRecipientList> drl = new DeathRecipientList;
        drl->incStrong((void*)javaObjectForIBinder);
         // BinderProxy.mOrgue成员变量记录死亡通知对象
        env->SetLongField(object, gBinderProxyOffsets.mOrgue, reinterpret_cast<jlong>(drl.get()));

        android_atomic_inc(&gNumProxyRefs);
        incRefsCreated(env);
    }
    return object;
}
```

上面的大致流程如下：

-   1、第二个入参val在有些时候指向BpBinder，有些时候指向JavaBBinder
-   2、至于是BpBinder还是JavaBBinder是通过if (val->checkSubclass(&gBinderOffsets)) 这个函数来区分的，如果是JavaBBinder，则为true，则就会通过成员函数object()，返回一个Java对象，这个对象就是Java层的Binder对象。由于我们这里是BpBinder，所以是 \*\* 返回false\*\*
-   3如果是BpBinder，会先判断是不是第一次，如果是第一次,下面的object为null。

```
jobject object = (jobject)val->findObject(&gBinderProxyOffsets);
```

如果不是第一次，就会先查找是否已经存在需要使用的BinderProxy对象，如果找到就会返回引用

-   4、如果没有找到可用的引用，就new一个BinderProxy对象

所以主要是根据BpBinder(C++) 生成BinderProxy(Java对象)，主要工作是创建BinderProxy对象，并把BpBinder对象地址保存到BinderProxy.mObject成员变量。到此，可知ServiceManagerNative.asInterface(BinderInternal.getContextObject()) 等价于

```
ServiceManagerNative.asInterface(new BinderProxy())
```

###### 2、再来看下ServiceManagerNative.asInterface()方法

```
//frameworks/base/core/java/android/os/ServiceManagerNative.java      33行
    /**
     * Cast a Binder object into a service manager interface, generating
     * a proxy if needed.
     * 将Binder对象转换service manager interface，如果需要，生成一个代理。
     */
    static public IServiceManager asInterface(IBinder obj)
    {
        //obj为 BpBinder
        // 如果 obj为null 则直接返回
        if (obj == null) {
            return null;
        }
        // 由于是BpBinder，所以BpBinder的queryLocalInterface(descriptor) 默认返回null
        IServiceManager in =
            (IServiceManager)obj.queryLocalInterface(descriptor);
        if (in != null) {
            return in;
        }
        return new ServiceManagerProxy(obj);
    }
```

我们看下这个obj.queryLocalInterface(descriptor)方法，其实他是调用的IBinder的native方法如下

```
public interface IBinder {
    .....
    /**
     * Attempt to retrieve a local implementation of an interface
     * for this Binder object.  If null is returned, you will need
     * to instantiate a proxy class to marshall calls through
     * the transact() method.
     */
    public IInterface queryLocalInterface(String descriptor);
    .....
}
```

> 通过注释我们知道,queryLocalInterface是查询本地的对象，我简单解释下什么是本地对象，这里的本地对象是指，如果进行IPC调用，如果是两个进程是同一个进程，即对象是本地对象；如果两个进程是两个不同的进程，则返回的远端的代理类。所以在BBinder的子类BnInterface中，重载了这个方法，返回this，而在BpInterface并没有重载这个方法。又因为queryLocalInterface 默认返回的是null，所以obj.queryLocalInterface=null。 所以最后结论是 **return new ServiceManagerProxy(obj);**

那我们来看下ServiceManagerProxy

###### 2.1、ServiceManagerProxy

PS:ServiceManagerProxy是ServiceManagerNative类的内部类

```
//frameworks/base/core/java/android/os/ServiceManagerNative.java    109行
class ServiceManagerProxy implements IServiceManager {
    public ServiceManagerProxy(IBinder remote) {
        mRemote = remote;
    }
}
```

> mRemote为BinderProxy对象，该BinderProxy对象对应于BpBinder(0)，其作为binder代理端，指向native的层的Service Manager。

所以说：

> ServiceManager.getIServiceManager最终等价于new ServiceManagerProxy(new BinderProxy())。所以

```
 getIServiceManager().addService()
```

等价于

```
ServiceManagerNative.addService();
```

framework层的ServiceManager的调用实际的工作确实交给了ServiceManagerProxy的成员变量BinderProxy；而BinderProxy通过JNI的方式，最终会调用BpBinder对象；可见上层binder结构的核心功能依赖native架构的服务来完成的。

##### (二) addService()方法详解

上面已经知道了

```
getIServiceManager().addService(name, service, allowIsolated); 
```

等价于

```
ServiceManagerProxy..addService(name, service, allowIsolated);
```

> PS:上面的ServiceManagerProxy代表ServiceManagerProxy对象

所以让我们来看下ServiceManagerProxy的addService()方法

###### 1、ServiceManagerProxy的addService()

```
//frameworks/base/core/java/android/os/ServiceManagerNative.java     142行
    public void addService(String name, IBinder service, boolean allowIsolated)
            throws RemoteException {
        Parcel data = Parcel.obtain();
        Parcel reply = Parcel.obtain();
        //是个常量是 “android.os.IServiceManager"
        data.writeInterfaceToken(IServiceManager.descriptor);
        data.writeString(name);
        data.writeStrongBinder(service);
        data.writeInt(allowIsolated ? 1 : 0);
        mRemote.transact(ADD_SERVICE_TRANSACTION, data, reply, 0);
        reply.recycle();
        data.recycle();
    }
```

里面代码都比较容易理解，这里重点说下**data.writeStrongBinder(service);** 和 **mRemote.transact(ADD\_SERVICE\_TRANSACTION, data, reply, 0);**

###### 2、Parcel.writeStrongBinder()

那我们就来看下Parcel的writeStrongBinder()方法

```
//frameworks/base/core/java/android/os/Parcel.java     583行
    /**
     * Write an object into the parcel at the current dataPosition(),
     * growing dataCapacity() if needed.
     */
    public final void writeStrongBinder(IBinder val) {
        nativeWriteStrongBinder(mNativePtr, val);
    }
```

先看下注释，翻译一下

> 在当前的dataPosition()的位置上写入一个对象，如果空间不足，则增加空间

通过上面代码我们知道 **writeStrongBinder()** 方法里面实际是调用的 **nativeWriteStrongBinder()** 方法，那我们来看下 \*\* nativeWriteStrongBinder()\*\* 方法

###### 2.1 nativeWriteStrongBinder()方法

```
/frameworks/base/core/java/android/os/Parcel.java      265行
    private static native void nativeWriteStrongBinder(long nativePtr, IBinder val);
```

我们知道了nativeWriteStrongBinder是一个native方法，那我们继续跟踪

###### 2.2 android\_os\_Parcel\_writeStrongBinder()函数

```
//frameworks/base/core/jni/android_os_Parcel.cpp    298行
static void android_os_Parcel_writeStrongBinder(JNIEnv* env, jclass clazz, jlong nativePtr, jobject object)
{
    //将java层Parcel转换为native层Parcel
    Parcel* parcel = reinterpret_cast<Parcel*>(nativePtr);
    if (parcel != NULL) {
        const status_t err = parcel->writeStrongBinder(ibinderForJavaObject(env, object));
        if (err != NO_ERROR) {
            signalExceptionForError(env, clazz, err);
        }
    }
}
```

这里主要涉及的两个重要的函数

-   writeStrongBinder()函数
-   ibinderForJavaObject()函数

那我们就来详细研究这两个函数

###### 2.2.1 ibinderForJavaObject()函数

```
// frameworks/base/core/jni/android_util_Binder.cpp   603行
sp<IBinder> ibinderForJavaObject(JNIEnv* env, jobject obj)
{
    if (obj == NULL) return NULL;

    //Java层的Binder对象
    //mClass指向Java层中的Binder class
    if (env->IsInstanceOf(obj, gBinderOffsets.mClass)) {
        JavaBBinderHolder* jbh = (JavaBBinderHolder*)
            env->GetLongField(obj, gBinderOffsets.mObject);
        //get()返回一个JavaBBinder，继承自BBinder
        return jbh != NULL ? jbh->get(env, obj) : NULL;
    }
    //Java层的BinderProxy对象
    // mClass 指向Java层的BinderProxy class
    if (env->IsInstanceOf(obj, gBinderProxyOffsets.mClass)) {
        //返回一个 BpBinder，mObject 是它的地址值
        return (IBinder*)env->GetLongField(obj, gBinderProxyOffsets.mObject);
    }
    return NULL;
}
```

根据Binder(Java)生成JavaBBinderHolder(C++)对象，主要工作是创建JavaBBinderHolder对象，并把JavaBBinder对象保存在到Binder.mObject成员变量。

-   这个函数，本质就是根据传进来的Java对象找到对应的C++对象，这里的obj可能会指向两种对象:Binder对象和BinderProxy对象。
-   如果传进来的是Binder对象，则会把gBinderOffsets.mObject转化为JavaBBinderHolder，并从中获得一个JavaBBinder对象(JavaBBinder继承自BBinder)。
-   如果是BinderProxy对象，会返回一个BpBinder，这个BpBinder的地址值保存在gBinderProxyOffsets.mObject中

在上面的代码里面调用了get()函数，如下图

```
JavaBBinderHolder* jbh = (JavaBBinderHolder*)
env->GetLongField(obj, gBinderOffsets.mObject);
//get()返回一个JavaBBinder，继承自BBinder
return jbh != NULL ? jbh->get(env, obj) : NULL;
```

那我们就来研究下JavaBBinderHolder.get()函数

###### 2.2.1.1 JavaBBinderHolder.get()函数

```
// frameworks/base/core/jni/android_util_Binder.cpp      316行
sp<JavaBBinder> get(JNIEnv* env, jobject obj)
{
    AutoMutex _l(mLock);
    sp<JavaBBinder> b = mBinder.promote();
    if (b == NULL) {
        //首次进来，创建JavaBBinder对象
        b = new JavaBBinder(env, obj);
        mBinder = b;
    }
    return b;
}
```

> JavaBBinderHolder有一个成员变量mBinder，保存当前创建的JavaBBinder对象，这是一个wp类型的，可能会被垃圾回收器给回收的，所以每次使用前都需要先判断是否存在。

那我们再来看看下JavaBBinder的初始化

###### 2.2.1.2 JavaBBinder的初始化

```
JavaBBinder(JNIEnv* env, jobject object)
    : mVM(jnienv_to_javavm(env)), mObject(env->NewGlobalRef(object))
{
    ALOGV("Creating JavaBBinder %p\n", this);
    android_atomic_inc(&gNumLocalRefs);
    incRefsCreated(env);
}
```

创建JavaBBinder，该对象继承于BBinder对象。

###### 2.2.1.3 总结

> 所以说 data.writeStrongBinder(Service)最终等价于parcel->writeStringBinder(new JavaBBinder(env, obj));

###### 2.2.2、writeStrongBinder() 函数

```
// frameworks/native/libs/binder/Parcel.cpp     872行
status_t Parcel::writeStrongBinder(const sp<IBinder>& val)
{
    return flatten_binder(ProcessState::self(), val, this);
}
```

我们看到writeStrongBinder()函数 实际上是调用的flatten\_binder()函数

###### 2.2.2.1、 writeStrongBinder() 函数

```
//frameworks/native/libs/binder/Parcel.cpp    205行
status_t flatten_binder(const sp<ProcessState>& /*proc*/,
    const sp<IBinder>& binder, Parcel* out)
{
    flat_binder_object obj;
    obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    if (binder != NULL) {
        IBinder *local = binder->localBinder();
        if (!local) {
            //如果不是本地Binder
            BpBinder *proxy = binder->remoteBinder();
            const int32_t handle = proxy ? proxy->handle() : 0;
            //远程Binder
            obj.type = BINDER_TYPE_HANDLE; 
            obj.binder = 0; 
            obj.handle = handle;
            obj.cookie = 0;
        } else {
            //如果是本地Binder
            obj.type = BINDER_TYPE_BINDER; 
            obj.binder = reinterpret_cast<uintptr_t>(local->getWeakRefs());
            obj.cookie = reinterpret_cast<uintptr_t>(local);
        }
    } else {
        //本地Binder
        obj.type = BINDER_TYPE_BINDER;  
        obj.binder = 0;
        obj.cookie = 0;
    }
    return finish_flatten_binder(binder, obj, out);
}
```

将Binder对象扁平化，转换成flat\_binder\_object对象

-   对于Binder实体，则cookie记录Binder实体指针
-   对于Binder代理，则用handle记录Binder代理的句柄

关于localBinder，在Binder.cpp里面

```
BBinder* BBinder::localBinder()
{
    return this;
}

BBinder* IBinder::localBinder()
{
    return NULL;
}
```

在最后面调用了finish\_flatten\_binder()函数，那我们再研究下finish\_flatten\_binder()函数

###### 2.2.2.2、 finish\_flatten\_binder() 函数

```
//frameworks/native/libs/binder/Parcel.cpp    199行
inline static status_t finish_flatten_binder(
    const sp<IBinder>& , const flat_binder_object& flat, Parcel* out)
{
    return out->writeObject(flat, false);
}
```

这个大家看明白了吧，就是写入一个object。

###### Parcel.writeStrongBinder()整个流程已经结束了。下面让我回来看下

ServiceManagerProxy的addService()中的mRemote.transact(ADD\_SERVICE\_TRANSACTION, data, reply, 0);

###### 3、IBinder.transact()

> ServiceManagerProxy的addService()中的mRemote.transact(ADD\_SERVICE\_TRANSACTION, data, reply, 0);里面的mRemote的类型是BinderProxy的，所以调用是BinderProxy的transact()方法，那我们就进去看看

###### 3.1、BinderProxy.transact()

###### 温馨提示：BinderProxy类是Binder类的内部类

他其实是重写的IBinder的里面的transact()方法，那让我们看下IBinder里面

```
// frameworks/base/core/java/android/os/IBinder.java  223行
    /**
     * Perform a generic operation with the object.
     * 
     * @param code The action to perform.  This should
     * be a number between {@link #FIRST_CALL_TRANSACTION} and
     * {@link #LAST_CALL_TRANSACTION}.
     * @param data Marshalled data to send to the target.  Must not be null.
     * If you are not sending any data, you must create an empty Parcel
     * that is given here.
     * @param reply Marshalled data to be received from the target.  May be
     * null if you are not interested in the return value.
     * @param flags Additional operation flags.  Either 0 for a normal
     * RPC, or {@link #FLAG_ONEWAY} for a one-way RPC.
     */
    public boolean transact(int code, Parcel data, Parcel reply, int flags)
        throws RemoteException;
```

其实让大家看这个主要是向大家说下这个注释，(_\_\__) 嘻嘻…… 翻译一下

-   用对象执行一个操作
-   参数code 为操作码，是介于FIRST\_CALL\_TRANSACTION和LAST\_CALL\_TRANSACTION之间
-   参数data 是要发往目标的数据，一定不能null，如果你没有数据要发送，你也要创建一个Parcel，哪怕是空的。
-   参数reply 是从目标发过来的数据，如果你对这个数据没兴趣，这个数据是可以为null的。
-   参数flags 一个操作标志位，要么是0代表普通的RPC，要么是FLAG\_ONEWAY代表单一方向的RPC即不管返回值

这时候我们再回来看

```
/frameworks/base/core/java/android/os/Binder.java   501行
final class BinderProxy implements IBinder {
    public boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
        Binder.checkParcel(this, code, data, "Unreasonably large binder buffer");
        if (Binder.isTracingEnabled()) { Binder.getTransactionTracker().addTrace(); }
        return transactNative(code, data, reply, flags);
    }
}
```

先来看下 Binder.checkParcel方法

###### 3.1.1、Binder.checkParcel()

```
/frameworks/base/core/java/android/os/Binder.java   415行
    static void checkParcel(IBinder obj, int code, Parcel parcel, String msg) {
        if (CHECK_PARCEL_SIZE && parcel.dataSize() >= 800*1024) {
            // Trying to send > 800k, this is way too much
            StringBuilder sb = new StringBuilder();
            sb.append(msg);
            sb.append(": on ");
            sb.append(obj);
            sb.append(" calling ");
            sb.append(code);
            sb.append(" size ");
            sb.append(parcel.dataSize());
            sb.append(" (data: ");
            parcel.setDataPosition(0);
            sb.append(parcel.readInt());
            sb.append(", ");
            sb.append(parcel.readInt());
            sb.append(", ");
            sb.append(parcel.readInt());
            sb.append(")");
            Slog.wtfStack(TAG, sb.toString());
        }
    }
```

这段代码很简单，主要是检查Parcel大小是否大于800K。

执行完Binder.checkParcel后，直接调用了transactNative()方法，那我们就来看看transactNative()方法

###### 3.1.2、transactNative()方法

```
// frameworks/base/core/java/android/os/Binder.java   507行
    public native boolean transactNative(int code, Parcel data, Parcel reply,
            int flags) throws RemoteException;
```

我们看到他是一个native函数，后面肯定经过JNI调用到了native层，根据包名，它对应的方法应该是"android\_os\_BinderProxy\_transact"函数，那我们继续跟踪

###### 3.1.3、android\_os\_BinderProxy\_transact()函数

```
// frameworks/base/core/jni/android_util_Binder.cpp     1083行
   static jboolean android_os_BinderProxy_transact(JNIEnv*env, jobject obj,
                                                    jint code, jobject dataObj, jobject replyObj, jint flags) // throws RemoteException
    {
        if (dataObj == NULL) {
            jniThrowNullPointerException(env, NULL);
            return JNI_FALSE;
        }
        // 将 java Parcel转化为native Parcel
        Parcel * data = parcelForJavaObject(env, dataObj);
        if (data == NULL) {
            return JNI_FALSE;
        }
        Parcel * reply = parcelForJavaObject(env, replyObj);
        if (reply == NULL && replyObj != NULL) {
            return JNI_FALSE;
        }
        // gBinderProxyOffsets.mObject中保存的是new BpBinder(0)对象
        IBinder * target = (IBinder *)
        env -> GetLongField(obj, gBinderProxyOffsets.mObject);
        if (target == NULL) {
            jniThrowException(env, "java/lang/IllegalStateException", "Binder has been finalized!");
            return JNI_FALSE;
        }

        ALOGV("Java code calling transact on %p in Java object %p with code %"PRId32"\n",
                target, obj, code);


        bool time_binder_calls;
        int64_t start_millis;
        if (kEnableBinderSample) {
            // Only log the binder call duration for things on the Java-level main thread.
            // But if we don't
            time_binder_calls = should_time_binder_calls();

            if (time_binder_calls) {
                start_millis = uptimeMillis();
            }
        }

        //printf("Transact from Java code to %p sending: ", target); data->print();
         // 此处便是BpBinder:: transact()，经过native层，进入Binder驱动。
        status_t err = target -> transact(code, * data, reply, flags);
        //if (reply) printf("Transact from Java code to %p received: ", target); reply->print();

        if (kEnableBinderSample) {
            if (time_binder_calls) {
                conditionally_log_binder_call(start_millis, target, code);
            }
        }

        if (err == NO_ERROR) {
            return JNI_TRUE;
        } else if (err == UNKNOWN_TRANSACTION) {
            return JNI_FALSE;
        }
        signalExceptionForError(env, obj, err, true /*canThrowRemoteException*/, data -> dataSize());
        return JNI_FALSE;
    }
```

通过上面的代码我们知道，Java层BinderProxy.transact()最终交由Native层的BpBinder::transact()完成。这部分之前代码讲解过了，我这里就不详细说明了。不过注意，该方法可能会抛出RemoteException。

##### (二)、总结

所以 整个 addService的核心可以缩写为向下面的代码

```
public void addService(String name, IBinder service, boolean allowIsolated)
        throws RemoteException {
    ...
     //此处还需要将Java层的Parcel转化为Native层的Parcel
    Parcel data = Parcel.obtain();
    data->writeStrongBinder(new JavaBBinder(env, obj));
    //与Binder驱动交互
    BpBinder::transact(ADD_SERVICE_TRANSACTION, *data, reply, 0);
    ...
}
```

说白了，注册服务过程就是通过BpBinder来发送ADD\_SERVICE\_TRANSACTION命令，与binder驱动进行数据交互。

### 四、获取服务

##### (一)、ServiceManager.getService()方法

```
//frameworks/base/core/java/android/os/ServiceManager.java    49行
public static IBinder getService(String name) {
    try {
        //先从缓存中查看
        IBinder service = sCache.get(name); 
        if (service != null) {
            return service;
        } else {
            return getIServiceManager().getService(name); 
        }
    } catch (RemoteException e) {
        Log.e(TAG, "error in getService", e);
    }
    return null;
}
```

-   1、先从缓存中取出，如果有，则直接return。其中sCache是以HashMap格式的缓存
-   2、如果没有调用getIServiceManager().getService(name)获取一个，并且return 通过前面的内容我们知道

等价于

```
new  ServiceManagerProxy(new BinderProxy())
```

那我们来下ServiceManagerProxy的getService()方法

###### 1、ServiceManagerProxy.getService(name)

```
// frameworks/base/core/java/android/os/ServiceManagerNative.java     118行
    public IBinder getService(String name) throws RemoteException {
        Parcel data = Parcel.obtain();
        Parcel reply = Parcel.obtain();
        data.writeInterfaceToken(IServiceManager.descriptor);
        data.writeString(name);
        //mRemote为BinderProxy
        mRemote.transact(GET_SERVICE_TRANSACTION, data, reply, 0);
        //从replay里面解析出获取的IBinder对象
        IBinder binder = reply.readStrongBinder();
        reply.recycle();
        data.recycle();
        return binder;
    }
```

这里面有两个重点方法，一个是 mRemote.transact(),一个是 reply.readStrongBinder()。那我们就逐步研究下

###### 2、mRemote.transact()方法

我们mRemote其实是BinderPoxy，那我们来看下BinderProxy的transact方法

```
     //frameworks/base/core/java/android/os/Binder.java   501行
    public boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
        Binder.checkParcel(this, code, data, "Unreasonably large binder buffer");
        if (Binder.isTracingEnabled()) { Binder.getTransactionTracker().addTrace(); }
        return transactNative(code, data, reply, flags);
    }

     // frameworks/base/core/java/android/os/Binder.java   507行
    public native boolean transactNative(int code, Parcel data, Parcel reply,
            int flags) throws RemoteException;
```

关于Binder.checkParcel()方法，上面已经说过了，就不详细说了。transact()方法其实是调用了natvie的transactNative()方法，这样就进入了JNI里面了

###### 2.1、mRemote.transact()方法

```
// frameworks/base/core/jni/android_util_Binder.cpp     1083行
static jboolean android_os_BinderProxy_transact(JNIEnv* env, jobject obj,
        jint code, jobject dataObj, jobject replyObj, jint flags) // throws RemoteException
{
    if (dataObj == NULL) {
        jniThrowNullPointerException(env, NULL);
        return JNI_FALSE;
    }
    //Java的 Parcel 转为native的 Parcel
    Parcel* data = parcelForJavaObject(env, dataObj);
    if (data == NULL) {
        return JNI_FALSE;
    }
    Parcel* reply = parcelForJavaObject(env, replyObj);
    if (reply == NULL && replyObj != NULL) {
        return JNI_FALSE;
    }
    // gBinderProxyOffsets.mObject中保存的的是new BpBinder(0)对象
    IBinder* target = (IBinder*)
        env->GetLongField(obj, gBinderProxyOffsets.mObject);
    if (target == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", "Binder has been finalized!");
        return JNI_FALSE;
    }

    ALOGV("Java code calling transact on %p in Java object %p with code %" PRId32 "\n",
            target, obj, code);

    bool time_binder_calls;
    int64_t start_millis;
    if (kEnableBinderSample) {
        // Only log the binder call duration for things on the Java-level main thread.
        // But if we don't
        time_binder_calls = should_time_binder_calls();

        if (time_binder_calls) {
            start_millis = uptimeMillis();
       }
    }

    //printf("Transact from Java code to %p sending: ", target); data->print();
    //gBinderProxyOffseets.mObject中保存的是new BpBinder(0) 对象
    status_t err = target->transact(code, *data, reply, flags);
    //if (reply) printf("Transact from Java code to %p received: ", target); reply->print();

    if (kEnableBinderSample) {
        if (time_binder_calls) {
            conditionally_log_binder_call(start_millis, target, code);
        }
    }

    if (err == NO_ERROR) {
        return JNI_TRUE;
    } else if (err == UNKNOWN_TRANSACTION) {
        return JNI_FALSE;
    }

    signalExceptionForError(env, obj, err, true /*canThrowRemoteException*/, data->dataSize());
    return JNI_FALSE;
}
```

上面代码中，有一段重点代码

```
status_t err = target->transact(code, *data, reply, flags);
```

现在 我们看一下他里面的事情

###### 2.2、BpBinder::transact()函数

```
/frameworks/native/libs/binder/BpBinder.cpp    159行
status_t BpBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    if (mAlive) {
        status_t status = IPCThreadState::self()->transact(
            mHandle, code, data, reply, flags);
        if (status == DEAD_OBJECT) mAlive = 0;
        return status;
    }
    return DEAD_OBJECT;
}
```

其实是调用的IPCThreadState的transact()函数

###### 2.3、BpBinder::transact()函数

```
//frameworks/native/libs/binder/IPCThreadState.cpp    548行
status_t IPCThreadState::transact(int32_t handle,
                                  uint32_t code, const Parcel& data,
                                  Parcel* reply, uint32_t flags)
{
    status_t err = data.errorCheck(); //数据错误检查
    flags |= TF_ACCEPT_FDS;
    ....
    if (err == NO_ERROR) {
         // 传输数据
        err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, NULL);
    }
    ...

    // 默认情况下,都是采用非oneway的方式, 也就是需要等待服务端的返回结果
    if ((flags & TF_ONE_WAY) == 0) {
        if (reply) {
            //等待回应事件
            err = waitForResponse(reply);
        }else {
            Parcel fakeReply;
            err = waitForResponse(&fakeReply);
        }
    } else {
        err = waitForResponse(NULL, NULL);
    }
    return err;
}
```

主要就是两个步骤

-   首先，调用writeTransactionData()函数 传输数据
-   其次，调用waitForResponse()函数来获取返回结果

那我们来看下waitForResponse()函数里面的重点实现

###### 2.4、IPCThreadState::waitForResponse函数

```
//frameworks/native/libs/binder/IPCThreadState.cpp    712行
status_t IPCThreadState::waitForResponse(Parcel *reply, status_t *acquireResult)
{
    int32_t cmd;
    int32_t err;
    while (1) {
        if ((err=talkWithDriver()) < NO_ERROR) break; 
        ...
        cmd = mIn.readInt32();
        switch (cmd) {
          case BR_REPLY:
          {
            binder_transaction_data tr;
            err = mIn.read(&tr, sizeof(tr));
            if (reply) {
                if ((tr.flags & TF_STATUS_CODE) == 0) {
                    //当reply对象回收时，则会调用freeBuffer来回收内存
                    reply->ipcSetDataReference(
                        reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                        tr.data_size,
                        reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                        tr.offsets_size/sizeof(binder_size_t),
                        freeBuffer, this);
                } else {
                    ...
                }
            }
          }
          case :...
        }
    }
    ...
    return err;
}
```

这时候就在等待回复了，如果有回复，则通过cmd = mIn.readInt32()函数获取命令

###### 2.5、IPCThreadState::waitForResponse函数

```
//
void binder_send_reply(struct binder_state *bs,
                       struct binder_io *reply,
                       binder_uintptr_t buffer_to_free,
                       int status)
{
    struct {
        uint32_t cmd_free;
        binder_uintptr_t buffer;
        uint32_t cmd_reply;
        struct binder_transaction_data txn;
    } __attribute__((packed)) data;
    //free buffer命令
    data.cmd_free = BC_FREE_BUFFER; 
    data.buffer = buffer_to_free;
    // reply命令
    data.cmd_reply = BC_REPLY; // reply命令
    data.txn.target.ptr = 0;
    data.txn.cookie = 0;
    data.txn.code = 0;
    if (status) {
        ...
    } else {=
    
        data.txn.flags = 0;
        data.txn.data_size = reply->data - reply->data0;
        data.txn.offsets_size = ((char*) reply->offs) - ((char*) reply->offs0);
        data.txn.data.ptr.buffer = (uintptr_t)reply->data0;
        data.txn.data.ptr.offsets = (uintptr_t)reply->offs0;
    }
    //向Binder驱动通信
    binder_write(bs, &data, sizeof(data));
}
```

binder\_write将BC\_FREE\_BUFFER和BC\_REPLY命令协议发送给驱动，进入驱动。 在驱动里面bingder\_ioctl -> binder\_ioctl\_write\_read ->binder\_thread\_write，由于是BC\_REPLY命令协议，则进入binder\_transaction，该方法会向请求服务的线程TODO队列插入事务。接来下，请求服务的进程在执行talkWithDriver的过程执行到binder\_thread\_read()，处理TODO队列的事物。

###### 3、Parcel.readStrongBinder()方法

其实Parcel.readStrongBinder()的过程基本上就是writeStrongBinder的过程。 我们先来看下它的源码

```
//frameworks/base/core/java/android/os/Parcel.java    1686行
    /**
     * Read an object from the parcel at the current dataPosition().
     * 在当前的 dataPosition()位置上读取一个对象
     */
    public final IBinder readStrongBinder() {
        return nativeReadStrongBinder(mNativePtr);
    }


  private static native IBinder nativeReadStrongBinder(long nativePtr);
```

其实它内部是调用的是nativeReadStrongBinder()方法，通过上面的源码我们知道nativeReadStrongBinder是一个native的方法，所以通过JNI调用到android\_os\_Parcel\_readStrongBinder这个函数

###### 3.1、android\_os\_Parcel\_readStrongBinder()函数

```
//frameworks/base/core/jni/android_os_Parcel.cpp          429行
static jobject android_os_Parcel_readStrongBinder(JNIEnv* env, jclass clazz, jlong nativePtr)
{
    Parcel* parcel = reinterpret_cast<Parcel*>(nativePtr);
    if (parcel != NULL) {
        return javaObjectForIBinder(env, parcel->readStrongBinder());
    }
    return NULL;
}
```

javaObjectForIBinder将native层BpBinder对象转换为Java层的BinderProxy对象。 上面的函数中，调用了readStrongBinder()函数

###### 3.2、readStrongBinder()函数

```
//frameworks/native/libs/binder/Parcel.cpp  1334行
sp<IBinder> Parcel::readStrongBinder() const
{
    sp<IBinder> val;
    unflatten_binder(ProcessState::self(), *this, &val);
    return val;
}
```

这里面也很简单，主要是调用unflatten\_binder()函数

###### 3.3、unflatten\_binder()函数

```
//frameworks/native/libs/binder/Parcel.cpp  293行
status_t unflatten_binder(const sp<ProcessState>& proc,
    const Parcel& in, sp<IBinder>* out)
{
    const flat_binder_object* flat = in.readObject(false);
    if (flat) {
        switch (flat->type) {
            case BINDER_TYPE_BINDER:
                *out = reinterpret_cast<IBinder*>(flat->cookie);
                return finish_unflatten_binder(NULL, *flat, in);
            case BINDER_TYPE_HANDLE:
                //进入该分支
                *out = proc->getStrongProxyForHandle(flat->handle);
                //创建BpBinder对象
                return finish_unflatten_binder(
                    static_cast<BpBinder*>(out->get()), *flat, in);
        }
    }
    return BAD_TYPE;
}
```

PS:在/frameworks/native/libs/binder/Parcel.cpp/frameworks/native/libs/binder/Parcel.cpp 里面有两个unflatten\_binder()函数，其中区别点是，最后一个入参，一个是sp<IBinder>\* out，另一个是wp<IBinder>\* out。大家别弄差了。

在unflatten\_binder里面进入 case BINDER\_TYPE\_HANDLE: 分支，然后执行getStrongProxyForHandle()函数。

###### 3.4、getStrongProxyForHandle()函数

```
sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);
    //查找handle对应的资源项
    handle_entry* e = lookupHandleLocked(handle);

    if (e != NULL) {
        IBinder* b = e->binder;
        if (b == NULL || !e->refs->attemptIncWeak(this)) {
            ...
            //当handle值所对应的IBinder不存在或弱引用无效时，则创建BpBinder对象
            b = new BpBinder(handle);
            e->binder = b;
            if (b) e->refs = b->getWeakRefs();
            result = b;
        } else {
            result.force_set(b);
            e->refs->decWeak(this);
        }
    }
    return result;
}
```

经过该方法，最终创建了指向Binder服务端的BpBinder代理对象。所以说javaObjectForIBinder将native层的BpBinder对象转化为Java层BinderProxy对象。也就是说通过getService()最终取得了指向目标Binder[服务器](https://cloud.tencent.com/product/cvm/?from_column=20065&from=20065)的代理对象BinderProxy。

###### 4、总结

所以说getService的核心过程：

```
public static IBinder getService(String name) {
    ...
    //此处还需要将Java层的Parcel转化为Native层的Parcel
    Parcel reply = Parcel.obtain(); 
    // 与Binder驱动交互
    BpBinder::transact(GET_SERVICE_TRANSACTION, *data, reply, 0);  
    IBinder binder = javaObjectForIBinder(env, new BpBinder(handle));
    ...
}
```

javaObjectForIBinder作用是创建BinderProxy对象，并将BpBinder对象的地址保存到BinderProxy对象的mObjects中，获取服务过程就是通过BpBinder来发送GET\_SERVICE\_TRANSACTION命令，实现与binder驱动进行数据交互。