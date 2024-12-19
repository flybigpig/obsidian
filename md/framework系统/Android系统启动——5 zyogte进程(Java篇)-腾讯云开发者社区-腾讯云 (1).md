本篇文章的主要内容如下：

-   1、Java层的ZygoteInit的main()方法
-   2、registerZygoteSocket(socketName)方法解析
-   3、预加载系统类和资源
-   4、启动SystemServer
-   5、处理启动应用的请求——runSelectLoop()方法解析
-   6、Zygote总结

上一篇文章，我们知道在[AndroidRuntime.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjni%252FAndroidRuntime.cpp&objectId=1199510&objectType=1&isNewArticle=undefined)的**start()**函数里面是调用的**Zygoteinit**类的**main()**函数，那我们就继续研究

### 一、Java层的ZygoteInit的main()方法

代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined) 565行

```
    public static void main(String argv[]) {
        try {

            //**************** 第一阶段 **********************

            // 启动DDMS
            RuntimeInit.enableDdms();
            // Start profiling the zygote initialization.

            // 启动性能统计 
            SamplingProfilerIntegration.start();

            boolean startSystemServer = false;
            String socketName = "zygote";
            String abiList = null;
            for (int i = 1; i < argv.length; i++) {
                if ("start-system-server".equals(argv[i])) {
                    startSystemServer = true;
                } else if (argv[i].startsWith(ABI_LIST_ARG)) {
                    abiList = argv[i].substring(ABI_LIST_ARG.length());
                } else if (argv[i].startsWith(SOCKET_NAME_ARG)) {
                    socketName = argv[i].substring(SOCKET_NAME_ARG.length());
                } else {
                    throw new RuntimeException("Unknown command line argument: " + argv[i]);
                }
            }

            if (abiList == null) {
                throw new RuntimeException("No ABI list supplied.");
            }

           //**************** 第二阶段 **********************
            registerZygoteSocket(socketName);
            EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_START,
                SystemClock.uptimeMillis());

           //**************** 第三阶段 **********************
            preload();
            EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_END,
                SystemClock.uptimeMillis());

            // Finish profiling the zygote initialization.
            SamplingProfilerIntegration.writeZygoteSnapshot();

            // Do an initial gc to clean up after startup
            gcAndFinalize();

            // Disable tracing so that forked processes do not inherit stale tracing tags from
            // Zygote.
            Trace.setTracingEnabled(false);

           //**************** 第四阶段 **********************
            if (startSystemServer) {
                startSystemServer(abiList, socketName);
            }

            Log.i(TAG, "Accepting command socket connections");


           //**************** 第五阶段 **********************
            runSelectLoop(abiList);

            closeServerSocket();
        } catch (MethodAndArgsCaller caller) {
            caller.run();
        } catch (RuntimeException ex) {
            Log.e(TAG, "Zygote died with exception", ex);
            closeServerSocket();
            throw ex;
        }
    }
```

我将ZygoteInit的main()方法分为5个阶段，阶段解析如下：

-   第一阶段：主要是解析调用的参数，即argv\[\]，通过for循环遍历解析，通过string的方法来判断，主要出是初始化startSystemServer、abiList和socketName变量
-   第二阶段：调用registerZygoteSocket(socketName)方法注册Zygote的socket监听接口，用来启动应用程序的消息
-   第三阶段：调用preload()方法装载系统资源，包括系统预加载类、Framework资源和openGL的资源。这样当程序被fork处理后，应用的进程内已经包含了这些系统资源，大大节省了应用的启动时间。
-   第四阶段：调用startSystemServer()方法启动SystemServer进程
-   第五阶段：调动runSelectLooper方法进入监听和接收消息的循环

> PS：在整个catch里面有个MethodAndArgsCaller。这个MethodAndArgsCaller类是Exception的子类，MethodAndArgsCaller类在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined) 711行，这个类主要是为了清除Zygote中当前的栈信息，通过的方式就是其run()方法。

下面我们就依次跟踪下

那我们先来看下代码 代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined) 107行

```
    /**
     * Registers a server socket for zygote command connections
     *
     * @throws RuntimeException when open fails
     */
    private static void registerZygoteSocket(String socketName) {
        if (sServerSocket == null) {
            int fileDesc;
            final String fullSocketName = ANDROID_SOCKET_PREFIX + socketName;
            try {
                // 我们知道 fullSocketName等于ANDROID_SOCKET_zygote
                String env = System.getenv(fullSocketName);
                fileDesc = Integer.parseInt(env);
            } catch (RuntimeException ex) {
                throw new RuntimeException(fullSocketName + " unset or invalid", ex);
            }

            try {
                FileDescriptor fd = new FileDescriptor();
                fd.setInt$(fileDesc);
                sServerSocket = new LocalServerSocket(fd);
            } catch (IOException ex) {
                throw new RuntimeException(
                        "Error binding to local socket '" + fileDesc + "'", ex);
            }
        }
    }
```

首先翻译一下注释

> 为zygote命令 注册一个socket连接的服务端socket
> 
> 通过前面的文章，我们知道init进程会根据这条选项来创建一个"AF\_UNIX"socket，并把它的句柄放到环境变量"ANDROID\_SOCKET\_zygote"中。

同理我们也可以这样得到句柄，得到句柄后，new了一个FileDescriptor对象，并通过调用**setInt$()**方法来设置其值。最后new了LocalServerSocket对象，来创建本地的服务socket，并将其值保存在全局变量sServerSocket中。

### 三、预加载系统类和资源

为了加快应用程序的启动，Android把系统公用的Java类和一部分Framework的资源保存在zygote中了，这样就可以保证zygote进程fork子进程的是共享的。如下图所示

![](https://ask.qcloudimg.com/http-save/yehe-2957818/dgwxkuqc2l.png)

预加载.png

我们前面也说Zygote类的main()方法里面的**第三阶段调用preload加载资源**，那我们就一起来看下

代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined) 180行

```
    static void preload() {
        Log.d(TAG, "begin preload");
        preloadClasses();
        preloadResources();
        preloadOpenGL();
        preloadSharedLibraries();
        preloadTextResources();
        // Ask the WebViewFactory to do any initialization that must run in the zygote process,
        // for memory sharing purposes.
        WebViewFactory.prepareWebViewInZygote();
        Log.d(TAG, "end preload");
    }
```

我们看到preload()方法中又调用一些方法，我们来简单看下

-   preloadClasses()：预加载Java类
-   preloadResources()：预加资源
-   preloadOpenGL()：预加载OpenGL资源
-   preloadSharedLibraries()：预计加载共享库
-   preloadTextResources()：预加载文本资源
-   WebViewFactory.prepareWebViewInZygote()：初始化WebView

其中 preloadTextResources()是6.0新增的方法

那我们就依次来看下

##### (一) 预加载Java类

我们先来看下preloadClasses函数的内部实现，代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined) 217行

```
    /**
     * Performs Zygote process initialization. Loads and initializes
     * commonly used classes.
     *
     * Most classes only cause a few hundred bytes to be allocated, but
     * a few will allocate a dozen Kbytes (in one case, 500+K).
     */
    private static void preloadClasses() {
        // 获取虚拟机实例
        final VMRuntime runtime = VMRuntime.getRuntime();

        InputStream is;
        try {
            // 获取指定文件的输入流 
            // PRELOADED_CLASSES=/system/etc/preloaded-classes
            is = new FileInputStream(PRELOADED_CLASSES);
        } catch (FileNotFoundException e) {
            Log.e(TAG, "Couldn't find " + PRELOADED_CLASSES + ".");
            return;
        }

        Log.i(TAG, "Preloading classes...");
        long startTime = SystemClock.uptimeMillis();

        // Drop root perms while running static initializers.
        final int reuid = Os.getuid();
        final int regid = Os.getgid();

        // We need to drop root perms only if we're already root. In the case of "wrapped"
        // processes (see WrapperInit), this function is called from an unprivileged uid
        // and gid.
        boolean droppedPriviliges = false;
        if (reuid == ROOT_UID && regid == ROOT_GID) {
            try {
                Os.setregid(ROOT_GID, UNPRIVILEGED_GID);
                Os.setreuid(ROOT_UID, UNPRIVILEGED_UID);
            } catch (ErrnoException ex) {
                throw new RuntimeException("Failed to drop root", ex);
            }

            droppedPriviliges = true;
        }

        // Alter the target heap utilization.  With explicit GCs this
        // is not likely to have any effect.
        float defaultUtilization = runtime.getTargetHeapUtilization();
        runtime.setTargetHeapUtilization(0.8f);

        try {
            BufferedReader br
                = new BufferedReader(new InputStreamReader(is), 256);

            int count = 0;
            String line;
            // 开始读
            while ((line = br.readLine()) != null) {
                // Skip comments and blank lines.
                line = line.trim();
   
                // 跳空注释，和空白行
                if (line.startsWith("#") || line.equals("")) {
                    continue;
                }

                try {
                    if (false) {
                        Log.v(TAG, "Preloading " + line + "...");
                    }
                    // Load and explicitly initialize the given class. Use
                    // Class.forName(String, boolean, ClassLoader) to avoid repeated stack lookups
                    // (to derive the caller's class-loader). Use true to force initialization, and
                    // null for the boot classpath class-loader (could as well cache the
                    // class-loader of this class in a variable).
                    Class.forName(line, true, null);
                    count++;
                } catch (ClassNotFoundException e) {
                    Log.w(TAG, "Class not found for preloading: " + line);
                } catch (UnsatisfiedLinkError e) {
                    Log.w(TAG, "Problem preloading " + line + ": " + e);
                } catch (Throwable t) {
                    Log.e(TAG, "Error preloading " + line + ".", t);
                    if (t instanceof Error) {
                        throw (Error) t;
                    }
                    if (t instanceof RuntimeException) {
                        throw (RuntimeException) t;
                    }
                    throw new RuntimeException(t);
                }
            }

            Log.i(TAG, "...preloaded " + count + " classes in "
                    + (SystemClock.uptimeMillis()-startTime) + "ms.");
        } catch (IOException e) {
            Log.e(TAG, "Error reading " + PRELOADED_CLASSES + ".", e);
        } finally {
            IoUtils.closeQuietly(is);
            // Restore default.
            runtime.setTargetHeapUtilization(defaultUtilization);

            // Fill in dex caches with classes, fields, and methods brought in by preloading.
            runtime.preloadDexCaches();

            // Bring back root. We'll need it later if we're in the zygote.
            if (droppedPriviliges) {
                try {
                    Os.setreuid(ROOT_UID, ROOT_UID);
                    Os.setregid(ROOT_GID, ROOT_GID);
                } catch (ErrnoException ex) {
                    throw new RuntimeException("Failed to restore root", ex);
                }
            }
        }
    }
```

我规矩，先来翻译一下注释

> 执行Zygote进程的初始化，加载一起初始化共用的类 大多数类只分配几百个字节，但是有极少的几个了类，将会分配几千个字节(个别有大于500K的)

代码很简单，我将上面的代码内容分为三块

-   找到装载 “预加载类” 的文件
-   读取“预加载类” 的文件里面内容
-   调用Class.forName()方法来加载类。(Class的forName()方法只会装载Java类的信息，并不会创建一个类的对象。它是一个一个本地方法，最终调用native层的dvmFindClassByName()函数来完成装载过程)

通过上面代码，我们知道，Android把预加载的类放到一个文件中，这个文件是PRELOADED\_CLASSES，那么这个文件在哪？ 如下，在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined) 97行

```
    /**
     * The path of a file that contains classes to preload.
     */
    private static final String PRELOADED_CLASSES = "/system/etc/preloaded-classes";";
```

我们知道在是/system/etc/preloaded-classes

> PS：这里是硬件设备上的目录地址，不是源码的地址。

这个文件位于设备上的framework.jar里面。位置在[/frameworks/base/preloaded-classes](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fpreloaded-classes&objectId=1199510&objectType=1&isNewArticle=undefined)，一共合计3832行，我就不全部粘贴，上面有链接，大家可以自行去看。

##### (二) 预加载资源

我们先来看下preloadResources函数的内部实现，代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined) 326行

