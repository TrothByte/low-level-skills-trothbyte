# Linux VFS fops Rules

Source-backed rule set for `struct file_operations` drivers. Each entry:
RULE -> WHY AI GETS IT WRONG -> CORRECT REASONING -> EXAMPLE -> COUNTEREXAMPLE
-> VERIFICATION -> SOURCE. Confidence markers: KNOWN (documented kernel
contract), INFERRED (derived from the contract).

## 1. The fops dispatch table

- **RULE**: `struct file_operations` is a dispatch table: the VFS routes a
  syscall on a `struct file` to exactly one callback (`f_op->read`,
  `f_op->write`, `f_op->llseek`, `f_op->unlocked_ioctl`). A NULL callback
  makes the syscall fail with the conventional errno (-ESPIPE for llseek,
  -ENOTTY for ioctl, -EINVAL for read/write), and a wrongly-typed callback is
  a silent ABI corruption of the argument registers.
- **WHY AI GETS IT WRONG**: agents write `read` / `write` as free functions
  and "just add them to the struct", assuming the VFS adapts. A mismatch in
  the `ssize_t (*)(struct file *, char __user *, size_t, loff_t *)`
  signature compiles fine and crashes or corrupts at runtime.
- **CORRECT REASONING**: the VFS calls through the exact typedef. Write the
  callback to the typedef: return `ssize_t`, take `struct file *`,
  `char __user *`, `size_t`, `loff_t *`. Only callbacks present in the table
  are reachable; the struct must be initialized with the designated
  initializer form (`.read = ...`), never positionally.
- **EXAMPLE** (bad):
  ```c
  struct file_operations fops = { my_read, my_write, my_llseek };  /* order bug */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static struct file_operations fops = {
      .owner = THIS_MODULE,
      .llseek = my_llseek,
      .read = my_read,
      .write = my_write,
      .unlocked_ioctl = my_ioctl,
      .compat_ioctl = my_ioctl,   /* layout-identical commands */
  };
  ```
- **VERIFICATION**: host harness: dispatch wrappers call the exact callback
  and check the return against the documented convention; sparse `C=1`
  flags `__user` mismatches in the signatures.
- **SOURCE**: linux-vfs-docs, kernel-driver-api, ldd3

## 2. open / release pairing

- **RULE**: `.open` and `.release` are a matched pair. The driver must
  allocate and initialize `private_data` in `.open` (on success) and free it
  in `.release`; `.release` runs exactly once per `struct file`, at the last
  `fput`. An open that fails must clean up everything it allocated before
  returning the negative errno.
- **WHY AI GETS IT WRONG**: agents put allocation in `init_module` and free
  in `cleanup_module`, or allocate lazily inside `read`, or free in
  `release` while assuming a 1:1 open/close without refcounts.
- **CORRECT REASONING**: a file can be opened multiple times (each open gets
  its own `struct file` and, usually, its own `private_data`). `release`
  pairs with `open`, not with the module lifecycle. Failure paths inside
  `open` must undo partial state before returning -ENOMEM/-ENXIO.
- **EXAMPLE** (bad):
  ```c
  static int bad_open(struct inode *inode, struct file *file) {
      (void)inode;
      file->private_data = kmalloc_emu(sizeof(struct dev_ctx));   /* unchecked */
      return 0;
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static int good_open(struct inode *inode, struct file *file) {
      struct dev_ctx *ctx;
      (void)inode;
      ctx = kmalloc_emu(sizeof *ctx);
      if (ctx == NULL)
          return -ENOMEM;
      file->private_data = ctx;   /* born in open */
      return 0;
  }
  ```
- **VERIFICATION**: harness: two sequential opens produce two distinct
  `private_data`; close frees each exactly once (release counter == open
  counter).
- **SOURCE**: ldd3, kernel-driver-api, kernel-coding-style

## 3. read / write return contract

- **RULE**: `.read` / `.write` return the number of bytes transferred.
  `0` means EOF (read) or zero bytes written (write); a short positive count
  is legal when the device has less data than requested; a negative value is
  an errno. The VFS advances `f_pos` by the positive return. A return larger
  than the request is a driver bug that corrupts position accounting.
