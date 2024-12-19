## 前提

## 1.回顾

在Android系统中，第一个启动的就是init进程，由init进程加载并解析init.rc以及init.zygote64.rc等等配置文件。来启动相应的service。Zygote就是由他启动起来的。Zygote是一个孵化器，system\_server和所有的应用程序都是由他创建出来的。最初的时候Zygote进程的名称并不是Zygote而是app\_process。Zygote启动后，Linux系统下的pctrl系统会调用app\_process,所以把名称换成了Zygote。

## 2.架构

Zygote他是一个C/S架构。Zygote进程做为服务端，通过Socket的方式和其他进程进行通信。当接收到数据的时候会fork子进程，fork是不会复制父进程的内存，而是和父进程共享一个内存空间，只有需要进行内存数据修改的时候才会进行内存复制，也就是常说的读时共享，写时复制。

## 3.工作内容

Zygote做为一个孵化器，会提前加载一些系统资源、创建Java虚拟机、注册JNI等等，这样子进程就可以直接使用，避免重复加载。比如Zygote中的JNI函数、主题资源等等。

## 正文

## 1.Zygote的启动

在之前的init中有讲到，init解析init.rc的时候会解析service字段 并添加到serviceList中，并且执行fork对应的进程，执行程序。这些启动脚本都存放在 `system/core/rootdir`目录中。

代码如下：

```
#解析服务 并且添加到serviceList中
service zygote /system/bin/app_process64 -Xzygote /system/bin --zygote --start-system-server
    class main
    priority -20
    user root
    group root readproc reserved_disk
    socket zygote stream 660 root system
    socket usap_pool_primary stream 660 root system
    onrestart write /sys/android_power/request_state wake
    onrestart write /sys/power/state on
    onrestart restart audioserver
    onrestart restart cameraserver
    onrestart restart media
    onrestart restart netd
    onrestart restart wificond
    writepid /dev/cpuset/foreground/tasks
    
#遍历serviceList 调用start函数 fork进程 execv执行app_process64
on nonencrypted class_start main class_start late_start
    
```

app\_process64对应代码就在`/frameworks/base/cmds/app_process`目录下。对应的源文件就是app\_main.cpp。 init进程启动后通过调用execv("/system/bin/app\_process64","-Xzygote /system/bin --zygote --start-system-server") 执行程序，并且将参数传递给main函数。

接下来我们看看zygote的main函数做了什么?

```
//init传递过来的参数如下 -Xzygote /system/bin --zygote --start-system-server
int main(int argc, char* const argv[])
{
    //#ifndef LOG_NDEBUG
    //#ifdef NDEBUG
    //#define LOG_NDEBUG 1
    //#else
    //#define LOG_NDEBUG 0  所以这里是true 会把参数存放到argv_String中，然后打印出来
    if (!LOG_NDEBUG) {
      String8 argv_String;
      for (int i = 0; i < argc; ++i) {
        argv_String.append(""");
        argv_String.append(argv[i]);
        argv_String.append("" ");
      }
      ALOGV("app_process main with argv: %s", argv_String.string());
    }
   //创建开启AppRuntime，并将参数传递给AppRuntime
    AppRuntime runtime(argv[0], computeArgBlockSize(argc, argv));
    argc--;
    argv++;
    bool known_command = false;

    int i;
    for (i = 0; i < argc; i++) {
        if (known_command == true) {
          runtime.addOption(strdup(argv[i]));
          ALOGV("app_process main add known option '%s'", argv[i]);
          known_command = false;
          continue;
        }

        for (int j = 0;
             j < static_cast<int>(sizeof(spaced_commands) / sizeof(spaced_commands[0]));
             ++j) {
          if (strcmp(argv[i], spaced_commands[j]) == 0) {
            known_command = true;
            ALOGV("app_process main found known command '%s'", argv[i]);
          }
        }

        if (argv[i][0] != '-') {//如果参数第一个字符是'-'跳出循环，Zygote传递的第一个参数是-Xzygote 所以执行到这里会跳出循环
            break;
        }
        if (argv[i][1] == '-' && argv[i][2] == 0) {
            ++i; // Skip --.
            break;
        }

        runtime.addOption(strdup(argv[i]));
        ALOGV("app_process main add option '%s'", argv[i]);
    }

    // Parse runtime arguments.  Stop at first unrecognized option.
    bool zygote = false;
    bool startSystemServer = false;
    bool application = false;
    String8 niceName;
    String8 className;

    ++i;  
    while (i < argc) {
        const char* arg = argv[i++];
        if (strcmp(arg, "--zygote") == 0) {//传递的参数有--zygote的就把zygote赋值为true niceName = zygote
            zygote = true;
            niceName = ZYGOTE_NICE_NAME;
        } else if (strcmp(arg, "--start-system-server") == 0) { //传递的参数有start--system-server 把startSystemServer 赋值为tue表示当前进程的main是需要开启system_server的
            startSystemServer = true;
        } else if (strcmp(arg, "--application") == 0) {//如果传递的参数包含了--application 表示当前是应用程序
            application = true;
        } else if (strncmp(arg, "--nice-name=", 12) == 0) {//指定进程名
            niceName.setTo(arg + 12);
        } else if (strncmp(arg, "--", 2) != 0) {//application程序传递过来的className 也就是需要启动的class
            className.setTo(arg);
            break;
        } else {
            --i;
            break;
        }
    }

    Vector<String8> args;
    if (!className.isEmpty()) {//如果class不为空 说明是application，但是此时我们是空的，所以我们会走下面的分支
        args.add(application ? String8("application") : String8("tool"));
        runtime.setClassNameAndArgs(className, argc - i, argv + i);

        if (!LOG_NDEBUG) {
          String8 restOfArgs;
          char* const* argv_new = argv + i;
          int argc_new = argc - i;
          for (int k = 0; k < argc_new; ++k) {
            restOfArgs.append(""");
            restOfArgs.append(argv_new[k]);
            restOfArgs.append("" ");
          }
          ALOGV("Class name = %s, args = %s", className.string(), restOfArgs.string());
        }
    } else {//这里就是zygote启动模式
        // We're in zygote mode.
        maybeCreateDalvikCache();//创建Dalvik的缓存目录， data/dalvik-cache的目录

        if (startSystemServer) {//如果需要运行system_server的 会添加这个参数
            args.add(String8("start-system-server"));
        }

        char prop[PROP_VALUE_MAX];
        if (property_get(ABI_LIST_PROPERTY, prop, NULL) == 0) {
            LOG_ALWAYS_FATAL("app_process: Unable to determine ABI list from property %s.",
                ABI_LIST_PROPERTY);
            return 11;
        }

        String8 abiFlag("--abi-list=");
        abiFlag.append(prop);
        args.add(abiFlag);

        for (; i < argc; ++i) {
            args.add(String8(argv[i]));
        }
    }

    if (!niceName.isEmpty()) {//设置进程别名
        runtime.setArgv0(niceName.string(), true /* setProcName */);
    }

    if (zygote) {//注意这里之前传递的参数zygote 所以这里是true。他会调用runtime的start函数 传递com.android.internal.os.ZygoteInit 以及 init传递过来的参数 和true
        runtime.start("com.android.internal.os.ZygoteInit", args, zygote);
    } else if (className) {//application模式
        runtime.start("com.android.internal.os.RuntimeInit", args, zygote);
    } else {
        fprintf(stderr, "Error: no class name or --zygote supplied.\n");
        app_usage();
        LOG_ALWAYS_FATAL("app_process: no class name or --zygote supplied.");
    }
}

```

