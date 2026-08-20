# ARM MTE (Memory Tagging Extension) — reference

This is the deep reference for `arm-mte-programming`. The SKILL.md carries the
operational rules; this file holds the background, constants, and instruction
details. Stability: `researched` — the tag-machinery model is verified on this
host (see `evals/README.md`), but every value marked TARGET-ONLY requires an
Armv8.5+/Armv9 machine with FEAT_MTE (real hardware, Android device, or QEMU
with `-cpu max`) to execute.

## 1. What MTE is

Memory Tagging Extension, introduced in Armv8.5-A, mandatory in Armv9-A
(FEAT_MTE / FEAT_MTE2). Hardware memory-safety:

- Every **16-byte granule** of memory has a **4-bit allocation tag**
  (16 possible values) stored in the memory system (adjacent granule tag data
  is held in the tags, not in the mapped address space).
- A **tagged pointer** carries the pointer tag in bits 56-59 of the 64-bit
  address. Addressing uses Top-Byte-Ignore (TBI, FEAT_TBI): the whole top byte
  (bits 56-63) is ignored for address translation, so the tag costs nothing on
  dereference.
- On every memory access the hardware compares the pointer tag with the
  granule's allocation tag. Mismatch -> tag fault.

Because the free path re-tags the granule with a fresh random tag, a stale
pointer (old tag) faults — that is the use-after-free guarantee. Because
adjacent granules hold independent (usually different) tags, an overflow past
the granule boundary faults — that is the (probabilistic) spatial guarantee.

## 2. Modes of operation (Tag Check Fault, TCF)

Controlled per process via prctl; applies to accesses made at EL0.

| Mode | Reports | Use |
|---|---|---|
| MTE_SYNC | precise: fault reported at the faulting instruction, SIGSEGV `SEGV_MTESERR` | debugging, finding the exact line |
| MTE_ASYNC | deferred: the memory system checks in the background, kernel delivers `SEGV_MTEAERR` later | production: lower overhead, but the reported PC/si_addr is NOT the bug site |
| MTE_ASYMM | synchronous for loads, asynchronous for stores | asymmetric protection, e.g. where stores are on a hotter path |

KNOWN/INFERRED: ASYMM availability is CPU-implementation dependent
(FEAT_MTE, the original "MTE" with only ASYNC store checking in some
implementations). For portable code verify availability per-device before
selecting ASYMM.

## 3. Tag and granule numbers

- Granule: 16 bytes. An allocation smaller than 16 bytes still occupies one
  granule and has one tag; the tag covers the whole granule regardless of the
  object's real size.
- Tags: 4 bits -> 16 values. With 16 colors, tag reuse is probabilistic. A
  fresh random tag per allocation means a stale or malicious pointer matches
  with probability ~1/16 per granule. MTE is therefore **probabilistic**, not
  a substitute for bounds checking.

## 4. Instructions (AArch64, A64)

| Mnemonic | Meaning |
|---|---|
| IRG <Xd>, <Xn> | Insert Random Tag: copy Xn to Xd, set bits 56-59 to a random tag |
| ADDG / SUBG | add/sub with tag + address carry: arithmetic on both address and tag |
| LDG / STG | load / store the allocation tag of the granule containing the address |
| LDGM / STGM | load / store multiple granules' allocation tags |
| ST2G / LD2G | store/load allocation tag and set/clear memory to 0/1 (guard memory) |
| GMI | get memory tag index from the register pair |
| SUBPS | subtract pointers and tags |
| TST | test tags: compare address tag vs allocation tag, set flags |

TARGET-ONLY: all of the above execute on ARMv8.5+/Armv9 hardware. The host
x86 MinGW toolchain cannot assemble or run them.

Excluded addresses: per ELR/DCZID and the address translation, tag-checking
may be disabled for certain memory (e.g. device memory is never tagged;
`DC ZVA` and `DC GZVA` zero the allocation tags). Kernel memory (EL1) tagging
is a separate feature; the prctl path below tags only EL0 accesses.

## 5. Enabling MTE on Linux (arm64)

1. Hardware: `ID_AA64PFR1_EL1.MTE` (bits 8-11) != 0. `1` = EL0 only,
   `2` = EL1+EL0. Also check the `mte` flag in `/proc/cpuinfo`.
