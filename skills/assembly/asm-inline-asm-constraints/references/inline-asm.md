# Extended Inline Assembly — GCC/Clang & Rust

Rules are stated for GCC/Clang extended asm (`__asm__` / `__asm__ volatile`) and
the Rust `asm!` macro. Verified with GCC 16.1.0 (MinGW x86-64) and rustc 1.97.1
(x86_64-pc-windows-msvc). Format per rule: RULE → WHY AI GETS IT WRONG →
CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. Full asm template structure

- **RULE**: GCC extended asm is `asm [volatile|goto] (template : outputs : inputs
  : clobbers [: goto labels])`. `%0`, `%1`, ... refer to operands in order;
  `%%reg` emits a literal register name such as `%%eax`; `%lN` in an `asm goto`
  names a goto label. `template` is an assembler string; the compiler does NOT
  parse it, so only what the operands and clobbers declare is known to it.
- **WHY AI GETS IT WRONG**: writes `%eax` instead of `%%eax` and the assembler
  errors "bad register name", or assumes the compiler "understands" what the
  template does (it does not).
- **CORRECT REASONING**: every effect of the asm on registers or memory that the
  C/C++ code depends on must be declared in outputs, inputs, or clobbers. Undeclared
  effects are invisible to the optimizer and treated as nonexistent.
- **EXAMPLE** (bad): `__asm__ volatile("movl $2, g")` — writes a global that the
  compiler may keep caching (see rule 6).
- **COUNTEREXAMPLE** (good): `__asm__ volatile("movl %1, %0" : "=r"(r) : "r"(x))`.
- **VERIFICATION**: `gcc -Wall -Wextra -Werror -O2 -S`; no assembler errors.
- **SOURCE**: gcc-manual (Extended Asm, Basic Asm); clang-docs (UsersManual, Inline Assembly).

## 2. Output and input operands

- **RULE**: outputs need `"="` (write-only) or `"+"` (read-write) before the
  constraint (`"=r"`, `"+r"`). Inputs use a plain constraint (`"r"`). An output
  without `=`/`+` is a compile error: "output operand constraint lacks '='".
  Outputs must be written before any input is read unless the input is declared
  with a matching constraint (rule 4).
- **WHY AI GETS IT WRONG**: treats an output as if plain `"r"` were enough, or
  writes the output after reading an input that shares the same register.
- **CORRECT REASONING**: GCC may put an input and an output in the same register;
  if the asm writes `%0` before consuming `%1`, the input value is gone. This is
  why the classic idiom moves the input into the output first or uses matching
  constraints.
- **EXAMPLE** (bad): `"=r"(r) : "r"(x)` with template `"movl %1, %0"` where `%0`
  and `%1` may be the same register — undefined result unless a copy is done first.
- **COUNTEREXAMPLE** (good): `"=r"(r) : "r"(x), "0"(x)` — input `"0"` forces the
  same register, which is the documented correct pattern.
- **VERIFICATION**: `gcc -O2 -S`; confirm register allocation overlaps exactly as
  declared.
- **SOURCE**: gcc-manual (Extended Asm: OutputOperands, InputOperands, SimpleConstraints).

## 3. Constraint codes

- **RULE**: `r` = any general register; `m` = memory operand (address allowed);
  `i` = immediate integer constant; `a`/`b`/`c`/`d`/`S`/`D` = the named register
  (rax, rbx, rcx, rdx, rsi, rdi); `q` = any of a/b/c/d; `g` = general reg,
  memory, or immediate; `0`-`9` = matching operand number; `%` = commutative.
  A constraint that cannot be satisfied with the given expression is a compile
  error: "impossible constraint in 'asm'".
- **WHY AI GETS IT WRONG**: uses `"i"` for a runtime value, `"r"` where the
  hardware requires a specific register (`"c"` for `mul`/shift counts), or `"m"`
  where a register is mandatory.
- **CORRECT REASONING**: the constraint is a contract between C expression and
  instruction operand. If the ISA needs the count in `cl`, the constraint must be
  `"c"` (and the template must print the 8-bit name, rule 10). For `mull` the
  multiplier must be in `eax`/`edx`, so `"a"`/`"d"` are required.
- **EXAMPLE** (bad): `: "i"(x)` with a runtime `x` — "impossible constraint in
  'asm'".