代码比较长,我总结下首先会打印日志把传递过来的参数打印出来。接着会创建`AppRuntime`对象，将参数传递给AppRuntime并初始化。接着根据init传递过来的参数`--zygote`把`zygote`设置成`true`表示是以zygote模式启动，如果需要开启system\_server的会把`start-system-server` 也设置为`true`。如果传递的是application就是以app模式启动，他就会查找对应的className，如果className部位空就会添加对应的参数。否则就是Zygote模式，在Zygote模式中会先创建Davik的缓存目录。最后调用`runtime.start()`,看是以什么模式启动，如果是Zygote模式启动就会传递`com.android.internal.os.ZygoteInit`和将整理的参数以及zygote 传递下去，如果是app模式就会传递`com.android.internal.os.RuntimeInit`和 args以及 zygote;

## 2.AndroidRuntime

接下来我们需要接触两个非常重要的类AppRuntime和AndroidRuntime。这两个类是整个Android Runtime环境的接口类， 他们的关系是:

```
class AppRuntime : public AndroidRuntime
```

AppRuntime是AndroidRuntime的子类。

看看AppRuntime::start()

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/efe8a4200b2441db94cc04a55db85214~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?) 我们看到AppRuntime没有start函数，start函数的实现在父类`AndroidRuntime`中。我们直接跳到AndroidRuntime中看看。 文件目录：`/framework/base/core/jni/AndroidRuntime`

```

void AndroidRuntime::start(const char* className, const Vector<String8>& options, bool zygote)
{
    ALOGD(">>>>>> START %s uid %d <<<<<<\n",
            className != NULL ? className : "(unknown)", getuid());

    static const String8 startSystemServer("start-system-server");

    /*
     * 'startSystemServer == true' means runtime is obsolete and not run from
     * init.rc anymore, so we print out the boot start event here.
     */
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == startSystemServer) {
           /* track our progress through the boot sequence */
           const int LOG_BOOT_PROGRESS_START = 3000;
           LOG_EVENT_LONG(LOG_BOOT_PROGRESS_START,  ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));
        }
    }

    //获取ANDROID_ROOT 目录
    const char* rootDir = getenv("ANDROID_ROOT");
    if (rootDir == NULL) {
        rootDir = "/system";
        if (!hasDir("/system")) {
            LOG_FATAL("No root directory specified, and /system does not exist.");
            return;
        }
        setenv("ANDROID_ROOT", rootDir, 1);
    }
    //获取ANDROID_RUNTIME_ROOT目录
    const char* runtimeRootDir = getenv("ANDROID_RUNTIME_ROOT");
    if (runtimeRootDir == NULL) {
        LOG_FATAL("No runtime directory specified with ANDROID_RUNTIME_ROOT environment variable.");
        return;
    }

    const char* tzdataRootDir = getenv("ANDROID_TZDATA_ROOT");
    if (tzdataRootDir == NULL) {
        LOG_FATAL("No tz data directory specified with ANDROID_TZDATA_ROOT environment variable.");
        return;
    }

    /* start the virtual machine */
    JniInvocation jni_invocation;
    jni_invocation.Init(NULL);//初始化JNI，加载libart.so
    JNIEnv* env;
    if (startVm(&mJavaVM, &env, zygote) != 0) {//创建虚拟机
        return;
    }
    onVmCreated(env);//当VM创建成功后会把env传递给AppRuntime来进行处理。


    if (startReg(env) < 0) {//开启注册函数 会注册很多关键的JNI函数 比如com/android/internal/os/ZygoteInit.nativeZygoteInit->com_android_internal_os_ZygoteInit_nativeZygoteInit等等函数的注册
        ALOGE("Unable to register all android natives\n");
        return;
    }

    jclass stringClass;
    jobjectArray strArray;
    jstring classNameStr;

    stringClass = env->FindClass("java/lang/String");
    assert(stringClass != NULL);
    strArray = env->NewObjectArray(options.size() + 1, stringClass, NULL);
    assert(strArray != NULL);
    //创建对应的class 此时的className 就是main函数传递过来的com.android.internal.os.ZygoteInit 把他存入strArray
    classNameStr = env->NewStringUTF(className);
    assert(classNameStr != NULL);
    env->SetObjectArrayElement(strArray, 0, classNameStr);

    for (size_t i = 0; i < options.size(); ++i) {
        jstring optionsStr = env->NewStringUTF(options.itemAt(i).string());
        assert(optionsStr != NULL);
        env->SetObjectArrayElement(strArray, i + 1, optionsStr);
    }

    char* slashClassName = toSlashClassName(className != NULL ? className : "");//把className的. 替换成/
    jclass startClass = env->FindClass(slashClassName);
    if (startClass == NULL) {
        ALOGE("JavaVM unable to locate class '%s'\n", slashClassName);
        /* keep going */
    } else {
        //执行ZygoteInit的main方法 带着传递过来的参数 回调到java层
        jmethodID startMeth = env->GetStaticMethodID(startClass, "main",
            "([Ljava/lang/String;)V");
        if (startMeth == NULL) {
            ALOGE("JavaVM unable to find main() in '%s'\n", className);
            /* keep going */
        } else {
            env->CallStaticVoidMethod(startClass, startMeth, strArray);

#if 0
            if (env->ExceptionCheck())
                threadExitUncaughtException(env);
#endif
        }
    }
    free(slashClassName);

    ALOGD("Shutting down VM\n");
    if (mJavaVM->DetachCurrentThread() != JNI_OK)
        ALOGW("Warning: unable to detach main thread\n");
    if (mJavaVM->DestroyJavaVM() != 0)
        ALOGW("Warning: VM did not shut down cleanly\n");
}
```

