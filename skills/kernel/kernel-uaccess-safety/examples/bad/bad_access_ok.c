/* BAD: raw __copy_from_user used without a prior access_ok (LDD3 rule). */
#include "../stubs.h"
#include <stdio.h>

struct cfg {
	int a;
	int b;
};

/* BAD: __copy_from_user has no internal access_ok; the caller must check */
static int load_cfg_raw(struct cfg *c, const struct cfg __user *u)
{
	__copy_from_user_emu(c, u, sizeof *c);
	return 0;
}

int main(void)
{
	struct cfg kc = { 0, 0 };
	struct cfg src = { 7, 9 };

	/* src is a kernel address, not inside the user region; the raw copy
	 * proceeds anyway because no access_ok gate exists. */
	load_cfg_raw(&kc, (const struct cfg __user *)&src);
	if (kc.a == 7 && kc.b == 9)
		printf("BUG reproduced: raw copy bypassed access_ok\n");
	return 0;
}
