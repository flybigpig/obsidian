目录



一、打开官网下载最新版Typora



Typora 官网下载



安装：



二、激活过程



关闭软件每次启动时的已激活弹窗



提示



去除软件左下角“未激活”提示



激活结束



三、闪退、打不开md文档问题



一、打开官网下载最新版Typora

Typora 官网下载

安装：

Typora中文官网：[https://typoraio.cn/](https://typoraio.cn/)

Typora官网：[https://typora.io/releases/all](https://typora.io/releases/all)



官网长这个样子







安装完成之后就可以进行激活操作了



（最好不要安在C盘，并且打开文件夹的时候用管理员权限打开）







二、激活过程

右键软件——打开文件所在位置







根据这个路径进行查找文件



Typora\resources\page-dist\static\js











 







右键——打开方式——使用记事本打开



Ctrl+F进行查找：



e.hasActivated="true"==e.hasActivated



替换为



e.hasActivated="true"=="true"



此时打开就会发现提示已经激活成功，但是每次打开都会提示这个激活，并且左下角会提示未激活。



关闭软件每次启动时的已激活弹窗

在Typora安装目录依次找到这个文件



resources\page-dist\license.html







用记事本打开它，



查找



</body></html>



替换为



</body><script>window.οnlοad=function(){setTimeout(()=>{window.close();},5);}</script></html>



如果还不行：



弹窗已激活的暴力解决方案：给license的html文件删了（来自评论区——其他为解决问题可以多看看评论区的大佬们的解决方案）————本人尝试后发现会弹出白窗，感觉行不通



来自评论区：onload事件名称的字符并不是标准的英文字母o和n，而是看起来像是使用了希腊字母或其他Unicode字符的相似字符（例如，ο是希腊字母omicron，而ο和o在视觉上非常相似）。这通常是一个错误，可能是复制粘贴时引入的。正确的应该是window.onload



解决办法：手敲(可以在VSCode打开这个文件进行编辑，会发现onload这里两个o会框起来，手敲之后就没有了，此时不会再弹窗了)



提示

请仔细检查自己的拼写，如果直接复制粘贴可能格式有问题，可以自己试着手敲，如果发现本文哪里有错误也可以及时评论纠正，谢谢



去除软件左下角“未激活”提示

在Typora安装目录依次找到这个文件



resources\locales\zh-Hans.lproj\Panel.json 











查  找



"UNREGISTERED":"未激活"



替换为



"UNREGISTERED":" "



激活结束

激活后的 Typora，各种功能均能正常使用，仅有“许可证信息”/“我的许可证”页面无法打开、左下角存在“x”（可手工点击关闭但重新打开软件会重新出现）。



参考文章：[http://t.csdnimg.cn/919Uy](http://t.csdnimg.cn/919Uy)



三、闪退、打不开md文档问题

参考我以前的文章



[http://t.csdnimg.cn/YG3Bp](http://t.csdnimg.cn/YG3Bp)

[http://t.csdnimg.cn/YG3Bp](http://t.csdnimg.cn/YG3Bp)

————————————————


原文链接：[https://blog.csdn.net/weixin_73609038/article/details/137751767](https://blog.csdn.net/weixin_73609038/article/details/137751767)

