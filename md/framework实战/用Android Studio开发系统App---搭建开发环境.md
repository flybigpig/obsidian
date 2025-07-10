## 1. 编译 framework 模块

系统应用可以调用隐藏的API，这需要我们引入包含被隐藏 API 的 jar 包。

为了得到这个 jar 包，我们需要在源码下编译 Framework 模块：

```
source 
lunch rice14-eng
# Android10 及以前
make framework
# Android11 及以后
#make framework-minus-apex
```


编译完成后，我们在 `out/target/common/obj/JAVA_LIBRARIES/framework_intermediates` 目录下找到 `classes.jar` 文件，为方便识别，我们将该文件拷贝到其他地方，并将文件名修改为 `framework.jar` 。

## [#](http://ahaoframework.tech/002.%E7%8E%A9%E8%BD%ACAOSP%E7%AF%87/008.%E4%BD%BF%E7%94%A8%20Android%20Studio%20%E5%BC%80%E5%8F%91%E7%B3%BB%E7%BB%9F%20App.html#_2-%E5%88%9B%E5%BB%BA%E7%B3%BB%E7%BB%9F-app-%E9%A1%B9%E7%9B%AE)2. 创建系统 App 项目

使用 Android Studio 创建一个 Empty Activity 空项目。接着把之前准备好的 framework.jar 拷贝到项目的 `app/libs` 文件夹中。

接着修改项目根目录下的 build.gradle，添加如下内容：

Android studio 4.2 及以后：

```
allprojects{
    gradle.projectsEvaluated {
        tasks.withType(JavaCompile) {
            Set<File> fileSet = options.bootstrapClasspath.getFiles()
            List<File> newFileList = new ArrayList<>();
            newFileList.add(new File("./app/libs/framework.jar"))
            newFileList.addAll(fileSet)
            options.bootstrapClasspath = files(
                    newFileList.toArray()
            )
        }
    }
}
```

 

Android Studio 4.2 前包括Android Studio 3.x：

```
allprojects {
gradle.projectsEvaluated {
        tasks.withType(JavaCompile) {
        	//相对位置，根据存放的位置修改路径
            options.compilerArgs.add('-Xbootclasspath/p:app/libs/framework.jar')
        }
    }
}
```



app.iml 这个文件编译时是自动生成的，我们只需要动态调整 framwork.jar 的位置，将其位置放到其他 jar 包之前即可。但是重新编译后需要再次进行修改。

