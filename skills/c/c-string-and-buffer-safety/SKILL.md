---
name: c-string-and-buffer-safety
description: Use when writing or reviewing C code that copies strings or fills buffers — strncpy, snprintf, strcpy, strcat, memcpy/memmove, sizeof on arrays vs pointers, _FORTIFY_SOURCE, or anything that can overrun or fail to NUL-terminate a buffer.
---

# C String & Buffer Safety

## When to use

- Writing C that copies user-controlled or variable-length strings into fixed buffers.
- Reviewing code that uses `strncpy`, `snprintf`, `strcpy`, `strcat`, `sprintf`, `gets`, `memcpy`, `memset`.
- Checking whether a buffer is large enough for what is copied into it.
- Reasoning about truncation: does the call report when the output was cut off?
- Porting code between BSD (strlcpy/strscpy), Linux kernel, and standard C11 environments.

## When not to use

- Pure C++ code with `std::string` / `std::vector` — see `raii-descriptor-types-api-design`.
- Rust — ownership + bounds checks are compile-time; use `rust-unsafe-reasoning` for unsafe blocks.
- When the concern is a specific UB construct rather than string copying, route through `c-undefined-behavior`.
- Memory allocation correctness (leaks, double-free) — that is memory ownership, not buffer sizing.

## What the agent often gets wrong

- "strncpy always NUL-terminates." It does NOT: if `src` is `>= n` bytes, the destination is
  left without a NUL terminator (N1570 §7.24.2.4; CERT STR32-C, STR31-C).
- "snprintf is always safe, so I can ignore its return value." Truncation silently drops data;
  the return value is the length that WOULD have been written, so it must be checked.
- "sizeof(ptr) gives the buffer size." Inside a function, `char buf[]` param decays to `char *`;
  `sizeof` then gives 8, not the caller's buffer length.
- "strcpy is fine for short input." Input length is rarely bounded at the call site; strcpy is
  unbounded by design.
- "memcpy is just memmove that is faster." Overlap makes memcpy UB; memmove is the only
  overlap-safe copy.
- "_FORTIFY_SOURCE will save me." It only detects SOME overflows at runtime (when size is known
  at compile time) and aborts; it does not make broken code correct.

## How to reason correctly

1. Identify every buffer, its true capacity, and the maximal length of the source.
2. Prefer a single NUL-terminating primitive: `snprintf` with the destination size, then
   CHECK the return value (`>= sizeof(dst)` means truncated).
3. When forced to use `strncpy`, explicitly NUL-terminate afterwards or the copy is unusable
   as a string.
4. Never write `sizeof` on an array that has decayed to a pointer; thread the buffer length as
   a parameter, or use a `char (*)[N]` / `char (*)[*]` typed pointer to preserve the size.
5. For overlapping regions use `memmove`, never `memcpy`.
6. At the API boundary, prefer bounds-taking interfaces (`snprintf`, `strlcpy`, `strscpy`,
   `strncpy_s` where available) over unbounded ones.

## What to verify

- Every `snprintf` return value is compared against the buffer size; truncation is handled.
- Every `strncpy` result is NUL-terminated, OR the buffer is used as a byte array, not a string.
- No `sizeof` applied to a decayed pointer.
- No `strcpy`/`strcat`/`sprintf`/`gets` on untrusted input.
- `memcpy` destinations never overlap sources; if they might, `memmove`.
- Code is built with `-Wall -Wextra` and (on glibc) `-D_FORTIFY_SOURCE=2 -O2`; the warnings
  that appear are either fixed or explained.

## How to verify

```
gcc -Wall -Wextra -Werror -O2 -D_FORTIFY_SOURCE=2 file.c -o out && ./out
# expect: compile clean, runtime asserts pass
# -Wstringop-truncation, -Wformat-truncation, -Wsizeof-pointer-memaccess fire on bad code
```

For a truncation/non-termination check, build a small runtime probe (see examples) that fills
the buffer and asserts `dst[sizeof dst - 1] == '\0'` after the copy. ASan
(`-fsanitize=address`) and clang-tidy (`clang-analyzer-security.insecureAPI.strcpy`,
`bugprone-not-null-terminated-result`, `bugprone-suspicious-memory-comparison`) catch the
runtime and static cases.

## Where the knowledge comes from

- ISO C11 N1570 §7.21.6.5 (snprintf), §7.24.2.4 (strncpy), §7.24.2.1/2.2 (memcpy/memmove),
  §7.24.2.3/2.4 (strcat/strcpy, no bounds)
- cppreference C `strncpy`/`snprintf`/`strlcpy` behavior notes
- SEI CERT C (STR30-C, STR31-C, STR32-C, STR35-C, ARR01-C, MEM03-C)
- GCC documentation: `-Wstringop-truncation`, `-Wformat-truncation`, `_FORTIFY_SOURCE`
- curl CVE-2023-38545, OpenSSL CVE-2022-3602, nginx CVE-2021-23017

## Related skills

- `c-undefined-behavior` — out-of-bounds / non-termination are UB (require of)
- `c-integer-promotion-and-conversion` — length arithmetic (`sizeof` is `size_t`)
- `sanitizer-report-reading` — ASan reports from these bugs (recommend)
- `compiler-ub-assumptions` — why the compiler can assume a buffer is NUL-terminated

## Evaluation

Historical CVEs: CVE-2023-38545 (oversized hostname), CVE-2022-3602 (punycode off-by-one),
CVE-2021-23017 (resolver off-by-one write). Synthetic: strncpy non-termination,
unchecked snprintf truncation, sizeof(ptr). False-positive: correct strncpy+terminate and
checked snprintf must NOT be flagged.
