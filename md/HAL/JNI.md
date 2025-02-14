
```c
#include <jni.h>
#include <android/log.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include<android/log.h>

#include "com_example_myapplicationndk_MainActivity.h"

#define  TAG = " asd"
#define  LOG_TAG    "nativeprint"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

JNIEXPORT jstring JNICALL
Java_com_example_myapplicationndk_MainActivity_fromJNIString(JNIEnv *env, jobject thiz) {
    // TODO: implement fromJNIString()
    return (*env)->NewStringUTF(env, "I am From Native C 1121312   as");
}

JNIEXPORT jstring JNICALL
Java_com_example_myapplicationndk_MainActivity_testString(JNIEnv *env, jobject thiz,
                                                          jstring jstr) {
    const char *c_str = NULL;
    char buff[128] = {0};
    jboolean isCopy;    // 返回JNI_TRUE表示原字符串的拷贝，返回JNI_FALSE表示返回原字符串的指针
    c_str = (*env)->GetStringUTFChars(env, jstr, &isCopy);
    printf("isCopy:%d\n", isCopy);
    if (c_str == NULL) {
        return NULL;
    }
    printf("C_str: %s \n", c_str);
    sprintf(buff, "hello %s", c_str);
    (*env)->ReleaseStringUTFChars(env, jstr, c_str);
    return (*env)->NewStringUTF(env, buff);
}

JNIEXPORT jint JNICALL
Java_com_example_myapplicationndk_MainActivity_add(JNIEnv *env, jobject thiz, jint a, jint b) {
    // TODO: implement add()
    return a + b;
}

JNIEXPORT jstring JNICALL
Java_com_example_myapplicationndk_MainActivity_jniIntArrayToString(JNIEnv *env, jobject thiz,
                                                                   jintArray intArrayParams) {
    // TODO: implement jniIntArrayToString()
    char ret[256] = "";
    char c[256] = "";
    int paramCount = (*env)->GetArrayLength(env, intArrayParams);  // 长度
    jint *paramValues = (*env)->GetIntArrayElements(env, intArrayParams, 0);
    for (int i = 0; i < paramCount; ++i) {
        jint *curParamValue = (paramValues + i);
        sprintf(c, "%d", *curParamValue);
        strcat(ret, c);
        printf("=============== %s", ret);
        if (i != paramCount - 1) {
            strcat(ret, ",");
        }
    }
    (*env)->ReleaseIntArrayElements(env, intArrayParams, paramValues, 0);
    return (*env)->NewStringUTF(env, ret);
}

JNIEXPORT jstring JNICALL
Java_com_example_myapplicationndk_MainActivity_callJavaStaticMethodFromJni(JNIEnv *env,
                                                                           jobject thiz,
                                                                           jclass classes) {
    // TODO: implement callJavaStaticMethodFromJni()
    jclass jniHandle = (*env)->FindClass(env, "com/example/myapplicationndk/utils/JniHandle");
    if (NULL == jniHandle) {
        return (*env)->NewStringUTF(env, "");
    }
    jmethodID getStringFromStatic = (*env)->GetStaticMethodID(env, classes, "getStringFromStatic",
                                                              "()Ljava/lang/String;");
    if (NULL == getStringFromStatic) {
        (*env)->DeleteLocalRef(env, jniHandle);
        return (*env)->NewStringUTF(env, "");
    }

    jstring result = (*env)->CallStaticObjectMethod(env, classes, getStringFromStatic);
    const char *resultChar = (*env)->GetStringUTFChars(env, result, NULL);
    (*env)->DeleteLocalRef(env, jniHandle);
    (*env)->DeleteLocalRef(env, result);
    LOGD("resultChar :  %s", resultChar);
}

JNIEXPORT jstring JNICALL
Java_com_example_myapplicationndk_MainActivity_callJavaMethodFromJni(JNIEnv *env,
                                                                     jobject thiz,
                                                                     jclass classes) {

    jmethodID constructor = (*env)->GetMethodID(env, classes, "<init>", "()V");
    if (NULL == constructor) {
        LOGD("can't constructor JniHandle");
        return (*env)->NewStringUTF(env, "1");
    }

    jobject jniHandleObject = (*env)->NewObject(env, classes, constructor);
    if (NULL == jniHandleObject) {
        LOGD("can't new JniHandle");
        return (*env)->NewStringUTF(env, "2");
    }
    jmethodID getStringForJava = (*env)->GetMethodID(env, classes, "getNames",
                                                     "()Ljava/lang/String;");
    if (NULL == getStringForJava) {
        LOGD("can't find method of getStrin "
             "gForJava");
        (*env)->DeleteLocalRef(env, classes);
        (*env)->DeleteLocalRef(env, jniHandleObject);
        return (*env)->NewStringUTF(env, "3");
    }
    jstring result = (*env)->CallObjectMethod(env, jniHandleObject, getStringForJava);
    const char *resultChar = (*env)->GetStringUTFChars(env, result, NULL);
    (*env)->DeleteLocalRef(env, jniHandleObject);
    (*env)->DeleteLocalRef(env, result);
    return (*env)->NewStringUTF(env, resultChar);
}

```