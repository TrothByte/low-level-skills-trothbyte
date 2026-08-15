---
name: kernel-scheduler-mm-vfs-internals
description: Use when writing or reviewing claims about Linux internals — the fair scheduler (CFS vs EEVDF), vruntime and lag, buddy/SLUB allocation, kmalloc vs vmalloc, and VFS dcache/inode behavior. Prevents stale pre-6.6 scheduler lore and invented /proc fields from passing as kernel knowledge.
---

# Kernel Scheduler / MM / VFS Internals

## When to use

- Answering or reviewing "how does the Linux scheduler/allocator/VFS work"
  questions where the answer depends on the running kernel version.
- Explaining CPU fairness (vruntime/lag), memory allocation choices
  (kmalloc/vmalloc/buddy/SLUB), or file path lookup (dcache/inode/file).
- Reading `/proc/sched_debug`, `/proc/buddyinfo`, `/proc/slabinfo`,
  `/proc/vmallocinfo`, or `/proc/meminfo` and interpreting their columns.
- Reviewing kernel-style code or documentation claims for an agent or PR.

## When not to use

- Drivers/syscall interfaces (use `kernel-uaccess-safety`, `kernel-atomic-context`).
- ftrace/kprobes/kdump *tooling* — use `kernel-debugging-ftrace-kprobes-kdump`.
- Containers/cgroups/namespaces — use `kernel-container-internals`.
- Actually modifying the kernel scheduler/MM/VFS — that needs the tree and a
  test host; this skill only validates the claims you make about it.

## What the agent often gets wrong

- Describing the default fair scheduler as CFS with "pick smallest vruntime"
  — since 6.6 it is EEVDF with virtual deadlines and lag vs `avg_vruntime`.
- Quoting `/proc/sched_debug` fields that no kernel prints
  (`cfs_avg_vruntime`), or claiming `lag:`/`deadline:` lines without checking.
- Buddy semantics: claiming merging happens on allocation (it splits; only
  free coalesces), or misreading `/proc/buddyinfo` counts.
- SLUB vs SLAB: claiming SLAB still ships (removed in 6.9) or misparsing
  `/proc/slabinfo` columns (column 1 is the cache name, not active objects).
- kmalloc vs vmalloc: using vmalloc in atomic/IRQ context (it sleeps) or
  pretending kmalloc can always satisfy large order-N requests.
- VFS: conflating file/dentry/inode and inventing a `Dcache:` line in
  `/proc/meminfo`.

## How to reason correctly

1. Anchor every claim to a kernel version; if the question is not about a
   specific version, state that the answer changed at 6.6 (EEVDF) and 6.9
   (SLAB removal).
2. For scheduler claims, name the mechanism: EEVDF placement uses a virtual
   deadline derived from vruntime and lag against `avg_vruntime`.
3. For allocator claims, state contiguity and context: kmalloc = physical,
   fast, atomic-safe; vmalloc = virtual, page-table-backed, may sleep;
   kvmalloc picks by size.
4. For proc claims, verify the column header of the file you cite
   (`head -1 /proc/slabinfo`, the buddyinfo header) instead of trusting
   memory; never grep for a line name you have not seen.
5. For VFS claims, separate the three objects: file (open instance), dentry
   (path component), inode (metadata); accounting is global slab memory.

## What to verify

- The kernel version (`uname -r`) before any scheduler claim.
- That a proc file exists and its header matches the claimed column order.
- That a "lag/deadline" token really appears in `/proc/sched_debug` output.
- That no claim relies on a line name that does not exist (`Dcache:`).
- That context limits (atomic/sleeping) are respected in allocation advice.

## How to verify

```
uname -r
grep -E "vruntime|lag|deadline|avg_vruntime" /proc/sched_debug | head -40
cat /proc/buddyinfo
head -1 /proc/slabinfo; grep -E "^(dentry|inode_cache) " /proc/slabinfo
grep -E "vmalloc" /proc/vmallocinfo | head -5
grep -E "^(Slab|SReclaimable|SUnreclaim):" /proc/meminfo
echo 0 > /sys/kernel/tracing/tracing_on
echo "d_lookup" > /sys/kernel/tracing/set_event
echo 1 > /sys/kernel/tracing/tracing_on; sleep 2; echo 0 > /sys/kernel/tracing/tracing_on
head -25 /sys/kernel/tracing/trace
```

Boot the kernel under QEMU for repeatable runs. On this host (Windows, no
Linux/QEMU) all commands above are researched and documented, not executed.

## Where the knowledge comes from

- `kernel-docs-sched` — sched-eevdf.rst; `/proc/sched_debug` description.
- `kernel-docs-mm` — buddy allocator, SLUB, vmalloc/vzalloc docs.
- `kernel-docs-fs` — path lookup, dcache and inode life cycle.
- `kernel-source` — kernel/sched/fair.c (`pick_eevdf`), mm/page_alloc.c,
  mm/slub.c, mm/vmalloc.c, fs/dcache.c.

## Related skills

- `kernel-debugging-ftrace-kprobes-kdump` — the tooling commands used in
  "How to verify".
- `kernel-container-internals` — cgroups v2 controller semantics that sit on
  top of the scheduler/MM.
- `kernel-rcu-memory-barriers`, `kernel-uaccess-safety`, `kernel-atomic-context`
  — other kernel-internals claims with the same verify-first discipline.

## Evaluation

- Synthetic: bad fixtures must be recognized (pre-6.6 CFS claims, invented
  proc fields, backwards slabinfo/vmallocinfo columns, vmalloc in IRQ).
- False-positive: version-gated, header-checked, tracepoint-based answers
  must pass; correct use of kvmalloc for medium allocations is fine.
- Adversarial: a "confident" wrong claim in plausible /proc wording
  (`cfs_avg_vruntime`, `Dcache:`) must be caught by requiring a real file
  check.
- Historical: no live kernel here — the CFS→EEVDF and SLAB→SLUB transitions
  are KNOWN kernel history (merged 6.6 / removed 6.9), but none of the
  `/proc` fixtures were executed on a real kernel (UNVERIFIED on hardware).
- Researched gap: all verification commands are exact and documented but
  were not run; a QEMU/Linux host is required to make this skill source-backed.
