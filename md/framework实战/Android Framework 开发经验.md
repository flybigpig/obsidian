

用Android Studio 看代码，千万不要用Source Insight，Android Studio 看代码快捷，搜索也快。 Source Insight搜索代码太慢，而且不方便，搜个代码要几分钟。
学会断点调试。 将镜像编译成UserDebug版本，这样可以对system_process进程断点调试。 如果是传统User版本，则看不到system_process进程。
多加调试代码，例如：把调用堆栈显示出来

```
if (DEBUG) 
{
       StackTraceElement[] stack = new Throwable().getStackTrace();
       for (StackTraceElement element : stack)
      {
            Slog.d(TAG, "TAG |----" + element.toString());
      }
}

```


原文链接：https://blog.csdn.net/achirandliu/article/details/106167197