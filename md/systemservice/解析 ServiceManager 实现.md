在 Android 系统中，`ServiceManager` 是一个用于管理系统中所有 Binder 服务的重要组件。`ServiceManager` 本身有对应的实现类，下面分别从 Java 和 C++ 层面来介绍其相关实现类和关键方法的实现。

  
  

在 Java 中，`ServiceManager` 类位于 `android.os` 包下，它提供了静态方法来与底层的 C++ 实现进行交互，从而完成服务的注册、查询等操作。

  
  

```
package android.os;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.os.IBinder.DeathRecipient;
import android.os.ServiceManagerNative;
import android.os.ServiceManagerProxy;
import java.util.Objects;

public final class ServiceManager {
    private static final String TAG = "ServiceManager";

    private static IServiceManager sServiceManager;
    private static Object sServiceManagerLock = new Object();

    // 获取 IServiceManager 实例
    private static IServiceManager getIServiceManager() {
        synchronized (sServiceManagerLock) {
            if (sServiceManager != null) {
                return sServiceManager;
            }
            // 通过 native 方法获取 Binder 对象
            sServiceManager = ServiceManagerNative
                   .asInterface(Binder.allowBlocking(BinderInternal.getContextObject()));
            return sServiceManager;
        }
    }

    // 向 ServiceManager 注册服务
    public static void addService(@NonNull String name, @Nullable IBinder service, boolean allowIsolated, int dumpPriority) {
        try {
            getIServiceManager().addService(name, service, allowIsolated, dumpPriority);
        } catch (RemoteException e) {
            Log.e(TAG, "error in addService", e);
        }
    }

    // 从 ServiceManager 获取服务
    public static @Nullable IBinder getService(@NonNull String name) {
        try {
            IBinder service = getIServiceManager().getService(name);
            if (service != null) {
                return service;
            } else {
                return Binder.allowBlocking(findService(name));
            }
        } catch (RemoteException e) {
            Log.e(TAG, "error in getService", e);
        }
        return null;
    }

    // 查找服务
    private static @Nullable IBinder findService(@NonNull String name) {
        try {
            return getIServiceManager().checkService(name);
        } catch (RemoteException e) {
            Log.e(TAG, "error in checkService", e);
        }
        return null;
    }

    // 获取所有服务的名称
    public static String[] listServices(int dumpPriority) throws RemoteException {
        return getIServiceManager().listServices(dumpPriority);
    }
}
```

  
  

-   **`getIServiceManager` 方法**：这是一个获取 `IServiceManager` 实例的私有方法，采用单例模式，确保全局只有一个 `IServiceManager` 实例。它通过 `BinderInternal.getContextObject()` 这个 native 方法获取底层的 Binder 对象，再将其转换为 `IServiceManager` 接口类型。
-   **`addService` 方法**：用于向 `ServiceManager` 注册服务。它调用 `IServiceManager` 实例的 `addService` 方法来完成注册操作，如果出现远程异常，会记录错误日志。
-   **`getService` 方法**：用于从 `ServiceManager` 获取指定名称的服务。先尝试通过 `getService` 方法获取，如果获取失败则调用 `findService` 方法进一步查找。若出现远程异常，会记录错误日志。
-   **`findService` 方法**：是一个私有方法，用于查找服务。它调用 `IServiceManager` 实例的 `checkService` 方法来查找服务，若出现远程异常，会记录错误日志。
-   **`listServices` 方法**：用于获取所有已注册服务的名称。它调用 `IServiceManager` 实例的 `listServices` 方法来获取服务列表，若出现远程异常，会将异常抛出。

  
  

在 C++ 中，`ServiceManager` 的核心实现主要涉及 `IServiceManager` 接口及其实现类 `BpServiceManager`（代理端）和 `BnServiceManager`（服务端），以及 `service_manager` 进程中的具体逻辑。

  
  

```
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <binder/RemoteException.h>

namespace android {

class BpServiceManager : public BpInterface<IServiceManager>
{
public:
    BpServiceManager(const sp<IBinder>& impl)
        : BpInterface<IServiceManager>(impl)
    {
    }

    // 向 ServiceManager 注册服务
    virtual status_t addService(const String16& name, const sp<IBinder>& service,
            bool allowIsolated, int dumpPriority)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());
        data.writeString16(name);
        data.writeStrongBinder(service);
        data.writeInt32(allowIsolated ? 1 : 0);
        data.writeInt32(dumpPriority);
        status_t err = remote()->transact(ADD_SERVICE_TRANSACTION, data, &reply);
        return err == NO_ERROR ? reply.readExceptionCode() : err;
    }

    // 从 ServiceManager 获取服务
    virtual sp<IBinder> getService(const String16& name) const
    {
        for (int i = 0; i < 5; i++){
            sp<IBinder> svc = checkService(name);
            if (svc != nullptr) return svc;
            ALOGI("Waiting for service %s...", String8(name).string());
            sleep(1);
        }
        return nullptr;
    }

    // 查找服务
    virtual sp<IBinder> checkService(const String16& name) const
    {
        Parcel data, reply;
        data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());
        data.writeString16(name);
        remote()->transact(CHECK_SERVICE_TRANSACTION, data, &reply);
        return reply.readStrongBinder();
    }

    // 获取所有服务的名称
    virtual Vector<String16> listServices(int dumpPriority)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IServiceManager::getInterfaceDescriptor());
        data.writeInt32(dumpPriority);
        status_t err = remote()->transact(LIST_SERVICES_TRANSACTION, data, &reply);
        if (err != NO_ERROR) return Vector<String16>();
        size_t N = reply.readInt32();
        Vector<String16> list;
        list.setCapacity(N);
        for (size_t i=0; i<N; i++) {
            list.add(reply.readString16());
        }
        return list;
    }
};

IMPLEMENT_META_INTERFACE(ServiceManager, "android.os.IServiceManager");

} // namespace android
```

  
  

-   **`addService` 方法**：通过 `Parcel` 来封装要注册的服务信息，包括服务名称、服务的 `Binder` 对象、是否允许隔离以及转储优先级等。然后调用 `remote()->transact` 方法将请求发送给服务端，最后读取响应并返回结果。
-   **`getService` 方法**：尝试多次调用 `checkService` 方法来获取服务，如果获取失败则会等待一段时间后重试，最多重试 5 次。若最终仍未获取到服务，则返回 `nullptr`。
-   **`checkService` 方法**：通过 `Parcel` 封装要查找的服务名称，调用 `remote()->transact` 方法将请求发送给服务端，最后从响应中读取服务的 `Binder` 对象并返回。
-   **`listServices` 方法**：通过 `Parcel` 封装转储优先级信息，调用 `remote()->transact` 方法将请求发送给服务端。从响应中读取服务列表的数量，然后依次读取每个服务的名称并存储在 `Vector` 中返回。

  

Java 层面的 `ServiceManager` 类通过调用底层的 C++ 实现来完成服务的注册、查询等操作，而 C++ 层面则涉及到 `IServiceManager` 接口及其代理类和服务类的具体实现，通过 `Binder` 机制实现跨进程通信，从而完成服务的管理。