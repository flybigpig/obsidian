## 使用 Ubuntu Linux 测试 Linux 驱动

### 1\. 测试 Linux 驱动准备工作

​ 对于一个 Linux 驱动程序，一开始可以在 Ubuntu Linux 上做前期开发和测试。对于访问硬件部分也可以在 Ubuntu Linux 用软件进行模拟,切记不能代替真实的环境！当基本开发完成后，就需要在开发板或工程样机上使 用真实的硬件进行测试，当然，最后还需要在最终销售的产品上进行测试。最终测试通过，Linux 驱动才能算真 正开发完成。

#### 1.1 实现步骤

1）先测试自己 ubuntu 虚拟机的版本号

```shell
sun@ubuntu:~$ uname -r
```

2）找到对应版本的内核源码编译的目录：

```shell
sun@ubuntu:/lib/modules$ ls
5.15.0-43-generic  5.15.0-56-generic  5.19.0-32-generic
5.15.0-53-generic  5.15.0-60-generic  5.19.0-35-generic

sun@ubuntu:/lib/modules$ cd 5.19.0-32-generic
sun@ubuntu:/lib/modules/5.19.0-32-generic$ ls
build              modules.builtin.alias.bin  modules.order
initrd             modules.builtin.bin        modules.softdep
kernel             modules.builtin.modinfo    modules.symbols
modules.alias      modules.dep                modules.symbols.bin
modules.alias.bin  modules.dep.bin            vdso
modules.builtin    modules.devname

sun@ubuntu:/lib/modules/5.19.0-32-generic$ cd build
sun@ubuntu:/lib/modules/5.19.0-32-generic/build$ ls
arch    Documentation  init      Kconfig   mm              scripts   ubuntu
block   drivers        io_uring  kernel    Module.symvers  security  usr
certs   fs             ipc       lib       net             sound     virt
crypto  include        Kbuild    Makefile  samples   

```

3）修改测试驱动程序的 Makefile 文件

```shell
KERNEL_DIR = /lib/modules/5.19.0-32-generic/build #ubuntu 内核目录
CUR_DIR = $(shell pwd)
MYAPP = virtdev_test
all:
	make -C $(KERNEL_DIR) M=$(CUR_DIR) modules
	gcc -o $(MYAPP) $(MYAPP).c
clean:
	make -C $(KERNEL_DIR) M=$(CUR_DIR) clean
	rm -rf $(MYAPP)
obj-m = virtdev_drv.o
```

### 2\. 测试驱动程序

#### 2.1 编写驱动程序 virtdev\_drv.c

```c
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
// 定义设备文件名
#define DEVICE_NAME "virtdev"
static int atom = 0;
// 参数 atom=0: 多个进程可以同时打开 vritdev 设备文件
// 参数 atom 非 0：同时只能有一个进程打开 virtdev 设备文件
static atomic_t int_atomic_available = ATOMIC_INIT(1);
// 原子变量值为 1
static int virtdev_open(struct inode *node, struct file *file)
{
    if (atom)
    {
        if (!atomic_dec_and_test(&int_atomic_available))
        {
            atomic_inc(&int_atomic_available);
            return -EBUSY;
        }
    }
    return 0;
}

static int virtdev_close(struct inode *node, struct file *file)
{
    if (atom)
    {
        atomic_inc(&int_atomic_available);
    }
    return 0;
}

static struct file_operations dev_fops =
{
    .owner = THIS_MODULE,
    .open = virtdev_open,
    .release = virtdev_close
};

static struct miscdevice misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &dev_fops
};

// 初始化 Linux 驱动
static int __init virtdev_init(void)
{
    // 建立设备文件
    int ret = misc_register(&misc);
    printk("<jaylen> atomic=%d\n", atom);
    printk("virtdev_init_success\n");
    return ret;
}

// 卸载 Linux 驱动
static void __exit virtdev_exit(void)
{
    printk("<jaylen> atomic=%d\n", atom);
    printk("<jaylen> virtdev_exit_success\n");
    // 删除设备文件
    misc_deregister(&misc);
}

// 注册初始化 Linux 驱动的函数
module_init(virtdev_init);
// 注册卸载 Linux 驱动的函数
module_exit(virtdev_exit);

module_param(atom, int, S_IRUGO | S_IWUSR);
MODULE_LICENSE("GPL");

```

#### 2.2 编写应用程序 virtdev\_test.c

```c
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
int main(int argc, char *argv[])
{
    printf("app pid=%d is running\n", getpid());
    int fd = open("/dev/virtdev", O_RDWR);
    printf("ret:%d\n", fd);
    if (fd < 0)
    {
        printf("errno:%d\n", errno);
    }
    else
    {
        sleep(10);
        close(fd);
    }
    printf("app pid=%d is over\n", getpid());
    return 0;
}
```

