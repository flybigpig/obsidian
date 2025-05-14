---
created: 2025-05-14T16:37:17 (UTC +08:00)
tags: [gralloc]
source: https://blog.csdn.net/wudexiaoade2008/article/details/144167563
author: 成就一亿技术人!
---

# Android 图形系统之五：Gralloc-CSDN博客

> ## Excerpt
> 文章浏览阅读1.7k次，点赞26次，收藏30次。Gralloc (Graphics Allocator) 是 Android 系统中的关键组件之一，用于管理图形缓冲区的分配、映射以及处理。在 Android 的图形架构中，Gralloc 充当了 HAL (Hardware Abstraction Layer) 的一部分，为系统和硬件提供了通用的接口，使应用程序能够高效地处理图形数据。_gralloc

---
Gralloc (Graphics Allocator) 是 [Android](https://so.csdn.net/so/search?q=Android&spm=1001.2101.3001.7020) 系统中的关键组件之一，用于管理图形缓冲区的分配、映射以及处理。在 Android 的图形架构中，Gralloc 充当了 HAL (Hardware Abstraction Layer) 的一部分，为系统和硬件提供了通用的接口，使应用程序能够高效地处理图形数据。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/53a77925fe074d828f5ca82063438ca9.png#pic_center)

### Gralloc 的主要功能

Gralloc 的功能可以划分为以下几个核心部分：

#### 1\. **图形缓冲区分配**

-   **目标**：为应用程序或系统组件分配物理内存，用于存储显示图形数据。
-   **细节**：缓冲区的分配依据 **宽度、高度、像素格式** 和 **使用场景 (Usage Flags)**。显存分配通常由底层 GPU 或特定硬件模块管理。

#### 2\. **内存映射**

-   **目标**：通过映射将物理内存的缓冲区暴露给用户态程序。
-   **细节**：映射操作将内存映射到调用进程的虚拟地址空间。支持映射为 CPU 可访问的地址（用户态程序可直接操作数据）。

#### 3\. **跨进程共享**

-   **目标**：缓冲区能在不同进程间高效传递。
-   **细节**：Gralloc 使用文件描述符 (FD) 作为缓冲区的共享句柄。通过 binder 或其他机制，FD 可在生产者和消费者之间传递，无需拷贝数据。

#### 4\. **缓冲区属性管理**

-   **目标**：为缓冲区附加元数据，描述缓冲区的用途和限制。
-   **细节**：包括 **像素格式**（RGB、YUV 等）、**宽高尺寸**、**使用标志**（Usage Flags）等。这些属性会影响底层驱动分配显存的策略。

### Gralloc 的架构设计

Gralloc 的架构可以分为以下几个层次：

#### 1\. **高层：应用程序接口**

-   上层应用通常通过图形库（如 OpenGL ES、Vulkan）或 Android 提供的 UI 组件（如 SurfaceView、TextureView）使用 Gralloc。
-   示例：OpenGL ES 使用 GPU 渲染后，需要通过 Gralloc 分配的缓冲区将结果共享到显示层。视频解码时，MediaCodec 使用 Gralloc 分配的缓冲区存储解码后的帧。

#### 2\. **中间层：Framework 和 HAL**

-   **Gralloc HAL**提供抽象接口，屏蔽硬件细节，支持多种硬件实现。HAL 的接口通过 allocator 和 mapper 提供主要功能：IGraphicBufferAllocator：缓冲区分配接口。IGraphicBufferMapper：缓冲区映射、属性管理和释放接口。HAL 的实现位于 /hardware/interfaces/graphics/。
-   **BufferQueue\*\*\*\*BufferQueue** 是 Android 图形堆栈的核心组件，管理生产者（Producer）和消费者（Consumer）之间的缓冲区流转。生产者使用 Gralloc 分配缓冲区后，交由消费者处理。BufferQueue 通过 Binder IPC 支持跨进程通信。

#### 3\. **底层：驱动与硬件**

-   Gralloc 的底层实现依赖硬件供应商提供的驱动支持：显存分配：显存通常由 GPU 驱动分配。DMA 支持：某些硬件支持通过 DMA 直接访问内存。Cache 操作：确保 CPU 和 GPU 对缓冲区的访问保持一致。
-   **映射内存管理**内存映射需要确保多进程、多设备的一致性，通常通过内核接口（如 mmap）实现。

#### Gralloc 的关键组件

##### 1\. **GraphicBuffer**

-   它是 Android Framework 中用于表示图形缓冲区的核心类，封装了分配、引用和访问缓冲区的功能。

```
sp buffer = new GraphicBuffer(width, height, format, usage);
```

##### 2\. **BufferQueue**

-   BufferQueue 是 Android 图形堆栈的核心组件之一，负责管理生产者（Producer）和消费者（Consumer）之间的缓冲区传递。

##### 3\. **Buffer 分配策略**

Gralloc 的分配通常基于以下参数：

-   **大小（宽度和高度）**
-   **格式（如 RGB、YUV）**
-   **用途（Usage Flags，例如 CPU/GPU 读取或写入）**

**示例 Usage Flags**：

-   GRALLOC\_USAGE\_SW\_READ\_OFTEN：缓冲区需要频繁被软件读取。
-   GRALLOC\_USAGE\_HW\_TEXTURE：缓冲区用作 GPU 纹理。

### [数据流](https://so.csdn.net/so/search?q=%E6%95%B0%E6%8D%AE%E6%B5%81&spm=1001.2101.3001.7020)分析

以应用程序绘制一帧图像到屏幕为例，Gralloc 的典型数据流如下：

1.  **分配缓冲区**
    -   应用通过 SurfaceFlinger 或其他接口请求分配图形缓冲区。
    -   SurfaceFlinger 调用 Gralloc HAL 完成分配。
    -   分配的缓冲区封装为 GraphicBuffer。
2.  **写入缓冲区**
    -   生产者（如 GPU）通过 OpenGL/Vulkan API 渲染到缓冲区。
    -   Gralloc 确保缓冲区内存可被写入。
3.  **共享缓冲区**

-   渲染完成后，缓冲区通过 BufferQueue 提供给消费者。
-   消费者可以是显示系统（如 SurfaceFlinger）或其他应用程序。

4.  **释放缓冲区**
    -   使用完成后，缓冲区由 Gralloc HAL 释放，回收资源。

### Gralloc 的具体实现

#### 1\. **Gralloc HAL 接口**

##### 接口定义

Gralloc [HAL](https://so.csdn.net/so/search?q=HAL&spm=1001.2101.3001.7020) 主要接口位于 `/hardware/interfaces/graphics/allocator` 和 `/hardware/interfaces/graphics/mapper` 中，定义如下：

-   **IGraphicBufferAllocator**：分配缓冲区

```
// allocator.hal
virtual Error allocate(
    uint32_t width, uint32_t height, PixelFormat format,
    uint64_t usage, uint32_t stride, buffer_handle_t* handle) = 0;
```

-   **IGraphicBufferMapper**：映射和管理缓冲区

```
// mapper.hal
virtual Error lock(
    buffer_handle_t buffer, uint64_t usage,
    const Rect& accessRegion, void** outData) = 0;

virtual Error unlock(buffer_handle_t buffer) = 0;
```

#### 2\. **GraphicBuffer 类**

`GraphicBuffer` 是 Android Framework 层对 Gralloc 分配的缓冲区的封装，定义在 `/frameworks/native/libs/ui/GraphicBuffer.h`。

**构造函数示例**：

```
GraphicBuffer::GraphicBuffer(
    uint32_t width, uint32_t height, PixelFormat format, uint32_t usage)
{
    sp allocator = GraphicBufferAllocator::get();
    allocator->allocate(width, height, format, usage, &mHandle, &mStride);
}
```

### 代码示例

以下是使用 Gralloc HAL 分配缓冲区的代码示例：

```
#include 
#include 

const hw_module_t* module;
gralloc_module_t* gralloc;
buffer_handle_t buffer;
int stride;
void* mappedData;

// 初始化 Gralloc 模块
if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) == 0) {
    gralloc = reinterpret_cast(module);

    // 分配缓冲区
    gralloc->alloc(gralloc, 1920, 1080, HAL_PIXEL_FORMAT_RGBA_8888,
                   GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN,
                   &buffer, &stride);

    // 映射缓冲区
    gralloc->lock(gralloc, buffer, GRALLOC_USAGE_SW_WRITE_OFTEN, 0, 0, 1920, 1080, &mappedData);

    // 操作缓冲区数据
    memset(mappedData, 0, 1920 * 1080 * 4); // 清除为黑色

    // 取消映射缓冲区
    gralloc->unlock(gralloc, buffer);

    // 释放缓冲区
    gralloc->free(gralloc, buffer);
}
```

### Android 11 及后续版本的变化

1.  **Allocator 3.x 和 Mapper 4.x**
    -   新版本通过 AIDL/HIDL 提供更统一的分配和管理接口。
    -   增强了跨设备的兼容性。
2.  **GPU 缓存一致性优化**
    -   增加支持 CPU/GPU 混合访问场景的优化，减少同步开销。

#### 相关资料

-   Graphics HAL 源码
-   [Android Graphics Architecture](https://source.android.com/docs/core/graphics/overview)
