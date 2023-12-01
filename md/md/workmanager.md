# workmanager

Android上有许多选项可用于延迟后台工作，此代码库涵盖了WorkManager

这是一个兼容 灵活且简单的库。用于可延迟的后台工作。

WorkManager目前处于alpha状态。他将成为Android上推荐的任务调度程序。



##### workmanager

支持异步一次性和定期任务

支持网络条件 存储空间和计费状态等约束

链接复杂的工作请求，包括并行运行工作。

一个工作请求的输出用作下一个的输入

将api级别兼容性处理回api级别14

使用或者不适用Google play服务

遵循系统健康最佳实践

livedata支持。可以再ui 中显示工作请求状态。



> Worker
>  任务的执行者，是一个抽象类，需要继承它实现要执行的任务。
>
> WorkRequest
>  指定让哪个 Woker 执行任务，指定执行的环境，执行的顺序等。
>  要使用它的子类 OneTimeWorkRequest 或 PeriodicWorkRequest。
>
> WorkManager
>  管理任务请求和任务队列，发起的 WorkRequest 会进入它的任务队列。
>
> WorkStatus
>  包含有任务的状态和任务的信息，以 LiveData 的形式提供给观察者





```kotlin
dependencies {
    ......
    implementation 'androidx.work:work-runtime:2.3.4'
}
```

```kotlin
class SimpleWorker(context: Context, workerParams: WorkerParameters) :
    Worker(context, workerParams) {
    override fun doWork(): Result {
        Log.d("SimpleWorker", "doWork")
        return Result.success()
    }
}
```

```kotlin
// 执行一次的任务
val request = OneTimeWorkRequest.Builder(SimpleWorker::class.java).build()
// 周期任务，循环周期不可小于15分钟
val request2 = PeriodicWorkRequest.Builder(SimpleWorker::class.java, 15, TimeUnit.MINUTES).build()
```

```kotlin
WorkManager.getInstance(this).enqueue(request)
```

```kotlin
 // 创建链式任务, 如果在前面的人任务失败，之后的任务不会得到在执行
 WorkManager.getInstance(this)
         .beginWith(request)
         .then(request)
         .then(request).enqueue()
```
