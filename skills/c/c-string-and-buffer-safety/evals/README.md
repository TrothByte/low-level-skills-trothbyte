# Evaluation — c-string-and-buffer-safety

Skill: `skills/c/c-string-and-buffer-safety`. Stability target: `evaluated`.
Registry entries: `registry/evals.yaml` (historical CVE CVE-2023-38545, CVE-2021-23017;
secondary CVE-2014-0160; routing; FP-01), `registry/claims.yaml` CL-001.

## Historical CVE evals

| CVE | Class | Fixture | Detect | Fix | Verify |
|---|---|---|---|---|---|
| CVE-2023-38545 | heap overflow, oversized hostname | curl SOCKS5 state machine | hostname length not checked against buffer bound before copy | reject hostnames > 255 bytes (CURLPX_LONG_HOSTNAME) | ASan + oversized hostname test |
| CVE-2022-3602 | off-by-one (punycode) | OpenSSL `ossl_punycode_decode` | `written_out > max_out` should be `>=` | 1-line comparison fix | ASan + crafted punycode input |
| CVE-2021-23017 | off-by-one write | nginx `ngx_resolver_copy` | terminating NUL written 1 byte past allocated buffer | allocate +1 for terminator | ASan + crafted DNS response |

Secondary: CVE-2014-0160 (Heartbleed — missing bounds check before response copy).

Each eval: DETECT (find the unchecked copy/termination) → EXPLAIN (name the rule from
`references/string-safety.md`) → FIX (bound the copy) → VERIFY (compile clean + runtime
termination/truncation probe or ASan).

## Synthetic evals

- **easy/negative**: `strncpy` into a full buffer without explicit termination — must flag
  non-termination (STR32-C). See `examples/bad/strncpy_bad.c`.
- **easy/positive**: correct `snprintf` + return check — must NOT flag.
- **medium/negative**: unchecked `snprintf` return used later — must detect silent truncation.
- **medium/negative**: `sizeof` on a decayed array parameter used as a bound — must detect
  (see `examples/bad/sizeof_ptr_bad.c`).
- **hard/negative**: `strcpy`/`strcat` on user-controlled input into a small buffer — must
  detect unbounded copy.
- **adversarial**: code that compiles and passes tests on a known-good input but overflows on
  a boundary input (e.g. exactly `sizeof buf` bytes), so the agent must reason about capacity,
  not just run the happy path.

## False-positive evals (correct code must not be flagged)

- FP-01 (registry): correct `strncpy` followed by explicit `dst[n-1] = '\0'` — do NOT flag
  non-termination.
- Correct `snprintf` with the return value checked against the buffer size — do NOT flag
  truncation.
- Correct `memcpy` on provably disjoint regions — do NOT demand `memmove`.
- `sizeof` applied at the call site (not on a decayed pointer) — do NOT flag.

## Verification commands

```
gcc -Wall -Wextra -Werror -O2 examples/good/strncpy_good.c -o good.exe && good.exe   # clean, asserts pass
gcc -Wall -Wextra -Wno-error -O2 examples/bad/strncpy_bad.c -o bad.exe   # -Wstringop-truncation
gcc -Wall -Wextra -Wno-error -O2 examples/bad/sizeof_ptr_bad.c -o bad2.exe   # -Wsizeof-pointer-memaccess
```

Runtime probe (strncpy non-termination):
```
gcc -Wall -Wextra -Werror -O2 examples/bad/strncpy_bad.c -o probe.exe && probe.exe
# expect: strlen scans past the 8-byte buffer (over-read), demonstrating no NUL terminator
```

## Verified facts

- `strncpy` into an exactly-full 8-byte buffer leaves no NUL terminator; `strlen` over-reads
  past the buffer (verified with GCC 16.1, output recorded in the WORKLOG/session notes).
- `snprintf` always writes a terminating NUL within the given size; its return value reports
  the length that would have been written, enabling truncation detection.
- `sizeof` on a `char dst[]` parameter equals the pointer size (8 on x64), not the caller's
  buffer size.
- GCC emits `-Wstringop-truncation` for the bad strncpy and `-Wsizeof-pointer-memaccess` for
  the sizeof-pointer bug at `-Wall -Wextra -O2`.

## Scoring (for routing eval)

- precision: every flagged site must map to a real rule in `references/string-safety.md`.
- recall: each bad example must be detected (termination, truncation, sizing, unbounded copy,
  overlap).
- FP-rate: good examples must produce zero flags.
