---
created: 2025-05-22T14:55:19 (UTC +08:00)
tags: [Android,Linux中文技术社区,前端开发社区,前端技术交流,前端框架教程,JavaScript 学习资源,CSS 技巧与最佳实践,HTML5 最新动态,前端工程师职业发展,开源前端项目,前端技术趋势]
source: https://juejin.cn/post/7201400444873293885
author: 满嘴跑火车的小土匪
---

# 图解 Binder：初始化本文主要分两部分： 1. binder_init()的解析 2. 内核对binder_init - 掘金

> ## Excerpt
> 本文主要分两部分： 1. binder_init()的解析 2. 内核对binder_init()的调用

---
> 这是一系列的 Binder 文章，会从内核层到 Framework 层，再到 Java 层，深入浅出，介绍整个 Binder 的设计。详见《[图解 Binder：概述](https://juejin.cn/post/7244018340880007226 "https://juejin.cn/post/7244018340880007226")》。
> 
> 本文基于 Android 内核分支 common-android13-5.15 解析。
> 
> 一些关键代码的链接，可能会因为源码的变动，发生位置偏移、丢失等现象。可以搜索函数名，重新进行定位。

Linux的内核模块机制允许开发者向内核添加功能。很多功能或者外设驱动都可以编译成模块。Linux系统使用两种方式去加载系统中的模块：动态和静态。Binder驱动是通过静态的方式，在编译期编译到内核中，在内核启动的时候，执行初始化。

## Binder驱动初始化

Binder驱动初始化的入口在[common/drivers/android/binder.c](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D6376 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=6376")：

```
static int __init binder_init(void)
{
    ...
}

device_initcall(binder_init);
```

本文主要分两部分：

-   binder\_init()的解析
-   内核对binder\_init()的调用

## 细说binder\_init()

binder\_init()是Binder驱动的初始化函数。binder\_init()主要做以下操作：

1.  创建几个帮助调试的文件和目录
2.  注册misc设备
3.  注册binder文件系统

-   [binder\_init()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D6376 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=6376")
    -   [init\_binder\_device()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D6347 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=6347")
    -   [init\_binderfs()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinderfs.c%3Bl%3D788 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binderfs.c;l=788")

```
static int __init binder_init(void)
{
int ret;
char *device_name, *device_tmp;
struct binder_device *device;
char *device_names = NULL;

        //创建目录：/binder
binder_debugfs_dir_entry_root = debugfs_create_dir("binder", NULL);
if (binder_debugfs_dir_entry_root) {
const struct binder_debugfs_entry *db_entry;
                //创建几个文件：
                //    /binder/state
                //    /binder/stats
                //    /binder/transactions
                //    /binder/transaction_log
                //    /binder/failed_transaction_log
binder_for_each_debugfs_entry(db_entry)
debugfs_create_file(db_entry->name,
    db_entry->mode,
    binder_debugfs_dir_entry_root,
    db_entry->data,
    db_entry->fops);
                //创建目录：/binder/proc
binder_debugfs_dir_entry_proc = debugfs_create_dir("proc",
 binder_debugfs_dir_entry_root);
}

if (!IS_ENABLED(CONFIG_ANDROID_BINDERFS) &&
    strcmp(binder_devices_param, "") != 0) {
                //binder_devices_param固定为 binder,hwbinder,vndbinder
                device_names = kstrdup(binder_devices_param, GFP_KERNEL);
device_tmp = device_names;
while ((device_name = strsep(&device_tmp, ","))) {
                        //分别初始化设备binder、hwbinder、vndbinder
ret = init_binder_device(device_name);
}
}

        //初始化binder文件系统
ret = init_binderfs();
return ret;
}
```

-   可以通过查看/sys/kernel/debug/binder查看state、stats、transactions、transaction\_log、failed\_transaction\_log文件，以及一个proc目录。
    -   /sys/kernel/debug/binder/state：整体以及各个进程的thread/node/ref/buffer的状态信息，如有deadnode也会打印
    -   /sys/kernel/debug/binder/stats：整体以及各个进程的线程数，事务个数等的统计信息
    -   /sys/kernel/debug/binder/failed\_transaction\_log：记录32条最近的传输失败事件
    -   /sys/kernel/debug/binder/transaction\_log：记录32条最近的传输事件
    -   /sys/kernel/debug/binder/transactions：遍历所有进程的buffer分配情况
    -   proc目录中都是进程号，观察其中的进程都是注册在驱动的进程，其中包括servicemanager。可以通过命令`cat /sys/kernel/debug/binder/proc/进程号`查看对应进程的binder信息。

### 注册misc设备——init\_binder\_device()

-   [init\_binder\_device()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder.c%3Bl%3D6347 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder.c;l=6347")

```
static int __init init_binder_device(const char *name)
{
int ret;
struct binder_device *binder_device;

binder_device = kzalloc(sizeof(*binder_device), GFP_KERNEL);

        //设置misc设备的操作
binder_device->miscdev.fops = &binder_fops;
        //设置misc设备的次设备号（动态分配的）
binder_device->miscdev.minor = MISC_DYNAMIC_MINOR;
        //设置misc设备的设备名
binder_device->miscdev.name = name;

binder_device->context.binder_context_mgr_uid = INVALID_UID;
binder_device->context.name = name;
        //初始化互斥锁
mutex_init(&binder_device->context.context_mgr_node_lock);

        //注册misc设备
ret = misc_register(&binder_device->miscdev);
        //将binder设备加入链表（头插法）
hlist_add_head(&binder_device->hlist, &binder_devices);

return ret;
}
```

-   misc设备：Linux内核把无法归类的设备定义为misc设备，譬如看门狗、实时时钟等。Linux内核把所有的misc设备组织在一起，构成一个子系统，进行统一管理。在这个子系统里的所有misc类型的设备共享一个主设备号MISC\_MAJOR(10)，但它们次设备号不同。
-   结构体[binder\_device](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinder_internal.h%3Bl%3D34 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binder_internal.h;l=34")表示一个binder设备节点，记录了该节点相关信息。
-   `binder_device->miscdev`：即结构体[miscdevice](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Fmiscdevice.h%3Bl%3D79 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/miscdevice.h;l=79")。它表示一个misc设备，记录该设备的名称、次版本号、支持的系统调用操作等。
-   `binder_device->miscdev.fops = &binder_fops;`

`binder_device->miscdev.fops`即结构体[file\_operations](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Ffs.h%3Bl%3D2041 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/fs.h;l=2041")。它是把系统调用和驱动程序关联起来的关键结构。这个结构的每一个成员都对应着一个系统调用，Linux系统调用通过调用file\_operations中相应的函数指针，接着把控制权转交给函数，从而完成Linux设备驱动程序的工作。

```
struct file_operations {
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *); //对应系统调用read
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *); //对应系统调用write
    __poll_t (*poll) (struct file *, struct poll_table_struct *); //对应系统调用poll
    int (*open) (struct inode *, struct file *); //对应系统调用open
    ...
}
```

而`binder_fops`的定义如下：

```
const struct file_operations binder_fops = {
.owner = THIS_MODULE,
.poll = binder_poll,
.unlocked_ioctl = binder_ioctl,
.compat_ioctl = compat_ptr_ioctl,
.mmap = binder_mmap,
.open = binder_open,
.flush = binder_flush,
.release = binder_release,
};
```

这意味着当binder驱动执行系统调用时，

1.  如果是系统调用ioctl()，最终会调用binder\_ioctl()
2.  如果是系统调用mmap()，最终会调用binder\_map()
3.  如果是系统调用open()，最终会调用binder\_open()
4.  ...

### 注册Binder文件系统

Linux将文件系统分为了两层：VFS（虚拟文件系统）、具体文件系统。

VFS（Virtual Filesystem Switch）不是一种实际的文件系统，它只存在于内存中。它是一个内核软件层，是在具体的文件系统之上抽象的一层，用来处理与Posix文件系统相关的所有调用。它屏蔽了底层各种文件系统复杂的调用实现，为各种文件系统提供一个通用的接口。

当通过系统调用 open() 打开 "/dev/binder" 设备文件时，就会沿着 VFS，最后定位到 binder 文件系统，调用其对应的 binder\_open() 实现，完成 binder 驱动的打开。其他系统调用，也是类似的。

![](https://p6-juejin.byteimg.com/tos-cn-i-k3u1fbpfcp/986e02443ef84e3c92f7115fdb6654d9~tplv-k3u1fbpfcp-zoom-in-crop-mark:1512:0:0:0.awebp?)

Binder驱动在使用前，必须：

-   在VFS上注册
-   挂载对应的文件系统

###### 注册

-   [init\_binderfs()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Fdrivers%2Fandroid%2Fbinderfs.c%3Bl%3D788 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/drivers/android/binderfs.c;l=788")

```
static struct file_system_type binder_fs_type = {
.name= "binder",
.init_fs_context= binderfs_init_fs_context,
.parameters= binderfs_fs_parameters,
.kill_sb= kill_litter_super,
.fs_flags= FS_USERNS_MOUNT,
};

int __init init_binderfs(void)
{
int ret;
ret = register_filesystem(&binder_fs_type);
return ret;
}
```

-   register\_filesystem()会向VFS注册binder文件系统。
-   每个注册的文件系统都用一个类型为[file\_system\_type](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Ffs.h%3Bl%3D2499 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/fs.h;l=2499")的对象表示。file\_system\_type主要记录文件系统的类型相关信息，比如名称、上下文初始化函数指针等。`binder_fs_type`指明将要挂载的Binder文件系统名为`binder`。

###### 挂载

Binder文件系统挂载到VFS的时机在init进程启动的时候。相关挂载指令在[system/core/rootdir/init.rc](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Frootdir%2Finit.rc%3Bl%3D269 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/rootdir/init.rc;l=269")中：

```
mkdir /dev/binderfs
mount binder binder /dev/binderfs stats=global
chmod 0755 /dev/binderfs

symlink /dev/binderfs/binder /dev/binder
symlink /dev/binderfs/hwbinder /dev/hwbinder
symlink /dev/binderfs/vndbinder /dev/vndbinder
```

`mount`指令的解析函数是[do\_mount()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Finit%2Fbuiltins.cpp%3Bl%3D478 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/init/builtins.cpp;l=478")。

`symlink`指令的解析函数是[do\_symlink()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fplatform%2Fsuperproject%2F%2B%2Fandroid-13.0.0_r1%3Asystem%2Fcore%2Finit%2Fbuiltins.cpp%3Bl%3D820 "https://cs.android.com/android/platform/superproject/+/android-13.0.0_r1:system/core/init/builtins.cpp;l=820")。

do\_mount()会调用[mount()](https://link.juejin.cn/?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fmount.2.html "https://man7.org/linux/man-pages/man2/mount.2.html")将代表Binder驱动的路径`/dev/binderfs`挂载到Binder文件系统。

do\_symlink()会调用[symlink()](https://link.juejin.cn/?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fsymlink.2.html "https://man7.org/linux/man-pages/man2/symlink.2.html")，为相应的文件路径起别名。比如，`/dev/binderfs/binder`对应别名就是`/dev/binder`。这样当我们执行`open("/dev/binder", O_RDWR | O_CLOEXEC)`时，实际就是打开位于`/dev/binderfs/binder`的Binder驱动。

## 何时调用binder\_init()？

内核主要是通过initcall机制，在启动init进程的时候，完成对binder\_init()的调用。

### 几个重要的宏定义

回顾一下下面的代码：

```
static int __init binder_init(void)
{
    ...
}

device_initcall(binder_init);
```

这里有几个重要的宏定义：

1） `__init`

`__init`的宏定义在[common/include/linux/init.h](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Finit.h%3Bl%3D50 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/init.h;l=50")中：

```
#define __init__section(".init.text") __cold  __latent_entropy __noinitretpoline __nocfi
```

`__section`是GCC的一个编译属性，在GCC中[定义](https://link.juejin.cn/?target=https%3A%2F%2Fgcc.gnu.org%2Fonlinedocs%2Fgcc%2FCommon-Function-Attributes.html%23index-functions-that-return-more-than-once "https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-functions-that-return-more-than-once")如下：

> `section ("section-name")`
> 
> Normally, the compiler places the code it generates in the `text` section. Sometimes, however, you need additional sections, or you need certain particular functions to appear in special sections. The `section` attribute specifies that a function lives in a particular section. For example, the declaration:
> 
> ```
> extern void foobar (void) __attribute__ ((section ("bar")));
> ```
> 
> puts the function `foobar` in the `bar` section. Some file formats do not support arbitrary sections so the `section` attribute is not available on all platforms. If you need to map the entire contents of a module to a particular section, consider using the facilities of the linker instead.

通常，编译器会将生成的代码是放在ELF的`.text`段中的。不过`section`属性可以指定一个函数，将其放在一个特定的段中。

所以，所有标识为`__init`的函数都会放在`.init.text`这个段内。在这个段中，函数的摆放顺序是和链接的顺序有关的，是不确定的。

所以，`binder_init`会被放入`.init.text`这个段内。

2） `device_initcall`

`device_initcall(binder_init)`就是将指向binder\_init的函数指针，注册在`.initcall6.init`段里。内核启动时，会调用它，对Binder驱动进行初始化。

`device_initcall`的宏定义在[common/include/linux/init.h](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Flinux%2Finit.h%3Bl%3D291 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/linux/init.h;l=291")中：

```
typedef int (*initcall_t)(void);
typedef initcall_t initcall_entry_t;

/* Format: <modname>__<counter>_<line>_<fn> */
#define __initcall_id(fn)\
__PASTE(__KBUILD_MODNAME,\
__PASTE(__,\
__PASTE(__COUNTER__,\
__PASTE(_,\
__PASTE(__LINE__,\
__PASTE(_, fn))))))

/* Format: __<prefix>__<iid><id> */
#define __initcall_name(prefix, __iid, id)\
__PASTE(__,\
__PASTE(prefix,\
__PASTE(__,\
__PASTE(__iid, id))))
      
#define __initcall_section(__sec, __iid)\
#__sec ".init"

#define __initcall_stub(fn, __iid, id)fn

#define ____define_initcall(fn, __unused, __name, __sec)\
static initcall_t __name __used \
__attribute__((__section__(__sec))) = fn;
#endif

#define __unique_initcall(fn, id, __sec, __iid)\
____define_initcall(fn,\
__initcall_stub(fn, __iid, id),\
__initcall_name(initcall, __iid, id),\
__initcall_section(__sec, __iid))

#define ___define_initcall(fn, id, __sec)\
__unique_initcall(fn, id, __sec, __initcall_id(fn))

#define __define_initcall(fn, id) ___define_initcall(fn, id, .initcall##id)

#define device_initcall(fn)__define_initcall(fn, 6)
```

`device_initcall(binder_init)`展开就是：

```
device_initcall(binder_init)
        ↓
__define_initcall(binder_init, 6)
        ↓
___define_initcall(binder_init, 6, .initcall6)
        ↓
__unique_initcall(binder_init, 6, .initcall6, <modname>__<counter>_<line>_binder_init)
        ↓
____define_initcall(binder_init, 
        binder_init,
        __initcall__<modname>__<counter>_<line>_binder_init6,
        .initcall6.init)
        ↓
static initcall_t __initcall__<modname>__<counter>_<line>_binder_init6 __used 
      __attribute__((__section__(.initcall6.init))) = binder_init;
```

`device_initcall(binder_init)`的含义是：将`binder_init`的函数指针赋值给变量`__initcall__<modname>__<counter>_<line>_binder_init6`，然后将该变量存放在`.initcall6.init`段中。

涉及的另外几个宏：

-   `_used_`使用前提是在编译器编译过程中，如果定义的符号没有被引用，编译器就会对其进行优化，不保留这个符号，而\_\_attribute\_\_((_used_))的作用是告诉编译器这个静态符号在编译的时候即使没有使用到也要保留这个符号。
-   [`__PASTE`](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.10%3Acommon%2Finclude%2Flinux%2Fcompiler_types.h%3Bl%3D60 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.10:common/include/linux/compiler_types.h;l=60")是做简单的字符串拼接：

```
#define ___PASTE(a,b) a##b
#define __PASTE(a,b) ___PASTE(a,b)
```

-   [`__KBUILD_MODNAME__`](https://link.juejin.cn/?target=https%3A%2F%2Fzhidao.baidu.com%2Fquestion%2F616617125144174532.html "https://zhidao.baidu.com/question/616617125144174532.html")是Linux kbuild的体系在编译模块的时候生成的。
-   [`__LINE__`](https://link.juejin.cn/?target=https%3A%2F%2Fwww.cnblogs.com%2Fjiexianzhu%2Fp%2F10274455.html "https://www.cnblogs.com/jiexianzhu/p/10274455.html")在预处理阶段，会被替换成代码行号。
-   [`__COUNTER__`](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fweixin_44651376%2Farticle%2Fdetails%2F124258349 "https://blog.csdn.net/weixin_44651376/article/details/124258349")是GNU 编译器的非标准编译器扩展，可以认为它是一个计数器，代表一个整数，它的值一般被初始化为0，在每次编译器编译到它时，会自动 +1。

### initcall机制

当我们试图将一个驱动程序加载进内核时，我们需要提供一个xxx\_init()函数。这样内核才会定位到该函数，加载驱动，初始化驱动。

`binder_init()`就是这样一个初始化驱动的函数。但是怎么向内核注册这样一个函数呢？直观的做法是维护一个初始化驱动的函数指针的数组，将`binder_init()`添加进该数组中。不过这样在多人开发时，容易造成编码冲突。

linux采用了更优雅的方法——`initcall机制`：

在内核镜像文件中，自定义一个段，这个段里面专门用来存放这些初始化函数的地址，内核启动时，只需要在这个段地址处取出函数指针，一个个执行即可。

###### .initcallXX.init段

`.initcallXX.init`段就是专门用来存放各个内核模块的初始化函数的地址。

`device_initcall(fn)`就是表示将指向fn的函数指针，存放在`.initcall6.init`段。类似的宏定义有：

```
#define pure_initcall(fn)__define_initcall(fn, 0)              →  .initcall0.init
#define core_initcall(fn)__define_initcall(fn, 1)              →  .initcall1.init
#define core_initcall_sync(fn)__define_initcall(fn, 1s)             →  .initcall1s.init
#define postcore_initcall(fn)__define_initcall(fn, 2)              →  .initcall2.init
#define postcore_initcall_sync(fn)__define_initcall(fn, 2s)             →  .initcall2s.init
#define arch_initcall(fn)__define_initcall(fn, 3)              →  .initcall3.init
#define arch_initcall_sync(fn)__define_initcall(fn, 3s)             →  .initcall3s.init
#define subsys_initcall(fn)__define_initcall(fn, 4)              →  .initcall4.init
#define subsys_initcall_sync(fn)__define_initcall(fn, 4s)             →  .initcall4s.init
#define fs_initcall(fn)__define_initcall(fn, 5)              →  .initcall5.init
#define fs_initcall_sync(fn)__define_initcall(fn, 5s)             →  .initcall5s.init
#define rootfs_initcall(fn)__define_initcall(fn, rootfs)         →  .initcallrootfs.init
#define device_initcall(fn)__define_initcall(fn, 6)              →  .initcall6.init
#define device_initcall_sync(fn)__define_initcall(fn, 6s)             →  .initcall6s.init
#define late_initcall(fn)__define_initcall(fn, 7)              →  .initcall7.init
#define late_initcall_sync(fn)__define_initcall(fn, 7s)             →  .initcall7s.init
```

###### .initcallXX.init段的定义

`.initcallXX.init`段的定义是在[common/include/asm-generic/vmlinux.lds.h](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finclude%2Fasm-generic%2Fvmlinux.lds.h%3Bl%3D919 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/include/asm-generic/vmlinux.lds.h;l=919")和[common/arch/arm64/kernel/vmlinux.lds.S](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Farch%2Farm64%2Fkernel%2Fvmlinux.lds.S%3Bl%3D234 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/arch/arm64/kernel/vmlinux.lds.S;l=234")中。

```
//common/include/asm-generic/vmlinux.lds.h
#define INIT_CALLS_LEVEL(level)\
__initcall##level##_start = .;\
KEEP(*(.initcall##level##.init))\
KEEP(*(.initcall##level##s.init))\

#define INIT_CALLS\
__initcall_start = .;\
KEEP(*(.initcallearly.init))\
INIT_CALLS_LEVEL(0)\
INIT_CALLS_LEVEL(1)\
INIT_CALLS_LEVEL(2)\
INIT_CALLS_LEVEL(3)\
INIT_CALLS_LEVEL(4)\
INIT_CALLS_LEVEL(5)\
INIT_CALLS_LEVEL(rootfs)\
INIT_CALLS_LEVEL(6)\
INIT_CALLS_LEVEL(7)\
__initcall_end = .;
                
//common/arch/arm64/kernel/vmlinux.lds.S
SECTIONS
{
        ...
        .init.data : {
INIT_DATA
INIT_SETUP(16)
INIT_CALLS
CON_INITCALL
INIT_RAM_FS
*(.init.altinstructions .init.bss)/* from the EFI stub */
}
        ...
}
```

-   vmlinux.lds.S中的不是汇编代码，而是[Linker Script](https://link.juejin.cn/?target=https%3A%2F%2Fsourceware.org%2Fbinutils%2Fdocs%2Fld%2FScripts.html "https://sourceware.org/binutils/docs/ld/Scripts.html")。
-   vmlinux是一个包含linux kernel的静态链接的可执行文件，文件类型通常是linux接受的可执行文件格式ELF。
-   `INIT_CALLS`中，定义了16个段，每隔两个段，都会定义一个函数指针，指向这两个段的起始地址，比如：`__initcall_0_start`指向`.initcall_0.init`、`.initcall_0s.init`这两个段的起始地址。
-   `INIT_CALLS`还定义了两个函数指针`__initcall_start`、`__initcall_end`，分别指向这16个段之前、之后的位置。

###### 调用.initcallXX.init段里的初始化函数

代码经过编译、链接后，`binder_init`这样的初始化函数的函数指针，会按照一定的顺序，插入vmlinux的二进制文件中。在内核启动的时候，由内核一一调用它们。

大致的调用栈是：

-   [start\_kernel()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finit%2Fmain.c%3Bl%3D935 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/init/main.c;l=935")
    -   [arch\_call\_rest\_init()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finit%2Fmain.c%3Bl%3D887 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/init/main.c;l=887")
        -   [rest\_init()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finit%2Fmain.c%3Bl%3D690 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/init/main.c;l=690")
            -   [kernel\_thread(kernel\_init)](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finit%2Fmain.c%3Bl%3D1501 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/init/main.c;l=1501")
                -   [kernel\_init\_freeable()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finit%2Fmain.c%3Bl%3D1589 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/init/main.c;l=1589")
                    -   [do\_basic\_setup()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finit%2Fmain.c%3Bl%3D1408 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/init/main.c;l=1408")
                        -   [do\_initcalls()](https://link.juejin.cn/?target=https%3A%2F%2Fcs.android.com%2Fandroid%2Fkernel%2Fsuperproject%2F%2B%2Fcommon-android13-5.15%3Acommon%2Finit%2Fmain.c%3Bl%3D1382 "https://cs.android.com/android/kernel/superproject/+/common-android13-5.15:common/init/main.c;l=1382")

核心函数是`do_initcalls()`，它会完成`.initcallXX.init`段里的初始化函数的定位，并逐一调用它们：

```
//__initcall0_start这些函数指针的定义，就是在common/include/asm-generic/vmlinux.lds.h中
static initcall_entry_t *initcall_levels[] __initdata = {
__initcall0_start,
__initcall1_start,
__initcall2_start,
__initcall3_start,
__initcall4_start,
__initcall5_start,
__initcall6_start,
__initcall7_start,
__initcall_end,
};

/* Keep these in sync with initcalls in include/linux/init.h */
static const char *initcall_level_names[] __initdata = {
"pure",
"core",
"postcore",
"arch",
"subsys",
"fs",
"device",
"late",
};

static void __init do_initcalls(void)
{
int level;
        //遍历各个.initcallXX.init段
        //initcall_levels数组最后一个值是__initcall_end，所以遍历不包括它
        for (level = 0; level < ARRAY_SIZE(initcall_levels) - 1; level++) {
do_initcall_level(level, command_line);
}
}

static void __init do_initcall_level(int level, char *command_line)
{
        initcall_entry_t *fn;
        //遍历.initcallXX.init段里存放的各个初始化函数，并逐一调用它们
        for (fn = initcall_levels[level]; fn < initcall_levels[level+1]; fn++)
do_one_initcall(initcall_from_entry(fn));
}

int __init_or_module do_one_initcall(initcall_t fn)
{
        int ret;
        //调用初始化函数
        ret = fn();
        return ret;
}
```

至此，最终完成了对binder\_init()的调用。

## 参考资料

[Android源码分析 - Binder驱动（上）](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fqq_34231329%2Farticle%2Fdetails%2F125523401 "https://blog.csdn.net/qq_34231329/article/details/125523401")

[【GCC系列】深入理解Linux内核 -- \_\_init宏定义](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2FIvan804638781%2Farticle%2Fdetails%2F111313218 "https://blog.csdn.net/Ivan804638781/article/details/111313218")

[**attribute**((section(x))) 使用详解](https://link.juejin.cn/?target=https%3A%2F%2Fblog.csdn.net%2Fqq_42370291%2Farticle%2Fdetails%2F103639349 "https://blog.csdn.net/qq_42370291/article/details/103639349")

[vmlinux.lds.s 脚本语法](https://link.juejin.cn/?target=https%3A%2F%2Fwww.geek-share.com%2Fdetail%2F2608311768.html "https://www.geek-share.com/detail/2608311768.html")

[Linux 各种 initcall 的调用原理](https://link.juejin.cn/?target=https%3A%2F%2Fwww.byteisland.com%2Flinux-%25E5%2590%2584%25E7%25A7%258D-initcall-%25E7%259A%2584%25E8%25B0%2583%25E7%2594%25A8%25E5%258E%259F%25E7%2590%2586%2F "https://www.byteisland.com/linux-%E5%90%84%E7%A7%8D-initcall-%E7%9A%84%E8%B0%83%E7%94%A8%E5%8E%9F%E7%90%86/")

[linux内核链接脚本vmlinux.lds分析续篇之 --- initcall机制（十三）](https://link.juejin.cn/?target=https%3A%2F%2Fwww.cnblogs.com%2Fjianhua1992%2Fp%2F16852793.html "https://www.cnblogs.com/jianhua1992/p/16852793.html")
