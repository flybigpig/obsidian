> 前言：工欲善其事，必先利其器。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c7ef1333932a618263b2e3506ae352b6.png#pic_center)

### 启动优化大纲

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/830e93d63e809be6c2489a7318894ba6.png#pic_center)

### 一、概述

#### 1\. 启动优化的意义

  **启动是指用户从点击 icon 到看到页面首帧的整个过程**，启动优化的目标就是**减少**这一过程的**耗时**。启动性能是 APP 使用体验的门面，启动过程耗时较长很可能导致用户使用 APP 的兴趣骤减。提高启动速度是每一个 APP 在体验优化方向上必须要做的关键技术突破。

#### 2\. 启动优化的价值

用户如果想打开一个应用，就一定要经过**启动**这个步骤。APP启动时间的长短，不只是[用户体验](https://so.csdn.net/so/search?q=%E7%94%A8%E6%88%B7%E4%BD%93%E9%AA%8C&spm=1001.2101.3001.7020)的问题，对于淘宝、京东等大型APP来说，会直接影响用户的**留存和转化**等核心数据。对研发人员来说，启动速度是我们的**门面**，它清清楚楚可以被所有人看到，我们都希望自己应用的启动速度可以秒杀所有竞争对手。

**工欲善其事，必先利其器**。特别是在性能优化领域，大量复杂繁琐的疑难杂症问题，需要借助工具的能力，在解决这些问题上会有质的飞跃。正所谓磨刀不误砍柴工，个人认为，用好性能分析工具，性能优化的提升就成功了一大半。下面也会详细介绍**八种**定位耗时问题的方式。

### 二、启动流程的内部机制

从用户点击应用图标开始，整个启动过程经过哪几个**关键阶段**，启动过程又究竟会出现哪些问题，又会给用户带来哪些体验问题。

#### 1\. 进程创建流程分析

应用进程是如何被创建的？每个 App 在启动前必须先创建一个进程，该进程是**由 zygote 进程 fork 出来**，进程具有独立的资源空间，用于承载 App 上运行的各种 Activity/Service 等组件。大多数情况一个 App 就运行在一个进程中。

进程的创建主要为以下三个步骤：

1.  当点击 App 图标启动应用时或者在应用内启动一个带有 process 标签的 Activity 时，都会触发创建新进程的请求，这种请求会先通过 Binder 发送给 system\_server 进程，也即是发送给 `ActivityManagerService` 进行处理。
2.  system\_server 进程会调用 `Process.start()` 方法，会先收集 uid、gid 等参数，然后通过 Socket 方式发送给 Zygote 进程，请求创建新进程。
3.  Zygote 进程接收到创建新进程的请求后，调用 `ZygoteInit.main()` 方法进行 `runSelectLoop()` 循环体内，当有客户端连接时执行 `ZygoteConnection.runOnce()` 方法，再经过层层调用后 fork 出新的应用进程。  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/47ad411c56804ac6cec26bce53ca8d77.png#pic_center)

-   **system\_server 进程**：是用于管理整个 Java [framework](https://so.csdn.net/so/search?q=framework&spm=1001.2101.3001.7020) 层，包含 ActivityManager，PowerManager 等各种系统服务;
    
-   **Zygote 进程**：是 Android 系统的首个 Java 进程，Zygote 是所有 Java 进程的父进程，包括 system\_server 进程以及所有的 App 进程都是 Zygote 的子进程。
    

#### 2\. 应用启动流程分析

应用的启动流程主要分为三步：**启动主线程，创建 Application，创建 MainActivity**。

1.  进程创建后，最后调用 `ActivityThread#main()` 方法，进入应用创建启动流程。
    
2.  **ActivityThread 就相当于我们的主线程，是应用程序的入口**。在 `main()` 方法里对 ActivityThread、主线程 Handler 进行初始化，然后 looper 开启消息轮询。执行到 `bindApplication()` 方法，开始创建 Application 并初始化，这里使用反射去创建，调用 Application 相关的生命周期。
    
3.  最后，通过主线程 Handler，回到主线程中执行 Activity 的创建和启动，然后执行 Activity 的相关生命周期函数。在 Activity LifeCycle 结束之后，**就会执行到 ViewRootImpl，这时才会进行真正的页面的绘制**。
    

在冷启动开始时，系统有以下三项任务：

1.  加载并启动应用；
2.  在启动后立即显示应用的空白启动窗口；
3.  创建应用进程。

系统创建应用进程后续阶段：

1.  创建应用对象，并走 Application 相关生命周期；
2.  启动主线程，Loop 消息循环；
3.  创建主 Activity，执行 Activity 的相关生命周期；
4.  解析视图，创建屏幕布局，执行初步绘制。  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d0f88a23b81ebec443f63356555bfb08.png#pic_center)  
    这是进程创建和启动的整个流程。

### 三、应用启动状态

应用有三种启动状态：**冷启动、温启动和热启动**。每种状态都会影响应用向用户显示所需的时间。在冷启动中，应用从头开始启动。在另外两种状态中，系统需要将后台运行的应用带入前台。启动优化一般是在冷启动的基础上进行优化，这样做也可以提升温启动和热启动的性能。

#### 1\. 冷启动

**冷启动是指应用从头开始启动，也就是用户点击桌面 Icon 到应用创建完成的过程**。所以系统进程是在冷启动后才创建应用进程。发生冷启动的情况包括应用自设备启动后或系统终止应用后首次启动。

常见的场景是 APP 首次启动或 APP 被完全杀死后重新启动。这种启动需要的时间最长的，因为系统和应用要做的工作比温启动和热启动状态下更多。**冷启动具有耗时最多，是衡量启动耗时的标准**。

以时间值的形式衡量，也就是指应用与用户进入可交互状态所需的时间。冷启动包含以下事件序列的总经过时间：

1.  创建和启动进程；
2.  创建 Application，启动主线程；
3.  创建启动主 Activity；
4.  解析布局；
5.  首次绘制应用。

#### 2\. 温启动

**温启动只是冷启动操作的一部分**。当启动应用时，后台已有该应用的进程，但是 **Activity 需要重新创建**。这样系统会从已有的进程中来启动这个 Activity，这个启动方式叫温启动。它的开销要比热启动高，比冷启动低。

温启动常见的场景有两种：

-   用户在退出应用后又重新启动应用。进程可能还在运行，但应用必须通过调用 `onCreate()` 重新创建 Activity。
-   系统因内存不足等原因将应用回收，然后用户又重新启动这个应用。Activity 需要重启，但传递给 `onCreate()` 的 state bundle 已保存相关数据。

#### 3\. 热启动

**在热启动中，系统的工作就是将 Activity 带到前台**。只要应用的所有 Activity 仍留在内存中，就不会重复执行进程，应用以及 Activity 的创建，避免重复初始化对象、布局解析和显现。应用的热启动开销比温启动更低，也是最简单的一种。热启动常见的场景: 当我们按了 Home 键或其它情况 App 被切换到后台，再次启动 App 的过程。

但是，如果一些内存为响应内存整理事件（如 `onTrimMemory()`）而被完全清除，则需要为了响应热启动而重新创建相应的对象。热启动显示的屏幕上行为和冷启动场景相同。系统进程显示空白屏幕，直到应用完成 Activity 呈现。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/83838d161bffbd4030a92138165d6e53.png#pic_center)

这就是应用三种启动状态的生命周期图。

### 四、启动优化阶段分析

启动速度的优化方向是 Application 和 Activity 生命周期阶段，创建进程阶段都是系统做的，从启动应用阶段开始，随后的任务和我们自己写的代码有一定的关系。比如 Application 的 `onCreate()` 和 `attachBaseContext()` 这两个生命周期回调方法的执行时间，在 Application 和 Activity 的回调方法中做的事情是我们可以干预的。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/24eea78c75e14ef13a8fcc6b607c2a30.png#pic_center)

#### 1\. Application阶段

在 Application阶段，可以在 attachBaseContext，installProvider 和 app:onCreate 三个时间段进行相关优化。

-   **bindApplication**：APP 进程由 zygote 进程 fork 出来后会执行 ActivityThread 的 main 方法，该方法最终触发执行 `bindApplication()`，这也是 Application 阶段的起点；
-   **attachBaseContext**：在应用中最早能触达到的生命周期，本阶段也是最早的预加载时机；
-   **installProvider**：很多三方 sdk 借助该时机来做初始化操作，很可能导致启动耗时的不可控情形，需要按具体 case 优化；
-   **onCreate**：这里有很多三方库和业务的初始化操作，是通过异步、按需、预加载等手段做优化的主要时机，它也是 Application 阶段的末尾。

#### 2\. Activity阶段

创建主 Activity 并且执行相关生命周期方法。在启动优化的专项中，Activity 阶段最关键的生命周期是 `onCreate()`，这个阶段中包含了大量的 UI 构建、首页相关业务、对象初始化等耗时任务，是我们在优化启动过程中非常重要的一环，我们可以通过异步、预加载、延迟执行等手段做各方面的优化。

#### 3\. 界面绘制阶段

来到 **View 构建的阶段**，该阶段也是比较耗时，可采用异步 Inflate 配合 X2C（编译期将 xml 布局转代码）并提升相应异步线程优先级的方法综合优化。

**View 的整体渲染阶段**，涵盖 measure、layout、draw 三部分，这里可尝试从层级、布局、渲染上取得优化收益。

最后是**首屏数据加载阶段**，这部分涵盖非常多数据相关的操作，也需要综合性优化，可尝试预加载、三级缓存或网络优先级调度等手段进行优化。

#### 4\. 首帧时间定义

我们在应用中能触达到的 **attachBaseContext** 阶段，这是最早的预加载时机。  
可以把这个方法的回调时间当作**启动开始时间**，因为 `attachBaseContext()` 是应用进程的第一个生命周期。但是准确来说，**应用的启动时间包括应用进程的创建，它应该是在冷启动时用户点击应用 Icon 开始计算**（下面会介绍统计方法）。但是结束时间点该如何来统计呢？

`Activity#onWindowFocusChanged()` 这个方法的调用时机是**用户与 Activity 交互的最佳时间点**，当 Activity 中的 View 测量绘制完成之后会回调 Activity 的 `onWindowFocusChanged()` 方法，可以选择它来当作时间结束的时间点。

但是这种还不够准确，大部分数据是通过请求接口回来之后，才能填充页面才能显示出来，当执行到 `onWindowFocusChanged()` 的时候，请求数据还没完成，页面上依旧是没有数据的，用户仅仅可以交互写死在 XML 布局当中的视图，更多的内容还是不可见，不可交互的。

所以结束时间点通常选择在**列表上面第一个 itemView 的 `perDrawCallback()` 方法**的回调时机当作时间结束点，也就是**首帧**时间。当列表上面第一个 itemView 被显示出来的时候说明网络请求已经完成。页面上的 View 已经填充了数据，并且开始重新渲染了。此时用户是可以交互的，这个才是比较有意义的时间节点。

```
// itemView添加预绘制回调监听
itemView.viewTreeObserver.addOnPreDrawListener(object : ViewTreeObserver.OnPreDrawListener {
    override fun onPreDraw(): Boolean {
        return false
    }
})
```

**从用户点击应用 icon 到页面生成首帧帧所用的时间，就是 App 启动耗时时间**。

### 五、启动问题分析

从启动流程的 3 个关键阶段，我们可以推测出用户启动过程会遇到比较多的 3 个问题。这 3 个问题其实也是大多数应用在启动时可能会遇到的。

#### 1\. 点击图标很久都不响应

如果我们禁用了预览窗口或者指定了透明的皮肤，那用户点击了图标之后，需要在创建启动页后才能真正看到应用闪屏。对于用户体验来说，点击了图标，过了几秒还是停留在桌面，看起来就像没有点击成功，这在中低端机中更加明显。

#### 2\. 首页显示太慢

现在应用启动流程越来越复杂，闪屏广告、热修复框架、插件化框架、各种SDK初始化，所有准备工作都需要集中在启动阶段完成。上面说的 Activity 阶段显示时间对于中低端机来说简直就是噩梦，经常会达到十秒的时间。

#### 3\. 首页显示后无法操作

既然首页显示那么慢，那我能不能把尽量多的工作都通过异步化延后执行呢？很多应用的确就是这么做的，但这会造成两种后果：要么首页会出现白屏，要么首页出来后用户根本无法操作交互。

很多开发者把启动结束时间的统计放到首页刚出现的时候，这对用户是不负责任的。看到一个首页，但是停住几秒都不能滑动，这对用户来说完全没有意义。**启动优化不能过于 KPI 化，要从用户的真实体验出发，要着眼从点击图标到用户可操作的整个过程。**

### 六、启动耗时统计的八种方式

#### 1\. Displayed

在 Android 4.4（API 级别 19）及更高版本中，在 Android Studio Logcat 中过滤关键**Displayed**，可以看到对应的冷启动耗时日志值。**此值代表从启动进程到在屏幕上完成对应 Activity 的绘制所用的时间**。

```
Displayed com.sum.tea/com.sum.main.MainActivity: +2s141ms
```

时间测量值是从应用进程启动时开始计算，仅针对第一个绘制的 Activity。它可能会省去布局文件中未引用的资源或被应用作为对象初始化一部分创建的资源。因为加载它们是一个内嵌进程，并且不会阻止应用的初步显示。

**这种方式最简单，适用于收集 App 与竞品 App 启动耗时对比分析**。

#### 2\. adb shell 命令方式

通过 `adb` shell activity Manager 命令运行应用来测量耗时时间。

```
adb shell am start -W [packageName]/[启动Activity的全路径]
```

比如命令窗口输入：

```
adb shell am start -W com.sum.tea/com.sum.main.MainActivity
```

命令窗口会显示以下内容：

```
Starting: Intent { act=android.intent.action.MAIN cat=[android.intent.category.LAUNCHER] cmp=com.sum.tea/com.sum.main.MainActivity }
Status: ok
Activity: com.sum.tea/com.sum.main.MainActivity
ThisTime: 1913
TotalTime: 1913
WaitTime: 2035
```

-   ThisTime：表示最后一个 Activity 启动耗时；
-   TotalTime：表示所有 Activity 启动耗时；
-   WaitTime：表示 AMS 启动 Activity 的总耗时。

这三者之间的关系为 `ThisTime <= TotalToime < WaitTime`。TotalTime 就是应用的启动时间，它包括**创建进程 + Application初始化 + Activity初始化到界面显示的过程**。

这种方式只适合线下使用，而且还能用于测量竞品 App 的耗时。缺点是不能带到线上而且不能**精确**控制启动时间的开始和结束，**数据不够严谨**。

#### 3\. reportFullyDrawn

当我们在使用**异步的方式来加载数据**，这会导致的一个问题就是应用画面已经显示，同时 Displayed 日志已经打印，可是**内容却还在加载中**。为了衡量这些异步加载资源所耗费的时间，可以在异步加载完毕之后调用 `activity.reportFullyDrawn()` 方法来让系统打印到调用此方法为止的启动耗时数据。

在首页的请求 Banner 数据完成时：

```
private fun refresh() {
    mViewModel.getBannerList().observe(this) { banners ->
        activity?.reportFullyDrawn()
    }
}
```

Logcat 打印数据如下：

```
Displayed com.sum.tea/com.sum.main.MainActivity: +3s171ms
Fully drawn com.sum.tea/com.sum.main.MainActivity: +4s459ms
```

#### 4\. 自定义埋点工具

如果要统计每个函数或者任务具体的耗时时间，最简单的方式就是自定义埋点工具。在函数执行前进行埋点，执行结束时埋点，两者差值就是函数执行耗时的时间。

```
object LaunchTimer {
    private var currentTime: Long = 0

    // 记录开始时间
    fun startRecord() {
        currentTime = System.currentTimeMillis()
    }

    // 记录结束时间，某个tag的耗时
    @JvmOverloads
    fun stopRecord(title: String = "") {
        val t = System.currentTimeMillis() - currentTime
        Log.d("LaunchTimer", "$title | time：$t")
    }
}
```

注意：还可以用 `SystemClock.currentThreadTimeMillis()` 获取 CPU 真正执行的时间。

开始记录的位置放在 Application 的 `attachBaseContext()` 中，它是我们应用能接收到的最早的一个生命周期回调方法。

```
class SumApplication : Application() {
    // 应用最早回调的生命周期方法
    override fun attachBaseContext(base: Context?) {
        super.attachBaseContext(base)
        LaunchTimer.startRecord()
        MultiDex.install(base)
    }
}
```

在数据加载显示首帧时添加结束计时打点，列表上面第一个 itemView 被显示出来的时候说明网络请求已经完成。

```
class HomeBannerAdapter : BaseBannerAdapter<Banner, BannerImageHolder>() {
    var isRecord = false

    override fun onBindView(holder: BannerImageHolder, data: Banner, position: Int, pageSize: Int) {
        // 第一个item，并且没有没注册监听
        if (position == 0 && !isRecord) {
            isRecord = true
            holder.itemView.viewTreeObserver.addOnPreDrawListener(object : ViewTreeObserver.OnPreDrawListener {
                override fun onPreDraw(): Boolean {
                    LaunchTimer.stopRecord("首帧绘制时间：")
                    // 移除监听
                    holder.itemView.viewTreeObserver.removeOnPreDrawListener(this)
                    return false
                }
            })
        }
    }
}
```

在 Adapter 中记录启动耗时要加一个布尔值变量 `isRecord` 进行判断，避免 onBindViewHolder 方法被多次调用。

但是这种方式**优点是可以精确控制开始和结束的位置而且可以带到线上**，进行用户数据的采集，把数据上报给服务器，服务器可以针对所有用户上报的启动数据。

**缺点是与业务强耦合，入侵性很强**，大型app启动流程复杂，业务繁多，工作量大，这种方案就显得很普通。

### 七、AOP编译插桩技术

Aspect Oriented Programming 是面向切面编程，**通过预编译和运行期动态代理实现程序功能统一维护**的一种技术。在统计方法耗时更多是使用切面编程的方式，可以在编译时期插入一些代码。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/a9f4a7db4e572f2d4f20a9198f46de14.png)

其实**编译插桩技术**早已经深入 `Android` 开发中的各个领域，而 `AOP` 技术正是一种高效实现插桩的模式。

#### 1\. AOP核心概念

| AOP核心概念 | 说明 |
| --- | --- |
| **横切关注点** | 对哪些方法进行拦截，拦截后怎么处理。 |
| **切面（Aspect）** | 类是对物体特征的抽象，切面就是对横切关注点的抽象。 |
| **连接点（JoinPoint）** | 被拦截到的点（方法、字段、构造器）。 |
| **切入点（PointCut）** | 对JoinPoint进行拦截的定义。 |
| **通知（Advice）** | 拦截到JoinPoint后要执行的代码，分为前置、后置、环绕三种类型。 |

#### 2\. Advice注解类型

| 注解类型 | 说明 |
| --- | --- |
| **Before** | PointCut 切入点之前执行 |
| **After** | PointCut 切入点之后执行 |
| **Around** | PointCut 切入点之前、之后分别执行 |

#### 3\. Join Point类型

| 访问类型 | 说明 |
| --- | --- |
| **call** | **代表调用函数的位置，插入在函数体外面** |
| **execution** | **代表函数执行的位置，插入在函数体内部**。 |

#### 4\. AOP埋点实战

1.  需要在项目根目录的 `build.gradle` 文件中加入依赖：

```
buildscript {
    dependencies {
        // AOP切面编程
        classpath 'com.hujiang.aspectjx:gradle-android-plugin-aspectjx:2.0.10'
    }
}
```

在 app 的 `build.gradle` 文件中加入插件和依赖：

```
// 加入插件
apply plugin: 'android-aspectjx'
// AOP 切面编程
implementation 'org.aspectj:aspectjrt:1.9.5'
```

2.  JoinPoint 一般定位在**函数调用**、**获取、设置变量**、**类初始化**这三个位置。使用 PointCut 对我们指定的连接点进行拦截，通过 Advice 就可以拦截到 JoinPoint 后要执行的代码：

```
@Before("execution(* android.app.Activity.on**(..))")
fun onActivityCalled(joinPoint: JoinPoint) {
    // 这里可以插入任意代码段
}
```

**在 execution 中的是一个匹配规则，第一个 \* 代表匹配任意的方法返回值，后面的语法代码匹配所有 Activity 中 on 开头的方法，`..`代表该方法可以是任意参数**。这样就可以在 App 中所有 Activity 中以 on 开头的方法中输出代码了。

3.  还可以统计 Application 中的所有方法耗时：

```
// 注解产生关联
@Aspect
class SumOptAop {
    // @Around每个函数前后都插入代码
    @Around("call(* com.sum.tea.SumApplication**(..))")
    fun getApplicationTime(joinPoint: ProceedingJoinPoint) {
        val signature = joinPoint.signature // 获取函数名称
        val curTime = System.currentTimeMillis()
        // 上面的代码会插入在函数前面
        try {
            // 执行原方法代码
            joinPoint.proceed()
        } catch (e: Exception) {
            e.printStackTrace()
        }
        // 下面的代码会插入在函数后面
        LogUtil.e("${signature.name} | time：${System.currentTimeMillis() - curTime}")
    }
}
```

要注意不同的 Action 类型其对应的方法入参是不同的：

-   如果 Action 为 **Before、After** 时，方法入参为 **JoinPoint**。
-   如果 Action 为 **Around** 时，方法入参为 **ProceedingPoint**。

它们的区别是：ProceedingPoint 是提供了 `proceed()` 方法执行目标方法的，而 JoinPoint 是没有的。

4.  表达式格式和常用写法

```
// 任何一个以set开始的方法
@Before("execution(* set*(..))")
// 任意公共方法的执行
@After("execution(public * *(..))")
// Activity 的任意方法
@Around("execution(android.app.Activity. *(..))")
// View的点击事件
@Around("execution(void android.view.View.OnClickListener.onClick(..))")
```

#### 5\. AOP的使用场景

-   无痕埋点：分离业务代码和统计代码；
-   安全控制：比如全局的登录状态流程控制；
-   日志记录：侵入性更低更有利于管控日志系统；
-   事件防抖：防止View被连续点击触发多次事件；
-   性能统计：检测方法耗时。采用 AOP 思想对每个方法做一个切点，在执行后打印方法耗时。

利用 AOP 可以对业务逻辑的各个部分进行隔离，从而使得业务逻辑各部分之间的耦合性降低，提高程序的可重用性，同时大大提高了开发效率。并且**无侵入性，修改方便**。

### 八、启动速度分析工具

#### 1\. TraceView

Traceview 是 android 平台配备一个很好的性能分析的工具。它可以通过图形化的方式让我们了解我们要跟踪的程序的性能，并且能具体到每个方法的执行时间。

首先调用 `startMethodTracing(String tracePath)` 开始记录 CPU 使用情况，调用 `stopMethodTracing()` 停止记录。系统就会为我们生成一个 `.trace` 文件，我们可以通过 CPU Profiler 查看这个文件记录的内容。

```
class SumApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        // 自定义路径和文件大小
        // Debug.startMethodTracing(getExternalFilesDir(null) + "SumTea.trace", 12 * 1024) 
        // 开始记录
        Debug.startMethodTracing()
        initSumHelper()
        initMmkv()
        initAppManager()
        initRefreshLayout()
        initArouter()
        // 结束记录
        Debug.stopMethodTracing()
     }
} 
```

文件生成的位置默认在(SD卡) `Android/data/包名/files` 目录下，可以通过 Android Studio 的 `Device File Exploer` 设备文件管理器中查看：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/a9d9fced10c9f2c8a2c08f580182b5fe.jpeg#pic_center)  
**注意：文件最大默认是8M，可以手动扩充大小，也可以自定义文件路径。**

在 Android Studio 中双击该文件可以在 CPU Profiler 直接打开：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/02633cfafc4cb3362febcf4d08ad2909.png#pic_center)  
这里有三个主要区域，**时间范围区域，线程区域，分析数据区域**。分析数据区域有四种方式，分别是Call Chart、Flame Chart、Top Down、Bottom Up。

-   **1\. 时间选择范围**：表示 trace 文件的**整个时间段**，可以拖动蓝色区域选择具体的检查记录时间范围来检查。
    
-   **2\. 线程区域**：表示所有线程的数据，沿时间轴显示显示线程状态活动和调用图，**main 是主线程**。
    
-   **3\. Call Chart**：线程区域的右侧其实就是 Call Chart，**它按照应用程序的函数执行顺序来展示，适合用于分析整个流程的调用**。水平轴：表示调用的时间段和时间。垂直轴：显示被调用方。橙色：系统 API；绿色：应用自有方法；蓝色：第三方 API（包括 Java API）。右键点击 `Jump to source` 跳转至指定函数。
    
-   **4\. 数据分析区域**：显示所选时间范围和线程或方法调用的跟踪数据。可以查看每个堆栈跟踪(使用分析选项卡)以及如何度量执行时间。
    
-   **5\. Flame Chart**：**火焰图**，用来汇总完全相同的调用栈，将具有相同调用方顺序的完全相同的方法收集起来，并在火焰图中将它们表示为一个较长的横条。看顶层的哪个函数占据的宽度最大（平顶），可能存在性能问题。
    
-   **6\. Top Down**：函数递归调用列表，在该列表中可以追踪到具体方法以及耗时，可以显示精确的时间。图中的每个箭头都是从调用方指向被调用方。可以点击跳转源码。
    
-   **7\. Bottom Up**：展开函数的节点会显示它的调用方，可以很方便的找到某个方法的调用栈，可以看到有哪些方法调用了自己。按照占用的的 CPU 时间由多到少（或由少到多）的顺序对方法排序，与 Top Down 相反。
    

左侧的程序执行时间和 CPU 消耗时间：

-   **wall clock time**：代码执行在线程，这个线程真正花费的时间。
-   **thread time**：CPU 执行消耗的时间，表示实际经过的时间减去线程没占用 CPU 资源的那部分时间。可以更好地了解给定方法或函数消耗了多少线程的实际 CPU 使用量。两者关系：`thread time >= wall clock time`。

数据分析区域中有几种时间单位：

-   **total**：表示函数调用的总时间，Self 和 Childern 时间的总和；
-   **self time**：表示执行自身代码花费的时间；
-   **childern time**：表示子方法执行花费的时间。

Call Chart 是 Traceview 和 systrace 默认使用的展示方式。当我们不想知道应用程序的整个调用流程，只想直观看出哪些代码路径花费的 CPU 时间较多时，火焰图就是一个非常好的选择。

通过 TraceView 主要可以得到两种数据：单次执行耗时的方法以及执行次数多的方法。它的优点是可以在代码中埋点，埋点后可以用 CPU Profiler 进行分析，因为我们现在优化的是启动阶段的代码。

但是 TraceView 是利用 Android Runtime 函数调用的 event 事件，将函数运行的耗时和调用关系写入 trace 文件中。它属于 `instrument` 类型，能查看整个过程有哪些函数调用，**但是工具本身带来的性能开销过大，有时无法反映真实的情况，可能会带偏优化方向**。比如一个函数本身的耗时是 1 秒，开启 Traceview 后可能会变成 3 秒，而且这些函数的耗时变化并不是成比例放大。

在 Android 5.0 之后，新增了 `startMethodTracingSampling` 方法，可以使用基于样本的方式进行分析，**以减少分析对运行时的性能影响**。新增了 sample 类型后，就需要我们在开销和信息丰富度之间做好权衡。

#### 2\. CPU Profiler

另一种方式就是使用 Android Studio3.2 或更高版本，通过 CPU Profiler 来查看 App 的启动时间：

1.  在 Android Studio 工具中选择 `Run > Edit Configurations` 配置界面；
2.  在 App 中选择 Profiling，勾选 `Start this recording on startup` 选项；
3.  从菜单中选择 `Java/Kotlin Methods Trace`；
4.  单击 Apply，配置生效；
5.  通过选择 Run > Profile，待应用运行起来（需要 USB 连接手机 App）。  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c4dbbfdb30166eab34952e3e3ed818fa.png#pic_center)  
    记录配置 `Trace types` 有四种类型：

| 类型 | 说明 |
| --- | --- |
| **Sample Java Methods** | 对 java/Kotlin 方法进行采样，**频繁的采集应用调用堆栈**，并收集 java 代码执行的时间和相关的资源使用信息。对运行时性能的影响比较小，**能够记录更大的数据区域**。 |
| **Trace Java Methods** | 系统会收集并比较每个 Java/kotlin 方法的时间信息和 CPU 使用率，生成方法跟踪数据，对性能影响较大。 |
| **Sample C/C++ Functions** | 对 C/C++ 函数进行采样，捕获应用程序 Native 线程的采样轨迹。需要 Andorid8.0 以上 |
| **Trace System Calls** | 跟踪系统调用并捕获细节，用于检查应用和系统资源的交互情况。可检测线程状态的时间信息和所有内核的 CPU，并添加自定义跟踪事件。 |

当工具运行起来后，点击 stop 按钮停止记录，然后工具会自动分析并生成生成一份 `Java Method Trace Record` 文件：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9c1f950ff755e3d17c69e4dfd8dc0dc9.png#pic_center)

文件生成后会自动跳转到**数据分析面板**，和 TraceView 中的数据分析面板是一样的（这里不重复分析了）。

CPU Profiler 的默认视图包括以下时间轴:

-   **1\. Event timeline**：表示事件时间线，显示应用程序中在其生命周期中转换不同状态的活动，如用户交互、屏幕旋转事件等。
-   **2\. CPU timeline**：表示 CPU 时间线，显示 App 实时 CPU 使用情况、其它进程实的 CPU 使用率、应用程序使用的线程总数。
-   **3\. Thread activity timeline**：表示线程活动时间线，列出 App 进程中的每个线程，并使用了不同的颜色在其时间轴上指示其活动。可以选择一个线程，在跟踪面板中检查它的数据。

线程活动时间线不同的颜色表示的含义：

-   绿色：表示线程处于活动状态或准备好使用 CPU，它处于运行或可运行状态。
-   黄色：表示线程是活动的，正等待 IO 操作。（重要）
-   灰色：表示线程正在睡眠，不消耗 CPU 时间。

**使用 CPU Profiler 在与 App 交互时能实时检查 CPU 的使用率和线程活动，也可以检查记录的方法轨迹、函数轨迹和系统轨迹的详情**。CPU 性能分析器记录和显示的详细信息取决于您选择的记录配置。它是性能优化方面非常重要的工具之一。

#### 3\. SysTrace + 编译插桩

Systrace 结合了 Android 内核数据，分析了线程活动后会给我们生成一个非常精确 HTML 格式的报告。我们一般是通过**代码插桩**的形式配合使用。

Systrace 原理是在系统的一些关键链路（如 SystemServcie、虚拟机、Binder 驱动）插入一些信息（Label）。然后，通过 Label 的**开始和结束**来确定某个核心过程的执行时间，并把这些 Label 信息收集起来得到系统关键路径的运行时间信息，最后得到整个系统的运行性能信息。其中，Android Framework 里面一些重要的模块都插入了 label 信息，用户 App 中也可以添加自定义的 Lable。

Systrace 工具只能监控特定系统调用的耗时情况，所以它是属于 sample 类型，而且性能开销非常低。它不支持应用程序代码的耗时分析，所以在使用时有一些局限性。

但是由于系统预留了 `Trace.beginSection` 接口来监听应用程序的调用耗时，**通过编译时给每个函数插桩的方式来实现**，也就是在重要函数的入口和出口分别增加 `Trace.begainSection()`，`Trace.endSection()`方法。

1.  在想要分析耗时的方法**前后进行插桩**，这里将 `Application#onCreate()` 中的初始化任务分别插桩处理，重新运行 App:

```
class SumApplication : Application() {
    // 应用最早回调的生命周期方法
    override fun attachBaseContext(base: Context?) {
        super.attachBaseContext(base)
        Trace.beginSection("MultiDex")
        MultiDex.install(base)
        Trace.endSection()
    }

    override fun onCreate() {
        super.onCreate()
        // 添加标识，方便查询   
        Trace.beginSection("initMmkv")
        initMmkv()
        Trace.endSection()
        
        Trace.beginSection("initAppManager")
        initAppManager()
        Trace.endSection()
        
        Trace.beginSection("initRefreshLayout")
        initRefreshLayout()
        Trace.endSection()
        
        Trace.beginSection("initArouter")
        initArouter()
        Trace.endSection()
    }
}
```

2.  然后 cd 到 SDK 的 `platform-tools/systrace` 目录下，执行 `systrace.py` 脚本命令：

```
python systrace.py -t 8 -a "com.sum.tea" -o sum_tea_01.html
```

这里定义的一些具体参数含义如下：

-   **\-t**：后面表示的是跟踪的时间，比如设定的是 8 秒就结束；
-   **shced**：cpu调度信息；
-   **gfx**：图形信息；
-   **view**：视图；
-   **wm**：窗口管理；
-   **am**：活动管理；
-   **app**：应用信息；
-   **webview**：webview信息；
-   **\-a**：指定目标应用程序的包名；
-   **\-o**：文件输出指定目录下，生成的systrace.html文件。

```
//切换到systrace目录下
dnsb1389@SUM ~ % cd /Users/dnsb1389/Library/Android/sdk/platform-tools/systrace
// 输入命令
dnsb1389@SUM systrace % python systrace.py -t 8 -a "com.sum.tea" -o sum_tea_01.html

Agent cgroup_data not started.
These categories are unavailable: memory workq
Warning: Only 2 of 3 tracing agents started.
Starting tracing (8 seconds)
```

3.  当出现 `Starting tracing (8 seconds)` 时，手动启动 App，跟踪8秒后就会在指定目录生成了 html 文件。

```
Tracing completed. Collecting output...
Outputting Systrace results...
Tracing complete, writing results

Wrote trace HTML file: file:///Users/dnsb1389/Library/Android/sdk/platform-tools/
/sum_tea_01.html
```

4.  到这里追踪完毕，追踪的数据信息都写到 `sum_tea_01.html` 文件中了，接下来用浏览器打开这个文件：  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/12ba5b61336ee4dc4d2799abf02e90ea.png#pic_center)  
    在这个数据分析表中我们主要关注四个区域：

-   **1\. UI Thread**：找到包名下的主线程，主要关注这里运行了哪些耗时方法。
-   **2\. 方法的分析区**：通过上面插桩的方法的具体耗时都显示在这里了，段落越长代表耗时越多。
-   **3\. Slice耗时信息**：选中2中的某个方法段落，会显示出具体的耗时时间。
-   **4\. 指示控制器**：通过这个工具可以移动、缩放来操作这个分析图形区域，展示到适合的大小。

5.  在 Slice 标签下的耗时信息包括 `Wall Duration` 和 `Self Time`，它们是有区别的：

-   `Wall Duration`： **表示执行这段代码耗费的时间，它不能作为优化指标**。假如我们的代码要进入锁的临界区，如果锁被其他线程持有，当前线程就进入了阻塞状态，而等待的时间是会被计算到 `Wall Duration` 中的。
    
-   `Self Time`：**方法执行真正的耗时，CPU 真正花在这段代码上的时间，它是我们关心的优化指标**。
    

在上面的例子中 `Wall Duration` 是 844 毫秒，`Self Time` 是 830 毫秒，也就是在这段时间内一共有 14 毫秒 CPU 是处于休息状态的，真正执行代码的时间只花了 830 毫秒。

**注意：**

1.  这种方式实际上是通过执行 `platform-tools/systrace` 目录中 `systrace.py` 的 python 脚本，获取追踪系统信息的，需要电脑端安装 Python 软件。
2.  `platform-tools` 目录中的 systrace 插件在30之后的版本已经被**移除**了，如果要使用这种方式，需要替换回 platform-tools-29（含）之前的版本。

**优缺点**

-   Systrace 的性能开销非常低，因为它只会在我们埋点区间进行数据追踪记录。而 Traceview 是会把所有的线程的堆栈调用情况都记录下来。
    
-   Systrace 是结合 **Android 内核的数据**，生成 **Html** 报告的。可以很直观地看到 CPU 利用率的情况。当发现 CPU 利用率低的时候，可以考虑让更多代码以异步的方式执行，以提高 CPU 利用率。
    

Systrace 主要用于分析绘制性能方面的问题和分析**系统关键方法和应用方法耗时**。而且**系统版本越高，Android Framework 中添加的系统可用 Label 就越多，能够支持和分析的系统模块也就越多**。

#### 4\. Perfetto

另外，Perfetto 是 Android 10 中引入的全新平台级跟踪工具，可以看作 Systrace 的升级版。这是适用于 Android、Linux 和 Chrome 的更加通用和复杂的开源跟踪项目。与 Systrace 不同，它提供数据源超集，可让你以 protobuf 编码的二进制流形式记录任意长度的跟踪记录，可以在 Perfetto 界面中打开这些跟踪记录。这里就不做分析了，有兴趣的同学可以参考[Perfetto](https://perfetto.dev/docs/)。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d928bf340bbe3389bec632cf7bdfa7d3.png#pic_center)

### 九、耗时统计数据分析

#### 1\. 低中高端设备

在 Android 中，内存和 CPU 是描述低端机型的比较关键的两个指标，我们根据 Android 用户的不同设备做了性能划分，初步可划分为高、中、低3种等级。**启动性能优化目标是以低端机为重点，辐射中高端机。**

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/e75e1a4abd7eb7a94c2679003fc61b81.png#pic_center)

由于 App 启动速度在不同的设备上差别很大，我们在获取耗时数据时也最好对低、中、高机型都进行统计分析。可以使用低端机型，中端机型，高端机型三种定制不同的目标。

#### 2\. 耗时数据统计

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/afaa11ff1aa86f4c21d74f29b77b2c17.png#pic_center)

**数据统计为后续启动优化提高应用启动速度做好数据准备**。耗时统计从用户点击 App 开始统计，直到**首帧**时间结束。表格数据在同一机型下冷启动**三次结果取平均值**，这样才更具代表性和意义。因为**单次数据**的启动可能存在较大的误差，取均值能将误差降到最低。

#### 3\. App竞品对比

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3bf2ccf0c588fe12725bac867f037e11.png#pic_center)

我们都希望自己应用的启动速度可以秒杀所有竞争对手。统计竞品 APP 启动耗时与自身 App 对比，更清楚了解到当前 App 与竞品 App 之间的差距。

**只有准确的数据评估才能指引优化的方向，这一步是非常非常重要的**。太多同学在没有充分评估或者评估使用了错误的方法，最终得到了错误的方向。辛辛苦苦一两个月，最后发现根本达不到预期的效果。

由于篇幅有限，启动速度优化的实战方案在下一篇中讲解，请不要走开，我马上回来……

### 点关注，不迷路

好了各位，以上就是这篇文章的全部内容了，很感谢您阅读这篇文章。我是suming，感谢支持和认可，您的**点赞**就是我创作的最大动力。**山水有相逢**，我们下篇文章见！

本人水平有限，文章难免会有错误，请批评指正，不胜感激 ！

### 参考链接

-   [Google官网：应用启动](https://developer.android.google.cn/topic/performance/vitals/launch-time?hl=zh-cn)
-   [Improving App Performance with Baseline Profiles](https://android-developers.googleblog.com/2022/01/improving-app-performance-with-baseline.html)
-   [Android应用启动全流程分析](https://www.jianshu.com/p/37370c1d17fc)
-   [理解Android进程创建流程](https://gityuan.com/2016/03/26/app-process-create/)
-   [Android进程框架：进程的创建、启动与调度流程](https://juejin.cn/post/6844903553283145735?searchId=20240405064124E4E599293F6DD052E5AD#heading-4)
-   [Systrace](https://developer.android.google.cn/topic/performance/tracing/navigate-report?hl=zh-cn)
-   [使用 CPU Profiler 检查 CPU 活动](https://developer.android.google.cn/studio/profile/cpu-profiler?hl=zh)
-   [记录Trace](https://developer.android.google.cn/studio/profile/record-traces#configurations)
-   [Android 开发高手课](https://time.geekbang.org/column/intro/142?gk_ucode=6891EC39BDEA36&tab=catalog)
-   [国内Top团队大牛带你玩转Android性能分析与优化](https://coding.imooc.com/class/package/308.html)
-   [Android App 启动优化全记录](https://androidperformance.com/2019/11/18/Android-App-Lunch-Optimize)
-   [抖音Android性能优化系列：启动优化之理论和工具篇](https://mp.weixin.qq.com/s?__biz=MzI1MzYzMjE0MQ==&mid=2247491335&idx=1&sn=e3eabd9253ab2f83925af974db3f3072&scene=21#wechat_redirect)
-   [深入探索Android启动速度优化（上）](https://juejin.cn/post/6844904093786308622#heading-164)
-   [Android启动优化实践 - 秒开率从17%提升至75%](https://juejin.cn/post/7306692609497546752#heading-19)
-   [探索 Android 启动优化方法](https://juejin.cn/post/6844903919580086280#heading-15)

#### 希望我们能成为朋友，在 [Github](https://github.com/suming77/Android-Core-Realm)、[博客](https://blog.csdn.net/m0_37796683) 上一起分享知识，一起共勉！Keep Moving!