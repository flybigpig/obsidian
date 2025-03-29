> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

本章看看客户端是如何去操作属性的，这里主要分为 2 个部分：

-   服务端中属性的初始化过程由init完成，那客户端属性初始化是哪里做的呢
-   客户端如何发起属性操作

## 客户端属性初始化

客户端对属性初始化是由\_\_system\_properties\_init()函数完成，其调用路径如下：

```
// bionic/libc/bionic/libc_init_dynamic.cpp

// We flag the __libc_preinit function as a constructor to ensure that
// its address is listed in libc.so's .init_array section.
// This ensures that the function is called by the dynamic linker as
// soon as the shared library is loaded.
// We give this constructor priority 1 because we want libc's constructor
// to run before any others (such as the jemalloc constructor), and lower
// is better (http://b/68046352).
__attribute__((constructor(1))) static void __libc_preinit() {
  // The linker has initialized its copy of the global stack_chk_guard, and filled in the main
  // thread's TLS slot with that value. Initialize the local global stack guard with its value.
  __stack_chk_guard = reinterpret_cast<uintptr_t>(__get_tls()[TLS_SLOT_STACK_GUARD]);

  __libc_preinit_impl();
}

__attribute__((noinline))
static void __libc_preinit_impl() {
  ...省略代码
  
  __libc_init_common();
  
  ...省略代码
}

// bionic/libc/bionic/libc_init_common.cpp
void __libc_init_common() {
  ...省略代码

  __system_properties_init(); // Requires 'environ'.
  
  ...省略代码
}
```

我看看重点看看\_\_libc\_preinit函数的注释

```
// We flag the __libc_preinit function as a constructor to ensure that
// its address is listed in libc.so's .init_array section.
// This ensures that the function is called by the dynamic linker as
// soon as the shared library is loaded.
// We give this constructor priority 1 because we want libc's constructor
// to run before any others (such as the jemalloc constructor), and lower
// is better (http://b/68046352).
```

\_\_libc\_preinit函数会被放到libc.so的init\_array段中,这样就确保了共享库被加载时能尽可能快的调用\_\_libc\_preinit；constructor被设置为 1，这就意味着\_\_libc\_preinit被执行的优先级会非常高（值越低执行优先级越高）

总结：

-   \_\_system\_properties\_init在加载libc.so时被执行
-   所有应用程序无论c、c++、java，系统都会自动加载libc.so

这样应用程序就完成了属性的初始化。接下来看看\_\_system\_properties\_init

```
// bionic/libc/bionic/system_property_api.cpp
__BIONIC_WEAK_FOR_NATIVE_BRIDGE
int __system_properties_init() {
  return system_properties.Init(PROP_FILENAME) ? 0 : -1;
}
```

看看又调用了system\_property\_api.cpp这个API文件中，里面最后还是调用我们前面说的大管家类SystemProperties。

```
bool SystemProperties::Init(const char* filename) {
  // This is called from __libc_init_common, and should leave errno at 0 (http://b/37248982).
  ErrnoRestorer errno_restorer;

  if (initialized_) {
    contexts_->ResetAccess();
    return true;
  }

  if (strlen(filename) >= PROP_FILENAME_MAX) {
    return false;
  }
  strcpy(property_filename_, filename);

  if (is_dir(property_filename_)) {
    if (access("/dev/__properties__/property_info", R_OK) == 0) {
      contexts_ = new (contexts_data_) ContextsSerialized();
      if (!contexts_->Initialize(false, property_filename_, nullptr)) {
        return false;
      }
    } else {
      contexts_ = new (contexts_data_) ContextsSplit();
      if (!contexts_->Initialize(false, property_filename_, nullptr)) {
        return false;
      }
    }
  } else {
    contexts_ = new (contexts_data_) ContextsPreSplit();
    if (!contexts_->Initialize(false, property_filename_, nullptr)) {
      return false;
    }
  }
  initialized_ = true;
  return true;
}
```

这个contexts\_->Initialize方法我们在"服务端属性文件创建和mmap映射"这篇文章中详细分析过，它完成如下功能：

-   加载"/dev/**properties**/property\_info"文件的内容（前一章属性安全上下文序列化的内容）
-   创建ContextNode数组
-   创建属性文件，并mmap映射属性文件，地址保存到ContextNode中

这个过程其实就是在客户端进程初始化构建了橙色框中的数据结构，后续可以通过属性名查找到prop\_area对象，进而读取到属性值。唯一需要注意的是，客户端进程只能读属性不能修改，所以第一个参数设置的false，标识prop\_area映射的内存控件只读。

