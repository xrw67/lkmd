#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

int __init test_init(void)
{
    __asm__("int3\n");
    return -EACCES;
}

void __exit test_exit(void)
{
    return;
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("GPL");