```
    /**
     * Load in commonly used resources, so they can be shared across
     * processes.
     *
     * These tend to be a few Kbytes, but are frequently in the 20-40K
     * range, and occasionally even larger.
     */
    private static void preloadResources() {
        // 获取虚拟机实例
        final VMRuntime runtime = VMRuntime.getRuntime();

        try {
            // 获取Resources对象
            mResources = Resources.getSystem();

            // 开始加载资源，其实是添加标志位mPreloading
            mResources.startPreloading();
            if (PRELOAD_RESOURCES) {
                Log.i(TAG, "Preloading resources...");

                long startTime = SystemClock.uptimeMillis();
  
                // 预加载图片资源
                TypedArray ar = mResources.obtainTypedArray(
                        com.android.internal.R.array.preloaded_drawables);
                int N = preloadDrawables(runtime, ar);
                ar.recycle();
                Log.i(TAG, "...preloaded " + N + " resources in "
                        + (SystemClock.uptimeMillis()-startTime) + "ms.");

                startTime = SystemClock.uptimeMillis();

                // 预加载装载颜色资源
                ar = mResources.obtainTypedArray(
                        com.android.internal.R.array.preloaded_color_state_lists);
                N = preloadColorStateLists(runtime, ar);
                ar.recycle();
                Log.i(TAG, "...preloaded " + N + " resources in "
                        + (SystemClock.uptimeMillis()-startTime) + "ms.");
            }
 
            // 结束加载资源，其实是删除标志位mPreloading
            mResources.finishPreloading();
        } catch (RuntimeException e) {
            Log.w(TAG, "Failure preloading resources", e);
        }
    }
```

老规矩，先来翻译一下注释

> 加载常用资源，以便跨进程使用 往往只有几K字节，偶尔有20-40K，有时会更大

我将上面代码大致分为3个部分，如下：

-   1 调用Resources.getSystem()获取Resources对象。该方法是一个androidSDK 公开的方法，但一般在应用开发中较少用到，因为该方法返回的是Resource对象仅能访问framework的资源
-   2、调用mResources.startPreloading()和mResources.finishPreloading()分别在开始和结束的时候重置加载标志mPreloading，这个标志位在Resources.loadDrawable()方法中将起到关键性作用，区别是否zygote进程预加载资源
-   3、调用preloadDrawables()和preloadColorStateLists()分别加载res/values/array.xml数组preload\_drawable、preload\_color\_states\_list中定义的资源。

在源码目录[frameworks/base/core/res/res/values/arrays.xml)](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fres%252Fres%252Fvalues%252Farrays.xml&objectId=1199510&objectType=1&isNewArticle=undefined) 下，里面定义了**preloaded\_drawables**和**preloaded\_color\_state\_lists**两个数组，代码就不粘贴了，大家自行去查看，这两个数组正式需要预加载的图片资源和状态颜色资源。

##### (三) 预加载OpenGL资源

我们先来看下preloadOpenGL函数的内部实现，代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined) 200行

```
    private static void preloadOpenGL() {
        //调用系统属性中是否禁止了预加载openGL的预加载
        if (!SystemProperties.getBoolean(PROPERTY_DISABLE_OPENGL_PRELOADING, false)) {
            EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
        }
    }
```

代码很简单，如果允许预加载openGL，则调用EGL14.eglGetDisplay来预加载openGL。

##### (四) 预加载共享库

我们先来看下preloadOpenGL函数的内部实现，代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined) 193行

```
    private static void preloadSharedLibraries() {
        Log.i(TAG, "Preloading shared libraries...");
        System.loadLibrary("android");
        System.loadLibrary("compiler_rt");
        System.loadLibrary("jnigraphics");
    }
```

从代码中，我们看到这里加载了libandroid.so,libcomiler\_rt.so，libjnigraphics.so三个文件

##### (五) 预加载文本资源

我们先来看下preloadOpenGL函数的内部实现，代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined) 206行

```
    private static void preloadTextResources() {
        Hyphenator.init();
    }
```

我们是通过Hyphenator的静态函数init来完成文件初始化的

##### (六) 初始化WebView

我们先来看下WebViewFactory的prepareWebViewInZygote()函数的内部实现，代码在[WebViewFactory.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fandroid%252Fwebkit%252FWebViewFactory.java&objectId=1199510&objectType=1&isNewArticle=undefined) 243行

```
    /**
     * Perform any WebView loading preparations that must happen in the zygote.
     * Currently, this means allocating address space to load the real JNI library later.
     */
    public static void prepareWebViewInZygote() {
        try {
            // 加载libwebviewchromium_loader.so
            System.loadLibrary("webviewchromium_loader");

            // 通过系统属性获取地址空间
            long addressSpaceToReserve =
                    SystemProperties.getLong(CHROMIUM_WEBVIEW_VMSIZE_SIZE_PROPERTY,
                    CHROMIUM_WEBVIEW_DEFAULT_VMSIZE_BYTES);
            sAddressSpaceReserved = nativeReserveAddressSpace(addressSpaceToReserve);

            if (sAddressSpaceReserved) {
                // 获取地址
                if (DEBUG) {
                    Log.v(LOGTAG, "address space reserved: " + addressSpaceToReserve + " bytes");
                }
            } else {
                Log.e(LOGTAG, "reserving " + addressSpaceToReserve +
                        " bytes of address space failed");
            }
        } catch (Throwable t) {
            // Log and discard errors at this stage as we must not crash the zygote.
            Log.e(LOGTAG, "error preparing native loader", t);
        }
    }
```

先看下注释

> 开始WebView的准备工作，这个方法只能被zygote调用，所以先分配地址空间，然后加载真正的JNI库

所以WebViewFactory类的静态成员方法prepareWebViewInZygote首先会记载一个名称Wie"webviewchromium\_loader"的动态库，然后又会获得需要为Chromium动态库预留的地址空间大小addressSpaceToReserve。知道了要预留的地址空间的大小之后，WebViewFactory类的静态成员方法prepareWebViewInZygote又会调用另外一个静态成员方法nativeReserveAddressSpace为Chromium动态库预留地址空间。

所以说WebViewFactory.prepareWebViewInZygote()主要目的就是Chromium动态库预保留加载地址。

### 四、启动SystemServer

我们前面也说Zygote类的main()方法里面的**第四阶段调用startSystemServer启动系统服务**，那我们就一起来看下

代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined) 493行

```
    /**
     * Prepare the arguments and fork for the system server process.
     */
    private static boolean startSystemServer(String abiList, String socketName)
            throws MethodAndArgsCaller, RuntimeException {

        // 调用posixCapabilitiesAsBits方法获取POSIX功能列表的相关位数
        long capabilities = posixCapabilitiesAsBits(
            OsConstants.CAP_BLOCK_SUSPEND,
            OsConstants.CAP_KILL,
            OsConstants.CAP_NET_ADMIN,
            OsConstants.CAP_NET_BIND_SERVICE,
            OsConstants.CAP_NET_BROADCAST,
            OsConstants.CAP_NET_RAW,
            OsConstants.CAP_SYS_MODULE,
            OsConstants.CAP_SYS_NICE,
            OsConstants.CAP_SYS_RESOURCE,
            OsConstants.CAP_SYS_TIME,
            OsConstants.CAP_SYS_TTY_CONFIG
        );

        // 硬编码命令行启动服务器
        /* Hardcoded command line to start the system server */
        String args[] = {
            "--setuid=1000",
            "--setgid=1000",
            "--setgroups=1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1018,1021,1032,3001,3002,3003,3006,3007",
            "--capabilities=" + capabilities + "," + capabilities,
            "--nice-name=system_server",
            "--runtime-args",
            "com.android.server.SystemServer",
        };
        ZygoteConnection.Arguments parsedArgs = null;

        int pid;

        try {

            // 将上面的命令转换为Arguments对象
            parsedArgs = new ZygoteConnection.Arguments(args);

             // 设置是否所有应用都可调试
             // 将调试器 系统属性 应用于zygote参数。
             // 如果“ro.debuggable”为“1”，则所有的应用程序都是可调试的
             // 否则，调试器状态通过产生请求中的“--enable-debugger”标志指定。
            ZygoteConnection.applyDebuggerSystemProperty(parsedArgs);

            // 将系统属性应用于zygote属性
            ZygoteConnection.applyInvokeWithSystemProperty(parsedArgs);


            // 从Zygote进程fork一个system server 子进程
            /* Request to fork the system server process */
            pid = Zygote.forkSystemServer(
                    parsedArgs.uid, parsedArgs.gid,
                    parsedArgs.gids,
                    parsedArgs.debugFlags,
                    null,
                    parsedArgs.permittedCapabilities,
                    parsedArgs.effectiveCapabilities);
        } catch (IllegalArgumentException ex) {
            throw new RuntimeException(ex);
        }


         // 进入子进程system_server
        /* For child process */
        if (pid == 0) {
            if (hasSecondZygote(abiList)) {
                // 从zygote进程fork新进程后，需要关闭zygote原有socket。
                //另外，对于有连个zygote进程情况，需要等待2个zygote创建完成。
                waitForSecondaryZygote(socketName);
            }

            // 完成system server进程剩余工作
            handleSystemServerProcess(parsedArgs);
        }

        return true;
    }
```

老规矩，先来看下注释：

> 准备参数并且fork系统进程

我将这块代码分为三块内容

-   1、为fork准备参数parsedArgs
-   2、调用Zygote.forkSystemServer()方法来创建system\_server
-   3、调用handleSystemServerProcess()方法执行system\_server的剩余工作

PS：通过上面代码，我们知道system\_server进程的参数信息为uid=1000,gid=1000，进程名为sytem\_server。

所以这里有两个关键函数即Zygote.forkSystemServer()和handleSystemServerProcess()，那我们就依次来看下。

##### (一)、创建system\_server进程——Zygote.forkSystemServer()函数解析

代码在[Zygote.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygote.java&objectId=1199510&objectType=1&isNewArticle=undefined) 134

```
    /**
     * Special method to start the system server process. In addition to the
     * common actions performed in forkAndSpecialize, the pid of the child
     * process is recorded such that the death of the child process will cause
     * zygote to exit.
     *
     * @param uid the UNIX uid that the new process should setuid() to after
     * fork()ing and and before spawning any threads.
     * @param gid the UNIX gid that the new process should setgid() to after
     * fork()ing and and before spawning any threads.
     * @param gids null-ok; a list of UNIX gids that the new process should
     * setgroups() to after fork and before spawning any threads.
     * @param debugFlags bit flags that enable debugging features.
     * @param rlimits null-ok an array of rlimit tuples, with the second
     * dimension having a length of 3 and representing
     * (resource, rlim_cur, rlim_max). These are set via the posix
     * setrlimit(2) call.
     * @param permittedCapabilities argument for setcap()
     * @param effectiveCapabilities argument for setcap()
     *
     * @return 0 if this is the child, pid of the child
     * if this is the parent, or -1 on error.
     */
    public static int forkSystemServer(int uid, int gid, int[] gids, int debugFlags,
            int[][] rlimits, long permittedCapabilities, long effectiveCapabilities) {
        VM_HOOKS.preFork();
        int pid = nativeForkSystemServer(
                uid, gid, gids, debugFlags, rlimits, permittedCapabilities, effectiveCapabilities);
        // Enable tracing as soon as we enter the system_server.
        if (pid == 0) {
            Trace.setTracingEnabled(true);
        }
        VM_HOOKS.postForkCommon();
        return pid;
    }
```

哎，我就是喜欢有注释的代码，先来翻译下注释

> 专门用来启动系统服务(system\_server)的方法。除了forkAndSpecialize方法中的执行的常见操作之外，还会记录子进程的pid，这样在子进程死亡就会方便zygote退出

-   入参uid：UNIX新的进程的uid应该在fork()方法调用之后，并且在产生任何线程之前调用setuid()来设置uid的值
-   入参gid：UNIX新的进程的gid应该在fork()方法调用之后，并且在产生任何线程之前调用setgid()来设置uid的值
-   入参gids：UNIX新的进程组的gids应该在fork()方法调用之后，并且在产生任何线程之前调用setgroups()来设置uid的值
-   入参debugFlags： 启动debug调试功能的标志
-   入参rlimits：int类型的二维数组，第二维的长度为3，表示resource、rlim\_cur、rlim\_max。通过posix的setrlimit(2)调用设置的
-   入参permittedCapabilities：是setcap()方法用到的参数
-   入参effectiveCapabilities：是setcap()方法用到的参数
-   返回值：如果是子线程，pid为0，如果不是子线程是父线程则返回-1

这个方法内部很简单，先调用几个方法而已，主要是调用nativeForkSystemServer方法，通过C层来实现创建system\_server进程

在讲解nativeForkSystemServer之前，我们先来看下**VM\_HOOKS.preFork();**，**VM\_HOOKS.postForkCommon();**方法的实现

###### 1、VM\_HOOKS.preFork()与VM\_HOOKS.postForkCommon()方法解析

代码在[ZygoteHooks.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Flibcore%252Fdalvik%252Fsrc%252Fmain%252Fjava%252Fdalvik%252Fsystem%252FZygoteHooks.java&objectId=1199510&objectType=1&isNewArticle=undefined) 里面

