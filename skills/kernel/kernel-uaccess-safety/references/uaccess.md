# Linux Kernel UAccess Rules

Source-backed rule set for user/kernel data exchange. Each entry:
RULE -> WHY AI GETS IT WRONG -> CORRECT REASONING -> EXAMPLE -> COUNTEREXAMPLE
-> VERIFICATION -> SOURCE. Confidence markers: KNOWN (documented contract),
INFERRED (derived), UNVERIFIED (never use in a stable skill).

## 1. Never dereference `__user` pointers directly

- **RULE**: A `__user` pointer arrives from user space and must never be
  read or written by plain dereference. Route every access through a uaccess
  helper (`copy_to_user`, `copy_from_user`, `get_user`, `put_user`,
  `strncpy_from_user`, `strnlen_user`) or through `access_ok` + a raw helper.
- **WHY AI GETS IT WRONG**: a pointer is a pointer; on the host and in many
  QEMU smoke tests the "user" address happens to be mapped, so tests pass.
- **CORRECT REASONING**: user memory can be unmapped or swapped at any instant
  because the process runs concurrently. Only the uaccess helpers use
  exception-table fixups, so a fault on a plain dereference oopses the kernel;
  if the page is mapped, the driver reads attacker-controlled data (info leak)
  or writes it (corruption). Sparse also rejects deref of `__user` noderef.
- **EXAMPLE** (bad):
  ```c
  int v = *((int __user *)arg);       /* no fixup, no range check */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int v;
  if (get_user(v, (int __user *)arg))  /* or get_user_emu in the harness */
      return -EFAULT;
  ```
- **VERIFICATION**: `make C=1 CHECK=sparse` reports "dereference of noderef
  expression"; host harness: direct deref never returns -EFAULT for a bad
  pointer, the helper does.
- **SOURCE**: ldd3 (ch 3, "Dealing with user space"); kernel-coding-style
  (`__user` annotation); cwe (CWE-787, CWE-252).

## 2. `copy_to_user` / `copy_from_user` return semantics

- **RULE**: Both return the number of bytes NOT copied; 0 means success.
  `copy_to_user` never returns a negative errno from the fault path. A
  nonzero `copy_from_user` result means the destination must not be used.
- **WHY AI GETS IT WRONG**: mirrors `memcpy` habits — agents read the return
  as "bytes copied" or as a boolean the wrong way round.
- **CORRECT REASONING**: On a fault the helper returns the untransferred tail
  (`n` when nothing was copied). On a partial page fault modern kernels
  zero-fill the uncopied tail of the destination (KNOWN kernel uaccess
  contract) — but the buffer is still invalid, so the caller must check the
  return before using it. The idiomatic check is `if (copy_from_user(...))
  return -EFAULT;`.
- **EXAMPLE** (bad):
  ```c
  copy_from_user_emu(&req, arg, sizeof req);   /* return ignored */
  use(&req);                                   /* stale/partial data */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (copy_from_user_emu(&req, arg, sizeof req))
      return -EFAULT;
  use(&req);
  ```
- **VERIFICATION**: harness: invalid pointer -> helper returns `n`; checked
  code returns -EFAULT and never touches the buffer contents.
- **SOURCE**: ldd3 (ch 3, copy helpers); cwe (CWE-252 unchecked return value).

## 3. `get_user` / `put_user`

- **RULE**: `get_user(x, p)` / `put_user(x, p)` move one simple scalar between
  kernel and user memory and return 0 on success or -EFAULT. They are for a
  single value of simple type — not structs, arrays, or multi-field copies.
- **WHY AI GETS IT WRONG**: agents copy whole structs with them, or ignore the
  return value as if the call could not fail.
- **CORRECT REASONING**: `get_user` / `put_user` perform the `access_ok` range
  check internally and copy with fault-handled asm. The raw `__get_user` /
  `__put_user` variants skip the check and require a prior `access_ok` —
  the pattern LDD3's `scull_ioctl` demonstrates.
- **EXAMPLE** (bad):
  ```c
  int x;
  __get_user_emu(&x, (int __user *)arg);   /* no access_ok, no return check */
  return x;
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int x;
  if (!access_ok_emu(arg, sizeof(int)))
      return -EFAULT;
  if (__get_user_emu(&x, (int __user *)arg))
      return -EFAULT;
  return x;
  ```
- **VERIFICATION**: harness: bad pointer -> -EFAULT; valid pointer -> value
  round-trips into the kernel variable.
- **SOURCE**: ldd3 (ch 3, scull_ioctl); kernel-coding-style; cwe (CWE-252).

## 4. `access_ok`

