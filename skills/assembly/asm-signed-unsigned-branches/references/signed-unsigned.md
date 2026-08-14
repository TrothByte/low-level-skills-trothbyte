# Signed vs Unsigned: Instruction Selection Rules

Sources cited by registry id: `intel-sdm`, `iso-c11-n1570`, `sysv-amd64-abi`, `gcc-manual`.
AT&T syntax (what gcc emits); Intel operands given in parentheses where useful.

## 1. Comparison mnemonics: jl/jg/jge/jle (signed) vs jb/ja/jae/jbe (unsigned)

- **RULE**: `cmp a, b` computes `a - b` and sets the flags. The signed conditions decode
  SF (sign) and OF (overflow): `jl` = SF != OF, `jge` = SF == OF, `jg` = ZF=0 and SF==OF,
  `jle` = ZF=1 or SF!=OF. The unsigned conditions decode CF (carry/borrow): `jb` = CF=1,
  `jae` = CF=0, `ja` = CF=0 and ZF=0, `jbe` = CF=1 or ZF=1. `test` = bitwise AND, clears
  CF and OF (useful only for zero/sign tests: `js`/`jns`/`jz`). `setcc` uses the same
  conditions: `setl`, `setb`, etc.
- **WHY AI GETS IT WRONG**: the mnemonics look like ordinary math ("greater-or-equal");
  the agent forgets that "greater" (`jg`) means SIGNED greater and "above" (`ja`) means
  UNSIGNED above. Same `cmp`, different flags read.
- **CORRECT REASONING**: signedness is not encoded in the operand values or the flags;
  only the chosen mnemonic selects the condition. For one flag state the two classes can
  disagree: after `cmpl $-1, $2` (i.e. 2 - (-1)), the carry flag is 0 (no borrow), but
  SF is 1 and OF is 0 (2 - (-1) overflows), so `jge` is false while `jae` is true.
- **EXAMPLE** (bad):
  ```asm
  cmpl    %esi, %edi        # a - b
  jge     .taken            # tests SF==OF: WRONG if operands are unsigned
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  cmpl    %esi, %edi        # a - b
  jae     .taken            # CF=0: unsigned a >= b
  ```
- **VERIFICATION**: `gcc -O0 -S` on `int f(int a,int b){return a<b;}` yields `setl`;
  on `int g(unsigned a,unsigned b){return a<b;}` yields `setb`. A branch probe
  `if (a < b) return 1; return 2;` yields `jge` (skip-when-not-less, signed) vs `jnb`
  (skip-when-not-below, unsigned) at `-O0` on gcc 16.1.
- **SOURCE**: `intel-sdm` Vol.2 (Jcc, SETcc, CMP, TEST, SUB flag semantics).

## 2. `test`/`cmp` flag semantics

- **RULE**: `cmp` = `sub` (subtracts, no store): sets CF (borrow), OF, SF, ZF. `test` =
  `and` (no store): clears CF and OF, sets SF/ZF/PF. Zero/sign tests use `test`/`js`/`jns`
  or `cmp` against zero. A `cmp $0, x` + `jge` is a sign+zero test (SF==OF with OF=0 means
  SF=0, so it means "x >= 0"); `jns` tests the sign bit directly.
- **WHY AI GETS IT WRONG**: agents substitute `test` for `cmp` in comparisons that need
  CF, then wonder why `jb`/`jae` misbehave (CF is always 0 after `test`).
- **CORRECT REASONING**: after `test`, CF and OF are both 0, so the unsigned conditions
  always take the "no borrow / no overflow" interpretation — `jb` never fires. Use `cmp`
  when a magnitude ordering is intended.
- **EXAMPLE** (bad): `testl %esi, %edi; jae .taken` — CF=0 always, branch always taken.
- **COUNTEREXAMPLE** (good): `cmpl %esi, %edi; jae .taken`; use `testl %esi,%esi; jns`
  only for the sign test of a single operand.
- **VERIFICATION**: objdump the two sequences and confirm CF behavior; or run the flags
  through a single-step debugger (gdb `p $eflags`).
- **SOURCE**: `intel-sdm` Vol.2 (TEST, CMP, SUB, AND flag summaries).

## 3. Right shift: `sar` (signed) vs `shr` (unsigned)

- **RULE**: `sar` (arithmetic) fills vacated bits with the sign bit; `shr` (logical) fills
  with 0. C `>>` on a signed negative value is implementation-defined (N1570 §6.5.7p5);
  GCC on x86 emits `sar` for signed operands and `shr` for unsigned. Left shift is the same
  for both (`shl`/`sal`).
- **WHY AI GETS IT WRONG**: "right shift" defaults to `shr` in the agent's memory; or the
  agent writes `sar` blindly without checking the operand type.
- **CORRECT REASONING**: `-16 >> 4` must be `-1`; with `shr` it becomes `0x0fffffff`
  (268435455). The compiler chooses the instruction from the type, so a `sar` in
  disassembly is evidence the operand is signed and `shr` evidence it is unsigned.
