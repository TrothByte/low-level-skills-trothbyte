# errno & Syscall Return Rules

Primary target: POSIX semantics. Windows CRT (`_read`/`_write` from `<io.h>`) shares the call
shape; Windows-CRT differences are marked inline. Confidence labels: KNOWN / INFERRED / UNVERIFIED.
Rule format: RULE -> WHY AI GETS IT WRONG -> CORRECT REASONING -> EXAMPLE -> COUNTEREXAMPLE ->
VERIFICATION -> SOURCE.

## 1. errno is only meaningful when the call reports failure

- **RULE**: Read `errno` only after a call whose return value indicates failure. After a
  successful call, `errno` is unspecified and may hold a value from any earlier call.
- **WHY AI GETS IT WRONG**: treats `errno` as a reliable global status flag ("if errno != 0 then
  something failed").
- **CORRECT REASONING**: library functions may write a positive value to `errno` whether or not an
  error occurred, and may leave it unchanged on success. Only the return value tells you whether to
  consult errno (KNOWN, cppreference errno page).
- **EXAMPLE** (bad):
  ```c
  long v = strtol(s, NULL, 10);
  if (errno != 0) return -1;   /* may be a stale value; parse may have succeeded */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  errno = 0;
  long v = strtol(s, NULL, 10);
  if (errno == ERANGE) return -1;   /* return value was fine; only range errors checked */
  ```
- **VERIFICATION**: review every `errno` read; it must sit on a failure branch of the return value.
- **SOURCE**: iso-c11-n1570 (7.5p3); cert-c (ERR30-C); cppreference-c-behavior (errno page).

## 2. Read errno immediately after the failing call

- **RULE**: After a failing call, capture or act on `errno` before any other library call, because
  any subsequent call may overwrite it.
- **WHY AI GETS IT WRONG**: sprinkles the errno check later, often after `printf` or logging, which
  are themselves allowed to set errno.
- **CORRECT REASONING**: errno is a shared modifiable lvalue (thread-local since C11); the value
  survives only until the next call that writes to it. Check, or save into a local, on the same
  statement region as the failing call (KNOWN, cppreference errno page; CERT ERR30-C/ERR33-C).
- **EXAMPLE** (bad):
  ```c
  FILE *f = fopen(name, "r");
  printf("open attempted\n");       /* printf may clobber errno */
  if (f == NULL) fprintf(stderr, "%d", errno);  /* may be wrong error */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  FILE *f = fopen(name, "r");
  if (f == NULL) { int e = errno; /* save immediately */ report(e); }
  ```
- **VERIFICATION**: fault-inject a failure, then call `printf` between the call and the errno read
  and observe the code still reports the original error.
- **SOURCE**: cert-c (ERR30-C, ERR33-C); cppreference-c-behavior (errno page).

## 3. Set errno = 0 before the call when you need to detect errors

- **RULE**: If errno is the only way to learn a call failed, set `errno = 0` immediately before the
  call, then check it after the call on the failure path.
- **WHY AI GETS IT WRONG**: assumes calls "reset" errno, so a previous error is reported as the
  current call's failure.
- **CORRECT REASONING**: the value of `errno` is 0 at program startup, but is never set to 0 by any
  library function. A stale nonzero errno persists across calls (KNOWN, N1570 §7.5p3).
- **EXAMPLE** (bad):
  ```c
  long v = strtol(s, NULL, 10);
  if (errno == ERANGE) return -1;  /* stale ERANGE from an earlier call = false failure */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  errno = 0;
  long v = strtol(s, NULL, 10);
  if (errno == ERANGE) return -1;  /* now ERANGE can only mean this call overflowed */
  ```
- **VERIFICATION**: set errno to ERANGE, then parse a valid number with the good pattern and assert
  it succeeds (see examples/good/errno_reset_and_check.c).
- **SOURCE**: iso-c11-n1570 (7.5p3); cert-c (ERR30-C); cppreference-c-behavior (errno page).

## 4. errno is thread-local since C11

- **RULE**: `errno` expands to a thread-local modifiable lvalue of type int. Do not share "last
  error" assumptions between threads.
- **WHY AI GETS IT WRONG**: models errno as a process-global, so error handling is written as if a
  failure in thread A is visible in thread B.
- **CORRECT REASONING**: C11 requires errno to be a macro that expands to a thread-local object;
  each thread sees its own copy (KNOWN, N1570 §7.5p2; cppreference errno page).
- **EXAMPLE** (bad): thread A reads `errno` after thread B's failed call and reports B's error.
- **COUNTEREXAMPLE** (good): return error codes from worker functions instead of consulting errno
  across threads; if needed, capture errno into the thread's own result record.
- **VERIFICATION**: run two threads doing failing calls concurrently and assert each thread's errno
  read is its own.
- **SOURCE**: iso-c11-n1570 (7.5p2/p3); cppreference-c-behavior (errno page).

## 5. Check the return value first: -1 + errno convention

- **RULE**: POSIX syscalls and libc wrappers return `-1` on failure and set errno; success returns
  a non-negative value. The return value is the primary signal; errno is the detail.
- **WHY AI GETS IT WRONG**: checks errno before (or instead of) the return value, so "success with
  stale errno" and "failure with errno == 0" are both misclassified (A17/A19).
- **CORRECT REASONING**: the convention is `if (r == -1) { errno explains why }`. A function can
  fail without setting errno (e.g. syscalls that return -1 without errno are rare but documented),
  and success does not reset errno (KNOWN, CERT ERR30-C; observed via strace/perf trace).
- **EXAMPLE** (bad):
  ```c
  if (errno != 0) handle_error();   /* read BEFORE the return check, or instead of it */
  int r = _write(fd, buf, n);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int r = _write(fd, buf, n);
  if (r == -1) { if (errno == EINTR) continue; return -1; }
  ```
- **VERIFICATION**: on Linux, `strace -e trace=write` shows -1 returns; assert the code branches on
  the return value, not on errno.
- **SOURCE**: cert-c (ERR30-C); cppreference-c-behavior (errno page); perf-wiki (syscall tracing).

## 6. read()/recv() returning 0 means EOF, not an error

- **RULE**: A byte-stream read that returns 0 has reached end-of-file (or orderly shutdown).
  It is a normal state, not a failure, and errno is not consulted.
- **WHY AI GETS IT WRONG**: treats 0 as "nothing yet, try again" (busy loop / hang) or as an error
  (false failure), or ignores it so EOF is indistinguishable from data.
- **CORRECT REASONING**: for `fread`, a return of 0 items means EOF or error; the error case is
  distinguished via `ferror`. For POSIX `read`, `0 == EOF` is the contract (KNOWN, N1570 §7.21.8.1;
  CERT ERR33-C). Confusing `0` with `EAGAIN` is a classic hang/DoS (CVE-2024-32650).
- **EXAMPLE** (bad):
  ```c
  for (;;) {
      int n = _read(fd, buf, sizeof buf);
      if (n == 0) continue;        /* EOF forever: infinite loop */
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int n;
  while ((n = _read(fd, buf, sizeof buf)) > 0) process(buf, n);  /* n==0 breaks: EOF */
  if (n < 0) handle_error();
  ```
- **VERIFICATION**: read past the end of a file and assert the loop terminates exactly at EOF.
- **SOURCE**: iso-c11-n1570 (7.21.8.1); cert-c (ERR33-C); cppreference-c-behavior (errno page).

## 7. read() may return a short count — accumulate

- **RULE**: `read`/`recv`/`fread` may transfer fewer bytes than requested (short read), even without
  error. Callers that need a full buffer must loop.
- **WHY AI GETS IT WRONG**: writes `n = read(fd, buf, 4096); process(buf);` and assumes 4096 arrived,
  silently truncating records (A18).
- **CORRECT REASONING**: for pipes, sockets, and signals, a short count is guaranteed to be allowed;
  for regular files it is possible. The only robust pattern is loop-until-n-or-EOF (KNOWN, POSIX
  read; N1570 §7.21.8.1 for fread item counts).
- **EXAMPLE** (bad):
  ```c
  int n = _read(fd, buf, 4096);   /* may return 8 */
  /* caller then treats 8 bytes as the full 4096-byte record */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  size_t done = 0;
  while (done < want) {
      int n = _read(fd, buf + done, (unsigned)(want - done));
      if (n < 0) { if (errno == EINTR) continue; return -1; }
      if (n == 0) break;             /* EOF: fewer than want is fine */
      done += (size_t)n;
  }
  ```
- **VERIFICATION**: read a 20-byte file into an 8-byte buffer with the loop; assert all 20 bytes
  arrive across several `_read` calls (examples/good/read_full_eintr_loop.c).
- **SOURCE**: iso-c11-n1570 (7.21.8.1); cert-c (ERR33-C); cppreference-c-behavior (errno page).

## 8. write() may return a short count — keep writing

- **RULE**: `write`/`send`/`fwrite` may accept fewer bytes than requested. Callers that must emit
  everything must loop from the offset of the last accepted byte.
- **WHY AI GETS IT WRONG**: treats one `write` as all-or-nothing, dropping the tail silently, or
  re-writes from the start on error (duplication).
- **CORRECT REASONING**: a short write reports how many bytes were accepted; resume from
  `buf + done` with `count - done`. Retry only on `EINTR` (KNOWN, POSIX write; N1570 §7.21.8.2 for
  fwrite item counts).
- **EXAMPLE** (bad):
  ```c
  int w = _write(fd, buf, 1000);
  if (w < 1000) return;   /* silently accepted partial payload */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  size_t done = 0;
  while (done < total) {
      int w = _write(fd, buf + done, (unsigned)(total - done));
      if (w < 0) { if (errno == EINTR) continue; return -1; }
      done += (size_t)w;
  }
  ```
- **VERIFICATION**: write a payload with a mock that accepts one byte per call; assert the loop
  completes and the round-trip read matches (examples/good/write_full_eintr_loop.c).
- **SOURCE**: iso-c11-n1570 (7.21.8.2); cert-c (ERR33-C); cppreference-c-behavior (errno page).

## 9. EINTR: retry interrupted read/write/accept/connect

- **RULE**: On POSIX, if a signal interrupts a blocking `read`/`write`/`accept`/`connect` before any
  data is transferred, the call returns -1 with `errno == EINTR`; the operation did not happen and
  must be retried as-is.
- **WHY AI GETS IT WRONG**: treats EINTR like any fatal error, so a signal kills a transfer that was
  simply not yet started; or retries forever without an error budget.
- **CORRECT REASONING**: retry the identical call with the same arguments on `EINTR` (no data was
  consumed). Guard against an endless loop with a retry bound. On the Windows CRT, `_read`/`_write`
  rarely deliver EINTR (no POSIX signal model); the loop is written for portability and the POSIX
  behavior is the primary target (KNOWN, POSIX read/write; CERT ERR30-C; Windows-CRT difference
  marked INFERRED from CRT behavior).
- **EXAMPLE** (bad):
  ```c
  int n = _read(fd, buf, sizeof buf);
  if (n < 0 && errno == EINTR) return -1;  /* interrupted read abandoned */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  do {
      n = _read(fd, buf, sizeof buf);
  } while (n < 0 && errno == EINTR);   /* retry the interrupted call */
  if (n < 0) return -1;                /* real errors after the retry */
  ```
- **VERIFICATION**: on Linux, deliver SIGALRM during a blocking read and assert the retried read
  completes; observe the EINTR return with `perf trace`.
- **SOURCE**: cert-c (ERR30-C, ERR series); cppreference-c-behavior (errno page); perf-wiki
  (observing EINTR via syscall tracing).

## 10. open() returns -1 on failure — never test !fd

- **RULE**: descriptor-returning functions report failure with `-1` (POSIX `open`) or `NULL`
  (ISO C `fopen`). A successful `open` may return 0, so truthiness tests are wrong.
- **WHY AI GETS IT WRONG**: writes `if (!fd)`, forgetting that stdin is fd 0, so a valid descriptor
  is falsy and the check passes/fails on the wrong branch (A19).
- **CORRECT REASONING**: compare against the documented failure sentinel: `fd == -1` for POSIX
  descriptors, `f == NULL` for FILE*. Treat 0 as a normal descriptor (KNOWN, N1570 §7.21.5.3 fopen
  returns null pointer on failure; POSIX open).
- **EXAMPLE** (bad):
  ```c
  int fd = _open(path, _O_RDONLY);
  if (!fd) return -1;   /* fd == 0 is a valid descriptor: stdin */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int fd = _open(path, _O_RDONLY);
  if (fd == -1) return -1;   /* read errno immediately for the reason */
  ```
- **VERIFICATION**: close stdin, open a file, and assert the code accepts the resulting fd 0
  (examples/good/open_close_check.c).
- **SOURCE**: iso-c11-n1570 (7.21.5.3); cert-c (ERR30-C); cppreference-c-behavior (errno page).

## 11. close() may return -1 — check it on the write path

- **RULE**: `close` returns -1 on failure (e.g. `EBADF`, pending write errors on some filesystems)
  and sets errno. Check it when the data must be durable.
- **WHY AI GETS IT WRONG**: assumes close cannot fail, so write errors that only surface at close
  are silently lost.
- **CORRECT REASONING**: deferred errors (e.g. NFS) are reported at `close`/`fflush`/`fclose`.
  On the Windows CRT, `_close` returns -1 and sets errno on failure (KNOWN, N1570 §7.21.8.3 fclose
  returns EOF on error; POSIX close).
- **EXAMPLE** (bad):
  ```c
  _close(fd);   /* result ignored: a pending write error is lost */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (_close(fd) == -1) { /* errno still valid here */ return -1; }
  ```
- **VERIFICATION**: force a write error (full disk or invalid handle) and assert close failure is
  propagated.
- **SOURCE**: iso-c11-n1570 (7.21.8.3); cert-c (ERR33-C); cppreference-c-behavior (errno page).

## 12. Syscall return conventions: raw kernel vs libc vs Windows

- **RULE**: In the Linux kernel, a failing syscall returns a negative errno (`-EINTR`) directly to
  libc; libc sets `errno` and returns `-1` to the caller. Some raw syscalls also use pointer
  results/`-EFAULT`. Windows Win32 uses a different convention (`FALSE`/`INVALID_HANDLE_VALUE` +
  `GetLastError`).
- **WHY AI GETS IT WRONG**: mixes conventions — tests a libc return against a kernel-style negative
  errno, or checks `GetLastError` after a call that succeeded, or assumes `-1` is the only failure
  sentinel for every function (A19).
- **CORRECT REASONING**: read the API's own contract. libc wrappers: `-1` + errno. Kernel: negative
  errno in the return register. Win32: documented sentinel + `GetLastError`. Never blend them
  (KNOWN, kernel syscall ABI observed via strace/perf; CERT ERR30-C for the libc contract).
- **EXAMPLE** (bad):
  ```c
  int r = _write(fd, buf, n);     /* libc wrapper: -1 + errno */
  if (r == -ENOSPC) ...           /* wrong: mixing kernel and libc conventions */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int r = _write(fd, buf, n);
  if (r == -1) { if (errno == ENOSPC) ... }   /* one convention per layer */
  ```
- **VERIFICATION**: on Linux, compare the libc return to the raw `syscall(SYS_write, ...)` return
  for the same failing input; on Windows, verify the CRT sentinel with `GetLastError` never used for
  `_write`.
- **SOURCE**: cert-c (ERR30-C); cppreference-c-behavior (errno page); perf-wiki (syscall return
  observation).

## Windows-CRT notes (INFERRED, verify per runtime)

- `_read`/`_write` return `int`, `-1` on error with `errno` set, matching the POSIX shape; they
  operate on C runtime descriptors, not Win32 HANDLEs.
- `EINTR` is rarely returned by `_read`/`_write` on Windows (no POSIX signal delivery model); the
  retry loop is written for portability and documents the POSIX primary target.
- `open` is `_open`, `close` is `_close`, `read`/`write` are `_read`/`_write` in `<io.h>`;
  `_O_BINARY` avoids CRT newline translation that would corrupt byte counts.
