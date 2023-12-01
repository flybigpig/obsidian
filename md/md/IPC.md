# IPC



### AIDL

我们所写的aidl文件会在gen目录下自动生成对应的java文件，这个类中继承了IInterface这个接口，同时它自己也还是个接口。里面的有个内部类Stub，这个Stub就是Binder类，当客户端和服务端都位于同一个进程时，方法调用不会走跨进程的transact过程，当两者位于不同进程，方法调用走transact过程，这个逻辑由Stub的内部代理类Proxy来完成



> DESCRIPTOR
> Binder的唯一标识，一般用当前Binder的类名表示。
> asInterface(android.os.IBinder obj)
> 用于将服务端的Binder对象转换成客户端所需要的AIDL接口类型对象，这种转换过程是区分进程的，如果客户端和服务端用于同一进程，那么此对象返回就是服务端的Stub对象本身，否则返回的就是系统封装后的Stub.proxy
> asBinder
> 此方法用于返回当前Binder对象
> onTransact
> 这个方法运行在服务端中的Binder线程池中，当客户端发起跨进程请求时，远程请求会通过系统底层封装后交由此方法来处理。这个方法的原型是public Boolean onTransact(int code, Parcelable data, Parcelable reply, int flags)服务端通过code可以知道客户端请求的目标方法，接着从data中取出所需的参数，然后执行目标方法，执行完毕之后，将结果写入到reply中。如果此方法返回false，说明客户端的请求失败，利用这个特性可以做权限验证(即验证是否有权限调用该服务)。
> Proxy#[Method]
> 代理类中的接口方法，这些方法运行在客户端，当客户端远程调用此方法时，它的内部实现是：首先创建该方法所需要的输入型Parcel对象_data、输入型Parcel对象_reply和返回值对象List，然后把方法的参数信息写入到_data中，接着调用transact方法来发起RPC（远程调用）请求，同时当前线程挂起；然后服务端的onTransact方法会被调用，直到RPC过程返回后，当前线程继续执行，并从_reply中取出RPC过程的返回结果，最后返回_reply中的数据。



