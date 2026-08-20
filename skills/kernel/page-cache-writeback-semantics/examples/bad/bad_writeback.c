/* BAD: assumes write() made the data durable and skips fsync. The write
 * path only filled the page cache and marked the pages dirty; the durable
 * store is untouched until a flusher pass or fsync runs. */
#include "../stubs.h"
#include <stdio.h>

int main(void)
{
	struct file_emu f;
	unsigned char buf[PAGE_CACHE_PAGE_SIZE * 3];
	size_t i;

	for (i = 0; i < sizeof buf; i++)
		buf[i] = (unsigned char)(i * 13u + 1);

	reset_writeback_emu();
	init_file_emu(&f, 1);

	if (write_emu(&f, buf, sizeof buf) != (long)sizeof buf) {
		printf("write_emu failed\n");
		return 1;
	}

	/* BUG: no fsync/fdatasync before assuming the data is on disk.
	 * 3/16 pages = 18% < dirty_background_ratio, so no flusher ran:
	 * the durable store still holds its reset sentinel (0xFF). */
	if (!durable_matches_emu(&f, 0, buf, sizeof buf)) {
		printf("BUG reproduced: write() returned but data not durable\n");
		return 0;
	}

	printf("BUG NOT reproduced: data durable without fsync\n");
	return 1;
}
