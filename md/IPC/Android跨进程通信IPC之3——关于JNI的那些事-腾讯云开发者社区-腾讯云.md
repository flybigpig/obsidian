在分析IPC基于Android 6.0)的过程中，里面的核心部分是Native的，并且还包含一些linux kernel，而作为Android开发者看到的代码大部分都是Java层，所以这就一定会存在Java与C/C++代码的来回跳转，那么久很有必要来先说一下JNI，本文主要内容如下：

-   1、相关代码
-   2、JNI简介
-   3、Android中应用程序框架
-   4、JNI查找方式
-   5、loadLibrary源码分析
-   6、JNI资源
-   7、总结

### 一、相关代码

##### (一)、代码位置

```
frameworks/base/core/jni/AndroidRuntime.cpp

libcore/luni/src/main/java/java/lang/System.java
libcore/luni/src/main/java/java/lang/Runtime.java
libnativehelper/JNIHelp.cpp
libnativehelper/include/nativehelper/jni.h

frameworks/base/core/java/android/os/MessageQueue.java
frameworks/base/core/jni/android_os_MessageQueue.cpp

frameworks/base/core/java/android/os/Binder.java
frameworks/base/core/jni/android_util_Binder.cpp

frameworks/base/media/java/android/media/MediaPlayer.java
frameworks/base/media/jni/android_media_MediaPlayer.cpp
```

##### (二)、代码链接

-   [AndroidRuntime.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2FAndroidRuntime.cpp&objectId=1199091&objectType=1&isNewArticle=undefined)
-   [System.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Flibcore%2Fluni%2Fsrc%2Fmain%2Fjava%2Fjava%2Flang%2FSystem.java&objectId=1199091&objectType=1&isNewArticle=undefined)
-   [Runtime.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Flibcore%2Fluni%2Fsrc%2Fmain%2Fjava%2Fjava%2Flang%2FRuntime.java&objectId=1199091&objectType=1&isNewArticle=undefined)
-   [JNIHelp.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Flibnativehelper%2FJNIHelp.cpp&objectId=1199091&objectType=1&isNewArticle=undefined)
-   [jni.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Flibnativehelper%2Finclude%2Fnativehelper%2Fjni.h&objectId=1199091&objectType=1&isNewArticle=undefined)
-   [MessageQueue.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FMessageQueue.java&objectId=1199091&objectType=1&isNewArticle=undefined)
-   [android\_os\_MessageQueue.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_os_MessageQueue.cpp&objectId=1199091&objectType=1&isNewArticle=undefined)
-   [Binder.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjava%2Fandroid%2Fos%2FBinder.java&objectId=1199091&objectType=1&isNewArticle=undefined)
-   [android\_util\_Binder.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_util_Binder.cpp&objectId=1199091&objectType=1&isNewArticle=undefined)
-   [MediaPlayer.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fmedia%2Fjava%2Fandroid%2Fmedia%2FMediaPlayer.java&objectId=1199091&objectType=1&isNewArticle=undefined)
-   [android\_media\_MediaPlayer.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fmedia%2Fjni%2Fandroid_media_MediaPlayer.cpp&objectId=1199091&objectType=1&isNewArticle=undefined)

### 二、JNI简介

##### (一)、JNI介绍

JNI(Java Native Interface，Java本地接口)，用于打通Java层与Native(C/C++)层。这不是Android系统独有的，而是Java所有。众所周知，Java语言是是跨平台的语言，而这跨平台的背后都是一开Java虚拟机，虚拟机采用C/C++编写，适配各个系统，通过JNI为上层Java提供各种服务，保证跨平台性。

其实不少的Java的程序员，享受着其跨平台性，可能全然不知JNI的存在。在Android平台，让JNI大放异彩，为更多的程序员所数值，往往为了提供效率或者其他功能需求，就需要在NDK上开发。本文的主要目的是介绍android上层中Java与Native的纽带JNI。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/buzn91ejwl.png)

JNI.png

##### (二)、Java/JNI/C的关系

###### 1、C与Java的侧重

-   C语言：C语言中重要的是函数 fuction
-   Java语言：Java中最重要的是JVM，class类，以及class中的方法

###### 2、C与Java的"面向"

-   C语言：是面向函数的语言
-   Java语言：是面向对象的语言

###### 3、C与Java如何交流

-   JNI规范：C语言与Java语言交流需要一个适配器，[中间件](https://cloud.tencent.com/product/message-queue-catalog?from_column=20065&from=20065)，即JNI，JNI提供一共规范
-   C语言中调用Java的方法：可以让我们在C代码中找到Java代码class的方法，并且调用该方法
-   Java语言中调用C语言方法：同时也可以在Java代码中，将一个C语言的方法映射到Java的某个方法上
-   JNI的桥梁作用：JNI提供了一个桥梁，打通了C语言和Java语言的之间的障碍。

###### 4、JNI中的一些概念

-   natvie：Java语言中修饰本地方法的修饰符(也可以理解为关键字)，被该修饰符修饰的方法没有方法体
-   Native方法：在Java语言中被native关键字修饰的方法是Native方法
-   JNI层：Java声明Native方法的部分
-   JNI函数：JNIEnv提供的函数，这些函数在jni.h中进行定义
-   JNI方法：Native方法对应JNI实现的C/C++方法，即在jni目录中实现的那些C语言代码

###### 5、JNI接口函数和指针

平台相关代码是通过调用JNI函数来访问Java虚拟机功能的。JNI函数可以通过接口指针来获得。接口指针是指针的指针，它指向一个指针数组，而指针数组中的每个元素又指向一个接口函数。每个接口函数都处在数组的某个预定偏移量中。下图说明了接口指针的组织结构。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/st16gd7gqh.png)

接口指针.png

JNI接口的组织类似于C++虚拟函数表或COM接口。使用接口表而不实用硬性编入的函数表的好处是使JNI名字空间与平台代码分开。虚拟机可以很容易地提供了多个版本的JNI寒暑表。例如，虚拟机 可以支持以下两个JNI函数表：

-   一个表对非法参数进行全面检查，适用于调试程序
-   另一个表只进行JNI规范所要求的最小程度的检查，因此效率较高。

JNI接口指针只在当前线程中有效。因此，本地方法不能讲接口指针从一个线程传递到另一个线程中。实现JNI虚拟机可能将本地线程的数据分配和储存在JNI接口指针所指向区域中。本地方法将JNI接口指针当做参数来接受。虚拟机在从相同的Java线程中对本地方法进行多次调用时，保证传递给本地方法的接口指针是相同的。但是，一个本地方方可以被不同的Java线程所调用，因此可以接受不同的JNI接口指针。