```
contexts_->Initialize(false, property_filename_, nullptr)
```

而init进程设置的是true，是可以写的

```
bool SystemProperties::AreaInit(const char* filename, bool* fsetxattr_failed) {
  ...省略代码

  contexts_ = new (contexts_data_) ContextsSerialized();
  if (!contexts_->Initialize(true, property_filename_, fsetxattr_failed)) {
    return false;
  }

  ...省略代码
  
  return true;
}
```

![数据流程架构图.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/2e0f533a81824ed793b52437e044803c~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743249673&x-signature=H%2FX0HIjNV41YNx023KTS0gDHme0%3D)

## 属性操作

客户端可以通过如下代码操作属性

```
// system/core/libcutils/include/cutils/properties.h
// system/core/libcutils/properties.cpp

#include <cutils/properties.h>

int property_set(const char *key, const char *value)
int property_get(const char *key, char *value, const char *default_value)
```

### 读属性

```
int property_get(const char *key, char *value, const char *default_value) {
    int len = __system_property_get(key, value);
    if (len > 0) {
        return len;
    }
    if (default_value) {
        len = strnlen(default_value, PROPERTY_VALUE_MAX - 1);
        memcpy(value, default_value, len);
        value[len] = '\0';
    }
    return len;
}

// bionic/libc/bionic/system_property_api.cpp
__BIONIC_WEAK_FOR_NATIVE_BRIDGE
int __system_property_get(const char* name, char* value) {
  return system_properties.Get(name, value);
}
```

property\_get最后还是调用到大管家SystemProperties中的Get方法

```
int SystemProperties::Get(const char* name, char* value) {
  const prop_info* pi = Find(name);

  if (pi != nullptr) {
    return Read(pi, nullptr, value);
  } else {
    value[0] = 0;
    return 0;
  }
}
```

Get方法还是先通过属性名找到prop\_info，然后读取里面的属性。

### 写属性

```
int property_set(const char *key, const char *value) {
    return __system_property_set(key, value);
}
```

前面看到调用的很多方法都是在system\_property\_api.cpp中，property\_set有所不同，它调用的是system\_property\_set.cpp中方法

```
// bionic/libc/bionic/system_property_set.cpp

__BIONIC_WEAK_FOR_NATIVE_BRIDGE
int __system_property_set(const char* key, const char* value) {
  if (key == nullptr) return -1;
  if (value == nullptr) value = "";

  if (g_propservice_protocol_version == 0) {
    detect_protocol_version();
  }

  if (g_propservice_protocol_version == kProtocolVersion1) {//旧版本
    ....省略代码
  } else {//走这里新版本
    // New protocol only allows long values for ro. properties only.
    if (strlen(value) >= PROP_VALUE_MAX && strncmp(key, "ro.", 3) != 0) return -1;
    // Use proper protocol
    PropertyServiceConnection connection;
    if (!connection.IsValid()) {
      errno = connection.GetLastError();
      async_safe_format_log(
          ANDROID_LOG_WARN, "libc",
          "Unable to set property \"%s\" to \"%s\": connection failed; errno=%d (%s)", key, value,
          errno, strerror(errno));
      return -1;
    }

    SocketWriter writer(&connection);
    if (!writer.WriteUint32(PROP_MSG_SETPROP2).WriteString(key).WriteString(value).Send()) {
      errno = connection.GetLastError();
      async_safe_format_log(ANDROID_LOG_WARN, "libc",
                            "Unable to set property \"%s\" to \"%s\": write failed; errno=%d (%s)",
                            key, value, errno, strerror(errno));
      return -1;
    }

    int result = -1;
    if (!connection.RecvInt32(&result)) {
      errno = connection.GetLastError();
      async_safe_format_log(ANDROID_LOG_WARN, "libc",
                            "Unable to set property \"%s\" to \"%s\": recv failed; errno=%d (%s)",
                            key, value, errno, strerror(errno));
      return -1;
    }

    if (result != PROP_SUCCESS) {
      async_safe_format_log(ANDROID_LOG_WARN, "libc",
                            "Unable to set property \"%s\" to \"%s\": error code: 0x%x", key, value,
                            result);
      return -1;
    }

    return 0;
  }
}
```

-   PropertyServiceConnection构造函数创建socket，并连接"/dev/socket/property\_service"
-   然后调用如下代码发送请求

```
writer.WriteUint32(PROP_MSG_SETPROP2).WriteString(key).WriteString(value).Send()
```

到这里客户端的写操作也就玩成了。