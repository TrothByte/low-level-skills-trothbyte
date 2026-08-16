# TOCTOU in the Kernel — Reference

Sources: `cwe` (CWE-367/362), `kernel-source` (fs/open.c, fs/namei.c,
fs/fs_struct.c), `kernel-docs-fs`, `linux-namespaces`. The Python race model
below was executed on this host.

## 1. The check-then-use window

- **RULE**: a TOCTOU bug exists whenever a validation and the operation it
  protects are separated in time and the validated resource is mutable in
  between (another thread, an unmount, user memory being freed).
- **WHY AI GETS IT WRONG**: "the check passed a moment ago, so the resource
  is still in that state" — the window is the entire time between the two
  operations (B2).
- **CORRECT REASONING**: name the two points T1 (check) and T2 (use). If any
  mutation can occur between T1 and T2, the check result is stale at T2.
- **EXAMPLE**: `if (access(path, W_OK) == 0) fd = open(path, O_WRONLY)` — the
  attacker swaps the file between the two syscalls.
- **COUNTEREXAMPLE**: holding `mmap_lock` (or a refcount) across the whole
  check+use is a *closed* window, not a TOCTOU.
- **VERIFICATION**: `examples/bad/check_then_use.py` shows an inconsistent
  final state (run on host).
- **SOURCE**: `cwe` CWE-367.

## 2. Path-resolution races and their atomic fixes

- **RULE**: for filesystem paths, the atomic alternatives to check-then-use
  are: (a) `openat2` with `RESOLVE_*` flags (no symlinks, beneath, in_root),
  (b) `O_PATH` + `openat` with `AT_EMPTY_PATH`, (c) `renameat2` with
  `RENAME_EXCHANGE`/`RENAME_NOREPLACE`, (d) re-validating the opened fd's
  path (`/proc/self/fd`) before use.
- **WHY AI GETS IT WRONG**: proposing `access()`+`open()` "because it checks
  permissions first" or re-running `stat()` "to be safe" (A10).
- **CORRECT REASONING**: the kernel must resolve the path exactly once and
  atomically with respect to the permission check — openat2 resolves under
  the same lookup that checks the `RESOLVE_*` constraints.
- **EXAMPLE**: `openat2(fd, "dir/file", {.flags = RESOLVE_BENEATH |
  RESOLVE_NO_MAGICLINKS}, ...)`.
- **COUNTEREXAMPLE**: `access(path)` + `open(path)` without flags — the two
  syscalls race.
- **VERIFICATION**: `examples/good/openat2_demo.c` (compiles on Linux host,
  documented; not run here).
- **SOURCE**: `kernel-source` (fs/open.c `do_openat2`), `cwe` CWE-367.

## 3. User-pointer validation vs copy semantics

- **RULE**: `access_ok`/`strnlen_user` verify a range at check time; the
  memory can be unmapped before `copy_from_user` — so the copy itself must
  handle faults, and the *copied* data, not the live pointer, is what you
  validate and use.
- **WHY AI GETS IT WRONG**: "I checked the size, now I can dereference" — the
  check does not pin the memory.
- **CORRECT REASONING**: copy first (`copy_from_user` returns 0 on success),
  then validate the copy in a kernel buffer; never keep a pointer to user
  memory for later use.
- **EXAMPLE**: `copy_from_user(&kbuf, ptr, len)` then validate `kbuf`.
- **COUNTEREXAMPLE**: `if (access_ok(ptr, len)) memcpy(kbuf, ptr, len)` —
  `memcpy` faults if unmapped in between.
- **VERIFICATION**: host stub (user-side model) in `examples/good` — the
  copy-then-validate ordering is enforced by structure.
- **SOURCE**: `kernel-uaccess-safety` skill; `cwe` CWE-367.

## 4. Lookup-then-use of kernel objects (refcounts)

- **RULE**: `find_vma`/`pid_task`/`idr_find` return a pointer that can be
  freed or repurposed; hold the governing lock or take a reference before the
  use and release it after, in the same critical section.
- **WHY AI GETS IT WRONG**: treating lookup results as durable pointers
  (B7).
- **CORRECT REASONING**: the lifetime of the object is managed by
  refcounts/RCU; the lookup is atomic only if the use is also under the
  protecting mechanism (RCU read-side or lock).
- **EXAMPLE**: `rcu_read_lock(); task = find_task_by_pid_ns(...);
  get_task_struct(task); rcu_read_unlock(); /* use task */ put_task_struct`.
- **COUNTEREXAMPLE**: `task = find_task_by_pid(...); /* deref without
  refcount */` — use-after-free.
- **VERIFICATION**: lockdep + KASAN on the driver; documented target test.
- **SOURCE**: `kernel-docs-fs` (VFS lookup locking); `kernel-source` (kernel/
  pid.c, kernel/exit.c).
