# C String & Buffer Safety — Rules

Sources: ISO C11 N1570, SEI CERT C, cppreference C behavior, GCC manual.
Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. strncpy does not guarantee NUL-termination

- **RULE**: `strncpy(dst, src, n)` copies at most `n` chars and pads the rest with NULs, but if
  `strlen(src) >= n`, the destination is NOT NUL-terminated. Any later `strlen`/`%s` reads past
  the buffer (over-read, UB) or the caller writes a NUL past the end (overflow).
- **WHY AI GETS IT WRONG**: the "strncpy is the safe strcpy" habit; the `n` looks like a bound
  so it is assumed to behave like a truncating copy.
- **CORRECT REASONING**: `strncpy` is defined to produce a fixed-width field of exactly `n`
  bytes, not a bounded string. After the call, `dst` is only a string if
  `memchr(dst, '\0', n)` finds a NUL. Use `snprintf(dst, n, "%s", src)` or explicit
  `dst[n-1] = '\0'` after the copy.
- **EXAMPLE** (bad):
  ```c
  char buf[16];
  strncpy(buf, long_src, sizeof buf);   // no NUL when long_src >= 16
  printf("%s", buf);                    // reads past buf
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  char buf[16];
  snprintf(buf, sizeof buf, "%s", long_src);   // truncates AND NUL-terminates
  if (strlen(long_src) >= sizeof buf) { /* handle truncation */ }
  ```
- **VERIFICATION**: `-Wstringop-truncation` (GCC 8+); ASan `-fsanitize=address`; clang-tidy
  `bugprone-not-null-terminated-result`.
- **SOURCE**: N1570 §7.24.2.4; CERT STR32-C, STR31-C; cppreference-c-behavior.

## 2. snprintf return value reports truncation

- **RULE**: `snprintf(dst, size, fmt, ...)` returns the number of chars that WOULD have been
  written (excluding the NUL), even if `size` was too small. If the return value `>= size`,
  output was truncated. Ignoring the return silently drops data.
- **WHY AI GETS IT WRONG**: `snprintf` is "the safe version", so the return is treated like
  `printf`'s return (ignored).
- **CORRECT REASONING**: the return is a length, not a status. Compare against the buffer size:
  `int n = snprintf(dst, sizeof dst, ...); if ((size_t)n >= sizeof dst) → truncated`. Negative
  return means an encoding error (not enough info in `%lc`), also UB-ish for callers.
- **EXAMPLE** (bad):
  ```c
  char host[16];
  snprintf(host, sizeof host, "%s", untrusted);   // silently truncated at 15
  connect(host);                                  // uses a partial hostname
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  char host[16];
  int n = snprintf(host, sizeof host, "%s", untrusted);
  if (n < 0 || (size_t)n >= sizeof host) return ERROR_TOO_LONG;
  ```
- **VERIFICATION**: `-Wformat-truncation` (GCC 7+); runtime size probe; clang-tidy
  `bugprone-*` truncation checks.
- **SOURCE**: N1570 §7.21.6.5p3; cppreference-c-behavior; CERT STR35-C.

## 3. strcpy / strcat are unbounded by design

- **RULE**: `strcpy(dst, src)` and `strcat(dst, src)` copy until the source NUL with NO size
  argument. If the source is longer than the destination, this is a classic buffer overflow
  (CWE-121). They must not be used on untrusted or variable-length input.
- **WHY AI GETS IT WRONG**: they "look simple and work in examples"; the agent compiles a test
  with a short literal and never triggers the overflow.
- **CORRECT REASONING**: these functions perform no length check, so correctness depends on a
  property the caller usually cannot prove (source length). The destination size must be known
  and the source checked: use `snprintf(dst, sizeof dst, "%s", src)` or `strlcpy`/`strscpy`.
- **EXAMPLE** (bad):
  ```c
  char name[16];
  strcpy(name, user_input);      // overflow if user_input > 15 bytes
  strcat(name, ".log");          // second unbounded append
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  char name[16];
  int n = snprintf(name, sizeof name, "%s.log", user_input);
  if ((size_t)n >= sizeof name) return ERROR_TOO_LONG;
  ```
