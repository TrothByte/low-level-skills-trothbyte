# Evaluation — asm-signed-unsigned-branches

Skill: `skills/assembly/asm-signed-unsigned-branches`. Stability: `source-backed`, `verified`
(toolchain present: gcc 16.1.0, MSYS2, x86-64; see Toolchain status).

## Toolchain status

- `gcc 16.1.0` on PATH; used for every verification command below. All emitted-asm facts in
  `references/signed-unsigned.md` and in the Verified facts section were produced by this
  exact compiler on x86-64. Nothing is INFERRED.

## Verified facts (actual asm recorded from gcc 16.1, AT&T syntax)

Source probe (see SKILL.md "How to verify"): `f` signed `a<b`, `g` unsigned `a<b`,
`h` signed `a<0`, `d`/`dd` signed/unsigned division, `s`/`su` signed/unsigned right shift,
`e`/`eu` signed/unsigned short→int.

| C source | gcc -O0 emits | gcc -O2 emits | Reading |
|---|---|---|---|
| `int f(int a,int b){return a<b;}` | `setl %al` | `setl %al` | jl family (signed) |
| `int g(unsigned a,unsigned b){return a<b;}` | `setb %al` | `setb %al` | jb family (unsigned) |
| `int h(int a){return a<0;}` | `shrl $31, %eax` | `shrl $31, %eax` | sign-bit extraction |
| `int d(int a,int b){return a/b;}` | `cltd` + `idivl 24(%rbp)` | `cltd` + `idivl %ecx` | cdq sign-extends EAX into EDX |
| `unsigned dd(unsigned a,unsigned b){return a/b;}` | `movl $0, %edx` + `divl` | `xorl %edx, %edx` + `divl` | high half zeroed for div |
| `int s(int a,int n){return a>>n;}` | `sarl %cl, %edx` | `sarl %cl, %eax` | arithmetic shift (signed) |
| `int su(unsigned a,int n){return (int)(a>>n);}` | `shrl %cl, %edx` | `shrl %cl, %eax` | logical shift (unsigned) |
| `int e(short s){return s;}` | `movswl 16(%rbp), %eax` | `movswl %cx, %eax` | movsx (signed) |
| `int eu(unsigned short s){return (int)s;}` | `movzwl 16(%rbp), %eax` | `movzwl %cx, %eax` | movzx (unsigned) |

Branch probe at -O0 (`if (a<b) return 1; return 2;`): signed → `jge` (skip when NOT less),
unsigned → `jnb` (skip when NOT below); `<=` signed → `jg`, `<=` unsigned → `jb`;
`a < 0` signed → `jns` (skip when NOT sign). These confirm the same flag classes as the
setcc output.

## Synthetic evals

- **easy/negative**: `jge` used for an unsigned `>=` (`examples/bad/bad_signed_unsigned.s`
  `bad_unsigned_jge`) — must replace with `jae`.
- **easy/negative**: `shr` on a signed value (`bad_signed_shr`) — must replace with `sar`.
- **medium/negative**: `idiv` without `cdq` (`bad_idiv_no_cdq`) — must insert `cdq` before
  `idiv`.
- **medium/negative**: `div` with stale EDX (`bad_div_stale_edx`) — must zero EDX first.
- **hard/negative**: `movzx` on a signed load (`bad_movzx_signed`) — must use `movsx`;
  explain the value difference at `(short)-1`.
- **ambiguous**: a compare whose operands are only provably non-negative at runtime
  (e.g. a counter known `< INT_MAX`). Correct answer: mnemonic must still follow the C
  TYPE (unsigned → `jb`), not the value range; flag it only if the C source type is signed
  while the asm uses the unsigned mnemonic.
- **adversarial**: hand-written asm that "works" for all test inputs below 2^31 but is
  wrong for values >= 2^31 (the `jge`-for-unsigned pattern with a non-negative test suite).

## False-positive evals (correct code must NOT be flagged)

- `good_unsigned_jae` / `good_unsigned_jb` — correct unsigned branches, must NOT be
  rewritten to `jge`/`jl`.
- `good_signed_sar` — correct arithmetic shift, must NOT be "fixed" to `shr`.
- `good_idiv_cdq` / `good_div_zeroed` — the exact gcc 16.1 division sequences, must NOT be
  flagged (e.g. no "optimize away the cdq because edx starts zero").
- `good_movsx_signed` / `good_movzx_unsigned` — correct extension choice per type.
- `int h(int a){return a<0;}` → `shrl $31` — correct sign test; must NOT be reported as a
  missing compare.

## Verification commands

```
gcc -c examples/good/good_signed_unsigned.s -o /tmp/good.o        # assembles clean
gcc -c examples/bad/bad_signed_unsigned.s -o /tmp/bad.o           # assembles (logic bug, not syntax)
# emitted-mnemonic checks (expect setl/setb/sarl/shrl/cltd/divl/movswl/movzwl):
gcc -O0 -S -o - -x c - <<'EOF'
int f(int a,int b){return a<b;}
int g(unsigned a,unsigned b){return a<b;}
int h(int a){return a<0;}
int d(int a,int b){return a/b;}
unsigned dd(unsigned a,unsigned b){return a/b;}
int s(int a,int n){return a>>n;}
int su(unsigned a,int n){return (int)(a>>n);}
int e(short s){return s;}
int eu(unsigned short s){return (int)s;}
EOF
```

Expected: `setl`/`setb`/`shrl $31`/`cltd`+`idivl`/`xorl %edx,%edx`+`divl`/`sarl`/`shrl`/
`movswl`/`movzwl`. Run the same at `-O2` and confirm identical mnemonics (register
allocation may differ).

## Scoring

- detection: names the exact wrong instruction and the C type that was violated.
- reasoning: explains the flag (CF vs SF/OF) or register-pair (edx:eax) mechanism, not
  just "use the other mnemonic".
- fix: minimal change at the right instruction (one mnemonic / one inserted instruction).
- verification: recompiles and diffs `gcc -S` output against the Verified facts table.
