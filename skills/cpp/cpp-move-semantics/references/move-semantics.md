# C++ Move Semantics — Reference

Source: ISO C++20 draft N4861 [class.copy.ctor]/[class.copy.assign]/[class.copy.elision]/
[dcl.init.ref]/[class.temporary]/[lib.types.movedfrom]; C++ Core Guidelines C.20-C.22/C.63-C.66/
F.19/F.20/F.21/F.45/R.10-R.12; cppreference C++ reference and UB list; clang-tidy checks list.
Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE →
VERIFICATION → SOURCE.

## 1. Rvalue references

- **RULE**: an rvalue reference (`T&&`) binds only to rvalues (prvalues and xvalues). Binding it
  to a prvalue temporary extends that temporary's lifetime to the reference's lifetime
  ([class.temporary] p5); binding it to an xvalue does not extend anything. A named rvalue
  reference is itself an lvalue. `std::move(x)` is exactly `static_cast<T&&>(x)`: it only changes
  the value category, it moves nothing.
- **WHY AI GETS IT WRONG**: assumes `std::move` performs a transfer, or that a `T&&` parameter
  "moves automatically", or that every reference binding to a temporary extends its lifetime.
- **CORRECT REASONING**: `std::move` produces an xvalue so overload resolution can pick a move
  ctor; whether anything is actually moved is decided by the selected constructor. Lifetime
  extension applies to prvalue temporaries only, never through `std::move`, and never when the
  reference escapes (a returned `T&&` bound to a local is dangling).
- **EXAMPLE** (bad):
  ```cpp
  std::string&& f() {
      std::string s = "temp";
      return std::move(s);       // xvalue: no lifetime extension -> dangling
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  std::string f() {              // return by value: elided or moved, never dangling
      std::string s = "temp";
      return s;
  }
  ```
- **VERIFICATION**: `g++ -std=c++17 -Wall -Wextra -Werror -O2`; the bad function compiles but
  reading the result is dangling (ASan flags it at runtime if actually read).
- **SOURCE**: iso-cpp20-n4861 [dcl.init.ref] p5, [class.temporary] p5; cpp-core-guidelines F.45.

## 2. Move constructor and move assignment

- **RULE**: a move ctor/assignment transfers resources from the source and leaves it in a state
  the class defines (for std types: valid but unspecified). A move op is implicitly generated
  only when there is NO user-declared copy ctor, copy assignment, move ctor, move assignment, or
  destructor ([class.copy.ctor] p6). When no move exists, overload resolution falls back to the
  copy op — `std::move` then copies silently.
- **WHY AI GETS IT WRONG**: assumes `std::move` always selects a move; adding a user-declared
  destructor (common for logging/dtor-only classes) quietly disables moves, so code copies with
  no compiler warning.
- **CORRECT REASONING**: check class completeness against the five special members before
  assuming a move happens. A user-declared dtor suppresses implicit moves; either delete/define
  them explicitly (rule of five) or rely on `std::vector`/`unique_ptr` members (rule of zero).
- **EXAMPLE** (bad):
  ```cpp
  struct Buffer {
      Buffer() = default;
      ~Buffer() {}                       // user-declared dtor -> no implicit move
      Buffer(const Buffer& o) { /* copies */ }
  };
  Buffer b(std::move(a));                // "copy ctor" runs, nothing moved
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  struct Buffer {
      Buffer() = default;
      ~Buffer() { delete[] data; }
      Buffer(Buffer&& o) noexcept : data(o.data) { o.data = nullptr; }
      Buffer& operator=(Buffer&& o) noexcept { /* transfer + reset o */ }
      Buffer(const Buffer&) = delete;    // copy deliberately unavailable
  };
  Buffer b(std::move(a));                // "move ctor" runs
  ```
- **VERIFICATION**: print instrumentation in each ctor + `static_assert(
  std::is_move_constructible_v<Buffer>)`; run `examples/bad/missing_move_ctor.cpp` vs
  `examples/good/move_ctor_implemented.cpp`.