```
30    /**
31     * Called by the zygote prior to every fork. Each call to {@code preFork}
32     * is followed by a matching call to {@link #postForkChild(int, String)} on the child
33     * process and {@link #postForkCommon()} on both the parent and the child
34     * process. {@code postForkCommon} is called after {@code postForkChild} in
35     * the child process.
36     */
37    public void preFork() {
            // 停止4个Daemon子线程，里面包括：
            // HeapTaskDaemon.INSTANCE.stop();Java堆整理线程
            / /ReferenceQueueDaemon.INSTANCE.stop(); 引用队列线程
            // FinalizerDaemon.INSTANCE.stop(); 析构线程
            // FinalizerWatchdogDaemon.INSTANCE.stop(); 析构监控线程
38        Daemons.stop();
            // 等待所有子线程结束
39        waitUntilAllThreadsStopped();
            // 完成gc堆的初始化
40        token = nativePreFork();
41    }


......

54    /**
55     * Called by the zygote in both the parent and child processes after
56     * every fork. In the child process, this method is called after
57     * {@code postForkChild}.
58     */
59    public void postForkCommon() {
            // 启动Zygote的4个Daemon线程，Java堆整理，引用队列，以及析构线程
60        Daemons.start();
61    }
```

-   VM\_HOOKS.preFork()这个方法的主要功能是停止Zygote的4个Daemon子线程的运行，等待并确保Zygote的单线程(用于fork效率)，并等待这些线程的停止，初始化gc堆的工作。

> Zygote进程的4个Daemon子线程分别是**ReferenceQueueDaemon**、**FinalizerDaemon**、**FinalizerWatchdogDaemon**、**HeapTaskDaemon**，此处称为Zygote的4个Daemon子线程。

-   VM\_HOOKS.postForkCommon()这个方法的主要功能是在fork新进程后，启动Zygote的4个Deamon线程，Java堆整理，引用队列，以及析构线程。

了解完VM\_HOOKS.preFork()与VM\_HOOKS.postForkCommon()方法后，我们来看下nativeForkSystemServer()方法的实现

###### 2、nativeForkSystemServer方法解析

我们看到nativeForkSystemServer方法是一个native方法，根据我们之前的学习，代码如下 [Zygote.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygote.java&objectId=1199510&objectType=1&isNewArticle=undefined) 147行

```
    native private static int nativeForkSystemServer(int uid, int gid, int[] gids, int debugFlags,
            int[][] rlimits, long permittedCapabilities, long effectiveCapabilities);
```

对应的JNI函数如下：代码在[com\_android\_internal\_os\_Zygote.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjni%252Fcom_android_internal_os_Zygote.cpp&objectId=1199510&objectType=1&isNewArticle=undefined)

```
625static jint com_android_internal_os_Zygote_nativeForkSystemServer(
626        JNIEnv* env, jclass, uid_t uid, gid_t gid, jintArray gids,
627        jint debug_flags, jobjectArray rlimits, jlong permittedCapabilities,
628        jlong effectiveCapabilities) {
      // fork 子子进程
629  pid_t pid = ForkAndSpecializeCommon(env, uid, gid, gids,
630                                      debug_flags, rlimits,
631                                      permittedCapabilities, effectiveCapabilities,
632                                      MOUNT_EXTERNAL_DEFAULT, NULL, NULL, true, NULL,
633                                      NULL, NULL);
      // zygote进程，检测system_server进程是否创建
634  if (pid > 0) {
635      // The zygote process checks whether the child process has died or not.
636      ALOGI("System server process %d has been created", pid);
637      gSystemServerPid = pid;
638      // There is a slight window that the system server process has crashed
639      // but it went unnoticed because we haven't published its pid yet. So
640      // we recheck here just to make sure that all is well.
641      int status;
642      if (waitpid(pid, &status, WNOHANG) == pid) {
643          ALOGE("System server process %d has died. Restarting Zygote!", pid);
             // 当system_server进程死亡后，重启zygote进程 
644          RuntimeAbort(env);
645      }
646  }
647  return pid;
648}
```

通过上面的代码，我们知道，该块代码主要分为两部分

-   1、调用ForkAndSpecializeCommon函数来fork子进程
-   2、zygote进程检测

先来说下检测，当system\_server进程创建失败时，将会重启zygote进程。这里需要注意，对于Android 5.0以后，有两个进程，一个是zyogetz进程，一个是zygote64个进程，system\_server的父进程，一般来说64位系统其父进程是zygote64进程。说一下杀进程的情况：

-   当杀system\_server进城后，只重启zygote64和system\_server，不重启zygote
-   当杀 zygote64进程后，只重启zygote64和system\_server，也不重启zygote
-   当杀 zygoet进程后，则重启zygote、zygoet64以及system\_server。

###### 3、ForkAndSpecializeCommon函数解析

下面我们来看下ForkAndSpecializeCommon函数的实现代码[com\_android\_internal\_os\_Zygote.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjni%252Fcom_android_internal_os_Zygote.cpp&objectId=1199510&objectType=1&isNewArticle=undefined)

```
442// Utility routine to fork zygote and specialize the child process.
443static pid_t ForkAndSpecializeCommon(JNIEnv* env, uid_t uid, gid_t gid, jintArray javaGids,
444                                     jint debug_flags, jobjectArray javaRlimits,
445                                     jlong permittedCapabilities, jlong effectiveCapabilities,
446                                     jint mount_external,
447                                     jstring java_se_info, jstring java_se_name,
448                                     bool is_system_server, jintArray fdsToClose,
449                                     jstring instructionSet, jstring dataDir) {
//************************** 第1步 **************************
       // 设置子进程的signal信号处理函数
       // 如果子进程system_server如果挂了，那么Zygote会调用kill函数把自己杀了
450  SetSigChldHandler();
451
452#ifdef ENABLE_SCHED_BOOST
453  SetForkLoad(true);
454#endif
455
//************************** 第2步 **************************
        // fork 子进程
456  pid_t pid = fork();
457
458  if (pid == 0) {
459    // The child process.
460    gMallocLeakZygoteChild = 1;
461
462    // Clean up any descriptors which must be closed immediately
关闭并清除文件描述符
          // 关闭并清除文件描述符
463    DetachDescriptors(env, fdsToClose);
464
465    // Keep capabilities across UID change, unless we're staying root.
466    if (uid != 0) {
            // 非 root用户，禁止动态改变进程的权限
467      EnableKeepCapabilities(env);
468    }
469
          // 取消进程的已有Capablilities权限
470    DropCapabilitiesBoundingSet(env);
471
          // 检测是否需要native_bridge
472    bool use_native_bridge = !is_system_server && (instructionSet != NULL)
473        && android::NativeBridgeAvailable();
474    if (use_native_bridge) {
475      ScopedUtfChars isa_string(env, instructionSet);
476      use_native_bridge = android::NeedsNativeBridge(isa_string.c_str());
477    }
478    if (use_native_bridge && dataDir == NULL) {
479      // dataDir should never be null if we need to use a native bridge.
480      // In general, dataDir will never be null for normal applications. It can only happen in
481      // special cases (for isolated processes which are not associated with any app). These are
482      // launched by the framework and should not be emulated anyway.
483      use_native_bridge = false;
484      ALOGW("Native bridge will not be used because dataDir == NULL.");
485    }
486
//************************** 第3步 **************************
         // 挂载 external storage
487    if (!MountEmulatedStorage(uid, mount_external, use_native_bridge)) {
            //mount命名空间
488      ALOGW("Failed to mount emulated storage: %s", strerror(errno));
489      if (errno == ENOTCONN || errno == EROFS) {
490        // When device is actively encrypting, we get ENOTCONN here
491        // since FUSE was mounted before the framework restarted.
492        // When encrypted device is booting, we get EROFS since
493        // FUSE hasn't been created yet by init.
494        // In either case, continue without external storage.
495      } else {
496        ALOGE("Cannot continue without emulated storage");
497        RuntimeAbort(env);
498      }
499    }
500
          // 对于非system_server子进程，则创建进程组
501    if (!is_system_server) {
502        int rc = createProcessGroup(uid, getpid());
503        if (rc != 0) {
504            if (rc == -EROFS) {
505                ALOGW("createProcessGroup failed, kernel missing CONFIG_CGROUP_CPUACCT?");
506            } else {
507                ALOGE("createProcessGroup(%d, %d) failed: %s", uid, pid, strerror(-rc));
508            }
509        }
510    }
511
//************************** 第4步 **************************
          // 设置group id
512    SetGids(env, javaGids);
513
//************************** 第5步 **************************
          // 设置资源limit，javaRlimits等于null，不限制
514    SetRLimits(env, javaRlimits);
515
516    if (use_native_bridge) {
517      ScopedUtfChars isa_string(env, instructionSet);
518      ScopedUtfChars data_dir(env, dataDir);
519      android::PreInitializeNativeBridge(data_dir.c_str(), isa_string.c_str());
520    }
521
          // 分别设置真实的、有效的、保存过的group标示号
522    int rc = setresgid(gid, gid, gid);
523    if (rc == -1) {
524      ALOGE("setresgid(%d) failed: %s", gid, strerror(errno));
525      RuntimeAbort(env);
526    }
527
          // 分别设置真实的、有效的 和保存过的用户标示号
528    rc = setresuid(uid, uid, uid);
529    if (rc == -1) {
530      ALOGE("setresuid(%d) failed: %s", uid, strerror(errno));
531      RuntimeAbort(env);
532    }
533
        // 处理解ARM内核ASLR损失
534    if (NeedsNoRandomizeWorkaround()) {
535        // Work around ARM kernel ASLR lossage (http://b/5817320).
536        int old_personality = personality(0xffffffff);
537        int new_personality = personality(old_personality | ADDR_NO_RANDOMIZE);
538        if (new_personality == -1) {
539            ALOGW("personality(%d) failed: %s", new_personality, strerror(errno));
540        }
541    }
542
//************************** 第6步 **************************
          // 设置Capabilities进程权限
543    SetCapabilities(env, permittedCapabilities, effectiveCapabilities);
544
//************************** 第7步 **************************
          // 设置调度策略
545    SetSchedulerPolicy(env);
546
547    const char* se_info_c_str = NULL;
548    ScopedUtfChars* se_info = NULL;
549    if (java_se_info != NULL) {
550        se_info = new ScopedUtfChars(env, java_se_info);
551        se_info_c_str = se_info->c_str();
552        if (se_info_c_str == NULL) {
553          ALOGE("se_info_c_str == NULL");
554          RuntimeAbort(env);
555        }
556    }
557    const char* se_name_c_str = NULL;
558    ScopedUtfChars* se_name = NULL;
559    if (java_se_name != NULL) {
560        se_name = new ScopedUtfChars(env, java_se_name);
561        se_name_c_str = se_name->c_str();
562        if (se_name_c_str == NULL) {
563          ALOGE("se_name_c_str == NULL");
564          RuntimeAbort(env);
565        }
566    }
//************************** 第8步 **************************
          // selinux上下文
567    rc = selinux_android_setcontext(uid, is_system_server, se_info_c_str, se_name_c_str);
568    if (rc == -1) {
569      ALOGE("selinux_android_setcontext(%d, %d, \"%s\", \"%s\") failed", uid,
570            is_system_server, se_info_c_str, se_name_c_str);
571      RuntimeAbort(env);
572    }
573
574    // Make it easier to debug audit logs by setting the main thread's name to the
575    // nice name rather than "app_process".
          // 设置线程的的名字为system_server
576    if (se_info_c_str == NULL && is_system_server) {
577      se_name_c_str = "system_server";
578    }
579    if (se_info_c_str != NULL) {
580      SetThreadName(se_name_c_str);
581    }
582
583    delete se_info;
584    delete se_name;
585
//************************** 第9步 **************************
       // 在Zygote子进程中，设置信号SIGCHLD的处理器回复默认行为
586    UnsetSigChldHandler();
587
//************************** 第10步 **************************
      // 等价于调用zygote.callPostForkChildHooks()
      // 完成一些运行时的后期工作
588    env->CallStaticVoidMethod(gZygoteClass, gCallPostForkChildHooks, debug_flags,
589                              is_system_server ? NULL : instructionSet);
590    if (env->ExceptionCheck()) {
591      ALOGE("Error calling post fork hooks.");
592      RuntimeAbort(env);
593    }
594  } else if (pid > 0) {
       // 进入父进程，即Zygote64进程
595    // the parent process
596
597#ifdef ENABLE_SCHED_BOOST
598    // unset scheduler knob
599    SetForkLoad(false);
600#endif
601
602  }
603  return pid;
604}
```

我将上面代码的整体分为7个部分如下：

