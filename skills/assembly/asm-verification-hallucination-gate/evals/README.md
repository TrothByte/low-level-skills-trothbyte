# Evaluation — asm-verification-hallucination-gate

Skill: `skills/assembly/asm-verification-hallucination-gate`.
Stability target: `evaluated`. Toolchain: GCC 16.1.0 / GNU as 2.46 / objdump
2.46 / ld (MSYS2 MinGW ucrt64, target x86_64-w64-mingw32, PE/COFF). `nasm`,
`clang`, `llvm-mc`, `qemu` are NOT installed — the gate procedure is identical
on other targets, but only the GAS/AT&T x86-64 runs were recorded here.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/invented_mnemonic.s` | assembler rejects invented mnemonic | exit 1, "no such instruction: `movqad'" |
| easy/negative | `bad/cdc_compass_pseudo.s` | CDC pseudo-ops rejected | exit 1, `jo`/`sst`/`oct` errors |
| easy/negative | `bad/att_inverted.s` | AT&T reversal rejected | exit 1, "operand type mismatch for `mov'" |
| medium/negative | `bad/att_silent_swap.s` | silent src/dest swap caught by review | exit 0, must be flagged |
| medium/negative | `bad/ax_is_8bit.s` | AX-width claim flagged | exit 0, must be flagged |
| medium/negative | `bad/esp_offset.s` | wrong stack slot flagged | exit 0, must be flagged |
| hard/negative | `bad/imul_nulled_imm.s` | nulled immediate by objdump | exit 0, byte-level review |
| hard/negative | `bad/byte_blind.s` | REX-less byte claim flagged | exit 0, byte-level review |

## False-positive evals (correct code must not be flagged)

- `good/gate_pass.s` — valid AT&T (source-first `movl %ebx,%eax`), correct `$`
  immediate (`movl $0,-4(%rbp)`), correct widths, correct stack slot
  `movl (%rsp),%eax`, correct IMUL imm8 encoding `imull $38,%eax,%eax`.
- `good/roundtrip.s` — full assemble→disassemble round-trip: every byte shown
  in objdump matches the comment; this is the reference for "correct".
- A deliberate `movl $0x1234,%eax`-style 32-bit zero-extension is correct and
  must NOT be flagged as truncation.
- Valid `movq %rax,%rbx` must NOT be flagged: it genuinely copies rax into rbx
  in AT&T.

## Historical evals (real incidents, reproduced)

- **HerraduraKEx PR#33** — `mov ecx,[esp+4]` where `[esp]` was the correct slot;
  products silently written to the wrong slot. Reproduced as `bad/esp_offset.s`
  (x86-64 `8(%rsp)` vs `(%rsp)`); runtime test in Verified facts confirms the
  two slots differ.
- **BBoeOS PR#584** — `imul eax,eax,38` parsed with the immediate nulled →
  `69 c0 00 00 00 00` multiplies by zero. Reproduced as `bad/imul_nulled_imm.s`;
  objdump output recorded below.
- **r/asm AX=8-bit** (ASM-2) — "AX is 8-bit accumulator" claim. Reproduced as
  `bad/ax_is_8bit.s`.
- **CDC COMPASS fabrication** (ASM-3) — invented `JOB`/`SST`/`OCT` pseudo-ops.
  Reproduced as `bad/cdc_compass_pseudo.s`.

## Adversarial evals

- `bad/att_silent_swap.s` — assembles (exit 0) and runs, but `movq %rax,%rbx`
  stores rax into rbx, the reverse of the documented intent ("rax = rbx").
  Runtime test: swapped function returns 0 while the correct one returns 7.
- `bad/imul_nulled_imm.s` and `bad/byte_blind.s` — hand-encoded bytes that
  "assemble" (as data) and decode to something different from the claim.

## Verified facts (ACTUAL command output, recorded 2026-08-15)

```
gcc -c bad/invented_mnemonic.s
  exit 1: "Error: no such instruction: `movqad %rax,%rbx'"

gcc -c bad/cdc_compass_pseudo.s
  exit 1:
    "Error: number of operands mismatch for `jo'"        (JOB -> jo+b)
    "Error: no such instruction: `sst'"                  (SST)
    "Error: no such instruction: `oct 10'"               (OCT)

gcc -c bad/att_inverted.s
  exit 1: "Error: operand type mismatch for `mov'"       (movl 0x0,-0x4(%rbp))

gcc -c bad/{att_silent_swap,ax_is_8bit,esp_offset,imul_nulled_imm,byte_blind}.s
  exit 0 each — silent cases, caught by review/disassembly, not the assembler

gcc -c good/{gate_pass,roundtrip}.s
  exit 0 each

objdump -d (real output, PE/COFF x86-64):

good/gate_pass.o:
  0: 6b c0 26        imul  $0x26,%eax,%eax      (38 = 0x26, imm8 form)
  4: 89 d8           mov   %ebx,%eax
  6: c7 45 fc 00 00 00 00  movl $0x0,-0x4(%rbp)
  e: 8b 04 24        mov   (%rsp),%eax

good/roundtrip.o:
  0: 55              push  %rbp
  1: 48 89 e5        mov   %rsp,%rbp
  4: b8 26 00 00 00  mov   $0x26,%eax
  9: 6b c0 26        imul  $0x26,%eax,%eax
  c: 48 89 ec        mov   %rbp,%rsp
  f: 5d              pop   %rbp
 10: c3              ret

bad/imul_nulled_imm.o:
  0: 69 c0 00 00 00 00  imul  $0x0,%eax,%eax      (intended *38; immediate nulled)

bad/byte_blind.o:
  0: 8b 00           mov   (%rax),%eax            (claimed mov (%r8),%eax)
```

## Verified facts (runtime tests, pre-linked, both exit 0)

- Stack-slot test (`esp.s`): main stores 1234 at `(%rsp)` and calls two readers.
  `read_slot_plus8` (`8(%rsp)`) returns 1234; `read_slot_zero` (`(%rsp)`)
  returns the return address (≠1234). Program returns 0 iff both assertions
  hold — confirms `(%rsp)` and `8(%rsp)` are different slots at entry.
- AT&T swap test (`swap.s`): `movq %rax,%rbx` makes rbx=7 (correct AT&T);
  `movq %rbx,%rax` makes rax=0 (Intel-order reversal). Program returns 0 iff
  both hold — confirms the silent swap semantics.

## Scoring (for routing eval)

- precision: every flagged case maps to a named reference rule (1-8).
- recall: all bad snippets detected — assembler rejections, review-time width/
  offset/order violations, byte-level decoding, runtime reversals.
- FP-rate: good snippets produce zero flags.

## Target toolchains (absent, documented)

- `nasm` / `clang` / `llvm-mc` / `qemu`: not installed. NASM syntax (`$`
  directives, `default rel`), Thumb-2 `cbz` ranges, NEON and RISC-V encodings
  are documented in their own skills as researched; the gate procedure (assemble
  → disassemble → compare) transfers unchanged.
