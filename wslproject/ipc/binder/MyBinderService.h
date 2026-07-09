// MyBinderService.h
#ifndef MYBINDERSERVICE_H
#define MYBINDERSERVICE_H

#include "IMyBinder.h"
#include "TransactCode.h"

namespace android {

// 服务端实现
class MyBinderService : public IMyBinder::BnMyBinder {
public:
    MyBinderService();
    virtual ~MyBinderService();

    // 实现 BnMyBinder 的虚函数
    virtual status_t handleGetServiceName(String16& name) override;
    virtual status_t handleAdd(int32_t a, int32_t b, int32_t* result) override;
    virtual status_t handleSendMessage(const String16& message) override;
    virtual status_t handleIsConnected(bool* connected) override;

    // IBinder 接口
    virtual IBinder* asBinder() override { return this; }
    virtual const IBinder* asBinder() const override { return this; }

private:
    String16 mServiceName;
    String16 mReceivedMessage;
    bool mConnected;
};

} // namespace android

#endif // MYBINDERSERVICE_H
