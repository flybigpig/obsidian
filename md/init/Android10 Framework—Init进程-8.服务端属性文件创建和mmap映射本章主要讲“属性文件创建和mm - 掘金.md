> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

本章主要讲“属性文件创建和mmap映射”，现给出完整数据流程图 ![数据流程架构.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/e9d8068b35d34aee84220e9bd96336fa~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743249638&x-signature=YdKOiZzhQr63Id%2Bp8M1O1HSVoc0%3D) 上一章中讲解了上图左侧"属性安全上下文序列化"，右侧部分就是“属性文件创建和mmap映射“做的工作，入口代码为\_\_system\_property\_area\_init

```
void property_init() {
    mkdir("/dev/__properties__", S_IRWXU | S_IXGRP | S_IXOTH);
    CreateSerializedPropertyInfo();
    if (__system_property_area_init()) {
        LOG(FATAL) << "Failed to initialize property area";
    }
    if (!property_info_area.LoadDefaultPath()) {
        LOG(FATAL) << "Failed to load serialized property info file";
    }
}

bionic/libc/include/sys/_system_properties.h
#define PROP_FILENAME "/dev/__properties__"

// bionic/libc/include/sys/_system_properties.h 
// bionic/libc/bionic/system_property_api.cpp
__BIONIC_WEAK_FOR_NATIVE_BRIDGE
int __system_property_area_init() {
  bool fsetxattr_failed = false;
  return system_properties.AreaInit(PROP_FILENAME, &fsetxattr_failed) && !fsetxattr_failed ? 0 : -1;
}
```

可以看到\_\_system\_property\_area\_init是调用到system\_properties.AreaInit，system\_properties实际是图中SystemProperties对象，它是一个大管家对象，它又如下作用：

-   通过它里面的contexts\_可以找到所有属性文件的映射地址，然后访问它们
-   system\_property\_api.cpp文件中所有API接口其实都是调用SystemProperties对象中相关接口 在源码中并没有看到system\_properties实例创建的地方，通过其注释我们可以窥见端倪

```
class SystemProperties {
 public:
  friend struct LocalPropertyTestState;
  friend class SystemPropertiesTest;
  // Note that system properties are initialized before libc calls static initializers, so
  // doing any initialization in this constructor is an error.  Even a Constructor that zero
  // initializes this class will clobber the previous property initialization.
  // We rely on the static SystemProperties in libc to be placed in .bss and zero initialized.
  SystemProperties() = default;
  // Special constructor for testing that also zero initializes the important members.
  explicit SystemProperties(bool initialized) : initialized_(initialized) {
  }
}
```

可见system\_properties实例是由libc调用初始化的。

```
bool SystemProperties::AreaInit(const char* filename, bool* fsetxattr_failed) {
  if (strlen(filename) >= PROP_FILENAME_MAX) {
    return false;
  }
  strcpy(property_filename_, filename);

  contexts_ = new (contexts_data_) ContextsSerialized();
  if (!contexts_->Initialize(true, property_filename_, fsetxattr_failed)) {
    return false;
  }
  initialized_ = true;
  return true;
}
```

在AreaInit方法中又调用了ContextsSerialized->Initialize

> 注意：第一个参数为writable为true表示可写的，这是一个比较重要的参数

```
bool ContextsSerialized::Initialize(bool writable, const char* filename, bool* fsetxattr_failed) {
  filename_ = filename;
  if (!InitializeProperties()) {
    return false;
  }

  if (writable) {
    mkdir(filename_, S_IRWXU | S_IXGRP | S_IXOTH);
    bool open_failed = false;
    if (fsetxattr_failed) {
      *fsetxattr_failed = false;
    }

    for (size_t i = 0; i < num_context_nodes_; ++i) {
      if (!context_nodes_[i].Open(true, fsetxattr_failed)) {
        open_failed = true;
      }
    }
    if (open_failed || !MapSerialPropertyArea(true, fsetxattr_failed)) {
      FreeAndUnmap();
      return false;
    }
  } else {
    if (!MapSerialPropertyArea(false, nullptr)) {
      FreeAndUnmap();
      return false;
    }
  }
```

