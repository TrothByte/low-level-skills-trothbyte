---
name: cpp-object-lifecycle
description: Use when writing, reviewing, or debugging C++ object lifetime issues: constructor/destructor order, virtual calls during construction, static initialization order fiasco, copy vs move semantics, use-after-move, destructor exceptions, dangling references and pointers, and basic.life violations.
---

# C++ Object Lifecycle: Construction, Destruction, and Lifetime Rules

## When to use

- Writing classes with constructors/destructors, bases, or member objects and
  needing correct construction/destruction order.
- Calling virtual functions or `typeid` from a constructor or destructor.
- A constructor reads a global defined in another translation unit (static
  initialization order fiasco).
- Deciding between copy and move, or reading a moved-from object.
- Destructors whose cleanup can fail, or pointers/references/iterators that may
  outlive an object.

## When not to use

- C lifetime rules — use `c-undefined-behavior` / `c-string-and-buffer-safety`.
- Rust ownership — use `rust-unsafe-reasoning`.
- Memory ordering / concurrency — use `memory-ordering-reasoning`.
- RAII ownership API design — use `raii-descriptor-types-api-design`.

## What the agent often gets wrong

- "A virtual call from a constructor reaches the derived override." It resolves to
  the class under construction; the derived subobject does not exist yet.
- "Reading a moved-from object is UB." It is valid-but-unspecified for std types;
  the error is assuming the old value survives. Dereferencing a moved-from
  `unique_ptr`/`optional` is a null deref and crashes.
- "`std::move(x)` moves x." It is only a cast to rvalue; the move ctor/assign (or a
  copy if none exists) does the work.
- "Destructors can report errors by throwing." Destructors are `noexcept` by default;
  an escaping throw calls `std::terminate`.
- "Globals initialize in the order I wrote them, even across files." Within a TU it
  is definition order; across TUs it is unspecified.
- "An object's lifetime is its variable's lifetime." References/pointers to
  destroyed storage dangle; using them is UB.

## How to reason correctly

1. Construction: bases, then members (declaration order), then the body. Destruction
   is exactly the reverse. Only already-constructed subobjects exist at any step.
2. From a ctor/dtor the dynamic type is the class under construction; virtual calls
   resolve to that class. If derived behavior is needed, call the virtual from a
   separate `init()` after the object is fully built.
3. Statics: dynamic init follows definition order within a TU, unspecified across
   TUs — use function-local statics (construct on first use, I.22) or `constinit`.
4. Moved-from objects are valid-but-unspecified; reset or overwrite before reading.
5. Destructors: never let an exception escape; expose a throwing `close()`/`release()`
   and keep `~T()` `noexcept`. During unwinding a second throw is always terminate.
6. Lifetime starts after the constructor completes and ends when the destructor
   starts; use outside that interval is UB ([basic.life]). All references, pointers,
   iterators to it are dead after the destructor.

## What to verify

- Construction/destruction order matches the rules (a print-statement test confirms
  base/member/body order and its reverse).
- No virtual call or `typeid` reached from a ctor/dtor unless the effect (resolving
  to the current class) is intended.
- No cross-TU static-init dependency; function-local statics or `constinit` used.
- Moved-from objects reset before reading; no deref of a moved-from pointer/optional.
- Destructors are `noexcept`-safe; failures reported via an explicit `close()`.
- No reference/pointer/iterator outlives its object (return-by-reference, stored
  references, reallocation, temporaries).

## How to verify

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

Bad virtual-call example prints `Base`, good prints `Derived`. `dtor_throw` does not
compile under `-Werror` (`-Wterminate`); with `-Wno-terminate` it terminates with
"terminate called after throwing...". The SIOF pair misbehaves in at least one link
order (reads `0`); the Meyers good version always prints `42`. All good examples
compile clean and exit 0. False-positive gate: correct Rule-of-Five, Meyers
singleton, and `noexcept`-safe destructors must NOT be flagged.

## Where the knowledge comes from

- ISO C++20 draft N4861: [intro.object], [basic.life], [class.cdtor],
  [class.base.init], [class.dtor], [class.copy.ctor]/[class.copy.assign].
- C++ Core Guidelines: I.22, C.20/C.21/C.30-C.34, R.1/R.37, E.28.
- cppreference C++ UB list ([basic.life] violations, non-live objects).
- Chandler Carruth, "Garbage In, Garbage Out" (why these assumptions compile and then
  break under the optimizer).

## Related skills

- `cpp-move-semantics` — move/copy details and forwarding (require of)
- `raii-descriptor-types-api-design` — ownership and RAII API design (recommend)
- `c-undefined-behavior` — C-side lifetime/UB reasoning (recommend)
- `rust-panic-safety` — cross-language analog of exception-safety guarantees

## Evaluation

Synthetic: construction-order print test, virtual call from ctor, SIOF across two
TUs, moved-from `std::string` size, moved-from `unique_ptr` deref, destructor
throwing during unwinding.
False-positive: Rule-of-Five class with `noexcept` move, Meyers singleton, destructor
that catches a cleanup error — must NOT be flagged.
Adversarial: base ctor calling an `override`; a global factory reading another TU's
global; `std::move` used where the type silently copies; a destructor that throws
during stack unwinding.
