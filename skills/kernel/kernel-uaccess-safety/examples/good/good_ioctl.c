/* GOOD: ioctl command space fully validated before any copy. */
#include "../stubs.h"
#include <assert.h>
#include <string.h>

#define MY_MAGIC 0xA5
#define MY_MAXNR 4

struct ioctl_arg {
	unsigned long value;
	char payload[64];
};

#define MY_GETARG _IOR(MY_MAGIC, 1, struct ioctl_arg)
#define MY_SETARG _IOW(MY_MAGIC, 2, struct ioctl_arg)

static long dev_ioctl_good(unsigned int cmd, void __user *arg)
{
	struct ioctl_arg a;
	unsigned int dir = _IOC_DIR(cmd);

	if (_IOC_TYPE(cmd) != MY_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) >= MY_MAXNR)
		return -ENOTTY;
	if (dir != _IOC_READ && dir != _IOC_WRITE &&
	    dir != (_IOC_READ | _IOC_WRITE))
		return -ENOTTY;
	if (_IOC_SIZE(cmd) != sizeof(struct ioctl_arg))
		return -EINVAL;
	if (copy_from_user_emu(&a, arg, sizeof a))
		return -EFAULT;
	(void)a.value;
	return 0;
}

/* compat path: shared here because the 32-bit struct layout is identical */
static long dev_ioctl_compat(unsigned int cmd, void __user *arg)
{
	return dev_ioctl_good(cmd, arg);
}

int main(void)
{
	unsigned int bad_type = _IOW(0x77, 1, struct ioctl_arg);
	unsigned int bad_nr   = _IOW(MY_MAGIC, 9, struct ioctl_arg);
	unsigned int bad_size = _IOW(MY_MAGIC, 1, char[128]);
	unsigned int good     = _IOW(MY_MAGIC, 2, struct ioctl_arg);

	memset(__user_mem, 0, PAGE_SIZE);

	assert(dev_ioctl_good(bad_type, (void __user *)__user_mem) == -ENOTTY);
	assert(dev_ioctl_good(bad_nr, (void __user *)__user_mem) == -ENOTTY);
	assert(dev_ioctl_good(bad_size, (void __user *)__user_mem) == -EINVAL);
	assert(dev_ioctl_good(good, (void __user *)__user_mem) == 0);
	assert(dev_ioctl_compat(good, (void __user *)__user_mem) == 0);
	return 0;
}
