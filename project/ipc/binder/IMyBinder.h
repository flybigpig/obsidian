// IMyBinder.h
// Binder 接口定义（类似 AIDL）
#ifndef IMYBINDER_H
#define IMYBINDER_H

#include <binder/IBinder.h>
#include <binder/IInterface.h>

namespace android {

// 前向声明
class BpMyBinder;

// 描述符
static const String16 DESCRIPTOR("com.example.binder.IMyBinder");

// Oneway 接口声明（可选，用于不需要响应的情况）
class IMyBinder : public IInterface {
public:
    // 声明 BpBinder 代理类会调用的方法
    virtual status_t getServiceName(String16& name) = 0;
    virtual status_t add(int32_t a, int32_t b, int32_t* result) = 0;
    virtual status_t sendMessage(const String16& message) = 0;
    virtual status_t isConnected(bool* connected) = 0;

    // 获取 Binder 对象
    virtual IBinder* asBinder() = 0;
    virtual const IBinder* asBinder() const = 0;

    // BpBinder 代理类
    class BpMyBinder : public BpInterface<IMyBinder> {
    public:
        BpMyBinder(const sp<IBinder>& impl);

        virtual status_t getServiceName(String16& name) override;
        virtual status_t add(int32_t a, int32_t b, int32_t* result) override;
        virtual status_t sendMessage(const String16& message) override;
        virtual status_t isConnected(bool* connected) override;

        virtual IBinder* asBinder() override { return this; }
        virtual const IBinder* asBinder() const override { return this; }
    };

    // BnBinder 服务端类 - 需要用户继承实现
    class BnMyBinder : public BnInterface<IMyBinder> {
    public:
        BnMyBinder() {}
        virtual status_t onTransact(uint32_t code, const Parcel& data,
                                    Parcel* reply, uint32_t flags = 0) override;

    protected:
        // 子类需要实现的虚函数
        virtual status_t handleGetServiceName(String16& name) = 0;
        virtual status_t handleAdd(int32_t a, int32_t b, int32_t* result) = 0;
        virtual status_t handleSendMessage(const String16& message) = 0;
        virtual status_t handleIsConnected(bool* connected) = 0;
    };

    // 辅助函数：获取服务
    static sp<IMyBinder> getService();
    // 辅助函数：检查接口是否相同
    static bool isMyBinder(const sp<IBinder>& binder);
};

// IMPLEMENT_META_INTERFACE 宏定义（类似 AIDL 生成）
#define IMPLEMENT_MY_BINDER_INTERFACE()                           \
    android::sp<android::IMyBinder> IMyBinder::getService() {       \
        android::sp<android::IBinder> binder =                     \
            defaultServiceManager()->getService(DESCRIPTOR);       \
        return interface_cast<IMyBinder>(binder);                  \
    }                                                                \
    bool IMyBinder::isMyBinder(const sp<IBinder>& binder) {        \
        return binder->queryLocalInterface(DESCRIPTOR) != nullptr; \
    }

} // namespace android

#endif // IMYBINDER_H
