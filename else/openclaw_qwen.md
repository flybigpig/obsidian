****OpenClaw********（********小龙虾********）********本地部署教程+********千问********或者deepseek模型****

首先下载需要用到的软件Node.js

Node.js官网链接：[https://nodejs.cn/en/download](https://nodejs.cn/en/download "https://nodejs.cn/en/download")

![](https://i-blog.csdnimg.cn/direct/2560464b5dfc401291bc7e655a363225.png)

我们安装到E盘，自定义安装

![](https://i-blog.csdnimg.cn/direct/13604d659b4b4845aa65b51a7f6077d3.png)

Win+r  cmd进入终端出来版本有没有安装成功

![](https://i-blog.csdnimg.cn/direct/523665c504054bccb65c8f5d1235efc7.png)

查看版本：node -v   安装成功后会自己安装npm 查看：npm -v

![](https://i-blog.csdnimg.cn/direct/3bfb8b2fa85e46a1bce93f41141cf988.png)

在访问OpenCaw小龙虾中文官网

官网链接：[https://clawd.org.cn/](https://clawd.org.cn/ "https://clawd.org.cn/")

![](https://i-blog.csdnimg.cn/direct/20628f635f4545bcaa0d3f71209abed2.png)

我们点击右上角的github小龙虾官网：[https://github.com/jiulingyun/openclaw-cn](https://github.com/jiulingyun/openclaw-cn "https://github.com/jiulingyun/openclaw-cn")

![](https://i-blog.csdnimg.cn/direct/9fe04608e54442acbb13d88733aff777.png)

滑到最下面找到快速开始或安装方式

![](https://i-blog.csdnimg.cn/direct/98c14c40cb534cc88509a5c6e0bcf5ec.png)

然后我们自定义安装到E盘这个路径：E:\\OpenClaw\\Open

![](https://i-blog.csdnimg.cn/direct/652c985ead6c4f45bf9b537616614634.png)

在目录上面输入cmd打开终端

![](https://i-blog.csdnimg.cn/direct/2c8ffb4ac6594aa6ad533ba95cd6d632.png)

现在安装OpenClaw，我们使用官网命令：

\# 安装

npm install -g openclaw-cn@latest

![](https://i-blog.csdnimg.cn/direct/f579ece832fb4264b51640b4858f4ea0.png)

复制到终端，然后回车！

![](https://i-blog.csdnimg.cn/direct/96ad0e45472b44a38288accbcf6109af.png)

已经安装成功！

![](https://i-blog.csdnimg.cn/direct/e9ddc5aa931f4e7ea99933f3543a27ad.png)

在复制第二行命令

\# 运行安装向导

openclaw-cn onboard --install-daemon

![](https://i-blog.csdnimg.cn/direct/f25a6069122a4386835e237dd0608b79.png)

现在肯定部署，选择yes然后回车，按左边键盘箭头

![](https://i-blog.csdnimg.cn/direct/4fbad88713b64b53828ce583e403e198.png)

我们可以选择手动或者快速快速，这里我们选择快速开始！然后回车

![](https://i-blog.csdnimg.cn/direct/3d95d9c381304d358a7efd921dc98bcf.png)

这里直接回车，就可以！

![](https://i-blog.csdnimg.cn/direct/781ac06ff1a0421fa86b1570bfd7defa.png)

开始认证模型，有很多，我选择deepseek、都是需要付费，所以我选择千问，直接选择 Qwen 提供商 或 使用自定义模型，然后回车！

![](https://i-blog.csdnimg.cn/direct/61acc82146e34f8b85d1acf80ef7f38d.png)

回车就会出现认证方法

![](https://i-blog.csdnimg.cn/direct/c5f55cd087f94832ae396afd896a7fb7.png)

然后我们打开千问，点击或者访问下面这个网址

![](https://i-blog.csdnimg.cn/direct/23cf402eec414caa828b19aa9babfdbe.png)

如果没有注册的，重新注册一个账号，这里我已经注册成功！

![](https://i-blog.csdnimg.cn/direct/cef1f0da7fb146e6895decdd86dc5082.png)

注册成功，就会出现已经登录

![](https://i-blog.csdnimg.cn/direct/6ac1322e1cc54046b4a738d0d451835a.png)

选择默认模型，保持当前，回车

![](https://i-blog.csdnimg.cn/direct/e1a8ada2a8f8497bbc11e792bb9ebf4c.png)

选择通道，可以选择飞书等等，我们需要下面的暂时跳过！

![](https://i-blog.csdnimg.cn/direct/96d3ee5683e54f2f821a36c0b2147487.png)

配置技能选择yes，然后回车！

![](https://i-blog.csdnimg.cn/direct/5abffff97f6c42469eb89189e95215f8.png)

技能管理器我们现在npm，因为我们安装Node.js了，然后回车！

![](https://i-blog.csdnimg.cn/direct/0a6f517b7ecb41f0822928deb4a7a248.png)

钩子这里，我们选择暂时跳过，先按空格键，然后出现+，在按回车！

![](https://i-blog.csdnimg.cn/direct/d744b2d2f9cc43a4ae14fdc10500da10.png)

出现网关服务失败

![](https://i-blog.csdnimg.cn/direct/1ad704f755164c2d8f607ad08d2651cb.png)

选择github第三行启动网关

\# 启动网关

openclaw-cn gateway --port 18789 --verbose

![](https://i-blog.csdnimg.cn/direct/4fbb342d690f4ffbb9a2a96836688e51.png)

然后在命令中输入，回车

![](https://i-blog.csdnimg.cn/direct/2aae349e64d546e6a62b55054ee8f6a4.png)

最后，已经配置成功，出现网址，现在已经搭建属于自己的al模型了

![](https://i-blog.csdnimg.cn/direct/03fa8c67643b49fb8c7deb0a43f74c09.png)

我们浏览器访问自己的官网链接：[http://127.0.0.1:18789/](http://127.0.0.1:18789/ "http://127.0.0.1:18789/") 右上角出现正常状态说明，已经本地部署成功了！

![](https://i-blog.csdnimg.cn/direct/2e81883ca9f34c15b9246399ee539de9.png)

我们问问OpenClaw能不能回答我们的问题！

您好，你是什么模型？，最后点击发送

![](https://i-blog.csdnimg.cn/direct/9654a3d185294dc4840c1f0f76b4d464.png)

标题

他说：我是 Qwen Coder 模型，由通义千问团队开发。有什么我可以帮你的吗？

这里已经帮我自动打开，我关闭了PPT页面，因为OpenClaw（小龙虾），有最高控制权限，而且没有更好的完善，可能会存在bug漏洞，使用尽量等完善后在使用，保护自己的隐私！！！

而且尽量少在自己本机搭建，这里演示，我已经关闭了！！！

切记，一定要保护好自己的个人隐私不被泄露！！！
