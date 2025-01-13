-   原标题: Android Fragments: Fragment Result
-   原文地址: [https://proandroiddev.com/android-fragments-fragment-result......](https://link.zhihu.com/?target=https%3A//proandroiddev.com/android-fragments-fragment-result-805a6b2522ea)
-   原文作者: Husayn Hakeem

今年 Google 推出了 `Fragment Result API` 和 `Activity Results API`，用来取代之前的 Activity 和 Fragment 之间通信方式的不足。

这篇文章大概是我在 5 月份的写的，主要介绍 `Fragment Result API`，分为 **译文** 和 **译者的思考** 两个部分。

`Fragment Result API` 主要介绍 Fragment 间通信的新方式，是在 `Fragment 1.3.0-alpha04` 新增加的 API ，而现在最新版本已经到 `fragment-1.3.0-beta01` 应该很快就能应用在项目里面了。

接下来分析一下 `Fragment Result API` 主要为我们解决了什么问题，它都有那些更新。

**通过这篇文章你将学习到以下内容，将在译者思考部分会给出相应的答案**

-   新 Fragment 间通信的方式的使用？
-   新 Fragment 间通信的源码分析？
-   汇总 Fragment 之间的通信的方式？

## **译文**

Frrgament 间传递数据可以通过多种方式，包括使用 target Fragment APIs (`Fragment.setTargetFragment()` 和 `Fragment.getTargetFragment()`)，`ViewModel` 或者 使用 Fragments’ 父容器 Activity，target Fragment APIs 已经过时了，现在鼓励使用新的 Fragment result APIs 完成 Frrgament 之间传递数据，其中传递数据由 FragmentManager 处理，并且在 Fragments 设置发送数据和接受数据

### **在 Frrgament 之间传递数据**

使用新的 Fragment APIs 在 两个 Frrgament 之间的传递，没有任何引用，可以使用它们公共的 FragmentManager，它充当 Frrgament 之间传递数据的中心存储。

### **接受数据**

如果想在 Fragment 中接受数据，可以在 FragmentManager 中注册一个 FragmentResultListener，参数 requestKey 可以过滤掉 FragmentManager 发送的数据

```
FragmentManager.setFragmentResultListener(
    requestKey,
    lifecycleOwner,
    FragmentResultListener { requestKey: String, result: Bundle ->
        // Handle result
    })
```

参数 lifecycleOwner 可以观察生命周期，当 Fragment 的生命周期处于 [STARTED](https://link.zhihu.com/?target=https%3A//developer.android.com/reference/androidx/lifecycle/Lifecycle.State%23STARTED) 时接受数据。如果监听 Fragment 的生命周期，您可以在接收到新数据时安全地更新 UI，因为 view 的创建(onViewCreated() 方法在 onStart() 之前被调用)。

![](https://pic2.zhimg.com/v2-bf93d82d95796419183ee206ddd09ba1_1440w.jpg)

当生命周期处于 LifecycleOwner STARTED 的状态之前，如果有多个数据传递，只会接收到最新的值

![](https://pic1.zhimg.com/v2-039cccc4999fbb57d7144a966f91633e_1440w.jpg)

当生命周期处于 LifecycleOwner DESTROYED 时，它将自动移除 listener，如果想手动移除 listener，需要调用 FragmentManager.setFragmentResultListener() 方法，传递空的 FragmentResultListener

![](https://pic1.zhimg.com/v2-10c9b67a48e49a2f8b47e3ff71388e30_1440w.jpg)

在 FragmentManager 中注册 listener，依赖于 Fragment 发送返回的数据

-   如果在 FragmentA 中接受 FragmentB 发送的数据，FragmentA 和 FragmentB 处于相同的层级，通过 parent FragmentManager 进行通信，FragmentA 必须使用 parent FragmentManager 注册 listener

```
parentFragmentManager.setFragmentResultListener(...)
```

-   如果在 FragmentA 中接受 FragmentB 发送的数据，FragmentA 是 FragmentB 的父容器， 他们通过 child FragmentManager 进行通信

```
childFragmentManager.setFragmentResultListener(...)
```

listener 必须设置的Fragment 相同的 FragmentManager

### **发送数据**

如果 FragmentB 发送数据给 FragmentA，需要在 FragmentA 中注册 listener，通过 parent FragmentManager 发送数据

```
parentFragmentManager.setFragmentResult(
    requestKey, // Same request key FragmentA used to register its listener
    bundleOf(key to value) // The data to be passed to FragmentA
)
```

### **测试 Fragment Results**

测试 Fragment 是否成功接收或发送数据，可以使用 [FragmentScenario](https://link.zhihu.com/?target=https%3A//developer.android.com/reference/androidx/fragment/app/testing/FragmentScenario) API

### **接受数据**

如果在 FragmentA 中注册 FragmentResultListener 接受数据，你可以模拟 parent FragmentManager 发送数据，如果在 FragmentA 中正确注册了 listener，可以用来验证 FragmentA 是否能收到数据，例如，如果在 FragmentA 中接受数据并更新 UI, 可以使用 Espresso APIs 来验证是否期望的数据

```
@Test
fun shouldReceiveData() {
    val scenario = FragmentScenario.launchInContainer(FragmentA::class.java)

    // Pass data using the parent fragment manager
    scenario.onFragment { fragment ->
        val data = bundleOf(KEY_DATA to "value")
        fragment.parentFragmentManager.setFragmentResult("aKey", data)
    }

    // Verify data is received, for example, by verifying it's been displayed on the UI
   onView(withId(R.id.textView)).check(matches(withText("value"))) 
}
```

### **发送数据**

可以在 FragmentB 的 parent FragmentManager 上注册一个 FragmentResultListener 来测试 FragmentB 是否成功发送数据，当发送数据结束时，可以来验证这个 listener 是否能收到数据

```
@Test
fun shouldSendData() {
    val scenario = FragmentScenario.launchInContainer(FragmentB::class.java)

    // Register result listener
    var receivedData = ""
    scenario.onFragment { fragment ->
        fragment.parentFragmentManager.setFragmentResultListener(
            KEY,
            fragment,
            FragmentResultListener { key, result ->
                receivedData = result.getString(KEY_DATA)
            })
    }

    // Send data
    onView(withId(R.id.send_data)).perform(click())

    // Verify data was successfully sent
    assertThat(receivedData).isEqualTo("value")
}
```

### **总结**

虽然使用了 Fragment result APIs，替换了过时的 Fragment target APIs，但是新的 APIs 在Bundle 作为数据传传递方面有一些限制，只能传递简单数据类型、Serializable 和 Parcelable 数据，Fragment result APIs 允许程序从崩溃中恢复数据，而且不会持有对方的引用，避免当 Fragment 处于不可预知状态的时，可能发生未知的问题

## **译者的思考**

这是译者的一些思考，总结一下 **Fragment 1.3.0-alpha04** 新增加的 Fragment 间通信的 API

**数据接受**

```
FragmentManager.setFragmentResultListener(
    requestKey,
    lifecycleOwner,
    FragmentResultListener { requestKey: String, result: Bundle ->
        // Handle result
    })
```

**数据发送**

```
parentFragmentManager.setFragmentResult(
    requestKey, // Same request key FragmentA used to register its listener
    bundleOf(key to value) // The data to be passed to FragmentA
)
```

那么 Fragment 间通信的新 API 给我们带来哪些好处呢：

-   在 Fragment 之间传递数据，不会持有对方的引用
-   当生命周期处于 ON\_START 时开始处理数据，避免当 Fragment 处于不可预知状态的时，可能发生未知的问题
-   当生命周期处于 ON\_DESTROY 时，移除监听

我们一起来从源码的角度分析一下 Google 是如何做的

### **源码分析**

按照惯例从调用的方法来分析，数据接受时，调用了 FragmentManager 的 setFragmentResultListener 方法

**androidx.fragment/fragment/1.3.0-alpha04......androidx/fragment/app/FragmentManager.java**

```
private final ConcurrentHashMap<String, LifecycleAwareResultListener> mResultListeners =
        new ConcurrentHashMap<>();

@Override
public final void setFragmentResultListener(@NonNull final String requestKey,
                                            @NonNull final LifecycleOwner lifecycleOwner,
                                            @Nullable final FragmentResultListener listener) {
    // mResultListeners 是 ConcurrentHashMap 的实例，用来储存注册的 listener
    // 如果传递的参数 listener 为空时，移除 requestKey 对应的 listener
    if (listener == null) {
        mResultListeners.remove(requestKey);
        return;
    }

    // Lifecycle是一个生命周期感知组件，一般用来响应Activity、Fragment等组件的生命周期变化
    final Lifecycle lifecycle = lifecycleOwner.getLifecycle();
    // 当生命周期处于 DESTROYED 时，直接返回
    // 避免当 Fragment 处于不可预知状态的时，可能发生未知的问题
    if (lifecycle.getCurrentState() == Lifecycle.State.DESTROYED) {
        return;
    }

    // 开始监听生命周期
    LifecycleEventObserver observer = new LifecycleEventObserver() {
        @Override
        public void onStateChanged(@NonNull LifecycleOwner source,
                                   @NonNull Lifecycle.Event event) {
            // 当生命周期处于 ON_START 时开始处理数据
            if (event == Lifecycle.Event.ON_START) {
                // 开始检查受到的数据
                Bundle storedResult = mResults.get(requestKey);
                if (storedResult != null) {
                    // 如果结果不为空，调用回调方法
                    listener.onFragmentResult(requestKey, storedResult);
                    // 清除数据
                    setFragmentResult(requestKey, null);
                }
            }

            // 当生命周期处于 ON_DESTROY 时，移除监听
            if (event == Lifecycle.Event.ON_DESTROY) {
                lifecycle.removeObserver(this);
                mResultListeners.remove(requestKey);
            }
        }
    };
    lifecycle.addObserver(observer);
    mResultListeners.put(requestKey, new FragmentManager.LifecycleAwareResultListener(lifecycle, listener));
}
```

-   Lifecycle是一个生命周期感知组件，一般用来响应Activity、Fragment等组件的生命周期变化
-   获取 Lifecycle 去监听 Fragment 的生命周期的变化
-   当生命周期处于 ON\_START 时开始处理数据，避免当 Fragment 处于不可预知状态的时，可能发生未知的问题
-   当生命周期处于 ON\_DESTROY 时，移除监听

接下来一起来看一下数据发送的方法，调用了 FragmentManager 的 setFragmentResult 方法

**androidx.fragment/fragment/1.3.0-alpha04......androidx/fragment/app/FragmentManager.java**

```
private final ConcurrentHashMap<String, Bundle> mResults = new ConcurrentHashMap<>();
private final ConcurrentHashMap<String, LifecycleAwareResultListener> mResultListeners =
        new ConcurrentHashMap<>();
    
@Override
public final void setFragmentResult(@NonNull String requestKey, @Nullable Bundle result) {
    if (result == null) {
        // mResults 是 ConcurrentHashMap 的实例，用来存储数据传输的 Bundle
        // 如果传递的参数 result 为空，移除 requestKey 对应的 Bundle
        mResults.remove(requestKey);
        return;
    }

    // mResultListeners 是 ConcurrentHashMap 的实例，用来储存注册的 listener
    // 获取 requestKey 对应的 listener
    LifecycleAwareResultListener resultListener = mResultListeners.get(requestKey);
    if (resultListener != null && resultListener.isAtLeast(Lifecycle.State.STARTED)) {
        // 如果 resultListener 不为空，并且生命周期处于 STARTED 状态时，调用回调
        resultListener.onFragmentResult(requestKey, result);
    } else {
        // 否则保存当前传输的数据
        mResults.put(requestKey, result);
    }
}
```

-   获取 requestKey 注册的 listener
-   当生命周期处于 STARTED 状态时，开始发送数据
-   否则保存当前传输的数据

源码分析到这里结束了，我们一起来思考一下，在之前我们的都有那些数据传方式

### **汇总 Fragment 之间的通信的方式**

-   通过共享 ViewModel 或者关联 Activity来完成，Fragment 之间不应该直接通信 [参考 Google: ViewModel#sharing](https://link.zhihu.com/?target=https%3A//developer.android.com/topic/libraries/architecture/viewmodel%23sharing)
-   通过接口，可以在 Fragment 定义接口，并在 Activity 实现它 [参考 Google: 与其他 Fragment 通信](https://link.zhihu.com/?target=https%3A//developer.android.com/training/basics/fragments/communicating)
-   通过使用 findFragmentById 方法，获取 Fragment 的实例，然后调用 Fragment 的公共方法 [参考 Google: 与其他 Fragment 通信](https://link.zhihu.com/?target=https%3A//developer.android.com/training/basics/fragments/communicating)
-   调用 Fragment.setTargetFragment() 和 Fragment.getTargetFragment() 方法，**但是注意 target fragment 需要直接访问另一个 fragment 的实例，这是十分危险的，因为你不知道目标 fragment 处于什么状态**
-   Fragment 新的 API, setFragmentResult() 和 setFragmentResultListener()

综合以上通信方式，那么你认为 Fragment 之间通信最好的方式是什么？

## **参考文献**

-   [Now in Android #17: https://medium.com/androiddeve......](https://link.zhihu.com/?target=https%3A//medium.com/androiddevelopers/now-in-android-17-9d73f7bed7f)
-   [Pass data between fragments: https://developer.android.com/training/basi......](https://link.zhihu.com/?target=https%3A//developer.android.com/training/basics/fragments/pass-data-between)
-   [ViewModel#sharing: https://developer.android.com/topic/librari......](https://link.zhihu.com/?target=https%3A//developer.android.com/topic/libraries/architecture/viewmodel%23sharing)
-   [与其他 Fragment 通信: https://developer.android.com/training/basic......](https://link.zhihu.com/?target=https%3A//developer.android.com/training/basics/fragments/communicating)

## **结语**

全文到这里就结束了，如果有帮助 **点个赞** 就是对我最大的鼓励！

致力于分享一系列 Android 系统源码、[逆向分析](https://zhida.zhihu.com/search?content_id=148769851&content_type=Article&match_order=1&q=%E9%80%86%E5%90%91%E5%88%86%E6%9E%90&zhida_source=entity)、算法、翻译、Jetpack 源码相关的文章，在技术的道路上一起前进

___

最后推荐我一直在更新维护的项目和网站：

-   计划建立一个最全、最新的 AndroidX Jetpack 相关组件的实战项目 以及 相关组件原理分析文章，正在逐渐增加 Jetpack 新成员，仓库持续更新，欢迎前去查看：

-   LeetCode / 剑指 offer / 国内外大厂面试题 / 多线程 题解，语言 Java 和 kotlin，包含多种解法、解题思路、时间复杂度、[空间复杂度分析](https://zhida.zhihu.com/search?content_id=148769851&content_type=Article&match_order=1&q=%E7%A9%BA%E9%97%B4%E5%A4%8D%E6%9D%82%E5%BA%A6%E5%88%86%E6%9E%90&zhida_source=entity)

![](https://pic1.zhimg.com/v2-e30c2131d7ec03cb2464c922cea85b78_1440w.jpg)

-   剑指 offer 及国内外大厂面试题解：

-   LeetCode 系列题解：

-   最新 Android 10 源码分析系列文章，了解系统源码，不仅有助于分析问题，在面试过程中，对我们也是非常有帮助的，仓库持续更新，欢迎前去查看

-   整理和翻译一系列精选国外的技术文章，每篇文章都会有**译者思考**部分，对原文的更加深入的解读，仓库持续更新，欢迎前去查看

-   「为互联网人而设计，国内国外名站导航」涵括新闻、体育、生活、娱乐、设计、产品、运营、前端开发、Android 开发等等网址，欢迎前去查看