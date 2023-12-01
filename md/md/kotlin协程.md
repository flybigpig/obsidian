# kotlin协程



> 协程 
>
> 一种组织代码运行的方式。
>
> 协程和线程 进程不同，他不是由操作系统直接提供支持，二十通过应用层的库来实现，譬如 kotlin协程，其实就是依赖java/android的线程、线程池再加上一些对上下文的控制逻辑来实现的。

优点：

 1 因为某些协程库的实现使用了任务分发，于是在协程函数中（也就是kotlin,corountine的suspend方法）中无线调用递归不会栈溢出，当然这是依赖实现，不能保证全部。

2 如上文所说，协程是一种组织代码的方式，因此可以将一部调用组织成掉哟个的书写形式，因而可以免除了毁掉的地域问题

3 因为协程本质上是一种用户线程，在现成的基础上加了一层自己的调度，它的创建和delay延迟都开销很小

```
implementation "org.jetbrains.kotlinx:kotlinx-coroutines-android:1.0.0"
```





```
def anko_version = "0.10.8"
// Anko Commons
implementation "org.jetbrains.anko:anko-commons:$anko_version"

implementation 'com.github.CymChad:BaseRecyclerViewAdapterHelper:3.0.4'

def room_version = "2.3.0"

implementation "androidx.room:room-runtime:$room_version"
implementation "androidx.room:room-ktx:$room_version"

kapt "androidx.room:room-compiler:2.2.5"

// optional - Test helpers
testImplementation "androidx.room:room-testing:$room_version"


def appsearch_version = "1.0.0-alpha01"

implementation("androidx.appsearch:appsearch:$appsearch_version")

kapt("androidx.appsearch:appsearch-compiler:$appsearch_version")

implementation("androidx.appsearch:appsearch-local-storage:$appsearch_version")

implementation 'androidx.multidex:multidex:2.0.1'

implementation 'com.jakewharton.retrofit:retrofit2-kotlin-coroutines-adapter:0.9.2'
```





协程启动模式

GlobalScope.launch{  }

启动协程需要三洋东西，分别是上下文，启动模式，协程体。

协程体好比Thread.run 中的代码

```

public enum class CoroutineStart {
    DEFAULT,               // 立即执行
    LAZY, 					// 只有在需要的时候才运行
    @ExperimentalCoroutinesApi   
    ATOMIC,          // 立即执行，但是在开始运行前无法取消
    @ExperimentalCoroutinesApi
    UNDISPATCHED;          //  立即在当前线程执行协程体 知道第一个suspend调用
}
```

> JVM 上默认调度器的实现也许你已经猜到，没错，就是开了一个线程池，但区区几个线程足以调度成千上万个协程，而且每一个协程都有自己的调用栈，这与纯粹的开线程池去执行异步任务有本质的区别。
>
> 当然，我们说 Kotlin 是一门跨平台的语言，因此上述代码还可以运行在 JavaScript 环境中，例如 Nodejs。在 Nodejs 中，Kotlin 协程的默认调度器则并没有实现线程的切换，输出结果也会略有不同，这样似乎更符合 JavaScript 的执行逻辑。