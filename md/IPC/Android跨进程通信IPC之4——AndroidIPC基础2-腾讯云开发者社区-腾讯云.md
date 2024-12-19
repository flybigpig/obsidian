###### 8 、Binder对象的写入

> 前面说完了基本的数据传输流程，心里有了一个大致的流程，再来看一下Binder对象的传输。首先需要对Binder有一个概念，就是每一个java端的Binder对象(服务端)在初始化时都会对应一个native对象，类型是BBinder，它继承于IBinder类

时序图如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/8hecbfyatq.jpeg)

写入Binder对象.jpeg

通过 Parcel的writeStrongBinder方法将Binder对象序列化:

###### (1)、Parcel.writeStrongBinder(IBinder)

```
//Parcel.java
    /**
     * Write an object into the parcel at the current dataPosition(),
     * growing dataCapacity() if needed.
     */
    public final void writeStrongBinder(IBinder val) {
        nativeWriteStrongBinder(mNativePtr, val);
    }

    private static native void nativeWriteStrongBinder(long nativePtr, IBinder val);
```

我们看到writeStrongBinder(IBinder)内部是调用了native的方法nativeWriteStrongBinder(long,IBinder),这个方法对应JNI的android\_os\_Parcel\_writeStrongBinder()函数

###### (2)、android\_os\_Parcel\_writeStrongBinder(JNIEnv\* env, jclass clazz, jlong nativePtr, jobject object)

代码在[android\_os\_Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_os_Parcel.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 298行

```
static void android_os_Parcel_writeStrongBinder(JNIEnv* env, jclass clazz, jlong nativePtr, jobject object)
{
    Parcel* parcel = reinterpret_cast<Parcel*>(nativePtr);
    if (parcel != NULL) {
        const status_t err = parcel->writeStrongBinder(ibinderForJavaObject(env, object));
        if (err != NO_ERROR) {
            signalExceptionForError(env, clazz, err);
        }
    }
}
```

这里说下ibinderForJavaObject()函数，返回的是BBinder对象(实际上是JavaBBinder，它继承自BBinder) 后面讲解Binder的时候再详细说

然后调用了Parcel-Native的writeStrongBinder函数

###### (3)、Parcel::writeStrongBinder

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 872行

```
status_t Parcel::writeStrongBinder(const sp<IBinder>& val)
{
    return flatten_binder(ProcessState::self(), val, this);
}
```

这块代码很简单，主要是调用了flatten\_binder()函数 这里说一下ProcessState::self() 是获取ProcessState的单例方法

###### (4)、Parcel::flatten\_binder(const sp<ProcessState>& /_proc_/, const sp<IBinder>& binder, Parcel\* out)

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 205行

```
status_t flatten_binder(const sp<ProcessState>& /*proc*/,
    const sp<IBinder>& binder, Parcel* out)
{
    flat_binder_object obj;
    obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    if (binder != NULL) {
        //JavaBBinder返回的是this,也就是自己 
        IBinder *local = binder->localBinder();
        //不是本地进程，即跨进程
        if (!local) {
            //分支一，如果local为空
            BpBinder *proxy = binder->remoteBinder();
            if (proxy == NULL) {
                ALOGE("null proxy");
            }
            const int32_t handle = proxy ? proxy->handle() : 0;
            obj.type = BINDER_TYPE_HANDLE;
            obj.binder = 0; /* Don't pass uninitialized stack data to a remote process */
            obj.handle = handle;
            obj.cookie = 0;
        } else {
            //分支二，local不为空
            //写入JavaBBinder将对应这一段
            obj.type = BINDER_TYPE_BINDER;
             // 弱引用对象
            obj.binder = reinterpret_cast<uintptr_t>(local->getWeakRefs());
            //  this 对象，BBinder本身
            obj.cookie = reinterpret_cast<uintptr_t>(local);
        }
    } else {
        obj.type = BINDER_TYPE_BINDER;
        obj.binder = 0;
        obj.cookie = 0;
    }
    return finish_flatten_binder(binder, obj, out);
}
```

这里主要是分别是本地Binder还是远程Binder，对两种Binder采取了两种不同的方式。

-   Binder如果是JavaBBinder，则它的localBinder会返回localBinder。
-   Binder如果是BpBinder，则它的localBinder会返回null

通过上文我们知道ibinderForJavaObject就返回JavaBBinder。所以我们知道走入分支二

flat\_binder\_object是Binder写入对象的结构体，它对应着Binder。

-   flat\_binder\_object的handle表示Binder对象在Binder驱动中的标志，比如ServiceManager的handle为0。
-   flat\_binder\_object的type表示当前传输的Binder是本地的(同进程)，还是一个Proxy(跨进程)。

通过上面代码我们知道这里取得的flat\_binder\_object对应的值如下

-   type为BINDER\_TYPE\_BINDER
-   binder为reinterpret\_cast(local->getWeakRefs());
-   cookie为reinterpret\_cast(local)

binder，cookie保存着Binder对象的指针。最后调用了finish\_flatten\_binder()函数

###### (5)、Parcel::finish\_flatten\_binder()函数

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 199行

```
inline static status_t finish_flatten_binder(
    const sp<IBinder>& /*binder*/, const flat_binder_object& flat, Parcel* out)
{
    return out->writeObject(flat, false);
}
```

finish\_flatten\_binder()函数主要是调用writeObject()函数将flat\_binder\_object写入到out里面里面，最终写入到Binder驱动中，那我们继续跟踪writeObject()函数。

###### (6)、Parcel::writeObject()函数

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 1035行

```
status_t Parcel::writeObject(const flat_binder_object& val, bool nullMetaData)
{
    const bool enoughData = (mDataPos+sizeof(val)) <= mDataCapacity;
    const bool enoughObjects = mObjectsSize < mObjectsCapacity;
    //分支一
    if (enoughData && enoughObjects) {
restart_write:
         // mObjects 数据写入
        *reinterpret_cast<flat_binder_object*>(mData+mDataPos) = val;

        // remember if it's a file descriptor
        if (val.type == BINDER_TYPE_FD) {
            if (!mAllowFds) {
                // fail before modifying our object index
                return FDS_NOT_ALLOWED;
            }
            mHasFds = mFdsKnown = true;
        }

        // Need to write meta-data?
        if (nullMetaData || val.binder != 0) {
            mObjects[mObjectsSize] = mDataPos;
            acquire_object(ProcessState::self(), val, this, &mOpenAshmemSize);
            mObjectsSize++;
        }

        return finishWrite(sizeof(flat_binder_object));
    }
    //分支二
    if (!enoughData) {
        const status_t err = growData(sizeof(val));
        if (err != NO_ERROR) return err;
    }
    //分支三
    if (!enoughObjects) {
        size_t newSize = ((mObjectsSize+2)*3)/2;
        if (newSize < mObjectsSize) return NO_MEMORY;   // overflow
        binder_size_t* objects = (binder_size_t*)realloc(mObjects, newSize*sizeof(binder_size_t));
        if (objects == NULL) return NO_MEMORY;
        mObjects = objects;
        mObjectsCapacity = newSize;
    }

    goto restart_write;
}
```

finish\_flatten\_binder()函数主要是调用writeObject()函数

这里面有三个分支，我们就来依次说下

-   如果mData和mObjects空间足够，则走分支一
-   如果mData空间不足，则扩展空间，growData()函数看前面
-   如果mObjects空间不足，则扩展空间，和mData空间扩展基本相似

调用 \* reinterpret\_cast<flat\_binder\_object\*>(mData+mDataPos) = val; 写入mObject

最后调用finishWrite()函数，就是调整调整 mDataPos 和 mDataSize，上面已经说过了，我就跟踪了。至此整体写入流程已经完成了。

###### 9 、Binder对象的读出

Parcel对象的读出，首先还是在Parcel.java这个类里面，对应的方法是

时序图如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/i8vxcsjase.png)

