---
created: 2025-04-24T09:57:21 (UTC +08:00)
tags: [picgo+typora+gitee]
source: https://blog.csdn.net/qq_38697767/article/details/147120124
author: 成就一亿技术人!
---

# （保姆级教程） Typora+PicGo+gitee搭建博客写作环境（超详细） 同步解决：PicGo插件安装失败问题_picgo+typora+gitee-CSDN博客

> ## Excerpt
> 文章浏览阅读551次，点赞23次，收藏26次。本文主要是讲解了PicGo+gitee+typora搭建个人博客创作环境的文章。同时解决了PicGo 2.3.1插件无法下载的问题_picgo+typora+gitee

---
在我们的博客写作中，通常有这样的需求：

使用Markdown编辑博客

-   一篇博客发布多个平台
-   图片一次上传，处处引用

为了方便写作，我们可以采用**Typora+PicGo+Gitee**的方案来搭建我们的博客写作环境。

-   Typora：它是一款轻便简洁的Markdown编辑器，支持即时渲染技术，即所写立刻所见，少了排版的时间，专注于文章内容的编辑。  
    （https://typoraio.cn/）
-   PicGo:它是一个用于快速上传图片并获取图片 URL 链接的工具，支持多种图库。（https://picgo.github.io/PicGo-Doc/zh/）
-   Gitee:目前中国最大的代码托管的工具，除了代码，还可以用作图片存储。（快，比github快）

## **一：软件安装**

### 1.1 typora

