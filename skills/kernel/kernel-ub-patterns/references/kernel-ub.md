# Kernel-Specific UB Patterns — Reference Rules

Knowledge layer for `kernel-ub-patterns`. Format: RULE → WHY AI GETS IT
WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION →
SOURCE. Uncertainty marked KNOWN / INFERRED / UNVERIFIED. The
aliasing/container_of/overflow fixtures were executed on this host
(gcc 16.1.0); sparse and kernel-build runs are UNVERIFIED. Relative
paths assume the skill directory as CWD.

## 1. Strict aliasing: no `*(T *)p` on buffers; use memcpy / get_unaligned

- **RULE**: C11 6.5p7 limits which lvalue types may access an object: a
  `char[]` buffer accessed as `*(uint32_t *)buf` is UB unless the
  buffer's effective type is `uint32_t`. The compiler may merge or
  reorder such accesses under `-O2` even with `-fno-strict-aliasing` in
  non-obvious ways (and sanitizers flag them). Kernel-correct reads use
  `memcpy` into a local or `get_unaligned()`/`put_unaligned()`.
- **WHY AI GETS IT WRONG**: agents write the natural-looking cast and
  conclude it is fine because `-fno-strict-aliasing` is set, or because
  the test "works" at -O0.
- **CORRECT REASONING**: `-fno-strict-aliasing` suppresses the optimizer's
  assumption but does not make the code standard-correct; the standard's
  rule governs modules, sanitizers, and other toolchains. `memcpy` is
  always legal because `char` is allowed to alias anything.
- **EXAMPLE** (bad): `examples/bad/strict_aliasing_violation.c` —
  `uint32_t v = *(uint32_t *)buf` on a `uint8_t` array; -O2 produces a
  wrong value.
- **COUNTEREXAMPLE** (good): `examples/good/kernel_ub_good.c` — the same
  read via `memcpy`, correct value at all optimization levels.
- **VERIFICATION**: `alibad` prints the wrong value (executed); the good
  fixture passes under `-fsanitize=undefined` (executed).
- **SOURCE**: iso-c11-n1570 (6.5p7); kernel-coding-style; kernel-source.

## 2. container_of requires proven provenance and the correct member

- **RULE**: `container_of(ptr, type, member)` computes
  `type - offsetof(type, member)` from the member pointer. It is only
  valid when `ptr` was actually obtained as `&obj->member` (the
  list/htable/xarray iteration macros guarantee this relationship). A
  wrong member name silently changes the computed offset; a pointer from
  an unrelated source computes a bogus address with no compile-time
  diagnostic.
- **WHY AI GETS IT WRONG**: agents reach for `container_of` "to get the
  struct" from any pointer they have, without tracing whether that
  pointer is the member.
- **CORRECT REASONING**: trace provenance: the pointer must come from the
  traversal macro or from a function that provably returns the member
  address. Verify member name against the struct. Use a compile-time
  offsetof assert for safety.
- **EXAMPLE** (bad): `examples/bad/container_of_misuse.c` — passes a
  pointer to a *different* member/struct, computing a bogus base.
- **COUNTEREXAMPLE** (good): `examples/good/kernel_ub_good.c` — the list
  iteration provides the member relationship, and the fixture asserts the
  recovered pointer equals the original object.
- **VERIFICATION**: the good fixture asserts pointer equality; the bad
  fixture prints a corrupted "struct" (executed).
- **SOURCE**: kernel-source (container_of, list_for_each_entry);
  kernel-coding-style.

## 3. __user is a sparse annotation; never dereference user pointers

- **RULE**: `__user` marks pointers that must only be touched via the
  kernel's copy helpers (`copy_from_user`, `copy_to_user`), which check
  the address range and fault handling. `__user` is enforced by sparse
  (`make C=1 CHECK=sparse`), not by gcc. Dereferencing a `__user` pointer
  in kernel space, or calling a copy helper with a non-annotated pointer,
  is a real memory-safety bug that sparse catches.
- **WHY AI GETS IT WRONG**: agents see a pointer and dereference it
  without checking whether it is user-space memory; or they drop the
  annotation and the compiler (which ignores it) stays silent.