- **SOURCE**: iso-cpp20-n4861 [class.copy.ctor] p6-p8; cpp-core-guidelines C.20, C.21, C.63.

## 3. Moved-from state: valid but unspecified

- **RULE**: objects of C++ standard library types that have been moved from are placed in a
  "valid but unspecified" state ([lib.types.movedfrom]): they can be assigned to, destroyed, and
  have member functions called that do not depend on their current value, but their value is not
  guaranteed. Exceptions: `std::unique_ptr` moved-from is guaranteed empty; `std::string` is
  commonly empty but that is an implementation detail.
- **WHY AI GETS IT WRONG**: either believes reading a moved-from object is UB (it is not, for
  std types) or believes the old value survives and reads it.
- **CORRECT REASONING**: treat a moved-from object as an unknown-but-usable empty object: reset
  or overwrite it before reading its value. Dereferencing a moved-from `unique_ptr`/`optional`
  is a null dereference (UB, crashes), NOT an unspecified value.
- **EXAMPLE** (bad):
  ```cpp
  std::string s = "payload";
  std::string t = std::move(s);
  std::printf("size: %zu\n", s.size());  // prints 0 on libstdc++, but unspecified
  auto p = std::make_unique<int>(7);
  auto q = std::move(p);
  std::printf("%d\n", *p);               // null deref -> crash
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  std::string s = "payload";
  std::string t = std::move(s);
  s = "fresh";                           // reset before reading
  auto p = std::make_unique<int>(7);
  auto q = std::move(p);
  if (p) { std::printf("%d\n", *p); }    // null-check before deref
  ```
- **VERIFICATION**: run `examples/bad/use_after_move.cpp` (records moved-from size 0, then
  access violation on the null deref) vs `examples/good/use_after_move_fixed.cpp` (exit 0).
- **SOURCE**: iso-cpp20-n4861 [lib.types.movedfrom]; cppreference-cpp-ub (null deref block);
  cpp-core-guidelines R.10.

## 4. Use-after-move is a bug

- **RULE**: reading the value of a moved-from std object, or using a moved-from pointer wrapper
  as if it were non-null, is a logic bug. It is valid-but-unspecified (not UB) for containers,
  but null-deref UB for `unique_ptr`/`optional`. GCC does NOT warn under `-Wall -Wextra -Werror`
  (verified with g++ 16.1); clang-tidy `bugprone-use-after-move` is the canonical detector.
- **WHY AI GETS IT WRONG**: "it compiles and prints a value, so it is fine" — the run happened to
  produce the old value or 0; the code is still wrong and optimizer-dependent.
- **CORRECT REASONING**: after `std::move(x)` the compiler does not track x; the standard
  guarantees nothing about its value. Any read before a reset is a defect even when it happens
  to print a sane value.
- **EXAMPLE** (bad):
  ```cpp
  std::string a = "abc", b = std::move(a);
  if (a.empty()) /* wrong assumption */;
  std::unique_ptr<int> u = std::make_unique<int>(1);
  auto v = std::move(u);
  use(*u);                    // null deref
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  std::string a = "abc", b = std::move(a);
  a.clear();                  // reset before any read
  if (a.empty()) /* now safe */;
  std::unique_ptr<int> u = std::make_unique<int>(1);
  auto v = std::move(u);
  if (u) use(*u); else /* safe */;
  ```
- **VERIFICATION**: g++ `-Wall -Wextra -Werror -O2` compiles the bad example without a single
  warning (recorded) — run clang-tidy `-checks=bugprone-use-after-move` to get the diagnostic.
- **SOURCE**: clang-tidy-checks (bugprone-use-after-move); iso-cpp20-n4861 [lib.types.movedfrom];
  cppreference-cpp-ub.

## 5. Copy elision, NRVO, guaranteed elision (C++17)

