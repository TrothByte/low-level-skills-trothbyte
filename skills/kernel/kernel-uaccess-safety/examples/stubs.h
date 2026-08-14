/*
 * stubs.h — self-contained host stubs for Linux uaccess-shaped code.
 * Models the user address space as a fixed global buffer so that
 * copy_*_user / get_user / put_user / strn*_user semantics (return bytes
 * NOT copied, -EFAULT, count+1 sentinel) can be exercised with a plain gcc
 * build. No kernel headers required. Not kernel code.
 *
 * Copies are done byte-wise through uintptr_t + volatile so GCC's static
 * bounds analysis cannot re-derive the source object and report false
 * -Warray-bounds: like the real kernel fixup path, the stub only knows
 * addresses, and the access_ok gate is the actual safety boundary.
 */
#ifndef KERNEL_UACCESS_STUBS_H
#define KERNEL_UACCESS_STUBS_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define __user
#define __iomem

#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)

#define EFAULT 14
#define EINVAL 22
#define ENOTTY 25
#define E2BIG  7
#define ENOMEM 12

#define POLLIN   0x0001u
#define POLLOUT  0x0004u
#define POLLERR  0x0008u
#define POLL_IN  0x0001u
#define SIGIO    29

typedef unsigned long pgprot_t;

unsigned char __user_mem[PAGE_SIZE];

static inline void memcpy_emu(void *to, const void *from, unsigned long n)
{
	volatile unsigned char *d = (volatile unsigned char *)(uintptr_t)to;
	const volatile unsigned char *s =
		(const volatile unsigned char *)(uintptr_t)from;
	unsigned long i;
	for (i = 0; i < n; i++)
		d[i] = s[i];
}

/* emulated user address space check: addr+size must fit in __user_mem */
static inline int access_ok_emu(const void __user *addr, unsigned long size)
{
	uintptr_t a = (uintptr_t)addr;
	uintptr_t lo = (uintptr_t)__user_mem;
	uintptr_t hi = lo + PAGE_SIZE;
	return size <= PAGE_SIZE && a >= lo && a <= hi - size;
}

/* checked helpers: perform access_ok internally (like the kernel API) */
static inline unsigned long copy_to_user_emu(void __user *to, const void *from,
					     unsigned long n)
{
	if (n && !access_ok_emu(to, n))
		return n;
	if (n)
		memcpy_emu(to, from, n);
	return 0;
}

static inline unsigned long copy_from_user_emu(void *to, const void __user *from,
					       unsigned long n)
{
	if (n && !access_ok_emu(from, n))
		return n;
	if (n)
		memcpy_emu(to, from, n);
	return 0;
}

static inline int get_user_emu(void *dst, const void __user *src, size_t sz)
{
	if (!access_ok_emu(src, sz))
		return -EFAULT;
	memcpy_emu(dst, src, sz);
	return 0;
}

static inline int put_user_emu(void __user *dst, const void *src, size_t sz)
{
	if (!access_ok_emu(dst, sz))
		return -EFAULT;
	memcpy_emu(dst, src, sz);
	return 0;
}

/* raw helpers: no access_ok — caller must check it first (LDD3 pattern) */
static inline unsigned long __copy_to_user_emu(void __user *to, const void *from,
					       unsigned long n)
{
	if (n)
		memcpy_emu(to, from, n);
	return 0;
}

static inline unsigned long __copy_from_user_emu(void *to, const void __user *from,
						 unsigned long n)
{
	if (n)
		memcpy_emu(to, from, n);
	return 0;
}

static inline int __get_user_emu(void *dst, const void __user *src, size_t sz)
{
	memcpy_emu(dst, src, sz);
	return 0;
}

static inline int __put_user_emu(void __user *dst, const void *src, size_t sz)
{
	memcpy_emu(dst, src, sz);
	return 0;
}

/* strncpy_from_user: returns length INCLUDING trailing NUL, or -EFAULT /
 * -E2BIG. strnlen_user: returns length incl. NUL, 0 on fault, count+1 if
 * unterminated within count. */
static inline long strncpy_from_user_emu(char *dst, const char __user *src,
					 long count)
{
	volatile unsigned char *d = (volatile unsigned char *)(uintptr_t)dst;
	const volatile unsigned char *s =
		(const volatile unsigned char *)(uintptr_t)src;
	long i;
	if (count <= 0)
		return 0;
	if (!access_ok_emu(src, (unsigned long)count))
		return -EFAULT;
	for (i = 0; i < count; i++) {
		unsigned char c = s[i];
		d[i] = c;
		if (c == '\0')
			return i + 1;
	}
	return -E2BIG;
}

