> 十年鹅厂程序员，专注大前端、AI、个人成长、副业搞钱  
> 技术交流群qiejun2020（备注：Android）  
> 公众号：企鹅君技术圈  

## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

Android系统中

-   应用层打印日志 import android.util.Log, 在 需 要 打 印 Log 的 地 方 执 行

```
Log.v,Log.d,Log.i,Log.w,Log.e
```

-   framework层

老版本包含 #include <cutils/log.h>，代码调用 LOGV,LOGD,LOGI,LOGW,LOGE；

新版本包含 #include <log/log.h>， 代码调用 ALOGV,ALOGD,ALOGI,ALOGW,ALOGE;

-   内核层

使用printk打印日志

```
int printk(const char *fmt, ...)
```

内核层的printk是内核独立的，而应用层和framework层日志的打印都依赖logd服务，而logd服务是由其祖先init进程创建的，因此引发一个问题：logd服务还未启动时，init进程的log该如何处理？

答案：init进程的日志写入内核。

init进程main入口代码如下

```
//system/core/init/main.cpp

#include <android-base/logging.h>

int main(int argc, char** argv) {
    if (!strcmp(basename(argv[0]), "ueventd")) {
        return ueventd_main(argc, argv);
    }

    if (argc > 1) {
        if (!strcmp(argv[1], "subcontext")) {
            android::base::InitLogging(argv, &android::base::KernelLogger);
            const BuiltinFunctionMap function_map;

            return SubcontextMain(argc, argv, &function_map);
        }

        if (!strcmp(argv[1], "selinux_setup")) {
            return SetupSelinux(argv);
        }

        if (!strcmp(argv[1], "second_stage")) {
            return SecondStageMain(argc, argv);
        }
    }

    return FirstStageMain(argc, argv);
}
```

会执行init进程的第一阶段代码FirstStageMain

> 其中引入的android-base/logging.h头文件很重要

```
//system/core/init/first_stage_init.cpp

int FirstStageMain(int argc, char** argv) {
    InitKernelLogging(argv);

    LOG(INFO) << "init first stage started!";
}

void InitKernelLogging(char** argv) {
    SetFatalRebootTarget();
    android::base::InitLogging(argv, &android::base::KernelLogger, InitAborter);
}
```

在FirstStageMain中先回调用InitKernelLogging初始化日志，最重要的代码如下

```
// system/core/base/logging.cpp

void InitLogging(char* argv[], LogFunction&& logger, AbortFunction&& aborter) {
  SetLogger(std::forward<LogFunction>(logger));
  ......
}  

void SetLogger(LogFunction&& logger) {
  std::lock_guard<std::mutex> lock(LoggingLock());
  Logger() = std::move(logger);
}

static LogFunction& Logger() {
#ifdef __ANDROID__
  static auto& logger = *new LogFunction(LogdLogger());
#else
  static auto& logger = *new LogFunction(StderrLogger);
#endif
  return logger;
}
```

最后的结果就是将android::base::KernelLogger赋值给静态变量logger。而KernelLogger如下：

```
void KernelLogger(android::base::LogId, android::base::LogSeverity severity,
                  const char* tag, const char*, unsigned int, const char* msg) {
  // clang-format off
  static constexpr int kLogSeverityToKernelLogLevel[] = {
      [android::base::VERBOSE] = 7,              // KERN_DEBUG (there is no verbose kernel log
                                                 //             level)
      [android::base::DEBUG] = 7,                // KERN_DEBUG
      [android::base::INFO] = 6,                 // KERN_INFO
      [android::base::WARNING] = 4,              // KERN_WARNING
      [android::base::ERROR] = 3,                // KERN_ERROR
      [android::base::FATAL_WITHOUT_ABORT] = 2,  // KERN_CRIT
      [android::base::FATAL] = 2,                // KERN_CRIT
  };
  // clang-format on
  static_assert(arraysize(kLogSeverityToKernelLogLevel) == android::base::FATAL + 1,
                "Mismatch in size of kLogSeverityToKernelLogLevel and values in LogSeverity");

  static int klog_fd = OpenKmsg();
  if (klog_fd == -1) return;

  int level = kLogSeverityToKernelLogLevel[severity];

  // The kernel's printk buffer is only 1024 bytes.
  // TODO: should we automatically break up long lines into multiple lines?
  // Or we could log but with something like "..." at the end?
  char buf[1024];
  size_t size = snprintf(buf, sizeof(buf), "<%d>%s: %s\n", level, tag, msg);
  if (size > sizeof(buf)) {
    size = snprintf(buf, sizeof(buf), "<%d>%s: %zu-byte message too long for printk\n",
                    level, tag, size);
  }

  iovec iov[1];
  iov[0].iov_base = buf;
  iov[0].iov_len = size;
  TEMP_FAILURE_RETRY(writev(klog_fd, iov, 1));
}
```

