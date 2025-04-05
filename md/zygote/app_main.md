```
int main(int argc, char* const argv[])  
{  
  
    // Parse runtime arguments.  Stop at first unrecognized option.  
    bool zygote = false;  
    bool startSystemServer = false;  
    bool application = false;  
    String8 niceName;  
    String8 className;  
  
  
    // //init传递过来的参数如下 -Xzygote /system/bin --zygote --start-system-server    ++i;  // Skip unused "parent dir" argument.  
    while (i < argc) {  
        const char* arg = argv[i++];  
        if (strcmp(arg, "--zygote") == 0) {//传递的参数有--zygote的就把zygote赋值为true niceName = zygote  
            zygote = true;  
            niceName = ZYGOTE_NICE_NAME;  
        } else if (strcmp(arg, "--start-system-server") == 0) { //传递的参数有start--system-server 把startSystemServer 赋值为tue表示当前进程的main是需要开启system_server的  
            startSystemServer = true;  
        } else if (strcmp(arg, "--application") == 0) {//如果传递的参数包含了--application 表示当前是应用程序  
            application = true;  
        } else if (strncmp(arg, "--nice-name=", 12) == 0) {//指定进程名  
            niceName.setTo(arg + 12);  
        } else if (strncmp(arg, "--", 2) != 0) {//application程序传递过来的className 也就是需要启动的class  
            className.setTo(arg);  
            break;        } else {  
            --i;  
            break;        }  
    }  
  
  
    if (zygote) {  
        runtime.start("com.android.internal.os.ZygoteInit", args, zygote);  
    } else if (className) {  
        runtime.start("com.android.internal.os.RuntimeInit", args, zygote);  
    } else {  
        fprintf(stderr, "Error: no class name or --zygote supplied.\n");  
        app_usage();  
        LOG_ALWAYS_FATAL("app_process: no class name or --zygote supplied.");  
    }  
}
```