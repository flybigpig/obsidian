_该文章是一个系列文章，是本人在__Android__开发的漫漫长途上的一点感想和记录，我会尽量按照先易后难的顺序进行编写该系列。该系列引用了《Android开发艺术探索》以及《深入理解Android 卷Ⅰ，Ⅱ，Ⅲ》中的相关知识，另外也借鉴了其他的优质博客，在此向各位大神表示感谢，膜拜！！！另外，本系列文章知识可能需要有一定Android开发基础和项目经验的同学才能更好理解，也就是说该系列文章面向的是Android中高级开发工程师。_

___

## 前言

上一次还不如不说去面试了呢，估计是挂了，数据结构与算法方面虽然面试前突击了一波，但是时间太短，当时学的也不好。另外Android的一些知识也不是很了解。不过这也加大了我写博客的动力。许多知识总觉得自己掌握的还挺好，不过一问到比较细节的方面就不太清楚了。所以写这整个博客的目的也是加深自己的知识，培养自己的沟通能力，和大家一起学习吧。 好了，闲话少说，我们这一篇先解决上一篇中遗留的问题，之后有时间的话，我把这次的面试经历单写一篇博客，和大家共勉。 本篇我们来看一下ServiceManager。上一篇中没怎么说它，ServiceManager作为Android系统服务的大管家。我们还是有必要来看一下它的。

## ServiceManager概述

ServiceManager是Android世界中所有重要系统服务的大管家。像前文提到的AMS(ActivityManagerService)，还有许多以后可能分析到的PackageManagerService等等服务都需要像ServiceManager中注册。那么为何需要一个ServiceManager呢，其重要作用何在呢？私认为有以下几点：

1.  ServiceManager能集中管理系统内的所有服务，它能施加权限控制，并不是任何进程都能注册服务的。
2.  ServiceManager支持通过字符串名称来查找对应的Service。这个功能很像DNS。由于各种原因的影响，Server进程可能生死无常。 如果让每个Client都去检测，压力实在太大了。 现在有了统一的管理机构，Client只需要查询ServiceManager，就能把握动向，得到最新信息。

## ServiceManager

\[SystemServer.java\]

```csharp
public void setSystemProcess() {
    try {
        //注册服务，第二个参数为this，这里假设SystemServer通过“socket”与SM交互
        ServiceManager.addService(Context.ACTIVITY_SERVICE, this, true);
        ..........
    } catch (PackageManager.NameNotFoundException e) {
        ........
    }
}
```

我们SystemServer进程中的AMS通过SM的代理与SM进程交互(读者也可以把这个过程想象为你所能理解的进程间通信方式，例如管道、Socket等)，并把自己注册在SM中。这个情况下，我们使用ServiceManager的代理与SM进程交互，既然有代理，那么也得有对应的服务端。那么根据我们之前博客的思路分析的话，就是如下的流程：

### ServiceManager是如何启动的？

按照我们之前博客的思路，我们在SystemServer端有了个ServiceManager的代理，那么Android系统中应该提供类似AMS这样的继承或间接继承自java层Binder然后重写onTransact方法以处理请求。但是并没有，ServiceManager并没有使用如AMS这样复杂的Binder类结构。而是直接与Binder驱动设备打交道。所以我们上一篇说了ServiceManager不一样。我们来看具体看一下。

ServiceManager在init.rc配置文件中配置启动，是一个以c/c++语言编写的程序。init进程、SM进程等关系如下图