- **COUNTEREXAMPLE** (good): `"=A"(r) : "r"(b), "a"(a)` for `mull %1` (GCC 16.1
  verified: computes the full 64-bit product).
- **VERIFICATION**: `gcc -Wall -Wextra -Werror -O2 -c` must succeed; inspect `-S`.
- **SOURCE**: gcc-manual (Constraints, Machine Constraints, SimpleConstraints); intel-sdm Vol.2 (instruction forms).

## 4. Matching constraints and non-commutativity

- **RULE**: a digit `N` makes this operand use the same register (or memory) as
  operand `N`, which is how an input that must not be clobbered by the output is
  bound. `%` marks two operands as commutative and lets the compiler swap them.
- **WHY AI GETS IT WRONG**: writes an input into a matching operand without
  declaring it (`"0"(x)`), or assumes an instruction is commutative when only one
  operand order is correct (e.g. `sub`, `div`), or declares `"+"` AND a matching
  digit for the same operand ("inconsistent operand constraints").
- **CORRECT REASONING**: matching constraints remove the implicit copy but bind
  the output register, so the template must consume the input before writing the
  output. Commutativity must be declared with `%`, never assumed, otherwise a
  `sub %1, %0` becomes `%1 - %0` when the compiler swaps and the result flips sign.
- **EXAMPLE** (bad): `asm("subl %1, %0" : "+r"(r) : "r"(b), "0"(a))` — `+` and
  `"0"` conflict (GCC: "inconsistent operand constraints in an 'asm'").
- **COUNTEREXAMPLE** (good): `asm("addl %1, %0" : "=r"(r) : "r"(b), "0"(a))` —
  GCC 16.1 verified correct.
- **VERIFICATION**: `gcc -O2 -S` and a runtime assertion of the result.
- **SOURCE**: gcc-manual (Extended Asm, SimpleConstraints, Modifiers); clang-docs (Inline Asm).

## 5. Early-clobber `&`

- **RULE**: `"=&r"` tells GCC the output is written before all inputs are read,
  so it must not share a register with any input. Without `&`, GCC may reuse an
  input register for the output, destroying the still-needed input.
- **WHY AI GETS IT WRONG**: assumes the output register is always fresh; the
  optimizer then aliases an input and the asm reads the overwritten value.
- **CORRECT REASONING**: use `&` whenever the output write can precede the last
  input read, typically in multi-instruction templates.
- **EXAMPLE** (bad): `asm("xorl %1, %0\n\tmovl %2, %1" : "=r"(out) : "r"(x), "r"(y))`
  — `%1` (x) may be the same register as `%0` and is clobbered by the first
  instruction before the second reads `%2`.
- **COUNTEREXAMPLE** (good): `"=&r"(out)` forces a register distinct from all inputs.
- **VERIFICATION**: `gcc -O2 -S`; check the allocated registers are distinct.
- **SOURCE**: gcc-manual (Extended Asm: Clobbers and Scratch Registers, Modifiers).

## 6. Why the "memory" clobber matters

- **RULE**: `"memory"` in the clobber list tells the compiler the asm may read or
  write any memory, so it must not keep cached values in registers and must not
  move loads/stores across the asm. Without it, the compiler assumes the asm does
  not touch memory and reorders, merges, or deletes surrounding accesses.
- **WHY AI GETS IT WRONG**: "my asm only touches the pointer I passed", so no
  clobber is needed — but the pointer is passed as a register `"r"`/`"i"` input,
  and memory reached through it is invisible to the optimizer.
- **CORRECT REASONING**: only `"m"` operands or the `"memory"` clobber make memory
  effects visible. A `"r"(&x)` input declares the address is read, NOT that the
  pointed-to memory is touched.
- **EXAMPLE** (bad, verified GCC 16.1, runtime result wrong): the asm writes 2
  through `"r"(&g)` but `g` is re-read after; without `"memory"` the compiler
  collapses both reads of `g` into one load and the function returns 2 instead of
  3. With `"memory"` it returns 3.
- **COUNTEREXAMPLE** (good): `__asm__ volatile("movl $2, (%0)" : : "r"(&g) : "memory");`
- **VERIFICATION**:
  ```
  gcc -O2 -S missing_memory_clobber.c   # one load: addl %eax,%eax / store merged
  gcc -O2 -S clobbers.c                 # two loads, two stores preserved
  ```
  Verified: `worker` collapses `*p=1; asm; *p=2` to a single `movl $2,(%rcx)`;
  `worker` with `"memory"` keeps both stores.
