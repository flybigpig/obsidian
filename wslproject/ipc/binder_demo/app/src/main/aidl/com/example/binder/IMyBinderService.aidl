// IMyBinderService.aidl
package com.example.binder;

// 声明接口
interface IMyBinderService {
    // 获取服务名称
    String getServiceName();

    // 求和运算
    int add(int a, int b);

    // 发送消息
    void sendMessage(String message);

    // 获取连接状态
    boolean isConnected();
}
