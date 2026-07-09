// IMyBinderService.java
// 由 AIDL 编译器自动生成 (对应 IMyBinderService.aidl)
package com.example.binder;

import android.os.Binder;
import android.os.IInterface;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;

/**
 * Binder IPC 服务接口 - AIDL 生成代码
 * 对应文件: IMyBinderService.aidl
 */
public interface IMyBinderService extends IInterface {
    
    // ==================== 接口方法声明 ====================
    String getServiceName() throws RemoteException;
    int add(int a, int b) throws RemoteException;
    void sendMessage(String message) throws RemoteException;
    boolean isConnected() throws RemoteException;

    // ==================== 描述符 ====================
    static final java.lang.String DESCRIPTOR = "com.example.binder.IMyBinderService";

    // ==================== 事务码 ====================
    int TRANSACTION_getServiceName = android.os.IBinder.FIRST_CALL_TRANSACTION + 0;
    int TRANSACTION_add = android.os.IBinder.FIRST_CALL_TRANSACTION + 1;
    int TRANSACTION_sendMessage = android.os.IBinder.FIRST_CALL_TRANSACTION + 2;
    int TRANSACTION_isConnected = android.os.IBinder.FIRST_CALL_TRANSACTION + 3;

    // ==================== Stub (服务端抽象类) ====================
    public abstract static class Stub extends Binder implements IMyBinderService {
        
        private static final java.lang.String TAG = "IMyBinderService$Stub";
        
        public Stub() {
            this.attachInterface(this, DESCRIPTOR);
        }

        /**
         * 将 IBinder 转换为 IMyBinderService 接口
         */
        public static IMyBinderService asInterface(IBinder obj) {
            if (obj == null) return null;
            // 查询本地接口（同进程直接返回）
            IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
            if (iin != null && iin instanceof IMyBinderService) {
                return (IMyBinderService) iin;
            }
            // 跨进程返回代理
            return new Proxy(obj);
        }

        /**
         * 获取 Binder 对象
         */
        @Override
        public IBinder asBinder() {
            return this;
        }

        /**
         * 处理客户端请求 (onTransact)
         * @param code 事务码
         * @param data 输入参数 Parcel
         * @param reply 返回值 Parcel
         * @param flags 标志位
         */
        @Override
        public boolean onTransact(int code, Parcel data, Parcel reply, int flags)
                throws RemoteException {
            
            data.enforceInterface(DESCRIPTOR);
            
            switch (code) {
                case TRANSACTION_getServiceName: {
                    String _result = this.getServiceName();
                    reply.writeNoException();
                    reply.writeString(_result);
                    return true;
                }
                
                case TRANSACTION_add: {
                    int _arg0 = data.readInt();
                    int _arg1 = data.readInt();
                    int _result = this.add(_arg0, _arg1);
                    reply.writeNoException();
                    reply.writeInt(_result);
                    return true;
                }
                
                case TRANSACTION_sendMessage: {
                    String _arg0 = data.readString();
                    this.sendMessage(_arg0);
                    reply.writeNoException();
                    return true;
                }
                
                case TRANSACTION_isConnected: {
                    boolean _result = this.isConnected();
                    reply.writeNoException();
                    reply.writeInt((boolean)_result ? 1 : 0);
                    return true;
                }
                
                default:
                    return super.onTransact(code, data, reply, flags);
            }
        }

        // ==================== Proxy (客户端代理类) ====================
        private static class Proxy implements IMyBinderService {
            private IBinder mRemote;

            public Proxy(IBinder remote) {
                mRemote = remote;
            }

            @Override
            public IBinder asBinder() {
                return mRemote;
            }

            @Override
            public String getInterfaceDescriptor() {
                return DESCRIPTOR;
            }

            /**
             * 获取服务名称 (远程调用)
             */
            @Override
            public String getServiceName() throws RemoteException {
                Parcel _data = Parcel.obtain();
                Parcel _reply = Parcel.obtain();
                String _result;
                
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    boolean _status = mRemote.transact(TRANSACTION_getServiceName, _data, _reply, 0);
                    
                    if (!_status) {
                        throw new DeadObjectException("Binder died");
                    }
                    
                    _reply.readException();
                    _result = _reply.readString();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                
                return _result;
            }

            /**
             * 求和运算 (远程调用)
             */
            @Override
            public int add(int a, int b) throws RemoteException {
                Parcel _data = Parcel.obtain();
                Parcel _reply = Parcel.obtain();
                int _result;
                
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    _data.writeInt(a);
                    _data.writeInt(b);
                    
                    boolean _status = mRemote.transact(TRANSACTION_add, _data, _reply, 0);
                    
                    if (!_status) {
                        throw new DeadObjectException("Binder died");
                    }
                    
                    _reply.readException();
                    _result = _reply.readInt();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                
                return _result;
            }

            /**
             * 发送消息 (远程调用)
             */
            @Override
            public void sendMessage(String message) throws RemoteException {
                Parcel _data = Parcel.obtain();
                Parcel _reply = Parcel.obtain();
                
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    _data.writeString(message);
                    mRemote.transact(TRANSACTION_sendMessage, _data, _reply, 0);
                    _reply.readException();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
            }

            /**
             * 获取连接状态 (远程调用)
             */
            @Override
            public boolean isConnected() throws RemoteException {
                Parcel _data = Parcel.obtain();
                Parcel _reply = Parcel.obtain();
                boolean _result;
                
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    boolean _status = mRemote.transact(TRANSACTION_isConnected, _data, _reply, 0);
                    
                    if (!_status) {
                        throw new DeadObjectException("Binder died");
                    }
                    
                    _reply.readException();
                    _result = _reply.readInt() != 0;
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                
                return _result;
            }
        }
    }
}