- **EXAMPLE** (bad): hand-written signed `a >> 4` as `shrl $4, %eax` — sign lost.
- **COUNTEREXAMPLE** (good): `sarl $4, %eax` (AT&T) / `sar eax, 4` (Intel).
- **VERIFICATION**: `int s(int a,int n){return a>>n;}` → `sarl %cl, %eax`;
  `int su(unsigned a,int n){return (int)(a>>n);}` → `shrl %cl, %eax` (gcc 16.1, -O0/-O2).
- **SOURCE**: `intel-sdm` Vol.2 (SAR, SHR); `iso-c11-n1570` §6.5.7p5; `gcc-manual`
  (Shift options/behavior for negative signed operands).

## 4. Signed division: `cdq`/`cqo` before `idiv`

- **RULE**: `idiv` divides the double-width dividend `EDX:EAX` (32-bit) or `RDX:RAX`
  (64-bit) by the operand; the high half must contain the sign-extension of the low half:
  `cdq` (AT&T `cltd`) for 32-bit, `cqo` (AT&T `cqto`) for 64-bit. With `idiv`, the divisor
  must be non-zero and `INT_MIN / -1` overflows (raises #DE), which is UB in C.
- **WHY AI GETS IT WRONG**: the agent loads only `eax` and divides, forgetting the
  instruction uses the `edx:eax` register pair, so `edx` still holds garbage — quotient
  is wrong or the CPU traps (#DE).
- **CORRECT REASONING**: a 32-bit signed divide takes a 64-bit dividend. Sequence is
  `movl a, %eax; cltd; idivl b` (AT&T) / `mov eax, a; cdq; idiv b` (Intel). The compiler
  emits exactly this: gcc 16.1 `int d(int a,int b){return a/b;}` → `cltd` then `idivl`.
- **EXAMPLE** (bad):
  ```asm
  movl    %edi, %eax
  movl    %esi, %ecx
  idivl   %ecx           # edx uninitialized: wrong quotient or #DE
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  movl    %edi, %eax
  movl    %esi, %ecx
  cltd                    # cdq: sign-extend eax into edx
  idivl   %ecx
  ```
- **VERIFICATION**: compile `int d(int a,int b){return a/b;}` → `cltd; idivl %ecx`
  at -O0 and -O2 (gcc 16.1). Run `d(-7, 2)`; without `cltd` the quotient is wrong.
- **SOURCE**: `intel-sdm` Vol.2 (IDIV, CDQ/CQO); `sysv-amd64-abi` §3.2 (edx caller-saved,
  clobbered across calls, reused as high dividend half); `iso-c11-n1570` §6.5.5p5
  (INT_MIN/-1 UB).

## 5. Unsigned division: `xor edx,edx` (or `xor rdx,rdx`) before `div`

- **RULE**: `div` is the unsigned divide; it also reads `EDX:EAX`, but the high half must
  be zero: `xorl %edx, %edx` before `divl` (64-bit: `xorq %rdx, %rdx` before `divq`).
- **WHY AI GETS IT WRONG**: the agent either reuses the `cdq` habit for unsigned operands
  (sign-extends a value that is already zero-extended, which for a positive value in eax
  actually still works — masking the mistake), or writes `div` with a stale edx.
- **CORRECT REASONING**: zeroing edx is the unsigned counterpart of `cdq`. If the value in
  eax is non-negative both approaches coincide, so the bug only shows for large operands
  or stale edx. Do not "optimize" away the `xor`.
- **EXAMPLE** (bad): `divl %ecx` after `movl %edi,%eax` where edx holds leftover data.
- **COUNTEREXAMPLE** (good):
  ```asm
  movl    %edi, %eax
  movl    %esi, %ecx
  xorl    %edx, %edx      # high half = 0 (unsigned)
  divl    %ecx
  ```
- **VERIFICATION**: `unsigned dd(unsigned a,unsigned b){return a/b;}` → `movl $0, %edx`
  at -O0 and `xorl %edx, %edx` at -O2, then `divl` (gcc 16.1).
- **SOURCE**: `intel-sdm` Vol.2 (DIV); `sysv-amd64-abi` §3.2 (edx clobber).

## 6. Sign vs zero extension: `movsx` vs `movzx`

- **RULE**: `movsx` sign-extends a narrow value into a wider register; `movzx`
  zero-extends. C assignment `(int)s` from a signed `short` must preserve the sign →
  `movsx` (AT&T `movswl`); from `unsigned short` → `movzx` (AT&T `movzwl`). Byte variants:
  `movsbl`/`movzbl`.
- **WHY AI GETS IT WRONG**: "movzx clears the upper bits, that is what I want" — but for a
  signed value the upper bits must be the sign extension, not zeros; or the agent writes a
  plain `mov` and leaves the upper bits as garbage.
- **CORRECT REASONING**: `mov` of a 16-bit value preserves the stale upper 16 bits of the
  destination. Only `movsx`/`movzx` define the full destination. The compiler picks by
  type: gcc 16.1 `int e(short s){return s;}` → `movswl %cx, %eax`;
  `int eu(unsigned short s){return (int)s;}` → `movzwl %cx, %eax`.
- **EXAMPLE** (bad): `movzwl %cx, %eax` for `(int)(short)s` — sign bit lost, `(short)-1`
  becomes 65535.
- **COUNTEREXAMPLE** (good): `movswl %cx, %eax` for signed; `movzwl %cx, %eax` for unsigned.
- **VERIFICATION**: compile the two C functions above and diff the mnemonics (`movswl`
  vs `movzwl`).
- **SOURCE**: `intel-sdm` Vol.1 §3.4.1.1 (sign/zero extension), Vol.2 (MOVSX, MOVZX);
  `iso-c11-n1570` §6.3.1.3 (integer conversion preserves value; signed→unsigned is modulo).

## 7. How the C compiler selects the branch instruction

- **RULE**: the branch mnemonic is chosen by the usual arithmetic conversions
  (N1570 §6.3.1.8), not by the values. `if (a < b)` with two `int` → signed compare →
  `jl` (or `setl`). `if ((unsigned)a < b)` → unsigned compare → `jb` (or `setb`). If one
  operand is `unsigned` with the same rank as the signed one, the signed operand converts
  to unsigned (modulo 2^N) and the compare is unsigned.
- **WHY AI GETS IT WRONG**: the agent assumes `a < b` always compiles to the same mnemonic
  and hand-writes `jl` for code that is actually comparing unsigned values, or reads a
  disassembly and "translates" it back to C without checking types.
- **CORRECT REASONING**: for a given flag state, signed and unsigned conditions can give
  opposite answers. `-1 < 2` is TRUE signed and FALSE as `(unsigned)-1 < 2u`; the same
  `cmpl` flags decode differently. So `setl` vs `setb` is a type-level fact, not a
  value-level one. `int h(int a){return a<0;}` needs no compare at all — gcc 16.1 emits
  `shrl $31, %eax` (sign-bit extraction).
- **EXAMPLE** (bad): reading `cmpl %edx, %ecx; jge` and concluding the function returns
  `a >= b` — correct only if the source operands are signed.
- **COUNTEREXAMPLE** (good): map the mnemonic to the C type: signed → `jl/jg/jge/jle`;
  unsigned → `jb/ja/jae/jbe`; check the source cast `(unsigned)a` before trusting a `jb`.
- **VERIFICATION**: compile `f`/`g`/`h` probes and diff: `setl` vs `setb` vs `shrl $31`
  (gcc 16.1, -O0 and -O2). Build with `-Wall -Wextra` to surface `-Wsign-compare` on the
  source level.
- **SOURCE**: `iso-c11-n1570` §6.3.1.8, §6.5.8 (relational operators produce int and
  apply usual conversions); `gcc-manual` (-Wsign-compare); `intel-sdm` Vol.2 (Jcc).

## 8. Reading disassembly to detect sign mistakes

- **RULE**: a sign mistake is detectable as a mnemonic-class mismatch: unsigned operands
  with `jg/jl/jge/jle`, `shr` on a signed value, `idiv` without `cdq`/`cqo`, `div` with
  edx not zeroed, `movzx` on a signed load. The compiler's own instruction choice is the
  ground truth for the type.
- **WHY AI GETS IT WRONG**: disassembly shows only mnemonics and registers; the C type is
  gone, so the agent "translates" mechanically instead of validating semantics.
- **CORRECT REASONING**: for each instruction ask: (a) which flags does this jcc read —
  SF/OF (signed) or CF (unsigned)? (b) does the operation need the high half sign-extended
  (`idiv`) or zeroed (`div`)? (c) does the shift fill with the sign bit (`sar`) or 0
  (`shr`)? Recover types from the compiler's choice: `sar` ⇒ signed, `shr` ⇒ unsigned,
  `cltd`/`cqto` ⇒ signed divide, `xor edx` + `div` ⇒ unsigned divide, `movswl`/`movsbl` ⇒
  signed load, `movzwl`/`movzbl` ⇒ unsigned load.
- **EXAMPLE** (bad): a routine that computes "a / 16 rounding toward negative infinity"
  but the disassembly shows `shrl $4` — for `a = -32` the correct result is `-2`, the asm
  produces `0x0ffffffe`. The signed shift requires `sar`.
- **COUNTEREXAMPLE** (good): the same routine with `sarl $4`; verify by compiling the
  signed C version and diffing the instruction.
- **VERIFICATION**: `objdump -d`/`gcc -S` the C probe and grep the mnemonic classes;
  the reference lists exact gcc 16.1 outputs for compares, shifts, divides, extensions.
- **SOURCE**: `intel-sdm` Vol.2 (SAR/SHR, DIV/IDIV, MOVSX/MOVZX, Jcc flag semantics);
  `sysv-amd64-abi` §3.2; `gcc-manual` (-S output, -O2 code motion).
