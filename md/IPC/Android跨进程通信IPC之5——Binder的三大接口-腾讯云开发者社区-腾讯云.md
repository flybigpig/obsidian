本片文章的主要目的是让大家对Binder有个初步的了解，既然是初步了解，肯定所是以源码上的注释为主，让大家对Binder有一个更直观的认识。PS:大部分注释我是写在类里面了， 重要的我会单独的拿出来。 主要内容如下：

> 1、IInterface 2、IBinder 3、Binder与BinderProxy类 4、总结

### 一、IInterface

```
/**
 * Base class for Binder interfaces.  When defining a new interface,
 * you must derive it from IInterface.
 */
public interface IInterface
{
    /**
     * Retrieve the Binder object associated with this interface.
     * You must use this instead of a plain cast, so that proxy objects
     * can return the correct result.
     */
    public IBinder asBinder();
}
```

##### (一)、类注释

简单的翻译一下：

> IInterface是Binder中相关接口的基类。 定义新接口的时候，你必须从IInterface派生。

##### (二)、抽象方法注释

> 如果想获取和该接口关联的Binder对象。你必须使用这个方法来而不是使用一个简单的类型转化。这样代理对象才能返回正确的结果

##### (三)、总结

> 所以可以这样说IInterface接口提供了类型转化的功能，将服务或者服务代理类转为IBinder类型。其实实际的类型转换是由BnInterface、BpInterface两个类完成的，BnInterface将服务类转换成IBinder类型，而BpInterface则将代理服务类转换成IBinder类型。在通过Binder Driver传递Binder对象时，必须进行类型转换，比如在想系统注册服务时，需要先将服务类型转换成IBinder，在其传递给Context Manager。

### 二、IBinder

