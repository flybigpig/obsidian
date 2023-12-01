Android Studio 进行android开发，但是编译的耗时又是很头疼的问题。所以决定拉取源代码编译一下他，研究下如何提升性能。过程很坎坷。 先贴下效果图：

![WX20220311-173820@2x.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/2fa4b8eff0044b1883c6a8af6d023548~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/9c6b1c3de9164e729180a07da7ac86ea~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

但是我写这个的目的 不光记录下编译的过程，也想分享给需要的人，可以少走坑。所以我也从头来吧：

### 1.Clone代码，这个还是比较简单的

官方教程: [source.android.com/source/down…](https://link.juejin.cn/?target=https%3A%2F%2Fsource.android.com%2Fsource%2Fdownloading%3Fhl%3Dzh-cn "https://source.android.com/source/downloading?hl=zh-cn")

1.首先我们需要创建目录。 **mac同学注意了，代码一定要存储在大小写敏感的磁盘里。** 首先我们创建自己的目录:

```
mkdir android-studio-master 
cd android-studio-master
```

2.获取代码 repo init -u [android.googlesource.com/platform/ma…](https://link.juejin.cn/?target=https%3A%2F%2Fandroid.googlesource.com%2Fplatform%2Fmanifest "https://android.googlesource.com/platform/manifest") -b 分支 我们这里编译的是2021.1.1版本 所以分支我们就填入studio-2021.1.1

```
repo init -u https://android.googlesource.com/platform/manifest -b studio-2021.1.1
```

关于版本管理可以参考[Android Studio版本说明](https://link.juejin.cn/?target=https%3A%2F%2Fdeveloper.android.google.cn%2Fstudio%2Freleases%2Findex.html%23bumblebee "https://developer.android.google.cn/studio/releases/index.html#bumblebee"),当前的最新版本就是2021.1.1。 ![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/1bedc3a745bb44ed8024d5d1a6507062~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

我们就等待他同步相关的仓库信息。

关于repo的安装 可以看官方的文档，这里我就贴出对应的命令。

```
mkdir ~/bin
PATH=~/bin:$PATH
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
```

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/c9de0c9b9d7d463d9a7362f81be89647~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

当看到输出initialized 就表示同步成功。同步完成以后运行`repo sync -j4`，进行代码的拉取。时间比较久，我们就让他同步着吧。

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ad572aea9de442c488a2894678f5bf06~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

我也是新拉取的，所以先写到这里，等代码拉取成功再更新。

### 2.项目介绍

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/fefbf3d3d59c418aa6b2f60dad8fc849~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?) 当看到这个就说明代码拉取成功了，接下来就要导入项目了。 这里我就使用IntelliJ IDEA 2021.3.2版本了。 ![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/b5c03557a33344b98a66030341a6a05e~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

介绍下我们需要用到的目录: tools/base:这个库是Android Studio的公共库，构建系统gradle plugin和sdk组件。 包括：

1.  Instant-run
2.  SDKManager
3.  Manifest Merger
4.  以及一些公共库、测试库 和性能分析工具profiler。

tools/base/adt/idea:这个库是IntelliJ平台的Android插件源代码，也是Android Studio最核心的部分。 包括：

1.  Android light classes
2.  Assistant
3.  Color picker
4.  Kotlin light-class
5.  Layout editor placeholder
6.  Logging
7.  Resources system等等。 我们主要导入tools/base/adt/idea。 导入后的界面：

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d0aba061ecc24232bafec3dbd22a415b~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?) 从右上角的**Configurations**中下拉选中Android Studio 不过这个时候是报错的，是因为我们缺少了依赖环境。

### 3.依赖环境

1.  JDK: 我们不需要重新下载JDK，在我们拉取的项目中已经有了JDK。 `prebuilts/studio/jdk/mac`默认目录。需要在项目中导入该JDK环境。

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ef45069352084e7c891e685986cc99c1~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?) 设置JDK后，我们还需要设置插件的依赖库，也就是tools/base库的依赖。 2. 公共库依赖

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/17cadb42bc914a50b6ff9ff74d5728ef~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?) 缺了这么多库，不用慌张。我们直接构建一次Android Studio就会生成这些库了。 在tools/idea目录 运行build\_studio.sh 脚本就会为我们构建Android Studio了。

```
$ cd tools/idea 
$ ./build_studio.sh
```

中间会出现很多问题，我们一步一步来解决。

开始：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/a7e02a33d4ce46d9b46e40a992517127~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/7264e3c7d750463192611831617f7b23~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?) 🤣刚开始就报错了。 我们来看看错误

`[java] [gant] The following modules from productProperties.productLayout.bundledPluginModules aren't found in the project: [intellij.c.clangd, intellij.c.plugin, intellij.cidr.debugger.plugin, intellij.cidr.base.plugin]`

这是因为CIDR插件源码中是没有的，这些库源码中是没有的，所以我们需要注掉。具体的代码在构建Android Studio的groovy脚本中。

```
/Volumes/android_studio/android-studio-master/tools/idea/build/groovy/org/jetbrains/intellij/build/AndroidStudioProperties.groovy
```

我们注掉

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8fc71ef9619145e8b867104887b4ad83~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/8c6f8f4f987f444296ad15cda3542909~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

搞定，我们继续。重新运行build\_studio.sh 构建成功：

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/dda32280bfb748ad988304077552daf7~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

```
tools/idea/out/studio/dist/android-studio-211.7628.21.2111.__BUILD_NUMBER__.mac.zip
```

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/65edc16549de44b2b9e4ca8674888cbe~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?) 这就是编译出来的Android Studio了。

