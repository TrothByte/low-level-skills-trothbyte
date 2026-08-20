# Linux Page Cache & Writeback Rules

Source-backed rule set for filesystem code that depends on page cache and
writeback durability. Each entry: RULE -> WHY AI GETS IT WRONG -> CORRECT
REASONING -> EXAMPLE -> COUNTEREXAMPLE -> VERIFICATION -> SOURCE. Confidence
markers: KNOWN (documented kernel contract), INFERRED (derived), UNVERIFIED
(never use in a stable skill). Code snippets use the self-contained emulation
from `examples/stubs.h`.

## 1. A successful write() only fills the page cache

- **RULE**: a successful write() places data into page cache pages and marks
  them dirty. It does not reach the durable store. Durability requires
  writeback plus fsync/fdatasync (or O_SYNC on the write).
- **WHY AI GETS IT WRONG**: on the host, a write() to a real file followed by
  a read() reads from the same page cache, so the data "persists"; a power
  failure would not.
- **CORRECT REASONING**: the kernel write path copies into the cache and
  leaves the pages for the flusher. Nothing is guaranteed durable until a
  sync call returns (KNOWN kernel contract). The durable store only changes
  when writeback runs.
- **EXAMPLE** (bad):
  ```c
  write_emu(&f, buf, n);   /* no fsync: data only in page cache */
  /* power loss here loses the write */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  write_emu(&f, buf, n);
  assert(fsync_emu(&f, 1) == 0);   /* now durable */
  ```
- **VERIFICATION**: after a sub-threshold write_emu, `durable_matches_emu()`
  is false; after fsync_emu it is true (good_writeback test 1).
- **SOURCE**: linux-writeback-docs; kernel-source (mm/filemap.c write path);
  ldd3 (block and page I/O).

## 2. The dirty-page lifecycle

- **RULE**: a page moves clean -> dirty (modified in cache) -> writeback
  (flusher started) -> clean. "Dirty" marks a page whose cache contents are
  newer than the disk copy; it never means "already on disk".
- **WHY AI GETS IT WRONG**: reads "dirty" as "written" and treats the dirty
  counter as evidence of durability.
- **CORRECT REASONING**: dirty is a deferred-write marker. Writeback is what
  copies dirty page contents into the durable store; only then does the page
  become clean (KNOWN: mm/page-writeback.c lifecycle).
- **EXAMPLE** (bad):
  ```c
  if (dirty_pages_get_emu() == 0)   /* confused by counter */
      report_durable();
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  assert(dirty_pages_get_emu() == 0);
  assert(durable_matches_emu(&f, 0, buf, len));  /* check the model */
  ```
- **VERIFICATION**: dirty_pages_get_emu() > 0 while durable_matches_emu()
  is false; both invert after fsync_emu.
- **SOURCE**: linux-writeback-docs; linux-mm-docs (page lifecycle);
  kernel-source (mm/page-writeback.c).

## 3. fsync() semantics and durability

- **RULE**: fsync() flushes the file's dirty pages AND forces the metadata
  needed to retrieve them (size, block map). After fsync returns, the data
  is durable: it survives crash and power loss.
- **WHY AI GETS IT WRONG**: treats fsync as "another flush" and drops it, or
  assumes a successful write is enough.
- **CORRECT REASONING**: fsync is the point of no return for acknowledged
  writes (KNOWN POSIX/kernel contract). Data-only and metadata are both
  forced; on data=ordered filesystems data is on disk before the metadata
  commit completes.
- **EXAMPLE** (bad):
  ```c
  write_emu(&f, buf, n);
  /* no fsync; the flusher may not have run yet */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  write_emu(&f, buf, n);
  assert(fsync_emu(&f, 1) == 0);   /* durable after this line */
  ```
- **VERIFICATION**: durable_matches_emu() true and dirty_pages_get_emu() == 0
  after fsync_emu (good_writeback test 1).
