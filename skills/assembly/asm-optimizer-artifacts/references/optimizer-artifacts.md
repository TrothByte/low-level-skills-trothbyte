# Optimizer Artifacts in Generated Assembly — Reference

Empirical basis: GCC 16.1 (MinGW x86-64), recorded from `gcc -O0 -S` and `gcc -O2 -S`
on `examples/source_artifacts.c`. Verified 2026-08-14.
Format per rule: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE (bad) /
COUNTEREXAMPLE (good) / VERIFICATION / SOURCE (registry ids from `registry/sources.yaml`).

## 1. Inlining — function body merged into the caller

- **RULE**: when the optimizer decides a call is cheap and safe to inline, the callee's body
  is emitted inside the caller and the `call` is removed. The callee may still exist as a
  standalone symbol for external callers.
- **WHY AI GETS IT WRONG**: "my function `inline_me` has no callers anymore, the compiler
  removed the logic."
- **CORRECT REASONING**: the logic is present, fused into the caller. Example evidence:
  `caller(x)` returns `(x+1)*2`; at `-O2` GCC emits a single `leal 2(%rcx,%rcx), %eax`,
  which IS `2*x+2`. The `static` helper disappears entirely.
- **EXAMPLE** (bad): reading `-O0` asm, `caller` contains `call inline_me; addl %eax,%eax`,
  so the agent claims the `-O2` build "lost the helper call" and returns a wrong value.
- **COUNTEREXAMPLE** (good): at `-O2`, `caller` body `leal 2(%rcx,%rcx), %eax; ret` is
  recognized as the inlined + strength-reduced form of `(helper(x))*2`. Compare with `-O0`
  to confirm.
- **VERIFICATION**: `gcc -O2 -S` — `caller` has no `call`, but the arithmetic is present
  (`-O0`: `call inline_me` + `addl %eax,%eax`; `-O2`: `leal 2(%rcx,%rcx), %eax`).
- **SOURCE**: gcc-manual (Optimize Options, `-finline-functions`); godbolt-compiler.

## 2. Tail-call optimization — `jmp` replaces `call` + `ret`

- **RULE**: a function whose last action is calling another function and returning its result
  can be rewritten as `jmp callee`. No new stack frame, no `ret`; the callee returns directly
  to the original caller. Semantics are identical to `call` + `ret`.
- **WHY AI GETS IT WRONG**: "the asm shows `jmp`, there is no `call`, so `helper` is never
  invoked / the program behaves differently."
- **CORRECT REASONING**: the call IS performed; only the frame setup is elided. The argument
  register setup before the `jmp` (e.g. `movl %eax, %ecx`) is the same argument passing the
  source specifies.
- **EXAMPLE** (bad): `int tail(int x){ return helper(x); }`; at `-O2` GCC emits
  `jmp helper` with no `ret`. Agent concludes "helper is not called from tail".
- **COUNTEREXAMPLE** (good): the `jmp helper` transfers control to `helper`, which returns to
  `tail`'s caller — exactly the semantics of `tail` calling `helper` and returning its result.
  `-O0` shows the explicit `call helper; ret`, `-O2` shows the same behavior via `jmp helper`.
- **VERIFICATION**: `gcc -O2 -S` on `int tail(int x){ return helper(x); }` →
  `jmp helper` (verified GCC 16.1). Run the program: result equals the `-O0` build.
- **SOURCE**: gcc-manual (Optimize Options, `-foptimize-sibling-calls`);
  sysv-amd64-abi (3.2 function calling sequence); intel-sdm (JMP/CALL semantics).

## 3. Dead-code elimination — unused results removed

- **RULE**: any computation whose result is never used (no side effects, no volatile, no
  observable behavior) may be removed entirely.
- **WHY AI GETS IT WRONG**: "I wrote `dead = x * 100;` so the asm must contain `imull`."
- **CORRECT REASONING**: the local `dead` is never read; the store has no observable effect,
  so `-O2` deletes both the multiply and the store. `-O0` keeps them because it compiles
  expressions in source order without liveness analysis.
- **EXAMPLE** (bad): reading `-O0` asm for `int dce(int x){ int dead = x*100; return x+1; }`
  and claiming `-O2` "lost the multiply" because `imull $100` is absent.
- **COUNTEREXAMPLE** (good): at `-O2` only `leal 1(%rcx), %eax; ret` remains — exactly
  `x+1`. The multiply is provably dead. Check: make `dead` observable (e.g. `volatile` or
  return it) and it reappears.
- **VERIFICATION**: `gcc -O2 -S` — `imull $100` present at `-O0`, absent at `-O2` (verified).
- **SOURCE**: gcc-manual (Optimize Options, `-fdce`); godbolt-compiler.

## 4. Constant folding — compile-time expressions evaluated at build time

- **RULE**: expressions with all-constant operands are evaluated during compilation and the
  result emitted directly, so no runtime instructions remain for that computation.
- **WHY AI GETS IT WRONG**: "I wrote `2 + 3 * 4`, where are the `add` and `mul` instructions?"
- **CORRECT REASONING**: `2 + 3 * 4 == 14` is computed by the compiler; the function returns
  the constant. Note folding happens even at `-O0` (verified: both levels emit
  `movl $14, %eax`), so the `-O0` vs `-O2` diff is NOT the right way to detect this artifact.
- **EXAMPLE** (bad): claiming the compiler "skipped" the arithmetic and might return garbage.
- **COUNTEREXAMPLE** (good): `movl $14, %eax` at both `-O0` and `-O2` is recognized as folding
  of a constant expression; the program returns 14.
