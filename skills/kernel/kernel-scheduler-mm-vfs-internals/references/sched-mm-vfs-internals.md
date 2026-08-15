# Kernel Scheduler / MM / VFS Internals — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to
registry/sources.yaml. Status tags: KNOWN = verifiable in the cited kernel
source/docs; INFERRED = from secondary sources, confirm on target kernel.

## 1. The default scheduler is EEVDF since 6.6, not CFS

- **RULE**: Linux 6.6 merged EEVDF ("Earliest Eligible Virtual Deadline
  First") as the fair scheduler, replacing CFS's plain-vruntime picking.
  `Documentation/scheduler/sched-eevdf.rst` describes placement by a virtual
  deadline computed from `vruntime` and a per-task lag against the global
  average (`avg_vruntime`). CFS-era prose ("pick the task with the smallest
  vruntime") is wrong for 6.6+.
- **WHY AI GETS IT WRONG**: training data is dominated by pre-6.6 CFS
  material; the model answers "how does the fair scheduler work" with the
  old algorithm and even cites `/proc/sched_debug` fields that no longer
  exist or have new semantics.
- **CORRECT REASONING**: anchor on kernel version first. On 6.6+, a task's
  position is decided by its virtual deadline; lag matters at enqueue time
  (tasks running longer than their entitlement accumulate negative lag and
  are placed behind). `se->vruntime` still exists in `struct sched_entity`
  but is meaningful relative to `avg_vruntime`, not as an absolute rank.
- **EXAMPLE** (bad):
  ```sh
  grep "cfs_avg_vruntime" /proc/sched_debug   # no such field, ever
  ```
- **COUNTEREXAMPLE** (good):
  ```sh
  uname -r
  grep -E "vruntime|lag|deadline|avg_vruntime" /proc/sched_debug | head -40
  ```
- **VERIFICATION**: on Linux, `grep -E "lag|deadline" /proc/sched_debug`;
  `trace-cmd record -e sched_switch`; or read
  `kernel/sched/fair.c` `pick_eevdf()` and
  `Documentation/scheduler/sched-eevdf.rst`. Researched — no Linux host here;
  commands not run (UNVERIFIED on a live kernel).
- **SOURCE**: kernel-docs-sched (sched-eevdf.rst, KNOWN); kernel-source
  (kernel/sched/fair.c, pick_eevdf).

## 2. vruntime/lag semantics: lag is vruntime relative to the global average

- **RULE**: EEVDF defines a task's lag as `vruntime - avg_vruntime`. A task
  with negative lag (behind) is placed with a deadline sooner than its
  slice; a task with positive lag (ahead) is delayed. Weights/nice values
  scale the effective lag. This is measurable, not a black box.
- **WHY AI GETS IT WRONG**: models treat "vruntime" as an absolute timer and
  assume lag disappears at sleep/wakeup; in fact wakeup re-places the task
  using its accumulated lag (subject to the `sched_wakeup_granularity` and
  sleep-related heuristics).
- **CORRECT REASONING**: describe scheduling in terms of the three numbers
  the kernel exposes per task: `se->vruntime`, the global `avg_vruntime`,
  and the resulting lag. Any explanation that ignores the average is CFS-era.
- **EXAMPLE** (bad): "EEVDF always picks the task with the lowest vruntime
  after a wakeup."
- **COUNTEREXAMPLE** (good): "EEVDF converts lag to a virtual deadline; a
  newly woken task with negative lag gets an early deadline, but the
  deadline still competes with the current queue."
- **VERIFICATION**: `perf sched record`/`trace-cmd` sched_wakeup events; the
  exact lag formula lives in `kernel/sched/fair.c` (`lag()` helper). KNOWN
  concept, exact heuristics vary by kernel (6.6..6.12) — INFERRED for
  specific constants.
- **SOURCE**: kernel-docs-sched; kernel-source (kernel/sched/fair.c).

## 3. Buddy allocator: free_area orders and migratetypes; /proc/buddyinfo

- **RULE**: physical pages are grouped in `free_area[]` by order (0..MAX_ORDER,
  default 10). Blocks are split on allocation and coalesced on free;
  migratetypes (MIGRATE_MOVABLE/UNMOVABLE/RECLAIMABLE/ISOLATE) keep
  unmovable allocations from fragmenting movable pageblocks.
  `/proc/buddyinfo` prints, per zone, the count of free blocks per order.
- **WHY AI GETS IT WRONG**: models claim "buddy merges pages during
  allocation" (it splits), or read buddyinfo columns as "pages per order"
  when they are free *block* counts, or forget the migratetype dimension.
- **CORRECT REASONING**: allocation walks from the requested order up;
  splitting happens downward, merging (coalescing) only in `__free_one_page`.
  Long-lived movable-vs-unmovable pressure explains why `/proc/buddyinfo`
  shows uneven counts and why `compact` (or `/proc/sys/vm/compact_memory`)
  exists.
- **EXAMPLE** (bad): "allocating order-4 merges four order-2 buddies to
  build a bigger block."
- **COUNTEREXAMPLE** (good): "allocating order-4 splits an order-5 block; the
  leftover halves stay free and may re-coalesce later."
- **VERIFICATION**: `cat /proc/buddyinfo`; `echo 1 > /proc/sys/vm/compact_memory`.
  Researched — not run on this host.
- **SOURCE**: kernel-docs-mm; kernel-source (mm/page_alloc.c).

## 4. SLUB and /proc/slabinfo columns

- **RULE**: SLUB is the default slab allocator (since 2.6.23). Each
  `kmem_cache` manages slab pages with per-CPU partial lists.
  `/proc/slabinfo` columns: cache name, active_objs, num_objs, objsize,
  objperslab, pagesperslab, then flags. The first column is the name, not a
  counter.
- **WHY AI GETS IT WRONG**: models read column 1 as "active objects" (it is
  the cache name), invent a "max_objs" column, or claim SLAB is still an
  option on mainline (removed in 6.9 — only SLUB remains).
- **CORRECT REASONING**: know the column order by reading the header line
  `/proc/slabinfo` prints; validate any claim about a cache (e.g. `dentry`,
  `inode_cache`) by grepping slabinfo with the cache name as the first field.
- **EXAMPLE** (bad): `awk '{print $1}' /proc/slabinfo` reported as "active
  objects".
- **COUNTEREXAMPLE** (good): `head -1 /proc/slabinfo` first, then
  `grep -E "^(dentry|inode_cache) " /proc/slabinfo`.
- **VERIFICATION**: `head -1 /proc/slabinfo`, `grep dentry /proc/slabinfo`.
  Researched — not run on this host.
- **SOURCE**: kernel-docs-mm; kernel-source (mm/slub.c).

## 5. kmalloc vs vmalloc: contiguity, cost, and context limits

- **RULE**: `kmalloc` returns physically contiguous memory from slab caches
  (fast, sized caches, usable in atomic/IRQ with GFP_ATOMIC); `vmalloc`
  returns virtually contiguous, physically scattered memory backed by page
  tables (slower, can sleep, not for hot paths or very small objects, and
  needs PAGE_SIZE alignment). `kvmalloc` chooses between them by size.
- **WHY AI GETS IT WRONG**: models say "vmalloc is fine for the fast path"
  or "kmalloc handles 10 MB without effort" (order>0 allocations fail under
  fragmentation; vmalloc is the escape hatch). They also forget vmalloc's
  TLB/table cost and its sleeping requirement.
- **CORRECT REASONING**: size and context decide: small + atomic/IRQ →
  kmalloc; large/sparse/unmapped → vmalloc; "I don't care, medium" →
  kvmalloc. `/proc/vmallocinfo` shows what the kernel itself virtual-maps.
- **EXAMPLE** (bad): "use vmalloc in the timer IRQ because the buffer is 2 MiB."
- **COUNTEREXAMPLE** (good): "a 2 MiB buffer needed in IRQ context must be
  preallocated/physically contiguous (kmalloc or a preallocated pool);
  vmalloc sleeps during page-table setup."
- **VERIFICATION**: `grep vmalloc /proc/vmallocinfo`; `grep -E "^vmalloc"
  /proc/vmallocinfo`. Researched — not run on this host.
- **SOURCE**: kernel-docs-mm; kernel-source (mm/vmalloc.c, mm/slub.c).

## 6. VFS: dentry vs inode vs file; dcache is not in /proc/meminfo

- **RULE**: a `struct file` is one open instance (holds f_pos, f_op, refs);
  a `struct dentry` names one path component and links parent/child and the
  inode it points to; a `struct inode` owns the metadata (mode, size, ops).
  The dcache caches dentries, including *negative* dentries for failed
  lookups, to speed path resolution. `/proc/meminfo` has NO per-cache line
  ("Dcache:") — dcache pages show up inside `Slab`/`SReclaimable`.
- **WHY AI GETS IT WRONG**: models conflate the three structures ("file is
  the on-disk inode"), invent proc lines, or claim the dcache is drained by
  writing to `/proc/sys/vm/drop_caches=2` alone (it drops clean slab,
  including shrinkable dentries, but that is not a dcache-specific switch).
- **CORRECT REASONING**: name the role of each object for a `read()` call:
  fd → `struct file` → f_op → inode → address_space pages. Path lookup walks
  the dcache via RCU fast path; a miss allocates a new (possibly negative)
  dentry. Cache accounting is global, not per-name.
- **EXAMPLE** (bad): `grep -E "^Dcache:" /proc/meminfo` then concluding the
  dcache is empty.
- **COUNTEREXAMPLE** (good):
  ```sh
  grep -E "^(Slab|SReclaimable|SUnreclaim):" /proc/meminfo
  grep -E "^(dentry|inode_cache) " /proc/slabinfo
  ```
- **VERIFICATION**: the tracepoint route —
  ```sh
  echo 0 > /sys/kernel/tracing/tracing_on
  echo "d_lookup" > /sys/kernel/tracing/set_event
  echo 1 > /sys/kernel/tracing/tracing_on
  sleep 2; echo 0 > /sys/kernel/tracing/tracing_on
  head -25 /sys/kernel/tracing/trace
  ```
  Researched — not run on this host (no Linux/QEMU here).
- **SOURCE**: kernel-docs-fs (path lookup, dcache docs); kernel-source
  (fs/dcache.c, fs/inode.c).

## Quick reference table

| Claim | Correct fact | Status |
|---|---|---|
| Fair scheduler 6.6+ | EEVDF, virtual deadlines + lag vs avg_vruntime | KNOWN (kernel-docs-sched) |
| /proc/sched_debug fields | vruntime present; lag/deadline tokens appear 6.6+ | INFERRED (grep to confirm) |
| Buddy allocation | split on alloc, coalesce on free, orders 0..10 | KNOWN (kernel-source) |
| /proc/buddyinfo | per-zone free *block* counts per order | KNOWN (kernel-docs-mm) |
| Slab allocator | SLUB only on mainline (SLAB removed 6.9) | KNOWN (kernel-source) |
| /proc/slabinfo | col0=name, col1=active_objs, col2=num_objs | KNOWN (kernel-docs-mm) |
| kmalloc vs vmalloc | physical vs virtual contiguity; vmalloc sleeps | KNOWN (kernel-docs-mm) |
| dcache accounting | in Slab/SReclaimable; no "Dcache:" meminfo line | KNOWN (kernel-docs-fs) |
| file/dentry/inode | open instance / path name / metadata | KNOWN (kernel-docs-fs) |
