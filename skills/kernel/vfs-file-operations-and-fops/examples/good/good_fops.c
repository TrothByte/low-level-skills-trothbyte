/* GOOD: VFS-contract-correct file_operations.
 *
 * - open allocates private_data and pins the module via .owner
 * - read returns bytes transferred; 0 is EOF, not an error
 * - write returns bytes transferred and rejects oversized requests
 * - llseek validates SEEK_SET/SEEK_CUR/SEEK_END and negative offsets
 * - ioctl dispatches through unlocked_ioctl and shares a layout-safe
 *   compat_ioctl
 * - release frees private_data exactly once, at the last fput
 */
#include "../stubs.h"
#include <assert.h>
#include <stdio.h>

#define DEV_IOC_RESET    1
#define DEV_IOC_GET_USED 2

struct dev_ctx {
	unsigned char buf[8];
	size_t used;
	int magic;
};

static int release_count;
static const int ctx_magic = 0x504f;

static int good_open(struct inode *inode, struct file *file)
{
	struct dev_ctx *ctx;

	(void)inode;
	ctx = (struct dev_ctx *)kmalloc_emu(sizeof *ctx);
	if (ctx == NULL)
		return -ENOMEM;
	ctx->used = 0;
	ctx->magic = ctx_magic;
	file->private_data = ctx;   /* private_data is born in open */
	return 0;
}

static int good_release(struct inode *inode, struct file *file)
{
	struct dev_ctx *ctx;

	(void)inode;
	ctx = (struct dev_ctx *)file->private_data;
	assert(ctx != NULL);
	assert(ctx->magic == ctx_magic);
	kfree_emu(ctx);             /* freed once, at the last fput */
	file->private_data = NULL;
	release_count++;
	return 0;
}

static ssize_t good_read(struct file *file, char __user *buf, size_t count,
			 loff_t *pos)
{
	struct dev_ctx *ctx = (struct dev_ctx *)file->private_data;
	size_t avail;

	assert(ctx != NULL);
	assert(*pos >= 0);
	if ((unsigned long)*pos >= ctx->used)
		return 0;           /* EOF: no data, not an error */
	avail = ctx->used - (unsigned long)*pos;
	if (count > avail)
		count = avail;
	copy_to_user_emu(buf, ctx->buf + *pos, count);
	return (ssize_t)count;      /* bytes transferred; VFS advances *pos */
}

static ssize_t good_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *pos)
{
	struct dev_ctx *ctx = (struct dev_ctx *)file->private_data;
	size_t room;

	assert(ctx != NULL);
	assert(*pos >= 0);
	if (count > sizeof ctx->buf)
		return -EINVAL;     /* reject the oversized request */
	if ((unsigned long)*pos >= sizeof ctx->buf)
		return 0;
	room = sizeof ctx->buf - (unsigned long)*pos;
	if (count > room)
		count = room;       /* a short write is a short positive count */
	copy_from_user_emu(ctx->buf + *pos, buf, count);
	if ((unsigned long)*pos + count > ctx->used)
		ctx->used = (unsigned long)*pos + count;
	return (ssize_t)count;      /* bytes transferred, never "0 for ok" */
}

static loff_t good_llseek(struct file *file, loff_t offset, int whence)
{
	struct dev_ctx *ctx = (struct dev_ctx *)file->private_data;
	loff_t npos;

	assert(ctx != NULL);
	switch (whence) {
	case SEEK_SET:
		npos = offset;
		break;
	case SEEK_CUR:
		npos = file->f_pos + offset;
		break;
	case SEEK_END:
		npos = (loff_t)ctx->used + offset;
		break;
	default:
		return -EINVAL;     /* unknown whence */
	}
	if (npos < 0)
		return -EINVAL;     /* negative offsets rejected */
	file->f_pos = npos;
	return npos;
}

static long good_unlocked_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct dev_ctx *ctx = (struct dev_ctx *)file->private_data;

	assert(ctx != NULL);
	switch (cmd) {
	case DEV_IOC_RESET:
		ctx->used = 0;
		return 0;
	case DEV_IOC_GET_USED:
		__user_mem[arg] = (unsigned char)ctx->used;
		return 0;
	default:
		return -ENOTTY;     /* unknown command -> -ENOTTY */
	}
}

