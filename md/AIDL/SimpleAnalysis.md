```
// 编译后生成的 Stub 类核心方法
public interface IMyService extends android.os.IInterface {
    // 服务端实现类
    public abstract class Stub extends android.os.Binder implements IMyService {
        // ★ 关键：asInterface - 客户端获取代理对象
        public static IMyService asInterface(android.os.IBinder obj) {
            if ((obj == null)) { return null; }
            // 先查询本地缓存
            android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
            if (((iin != null) && (iin instanceof IMyService))) {
                return ((IMyService) iin); // 同进程，直接返回
            }
            return new Proxy(obj); // 跨进程，返回代理对象
        }
        
        // ★ Proxy 内部类 - 客户端的代理
        private static class Proxy implements IMyService {
            private android.os.IBinder mRemote;
            
            @Override  // 同步调用
            public String getData() throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                String _result;
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    // ★ transac 同步调用，会阻塞等待 reply
                    boolean _status = mRemote.transact(Stub.TRANSACTION_getData, 
                                                       _data, _reply, 0);
                    // ... 读取 _reply 结果 ...
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                return _result;
            }
        }

        // ★ 服务端 onTransact 接收请求
        @Override
        public boolean onTransact(int code, android.os.Parcel data,
                                  android.os.Parcel reply, int flags) {
            switch (code) {
                case TRANSACTION_getData: {
                    data.enforceInterface(DESCRIPTOR);
                    String _result = this.getData(); // 调用服务端实现
                    reply.writeNoException();
                    reply.writeString(_result);
                    return true;
                }
            }
        }
    }
}

```
