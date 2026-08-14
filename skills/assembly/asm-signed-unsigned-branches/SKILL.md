---
name: asm-signed-unsigned-branches
description: Use when writing or reading x86-64 assembly, inline asm, or disassembly where signed vs unsigned semantics decide the instruction — jl/jg/jge/jle vs jb/ja/jae/jbe, sar vs shr, cdq/idiv vs xor/div, movsx vs movzx. Teaches flag semantics and how compilers select branch mnemonics from C types.
---

# Assembly: Signed vs Unsigned Branches

## When to use

- Writing x86-64 assembly or `asm()` inline C that compares, shifts, divides, or sign/zero-extends.
- Reviewing disassembly (objdump/gdb) to check whether a compare, shift, or divide treats the
  operand as signed or unsigned.
- Porting a C `if`, `/`, `%`, `>>`, or cast to hand-written asm.
- Debugging wrong results only for large/negative inputs (classic sign-mnemonic bug).
- Answering "why did GCC emit `setl` here and `setb` there for the same-looking `a < b`".

## When not to use

- Purely in C with no asm inspection — use `c-integer-promotion-and-conversion` for the
  conversion rules and `c-undefined-behavior` for overflow/shift UB.
- Floating-point compares (different flag/instruction set: `ucomisd`, `jae` reads CF there).
- AArch64/ARM/RISC-V — different mnemonics (`csel lt` vs `cslo`, `asr` vs `lsr`).
- `mov`/`lea`/`add`-only code with no compares, shifts, divides, or extensions.

## What the agent often gets wrong

- "`jge` means greater-or-equal, so it is the right branch for `>=`." Wrong: `jge` is the
  SIGNED condition (reads SF/OF). `jae` (CF-based) is the unsigned `>=`.
- "The compiler figures out signedness from the value." No: same `cmp`, different flags
  decoded. Only the mnemonic carries signedness.
- "Right shift is `shr`." For signed values it must be `sar`; `shr` on a negative value
  loses the sign (C leaves negative `>>` implementation-defined, arithmetic on x86).
- "`idiv` takes one register." It divides EDX:EAX (or RDX:RAX); the high half must be
  sign-extended with `cdq`/`cqo` first, otherwise the quotient is garbage or a #DE trap.
- "`div` and `idiv` only differ in signedness of the result." They also differ in the
  required high half: zero (`xor edx,edx`) for `div`, sign-extended for `idiv`.
- "`movzx` is safe for any narrow load." Sign must be preserved with `movsx` (AT&T:
  `movswl`/`movsbl`).
- Treating `setcc` output as unrelated to `jcc`: `setl`/`setb` are the same conditions as
  `jl`/`jb` in the `-O0`-style branch probe.

## How to reason correctly

1. Identify the C type of each operand. Apply ISO C11 §6.3.1.8 usual arithmetic conversions:
   a signed `int` compared against an `unsigned` converts to `unsigned` (modulo 2^N) —
   the compare becomes unsigned, so `-1 < 1u` is FALSE and the asm must use the unsigned
   mnemonic.
2. Map the type to the condition class: signed → `jl/jg/jge/jle` (flags SF, OF, ZF);
   unsigned → `jb/ja/jae/jbe` (flags CF, ZF).
3. For every jcc, ask "which flag does it read?" `jl` reads SF!=OF; `jb` reads CF=1.
   They can disagree for the same flag state.
4. For shifts: signed `>>` → `sar` (fills with sign bit); unsigned `>>` → `shr` (fills 0).
5. For division: signed `x/y` → `cdq`/`cqo` then `idiv`; unsigned → `xor edx,edx` (or
   `xor rdx,rdx`) then `div`.
6. For narrow loads: signed type → `movsx`; unsigned type → `movzx`.
7. When reading disassembly, recover the source type from the compiler's instruction
   choice: `sar`/`cltd`/`movsx`/`jl` imply signed; `shr`/`xor edx`+`div`/`movzx`/`jb`
   imply unsigned. Do not guess the type from the register.

## What to verify

- The mnemonic class matches the C type: signed code shows `jl/jg/jge/jle`, unsigned code
  shows `jb/ja/jae/jbe`.
- Signed right shift compiles to `sar`, unsigned to `shr`.
- Signed division is preceded by `cdq`/`cqo` (AT&T `cltd`/`cqto`); unsigned division
  zeroes EDX/RDX first.
- Narrow loads use `movsx` for signed and `movzx` for unsigned types.
- No `-Wsign-compare` warnings on the source that motivated the asm.
- `if (a < 0)` compiles to a sign-bit test (`shrl $31`), not a signed compare.

## How to verify

Compile probes with the real compiler and diff the mnemonics:

```
cat > probe.c <<'EOF'
int f(int a, int b)        { return a < b; }              // signed   -> setl (jl)
int g(unsigned a, unsigned b) { return a < b; }           // unsigned -> setb (jb)
int s(int a, int n)        { return a >> n; }             // signed   -> sarl
int su(unsigned a, int n)  { return (int)(a >> n); }      // unsigned -> shrl
int d(int a, int b)        { return a / b; }              // signed   -> cltd + idivl
unsigned dd(unsigned a, unsigned b) { return a / b; }     // unsigned -> xorl %edx + divl
int e(short s)             { return s; }                  // signed   -> movswl (movsx)
int eu(unsigned short s)   { return (int)s; }             // unsigned -> movzwl (movzx)
EOF
gcc -O0 -S probe.c -o - | grep -E 'setl|setb|sar|shr|cltd|idiv|divl|movs[lwz]|movz'
gcc -O2 -S probe.c -o - | grep -E 'setl|setb|sar|shr|cltd|idiv|divl|movs[lwz]|movz'
```

Expected on x86-64 with gcc 16.1: `setl`, `setb`, `sarl %cl,%eax`, `shrl %cl,%eax`,
`cltd`+`idivl`, `xorl %edx,%edx`+`divl`, `movswl`, `movzwl`.

Hand-written examples assemble with `gcc -c examples/good/good_signed_unsigned.s`.

## Where the knowledge comes from

- `intel-sdm` Vol.2 instruction set reference: Jcc/SETcc flag conditions, CMP, TEST, SAR/SHR,
  DIV/IDIV, CDQ/CQO, MOVSX/MOVZX; Vol.1 §3.4.1.1 sign/zero extension.
- `iso-c11-n1570` §6.3.1.8 (usual arithmetic conversions pick the comparison), §6.5.7
  (negative right shift implementation-defined), §6.3.1.3 (signed→unsigned modulo).
- `sysv-amd64-abi` §3.2 calling sequence (args in edi/esi, edx caller-saved and used as
  the high dividend half).
- `gcc-manual` (-Wall/-Wsign-compare warnings, Optimize Options for the emitted code).

## Related skills

- `c-integer-promotion-and-conversion` — the conversion rules that decide signed vs unsigned
  compares (required prerequisite).
- `c-undefined-behavior` — signed overflow/divide traps behind `idiv` (e.g. INT_MIN / -1).
- `compiler-ub-assumptions` — why a sign mistake compiles but produces wrong code under -O2.

## Evaluation

Synthetic: signed vs unsigned compare on the same flag state; `sar` vs `shr` on a negative
value; `idiv` without `cdq`; `movzx` on a signed load; branch-vs-setcc equivalence.
False-positive: correct `cdq`+`idiv`, correct `jb` for unsigned, `xor edx,edx`+`div` must
NOT be flagged. Adversarial: code that is correct for non-negative inputs but wrong for
negative ones, and an `if ((unsigned)a < b)` C snippet whose disassembly uses the signed
mnemonic in hand-written asm.
