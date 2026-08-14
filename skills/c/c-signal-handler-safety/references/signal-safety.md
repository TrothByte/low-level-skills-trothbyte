# Signal Handler Safety Rules

Primary target: POSIX semantics (async-signal-safe list, sigaction). Windows CRT
differences are marked inline and were verified on MinGW gcc 16.1. Confidence
labels: KNOWN / INFERRED / UNVERIFIED. Rule format: RULE -> WHY AI GETS IT WRONG
-> CORRECT REASONING -> EXAMPLE -> COUNTEREXAMPLE -> VERIFICATION -> SOURCE.

## 1. Only async-signal-safe functions may be called from a handler (SIG30-C)

- **RULE**: A signal handler may call only functions on the POSIX
  async-signal-safe list. For ISO C signals delivered via `abort`/`raise`, the
  handler may call only `abort`, `_Exit`, `quick_exit`, `signal`, `raise`
  (C11 7.14.1.1). Anything else is undefined behavior.
- **WHY AI GETS IT WRONG**: judges safety by how the function *looks* ("it does
  not allocate or print, so it must be fine") and by "it ran once without
  crashing" — a single synchronous test proves nothing about a signal that
  interrupts arbitrary code.
- **CORRECT REASONING**: safety is defined by membership in a fixed list, not by
  appearance. `malloc`, `printf`, and locks are excluded because they take
  internal locks that the interrupted code may already hold, and re-entering them
  from the handler deadlocks or corrupts state (KNOWN, POSIX Signal Concepts;
  CERT SIG30-C; C11 7.14.1.1). If a function is not on the list, it must not be
  called, period.
- **EXAMPLE** (bad):
  ```c
  static void h(int s) {
      (void)s;
      printf("interrupted!\n");        /* printf: not async-signal-safe */
      char *p = malloc(64);            /* malloc: not async-signal-safe */
      free(p);
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static volatile sig_atomic_t stop = 0;
  static void h(int s) {
      (void)s;
      stop = 1;                        /* only sig_atomic_t is shared */
      (void)_write(1, "int\n", 4);     /* write is on the list */
  }
  ```
- **VERIFICATION**: enumerate every call transitively reachable from the handler
  and check each against the async-signal-safe list; grep the handler body for
  `printf|malloc|free|fprintf|fwrite|fopen|fclose|pthread_mutex|memcpy|strlen|
  sprintf`.
- **SOURCE**: cert-c (SIG30-C); iso-c11-n1570 (7.14.1.1); cppreference-c-behavior
  (signal page).

## 2. The safe set: write, read, signal, sigaction, _Exit, _exit, kill, getpid, ...

- **RULE**: Safe in a handler: `write`/`_write`, `read`/`_read`, `signal`,
  `sigaction`, `_Exit`/`_exit`, `kill`, `raise`, `getpid`, `open`/`openat`,
  `close`, `fstat`, `lseek`, `fsync`, `umask`, `setuid`, `dup` and a few dozen
  more in the POSIX list. NOT safe: the entire stdio family (`printf`, `fprintf`,
  `snprintf`, `sprintf`, `fwrite`, `fopen`, `fclose`, `fflush`, `ferror`), the
  whole heap family (`malloc`, `free`, `realloc`, `calloc`), `strlen`/`strcpy`/
  `memcpy`/`memset`, and every pthread lock/cond function.
- **WHY AI GETS IT WRONG**: memorizes "printf and malloc are bad" and treats every
  other function as implicitly safe, or believes a small pure function such as
  `memcpy` must be allowed because it has no locks.
- **CORRECT REASONING**: the list is closed and short. stdio functions lock the
  `FILE` object; heap functions lock the allocator; `pthread_mutex_*` locks user
  mutexes. A signal delivered while the interrupted code holds any of those locks
  makes the handler deadlock on re-entry. `_Exit`/`_exit` never return to the
  heap or stdio, which is why they are allowed (KNOWN, POSIX Signal Concepts;
  CERT SIG30-C; N1570 7.22.4.5). Note `_write` is the CRT equivalent of `write`
  on MinGW and shares the same POSIX shape; EINTR/short-count handling is
  `c-errno-and-syscall-returns`.
- **EXAMPLE** (bad):
  ```c
  static void h(int s) {
      (void)s;
      fprintf(stderr, "caught %d\n", s);   /* stdio lock */
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static volatile sig_atomic_t stop = 0;
  static void h(int s) {
      (void)s;
      stop = 1;
      if (s == SIGINT) (void)_write(1, "SIGINT\n", 7);
      else              _Exit(128 + s);    /* hard terminate, no re-entry */
  }
  ```
- **VERIFICATION**: for each call in the handler, either it is on the POSIX list
  or the fixture must be rewritten; the good example must exit 0 under
  `raise(SIGINT)` on MinGW.
- **SOURCE**: cert-c (SIG30-C); iso-c11-n1570 (7.14.1.1, 7.22.4.5);
  cppreference-c-behavior (signal page).

## 3. volatile sig_atomic_t is the only shared type a handler may touch (SIG31-C)

- **RULE**: For a signal not delivered by `abort`/`raise`, a handler may refer to
  objects with static or thread storage duration only if they are `volatile
  sig_atomic_t` (C11 7.14.1.1p5, 7.14p5, 5.1.2.3p5). Lock-free C11 atomic objects
  are the other sanctioned exception. The handler's own automatic locals are fine.
- **WHY AI GETS IT WRONG**: uses `static int flag`, `static bool running`, or
  `volatile int` and expects them to behave; forgets the `volatile` so the
  optimizer caches the value and the loop never sees the update; shares a struct
  or buffer expecting an "atomic" update.
- **CORRECT REASONING**: `volatile` forces a fresh read/write of the object at
  every use, so the compiler cannot hoist the flag check out of a loop. `sig_atomic_t`
  is the type guaranteed to be read/written as a single unit in the presence of
  asynchronous interrupts (typically `int`). A `volatile long long` is atomic
  neither in C nor on many targets (torn write possible). Accessing a
  non-`volatile sig_atomic_t` static/thread object from a handler is undefined
  behavior (KNOWN, N1570 5.1.2.3p5, 7.14.1.1p5; CERT SIG31-C).
- **EXAMPLE** (bad):
  ```c
  static int interrupted = 0;           /* not sig_atomic_t, not volatile */
  static void h(int s) { (void)s; interrupted = 1; }
  int main(void) {
      signal(SIGINT, h);
      while (!interrupted) { }          /* optimizer may cache the read */
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static volatile sig_atomic_t interrupted = 0;
  static void h(int s) { (void)s; interrupted = 1; }
  int main(void) {
      signal(SIGINT, h);
      while (!interrupted) { }          /* reloaded on every iteration */
  }
  ```
- **VERIFICATION**: at `-O2`, inspect the asm — a non-volatile flag read is
  hoisted out of the loop; the `volatile sig_atomic_t` read is not. Assert every
  object referenced in the handler is `volatile sig_atomic_t`.
- **SOURCE**: iso-c11-n1570 (5.1.2.3p5, 7.14p5, 7.14.1.1p5); cert-c (SIG31-C);
  cppreference-c-behavior (signal page).

## 4. signal() is one-shot (reset to SIG_DFL); prefer sigaction() on POSIX

- **RULE**: With ISO `signal()`, before entering a handler the implementation
  either executes the equivalent of `signal(sig, SIG_DFL)` or blocks the
  implementation-defined signal set (C11 7.14.1.1p4). On most Unix systems the
  handler is therefore reset to default — a second occurrence of the signal
  terminates the process. POSIX `sigaction()` keeps the handler installed and
  lets the caller control `SA_RESTART`, `SA_NODEFER`, `SA_RESETHAND`, and the
  handler mask.
- **WHY AI GETS IT WRONG**: assumes the handler persists for the whole program,
  so "it works once" is taken as proof; does not know why the second Ctrl+C kills
  the process; treats `signal()` and `sigaction()` as interchangeable.
- **CORRECT REASONING**: the reset is required by the C standard on platforms
  that do not block the signal set, so a `signal()`-based handler is fragile by
  contract, and reinstalling the handler from inside itself opens a race. POSIX
  `sigaction` with a persistent handler closes the race; `SA_RESTART` also
  restarts interrupted syscalls (see `c-errno-and-syscall-returns` for the EINTR
  fallback). MinGW provides only `signal()` — `sigaction` is not declared in
  MinGW `<signal.h>` (VERIFIED 2026-08-14) and is a documented POSIX-only
  primary target.
- **EXAMPLE** (bad):
  ```c
  static void h(int s) { (void)s; /* ... */ }
  int main(void) {
      signal(SIGINT, h);            /* two Ctrl+C: second takes SIG_DFL -> exit */
      for (;;) pause();             /* POSIX */
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  struct sigaction sa;                     /* POSIX, primary target */
  sa.sa_handler = h;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sa, NULL);            /* handler stays installed */
  ```
  Portable ISO fallback (single-threaded): reinstall inside the handler with
  `signal(sig, h)` immediately, accepting the reset race on POSIX.
- **VERIFICATION**: send the signal twice. With `signal()` the second delivery
  takes default action (process terminates on SIGINT); with `sigaction` it does
  not. On MinGW, assert `sigaction` is undeclared (compile error) and the
  one-shot reset is observable.
- **SOURCE**: iso-c11-n1570 (7.14.1.1p4); cert-c (SIG34-C, SIG32-C);
  cppreference-c-behavior (signal page).

## 5. Never return from a SIGFPE/SIGILL/SIGSEGV handler (SIG35-C)

- **RULE**: If a handler for a computational exception — `SIGFPE`, `SIGILL`,
  `SIGSEGV`, or an implementation-defined equivalent — returns, the behavior is
  undefined (C11 7.14.1.1p5). The handler must terminate the process (or
  `siglongjmp` to a saved context on POSIX).
- **WHY AI GETS IT WRONG**: writes a "crash handler" that sets a flag and returns,
  expecting the program to resume; treats SIGSEGV like SIGINT.
- **CORRECT REASONING**: for these signals the interrupted operation cannot be
  resumed meaningfully — the faulting instruction re-executes and re-triggers the
  handler (infinite re-entrancy), or the interrupted state is inconsistent.
  Returning is UB even in theory (KNOWN, N1570 7.14.1.1p5; CERT SIG35-C). On the
  Windows CRT, `raise(SIGSEGV)` happens to invoke the handler and returning
  resumes the caller (VERIFIED 2026-08-14) — do not rely on it; real access
  violations never reach a signal handler on Windows at all (VERIFIED: exit
  0xC0000005 = -1073741819, unhandled).
- **EXAMPLE** (bad):
  ```c
  static void h(int s) { (void)s; crash_seen = 1; }  /* returning: UB */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static void h(int s) {
      (void)s;
      (void)_write(1, "fault\n", 6);
      _Exit(134);                     /* terminate; never return */
  }
  ```
- **VERIFICATION**: for SIGFPE/SIGILL/SIGSEGV handlers, assert the handler does
  not return on any path (exits or longjmps). On POSIX, deliver a real fault and
  observe termination; on Windows, real faults bypass the handler entirely.
- **SOURCE**: iso-c11-n1570 (7.14.1.1p5); cert-c (SIG35-C);
  cppreference-c-behavior (signal page).

## 6. Signal handling in multithreaded programs (SIG32-C, SIG34-C)

- **RULE**: For a process-directed signal, the POSIX thread that receives it is
  unspecified; the handler runs in that thread, concurrently with other threads.
  Calling `signal()` in a multithreaded program is undefined behavior (CERT
  SIG32-C). Use `sigaction()` (POSIX) from the main thread, or block the signal
  in all threads (`pthread_sigmask`) and consume it in one dedicated thread with
  `sigwait`/`signalfd` (POSIX).
- **WHY AI GETS IT WRONG**: assumes signals always land on "the main thread";
  believes a `pthread_mutex_t` guard in the handler protects the shared counter;
  installs handlers from worker threads.
- **CORRECT REASONING**: a handler is an async interrupt on an arbitrary thread.
  `pthread_mutex_lock` is not async-signal-safe — if the interrupted thread
  already holds the mutex, the handler deadlocks on its first lock. Only
  `volatile sig_atomic_t` (or lock-free C11 atomics) is race-safe between the
  handler and other threads (KNOWN, CERT SIG32-C; N1570 5.1.2.3). The robust
  POSIX design is a dedicated `sigwait` thread; MinGW has no `sigwait`/`sigaction`
  and POSIX behavior here is the documented primary target (INFERRED for the
  Windows thread-delivery model).
- **EXAMPLE** (bad): a worker thread installs the handler with `signal()` and the
  handler updates a `pthread_mutex_t`-guarded counter shared with the main thread.
- **COUNTEREXAMPLE** (good): all threads block SIGINT via `pthread_sigmask`;
  one thread loops on `sigwait(&set, &sig)` and runs the cleanup with normal libc.
- **VERIFICATION**: assert no lock/heap/stdio call inside any handler; on POSIX,
  run a signal storm against the `sigwait` design and assert no deadlock and no
  missed signal.
- **SOURCE**: cert-c (SIG32-C, SIG34-C); iso-c11-n1570 (5.1.2.3);
  cppreference-c-behavior (signal page).

## 7. Self-pipe trick: wake a blocked loop from the handler

- **RULE**: The handler writes one byte to a non-blocking pipe with `write()`
  (async-signal-safe). The main/event loop `select()`/`poll()`/`read()`s the pipe
  and does the real, non-async-safe work there. A plain flag does not wake a loop
  blocked in `select`/`read`.
- **WHY AI GETS IT WRONG**: sets the flag but the main loop is blocked in
  `select`/`read` so the shutdown never runs; or the handler itself writes to a
  `FILE*`/socket/stdio instead of a pipe.
- **CORRECT REASONING**: the self-pipe converts "a signal happened" into "fd is
  readable", which is exactly what `select`/`poll` can wait on. The handler calls
  only `write()`; all processing moves to the loop where full libc is safe. Use a
  pipe (POSIX `pipe`, MinGW `_pipe`), keep it non-blocking and accept `EAGAIN`
  (KNOWN, documented pattern; CERT SIG30-C references the write-only discipline;
  POSIX primary target).
- **EXAMPLE** (bad): handler sets `volatile sig_atomic_t stop` but the main loop
  is `select()`-blocked — it never wakes; shutdown code is instead forced into the
  handler, which then calls `fclose`/`free`.
- **COUNTEREXAMPLE** (good):
  ```c
  /* handler: */ (void)_write(sp[1], "S", 1);
  /* loop: */   while (select(sp[0]+1, &r, NULL, NULL, NULL) > 0)
                    if (read(sp[0], &c, 1) == 1) run_shutdown();
  ```
- **VERIFICATION**: on POSIX, `kill -INT` the process while it is blocked in
  `select` on the pipe; assert the loop wakes exactly once and consumes one byte.
- **SOURCE**: cert-c (SIG30-C); cppreference-c-behavior (signal page); POSIX
  Signal Concepts (write in the safe list).

## 8. Defer all real work to the main loop

- **RULE**: A handler should set a flag or write to the self-pipe and return (for
  non-computational signals). All non-async-signal-safe work — logging,
  allocations, file I/O, object cleanup, shutdown — belongs in the main/event
  loop, where normal library rules apply.
- **WHY AI GETS IT WRONG**: writes the whole shutdown sequence (`fclose`, `free`,
  logger calls) inside the handler because "it is only a few calls" — each one is
  a potential deadlock or state corruption.
- **CORRECT REASONING**: the handler executes in interrupt context relative to
  arbitrary interrupted code; the main loop is the only place where the full
  standard library is guaranteed safe. The handler's only jobs are (a) record that
  a signal arrived (`volatile sig_atomic_t`) and (b) if the loop is blocked, wake
  it (self-pipe). Everything else is read from the loop (KNOWN, CERT SIG30-C/
  SIG31-C; C11 7.14.1.1).
- **EXAMPLE** (bad): handler calls `free(p); fclose(f); log_it("bye");`.
- **COUNTEREXAMPLE** (good):
  ```c
  static volatile sig_atomic_t g_stop = 0;
  int main(void) {
      signal(SIGINT, h);
      while (!g_stop) {
          /* loop: poll, run work, check flag each iteration */
      }
      run_cleanup();          /* fclose/free/log are safe here */
  }
  ```
- **VERIFICATION**: code-review checklist — the handler body contains only
  async-signal-safe calls and all cleanup calls sit after the loop, never inside
  the handler.
- **SOURCE**: cert-c (SIG30-C, SIG31-C); iso-c11-n1570 (7.14.1.1);
  cppreference-c-behavior (signal page).

## Windows-CRT notes (VERIFIED 2026-08-14, MinGW gcc 16.1; POSIX is primary target)

- MinGW `<signal.h>` declares only ISO `signal()`/`raise()`; `sigaction` is not
  available (compile error) — POSIX `sigaction` is documented as the primary
  target on Linux/BSD.
- `signal(SIGINT/SIGTERM/SIGSEGV, h)` + `raise(sig)` invokes the handler
  synchronously on the calling thread; returning resumes execution for SIGINT and
  SIGTERM (VERIFIED, exit 0).
- `raise(SIGSEGV)` runs the handler and returning resumes the caller on the
  Windows CRT (VERIFIED) — unlike POSIX, where returning from a
  computational-exception handler is documented UB (C11 7.14.1.1p5).
- A real null-pointer dereference does NOT reach a SIGSEGV handler on Windows;
  the process dies with unhandled 0xC0000005 (VERIFIED, exit -1073741819).
- `printf` and `malloc` inside a handler run without crashing under a synchronous
  `raise()` on Windows (VERIFIED) — the hazard (deadlock when the signal
  interrupts the same internal lock) is latent on this platform and deterministic
  on POSIX with async delivery.
- Windows console Ctrl+C (SIGINT) is the realistic async source; other POSIX
  signals are not delivered by `kill` semantics.