```
/**
 * Base interface for a remotable object, the core part of a lightweight
 * remote procedure call mechanism designed for high performance when
 * performing in-process and cross-process calls.  This
 * interface describes the abstract protocol for interacting with a
 * remotable object.  Do not implement this interface directly, instead
 * extend from {@link Binder}.
 * 
 * <p>The key IBinder API is {@link #transact transact()} matched by
 * {@link Binder#onTransact Binder.onTransact()}.  These
 * methods allow you to send a call to an IBinder object and receive a
 * call coming in to a Binder object, respectively.  This transaction API
 * is synchronous, such that a call to {@link #transact transact()} does not
 * return until the target has returned from
 * {@link Binder#onTransact Binder.onTransact()}; this is the
 * expected behavior when calling an object that exists in the local
 * process, and the underlying inter-process communication (IPC) mechanism
 * ensures that these same semantics apply when going across processes.
 * 
 * <p>The data sent through transact() is a {@link Parcel}, a generic buffer
 * of data that also maintains some meta-data about its contents.  The meta
 * data is used to manage IBinder object references in the buffer, so that those
 * references can be maintained as the buffer moves across processes.  This
 * mechanism ensures that when an IBinder is written into a Parcel and sent to
 * another process, if that other process sends a reference to that same IBinder
 * back to the original process, then the original process will receive the
 * same IBinder object back.  These semantics allow IBinder/Binder objects to
 * be used as a unique identity (to serve as a token or for other purposes)
 * that can be managed across processes.
 * 
 * <p>The system maintains a pool of transaction threads in each process that
 * it runs in.  These threads are used to dispatch all
 * IPCs coming in from other processes.  For example, when an IPC is made from
 * process A to process B, the calling thread in A blocks in transact() as
 * it sends the transaction to process B.  The next available pool thread in
 * B receives the incoming transaction, calls Binder.onTransact() on the target
 * object, and replies with the result Parcel.  Upon receiving its result, the
 * thread in process A returns to allow its execution to continue.  In effect,
 * other processes appear to use as additional threads that you did not create
 * executing in your own process.
 * 
 * <p>The Binder system also supports recursion across processes.  For example
 * if process A performs a transaction to process B, and process B while
 * handling that transaction calls transact() on an IBinder that is implemented
 * in A, then the thread in A that is currently waiting for the original
 * transaction to finish will take care of calling Binder.onTransact() on the
 * object being called by B.  This ensures that the recursion semantics when
 * calling remote binder object are the same as when calling local objects.
 * 
 * <p>When working with remote objects, you often want to find out when they
 * are no longer valid.  There are three ways this can be determined:
 * <ul>
 * <li> The {@link #transact transact()} method will throw a
 * {@link RemoteException} exception if you try to call it on an IBinder
 * whose process no longer exists.
 * <li> The {@link #pingBinder()} method can be called, and will return false
 * if the remote process no longer exists.
 * <li> The {@link #linkToDeath linkToDeath()} method can be used to register
 * a {@link DeathRecipient} with the IBinder, which will be called when its
 * containing process goes away.
 * </ul>
 * 
 * @see Binder
 */
public interface IBinder {
    /**
     * The first transaction code available for user commands.
     * 第一个可用于用户命令的事务代码。
     */
    int FIRST_CALL_TRANSACTION  = 0x00000001;
    /**
     * The last transaction code available for user commands.
     * 最后一个可用于用户命令的事务代码。
     */
    int LAST_CALL_TRANSACTION   = 0x00ffffff;
    
    /**
     * IBinder protocol transaction code: pingBinder().
     * IBinder协议事物码：在pingBinder()会用到
     */
    int PING_TRANSACTION        = ('_'<<24)|('P'<<16)|('N'<<8)|'G';
    
    /**
     * IBinder protocol transaction code: dump internal state.
     * IBinder协议事物码: 代表清除内部状态 
     */
    int DUMP_TRANSACTION        = ('_'<<24)|('D'<<16)|('M'<<8)|'P';
    
    /**
     * IBinder protocol transaction code: execute a shell command.
     * IBinder协议事物码：代表执行一个shell命令
     * @hide
     */
    int SHELL_COMMAND_TRANSACTION = ('_'<<24)|('C'<<16)|('M'<<8)|'D';

    /**
     * IBinder protocol transaction code: interrogate the recipient side
     * of the transaction for its canonical interface descriptor.
     *  IBinder协议事物码：代表询问被调用方的接口描述符号
     */
    int INTERFACE_TRANSACTION   = ('_'<<24)|('N'<<16)|('T'<<8)|'F';

    /**
     * IBinder protocol transaction code: send a tweet to the target
     * object.  The data in the parcel is intended to be delivered to
     * a shared messaging service associated with the object; it can be
     * anything, as long as it is not more than 130 UTF-8 characters to
     * conservatively fit within common messaging services.  As part of
     * {@link Build.VERSION_CODES#HONEYCOMB_MR2}, all Binder objects are
     * expected to support this protocol for fully integrated tweeting
     * across the platform.  To support older code, the default implementation
     * logs the tweet to the main log as a simple emulation of broadcasting
     * it publicly over the Internet.
     * 
     * <p>Also, upon completing the dispatch, the object must make a cup
     * of tea, return it to the caller, and exclaim "jolly good message
     * old boy!".
     * IBinder协议事物码:向目标对象发出一个呼叫。在parcel中的数据是用于
     * 分发一个与对象结合的共享信息服务。它可以是任何东西，只要它不超
     * 过130个UTF-8字符，保证适用于常见的信息服务。 作为 
     * Build.VERSION_CODES#HONEYCOMB_MR2的一部分，所有的binder
     * 对象都支持这个协议，为了在跨平台间完整推送同时也为了支持旧的交
     * 互码，一个默认的实现是在主日志中记录了一个推送，作为广播的一个
     * 简单模仿。
     * 并且，为了完成消息的派送，对象必须返回给调用者一个说明，
     * 表明是旧的信息。
     */
    int TWEET_TRANSACTION   = ('_'<<24)|('T'<<16)|('W'<<8)|'T';

    /**
     * IBinder protocol transaction code: tell an app asynchronously that the
     * caller likes it.  The app is responsible for incrementing and maintaining
     * its own like counter, and may display this value to the user to indicate the
     * quality of the app.  This is an optional command that applications do not
     * need to handle, so the default implementation is to do nothing.
     * 
     * <p>There is no response returned and nothing about the
     * system will be functionally affected by it, but it will improve the
     * app's self-esteem.
     * IBinder协议事物码:异步地告诉app 有一个调用者在呼叫它。这个app负
     * 责计算和维护自己的呼叫者数目。 并且可以展示这个值来告诉用户app
     * 的状态。这个是可选的命令，app不需要掌管它，所以默认是实现是什
     * 么都不做。
     * 这是没有响应的，并且不会对系统带来影响，但是它提高了app的自控
     * (我真的不知道怎么翻译，elf-esteem其实是自尊的意思)
     */
    int LIKE_TRANSACTION   = ('_'<<24)|('L'<<16)|('I'<<8)|'K';

    /** @hide */
    //隐藏的API
    int SYSPROPS_TRANSACTION = ('_'<<24)|('S'<<16)|('P'<<8)|'R';

    /**
     * Flag to {@link #transact}: this is a one-way call, meaning that the
     * caller returns immediately, without waiting for a result from the
     * callee. Applies only if the caller and callee are in different
     * processes.
     * 用于transact()方法中的flag，表示单向RPC，表明呼叫者会马上返回，
     * 不必等结果从被呼叫者返回。只有当呼叫者和被呼叫者在不同的进程中
     * 才有效.
     */
    int FLAG_ONEWAY             = 0x00000001;

    /**
     * Limit that should be placed on IPC sizes to keep them safely under the
     * transaction buffer limit.
     * @hide
     * 为了让其安全地保持在事物缓冲区限制之下，应该限制IPC的大小
     *  隐藏的API
     */
    public static final int MAX_IPC_SIZE = 64 * 1024;

    /**
     * Get the canonical name of the interface supported by this binder.
     *  获取一个支持binder的接口规范名称
     */
    public String getInterfaceDescriptor() throws RemoteException;

    /**
     * Check to see if the object still exists.
     * 
     * @return Returns false if the
     * hosting process is gone, otherwise the result (always by default
     * true) returned by the pingBinder() implementation on the other
     * side.
     * 检查对象是否仍然存在。
     * 如果返回false，主机进程消失，否则结果为true（始终默认true）由对
     * 手方来实现pingBinder()这个方法。
     */
    public boolean pingBinder();

    /**
     * Check to see if the process that the binder is in is still alive.
     *
     * @return false if the process is not alive.  Note that if it returns
     * true, the process may have died while the call is returning.
     * 检查该binder所在的进程是否仍然存在
     * 如果进程不存在，则返回false。 请注意，如果返回true，则调用返回时
     * 进程可能已经死机。
     */
    public boolean isBinderAlive();
    
    /**
     * Attempt to retrieve a local implementation of an interface
     * for this Binder object.  If null is returned, you will need
     * to instantiate a proxy class to marshall calls through
     * the transact() method.
     */
    public IInterface queryLocalInterface(String descriptor);

    /**
     * Print the object's state into the given stream.
     * 
     * @param fd The raw file descriptor that the dump is being sent to.
     * @param args additional arguments to the dump request.
     */
    public void dump(FileDescriptor fd, String[] args) throws RemoteException;

    /**
     * Like {@link #dump(FileDescriptor, String[])} but always executes
     * asynchronously.  If the object is local, a new thread is created
     * to perform the dump.
     *
     * @param fd The raw file descriptor that the dump is being sent to.
     * @param args additional arguments to the dump request.
     */
    public void dumpAsync(FileDescriptor fd, String[] args) throws RemoteException;

    /**
     * Execute a shell command on this object.  This may be performed asynchrously from the caller;
     * the implementation must always call resultReceiver when finished.
     *
     * @param in The raw file descriptor that an input data stream can be read from.
     * @param out The raw file descriptor that normal command messages should be written to.
     * @param err The raw file descriptor that command error messages should be written to.
     * @param args Command-line arguments.
     * @param resultReceiver Called when the command has finished executing, with the result code.
     * @hide
     */
    public void shellCommand(FileDescriptor in, FileDescriptor out, FileDescriptor err,
            String[] args, ResultReceiver resultReceiver) throws RemoteException;

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

    /**
     * Interface for receiving a callback when the process hosting an IBinder
     * has gone away.
     * 
     * @see #linkToDeath
     */
    public interface DeathRecipient {
        public void binderDied();
    }

    /**
     * Register the recipient for a notification if this binder
     * goes away.  If this binder object unexpectedly goes away
     * (typically because its hosting process has been killed),
     * then the given {@link DeathRecipient}'s
     * {@link DeathRecipient#binderDied DeathRecipient.binderDied()} method
     * will be called.
     * 
     * <p>You will only receive death notifications for remote binders,
     * as local binders by definition can't die without you dying as well.
     * 
     * @throws RemoteException if the target IBinder's
     * process has already died.
     * 
     * @see #unlinkToDeath
     */
    public void linkToDeath(DeathRecipient recipient, int flags)
            throws RemoteException;

    /**
     * Remove a previously registered death notification.
     * The recipient will no longer be called if this object
     * dies.
     * 
     * @return {@code true} if the <var>recipient</var> is successfully
     * unlinked, assuring you that its
     * {@link DeathRecipient#binderDied DeathRecipient.binderDied()} method
     * will not be called;  {@code false} if the target IBinder has already
     * died, meaning the method has been (or soon will be) called.
     * 
     * @throws java.util.NoSuchElementException if the given
     * <var>recipient</var> has not been registered with the IBinder, and
     * the IBinder is still alive.  Note that if the <var>recipient</var>
     * was never registered, but the IBinder has already died, then this
     * exception will <em>not</em> be thrown, and you will receive a false
     * return value instead.
     */
    public boolean unlinkToDeath(DeathRecipient recipient, int flags);
}
```

