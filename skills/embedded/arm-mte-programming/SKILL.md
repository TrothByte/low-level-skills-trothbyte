---
name: arm-mte-programming
description: Use when writing or reviewing memory allocators, garbage collectors, or pointer-tagging code for Armv9/Android, or when debugging hardware tag faults. Covers ARM Memory Tagging Extension modes (SYNC/ASYNC/ASYMM), 16-byte granules, 4-bit tags, IRG/LDG/STG instructions, top-byte-ignore, and enabling MTE with prctl.
---

# ARM MTE (Memory Tagging Extension) Programming

## When to use

- Writing or reviewing an allocator (Scudo-style, custom, or embedded libc) that
  tags memory for use-after-free and heap-overflow detection on Armv9/Android.
- Instrumenting code with IRG/ADDG/STG/LDG, or debugging a SIGSEGV that arrives
  as `SEGV_MTESERR` (sync) or `SEGV_MTEAERR` (async).
- Enabling or tuning MTE via `prctl(PR_SET_TAGGED_ADDR_CTRL, ...)`, choosing
  SYNC vs ASYNC vs ASYMM, or explaining why a stale-pointer dereference no
  longer faults.
- Reviewing LLM-produced memory-safety patches that add tagging to arbitrary
  allocations or "fix" the wrong line after an async tag report.

## When not to use

- x86/other-arch allocator review — no MTE; use the sanitizer skills
  (`sanitizer-report-reading`, `sanitizer-agent-ci-loop`) or
  `c-undefined-behavior`.
- Precise spatial bounds checking — MTE is probabilistic (16 colors); for exact
  bounds use ASan/HWASan (see `c-undefined-behavior`, `page-table-management`).
- Debugging a generic SIGSEGV with no tag-fault code — first rule out plain
  null/wild pointers (`c-undefined-behavior`, `debugging-crash-triage`).
- Purely C++/Rust allocator abstraction logic where the hardware behavior is
  irrelevant — stay in `embedded-volatile-and-memory-ordering` /
  `memory-ordering-reasoning` territory.

## What the agent often gets wrong

- "Tags are per-object." They are per-16-byte granule; two objects sharing one
  granule share a tag, and intra-granule overflow is NOT caught.
- "A tagged pointer just works." It does only with TBI enabled and with the
  top byte ignored for translation; without TBI the tag bits are real address
  bits and the pointer is garbage.
- "ASYNC report means the fault is at si_addr." In MTE_ASYNC the fault is
  detected asynchronously and reported later — the reported PC/si_addr is a
  hint, and "fixing" it chases the wrong line. Re-run in SYNC for diagnosis.
- "I can tag any malloc chunk." Allocation must be 16-byte aligned so the
  object owns a granule; unaligned objects share a granule (and tag) with
  neighbors.
- "MTE is bounds checking." It catches use-after-free and cross-granule
  overflow probabilistically; 4-bit tags mean ~1/16 chance a stale or crafted
  pointer still matches.
- "The kernel/Scudo does everything." Scudo tags on Android; a custom
  allocator must do IRG+STG on alloc and re-tag on free itself, or the
  use-after-free guarantee silently disappears.
- Getting prctl constants wrong: `PR_MTE_TCF_SYNC` is `(1UL<<1)`,
  `PR_MTE_TCF_ASYNC` is `(2UL<<1)`, the mode is OR'd with
  `PR_TAGGED_ADDR_ENABLE` and `PR_MTE_TAG_MASK`, and `prctl` must return 0 —
  a nonzero return means "unsupported here", not "enabled".

## How to reason correctly

1. Gate everything on support first: `ID_AA64PFR1_EL1.MTE != 0` (or the `mte`
   flag in `/proc/cpuinfo`) and a successful
   `prctl(PR_SET_TAGGED_ADDR_CTRL, ...)`. If prctl fails, the allocation must
   fall back to untagged — never assume tags are in effect.
2. Keep allocations 16-byte aligned; give each allocation a fresh random tag
   (IRG) and store it over the range (STG). The kernel/Scudo do this on
   Android, but a custom allocator must do it explicitly.
3. On free, poison: store a fresh random tag over the granule. A stale pointer
   then faults on the next access — that is the entire use-after-free story.
4. In SYNC mode the faulting instruction is precise; in ASYNC mode treat the
   report as "somewhere recently" and re-run under SYNC to localize.
