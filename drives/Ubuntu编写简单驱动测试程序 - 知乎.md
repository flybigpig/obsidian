参考学习：[ubuntu下调试驱动\_ubuntu 编译驱动\_Engineer-Jaylen\_Sun的博客-CSDN博客](https://link.zhihu.com/?target=https%3A//blog.csdn.net/weixin_40209493/article/details/129469828%23%3A~%3Atext%3D%25E4%25BD%25BF%25E7%2594%25A8%2520Ubuntu%2520Linux%2520%25E6%25B5%258B%25E8%25AF%2595%2520Linux%2520%25E9%25A9%25B1%25E5%258A%25A8%25201%25201.%2C...%25202%25202.%2520%25E6%25B5%258B%25E8%25AF%2595%25E9%25A9%25B1%25E5%258A%25A8%25E7%25A8%258B%25E5%25BA%258F%25202.1%2520%25E7%25BC%2596%25E5%2586%2599%25E9%25A9%25B1%25E5%258A%25A8%25E7%25A8%258B%25E5%25BA%258F%2520virtdev_drv.c%2520)

另外可以阅读之前的文章加强学习：

直接源码解析

## 源码解析

### virhello.c

```
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

### virhello\_test.c

```
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

### Makefile

```
KERNDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

obj-m:=virhello.o

all:
make -C $(KERNDIR) M=$(PWD) modules

clean:
make -C $(KERNDIR) M=$(PWD) clean
```

## 详细步骤

1.  make直接编译makefile,将生成一系列的ko等文件

![](https://pic4.zhimg.com/v2-108766b960b2336be0e043625e44a7c3_r.jpg)

2\. 编译可执行文件

```
gcc virhello_test.c -o virhello_test
```

3\. 切到root权限，安装测试驱动

![](https://pic4.zhimg.com/v2-14b740b7b14e521e650fe207d382a263_r.jpg)

4\. 传参加载驱动

```
insmod virhello.ko atom=1
```

5\. 查看驱动加载过程

![](https://pic3.zhimg.com/v2-7c170d86acb1091e1434833e12ee1e96_r.jpg)

6\. 用户测试

![](https://pic1.zhimg.com/v2-2ad9a7112c71a9e55fd7c123eb15d31c_r.jpg)

7\. 驱动卸载，还是要进root

![](https://pic4.zhimg.com/v2-f30675fef017f3b41fe0175c64194f87_r.jpg)

8\. 最后再看一下驱动的流程，demsg

![](https://pic4.zhimg.com/v2-3d382748054de8104ee2fea00ee5453b_r.jpg)

## 总结

本期主要是对设备驱动程序的测试案例。主要是对module\_init和module\_exit的学习。