static struct file_operations good_fops = {
	.owner           = THIS_MODULE,          /* module pinned while open */
	.llseek          = good_llseek,
	.read            = good_read,
	.write           = good_write,
	.open            = good_open,
	.release         = good_release,
	.unlocked_ioctl  = good_unlocked_ioctl,
	.compat_ioctl    = good_unlocked_ioctl,  /* same layout: sharing is ok */
};

int main(void)
{
	struct file f1, f2;
	struct file *dup;
	struct dev_ctx *ctx;
	char __user *ubuf = (char __user *)__user_mem;
	loff_t pos = 0;
	ssize_t n;

	memset(&f1, 0, sizeof f1);
	f1.f_op = &good_fops;
	assert(vfs_open_emu(&f1) == 0);
	assert(f1.private_data != NULL);
	assert(module_refcnt == 1);   /* .owner pinned the module */

	/* rmmod while the file is open must fail: refcount is held */
	unload_module_emu();
	assert(module_unloaded == 0);

	ctx = (struct dev_ctx *)f1.private_data;

	/* write: returns bytes transferred, data lands in the device */
	memset(__user_mem, 'W', 64);
	n = vfs_write_emu(&f1, ubuf, 5, &pos);
	assert(n == 5);
	assert(pos == 5);
	assert(memcmp(ctx->buf, __user_mem, 5) == 0);

	/* write: oversized request rejected with -EINVAL, pos untouched */
	n = vfs_write_emu(&f1, ubuf, 64, &pos);
	assert(n == -EINVAL);
	assert(pos == 5);

	/* read: returns bytes copied; 0 is EOF and does not advance pos */
	memset(__user_mem, 0, 64);
	pos = 0;
	n = vfs_read_emu(&f1, ubuf, 3, &pos);
	assert(n == 3);
	assert(memcmp(ubuf, ctx->buf, 3) == 0);
	assert(pos == 3);
	n = vfs_read_emu(&f1, ubuf, 64, &pos);   /* drains the remaining 2 */
	assert(n == 2);
	assert(pos == 5);
	n = vfs_read_emu(&f1, ubuf, 64, &pos);   /* now at EOF */
	assert(n == 0);                         /* EOF, not an error */
	assert(pos == 5);                       /* EOF does not advance pos */

	/* llseek: SEEK_SET / SEEK_CUR / SEEK_END, negatives and whence */
	assert(vfs_llseek_emu(&f1, 1, SEEK_SET) == 1);
	assert(vfs_llseek_emu(&f1, 2, SEEK_CUR) == 3);
	assert(vfs_llseek_emu(&f1, 0, SEEK_END) == 5);
	assert(vfs_llseek_emu(&f1, -1, SEEK_SET) == -EINVAL);
	assert(vfs_llseek_emu(&f1, 0, 99) == -EINVAL);

	/* ioctl dispatch and a layout-safe compat path */
	assert(vfs_ioctl_emu(&f1, DEV_IOC_GET_USED, 0) == 0);
	assert(__user_mem[0] == 5);
	assert(vfs_ioctl_emu(&f1, DEV_IOC_RESET, 0) == 0);
	assert(ctx->used == 0);
	assert(vfs_ioctl_emu(&f1, 99, 0) == -ENOTTY);
	assert(compat_ioctl_emu(&f1, DEV_IOC_GET_USED, 1) == 0);

	/* second independent open/close: private_data per file */
	memset(&f2, 0, sizeof f2);
	f2.f_op = &good_fops;
	assert(vfs_open_emu(&f2) == 0);
	assert(f2.private_data != NULL);
	assert(module_refcnt == 2);
	vfs_fput_emu(&f2);
	assert(release_count == 1);
	assert(module_refcnt == 1);

	/* dup shares the file object: release runs only at the LAST fput */
	dup = &f1;
	get_file_emu(dup);
	assert(f1.f_count == 2);
	vfs_fput_emu(&f1);
	assert(release_count == 1);   /* not released yet */
	vfs_fput_emu(dup);          /* last fput -> release */
	assert(release_count == 2);
	assert(module_refcnt == 0);  /* module refcount balanced */
	assert(f1.private_data == NULL);

	printf("ALL CHECKS PASSED\n");
	return 0;
}
