/* BAD: device I/O region mapped with default (cacheable) protection. */
#include "../stubs.h"
#include <stdio.h>
#include <string.h>

/* BAD: no pgprot_noncached / pgprot_writecombine override; the CPU cache
 * may absorb MMIO writes and serve stale device reads */
static long dev_mmap_bad(struct vm_area_struct *vma, unsigned long pfn)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	return remap_pfn_range_emu(vma, vma->vm_start, pfn, size,
				   vma->vm_page_prot);
}

int main(void)
{
	struct vm_area_struct vma;

	memset(&vma, 0, sizeof vma);
	vma.vm_start = 0x1000;
	vma.vm_end = 0x2000;
	vma.vm_page_prot = 0;

	dev_mmap_bad(&vma, 0x1010);
	if (last_map_prot_noncached == 0)
		printf("BUG reproduced: device I/O mapped cacheable\n");
	return 0;
}
