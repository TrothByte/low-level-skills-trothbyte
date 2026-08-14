/* GOOD: irq-safe allocation inside a spinlock uses GFP_ATOMIC and checks
 * the result (GFP_ATOMIC can still fail under memory pressure). */
#include "../stubs.h"
#include <stdio.h>

struct device {
	spinlock_t lock;
	int *buf;
};

static int dev_open_good(struct device *d, size_t count)
{
	int ret = 0;

	spin_lock_emu(&d->lock);
	d->buf = kmalloc_emu(count, GFP_ATOMIC);   /* never sleeps */
	if (!d->buf)
		ret = -ENOMEM;
	spin_unlock_emu(&d->lock);
	return ret;
}

int main(void)
{
	struct device d = {0};
	int r = dev_open_good(&d, 32);

	if (g_sleep_in_atomic) {
		printf("FAIL: sleep detected in atomic context\n");
		return 1;
	}
	if (r != 0 || d.buf == NULL) {
		printf("FAIL: GFP_ATOMIC allocation failed\n");
		return 1;
	}
	printf("GOOD: GFP_ATOMIC under spinlock, no sleep, result checked\n");
	return 0;
}
