/*
 * GOOD: runtime-availability-guarded hook pattern.
 * Resolve the symbol through a lookup that returns NULL when unexported (the
 * 6.9+ sys_call_table case) and fail loudly — never install a silently-dead
 * hook. This is the pattern a reviewer must require for any API whose export
 * status is version-gated.
 */
#include <linux/module.h>
#include <linux/kernel.h>

#ifndef __NR_read
#define __NR_read 0
#endif

/* lookup returns NULL when the symbol is not exported in this kernel */
extern void **resolve_kallsyms(const char *name);

static void **sys_call_table = NULL;

static int __init good_hook_init(void)
{
	sys_call_table = resolve_kallsyms("sys_call_table");
	if (!sys_call_table) {
		pr_warn("sys_call_table unavailable (6.9+) — hook disabled\n");
		return -ENOTSUPP;
	}
	pr_info("hook installed with verified table\n");
	return 0;
}

static void __exit good_hook_exit(void)
{
	sys_call_table = NULL;
}

module_init(good_hook_init);
module_exit(good_hook_exit);
MODULE_LICENSE("GPL");