本地方法将JNI接口指针当参数来接受。虚拟机在从相同的Java线程对本地方法进行多次调用时，保证传递给本地方法的接口指针是相同的。但是，一个本地方法可被不同的Java线程所调用，因此可以接受不同的JNI接口指针。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/885yfpuwh1.png)

image.png

###### 6、JavaVM和JNIEnv

####### (1)、JavaVM

> 代表Java虚拟机。所有的工作都是从获取虚拟机接口开始的。有两种方式：第一种方式，在加载动态链接库时，JVM会调用JNI\_OnLoad(JavaVM \* jvm, void \* reserved)(如果定了该函数)。第一个参数会传入JavaVM指针；第二种方式，在native\_code中调用JNI\_CreateJavaVM(&jvm,(void\*)&env,&vm\_args) 可以得到JavaVM指针。两种方式都可以用全局变量，比如JavaVM \* g\_jvm来保存获取到的指针以便在任意上下文中使用。Android系统是利用第二种方式Invocation interface来创建JVM

####### (2)、JNIEnv

> JNIEnv，即JNIEnvironment；字面意思就是JNI环境。其实它是一个与线程相关的JNI环境结构体。所以JNIEnv类型实际代表了Java环境。通过这个JNIEnv\*指针，就可以对Java端代码进行操作。与线程相关，不同线程的JNIEnv相互独立。 JNIEnv只在当前线程中有效。Native方法不能将JNIEnv从一个线程传递到另一个线程中。相同的Java线程对Native方法多次调用时，传递给Native方法的JNIEnv是相同的。但是，一个本地方法可能会被不同的Java线程调用，因此可以接受不同的JNIEnv。

和JNIEnv相比，JavaVM可以在进程中各个线程间共享。理论上一个进程可以有多个JavaVM，但Android只允许一个JavaVM。需要强调在Android SDK中强调了额 **" do not cache JNIEnv \* "**，要用的时候在不同的线程中通过JavaVM \* jvm的方法来获取与当前线程相关的JNIEnv \*。

> 在Java里，每一个一个process可以产生多个JavaVM对象，但是在android上，每一个process只有一个Dalvik虚拟机对象，也就是在android进程中是通过有且只有一个虚拟机对象来服务所有Java和C/C++代码。Java的dex字节码和C/C++的 xxx.so 同时运行Dalvik虚拟机之内，共同使用一个进程空间。之所以可以相互调用，也是因为有Dalvik虚拟机。当Java代码需要C/C++代码时，Dalvik虚拟机加载xxx.so库时，会先调用JNI\_Onload()，此时会把Java对象的指针存储于C层JNI组件的全局环境中，在Java层调用C层的Native函数时，调用Native函数线程必然通过Dalvik虚拟机来调用C层的Native函数。此时，虚拟机会为Native的C组件是实例化一个JNIEnv指针，该指针指向Dalvik虚拟机的具体函数列表，当JNI的C组件调用Java层的方法或属性时，需要JNIEnv指针来进行调用。当本地C/C++想获的当前线程所要使用的JNIEnv时，可以使用Dalvik虚拟机对象的JavaVM \* jvm—>GetEnv()返回当前线程所在的JNIEnv\*。

### 三、Android中应用程序框架

##### (一)、正常情况下的Android框架

最顶层是\*\* Android应用程序代码 \*\*，上层的 \*\* 应用层 \*\* 和 \*\* 应用框架层 \*\* 主要是Java代码，中间有一层的 \*\* Framework框架层代码 \*\* 是C/C++代码，通过Framework进行系统调用，调用底层的库和 Linux内核

