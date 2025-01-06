**1.应用权限机制**

**2.Android版本权限变更**

**3.应用权限变更适配实例**

**4.默认授权**

**5.AppOps机制**

**6.Selinux权限介绍**

## 1.应用权限机制

**权限机制？**

Android 是一个权限分离的系统。这是利用 Linux 已有的权限管理机制，通过为每一个 Application 分配不同的 uid 和 gid，从而使得不同的 Application 之间的私有数据和访问(native 以及 Java 层通过这种机制，都可以)达到隔离的目的。与此同时，Android 还在此基础上进行扩展，提供了permission机制，它主要是用来对 Application 可以执行的某些具体操作进行权限细分和访问控制，同时提供了URI permission机制，用来提供对某些特定的数据块进行专门权限进行限制。

Android Permission权限机制是对Android安全机制的一个重要补充，控制了应用对于系统接口或者对外接口的访问。

**权限信息:**

![](https://upload-images.jianshu.io/upload_images/26874665-723f17a25c940216.png?imageMogr2/auto-orient/strip|imageView2/2/w/363/format/webp)

权限信息

可以使用adb shell pm list permissions -f 命令详细查看 Android 预定义的权限详细信息(危险权限组未在此列表中), 例如下所示：

![](https://upload-images.jianshu.io/upload_images/26874665-c30c86089ce3d7ce.png?imageMogr2/auto-orient/strip|imageView2/2/w/500/format/webp)

系统目录下查看声明的权限信息路径：

```csharp
frameworks/base/core/res/AndroidManifest.xml
```

**常用权限命令:**

pm list permission-groups

打印所有已知的权限组

pm list permissions [options] [GROUP]

打印权限

例如：pm list permissions –g -d (危险权限组信息)

![](https://upload-images.jianshu.io/upload_images/26874665-599cae2b00debb92.png?imageMogr2/auto-orient/strip|imageView2/2/w/379/format/webp)

危险权限信息

**其它常用参数：**

![](https://upload-images.jianshu.io/upload_images/26874665-4229973b4556c147.png?imageMogr2/auto-orient/strip|imageView2/2/w/370/format/webp)

其它命令

**权限级别:**

描述权限中隐含的潜在风险，并指示系统在确定是否将权限授予请求它的应用程序时应遵循的程序。Standard permissions具有预定义的永久保护级别。如果您在应用程序中创建自定义权限，您可以使用下面列出的值之一定义 protectionLevel 属性。如果没有为自定义权限定义 protectionLevel，则系统分配默认值（“normal”）。

每个保护级别由基本权限类型和零个或多个标志组成(5个基本类型, 22个附加类型)。

**使用以下函数来提取它们：**

```cpp
int basePermissionType = permissionInfo.getProtection(); --- 基本权限类型

int permissionFlags = permissionInfo.getProtectionFlags(); --- 附加权限标志
```

**基本权限类型：**  

![](https://upload-images.jianshu.io/upload_images/26874665-1dd3120b4bf493dd.png?imageMogr2/auto-orient/strip|imageView2/2/w/924/format/webp)

基本权限类型

  
**其它附加权限标志,见如下google官网说明:**

**[https://developer.android.com/reference/android/R.attr?hl=zh-cn#protectionLevel](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.android.com%2Freference%2Fandroid%2FR.attr%3Fhl%3Dzh-cn%23protectionLevel)**

**权限管理：**

应用安装时，就给权限赋予了对应的状态，系统是使用PMS来管理权限。PMS维护管理权限状态数据，Android6.0之前，保存在data/system/packages.xml中；6.0之后增加了data/*../runtime-permissions.xml,数据持久化存储文件，用于记录着运行时权限的授予和拒绝状态，当权限状态变更时，申请的结果会动态更新 granted 值和flags状态。

![](https://upload-images.jianshu.io/upload_images/26874665-7de26c8b610255ba.png?imageMogr2/auto-orient/strip|imageView2/2/w/554/format/webp)

权限状态

**权限查看：**

Android12的runtime-permissions.xml被默认设置二进制xml格式，直接打开是乱码。

通过修改配置将其改为普通的xml格式: adb shell setprop persist.sys.binary_xml false

重启手机，再获取runtime-permissions.xml，就是普通xml格式。

##### 权限授予

**动态授权：**

Android6.0 此版本引入了一种新的权限模式，用户可直接在运行时管理应用权限。这种模式让用户能够更好地了解和控制权限，同时为应用开发者精简了安装和自动更新过程。用户可为所安装的各个应用分别授予或撤销权限。

**权限申请:**

![](https://upload-images.jianshu.io/upload_images/26874665-3976b44b9214aa22.png?imageMogr2/auto-orient/strip|imageView2/2/w/554/format/webp)

运行时权限授权流程

**权限检查:**

检查用户是否已向您的应用授予特定权限，将该权限传入checkSelfPermission() 方法。根据您的应用是否具有相应权限，此方法会返回PackageManager.PERMISSION_GRANTED(0)

或PackageManager.PERMISSION_DENIED(-1)。

![](https://upload-images.jianshu.io/upload_images/26874665-1e5ee211d73a36d3.png?imageMogr2/auto-orient/strip|imageView2/2/w/947/format/webp)

检查权限方法

检查应用自身权限，建议使用checkSelfPermission；

检查IPC的调用进程权限，建议使用checkCallingPermission(或者checkCallingOrSelfPermission);

指定PID、UID检查权限，建议使用checkPermission；

访问其它应用数据库，建议先检查访问的URI是否有权限；

**底层进行权限检查：**

我们了解了运行时权限在系统API接口调用的时候会去检查权限状态，但有些权限例如INTERNET、READ_LOGS、BLUETOOTH等调用框架层接口的时候并没有去检查权限，这类权限是如何检查授权的呢？

在底层，Linux内核使用用户和用户组来实施权限控制，这套权限模型是从Linux继承过来的，用于对文件系统实体进行访问控制，也可以对其他Android特定资源进行控制。这一模型通常被称为Android沙箱。以DalvikVM和Android框架形式存在的Android运行时实施了第二套权限模型。这套模型在用户安装应用时是向用户公开的，定义了应用拥有的权限，从而限制Android应用的能力。事实上，第二套权限模型中的某些权限直接映射到底层操作系统上的特定用户、用户组和权能。

内核和系统守护进程通过进程分配的 GID、UID和补充 GID来决定是否要赋予进程权限。

Android的权限模型是多方面的，有API权限、文件系统权限和IPC权限。在很多情况下，这些权限都会交织在一起。一些高级权限会后退映射到低级别的操作系统权能，这可能包括打开套接字、蓝牙设备和文件系统路径等。

**应用在AndroidMainfest.xml申明的权限如何映射到底层的UID、GID？**

内置权限到GID的映射定义在/etc/permissions/platform.xml 中

![](https://upload-images.jianshu.io/upload_images/26874665-d7a822e639d73de4.png?imageMogr2/auto-orient/strip|imageView2/2/w/554/format/webp)

包管理器在启动时读取 platform.xml，并维护「permission-GID」对应的列表。当它给安装中的包授权时，会把权限对应的 GID 加入到该应用进程的补充 GID 中。

权限对应的 GID 加入到该应用进程的补充 GID 中，可以通过adb shell--cd proc--cd PID--cat status查看：

![](https://upload-images.jianshu.io/upload_images/26874665-7f95e2e49beab8b8.png?imageMogr2/auto-orient/strip|imageView2/2/w/554/format/webp)

system/core/libcutils/include/private/android_filesystem_config.h

![](https://upload-images.jianshu.io/upload_images/26874665-91e21f7a83dff369.png?imageMogr2/auto-orient/strip|imageView2/2/w/554/format/webp)

```cpp
static const struct android_id_info android_ids[] = {

{ "camera", AID_CAMERA, },

{ "sdcard_r", AID_SDCARD_R, },

{ "sdcard_rw", AID_SDCARD_RW, },

{ "inet", AID_INET},
 ···
};
```

**注**:AndroidN及之前android_ids放在android_filesystem_config.h中之后版本迁移到pwd/grp functionality.

Android定义了从名称到独特标识符Android ID（AID）的映射表。初始的AID映射表包含了一些与特权用户及系统关键用户（如system用户/用户组）对应的静态保留条目。Android还保留了一段AID范围，用于提供原生应用的UID。Android 4.1之后的版本为多用户资料档案和隔离进程用户增加了额外的AID范围段（如Chrome沙箱）。

除了AID，Android还使用了辅助用户组机制，以允许进程访问共享或受保护的资源。例如，sdcard_rw用户组中的成员允许进程读写/sdcard目录，因为它的加载项规定了哪些用户组可以读写该目录。这与许多Linux发行版中对辅助用户组机制的使用是类似的。

注意　尽管所有的AID条目都映射到一个UID和GID，但是UID在描述系统上的一个用户时并不是必需的。例如，AID_SDCARD_RW映射到sdcard_rw，但是它仅仅用作一个辅助用户组，而不是系统上的UID。

在应用执行时，它们的UID、GID和辅助用户组都会被分配给新创建的进程。在一个独特UID和GID环境下运行，使得操作系统可以在内核中实施底层的限制措施，也让运行环境能够控制应用之间的交互。这就是Android沙箱的关键所在。

在应用包条目中定义的权限后面会通过两种方式实施检查：一种检查在调用给定方法时进行，由运行环境实施；另一种检查在操作系统底层进行，由库或内核实施。

**权限对应的 GID 何时被加入到应用进程的补充 GID 中？**

每个应用都会运行在自己的 Dalvik 虚拟机进程中，但是为了提高启动效率，Android 不会为每个应用都新建一个 Dalvik 进程，而是采用 fork 的形式。每个进程都 fork form zygote 进程。

因为 zygote 进程已经预加载了大部分核心和 Java 应用框架库，fork 的子进程会继承 zygote 的进程空间，也就是说，fork 的子进程，可以共享这些预加载的副本（记住，是副本，不是直接共享。fork 时，会 copy-on-write 预加载的内容），减少了重新加载核心库的时间。

当 zygote 收到启动新进程的请求时，它会 fork 自身出一个子进程，并对该子进程做特殊化处理。其源代码位于 framework/base/core/jni/com_android_internal_os_Zygote.cpp 中。SpecializeCommon() 的主要代码如下：

这里设置进程的组 ID 和用户 ID，通过 fork 创建的子进程调用 setgroups Intarray 设置该进程所属的组，这样应用程序就拥有了该组的权限，并且可以通过 setgid() 及 setuid() 确定应用程序的 GID 及 UID 值。

每个应用进程都分配好自己的 GID、UID和补充 GID，系统内核和守护进程就可以用这些标识来决定，是否要赋予进程权限。

**应用权限内容：**

![](https://upload-images.jianshu.io/upload_images/26874665-34aef56fc8889c40.png?imageMogr2/auto-orient/strip|imageView2/2/w/597/format/webp)

应用权限

## 2.Android版本权限变更

Android从1.0开始其根基Linux继承了已经深入人心的类Unix进程隔离机制与最小权限原则。引入了Android沙箱及扩展了Android使用的权限模型，包括Android对Unix系统UID/GID映射关系的特殊实现AID，以及在整个系统中实施的限制和权能。

**目前到Android13.0各版本中有关权限变更的重要调整如下：**

| 版本    | 重要变更                                                                                                                                                                                                                                                                                                                                                                   |
| ----- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1.6   | 新增[WRITE_EXTERNAL_STORAGE](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.android.com%2Freference%2Fandroid%2FManifest.permission%23WRITE_EXTERNAL_STORAGE)权限，允许程序写入外部存储                                                                                                                                                                                      |
| 2.2   | android.permission.KILL_BACKGROUND_PROCESSES— 允许应用程序调用[killBackgroundProcesses(String)](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.android.com%2Freference%2Fandroid%2Fapp%2FActivityManager%23killBackgroundProcesses%28java.lang.String%29)                                                                                                             |
| 2.3   | ![](https://upload-images.jianshu.io/upload_images/26874665-9685387392f6a564.png?imageMogr2/auto-orient/strip\|imageView2/2/w/481/format/webp)<br><br>image.png                                                                                                                                                                                                        |
| 3.0   | ![](https://upload-images.jianshu.io/upload_images/26874665-0febf43d10d3796f.png?imageMogr2/auto-orient/strip\|imageView2/2/w/485/format/webp)<br><br>image.png                                                                                                                                                                                                        |
| 4.0   | ![](https://upload-images.jianshu.io/upload_images/26874665-747de366e16e486f.png?imageMogr2/auto-orient/strip\|imageView2/2/w/469/format/webp)<br><br>image.png                                                                                                                                                                                                        |
| 4.0.3 | ![](https://upload-images.jianshu.io/upload_images/26874665-520e7a561683a19a.png?imageMogr2/auto-orient/strip\|imageView2/2/w/437/format/webp)<br><br>image.png                                                                                                                                                                                                        |
| 4.1   | ![](https://upload-images.jianshu.io/upload_images/26874665-9e6a93562916504f.png?imageMogr2/auto-orient/strip\|imageView2/2/w/448/format/webp)<br><br>image.png                                                                                                                                                                                                        |
| 4.2   | 精细化区分[ACCESS_COARSE_LOCATION](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.android.com%2Freference%2Fandroid%2FManifest.permission%23ACCESS_COARSE_LOCATION)权限和[ACCESS_FINE_LOCATION](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.android.com%2Freference%2Fandroid%2FManifest.permission%23ACCESS_FINE_LOCATION),不请求FINE_LOCATION获取的位置结果可能不太准确 |
| 4.3   | BIND_NOTIFICATION_LISTENER_SERVICE：需要使用新的 NotificationListenerService API                                                                                                                                                                                                                                                                                              |
| 4.4   | 从 Android 4.4 开始，当您仅访问您的应用特定外部存储区域时，此平台不再要求您的应用获取 WRITE_EXTERNAL_STORAGE 或 READ_EXTERNAL_STORAGE。不过，如果您要访问 getExternalStoragePublicDirectory() 提供的外部存储空间的可共享区域，则需要这些权限                                                                                                                                                                                                 |
| 5.0   | ![](https://upload-images.jianshu.io/upload_images/26874665-2c8a0481102b299e.png?imageMogr2/auto-orient/strip\|imageView2/2/w/447/format/webp)<br><br>image.png                                                                                                                                                                                                        |
| 6.0   | 此版本引入了一种新的权限模式，如今，用户可直接在运行时管理应用权限。这种模式让用户能够更好地了解和控制权限，同时为应用开发者精简了安装和自动更新过程。用户可为所安装的各个应用分别授予或撤销权限。对于以 Android 6.0（API 级别 23）或更高版本为目标平台的应用，请务必在运行时检查和请求权限。要确定您的应用是否已被授予权限，请调用新增的 checkSelfPermission() 方法。要请求权限，请调用新增的 requestPermissions() 方法。即使您的应用并不以 Android 6.0（API 级别 23）为目标平台，您也应该在新权限模式下测试您的应用。                                                                  |
| 7.0   | 文件系统权限更改：为了提高隐私文件的安全性，Android 7.0 或更高版本的应用程序的隐私目录已限制访问。                                                                                                                                                                                                                                                                                                                |
| 8.0   | 在 Android 8.0 之前，如果应用在运行时请求权限并且被授予该权限，系统会错误地将属于同一权限组并且在清单中注册的其他权限也一起授予应用。对于针对 Android 8.0 的应用，此行为已被纠正。系统只会授予应用明确请求的权限。然而，一旦用户为应用授予某个权限，则所有后续对该权限组中权限的请求都将被自动批准。                                                                                                                                                                                                        |
| 9.0   | 为了增强用户隐私，Android 9 引入了若干行为变更，如限制后台应用访问设备传感器、限制通过 Wi-Fi 扫描检索到的信息，以及与通话、手机状态和 Wi-Fi 扫描相关的新权限规则和权限组。无论采用哪一种目标 SDK 版本，这些变更都会影响运行于 Android 9 上的所有应用。                                                                                                                                                                                                                        |
| 10    | ![](https://upload-images.jianshu.io/upload_images/26874665-33ad343d6bcd05e3.png?imageMogr2/auto-orient/strip\|imageView2/2/w/436/format/webp)<br><br>image.png                                                                                                                                                                                                        |
| 11    | Android 11 对应用授予权限的方式进行了多项更改。这些更改旨在通过更加有意地授予权限来保护用户。强制执行分区存储、单次授权、自动重置权限、后台位置信息访问权限、软件包可见性、前台服务等的调整。                                                                                                                                                                                                                                                                   |
| 12    | 在搭载 Android 12 或更高版本的设备上，[用户可以要求](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.android.com%2Ftraining%2Flocation%2Fpermissions%23approximate-request)您的应用只能访问[大致位置](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.android.com%2Ftraining%2Flocation%2Fpermissions%23accuracy)信息。                                                                  |
| 13    | 存储权限变更、新增通知权限、后台使用身体传感器权限。                                                                                                                                                                                                                                                                                                                                             |

**总结**：从Android版本迭代有关权限变更来看，Android6.0之前基本是针对权限进行扩展调整，Android6.0之后加强了用户对系统的安全管控，Android10.0之后权限管控更加严格，进一步提升用户安全性。

**Google各版本变更信息：[https://developer.android.com/about/versions](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.android.com%2Fabout%2Fversions)**

## 3.应用权限变更适配实例

**实例一：后台定位权限适配**

为了让用户更好地控制应用对位置信息的访问权限，Android 10 引入了 ACCESS_BACKGROUND_LOCATION 权限。与 ACCESS_FINE_LOCATION 和 ACCESS_COARSE_LOCATION 权限不同，ACCESS_BACKGROUND_LOCATION 权限仅会影响应用在后台运行时对位置信息的访问权限。除非符合以下条件之一，否则应用将被视为在后台访问位置信息：

.属于该应用的 Activity 可见。

.该应用运行的某个前台设备已声明前台服务类型为 location。

.要声明您的应用中的某个服务的前台服务类型，请将应用的 targetSdkVersion 或 compileSdkVersion 设置为 29 或更高版本。

![](https://upload-images.jianshu.io/upload_images/26874665-d4ff747db584c030.png?imageMogr2/auto-orient/strip|imageView2/2/w/554/format/webp)

定位权限状态

**AndroidQ(10) 获取后台定位权限：**

1.Android 10 之前只有ACCESS_FINE_LOCATION和ACCESS_COARSE_LOCATION；

2.Android 10 新增加了后台定位权限：ACCESS_BACKGROUND_LOCATION，该权限对应始终允许；老的权限： ACCESS_FINE_LOCATION和ACCESS_COARSE_LOCATION代表仅前台使用允许；

3.应用的targetSdkVersion<Q，谷歌提供了兼容性方案，只要应用申请了老的位置权限ACCESS_FINE_LOCATION或者ACCESS_COARSE_LOCATION，会默认请求ACCESS_BACKGROUND_LOCATION权限，动态弹框根据用户选择授权。

4.应用的targetSdkVersion>=Q，如果应用必须要始终定位，可以只申请ACCESS_BACKGROUND_LOCATION即可；如果应用只需要申请前台定位，则只需要申请老的定位权限即可。

5.如果用户选择仅前台使用允许，应用的页面退后台，通过启动前台服务让应用处于前台状态，必须把前台服务标为：foregroundServiceType=“location”，才能获取位置信息。

```xml
<manifest> ...

<service  android:name="com.amap.api.location.APSService"

android:foregroundServiceType="location" />

 </manifest>
```

![](https://upload-images.jianshu.io/upload_images/26874665-882f86c03b3a315c.png?imageMogr2/auto-orient/strip|imageView2/2/w/554/format/webp)

定位权限授权对比

**Android R(11) 获取后台位置权限：**

当应用申请后台位置信息访问权限时，让用户授予权限的弹窗中，将不再提供“始终允许”的选项，这个选项只存在于设置中的应用权限授予页面，并且后台位置权限的申请需要应用已经拥有前台位置权限。

适配建议：

若您的应用确定需要获得后台位置信息访问的权限，现在则需要分为两个步骤，因为已无法在没有前台位置信息访问权限的时候直接申请后台位置信息访问权限。

1. 先申请前台位置信息访问权限；
    
2. 再申请后台位置信息访问权限，引导用户到设置中进行授予。
    

**Android S(12) 获取后台位置权限：**

在搭载 Android 12 或更高版本的设备上，用户可以要求您的应用只能访问大致位置信息。

确切位置：可访问确切位置信息(10英尺到160英尺)。

大致位置：只能访问大致位置信息(1 英里)。

**注意**：如果用户从权限对话框或在系统设置中将应用的位置信息使用权从确切位置降级到大致位置，系统会重启应用的进程。

**实例二：存储权限适配**

为了让用户更好地控制自己的文件并减少混乱，Android 10 针对应用推出了一种新的存储范例，称为分区存储。分区存储改变了应用在设备的外部存储设备中存储和访问文件的方式。

![](https://upload-images.jianshu.io/upload_images/26874665-c510bc29f819ecb8.png?imageMogr2/auto-orient/strip|imageView2/2/w/554/format/webp)

受影响的文件接口

**AndroidQ(10) 获取外部存储:**

1.应用targetSdkVersion<Q(29) 按照请求应用权限中所述的最佳做法，请求 READ\WRITE_EXTERNAL_STORAGE 权限。

或者使用 MediaStore API 修改或删除媒体文件。

2.如果您的应用以 Android 10（API 级别 29）为目标平台targetSdkVersion=Q(29)，请停用分区存储,继续使用适用于 Android 9 及更低版本的方法来执行此操作。

如果您以 Android 10 为目标平台，则需要在应用的清单文件中将 requestLegacyExternalStorage 的值设置为 true：

```xml
<manifest ... >

 <!-- This attribute is "false" by default on apps targeting

 Android 10\. -->

 <application android:requestLegacyExternalStorage="true" ... >
 ...
 </application>

</manifest>
```

**AndroidR(11) 及更高版本获取外部存储:**

1.使用 MediaStore.createWriteRequest() 或 MediaStore.createTrashRequest() 为应用的写入或删除请求创建待定 intent，然后通过调用该 intent 提示用户授予修改一组文件的权限。

**注意：**在 Android 11 或更高版本（无论目标 SDK 级别是什么）中，其他应用无法访问外部存储设备上的应用专用目录中存储的文件。

Android 存储用例和最佳做法 :

[https://developer.android.com/training/data-storage/use-cases](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.android.com%2Ftraining%2Fdata-storage%2Fuse-cases)

**实例三：获取应用列表**

Android 11 更改了应用查询用户已在设备上安装的其他应用以及与之交互的方式。使用 <queries> 元素，应用可以定义一组自身可访问的其他软件包。通过告知系统应向您的应用显示哪些其他软件包，此元素有助于鼓励最小权限原则。

受影响的接口:
```
.PackageManager.getInstalledApplications

 .PackageManager.getInstalledPackages
```
只返回系统可见以及与应用交互授权的应用列表。

以 Android 11（API 级别 30）或更高版本为目标平台，部分类型的应用也始终对您的应用可见：详见以下文档

[https://developer.android.com/training/basics/intents/package-visibility?hl=zh-cn#automatic](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.android.com%2Ftraining%2Fbasics%2Fintents%2Fpackage-visibility%3Fhl%3Dzh-cn%23automatic)

查询所有应用及与之交互：

在极少数情况下，您的应用可能需要查询设备上的所有已安装应用或与之交互，不管这些应用包含哪些组件。为了允许您的应用看到其他所有已安装应用，系统会提供 QUERY_ALL_PACKAGES 权限。

.如果应用的target< 30，是兼容之前的逻辑；

.如果应用的target>=30，必须要申请android.permission.QUERY_ALL_PACKAGES这个权限, 才能查询所有应用。

**注意：**如果您的应用以 Android 10（API 级别 29）或更低版本为目标平台，那么全部应用均会自动对您的应用可见。

**应用安装卸载广播，仍然受软件包可见性影响：**

如果监听安装/卸载的应用不符合可见性原则，或者未声明QUERY_ALL_PACKAGES权限, onReceive将不会回调。

![](https://upload-images.jianshu.io/upload_images/26874665-11c5e2f673f96e2d.png?imageMogr2/auto-orient/strip|imageView2/2/w/554/format/webp)

广播回调接口受影响

## 4.默认授权

**手机第一次开机或者fota升级后第一次开机进行的默认授权:**

系统预置的默认授权机制，在Pms systemReady里面会根据Build.FINGERPRINT来判断是不是第一次开机，来做默认授权。

![](https://upload-images.jianshu.io/upload_images/26874665-572d8112713a2984.png?imageMogr2/auto-orient/strip|imageView2/2/w/554/format/webp)

1. share systemuid默认授权权限(一般是在Manifest中配置类似的  
    android:sharedUserId=”android.uid.system”)
2. 系统特权应用并且是persistent进行默认授权危险权限；
3. 系统默认应用进行局部权限授权（例如：开机向导、相机、下载的Provider等）

原生机制第一次开机默认授权类：

```csharp
./frameworks/base/services/core/java/com/android/server/pm/DefaultPermissionGrantPolicy.java

public  void grantDefaultPermissions(int userId)  {

grantPermissionsToSysComponentsAndPrivApps(userId); ---授权系统&特权应用

grantDefaultSystemHandlerPermissions(userId); --- 授权默认应用

}
```

![](https://upload-images.jianshu.io/upload_images/26874665-9f104a23a47da76f.png?imageMogr2/auto-orient/strip|imageView2/2/w/554/format/webp)

系统默认授权应用

**新应用第一次安装授权:**

应用安装会走Pms，应用安装的过程中Pms会根据应用声明权限的level（Normal、Dangerous、Signature），来决定是否允许应用使用该权限。

Normal：只要应用申请，就允许应用使用该权限；

Signature：判断该应用签名是否和平台签名一样，如果一样就允许否则不允许使用该权限；

Dangerous：不给应用授予该权限，需要应用在用的时候动态申请该权限。

## 5.AppOps机制

AppOps全称是Application Operations，类似我们平时常说的应用程序的操作（权限）管理。Google从4.3开始推出Appops, 在Settings里面未开放Appops的入口，但这套方案却一直在后台默默的运行着。

**涉及的类:**

```css
android.app.AppOpsManager

com.android.server.AppOpsService

配置文件：appops.xml
```

appops.xml位于/data/system/目录下，存储各个app的权限设置和操作信息。

Android提供了命令行的方式来更改某个应用的某个权限，进入adb shell可以查看**相应用法：**

![](https://upload-images.jianshu.io/upload_images/26874665-55f8e50429baa7de.png?imageMogr2/auto-orient/strip|imageView2/2/w/509/format/webp)

appops命令

**AppOps权限检查流程:**

![](https://upload-images.jianshu.io/upload_images/26874665-30fd0dc2421c6f7a.png?imageMogr2/auto-orient/strip|imageView2/2/w/554/format/webp)

appops检查权限流程

## 6. Selinux权限介绍

**为什么增加Selinux？**

进程理论上所拥有的权限与执行它的用户的权限相同。比如，以root用户启动FileManager，那么FileManager就有root用户的权限，在Linux系统上能干任何事情。也就是像4.4以前的版本，只要我们对结点给予足够的权限，就可以随意的进行任意的操作。

Linux DAC有明显的不足，其中一个重要点就是，Root权限几乎可以做任意事情，一旦入侵者拿到root权限，即已经完全掌控了系统。另外每一个进程默认都拿到对应这个用户的所有权限，可以改动/删除这个用户的所有文件资源，明显这个难以防止恶意软件。所以在DAC之外设计了一个新的安全模型，叫MAC（Mandatory Access control），强制性访问控制，即系统针对每一项访问都进行严格的限制，具体的限制策略由开发者给出。MAC：即任何进程想在SELinux系统中干任何事情，都必须先在安全策略配置文件中赋予权限。凡是没有出现在安全策略配置文件中的权限，进程就没有该权限。

**打开和关闭selinux功能:**

出现了selinux相关的权限拒绝，则在kernel log 或者android log中都有对应的”avc: denied”，当然也可能和selinux的模式有关系，我们需要首先要确认当时SELinux 的模式, 是enforcing mode 还是 permissve mode。

如果问题容易复现，我们可以先将SELinux 模式调整到Permissive mode，然后再测试确认是否与SELinux 约束相关。

**可以通过以下命令设置:**

```undefined
adb shell setenforce 0

setenforce 0 设置SELinux 成为permissive模式 临时关闭selinux

setenforce 1 临时打开selinux
```

**如何配置selinux权限：**

例如报如下错误：

```bash
TcmReceiver: type=1400 audit(0.0:4214): avc: denied { write } for name="tcmd" dev="tmpfs" ino=1301 scontext=u:r:remote_prov_app:s0:c162,c256,c512,c768 tcontext=u:object_r:vendor_dpmtcm_socket:s0 tclass=sock_file permissive=0 app=com.android.remoteprovisioner
```

**具体参数表示：**

```undefined
权限: avc: denied { write } 表示缺少write权限

哪个te文件缺少权限: scontext=u:r:remote_prov_app:s0:c162,c256,c512,c768 ---- remote_prov_app缺少权限

哪个文件缺少权限: tcontext=u:object_r:vendor_dpmtcm_socket:s0 ---- remote_prov_app缺少对vendor_dpmtcm_socket的权限配置

文件类型:  tclass=sock_file 权限文件类型

表达含义：remote_prov_app文件需要新增类型为sock_file的vendor_dpmtcm_socket的write权限。
```

**如何修改：**配置te文件,添加 tclass=sock_file，编译selinux_policy，把system/ect下面的selinux文件夹，push到手机syste/ect进行覆盖。然后重启验证修改是否生效。

**注：**不要修改neverallow，会导致CTS测试失败。如果修改了file_contexts,替换selinux是不生效的。因为⽬标文件的context已经⽣成了，替换selinux并不会重新给⽬标文件设置context。这时可以通过chcon命令⼿动修改context。

##### 参考文章：

1. 「Android 安全架构」你真的了解权限机制吗？[https://www.jianshu.com/p/a17c8bed79d9](https://www.jianshu.com/p/a17c8bed79d9)
2. 「 Android 沙箱机制」 [https://blog.csdn.net/fei20121106/article/details/84023953](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.csdn.net%2Ffei20121106%2Farticle%2Fdetails%2F84023953)
3. 「 Android 安全架构及权限控制机制剖析」[http://www.uml.org.cn/mobiledev/201303281.asp](https://links.jianshu.com/go?to=http%3A%2F%2Fwww.uml.org.cn%2Fmobiledev%2F201303281.asp)

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/5284fd388394  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。