- **VERIFICATION**: `-Wstringop-overflow` (GCC 11+); ASan; clang-tidy
  `clang-analyzer-security.insecureAPI.strcpy` / `strcat`; cppcheck.
- **SOURCE**: N1570 §7.24.2.3, §7.24.2.4; CERT STR31-C; CWE-121.

## 4. memcpy requires non-overlapping regions; memmove is overlap-safe

- **RULE**: `memcpy(dst, src, n)` is UB if the regions overlap; `memmove(dst, src, n)` behaves
  as if the bytes are copied through a temporary, so overlap is allowed (N1570 §7.24.2.1p2 vs
  §7.24.2.2).
- **WHY AI GETS IT WRONG**: "memcpy handles overlap anyway" — both compile to the same code on
  many platforms, so it appears to work until the optimizer picks a vectorized direction.
- **CORRECT REASONING**: the standard does not require memcpy to work for overlap; a correct
  implementation may legally corrupt. If you cannot prove non-overlap, use `memmove`.
- **EXAMPLE** (bad):
  ```c
  memcpy(buf + 1, buf, n);   // UB if ranges overlap (they do here)
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  memmove(buf + 1, buf, n);  // defined: copies as through an intermediate
  ```
- **VERIFICATION**: ASan `-fsanitize=address` (memcpy-param-overlap); `-D_FORTIFY_SOURCE=2`
  can abort; code review.
- **SOURCE**: N1570 §7.24.2.1p2, §7.24.2.2; cppreference-c-behavior.

## 5. sizeof(array) vs sizeof(decayed pointer)

- **RULE**: an array parameter `void f(char b[])` decays to `char *`; `sizeof(b)` is the pointer
  size (typically 8), not the buffer size. Using it as a bound under-copies or over-writes.
- **WHY AI GETS IT WRONG**: the declaration syntax `char b[16]` makes the array look sized at
  the callee; the agent writes `sizeof(b)` and assumes 16.
- **CORRECT REASONING**: inside the callee the array size is lost. Thread the size explicitly
  (`f(char *b, size_t cap)`), or pass a pointer-to-array `void f(char (*b)[16])` to preserve
  the extent, or use `char b[static 16]` in the prototype for the compiler to assume capacity.
- **EXAMPLE** (bad):
  ```c
  void copy_into(char dst[16]) {
      strncpy(dst, src, sizeof dst);   // sizeof dst == 8, half-copies
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  void copy_into(char *dst, size_t cap) {
      snprintf(dst, cap, "%s", src);
  }
  ```
- **VERIFICATION**: `-Wsizeof-pointer-memaccess` (GCC 8+); `-Warray-bounds`; code review.
- **SOURCE**: N1570 §6.7.6.3p7 (array-to-pointer adjustment), §6.5.3.4; CERT ARR01-C.

## 6. _FORTIFY_SOURCE is a partial backstop, not a fix

- **RULE**: `_FORTIFY_SOURCE=1/2` (glibc) redirects `strcpy`/`memcpy`/`snprintf` etc. to
  fortified variants that call `__builtin_object_size` and abort on SOME detected overflows.
  It only helps when the compiler knows the object size, and it never restores correctness.
- **WHY AI GETS IT WRONG**: "we build with FORTIFY, so buffer overflows are impossible."
- **CORRECT REASONING**: FORTIFY is a runtime sanity net for known-size objects; it turns some
  silent overflows into `abort()` (DoS) but misses dynamic or heap sizes it cannot prove.
  Fix the code; treat FORTIFY as defense-in-depth alongside ASan.
- **EXAMPLE** (bad):
  ```c
  // with -D_FORTIFY_SOURCE=2 -O2, a known-size strcpy may abort instead of overflow —
  // that is a crash, not a fix.
  char buf[8];
  strcpy(buf, "0123456789");   // fortify may detect; plain build overflows
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  char buf[8];
  snprintf(buf, sizeof buf, "%s", "0123456789");  // correct + fortify-inspectable
  ```