读出Binder对象.png

###### 9.1 Parcel.readStrongBinder()方法

```
    /**
     * Read an object from the parcel at the current dataPosition().
     */
    public final IBinder readStrongBinder() {
        return nativeReadStrongBinder(mNativePtr);
    }
```

我们看到readStrongBinder()方法内部调用了native的nativeReadStrongBinder()方法，

###### 9.2 Parcel.nativeReadStrongBinder()方法

```
    private static native IBinder nativeReadStrongBinder(long nativePtr);
```

而这个native方法又对应这个JNI的一个方法，通过上文我们知道，对应的是 **/frameworks/base/core/jni/android\_os\_Parcel.cpp的android\_os\_Parcel\_readStrongBinder()函数**

###### 9.3 android\_os\_Parcel\_readStrongBinder()函数

代码在[android\_os\_Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_os_Parcel.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 429行

```
static jobject android_os_Parcel_readStrongBinder(JNIEnv* env, jclass clazz, jlong nativePtr)
{
    Parcel* parcel = reinterpret_cast<Parcel*>(nativePtr);
    if (parcel != NULL) {
        return javaObjectForIBinder(env, parcel->readStrongBinder());
    }
    return NULL;
}
```

这个函数里面先调用了Parcel-Native的readStrongBinder()函数，然后又用这个函数的返回值作为参数调用了javaObjectForIBinder()函数。那我们就依次来看一下。

###### 9.4 readStrongBinder()函数

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 1134行

```
sp<IBinder> Parcel::readStrongBinder() const
{
    sp<IBinder> val;
    unflatten_binder(ProcessState::self(), *this, &val);
    return val;
}
```

我们看到这个函数什么也没做，主要就是调用了unflatten\_binder()函数。

readStrongBinder 其实挺简单的，是本地的可以直接用，远程的那个 getStrongProxyForHandle 也是放到后面 ServiceManager 再细说。到这里目标进程就收到原始进程传递过来的 binder 对象了，然后可以转化为 binder 的 interface 调用对应的 IPC 接口。

> PS: 这里说下，ProcessState::self()函数是返回的ProcessState的对象

那我们再来看下unflatten\_binder()函数

###### 9.5 unflatten\_binder()函数

代码在[android\_os\_Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_os_Parcel.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 293行

```
status_t unflatten_binder(const sp<ProcessState>& proc,
    const Parcel& in, sp<IBinder>* out)
{
    const flat_binder_object* flat = in.readObject(false);

    if (flat) {
        switch (flat->type) {
            case BINDER_TYPE_BINDER:
                //如果是Bn的话，本地直接强转
                *out = reinterpret_cast<IBinder*>(flat->cookie);
                return finish_unflatten_binder(NULL, *flat, in);
            case BINDER_TYPE_HANDLE:
                //如果是Bp的话，要通过handle构造一个远程的代理对象
                *out = proc->getStrongProxyForHandle(flat->handle);
                return finish_unflatten_binder(
                    static_cast<BpBinder*>(out->get()), *flat, in);
        }
    }
    return BAD_TYPE;
}
```

这里说下这里的两个分支

-   BINDER\_TYPE\_BINDER分支：是Bn意味着是同一个进程
-   BINDER\_TYPE\_HANDLE分支：是Bp意味着是跨进程

我们再来分下一下这个函数，主要就是两个两个流程

-   1、从Binder驱动中读取一个flat\_binder\_object对象flat
-   2、根据flat对象的type值来分别处理。如果是BINDER\_TYPE\_BINDER，则使用cookie中的值，强制转换成指针。如果是BINDER\_TYPE\_HANDLE，则使用Proxy，即通过getStringProxyForHandle()函数，并根据handle创建BpBinder。

ProcessState::getStrongProxyForHandle()函数后续讲解Binder的时候再详细讲解，这里就不说了。那我们来看一下finish\_unflatten\_binder()

###### 9.6 finish\_unflatten\_binder()函数

代码在[android\_os\_Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_os_Parcel.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 286行

```
inline static status_t finish_unflatten_binder(
    BpBinder* /*proxy*/, const flat_binder_object& /*flat*/,
    const Parcel& /*in*/)
{
    return NO_ERROR;
}
```

这行代码很简单，就是返回NO\_ERROR。这时候我们回到了javaObjectForIBinder()函数。

###### 9.7 javaObjectForIBinder()函数

javaObjectgForBinder与ibinderForJavaObject相对应的，把IBinder对象转换成对应的Java层的Object。这个函数是关键。 代码在[android\_os\_Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 547行

```
jobject javaObjectForIBinder(JNIEnv* env, const sp<IBinder>& val)
{
    if (val == NULL) return NULL; 
         // One of our own!
    if (val->checkSubclass(&gBinderOffsets)) {
          //如果是本地的，那么会直接进入这个部分代码，因为这个val
         //是写入的时候的同一个对象，gBinderOffsets也是一致的。如
         //果val是一种Poxy对象，则不然，会继续往下执行，找到一个
         //Proxy对象
        // One of our own!
        jobject object = static_cast<JavaBBinder*>(val.get())->object();
        LOGDEATH("objectForBinder %p: it's our own %p!\n", val.get(), object);
        return object;
    }
    // For the rest of the function we will hold this lock, to serialize
    // looking/creation of Java proxies for native Binder proxies.
    AutoMutex _l(mProxyLock);
    // Someone else's...  do we know about it?
     // BpBinder没有带proxy过来
    jobject object = (jobject)val->findObject(&gBinderProxyOffsets);
    if (object != NULL) {
        jobject res = jniGetReferent(env, object);
        if (res != NULL) {
            ALOGV("objectForBinder %p: found existing %p!\n", val.get(), res);
            return res;
        }
        LOGDEATH("Proxy object %p of IBinder %p no longer in working set!!!", object, val.get());
        android_atomic_dec(&gNumProxyRefs);
        val->detachObject(&gBinderProxyOffsets);
        env->DeleteGlobalRef(object);
    }
      // 创建一个proxy  
    object = env->NewObject(gBinderProxyOffsets.mClass, gBinderProxyOffsets.mConstructor);
    if (object != NULL) {
        // 给object的相关字段赋值
        LOGDEATH("objectForBinder %p: created new proxy %p !\n", val.get(), object);
        // The proxy holds a reference to the native object.
         // 把BpBinder(0) 赋值给BinderProxy的mObject
        env->SetLongField(object, gBinderProxyOffsets.mObject, (jlong)val.get());
        val->incStrong((void*)javaObjectForIBinder);
        // The native object needs to hold a weak reference back to the
        // proxy, so we can retrieve the same proxy if it is still active.
        jobject refObject = env->NewGlobalRef(
                env->GetObjectField(object, gBinderProxyOffsets.mSelf));
        val->attachObject(&gBinderProxyOffsets, refObject,
                jnienv_to_javavm(env), proxy_cleanup);
        // Also remember the death recipients registered on this proxy
        sp<DeathRecipientList> drl = new DeathRecipientList;
        drl->incStrong((void*)javaObjectForIBinder);
        env->SetLongField(object, gBinderProxyOffsets.mOrgue, reinterpret_cast<jlong>(drl.get()));
        // Note that a new object reference has been created.
        android_atomic_inc(&gNumProxyRefs);
        incRefsCreated(env);
    }
    return object;
}
```

-   首先判断是不是同一个进程，如果是同一个进程，则val是JavaBBinder。那么在checkSubclass()函数中，它所包含的gBinderOffsets指针参数传入的gBinderOffsets的指针必然是同一个值，则满足if条件，直接将指针强制转化为JavaBBinder，返回对应的jobject。如果是不是同一个进程，那么val也就是BpBinder。
-   其次，在BpBinder对象中查找是否保存相关的BinderProxy的对象，如果有，向Java层返回这个对象。如果没有，则创建一个BinderProxy对象，并将新创建的BinderProxy对象，attach到BpBinder对象中。

在构造Java对象的时候，上面用到了 binderproxy\_offsets\_t 结构体 ，那我们就来看下这个结构体

###### 9.7.1 binderproxy\_offsets\_t 结构体

代码在[android\_os\_Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 95行

```
static struct binderproxy_offsets_t
{
    // Class state.
    jclass mClass;
    jmethodID mConstructor;
    jmethodID mSendDeathNotice;

    // Object state.
    jfieldID mObject;
    jfieldID mSelf;
    jfieldID mOrgue;

} gBinderProxyOffsets;
```

gBinderProxyOffsets的初始化是在虚拟机启动的时候（即在 AndroidRuntime::start），最终赋值是在[android\_os\_Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 的中1254行的函数int\_register\_android\_os\_BinderProxy()中，代码如下

```
static int int_register_android_os_BinderProxy(JNIEnv* env)
{
    jclass clazz = FindClassOrDie(env, "java/lang/Error");
1257    gErrorOffsets.mClass = MakeGlobalRefOrDie(env, clazz);

    clazz = FindClassOrDie(env, kBinderProxyPathName);
    gBinderProxyOffsets.mClass = MakeGlobalRefOrDie(env, clazz);
    gBinderProxyOffsets.mConstructor = GetMethodIDOrDie(env, clazz, "<init>", "()V");
    gBinderProxyOffsets.mSendDeathNotice = GetStaticMethodIDOrDie(env, clazz, "sendDeathNotice",
            "(Landroid/os/IBinder$DeathRecipient;)V");

    gBinderProxyOffsets.mObject = GetFieldIDOrDie(env, clazz, "mObject", "J");
    gBinderProxyOffsets.mSelf = GetFieldIDOrDie(env, clazz, "mSelf",
                                                "Ljava/lang/ref/WeakReference;");
    gBinderProxyOffsets.mOrgue = GetFieldIDOrDie(env, clazz, "mOrgue", "J");

    clazz = FindClassOrDie(env, "java/lang/Class");
    gClassOffsets.mGetName = GetMethodIDOrDie(env, clazz, "getName", "()Ljava/lang/String;");

    return RegisterMethodsOrDie(
        env, kBinderProxyPathName,
        gBinderProxyMethods, NELEM(gBinderProxyMethods));
}
```

通过上面代码我们知道这个类型是android.os.BinderProxy，也就是说代理端Java层的对象是android.os.BinderProxy

###### 9.8 总结

结合下面的关系图，我们得出下面的逻辑关系:

-   每个进程都会保存多个当前调用过的BpBinder对象
-   每个BpBinder对象都会保存与之对应的Java层的BinderProxy

将创建的BinderProxy attach到BpBinder的意义在于，通过这种方式，Java应用层偏饭获取同一个Service的IBinder时，获取的是同一个BinderProxy。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/uxdqf8w1n6.png)

关系图.png

###### 10 Parcel读取写入总结

-   上面介绍了Parcel整个写入读取的流程，最后代替Binder传输的是 flat\_binder\_object。在Parcel-Native中，根据跨进程和非跨进程，flat\_binder\_object的值是不一样的：跨进程的时候flat\_binder\_object的type是BINDER\_TYPE\_HANDLE；非跨进程的时候flat\_binder\_object的type是BINDER\_TYPE\_BINDER。在这里可以发现跨进程与非跨进程的时候传输数据的区别。客户端的Parcel读取Binder的时候，根据flat\_binder\_object的type值进行区分对待，返回不同的内容。而写入的时候也是一样的，根据是否是Proxy，来决定写入HANDLE还是BINDER。最终这些内容都会通过ioctl与Binder驱动进行数据通信。所以最终处理不同进程间的Binder数据传输处理也只能是Binder驱动了。
-   Binder对象传入Binder驱动最底层是转化为flat\_binder\_object传递的。Parcel是根据从驱动中读取的数据做出不同的处理，如果从Binder驱动中取出的flat\_binder\_object的type为BINDER\_TYPE\_HANDLE，则创建BpBinder，在Java层创建的BinderProxy返回，如果读出的flat\_binder\_object的type为BINDER\_TYPE\_BINDER则直接使用cookie的指针，将它装置转为化JavaBBinder，在Java层为原来的Service的Binder对象(相同进程)。
-   在 **/frameworks/base/core/jni/android\_util\_Binder.cpp** 中有两个函数分别是ibinderForJavaObject()函数与javaObjectForIBinder()函数是相互对应的，一个是把Native中对应的IBinder对象化为Java对象，一个是将Java的对象转为化Native中对应的IBinder对象

### 五、智能指针

> Java和C/C++的一个重大区别，就是它没有"指针"的概念，这并不代表Java不需要只用指针，而是将这个"超级武器隐藏了"。如果大家使用C/C++开发过一些大型项目，就会知道一个比较头疼的问题——指针异常。所以Java以其他更"安全"的形式向开发人员提供了隐形的"指针"，使得用户既能享受到指针的强大功能，又能尽量避免指针带来的问题。

##### (一)、C/C++中常见的指针问题

###### 1、指针没有初始化

对指针进行初始化是程序员必须养成的良好习惯，也是指针问题中最容易解决和控制的一个问题

###### 2、new了对象没有及时delete

> 动态分配内存的对象，其实声明周期的控制不当常常会引起不少麻烦。如果只有一个程序员在维护时，问题通常不大，因为只要稍微留心就可以实现new和delete的配套操作；但是如果一个大型工程(特别是多滴协同研发的软件项目)，由于沟通的不及时或者人员素质的残差不起，就很可能会出现动态分配的内存没有回收的情况——造成的内存泄露问题往往是致命的。

###### 3、野指针

假设1：我们new了一个对象A，并将指针ptr指向这个新是对象(即ptr= new )。当对A使用结束后，我们也主动delete了A，但是唯一没做的是将ptr指针置空，那么可能出现什么问题？没错，就是野指针。因此如果有"第三方"视图用ptr来使用内存对象，它首先通过判断发现ptr不为空，自然而然的就认为这个对象还是存在的，其结果就是导致死机。 假设2：假设ptr1和ptr2都指向对象A，后来我们通过ptr1释放了A的内存空间，并且将ptr1也置为null；但是ptr2并不知道它所指向的内存对象已经不存在了，此时如果ptr2来访问A也会导致死机

##### (二)、我们设计的解决方案

上面分析了C/C++指针的问题。如果让我们设计Android的智能指针，怎么做才能防止以上几个问题？解决方案思路如下:

-   问题1的解决方案：这个简单，只要让指针在创建时设置为null即可
-   问题2的解决方案：比较复杂，既然是智能指针就为意味着它应该是一个"雷锋"，尽可能自动的实现new和delete的相应工作，那什么时候应该delete一个内存对象呢？肯定是"不需要的时候"(其实是个废话)。那怎么来分别什么是"需要"和"不需要"？在C/C++中，我们一般认为：当一个指针指向一个object的时候，这个内存对象就是"需要"的，当这个指针接触了与内存对象的关系，我们就认为这个内存对象已经"不需要"了。所以我们想到用一个布尔类型变量来保存即可。
-   问题3的解决方案：问题又来了，当有两个指针及两个以上指针同时使用这个内存怎么办，用布尔类型肯定是不行的。所以我们要用一个计数器来记录这个内存对象"被需要"的个数即可，当这个计数器递减到零时，就说明这个内存对象应该"寿终正寝"了。这就是在很多领域了都得到广泛应用的"引用计数"的概念。如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/6fq4thbveh.png)

引用计数.png

那Android到底是怎么设计的？

##### (三)、Android智能指针的原理

重点强调 ：

###### 智能指针是一个对象，而不是一个指针。

-   Android设计了基类RefBase，用以管理引用数，所有类必须从RefBase派生，RefBase是所有对象的始祖。
-   设计模板类sp、wp，用以引用实际对象，sp强引用和wp弱引用。sp、wp声明为栈对象，作用域结束时，自动释放，自动调用机析构函数。因此可以在sp、wp的构造函数中，增加引用计数，在析构函数中，减少引用计数。
-   专门设计weakref\_impl类，该类是RefBase的累不累，用来做真正的引用数管理，都有mRef来管理

Android智能指针的关系图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/c82g1r35aw.png)

Android智能指针的关系图.png

##### (四)、Android智能指针的源码位置

android中的智能指针的主要代码是：RefBase.h和RefBase.cpp StrongPointer.h 这三个文件，他们分别位于：

> RefBase.cpp：Android源码目录 /system/core/libutils/RefBase.cp RefBase.h：Android源码目录 /system/core/include/utils/RefBase.h StrongPointer.h：Android源码目录/system/core/include/utils/StrongPointer.h

链接如下

-   [RefBase.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Flibutils%2FRefBase.cpp&objectId=1199095&objectType=1&isNewArticle=undefined)
-   [RefBase.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Finclude%2Futils%2FRefBase.h&objectId=1199095&objectType=1&isNewArticle=undefined)
-   [StrongPointer.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Finclude%2Futils%2FStrongPointer.h&objectId=1199095&objectType=1&isNewArticle=undefined)

##### (五)、强指针sp

看到sp，很多人会以为是StrongPointer的缩写。与sp对应的是wp，我们将会在下一节讲解。 先来看下源码：

```
///system/core/include/utils/StrongPointer.h    58行
template<typename T>
class sp {
public:
    inline sp() : m_ptr(0) { }
   
    sp(T* other);   //常用的构造函数
    sp(const sp<T>& other); 
    template<typename U> sp(U* other);
    template<typename U> sp(const sp<U>& other);

    ~sp();    //析构函数

    // Assignment

    sp& operator = (T* other);   // 重载运算符"="
    sp& operator = (const sp<T>& other);

    template<typename U> sp& operator = (const sp<U>& other);  
    template<typename U> sp& operator = (U* other);

    //! Special optimization for use by ProcessState (and nobody else).
    void force_set(T* other);

    // Reset

    void clear();

    // Accessors

    inline  T&      operator* () const  { return *m_ptr; } // 重载运算符 " * "
    inline  T*      operator-> () const { return m_ptr;  }  // 重载运算符" -> "
    inline  T*      get() const         { return m_ptr; }

    // Operators

    COMPARE(==)
    COMPARE(!=)
    COMPARE(>)
    COMPARE(<)
    COMPARE(<=)
    COMPARE(>=)

private:
    template<typename Y> friend class sp;
    template<typename Y> friend class wp;
    void set_pointer(T* ptr);
    T* m_ptr;
};
```

通过阅读源码，我们知道这个sp类的设计和我们之前想象的基本一致，比如运算符的实现为：

```
//system/core/include/utils/StrongPointer.h    157行
template<typename T>
sp<T>& sp<T>::operator =(T* other) {
    if (other)
        other->incStrong(this);    //增加引用计数
    if (m_ptr)
        m_ptr->decStrong(this); // 减少引用计数
    m_ptr = other;
    return *this;
}
```

上面的diamante同时考虑了对一个智能指针重复赋值的情况。即当m\_ptr不为空时，要先撤销它之前指向的内存对象，然后才能赋予其新值。另外为sp分配一个内存对象，不一定要通过操作运算符(比如等号)，它的构造函数也是可以的。比如下面这段代码

```
//system/core/include/utils/StrongPointer.h    112行
template<typename T> 
sp<T>::sp(T* other)
        : m_ptr(other) {
    if (other)
        other->incStrong(this); //因为是构造函数，所以不同担心mptr之前已经赋值过
}
```

这时候m\_ptr就不用先置为null，可以直接指向目标对象。而析构函数的做法和我们的预想也是一样。

```
template<typename T>
sp<T>::~sp() {
    if (m_ptr)
        m_ptr->decStrong(this);
}
```

##### (六)、弱指针wp

###### 1、弱引用产生的背景：

> 在前面讨论之智能指针的"设计理念"时，其实是以强指针为原型逐步还原出智能指针作者的"意图'，那么"弱指针"又由什么作用?

其实弱指针主要是为了解决一个问题？那是什么问题那？有这么一种情况：父对象指向子对象child，然后子对象又指向父对象，这就存在了虚幻引用的现象。比如有两个class

```
struct Parent
{
    Child *myson;
}

struct Child
{
    Parent *myfather;
}
```

这样就会产生上面的问题。如果不考虑智能指针，这样的情况不会导致任何问题，但是在智能指针的场景下，就要注意了，因为Parent指向了Child，所以Child的引用计数器不为零。同时又由于Child指向了Parent，所以Parent的引用器不会为零。这有点类似于Java中的死锁了。因为内存回收者返现两者都是"被需要"的状态，当然不能释放，从而形成了恶性循环。

为了解决上面这个问题，产生了"弱引用"。具体措施如下：

> Parent使用强指针来引用Child，而Child只使用弱引用来指向父Parent类。双方规定当强引用计数器为0时，不论弱引用是否为0，都可以delete自己(Android系统中这个规定是可以调整的，后面有介绍)。这样只要一方得到了释放了，就可以成功避免死锁。当然这样就会造成野指针。是的，比如Parent因为因为强指针计数器计数已经到0了，根据规则生命周期就结束了。但是此时Child还持有父类的弱引用，显然如果Child此时用这个指针访问Parent会引发致命的问题。为了别面这个问题，我们还规定： **弱指针必须先升级为强指针，才能访问它所指向的目标对象。**

所以我们也可以说，若指针的主要使用就是解决循环引用的问题。下面具体看看它和强指针的区别。我们先从代码上看

###### 2、wp的源码解析：

代码在[RefBase.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Finclude%2Futils%2FRefBase.h&objectId=1199095&objectType=1&isNewArticle=undefined) 215行

```
template <typename T>
class wp
{
public:
    typedef typename RefBase::weakref_type weakref_type;

    inline wp() : m_ptr(0) { }

    wp(T* other);   //构造函数
    wp(const wp<T>& other);
    wp(const sp<T>& other);
    template<typename U> wp(U* other);
    template<typename U> wp(const sp<U>& other);
    template<typename U> wp(const wp<U>& other);

    ~wp();

    // Assignment

    wp& operator = (T* other);    //运算符重载
    wp& operator = (const wp<T>& other);
    wp& operator = (const sp<T>& other);

    template<typename U> wp& operator = (U* other);
    template<typename U> wp& operator = (const wp<U>& other);
    template<typename U> wp& operator = (const sp<U>& other);

    void set_object_and_refs(T* other, weakref_type* refs);

    // promotion to sp

    sp<T> promote() const;    //升级为强指针

    // Reset

    void clear();

    // Accessors

    inline  weakref_type* get_refs() const { return m_refs; }

    inline  T* unsafe_get() const { return m_ptr; }

    // Operators

    COMPARE_WEAK(==)
    COMPARE_WEAK(!=)
    COMPARE_WEAK(>)
    COMPARE_WEAK(<)
    COMPARE_WEAK(<=)
    COMPARE_WEAK(>=)

    inline bool operator == (const wp<T>& o) const {
        return (m_ptr == o.m_ptr) && (m_refs == o.m_refs);
    }
    template<typename U>
    inline bool operator == (const wp<U>& o) const {
        return m_ptr == o.m_ptr;
    }

    inline bool operator > (const wp<T>& o) const {
        return (m_ptr == o.m_ptr) ? (m_refs > o.m_refs) : (m_ptr > o.m_ptr);
    }
    template<typename U>
    inline bool operator > (const wp<U>& o) const {
        return (m_ptr == o.m_ptr) ? (m_refs > o.m_refs) : (m_ptr > o.m_ptr);
    }

    inline bool operator < (const wp<T>& o) const {
        return (m_ptr == o.m_ptr) ? (m_refs < o.m_refs) : (m_ptr < o.m_ptr);
    }
    template<typename U>
    inline bool operator < (const wp<U>& o) const {
        return (m_ptr == o.m_ptr) ? (m_refs < o.m_refs) : (m_ptr < o.m_ptr);
    }
                         inline bool operator != (const wp<T>& o) const { return m_refs != o.m_refs; }
    template<typename U> inline bool operator != (const wp<U>& o) const { return !operator == (o); }
                         inline bool operator <= (const wp<T>& o) const { return !operator > (o); }
    template<typename U> inline bool operator <= (const wp<U>& o) const { return !operator > (o); }
                         inline bool operator >= (const wp<T>& o) const { return !operator < (o); }
    template<typename U> inline bool operator >= (const wp<U>& o) const { return !operator < (o); }

private:
    template<typename Y> friend class sp;
    template<typename Y> friend class wp;

    T*              m_ptr;
    weakref_type*   m_refs;
};
```

通过和sp相比，我们发现有如下区别：

-   除了指向目标对象的m\_ptr外，wp另外有一个m\_refs指针，类型为weakref\_type。
-   没有重载 " -> " 、" \* " 等运算符。
-   有一个prmote方法将wp提升为sp。
-   目标对象的父类不是LightRefBase，而是RefBase

###### 3、wp的构造函数：

```
template<typename T>
wp<T>::wp(T* other)
    : m_ptr(other)
{
    if (other) m_refs = other->createWeak(this);
}
```

通过和强指针的中的构造函数进行对比，我们发现，wp并没有直接增加目标对象的引用计数值，而是调用了createWeak()函数。这个函数是RefBase类的

那我们来看下RefBase类

###### 3.1RefBase类

在代码在[RefBase.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Finclude%2Futils%2FRefBase.h&objectId=1199095&objectType=1&isNewArticle=undefined) 69行

```
class RefBase
{
public:
            void            incStrong(const void* id) const;   //增加强引用计数器的值
            void            decStrong(const void* id) const;  //减少强引用计数器的值

            void            forceIncStrong(const void* id) const;

            //! DEBUGGING ONLY: Get current strong ref count.
            int32_t         getStrongCount() const;

    class weakref_type   //嵌套类，wp中用到的就是这个类
    {
    public:
        RefBase*            refBase() const;

        void                incWeak(const void* id);   //增加弱引用计数器的值
        void                decWeak(const void* id);  //减少弱引用计数器的值

        // acquires a strong reference if there is already one.
        bool                attemptIncStrong(const void* id);

        // acquires a weak reference if there is already one.
        // This is not always safe. see ProcessState.cpp and BpBinder.cpp
        // for proper use.
        bool                attemptIncWeak(const void* id);

        //! DEBUGGING ONLY: Get current weak ref count.
        int32_t             getWeakCount() const;

        //! DEBUGGING ONLY: Print references held on object.
        void                printRefs() const;

        //! DEBUGGING ONLY: Enable tracking for this object.
        // enable -- enable/disable tracking
        // retain -- when tracking is enable, if true, then we save a stack trace
        //           for each reference and dereference; when retain == false, we
        //           match up references and dereferences and keep only the
        //           outstanding ones.

        void                trackMe(bool enable, bool retain);
    };

            weakref_type*   createWeak(const void* id) const;

            weakref_type*   getWeakRefs() const;

            //! DEBUGGING ONLY: Print references held on object.
    inline  void            printRefs() const { getWeakRefs()->printRefs(); }

            //! DEBUGGING ONLY: Enable tracking of object.
    inline  void            trackMe(bool enable, bool retain)
    {
        getWeakRefs()->trackMe(enable, retain);
    }

    typedef RefBase basetype;

protected:
                            RefBase();    //构造函数
    virtual                 ~RefBase();   //析构函数

    //! Flags for extendObjectLifetime()
    // 以下参数用于修改object的生命周期
    enum {
        OBJECT_LIFETIME_STRONG  = 0x0000,
        OBJECT_LIFETIME_WEAK    = 0x0001,
        OBJECT_LIFETIME_MASK    = 0x0001
    };

            void            extendObjectLifetime(int32_t mode);

    //! Flags for onIncStrongAttempted()
    enum {
        FIRST_INC_STRONG = 0x0001
    };

    virtual void            onFirstRef();
    virtual void            onLastStrongRef(const void* id);
    virtual bool            onIncStrongAttempted(uint32_t flags, const void* id);
    virtual void            onLastWeakRef(const void* id);

private:
    friend class weakref_type;
    class weakref_impl;

                            RefBase(const RefBase& o);
            RefBase&        operator=(const RefBase& o);

private:
    friend class ReferenceMover;

    static void renameRefs(size_t n, const ReferenceRenamer& renamer);

    static void renameRefId(weakref_type* ref,
            const void* old_id, const void* new_id);

    static void renameRefId(RefBase* ref,
            const void* old_id, const void* new_id);

        weakref_impl* const mRefs;
};
```

RefBase嵌套了一个重要的类weakref\_type，也就是前面的m\_refs指针所属的类型。RefBase中还有一个mRefs的成员变量，类型为weakref\_impl。从名称上来看，它应该是weak\_type的实现类。

###### 3.2 weakref\_impl类

在代码在[RefBase.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Flibutils%2FRefBase.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 64行

```
class RefBase::weakref_impl : public RefBase::weakref_type
{
public:
    volatile int32_t    mStrong;   //强引用计数器的值
    volatile int32_t    mWeak;   //弱引用计数器的值
    RefBase* const      mBase;      
    volatile int32_t    mFlags;

#if !DEBUG_REFS    //非Debug模式下，DEBUG_REFS是个宏

    weakref_impl(RefBase* base)
        : mStrong(INITIAL_STRONG_VALUE)
        , mWeak(0)
        , mBase(base)
        , mFlags(0)
    {
    }

    void addStrongRef(const void* /*id*/) { }
    void removeStrongRef(const void* /*id*/) { }
    void renameStrongRefId(const void* /*old_id*/, const void* /*new_id*/) { }
    void addWeakRef(const void* /*id*/) { }
    void removeWeakRef(const void* /*id*/) { }
    void renameWeakRefId(const void* /*old_id*/, const void* /*new_id*/) { }
    void printRefs() const { }
    void trackMe(bool, bool) { }

#else     //debug的情况下

    weakref_impl(RefBase* base)
        : mStrong(INITIAL_STRONG_VALUE)
        , mWeak(0)
        , mBase(base)
        , mFlags(0)
        , mStrongRefs(NULL)
        , mWeakRefs(NULL)
        , mTrackEnabled(!!DEBUG_REFS_ENABLED_BY_DEFAULT)
        , mRetain(false)
    {
    }

    ~weakref_impl()
    {
        bool dumpStack = false;
        if (!mRetain && mStrongRefs != NULL) {
            dumpStack = true;
            ALOGE("Strong references remain:");
            ref_entry* refs = mStrongRefs;
            while (refs) {
                char inc = refs->ref >= 0 ? '+' : '-';
                ALOGD("\t%c ID %p (ref %d):", inc, refs->id, refs->ref);
#if DEBUG_REFS_CALLSTACK_ENABLED
                refs->stack.log(LOG_TAG);
#endif
                refs = refs->next;
            }
        }

        if (!mRetain && mWeakRefs != NULL) {
            dumpStack = true;
            ALOGE("Weak references remain!");
            ref_entry* refs = mWeakRefs;
            while (refs) {
                char inc = refs->ref >= 0 ? '+' : '-';
                ALOGD("\t%c ID %p (ref %d):", inc, refs->id, refs->ref);
#if DEBUG_REFS_CALLSTACK_ENABLED
                refs->stack.log(LOG_TAG);
#endif
                refs = refs->next;
            }
        }
        if (dumpStack) {
            ALOGE("above errors at:");
            CallStack stack(LOG_TAG);
        }
    }

    void addStrongRef(const void* id) {
        //ALOGD_IF(mTrackEnabled,
        //        "addStrongRef: RefBase=%p, id=%p", mBase, id);
        addRef(&mStrongRefs, id, mStrong);
    }

    void removeStrongRef(const void* id) {
        //ALOGD_IF(mTrackEnabled,
        //        "removeStrongRef: RefBase=%p, id=%p", mBase, id);
        if (!mRetain) {
            removeRef(&mStrongRefs, id);
        } else {
            addRef(&mStrongRefs, id, -mStrong);
        }
    }

    void renameStrongRefId(const void* old_id, const void* new_id) {
        //ALOGD_IF(mTrackEnabled,
        //        "renameStrongRefId: RefBase=%p, oid=%p, nid=%p",
        //        mBase, old_id, new_id);
        renameRefsId(mStrongRefs, old_id, new_id);
    }

    void addWeakRef(const void* id) {
        addRef(&mWeakRefs, id, mWeak);
    }

    void removeWeakRef(const void* id) {
        if (!mRetain) {
            removeRef(&mWeakRefs, id);
        } else {
            addRef(&mWeakRefs, id, -mWeak);
        }
    }

    void renameWeakRefId(const void* old_id, const void* new_id) {
        renameRefsId(mWeakRefs, old_id, new_id);
    }

    void trackMe(bool track, bool retain)
    {
        mTrackEnabled = track;
        mRetain = retain;
    }

    void printRefs() const
    {
        String8 text;

        {
            Mutex::Autolock _l(mMutex);
            char buf[128];
            sprintf(buf, "Strong references on RefBase %p (weakref_type %p):\n", mBase, this);
            text.append(buf);
            printRefsLocked(&text, mStrongRefs);
            sprintf(buf, "Weak references on RefBase %p (weakref_type %p):\n", mBase, this);
            text.append(buf);
            printRefsLocked(&text, mWeakRefs);
        }

        {
            char name[100];
            snprintf(name, 100, DEBUG_REFS_CALLSTACK_PATH "/%p.stack", this);
            int rc = open(name, O_RDWR | O_CREAT | O_APPEND, 644);
            if (rc >= 0) {
                write(rc, text.string(), text.length());
                close(rc);
                ALOGD("STACK TRACE for %p saved in %s", this, name);
            }
            else ALOGE("FAILED TO PRINT STACK TRACE for %p in %s: %s", this,
                      name, strerror(errno));
        }
    }

private:
    struct ref_entry
    {
        ref_entry* next;
        const void* id;
#if DEBUG_REFS_CALLSTACK_ENABLED
        CallStack stack;
#endif
        int32_t ref;
    };

    void addRef(ref_entry** refs, const void* id, int32_t mRef)
    {
        if (mTrackEnabled) {
            AutoMutex _l(mMutex);

            ref_entry* ref = new ref_entry;
            // Reference count at the time of the snapshot, but before the
            // update.  Positive value means we increment, negative--we
            // decrement the reference count.
            ref->ref = mRef;
            ref->id = id;
#if DEBUG_REFS_CALLSTACK_ENABLED
            ref->stack.update(2);
#endif
            ref->next = *refs;
            *refs = ref;
        }
    }

    void removeRef(ref_entry** refs, const void* id)
    {
        if (mTrackEnabled) {
            AutoMutex _l(mMutex);

            ref_entry* const head = *refs;
            ref_entry* ref = head;
            while (ref != NULL) {
                if (ref->id == id) {
                    *refs = ref->next;
                    delete ref;
                    return;
                }
                refs = &ref->next;
                ref = *refs;
            }

            ALOGE("RefBase: removing id %p on RefBase %p"
                    "(weakref_type %p) that doesn't exist!",
                    id, mBase, this);

            ref = head;
            while (ref) {
                char inc = ref->ref >= 0 ? '+' : '-';
                ALOGD("\t%c ID %p (ref %d):", inc, ref->id, ref->ref);
                ref = ref->next;
            }

            CallStack stack(LOG_TAG);
        }
    }

    void renameRefsId(ref_entry* r, const void* old_id, const void* new_id)
    {
        if (mTrackEnabled) {
            AutoMutex _l(mMutex);
            ref_entry* ref = r;
            while (ref != NULL) {
                if (ref->id == old_id) {
                    ref->id = new_id;
                }
                ref = ref->next;
            }
        }
    }

    void printRefsLocked(String8* out, const ref_entry* refs) const
    {
        char buf[128];
        while (refs) {
            char inc = refs->ref >= 0 ? '+' : '-';
            sprintf(buf, "\t%c ID %p (ref %d):\n",
                    inc, refs->id, refs->ref);
            out->append(buf);
#if DEBUG_REFS_CALLSTACK_ENABLED
            out->append(refs->stack.toString("\t\t"));
#else
            out->append("\t\t(call stacks disabled)");
#endif
            refs = refs->next;
        }
    }

    mutable Mutex mMutex;
    ref_entry* mStrongRefs;
    ref_entry* mWeakRefs;

    bool mTrackEnabled;
    // Collect stack traces on addref and removeref, instead of deleting the stack references
    // on removeref that match the address ones.
    bool mRetain;

#endif
};
```

-   从开头的几个变量大概可以猜出weakref\_impl所做的工作，其中mStrong用于强引用计数，mWeak用于弱引用计数。宏DEBUG\_REFS用于指示release或debug版本，可以看出，在release版本下，addStrongRef,removeStrongRef相关的一系列方法都没有具体实现，也就是说，这些方法实际上是用于调试的，我们在分析时完全可以用户例会。这样一来，整体分析也清晰了很多。
-   Debug和Release版本都将mStrong初始化为INITIAL\_STRONG\_VALUE。这个值定义如下：

在代码在[RefBase.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Flibutils%2FRefBase.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 640行

```
#define INITIAL_STRONG_VALUE (1<<28)
```

而mWeak则初始化为0。上面的代码并没有引用计数器相关控制的实现，真正有用的代码在类声明的外面。比如我们在wp构造函数中遇到的createWeak函数，那让我们来看一下RefBase::createWeak()函数

###### 3.3 RefBase::createWeak()函数

在代码在[RefBase.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Flibutils%2FRefBase.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 572行

```
RefBase::weakref_type* RefBase::createWeak(const void* id) const
{
    mRefs->incWeak(id);  //增加弱引用计数
    return mRefs;  //直接返回weakref_type对象
}
```

这个函数先增加了mRefs(也就是weak\_impl类型成员变量)中的弱引用计数值，然后返回这个mRefs。

###### 3.4 wp与RefBase

关于类的关系图如下

![](https://ask.qcloudimg.com/http-save/yehe-2957818/743dm829fz.png)

image.png

-   首先 wp中的m\_ptr还是要指向目标对象(继承自RefBase)。RefBase提供了弱引用控制以及其他新的功能。
-   其次 因为RefBase需要处理多种计数类型，所以RefBase不直接使用int来保存应用计数器中的计数值，而是采用了weakref\_type的计数器。另外wp也同时保存了这个计数器的地址，也就是wp中的m\_refs和RefBase中的mRefs都指向了计数器。其中wp是通过构造函数中调用目标对象的createWeak来获取计数器地址的，而计数器本身是由RefBase在构造时创建的。
-   整个wp机制看起来很复杂，但与强指针相比实际上只是启动了一个新的计数器weakref\_impl而已，其他所有工作都是围绕如何操作这个计数器而展开的。虽然weakref\_impl是RefBase的成员变量，但是wp也可以直接控制它，所以整个逻辑显得稍微有点混乱。

在createWeak中，mRefs通过incWeak增加了计数器的弱引用。代码如下： 在代码在[RefBase.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Flibutils%2FRefBase.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 391行

```
void RefBase::weakref_type::incWeak(const void* id)
{
    weakref_impl* const impl = static_cast<weakref_impl*>(this);
    impl->addWeakRef(id);
    const int32_t c __unused = android_atomic_inc(&impl->mWeak);
    ALOG_ASSERT(c >= 0, "incWeak called on %p after last weak ref", this);
}
```

> 这个函数真真的有用的语句就是**android\_atomic\_inc(&impl->mWeak);** ，它增加了mWeak计数器的值，而其他都与调试有关。

这样当wp构造完成以后，RefBase所持有的weakref\_type计算器中的mWeak就为1。后面如果有新的wp指向这个目标对象，mWeak还会持续增加。

上面是wp增加引用的逻辑，那么如果sp指向它会怎么样？上面我们已经说了sp会调用目标对象的incStrong方法来增加强引用计数器的值，当目标对象继承自RefBase时，这个函数实现是

###### 3.5 incStrong()函数

在代码在[RefBase.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Flibutils%2FRefBase.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 572行

```
void RefBase::incStrong(const void* id) const
{
    weakref_impl* const refs = mRefs;
    refs->incWeak(id);  //增加弱引用计数值

    refs->addStrongRef(id);
    const int32_t c = android_atomic_inc(&refs->mStrong);  //增加强引用计数器的值
    ALOG_ASSERT(c > 0, "incStrong() called on %p after last strong ref", refs);
#if PRINT_REFS
    ALOGD("incStrong of %p from %p: cnt=%d\n", this, id, c);
#endif
    //判断是否不是第一次
    if (c != INITIAL_STRONG_VALUE)  {
        //不是第一次，直接返回
        return;
    }
    android_atomic_add(-INITIAL_STRONG_VALUE, &refs->mStrong);
    refs->mBase->onFirstRef();
}
```

其实核心就两行代码

```
refs->incWeak(id);
const int32_t c = android_atomic_inc(&refs->mStrong); 
```

其实也就是同时增加弱引用和强引用的计数器的值。然后还要判断目标对象是不是第一次被引用，其中C的变量得到的是"增加之前的值"，因而如果等于INITIAL\_STRONG\_VALUE就说明是第一次。这时候一方面回调onFirseRef通过对象自己被引用，另一方面要对mStrong值做下小调整。因为mStrong先是被置为INITIAL\_STRONG\_VALUE=1<<28，那么当一次增加时，它就是1<<28+1，所以还要再次减掉INITIAL\_STRONG\_VALUE才能得到1。

###### 4、对象释放

现在我们再来分析下目标对象在什么情况下会被释放。无非就是考察减少强弱引用时系统所遵循的规则，如下所示是decStrong的情况。 在代码在[RefBase.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Flibutils%2FRefBase.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 341行

```
void RefBase::decStrong(const void* id) const
{
    weakref_impl* const refs = mRefs;
    refs->removeStrongRef(id);
    //减少强引用计数器的值
    const int32_t c = android_atomic_dec(&refs->mStrong);
#if PRINT_REFS
    ALOGD("decStrong of %p from %p: cnt=%d\n", this, id, c);
#endif
    ALOG_ASSERT(c >= 1, "decStrong() called on %p too many times", refs);
    if (c == 1) {
        //减少强引用计数器的值已经降为0
        //通知事件
        refs->mBase->onLastStrongRef(id);
        if ((refs->mFlags&OBJECT_LIFETIME_MASK) == OBJECT_LIFETIME_STRONG) {
             //删除对象
            delete this;
        }
    }
    //减少弱引用计数器的值
    refs->decWeak(id);
}
```

整体流程如下：

-   首先减少mStrong计数器。
-   如果发现已经减到0（即c==1），就要回调onLastStrongRef通知这一事件，然后执行删除操作(如果标志是OBJECT\_LIFETIME\_STRONG)。
-   最后减少弱引用计数器的值

PS:特别注意，减少弱引用计数器的值还要同时减少弱引用计数器的值，即最后decWeak(id)。

###### 4.1、decWeak()函数

在代码在[RefBase.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Flibutils%2FRefBase.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 400行，实现代码如下：

```
void RefBase::weakref_type::decWeak(const void* id)
{
    weakref_impl* const impl = static_cast<weakref_impl*>(this);
    impl->removeWeakRef(id);
    //减少弱引用的值
    const int32_t c = android_atomic_dec(&impl->mWeak);
    ALOG_ASSERT(c >= 1, "decWeak called on %p too many times", this);
    if (c != 1) return;

    if ((impl->mFlags&OBJECT_LIFETIME_WEAK) == OBJECT_LIFETIME_STRONG) {
        // This is the regular lifetime case. The object is destroyed
        // when the last strong reference goes away. Since weakref_impl
        // outlive the object, it is not destroyed in the dtor, and
        // we'll have to do it here.
        if (impl->mStrong == INITIAL_STRONG_VALUE) {
            // Special case: we never had a strong reference, so we need to
            // destroy the object now.
            delete impl->mBase;
        } else {
            // ALOGV("Freeing refs %p of old RefBase %p\n", this, impl->mBase);
            delete impl;
        }
    } else {
        // less common case: lifetime is OBJECT_LIFETIME_{WEAK|FOREVER}
        impl->mBase->onLastWeakRef(id);
        if ((impl->mFlags&OBJECT_LIFETIME_MASK) == OBJECT_LIFETIME_WEAK) {
            // this is the OBJECT_LIFETIME_WEAK case. The last weak-reference
            // is gone, we can destroy the object.
            delete impl->mBase;
        }
    }
}
```

通过阅读上面的代码，我们发现

-   首先 显示减少mWeak计数器的值
-   其次 如果发现是0(即c==1)，就直接返回
-   如果发现不是0(即 c！=1)，则根据LIFETIME标志分别处理。

###### 4.2、LIEFTIME的标志

> LIEFTIME的标志是一个枚举类，代码如下 在代码在[RefBase.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Finclude%2Futils%2FRefBase.h&objectId=1199095&objectType=1&isNewArticle=undefined) 132行

```
    //! Flags for extendObjectLifetime()
    enum {
        OBJECT_LIFETIME_STRONG  = 0x0000,
        OBJECT_LIFETIME_WEAK    = 0x0001,
        OBJECT_LIFETIME_MASK    = 0x0001
    };
```

每个目标对象都可以通过以下方法来更改它的引用规则

在代码在[RefBase.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Flibutils%2FRefBase.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 609行

```
void RefBase::extendObjectLifetime(int32_t mode)
{
    android_atomic_or(mode, &mRefs->mFlags);
}
```

所以实际上就是改变了mFlags标志值——默认情况下它是0，即OBJECT\_LIFETIME\_STRONG。释放规则则受强引用控制的情况。有的人可能会想，既然是强引用控制，那么弱引用还要干什么？理论上它确实可以直接返回了，不过还有些特殊情况。前面在incString函数里，我们看到它同时增加了强、弱引用计数值。而增加弱引用是不会同时增加强引用的，这说明弱引用的值一定会大于强引用值。当程序走到这里，弱引用数值一定为0，而强引用的的值有两种可能:

-   一种是强引用值为INITIAL\_STRONG\_VALUE，说明这个目标对象没有被强引用过，也就是说没有办法靠强引用指针来释放目标，所以需要 **delete impl->mBase**
-   另外一种就是在有强引用的情况下，此时要delete impl，而目标对象会由强引用的decStrong来释放。

那么为什么在这里delete这个是计数器？weakref\_impl既然是由RefBase创建的，那么按道理来说应该由它来删除。实际上RefBase也想做这个工作，只是力不从心。其析构函数如下：

在代码在[RefBase.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fsystem%2Fcore%2Flibutils%2FRefBase.cpp&objectId=1199095&objectType=1&isNewArticle=undefined) 588行

```
RefBase::~RefBase()
{
    if (mRefs->mStrong == INITIAL_STRONG_VALUE) {
        // we never acquired a strong (and/or weak) reference on this object.
        delete mRefs;
    } else {
        // life-time of this object is extended to WEAK or FOREVER, in
        // which case weakref_impl doesn't out-live the object and we
        // can free it now.
        if ((mRefs->mFlags & OBJECT_LIFETIME_MASK) != OBJECT_LIFETIME_STRONG) {
            // It's possible that the weak count is not 0 if the object
            // re-acquired a weak reference in its destructor
            if (mRefs->mWeak == 0) {
                delete mRefs;
            }
        }
    }
    // for debugging purposes, clear this.
    const_cast<weakref_impl*&>(mRefs) = NULL;
}
```

在这种情况下，RefBase既然是有decStrong删除的，那么从上面的decStrong的执行顺序来看mWeak值还不为0，因而并不会被执行。 如果弱引用控制下的判断规则(即OBJECT\_LIFTIME\_WEAK)，其实和decStrong中的处理一样，要首先回调通知目标对象这一时间，然后才能执行删除操作。

###### 5、总结

关于Android的智能指针就分析到这里，我们总结一下：

-   1、智能指针分为强指针sp和弱指针wp
-   2、通常情况下目标对象的父类是RefBase——这个基类提供了一个weakref\_impl类型的引用计数器，可以同时进行强弱引用的控制(内部由mStrong和mWeak提供计数)
-   3、当incStrong增加强引用，也会增加弱引用
-   4、当incWeak时只增加弱引用计数
-   5、使用者可以通过extendObjectLifetime设置引用计数器的规则，不同规则下对删除目标对象的时机判断也是不一样的
-   6、使用者可以根据程序需求来选择合适的智能指针类型和计数器规则