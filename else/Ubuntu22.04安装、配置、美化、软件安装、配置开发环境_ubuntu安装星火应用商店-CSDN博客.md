## Ubuntu22.04安装、配置、美化、软件安装、配置开发环境

___

### 一、Ubuntu、Windows11（10）[双系统](https://so.csdn.net/so/search?q=%E5%8F%8C%E7%B3%BB%E7%BB%9F&spm=1001.2101.3001.7020)安装

> 因为ubuntu的安装网上的教程特别多了，所以这里不做赘述，推荐使用小破站这个up主的教程：[Windows 和 Ubuntu 双系统从安装到卸载\_哔哩哔哩\_bilibili](https://www.bilibili.com/video/BV1554y1n7zv?spm_id_from=333.337.search-card.all.click&vd_source=98dafe5e7469d9aa36a78da9510c266c "Windows 和 Ubuntu 双系统从安装到卸载_哔哩哔哩_bilibili")

### 二、安装后的配置

### 1.更换镜像源

> （1）打开软件和更新
> 
> ![](https://img-blog.csdnimg.cn/41c675b46b0a4aca95320402978c0078.png)
> 
> （2）在 “**下载自**” 中选择 “**其他**”  选中**中科大镜像源**（也可选择[阿里源](https://so.csdn.net/so/search?q=%E9%98%BF%E9%87%8C%E6%BA%90&spm=1001.2101.3001.7020)，清华源，这里以中科大源为例），点击 “**选择服务器**”，
> 
> ![](https://img-blog.csdnimg.cn/acef545c3c584e5aaf2ba384eb770960.png)
> 
> （3）输入密码后，点击右下角 “**关闭**”
> 
> ![](https://img-blog.csdnimg.cn/f03dd486b58e4ce390eeb7b0bcc32545.png)
> 
> （3）在弹出的弹框中点击 “**重新载入**” ，等待载入完成即可
> 
> ![](https://img-blog.csdnimg.cn/9a36484f89564ab2a062467c2f818587.png)

### 2.系统更新和语言支持

> （1）打开 “**软件更新器**”
> 
> ![](https://img-blog.csdnimg.cn/93a9d86e1dcb48b7bbe9c0532c6a1cb0.png)
> 
> （2）点击 “**立即安装**”
> 
> ![](https://img-blog.csdnimg.cn/fb602d84aa384fe197f7275cd1314903.png)
> 
> （3）更新完成后，选择 “**现在重启**”
> 
> ![](https://img-blog.csdnimg.cn/943bae91d0834a00b60d7adf8fa04c7a.png)
> 
> （4）打开**语言支持**
> 
> ![](https://img-blog.csdnimg.cn/ba65abc70f5f491cb97aaaef546e3696.png)
> 
> （5）在弹出的弹框中，选择**安装**，等待安装完成，注销或重启计算机
> 
> ![](https://img-blog.csdnimg.cn/93d8ef1963254156aed2a693d577188d.png)

### 3.设置双系统时间同步

> （1）打开终端，下载时间同步软件：**sudo apt install ntpdate**
> 
> ![](https://img-blog.csdnimg.cn/d8c9ab35af7d4e8c9e7ca84dffb62deb.png)
> 
> （2）使用下载的软件同步时间： **sudo ntpdate time.windows.com**
> 
> 出现图中红色标注的提示才说明同步成功，若不成功，重复执行上述代码即可
> 
> ![](https://img-blog.csdnimg.cn/3405b8b8d7f84824943e641890ce8540.png)
> 
> （3）修改ubuntu时间同步机制改为从主板同步：**sudo hwclock --localtime --systohc**
> 
> ![](https://img-blog.csdnimg.cn/3b58fd301077487a89a87c75b706b8b5.png)

### 4.安装显卡驱动

> （1）打开 “**附加驱动**”
> 
> ![](https://img-blog.csdnimg.cn/7447ed1cc4c141988be2290729bba206.png)
> 
> （2）选择闭源驱动，在这片文章发布时，最新的驱动为**515驱动**，但我不推荐使用**515驱动**            或**470驱动**，不知道为什么我安装了**515驱动**后，白色页面会微微偏黄，而**470驱动**安              装有一丢丢的麻烦；所以这里推荐使用**510驱动。（来自22年12月的补充：该文章第              一次编写于22年7月，但后面需要完成毕业设计拖延了，12月显卡已经是525了，我的            实体机安装22.04是好的，但22.04.1我尝试了好几个显卡驱动的版本都会在应用最小              化时卡一下屏，可能是我的电脑原因，后来转kubuntu了，所以大家可以装525驱动              试一下，如果出问题，重新换一个版本即可）**
> 
> （3）验证驱动是否安装成功

### 5.系统调优

> 因为ubuntu的系统优化已经相当不错了，所以这里我们只做一项优化，即交换空间的优化，ubuntu默认是当内存使用达到\*\*40%**就向交换空间写入数据，这样会大大的降低电脑运行效率，我们将它改为**10%\*\*再向交换空间写入数据
> 
> （1）打开终端，输入命令，**回车**打开配置文件：**sudo gedit /etc/sysctl.conf**
> 
> ![](https://img-blog.csdnimg.cn/48b21b0a723d436c843afcdd302e186d.png)
> 
> （2）在配置文件**最后一行**加入：**vm.swappiness=10**
> 
> ![](https://img-blog.csdnimg.cn/26c7423cf47c486ab33914f138b87eb1.png)
> 
> （3）保存配置文件：点击右上角 “**保存**” 后，关闭窗口，**重启电脑**
> 
> ![](https://img-blog.csdnimg.cn/3f967fffdb424ce18c3c133d8aae0adf.png)
> 
> （4）验证，在终端输入： **cat /proc/sys/vm/swappiness**  ，若输出为10，则修改成功
> 
> ![](https://img-blog.csdnimg.cn/401661b0294f4fb1be1179212fbd695e.png)

### 三、美化

### 1.美化前准备

> **依次在终端输入如下命令（输一条就回车等待执行应该不用教吧！！！）：**
> 
> （1）更新系统：**sudo apt update**
> 
> （2）下载更新软件：**sudo apt upgrade**
> 
> （3）下载gnome管理软件：**sudo apt install gnome-tweaks chrome-gnome-shell**
> 
> （4）下载扩展管理软件：**sudo apt install gnome-shell-extensions**

> #### **注意:**
> 
> 在开始美化前需要给大家普及一些知识，ubuntu22.04使用的Firefox是snap打包的，并不是原生的Firefox，22.04中的Firefox并不能安装扩展，我也不知道为什么，可能是一个bug，但22.04之前的版本都是原生的。下面介绍解决方法：

> **方法：更换为原生的Firefox**
> 
> （1）从[火狐浏览器官网](http://www.firefox.com.cn/ "火狐浏览器官网")下载安装包：
> 
> ![](https://img-blog.csdnimg.cn/0c1f9871164747d38ade34afc5826507.png)
> 
> （2）打开**文件管理器**，进入**下载**，选中Firefox的安装包，右键**提取到此处**
> 
> ![](https://img-blog.csdnimg.cn/e8eb46d404b84733918a67cd8d3a7335.png)
> 
> （3）打开**终端**，依次执行以下**命令**
> 
> 1）进入解压出来的目录，右键在终端打开：![](https://img-blog.csdnimg.cn/90cba5abe5a446019dccd2bea7b80c2a.png)
> 
> 2）命令：**sudo mv firefox /opt**
> 
> ![](https://img-blog.csdnimg.cn/662744aae29946fe8b9389e2994a1d18.png)
> 
> 3）命令：**sudo ln -s /opt/firefox/firefox /usr/local/bin/firefox**
> 
> ![](https://img-blog.csdnimg.cn/ba0db30fefd848e1b93fbe6a9d8c109f.png)
> 
> 4）命令：**sudo wget https://raw.githubusercontent.com/mozilla/sumo-kb/main/install-firefox-linux/firefox.desktop -P /usr/local/share/applications**
> 
> ![](https://img-blog.csdnimg.cn/69b66b10b59e4e49b839d42510e22111.png)
> 
> 注意：这一步可能会报错，如图，也可能一帆风顺
> 
> ![](https://img-blog.csdnimg.cn/4e3df7f0f19c416aa9a15325f0c33260.png)
> 
> 解决办法：
> 
> 1）打开该网址：[ip查询 查ip 网站ip查询 同ip网站查询 iP反查域名 iP查域名 同ip域名](https://site.ip138.com/raw.githubusercontent.com/ "ip查询 查ip 网站ip查询 同ip网站查询 iP反查域名 iP查域名 同ip域名")，把  raw.githubusercontent.com 输入后查询 ，复制第一个![](https://img-blog.csdnimg.cn/57a32b74f0994d899fe8eea8af4c8446.png)
> 
> 2）打开终端，输入命令：**sudo gedit /etc/hosts**
> 
> 3）在打开的页面中把复制的网址和域名填入
> 
> ![](https://img-blog.csdnimg.cn/14f9df4e3b0945429adde1ee752a8fb8.png)
> 
> 5）卸载ubuntu自带的Firefox：**sudo snap remove firefox**
> 
> ![](https://img-blog.csdnimg.cn/98344da5c469486cae51c495dcea4f2b.png)
> 
> 6）启动我们安装的Firefox：
> 
> ![](https://img-blog.csdnimg.cn/a41fb0c08390473f95c9102691a14d75.png)

### 2.扩展安装

> 扩展官网：[GNOME Shell Extensions](https://extensions.gnome.org/ "GNOME Shell Extensions")
> 
> （1）无论是Firefox还是谷歌浏览器，进入扩展官网后，点击下图**红框所示按钮**，将扩展添             加到浏览器
> 
> ![](https://img-blog.csdnimg.cn/14ddf839fea341f8ac396419c9abfabc.png)
> 
> （2）**刷新页面**，出现如下页面，则安装成功
> 
> ![](https://img-blog.csdnimg.cn/b9baea31b5c74e0e890cf2069a6279f1.png)
> 
> （3）下面我以其中一个插件为例（**该扩张为必装**），为大家演示安装过程
> 
> 1）进入**插件详情页**以后，点击右上角的红框框住**按钮**
> 
> ![](https://img-blog.csdnimg.cn/41a38c1ebe214b43b51147d4bf62a881.png)
> 
> 2）在出现的弹框中选择**安装**
> 
> ![](https://img-blog.csdnimg.cn/b37eeea7bcd3443cad9868e0b1b1ed5e.png)
> 
> 3）**刷新页面**，出现如图所示的页面，则该插件**安装成功**
> 
> ![](https://img-blog.csdnimg.cn/e1b473ee630042719181effb5d2bd3e9.png)
> 
> **Ps：在这个网站里有许多优秀的插件，下面是我为大家推荐的一些插件，大家可以按照上面的步骤安装。这些插件里只有User  Themes是必装的，其他的可装可不装**
> 
> 1.[Applications Menu - GNOME Shell Extensions](https://extensions.gnome.org/extension/6/applications-menu/ "Applications Menu - GNOME Shell Extensions")  -------在左上角显示应用程序
> 
> 2.[Compiz alike magic lamp effect - GNOME Shell Extensions](https://extensions.gnome.org/extension/3740/compiz-alike-magic-lamp-effect/ "Compiz alike magic lamp effect - GNOME Shell Extensions")   ------窗口最小化神奇效果
> 
> 3.[Removable Drive Menu - GNOME Shell Extensions](https://extensions.gnome.org/extension/7/removable-drive-menu/ "Removable Drive Menu - GNOME Shell Extensions")  -----设备链接时，在顶栏显示
> 
> 4.[TopIconsFix - GNOME Shell Extensions](https://extensions.gnome.org/extension/1674/topiconsfix/ "TopIconsFix - GNOME Shell Extensions")  ------优化win程序运行
> 
> 5.[User Themes - GNOME Shell Extensions](https://extensions.gnome.org/extension/19/user-themes/ "User Themes - GNOME Shell Extensions")  -------使用用户shell
> 
> 6.[Net speed Simplified - GNOME Shell Extensions](https://extensions.gnome.org/extension/3724/net-speed-simplified/ "Net speed Simplified - GNOME Shell Extensions")  -------显示网速
> 
> 8.[Coverflow Alt-Tab - GNOME Shell Extensions](https://extensions.gnome.org/extension/97/coverflow-alt-tab/ "Coverflow Alt-Tab - GNOME Shell Extensions")\--------改变Alt-Tab窗口样式

### 3.开始美化

> **我提供了几个安装包，大家可以用我提供的安装包，在文章末尾，如果大家使用我提供的安装包，则需要跳到后面的软件安装部分，先安装百度网盘。**
> 
> **如果要使用自己喜欢的，则可以自己到官网下载，记住：最好找有github地址的，直接从GitHub下载，不要在这个网站下载，根本下载不下来。官网地址：[Linux/Unix Desktops - pling.com](https://www.pling.com/browse?cat=148&ord=latest "Linux/Unix Desktops - pling.com")**
> 
> 给大家介绍一下这个网站：
> 
> ![](https://img-blog.csdnimg.cn/af7b13920da74381a33a656728637c59.png)
> 
> （2）红框框住的就是我们可以**下载的资源，**也可以点击左上角的**github地址，到github下载安装包（推荐使用该方式）**
> 
> ![](https://img-blog.csdnimg.cn/c8ff564f20c946fda987c5ae0a47c722.png)
> 
> ![](https://img-blog.csdnimg.cn/5bb7b180cb2d4429ad7812939d628020.png)
> 
> ![](https://img-blog.csdnimg.cn/1796d25e0ce74bc19f71b301953f1e04.png)
> 
> **注意：不同的主题安装的方法是不同的，大家可以在product部分或github详情页查看安装的注意事项**

### 4.美化-主题

> （1）官网地址：[WhiteSur Gtk Theme - pling.com](https://www.pling.com/p/1403328/ "WhiteSur Gtk Theme - pling.com")
> 
> （2）github地址：[https://github.com/vinceliuice/WhiteSur-gtk-theme](https://github.com/vinceliuice/WhiteSur-gtk-theme "https://github.com/vinceliuice/WhiteSur-gtk-theme")
> 
> （3）百度网盘地址：文章末尾
> 
> （4）安装：
> 
> 1）在文件管理器中选择**主目录**，在右上角三横中选中**显示影藏文件**
> 
> ![](https://img-blog.csdnimg.cn/443ccbe4802c457e856837ec338ac574.png)
> 
> 2）在该页面新建两个文件夹，分别是    **.themes**   和     **.icons**
> 
> ![](https://img-blog.csdnimg.cn/1e25a17131494e2c8f45adb0f3f5aa03.png)
> 
> 3)把WhiteSur-gtk-theme-master**解压**后，**剪切到.themes文件夹中**
> 
> ![](https://img-blog.csdnimg.cn/dde6e8fda12d4321adf226e86f95b339.png)
> 
> 4）**进入**WhiteSur-gtk-theme-master，右键**在终端打开**
> 
> ![](https://img-blog.csdnimg.cn/b4d841eccd2542d3b48bfcd539da767a.png)
> 
> 5）在终端执行命令：**./install.sh -t all**
> 
> ![](https://img-blog.csdnimg.cn/392d49775020494dbefa96b255a5a5e5.png)
> 
> 6）继续执行命令：**sudo ./tweaks.sh -g**
> 
> ![](https://img-blog.csdnimg.cn/8e6c62d409484b4ea00cab494ce0851b.png)
> 
> 7）美化firefox\*\*，\*\*这一步可根据自己的喜好选择，可做可不做。步骤为继续执行命令：
> 
> \*\*./tweaks.sh -f monterey，\*\*下图为美化前后对比，大家自行选择：
> 
> ![](https://img-blog.csdnimg.cn/60c1f610587447acb5abc32fc2677508.png)
> 
> ![](https://img-blog.csdnimg.cn/d2df7b04d724412c9914235daaf22bd8.png)

### 5.美化-图标

> （1）官网地址：[WhiteSur icon theme - pling.com](https://www.pling.com/p/1405756/ "WhiteSur icon theme - pling.com")
> 
> （2）github地址：[https://github.com/vinceliuice/WhiteSur-icon-theme](https://github.com/vinceliuice/WhiteSur-icon-theme "https://github.com/vinceliuice/WhiteSur-icon-theme")
> 
> （3）百度网盘地址：文章末尾
> 
> （4）安装：
> 
> 1）把WhiteSur-icon-theme-master**解压**后，**剪切到.icons文件夹中**
> 
> ![](https://img-blog.csdnimg.cn/dfc7d7ba7ce2480e9c0b65d032f5ef74.png)
> 
> 2）**进入**WhiteSur-icon-theme，右键**在终端打开**
> 
> ![](https://img-blog.csdnimg.cn/1ce1240148fc46dd88a158141b1bfba7.png)
> 
> 3）在终端执行命令：**./install.sh**
> 
> ![](https://img-blog.csdnimg.cn/ca5328f1df6d48f89469c03ef3dc10e6.png)

### 6.美化-光标

> （1）官网地址：[McMojave cursors - pling.com](https://www.pling.com/p/1355701/ "McMojave cursors - pling.com")
> 
> （2）github地址：[https://github.com/vinceliuice/McMojave-cursors](https://github.com/vinceliuice/McMojave-cursors "https://github.com/vinceliuice/McMojave-cursors")
> 
> （3）百度网盘地址：文章末尾
> 
> （4）安装;
> 
> 1）把McMojave-cursors**解压**后，**剪切到.icons文件夹中**
> 
> ![](https://img-blog.csdnimg.cn/42566415b150428caed28da41bfc7824.png)
> 
> 2）**进入**McMojave-cursors，右键**在终端打开**
> 
> ![](https://img-blog.csdnimg.cn/3c7e910f194a4aa6aa474effe9df216f.png)
> 
> 3）在终端执行命令：**sudo ./install.sh**
> 
> ![](https://img-blog.csdnimg.cn/387dbe30027041fe9534669290c39a8c.png)
> 
> 4）继续执行命令：**./build.sh**
> 
> 这个过程会持续一会，等待完成即可
> 
> ![](https://img-blog.csdnimg.cn/3d828481c9a74814b5cceec7c5dbc7fa.png)

### 7.配置

> （1）打开**优化**
> 
> ![](https://img-blog.csdnimg.cn/32d56b1e78614c9ebc5772c715ee3860.png)
> 
> （2）选择**外观**选项，各项**配置**如图，也可自己**选择喜欢**的，不只有白色，还有黑色等待
> 
> ![](https://img-blog.csdnimg.cn/8cbcb8a0b5c2486397e0f71316b60fda.png)
> 
> （3）选择**窗口**选项，打开**居中显示新窗口**
> 
> ![](https://img-blog.csdnimg.cn/a56859f4a41b468aa48e1cb88bef9c67.png)
> 
> （4）选择**窗口标题栏**选项，放置**可左可右**
> 
> ![](https://img-blog.csdnimg.cn/a4108e956b134d1bb0a81522950c1f96.png)
> 
> （5）设置中的配置，打开**设置**，选中**外观**，关闭**显示个人文件夹**
> 
> ![](https://img-blog.csdnimg.cn/fd5e0abf6a004beeac56dfc5757cc3b4.png)
> 
> （6）打开**扩展官网**，添加以下插件
> 
> ![](https://img-blog.csdnimg.cn/49d7a40796864489b77616b010a4a5c0.png)
> 
> （7）打开**扩展**
> 
> ![](https://img-blog.csdnimg.cn/15479250c4934a7e9dc1167372eae96d.png)
> 
> （8）关闭**ubuntu dock**
> 
> ![](https://img-blog.csdnimg.cn/b2ec69bfc05f4eb0a6f65c963ad1d152.png)
> 
> （9）配置**dash to dock**
> 
> ![](https://img-blog.csdnimg.cn/99df99c55c224ae99e6ee086b048ca27.png)
> 
> （10）配置如下，也可根据自己喜好配置
> 
> ![](https://img-blog.csdnimg.cn/51f733466346489786357af4b3ed34c0.png)

### 9.美化-双系统启动界面

> （1）官网地址：[Poly light - pling.com](https://www.pling.com/p/1176413 "Poly light - pling.com")
> 
> （2）github地址：[GitHub - shvchk/poly-light: Poly light GRUB theme](https://github.com/shvchk/poly-light "GitHub - shvchk/poly-light: Poly light GRUB theme")
> 
> （3）百度网盘地址：文章末尾
> 
> （4）安装;
> 
> 1）把poly-light-master**解压**后，**剪切到你想要存放的文件夹即可**
> 
> ![](https://img-blog.csdnimg.cn/c9ab22dd80f14917b521a1d6c8f8075f.png)
> 
> 2）**进入**poly-light-master，右键**在终端打开**
> 
> ![](https://img-blog.csdnimg.cn/9d99e2aa757748c7ad23ab1f2de7a90e.png)
> 
> 3）在终端执行命令： **./install.sh**
> 
> ![](https://img-blog.csdnimg.cn/32525777e91444868999a362e310ce2a.png)

### 四.常用软件安装

> ### 1.安装搜狗输入法
> 
> （1）到[搜狗输入法Linux版](https://shurufa.sogou.com/linux "搜狗输入法Linux版")官网下载**deb包**，将deb包剪切到你想存放的文件夹即可
> 
> ![](https://img-blog.csdnimg.cn/45c02fa4f9b94c2698af6e570d89a882.png)
> 
> ![](https://img-blog.csdnimg.cn/1abcc86052134df082e37a11f4c276e2.png)
> 
> ![](https://img-blog.csdnimg.cn/96e9b6fd01d146e6aa8c56763c0c983b.png)
> 
> （2）打开**终端**，依次**执行以下命令**
> 
> 1）下载**fcitx**输入法框架：**sudo apt install fcitx**
> 
> ![](https://img-blog.csdnimg.cn/1315e48a8cb84d678f964f4178e60895.png)
> 
> 2）打开**语言支持**
> 
> ![](https://img-blog.csdnimg.cn/981bb7d051ad44c38dee02ee7dad2b52.png)
> 
> 3）将**键盘输入法系统改为Fcitx4**，然后点击**应用到整个系统**，输入密码即可
> 
> ![](https://img-blog.csdnimg.cn/d57a873e5e2a4bd492069063ecc5f863.png)
> 
> 4）在**终端**执行命令，把**Fcitx4**加入**开机自启动**：
> 
> **sudo cp /usr/share/applications/fcitx.desktop /etc/xdg/autostart/**
> 
> ![](https://img-blog.csdnimg.cn/780145752a004ea482e28cb2cf9528ba.png)
> 
> 5）打开**终端**，输入命令，**卸载**系统自带的**ibus**输入法（这个输入法很难用）：
> 
> **sudo apt purge ibus**
> 
> ![](https://img-blog.csdnimg.cn/b7fb17ff18f645a58d3fc206c6634a34.png)
> 
> 6）到**存放搜狗输入法deb包的文件夹**，右键在**终端**打开
> 
> ![](https://img-blog.csdnimg.cn/ee4a9a5d7e354b609b69909cd39b11e6.png)
> 
> 7）在**终端**执行命令：**sudo dpkg -i sogoupinyin\_4.0.1.2800\_x86\_64.deb**
> 
> ![](https://img-blog.csdnimg.cn/06fda487e89e4b7e8ba2370d447bdb58.png)
> 
> 8）继续安装**依赖**，在**终端**执行命令：
> 
> **sudo apt install libqt5qml5 libqt5quick5 libqt5quickwidgets5 qml-module-qtquick2**
> 
> **sudo apt install libgsettings-qt1**
> 
> ![](https://img-blog.csdnimg.cn/404ee0269fd548dab1046c62af9f7a32.png)
> 
> ![](https://img-blog.csdnimg.cn/04230a33e5e241989052692732c7a355.png)
> 
> 9）**重启**电脑，点击右上角输入法按钮，调出输入法**配置**
> 
> ![](https://img-blog.csdnimg.cn/73605a9a459143b28a243e828ef1c043.png)
> 
> 10）将搜狗输入法调至**第一位，ctrl + 空格即可切换输入法**
> 
> ![](https://img-blog.csdnimg.cn/c41cf0a4ea9b4099b83866704236f63d.png)
> 
> ### 2.安装**星火应用商店**，很多软件我们都可以从这里面安装
> 
> （1）到[官网](https://spark-app.store/ "官网")下载安装包
> 
> ![](https://img-blog.csdnimg.cn/47ea33c41cc64c8984ef67e9019e474d.png)
> 
> （2）需要注意，**22.04**之前版本要下载**依赖包**和**软件本体**，22.04下载**软件本体**即可
> 
> ![](https://img-blog.csdnimg.cn/1448179631384628bd7547a4edd3b358.png)
> 
> （3）版本不同，**自己仔细阅读安装说明**，**22.04**版本直接点击开始下载，20.04需要安装依赖
> 
> ![](https://img-blog.csdnimg.cn/6935bd2446f1493282b5ded30525d54a.png)
> 
> （4）把安装包剪切到**你想要存放**的文件夹，右键打开**终端**
> 
> ![](https://img-blog.csdnimg.cn/6f0a8f0620a2407189c35145b19b0711.png)
> 
> （5）执行命令完成安装：**sudo apt install ./spark-store\_3.1.2\_amd64.deb**
> 
> ![](https://img-blog.csdnimg.cn/d97458050b374a3ab933233531ecb18b.png)
> 
> ### 3.安装微信
> 
> （1）打开**星火应用商店**，找到Linux版本的微信，这个版本的微信是**linux**版本的，不需要使           用**win**环境，如下图，推荐使用该版本
> 
> ![](https://img-blog.csdnimg.cn/277fbb16363b4811a6295e7ff8739a08.png)
> 
> （2）**win**版微信，这个版本需要使用win环境，**启动慢**，但**功能齐全**使用与windows版本一致
> 
> ![](https://img-blog.csdnimg.cn/c29b93b3fccd40ecbf9dc68df88bc3b6.png)
> 
> ### 4.安装QQ
> 
> 好消息，重大好消息，挺尸了不知多少年的linux版本QQ，最近开始更新了，并且已经开始内测，我也12月份才知道的，并对该部分的内容进行了修改。大家如果对QQ没有必要需求的话，暂时不建议安装任何版本的QQ，等众测或软件上线后再选择安装。如果需要的话可以安装TIM顶替一下，这个TIM是win的，并且有些bug。

> ### 5.安装音乐软件
> 
> **这两个都可以**
> 
> ![](https://img-blog.csdnimg.cn/adeeaefdc11d46a78f90d378ee860ac1.png)

> ### 6.安装腾讯会议
> 
> ![](https://img-blog.csdnimg.cn/fb9ce27e316747dc889622248b8cb38a.png)

> ### 7.安装wps
> 
> ![](https://img-blog.csdnimg.cn/59d2608eeb1841d89b4b5d9fd8420daf.png)
> 
> ### 安装wps缺失的字体
> 
> ![](https://img-blog.csdnimg.cn/7038273f21224a7a85204b094d26a0bc.png)

> ### 8.安装百度网盘
> 
> ![](https://img-blog.csdnimg.cn/a7d17c39a2b84c60a2b3e461c68b04ca.png)

### 五.java开发环境配置

> ### 1.安装jdk1.8
> 
> （1）在**终端**输入命令安装：**sudo apt-get install openjdk-8-jdk**
> 
> （2）验证：输入命令    **java -version**     和     \*\*javac -version，\*\*输出如图则安装成功
> 
> ![](https://img-blog.csdnimg.cn/407983cbe5cc4bcaa699afcce36d95fc.png)

> ### 2.安装idea
> 
> （1）从[IDEA官网](https://www.jetbrains.com/idea/download/#section=linux "IDEA官网")下载你喜欢的版本
> 
> ![](https://img-blog.csdnimg.cn/ba712d71324a4d4ab3a585aed25c1364.png)
> 
> （2）把下载的安装包解压出来，进入bin目录，右键在终端打开
> 
> （3）输入命令启动：**./idea.sh**
> 
> （4）**创建一个空项目**，点击右上角的**搜索**按钮，搜索**create desktop**，创建桌面快捷方式
> 
> ![](https://img-blog.csdnimg.cn/fe301b04310340239c38142c6ed30d47.png)

> ### 3.配置vue开发环境
> 
> （1）更新系统
> 
> ```
> sudo apt update && sudo apt upgrade
> ```
> 
> （2）安装依赖
> 
> ```
> sudo apt install curl gnupg2 gnupg git wget -y
> ```
> 
> （3）拉取node.js
> 
> ```
> curl -fsSL https://deb.nodesource.com/setup_lts.x | sudo bash -
> ```
> 
> （4）更新一下系统
> 
> ```
> sudo apt update
> ```
> 
> （5）安装node.js
> 
> ```
> sudo apt install nodejs
> ```
> 
> （6）验证node.js安装是否成功
> 
> ```
> node --version
> ```
> 
> ![](https://img-blog.csdnimg.cn/a81cbcbbd3b940c68ed9dcf62e2dfa63.png)
> 
> （7）把NPM更新到最新版本
> 
> ```
> sudo npm install npm@latest -g
> ```
> 
> （8）验证npm的安装
> 
> ```
> npm -v
> ```
> 
> ![](https://img-blog.csdnimg.cn/1a198c6d996f4382b8835fd99c0685db.png)
> 
> （9）安装vue.js
> 
> ```
> sudo npm install -g @vue/cli
> ```
> 
> （10）验证vue的安装
> 
> ```
> vue --version
> ```
> 
> ![](https://img-blog.csdnimg.cn/83a67bb520474f1a9d5eb3b812a16bcc.png)

> ### 4.安装MySQL
> 
> -   从官网[MySQL :: Download MySQL Community Server (Archived Versions)](https://downloads.mysql.com/archives/community/ "MySQL :: Download MySQL Community Server (Archived Versions)")下载MySQL5.7.32  Ubuntu版本
> 
> ![](https://img-blog.csdnimg.cn/a88cdb02cdc9432ab65e593002a40f93.png)
> 
> -   安装：
> 
> ```
> dpkg --list|grep mysql将下载的安装包剪切到你自己的安装目录，右键解压sudo apt-get updatesudo apt-get upgradesudo apt-get install libaio1sudo apt-get install libtinfo5sudo dpkg -i mysql-common_5.7.32-1ubuntu18.04_amd64.debsudo dpkg-preconfigure mysql-community-server_5.7.32-1ubuntu18.04_amd64.deb sudo dpkg -i libmysqlclient20_5.7.32-1ubuntu18.04_amd64.debsudo dpkg -i libmysqlclient-dev_5.7.32-1ubuntu18.04_amd64.debsudo dpkg -i libmysqld-dev_5.7.32-1ubuntu18.04_amd64.debsudo dpkg -i mysql-community-client_5.7.32-1ubuntu18.04_amd64.debsudo dpkg -i mysql-client_5.7.32-1ubuntu18.04_amd64.debsudo dpkg -i mysql-common_5.7.32-1ubuntu18.04_amd64.debsudo apt-get -f installsudo apt-get -f install libmecab2sudo dpkg -i mysql-community-server_5.7.32-1ubuntu18.04_amd64.debsudo dpkg -i mysql-server_5.7.32-1ubuntu18.04_amd64.debmysql -u root -p
> ```
> 
> ### 5.配置MySQL
> 
> -   配置MySQL：
> 
> ```
> sudo mysql_secure_installation
> ```
> 
> -   按提示操作，出现结尾的图片即表示安装成功：
> 
> ```
> VALIDATE PASSWORD PLUGIN can be used to test passwords...Press y|Y for Yes, any other key for No: N (选择N，不进行密码的强度校验)Change the password for root ? ((Press y|Y for Yes, any other key for No) :N（选N，不修改root密码）###By default, a MySQL installation has an anonymous user allowing anyone to log into MySQL without having to have a user account created for them...Remove anonymous users? (Press y|Y for Yes, any other key for No) : N (选择N，不删除匿名用户)###Normally, root should only be allowed to connect from，'localhost'. This ensures that someone cannot guess at the root password from the network...Disallow root login remotely? (Press y|Y for Yes, any other key for No) : Y (选择y，不允许root远程连接)###By default, MySQL comes with a database named 'test' that anyone can access...Remove test database and access to it? (Press y|Y for Yes, any other key for No) : N (选择N，不删除test数据库)###Reloading the privilege tables will ensure that all changes made so far will take effect immediately.Reload privilege tables now? (Press y|Y for Yes, any other key for No) : Y (选择Y，修改权限立即生效)#检查MySQL状态systemctl status mysql.service
> ```
> 
> ![](https://img-blog.csdnimg.cn/ae4846a208524363aba6bc6fc75795de.png)
> 
> -   配置MySQL字符集
> 
> ```
> SHOW VARIABLES LIKE 'character%';sudo gedit /etc/mysql/conf.d/mysql.cnfno-auto-rehashdefault-character-set=utf8sudo gedit /etc/mysql/mysql.conf.d/mysqld.cnfcharacter-set-server=utf8service mysql restart
> ```
> 
> -   验证是否配置成功：
> 
> 登录mysql
> 
> SHOW VARIABLES LIKE ‘character%’;     -----查看mysql字符集
> 
> 如图为配置成功：
> 
> ![](https://img-blog.csdnimg.cn/074d202378fe4c16a5897ef1448472e3.png)
> 
> ### 6.安装Navicat16
> 
> -   进入网站（非常感谢提供该网站的大神）：[Navicat For Linux - rainerosion (rainss.cc)](https://navicat.rainss.cc/ "Navicat For Linux - rainerosion (rainss.cc)")，按照该网站提供的的操作步骤自行操作，软件下载16版本。
> -   将安装包剪切到你的安装目录，执行以下命令，运行程序
> 
> ```
> sudo apt install libfuse2chmod +x Navicat_Premium_16_cs-x86_64.AppImage./Navicat_Premium_16_cs-x86_64.AppImage
> ```

> **百度网盘资源分享地址：https://pan.baidu.com/s/12h0Cug-qnvaOYXKt\_EfVGw   
> 提取码：cfyl**