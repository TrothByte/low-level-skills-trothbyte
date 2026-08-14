# Evaluation — asm-x86-64-registers-and-addressing

Skill: `skills/assembly/asm-x86-64-registers-and-addressing`.
Stability target: `evaluated`. Toolchain: GCC 16.1.0 / GNU as 2.46 / objdump 2.46
(MSYS2 MinGW, target x86_64-w64-mingw32, PE/COFF). `nasm` and `llvm-mc` are NOT
installed — documented as target toolchains (Intel-syntax checks run there later).

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/sizes.s` | assembler rejects size mismatch | exit 1 |
| easy/negative | `bad/addressing.s` | assembler rejects scale 3 and `%rsp` index | exit 1 |
| medium/negative | `bad/sizes_truncate.s` | silent truncation caught by review | exit 0, must be flagged |
| medium/negative | `bad/flags.s` | stale-flag `lea`+`jz`, `jb` on signed data | exit 0, must be flagged |
| medium/negative | `bad/sign_extend.s`, `bad/sign_extend2.s` | wrong extension width | exit 0, must be flagged |
| hard/negative | `bad/imm.s` | hand-encoded C7 imm32 sign-extension | exit 0, byte-level review |
| hard/negative | `bad/rex.s` | hand-encoded bytes missing REX.B | exit 0, byte-level review |
| adversarial | `bad/canonical.s` | assembles cleanly, faults (#GP) at runtime | exit 0, runtime fault |

Detection rule for the silent cases: the reviewer must decide the *intended* width
from the API contract, not from the mnemonic, and reject any load/store that does
not carry the full value or the right extension.

## False-positive evals (correct code must not be flagged)

- `good/sizes.s` — suffix matches width; deliberate 32-bit load with documented
  zero-extension.
- `good/flags.s` — `cmp`+`jl` (signed) and `cmp`+`jb` (unsigned): correct per type.
- `good/addressing.s` — base+index*8, negative disp32, RIP-relative: correct.
- `good/sign_extend.s` — `movslq`/`movsbq`/`movzbl` matched to intended widths.
- `good/imm.s` — `movq $-1`, `movl $0xFFFFFFFF`, `movabsq`: correct encodings.
- `good/rex.s` — r8-r15 with assembler-generated REX: correct.
- `good/canonical.s` — caller pointer and `lea`-resolved addresses: correct.
- `good/inline_asm.c` — `movslq` and deliberate 32-bit load with `%k0`: correct.
- `movl (%rdi),%eax` returning the low 32 bits of a documented `uint32_t` field is
  correct and must NOT be flagged as "truncation".

## Verification commands (ACTUAL, recorded 2026-08-14)

```
gcc -c examples/bad/sizes.s
  exit 1: "Error: incorrect register `%ax' used with `l' suffix"

gcc -c examples/bad/addressing.s
  exit 1: "Error: expecting scale factor of 1, 2, 4, or 8: got `3'"
          "Error: `(%rax,%rsp,2)' is not a valid base/index expression"

gcc -c examples/bad/{sizes_truncate,flags,canonical,imm,rex,sign_extend,sign_extend2}.s
  exit 0 each — silent cases, caught by review/runtime, not by the assembler

gcc -c examples/good/{sizes,flags,addressing,canonical,imm,rex,sign_extend}.s
  exit 0 each

gcc -Wall -Wextra -Werror -O2 -c examples/bad/inline_asm.c   exit 0
gcc -Wall -Wextra -Werror -O2 -c examples/good/inline_asm.c  exit 0

objdump -d on good objects confirms the intended encodings:
  good/addressing: 48 8b 04 f7 (base+idx*8) | 48 8b 5f f8 (disp -8) |
                   48 8b 0d ... with IMAGE_REL_AMD64_REL32 (RIP-relative,
                   PE form of R_X86_64_PC32) | 48 8d 15 ... (lea RIP-relative)
  good/imm:        48 c7 c0 ff ff ff ff = mov $0xffffffffffffffff,%rax (-1)
                   b8 ff ff ff ff       = mov $0xffffffff,%eax (zero-extends)
                   48 b8 ff ff ff ff 00 00 00 00 = movabs (exact 64-bit)
  good/rex:        4d 89 c8 (mov %r9,%r8, REX.W+R+B) | 41 b8 01 00 00 00 |
                   44 0f b6 07 (movzbl (%rdi),%r8d)
  good/sign_extend: 48 63 07 (movslq) | 48 0f be 07 (movsbq) | 0f b6 07 (movzbl)
  bad/rex:          8b 00 = mov (%rax),%eax  (missing 41 REX.B)
  bad/imm:          48 c7 c0 ff ff ff ff = mov $0xffffffffffffffff,%rax
  bad/canonical:    movabs $0x800000000000,%rax + mov (%rax),%rcx
```

## Verified facts (runtime tests, all exit 0)

- `jl` on `cmp %rsi,%rdi` with rdi=-1, rsi=1 branches (signed -1 < 1); `jb` on the
  same flags does NOT branch (unsigned 0xFF..F > 1) — confirms rule 9.
- `movl $0,%eax` after loading rax clears the upper 32 bits — confirms rule 2.
- `movl $0xFFFFFFFF,%eax` yields rax == 0x00000000FFFFFFFF; `movq $-1,%rax`
  (C7 imm32 form) yields rax == all ones — confirms rules 2 and 10.
- `cmpq $0xFFFFFFFF,%rax` does NOT assemble ("operand type mismatch") because the
  compact imm32 is sign-extended — direct evidence for rule 10.
- Inline asm on MinGW: `long` is 32-bit (LLP64), so a `long` output allocates `%eax`;
  use `long long` for 64-bit operands and `%k0` to print a 32-bit register name.

## Scoring (for routing eval)

- precision: every flagged case maps to a named reference rule (1-10).
- recall: all bad snippets detected (assembler errors, review-time width/flag/extension
  violations, byte-level encoding review, runtime fault).
- FP-rate: good snippets produce zero flags.

## Target toolchains (absent, documented)

- `nasm` / `llvm-mc`: not installed. Intel-syntax variants of the same examples
  (`mov rax, [rbx+rcx*4]`, `movzx/movsx`, REX in machine bytes) are the planned
  second pass; the rules in references/ are syntax-independent.
