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
