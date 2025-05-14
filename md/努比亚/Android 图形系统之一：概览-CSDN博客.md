---
created: 2025-05-14T16:30:53 (UTC +08:00)
tags: [android 图形系统]
source: https://blog.csdn.net/wudexiaoade2008/article/details/144095149
author: 成就一亿技术人!
---

版权声明：本文为博主原创文章，遵循 [CC 4.0 BY-SA](http://creativecommons.org/licenses/by-sa/4.0/) 版权协议，转载请附上原文出处链接和本声明。

[Android](https://so.csdn.net/so/search?q=Android&spm=1001.2101.3001.7020) 图形系统是一套完整的架构，用于管理从应用绘制到显示屏幕的整个流程。它涉及多个层次和组件，从应用程序到硬件，确保每一帧都能准确、高效地呈现到用户的设备屏幕上。

### **1. Android 图形系统的架构**

Android 图形系统的架构可以分为以下几层：

#### **1.1 应用层**

- **主要功能**：负责生成绘制内容（UI）。
    
- **核心组件**：
    
    - **View 和 ViewGroup**：应用 UI 的基础构件。
        
    - **Canvas**：绘制 2D 图形的接口。
        
    - **SurfaceView/TextureView**：支持自定义绘制和高性能显示。
        

#### **1.2 框架层**

- **主要功能**：桥接应用层和底层图形系统。
    
- **核心组件**：
    
    - **ViewRootImpl**：管理 View 的绘制、布局以及输入事件的分发。
        
    - **SurfaceControl**：控制 Surface（窗口或图层）的创建、属性设置等。
        
    - **Choreographer**：协调绘制和显示的同步，处理 VSync 信号。
        

#### **1.3 原生层**

- **主要功能**：管理图形缓冲区和帧合成。
    
- **核心组件**：
    
    - **SurfaceFlinger**：Android 的窗口合成器，负责将多个应用的窗口内容合成到屏幕。
        
    - **BufferQueue**：生产者和消费者模型，负责在应用和 SurfaceFlinger 之间传递帧。
        
    - **Gralloc**：图形内存分配器，为图形缓冲区分配共享内存。
        

#### **1.4 硬件抽象层 (HAL)**

- **主要功能**：为系统提供统一的硬件接口。
    
- **核心组件**：
    
    - **Hardware Composer (HWC)**：负责与显示硬件交互，优化图层合成（如直接使用硬件 Overlays）。
        
    - **OpenGL ES/Vulkan**：提供 GPU 加速的渲染接口。
        

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/aed1b48da2f44427a8c21702fd8cbc01.png#pic_center) 图片源于[Linux Graphic Stack相關的名詞](https://oopsmonk.github.io/posts/2017-05-30-linux-graphic-stack/)

#### **1.5 硬件层**

- **主要功能**：包括 GPU 和显示控制器。
    
- **核心组件**：**GPU**：负责执行 OpenGL ES 或 Vulkan 命令，处理复杂图形计算。
    
- **显示控制器**：直接控制显示设备，如屏幕刷新。
    

### **2. 图形系统的关键流程**

#### **2.1 渲染和绘制流程**

以下是从应用代码到屏幕内容呈现的完整流程：

1. **应用绘制**：
    
    - 应用通过 View 或 Canvas 发起绘制。
        
    - 绘制内容被写入 Surface（帧缓冲区）。
        
2. **帧提交**：
    
    - 应用通过 Surface 提交绘制的帧，写入 BufferQueue 的生产端。
        
3. **SurfaceFlinger 合成**：
    
    - 从 BufferQueue 的消费端获取缓冲区。
        
    - 通过 GPU 或 HWC 合成所有窗口的图层。
        
    - 最终合成的帧被提交给显示设备。
        
4. **屏幕显示**：
    
    - 显示控制器接收最终帧，并根据 VSync 刷新显示屏幕。
        

#### **2.2 VSync 信号和帧同步**

- **VSync 信号**：由显示控制器产生，用于驱动屏幕刷新。
    
- **帧同步流程**：VSync 信号触发 Choreographer。应用通过 doFrame 回调完成 UI 渲染。SurfaceFlinger 在下一次 VSync 周期前完成合成。
    

### **3. Android 图形系统的核心组件**

#### **3.1 SurfaceFlinger**

- **职责**：管理窗口的图层合成和屏幕内容显示。
    
- **工作方式**：使用 GPU 或 HWC 完成图层的合成。确保屏幕内容按正确顺序和属性（透明度、旋转等）呈现。
    

#### **3.2 Hardware Composer (HWC)**

- **职责**：直接与显示硬件交互，优化合成。
    
- **功能**：将部分图层直接传递给硬件 Overlay，避免 GPU 合成。
    

#### **3.3 BufferQueue**

- **职责**：在应用和 SurfaceFlinger 之间传递缓冲区。
    
- **模型**：**生产者**：应用写入图像帧。**消费者**：SurfaceFlinger 获取并合成图像帧。
    

### **4. Android 图形性能调优**

#### **4.1 性能指标**

- **FPS（帧率）**：反映屏幕的流畅度。
    
- **Frame Time（帧时间）**：每帧所需时间，理想值为 16.67ms（60Hz 屏幕）。
    
- **Jank（卡顿）**：掉帧导致的用户体验不佳。
    

#### **4.2 性能瓶颈**

- **CPU 过载**：应用逻辑过于复杂。
    
- **GPU 瓶颈**：复杂绘制导致 GPU 渲染延迟。
    
- **缓冲区问题**：BufferQueue 过满或过慢导致帧延迟。
    

#### **4.3 调试工具**

1. **Systrace**：
    
    - 分析应用和系统的图形性能。
        
    - 定位掉帧和绘制延迟问题。
        
2. **GPU Profiler**：
    
    - 查看 GPU 的性能使用情况。
        
    - 分析渲染效率。
        
3. **Perfetto**：
    
    - 深入分析系统级性能，包括 SurfaceFlinger 和 HWC 的行为。
        

### **5. 图形系统的未来趋势**

1. **高刷新率支持**：适配 120Hz 或更高刷新率设备。
    
2. **Vulkan 加速**：更多应用使用 Vulkan 进行高性能渲染。
    
3. **多窗口和多显示支持**：优化多任务和外接显示器的用户体验。
    
4. **节能优化**：利用硬件 Overlay 和 Display Panel 自适应刷新率降低功耗。
    

### **图示：Android 图形架构**

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/fd3587330ff1465db7dff92939595ceb.png#pic_center)

图片源于[Android进阶宝典 – Android系统图像绘制原理大全解](https://juejin.cn/post/7258675185548574776)

如果对某一模块感兴趣，可以进一步深入讲解实现细节或调试方法！