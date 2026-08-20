/*
 * stubs.h — self-contained host stubs modeling the Linux page cache and
 * writeback pipeline. Tracks per-page state (clean/dirty/writeback), the
 * dirty-page counter, the dirty_background_ratio / dirty_ratio thresholds,
 * the flusher writeback loop, and a separate "durable" disk store that ONLY
 * writeback updates. fsync/fdatasync/O_SYNC semantics (data + essential
 * metadata vs data-only) are emulated so page-cache durability bugs are
 * observable with a plain gcc build. No kernel headers required.
 * Not kernel code.
 *
 * Model notes:
 *  - mark_page_dirty_emu on a page under writeback redirties it: the page
 *    stays dirty after writeback completes and is written again later.
 *  - balance_dirty_pages_emu blocks the writing task (stalls) only above
 *    dirty_ratio, draining down to the background threshold; above the
 *    background threshold it merely kicks the (synchronous here) flusher.
 *  - inject_io_error_emu latches a writeback failure that a later fsync
 *    reports as -EIO, mirroring deferred write-error reporting.
 */
#ifndef PAGE_CACHE_WRITEBACK_STUBS_H
#define PAGE_CACHE_WRITEBACK_STUBS_H

#include <stddef.h>
#include <string.h>

#define EIO 5

#define PAGE_CACHE_PAGE_SIZE 64
#define PAGE_CACHE_NPAGES    16

enum {
	PG_CLEAN = 0,
	PG_DIRTY = 1,
	PG_WRITEBACK = 2,
};

struct page_emu {
	unsigned char data[PAGE_CACHE_PAGE_SIZE];
	int state;
	int redirtied;
};

struct file_emu {
	int fd;
	size_t size;		/* logical size (metadata) */
	size_t disk_size;	/* size that reached the durable store */
	unsigned long mtime;
	unsigned long disk_mtime;
};

static struct page_emu pages[PAGE_CACHE_NPAGES];
static unsigned char disk_store[PAGE_CACHE_NPAGES][PAGE_CACHE_PAGE_SIZE];

static unsigned long dirty_pages_emu;
static unsigned long writeback_passes_emu;
static unsigned long throttle_stalls_emu;
static int io_error_latched;
static int io_error_armed;

static const int dirty_background_ratio_emu = 40;
static const int dirty_ratio_emu = 60;

static inline void reset_writeback_emu(void)
{
	int i;
	for (i = 0; i < PAGE_CACHE_NPAGES; i++) {
		memset(pages[i].data, 0, PAGE_CACHE_PAGE_SIZE);
		pages[i].state = PG_CLEAN;
		pages[i].redirtied = 0;
		memset(disk_store[i], 0xFF, PAGE_CACHE_PAGE_SIZE);
	}
	dirty_pages_emu = 0;
	writeback_passes_emu = 0;
	throttle_stalls_emu = 0;
	io_error_latched = 0;
	io_error_armed = 0;
}

static inline void init_file_emu(struct file_emu *f, int fd)
{
	f->fd = fd;
	f->size = 0;
	f->disk_size = 0;
	f->mtime = 0;
	f->disk_mtime = 0;
}

static inline int dirty_percent_emu(void)
{
	return (int)(dirty_pages_emu * 100 / PAGE_CACHE_NPAGES);
}

/* mark a page dirty. On a page under writeback this redirties it: the page
 * stays dirty after writeback completes (the completion path checks the
 * redirtied flag). */
static inline void mark_page_dirty_emu(int idx)
{
	if (pages[idx].state == PG_CLEAN) {
		pages[idx].state = PG_DIRTY;
		dirty_pages_emu++;
	} else if (pages[idx].state == PG_WRITEBACK) {
		pages[idx].redirtied = 1;
		dirty_pages_emu++;
	}
}

/* writeback start: snapshot the page into the durable store. On an injected
 * error the data does NOT reach the durable store; the error is latched and
 * surfaced by the next fsync. */
static inline void write_page_to_disk_emu(int idx)
{
	int i;
	if (io_error_armed) {
		io_error_armed = 0;
		io_error_latched = 1;
		pages[idx].state = PG_WRITEBACK;
		return;
	}
	for (i = 0; i < PAGE_CACHE_PAGE_SIZE; i++)
		disk_store[idx][i] = pages[idx].data[i];
	pages[idx].state = PG_WRITEBACK;
}

