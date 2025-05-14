---
created: 2025-05-14T16:41:38 (UTC +08:00)
tags: [Android显示系统设计框架介绍_快乐安卓-GitCode 开源社区]
source: https://gitcode.csdn.net/66dffe2d59bcf8384a5bf714.html?dp_token=eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpZCI6NTU3ODExLCJleHAiOjE3NDc4MTY4MTMsImlhdCI6MTc0NzIxMjAxMywidXNlcm5hbWUiOiJxcV8zNzM5NTg4NSJ9.vQEPbhjcOdEmM0uRamDp6S1U2swdk-ZCHGt_CI8PPUA&spm=1001.2101.3001.6650.4&utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromBaidu%7Eactivity-4-22647255-blog-129666617.235%5Ev43%5Epc_blog_bottom_relevance_base8&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromBaidu%7Eactivity-4-22647255-blog-129666617.235%5Ev43%5Epc_blog_bottom_relevance_base8&utm_relevant_index=8
author: 快乐安卓      
          30700人浏览 · 2014-04-02 08:32:22
---

# Android显示系统设计框架介绍_快乐安卓-GitCode 开源社区

> ## Excerpt
> 1. Linux内核提供了统一的framebuffer显示驱动，设备节点/dev/graphics/fb*或者/dev/fb*，以fb0表示第一个显示屏，当前实现中只用到了一个显示屏。2. Android的HAL层提供了Gralloc，分为fb和gralloc两个设备。设备fb负责打开内核中的framebuffer以及提供post、setSwapInterval等操作，设备gralloc则负责 快乐安卓 GitCode 开源社区

---
![](https://img-blog.csdn.net/20140331100620750?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQveWFuZ3dlbjEyMw==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)

1\. Linux内核提供了统一的framebuffer显示驱动，设备节点/dev/graphi[c](https://link.csdn.net/?target=https%3A%2F%2Fgitcode.com%2FopenHiTLS%2Fopenhitls%3Futm_source%3Ddevpress_gitcode_keyword%26login%3Dfrom_csdn)s/fb\*或者/dev/fb\*，以fb0表示第一个显示屏，当前实现中只用到了一个显示屏。

2\. Android的HAL层提供了Gralloc，分为fb和gralloc两个设备。设备fb负责打开内核中的framebuffer以及提供post、setSwapInterval等操作，设备gralloc则负责管理帧缓冲区的分配和释放。上层只能通过Gralloc访问帧缓冲区，这样一来就实现了有序的封装保护。

3\. 由于OpenGL ES是一个通用的函数库，在不同的平台系统上需要被“本地化”——即把它与具体平台上的窗口系统建立起关联，这样才能保证它正常工作。从FramebufferNativeWindow就是将OpenGL ES在Android平台上本地化窗口。

4\. OpenGL或者OpenGL ES 更多的只是一个接口协议，实现上既可以采用软件，也能依托于硬件。EGL通过读取egl.cfg配置文件，根据用户的设定来动态加载libagl(软件实现)或者libhgl(硬件实现)。然后上层才可以正常使用各种glXXX接口。

5\. SurfaceFlinger中持有一个GraphicPlane成员变量mGraphicPlanes来描述“显示屏”;GraphicPlane类中又包含了一个DisplayHardware对象实例(mHw)。DisplayHardware在初始化时还将调用eglInitialize、eglCreateWindowSurface等接口，并利用EGL完成对OpenGLES环境的搭建。

Android提供了两种本地窗口：

1)         面向SurfaceFlinger的FramebufferNativeWindow；

2)         面向应用程序的SurfaceTextureClient；

