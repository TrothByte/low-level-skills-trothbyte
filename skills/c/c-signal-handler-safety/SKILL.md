---
name: c-signal-handler-safety
description: Use when writing or reviewing C code that installs or runs signal handlers — SIGINT/SIGTERM shutdown, async flag set, EINTR handling, crash handlers. Covers async-signal-safe functions, volatile sig_atomic_t, sigaction vs signal, self-pipe trick, and signal behavior in multithreaded programs.
---

# C Signal Handler Safety

## When to use

- Installing a handler with `signal()` or `sigaction()` — SIGINT/SIGTERM shutdown,
  "set a flag" patterns, watchdog or crash handlers.
- Code that calls `write`/`_write` or sets a flag from a handler, or defers work
  to a main loop after a signal.
- Debugging a program that hangs, crashes, or drops data only after Ctrl+C or
  `kill` — the classic signal-vs-malloc/stdio deadlock.
- Reviewing C that intersects with `c-errno-and-syscall-returns` (EINTR is caused
  by signals; interrupted syscalls must be retried there).

## When not to use

- POSIX delivery/semantics on Windows — Windows console signals are a limited
  subset (see references); MinGW has no `sigaction`. This skill targets POSIX
  semantics as primary and marks Windows-CRT differences.
- Writing signal handlers in Rust or C++ with exceptions — separate skills.
- Pure computation with no handler installed — see `c-undefined-behavior`.
- `longjmp`/`siglongjmp` design decisions and kernel signal handling — out of scope.

## What the agent often gets wrong

- "It ran and printed the message, so the handler is fine." A non-async-signal-safe
  call inside a handler can work in a synchronous test and deadlock in production
  when the signal interrupts the same lock it needs.
- "printf is safe, it's just output." `printf`/`fprintf`/`fwrite` lock stdio
  internals and are NOT on the async-signal-safe list.
- "malloc is safe unless allocation fails." The heap allocator uses internal locks;
  a signal delivered inside another `malloc` deadlocks the handler.
- "A plain `volatile int` flag is fine." Only `volatile sig_atomic_t` (and C11
  lock-free atomics) are safe to share with a handler.
- "One handler call, then never again." ISO `signal()` resets the handler to
  `SIG_DFL` before entry, so a second signal terminates the program.
- "A SIGSEGV handler that returns resumes the program." Returning from a
  computational-exception handler (SIGFPE/SIGILL/SIGSEGV) is undefined behavior.
- "The handler runs in the main thread." The delivery thread is unspecified in
  multithreaded programs; the handler may run concurrently with other threads.

## How to reason correctly

1. Enumerate every function the handler can reach, directly or transitively, and
   check each against the POSIX async-signal-safe list (see `references/signal-safety.md`).
   Not on the list → not allowed, regardless of how harmless it looks.
2. For shared state, use `volatile sig_atomic_t` (or lock-free C11 atomics) — the
   only types whose reads/writes are atomic with respect to signals.
3. Keep the handler minimal: set a flag and return (or `_Exit` on a repeated
   signal). Defer all real work — allocations, stdio, logging, cleanup — to the
   main loop.
4. If the main loop blocks (select/poll/read), a flag alone is not enough: use the
   self-pipe trick so the loop wakes up when the signal arrives.
5. For process-directed signals in multithreaded programs, prefer blocking the
   signal in all threads and handling it in one dedicated thread (`sigwait`,
   POSIX), or `sigaction` from the main thread — never `signal()` from workers.

## What to verify

- Every call reachable from the handler is on the async-signal-safe list.
- All objects shared with the handler are `volatile sig_atomic_t` (no plain
  globals, no `volatile int`, no `long long`).
- Handler returns only for non-computational signals (SIGINT/SIGTERM/...);
  SIGFPE/SIGILL/SIGSEGV handlers terminate or never return.
- Blocking main loop wakes on the signal (self-pipe or equivalent) — a pure flag
  is insufficient when the loop is blocked.
- No `signal()` call from a multithreaded program; single-threaded `signal()` use
  accounts for the one-shot reset.
- Compiles clean under `gcc -Wall -Wextra -Werror -O2`.

## How to verify

```
gcc -Wall -Wextra -Werror -O2 examples/good/volatile_flag_write_handler.c -o out && ./out
```

- Raise the handled signal (`raise(SIGINT)`) synchronously and assert the flag is
  set and the `_write` output appears (good example; verified on MinGW).
- Review the handler body against the allow-list — grep for `printf|malloc|free|
  fwrite|fopen|fclose|pthread_mutex|memcpy|strlen|sprintf` inside it.
- On Linux, deliver the signal asynchronously (`kill -INT`) while the main loop is
  blocked inside `select` on a self-pipe and assert the loop wakes exactly once.
- `-O2` asm check: a `volatile sig_atomic_t` flag read must not be hoisted out of
  the polling loop; a non-volatile flag read may be cached by the optimizer.

## Where the knowledge comes from

- ISO C11 N1570 §7.14.1.1 (signal: handler reset, allowed library calls, shared
  objects), §7.14p5 (sig_atomic_t), §5.1.2.3p5, §7.22.4.5 (_Exit).
- POSIX Signal Concepts — the async-signal-safe function list (primary target).
- SEI CERT C — SIG30-C (async-signal-safe calls), SIG31-C (shared objects),
  SIG32-C (signal() in multithreaded programs), SIG34-C (signal() from within
  handlers), SIG35-C (returning from computational-exception handlers).
- cppreference C signal page — behavior summary.
- MinGW/Windows CRT facts verified on gcc 16.1 (2026-08-14); POSIX is the
  documented primary target, Windows differences marked.

## Related skills

- `c-errno-and-syscall-returns` — require (EINTR comes from signals; retry loops).
- `c-undefined-behavior` — handler state/UB edges.
- `safe-low-level-from-scratch` — positive writing path.

## Evaluation

- Synthetic negative: `printf` in handler, `malloc` in handler (both compile clean
  under `-Wall -Wextra -Werror -O2` and "work" under synchronous `raise()` on
  Windows — the false-negative trap). Detector: rule-based review against SIG30-C.
- Synthetic positive: `volatile sig_atomic_t` flag + `_write`-based handler runs
  and returns 0 (verified).
- Adversarial: signal delivered while the main thread is inside `printf`/`malloc`
  on POSIX — the handler deadlocks; documented trigger, not reproducible under
  single-threaded `raise()` on Windows.
- False-positive: a correct flag+`_write` handler and a POSIX `sigaction` +
  self-pipe loop must NOT be flagged.
- Historical: returning-from-SIGSEGV and one-shot `signal()` reset are documented
  UB (C11 7.14.1.1); a specific CVE fixture was not verified this session
  (UNVERIFIED — see `evals/README.md`).
