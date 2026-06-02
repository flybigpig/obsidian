## 概述

RenderThread 是 Android 5.0 (Lollipop) 引入的独立渲染线程，用于将渲染工作从 UI Thread 分离出来，提升应用流畅性。其核心流程为：**sync → draw → GPU 渲染**。

## RenderThread 完整主线流程

```
┌─────────────────────────────────────────────────────────────────┐
```

## DisplayList 构建流程详解

### 什么是 DisplayList

DisplayList（显示列表）是 Android 硬件加速渲染的核心机制，它是一个**记录绘制指令的缓存数据结构**，而不是立即执行绘制操作。当 View 需要重新绘制时，Android 会将绘制指令记录到 DisplayList 中，然后由 RenderThread 异步执行这些指令。

**核心优势：**

1.  **分离记录和执行**
    
    ：UI Thread 只负责记录，RenderThread 负责执行
    
2.  **缓存复用**
    
    ：View 未改变时可复用之前的 DisplayList
    
3.  **异步渲染**
    
    ：不阻塞 UI Thread，提升流畅性
    
4.  **优化空间**
    
    ：可在 RenderThread 进行裁剪、合并等优化
    

### DisplayList 构建完整流程

```
┌─────────────────────────────────────────────────────────────────┐
```

### DisplayList 记录的绘制操作类型

```
┌─────────────────────────────────────────────────────────────────┐
```

### DisplayList 与 View 树的映射关系

```
┌─────────────────────────────────────────────────────────────────┐
```

### DisplayList 的缓存和复用机制

```
┌─────────────────────────────────────────────────────────────────┐
```

DisplayList 的内存结构  

```
┌─────────────────────────────────────────────────────────────────┐
```

### DisplayList 的性能优化建议

```
┌─────────────────────────────────────────────────────────────────┐
```

### DisplayList 在 Perfetto 中的追踪

```
┌─────────────────────────────────────────────────────────────────┐
```

## RenderThread三大阶段详细分析

### Phase 1: SYNC 阶段 (同步阶段)

```
┌─────────────────────────────────────────────────────────────┐
```

**SYNC 阶段关键点：**

1.  **数据同步**
    
    ：将 UI Thread 的更改同步到 RenderThread
    
2.  **DisplayList 传递**
    
    ：传递记录的绘制指令
    
3.  **属性更新**
    
    ：同步 View 的变换属性（位移、旋转、缩放、透明度）
    
4.  **动画状态**
    
    ：更新动画的当前状态
    
5.  **RenderNode 更新**
    
    ：更新渲染节点树结构
    
6.  **线程解耦**
    
    ：UI Thread 可以继续准备下一帧，不阻塞
    

### Phase 2: DRAW 阶段 (绘制阶段)

```
┌─────────────────────────────────────────────────────────────┐
```

**DRAW 阶段关键点：**

1.  **树遍历**
    
    ：深度优先遍历 RenderNode 树
    
2.  **裁剪优化**
    
    ：
    

-   Clip rect：裁剪不可见区域
    
-   Frustum culling：视锥体剔除
    
-   Occlusion culling：遮挡剔除
    

3.  **Layer 处理**
    
    ：
    

-   Hardware Layer 缓存
    
-   Texture 复用
    
-   Layer 合并优化
    

4.  **绘制排序**
    
    ：按 Z-order 排序，减少 overdraw
    
5.  **Batch 优化**
    
    ：合并相似渲染操作
    
6.  **指令转换**
    
    ：将高层绘制指令转换为 GPU API 调用
    

Phase 3: GPU 渲染阶段

```
┌─────────────────────────────────────────────────────────────┐
```

**GPU 渲染阶段关键点：**

## 时序图

2.  **swapBuffers**
    
    ：交换前后缓冲区
    
3.  **命令提交**
    
    ：将 OpenGL/Vulkan 指令提交到 GPU
    
4.  **GPU Pipeline**
    
    ：
    

## 时序图

## 时序图

-   顶点着色器：处理顶点变换
    
