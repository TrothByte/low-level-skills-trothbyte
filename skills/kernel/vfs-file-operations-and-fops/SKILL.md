---
name: vfs-file-operations-and-fops
description: Use when writing, reviewing, or debugging Linux kernel file_operations drivers — open/release, read/write return semantics, llseek, unlocked_ioctl dispatch, .owner, private_data lifetime, and refcounting that prevents use-after-free on last fput. Teaches the VFS dispatch contract for each fops method.
---

# VFS File Operations & fops

The per-method contract the VFS imposes on `struct file_operations`
implementations. Each fops callback has exact return semantics; the driver
owns `private_data` lifetime, the `struct file` refcount, and the module
refcount. Load `references/README.md` before writing a new `.read` /
`.write` / `.llseek` / `.open` / `.release` / `.unlocked_ioctl` path.

## When to use

- Writing, reviewing, or debugging a driver that installs a `struct
  file_operations` on a device node, char device, proc/debugfs file, or
  other VFS object.
- Implementing `.open` / `.release`, `.read` / `.write`, `.llseek`,
  `.unlocked_ioctl` / `.compat_ioctl`.
- Reasoning about `private_data` lifetime, `struct file` refcounting
  (get_file / fput), or the module-unloaded-while-open class of bugs.
- Reviewing patches for CWE-416 / CWE-476 / CWE-252 regressions inside
  file-operation callbacks.

## When not to use

- Filesystems: their `file_operations` are installed by the VFS itself;
  this skill targets code that *supplies* the fops.
- Block (`blkdev`) or network (`netdev`) drivers with a probe/remove
  lifecycle instead of open/release.
- Userspace or emulated code with no real `struct file`.
- `kernel-driver-char-device-lifecycle` covers cdev/class/device
  registration; `kernel-uaccess-safety` covers the `__user` copy rules.

## What the agent often gets wrong

- `.read` / `.write` return the number of bytes transferred. Returning 0 for
  an "empty" or short read is EOF, not "did nothing"; returning 0 from a
  non-empty write is a silent zero-byte write.
- `.write` returns bytes NOT transferred (the unwritten tail) — the VFS then
  believes `count - ret` bytes were written and advances `f_pos` by that.
- Forgetting `.owner = THIS_MODULE`, so the module can be unloaded while an
  open file still calls its methods (release after unload, code UAF).
- Freeing `private_data` in `.release` while another path still holds a
  pointer to it; release runs only after the LAST fput, but stale aliases
  can outlive it.
- Reading inode size with `i_size_read` without the locking discipline the
  filesystem requires (missing or wrong lock).
- Returning 0 on failure instead of a negative errno (`-EINVAL`,
  `-EFAULT`, `-EBADF`) — positive is success, negative is error.
- Skipping SEEK_END / SEEK_CUR offset validation in `.llseek`, or accepting
  negative resulting positions.
- Ignoring `f_pos`: the VFS advances `*ppos` on a positive return; the
  driver must not double-update it or manage its own private position.

## How to reason correctly

1. A fops is a dispatch table: the VFS calls `fop->open`, then routes
   read/write/llseek/ioctl to the matching callback on the same `struct
   file`. Only `.owner` and the callback pointers matter.
2. `.open` and `.release` are a pair: `private_data` is allocated in open
   and freed in release. release runs exactly once, when the last reference
   to the `struct file` is fput.
3. `.read` / `.write` contract: return the bytes transferred; a short count
   is legal when the device has less; 0 means EOF / nothing written; only a
   negative errno is a failure. Never "bytes not transferred".
4. `*ppos` is owned by the VFS for regular reads/writes: the kernel advances
   it by the positive return. `.llseek` validates SEEK_SET/SEEK_CUR/SEEK_END
   (negative offset, overflow) and updates `f_pos` only on success.
5. `.owner = THIS_MODULE` + `try_module_get` in open / `module_put` in
   release keeps module code alive while any file exists.
6. `private_data` must outlive every callback that can touch it. Any global
   or module-level alias of it is a use-after-free waiting for the last fput.
7. Return convention: non-negative = byte count / success, negative = errno.
   Two different numberspaces; a byte count is never a valid errno.

## What to verify

- Every fops method signature matches the kernel typedef (`ssize_t`, `loff_t
  *`, `size_t`); mismatches are silent ABI bugs.
- `read`/`write` return `bytes` transferred, not `0` on short and not
  `count - done`.
- `.llseek` rejects negative positions and unknown whence.
- `.owner = THIS_MODULE` is set; open/release keep the module refcount
  balanced (assert refcount back to 0 after close).
- `private_data` is set in open, freed exactly once in release.
- `unlocked_ioctl` validates the command; `compat_ioctl` exists for 32-bit
  callers on 64-bit kernels.
- No fops pointer is used after the last fput (no module-level stale alias).

## How to verify

Host-compilable logic checks (self-contained stubs, no kernel headers):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_fops.c -o /tmp/good_fops
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_fops.c -o /tmp/bad_fops
```

(Windows/MinGW: point `-o` at an existing temp dir, e.g.
`-o "$env:TEMP\kilo\good_fops.exe"`; never put build artifacts in the repo.)

Target (kernel) checks — document these, do not claim to have run them:

```
make C=1 CHECK=sparse            # fops signature / __user annotation issues
# full VM: kernel with KASAN + KUnit under QEMU, open/read/write/llseek/
# ioctl close-loop on the driver; rmmod with the device open must refuse
# (EBUSY), not crash.
```

## Where the knowledge comes from

- `linux-vfs-docs`
- `kernel-docs-fs`
- `kernel-driver-api`
- `kernel-source`
- `ldd3`
- `kernel-coding-style`
- `cwe`
- `nvd-cve`

## Related skills

- `kernel-uaccess-safety` — require: the `__user` copy rules fops callbacks
  must follow.
- `kernel-driver-char-device-lifecycle` — recommend: cdev/class/device
  registration around the same fops.
- `kernel-atomic-context` — recommend: what callbacks may do in atomic vs
  sleeping contexts.
- `kernel-rcu-memory-barriers` — recommend: ordering rules when callbacks
  share state across CPUs.
- `toctou-kernel` — recommend: check-then-use races in open/ioctl paths.
- `c-integer-promotion-and-conversion` — recommend: signedness in size and
  offset arithmetic.

## Evaluation

Historical CVEs: CVE-2022-0185 (fs_context `legacy_parse_param` unsigned
underflow in the mount-API path, CWE-191) and CVE-2023-0386 (overlayfs FUSE
copy-up umask handling that let a low-privileged user create files with
unexpected permissions, CWE-269-class). Both are historical eval classes
only; the skill teaches the fops contract that host fixtures reproduce.
Synthetic: read returning 0 on short, write returning bytes-not-transferred,
forgotten `.owner`, `private_data` freed before last fput, llseek accepting
negative offsets, missing compat_ioctl. Adversarial: a driver that "passes"
a single-open smoke test but breaks when two files share a module global.
False-positive: correct short counts, balanced module refcounts, validated
llseek must NOT be flagged.
