Here’s a **step-by-step guide** to adding a **native service** to AOSP (Android Open Source Project), including **code templates, build configurations, and debugging tips**. This guide covers **pure C++ native services** (no Java) and **hybrid services** (C++ with Java bindings).

---

---

# **🚀 Adding a Native Service to AOSP**
*(For Custom ROMs, System Modifications, or New Features)*

> **⚠️ WARNING**: Native services run with **system privileges**. Incorrect implementations can cause **bootloops, crashes, or security vulnerabilities**. Always test in an **emulator first** (`aosp_x86_64-eng`).

---

---

## **📌 1. Overview of Native Services in Android**
Native services in AOSP are **C++/Rust-based** and communicate via **Binder IPC**. They are typically used for:
- **Performance-critical tasks** (e.g., `SurfaceFlinger`, `AudioFlinger`).
- **Hardware interactions** (e.g., `DrmManagerService`, `CameraService`).
- **Low-level system functions** (e.g., `PowerManagerService` in some implementations).

### **🔹 Key Components**
| **Component** | **Purpose** | **Example** |
|--------------|------------|------------|
| **Service Implementation** | Core logic in C++ | `HelloService.cpp` |
| **Binder Interface** | IPC communication (C++ or AIDL) | `IHelloService.h` |
| **ServiceManager Registration** | Exposes the service to the system | `defaultServiceManager()->addService()` |
| **init.rc** | Starts the service at boot | `helloservice.rc` |
| **Android.mk** | Build configuration | Compiles the service into a binary |

---

---

## **📌 2. Step-by-Step: Adding a Pure Native Service**
We’ll create a **`HelloService`** that:
- Runs as a **standalone native process**.
- Exposes a `sayHello()` method via Binder.
- Can be called from **Java (optional)**.

---

### **🔹 Step 1: Create the Service Directory**
```bash
mkdir -p frameworks/native/services/helloservice
cd frameworks/native/services/helloservice
```

---

### **🔹 Step 2: Define the Binder Interface (C++)**
#### **File: `IHelloService.h`**
```cpp
// frameworks/native/services/helloservice/IHelloService.h
#ifndef ANDROID_IHELLOSERVICE_H
#define ANDROID_IHELLOSERVICE_H

#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/String16.h>

namespace android {

// Interface for the HelloService
class IHelloService : public IInterface {
public:
    // Declare the meta-interface (required for Binder)
    DECLARE_META_INTERFACE(HelloService);

    // Pure virtual method to be implemented by the service
    virtual String16 sayHello(const String16& name) = 0;
};

// Binder proxy class (handles incoming transactions)
class BnHelloService : public BnInterface<IHelloService> {
public:
    virtual status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags = 0);
};

} // namespace android

#endif // ANDROID_IHELLOSERVICE_H
```

#### **File: `IHelloService.cpp`**
```cpp
// frameworks/native/services/helloservice/IHelloService.cpp
#include "IHelloService.h"

namespace android {

// Transaction codes (must match Java if used)
enum {
    SAY_HELLO = IBinder::FIRST_CALL_TRANSACTION, // = 1
};

// Implement onTransact for BnHelloService
status_t BnHelloService::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
    switch (code) {
        case SAY_HELLO: {
            CHECK_INTERFACE(IHelloService, data, reply);
            String16 name = data.readString16();
            String16 result = sayHello(name);
            reply->writeString16(result);
            return NO_ERROR;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

// Required for Binder meta-interface
IMPLEMENT_META_INTERFACE(HelloService, "android.os.IHelloService");

} // namespace android
```

---

### **🔹 Step 3: Implement the Service**
#### **File: `HelloService.h`**
```cpp
// frameworks/native/services/helloservice/HelloService.h
#ifndef ANDROID_HELLOSERVICE_H
#define ANDROID_HELLOSERVICE_H

#include "IHelloService.h"

namespace android {

class HelloService : public BnHelloService {
public:
    // Static method to register the service with ServiceManager
    static void instantiate();

    // Implement the sayHello method
    String16 sayHello(const String16& name) override;
};

} // namespace android

#endif // ANDROID_HELLOSERVICE_H
```

