// MyBinderClient.cpp
// Binder 客户端实现
#include "MyBinderClient.h"
#include <binder/IServiceManager.h>
#include <utils/Log.h>

#undef LOG_TAG
#define LOG_TAG "MyBinderClient"

namespace android {

MyBinderClient::MyBinderClient()
    : mConnected(false) {
    ALOGI("MyBinderClient constructed");
}

MyBinderClient::~MyBinderClient() {
    disconnect();
    ALOGI("MyBinderClient destroyed");
}

bool MyBinderClient::connect() {
    ALOGI("Connecting to MyBinderService...");
    
    // 获取服务
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == nullptr) {
        ALOGE("Failed to get IServiceManager");
        return false;
    }

    sp<IBinder> binder = sm->getService(String16("com.example.binder.IMyBinder"));
    if (binder == nullptr) {
        ALOGE("Failed to get service");
        return false;
    }

    // 检查服务是否存活
    if (binder->pingBinder()) {
        ALOGE("Service is dead");
        return false;
    }

    // 获取接口
    mService = interface_cast<IMyBinder>(binder);
    if (mService == nullptr) {
        ALOGE("Failed to cast to IMyBinder");
        return false;
    }

    mConnected = true;
    ALOGI("Connected successfully");
    return true;
}

void MyBinderClient::disconnect() {
    mService = nullptr;
    mConnected = false;
    ALOGI("Disconnected");
}

status_t MyBinderClient::getServiceName(String16& name) {
    if (!mConnected || mService == nullptr) {
        ALOGE("Not connected");
        return NO_INIT;
    }
    return mService->getServiceName(name);
}

status_t MyBinderClient::add(int32_t a, int32_t b, int32_t* result) {
    if (!mConnected || mService == nullptr) {
        ALOGE("Not connected");
        return NO_INIT;
    }
    return mService->add(a, b, result);
}

status_t MyBinderClient::sendMessage(const String16& message) {
    if (!mConnected || mService == nullptr) {
        ALOGE("Not connected");
        return NO_INIT;
    }
    return mService->sendMessage(message);
}

status_t MyBinderClient::isConnected(bool* connected) {
    if (!mConnected || mService == nullptr) {
        ALOGE("Not connected");
        return NO_INIT;
    }
    return mService->isConnected(connected);
}

} // namespace android