- **RULE**: in C++17 a prvalue is materialized directly into its result object — elision is
  guaranteed ([class.copy.elision] p1-p2), not an optimization. NRVO (named return value
  optimization) for `return local;` is optional but performed by GCC/Clang. When elision does
  not apply, the return of a local uses a move (if available), never a copy.
- **WHY AI GETS IT WRONG**: adds `return std::move(local);` believing a move is required, which
  actually prevents NRVO (GCC: `-Wpessimizing-move`) and forces an extra move; or assumes
  `return by value` always copies at `-O0`.
- **CORRECT REASONING**: `return expression;` where the expression is a prvalue is guaranteed-
  elided; `return local;` is NRVO-eligible (may still elide at `-O2`) or falls back to a move.
  `std::move` is neither needed nor helpful for any of these paths.
- **EXAMPLE** (bad):
  ```cpp
  std::string make() {
      std::string s = "...";
      return std::move(s);    // blocks NRVO; extra move; -Wpessimizing-move
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  std::string make() {
      std::string s = "...";
      return s;               // NRVO elides; else a move; never a copy
  }
  ```
- **VERIFICATION**: print-instrumented move/copy ctor: `return_by_value.cpp` at `-O2` prints no
  ctor calls; `-fno-elide-constructors` prints one `move ctor`; the bad file fails `-Werror`
  with `-Wpessimizing-move`.
- **SOURCE**: iso-cpp20-n4861 [class.copy.elision] p1-p3; cpp-core-guidelines F.20, F.21.

## 6. std::move vs std::forward (perfect forwarding)

- **RULE**: `std::move(x)` unconditionally casts to rvalue. `std::forward<T>(x)` casts to rvalue
  only when `T` deduced in a forwarding context is an rvalue reference; it preserves the caller's
  value category. Never `std::forward` outside a template, never `std::move` on a generic
  forwarding parameter (it always moves, discarding lvalue-ness), and never `std::move` on a
  `const` object (the copy ctor wins).
- **WHY AI GETS IT WRONG**: treats `std::move` and `std::forward` as interchangeable, or uses
  `std::move` on a parameter that is later read again.
- **CORRECT REASONING**: in `template <class T> void f(T&& x)`, `T&&` is a forwarding reference:
  forward `x` with `std::forward<T>(x)` exactly once, to the call that consumes it. Use
  `std::move` only on a concrete object you are done with.
- **EXAMPLE** (bad):
  ```cpp
  template <class T> void f(T&& x) { sink(std::move(x)); }  // always moves
  std::string s;
  f(s);                    // caller's lvalue is consumed anyway
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  template <class T> void f(T&& x) { sink(std::forward<T>(x)); }  // moves only if rvalue
  std::string s;
  f(s);                            // lvalue -> copy overload
  f(std::string("tmp"));           // rvalue -> move overload
  ```
- **VERIFICATION**: run `examples/good/perfect_forwarding.cpp` — output shows copy for the
  lvalue and move for the rvalue.
- **SOURCE**: iso-cpp20-n4861 [dcl.init.ref] p5 (forwarding reference); cpp-core-guidelines
  F.19.

## 7. Rule of five

- **RULE**: if you define or delete any of copy ctor, copy assignment, move ctor, move
  assignment, or destructor, consider defining/deleting all five (rule of five). Declaring any
  of the copy ops or the destructor suppresses the implicit move ops, so a class with a custom
  destructor but defaulted copy silently copies on `std::move`.
- **WHY AI GETS IT WRONG**: defines only a destructor and copy ctor, assuming moves still work;
  the result is a performance bug (silent copies) or a correctness bug (deleted move that
  surfaces as a copy-ctor error).
- **CORRECT REASONING**: prefer the rule of zero (store resources in std types). When custom
  ownership is needed, define all five with `noexcept` moves (C.66) and `= delete` on what is
  not supported.
