## 简历书写建议

1、写上自己突出重点的framework一些模块，比如你精通某一个framework模块可以大胆写出，不要担心自己模块少，或担心和招聘需求的不完全吻合

2、写上一些自己曾经解决分析过的一些经典疑难问题，比如anr，闪黑，黑屏，冻屏，或性能优化，等系统问题

3、不写一些和framework不是太相关的技能，比如app开发的一些第三方开源框架等

4、不挨个写一大堆历年做的项目，建议留下1-2最有成就的稍微写一下即可以，写上简历的技能就一定要保证自己可以答得非常好，可以得到比较好面试成绩。

5、重点突出自己framework方面的技能点，及熟悉模块和优势案例展现

6、考虑针对某个公司的职位描述编写对应的简历，尽量编写技能靠近职位描述相关的亮点，实现简历的定制化，凸显出自己亮点优点，提高面试的答题得分。

你的简历应该怎么写，也可以看下面这个视频：https://www.bilibili.com/video/BV1iw411s77V/

![在这里插入图片描述](https://mmbiz.qpic.cn/sz_mmbiz_png/DYicOkJDdA2pjrCpQMN4043V6L4zErIIhLvmPeMibk3EABZepcewQfwS4cMATiaxZzFZzvrcE8uTKHH9pJEcSXJ6A/640?wx_fmt=png&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=0)

在这里插入图片描述

## 近期学员面试fw相关真题：

**同学A：**

开机动画到桌面流程

ANR如何处理

冻屏如何处理

是否遇到过native crash

是否遇到过黑屏

闪黑如何处理

启动应用黑屏如何处理

**同学B：**

你了解堆栈相关的打印，请问你native堆栈如何打印？是否遇过编译引入后库冲突问题，你是如何解决？

你说你这边了解vsync，你能说说vsync的整个流程如何么？

你这边可以说说SurfaceControl中有个setFrameRate方法设置设置帧率，你能说说它的背景和作用么？以及在SurfaceFlinger中是如何处理setFrameRate相关操作，与sf中的FrameRateOverride是否有关联？

你这边平时怎么看实时帧率的请说说，你又是怎么开发的这个帧率实时显示控件的，麻烦说说原理。

你说你了解Perfetto，请问线程的运行状态有哪几种，一般线程间的唤醒等你是如何在Perfetto中看的。

你面对卡顿问题时候一般是怎么分析的，你有什么方式能够帮助app能够自动化实现卡顿检测分析经验么？

你是否做过开机速度优化方案？如果做过请大概描述描述1-2种确实有优化效果的成功方案

## framework面试官角度分享面试经验

下面以面试官角度列出framework面试的正确姿势：

 1、简历中写的framework精通或者熟悉模块不会要求很多，但是写上去的就要求真正掌握，framework开发涉及模块太多了，经典就有binder，ams，atms，wms，input，pms等经典的大模块，还有若干native模块input，SurfaceFlinger，audioflinger等，你说一个人都掌握了也不太可能，而且招聘需求都是会要求只要精通其中一两个模块即可以。

2、framework的承担开发要求精，因为framework毕竟属于系统层面东西，一修改影响整个系统的功能和稳定，分析代码理解代码精通代码才可能可以修改掉系统一些问题，及最小波及的实现系统的一下新功能，不是网络百度复制粘贴一下即可以，所以这里要求是对简历上写出的精通或熟悉模块，面试官会挖的比较深入，会对模块很多重要部分进行提问考察，这个部分就是对你真正是否熟悉这个模块的考察，也是区别你是真懂的实战派还是说看了看blog和记忆性的背书党关键。一般面试官不会提那种泛泛而谈让你顺利背书的题。

3、了解面试其实是对某个岗位的招聘，很重要一点是看你是否符合和这个岗位相符合，如招聘个wms岗位开发，主要看你是不是懂wms，当然这种完全刚好符合的人其实还是比较少的。所以最重要是综合能力考察，这个综合能力就有若干因数决定，比如你的framework基础知识能力，可能你不会wms模块，但是发现你负责过的input模块还是很熟悉，对待技术的态度热情比较好，这样其实也是属于很符合情况。

4、多分享一些framework开发过程中的一些非常有技术含量的问题，像一些黑屏，闪黑，冻屏等疑难问题解决，例如：解决了系统某一个场景下的闪黑，一般面试官都对这类疑难问题比较感兴趣，大家一定要记得把自己怎么解决闪黑问题的过程描述清楚，包括分析过程，使用工具，根本原因定位，修改后如何验证，及波及问题考虑等角度全面讲述。

5、性能优化，疑难问题黑屏，闪屏，anr等，有相关经验或者亮点，简历中尽量体现，属于面试官都喜欢的一个必问的部分

  

   6、回答问题尽量简明扼要，懂的东西可以深入说出，不懂的问题可以承认自己不懂，但是也可以说出自己一些想法和思路如何对待，切勿不懂问题故意转移话题到了另一个问题上绕开询问。

面试指导，简历修改，课程优惠购买成为vip学员进入vip群，积极讨论各种行业难点痛点疑难问题，请联系马哥微信

![图片](https://mmbiz.qpic.cn/sz_mmbiz_png/DYicOkJDdA2psicybK2UNpjjHiagw9kwTgju4GQKtkwYAl5pAE7X6CJVVXDpAyAkSMvmNuUczgLk4n4xnYXkHGwMw/640?wx_fmt=other&wxfrom=5&wx_lazy=1&wx_co=1&randomid=225d6c9c&tp=webp#imgIndex=12)

详细课表

[Android Framework开发rom实战合集课表/车载车机手机高级系统开发工程必会技能](https://mp.weixin.qq.com/s?__biz=MzkzOTQ4NDUyNg==&mid=2247484186&idx=1&sn=328a6efaf16b78b1029b3595be03268b&scene=21#wechat_redirect)

[开学第一课：安卓音频框架Audio子系统实战专题--首发优惠活动](https://mp.weixin.qq.com/s?__biz=MzkzOTQ4NDUyNg==&mid=2247491623&idx=1&sn=02024a0adb7cec35f176dd6c70dd44f8&scene=21#wechat_redirect)

[重大消息：车载AAOS系统实现三分屏二分屏课程实战专题](https://mp.weixin.qq.com/s?__biz=MzkzOTQ4NDUyNg==&mid=2247493281&idx=1&sn=28785d330eb3c89b030e5c68680538e7&scene=21#wechat_redirect)


![图片](https://mmbiz.qpic.cn/sz_mmbiz_png/DYicOkJDdA2osas0xlUuOGicHsjnhibC1f59PLibaT8XRca0vysZoleXmG6iaiaB6ppyBydjRIt28ibjj4y9t6Zg23JQA/640?wx_fmt=png&from=appmsg&wxfrom=5&wx_lazy=1&wx_co=1&tp=webp#imgIndex=3)