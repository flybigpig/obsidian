// client_main.cpp
// Binder 客户端入口
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <utils/Log.h>
#include <iostream>

#include "MyBinderClient.h"

#undef LOG_TAG
#define LOG_TAG "MyBinderClientMain"

using namespace android;

int main() {
    ALOGI("Starting MyBinderClient...");

    // 初始化 ProcessState
    sp<ProcessState> proc = ProcessState::self();

    // 创建客户端并连接服务
    MyBinderClient client;

    if (!client.connect()) {
        ALOGE("Failed to connect to service");
        return -1;
    }

    // 调用远程方法
    String16 name;
    if (client.getServiceName(name) == OK) {
        ALOGI("Service Name: %s", String8(name).string());
    }

    int32_t result = 0;
    if (client.add(100, 200, &result) == OK) {
        ALOGI("100 + 200 = %d", result);
    }

    client.sendMessage(String16("Hello from client!"));

    bool connected = false;
    if (client.isConnected(&connected) == OK) {
        ALOGI("Connected: %s", connected ? "true" : "false");
    }

    // 断开连接
    client.disconnect();

    ALOGI("Client finished");
    return 0;
}
