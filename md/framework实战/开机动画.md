

## Android开机动画

> hongxi.zhu 2023-6-12  
> Lineageos\_20(Android T) on Pixel 2XL

#### 目录

+   [Android开机动画](#Android_0)
+   +   [一、 开机动画的启动](#__5)
    +   +   [1.1 init.rc启动相应的进程](#11_initrc_6)
        +   [1.2 surfaceflinger服务的启动](#12_surfaceflinger_74)
        +   [1.3 开机动画进程的启动](#13__141)
        +   +   [1.3.1 检查是否禁用了开机动画](#131__175)
            +   [1.3.2 创建BootAnimation对象，预加载动画文件](#132_BootAnimation_197)
            +   [1.3.3 等待SF服务的启动](#133_SF_465)
            +   [1.3.4 启动播放线程，播放动画](#134__492)
    +   [二、结束开机动画](#_807)
    +   +   [2.1 system server进程](#21_system_server_808)
        +   [2.2 SurfaceFlinger进程](#22_SurfaceFlinger_870)

### 一、 开机动画的启动

#### 1.1 init.rc启动相应的进程

开机动画跑起来除了需要自身进程的启动外，还肯定以来显示系统的相关进程，即一定需要SurfaceFlinger的进程的合成和送显，所以这里需要启动SurfaceFlinger服务和bootanim服务，两者是在init.rc中启动。rc的触发阶段和引用关系如下：

+   system/core/rootdir/init.rc

```shell
...
import /init.${ro.hardware}.rc
import /vendor/etc/init/hw/init.${ro.hardware}.rc
...

# Mount filesystems and start core system services.  //在这个阶段就可以启动系统核心的服务了，包括SF等
on late-init
	...
    # Mount fstab in init.{$device}.rc by mount_all with '--late' parameter
    # to only mount entries with 'latemount'. This is needed if '--early' is
    # specified in the previous mount_all command on the fs stage.
    # With /system mounted and properties form /system + /factory available,
    # some services can be started.
    trigger late-fs
	...
```

+   device/google/wahoo/init.hardware.rc

```shell
on late-fs
    # Start devices by sysfs trigger
    start vendor.devstart_sh
    # Start services for bootanim
    start vendor.power-hal-1-3
    start surfaceflinger  # 先启动surfaceflinger
    start bootanim        # 然后是bootanim
    start vendor.hwcomposer-2-1
    start vendor.configstore-hal
    start vendor.gralloc-2-0
	...
```

这里说下有时候这个文件源码里名字和设备里的名字不一样，是因为这个文件编译时会改名并拷贝到`vendor/etc/init/hw/init.taimen.rc`，可以从`device.mk`看出，我的设备Pixel 2XL taimen派生于wahoo，会引用wahoo的device.mk

+   device/google/wahoo/device.mk

```shell
  PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.hardware.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.$(PRODUCT_HARDWARE).rc
```

上面init.hardware.rc中start的`surfaceflinger`和`bootanim`是在它们各自的module下声明的`rc`文件，当build的时候，编译系统会解析`Android.bp`或者`Android.mk`获取对应的目标的rc文件，并将rc的内容`include`到主rc中。

+   SurfaceFlinger服务的启动文件  
    frameworks/native/services/surfaceflinger/surfaceflinger.rc

```shell
service surfaceflinger /system/bin/surfaceflinger
    class core animation
    user system
    group graphics drmrpc readproc
    capabilities SYS_NICE
    onrestart restart --only-if-running zygote
    task_profiles HighPerformance
    socket pdx/system/vr/display/client     stream 0666 system graphics u:object_r:pdx_display_client_endpoint_socket:s0
    socket pdx/system/vr/display/manager    stream 0666 system graphics u:object_r:pdx_display_manager_endpoint_socket:s0
    socket pdx/system/vr/display/vsync      stream 0666 system graphics u:object_r:pdx_display_vsync_endpoint_socket:s0
```

+   bootanim服务的启动文件  
    frameworks/base/cmds/bootanimation/bootanim.rc

```shell
service bootanim /system/bin/bootanimation
    class core animation
    user graphics
    group graphics audio
    disabled
    oneshot
    ioprio rt 0
    task_profiles MaxPerformance
```

#### 1.2 surfaceflinger服务的启动

动画播放依赖于SF进程，所以它希望先启动（无法保证两者一定能有序的启动），SF进程启动后会加载它的`main`方法  
frameworks/native/services/surfaceflinger/main\_surfaceflinger.cpp

```cpp
int main(int, char**) {
	...
    // instantiate surfaceflinger
    sp<SurfaceFlinger> flinger = surfaceflinger::createSurfaceFlinger();
	...
    // initialize before clients can connect
    flinger->init(); // 这里init会去设置开机动画相关的属性

    // publish surface flinger
    sp<IServiceManager> sm(defaultServiceManager());
    sm->addService(String16(SurfaceFlinger::getServiceName()), flinger, false,
                   IServiceManager::DUMP_FLAG_PRIORITY_CRITICAL | IServiceManager::DUMP_FLAG_PROTO);

    // publish gui::ISurfaceComposer, the new AIDL interface
    sp<SurfaceComposerAIDL> composerAIDL = new SurfaceComposerAIDL(flinger);
    sm->addService(String16("SurfaceFlingerAIDL"), composerAIDL, false,
                   IServiceManager::DUMP_FLAG_PRIORITY_CRITICAL | IServiceManager::DUMP_FLAG_PROTO);

    startDisplayService(); // dependency on SF getting registered above
	...
    // run surface flinger in this thread
    flinger->run();  //SurfaceFinger启动，并运行在主线程，通过消息机制循环等待任务

    return 0;
}
```

frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp

```cpp
// Do not call property_set on main thread which will be blocked by init
// Use StartPropertySetThread instead.
void SurfaceFlinger::init() {
    ALOGI(  "SurfaceFlinger's main thread ready to run. "
            "Initializing graphics H/W...");
	...
    mStartPropertySetThread = getFactory().createStartPropertySetThread(presentFenceReliable);
	// 开启一个子现场去设置开机动画相关的属性（主线程设置属性会造成block）
    if (mStartPropertySetThread->Start() != NO_ERROR) {
        ALOGE("Run StartPropertySetThread failed!");
    }

    ALOGV("Done initializing");
}
```

frameworks/native/services/surfaceflinger/StartPropertySetThread.cpp

```cpp
bool StartPropertySetThread::threadLoop() {
    // Set property service.sf.present_timestamp, consumer need check its readiness
    property_set(kTimestampProperty, mTimestampPropertyValue ? "1" : "0");
    // Clear BootAnimation exit flag
    property_set("service.bootanim.exit", "0");  //重置开机动画退出属性
    property_set("service.bootanim.progress", "0");  // 开机动画进度
    // Start BootAnimation if not started
    property_set("ctl.start", "bootanim");  //启动开机动画的属性，这里保险起见还是会去启动bootanim服务，但是bootanim服务有可能在前面已经被init进程拉起来了
    // Exit immediately
    return false;  //只执行一次，设置完就退出
}
```

这里主要是做了两个事：

1.  重置开机动画的退出属性，开机动画进程会循环check这个属性，如果为`1`就结束播放并退出，这里先初始化为0
2.  设置开机动画启动属性，如果开机动画进程没有被前面的init进程拉起来，那么这里还会再次主动让属性服务拉起开机动画进程。

接下来就是进入启动开机动画进程的流程了。

#### 1.3 开机动画进程的启动

frameworks/base/cmds/bootanimation/bootanimation\_main.cpp

```cpp
int main()
{
    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_DISPLAY);

    bool noBootAnimation = bootAnimationDisabled();  //检查是否禁用了开机动画
    ALOGI_IF(noBootAnimation,  "boot animation disabled");
    if (!noBootAnimation) {

        sp<ProcessState> proc(ProcessState::self());
        ProcessState::self()->startThreadPool();

        // create the boot animation object (may take up to 200ms for 2MB zip)
        sp<BootAnimation> boot = new BootAnimation(audioplay::createAnimationCallbacks()); //创建动画对象，这个过程会去解析的动画文件，文件越大越耗时

        waitForSurfaceFlinger();  //向servicemanager检查SF进程是否正常启动了, 死循环等待SF注册成功

        boot->run("BootAnimation", PRIORITY_DISPLAY); //开始跑动画逻辑

        ALOGV("Boot animation set up. Joining pool.");

        IPCThreadState::self()->joinThreadPool();
    }
    return 0;
}
```

主要是：

+   检查是否禁用了开机动画，如果禁用了进程直接结束，不播放Android动画（也许厂商自己用其他方式实现）
+   创建BootAnimation对象，加载并解析动画文件，加载时间根据文件大小有关（图片大小，数量）
+   循环等待SurfaceFlinger服务的启动（没有SF也播放不了动画）
+   启动动画线程(BootAnimation继承于Thread类，它本身就是一个thread)，开始播放动画

##### 1.3.1 检查是否禁用了开机动画

```cpp
bool bootAnimationDisabled() {
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.sf.nobootanimation", value, "0");
    if (atoi(value) > 0) {
        return true;
    }

    property_get("ro.boot.quiescent", value, "0");
    if (atoi(value) > 0) {
        // Only show the bootanimation for quiescent boots if this system property is set to enabled
        if (!property_get_bool("ro.bootanim.quiescent.enabled", false)) {
            return true;
        }
    }

    return false;
}
```

主要受三个属性控制，三个属性优先级有前后。

##### 1.3.2 创建BootAnimation对象，预加载动画文件

```cpp
BootAnimation::BootAnimation(sp<Callbacks> callbacks)
        : Thread(false), mLooper(new Looper(false)), mClockEnabled(true), mTimeIsAccurate(false),
        mTimeFormat12Hour(false), mTimeCheckThread(nullptr), mCallbacks(callbacks) {
    mSession = new SurfaceComposerClient(); // 获取SF在开机动画进程的代理，后面会使用该对象与SF跨进程通信

    std::string powerCtl = android::base::GetProperty("sys.powerctl", "");
    if (powerCtl.empty()) { // 判断是开机动画还是关机动画
        mShuttingDown = false;
    } else {
        mShuttingDown = true;
    }
    ALOGD("%sAnimationStartTiming start time: %" PRId64 "ms", mShuttingDown ? "Shutdown" : "Boot",
            elapsedRealtime());
}
```

上面的构造方法中仅仅中只是获取了SF的session代理对象, 真正的加载逻辑在这个对象的第一次引用回调方法中（在前面的sp指针实例化时被回调）。

```cpp
void BootAnimation::onFirstRef() {
    status_t err = mSession->linkToComposerDeath(this);
    SLOGE_IF(err, "linkToComposerDeath failed (%s) ", strerror(-err));
    if (err == NO_ERROR) {
        // Load the animation content -- this can be slow (eg 200ms)
        // called before waitForSurfaceFlinger() in main() to avoid wait
        ALOGD("%sAnimationPreloadTiming start time: %" PRId64 "ms",
                mShuttingDown ? "Shutdown" : "Boot", elapsedRealtime());
        preloadAnimation();
        ALOGD("%sAnimationPreloadStopTiming start time: %" PRId64 "ms",
                mShuttingDown ? "Shutdown" : "Boot", elapsedRealtime());
    }
}
```

加载的逻辑主要是`preloadAnimation()`，这个过程主要是解析`bootanimation.zip`文件

```cpp
bool BootAnimation::preloadAnimation() {
    findBootAnimationFile();  // 检索系统的几个预设定路径下是否存在bootanimation.zip文件
    if (!mZipFileName.isEmpty()) {
        mAnimation = loadAnimation(mZipFileName);  // 加载动画文件
        return (mAnimation != nullptr);
    }

    return false;
}
```

`findBootAnimationFile`方法是去检索系统的几个预设定路径下是否存在bootanimation.zip文件, 如果存在赋值给mZipFileName, 几个预设定路径是：（寻找是根据系统是否加密、是否是深色主题进行选择，pixel 2xl的文件是 /product/media/下）

```cpp
static const char OEM_BOOTANIMATION_FILE[] = "/oem/media/bootanimation.zip";
static const char PRODUCT_BOOTANIMATION_DARK_FILE[] = "/product/media/bootanimation-dark.zip";
static const char PRODUCT_BOOTANIMATION_FILE[] = "/product/media/bootanimation.zip";
static const char SYSTEM_BOOTANIMATION_FILE[] = "/system/media/bootanimation.zip";
static const char APEX_BOOTANIMATION_FILE[] = "/apex/com.android.bootanimation/etc/bootanimation.zip";
static const char PRODUCT_ENCRYPTED_BOOTANIMATION_FILE[] = "/product/media/bootanimation-encrypted.zip";
static const char SYSTEM_ENCRYPTED_BOOTANIMATION_FILE[] = "/system/media/bootanimation-encrypted.zip";
```

`loadAnimation()`方法是去解析`bootanimation.zip`文件

```cpp
BootAnimation::Animation* BootAnimation::loadAnimation(const String8& fn) {
    if (mLoadedFiles.indexOf(fn) >= 0) {  //mLoadedFiles是根据文件名是否加载过来防止加载动画zip包多次
        SLOGE("File \"%s\" is already loaded. Cyclic ref is not allowed",
            fn.string());
        return nullptr;
    }
    ZipFileRO *zip = ZipFileRO::open(fn);  //打开zip文件
    if (zip == nullptr) {
        SLOGE("Failed to open animation zip \"%s\": %s",
            fn.string(), strerror(errno));
        return nullptr;
    }

    ALOGD("%s is loaded successfully", fn.string());

    Animation *animation =  new Animation;
    animation->fileName = fn;
    animation->zip = zip;
    animation->clockFont.map = nullptr;
    mLoadedFiles.add(animation->fileName);  //整个zip也用一个Animation来描述

    parseAnimationDesc(*animation);  //解析‘desc.txt’文件，根据这个文件创建每个part的Animation对象
    if (!preloadZip(*animation)) {  //加载每一个part对应的图片，并填充到animation.part.frames
        releaseAnimation(animation);
        return nullptr;
    }

    mLoadedFiles.remove(fn);
    return animation;
}
```

解析`desc.txt`文件

```cpp
bool BootAnimation::parseAnimationDesc(Animation& animation)  {
	...
    // Parse the description file
    for (;;) {
        const char* endl = strstr(s, "\n");
        if (endl == nullptr) break;
        String8 line(s, endl - s);
        const char* l = line.string();
        int fps = 0;
        int width = 0;
        int height = 0;
        int count = 0;
        int pause = 0;
        int progress = 0;
        int framesToFadeCount = 0;
        int colorTransitionStart = 0;
        int colorTransitionEnd = 0;
        char path[ANIM_ENTRY_NAME_MAX];
        char color[7] = "000000"; // default to black if unspecified
        char clockPos1[TEXT_POS_LEN_MAX + 1] = "";
        char clockPos2[TEXT_POS_LEN_MAX + 1] = "";
        char dynamicColoringPartNameBuffer[ANIM_ENTRY_NAME_MAX];
        char pathType;
        // start colors default to black if unspecified
        char start_color_0[7] = "000000";
        char start_color_1[7] = "000000";
        char start_color_2[7] = "000000";
        char start_color_3[7] = "000000";

        int nextReadPos;

        int topLineNumbers = sscanf(l, "%d %d %d %d", &width, &height, &fps, &progress);
        if (topLineNumbers == 3 || topLineNumbers == 4) { //解析第一行，获取width/height/fps参数
            // SLOGD("> w=%d, h=%d, fps=%d, progress=%d", width, height, fps, progress);
            animation.width = width;
            animation.height = height;
            animation.fps = fps;
            if (topLineNumbers == 4) {  //如果配置了progress，一般不会配置
              animation.progressEnabled = (progress != 0);
            } else {
              animation.progressEnabled = false;
            }
        } else if (sscanf(l, "dynamic_colors %" STRTO(ANIM_PATH_MAX) "s #%6s #%6s #%6s #%6s %d %d",
            dynamicColoringPartNameBuffer,加载每一个part对应的图片，并填充到animation.part.frames
            start_color_0, start_color_1, start_color_2, start_color_3,
            &colorTransitionStart, &colorTransitionEnd)) {  //动态颜色，我们一般不配置，所以不会走这里
            animation.dynamicColoringEnabled = true;
            parseColor(start_color_0, animation.startColors[0]);
            parseColor(start_color_1, animation.startColors[1]);
            parseColor(start_color_2, animation.startColors[2]);
            parseColor(start_color_3, animation.startColors[3]);
            animation.colorTransitionStart = colorTransitionStart;
            animation.colorTransitionEnd = colorTransitionEnd;
            dynamicColoringPartName = std::string(dynamicColoringPartNameBuffer);
        } else if (sscanf(l, "%c %d %d %" STRTO(ANIM_PATH_MAX) "s%n",
                          &pathType, &count, &pause, path, &nextReadPos) >= 4) {  //解析第二行开始的part部分
            if (pathType == 'f') {
                sscanf(l + nextReadPos, " %d #%6s %16s %16s", &framesToFadeCount, color, clockPos1,
                       clockPos2);
            } else {
                sscanf(l + nextReadPos, " #%6s %16s %16s", color, clockPos1, clockPos2);
            }
            // SLOGD("> type=%c, count=%d, pause=%d, path=%s, framesToFadeCount=%d, color=%s, "
            //       "clockPos1=%s, clockPos2=%s",
            //       pathType, count, pause, path, framesToFadeCount, color, clockPos1, clockPos2);
            Animation::Part part; 
            if (path == dynamicColoringPartName) {
                // Part is specified to use dynamic coloring.
                part.useDynamicColoring = true;
                part.postDynamicColoring = false;
                postDynamicColoring = true;
            } else {   //我们一般只配置pathType、count、pause，其他的都是空
                // Part does not use dynamic coloring.
                part.useDynamicColoring = false;
                part.postDynamicColoring =  postDynamicColoring;
            }
            part.playUntilComplete = pathType == 'c';
            part.framesToFadeCount = framesToFadeCount;
            part.count = count;
            part.pause = pause;加载
            part.path = path;
            part.audioData = nullptr;  //新版本还加入了开机动画每个part可以添加对应的音频文件，但是一般不配置，且不是在这里加载
            part.animation = nullptr;  //这里每个part下面一般不会再嵌套动画文件了，所以它已经是节点了
            if (!parseColor(color, part.backgroundColor)) {  //color和backgroundcolor没有特殊配置使用默认的black
                SLOGE("> invalid color '#%s'", color);
                part.backgroundColor[0] = 0.0f;
                part.backgroundColor[1] = 0.0f;
                part.backgroundColor[2] = 0.0f;
            }
            parsePosition(clockPos1, clockPos2, &part.clockPosX, &part.clockPosY);
            animation.parts.add(part);  //将part加入animation对象中，这样就能拿到他的fps/type/count/pause, 但是frames数据还没有填充
        }
        else if (strcmp(l, "$SYSTEM") == 0) {  //一种特殊情况，不会走这里
            // SLOGD("> SYSTEM");
            Animation::Part part;
            part.playUntilComplete = false;
            part.framesToFadeCount = 0;
            part.count = 1;
            part.pause = 0;
            part.audioData = nullptr;
            part.animation = loadAnimation(String8(SYSTEM_BOOTANIMATION_FILE));
            if (part.animation != nullptr)
                animation.parts.add(part);
        }
        s = ++endl;
    }

    return true;
}
```

解析完`desc.txt`，已经将配置文件中每个part对应到每个实际的part文件夹，但是真正的图片，还没有去加载，接下来`preloadZip`加载每一个part对应的图片，并填充到`animation.part.frames`

```cpp
bool BootAnimation::preloadZip(Animation& animation) {
    // read all the data structures
    const size_t pcount = animation.parts.size();
    void *cookie = nullptr;
    ZipFileRO* zip = animation.zip;
    if (!zip->startIteration(&cookie)) {
        return false;
    }

    ZipEntryRO entry;
    char name[ANIM_ENTRY_NAME_MAX];
    while ((entry = zip->nextEntry(cookie)) != nullptr) {  //遍历整个zip下的文件树，一般我们都不会嵌套多层，叶子节点就是每个part文件夹下的文件
        const int foundEntryName = zip->getEntryFileName(entry, name, ANIM_ENTRY_NAME_MAX);
        if (foundEntryName > ANIM_ENTRY_NAME_MAX || foundEntryName == -1) {
            SLOGE("Error fetching entry file name");
            continue;
        }

        const String8 entryName(name);
        const String8 path(entryName.getPathDir());
        const String8 leaf(entryName.getPathLeaf());
        if (leaf.size() > 0) {
			...
            for (size_t j = 0; j < pcount; j++) {  //遍历每个part
                if (path == animation.parts[j].path) {
                    uint16_t method;
                    // supports only stored png files
                    if (zip->getEntryInfo(entry, &method, nullptr, nullptr, nullptr, nullptr, nullptr)) {
                        if (method == ZipFileRO::kCompressStored) {  //从直接可以知道bootanimation.zip必须使用存储方式打包，不能压缩，否则将不能播放
                            FileMap* map = zip->createEntryFileMap(entry);
                            if (map) {
                                Animation::Part& part(animation.parts.editItemAt(j));
                                if (leaf == "audio.wav") { // 如果是音频文件
                                    // a part may have at most one audio file
                                    part.audioData = (uint8_t *)map->getDataPtr();
                                    part.audioLength = map->getDataLength();
                                } else if (leaf == "trim.txt") {  //一般不会使用trim
                                    part.trimData.setTo((char const*)map->getDataPtr(),
                                                        map->getDataLength());
                                } else {
                                    Animation::Frame frame;
                                    frame.name = leaf;
                                    frame.map = map;
                                    frame.trimWidth = animation.width;
                                    frame.trimHeight = animation.height;
                                    frame.trimX = 0;
                                    frame.trimY = 0;
                                    part.frames.add(frame); // 将part下面的图片添加到part的frames
                                }
                            }
                        } else {
                            SLOGE("bootanimation.zip is compressed; must be only stored");
                        }
                    }
                }
            }
        }
    }
	...
    zip->endIteration(cookie);

    return true;
}
```

到这里动画文件的预加载就完成了，这时候需要check SF是否已经正常启动。

##### 1.3.3 等待SF服务的启动

frameworks/base/cmds/bootanimation/BootAnimationUtil.cpp

```cpp
void waitForSurfaceFlinger() {
    // TODO: replace this with better waiting logic in future, b/35253872
    int64_t waitStartTime = elapsedRealtime();
    sp<IServiceManager> sm = defaultServiceManager();
    const String16 name("SurfaceFlinger");
    const int SERVICE_WAIT_SLEEP_MS = 100;
    const int LOG_PER_RETRIES = 10;
    int retry = 0;
    while (sm->checkService(name) == nullptr) {
        retry++;
        if ((retry % LOG_PER_RETRIES) == 0) {
            ALOGW("Waiting for SurfaceFlinger, waited for %" PRId64 " ms",
                  elapsedRealtime() - waitStartTime);
        }
        usleep(SERVICE_WAIT_SLEEP_MS * 1000);
    };
    int64_t totalWaited = elapsedRealtime() - waitStartTime;
    if (totalWaited > SERVICE_WAIT_SLEEP_MS) {
        ALOGI("Waiting for SurfaceFlinger took %" PRId64 " ms", totalWaited);
    }
}
```

向ServiceMananger询问SF是否注册，如果没注册死循环，每秒问一次，启动了就开始播放动画

##### 1.3.4 启动播放线程，播放动画

前面的`main`方法的`boot->run("BootAnimation", PRIORITY_DISPLAY);`会先走BootAnimation线程的`readyToRun`, 然后才执行真正的线程循环体`threadLoop`，先看下`readyToRun`

+   readyToRun

```cpp
status_t BootAnimation::readyToRun() {
	...
    mDisplayToken = SurfaceComposerClient::getInternalDisplayToken();  //获取DisplayToken， 用于获取Display(屏幕)的参数
    if (mDisplayToken == nullptr)
        return NAME_NOT_FOUND;

    DisplayMode displayMode;
    const status_t error =
            SurfaceComposerClient::getActiveDisplayMode(mDisplayToken, &displayMode);  //获取DisplayMode，用于获取分辨率等信息
	...
    resolution = limitSurfaceSize(resolution.width, resolution.height);
    // create the native surface
    sp<SurfaceControl> control = session()->createSurface(String8("BootAnimation"),
            resolution.getWidth(), resolution.getHeight(), PIXEL_FORMAT_RGB_565);  //创建Surface并返回一个surfacecontrol对象，动画需要他通过opengl绘制到surface上

    SurfaceComposerClient::Transaction t;  //创建与SF通信的事务
	...
	
    // Scale forced resolution to physical resolution
    Rect forcedRes(0, 0, resolution.width, resolution.height);
    Rect physRes(0, 0, displayMode.resolution.width, displayMode.resolution.height);
    t.setDisplayProjection(mDisplayToken, ui::ROTATION_0, forcedRes, physRes);  //设置屏幕的投影参数

    t.setLayer(control, 0x40000000) //将Layer和SurfaceControl绑定
        .apply(); //提交事务

    sp<Surface> s = control->getSurface(); //获取一个surface

	//初始化opengl and egl
    // initialize opengl and egl
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);
    EGLConfig config = getEglConfig(display);
    EGLSurface surface = eglCreateWindowSurface(display, config, s.get(), nullptr);
    // Initialize egl context with client version number 2.0.
    EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttributes);
    EGLint w, h;
    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE)
        return NO_INIT;

    mDisplay = display;
    mContext = context;
    mSurface = surface;
    mInitWidth = mWidth = w;
    mInitHeight = mHeight = h;
    mFlingerSurfaceControl = control;
    mFlingerSurface = s;
    mTargetInset = -1;
    
	...
	
    projectSceneToWindow(); // 裁剪窗口、转化坐标

    // Register a display event receiver
    mDisplayEventReceiver = std::make_unique<DisplayEventReceiver>();
    status_t status = mDisplayEventReceiver->initCheck();
    SLOGE_IF(status != NO_ERROR, "Initialization of DisplayEventReceiver failed with status: %d",
            status);
    mLooper->addFd(mDisplayEventReceiver->getFd(), 0, Looper::EVENT_INPUT,
            new DisplayEventCallback(this), nullptr);

    return NO_ERROR;
}

```

+   threadLoop

```cpp
bool BootAnimation::threadLoop() {
    bool result;
    initShaders();  //初始化opengl着色器

    // We have no bootanimation file, so we use the stock android logo
    // animation.
    if (mZipFileName.isEmpty()) {
        ALOGD("No animation file");
        result = android();  //当我们没有定制bootanimation.zip（不存在这个文件）时，系统会去播放assets下面那两张图片，一个android字样的闪烁动画
    } else {
        result = movie();  //当存在bootanimation.zip时，走这个分支
    }

    mCallbacks->shutdown();
    eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(mDisplay, mContext);
    eglDestroySurface(mDisplay, mSurface);
    mFlingerSurface.clear();
    mFlingerSurfaceControl.clear();
    eglTerminate(mDisplay);
    eglReleaseThread();
    IPCThreadState::self()->stopProcess();
    return result;
}
```

我们主要看下`movie`的情况，这个名字也是很直白，开机动画的实现是图片逐帧动画，和电影的那种原理相同

```cpp
bool BootAnimation::movie() {
	...
    // mCallbacks->init() may get called recursively,
    // this loop is needed to get the same results
    for (const Animation::Part& part : mAnimation->parts) {
        if (part.animation != nullptr) {
            mCallbacks->init(part.animation->parts); // 初始化每一个part的音频（一般没音频）
        }
    }
    mCallbacks->init(mAnimation->parts);
	...
	//配置Blend
    // Blend required to draw time on top of animation frames.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DITHER);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
	// 开启2D纹理的配置
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    bool clockFontInitialized = false;
    if (mClockEnabled) {  //一般不配置显示这个时钟，不走这里
        clockFontInitialized =
            (initFont(&mAnimation->clockFont, CLOCK_FONT_ASSET) == NO_ERROR);
        mClockEnabled = clockFontInitialized;
    }

    initFont(&mAnimation->progressFont, PROGRESS_FONT_ASSET);  //初始化字体。用于绘制始终和进度，一般不配置
	...
    playAnimation(*mAnimation);  //真正的播放
	...
    releaseAnimation(mAnimation);
    mAnimation = nullptr;

    return false;
}
```

最终的播放逻辑在`playAnimation`中

```cpp
bool BootAnimation::playAnimation(const Animation& animation) {
    const size_t pcount = animation.parts.size();
    nsecs_t frameDuration = s2ns(1) / animation.fps;

    SLOGD("%sAnimationShownTiming start time: %" PRId64 "ms", mShuttingDown ? "Shutdown" : "Boot",
            elapsedRealtime());

    int fadedFramesCount = 0;
    int lastDisplayedProgress = 0;
    int colorTransitionStart = animation.colorTransitionStart;
    int colorTransitionEnd = animation.colorTransitionEnd;
    for (size_t i=0 ; i<pcount ; i++) {
        const Animation::Part& part(animation.parts[i]);
        const size_t fcount = part.frames.size();

        // Handle animation package
        if (part.animation != nullptr) {  //如果存在嵌套动画文件，就递归去播放，一般没有
            playAnimation(*part.animation);
            if (exitPending())
                break;
            continue; //to next part
        }

        // process the part not only while the count allows but also if already fading
        //如果part的count = 0 或者 还没有循环到part的count次数，就继续循环
        for (int r=0 ; !part.count || r<part.count || fadedFramesCount > 0 ; r++) {
            if (shouldStopPlayingPart(part, fadedFramesCount, lastDisplayedProgress)) break; //每次循环进入播放part时检查是否应该退出，判断是否boot完成且part的repeat-type不是c,即p时才能退出
			...
            mCallbacks->playPart(i, part, r); //播放part里面的音频，一般没音频

            glClearColor(
                    part.backgroundColor[0],
                    part.backgroundColor[1],
                    part.backgroundColor[2],
                    1.0f);

            ALOGD("Playing files = %s/%s, Requested repeat = %d, playUntilComplete = %s",
                    animation.fileName.string(), part.path.string(), part.count,
                    part.playUntilComplete ? "true" : "false");

            // For the last animation, if we have progress indicator from
            // the system, display it.
            int currentProgress = android::base::GetIntProperty(PROGRESS_PROP_NAME, 0);
            bool displayProgress = animation.progressEnabled &&
                (i == (pcount -1)) && currentProgress != 0;

            for (size_t j=0 ; j<fcount ; j++) {
                if (shouldStopPlayingPart(part, fadedFramesCount, lastDisplayedProgress)) break;  // 每次循环进入播放每一个图片时检查是否应该退出，判断是否boot完成且part的repeat-type不是c,即p时才能退出
				...
                processDisplayEvents();  //轮询是否有display的回调事件

                const double ratio_w = static_cast<double>(mWidth) / mInitWidth;
                const double ratio_h = static_cast<double>(mHeight) / mInitHeight;
                const int animationX = (mWidth - animation.width * ratio_w) / 2;
                const int animationY = (mHeight - animation.height * ratio_h) / 2;

                const Animation::Frame& frame(part.frames[j]);  //拿到具体part中的某张图片
                nsecs_t lastFrame = systemTime();

                if (r > 0) {
                    glBindTexture(GL_TEXTURE_2D, frame.tid);
                } else {  //如果是第一次播放图片（第一张图片）
                    glGenTextures(1, &frame.tid);  //生成纹理
                    glBindTexture(GL_TEXTURE_2D, frame.tid);  //将图片的句柄和纹理绑定
                    int w, h;
                    // Set decoding option to alpha unpremultiplied so that the R, G, B channels
                    // of transparent pixels are preserved.
                    initTexture(frame.map, &w, &h, false /* don't premultiply alpha */);  //初始化纹理
                }

                const int trimWidth = frame.trimWidth * ratio_w;
                const int trimHeight = frame.trimHeight * ratio_h;
                const int trimX = frame.trimX * ratio_w;
                const int trimY = frame.trimY * ratio_h;
                const int xc = animationX + trimX;
                const int yc = animationY + trimY;
                glClear(GL_COLOR_BUFFER_BIT);
                // specify the y center as ceiling((mHeight - frame.trimHeight) / 2)
                // which is equivalent to mHeight - (yc + frame.trimHeight)
                const int frameDrawY = mHeight - (yc + trimHeight);

                float fade = 0;
				...
                glUseProgram(mImageShader);
                glUniform1i(mImageTextureLocation, 0);
                glUniform1f(mImageFadeLocation, fade);
                if (animation.dynamicColoringEnabled) {
                    glUniform1f(mImageColorProgressLocation, colorProgress);
                }
                glEnable(GL_BLEND);
                drawTexturedQuad(xc, frameDrawY, trimWidth, trimHeight);
                glDisable(GL_BLEND);

				...

                handleViewport(frameDuration);  //处理视口的区域的变化

                eglSwapBuffers(mDisplay, mSurface);  //交换显示buffer,显示出来

                nsecs_t now = systemTime();
                nsecs_t delay = frameDuration - (now - lastFrame);
                //SLOGD("%lld, %lld", ns2ms(now - lastFrame), ns2ms(delay));
                lastFrame = now;

                if (delay > 0) {
                    struct timespec spec;
                    spec.tv_sec  = (now + delay) / 1000000000;
                    spec.tv_nsec = (now + delay) % 1000000000;
                    int err;
                    do {
                        err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &spec, nullptr);
                    } while (err == EINTR);
                }

                checkExit();  //每一个帧显示完检查下是否需要退出,具体就是去查询boot是否完成，如果boot完成SF会设置某个系统属性，那么就会设置线程的请求退出的标志
            }

            usleep(part.pause * ns2us(frameDuration)); //播放完本次part了，如果pause ！= 0 就需要延时（pause * frame时长）的时间再接着往下播放当前part的下一个循环或者下一个part

			//这里的判断很重要，从前面part的for循环和退出判断可知，当part的repeat-type为c时，且count = 0时，
			//即使boot完成了也不会退出循环，会进行播放，但是什么时候退出呢，答案就是下面的判断，如果boot完成了（exitPending() = true）且part = 0, 即part的配置为（c 0 x）时会结束循环，
			//所以可知当配置了part repeat为c时，那么这个part至少会播放一次，如果一个zip里有多个c-part时，
			//每个c-part都至少播放一次，无论它的count是0还是非0,即使boot完成。
            if (exitPending() && !part.count && mCurrentInset >= mTargetInset &&
                !part.hasFadingPhase()) {
                if (lastDisplayedProgress != 0 && lastDisplayedProgress != 100) {
                    android::base::SetProperty(PROGRESS_PROP_NAME, "100");
                    continue;
                }
                break; // exit the infinite non-fading part when it has been played at least once
            }
        }
    }

    // Free textures created for looping parts now that the animation is done.
    for (const Animation::Part& part : animation.parts) {
        if (part.count != 1) {
            const size_t fcount = part.frames.size();
            for (size_t j = 0; j < fcount; j++) {
                const Animation::Frame& frame(part.frames[j]);
                glDeleteTextures(1, &frame.tid);
            }
        }
    }

    ALOGD("%sAnimationShownTiming End time: %" PRId64 "ms", mShuttingDown ? "Shutdown" : "Boot",
            elapsedRealtime());

    return true;
}

void BootAnimation::checkExit() {
    // Allow surface flinger to gracefully request shutdown  //从这里我们也可以知道是SF直接设置这个属性让开机动画退出
    char value[PROPERTY_VALUE_MAX];
    property_get(EXIT_PROP_NAME, value, "0"); //service.bootanim.exit = 1，则调用requestExit()
    int exitnow = atoi(value);
    if (exitnow) {
        requestExit();  //这个是Thread类的方法，调用这个方法后exitPending() = true
    }
}

bool BootAnimation::shouldStopPlayingPart(const Animation::Part& part,
                                          const int fadedFramesCount,
                                          const int lastDisplayedProgress) {
    // stop playing only if it is time to exit and it's a partial part which has been faded out
    return exitPending() && !part.playUntilComplete && fadedFramesCount >= part.framesToFadeCount &&
        (lastDisplayedProgress == 0 || lastDisplayedProgress == 100);  //part.playUntilComplete就是part的repeat type ->c或者p
}
```

### 二、结束开机动画

#### 2.1 system server进程

开机动画的结束是开机过程中，系统完成开机完成，即将显示Home应用时调用的，具体是在`performEnableScreen`方法中  
framework/base/services/core/java/com/android/server/wm/WindowManagerService.java

```java
    private void performEnableScreen() {
        synchronized (mGlobalLock) {
            ProtoLog.i(WM_DEBUG_BOOT, "performEnableScreen: mDisplayEnabled=%b"
                            + " mForceDisplayEnabled=%b" + " mShowingBootMessages=%b"
                            + " mSystemBooted=%b mOnlyCore=%b. %s", mDisplayEnabled,
                    mForceDisplayEnabled, mShowingBootMessages, mSystemBooted, mOnlyCore,
                    new RuntimeException("here").fillInStackTrace());
    		...
    		
            if (!mBootAnimationStopped) {
                Trace.asyncTraceBegin(TRACE_TAG_WINDOW_MANAGER, "Stop bootanim", 0);
                // stop boot animation
                // formerly we would just kill the process, but we now ask it to exit so it
                // can choose where to stop the animation.
                SystemProperties.set("service.bootanim.exit", "1");  //设置开机动画退出属性，开机动画播放线程循环中会去check这个属性是否设置为1,然后请求退出循环
                mBootAnimationStopped = true;
            }

            if (!mForceDisplayEnabled && !checkBootAnimationCompleteLocked()) {
                ProtoLog.i(WM_DEBUG_BOOT, "performEnableScreen: Waiting for anim complete");
                return;
            }

            try {
            	//跨进程通知SF去处理让开机动画退出
                IBinder surfaceFlinger = ServiceManager.getService("SurfaceFlinger");
                if (surfaceFlinger != null) {
                    ProtoLog.i(WM_ERROR, "******* TELLING SURFACE FLINGER WE ARE BOOTED!");
                    Parcel data = Parcel.obtain();
                    data.writeInterfaceToken("android.ui.ISurfaceComposer");
                    surfaceFlinger.transact(IBinder.FIRST_CALL_TRANSACTION, // BOOT_FINISHED
                            data, null, 0);  //注意这个IBinder.FIRST_CALL_TRANSACTION，这是system server第一次和SF binder通信，SF当收到是这个FLAG时处理通知关闭开机动画
                    data.recycle();
                }
            } catch (RemoteException ex) {
                ProtoLog.e(WM_ERROR, "Boot completed: SurfaceFlinger is dead!");
            }

            EventLogTags.writeWmBootAnimationDone(SystemClock.uptimeMillis());
            Trace.asyncTraceEnd(TRACE_TAG_WINDOW_MANAGER, "Stop bootanim", 0);
            mDisplayEnabled = true;
            ProtoLog.i(WM_DEBUG_SCREEN_ON, "******************** ENABLING SCREEN!");

            // Enable input dispatch.
            mInputManagerCallback.setEventDispatchingLw(mEventDispatchingEnabled);  //开始使能input dispatch 可以开始分发事件
        }

        try {
            mActivityManager.bootAnimationComplete(); // 告知AMS开机动画已经完成
        } catch (RemoteException e) {
        }

        mPolicy.enableScreenAfterBoot();

        // Make sure the last requested orientation has been applied.
        updateRotationUnchecked(false, false);
    }
```

#### 2.2 SurfaceFlinger进程

WMS跨进程调用到`SurfaceFlinger::onTransact`中处理  
frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp

```cpp
status_t SurfaceFlinger::onTransact(uint32_t code, const Parcel& data, Parcel* reply,
                                    uint32_t flags) {
    if (const status_t error = CheckTransactCodeCredentials(code); error != OK) {
        return error;
    }

    status_t err = BnSurfaceComposer::onTransact(code, data, reply, flags);  //由SurfaceFlinger父类处理    ...
}
```

frameworks/native/libs/gui/include/gui/ISurfaceComposer.h

```cpp
class BnSurfaceComposer: public BnInterface<ISurfaceComposer> {
public:
    enum ISurfaceComposerTag {
        // Note: BOOT_FINISHED must remain this value, it is called from
        // Java by ActivityManagerService.
        BOOT_FINISHED = IBinder::FIRST_CALL_TRANSACTION,  //远程调用的FLAG, 实际上是BOOT_FINISHED
		...
    };

    virtual status_t onTransact(uint32_t code, const Parcel& data,
            Parcel* reply, uint32_t flags = 0);
};
```

frameworks/native/libs/gui/ISurfaceComposer.cpp

```cpp
status_t BnSurfaceComposer::onTransact(  //由上面可知，在父类中处理BOOT_FINISHED
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
		...
        case BOOT_FINISHED: {
            CHECK_INTERFACE(ISurfaceComposer, data, reply);
            bootFinished();  //父类中的纯虚函数需要子类实现，所以会调用SF子类的实现
            return NO_ERROR;
        }
```

frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp  
父类中的纯虚函数需要子类实现，所以会调用SF的实现

```cpp
void SurfaceFlinger::bootFinished() {
	...
    // stop boot animation
    // formerly we would just kill the process, but we now ask it to exit so it
    // can choose where to stop the animation.
    property_set("service.bootanim.exit", "1");  //设置service.bootanim.exit -> 1 告知开机动画进程退出

    const int LOGTAG_SF_STOP_BOOTANIM = 60110;
    LOG_EVENT_LONG(LOGTAG_SF_STOP_BOOTANIM,
                   ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));

    sp<IBinder> input(defaultServiceManager()->getService(String16("inputflinger")));
	...
}
```

到这里退出开机动画的时间点和动作就了解清楚了。开机动画基本已经分析结束。