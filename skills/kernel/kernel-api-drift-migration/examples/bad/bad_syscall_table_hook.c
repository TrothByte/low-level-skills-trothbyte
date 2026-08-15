/*
 * BAD: // intentionally incorrect — unguarded sys_call_table hook (6.9+).
 * sys_call_table stopped being exported in 6.9. This code compiles fine
 * against older headers (extern declaration) but on a 6.9+ kernel the
 * symbol never resolves: the hook loads and silently does nothing. The 2024
 * StackOverflow report class of failure. Requires a runtime availability check
 * or a supported mechanism (ftrace/BPF).
 */
#include <linux/module.h>
#include <linux/kernel.h>

extern void **sys_call_table;

static int __init bad_hook_init(void)
{
	/* NULL deref or silent no-op on 6.9+ — nothing validates the symbol */
	void *old = xchg(&sys_call_table[__NR_read], (void *)0x0);
	(void)old;
	pr_info("hook installed\n");
	return 0;
}

static void __exit bad_hook_exit(void)
{
}

module_init(bad_hook_init);
module_exit(bad_hook_exit);
MODULE_LICENSE("GPL");
