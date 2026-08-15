# AArch64 NEON/SIMD Safety — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to
registry/sources.yaml.

## 1. SIMD lanes overflow independently; guards are per-lane

- **RULE**: a NEON vector is parallel lanes, not one wide number. `vaddq_u32`
  adds four independent 32-bit values; any lane can overflow/wrap while the
  others are fine. There is no cross-lane carry and no automatic widening.
- **WHY AI GETS IT WRONG**: models treat the vector as a big scalar accumulator
  and forget lane width. Lemire's documented failure: SIMD loops with missing
  per-lane overflow guards wrap every ~255 iterations (byte lanes) or 2^32
  (word lanes); the fix reduced his hot loop from 1200 to 154 instructions.
- **CORRECT REASONING**: name the lane width (8/16/32/64), the element count
  (8/4/2/1), and the wrap point per lane. If iterations*step can reach the lane
  width, insert a horizontal reduce (`vaddvq`, `vst1q`+scalar add) into a wider
  accumulator or guard every 2^(lane_bits)/step iterations.
- **EXAMPLE** (bad): 16-byte counters `add v0.16b,v0.16b,v1.16b` — wrap at 256.
- **COUNTEREXAMPLE** (good): 64-bit lanes `add v0.2d,v0.2d,v1.2d`, or scalar
  guard reducing every 65536 iterations.
- **VERIFICATION**: `clang --target=aarch64-none-elf -march=armv8-a+simd -c`
  for both; runtime edge-input test on a host with an A64 CPU/emulator
  (researched — toolchain absent here).
- **SOURCE**: arm-arm (NEON instructions, lane semantics); arxiv-2511-01183
  (SIMD-loop correctness context); Lemire case in
  research/2026-08-15-asm-agent-failures-survey.md (ASM-18).

## 2. Saturation is explicit: mul/mla wrap, sq* saturate

- **RULE**: plain `mul`/`mla`/`add` wrap on overflow (two's-complement).
  Saturating forms have a prefix: `sq` (signed saturating) / `uq` (unsigned
  saturating), e.g. `sqdmull`, `uqrshl`. The choice of form changes results
  silently on overflow.
- **WHY AI GETS IT WRONG**: "bitmask arithmetic" failures in the survey came
  from models using plain arithmetic where saturating (or vice versa) was
  required; the two look identical until an overflow value reaches the code.
- **CORRECT REASONING**: decide overflow semantics from the algorithm (audio,
  fixed-point, color math usually saturate; hashing, checksums wrap). Then
  pick the exact mnemonic family; verify with edge values.
- **EXAMPLE** (bad): `mla v0.4s,v1.4s,v1.4s` with 0x7fffffff inputs — wraps.
- **COUNTEREXAMPLE** (good): `sqdmull v0.2d,v1.2s,v1.2s` — saturates.
- **VERIFICATION**: `clang --target=aarch64-none-elf -march=armv8-a+simd -c`;
  runtime edge test (researched).
- **SOURCE**: arm-arm (NEON multiply/accumulate forms, sq/uq prefixes);
  arm-sve-acle (SVE saturating variants); survey ASM-18/ASM-19.

## 3. Intrinsics and asm must agree on element type

- **RULE**: NEON intrinsics are strongly typed (`uint32x4_t` vs `uint16x8_t`
  vs `float32x4_t`); an asm file must match the C-level element type exactly or
  lane interpretation silently changes. The intrinsic set for A64 is
  `arm_neon.h`; SVE uses `arm_sve.h`.
- **WHY AI GETS IT WRONG**: models swap `vaddq_u32`/`vaddq_s32`/`vaddq_f32`
  or mix intrinsic names across architectures (x86 `_mm_add_epi32` habits).
- **CORRECT REASONING**: the type suffix names signedness and width; changing
  it changes wrap behavior, not just the name. Verify with the exact intrinsic
  signature against the Arm intrinsic reference.
- **EXAMPLE** (bad): `vaddq_u32(acc, v)` where `v` is actually `int32x4_t`.
- **COUNTEREXAMPLE** (good): `vaddq_s32` for signed lanes.
- **VERIFICATION**: `clang --target=aarch64-none-elf -march=armv8-a+simd -c`
  with `-Wall -Wextra` (researched).
- **SOURCE**: arm-arm (NEON instruction data types); arm-sve-acle (SVE
  intrinsics); arm-abi-aa (vector ABI).

## 4. Tail handling: the loop bound must be element-count aware

- **RULE**: a 4-element vector processes 4 items per iteration; the loop bound
  must be `n - n%4` (or use masked loads/SVE), and the remainder handled by a
  scalar tail. Processing past `n` reads out of bounds; stopping before `n`
  silently drops the tail.
- **WHY AI GETS IT WRONG**: scalar-loop habit — `for (i=0; i<n; i+=4)` without
  checking that `n` is a multiple of 4, or without a tail.
- **CORRECT REASONING**: always compute `vector_end = n & ~(LANES-1)`, run the
  SIMD loop to `vector_end`, then a scalar loop for `n - vector_end`; or use
  SVE predication (`whilelt`) which handles tails natively.
- **EXAMPLE** (bad): `for (i=0; i<n; i+=4) vld1q_u32(src+i)` with n not
  multiple of 4 — over-read.
- **COUNTEREXAMPLE** (good): main loop to `n&~3` + scalar tail, or SVE
  `whilelt p0.s, x1, x2`.
- **VERIFICATION**: `clang --target=aarch64-none-elf` with runtime edge test
  (researched).
- **SOURCE**: arm-arm (NEON load/store); arm-sve-acle (predication, `whilelt`).

## 5. Optimization claims need the old and new instruction counts

- **RULE**: any "SIMD is faster" claim must compare instruction counts or
  cycles before/after (Lemire's 1200→154 is the canonical evidence style).
  A "faster" rewrite that wraps lanes or drops the tail is not an optimization.
- **WHY AI GETS IT WRONG**: models assert speedup from structure alone,
  without measuring; correctness regressions hide inside "optimized" loops.
- **CORRECT REASONING**: keep the scalar baseline; measure both; verify
  functional equivalence on edge inputs (0, max values, non-multiple-of-lanes).
- **EXAMPLE** (bad): claiming 8x speedup for a loop that now wraps counters.
- **COUNTEREXAMPLE** (good): measured 1200→154 instructions with correct
  results on edge inputs.
- **VERIFICATION**: perf counters/`-O3` objdump before and after (researched).
- **SOURCE**: arxiv-2511-01183 (IR→asm correctness context); survey ASM-18.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Lanes | each lane overflows independently; no cross-lane carry |
| Guards | reduce into wider accumulator every 2^(lane_bits)/step iterations |
| Saturation | mul/mla wrap; sq*/uq* saturate; choose per algorithm |
| Types | intrinsic suffix = signedness+width; match asm to C exactly |
| Tails | bound = n&~3 + scalar tail, or SVE whilelt predication |
| Claims | measure instructions before/after (1200→154 pattern) |
