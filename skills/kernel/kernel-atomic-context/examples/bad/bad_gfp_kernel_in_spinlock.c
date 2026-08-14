/* BAD: kmalloc(GFP_KERNEL) called while a spinlock is held. */
#include "../stubs.h"
#include <stdio.h>

struct device {
	spinlock_t lock;
	int *buf;
};

/* BAD: GFP_KERNEL may sleep; the spinlock disables preemption, so this
 * schedules while atomic (oops or deadlock). */
static int dev_open_bad(struct device *d, size_t count)
{
	spin_lock_emu(&d->lock);
	d->buf = kmalloc_emu(count, GFP_KERNEL);   /* may sleep */
	spin_unlock_emu(&d->lock);
	return d->buf ? 0 : -ENOMEM;
}

int main(void)
{
	struct device d = {0};
	dev_open_bad(&d, 32);
	if (g_sleep_in_atomic)
		printf("BUG reproduced: kmalloc(GFP_KERNEL) in spinlock\n");
	return 0;
}
