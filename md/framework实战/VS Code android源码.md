
本文可以转载，请注明来源：[https://blog.csdn.net/zhaojia92](https://blog.csdn.net/zhaojia92)

Android系统源码自带调试脚本gdbclient.py可调试HAL层和Native程序，该工具免除了手动配置gdb的麻烦。gdbclient.py底层调用adb与手机建立连接，调用linux gdb远程连接手机端的gdbserver进行调试；由于是在字符界面调试，有时会对查看和阅读上下文代码造成不便，android source官网提供了使用VSCode作为前端调试C++代码的方法，经尝试效果不错。

使用VS Code作为前端调试Android HAL/Native代码的官方说明见：

[https://source.android.google.cn/devices/tech/debug/gdb?hl=zh-cn](https://source.android.google.cn/devices/tech/debug/gdb?hl=zh-cn)

* * *

以下为使用VS Code调试相机Camera2消息处理功能的过程（Android Q版本）

一. 下载编译Android源码

repo拉取android源码后使用make编译，经过漫长的等待终于编译完成。（注意：若使用emulator调试，编译时选择与host平台一致的编译版本可加快emulator运行；若使用手机调试，编译时选择手机平台即可。用i7-7700首次编译使用make -j8耗费了3个多小时。）

二. 运行模拟器查看系统是否可以运行

新开一个终端，输入“source build/envsetup.sh”导入环境变量，再输入“lunch 目标平台”设置编译运行环境如下：

![](https://i-blog.csdnimg.cn/blog_migrate/440a381035379bd3d64897e5f928c5fb.jpeg)

在该终端内输入emulator启动模拟器，发现系统在模拟器中可以运行。

![](https://i-blog.csdnimg.cn/blog_migrate/ecdf044e41eda9127fc7a3bf1647388e.jpeg)

三、使用adb查看要调试的进程pid

不要关闭模拟器，源码路径另开一终端，输入adb shell进入手机控制台，输入ps -A | grep -i camera查看所有camera相关进程。

![](https://i-blog.csdnimg.cn/blog_migrate/5479409835ab83782d097bdc1fc578bb.png)

由于Camera2消息处理流程在cameraserver进程中（位于frameworks/av/camera/camera2/CaptureRequest.cpp中，查看编译文件Android.mk、Android.bp可找到其对应关系）, 确定其pid=3795，exit退出手机控制台。

在此终端内输入“source build/envsetup.sh”和“lunch 目标平台”配置运行环境。

输入gdbclient.py -p 3795 --setup-forwarding vscode 以建立调试环境，终端内返回JSON配置内容，然后gdbclient.py等待enter退出。此时保持gdbclient不要退出，将JSON内容复制出来。

![](https://i-blog.csdnimg.cn/blog_migrate/bea2ff2dcc681f06ceafaf6524523ba7.png)

四、启动VS Code调试源码

启动VS Code，打开Workspace目录为android源码路径，点击菜单Debug->Open Configurations新建一个C++(GDB/LLDB) lunch.json文件。

![](https://i-blog.csdnimg.cn/blog_migrate/cad569f99b24f26ae3ebb549cf1b6edd.png)

删除该文件中自动创建的配置，将第三步拷贝的JSON内容粘贴到"configurations"字段内,  完成后如上图所示。Ctrl+S保存后关闭该lunch.json。

在Android源码中加入断点，点击菜单Debug->Start Debugging启动调试，等待VS Code调用gdb与模拟器建立连接，此时gdb自动搜索加载所需so文件和符号表，跟踪程序运行。在VS Code调试窗口可见gdb运行过程。

模拟器中开启相机程序Camera2，点击拍照按钮。VS Code命中断点，此时可查看相关变量、堆栈、汇编指令，并可单步调试。

![](https://i-blog.csdnimg.cn/blog_migrate/811320159b2f6a8a383ae98d0963a9da.jpeg)

![](https://i-blog.csdnimg.cn/blog_migrate/81f2ad192c32fa9d915d8380e76b998d.jpeg)

调试完成后关闭VS Code调试，再关闭gdbclient.py，最后停止emulator程序。

每次按上述方法开启gdbclient.py, 都会生成不同的JSON配置（其远程连接端口会改变），因此VS Code开启新调试前务必确认lunch.json文件是否与gdbclient.py生成的一致。