-   第1步：设置子进程的signal信号处理函数
-   第2步：fork子进程
-   第3步：在子进程挂载external storage
-   第4步：在子进程设置用户Id、组Id和进程所属的组
-   第5步：在在进程执行系统调用setrlimit来设置进程的系统资源限制
-   第6步：在子进程调用SetCapabilities()函数并在其中执行系统调动系统调capset来设置进程的权限
-   第7步：在子进程调用SetSchedulerPolicy()函数并在其中执行系统调动系统调set\_sched\_policy来设置调度策略
-   第8步：在子进程设置应用进程的安全上下文
-   第9步：回复signal信号处理函数
-   第10步：完成一些运行时后的工作

这里面有三个核心函数，即**SetSigChldHandler()与UnsetSigChldHandler()函数**、\*\* fork()函数**和**zygote.callPostForkChildHooks()函数\*\*，那我们来依次看下

###### 3.1、SetSigChldHandler()与UnsetSigChldHandler()函数解析

在[com\_android\_internal\_os\_Zygote.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjni%252Fcom_android_internal_os_Zygote.cpp&objectId=1199510&objectType=1&isNewArticle=undefined)里面

```
133// Configures the SIGCHLD handler for the zygote process. This is configured
134// very late, because earlier in the runtime we may fork() and exec()
135// other processes, and we want to waitpid() for those rather than
136// have them be harvested immediately.
137//
138// This ends up being called repeatedly before each fork(), but there's
139// no real harm in that.
140static void SetSigChldHandler() {
141  struct sigaction sa;
142  memset(&sa, 0, sizeof(sa));
143  sa.sa_handler = SigChldHandler;
144
      // 设置信号处理函数，SIGCHLD是子进程终止的信号
145  int err = sigaction(SIGCHLD, &sa, NULL);
146  if (err < 0) {
147    ALOGW("Error setting SIGCHLD handler: %s", strerror(errno));
148  }
149}
150
151// Sets the SIGCHLD handler back to default behavior in zygote children.
152static void UnsetSigChldHandler() {
153  struct sigaction sa;
154  memset(&sa, 0, sizeof(sa));
155  sa.sa_handler = SIG_DFL;
156
157  int err = sigaction(SIGCHLD, &sa, NULL);
158  if (err < 0) {
159    ALOGW("Error unsetting SIGCHLD handler: %s", strerror(errno));
160  }
161}
```

通过上面代码，我们发现**SetSigChldHandler**函数与**UnsetSigChldHandler**的区别就1处，即**SetSigChldHandler**里面的sa.sa\_handle是**SigChldHandler**，而**UnsetSigChldHandler**里面 sa.sa\_handler是**SIG\_DFL**。而**SigChldHandler**是com\_android\_internal\_os\_Zygote.cpp的一个方法，那**SIG\_DFL**是什么，**SIG\_DFL**是SIGCHLD下的一种处理方式，**SIG\_DFL**表示默认信号处理程序，与之对应的是**SIG\_IGN**表示葫芦信号处理程序。

那我们来看下SigChldHandler方法的内部实现 在[com\_android\_internal\_os\_Zygote.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjni%252Fcom_android_internal_os_Zygote.cpp&objectId=1199510&objectType=1&isNewArticle=undefined)里面

```
81// This signal handler is for zygote mode, since the zygote must reap its children
82static void SigChldHandler(int /*signal_number*/) {
83  pid_t pid;
84  int status;
85
86  // It's necessary to save and restore the errno during this function.
87  // Since errno is stored per thread, changing it here modifies the errno
88  // on the thread on which this signal handler executes. If a signal occurs
89  // between a call and an errno check, it's possible to get the errno set
90  // here.
91  // See b/23572286 for extra information.
92  int saved_errno = errno;
93
   // zygote监听所有子进程的死亡
94  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
95     // Log process-death status that we care about.  In general it is
96     // not safe to call LOG(...) from a signal handler because of
97     // possible reentrancy.  However, we know a priori that the
98     // current implementation of LOG() is safe to call from a SIGCHLD
99     // handler in the zygote process.  If the LOG() implementation
100     // changes its locking strategy or its use of syscalls within the
101     // lazy-init critical section, its use here may become unsafe.
         //某一个子进程退出了
102    if (WIFEXITED(status)) {
103      if (WEXITSTATUS(status)) {
104        ALOGI("Process %d exited cleanly (%d)", pid, WEXITSTATUS(status));
105      }
106    } else if (WIFSIGNALED(status)) {
          //某一个子进程挂了
107      if (WTERMSIG(status) != SIGKILL) {
108        ALOGI("Process %d exited due to signal (%d)", pid, WTERMSIG(status));
109      }
110      if (WCOREDUMP(status)) {
111        ALOGI("Process %d dumped core.", pid);
112      }
113    }
114
115    // If the just-crashed process is the system_server, bring down zygote
116    // so that it is restarted by init and system server will be restarted
117    // from there.
          // 如果挂掉的是system_server
118    if (pid == gSystemServerPid) {
119      ALOGE("Exit zygote because system server (%d) has terminated", pid);
         // zygote 自杀
120      kill(getpid(), SIGKILL);
121    }
122  }
123
124  // Note that we shouldn't consider ECHILD an error because
125  // the secondary zygote might have no children left to wait for.
126  if (pid < 0 && errno != ECHILD) {
127    ALOGW("Zygote SIGCHLD error in waitpid: %s", strerror(errno));
128  }
129
130  errno = saved_errno;
131}
```

说上面的代码表示当信号SIGCHILD来到的时候，会进入信号处理函数。如果子进程system\_server挂了，Zygote就会自杀，从而导致Zygote重启

###### 3.2、fork()函数解析

fork()采用的**copy on write**技术，这是linux创建进程的标准方法，调用一次，返回两次，返回值有3种类型：

-   父进程中，fork返回新创建的子进程的pid
-   子进程中，fork返回0
-   当出现错误时，fork返回负数(比如进程数量超过上限，或者内存不足时会出错)。

fork()的主要工作是寻找空闲的进程号pid，然后从父进程拷贝进程信息，例如数据段和代码段，fork()后子进程要执行的代码段等。Zygote进程是所有Android进程的母体，包括system\_server和各个App进程。zygote利用fork()方法生成新进程，对于新进程A复用Zzygote进程本身的资源，再加上新进程A相关资源，构成新的应用进程A。如下图"预加载"。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/dgwxkuqc2l.png)

预加载.png

-   **copy on write**过程：当父进程任一方修改内存数据时（这是on write实际），才发生缺页中断，从而分配新的物理内存（这是copy操作）。
-   **copy on write**过程：写拷贝是指子进程与父进程的页表都指向同一块物理内存，fork过程拷贝父进程的页表，并标记这些页表是只读的。父进程共用同一份物理内存，如果父进程任一方想要修改这块物理内存，就会触发缺页异常(page fault)，Linux收到该中断便会创建新的物理内存，并将两个物理内存标记设置为可写状态，从而父子进程都有各自的独立的物理内存

现在我们来看下fork函数的具体实现 在[fork.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fbionic%252Flibc%252Fbionic%252Ffork.cpp&objectId=1199510&objectType=1&isNewArticle=undefined)

```
29#include <unistd.h>
30#include <sys/syscall.h>
31
32#include "pthread_internal.h"
33
34#define FORK_FLAGS (CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD)
35
36int fork() {
   // fork前，父进程的回调方法
37  __bionic_atfork_run_prepare();
38
39  pthread_internal_t* self = __get_thread();
40
41  // Remember the parent pid and invalidate the cached value while we fork.
    // fork期间，获取父进程pid，并使其缓存值无效
42  pid_t parent_pid = self->invalidate_cached_pid();
43
44#if defined(__x86_64__) // sys_clone's last two arguments are flipped on x86-64.
45  int result = syscall(__NR_clone, FORK_FLAGS, NULL, NULL, &(self->tid), NULL);
46#else
47  int result = syscall(__NR_clone, FORK_FLAGS, NULL, NULL, NULL, &(self->tid));
48#endif
49  if (result == 0) {
50    self->set_cached_pid(gettid());
     // fork完成执行子进程回调方法
51    __bionic_atfork_run_child();
52  } else {
53    self->set_cached_pid(parent_pid);
     // fork完成执行父进程的回调方法
54    __bionic_atfork_run_parent();
55  }
56  return result;
57}
```

在执行syscal的前后，都会有相应的回调方法：

-   \_\_bionic\_atfork\_run\_prepare：fork完成前，父进程的回调方法
-   \_\_bionic\_atfork\_run\_child：fork完成后，子进程回调方法
-   \_\_bionic\_atfork\_run\_paren：fork完成后，父进程回调方法

以上3个方法的实现都位于bionic/pthread\_atfork.cpp。如果有需要，可以扩展该回调方法，添加相关的业务需求。

###### 3.3、Zygote.callPostForkChildHooks()函数解析

代码在[Zygote.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygote.java&objectId=1199510&objectType=1&isNewArticle=undefined)

```
150    private static void callPostForkChildHooks(int debugFlags, String instructionSet) {
151        VM_HOOKS.postForkChild(debugFlags, instructionSet);
152    }
```

那我们继续跟踪，看到在Zygote的callPostForkChildHooks()方法里面，调用的是ZygoteHooks类的postForkChild()方法，那我们就继续跟踪。来看下postForkChild(int,String)的内部实现

> 在这里，设置了新进程Random随机数种子为当前系统时间，也就是在进程创建的那一刻就决定了未来随机数的情况，也就是伪随机。

代码在[ZygoteHooks.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Flibcore%252Fdalvik%252Fsrc%252Fmain%252Fjava%252Fdalvik%252Fsystem%252FZygoteHooks.java&objectId=1199510&objectType=1&isNewArticle=undefined)中 43行

```
43    /**
44     * Called by the zygote in the child process after every fork. The debug
45     * flags from {@code debugFlags} are applied to the child process. The string
46     * {@code instructionSet} determines whether to use a native bridge.
47     */
48    public void postForkChild(int debugFlags, String instructionSet) {
49        nativePostForkChild(token, debugFlags, instructionSet);
50
51        Math.setRandomSeedInternal(System.currentTimeMillis());
52    }
```

先来看下注释，简单翻译一下

> 在子进程被fork后，在子进程中被zygote调用。

-   入参debugFlags 标志：表示否是应用在debug子进程
-   入参instructionSet 标志：表示是否使用 native bridge

我们看到在postForkChild(int,String)内部代码很简单就是调用了nativePostForkChild这个方法，通过方法名，我们知道它是一个native函数，所以我们继续跟踪

[dalvik\_system\_ZygoteHooks.cc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fart%252Fruntime%252Fnative%252Fdalvik_system_ZygoteHooks.cc&objectId=1199510&objectType=1&isNewArticle=undefined)

```
144static void ZygoteHooks_nativePostForkChild(JNIEnv* env, jclass, jlong token, jint debug_flags,
145                                            jstring instruction_set) {
146  Thread* thread = reinterpret_cast<Thread*>(token);
147  // Our system thread ID, etc, has changed so reset Thread state.
     // 设置新进程的主线程id
148  thread->InitAfterFork();
149  EnableDebugFeatures(debug_flags);
150
151  // Update tracing.
152  if (Trace::GetMethodTracingMode() != TracingMode::kTracingInactive) {
153    Trace::TraceOutputMode output_mode = Trace::GetOutputMode();
154    Trace::TraceMode trace_mode = Trace::GetMode();
155    size_t buffer_size = Trace::GetBufferSize();
156
157    // Just drop it.
158    Trace::Abort();
159
160    // Only restart if it was streaming mode.
161    // TODO: Expose buffer size, so we can also do file mode.
162    if (output_mode == Trace::TraceOutputMode::kStreaming) {
163      const char* proc_name_cutils = get_process_name();
164      std::string proc_name;
165      if (proc_name_cutils != nullptr) {
166        proc_name = proc_name_cutils;
167      }
168      if (proc_name_cutils == nullptr || proc_name == "zygote" || proc_name == "zygote64") {
169        // Either no process name, or the name hasn't been changed, yet. Just use pid.
170        pid_t pid = getpid();
171        proc_name = StringPrintf("%u", static_cast<uint32_t>(pid));
172      }
173
174      std::string profiles_dir(GetDalvikCache("profiles", false /* create_if_absent */));
175      if (!profiles_dir.empty()) {
176        std::string trace_file = StringPrintf("%s/%s.trace.bin", profiles_dir.c_str(),
177                                              proc_name.c_str());
178        Trace::Start(trace_file.c_str(),
179                     -1,
180                     buffer_size,
181                     0,   // TODO: Expose flags.
182                     output_mode,
183                     trace_mode,
184                     0);  // TODO: Expose interval.
185        if (thread->IsExceptionPending()) {
186          ScopedObjectAccess soa(env);
187          thread->ClearException();
188        }
189      } else {
190        LOG(ERROR) << "Profiles dir is empty?!?!";
191      }
192    }
193  }
194
195  if (instruction_set != nullptr) {
196    ScopedUtfChars isa_string(env, instruction_set);
197    InstructionSet isa = GetInstructionSetFromString(isa_string.c_str());
198    Runtime::NativeBridgeAction action = Runtime::NativeBridgeAction::kUnload;
199    if (isa != kNone && isa != kRuntimeISA) {
200      action = Runtime::NativeBridgeAction::kInitialize;
201    }
202    Runtime::Current()->DidForkFromZygote(env, action, isa_string.c_str());
203  } else {
204    Runtime::Current()->DidForkFromZygote(env, Runtime::NativeBridgeAction::kUnload, nullptr);
205  }
206}
```