首先校验一些目录，然后加载libart.so，创建VM虚拟机，创建成功之后会调用`onVmCreated(env)`回到AppRuntime中,不过我们当前是Zygote 直接 return。函数如下

```

virtual void onVmCreated(JNIEnv* env)
{
    if (mClassName.isEmpty()) {
        return; // Zygote. Nothing to do here.
    }
    char* slashClassName = toSlashClassName(mClassName.string());
    mClass = env->FindClass(slashClassName);
    if (mClass == NULL) {
        ALOGE("ERROR: could not find class '%s'\n", mClassName.string());
    }
    free(slashClassName);

    mClass = reinterpret_cast<jclass>(env->NewGlobalRef(mClass));
}
```

下来调用startReg(env);开启注册函数，在这里会注册非常多的JNI函数，例如`com/android/internal/os/ZygoteInit.nativeZygoteInit->com_android_internal_os_ZygoteInit_nativeZygoteInit`等等。 接着调用toSlashClassName()把main函数传递过来的`com.android.internal.os.ZygoteInit`，把`.`替换成`/`。通过env->findClass("com/android/internal/os/ZygoteInit")查找到对应的javaClass，再调用对应的Main函数。这样Zygote就进入了Java层。

我们看下Java层的代码： 文件目录：`/frameworks/base/core/java/com/android/internal/os/ZygoteInit`

```

public static void main(String argv[]) {
    
    //zygoteServer服务端
    ZygoteServer zygoteServer = null;

    ZygoteHooks.startZygoteNoThreadCreation();

    try {
        Os.setpgid(0, 0);
    } catch (ErrnoException ex) {
        throw new RuntimeException("Failed to setpgid(0,0)", ex);
    }

    Runnable caller;
    try {
        // Report Zygote start time to tron unless it is a runtime restart
        if (!"1".equals(SystemProperties.get("sys.boot_completed"))) {
            MetricsLogger.histogram(null, "boot_zygote_init",
                    (int) SystemClock.elapsedRealtime());
        }

        String bootTimeTag = Process.is64Bit() ? "Zygote64Timing" : "Zygote32Timing";
        TimingsTraceLog bootTimingsTraceLog = new TimingsTraceLog(bootTimeTag,
                Trace.TRACE_TAG_DALVIK);
        bootTimingsTraceLog.traceBegin("ZygoteInit");
        RuntimeInit.enableDdms();

        boolean startSystemServer = false;
        String zygoteSocketName = "zygote";
        String abiList = null;
        boolean enableLazyPreload = false;
        for (int i = 1; i < argv.length; i++) {
            if ("start-system-server".equals(argv[i])) {//此时我们传递的是有值的 所以会把startSystemServer 设置为true
                startSystemServer = true;
            } else if ("--enable-lazy-preload".equals(argv[i])) {
                enableLazyPreload = true;
            } else if (argv[i].startsWith(ABI_LIST_ARG)) {
                abiList = argv[i].substring(ABI_LIST_ARG.length());
            } else if (argv[i].startsWith(SOCKET_NAME_ARG)) {
                zygoteSocketName = argv[i].substring(SOCKET_NAME_ARG.length());
            } else {
                throw new RuntimeException("Unknown command line argument: " + argv[i]);
            }
        }

        final boolean isPrimaryZygote = zygoteSocketName.equals(Zygote.PRIMARY_SOCKET_NAME);

        if (abiList == null) {
            throw new RuntimeException("No ABI list supplied.");
        }

        if (!enableLazyPreload) {
            bootTimingsTraceLog.traceBegin("ZygotePreload");
            EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_START,
                    SystemClock.uptimeMillis());
            //预加载资源
            preload(bootTimingsTraceLog);
            EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_END,
                    SystemClock.uptimeMillis());
            bootTimingsTraceLog.traceEnd(); // ZygotePreload
        } else {
            Zygote.resetNicePriority();
        }

        bootTimingsTraceLog.traceBegin("PostZygoteInitGC");
        gcAndFinalize();//GC的初始化并启动
        bootTimingsTraceLog.traceEnd(); // PostZygoteInitGC

        bootTimingsTraceLog.traceEnd(); // ZygoteInit
        // Disable tracing so that forked processes do not inherit stale tracing tags from
        // Zygote.
        Trace.setTracingEnabled(false, 0);


        Zygote.initNativeState(isPrimaryZygote);

        ZygoteHooks.stopZygoteNoThreadCreation();

        zygoteServer = new ZygoteServer(isPrimaryZygote);

        if (startSystemServer) {//此时为True 调用forkSystemServer
            Runnable r = forkSystemServer(abiList, zygoteSocketName, zygoteServer);

            // {@code r == null} in the parent (zygote) process, and {@code r != null} in the
            // child (system_server) process.
            if (r != null) {
                r.run();
                return;
            }
        }

        Log.i(TAG, "Accepting command socket connections");

        // The select loop returns early in the child process after a fork and
        // loops forever in the zygote.
        //服务端开启循环 等待客户端来请求
        caller = zygoteServer.runSelectLoop(abiList);
    } catch (Throwable ex) {
        Log.e(TAG, "System zygote died with exception", ex);
        throw ex;
    } finally {
        if (zygoteServer != null) {
            zygoteServer.closeServerSocket();
        }
    }

    // We're in the child process and have exited the select loop. Proceed to execute the
    // command.
    if (caller != null) {
        caller.run();
    }
}
```

