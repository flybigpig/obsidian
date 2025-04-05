```
public static void main(String argv[]) {  
    ZygoteServer zygoteServer = null;  
  
    // Mark zygote start. This ensures that thread creation will throw  
    // an error.    ZygoteHooks.startZygoteNoThreadCreation();  
  
    // Zygote goes into its own process group.  
    try {  
        Os.setpgid(0, 0);  
    } catch (ErrnoException ex) {  
        throw new RuntimeException("Failed to setpgid(0,0)", ex);  
    }  
  
    Runnable caller;  
    try {  
        // Report Zygote start time to tron unless it is a runtime restart  
        if (!"1".equals(SystemProperties.get("sys.boot_completed"))) {  
            MetricsLogger.histogram(null, "boot_zygote_init",  
                    (int) SystemClock.elapsedRealtime());  
        }  
  
        String bootTimeTag = Process.is64Bit() ? "Zygote64Timing" : "Zygote32Timing";  
        TimingsTraceLog bootTimingsTraceLog = new TimingsTraceLog(bootTimeTag,  
                Trace.TRACE_TAG_DALVIK);  
        bootTimingsTraceLog.traceBegin("ZygoteInit");  
        RuntimeInit.enableDdms();  
  
        boolean startSystemServer = false;  
        String zygoteSocketName = "zygote";  
        String abiList = null;  
        boolean enableLazyPreload = false;  
        for (int i = 1; i < argv.length; i++) {  
            if ("start-system-server".equals(argv[i])) {  
                startSystemServer = true;  
            } else if ("--enable-lazy-preload".equals(argv[i])) {  
                enableLazyPreload = true;  
            } else if (argv[i].startsWith(ABI_LIST_ARG)) {  
                abiList = argv[i].substring(ABI_LIST_ARG.length());  
            } else if (argv[i].startsWith(SOCKET_NAME_ARG)) {  
                zygoteSocketName = argv[i].substring(SOCKET_NAME_ARG.length());  
            } else {  
                throw new RuntimeException("Unknown command line argument: " + argv[i]);  
            }  
        }  
  
        final boolean isPrimaryZygote = zygoteSocketName.equals(Zygote.PRIMARY_SOCKET_NAME);  
  
        if (abiList == null) {  
            throw new RuntimeException("No ABI list supplied.");  
        }  
  
        // In some configurations, we avoid preloading resources and classes eagerly.  
        // In such cases, we will preload things prior to our first fork.        if (!enableLazyPreload) {  
            bootTimingsTraceLog.traceBegin("ZygotePreload");  
            EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_START,  
                    SystemClock.uptimeMillis());  
            preload(bootTimingsTraceLog);  // 加载class  资源  
            EventLog.writeEvent(LOG_BOOT_PROGRESS_PRELOAD_END,  
                    SystemClock.uptimeMillis());  
            bootTimingsTraceLog.traceEnd(); // ZygotePreload  
        } else {  
            Zygote.resetNicePriority();  
        }  
  
        // Do an initial gc to clean up after startup  
        bootTimingsTraceLog.traceBegin("PostZygoteInitGC");  
        gcAndFinalize();  
        bootTimingsTraceLog.traceEnd(); // PostZygoteInitGC  
  
        bootTimingsTraceLog.traceEnd(); // ZygoteInit  
        // Disable tracing so that forked processes do not inherit stale tracing tags from        // Zygote.        Trace.setTracingEnabled(false, 0);  
  
  
        Zygote.initNativeState(isPrimaryZygote);  
  
        ZygoteHooks.stopZygoteNoThreadCreation();  
  
        zygoteServer = new ZygoteServer(isPrimaryZygote);  // 初始化zygoteserver  创建socket  
  
        if (startSystemServer) {  
            Runnable r = forkSystemServer(abiList, zygoteSocketName, zygoteServer);  
  
            // {@code r == null} in the parent (zygote) process, and {@code r != null} in the  
            // child (system_server) process.            if (r != null) {  
                r.run();  
                return;            }  
        }  
  
        Log.i(TAG, "Accepting command socket connections");  
  
        // The select loop returns early in the child process after a fork and  
        // loops forever in the zygote.        caller = zygoteServer.runSelectLoop(abiList);  
    } catch (Throwable ex) {  
        Log.e(TAG, "System zygote died with exception", ex);  
        throw ex;  
    } finally {  
        if (zygoteServer != null) {  
            zygoteServer.closeServerSocket();  
        }  
    }  
  
    // We're in the child process and have exited the select loop. Proceed to execute the  
    // command.    if (caller != null) {  
        caller.run();  
    }  
}
```