- **RULE**: `access_ok(type, addr, size)` verifies that `[addr, addr+size)`
  lies inside the user address space. The checked helpers (`copy_*_user`,
  `get_user`, `put_user`, `strncpy_from_user`, `strnlen_user`) do this
  internally; the raw `__copy_*_user` / `__get_user` / `__put_user` do not
  and require an explicit `access_ok` first.
- **WHY AI GETS IT WRONG**: either drops the check entirely ("it will fault
  anyway") or calls `access_ok` but then uses checked helpers (harmless but
  redundant, and LDD3-style code uses the raw pair).
- **CORRECT REASONING**: the check must cover the whole `addr + size` range
  (a range test, not just non-null) and must happen before any raw access.
  `access_ok` is not a substitute for size validation: if the size itself is
  attacker-computed, also validate it against the driver's real allocation.
- **EXAMPLE** (bad):
  ```c
  __copy_from_user_emu(dst, (void __user *)arg, n);   /* no access_ok */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (!access_ok_emu(arg, n))
      return -EFAULT;
  __copy_from_user_emu(dst, arg, n);
  ```
- **VERIFICATION**: harness: the raw path copies an out-of-range pointer; the
  checked path returns -EFAULT.
- **SOURCE**: ldd3 (ch 3); kernel-coding-style; cwe (CWE-787).

## 5. `strncpy_from_user` / `strnlen_user`

- **RULE**: `strncpy_from_user(dst, src, count)` returns the string length
  INCLUDING the trailing NUL on success, or a negative errno (KNOWN:
  -EFAULT on fault; modern kernels return -E2BIG when the string does not fit
  in `count`, INFERRED for older kernels which returned `count`).
  `strnlen_user(src, count)` returns the length including the NUL, 0 on
  fault, or `count + 1` when no NUL exists within `count`.
- **WHY AI GETS IT WRONG**: applies `strlen`/`strncpy` semantics; treats
  `strnlen_user`'s result as a safe maximum and forgets the `count + 1`
  sentinel, causing over-allocation / over-copy.
- **CORRECT REASONING**: the "user" variants tolerate faults and report them
  via the return value. `if (strnlen_user(...) > count) return -EINVAL;` is
  mandatory before using the result as a size. Never add 1 to a
  `strncpy_from_user` result — the NUL is already counted.
- **EXAMPLE** (bad):
  ```c
  long l = strnlen_user_emu(src, 64);
  char *buf = kzalloc(l);                 /* l == 65 when unterminated */
  copy_from_user_emu(buf, src, l);        /* copies one byte too many */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  long l = strnlen_user_emu(src, 64);
  if (l <= 0 || l > 64)
      return -EINVAL;
  char *buf = kzalloc(l);
  if (copy_from_user_emu(buf, src, l))
      return -EFAULT;
  ```
- **VERIFICATION**: harness: unterminated 64-byte string returns 65; the good
  path rejects it.
- **SOURCE**: ldd3 (ch 3, uaccess string helpers); cwe (CWE-120, CWE-170).

## 6. ioctl command encoding: `_IO` / `_IOW` / `_IOR`

- **RULE**: `cmd` encodes direction, type, number, and size via
  `_IOC_DIR` / `_IOC_TYPE` / `_IOC_NR` / `_IOC_SIZE`. The handler must
  validate `_IOC_TYPE(cmd) == magic` and `_IOC_NR(cmd) < max` (else
  -ENOTTY) and must not use `_IOC_SIZE(cmd)` as a copy length until it
  equals the driver's expected `sizeof(struct ...)` (else -EINVAL).
- **WHY AI GETS IT WRONG**: treats `cmd` as an internal enum and reads
  `_IOC_SIZE(cmd)` as an authority for how many bytes to copy.
- **CORRECT REASONING**: both `cmd` and `arg` are attacker-controlled. The
  size bits come from the attacker, so copying `_IOC_SIZE(cmd)` bytes into a
  fixed buffer without a size check is a buffer overflow. `_IO` carries no
  size, `_IOR` reads from the device, `_IOW` writes to it, `_IOWR` does both.
- **EXAMPLE** (bad):
  ```c
  copy_from_user_emu(&a, arg, _IOC_SIZE(cmd));   /* size from cmd bits */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (_IOC_TYPE(cmd) != MY_MAGIC || _IOC_NR(cmd) >= MY_MAXNR)
      return -ENOTTY;
  if (_IOC_SIZE(cmd) != sizeof(struct my_arg))
      return -EINVAL;
  if (copy_from_user_emu(&a, arg, sizeof(a)))
      return -EFAULT;
  ```
- **VERIFICATION**: harness: inflated-size cmd is rejected; wrong magic /
  number returns -ENOTTY.
