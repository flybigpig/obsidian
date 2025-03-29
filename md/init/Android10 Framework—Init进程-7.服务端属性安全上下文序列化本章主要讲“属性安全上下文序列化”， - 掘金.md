> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

本章主要讲“属性安全上下文序列化”，现给出完整数据流程图 ![数据流程架构.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/e9d8068b35d34aee84220e9bd96336fa~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743246851&x-signature=7HyS1swpihJcTnWyumlM3Fa2M0U%3D)

```
void CreateSerializedPropertyInfo() {
    auto property_infos = std::vector<PropertyInfoEntry>();
    //读取属性相关安全上下文文件内容，构建成vector<PropertyInfoEntry>数组
    if (access("/system/etc/selinux/plat_property_contexts", R_OK) != -1) {
        if (!LoadPropertyInfoFromFile("/system/etc/selinux/plat_property_contexts",
                                      &property_infos)) {
            return;
        }
        // Don't check for failure here, so we always have a sane list of properties.
        // E.g. In case of recovery, the vendor partition will not have mounted and we
        // still need the system / platform properties to function.
        if (!LoadPropertyInfoFromFile("/vendor/etc/selinux/vendor_property_contexts",
                                      &property_infos)) {
            // Fallback to nonplat_* if vendor_* doesn't exist.
            LoadPropertyInfoFromFile("/vendor/etc/selinux/nonplat_property_contexts",
                                     &property_infos);
        }
        if (access("/product/etc/selinux/product_property_contexts", R_OK) != -1) {
            LoadPropertyInfoFromFile("/product/etc/selinux/product_property_contexts",
                                     &property_infos);
        }
        if (access("/odm/etc/selinux/odm_property_contexts", R_OK) != -1) {
            LoadPropertyInfoFromFile("/odm/etc/selinux/odm_property_contexts", &property_infos);
        }
    } else {
        if (!LoadPropertyInfoFromFile("/plat_property_contexts", &property_infos)) {
            return;
        }
        if (!LoadPropertyInfoFromFile("/vendor_property_contexts", &property_infos)) {
            // Fallback to nonplat_* if vendor_* doesn't exist.
            LoadPropertyInfoFromFile("/nonplat_property_contexts", &property_infos);
        }
        LoadPropertyInfoFromFile("/product_property_contexts", &property_infos);
        LoadPropertyInfoFromFile("/odm_property_contexts", &property_infos);
    }

    auto serialized_contexts = std::string();
    auto error = std::string();
    //将vector<PropertyInfoEntry>再转化为TrieBuilder数据结构
    //使用TrieSerializer类对TrieBuilder数据结构序列化为字符串
    if (!BuildTrie(property_infos, "u:object_r:default_prop:s0", "string", &serialized_contexts,
                   &error)) {
        LOG(ERROR) << "Unable to serialize property contexts: " << error;
        return;
    }

    constexpr static const char kPropertyInfosPath[] = "/dev/__properties__/property_info";
    //将字符串写入/dev/__properties__/property_info文件中
    if (!WriteStringToFile(serialized_contexts, kPropertyInfosPath, 0444, 0, 0, false)) {
        PLOG(ERROR) << "Unable to write serialized property infos to file";
    }
    selinux_android_restorecon(kPropertyInfosPath, 0);
}
```

这张图左侧将CreateSerializedPropertyInfo函数功能分为如下几部分：

-   读取属性相关安全上下文文件内容，构建成vector数组
-   将vector转化为TrieBuilder数据结构
-   使用TrieSerializer类对TrieBuilder中数据按照一定的结构写入内存，然后序列化为字符串
-   将字符串写入/dev/**properties**/property\_info文件中

## 构建PropertyInfoEntry数组

属性安全上下文文件是在编译系统源码时产生的（这个过程就不详细讨论了），以AS上的如下google模拟器为例

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/7f9f21f038b843ed8b6707a60837ede2~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743246851&x-signature=f%2FJB3st%2B%2BAoZiOUyqOlgrldDyY4%3D)

属性安全上下文文件有如下文件