在ZygoteInit的Main函数中，首先创建了ZygoteServer服务端用于和客户端通信，因为Zygote是C/S架构的嘛。我们从init中带过来的参数`start-system-server`所以当前的`startSystemServer`是True。接着调用了preload()

```
static void preload(TimingsTraceLog bootTimingsTraceLog) {
    Log.d(TAG, "begin preload");
    bootTimingsTraceLog.traceBegin("BeginPreload");
    beginPreload();
    bootTimingsTraceLog.traceEnd(); // BeginPreload
    bootTimingsTraceLog.traceBegin("PreloadClasses");
    preloadClasses();//预加载class  比如我们用到的activity  fragment service等等  文件目录在/framework/base/config/preloaded-classes
    bootTimingsTraceLog.traceEnd(); // PreloadClasses
    bootTimingsTraceLog.traceBegin("CacheNonBootClasspathClassLoaders");
    cacheNonBootClasspathClassLoaders();
    bootTimingsTraceLog.traceEnd(); // CacheNonBootClasspathClassLoaders
    bootTimingsTraceLog.traceBegin("PreloadResources");
    preloadResources();//预加载系统资源 比如主题 图片等等
    bootTimingsTraceLog.traceEnd(); // PreloadResources
    Trace.traceBegin(Trace.TRACE_TAG_DALVIK, "PreloadAppProcessHALs");
    nativePreloadAppProcessHALs();//HAL 硬件抽象层
    Trace.traceEnd(Trace.TRACE_TAG_DALVIK);
    Trace.traceBegin(Trace.TRACE_TAG_DALVIK, "PreloadGraphicsDriver");
    maybePreloadGraphicsDriver();
    Trace.traceEnd(Trace.TRACE_TAG_DALVIK);
    preloadSharedLibraries();//预加载库资源
    preloadTextResources();
    // Ask the WebViewFactory to do any initialization that must run in the zygote process,
    // for memory sharing purposes.
    //WebView相关
    WebViewFactory.prepareWebViewInZygote();
    endPreload();
    warmUpJcaProviders();
    Log.d(TAG, "end preload");

    sPreloadComplete = true;
}


//预加载class
private static void preloadClasses() {
    final VMRuntime runtime = VMRuntime.getRuntime();

    InputStream is;
    try {
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

   
    float defaultUtilization = runtime.getTargetHeapUtilization();
    runtime.setTargetHeapUtilization(0.8f);

    try {
        BufferedReader br =
                new BufferedReader(new InputStreamReader(is), Zygote.SOCKET_BUFFER_SIZE);//创建BufferReader读取文件

        int count = 0;
        String line;
        while ((line = br.readLine()) != null) {
            // Skip comments and blank lines.
            line = line.trim();
            if (line.startsWith("#") || line.equals("")) {
                continue;
            }

            Trace.traceBegin(Trace.TRACE_TAG_DALVIK, line);
            try {
                if (false) {
                    Log.v(TAG, "Preloading " + line + "...");
                }
                //通过Class.forName来预加载Class
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
            Trace.traceEnd(Trace.TRACE_TAG_DALVIK);
        }

        Log.i(TAG, "...preloaded " + count + " classes in "
                + (SystemClock.uptimeMillis() - startTime) + "ms.");
    } catch (IOException e) {
        Log.e(TAG, "Error reading " + PRELOADED_CLASSES + ".", e);
    } finally {
        IoUtils.closeQuietly(is);
        // Restore default.
        runtime.setTargetHeapUtilization(defaultUtilization);

        // Fill in dex caches with classes, fields, and methods brought in by preloading.
        Trace.traceBegin(Trace.TRACE_TAG_DALVIK, "PreloadDexCaches");
        runtime.preloadDexCaches();
        Trace.traceEnd(Trace.TRACE_TAG_DALVIK);

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

在preload函数中，预加载了很多的资源比如class（我们常用的Activity Fragment android.app.ActivityManager...），也加载了一些系统资源 比如图片 颜色 主题等等。大家知道为什么要在这里加载这些资源吗？ 之前有说过fork 会和父进程共享内存，写数据的时候会复制。所有的应用都是由于Zygote孵化，所以在这里进行预加载 所孵化的应用也默认拥有这些资源。

接着`startSystemServer` 为 `True`所以会执行forkSystemServer，关于SystemServer我们留在下次讲。最后执行`zygoteServer.runSelectLoop();`

```
Runnable runSelectLoop(String abiList) {
    ArrayList<FileDescriptor> socketFDs = new ArrayList<FileDescriptor>();
    ArrayList<ZygoteConnection> peers = new ArrayList<ZygoteConnection>();

    socketFDs.add(mZygoteSocket.getFileDescriptor());
    peers.add(null);

    while (true) {
        fetchUsapPoolPolicyPropsWithMinInterval();

        int[] usapPipeFDs = null;
        StructPollfd[] pollFDs = null;//需要监听的fds

        if (mUsapPoolEnabled) {
            usapPipeFDs = Zygote.getUsapPipeFDs();
            pollFDs = new StructPollfd[socketFDs.size() + 1 + usapPipeFDs.length];
        } else {
            pollFDs = new StructPollfd[socketFDs.size()];
        }


        int pollIndex = 0;
        for (FileDescriptor socketFD : socketFDs) {
            pollFDs[pollIndex] = new StructPollfd();
            pollFDs[pollIndex].fd = socketFD;
            pollFDs[pollIndex].events = (short) POLLIN;//对每一个socket需要关注POLLIN
            ++pollIndex;
        }

        final int usapPoolEventFDIndex = pollIndex;

        if (mUsapPoolEnabled) {
            pollFDs[pollIndex] = new StructPollfd();
            pollFDs[pollIndex].fd = mUsapPoolEventFD;
            pollFDs[pollIndex].events = (short) POLLIN;
            ++pollIndex;

            for (int usapPipeFD : usapPipeFDs) {
                FileDescriptor managedFd = new FileDescriptor();
                managedFd.setInt$(usapPipeFD);

                pollFDs[pollIndex] = new StructPollfd();
                pollFDs[pollIndex].fd = managedFd;
                pollFDs[pollIndex].events = (short) POLLIN;
                ++pollIndex;
            }
        }

        try {//zygote阻塞在这里等待poll事件
            Os.poll(pollFDs, -1);
        } catch (ErrnoException ex) {
            throw new RuntimeException("poll failed", ex);
        }

        boolean usapPoolFDRead = false;
        //注意这里是采用的倒序方式的，也就是说优先处理已经建立连接的请求，后处理新建立链接的请求
        while (--pollIndex >= 0) {
            if ((pollFDs[pollIndex].revents & POLLIN) == 0) {
                continue;
            }

            if (pollIndex == 0) {//初始化的时候socketFDs中只有server socket。所以会创建新的通信连接调用acceptCommandPeer
                // Zygote server socket

                ZygoteConnection newPeer = acceptCommandPeer(abiList);
                peers.add(newPeer);
                socketFDs.add(newPeer.getFileDescriptor());

            } else if (pollIndex < usapPoolEventFDIndex) {
                // Session socket accepted from the Zygote server socket

                try {//获取到客户端链接
                    ZygoteConnection connection = peers.get(pollIndex);
                    //客户端有请求进行处理
                    final Runnable command = connection.processOneCommand(this);

                    if (mIsForkChild) {
                        if (command == null) {
                            throw new IllegalStateException("command == null");
                        }

                        return command;
                    } else {
                        if (command != null) {
                            throw new IllegalStateException("command != null");
                        }

                        // We don't know whether the remote side of the socket was closed or
                        // not until we attempt to read from it from processOneCommand. This
                        // shows up as a regular POLLIN event in our regular processing loop.
                        if (connection.isClosedByPeer()) {
                            connection.closeSocket();
                            peers.remove(pollIndex);
                            socketFDs.remove(pollIndex);
                        }
                    }
                } catch (Exception e) {
                    if (!mIsForkChild) {

                        Slog.e(TAG, "Exception executing zygote command: ", e);

                        ZygoteConnection conn = peers.remove(pollIndex);
                        conn.closeSocket();

                        socketFDs.remove(pollIndex);
                    } else {
                        Log.e(TAG, "Caught post-fork exception in child process.", e);
                        throw e;
                    }
                } finally {
                    mIsForkChild = false;
                }
            } else {
                long messagePayload = -1;

                try {
                    byte[] buffer = new byte[Zygote.USAP_MANAGEMENT_MESSAGE_BYTES];
                    int readBytes = Os.read(pollFDs[pollIndex].fd, buffer, 0, buffer.length);

                    if (readBytes == Zygote.USAP_MANAGEMENT_MESSAGE_BYTES) {
                        DataInputStream inputStream =
                                new DataInputStream(new ByteArrayInputStream(buffer));

                        messagePayload = inputStream.readLong();
                    } else {
                        Log.e(TAG, "Incomplete read from USAP management FD of size "
                                + readBytes);
                        continue;
                    }
                } catch (Exception ex) {
                    if (pollIndex == usapPoolEventFDIndex) {
                        Log.e(TAG, "Failed to read from USAP pool event FD: "
                                + ex.getMessage());
                    } else {
                        Log.e(TAG, "Failed to read from USAP reporting pipe: "
                                + ex.getMessage());
                    }

                    continue;
                }

                if (pollIndex > usapPoolEventFDIndex) {
                    Zygote.removeUsapTableEntry((int) messagePayload);
                }

                usapPoolFDRead = true;
            }
        }

        if (usapPoolFDRead) {
            int[] sessionSocketRawFDs =
                    socketFDs.subList(1, socketFDs.size())
                            .stream()
                            .mapToInt(fd -> fd.getInt$())
                            .toArray();

            final Runnable command = fillUsapPool(sessionSocketRawFDs);

            if (command != null) {
                return command;
            }
        }
    }
}

