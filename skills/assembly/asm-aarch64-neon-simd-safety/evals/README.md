# Evaluation — asm-aarch64-neon-simd-safety

Skill: `skills/assembly/asm-aarch64-neon-simd-safety`.
Stability target: `researched`. Toolchain status: `clang`, `llvm-objdump`,
`qemu-aarch64` are NOT installed on this host (MSYS2 ucrt64: gcc 16.1.0/as/
objdump target x86_64-w64-mingw32 only). **No A64 command was run.** All
cases are documented with the exact verification command; "expected" labels
are predictions from `arm-arm`/`arm-sve-acle`, marked UNVERIFIED until run on
an A64 toolchain host.

## Synthetic evals

| Case | Fixture | Expected (UNVERIFIED) | Verification command |
|---|---|---|---|
| easy/negative | `bad/neon_counter_overflow.c` | wraps silently for large n | `clang --target=aarch64-none-elf -march=armv8-a+simd -c` |
| easy/negative | `bad/neon_counter_overflow.s` | byte-lane wrap at 256 | `clang --target=aarch64-none-elf -march=armv8-a+simd -c` |
| medium/negative | `bad/mla_saturating.s` | plain mla wraps; not saturating | `clang --target=aarch64-none-elf -march=armv8-a+simd -c` |
| easy/positive | `good/neon_counter_overflow.c` | guard+reduce pattern | `clang --target=aarch64-none-elf -march=armv8-a+simd -c` |
| easy/positive | `good/neon_counter_overflow.s` | 64-bit lanes, no wrap | `clang --target=aarch64-none-elf -march=armv8-a+simd -c` |
| easy/positive | `good/mla_saturating.s` | sqdmull saturates | `clang --target=aarch64-none-elf -march=armv8-a+simd -c` |

## False-positive evals (correct code must not be flagged)

- `good/neon_counter_overflow.c` — the horizontal-reduce guard every 65536
  iterations is the RECOMMENDED pattern (Lemire); it must not be called
  "inefficient" or "unnecessary".
- `good/neon_counter_overflow.s` — 64-bit `.2d` lanes for a counter are
  correct.
- `good/mla_saturating.s` — `sqdmull` (saturating) chosen for an algorithm
  that requires saturation: correct.
- A deliberate `vaddq_u32` on documented uint32 data must NOT be flagged as a
  "signedness error".

## Historical evals (real incident)

- **Lemire NEON counter overflow (2026, ASM-18)** — SIMD loops with per-lane
  counters missing an overflow guard wrap every ~255 iterations; the guarded
  fix cut the hot loop from 1200 to 154 instructions (8x). Reproduced as
  `bad/neon_counter_overflow.s` (byte lanes) and
  `bad/neon_counter_overflow.c` (word lanes, large n).

## Adversarial evals

- `bad/neon_counter_overflow.c` — compiles and runs cleanly, and for small `n`
  returns correct results; only at large `n` do lanes wrap silently. Classic
  "passes the build, wrong at scale" case: the reviewer must reason about lane
  width and iteration count, not just run the happy path.
- `bad/mla_saturating.s` — correct-looking multiply that wraps where the
  algorithm (bitmask arithmetic) expects saturation.

## Verified facts

None — researched skill. No commands executed. This is stated honestly: the
host lacks an AArch64 toolchain and emulator, so lane-wrap behavior and
saturating encodings are KNOWN from `arm-arm`/`arm-sve-acle` but their
byte-level/run-level consequences are UNVERIFIED here. The verification
commands above are complete and ready to run on any A64-capable host.

## Scoring (for routing eval)

- precision: each flagged case maps to a named reference rule (1-5).
- recall: all bad snippets detected by review (lane wrap, saturation mismatch,
  type mismatch, tail loss, unmeasured claims).
- FP-rate: good snippets produce zero flags.
- confidence: every claim marked UNVERIFIED until the A64 run records actual
  exit codes and edge-input results.

## Target toolchains (absent, documented)

- `clang --target=aarch64-none-elf -march=armv8-a+simd -c`: NEON verification
  command for all cases; `-march=armv8-a+sve` for SVE.
- `llvm-objdump -d`: byte-level check of lane widths and `sq*` forms.
- `qemu-aarch64`: runtime edge-input test on a host that has it.