本快代码有两个核心函数，即48行的**thread->InitAfterFork();**和202行的\*\* DidForkFromZygote()\*\*。其中thread->InitAfterFork()具体实现在 [thread.cc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fart%252Fruntime%252Fthread.cc&objectId=1199510&objectType=1&isNewArticle=undefined) 232行。

那我们来看下DidForkFromZygote函数的实现。他在[runtime.cc](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fart%252Fruntime%252Fruntime.cc&objectId=1199510&objectType=1&isNewArticle=undefined)

```
633void Runtime::DidForkFromZygote(JNIEnv* env, NativeBridgeAction action, const char* isa) {
634  is_zygote_ = false;
635
636  if (is_native_bridge_loaded_) {
637    switch (action) {
638      case NativeBridgeAction::kUnload:
           // 卸载用于跨平台的桥连库 也就是native bridge
639        UnloadNativeBridge();
640        is_native_bridge_loaded_ = false;
641        break;
642
643      case NativeBridgeAction::kInitialize:
               // 初始化跨平台桥 也就是native bridge
644        InitializeNativeBridge(env, isa);
645        break;
646    }
647  }
648
649  // Create the thread pools.
        // 创建Java堆处理的线程池
650  heap_->CreateThreadPool();
651  // Reset the gc performance data at zygote fork so that the GCs
652  // before fork aren't attributed to an app.
      // 重置gc性能数据，以保证进程在创建之前的GCs不会计算到当前app上
653  heap_->ResetGcPerformanceInfo();
654
655  if (jit_.get() == nullptr && jit_options_->UseJIT()) {
656    // Create the JIT if the flag is set and we haven't already create it (happens for run-tests).
          // 当flag被设置，并且还没有创建JIT时，则创建JIT
657    CreateJit();
658  }
659
     // 设置信号处理函数
660  StartSignalCatcher();
661
662  // Start the JDWP thread. If the command-line debugger flags specified "suspend=y",
663  // this will pause the runtime, so we probably want this to come last.
     // 启动JDWP线程，当命令debug的flags指定"suspend=y"是，则暂停runtime
664  Dbg::StartJdwp();
665}
```

###### 3.4、ForkAndSpecializeCommon()小结

至此整个\*\* ForkAndSpecializeCommon\*\*解析完毕，我们来小结一下

该方法主要功能：

-   preFork：停止Zyote的4个Daemon子线程的运行，初始化gc堆
-   nativeForkAndSpecialize：调用fork()创建新基础讷航，设置新进程的主线程id，重置gc堆性能数据，设置信号处理函数等功能
-   postForkCommon：启动4个Deamon子线程

其调用关系链：

```
Zygote.forkAndSpecialize
    ZygoteHooks.preFork
        Daemons.stop
        ZygoteHooks.nativePreFork
            dalvik_system_ZygoteHooks.ZygoteHooks_nativePreFork
                Runtime::PreZygoteFork
                    heap_->PreZygoteFork()
    Zygote.nativeForkAndSpecialize
        com_android_internal_os_Zygote.ForkAndSpecializeCommon
            fork()
            Zygote.callPostForkChildHooks
                ZygoteHooks.postForkChild
                    dalvik_system_ZygoteHooks.nativePostForkChild
                        Runtime::DidForkFromZygote
    ZygoteHooks.postForkCommon
        Daemons.start
```

时序图如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/i5z7871foi.png)

image.png

到进程已经完了创建成system server进程的大部分工作，接下来就是开始system server进程的剩余工作，在 handleSystemServerProcess(parsedArgs)函数里面实现的。

##### (二)、初始化system\_server进程——handleSystemServerProcess()函数解析

代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined)

```
412    /**
413     * Finish remaining work for the newly forked system server process.
414     */
415    private static void handleSystemServerProcess(
416            ZygoteConnection.Arguments parsedArgs)
417            throws ZygoteInit.MethodAndArgsCaller {
418
            // 在fork过程中复制了原来位于zygote进程的socket服务端，这里关闭了从父进程复制而来的socket
419        closeServerSocket();
420

           // 通过umask设置创建文件的默认权限
421        // set umask to 0077 so new files and directories will default to owner-only permissions.
422        Os.umask(S_IRWXG | S_IRWXO);
423
424        if (parsedArgs.niceName != null) {
              // 设置进程名，即设置当前进程名为"system_server"
425            Process.setArgV0(parsedArgs.niceName);
426        }
427
           // 获取环境变量SYSTEMSERVERCLASSPATH，环境变量位于init.environ.rc中
428        final String systemServerClasspath = Os.getenv("SYSTEMSERVERCLASSPATH");
429        if (systemServerClasspath != null) {
               // 对环境变量SYSTEMSERVERCLASSPATH中的jar包进行dex优化
430            performSystemServerDexOpt(systemServerClasspath);
431        }
432
            //由于 zygote的启动参数未包含"--invoke-with"，故本条件不成立，直接走else
433        if (parsedArgs.invokeWith != null) {
434            String[] args = parsedArgs.remainingArgs;
435            // If we have a non-null system server class path, we'll have to duplicate the
436            // existing arguments and append the classpath to it. ART will handle the classpath
437            // correctly when we exec a new process.
438            if (systemServerClasspath != null) {
439                String[] amendedArgs = new String[args.length + 2];
440                amendedArgs[0] = "-cp";
441                amendedArgs[1] = systemServerClasspath;
442                System.arraycopy(parsedArgs.remainingArgs, 0, amendedArgs, 2, parsedArgs.remainingArgs.length);
443            }
444
445            WrapperInit.execApplication(parsedArgs.invokeWith,
446                    parsedArgs.niceName, parsedArgs.targetSdkVersion,
447                    VMRuntime.getCurrentInstructionSet(), null, args);
448        } else {
449            ClassLoader cl = null;
450            if (systemServerClasspath != null) {
                      // new 一个PathClassLoader的实例
451                cl = new PathClassLoader(systemServerClasspath, ClassLoader.getSystemClassLoader());
452                Thread.currentThread().setContextClassLoader(cl);
453            }
454
455            /*
456             * Pass the remaining arguments to SystemServer.
457             */
              // 执行目标类的main()方法
458            RuntimeInit.zygoteInit(parsedArgs.targetSdkVersion, parsedArgs.remainingArgs, cl);
459        }
460
461        /* should never reach here */
462    }
```

先来看下注释

> 完成fork后新的system server进程的剩余工作

为了更好的理解这个方法的执行，我们看来先看parsedArgs里面的字段数据。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/hpc21j9iee.png)

parsedArgs图.png

> PS：这个方法会抛出MethodAndArgsCaller异常，我们知道这个异常其实就是处理正常业务逻辑的，相当于一个回调。

我将这个函数内部分为5部分，如下：

-   1、关闭Zygote的socket两端的连接
-   2、通过设置umask创建文件的默认权限
-   3、设置进程名字
-   4、获取SYSTEMSERVERCLASSPATH环境变量值(一系列jar)，如果需要，则进行dex优化
-   5、最后一步，也是最重要的一步：由于**invokeWith**为null，所以 会通过**RuntimeInit.zygoteInit**中调用**applicationInit**，进而调用**invokeStaticMain**，然后就会调用SystemServer的main()方法，下面会详细讲解的

下面我们来依次讲解下

###### 1、closeServerSocket() 函数解析

```
142    /**
143     * Close and clean up zygote sockets. Called on shutdown and on the
144     * child's exit path.
145     */
146    static void closeServerSocket() {
147        try {
148            if (sServerSocket != null) {
149                FileDescriptor fd = sServerSocket.getFileDescriptor();
150                sServerSocket.close();
151                if (fd != null) {
152                    Os.close(fd);
153                }
154            }
155        } catch (IOException ex) {
156            Log.e(TAG, "Zygote:  error closing sockets", ex);
157        } catch (ErrnoException ex) {
158            Log.e(TAG, "Zygote:  error closing descriptor", ex);
159        }
160
161        sServerSocket = null;
162    }
```

先来看下注释：

> 在关闭和子进程退出的时候，用来关闭并清理zygote的socket，

代码很简单，就是先close，然后在指向null。 上面第四部分提到环境变量，那我们就看下其环境变量

###### 2、环境变量解析

Android的环境变量是由init进程启动过程中读取[system/core/rootdir/init.environ.rc.in](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252Finit.environ.rc.in&objectId=1199510&objectType=1&isNewArticle=undefined)文件设置的。 内容如下：

```
1# set up the global environment
2on init
3    export ANDROID_BOOTLOGO 1
4    export ANDROID_ROOT /system
5    export ANDROID_ASSETS /system/app
6    export ANDROID_DATA /data
7    export ANDROID_STORAGE /storage
8    export EXTERNAL_STORAGE /sdcard
9    export ASEC_MOUNTPOINT /mnt/asec
10    export BOOTCLASSPATH %BOOTCLASSPATH%
11    export SYSTEMSERVERCLASSPATH %SYSTEMSERVERCLASSPATH%
```

那我们再来看下[system/core/rootdir/Android.mk](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fsystem%252Fcore%252Frootdir%252FAndroid.mk&objectId=1199510&objectType=1&isNewArticle=undefined)文件，如下：

```
1LOCAL_PATH:= $(call my-dir)
2
3#######################################
4# init.rc
5# Only copy init.rc if the target doesn't have its own.
6ifneq ($(TARGET_PROVIDES_INIT_RC),true)
7include $(CLEAR_VARS)
8
9LOCAL_MODULE := init.rc
10LOCAL_SRC_FILES := $(LOCAL_MODULE)
11LOCAL_MODULE_CLASS := ETC
12LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)
13
14include $(BUILD_PREBUILT)
15endif
16#######################################
17# init.environ.rc
18
19include $(CLEAR_VARS)
20LOCAL_MODULE_CLASS := ETC
21LOCAL_MODULE := init.environ.rc
22LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)
23
24# Put it here instead of in init.rc module definition,
25# because init.rc is conditionally included.
26#
27# create some directories (some are mount points)
28LOCAL_POST_INSTALL_CMD := mkdir -p $(addprefix $(TARGET_ROOT_OUT)/, \
29    sbin dev proc sys system data oem)
30
31include $(BUILD_SYSTEM)/base_rules.mk
32
33# Regenerate init.environ.rc if PRODUCT_BOOTCLASSPATH has changed.
34bcp_md5 := $(word 1, $(shell echo $(PRODUCT_BOOTCLASSPATH) $(PRODUCT_SYSTEM_SERVER_CLASSPATH) | $(MD5SUM)))
35bcp_dep := $(intermediates)/$(bcp_md5).bcp.dep
36$(bcp_dep) :
37  $(hide) mkdir -p $(dir $@) && rm -rf $(dir $@)*.bcp.dep && touch $@
38
39$(LOCAL_BUILT_MODULE): $(LOCAL_PATH)/init.environ.rc.in $(bcp_dep)
40  @echo "Generate: $< -> $@"
41  @mkdir -p $(dir $@)
42  $(hide) sed -e 's?%BOOTCLASSPATH%?$(PRODUCT_BOOTCLASSPATH)?g' $< >$@
43  $(hide) sed -i -e 's?%SYSTEMSERVERCLASSPATH%?$(PRODUCT_SYSTEM_SERVER_CLASSPATH)?g' $@
44
45bcp_md5 :=
46bcp_dep :=
47#######################################
```

请看其中的43行，我们知道**"SYSTEMSERVERCLASSPATH"**是由**"PRODUCT\_SYSTEM\_SERVER\_CLASSPATH"**变量来指定的。而**"PRODUCT\_SYSTEM\_SERVER\_CLASSPATH"**是由**"PRODUCT\_SYSTEM\_SERVER\_JARS"**来决定的，代码如下：

