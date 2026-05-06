package com.example.binder;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

/**
 * Binder 客户端
 * 绑定服务并调用远程方法
 */
public class BinderClient {
    private static final String TAG = "BinderClient";
    private static final String PACKAGE = "com.example.binder";
    private static final String SERVICE_CLASS = "com.example.binder.BinderService";

    private Context mContext;
    private IMyBinderService mService;
    private boolean mBound = false;

    // ServiceConnection 回调
    private final ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            Log.d(TAG, "onServiceConnected: " + name);
            // 将 IBinder 转换为 AIDL 接口
            mService = IMyBinderService.Stub.asInterface(service);
            mBound = true;
            Log.d(TAG, "Service bound successfully");
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            Log.d(TAG, "onServiceDisconnected: " + name);
            mService = null;
            mBound = false;
        }
    };

    public BinderClient(Context context) {
        mContext = context;
    }

    /**
     * 绑定服务
     */
    public void bindService() {
        Intent intent = new Intent();
        intent.setClassName(PACKAGE, SERVICE_CLASS);
        // FLAG_NOT_FOREGROUND: 不创建前台服务通知
        mContext.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
    }

    /**
     * 解绑服务
     */
    public void unbindService() {
        if (mBound) {
            mContext.unbindService(mConnection);
            mBound = false;
        }
    }

    /**
     * 调用远程方法：获取服务名称
     */
    public String getServiceName() {
        if (!mBound || mService == null) {
            Log.e(TAG, "Service not bound!");
            return null;
        }
        try {
            return mService.getServiceName();
        } catch (RemoteException e) {
            Log.e(TAG, "RemoteException in getServiceName", e);
            return null;
        }
    }

    /**
     * 调用远程方法：计算两数之和
     */
    public int add(int a, int b) {
        if (!mBound || mService == null) {
            Log.e(TAG, "Service not bound!");
            return -1;
        }
        try {
            return mService.add(a, b);
        } catch (RemoteException e) {
            Log.e(TAG, "RemoteException in add", e);
            return -1;
        }
    }

    /**
     * 调用远程方法：发送消息
     */
    public void sendMessage(String message) {
        if (!mBound || mService == null) {
            Log.e(TAG, "Service not bound!");
            return;
        }
        try {
            mService.sendMessage(message);
        } catch (RemoteException e) {
            Log.e(TAG, "RemoteException in sendMessage", e);
        }
    }

    /**
     * 检查连接状态
     */
    public boolean isConnected() {
        if (!mBound || mService == null) {
            return false;
        }
        try {
            return mService.isConnected();
        } catch (RemoteException e) {
            Log.e(TAG, "RemoteException in isConnected", e);
            return false;
        }
    }

    /**
     * 是否已绑定
     */
    public boolean isBound() {
        return mBound;
    }
}
