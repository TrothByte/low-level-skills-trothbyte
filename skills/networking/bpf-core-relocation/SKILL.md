---
name: bpf-core-relocation
description: Use when writing or reviewing eBPF programs meant to run on many kernel versions — BTF-based CO-RE, vmlinux.h, bpf_core_read, field/type/enum relocation, or when direct kernel-struct access breaks across kernels. Teaches Compile-Once-Run-Everywhere BPF portability, distinct from verifier-feedback skills.
---

# BPF CO-RE Relocation

## When to use

- Writing kprobe / tracepoint / fentry programs (or any program that reads
  kernel structs such as `task_struct`, `mm_struct`, `sock`, `net`) that must
  run on more than one kernel version.
- Reviewing BPF code for hardcoded struct offsets, plain derefs of kernel
  pointers, or fields accessed without existence guards.
- Debugging load-time errors like `CO-RE relocation failed`, `can't find field`,
  `can't find type`, or `unknown relocation type` when loading a compiled
  object with `bpftool` / libbpf.
- Deciding whether an access needs `bpf_core_read`, `BPF_CORE_READ`, or
  `bpf_probe_read_kernel`, and whether a field needs a `bpf_core_field_exists`
  guard.

## When not to use

- Userspace or non-BPF code: CO-RE is a load-time BPF relocation stage, not a
  general portability layer.
- Kernels that do not expose vmlinux BTF (`CONFIG_DEBUG_INFO_BTF` off, or too
  old) — relocations cannot resolve there; that is a fallback-to-raw-helpers
  scenario, not a CO-RE one.
- A program that *loaded* but misbehaves at runtime: that is a runtime logic
  bug, not a relocation problem.
- Rejection messages from the kernel verifier (`R0 invalid mem access`, map
  bounds, loop limits): those are the verifier skills' domain — CO-RE failures
  happen in libbpf *before* the verifier runs.

## What the agent often gets wrong

- Writing `p->field` and expecting CO-RE to relocate it. A plain deref bakes in
  the compiling kernel's offset; in probe context (kprobe/fentry, page faults
  disabled) a bad kernel pointer faults instead of returning an error.
- Copying struct offsets from one kernel's disassembly or `pahole` output. The
  program then loads cleanly on that one kernel and silently reads the wrong
  field on every other.
- Dropping `-g` from `clang -target bpf` because "release builds do not need
  debug info" — without BTF there is nothing for libbpf to relocate.
- Hand-declaring kernel structs instead of including `vmlinux.h`, producing
  stale or conflicting layouts whose names match but whose offsets are wrong.
- Assuming a field or helper exists on all kernels and skipping
  `bpf_core_field_exists` / `bpf_core_type_exists` guards.
- Confusing CO-RE with the verifier. CO-RE is load-time portability; the
  verifier checks safety at load. A relocation error and a verifier rejection
  are different failure classes with different fixes.
- Reaching for `bpf_probe_read_kernel` for every read when the relocatable
  `bpf_core_read` / `BPF_CORE_READ` is the right pattern for named fields.

## How to reason correctly

