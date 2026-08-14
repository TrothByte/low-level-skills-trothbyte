# Evaluation — cpp-move-semantics

Skill: `skills/cpp/cpp-move-semantics`. Stability target: `evaluated`.

## Synthetic evals

- **easy/positive**: `std::move` a `std::string`, then reassign the source before reading it —
  expected: reviewer accepts the reset and explains the moved-from state is valid-but-unspecified.
- **medium/positive**: a class with a user-declared destructor and no move ops; `std::move(x)`
  is used to initialize another instance — expected: reviewer identifies that the copy ctor runs
  (implicit moves suppressed by the dtor, [class.copy.ctor] p6) and adds explicit `noexcept`
  moves or rule-of-five.
- **medium/positive**: `return std::move(local);` in a function returning by value — expected:
  reviewer flags the anti-pattern (blocks NRVO, `-Wpessimizing-move`) and rewrites to
  `return local;`.
- **hard/positive**: a forwarding template `template<class T> void f(T&& x)` implemented with
  `std::move(x)` — expected: reviewer replaces with `std::forward<T>(x)` and explains the value
  category is preserved.
- **hard/positive**: return-by-value chain with print-instrumented ctor — expected: at `-O2`
  zero ctor calls (NRVO), with `-fno-elide-constructors` exactly one move, never a copy.
- **adversarial**: `std::move` applied to a `const` object — expected: the copy ctor is
  selected, no move happens; `std::move` on a parameter that is later re-read — expected:
  flagged as use-after-move.
- **adversarial**: moved-from `unique_ptr` dereferenced — expected: null-deref (UB), not
  "unspecified value".

## False-positive evals

- A correct rule-of-five class with `noexcept` move ctor/assign — must NOT be flagged as missing
  move or use-after-move.
- `return local;` returning by value — must NOT be flagged as pessimistic or copying.
- `std::forward<T>(x)` in a forwarding template — must NOT be flagged as "unconditional move".
- Moving an object and resetting it before the next read — must NOT be flagged as use-after-move.
- Copying a value type that deliberately deletes moves (`= delete`) and relies on copies — must
  NOT be flagged as "missing move".

## Verification commands (g++ 16.1, MinGW)

```
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/bad/use_after_move.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/bad/missing_move_ctor.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/bad/return_std_move_local.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/use_after_move_fixed.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/move_ctor_implemented.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/return_by_value.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 -fno-elide-constructors examples/good/return_by_value.cpp -o out2 && ./out2
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/perfect_forwarding.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/move_only_type.cpp -o out && ./out
```

Expected results (actual recorded run, see "Verified facts"):

- bad/use_after_move: compiles clean — GCC emits NO warning (clang-tidy
  `bugprone-use-after-move` is the canonical detector). Runs, prints
  `moved-from string size: 0`, `q value: 7`, then access violation on `*p`
  (exit 0xC0000005).
- bad/missing_move_ctor: compiles clean, prints `copy ctor: std::move did NOT move` —
  proves implicit moves are suppressed by the user-declared destructor.
- bad/return_std_move_local: does NOT compile under `-Werror`
  (`-Werror=pessimizing-move`); compiled with `-Wno-pessimizing-move` it runs and prints
  `len: 7`.
- good/return_by_value: at `-O2` prints only `done` (NRVO fully elides); with
  `-fno-elide-constructors` prints `move ctor` then `done`.
- All other good examples: clean under `-Wall -Wextra -Werror -O2`, exit 0.

## Verified facts (recorded run, GCC 16.1 MinGW, 2026-08-14)

- `use_after_move.cpp`: compile OK with `-Wall -Wextra -Werror -O2`, zero warnings —
  confirmed GCC does NOT diagnose use-after-move; clang-tidy was NOT available on PATH
  (recorded), so the detection claim is sourced to `bugprone-use-after-move` docs, not to a
  local run. Runtime: printed `moved-from string size: 0 (valid but unspecified)` and
  `q value: 7`, then crashed with exit 0xC0000005 (-1073741819, access violation) evaluating
  `*p`; the final `deref moved-from unique_ptr: ...` line never printed.
- `missing_move_ctor.cpp`: compile OK; output `default ctor`, `copy ctor: std::move did NOT
  move`, `b.id = 1`, two `dtor` lines; exit 0 — the user-declared destructor suppressed the
  implicit move ctor and `std::move` selected the copy.
- `return_std_move_local.cpp`: `-Werror` compile FAILS with
  `error: moving a local object in a return statement prevents copy elision
  [-Werror=pessimizing-move]` and a GCC note `remove 'std::move' call`. With
  `-Wno-pessimizing-move`: compile OK, output `len: 7`, exit 0.
- `use_after_move_fixed.cpp`: compile OK; output `t=payload s=fresh`,
  `p is null after move (checked before deref)`, `q value: 7`; exit 0.
- `move_ctor_implemented.cpp`: compile OK; output `move ctor`, `a.data=0000000000000000
  b.n=1000`, `move assign`; exit 0 — the move ctor ran and left the source with
  `data == nullptr` (valid, specified state).
- `return_by_value.cpp`: at `-O2` output is `done` only (no ctor calls — NRVO elided the move);
  with `-fno-elide-constructors` output is `move ctor` then `done` (one move, no copy). Both
  exit 0.
- `perfect_forwarding.cpp`: compile OK; output `sink copied: hello` then `sink moved: tmp` —
  `std::forward` preserved the caller's value category.
- `move_only_type.cpp`: compile OK; output `q=7 p=0000000000000000` — `unique_ptr` source is
  guaranteed empty after the move; exit 0.

## Scoring

- rvalue refs: no dangling `T&&` returns; `std::move` recognized as a value-category cast.
- move ops: intended ctor selected (instrumentation/`is_move_constructible_v`); `noexcept`
  moves for resource-owning types.
- moved-from: no read before reset; no deref of moved-from pointer wrappers.
- elision: `return local;` used; no `std::move` in return statements (`-Wpessimizing-move` gate).
- forwarding: `T&&` + `std::forward<T>` exactly once; no `std::move` on forwarding parameters.
- rule of five: no implicit-move suppression surprises; copy-deleted types moved, not copied.
- False-positive gate: clean rule-of-five, `return local;`, and reset-before-read pass unflagged.
