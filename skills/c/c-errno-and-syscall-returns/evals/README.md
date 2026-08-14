# Evaluation — c-errno-and-syscall-returns

Skill: `skills/c/c-errno-and-syscall-returns`. Stability target: `evaluated`.
Bug classes: A17 (errno checked too late / wrong branch, CWE-248), A18 (EINTR not handled,
CWE-252), A19 (negative syscall return treated as success, CWE-252).

## Historical CVE eval

| CVE | Class | Fixture | Detect | Fix | Verify |
|---|---|---|---|---|---|
| CVE-2024-32650 | EOF return mishandling | rustls `complete_io` (Rust) | `read() == 0` (clean EOF) treated as incomplete read | stop the loop on `Ok(0)` | hang test / bound the loop |

Each eval: DETECT (find the missing/incorrect return check) -> EXPLAIN (name the rule in
`references/errno-syscalls.md`) -> FIX (retry/accumulate/check errno in the right place)
-> VERIFY (rerun the example pair).

## Synthetic evals

- easy/negative: `examples/bad/errno_stale.c` — stale `errno == ERANGE` after a successful
  `strtol` is reported as a false failure. Must detect "errno read without clearing and not on a
  failure path" (Rule 1/3).
- easy/negative: `examples/bad/read_no_eintr.c` — single-shot `_read`, no `EINTR` retry, no
  short-count accumulation; 20-byte payload through an 8-byte buffer truncates (Rules 5/7/9).
- medium/negative: `examples/bad/errno_late.c` — `errno` read after an intervening `fopen`
  failure clobbers the `strtol` result (Rule 2).
- medium/negative: `examples/bad/read_retval_ignored.c` — `_read` return ignored so EOF is
  invisible and stale data is reprocessed (Rules 6/8).
- hard/adversarial: a signal delivered between a blocking `read` and its `errno` check on POSIX
  (EINTR arrives, code reports a fatal error instead of retrying) — simulate with a mock that
  returns `-1`/`EINTR` once.

## False-positive evals (correct code must not be flagged)

- `examples/good/errno_reset_and_check.c` — `errno = 0` before, immediate check on the failure
  path only — must NOT be flagged.
- `examples/good/read_full_eintr_loop.c` and `write_full_eintr_loop.c` — EINTR retry + short-count
  accumulation + EOF break — must NOT be flagged.
- `examples/good/read_eof_is_not_error.c` — `_read` returning 0 handled as EOF — must NOT be
  flagged.
- `examples/good/open_close_check.c` — `fd == -1` check, errno read immediately — must NOT be
  flagged.

## Verification commands

```
gcc -Wall -Wextra -Werror -O2 examples/good/errno_reset_and_check.c -o out && ./out   # exit 0
gcc -Wall -Wextra -Werror -O2 examples/good/read_full_eintr_loop.c -o out && ./out   # exit 0
gcc -Wall -Wextra -Werror -O2 examples/good/write_full_eintr_loop.c -o out && ./out  # exit 0
gcc -Wall -Wextra -Werror -O2 examples/good/open_close_check.c -o out && ./out       # exit 0
gcc -Wall -Wextra -Werror -O2 examples/good/read_eof_is_not_error.c -o out && ./out  # exit 0
gcc -Wall -Wextra -Werror -O2 examples/bad/errno_stale.c -o out && ./out             # exit 1 (BUG)
gcc -Wall -Wextra -Werror -O2 examples/bad/errno_late.c -o out && ./out              # exit 1 (BUG)
gcc -Wall -Wextra -Werror -O2 examples/bad/read_no_eintr.c -o out && ./out           # exit 1 (BUG)
gcc -Wall -Wextra -Werror -O2 examples/bad/read_retval_ignored.c -o out && ./out     # exit 1 (BUG)
```

On Linux: `strace -e trace=read,write` or `perf trace` to observe syscall returns and `EINTR`.
Sanitizers do not detect errno misuse; these runtime demos plus review are the detector.

## Verified facts (2026-08-14, gcc 16.1 MinGW, Windows CRT)

- All 9 examples compile with `-Wall -Wextra -Werror -O2` (zero warnings).
- `errno_stale.c`: stale `errno=34` (ERANGE) survives a successful `strtol("123")` -> false
  failure, exit 1. Confirms N1570 §7.5p3: library functions never set errno to 0.
- `errno_late.c`: failed `fopen` sets `errno=2` (ENOENT), clobbering the strtol check, exit 1.
- `read_no_eintr.c`: `_read` returns 8 of 20 requested bytes (short count) -> truncation, exit 1.
- `read_retval_ignored.c`: second `_read` returns 0 (EOF) with the buffer untouched -> stale data
  reprocessed, exit 1.
- Good examples all exit 0: parse succeeds despite stale errno; full-read loop assembles 8 bytes
  across multiple `_read` calls from a 20-byte file; full-write loop round-trips 20 bytes; EOF
  returns 0 cleanly; open/close sentinels checked.
- Windows-CRT notes: `_read`/`_write` return -1 and set errno, matching the POSIX shape; `EINTR`
  is rarely delivered by the CRT (no POSIX signal model), so the retry loops are written as the
  portable pattern with POSIX as the primary target (INFERRED, marked in references).

## Scoring (for routing eval)

- precision: every flagged line must map to a rule in `references/errno-syscalls.md`.
- recall: each bad example must be detected (stale errno, late errno, no EINTR, ignored return).
- FP-rate: good examples must produce zero flags.