```
/system/etc/selinux/plat_property_contexts
/vendor/etc/selinux/vendor_property_contexts
```

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/9010e69dbc264f2aba527b333e192f87~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743246851&x-signature=%2FUcnzAHDzwBy4b8Wahl8EHyqRSs%3D) 文件部分内容如下：

```
audio.camerasound.force u:object_r:exported_audio_prop:s0 exact bool
audio.deep_buffer.media u:object_r:exported3_default_prop:s0 exact bool
audio.offload.video u:object_r:exported3_default_prop:s0 exact bool
audio.offload.min.duration.secs u:object_r:exported3_default_prop:s0 exact int
camera.disable_zsl_mode u:object_r:exported3_default_prop:s0 exact bool
camera.fifo.disable u:object_r:exported3_default_prop:s0 exact int
dalvik.vm.appimageformat u:object_r:exported_dalvik_prop:s0 exact string
dalvik.vm.backgroundgctype u:object_r:exported_dalvik_prop:s0 exact string
dalvik.vm.boot-dex2oat-threads u:object_r:exported_dalvik_prop:s0 exact int
dalvik.vm.boot-image u:object_r:exported_dalvik_prop:s0 exact string
dalvik.vm.checkjni u:object_r:exported_dalvik_prop:s0 exact bool
dalvik.vm.dex2oat-Xms u:object_r:exported_dalvik_prop:s0 exact string
dalvik.vm.dex2oat-Xmx u:object_r:exported_dalvik_prop:s0 exact string
```

![image.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/64e858981b994802b3f70daeb96edf87~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743246851&x-signature=7zmYvsN3kRY1uF3Otav2HCpejes%3D) 文件部分内容如下：

```
qemu.                   u:object_r:qemu_prop:s0
qemu.cmdline            u:object_r:qemu_cmdline:s0
vendor.qemu             u:object_r:qemu_prop:s0
vendor.network          u:object_r:vendor_net:s0
ro.emu.                 u:object_r:qemu_prop:s0
ro.emulator.            u:object_r:qemu_prop:s0
ro.radio.noril          u:object_r:radio_noril_prop:s0
net.eth0.               u:object_r:net_eth0_prop:s0
net.radio0.             u:object_r:net_radio0_prop:s0
net.shared_net_ip       u:object_r:net_share_prop:s0
ro.zygote.disable_gl_preload            u:object_r:qemu_prop:s0
#line 1 "system/sepolicy/reqd_mask/property_contexts"
# empty property_contexts file - this file is used to generate an empty
# non-platform property context for devices without any property_contexts
# customizations.
```

plat\_property\_contexts中内容结构如下：

```
属性名                   属性安全上下文(与selinux有关)         标志  属性值的类型
audio.camerasound.force u:object_r:exported_audio_prop:s0 exact bool
```

vendor\_property\_contexts内容与plat\_property\_contexts有以下几点差异：

-   vendor\_property\_contexts中没有类似exact string等字段
-   vendor\_property\_contexts中属性key有的是以.结尾的（这个后续分析代码会用到）

LoadPropertyInfoFromFile函数的作用就很清晰了，就是解析这些文件的内容保存到vector数组中（函数实现不复杂，大家自行看一下就可以了）。

> 注意：属性安全上下文文件中是没有属性value的，因为属性value的初始化不在在这个阶段实现的

```
//system/core/property_service/libpropertyinfoserializer/property_info_file.cpp
    
void ParsePropertyInfoFile(const std::string& file_contents,
                           std::vector<PropertyInfoEntry>* property_infos,
                           std::vector<std::string>* errors) {
  // Do not clear property_infos to allow this function to be called on multiple files, with
  // their results concatenated.
  errors->clear();

  //按行读取  
  for (const auto& line : Split(file_contents, "\n")) {
    auto trimmed_line = Trim(line);
    if (trimmed_line.empty() || StartsWith(trimmed_line, "#")) {
      continue;
    }

    auto property_info_entry = PropertyInfoEntry{};
    auto parse_error = std::string{};
    //对一行数据进行解析，然后将数据填充到property_info_entry
    if (!ParsePropertyInfoLine(trimmed_line, &property_info_entry, &parse_error)) {
      errors->emplace_back(parse_error);
      continue;
    }
    //添加到数组中
    property_infos->emplace_back(property_info_entry);
  }
}    
```