#### 2.3 编译驱动

在 ubuntu 普通用户下完成编译驱动

```shell
un@ubuntu:ubuntu_drv_test$ make
make -C /lib/modules/5.19.0-32-generic/build M=/home/sun/work/ubuntu_drv_test modules
make[1]: 进入目录“/usr/src/linux-headers-5.19.0-32-generic”
warning: the compiler differs from the one used to build the kernel
  The kernel was built by: x86_64-linux-gnu-gcc (Ubuntu 11.3.0-1ubuntu1~22.04) 11.3.0
  You are using:           gcc (Ubuntu 11.3.0-1ubuntu1~22.04) 11.3.0
  CC [M]  /home/sun/work/ubuntu_drv_test/virtdev_drv.o
  MODPOST /home/sun/work/ubuntu_drv_test/Module.symvers
  CC [M]  /home/sun/work/ubuntu_drv_test/virtdev_drv.mod.o
  LD [M]  /home/sun/work/ubuntu_drv_test/virtdev_drv.ko
  BTF [M] /home/sun/work/ubuntu_drv_test/virtdev_drv.ko
Skipping BTF generation for /home/sun/work/ubuntu_drv_test/virtdev_drv.ko due to unavailability of vmlinux
make[1]: 离开目录“/usr/src/linux-headers-5.19.0-32-generic”
gcc -o virtdev_test virtdev_test.c
```

#### 1.2.4 测试驱动

在 ubuntu 下需要切换到管理员用户下才能安装测试驱动

```shell
sun@ubuntu:ubuntu_drv_test$ sudo su
root@ubuntu:/home/sun/work/ubuntu_drv_test# ls
hello_test     Module.symvers  virtdev_drv.mod    virtdev_drv.o
Makefile       virtdev_drv.c   virtdev_drv.mod.c  virtdev_test
modules.order  virtdev_drv.ko  virtdev_drv.mod.o  virtdev_test.c


```

安装驱动

```shell
root@ubuntu:/home/sun/work/ubuntu_drv_test# insmod virtdev_drv.ko 
```

传参加载驱动

```shell
root@ubuntu:/home/sun/work/ubuntu_drv_test# insmod virtdev_drv.ko atom=1
```

打印并清空缓冲区日志 `dmesg -c`, 以下是测试过程

**驱动加载过程**

```
root@ubuntu:/home/sun/work/ubuntu_drv_test# dmesg
[25849.067381] <robin> atomic=0
[25849.067387] <robin> virtdev_exit_success
[25867.980387] <robin> atomic=1
[25867.980395] virtdev_init_success

root@ubuntu:/home/sun/work/ubuntu_drv_test# lsmod
Module                  Size  Used by
virtdev_drv            16384  1

root@ubuntu:/home/sun/work/ubuntu_drv_test# ls /dev/virtdev -al
crw------- 1 root root 10, 121  3月 11 22:14 /dev/virtdev

```

**用户测试**

```shell
sun@ubuntu:ubuntu_drv_test$ sudo ./virtdev_test 
app pid=43934 is running
ret:3
app pid=43934 is over

```

**驱动卸载**

```shell
root@ubuntu:/home/sun/work/ubuntu_drv_test# rmmod virtdev_drv
root@ubuntu:/home/sun/work/ubuntu_drv_test# dmesg 
[25849.067381] <robin> atomic=0
[25849.067387] <robin> virtdev_exit_success
[25867.980387] <robin> atomic=1
[25867.980395] virtdev_init_success
[26054.089448] <robin> atomic=1
[26054.089456] <robin> virtdev_exit_success

```

### 3 Linux 内核日志查看之 dmesg 命令简介

Linux 内核启动时会加载硬件驱动，在有新硬件时也会加载驱动，如果想要查看内核的活动，可以使用 dmesg 命令。Linux 内核日志存储在一个 ring-buffer 中，它的大小是固定的，当队列满时，新的消息会覆盖掉最旧的消 息。实际上，在 boot 阶段，所有的应用还没有启动，syslogd 也未启动，这时内核日志是非常重要的信息。 除了设备初始化日志、内核模块日志，它还会包含一些应用崩溃的相关信息记录，了解 dmesg 的使用对于调 试程序相当重要。

对于嵌入式设备的调试，它会比较清楚地展现当前的 log 信息。 `dmesg -c` 显示并清除当前的日志内容。 下次再 dmesg 时就没有以前的日志了。