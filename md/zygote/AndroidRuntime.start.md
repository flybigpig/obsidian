```
void AndroidRuntime::start(const char* className, const Vector<String8>& options, bool zygote)  
{  
    JniInvocation jni_invocation;  
    jni_invocation.Init(NULL);  
    JNIEnv* env;  
    if (startVm(&mJavaVM, &env, zygote) != 0) {  
        return;  
    }  
    onVmCreated(env);  
  
    /*  
     * Register android functions.     */    if (startReg(env) < 0) {  
        ALOGE("Unable to register all android natives\n");  
        return;    }  
  
    /*  
     * We want to call main() with a String array with arguments in it.     * At present we have two arguments, the class name and an option string.     * Create an array to hold them.     */    jclass stringClass;  
    jobjectArray strArray;  
    jstring classNameStr;  
  
    stringClass = env->FindClass("java/lang/String");  
    assert(stringClass != NULL);  
    strArray = env->NewObjectArray(options.size() + 1, stringClass, NULL);  
    assert(strArray != NULL);  
    classNameStr = env->NewStringUTF(className);  
    assert(classNameStr != NULL);  
    env->SetObjectArrayElement(strArray, 0, classNameStr);  
  
    for (size_t i = 0; i < options.size(); ++i) {  
        jstring optionsStr = env->NewStringUTF(options.itemAt(i).string());  
        assert(optionsStr != NULL);  
        env->SetObjectArrayElement(strArray, i + 1, optionsStr);  
    }  
  
    /*  
     * Start VM.  This thread becomes the main thread of the VM, and will     * not return until the VM exits.     */    char* slashClassName = toSlashClassName(className != NULL ? className : "");  
    jclass startClass = env->FindClass(slashClassName);  
    if (startClass == NULL) {  
        ALOGE("JavaVM unable to locate class '%s'\n", slashClassName);  
        /* keep going */  
    } else {  
        jmethodID startMeth = env->GetStaticMethodID(startClass, "main",   "([Ljava/lang/String;)V");  
        if (startMeth == NULL) {  
            ALOGE("JavaVM unable to find main() in '%s'\n", className);  
            /* keep going */  
        } else {  
            env->CallStaticVoidMethod(startClass, startMeth, strArray);  
  
#if 0  
            if (env->ExceptionCheck())  
                threadExitUncaughtException(env);  
#endif  
        }  
    }  
    free(slashClassName);  
  
    ALOGD("Shutting down VM\n");  
    if (mJavaVM->DetachCurrentThread() != JNI_OK)  
        ALOGW("Warning: unable to detach main thread\n");  
    if (mJavaVM->DestroyJavaVM() != 0)  
        ALOGW("Warning: VM did not shut down cleanly\n");  
}
```