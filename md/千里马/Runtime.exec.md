## 感谢学员朋友hh给我分享一篇关于Runtime.getRuntime().exec

## 执行命令原理剖析文章。

## 背景：

在安卓开发中，Runtime.getRuntime().exec() 是开发者执行系统命令的常用方式，比如模拟按键、调用系统工具等。看似一行简单的代码，背后却贯穿了Java层封装、Native层进程创建、Linux系统调用以及shell命令解析的完整链路。![在这里插入图片描述](https://mmbiz.qpic.cn/sz_mmbiz_png/NaxKxZyPlpYsJialwqlFI9MuaGoGyoPWoCTQZtrIiamibwob4fGiaiau1vha8CIV1ufs0OTjhDeElxDRukialTI9fRBEibr6z5E4snuVdPAfy0ZFqo/640?wx_fmt=png&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1#imgIndex=0)

## 核心流程总览

安卓App调用Runtime.exec()执行命令的完整链路，全程对应AOSP真实源码，可精准概括为以下流程，每一步均为底层执行的关键环节，缺一不可：

```python
App进程调用 Runtime.exec("input keyevent 26")    ↓Java层：Runtime.exec() → ProcessBuilder.start() → ProcessImpl.start()    ↓Java层：new UNIXProcess() → 调用Native层 UNIXProcess_forkAndExec    ↓Native层：UNIXProcess_forkAndExec        1. 解码Java参数，得到 argv = [/system/bin/sh, -c, input keyevent 26]        2. 创建4个管道（in[2]、out[2]、err[2]、fail[2]），用于父子通信与状态同步        3. 调用 startChild()，内部通过fork()系统调用创建子进程    ↓子进程执行 childProcess() 函数        1. 关闭父进程侧管道端口，避免文件描述符泄漏        2. 重定向子进程标准输入、输出、错误到对应管道，建立父子通信通道        3. 关闭子进程继承自App进程的所有无用文件描述符        4. （可选）若Java层指定工作目录，子进程切换至该目录        5. 调用 JDK_execvpe，内部最终调用execve系统调用，加载/system/bin/sh    ↓子进程被替换为shell进程（**用户与App进程一致**），解析并执行命令 "input keyevent 26"    ↓shell进程再次fork创建新子进程（用户仍与App进程一致），通过execve加载/system/bin/input，生成input进程    ↓input进程解析keyevent 26（对应电源键），通过Binder调用系统服务InputManagerService    ↓InputManagerService注入按键事件，PhoneWindowManager拦截并执行亮屏/灭屏逻辑    ↓命令执行完成，父子进程通过fail管道同步执行状态，回收相关资源
```

## 分步解析核心源码流程

### 第一步：App调用Runtime.exec()，触发Java层层层委托

当App代码中执行Runtime.getRuntime().exec(&#34;input keyevent 26&#34;)时，首先进入Java层的封装流程，核心作用是完成参数预处理和委托，为后续Native层调用做准备，全程不涉及底层进程操作。

1.  Runtime.exec()：源码路径为libcore/luni/src/main/java/java/lang/Runtime.java，作为所有命令执行的入口。该方法会先将传入的命令字符串（如&#34;input keyevent 26&#34;）解析为字符串数组，然后委托给ProcessBuilder.start()方法，主要负责参数校验、格式转换，不参与底层逻辑。
    
2.  ProcessBuilder.start()：源码路径为libcore/luni/src/main/java/java/lang/ProcessBuilder.java，是Java层与Native层之间的关键桥梁。它会将命令参数、环境变量、工作目录等信息整理后，传递给ProcessImpl.start()，完成Java层到Native层的过渡。
    
3.  ProcessImpl.start()：源码路径为libcore/luni/src/main/java/java/lang/ProcessImpl.java，是Java层调用Native层的直接入口。该方法会创建UNIXProcess实例，而UNIXProcess的构造方法会直接调用Native层的UNIXProcess\_forkAndExec函数，正式进入Native层的核心执行逻辑。
    

### 第二步：Native层UNIXProcess\_forkAndExec，完成子进程启动准备

UNIXProcess\_forkAndExec是Native层的核心入口函数，对应的源码路径为libcore/ojluni/src/main/native/UNIXProcess\_md.c（用户指定路径），其核心职责是完成参数解码、管道创建，并最终启动子进程，是连接Java层参数与子进程创建的关键。

1.  参数解码：Java层传递的命令、参数、环境变量均为byte\[\]类型，无法直接被C语言系统调用识别。UNIXProcess\_forkAndExec会通过getBytes等方法，将这些参数解码为C语言可识别的char\*类型，最终生成argv数组。结合本次场景，argv数组的具体内容为\[&#34;/system/bin/sh&#34;, &#34;-c&#34;, &#34;input keyevent 26&#34;, NULL\]，其中/system/bin/sh是安卓系统默认的shell解析器，-c参数表示执行后续的字符串命令。
    
2.  管道创建：为实现父子进程之间的通信以及命令执行状态的同步，该函数会创建4个管道，每个管道均有两个端口（读端和写端），具体作用如下：
    
3.  in\[2\]：父进程写端、子进程读端，用于父进程向子进程传递标准输入；
    
4.  out\[2\]：子进程写端、父进程读端，用于子进程向父进程传递标准输出；
    
5.  err\[2\]：子进程写端、父进程读端，用于子进程向父进程传递错误输出；
    
6.  fail\[2\]：子进程写端、父进程读端，专门用于同步exec系统调用的执行状态，告知父进程命令是否执行成功。
    
7.  启动子进程：参数解码和管道创建完成后，UNIXProcess\_forkAndExec会调用startChild函数，该函数内部会调用Linux的fork()系统调用，创建一个与父进程（App进程）完全独立的子进程。**关键强调：fork()创建的子进程，会完全继承父进程（App进程）的用户身份（UID/GID），后续所有基于该子进程衍生的进程（包括shell进程、input进程），用户身份均与App进程一致**。子进程创建成功后，会立即执行childProcess函数，而父进程则继续等待子进程的执行状态。
    

### 第三步：子进程执行childProcess，触发命令真正执行

childProcess函数与UNIXProcess\_forkAndExec同属libcore/ojluni/src/main/native/UNIXProcess\_md.c文件，是子进程的完整生命周期函数，负责完成命令执行前的所有准备工作，并最终调用exec系统调用，实现子进程的替换。

1.  清理父进程侧管道：子进程创建后，会继承父进程的管道端口，但子进程仅需要自己的读端或写端，因此会先关闭父进程侧的管道端口（in\[1\]、out\[0\]、err\[0\]、fail\[0\]），避免文件描述符泄漏。
    
2.  文件描述符重定向：为了让父子进程能够通过管道正常通信，childProcess会将子进程的标准输入（STDIN\_FILENO）、标准输出（STDOUT\_FILENO）、标准错误（STDERR\_FILENO），分别重定向到之前创建的in、out、err管道的对应端口，确保子进程的输入输出能够被父进程捕获。
    
3.  关闭无用文件描述符：子进程会继承父进程（App进程）的所有文件描述符，除了已重定向的管道端口外，其他无用的文件描述符会被逐一关闭，防止资源泄漏，避免影响系统性能。
    
4.  切换工作目录（可选）：若Java层通过ProcessBuilder指定了工作目录，childProcess会调用chdir函数，将子进程的工作目录切换到指定路径，确保命令在正确的目录下执行。
    
5.  执行命令（核心步骤）：调用JDK\_execvpe函数，该函数是安卓系统对Linux execve系统调用的封装，接收三个核心参数：argv\[0\]（要执行的程序路径，即/system/bin/sh）、argv（命令参数数组）、envv（环境变量数组）。JDK\_execvpe内部会直接调用execve系统调用，用/system/bin/sh程序的代码段、数据段、堆栈，完全覆盖子进程的原有内容。此时，子进程彻底变成shell进程——**重点澄清：此处的shell进程，并非系统级shell（如root用户的shell），其用户身份与父进程（App进程）完全一致，仅为执行命令而启动的临时shell解析进程，所有操作权限均受App进程的UID/GID限制**。原有的childProcess函数逻辑不再继续执行。
    
6.  异常处理：若execve执行失败（比如命令不存在、权限不足等），子进程会进入WhyCantJohnnyExec分支，将错误码写入fail管道，通知父进程执行失败，随后调用\_exit(-1)退出子进程，避免资源泄漏。
    

### 第四步：shell解析命令，完成最终执行流程

当childProcess中的JDK\_execvpe执行成功后，子进程已被替换为shell进程（用户与App进程一致），后续的命令解析和执行，均由该shell进程完成，具体流程如下：

1.  shell进程解析参数：shell进程启动后，会解析argv数组中的参数，明确需要执行的目标命令是input keyevent 26。由于该shell进程的用户身份与App进程一致，其解析和执行命令的权限，完全继承自App进程，无法执行App无权限的系统操作。
    
2.  创建input进程：shell进程会再次调用fork()系统调用，创建一个新的子进程，该新子进程同样继承shell进程的用户身份（即与App进程一致），随后通过execve系统调用，加载/system/bin/input程序，将新子进程替换为input进程。因此，input进程的用户身份也与App进程一致，其操作权限受App进程限制。
    
3.  注入按键事件：input进程启动后，会解析参数keyevent 26（该参数对应安卓系统的电源键），通过Binder跨进程调用，向系统服务InputManagerService发送按键注入请求。能否成功注入事件，取决于App进程是否拥有对应的权限（如INJECT\_EVENTS权限），与shell进程本身无关。
    
4.  执行系统逻辑：InputManagerService接收请求后，会将按键事件传递给PhoneWindowManager，该服务会拦截电源键事件，根据当前系统状态（亮屏/灭屏），执行对应的亮屏或灭屏逻辑，至此，命令执行完成。
    

## 注意点

-   核心组合逻辑：Runtime.exec()的本质是fork() + exec()的组合，二者缺一不可。fork()负责创建独立的子进程，复制父进程的地址空间、文件描述符以及用户身份（UID/GID）；exec()负责用目标程序（如shell、input）覆盖子进程，实现子进程的功能替换，替换后进程的用户身份保持不变。
    
-   exec系统调用特性：exec系列函数（包括execve、JDK\_execvpe）执行成功后，不会返回任何值，因为子进程的代码、数据已被新程序完全覆盖，原有逻辑无法继续执行；只有当执行失败时，才会返回错误码，进入异常处理分支。
    
-   shell进程用户身份重点强调：本文中提到的shell进程，是App进程通过fork() + exec()启动的临时解析进程，**其用户身份与启动它的App进程完全一致**，并非独立的系统shell用户。该shell进程的所有操作权限，均受App进程的UID/GID限制，无法突破App本身的权限范围，这也是安卓系统权限隔离的核心体现。
    

## 总结

安卓App调用Runtime.exec()执行命令，看似简单，实则是一个跨Java层、Native层和系统服务的复杂流程，其中shell进程的用户身份是容易误解的关键点。核心逻辑可概括为：Java层负责参数封装和委托，Native层通过UNIXProcess\_forkAndExec创建子进程（继承App用户身份），通过childProcess完成准备工作并调用exec替换子进程为shell进程（用户与App一致），最终由该shell进程解析命令、启动input进程（用户仍与App一致），注入事件并完成执行。