-   片段着色器：处理像素着色
    
-   光栅化：将几何图形转换为像素
    
-   纹理采样：应用纹理
    
-   混合和深度测试
    

## 时序图

5.  **异步渲染**
    
    ：RenderThread 提交命令后可继续工作
    
6.  **Fence 同步**
    
    ：GPU Fence 信号通知渲染完成
    
7.  **Buffer 提交**
    
    ：queueBuffer 将渲染结果提交给 SurfaceFlinger
    

## 时序图

## 时序图

```java
时序图时间线 (单位: ms，以 16.6ms 为例)═════════════════════════════════════════════════════════════0ms                    8ms                   16.6ms│                      │                      │▼                      ▼                      ▼Vsync-app             Vsync-app             Vsync-app│                      │                      ││                      │                      │UI Thread (Frame N):   │                      │├─ Choreographer       │                      │├─ measure/layout      │                      │├─ draw (2ms)          │                      │└─ build DisplayList   │                      ││                   │                      ││ 触发 RenderThread  │                      │▼                   │                      │RenderThread (Frame N):│                      │├─ SYNC (1ms) ─────────┤                      │├─ DRAW (3ms) ─────────┼──────────┤           │└─ GPU submit          │          │           ││                   │          │           ││                   │          ▼           ││                   │     GPU Render       ││                   │     (6ms)            ││                   │          │           ││                   │          ▼           ││                   │     queueBuffer      ││                   │          │           │UI Thread (Frame N+1): │          │           │├─ Choreographer ──────┤          │           │├─ measure/layout      │          │           │├─ draw                │          │           │└─ build DisplayList   │          │           ││                   │          │           │▼                   ▼          ▼           ▼RenderThread (Frame N+1):                     │├─ SYNC ───────────────┼──────────┤           │├─ DRAW ───────────────┼──────────┼───────┤   │└─ GPU submit          │          │       │   ││          │       │   ││          │       ▼   ││          │   GPU Render (Frame N+1)
```

## 性能优化要点

### 1\. SYNC 阶段优化

## 时序图

-   **减少 DisplayList 大小**
    
    ：简化 View 层级
    
-   **避免频繁属性变更**
    
    ：批量更新属性
    
-   **使用 Hardware Layer**
    
    ：缓存复杂视图
    
-   **减少动画计算**
    
    ：使用插值器优化
    

### 2\. DRAW 阶段优化

## 时序图

-   **减少 Overdraw**
    
    ：避免多层重绘
    
-   **启用裁剪优化**
    
    ：clipRect 减少绘制区域
    
-   **合并绘制操作**
    
    ：使用 Canvas.saveLayer 合理
    
-   **优化 Path 绘制**
    
    ：简化复杂路径
    
-   **纹理复用**
    
    ：使用 Bitmap 缓存
    

### 3\. GPU 渲染阶段优化

## 时序图

-   **减少状态切换**
    
    ：批量渲染相同材质
    
-   **优化 Shader**
    
    ：简化着色器逻辑
    
-   **纹理压缩**
    
    ：使用 ETC2/ASTC 格式
    
-   **避免 GPU 过载**
    
    ：控制绘制复杂度
    
-   **使用 Vulkan**
    
    ：更高效的 GPU API (Android 7.0+)  
    

## 总结

RenderThread 的 **sync → draw → GPU 渲染** 流程是 Android 渲染架构的核心：

## 时序图

2.  **SYNC 阶段**
    
    ：同步 UI Thread 和 RenderThread 的状态，实现线程解耦
    
3.  **DRAW 阶段**
    
    ：遍历渲染树，构建 GPU 渲染指令，进行各种优化
    
4.  **GPU 渲染阶段**
    
    ：异步执行 GPU 渲染，通过 Fence 同步，提交给 SurfaceFlinger
    

这三个阶段形成流水线，使得 UI Thread 可以提前准备下一帧，RenderThread 专注渲染，GPU 异步执行，最大化利用多核和 GPU 资源，实现高性能渲染