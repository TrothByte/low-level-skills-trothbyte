# RAII & Descriptor-Type Design — Reference

Sources: C++ Core Guidelines R.*; Rust API Guidelines C-*; matklad "Preconditions"; N1570
malloc/free contract. Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. Wrap the raw handle in a typed descriptor

- **RULE**: expose `struct File(int fd)` / `class File { int fd_; ... }` / opaque `FILE*`
  + functions — never bare `int fd` / `void*` with no ownership rules.
- **WHY AI GETS IT WRONG**: "the OS returns `int`, so I'll pass `int` around."
- **CORRECT REASONING**: a typed wrapper carries validity state and prevents mixing unrelated
  handles; Rust newtype (`struct Fd(OwnedFd)`) gives static distinctions (C-NEWTYPE).
- **EXAMPLE** (good): `class File { int fd_; ~File() { close(fd_); } /* non-copyable */ };`.
- **COUNTEREXAMPLE** (bad): `int open_and_return_fd(void);` with every caller remembering to close.
- **VERIFICATION**: code review — every handle in a parameter list is a wrapper type.
- **SOURCE**: Rust API Guidelines C-NEWTYPE; C++ CG R.3 (raw pointer non-owning).

## 2. RAII: acquire in ctor, release in dtor

- **RULE**: acquire the resource in the constructor, release in the destructor; give the
  result of allocation to a manager object immediately (R.12).
- **WHY AI GETS IT WRONG**: "the caller frees it" — ownership spread across callers.
- **CORRECT REASONING**: destructors run on all exit paths including exceptions; `make_unique`/
  scoped objects remove manual management.
- **EXAMPLE** (good): `auto buf = std::make_unique<char[]>(n);` — freed on scope exit, even on throw.
- **COUNTEREXAMPLE** (bad): `char *buf = new char[n]; ... if (err) return; delete[] buf;` — leak on the early return.
- **VERIFICATION**: error-path unit tests + ASan/leak checker.
- **SOURCE**: C++ CG R.1, R.12, R.20/R.23; Stroustrup RAII.

## 3. Delete copy / implement move for unique resources

- **RULE**: a unique-resource type is non-copyable; C++ `= delete` copy ctor/assign, implement
  move; Rust it's naturally `!Copy` + `Drop`.
- **WHY AI GETS IT WRONG**: default copy makes two owners of one fd (double-close).
- **CORRECT REASONING**: value semantics for unique resources means move, not copy.
- **EXAMPLE** (good): `File(File&&) noexcept; File& operator=(File&&); File(const File&) = delete;`.
- **COUNTEREXAMPLE** (bad): default copy ctor — `File a; File b = a;` double-close on destruction.
- **VERIFICATION**: ASan double-close test; compile-time copy attempt fails.
- **SOURCE**: C++ CG R.37; C++ [class.copy.ctor].

## 4. Typed errors, not errno

- **RULE**: return typed errors (enum/Result) instead of global `errno` or magic `-1`.
- **WHY AI GETS IT WRONG**: "errno is how C does it" — global state races and stale-checks (A17).
- **CORRECT REASONING**: typed errors carry context, are checked-by-type, and can't be read
  after another call overwrites them.
- **EXAMPLE** (good): `enum class Status { Ok, NotFound, Invalid }; Status load(Path, Buffer&);`.
- **COUNTEREXAMPLE** (bad): `int load(...)` returning `-1` with `errno = ENOENT`.
- **VERIFICATION**: static type of the return forces handling.
- **SOURCE**: Rust API Guidelines C-GOOD-ERR; C++ CG E.28; matklad error models.

## 5. Preconditions via debug_assert

- **RULE**: assert invariants at the entry of APIs that rely on them; document the contract.
- **WHY AI GETS IT WRONG**: "callers will behave" — silent assumption, or only a comment.
- **CORRECT REASONING**: debug asserts catch contract violations in development cheaply;
  the documented invariant becomes the API contract (C-VALIDATE).
- **EXAMPLE** (good): `void write(const Buffer& b) { assert(b.size() <= capacity_); ... }`.
- **COUNTEREXAMPLE** (bad): no check — OOB by construction, caught only by ASan later.
- **VERIFICATION**: run tests with asserts on (debug build).
- **SOURCE**: Rust API Guidelines C-VALIDATE; matklad "Preconditions".

## 6. Builder for complex construction

- **RULE**: complex construction goes through a builder that validates before producing the
  object; simple types get validating constructors.
- **WHY AI GETS IT WRONG**: huge constructor with magic defaults; partial construction states.
- **CORRECT REASONING**: the builder makes invalid states unrepresentable until `.build()`
  validates and returns Result.
- **EXAMPLE** (good): `ReaderBuilder::new().buf_size(4096).mode(Mode::Mmap).build()?`.
- **COUNTEREXAMPLE** (bad): `Reader(4096, 0, true, nullptr)` positional argument soup.
- **VERIFICATION**: no way to construct an invalid object.
- **SOURCE**: Rust API Guidelines C-BUILDER.
