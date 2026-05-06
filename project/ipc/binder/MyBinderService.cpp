// MyBinderService.cpp
// Binder 服务端实现
#include "MyBinderService.h"
#include <binder/IServiceManager.h>
#include <utils/Log.h>

#undef LOG_TAG
#define LOG_TAG "MyBinderService"

namespace android {

MyBinderService::MyBinderService()
    : mServiceName(String16("MyBinderService")),
      mConnected(false) {
    ALOGI("MyBinderService constructed");
}

MyBinderService::~MyBinderService() {
    ALOGI("MyBinderService destroyed");
}

status_t MyBinderService::handleGetServiceName(String16& name) {
    ALOGI("handleGetServiceName");
    name = mServiceName;
    return OK;
}

status_t MyBinderService::handleAdd(int32_t a, int32_t b, int32_t* result) {
    ALOGI("handleAdd: %d + %d", a, b);
    *result = a + b;
    return OK;
}

status_t MyBinderService::handleSendMessage(const String16& message) {
    ALOGI("handleSendMessage: %s", String8(message).string());
    mReceivedMessage = message;
    mConnected = true;
    return OK;
}

status_t MyBinderService::handleIsConnected(bool* connected) {
    ALOGI("handleIsConnected: %s", mConnected ? "true" : "false");
    *connected = mConnected;
    return OK;
}

} // namespace android
