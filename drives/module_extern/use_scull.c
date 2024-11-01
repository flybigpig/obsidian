// use_scull.c
#include <linux/init.h>   //mod init include
#include <linux/module.h> //mod must include
#include <linux/kernel.h> //printk
#include "scull.h"
extern void show(void);
static __init int use_init(void)
{
    printk(KERN_INFO"use_%s\n",__func__);
    show();
    return 0;
}

static __exit void use_exit(void)
{
    printk("use_%s\r\n",__FUNCTION__);
}
module_init(use_init);
module_exit(use_exit);

MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.2");
MODULE_DESCRIPTION("A example module");