static inline long strnlen_user_emu(const char __user *src, long count)
{
	const volatile unsigned char *s =
		(const volatile unsigned char *)(uintptr_t)src;
	long i;
	if (!access_ok_emu(src, (unsigned long)count))
		return 0;
	for (i = 0; i < count; i++)
		if (s[i] == '\0')
			return i + 1;
	return count + 1;
}

/* ioctl command encoding (uapi asm-generic layout) */
typedef unsigned int cmd_t;
#define _IOC_NRBITS   8
#define _IOC_TYPEBITS 8
#define _IOC_SIZEBITS 14
#define _IOC_DIRBITS  2
#define _IOC_NRSHIFT  (_IOC_SIZEBITS)
#define _IOC_TYPESHIFT (_IOC_SIZEBITS + _IOC_NRBITS)
#define _IOC_DIRSHIFT (_IOC_SIZEBITS + _IOC_NRBITS + _IOC_TYPEBITS)
#define _IOC_NONE  0u
#define _IOC_WRITE 1u
#define _IOC_READ  2u
#define _IOC(dir, type, nr, size) \
	(((cmd_t)(dir) << _IOC_DIRSHIFT) | \
	 ((cmd_t)(type) << _IOC_TYPESHIFT) | \
	 ((cmd_t)(nr) << _IOC_NRSHIFT) | \
	 ((cmd_t)(size)))
#define _IO(type, nr)      _IOC(_IOC_NONE, type, nr, 0)
#define _IOR(type, nr, t)  _IOC(_IOC_READ, type, nr, sizeof(t))
#define _IOW(type, nr, t)  _IOC(_IOC_WRITE, type, nr, sizeof(t))
#define _IOWR(type, nr, t) _IOC(_IOC_READ | _IOC_WRITE, type, nr, sizeof(t))
#define _IOC_DIR(cmd)  (((cmd) >> _IOC_DIRSHIFT) & ((1u << _IOC_DIRBITS) - 1))
#define _IOC_TYPE(cmd) (((cmd) >> _IOC_TYPESHIFT) & ((1u << _IOC_TYPEBITS) - 1))
#define _IOC_NR(cmd)   (((cmd) >> _IOC_NRSHIFT) & ((1u << _IOC_NRBITS) - 1))
#define _IOC_SIZE(cmd) ((cmd) & ((1u << _IOC_SIZEBITS) - 1))

/* mmap stubs: emulated device I/O region at pfns [0x1000, 0x1020) */
struct vm_area_struct {
	unsigned long vm_start;
	unsigned long vm_end;
	unsigned long vm_pgoff;
	pgprot_t vm_page_prot;
};

int last_map_prot_noncached;

static inline pgprot_t pgprot_noncached(pgprot_t p)
{
	return p | 0x4u;
}

static inline pgprot_t pgprot_writecombine(pgprot_t p)
{
	return p | 0x2u;
}

static inline int remap_pfn_range_emu(struct vm_area_struct *vma,
				      unsigned long addr, unsigned long pfn,
				      unsigned long size, pgprot_t prot)
{
	unsigned long npages;
	(void)vma;
	(void)addr;
	if (size == 0)
		return -EINVAL;
	npages = size / PAGE_SIZE + (size % PAGE_SIZE != 0);
	if (pfn < 0x1000ul || pfn + npages > 0x1020ul)
		return -EINVAL;
	last_map_prot_noncached = (prot & 0x4u) != 0;
	return 0;
}

/* poll / fasync stubs */
struct poll_table_struct;
typedef struct poll_table_struct poll_table;

struct wait_queue_head {
	int fake;
};

struct file {
	void *private_data;
};

int fasync_registered;
int fasync_signalled;

static inline void poll_wait_emu(void *filp, struct wait_queue_head *wait,
				 poll_table *pt)
{
	(void)filp;
	(void)wait;
	(void)pt;
}

static inline int fasync_helper_emu(int fd, void *filp, int on, int *reg)
{
	(void)fd;
	(void)filp;
	*reg = on;
	fasync_registered = *reg;
	return 0;
}

static inline void kill_fasync_emu(int *reg, int sig, int band)
{
	(void)sig;
	(void)band;
	if (*reg)
		fasync_signalled = 1;
}

#endif