## PropertyInfoEntry数组转化为TrieBuilder

这一过程是在如下调用中实现

```
//调用    
BuildTrie(property_infos, "u:object_r:default_prop:s0", "string", &serialized_contexts,
                   &error)
    
//实现
bool BuildTrie(const std::vector<PropertyInfoEntry>& property_info,
               const std::string& default_context, const std::string& default_type,
               std::string* serialized_trie, std::string* error) {
  //将vector<PropertyInfoEntry>转化为TrieBuilder数据结构
  // Check that names are legal first
  auto trie_builder = TrieBuilder(default_context, default_type);
  for (const auto& [name, context, type, is_exact] : property_info) {
    if (!trie_builder.AddToTrie(name, context, type, is_exact, error)) {
      return false;
    }
  }

  //使用TrieSerializer类对TrieBuilder中数据按照一定的结构写入内存，然后序列化为字符串
  auto trie_serializer = TrieSerializer();
  *serialized_trie = trie_serializer.SerializeTrie(trie_builder);
  return true;
}
```

将vector转化为TrieBuilder数据结构过程大致做了如下工作：

-   将vector所有PropertyInfoEntry->context保存到TrieBuilder->contexts\_中
-   将vector所有PropertyInfoEntry->type保存到TrieBuilder->types\_中
-   将vector构建成一棵字典树（自行查阅字典树结构）
    -   字典树的根节点是TrieBuilder->builder\_root\_
    -   字典树每个节点是TrieBuilderNode，它里面会保存PropertyInfoEntry中的数据信息

```
//调用
auto trie_builder = TrieBuilder("u:object_r:default_prop:s0", "string");
    
//定义
TrieBuilder::TrieBuilder(const std::string& default_context, const std::string& default_type)
    : builder_root_("root") {
  auto* context_pointer = StringPointerFromContainer(default_context, &contexts_);
  builder_root_.set_context(context_pointer);
  auto* type_pointer = StringPointerFromContainer(default_type, &types_);
  builder_root_.set_type(type_pointer);
}
```

-   调用TrieBuilder构造函数，同时创建root节点builder\_root\_，并设置root->property\_entry\_的name、context、type分别为"root"、"u:object\_r:default\_prop:s0"、"string"
-   StringPointerFromContainer将root节点默认的default\_context和default\_type分别保存到TrieBuilder->contexts\_和TrieBuilder->types\_中

```
//调用
for (const auto& [name, context, type, is_exact] : property_info) {
    if (!trie_builder.AddToTrie(name, context, type, is_exact, error)) {
      return false;
    }
}
    
//定义
bool TrieBuilder::AddToTrie(const std::string& name, const std::string& context,
                            const std::string& type, bool exact, std::string* error) {
  auto* context_pointer = StringPointerFromContainer(context, &contexts_);
  auto* type_pointer = StringPointerFromContainer(type, &types_);
  return AddToTrie(name, context_pointer, type_pointer, exact, error);
}
```

这里就是循环遍历vector数组，StringPointerFromContainer将PropertyInfoEntry的context和type保存到TrieBuilder->contexts\_和TrieBuilder->types\_中

```
bool TrieBuilder::AddToTrie(const std::string& name, const std::string* context,
                            const std::string* type, bool exact, std::string* error) {
  TrieBuilderNode* current_node = &builder_root_;
  //将属性的名字以.进行分割
  auto name_pieces = Split(name, ".");
  //如果属性是以.结尾的，那最后一项就是空，需要把最后一项去除  
  bool ends_with_dot = false;
  if (name_pieces.back().empty()) {
    ends_with_dot = true;
    name_pieces.pop_back();
  }

  // Move us to the final node that we care about, adding incremental nodes if necessary.
  //构建字典树
  while (name_pieces.size() > 1) {
    auto child = current_node->FindChild(name_pieces.front());
    //新节点，则向字典树插入节点
    if (child == nullptr) {
      child = current_node->AddChild(name_pieces.front());
    }
    //添加失败处理
    if (child == nullptr) {
      *error = "Unable to allocate Trie node";
      return false;
    }
    //新插入节点作为当前节点，以便再次循环
    current_node = child;
    //name_pieces的第一项插入到字典树，因此去除第一项
    name_pieces.erase(name_pieces.begin());
  }

  return true;
}
```