- **SOURCE**: gcc-manual (Extended Asm: Clobbers, asm volatile); clang-docs (Inline Assembly); llvm-langref (inline asm semantics).

## 7. Register clobber lists

- **RULE**: every register destroyed by the asm must be named in the clobber
  list (`: : : "rax", "cc", "memory"`). Callee-saved registers (rbx, r12-r15,
  rbp, rsp on SysV) are spilled around the asm. `"cc"` covers condition flags.
- **WHY AI GETS IT WRONG**: forgets to list a register the template writes
  directly (`%%eax`); the compiler then keeps a live value in it across the asm
  and returns the corrupted value.
- **CORRECT REASONING**: the compiler allocates other values into any register it
  believes is untouched. An unlisted clobber is a silent miscompile, not a
  warning.
- **EXAMPLE** (bad, verified GCC 16.1 -S): `__asm__ volatile("xorl %%eax, %%eax");`
  compiled as `movl %ecx,%eax; xorl %eax,%eax; ret` — the value in eax is zeroed.
- **COUNTEREXAMPLE** (good): same asm with `: : : "eax", "cc"` compiles to
  `xorl %eax,%eax; movl %ecx,%eax; ret` — value preserved.
- **VERIFICATION**: `gcc -O2 -S` and diff the two versions; run with a runtime
  input.
- **SOURCE**: gcc-manual (Extended Asm: Clobbers); sysv-amd64-abi (3.2.1 register usage, callee-saved).

## 8. volatile asm and `__asm__ __volatile__`

- **RULE**: `volatile` (or `__volatile__`) forbids deleting or moving the asm and
  forbids reordering it against other volatile operations; it does NOT add a
  memory barrier. Without `volatile`, an asm with no outputs may be removed as
  dead code if the compiler sees no reason to keep it.
- **WHY AI GETS IT WRONG**: believes `volatile` also acts as a full compiler
  barrier, or omits it and wonders why the asm vanished.
- **CORRECT REASONING**: `asm volatile("" ::: "memory")` is the full barrier;
  `asm volatile("")` alone only pins the instruction. `volatile` never implies
  ordering of ordinary (non-volatile) C memory accesses unless `"memory"` is also
  clobbered.
- **EXAMPLE** (bad): `__asm__("")` in a loop hoping to prevent a store merge — GCC
  16.1 deletes the asm and merges the stores (see rule 6).
- **COUNTEREXAMPLE** (good): `__asm__ volatile("" ::: "memory")` between two stores.
- **VERIFICATION**: `gcc -O2 -S`; compare with and without `volatile`/`"memory"`.
- **SOURCE**: gcc-manual (Extended Asm: Volatile); clang-docs (Inline Assembly); llvm-langref.

## 9. asm goto

- **RULE**: `asm goto` adds a list of C labels after the clobbers and branches to
  them with `%lN` (or `%l[name]`). Goto asm cannot use outputs (GCC/Clang); use
  inputs, clobbers, and `volatile`. The fall-through path continues normally.
- **WHY AI GETS IT WRONG**: tries to write an output in an asm goto (compile
  error) or uses `%` instead of `%l` for the label.
- **CORRECT REASONING**: `asm goto("cmpl $0, %1\n\tje %l2" : : "r"(x) : "cc" : zero)`
  jumps to `zero:` when the input is 0 (GCC 16.1 verified: prints `2 -1`).
- **EXAMPLE** (bad): `asm goto(... : "=r"(out) ... : : ... : lbl)` — outputs are
  not allowed with goto.
- **COUNTEREXAMPLE** (good): compute results only on the fall-through path.
- **VERIFICATION**: compile and run with both taken and not-taken paths.
- **SOURCE**: gcc-manual (Extended Asm: GotoLabels); clang-docs (Inline Assembly).

## 10. Operand modifiers (size suffix control)

- **RULE**: a letter after the operand number changes how the register is printed:
  `%b0` = 8-bit name (`%al`), `%w0` = 16-bit (`%ax`), `%k0` = 32-bit (`%eax`),
  `%q0` = 64-bit (`%rax`). The default prints the register matching the operand's
  size. Shift/rotate counts are only encodable as `cl`, so the template must print
  the 8-bit name: `shll %b1, %0`.
