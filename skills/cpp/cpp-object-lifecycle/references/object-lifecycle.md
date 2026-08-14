# C++ Object Lifecycle — Reference

Source: ISO C++20 draft N4861 [intro.object]/[basic.life]/[class.cdtor]/[class.base.init]/
[class.dtor]/[class.copy.ctor]/[class.copy.assign]; C++ Core Guidelines I.22/C.20/C.21/C.30-R.37/E.28;
cppreference C++ UB list; Carruth "Garbage In, Garbage Out" (CppCon 2016).
Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE →
VERIFICATION → SOURCE.

## 1. Construction and destruction order

- **RULE**: construction order is: virtual bases, then direct bases in declaration
  order, then members in declaration order, then the constructor body. Destruction is
  the exact reverse: body, members in reverse declaration order, direct bases in
  reverse declaration order, virtual bases last. At any step only already-constructed
  subobjects exist.
- **WHY AI GETS IT WRONG**: assumes members are initialized in the order they appear in
  the constructor's member-initializer list, or that destruction mirrors something
  other than reverse construction.
- **CORRECT REASONING**: the initialization order is fixed by the declaration order of
  bases/members, regardless of initializer-list order (GCC/Clang warn via
  `-Wreorder`). Destruction is strictly reverse construction; relying on any other
  order is a defect that sanitizers and ASan may miss.
- **EXAMPLE** (bad):
  ```cpp
  struct D : B {
      int y;
      int x;
      D() : x(1), y(2) {}   // y initialized BEFORE x: order is declaration order
  };
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  struct D : B {
      int x;
      int y;
      D() : x(1), y(2) {}   // initializer order matches declaration order
  };
  ```
- **VERIFICATION**: `g++ -Wall -Wextra -Werror -Wreorder`; a print-statement test that
  logs ctor/dtor entries and confirms reverse order.
- **SOURCE**: iso-cpp20-n4861 [class.base.init] p13; cpp-core-guidelines C.20.

## 2. Virtual dispatch during construction/destruction

- **RULE**: a virtual call (and `typeid`) from a constructor or destructor resolves to
  the class currently being constructed/destroyed, NOT to the most-derived class.
- **WHY AI GETS IT WRONG**: assumes virtual calls always dispatch dynamically to the
  most-derived override, so a base ctor "should" reach the derived override.
- **CORRECT REASONING**: during construction/destruction the dynamic type is the class
  under construction/destruction; the derived subobject does not exist yet (or is
  already destroyed), so the vtable in use is the base's. The derived override's
  members are uninitialized and must not be touched.
- **EXAMPLE** (bad):
  ```cpp
  struct Base {
      Base() { log(); }            // prints "Base", not "Derived"
      virtual void log() const { puts("Base"); }
  };
  struct Derived : Base { void log() const override { puts("Derived"); } };
  Derived d;                       // "Base" printed; derived members uninitialized
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  struct Base {
      void init() { log(); }       // called after construction completes
      virtual void log() const { puts("Base"); }
  };
  Derived d; d.init();             // full dynamic dispatch: prints "Derived"
  ```
- **VERIFICATION**: run `examples/bad/virtual_call_in_ctor.cpp` — output is `Base`;
  run the good file — output is `Derived`. Cppcheck/clang-tidy flag the bad pattern.
- **SOURCE**: iso-cpp20-n4861 [class.cdtor] p4; cpp-core-guidelines C.20; cppreference-cpp-ub.

## 3. Static initialization order fiasco

- **RULE**: the order of dynamic initialization of non-local statics across translation
  units is unspecified. A constructor reading a global defined in another TU may see the
  zero-initialized value before that global's constructor runs.
- **WHY AI GETS IT WRONG**: assumes source order, build order, or link order is a
  guarantee; on many toolchains the observed order just "happens" to work.
- **CORRECT REASONING**: within a TU, dynamic init follows definition order; across TUs
  it is unspecified ([basic.start.dynamic]). The fix is construct-on-first-use
  (function-local static, a Meyers singleton) or `constinit`/`inline` (C++17),
  never reordering files.
- **EXAMPLE** (bad):
  ```cpp
  // a.cpp
  int get_b();                       // defined in b.cpp
  struct A { A() { printf("A sees %d\n", get_b()); } };
  A g_a;                             // may run before b.cpp's global is initialized
  ```
  ```cpp
  // b.cpp
  struct B { B() : v(42) {} int v; };
  B g_b;
  int get_b() { return g_b.v; }      // g_b.v is 0 until B::B runs
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  struct B { B() : v(42) {} int v; };
  B& get_b() { static B b; return b; }   // constructed on first use, always 42
  struct A { A() : seen(get_b().v) {} int seen; };
  A g_a;
  ```
- **VERIFICATION**: compile the two-TU bad example with `g++ a.cpp b.cpp` and with
  `g++ b.cpp a.cpp`; at least one order must print `0`. The good version always prints
  `42` under both orders.
- **SOURCE**: iso-cpp20-n4861 [basic.start.dynamic]; cpp-core-guidelines I.22;
  carruth-gigo.

## 4. Copy vs move semantics

- **RULE**: copy leaves the source unchanged and yields an independent copy; move
  transfers resources, leaving the source in a valid-but-unspecified state. `std::move`
  is only a cast to rvalue — it moves nothing by itself.
- **WHY AI GETS IT WRONG**: treats copy and move as interchangeable, or believes
  `std::move(x)` immediately mutates x.
- **CORRECT REASONING**: a move constructor/assignment may steal the source's buffers;
  the standard-library contract only guarantees the moved-from object is destructible
  and assignable. The type's own move ctor defines the state; if none exists, an rvalue
  falls back to the copy ctor.