- **VERIFICATION**: build with `-D_FORTIFY_SOURCE=2 -O2`; run with a runtime probe; compare
  abort vs silent corruption.
- **SOURCE**: gcc-manual (Optimize Options, Object Size Checking); glibc fortify docs.

## 7. strlcpy / strscpy semantics (where available)

- **RULE**: BSD `strlcpy(dst, src, size)` always NUL-terminates (when `size > 0`) and returns
  `strlen(src)`, so the caller detects truncation by comparing the return to `size`. Linux
  kernel `strscpy` returns the number of chars copied or `-E2BIG` on truncation, and always
  NUL-terminates. Neither is in ISO C11; do not assume availability.
- **WHY AI GETS IT WRONG**: porting `strlcpy` code to a platform without it, or assuming
  `strscpy`'s return is a length when it is a count/-errno.
- **CORRECT REASONING**: know which standard/library you target. On glibc, use `snprintf`
  (portable) or `strlcpy` if available (>=2.38). In the kernel, `strscpy` is the preferred
  bounded copy; `strlcpy` is deprecated there.
- **EXAMPLE** (bad):
  ```c
  char buf[16];
  strlcpy(buf, src, sizeof buf);   // compiles on BSD, error on strict C11 platforms
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  #ifdef __linux__
  int n = strscpy(buf, src, sizeof buf);   // kernel: -E2BIG on truncation
  #else
  int n = snprintf(buf, sizeof buf, "%s", src);
  if ((size_t)n >= sizeof buf) /* truncated */;
  #endif
  ```
- **VERIFICATION**: compile on the target platform; runtime truncation probe.
- **SOURCE**: cppreference-c-behavior (strlcpy notes); Linux kernel `strscpy` docs; CERT STR35-C.

## 8. Prefer bounds-checked APIs at the boundary

- **RULE**: any interface that accepts an untrusted buffer of unknown length must take an
  explicit size parameter and return an error on truncation. Standard C11 Annex K
  (`strncpy_s`, `snprintf_s`) is optional and rarely fully implemented; portable code uses
  `snprintf` + return check.
- **WHY AI GETS IT WRONG**: mixing "safe" names (`*_s`, `strncpy`) with actual guarantees; or
  assuming Annex K is available everywhere (it is not in glibc by default, MSVC only).
- **CORRECT REASONING**: pick the primitive that the target libc actually provides, then verify
  termination AND truncation detection independently. A function named `*_s` is not magic: check
  its constraints too.
- **EXAMPLE** (bad):
  ```c
  char buf[16];
  strncpy_s(buf, sizeof buf, src, strlen(src));  // on MSVC, may raise constraint handler
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  char buf[16];
  int n = snprintf(buf, sizeof buf, "%s", src);
  if (n < 0 || (size_t)n >= sizeof buf) return ERROR_TRUNCATED;
  ```
- **VERIFICATION**: compile on target libc (glibc/BSD/MSVC); run truncation probe.
- **SOURCE**: N1570 §K.3.7.1.4 (Annex K is optional); CERT STR35-C.

## Historical CVEs mapped to these rules

- **CVE-2023-38545 (curl, SOCKS5)**: heap overflow when a hostname > 255 bytes was passed to a
  SOCKS5 proxy; the buffer bound and the copied length were mismatched (oversized hostname).
  Rule 2/3/5: length not checked against capacity before copy. Verify: ASan + oversized hostname
  test. Source: cwe (CWE-122/787), curl advisory.
- **CVE-2022-3602 (OpenSSL punycode)**: off-by-one in punycode decode: `if (*written_out > max_out)`
  should have been `>=`; a 4-byte overflow on the output buffer. Rule 1/2: boundary comparison
  must be `>=` (i.e. output index may equal the limit). Verify: ASan + crafted punycode.
- **CVE-2021-23017 (nginx resolver)**: off-by-one write in `ngx_resolver_copy`: the terminating
  NUL byte was written one past the allocated buffer. Rule 1/5: allocation size did not include
  the +1 for the terminator. Verify: ASan + crafted DNS response.