```
1####################################
2# dexpreopt support - typically used on user builds to run dexopt (for Dalvik) or dex2oat (for ART) ahead of time
3#
4####################################
5
6# list of boot classpath jars for dexpreopt
7DEXPREOPT_BOOT_JARS := $(subst $(space),:,$(PRODUCT_BOOT_JARS))
8DEXPREOPT_BOOT_JARS_MODULES := $(PRODUCT_BOOT_JARS)
9PRODUCT_BOOTCLASSPATH := $(subst $(space),:,$(foreach m,$(DEXPREOPT_BOOT_JARS_MODULES),/system/framework/$(m).jar))
10
11PRODUCT_SYSTEM_SERVER_CLASSPATH := $(subst $(space),:,$(foreach m,$(PRODUCT_SYSTEM_SERVER_JARS),/system/framework/$(m).jar))
12
13DEXPREOPT_BUILD_DIR := $(OUT_DIR)
14DEXPREOPT_PRODUCT_DIR_FULL_PATH := $(PRODUCT_OUT)/dex_bootjars
15DEXPREOPT_PRODUCT_DIR := $(patsubst $(DEXPREOPT_BUILD_DIR)/%,%,$(DEXPREOPT_PRODUCT_DIR_FULL_PATH))
16DEXPREOPT_BOOT_JAR_DIR := system/framework
17DEXPREOPT_BOOT_JAR_DIR_FULL_PATH := $(DEXPREOPT_PRODUCT_DIR_FULL_PATH)/$(DEXPREOPT_BOOT_JAR_DIR)
18
19# The default value for LOCAL_DEX_PREOPT
20DEX_PREOPT_DEFAULT ?= true
21
22# $(1): the .jar or .apk to remove classes.dex
23define dexpreopt-remove-classes.dex
24$(hide) zip --quiet --delete $(1) classes.dex; \
25dex_index=2; \
26while zip --quiet --delete $(1) classes$${dex_index}.dex > /dev/null; do \
27  let dex_index=dex_index+1; \
28done
29endef
30
31# Special rules for building stripped boot jars that override java_library.mk rules
32
33# $(1): boot jar module name
34define _dexpreopt-boot-jar-remove-classes.dex
35_dbj_jar_no_dex := $(DEXPREOPT_BOOT_JAR_DIR_FULL_PATH)/$(1)_nodex.jar
36_dbj_src_jar := $(call intermediates-dir-for,JAVA_LIBRARIES,$(1),,COMMON)/javalib.jar
37
38$$(_dbj_jar_no_dex) : $$(_dbj_src_jar) | $(ACP) $(AAPT)
39  $$(call copy-file-to-target)
40ifneq ($(DEX_PREOPT_DEFAULT),nostripping)
41  $$(call dexpreopt-remove-classes.dex,$$@)
42endif
43
44_dbj_jar_no_dex :=
45_dbj_src_jar :=
46endef
47
48$(foreach b,$(DEXPREOPT_BOOT_JARS_MODULES),$(eval $(call _dexpreopt-boot-jar-remove-classes.dex,$(b))))
49
50include $(BUILD_SYSTEM)/dex_preopt_libart.mk
51
52# Define dexpreopt-one-file based on current default runtime.
53# $(1): the input .jar or .apk file
54# $(2): the output .odex file
55define dexpreopt-one-file
56$(call dex2oat-one-file,$(1),$(2))
57endef
58
59DEXPREOPT_ONE_FILE_DEPENDENCY_TOOLS := $(DEX2OAT_DEPENDENCY)
60DEXPREOPT_ONE_FILE_DEPENDENCY_BUILT_BOOT_PREOPT := $(DEFAULT_DEX_PREOPT_BUILT_IMAGE_FILENAME)
61ifdef TARGET_2ND_ARCH
62$(TARGET_2ND_ARCH_VAR_PREFIX)DEXPREOPT_ONE_FILE_DEPENDENCY_BUILT_BOOT_PREOPT := $($(TARGET_2ND_ARCH_VAR_PREFIX)DEFAULT_DEX_PREOPT_BUILT_IMAGE_FILENAME)
63endif  # TARGET_2ND_ARCH
```

请注意11行，PRODUCT\_SYSTEM\_SERVER\_JARS变量的值可以根据产品的需求进行增减。这样在获取环境变量SYSTEMSERVERCLASSPATH指定的jar包后，就要对这个jar包进行dex优化了。

关于dex优化，我们在讲解**APK安装流程详解**，讲解过了，这里就不详细讲解了。

###### 3、 RuntimeInit.zygoteInit函数解析

[RuntimeInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FRuntimeInit.java&objectId=1199510&objectType=1&isNewArticle=undefined)

```
256    /**
257     * The main function called when started through the zygote process. This
258     * could be unified with main(), if the native code in nativeFinishInit()
259     * were rationalized with Zygote startup.<p>
260     *
261     * Current recognized args:
262     * <ul>
263     *   <li> <code> [--] &lt;start class name&gt;  &lt;args&gt;
264     * </ul>
265     *
266     * @param targetSdkVersion target SDK version
267     * @param argv arg strings
268     */
269    public static final void zygoteInit(int targetSdkVersion, String[] argv, ClassLoader classLoader)
270            throws ZygoteInit.MethodAndArgsCaller {
271        if (DEBUG) Slog.d(TAG, "RuntimeInit: Starting application from zygote");
272
273        Trace.traceBegin(Trace.TRACE_TAG_ACTIVITY_MANAGER, "RuntimeInit");
          // 日志重定向
274        redirectLogStreams();
275
           // 通用的初始化工作
276        commonInit();
            // zygote初始化
277        nativeZygoteInit();
            // 应用的初始化
278        applicationInit(targetSdkVersion, argv, classLoader);
279    }
```

先来看下注释

> 通过zygote方法，在开启的时候，来调用main方法。如果native代码的nativeFinishInit()中通过Zygote合理的启动，将会与main()统一。

-   targetSdkVersion：目标sdk标准
-   argv：标志参数

这个方法方里面 主要就是进行两件事

-   在调用applicationInit方法前进行一些初始化操作
    -   日志重定向
    -   zygote初始化
-   调用applicationInit进行应用初始化

###### 3.1、 commonInit()方法解析

代码在[RuntimeInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FRuntimeInit.java&objectId=1199510&objectType=1&isNewArticle=undefined)

```
106    private static final void commonInit() {
107        if (DEBUG) Slog.d(TAG, "Entered RuntimeInit!");
108
109        /* set default handler; this applies to all threads in the VM */
           // 设置默认的未捕获异常处理方法
110        Thread.setDefaultUncaughtExceptionHandler(new UncaughtHandler());
111
112        /*
113         * Install a TimezoneGetter subclass for ZoneInfo.db
114         */
           // 设置市区，比如中国时区为"Asia/Beijing"
115        TimezoneGetter.setInstance(new TimezoneGetter() {
116            @Override
117            public String getId() {
118                return SystemProperties.get("persist.sys.timezone");
119            }
120        });
           // 设置默认时区
121        TimeZone.setDefault(null);
122
123        /*
124         * Sets handler for java.util.logging to use Android log facilities.
125         * The odd "new instance-and-then-throw-away" is a mirror of how
126         * the "java.util.logging.config.class" system property works. We
127         * can't use the system property here since the logger has almost
128         * certainly already been initialized.
129         */
           //重置log配置
130        LogManager.getLogManager().reset();
131        new AndroidConfig();
132
133        /*
134         * Sets the default HTTP User-Agent used by HttpURLConnection.
135         */
136        String userAgent = getDefaultUserAgent();
           // 设置默认的HTTP User-agent
           // 例如 "Dalvik/1.1.0 (Linux; U; Android 6.0.1；LenovoX3c70 Build/LMY47V)".
137        System.setProperty("http.agent", userAgent);
138
139        /*
140         * Wire socket tagging to traffic stats.
141         */
142        NetworkManagementSocketTagger.install();
143
144        /*
145         * If we're running in an emulator launched with "-trace", put the
146         * VM into emulator trace profiling mode so that the user can hit
147         * F9/F10 at any time to capture traces.  This has performance
148         * consequences, so it's not something you want to do always.
149         */
150        String trace = SystemProperties.get("ro.kernel.android.tracing");
151        if (trace.equals("1")) {
152            Slog.i(TAG, "NOTE: emulator trace profiling enabled");
153            Debug.enableEmulatorTraceOutput();
154        }
155
156        initialized = true;
157    }
```

这个方法主要是提供通用的初始化

###### 3.2、 nativeZygoteInit()方法解析

代码在[RuntimeInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FRuntimeInit.java&objectId=1199510&objectType=1&isNewArticle=undefined)

```
55    private static final native void nativeZygoteInit();
```

对应的jni的方法在[AndroidRuntime.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjni%252FAndroidRuntime.cpp&objectId=1199510&objectType=1&isNewArticle=undefined)

```
205static void com_android_internal_os_RuntimeInit_nativeZygoteInit(JNIEnv* env, jobject clazz)
206{
207    gCurRuntime->onZygoteInit();
208}
```

我们看到在com\_android\_internal\_os\_RuntimeInit\_nativeZygoteInit函数中什么也没做，就是做调用了onZygoteInit()函数，而通过上面的代码，我们知道，onZygoteInit的()函数的具体实现是在AppRuntime里面 [app\_main.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcmds%252Fapp_process%252Fapp_main.cpp&objectId=1199510&objectType=1&isNewArticle=undefined)

```
91    virtual void onZygoteInit()
92    {
93        sp<ProcessState> proc = ProcessState::self();
94        ALOGV("App process: starting thread pool.\n");
95        proc->startThreadPool();
96    }
```

我们看到没什么东西，就是在里面构造了进程的ProcessState全局变量，而且启动了线程池。

> ProcessState::self()是单例模式。主要作用就是调用open()打开**/dev/binder**驱动设备，再利用mmap()映射内核的地址空间，将Binder驱动的fd赋值ProcessState对象中的变量mDriverFD，用于交互操作。startThreadPoll()是创建一个新的binder，不断进行talkWithDriver()，在binder系列文章有讲解过的。这里就不继续跟了。

ok上面两个初始化的行为全部讲解完毕，现在来看下applicationInit()方法的内部实现

###### 3.3、 applicationInit()函数解析

代码在[RuntimeInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FRuntimeInit.java&objectId=1199510&objectType=1&isNewArticle=undefined)中

```
299    private static void applicationInit(int targetSdkVersion, String[] argv, ClassLoader classLoader)
300            throws ZygoteInit.MethodAndArgsCaller {
301        // If the application calls System.exit(), terminate the process
302        // immediately without running any shutdown hooks.  It is not possible to
303        // shutdown an Android application gracefully.  Among other things, the
304        // Android runtime shutdown hooks close the Binder driver, which can cause
305        // leftover running threads to crash before the process actually exits.
           // true 代表应用程序退出时，不调用AppRuntime.onExit()，否则会在退出前调用
306        nativeSetExitWithoutCleanup(true);
307
308        // We want to be fairly aggressive about heap utilization, to avoid
309        // holding on to a lot of memory that isn't needed.
           // 设置虚拟机的内存利用率数值为0.75
310        VMRuntime.getRuntime().setTargetHeapUtilization(0.75f);
311        VMRuntime.getRuntime().setTargetSdkVersion(targetSdkVersion);
312
313        final Arguments args;
314        try {
                // 解析参数
315            args = new Arguments(argv);
316        } catch (IllegalArgumentException ex) {
317            Slog.e(TAG, ex.getMessage());
318            // let the process exit
319            return;
320        }
321
322        // The end of of the RuntimeInit event (see #zygoteInit).
323        Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
324
325        // Remaining arguments are passed to the start class's static main
         //调用startClass的static方法main()方法
326        invokeStaticMain(args.startClass, args.startArgs, classLoader);
327    }
```

来看下下面这个图，我们知道**args.startClass**为**"com.android.server.SystemServer"**