Initialize完成了橙色框中上面部分的工作：

-   加载"/dev/**properties**/property\_info"文件的内容（前一章属性安全上下文序列化的内容）
-   创建ContextNode数组
-   创建属性文件，并mmap映射属性文件，地址保存到ContextNode中

## 加载property\_info

```
bool ContextsSerialized::InitializeProperties() {
  //加载/dev/__properties__/property_info
  if (!property_info_area_file_.LoadDefaultPath()) {
    return false;
  }
  
  .....省略代码

  return true;
}

//system/core/property_service/libpropertyinfoparser/property_info_parser.cpp
bool PropertyInfoAreaFile::LoadDefaultPath() {
  return LoadPath("/dev/__properties__/property_info");
}

bool PropertyInfoAreaFile::LoadPath(const char* filename) {
  int fd = open(filename, O_CLOEXEC | O_NOFOLLOW | O_RDONLY);

  auto mmap_size = fd_stat.st_size;

  void* map_result = mmap(nullptr, mmap_size, PROT_READ, MAP_SHARED, fd, 0);
  if (map_result == MAP_FAILED) {
    close(fd);
    return false;
  }

  auto property_info_area = reinterpret_cast<PropertyInfoArea*>(map_result);
  if (property_info_area->minimum_supported_version() > 1 ||
      property_info_area->size() != mmap_size) {
    munmap(map_result, mmap_size);
    close(fd);
    return false;
  }

  close(fd);
  mmap_base_ = map_result;
  mmap_size_ = mmap_size;
  return true;
}
```

-   使用mmap映射"/dev/**properties**/property\_info"
-   然后让PropertyInfoAreaFile->mmap\_base\_指向这块内存区域的起始地址
-   然后让PropertyInfoAreaFile->mmap\_size\_指向这块内存区域的大小 这样后续就可以通过PropertyInfoAreaFile类操作里面的文件了。

## 创建ContextNode数组

```
bool ContextsSerialized::InitializeProperties() {
  .....省略代码
  
  //创建ContextNode数组
  if (!InitializeContextNodes()) {
    FreeAndUnmap();
    return false;
  }

  return true;
}

bool ContextsSerialized::InitializeContextNodes() {
  auto num_context_nodes = property_info_area_file_->num_contexts();
  auto context_nodes_mmap_size = sizeof(ContextNode) * num_context_nodes;
  // We want to avoid malloc in system properties, so we take an anonymous map instead (b/31659220).
  void* const map_result = mmap(nullptr, context_nodes_mmap_size, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (map_result == MAP_FAILED) {
    return false;
  }

  prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, map_result, context_nodes_mmap_size,
        "System property context nodes");

  context_nodes_ = reinterpret_cast<ContextNode*>(map_result);
  num_context_nodes_ = num_context_nodes;
  context_nodes_mmap_size_ = context_nodes_mmap_size;

  for (size_t i = 0; i < num_context_nodes; ++i) {
    new (&context_nodes_[i]) ContextNode(property_info_area_file_->context(i), filename_);
  }

  return true;
}
```

-   先从PropertyInfoAreaFile中读取映射数据结构中num\_contexts数量，其实就是图中的TrieBuilder->contexts数组
-   然后映射分配了num\_context\_nodes个ContextNode大小的空间
-   让ContextsSerialized.context\_nodes\_指向映射空间的起始地址
-   让ContextsSerialized.num\_context\_nodes\_指向映射空间ContextNode的个数
-   让ContextsSerialized.context\_nodes\_mmap\_size\_指向映射空间的大小
-   最后初始化每个ContextNode的context（安全上下文）和filename，filename是在前面\_\_system\_property\_area\_init函数中调用时传入的"/dev/**properties**"

## 创建属性文件

