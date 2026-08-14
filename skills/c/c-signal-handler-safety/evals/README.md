# Evaluation — c-signal-handler-safety

Skill: `skills/c/c-signal-handler-safety`. Stability target: `evaluated`.
Bug classes: SIG30-C (non-async-signal-safe call in a handler), SIG31-C (shared
object accessed from a handler), SIG32-C/SIG34-C (signal() misuse in threads /
one-shot reset), SIG35-C (returning from a computational-exception handler).

## Historical CVE eval

A specific CVE fixture was not verified this session (UNVERIFIED): signal-handler
deadlocks are real and documented (CERT SIG30-C rationale cites production
incidents where handlers re-entered malloc/stdio), but no CVE id was confirmed
against a primary source here. Do not claim one. The documented failure mode —
signal delivered while the main thread holds the stdio or heap lock, handler
re-enters the same lock and deadlocks — is reproduced conceptually by the two bad
fixtures, and the one-shot `signal()` reset and returning-from-SIGSEGV rules are
grounded in C11 7.14.1.1 directly.

## Synthetic evals

- easy/negative: `examples/bad/printf_in_handler.c` — `printf` in a SIGINT
  handler; rule 1/2 (SIG30-C). Compiles clean under `-Wall -Wextra -Werror -O2`,
  which is the point: the compiler cannot see the bug.
- easy/negative: `examples/bad/malloc_in_handler.c` — `malloc`/`free` in a SIGINT
  handler; rule 1/2 (SIG30-C).
- medium/negative: plain `static int flag` set in a handler and polled in a loop
  — rule 3 (SIG31-C); at `-O2` the read is hoisted out of the loop.
- medium/negative: two `raise(SIGINT)` calls with a `signal()`-installed handler —
  second delivery takes SIG_DFL (one-shot reset), rule 4.
- hard/negative: handler for SIGSEGV sets a flag and returns — rule 5 (SIG35-C),
  UB.
- hard/negative: handler calls `pthread_mutex_lock` on a counter shared with the
  main thread — rule 6 (SIG32-C).
- adversarial: SIGINT delivered asynchronously (POSIX `kill`) while the main
  thread is inside `printf`/`malloc`; the handler deadlocks. Documented trigger;
  not reproducible under single-threaded `raise()` on Windows (VERIFIED).

## False-positive evals (correct code must not be flagged)

- `examples/good/volatile_flag_write_handler.c` — `volatile sig_atomic_t` flag +
  `_write`-based handler, cleanup in main — must NOT be flagged.
- A POSIX `sigaction` handler with `SA_RESTART` that stays installed — must NOT be
  flagged as "one-shot".
- A POSIX self-pipe loop where the handler writes one byte and the loop does the
  cleanup — must NOT be flagged.
- A SIGSEGV handler that calls `_Exit` without returning — must NOT be flagged.

## Verification commands

```
gcc -Wall -Wextra -Werror -O2 examples/good/volatile_flag_write_handler.c -o out && ./out   # exit 0
gcc -Wall -Wextra -Werror -O2 examples/bad/printf_in_handler.c -o out && ./out              # exit 1 (BUG)
gcc -Wall -Wextra -Werror -O2 examples/bad/malloc_in_handler.c -o out && ./out              # exit 1 (BUG)
```

Sanitizers do not detect non-async-signal-safe handler calls; the detector is a
rule-based review (grep the handler body against the allow-list) plus the runtime
demos above.

## Verified facts (2026-08-14, gcc 16.1 MinGW, Windows CRT)

- All 3 examples compile with `-Wall -Wextra -Werror -O2` (zero warnings).
- `signal(SIGINT, h)` + `raise(SIGINT)`: handler runs synchronously, flag set,
  `_write(1, ...)` emits the handler line, exit 0 (good example).
- `signal(SIGTERM, h)` + `raise(SIGTERM)`: handler runs, exit 0.
- `signal(SIGSEGV, h)` + `raise(SIGSEGV)`: handler runs and returning resumes the
  caller on the Windows CRT — differs from POSIX, where returning from a
  computational-exception handler is documented UB (C11 7.14.1.1p5).
- Real null-pointer dereference does NOT reach a SIGSEGV handler on Windows;
  process exits unhandled 0xC0000005 (observed exit -1073741819).
- `printf` and `malloc` inside a handler run without crashing under synchronous
  `raise()` on Windows (both bad fixtures exit with the expected BUG message) —
  the hazard is latent here and deterministic on POSIX with async delivery.
- `sigaction` is NOT declared in MinGW `<signal.h>` (compile error); only ISO
  `signal()` is available. POSIX `sigaction` is the documented primary target on
  Linux/BSD.

## Scoring (for routing eval)

- precision: every flagged line must map to a rule in `references/signal-safety.md`.
- recall: each bad fixture must be detected (printf, malloc, shared object,
  one-shot reset, computational-exception return, lock in handler).
- FP-rate: good fixtures (flag + write handler, sigaction, self-pipe, _Exit in
  SIGSEGV handler) must produce zero flags.
