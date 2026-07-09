
```c
#include <linux/init.h>
#include <linux/module.h>

static int hello_init(void)
{
    printk(KERN_INFO "Hello, world\n");
    return 0;
}

static void hello_exit(void)
{
    printk(KERN_INFO "Goodbye cruel world\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
 
```

```makefile
% make 
make[1]: Entering directory `/usr/src/linux-2.6.10' 
 CC [M] /home/ldd3/src/misc-modules/hello.o 
 Building modules, stage 2. 
 MODPOST 
 CC /home/ldd3/src/misc-modules/hello.mod.o  
 LD [M] /home/ldd3/src/misc-modules/hello.ko 
make[1]: Leaving directory `/usr/src/linux-2.6.10' 
% su 
root# insmod ./hello.ko 
Hello, world 
root# rmmod hello 
Goodbye cruel world root# 

```

```
注
这是一个经典的 Linux 内核模块（LKM） 开发示例，来自《Linux Device Drivers 3》(LDD3) 书中的 "hello world" 模块。

运行流程解析：

make — 编译内核模块

CC 编译 hello.o 目标文件
MODPOST 模块后处理
CC 编译 hello.mod.o
LD 链接生成最终的 hello.ko 内核模块
insmod ./hello.ko — 加载模块到内核

触发模块的 init 函数，输出：Hello, world
rmmod hello — 从内核卸载模块

触发模块的 exit 函数，输出：Goodbye cruel world
```

一旦你已建立起所有东西, 给你的模块创建一个 makefile 就是直截了当的. 实际上, 对
于本章前面展示的" hello world" 例子, 单行就够了: 
obj-m := hello.o 
熟悉 make , 但是对 2.6 内核建立系统不熟悉的读者, 可能奇怪这个 makefile 如何工作. 
毕竟上面的这一行不是一个传统的 makefile 的样子. 答案, 当然, 是内核建立系统处理了余下的工作. 上面的安排( 它利用了由 GNU make 提供的扩展语法 )表明有一个模块要从目标文件 hello.o 建立. 在从目标文件建立后结果模块命名为 hello.ko. 
反之, 如果你有一个模块名为 module.ko, 是来自 2 个源文件( 姑且称之为, file1.c 和 
file2.c ), 正确的书写应当是: 
``` makefile
obj-m := module.o 
module-objs := file1.o file2.o 
```
对于一个象上面展示的要工作的 makefile, 它必须在更大的内核建立系统的上下文被调用. 
如果你的内核源码数位于, 假设, 你的 ~/kernel-2.6 目录, 用来建立你的模块的 make 
命令( 在包含模块源码和 makefile 的目录下键入 )会是: 
```makefile
make -C ~/kernel-2.6 M=`pwd` modules 
```
这个命令开始是改变它的目录到用 -C 选项提供的目录下( 就是说, 你的内核源码目录 ). 它在那里会发现内核的顶层 makefile. 这个 M= 选项使 makefile 在试图建立模块目标前, 回到你的模块源码目录. 这个目标, 依次地, 是指在 obj-m 变量中发现的模块列表, 在我们的例子里设成了 module.o. 
键入前面的 make 命令一会儿之后就会感觉烦, 所以内核开发者就开发了一种 makefile 方式, 使得生活容易些对于那些在内核树之外建立模块的人. 这个窍门是如下书写你的 makefile: 
```makefile
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
obj-m := hello.o

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif

```


```
执行流程：

第一次调用（用户在命令行运行 make）

    KERNELRELEASE 未定义 → 进入 else 分支
    $(MAKE) -C $(KERNELDIR) 跳转到内核源码树目录执行构建
    M=$(PWD) 告诉 kbuild 回到当前目录查找模块源码
    kbuild 会再次调用当前目录的 Makefile

第二次调用（由 kbuild 系统调用）

    KERNELRELEASE 已定义 → 进入 ifneq 分支
    obj-m := hello.o 告诉 kbuild 将 hello.c 编译为可加载模块

关键变量：

    KERNELDIR — 内核构建目录，通常指向 /lib/modules/$(uname -r)/build
    M=$(PWD) — 外部模块源码路径
    obj-m — 编译为模块（obj-y 则编入内核映像）

```

再一次, 我们看到了扩展的 GNU make 语法在起作用. 这个 makefile 在一次典型的建立中要被读 2 次. 当从命令行中调用这个 makefile , 它注意到 KERNELRELEASE 变量没有设置. 它利用这样一个事实来定位内核源码目录, 即已安装模块目录中的符号连接指回内核建立树. 如果你实际上没有运行你在为其而建立的内核, 你可以在命令行提供一个 
KERNELDIR= 选项, 设置 KERNELDIR 环境变量, 或者重写 makefile 中设置 KERNELDIR 的那一行. 一旦发现内核源码树, makefile 调用 default: 目标, 来运行第 2 个 make 命令( 在 makefile 里参数化成 $(MAKE))象前面描述过的一样来调用内核建立系统. 在第 2 次读, makefile 设置 obj-m, 并且内核的 makefile 文件完成实际的建立模块工作. 