![](https://ask.qcloudimg.com/http-save/yehe-2957818/hpc21j9iee.png)

parsedArgs图.png

所以调用的是com.android.server.SystemServer的静态main方法

那我们来看下invokeStaticMain方法的内部实现

###### 3.3.1、 invokeStaticMain()方法解析

代码在[RuntimeInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FRuntimeInit.java&objectId=1199510&objectType=1&isNewArticle=undefined)中

```
189    /**
190     * Invokes a static "main(argv[]) method on class "className".
191     * Converts various failing exceptions into RuntimeExceptions, with
192     * the assumption that they will then cause the VM instance to exit.
193     *
194     * @param className Fully-qualified class name
195     * @param argv Argument vector for main()
196     * @param classLoader the classLoader to load {@className} with
197     */
198    private static void invokeStaticMain(String className, String[] argv, ClassLoader classLoader)
199            throws ZygoteInit.MethodAndArgsCaller {
200        Class<?> cl;
201
202        try {
               // 加载类
203            cl = Class.forName(className, true, classLoader);
204        } catch (ClassNotFoundException ex) {
205            throw new RuntimeException(
206                    "Missing class when invoking static main " + className,
207                    ex);
208        }
209
210        Method m;
211        try {
212            m = cl.getMethod("main", new Class[] { String[].class });
213        } catch (NoSuchMethodException ex) {
214            throw new RuntimeException(
215                    "Missing static main on " + className, ex);
216        } catch (SecurityException ex) {
217            throw new RuntimeException(
218                    "Problem getting static main on " + className, ex);
219        }
220
221        int modifiers = m.getModifiers();
222        if (! (Modifier.isStatic(modifiers) && Modifier.isPublic(modifiers))) {
223            throw new RuntimeException(
224                    "Main method is not public and static on " + className);
225        }
226
227        /*
228         * This throw gets caught in ZygoteInit.main(), which responds
229         * by invoking the exception's run() method. This arrangement
230         * clears up all the stack frames that were required in setting
231         * up the process.
232         */
           // 通过抛出异常的方式，回到ZygoteInit.main()，这样做的好处是清空栈帧，提高栈帧利用率
233        throw new ZygoteInit.MethodAndArgsCaller(m, argv);
234    }
```

先来翻译一下注释

> 调用目标类className类的静态main(argv \[\]) 方法。将各种失败异常转化为RuntimeExceptions，并且这些异常将会导致VM实例退出

-   入参 className：全类名
-   入参argv：main函数的入参
-   入参classLoader：加载className类的类加载器

代码中以Class.forName的方式获取到SystemServer类及其main函数。 注意：该函数最后一句抛出异常的语句，根据注释，这个ZygoteInit.MethodAndArgsCaller的"异常"会被ZygoteInit.main()捕获，并且出发执行异常类的run方法。那回头来再看看ZygoteInit.main()函数的代码 代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined)

```
public static void main(String argv[]) {
    try {
        ....
    } catch (MethodAndArgsCaller caller) {
        caller.run();
    } catch (RuntimeException ex) {
        closeServerSocket();
        throw ex;
    }
}
```

这里，RuntimeInit.applicationInit有抛出ZygoteInit.MethodAndArgsCaller"异常"，然后在ZygoteInit.main()中进行捕获，不过需要注意的是由于执行handleSystemServerProcess开始就处于system\_server进程了，因此捕获ZygoteInit.MethodAndArgsCaller"异常"的进程是system\_server进程，捕获就会调用MethodAndArgsCaller.run()方法。那让我们来看下MethodAndArgsCaller.run()方法的具体实现。

###### 3.3.2、MethodAndArgsCaller.run()方法解析

代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined)中

```
706    /**
707     * Helper exception class which holds a method and arguments and
708     * can call them. This is used as part of a trampoline to get rid of
709     * the initial process setup stack frames.
710     */
711    public static class MethodAndArgsCaller extends Exception
712            implements Runnable {
713        /** method to call */
714        private final Method mMethod;
715
716        /** argument array */
717        private final String[] mArgs;
718
719        public MethodAndArgsCaller(Method method, String[] args) {
                  // 此时method描述的是System类的main函数
720            mMethod = method;
721            mArgs = args;
722        }
723
724        public void run() {
725            try {
                  // 根据传递过来的参数，可知此处通过反射机制调用的是SystemServer.main()方法
726                mMethod.invoke(null, new Object[] { mArgs });
727            } catch (IllegalAccessException ex) {
728                throw new RuntimeException(ex);
729            } catch (InvocationTargetException ex) {
730                Throwable cause = ex.getCause();
731                if (cause instanceof RuntimeException) {
732                    throw (RuntimeException) cause;
733                } else if (cause instanceof Error) {
734                    throw (Error) cause;
735                }
736                throw new RuntimeException(ex);
737            }
738        }
739    }
```

终于，zygote启动system\_server进程的流程已经一步步的简要分析完了，后面就是通过反射机制进入到SystemServer.main中，进行类似与初始化的工作内容了。

后面关于SystemServer的main方法执行，我们后续单独的文章中讲解

##### (三)、关于进程的位置

因为在Zygote进程fork系统进程的时候，会有两个进程，很多同学弄不清，那个方法是在那个进程里面，执行的。关于那个方法在那个进程如下：

```
ZygoteInit.startSystemServer
    Zygote.forkSystemServer
        Zygote.nativeForkSystemServer
        com_android_internal_os_Zygote_nativeForkSystemServer //com_android_internal_os_Zygote_nativeForkSystemServer.cpp文件中
            ForkAndSpecializeCommon //com_android_internal_os_Zygote_nativeForkSystemServer.cpp文件中
------------------------------------------------------------
该分界线上方处于zygote进程      下方则运行在system_server进程
------------------------------------------------------------
    ZygoteInit.handleSystemServerProcess
        ZygoteInit.performSystemServerDexOpt
        RuntimeInit.zygoteInit
            RuntimeInit.commonInit()
            RuntimeInit.nativeZygoteInit()
            RuntimeInit.applicationInit
                RuntimeInit.invokeStaticMain
                    SystemServer.main
```

### 五、处理启动应用的请求——runSelectLoop()方法解析

ZygoteInit类的main()方法调用runSelectLoop()方法来监听和处理启动应用的请求。 代码在[ZygoteInit.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteInit.java&objectId=1199510&objectType=1&isNewArticle=undefined)

```
654    /**
655     * Runs the zygote process's select loop. Accepts new connections as
656     * they happen, and reads commands from connections one spawn-request's
657     * worth at a time.
658     *
659     * @throws MethodAndArgsCaller in a child process when a main() should
660     * be executed.
661     */
662    private static void runSelectLoop(String abiList) throws MethodAndArgsCaller {
663        ArrayList<FileDescriptor> fds = new ArrayList<FileDescriptor>();
664        ArrayList<ZygoteConnection> peers = new ArrayList<ZygoteConnection>();
665
          //fds[0]为sServerSocket，即sServerSocket为位于zygote进程中的socket服务端
666        fds.add(sServerSocket.getFileDescriptor());
667        peers.add(null);
668
669        while (true) {
//************************** 第1部分   ************************** 
670            StructPollfd[] pollFds = new StructPollfd[fds.size()];
671            for (int i = 0; i < pollFds.length; ++i) {
672                pollFds[i] = new StructPollfd();
                   // pollFds[0].fd即为sServerSocket，位于zygote进程中的socket服务端。
673                pollFds[i].fd = fds.get(i);
674                pollFds[i].events = (short) POLLIN;
675            }
676            try {
                   // 查询轮训状态，当pollFdd有事件到来则往下执行，否则阻塞在这里
677                Os.poll(pollFds, -1);
678            } catch (ErrnoException ex) {
679                throw new RuntimeException("poll failed", ex);
680            }
681            for (int i = pollFds.length - 1; i >= 0; --i) {
                 // 采用I/O 多路复用机制，当接受到客户端发出的连接请求，或者处理出具时，则往下执行
                 // 否则进入continue，跳出本次循环 
682                if ((pollFds[i].revents & POLLIN) == 0) {
683                    continue;
684                }
//************************** 第2部分   **************************
685                if (i == 0) {
                      // 客户端第一次请求服务端，服务端调用accept与客户端建立连接，客户端在zygote以ZygoteConnection对象表示
686                    ZygoteConnection newPeer = acceptCommandPeer(abiList);
687                    peers.add(newPeer);
688                    fds.add(newPeer.getFileDesciptor());
689                } else {
//*************************** 第3部分   **************************
                      // 经过上个if操作后，客户端与服务端已经建立连接，并开始发送数据
                      //peers.get(index)取得发送数据客户端的ZygoteConnection对象
                      // 然后调用runOnce()方法来出具具体请求
690                    boolean done = peers.get(i).runOnce();
691                    if (done) {
692                        peers.remove(i);
                           // 处理完则从fds中移除该文件描述符
693                        fds.remove(i);
694                    }
695                }
696            }
697        }
698    }
```

先来看下翻译

> 执行zygote进程的循环。当来一个新的连接请求时，则建立接受并建立连接，并在连接中读取请求的命令

为了更好的理解，我将runSelectLoop()方法内部分为3大块，每一块都有自己的核心人物理念：

-   1、监听socket事件
-   2、接受连接请求
-   3、处理连接请求

那我们依次讲解下

##### 1、监听socket事件

在runSelectLoop里面利用 while (true) 的死循环， Os.poll(pollFds, -1)来查询轮训状态，如果有pollFdd时间来，则往下执行，否则便会阻塞在这里。

##### 2、接受连接请求

当i的值为0时，说明请求连接的事件来了，这时候调用acceptCommandPeer()来和客户端简历一个socket连接，然后吧这个socket加入监听的数组中。等待这个socket的上的命令的到来。

##### 3、接受消息

如果i>0，说明是已经连接socket上的命令来了。一旦接收到已和客户端连接的socket的传过来的命令，runSelectLoop()方法会调用ZygoteConnection类的runOnce()方法去处理命令。处理完后，就会断开与客户端的连接，并把用于连接的socket从监听表中移除。

PS：Zygote采用高效的I/O多路复用机制，保证没有客户端连接请求或数据处理时休眠，否则相应客户端的请求。

所以sunrunSelectLoop方法的内部还是比较简单的，就是处理客户端的连接和请求，其中客户端在zygote进程中使用ZygoteConnection对象表示。客户端的请求由ZygoteConnection的runOnce来处理。

那我们来看下ZygoteConnection的runOnce()方法 [ZygoteConnection.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteConnection.java&objectId=1199510&objectType=1&isNewArticle=undefined)

```
118    /**
119     * Reads one start command from the command socket. If successful,
120     * a child is forked and a {@link ZygoteInit.MethodAndArgsCaller}
121     * exception is thrown in that child while in the parent process,
122     * the method returns normally. On failure, the child is not
123     * spawned and messages are printed to the log and stderr. Returns
124     * a boolean status value indicating whether an end-of-file on the command
125     * socket has been encountered.
126     *
127     * @return false if command socket should continue to be read from, or
128     * true if an end-of-file has been encountered.
129     * @throws ZygoteInit.MethodAndArgsCaller trampoline to invoke main()
130     * method in child process
131     */
132    boolean runOnce() throws ZygoteInit.MethodAndArgsCaller {
133
134        String args[];
135        Arguments parsedArgs = null;
136        FileDescriptor[] descriptors;
137
//************************* 第1部分 *************************
138        try { 
               // 读取参数
139            args = readArgumentList();
140            descriptors = mSocket.getAncillaryFileDescriptors();
141        } catch (IOException ex) {
142            Log.w(TAG, "IOException on command socket " + ex.getMessage());
143            closeSocket();
144            return true;
145        }
146
147        if (args == null) {
148            // EOF reached.
149            closeSocket();
150            return true;
151        }
152
153        /** the stderr of the most recent request, if avail */
154        PrintStream newStderr = null;
155
156        if (descriptors != null && descriptors.length >= 3) {
157            newStderr = new PrintStream(
158                    new FileOutputStream(descriptors[2]));
159        }
160
161        int pid = -1;
162        FileDescriptor childPipeFd = null;
163        FileDescriptor serverPipeFd = null;
164
//************************* 第2部分 *************************
165        try {
              // 将binder 客户端传递过来的参数，解析成Arguments对象格式
166            parsedArgs = new Arguments(args);
167
168            if (parsedArgs.abiListQuery) {
169                return handleAbiListQuery();
170            }
171
172            if (parsedArgs.permittedCapabilities != 0 || parsedArgs.effectiveCapabilities != 0) {
173                throw new ZygoteSecurityException("Client may not specify capabilities: " +
174                        "permitted=0x" + Long.toHexString(parsedArgs.permittedCapabilities) +
175                        ", effective=0x" + Long.toHexString(parsedArgs.effectiveCapabilities));
176            }
177
//************************* 第3部分 *************************
178            applyUidSecurityPolicy(parsedArgs, peer);
179            applyInvokeWithSecurityPolicy(parsedArgs, peer);
180
181            applyDebuggerSystemProperty(parsedArgs);
182            applyInvokeWithSystemProperty(parsedArgs);
183
184            int[][] rlimits = null;
185
186            if (parsedArgs.rlimits != null) {
187                rlimits = parsedArgs.rlimits.toArray(intArray2d);
188            }
189
190            if (parsedArgs.invokeWith != null) {
191                FileDescriptor[] pipeFds = Os.pipe2(O_CLOEXEC);
192                childPipeFd = pipeFds[1];
193                serverPipeFd = pipeFds[0];
194                Os.fcntlInt(childPipeFd, F_SETFD, 0);
195            }
196
197            /**
198             * In order to avoid leaking descriptors to the Zygote child,
199             * the native code must close the two Zygote socket descriptors
200             * in the child process before it switches from Zygote-root to
201             * the UID and privileges of the application being launched.
202             *
203             * In order to avoid "bad file descriptor" errors when the
204             * two LocalSocket objects are closed, the Posix file
205             * descriptors are released via a dup2() call which closes
206             * the socket and substitutes an open descriptor to /dev/null.
207             */
208
209            int [] fdsToClose = { -1, -1 };
210
211            FileDescriptor fd = mSocket.getFileDescriptor();
212
213            if (fd != null) {
214                fdsToClose[0] = fd.getInt$();
215            }
216
217            fd = ZygoteInit.getServerSocketFileDescriptor();
218
219            if (fd != null) {
220                fdsToClose[1] = fd.getInt$();
221            }
222
223            fd = null;
224
//************************* 第4部分 *************************
               // 分裂出新进程
225            pid = Zygote.forkAndSpecialize(parsedArgs.uid, parsedArgs.gid, parsedArgs.gids,
226                    parsedArgs.debugFlags, rlimits, parsedArgs.mountExternal, parsedArgs.seInfo,
227                    parsedArgs.niceName, fdsToClose, parsedArgs.instructionSet,
228                    parsedArgs.appDataDir);
229        } catch (ErrnoException ex) {
230            logAndPrintError(newStderr, "Exception creating pipe", ex);
231        } catch (IllegalArgumentException ex) {
232            logAndPrintError(newStderr, "Invalid zygote arguments", ex);
233        } catch (ZygoteSecurityException ex) {
234            logAndPrintError(newStderr,
235                    "Zygote security policy prevents request: ", ex);
236        }
237
//************************* 第5部分 *************************
238        try {
239            if (pid == 0) { 

                    //子进程执行
                    // 当pid=0则说明是新创建的子进程中执行的，
                    // 这时候ZygoteConnection类就会调用handleChildProc来启动这个子进程
240                // in child
241                IoUtils.closeQuietly(serverPipeFd);
242                serverPipeFd = null;
                   // 子进程的入口函数
243                handleChildProc(parsedArgs, descriptors, childPipeFd, newStderr);
244
245                // should never get here, the child is expected to either
246                // throw ZygoteInit.MethodAndArgsCaller or exec().
                   // 不会到达此处，子进程预期的是抛出异常，ZygoteInit.MethodAndArgsCaller或者执行exec().
247                return true;
248            } else {
                   // 父进程流程
249                // in parent...pid of < 0 means failure
250                IoUtils.closeQuietly(childPipeFd);
251                childPipeFd = null;
252                return handleParentProc(pid, descriptors, serverPipeFd, parsedArgs);
253            }
254        } finally {
255            IoUtils.closeQuietly(childPipeFd);
256            IoUtils.closeQuietly(serverPipeFd);
257        }
258    }
```

