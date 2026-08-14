---
name: c-errno-and-syscall-returns
description: Use when writing or reviewing C code that calls libc or system calls which report errors through errno and negative return values — read, write, accept, connect, open, close, strtol. Covers errno discipline, EINTR retry loops, partial read/write handling, EOF detection, and fd validation.
---

# C errno & Syscall Return Handling

## When to use

- C crossing the OS boundary: `read`/`write`/`accept`/`connect`, `open`/`close`, and libc
  functions that report errors via errno (`strtol`, `strtod`, `fread`, `fwrite`, `malloc`, `fopen`).
- Flaky I/O: lost or duplicated data, false errors, hangs, failures only under load or after signals.
- Retry/aggregation: read a buffer "until done", write "until all bytes are out".

## When not to use

- Pure computation with no libc/OS boundary — see `c-undefined-behavior`.
- Win32 `FALSE`/`GetLastError`/`HRESULT` conventions — different model; this skill covers CRT
  `_read`/`_write` and POSIX, not `CreateFile`/`ReadFile`.
- C++ exceptions/RAII — `cpp-object-lifecycle`.
- Inside signal handlers — `c-signal-handler-safety` (requires this skill).

## What the agent often gets wrong

- Checks `errno` after a success or instead of the return value (ERR30-C).
- Reads `errno` too late — `printf`/`fopen` in between may clobber it.
- Assumes calls reset `errno`; library functions never set it to 0, so stale errors leak (N1570 §7.5).
- No `EINTR` retry — one interrupted `read`/`write` drops or duplicates data.
- One `read()` assumed to fill the buffer: short counts silently truncate.
- `read() == 0` treated as error or "no data yet" — false errors, infinite loops.
- `if (!fd)` — fd 0 (stdin) is valid and falsy.
- Uninitialized buffer tail when the byte count is not checked.

## How to reason correctly

1. Return value first, `errno` second — errno is valid only when the return says "failed".
2. On failure, save `errno` immediately, before any other call.
3. Set `errno = 0` before the call when errno is the only failure signal.
4. Three outcomes for byte-stream I/O: `n > 0` data, `n == 0` EOF, `n < 0` error (retry `EINTR`).
5. Test `fd == -1`/`f == NULL`, never truthiness.
6. POSIX is the reference model; Windows CRT rarely delivers `EINTR`.

## What to verify

- `errno` read only on failure branches, immediately after the call.
- `errno = 0` set before calls whose only signal is errno.
- I/O loops retry `EINTR`, accumulate short counts, stop on EOF.
- `read() == 0` handled as EOF; FDs checked `== -1`, never `!fd`.
- Compiles clean under `gcc -Wall -Wextra -Werror -O2`.

## How to verify

```
gcc -Wall -Wextra -Werror -O2 examples/good/read_full_eintr_loop.c -o out && ./out
gcc -Wall -Wextra -Werror -O2 examples/good/write_full_eintr_loop.c -o out && ./out
```

- Mock read/write returning short counts and one `EINTR` — the loop must complete.
- Linux: `strace`/`perf trace` to observe real syscall returns.
- Sanitizers do not catch errno misuse; review plus runtime demos are the detector.

## Where the knowledge comes from

- ISO C11 N1570 §7.5 — errno is 0 at startup, never set to 0 by library functions; thread-local.
- cppreference C errno page — functions may write positive errno without error; check immediately.
- SEI CERT C — ERR30-C (clear errno before, check only after failure), ERR33-C (detect library errors).
- perf wiki / Linux tracing — syscall returns and `EINTR` observed in the wild.
- POSIX read/write/accept/connect semantics (primary target; Windows-CRT differences marked).

## Related skills

- `c-undefined-behavior` — require (short-read tails and stale values edge into UB).
- `c-signal-handler-safety` — requires this skill; `EINTR` comes from signals.
- `c-string-and-buffer-safety`, `safe-low-level-from-scratch`, `sanitizer-agent-ci-loop`.

## Evaluation

- Historical CVE: CVE-2024-32650 — rustls `complete_io` loops forever on clean EOF (`read() == 0`),
  DoS (CWE-835).
- Synthetic: stale errno after successful `strtol`; errno read after an intervening `fopen`;
  single-shot `_read` truncation; ignored `_read` return (EOF invisible). Each maps to a rule
  in `references/errno-syscalls.md`.
- False-positive: correct `errno = 0` + immediate check and correct EINTR/EOF loops must NOT be
  flagged.