注释有点多，我们一个一个来

##### (一)、类注释

简单的翻译一下：

-   一个远程对象的基接口，是为高性能而设计的轻量级远程调用机制的核心部分，它不仅用于远程调用，也可以用于进程内调用。这个接口描述了与远程对象进行交互的抽象协议。建议继承Binder类，而不是直接实现这个接口。 IBinder的主要API是transact()，与它对应另一个方法是Binder.onTransact。第一个方法使你可以向远端的IBinder对象发送调用，第二个方法使你自己的远程对象能够响应接收到的调用。IBinder的API都是同步执行的，比如transact()直到对方的Binder.onTransact()方法调用完成后才返回。而在跨进程的时候，在IPC的帮助下，也是同样的效果。 通过transact()发送的数据是Parcel，Parcel是一种一般的缓冲区，除了有数据外还带有一些描述它内容的元数据。元数据用于管理IBinder对象的引用，这样就能在缓冲去从一个进程移动到另一个进程时，保存这些引用。这样就保证了当一个IBinder被写入到Parcel并发送到另一个进程中，如果另一个进程把同一个IBinder的引用回发到原来的进程，那么这个原来的进程就能接收到发出的那个IBinder的引用。这种机制使IBinder和Binder像唯一标志符那样在进程间管理。 系统为每一个进程维持一个存放交互的线程池。这些交互的线程用于派发所有从其他进程发来的IPC调用。例如：当一个IPC从进程A发到进程B，A中那个发出调用的线程就阻塞在transact()中了。进程B中的交互线程池的一个线程池接收了这个调用，它调用Binder.onTransact()，完成后用一个Parcel来作为结果返回。然后进程A中的那个等待线程在收到返回的Parcel才能继续执行。实际上，另一个进程看起来就像当前进程的一个线程，但不是当前进程创建的。 Binder机制还支持进程间的递归调用。例如，进程A执行自己的IBinder的transact()调用进程B的Binder,而进程B在其Binder.onTransact()中又用transact()向进程A发起调用，那么进程A在等待它发布出的调用返回的同时，还会用Binder.onTransact()响应进程B的transact()。总之，Binder造成的结果就是让我们感觉到跨进程的调用与进程内的调用没有什么区别。 当操作远程对象的时候，你需要经常查看它们是否有效，有3种方法可以使用：
    -   1 transact()方法将在IBinder所在的进程不存在时抛出RemoteException异常
    -   2 如果目标进程不存在，那么调用pingBinder()时返回false\\
    -   3 可以用linkToDeath()方法向IBinder注册一个IBinder.DeathRecipient，在IBinder代表的进程退出时被调用。

##### (二)、重要方法注释

###### 1、queryLocalInterface(String descriptor) 方法

```
    /**
     * Attempt to retrieve a local implementation of an interface
     * for this Binder object.  If null is returned, you will need
     * to instantiate a proxy class to marshall calls through
     * the transact() method.
     */
    public IInterface queryLocalInterface(String descriptor);
```

翻译如下：

> 用于接收一个用于当前binder对象的本地接口的实现。如果返回null，你需要通过transact()方法去实例化一个代理类。

###### 2、dump(FileDescriptor fd, String\[\] args) 方法

```
    /**
     * Print the object's state into the given stream.
     * 
     * @param fd The raw file descriptor that the dump is being sent to.
     * @param args additional arguments to the dump request.
     */
    public void dump(FileDescriptor fd, String[] args) throws RemoteException;
```

翻译如下：

> 将对象状态打印入给定的数据流中.

-   入参fd:转储发送到的原始文件描述符。
-   入参args: 转储请求的附加参数

###### 3、dumpAsync(FileDescriptor fd, String\[\] args) 方法

```
    /**
     * Like {@link #dump(FileDescriptor, String[])} but always executes
     * asynchronously.  If the object is local, a new thread is created
     * to perform the dump.
     *
     * @param fd The raw file descriptor that the dump is being sent to.
     * @param args additional arguments to the dump request.
     */
    public void dumpAsync(FileDescriptor fd, String[] args) throws RemoteException;
```

翻译如下：

> 类似dump(FileDescriptor, String\[\])方法，但是总是异步执行。如果对象在本地， 一个新的线程将会被创建去执行这个操作。

-   入参fd:转储发送到的原始文件描述符。
-   入参args: 转储请求的附加参数

###### 4、shellCommand(FileDescriptor in, FileDescriptor out, FileDescriptor err,String\[\] args, ResultReceiver resultReceiver) 方法

```
    /**
     * Execute a shell command on this object.  This may be performed asynchrously from the caller;
     * the implementation must always call resultReceiver when finished.
     *
     * @param in The raw file descriptor that an input data stream can be read from.
     * @param out The raw file descriptor that normal command messages should be written to.
     * @param err The raw file descriptor that command error messages should be written to.
     * @param args Command-line arguments.
     * @param resultReceiver Called when the command has finished executing, with the result code.
     * @hide
     */
    public void shellCommand(FileDescriptor in, FileDescriptor out, FileDescriptor err,String[] args, ResultReceiver resultReceiver) throws RemoteException;
```

翻译如下：

> 对此对象执行shell命令。 可以异步执行; 执行完毕后必须始终调用resultReceiver。

-   入参in:可以读取输入数据流的原始文件描述符。
-   入参out: 正常命令消息应写入的原始文件描述符。
-   入参err:命令错误消息应写入的原始文件描述符。
-   入参args: 命令行参数。
-   入参resultReceiver: 当命令执行结束后，使用结果代码调用。

