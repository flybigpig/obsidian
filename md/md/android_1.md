# android_1



### 深入理解jni



当java层调用native_init函数时，它会从对应的jni 库寻找**Java_android_media_MediaScanner_native_init**函数。如果没有，就会报错。如果找到，则会为这个**native_init**和**Java_android_media_MediaScanner_native_init**建立一个联系，后面在调用，就直接使用这个函数指针就好。



**弊端：**

1  需要编译声明native函数的java类。每个生成的class文件都得用javah生成一个头文件。

2  javah生成的jni函数名比较长，不方便书写。

3  初次调用native函数时要根据函数名字搜索对应的jni层函数来建立关系。这样影响效率



java native是通过函数指针和jni 层建立关系

如果直接让native函数知道jni 层对应的函数指针，那就提高效率。



动态注册
原理：利用 RegisterNatives 方法来注册 java 方法与 JNI 函数的一一对应关系；

实现流程：



> 编写Java端的相关native方法
>
> 编写C/C++代码, 实现JNI_Onload()方法
>
> 将Java 方法和 C/C++方法通过签名信息一一对应起来
>
> 通过JavaVM获取JNIEnv, JNIEnv主要用于获取Java类和调用一些JNI提供的方法
>
> 使用类名和对应起来的方法作为参数, 调用JNI提供的函数RegisterNatives()注册方法



    // jni头文件 
    #include <jni.h>
     
    #include <cassert>
    #include <cstdlib>
    #include <iostream>
    using namespace std;
     
     
    //native 方法实现
    jint get_random_num(){
        return rand();
    }
    /*需要注册的函数列表，放在JNINativeMethod 类型的数组中，
    以后如果需要增加函数，只需在这里添加就行了
    参数：
    1.java中用native关键字声明的函数名
    2.签名（传进来参数类型和返回值类型的说明） 
    3.C/C++中对应函数的函数名（地址）
    */
    static JNINativeMethod getMethods[] = {
            {"getRandomNum","()I",(void*)get_random_num},
    };
    //此函数通过调用RegisterNatives方法来注册我们的函数
    static int registerNativeMethods(JNIEnv* env, const char* className,JNINativeMethod* getMethods,int methodsNum){
        jclass clazz;
        //找到声明native方法的类
        clazz = env->FindClass(className);
        if(clazz == NULL){
            return JNI_FALSE;
        }
       //注册函数 参数：java类 所要注册的函数数组 注册函数的个数
        if(env->RegisterNatives(clazz,getMethods,methodsNum) < 0){
            return JNI_FALSE;
        }
        return JNI_TRUE;
    }
     
    static int registerNatives(JNIEnv* env){
        //指定类的路径，通过FindClass 方法来找到对应的类
        const char* className  = "com/example/wenzhe/myjni/JniTest";
        return registerNativeMethods(env,className,getMethods, sizeof(getMethods)/ sizeof(getMethods[0]));
    }
    //回调函数
    JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved){
        JNIEnv* env = NULL;
       //获取JNIEnv
        if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            return -1;
        }
        assert(env != NULL);
        //注册函数 registerNatives ->registerNativeMethods ->env->RegisterNatives
        if(!registerNatives(env)){
            return -1;
        }
        //返回jni 的版本 
        return JNI_VERSION_1_6;
    }


JNINativeMethod
在动态注册的过程中使用到了结构体 JNINativeMethod 用于记录 java 方法与 jni 函数的对应关系

```
typedef struct {
    const char* name;
    const char* signature;
    void*       fnPtr;
} JNINativeMethod;
```



结构体的第一个参数 name 是java 方法名；

第二个参数 signature 用于描述方法的参数与返回值；

第三个参数 fnPtr 是函数指针，指向 jni 函数；

其中，第二个参数 signature 使用字符串记录方法的参数与返回值，具体格式形如“()V”、“(II)V”，其中分为两部分，括号内表示的是参数，括号右侧表示的是返回值；