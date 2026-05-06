package com.example.binder;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

/**
 * Binder 服务端 Service
 * 继承 Stub（自动生成），实现 AIDL 接口
 */
public class BinderService extends Service {
    private static final String TAG = "BinderService";

    private final IMyBinderService.Stub mBinder = new IMyBinderService.Stub() {
        private String mServiceName = "BinderService";
        private String mReceivedMessage = "";
        private boolean mConnected = false;

        @Override
        public String getServiceName() throws RemoteException {
            Log.d(TAG, "getServiceName() called");
            return mServiceName;
        }

        @Override
        public int add(int a, int b) throws RemoteException {
            Log.d(TAG, "add(" + a + ", " + b + ")");
            return a + b;
        }

        @Override
        public void sendMessage(String message) throws RemoteException {
            Log.d(TAG, "sendMessage: " + message);
            mReceivedMessage = message;
            mConnected = true;
        }

        @Override
        public boolean isConnected() throws RemoteException {
            Log.d(TAG, "isConnected: " + mConnected);
            return mConnected;
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "onBind: " + intent);
        return mBinder;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d(TAG, "onCreate");
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy");
    }
}