###### 5、transact(int code, Parcel data, Parcel reply, int flags)方法

```
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

翻译如下：

> 对对象执行一个通用的操作

-   入参code:操作码必须在FIRST\_CALL\_TRANSACTION和LAST\_CALL\_TRANSACTION之间
-   入参data: 传输给目标的数据。如果你没有传输任何数据，你必须创建一个空的Parcel
-   入参reply:从目标接收的数据。可以是null如果你对返回的值不感兴趣。
-   入参flags: 附加操作标志。0是指普通的RPC。或者FLAG\_ONEWAY，指单向RPC。

###### 6、linkToDeath(DeathRecipient recipient, int flags)方法

```
    /**
     * Register the recipient for a notification if this binder
     * goes away.  If this binder object unexpectedly goes away
     * (typically because its hosting process has been killed),
     * then the given {@link DeathRecipient}'s
     * {@link DeathRecipient#binderDied DeathRecipient.binderDied()} method
     * will be called.
     * 
     * <p>You will only receive death notifications for remote binders,
     * as local binders by definition can't die without you dying as well.
     * 
     * @throws RemoteException if the target IBinder's
     * process has already died.
     * 
     * @see #unlinkToDeath
     */
    public void linkToDeath(DeathRecipient recipient, int flags)
            throws RemoteException;
```

翻译如下：

> 注册一个recipient用于提示binder消失。如果binder对象异常地消失(例如由于主进程被杀)，DeathRecipient中的binderDied会被调用.

###### 7、unlinkToDeath(DeathRecipient recipient, int flags)方法

```
    /**
     * Remove a previously registered death notification.
     * The recipient will no longer be called if this object
     * dies.
     * 
     * @return {@code true} if the <var>recipient</var> is successfully
     * unlinked, assuring you that its
     * {@link DeathRecipient#binderDied DeathRecipient.binderDied()} method
     * will not be called;  {@code false} if the target IBinder has already
     * died, meaning the method has been (or soon will be) called.
     * 
     * @throws java.util.NoSuchElementException if the given
     * <var>recipient</var> has not been registered with the IBinder, and
     * the IBinder is still alive.  Note that if the <var>recipient</var>
     * was never registered, but the IBinder has already died, then this
     * exception will <em>not</em> be thrown, and you will receive a false
     * return value instead.
     */
    public boolean unlinkToDeath(DeathRecipient recipient, int flags);
}
```

翻译如下：

> 删除以前注册的死亡通知。 如果对象死亡，则recipient不再会被调用

##### (三)、内部接口注释

###### 1、DeathRecipient(int code, Parcel data, Parcel reply, int flags)方法

```
   /**
     * Interface for receiving a callback when the process hosting an IBinder
     * has gone away.
     * 
     * @see #linkToDeath
     */
    public interface DeathRecipient {
        public void binderDied();
    }
```

翻译如下：

> 当持有IBinder进程消失，会回调这个接口

##### (四)、总结：

通过上面对IBinder注释，我们大概可以知道以下信息

-   1、IBindre是远程对象的基接口，不仅可以在跨进程可以调用，也可以在进程内部调用
-   2、在远程调用的时候，一端用IBinder.transact()发送，另一端用Binder的Binder.onTransact()接受，并且是同步的
-   3、transact()方法发送的是Parcel。
-   4、系统为每个进程维护一个进行跨进程调用的线程池。
-   5、可以使用pingBinder()方法来检测目标进程是否存在
-   6、可以调用linkToDeath()来向IBinder注册一个IBinder.DeathRecipien。用于目标进程退出时候的提醒。
-   7、建议继承Binder类，而不是直接实现这个接口。

### 三、Binder与BinderProxy类

##### (一)、Binder

```
/**
 * Base class for a remotable object, the core part of a lightweight
 * remote procedure call mechanism defined by {@link IBinder}.
 * This class is an implementation of IBinder that provides
 * standard local implementation of such an object.
 *
 * <p>Most developers will not implement this class directly, instead using the
 * <a href="{@docRoot}guide/components/aidl.html">aidl</a> tool to describe the desired
 * interface, having it generate the appropriate Binder subclass.  You can,
 * however, derive directly from Binder to implement your own custom RPC
 * protocol or simply instantiate a raw Binder object directly to use as a
 * token that can be shared across processes.
 *
 * <p>This class is just a basic IPC primitive; it has no impact on an application's
 * lifecycle, and is valid only as long as the process that created it continues to run.
 * To use this correctly, you must be doing so within the context of a top-level
 * application component (a {@link android.app.Service}, {@link android.app.Activity},
 * or {@link android.content.ContentProvider}) that lets the system know your process
 * should remain running.</p>
 *
 * <p>You must keep in mind the situations in which your process
 * could go away, and thus require that you later re-create a new Binder and re-attach
 * it when the process starts again.  For example, if you are using this within an
 * {@link android.app.Activity}, your activity's process may be killed any time the
 * activity is not started; if the activity is later re-created you will need to
 * create a new Binder and hand it back to the correct place again; you need to be
 * aware that your process may be started for another reason (for example to receive
 * a broadcast) that will not involve re-creating the activity and thus run its code
 * to create a new Binder.</p>
 *
 * @see IBinder
 */
public class Binder implements IBinder {
    /*
     * Set this flag to true to detect anonymous, local or member classes
     * that extend this Binder class and that are not static. These kind
     * of classes can potentially create leaks.
     * 通过设置这个标记来检测是否是Binder的匿名内部类，或者Binder的
     * 本地内部类，因为这些类可能会存在潜在的内存泄露  handler里面也
     * 有这段话哦~
     */
    private static final boolean FIND_POTENTIAL_LEAKS = false;
    private static final boolean CHECK_PARCEL_SIZE = false;
    static final String TAG = "Binder";

    /** @hide */
    public static boolean LOG_RUNTIME_EXCEPTION = false; // DO NOT SUBMIT WITH TRUE

    /**
     * Control whether dump() calls are allowed.
     * 控制是否允许dump()方法的调用。
     */
    private static String sDumpDisabled = null;

    /**
     * Global transaction tracker instance for this process.
     * 进程的全局事务跟踪器实例。
     */
    private static TransactionTracker sTransactionTracker = null;

    // Transaction tracking code.
    // 事务跟踪代码

