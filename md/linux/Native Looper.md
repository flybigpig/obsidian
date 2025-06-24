
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






### Native Looper 与 Java Looper 的协同机制及应用场景

#### **一、Native Looper 与 Java Looper 的关系**

Native Looper（C++ 层）与 Java Looper（Java 层）是 Android 消息循环体系的**上下层实现**，二者通过 **JNI 桥接** 协同工作：

  

1. **架构分层**
    
    plaintext
    
    ```plaintext
    Java 层（android.os.Looper）
        ↓ JNI 调用
    Native 层（system/core/libutils/Looper.cpp）
        ↓ 底层实现
    Linux 内核（epoll/poll 事件监听）
    ```
    
      
    
2. **关键关联**
    
    - Java 层的 `Looper` 类通过 `nativeInit()`、`nativePollOnce()` 等 JNI 方法调用 Native Looper 的功能。
    - Native Looper 负责**事件监听**（如文件描述符、定时器）和**底层消息处理**，Java Looper 则封装上层逻辑（如 `MessageQueue`、`Handler`）。

#### **二、协同工作流程**

以 `Handler.sendMessage()` 为例：

  

1. **Java 层发送消息**
    
    java
    
    ```java
    handler.sendMessage(message); // 调用 Handler.enqueueMessage()
    ```
    
      
    
2. **JNI 桥接至 Native 层**
    
    - `MessageQueue.enqueueMessage()` 调用 `nativeEnqueueMessage()` JNI 方法。
    - Native 层将消息插入 `Looper` 的消息队列，并通过 `epoll_ctl()` 触发事件通知。
3. **Native Looper 处理事件**
    
    - `Looper::pollOnce()` 监听事件（如消息队列中有新消息）。
    - 取出消息并回调 Java 层的 `MessageQueue.nativePollOnce()`，触发消息处理。
4. **Java 层执行消息**
    
    - `MessageQueue.next()` 获取消息，调用 `Handler.dispatchMessage()` 执行回调。

#### **三、Native Looper 核心机制**

#### 1. **事件监听模型**

- 基于 **epoll/poll** 实现高效的 I/O 多路复用，支持监听：
    - 文件描述符（如 Socket、管道）
    - 定时器事件（通过 `timerfd_create()`）
    - 自定义事件（通过 `eventfd_create()`）



运行

```cpp
// Native Looper 添加文件描述符监听示例
int fd = eventfd(0, EFD_NONBLOCK);
looper->addFd(fd, 0, ALOOPER_EVENT_INPUT, [](int fd, int events, void* data) {
    uint64_t value;
    read(fd, &value, sizeof(value));
    // 处理事件...
    return 1; // 继续监听
}, nullptr);
```

#### 2. **消息队列与同步机制**

- 消息队列（`MessageQueue`）通过双向链表存储消息，按时间戳排序。
- 同步屏障（`sync barrier`）机制：通过 `Looper::postSyncBarrier()` 插入特殊消息，优先处理异步消息（如输入事件）。

#### **四、典型应用场景**

#### 1. **系统服务底层通信**

- **SurfaceFlinger**：接收应用的绘制请求（通过 `Client` 端 Socket），由 Native Looper 监听并触发合成流程。
- **AudioFlinger**：处理音频数据传输，通过 Native Looper 监听音频驱动的 I/O 事件。

#### 2. **JNI 桥接与性能优化**

- 在高性能场景（如游戏、视频解码）中，通过 Native Looper 直接处理事件，避免 Java 层 GC 开销。
- 示例：自定义 Native 线程处理视频帧解码，通过 `ALooper` 与 Java 层通信：
    
    cpp
    
    运行
    
    ```cpp
    // Native 层创建 Looper 并注册回调
    sp<ALooper> looper = ALooper::getForThread();
    if (looper == nullptr) {
        looper = new ALooper;
        looper->prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    }
    ```
    
      
    

#### 3. **Choreographer 底层支持**

- Choreographer 的 VSync 信号接收（通过 `FrameDisplayEventReceiver`）依赖 Native Looper 监听硬件事件。

#### **五、Native Looper 与 Java Looper 的对比**

|特性|Native Looper（C++）|Java Looper（Java）|
|---|---|---|
|**性能**|更高（无 JNI 开销、直接操作底层）|较低（需跨层调用）|
|**适用场景**|系统服务、高性能模块、底层通信|应用层逻辑、UI 线程消息处理|
|**编程复杂度**|较高（需手动管理资源、同步）|较低（封装完善，提供 `Handler`）|
|**异常处理**|不支持（需手动处理错误码）|支持 Java 异常机制|

#### **六、实战：Native Looper 自定义线程**

以下是一个 Native Looper 自定义线程的示例，实现与 Java 层的双向通信：

  

cpp

运行

```cpp
// NativeLooperThread.cpp
#include <jni.h>
#include <utils/Looper.h>
#include <utils/Log.h>
#include <android_runtime/AndroidRuntime.h>

using namespace android;

// Java 回调方法 ID
jmethodID gCallbackMethod = nullptr;
jobject gCallbackObj = nullptr;

// Native Looper 消息处理回调
int looperCallback(int fd, int events, void* data) {
    JNIEnv* env =AndroidRuntime::getJNIEnv();
    if (env && gCallbackMethod && gCallbackObj) {
        env->CallVoidMethod(gCallbackObj, gCallbackMethod);
    }
    return 1; // 继续监听
}

// JNI 初始化函数
extern "C" JNIEXPORT void JNICALL
Java_com_example_NativeLooper_init(JNIEnv* env, jclass clazz, jobject callback) {
    gCallbackObj = env->NewGlobalRef(callback);
    jclass callbackClass = env->GetObjectClass(callback);
    gCallbackMethod = env->GetMethodID(callbackClass, "onEvent", "()V");
}

// 启动 Native Looper 线程
extern "C" JNIEXPORT void JNICALL
Java_com_example_NativeLooper_start(JNIEnv* env, jclass clazz) {
    // 创建线程
    pthread_t thread;
    pthread_create(&thread, nullptr, [](void* arg) {
        // 为线程创建 Looper
        sp<Looper> looper = Looper::prepare(0);
        
        // 创建事件文件描述符
        int eventFd = eventfd(0, EFD_NONBLOCK);
        if (eventFd < 0) {
            ALOGE("Failed to create eventfd");
            return nullptr;
        }
        
        // 添加监听
        looper->addFd(eventFd, 0, ALOOPER_EVENT_INPUT, looperCallback, nullptr);
        
        ALOGD("Native Looper started");
        looper->loop(); // 启动消息循环
        
        close(eventFd);
        return nullptr;
    }, nullptr);
}

// 向 Native Looper 发送事件
extern "C" JNIEXPORT void JNICALL
Java_com_example_NativeLooper_postEvent(JNIEnv* env, jclass clazz) {
    sp<Looper> looper = Looper::getForThread();
    if (looper) {
        // 此处简化处理，实际需通过 fd 发送事件
        ALOGD("Post event to Native Looper");
    }
}
```

  

  
  

**Java 层调用示例**：

  

java

```java
public class NativeLooper {
    static {
        System.loadLibrary("native-looper");
    }
    
    private native void init(OnEventListener listener);
    private native void start();
    private native void postEvent();
    
    public interface OnEventListener {
        void onEvent();
    }
}
```

  
#### **七、总结**

Native Looper 作为 Android 系统的底层事件循环机制，与 Java Looper 形成互补：前者提供高效的底层通信能力，后者封装易用的上层接口。理解二者的协同原理，有助于开发高性能的系统组件、优化跨层通信效率，并深入分析系统卡顿、ANR 等问题的底层原因。