- **VERIFICATION**: `gcc -S` (any optimization level) — `fold` body is `movl $14, %eax` (verified).
- **SOURCE**: gcc-manual (Optimize Options); intel-sdm (MOV semantics).

## 5. Strength reduction — `lea` for small-constant multiplication

- **RULE**: multiplying by small constants is cheaper via address-computation: `x*3` becomes
  `lea (x + x*2)` (one instruction, no multiply unit). `lea` performs arithmetic on an address,
  it is not necessarily "taking an address".
- **WHY AI GETS IT WRONG**: "I wrote `*`, the asm has no `imul`, the multiply is gone" — or
  "`lea` is a pointer instruction, so `mul3` returns a pointer, that must be a bug".
- **CORRECT REASONING**: `int mul3(int x){ return x*3; }` at `-O2` emits
  `leal (%rcx,%rcx,2), %eax` = `rcx + rcx*2 = 3*x`. `-O0` emits the naive
  `addl %eax,%eax; addl %edx,%eax` (x+x+x).
- **EXAMPLE** (bad): reading `-O0` asm (three-instruction add sequence) and asserting the
  `-O2` `lea`-based build computes a different result.
- **COUNTEREXAMPLE** (good): `leal (%rcx,%rcx,2), %eax` is decoded as `3*x`; behavior matches
  `-O0`. Verify by running both binaries on the same input.
- **VERIFICATION**: `gcc -O2 -S` on `int mul3(int x){ return x*3; }` →
  `leal (%rcx,%rcx,2), %eax` (verified GCC 16.1).
- **SOURCE**: intel-sdm (LEA instruction reference); gcc-manual (Optimize Options);
  godbolt-compiler.

## 6. RIP-relative addressing — position-independent global access

- **RULE**: in position-independent / default code, access to globals is computed relative to
  the instruction pointer: `movq .refptr.g(%rip), %rax` then dereference. This is how the code
  stays relocatable; the address is not an absolute constant.
- **WHY AI GETS IT WRONG**: "there is a load from memory I did not write" / "the variable
  access must go through the GOT, this is PIC-specific junk" / confusing the `%rip`-relative
  load with an unrelated memory read.
- **CORRECT REASONING**: `int pic_read(void){ return g; }` needs to read the global `g`; at
  `-O2` GCC (MinGW) emits `movq .refptr.g(%rip), %rax; movl (%rax), %eax`. The first load
  fetches the address of `g` relative to RIP, the second dereferences it. Both `-O0` and `-O2`
  use this form; the difference between levels is only frame setup, not the addressing scheme.
- **EXAMPLE** (bad): reporting a spurious memory read as evidence of "code the source does
  not contain".
- **COUNTEREXAMPLE** (good): `pic_read` is recognized as `return g;` — one address load plus
  one data load. Verify by changing `g`'s value and re-running: the returned value tracks it.
- **VERIFICATION**: `gcc -O2 -S` — `movq .refptr.g(%rip), %rax; movl (%rax), %eax` for
  `return g;` (verified GCC 16.1, MinGW). On ELF targets the RIP-relative form is
  `movl g(%rip), %eax` directly.
- **SOURCE**: sysv-amd64-abi (4 Data Representation, position-independent code);
  intel-sdm (RIP-relative addressing, Vol.1 §3.7.5).

## 7. Instruction selection differences — `-O0` vs `-O2` codegen style

- **RULE**: `-O0` emits a rigid, stack-heavy, source-ordered form (every local spilled to the
  stack, arguments spilled then reloaded); `-O2` emits register-based code with minimal
  memory traffic. Both are correct implementations of the same source.
- **WHY AI GETS IT WRONG**: "the `-O0` asm stores to `16(%rbp)` then reloads — the program
  must be reading uninitialized or duplicated state", or "the two asm versions can't be the
  same function".
- **CORRECT REASONING**: `-O0` spills arguments because it keeps an addressable stack slot for
  each object; `-O2` keeps values in registers. Example: `tail` at `-O0` does
  `movl %ecx,16(%rbp)` then `movl 16(%rbp),%eax; movl %eax,%ecx; call helper`; at `-O2` it is
  `jmp helper` with `%ecx` already holding the argument.
- **EXAMPLE** (bad): claiming the `-O0` store/reload is a "data race" or "extra memory access
  the source never does".
- **COUNTEREXAMPLE** (good): recognize the stack traffic as the unoptimized codegen contract
  and use `-O2` asm (or `-O0` with `-fverbose-asm` and symbols) to reason about real data flow.
- **VERIFICATION**: diff `-O0` vs `-O2` on the same function; every removed store/reload must
  be traceable to a register-held value.
- **SOURCE**: gcc-manual (Optimize Options, `-fomit-frame-pointer`, `-O` levels);
  sysv-amd64-abi (3.2 calling sequence); godbolt-compiler.

## Detection workflow

```
1. Always compile the same source at -O0 and at the target level; diff.
2. For each difference, classify: inlining, tail-call, DCE, folding, strength reduction,
   addressing mode, or codegen style.
3. If a check or computation disappeared and none of the above explains it, suspect UB
   (compiler-ub-assumptions).
4. Confirm semantics by running the -O2 build against expected outputs, not by counting
   instructions.
```

## Calibration

- Most "missing code" reports against `-O2` asm are one of the seven artifacts above; the
  code is correct and complete.
- A tail-call `jmp` does NOT mean "no call"; DCE does NOT mean "the variable was optional";
  folding does NOT mean "the arithmetic is skipped at runtime for undefined reasons".
- Escalate to `c-undefined-behavior` only when a deletion is unexplained by artifacts AND the
  source contains UB the optimizer can assume away.
