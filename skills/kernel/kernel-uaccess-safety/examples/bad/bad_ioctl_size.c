/* BAD: ioctl trusts _IOC_SIZE(cmd) without validating type/nr/size. */
#include "../stubs.h"
#include <stdio.h>
#include <string.h>

#define MY_MAGIC 0xA5

struct ioctl_arg {
	unsigned long value;
	char payload[64];
};

/* kernel-side argument storage with an adjacent canary so an oversized
 * copy is observable without crashing the host harness */
struct ioctl_buf {
	struct ioctl_arg arg;
	unsigned char canary[32];
};

static struct ioctl_buf dev_buf;

/* BAD: cmd bits are attacker-controlled; want is used as a copy length */
static long dev_ioctl_bad(unsigned int cmd, void __user *arg)
{
	unsigned long want = _IOC_SIZE(cmd);

	if (copy_from_user_emu(&dev_buf.arg, arg, want) != 0)
		return -EFAULT;
	return 0;
}

int main(void)
{
	unsigned int cmd = _IOC(_IOC_READ, MY_MAGIC, 1,
				sizeof(struct ioctl_arg) + 8);
	size_t i;
	int corrupted = 0;

	memset(__user_mem, 0xAA, PAGE_SIZE);
	memset(&dev_buf, 0, sizeof dev_buf);

	dev_ioctl_bad(cmd, (void __user *)__user_mem);
	for (i = 0; i < sizeof dev_buf.canary; i++)
		if (dev_buf.canary[i] != 0)
			corrupted = 1;
	if (corrupted)
		printf("BUG reproduced: oversized copy wrote past the ioctl argument buffer\n");
	return 0;
}