- **SOURCE**: ldd3 (ch 6, ioctl method); iso-c11-n1570 (`sizeof` semantics);
  cwe (CWE-787, CWE-252).

## 7. `compat_ioctl` (32-bit callers on 64-bit kernels)

- **RULE**: for a 32-bit process on a 64-bit kernel the VFS dispatches ioctl
  to `fops->compat_ioctl`. If it is NULL the call fails with -ENOTTY. Structs
  containing `long` or pointers differ in size between 32- and 64-bit, so the
  compat path must use 32-bit layouts and compat copy sizes.
- **WHY AI GETS IT WRONG**: "works on my x86_64 machine" — 32-bit builds are
  ignored, or native struct sizes are reused for compat buffers.
- **CORRECT REASONING**: `compat_ioctl` must cover the same command space
  with compat-sized structures (32-bit `unsigned long`, `compat_ptr` for
  pointer args). Real OOB bugs live exactly in this translation layer —
  CVE-2021-22555 is a compat-mode x_tables out-of-bounds write (KNOWN, NVD).
  If the layout is identical (only 32-bit ints), sharing the native handler
  is fine; otherwise translate fields and validate compat sizes.
- **EXAMPLE** (bad):
  ```c
  .unlocked_ioctl = dev_ioctl,        /* no .compat_ioctl */
  ```
  32-bit clients then get -ENOTTY or the handler parses a 64-bit layout from
  a 32-bit buffer.
- **COUNTEREXAMPLE** (good):
  ```c
  .unlocked_ioctl = dev_ioctl,
  .compat_ioctl   = dev_ioctl_compat, /* validates compat sizes, converts */
  ```
- **VERIFICATION**: run a 32-bit client against the driver in a 64-bit VM
  and check the ioctl return; KASAN with a compat reproducer.
- **SOURCE**: cwe (CWE-787, CWE-843); ldd3 (ioctl method); kernel-coding-style.

## 8. mmap with `remap_pfn_range` / `pgprot_noncached`

- **RULE**: device/MMIO memory mapped into user space must use non-cacheable
  or write-combining protections. Apply `pgprot_noncached(vma->vm_page_prot)`
  (MMIO) or `pgprot_writecombine(...)` before `remap_pfn_range`, validate the
  pfn/region, and never map beyond the VMA size or the device resource.
- **WHY AI GETS IT WRONG**: copies a RAM mmap example (cacheable is correct
  for RAM) onto device memory; the driver "works" until the CPU cache line is
  never flushed and device writes vanish.
- **CORRECT REASONING**: `vma->vm_page_prot` defaults to cacheable RAM
  semantics. For I/O memory the CPU cache and posted writes must be disabled,
  or reads return stale values and writes never reach the device
  (linux-memory-barriers: device accesses must not be reordered/cached).
  Accessing a badly-mapped region faults the user process with SIGSEGV.
- **EXAMPLE** (bad):
  ```c
  remap_pfn_range_emu(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
  /* device I/O mapped cacheable */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  return remap_pfn_range_emu(vma, vma->vm_start, pfn, size,
                             pgprot_noncached(vma->vm_page_prot));
  ```
- **VERIFICATION**: harness asserts the protection passed to remap is
  noncached; target: QEMU VM, MMIO read/write round-trip through the driver.
- **SOURCE**: ldd3 (ch 15, memory mapping); linux-memory-barriers (device
  memory ordering); cwe (CWE-787).

## 9. `poll` / `fasync`

- **RULE**: `.poll` returns a `POLLIN`/`POLLOUT`/`POLLERR` mask and MUST call
  `poll_wait(file, &queue, pt)` for every wait queue the driver can wake.
  Async notification is registered via `fasync_helper(fd, file, on, &dev->fa)`
  (driven by the VFS `F_SETFL`/`O_ASYNC` path) and fired with
  `kill_fasync(&dev->fa, SIGIO, POLL_IN)`; both state must be cleaned up in
  `.release`.
- **WHY AI GETS IT WRONG**: returns a constant mask, omits `poll_wait`
  (busy-waits), or expects SIGIO without ever storing an fasync entry.
- **CORRECT REASONING**: `poll` is a synchronous query; the `poll_table`
  argument is where the sleeping process parks, so any queue the driver can
  wake must be passed to `poll_wait` or the waiter never wakes. fasync is
  just bookkeeping: the driver stores the helper's result and calls
  `kill_fasync` when data becomes available.
