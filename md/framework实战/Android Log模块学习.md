
本文介绍了如何在Android Studio中控制应用程序在debug和release模式下日志的打印。通过检查BuildConfig.DEBUG的值，可以在debug模式下打印Log，并利用Log.isLoggable方法结合adb shell命令设置特定tag的日志级别，从而在release环境中抑制不必要的日志输出。示例代码展示了如何在日志级别达到VERBOSE时打印特定tag的日志，并提供了adb命令来开启或关闭特定tag的日志打印。


AndroidStudio中：

1.  如果我们在开发应用过程中只要debug状态时打印log，在release环境就不会打印log，可如下封装(适用于gradle编译)：  
    就是检测BuildConfig.DEBUG的值

```java
if (BuildConfig.DEBUG) {
	Log.v(tag, message);
}
```

2.  控制打印某个log level以下的日志

```java
if (BuildConfig.DEBUG && Log.isLoggable("volley", Log.VERBOSE)) {
	Log.v("volley", message);
}
```

系统中：  
这里我们在打印tag为volley的日志之前，先判断当前的Log level是否是大于等于VERBOSE的，我们期望的是tag为volley的日志仅当log level大于等于VERBOSE的时候才打印出来,就可以使用以下命令：

```shell
adb shell setprop log.tag.<YOUR_LOG_TAG>  <LEVEL>
```

例如:

```shell
adb shell setprop log.tag.volley VERBOSE  # 只打印我们定义的tag为volley且level是VERBOSE  的log
```

LEVEL的值可以是VERBOSE, DEBUG, INFO, WARN, ERROR, ASSERT, 或者 SUPPRESS， SUPPRESS会禁止打印所有日志。

在Android源码中SQLiteDebug.Java中就有这样的实现：

```shell
adb shell setprop log.tag.SQLiteLog V  打印  tag是SQLiteLog 的VERBOSE级别的日志
adb shell setprop log.tag.SQLiteStatements V  打印  tag是SQLiteStatements 的VERBOSE级别的日志
adb shell stop  关闭日志
adb shell start 开启日志
```