- **WHY AI GETS IT WRONG**: agents mirror `copy_*_user` semantics ("0 means
  success"), or return 0 on an empty buffer, or return the bytes NOT
  transferred — the VFS then reports `count - ret` bytes written and the
  user space sees phantom progress.
- **CORRECT REASONING**: unlike uaccess helpers, fops returns are data
  counts, not status codes. `read` returning fewer bytes than requested is
  "short read", which read(2) delivers as-is; `write` returning 0 for a
  non-zero request is a silent lost write (POSIX treats it as no progress).
- **EXAMPLE** (bad):
  ```c
  static ssize_t bad_write(struct file *f, const char __user *b, size_t c, loff_t *p) {
      (void)f; (void)p;
      copy_from_user_emu(kbuf, b, c);
      return (ssize_t)(c - written);   /* bytes NOT transferred */
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static ssize_t good_write(struct file *f, const char __user *b, size_t c, loff_t *p) {
      (void)f; (void)p;
      if (c > sizeof kbuf)
          return -EINVAL;
      copy_from_user_emu(kbuf, b, c);
      return (ssize_t)c;               /* bytes transferred */
  }
  ```
- **VERIFICATION**: harness: the VFS wrapper advances `*pos` by the positive
  return; the bad write advances it by `count - written` and is caught by
  comparing against the device capacity.
- **SOURCE**: linux-vfs-docs, kernel-source, cwe (CWE-252)

## 4. llseek validation

- **RULE**: `.llseek(file, offset, whence)` must validate `whence` (only
  SEEK_SET / SEEK_CUR / SEEK_END), reject negative resulting positions with
  -EINVAL, handle overflow with the same arithmetic checks as the VFS, and
  update `file->f_pos` only on success, returning the new position.
- **WHY AI GETS IT WRONG**: agents copy `lseek()` habits: accept anything,
  skip SEEK_END bounds, or return `offset` verbatim for SEEK_SET without
  checking negative offsets.
- **CORRECT REASONING**: SEEK_CUR is `f_pos + offset`, SEEK_END is `size +
  offset`, SEEK_SET is `offset`. Each must be checked against `< 0` and
  against `MAX_LFS_FILESIZE` before storing; unknown whence returns -EINVAL
  (-ESPIPE only if the file is genuinely not seekable). Overflow must use
  guarded addition, not unchecked wrap.
- **EXAMPLE** (bad):
  ```c
  static loff_t bad_llseek(struct file *f, loff_t off, int whence) {
      switch (whence) {
      case SEEK_SET: f->f_pos = off; break;      /* negative off accepted */
      case SEEK_END: f->f_pos = size + off;      /* wrap unchecked */
      default: return -EINVAL;
      }
      return f->f_pos;
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static loff_t good_llseek(struct file *f, loff_t off, int whence) {
      loff_t npos;
      switch (whence) {
      case SEEK_SET: npos = off; break;
      case SEEK_CUR: npos = f->f_pos + off; break;
      case SEEK_END: npos = (loff_t)size + off; break;
      default: return -EINVAL;
      }
      if (npos < 0)
          return -EINVAL;
      f->f_pos = npos;
      return npos;
  }
  ```
- **VERIFICATION**: harness: negative SEEK_SET, negative SEEK_CUR result,
  and unknown whence all return -EINVAL; valid positions round-trip through
  `f_pos`.
- **SOURCE**: linux-vfs-docs, kernel-source, kernel-coding-style

## 5. .owner and the module refcount

- **RULE**: `.owner = THIS_MODULE` must be set so the VFS can pin the module
  with `try_module_get` while the file is open; `module_put` in release (or
  the VFS doing it for the file) balances it. Without `.owner`, rmmod
  succeeds while open files still call the driver's code — a code
  use-after-free.
- **WHY AI GETS IT WRONG**: agents omit `.owner` entirely ("it compiles
  without it") or add `try_module_get`/`module_put` in the wrong methods,
  producing unbalanced refcounts.
- **CORRECT REASONING**: the module refcount is a lifetime guard on the
  *code*. Every open must take one reference and every last fput must drop
  it; the refcount must return to zero only when no file references the
  module. `try_module_get` fails (returns 0) when the module is being
  unloaded, and `open` must then fail.
- **EXAMPLE** (bad):
  ```c
  static struct file_operations fops = {
      .read = my_read,      /* .owner missing */
  };
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static struct file_operations fops = {
      .owner = THIS_MODULE,   /* pinned while any file is open */
      .read = my_read,
  };
  ```
- **VERIFICATION**: harness: with `.owner` set, `module_refcnt` is 1 after
  open and 0 after the last fput; `unload_module_emu()` refuses while a file
  is open. With `.owner` NULL, `module_refcnt` stays 0 and the unload
  succeeds — the bug is reproduced.
- **SOURCE**: kernel-driver-api, ldd3, kernel-source

## 6. private_data lifetime

- **RULE**: `private_data` is owned by the driver: created in `.open`,
  accessed by every method, and freed exactly once in `.release` after the
  last fput. Any pointer to it stored in a module global, a static, or
  another file's `private_data` is a use-after-free candidate.
- **WHY AI GETS IT WRONG**: agents cache `private_data` in a module-level
  variable (to avoid passing it around) and reuse it after close, or free it
  in an ioctl/cleanup path instead of release.
- **CORRECT REASONING**: a `struct file` may be duplicated (dup/fork) and
  closed many times; release is the single guaranteed last touch of
  `private_data`. The correct handle for shared driver state is a reference-
  counted object (`kref`), not a raw pointer in a global. If you keep a raw
  alias, it dies when the first file closes.
- **EXAMPLE** (bad):
  ```c
  static struct dev_ctx *last_ctx;               /* module-level cache */
  static ssize_t bad_read(struct file *f, char __user *b, size_t c, loff_t *p) {
      (void)f; (void)p;
      return read_from(last_ctx, b, c);          /* stale after close */
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static ssize_t good_read(struct file *f, char __user *b, size_t c, loff_t *p) {
      struct dev_ctx *ctx = f->private_data;     /* owned by this file */
      return read_from(ctx, b, c);
  }
  ```
- **VERIFICATION**: harness: after close, the freed region is poisoned
  (0xAA); the bad read returns poison bytes and the harness prints "BUG
  reproduced: use-after-free of private_data".
- **SOURCE**: ldd3, cwe (CWE-416, CWE-476)

## 7. struct file refcount and the last fput

- **RULE**: a `struct file` carries a refcount (`f_count`); `fget`/`dup`
  take references, `fput` drops them, and `.release` is invoked only when
  the count reaches zero — the last fput. The driver must never free
  `private_data` or other per-file state before the last fput, and must not
  access it after.
- **WHY AI GETS IT WRONG**: agents model close() as synchronous 1:1 with
  open(), ignoring dup/fork/dup2 which share the file struct, and therefore
  free on the first close instead of the last.
- **CORRECT REASONING**: the file object lives until the last reference is
  gone; there is no way for the driver to know "this is the last" except the
  release callback firing. Everything tied to the file (private_data, state,
  module ref) must be torn down there, once.
- **EXAMPLE** (bad):
  ```c
  /* close of a duplicated fd runs release a second time */
  static int bad_release(struct inode *i, struct file *f) {
      (void)i;
      kfree_emu(f->private_data);        /* may already be NULL */
      return 0;
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static int good_release(struct inode *i, struct file *f) {
      struct dev_ctx *ctx = f->private_data;
      (void)i;
      if (ctx != NULL) {
          kfree_emu(ctx);
          f->private_data = NULL;        /* once, at the last fput */
      }
      return 0;
  }
  ```
- **VERIFICATION**: harness: `get_file_emu` raises `f_count` to 2; the
  first fput does NOT run release, the second does (release counter == 1).
- **SOURCE**: linux-vfs-docs, kernel-source, cwe (CWE-416)

## 8. f_pos ownership

- **RULE**: for read/write the VFS advances `*pos` by the positive return of
  the callback; the driver must treat `f_pos` as VFS-owned and must not
  double-update it or substitute a private position counter. `.llseek` is the
  only method that legitimately writes `f_pos`.
- **WHY AI GETS IT WRONG**: agents advance `*pos` inside read/write AND the
  VFS advances it again (position doubles), or they ignore `*pos` entirely
  and always read from offset 0.
- **CORRECT REASONING**: the VFS passes `&file->f_pos` as `pos` and performs
  `pos += ret` after a positive callback return. The driver reads `*pos` to
  find where to transfer and returns the byte count; it must not mutate
  `*pos` itself.
- **EXAMPLE** (bad):
  ```c
  static ssize_t bad_read(struct file *f, char __user *b, size_t c, loff_t *p) {
      (void)f; (void)b; (void)c;
      *p += read_from(kbuf, c);    /* driver and VFS both advance pos */
      return c;
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static ssize_t good_read(struct file *f, char __user *b, size_t c, loff_t *p) {
      size_t n = read_from(kbuf, *p, c);   /* VFS advances *p by the return */
      copy_to_user_emu(b, kbuf, n);
      return (ssize_t)n;
  }
  ```
- **VERIFICATION**: harness: `vfs_read_emu` adds `ret` to `*pos`; a double-
  updating driver yields `pos` larger than the data actually moved.
- **SOURCE**: kernel-source, linux-vfs-docs

## 9. unlocked_ioctl dispatch

- **RULE**: `.unlocked_ioctl` is the modern ioctl handler (no BKL). It must
  validate the command space — magic, number, size — before acting, return
  -ENOTTY for unknown commands, and return 0 on success with results copied
  out through `arg`. The VFS dispatches ioctl(2) to it only.
- **WHY AI GETS IT WRONG**: agents treat `cmd` as a private enum, trust
  `_IOC_SIZE(cmd)` (attacker-controlled) as a copy length, or return the
  count of something instead of 0/-errno.
- **CORRECT REASONING**: both `cmd` and `arg` are user-controlled. The
  dispatch must be a pure switch on a validated command: unknown -> -ENOTTY,
  bad size -> -EINVAL, bad pointer -> -EFAULT, success -> 0. No "return
  bytes handled" style returns.
- **EXAMPLE** (bad):
  ```c
  static long bad_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
      (void)f;
      return copy_to_user_emu((void __user *)arg, &data, _IOC_SIZE(cmd));
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static long good_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
      (void)f;
      if (_IOC_TYPE(cmd) != MY_MAGIC || _IOC_NR(cmd) >= MY_MAXNR)
          return -ENOTTY;
      switch (cmd) {
      case MY_IOC_RESET: reset(); return 0;
      case MY_IOC_GET:   return put_into((void __user *)arg);
      default: return -EINVAL;
      }
  }
  ```
- **VERIFICATION**: harness: valid command returns 0 and updates the
  emulated user region; unknown command returns -ENOTTY.
- **SOURCE**: linux-vfs-docs, ldd3, cwe (CWE-252)

## 10. compat_ioctl

- **RULE**: for a 32-bit caller on a 64-bit kernel the VFS dispatches ioctl
  to `fops->compat_ioctl`; if it is NULL the call fails with -ENOTTY.
  Structures containing `long` or pointers differ in size, so the compat
  handler must use 32-bit layouts and compat copy sizes.
- **WHY AI GETS IT WRONG**: "works on my x86_64 machine" — 32-bit builds are
  ignored, native struct sizes are reused for compat buffers, or the field
  is simply left NULL.
- **CORRECT REASONING**: `compat_ioctl` must cover the same command space
  with compat-sized structures (`compat_ulong_t`, `compat_ptr` for pointer
  args). If the command's struct layout is identical in both modes (only
  32-bit ints), pointing `compat_ioctl` at the native handler is correct;
  otherwise translate fields and validate compat sizes.
- **EXAMPLE** (bad):
  ```c
  static struct file_operations fops = {
      .unlocked_ioctl = my_ioctl,      /* no .compat_ioctl */
  };
  ```
  32-bit clients then get -ENOTTY or the handler parses a 64-bit layout from
  a 32-bit buffer.
- **COUNTEREXAMPLE** (good):
  ```c
  static struct file_operations fops = {
      .unlocked_ioctl = my_ioctl,
      .compat_ioctl   = my_ioctl_compat,  /* compat sizes validated */
  };
  ```
- **VERIFICATION**: harness: `compat_ioctl_emu` returns -ENOTTY when the
  field is NULL and dispatches when it is set; target: 32-bit client against
  the driver in a 64-bit VM.
- **SOURCE**: linux-vfs-docs, cwe (CWE-843), kernel-coding-style

## 11. Module unload vs open files

- **RULE**: if the module is unloaded while a file is still open, the file's
  method pointers point to freed code; the next syscall (or the release
  callback itself) is a use-after-free. Correct `.owner` + refcounting makes
  `rmmod` fail with EBUSY while any file is open. A release that runs after
  unload means the refcount was wrong.
- **WHY AI GETS IT WRONG**: agents verify rmmod "succeeds" (module removed
  instantly) without checking that no file is open, and miss that the
  success itself is the bug.
- **CORRECT REASONING**: rmmod success with open files is the failure
  symptom. The module refcount, held from open to last fput, is the guard;
  `try_module_get` returning 0 during unload must make open fail. If release
  is ever observed after unload, the refcount path is broken.
- **EXAMPLE** (bad): an agent celebrating `rmmod` that returns 0 while a
  file descriptor is open.
- **COUNTEREXAMPLE** (good): `rmmod` returns -EBUSY with open files; the
  module refcount returns to 0 only after the last fput.
- **VERIFICATION**: harness: with `.owner` NULL, `unload_module_emu()`
  succeeds while f_count == 1 and prints "BUG reproduced: module unloaded
  while a file is open"; with `.owner` set it refuses.
- **SOURCE**: kernel-driver-api, ldd3, cwe (CWE-416)

## 12. Error returns: positive vs negative errno

- **RULE**: fops callbacks return a byte count or 0 on success and a
  *negative* errno on failure (-EINVAL, -EFAULT, -EBADF, -ENOTTY...). A
  positive value is never an error; a negative value is never a byte count.
  Returning 0 on a failure path turns an error into silent EOF/zero-write.
- **WHY AI GETS IT WRONG**: agents return `0` "as success" on error paths
  (copy_from_user habit) or return `EFAULT` without the minus sign, which
  the VFS interprets as a huge positive byte count.
- **CORRECT REASONING**: the sign is the contract. Compute the value, then
  sign it: `return -EINVAL;` not `return EINVAL;`. A failed read must not
  look like EOF, and a failed write must not look like a zero-byte write.
- **EXAMPLE** (bad):
  ```c
  if (!access_ok_emu(arg, sizeof *arg))
      return EFAULT;          /* positive: VFS treats it as bytes */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (!access_ok_emu(arg, sizeof *arg))
      return -EFAULT;         /* negative errno, clearly a failure */
  ```
- **VERIFICATION**: harness: the positive return propagates as a byte count
  and advances f_pos; the negative return is passed to user space as an
  error with f_pos untouched.
- **SOURCE**: kernel-coding-style, kernel-source, cwe (CWE-252)

## Quick detection table

| Pattern | Class | Check |
|---|---|---|
| `read`/`write` return 0 for "did nothing" | CWE-252 | return bytes transferred |
| `write` returns `count - done` | CWE-252 | compare vs device capacity |
| `.owner` missing | CWE-416 | open/release refcount balance |
| `private_data` freed before last fput | CWE-416/476 | release-only teardown |
| stale module global aliasing `private_data` | CWE-416 | no file-scope ctx pointers |
| llseek accepts negative position | CWE-190/191 | `npos < 0` -> -EINVAL |
| driver mutates `*pos` in read/write | logic | VFS owns position |
| `_IOC_SIZE(cmd)` trusted in ioctl | CWE-787 | validate type/nr/size |
| no `compat_ioctl` on 64-bit | CWE-843 | add compat path |
| positive errno returned | CWE-252 | return `-EINVAL` not `EINVAL` |
| rmmod succeeds with open files | CWE-416 | unload must return -EBUSY |
