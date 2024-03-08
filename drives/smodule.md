# 参数驱动



```
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>


static int itype=0;
module_param(itype, int, 0);

static bool btype = 0;
module_param(btype, bool, 0);

static unsigned char ctype=0;
module_param(ctype, byte, 0);

static char *stype=0;
module_param(stype, charp, 0);

static int __init demo_module_init(void)
{
	printk("simple module init\n");
	printk("itype=%d\n",itype);
	printk("btype=%d\n",btype);
	printk("ctype=%d\n",ctype);
	printk("stype='%s'\n",stype);
	return 0;
}

static void __exit demo_module_exit(void)
{
	printk("simple module exit\n");
}
module_init(demo_module_init);
module_exit(demo_module_exit);

MODULE_AUTHOR("fgjnew <fgjnew@163.com>");
MODULE_DESCRIPTION("simple module");
MODULE_LICENSE("GPL");
```





```
insmod smodule.ko itype=3 btype=1ctype=0xAC stype='a'
```





EXPORT_SYMBOL_GPL是Linux内核中的一个宏，用于将一个符号（函数、变量或其他）导出为符号表的全局符号。它的作用是允许其他模块或驱动程序使用该符号，即可以在其他模块中调用该导出的符号。

EXPORT_SYMBOL_GPL与EXPORT_SYMBOL的区别在于，EXPORT_SYMBOL_GPL将符号标记为“GPL许可证”下可用，意味着只有遵循GPL许可证规定的模块或驱动程序才能使用该导出的符号。

这样设计的目的是保护Linux内核的代码，防止许可证冲突和代码滥用。只有那些遵循GPL许可证的模块或驱动程序才能使用EXPORT_SYMBOL_GPL导出的符号，其他模块或驱动程序则不能使用。这种限制可以确保Linux内核代码的安全性和一致性。
