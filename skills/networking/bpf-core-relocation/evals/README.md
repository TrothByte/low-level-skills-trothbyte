# Evaluation — bpf-core-relocation

Skill: `skills/networking/bpf-core-relocation`. Stability: `researched`
(reference + host-verified python model; target clang/bpftool steps are
documented-as-target because the host is Windows without a BPF toolchain).

## Verified facts (host, recorded 2026-08-20)

| Fact | Status | Evidence |
|---|---|---|
| Python 3.11.9 available on host | VERIFIED | `python --version` |
| `python examples/good/core_reloc_model.py` runs, exit 0 | VERIFIED | output below |
| good model: relocated read `task.mm` PASS on v1 and v2 | VERIFIED | output below |
| good model: `bpf_core_field_exists`-guarded read PASS on v1 and v2 (graceful fallback on v2) | VERIFIED | output below |
| good model: unguarded optional field flagged on v2 (relocation unresolvable) | VERIFIED | output below |
| good model: hardcoded offset `0x10` flagged on v2 (silently reads `task_struct.pid`) | VERIFIED | output below |
| `python examples/bad/reloc_misuse.py` runs, exit 1 (checker rejects) | VERIFIED | output below |
| clang with BPF target on host | UNVERIFIED / absent | not installed |
| bpftool / Linux BTF kernel on host | UNVERIFIED / absent | eBPF loading requires Linux |
| `clang -target bpf -g -O2` + `bpftool prog load -d` relocation values | documented-as-target | commands in SKILL.md; not executed here |
| libbpf load-error strings (`CO-RE relocation failed`, `can't find field`) | VERIFIED (from source) | libbpf `bpf_core.c`/`libbpf.c`; Cilium CO-RE docs |

### Host output — good model (executed 2026-08-20)

```
CO-RE field-offset relocation model (guarded access degrades, hardcoded offsets break)

v1 layout: task_struct.mm@16, rss_stat.count exists
v2 layout: task_struct.mm@24 (moved), rss_stat.count renamed to rss_stat.stat

  [1] relocated read task.mm on v1: got 0x20, expected 0x20 -> PASS
  [1] relocated read task.mm on v2: got 0x20, expected 0x20 -> PASS
  [2] guarded read rss_stat.count on v1: got 0xc0ffee, expected 0xc0ffee -> PASS
  [2] guarded read rss_stat.count on v2: got 0xf411bac, expected 0xf411bac -> PASS
  [3] unguarded read rss_stat.count on v1: reads 0xc0ffee -> PASS
  [3] unguarded read rss_stat.count on v2: unresolvable -> REJECT_DETECTED (load-time relocation failure)
  [4] hardcoded offset +0x10 on v1: got 0x20 -> PASS
  [4] hardcoded offset +0x10 on v2: got 0x1122 -> REJECT_DETECTED: silently read task_struct.pid: mm moved to +0x18 on v2, offset +0x10 now holds another field

RESULT: 6 portable accesses read correctly (relocated task.mm@v1, relocated task.mm@v2, guarded task.mm.rss_stat.count@v1, guarded task.mm.rss_stat.count@v2, unguarded rss_stat.count@v1, hardcoded offsetof(task_struct,mm)@v1); the checker correctly REJECTS 2 non-portable patterns on v2: unguarded optional field -> load-time relocation failure, hardcoded offset -> silent wrong value (correct on v1 only because it was tuned to v1's offsets). A program using patterns [1] and [2] is Compile-Once, Run-Everywhere; patterns [3] and [4] must be rejected.
```

### Host output — bad model (executed 2026-08-20, exit 1)

```
BAD program model: hardcoded offsets from kernel v1, no CO-RE relocation, no bpf_core_field_exists guards

  v1: task+0x10 -> mm pointer 0x20 (offset 0x10 is task_struct.mm on v1, so this read is correct)
  v2: task+0x10 -> 0x1122 == task_struct.pid -- NOT the mm pointer (expects 0x20). The kernel loads the program and it silently reads the wrong field.
  v1: rss_stat.count read -> 0xc0ffee
  v2: relocation for rss_stat.count cannot resolve (field renamed to rss_stat.stat on v2). libbpf load error: 'can't find field 'rss_stat.count'' -- program fails to load at runtime on v2, or worse, the field is read uninitialized if the fallback is missing.

RESULT: checker REJECTS this program: 2/2 accesses are wrong or unloadable on kernel v2. The good checker (examples/good/core_reloc_model.py) flags hardcoded offsets and unguarded optional fields; portable code uses bpf_core_read + bpf_core_field_exists and records relocations, not offsets.
```