    /**
     * Flag indicating whether we should be tracing transact calls.
     * 标志， 表示我们是否应该跟踪事务调用。
     */
    private static boolean sTracingEnabled = false;

    /**
     * Enable Binder IPC tracing.
     * 启动 Binder IPC 跟踪
     * @hide
     */
    public static void  enableTracing() {
        sTracingEnabled = true;
    };

    /**
     * Disable Binder IPC tracing.
     * 关闭 Binder IPC 跟踪
     * @hide
     */
    public static void  disableTracing() {
        sTracingEnabled = false;
    }

    /**
     * Check if binder transaction tracing is enabled.
     * 检查 Binder的事务跟踪是否已启用
     * @hide
     */
    public static boolean isTracingEnabled() {
        return sTracingEnabled;
    }

    /**
     * Get the binder transaction tracker for this process.
     * 获取此进程的 Binder 事务跟踪器。
     * @hide
     */
    public synchronized static TransactionTracker getTransactionTracker() {
        if (sTransactionTracker == null)
            sTransactionTracker = new TransactionTracker();
        return sTransactionTracker;
    }

    /* mObject is used by native code, do not remove or rename */
    // mObject 会被Native 代码调用，不要删除或重命名
    private long mObject;
    private IInterface mOwner;
    private String mDescriptor;

    /**
     * Return the ID of the process that sent you the current transaction
     * that is being processed.  This pid can be used with higher-level
     * system services to determine its identity and check permissions.
     * If the current thread is not currently executing an incoming transaction,
     * then its own pid is returned.
     * 返回向您发送正在处理的当前事务的进程的ID。 该pid可以与更高级
     * 别的系统服务一起使用，以确定其身份和检查权限。 如果当前线程当
     * 前没有执行传入事务，则返回其自己的pid。
     */
    public static final native int getCallingPid();
    
    /**
     * Return the Linux uid assigned to the process that sent you the
     * current transaction that is being processed.  This uid can be used with
     * higher-level system services to determine its identity and check
     * permissions.  If the current thread is not currently executing an
     * incoming transaction, then its own uid is returned.
     * 将分配给您的Linux uid返回给向您发送正在处理的当前事务的进程。
     * 这个uid可以与更高级别的系统服务一起使用，以确定其身份和检查
     * 权限。 如果当前线程当前没有执行传入事务，则返回其自己的uid。
     */
    public static final native int getCallingUid();

    /**
     * Return the UserHandle assigned to the process that sent you the
     * current transaction that is being processed.  This is the user
     * of the caller.  It is distinct from {@link #getCallingUid()} in that a
     * particular user will have multiple distinct apps running under it each
     * with their own uid.  If the current thread is not currently executing an
     * incoming transaction, then its own UserHandle is returned.
     * 返回分配给发送给正在处理的当前事务的进程的UserHandle。 这是
     * 呼叫者的用户。 与getCallingUid()不同，特定用户将具有多个不同的
     * 应用程序，每个应用程序都具有自己的uid。 如果当前线程当前没有
     * 执行传入事务，则返回其自己的UserHandle。
     */
    public static final UserHandle getCallingUserHandle() {
        return UserHandle.of(UserHandle.getUserId(getCallingUid()));
    }

    /**
     * Reset the identity of the incoming IPC on the current thread.  This can
     * be useful if, while handling an incoming call, you will be calling
     * on interfaces of other objects that may be local to your process and
     * need to do permission checks on the calls coming into them (so they
     * will check the permission of your own local process, and not whatever
     * process originally called you).
     * 重置当前线程的IPC的身份。 如果在处理来电时，您将会调用
     * 其他对象的接口，这些对象可能在本地进程中，并且需要对其中的调
     * 用进行权限检查（不管原来的进程是如何调用你的，他们都将将检查
     * 您自己进程的的访问权限）。
     *  //最后一句话我实在是翻译不好
     * @return Returns an opaque token that can be used to restore the
     * original calling identity by passing it to
     * {@link #restoreCallingIdentity(long)}.
     *
     *  返回一个不透明的token，通过restoreCallingIdentity(long)这个方法
     *  可以恢复原始呼叫的身份标识
     * @see #getCallingPid()
     * @see #getCallingUid()
     * @see #restoreCallingIdentity(long)
     */
    public static final native long clearCallingIdentity();

    /**
     * Restore the identity of the incoming IPC on the current thread
     * back to a previously identity that was returned by {@link
     * #clearCallingIdentity}.
     *  恢复之前当前线程上的传入IPC的身份标识。这个身份标识是由
     *  clearCallingIdentity()方法改变的
     * @param token The opaque token that was previously returned by
     * {@link #clearCallingIdentity}.
     * token 参数是  以前由{@link #clearCallingIdentity}返回的 tocken
     *
     * @see #clearCallingIdentity
     */
    public static final native void restoreCallingIdentity(long token);

    /**
     * Sets the native thread-local StrictMode policy mask.
     *  设置 native层线程的StrictMode策略掩码。
     * <p>The StrictMode settings are kept in two places: a Java-level
     * threadlocal for libcore/Dalvik, and a native threadlocal (set
     * here) for propagation via Binder calls.  This is a little
     * unfortunate, but necessary to break otherwise more unfortunate
     * dependencies either of Dalvik on Android, or Android
     * native-only code on Dalvik.
     *
     *  StrictMode设置保存在两个地方：Java级别的 本地线程  在libcore / Dalvik中进行设置，
     * 和 native的 本地线程则通过Binder调用来设置。 这有点儿不幸，但总
     * 比依赖于Android上的Dalvik或者Android上Dalvik的native-only代码要好
     * @see StrictMode
     * @hide
     */
    public static final native void setThreadStrictModePolicy(int policyMask);

    /**
     * Gets the current native thread-local StrictMode policy mask.
     * 获取当前native 的StrictMode策略掩码
     * @see #setThreadStrictModePolicy
     * @hide
     */
    public static final native int getThreadStrictModePolicy();

    /**
     * Flush any Binder commands pending in the current thread to the kernel
     * driver.  This can be
     * useful to call before performing an operation that may block for a long
     * time, to ensure that any pending object references have been released
     * in order to prevent the process from holding on to objects longer than
     * it needs to.
     * 将当前线程中的Binder命令刷新到内核驱动程序中。这在执行可能会
     * 长时间阻塞的操作之前调用是有用的，因为这样可以确保已经释放了
     * 任何挂起的对象引用，以防止进程持续到比需要的对象更长的时间。
     */
    public static final native void flushPendingCommands();
    
