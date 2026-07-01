The user is asking about adding a native service in the Linux kernel context. This is different from the Android framework native service we discussed before. In the Linux kernel, a "native service" typically means:

1. A kernel module (loadable kernel module, .ko)
2. A kernel thread (kthread)
3. A character device driver that exposes ioctl/read/write interfaces to userspace

Let me think about what the user likely wants. They said "linux kernel 添加 native 服务" - this could mean:

1. Adding a kernel module that provides a service
2. Adding a kernel thread
3. Adding a misc/char device driver

Since the existing document is about Android framework services, and the user is now asking about Linux kernel level services, this is a different topic. Let me provide comprehensive guidance on adding native services at the Linux kernel level.

Key topics to cover:
1. Kernel module (loadable kernel module)
2. Kernel thread (kthread)
3. Character device driver (for userspace interaction)
4. Proc/sysfs interface
5. Netlink socket for kernel-userspace communication
6. Build system integration (Kconfig, Makefile)

Let me write this out as a comprehensive guide that could be added to the existing document or as a standalone section.

Actually, looking at the context more carefully, the user has been working on an Obsidian note about Android framework services. They now want to add content about Linux kernel native services. This is a distinct topic from the Android framework services.

Let me provide the content directly since the diff tool has been having issues. I'll present it as a new section or a standalone guide.

Let me think about the structure:

## Linux Kernel 添加 Native 服务

### 1. 概述
- Kernel module vs built-in
- When to use kernel-level service

### 2. 内核模块开发
- Hello World kernel module
- Kconfig + Makefile integration
- Module parameters

### 3. 内核线程 (kthread)
- Creating kernel threads
- Thread lifecycle

### 4. 字符设备驱动
- Registering char device
- file_operations
- ioctl interface

### 5. Proc/sysfs 接口
- /proc interface
- /sys (sysfs) interface

### 6. Netlink 通信
- Kernel-side netlink
- Userspace client

### 7. 与 Android 的集成
- Init RC 启动 userspace 守护进程
- 与 binder 的关系

Let me write this up concisely but comprehensively.
这是一个与 Android Framework 不同层面的话题——在 **Linux Kernel** 层添加 native 服务。下面是完整指南，你可以将其添加到现有文档或作为独立笔记。

---

# Linux Kernel 添加 Native 服务

## 1. Kernel 层服务的几种形态

| 形态 | 适用场景 | 用户态交互方式 |
|------|---------|--------------|
| **A. 可加载内核模块 (LKM)** | 动态扩展内核功能，无需重启 | insmod/rmmod，通过设备文件交互 |
| **B. 内核线程 (kthread)** | 后台常驻任务（如数据采集、监控） | 通过 proc/sysfs/netlink |
| **C. 字符设备驱动** | 提供 ioctl/read/write 接口给用户态 | /dev/mydev，open/ioctl/read/write |
| **D. Platform Driver** | 绑定硬件设备（设备树匹配） | 通过 misc device 或 class 设备 |
| **E. Netlink 套接字** | 内核↔用户态双向异步通信 | socket(AF_NETLINK) |

## 2. 目录结构

```
drivers/
└── mycompany/
    ├── my_service/
    │   ├── my_service.c          # 内核模块主文件
    │   ├── my_service_thread.c   # 内核线程
    │   ├── my_service_dev.c      # 字符设备
    │   ├── my_service_netlink.c  # Netlink 通信
    │   ├── Kconfig               # 菜单配置
    │   └── Makefile              # 编译规则
    ├── Kconfig                   # 引用子目录
    └── Makefile
```

## 3. 最小可加载内核模块

**`drivers/mycompany/my_service/my_service.c`**

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int __init my_service_init(void)
{
    pr_info("my_service: module loaded\n");
    return 0;
}

static void __exit my_service_exit(void)
{
    pr_info("my_service: module unloaded\n");
}

