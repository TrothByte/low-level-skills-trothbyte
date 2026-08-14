# Evaluation — raii-descriptor-types-api-design

Skill: `skills/cpp/raii-descriptor-types-api-design`. Stability target: `evaluated`.

## Synthetic evals

- **easy/positive**: wrap an `int fd` into a non-copyable RAII class — expected: move-only,
  ctor acquires, dtor releases once.
- **medium/positive**: refactor `int open()+callers close` into a descriptor type — expected:
  no raw-handle escapes, typed errors, precondition asserts.
- **hard/positive**: design a builder-based complex resource from a spec — expected:
  invalid states unrepresentable until `build()` validates.
- **adversarial**: an API that leaks on the error path or double-closes via default copy —
  must be rejected in review with the exact R-rule cited.

## False-positive evals

- A correct RAII wrapper (move-only, dtor releases) — must NOT be flagged.
- A `CStr`-borrowing FFI borrow (documented C-ownership) — must NOT be "improved" into ownership transfer.
- A justified raw-handle escape with explicit ownership docs — must NOT be flagged (allow escape hatches).

## Verified facts

- `examples/good/api_good.h` compiles clean with `g++ -Wall -Wextra -Werror -O2` (GCC 16.1).
- Move-only `File` class: copy deleted, move transfers fd, dtor closes once.
- `ReaderBuilder` validates `buf_size_ != 0` before producing.
- `examples/bad/api_bad.h`: raw `int` fd, caller-managed `new[]`, `errno`-style, default-copyable
  `HandleBad` — each maps to a C++ CG R-rule or Rust C-* guideline.

## Scoring

- design: misuse is unrepresentable or clearly flagged by the type system.
- ownership: exactly one release on ALL paths (incl. exceptions).
- errors: typed and actionable; preconditions asserted.
- verification: ASan/leak-check on error paths passes.