先翻译一下注释

> 从socket中读取一个启动命令，如果成功，则在fork一个子进程，并在在子进程中抛出一个异常，但是在父进程中是正常返回的。如果失败，子进程不会被fork出来，并且把错误信息会被答应在日志中。这里会返回一个布尔的状态值，表示是否结束socket。

-   返回值 false：如果socket还能继续读取，则返回false，如果读取结束，则返回true。

我将上面代码分为5部分：

###### 3.1、 第1部分

调用readArgumentList()方法从socket连接中读入个多个参数，参数样式是"--setuid=1"，行与行之间以"\\r"、"\\n"或者"\\r\\n"分割。 以上面讲解的system\_server为例子如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/hpc21j9iee.png)

parsedArgs图.png

###### 3.2、 第2部分

读取完毕后，调用Arguments有参构造函数，new一个Arguments 对象即parsedArgs。将上面的参数解析成列表。这个列表对象就是parsedArgs

###### 3.3、 第3部分

解析完参数后，还要对这些参数进行检查和设置。其中**applyUidSecurityPolicy(parsedArgs, peer)**函数将检查客户端进程是否有权利指定进程用户id和组id以及所属的组。具体的规则是：

-   如果客户端进程是root进程，则则可以任意指定
-   如果客户端进程是system进程，则只有在系统属性"ro.factorytest"的值为-1或者-2的情况下可以指定；其余情况报错。如果没有指定用户id和组id，将继承客户端进程的值

**applyInvokeWithSecurityPolicy(parsedArgs, peer)**方法、**applyDebuggerSystemProperty(parsedArgs)**方法和 **applyInvokeWithSystemProperty(parsedArgs)**方法主要是用来检查客户端是否有资格让zygote进程来执行相关的系统调用。这中检查依据是SELinux定义的上下文的设置。

###### 3.4、 第4部分

参数检查无误后，将调用Zygote类的forkAndSpecialize来fork子进程，这块内容，上面已经讲解了，这里就详细讲解了。

###### 3.5、 第5部分

上面结束后，如果返回的pid等于0，表示处于子进程中，执行handleChildProc()，如果pid不等于0，则表示在zygote进程中，则调用handleParentProc()方法继续处理。

那我们就依次来看下

###### 3.5.1、 handleChildProc()方法解析

代码在[ZygoteConnection.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%253A%252F%252Fandroidxref.com%252F6.0.1_r10%252Fxref%252Fframeworks%252Fbase%252Fcore%252Fjava%252Fcom%252Fandroid%252Finternal%252Fos%252FZygoteConnection.java&objectId=1199510&objectType=1&isNewArticle=undefined)

```
702    /**
703     * Handles post-fork setup of child proc, closing sockets as appropriate,
704     * reopen stdio as appropriate, and ultimately throwing MethodAndArgsCaller
705     * if successful or returning if failed.
706     *
707     * @param parsedArgs non-null; zygote args
708     * @param descriptors null-ok; new file descriptors for stdio if available.
709     * @param pipeFd null-ok; pipe for communication back to Zygote.
710     * @param newStderr null-ok; stream to use for stderr until stdio
711     * is reopened.
712     *
713     * @throws ZygoteInit.MethodAndArgsCaller on success to
714     * trampoline to code that invokes static main.
715     */
716    private void handleChildProc(Arguments parsedArgs,
717            FileDescriptor[] descriptors, FileDescriptor pipeFd, PrintStream newStderr)
718            throws ZygoteInit.MethodAndArgsCaller {
719        /**
720         * By the time we get here, the native code has closed the two actual Zygote
721         * socket connections, and substituted /dev/null in their place.  The LocalSocket
722         * objects still need to be closed properly.
723         */
724
           // 关闭Zygote的socket两端的连接
725        closeSocket();
726        ZygoteInit.closeServerSocket();
727
728        if (descriptors != null) {
729            try {
730                Os.dup2(descriptors[0], STDIN_FILENO);
731                Os.dup2(descriptors[1], STDOUT_FILENO);
732                Os.dup2(descriptors[2], STDERR_FILENO);
733
734                for (FileDescriptor fd: descriptors) {
735                    IoUtils.closeQuietly(fd);
736                }
737                newStderr = System.err;
738            } catch (ErrnoException ex) {
739                Log.e(TAG, "Error reopening stdio", ex);
740            }
741        }
742
743        if (parsedArgs.niceName != null) {
                // 设置进程名
744            Process.setArgV0(parsedArgs.niceName);
745        }
746
747        // End of the postFork event.
748        Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
749        if (parsedArgs.invokeWith != null) {
                  // 用于检测进程内存泄露或者溢出时场景而设计
750            WrapperInit.execApplication(parsedArgs.invokeWith,
751                    parsedArgs.niceName, parsedArgs.targetSdkVersion,
752                    VMRuntime.getCurrentInstructionSet(),
753                    pipeFd, parsedArgs.remainingArgs);
754        } else {
                   // 执行目标类的main()方法
755            RuntimeInit.zygoteInit(parsedArgs.targetSdkVersion,
756                    parsedArgs.remainingArgs, null /* classLoader */);
757        }
758    }
```

先来翻译一下注释

> 处理子进程fork以后的初始化设置，可以根据需要关闭socket，根据情况重新打开stdio。最终如果成功，则抛出MethodAndArgsCaller异常，如果失败，则返回 入参 parsedArgs：非空，zygote的参数 入参 descriptors：可以为空，stdio的新文件描述符（如果可用）。 入参 pipeFd：非空，和Zygote通信的pipe 入参 newStderr：可以为空，用于stderr的流，直到stdio被重新打开。

其实这个方法内部实现很简单，就是子进程继承父进程，所以所子进程里面有zygote的socket，所以首先要将其关闭，然后调用RuntimeInit.zygoteInit()方法进行相应的初始化。关于后续的流程我们在讲解**handleSystemServerProcess()**中已经讲解很清楚了。这里就不继续跟踪了

> 大家发现没这段代码其实和**handleSystemServerProcess()**方法很像，内部执行逻辑，大体一致。

下面我们再来看下handleParentProc方法

###### 3.5.2、 handleParentProc()方法解析

```
760    /**
761     * Handles post-fork cleanup of parent proc
762     *
763     * @param pid != 0; pid of child if &gt; 0 or indication of failed fork
764     * if &lt; 0;
765     * @param descriptors null-ok; file descriptors for child's new stdio if
766     * specified.
767     * @param pipeFd null-ok; pipe for communication with child.
768     * @param parsedArgs non-null; zygote args
769     * @return true for "exit command loop" and false for "continue command
770     * loop"
771     */
772    private boolean handleParentProc(int pid,
773            FileDescriptor[] descriptors, FileDescriptor pipeFd, Arguments parsedArgs) {
774
775        if (pid > 0) {
776            setChildPgid(pid);
777        }
778
779        if (descriptors != null) {
780            for (FileDescriptor fd: descriptors) {
781                IoUtils.closeQuietly(fd);
782            }
783        }
784
785        boolean usingWrapper = false;
786        if (pipeFd != null && pid > 0) {
787            DataInputStream is = new DataInputStream(new FileInputStream(pipeFd));
788            int innerPid = -1;
789            try {
790                innerPid = is.readInt();
791            } catch (IOException ex) {
792                Log.w(TAG, "Error reading pid from wrapped process, child may have died", ex);
793            } finally {
794                try {
795                    is.close();
796                } catch (IOException ex) {
797                }
798            }
799
800            // Ensure that the pid reported by the wrapped process is either the
801            // child process that we forked, or a descendant of it.
802            if (innerPid > 0) {
803                int parentPid = innerPid;
804                while (parentPid > 0 && parentPid != pid) {
805                    parentPid = Process.getParentPid(parentPid);
806                }
807                if (parentPid > 0) {
808                    Log.i(TAG, "Wrapped process has pid " + innerPid);
809                    pid = innerPid;
810                    usingWrapper = true;
811                } else {
812                    Log.w(TAG, "Wrapped process reported a pid that is not a child of "
813                            + "the process that we forked: childPid=" + pid
814                            + " innerPid=" + innerPid);
815                }
816            }
817        }
818
              // 将创建的应用进程id返回给system_server进程
819        try {
820            mSocketOutStream.writeInt(pid);
821            mSocketOutStream.writeBoolean(usingWrapper);
822        } catch (IOException ex) {
823            Log.e(TAG, "Error writing to command socket", ex);
824            return true;
825        }
826
827        return false;
828    }
```

先来翻译一下

> 处理父进程fork后的清理工作

-   入参 pid：不为0，如果是0，则是子进程，如果小于0，则表示失败
-   入参descriptors：可以为空，指定了子进程的新的stdio文件名
-   入参pipeFd：可以为空，和子进程通信的pipe
-   入参parsedArgs：非空，zygote参数
-   出参：如果为退出命令循环，则为true，如果继续命令循环为false

这个方法内部其实很简答，主要就是做一些清理工作，然后等待请求进行下一次fork

### 六、Zygote总结

> 老子的<道德经> 里面说到，道生一，一生二，二升三，三生万物，在Android的世界中，Zygote就是这里面的"道"。它在android系统中创建了Java时间。并且它创建了第一个Java虚拟机，并且它成功的"繁殖"了framework的核心system\_server进程。

zygote的启动流程大致如下：

-   1 创建AppRuntime对象，并且调用其start函数。之后zygote的核心初始化都由AppRuntime中。
-   2 调用startVm创建Java虚拟机，然后调用startReg来注册JNI函数
-   3 通过JNI调用com.android.internal.os.ZygoteInit的main函数，从此进入了Java世界
-   4 调用registerZygoteSocket创建可以响应子孙后台请求的socket。同时zygote调用preload函数预加载常用的类、资源等，为Java世界添砖加瓦
-   5 调用startSystemServer函数fork一个system\_server来为Java服务
-   6 Zygote完成了Java的初始工作后，便调用runSelectLoop来让自己无限循环等待。之后，如果收到子孙后台的请求，它便会醒来为他们工作。

附上zygote流程图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/eaz7hgodj6.png)

zygote流程.png

最后附上整体流程图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/u4sdj4zsyv.png)

image.png

[大图链接](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%253A%252F%252Fpostimg.org%252Fimage%252Fx1z0eeni3%252F&objectId=1199510&objectType=1&isNewArticle=undefined)

### 官人\[飞吻\]，你都把臣妾从头看到尾了，喜欢就点个赞呗(眉眼)！！！