```
bool ContextsSerialized::Initialize(bool writable, const char* filename, bool* fsetxattr_failed) {
  .....省略代码

  if (writable) {
  .....省略代码
    for (size_t i = 0; i < num_context_nodes_; ++i) {
      if (!context_nodes_[i].Open(true, fsetxattr_failed)) {
        open_failed = true;
      }
    }
    
  } else {
    if (!MapSerialPropertyArea(false, nullptr)) {
      FreeAndUnmap();
      return false;
    }
  }
  return true;
}
```

循环遍历context\_nodes\_数组调用open方法创建属性文件

```
//调用
context_nodes_[i].Open(true, fsetxattr_failed)

//实现
bool ContextNode::Open(bool access_rw, bool* fsetxattr_failed) {
  lock_.lock();
  if (pa_) {
    lock_.unlock();
    return true;
  }

  char filename[PROP_FILENAME_MAX];
  int len = async_safe_format_buffer(filename, sizeof(filename), "%s/%s", filename_, context_);
  if (len < 0 || len >= PROP_FILENAME_MAX) {
    lock_.unlock();
    return false;
  }

  if (access_rw) {
    pa_ = prop_area::map_prop_area_rw(filename, context_, fsetxattr_failed);
  } else {
    pa_ = prop_area::map_prop_area(filename);
  }
  lock_.unlock();
  return pa_;
}
```

-   先构建属性文件的文件名格式为/dev/**properties**+context\_(安全上下文名称)
-   access\_rw为ture，表示以可读写的方式映射这个属性上下文（在属性服务框架中讲过init进程是唯一可以修改属性的进程，所以是可读写的方式映射）

```
constexpr size_t PA_SIZE = 128 * 1024;

prop_area* prop_area::map_prop_area_rw(const char* filename, const char* context,
                                       bool* fsetxattr_failed) {
  /* dev is a tmpfs that we can use to carve a shared workspace
   * out of, so let's do that...
   */
  const int fd = open(filename, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC | O_EXCL, 0444);

  if (fd < 0) {
    if (errno == EACCES) {
      /* for consistency with the case where the process has already
       * mapped the page in and segfaults when trying to write to it
       */
      abort();
    }
    return nullptr;
  }

  if (context) {
    if (fsetxattr(fd, XATTR_NAME_SELINUX, context, strlen(context) + 1, 0) != 0) {
      async_safe_format_log(ANDROID_LOG_ERROR, "libc",
                            "fsetxattr failed to set context (%s) for \"%s\"", context, filename);
      /*
       * fsetxattr() will fail during system properties tests due to selinux policy.
       * We do not want to create a custom policy for the tester, so we will continue in
       * this function but set a flag that an error has occurred.
       * Init, which is the only daemon that should ever call this function will abort
       * when this error occurs.
       * Otherwise, the tester will ignore it and continue, albeit without any selinux
       * property separation.
       */
      if (fsetxattr_failed) {
        *fsetxattr_failed = true;
      }
    }
  }

  if (ftruncate(fd, PA_SIZE) < 0) {
    close(fd);
    return nullptr;
  }

  pa_size_ = PA_SIZE;
  pa_data_size_ = pa_size_ - sizeof(prop_area);

  void* const memory_area = mmap(nullptr, pa_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (memory_area == MAP_FAILED) {
    close(fd);
    return nullptr;
  }

  prop_area* pa = new (memory_area) prop_area(PROP_AREA_MAGIC, PROP_AREA_VERSION);

  close(fd);
  return pa;
}
```

-   创建属性文件
-   fsetxattr为创建的属性文件设置安全上下文
-   然后映射属性文件，大小为128k
-   prop\_area->data指向文件映射的起始地址
-   最后将ContextNode->pa\_指向prop\_area

```
prop_area->pa_size_:整个prop_area数据结构的大小（包含data[0]）
prop_area->pa_data_size_:prop_area->data[0]可用最大空间
```

在/dev/\_\_properties\_\_下创建的属性文件如下

![属性文件示例.png](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/18bd3f056b65490a9d79f63244b77446~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743249638&x-signature=XkGPYuFZLnCg8MUQ6J64v54o7Mc%3D)

到此为止完整数据流程图中右侧的主要数据结构都构建完成，只剩下属性文件的数据了。