#### **File: `HelloService.cpp`**
```cpp
// frameworks/native/services/helloservice/HelloService.cpp
#include "HelloService.h"
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <cutils/log.h>
#include <utils/Log.h>

namespace android {

// Register the service with ServiceManager
void HelloService::instantiate() {
    ALOGI("HelloService: Registering with ServiceManager");
    defaultServiceManager()->addService(
        String16("helloservice"),  // Service name
        new HelloService()         // Service instance
    );
}

// Implement the sayHello method
String16 HelloService::sayHello(const String16& name) {
    ALOGI("HelloService: sayHello called with name: %s", String8(name).string());
    return String16("Hello, ") + name + String16(" from native!");
}

} // namespace android
```

---

### **🔹 Step 4: Create the Main Entry Point**
#### **File: `main_helloservice.cpp`**
```cpp
// frameworks/native/services/helloservice/main_helloservice.cpp
#include <binder/IProcessState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include "HelloService.h"

using namespace android;

int main(int argc, char** argv) {
    ALOGI("HelloService: Starting service");

    // Initialize Binder
    sp<ProcessState> proc(ProcessState::self());

    // Register the service
    HelloService::instantiate();

    // Start Binder thread pool
    ProcessState::self()->startThreadPool();

    // Join the thread pool (blocks forever)
    IPCThreadState::self()->joinThreadPool();

    return 0;
}
```

---

### **🔹 Step 5: Configure the Build System**
#### **File: `Android.mk`**
```makefile
# frameworks/native/services/helloservice/Android.mk
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# Service binary
LOCAL_MODULE := helloservice
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    IHelloService.cpp \
    HelloService.cpp \
    main_helloservice.cpp

# Include paths
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    frameworks/native/include \
    system/core/include

# Shared libraries
LOCAL_SHARED_LIBRARIES := \
    libbinder \
    libcutils \
    liblog \
    libutils

# init.rc file (starts the service at boot)
LOCAL_INIT_RC := helloservice.rc

include $(BUILD_EXECUTABLE)
```

---

### **🔹 Step 6: Define the Service Startup Script**
#### **File: `helloservice.rc`**
```rc
# frameworks/native/services/helloservice/helloservice.rc
service helloservice /system/bin/helloservice
    class main
    user system
    group system
    capabilities SYS_NICE
    onrestart restart
```

> **💡 Notes**:
> - `class main`: Starts early in the boot process.
> - `user system`: Runs with system permissions.
> - `capabilities SYS_NICE`: Allows priority adjustments.
> - `onrestart restart`: Automatically restarts if the service crashes.

---

### **🔹 Step 7: Add the Service to the Product Configuration**
Edit your device’s **BoardConfig.mk** or **product makefile** (e.g., `device/generic/arm64/BoardConfig.mk`):
```makefile
# Add helloservice to the system
PRODUCT_PACKAGES += helloservice
```

> **⚠️ Important**:
> - If you’re building for an **emulator**, modify:
>   - `device/generic/goldfish/BoardConfig.mk` (for ARM emulators)
>   - `device/generic/x86_64/BoardConfig.mk` (for x86_64 emulators)
> - For **real devices**, modify your device’s `BoardConfig.mk`.

---

### **🔹 Step 8: Build the Service**
```bash
# From AOSP root
source build/envsetup.sh
lunch aosp_x86_64-eng  # or your target

# Build only the service (faster)
mmm frameworks/native/services/helloservice/

# Or rebuild the entire framework (if you modified Java files)
make framework
```

> **🔍 Verify the build**:
> ```bash
> ls $OUT/system/bin/helloservice
> ```
> Should output: `/path/to/out/target/product/.../system/bin/helloservice`

---