    /**
     * Add the calling thread to the IPC thread pool.  This function does
     * not return until the current process is exiting.
    * 将调用线程添加到IPC线程池。 此方法在当前进程退出之前不返回。
     */
    public static final native void joinThreadPool();

    /**
     * Returns true if the specified interface is a proxy.
     * 如果指定的接口是代理，则返回true。
     * @hide
     */
    public static final boolean isProxy(IInterface iface) {
        return iface.asBinder() != iface;
    }

    /**
     * Call blocks until the number of executing binder threads is less
     * than the maximum number of binder threads allowed for this process.
     * 调用块直到执行绑定线程数量小于此进程允许的绑定线程的最大数量。
     * @hide
     */
    public static final native void blockUntilThreadAvailable();

    /**
     * Default constructor initializes the object.
     */
    public Binder() {
        init();

        if (FIND_POTENTIAL_LEAKS) {
            final Class<? extends Binder> klass = getClass();
            if ((klass.isAnonymousClass() || klass.isMemberClass() || klass.isLocalClass()) &&
                    (klass.getModifiers() & Modifier.STATIC) == 0) {
                Log.w(TAG, "The following Binder class should be static or leaks might occur: " +
                    klass.getCanonicalName());
            }
        }
    }
    
    /**
     * Convenience method for associating a specific interface with the Binder.
     * After calling, queryLocalInterface() will be implemented for you
     * to return the given owner IInterface when the corresponding
     * descriptor is requested.
     */
    public void attachInterface(IInterface owner, String descriptor) {
        mOwner = owner;
        mDescriptor = descriptor;
    }
    
    /**
     * Default implementation returns an empty interface name.
     */
    public String getInterfaceDescriptor() {
        return mDescriptor;
    }

    /**
     * Default implementation always returns true -- if you got here,
     * the object is alive.
     */
    public boolean pingBinder() {
        return true;
    }

    /**
     * {@inheritDoc}
     *
     * Note that if you're calling on a local binder, this always returns true
     * because your process is alive if you're calling it.
     */
    public boolean isBinderAlive() {
        return true;
    }
    
    /**
     * Use information supplied to attachInterface() to return the
     * associated IInterface if it matches the requested
     * descriptor.
     */
    public IInterface queryLocalInterface(String descriptor) {
        if (mDescriptor.equals(descriptor)) {
            return mOwner;
        }
        return null;
    }

    /**
     * Control disabling of dump calls in this process.  This is used by the system
     * process watchdog to disable incoming dump calls while it has detecting the system
     * is hung and is reporting that back to the activity controller.  This is to
     * prevent the controller from getting hung up on bug reports at this point.
     * @hide
     *
     * @param msg The message to show instead of the dump; if null, dumps are
     * re-enabled.
     */
    public static void setDumpDisabled(String msg) {
        synchronized (Binder.class) {
            sDumpDisabled = msg;
        }
    }

    /**
     * Default implementation is a stub that returns false.  You will want
     * to override this to do the appropriate unmarshalling of transactions.
     *
     * <p>If you want to call this, call transact().
     */
    protected boolean onTransact(int code, Parcel data, Parcel reply,
            int flags) throws RemoteException {
        if (code == INTERFACE_TRANSACTION) {
            reply.writeString(getInterfaceDescriptor());
            return true;
        } else if (code == DUMP_TRANSACTION) {
            ParcelFileDescriptor fd = data.readFileDescriptor();
            String[] args = data.readStringArray();
            if (fd != null) {
                try {
                    dump(fd.getFileDescriptor(), args);
                } finally {
                    IoUtils.closeQuietly(fd);
                }
            }
            // Write the StrictMode header.
            if (reply != null) {
                reply.writeNoException();
            } else {
                StrictMode.clearGatheredViolations();
            }
            return true;
        } else if (code == SHELL_COMMAND_TRANSACTION) {
            ParcelFileDescriptor in = data.readFileDescriptor();
            ParcelFileDescriptor out = data.readFileDescriptor();
            ParcelFileDescriptor err = data.readFileDescriptor();
            String[] args = data.readStringArray();
            ResultReceiver resultReceiver = ResultReceiver.CREATOR.createFromParcel(data);
            try {
                if (out != null) {
                    shellCommand(in != null ? in.getFileDescriptor() : null,
                            out.getFileDescriptor(),
                            err != null ? err.getFileDescriptor() : out.getFileDescriptor(),
                            args, resultReceiver);
                }
            } finally {
                IoUtils.closeQuietly(in);
                IoUtils.closeQuietly(out);
                IoUtils.closeQuietly(err);
                // Write the StrictMode header.
                if (reply != null) {
                    reply.writeNoException();
                } else {
                    StrictMode.clearGatheredViolations();
                }
            }
            return true;
        }
        return false;
    }

    /**
     * Implemented to call the more convenient version
     * {@link #dump(FileDescriptor, PrintWriter, String[])}.
     */
    public void dump(FileDescriptor fd, String[] args) {
        FileOutputStream fout = new FileOutputStream(fd);
        PrintWriter pw = new FastPrintWriter(fout);
        try {
            doDump(fd, pw, args);
        } finally {
            pw.flush();
        }
    }

    void doDump(FileDescriptor fd, PrintWriter pw, String[] args) {
        final String disabled;
        synchronized (Binder.class) {
            disabled = sDumpDisabled;
        }
        if (disabled == null) {
            try {
                dump(fd, pw, args);
            } catch (SecurityException e) {
                pw.println("Security exception: " + e.getMessage());
                throw e;
            } catch (Throwable e) {
                // Unlike usual calls, in this case if an exception gets thrown
                // back to us we want to print it back in to the dump data, since
                // that is where the caller expects all interesting information to
                // go.
                pw.println();
                pw.println("Exception occurred while dumping:");
                e.printStackTrace(pw);
            }
        } else {
            pw.println(sDumpDisabled);
        }
    }

    /**
     * Like {@link #dump(FileDescriptor, String[])}, but ensures the target
     * executes asynchronously.
     */
    public void dumpAsync(final FileDescriptor fd, final String[] args) {
        final FileOutputStream fout = new FileOutputStream(fd);
        final PrintWriter pw = new FastPrintWriter(fout);
        Thread thr = new Thread("Binder.dumpAsync") {
            public void run() {
                try {
                    dump(fd, pw, args);
                } finally {
                    pw.flush();
                }
            }
        };
        thr.start();
    }