![](https://ask.qcloudimg.com/http-save/yehe-2957818/zcpjvl44p5.png)

正常情况下的Android框架

##### (二)、使用JNI的Android框架

使用JNI时的Android框架：绕过Framework提供的底层代码，直接调用自己的写的C代码，该代码最终会编译成一个库，这个库通过JNI提供的一个Stable的ABI 调用Linux kernel，ABI是二进制程序接口 application binary interface

![](https://ask.qcloudimg.com/http-save/yehe-2957818/88yefpi2r1.png)

使用JNI的Android框架.png

##### (三)、Android框架中的JNI

###### 1、纽带

JNI是连接框架层(Framework - C/C++) 和应用框架层(Application Framework - Java )的纽带

###### 2、JNI在Android中的作用

JNI可以调用本地代码库(即C/C++代码)，并通过Dalvik虚拟机与应用层和应用框架层进行交互，Android 中的JNI主要位于应用层和应用框架层之间

-   应用层：该层是由JNI开发，主要使用标准的JNI编程模型
-   应用框架层： 使用的是Android中自定义的一套JNI编程模型，该自定义的JNI编程模型弥补了标准的JNI编程模型的不足

###### 3、NDK与JNI区别：

-   NDK：NDK是Google开发的一套开发和编译工具集，主要用于AndroidJNI开发
-   JNI：JNI十套编程接口，用来实现Java代码与本地C/C++代码进行交互

### 四、JNI查找方式

Android系统在启动的过程中，先启动Kernel创建init进程，紧接着由init进程fork第一个横穿Java和C/C++的进程，即Zygote进程。Zygote启动过程会在 **AndroidRuntime.cpp** 中的startVM创建虚拟机，VM创建完成后，紧接着调用 **startReg** 完成虚拟机中的JNI方法注册。

##### (一)、startReg

```
// frameworks/base/core/jni/AndroidRuntime.cpp      1440行
/*
 * Register android native functions with the VM.
 * 在虚拟机上注册Android的native方法
 */
 int AndroidRuntime::startReg(JNIEnv*env) {
    /*
     * This hook causes all future threads created in this process to be
     * attached to the JavaVM.  (This needs to go away in favor of JNI
     * Attach calls.)
     * 此钩子将导致在此过程中创建的所有未来线程 附加到JavaVM。 （这需要消除对
     * JNI的支持附加调用。）
     * 说白了就是设置线程的创建方法为 javaCreateThreadEtc
     */
        androidSetCreateThreadFunc((android_create_thread_fn) javaCreateThreadEtc);
        ALOGV("--- registering native functions ---\n");
    /*
     * Every "register" function calls one or more things that return
     * a local reference (e.g. FindClass).  Because we haven't really
     * started the VM yet, they're all getting stored in the base frame
     * and never released.  Use Push/Pop to manage the storage.
     * 每个“注册”函数调用一个或多个返回的东西本地引用（例如FindClass）。
     * 因为我们没有真的启动虚拟机，它们都被存储在基本框架中并没有发布。 
     * 使用Push / Pop管理存储。
     */
        env -> PushLocalFrame(200);
        // 进程JNI注册函数
        if (register_jni_procs(gRegJNI, NELEM(gRegJNI), env) < 0) {
            env -> PopLocalFrame(NULL);
            return -1;
        }
        env -> PopLocalFrame(NULL);
        //createJavaThread("fubar", quickTest, (void*) "hello");
        return 0;
       }
```

startReg()函数里面调用了register\_jni\_procs()函数，这个函数是真正的注册，那我们就来看下这个register\_jni\_procs()函数

###### 1、register\_jni\_procs()

```
// frameworks/base/core/jni/AndroidRuntime.cpp      1283行
    static int register_jni_procs(const RegJNIRec array[], size_t count, JNIEnv*env) {
        for (size_t i = 0; i < count; i++) {
            if (array[i].mProc(env) < 0) {
            #ifndef NDEBUG
                ALOGD("----------!!! %s failed to load\n", array[i].mName)
            #endif
                return -1;
            }
        }
        return 0;
    }
```

> 发现上面的代码很简单，register\_jni\_procs(gRegJNI, NELEM(gRegJNI), env)函数的的作用就是循环调用gRegJNI数组成员所对应的方法

上面提到了gRegJNI数组，gRegJNI是什么，我们一起来看下

###### 2、gRegJNI数组

```
// frameworks/base/core/jni/AndroidRuntime.cpp      1296行
static const RegJNIRec gRegJNI[] = {
   REG_JNI(register_com_android_internal_os_RuntimeInit),
    REG_JNI(register_android_os_SystemClock),
    REG_JNI(register_android_util_EventLog),
    REG_JNI(register_android_util_Log),
    REG_JNI(register_android_content_AssetManager),
    REG_JNI(register_android_content_StringBlock),
    REG_JNI(register_android_content_XmlBlock),
    REG_JNI(register_android_emoji_EmojiFactory),
    REG_JNI(register_android_text_AndroidCharacter),
    REG_JNI(register_android_text_StaticLayout),
    REG_JNI(register_android_text_AndroidBidi),
    REG_JNI(register_android_view_InputDevice),
    REG_JNI(register_android_view_KeyCharacterMap),
    REG_JNI(register_android_os_Process),
    REG_JNI(register_android_os_SystemProperties),
    REG_JNI(register_android_os_Binder),
    REG_JNI(register_android_os_Parcel),
    REG_JNI(register_android_nio_utils),
    REG_JNI(register_android_graphics_Graphics),
    REG_JNI(register_android_view_DisplayEventReceiver),
    REG_JNI(register_android_view_RenderNode),
    REG_JNI(register_android_view_RenderNodeAnimator),
    REG_JNI(register_android_view_GraphicBuffer),
    REG_JNI(register_android_view_DisplayListCanvas),
    REG_JNI(register_android_view_HardwareLayer),
    REG_JNI(register_android_view_ThreadedRenderer),
    REG_JNI(register_android_view_Surface),
    REG_JNI(register_android_view_SurfaceControl),
    REG_JNI(register_android_view_SurfaceSession),
    REG_JNI(register_android_view_TextureView),
    REG_JNI(register_com_android_internal_view_animation_NativeInterpolatorFactoryHelper),
    REG_JNI(register_com_google_android_gles_jni_EGLImpl),
    REG_JNI(register_com_google_android_gles_jni_GLImpl),
    REG_JNI(register_android_opengl_jni_EGL14),
    REG_JNI(register_android_opengl_jni_EGLExt),
    REG_JNI(register_android_opengl_jni_GLES10),
    REG_JNI(register_android_opengl_jni_GLES10Ext),
    REG_JNI(register_android_opengl_jni_GLES11),
    REG_JNI(register_android_opengl_jni_GLES11Ext),
    REG_JNI(register_android_opengl_jni_GLES20),
    REG_JNI(register_android_opengl_jni_GLES30),
    REG_JNI(register_android_opengl_jni_GLES31),
    REG_JNI(register_android_opengl_jni_GLES31Ext),

    REG_JNI(register_android_graphics_Bitmap),
    REG_JNI(register_android_graphics_BitmapFactory),
    REG_JNI(register_android_graphics_BitmapRegionDecoder),
    REG_JNI(register_android_graphics_Camera),
    REG_JNI(register_android_graphics_CreateJavaOutputStreamAdaptor),
    REG_JNI(register_android_graphics_Canvas),
    REG_JNI(register_android_graphics_CanvasProperty),
    REG_JNI(register_android_graphics_ColorFilter),
    REG_JNI(register_android_graphics_DrawFilter),
    REG_JNI(register_android_graphics_FontFamily),
    REG_JNI(register_android_graphics_Interpolator),
    REG_JNI(register_android_graphics_LayerRasterizer),
    REG_JNI(register_android_graphics_MaskFilter),
    REG_JNI(register_android_graphics_Matrix),
    REG_JNI(register_android_graphics_Movie),
    REG_JNI(register_android_graphics_NinePatch),
    REG_JNI(register_android_graphics_Paint),
    REG_JNI(register_android_graphics_Path),
    REG_JNI(register_android_graphics_PathMeasure),
    REG_JNI(register_android_graphics_PathEffect),
    REG_JNI(register_android_graphics_Picture),
    REG_JNI(register_android_graphics_PorterDuff),
    REG_JNI(register_android_graphics_Rasterizer),
    REG_JNI(register_android_graphics_Region),
    REG_JNI(register_android_graphics_Shader),
    REG_JNI(register_android_graphics_SurfaceTexture),
    REG_JNI(register_android_graphics_Typeface),
    REG_JNI(register_android_graphics_Xfermode),
    REG_JNI(register_android_graphics_YuvImage),
    REG_JNI(register_android_graphics_pdf_PdfDocument),
    REG_JNI(register_android_graphics_pdf_PdfEditor),
    REG_JNI(register_android_graphics_pdf_PdfRenderer),

    REG_JNI(register_android_database_CursorWindow),
    REG_JNI(register_android_database_SQLiteConnection),
    REG_JNI(register_android_database_SQLiteGlobal),
    REG_JNI(register_android_database_SQLiteDebug),
    REG_JNI(register_android_os_Debug),
    REG_JNI(register_android_os_FileObserver),
    REG_JNI(register_android_os_MessageQueue),
    REG_JNI(register_android_os_SELinux),
    REG_JNI(register_android_os_Trace),
    REG_JNI(register_android_os_UEventObserver),
    REG_JNI(register_android_net_LocalSocketImpl),
    REG_JNI(register_android_net_NetworkUtils),
    REG_JNI(register_android_net_TrafficStats),
    REG_JNI(register_android_os_MemoryFile),
    REG_JNI(register_com_android_internal_os_Zygote),
    REG_JNI(register_com_android_internal_util_VirtualRefBasePtr),
    REG_JNI(register_android_hardware_Camera),
    REG_JNI(register_android_hardware_camera2_CameraMetadata),
    REG_JNI(register_android_hardware_camera2_legacy_LegacyCameraDevice),
    REG_JNI(register_android_hardware_camera2_legacy_PerfMeasurement),
    REG_JNI(register_android_hardware_camera2_DngCreator),
    REG_JNI(register_android_hardware_Radio),
    REG_JNI(register_android_hardware_SensorManager),
    REG_JNI(register_android_hardware_SerialPort),
    REG_JNI(register_android_hardware_SoundTrigger),
    REG_JNI(register_android_hardware_UsbDevice),
    REG_JNI(register_android_hardware_UsbDeviceConnection),
    REG_JNI(register_android_hardware_UsbRequest),
    REG_JNI(register_android_hardware_location_ActivityRecognitionHardware),
    REG_JNI(register_android_media_AudioRecord),
    REG_JNI(register_android_media_AudioSystem),
    REG_JNI(register_android_media_AudioTrack),
    REG_JNI(register_android_media_JetPlayer),
    REG_JNI(register_android_media_RemoteDisplay),
    REG_JNI(register_android_media_ToneGenerator),

    REG_JNI(register_android_opengl_classes),
    REG_JNI(register_android_server_NetworkManagementSocketTagger),
    REG_JNI(register_android_ddm_DdmHandleNativeHeap),
    REG_JNI(register_android_backup_BackupDataInput),
    REG_JNI(register_android_backup_BackupDataOutput),
    REG_JNI(register_android_backup_FileBackupHelperBase),
    REG_JNI(register_android_backup_BackupHelperDispatcher),
    REG_JNI(register_android_app_backup_FullBackup),
    REG_JNI(register_android_app_ActivityThread),
    REG_JNI(register_android_app_NativeActivity),
    REG_JNI(register_android_view_InputChannel),
    REG_JNI(register_android_view_InputEventReceiver),
    REG_JNI(register_android_view_InputEventSender),
    REG_JNI(register_android_view_InputQueue),
    REG_JNI(register_android_view_KeyEvent),
    REG_JNI(register_android_view_MotionEvent),
    REG_JNI(register_android_view_PointerIcon),
    REG_JNI(register_android_view_VelocityTracker),

    REG_JNI(register_android_content_res_ObbScanner),
    REG_JNI(register_android_content_res_Configuration),

    REG_JNI(register_android_animation_PropertyValuesHolder),
    REG_JNI(register_com_android_internal_content_NativeLibraryHelper),
    REG_JNI(register_com_android_internal_net_NetworkStatsFactory),
};
```

gRegJNI数组，有138个成员变量，定义在**AndroidRuntime.cpp**中，该数组中每一个成员都代表一类文件的jni映射，其中REG\_JNI是一个宏定义，让我们来看下

###### 3、REG\_JNI 宏定义

```
#ifdef NDEBUG
    #define REG_JNI(name)      { name }
    struct RegJNIRec {
        int (*mProc)(JNIEnv*);
    };
#else
    #define REG_JNI(name)      { name, #name }
    struct RegJNIRec {
        int (*mProc)(JNIEnv*);
        const char* mName;
    };
#endif
```

> 其中 **mProc**，就等价于调用其参数名所指向的函数，例如 **REG\_JNI(register\_com\_android\_internal\_os\_RuntimeInit).mProc** 也就是指进入 **register\_com\_android\_internal\_os\_RuntimeInit**的方法，以此为例，看下面的代码

```
int register_com_android_internal_os_RuntimeInit(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, "com/android/internal/os/RuntimeInit",
        gMethods, NELEM(gMethods));
}

//gMethods：java层方法名与jni层的方法的一一映射关系
static JNINativeMethod gMethods[] = {
    { "nativeFinishInit", "()V",
        (void*) com_android_internal_os_RuntimeInit_nativeFinishInit },
    { "nativeZygoteInit", "()V",
        (void*) com_android_internal_os_RuntimeInit_nativeZygoteInit },
    { "nativeSetExitWithoutCleanup", "(Z)V",
        (void*) com_android_internal_os_RuntimeInit_nativeSetExitWithoutCleanup },
};
```

所以REG\_JNI就是一个宏定义，该宏的作用就是调用相应的方法。

##### (二)、如何查找native方法

> 当大家在看framework层代码时，经常会看到native方法，这往往需要查看所对应的C++方法在那个文件，对应那个方法？下面从一个实例出发带大家如何查看Java层方法所对应的Native方法位置

###### 1、实例(一)

在后面分析Android消息机制源码，遇到MessageQueue.java中有多个native方法，比如：

```
private native void nativePollOnce(long ptr, int timeoutMillis);
```

这样要怎么查找那？主要分为两个步骤

-   第一步：MessageQueue.java的全限定名为android.os.MessageQueue.java。方法名为android.os.MessageQueue.nativePollOnce()，而相对应的native层方法名只是将点号替换为下划线，所以可得android\_os\_MessgaeQueue\_nativePollOnce()。所以：nativePollOnce---->android\_os\_MessageQueue\_nativePollOnce()
-   第二步：有了对应的native方法，接下来就需要这个native方法在那个文件中，上面已经说了，Android系统启动的时候已经注册了大量的JNI方法。

在AndroidRumtime.cpp的gRegJNI数组。这些注册方法命名方式如下：

那么MessageQueue.java所定义的JNI注册方法名应该是 \*\* register\_android\_os\_MessageQueue \*\* ，的确存在于 \*\* gRegJNI \*\* 数组，说明这次JNI注册过程是开机过程完成的。该方法是在 \*\* AndroidRuntime.cpp \*\* 申明为extern方法:

```
extern int register_android_os_MessageQueue(JNIEnv* env);
```

这些extern方法绝大多数位于 \*\* /framework/base/core/jni \*\* 目录，大多数情况下 native命名方式为

```
[包名]_[类名].cpp
[包名]_[类名].h
```

所以 MessageQueue.java--->android\_os\_MessageQueue.cpp。 打开android\_os\_MessageQueue.cpp文件，搜索android\_os\_MessageQueue\_nativePollOnce方法，这便找到了目标方法：

```
static void android_os_MessageQueue_nativePollOnce(JNIEnv* env, jobject obj,
        jlong ptr, jint timeoutMillis) {
    NativeMessageQueue* nativeMessageQueue = reinterpret_cast<NativeMessageQueue*>(ptr);
    nativeMessageQueue->pollOnce(env, obj, timeoutMillis);
}
```

到这里完成了一次从Java层方法搜索到所对应的C++方法的过程。

###### 2、实例(二)

对于native文件命名方式，有时并非\[包名\]\_\[类名\].cpp，比如Binder.java，Binder.java所对应的native文件：android\_util\_Binder.cpp。

比如：Binder.java的native方法，代码如下：

```
public static final native int getCallingPid();
```

> 根据实例(一)的方式，找到getCallingPid--->android\_os\_Binder\_getCallingPid()，并且在AndroidRumtime.cpp中的gRegJNI数组找到了register\_android\_os\_Binder。按照实例(一)方式则native中的文件名为android\_os\_Binder.cpp，可是在/framwork/base/core/jni/目录下找不到该文件，这是例外的情况。其实真正的文件名为android\_util\_Binder.cpp，这就是例外，这一点有些费劲，不明白为何google打破之前的规律。

```
//frameworks/base/core/jni/android_util_Binder.cpp    761行
static jint android_os_Binder_getCallingPid(JNIEnv* env, jobject clazz)
{
    return IPCThreadState::self()->getCallingPid();
}
```

有人会问，以后遇到打破常规的文件命名的文件怎么办？其实很简单，首先，先尝试在/framework/base/core/jni/中搜索，对于Binder.java，可以直接搜索Binder关键字，其他类似。如果这里找不到，可以通过grep全局搜索android\_os\_Binder\_getCallingPid()这个方法在那个文件上。

jni存在的常见目录：

-   /framework/base/core/jni
-   /framework/base/services/core/jni
-   /framework/base/media/jni

###### 3、实例(三)

前面两种都是在Android系统启动之初，便已经注册过JNI所对应的方法。那么如果程序自己定义的JNI方法，该如何查看JNI方法所在的位置？下面以MediaPlayer.java为例，其包名为android.media:

```
public class MediaPlayer{
    static {
        System.loadLibrary("media_jni");
        native_init();
    }

    private static native final void native_init();
}
```

> 通过static 静态代码块中的System.loadLibrary()方法来加载动态库，库名为media\_jni，Android平台则会自动扩展成所对应的libmedia\_jni.so库。接着通过关键字native加载native\_init方法之前，便可以在java层直接使用native层方法。

接下来便要查看libmedia\_jni.so库定义所在文件，一般都是通过android.mk文件中定义的LOCAL\_MODULE:= libmedia\_jni，可以采用grep或者mgrep来搜索包含libmedia\_jni字段的Android.mk所在路径。

搜索可知，libmedia\_jni.so位于/framework/base/media/jni/Android.mk。用前面的实例(一)中的知识来查看相应的文件和方法分别为:

```
android_media_MediaPlayer.cpp
android_media_MediaPlayer_native_init()
```

再然后，你会发现果然在该Android.mk所在目录/frameworks/base/media/jni中找到android\_media\_MediaPlayer.cpp文件。并在文件中存在相应的方法

```
//frameworks/base/media/jni/android_media_MediaPlayer.cpp     820行
    // This function gets some field IDs, which in turn causes class initialization.
    // It is called from a static block in MediaPlayer, which won't run until the
    // first time an instance of this class is used.
    // 当类初始化的时候此函数获取一些字段ID。因为它是在MediaPlayer中的静态块中调用的，所以除非是第一次使用此类的实例，否则它将不会运行。
    static void android_media_MediaPlayer_native_init(JNIEnv *env)
    {
        jclass clazz;
        clazz = env->FindClass("android/media/MediaPlayer");
        if (clazz == NULL) {
            return;
        }
        fields.context = env->GetFieldID(clazz, "mNativeContext", "J");
        if (fields.context == NULL) {
            return;
        }
        fields.post_event = env->GetStaticMethodID(clazz, "postEventFromNative",
                "(Ljava/lang/Object;IIILjava/lang/Object;)V");
        if (fields.post_event == NULL) {
            return;
        }
        fields.surface_texture = env->GetFieldID(clazz, "mNativeSurfaceTexture", "J");
        if (fields.surface_texture == NULL) {
            return;
        }
        env->DeleteLocalRef(clazz);
        clazz = env->FindClass("android/net/ProxyInfo");
        if (clazz == NULL) {
            return;
        }
        fields.proxyConfigGetHost =
                env->GetMethodID(clazz, "getHost", "()Ljava/lang/String;");
        fields.proxyConfigGetPort =
                env->GetMethodID(clazz, "getPort", "()I");
        fields.proxyConfigGetExclusionList =
                env->GetMethodID(clazz, "getExclusionListAsString", "()Ljava/lang/String;");
        env->DeleteLocalRef(clazz);
        gPlaybackParamsFields.init(env);
        gSyncParamsFields.init(env);
    }
```

所以 MediaPlayer.java中的native\_init()方法位于/frameworks/base/media/jni/目录下的android\_media\_MediaPlayer.cpp文件中的android\_media\_MediaPlayer\_native\_init方法。

##### (三)、总结

> JNI作为连接Java世界和C/C++世界的桥梁，很有必要掌握。看完本文后，一定要掌握在分析Android源码过程汇总如何查找native方法。首先明白native方法名和文件名的命名规律，其次要懂得该如何去搜索代码。JNI方式注册无非是Android系统启动中Zygote注册以及通过System.loadLibrary方式注册，对于系统启动过程注册的，可以通过查询AndroidRuntime.cpp中的gRegJNI是否存在对应的register方法，如果不存在，则大多数通过LoadLibrary方式来注册。

### 五、loadLibrary源码分析

> 再来进一步分析，Java层与native层方法是如何注册并映射的，继续以MediaPlayer为例，进一步分析。

在MediaPlayer.java中调用System.loadLibrary("media\_jni"),把libmedia\_jni.so动态库加载到内存。接下来以loadLibrary为起点展开JNI注册流程的过程分析。

##### (一) loadLibrary() 流程

###### 1、loadLibrary()方法

```
//libcore/luni/src/main/java/java/lang/System.java     1075行
    /**
     * Loads the system library specified by the <code>libname</code>
     * argument. The manner in which a library name is mapped to the
     * actual system library is system dependent.
     *
     * 加载由 libname 参数指定的系统库， library库名是通过系统依赖映射到实际系统库的。
     *
     * <p>
     * The call <code>System.loadLibrary(name)</code> is effectively
     * equivalent to the call
     * <blockquote><pre>
     * Runtime.getRuntime().loadLibrary(name)
     * </pre></blockquote>
     *
     * 调用 System.loadLibrary(name)实际上等价于调用
     * Runtime.getRuntime().loadLibrary（name）
     *
     * @param      libname   the name of the library.      lib库的名字
     * @exception  SecurityException  if a security manager exists and its
     *             <code>checkLink</code> method doesn't allow
     *             loading of the specified dynamic library
     * 如果存在安全管理员，并且其 checkLink 方法不允许 加载指定的动态库，则会抛出SecurityException
     * @exception  UnsatisfiedLinkError  if the library does not exist.
     * 如果库不存在则抛出UnsatisfiedLinkError
     * @exception  NullPointerException if <code>libname</code> is
     *             <code>null</code>
     * 如果libname是null则抛出NullPointerException
     * @see        java.lang.Runtime#loadLibrary(java.lang.String)
     * @see        java.lang.SecurityManager#checkLink(java.lang.String)
     */
    public static void loadLibrary(String libname) {
         Runtime.getRuntime().loadLibrary(libName, VMStack.getCallingClassLoader());
    }
```

通过代码和上面的注释，我们知道了，loadLibrary(String)其本质是调用了 Runtime.getRuntime().loadLibrary(String,ClassLoader) 那我们就来跟踪一下

###### 2、Runtime.getRuntime().loadLibrary(String,ClassLoader)方法

```
     /*
      * Searches for and loads the given shared library using the given ClassLoader.
      * 使用指定的ClassLoader搜索并加载给定的共享库
      */
    void loadLibrary(String libraryName, ClassLoader loader) {
         //如果load不为null 则进入该分支 
        if (loader != null) {
            //查找库所在的路径
            String filename = loader.findLibrary(libraryName);
            // 如果路径为null则说明找不到库
            if (filename == null) {
                // It's not necessarily true that the ClassLoader used
                // System.mapLibraryName, but the default setup does, and it's
                // misleading to say we didn't find "libMyLibrary.so" when we
                // actually searched for "liblibMyLibrary.so.so".
                // 当我们搜索liblibMyLibrary.so.so的时候，可能会提示我们没有找
                // 到"ibMyLibrary.so"。是因为ClassClassLoader不一定使用System，
                 // 但是默认设置又是这样的，所以会有一定的误导性
                throw new UnsatisfiedLinkError(loader + " couldn't find \"" +
                        System.mapLibraryName(libraryName) + "\"");
            }
            //找到路径，则加载库
            String error = doLoad(filename, loader);
            if (error != null) {
                throw new UnsatisfiedLinkError(error);
            }
            return;
        }
        // 如果loader为null，则执行下面的代码
        // 其中 System.mapLibraryName(String) 是"根据库名返回本地的库"
        String filename = System.mapLibraryName(libraryName);
        List<String> candidates = new ArrayList<String>();
        String lastError = null;
        for (String directory : mLibPaths) {
            String candidate = directory + filename;
            candidates.add(candidate);
            if (IoUtils.canOpenReadOnly(candidate)) {
                //加载库
                String error = doLoad(candidate, loader);
                if (error == null) {
                    return; // We successfully loaded the library. Job done.
                }
                lastError = error;
            }
        }
        if (lastError != null) {
            throw new UnsatisfiedLinkError(lastError);
        }
        throw new UnsatisfiedLinkError("Library " + libraryName + " not found; tried " + candidates);
    }
```

通过上面的代码，我们知道，无论loader是否为null，最后都是通过doLoad(String, ClassLoader)来真正的加载。那我们来一起看一下

###### 3、doLoad(String name, ClassLoader loader) 方法

```
    //libcore/luni/src/main/java/java/lang/Runtime.java            401行
    private String doLoad(String name, ClassLoader loader) {
        // Android apps are forked from the zygote, so they can't have a custom LD_LIBRARY_PATH,
        // which means that by default an app's shared library directory isn't on LD_LIBRARY_PATH.
       // Android应用程序从zygote分支fork出来的，所以他们无法自定义
       // LD_LIBRARY_PATH，这意味着默认情况下，应用程序的共享库目录不在
       // LD_LIBRARY_PATH上。

        // The PathClassLoader set up by frameworks/base knows the appropriate path, so we can load
        // libraries with no dependencies just fine, but an app that has multiple libraries that
        // depend on each other needed to load them in most-dependent-first order.

        // We added API to Android's dynamic linker so we can update the library path used for
        // the currently-running process. We pull the desired path out of the ClassLoader here
        // and pass it to nativeLoad so that it can call the private dynamic linker API.

        // We didn't just change frameworks/base to update the LD_LIBRARY_PATH once at the
        // beginning because multiple apks can run in the same process and third party code can
        // use its own BaseDexClassLoader.

        // We didn't just add a dlopen_with_custom_LD_LIBRARY_PATH call because we wanted any
        // dlopen(3) calls made from a .so's JNI_OnLoad to work too.

        // So, find out what the native library search path is for the ClassLoader in question...
        // 由framework / base设置的PathClassLoader知道适具体的路径，因此我们可以
        // 加载没有依赖关系的库，但是具有依赖于彼此的多个库的应用程序需要以大多
        // 数依赖的顺序加载它们。 
        // 为了让我们可以在正在运行的进程中更新库路径，所以我们向Android的动态链
        // 接器添加了API。
        // 我们将所需的路径从ClassLoader中拉出，并将其传递给nativeLoad，便可以  
        // 调用私有动态链接器API。
        // 我们不仅仅是更改框架/基础来更新LD_LIBRARY_PATH一次，因为多个apk
        // 可以在同一进程中运行，第三方代码可以使用自己的BaseDexClassLoader。
        // 我们没有添加dlopen_with_custom_LD_LIBRARY_PATH调用，因为我们希望
        // 使用.so的JNI_OnLoad进行的任何dlopen（3）调用也可以工作。
        //因此，找出本机库搜索路径对于有问题的ClassLoader

        String ldLibraryPath = null;
        String dexPath = null;
        if (loader == null) {
            // We use the given library path for the boot class loader. This is the path
            // also used in loadLibraryName if loader is null.
            ldLibraryPath = System.getProperty("java.library.path");
        } else if (loader instanceof BaseDexClassLoader) {
            BaseDexClassLoader dexClassLoader = (BaseDexClassLoader) loader;
            ldLibraryPath = dexClassLoader.getLdLibraryPath();
        }
        // nativeLoad should be synchronized so there's only one LD_LIBRARY_PATH in use regardless
        // of how many ClassLoaders are in the system, but dalvik doesn't support synchronized
        // internal natives.
        // nativeLoad应该同步，所以使用中只有一个LD_LIBRARY_PATH，无论系统中有多少个ClassLoaders，dalvik不支持native的同步。
        synchronized (this) {
            return nativeLoad(name, loader, ldLibraryPath);
        }
    }
```

nativeLoad()这是一个native方法，再进去ART虚拟机java\_lang\_Runtime.cc，再细讲就要深入剖析虚拟机内部，这里就不往下深入了。后续有时间单独讲解下虚拟机。这里直接说结论

-   调用dlopen()函数，打开一个so文件并创建一个handle;
-   调用dlsym()函数，查看相应so文件的JNIOnLoad()函数指针，并执行相应函数。

###### 4、总结

所以说，System.loadLibrary()的作用就是调用相应库中的JNI\_OnLoad()方法。那我们就来看下JNI\_OnLoad()过程

##### (二) JNI\_OnLoad流程

###### 1、JNI\_OnLoad()

```
jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    //frameworks/base/media/jni/android_media_MediaPlayer.cpp    1132行
    // 注册JNI
    if (register_android_media_MediaPlayer(env) < 0) {
        goto bail;
    }
    ...
}
```

这里面主要通过调用register\_android\_media\_MediaPlayer()函数来进行注册的。

###### 2、register\_android\_media\_MediaPlayer()

```
//frameworks/base/media/jni/android_media_MediaPlayer.cpp 1086行
// This function only registers the native method
// 这个函数仅仅是用来注册native方法的
static int register_android_media_MediaPlayer(JNIEnv *env)
{
    return AndroidRuntime::registerNativeMethods(env,
                "android/media/MediaPlayer", gMethods, NELEM(gMethods));
}
```

我们看到register\_android\_media\_MediaPlayer()函数里面，实际上是调用的AndroidRuntime的registerNativeMethods()函数。

在看AndroidRuntime的registerNativeMethods()函数之前，先说下gMethods

###### 3、gMethods

```
//frameworks/base/media/jni/android_media_MediaPlayer.cpp    1036行
static JNINativeMethod gMethods[] = {
    {
        "nativeSetDataSource",
        "(Landroid/os/IBinder;Ljava/lang/String;[Ljava/lang/String;"
        "[Ljava/lang/String;)V",
        (void *)android_media_MediaPlayer_setDataSourceAndHeaders
            
    },
    {"_setDataSource",      "(Ljava/io/FileDescriptor;JJ)V",    (void *)android_media_MediaPlayer_setDataSourceFD},
    {"_setDataSource",      "(Landroid/media/MediaDataSource;)V",(void *)android_media_MediaPlayer_setDataSourceCallback },
    {"_setVideoSurface",    "(Landroid/view/Surface;)V",        (void *)android_media_MediaPlayer_setVideoSurface},
    {"_prepare",            "()V",                              (void *)android_media_MediaPlayer_prepare},
    {"prepareAsync",        "()V",                              (void *)android_media_MediaPlayer_prepareAsync},
    {"_start",              "()V",                              (void *)android_media_MediaPlayer_start},
    {"_stop",               "()V",                              (void *)android_media_MediaPlayer_stop},
    {"getVideoWidth",       "()I",                              (void *)android_media_MediaPlayer_getVideoWidth},
    {"getVideoHeight",      "()I",                              (void *)android_media_MediaPlayer_getVideoHeight},
    {"setPlaybackParams", "(Landroid/media/PlaybackParams;)V", (void *)android_media_MediaPlayer_setPlaybackParams},
    {"getPlaybackParams", "()Landroid/media/PlaybackParams;", (void *)android_media_MediaPlayer_getPlaybackParams},
    {"setSyncParams",     "(Landroid/media/SyncParams;)V",  (void *)android_media_MediaPlayer_setSyncParams},
    {"getSyncParams",     "()Landroid/media/SyncParams;",   (void *)android_media_MediaPlayer_getSyncParams},
    {"seekTo",              "(I)V",                             (void *)android_media_MediaPlayer_seekTo},
    {"_pause",              "()V",                              (void *)android_media_MediaPlayer_pause},
    {"isPlaying",           "()Z",                              (void *)android_media_MediaPlayer_isPlaying},
    {"getCurrentPosition",  "()I",                              (void *)android_media_MediaPlayer_getCurrentPosition},
    {"getDuration",         "()I",                              (void *)android_media_MediaPlayer_getDuration},
    {"_release",            "()V",                              (void *)android_media_MediaPlayer_release},
    {"_reset",              "()V",                              (void *)android_media_MediaPlayer_reset},
    {"_setAudioStreamType", "(I)V",                             (void *)android_media_MediaPlayer_setAudioStreamType},
    {"_getAudioStreamType", "()I",                              (void *)android_media_MediaPlayer_getAudioStreamType},
    {"setParameter",        "(ILandroid/os/Parcel;)Z",          (void *)android_media_MediaPlayer_setParameter},
    {"setLooping",          "(Z)V",                             (void *)android_media_MediaPlayer_setLooping},
    {"isLooping",           "()Z",                              (void *)android_media_MediaPlayer_isLooping},
    {"_setVolume",          "(FF)V",                            (void *)android_media_MediaPlayer_setVolume},
    {"native_invoke",       "(Landroid/os/Parcel;Landroid/os/Parcel;)I",(void *)android_media_MediaPlayer_invoke},
    {"native_setMetadataFilter", "(Landroid/os/Parcel;)I",      (void *)android_media_MediaPlayer_setMetadataFilter},
    {"native_getMetadata", "(ZZLandroid/os/Parcel;)Z",          (void *)android_media_MediaPlayer_getMetadata},
    {"native_init",         "()V",                              (void *)android_media_MediaPlayer_native_init},
    {"native_setup",        "(Ljava/lang/Object;)V",            (void *)android_media_MediaPlayer_native_setup},
    {"native_finalize",     "()V",                              (void *)android_media_MediaPlayer_native_finalize},
    {"getAudioSessionId",   "()I",                              (void *)android_media_MediaPlayer_get_audio_session_id},
    {"setAudioSessionId",   "(I)V",                             (void *)android_media_MediaPlayer_set_audio_session_id},
    {"_setAuxEffectSendLevel", "(F)V",                          (void *)android_media_MediaPlayer_setAuxEffectSendLevel},
    {"attachAuxEffect",     "(I)V",                             (void *)android_media_MediaPlayer_attachAuxEffect},
    {"native_pullBatteryData", "(Landroid/os/Parcel;)I",        (void *)android_media_MediaPlayer_pullBatteryData},
    {"native_setRetransmitEndpoint", "(Ljava/lang/String;I)I",  (void *)android_media_MediaPlayer_setRetransmitEndpoint},
    {"setNextMediaPlayer",  "(Landroid/media/MediaPlayer;)V",   (void *)android_media_MediaPlayer_setNextMediaPlayer},
};
```

gMethods，记录java层和C/C++层方法的一一映射关系。这里涉及到结构体JNINativeMethod，其定义在jni.h文件：

```
/、libnativehelper/include/nativehelper/jni.h    129行
typedef struct {
    const char* name;  //Java层native函数名
    const char* signature;   //Java函数签名，记录参数类型和个数，以及返回值类型
    void*       fnPtr;  //Native层对应的函数指针
} JNINativeMethod;
```

下面让我们看一下AndroidRuntime的registerNativeMethods()函数

###### 4、AndroidRuntime的registerNativeMethods()函数

```
//frameworks/base/core/jni/AndroidRuntime.cpp  262行
/*
 * Register native methods using JNI.
 * 使用JNI注册native方法
 */
int AndroidRuntime::registerNativeMethods(JNIEnv* env,
    const char* className, const JNINativeMethod* gMethods, int numMethods)
{
    return jniRegisterNativeMethods(env, className, gMethods, numMethods);
}
```

jniRegisterNativeMethods该方法是由Android JNI帮助类JNIHelp.cpp来完成。

###### 5、jniRegisterNativeMethods()函数

```
//libnativehelper/JNIHelp.cpp     73行
extern "C" int jniRegisterNativeMethods(C_JNIEnv* env, const char* className,
    const JNINativeMethod* gMethods, int numMethods)
{
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);

    ALOGV("Registering %s's %d native methods...", className, numMethods);
    scoped_local_ref<jclass> c(env, findClass(env, className));
    //找不到native注册方法
    if (c.get() == NULL) {
        char* msg;
        asprintf(&msg, "Native registration unable to find class '%s'; aborting...", className);
        e->FatalError(msg);
    }
     //调用JNIEnv结构体的成员变量
    if ((*env)->RegisterNatives(e, c.get(), gMethods, numMethods) < 0) {
        //如果native方法注册失败
        char* msg;
        asprintf(&msg, "RegisterNatives failed for '%s'; aborting...", className);
        e->FatalError(msg);
    }

    return 0;
}
```

通过上面的代码我们发现jniRegisterNativeMethods()内部真正的注册函数是RegisterNatives()函数，那我们继续跟踪

###### 6、RegisterNatives()函数

```
// libnativehelper/include/nativehelper/jni.h         976行
    jint RegisterNatives(jclass clazz, const JNINativeMethod* methods,
        jint nMethods)
    { return functions->RegisterNatives(this, clazz, methods, nMethods); }
}
```

其中functions是指向JNINativeInterface结构体指针，也就是将调用下面的方法:

```
struct JNINativeInterface {
    jint (*RegisterNatives)(JNIEnv*, jclass, const JNINativeMethod*,jint);
}
```

再往下深入就到了虚拟机内部了，就不在深入了。

##### (三) 总结

总之，这个过程完成了gMethods数组中的方法的映射关系，比如java层的native\_init()方法，映射到native层的android\_media\_MediaPlayer\_native\_init()方法。

虚拟机相关的变量中有两个非常重要的变量JavaVM和JNIEnv:

-   JavaVM：是指进程虚拟机环境，每个进程有且只有一个JavaVM实例。
-   JNIEnv：是指线程上下文环境，每个线程有且只有一个JNIEnv实例。

### 六、JNI资源

JNINativeMethod结构体中有一个字段为signature(签名)，在介绍signature格式之前需要掌握各种数据类型在Java层、Native层以及签名所采用的签名格式。

##### (一)、数据类型

###### 1、基本数据类型

|Signature|Java|Native|
|---|---|---|
|B|byte|jbyte|
|C|char|jchar|
|D|double|jdouble|
|F|float|jfloat|
|I|int|jint|
|S|short|jshort|
|J|long|jlong|
|Z|boolean|jboolean|
|V|void|void|
###### 2、数组数据类型

数组简称则是在前面添加" \*\*\[ \*\* " ：

|Signature格式|Java|Native|
|---|---|---|
|[B|byte[]|jbyteArray|
|[C|char[]|jcharArray|
|[D|double[]|jdoubleArray|
|[F|float[]|jfloatArray|
|[I|int[]|jintArray|
|[S|shor[]|jshortArray|
|[ J|long[]|jlongArray|
|[ Z|boolean[]|jbooleanArray|
###### 3、复杂数据类型

对象类型简称：L+ \*\* classname \*\*+;

|Signature格式|Java|Native|
|---|---|---|
|Ljava/lang/String|String|jstring|
|L+class+ ;|所有对象|jobject|
|[ L + classname + ;|Object[]|jobjectArray|
|Ljava.lang.Class|Class|jclass|
|Ljava.lang.Throwable|Throwable|jthrowable|
###### 4、Signature

有了前面的讲解，我们通过案例来说说函数签名:\*\* (入参) 返回值参数 \*\*，这里用到的便是前面介绍的Signature格式

| Java格式                          | 对应的签名                     |
| ------------------------------- | ------------------------- |
| void foo()                      | ()V                       |
| float foo(int i)                | (I) F                     |
| long foo(int[] i)               | ([ i) J                   |
| double foo (Class c)            | (Ljava/lang/Class;) D     |
| boolean foo (int [] i,String s) | ([ILjava/lang/String ;) Z |
| String foo(int i)               | (I) Ljava/lang/String ;   |
##### (二)、其他注意事项

###### 1、垃圾回收

> 对于Java而言，开发者是无需关心垃圾回收的，因为这完全由虚拟机GC来负责垃圾回收，而对于JNI开发人员，由于内存释放需要谨慎处理，需要的时候申请，使用完后记得释放内存，以免发生内存泄露。

所以JNI提供了了三种Reference类型:

-   Local Reference(本地引用)
-   Global Reference(全局引用)
-   Weak Global Reference (全局弱引用)

其中Global Reference 如果不主动释放，则一直不会释放；对于其他两个类型的引用都是释放的可能性，那是不是意味着不需要手动释放? 答案是否定的，不管这三种类型的那种引用，都尽可能在某个内存不需要时，立即释放，这对系统更为安全可靠，以减少不可预知的性能与稳定性问题。

###### 另外，ART虚拟机在GC算法有所优化，为了减少内存碎片化问题，在GC之后有可能会移动对象内存的位置，对于Java层程序并没有影响，但是对于JNI程序要注意了，对于通过指针来直接访问内存对象时，Dalvik能正确运行的程序，ART下未必能正常运行。

###### 2、异常处理

> Java层出现异常，虚拟机会直接抛出异常，这是需要try... catch 或者继续向外throw。但是对于JNI出现异常时，即执行到JNIEnv 中某个函数异常时，并不会立即抛出异常来中断程序的执行，还可以继续执行内存之类的清理工作，知道返回Java层才会抛出相应的异常。

另外，Dalvik虚拟机有些情况下JNI函数出错可能会返回NULL，但ATR虚拟机在出错时更多是抛出异常。这样导致的问题就可能是在Dalvik版本能正常运行的成员，在ART虚拟机上并没正确处理异常而崩溃。

### 七、总结

本文主要是通过实例，基于Android 6.0源码分析 JNI原理，讲述JNI核心功能：

-   介绍了JNI的概念及如何查找JNI方法，让大家明白如何从Java层跳转到Native层
-   分了JNI函数注册流程，进异步加深对JNI的理解
-   列举Java与Native一级函数签名方式。