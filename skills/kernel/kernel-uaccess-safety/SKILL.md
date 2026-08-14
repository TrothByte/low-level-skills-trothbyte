---
name: kernel-uaccess-safety
description: Use when writing, reviewing, or fixing Linux kernel driver code that exchanges data with user space — read/write, ioctl, mmap, poll/fasync, copy_to_user/copy_from_user, get_user/put_user, access_ok, compat_ioctl. Teaches fault-safe copying, size validation, and the -EFAULT/-ENOTTY/SIGSEGV symptom classes.
---

# Kernel UAccess Safety

Rules for exchanging data between a Linux kernel driver and user space without
-EFAULT, crashes, or privilege-escalation holes. Load `references/uaccess.md`
before writing any `copy_to_user`, ioctl, or mmap path.

## When to use

- Implementing or reviewing `.read` / `.write` / `.ioctl` / `.mmap` /
  `.poll` / `.fasync` file operations.
- Copying data across the boundary (`copy_to_user`, `copy_from_user`,
  `get_user`, `put_user`, `strncpy_from_user`, `strnlen_user`).
- Handling `_IO` / `_IOW` / `_IOR` / `_IOWR` ioctl commands, including
  32-bit `compat_ioctl` on 64-bit kernels.
- Mapping device memory with `remap_pfn_range` / `pgprot_noncached`.
- Debugging `-EFAULT`, `-ENOTTY`, or SIGSEGV symptoms in drivers.
- Reviewing patches for CWE-787 / CWE-252 / CWE-190 / CWE-191.

## When not to use

- User-space-only code, or code with no `__user` pointers.
- Kernel-internal copies between kernel buffers (use `memcpy` / `memmove`).
- Other kernels: BSD/Darwin (`copyin`/`copyout`) and Windows
  (`ProbeForRead` / `MmCopyToMdl`) uaccess rules differ.
- Performance tuning of a correct driver; this skill is about correctness.

## What the agent often gets wrong

- Dereferencing a `__user` pointer directly "because it is just a pointer".
- Reading `copy_to_user` / `copy_from_user` returns as "bytes copied". They
  return bytes NOT copied; 0 means success.
- Trusting `_IOC_TYPE` / `_IOC_NR` / `_IOC_SIZE` from the ioctl `cmd` bits
  (fully user-controlled) without validating them.
- Believing `strnlen_user` returns a safe bound; it returns `count + 1` when
  unterminated, so the caller must reject `ret > count`.
- Mapping device memory without `pgprot_noncached`, then seeing lost or stale
  MMIO reads/writes.
- Assuming 32-bit ioctl layout equals 64-bit; a missing `compat_ioctl` shows
  up as -ENOTTY on 32-bit clients.

## How to reason correctly

1. Annotate every user-space pointer `__user` and never dereference it —
   route it through a uaccess helper.
2. Check each helper's contract before trusting its result:
   `copy_*_user` returns untransferred bytes (0 = success);
   `get_user` / `put_user` return 0 or -EFAULT;
   `strncpy_from_user` returns length-including-NUL or a negative error;
   `strnlen_user` returns 0, length-including-NUL, or `count + 1`.
3. Treat ioctl `cmd` as attacker-controlled: validate `_IOC_TYPE`, `_IOC_NR`,
   and `_IOC_SIZE` before using them as a copy length.
4. Validate sizes with overflow-safe arithmetic (`size + len > limit`),
   never `limit - len` with attacker-controlled `len` (unsigned underflow).
5. For device/MMIO memory, override caching with `pgprot_noncached` (or
   `pgprot_writecombine`) before `remap_pfn_range`.
6. Provide `compat_ioctl` for 32-bit callers on 64-bit kernels, using compat
   structs where layout differs.

## What to verify

- No direct `__user` deref anywhere (sparse-clean).
- Every `copy_from_user` return checked before the buffer is used.
- ioctl validates `_IOC_TYPE`, `_IOC_NR`, `_IOC_SIZE`.
- `strnlen_user` results bounded (`ret > count` rejected).
- Device mmap uses `pgprot_noncached` / write-combine.
- `poll` calls `poll_wait`; `fasync` uses `fasync_helper` + `kill_fasync`.
- Compat paths exist with correct 32-bit layout.

## How to verify

Host-compilable logic checks (self-contained stubs, no kernel headers):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_uaccess.c -o /tmp/good_uaccess
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_uaccess.c -o /tmp/bad_uaccess
```

Target (kernel) checks — document these, do not claim to have run them:

```
make C=1 CHECK=sparse            # __user annotation violations
# full VM: kernel with KASAN + KUnit, booted under QEMU, uaccess tests run,
# driver fuzzed with syzkaller (CONFIG_KCOV)
```

## Where the knowledge comes from

- `ldd3` — Linux Device Drivers, 3rd ed: uaccess helpers, ioctl, mmap
- `linux-memory-barriers` — device-memory ordering and caching rules
- `kernel-coding-style` — `__user` annotations and driver style
- `cwe` — CWE-787/252/190/191/120/170 weakness classes
- `iso-c11-n1570` — C arithmetic and `sizeof` semantics

## Related skills

- `kernel-rcu-memory-barriers` — kernel memory model around driver paths
- `kernel-atomic-context` — what is legal in atomic/sleeping contexts
- `c-integer-promotion-and-conversion` — overflow in size arithmetic
- `c-string-and-buffer-safety` — string copy/termination rules
- `c-undefined-behavior` — OOB access and unchecked returns
- `safe-low-level-from-scratch` — positive writing path

## Evaluation

Historical CVEs: CVE-2021-22555 (x_tables compat OOB write), CVE-2022-0185
(legacy_parse_param unsigned underflow). Synthetic: direct `__user` deref,
ignored copy return, missing `access_ok`, ioctl size trust, cacheable MMIO
mmap. Adversarial: code that "works" in a 64-bit-only build but breaks 32-bit
compat. False-positive: correct copy-with-check, validated ioctl, noncached
mmap must NOT be flagged.