    /**
     * Print the object's state into the given stream.
     * 
     * @param fd The raw file descriptor that the dump is being sent to.
     * @param fout The file to which you should dump your state.  This will be
     * closed for you after you return.
     * @param args additional arguments to the dump request.
     */
    protected void dump(FileDescriptor fd, PrintWriter fout, String[] args) {
    }

    /**
     * @param in The raw file descriptor that an input data stream can be read from.
     * @param out The raw file descriptor that normal command messages should be written to.
     * @param err The raw file descriptor that command error messages should be written to.
     * @param args Command-line arguments.
     * @param resultReceiver Called when the command has finished executing, with the result code.
     * @throws RemoteException
     * @hide
     */
    public void shellCommand(FileDescriptor in, FileDescriptor out, FileDescriptor err,
            String[] args, ResultReceiver resultReceiver) throws RemoteException {
        onShellCommand(in, out, err, args, resultReceiver);
    }

    /**
     * Handle a call to {@link #shellCommand}.  The default implementation simply prints
     * an error message.  Override and replace with your own.
     * <p class="caution">Note: no permission checking is done before calling this method; you must
     * apply any security checks as appropriate for the command being executed.
     * Consider using {@link ShellCommand} to help in the implementation.</p>
     * @hide
     */
    public void onShellCommand(FileDescriptor in, FileDescriptor out, FileDescriptor err,
            String[] args, ResultReceiver resultReceiver) throws RemoteException {
        FileOutputStream fout = new FileOutputStream(err != null ? err : out);
        PrintWriter pw = new FastPrintWriter(fout);
        pw.println("No shell command implementation.");
        pw.flush();
        resultReceiver.send(0, null);
    }

    /**
     * Default implementation rewinds the parcels and calls onTransact.  On
     * the remote side, transact calls into the binder to do the IPC.
     */
    public final boolean transact(int code, Parcel data, Parcel reply,
            int flags) throws RemoteException {
        if (false) Log.v("Binder", "Transact: " + code + " to " + this);

        if (data != null) {
            data.setDataPosition(0);
        }
        boolean r = onTransact(code, data, reply, flags);
        if (reply != null) {
            reply.setDataPosition(0);
        }
        return r;
    }
    
    /**
     * Local implementation is a no-op.
     */
    public void linkToDeath(DeathRecipient recipient, int flags) {
    }

    /**
     * Local implementation is a no-op.
     */
    public boolean unlinkToDeath(DeathRecipient recipient, int flags) {
        return true;
    }
    
    protected void finalize() throws Throwable {
        try {
            destroy();
        } finally {
            super.finalize();
        }
    }

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

    private native final void init();
    private native final void destroy();

    // Entry point from android_util_Binder.cpp's onTransact
    private boolean execTransact(int code, long dataObj, long replyObj,
            int flags) {
        Parcel data = Parcel.obtain(dataObj);
        Parcel reply = Parcel.obtain(replyObj);
        // theoretically, we should call transact, which will call onTransact,
        // but all that does is rewind it, and we just got these from an IPC,
        // so we'll just call it directly.
        boolean res;
        // Log any exceptions as warnings, don't silently suppress them.
        // If the call was FLAG_ONEWAY then these exceptions disappear into the ether.
        try {
            res = onTransact(code, data, reply, flags);
        } catch (RemoteException|RuntimeException e) {
            if (LOG_RUNTIME_EXCEPTION) {
                Log.w(TAG, "Caught a RuntimeException from the binder stub implementation.", e);
            }
            if ((flags & FLAG_ONEWAY) != 0) {
                if (e instanceof RemoteException) {
                    Log.w(TAG, "Binder call failed.", e);
                } else {
                    Log.w(TAG, "Caught a RuntimeException from the binder stub implementation.", e);
                }
            } else {
                reply.setDataPosition(0);
                reply.writeException(e);
            }
            res = true;
        } catch (OutOfMemoryError e) {
            // Unconditionally log this, since this is generally unrecoverable.
            Log.e(TAG, "Caught an OutOfMemoryError from the binder stub implementation.", e);
            RuntimeException re = new RuntimeException("Out of memory", e);
            reply.setDataPosition(0);
            reply.writeException(re);
            res = true;
        }
        checkParcel(this, code, reply, "Unreasonably large binder reply buffer");
        reply.recycle();
        data.recycle();

        // Just in case -- we are done with the IPC, so there should be no more strict
        // mode violations that have gathered for this thread.  Either they have been
        // parceled and are now in transport off to the caller, or we are returning back
        // to the main transaction loop to wait for another incoming transaction.  Either
        // way, strict mode begone!
        StrictMode.clearGatheredViolations();

        return res;
    }
}
```

###### 1、类注释

-   远程对象的基类，由IBinder定义的轻量级远程调用机制的核心部分。这个类是IBinder的实现类。它提供了这种对象的标准本地实现。
-   大多数开发人员不会直接使用这个类，而是使用 AIDL工具来实现这个接口，使其生成适当的Binder子类。 但是，您可以直接从Binder派生自己的自定义RPC协议，也可以直接实例化一个原始的Binder对象，把它当做一个token，来进行跨进程通信。
-   这个Binder类是一个基础的IPC原生类，它对application的生命周期没有影响的，它仅当创建它的进程还活着的时候才有效。所以为了正确的使用它，你必须在一个顶级的app组件里明确地让系统知道， 这个Binder类是一个基础的IPC原生类，它对applicant的生命周期没有影响，它仅当创建它的进程还活着的时候才有效。所以为了正确地使用它，你必须在一个顶级app组件(例如service、activity或者ContentProvider)里明确地让系统知道，您的进程应该保持运行。
-   你必须牢记你的进程可能会消失的情况，如果发生了这种情况，你必须在进程重启的时候创建一个新的Binder，并且关联这个进程。例如，如果你在Activity里面使用了Binder，你的Activity进程可能会被杀死，过了一会后，如果activity比重新启动了，这时候你要重新创建的一个新的Binder，并且把这个心的Binder放回之前的位置。你也要注意到是，你的进程可能因为一些原因(比如接收broadcast)而启动，在这种情况下，是不需要重新创建Activity的，这时候就需要运行其他的一些代码去创建Binder对象。

###### 2、关于Binder的构造函数

Binder就提供一个默认的构造函数,代码如下

```
    /**
     * Default constructor initializes the object.
     */
    public Binder() {
        init();

        if (FIND_POTENTIAL_LEAKS) {
            final Class<? extends Binder> klass = getClass();
            if ((klass.isAnonymousClass() || klass.isMemberClass() || klass.isLocalClass()) &&
                    (klass.getModifiers() & Modifier.STATIC) == 0) {
                Log.w(TAG, "The following Binder class should be static or leaks might occur: " +
                    klass.getCanonicalName());
            }
        }
    }
    private native final void init();
