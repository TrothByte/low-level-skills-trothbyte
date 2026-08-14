---
name: meta-assumptions
description: Use when code correctness depends on implicit assumptions — compiler, ABI, platform, memory model, optimization level, or endianness. Forces surfacing and documenting every assumption before concluding.
---

# Meta: Surface Assumptions

## When to use

- Writing FFI/layout-dependent code, inline asm, atomics, or anything platform-sensitive.
- Reviewing code that "worked" somewhere.
- Diagnosing cross-compiler or cross-platform failures.

## What the agent often gets wrong

- Assumes "this ABI is probably the same" (B14) — SysV vs Windows x64 vs AAPCS64 differ.
- Assumes "the instruction should exist" without checking the ISA (B2).
- Assumes the compiler "won't do that" (AD-09) — UB license says otherwise.
- Assumes endianness/padding/`sizeof` are "obvious".
- Assumes optimization level doesn't change semantics.

## How to reason correctly

1. Enumerate assumptions explicitly before writing/reviewing: target ABI, ISA, compiler,
   optimization flags, endianness, memory model, `int`/`size_t` widths.
2. Classify each: platform-independent / platform-specific / undefined.
3. For platform-specific ones, pick ONE canonical reference (ABI doc) and verify.
4. If an assumption can't be verified, mark UNVERIFIED and refuse to bet correctness on it.

## Common assumption traps

| Assumption | Truth |
|---|---|
| "ABI is the same everywhere" | SysV (6 regs) vs Win64 (4 regs) vs AAPCS64 (x0-x7 + x8 sret) |
| "`int` is 32-bit, pointers 64-bit" | ILP32/LLP64/LP64 differ by platform |
| "little-endian everywhere" | ARM/RISC-V/MIPS can be big-endian (firmware!) |
| "`volatile` syncs threads" | no — it's not an atomic (A11) |
| "relaxed ordering is fine" | no sync edge → data race (AD-01) |
| "compiler keeps my checks" | UB → checks deleted (AD-03) |

## What to verify

- Each assumption is written down in the task context (visible to the reviewer).
- Every verified-vs-assumed distinction is explicit.
- No correctness bet rests on an unverified assumption.

## When not to use

- When assumptions are already pinned by an explicit spec/ABI doc in scope — still list them, but no extra ceremony.
- Pure portability-free throwaway code — note assumptions once in the task, don't repeat.

## How to verify

- For each platform-specific assumption, test on the actual target (compile+run) or cite the
  authoritative doc (ABI section, ISA spec).
- Cross-compile or use Godbolt for a second target when the assumption is load-bearing.

## Where the knowledge comes from

- `registry/cross-links.yaml` (collisions), `registry/sources.yaml` (per-domain authorities),
  per-ABI references in `skills/abi/abi-layout-reasoning/references/`.

## Related skills

- `abi-layout-reasoning` — the ABI-specific assumption table.
- `meta-evidence` — assumptions must be labeled KNOWN/INFERRED/UNVERIFIED.

## Evaluation

- Adversarial AD-05/AD-06: agent must surface the ABI/layout assumption and mark it
  UNVERIFIED until checked, rather than betting correctness on it.
