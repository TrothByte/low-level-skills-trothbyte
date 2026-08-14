/* BAD: direct dereference of a __user pointer instead of a uaccess helper. */
#include "../stubs.h"
#include <stdio.h>

struct dev_config {
	unsigned long len;
	char tag[16];
};

/* BAD: no access_ok, no fault handling, sparse would reject this deref */
static unsigned long read_len_direct(struct dev_config __user *cfg)
{
	return cfg->len;
}

int main(void)
{
	struct dev_config kcfg;
	unsigned long v;

	kcfg.len = 42;
	/* In a real kernel this pointer would point into the user address
	 * space; the deref would read attacker-controlled memory or oops. */
	v = read_len_direct((struct dev_config __user *)&kcfg);
	if (v == 42)
		printf("BUG reproduced: __user pointer dereferenced directly\n");
	return 0;
}