```

-   首先是执行init()方法，init()是native层的，这里就暂不跟踪了。
-   判断是否是匿名内部类，或者内部类，如果是的话，打印日志。

通过上面我们知道Binder这个类的核心构造函数是在native实现的。

###### 3、Binder的重要方法

###### (1)、attachInterface(IInterface, String)方法

```
    /**
     * Convenience method for associating a specific interface with the Binder.
     * After calling, queryLocalInterface() will be implemented for you
     * to return the given owner IInterface when the corresponding
     * descriptor is requested.
     * 将特定接口与Binder相关联的快捷方法。 调用之后，将会实现
     * queryLocalInterface(), 当你请求相应的描述符时，queryLocalInterface()
     * 将返回给定的所有者IInterface。
     */
    public void attachInterface(IInterface owner, String descriptor) {
        mOwner = owner;
        mDescriptor = descriptor;
    }
```

###### (2)、getInterfaceDescriptor()方法

```
    /**
     * Default implementation returns an empty interface name.
     * 默认实现返回一个空的接口名称。
     */
    public String getInterfaceDescriptor() {
        return mDescriptor;
    }
```

###### (3)、pingBinder()方法

```
    /**
     * Default implementation always returns true -- if you got here,
     * the object is alive.
     * 默认实现总是返回true - 如果你走到这里，对象是活着的。
     */
    public boolean pingBinder() {
        return true;
    }
```

###### (4)、isBinderAlive()方法

```
    /**
     * {@inheritDoc}
     *
     * Note that if you're calling on a local binder, this always returns true
     * because your process is alive if you're calling it.
     * 请注意，如果您正在调用本地的Binder，则始终返回true
     * 如果你能调用他，则你的进程一定是活着的。
     */
    public boolean isBinderAlive() {
        return true;
    }
```

###### (5)、queryLocalInterface(String)方法

```
    /**
     * Use information supplied to attachInterface() to return the
     * associated IInterface if it matches the requested
     * descriptor.
     * 如果提供的描述符和之前关联的IInterface(通过attachInterface()
     * 方法进行关联)的描述符一致，则返回相对应的IInterface
     */
    public IInterface queryLocalInterface(String descriptor) {
        if (mDescriptor.equals(descriptor)) {
            return mOwner;
        }
        return null;
    }
```

###### (6)、setDumpDisabled(String)方法

```
   /**
     * Control disabling of dump calls in this process.  This is used by the system
     * process watchdog to disable incoming dump calls while it has detecting the system
     * is hung and is reporting that back to the activity controller.  This is to
     * prevent the controller from getting hung up on bug reports at this point.
     * @hide
     *
     * @param msg The message to show instead of the dump; if null, dumps are
     * re-enabled.
     */
    public static void setDumpDisabled(String msg) {
        synchronized (Binder.class) {
            sDumpDisabled = msg;
        }
    }
```

##### (二)、BinderProxy

```
final class BinderProxy implements IBinder {
    public native boolean pingBinder();
    public native boolean isBinderAlive();

    public IInterface queryLocalInterface(String descriptor) {
        return null;
    }

    public boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
        Binder.checkParcel(this, code, data, "Unreasonably large binder buffer");
        if (Binder.isTracingEnabled()) { Binder.getTransactionTracker().addTrace(); }
        return transactNative(code, data, reply, flags);
    }

    public native String getInterfaceDescriptor() throws RemoteException;
    public native boolean transactNative(int code, Parcel data, Parcel reply,
            int flags) throws RemoteException;
    public native void linkToDeath(DeathRecipient recipient, int flags)
            throws RemoteException;
    public native boolean unlinkToDeath(DeathRecipient recipient, int flags);

    public void dump(FileDescriptor fd, String[] args) throws RemoteException {
        Parcel data = Parcel.obtain();
        Parcel reply = Parcel.obtain();
        data.writeFileDescriptor(fd);
        data.writeStringArray(args);
        try {
            transact(DUMP_TRANSACTION, data, reply, 0);
            reply.readException();
        } finally {
            data.recycle();
            reply.recycle();
        }
    }
    
    public void dumpAsync(FileDescriptor fd, String[] args) throws RemoteException {
        Parcel data = Parcel.obtain();
        Parcel reply = Parcel.obtain();
        data.writeFileDescriptor(fd);
        data.writeStringArray(args);
        try {
            transact(DUMP_TRANSACTION, data, reply, FLAG_ONEWAY);
        } finally {
            data.recycle();
            reply.recycle();
        }
    }

    public void shellCommand(FileDescriptor in, FileDescriptor out, FileDescriptor err,
            String[] args, ResultReceiver resultReceiver) throws RemoteException {
        Parcel data = Parcel.obtain();
        Parcel reply = Parcel.obtain();
        data.writeFileDescriptor(in);
        data.writeFileDescriptor(out);
        data.writeFileDescriptor(err);
        data.writeStringArray(args);
        resultReceiver.writeToParcel(data, 0);
        try {
            transact(SHELL_COMMAND_TRANSACTION, data, reply, 0);
            reply.readException();
        } finally {
            data.recycle();
            reply.recycle();
        }
    }

    BinderProxy() {
        mSelf = new WeakReference(this);
    }
    
    @Override
    protected void finalize() throws Throwable {
        try {
            destroy();
        } finally {
            super.finalize();
        }
    }
    
    private native final void destroy();
    
    private static final void sendDeathNotice(DeathRecipient recipient) {
        if (false) Log.v("JavaBinder", "sendDeathNotice to " + recipient);
        try {
            recipient.binderDied();
        }
        catch (RuntimeException exc) {
            Log.w("BinderNative", "Uncaught exception from death notification",
                    exc);
        }
    }
   
    final private WeakReference mSelf;
    private long mObject;
    private long mOrgue;
}
```

### 四、总结

所以大体的结构类图图下图:

![](https://ask.qcloudimg.com/http-save/yehe-2957818/nzlujg57vo.png)

Java层的Binder对象模型.png

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2017.08.04 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除