module_init(my_service_init);
module_exit(my_service_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MyCompany");
MODULE_DESCRIPTION("My Custom Kernel Service");
```

## 4. Kconfig + Makefile 集成

**`drivers/mycompany/my_service/Kconfig`**

```kconfig
config MY_SERVICE
    tristate "My Custom Kernel Service"
    default m
    help
      This is a custom kernel service for demonstration.
      Can be built as module (M) or built-in (Y).

config MY_SERVICE_THREAD
    bool "Enable kernel thread for My Service"
    depends on MY_SERVICE
    default y

config MY_SERVICE_NETLINK
    bool "Enable Netlink communication for My Service"
    depends on MY_SERVICE
    default y
```

**`drivers/mycompany/my_service/Makefile`**

```makefile
obj-$(CONFIG_MY_SERVICE) += my_service.o
obj-$(CONFIG_MY_SERVICE_THREAD) += my_service_thread.o
obj-$(CONFIG_MY_SERVICE_NETLINK) += my_service_netlink.o
```

**`drivers/mycompany/Kconfig`**（上层引用）

```kconfig
source "drivers/mycompany/my_service/Kconfig"
```

**`drivers/mycompany/Makefile`**（上层引用）

```makefile
obj-$(CONFIG_MY_SERVICE) += my_service/
```

**修改 `drivers/Kconfig`**，在末尾追加：

```kconfig
source "drivers/mycompany/Kconfig"
```

**修改 `drivers/Makefile`**，在末尾追加：

```makefile
obj-$(CONFIG_MY_SERVICE) += mycompany/
```

## 5. 内核线程 (kthread)

**`drivers/mycompany/my_service/my_service_thread.c`**

```c
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/module.h>

static struct task_struct *my_thread;

static int my_service_thread_fn(void *data)
{
    pr_info("my_service: kernel thread started\n");

    while (!kthread_should_stop()) {
        pr_debug("my_service: thread heartbeat\n");

        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(msecs_to_jiffies(1000));
    }

    pr_info("my_service: kernel thread exiting\n");
    return 0;
}

int my_service_thread_start(void)
{
    my_thread = kthread_create(my_service_thread_fn, NULL, "my_svc_thread");
    if (IS_ERR(my_thread)) {
        pr_err("my_service: failed to create kernel thread\n");
        return PTR_ERR(my_thread);
    }
    wake_up_process(my_thread);
    return 0;
}

void my_service_thread_stop(void)
{
    if (my_thread) {
        kthread_stop(my_thread);
        my_thread = NULL;
    }
}
```

在 `my_service.c` 中调用：

```c
extern int my_service_thread_start(void);
extern void my_service_thread_stop(void);

static int __init my_service_init(void)
{
    int ret;
    pr_info("my_service: module loaded\n");
    ret = my_service_thread_start();
    if (ret)
        return ret;
    return 0;
}

static void __exit my_service_exit(void)
{
    my_service_thread_stop();
    pr_info("my_service: module unloaded\n");
}
```

## 6. 字符设备驱动（用户态交互）

**`drivers/mycompany/my_service/my_service_dev.c`**

```c
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define MY_SERVICE_MINOR  0
#define MY_SERVICE_COUNT  1
#define MY_SERVICE_NAME   "my_service"

struct my_service_dev {
    struct cdev cdev;
    struct class *class;
    dev_t devno;
};

static struct my_service_dev my_dev;

static long my_service_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    pr_info("my_service: ioctl cmd=%u\n", cmd);
    switch (cmd) {
    case 0x01:
        pr_info("my_service: CMD_SET received\n");
        break;
    case 0x02:
        pr_info("my_service: CMD_GET received\n");
        break;
    default:
        return -ENOTTY;
    }
    return 0;
}

static int my_service_open(struct inode *inode, struct file *filp)
{
    pr_info("my_service: device opened\n");
    return 0;
}

static int my_service_release(struct inode *inode, struct file *filp)
{
    pr_info("my_service: device closed\n");
    return 0;
}

static const struct file_operations my_service_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = my_service_ioctl,
    .open           = my_service_open,
    .release        = my_service_release,
};