1. Always compile with BTF: `clang -target bpf -g -O2 -c prog.c -o prog.o`,
   and include `vmlinux.h` (generated from the kernel's BTF) for kernel types.
   Check the object: `readelf -S prog.o` must show `.BTF` and `.BTF.ext`.
2. For every kernel-struct access in tracing context, route through
   `bpf_core_read` / `BPF_CORE_READ` or mark the access with
   `__builtin_preserve_access_index` so the loader relocates the offset. A
   plain deref is the anti-pattern.
3. Guard optional fields, types, and enum values with
   `bpf_core_field_exists()`, `bpf_core_type_exists()`, `bpf_core_enum_value()`
   so behavior degrades gracefully where the thing does not exist. The guard
   is resolved at load time and the dead branch is eliminated.
4. Treat the program as one binary for all kernels: test on the oldest and the
   newest kernel you must support; a single-kernel test proves nothing about
   portability.
5. Verify relocation at load with `bpftool prog load -d` and read the resolved
   values; hardcoded-offset and unguarded-optional-field bugs show up as wrong
   values or `CO-RE relocation failed`, not as verifier rejections.

## What to verify

- The program builds with `clang -target bpf -g -O2` and the object carries
  `.BTF` / `.BTF.ext`.
- `bpftool prog load` succeeds on at least two kernel versions (the model here
  simulates the v1/v2 difference with the python relocation checker).
- Every kernel-struct field read uses a relocatable pattern
  (`bpf_core_read`, `BPF_CORE_READ`, or `preserve_access_index`).
- Optional fields/types are guarded; there are no hardcoded offsets anywhere
  in the program.
- Load-time relocation succeeded: `bpftool prog load -d` output shows nonzero
  relocated offsets for the accessed fields.

## How to verify

Host (Windows-safe, run from this skill directory):

```
python examples/good/core_reloc_model.py    # relocation model: 6 PASS + 2 expected rejections, exit 0
python examples/bad/reloc_misuse.py         # hardcoded-offset misuse: checker rejects, exit 1
```

The good model builds two kernel layouts (v1: `task_struct.mm` at offset 0x10;
v2: moved to 0x18, `rss_stat.count` renamed) and shows that a relocated read
and a `bpf_core_field_exists`-guarded read stay correct on both, while an
unguarded optional field fails to relocate and a hardcoded offset silently
reads the wrong field on v2.

Target (Linux with libbpf/bpftool, requires BTF-enabled kernel):

```
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
clang -target bpf -g -O2 -c prog.c -o prog.o
bpftool prog load -d prog.o /sys/fs/bpf/prog    # -d prints relocated offsets
```

The C snippets `examples/good/portable_read.c` and
`examples/bad/hardcoded_offset.c` are TARGET-ONLY (they need vmlinux.h and
BPF headers); the relocation mechanics they rely on are proven on the host by
the python models.

## Where the knowledge comes from

- BPF and XDP Reference Guide — CO-RE (https://docs.cilium.io/en/latest/bpf/co-re/)
- BTF: BPF Type Format documentation (https://docs.kernel.org/bpf/btf.html)
- bpf-helpers / bpf_core_read man pages, libbpf docs (https://github.com/libbpf/libbpf)
- Facebook bpf-notes / BPF CO-RE references (https://github.com/facebookincubator/bpf-notes)

## Related skills

- `ebpf-verifier-reasoning` — the other half of "loads cleanly": verifier
  safety checks run after CO-RE relocation and reject programs the relocator
  accepted (require)
- `ebpf-verifier-opaque-feedback-iteration` — when a load fails and the
  message is opaque, isolate whether the failure is relocation-stage (this
  skill) or verifier-stage (recommend)
- `kernel-api-drift-migration` — API and struct layout drift across kernel
  versions, the underlying reason CO-RE exists (recommend)
- `kernel-scheduler-mm-vfs-internals` — what `task_struct` / `mm_struct`
  fields actually mean when the access you relocate is a scheduler or MM
  field (recommend)
- `kernel-rcu-memory-barriers` — memory-ordering context for fields read via
  `bpf_core_read` in trace points (recommend)

## Evaluation

Synthetic: the 8-case python relocation model (`core_reloc_model.py`) —
relocated and guarded reads must PASS on both layouts, unguarded optional
field and hardcoded offset must be detected as non-portable on v2; the bad
model (`reloc_misuse.py`) must exit non-zero. Each C fixture maps to a load
stage: `hardcoded_offset.c` (silent wrong value) vs `portable_read.c` (clean
load with relocated offsets).
False-positive: a `bpf_core_field_exists`-guarded optional field and a
`BPF_CORE_READ` chain must NOT be flagged; relocations that resolve must not
be reported as broken.
Historical: kernel struct churn breaking old BPF tools — `task_struct` and
related fields moved or renamed across 5.x/6.x releases, which is precisely
the v1/v2 move/rename modeled here.
Adversarial: a field present on v1 but renamed on v2; a struct field that
moved (offset change with no rename); direct deref of a kernel pointer where
`bpf_core_read` was required.
Verified on this host: both python models executed with recorded output
(2026-08-20); clang/bpftool target steps are documented-as-target (Windows
host, no clang BPF backend / no Linux).

For full detail see `references/bpf-core.md`.
