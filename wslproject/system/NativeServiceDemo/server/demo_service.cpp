/*
 * Copyright (C) 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "DemoService"

#include <android-base/logging.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include "com/demo/nativeservice/BnDemoService.h"

namespace com {
namespace demo {
namespace nativeservice {

class DemoService : public BnDemoService {
public:
    DemoService() : mVerbose(false) {}
    ~DemoService() override = default;

    binder::Status add(int32_t a, int32_t b, int32_t* _aidl_return) override {
        *_aidl_return = a + b;
        if (mVerbose) {
            LOG(INFO) << "add(" << a << ", " << b << ") = " << *_aidl_return;
        }
        return binder::Status::ok();
    }

    binder::Status getName(std::string* _aidl_return) override {
        *_aidl_return = "NativeDemoService";
        return binder::Status::ok();
    }

    binder::Status setVerbose(bool enable) override {
        mVerbose = enable;
        LOG(INFO) << "Verbose mode: " << (enable ? "ON" : "OFF");
        return binder::Status::ok();
    }

private:
    bool mVerbose;
};

}  // namespace nativeservice
}  // namespace demo
}  // namespace com

int main(int argc, char** argv) {
    
    android::sp<com::demo::nativeservice::DemoService> service =
            new com::demo::nativeservice::DemoService();

    android::sp<android::ProcessState> proc(android::ProcessState::self());
    android::defaultServiceManager()->addService(
            android::String16("demo_service"), service);

    LOG(INFO) << "DemoService started";

    android::IPCThreadState::self()->joinThreadPool();
    return 0;
}
