_努比亚技术团队原创内容，转载请务必注明出处。_

## 8. 应用是如何绘图的

目前很多游戏类应用都是借由SurfaceView申请到画布，然后自主上帧，并不依赖Vsync信号， 所以本章通过几个helloworld示例来看下应用侧是如何绘图和上帧的。

由于java层很多接口是对C层接口的JNI封装，这里我们只看一些C层接口的用法。下面的示例代码为缩减篇幅把一些异常处理部分的代码去除了，只保留了重要的部分，如果读者需要执行示例代码，可以自行加入一些异常处理部分。

### 8.1. 无图形库支持下的绘图

下面的示例中演示的是如何使用C层接口向SurfaceFlinger申请一块画布，然后不使用任何图形库，直接修改画布上的像素值，最后提交给SurfaceFlinger显示。

```rust
int main()
{
    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();//在应用和SurfaceFlinger沟通过程中要使用到binder, 所以这里要先初始化binder线程池

    sp<SurfaceComposerClient> client = new SurfaceComposerClient();//SurfaceComposerClient是SurfaceFlinger在应用侧的代表， SurfaceFlinger的接口通过它来提供
    client->initCheck();
    //先通过createSurface接口来申请一块画布，参数里包含对画布起的名字，大小，位深信息
    sp<SurfaceControl> surfaceControl = client->createSurface(String8("Console Surface"),800, 600, PIXEL_FORMAT_RGBA_8888);

    SurfaceComposerClient::Transaction t;
    t.setLayer(surfaceControl, 0x40000000).apply();
    //通过getSurface接口获取到Surface对象
    sp<Surface> surface = surfaceControl->getSurface();
    
    ANativeWindow_Buffer buffer;
    //通过Surface的lock方法调用到dequeueBuffer，获取到一个BufferQueue可用的Slot
    status_t err = surface->lock(&buffer, NULL);// &clipRegin

    void* addr = buffer.bits;
    ssize_t len = buffer.stride * 4 * buffer.height;
    memset(addr, 255, len);//这里绘图，由于我们没有使用任何图形库，所以这里把内存填成255， 画一个纯色画面
    
    surface->unlockAndPost();//这里会调用到queueBuffer,把我们绘制好的画面提交给SurfaceFlinger

    printf("sleep...\n");
    usleep(5 * 1000 * 1000);
    
    surface.clear();
    surfaceControl.clear();
    
    printf("complete. CTRL+C to finish.\n");
    IPCThreadState::self()->joinThreadPool();
    return 0;
}
```

在上面的示例中，几个关建点是，第一步，先创建出一个SurfaceComposerClient，它是我们和Surfaceflinger沟通的桥梁，第二步，通过SurfaceComposerClient的createLayer接口创建一个SurfaceControl，这是我们控制Surface的一个工具，第三步，从SurfaceControl的getSurface接口来获取Surface对象，这是我们操作BufferQueue的接口。

有了Surface对象，我们可以通过Surface的lock方法来dequeueBuffer, 再通过unlockAndPost接口来queueBuffer, 循环执行，我们就可以对画布进行连续绘制和提交数据了，屏幕上动态的画面就出来了。

所以对于SurfaceFlinger或者说对于Display系统底层所提供的接口主要就是这三个SurfaceComposerClient， SurfaceControl和Surface. 这里我们不妨称其为Display系统接口三大件。

### 8.2. 有图形库支持下的绘图

在上节示例中，我们并没有去绘画复杂的图案，只是使用内存填充的方式画了一个纯色画面，在本节中我们将尝试使用图形库在给定的画布上画一些复杂的图案，比如画一张图片上去。

在上节的讨论中我们知道要画画面出来，要拿到Display的三大件（SurfaceComposerClient， SurfaceControl和Surface），接下来拿到画布后我们使用skia库来画一张图片到屏幕上。

