Android bootanimation

[](https://github.com/i-rtfsc/note/edit/main/docs/Android/bootanimation%E7%A8%8B%E5%BA%8F%E7%AE%80%E4%BB%8B.md "编辑此页")

bootanimation是在kernel启动完毕后，由init程序根据rc文件call起来的程序，其主要功能是在启动Android服务时，屏幕显示一个开机动画，以此来改善用户体验的一个程序。本文分析该程序主要流程，以及个功能模块的代码实现。

## 代码结构[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#_1 "Permanent link")

![](https://i-rtfsc.github.io/Android/res/android_boot_animation_all.png)

（图片来源于网络，从Android12.0的代码没有看到支持video的部分，请忽略）

如上图所示，程序的主要功能类都已经在上图中描述。应用代码位于[frameworks/base/cmds/bootanimation](http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/)，各代码文件如下表所示：

## 创建BootAnimation对象[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#bootanimation "Permanent link")

BootAnimation的程序入口为bootanimation\_main.cpp中的main方法，该方法中会创建BootAnimation对象。

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/bootanimation_main.cpp

int main()
{
    ...
        bool noBootAnimation = bootAnimationDisabled();
    if (!noBootAnimation) {

        ...

            // create the boot animation object (may take up to 200ms for 2MB zip)
            sp&lt;BootAnimation&gt; boot = new BootAnimation(audioplay::createAnimationCallbacks());

        waitForSurfaceFlinger();

        ...
        }
    return 0;
}</code>
```

main方法中创建BootAnimation对象后会将其赋值给一个sp\[BootAnimation\]对象，sp类重载了=运算符，赋值时会调用右值对象的incStrong()方法，以此标记当前对象正在使用。赋值的最终目的是将BootAnimation对象对象指针保存到sp对象的m\_ptr成员中。

```
<span></span><code id="code-lang-css">sp[BootAnimation] 应该写成 sp&lt;BootAnimation&gt; ，目前因为 obsidian 对markdown 支持还有点问题。</code>
```

```
<span></span><code id="code-lang-php">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

BootAnimation::BootAnimation(sp&lt;Callbacks&gt; callbacks)
        : Thread(false), mLooper(new Looper(false)), mClockEnabled(true), mTimeIsAccurate(false),
        mTimeFormat12Hour(false), mTimeCheckThread(nullptr), mCallbacks(callbacks) {
    mSession = new SurfaceComposerClient();

    std::string powerCtl = android::base::GetProperty("sys.powerctl", "");
    if (powerCtl.empty()) {
        mShuttingDown = false;
    } else {
        mShuttingDown = true;
    }
    ALOGD("%sAnimationStartTiming start time: %" PRId64 "ms", mShuttingDown ? "Shutdown" : "Boot",
            elapsedRealtime());
}</code>
```

BootAnimation的构造函数中：

-   mSession是BootAnimation类的一个成员变量，它的类型为SurfaceComposerClient，是用来和SurfaceFlinger执行Binder进程间通信。

SurfaceComposerClient类内部有一个实现了ISurfaceComposerClient接口的Binder代理对象mClient，这个Binder代理对象引用了SurfaceFlinger服务，SurfaceComposerClient类就是通过它来和SurfaceFlinger服务通信的。 由于BootAnimation类引用了SurfaceFlinger服务，因此，当SurfaceFlinger服务意外死亡时，BootAnimation类就需要得到通知，并执行binderDied函数。

```
<span></span><code id="code-lang-scss">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp


void BootAnimation::binderDied(const wp&lt;IBinder&gt;&amp;) {
    // woah, surfaceflinger died!
    SLOGD("SurfaceFlinger died, exiting...");

    // calling requestExit() is not enough here because the Surface code
    // might be blocked on a condition variable that will never be updated.
    kill( getpid(), SIGKILL );
    requestExit();
}</code>
```

binderDied函数会杀死当前进程，并退出。

-   通过读取属性“sys.powerctl”来确定当前播放的是开机动画还是关机动画。

对于不熟悉cpp代码的同学看到这就觉得很奇怪，在bootanimation\_main.cpp的main还是里调用BootAnimation构造函数，也没看到BootAnimation的构造函数里继续做任何工作啊，那这个程序怎么跑呢？ 首先我们看下BootAnimation的头文件：

```
<span></span><code tabindex="0" id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.h

class BootAnimation : public Thread, public IBinder::DeathRecipient    //继承了Thread类和IBinder::DeathRecipient类
{
    ...

 private:
    virtual bool        threadLoop();//线程体，如果返回true，且requestExit()没有被调用，则该函数会再次执行；如果返回false，则threadloop中的内容仅仅执行一次，线程就会退出
    virtual status_t    readyToRun();//线程体执行前的初始化工作
    virtual void        onFirstRef();//属于其父类RefBase，该函数在强引用sp新增引用计数時调用，就是当有sp包装的类初始化的时候调用    
    virtual void        binderDied(const wp&lt;IBinder&gt;&amp; who);//当对象死掉或者其他情况导致该Binder结束时，就会回调binderDied()方法

    ...
}</code>
```

### onFirstRef()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#onfirstref "Permanent link")

BootAnimation类继承了Thread类和IBinder::DeathRecipient类，BootAnimation类间接地继承了RefBase类，并且重写了RefBase类的成员函数onFirstRef，当一个BootAnimation对象第一次被智能指针引用时，这个BootAnimation对象的成员函数onFirstRef就会被调用。

### readyToRun()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#readytorun "Permanent link")

BootAnimation类继承了Thread类，当BootAnimation类的成员函数onFirstRef调用了父类Thread的成员函数run之后，系统就会创建一个线程，这个线程在第一次运行之前，会调用BootAnimation类的成员函数readyToRun来执行一些Thread执行前的初始化工作。

### threadLoop()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#threadloop "Permanent link")

后面再调用BootAnimation类的成员函数threadLoop来显示开机画面,每个线程类都要实现的，在这里定义thread的执行内容。这个函数如果返回true，且没有调用requestExit()，则该函数会再次执行；如果返回false，则threadloop中的内容仅仅执行一次，线程就会退出。

### android()或者movie()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#androidmovie "Permanent link")

显示完成之后，就会销毁前面所创建的EGLContext对象mContext、EGLSurface对象mSurface，以及EGLDisplay对象mDisplay等。android()方法里主要是使用方法绘制安卓字样，这边的退出使用的是checkExit(）；一般使用自定义动画播放，因此主要看一下movie方法里的主要调用方法。

## 初始化动画文件[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#_2 "Permanent link")

前面讲到在sp的赋值过程中会调用RefBase的incStrong方法，该方法会在首次引用时调用当前类的onFirstRef方法，该方法被BootAnimation类重写了。 也就是说BootAnimation类间接继承了RefBase类，且上面的main函数创建BootAnimation对象的时候使用智能指针引用，所以执行BootAnimation类的构造函数创建对象时，也会执行onFirstRef函数，下面是onFirstRef函数：

### onFirstRef()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#onfirstref_1 "Permanent link")

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

void BootAnimation::onFirstRef() {
    //注册SurfaceFlinger服务的死亡接收通知
    status_t err = mSession-&gt;linkToComposerDeath(this);
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
}</code>
```

onFirstRef先注册SurfaceFlinger服务的死亡接收通知，接着调用preloadAnimation加载开机动画资源文件。

### preloadAnimation()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#preloadanimation "Permanent link")

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp


bool BootAnimation::preloadAnimation() {
    findBootAnimationFile();
    if (!mZipFileName.isEmpty()) {
        mAnimation = loadAnimation(mZipFileName);
        return (mAnimation != nullptr);
    }

    return false;
}</code>
```

调用findBootAnimationFile找到动画文件，也就是初始化 mZipFileName。

### findBootAnimationFile()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#findbootanimationfile "Permanent link")

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

void BootAnimation::findBootAnimationFile() {
    // If the device has encryption turned on or is in process
    // of being encrypted we show the encrypted boot animation.
    char decrypt[PROPERTY_VALUE_MAX];
    property_get("vold.decrypt", decrypt, "");

    bool encryptedAnimation = atoi(decrypt) != 0 ||
        !strcmp("trigger_restart_min_framework", decrypt);

    if (!mShuttingDown &amp;&amp; encryptedAnimation) {
        static const std::vector&lt;std::string&gt; encryptedBootFiles = {
            PRODUCT_ENCRYPTED_BOOTANIMATION_FILE, SYSTEM_ENCRYPTED_BOOTANIMATION_FILE,
        };
        if (findBootAnimationFileInternal(encryptedBootFiles)) {
            return;
        }
    }

    const bool playDarkAnim = android::base::GetIntProperty("ro.boot.theme", 0) == 1;
    //开机动画文件
    static const std::vector&lt;std::string&gt; bootFiles = {
        APEX_BOOTANIMATION_FILE, playDarkAnim ? PRODUCT_BOOTANIMATION_DARK_FILE : PRODUCT_BOOTANIMATION_FILE,
        OEM_BOOTANIMATION_FILE, SYSTEM_BOOTANIMATION_FILE
    };
    //关机动画文件
    static const std::vector&lt;std::string&gt; shutdownFiles = {
        PRODUCT_SHUTDOWNANIMATION_FILE, OEM_SHUTDOWNANIMATION_FILE, SYSTEM_SHUTDOWNANIMATION_FILE, ""
    };
    //重启动画文件
    static const std::vector&lt;std::string&gt; userspaceRebootFiles = {
        PRODUCT_USERSPACE_REBOOT_ANIMATION_FILE, OEM_USERSPACE_REBOOT_ANIMATION_FILE,
        SYSTEM_USERSPACE_REBOOT_ANIMATION_FILE,
    };

    //判断是重启、关机还是开机
    if (android::base::GetBoolProperty("sys.init.userspace_reboot.in_progress", false)) {
        findBootAnimationFileInternal(userspaceRebootFiles);
    } else if (mShuttingDown) {
        findBootAnimationFileInternal(shutdownFiles);
    } else {
        findBootAnimationFileInternal(bootFiles);
    }
}</code>
```

-   首先根据系统属性vold.decrypt来判断系统是否启动加密或者正在加密处理，如果是，则会根据优先级降级的方式，去播放如下两个路径的加密动画：
-   static const char PRODUCT\_ENCRYPTED\_BOOTANIMATION\_FILE\[\] = "/product/media/bootanimation-encrypted.zip"
-   static const char SYSTEM\_ENCRYPTED\_BOOTANIMATION\_FILE\[\] = "/system/media/bootanimation-encrypted.zip"
-   其次根据系统属性sys.init.userspace\_reboot.in\_progress来判断系统是否正在重启，如果是，则会根据优先级降级的方式，去播放如下三个路径的重启动画：
-   static constexpr const char\* PRODUCT\_USERSPACE\_REBOOT\_ANIMATION\_FILE = "/product/media/userspace-reboot.zip"
-   static constexpr const char\* OEM\_USERSPACE\_REBOOT\_ANIMATION\_FILE = "/oem/media/userspace-reboot.zip"
-   static constexpr const char\* SYSTEM\_USERSPACE\_REBOOT\_ANIMATION\_FILE = "/system/media/userspace-reboot.zip"
-   再次根据Bootanimation构造函数中获取到的系统属性sys.powerctl判断系统是否正在关机，如果是，则会根据优先级降级的方式，去播放如下三个路径的关机动画：
-   static const char PRODUCT\_SHUTDOWNANIMATION\_FILE\[\] = "/product/media/shutdownanimation.zip"
-   static const char OEM\_SHUTDOWNANIMATION\_FILE\[\] = "/oem/media/shutdownanimation.zip"
-   static const char SYSTEM\_SHUTDOWNANIMATION\_FILE\[\] = "/system/media/shutdownanimation.zip"
-   否则为开机，会根据优先级降级的方式，去播放如下五个路径的开机动画：
-   static const char APEX\_BOOTANIMATION\_FILE\[\] = "/apex/com.android.bootanimation/etc/bootanimation.zip" （apex升级的方式最优先）
-   static const char PRODUCT\_BOOTANIMATION\_DARK\_FILE\[\] = "/product/media/bootanimation-dark.zip" （根据系统属性[ro.boot.t](http://aospxref.com/android-12.0.0_r3/s?path=ro.boot.t&project=frameworks)heme判断是暗黑主题还是明亮主题）
-   static const char PRODUCT\_BOOTANIMATION\_FILE\[\] = "/product/media/bootanimation.zip"
-   static const char OEM\_BOOTANIMATION\_FILE\[\] = "/oem/media/bootanimation.zip"
-   static const char SYSTEM\_BOOTANIMATION\_FILE\[\] = "/system/media/bootanimation.zip"
    
    > 总结：优先级的顺序一般是 product > oem > system
    

### findBootAnimationFileInternal()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#findbootanimationfileinternal "Permanent link")

```
<span></span><code id="code-lang-php">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

bool BootAnimation::findBootAnimationFileInternal(const std::vector&lt;std::string&gt; &amp;files) {
    for (const std::string&amp; f : files) {
        if (access(f.c_str(), R_OK) == 0) {
            mZipFileName = f.c_str();
            return true;
        }
    }
    return false;
}</code>
```

可以看出来就是把数据的第一个元素赋值给mZipFileName。

## 解析动画文件[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#_3 "Permanent link")

在preloadAnimation()初始化mZipFileName后调用loadAnimation(mZipFileName)来解析动画文件。

### loadAnimation()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#loadanimation "Permanent link")

```
<span></span><code id="code-lang-rust">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

BootAnimation::Animation* BootAnimation::loadAnimation(const String8&amp; fn) {
    //如果已经加载，则返回
    if (mLoadedFiles.indexOf(fn) &gt;= 0) {
        SLOGE("File \"%s\" is already loaded. Cyclic ref is not allowed",
            fn.string());
        return nullptr;
    }

    //打开zip文件
    ZipFileRO *zip = ZipFileRO::open(fn);
    if (zip == nullptr) {
        SLOGE("Failed to open animation zip \"%s\": %s",
            fn.string(), strerror(errno));
        return nullptr;
    }

    Animation *animation =  new Animation;
    animation-&gt;fileName = fn;
    animation-&gt;zip = zip;
    animation-&gt;clockFont.map = nullptr;
    mLoadedFiles.add(animation-&gt;fileName);

    //解析描述文件"desc.txt"
    parseAnimationDesc(*animation);
    if (!preloadZip(*animation)) {
        releaseAnimation(animation);
        return nullptr;
    }

    mLoadedFiles.remove(fn);
    return animation;
}</code>
```

打开zip文件并调用parseAnimationDesc解析描述文件"desc.txt"。

### parseAnimationDesc()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#parseanimationdesc "Permanent link")

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

bool BootAnimation::parseAnimationDesc(Animation&amp; animation)  {
    String8 desString;

    if (!readFile(animation.zip, "desc.txt", desString)) {
        return false;
    }
    char const* s = desString.string();

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
        char path[ANIM_ENTRY_NAME_MAX];
        char color[7] = "000000"; // default to black if unspecified
        char clockPos1[TEXT_POS_LEN_MAX + 1] = "";
        char clockPos2[TEXT_POS_LEN_MAX + 1] = "";
        char pathType;

        int nextReadPos;

        int topLineNumbers = sscanf(l, "%d %d %d %d", &amp;width, &amp;height, &amp;fps, &amp;progress);
        if (topLineNumbers == 3 || topLineNumbers == 4) {
            // SLOGD("&gt; w=%d, h=%d, fps=%d, progress=%d", width, height, fps, progress);
            animation.width = width;
            animation.height = height;
            animation.fps = fps;
            if (topLineNumbers == 4) {
              animation.progressEnabled = (progress != 0);
            } else {
              animation.progressEnabled = false;
            }
        } else if (sscanf(l, "%c %d %d %" STRTO(ANIM_PATH_MAX) "s%n",
                          &amp;pathType, &amp;count, &amp;pause, path, &amp;nextReadPos) &gt;= 4) {
            if (pathType == 'f') {
                sscanf(l + nextReadPos, " %d #%6s %16s %16s", &amp;framesToFadeCount, color, clockPos1,
                       clockPos2);
            } else {
                sscanf(l + nextReadPos, " #%6s %16s %16s", color, clockPos1, clockPos2);
            }
            // SLOGD("&gt; type=%c, count=%d, pause=%d, path=%s, framesToFadeCount=%d, color=%s, "
            //       "clockPos1=%s, clockPos2=%s",
            //       pathType, count, pause, path, framesToFadeCount, color, clockPos1, clockPos2);
            Animation::Part part;
            part.playUntilComplete = pathType == 'c';
            part.framesToFadeCount = framesToFadeCount;
            part.count = count;
            part.pause = pause;
            part.path = path;
            part.audioData = nullptr;
            part.animation = nullptr;
            if (!parseColor(color, part.backgroundColor)) {
                SLOGE("&gt; invalid color '#%s'", color);
                part.backgroundColor[0] = 0.0f;
                part.backgroundColor[1] = 0.0f;
                part.backgroundColor[2] = 0.0f;
            }
            parsePosition(clockPos1, clockPos2, &amp;part.clockPosX, &amp;part.clockPosY);
            animation.parts.add(part);
        }
        else if (strcmp(l, "$SYSTEM") == 0) {
            // SLOGD("&gt; SYSTEM");
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
}</code>
```

乍一看这个代码很复杂，其实主要是通过sscanf函数按照顺序从上到下依次解析每一行。如果第一行包含3或者4个数字则是：用来描述开机动画在屏幕显示的大小及速度。否则为一个播放片段，播放片段分为标识符、循环的次数、阶段切换间隔时间、图片的目录、**动画淡出帧。**

下面来结合描述文件的具体内容一起分析。

### desc.txt描述文件[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#desctxt "Permanent link")

我们找到一个bootanimation.zip的解压：

```
<span></span><code id="code-lang-kotlin">╭─[solo@10.0.12.7] ➤ [/Users/solo/Downloads]
╰─(py39tf2.x) ❯❯❯❯❯❯ ll bootanimation
total 8
drwx------@   8 solo  staff   256B Jul  1 12:30 .
drwx------+  45 solo  staff   1.4K Jul  1 12:13 ..
-rw-------@   1 solo  staff    75B Jan 14  2021 desc.txt
drwxr-xr-x@   3 solo  staff    96B Jul  1 12:13 part0
drwxr-xr-x@  36 solo  staff   1.1K Jul  1 12:13 part1
drwxr-xr-x@  41 solo  staff   1.3K Jul  1 12:13 part2
drwxr-xr-x@ 187 solo  staff   5.8K Jul  1 12:13 part3
drwxr-xr-x@ 104 solo  staff   3.3K Jul  1 12:13 part4</code>
```

其"desc.txt"文件中的内容如下：

```
<span></span><code id="code-lang-r">512 416 60
c 1 0 part0
c 2 15 part1
c 1 0 part2
c 1 0 part3
f 0 0 part4 10</code>
```

结合上面的代码，我们把desc.txt拆分成两个部分：topLine和非topLine。

-   如果topLine包含3或者4个数字，则是用来描述开机动画在屏幕显示的大小及速度。

具体为：开机动画的宽度为512个像素，高度为416个像素，显示频率为每秒60帧，即每帧显示1/60秒。 Android12.0中加入了“显示百分比”，就是代码中的progressEnabled。

-   否则为一个播放片段

-   标识符：Android5.1之前只有p；Android5.1中加入了c；Android12.0中加入了f。
-   循环的次数：表示该片段显示的次数，如果为‘0’，表示会无限重复显示。
-   阶段切换间隔时间：一个片段或与下一个片段之间的时间间隔；**阶段切换间隔时间 \* （1/每秒显示的帧数）。**
-   相应图片的目录：bootanimation.zip解析出来后的文件目录，
-   \*\*动画淡出帧：\*\*Android12.0中加入了动画淡出帧，也就是代码中的framesToFadeCount，到第几帧后动画开始淡出。

举例：

-   c 1 0 part0：代表该片段显示1次，与下一个片段间隔0s，该片段的显示图片路径为bootanimation.zip/part0。
-   c 2 15 part1：代表该片段显示2次，与下一个片段间隔15\*(1/60)=(¼)s，与下一个片段间隔15\*(1/60)=(¼)s，该片段的显示图片路径为bootanimation.zip/part1。
-   f 0 0 part4 10：代表该片段无限循环显示，且两次显示的间隔为0s，该片段的显示图片路径为bootanimation.zip/part4，到第10帧动画开始淡出。

通过"desc.txt"文件结合代码（看一眼就能知道的）我们大概知道了其运作原理，但刚才我们是遗留几个问题没有阐述清楚：

-   标识符
-   \*\*显示百分比\*\*进度
-   **动画淡出帧**

#### 标识符 和 动画淡出帧[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#_4 "Permanent link")

如果标识符为c：

```
<span></span><code id="code-lang-perl">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

bool BootAnimation::parseAnimationDesc(Animation&amp; animation)  {
    ...

    } else if (sscanf(l, "%c %d %d %" STRTO(ANIM_PATH_MAX) "s%n",
                      &amp;pathType, &amp;count, &amp;pause, path, &amp;nextReadPos) &gt;= 4) {
        if (pathType == 'f') {
            sscanf(l + nextReadPos, " %d #%6s %16s %16s", &amp;framesToFadeCount, color, clockPos1,
                   clockPos2);
        } else {
            sscanf(l + nextReadPos, " #%6s %16s %16s", color, clockPos1, clockPos2);
        }
        // SLOGD("&gt; type=%c, count=%d, pause=%d, path=%s, framesToFadeCount=%d, color=%s, "
        //       "clockPos1=%s, clockPos2=%s",
        //       pathType, count, pause, path, framesToFadeCount, color, clockPos1, clockPos2);
        Animation::Part part;
        part.playUntilComplete = pathType == 'c';
        part.framesToFadeCount = framesToFadeCount;
        ...
    }
   ...
}</code>
```

可以看到，如果pathType等于'c'，part.playUntilComplete等于true，否则为false。接着，看一下显示代码：

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

bool BootAnimation::shouldStopPlayingPart(const Animation::Part&amp; part,
                                          const int fadedFramesCount,
                                          const int lastDisplayedProgress) {
    // stop playing only if it is time to exit and it's a partial part which has been faded out
    return exitPending() &amp;&amp; !part.playUntilComplete &amp;&amp; fadedFramesCount &gt;= part.framesToFadeCount &amp;&amp;
        (lastDisplayedProgress == 0 || lastDisplayedProgress == 100);
}



bool BootAnimation::playAnimation(const Animation&amp; animation) {
    ...
        for (size_t i=0 ; i&lt;pcount ; i++) {
            ...
                // process the part not only while the count allows but also if already fading
                for (int r=0 ; !part.count || r&lt;part.count || fadedFramesCount &gt; 0 ; r++) {
                    if (shouldStopPlayingPart(part, fadedFramesCount, lastDisplayedProgress)) break;

                    ...

                        for (size_t j=0 ; j&lt;fcount ; j++) {
                            if (shouldStopPlayingPart(part, fadedFramesCount, lastDisplayedProgress)) break;

                            ...
                            }

                    ...
                    }
        }

    ....
}</code>
```

可以看到，如果exitPending()返回值为true且part.playUntilComplete=false，则会break。即：当SurfaceFlinger服务要求bootanimation停止显示动画时，以‘p’标识的片段会停止，而以'c'标识的片段会继续显示。这就是两者之间的主要区别。 这里有个问题：重复循环显示的'c'标识片段，会不受任何约束的一直显示下去，这显然是不合适的。所以在Android12.0又新增了条件：

```
<span></span><code>fadedFramesCount &gt;= part.framesToFadeCount &amp;&amp; (lastDisplayedProgress == 0 || lastDisplayedProgress == 100</code>
```

当前播放的帧大于等于描述文件里设置的framesToFadeCount，就停止动画。

#### 显示百分比进度[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#_5 "Permanent link")

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

bool BootAnimation::parseAnimationDesc(Animation&amp; animation)  {
    ...
    int topLineNumbers = sscanf(l, "%d %d %d %d", &amp;width, &amp;height, &amp;fps, &amp;progress);
    if (topLineNumbers == 3 || topLineNumbers == 4) {
        ...
        if (topLineNumbers == 4) {
          animation.progressEnabled = (progress != 0);
        } else {
          animation.progressEnabled = false;
        }
    } 
    ...
}</code>
```

如果描述文件里第一行有第四个数字，并且不为0，则显示播放动画的时候显示进度。

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

bool BootAnimation::playAnimation(const Animation&amp; animation) {
    ...
    for (size_t i=0 ; i&lt;pcount ; i++) {
        ...

        // process the part not only while the count allows but also if already fading
        for (int r=0 ; !part.count || r&lt;part.count || fadedFramesCount &gt; 0 ; r++) {
            ...

            // For the last animation, if we have progress indicator from
            // the system, display it.
            int currentProgress = android::base::GetIntProperty(PROGRESS_PROP_NAME, 0);
            bool displayProgress = animation.progressEnabled &amp;&amp;
                (i == (pcount -1)) &amp;&amp; currentProgress != 0;

            for (size_t j=0 ; j&lt;fcount ; j++) {
                ...

                if (displayProgress) {
                    int newProgress = android::base::GetIntProperty(PROGRESS_PROP_NAME, 0);
                    // In case the new progress jumped suddenly, still show an
                    // increment of 1.
                    if (lastDisplayedProgress != 100) {
                      // Artificially sleep 1/10th a second to slow down the animation.
                      usleep(100000);
                      if (lastDisplayedProgress &lt; newProgress) {
                        lastDisplayedProgress++;
                      }
                    }
                    // Put the progress percentage right below the animation.
                    int posY = animation.height / 3;
                    int posX = TEXT_CENTER_VALUE;
                    drawProgress(lastDisplayedProgress, animation.progressFont, posX, posY);
                }

                ...
            }

            ...
        }
    }

    ...
}</code>
```

代码细节这里不展开叙述。

### preloadZip()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#preloadzip "Permanent link")

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

bool BootAnimation::preloadZip(Animation&amp; animation) {
    // read all the data structures
    const size_t pcount = animation.parts.size();
    void *cookie = nullptr;
    ZipFileRO* zip = animation.zip;
    if (!zip-&gt;startIteration(&amp;cookie)) {
        return false;
    }

    ZipEntryRO entry;
    char name[ANIM_ENTRY_NAME_MAX];
    while ((entry = zip-&gt;nextEntry(cookie)) != nullptr) {
        const int foundEntryName = zip-&gt;getEntryFileName(entry, name, ANIM_ENTRY_NAME_MAX);
        if (foundEntryName &gt; ANIM_ENTRY_NAME_MAX || foundEntryName == -1) {
            SLOGE("Error fetching entry file name");
            continue;
        }

        const String8 entryName(name);
        const String8 path(entryName.getPathDir());
        const String8 leaf(entryName.getPathLeaf());
        if (leaf.size() &gt; 0) {
            if (entryName == CLOCK_FONT_ZIP_NAME) {
                FileMap* map = zip-&gt;createEntryFileMap(entry);
                if (map) {
                    animation.clockFont.map = map;
                }
                continue;
            }

            if (entryName == PROGRESS_FONT_ZIP_NAME) {
                FileMap* map = zip-&gt;createEntryFileMap(entry);
                if (map) {
                    animation.progressFont.map = map;
                }
                continue;
            }

            for (size_t j = 0; j &lt; pcount; j++) {
                if (path == animation.parts[j].path) {
                    uint16_t method;
                    // supports only stored png files
                    if (zip-&gt;getEntryInfo(entry, &amp;method, nullptr, nullptr, nullptr, nullptr, nullptr)) {
                        if (method == ZipFileRO::kCompressStored) {
                            FileMap* map = zip-&gt;createEntryFileMap(entry);
                            if (map) {
                                Animation::Part&amp; part(animation.parts.editItemAt(j));
                                if (leaf == "audio.wav") {
                                    // a part may have at most one audio file
                                    part.audioData = (uint8_t *)map-&gt;getDataPtr();
                                    part.audioLength = map-&gt;getDataLength();
                                } else if (leaf == "trim.txt") {
                                    part.trimData.setTo((char const*)map-&gt;getDataPtr(),
                                                        map-&gt;getDataLength());
                                } else {
                                    Animation::Frame frame;
                                    frame.name = leaf;
                                    frame.map = map;
                                    frame.trimWidth = animation.width;
                                    frame.trimHeight = animation.height;
                                    frame.trimX = 0;
                                    frame.trimY = 0;
                                    part.frames.add(frame);
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

    // If there is trimData present, override the positioning defaults.
    for (Animation::Part&amp; part : animation.parts) {
        const char* trimDataStr = part.trimData.string();
        for (size_t frameIdx = 0; frameIdx &lt; part.frames.size(); frameIdx++) {
            const char* endl = strstr(trimDataStr, "\n");
            // No more trimData for this part.
            if (endl == nullptr) {
                break;
            }
            String8 line(trimDataStr, endl - trimDataStr);
            const char* lineStr = line.string();
            trimDataStr = ++endl;
            int width = 0, height = 0, x = 0, y = 0;
            if (sscanf(lineStr, "%dx%d+%d+%d", &amp;width, &amp;height, &amp;x, &amp;y) == 4) {
                Animation::Frame&amp; frame(part.frames.editItemAt(frameIdx));
                frame.trimWidth = width;
                frame.trimHeight = height;
                frame.trimX = x;
                frame.trimY = y;
            } else {
                SLOGE("Error parsing trim.txt, line: %s", lineStr);
                break;
            }
        }
    }

    zip-&gt;endIteration(cookie);

    return true;
}</code>
```

解析bootanimation.zip中的png或者音频文件audio.wav。

## 启动BootAnimation线程[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#bootanimation_1 "Permanent link")

### readyToRun()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#readytorun_1 "Permanent link")

在前面讲到BootAnimation对象创建的时候讲到过，线程的创建先readyToRun()来执行一些Thread执行前的初始化工作。

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

status_t BootAnimation::readyToRun() {
    mAssets.addDefaultAssets();

    mDisplayToken = SurfaceComposerClient::getInternalDisplayToken();
    if (mDisplayToken == nullptr)
        return NAME_NOT_FOUND;

    DisplayMode displayMode;
    const status_t error =
            SurfaceComposerClient::getActiveDisplayMode(mDisplayToken, &amp;displayMode);
    if (error != NO_ERROR)
        return error;

    mMaxWidth = android::base::GetIntProperty("ro.surface_flinger.max_graphics_width", 0);
    mMaxHeight = android::base::GetIntProperty("ro.surface_flinger.max_graphics_height", 0);
    ui::Size resolution = displayMode.resolution;
    resolution = limitSurfaceSize(resolution.width, resolution.height);
    // create the native surface
    sp&lt;SurfaceControl&gt; control = session()-&gt;createSurface(String8("BootAnimation"),
            resolution.getWidth(), resolution.getHeight(), PIXEL_FORMAT_RGB_565);

    SurfaceComposerClient::Transaction t;

    // this guest property specifies multi-display IDs to show the boot animation
    // multiple ids can be set with comma (,) as separator, for example:
    // setprop persist.boot.animation.displays 19260422155234049,19261083906282754
    Vector&lt;PhysicalDisplayId&gt; physicalDisplayIds;
    char displayValue[PROPERTY_VALUE_MAX] = "";
    property_get(DISPLAYS_PROP_NAME, displayValue, "");
    bool isValid = displayValue[0] != '\0';
    if (isValid) {
        char *p = displayValue;
        while (*p) {
            if (!isdigit(*p) &amp;&amp; *p != ',') {
                isValid = false;
                break;
            }
            p ++;
        }
        if (!isValid)
            SLOGE("Invalid syntax for the value of system prop: %s", DISPLAYS_PROP_NAME);
    }
    if (isValid) {
        std::istringstream stream(displayValue);
        for (PhysicalDisplayId id; stream &gt;&gt; id.value; ) {
            physicalDisplayIds.add(id);
            if (stream.peek() == ',')
                stream.ignore();
        }

        // In the case of multi-display, boot animation shows on the specified displays
        // in addition to the primary display
        auto ids = SurfaceComposerClient::getPhysicalDisplayIds();
        constexpr uint32_t LAYER_STACK = 0;
        for (auto id : physicalDisplayIds) {
            if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
                sp&lt;IBinder&gt; token = SurfaceComposerClient::getPhysicalDisplayToken(id);
                if (token != nullptr)
                    t.setDisplayLayerStack(token, LAYER_STACK);
            }
        }
        t.setLayerStack(control, LAYER_STACK);
    }

    t.setLayer(control, 0x40000000)
        .apply();

    sp&lt;Surface&gt; s = control-&gt;getSurface();

    // initialize opengl and egl
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);
    EGLConfig config = getEglConfig(display);
    EGLSurface surface = eglCreateWindowSurface(display, config, s.get(), nullptr);
    EGLContext context = eglCreateContext(display, config, nullptr, nullptr);
    EGLint w, h;
    eglQuerySurface(display, surface, EGL_WIDTH, &amp;w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &amp;h);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE)
        return NO_INIT;

    mDisplay = display;
    mContext = context;
    mSurface = surface;
    mWidth = w;
    mHeight = h;
    mFlingerSurfaceControl = control;
    mFlingerSurface = s;
    mTargetInset = -1;

    projectSceneToWindow();

    // Register a display event receiver
    mDisplayEventReceiver = std::make_unique&lt;DisplayEventReceiver&gt;();
    status_t status = mDisplayEventReceiver-&gt;initCheck();
    SLOGE_IF(status != NO_ERROR, "Initialization of DisplayEventReceiver failed with status: %d",
            status);
    mLooper-&gt;addFd(mDisplayEventReceiver-&gt;getFd(), 0, Looper::EVENT_INPUT,
            new DisplayEventCallback(this), nullptr);

    return NO_ERROR;
}</code>
```

-   获取到SurfaceComposer
-   创建Surface
-   初始化opengl 和 egl
-   监听display事件

### threadLoop()[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#threadloop_1 "Permanent link")

这部分可以说是真正的动画入口，threadLoop函数中会调用到具体的动画实现方法。

```
<span></span><code id="code-lang-rust">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

bool BootAnimation::threadLoop() {
    bool result;
    // We have no bootanimation file, so we use the stock android logo
    // animation.
    if (mZipFileName.isEmpty()) {
        result = android();
    } else {
        result = movie();
    }

    mCallbacks-&gt;shutdown();
    eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(mDisplay, mContext);
    eglDestroySurface(mDisplay, mSurface);
    mFlingerSurface.clear();
    mFlingerSurfaceControl.clear();
    eglTerminate(mDisplay);
    eglReleaseThread();
    IPCThreadState::self()-&gt;stopProcess();
    return result;
}</code>
```

如果线程起来后，mZipFileName还没初始化完成，则播放“原生动画”，否则就播放自定义的动画。 播放完成后，需要处理的资源释放与清理工作。

## 播放动画[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#_6 "Permanent link")

### 原生动画[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#_7 "Permanent link")

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

bool BootAnimation::android() {
    SLOGD("%sAnimationShownTiming start time: %" PRId64 "ms", mShuttingDown ? "Shutdown" : "Boot",
            elapsedRealtime());
    initTexture(&amp;mAndroid[0], mAssets, "images/android-logo-mask.png");
    initTexture(&amp;mAndroid[1], mAssets, "images/android-logo-shine.png");

    mCallbacks-&gt;init({});

    // clear screen
    glShadeModel(GL_FLAT);
    glDisable(GL_DITHER);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(mDisplay, mSurface);

    glEnable(GL_TEXTURE_2D);
    glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    // Blend state
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    const nsecs_t startTime = systemTime();
    do {
        processDisplayEvents();
        const GLint xc = (mWidth  - mAndroid[0].w) / 2;
        const GLint yc = (mHeight - mAndroid[0].h) / 2;
        const Rect updateRect(xc, yc, xc + mAndroid[0].w, yc + mAndroid[0].h);
        glScissor(updateRect.left, mHeight - updateRect.bottom, updateRect.width(),
                updateRect.height());

        nsecs_t now = systemTime();
        double time = now - startTime;
        float t = 4.0f * float(time / us2ns(16667)) / mAndroid[1].w;
        GLint offset = (1 - (t - floorf(t))) * mAndroid[1].w;
        GLint x = xc - offset;

        glDisable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);

        glEnable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glBindTexture(GL_TEXTURE_2D, mAndroid[1].name);
        glDrawTexiOES(x,                 yc, 0, mAndroid[1].w, mAndroid[1].h);
        glDrawTexiOES(x + mAndroid[1].w, yc, 0, mAndroid[1].w, mAndroid[1].h);

        glEnable(GL_BLEND);
        glBindTexture(GL_TEXTURE_2D, mAndroid[0].name);
        glDrawTexiOES(xc, yc, 0, mAndroid[0].w, mAndroid[0].h);

        EGLBoolean res = eglSwapBuffers(mDisplay, mSurface);
        if (res == EGL_FALSE)
            break;

        // 12fps: don't animate too fast to preserve CPU
        const nsecs_t sleepTime = 83333 - ns2us(systemTime() - now);
        if (sleepTime &gt; 0)
            usleep(sleepTime);

        checkExit();
    } while (!exitPending());

    glDeleteTextures(1, &amp;mAndroid[0].name);
    glDeleteTextures(1, &amp;mAndroid[1].name);
    return false;
}

status_t BootAnimation::initTexture(Texture* texture, AssetManager&amp; assets,
        const char* name) {
    Asset* asset = assets.open(name, Asset::ACCESS_BUFFER);
    if (asset == nullptr)
        return NO_INIT;

    AndroidBitmapInfo bitmapInfo;
    void* pixels = decodeImage(asset-&gt;getBuffer(false), asset-&gt;getLength(), &amp;bitmapInfo);
    auto pixelDeleter = std::unique_ptr&lt;void, decltype(free)*&gt;{ pixels, free };

    asset-&gt;close();
    delete asset;

    if (!pixels) {
        return NO_INIT;
    }

    const int w = bitmapInfo.width;
    const int h = bitmapInfo.height;

    GLint crop[4] = { 0, h, w, -h };
    texture-&gt;w = w;
    texture-&gt;h = h;

    glGenTextures(1, &amp;texture-&gt;name);
    glBindTexture(GL_TEXTURE_2D, texture-&gt;name);

    switch (bitmapInfo.format) {
        case ANDROID_BITMAP_FORMAT_A_8:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA,
                    GL_UNSIGNED_BYTE, pixels);
            break;
        case ANDROID_BITMAP_FORMAT_RGBA_4444:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                    GL_UNSIGNED_SHORT_4_4_4_4, pixels);
            break;
        case ANDROID_BITMAP_FORMAT_RGBA_8888:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, pixels);
            break;
        case ANDROID_BITMAP_FORMAT_RGB_565:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB,
                    GL_UNSIGNED_SHORT_5_6_5, pixels);
            break;
        default:
            break;
    }

    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, crop);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return NO_ERROR;
}</code>
```

原生的开机动画位于[frameworks/base/core/res/assets/images](http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/core/res/assets/images/)目录下，由android-logo-shine.png和android-logo-mask.png两张png图片组成。熟悉framework-res的同学都知道这两张图片其实是编译到了framework-res.apk中。initTexture()函数会根据这两张图片来分别创建两个纹理对象，并存储在Bootanimation类的成员变量数组mAndroid中。通过混合渲染这两个纹理对象，我们就可以得到一个开机动画，这是通过中间的while循环语句来实现的。

图片android-logo-mask.png用作动画前景，它是一个镂空的“ANDROID”图像。图片android-logo-shine.png用作动画背景，它的中间包含有一个高亮的呈45度角的条纹。在每一次循环中，图片android-logo-shine.png被划分成左右两部分内容来显示。左右两个部分的图像宽度随着时间的推移而此消彼长，这样就可以使得图片android-logo-shine.png中间高亮的条纹好像在移动一样。另一方面，在每一次循环中，图片android-logo-shine.png都作为一个整体来渲染，而且它的位置是恒定不变的。由于它是一个镂空的“ANDROID”图像，因此，我们就可以通过它的镂空来看到它背后的图片android-logo-shine.png的条纹一闪一闪地划过。

### 自定义动画[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#_8 "Permanent link")

```
<span></span><code id="code-lang-rust">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

bool BootAnimation::movie() {
    if (mAnimation == nullptr) {
        mAnimation = loadAnimation(mZipFileName);
    }

    if (mAnimation == nullptr)
        return false;

    // mCallbacks-&gt;init() may get called recursively,
    // this loop is needed to get the same results
    for (const Animation::Part&amp; part : mAnimation-&gt;parts) {
        if (part.animation != nullptr) {
            mCallbacks-&gt;init(part.animation-&gt;parts);
        }
    }
    mCallbacks-&gt;init(mAnimation-&gt;parts);

    bool anyPartHasClock = false;
    for (size_t i=0; i &lt; mAnimation-&gt;parts.size(); i++) {
        if(validClock(mAnimation-&gt;parts[i])) {
            anyPartHasClock = true;
            break;
        }
    }
    if (!anyPartHasClock) {
        mClockEnabled = false;
    }

    // Check if npot textures are supported
    mUseNpotTextures = false;
    String8 gl_extensions;
    const char* exts = reinterpret_cast&lt;const char*&gt;(glGetString(GL_EXTENSIONS));
    if (!exts) {
        glGetError();
    } else {
        gl_extensions.setTo(exts);
        if ((gl_extensions.find("GL_ARB_texture_non_power_of_two") != -1) ||
            (gl_extensions.find("GL_OES_texture_npot") != -1)) {
            mUseNpotTextures = true;
        }
    }

    // Blend required to draw time on top of animation frames.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(GL_FLAT);
    glDisable(GL_DITHER);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_TEXTURE_2D);
    glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    bool clockFontInitialized = false;
    if (mClockEnabled) {
        clockFontInitialized =
            (initFont(&amp;mAnimation-&gt;clockFont, CLOCK_FONT_ASSET) == NO_ERROR);
        mClockEnabled = clockFontInitialized;
    }

    initFont(&amp;mAnimation-&gt;progressFont, PROGRESS_FONT_ASSET);

    if (mClockEnabled &amp;&amp; !updateIsTimeAccurate()) {
        mTimeCheckThread = new TimeCheckThread(this);
        mTimeCheckThread-&gt;run("BootAnimation::TimeCheckThread", PRIORITY_NORMAL);
    }

    playAnimation(*mAnimation);

    if (mTimeCheckThread != nullptr) {
        mTimeCheckThread-&gt;requestExit();
        mTimeCheckThread = nullptr;
    }

    if (clockFontInitialized) {
        glDeleteTextures(1, &amp;mAnimation-&gt;clockFont.texture.name);
    }

    releaseAnimation(mAnimation);
    mAnimation = nullptr;

    return false;
}</code>
```

-   如果在onFirstRef()的流程里还没有加载好动画，则需要调用loadAnimation加载动画。（参考之前的流程）
-   调用playAnimation()播放动画。
-   动画播放结束后，释放资源。

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

bool BootAnimation::playAnimation(const Animation&amp; animation) {
    const size_t pcount = animation.parts.size();
    nsecs_t frameDuration = s2ns(1) / animation.fps;

    SLOGD("%sAnimationShownTiming start time: %" PRId64 "ms", mShuttingDown ? "Shutdown" : "Boot",
            elapsedRealtime());

    int fadedFramesCount = 0;
    int lastDisplayedProgress = 0;
    for (size_t i=0 ; i&lt;pcount ; i++) {
        const Animation::Part&amp; part(animation.parts[i]);
        const size_t fcount = part.frames.size();
        glBindTexture(GL_TEXTURE_2D, 0);

        // Handle animation package
        if (part.animation != nullptr) {
            playAnimation(*part.animation);
            if (exitPending())
                break;
            continue; //to next part
        }

        // process the part not only while the count allows but also if already fading
        for (int r=0 ; !part.count || r&lt;part.count || fadedFramesCount &gt; 0 ; r++) {
            if (shouldStopPlayingPart(part, fadedFramesCount, lastDisplayedProgress)) break;

            mCallbacks-&gt;playPart(i, part, r);

            glClearColor(
                    part.backgroundColor[0],
                    part.backgroundColor[1],
                    part.backgroundColor[2],
                    1.0f);

            // For the last animation, if we have progress indicator from
            // the system, display it.
            int currentProgress = android::base::GetIntProperty(PROGRESS_PROP_NAME, 0);
            bool displayProgress = animation.progressEnabled &amp;&amp;
                (i == (pcount -1)) &amp;&amp; currentProgress != 0;

            for (size_t j=0 ; j&lt;fcount ; j++) {
                if (shouldStopPlayingPart(part, fadedFramesCount, lastDisplayedProgress)) break;

                processDisplayEvents();

                const int animationX = (mWidth - animation.width) / 2;
                const int animationY = (mHeight - animation.height) / 2;

                const Animation::Frame&amp; frame(part.frames[j]);
                nsecs_t lastFrame = systemTime();

                if (r &gt; 0) {
                    glBindTexture(GL_TEXTURE_2D, frame.tid);
                } else {
                    if (part.count != 1) {
                        glGenTextures(1, &amp;frame.tid);
                        glBindTexture(GL_TEXTURE_2D, frame.tid);
                        glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    }
                    int w, h;
                    initTexture(frame.map, &amp;w, &amp;h);
                }

                const int xc = animationX + frame.trimX;
                const int yc = animationY + frame.trimY;
                Region clearReg(Rect(mWidth, mHeight));
                clearReg.subtractSelf(Rect(xc, yc, xc+frame.trimWidth, yc+frame.trimHeight));
                if (!clearReg.isEmpty()) {
                    Region::const_iterator head(clearReg.begin());
                    Region::const_iterator tail(clearReg.end());
                    glEnable(GL_SCISSOR_TEST);
                    while (head != tail) {
                        const Rect&amp; r2(*head++);
                        glScissor(r2.left, mHeight - r2.bottom, r2.width(), r2.height());
                        glClear(GL_COLOR_BUFFER_BIT);
                    }
                    glDisable(GL_SCISSOR_TEST);
                }
                // specify the y center as ceiling((mHeight - frame.trimHeight) / 2)
                // which is equivalent to mHeight - (yc + frame.trimHeight)
                const int frameDrawY = mHeight - (yc + frame.trimHeight);
                glDrawTexiOES(xc, frameDrawY, 0, frame.trimWidth, frame.trimHeight);

                // if the part hasn't been stopped yet then continue fading if necessary
                if (exitPending() &amp;&amp; part.hasFadingPhase()) {
                    fadeFrame(xc, frameDrawY, frame.trimWidth, frame.trimHeight, part,
                              ++fadedFramesCount);
                    if (fadedFramesCount &gt;= part.framesToFadeCount) {
                        fadedFramesCount = MAX_FADED_FRAMES_COUNT; // no more fading
                    }
                }

                if (mClockEnabled &amp;&amp; mTimeIsAccurate &amp;&amp; validClock(part)) {
                    drawClock(animation.clockFont, part.clockPosX, part.clockPosY);
                }

                if (displayProgress) {
                    int newProgress = android::base::GetIntProperty(PROGRESS_PROP_NAME, 0);
                    // In case the new progress jumped suddenly, still show an
                    // increment of 1.
                    if (lastDisplayedProgress != 100) {
                      // Artificially sleep 1/10th a second to slow down the animation.
                      usleep(100000);
                      if (lastDisplayedProgress &lt; newProgress) {
                        lastDisplayedProgress++;
                      }
                    }
                    // Put the progress percentage right below the animation.
                    int posY = animation.height / 3;
                    int posX = TEXT_CENTER_VALUE;
                    drawProgress(lastDisplayedProgress, animation.progressFont, posX, posY);
                }

                handleViewport(frameDuration);

                eglSwapBuffers(mDisplay, mSurface);

                nsecs_t now = systemTime();
                nsecs_t delay = frameDuration - (now - lastFrame);
                //SLOGD("%lld, %lld", ns2ms(now - lastFrame), ns2ms(delay));
                lastFrame = now;

                if (delay &gt; 0) {
                    struct timespec spec;
                    spec.tv_sec  = (now + delay) / 1000000000;
                    spec.tv_nsec = (now + delay) % 1000000000;
                    int err;
                    do {
                        err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &amp;spec, nullptr);
                    } while (err&lt;0 &amp;&amp; errno == EINTR);
                }

                checkExit();
            }

            usleep(part.pause * ns2us(frameDuration));

            if (exitPending() &amp;&amp; !part.count &amp;&amp; mCurrentInset &gt;= mTargetInset &amp;&amp;
                !part.hasFadingPhase()) {
                if (lastDisplayedProgress != 0 &amp;&amp; lastDisplayedProgress != 100) {
                    android::base::SetProperty(PROGRESS_PROP_NAME, "100");
                    continue;
                }
                break; // exit the infinite non-fading part when it has been played at least once
            }
        }
    }

    // Free textures created for looping parts now that the animation is done.
    for (const Animation::Part&amp; part : animation.parts) {
        if (part.count != 1) {
            const size_t fcount = part.frames.size();
            for (size_t j = 0; j &lt; fcount; j++) {
                const Animation::Frame&amp; frame(part.frames[j]);
                glDeleteTextures(1, &amp;frame.tid);
            }
        }
    }

    return true;
}</code>
```

-   const size\_t pcount = animation.parts.size(); 也就是desc.txt里播放片段的数量；所以for (size\_t i=0 ; i<pcount ; i++) 循环是有多少个播放片段就循环多少次。
-   在preloadZip()函数里会把每一个播放片段包含的帧数存在Part.frames，如：part.frames.add(frame)；所以for (size\_t j=0 ; j<fcount ; j++) 循环是当前播放有多少张图片就循环多少次。
-   调用shouldStopPlayingPart()判断是否要退出动画。（前面聊标识符和动画淡出帧的时候有介绍）
-   调用initTexture()显示
-   drawClock()显示时间
-   drawProgress()是否显示进度（Android12.0新增功能）
-   checkExit()检查是否退出动画

## 退出动画[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#_9 "Permanent link")

```
<span></span><code id="code-lang-cpp">http://aospxref.com/android-12.0.0_r3/xref/frameworks/base/cmds/bootanimation/BootAnimation.cpp

static const char EXIT_PROP_NAME[] = "service.bootanim.exit";

void BootAnimation::checkExit() {
    // Allow surface flinger to gracefully request shutdown
    char value[PROPERTY_VALUE_MAX];
    property_get(EXIT_PROP_NAME, value, "0");
    int exitnow = atoi(value);
    if (exitnow) {
        requestExit();
    }
}</code>
```

检测service.bootanim.exit的值，当属性值为1的时候，开机动画会requestExit()，从而结束开机动画。这个属性值的更改主要涉及以下内容：

![](https://i-rtfsc.github.io/Android/res/android_boot_animation_prop_exit.png)

## 总结[¶](https://i-rtfsc.github.io/android/bootanimation/bootanimation-intro/#_10 "Permanent link")

BootAnimation.cpp方法主要作用如下:

-   onFirstRef() : 建立BootAnimation进程与surfaceFlinger进程的通信，及加载资源
-   preloadAnimation() : 加载开机动画资源文件
-   findBootAnimationFile() : 主要是初始化 mZipFileName
-   findBootAnimationFileInternal() : 将mZipFileName存入索引
-   loadAnimation() : 解析资源，加载动画文件，这里的mZipFileName就是在readyToRun中获取的动画文件位置
-   parseAnimationDesc() : 解析读取desc.txt文件，设置相应animation参数
-   parseColor() : 解析颜色
-   parseTextCoord() : 解析文本坐标
-   parsePosition() : 解析位置
-   readFile() : 读取文件
-   preloadZip() : 用于图像的预加载阶段
-   decodeImage() : 解析图像信息，并存储
-   readyToRun() : 判断开机动画的压缩包是否存在，主要是对opengl工作环境进行初始化，初始化EGL环境，为送图做准备工作，做一个动画播放的预操作
-   threadLoop() : 显示开机画面,每个线程类都要实现的，在这里定义thread的执行内容。这个函数如果返回true，且没有调用requestExit()，则该函数会再次执行；如果返回false，则threadloop中的内容仅仅执行一次，线程就会退出。
-   doThreadLoop() : 超时检测线程的执行函数
-   playAnimation() : 会拿到 mAnimation的图片，还有desc.txt中定义的图片分辨率，帧率等信息，依次播放part0,part1中图片，合成Surface，然后调用eglSwapBuffers(mDisplay, mSurface);动图给显示设备．
-   movie() : 自定义的开机动画
-   getEglConfig() : 绘制目标framebuffer的配置属性及显示窗口内容
-   limitSurfaceSize() : 该方法的作用是将width和height限制在设备GPU支持的范围内
-   resizeSurface() : 调整开机动画的surface大小
-   releaseAnimation() : 释放动画资源
-   initFont() : 加载字体资源
-   initTexture() : 加载系统默认UI资源，通过decodeImage来解码图片，并显示在SurfaceLayer之上
-   drawClock() : 绘制时钟进行当前时间的显示
-   updateIsTimeAccurate() : 记录最新修改的时间
-   addTimeDirWatch() : 增加时间监测
-   handleEvent() : handle事件，更新UI操作信息
-   handleViewport() : 负责图表视图中可见的内容
-   processDisplayEvents() : 处理显示事件
-   checkExit() : 检测开机动画是否停止
-   binderDied() : 当Binder机制的客户端死掉，导致了该Binder结束，会回调此方法(此处一般指surfaceflinger）
-   DisplayEventCallback : 进行事件的处理以及调用surfaceComposerClient里的getPhysicalDisplayToken()进行物理屏幕的显示
-   TimeCheckThread : 超时检测机制线程

整个BootAnimation程序的流程图下图：

![](https://i-rtfsc.github.io/Android/res/android_boot_animation_sequence_diagram.png)