- **CORRECT REASONING**: every pointer that can hold a user address is
  declared `__user`; copies go through `copy_from_user`/`copy_to_user`
  whose return value (bytes not copied) is checked; the user pointer is
  never dereferenced directly.
- **EXAMPLE** (bad): `examples/bad/user_pointer_and_size.c` — reads
  `user_ptr->field` directly.
- **COUNTEREXAMPLE** (good): `examples/good/kernel_ub_good.c` — copy
  helper + return check (stubbed for host build).
- **VERIFICATION**: sparse (documented, UNVERIFIED on this host);
  the stub-based good fixture compiles with the annotation carried.
- **SOURCE**: kernel-driver-api (copy_to/from_user contracts);
  kernel-source (__user, sparse).

## 4. Integer overflow in size_t arithmetic: use array_size / struct_size / check_mul_overflow

- **RULE**: `kmalloc(count * sizeof(*x))` overflows for large `count`
  when `size_t` is 32-bit (many embedded/kernel builds), allocating an
  undersized buffer → heap overflow on write. Kernel-correct code uses
  `array_size(a, b)`, `struct_size(ptr, member, n)`, `size_mul()`, or
  `check_mul_overflow()` and validates `count` against a maximum.
- **WHY AI GETS IT WRONG**: agents write `n * sizeof` and only reason
  about normal `n` values; wrap is invisible until `n` is huge.
- **CORRECT REASONING**: compute sizes with overflow-checking helpers or
  explicit `check_mul_overflow`; validate `n` up front; the result is a
  size_t that is known to fit.
- **EXAMPLE** (bad): `examples/bad/user_pointer_and_size.c` — `n * sizeof`
  wraps (32-bit size_t simulation) and the allocation is too small.
- **COUNTEREXAMPLE** (good): `examples/good/kernel_ub_good.c` —
  `check_mul_overflow` rejects the oversized `n`.
- **VERIFICATION**: the bad fixture simulates the wrap and detects the
  undersized allocation; the good fixture rejects it (executed).
- **SOURCE**: kernel-source (overflow helpers); cert-c (INT32-C,
  INT30-C).

## 5. Kernel build flags: -fwrapv and -fno-strict-aliasing are relief, not license

- **RULE**: the Linux kernel builds with `-fno-strict-aliasing` and
  `-fwrapv` (signed overflow defined as wrap). This means a signed
  overflow in kernel code is NOT UB in the kernel build — but relying on
  wrap, or on alias suppression, is wrong: modules, sanitizers, other
  builds, and the standard still treat the code as suspect. Keep code
  standard-correct.
- **WHY AI GETS IT WRONG**: agents either (a) claim "signed overflow is
  UB in the kernel" (false under -fwrapv) and "fix" defined code, or
  (b) claim "aliasing is fine, -fno-strict-aliasing is on" (true for
  miscompilation, false for correctness).
- **CORRECT REASONING**: name the build flags and their exact effect:
  `-fwrapv` defines signed wrap; `-fno-strict-aliasing` disables the
  optimizer's alias assumption. Neither makes nonstandard code correct.
- **EXAMPLE** (bad): code that relies on alias-punned reads "working"
  because of `-fno-strict-aliasing`.
- **COUNTEREXAMPLE** (good): standard-correct code that survives
  `-fsanitize=undefined` and any optimization level.
- **VERIFICATION**: `-fsanitize=undefined` on the fixtures (executed);
  the flag facts are KNOWN from kernel-source build config.
- **SOURCE**: kernel-source (Kconfig/Makefile compiler flags);
  iso-c11-n1570 (the underlying standard rules).

## Quick reference table

| Pattern | Correct rule | Tool |
|---|---|---|
| strict aliasing | memcpy / get_unaligned, never *(T*)p on buffers | -fsanitize=alias, -O2 test |
| container_of | proven provenance + correct member + offsetof assert | review + static assert |
| __user | copy helpers + return check, never deref | sparse `make C=1` |
| size_t overflow | array_size/struct_size/check_mul_overflow | -fsanitize=undefined |
| -fwrapv / -fno-strict-aliasing | relief from miscompilation, not license to be nonstandard | sanitizers, module builds |
