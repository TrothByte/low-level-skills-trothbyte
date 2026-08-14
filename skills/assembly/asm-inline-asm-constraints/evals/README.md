# Evaluation — asm-inline-asm-constraints

Skill: `skills/assembly/asm-inline-asm-constraints`. Stability target: `evaluated`.
Toolchain: GCC 16.1.0 (MSYS2 MinGW x86-64), rustc 1.97.1 (x86_64-pc-windows-msvc).

## Synthetic evals

- **easy/negative**: `asm volatile("movl $2, (%0)" : : "r"(&g))` then re-reading
  `g` — agent must flag the missing `"memory"` clobber and predict the wrong sum.
- **medium/negative**: `asm volatile("xorl %%eax, %%eax")` with a value live in
  eax — must flag the missing `"eax"`/`"cc"` clobber.
- **hard/negative**: `asm volatile("shll %1, %0" : "+r"(x) : "c"(n))` — must
  identify that the template needs the `%b` modifier (`%cl`).
- **adversarial**: AT&T-style `asm!("add {1:e}, {0:e}", inout(reg) r, in(reg) b)`
  inside Rust — must catch the Intel-syntax inversion that silently returns `a`.

## False-positive evals

- `examples/good/constraints.c` (matching constraint, `"a"`/`"A"` multiply,
  `"m"` operand, `"c"`+`%b1` shift, asm goto) — must NOT be flagged.
- `examples/good/clobbers.c` (`"memory"` and `"eax","cc"` clobbers) — must NOT be
  flagged.
- `examples/good/rust_asm.rs` (u64 `inout`, `const` operand, `lateout`,
  `options(nostack)`) — must NOT be flagged.
- A correct `asm volatile("" ::: "memory")` barrier — must NOT be "simplified".

## Verified facts

- Missing `"memory"` clobber (GCC 16.1): asm writing 2 through `"r"(&g)` is
  invisible to the compiler; the two reads of `g` collapse to one load and the
  function returns 2 instead of 3. With `"memory"` it returns 3 (run verified).
- Store merge (GCC 16.1, `-O2 -S`): `*p=1; asm volatile(""); *p=2` emits only
  `movl $2,(%rcx)`; with `"memory"` both `movl $1` and `movl $2` are emitted.
- Load CSE (`-O2 -S`): `unsigned a=*p; asm(""); b=*p; return a+b` emits one load
  (`addl %eax,%eax`); with `"memory"` it emits two loads.
- Missing register clobber (GCC 16.1, `-O2 -S`): unlisted `xorl %%eax,%%eax`
  compiles to `movl %ecx,%eax; xorl %eax,%eax; ret` (returns 0); listing
  `"eax","cc"` gives `xorl %eax,%eax; movl %ecx,%eax; ret` (returns the input).
- Wrong constraint errors (GCC 16.1): `"i"` with runtime value →
  "impossible constraint in 'asm'"; output without `=` → "output operand
  constraint lacks '='"; `+` plus matching `"0"` on the same operand →
  "inconsistent operand constraints in an 'asm'".
- Rust asm! (rustc 1.97.1): Intel syntax; `add {0:e}, {1:e}` with
  `inout(reg), in(reg)` returns 42, while AT&T-style `add {1:e}, {0:e}` returns
  20 (wrong). `add {0}, {number}` with `number = const 5` returns 8. `lateout`
  + `pushfq/pop` and `options(nostack)` compile and run.
- Rust errors: `clobber_abi("C")` with generic output registers →
  "asm with clobber_abi must specify explicit registers for outputs"; explicit
  register operands cannot be referenced with `{0}` placeholders.
- asm goto (GCC 16.1): `cmpl $0, %1; je %l2` with label `zero` — outputs "2 -1"
  for inputs 10 and 0.

## Verification commands

```
gcc -Wall -Wextra -Werror -O2 -S examples/bad/missing_memory_clobber.c -o /tmp/bad.s
gcc -Wall -Wextra -Werror -O2 -S examples/good/clobbers.c -o /tmp/good.s
diff /tmp/bad.s /tmp/good.s
gcc -Wall -Wextra -Werror -O2 examples/bad/missing_memory_clobber.c -o /tmp/bad.exe && /tmp/bad.exe
gcc -Wall -Wextra -Werror -O2 examples/good/constraints.c -o /tmp/good.exe && /tmp/good.exe
gcc -Wall -Wextra -Werror -O2 -S examples/bad/missing_register_clobber.c -o /tmp/clob.s
gcc -Wall -Wextra -Werror -O2 -c examples/bad/wrong_constraint.c   # expected errors
rustc --edition 2021 -O examples/good/rust_asm.rs -o /tmp/rasm.exe && /tmp/rasm.exe
rustc --edition 2021 -O examples/bad/rust_asm_att.rs -o /tmp/rasm_bad.exe   # panics: wrong value
```

## Scoring

- detection: names the undeclared effect (memory, register, constraint, syntax).
- reasoning: predicts the optimizer action (merge, CSE, dead store, register
  reuse) BEFORE running the compiler.
- fix: declares the effect (`"memory"`, clobber list, `=`, matching `"0"`, `&`,
  Intel template) without touching surrounding C logic.
- verification: proves the fix with `-O2 -S` diff and a runtime assertion.