2. Kernel: arm64 with `ARM64_MTE` (CONFIG_ARM64_MTE). The kernel clears the
   tags on fork/exec and on the `mte` flag transitions.
3. Enable for the process:

```c
prctl(PR_SET_TAGGED_ADDR_CTRL,
      PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC /* or ASYNC/ASYMM */
      | PR_MTE_TAG_MASK,   /* 0xffff << 3: which tag values IRG may pick */
      0, 0, 0);
```

Constants (Linux UAPI, `include/uapi/linux/prctl.h`):

```
PR_SET_TAGGED_ADDR_CTRL  55
PR_GET_TAGGED_ADDR_CTRL  56
PR_TAGGED_ADDR_ENABLE    (1UL << 0)
PR_MTE_TCF_SHIFT         1
PR_MTE_TCF_NONE          (0UL << 1)
PR_MTE_TCF_SYNC          (1UL << 1)
PR_MTE_TCF_ASYNC         (2UL << 1)
PR_MTE_TAG_MASK_SHIFT    3
PR_MTE_TAG_MASK          (0xffffUL << 3)
```

4. Signal: tag faults arrive as SIGSEGV. Code values (`asm/sigcontext.h`):

```
SEGV_MTEAERR  35   async tag error (MTE_ASYNC) — not precise
SEGV_MTESERR  36   sync tag error (MTE_SYNC)   — si_addr is the fault address
```

5. Tagged pointers and C: with TBI on, the tagged pointer dereferences like a
   plain pointer (the hardware ignores the top byte). You do NOT need to mask
   before dereferencing in the tagged region; do mask for address comparisons
   and for untagged syscalls/pointers handed to code that does arithmetic on
   the top byte.

## 6. Android specifics

- Available on Armv9 SoCs: Snapdragon 8 Gen 2+, Google Tensor G3 (Pixel 8 and
  later); Android 13 introduced MTE support, Android 14 enabled it on
  supported hardware. `/proc/cpuinfo`/`ID_AA64PFR1_EL1` still gate it.
- Scudo (the Android user-space allocator) integrates MTE: it keeps
  allocations 16-byte aligned and re-tags on free.
- HWASan (AArch64) emulates the MTE model in software using shadow memory and
  the same bits 56-59; it is the standard way to find MTE-detectable bugs on
  hosts/emulators without MTE hardware. MTE and HWASan share the tag-visible
  semantics but not the implementation; an MTE-clean binary may still have
  HWASan-uncovered bugs and vice versa (different instrumentation points).
- Chromium: Android builds shipped with MTE enabled (rollout on Pixel 8-era
  devices); their metrics treat MTE as an in-production detector, not a
  fixer, and ASYNC mode keeps overhead low.

## 7. What MTE does NOT do

- Does NOT catch intra-granule overflow: two objects inside one 16-byte
  granule share a tag; an overwrite from one to the other matches.
- Does NOT catch logical errors, wild pointers that happen to carry a matching
  tag (~1/16 probability per granule), or non-adjacent spatial errors.
- Does NOT tag stack on its own: stack tagging needs compiler support
  (`-fsanitize=memtag`/`-fsanitize=memtag-stack` or libc stack tagging) and
  the stack must be 16-byte aligned.
- ASYNC faults are delayed; never attribute ASYNC reports to the PC/si_addr.

## 8. Verification targets (TARGET-ONLY)

```
# Real ARMv9 Linux / Android (adb root) or QEMU user mode:
qemu-aarch64 -cpu max ./prctl_mte            # expect "MTE enabled (SYNC)"
grep -E '^Features.*\bmte\b' /proc/cpuinfo   # expect mte present
# Compile the C sketches (target toolchain):
aarch64-linux-gnu-gcc -mcpu=armv9-a -march=armv9-a+memtag -o /tmp/mte \
    examples/good/prctl_mte.c
```

## 9. Sources

- Arm Memory Tagging Extension whitepaper (Arm Developer, PDF).
- Arm Architecture Reference Manual (Armv9), FEAT_MTE chapter.
- Linux kernel: `Documentation/arch/arm64/memory-tagging-extension.rst`.
- Android security docs: memory tagging / HWASan.
- AArch64 Base Instructions: IRG/ADDG/LDG/STG/ST2G/TST/GMI/SUBPS.
