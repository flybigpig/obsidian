// MyBinderClient.h
#ifndef MYBINDERCLIENT_H
#define MYBINDERCLIENT_H

#include "IMyBinder.h"
#include <utils/Thread.h>

namespace android {

// 客户端封装类
class MyBinderClient : public RefBase {
public:
    MyBinderClient();
    ~MyBinderClient();

    // 连接服务
    bool connect();
    // 断开连接
    void disconnect();

    // 调用远程方法
    status_t getServiceName(String16& name);
    status_t add(int32_t a, int32_t b, int32_t* result);
    status_t sendMessage(const String16& message);
    status_t isConnected(bool* connected);

    // 检查连接状态
    bool isConnected() const { return mConnected; }

private:
    sp<IMyBinder> mService;
    bool mConnected;
};

} // namespace android

#endif // MYBINDERCLIENT_H
