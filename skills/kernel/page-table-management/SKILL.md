---
name: page-table-management
description: Use when writing or reviewing virtual-memory code — x86-64 4/5-level paging, AArch64 VMSAv8-64, RISC-V Sv39/48, PTE flags, TLB invalidation, kernel vs user mappings, large pages, and page-fault debugging. Teaches page-table structure, walk logic, and the TLB-coherency rules that must hold after any PTE change.
---

# Page Table Management

## When to use

- Initializing or tearing down page tables (kernel VA setup, early boot, mmu
  init, hypervisor guest mappings).
- Changing PTE contents (protection, NX, present, large-page split) and deciding
  what TLB maintenance to issue.
- Writing an MMU driver or a hypervisor's stage-2 mapping (EPT/NPT).
- Debugging page faults, NULL-deref, or "works on one CPU core, crashes on
  another" (stale TLB).
- Reasoning about kernel vs user address ranges and guard pages.

## When not to use

- Pure userspace pointer/address math (no MMU involvement) — use `elf-layout`
  skills instead.
- Device-tree / DMA configuration — different layer (`embedded-device-tree`).
- I/O-mapped MMIO setup where only `pgprot_noncached` matters and no paging
  structure is touched.
- Platform-specific ASID/PCID tuning without a change to paging structures.

## What the agent often gets wrong

- "x86-64 paging has 3 levels" — long mode uses 4 levels (PML4→PDPT→PD→PT)
  with 4 KiB pages, 5 with LA57. PAE 32-bit uses 3. Confusing the two (B2).
- "A TLB flush is a single magic instruction" — granularity matters: `invlpg`
  (one VA), `invpcid` (per-PCID/global), full `cr3` reload; on ARM the
  instruction is `tlbi` (VA/ASID/VMID), on RISC-V `sfence.vma`.
- "The PTE is just a physical address" — the flag bits (P/RW/US/NX/A/D/PS)
  are as important as the PFN; a wrong `PS` bit turns a 4 KiB page into a 2 MiB
  one (A10).
- "After writing a PTE the change is visible" — the CPU may cache the old
  translation in the TLB; you must invalidate the specific entry or the whole
  context, and on ARM issue `dsb ish` + `isb` before relying on the new
  translation (B7).
- Mapping kernel memory as user-accessible or executable — the U/S and NX bits
  gate W^X; forgetting NX on a user page lets code run from data.
- "ASID/PCID is optional" — on ARM without ASID and on x86 without PCID, a
  full context switch forces complete TLB flushes; missing maintenance is a
  correctness bug, not a perf nit.

## How to reason correctly

1. Identify the architecture and granularity: x86-64 → 4 levels (PML4, PDPT,
   PD, PT), 9 bits per level, 4 KiB pages (PS bit gives 2 MiB/1 GiB); AArch64
   VMSAv8-64 → up to 4 levels with 4 KiB/16 KiB/64 KiB granule, 2 levels with
   the 1 GiB granule, TTBR0 (user) vs TTBR1 (kernel); RISC-V → Sv39 (3
   levels, 9+9+9), Sv48 (4 levels), satp register.
2. Compute the walk by hand for the address in question: level indices are
   `(va >> shift) & 0x1ff`, stop at the first level with `PS`/page-size bit
   set.
3. Set PTE flags deliberately: Present, R/W, User, NX, A, D. W^X: no page is
   simultaneously writable and executable.
4. After every PTE change, issue the matching TLB maintenance: x86
   `invlpg`/`invpcid`; ARM `tlbi` + `dsb` + `isb` (or the `v8` broadcast
   variants); RISC-V `sfence.vma` — at the correct scope (per-VA, per-ASID,
   per-VMID, global).
5. On a page fault, decode the error code (PF flags) — user vs kernel, present
   vs not, read vs write vs exec — before assuming a NULL-deref.
6. Verify with disassembly/stubs on the host and with an on-target boot test
   or hypervisor unit test where available.

## What to verify

- The computed level indices match a reference walk (host stub below).
- PTE flags are consistent with the mapping intent (no W+X, correct U/S).
- Every PTE store is followed by the correct-scope TLB invalidation.
- The walk terminates at the expected level (4K vs 2M vs 1G on x86).
- Kernel/VA split matches the architecture (TTBR0/TTBR1, PML4 index boundary).

## How to verify

Host-compilable logic check (no kernel headers needed):

```
gcc -Wall -Wextra -O2 examples/good/x86_64_walk.c -o /tmp/walk && /tmp/walk 0xffff888012345678
```

Target commands (documented, toolchain not on this host):

```
# ARM64: compile and inspect page-table setup from Linux source (not run here)
clang --target=aarch64-none-elf -O2 -c examples/good/aarch64_pgtable_snippet.c -o /dev/null
# RISC-V: same idea with Sv39 constants
clang --target=riscv64-unknown-elf -O2 -c examples/good/riscv_sv39_snippet.c -o /dev/null
# On-target TLB validation: boot a hypervisor stage-2 unit test under QEMU
qemu-system-aarch64 -machine virt -cpu cortex-a57 -kernel <test>
```

## Where the knowledge comes from

- `intel-sdm` — Vol.3A §4 (paging, 4/5-level, PTE flags, TLB)
- `arm-arm` — VMSAv8-64 translation tables, TLBI instructions
- `riscv-isa-spec` — privileged spec: Sv39/Sv48, satp, sfence.vma
- `linux-mm-docs` — kernel memory management and page-table documentation
- `kernel-source` — arch/x86/mm, arch/arm64/mm for real-world reference

## Related skills

- `kernel-uaccess-safety` — user/kernel pointer crossing relies on paging (recommend)
- `kernel-rcu-memory-barriers` — TLB maintenance and memory barriers in drivers (recommend)
- `hypervisor-vmx-svm-internals` — stage-2 paging (EPT/NPT) builds on this (recommend)
- `embedded-mpu-trustzone` — MPU vs MMU, trust boundary setup (recommend)
- `c-undefined-behavior` — pointer/alignment UB in paging code (recommend)

## Evaluation

Synthetic: x86-64 4-level walk for 0xffff888012345678 (PML4 idx), a 2 MiB
large-page walk, an ARM64 VMSAv8-64 3-level walk, a Sv39 walk. Adversarial:
"3-level paging on x86-64" answer must be rejected; a missing `invlpg` after a
PTE change; a W+X mapping. Historical: real Linux page-table bugs (arch/x86
`mmap_min_addr` guard, the classic `pmd` race), and the Meltdown/KPTI class
(page-table isolation relies on correct user/kernel PTE separation). FP: a
correct, complete walk with all levels and flags must NOT be flagged.
