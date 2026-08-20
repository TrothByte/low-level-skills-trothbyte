/*
 * stubs.h — self-contained host stubs for Linux VFS file_operations code.
 * Models just enough of the VFS to exercise the fops contract on the host:
 * a struct file with a refcount, a struct file_operations, dispatch
 * wrappers (vfs_open_emu / vfs_read_emu / vfs_write_emu / vfs_llseek_emu /
 * vfs_ioctl_emu / compat_ioctl_emu / vfs_fput_emu) that enforce the
 * documented VFS return conventions, a fake __user region, a static-pool
 * heap whose freed blocks are poisoned (so a stale-pointer read yields
 * deterministic 0xAA bytes instead of a host crash), and a module refcount.
 * No kernel headers required. Not kernel code.
 */
#ifndef VFS_FOPS_STUBS_H
#define VFS_FOPS_STUBS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define __user

#define EINVAL    22
#define EFAULT    14
#define EBADF      9
#define EBUSY     16
#define ENOMEM    12
#define ENOTTY    25
#define ESPIPE    29
#define EOVERFLOW 75

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef long loff_t;
#ifndef _SSIZE_T_DEFINED
#ifndef __ssize_t_defined
typedef long ssize_t;
#define __ssize_t_defined
#endif
#endif

/* ---- fake user memory region (__user pointers point into it) ---- */
unsigned char __user_mem[4096];

static inline void copy_to_user_emu(void __user *dst, const void *src,
                                    size_t n)
{
	memcpy((void *)(uintptr_t)dst, src, n);
}

static inline void copy_from_user_emu(void *dst, const void __user *src,
                                      size_t n)
{
	memcpy(dst, (const void *)(uintptr_t)src, n);
}

/* ---- emulated heap: static pool, freed regions poisoned, never reused ---- */
#define POISON_BYTE 0xAA
#define MAX_ALLOCS 64

struct alloc_slot {
	void *ptr;
	size_t size;
	int live;
};

struct alloc_slot alloc_table[MAX_ALLOCS];
unsigned char alloc_pool[8192];
size_t pool_offset;

static inline void *kmalloc_emu(size_t size)
{
	void *p;
	size_t i;
	size = (size + 15u) & ~(size_t)15u;
	if (pool_offset + size > sizeof alloc_pool)
		return NULL;
	p = alloc_pool + pool_offset;
	pool_offset += size;
	memset(p, 0, size);
	for (i = 0; i < MAX_ALLOCS; i++) {
		if (!alloc_table[i].live) {
			alloc_table[i].ptr = p;
			alloc_table[i].size = size;
			alloc_table[i].live = 1;
			return p;
		}
	}
	return NULL;
}

static inline void kfree_emu(void *ptr)
{
	size_t i;
	if (ptr == NULL)
		return;
	for (i = 0; i < MAX_ALLOCS; i++) {
		if (alloc_table[i].live && alloc_table[i].ptr == ptr) {
			memset(ptr, POISON_BYTE, alloc_table[i].size);
			alloc_table[i].live = 0;
			return;
		}
	}
}

/* ---- module refcount: .owner pins the module while a file is open ---- */
struct module {
	int refcnt;
};

struct module demo_module;
int module_refcnt;
int module_unloaded;

#define THIS_MODULE (&demo_module)

static inline int try_module_get(struct module *m)
{
	if (m == NULL || module_unloaded)
		return 0;
	m->refcnt++;
	module_refcnt++;
	return 1;
}

static inline void module_put(struct module *m)
{
	if (m == NULL)
		return;
	m->refcnt--;
	module_refcnt--;
}

static inline void unload_module_emu(void)
{
	if (module_refcnt == 0)
		module_unloaded = 1;
}

/* ---- VFS objects ---- */
struct inode;  /* opaque in this harness */

struct file {
	void *private_data;
	loff_t f_pos;
	unsigned long f_count;
	struct file_operations *f_op;
};

struct file_operations {
	struct module *owner;
	loff_t (*llseek)(struct file *, loff_t, int);
	ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
	int (*open)(struct inode *, struct file *);
	int (*release)(struct inode *, struct file *);
	long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
	long (*compat_ioctl)(struct file *, unsigned int, unsigned long);
};

/* ---- VFS dispatch wrappers (enforce the documented conventions) ---- */
static inline int vfs_open_emu(struct file *file)
{
	struct file_operations *fop = file->f_op;
	int ret;

	if (fop == NULL)
		return -EBADF;
	if (file->f_count != 0)
		return -EBUSY;
	file->f_count = 1;
	file->f_pos = 0;
	if (fop->owner != NULL && !try_module_get(fop->owner))
		return -EBUSY;  /* module being unloaded */
	if (fop->open != NULL) {
		ret = fop->open(NULL, file);
		if (ret != 0) {
			if (fop->owner != NULL)
				module_put(fop->owner);
			file->f_count = 0;
			return ret;
		}
	}
	return 0;
}

static inline struct file *get_file_emu(struct file *file)
{
	file->f_count++;
	return file;
}

static inline void vfs_fput_emu(struct file *file)
{
	struct file_operations *fop = file->f_op;

	if (file->f_count == 0)
		return;
	file->f_count--;
	if (file->f_count == 0) {
		/* release runs exactly once, at the LAST fput */
		if (fop->release != NULL)
			fop->release(NULL, file);
		if (fop->owner != NULL)
			module_put(fop->owner);
	}
}

static inline ssize_t vfs_read_emu(struct file *file, char __user *buf,
				   size_t count, loff_t *pos)
{
	ssize_t ret;

	if (file->f_count == 0 || file->f_op->read == NULL)
		return -EBADF;
	ret = file->f_op->read(file, buf, count, pos);
	if (ret > (ssize_t)count)
		return -EOVERFLOW;  /* outside the VFS return contract */
	if (ret > 0)
		*pos += ret;      /* VFS advances f_pos on a positive ret */
	return ret;
}

static inline ssize_t vfs_write_emu(struct file *file, const char __user *buf,
				    size_t count, loff_t *pos)
{
	ssize_t ret;

	if (file->f_count == 0 || file->f_op->write == NULL)
		return -EBADF;
	ret = file->f_op->write(file, buf, count, pos);
	if (ret > (ssize_t)count)
		return -EOVERFLOW;
	if (ret > 0)
		*pos += ret;
	return ret;
}

static inline loff_t vfs_llseek_emu(struct file *file, loff_t offset, int whence)
{
	if (file->f_count == 0 || file->f_op->llseek == NULL)
		return -ESPIPE;
	return file->f_op->llseek(file, offset, whence);
}

static inline long vfs_ioctl_emu(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	if (file->f_count == 0 || file->f_op->unlocked_ioctl == NULL)
		return -ENOTTY;
	return file->f_op->unlocked_ioctl(file, cmd, arg);
}

static inline long compat_ioctl_emu(struct file *file, unsigned int cmd,
				    unsigned long arg)
{
	if (file->f_count == 0 || file->f_op->compat_ioctl == NULL)
		return -ENOTTY;  /* 32-bit caller, no compat handler */
	return file->f_op->compat_ioctl(file, cmd, arg);
}

#endif
