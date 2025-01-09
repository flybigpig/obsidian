## Android系统Surface机制的SurfaceFlinger服务的启动过程分析

在前面一篇文章中，我们简要介绍了Android系统Surface机制中的SurfaceFlinger服务。SurfaceFlinger服务是在System进程中启动的，并且负责统一管理设备的帧缓冲区。SurfaceFlinger服务在启动的过程中，会创建两个线程，其中一个线程用来监控控制台事件，而另外一个线程用来渲染系统的UI。在本文中，我们就将详细分析SurfaceFlinger服务的启动过程。

从前面[Android系统进程Zygote启动过程的源代码分析](https://www.androidos.net.cn/redirecturl.do?url=http%3A//blog.csdn.net/luoshengyang/article/details/6768304)一文可以知道，System进程是由Zygote进程启动的，并且是以Java层的SystemServer类的静态成员函数main为入口函数的。因此，接下来我们就从SystemServer类的静态成员函数main开始，分析SurfaceFlinger服务的启动过程，如图1所示。

![](https://oss-cn-hangzhou.aliyuncs.com/codingsky/cdn/codingsky/upload/img/blog/0e55300d7a687e03aba84a8661d52fc6.jpg)

图1 SurfaceFlinger服务的启动过程

SurfaceFlinger服务的启动过程可以划分为8个步骤，接下来我们就详细分析每一个步骤。

Step 1. SystemServer.main

```
public class SystemServer
    {
        ......

        native public static void init1(String[] args);

        public static void main(String[] args) {
            ......

            System.loadLibrary("android_servers");
            init1(args);
        }

        ......
    }
```

这个函数定义在文件frameworks/base/services/java/com/android/server/SystemServer.java中。

SystemServer类的静态成员函数main首先将android\_servers库加载到System进程中来，接着调用另外一个静态成员函数init1来启动那些使用C++语言来实现的系统服务。

SystemServer类的静态成员函数init1是一个JNI方法，它是由C++层的函数android\_server\_SystemServer\_init1来实现的，接下来我们就继续分析它的实现。

Step 2. SystemServer.init1

```
static void android_server_SystemServer_init1(JNIEnv* env, jobject clazz)
    {
        system_init();
    }
```

这个函数定义在文件frameworks/base/services/jni/com\_android\_server\_SystemServer.cpp 中。

SystemServer类的静态成员函数init1调用另外一个函数system\_init来启动那些使用C++语言来实现的系统服务，它的实现在文件frameworks/base/cmds/system\_server/library/system\_init.cpp中，如下所示：

```
extern "C" status_t system_init()
    {
        LOGI("Entered system_init()");

        sp<ProcessState> proc(ProcessState::self());
        ......

        char propBuf[PROPERTY_VALUE_MAX];
        property_get("system_init.startsurfaceflinger", propBuf, "1");
        if (strcmp(propBuf, "1") == 0) {
            // Start the SurfaceFlinger
            SurfaceFlinger::instantiate();
        }

        ......

        if (proc->supportsProcesses()) {
            LOGI("System server: entering thread pool.\n");
            ProcessState::self()->startThreadPool();
            IPCThreadState::self()->joinThreadPool();
            LOGI("System server: exiting thread pool.\n");
        }
        return NO_ERROR;
    }
```

函数首先获得System进程中的一个ProcessState单例，并且保存在变量proc中，后面会通过调用它的成员函数supportsProcesses来判断系统是否支持Binder进程间通信机制。我们知道，在Android系统中，每一个需要使用Binder进程间通信机制的进程内部都有一个ProcessState单例，它是用来和Binder驱动程序建立连接的，具体可以参考[Android系统进程间通信（IPC）机制Binder中的Server启动过程源代码分析](https://www.androidos.net.cn/redirecturl.do?url=http%3A//blog.csdn.net/luoshengyang/article/details/6627260)一文。

函数接下来就检查系统中是否存在一个名称为"system\_init.startsurfaceflinger"的属性。如果存在的话，就将它的值获取回来，并且保存在缓冲区proBuf中。如果不存在的话，那么函数property\_get就会将缓冲区proBuf的值设置为"1"。当缓冲区proBuf的值等于"1"的时候，就表示需要在System进程中将SurfaceFlinger服务启动起来，这是通过调用SurfaceFlinger类的静态成员函数instantiate来实现的。

函数最后检查系统是否支持Binder进程间通信机制。如果支持的话，那么接下来就会调用当前进程中的ProcessState单例的成员函数startThreadPool来启动一个Binder线程池，并且调用当前线程中的IPCThreadState单例来将当前线程加入到前面所启动的Binder线程池中去。从前面[Android系统进程Zygote启动过程的源代码分析](https://www.androidos.net.cn/redirecturl.do?url=http%3A//blog.csdn.net/luoshengyang/article/details/6768304)和[Android应用程序进程启动过程的源代码分析](https://www.androidos.net.cn/redirecturl.do?url=http%3A//blog.csdn.net/luoshengyang/article/details/6747696)两篇文章可以知道，System进程前面在初始化运行时库的过程中，已经调用过当前进程中的ProcessState单例的成员函数startThreadPool来启动Binder线程池了，因此，这里其实只是将当前线程加入到这个Binder线程池中去。有了这个Binder线程池之后，SurfaceFlinger服务在启动完成之后，就可以为系统中的其他组件或者进程提供服务了。

假设系统存在一个名称为"system\_init.startsurfaceflinger"的属性，并且它的值等于"1"，接下来我们就继续分析SurfaceFlinger类的静态成员函数instantiate的实现，以便可以了解SurfaceFlinger服务的启动过程。由于SurfaceFlinger类的静态成员函数instantiate是从父类BinderService继承下来的，因此，接下来我们要分析的实际上是BinderService类的静态成员函数instantiate的实现。

Step 3. BinderService.instantiate

```
template<typename SERVICE>
    class BinderService
    {
    public:
        ......

        static void instantiate() { publish(); }

        ......
    };
```

这个函数定义在文件frameworks/base/include/binder/BinderService.h中。

BinderService类的静态成员函数instantiate的实现很简单，它只是调用BinderService类的另外一个静态成员函数publish来继续执行启动SurfaceFlinger服务的操作。

Step 4. BinderService.publish

```
template<typename SERVICE>
    class BinderService
    {
    public:
        static status_t publish() {
            sp<IServiceManager> sm(defaultServiceManager());
            return sm->addService(String16(SERVICE::getServiceName()), new SERVICE());
        }

        ......
    };
```

这个函数定义在文件frameworks/base/include/binder/BinderService.h中。

BinderService是一个模板类，它有一个模板参数SERVICE。当BinderService类被SurfaceFlinger类继承时，模板参数SERVICE的值就等于SurfaceFlinger。因此，BinderService类的静态成员函数publish所执行的操作就是创建一个SurfaceFlinger实例，用来作为系统的SurfaceFlinger服务，并且将这个服务注册到Service Manager中去，这样系统中的其它组件或者进程就可以通过Service Manager来获得SurfaceFlinger服务的Binder代理对象，进而使用它所提供的服务。Binder进程间通信机制中的服务对象的注册过程可以参考[Android系统进程间通信（IPC）机制Binder中的Server启动过程源代码分析](https://www.androidos.net.cn/redirecturl.do?url=http%3A//blog.csdn.net/luoshengyang/article/details/6629298)一文。

接下来，我们就继续分析SurfaceFlinger服务的创建过程。

Step 5. new SurfaceFlinger

```
SurfaceFlinger::SurfaceFlinger()
        :   BnSurfaceComposer(), Thread(false),
            ......
    {
        init();
    }
```

这个函数定义在文件frameworks/base/services/surfaceflinger/SurfaceFlinger.cpp中。

从前面[Android系统Surface机制的SurfaceFlinger服务简要介绍和学习计划](https://www.androidos.net.cn/redirecturl.do?url=http%3A//blog.csdn.net/luoshengyang/article/details/8010977)一文可以知道，SurfaceFlinger类继承了BnSurfaceComposer类，而后者是一个实现了ISurfaceComposer接口的Binder本地对象类。此外，SurfaceFlinger类还继承了Thread类，后者是用来创建一个线程的，这个线程就是我们在[Android系统Surface机制的SurfaceFlinger服务简要介绍和学习计划](https://www.androidos.net.cn/redirecturl.do?url=http%3A//blog.csdn.net/luoshengyang/article/details/8010977)一文中提到的UI渲染线程，它的线程执行体函数为SurfaceFlinger类的成员函数threadLoop。后面在分析SurfaceFlinger服务渲染UI的过程时，我们再分析SurfaceFlinger类的成员函数threadLoop的实现。注意，在初始化SurfaceFlinger的父类Thread时，传进去的参数为false，表示先不要将SurfaceFlinger服务的UI渲染线程启动起来，等到后面再启动。

SurfaceFlinger服务在创建的过程中，会调用SurfaceFlinger类的成员函数init来执行初始化的操作，接下来，我们就继续分析它的实现。

Step 6. SurfaceFlinger.init

```
void SurfaceFlinger::init()
    {
        LOGI("SurfaceFlinger is starting");

        // debugging stuff...
        char value[PROPERTY_VALUE_MAX];
        property_get("debug.sf.showupdates", value, "0");
        mDebugRegion = atoi(value);
        property_get("debug.sf.showbackground", value, "0");
        mDebugBackground = atoi(value);

        LOGI_IF(mDebugRegion,       "showupdates enabled");
        LOGI_IF(mDebugBackground,   "showbackground enabled");
    }
```

这个函数定义在文件frameworks/base/services/surfaceflinger/SurfaceFlinger.cpp中。

SurfaceFlinger类的成员函数init的实现很简单，它分别获得系统中两个名称为"debug.sf.showupdates"和"debug.sf.showbackground"的属性的值，并且分别保存在SurfaceFlinger类的成员变量mDebugRegion和mDebugBackground中。这两个成员变量是与调试相关的，我们不关心。

这一步执行完成之后，返回到前面的Step 4中，即BinderService类的静态成员函数publish中，这时候在前面的Step 5中所创建的一个SurfaceFlinger实例就会被注册到Service Manager中，这是通过调用Service Manager的Binder代理对象的成员函数addService来实现的。由于Service Manager的Binder代理对象的成员函数addService的第二个参数是一个类型为IBinder的强指针引用。从前面[Android系统的智能指针（轻量级指针、强指针和弱指针）的实现原理分析](https://www.androidos.net.cn/redirecturl.do?url=http%3A//blog.csdn.net/luoshengyang/article/details/6786239)一文可以知道，当一个对象第一次被一个强指针引用时，那么这个对象的成员函数onFirstRef就会被调用。因此，接下来前面所创建的SurfaceFlinger实例的成员函数onFirstRef就会被调用，以便可以继续执行初始化操作。

Step 7. SurfaceFlinger.onFirstRef

```
void SurfaceFlinger::onFirstRef()
    {
        run("SurfaceFlinger", PRIORITY_URGENT_DISPLAY);

        // Wait for the main thread to be done with its initialization
        mReadyToRunBarrier.wait();
    }
```

这个函数定义在文件frameworks/base/services/surfaceflinger/SurfaceFlinger.cpp中。

函数首先调用从父类继承下来的成员函数run来启动一个名秒为"SurfaceFlinger"的线程，用来执行UI渲染操作。这就是前面我们所说的UI渲染线程了。这个UI渲染线程创建完成之后，首先会调用SurfaceFlinger类的成员函数readyToRun来执行一些初始化操作，接着再循环调用SurfaceFlinger类的成员函数threadLoop来作为线程的执行体。

mReadyToRunBarrier是SurfaceFlinger类的一个成员变量，它的类型是Barrier，用来描述一个屏障，是通过条件变量来实现的。我们可以把它看作是一个线程同步工具，即阻塞当前线程，直到SurfaceFlinger服务的UI渲染线程执行完成初始化操作为止。

接下来，我们就继续分析SurfaceFlinger类的成员函数readyToRun的实现，以便可以了解SurfaceFlinger服务的UI渲染线程的初始化过程。

Step 8. SurfaceFlinger.oreadyToRun

这个函数定义在文件frameworks/base/services/surfaceflinger/SurfaceFlinger.cpp文件中，用来初始化SurfaceFlinger服务的UI渲染线程，我们分段来阅读：

```
status_t SurfaceFlinger::readyToRun()
    {
        LOGI(   "SurfaceFlinger's main thread ready to run. "
                "Initializing graphics H/W...");

        // we only support one display currently
        int dpy = 0;

        {
            // initialize the main display
            GraphicPlane& plane(graphicPlane(dpy));
            DisplayHardware* const hw = new DisplayHardware(this, dpy);
            plane.setDisplayHardware(hw);
        }
```

这段代码首先创建了一个DisplayHardware对象hw，用来描述设备的显示屏，并且用这个DisplayHardware对象来初始化SurfaceFlinger类的成员变量mGraphicPlanes所描述的一个GraphicPlane数组的第一个元素。在DisplayHardware对象hw的创建过程中，会创建另外一个线程，用来监控控制台事件，即监控硬件帧缓冲区的睡眠和唤醒事件。在后面一篇文章中介绍SurfaceFlinger服务是如何管理硬件帧缓冲区时，我们就会看到这个控制台事件监控线程的创建过程。

我们接着往下阅读代码：

```
    // create the shared control-block
        mServerHeap = new MemoryHeapBase(4096,
                MemoryHeapBase::READ_ONLY, "SurfaceFlinger read-only heap");
        LOGE_IF(mServerHeap==0, "can't create shared memory dealer");

        mServerCblk = static_cast<surface_flinger_cblk_t*>(mServerHeap->getBase());
        LOGE_IF(mServerCblk==0, "can't get to shared control block's address");

        new(mServerCblk) surface_flinger_cblk_t;
```

这段代码首先创建了一块大小为4096，即4KB的匿名共享内存，接着将这块匿名共享内存结构化为一个surface\_flinger\_cblk\_t对象来访问。这个surface\_flinger\_cblk\_t对象就保存在SurfaceFlinger类的成员变量mServerCblk中。

这块匿名共享内存用来保存设备显示屏的属性信息，例如，宽度、高度、密度和每秒多少帧等信息，后面我们就会看到这块匿名共享内存的初始化过程。为什么会使用匿名共享内存来保存设备显示屏的属性信息呢？这是为了方便将这些信息传递给系统中的其它进程访问的。系统中的其它进程可以通过调用调用SurfaceFlinger服务的代理对象的成员函数getCblk来获得这块匿名共享内存的内容。

我们再接着往下阅读代码：

```
    // initialize primary screen
        // (other display should be initialized in the same manner, but
        // asynchronously, as they could come and go. None of this is supported
        // yet).
        const GraphicPlane& plane(graphicPlane(dpy));
        const DisplayHardware& hw = plane.displayHardware();
        const uint32_t w = hw.getWidth();
        const uint32_t h = hw.getHeight();
        const uint32_t f = hw.getFormat();
        hw.makeCurrent();
```

这段代码首先获得SurfaceFlinger类的成员变量mGraphicPlanes所描述的一个GraphicPlane数组的第一个元素plane，接着再设置它的宽度、长度和像素格式等作息，最后再调用它里面的一个DisplayHardware对象hw的成员函数makeCurrent来将它作为系统的主显示屏。这个DisplayHardware对象hw是在前面第一段代码中创建的，在创建的过程中，它会执行一些初始化操作，这里将它设置为系统主显示屏之后，后面就可以将系统的UI渲染在它上面了。在后面一篇文章中介绍SurfaceFlinger服务是如何管理硬件帧缓冲区时，我们再分析DisplayHardware类的成员函数makeCurrent的实现。

我们继续往下阅读代码：

```
    // initialize the shared control block
        mServerCblk->connected |= 1<<dpy;
        display_cblk_t* dcblk = mServerCblk->displays + dpy;
        memset(dcblk, 0, sizeof(display_cblk_t));
        dcblk->w            = plane.getWidth();
        dcblk->h            = plane.getHeight();
        dcblk->format       = f;
        dcblk->orientation  = ISurfaceComposer::eOrientationDefault;
        dcblk->xdpi         = hw.getDpiX();
        dcblk->ydpi         = hw.getDpiY();
        dcblk->fps          = hw.getRefreshRate();
        dcblk->density      = hw.getDensity();
```

这段代码将系统主显示屏的属性信息保存在前面所创建的一块匿名共享内存中，以便可以将系统主显示屏的属性信息返回给系统中的其它进程访问。

我们再继续往下阅读代码：

```
    // Initialize OpenGL|ES
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnable(GL_SCISSOR_TEST);
        glShadeModel(GL_FLAT);
        glDisable(GL_DITHER);
        glDisable(GL_CULL_FACE);

        const uint16_t g0 = pack565(0x0F,0x1F,0x0F);
        const uint16_t g1 = pack565(0x17,0x2f,0x17);
        const uint16_t textureData[4] = { g0, g1, g1, g0 };
        glGenTextures(1, &mWormholeTexName);
        glBindTexture(GL_TEXTURE_2D, mWormholeTexName);
        glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0,
                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, textureData);

        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrthof(0, w, h, 0, 0, 1);
```

这段代码用来初始化OpenGL库，因为SurfaceFlinger服务是通过OpenGL库提供的API来渲染系统的UI的。这里我们就不详细分析OpenGL库的初始化过程中，有兴趣的读者可以参考官方网站：[http://cn.khronos.org/](https://www.androidos.net.cn/redirecturl.do?url=http%3A//cn.khronos.org/)。

我们再继续往下阅读最后一段代码：

```
    LayerDim::initDimmer(this, w, h);

        mReadyToRunBarrier.open();

        /*
         *  We're now ready to accept clients...
         */

        // start boot animation
        property_set("ctl.start", "bootanim");

        return NO_ERROR;
    }
```

这段代码做了三件事情。

第一件事情是调用LayerDim类的静态成员函数initDimmer来初始化LayerDim类。LayerDim类是用来描述一个具有颜色渐变功能的Surface的，这种类型的Surface与普通的Surface不一样，前者是在后者的基础上创建和渲染的。

第二件事情是调用SurfaceFlinger类的成员变量mReadyToRunBarrier所描述的一个屏障的成员函数open来告诉System进程的主线程，即在前面的Step 7中正在等待的线程，SurfaceFlinger服务的UI渲染线程已经创建并且初始化完成了，这时候System进程的主线程就可以继续向前执行其它操作了。

第三件事情是调用函数property\_set来设置系统中名称为"ctl.start"的属性，即将它的值设置为"bootanim"。从前面[Android系统的开机画面显示过程分析](https://www.androidos.net.cn/redirecturl.do?url=http%3A//blog.csdn.net/luoshengyang/article/details/7691321)一文可以知道，ctl.start是Android系统的一个控制属性，当它的值等于""bootanim"的时候，就表示要启动Android系统的开机动画。从这里就可以看出，当我们看到Android系统的开机动画时，就说明Android系统的SurfaceFlinger服务已经启动起来了。

至此，我们就分析完成SurfaceFlinger服务的启动过程中了。在分析过程中，有两个比较重要的知识点：第一个知识点是系统主显示屏的创建和初始化过程，第二个知识点是UI渲染线程的执行过程。在接下来的第一篇文章中，我们将详细分析第一个知识点。在分析第一个知识点的过程中，会涉及到SurfaceFlinger服务的控制台事件监控线程的创建过程，因此，结合Step 2提到的Binder线程，以及Step 7提到的UI渲染线程，我们将在接下来的第二篇文章中，综合描述SurfaceFlinger服务的线程协作模型。有了前面的基础知识之后，在接下来的第三篇文章中，我们就将详细分析第二个知识点。敬请关注！