- **SOURCE**: linux-writeback-docs (sync/fsync semantics); kernel-source
  (fs/sync.c, fs/fs-writeback.c); nvd-cve (data-loss bug classes).

## 4. fdatasync() skips non-essential metadata

- **RULE**: fdatasync() skips non-essential metadata such as mtime/atime but
  STILL persists the metadata required to retrieve the data (size, block
  map). Data must remain reachable after fdatasync.
- **WHY AI GETS IT WRONG**: believes fdatasync skips ALL metadata, or that it
  is just fsync spelled differently.
- **CORRECT REASONING**: size is essential — without it the written bytes are
  unreachable; mtime is not. Data-only writers (logs, journals) use
  fdatasync and get nearly all of the guarantee for less write cost.
- **EXAMPLE** (bad):
  ```c
  fdatasync_emu(&f);
  assert(f.disk_mtime == f.mtime);   /* wrong: mtime not persisted */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  assert(fdatasync_emu(&f) == 0);
  assert(f.disk_size == f.size);     /* essential metadata persisted */
  /* do NOT assert on mtime */
  ```
- **VERIFICATION**: after fdatasync_emu, disk_size == size but disk_mtime
  still differs from mtime; fsync_emu then equalizes them (good_writeback
  test 2).
- **SOURCE**: linux-writeback-docs; kernel-source (fs/fs-writeback.c).

## 5. O_SYNC and O_DSYNC are per-write fsync/fdatasync

- **RULE**: O_SYNC makes every write wait until data and the metadata needed
  to retrieve it are flushed. O_DSYNC waits only for data (and size), not for
  non-essential metadata. They are per-write syncs, not background flushes.
- **WHY AI GETS IT WRONG**: swaps the two, or thinks O_SYNC merely orders
  metadata after data.
- **CORRECT REASONING**: O_SYNC on each write is the strongest guarantee but
  the most expensive. For data-only streams O_DSYNC plus a periodic fsync is
  the kernel-recommended pattern.
- **EXAMPLE** (bad):
  ```c
  int fd = open(path, O_SYNC);       /* mtime still not synced per write */
  ```
  (with the agent believing every metadata field is ordered)
- **COUNTEREXAMPLE** (good):
  ```c
  assert(write_sync_emu(&f, buf, n) == n);   /* O_SYNC: durable on return */
  assert(durable_matches_emu(&f, 0, buf, n));
  ```
- **VERIFICATION**: write_sync_emu leaves dirty_pages_get_emu() == 0 with no
  explicit fsync (good_writeback test 7).
- **SOURCE**: linux-writeback-docs; kernel-source (fs/fs-writeback.c, VFS
  O_SYNC handling).

## 6. Writeback thresholds and balance_dirty_pages

- **RULE**: when dirty memory exceeds dirty_background_ratio a background
  flusher starts; when it exceeds dirty_ratio the writing task is throttled
  in balance_dirty_pages until the dirty level drops (to roughly the
  background threshold).
- **WHY AI GETS IT WRONG**: thinks writeback happens only on explicit sync,
  so a large unbounded write is "fine".
- **CORRECT REASONING**: throttling happens in the writing task's context
  inside balance_dirty_pages; the flusher work item does the actual writeback
  (KNOWN: mm/page-writeback.c).
- **EXAMPLE** (bad):
  ```c
  /* one huge write with no dirty-ratio accounting in the model */
  write_emu(&f, huge, HUGE_LEN);     /* model shows no throttle */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  assert(throttle_stalls_get_emu() > 0);   /* task was throttled */
  assert(dirty_pages_get_emu() == 0);      /* drained to background */
  ```
- **VERIFICATION**: a 12-page write (75%) records throttle stalls and drains
  to 0 dirty; a 7-page write (43%) only kicks the flusher (good_writeback
  tests 4 and 5).
- **SOURCE**: linux-writeback-docs (dirty ratios, balance_dirty_pages);
  kernel-source (mm/page-writeback.c).

