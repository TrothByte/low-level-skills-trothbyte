---
name: asm-arm-thumb-2-encoding
description: Use when writing or reviewing Thumb-2 assembly for Cortex-M and other Arm A/R-profile cores. Covers CBZ/CBNZ r0-r7 constraint and range, 16/32-bit mixed encoding, IT blocks for conditional execution, and A32-vs-Thumb differences. Prevents invalid hi-register branches, far-range branch bugs, and hand-encoded byte corruption.
---

# Arm Thumb-2 Encoding

## When to use

- Writing, reviewing, or porting Thumb-2 assembly for Cortex-M (M0/M3/M4/M7),
  or mixed A-profile code with `.thumb`.
- Fixing a branch that "should work" but targets a hi-register (`cbz r9`) or
  a far label (>~126 bytes).
- Checking that conditionally executed instructions sit inside IT blocks.
- Verifying that hand-written bytes match the assembler's 16/32-bit choice.

## When not to use

- AArch64 (A64) — different encoding, registers, and conditional scheme.
- Cortex-M register/file layout or startup code — use `embedded-linker-script`
  or `embedded-mpu-trustzone`.
- Verifying that an instruction exists at all — use
  `asm-verification-hallucination-gate`.
- NEON/SIMD on AArch64 — use `asm-aarch64-neon-simd-safety`.

## What the agent often gets wrong

- Uses `cbz r9, target` — Thumb-2 CBZ/CBNZ only encode r0-r7 (HerraduraKEx
  PR#33 merged with exactly this bug).
- Forgets CBZ's ~+/-126-byte range and branches to far labels, assuming a
  32-bit literal.
- Emits `moveq`/`movne` outside an IT block, expecting A32-style implicit
  conditional execution.
- Treats Thumb-2 as uniform 32-bit (A32) encoding and hand-patches bytes that
  desynchronize the 16/32-bit stream.
- Forgets Cortex-M is Thumb-only; A32 instructions silently fail or are
  rejected.

## How to reason correctly

1. Confirm execution state first: `.thumb` (Thumb-2) vs `.arm` (A32); Cortex-M
   is Thumb-only.
2. For each CBZ/CBNZ: register in r0-r7? target within ~+/-126 bytes? If not,
   rewrite as `cmp` + `beq`/`bne` or invert condition + `b.w`.
3. Every conditionally executed (non-branch) instruction: is it inside its IT
   block with the right letter count (`it`, `itt`, `ite`)?
4. Width: let the assembler pick 16/32-bit; force with `w`/`n` suffixes only
   when needed. Never hand-encode without disassembling the result.
5. Verify with `objdump`/`llvm-objdump`: halfword counts and displacements.

## What to verify

- File assembles with the Thumb target (researched: run
  `clang --target=armv7m-none-eabi -mthumb -c`).
- All CBZ/CBNZ use r0-r7 and in-range targets; hi-register zero-tests are
  `cmp`+`beq`.
- Every conditional instruction has its IT block; IT letter count matches.
- Disassembly shows intended 16/32-bit mixes; no desync from hand bytes.
- Cortex-M build contains no A32 encodings.

## How to verify

```
# Researched — toolchain (clang/llvm-mc) NOT installed on this host.
# Verification command for each example:
clang --target=armv7m-none-eabi -mthumb -c examples/bad/cbz_hi_reg.s
  expected exit 1: operand r9 out of range / invalid register for cbz
clang --target=armv7m-none-eabi -mthumb -c examples/good/cbz_low_reg.s
  expected exit 0
clang --target=armv7m-none-eabi -mthumb -c examples/bad/it_block.s
  expected exit 1 (or mis-encoded unconditional) — misplaced conditional
clang --target=armv7m-none-eabi -mthumb -c examples/good/it_block.s
  expected exit 0
llvm-objdump -d example.o   # check 16/32-bit halfwords and displacements
```

## Where the knowledge comes from

- `arm-arm` — A-profile Architecture Reference Manual: Thumb instruction set
  encoding, CBZ/CBNZ T1 (Rt range, imm6), IT instruction, M-profile
  execution state.
- `arm-abi-aa` — AAPCS: Thumb function-pointer bit 0, interworking.
- `asm-verification-hallucination-gate` — the general mnemonic/encoding gate.
- Failure survey `research/2026-08-15-asm-agent-failures-survey.md` (ASM-8:
  HerraduraKEx PR#33 cbz on hi-registers).

## Related skills

- `asm-verification-hallucination-gate` — verify each mnemonic exists.
- `asm-aarch64-neon-simd-safety` — A64 counterpart (different encoding).
- `embedded-linker-script` — vector tables, startup for Cortex-M.
- `asm-calling-conventions` — AAPCS register roles.

## Evaluation

- Synthetic: bad Thumb-2 (cbz hi-reg, cbz far range, bare conditional, A32
  leak) must be flagged; good Thumb-2 (cbz r0-r7, IT blocks, wide branches)
  must pass.
- False-positive: correct `it`+`moveq`, `cmp r9,#0`+`beq`, `b.w` for far
  targets must NOT be flagged.
- Historical: HerraduraKEx PR#33 reproduced as `bad/cbz_hi_reg.s`.
- Adversarial: `bad/cbz_range.s` would assemble with a linker-displaced target
  or wrong behavior despite "looking correct"; `bad/it_block.s` runs
  unconditionally.
- Commands: `evals/README.md`. All Thumb-2 cases are researched (toolchain
  absent); no run claims are made.
