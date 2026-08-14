# C Undefined Behavior Taxonomy

Source: ISO C11 N1570 Annex J.2 (191 items), organized into operational classes.
Each entry: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. Signed integer overflow

- **RULE**: Signed integer overflow is UB. Unsigned wraps (modulo 2^N).
- **WHY AI GETS IT WRONG**: assumes `INT_MAX + 1` wraps to `INT_MIN` (two's complement habit).
- **CORRECT REASONING**: the compiler may assume `x + 1 > x` for signed `x`, enabling
  optimizations that change behavior once overflow is possible.
- **EXAMPLE** (bad):
  ```c
  int f(int x) { return x + 1 > x; }  // compiler may fold to constant 1
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  unsigned f(unsigned x) { return x + 1 > x; }  // well-defined wrap check
  ```
- **VERIFICATION**: `clang -O2 -S`; UBSan `-fsanitize=signed-integer-overflow`.
- **SOURCE**: N1570 §6.5p5, §6.2.5p9; cppreference "signed overflow"; CERT INT32-C.

## 2. Shift by negative or >= width

- **RULE**: `x << n` / `x >> n` is UB if `n < 0` or `n >= width`. Shifting into the sign
  bit of a signed value is also UB.
- **WHY AI GETS IT WRONG**: assumes `x << 32` gives 0, or that negative shift is "just wrong result".
- **CORRECT REASONING**: on x86 the hardware masks the count, but the compiler is free to
  assume `n` is in range and elide range checks.
- **EXAMPLE** (bad): `1u << n` where `n` can be 32+; `x >> -1`.
- **COUNTEREXAMPLE** (good): guard `if (n >= 0 && n < 32)` before shifting.
- **VERIFICATION**: UBSan `-fsanitize=shift`.
- **SOURCE**: N1570 §6.5.7p3; CERT INT34-C.

## 3. Out-of-bounds array access / pointer arithmetic outside array

- **RULE**: accessing an array out of bounds, or forming a pointer more than one past the
  end, is UB. `arr[-1]`, `arr[len]`, and `ptr + k` with `k` outside `[0, len]` are UB.
- **WHY AI GETS IT WRONG**: "reading `arr[len]` just reads adjacent memory".
- **CORRECT REASONING**: the compiler may assume indices are in-bounds and remove bounds
  checks or reorder memory accesses.
- **EXAMPLE** (bad): `for (int i = 0; i <= n; i++) sum += a[i];` (off-by-one).
- **COUNTEREXAMPLE** (good): `i < n`; use `size_t` for indices.
- **VERIFICATION**: ASan `-fsanitize=address`; `-fsanitize=bounds`.
- **SOURCE**: N1570 §6.5.6p8, §6.5.2.1; CERT ARR30-C.

## 4. Null pointer dereference

- **RULE**: dereferencing a null pointer is UB (not necessarily a segfault).
- **WHY AI GETS IT WRONG**: assumes "null deref always crashes", so a check "can't be needed".
- **CORRECT REASONING**: the optimizer may remove a later null check because it assumes the
  earlier dereference proved non-null.
- **EXAMPLE** (bad):
  ```c
  int f(int *p) { int x = *p; if (!p) return 0; return x; }  // !p check may be deleted
  ```
- **COUNTEREXAMPLE** (good): check `p` before dereferencing.
- **VERIFICATION**: `-fno-delete-null-pointer-checks` disables the optimization; UBSan.
- **SOURCE**: N1570 §6.5.3.2p4; CERT EXP34-C.

## 5. Reading uninitialized memory

- **RULE**: reading an object whose value is indeterminate is UB (for some types, an
  unspecified value; for automatic `int`, indeterminate → UB if it could be a trap).
- **WHY AI GETS IT WRONG**: "uninitialized just means some garbage number".
- **CORRECT REASONING**: the optimizer may assume the value is not "poison" and fold
  comparisons to either branch, or MSan will flag the read.
- **EXAMPLE** (bad): `int x; if (x > 0) ...` — compiler may assume either.
- **COUNTEREXAMPLE** (good): initialize `int x = 0;` or use `calloc`.
- **VERIFICATION**: MSan `-fsanitize=memory`; `-Wuninitialized`; Valgrind.
- **SOURCE**: N1570 §6.3.2.1p2, §6.7.9p10; CERT EXP33-C.

## 6. Use-after-free / double-free / invalid free

- **RULE**: using a pointer after `free`, freeing the same pointer twice, or freeing a
  non-heap pointer is UB.
- **WHY AI GETS IT WRONG**: assumes freed memory "still has the old value until reused".
- **CORRECT REASONING**: `free` ends the object's lifetime; any use is UB, and ASan will
  flag heap-use-after-free. Set pointers to NULL after free as a defensive habit (does not
  fix double-free across aliases).
- **EXAMPLE** (bad): `free(p); use(p);` or `free(p); free(q); /* q aliases p */`.
- **COUNTEREXAMPLE** (good): single owner, free once, null out.
- **VERIFICATION**: ASan `-fsanitize=address` (heap-use-after-free, double-free).
- **SOURCE**: N1570 §7.22.3.3/7.22.3.4; CERT MEM30-C, MEM31-C, MEM34-C.

## 7. Overlapping buffers with memcpy

- **RULE**: `memcpy` with overlapping source and destination is UB; use `memmove`.
- **WHY AI GETS IT WRONG**: assumes `memcpy` handles overlap "anyway".
- **CORRECT REASONING**: `memcpy` may be implemented with vector loads that corrupt on
  overlap. `memmove` is required to be overlap-safe.
- **EXAMPLE** (bad): `memcpy(buf + 1, buf, n);`.
- **COUNTEREXAMPLE** (good): `memmove(buf + 1, buf, n);`.
- **VERIFICATION**: ASan `-fsanitize=address` (memcpy-param-overlap); `_FORTIFY_SOURCE`.
- **SOURCE**: N1570 §7.24.2.1p2; PVS-Studio V512.

## 8. Strict aliasing violation

- **RULE**: accessing an object through an lvalue of an incompatible type (with narrow
  exceptions for `char*`, union member, etc.) is UB.
- **WHY AI GETS IT WRONG**: assumes "reinterpreting a pointer always works" (type-punning via cast).
- **CORRECT REASONING**: use `memcpy` for type punning, or `union`, or `char*` access; not
  `*(float*)&int_val` unless allowed.
- **EXAMPLE** (bad): `*(float*)&i` (strict aliasing violation unless i is float).
- **COUNTEREXAMPLE** (good): `memcpy(&f, &i, sizeof f);` (compiler optimizes to the same asm).
- **VERIFICATION**: `-fstrict-aliasing -Wstrict-aliasing=2`; `-O2` asm diff.
- **SOURCE**: N1570 §6.5p7; CERT EXP39-C.

## 9. Division by zero / INT_MIN % -1

- **RULE**: `x / 0`, `x % 0`, and `INT_MIN / -1` (overflow) are UB.
- **WHY AI GETS IT WRONG**: assumes "divide by zero raises SIGFPE so it's 'handled'".
- **CORRECT REASONING**: for integers, division by zero is UB (not a guaranteed trap); the
  optimizer may assume the divisor is nonzero. `INT_MIN / -1` overflows and is UB.
- **EXAMPLE** (bad): `avg = sum / count;` where `count` can be 0.
- **COUNTEREXAMPLE** (good): `if (count == 0) ...; else avg = sum / count;`.
- **VERIFICATION**: UBSan `-fsanitize=integer-divide-by-zero`; `-fsanitize=divide`.
- **SOURCE**: N1570 §6.5.5p5; cppreference "division by zero".

## 10. Modifying a string literal

- **RULE**: attempting to modify a string literal is UB (literals may live in read-only memory).
- **WHY AI GETS IT WRONG**: `char *s = "abc"; s[0] = 'x';` — assumes literals are writable.
- **CORRECT REASONING**: use `const char *s = "abc";` or `char s[] = "abc";` (mutable copy).
- **EXAMPLE** (bad): `char *p = "abc"; p[0] = 'A';`.
- **COUNTEREXAMPLE** (good): `char p[] = "abc"; p[0] = 'A';`.
- **VERIFICATION**: `-Wwrite-strings`; runtime segfault on rodata.
- **SOURCE**: N1570 §6.4.5p7; CERT STR30-C.

## 11. Accessing an object after its lifetime ends (temporary)

- **RULE**: referencing an object outside its lifetime is UB (e.g. returning pointer to a
  local, or modifying a temporary).
- **WHY AI GETS IT WRONG**: "the stack memory still holds the value, so it works".
- **CORRECT REASONING**: lifetime ended → any use is UB; it may appear to work at -O0 and
  break at -O2.
- **EXAMPLE** (bad): `int *f() { int x = 5; return &x; }`.
- **COUNTEREXAMPLE** (good): return by value, or `static`/`malloc` with explicit lifetime.
- **VERIFICATION**: `-Wdangling-pointer`; ASan stack-use-after-return.
- **SOURCE**: N1570 §6.2.4; CERT EXP35-C.

## 12. Passing non-null-terminated string to string functions

- **RULE**: passing a non-null-terminated char sequence to a string function expecting
  NUL-termination is UB (over-read).
- **WHY AI GETS IT WRONG**: assumes `strncpy` result is always NUL-terminated.
- **CORRECT REASONING**: `strncpy(dst, src, n)` omits NUL if `src` is `>= n` bytes. Always
  terminate explicitly or use `snprintf`.
- **EXAMPLE** (bad): `strncpy(dst, src, sizeof dst); strlen(dst);` (may over-read).
- **COUNTEREXAMPLE** (good): `dst[sizeof dst - 1] = '\0';` after copy, or `snprintf`.
- **VERIFICATION**: `-Wstringop-truncation`; ASan; clang-tidy `bugprone-not-null-terminated-result`.
- **SOURCE**: N1570 §7.24.2.4; CERT STR32-C, STR31-C.

## Quick detection table

| UB class | Tool | Flag |
|---|---|---|
| signed overflow | UBSan | `-fsanitize=signed-integer-overflow` |
| shift | UBSan | `-fsanitize=shift` |
| OOB / null / UAF | ASan | `-fsanitize=address` |
| uninitialized | MSan | `-fsanitize=memory` |
| aliasing | compiler | `-Wstrict-aliasing=2` |
| integer divide | UBSan | `-fsanitize=integer-divide-by-zero` |
| overlapping memcpy | ASan | `-fsanitize=address` (memcpy-param-overlap) |
