// BAD: // intentionally incorrect — no MODULE_LICENSE on a driver that calls
// GPL-only (EXPORT_SYMBOL_GPL) kernel APIs. The module "compiles" but fails at
// load with unresolved GPL-only symbols, and the kernel marks it tainted.
#include <linux/module.h>
#include <linux/kernel.h>

extern int gpl_only_api(void);

static int __init bad_init(void)
{
	pr_info("value: %d\n", gpl_only_api());
	return 0;
}

static void __exit bad_exit(void)
{
}

module_init(bad_init);
module_exit(bad_exit);