- **EXAMPLE** (bad):
  ```c
  unsigned int dev_poll(...) { return POLLIN; }   /* no poll_wait */
  int dev_fasync(...) { return 0; }               /* never stores fasync */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  poll_wait_emu(filp, &ds->rq, pt);
  return ds->data_ready ? POLLIN : 0;
  /* F_SETFL path: */
  if (fasync_helper_emu(fd, filp, on, &ds->fasync_on) < 0)
      return -EIO;
  /* on data: */
  kill_fasync_emu(&ds->fasync_on, SIGIO, POLL_IN);
  ```
- **VERIFICATION**: harness: poll_wait called with the driver queue; after a
  data push `kill_fasync` fires and the mask becomes POLLIN.
- **SOURCE**: ldd3 (ch 6, blocking I/O, poll, fasync); kernel-coding-style.

## 10. Size validation before copy

- **RULE**: every length that reaches a copy must be validated with
  overflow-safe arithmetic. Compare the attacker-side sum against the limit
  (`size + len + slack > limit` -> reject), never compute `limit - len` with
  attacker-controlled `len`, because unsigned arithmetic wraps modulo 2^N.
- **WHY AI GETS IT WRONG**: writes `if (len > PAGE_SIZE - 2 - size)` with
  unsigned `size` and believes it bounds the copy — it underflows when
  `size` is large. CVE-2022-0185 is exactly this class (KNOWN, NVD, CWE-191).
- **CORRECT REASONING**: unsigned subtraction wraps; put the addition on the
  attacker side: `if (size + len + 2 > PAGE_SIZE) return -EINVAL;`. Validate
  against the driver's actual allocation size, not just the user region.
- **EXAMPLE** (bad):
  ```c
  size_t size = atk_size;
  if (len > PAGE_SIZE - 2 - size)      /* underflows for big size */
      return -EINVAL;
  memcpy(dst + size, src, len);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (size + len + 2 > PAGE_SIZE)      /* no underflow possible */
      return -EINVAL;
  memcpy(dst + size, src, len);
  ```
- **VERIFICATION**: harness: craft a size near the limit and observe the
  underflow path accept an oversized copy while the addition form rejects it.
- **SOURCE**: iso-c11-n1570 (§6.2.5p9 unsigned wrap); cwe (CWE-190 overflow,
  CWE-191 underflow); ldd3.

## 11. Symptom classes: -EFAULT, -ENOTTY, SIGSEGV

- **RULE**: -EFAULT means a uaccess helper rejected an address/range or a
  raw copy faulted. -ENOTTY means the ioctl command was not recognized —
  including a missing `compat_ioctl` for a 32-bit caller. SIGSEGV in the user
  process usually means it dereferenced a mapping the driver created badly
  (region not mapped, wrong caching, pfn beyond the device).
- **WHY AI GETS IT WRONG**: blames userspace for the SIGSEGV, or greps for
  the wrong errno instead of walking the uaccess path.
- **CORRECT REASONING**: -EFAULT -> which pointer/range failed, and was it
  access_ok-checked with the right size? -ENOTTY -> wrong `_IOC_TYPE` /
  `_IOC_NR`, or a compat call with no compat handler? SIGSEGV -> check
  `/proc/PID/maps` and dmesg for the faulting address and compare with what
  the driver mapped.
- **EXAMPLE** (bad): debugging a 32-bit client's -ENOTTY for hours without
  checking that `fops->compat_ioctl` is NULL.
- **COUNTEREXAMPLE** (good): strace confirms ENOTTY from ioctl; add
  `.compat_ioctl`; the call then proceeds and the client runs.
- **VERIFICATION**: harness triggers each symptom and confirms the expected
  errno; target: QEMU + strace inside the VM.
- **SOURCE**: ldd3 (ioctl and errno conventions); kernel-coding-style;
  cwe (CWE-252).

## Quick detection table

| Pattern | Class | Check |
|---|---|---|
| `__user` deref | CWE-787 / leak | sparse `C=1` |
| ignored copy return | CWE-252 | review each `copy_*_user` |
| missing `access_ok` before raw helper | CWE-787 | review raw `__copy_*_user` |
| `_IOC_SIZE(cmd)` trusted | CWE-787 | validate type/nr/size |
| `strnlen_user` `count+1` missed | CWE-120/170 | reject `ret > count` |
| `limit - atk_size` underflow | CWE-191 | use addition form |
| MMIO mmap cacheable | ordering | `pgprot_noncached` |
| no `compat_ioctl` on 64-bit | CWE-787/843 | add compat path |
| -EFAULT symptom | bad address | access_ok range |
| -ENOTTY symptom | unknown cmd / no compat | `_IOC_TYPE`/`_IOC_NR` |
| SIGSEGV symptom | bad mmap mapping | `/proc/PID/maps` + dmesg |
