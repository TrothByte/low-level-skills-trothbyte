---
name: toctou-kernel
description: Use when writing or reviewing kernel/driver code that validates then uses a resource — file paths, user pointers, fd tables, pid/name lookups, mounts — where another thread can mutate between check and use. Teaches TOCTOU patterns, openat2/renameat2/AT_EMPTY_PATH idioms, refcount-and-lock fixes, and how to make check-then-use atomic.
---

# TOCTOU in the Kernel

## When to use

- Any kernel path that does `check(path)` then `use(path)` (open, stat,
  chmod, rename, mount).
- User-pointer validation (`access_ok`/size checks) followed by
  `copy_from_user`/`get_user`.
- Lookup-then-use of fd, pid, inode, dentry, vma, or idr objects.
- Reviewing for CWE-367 (time-of-check time-of-use).
- Writing filesystem or VFS-layer code that resolves paths more than once.
- Mount/namespace traversal where the mount can change between two
  resolutions.

## When not to use

- Userspace-only code with no shared mutable kernel state (locks suffice at
  userspace level; see `concurrency-deadlock` skills).
- Code that validates and uses the object while holding the same lock the
  mutation path uses (already atomic — just verify).
- Database transactions (different abstraction level).
- Pure memory-ordering problems without a check/use pair.

## What the agent often gets wrong

- "The kernel is privileged, so the check can't be raced" — kernel objects
  are mutable by other kernel threads, and user-space can mutate memory the
  kernel is about to copy (B2).
- Relying on `access()` then `open()` (or `stat()` then `open()`) — this is
  the textbook TOCTOU; the fix is `openat2` with `RESOLVE_*` flags or
  `O_PATH`+`AT_EMPTY_PATH`, not re-checking (A10).
- `access_ok`/`strnlen_user`/`copy_from_user` — believing the pointer is
  still valid after the check; the buffer can be unmapped by another thread
  between check and copy (the copy can fault).
- Lookup-then-use without holding a reference: `find_vma` then deref without
  `mmap_read_lock`; `pid_task` then use without `get_task_struct`.
- "I'll lock around the check" but the use happens after releasing the lock —
  the window survives.
- Confusing `rename`/`symlink` races: `renameat2(RENAME_EXCHANGE)` exists
  exactly because two-step renames race.
- Fixing TOCTOU by adding `sleep`/`yield` ("let the other thread finish") —
  the window is defined by check+use, not by timing (B7).

## How to reason correctly

1. Enumerate every check-then-use pair: for each, list what can change between
   them (another thread, an unmount, a fork/exec changing mm, user memory
   being freed).
2. Eliminate the window, in order of preference:
   a. Make check and use a single atomic operation: `openat2` with
      `RESOLVE_NO_SYMLINKS`/`RESOLVE_BENEATH` etc. for paths; `*at` family
      with `AT_EMPTY_PATH`; `renameat2` with exchange flags.
   b. Hold a reference/lock across both: `get_task_struct` before use,
      `mmap_read_lock` for vma use, refcount on the dentry/inode.
   c. Re-validate inside the use operation (e.g., the copy itself faults).
3. For user pointers: copy first, validate the copy, then use the copy —
   never validate the live pointer and dereference it later.
4. Never "double-check" with the same racy pattern; the fix must close the
   window, not narrow it.
5. Verify: model the race with a test (two threads mutating the path),
   lockdep for lock ordering, and targeted fuzzing (syzkaller) for
   kernel-space cases.

## What to verify

- Every check/use pair is atomic (one syscall, or lock/ref held across both).
- No `access()`+`open()` or `stat()`+`open()` idiom remains.
- User-pointer code copies-then-validates, not validates-then-copies.
- References are held across lookup-then-use (`get_*` before use, `put_*`
  after).
- No revalidation of the same mutable object under a different lock.
- Lock ordering is consistent (lockdep-clean).

## How to verify

Host-executable race model (Python, self-contained):

```
python3 examples/good/atomic_check_use.py   # lock covers check+use: consistent
python3 examples/bad/check_then_use.py      # two-step race: inconsistent state
```

Documented target verification (kernel host, not on this host):

```
# openat2-based demo on Linux:
gcc -O2 examples/good/openat2_demo.c -o /tmp/openat2  && /tmp/openat2
# race demonstration under load:
# strace -f -e trace=openat,rename ./check_then_use_demo
# kernel: syzkaller + lockdep for the driver under test
```

## Where the knowledge comes from

- `cwe` — CWE-367 (TOCTOU), CWE-362 (race condition)
- `kernel-source` — fs/open.c (openat2), fs/namei.c (path resolution),
  fs/fs_struct.c (rename locking)
- `kernel-docs-fs` — VFS path resolution and locking model
- `linux-namespaces` — mount/namespace resolution races
- `kernel-uaccess-safety` — user-pointer copy discipline (skill)

## Related skills

- `kernel-uaccess-safety` — copy-then-validate for user pointers (require)
- `concurrency-deadlock-and-lock-ordering` — locking that closes the window (recommend)
- `kernel-driver-char-device-lifecycle` — refcounts in driver object lifecycle (recommend)
- `c-errno-and-syscall-returns` — handling ENOENT/ESTALE/EBUSY from atomic ops (recommend)
- `meta-rationalizations` — resist "this race is unlikely" rationalization (recommend)

## Evaluation

Synthetic: identify the check/use pair and the mutation in a small snippet;
flag `access()`+`open()`; approve `openat2`; approve lock-held lookups.
Adversarial: code that passes `if (access(path))` then `open(path)` and "works
in testing" — must be flagged without a runtime failure; a lock released
between check and use. Historical: CVE-2021-... TOCTOU in kernel path
resolution (documented in CWE-367 examples); the `/tmp` symlink race class;
the Linux `stat`/`open` TOCTOU used in exploit chains. FP: `openat2` with the
correct `RESOLVE_*` flags and a `get_task_struct`-held lookup must NOT be
flagged.
