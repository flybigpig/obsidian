<think>
用户想要用 C++ 语言创建一个 Native Binder 通信示例，类似于之前创建的 Android AIDL 版本，但是使用 C++ 语言在 Native 层实现。

这需要创建：
1. BnInterface 和 BnMyBinder（服务端）
2. BpInterface 和 BpMyBinder（客户端代理）
3. IInterface 相关宏
4. Parcel 数据序列化
5. service 和 client 可执行文件

让我创建一个完整的 Native Binder 示例。
</think>



<think>
所有文件已创建。让我给用户一个清晰的总结。
</think>

已创建 Native Binder C++ 通信示例，位于 `/home/fly/work/ipc/binder/`:

```
binder/
├── IMyBinder.h          # 接口基类（BnInterface/BpInterface）
├── IMyBinder.cpp        # onTransact 实现
├── TransactCode.h       # 事务码定义
├── MyBinderService.h    # 服务端头文件
├── MyBinderService.cpp  # 服务端实现
├── MyBinderClient.h     # 客户端头文件
├── MyBinderClient.cpp   # 客户端实现
├── service_main.cpp     # 服务端入口
├── client_main.cpp      # 客户端入口
├── Android.bp          # Soong 构建配置
└── Android.mk          # Legacy 构建配置
```

## 核心架构

```
┌─────────────────┐         Parcel          ┌─────────────────┐
│  BpMyBinder     │◄──── (序列化/反序列化) ──►│  BnMyBinder     │
│  (客户端代理)    │         Binder         │  (服务端)        │
└────────┬────────┘                          └────────┬────────┘
         │                                             │
         │ asInterface()                               │ handleXxx()
         ▼                                             ▼
┌─────────────────┐                          ┌─────────────────┐
│  IBinder        │                          │  MyBinderService│
│  (跨进程引用)    │                          │  (具体业务逻辑)  │
└─────────────────┘                          └─────────────────┘
```

## 关键代码

**服务端实现：**
```cpp
class MyBinderService : public IMyBinder::BnMyBinder {
    status_t handleAdd(int32_t a, int32_t b, int32_t* result) override {
        *result = a + b;
        return OK;
    }
};

// 注册服务
sm->addService(String16("com.example.binder.IMyBinder"), service);
```

**客户端调用：**
```cpp
MyBinderClient client;
client.connect();
int32_t result;
client.add(100, 200, &result);  // 调用远程方法
```

## 编译运行

```bash
# Android 环境
m mm IMyBinder.cpp MyBinderService.cpp MyBinderClient.cpp
m mm mybinder_service mybinder_client

# 运行服务
./mybinder_service &
# 运行客户端
./mybinder_client
```