[Typora下载](https://so.csdn.net/so/search?q=Typora%E4%B8%8B%E8%BD%BD&spm=1001.2101.3001.7020)地址：[typora官网](https://typoraio.cn/)，下载对应的版本，一步步安装即可：  
![ ](https://i-blog.csdnimg.cn/direct/443ce068b3a743178ebb411d7b695f7a.png)

### 1.2 [PicGO](https://so.csdn.net/so/search?q=PicGO&spm=1001.2101.3001.7020)

PicGO下载地址：[PicGO官网](https://picgo.github.io/PicGo-Doc/zh/)，找到官方正式版（本文采用了2.3.1版本）  
![官网地址下载](https://i-blog.csdnimg.cn/direct/4ac3c9eede27485da8bef449abd52ded.png)  
找到对应系统进行下载  
![山东大学资源站](https://i-blog.csdnimg.cn/direct/89b2b1f0c14947b8a534ed90c239c637.png)  
下载好的安装文件，选择安装路径，然后一直点击下一步就可以（**建议安装在非系统盘，我就是系统被重置，导致了这篇文诞生！**）  
![安装完成](https://i-blog.csdnimg.cn/direct/55878bb5c9214ae78caaab1e2e53dac7.png)

## 二：配置图床

### 2.1 创建gitee仓库

登录[gitee](https://gitee.com/)并创建一个**公开的仓库**，需要注意仓库必须是**公开**的，否则无法预览。

点击右上角头像旁边的➕

![ ](https://i-blog.csdnimg.cn/direct/40bc92351e64496fa09472ca1e1283c5.png)  
这里要设置成开源，勾选设置模板，单分支。  
![](https://i-blog.csdnimg.cn/direct/ad7f978241084174804b8b4577345a03.png)

### 2.2 设置私人令牌

点击右上角头像-设置，找到 **安全设置**下 **私人令牌** ，然后点击**生成新令牌**，权限选择**project**即可。  
![ ](https://i-blog.csdnimg.cn/direct/1b175cf998c24305b137a70fd5e5f161.png)

![ ](https://i-blog.csdnimg.cn/direct/278fcd8d81e348ff8e1def76b4cae798.png)  
填写好之后点 **提交** ，就会生成私人令牌  
![ ](https://i-blog.csdnimg.cn/direct/f20b6590e411483bb5c11fb58561a36f.png)  
![ ](https://i-blog.csdnimg.cn/direct/c81958e643a14746ad18c8e2f21b52f3.png)  
这里私人令牌信息要复制保存好，后面设置图床的时候要使用。

### 2.3 安装gitee-uploader 1.1.2 插件

打开PicGo，在 **插件设置** 中查找gitee，安装gitee-uploader  
![正常效果图](https://i-blog.csdnimg.cn/direct/f823e0bc0c2548948bacc8c1efe17e4b.png)  
如果这里如上图所示，那就直接点击安装即可。后续请直接继续查看 2.4PicGo配置Gitee图床

但是**PicGo 2.3.1**版本或者新的**PicGO2.4Beta8** 版本，我尝试都不会出现插件查询结果，如下图  
![ ](https://i-blog.csdnimg.cn/direct/ad98bde15f614826a8d45b7db2a79495.png)  
如果出现如上错误情况，请继续往下看。  
![ ](https://i-blog.csdnimg.cn/direct/ca63d1370ed34d03aec26a6fa8f32303.png)  
这里我验证了一下，[官方插件库](https://github.com/Molunerfinn/PicGo)肯定是存在gitee图床支持插件的

我这里采用了**npm 本地安装插件然后导入PicGo**的做法，欢迎小伙伴一起交流其他解决方式！

第一步先检测环境

```
node -v #显示node.js版本
npm -v  #显示npm版本
```

正常显示：  
![npm环境校验](https://i-blog.csdnimg.cn/direct/5d79968c26424db482913223359844c0.png)  
如果出现npm不是系统命令的情况，请直接下载安装[Node.js](https://nodejs.org/zh-cn)环境，下载安装包直接一路点击下一步。  
![node.js官网](https://i-blog.csdnimg.cn/direct/ddd93660cbdc411291fc9dc4da2f08b4.png)  
第二步 找到PicGo的安装路径，打开控制台，采用npm命令安装  
![picgo安装路径](https://i-blog.csdnimg.cn/direct/aa814295d64a4563adf6a06d2520dda8.png)

安装命令如下：

```
 npm install picgo-plugin-gitee-uploader
```

在上步插件安装完成后，目录(D:\\PicGo\\node\_modules)下会出现一个文件夹node\_modules，点开发现里面有了刚安装的插件  
![插件本地地址](https://i-blog.csdnimg.cn/direct/4aba14d9f77543699398179defae5b2f.png)  
第三步导入插件到PicGo，这里打开PicGo主窗口，点击插件设置，点击右上角⬇️，在新打开的窗口下找到PicGO安装的地方的node\_module文件夹下，找到uploader插件导入，然后重启PicGo即可。  
![导入插件图](https://i-blog.csdnimg.cn/direct/b5b7f2cc42034847b4c8b95cbb97bf4d.png)  
正常导入后的结果如下图  
![ ](https://i-blog.csdnimg.cn/direct/d91ad27f1f2f4f268166c5ddcc202615.png)

到这里之后就可以配置图床了

### 2.4. PicGo配置Gitee图床

打开PicGo主窗口，点击PicGo设置，找到最下面的Gitee，选择  
![](https://i-blog.csdnimg.cn/direct/d8fba15fb7604f9a9f72ee1d4e2d84db.png)  
然后就可以在侧边菜单上找到gitee了。  
![](https://i-blog.csdnimg.cn/direct/298adfb331d341e088c2aff864b5c74d.png)  
配置参数说明：

repo：仓库路径。填写gitee的 **账户名/仓库名**,如下图  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/fe66b9dc227c466c956b2755d114c163.png)

branch 分支，这里默认填写**master**即可，（这里是之前创建仓库选择单分支的原因）  
token：写入前面生成的**私人令牌**  
path 路径，仓库创建的文件夹路径，我这里写的是**img**，可不填写。  
customPath 提交消息。在提交到码云后，会显示提交消息，插件默认提交的是 Upload 图片名 by picGo - 时间，一般选择**default**  
customUrl 本地图库，可不填写

填写完如下图：  
![](https://i-blog.csdnimg.cn/direct/74866db6c346497491cfe13865725d01.png)  
上传区验证上传图片，在相册中能找到说明配置成功啦~~  
![成功](https://i-blog.csdnimg.cn/direct/2ec818afd0594fc48011100e63e2cc3c.png)

## 三：配置typora图床默认为PicGo

首先，请确认你已经搭建好了PicGo图床。然后，打开Typora设置。点击左侧的\*\*「图像」**，在「上传服务设定」处选择**PicGo.app\*\*，如下图箭头所示。  
在PicGo路径选择，之前PicGo的安装路径。

![](https://i-blog.csdnimg.cn/direct/f243dfabfe094adaac765854af7a1093.png)

> 注意：对于【对网络位置的图片应用上述规则】可选可不选，选中就是图片也会复制到本地。

配置成功后，点击验证图片上传选项。在picGo相册中发现如图所示的图片，证明配置成功  
![](https://i-blog.csdnimg.cn/direct/6c96a49368494774a9ff70029d1529e7.png)

到此便完成了配置，非常简单。当你用Typora进行写作时，从本地（或者通过截图）复制任意一张图片，然后粘贴到Typora中。此时，该图片会自动通过PicGo上传到云端，并形成一个图床链接加载到Typora中。

**欢迎各位大佬评论区进行技术交流**

> 注：本文中用到的所有截图都是采用[Snipaste](https://zh.snipaste.com/)完成的截图操作。
