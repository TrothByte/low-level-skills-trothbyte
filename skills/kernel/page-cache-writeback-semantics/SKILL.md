---
name: page-cache-writeback-semantics
description: Use when writing or reviewing kernel filesystem code that depends on page cache and writeback semantics — marking pages dirty, fsync/fdatasync data and metadata integrity, O_SYNC/O_DSYNC, writeback thresholds, balance_dirty_pages throttling, and the difference between dirty pages and written-to-disk data. Teaches what flush actually guarantees.
---

# Page Cache & Writeback Semantics

Rules for Linux filesystem code whose correctness depends on the page cache
and the writeback pipeline: when data is really on disk, what fsync and
fdatasync guarantee, how writeback gets triggered, and how dirty-page
accounting throttles writers. Load `references/README.md` before touching
fsync, O_SYNC, or page-dirtying paths.

## When to use

- Filesystem code that marks pages dirty (`set_page_dirty`,
  `mark_buffer_dirty`, the write() page-cache path, `generic_write_end`).
- Implementing or reviewing fsync/fdatasync/sync_file_range handling and
  O_SYNC / O_DSYNC flag paths.
- Code where a crash must not lose acknowledged writes (journalling and
  log-structured filesystems).
- mmap-shared durability reasoning: flusher writeback vs msync.
- Reviewing patches for data-loss-on-power-failure bugs and dirty-page
  accounting mistakes.

## When not to use

- User-space-only code; the POSIX fsync(2) contract lives elsewhere.
- O_DIRECT / raw block I/O that bypasses the page cache entirely.
- Driver-private buffers with no page cache backing (DMA rings, device
  memory) — that is `dma-cache-coherency` territory.
- Filesystems that manage their own storage without the generic writeback
  layer (e.g. network filesystem daemons).

## What the agent often gets wrong

- "write() succeeded, so the data is on disk." It sits in the page cache,
  marked dirty, and can be lost on power failure.
- Reading "dirty page" as "written out". Dirty means modified and not yet
  written back — the opposite of durable.
- fsync == fdatasync. fsync also forces metadata; fdatasync skips
  non-essential metadata.
- Believing O_SYNC orders metadata the way fsync does, or that O_DSYNC also
  syncs metadata.
- Writing back in the caller's syscall context. Background writeback runs in
  the flusher thread; the caller is only throttled in balance_dirty_pages.
- Assuming a page whose writeback completes is clean forever — a concurrent
  writer can redirty it and it needs another pass.
- Expecting write() to report I/O errors. A failed writeback can surface as
  -EIO on a later fsync even when every write() returned success.

## How to reason correctly

1. Trace the dirty lifecycle: read or allocate a page, modify it, mark it
   dirty, and let the flusher write it back. Until writeback, the data is
   not on disk.
2. Separate the page cache (volatile copy) from the durable store. Only
   writeback copies cache contents into storage; assert on the durable
   model, never on the cache.
3. fsync() forces writeback of the file's dirty pages AND persists the
   metadata needed to retrieve the data; after it returns, the data survives
   a crash. fdatasync() skips non-essential metadata (mtime/atime) but must
   still persist size and block map — data must remain reachable.
4. O_SYNC makes each write wait until data and required metadata are
   flushed; O_DSYNC waits only for data (plus size). For data-only streams,
   prefer O_DSYNC plus a periodic fsync over O_SYNC on every write.
5. Writeback is threshold-driven: above dirty_background_ratio a background
   flusher starts; above dirty_ratio the writing task is throttled in
   balance_dirty_pages until the dirty level drops.
6. Handle the redirty race: a page redirtied while under writeback must stay
   dirty after writeback completes and be written again.
7. Check fsync/fdatasync return values — they report the I/O errors the
   page-cache write path absorbed silently.

## What to verify

- Every write that must be durable is followed by fsync/fdatasync, or uses
  O_SYNC/O_DSYNC, and the return value is checked.
- Data-only files use fdatasync, not fsync, when metadata is irrelevant.
- No assumption that a page clean after writeback stays clean: redirty
  handling present (completion path re-checks the dirty/redirtied state).
- Dirty-page accounting matches the model: dirty count equals modified
  pages not yet written back.
- balance_dirty_pages exists so a large write throttles instead of pinning
  unbounded dirty memory.
- fsync surfaces latched I/O errors instead of returning 0.

## How to verify

Host-compilable logic checks (self-contained stubs, no kernel headers):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_writeback.c -o /tmp/good_writeback
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_writeback.c -o /tmp/bad_writeback
```

Target (kernel) checks — document these, do not claim to have run them:

```
make defconfig && make -j$(nproc)     # build with CONFIG_MM + CONFIG_BLOCK
# QEMU boot, then xfstests: generic/484 (fsync), generic/342 (fdatasync),
# generic/099 + power-fail harness on ext4 data=ordered (crash consistency)
```

## Where the knowledge comes from

- `linux-writeback-docs` — dirty ratios, balance_dirty_pages,
  wb_writeback loop, sync/fsync/fdatasync semantics
- `linux-mm-docs` — page cache and page lifecycle in the MM documentation
- `kernel-docs-mm` — page cache semantics (docs.kernel.org/mm)
- `kernel-source` — mm/page-writeback.c, fs/fs-writeback.c, fs/sync.c, and
  the mm/filemap.c write/dirty paths
- `linux-memory-barriers` — ordering data stores before setting PageDirty so
  writeback never reads a half-written page
- `ldd3` — block and page I/O background for device drivers
- `nvd-cve` — CVE-2022-0847 and CVE-2016-5195 page-cache classes
- `cwe` — CWE-362 race class behind Dirty COW

## Related skills

- `kernel-driver-char-device-lifecycle` — device lifetime around cached
  pages
- `vfs-file-operations-and-fops` — file_operations write/fsync plumbing
- `kernel-atomic-context` — what is legal where the write path runs
- `data-race-kernel-detection` — redirty/clean races on page flags
- `dma-cache-coherency` — device-side writes to page-cache-backed buffers
- `kernel-scheduler-mm-vfs-internals` — flusher threads and mm accounting

## Evaluation

Historical CVEs: CVE-2022-0847 (Dirty Pipe — pipe_buffer page cache flags
not cleared, so a page spliced into a pipe aliases the page cache and an
unprivileged process can overwrite read-only file data; flags/ownership of
the cached page must be restored on the alias path). CVE-2016-5195 (Dirty
COW — writeable private mapping race in the page fault / COW of a page
cache page: the mapping is writable while the page is being copied, and a
racing write lands in read-only memory; documented class is the
page-fault/COW race, CWE-362). Synthetic: skip fsync after write then
simulate power loss; assume dirty == on disk; call fdatasync where metadata
must survive; drop the balance_dirty_pages check. Adversarial: code that
"passes" a host test where writeback runs synchronously but a real kernel
writeback is asynchronous; code that is correct only because the file is too
small to cross the background threshold. False-positive: correct code that
calls and checks fsync/fdatasync; ephemeral temp files that deliberately
skip fsync; a page redirtied during writeback that is correctly re-queued —
these must NOT be flagged.
