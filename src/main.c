// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/moduleparam.h>

static char *whom = "world";
static int howmany = 1;

static int __init hello_init(void) {
	for (int i = 0; i < howmany; ++i) {
		pr_info("Hello, %s!\n", whom);
	}
	return 0;
}

static void __exit hello_exit(void) {
	for (int i = 0; i < howmany; ++i) {
		pr_info("Goodbye, %s!\n", whom);
	}
}

module_param(howmany, int, S_IRUGO);
module_param(whom, charp, S_IRUGO);

module_init(hello_init);
module_exit(hello_exit);

MODULE_AUTHOR("Loboda Grigoriy");
MODULE_DESCRIPTION("print Hello, World messages");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL v2");