```

首先注册poll的event 对POLLIN敏感，Os.poll(pollFDs)，通过`acceptCommandPeer` 创建Socket服务端的连接对象ZygoteConnection，通过 `processOneCommand`处理客户端的请求。

```
Runnable processOneCommand(ZygoteServer zygoteServer) {
        String args[];
        ZygoteArguments parsedArgs = null;
        FileDescriptor[] descriptors;

        try {
            args = Zygote.readArgumentList(mSocketReader);

            descriptors = mSocket.getAncillaryFileDescriptors();
        } catch (IOException ex) {
            throw new IllegalStateException("IOException on command socket", ex);
        }

        if (args == null) {
            isEof = true;
            return null;
        }

        int pid = -1;
        FileDescriptor childPipeFd = null;
        FileDescriptor serverPipeFd = null;

        parsedArgs = new ZygoteArguments(args);

        if (parsedArgs.mAbiListQuery) {
            handleAbiListQuery();
            return null;
        }

        if (parsedArgs.mPidQuery) {
            handlePidQuery();
            return null;
        }

        if (parsedArgs.mUsapPoolStatusSpecified) {
            return handleUsapPoolStatusChange(zygoteServer, parsedArgs.mUsapPoolEnabled);
        }

        if (parsedArgs.mPreloadDefault) {
            handlePreload();
            return null;
        }

        if (parsedArgs.mPreloadPackage != null) {
            handlePreloadPackage(parsedArgs.mPreloadPackage, parsedArgs.mPreloadPackageLibs,
                    parsedArgs.mPreloadPackageLibFileName, parsedArgs.mPreloadPackageCacheKey);
            return null;
        }

        if (canPreloadApp() && parsedArgs.mPreloadApp != null) {
            byte[] rawParcelData = Base64.getDecoder().decode(parsedArgs.mPreloadApp);
            Parcel appInfoParcel = Parcel.obtain();
            appInfoParcel.unmarshall(rawParcelData, 0, rawParcelData.length);
            appInfoParcel.setDataPosition(0);
            ApplicationInfo appInfo = ApplicationInfo.CREATOR.createFromParcel(appInfoParcel);
            appInfoParcel.recycle();
            if (appInfo != null) {
                handlePreloadApp(appInfo);
            } else {
                throw new IllegalArgumentException("Failed to deserialize --preload-app");
            }
            return null;
        }

        if (parsedArgs.mApiBlacklistExemptions != null) {
            return handleApiBlacklistExemptions(zygoteServer, parsedArgs.mApiBlacklistExemptions);
        }

        if (parsedArgs.mHiddenApiAccessLogSampleRate != -1
                || parsedArgs.mHiddenApiAccessStatslogSampleRate != -1) {
            return handleHiddenApiAccessLogSampleRate(zygoteServer,
                    parsedArgs.mHiddenApiAccessLogSampleRate,
                    parsedArgs.mHiddenApiAccessStatslogSampleRate);
        }

        if (parsedArgs.mPermittedCapabilities != 0 || parsedArgs.mEffectiveCapabilities != 0) {
            throw new ZygoteSecurityException("Client may not specify capabilities: "
                    + "permitted=0x" + Long.toHexString(parsedArgs.mPermittedCapabilities)
                    + ", effective=0x" + Long.toHexString(parsedArgs.mEffectiveCapabilities));
        }

        Zygote.applyUidSecurityPolicy(parsedArgs, peer);
        Zygote.applyInvokeWithSecurityPolicy(parsedArgs, peer);

        Zygote.applyDebuggerSystemProperty(parsedArgs);
        Zygote.applyInvokeWithSystemProperty(parsedArgs);

        int[][] rlimits = null;

        if (parsedArgs.mRLimits != null) {
            rlimits = parsedArgs.mRLimits.toArray(Zygote.INT_ARRAY_2D);
        }

        int[] fdsToIgnore = null;

        if (parsedArgs.mInvokeWith != null) {
            try {
                FileDescriptor[] pipeFds = Os.pipe2(O_CLOEXEC);
                childPipeFd = pipeFds[1];
                serverPipeFd = pipeFds[0];
                Os.fcntlInt(childPipeFd, F_SETFD, 0);
                fdsToIgnore = new int[]{childPipeFd.getInt$(), serverPipeFd.getInt$()};
            } catch (ErrnoException errnoEx) {
                throw new IllegalStateException("Unable to set up pipe for invoke-with", errnoEx);
            }
        }


        int [] fdsToClose = { -1, -1 };

        FileDescriptor fd = mSocket.getFileDescriptor();

        if (fd != null) {
            fdsToClose[0] = fd.getInt$();
        }

        fd = zygoteServer.getZygoteSocketFileDescriptor();

        if (fd != null) {
            fdsToClose[1] = fd.getInt$();
        }

        fd = null;

        //调用native的fork
        pid = Zygote.forkAndSpecialize(parsedArgs.mUid, parsedArgs.mGid, parsedArgs.mGids,
                parsedArgs.mRuntimeFlags, rlimits, parsedArgs.mMountExternal, parsedArgs.mSeInfo,
                parsedArgs.mNiceName, fdsToClose, fdsToIgnore, parsedArgs.mStartChildZygote,
                parsedArgs.mInstructionSet, parsedArgs.mAppDataDir, parsedArgs.mTargetSdkVersion);

        try {
            if (pid == 0) {
                // 在子进程中 首先关闭掉ServerSocket，因为它是Zygote孵化出来的 所以socket 需要关闭掉。
                zygoteServer.setForkChild();

                zygoteServer.closeServerSocket();
                IoUtils.closeQuietly(serverPipeFd);
                serverPipeFd = null;
                //调用HandleChildProc mStartChildZygote = --start-child-zygote
                return handleChildProc(parsedArgs, descriptors, childPipeFd,
                        parsedArgs.mStartChildZygote);
            } else {
                // In the parent. A pid < 0 indicates a failure and will be handled in
                // handleParentProc.
                IoUtils.closeQuietly(childPipeFd);
                childPipeFd = null;
                handleParentProc(pid, descriptors, serverPipeFd);
                return null;
            }
        } finally {
            IoUtils.closeQuietly(childPipeFd);
            IoUtils.closeQuietly(serverPipeFd);
        }
    }
