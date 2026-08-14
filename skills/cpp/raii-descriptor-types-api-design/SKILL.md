---
name: raii-descriptor-types-api-design
description: Use when designing NEW C/C++/Rust APIs that wrap resources — file descriptors, sockets, buffers, handles, locks. Teaches positive design patterns: descriptor/newtype types, RAII ownership, typed errors, builders, and debug-asserted preconditions, so the type system makes misuse hard.
---

# RAII & Descriptor-Type API Design (positive patterns)

## When to use

- Designing a resource-owning API (file/buffer/connection/handle wrapper).
- Choosing how to expose ownership, errors, and construction to callers.
- Reviewing whether an existing API lets callers misuse it (raw `int fd`, `void*`, `bool`).

## When not to use

- Fixing a memory bug in existing code — that's `c-undefined-behavior`/`meta-rationalizations`.
- Pure algorithm code without resources.

## What the agent often gets wrong

- Exposes raw handles (`int fd`, `void*`, `T*`) instead of typed wrappers (B8: abstraction-less
  and unhelpful API).
- Uses `bool`/magic ints for states instead of enums/newtypes.
- Hand-writes free/close in every caller instead of RAII (leak/UAF by construction).
- Error model is `int` codes or `errno` instead of typed errors.
- No precondition checks — invariants assumed silently (C-VALIDATE violation).

## How to reason correctly (the positive pattern)

1. **Descriptor/newtype**: wrap the raw handle in a type with valid states (Rust `struct
   File(OwnedFd)` / `C-NEWTYPE`; C++ RAII class; C opaque struct + functions).
2. **RAII by construction**: acquire in constructor, release in destructor; never expose the
   raw handle without an escape hatch that documents ownership (C++ R.12, R.20).
3. **Typed errors**: enum/Result-typed failures; never `errno`-style global state (C-GOOD-ERR,
   E.28).
4. **Preconditions**: `debug_assert` invariants at entry, documented contracts (C-VALIDATE,
   matklad Preconditions).
5. **Builders for complex construction**: simple types via validating constructors, complex
   via builder (C-BUILDER).
6. **Move/copy semantics**: C++ delete copy / implement move for unique resources;
   Rust `!Copy` + `Drop`.

## What to verify

- Every raw-handle escape is justified and ownership-documented.
- Destructor/drop path releases exactly once, including error paths (no leak on exception/early return).
- Invariants hold: `debug_assert` guards are in place and actually check something.
- Errors are typed and actionable, not `-1`/`errno`.

## How to verify

```
# C++: compile with -Wall -Wextra -Werror; run ASan/UBSan; leak-check on error paths
# Rust: clippy -D warnings; test Drop runs on error returns (unit test)
# C: valgrind on error-path tests
```

## Where the knowledge comes from

- C++ Core Guidelines R.1/R.3/R.10-R.23/R.37; Rust API Guidelines C-NEWTYPE/C-CUSTOM-TYPE/
  C-BUILDER/C-VALIDATE/C-GOOD-ERR/C-DTOR-FAIL; matklad Preconditions; Stroustrup RAII.
- Positive-pattern inventory: `roadmap/research-ingestion.yaml` (Блок 5).

## Related skills

- `safe-low-level-from-scratch` (recommend — Step 2 of the process)
- `cpp-object-lifecycle` (require)
- `ffi-boundary-cross-language` (opaque handles at boundaries — extend)

## Evaluation

Synthetic: design a buffered-reader/handle API from a spec; the resulting API must make
double-close/use-after-close unrepresentable or clearly flagged. Positive/negative/ambiguous:
a correct RAII wrapper must pass ASan + leak checks on error paths; a `void*`-leaking design
must be rejected in review.
