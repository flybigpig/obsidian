// service_main.cpp
// Binder 服务端入口
#include <binderbinder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>
#include <iostream>

#include "MyBinderService.h"

#undef LOG_TAG
#define LOG_TAG "MyBinderServiceMain"

using namespace android;

int main() {
    ALOGI("Starting MyBinderService...");

    // 初始化 ProcessState（单例，每个进程只需调用一次）
    sp<ProcessState> proc = ProcessState::self();
    
    // 获取 ServiceManager
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == nullptr) {
        ALOGE("Failed to get ServiceManager");
        return -1;
    }

    // 创建服务实例
    sp<MyBinderService> service = new MyBinderService();

    // 注册服务
    status_t status = sm->addService(
        String16("com.example.binder.IMyBinder"),
        service
    );

    if (status != OK) {
        ALOGE("Failed to register service, status = %d", status);
        return -1;
    }

    ALOGI("MyBinderService registered successfully");

    // 进入消息循环，处理 Binder 通信
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();

    return 0;
}