## Synthetic evals

| Case | Expected | Fixture |
|---|---|---|
| Relocated field read, field present on both kernels | PASS both | good model `[1]` |
| `bpf_core_field_exists`-guarded read, field renamed on v2 | PASS both, fallback on v2 | good model `[2]` |
| Unguarded optional field | v1 PASS, v2 REJECT_DETECTED (load-time relocation failure) | good model `[3]`, bad model access 2 |
| Hardcoded offset | v1 PASS (by luck), v2 REJECT_DETECTED (silent wrong value) | good model `[4]`, bad model access 1, `examples/bad/hardcoded_offset.c` |
| Portable pattern (`BPF_CORE_READ` + guard) | clean target load | `examples/good/portable_read.c` (target-only) |

The python checker must exit 0 on the good model and 1 on the bad model; that
exit contract is the runnable gate.

## False-positive evals

- A `bpf_core_field_exists`-guarded optional field must NOT be reported as a
  portability bug — it is the correct degradation mechanism.
- A `BPF_CORE_READ(task, mm, rss_stat, count)` chain with all fields present
  must NOT be reported as a relocation failure.
- A relocated read on a kernel where the field exists must NOT be flagged just
  because a *different* field is optional.
- The v1 PASS of the hardcoded pattern must NOT be read as evidence that
  hardcoded offsets are acceptable.

## Historical evals

- Kernel struct churn repeatedly broke BPF tools that hardcoded offsets:
  `task_struct` layout changed across 5.x/6.x (fields added, reordered, or
  renamed), which is the real-world shape of the v1→v2 move/rename in the
  model. BTF (introduced ~5.2, vmlinux BTF standardized in later 5.x with
  `CONFIG_DEBUG_INFO_BTF`) is the kernel-side answer to that churn.
- Historical `bpf_probe_read` → `bpf_probe_read_kernel` → `bpf_core_read`
  evolution: raw probe reads do not relocate; the relocatable wrappers are the
  durable pattern.
- The eval must reproduce the "loads on the tuned kernel, silently wrong on
  others" failure mode, and the "renamed field kills the load" failure mode —
  both present in the recorded output above.

## Adversarial evals

- Field present on v1, renamed on v2 (`rss_stat.count` → `rss_stat.stat`):
  unguarded access must be detected (relocation unresolvable).
- Struct field moved with no rename (`task_struct.mm` 0x10 → 0x18): hardcoded
  offset must be detected via wrong read value.
- Direct deref `t->mm` where `bpf_core_read` is required: must be flagged (no
  relocation; probe-context fault risk).
- Guard on the *wrong* field (guarding `rss_stat.count` while reading
  `rss_stat.stat` unguarded) must still be detected.
- Offset hardcoded as the result of a `__builtin_offsetof` computed at
  compile time from a stale header — same detection as a literal constant.

## Verification commands (target — Linux with libbpf/bpftool)

Requires a BTF-enabled kernel (`CONFIG_DEBUG_INFO_BTF=y`), clang with the BPF
backend, libbpf, bpftool, and root.

```
# 1. generate vmlinux.h from the running kernel's BTF
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

# 2. build the good and bad C fixtures
clang -target bpf -g -O2 -c examples/good/portable_read.c -o /tmp/portable.o
clang -target bpf -g -O2 -c examples/bad/hardcoded_offset.c -o /tmp/hardcoded.o

# 3. inspect: BTF sections must exist; -d prints relocated offsets
readelf -S /tmp/portable.o | grep BTF
bpftool prog load -d /tmp/portable.o /sys/fs/bpf/portable     # expect success + nonzero relocations
bpftool prog load -d /tmp/hardcoded.o /sys/fs/bpf/hardcoded   # loads on the tuned kernel
# 4. portability check: repeat step 3 on the OLDEST and NEWEST kernels you must
#    support; diff the relocated offsets (bpftool -d) between them.
```

Host (any OS, no Linux needed):

```
python examples/good/core_reloc_model.py   # exit 0: 6 PASS + 2 expected rejections
python examples/bad/reloc_misuse.py        # exit 1: checker rejects the bad approach
```

## Scoring

- precision: every rejected pattern maps to a real CO-RE failure class
  (unresolvable relocation, or silent wrong-value read); no good pattern is
  rejected.
- recall: unguarded optional field and hardcoded offset must both be caught by
  the checker on the v2 layout.
- FP-rate: relocated and guarded reads on both layouts must pass; the historical
  and adversarial cases must not produce false portability verdicts.
- Target confidence is `researched`: load-time relocation behavior on real
  kernels (step 3/4 above) is documented, not yet executed on this host.
