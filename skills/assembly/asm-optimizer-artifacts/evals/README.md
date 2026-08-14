# Evaluation — asm-optimizer-artifacts

Skill: `skills/assembly/asm-optimizer-artifacts`. Stability target: `evaluated`.

## Synthetic evals (core)

| ID | Case | Expected agent behavior |
|---|---|---|
| AO-01 | `int tail(int x){ return helper(x); }` at `-O2` | recognize `jmp helper` as a tail call, NOT "call deleted"; argument flow intact |
| AO-02 | `int mul3(int x){ return x*3; }` at `-O2` | decode `leal (%rcx,%rcx,2), %eax` as `3*x`; no `imul` needed |
| AO-03 | `caller` with a `static` helper at `-O2` | recognize inlining: `leal 2(%rcx,%rcx), %eax` = `(x+1)*2`, no `call` remains |
| AO-04 | `int dce(int x){ int dead=x*100; return x+1; }` at `-O2` | explain `imull` absence as DCE; make `dead` observable and it returns |
| AO-05 | `int fold(void){ return 2+3*4; }` at `-O2` | constant folding: `movl $14, %eax`; NOT a runtime skip of arithmetic |
| AO-06 | `extern int g; int f(void){ return g; }` at `-O2` | recognize RIP-relative global read (`movq .refptr.g(%rip), %rax; movl (%rax), %eax`) |

## False-positive evals

- `-O0` asm showing stack spill/reload must NOT be flagged as "extra memory access" or a bug.
- An absent `call` in tail position must NOT be reported as "helper is never called".
- A folded constant must NOT be reported as "compiler deleted the function".
- `lea` with register operands must NOT be misread as a pointer computation.

## Adversarial evals

- UB-driven deletion presented as an optimizer artifact: `int f(int*p){ int x=*p; if(!p) return 0; return x; }`
  — the removed check is UB-based, NOT one of the seven artifacts; must escalate to
  `c-undefined-behavior` / `compiler-ub-assumptions`.
- Compiler-divergent artifact claims: must verify with the actual toolchain, not assert.

## Verification fixtures and commands

```
# regenerate the checked-in asm (GCC 16.1, MinGW x86-64)
gcc -O0 -S examples/bad/artifacts.c -o examples/bad/artifacts_O0.s
gcc -O2 -S examples/good/artifacts.c -o examples/good/artifacts_O2.s
diff examples/bad/artifacts_O0.s examples/good/artifacts_O2.s
```

## Verified facts (GCC 16.1.0, MinGW x86-64, 2026-08-14)

- `tail` `-O0`: `movl %ecx,16(%rbp); movl 16(%rbp),%eax; movl %eax,%ecx; call helper; ...; ret`
- `tail` `-O2`: `jmp helper` (no frame, no `ret`)
- `mul3` `-O2`: `leal (%rcx,%rcx,2), %eax` (x*3 via address arithmetic)
- `fold`: `movl $14, %eax` at BOTH `-O0` and `-O2` (folding is level-independent)
- `dce` `-O2`: only `leal 1(%rcx), %eax; ret` (imull $100 removed)
- `caller` `-O2`: `leal 2(%rcx,%rcx), %eax` (inlined + folded `(x+1)*2`)
- `pic_read` (both levels): `movq .refptr.g(%rip), %rax; movl (%rax), %eax` (MinGW RIP-relative via refptr)

## Scoring

- detection: names the artifact class (inlining / tail-call / DCE / folding / strength
  reduction / addressing / codegen style).
- reasoning: reconstructs source semantics from the artifact BEFORE re-running the compiler.
- verification: demonstrates with `-O0 -S` vs `-O2 -S` diff and actual instruction lines.
- escalation: does not call UB-driven deletions "optimizer artifacts".