### **🔹 Step 9: Flash and Test**
#### **Option A: Emulator (Recommended)**
```bash
# Start emulator (with writable system)
emulator -avd aosp_x86_64 -writable-system -no-snapshot-load &

# Push the service binary
adb remount
adb push $OUT/system/bin/helloservice /system/bin/
adb chmod 644 /system/bin/helloservice
adb reboot
```

#### **Option B: Physical Device**
```bash
# Unlock bootloader (if not already)
adb reboot bootloader
fastboot oem unlock

# Flash custom recovery (TWRP)
fastboot flash recovery twrp.img
fastboot reboot recovery

# Push the service binary
adb remount
adb push $OUT/system/bin/helloservice /system/bin/
adb chmod 644 /system/bin/helloservice
adb reboot
```

---

### **🔹 Step 10: Verify the Service is Running**
```bash
# Check if the service is registered
adb shell service list | grep helloservice

# Check if the process is running
adb shell ps -A | grep helloservice

# Check logs
adb logcat | grep -i "HelloService"
```
**Expected Output**:
```
HelloService: Starting service
HelloService: Registering with ServiceManager
```

---

---

## **📌 3. Calling the Native Service from Java (Optional)**
To call your native service from **Java (e.g., an app or system UI)**, you need a **Java wrapper** that uses **Binder IPC**.

---

### **🔹 Step 1: Define the Java Interface**
#### **File: `frameworks/base/core/java/android/os/IHelloService.java`**
```java
package android.os;

import android.os.Binder;
import android.os.IBinder;
import android.os.IInterface;
import android.os.Parcel;
import android.os.RemoteException;

public interface IHelloService extends IInterface {
    String sayHello(String name) throws RemoteException;

    public static abstract class Stub extends Binder implements IHelloService {
        private static final String DESCRIPTOR = "android.os.IHelloService";
        static final int TRANSACTION_sayHello = (IBinder.FIRST_CALL_TRANSACTION + 0);

        private static class Proxy implements IHelloService {
            private IBinder mRemote;

            Proxy(IBinder remote) {
                mRemote = remote;
            }

            @Override
            public String sayHello(String name) throws RemoteException {
                Parcel _data = Parcel.obtain();
                Parcel _reply = Parcel.obtain();
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    _data.writeString(name);
                    mRemote.transact(TRANSACTION_sayHello, _data, _reply, 0);
                    _reply.readException();
                    String _result = _reply.readString();
                    return _result;
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
            }

            @Override
            public IBinder asBinder() {
                return mRemote;
            }
        }

        public Stub() {
            this.attachInterface(this, DESCRIPTOR);
        }

        public static IHelloService asInterface(IBinder obj) {
            if (obj == null) {
                return null;
            }
            IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
            if (iin != null && iin instanceof IHelloService) {
                return (IHelloService) iin;
            }
            return new Proxy(obj);
        }

        @Override
        public IBinder asBinder() {
            return this;
        }

        @Override
        public boolean onTransact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
            if (!data.enforceInterface(DESCRIPTOR)) {
                reply.writeString(DESCRIPTOR);
                return false;
            }
            switch (code) {
                case INTERFACE_TRANSACTION:
                    reply.writeString(DESCRIPTOR);
                    return true;
                case TRANSACTION_sayHello:
                    data.enforceInterface(DESCRIPTOR);
                    String _arg0 = data.readString();
                    String _result = this.sayHello(_arg0);
                    reply.writeNoException();
                    reply.writeString(_result);
                    return true;
                default:
                    return super.onTransact(code, data, reply, flags);
            }
        }
    }
}
```

---

### **🔹 Step 2: Create a Manager Class**
#### **File: `frameworks/base/core/java/android/os/HelloManager.java`**
```java
package android.os;

import android.util.Log;

public class HelloManager {
    private static final String TAG = "HelloManager";
    private static IHelloService sService;

    public static IHelloService getService() {
        if (sService != null) {
            return sService;
        }
        IBinder binder = ServiceManager.getService("helloservice");
        if (binder != null) {
            sService = IHelloService.Stub.asInterface(binder);
        } else {
            Log.e(TAG, "HelloService not available!");
        }
        return sService;
    }

    public static String sayHello(String name) {
        try {
            IHelloService service = getService();
            if (service != null) {
                return service.sayHello(name);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Error calling sayHello", e);
        }
        return "Error: Service not available";
    }
}
```