编译出来的Android Studio是不可以直接运行的，我们导入项目依赖:

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/93765a46d27b450abc09790d91c61416~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?) 还有报红的库，是因为我们还缺少android artifacts。需要使用bazel构建artifacts。

```
cd /tools/base/bazel
chmod u+x ./bazel
./bazel verion
```

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/51b366f36ad7479ba012b0e6e2d5e5eb~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

```
./bazel build //tools/adt/idea/android:artifacts
```

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3232100c26394e60ae6dcf72bb610117~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

🤣🤣🤣刚开始又挂了，是因为tools/vendor/google3/bazel 是内部使用的，我们没有，官方也做了处理，我们注掉就可以，不影响构建。 文件就在**toplevel.WORKSPACE**。

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/4e0e358af968468b82b50f1d52cc50b2~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

修改内容如下： ![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/67482ae392d04fadbc18170c337fa622~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

继续 **./bazel build //tools/adt/idea/android:artifacts**。

又挂了 ![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/fc1e48e787ca4120ab5b68776da89cfc~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?) 缺少了BUILD文件 这个需要我们配置BUIlD。内容如下：

```

package(default_visibility = ["//visibility:public"])

java_import(
    name = "studio-sdk",
    jars = glob(["AI-211/iehshx/lib/*.jar","AI-211/iehshx/plugins/java/lib/*.jar"],
                ["AI-211/iehshx/lib/annotations-java5.jar"]),
)

java_import(
    name = "studio-sdk-plugin-Groovy",
    jars = glob(["AI-211/iehshx/plugins/Groovy/lib/*.jar"]),
)

java_import(
    name = "studio-sdk-plugin-Kotlin",
    jars = glob(["AI-211/iehshx/plugins/Kotlin/lib/*.jar"]),
)

java_import(
    name = "studio-sdk-plugin-git4idea",
    jars = glob(["AI-211/iehshx/plugins/git4idea/lib/*.jar"]),
)

java_import(
    name = "studio-sdk-plugin-webp",
    jars = glob(["AI-211/iehshx/plugins/webp/lib/*.jar"]),
)

java_import(
    name = "studio-sdk-plugin-junit",
    jars = glob(["AI-211/iehshx/plugins/junit/lib/*.jar"]),
)

java_import(
    name = "studio-sdk-plugin-properties",
    jars = glob(["AI-211/iehshx/plugins/properties/lib/*.jar"]),
)

java_import(
    name = "studio-sdk-plugin-gradle",
    jars = glob(["AI-211/iehshx/plugins/gradle/lib/*.jar"]),
)

java_import(
    name = "studio-sdk-plugin-gradle-java",
    jars = glob(["AI-211/iehshx/plugins/gradle-java/lib/*.jar"]),
)

java_import(
    name = "studio-sdk-plugin-platform-images",
    jars = glob(["AI-211/iehshx/plugins/platform-images/lib/*.jar"]),
)

java_import(
    name = "studio-sdk-plugin-IntelliLang",
    jars = glob(["AI-211/iehshx/plugins/IntelliLang/lib/*.jar"]),
)
```

继续... 挂

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d5ba77baabc942088f277e633e60e6aa~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

这是少配置了ndk的环境，需要配置下， 把ndk放在/Volumes/android\_studio/android-studio-master/prebuilts/studio/sdk/darwin/ndk/。 默认使用的是20.1.5948944，推荐我们使用默认版本，在 _**/tools/base/bazl/repositories.bzl**_中配置ndk目录。

继续...

![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/19529a81021b45488d062ff33805f43f~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?) 挂 这是因为缺少jvmti.h头文件，我们把他下载到 **tools/vendor/google/android-ndk/includes**目录下。

```
 curl https://android.googlesource.com/platform/art/+/master/openjdkjvmti/include/jvmti.h\?format\=TEXT | base64 -d >jvmti.h

```

![image.png](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/ca6cdffb1e5d4241abfe0b8ba7858e70~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

创建BUILD: tools/vendor/google/android-ndk/BUILD文件 内容如下:

```
package(default_visibility = ["//visibility:public"])

cc_library(
    name = "jvmti",
    includes = ["includes"],
)

```

继续build...

![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/04200c4a4024426f94dfcf24f146e38a~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

挂： ![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d8cdab88efe74bedb6907432e3e5d848~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?) 缺少android.jar 我们补充到prebuilts/studio/sdk:darwin/sdk 把platfroms拷贝进来，或者编辑指定目录 **prebuilts/studio/sdk/BUILD**

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/98b8ce15e8e44f9b89cdc65a45b11fb9~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

这里我就选择了拷贝进来

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/95be2c5c5fa24158a24b1c38d12b369f~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?) 这样环境就齐全了。

### 4.运行

右上角下拉框选择AndroidStudio，点击哪个有点绿的播放按钮。开始~ ![image.png](https://p1-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/d708f001c6ef44c9ae256c89fa8f5175~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

成功~~~ ![image.png](https://p9-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/3f9bd5c8398a43b0b463bb83612a87e5~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

附上已经编译好的:

链接: [pan.baidu.com/s/1Na\_Ws7yp…](https://link.juejin.cn/?target=https%3A%2F%2Fpan.baidu.com%2Fs%2F1Na_Ws7yptBBaZr5-2E0dwA "https://pan.baidu.com/s/1Na_Ws7yptBBaZr5-2E0dwA")

提取码: r9yx

欢迎大家一起讨论

![image.png](https://p3-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/f471522ab6be4941aa58670ca2a2fea9~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)