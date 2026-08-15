# VM/Allocator Leak Diagnosis — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Classify the leak layer: heap vs VM vs commit

- **RULE**: a memory leak lives in one of three layers — heap (lost malloc
  blocks), VM (mapped regions never unmapped), or commit (private committed
  pages). The symptom tells you which: RSS/commit grows while the heap is
  flat → VM leak; heap grows → heap leak. Heap checkers (ASan/leak-check)
  only see the heap layer.
- **WHY AI GETS IT WRONG**: runs a heap checker, sees it clean, and concludes
  "no leak" even though RSS is climbing; or blames the workload because the
  heap shows nothing.
- **CORRECT REASONING**: measure the layers independently — heap size (ASan /
  massif), VM region count and total (`/proc/<pid>/maps`), RSS
  (`/proc/<pid>/status` Rss). The layer that grows while the heap stays flat
  is where the leak lives.
- **EXAMPLE** (bad): "ASan is clean, so the 40 GB RSS growth is the workload's
  fault." The heap is clean because the leak is in the VM layer.
- **COUNTEREXAMPLE** (good): "Heap is flat at 4 MB; /proc/PID/maps totals 40 GB
  with 12,000 regions — VM leak, regions never unmapped."
- **VERIFICATION**: on this host `bad/page_pool.c` grows commit to 73 MiB with
  zero heap involvement (no malloc); `good/release_to_os.c` returns to 0 MiB.
- **SOURCE**: linux-mm-docs; ghostty-memory-leak.

## 2. Pools reuse without release

- **RULE**: allocators keep freed regions for reuse. The bug is a reuse path
  that never returns memory to the OS: the unmap (`munmap`/`VirtualFree`)
  branch is unreachable or skipped for some class of regions. Ghostty's
  PageList.zig reset the size metadata of a pooled page but never called
  `munmap` on the previously direct-mapped regions — the pool grew forever.
- **WHY AI GETS IT WRONG**: looks for a missing `free()`; a page pool has no
  lost pointers, so the "leak" is invisible to the usual search.
- **CORRECT REASONING**: enumerate every free path of the pool. For each,
  check whether the region is returned to the OS or just re-pooled
  (re-pooling is fine and bounded; growing the pool without bound, or
  re-pooling without dropping the old mapping, is the leak).
- **EXAMPLE** (bad): the pool holds every block ever mapped; `pool_get`
  reuses a slot but the replaced mapping is never released
  (`bad/page_pool.c`).
- **COUNTEREXAMPLE** (good): the pool returns blocks to the OS when idle;
  commit returns to baseline (`good/release_to_os.c`).
- **VERIFICATION**: `./pp.exe` prints `after_pool=32 MiB after_extra=73 MiB`;
  `./rto.exe` prints `during=40 MiB after=0 MiB`.
- **SOURCE**: ghostty-memory-leak; linux-mm-docs.

## 3. Measure VM footprint, not allocation count

- **RULE**: allocation count and heap size are the wrong metrics for a VM
  leak. Track total mapped size, region count, and RSS over the scenario.
- **WHY AI GETS IT WRONG**: reports "10,000 allocations, all freed" as
  evidence of no leak; freed-but-still-mapped regions do not show up.
- **CORRECT REASONING**: a region freed in the allocator but never unmapped
  still counts in `/proc/<pid>/maps` and still costs RSS when touched.
- **EXAMPLE** (bad): "The allocator's free() ran for every block, so there is
  no leak."
- **COUNTEREXAMPLE** (good): "12,000 regions in /proc/PID/maps; only 40 are
  in use by the pool — the rest are freed-but-never-unmapped."
- **VERIFICATION**: on Linux `grep -c '^' /proc/<pid>/maps` grows without
  bound; on this host PagefileUsage grows 0→32→73 MiB.
- **SOURCE**: linux-mm-docs.

## 4. Trigger is not the cause

- **RULE**: a leak can stay latent for years and surface only when a specific
  workload makes the defective path hot. The workload is the TRIGGER; the
  missing unmap is the CAUSE. Fixing the trigger (or blaming it) does not fix
  the leak.
- **WHY AI GETS IT WRONG**: attributes the leak to the thing that surfaced it
  — the popular (false) narrative "Claude Code broke Ghostty".
- **CORRECT REASONING**: separate the two claims: (a) what made the leak
  observable (constant repaint under heavy terminal use), (b) why memory
  grows (pool reuse without `munmap`). Only (b) is a defect; the primary
  analysis states explicitly no AI was used in the fix work.
- **EXAMPLE** (bad): "Claude Code caused the Ghostty leak."
- **COUNTEREXAMPLE** (good): "The repaint-heavy workload triggered it; the
  cause is PageList.zig's reuse-without-munmap path."
- **VERIFICATION**: the recorded Windows demonstration grows commit with no
  trigger at all — the mechanism alone explains the growth.
- **SOURCE**: ghostty-memory-leak.

## 5. Fix the unmap path, not every free

- **RULE**: the fix makes the unmap branch unconditional for the right class
  of regions (e.g., non-pooled/direct mappings), or bounds the pool. A blanket
  "munmap on every free" destroys pooling and can regress performance by
  orders of magnitude.
- **WHY AI GETS IT WRONG**: proposes the bluntest patch (free/unmap
  everything) without measuring; or proposes re-running a "garbage
  collection" that still never unmaps.
- **CORRECT REASONING**: the defect is a specific path — the one that re-pools
  a region whose unmap was skipped. Fix that path; then re-run the growth
  scenario and confirm RSS/commit plateaus or returns to baseline.
- **EXAMPLE** (bad): adding `VirtualFree(p, 0, MEM_RELEASE)` inside a hot
  reuse loop, destroying the pool and slowing the app.
- **COUNTEREXAMPLE** (good): the good fixture releases blocks only when the
  pool no longer needs them, and commit returns to baseline.
- **VERIFICATION**: after the fix, re-run the scenario and compare
  `/proc/<pid>/status` RSS and maps count before/after.
- **SOURCE**: ghostty-memory-leak; linux-mm-docs.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Leak layers | heap (lost pointers) vs VM (unmapped regions) vs commit — heap clean + RSS up = VM |
| Pool leak | a reuse path that never runs the unmap; re-pooling without bound |
| Metrics | /proc/PID/maps total + count, RSS delta — not allocation count |
| Trigger vs cause | workload surfaces it; the missing munmap is the cause |
| Fix | unconditional unmap for the right class of regions; then re-measure |
| Ghostty | PageList.zig reuse-without-munmap; 37-130 GB; trigger ≠ cause |