- **EXAMPLE** (bad):
  ```cpp
  struct Widget {
      ~Widget() {}                      // suppresses implicit moves
      Widget(const Widget&) = default;
      Widget& operator=(const Widget&) = default;
  };
  Widget b(std::move(a));               // copy ctor runs
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  struct Widget {
      Widget() = default;
      ~Widget() = default;
      Widget(const Widget&) = default;
      Widget& operator=(const Widget&) = default;
      Widget(Widget&&) noexcept = default;
      Widget& operator=(Widget&&) noexcept = default;
  };
  Widget b(std::move(a));               // move ctor runs
  ```
- **VERIFICATION**: `static_assert(std::is_nothrow_move_constructible_v<Widget>)`; print
  instrumentation showing which ctor runs.
- **SOURCE**: iso-cpp20-n4861 [class.copy.ctor] p6; cpp-core-guidelines C.20, C.21, C.22,
  C.66.

## 8. Why returning by value is usually fine

- **RULE**: return by value is the idiomatic way to produce objects: prvalue returns are
  guaranteed-elided (C++17), named locals are NRVO'd or moved, and heap-heavy types are cheap to
  move. Do NOT return rvalue references to locals (dangling) and do NOT use `std::move` in the
  return statement (blocks elision). Move-only types like `unique_ptr` return by value fine.
- **WHY AI GETS IT WRONG**: reaches for out-parameters, raw pointers, or `std::move` returns to
  "avoid a copy", or writes `return std::move(local);` thinking it is the fast path.
- **CORRECT REASONING**: `T f() { ...; return local; }` and `auto x = f();` produce at most one
  move (usually zero). Guideline F.20/F.21 prefer return values over out-parameters; F.45 says
  never return a `T&&`.
- **EXAMPLE** (bad):
  ```cpp
  std::unique_ptr<int> p(std::unique_ptr<int>& out) { ... }  // out-param dance
  std::string&& bad() { std::string s; return std::move(s); } // dangling
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  std::unique_ptr<int> make(int v) { return std::make_unique<int>(v); }
  std::string good() { std::string s = "..."; return s; }
  auto p = make(7);          // guaranteed elision, no extra move
  ```
- **VERIFICATION**: `examples/good/return_by_value.cpp` prints no ctor calls at `-O2`; run the
  file and compare with `-fno-elide-constructors`.
- **SOURCE**: iso-cpp20-n4861 [class.copy.elision] p1-p2; cpp-core-guidelines F.20, F.21, F.45.

## 9. Move-only types

- **RULE**: types with a deleted copy and a valid move (`std::unique_ptr`, `std::mutex`,
  `std::promise`, iostreams, many RAII wrappers) cannot be copied. They are transferred with
  `std::move` (or returned by value). Passing a move-only type to a function by value moves it
  in; the source is left empty.
- **WHY AI GETS IT WRONG**: writes `auto p2 = p;` for a `unique_ptr` and is surprised by the
  compile error, or tries to copy a mutex, or uses `std::move` on a move-only type that already
  has guaranteed-empty moved-from semantics and misreads the state.
- **CORRECT REASONING**: copy being deleted is the point: ownership moves once. After the move
  the source is valid and (for `unique_ptr`) guaranteed empty; check it or reassign before use.
- **EXAMPLE** (bad):
  ```cpp
  auto p = std::make_unique<int>(7);
  auto q = p;                  // error: unique_ptr copy is deleted
  std::mutex m2 = m1;          // error: mutex is move-only
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  auto p = std::make_unique<int>(7);
  auto q = std::move(p);       // ownership transferred; p is now empty
  std::unique_ptr<int> r = make(42);   // return by value, guaranteed elision
  ```
- **VERIFICATION**: `examples/good/move_only_type.cpp` compiles clean and prints `q=7 p=(null)`;
  the bad copy line fails to compile with "use of deleted function".
- **SOURCE**: iso-cpp20-n4861 [class.copy.ctor] (deleted copy), [unique.ptr]; cpp-core-guidelines
  R.10, R.11, R.12, C.21.