```

processOneCommand客户端请求的处理函数:通过`Zygote.forkAndSpecialize`调用到native函数`nativeForkAndSpecialize`来fork出来客户端进程。

文件目录:/frameworks/base/core/com\_android\_internal\_os\_Zygote.cpp

```

static jint com_android_internal_os_Zygote_nativeForkAndSpecialize(
        JNIEnv* env, jclass, jint uid, jint gid, jintArray gids,
        jint runtime_flags, jobjectArray rlimits,
        jint mount_external, jstring se_info, jstring nice_name,
        jintArray managed_fds_to_close, jintArray managed_fds_to_ignore, jboolean is_child_zygote,
        jstring instruction_set, jstring app_data_dir) {
    ……………………
    pid_t pid = ForkCommon(env, false, fds_to_close, fds_to_ignore);

    if (pid == 0) {
      SpecializeCommon(env, uid, gid, gids, runtime_flags, rlimits,
                       capabilities, capabilities,
                       mount_external, se_info, nice_name, false,
                       is_child_zygote == JNI_TRUE, instruction_set, app_data_dir);
    }
    return pid;
}



static pid_t ForkCommon(JNIEnv* env, bool is_system_server,
                        const std::vector<int>& fds_to_close,
                        const std::vector<int>& fds_to_ignore) {
  SetSignalHandlers();
  ……………………
  pid_t pid = fork();//fork出来客户端请求的进程。

  if (pid == 0) {
    // The child process.
    PreApplicationInit();

    // Clean up any descriptors which must be closed immediately
    DetachDescriptors(env, fds_to_close, fail_fn);

    // Invalidate the entries in the USAP table.
    ClearUsapTable();

    // Re-open all remaining open file descriptors so that they aren't shared
    // with the zygote across a fork.
    gOpenFdTable->ReopenOrDetach(fail_fn);

    // Turn fdsan back on.
    android_fdsan_set_error_level(fdsan_error_level);
  } else {
    ALOGD("Forked child process %d", pid);
  }


```

fork出来进程之后在子进程中 首先关闭掉ServerSocket，因为它是Zygote孵化出来的 所以socket 需要关闭掉。 调用`handleParentProc` 设置子进程.

```
private Runnable handleChildProc(ZygoteArguments parsedArgs, FileDescriptor[] descriptors,
        FileDescriptor pipeFd, boolean isZygote) {

    closeSocket();//关闭socket
    if (descriptors != null) {
        try {
            Os.dup2(descriptors[0], STDIN_FILENO);
            Os.dup2(descriptors[1], STDOUT_FILENO);
            Os.dup2(descriptors[2], STDERR_FILENO);

            for (FileDescriptor fd: descriptors) {
                IoUtils.closeQuietly(fd);
            }
        } catch (ErrnoException ex) {
            Log.e(TAG, "Error reopening stdio", ex);
        }
    }

    if (parsedArgs.mNiceName != null) {
        Process.setArgV0(parsedArgs.mNiceName);
    }

    // End of the postFork event.
    Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
    if (parsedArgs.mInvokeWith != null) {
        WrapperInit.execApplication(parsedArgs.mInvokeWith,
                parsedArgs.mNiceName, parsedArgs.mTargetSdkVersion,
                VMRuntime.getCurrentInstructionSet(),
                pipeFd, parsedArgs.mRemainingArgs);

        // Should not get here.
        throw new IllegalStateException("WrapperInit.execApplication unexpectedly returned");
    } else {
        if (!isZygote) {//此时看客户端是否传递--start-child-zygote
            return ZygoteInit.zygoteInit(parsedArgs.mTargetSdkVersion,
                    parsedArgs.mRemainingArgs, null /* classLoader */);
        } else {
            return ZygoteInit.childZygoteInit(parsedArgs.mTargetSdkVersion,
                    parsedArgs.mRemainingArgs, null /* classLoader */);
        }
    }
}
```

关闭当前socket，然后根据客户端传递的参数来决定是走哪一个逻辑，如果不是child-zygote就会执行ZygoteInit.zygoteInit();

```
public static final Runnable zygoteInit(int targetSdkVersion, String[] argv,
        ClassLoader classLoader) {

    RuntimeInit.redirectLogStreams();

    RuntimeInit.commonInit();
    //调用了nativeZygoteInit
    ZygoteInit.nativeZygoteInit();
    return RuntimeInit.applicationInit(targetSdkVersion, argv, classLoader);
}
```

调用了native函数nativeZygoteInit 在AndroidRuntime中

```
static void com_android_internal_os_ZygoteInit_nativeZygoteInit(JNIEnv* env, jobject clazz)
{
    gCurRuntime->onZygoteInit();
}
```

调用到AppRuntime的onZygoteInit

```
virtual void onZygoteInit()
{
    //这个可以看我之前写的binder文章。让当前进程支持IPC通讯，binder。
    sp<ProcessState> proc = ProcessState::self();
    ALOGV("App process: starting thread pool.\n");
    proc->startThreadPool();
}
```

最后调用`RuntimeInit.applicationInit(targetSdkVersion, argv, classLoader)`

```

protected static Runnable applicationInit(int targetSdkVersion, String[] argv,
        ClassLoader classLoader) {
   
    nativeSetExitWithoutCleanup(true);
    VMRuntime.getRuntime().setTargetHeapUtilization(0.75f);
    VMRuntime.getRuntime().setTargetSdkVersion(targetSdkVersion);

    final Arguments args = new Arguments(argv);

    Trace.traceEnd(Trace.TRACE_TAG_ACTIVITY_MANAGER);
    //根据客户端请求args 来找到需要启动的class 执行Main函数
    return findStaticMain(args.startClass, args.startArgs, classLoader);
}


protected static Runnable findStaticMain(String className, String[] argv,
        ClassLoader classLoader) {
    Class<?> cl;

    try {
        cl = Class.forName(className, true, classLoader);
    } catch (ClassNotFoundException ex) {
        throw new RuntimeException(
                "Missing class when invoking static main " + className,
                ex);
    }

    Method m;
    try {
        m = cl.getMethod("main", new Class[] { String[].class });
    } catch (NoSuchMethodException ex) {
        throw new RuntimeException(
                "Missing static main on " + className, ex);
    } catch (SecurityException ex) {
        throw new RuntimeException(
                "Problem getting static main on " + className, ex);
    }

    int modifiers = m.getModifiers();
    if (! (Modifier.isStatic(modifiers) && Modifier.isPublic(modifiers))) {
        throw new RuntimeException(
                "Main method is not public and static on " + className);
    }

    return new MethodAndArgsCaller(m, argv);
}


```

最后返回MethodAndArgsCaller到ZygoteInit.java的Main函数中 执行对应的run 函数.也就是客户端请求的class的Main函数了。

```
static class MethodAndArgsCaller implements Runnable {
    /** method to call */
    private final Method mMethod;

    /** argument array */
    private final String[] mArgs;

    public MethodAndArgsCaller(Method method, String[] args) {
        mMethod = method;
        mArgs = args;
    }

    public void run() {
        try {
            mMethod.invoke(null, new Object[] { mArgs });
        } catch (IllegalAccessException ex) {
            throw new RuntimeException(ex);
        } catch (InvocationTargetException ex) {
            Throwable cause = ex.getCause();
            if (cause instanceof RuntimeException) {
                throw (RuntimeException) cause;
            } else if (cause instanceof Error) {
                throw (Error) cause;
            }
            throw new RuntimeException(ex);
        }
    }
}

```

以上 整个Zygote的流程就结束了。

## 总结

Zygote的代码流程总结完毕了，我们用文字再来总结下整体的流程。

## 文字总结：

1.Zygote的启动是init在解析init.rc文件的时候fork出来的进程，exec执行的/syste/bin/app\_process64程序。

2.在main函数中，Zygote创建了AppRuntime，判断是Zygote还是应用程序，这里我们是Zygote所以添加`start-system-server`参数调用runtime.start('com.android.internal.os.ZygoteInit',args,true)

3.AndroidRunTime::start中调用`startVM(env)`创建Java虚拟机，并且调用`StartReg(env)`注册JNI函数 例如：com/android/internal/os/ZygoteInit.nativeZygoteInit->com\_android\_internal\_os\_ZygoteInit\_nativeZygoteInit等.注册完成后通过JNI调用Java层的com.android.internal.os.ZygoteInit.Main函数。

4.Zygote的`Main`函数 根据之前传递的 `start-system-server` 将`startSystemServer`设置为True。调用`forkSystemServer`(当前的主角是Zygote.SystemServer这里我们没有分析，下次分析.)调用`preload()`//预加载一些类Class.forName（Activity、Fragment等） 以及主题、图片、颜色等资源。然后调用`runSelectLoop()`;函数进入等待状态，等待客户端的连接.

5.ZygoteServer的`runSelectLoop`是一个死循环，在循环中Zygote等到poll事件的到来，如果有客户端连接进来会调用`ZygoteConnect`的`processOneCommand()`

6.ZygoteConnect的`processOneCommand()`会先通过调用`com_android_internal_os_Zygote.cpp`的native函数`com_android_internal_os_Zygote_nativeForkAndSpecialize`fork出来对应的进程，fork出来后会关闭ServerSocket，因为子进程是Zygote孵化出来的，所以子进程fork出来后第一件事就是关闭掉socket。fork进程完毕之后调用`HandleChildProc`来设置子进程相关的信息，如果!isZygote会调用`ZygoteInit.zygoteInit(parsedArgs.mTargetSdkVersion, parsedArgs.mRemainingArgs, null)`返回到ZygoteInit执行zygoteInit()函数。

7.`ZygoteInit`的`zygoteInit`函数调用`nativeZygoteInit`最终调用到Native层`AndroidRuntime`中的`com_android_internal_os_ZygoteInit_nativeZygoteInit`它又调用到AppRuntime的onZygoteInit() 创建进程信息，初始化binder等。 `所以说Android的应用默认是支持跨进程(binder)通讯的`。（关于binder可以参考我之前的文章[Binder究竟是什么。](https://juejin.cn/post/7092230588664397860 "https://juejin.cn/post/7092230588664397860")最后调用`RuntimeInit.applicationInit()`根据客户端请求的参数 找到对应需要启动的class的Main函数包装成MethodAndArgsCaller().返回到Main函数中 执行caller.run()`也就是客户端请求的targetClass的Main函数`。

## 一张图来概括

![Zygote.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/99d6d04c9f634d6d89be0bf446f1ec5e~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

## 面试题

Q1.为什么预加载Class的时候要使用Class.forname呢？他和loadClass有什么区别？ A1:Class.forName除了把class加载到jvm中，还会对class进行解释，并且执行类中的static块。而loadClass只是把class加载到jvm中，只有在newinstance中才会执行static。

Q2.Zygote是用什么进行IPC通信的？是Binder吗？ A2:Zygote是C/S架构，通过Socket进行通信。不是Binder，Binder还没启动，在创建ProcessState之后才可以使用Binder。

Q3:简述Zygote的流程。 A3:1.创建虚拟机并注册JNI环境函数。2.forkSystemServer。3.预加载class 系统资源。4.循环等待客户端的连接。5.有客户端的连接后fork进程，初始化进程，创建ProcessState初始化binder。6.根据请求的targetClass 执行Main函数。

好了Zygote进程就到这里，下一次再来看看SystemServer。

视频地址： 在线: [www.bilibili.com/video/BV132…](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1324y1775V%2F%3Fshare_source%3Dcopy_web%26vd_source%3D0cf6c1c4827d129d9daea3a51b663710 "https://www.bilibili.com/video/BV1324y1775V/?share_source=copy_web&vd_source=0cf6c1c4827d129d9daea3a51b663710")

网盘: 链接: [pan.baidu.com/s/1mWAnjNnE…](https://link.juejin.cn/?target=https%3A%2F%2Fpan.baidu.com%2Fs%2F1mWAnjNnEknAET2M7WTz3yQ "https://pan.baidu.com/s/1mWAnjNnEknAET2M7WTz3yQ")

提取码: j4tg