/* begin phase: every dirty page is written back (snapshot taken). */
static inline unsigned long writeback_pass_emu(void)
{
	unsigned long n = 0;
	int i;
	for (i = 0; i < PAGE_CACHE_NPAGES; i++) {
		if (pages[i].state == PG_DIRTY) {
			write_page_to_disk_emu(i);
			dirty_pages_emu--;
			n++;
		}
	}
	return n;
}

/* completion phase: writeback finished. A redirtied page must stay dirty
 * and be written again on the next pass. */
static inline unsigned long writeback_complete_emu(void)
{
	unsigned long n = 0;
	int i;
	for (i = 0; i < PAGE_CACHE_NPAGES; i++) {
		if (pages[i].state == PG_WRITEBACK) {
			if (pages[i].redirtied) {
				pages[i].redirtied = 0;
				pages[i].state = PG_DIRTY;
			} else {
				pages[i].state = PG_CLEAN;
			}
			n++;
		}
	}
	return n;
}

/* one flusher pass: write back all currently dirty pages. */
static inline unsigned long writeback_run_emu(void)
{
	writeback_passes_emu++;
	writeback_pass_emu();
	writeback_complete_emu();
	return dirty_pages_emu;
}

/* called from the write path. Above dirty_ratio the writing task stalls
 * (throttled) until the dirty level drops to the background threshold;
 * above dirty_background_ratio a flusher pass is kicked instead. */
static inline unsigned long balance_dirty_pages_emu(void)
{
	unsigned long stalls = 0;
	while (dirty_percent_emu() > dirty_ratio_emu) {
		writeback_run_emu();
		stalls++;
		if (stalls > 64ul)
			break;
	}
	if (dirty_percent_emu() > dirty_background_ratio_emu)
		writeback_run_emu();
	throttle_stalls_emu += stalls;
	return stalls;
}

/* write(): data goes into the page cache and marks pages dirty; the durable
 * store is untouched. Returns bytes accepted. */
static inline long write_emu(struct file_emu *f, const unsigned char *buf,
			     size_t len)
{
	size_t i;
	for (i = 0; i < len; i++) {
		size_t pos = f->size + i;
		int pg = (int)(pos / PAGE_CACHE_PAGE_SIZE);
		if (pg >= PAGE_CACHE_NPAGES)
			break;
		pages[pg].data[pos % PAGE_CACHE_PAGE_SIZE] = buf[i];
		mark_page_dirty_emu(pg);
	}
	f->size += i;
	f->mtime++;
	balance_dirty_pages_emu();
	return (long)i;
}

/* metadata persistence. size is essential (data is unreachable without it);
 * mtime is non-essential and only fsync persists it. */
static inline void persist_metadata_emu(struct file_emu *f, int include_mtime)
{
	f->disk_size = f->size;
	if (include_mtime)
		f->disk_mtime = f->mtime;
}

/* fsync(): force all dirty pages of the file to the durable store and, with
 * sync_metadata, persist the metadata. Reports latched writeback errors. */
static inline int fsync_emu(struct file_emu *f, int sync_metadata)
{
	if (io_error_latched)
		return -EIO;
	writeback_run_emu();
	persist_metadata_emu(f, sync_metadata);
	if (io_error_latched)
		return -EIO;
	return 0;
}

/* fdatasync(): data + essential metadata only; mtime is not persisted. */
static inline int fdatasync_emu(struct file_emu *f)
{
	return fsync_emu(f, 0);
}

/* O_SYNC write: data is flushed per write, before the call returns. */
static inline long write_sync_emu(struct file_emu *f,
				  const unsigned char *buf, size_t len)
{
	long n = write_emu(f, buf, len);
	if (n > 0 && fsync_emu(f, 0) != 0)
		return -EIO;
	return n;
}

/* durable store query: does the durable store hold buf at file offset off? */
static inline int durable_matches_emu(const struct file_emu *f, size_t off,
				      const void *buf, size_t len)
{
	const unsigned char *b = (const unsigned char *)buf;
	size_t i;
	(void)f;
	for (i = 0; i < len; i++) {
		size_t pos = off + i;
		int pg = (int)(pos / PAGE_CACHE_PAGE_SIZE);
		if (disk_store[pg][pos % PAGE_CACHE_PAGE_SIZE] != b[i])
			return 0;
	}
	return 1;
}

static inline void inject_io_error_emu(void)
{
	io_error_armed = 1;
}

static inline unsigned long dirty_pages_get_emu(void)
{
	return dirty_pages_emu;
}

static inline unsigned long writeback_passes_get_emu(void)
{
	return writeback_passes_emu;
}

static inline unsigned long throttle_stalls_get_emu(void)
{
	return throttle_stalls_emu;
}

#endif
