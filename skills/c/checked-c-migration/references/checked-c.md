# Checked C — Migration Reference

Sources: Checked C specification (checkedc.md), C to Checked C by 3C (arXiv 2203.13445),
A Formal Model of Checked C (arXiv 2201.13394), Checked C clang fork.
Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → VERIFICATION → SOURCE.

## 1. Checked pointer types: _Ptr vs _Array_ptr vs _Nt_array_ptr

- **RULE**: `_Ptr<T>` is a non-null pointer to a single object: no pointer arithmetic, no
  subscripting. `_Array_ptr<T>` is a pointer to a contiguous array and requires a bounds
  declaration; it supports arithmetic and subscripting within those bounds.
  `_Nt_array_ptr<T>` is a pointer to a null-terminated array whose bounds are derived from
  the NUL terminator (`p..p+strlen(p)+1`). All three are checked pointer types: the compiler
  inserts runtime bounds checks on dereference and rejects provably-bad code statically.
- **WHY AI GETS IT WRONG**: "every pointer is `_Ptr`" — over-annotation. A loop `for (i...) a[i]`
  on a `_Ptr` is a compile error, so the agent then inserts casts instead of re-classifying.
- **CORRECT REASONING**: classify by USE, not by gut: does this pointer ever advance,
  subscript with `i > 0`, or get a `count`? If it only ever dereferences `*p` (or `p[0]`)
  and is never incremented → `_Ptr`. If it walks an array whose size is known → `_Array_ptr`
  + `count(n)`. If it is a C string that must be NUL-terminated and the termination is
  provable → `_Nt_array_ptr`. A null value is not representable in any checked pointer
  (they are non-null by default); use `_Array_ptr` with `bounds(none)` + `_Dynamic_check`
  or an explicit null test for nullable data.
- **EXAMPLE**: `int *p = malloc(n * sizeof(int)); ... p[i]` → `_Array_ptr<int> p : count(n)`
  with the loop rewritten to iterate `[0, n)`. `int *x = &obj; ... *x` → `_Ptr<int> x`.
- **VERIFICATION**: compile with `clang -fcheckedc-extension`; the checker errors on
  arithmetic/subscript over `_Ptr` and on unproven bounds.
- **SOURCE**: checkedc.md §2.3, §2.4 (checked pointer types).

## 2. Bounds declarations: count(n) vs byte_count(n) vs bounds(none)

- **RULE**: `count(n)` declares n ELEMENTS; `byte_count(n)` declares n BYTES;
  `bounds(lo, hi)` declares the half-open address range `[lo, hi)`. `bounds(none)` declares
  that no static bound is tracked and every use requiring a bound must be guarded by a
  `_Dynamic_check`. Relative modifiers (`_Relative_alignment`) can appear with
  `_Array_ptr`/`_Nt_array_ptr` bounds.
- **WHY AI GETS IT WRONG**: `count(sizeof(buf))` — counting bytes where elements are needed
  (the compiler then rejects `buf[i]` for `i > 1`, or the agent changes the count and creates
  an off-by-size hole). Conversely `byte_count(n)` used to index a `struct`/`int` array with
  non-byte elements.
- **CORRECT REASONING**: express the bound in the same unit as the access. A buffer of
  `int` of capacity `cap` bytes holds `cap / sizeof(int)` elements:
  `_Array_ptr<int> p : count(cap / sizeof(int))` (verify no remainder first). For raw byte
  copies only (`memcpy`/`memmove`) `byte_count(n)` is right. Prefer the SPECIFIC bound that
  the real allocation and all accesses share, derived from the caller's actual count.
- **EXAMPLE**: `_Array_ptr<char> dst : byte_count(size) = ...; memcpy(dst, src, size);` is
  sound for a byte buffer; indexing `dst[i]` beyond element 0 is NOT covered by byte_count
  unless size semantics are byte elements.
- **VERIFICATION**: clang `-fcheckedc-extension` bounds checker + a runtime OOB probe
  (`-DOOB_PROBE` build in `examples/good/annotated_checkedc.c`).
