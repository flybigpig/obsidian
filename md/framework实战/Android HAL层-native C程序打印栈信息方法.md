       在调试Android系统底层函数时，经常需要跟踪函数调用流程，特别在HAL层需要确定参数来源时。使用栈信息逆向跟踪可快速分析函数调用流程，结合使用addr2line工具、绘图工具可绘制函数关系图。本文记录在Android S上打印C/C++函数栈信息的方法，以作参考。

       Android官网及芯片厂商介绍HAL层的资料不多，有些作为内部文档不会外传。在Android系统层次中，kernel和HAL层与硬件强相关，在不同手机上这两层的差别较大，也最能体现产品特点和性能。

![](https://i-blog.csdnimg.cn/blog_migrate/0bec8d909e2a7998afc87b3e712aec33.jpeg)

（图片来源：[Android 架构  |  Android 开源项目  |  Android Open Source Project](https://source.android.google.cn/devices/architecture "Android 架构  |  Android 开源项目  |  Android Open Source Project")）

       HAL层函数运行于用户态，一般该调用链起始于HIDL service启动的某个线程中；而App层和Java API Framework层的函数调用链一般起始于Android Java Runtime维护的某个线程中（由Java虚拟机统一管理）。HIDL service为Linux自启动进程，当手机启动后常驻于后台挂起。Framework/App和HIDL service进程间使用binder机制通信。因此，在使用栈信息追踪了调用链后，仍要结合源码和Log分析数据传递过程，特别要注意进程同步机制，这是考验经验的过程。

       以相机Camera2消息处理函数为例，打印栈信息到Log中。

       1. Android源码提供了libutils库（源码注释有对库功能的说明，需要具体分析），该库含有CallStack类，提供了栈打印功能。在Android S源码中该库位于/system/core/libutils内，头文件在/system/core/include/utils软链接指向/system/core/libutils/include/utils，如下所示：

![](https://i-blog.csdnimg.cn/blog_migrate/08010c8eabb7dab11c56ddbb9d0d8a8e.jpeg)

CallStack类声明位于libutils/utils/CallStack.h内，在namespace android中。该类的构造方法（在CallStack.cpp中）内即打印栈信息到Log，构造方法实现如下：

```cpp
CallStack::CallStack() {
}

CallStack::CallStack(const char* logtag, int32_t ignoreDepth) {
    this->update(ignoreDepth+1);
    this->log(logtag);
}
```

CallStack构造方法第一个参数为Log tag，第二个参数为忽略打印的栈深度，默认值为1。

分析libutils编译文件Android.bp，发现该CallStack.cpp最后编译为libutilscallstack.so，依赖于libutils.so和libbacktrace.so文件。且该libutilscallstack库继承了libutils\_defaults的属性，查看libutils\_defaults已将VNDK开放于vendor，则libutilscallstack库也开放于vendor，可被vendor模块使用。

```
cc_library {
    name: "libutilscallstack",
    defaults: ["libutils_defaults"],

    srcs: [
        "CallStack.cpp",
    ],

    arch: {
        mips: {
            cflags: ["-DALIGN_DOUBLE"],
        },
    },

    shared_libs: [
         "libutils",
         "libbacktrace",
    ],
    ......
}
```

       2. 在camera2相机底层消息处理函数内添加创建android::CallStack对象，包含头文件#include <utils/CallStack.h>。

```cpp
// #define LOG_NDEBUG 0
#define LOG_TAG "CameraRequest"
#include <utils/Log.h>
#include <utils/String16.h>
#include <utils/CallStack.h>  //CallStack.h

#include <camera/camera2/CaptureRequest.h>

#include <binder/Parcel.h>
#include <gui/Surface.h>
#include <gui/view/Surface.h>

namespace android {
namespace hardware {
namespace camera2 {

// These must be in the .cpp (to avoid inlining)
...


status_t CaptureRequest::readFromParcel(const android::Parcel* parcel) {
    if (parcel == NULL) {
        ALOGE("%s: Null parcel", __FUNCTION__);
        return BAD_VALUE;
    }

    mSurfaceList.clear();
    mStreamIdxList.clear();
    mSurfaceIdxList.clear();
    mPhysicalCameraSettings.clear();
    
    android::CallStack dumpstack("EMUL",1); //新添加语句打印栈信息

    status_t err = OK;

    int32_t settingsCount;
```

在编译配置文件Android.bp内添加libutilscallstack.so依赖

```
cc_library_shared {
    name: "libcamera_client",
    aidl: {
        ...
        ],
    },
    srcs: [
        ...
    ],

    shared_libs: [
        "libcutils",
        "libutils",
        "liblog",
        "libbinder",
        "libgui",
        "libcamera_metadata",
        "libnativewindow",
	    "libutilscallstack",
    ],
...
}
```

重新编译，等待libcamera\_client生成完成。

       3. 运行emulator测试栈打印结果。开启emulator模拟器，打开LogCat抓取日志；开启Camera2 App，点击拍照按钮，Log中即可得到栈打印。

![](https://i-blog.csdnimg.cn/blog_migrate/062cbcfd59525a0391bdb1e73093cec9.png)

Log中搜索EMUL，得到的栈信息如下，#00代表栈顶，从栈底到栈顶依次为调用链上的每个函数。所在文件和位置也一目了然。

![](https://i-blog.csdnimg.cn/blog_migrate/b2233adc871d461805402fae5c56d9cc.png)

     4. 有时栈信息只显示指令地址如00046d67和所在文件libcamera\_client.so，不显示函数名。

     借助addr2line工具可获得函数名称。首先根据编译平台（android lunch所选平台为x86\_64\_generic）选取对应的addr2line工具（该工具位置根据find指令查找prebuilts文件夹获得），注意addr2line选择解析的so文件必须是包含符号表的文件，指令为：

![](https://i-blog.csdnimg.cn/blog_migrate/cd167005ba02dc3966f03b92a2156ad1.jpeg)

解析结果如下所示：

![](https://i-blog.csdnimg.cn/blog_migrate/8ae9224cd795970245a48d1f26fc0951.jpeg)

可以发现其显示了代码内容和源文件。通过使用addr2line依次获得每个pc 地址对应的源代码，即可完整解析栈信息。

经验证，上述方法在HAL层函数中同样可获取栈信息内容。

* * *

这里特别强调：链接提示CallStack::CallStack()和CallStack::~CallStack()找不到的错误不是权限问题。思考如下工程配置：

![](https://i-blog.csdnimg.cn/blog_migrate/9c0a02b8e8852cb8f6b77c7c3d10bb27.jpeg)

为了方便，在debuglog.h中#include<CallStack.h>，通过某个宏定义CallStack的调用。然后该debuglog.h被广泛引用于其他vendor模块，这些vendor模块可能是动态库或静态库，每个库都有其对应的编译配置。

在某些调试中，仅在A.cpp工程的Android.bp内添加了libutilscallstack.so依赖，其他cpp对应的工程内未添加libutilscallstack.so依赖。那么编译时，当B.cpp使用了相关宏或CallStack后（可能为inline展开或静态引用而间接使用），则报 ” CallStack找不到 “ 链接错误，此问题在大型工程编译时尤其严重。

若在debuglog.h内添加CallStack相关宏，应当避免CallStack被以static或inline函数的方式导入到所有工程cpp内。还应避免CallStack相关函数以export方式导出到so外部供其他库使用，当soong编译未发现libutilscallstack.so依赖时，可能删除该库的符号表，导致栈回溯Log中无法显示函数名，而出现(deleted)相关提示。

当出现CallStack::CallStack()和CallStack::~CallStack()找不到的错误时，应当重点查看debuglog.h的书写方式，而不是反复审查VNDK权限或libutilscallstack.so的引用问题，至少在Android S上不应怀疑。

* * *

        在APP和Java Framework层，使用Log.getStackTraceString函数可获得当前位置的线程调用栈信息，该函数返回栈信息字符串，使用Log.d可打印到日志中。在想要获取的位置插入getStackTraceString以获得当前线程的调用栈。Log.d("TAG",Log.getStackTraceString(new Throwable()));

```java
public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        Log.d("EMUL",Log.getStackTraceString(new Throwable())); //打印栈信息
        FloatingActionButton fab = findViewById(R.id.fab);
        fab.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
               ...
            }
        });
    }
...
}
```

         插入栈打印后，编译程序并执行，在Logcat中可见栈信息：

![](https://i-blog.csdnimg.cn/blog_migrate/00b59267d04563db5f2e12bda60b94ab.png)

         该方法同样适用于借助android studio和真机调试Java Framework层（加载android源码ipr工程到android studio内）。调试前务必保持java Framework与手机的SDK版本一致。此外，android studio中Java Framework可加入断点用以判断某一函数是否被执行，断点命中完整显示了栈层次。

使用android studio抓取的栈打印信息，从而获取了Activity启动流程。

![](https://i-blog.csdnimg.cn/blog_migrate/a188e4fdacd557a6a5a62fa27df1eff1.jpeg)

         每层调用都显示了函数名称和源文件位置。

* * *

注意：编译android源码时，lunch选择的平台与运行平台一致；若运行于emulator，则最好与主机平台一致，以防止emulator运行缓慢。若emulator需运行于x86\_64平台，则lunch选择x86\_64\_generic，这样可加快emulator的运行速度；若lunch选择arm64，则emulator仍可在x86\_64主机上运行，但速度过慢。