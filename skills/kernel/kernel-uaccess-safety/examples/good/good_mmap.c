/* GOOD: device I/O mapping uses non-cached protection (pgprot_noncached). */
#include "../stubs.h"
#include <assert.h>
#include <string.h>

static long dev_mmap_good(struct vm_area_struct *vma, unsigned long pfn)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	return remap_pfn_range_emu(vma, vma->vm_start, pfn, size,
				   pgprot_noncached(vma->vm_page_prot));
}

int main(void)
{
	struct vm_area_struct vma;

	memset(&vma, 0, sizeof vma);
	vma.vm_start = 0x1000;
	vma.vm_end = 0x2000;
	vma.vm_page_prot = 0;

	assert(dev_mmap_good(&vma, 0x1010) == 0);
	assert(last_map_prot_noncached == 1);
	assert(dev_mmap_good(&vma, 0x0100) == -EINVAL);
	assert(dev_mmap_good(&vma, 0x1020) == -EINVAL);
	return 0;
}