![](https://cdn.jsdelivr.net/gh/zzh0838/MyImages@main/img/20230803155955.png)

接着修改 app/build.gradle：

```
dependencies {
    compileOnly files('libs/framework.jar')
    //.......
}
```


然后在项目的 AndroidManifest.xml 中添加：

```
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:tools="http://schemas.android.com/tools"
    android:sharedUserId="android.uid.system">
```


接着我们需要制作系统签名，这里使用 [keytool-importkeypair (opens new window)](https://github.com/getfatday/keytool-importkeypair)签名工具。

将 [keytool-importkeypair (opens new window)](https://github.com/getfatday/keytool-importkeypair)clone 到本地，并将其中的 keytool-importkeypair 文件添加到 PATH 路径。

接着进入系统源码下的 `build/target/product/security` 路径，接着执行：

```
keytool-importkeypair -k ./platform.keystore -p android -pk8 platform.pk8 -cert platform.x509.pem -alias platform
```



k 表示要生成的签名文件的名字，这里命名为 platform.keystore -p 表示要生成的 keystore 的密码，这里是 android -pk8 表示要导入的 platform.pk8 文件 -cert 表示要导入的platform.x509.pem -alias 表示给生成的 platform.keystore 取一个别名，这是命名为 platform

接着，把生成的签名文件 platform.keystore 拷贝到 Android Studio 项目的 app 目录下，然后在 app/build.gradle 中添加签名的配置信息：

```
android {
    signingConfigs {
        sign {
            storeFile file('platform.keystore')
            storePassword 'android'
            keyAlias = 'platform'
            keyPassword 'android'
        }
    }

    buildTypes {
        release {
            minifyEnabled false
            proguardFiles getDefaultProguardFile('proguard-android-optimize.txt'), 'proguard-rules.pro'
            signingConfig signingConfigs.sign
        }

        debug {
            minifyEnabled false
            signingConfig signingConfigs.sign
        }
    }
}
```



至此，我们在 AS 中就搭建好了我们的系统 App

## [#](http://ahaoframework.tech/002.%E7%8E%A9%E8%BD%ACAOSP%E7%AF%87/008.%E4%BD%BF%E7%94%A8%20Android%20Studio%20%E5%BC%80%E5%8F%91%E7%B3%BB%E7%BB%9F%20App.html#_3-%E7%B3%BB%E7%BB%9F-app-%E7%9A%84%E7%BC%96%E8%AF%91%E8%BF%90%E8%A1%8C)3. 系统 APP 的编译运行

在开发过程中，大部分情况下，我们可以直接点击 Android Stuido 中的运行按钮来运行我们的配置好的 App。当我们的 App 开发完成，我们需要将其预制到系统中：

我们在系统源码下的 `device/Jelly/Rice14` 目录下，创建如下的文件与文件夹：

```
AsSystemApp
├── Android.mk
└── app.apk
```

1  
2  
3  

其中 app.apk 是我们用 Android Studio 打包好的 apk 安装包。Android.mk 的内容如下(Android10 下，Android.bp 貌似还不支持引入 apk，实测 Android13 是没问题的，这里就使用 Android.mk 了)：

```
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := AsSystemApp
LOCAL_CERTIFICATE := PRESIGNED
LOCAL_SRC_FILES := app.apk
LOCAL_MODULE_CLASS := APPS
LOCAL_PRODUCT_MODULE := true
include $(BUILD_PREBUILT)
```



接着修改 `device/Jelly/Rice14/Rice14.mk`

```
PRODUCT_PACKAGES += \
    AsSystemApp
```



然后编译系统，启动虚拟机，就可以看到我们的 App 了。

```
source build/envsetup.sh
lunch Rice14-eng
make -j16
emulator 
```


## [#](http://ahaoframework.tech/002.%E7%8E%A9%E8%BD%ACAOSP%E7%AF%87/008.%E4%BD%BF%E7%94%A8%20Android%20Studio%20%E5%BC%80%E5%8F%91%E7%B3%BB%E7%BB%9F%20App.html#_4-%E7%B3%BB%E7%BB%9F-app-%E7%89%B9%E7%82%B9)4. 系统 App 特点

### [#](http://ahaoframework.tech/002.%E7%8E%A9%E8%BD%ACAOSP%E7%AF%87/008.%E4%BD%BF%E7%94%A8%20Android%20Studio%20%E5%BC%80%E5%8F%91%E7%B3%BB%E7%BB%9F%20App.html#_4-1-%E7%B3%BB%E7%BB%9F-app-%E5%8F%AF%E4%BB%A5%E6%89%A7%E8%A1%8C%E4%B8%89%E6%96%B9-app-%E4%B8%8D%E8%83%BD%E6%89%A7%E8%A1%8C%E7%9A%84-api)4.1 系统 app 可以执行三方 App 不能执行的 Api

一些 API 只能又系统 App 调用，比如：

```
SystemClock.setCurrentTimeMillis(0)
```


如果在普通 App 中使用，就会报以下的错误：

```
E/SystemClock: Unable to set RTC
    java.lang.SecurityException: setTime: Neither user 10099 nor current process has android.permission.SET_TIME.
```


网络上很多文章教你在 App 如何调用 Hide 的 Api，在系统 App 中可以直接使用这些 Hide Api：

```
import android.os.SystemClock

class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        SystemClock.setCurrentTimeMillis(0)

        intent = Intent(this, TestService::class.java)
        startService(intent)

        //在系统 App 中可以正常使用
        //AudioSystem 整个类被标记为 hide
        Log.d("MainActivity", "" + AudioSystem.STREAM_ACCESSIBILITY)

    }
}
```


### [#](http://ahaoframework.tech/002.%E7%8E%A9%E8%BD%ACAOSP%E7%AF%87/008.%E4%BD%BF%E7%94%A8%20Android%20Studio%20%E5%BC%80%E5%8F%91%E7%B3%BB%E7%BB%9F%20App.html#_4-2-%E8%BF%9B%E7%A8%8B%E4%BF%9D%E6%B4%BB)4.2 进程保活

为了保活，开发者不知道加了多少班，掉了多少头发。在系统 App 这里，可以说保活不要太简单了：

在 AndroidManifest.xml 中添加：

```
 <application
        android:persistent="true"
```

1  
2  

添加一个 Service：

```
class TestService : Service() {

    override fun onBind(p0: Intent?): IBinder? {
        return null
    }

    override fun onCreate() {
        super.onCreate()
        Thread() {
            while (true) {
                Log.d("TestService", "this is TestService")
                Thread.sleep(2000)
            }
        }.start()
    }
}
```



在 MainActivity 中启动 Service：

```
intent = Intent(this, TestService::class.java)
startService(intent)
```

1  
2  

最后不要忘了在 AndroidManifest.xml 中添加 Service 的声明：

```
        <service
            android:name=".TestService"
            android:enabled="true"
            android:exported="true">
        </service>
```


最后直接点击运行按钮启动我们的 App，可以看到 Service 一直再打 Log。

我们使用 `ps -ef | grep "AsSystemApp"` 查找到我们 App 对应的 pid，然后使用 kill 命令将其强杀掉，经过短暂的等待后，Log 窗口又开始打 Log 了，说明我们的 App 在强杀后，被系统重新拉起运行。

## [#](http://ahaoframework.tech/002.%E7%8E%A9%E8%BD%ACAOSP%E7%AF%87/008.%E4%BD%BF%E7%94%A8%20Android%20Studio%20%E5%BC%80%E5%8F%91%E7%B3%BB%E7%BB%9F%20App.html#_4-3-%E7%B3%BB%E7%BB%9F-app-%E6%9D%83%E9%99%90)4.3 系统 App 权限

相比三方 App，系统 App 可以使用更多的权限。因为这部分内容涉及较多系统权限相关的基础知识。我们就在系统权限部分再来讲解系统 App 权限相关的内容吧。