---
name: cpp-move-semantics
description: Use when writing, reviewing, or debugging C++ code involving rvalue references, std::move and std::forward, move constructors and assignment, moved-from state and use-after-move bugs, copy elision and NRVO, the rule of five, or returning objects by value.
---

# C++ Move Semantics & Use-After-Move

## When to use

- Choosing how to pass or return an object (by value, rvalue reference, forwarding reference).
- A class needs move construction/assignment or a rule-of-five decision.
- `std::move` or `std::forward` appears in code and must do the right thing.
- Debugging why "moving" actually copies, or why a moved-from object lost its value.
- Deciding whether a `return` copies, moves, or is elided.

## When not to use

- Lifetime, construction-order, and dangling issues — use `cpp-object-lifecycle`.
- Undefined behavior taxonomy (null-deref UB in general) — use `c-undefined-behavior`.
- Ownership/RAII API design — use `raii-descriptor-types-api-design`.
- Rust ownership — use `rust-unsafe-reasoning`.

## What the agent often gets wrong

- "`std::move(x)` moves x." It is `static_cast<T&&>(x)`; the move ctor/assign does the work.
- "Reading a moved-from object is UB." For std types it is valid-but-unspecified; the bug is
  assuming the old value survives. Dereferencing a moved-from `unique_ptr`/`optional` is a null
  deref.
- "A user-declared destructor does not affect moves." It suppresses the implicit move ops, so
  `std::move` silently falls back to a copy.
- "`return std::move(local);` is the fast way to return." It blocks NRVO and forces a move;
  GCC warns `-Wpessimizing-move`.
- "Returning by value copies." In C++17 prvalues are guaranteed-elided; named locals are NRVO'd
  or moved.
- "A `T&&` parameter moves automatically." A named `T&&` is an lvalue; without
  `std::forward<T>` it does not move.
- "`std::move` is right for forwarding parameters." For a generic `T&&`, `std::move` throws away
  the caller's value category; only `std::forward<T>` is correct there.

## How to reason correctly

1. Identify the value category: lvalue / xvalue / prvalue. `std::move` produces an xvalue; only
   rvalues can select a move ctor.
2. Determine which constructor overload runs: rvalue selects the move ctor (if one exists),
   lvalue the copy ctor, deleted copy is a compile error.
3. Check the class actually HAS a move ctor: any user-declared copy ctor, copy assign, move op,
   or destructor suppresses implicit moves ([class.copy.ctor] p6).
4. After a move the source is valid-but-unspecified (std types) or specified (e.g. `unique_ptr`
   is empty). Reset or overwrite before reading; null-check before deref.
5. `return local;` is correct: prvalues are guaranteed-elided, named locals are NRVO'd or moved.
   `return std::move(local);` pessimizes.
6. In forwarding contexts use `T&&` plus `std::forward<T>` exactly once, never `std::move`.
7. For resource-owning classes apply the rule of five with `noexcept` moves (C.66).

## What to verify

- The intended ctor actually runs (print instrumentation;
  `static_assert(std::is_move_constructible_v<T>)`).
- No read of a moved-from object before reset; no deref of a moved-from pointer wrapper.
- `std::move`/`std::forward` applied to the right objects; no `std::move` on `const`, no
  `std::forward` outside forwarding contexts.
- Return paths rely on elision/move, not on `std::move(local)`.
- Move-only types (copy deleted) are transferred by move, not copied.
- Implicit-move suppression by a user-declared destructor is intentional and checked.

## How to verify

```
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/bad/use_after_move.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/bad/missing_move_ctor.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/bad/return_std_move_local.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/use_after_move_fixed.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/move_ctor_implemented.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/return_by_value.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/perfect_forwarding.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/move_only_type.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 -fno-elide-constructors examples/good/return_by_value.cpp -o out2 && ./out2
```

GCC does NOT diagnose use-after-move (compiles clean under `-Wall -Wextra -Werror`; verified).
Use clang-tidy `bugprone-use-after-move` where available. `return_std_move_local.cpp` fails to
compile under `-Werror` with `-Wpessimizing-move`, proving the anti-pattern; with
`-Wno-pessimizing-move` it runs and performs a redundant move. NRVO: `return_by_value.cpp` at
`-O2` prints no ctor calls (fully elided); with `-fno-elide-constructors` it prints one
`move ctor`.

## Where the knowledge comes from

- ISO C++20 draft N4861: [class.copy.ctor], [class.copy.assign], [class.copy.elision],
  [dcl.init.ref], [class.temporary], [lib.types.movedfrom].
- C++ Core Guidelines: C.20-C.22 (rule of zero/five), C.63-C.66 (move ops, `noexcept`),
  F.19/F.20/F.21/F.45 (forwarding, return-by-value), R.10-R.12 (move-only types).
- cppreference C++ reference (move ctor, moved-from state) and the C++ UB list (null deref).
- clang-tidy checks list: `bugprone-use-after-move`.

## Related skills

- `cpp-object-lifecycle` — lifetime, rule of five, use-after-move context (require of)
- `raii-descriptor-types-api-design` — ownership/RAII API design (recommend)
- `c-undefined-behavior` — null-deref UB taxonomy (recommend)

## Evaluation

Synthetic: use-after-move (moved-from string read, moved-from `unique_ptr` deref), missing move
ctor (user-declared dtor falls back to copy), `return std::move(local)` pessimization,
return-by-value elision at `-O2` vs `-fno-elide-constructors`, perfect forwarding preserving
value category, move-only transfer.
False-positive: correct rule-of-five with `noexcept` moves, `return local;` (must NOT be
flagged), `std::forward` in a forwarding template, reset-before-read after a move — must NOT be
flagged.
Adversarial: `std::move` on a `const` object (copy ctor chosen), `std::move` on a parameter that
is later re-read, forwarding template that uses `std::move` instead of `std::forward`.