```php
using namespace android;
//先写一个函数把图片转成一个bitmap
static status_t initBitmap(SkBitmap* bitmap, const char* fileName) {
    if (fileName == NULL) {
        return NO_INIT;
    }
    sk_sp<SkData> data = SkData::MakeFromFileName(fileName);
    sk_sp<SkImage> image = SkImage::MakeFromEncoded(data);
     bool  result  = image->asLegacyBitmap(bitmap, SkImage::kRO_LegacyBitmapMode);
    if(!result ){
        printf("decode picture fail!");
        return NO_INIT;
    }
    return NO_ERROR;
}

int main()
{
    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();//和上一示例一样要开启binder线程池

    // create a client to surfaceflinger
    sp<SurfaceComposerClient> client = new SurfaceComposerClient();//三大件第一件
    client->initCheck();
    sp<SurfaceControl> surfaceControl = client->createSurface(String8("Consoleplayer Surface"),800, 600, PIXEL_FORMAT_RGBA_8888);//三大件第二件

    SurfaceComposerClient::Transaction t;
    t.setLayer(surfaceControl, 0x40000000).apply();

    sp<Surface> surface = surfaceControl->getSurface();//三大件第三件
    sp<IGraphicBufferProducer> graphicBufferProducer = surface->getIGraphicBufferProducer();

    ANativeWindow_Buffer buffer;
    status_t err = surface->lock(&buffer, NULL);//调用dequeueBuffer把buffer拿来
    
    SkBitmap* bitmapDevice = new SkBitmap;
    SkIRect* updateRect = new SkIRect;
    SkBitmap* bitmap = new SkBitmap;
    initBitmap(bitmap, "/sdcard/picture.png");//从文件读一个bitmap出来
    
    printf("decode picture done.\n");
    ssize_t bpr = buffer.stride * bytesPerPixel(buffer.format);
    SkColorType config = convertPixelFormat(buffer.format);
    bitmapDevice->setInfo(SkImageInfo::Make(buffer.width, buffer.height, config, kPremul_SkAlphaType), bpr);
    //上面我们创建了另一个SkBitmap对象bitmapDevice
    if (buffer.width > 0 && buffer.height > 0) {
        bitmapDevice->setPixels(buffer.bits);//这里把帧缓冲区buffer的地址设给了bitmapDevice，这时和bitmapDevice画东西就是在向帧缓冲区buffer画东西
    } else {
        bitmapDevice->setPixels(NULL);
    }
    //SkRegion region;
    printf("to create canvas..\n");
    SkCanvas* nativeCanvas = new SkCanvas(*bitmapDevice);
    SkRect sr;
    sr.set(*updateRect);
    nativeCanvas->clipRect(sr);
    SkPaint paint;
    nativeCanvas->clear(SK_ColorBLACK);
    const SkRect dst = SkRect::MakeXYWH(0,0,800, 600);
    paint.setAlpha(255);
    const SkIRect src1 = SkIRect::MakeXYWH(0, 0, bitmap->width(), bitmap->height());
    printf("draw ....\n");
    nativeCanvas->drawBitmapRect((*bitmap), src1, dst, &paint);//调用SkCanvas的drawBitmapRect把图片画到bitmapDevice，也就是画到了从Surface申请到的帧缓冲区buffer中
    
    surface->unlockAndPost();//调用queueBuffer把buffer提交给SurfaceFlinger显示

    printf("sleep...\n");
    usleep(10 * 1000 * 1000);
    
    surface.clear();
    surfaceControl.clear();
    
    printf("test complete. CTRL+C to finish.\n");
    IPCThreadState::self()->joinThreadPool();
    return 0;
}
```

在上面的示例中获取到帧缓冲区buffer的方式和上一个例子是一样的，不同点 是我们把申请到的buffer的地址空间给到了skia库，然后我们通过skia提供的操作接口把一张图片画到了帧缓冲区buffer中，由此可以看出我们想使用图形库来操作帧缓冲区的关键是要把帧缓冲区buffer的地址对接到图形库提供的接口上。

在android平台上，我们通常不会直接使用CPU去绘图，通常是调用opengl或其他图形库去指挥GPU去做这些绘图的事情，那么又是如何使用opengl库来完成绘图的呢？

### 8.3. 使用OpenGL&EGL的绘图

由上面第二个例子可知，要想使用一个图形库来向帧缓冲区buffer绘图的关建是要把对应的buffer给到图形库， 我们知道opengl是一套设备无关的api接口，它和平台是无关的，所以和Surface接口的任务是由EGL库来完成的，帧缓冲区buffer要和EGL库对接。