int my_service_dev_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&my_dev.devno, MY_SERVICE_MINOR,
                              MY_SERVICE_COUNT, MY_SERVICE_NAME);
    if (ret < 0) {
        pr_err("my_service: alloc_chrdev_region failed\n");
        return ret;
    }

    cdev_init(&my_dev.cdev, &my_service_fops);
    my_dev.cdev.owner = THIS_MODULE;
    ret = cdev_add(&my_dev.cdev, my_dev.devno, MY_SERVICE_COUNT);
    if (ret) {
        unregister_chrdev_region(my_dev.devno, MY_SERVICE_COUNT);
        return ret;
    }

    my_dev.class = class_create(MY_SERVICE_NAME);
    if (IS_ERR(my_dev.class)) {
        cdev_del(&my_dev.cdev);
        unregister_chrdev_region(my_dev.devno, MY_SERVICE_COUNT);
        return PTR_ERR(my_dev.class);
    }

    device_create(my_dev.class, NULL, my_dev.devno, NULL, MY_SERVICE_NAME);
    pr_info("my_service: char device /dev/%s created [major=%d]\n",
            MY_SERVICE_NAME, MAJOR(my_dev.devno));
    return 0;
}

void my_service_dev_exit(void)
{
    device_destroy(my_dev.class, my_dev.devno);
    class_destroy(my_dev.class);
    cdev_del(&my_dev.cdev);
    unregister_chrdev_region(my_dev.devno, MY_SERVICE_COUNT);
    pr_info("my_service: char device removed\n");
}
```

### 用户态调用示例

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

int main() {
    int fd = open("/dev/my_service", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    ioctl(fd, 0x01, 0);   /* CMD_SET */
    ioctl(fd, 0x02, 0);   /* CMD_GET */

    close(fd);
    return 0;
}
```

## 7. Netlink 通信（内核↔用户态双向）

**`drivers/mycompany/my_service/my_service_netlink.c`**

```c
#include <linux/netlink.h>
#include <net/sock.h>
#include <linux/skbuff.h>

#define NETLINK_MY_SERVICE  31

static struct sock *nl_sock;

static void my_service_nl_recv(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    int pid;
    struct sk_buff *skb_out;
    char *msg = "ACK from kernel";
    int msg_size = strlen(msg);

    nlh = (struct nlmsghdr *)skb->data;
    pid = nlh->nlmsg_pid;

    pr_info("my_service: netlink recv from pid=%d: %s\n",
            pid, (char *)nlmsg_data(nlh));

    skb_out = nlmsg_new(msg_size, GFP_KERNEL);
    if (!skb_out)
        return;

    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    NETLINK_CB(skb_out).dst_group = 0;
    strncpy(nlmsg_data(nlh), msg, msg_size);

    nlmsg_unicast(nl_sock, skb_out, pid);
}

int my_service_netlink_init(void)
{
    struct netlink_kernel_cfg cfg = {
        .input = my_service_nl_recv,
    };

    nl_sock = netlink_kernel_create(&init_net, NETLINK_MY_SERVICE, &cfg);
    if (!nl_sock) {
        pr_err("my_service: netlink kernel create failed\n");
        return -ENOMEM;
    }
    pr_info("my_service: netlink socket created (proto=%d)\n", NETLINK_MY_SERVICE);
    return 0;
}

void my_service_netlink_exit(void)
{
    if (nl_sock)
        netlink_kernel_release(nl_sock);
    pr_info("my_service: netlink socket released\n");
}
```

### 用户态 Netlink 客户端

```c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define NETLINK_MY_SERVICE  31
#define MAX_PAYLOAD        1024

int main() {
    struct sockaddr_nl src, dst;
    struct nlmsghdr *nlh;
    struct msghdr msg;
    struct iovec iov;
    int sock_fd;
    char buf[MAX_PAYLOAD];

    sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_MY_SERVICE);
    if (sock_fd < 0) { perror("socket"); return 1; }

    memset(&src, 0, sizeof(src));
    src.nl_family = AF_NETLINK;
    src.nl_pid = getpid();
    bind(sock_fd, (struct sockaddr *)&src, sizeof(src));

    memset(&dst, 0, sizeof(dst));
    dst.nl_family = AF_NETLINK;
    dst.nl_pid = 0;    /* kernel */

    nlh = (struct nlmsghdr *)buf;
    nlh->nlmsg_len = NLMSG_SPACE(strlen("hello kernel") + 1);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;
    strcpy(NLMSG_DATA(nlh), "hello kernel");

    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void *)&dst;
    msg.msg_namelen = sizeof(dst);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    sendmsg(sock_fd, &msg, 0);

    memset(buf, 0, sizeof(buf));
    recv(sock_fd, buf, sizeof(buf), 0);
    printf("Kernel replied: %s\n", (char *)NLMSG_DATA((struct nlmsghdr *)buf));

    close(sock_fd);
    return 0;
}
```