---

### **🔹 Step 3: Test from an App**
#### **Example: Call from an Activity**
```java
import android.os.HelloManager;
import android.util.Log;

public class MainActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        String response = HelloManager.sayHello("World");
        Log.d("HelloTest", "Response: " + response);
        // Expected: "Hello, World from native!"
    }
}
```

> **⚠️ Note**: The app must have the **`android.permission.INTERACT_ACROSS_USERS`** permission if calling from a non-system app (not required for system apps).

---

---

## **📌 4. Alternative: Hybrid Service (C++ + Java)**
If your service needs **both native and Java code**, you can create a **hybrid service** where:
- The **Java part** handles high-level logic.
- The **native part** handles performance-critical tasks.

### **🔹 Example: Hybrid HelloService**
#### **Step 1: Create a Java Service**
```java
// frameworks/base/services/core/java/com/android/server/HelloService.java
package com.android.server;

import android.os.IHelloService;
import android.util.Slog;

public class HelloService extends IHelloService.Stub {
    private static final String TAG = "HelloService";

    public HelloService() {
        Slog.i(TAG, "HelloService (Java) created");
    }

    @Override
    public String sayHello(String name) {
        Slog.i(TAG, "sayHello called with: " + name);
        // Call native code here if needed
        return "Hello, " + name + " from Java!";
    }
}
```

#### **Step 2: Register the Service in `SystemServer`**
Edit `frameworks/base/services/java/com/android/server/SystemServer.java`:
```java
// Add to the startOtherServices() method
try {
    Slog.i(TAG, "Starting HelloService");
    ServiceManager.addService("helloservice", new HelloService());
} catch (Throwable t) {
    Slog.e(TAG, "Failed to start HelloService", t);
}
```

#### **Step 3: Build and Test**
No need for `init.rc` or `Android.mk` (the service runs in `system_server`).

> **✅ Pros**: Easier to integrate with Java code.
> **❌ Cons**: Runs in `system_server` (if it crashes, the whole system crashes).

---

---

## **📌 5. Debugging Native Services**
### **🔹 Common Issues & Fixes**
| **Issue** | **Cause** | **Debugging Steps** |
|-----------|-----------|---------------------|
| **Service not registered** | `ServiceManager.addService()` failed | `adb logcat \| grep -i "ServiceManager\|HelloService"` |
| **Service crashes on start** | Missing libraries or null pointers | `adb logcat \| grep -i "Fatal\|Segmentation fault"` |
| **`service list` doesn’t show service** | `init.rc` not loaded | `adb shell getprop init.svc.helloservice` |
| **Binder transaction failed** | Mismatched transaction codes | Check `onTransact` in C++ and Java |
| **SELinux denial** | Missing permissions | `adb logcat \| grep -i avc` |
| **`adb push` fails** | Read-only `/system` | `adb remount` before pushing |

---

### **🔹 Debugging Commands**
```bash
# Check if service is running
adb shell ps -A | grep helloservice

# Check ServiceManager
adb shell service list | grep helloservice

# Check init service status
adb shell getprop init.svc.helloservice

# View full logs
adb logcat -c  # Clear logs
adb logcat | grep -i "HelloService\|helloservice"

# Check SELinux denials
adb logcat | grep -i avc

# Attach GDB to the service (advanced)
adb shell gdbserver :5039 /system/bin/helloservice
# Then in another terminal:
gdb-multiarch $OUT/system/bin/helloservice
(gdb) target remote :5039
```

---

### **🔹 Fixing SELinux Denials**
If you see **SELinux denials** (e.g., `avc: denied`), create a policy file:

