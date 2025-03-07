在Android系统中，所有的服务都必须注册到ServiceManger中，当客户进程需要请求某一服务时，首先从服务管家ServiceManger中查找出该服务，然后通过[RPC](https://so.csdn.net/so/search?q=RPC&spm=1001.2101.3001.7020)远程调用的方式使用该服务。服务在注册到ServiceManager时，需要将该服务对象发送到ServiceManager进程。Android是如何将一个binder对象进行序列化呢？本文将对Android的数据序列化进行详细分析。

在客户进程向服务进程发送IPC数据时，通常都是先将数据打包在Parcel对象中，然后通过内核空间发送到服务进程中。在[Android请求注册服务过程源码分析](http://blog.csdn.net/yangwen123/article/details/9075171 "Android请求注册服务过程源码分析")中分别从Java和C++层分析了服务注册过程的数据流程，[Android IPC数据在内核空间中的发送过程分析](http://blog.csdn.net/yangwen123/article/details/9078225 "Android IPC数据在内核空间中的发送过程分析")介绍了IPC数据在内核空间的交互过程。客户进程在将IPC数据打包到Parcel对象前，会首先获取一个Parcel对象，类似我们去邮局寄信件，首先需要从邮局获取信封，然后将信件装入到信封中，填写上收件人地址等就可以将信件发送出去。在Android的IPC通信中，Parcel对象就相当于信封，需要注册的服务相当于要邮寄的信件，handle就相当于收件人地址...

```java
public void addService(String name, IBinder service, boolean allowIsolated)
    throws RemoteException {
    Parcel data = Parcel.obtain();
    Parcel reply = Parcel.obtain();
    data.writeInterfaceToken(IServiceManager.descriptor);
    data.writeString(name);
    data.writeStrongBinder(service);
    data.writeInt(allowIsolated ? 1 : 0);
    mRemote.transact(ADD_SERVICE_TRANSACTION, data, reply, 0);
    reply.recycle();
    data.recycle();
}

```

### JNI函数注册

[Zygote进程启动过程的源代码分析](http://blog.csdn.net/yangwen123/article/details/8059995 "Zygote进程启动过程的源代码分析")中介绍了在Zygote进程启动时会注册系统JNI函数，对于Parcel对象也不例外：

```
REG_JNI(register_android_os_Parcel)
```

Parcel类的JNI注册函数实现：

```c++
int register_android_os_Parcel(JNIEnv * env) {
    jclass clazz;
    clazz = env - > FindClass(kParcelPathName);
    LOG_FATAL_IF(clazz == NULL, "Unable to find class android.os.Parcel");
    gParcelOffsets.clazz = (jclass) env - > NewGlobalRef(clazz);
    gParcelOffsets.mNativePtr = env - > GetFieldID(clazz, "mNativePtr", "I");
    gParcelOffsets.obtain = env - > GetStaticMethodID(clazz, "obtain", "()Landroid/os/Parcel;");
    gParcelOffsets.recycle = env - > GetMethodID(clazz, "recycle", "()V");
    return AndroidRuntime::registerNativeMethods(env, kParcelPathName, gParcelMethods, NELEM(gParcelMethods));
}
```

gParcelOffsets是C++中的静态类变量，在Zygote启动时通过JNI方法来读取android.os.Parcel类信息，并保持到gParcelOffsets结构体变量中，当C++层需要创建Java层的Parcel对象时，通过JNI方法及android.os.Parcel类信息就可以在C++层创建一个Java对象。

### 获取Parcel对象

通过Parcel类的静态成员函数obtain来获取一个Parcel对象实例

frameworks\\base\\core\\java\\android\\os\\Parcel.java

```java
public static Parcel obtain() {
    final Parcel[] pool = sOwnedPool;
    synchronized (pool) {
        Parcel p;
        for (int i = 0; i < POOL_SIZE; i++) {
            p = pool[i];
            if (p != null) {
                pool[i] = null;
                if (DEBUG_RECYCLE) {
                    p.mStack = new RuntimeException();
                }
                return p;
            }
        }
    }
    return new Parcel(0);
}

```

在Parcel类中，定义了一个静态Parcel对象池：

```java
private static final int POOL_SIZE = 6;

private static final Parcel[] sOwnedPool = new Parcel[POOL_SIZE];

private static final Parcel[] sHolderPool = new Parcel[POOL_SIZE];

```

函数obtain首先从该Parcel对象池中查找不为空的Parcel对象，如果没有找到，就创建一个新的Parcel对象，并且传递参数0.

```
private Parcel(int nativePtr) {if (DEBUG_RECYCLE) {mStack = new RuntimeException();}init(nativePtr);}
```

构造了Parcel对象后，调用init()函数来初始化该对象

```java
private void init(int nativePtr) {
    if (nativePtr != 0) {
        mNativePtr = nativePtr;
        mOwnsNativeParcelObject = false;
    } else {
        mNativePtr = nativeCreate();
        mOwnsNativeParcelObject = true;
    }
}

```

如果传进来的参数不为0，就将参数保存到mNativePtr变量中，该变量保存的是Java层Parcel对应的C++层Parcel对象的地址，同时设置mOwnsNativeParcelObject为false，表示该Java层Parcel对象现在还没有关联上C++层的Parcel对象。因为此时传进来的参数为0，因此函数将调用nativeCreate()函数来创建一个C++层的Parcel对象，并将该对象的地址保存在mNativePtr变量中，设置mOwnsNativeParcelObject为true，表示该Parcel对象已经关联了C++的Parcel对象。nativeCreate()函数是一个本地函数，其实现如下：

```c++
static jint android_os_Parcel_create(JNIEnv * env, jclass clazz) {
    Parcel * parcel = new Parcel();
    return reinterpret_cast < jint > (parcel);
}
```

这个函数很简单，就是构造一个Parcel对象，并将给对象的地址返回到Java层。

```c++
Parcel::Parcel() {
    initState();
}
void Parcel::initState() {
    mError = NO_ERROR;
    mData = 0;
    mDataSize = 0;
    mDataCapacity = 0;
    mDataPos = 0;
    ALOGV("initState Setting data size of %p to %d\n", this, mDataSize);
    ALOGV("initState Setting data pos of %p to %d\n", this, mDataPos);
    mObjects = NULL;
    mObjectsSize = 0;
    mObjectsCapacity = 0;
    mNextObjectHint = 0;
    mHasFds = false;
    mFdsKnown = true;
    mAllowFds = true;
    mOwner = NULL;
}
```

这样就构造了一对Parcel对象，分别是Java层的Parcel和C++层的Parcel对象，Java层的Parcel对象保存了C++层的Parcel对象的地址，而C++层在JNI函数注册时就保存了Java层的Parcel类的信息。通过这种方式就Java层的Parcel就可以很方便地找到与其对应的C++层Parcel对象，同时在C++层也可以创建出一个Java层的Parcel对象。

### 设置Parcel数据容量

在[Android IPC数据在内核空间中的发送过程分析](http://blog.csdn.net/yangwen123/article/details/9078225 "Android IPC数据在内核空间中的发送过程分析")中介绍了ProcessState，IPCThreadState对象与进程，线程之间的关系，同时介绍了每一个IPCThreadState对象都有mIn，mOut两个Parcel对象，用于存储进程间通信的IPC数据。在IPCThreadState对象的构造函数中，首先会初始化这两个数据容器的容量

```c++
IPCThreadState::IPCThreadState(): mProcess(ProcessState::self()), mMyThreadId(androidGetTid()), mStrictModePolicy(0), mLastTransactionBinderFlags(0) {
    pthread_setspecific(gTLS, this);
    clearCaller();
    mOrigCallingUid = mCallingUid;
    mIn.setDataCapacity(256);
    mOut.setDataCapacity(256);
}
```

接下来具体分析Parcel数据容量设置的完整过程

```c++
status_t Parcel::setDataCapacity(size_t size) {
    if (size > mDataCapacity) return continueWrite(size);
    return NO_ERROR;
}
```

传进来的参数size为256byte，前面介绍Parcel对象初始化时，mDataCapacity = 0，因此将调用continueWrite(256)作进一步处理

```c++
status_t Parcel::continueWrite(size_t desired) {
    size_t objectsSize = mObjectsSize;
    if (desired < mDataSize) {
        if (desired == 0) {
            objectsSize = 0;
        } else {
            while (objectsSize > 0) {
                if (mObjects[objectsSize - 1] < desired) break;
                objectsSize--;
            }
        }
    }
    if (mOwner) {
        if (desired == 0) {
            freeData();
            return NO_ERROR;
        }
        uint8_t * data = (uint8_t * ) malloc(desired);
        if (!data) {
            mError = NO_MEMORY;
            return NO_MEMORY;
        }
        size_t * objects = NULL;
        if (objectsSize) {
            objects = (size_t * ) malloc(objectsSize * sizeof(size_t));
            if (!objects) {
                mError = NO_MEMORY;
                return NO_MEMORY;
            }
            size_t oldObjectsSize = mObjectsSize;
            mObjectsSize = objectsSize;
            acquireObjects();
            mObjectsSize = oldObjectsSize;
        }
        if (mData) {
            memcpy(data, mData, mDataSize < desired ? mDataSize : desired);
        }
        if (objects && mObjects) {
            memcpy(objects, mObjects, objectsSize * sizeof(size_t));
        }
        mOwner(this, mData, mDataSize, mObjects, mObjectsSize, mOwnerCookie);
        mOwner = NULL;
        mData = data;
        mObjects = objects;
        mDataSize = (mDataSize < desired) ? mDataSize : desired;
        ALOGV("continueWrite Setting data size of %p to %d\n", this, mDataSize);
        mDataCapacity = desired;
        mObjectsSize = mObjectsCapacity = objectsSize;
        mNextObjectHint = 0;
    } else if (mData) {
        if (objectsSize < mObjectsSize) {
            const sp < ProcessState > proc(ProcessState::self());
            for (size_t i = objectsSize; i < mObjectsSize; i++) {
                const flat_binder_object * flat = reinterpret_cast < flat_binder_object * > (mData + mObjects[i]);
                if (flat - > type == BINDER_TYPE_FD) {
                    mFdsKnown = false;
                }
                release_object(proc, * flat, this);
            }
            size_t * objects = (size_t * ) realloc(mObjects, objectsSize * sizeof(size_t));
            if (objects) {
                mObjects = objects;
            }
            mObjectsSize = objectsSize;
            mNextObjectHint = 0;
        }
        if (desired > mDataCapacity) {
            uint8_t * data = (uint8_t * ) realloc(mData, desired);
            if (data) {
                mData = data;
                mDataCapacity = desired;
            } else if (desired > mDataCapacity) {
                mError = NO_MEMORY;
                return NO_MEMORY;
            }
        } else {
            if (mDataSize > desired) {
                mDataSize = desired;
                ALOGV("continueWrite Setting data size of %p to %d\n", this, mDataSize);
            }
            if (mDataPos > desired) {
                mDataPos = desired;
                ALOGV("continueWrite Setting data pos of %p to %d\n", this, mDataPos);
            }
        }
    } else {
        uint8_t * data = (uint8_t * ) malloc(desired);
        if (!data) {
            mError = NO_MEMORY;
            return NO_MEMORY;
        }
        if (!(mDataCapacity == 0 && mObjects == NULL && mObjectsCapacity == 0)) {
            ALOGE("continueWrite: %d/%p/%d/%d", mDataCapacity, mObjects, mObjectsCapacity, desired);
        }
        mData = data;
        mDataSize = mDataPos = 0;
        ALOGV("continueWrite Setting data size of %p to %d\n", this, mDataSize);
        ALOGV("continueWrite Setting data pos of %p to %d\n", this, mDataPos);
        mDataCapacity = desired;
    }
    return NO_ERROR;
}
```

```
mObjects = NULL;  
mObjectsSize = 0;  
mObjectsCapacity = 0;  
mData = malloc(256);  
mDataSize = 0 ;  
mDataPos = 0;  
mDataCapacity = 256;
```

### writeInterfaceToken函数

```c++
public final void writeInterfaceToken(String interfaceName) {
    nativeWriteInterfaceToken(mNativePtr, interfaceName);
}
static void android_os_Parcel_writeInterfaceToken(JNIEnv * env, jclass clazz, jint nativePtr, jstring name) {
    Parcel * parcel = reinterpret_cast < Parcel * > (nativePtr);
    if (parcel != NULL) {
        const jchar * str = env - > GetStringCritical(name, 0);
        if (str != NULL) {
            parcel - > writeInterfaceToken(String16(str, env - > GetStringLength(name)));
            env - > ReleaseStringCritical(name, str);
        }
    }
}
status_t Parcel::writeInterfaceToken(const String16 & interface) {
    writeInt32(IPCThreadState::self() - > getStrictModePolicy() | STRICT_MODE_PENALTY_GATHER);
    return writeString16(interface);
}
status_t Parcel::writeString16(const String16 & str) {
    return writeString16(str.string(), str.size());
}
```

函数调用了另外的writeString16函数，将要写入的字符串和字符串长度作为参数传递进去

```c++
status_t Parcel::writeString16(const char16_t * str, size_t len) {
    if (str == NULL) return writeInt32(-1);
    status_t err = writeInt32(len);
    if (err == NO_ERROR) {
        len *= sizeof(char16_t);
        uint8_t * data = (uint8_t * ) writeInplace(len + sizeof(char16_t));
        if (data) {
            memcpy(data, str, len);* reinterpret_cast < char16_t * > (data + len) = 0;
            return NO_ERROR;
        }
        err = mError;
    }
    return err;
}
```

![](https://i-blog.csdnimg.cn/blog_migrate/07d9f1edb6a896d24fae5c87f3c3298e.jpeg)

接下来看看字符串长度是如何写入的

```c++
status_t Parcel::writeInt32(int32_t val) {
    return writeAligned(val);
}
template < class T > status_t Parcel::writeAligned(T val) {
    COMPILE_TIME_ASSERT_FUNCTION_SCOPE(PAD_SIZE(sizeof(T)) == sizeof(T));
    if ((mDataPos + sizeof(val)) <= mDataCapacity) {
        restart_write: * reinterpret_cast < T * > (mData + mDataPos) = val;
        return finishWrite(sizeof(val));
    }
    status_t err = growData(sizeof(val));
    if (err == NO_ERROR) goto restart_write;
    return err;
}
```

![](https://i-blog.csdnimg.cn/blog_migrate/980c0267c3f4dc138a60096fa55fbeed.jpeg)

### writeStrongBinder函数

writeStrongBinder函数可以将一个Binder对象写入到Parcel中。在Android系统中，服务端的各个Service继承于Binder类，以下是Binder家族类关系图：

![](https://i-blog.csdnimg.cn/blog_migrate/edb23b1d2b0fdec4ebedbe1d6bf95eb2.jpeg)

下面分别介绍服务对象的构造过程。

对于ActivityManagerService服务来说，其继承于ActivityManagerNative，而ActivityManagerNative又继承与Binder类，对于其他的Service来说，根据aidl的模板规范，各个Service都继承与Stub类，该Stub类于继承于Binder类，它们之间的类继承关系如下图：

![](https://i-blog.csdnimg.cn/blog_migrate/d6c559084412e410d0f73dd087123411.png)

因此在构造Service对象时，会首先调用Binder类的构造函数，在调用Stub类的构造函数，最后才调用Service自身的构造函数，构造顺序如下：

![](https://i-blog.csdnimg.cn/blog_migrate/456354d2b70b463e7a71641f0c723dce.png)

Binder对象的构造过程

```c++
public Binder() {
    init();
    if (FIND_POTENTIAL_LEAKS) {
        final Class < ? extends Binder > klass = getClass();
        if ((klass.isAnonymousClass() || klass.isMemberClass() || klass.isLocalClass()) && (klass.getModifiers() & Modifier.STATIC) == 0) {
            Log.w(TAG, "The following Binder class should be static or leaks might occur: " + klass.getCanonicalName());
        }
    }
}
```

构造函数调用了init()函数来完成一些初始化工作。init()函数是一个本地函数，其对应的JNI函数为：

```c++
static void android_os_Binder_init(JNIEnv * env, jobject obj) {
    JavaBBinderHolder * jbh = new JavaBBinderHolder();
    if (jbh == NULL) {
        jniThrowException(env, "java/lang/OutOfMemoryError", NULL);
        return;
    }
    ALOGV("Java Binder %p: acquiring first ref on holder %p", obj, jbh);
    jbh - > incStrong((void * ) android_os_Binder_init);
    env - > SetIntField(obj, gBinderOffsets.mObject, (int) jbh);
}
```

Binder初始过程仅仅创建了一个JavaBBinderHolder对象，并且保存到了gBinderOffsets.mObject变量中了。

```
public final void writeStrongBinder(IBinder val) {nativeWriteStrongBinder(mNativePtr, val);}
```

nativeWriteStrongBinder是一个本地函数，其对应的JNI函数：

```c++
static void android_os_Parcel_writeStrongBinder(JNIEnv * env, jclass clazz, jint nativePtr, jobject object) {
    Parcel * parcel = reinterpret_cast < Parcel * > (nativePtr);
    if (parcel != NULL) {
        const status_t err = parcel - > writeStrongBinder(ibinderForJavaObject(env, object));
        if (err != NO_ERROR) {
            signalExceptionForError(env, clazz, err);
        }
    }
}
```

ibinderForJavaObject函数将Java层对象转换为C++层的对象。

```c++
sp < IBinder > ibinderForJavaObject(JNIEnv * env, jobject obj) {
    if (obj == NULL) return NULL;
    if (env - > IsInstanceOf(obj, gBinderOffsets.mClass)) {
        JavaBBinderHolder * jbh = (JavaBBinderHolder * ) env - > GetIntField(obj, gBinderOffsets.mObject);
        return jbh != NULL ? jbh - > get(env, obj) : NULL;
    }
    if (env - > IsInstanceOf(obj, gBinderProxyOffsets.mClass)) {
        return (IBinder * ) env - > GetIntField(obj, gBinderProxyOffsets.mObject);
    }
    ALOGW("ibinderForJavaObject: %p is not a Binder object", obj);
    return NULL;
}
```

函数首先判断需要转换的是Java层的Binder对象还是BinderProxy对象，如果是Binder对象，则取出在构造服务对象时创建的JavaBBinderHolder对象，如果该对象不为空，则调用JavaBBinderHolder对象的get函数来获取JavaBBinder对象，相反则返回空；如果是BinderProxy对象，则取出该BinderProxy对应的C++层的BpBinder对象。这里传进来的是一个服务对象，属于Binder对象，在构造该服务时已经创建了JavaBBinderHolder对象，因此此时取出来的对象不为空，通过get函数获取JavaBBinder对象：

```c++
sp < JavaBBinder > get(JNIEnv * env, jobject obj) {
    AutoMutex _l(mLock);
    sp < JavaBBinder > b = mBinder.promote();
    if (b == NULL) {
        b = new JavaBBinder(env, obj);
        mBinder = b;
        ALOGV("Creating JavaBinder %p (refs %p) for Object %p, weakCount=%d\n", b.get(), b - > getWeakRefs(), obj, b - > getWeakRefs() - > getWeakCount());
    }
    return b;
}
```

因为在构造JavaBBinderHolder对象时并没有初始化其成员变量mBinder，因此b=NULL，创建并返回一个新的JavaBBinder对象。JavaBBinder对象的构造过程如下：

```c++
JavaBBinder(JNIEnv * env, jobject object): mVM(jnienv_to_javavm(env)), mObject(env - > NewGlobalRef(object)) {
    ALOGV("Creating JavaBBinder %p\n", this);
    android_atomic_inc( & gNumLocalRefs);
    incRefsCreated(env);
}
```

回到ibinderForJavaObject函数，该函数首先创建一个JavaBBinder对象，并创建Java层服务Binder对象的全局引用，保存到JavaBBinder对象的mObject变量中，返回创建的JavaBBinder对象实例。

![](https://i-blog.csdnimg.cn/blog_migrate/9e1628011879c37ec109a5619c06cafe.jpeg)

1）由于每个服务都是一个Binder对象，在构造服务时，会首先在C++层构造一个JavaBBinderHolder对象，并将该对象的指针保存到Java层的服务的mObject变量中；

2）C++层的JavaBBinderHolder对象通过成员变量mBinder指向一个C++层的JavaBBinder对象，JavaBBinder类继承于BBinder类，是服务在C++层的表现形式；

3）C++层的JavaBBinder对象又通过成员变量mObject指向Java层的Binder服务对象；

当调用函数writeStrongBinder()来[序列化](https://so.csdn.net/so/search?q=%E5%BA%8F%E5%88%97%E5%8C%96&spm=1001.2101.3001.7020)一个Java层的Binder服务时，其实是序列化C++层的JavaBBinder对象。

```
parcel->writeStrongBinder(new JavaBBinder(env, service))
```

接着调用writeStrongBinder函数将创建的JavaBBinder对象写入到Parcel容器中。

```c++
status_t Parcel::writeStrongBinder(const sp < IBinder > & val) {
    return flatten_binder(ProcessState::self(), val, this);
}
```

函数直接调用flatten\_binder来写入Binder对象。该函数使用flat\_binder\_object结构体来描述Binder实体对象或Binder引用对象

```c++
status_t flatten_binder(const sp < ProcessState > & proc,
    const sp < IBinder > & binder, Parcel * out) {
    flat_binder_object obj;
    obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    if (binder != NULL) {
        IBinder * local = binder - > localBinder();
        if (!local) {
            BpBinder * proxy = binder - > remoteBinder();
            if (proxy == NULL) {
                ALOGE("null proxy");
            }
            const int32_t handle = proxy ? proxy - > handle() : 0;
            obj.type = BINDER_TYPE_HANDLE;
            obj.handle = handle;
            obj.cookie = NULL;
        } else {
            obj.type = BINDER_TYPE_BINDER;
            obj.binder = local - > getWeakRefs();
            obj.cookie = local;
        }
    } else {
        obj.type = BINDER_TYPE_BINDER;
        obj.binder = NULL;
        obj.cookie = NULL;
    }
    return finish_flatten_binder(binder, obj, out);
}
```

0x7f表示处理该Binder请求的线程最低优先级，FLAT\_BINDER\_FLAG\_ACCEPTS\_FDS表示该Binder可以接收文件描述符。传进来的参数binder为JavaBBinder对象，JavaBBinder继承于BBinder类，是一个Binder实体对象，因此参数binder不为空。

IBinder的localBinder()和remoteBinder()函数都是虚函数，由子类来实现，BBinder类实现了localBinder()函数，而BpBinder类实现了remoteBinder()函数。

```c++
BBinder * BBinder::localBinder() {
    return this;
}
BpBinder * BpBinder::remoteBinder() {
    return this;
}
```

因此local不为空，obj结构体成员的值为：

```c++
obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
obj.type = BINDER_TYPE_BINDER;
obj.binder = local - > getWeakRefs();
obj.cookie = local;
```

最后调用函数finish\_flatten\_binder将flat\_binder\_object写入到Parcel中

```c++
inline static status_t finish_flatten_binder(const sp < IBinder > & binder,
    const flat_binder_object & flat, Parcel * out) {
    return out - > writeObject(flat, false);
}
```

out传进来的是Parcel对象指针，

```c++
status_t Parcel::writeObject(const flat_binder_object & val, bool nullMetaData) {
    const bool enoughData = (mDataPos + sizeof(val)) <= mDataCapacity;
    const bool enoughObjects = mObjectsSize < mObjectsCapacity;
    if (enoughData && enoughObjects) {
        restart_write: * reinterpret_cast < flat_binder_object * > (mData + mDataPos) = val;
        if (nullMetaData || val.binder != NULL) {
            mObjects[mObjectsSize] = mDataPos;
            acquire_object(ProcessState::self(), val, this);
            mObjectsSize++;
        }
        if (val.type == BINDER_TYPE_FD) {
            if (!mAllowFds) {
                return FDS_NOT_ALLOWED;
            }
            mHasFds = mFdsKnown = true;
        }
        return finishWrite(sizeof(flat_binder_object));
    }
    if (!enoughData) {
        const status_t err = growData(sizeof(val));
        if (err != NO_ERROR) return err;
    }
    if (!enoughObjects) {
        size_t newSize = ((mObjectsSize + 2) * 3) / 2;
        size_t * objects = (size_t * ) realloc(mObjects, newSize * sizeof(size_t));
        if (objects == NULL) return NO_MEMORY;
        mObjects = objects;
        mObjectsCapacity = newSize;
    }
    goto restart_write;
}
```

每一个Binder对象都使用flat\_binder\_object结构体来描述，并写入到Parcel对象中，写入过程如下：

![](https://i-blog.csdnimg.cn/blog_migrate/2e805c31334aac7d2931888c03a2efe2.jpeg)

因此在注册一个Java服务时，向ServiceManager进程发送的是flat\_binder\_object数据，flat\_binder\_object，JavaBBinder，Service之间的关系如下图：

![](https://i-blog.csdnimg.cn/blog_migrate/9cb3374b5a0aadd2a45c1a304d627388.jpeg)

readStrongBinder()函数的对象转换过程：

![](https://i-blog.csdnimg.cn/blog_migrate/a44b8d70751e7470fd62ff95e8de874a.png)

### readStrongBinder函数

在ServiceManagerProxy类的getService（）函数中，通过mRemote.transact(GET\_SERVICE\_TRANSACTION, data, reply, 0)向ServiceManager进程发送服务查询信息，然后调用IBinder binder = reply.readStrongBinder()读取ServiceManager进程查询的服务代理对象。接下来详细分析服务Binder对象的读取过程。

```
public final IBinder readStrongBinder() {return nativeReadStrongBinder(mNativePtr);}
```

函数直接调用的是nativeReadStrongBinder（）函数，该函数是一个本地函数，参数mNativePtr是Parcel对象reply在C++中对应的Parcel对象地址。

```
private static native IBinder nativeReadStrongBinder(int nativePtr);
```

其对应的JNI函数为

frameworks\\base\\core\\jni\\android\_os\_Parcel.cpp

```c++
static jobject android_os_Parcel_readStrongBinder(JNIEnv * env, jclass clazz, jint nativePtr) {
    Parcel * parcel = reinterpret_cast < Parcel * > (nativePtr);
    if (parcel != NULL) {
        return javaObjectForIBinder(env, parcel - > readStrongBinder());
    }
    return NULL;
}
```

函数首先通过Java层传过来的nativePtr找到C++层的Parcel对象，然后调用该Parcel对象的readStrongBinder()函数来读取服务的Binder代理对象

```c++
sp < IBinder > Parcel::readStrongBinder() const {
    sp < IBinder > val;
    unflatten_binder(ProcessState::self(), * this, & val);
    return val;
}
```

直接调用unflatten\_binder()函数来完成读取过程

```c++
status_t unflatten_binder(const sp < ProcessState > & proc,
    const Parcel & in, sp < IBinder > * out) {
    const flat_binder_object * flat = in.readObject(false);
    if (flat) {
        switch (flat - > type) {
            case BINDER_TYPE_BINDER:
                * out = static_cast < IBinder * > (flat - > cookie);
                return finish_unflatten_binder(NULL, * flat, in);
            case BINDER_TYPE_HANDLE:
                * out = proc - > getStrongProxyForHandle(flat - > handle);
                return finish_unflatten_binder(static_cast < BpBinder * > (out - > get()), * flat, in);
        }
    }
    return BAD_TYPE;
}
```

当服务注册进程请求查询服务时，返回该服务的Binder本地对象地址：

```
fp->binder = ref->node\->ptr;  

fp->cookie = ref->node->cookie;  
```

当服务查询进程不是注册该服务的进程时，返回Binder驱动为服务查询进程创建的Binder对象的句柄值：

```
fp->handle = new\_ref->desc;
```

参数in是Java层的Parcel对象reply在C++层对应的Parcel对象，这里使用in.readObject(false)从Parcel对象中读取出ServiceManager进程写入的binder\_object，并转换为flat\_binder\_object类型指针

```c++
const flat_binder_object * Parcel::readObject(bool nullMetaData) const {
    const size_t DPOS = mDataPos;
    if ((DPOS + sizeof(flat_binder_object)) <= mDataSize) {
        const flat_binder_object * obj = reinterpret_cast <
            const flat_binder_object * > (mData + DPOS);
        mDataPos = DPOS + sizeof(flat_binder_object);
        if (!nullMetaData && (obj - > cookie == NULL && obj - > binder == NULL)) {
            return obj;
        }
        size_t *
            const OBJS = mObjects;
        const size_t N = mObjectsSize;
        size_t opos = mNextObjectHint;
        if (N > 0) {
            if (opos < N) {
                while (opos < (N - 1) && OBJS[opos] < DPOS) {
                    opos++;
                }
            } else {
                opos = N - 1;
            }
            if (OBJS[opos] == DPOS) {
                mNextObjectHint = opos + 1;
                ALOGV("readObject Setting data pos of %p to %d\n", this, mDataPos);
                return obj;
            }
            while (opos > 0 && OBJS[opos] > DPOS) {
                opos--;
            }
            if (OBJS[opos] == DPOS) {
                mNextObjectHint = opos + 1;
                ALOGV("readObject Setting data pos of %p to %d\n", this, mDataPos);
                return obj;
            }
        }
    }
    return NULL;
}
```

要理解整个读取过程，必须先了解Binder对象在Parcel中的存储方式

![](https://i-blog.csdnimg.cn/blog_migrate/2e805c31334aac7d2931888c03a2efe2.jpeg)

readObject()函数从Parcel中读取到ServiceManager进程返回来的flat\_binder\_object，函数unflatten\_binder()则根据flat\_binder\_object结构体中的内容生成JavaBBinder对象或者BpBinder对象，函数javaObjectForIBinder()在[Android请求注册服务过程源码分析](http://blog.csdn.net/yangwen123/article/details/9075171 "Android请求注册服务过程源码分析")已经详细介绍了，作用是根据BpBinder对象创建Java层的BinderProxy对象。

readStrongBinder()函数的对象转换过程：

![](https://i-blog.csdnimg.cn/blog_migrate/1d415794e25a3dbc4d2985ede081b675.png)