## 8. 完整模块初始化（整合所有组件）

**`drivers/mycompany/my_service/my_service.c`**

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

extern int my_service_thread_start(void);
extern void my_service_thread_stop(void);
extern int my_service_dev_init(void);
extern void my_service_dev_exit(void);
extern int my_service_netlink_init(void);
extern void my_service_netlink_exit(void);

static int __init my_service_init(void)
{
    int ret;

    pr_info("my_service: initializing\n");

    ret = my_service_dev_init();
    if (ret) goto err_dev;

    ret = my_service_thread_start();
    if (ret) goto err_thread;

    ret = my_service_netlink_init();
    if (ret) goto err_netlink;

    pr_info("my_service: all components initialized\n");
    return 0;

err_netlink:
    my_service_thread_stop();
err_thread:
    my_service_dev_exit();
err_dev:
    return ret;
}

static void __exit my_service_exit(void)
{
    my_service_netlink_exit();
    my_service_thread_stop();
    my_service_dev_exit();
    pr_info("my_service: all components cleaned up\n");
}

module_init(my_service_init);
module_exit(my_service_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MyCompany");
MODULE_DESCRIPTION("My Custom Kernel Service");
```

## 9. 编译 + 验证

```bash
# 1. 配置内核
make menuconfig
# 导航到 Device Drivers -> MyCompany -> My Custom Kernel Service
# 按 M 编译为模块，或 Y 编译进内核

# 2. 编译模块
make drivers/mycompany/my_service/ -j$(nproc)

# 3. 编译整个内核
make -j$(nproc)

# 4. 加载模块
insmod my_service.ko

# 5. 验证
dmesg | grep my_service
lsmod | grep my_service
ls -la /dev/my_service
cat /proc/devices | grep my_service

# 6. 用户态测试
./my_service_client        # ioctl 测试
./my_service_netlink_client # netlink 测试

# 7. 卸载
rmmod my_service
```

## 10. 关键点小结

| 组件 | 关键 API | 作用 |
|------|---------|------|
| 内核模块 | `module_init` / `module_exit` | 模块加载/卸载入口 |
| 内核线程 | `kthread_create` / `kthread_stop` | 后台常驻任务 |
| 字符设备 | `alloc_chrdev_region` / `cdev_add` | 用户态通过 /dev/ 交互 |
| Netlink | `netlink_kernel_create` / `nlmsg_unicast` | 内核↔用户态双向异步通信 |
| Kconfig | `tristate` / `depends on` | 菜单配置，控制编译选项 |
| Makefile | `obj-$(CONFIG_XXX)` | 条件编译 |

## 11. 易踩的坑

1. **`MODULE_LICENSE` 没写或写错** → 内核 tainted，无法使用 GPL-only 符号（如 `kthread_create`）。
2. **内核线程没检查 `kthread_should_stop()`** → `rmmod` 时死锁。
3. **字符设备 `copy_to_user` / `copy_from_user` 用错** → 内核空间直接访问用户指针会 OOPS。
4. **Netlink 协议号冲突** → 自定义协议号选 31 以下可能冲突，建议用 `NETLINK_USERSOCK` 或注册动态号。
5. **忘了 `class_create` / `device_create`** → `/dev/` 下不会自动出现设备节点，需手动 `mknod`。
6. **内核模块编译依赖当前运行的内核头文件** → 交叉编译时需指定 `KDIR`。

---

以上是 Linux Kernel 层添加 native 服务的完整指南。如果你希望我将此内容写入文件，请告诉我目标路径。