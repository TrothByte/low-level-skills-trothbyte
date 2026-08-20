/* BAD: a driver that violates three fops contracts at once.
 *
 * 1) .owner is NULL: the module is not pinned, so rmmod can unload it
 *    while a file is still open (the "release after module unload" pitfall).
 * 2) .write returns bytes NOT transferred instead of bytes written.
 * 3) .release frees private_data while the module-level cache last_ctx still
 *    aliases it — a later .read on a fresh file reads the freed object
 *    (use-after-free). The harness detects it via the poisoned pool bytes.
 *
 * The stubs make every flaw observable at runtime; nothing crashes.
 */
#include "../stubs.h"
#include <assert.h>
#include <stdio.h>

struct dev_ctx {
	unsigned char buf[8];
	size_t used;
};

static struct dev_ctx *last_ctx;   /* BAD: module-level cache, never refreshed */

static int bad_open(struct inode *inode, struct file *file)
{
	struct dev_ctx *ctx;

	(void)inode;
	ctx = (struct dev_ctx *)kmalloc_emu(sizeof *ctx);
	if (ctx == NULL)
		return -ENOMEM;
	ctx->used = 0;
	file->private_data = ctx;
	if (last_ctx == NULL)
		last_ctx = ctx;     /* BAD: caches only the first instance */
	return 0;
}

static int bad_release(struct inode *inode, struct file *file)
{
	struct dev_ctx *ctx;

	(void)inode;
	ctx = (struct dev_ctx *)file->private_data;
	if (ctx != NULL) {
		kfree_emu(ctx);     /* BAD: last_ctx still aliases this memory */
		file->private_data = NULL;
	}
	return 0;
}

static ssize_t bad_read(struct file *file, char __user *buf, size_t count,
			loff_t *pos)
{
	struct dev_ctx *ctx;

	(void)file;
	(void)pos;
	ctx = last_ctx;             /* BAD: stale pointer into freed memory */
	if (ctx == NULL || count > sizeof ctx->buf)
		count = sizeof ctx->buf;
	copy_to_user_emu(buf, ctx->buf, count);
	return (ssize_t)count;
}

static ssize_t bad_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *pos)
{
	struct dev_ctx *ctx = (struct dev_ctx *)file->private_data;
	size_t nwritten;

	(void)pos;
	if (ctx == NULL)
		return -EFAULT;
	nwritten = count;
	if (nwritten > sizeof ctx->buf)
		nwritten = sizeof ctx->buf;   /* device capacity is 8 */
	copy_from_user_emu(ctx->buf, buf, nwritten);
	ctx->used = nwritten;
	/* BAD: returns bytes NOT transferred; the VFS will believe
	 * (count - nwritten) bytes were written and advance f_pos by it */
	return (ssize_t)(count - nwritten);
}

static struct file_operations bad_fops = {
	.owner   = NULL,            /* BAD: .owner missing */
	.read    = bad_read,
	.write   = bad_write,
	.open    = bad_open,
	.release = bad_release,
};

int main(void)
{
	struct file f1, f2;
	char __user *ubuf = (char __user *)__user_mem;
	loff_t pos = 0;
	ssize_t n;
	int bugs = 0;
	int i;

	/* 1) missing .owner: the module can be unloaded while f1 is open */
	memset(&f1, 0, sizeof f1);
	f1.f_op = &bad_fops;
	assert(vfs_open_emu(&f1) == 0);
	unload_module_emu();
	if (module_unloaded) {
		printf("BUG reproduced: module unloaded while a file is open "
		       "(.owner missing)\n");
		bugs++;
	}

	/* 2) write returns bytes NOT transferred (24, device wrote 8) */
	memset(__user_mem, 'B', 64);
	n = vfs_write_emu(&f1, ubuf, 32, &pos);
	if (n == 24) {
		printf("BUG reproduced: write returned bytes NOT transferred "
		       "(24; device capacity 8)\n");
		bugs++;
	}

	/* close f1: release frees private_data, last_ctx keeps a stale alias */
	vfs_fput_emu(&f1);

	/* open f2: last_ctx is NOT refreshed (stale pointer is kept) */
	memset(&f2, 0, sizeof f2);
	f2.f_op = &bad_fops;
	vfs_open_emu(&f2);

	/* 3) read f2: the read path dereferences the freed last_ctx; the
	 * freed region was poisoned to 0xAA, so no host crash occurs but the
	 * data is deterministically wrong. */
	memset(__user_mem, 0, 64);
	n = vfs_read_emu(&f2, ubuf, 8, &pos);
	if (n == 8) {
		int poisoned = 1;
		for (i = 0; i < (int)n; i++)
			if ((unsigned char)ubuf[i] != POISON_BYTE)
				poisoned = 0;
		if (poisoned) {
			printf("BUG reproduced: use-after-free of private_data "
			       "(read returned freed memory)\n");
			bugs++;
		}
	}

	printf("BUG reproduced: driver violates the fops contract in %d ways\n",
	       bugs);
	return 0;
}