这个函数首先调用OpenKmsg打开设备节点/dev/kmsg（内核的日志信息就在此文件中）

```
static int OpenKmsg() {
#if defined(__ANDROID__)
  // pick up 'file w /dev/kmsg' environment from daemon's init rc file
  const auto val = getenv("ANDROID_FILE__dev_kmsg");
  if (val != nullptr) {
    int fd;
    if (android::base::ParseInt(val, &fd, 0)) {
      auto flags = fcntl(fd, F_GETFL);
      if ((flags != -1) && ((flags & O_ACCMODE) == O_WRONLY)) return fd;
    }
  }
#endif
  return TEMP_FAILURE_RETRY(open("/dev/kmsg", O_WRONLY | O_CLOEXEC));
}
```

然后调用writev(klog\_fd, iov, 1)将日志信息写入/dev/kmsg。

日志初始化玩成后，init进行就可以使用如下宏进行日志输出了

> LOG(severity):平常日志打印用  
> PLOG(severity)主要用于出错时打印，会将出错码打印出来 sterror(errno)

```
// system/core/base/include/android-base/logging.h

#define LOG(severity) LOG_TO(DEFAULT, severity)
#define PLOG(severity) PLOG_TO(DEFAULT, severity)


#define PLOG_TO(dest, severity)                                                        \
  LOGGING_PREAMBLE(severity) &&                                                        \
      ::android::base::LogMessage(__FILE__, __LINE__, ::android::base::dest,           \
                                  SEVERITY_LAMBDA(severity), _LOG_TAG_INTERNAL, errno) \
          .stream()
```

android::base::LogMessage调用了LogMessage类的构造函数，结果就是data\_被设置为LogMessageData对象

```
// system/core/base/logging.cpp

LogMessage::LogMessage(const char* file, unsigned int line, LogId id, LogSeverity severity,
                       const char* tag, int error)
    : data_(new LogMessageData(file, line, id, severity, tag, error)) {}
```

然后调用LogMessage的stream方法

```
std::ostream& LogMessage::stream() {
  return data_->GetBuffer();
}
```

此方法返回的就是LogMessageData的buffer\_，当调用如下代码进行日子输出时就会写入到buffer\_中。

```
LogMessage::~LogMessage() {
  {
    // Do the actual logging with the lock held.
    std::lock_guard<std::mutex> lock(LoggingLock());
    if (msg.find('\n') == std::string::npos) {
      LogLine(data_->GetFile(), data_->GetLineNumber(), data_->GetId(), data_->GetSeverity(),
              data_->GetTag(), msg.c_str());
    } else {
      msg += '\n';
      size_t i = 0;
      while (i < msg.size()) {
        size_t nl = msg.find('\n', i);
        msg[nl] = '\0';
        LogLine(data_->GetFile(), data_->GetLineNumber(), data_->GetId(), data_->GetSeverity(),
                data_->GetTag(), &msg[i]);
        // Undo the zero-termination so we can give the complete message to the aborter.
        msg[nl] = '\n';
        i = nl + 1;
      }
    }
  }
}
```

最后在LogMessage的析构函数中调用LogLine，而LogLine调用Logger(),它里面的静态变量logger就是前面赋值的android::base::KernelLogger。

```
void LogMessage::LogLine(const char* file, unsigned int line, LogId id, LogSeverity severity,
                         const char* tag, const char* message) {
  if (tag == nullptr) {
    std::lock_guard<std::recursive_mutex> lock(TagLock());
    if (gDefaultTag == nullptr) {
      gDefaultTag = new std::string(getprogname());
    }
    Logger()(id, severity, gDefaultTag->c_str(), file, line, message);
  } else {
    Logger()(id, severity, tag, file, line, message);
  }
}
```

到此为止init进程就通过/dev/kmsg写入了内核缓冲区。