#include <linux/init.h>   //mod init include
#include <linux/module.h> //mod must include
#include <linux/kernel.h> //printk

static int num = 10;

void show(void) {
	printk("show(),num=%d\n",num);
}
static __init int hello_init(void)
{
    printk("hello_init\n");
    show();
    return 0;
}
static __exit void hello_exit(void)
{
    printk("hello_exit\n");
}

EXPORT_SYMBOL_GPL(show);
module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.2");
MODULE_DESCRIPTION("A example module");
