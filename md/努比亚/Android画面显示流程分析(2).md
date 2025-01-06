_努比亚技术团队原创内容，转载请务必注明出处。_

[Android画面显示流程分析(1)](https://www.jianshu.com/p/df46e4b39428)  
[Android画面显示流程分析(2)](https://www.jianshu.com/p/f96ab6646ae3)  
[Android画面显示流程分析(3)](https://www.jianshu.com/p/3c61375cc15b)  
[Android画面显示流程分析(4)](https://www.jianshu.com/p/7a18666a43ce)  
[Android画面显示流程分析(5)](https://www.jianshu.com/p/dcaf1eeddeb1)

## 3. DRM

DRM，英文全称 Direct Rendering Manager, 即 直接渲染管理器。

DRM是linux内核的一个子系统，它提供一组API，用户空间程序可以通过它发送画面数据到GPU或者专用图形处理硬件（如高通的MDP），也可以使用它执行诸如配置分辨率，刷新率之类的设置操作。原本是设计提供给PC使用来支持复杂的图形设备，后来也用于嵌入式系统上。目前在高通平台手机Android系统上的显示系统也是使用的这组API来完成画面的渲染更新。

在DRM之前Linux内核已经有一个叫FBDEV的API，用于管理图形适配器的帧缓存区，但不能用于满足基于3D加速的现代基于GPU的视频硬件的需求，FBDEV社区维护者也较少; 且无法提供overlay hw cursor这样的features; 开发者们本身就鼓励以后迁移到DRM上。

### 3.1. 基本组件

DRM主要由如下部分组成：

**KMS**(Kernel Mode Setting): 主要是配置信息管理，如改变分辨率，刷新率，位深等  
**DRI**(Direct Rendering Infrastructure):　可以通过它直接访问一些硬件接口  
**GEM**(Graphics Execution Manager): 主要负责内存管理，CPU， GPU对内存的访问控制由它来完成。  
**DRM Driver in kernel side**: 访问硬件

在高通平台上其中部分模块所处位置见下图：

![](//upload-images.jianshu.io/upload_images/26874665-710256063273a099.png?imageMogr2/auto-orient/strip|imageView2/2/w/670/format/webp)

image-20210915104207418.png

其中KMS由frame buffer， CRTC, Encoder, Connector等组件组成

**CRTC**  
CRT controller,目前主要用于显示控制，如显示时序,分辨率，刷新率等控制，还要承担将framebuffer内容送到display,更新framebuffer等。

**Encoder**  
负责将数据转换成合适的格式，送给connector，比如HDMI需要TMDS信息, encoder就将数据转成HDMI需要的TMDS格式。

**Connector**  
它是具体某种显示接口的代表，如 hdmi, mipi等。用于传输信号给外部硬件显示设备，探测外部显示设备接入。

**Planes**  
一个Plane代表一个image layer, 最终的image由一个或者多个Planes组成

在Android系统上DRM就是通过KMS一面接收userspace交付的应用画面，一面通过其connector来向屏幕提交应用所绘制的画面。

### 3.2. DRM使用示例

如下仅是示意代码，篇幅所限只摘取了完整代码中的部分关键代码，代码演示的是初始化，创建surface, 在surface上作画（这里只是画了一张全红色的画面），然后通过page flip方式将画面更新到屏幕的过程。

1. 定义一些全局变量：

```cpp
......
#include <drm_fourcc.h>
#include <drm.h>
......
static drmModeCrtc *main_monitor_crtc;
static drmModeConnector *main_monitor_connector;
static int drm_fd = -1;
static drmModeRes *res = NULL;
```

2. 打开drm设备节点/dev/dri/card0

```bash
 uint64_t cap = 0;
 drm_fd = open("/dev/dri/card0", O_RDWR, 0);
 ret = drmGetCap(drm_fd, DRM_CAP_DUMB_BUFFER, &cap);
 res = drmModeGetResources(drm_fd);
```

3. 找到所使用的connector及其mode

```php
    int i = 0;
 //find main connector
    for(i = 0; i < res->count_connectors;i++) {
     drmModeConnector *connector;
        connector = drmModeGetConnector(drm_fd, res->connectors[i]);
        if(connector) {
            if((connector->count_modes > 0) && connector->connection == DRM_MODE_CONNECTED)) {
                main_monitor_connector = connector;
                break;
            }
            drmModeFreeConnector(connector);
        }
    }
    ......
    uint32_t select_mode = 0;
    for(int modes = 0; modes < main_monitor_connector->count_modes; modes++) {
        if(main_monitor_connector->modes[modes].type & DRM_MODE_TYPE_PREFERRED) {
            select_mode = modes;
            break;
        }
    }
```

4. 获取当前显示器的一些信息如宽高

```php
 drmModeEncoder *encoder = drmModeGetEncoder(drm_fd, main_monitor_connector->encoder_id);
 int32_t crtc = encoder->crtc_id;
 drmModeFreeEncoder(encoder);    
 drmModeCrtc *main_monitor_crtc = drmModeGetCrtc(fd, crtc);
 main_monitor_crtc->mode = mina_monitor_connector->modes[select_mode];
 int width = main_monitor_crtc->mode.hdisplay;
 int height = main_monitor_crtc->mode.vdisplay
```

5. 创建一个画布

```cpp
 struct GRSurface *surface;
 struct drm_mode_create_dumb create_dumb;
 uint32_t format;
 int ret;
 
 surface = (struct GRSurface*)calloc(1, sizeof(*surface));
 format = DRM_FORMAT_ARGB8888;
 ......
 create_dumb.height = height;
 create_dumb.width = width;
 create_dumb.bpp = drm_format_to_bpp(format);
 create_dumb.flags = 0;
 ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);
 surface->handle = create_dumb.handle;
 ......
 //创建一个FrameBuffer
 ret = drmModeAddFB2(drm_fd, width, height, fromat, handles, pitches, offsets, &(surface->fb_id), 0);
 ......
 ret = drmIoCtl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb);
 ......
 //这里通过mmap的方式把画布对应的buffer映射到本进程空间来
 surface->data =(unsigned char*)mmap(NULL, height*create_dumb.pitch, PROT_READ|PROT_WRITE, MAP_SHARED, drm_fd, map_dumb.offset);
 drmModeSetCrtc(drm_fd, main_monitor_crtc->crtc_id,
                 0, //fb_id
                 0, 0,//x,y
                 NULL, //connectors
                 0,//connector_count
              NULL);//mode    
```

6. 在画布上作画，这里是画了一个纯红色的画面
    
    ```cpp
        int x, y;
        unsigned char* p = surface->data;
        for(y = 0; y < height; y++) {
            unsigned char *px = p;
            for(x = 0; x < width; x++){
                *px++ = 255;//r
                *px++ = 0;  //g
                *px++ = 0;  //b
                *px++ = 255; //a
            }
            p += surface->row_bytes;
        }
    ```
    
7. 通过page flip将画面送向屏幕
    
    ```php
       drmModePageFlip(drm_fd, main_monitor_crtc->crtc_id, surface->fb_id, 0, NULL); 
    ```
    
    需要注意的是/dev/dri/card0的使用方式是独占的，也就是如果一个进程open了这个节点，其他进程是无法再打开的，在android平台测试时要运行测试程序时需要将原来的进程先kill掉，如在高通平台上要先kill掉这个进程：vendor.qti.hardware.display.composer-service，由于它会自动重启，所以要将它的执行文件 /vendor/bin/hw/vendor.qti.hardware.display.composer-service 重命名或删掉，才能做测试.
    

### 3.3. 本章小结

在本章节中我们认识了linux给userspace提供的屏幕操作的接口，通过一个简单例子粗略地了解了这些接口的一个用法，让我们知晓了可以通过这组api来向屏幕来提交我们所绘制的画面。那么在Android的display架构中是谁在使用这组api呢？

## 4. Userspace的帧数据流

在Android系统上应用要绘制一个画面，首先要向SurfaceFlinger申请一个画布，这个画布所使用的buffer是SurfaceFlinger通过allocator service（vendor.qti.hardware.display.allocator-service）来分配出来的，allocator service是通过ION从kernel开辟出来的一块共享内存，这里申请的都是每个图层所拥有独立buffer, 这个buffer会共享到HWC Service里，由SurfaceFlinger来作为中枢控制这块buffer的所有权，其所有权会随状态不同在App, SurfaceFlinger, HWC Service间来回流转。

  

![](//upload-images.jianshu.io/upload_images/26874665-33f463b655b32880.png?imageMogr2/auto-orient/strip|imageView2/2/w/793/format/webp)

image-20210915175702338.png

而HWC Service正是那个使用libdrm和kernel打交道的人 ，它负责把SurfaceFlinger交来的图层做合成，将合成后的画画提交给DRM去显示。

### 4.1. App到SurfaceFlinger

应用首先通过Surface的接口向SurfaceFlinger申请一块buffer, 需要留意的是Surface刚创建时是不会有buffer被alloc出来的，只有应用第一次要绘制画面时dequeueBuffer会让SurfaceFlinger去alloc buffer, 在应用侧会通过importBuffer把这块内存映射到应用的进程空间来，这个过程可以在systrace上观察到：

  

![](//upload-images.jianshu.io/upload_images/26874665-a98a3ecf047e6dd4.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210916102456850.png

之后App通过dequeueBuffer拿到画布， 通过queueBuffer来提交绘制好的数据， 这个过程可以在如下systrace上观察到：

  

![](//upload-images.jianshu.io/upload_images/26874665-141e8bca64c47ab5.png?imageMogr2/auto-orient/strip|imageView2/2/w/1185/format/webp)

image-20210915204636105.png

HWC Service负责将SurfaceFlinger送来的图层做合成，形成最终的画面，然后通过drm的接口更新到屏幕上去（注意：在DRM一章中给出的使用DRM的例子子demo的是通过page flip方式提交数据的，但hwc service使用的是另一api atomic commit的方式提交数据的，drm本身并不只有一种方式提交画面）

### 4.2. SurfaceFlinger到HWC Service

HWC Service的代码位置在 hardware/qcom/display, HWC Service使用libdrm提交帧数据的地方我们可以在systrace上观察到：

![](//upload-images.jianshu.io/upload_images/26874665-458af151c808a23e.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210915202625562.png

而上图中的DRMAtomicReq::Commit会调用到

drmModeAtomicCommit这个接口，该接口定义在 externel/libdrm/xf86drmMode.h， 其原型如下

```cpp
........
extern int drmModeAtomicCommit(int fd, drmModeAtomicReqPtr req, uint32_t flags, void *user_data);
.......
```

PageFlip方式的接口也是定义在这里：

```cpp
........
extern int drmModePageFlip(int fd, uint32_t crtc_id, uint32_t fb_id, uint32_t flags, void *user_data);
........
```

### 4.3. HWC Service到kernel

hwc service通过drmModeAtomicCommit接口向kernel提交合成数据：

代码位置位于：hardware/qcom/display/sde-drm/drm_atomic_req.cpp

```cpp
int DRMAtomicReq::Commit(bool synchronous, bool retain_planes) {
    DTRACE_SCOPED();//trace
    ......
    int ret = drmModeAtomicCommit(fd_, drm_atomic_req_, flags, nullptr);
    ......
}
```

drmModeAtomicCommit通过ioctl调用到kernel：

kernel/msm-5.4/techpack/display/msm/msm_atomic.c

```rust
static void _msm_drm_commit_work_cb(struct kthread_work *work) {
    ......
    SDE_ATRACE_BEGIN("complete_commit");
    complete_commit(commit);
    SDE_ATRACE_END("complete_commit");    
    ......
}

static struct msm_commit *commit_init(struct drm_atomic_state *state, bool nonblock) {
    ......
    kthread_init_work(&c->commit_work, _msm_drm_commit_work_cb);//将callback注册到commit_work
    ......
}

static void msm_atomic_commit_dispatch(struct drm_device *dev, 
          struct drm_atomic_state *state, struct msm_commit *commit) {
    ......
         kthread_queue_work(&priv->disp_thread[j].worker, &commit->commit_work);//向消息队列中加入一个消息，disp thread处理到该消息时会调用到_msm_drm_commit_work_cb
    ......
}

```

drmModeAtomicCommit的ioctl会触发msm_atomic_commit_dispatch，然后通知disp thread也就是如下图所示的crtc_commit线程去处理这个消息，然后执行

complete_commit函数，这个过程见下图：

![](//upload-images.jianshu.io/upload_images/26874665-7f830fb48d7758fc.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210916094646924.png

### 4.4. 本章小结

在本章中我们了解了APP绘画的画布是由SurfaceFlinger提供的，而画布是一块共享内存，APP向SurfaceFlinger申请到画布，是将共享内存的地址映射到自身进程空间。 App负责在画布上作画，画完的作品提交给SurfaceFlinger， 这个提交操作并不是把内存复制一份给SurfaceFlinger，而是把共享内存的控制权交还给SurfaceFlinger， SurfaceFlinger把拿来的多个应用的共享内存再送给HWC Service去合成， HWC Service把合成的数据交给DRM去输出完成app画面显示到屏幕上的过程。为了更有效地利用时间这样的共享内存不止一份，可能有两份或三份，即常说的double buffering, triple buffering.

那么我们就需要设计一个机制可以管理buffer的控制权，这个就是BufferQueue.

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/f96ab6646ae3  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。