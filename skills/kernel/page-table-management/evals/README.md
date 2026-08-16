# Evaluation — page-table-management

Skill: `skills/kernel/page-table-management`. Type: unique.
Stability: source-backed (x86-64 walk model compiled and run with gcc 16.1 on
this host — indices recorded; ARM/RISC-V descriptors are researched targets).

## Synthetic evals

| Case | Fixture | Expected | Status |
|------|---------|----------|--------|
| 4-level x86-64 walk of canonical VA | `examples/good/x86_64_walk.c` | PML4=511, PDPT, PD, PT indices | compiles/runs, see facts |
| PAE-width confusion | `examples/bad/wrong_level_widths.c` | FLAG: 10-bit fields wrong | compiles, silent |
| 2 MiB large-page early termination | walk stub with PS bit | terminates at PDE | logic check |
| NX/W^X flag placement | flag table | NX bit 63 on x86 | spec check |

## False-positive evals (correct code that must NOT be flagged)

- A correct 4-level walk with all indices computed by the reference formula.
- A `pmd`/`pud`-level kernel walk that correctly stops early on a large page.
- A correctly `invlpg`-following-PTE-change sequence (invalidate after the
  store) — must NOT be flagged as missing maintenance.

## Historical evals

- **Meltdown / KPTI (CVE-2017-5754)** — page-table isolation (KAISER/KPTI)
  split the kernel page table; agent must explain why user/kernel PTE
  separation and U/S-bit handling are the mitigation mechanism, and why
  `invlpg`/TLB flush timing matters for the fix.
- **Linux `arch/x86` PTE races** — kernel page-table updates must hold the
  correct `page_table_lock`/`mmap_lock` and issue TLB invalidation under the
  lock; agent must identify a missing-lock PTE write as a race even if it
  "works" on a single core.
- **CVE-2021-22555-class** (x_tables OOB) — not directly paging, but the
  boundary between user-visible pages and kernel-owned pages (guard pages)
  must be checked.

## Adversarial evals (compiles-but-wrong)

- A function that writes a PTE and returns without any TLB invalidation —
  compiles, often works on first run (TLB miss), breaks on the second access
  or on another CPU.
- A 2 MiB page "split" that edits PT-level PTEs which were never allocated —
  the new entries are dead until the large-page entry is invalidated.
- A W+X mapping presented as "just for convenience" — must be rejected under
  the W^X rule.

## Verification commands

Host (executed on this host):

```
gcc -Wall -Wextra -O2 examples/good/x86_64_walk.c -o /tmp/walk && /tmp/walk 0xffff888012345678
gcc -Wall -Wextra -O2 examples/bad/wrong_level_widths.c -o /tmp/badwalk && /tmp/badwalk 0xffff888012345678
```

Target (documented, toolchain not on this host):

```
clang --target=aarch64-none-elf -O2 -c examples/good/aarch64_pgtable_snippet.c
clang --target=riscv64-unknown-elf -O2 -c examples/good/riscv_sv39_snippet.c
qemu-system-aarch64 -machine virt -cpu cortex-a57 -kernel <stage-2 test>
```

## Verified facts (KNOWN / INFERRED / UNVERIFIED)

- KNOWN: the gcc stub compiled and ran on this host; for
  `0xffff888012345678` it printed PML4=273, PDPT=0, PD=145, PT=325,
  offset 0x678 (cross-checked against a Python recomputation on the same
  host).
- INFERRED: AArch64 VMSAv8-64 4-KiB-granule walk uses 9-bit fields with
  TTBR0/TTBR1 split and T0SZ=16 for 48-bit VA (researched from `arm-arm`).
- INFERRED: RISC-V Sv39 uses 3 levels × 9 bits with `satp` encoding
  (researched from `riscv-isa-spec`).
- UNVERIFIED: on-target TLB invalidation behavior (no ARM/RISC-V hardware or
  QEMU with those targets on this host).

## Scoring

- Precision: high for the x86-64 walk (structurally verified). Recall: limited
  to the specified architectures; ARM/RISC-V commands are documented but not
  run. FP-rate: low — correct walks and correctly-invalidated PTE changes pass.

## Tooling availability (honest)

- Available on this host: gcc 16.1.0, python 3.11.9.
- NOT installed: clang ARM/RISC-V targets, QEMU system emulation, kernel build
  tree, KVM. ARM/RISC-V verification commands are documented as target
  commands, not executed.
