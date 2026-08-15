# Evaluation — kernel-scheduler-mm-vfs-internals

Skill: `skills/kernel/kernel-scheduler-mm-vfs-internals`.
Toolchain status: RESEARCHED. No Linux host, no QEMU guest, no kernel source
tree on this machine. Every verification command below is exact and
documented but was NOT run. Claim status is marked per fact in
`references/sched-mm-vfs-internals.md`.

## Synthetic evals (researched — expected behavior documented, not executed)

| Case | Fixture | Expected on Linux | Command |
|---|---|---|---|
| sched/negative | `bad/sched_cfs_vruntime_average.sh` | grep returns empty, version contradicts claim | `grep -m1 cfs_avg_vruntime /proc/sched_debug` |
| mm/negative | `bad/mm_vmallocinfo_columns.sh` | column meanings wrong (col0=caller addr not size) | `head -3 /proc/vmallocinfo` |
| mm/negative | `bad/vfs_dcache_meminfo.sh` | no `Dcache:` line in meminfo | `grep -E "^Dcache:" /proc/meminfo` |
| sched/positive | `good/sched_eevdf_check.sh` | prints version + vruntime/lag/deadline tokens | `grep -E "vruntime\|lag\|deadline" /proc/sched_debug` |
| mm/positive | `good/mm_buddy_slub_check.sh` | buddyinfo + slabinfo + vmallocinfo headers parse | `cat /proc/buddyinfo` etc. |
| vfs/positive | `good/vfs_dcache_trace.sh` | d_lookup tracepoint records path walk events | `echo d_lookup > /sys/kernel/tracing/set_event` |

## Verified facts (ACTUAL on this host)

None — no Linux kernel artifacts are reachable from this Windows host. The
following facts are KNOWN from the cited primary sources (see references/),
not from a local run:

- EEVDF merged as the fair scheduler in Linux 6.6; SLAB allocator removed in
  6.9 (kernel history, kernel-docs-sched/kernel-source).
- `/proc/slabinfo` header order: name, active_objs, num_objs, objsize,
  objperslab, pagesperslab (kernel-docs-mm).
- `/proc/meminfo` exposes `Slab`, `SReclaimable`, `SUnreclaim`, not
  per-cache lines (kernel-docs-fs).

Status: UNVERIFIED for anything requiring a running kernel.

## False-positive evals (researched)

- A version-gated answer ("on 6.6+ EEVDF... on 6.5 and earlier CFS...") must
  pass even though it mentions two algorithms.
- `/proc/buddyinfo` free-block counts read per order with the zone name as
  the first token — correct, not a misparse.
- `kvmalloc` recommended for a medium, may-sleep allocation is correct and
  must not be flagged.

## Adversarial evals (researched)

- A plausible but false proc field embedded in otherwise correct advice
  (`cfs_avg_vruntime`, `Dcache:`) must be caught by the "grep before you
  cite" rule.
- A wrong column-order claim for slabinfo presented as a fact must be caught
  by comparing against `head -1 /proc/slabinfo`.

## Historical evals

- CFS→EEVDF (v6.6) and SLAB→SLUB-only (v6.9) are documented kernel history
  used as version anchors; the 6.6 boundary is the skill's calibration
  point. No per-case CVE database applies here.

## Target toolchains (absent, documented)

- Linux host / QEMU guest: not present. Planned elevation: boot a distro
  kernel under QEMU (`qemu-system-x86_64 -kernel vmlinuz ...`), run the
  good/*.sh fixtures, record real `/proc` output.
- `trace-cmd`/`perf`: not present; commands documented for the target host.