以下面属性为例

```
dalvik.vm.checkjni u:object_r:exported_dalvik_prop:s0 exact bool
dalvik.vm.dex2oat-Xms u:object_r:exported_dalvik_prop:s0 exact string
dalvik.vm.dex2oat-Xmx u:object_r:exported_dalvik_prop:s0 exact string
ro.emulator.            u:object_r:qemu_prop:s0
vendor.qemu             u:object_r:qemu_prop:s0
vendor.network          u:object_r:vendor_net:s0
```

会构建出如下字典树的一部分

![部分字典树1.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/2019c11489f44d4da30e19267ea8fd38~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743246851&x-signature=1Je1DvXkNBdz%2ByAzel%2BtHnGSrow%3D)

在上图所示字典树构建过程中current\_node会依次指向vm、ro、qemu等节点，然后调用如下代码进一步构建出完整字典树。

```
bool TrieBuilder::AddToTrie(const std::string& name, const std::string* context,
                            const std::string* type, bool exact, std::string* error) {
  TrieBuilderNode* current_node = &builder_root_;

  // Store our context based on what type of match it is.
  if (exact) {
    if (!current_node->AddExactMatchContext(name_pieces.front(), context, type)) {
      *error = "Duplicate exact match detected for '" + name + "'";
      return false;
    }
  } else if (!ends_with_dot) {
    if (!current_node->AddPrefixContext(name_pieces.front(), context, type)) {
      *error = "Duplicate prefix match detected for '" + name + "'";
      return false;
    }
  } else {
    auto child = current_node->FindChild(name_pieces.front());
    if (child == nullptr) {
      child = current_node->AddChild(name_pieces.front());
    }
    if (child == nullptr) {
      *error = "Unable to allocate Trie node";
      return false;
    }
    if (child->context() != nullptr || child->type() != nullptr) {
      *error = "Duplicate prefix match detected for '" + name + "'";
      return false;
    }
    child->set_context(context);
    child->set_type(type);
  }
  return true;
}
```

而这段代码的逻辑处理依据就是前面将的vendor\_property\_contexts内容与plat\_property\_contexts有以下几点差异,总结如下：

-   有exact关键字属性处理
-   没有exact关键字属性处理
    -   .结尾s属性处理
    -   非.结尾s属性处理 根据这些特性处理后，构建的完整字典树如下

![完整字典树.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/6786bf6f16344bc4996430e759d47dab~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743246851&x-signature=SaST6QS2F%2BSK1ttIASk1XWIFbCg%3D)

这里需要对TrieBuilderNode和PropertyEntryBuilder做一下详细说明

-   PropertyEntryBuilder
    -   name：当前节点的名字，例如图中vm节点
    -   context、type：如果该节点是整个属性的最后一个节点，那么值存在（例如emulator节点，因为ro.emulator没有定义type，所以没有值）；否则值为空
-   TrieBuilderNode
    -   property\_entry\_：记录当前节点的name、context、type信息
        
    -   children\_:子节点，例如dalvik.vm.checkjni，vm是dalvik的子节点
        
    -   exact\_matches\_:具有相同前缀且都有exact标志的属性（称为精准属性），都会放到此数组中，上图中如下属性就是此类情况
        
        ```
        dalvik.vm.checkjni u:object_r:exported_dalvik_prop:s0 exact bool
        dalvik.vm.dex2oat-Xms u:object_r:exported_dalvik_prop:s0 exact string
        dalvik.vm.dex2oat-Xmx u:object_r:exported_dalvik_prop:s0 exact string
        ```
        
    -   prefixes\_:具有相同前缀且没有exact标志的属性，都会放到此数组中，上图中如下属性就是此类情况
        
        ```
        vendor.qemu             u:object_r:qemu_prop:s0
        vendor.network          u:object_r:vendor_net:s0
        ```
        

有了上面这些知识后，读者分析上面这段代码就会变得简单许多（读者自行分析一下）。这里重点说一下.结尾的属性

