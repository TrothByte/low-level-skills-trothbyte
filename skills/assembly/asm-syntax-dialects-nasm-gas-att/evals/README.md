# Evaluation — asm-syntax-dialects-nasm-gas-att

Skill: `skills/assembly/asm-syntax-dialects-nasm-gas-att`.
Stability target: `evaluated` (GAS half), `researched` (NASM half). Toolchain:
GCC 16.1.0 / GNU as 2.46 / objdump 2.46 (MSYS2 MinGW ucrt64, x86_64-w64-mingw32,
PE/COFF). `nasm` is NOT installed — all NASM cases are documented as researched
with their exact verification command; nothing NASM was run here.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/att_order.s` | AT&T reversal rejected | exit 1, "operand type mismatch for `mov'" |
| medium/negative | `bad/att_immediate.s` | missing `$` caught by review | exit 0, must be flagged |
| medium/negative | `bad/nasm_case.asm` | case mismatch flagged | researched (nasm absent) |
| medium/negative | `bad/nasm_addr.asm` | address-vs-content flagged | researched (nasm absent) |
| medium/negative | `bad/nasm_size.asm` | missing size hint flagged | researched (nasm absent) |
| medium/negative | `bad/nasm_default_rel.asm` | absolute addressing flagged | researched (nasm absent) |
| medium/negative | `bad/nasm_dollar.asm` | `$` in 3.x directive flagged | researched (nasm absent) |

## False-positive evals (correct code must not be flagged)

- `good/att_correct.s` — correct AT&T: source-first, `$` immediate. NOT flagged.
- `good/intel_syntax.s` — GAS `.intel_syntax noprefix`: correct GAS Intel form.
  NOT flagged.
- `good/gas_case.s` — uppercase mnemonic `MOVL` in GAS: assembles fine (GAS is
  case-insensitive for mnemonics); must NOT be flagged as a NASM-style case
  error.
- `good/nasm_*.asm` — correct NASM: consistent case, `[buf]`/`lea`, explicit
  `qword`/`dword` hints, `default rel`, bare directive args. NOT flagged.
- `gcc -S` (AT&T) vs `gcc -S -masm=intel` of `good/src.c`: both correct; a
  reviewer must not call the AT&T output "reversed".

## Historical evals (real incidents, reproduced)

- **ocrosby PR#33** — four documented NASM error classes: label case, `buf` vs
  `[buf]`, missing size hints, missing `default rel`. Reproduced as
  `bad/nasm_case.asm`, `bad/nasm_addr.asm`, `bad/nasm_size.asm`,
  `bad/nasm_default_rel.asm` (researched).
- **BBoeOS PR#506** — `global $abs`/`extern $abs` invalid in NASM 3.x.
  Reproduced as `bad/nasm_dollar.asm` (researched).

## Adversarial evals

- `bad/att_immediate.s` — assembles cleanly (exit 0) and runs, but `movl 5,
  %eax` loads from absolute address 5 (memory), not constant 5. objdump
  proves it: `8b 04 25 05 00 00 00 mov 0x5,%eax`. Silent wrong semantics.
- `bad/nasm_addr.asm` — would assemble (once size issues fixed) while loading
  the address instead of the value — "not an error, just not what you meant".

## Verified facts (ACTUAL command output, recorded 2026-08-15)

```
# Source-backed (GAS):
gcc -O2 -S good/src.c              exit 0  (AT&T, default)
  f: imull $38, %ecx, %eax          addl (%rdx), %eax
gcc -O2 -masm=intel -S good/src.c   exit 0  (GAS Intel mode)
  f: imul eax, ecx, 38              add eax, DWORD PTR [rdx]

gcc -c good/att_correct.s           exit 0
objdump -d good/att_correct.o:
  0: c7 45 fc 00 00 00 00  movl $0x0,-0x4(%rbp)     (correct store)
  7: b8 05 00 00 00        mov $0x5,%eax            (immediate 5)

gcc -c good/intel_syntax.s          exit 0
objdump -d good/intel_syntax.o:
  0: b8 05 00 00 00        mov $0x5,%eax
  5: c7 45 fc 00 00 00 00  movl $0x0,-0x4(%rbp)

gcc -c good/gas_case.s              exit 0
objdump -d good/gas_case.o: 0: 89 c3  mov %eax,%ebx  (MOVL == movl in GAS)

gcc -c bad/att_order.s              exit 1
  Error: operand type mismatch for `mov'   (movl 0x0, -0x4(%rbp))

gcc -c bad/att_immediate.s          exit 0   <- adversarial silent case
objdump -d bad/att_immediate.o:
  0: 8b 04 25 05 00 00 00  mov 0x5,%eax   (address 5, NOT constant 5)
```

## Verified facts (researched — NASM absent on this host)

These have NOT been run here; the verification commands are given so they can
be executed on any host with NASM installed:

```
nasm -f elf64 bad/nasm_case.asm
  expected exit 1: "error: symbol `loop' not defined" (label case mismatch)

nasm -f elf64 bad/nasm_size.asm
  expected exit 1: "error: operation size not specified" (inc [counter])

nasm -f elf64 bad/nasm_dollar.asm
  expected exit 1 (NASM 3.x): "$" invalid in `global` argument

nasm -f elf64 good/nasm_case.asm      expected exit 0
nasm -f elf64 good/nasm_addr.asm      expected exit 0
nasm -f elf64 good/nasm_size.asm      expected exit 0
nasm -f elf64 good/nasm_default_rel.asm  expected exit 0
nasm -f elf64 good/nasm_dollar.asm    expected exit 0
```

## Scoring (for routing eval)

- precision: every flagged case maps to a named reference rule (1-7).
- recall: all bad snippets detected (assembler errors + review-time dialect
  violations + the silent `att_immediate` case).
- FP-rate: good GAS + good NASM + `gcc -S` outputs produce zero flags.

## Target toolchains (absent, documented)

- `nasm`: not installed. All NASM-specific verification is documented as
  researched with exact commands; no NASM claim here was run on this host.
- `gcc -masm=intel` and `.intel_syntax noprefix` ARE verified — the GAS half is
  fully source-backed.
