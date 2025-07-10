## \[Perfetto\]应用冷启动流程分析

最新推荐文章于 2025-05-18 15:27:14 发布

![](https://csdnimg.cn/release/blogv2/dist/pc/img/original.png)

[坂田民工](https://blog.csdn.net/qq_40731414 "坂田民工") ![](https://csdnimg.cn/release/blogv2/dist/pc/img/newCurrentTime2.png) 最新推荐文章于 2025-05-18 15:27:14 发布

版权声明：本文为博主原创文章，遵循 [CC 4.0 BY-SA](http://creativecommons.org/licenses/by-sa/4.0/) 版权协议，转载请附上原文出处链接和本声明。

![](https://img-home.csdnimg.cn/images/20240711042549.png) 当点击launcher图标启动应用时，事件从eventhub开始传递，经过inputdispatcher分发，唤醒launcher3进程。launcher3通过AMS跨进程启动新应用，zygotefork出新进程，执行ActivityThread的main方法。应用完成初始化、生命周期回调，绘制第一帧后，冷启动过程结束。

摘要生成于 [C知道](https://ai.csdn.net/?utm_source=cknow_pc_ai_abstract) ，由 DeepSeek-R1 满血版支持， [前往体验 >](https://ai.csdn.net/?utm_source=cknow_pc_ai_abstract)

应用冷启动流程分析

> hongxi.zhu 2023-3-4场景：点击lanucher图标启动应用

1.  在eventhub中，当驱动获取到触摸事件时，eventhub中通过epoll就会监听到有event可读，然后就会唤醒inputreader线程，读取事件并处理，然后inputreader会回调inputdispatcher的notifyMotion方法，将时间加入iq队列，然后通知inputdispatcher处理，inputdispatcher首先会去获取当前的焦点window, 然后将时间op队列后分发，分发完成后，将事件加入wq队列，进行事件的处理超时检测。  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/177b2b1e06d6e9f3f05548a5193aaaca.png)
    
2.  inputdeispatcher通过往目标窗口inputchannel发送数据，唤醒目标应用主线程，这里应用是launcher3, launcher3会从socket中读取对应的事件，并开始分发，其中主要还是往view树上分发，然后一直到点击事件的处理，通过startActivity启动新的应用的开始。  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/570fe88a55d59d4bb4a6c585a68cf3b8.png)
    
3.  launcher3通过跨进程调用AMS启动应用的流程，首先是先pause launcher3, 因为目标应用是冷启动，需要通过socket方式通知zygote先fork出新的应用的进程，新应用进程执行自己ActivityThread的main方法，初始化自己的消息队列等，然后通过跨进程bindApplication通知AMS，将该应用加入管理记录，然后继续跨进程通知应用往下执行相应的生命周期。  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/2f9daa0750b82aefd7d3a0d001ec0b95.png)
    
4.  应用执行完相应的生命周期后，开始第一帧绘制，然后显示到屏幕上，冷启动完成  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/af2e69a220391032bfb510bda97e509e.png)