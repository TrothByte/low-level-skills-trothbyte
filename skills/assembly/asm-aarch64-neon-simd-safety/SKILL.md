---
name: asm-aarch64-neon-simd-safety
description: Use when writing or reviewing AArch64 NEON/SIMD loops — vector intrinsics or assembly. Covers per-lane overflow, saturation vs wrap semantics, element types, and tail handling. Prevents the documented SIMD failure where counters overflow every ~255 iterations because no horizontal-reduce guard exists.
---

# AArch64 NEON/SIMD Safety

## When to use

- Writing or reviewing AArch64 NEON (`arm_neon.h`) or SVE (`arm_sve.h`) loops:
  vectorized sums, dot products, counters, fixed-point math.
- Diagnosing wrong results in "optimized" SIMD code that wraps lanes, drops
  tails, or saturates where it should wrap (and vice versa).
- Checking that a vector loop is functionally equivalent to its scalar
  baseline on edge inputs.

## When not to use

- Arm Thumb-2 / Cortex-M assembly — use `asm-arm-thumb-2-encoding`.
- x86 SSE/AVX equivalents — different lane widths, flags, and intrinsics.
- Performance measurement discipline generally — use
  `performance-measurement-discipline`.
- Vectorization strategy at the C level — use `simd-vectorization-cross-layer`
  or `vectorization-reasoning`.

## What the agent often gets wrong

- Assumes a NEON vector is one big number: no per-lane overflow guard. Lanes
  wrap at 256 (byte), 65536 (halfword), 2^32 (word); the documented failure
  wraps counters every ~255 iterations (Lemire; ASM-18 in the survey).
- Uses plain `mla`/`mul`/`add` where the algorithm needs saturating `sq*`/
  `uq*` forms — silent bitmask/saturation mismatch.
- Swaps element types (`vaddq_u32` vs `vaddq_s32` vs `vaddq_f32`), changing
  signedness and wrap behavior without an obvious error.
- Handles only multiples of the lane count: `for (i=0;i<n;i+=4)` with no
  scalar tail → over-read or dropped tail.
- Claims speedup without measuring instruction count before/after.

## How to reason correctly

1. Name lane width and element count for every vector: `v0.16b` = 16×8-bit,
   `v0.4s` = 4×32-bit, `v0.2d` = 2×64-bit.
2. Compute the wrap point per lane: 2^(lane_bits). If iterations×step can
   reach it, add a horizontal reduce (`vaddvq_u32`, or store+scalar add) into
   a wider accumulator, or keep counters in 64-bit lanes.
3. Decide overflow semantics from the algorithm: saturate (audio/fixed-point/
   color) vs wrap (hash/checksum); pick the exact mnemonic/intrinsic family
   and verify with edge values (0x7fffffff, -1).
4. Match intrinsic signedness/width to the C types exactly.
5. Bound the SIMD loop at `n & ~(LANES-1)` and add a scalar tail — or use SVE
   predication (`whilelt`).
6. Compare instruction counts with the scalar baseline before claiming
   speedup (Lemire's 1200→154 pattern).

## What to verify

- File compiles/assembles with the A64 toolchain (researched:
  `clang --target=aarch64-none-elf -march=armv8-a+simd -c` for NEON;
  `-march=armv8-a+sve` for SVE).
- Lane widths and wrap points are correct; guards or 64-bit lanes present
  where iteration count can exceed lane width.
- Saturation vs wrap matches the algorithm; edge inputs tested.
- Loop bound respects lane count; tails handled.
- No hand-written bytes without disassembling (`llvm-objdump -d`).

## How to verify

```
# Researched — clang/qemu NOT installed on this host. Verification commands:
clang --target=aarch64-none-elf -march=armv8-a+simd -c examples/good/neon_counter_overflow.c
clang --target=aarch64-none-elf -march=armv8-a+simd -c examples/bad/neon_counter_overflow.c
clang --target=aarch64-none-elf -march=armv8-a+simd -c examples/good/neon_counter_overflow.s
llvm-objdump -d examples/good/neon_counter_overflow.o   # inspect lane widths
```

Runtime edge test on an A64 host: feed `n` not a multiple of 4 and lane-max
values; compare with the scalar reference.

## Where the knowledge comes from

- `arm-arm` — A-profile manual: NEON instruction set, lane semantics,
  saturating `sq`/`uq` forms, vector data types.
- `arm-sve-acle` — SVE intrinsics (`whilelt`, predication, saturating forms).
- `arm-abi-aa` — vector ABI, register allocation for SIMD.
- Calibration: `arxiv-2511-01183` (IR→asm correctness 36% aarch64); Lemire
  case (1200→154 instructions) in
  `research/2026-08-15-asm-agent-failures-survey.md` (ASM-18).

## Related skills

- `asm-verification-hallucination-gate` — verify SIMD mnemonics exist/encode.
- `asm-arm-thumb-2-encoding` — other Arm ISA, no NEON.
- `simd-vectorization-cross-layer` / `vectorization-reasoning` — C-level
  vectorization strategy.
- `performance-measurement-discipline` — measurement before optimization.

## Evaluation

- Synthetic: bad NEON (per-lane overflow, wrap-vs-saturate, wrong types,
  missing tail) must be flagged; good NEON (64-bit lanes, guard+reduce,
  explicit saturating forms, scalar tail) must pass.
- False-positive: `vaddq_u32` on unsigned data with a guard, `sqdmull` for
  saturating multiply, SVE `whilelt` tail handling must NOT be flagged.
- Adversarial: `bad/neon_counter_overflow.c` compiles and runs but wraps
  silently for large `n`; the code "looks correct" while results are wrong —
  the Lemire failure class.
- Commands: `evals/README.md`. All cases are researched (A64 toolchain
  absent); no run claims are made.