在hwui绘图中是以如下结构对接的：

![](https://upload-images.jianshu.io/upload_images/26874665-943a63e19f669ba4.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210904123515681.png

首先EGL库会提供一个EGLSurface的对象，这个对象是对三大件中的Surface的一个封装，它本身与帧提交相关部分提供了两个接口：dequeue/queue,分别对应Surface的dequeueBuffer和queueBuffer.

下面我们通过一个示例来看下它在C层是如何使用和与三大件对接的：

```php
using namespace android;

int main()
{
    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();//同样地开启binder线程池

    // create a client to surfaceflinger
    sp<SurfaceComposerClient> client = new SurfaceComposerClient();//三大件第一件
    client->initCheck();
    sp<SurfaceControl> surfaceControl = client->createSurface(String8("Consoleplayer Surface"),800, 600, PIXEL_FORMAT_RGBA_8888);//三大件第二件

    SurfaceComposerClient::Transaction t;
    t.setLayer(surfaceControl, 0x40000000).apply();

    sp<Surface> surface = surfaceControl->getSurface();//三大件第三件
    
    // initialize opengl and egl
    const EGLint attribs[] = {
            EGL_RED_SIZE,   8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE,  8,
            EGL_DEPTH_SIZE, 0,
            EGL_NONE
    };
    
    //开始初始化EGL库
    EGLint w, h;
    EGLSurface eglSurface;
    EGLint numConfigs;
    EGLConfig config;
    EGLContext context;
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    
    eglInitialize(display, 0, 0);
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    eglSurface = eglCreateWindowSurface(display, config, surface.get(), NULL);//创建eglSurface(对Surface的一个封装)
    context = eglCreateContext(display, config, NULL, NULL);
    eglQuerySurface(display, eglSurface, EGL_WIDTH, &w);
    eglQuerySurface(display, eglSurface, EGL_HEIGHT, &h);

    if (eglMakeCurrent(display, eglSurface, eglSurface, context) == EGL_FALSE)//会调用dequeue以获取帧缓冲区buffer
        return NO_INIT;

    glShadeModel(GL_FLAT);
    glDisable(GL_DITHER);
    glDisable(GL_SCISSOR_TEST);
    //draw red 
    glClearColor(255,0,0,1);//这里用opengl库来一个纯红色的画面
    glClear(GL_COLOR_BUFFER_BIT);
    
    eglSwapBuffers(display, eglSurface);//这里会调用到Surface的queueBuffer方法，提交画好的帧缓冲区数据


    printf("sleep...\n");
    usleep(10 * 1000 * 1000);
    
    surface.clear();
    surfaceControl.clear();
    
    printf("test complete. CTRL+C to finish.\n");
    IPCThreadState::self()->joinThreadPool();
    return 0;
}
```

在上面的例子中我们看到了opengl&egl库对帧缓冲区buffer的使用方式，首先和8.1的示例中一样从三大件中获取的帧缓冲区操作接口，只是这里我们不再直接使用该接口，而是把Surface对象给到EGL库，由EGL库去使用它，我们使用opengl 的api来间接操作帧缓冲区buffer，这些操作包括申请新的BufferQueue slot和提交绘制好的BufferQueue slot.

asd

### 本章小结

本章我们通过三个示例程序了解了下display部分给应用层设计的接口，了解到了通过三大件可以拿到帧缓冲区buffer, 之后应用如何作画就是应用层的事情了，应用可以选择不使用图形库，也可以选择图形库让cpu来作画，也可以使用像opengl&egl这样的库来指挥GPU来作画。

## 9. 应用画面更新总结

通过以上章节的了解，APP的画面要显示到屏幕上大致上要经过如下图所示系统组件的处理：

![](https://upload-images.jianshu.io/upload_images/26874665-be31f4e960d74226.png?imageMogr2/auto-orient/strip|imageView2/2/w/1090/format/webp)

image-20210922143904647.png

首先App向SurfaceFlinger申请画布(通过dequeueBuffer接口)，SurfaceFlinger内部有一个BufferQueue的管理实体，它会分配一个GraphicBuffer给到APP， App拿到buffer后调用图形库向这块buffer内绘画。

APP绘画完成后使用向SurfaceFlinger提交绘制完成的buffer(通过queueBuffer接口)， 当然这时候的绘制完成只是说在CPU侧绘制完成，此时GPU可能还在该buffer上作画，所以这时向SurfaceFlinger提交数据的同时还会带上一个acquireFence，使用接下来使用该buffer的人能知道什么时候buffer使用完毕了。

SurfaceFlinger收到应用提交的帧缓冲区buffer后是在下一个vsync-sf信号来时做处理，首先遍历所有的Layer, 找到哪些Layer有上帧， 通过acquireBuffer把Buffer拿出来，通知给HWC Service去参与合成， 最后调用HWC Service的presentDisplay接口来告知HWC Service SurfaceFlinger的工作已完成。

HWC Service收到合成任务后开始合成数据，在SurfaceFlinger调用presetDisplay时会去调用DRM接口DRMAtomicReq::Commit通知kernel可以向DDIC发送数据了.

如果有TE信号来提示已进入消隐区，这时DRM驱动会马上开始通过DSI总线向DDIC传输数据，与此同时Panel的Disp Scan也在进行中，传输完成后这帧画面就完整地显示到了屏幕上。

至此，一帧画面的更新过程就完成了，我们这里讲了这么久的一个复杂的过程，其实在高刷手机上一秒钟要重复做100多次！_

## 10. 结语

Android的Display系统是Android平台上一个相对比较复杂的系统，文中所述均是笔者通过阅读源码、阅读网上其他人分享的文章、平时工作中的感悟以及在工作中向同事请教总结而来。限于自身的知识结构和技术背景，未必有些理解是正确的，请读者阅读过程中多思考，多以源码为准，文中所述请仅做参考。文中有不正确的地方也欢迎大家批评指正。

特别感谢如下作者的知识分享：

作者： [ariesjzj](https://links.jianshu.com/go?to=https%3A%2F%2Fjinzhuojun.blog.csdn.net%2F) 题目：《Android中的GraphicBuffer同步机制-Fence》 地址：[https://blog.csdn.net/jinzhuojun/article/details/39698317](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.csdn.net%2Fjinzhuojun%2Farticle%2Fdetails%2F39698317)

作者：[-Yaong-](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.cnblogs.com%2Fyaongtime%2F) 题目：《linux GPU上多个buffer间的同步之ww_mutex、dma_fence的使用 笔记》地址[https://www.cnblogs.com/yaongtime/p/14332526.html](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.cnblogs.com%2Fyaongtime%2Fp%2F14332526.html)

作者：[lyf](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.zhihu.com%2Fpeople%2Fliuyf5231) 题目《android graphic(16)—fence(简化)》 地址：[https://zhuanlan.zhihu.com/p/68782817](https://links.jianshu.com/go?to=https%3A%2F%2Fzhuanlan.zhihu.com%2Fp%2F68782817)

作者：[何小龙](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.csdn.net%2Fhexiaolong2009) 题目：《LCD显示异常分析——撕裂(tear effect)》 地址：[https://blog.csdn.net/hexiaolong2009/article/details/79319512?spm=1001.2014.3001.5501](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.csdn.net%2Fhexiaolong2009%2Farticle%2Fdetails%2F79319512%3Fspm%3D1001.2014.3001.5501)

作者：[迅猛一只虎](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.csdn.net%2Fwending1986) 题目：《LCD timing 时序参数总结》 地址：[https://blog.csdn.net/wending1986/article/details/106837597](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.csdn.net%2Fwending1986%2Farticle%2Fdetails%2F106837597)

作者：[kerneler_](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.csdn.net%2Fskyflying2012) 题目：《LCD屏时序分析》 地址：[https://blog.csdn.net/skyflying2012/article/details/8553893](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.csdn.net%2Fskyflying2012%2Farticle%2Fdetails%2F8553893)

©著作权归作者所有,转载或内容合作请联系作者

38人点赞

[Android画面更新流程分析](/nb/51272374)

更多精彩内容，就在简书APP

![](https://upload.jianshu.io/images/js-qrc.png)

"小礼物走一走，来简书关注我"

赞赏支持还没有人赞赏，支持一下

[![](https:https://upload.jianshu.io/users/upload_avatars/26874665/2a40e37f-0985-4482-b744-158567be55c4.PNG?imageMogr2/auto-orient/strip|imageView2/1/w/100/h/100/format/webp)](/u/167b54662111)

[努比亚技术团队](/u/167b54662111 "努比亚技术团队")努比亚技术团队专注于Android应用、框架、驱动、内核、通信、显示及性能优化/稳定性/功耗优...

总资产36共写了11.4W字获得993个赞共1,720个粉丝

关注

- [人面猴](/p/1003a129be45)
    
    序言：七十年代末，一起剥皮案震惊了整个滨河市，随后出现的几起案子，更是在滨河造成了极大的恐慌，老刑警刘岩，带你破解...
    
    [![](https:https://upload.jianshu.io/users/upload_avatars/15878160/783c64db-45e5-48d7-82e4-95736f50533e.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)沈念sama](/u/dcd395522934)阅读 213,254评论 6赞 492
    
- [死咒](/p/1c4506f51019)
    
    序言：滨河连续发生了三起死亡事件，死亡现场离奇诡异，居然都是意外死亡，警方通过查阅死者的电脑和手机，发现死者居然都...
    
    [![](https:https://upload.jianshu.io/users/upload_avatars/15878160/783c64db-45e5-48d7-82e4-95736f50533e.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)沈念sama](/u/dcd395522934)阅读 90,875评论 3赞 387
    
- [救了他两次的神仙让他今天三更去死](/p/1ded57e57939)
    
    文/潘晓璐 我一进店门，熙熙楼的掌柜王于贵愁眉苦脸地迎上来，“玉大人，你说我怎么就摊上这事。” “怎么了？”我有些...
    
    [![](https://cdn2.jianshu.io/assets/default_avatar/8-a356878e44b45ab268a3b0bbaaadeeb7.jpg)开封第一讲书人](/u/5891e866c93e)阅读 158,682评论 0赞 348
    
- [道士缉凶录：失踪的卖姜人](/p/25685c1b1f2b)
    
    文/不坏的土叔 我叫张陵，是天一观的道长。 经常有香客问我，道长，这世上最难降的妖魔是什么？ 我笑而不...
    
    [![](https://cdn2.jianshu.io/assets/default_avatar/8-a356878e44b45ab268a3b0bbaaadeeb7.jpg)开封第一讲书人](/u/5891e866c93e)阅读 56,896评论 1赞 285
    
- ﻿[港岛之恋（遗憾婚礼）](/p/553802eff5d6)
    
    正文 为了忘掉前任，我火速办了婚礼，结果婚礼上，老公的妹妹穿的比我还像新娘。我一直安慰自己，他们只是感情好，可当我...
    
    [![](https:https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](/u/0f438ff0a55f)阅读 66,015评论 6赞 385
    
- [恶毒庶女顶嫁案：这布局不是一般人想出来的](/p/59985a89b4ef)
    
    文/花漫 我一把揭开白布。 她就那样静静地躺着，像睡着了一般。 火红的嫁衣衬着肌肤如雪。 梳的纹丝不乱的头发上，一...
    
    [![](https://cdn2.jianshu.io/assets/default_avatar/8-a356878e44b45ab268a3b0bbaaadeeb7.jpg)开封第一讲书人](/u/5891e866c93e)阅读 50,152评论 1赞 291
    
- [城市分裂传说](/p/62a01de427e0)
    
    那天，我揣着相机与录音，去河边找鬼。 笑死，一个胖子当着我的面吹牛，可吹牛的内容都是我干的。 我是一名探鬼主播，决...
    
    [![](https:https://upload.jianshu.io/users/upload_avatars/15878160/783c64db-45e5-48d7-82e4-95736f50533e.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)沈念sama](/u/dcd395522934)阅读 39,208评论 3赞 412
    
- [双鸳鸯连环套：你想象不到人心有多黑](/p/6ccdc163474a)
    
    文/苍兰香墨 我猛地睁开眼，长吁一口气：“原来是场噩梦啊……” “哼！你这毒妇竟也来了？” 一声冷哼从身侧响起，我...
    
    [![](https://cdn2.jianshu.io/assets/default_avatar/8-a356878e44b45ab268a3b0bbaaadeeb7.jpg)开封第一讲书人](/u/5891e866c93e)阅读 37,962评论 0赞 268
    
- [万荣杀人案实录](/p/8796e3463067)
    
    序言：老挝万荣一对情侣失踪，失踪者是张志新（化名）和其女友刘颖，没想到半个月后，有当地人在树林里发现了一具尸体，经...
    
    [![](https:https://upload.jianshu.io/users/upload_avatars/15878160/783c64db-45e5-48d7-82e4-95736f50533e.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)沈念sama](/u/dcd395522934)阅读 44,388评论 1赞 304
    
- ﻿[护林员之死](/p/8a691dd8fa34)
    
    正文 独居荒郊野岭守林人离奇死亡，尸身上长有42处带血的脓包…… 初始之章·张勋 以下内容为张勋视角 年9月15日...
    
    [![](https:https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](/u/0f438ff0a55f)阅读 36,700评论 2赞 327
    
- ﻿[白月光启示录](/p/a5293fa3b5e0)
    
    正文 我和宋清朗相恋三年，在试婚纱的时候发现自己被绿了。 大学时的朋友给我发了我未婚夫和他白月光在一起吃饭的照片。...
    
    [![](https:https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](/u/0f438ff0a55f)阅读 38,867评论 1赞 341
    
- [活死人](/p/a83aa7e71001)
    
    序言：一个原本活蹦乱跳的男人离奇死亡，死状恐怖，灵堂内的尸体忽然破棺而出，到底是诈尸还是另有隐情，我是刑警宁泽，带...
    
    [![](https:https://upload.jianshu.io/users/upload_avatars/15878160/783c64db-45e5-48d7-82e4-95736f50533e.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)沈念sama](/u/dcd395522934)阅读 34,551评论 4赞 335
    
- ﻿[日本核电站爆炸内幕](/p/bee7d9c3fcf9)
    
    正文 年R本政府宣布，位于F岛的核电站，受9级特大地震影响，放射性物质发生泄漏。R本人自食恶果不足惜，却给世界环境...
    
    [![](https:https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](/u/0f438ff0a55f)阅读 40,186评论 3赞 317
    
- [男人毒药：我在死后第九天来索命](/p/c2cfc4cb0aa7)
    
    文/蒙蒙 一、第九天 我趴在偏房一处隐蔽的房顶上张望。 院中可真热闹，春花似锦、人声如沸。这庄子的主人今日做“春日...
    
    [![](https://cdn2.jianshu.io/assets/default_avatar/8-a356878e44b45ab268a3b0bbaaadeeb7.jpg)开封第一讲书人](/u/5891e866c93e)阅读 30,901评论 0赞 21
    
- [一桩弑父案，背后竟有这般阴谋](/p/c329b54bd638)
    
    文/苍兰香墨 我抬头看了看天上的太阳。三九已至，却和暖如春，着一层夹袄步出监牢的瞬间，已是汗流浃背。 一阵脚步声响...
    
    [![](https://cdn2.jianshu.io/assets/default_avatar/8-a356878e44b45ab268a3b0bbaaadeeb7.jpg)开封第一讲书人](/u/5891e866c93e)阅读 32,142评论 1赞 267
    
- [情欲美人皮](/p/d79d2f48417f)
    
    我被黑心中介骗来泰国打工， 没想到刚下飞机就差点儿被人妖公主榨干…… 1. 我叫王不留，地道东北人。 一个月前我还...
    
    [![](https:https://upload.jianshu.io/users/upload_avatars/15878160/783c64db-45e5-48d7-82e4-95736f50533e.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)沈念sama](/u/dcd395522934)阅读 46,689评论 2赞 362
    
- [代替公主和亲](/p/fc890ed5083c)
    
    正文 我出身青楼，却偏偏与公主长得像，于是被迫代替她去往敌国和亲。 传闻我的和亲对象是个残疾皇子，可洞房花烛夜当晚...
    
    [![](https:https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](/u/0f438ff0a55f)阅读 43,757评论 2赞 351
    

### 

全部评论9只看作者

按时间倒序

按时间正序

[![](https:https://upload.jianshu.io/users/upload_avatars/9196938/4cec5108-8676-4abe-825c-3540616dc70d.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/80/h/80/format/webp)](/u/987d16cd1860)

[Allenwang](/u/987d16cd1860)IP属地: 浙江

6楼 2022.10.28 17:21

讲的很清晰，厉害了

赞 回复

[![](https://cdn2.jianshu.io/assets/default_avatar/12-aeeea4bedf10f2a12c0d50d626951489.jpg)](/u/ed7cae1185ec)

[ed7cae1185ec](/u/ed7cae1185ec)IP属地: 浙江

5楼 2022.08.18 19:01

1

赞 回复

[![](https:https://upload.jianshu.io/users/upload_avatars/22926833/3ad80aaa-761c-47fd-980f-3793aac79041.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/80/h/80/format/webp)](/u/44099d5076e2)

[Android图形显示之路](/u/44099d5076e2)IP属地: 安徽

4楼 2022.05.12 18:13

大佬，可以请教下，我在native写，用sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromString("Skia", SkFont(nullptr, 64.0f, 1.0f, 0.0f)); canvas->drawTextBlob(blob.get(), 0, 0, paint); 绘制不出字体，绘制其他图形是可以的，也没报错，不知道为啥？

赞 回复

[![](https:https://upload.jianshu.io/users/upload_avatars/14919101/1b8c2c68-76e0-45ec-a257-4c384d768e61.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/80/h/80/format/webp)](/u/fd0b722ce11f)

[王小二的技术栈](/u/fd0b722ce11f)IP属地: 浙江

3楼 2021.11.08 14:47

有一个问题，请问sf收到vsync信号开始合成到显示到屏幕需要大概多久时间？  
按照下方的描述来看，感觉一个vsync周期就可以从sf收到vsync到显示到屏幕上了。  
  
HWC Service收到合成任务后开始合成数据，在SurfaceFlinger调用presetDisplay时会去调用DRM接口DRMAtomicReq::Commit通知kernel可以向DDIC发送数据了.  
  
如果有TE信号来提示已进入消隐区，这时DRM驱动会马上开始通过DSI总线向DDIC传输数据，与此同时Panel的Disp Scan也在进行中，传输完成后这帧画面就完整地显示到了屏幕上。

赞 回复

[![](https://cdn2.jianshu.io/assets/default_avatar/15-a7ac401939dd4df837e3bbf82abaa2a8.jpg)](/u/24380434ccf6)

[24380434ccf6](/u/24380434ccf6)IP属地: 四川

2022.03.15 22:56

按照我的理解：应用queueBuffer之后的第一个vsync这一帧数据是不会上屏的，因为这一帧里等不到TE信号来提示已进入消隐区了，内核不会通知驱动去传输数据。

回复

[![](https:https://upload.jianshu.io/users/upload_avatars/22926833/3ad80aaa-761c-47fd-980f-3793aac79041.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/80/h/80/format/webp)](/u/44099d5076e2)

[Android图形显示之路](/u/44099d5076e2)IP属地: 安徽

2022.03.24 11:43

sf commit 到hwc后，如无阻塞的情况下，会在下一个te上屏，正常来说app -> sf+hwc -> DDIC 共3个vsync

回复

[![](https:https://upload.jianshu.io/users/upload_avatars/14919101/1b8c2c68-76e0-45ec-a257-4c384d768e61.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/80/h/80/format/webp)](/u/fd0b722ce11f)

[王小二的技术栈](/u/fd0b722ce11f)IP属地: 浙江

2022.03.26 01:02

[@Android曼巴](/u/44099d5076e2) 好的，多谢解答疑惑

回复

添加新评论

[![](https://cdn2.jianshu.io/assets/default_avatar/14-0651acff782e7a18653d7530d6b27661.jpg)](/u/72e948987322)

[Wafer001](/u/72e948987322)IP属地: 广东

2楼 2021.11.03 23:25

牛逼，学习了

赞 回复

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/dcaf1eeddeb1  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。