![](https://ask.qcloudimg.com/http-save/yehe-1578745/9uv7k6u9ew.png)

我们来看它的main方法。

```cpp
int main(int argc, char **argv)
{
    struct binder_state *bs;
    //①应该是打开binder设备吧？
    bs = binder_open(128*1024);
    if (!bs) {
        ALOGE("failed to open binder driver\n");
        return -1;
    }
    //②成为manager
    if (binder_become_context_manager(bs)) {
        ALOGE("cannot become context manager (%s)\n", strerror(errno));
        return -1;
    }

   ......

    
    //③处理客户端发过来的请求
    binder_loop(bs, svcmgr_handler);

    return 0;
}
```

#### ①打开Binder设备

\[binder.c\]

```cpp
struct binder_state*binder_open(unsigned mapsize)
{
    struct binder_state*bs;
    bs=malloc(sizeof(*bs));
    ......
    //打开Binder设备
    bs->fd=open("/dev/binder"，O_RDWR);
    ......
    bs->mapsize=mapsize；
    //进行内存映射
    bs-＞mapped=mmap(NULL,mapsize,PROT_READ,MAP_PRIVATE,bs->
    fd,0);
}
```

这一步的目的是把内核层的binder驱动映射到用户空间。我们知道进程之间是独立的，进程呢运行在用户空间内，内核层的Binder驱动可以看成是一个文件（实际上它也是，Linux上都是文件）。这一步呢，可以看成把一个文件映射到用户空间，我们的进程呢通过这个文件进行交互。

![](https://ask.qcloudimg.com/http-save/yehe-1578745/6e6paoqu9h.png)

#### ②成为manager

\[Binder.c\]

```cpp
int binder_become_context_manager(struct binder_state*bs)
{
    //实现太简单了!这个有个0，什么鬼？
    return ioctl(bs->fd,BINDER_SET_CONTEXT_MGR,0);
}
```

#### ③处理客户端发过来的请求

\[Binder.c\]

```cpp
void binder_loop(struct binder_state *bs, binder_handler func)
{
    int res;
    struct binder_write_read bwr;
    uint32_t readbuf[32];

    bwr.write_size = 0;
    bwr.write_consumed = 0;
    bwr.write_buffer = 0;

    readbuf[0] = BC_ENTER_LOOPER;
    binder_write(bs, readbuf, sizeof(uint32_t));

    for (;;) {//果然是循环
        bwr.read_size = sizeof(readbuf);
        bwr.read_consumed = 0;
        bwr.read_buffer = (uintptr_t) readbuf;

        res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);

        if (res < 0) {
            ALOGE("binder_loop: ioctl failed (%s)\n", strerror(errno));
            break;
        }
        //接收到请求交给binder_parse，最终会调用func来处理这些请求
        res = binder_parse(bs, 0, (uintptr_t) readbuf, bwr.read_consumed, func);
        if (res == 0) {
            ALOGE("binder_loop: unexpected reply?!\n");
            break;
        }
        if (res < 0) {
            ALOGE("binder_loop: io error %d %s\n", res, strerror(errno));
            break;
        }
    }
}
```

上面传入func的是svcmgr\_ handler函数指针，所以会在svcmgr\_handler中进行集中处理客户端的请求。

\[service\_manager.c\]

```cpp
int svcmgr_handler(struct binder_state *bs,
                   struct binder_transaction_data *txn,
                   struct binder_io *msg,
                   struct binder_io *reply)
{
    struct svcinfo *si;
    uint16_t *s;
    size_t len;
    uint32_t handle;
    uint32_t strict_policy;
    int allow_isolated;

    
    if (txn->target.ptr != BINDER_SERVICE_MANAGER)
        return -1;

    if (txn->code == PING_TRANSACTION)
        return 0;

   
    strict_policy = bio_get_uint32(msg);
    s = bio_get_string16(msg, &len);
    if (s == NULL) {
        return -1;
    }

    if ((len != (sizeof(svcmgr_id) / 2)) ||
        memcmp(svcmgr_id, s, sizeof(svcmgr_id))) {
        fprintf(stderr,"invalid id %s\n", str8(s, len));
        return -1;
    }

    if (sehandle && selinux_status_updated() > 0) {
        struct selabel_handle *tmp_sehandle = selinux_android_service_context_handle();
        if (tmp_sehandle) {
            selabel_close(sehandle);
            sehandle = tmp_sehandle;
        }
    }

    switch(txn->code) {
    case SVC_MGR_GET_SERVICE://得到某个service的信息，service用字符串表示。
    case SVC_MGR_CHECK_SERVICE:
        s = bio_get_string16(msg, &len);//s是字符串表示的service名称。
        if (s == NULL) {
            return -1;
        }
        handle = do_find_service(bs, s, len, txn->sender_euid, txn->sender_pid);
        if (!handle)
            break;
        bio_put_ref(reply, handle);
        return 0;

    case SVC_MGR_ADD_SERVICE://对应addService请求。
        s = bio_get_string16(msg, &len);
        if (s == NULL) {
            return -1;
        }
        handle = bio_get_ref(msg);
        allow_isolated = bio_get_uint32(msg) ? 1 : 0;
        if (do_add_service(bs, s, len, handle, txn->sender_euid,
            allow_isolated, txn->sender_pid))
            return -1;
        break;

    case SVC_MGR_LIST_SERVICES: {//得到当前系统已经注册的所有service的名字。
        uint32_t n = bio_get_uint32(msg);

        if (!svc_can_list(txn->sender_pid)) {
            ALOGE("list_service() uid=%d - PERMISSION DENIED\n",
                    txn->sender_euid);
            return -1;
        }
        si = svclist;
        while ((n-- > 0) && si)
            si = si->next;
        if (si) {
            bio_put_string16(reply, si->name);
            return 0;
        }
        return -1;
    }
    default:
        ALOGE("unknown code %d\n", txn->code);
        return -1;
    }

    bio_put_uint32(reply, 0);
    return 0;
}
```

### ServiceManager的代理是如何获得的？

我们来回到最初的调用

\[SystemServer.java\]

```csharp
public void setSystemProcess() {
    try {
        //注册服务，第二个参数为this，这里假设SystemServer通过“socket”与SM交互
        ServiceManager.addService(Context.ACTIVITY_SERVICE, this, true);
        ..........
    } catch (PackageManager.NameNotFoundException e) {
        ........
    }
}
```

上面的请求最终是通过SM服务代理发送的，那这个代理是怎么来的呢？我们来看

\[ServiceManager.java\]

```typescript
public static void addService(String name, IBinder service) {
    try {
        getIServiceManager().addService(name, service, false);
    } catch (RemoteException e) {
        Log.e(TAG, "error in addService", e);
    }
}

private static IServiceManager getIServiceManager() {
    if (sServiceManager != null) {
        return sServiceManager;
    }

    // 是这里，没错了
    sServiceManager = ServiceManagerNative.asInterface(BinderInternal.getContextObject());
    return sServiceManager;
}
```

我们先来看BinderInternal.getContextObject()

\[BinderInternal.java\]

```java
//好吧，它还是个native函数
public static final native IBinder getContextObject();
```

跟进\[android\_ util\_Binder.cpp\]

```php
static jobject android_os_BinderInternal_getContextObject(JNIEnv* env, jobject clazz)
{
    sp<IBinder> b = ProcessState::self()->getContextObject(NULL);
    return javaObjectForIBinder(env, b);
}
```

跟进\[ProcessState.cpp\]

```rust
sp<IBinder> ProcessState::getContextObject(const sp<IBinder>& /*caller*/)
{
    return getStrongProxyForHandle(0);
}

/*这个函数是不是我们之前见过*/
sp<IBinder> ProcessState::getStrongProxyForHandle(int32_t handle)
{
    sp<IBinder> result;

    AutoMutex _l(mLock);

    handle_entry* e = lookupHandleLocked(handle);

    if (e != NULL) {
     
        IBinder* b = e->binder;
        if (b == NULL || !e->refs->attemptIncWeak(this)) {
            if (handle == 0) {//这里我们的handle为0
                
                Parcel data;
            //在handle对应的BpBinder第一次创建时
            //会执行一次虚拟的事务请求，以确保ServiceManager已经注册
                status_t status = IPCThreadState::self()->transact(
                        0, IBinder::PING_TRANSACTION, data, NULL, 0);
                if (status == DEAD_OBJECT)
                   return NULL;//如果ServiceManager没有注册，直接返回
            }
            //这里还是以handle参数创建了BpBinder
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

我们再一步步返回 \[android\_ util\_Binder.cpp\]

```kotlin
static jobject android_os_BinderInternal_getContextObject(JNIEnv* env, jobject clazz)
{
    sp<IBinder> b = ProcessState::self()->getContextObject(NULL);
    //这里的b = new BpBinder(0);
    return javaObjectForIBinder(env, b);
}
/*这个函数我们上一篇是不是也见过*/
jobject javaObjectForIBinder(JNIEnv* env, const sp<IBinder>& val) {
    if (val == NULL) return NULL;

    //如果val是Binder对象，进入下面分支，此时val是BpBinder
    if (val->checkSubclass(&gBinderOffsets)) {
        // One of our own!
        jobject object = static_cast<JavaBBinder*>(val.get())->object();
        LOGDEATH("objectForBinder %p: it's our own %p!\n", val.get(), object);
        return object;
    }
    .........
    //调用BpBinder的findObject函数
    //在Native层的BpBinder中有一个ObjectManager，它用来管理在Native BpBinder上创建的Java BinderProxy对象
    //findObject用于判断gBinderProxyOffsets中，是否存储了已经被ObjectManager管理的Java BinderProxy对象
    jobject object = (jobject)val->findObject(&gBinderProxyOffsets);
    if (object != NULL) {
        jobject res = jniGetReferent(env, object);
        ............
        //如果该Java BinderProxy已经被管理，则删除这个旧的BinderProxy
        android_atomic_dec(&gNumProxyRefs);
        val->detachObject(&gBinderProxyOffsets);
        env->DeleteGlobalRef(object);
    }

    //创建一个新的BinderProxy对象
    object = env->NewObject(gBinderProxyOffsets.mClass, gBinderProxyOffsets.mConstructor);
    if (object != NULL) {
        
        env->SetLongField(object, gBinderProxyOffsets.mObject, (jlong)val.get());
        val->incStrong((void*)javaObjectForIBinder);

       
        jobject refObject = env->NewGlobalRef(
                env->GetObjectField(object, gBinderProxyOffsets.mSelf));

        //新创建的BinderProxy对象注册到BpBinder的ObjectManager中，同时注册一个回收函数proxy_cleanup
        //当BinderProxy对象detach时，proxy_cleanup函数将被调用，以释放一些资源
        val->attachObject(&gBinderProxyOffsets, refObject,
                jnienv_to_javavm(env), proxy_cleanup);

        // Also remember the death recipients registered on this proxy
        sp<DeathRecipientList> drl = new DeathRecipientList;
        drl->incStrong((void*)javaObjectForIBinder);
        //将死亡通知list和BinderProxy联系起来
        env->SetLongField(object, gBinderProxyOffsets.mOrgue, reinterpret_cast<jlong>(drl.get()));

        // Note that a new object reference has been created.
        android_atomic_inc(&gNumProxyRefs);

        //垃圾回收相关；利用gNumRefsCreated记录创建出的BinderProxy数量
        //当创建出的BinderProxy数量大于200时，该函数将利用BinderInternal的ForceGc函数进行一个垃圾回收
        incRefsCreated(env);

        return object;
    }
}
```

接着返回到\[ServiceManager.java\]

```csharp
private static IServiceManager getIServiceManager() {
    if (sServiceManager != null) {
        return sServiceManager;
    }

    // 是这里，没错了BinderInternal.getContextObject()是BinderProxy对象
    sServiceManager = ServiceManagerNative.asInterface(BinderInternal.getContextObject());
    return sServiceManager;
}
```

跟进\[\[ServiceManagerNative.java\]\]

```java
static public IServiceManager asInterface(IBinder obj)
{
    if (obj == null) {
        return null;
    }
    我们知道这里的obj指向的是BinderProxy对象
    IServiceManager in =
        (IServiceManager)obj.queryLocalInterface(descriptor);
    if (in != null) {
        return in;
    }
    
    return new ServiceManagerProxy(obj);
}
```

跟进\[Binder.java\]

```typescript
final class BinderProxy implements IBinder {
   

    public IInterface queryLocalInterface(String descriptor) {
        return null;
    }
}
```

跟进\[ServiceManagerNative.java\]

```perl
class ServiceManagerProxy implements IServiceManager {
    public ServiceManagerProxy(IBinder remote) {
        //这里的mRemote指向了BinderProxy,与我们上一篇博客中讲述的遥相呼应
        mRemote = remote;
    }
}
```

### 本节小结

我们详尽讲述了SM进程的启动以及它作为服务大管家的意义。结合上一篇的内容我们总算是把Binder讲述的比较清楚了。

### AIDL

经过上面的介绍，你应该明白Java层Binder的架构中，Bp端可以通过BinderProxy的transact()方法与Bn端发送请求，而Bn端通过集成Binder重写onTransact()接收并处理来自Bp端的请求。这个结构非常清晰简单，在Android6.0，我们可以处处看到这样的设计，比如我们的ActivityManagerNavtive这个类，涉及到Binder通信的基本上都是这种设计。不过如果我们想要自己来定义一些远程服务。那这样的写法就比较繁琐，还好Android提供了AIDL，并且在Android8.0之后，我们可以看到与ActivityManagerNavtive相似的许多类已经被标注过时，因为Android系统也使用AIDL了。

#### AIDL的简单例子

AIDL的语法与定义一个java接口非常类似。下面我就定以一个非常简单的aidl

IMyAidlInterface.aidl

```csharp
interface IMyAidlInterface {
    int getTest();
}
```

然后基本上就行了，我们重新build之后会得到一个 IMyAidlInterface.java文件，这个文件由aidl工具生成，我们现在使用的基本是AndroidStudio,即使你使用的是Eclipse也没关系，这个文件会自动生成，不需要你操心。但是我们还是得来看看我们生成的这个文件

```java
public interface IMyAidlInterface extends android.os.IInterface {
    //抽象的Stub类，继承自Binder并实现我们定义的IMyAidlInterface接口
    //继承自Binder，重写onTransact方法，是不是感觉跟我们的XXXNative很像
    public static abstract class Stub extends android.os.Binder implements com.mafeibiao.testapplication.IMyAidlInterface {
        private static final java.lang.String DESCRIPTOR = "com.mafeibiao.testapplication.IMyAidlInterface";

        /**
         * Construct the stub at attach it to the interface.
         */
        public Stub() {
            this.attachInterface(this, DESCRIPTOR);
        }

        /**
         * Cast an IBinder object into an com.mafeibiao.testapplication.IMyAidlInterface interface,
         * generating a proxy if needed.
         */
        public static com.mafeibiao.testapplication.IMyAidlInterface asInterface(android.os.IBinder obj) {
            if ((obj == null)) {
                return null;
            }
            android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
            if (((iin != null) && (iin instanceof com.mafeibiao.testapplication.IMyAidlInterface))) {
                return ((com.mafeibiao.testapplication.IMyAidlInterface) iin);
            }
            return new com.mafeibiao.testapplication.IMyAidlInterface.Stub.Proxy(obj);
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
                case TRANSACTION_getTest: {
                    data.enforceInterface(DESCRIPTOR);
                    int _result = this.getTest();
                    reply.writeNoException();
                    reply.writeInt(_result);
                    return true;
                }
            }
            return super.onTransact(code, data, reply, flags);
        }
        
        /*这个Proxy不用说肯定是代理了，其内部还有个mRemote对象*/
        private static class Proxy implements com.mafeibiao.testapplication.IMyAidlInterface {
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

            @Override
            public int getTest() throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                int _result;
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    mRemote.transact(Stub.TRANSACTION_getTest, _data, _reply, 0);
                    _reply.readException();
                    _result = _reply.readInt();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                return _result;
            }
        }

        static final int TRANSACTION_getTest = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
    }

    public int getTest() throws android.os.RemoteException;
}
```

可见，AIDL的本质与XXXNative之类的类并没有什么本质的不同，不过他的出现使得构建一个Binder服务的工作大大简化了。

#### AIDL的使用详解

上面用一个非常简单的小例子来解密AIDL的本质，但是在实际使用AIDL的时候还有许多地方需要注意。

##### AIDL支持的数据类型

-   基本数据类型（int,long,charmboolean,double等）
-   String和CharSequence
-   List：只支持ArrrayList,并且里面每个元素的类型必须是AIDL支持的
-   Map:只支持HashMap,t,并且里面每个元素的类型必须是AIDL支持的
-   Parcelable:所有实现Parcelable接口的对象
-   AIDL:所有的AIDL接口本身也可以在AIDL文件中使用

以上6种数据类型就是AIDL所支持的所有类型，其中自定义的Parcel对象和AIDL对象必须要显示import进来，不管他们是否和当前的AIDL文件位于同一个包内。

另外一个需要注意的地方是如果我们在AIDL中使用了自定义的Parcelable接口的对象，那么我们必须新建一个和它同名的AIDL文件，并在其中声明它为Parcelable类型。 如下例 \[IBookManager.aidl\]

```java
package com.ryg.chapter_2.aidl;
/*这里显示import*/
import com.ryg.chapter_2.aidl.Book;


