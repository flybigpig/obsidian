

在Android系统中，USAP pool是指非专用应用进程池（Unspecialized App Process pool）。  
  
从Android Q（10）开始，Google引入了USAP机制，通过预fork的方式提前创建好一批进程，当有应用启动时，直接将已经创建好的进程分配给它，从而省去了fork的动作，以提升应用的启动性能。以下是其相关工作原理：  
- **进程创建与等待**：Zygote会首先fork出10个进程并加入到USAP pool中。这些USAP进程在等待socket通信到来前，会提升调度优先级，以便能快速响应应用的启动请求。  
- **应用启动分配**：当AMS接收到启动需求并决定采用USAP方式启动时，system_server会发起socket通信，将启动参数发送给USAP进程。系统底层会随机唤醒一个USAP进程来处理此次通信，该进程调用specializeAppProcess完成相关工作，最终进入ActivityThread.main方法，进而完成应用的启动。  
- **进程回收与补充**：当USAP进程退出时，会直接被zygote回收。zygote接收到SIGCHLD信号后，会调用SigChldHandler进行处理。USAP Pool中设定了两个阈值，对应两种refill方式。当pool中剩余进程数量不超过一半（5个）时，会发起一次延迟3秒的Delayed Refill，以避免与应用启动过程抢夺系统资源。如果在延迟期间，USAP Pool中的进程消耗殆尽，zygote会立即发起immediate fork，先fork出一个进程补充到pool中，同时再安排一次Delayed Refill用于完整填充。  
  
USAP的开启/关闭通过属性实现，可以在build.prop中增加相关行，或者获取root权限后调用setprop设置。