- **EXAMPLE** (bad):
  ```cpp
  std::string s = "payload";
  std::string t = std::move(s);
  printf("%zu\n", s.size());   // unspecified; libstdc++ prints 0
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  std::string s = "payload";
  std::string t = std::move(s);
  s.clear();                   // reset before any reuse
  s = "reused";
  ```
- **VERIFICATION**: run `examples/bad/use_after_move.cpp` and observe the moved-from
  size; the good example reads only after reset. `-Wpessimizing-move`/`-Wredundant-move`
  catch move misuse around constructors.
- **SOURCE**: iso-cpp20-n4861 [class.copy.ctor] p8, [class.copy.assign]; cpp-core-guidelines C.64/R.37.

## 5. Use-after-move

- **RULE**: reading the value of a moved-from standard-library object is valid but
  unspecified; dereferencing a moved-from pointer/optional-like object is a null
  dereference and UB.
- **WHY AI GETS IT WRONG**: assumes the old value survives the move, or treats
  use-after-move as always-UB and "therefore cannot happen".
- **CORRECT REASONING**: the moved-from object must be destructible and assignable, but
  its value is unspecified; code must reset it before reading. A moved-from
  `unique_ptr` holds `nullptr`, so `*p` is a null dereference — the optimizer may
  assume it never happens and delete surrounding checks.
- **EXAMPLE** (bad):
  ```cpp
  std::unique_ptr<int> p(new int(7));
  auto q = std::move(p);
  printf("%d\n", *p);   // null dereference: UB, crashes at runtime
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  std::unique_ptr<int> p(new int(7));
  auto q = std::move(p);
  if (p) { printf("%d\n", *p); }   // check for null before use
  ```
- **VERIFICATION**: run `examples/bad/use_after_move.cpp` — the deref line crashes
  (access violation / SIGSEGV); the good example runs to completion.
- **SOURCE**: iso-cpp20-n4861 [basic.life] p4-p8; cppreference-cpp-ub; cpp-core-guidelines R.1/R.37.

## 6. Destructor exception safety (RAII)

- **RULE**: destructors are `noexcept` by default since C++11; an exception escaping a
  destructor calls `std::terminate`. If one exception is already being unwound, a
  second throw in a destructor always terminates.
- **WHY AI GETS IT WRONG**: assumes a destructor can report failure like a normal
  function, or that throwing from a destructor is "just an error path".
- **CORRECT REASONING**: RAII cleanup must run on every exit path including unwinding;
  `noexcept` makes an escaping throw a fatal `std::terminate`. Report failures through
  an explicit `close()`/`release()` that can throw (or record the error), and keep the
  destructor `noexcept`.
- **EXAMPLE** (bad):
  ```cpp
  struct Boom { ~Boom() { throw std::runtime_error("dtor failure"); } };
  // terminate called after throwing an instance of 'std::runtime_error'
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  class Session {
      bool closed_ = false;
  public:
      bool close() noexcept { if (!closed_) { closed_ = true; } return true; }
      ~Session() noexcept { try { close(); } catch (...) {} }
  };
  ```
- **VERIFICATION**: run `examples/bad/dtor_throw.cpp` — it terminates with
  "terminate called after throwing an instance of 'std::runtime_error'" and a nonzero
  exit; `examples/good/dtor_noexcept.cpp` exits 0.
- **SOURCE**: iso-cpp20-n4861 [class.dtor] p3, [except.terminate]; cpp-core-guidelines E.28, R.1; herbsutter-gotw.

## 7. Object lifetime and reference/pointer invalidation

- **RULE**: an object's lifetime begins after its constructor completes and ends when
  its destructor starts (or storage is reused/released). Using an object outside its
  lifetime is UB; every reference/pointer/iterator to it is invalid after the
  destructor, and iterators/references may be invalidated earlier by member functions.
- **WHY AI GETS IT WRONG**: assumes storage and lifetime are the same, or that a stored
  reference/iterator stays valid as long as the container variable exists.
- **CORRECT REASONING**: the abstract machine tracks lifetime, not storage: `new` /
  placement-new / reuse change lifetime; `std::launder` is needed to access a new
  object through a pointer to the old one. Reference invalidation (e.g. after
  `std::vector::push_back` reallocation) is real and must be re-fetched. Returning a
  reference to a local or a temporary dangles immediately.
- **EXAMPLE** (bad):
  ```cpp
  int& dangling() { int x = 5; return x; }   // reference to destroyed local
  std::vector<int> v{1,2,3};
  int* p = &v[0];
  v.push_back(4);                            // may reallocate; *p is dangling
  ```
- **COUNTEREXAMPLE** (good):
  ```cpp
  int f() { int x = 5; return x; }           // return by value, no dangling
  size_t i = 0;
  v.push_back(4);
  int* p = &v[i];                            // fetch after reallocation
  ```
- **VERIFICATION**: ASan/`-fsanitize=address` for heap use-after-free; MSan for
  uninitialized; run the dangling/local example under `-O2` and observe the difference;
  `-Wdangling-pointer` (GCC 12+) flags return-by-reference-to-local.
- **SOURCE**: iso-cpp20-n4861 [basic.life] p1-p8, [intro.object]; cppreference-cpp-ub;
  carruth-gigo.

## Quick detection table

| Rule | Tool / command | Signal |
|---|---|---|
| ctor/dtor order | `-Wall -Wextra -Werror -Wreorder` | reorder warning |
| virtual call in ctor/dtor | cppcheck, clang-tidy | calling virtual from ctor |
| SIOF | two-TU link-order test | 0 vs 42 output |
| use-after-move | runtime crash / `-fsanitize=undefined` | null deref |
| dtor throw | run bad example | `terminate called after throwing` |
| dangling ref/iterator | ASan, `-Wdangling-pointer` | use-after-free / dangling |