interface IBookManager {
     //这里我们使用了自定义的Parcelable对象
     List<Book> getBookList();
     void addBook(in Book book);
}
```

这里我们新建一个与Book同名的AIDL文件并声明

\[Book.aidl\]

```go
package com.ryg.chapter_2.aidl;

parcelable Book;
```

##### 定向tag

定向tag：这是一个极易被忽略的点——这里的“被忽略”指的不是大家都不知道，而是很少人会正确的使用它。 **AIDL中的定向 tag 表示了在跨进程通信中数据的流向，其中 in 表示数据只能由客户端流向服务端， out 表示数据只能由服务端流向客户端，而 inout 则表示数据可在服务端与客户端之间双向流通。其中，****数据流****向是针对在客户端中的那个传入方法的对象而言的。in 为定向 tag 的话表现为服务端将会接收到一个那个对象的完整数据，但是客户端的那个对象不会因为服务端对传参的修改而发生变动；out 的话表现为服务端将会接收到那个对象的的空对象，但是在服务端对接收到的空对象有任何修改之后客户端将会同步变动；inout 为定向 tag 的情况下，服务端将会接收到客户端传来对象的完整信息，并且客户端将会同步服务端对该对象的任何变动。**

另外，Java 中的**基本类型和 String ，CharSequence 的定向 tag 默认且只能是 in 。**还有，请注意，请不要滥用定向 tag ，而是要根据需要选取合适的——要是不管三七二十一，全都一上来就用 inout ，等工程大了系统的开销就会大很多——因为排列整理参数的开销是很昂贵的。

**所有的非基本参数都需要一个定向tag来指出数据的流向，不管是 in , out , 还是 inout 。基本参数的定向tag默认是并且只能是 in 。**

___

## 本篇总结

我们本篇详细分析了ServiceManager，ServiceManager并没有使用复杂的类结构，他直接与Binder驱动设备交互达到IPC通信的目的。(欠下的债终于补上了)

___

## 下篇预告

下篇我们来讲一下Android序列化相关知识。

___

此致，敬礼

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2018-01-04 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除