## 自己动手调试Android源码(超简单)

[![](https://upload.jianshu.io/users/upload_avatars/142377/1477d180b32d.png?imageMogr2/auto-orient/strip|imageView2/1/w/100/h/100/format/webp)](https://www.jianshu.com/u/22f55ea83811)

12016.07.04 00:13:10字数 1,689阅读 17,020

在[自己动手编译Android最新源码](https://link.jianshu.com/?t=http://blog.csdn.net/dd864140130/article/details/51718187)一文中,我们为自己编译了一份最新的Android源码.很多时候,我们编译源码的目的不仅仅是尝试一番,而是希望对其进行调试,并修改源码,看看其中一些关键机制的运行原理.比如你对AMS掌握不深,那么就来单独的调试一下;又或者是说你想看看launcher的实现原理,同样也可以自己动手调试.更或者说,你觉得某个模块很不理想,想自己修改一番,那就更好了.

下面,我们来说说如何调试源码.同样这里的工作平台还是ubuntu 16.04和Android Studio.另外,本文参考源码中developent/tools/idegen/README文档.

___

## 基础准备

源码编译完整之后,我们就可以导入源码到Android Studio中进行调试了.那么如何调试呢?  
在源码中,存在idegen模块,该模块专门用来为idea工具生成系统源码的project.

在开始编译该模块之前,首先确保你已经编译过Android源码了,如果没有,可以参考上篇文章进行编译.  
和编译普通的模块一样,我们用mmm命令编译idegen.在开始编译之前,检查`out/host/linux-x86/framework/`目录下是否存在idegen.jar文件,存在则说明你已经编译过该模块,否者,则需要编译.执行如下命令即可:

```undefined
soruce build/envsetup.sh
mmm development/tools/idegen/
sudo ./development/tools/idegen/idegen.sh
```

其中mmm development/tools/idegen/执行完成后会生成idegen.jar,而sodo ./development/tools/idegen/idegen.sh则会在源码目录下生成IEDA工程配置文件:android.ipr,android.iml及android.iws.

简单的说明一下这三个文件的作用:

android.ipr:通常是保存工程相关的设置,比如编译器配置,入口,相关的libraries等  
android.iml:则是主要是描述了modules,比如modules的路径,依赖关系等.  
android.iws:则主要是包含了一些个人工作区的设置.

到目前为止,我们就完成了源码准备工作.

___

## 源码导入

## 2.1 修改AS配置文件

编译成功后,现在我们就可以将源码导入Android Studio了.但是在导入之前,我们先修改一下Android studio的配置:32位系统下修改idea.vmoptions,64位下修改idea64.vmotions  
调整其中的-Xms和-Xmx参数值,官方要求至少在748m以上,根据实际情况进行配置即可.  
然后进入android-studio目录下的bin文件夹,执行如下命令启动Android Studio

```undefined
./studio.sh
```

## 2.2 导入源码

接下来,我们导入源码:打开Android Studio,点击File->Open,选择刚才生成的android.ipr文件即可,然后就是漫长的等待,注意此时是将源码完全导入到AS中了,不出意外,你会觉得AS运行非常之慢.那么该如何做呢?继续往下看吧.

很多情况下,我们希望不导入某些模块,那么就可以在导入前修改android.iml文件,通过添加配置的方式告诉AS不导入某些模块,比如现在我不想导入art模块,那么就在android.iml文件中添加:

```bash
<excludeFloder url="file://$MODULE_DIR$"/abi>
```

不难发现,其格式为:`<excludeFloder url="file://$MODULE_DIR$"/模块名>`  
注:编译生成的android.iml文件中已经默认排除了一下模块,通过搜索excludeFolder关键字可找到.

我这里只保留了framworks和packages模块,将其他模块全部排除了,因此在android.iml中添加了以下配置:

```bash
<excludeFolder url="file://$MODULE_DIR$/.repo" />
<excludeFolder url="file://$MODULE_DIR$/abi" />
<excludeFolder url="file://$MODULE_DIR$/art" />
<excludeFolder url="file://$MODULE_DIR$/bionic" />
<excludeFolder url="file://$MODULE_DIR$/bootable" />
<excludeFolder url="file://$MODULE_DIR$/build" />
<excludeFolder url="file://$MODULE_DIR$/cts" />
<excludeFolder url="file://$MODULE_DIR$/dalvik" />
<excludeFolder url="file://$MODULE_DIR$/developers" />
<excludeFolder url="file://$MODULE_DIR$/development" />
<excludeFolder url="file://$MODULE_DIR$/device" />
<excludeFolder url="file://$MODULE_DIR$/docs" />
<excludeFolder url="file://$MODULE_DIR$/external" />
<excludeFolder url="file://$MODULE_DIR$/hardware" />
<excludeFolder url="file://$MODULE_DIR$/libcore" />
<excludeFolder url="file://$MODULE_DIR$/libnativehelper" />
<excludeFolder url="file://$MODULE_DIR$/ndk" />
<excludeFolder url="file://$MODULE_DIR$/out" />
<excludeFolder url="file://$MODULE_DIR$/pdk" />
<excludeFolder url="file://$MODULE_DIR$/prebuilt" />
<excludeFolder url="file://$MODULE_DIR$/prebuilts" />
<excludeFolder url="file://$MODULE_DIR$/sdk" />
<excludeFolder url="file://$MODULE_DIR$/system" />
<excludeFolder url="file://$MODULE_DIR$/tools" />
```

此时导入AS后的结果如下所示:

  

![](https://upload-images.jianshu.io/upload_images/142377-79bd9765a1c662c7?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

这里写图片描述

如果已经将全部项目导入到AS中,而又想排除一些模块该怎么办呢?  
此时可以在Project Scureture的Mobules中进行排除.比如这里我想排除art模块,那么做法如下图:

  

![](https://upload-images.jianshu.io/upload_images/142377-0fdff7260158c66a?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

这里写图片描述

## 2.3 源码查看

导入的过程是很漫长滴.导入完成之后,现在我们就可以在android studio中查看源码,如图:

  

![](https://upload-images.jianshu.io/upload_images/142377-b24cfc9a79267fdd?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

这里写图片描述

## 2.4 解决源码跳转错误问题

为了编码Android源码跳转错误问题,还需要做以下两点:配置SDK,JDK及修改依赖,具体操作如下:

### 2.4.1 配置SDK和JDK

我们需要为当前项目配置JDK和SDK:点击Project Structure,进入到项目配置界面,在SDKs设置中加入必须的JDK和SDK:

  

![](https://upload-images.jianshu.io/upload_images/142377-ffc10e9a8ebb05e8?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

这里写图片描述

为了确保使用的是Android源码库中的文件,我们将新添加的这个JDK的Classpath中的内容全部删掉,也就是需要删掉Classpath标签页下的所有jar包,然后在下面Android API 24 platform中指定使用刚才新增的JDK,最后在右侧选中Project标签,在Project SDK中选择对应的Android API版本.

### 2.4.2 修改依赖

同样打开Projects Stucture,选择Modules.首先将所有的依赖删除(为了方便,后边如果缺少,可以自行添加进来)

  

![](https://upload-images.jianshu.io/upload_images/142377-696ed5022271ac8e?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

这里写图片描述

删除完成之后(保留下图所示的两项),并点击右边的"+"添加Frameworks和external目录,结果如下图所示:

  

![](https://upload-images.jianshu.io/upload_images/142377-b770c06e055a6253?imageMogr2/auto-orient/strip|imageView2/2/w/1148/format/webp)

这里写图片描述

到现在为止,你就可以正常的查看源码,并在源码间进行跳转了.

___

## 源码调试

搞定上面之后，现在我们来看看如何用Android Studio一步一步调试代码.  
首先为刚才导入的工程添加Framework,以便让AS将它作为一个Android工程,从而能让我们进行调试,如果项目已经是Android工程了(目前最新的android源码导入到as中就是作为一个android工程,因此不需要在做这一步了)则不需要再次进行添加了.

在Project Structure中的Mouble中,为其添加Framework,如下图所示:

  

![](https://upload-images.jianshu.io/upload_images/142377-a0c2ef610c64b789?imageMogr2/auto-orient/strip|imageView2/2/w/1000/format/webp)

这里写图片描述

接下来就可以用debugger进行调试跟踪代码了.首先使用emulator命令启动我们的虚拟机.接下来选择Attache debugger to Android process,在弹出的Choose Process框内必须选择Show all processes,否则看不到相关的进程:

  

![](https://upload-images.jianshu.io/upload_images/142377-a178b9f4719a5ffc?imageMogr2/auto-orient/strip|imageView2/2/w/1192/format/webp)

这里写图片描述

## 调试演示

这里我一调试com.android.settings模块为例进行说明.  
在SettingActivity中下断点,如图:

  

![](https://upload-images.jianshu.io/upload_images/142377-e4135bbdc75a1a12?imageMogr2/auto-orient/strip|imageView2/2/w/1039/format/webp)

这里写图片描述

选择com.android.settings

  

![](https://upload-images.jianshu.io/upload_images/142377-a860165c220be87a?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

这里写图片描述

做完这些后,我们点击android 虚拟机中的设置,随后就可以一步一步调试了

  

这里写图片描述

___

## 总结

要想调试源码,不难发现,一共需要以下几个步骤:

1.  编译好的源码
2.  使用idegen模块生成必要的项目工程文件
3.  导入源码到AS中  
    现在呢,你可以放心的去调试了,在这期间出现问题可自行Google解决,一般都能找到答案.

最后编辑于

：2017.12.03 07:37:43

©

著作权归作者所有,转载或内容合作请联系作者  
【社区内容提示】社区部分内容疑似由AI辅助生成，浏览时请结合常识与多方信息审慎甄别。  
平台声明：文章内容（如有图片或视频亦包括在内）由作者上传并发布，文章内容仅代表作者本人观点，简书系信息发布平台，仅提供信息存储服务。

53人点赞

更多精彩内容，就在简书APP

"江湖人称小白哥，打赏我会生气的,不信你试试?"

![  ](https://cdn2.jianshu.io/assets/default_avatar/alipay-c192a7455bcf21ab8dd23d96679fc1c0.png)共1人赞赏

[![  ](https://upload.jianshu.io/users/upload_avatars/142377/1477d180b32d.png?imageMogr2/auto-orient/strip|imageView2/1/w/100/h/100/format/webp)](https://www.jianshu.com/u/22f55ea83811)

总资产446共写了13.5W字获得2,233个赞共1,394个粉丝

### 被以下专题收入，发现更多相似内容

### 相关阅读[更多精彩内容](https://www.jianshu.com/)

-   Android 自定义View的各种姿势1 Activity的显示之ViewRootImpl详解 Activity...
    
-   通过上一篇 Android FrameWork学习（一）Android 7.0系统源码下载\\编译 我们了解了如何进...
    
    [![](https://upload.jianshu.io/users/upload_avatars/1727036/93cc1578cd11?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)香辣牛肉面](https://www.jianshu.com/u/b0ca326ae9e2)阅读 12,539评论 10赞 58
    
-   妳，愿不愿意给我一个机会？ 一个打开妳心门的机会。 // 「玟星啊什么时候会更新～」 「星星什么时候才会出实体书呢...
    

-   本文来源罗辑思维60秒语音，由等风来【微信460579084】编辑整理，略有删改。 最近，我们在张罗Papi酱视频...
    
    [![](https://upload.jianshu.io/users/upload_avatars/1492524/c727f970882d?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)杨心武](https://www.jianshu.com/u/4dea53a0ead4)阅读 582评论 0赞 0