5. Remember TBI means the tagged pointer dereferences like an ordinary pointer
   (bits 56-63 ignored); no special dereference path is needed inside tagged
   memory.
6. Treat MTE as probabilistic: 4-bit tags, ~1/16 match chance per granule.
   Complement with bounds checks and never claim MTE proves spatial safety.

## What to verify

- MTE presence check is present and correct: `ID_AA64PFR1_EL1.MTE`, the `mte`
  `/proc/cpuinfo` flag, and the `prctl` return value (0 == enabled).
- Every tagged allocation is 16-byte aligned and tagged with STG; every free
  re-tags (poisons) the granule before returning the chunk.
- SYNC/ASYNC choice matches the use: SYNC for debugging (precise), ASYNC for
  production; ASYMM only where the implementation supports it.
- The signal/reporting path distinguishes `SEGV_MTESERR` (precise address)
  from `SEGV_MTEAERR` (not precise) and never "fixes" an ASYNC address.
- Tagged pointers never leak into address arithmetic (mask top byte before
  comparisons and before passing to untagged code).

## How to verify

The verifiable core is a host-runnable Python model of the tag machinery
(4-bit tags, 16-byte granules, pointer tag in bits 56-59, IRG/STG/LDG, SYNC vs
deferred ASYNC faults):

```
python examples/good/mte_sim.py
# Expected: 6 scenarios, "RESULT: 6 passed, 0 failed", exit 0.
# Scenarios: UAF faults; cross-granule overflow faults; intra-granule overflow
# NOT caught; correct alloc/free/re-alloc; TBI-off ignores tags; ASYNC defers.
```

Target (ARMv8.5+/Armv9 hardware, Android device, or QEMU with MTE; TARGET-ONLY
here, not run on this host):

```
qemu-aarch64 -cpu max ./prctl_mte              # "MTE enabled (SYNC)"
grep -E '^Features.*\bmte\b' /proc/cpuinfo     # 'mte' present
aarch64-linux-gnu-gcc -mcpu=armv9-a -march=armv9-a+memtag \
    examples/good/prctl_mte.c -o /tmp/mte
```

The C snippets in `examples/` are annotated target sketches (NOT compiled here:
this host is x86 MinGW and cannot assemble AArch64 or run MTE).

## Where the knowledge comes from

- Arm Memory Tagging Extension — Arm Developer (https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Arm_Memory_Tagging_Extension_Whitepaper.pdf)
- Arm Architecture Reference Manual (Armv9, FEAT_MTE)
- Linux kernel memory-tagging-extension docs (https://docs.kernel.org/arch/arm64/memory-tagging-extension.html)
- Android 13+ MTE docs (https://source.android.com/docs/security/test/memory-safety)
- AArch64 mte instructions (https://developer.arm.com/documentation/ddi0596/2024-12/Base-Instructions)

## Related skills

- `embedded-mpu-trustzone` — adjacent low-level memory-attribution feature on
  Arm; both make memory access behavior depend on per-region attributes.
- `embedded-volatile-and-memory-ordering` — once tagging and TBI are in play,
  keep MMIO/volatile rules and access ordering separate from tag semantics.
- `page-table-management` — granule/translation mechanics MTE rides on
  (16-byte granules, TBI for translation, excluded memory classes).
- `memory-ordering-reasoning` — happens-before reasoning applies when tag
  faults interact with concurrent access; MTE does not order anything.
- `embedded-linker-script` — stack/heap placement and alignment interact with
  16-byte granule alignment for tagging.
- `c-undefined-behavior` — tag faults are a hardware signal, not UB; pairing
  the two is how allocator memory-safety patches are reviewed.

## Evaluation

Synthetic: given an allocator/free path in C, decide whether MTE would catch a
specific bug (use-after-free yes; intra-granule overflow no; unaligned
allocation yes/no by granule ownership). Adversarial: a patch that tags a
`malloc`-returned pointer without checking alignment, or that claims an ASYNC
`si_addr` is the bug site, or that frees without poisoning — each must be
flagged. False-positive: an aligned, tagged, poisoned allocator and a SYNC
handler reading `si_addr` must pass clean. Historical: Android heap-overflow
mitigations (Scudo + MTE on Pixel 8) and Chromium's Android MTE rollout show
production ASYNC use and its crash-report caveats. See `evals/README.md`.

Stability: `researched` — the Python tag model runs and passes on this host
(recorded output in `evals/README.md`); the C sketches and instruction
behavior require an Armv9 target (QEMU or device) to execute.
