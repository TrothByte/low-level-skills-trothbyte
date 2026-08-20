/* GOOD: page cache & writeback reasoning — dirty pages are NOT durable until
 * writeback + fsync/fdatasync; writeback accounting is asserted against the
 * durable-store model; balance_dirty_pages throttles writers when the dirty
 * level crosses a threshold. */
#include "../stubs.h"
#include <assert.h>
#include <stdio.h>

static void fill(unsigned char *buf, size_t len, unsigned char seed)
{
	size_t i;
	for (i = 0; i < len; i++)
		buf[i] = (unsigned char)(i * 7u + seed);
}

int main(void)
{
	struct file_emu f;
	unsigned char buf[PAGE_CACHE_PAGE_SIZE * 12];
	size_t i;

	/* 1) write() leaves data in the page cache only; fsync makes it durable */
	reset_writeback_emu();
	init_file_emu(&f, 1);
	fill(buf, PAGE_CACHE_PAGE_SIZE * 3, 1);
	assert(write_emu(&f, buf, PAGE_CACHE_PAGE_SIZE * 3) ==
	       (long)(PAGE_CACHE_PAGE_SIZE * 3));
	assert(dirty_pages_get_emu() == 3UL);	  /* accounting: 3 dirty pages */
	assert(!durable_matches_emu(&f, 0, buf, PAGE_CACHE_PAGE_SIZE * 3));
	assert(fsync_emu(&f, 1) == 0);		  /* after fsync: durable */
	assert(durable_matches_emu(&f, 0, buf, PAGE_CACHE_PAGE_SIZE * 3));
	assert(dirty_pages_get_emu() == 0UL);	  /* writeback cleared them */

	/* 2) fdatasync persists data + essential metadata (size) but skips
	 *    non-essential metadata (mtime); fsync forces the metadata too */
	reset_writeback_emu();
	init_file_emu(&f, 2);
	fill(buf, PAGE_CACHE_PAGE_SIZE * 2, 2);
	assert(write_emu(&f, buf, PAGE_CACHE_PAGE_SIZE * 2) ==
	       (long)(PAGE_CACHE_PAGE_SIZE * 2));
	assert(fdatasync_emu(&f) == 0);
	assert(durable_matches_emu(&f, 0, buf, PAGE_CACHE_PAGE_SIZE * 2));
	assert(f.disk_size == f.size);		  /* size is essential */
	assert(f.disk_mtime != f.mtime);	  /* mtime NOT synced by fdatasync */
	assert(fsync_emu(&f, 1) == 0);
	assert(f.disk_mtime == f.mtime);	  /* fsync forced the metadata */

	/* 3) redirty race: a page redirtied while under writeback stays dirty
	 *    after completion; the durable store holds the old snapshot and
	 *    the next pass writes the new data */
	reset_writeback_emu();
	init_file_emu(&f, 3);
	fill(buf, PAGE_CACHE_PAGE_SIZE, 3);
	assert(write_emu(&f, buf, PAGE_CACHE_PAGE_SIZE) ==
	       (long)PAGE_CACHE_PAGE_SIZE);
	writeback_pass_emu();			  /* flusher snapshots page 0 */
	for (i = 0; i < PAGE_CACHE_PAGE_SIZE; i++)
		pages[0].data[i] = (unsigned char)(i + 0x80); /* concurrent writer */
	mark_page_dirty_emu(0);			  /* redirty while in writeback */
	writeback_complete_emu();
	assert(dirty_pages_get_emu() == 1UL);	  /* page still dirty */
	assert(durable_matches_emu(&f, 0, buf, PAGE_CACHE_PAGE_SIZE));
	assert(!durable_matches_emu(&f, 0, pages[0].data, PAGE_CACHE_PAGE_SIZE));
	writeback_run_emu();			  /* next flusher pass */
	assert(durable_matches_emu(&f, 0, pages[0].data, PAGE_CACHE_PAGE_SIZE));
	assert(dirty_pages_get_emu() == 0UL);

	/* 4) crossing dirty_background_ratio kicks the background flusher
	 *    without stalling the writing task */
	reset_writeback_emu();
	init_file_emu(&f, 4);
	fill(buf, PAGE_CACHE_PAGE_SIZE * 7, 4);	  /* 7/16 pages = 43% > 40% */
	assert(write_emu(&f, buf, PAGE_CACHE_PAGE_SIZE * 7) ==
	       (long)(PAGE_CACHE_PAGE_SIZE * 7));
	assert(writeback_passes_get_emu() == 1UL); /* background flusher ran */
	assert(throttle_stalls_get_emu() == 0UL);  /* task was NOT blocked */
	assert(dirty_pages_get_emu() == 0UL);	  /* drained to <= background */
	assert(durable_matches_emu(&f, 0, buf, PAGE_CACHE_PAGE_SIZE * 7));

	/* 5) crossing dirty_ratio throttles the writing task in
	 *    balance_dirty_pages until the dirty level drops to background */
	reset_writeback_emu();
	init_file_emu(&f, 5);
	fill(buf, PAGE_CACHE_PAGE_SIZE * 12, 5);  /* 12/16 pages = 75% > 60% */
	assert(write_emu(&f, buf, PAGE_CACHE_PAGE_SIZE * 12) ==
	       (long)(PAGE_CACHE_PAGE_SIZE * 12));
	assert(throttle_stalls_get_emu() > 0UL);  /* task was throttled */
	assert(dirty_pages_get_emu() == 0UL);
	assert(durable_matches_emu(&f, 0, buf, PAGE_CACHE_PAGE_SIZE * 12));

	/* 6) writeback errors are deferred: fsync reports -EIO even though
	 *    every write() returned success */
	reset_writeback_emu();
	init_file_emu(&f, 6);
	fill(buf, PAGE_CACHE_PAGE_SIZE * 2, 6);
	assert(write_emu(&f, buf, PAGE_CACHE_PAGE_SIZE * 2) ==
	       (long)(PAGE_CACHE_PAGE_SIZE * 2));
	inject_io_error_emu();
	assert(fsync_emu(&f, 1) == -EIO);
	assert(!durable_matches_emu(&f, 0, buf, PAGE_CACHE_PAGE_SIZE * 2));

	/* 7) O_SYNC: a sync write is durable when the call returns, with no
	 *    explicit fsync needed */
	reset_writeback_emu();
	init_file_emu(&f, 7);
	fill(buf, PAGE_CACHE_PAGE_SIZE * 2, 7);
	assert(write_sync_emu(&f, buf, PAGE_CACHE_PAGE_SIZE * 2) ==
	       (long)(PAGE_CACHE_PAGE_SIZE * 2));
	assert(durable_matches_emu(&f, 0, buf, PAGE_CACHE_PAGE_SIZE * 2));
	assert(dirty_pages_get_emu() == 0UL);

	printf("ALL CHECKS PASSED\n");
	return 0;
}
