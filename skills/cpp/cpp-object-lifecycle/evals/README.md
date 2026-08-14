# Evaluation — cpp-object-lifecycle

Skill: `skills/cpp/cpp-object-lifecycle`. Stability target: `evaluated`.

## Synthetic evals

- **easy/positive**: order a base + two members and a ctor body with print statements —
  expected: base ctor, member1 ctor, member2 ctor, body; destruction in reverse.
- **medium/positive**: derive a class and call a virtual from the base constructor —
  expected: output is the base implementation ("Base"), and reviewer explains why.
- **medium/positive**: two translation units where TU A's global ctor reads TU B's
  global — expected: reviewer identifies the SIOF and replaces it with a
  function-local static.
- **hard/positive**: moved-from `std::string` then reuse without reset; moved-from
  `unique_ptr` dereference — expected: unspecified-value and null-deref diagnosed.
- **adversarial**: a destructor that throws during stack unwinding (second exception
  in flight) — expected: `std::terminate`, never "handled".

## False-positive evals

- A Rule-of-Five class with `noexcept` move ctor/assign and correct copy — must NOT
  be flagged as use-after-move or missing move.
- A Meyers singleton (`static T& get() { static T t; return t; }`) — must NOT be
  flagged as SIOF.
- A destructor declared `noexcept` that catches a thrown cleanup error — must NOT be
  flagged as "throws from destructor".
- A virtual call from a NON-constructor member function — must NOT be flagged as
  "virtual call in ctor".
- Returning a value (not a reference) from a function — must NOT be flagged as
  dangling.

## Verification commands (g++ 16.1, MinGW)

```
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/bad/virtual_call_in_ctor.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/bad/static_order_a.cpp examples/bad/static_order_b.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/bad/use_after_move.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/bad/dtor_throw.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/virtual_call_after_ctor.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/static_init_meyers.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/move_reset_before_use.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 examples/good/dtor_noexcept.cpp -o out && ./out
```

Expected results (actual recorded run, see "Verified facts"):

- bad/virtual_call_in_ctor: prints `Base` (NOT `Derived`).
- bad/static_order: prints `A ctor: read 0` in at least one link order; the good
  Meyers version always prints `42`.
- bad/use_after_move: prints `moved-from string size: 0` then crashes on the null
  deref (no further output; exit 0xC0000005).
- bad/dtor_throw: does NOT compile under `-Werror` (GCC 16 `-Wterminate`: "'throw'
  will always call 'terminate'"); compiled with `-Wno-terminate` it prints
  `terminate called after throwing an instance of 'std::runtime_error'` and exits
  with code 0xC0000409.
- All good examples: clean compile under `-Wall -Wextra -Werror -O2`, exit code 0.

## Verified facts (recorded run, GCC 16.1 MinGW, 2026-08-14)

- `virtual_call_in_ctor.cpp`: compile OK, output `Base`, exit 0 — the virtual call
  from the ctor resolved to the base, proving [class.cdtor] dispatch.
- `virtual_call_after_ctor.cpp`: compile OK, output `Derived`, exit 0.
- `static_order_a.cpp` + `static_order_b.cpp`: order `a.cpp b.cpp` printed
  `A ctor: read 42`; order `b.cpp a.cpp` printed `A ctor: read 0` (trap
  reproduced). Link order on MinGW is reversed from the command line; either way
  at least one order misbehaves, which is the point.
- `static_init_meyers.cpp`: output `A ctor: read 42` / `g_a.seen = 42`, exit 0.
- `use_after_move.cpp`: compile OK; output `moved-from string size: 0`,
  `moved target: payload`, `q value: 7`; then access violation on the moved-from
  `unique_ptr` deref, exit 0xC0000005 (-1073741819). The `deref ...` line never
  printed — the load of `*p` faulted first.
- `dtor_throw.cpp`: `-Werror` compile FAILS with
  `error: 'throw' will always call 'terminate' [-Werror=terminate]` (GCC 12+
  static detection). Compiled with `-Wno-terminate` it runs, prints
  `terminate called after throwing an instance of 'std::runtime_error'` and
  `what():  dtor failure`, exit 0xC0000409 (-1073740791, terminate/abort).
- All four good examples: clean under `-Wall -Wextra -Werror -O2`, exit 0.

## Scoring

- order: base/member/body order and reverse destruction match [class.base.init].
- dispatch: no virtual call from a ctor/dtor resolves to an override whose
  subobject does not exist.
- statics: no cross-TU dynamic-init dependency; first-use construction used.
- move: moved-from objects are reset before reading; null checks precede deref.
- dtor: destructors are `noexcept`-safe; failures reported via explicit close().
- lifetime: no reference/pointer/iterator outlives its object; ASan clean.