## 7. Background writeback runs in the flusher, not the caller

- **RULE**: background writeback is executed by flusher threads / writeback
  work items (wb_workfn), asynchronously with respect to the caller's write()
  syscall. The caller only triggers balance_dirty_pages throttling or an
  explicit fsync/sync.
- **WHY AI GETS IT WRONG**: writes a "start writeback" path that runs
  synchronously in the caller and declares it complete.
- **CORRECT REASONING**: a writeback work item may complete after the syscall
  returned; durability must not be assumed from the write() return.
- **EXAMPLE** (bad):
  ```c
  write_emu(&f, buf, n);
  /* no fsync, assumes the caller's path ran writeback */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  write_emu(&f, buf, n);
  assert(writeback_passes_get_emu() == 0);   /* caller ran no writeback */
  assert(fsync_emu(&f, 1) == 0);             /* only fsync forces it */
  ```
- **VERIFICATION**: a sub-background write_emu leaves writeback_passes_emu()
  at 0 until an explicit flusher pass or fsync runs.
- **SOURCE**: linux-writeback-docs (flusher threads, wb_writeback);
  kernel-source (fs/fs-writeback.c).

## 8. The redirty race

- **RULE**: if a page is redirtied while under writeback, writeback
  completion must leave it dirty. The written snapshot is stale and the page
  must be written again. This is a race: the page can change between the
  writeback start and completion.
- **WHY AI GETS IT WRONG**: assumes writeback completion always cleans the
  page and forgets the concurrent writer.
- **CORRECT REASONING**: completion checks whether the page was redirtied
  (TestSetPageWriteback / redirtied flag). A page whose writeback completed
  is only clean if nobody redirtied it in the window.
- **EXAMPLE** (bad):
  ```c
  pages[i].state = PG_CLEAN;   /* unconditional on writeback completion */
  dirty_pages_emu--;
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (pages[i].redirtied) {
      pages[i].state = PG_DIRTY;   /* still dirty, write again */
  } else {
      pages[i].state = PG_CLEAN;
      dirty_pages_emu--;
  }
  ```
- **VERIFICATION**: pass -> redirty -> complete leaves the page PG_DIRTY and
  the durable store holding the old snapshot; the next pass writes the new
  data (good_writeback test 3).
- **SOURCE**: kernel-source (mm/page-writeback.c completion); linux-mm-docs;
  cwe (CWE-362 race).

## 9. Writeback errors surface at fsync, not at write()

- **RULE**: a failed writeback can be invisible to write(); fsync/fdatasync
  report the deferred error as -EIO. The sync call return must be checked.
- **WHY AI GETS IT WRONG**: checks only the write() return value and reports
  success.
- **CORRECT REASONING**: errors are latched by the writeback completion and
  surfaced on the next sync that forces them out (KNOWN kernel behavior).
- **EXAMPLE** (bad):
  ```c
  write_emu(&f, buf, n);
  if (n == (long)len)                /* write() returned success */
      report_durable();              /* wrong: writeback may have failed */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (fsync_emu(&f, 1) == -EIO)      /* deferred error reported here */
      handle_error();
  ```
- **VERIFICATION**: inject_io_error_emu() then fsync_emu returns -EIO and
  durable_matches_emu() is false (good_writeback test 6).
- **SOURCE**: kernel-source; linux-writeback-docs; cwe (CWE-252 unchecked
  return).

## 10. Metadata vs data journaling order

- **RULE**: on data=ordered filesystems, fsync writes data to disk before the
  metadata commit, so metadata durability implies data durability. On
  data=writeback the ordering is not guaranteed. Kernel code must not assume
  a journaling mode without checking the mount options.
- **WHY AI GETS IT WRONG**: assumes every journaled filesystem commits data
  atomically with metadata.
- **CORRECT REASONING**: durability guarantees differ per fs and per mount
  option; the portable contract is "after fsync returns, data is durable".
