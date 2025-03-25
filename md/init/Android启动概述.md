## [Android系列文章目录](https://juejin.cn/post/7440334843055030306 "https://juejin.cn/post/7440334843055030306")

BootRom->BootLoader->Linux Kernel->Init->Zygote->SystemServer->Launcher

- BootLoader层：主要包括Boot Rom和Boot Loader
- Kernel层：主要是Android内核层
- Native层：主要是包括init进程以及其fork出 来的用户空间的守护 进程、HAL层、开机动画等
- JAVA Framework层：主要是AMS和PMS等Service的初始化
- Application层：主要指SystemUI、Launcher的启动

Android架构图如下（图片来源于Gityuan）

![启动架构图.jpg](https://p3-xtjj-sign.byteimg.com/tos-cn-i-73owjymdk6/f283754e4acf4745a66caee41b1679e1~tplv-73owjymdk6-jj-mark-v1:0:0:0:0:5o6Y6YeR5oqA5pyv56S-5Yy6IEAg5LyB6bmF5ZCb5oqA5pyv5ZyI:q75.awebp?rk3s=f64ab15b&x-expires=1743239925&x-signature=W6sHOecvRA254ZGOnKJqAsp%2F4o0%3D)

Android 系统启动流程：

- 第一步：手机开机后，引导芯片启动，引导芯片开始从固化在 ROM里的预设代码执行，加载引导程序到到RAM，bootloader检 查RAM，初始化硬件参数等功能；
- 第二步：硬件等参数初始化完成后，进入到Kernel层，Kernel层 主要加载一些硬件设备驱动，初始化进程管理等操作。在Kernel 中首先启动swapper进程（pid=0），用于初始化进程管理、内管 管理、加载Driver等操作，再启动kthread进程(pid=2),这些linux 系统的内核进程，kthread是所有内核进程的鼻祖；
- 第三步：Kernel层加载完毕后，硬件设备驱动与HAL层进行交互。 初始化进程管理等操作会启动INIT进程 ，这些在Native层中；
- 第四步：init进程(pid=1，init进程是所有进程的鼻祖，第一个启 动)启动后，会启动adbd，logd等用户守护进程，并且会启动 servicemanager(binder服务管家)等重要服务，同时孵化出 zygote进程，这里属于C++ Framework，代码为C++程序；
- 第五步：zygote进程是由init进程解析init.rc文件后fork生成，它 会加载虚拟机，启动System Server(zygote孵化的第一个进程)； System Server负责启动和管理整个Java Framework，包含 ActivityManager，WindowManager，PackageManager， PowerManager等服务；
- 第六步：zygote同时会启动相关的APP进程，它启动的第一个APP 进程为Launcher，然后启动Email，SMS等进程，所有的APP进程 都有zygote fork生成。

  

作者：企鹅君技术圈  
链接：https://juejin.cn/post/7419482499598499850  
来源：稀土掘金  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。


---


  