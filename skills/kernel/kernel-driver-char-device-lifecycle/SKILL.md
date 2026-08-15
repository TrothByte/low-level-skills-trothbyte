---
name: kernel-driver-char-device-lifecycle
description: Use when writing or reviewing Linux character device drivers: file_operations dispatch, copy_from_user/copy_to_user return handling, cdev/class/device creation and teardown symmetry, module unload cleanup, and reference counting. Prevents unchecked user copies, inverted read/write contracts, double class_destroy, and unloading while the device is open.
---

# Linux Character Device Lifecycle and file_operations Contracts

## When to use

- Writing or reviewing a Linux char driver: `file_operations`, `ioctl`,
  `read`/`write`, `mmap`, `open`/`release`.
- Adding or removing a `cdev`, `class_create`/`class_destroy`,
  `device_create`/`device_destroy`, or module init/exit functions.
- Debugging "device disappears at rmmod", "double free on unload", "ioctl
  crashes", or "read returns garbage".
- Any code that copies data between kernel and user space
  (`copy_from_user`/`copy_to_user`, `get_user`/`put_user`).
- Adding reference counting (`try_module_get`/`module_put`, kref) to protect
  against unload during use.

## When not to use

- Block, network, or platform/bus drivers with different lifecycle models
  (`blkdev`, `netdev`, probe/remove on buses) — the object lifetime rules
  differ, though copy_to/from_user rules still apply.
- Userspace device emulation (VirtIO, vfio) where the kernel driver is a thin
  front for another subsystem.
- Debugfs/procfs pseudo-files — different creation API and teardown rules.
- This skill covers correctness contracts; for uaccess attack surface review use
  `kernel-uaccess-safety`; for kbuild details use `kernel-module-build-out-of-tree`.

## What the agent often gets wrong

- Ignores the return value of `copy_from_user` and then uses the partially
  copied buffer (the 2022 ChatGPT demo class of bug: unbounded
  `copy_from_user` into a fixed stack/array buffer).
- Inverts the read/write contract: `read` should copy device data to the user
  buffer (kernel `read` = `copy_to_user`), `write` should copy user data into
  the kernel (`copy_from_user`). Agents routinely write `read` that copies FROM
  user space.
- Calls `class_destroy` twice on unload (once in `__exit`, once in an error
  path), or destroys the class before all devices are removed.
- Frees memory while the device is still open / module is referenced —
  no reference counting, no `try_module_get`, so rmmod succeeds and the next
  open/read dereferences freed code/data.
- Forgets `device_create`'s returned `struct device *` needs `device_destroy`
  before `class_destroy`; forgets the minor number and `MKDEV` registration
  order (allocate chrdev first, then cdev_init/add, then device_create).
- Treats `copy_to_user` failure as "just skip it" and returns 0 instead of an
  error code like `-EFAULT`.

## How to reason correctly

1. Draw the lifecycle timeline on paper: init allocates chrdev region →
   `cdev_init`+`cdev_add` → `class_create` → `device_create`. Unload is the
   exact mirror in reverse order: `device_destroy` → `class_destroy` →
   `cdev_del` → `unregister_chrdev_region`. Every successful step must have a
   paired cleanup reachable from an error path.
2. Every `copy_from_user`/`copy_to_user` result must be checked: non-zero means
   the copy was incomplete. For `copy_from_user`, either return `-EFAULT` or
   zero the remainder — never proceed with partial data.
3. Match direction to semantics: `read` (user wants data) =
   `copy_to_user(user_buf, kernel_data, len)`; `write` (user supplies data) =
   `copy_from_user(kernel_buf, user_buf, len)`. Confirm by naming the two
   pointers, never by guess.
4. For unload safety: `open` should do `try_module_get(THIS_MODULE)`,
   `release` must `module_put`. If the driver keeps state across opens, use a
   kref or refcount and block removal until it reaches zero
   (`try_module_get` fails if the module is being removed).
5. Size discipline: cap user-supplied sizes against the real buffer capacity
   BEFORE copying; never copy a user-controlled length into a fixed array.

## What to verify

- Every uaccess call's return value is checked; partial-copy paths return an
  error.
- `read`/`write` directions are correct (spot-check the `copy_*_user` direction
  against the direction of data flow).
- Cleanup order is the reverse of init; `class_destroy` appears exactly once.
- `try_module_get`/`module_put` pairing (open/release, or per-ioctl on shared
  entry points).
- No user-controlled length reaches a stack/array copy without a bound check.
- rmmod and repeated open/read/write/close loops do not leak or double-free.

## How to verify

```
# On a Linux host with kernel headers for the running kernel:
make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo insmod lifecycle_demo.ko
sudo rmmod lifecycle_demo.ko          # must not crash or leak
dmesg | tail                          # inspect init/exit trace

# Full cycle under QEMU (documented target, not this host):
make -C /lib/modules/$(uname -r)/build M=$PWD modules
qemu-system-x86_64 -kernel arch/x86/boot/bzImage -initrd initramfs
# inside guest: insmod, open/read/write/ioctl/close, rmmod, check no BUG/oops
```

See `evals/README.md` for the actual status: the lifecycle logic is verified on
this host with self-contained stubs; the QEMU insmod/rmmod cycle is a
documented target command.

## Where the knowledge comes from

- `ldd3` — Linux Device Drivers, 3rd ed.: char device registration order
  (ch. 3), file_operations, open/release, module refcounting with
  `try_module_get` (ch. 2), and the lifecycle diagrams.
- `kernel-driver-api` — kernel.org "Driver APIs" / "Char drivers" docs:
  `cdev`/`device_create`/`class` contract, uaccess API contract
  (`copy_to_user` returns number of bytes NOT copied).

## Related skills

- `kernel-uaccess-safety` — attack-surface rules for the same uaccess calls.
- `kernel-module-build-out-of-tree` — building/loading this module.
- `kernel-api-drift-migration` — APIs whose contracts changed across versions.
- `c-errno-and-syscall-returns` — why `-EFAULT`-style returns must be checked.
- `kernel-atomic-context` — which calls are forbidden in atomic/ioctl context.

## Evaluation

- Synthetic: flag bad/ (unchecked copy_from_user into a stack buffer, inverted
  read/write, double class_destroy, missing module refcount); approve good/.
- False-positive: a checked copy with `if (copy_from_user(...)) return -EFAULT;`
  must NOT be flagged; a proper reverse-order teardown must NOT be flagged as
  double-cleanup.
- Historical/adversarial: an rmmod that succeeds while the file is still open
  must be identified as a use-after-free hazard; a driver whose ioctl copies an
  unbounded user size into a fixed buffer must be flagged even though it
  compiles and "works" for small inputs.
- Verified facts and commands: `evals/README.md`.
