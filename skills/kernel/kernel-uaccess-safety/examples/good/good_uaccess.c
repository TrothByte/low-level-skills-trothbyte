/* GOOD: fault-safe uaccess — every helper return value is checked. */
#include "../stubs.h"
#include <assert.h>
#include <string.h>

/* read path: copy_to_user failure must surface as -EFAULT */
static long dev_read_good(char __user *buf, unsigned long n,
			  const char *kbuf, unsigned long ksize)
{
	if (n > ksize)
		return -EINVAL;
	if (copy_to_user_emu(buf, kbuf, n))
		return -EFAULT;
	return (long)n;
}

/* write path: never use the buffer after an unchecked copy_from_user */
static long dev_write_good(char *kbuf, unsigned long ksize,
			   const char __user *buf, unsigned long n)
{
	if (n > ksize)
		return -EINVAL;
	if (copy_from_user_emu(kbuf, buf, n))
		return -EFAULT;
	return (long)n;
}

/* LDD3 get_user pattern: access_ok first, then the raw helper */
static long dev_getint_good(const void __user *arg)
{
	int x;
	if (!access_ok_emu(arg, sizeof(int)))
		return -EFAULT;
	if (__get_user_emu(&x, (const int __user *)arg, sizeof(int)))
		return -EFAULT;
	return x;
}

static long dev_putint_good(void __user *arg, int val)
{
	if (!access_ok_emu(arg, sizeof(int)))
		return -EFAULT;
	if (__put_user_emu((int __user *)arg, &val, sizeof(int)))
		return -EFAULT;
	return 0;
}

/* string paths: negative returns are errors; NUL is already counted */
static long dev_getname_good(char *dst, const char __user *src, long count)
{
	long len = strncpy_from_user_emu(dst, src, count);
	if (len < 0)
		return len;
	return len;
}

static long dev_checkname_good(const char __user *src, long count)
{
	long len = strnlen_user_emu(src, count);
	if (len <= 0 || len > count)
		return -EINVAL;
	return len;
}

int main(void)
{
	char __user *ubuf = (char __user *)__user_mem;
	char kbuf[64];
	long r;

	memset(__user_mem, 'U', PAGE_SIZE);
	memset(kbuf, 0, sizeof kbuf);

	/* write: valid user pointer, data must land in kbuf */
	r = dev_write_good(kbuf, sizeof kbuf, ubuf, 16);
	assert(r == 16);
	assert(memcmp(kbuf, __user_mem, 16) == 0);

	/* write: out-of-range pointer -> -EFAULT, buffer untouched */
	memset(kbuf, 0, sizeof kbuf);
	r = dev_write_good(kbuf, sizeof kbuf, (const char __user *)kbuf, 16);
	assert(r == -EFAULT);
	assert(kbuf[0] == 0);

	/* read: valid range copies kernel data out */
	memset(__user_mem, 'V', PAGE_SIZE);
	r = dev_read_good(ubuf, 5, "hello", 5);
	assert(r == 5);
	assert(memcmp(__user_mem, "hello", 5) == 0);

	/* read: range straddles the end of user space -> -EFAULT */
	r = dev_read_good(ubuf + PAGE_SIZE - 4, 8, "01234567", 8);
	assert(r == -EFAULT);

	/* get_user / put_user round-trip through the emulated user region */
	((int *)__user_mem)[0] = 7;
	assert(dev_getint_good(__user_mem) == 7);
	assert(dev_getint_good((const void __user *)kbuf) == -EFAULT);
	assert(dev_putint_good(__user_mem, 42) == 0);
	assert(((int *)__user_mem)[0] == 42);

	/* strncpy_from_user returns length including the trailing NUL */
	memset(kbuf, 0, sizeof kbuf);
	strcpy((char *)__user_mem, "hi");
	assert(dev_getname_good(kbuf, (const char __user *)__user_mem, 64) == 3);

	/* strnlen_user: unterminated within count -> count+1 -> rejected */
	memset(__user_mem, 'x', PAGE_SIZE);
	assert(dev_checkname_good((const char __user *)__user_mem, 64) == -EINVAL);

	/* strnlen_user: terminated string is accepted and bounded */
	strcpy((char *)__user_mem, "ok");
	assert(dev_checkname_good((const char __user *)__user_mem, 64) == 3);

	return 0;
}