#### **File: `system/sepolicy/private/helloservice.te`**
```te
# Allow helloservice to use binder
type helloservice, domain;
type helloservice_exec, exec_type, file_type;

# Service domain
init_daemon_domain(helloservice)

# Allow binder communication
allow helloservice servicemanager:binder { call };
allow helloservice { appdomain -system_app }:binder { call };

# Allow logging
allow helloservice logd:unix_stream_socket { connectto };
allow helloservice logd:chr_file { read write };
```

Then rebuild the SELinux policy:
```bash
make sepolicy
```

---

---

## **📌 6. Advanced Topics**
### **🔹 6.1 Adding AIDL Support for Native Services**
Starting from **Android 10 (Q)**, AOSP supports **AIDL for native services** (C++). This allows you to:
- Define the interface in **AIDL**.
- Generate **C++ code** automatically.

#### **Steps**:
1. **Define the AIDL file** (same as Java):
   ```aidl
   // IHelloService.aidl
   interface IHelloService {
       String sayHello(String name);
   }
   ```
2. **Generate C++ code**:
   ```bash
   aidl --cpp IHelloService.aidl -o out/
   ```
   > **⚠️ Note**: This requires **AIDL with C++ support** (available in Android 10+).

3. **Use the generated code** in your service.

> **📌 Reference**: [AOSP AIDL for Native](https://source.android.com/docs/core/os/aidl#cpp)

---

### **🔹 6.2 Using `hwbinder` (Hardware Binder)**
For **hardware-related services**, use `hwbinder` (faster than `libbinder`):
- Example: `CameraService`, `DrmManagerService`.
- **Header**: `#include <hwbinder/IInterface.h>`
- **Library**: `libhwbinder`

#### **Example: `hwbinder` Service**
```cpp
#include <hwbinder/IInterface.h>
#include <hwbinder/BnHwInterface.h>

class IHelloService : public hw::IInterface {
public:
    DECLARE_HW_META_INTERFACE(HelloService);
    virtual String16 sayHello(const String16& name) = 0;
};

class BnHelloService : public hw::BnHwInterface<IHelloService> {
public:
    hw::Return<void> sayHello(hw::hide::hidl_string name, sayHello_cb _hidl_cb) override {
        _hidl_cb("Hello, " + name + "!");
    }
};
```

---

### **🔹 6.3 Adding a Service to `system_server`**
If your service should run in `system_server` (like AMS, WMS):
1. **Implement the service in Java** (e.g., `HelloService.java`).
2. **Register it in `SystemServer.java`**:
   ```java
   // In frameworks/base/services/java/com/android/server/SystemServer.java
   try {
       ServiceManager.addService("helloservice", new HelloService());
   } catch (Throwable t) {
       Slog.e(TAG, "Failed to start HelloService", t);
   }
   ```
3. **No need for `init.rc` or `Android.mk`** (built into `framework.jar`).

> **✅ Pros**: Simpler integration with Java.
> **❌ Cons**: If `system_server` crashes, your service crashes too.

---

### **🔹 6.4 Using `hidl` (HAL Interface Definition Language)**
For **hardware abstraction layers (HALs)**, use **HIDL**:
- **Example**: `Camera HAL`, `Audio HAL`.
- **Files**: `.hal` (interface), `.cpp` (implementation).
- **Tools**: `hidl-gen` (generates C++ code).

#### **Example: HIDL Service**
1. **Define the interface** (`IHelloService.hal`):
   ```hal
   package android.hardware.hello@1.0;

   interface IHelloService {
       sayHello(string name) generates (string result);
   };
   ```
2. **Generate code**:
   ```bash
   hidl-gen -o out/ -Lc++-impl -randroid.hardware.hello@1.0 IHelloService
   ```
3. **Implement the service** in C++.

> **📌 Reference**: [AOSP HIDL Guide](https://source.android.com/docs/core/hal/hidl)

---

---

## **📌 7. Real-World Examples from AOSP**
| **Service** | **Type** | **Location** | **Purpose** |
|-------------|----------|--------------|-------------|
| **SurfaceFlinger** | Native (C++) | `frameworks/native/services/surfaceflinger` | Composites app surfaces |
| **AudioFlinger** | Native (C++) | `frameworks/av/services/audioflinger` | Manages audio |
| **DrmManagerService** | Native (C++) | `frameworks/av/drm` | Handles DRM content |
| **CameraService** | Hybrid (Java + Native) | `frameworks/av/services/camera` | Manages camera hardware |
| **PowerManagerService** | Java | `frameworks/base/services/core/java/com/android/server/power` | Manages power state |
| **PackageManagerService** | Java | `frameworks/base/services/core/java/com/android/server/pm` | Manages apps |

> **💡 Tip**: Study these services to understand **best practices** for native services.

---

---

## **📌 8. Best Practices**
### **✅ Do’s**
| **Best Practice** | **Why?** |
|-------------------|----------|
| **Use `android::sp` for Binder objects** | Automatic reference counting. |
| **Check for null in `onTransact`** | Prevents crashes from invalid calls. |
| **Log important events** | Helps with debugging (`ALOGI`, `ALOGE`). |
| **Use `DEFAULT_SERVICE_MANAGER`** | Ensures compatibility across Android versions. |
| **Test in emulator first** | Avoids bricking physical devices. |
| **Use `LOCAL_INIT_RC` for standalone services** | Ensures the service starts at boot. |
| **Follow AOSP coding style** | Makes it easier to upstream changes. |

### **❌ Don’ts**
| **Pitfall** | **Risk** | **Fix** |
|-------------|----------|---------|
| **Modify `libbinder` directly** | Breaks system-wide Binder | Extend `BnInterface` instead. |
| **Ignore SELinux denials** | Silent failures | Check `adb logcat \| grep avc`. |
| **Hardcode paths** | Breaks across devices | Use `android::String8` or `std::string`. |
| **Assume all clients are trusted** | Security vulnerabilities | Validate inputs in `onTransact`. |
| **Block Binder thread pool** | Freezes the system | Use `IPCThreadState::self()->joinThreadPool()`. |
| **Forget to call `startThreadPool()`** | Binder calls hang | Always call it in `main()`. |

---

### **🔹 Example: Safe `onTransact` Implementation**
```cpp
status_t BnHelloService::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
    switch (code) {
        case SAY_HELLO: {
            // Check interface token (required!)
            CHECK_INTERFACE(IHelloService, data, reply);

            // Read input safely
            String16 name;
            if (data.readString16(&name) != NO_ERROR) {
                ALOGE("Failed to read name from Parcel");
                return BAD_VALUE;
            }

            // Call the service method
            String16 result = sayHello(name);
            reply->writeString16(result);
            return NO_ERROR;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}
```

---

---

## **📌 9. Troubleshooting Guide**
### **🚨 Issue: Service Not Registered in `ServiceManager`**
**Symptoms**:
- `adb shell service list` does not show your service.
- `ServiceManager.getService("helloservice")` returns `null`.

**Debugging Steps**:
1. Check if the service binary exists:
   ```bash
   adb shell ls /system/bin/helloservice
   ```
2. Check if the service is running:
   ```bash
   adb shell ps -A | grep helloservice
   ```
3. Check `init.rc` syntax:
   ```bash
   adb shell getprop init.svc.helloservice
   ```
4. Check logs:
   ```bash
   adb logcat | grep -i "ServiceManager\|helloservice"
   ```
5. **Fix**: Ensure `instantiate()` is called in `main()`.

---

### **🚨 Issue: Binder Transaction Failed**
**Symptoms**:
- `adb logcat` shows `TransactionTooLargeException` or `DeadObjectException`.
- Java client gets `RemoteException`.

**Debugging Steps**:
1. Check transaction codes:
   - Ensure `TRANSACTION_sayHello` in Java matches the C++ code (`SAY_HELLO`).
2. Check interface descriptor:
   - Ensure `DESCRIPTOR` in Java matches `IMPLEMENT_META_INTERFACE` in C++.
3. Check Parcel reading/writing:
   - Ensure data types match (e.g., `String16` in C++ ↔ `String` in Java).

**Fix**: Use the same **interface name** and **transaction codes** in both C++ and Java.

---

### **🚨 Issue: SELinux Denial**
**Symptoms**:
- `adb logcat` shows `avc: denied` messages.
- Service works in **permissive mode** but not in **enforcing mode**.

**Debugging Steps**:
1. Set SELinux to permissive mode (temporarily):
   ```bash
   adb shell setenforce 0
   ```
2. Test if the service works. If it does, SELinux is the issue.
3. Check denials:
   ```bash
   adb logcat | grep -i avc
   ```
4. **Fix**: Add a policy file (see [Section 5.3](#-53-fixing-selinux-denials)).

---

### **🚨 Issue: Service Crashes on Start**
**Symptoms**:
- `adb logcat` shows `Fatal signal 11 (SIGSEGV)` or `Fatal signal 6 (SIGABRT)`.
- Service exits immediately after starting.

**Debugging Steps**:
1. Check logs for stack traces:
   ```bash
   adb logcat | grep -i "Fatal\|Stack\|#00"
   ```
2. Attach GDB:
   ```bash
   adb shell gdbserver :5039 /system/bin/helloservice
   gdb-multiarch $OUT/system/bin/helloservice
   (gdb) target remote :5039
   (gdb) bt
   ```
3. **Fix**: Check for **null pointers**, **missing libraries**, or **incorrect Binder usage**.

---

### **🚨 Issue: `adb push` Fails with "Read-only file system"**
**Fix**:
```bash
adb remount
adb push $OUT/system/bin/helloservice /system/bin/
adb chmod 644 /system/bin/helloservice
adb reboot
```
> **⚠️ Note**: Some devices require **`--force`** or a custom recovery.

---

---

## **📌 10. Full Example: File Structure**
```
frameworks/native/services/helloservice/
├── Android.mk
├── helloservice.rc
├── IHelloService.h
├── IHelloService.cpp
├── HelloService.h
├── HelloService.cpp
└── main_helloservice.cpp

frameworks/base/core/java/android/os/
├── IHelloService.java
└── HelloManager.java
```

---

---

## **📌 11. Next Steps**
### **🎯 Project Ideas**
| **Project** | **Service Type** | **Difficulty** | **Use Case** |
|-------------|------------------|----------------|--------------|
| **Battery Historian** | Native | ⭐⭐⭐ | Track battery usage |
| **Custom Keyguard** | Hybrid | ⭐⭐⭐ | Replace lock screen |
| **Screen Recorder** | Native | ⭐⭐⭐ | Record screen without root |
| **App Blocker** | Java | ⭐⭐ | Block specific apps |
| **Performance Booster** | Native | ⭐⭐⭐ | Adjust CPU governor |
| **Custom Clipboard** | Hybrid | ⭐⭐ | Modify clipboard behavior |
| **Network Monitor** | Native | ⭐⭐⭐ | Track data usage per app |

### **📚 Learning Resources**
| **Resource** | **Focus** | **Link** |
|--------------|-----------|----------|
| **AOSP Native Services Docs** | Official guide | [source.android.com](https://source.android.com/docs/core/os/binder) |
| **Android Binder IPC** | Deep dive into Binder | [Android Binder](https://blog.mindorks.com/android-binder-ipc-mechanism) |
| **SELinux for Android** | Security policies | [SELinux Android](https://source.android.com/docs/security/selinux) |
| **LineageOS Wiki** | Custom ROM development | [wiki.lineageos.org](https://wiki.lineageos.org) |
| **XDA Developers** | Community support | [forum.xda-developers.com](https://forum.xda-developers.com) |

---

### **💡 Need More Help?**
Let me know:
- What **type of service** you want to add (native, hybrid, HIDL)?
- What **functionality** it should provide?
- Any **specific errors** you’re encountering?

I can provide:
✅ **Custom code snippets** for your use case.
✅ **Debugging assistance** for crashes or denials.
✅ **Architecture advice** for complex services.