- **SOURCE**: checkedc.md §3.5 (bounds expressions), §3.5.6 (bounds(none)).

## 3. _Nt_array_ptr requires provable NUL termination

- **RULE**: initializing or assigning an `_Nt_array_ptr` requires the value to be provably
  null-terminated. The derived bounds are `p..p+strlen(p)+1`; any read past the NUL (including
  `strlen` scanning and `%s`-style consumers) is covered only if termination is real.
- **WHY AI GETS IT WRONG**: annotating `char *` parameters that may alias mid-buffer data or
  fixed-width (non-terminated) fields as `_Nt_array_ptr`. It compiles only when the checker
  can prove termination; otherwise the agent casts, silently keeping a non-terminated buffer
  inside an nt-typed pointer.
- **CORRECT REASONING**: for a string whose length is known (`len = strlen(s)`), the correct
  bound is `count(len + 1)` via `_Nt_array_ptr`, which is provable only if the source is
  itself provably terminated (string literal, earlier `_Nt_array_ptr`, or
  `s[len] = '\0'` before the assignment). If termination cannot be established in a checked
  scope, refactor: allocate one extra byte and force `s[len] = '\0'`, or use
  `_Array_ptr<char> : count(n)` with an explicit length parameter and `_Dynamic_check` that
  `n > 0 && s[n-1] == '\0'`.
- **EXAMPLE**: `char *s = argv[1];` from unchecked code → at the boundary the migration must
  prove `s` is terminated (argv strings are, by contract) or copy it into a checked buffer
  with a forced terminator before annotating `_Nt_array_ptr`.
- **VERIFICATION**: checker error "cannot prove bounds" on non-terminated assignment;
  runtime `strlen` probe must stop exactly at the NUL.
- **SOURCE**: checkedc.md §2.4.2 (_Nt_array_ptr), §3.5.5 (bounds from NUL terminator).

## 4. Runtime-dependent bounds need _Dynamic_check, not static lies

- **RULE**: `_Dynamic_check(e)` inserts a runtime test; on failure it invokes the failure
  handler (default: traps). When a bound depends on a value the compiler cannot infer
  (parsed lengths, network headers), the static checker cannot prove the bound and the
  correct annotation is `bounds(none)` + `_Dynamic_check` immediately before each use, or
  `_Dynamic_bounds_cast` for deliberately narrowed bounds after a check.
- **WHY AI GETS IT WRONG**: either the agent writes a fake static `count(huge)` to make the
  checker accept, or it omits the runtime guard entirely and the compile fails; then it
  blames the tool. Both produce unsound programs.
- **CORRECT REASONING**: the runtime check IS the annotation's enforcement. Validate the
  value once at the boundary (`if (len > cap) return error;`) then declare the proven bound.
  Prefer restructuring so the bound is data-derived (`count(len)` where `len <= cap` proven
  by a prior check) over `bounds(none)` sprinkles.
- **EXAMPLE**: `size_t n = header->len;` (unchecked input) → validate `n <= CAP` with
  `_Dynamic_check(n <= CAP)` or an ordinary `if`, then use `_Array_ptr<...> : count(n)`.
- **VERIFICATION**: run with a crafted input where `len` exceeds `CAP`; the check must fire.
- **SOURCE**: checkedc.md §3.8 (_Dynamic_check), §3.7 (bounds casts).

## 5. Checked scopes and checked blocks

- **RULE**: a `_Checked` function, block, or `_Checked {...}` compound statement requires
  every pointer declared inside to be a checked pointer, and all operations inside to respect
  checked semantics. Unchecked code interops with checked code at function boundaries: checked
  pointers have the same representation as ordinary pointers (binary-compatible), so an
  unchecked function can receive a checked pointer and a checked function can be called with
  ordinary pointers only through explicit interop conversions (checked ↔ unchecked casts at
  the boundary).
- **WHY AI GETS IT WRONG**: assuming a `_Checked` function makes its unchecked CALLERS safe,
  or that checked blocks can freely contain raw `char *` locals (they cannot), or declaring
  the whole file `_Checked` and breaking every call site in one commit.