- **EXAMPLE** (bad):
  ```c
  if (metadata_committed)
      assume_data_durable();   /* false on data=writeback */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (fsync_emu(&f, 1) != 0)
      return -EIO;
  assert(durable_matches_emu(&f, 0, buf, n));   /* check data itself */
  ```
- **VERIFICATION**: the durable-store model checks data independently of the
  metadata flag.
- **SOURCE**: kernel-source (fs/ext4 mount options); linux-writeback-docs;
  ldd3.

## 11. Shared mmap pages are written back by the flusher

- **RULE**: a shared mmap modifies page-cache pages directly; they become
  dirty and are written back by the flusher (or by msync/fsync). The
  faulting writer never performs writeback itself.
- **WHY AI GETS IT WRONG**: thinks mmap stores are synchronous stores to
  storage.
- **CORRECT REASONING**: mmap write == dirtying a page cache page: same
  lifecycle as write(), same durability requirements.
- **EXAMPLE** (bad):
  ```c
  memcpy(pages[0].data, new_data, PAGE_CACHE_PAGE_SIZE);
  /* assume it is on disk */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  memcpy(pages[0].data, new_data, PAGE_CACHE_PAGE_SIZE);
  mark_page_dirty_emu(0);
  writeback_run_emu();                 /* flusher/msync does the work */
  assert(durable_matches_emu(&f, 0, new_data, PAGE_CACHE_PAGE_SIZE));
  ```
- **VERIFICATION**: directly modified page is not durable until a writeback
  pass runs (same durable-store model).
- **SOURCE**: linux-mm-docs (mmap semantics); kernel-docs-mm; linux-writeback
  -docs.

## 12. Page-cache flag aliasing (CVE-2022-0847 class)

- **RULE**: when a page cache page is borrowed by another structure (e.g. a
  pipe_buffer via splice), its page-cache flags and ownership must be
  cleared/restored. If the flags leak, the borrowed page aliases the page
  cache and a different subsystem can write into cached file data.
- **WHY AI GETS IT WRONG**: assumes page flags are per-structure, not a
  per-page cache property.
- **CORRECT REASONING**: CVE-2022-0847 (Dirty Pipe): pipe_buffer page cache
  flags not cleared, so splicing a read-only file page into a pipe made it
  writable, overwriting read-only file offsets (KNOWN, NVD). The alias path
  must clear and restore the flags.
- **EXAMPLE** (bad): a splice path that fills a pipe_buffer page without
  clearing/restoring the page's cache flags.
- **COUNTEREXAMPLE** (good): the alias path clears the cache flags on borrow
  and restores them on release, so the pipe page can never be treated as a
  writable file page.
- **VERIFICATION**: KASAN + reproducer writing to a read-only file after
  splice; review the splice/pipe flag handling.
- **SOURCE**: nvd-cve (CVE-2022-0847); kernel-source; cwe.

## Quick detection table

| Pattern | Class | Check |
|---|---|---|
| write() assumed durable | durability | fsync/fdatasync or O_SYNC after the write |
| dirty == on disk | lifecycle | durable-store model, not the dirty counter |
| fdatasync where metadata must survive | data loss | use fsync, verify size/mtime |
| O_DSYNC assumed to sync metadata | ordering | O_DSYNC vs O_SYNC contract |
| writeback runs in the caller | context | wb_workfn flusher, not the syscall |
| completion always cleans the page | race | redirtied check (CWE-362) |
| write() errors assumed local | deferred errors | check fsync return (-EIO) |
| journaling mode assumed | ordering | verify mount options (data=ordered) |
| mmap write assumed synchronous | mmap durability | flusher + msync/fsync |
| pipe splice leaves cache flags | CVE-2022-0847 | clear/restore flags on borrow |
| writeable-mapping COW race | CVE-2016-5195 | page-fault/COW serialization |