```
auto child = current_node->FindChild(name_pieces.front());
if (child == nullptr) {
  child = current_node->AddChild(name_pieces.front());
}
if (child == nullptr) {
  *error = "Unable to allocate Trie node";
  return false;
}
if (child->context() != nullptr || child->type() != nullptr) {
  *error = "Duplicate prefix match detected for '" + name + "'";
  return false;
}
child->set_context(context);
child->set_type(type);
```

与其它类型属性不同，它会创建一个节点然后添加到字典树中，因为这个节点是整个属性最后的节点，所以会设置context和type。

## TrieBuilder序列化

```
//调用    
BuildTrie(property_infos, "u:object_r:default_prop:s0", "string", &serialized_contexts,
                   &error)
    
//实现
bool BuildTrie(const std::vector<PropertyInfoEntry>& property_info,
               const std::string& default_context, const std::string& default_type,
               std::string* serialized_trie, std::string* error) {
  //将vector<PropertyInfoEntry>转化为TrieBuilder数据结构
  // Check that names are legal first
  auto trie_builder = TrieBuilder(default_context, default_type);
  for (const auto& [name, context, type, is_exact] : property_info) {
    if (!trie_builder.AddToTrie(name, context, type, is_exact, error)) {
      return false;
    }
  }

  //使用TrieSerializer类对TrieBuilder中数据按照一定的结构写入内存，然后序列化为字符串
  auto trie_serializer = TrieSerializer();
  *serialized_trie = trie_serializer.SerializeTrie(trie_builder);
  return true;
}
```

接着看看SerializeTrie是如何实现序列化的

```
//system/core/property_service/libpropertyinfoserializer/trie_serializer.cpp
    
std::string TrieSerializer::SerializeTrie(const TrieBuilder& trie_builder) {
  arena_.reset(new TrieNodeArena());

  //arena_指针分配了一个header空间
  auto header = arena_->AllocateObject<PropertyInfoAreaHeader>(nullptr);
  //header写入数据
  header->current_version = 1;
  header->minimum_supported_version = 1;

  // Store where we're about to write the contexts.
  header->contexts_offset = arena_->size();
  //将TrieBuilder->contexts_序列化写入header的后面
  SerializeStrings(trie_builder.contexts());

  // Store where we're about to write the types.
  header->types_offset = arena_->size();
  //将TrieBuilder->types_序列化写入TrieBuilder->contexts_的后面
  SerializeStrings(trie_builder.types());

  // We need to store size() up to this point now for Find*Offset() to work.
  header->size = arena_->size();
  //将TrieBuilder->builder_root整颗字典树写入TrieBuilder->types_的后面
  uint32_t root_trie_offset = WriteTrieNode(trie_builder.builder_root());
  header->root_offset = root_trie_offset;

  // Record the real size now that we've written everything
  header->size = arena_->size();

  return arena_->truncated_data();
}
```

经过上面 4 部分写入后，内存中的数据排列如下：

![序列化后内存数据结构.png](https://p6-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/c3a2b86acaec4c9a9578208648fb8e06~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743246851&x-signature=z8ms7y2gXieATOIr4Pej5fJOkoY%3D)

最后回到BuildTrie调用的地方

```
void CreateSerializedPropertyInfo() {
    auto serialized_contexts = std::string();
    auto error = std::string();
    if (!BuildTrie(property_infos, "u:object_r:default_prop:s0", "string", &serialized_contexts,
                   &error)) {
        LOG(ERROR) << "Unable to serialize property contexts: " << error;
        return;
    }

    constexpr static const char kPropertyInfosPath[] = "/dev/__properties__/property_info";
    if (!WriteStringToFile(serialized_contexts, kPropertyInfosPath, 0444, 0, 0, false)) {
        PLOG(ERROR) << "Unable to write serialized property infos to file";
    }
    selinux_android_restorecon(kPropertyInfosPath, 0);
}
```

-   serialized\_contexts为序列化返回的字符串
-   WriteStringToFile将serialized\_contexts写入到"/dev/**properties**/property\_info"中

到此属性安全上下文序列化玩成。