- **CORRECT REASONING**: migrate leaves first, roots last. Keep the checked region small per
  step; every pointer that crosses the boundary gets a checked/unchecked interop conversion
  with documented reasoning. The safety guarantee holds inside checked code; unchecked code
  remains its own problem (see skill `c-undefined-behavior`).
- **EXAMPLE**: `_Checked { _Array_ptr<int> p : count(n) = ...; ... }` inside one function,
  while the rest of the module stays unchecked.
- **VERIFICATION**: `clang -fcheckedc-extension` accepts the region; an OOB probe inside the
  region traps.
- **SOURCE**: checkedc.md §2.8 (checked scopes), §4 (interoperation with unchecked code).

## 6. Migration workflow and 3C

- **RULE**: incremental migration is the supported path: convert one function/module,
  compile-check, then widen. 3C (`3c/src/3c.py`) automates the first pass: typ3c infers
  pointer types, boun3c infers bounds, and the tool reports root causes — the code sites
  (e.g. `p++` in loops, aliasing, missing sizes) that block provable bounds and must be
  refactored. The 3C paper evaluates on 11 programs totalling ~319 KLOC, converting a large
  share automatically and leaving a small, human-attended remainder (root causes).
- **WHY AI GETS IT WRONG**: presenting a single-shot annotation dump as a finished migration
  without running the compiler, or ignoring root-cause reports and cast-heavy fixes instead
  of refactoring the size plumbing.
- **CORRECT REASONING**: the loop is annotate → compile → read root causes → refactor →
  re-annotate → recompile, with the compiler as the oracle. Keep a plain-C control build for
  behavioral equivalence (see `examples/good/incremental_step.c`) so the migration never
  silently changes behavior while chasing bounds.
- **EXAMPLE**: a function that takes `int *buf` without a length cannot be checked; the root
  cause fix is to add the size parameter, then `_Array_ptr<int> buf : count(len)`.
- **VERIFICATION**: `python3 3c/src/3c.py file.c`, then `clang -fcheckedc-extension` on the
  result; every root-cause item either refactored or documented as a runtime-checked fallback.
- **SOURCE**: arXiv 2203.13445 (3C).

## 7. What the formal model guarantees

- **RULE**: the formal model (Coq, arXiv 2201.13394) proves spatial memory safety for fully
  checked code and characterizes the failure conditions at unchecked interop. A program that
  is entirely checked has no out-of-bounds memory access; the only way to break safety is
  through an unchecked path (unchecked code, unchecked casts, `bounds(none)` uses without
  runtime checks).
- **WHY AI GETS IT WRONG**: "we migrated with Checked C, so it is now memory-safe" — true only
  for the checked subset; an unchecked callee or a silenced cast reopens the hole.
- **CORRECT REASONING**: count the unchecked boundaries and treat each as a documented risk
  item. The goal of migration is to minimize the unchecked surface, not to declare victory at
  the first clean compile.
- **VERIFICATION**: audit that every cast is a documented interop conversion and every
  `bounds(none)` use is `_Dynamic_check`-guarded.
- **SOURCE**: arXiv 2201.13394.

## 8. LLM-assisted annotation

- **RULE**: Microsoft Research has demonstrated LLM-assisted migration to memory-safe C
  dialects: an LLM proposes checked-pointer and bounds annotations for legacy code, and the
  checker (clang `-fcheckedc-extension`) filters the proposals. The pattern is propose →
  compile → repair loop, exactly what this skill encodes.
- **WHY AI GETS IT WRONG**: an LLM (or agent) presents annotations without a compiler run and
  without the runtime OOB probe; unproven bounds are indistinguishable from proven ones on
  inspection.
- **CORRECT REASONING**: LLM-proposed annotations are hypotheses with no authority until
  `clang -fcheckedc-extension` accepts them AND a runtime probe fires the checks. Use the
  inference model in `examples/tools/annotation_infer.py` as a fast host-side filter before
  the target compiler run.
- **VERIFICATION**: annotate → compile → probe; see `evals/README.md`.
- **SOURCE**: Microsoft Research on LLM-assisted migration to memory-safe C dialects.
