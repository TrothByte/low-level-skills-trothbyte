# Page Table Walk & TLB Coherency — Reference

Sources: Intel SDM Vol.3A §4; Arm ARM (VMSAv8-64); RISC-V privileged spec;
Linux kernel mm documentation. The x86-64 walk constants are from `intel-sdm`
and were exercised with the gcc stub on this host (see evals/README.md).

## 1. x86-64 long-mode 4-level walk

- **RULE**: long mode walks PML4 → PDPT → PD → PT, 9 bits each (512 entries),
  with 4 KiB pages; `PS` set on a PDE gives a 2 MiB page, `PS` on a PDPTE gives
  1 GiB. Level index = `(va >> shift) & 0x1ff` where shifts are 12, 21, 30, 39.
- **WHY AI GETS IT WRONG**: "3 levels" (PAE/32-bit legacy) or "LA57 is the
  default".
- **CORRECT REASONING**: with 48-bit virtual addresses, indices are
  `PT=va[11:0]...va[20:12]`, `PD=va[29:21]`, `PDPT=va[38:30]`, `PML4=va[47:39]`.
- **EXAMPLE**: `0xffff888012345678` → PML4 index 273 (0x111), PDPT 0,
  PD 145, PT 325, offset 0x678 (verified by the stub run on this host).
- **COUNTEREXAMPLE**: treating `0xffff8880...` (canonical kernel VA) as a
  3-level walk gives nonsense offsets.
- **VERIFICATION**: `examples/good/x86_64_walk.c` — run with gcc on host.
- **SOURCE**: `intel-sdm` Vol.3A §4.4-4.5.

## 2. TLB invalidation granularity

- **RULE**: after modifying a PTE you must invalidate the affected translation
  at the correct scope: x86 `invlpg addr` (single VA), `invpcid` (per PCID),
  full TLB via `cr3` reload / `mov cr3`; ARM `tlbi vaae1is`/`tlbi vmalle1` +
  `dsb ish` + `isb`; RISC-V `sfence.vma` (optionally with rs1=VA, rs2=ASID).
- **WHY AI GETS IT WRONG**: "the hardware rereads the PTE on the next access"
  — it does NOT; it serves stale translations from the TLB until invalidated
  (B7: silent TLB coherency hole).
- **CORRECT REASONING**: TLB entries are cached translations, not pointers to
  the PTE; the walk reads memory only on a TLB miss. Invalidate the exact
  scope to avoid both stale entries (correctness) and global flushes (perf).
- **EXAMPLE**: a page-fault handler that unprotects a page must `invlpg` the
  faulting VA before returning; on ARM the `dsb`/`isb` ordering after `tlbi`
  matters.
- **COUNTEREXAMPLE**: modifying a PML4 entry without any TLB maintenance —
  other CPUs keep the old top-level translation.
- **VERIFICATION**: on-target: kernel test (boot + touch page), or a
  hypervisor stage-2 unit test under QEMU. Documented, not run on host.
- **SOURCE**: `intel-sdm` Vol.3A §4.10; `arm-arm` (TLBI, DSB, ISB).

## 3. PTE flags: present, R/W, U/S, NX, PS, A, D

- **RULE**: bit layout per architecture: x86 PTE bit 0=P, 1=R/W, 2=U/S,
  3=PWT, 4=PCD, 5=A, 6=D, 7=PS, 63=NX. AArch64 4K descriptor similar with
  UXN/PXN/AP. RISC-V PTE has V,R,W,X,U,G,A,D bits.
- **WHY AI GETS IT WRONG**: mixing bit meanings between architectures or
  forgetting NX means "execute disabled" — a page mapped NX=0 is executable.
- **CORRECT REASONING**: always read the architecture's descriptor format
  table; W^X means no page may have both W and X set, regardless of ISA.
- **EXAMPLE**: x86 kernel text is supervisor-only (U/S=0) and NX=1.
- **COUNTEREXAMPLE**: mapping user data with U/S=1 and NX=0 — classic
  DEP/W^X violation (A10).
- **VERIFICATION**: host stub check of flag bit positions against the spec
  table; target: `readelf -l`/`dmesg` NX warnings.
- **SOURCE**: `intel-sdm` Vol.3A §4.1.3/4.3; `riscv-isa-spec` privileged.

## 4. Large pages and their TLB behavior

- **RULE**: 2 MiB pages halve the walk (PDE has PS set, no PT); 1 GiB pages
  (PDPTE PS) skip two levels. Large pages reduce TLB pressure but make
  fine-grained protection impossible.
- **WHY AI GETS IT WRONG**: assuming the walk always terminates at the PT level.
- **CORRECT REASONING**: read the PS bit at each level; terminate early. When
  splitting a large page, you must invalidate the old entry — a stale 2 MiB
  translation hides the new 4 KiB PTEs.
- **EXAMPLE**: `mmap` of 2 MiB-aligned region with `MAP_HUGETLB` / `hugepages`.
- **COUNTEREXAMPLE**: changing protection on a 2 MiB page by only editing the
  PT-level PTEs that were never allocated.
- **VERIFICATION**: host stub returns the terminating level for a given VA.
- **SOURCE**: `intel-sdm` Vol.3A §4.4.
