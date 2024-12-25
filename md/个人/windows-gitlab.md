本教程用于学习在windows下如何安装gitlab，先是探索了[docker](https://edu.51cto.com/lesson/996017.html?utm_platform=pc&utm_medium=51cto&utm_source=shequ&utm_content=bk_article_keyword#docker)安装方式，最后使用了WSL方式安装

## 第一步：搜索引擎搜索 gitlab install

进入链接  [Install GitLab | GitLab](https://docs.gitlab.com/ee/install/)

![windows gitlab安装及配置教程_Docker](https://s2.51cto.com/images/blog/202407/13022606_6691753e41b5694998.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=/format,webp/resize,m_fixed,w_1184 "img")

## 第二步：点击Installation methods

![windows gitlab安装及配置教程_Docker_02](https://s2.51cto.com/images/blog/202407/13022606_6691753e6a2c842739.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=/format,webp/resize,m_fixed,w_1184 "img")

在此步骤发现没有windows下的安装，但是看到了docker 安装方式，所以先安装docker for windows（这里不着急安装，可以先往后看）

## 第三步：点击Docker

![windows gitlab安装及配置教程_Docker_03](https://s2.51cto.com/images/blog/202407/13022606_6691753e7f4051979.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=/format,webp/resize,m_fixed,w_1184 "img")

## 第四步：查看docker方式安装文档

这里发现官网说不支持docker for windows  

![windows gitlab安装及配置教程_docker_04](https://s2.51cto.com/images/blog/202407/13022606_6691753e9483657850.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=/format,webp/resize,m_fixed,w_1184 "img")

原文：Docker for Windows is not officially supported. There are known issues with volume permissions, and potentially other unknown issues. If you are trying to run on Docker for Windows, see the getting help page for links to community resources (such as IRC or forums) to seek help from other users.

翻译：Docker for Windows不受官方支持。卷权限存在已知问题，还可能存在其他未知问题。如果您正在尝试在Windows上运行Docker，请参阅获取帮助页面，以获得社区资源(如IRC或论坛)的链接，以寻求其他用户的帮助。

经过实践，确实无法运行，所以docker方式放弃，想想其他思路。这时候想到windows下有个 [WSL](https://learn.microsoft.com/zh-cn/windows/wsl/)功能，所以尝试通过WSL来运行[linux](https://edu.51cto.com/lesson/981900.html?utm_platform=pc&utm_medium=51cto&utm_source=shequ&utm_content=bk_article_keyword#linux)，然后再进行gitlab安装

安装WSL的过程略过，具体可以看微软官网的安装教程

WSL安装完成后，cmd命令行运行

```plain
wsl --install
```

这里会默认安装一个ubuntu linux系统，通过“开始”菜单搜索ubuntu，点击运行  

![windows gitlab安装及配置教程_git_05](https://s2.51cto.com/images/blog/202407/13022606_6691753ea98de85509.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=/format,webp/resize,m_fixed,w_1184 "img")

输入账号，密码进入  

![windows gitlab安装及配置教程_git_06](https://s2.51cto.com/images/blog/202407/13022606_6691753ebe21e5372.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=/format,webp/resize,m_fixed,w_1184 "img")

## 第五步：点击linux package 查看linux方式安装文档

![windows gitlab安装及配置教程_git_07](https://s2.51cto.com/images/blog/202407/13022606_6691753ed1db682317.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=/format,webp/resize,m_fixed,w_1184 "img")

## 第六步：点击install gitlab

![windows gitlab安装及配置教程_git_08](https://s2.51cto.com/images/blog/202407/13022606_6691753ee437848590.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=/format,webp/resize,m_fixed,w_1184 "img")

## 第七步：点击Manually downloading

注意不要点击上面的installing gitlab ，它会被重定向到极狐GitLab，看着像是要收费  

![windows gitlab安装及配置教程_docker_09](https://s2.51cto.com/images/blog/202407/13022607_6691753f1151727142.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=/format,webp/resize,m_fixed,w_1184 "img")

## 第八步：根据文档运行安装命令

![windows gitlab安装及配置教程_Docker_10](https://s2.51cto.com/images/blog/202407/13022607_6691753f25fa189603.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=/format,webp/resize,m_fixed,w_1184 "img")

```plain
# Ubuntu/Debian 企业版安装
sudo apt update && sudo apt install gitlab-ee
```

命令里安装的是企业版，不收费，不过是会带一些收费才能开通的功能，这里没有付费意愿的话，直接安装社区免费版就行，里面会去掉需要收费的功能,将gitlab-ee改为gitlab-ce即可。

```plain
# Ubuntu/Debian 社区免费版安装
sudo apt update && sudo apt install gitlab-ce
```

安装完成后会显示如下：  

![windows gitlab安装及配置教程_Docker_11](https://s2.51cto.com/images/blog/202407/13022607_6691753f36e4729996.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=/format,webp/resize,m_fixed,w_1184 "img")

gitlab默认会创建一个root用户，账号为：root ，密码在/etc/gitlab/initial_root_password文件里

可以通过命令查看：

```plain
sudo cat /etc/gitlab/initial_root_password
```

![windows gitlab安装及配置教程_git_12](https://s2.51cto.com/images/blog/202407/13022607_6691753f45b044731.png?x-oss-process=image/watermark,size_16,text_QDUxQ1RP5Y2a5a6i,color_FFFFFF,t_30,g_se,x_10,y_10,shadow_20,type_ZmFuZ3poZW5naGVpdGk=/format,webp/resize,m_fixed,w_1184 "img")

  

马赛克部分即是密码  

## 第九步：浏览器访问localhost或127.0.0.1

会显示如下界面，然后输入上一步的账号密码登录



------------------

## **一、重置root密码**

1. 以管理员身份打开 **PowerShell**

![](https://pica.zhimg.com/v2-89e021160872ed2ea79dfa0b495f97c6_1440w.jpg)

  

2. 输入命令 `wsl.exe --user root`

![](https://picx.zhimg.com/v2-ee5d27c53de966ba1135eb2ed8ceb769_1440w.jpg)

  

3. 输入命令 `passwd root` 修改 root 密码

![](https://pic4.zhimg.com/v2-d210dffd3c69b3f959b6bb5ad7cc6b6d_1440w.jpg)

  
4. 输入新的密码并确认后，root密码即得到重置

![](https://pic1.zhimg.com/v2-e6c7a4b3e279f3efedf2fc28a5c98880_1440w.jpg)

  

## **二、重置用户密码**

重置与用户密码与重置root密码类似，

1. 以管理员身份打开 **PowerShell**

2. 输入命令 `[wsl](https://zhida.zhihu.com/search?content_id=174193391&content_type=Article&match_order=2&q=wsl&zhida_source=entity).exe --user root`

3. 输入命令 `passwd username` 修改用户密码，username即待重置的用户的名称

  

如果对你有用，就点个赞吧~


---




1:执行

sudo apt-get update

2:执行

sudo apt-get install gitlab-ce

3:如果依旧失败出现以下提示
Reading package lists... Done
Building dependency tree       
Reading state information... Done
E: Unable to locate package gitlab-ce

执行

curl -sS https://packages.gitlab.com/install/repositories/gitlab/gitlab-ce/script.deb.sh | sudo bash

4:如果执行crul语句出错提示以下内容

Command 'curl' not found, but can be installed with:

apt install curl
执行:

apt install curl

5再次执行

sudo apt-get update

sudo apt-get install gitlab-ce

参考文件

Installing on Ubuntu 14.04 x64 Failed: "Unable to locate package gitlab-ce" (#2370) · Issues · GitLab.org / GitLab FOSS · GitLab