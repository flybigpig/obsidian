
```cpp
#include <utils/Looper.h>
#include <utils/Log.h>
#include <unistd.h>

using namespace android;

// 自定义消息处理类
class MyHandler : public MessageHandler {
public:
    virtual void handleMessage(const Message& message) {
        switch (message.what) {
            case 1:
                ALOGD("Received message: %s", (const char*)message.obj);
                break;
            default:
                ALOGD("Unknown message: %d", message.what);
        }
    }
};

// 线程函数
void* threadFunc(void* arg) {
    // 为当前线程创建 Looper
    sp<Looper> looper = Looper::prepare(0);
    
    // 创建消息处理器
    sp<MyHandler> handler = new MyHandler();
    
    // 发送消息
    Message msg;
    msg.what = 1;
    msg.obj = strdup("Hello from Native Looper!");
    handler->sendMessageDelayed(1000, msg); // 延迟1秒发送
    
    // 启动消息循环
    while (true) {
        // 处理消息（返回值为处理的消息数量）
        int result = looper->pollOnce(-1); // -1表示无限等待
        if (result < 0) {
            ALOGD("Polling finished: %d", result);
            break;
        }
    }
    
    return nullptr;
}

// 主函数
int main() {
    pthread_t thread;
    pthread_create(&thread, nullptr, threadFunc, nullptr);
    
    // 主线程等待子线程结束
    pthread_join(thread, nullptr);
    return 0;
}

-------------------------------------------------------------------------------------

### **四、与 Java Looper 的交互**

Java 层与 Native Looper 可通过 JNI 通信，典型场景：

  

1. **Java 调用 Native**：
    
    - 通过 `Looper.nativeInit()` 创建 Native Looper 实例。
    - Java 的 `MessageQueue` 通过 JNI 调用 Native Looper 的 `enqueueMessage()` 方法。
    
2. **Native 回调 Java**：
    
    - Native 层通过 `JNIEnv` 调用 Java 方法，触发 Java 层的消息处理。