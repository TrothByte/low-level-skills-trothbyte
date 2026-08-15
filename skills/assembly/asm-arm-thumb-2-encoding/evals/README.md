# Evaluation — asm-arm-thumb-2-encoding

Skill: `skills/assembly/asm-arm-thumb-2-encoding`.
Stability target: `researched`. Toolchain status: `clang`, `llvm-mc`,
`llvm-objdump`, `arm-none-eabi-gcc`, `qemu-arm` are NOT installed on this host
(MSYS2 ucrt64: gcc 16.1.0/as/objdump target x86_64-w64-mingw32 only).
**No Thumb-2 command was run.** All cases below are documented with the exact
verification command; every "expected" label is a prediction from `arm-arm`,
marked UNVERIFIED until executed on an Arm toolchain host.

## Synthetic evals

| Case | Fixture | Expected (UNVERIFIED) | Verification command |
|---|---|---|---|
| easy/negative | `bad/cbz_hi_reg.s` | assembler rejects cbz r9/r10 | `clang --target=armv7m-none-eabi -mthumb -c bad/cbz_hi_reg.s` |
| easy/negative | `bad/it_block.s` | bare conditional rejected/mis-encoded | `clang --target=armv7m-none-eabi -mthumb -c bad/it_block.s` |
| medium/negative | `bad/cbz_range.s` | far-target cbz fails range | `clang --target=armv7m-none-eabi -mthumb -c bad/cbz_range.s` |
| medium/negative | `bad/a32_vs_thumb.s` | A32 leak flagged | `clang --target=armv7m-none-eabi -mthumb -c bad/a32_vs_thumb.s` |
| easy/positive | `good/cbz_low_reg.s` | exit 0 | `clang --target=armv7m-none-eabi -mthumb -c good/cbz_low_reg.s` |
| easy/positive | `good/it_block.s` | exit 0 | `clang --target=armv7m-none-eabi -mthumb -c good/it_block.s` |
| easy/positive | `good/branch_ranges.s` | exit 0 | `clang --target=armv7m-none-eabi -mthumb -c good/branch_ranges.s` |
| easy/positive | `good/mixed_width.s` | exit 0 | `clang --target=armv7m-none-eabi -mthumb -c good/mixed_width.s` |

## False-positive evals (correct code must not be flagged)

- `good/cbz_low_reg.s` — `cbz r3`/`cbnz r7` are valid; the `cmp r9,#0`+`beq`
  idiom for hi-registers is the recommended pattern, not an error.
- `good/it_block.s` — `iteq`/`itte` blocks with matching letter counts are
  correct.
- `good/branch_ranges.s` — near `cbz`, explicit wide `b.w` for far targets:
  correct.
- `good/mixed_width.s` — normal 16/32-bit mix; must NOT be flagged as
  "inconsistent widths".

## Historical evals (real incident)

- **HerraduraKEx PR#33** — Thumb-2 `cbz r9/r10` on hi-registers, invalid
  because CBZ/CBNZ encode Rt in bits 7:3 (r0-r7 only). Reproduced as
  `bad/cbz_hi_reg.s`. This exact bug shipped in a merged PR, making it the
  primary historical anchor for this skill.

## Adversarial evals

- `bad/cbz_range.s` — target >126 bytes away. Even if the assembler's linker
  inserts a veneer or the range check fires late, the "looks correct" form
  (cbz + far label) either fails to assemble as intended or silently depends
  on linker machinery the author did not plan for.
- `bad/it_block.s` — assembles (or encodes unconditionally) while the author
  expected conditional execution; runtime behavior differs from intent without
  any build error.

## Verified facts

None — researched skill. No commands executed. This is stated honestly: the
host has no Arm toolchain, so `cbz` encodings, IT-block rules, and ranges are
KNOWN from `arm-arm` but their byte-level consequences are UNVERIFIED here.
The verification commands above are complete and ready to run on any host with
`clang`/`llvm-objdump`.

## Scoring (for routing eval)

- precision: each flagged case maps to a named reference rule (1-6).
- recall: all bad snippets detected by review (register range, branch range,
  IT placement, A32 leakage).
- FP-rate: good snippets produce zero flags.
- confidence: every claim marked UNVERIFIED until the Arm toolchain run
  records actual exit codes.

## Target toolchains (absent, documented)

- `clang --target=armv7m-none-eabi -mthumb`: verification command for all
  cases; to be run on an LLVM-equipped host (Linux/macOS/Windows with clang).
- `llvm-objdump -d`: byte-level check of 16/32-bit encodings and
  displacements.