- **WHY AI GETS IT WRONG**: writes `shll %1, %0` with a `"c"` constraint; GCC
  emits the 32-bit name and the assembler rejects it ("operand type mismatch
  for 'shl'").
- **CORRECT REASONING**: the constraint picks the register (ecx), the modifier
  picks the printed size (cl). Both are needed for variable shifts.
- **EXAMPLE** (bad): `__asm__ volatile("shll %1, %0" : "+r"(x) : "c"(n));`
- **COUNTEREXAMPLE** (good): `__asm__ volatile("shll %b1, %0" : "+r"(x) : "c"(n));`
  — GCC 16.1 verified: `shift_left(1, 31) == 0x80000000`.
- **VERIFICATION**: `gcc -Wall -Wextra -Werror -O2` and runtime assertion.
- **SOURCE**: gcc-manual (Extended Asm: x86 operand modifiers); intel-sdm Vol.2 (SHL/SHR encodings).

## 11. Rust asm! basics

- **RULE**: Rust `asm!` uses Intel syntax on x86/x86-64 (destination operand
  first), which differs from GCC's AT&T syntax. Operand classes: `out(reg)` (write-only),
  `in(reg)` (read-only), `inout(reg)` (read-write, like `+r`), `lateout(reg)`
  (write-only, written last), `inlateout(reg)`, `const N` (immediate, no
  register), and `name = const expr`. `clobber_abi("C")` clobbers all caller-saved
  registers of the ABI and requires explicit register operands. `options(nostack)`
  promises no stack use.
- **WHY AI GETS IT WRONG**: carries GCC AT&T templates into Rust and silently
  computes the wrong value (Intel operands are reversed), or omits `options`,
  or ignores the `asm_sub_register` warning on 32-bit operands.
- **CORRECT REASONING**: write the template for the target syntax, put the
  destination first in Intel form, and use `{N:e}` (e.g. `eax`) vs `{N:r}` (rax)
  formatting deliberately for the operand width.
- **EXAMPLE** (bad, verified rustc 1.97.1): `asm!("add {1:e}, {0:e}", inout(reg) r,
  in(reg) b)` returns `a` instead of `a + b` — Intel reads this as `b += r`.
- **COUNTEREXAMPLE** (good, verified): `asm!("add {0:e}, {1:e}", inout(reg) r,
  in(reg) b)` returns 42; `asm!("add {0}, {number}", inout(reg) x, number = const 5)`
  returns 8; `lateout(reg)` with `"pushfq"/"pop {0}"` works.
- **VERIFICATION**:
  ```
  rustc --edition 2021 -O rust_asm.rs && ./rust_asm   # good: "rust asm! ok"
  rustc --edition 2021 -O rust_asm_att.rs && ./rust_asm_att  # bad: panics (wrong value)
  ```
- **SOURCE**: rust-reference (inline assembly); clang-docs; rustonomicon (FFI and inline assembly caveats).

## 12. Common bug catalog

- **RULE**: the repeatable failure modes are: (1) missing `"memory"` clobber for
  asm that touches memory; (2) missing register clobbers for registers the
  template writes; (3) wrong constraint code (`"i"` for runtime values, missing
  `=`, `"c"` vs `"r"`); (4) AT&T vs Intel syntax mismatch when porting templates;
  (5) assuming commutative operands without `%`; (6) forgetting `volatile` for
  side-effect asm with no outputs.
- **WHY AI GETS IT WRONG**: each bug compiles clean (or errors confusingly) and
  only manifests as a wrong value at runtime or a register-allocator surprise in
  `-O2 -S`.
- **CORRECT REASONING**: treat inline asm as an opaque black box: every
  side effect must be declared. Verify with `-O2 -S` diffs and runtime
  assertions, never with "it compiled".
- **EXAMPLE** (bad): any of the examples under `examples/bad/`.
- **COUNTEREXAMPLE** (good): the corresponding `examples/good/` file with the
  constraint, clobber, or syntax fixed.
- **VERIFICATION**: compile both, diff `-O2 -S` output, compare runtime results.
- **SOURCE**: gcc-manual; clang-docs; rust-reference; empirical GCC 16.1 / rustc 1.97.1.