![](https://i-blog.csdnimg.cn/blog_migrate/766fee63fdf5082fed267a0166f2cd77.png)

使用OpenGL绘制图像前，需要通过EGL创建一个Surface画板：

frameworks\\native\\opengl\\libagl\\ Egl.cpp

```
EGLSurface eglCreateWindowSurface(  EGLDisplay dpy, EGLConfig config,
                                    NativeWindowType window,
                                    const EGLint *attrib_list)
{
    return createWindowSurface(dpy, config, window, attrib_list);
}
```

OpenGL是跨平台的图形库，不同平台上的NativeWindowType窗口类型有不同的定义：

frameworks\\native\\opengl\\include\\egl\\ Eglplatform.h

```
typedef EGLNativeWindowType  NativeWindowType;

#if defined(_WIN32) || defined(__VC32__) && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__) 
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>

typedef HDC     EGLNativeDisplayType;
typedef HBITMAP EGLNativePixmapType;
typedef HWND    EGLNativeWindowType;

#elif defined(__WINSCW__) || defined(__SYMBIAN32__)  

typedef int   EGLNativeDisplayType;
typedef void *EGLNativeWindowType;
typedef void *EGLNativePixmapType;

#elif defined(__ANDROID__) || defined(ANDROID)

struct ANativeWindow;
struct egl_native_pixmap_t;

typedef struct ANativeWindow*           EGLNativeWindowType;
typedef struct egl_native_pixmap_t*     EGLNativePixmapType;
typedef void*                           EGLNativeDisplayType;

#elif defined(__unix__)


#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef Display *EGLNativeDisplayType;
typedef Pixmap   EGLNativePixmapType;
typedef Window   EGLNativeWindowType;

#else
#error "Platform not recognized"
#endif
```

可以看出对于Android平台，NativeWindowType的类型为ANativeWindow

```
struct ANativeWindow
{
#ifdef __cplusplus 
    ANativeWindow(): flags(0), minSwapInterval(0), maxSwapInterval(0), xdpi(0), ydpi(0)
    {
        common.magic = ANDROID_NATIVE_WINDOW_MAGIC;
        common.version = sizeof(ANativeWindow);
        memset(common.reserved, 0, sizeof(common.reserved));
    }
    
    void incStrong(const void* id) const {
        common.incRef(const_cast<android_native_base_t*>(&common));
    }
    void decStrong(const void* id) const {
        common.decRef(const_cast<android_native_base_t*>(&common));
    }
#endif
    struct android_native_base_t common;
    
    const uint32_t flags;
    
    const int   minSwapInterval;
    
    const int   maxSwapInterval;
    
    const float xdpi;
    const float ydpi;
    
    intptr_t    oem[4];
    
    int     (*setSwapInterval)(struct ANativeWindow* window,
                int interval);
    
    int     (*dequeueBuffer)(struct ANativeWindow* window,
                struct ANativeWindowBuffer** buffer);
    
    int     (*lockBuffer)(struct ANativeWindow* window,
                struct ANativeWindowBuffer* buffer);
    
    int     (*queueBuffer)(struct ANativeWindow* window,
                struct ANativeWindowBuffer* buffer);
    
    int     (*query)(const struct ANativeWindow* window,
                int what, int* value);
    
    int     (*perform)(struct ANativeWindow* window,
                int operation, ... );
    
    int     (*cancelBuffer)(struct ANativeWindow* window,
                struct ANativeWindowBuffer* buffer);
    void* reserved_proc[2];
};
```

无论是基于应用程序的本地窗口SurfaceTextureClient还是基于SurfaceFlinger的FramebufferNativeWindow本地窗口，都必须实现ANativeWindow定义的一套窗口协议，这样应用程序和SurfaceFlinger才能使用OpenGL。ANativeWindow就相当于Java中的接口类型，只是用于定义窗口的功能函数，并不去实现这些函数，而是由需要使用OpenGL的对象来实现。

#### 应用程序本地窗口

应用程序端的SurfaceTextureClient实现了ANativeWindow类型中定义的函数，这样应用程序就可以使用OpenGL来绘制图形图像了，当然应用程序可以选择性地使用OpenGL或是Skia。

![](https://i-blog.csdnimg.cn/blog_migrate/341d5f7cd16b0aa5ce91cb31722c4600.jpeg)

#### SurfaceFlinger本地窗口

由于SurfaceFlinger必须要使用OpenGL来混合图像，因此SurfaceFlinger端的FramebufferNativeWindow必须实现ANativeWindow类型。

![](https://i-blog.csdnimg.cn/blog_migrate/5e531d8a660b573af0c510510e7fb1b4.jpeg)

通过加载gralloc抽象层，可以打开fb设备和gpu设备，fb设备用于操作framebuffer，gpu设备负责图形缓冲区的分配和释放。对于SurfaceFlinger服务端的本地窗口FramebufferNativeWindow将分别打开fb设备和gpu设备，FramebufferNativeWindow通过gpu设备从Framebuffer中分配图形缓冲区，通过fb设备来渲染经SurfaceFlinger混合后的图像。而对于应用程序端的SurfaceTextureClient本地窗口，其需要的图形缓冲区也是由SurfaceFlinger服务端来分配，应用程序进程只需要将服务端分配的图形缓冲区映射到应用程序的虚拟地址空间，图形缓冲区的映射过程也是由Gralloc模块完成。[Android图形显示之硬件抽象层Gralloc](https://link.csdn.net/?target=http%3A%2F%2Fblog.csdn.net%2Fyangwen123%2Farticle%2Fdetails%2F12192401%3Flogin%3Dfrom_csdn "Android图形显示之硬件抽象层Gralloc")对Gralloc模块进行了详细介绍，这里只简单带过。

![](https://i-blog.csdnimg.cn/blog_migrate/4eff273da743892a9d7c30c448e466c5.png)

gpu、fb和gralloc模块中定义的数据结构关系如下：

![](https://i-blog.csdnimg.cn/blog_migrate/568a0b9b67e8568dc469201f6a95bb37.png)

通过加载Gralloc动态库，可以分别打开fb设备和gpu设备。

![](https://i-blog.csdnimg.cn/blog_migrate/69c3111fffeaa8fdd5ddb5c3e024eee8.png)

#### gpu设备

Gpu打开过程就是创建并初始化一个gralloc\_context\_t对象，gpu负责图形buffer的分配和释放。

![](https://i-blog.csdnimg.cn/blog_migrate/683c587ba16f711c5b423457c5704dd9.png)

```
int gralloc_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, GRALLOC_HARDWARE_GPU0)) {
        gralloc_context_t *dev;
        dev = (gralloc_context_t*)malloc(sizeof(*dev));

        
        memset(dev, 0, sizeof(*dev));

        
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = gralloc_close;

        dev->device.alloc   = gralloc_alloc;
        dev->device.free    = gralloc_free;

        *device = &dev->device.common;
        status = 0;
    return status;
}

```

#### fb设备

Fb设备打开过程就是创建并初始化一个fb\_context\_t对象，关于屏幕大小、格式、刷新频率等信息都是通过Framebuffer驱动来获取，最后将Framebuffer映射到SurfaceFlinger进程地址空间，并将映射后的首地址保持到private\_module\_t的framebuffer->base变量中。

![](https://i-blog.csdnimg.cn/blog_migrate/99eb6892cda2acc5ecd98d9ea69f1c70.png)

```
int fb_device_open(hw_module_t const* module, const char* name,
        hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, GRALLOC_HARDWARE_FB0)) {
        alloc_device_t* gralloc_device;
        status = gralloc_open(module, &gralloc_device);
        if (status < 0)
            return status;

        
        fb_context_t *dev = (fb_context_t*)malloc(sizeof(*dev));
        memset(dev, 0, sizeof(*dev));

        
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = fb_close;
        dev->device.setSwapInterval = fb_setSwapInterval;
        dev->device.post            = fb_post;
        dev->device.setUpdateRect = 0;

        private_module_t* m = (private_module_t*)module;
        status = mapFrameBuffer(m);
        if (status >= 0) {
            int stride = m->finfo.line_length / (m->info.bits_per_pixel >> 3);
            int format = (m->info.bits_per_pixel == 32)
                         ? HAL_PIXEL_FORMAT_RGBX_8888
                         : HAL_PIXEL_FORMAT_RGB_565;
            const_cast<uint32_t&>(dev->device.flags) = 0;
            const_cast<uint32_t&>(dev->device.width) = m->info.xres;
            const_cast<uint32_t&>(dev->device.height) = m->info.yres;
            const_cast<int&>(dev->device.stride) = stride;
            const_cast<int&>(dev->device.format) = format;
            const_cast<float&>(dev->device.xdpi) = m->xdpi;
            const_cast<float&>(dev->device.ydpi) = m->ydpi;
            const_cast<float&>(dev->device.fps) = m->fps;
            const_cast<int&>(dev->device.minSwapInterval) = 1;
            const_cast<int&>(dev->device.maxSwapInterval) = 1;
            *device = &dev->device.common;
        }
    }
    return status;
}

```

为了方便应用程序及SurfaceFlinger使用Gralloc模块中的fb设备和gpu设备，Android对gpu设备进行了封装。我们知道，SurfaceFlinger不仅负责FramebufferNativeWindow本地窗口的图形buffer的分配，同时也负责应用程序本地窗口SurfaceTextureClient的图形buffer分配，应用程序只需将服务端分配的图形buffer映射到当前进程的虚拟地址空间即可，因此应用程序和SurfaceFlinger都将会使用到Gralloc模块。

![](https://i-blog.csdnimg.cn/blog_migrate/418a6f15fd624983e2dc13496028ab51.png)

SurfaceSession对象承担了应用程序与SurfaceFlinger之间的通信过程，每一个需要与SurfaceFlinger进程交互的应用程序端都需要创建一个SurfaceSession对象。

#### 客户端请求

frameworks\\base\\core\\java\\android\\view\\SurfaceSession.java

```
public SurfaceSession() {
init();
}

```

Java层的SurfaceSession对象构造过程会通过JNI在native层创建一个SurfaceComposerClient对象。

frameworks\\base\\core\\jni\\android\_view\_Surface.cpp

```
static void SurfaceSession_init(JNIEnv* env, jobject clazz)
{
    sp<SurfaceComposerClient> client = new SurfaceComposerClient;
    client->incStrong(clazz);
    env->SetIntField(clazz, sso.client, (int)client.get());
}

```

Java层的SurfaceSession对象与C++层的SurfaceComposerClient对象之间是一对一关系。

frameworks\\native\\libs\\gui\\SurfaceComposerClient.cpp

```
SurfaceComposerClient::SurfaceComposerClient()
    : mStatus(NO_INIT), mComposer(Composer::getInstance())
{
}

```

SurfaceComposerClient继承于RefBase类，当第一次被强引用时，onFirstRef函数被回调，在该函数中SurfaceComposerClient会请求SurfaceFlinger为当前应用程序创建一个Client对象，专门接收该应用程序的请求，在SurfaceFlinger端创建好Client本地Binder对象后，将该Binder代理对象返回给应用程序端，并保存在SurfaceComposerClient的成员变量mClient中。

```
void SurfaceComposerClient::onFirstRef() {

    sp<ISurfaceComposer> sm(getComposerService());
    if (sm != 0) {
        sp<ISurfaceComposerClient> conn = sm->createConnection();
        if (conn != 0) {
            mClient = conn;
            mStatus = NO_ERROR;
        }
    }
}

```

#### 服务端处理

在SurfaceFlinger服务端为应用程序创建交互的Client对象

frameworks\\native\\services\\surfaceflinger\\SurfaceFlinger.cpp

```
sp<ISurfaceComposerClient> SurfaceFlinger::createConnection()
{
    sp<ISurfaceComposerClient> bclient;
    sp<Client> client(new Client(this));
    status_t err = client->initCheck();
    if (err == NO_ERROR) {
        bclient = client;
    }
    return bclient;
}

```

#### 客户端请求

frameworks\\base\\core\\java\\android\\view\\Surface.java

```
public Surface(SurfaceSession s,int pid, String name, int display, int w, int h, int format, int flags)
throws OutOfResourcesException {
checkHeadless();
if (DEBUG_RELEASE) {
mCreationStack = new Exception();
}
mCanvas = new CompatibleCanvas();
init(s,pid,name,display,w,h,format,flags);
mName = name;
}


```

frameworks\\base\\core\\jni\\android\_view\_Surface.cpp

```
static void Surface_init(JNIEnv* env, jobject clazz,jobject session,jint, jstring jname, jint dpy, 
jint w, jint h, jint format, jint flags)
{
    if (session == NULL) {
        doThrowNPE(env);
        return;
}

    SurfaceComposerClient* client =
            (SurfaceComposerClient*)env->GetIntField(session, sso.client);

    
    sp<SurfaceControl> surface;
    if (jname == NULL) {
        surface = client->createSurface(dpy, w, h, format, flags);
    } else {
        const jchar* str = env->GetStringCritical(jname, 0);
        const String8 name(str, env->GetStringLength(jname));
        env->ReleaseStringCritical(jname, str);
        surface = client->createSurface(name, dpy, w, h, format, flags);
    }
    if (surface == 0) {
        jniThrowException(env, OutOfResourcesException, NULL);
        return;
}

    setSurfaceControl(env, clazz, surface);
}

```

该函数首先得到前面创建好的SurfaceComposerClient对象，通过该对象向SurfaceFlinger端的Client对象发送创建Surface的请求，最后得到一个SurfaceControl对象。

frameworks\\native\\libs\\gui\\SurfaceComposerClient.cpp

```
sp<SurfaceControl> SurfaceComposerClient::createSurface(
        const String8& name,
        DisplayID display,
        uint32_t w,
        uint32_t h,
        PixelFormat format,
        uint32_t flags)
{
    sp<SurfaceControl> result;
if (mStatus == NO_ERROR) {
    
        ISurfaceComposerClient::surface_data_t data;
        sp<ISurface> surface = mClient->createSurface(&data, name,
                display, w, h, format, flags);
        
        if (surface != 0) {
            result = new SurfaceControl(this, surface, data);
        }
    }
    return result;
}

```

SurfaceComposerClient将Surface创建请求转交给保存在其成员变量中的Bp SurfaceComposerClient对象来完成，在SurfaceFlinger端的Client本地对象会返回一个ISurface代理对象给应用程序，通过该代理对象为应用程序当前创建的Surface创建一个SurfaceControl对象。

frameworks\\native\\include\\gui\\ISurfaceComposerClient.h

```
virtual sp<ISurface> createSurface( surface_data_t* params,
                                        const String8& name,
                                        DisplayID display,
                                        uint32_t w,
                                        uint32_t h,
                                        PixelFormat format,
                                        uint32_t flags)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISurfaceComposerClient::getInterfaceDescriptor());
        data.writeString8(name);
        data.writeInt32(display);
        data.writeInt32(w);
        data.writeInt32(h);
        data.writeInt32(format);
        data.writeInt32(flags);
        remote()->transact(CREATE_SURFACE, data, &reply);
        params->readFromParcel(reply);
        return interface_cast<ISurface>(reply.readStrongBinder());
    }


```

#### 服务端处理

frameworks\\native\\include\\gui\\ISurfaceComposerClient.h

```
status_t BnSurfaceComposerClient::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
     switch(code) {
        case CREATE_SURFACE: {
            CHECK_INTERFACE(ISurfaceComposerClient, data, reply);
            surface_data_t params;
            String8 name = data.readString8();
            DisplayID display = data.readInt32();
            uint32_t w = data.readInt32();
            uint32_t h = data.readInt32();
            PixelFormat format = data.readInt32();
            uint32_t flags = data.readInt32();
            //Client继承于BnSurfaceComposerClient，调用Client的createSurface函数处理
            sp<ISurface> s = createSurface(¶ms, name, display, w, h,
                    format, flags);
            params.writeToParcel(reply);
            reply->writeStrongBinder(s->asBinder());
            return NO_ERROR;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

```

frameworks\\native\\services\\surfaceflinger\\SurfaceFlinger.cpp$ Client

```
sp<ISurface> Client::createSurface(
        ISurfaceComposerClient::surface_data_t* params,
        const String8& name,
        DisplayID display, uint32_t w, uint32_t h, PixelFormat format,
        uint32_t flags)
{
    sp<MessageBase> msg = new MessageCreateSurface(mFlinger.get(),
            params, name, this, display, w, h, format, flags);
    
    mFlinger->postMessageSync(msg);
    return static_cast<MessageCreateSurface*>( msg.get() )->getResult();
}

```

MessageCreateSurface消息是专门为应用程序请求创建Surface而定义的一种消息类型：

```
class MessageCreateSurface : public MessageBase {
sp<ISurface> result;
SurfaceFlinger* flinger;
ISurfaceComposerClient::surface_data_t* params;
Client* client;
const String8& name;
DisplayID display;
uint32_t w, h;
PixelFormat format;
uint32_t flags;
public:
MessageCreateSurface(SurfaceFlinger* flinger,
ISurfaceComposerClient::surface_data_t* params,
const String8& name, Client* client,
DisplayID display, uint32_t w, uint32_t h, PixelFormat format,uint32_t flags)
: flinger(flinger), params(params), client(client), name(name),display(display),
w(w), h(h), format(format), flags(flags)
{
}
sp<ISurface> getResult() const { return result; }

virtual bool handler() {
result = flinger->createSurface(params, name, client,
display, w, h, format, flags);
return true;
}
};
```

Client将应用程序创建Surface的请求转换为异步消息投递到SurfaceFlinger的消息队列中，将创建Surface的任务转交给SurfaceFlinger。

frameworks\\native\\services\\surfaceflinger\\SurfaceFlinger.cpp

```
sp<ISurface> SurfaceFlinger::createSurface(
        ISurfaceComposerClient::surface_data_t* params,
        const String8& name,
        const sp<Client>& client,
        DisplayID d, uint32_t w, uint32_t h, PixelFormat format,
        uint32_t flags)
{
    sp<LayerBaseClient> layer;
    sp<ISurface> surfaceHandle;
    if (int32_t(w|h) < 0) {
        ALOGE("createSurface() failed, w or h is negative (w=%d, h=%d)",int(w), int(h));
        return surfaceHandle;
    }
    
    sp<Layer> normalLayer;

    switch (flags & eFXSurfaceMask) {
        case eFXSurfaceNormal:
            normalLayer = createNormalSurface(client, d, w, h, flags, format);
            layer = normalLayer;
            break;
        case eFXSurfaceBlur:
            
            
        case eFXSurfaceDim:
            layer = createDimSurface(client, d, w, h, flags);
            break;
        case eFXSurfaceScreenshot:
            layer = createScreenshotSurface(client, d, w, h, flags);
            break;
    }

    if (layer != 0) {
        layer->initStates(w, h, flags);
        layer->setName(name);

        ssize_t token = addClientLayer(client, layer);

        surfaceHandle = layer->getSurface();
        if (surfaceHandle != 0) {

            params->token = token;

            params->identity = layer->getIdentity();

            if (normalLayer != 0) {
                Mutex::Autolock _l(mStateLock);
                mLayerMap.add(layer->getSurfaceBinder(), normalLayer);
            }
        }
        setTransactionFlags(eTransactionNeeded);
    }
    return surfaceHandle;
}

```

SurfaceFlinger根据标志位创建对应类型的Surface，当前系统定义了5种类型的Surface：

![](https://i-blog.csdnimg.cn/blog_migrate/82ff79a97b5f81b24dd250b261cac7a3.png)  
普遍Surface的创建过程：

```
sp<Layer> SurfaceFlinger::createNormalSurface(
        const sp<Client>& client, DisplayID display,
        uint32_t w, uint32_t h, uint32_t flags,
        PixelFormat& format)
{
    
    switch (format) { 
    case PIXEL_FORMAT_TRANSPARENT:
    case PIXEL_FORMAT_TRANSLUCENT:
        format = PIXEL_FORMAT_RGBA_8888;
        break;
    case PIXEL_FORMAT_OPAQUE:
#ifdef NO_RGBX_8888
        format = PIXEL_FORMAT_RGB_565;
#else
        format = PIXEL_FORMAT_RGBX_8888;
#endif
        break;
    }
#ifdef NO_RGBX_8888
    if (format == PIXEL_FORMAT_RGBX_8888)
        format = PIXEL_FORMAT_RGBA_8888;
#endif

    sp<Layer> layer = new Layer(this, display, client);
    status_t err = layer->setBuffers(w, h, format, flags);
    if (CC_LIKELY(err != NO_ERROR)) {
        ALOGE("createNormalSurfaceLocked() failed (%s)", strerror(-err));
        layer.clear();
    }
    return layer;
}

```

在SurfaceFlinger服务端为应用程序创建的Surface创建对应的Layer对象。应用程序请求创建Surface过程如下：

![](https://i-blog.csdnimg.cn/blog_migrate/023a5b31b74de31cfb483115fd3cacb0.png)

Layer构造过程

![](https://i-blog.csdnimg.cn/blog_migrate/15372821772d15ea03572b6cd9f764b4.jpeg)

frameworks\\native\\services\\surfaceflinger\\LayerBase.cpp

```
LayerBase::LayerBase(SurfaceFlinger* flinger, DisplayID display)
    : dpy(display), contentDirty(false),
      sequence(uint32_t(android_atomic_inc(&sSequence))),
      mFlinger(flinger), mFiltering(false),
      mNeedsFiltering(false),
      mOrientation(0),
      mPlaneOrientation(0),
      mTransactionFlags(0),
      mPremultipliedAlpha(true), mName("unnamed"), mDebug(false)
{
    const DisplayHardware& hw(flinger->graphicPlane(0).displayHardware());
    mFlags = hw.getFlags();
}

```

```
LayerBaseClient::LayerBaseClient(SurfaceFlinger* flinger, DisplayID display,
        const sp<Client>& client)
    : LayerBase(flinger, display),
      mHasSurface(false),
      mClientRef(client),
//为每个Layer对象分配一个唯一的标示号
      mIdentity(uint32_t(android_atomic_inc(&sIdentity)))
{
}
```

frameworks\\native\\services\\surfaceflinger\\Layer.cpp

```
Layer::Layer(SurfaceFlinger* flinger,
        DisplayID display, const sp<Client>& client)
    :   LayerBaseClient(flinger, display, client),
        mTextureName(-1U),
        mQueuedFrames(0),
        mCurrentTransform(0),
        mCurrentScalingMode(NATIVE_WINDOW_SCALING_MODE_FREEZE),
        mCurrentOpacity(true),
        mRefreshPending(false),
        mFrameLatencyNeeded(false),
        mFrameLatencyOffset(0),
        mFormat(PIXEL_FORMAT_NONE),
        mGLExtensions(GLExtensions::getInstance()),
        mOpaqueLayer(true),
        mNeedsDithering(false),
        mSecure(false),
        mProtectedByApp(false)
{
    mCurrentCrop.makeInvalid();
    glGenTextures(1, &mTextureName);
}

```

第一次强引用Layer对象时，onFirstRef()函数被回调

```
void Layer::onFirstRef()
{
    LayerBaseClient::onFirstRef();
    
sp<BufferQueue> bq = new SurfaceTextureLayer();
    
    mSurfaceTexture = new SurfaceTexture(mTextureName, true,
            GL_TEXTURE_EXTERNAL_OES, false, bq);
    mSurfaceTexture->setConsumerUsageBits(getEffectiveUsage(0));
    
    mSurfaceTexture->setFrameAvailableListener(new FrameQueuedListener(this));
    
    mSurfaceTexture->setSynchronousMode(true);
    
#ifdef TARGET_DISABLE_TRIPLE_BUFFERING
#warning "disabling triple buffering"
    mSurfaceTexture->setBufferCountServer(2);
#else
    mSurfaceTexture->setBufferCountServer(3);
#endif
}

```

BufferQueue构造过程

![](https://i-blog.csdnimg.cn/blog_migrate/206dfde7d259e52a1ba07996f735f4de.jpeg)

frameworks\\native\\libs\\gui\\ SurfaceTexture.cpp

```
SurfaceTextureLayer::SurfaceTextureLayer()
    : BufferQueue(true) {
}

```

frameworks\\native\\libs\\gui\\BufferQueue.cpp

```
BufferQueue::BufferQueue(  bool allowSynchronousMode, int bufferCount ) :
    mDefaultWidth(1),
    mDefaultHeight(1),
    mPixelFormat(PIXEL_FORMAT_RGBA_8888),
    mMinUndequeuedBuffers(bufferCount),
    mMinAsyncBufferSlots(bufferCount + 1),
    mMinSyncBufferSlots(bufferCount),
    mBufferCount(mMinAsyncBufferSlots),
    mClientBufferCount(0),
    mServerBufferCount(mMinAsyncBufferSlots),
    mSynchronousMode(false),
    mAllowSynchronousMode(allowSynchronousMode),
    mConnectedApi(NO_CONNECTED_API),
    mAbandoned(false),
    mFrameCounter(0),
    mBufferHasBeenQueued(false),
    mDefaultBufferFormat(0),
    mConsumerUsageBits(0),
    mTransformHint(0)
{
    
    mConsumerName = String8::format("unnamed-%d-%d", getpid(), createProcessUniqueId());
    ST_LOGV("BufferQueue");
    
    sp<ISurfaceComposer> composer(ComposerService::getComposerService());
    
    mGraphicBufferAlloc = composer->createGraphicBufferAlloc();
    if (mGraphicBufferAlloc == 0) {
        ST_LOGE("createGraphicBufferAlloc() failed in BufferQueue()");
    }
}

```

GraphicBufferAlloc构造过程

![](https://i-blog.csdnimg.cn/blog_migrate/9cb2b594257c4e76dd701d7ff996ae85.jpeg)

frameworks\\native\\services\\surfaceflinger\\SurfaceFlinger.cpp

```
sp<IGraphicBufferAlloc> SurfaceFlinger::createGraphicBufferAlloc()
{
    sp<GraphicBufferAlloc> gba(new GraphicBufferAlloc());
    return gba;
}
```

SurfaceTexture构造过程

frameworks\\native\\libs\\gui\\ SurfaceTexture.cpp

```
SurfaceTexture::SurfaceTexture(GLuint tex, bool allowSynchronousMode,
        GLenum texTarget, bool useFenceSync, const sp<BufferQueue> &bufferQueue) :
    mCurrentTransform(0),
    mCurrentTimestamp(0),
    mFilteringEnabled(true),
    mTexName(tex),
#ifdef USE_FENCE_SYNC
    mUseFenceSync(useFenceSync),
#else
    mUseFenceSync(false),
#endif
    mTexTarget(texTarget),
    mEglDisplay(EGL_NO_DISPLAY),
    mEglContext(EGL_NO_CONTEXT),
    mAbandoned(false),
    mCurrentTexture(BufferQueue::INVALID_BUFFER_SLOT),
    mAttached(true)
{
    
    mName = String8::format("unnamed-%d-%d", getpid(), createProcessUniqueId());
    ST_LOGV("SurfaceTexture");
    if (bufferQueue == 0) {
        ST_LOGV("Creating a new BufferQueue");
        mBufferQueue = new BufferQueue(allowSynchronousMode);
    }
    else {
        mBufferQueue = bufferQueue;
    }
    memcpy(mCurrentTransformMatrix, mtxIdentity,
            sizeof(mCurrentTransformMatrix));
    
    
    
    
    wp<BufferQueue::ConsumerListener> listener;
    sp<BufferQueue::ConsumerListener> proxy;


    listener = static_cast<BufferQueue::ConsumerListener*>(this);
    proxy = new BufferQueue::ProxyConsumerListener(listener);


    status_t err = mBufferQueue->consumerConnect(proxy);
    if (err != NO_ERROR) {
        ST_LOGE("SurfaceTexture: error connecting to BufferQueue: %s (%d)",
                strerror(-err), err);
    } else {
        mBufferQueue->setConsumerName(mName);
        mBufferQueue->setConsumerUsageBits(DEFAULT_USAGE_FLAGS);
    }
}

```

根据buffer可用监听器的注册过程，我们知道，当生产者也就是应用程序填充好图形buffer数据后，通过回调方式通知消费者的过程如下：

![](https://i-blog.csdnimg.cn/blog_migrate/ac90e241b53af90cd617e372d93d185d.png)  
在服务端为Surface创建Layer过程中，分别创建了SurfaceTexture、BufferQueue和GraphicBufferAlloc对象，它们之间的关系如下：

![](https://i-blog.csdnimg.cn/blog_migrate/3252d041fdfabef920e629fa1ddc18a6.png)

ISurface本地对象创建过程

以上完成Layer对象创建后，通过layer->getSurface()来创建ISurface的Binder本地对象，并将其代理对象返回给应用程序。

frameworks\\native\\services\\surfaceflinge\\ Layer.cpp

```
sp<ISurface> Layer::createSurface()
{
    sp<ISurface> sur(new BSurface(mFlinger, this));
    return sur;
}

```

总结一下Surface创建过程，应用程序通过SurfaceComposerClient请求SurfaceFlinger创建一个Surface，在SurfaceFlinger服务端将会创建的对象有：

1.         一个Layer对象：

2.         一个SurfaceTexture对象

3.         一个BufferQueue对象：用于管理当前创建的Surface的图形buffer

4.         一个GraphicBufferAlloc对象：用于分配图形buffer

5.         一个BSurface本地Binder对象：用于获取BufferQueue的Binder代理对象

关于Surface创建过程的详细分析请参考[Android应用程序创建Surface过程源码分析](https://link.csdn.net/?target=http%3A%2F%2Fblog.csdn.net%2Fyangwen123%2Farticle%2Fdetails%2F12521337%3Flogin%3Dfrom_csdn "Android应用程序创建Surface过程源码分析")。Android在SurfaceFlinger进程为应用程序定义了4个Binder对象：

1.         SurfaceFlinger： 有名Binder对象，可通过服务查询方式获取；

2.         Client：                 无名Binder对象，只能由SurfaceFlinger服务创建；

3.         BSurface：           无名Binder对象，只能由Client服务创建；

4.         BufferQueue：      无名Binder对象，只能通过BSurface服务返回；

![](https://i-blog.csdnimg.cn/blog_migrate/e403ebc9877585b065da9a112114a3a4.png)

以上各个Binder对象提供的接口函数如下所示：

![](https://i-blog.csdnimg.cn/blog_migrate/23e1cb08ccfa496a9e49271c7ac525f9.png)

linux-dash

A beautiful web dashboard for Linux

项目地址：https://gitcode.com/gh\_mirrors/li/linux-dash

从前面分析可知，SurfaceFlinger在处理应用程序请求创建Surface中，在SurfaceFlinger服务端仅仅创建了Layer对象，那么应用程序本地窗口Surface在什么时候、什么地方创建呢？

我们知道Surface继承于SurfaceTextureClient，而SurfaceTextureClient是面向应用程序的本地创建，因此它就应该是在应用程序进程中创建。在前面的分析中，我们也知道，SurfaceFlinger

为应用程序创建好了Layer对象并返回ISurface的代理对象给应用程序，应用程序通过该代理对象创建了一个SurfaceControl对象，Java层Surface需要通过android\_view\_Surface.cpp中的JNI函数来操作native层的Surface，在操作native层Surface前，首先需要获取到native的Surface，应用程序本地窗口Surface就是在这个时候创建的。

frameworks\\base\\core\\jni\\android\_view\_Surface.cpp

```
static sp<Surface> getSurface(JNIEnv* env, jobject clazz)
{
    
    sp<Surface> result(Surface_getSurface(env, clazz));

    if (result == 0) {
        
        SurfaceControl* const control =
            (SurfaceControl*)env->GetIntField(clazz, so.surfaceControl);
        if (control) {
    
            result = control->getSurface();
            if (result != 0) {
                result->incStrong(clazz);
                env->SetIntField(clazz, so.surface, (int)result.get());
            }
        }
    }
    return result;
}
```

frameworks\\native\\libs\\gui\\Surface.cpp

```
sp<Surface> SurfaceControl::getSurface() const
{
    Mutex::Autolock _l(mLock);
    if (mSurfaceData == 0) {
        sp<SurfaceControl> surface_control(const_cast<SurfaceControl*>(this));

        mSurfaceData = new Surface(surface_control);
    }
    return mSurfaceData;
}
```

```
Surface::Surface(const sp<SurfaceControl>& surface)
    : SurfaceTextureClient(),
      mSurface(surface->mSurface),
      mIdentity(surface->mIdentity)
{
    sp<ISurfaceTexture> st;
    if (mSurface != NULL) {
        st = mSurface->getSurfaceTexture();
    }
    init(st);
}
```

Surface继承于SurfaceTextureClient类，在构造Surface时，首先会调用SurfaceTextureClient的构造函数：

frameworks\\native\\libs\\gui\\SurfaceTextureClient.cpp

```
SurfaceTextureClient::SurfaceTextureClient() {
    SurfaceTextureClient::init();
}
```

```
void SurfaceTextureClient::init() {
    // Initialize the ANativeWindow function pointers.
    ANativeWindow::setSwapInterval  = hook_setSwapInterval;
    ANativeWindow::dequeueBuffer    = hook_dequeueBuffer;
    ANativeWindow::cancelBuffer     = hook_cancelBuffer;
    ANativeWindow::lockBuffer       = hook_lockBuffer;
    ANativeWindow::queueBuffer      = hook_queueBuffer;
    ANativeWindow::query            = hook_query;
    ANativeWindow::perform          = hook_perform;
    const_cast<int&>(ANativeWindow::minSwapInterval) = 0;
    const_cast<int&>(ANativeWindow::maxSwapInterval) = 1;
    mReqWidth = 0;
    mReqHeight = 0;
    mReqFormat = 0;
    mReqUsage = 0;
    mTimestamp = NATIVE_WINDOW_TIMESTAMP_AUTO;
    mCrop.clear();
    mScalingMode = NATIVE_WINDOW_SCALING_MODE_FREEZE;
    mTransform = 0;
    mDefaultWidth = 0;
    mDefaultHeight = 0;
    mUserWidth = 0;
    mUserHeight = 0;
    mTransformHint = 0;
    mConsumerRunningBehind = false;
    mConnectedToCpu = false;
}
```

父类SurfaceTextureClient构造完成后，通过ISurface的代理对象BpSurface请求BSurface获取BufferQueue的代理对象。

frameworks\\native\\services\\surfaceflinge\\ Layer.cpp

```
class BSurface : public BnSurface, public LayerCleaner {
wp<const Layer> mOwner;
virtual sp<ISurfaceTexture> getSurfaceTexture() const {
sp<ISurfaceTexture> res;
sp<const Layer> that( mOwner.promote() );
if (that != NULL) {
res = that->mSurfaceTexture->getBufferQueue();
}
return res;
}
public:
BSurface(const sp<SurfaceFlinger>& flinger,const sp<Layer>& layer)
: LayerCleaner(flinger, layer), mOwner(layer) { }
};
```

最后调用Surface的init函数进行初始化

frameworks\\native\\libs\\gui\\Surface.cpp

```
void Surface::init(const sp<ISurfaceTexture>& surfaceTexture)
{
    if (mSurface != NULL || surfaceTexture != NULL) {
        ALOGE_IF(surfaceTexture==0, "got a NULL ISurfaceTexture from ISurface");
        if (surfaceTexture != NULL) {
            setISurfaceTexture(surfaceTexture);
            setUsage(GraphicBuffer::USAGE_HW_RENDER);
        }

        DisplayInfo dinfo;
        SurfaceComposerClient::getDisplayInfo(0, &dinfo);
        const_cast<float&>(ANativeWindow::xdpi) = dinfo.xdpi;
        const_cast<float&>(ANativeWindow::ydpi) = dinfo.ydpi;
        const_cast<uint32_t&>(ANativeWindow::flags) = 0;
    }
}
```

到此应用程序的本地窗口Surface就创建完成了，通过上面的分析，可以知道，应用程序本地窗口的创建会在应用程序进程和SurfaceFlinger进程分别创建不同的对象：

1\. SurfaceFlinger进程：Layer、SurfaceTexture、BufferQueue等；

2\. 应用程序进程：Surface、SurfaceControl、SurfaceComposerClient等；

![](https://img-blog.csdn.net/20140331163355359?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQveWFuZ3dlbjEyMw==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)

ISurfaceTexture是应用程序与BufferQueue的传输通道。  
ISurfaceComposerClient是应用程序与SurfaceFlinger间的桥梁,在应用进程中则被封装在SurfaceComposerClient这个类中。这是一个匿名binder server,由应用程序调用SurfaceFlinger这个实名binder的createConnection方法来获取到,服务端的实现是SurfaceFlinger::Client。任何有UI界面的程序都在SurfaceFlinger中有且仅有一个Client实例。  
ISurface:由应用程序调用ISurfaceComposerClient::createSurface()得到,同时在SurfaceFlinger这一进程中将会有一个Layer被创建,代表了一个“画面”。ISurface就是控制这一画面的handle。  
Surface:从逻辑关系上看,它是上述ISurface的使用者。从继承关系上看,它是一个SurfaceTextureClient,也就是本地窗口。SurfaceTextureClient内部持有ISurfaceTexture,即BufferQueue的实现接口。  
以上Surface、Layer、SurfaceTexture、BufferQueue，应用程序和Client之间的关系如下图所示：

![](https://i-blog.csdnimg.cn/blog_migrate/25187ec1adbf5d7e488bea0bdc0f67f8.png)

在创建完应用程序本地窗口Surface后，想要在该Surface上绘图，首先需要为该Surface分配图形buffer。我们前面介绍了Android应用程序图形缓冲区的分配都是由SurfaceFlinger服务进程来完成，在请求创建Surface时，在服务端创建了一个BufferQueue本地Binder对象，该对象负责管理应用程序一个本地窗口Surface的图形缓冲区。在BufferQueue中定义了图形buffer的四个状态：

```
enum BufferState {




FREE = 0,












DEQUEUED = 1,









QUEUED = 2,


ACQUIRED = 3
};

```

![](https://i-blog.csdnimg.cn/blog_migrate/75df6d2ea680a0d1de2745b29606fb67.png)

BufferQueue对图形buffer的管理采用消费者-生产者模型，所有的buffer都由BufferQueue管理，当生产者也就是应用程序需要绘图时，必须向BufferQueue申请绘图缓冲区，并且将图形buffer设置为DEQUEUED出列状态，此时只有应用程序才能访问这块图形buffer。当应用程序完成绘图后，需要将图形缓冲区归还给BufferQueue管理，并设置当前buffer为QUEUED入列状态，同时通知消费者绘图完成。消费者又将向BufferQueue申请已完成的图形buffer，并将当前申请的图形buffer设置为ACQUIRED状态，此时的图形buffer只能被消费者处理。

![](https://i-blog.csdnimg.cn/blog_migrate/c1c5f5ca9a82852a722e685b0c799a8a.png)

#### 客户端请求

frameworks\\native\\libs\\gui\\SurfaceTextureClient.cpp

```
int SurfaceTextureClient::dequeueBuffer(android_native_buffer_t** buffer) {
    ATRACE_CALL();
    ALOGV("SurfaceTextureClient::dequeueBuffer");
    Mutex::Autolock lock(mMutex);

    int buf = -1;
    int reqW = mReqWidth ? mReqWidth : mUserWidth;
    int reqH = mReqHeight ? mReqHeight : mUserHeight;

    status_t result = mSurfaceTexture->dequeueBuffer(&buf, reqW, reqH,
            mReqFormat, mReqUsage);
    if (result < 0) {
        ALOGV("dequeueBuffer: ISurfaceTexture::dequeueBuffer(%d, %d, %d, %d)"
             "failed: %d", mReqWidth, mReqHeight, mReqFormat, mReqUsage,
             result);
        return result;
    }
    sp<GraphicBuffer>& gbuf(mSlots[buf].buffer);

    if (result & ISurfaceTexture::RELEASE_ALL_BUFFERS) {
        freeAllBuffers();
    }

    if ((result & ISurfaceTexture::BUFFER_NEEDS_REALLOCATION) || gbuf == 0) {

        result = mSurfaceTexture->requestBuffer(buf, &gbuf);
        if (result != NO_ERROR) {
            ALOGE("dequeueBuffer: ISurfaceTexture::requestBuffer failed: %d",result);
            return result;
        }
    }
    *buffer = gbuf.get();
    return OK;
}
```

frameworks\\native\\libs\\gui\\ISurfaceTexture.cpp$ BpSurfaceTexture

```
virtual status_t dequeueBuffer(int *buf, uint32_t w, uint32_t h,
uint32_t format, uint32_t usage) {
Parcel data, reply;
data.writeInterfaceToken(ISurfaceTexture::getInterfaceDescriptor());
data.writeInt32(w);
data.writeInt32(h);
data.writeInt32(format);
data.writeInt32(usage);
status_t result = remote()->transact(DEQUEUE_BUFFER, data, &reply);
if (result != NO_ERROR) {
return result;
}
*buf = reply.readInt32();
result = reply.readInt32();
return result;
}
```

#### 服务端处理

frameworks\\native\\libs\\gui\\ISurfaceTexture.cpp$BnSurfaceTexture

```
status_t BnSurfaceTexture::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case DEQUEUE_BUFFER: {
            CHECK_INTERFACE(ISurfaceTexture, data, reply);
            uint32_t w      = data.readInt32();
            uint32_t h      = data.readInt32();
            uint32_t format = data.readInt32();
            uint32_t usage  = data.readInt32();
            int buf;
            int result = dequeueBuffer(&buf, w, h, format, usage);
            reply->writeInt32(buf);
            reply->writeInt32(result);
            return NO_ERROR;
        } break;
    }
    return BBinder::onTransact(code, data, reply, flags);
}

```

frameworks\\native\\libs\\gui\\BufferQueue.cpp

```
status_t BufferQueue::dequeueBuffer(int *outBuf, uint32_t w, uint32_t h,
        uint32_t format, uint32_t usage) {
    ATRACE_CALL();
    ST_LOGV("dequeueBuffer: w=%d h=%d fmt=%#x usage=%#x", w, h, format, usage);
    if ((w && !h) || (!w && h)) {
        ST_LOGE("dequeueBuffer: invalid size: w=%u, h=%u", w, h);
        return BAD_VALUE;
    }
    status_t returnFlags(OK);
    EGLDisplay dpy = EGL_NO_DISPLAY;
    EGLSyncKHR fence = EGL_NO_SYNC_KHR;

    { 
        Mutex::Autolock lock(mMutex);
        if (format == 0) {
            format = mDefaultBufferFormat;
        }
        
        usage |= mConsumerUsageBits;
        int found = -1;
        int foundSync = -1;
        int dequeuedCount = 0;
        bool tryAgain = true;
        while (tryAgain) {
            if (mAbandoned) {
                ST_LOGE("dequeueBuffer: SurfaceTexture has been abandoned!");
                return NO_INIT;
            }
            const int minBufferCountNeeded = mSynchronousMode ? mMinSyncBufferSlots : mMinAsyncBufferSlots;
            const bool numberOfBuffersNeedsToChange = !mClientBufferCount &&
                    ((mServerBufferCount != mBufferCount) ||(mServerBufferCount < minBufferCountNeeded));

if (!mQueue.isEmpty() && numberOfBuffersNeedsToChange) {
                
                mDequeueCondition.wait(mMutex);
                
                
                continue;
            }

            if (numberOfBuffersNeedsToChange) {
                
                freeAllBuffersLocked();
                mBufferCount = mServerBufferCount;
                if (mBufferCount < minBufferCountNeeded)
                    mBufferCount = minBufferCountNeeded;
                mBufferHasBeenQueued = false;
                returnFlags |= ISurfaceTexture::RELEASE_ALL_BUFFERS;
            }
            
            found = INVALID_BUFFER_SLOT;
            foundSync = INVALID_BUFFER_SLOT;
            dequeuedCount = 0;
            for (int i = 0; i < mBufferCount; i++) {
                const int state = mSlots[i].mBufferState;
                if (state == BufferSlot::DEQUEUED) {
                    dequeuedCount++;
                }
if (state == BufferSlot::FREE) {

bool isOlder = mSlots[i].mFrameNumber < mSlots[found].mFrameNumber;
if (found < 0 || isOlder) {
foundSync = i;
found = i;
}
}
            }
            
            
            if (!mClientBufferCount && dequeuedCount) {
                ST_LOGE("dequeueBuffer: can't dequeue multiple buffers without "
                        "setting the buffer count");
                return -EINVAL;
            }
            
            
            
            if (mBufferHasBeenQueued) {
                
                
                const int avail = mBufferCount - (dequeuedCount+1);
                if (avail < (mMinUndequeuedBuffers-int(mSynchronousMode))) {
                    ST_LOGE("dequeueBuffer: mMinUndequeuedBuffers=%d exceeded ""(dequeued=%d)",
                            mMinUndequeuedBuffers-int(mSynchronousMode),
                            dequeuedCount);
                    return -EBUSY;
                }
            }
            
            tryAgain = found == INVALID_BUFFER_SLOT;
            if (tryAgain) {
                mDequeueCondition.wait(mMutex);
            }
        }
        if (found == INVALID_BUFFER_SLOT) {
            
            ST_LOGE("dequeueBuffer: no available buffer slots");
            return -EBUSY;
        }

        const int buf = found;
        *outBuf = found;
        ATRACE_BUFFER_INDEX(buf);
        const bool useDefaultSize = !w && !h;
        if (useDefaultSize) {
            
            w = mDefaultWidth;
            h = mDefaultHeight;
        }
        const bool updateFormat = (format != 0);
        if (!updateFormat) {
            
            format = mPixelFormat;
        }
        
        
        mSlots[buf].mBufferState = BufferSlot::DEQUEUED;
        const sp<GraphicBuffer>& buffer(mSlots[buf].mGraphicBuffer);

        if ((buffer == NULL) ||
            (uint32_t(buffer->width)  != w) ||
            (uint32_t(buffer->height) != h) ||
            (uint32_t(buffer->format) != format) ||
            ((uint32_t(buffer->usage) & usage) != usage))
        {
            status_t error;

            sp<GraphicBuffer> graphicBuffer(
                    mGraphicBufferAlloc->createGraphicBuffer(w, h, format, usage, &error));
            if (graphicBuffer == 0) {
                ST_LOGE("dequeueBuffer: SurfaceComposer::createGraphicBuffer ""failed");
                return error;
            }
            if (updateFormat) {
                mPixelFormat = format;
            }

            mSlots[buf].mAcquireCalled = false;
            mSlots[buf].mGraphicBuffer = graphicBuffer;
            mSlots[buf].mRequestBufferCalled = false;
            mSlots[buf].mFence = EGL_NO_SYNC_KHR;
            mSlots[buf].mEglDisplay = EGL_NO_DISPLAY;

            returnFlags |= ISurfaceTexture::BUFFER_NEEDS_REALLOCATION;
        }
        dpy = mSlots[buf].mEglDisplay;
        fence = mSlots[buf].mFence;
        mSlots[buf].mFence = EGL_NO_SYNC_KHR;
    }  
    if (fence != EGL_NO_SYNC_KHR) {
        EGLint result = eglClientWaitSyncKHR(dpy, fence, 0, 1000000000);
        
        
        
        if (result == EGL_FALSE) {
            ST_LOGE("dequeueBuffer: error waiting for fence: %#x", eglGetError());
        } else if (result == EGL_TIMEOUT_EXPIRED_KHR) {
            ST_LOGE("dequeueBuffer: timeout waiting for fence");
        }
        eglDestroySyncKHR(dpy, fence);
    }
    ST_LOGV("dequeueBuffer: returning slot=%d buf=%p flags=%#x", *outBuf,
            mSlots[*outBuf].mGraphicBuffer->handle, returnFlags);
    return returnFlags;
}



```

BufferQueue中有一个mSlots数组用于管理其内的各缓冲区，最大容量为32。mSlots在程序一开始就静态分配了32个BufferSlot大小的空间。但BufferSlot的内部变指针mGraphicBuffer所指向的图形buffer空间却是动态分配的。

如果从mSlots数组中找到了一个状态为FREE的图形buffer，但由于该图形buffer不合适，因此需要重新创建一个GraphicBuffer对象。

![](https://i-blog.csdnimg.cn/blog_migrate/125a0173f1f4f182f512496472b4b99c.jpeg)

frameworks\\native\\services\\surfaceflinger\\SurfaceFlinger.cpp

```
sp<GraphicBuffer> GraphicBufferAlloc::createGraphicBuffer(uint32_t w, uint32_t h,
        PixelFormat format, uint32_t usage, status_t* error) {

    sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(w, h, format, usage));
    status_t err = graphicBuffer->initCheck();
    *error = err;
    if (err != 0 || graphicBuffer->handle == 0) {
        if (err == NO_MEMORY) {
            GraphicBuffer::dumpAllocationsToSystemLog();
        }
        ALOGE("GraphicBufferAlloc::createGraphicBuffer(w=%d, h=%d) "
             "failed (%s), handle=%p",
                w, h, strerror(-err), graphicBuffer->handle);
        return 0;
    }
    return graphicBuffer;
}
```

frameworks\\native\\libs\\ui\\GraphicBuffer.cpp

```
GraphicBuffer::GraphicBuffer(uint32_t w, uint32_t h, 
        PixelFormat reqFormat, uint32_t reqUsage)
    : BASE(), mOwner(ownData), mBufferMapper(GraphicBufferMapper::get()),
      mInitCheck(NO_ERROR), mIndex(-1)
{
    width  = 
    height = 
    stride = 
    format = 
    usage  = 0;
    handle = NULL;

    mInitCheck = initSize(w, h, reqFormat, reqUsage);
}
```

根据图形buffer的宽高、格式等信息为图形缓冲区分配存储空间

```
status_t GraphicBuffer::initSize(uint32_t w, uint32_t h, PixelFormat format,
        uint32_t reqUsage)
{
    GraphicBufferAllocator& allocator = GraphicBufferAllocator::get();
    status_t err = allocator.alloc(w, h, format, reqUsage, &handle, &stride);
    if (err == NO_ERROR) {
        this->width  = w;
        this->height = h;
        this->format = format;
        this->usage  = reqUsage;
    }
    return err;
}
```

使用GraphicBufferAllocator对象来为图形缓冲区分配内存空间，GraphicBufferAllocator是对Gralloc模块中的gpu设备的封装类。关于GraphicBufferAllocator内存分配过程请查看[Android图形缓冲区分配过程源码分析](https://link.csdn.net/?target=http%3A%2F%2Fblog.csdn.net%2Fyangwen123%2Farticle%2Fdetails%2F12231687%3Flogin%3Dfrom_csdn "Android图形缓冲区分配过程源码分析")，图形缓冲区分配完成后，还会映射到SurfaceFlinger服务进程的虚拟地址空间。

我们知道，Surface为应用程序这边用于描述画板类，继承于SurfaceTextureClient类，是面向应用程序的本地窗口，实现了EGL窗口协议。在SurfaceTextureClient中定义了一个大小为32的mSlots数组，用于保存当前Surface申请的图形buffer。每个Surface在SurfaceFlinger服务端有一个layer对象与之对应，在前面也介绍了，每个Layer又拥有一个SurfaceTexture对象，每个SurfaceTexture对象又持有一个buffer队列BufferQue，BufferQue同样为每个Layer定义了一个大小为32的mSlots数组，同样用来保存每个Layer所使用的buffer。只不过应用程序端的mSlots数组元素和服务端的mSlots数组元素定义不同，应用程序在申请图形buffer时必须保存这两个队列数据中的buffer同步。

![](https://i-blog.csdnimg.cn/blog_migrate/10c32a05ac9b43f17874a9da4e0b3750.png)

如果重新为图形buffer分配空间，那么BufferQueue的dequeueBuffer函数返回值中需要加上BUFFER\_NEEDS\_REALLOCATION标志。客户端在发现这个标志后，它还应调用requestBuffer()来取得最新的buffer地址。

#### 客户端请求

frameworks\\native\\libs\\gui\\SurfaceTextureClient.cpp

```
int SurfaceTextureClient::dequeueBuffer(android_native_buffer_t** buffer) {
...
    if ((result & ISurfaceTexture::BUFFER_NEEDS_REALLOCATION) || gbuf == 0) {

        result = mSurfaceTexture->requestBuffer(buf, &gbuf);
        if (result != NO_ERROR) {
            ALOGE("dequeueBuffer: ISurfaceTexture::requestBuffer failed: %d",
                    result);
            return result;
        }
    }
    *buffer = gbuf.get();
    return OK;
}
```

frameworks\\native\\libs\\gui\\ISurfaceTexture.cpp$ BpSurfaceTexture

```
virtual status_t requestBuffer(int bufferIdx, sp<GraphicBuffer>* buf) {
Parcel data, reply;
data.writeInterfaceToken(ISurfaceTexture::getInterfaceDescriptor());
data.writeInt32(bufferIdx);
status_t result =remote()->transact(REQUEST_BUFFER, data, &reply);
if (result != NO_ERROR) {
return result;
}
bool nonNull = reply.readInt32();
if (nonNull) {
//在应用程序进程中创建一个GraphicBuffer对象
sp<GraphicBuffer> p = new GraphicBuffer();
result = reply.read(*p);
if (result != NO_ERROR) {
p = 0;
return result;
}
*buf = p;
}
result = reply.readInt32();
return result;
}
```

frameworks\\native\\libs\\ui\\GraphicBuffer.cpp

```
GraphicBuffer::GraphicBuffer()
    : BASE(), mOwner(ownData), mBufferMapper(GraphicBufferMapper::get()),
      mInitCheck(NO_ERROR), mIndex(-1)
{
    width  = 
    height = 
    stride = 
    format = 
    usage  = 0;
    handle = NULL;
}
```

服务端进程接收到应用程序进程requestBuffer请求后，将新创建的GraphicBuffer对象发送给应用程序。上面可以看到，应用程序进程这边也创建了一个GraphicBuffer对象，在SurfaceFlinger服务进程中也同样创建了一个GraphicBuffer对象，SurfaceFlinger服务进程只是将它进程中创建的GraphicBuffer对象传输给应用程序进程，我们知道，一个对象要在进程间传输必须继承于Flattenable类，并且实现flatten和unflatten方法，flatten方法用于序列化该对象，unflatten方法用于反序列化对象。

![](https://i-blog.csdnimg.cn/blog_migrate/3797fe1b20fff545ceb46a8e93e8a55c.png)

GraphicBuffer同样继承于Flattenable类并实现了flatten和unflatten方法，在应用程序读取来自服务进程的GraphicBuffer对象时，也就是result = reply.read(\*p)，会调用GraphicBuffer类的unflatten函数进行反序列化过程： 

```
status_t GraphicBuffer::unflatten(void const* buffer, size_t size,
        int fds[], size_t count)
{
    if (size < 8*sizeof(int)) return NO_MEMORY;
    int const* buf = static_cast<int const*>(buffer);
    if (buf[0] != 'GBFR') return BAD_TYPE;

    const size_t numFds  = buf[6];
    const size_t numInts = buf[7];

    const size_t sizeNeeded = (8 + numInts) * sizeof(int);
    if (size < sizeNeeded) return NO_MEMORY;

    size_t fdCountNeeded = 0;
    if (count < fdCountNeeded) return NO_MEMORY;

    if (handle) {
        
        free_handle();
    }
    if (numFds || numInts) {
        width  = buf[1];
        height = buf[2];
        stride = buf[3];
        format = buf[4];
        usage  = buf[5];

        native_handle* h = native_handle_create(numFds, numInts);
        memcpy(h->data,          fds,     numFds*sizeof(int));
        memcpy(h->data + numFds, &buf[8], numInts*sizeof(int));
        handle = h;
    } else {
        width = height = stride = format = usage = 0;
        handle = NULL;
    }
    mOwner = ownHandle;
    if (handle != 0) {

        status_t err = mBufferMapper.registerBuffer(handle);
        if (err != NO_ERROR) {
            native_handle_close(handle);
            native_handle_delete(const_cast<native_handle*>(handle));
            handle = NULL;
            return err;
        }
    }
    return NO_ERROR;
}

```

![](https://i-blog.csdnimg.cn/blog_migrate/47b0fc78005cce451a301ed0217d5ec4.png)

应用程序进程得到服务端进程返回来的GraphicBuffer对象后，还需要将该图形buffer映射到应用程序进程地址空间，有关图形缓存区的映射详细过程请查看[Android图形缓冲区映射过程源码分析](https://link.csdn.net/?target=http%3A%2F%2Fblog.csdn.net%2Fyangwen123%2Farticle%2Fdetails%2F12234931%3Flogin%3Dfrom_csdn "Android图形缓冲区映射过程源码分析")。

#### 服务端处理

frameworks\\native\\libs\\gui\\ISurfaceTexture.cpp$BnSurfaceTexture

```
status_t BnSurfaceTexture::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case REQUEST_BUFFER: {
            CHECK_INTERFACE(ISurfaceTexture, data, reply);
            int bufferIdx   = data.readInt32();
            sp<GraphicBuffer> buffer;

            int result = requestBuffer(bufferIdx, &buffer);
            reply->writeInt32(buffer != 0);

            if (buffer != 0) {
                reply->write(*buffer);
            }
            reply->writeInt32(result);
            return NO_ERROR;
        } break;
    }
    return BBinder::onTransact(code, data, reply, flags);
}
```

frameworks\\native\\libs\\gui\\BufferQueue.cpp

```
status_t BufferQueue::requestBuffer(int slot, sp<GraphicBuffer>* buf) {
    ATRACE_CALL();
    ST_LOGV("requestBuffer: slot=%d", slot);
    Mutex::Autolock lock(mMutex);
    if (mAbandoned) {
        ST_LOGE("requestBuffer: SurfaceTexture has been abandoned!");
        return NO_INIT;
    }
    if (slot < 0 || mBufferCount <= slot) {
        ST_LOGE("requestBuffer: slot index out of range [0, %d]: %d",
                mBufferCount, slot);
        return BAD_VALUE;
    }
    mSlots[slot].mRequestBufferCalled = true;
    *buf = mSlots[slot].mGraphicBuffer;
    return NO_ERROR;
}
```

由于GraphicBuffer继承于Flattenable类，在[Android 数据Parcel序列化过程源码分析](https://link.csdn.net/?target=http%3A%2F%2Fblog.csdn.net%2Fyangwen123%2Farticle%2Fdetails%2F9142521%3Flogin%3Dfrom_csdn "Android 数据Parcel序列化过程源码分析")中介绍了，将一个对象写入到Parcel中，需要使用flatten函数序列化该对象：

frameworks\\native\\libs\\ui\\GraphicBuffer.cpp

```
status_t GraphicBuffer::flatten(void* buffer, size_t size,
        int fds[], size_t count) const
{
    size_t sizeNeeded = GraphicBuffer::getFlattenedSize();
    if (size < sizeNeeded) return NO_MEMORY;

    size_t fdCountNeeded = GraphicBuffer::getFdCount();
    if (count < fdCountNeeded) return NO_MEMORY;

    int* buf = static_cast<int*>(buffer);
    buf[0] = 'GBFR';
    buf[1] = width;
    buf[2] = height;
    buf[3] = stride;
    buf[4] = format;
    buf[5] = usage;
    buf[6] = 0;
    buf[7] = 0;

    if (handle) {
        buf[6] = handle->numFds;
        buf[7] = handle->numInts;
        native_handle_t const* const h = handle;
        memcpy(fds,     h->data,             h->numFds*sizeof(int));
        memcpy(&buf[8], h->data + h->numFds, h->numInts*sizeof(int));
    }

    return NO_ERROR;
}
```

到此我们就介绍完了应用程序请求BufferQueue出列一个可用图形buffer的完整过程，那么应用程序什么时候发出这个请求呢？我们知道，在使用Surface绘图前，需要调用SurfaceHolder的lockCanvas()函数来锁定画布，然后才可以在画布上作图，应用程序就是在这个时候向SurfaceFlinger服务进程中的BufferQueue申请图形缓存区的。

![](https://i-blog.csdnimg.cn/blog_migrate/8660c7ced538088699dccca1d807faf1.jpeg)

当应用程序完成绘图后，需要调用SurfaceHolder的unlockCanvasAndPost(canvas)函数来释放画布，并请求SurfaceFlinger服务进程混合并显示该图像。

![](https://i-blog.csdnimg.cn/blog_migrate/d39d2aadca9dd81e0cfbb81ffccdc6f2.jpeg)

从以上时序图可以看到，应用程序完成绘图后，首先对当前这块图形buffer进行解锁，然后调用queueBuffer()函数请求SurfaceFlinger服务进程中的BufferQueue将当前已绘制好图形的buffer入列，也就是将当前buffer交还给BufferQueue管理。应用程序这个生产者在这块buffer中生产出了图像产品后，就需要将buffer中的图像产品放到BufferQueue销售市场中交易，SurfaceFlinger这个消费者得知市场上有新的图像产品出现，就立刻请求VSync信号，在下一次VSync到来时，SurfaceFlinger混合当前市场上的所有图像产品，并显示到屏幕上，从而完成图像产品的消费过程。

#### 客户端请求

frameworks\\native\\libs\\gui\\SurfaceTextureClient.cpp 

```
int SurfaceTextureClient::queueBuffer(android_native_buffer_t* buffer) {
    ATRACE_CALL();
    ALOGV("SurfaceTextureClient::queueBuffer");
    Mutex::Autolock lock(mMutex);
    int64_t timestamp;
    if (mTimestamp == NATIVE_WINDOW_TIMESTAMP_AUTO) {
        timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
        ALOGV("SurfaceTextureClient::queueBuffer making up timestamp: %.2f ms",
             timestamp / 1000000.f);
    } else {
        timestamp = mTimestamp;
    }
    int i = getSlotFromBufferLocked(buffer);
    if (i < 0) {
        return i;
    }
    
    Rect crop;
    mCrop.intersect(Rect(buffer->width, buffer->height), &crop);

    ISurfaceTexture::QueueBufferOutput output;
    ISurfaceTexture::QueueBufferInput input(timestamp, crop, mScalingMode,
            mTransform);
    status_t err = mSurfaceTexture->queueBuffer(i, input, &output);
    if (err != OK)  {
        ALOGE("queueBuffer: error queuing buffer to SurfaceTexture, %d", err);
    }
    uint32_t numPendingBuffers = 0;
    output.deflate(&mDefaultWidth, &mDefaultHeight, &mTransformHint,&numPendingBuffers);
    mConsumerRunningBehind = (numPendingBuffers >= 2);
    return err;
}
```

frameworks\\native\\libs\\gui\\ ISurfaceTexture.cpp $BpSurfaceTexture

```
virtual status_t queueBuffer(int buf,const QueueBufferInput& input, QueueBufferOutput* output) {
Parcel data, reply;
data.writeInterfaceToken(ISurfaceTexture::getInterfaceDescriptor());
data.writeInt32(buf);
memcpy(data.writeInplace(sizeof(input)), &input, sizeof(input));
status_t result = remote()->transact(QUEUE_BUFFER, data, &reply);
if (result != NO_ERROR) {
return result;
}
memcpy(output, reply.readInplace(sizeof(*output)), sizeof(*output));
result = reply.readInt32();
return result;
}

```

#### 服务端处理

frameworks\\native\\libs\\gui\\ ISurfaceTexture.cpp $BnSurfaceTexture

```
status_t BnSurfaceTexture::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case QUEUE_BUFFER: {
            CHECK_INTERFACE(ISurfaceTexture, data, reply);
            int buf = data.readInt32();
            QueueBufferInput const* const input =
                    reinterpret_cast<QueueBufferInput const *>(
                            data.readInplace(sizeof(QueueBufferInput)));
            QueueBufferOutput* const output =
                    reinterpret_cast<QueueBufferOutput *>(
                            reply->writeInplace(sizeof(QueueBufferOutput)));
            status_t result = queueBuffer(buf, *input, output);
            reply->writeInt32(result);
            return NO_ERROR;
        } break;
    }
    return BBinder::onTransact(code, data, reply, flags);
}

```

frameworks\\native\\libs\\gui\\ BufferQueue.cpp

```
status_t BufferQueue::queueBuffer(int buf,
        const QueueBufferInput& input, QueueBufferOutput* output) {
    ATRACE_CALL();
    ATRACE_BUFFER_INDEX(buf);
    Rect crop;
    uint32_t transform;
    int scalingMode;
    int64_t timestamp;
    input.deflate(×tamp, &crop, &scalingMode, &transform);
    ST_LOGV("queueBuffer: slot=%d time=%#llx crop=[%d,%d,%d,%d] tr=%#x "
            "scale=%s",
            buf, timestamp, crop.left, crop.top, crop.right, crop.bottom,
            transform, scalingModeName(scalingMode));
    sp<ConsumerListener> listener;

    { 
        Mutex::Autolock lock(mMutex);
        if (mAbandoned) {
            ST_LOGE("queueBuffer: SurfaceTexture has been abandoned!");
            return NO_INIT;
        }
        if (buf < 0 || buf >= mBufferCount) {
            ST_LOGE("queueBuffer: slot index out of range [0, %d]: %d",
                    mBufferCount, buf);
            return -EINVAL;
        } else if (mSlots[buf].mBufferState != BufferSlot::DEQUEUED) {
            ST_LOGE("queueBuffer: slot %d is not owned by the client "
                    "(state=%d)", buf, mSlots[buf].mBufferState);
            return -EINVAL;
        } else if (!mSlots[buf].mRequestBufferCalled) {
            ST_LOGE("queueBuffer: slot %d was enqueued without requesting a "
                    "buffer", buf);
            return -EINVAL;
        }
        const sp<GraphicBuffer>& graphicBuffer(mSlots[buf].mGraphicBuffer);
        Rect bufferRect(graphicBuffer->getWidth(), graphicBuffer->getHeight());
        Rect croppedCrop;
        crop.intersect(bufferRect, &croppedCrop);
        if (croppedCrop != crop) {
            ST_LOGE("queueBuffer: crop rect is not contained within the "
                    "buffer in slot %d", buf);
            return -EINVAL;
        }
        if (mSynchronousMode) {
            
            mQueue.push_back(buf);

            
            
            listener = mConsumerListener;
        } else {
            
            if (mQueue.empty()) {
                mQueue.push_back(buf);

                
                
                
                listener = mConsumerListener;
            } else {
                Fifo::iterator front(mQueue.begin());
                
                mSlots[*front].mBufferState = BufferSlot::FREE;
                
                *front = buf;
            }
        }
        mSlots[buf].mTimestamp = timestamp;
        mSlots[buf].mCrop = crop;
        mSlots[buf].mTransform = transform;
        switch (scalingMode) {
            case NATIVE_WINDOW_SCALING_MODE_FREEZE:
            case NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW:
            case NATIVE_WINDOW_SCALING_MODE_SCALE_CROP:
                break;
            default:
                ST_LOGE("unknown scaling mode: %d (ignoring)", scalingMode);
                scalingMode = mSlots[buf].mScalingMode;
                break;
        }
        mSlots[buf].mBufferState = BufferSlot::QUEUED;
        mSlots[buf].mScalingMode = scalingMode;
        mFrameCounter++;
        mSlots[buf].mFrameNumber = mFrameCounter;
        mBufferHasBeenQueued = true;

        mDequeueCondition.broadcast();
        output->inflate(mDefaultWidth, mDefaultHeight, mTransformHint,
                mQueue.size());
        ATRACE_INT(mConsumerName.string(), mQueue.size());
    } 
    
    if (listener != 0) {
        listener->onFrameAvailable();
    }
    return OK;
}
```

在前面构造SurfaceTexture对象时，通过mBufferQueue->consumerConnect(proxy)将ProxyConsumerListener监听器保存到了BufferQueue的成员变量mConsumerListener中，同时又将SurfaceTexture对象保存到ProxyConsumerListener的成员变量mConsumerListener中。

frameworks\\native\\libs\\gui\\SurfaceTexture.cpp

```
SurfaceTexture::SurfaceTexture(GLuint tex, bool allowSynchronousMode,
        GLenum texTarget, bool useFenceSync, const sp<BufferQueue> &bufferQueue) 
{
    ...
    wp<BufferQueue::ConsumerListener> listener;
    sp<BufferQueue::ConsumerListener> proxy;
    listener = static_cast<BufferQueue::ConsumerListener*>(this);
    proxy = new BufferQueue::ProxyConsumerListener(listener);
    status_t err = mBufferQueue->consumerConnect(proxy);
    ....
}
```

因此BufferQueue通过回调ProxyConsumerListener的onFrameAvailable()函数来通知消费者图形buffer已经准备就绪。  
frameworks\\native\\libs\\gui\\ BufferQueue.cpp 

```
void BufferQueue::ProxyConsumerListener::onFrameAvailable() {
    sp<BufferQueue::ConsumerListener> listener(mConsumerListener.promote());
    if (listener != NULL) {
        listener->onFrameAvailable();
    }
}
```

ProxyConsumerListener又回调SurfaceTexture的onFrameAvailable()函数来处理。

frameworks\\native\\libs\\gui\\ SurfaceTexture.cpp

```
void SurfaceTexture::onFrameAvailable() {
    ST_LOGV("onFrameAvailable");

    sp<FrameAvailableListener> listener;
    { 
        Mutex::Autolock lock(mMutex);
        listener = mFrameAvailableListener;
    }

    if (listener != NULL) {
        ST_LOGV("actually calling onFrameAvailable");
        listener->onFrameAvailable();
    }
}
```

在Layer对象的onFirstRef()函数中，通过调用SurfaceTexture的setFrameAvailableListener函数来为SurfaceTexture设置buffer可用监器为FrameQueuedListene，其实就是将FrameQueuedListener对象保存到SurfaceTexture的成员变量mFrameAvailableListener中：  
frameworks\\native\\services\\surfaceflinger\\ Layer.cpp 

```
mSurfaceTexture->setFrameAvailableListener(new FrameQueuedListener(this));
```

因此buffer可用通知最终又交给FrameQueuedListener来处理：

```
struct FrameQueuedListener : public SurfaceTexture::FrameAvailableListener {
        FrameQueuedListener(Layer* layer) : mLayer(layer) { }
private:
wp<Layer> mLayer;
virtual void onFrameAvailable() {
sp<Layer> that(mLayer.promote());
if (that != 0) {
that->onFrameQueued();
}
}
};
```

FrameQueuedListener的onFrameAvailable()函数又调用Layer类的onFrameQueued()来处理

```
void Layer::onFrameQueued() {
    android_atomic_inc(&mQueuedFrames);
    mFlinger->signalLayerUpdate();
}
```

接着又通知SurfaceFlinger来更新Layer层。  
frameworks\\native\\services\\surfaceflinger\\ SurfaceFlinger.cpp 

```
void SurfaceFlinger::signalLayerUpdate() {
    mEventQueue.invalidate();
}
```

该函数就是向SurfaceFlinger的事件队列中发送一个Vsync信号请求  
frameworks\\native\\services\\surfaceflinger\\ MessageQueue.cpp 

```
void MessageQueue::invalidate() {
    mEvents->requestNextVsync();
}
```

应用程序入列一个图形buffer的整个过程如下：

![](https://img-blog.csdn.net/20140401110108937?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQveWFuZ3dlbjEyMw==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)
