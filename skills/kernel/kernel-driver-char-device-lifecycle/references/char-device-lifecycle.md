# Char Device Lifecycle — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Init and exit must be exact mirror images

- **RULE**: teardown (`device_destroy` → `class_destroy` → `cdev_del` →
  `unregister_chrdev_region`) is the exact reverse of setup
  (`register_chrdev_region` → `cdev_init`+`cdev_add` → `class_create` →
  `device_create`). Every successful step needs a paired cleanup reachable from
  both the exit path and every error path.
- **WHY AI GETS IT WRONG**: agents add cleanup steps in an order that looks
  symmetric but is not (e.g. `class_destroy` before `device_destroy`), or copy
  the init list and forget that reverse order is required; a single
  `class_destroy` in `module_exit` that double-fires via an error path is also
  common.
- **CORRECT REASONING**: each object's teardown precondition is the removal of
  the objects that reference it. A `device` holds a reference to its `class`
  (and its `cdev` via the parent), so `device_destroy` must run first. `cdev_del`
  must run before `unregister_chrdev_region`. Reversing the init order
  guarantees every dependency is released last-in-first-out.
- **EXAMPLE** (bad):
  ```c
  static void __exit dev_exit(void) {
      class_destroy(dev_class);       /* device still references dev_class */
      device_destroy(dev_class, devno);
      cdev_del(&dev_cdev);
      unregister_chrdev_region(devno, 1);
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static void __exit dev_exit(void) {
      device_destroy(dev_class, devno);
      class_destroy(dev_class);
      cdev_del(&dev_cdev);
      unregister_chrdev_region(devno, 1);
  }
  ```
- **VERIFICATION**: `insmod` then `rmmod` under dmesg with a trace printed in
  each step; assert the ordering and that no "double free"/use-after-free oops
  appears. On host: a stub harness that mirrors the calls and asserts the
  reverse order (this skill's examples).
- **SOURCE**: ldd3 ch.3 (registration order, "teardown is the reverse");
  kernel-driver-api (device/class/char-device docs).

## 2. copy_from_user returns bytes NOT copied; check it

- **RULE**: `copy_from_user(dst, src, n)` returns the number of bytes NOT
  copied (0 = success). `copy_to_user` likewise. Unchecked failure leaves
  partial/untouched data that must never be used as if complete.
- **WHY AI GETS IT WRONG**: the model returns "0 = success" correctly but then
  skips the branch; or treats the return as "bytes copied". The 2022 ChatGPT
  demo class of bug was an unbounded `copy_from_user` into a stack buffer whose
  size was silently assumed.
- **CORRECT REASONING**: treat every uaccess return as "amount that did NOT
  make it". For `copy_from_user`: nonzero → either `return -EFAULT` or zero the
  remainder of the destination; never proceed with a partial buffer. For
  `copy_to_user`: nonzero → the user did not receive everything; return
  `-EFAULT`.
- **EXAMPLE** (bad):
  ```c
  char buf[32];
  copy_from_user(buf, user_ptr, user_len);   /* user_len can be > 32 */
  do_something(buf);                          /* stack overflow + partial data */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  char buf[32];
  unsigned long n = copy_from_user(buf, user_ptr, user_len);
  if (n) {
      memset(buf + user_len - n, 0, n);
      return -EFAULT;
  }
  /* bound user_len <= sizeof(buf) BEFORE the copy */
  ```
- **VERIFICATION**: host stub `copy_from_user_emu` that reports the count not
  copied; test with a too-large size and assert the error path is taken. Kernel:
  KASAN + QEMU reproducer.
- **SOURCE**: kernel-driver-api (uaccess API: "returns the number of bytes that
  could not be copied"); ldd3 ch.3.

## 3. read/write direction: kernel read = copy_to_user

- **RULE**: `read` moves data OUT of the device into the user buffer →
  `copy_to_user(buf, dev_data, count)`. `write` moves user data IN →
  `copy_from_user(dev_buf, buf, count)`. Getting this backwards produces
  either kernel-data disclosure to userspace or userspace data written into
  kernel memory.
- **WHY AI GETS IT WRONG**: "read from user" reads like `copy_from_user` and
  the model pairs the names without tracking who is the source. Inverted read
  is a serious leak: a `read` doing `copy_from_user` writes user memory INTO
  the device structure.
- **CORRECT REASONING**: read(2) means the USER is reading FROM the device: the
  kernel must copy device-owned data to the user-supplied buffer → `copy_to_user`.
  write(2) means the USER is writing TO the device: the kernel must copy
  user-supplied data into device memory → `copy_from_user`. Name the source
  first: read's source is kernel, write's source is user.
- **EXAMPLE** (bad):
  ```c
  static ssize_t dev_read(struct file *f, char __user *buf, size_t n, loff_t *o) {
      copy_from_user(buf, &dev_data, n);   /* writes kernel data INTO user buf */
      return n;
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static ssize_t dev_read(struct file *f, char __user *buf, size_t n, loff_t *o) {
      size_t got = min(n, sizeof(dev_data));
      unsigned long left = copy_to_user(buf, dev_data, got);
      return left ? -EFAULT : (ssize_t)got;
  }
  ```
- **VERIFICATION**: run a read() and a write() from a test program and inspect
  the buffer contents; a leaked kernel buffer or a mutated device structure
  proves the inversion. On host: stub harness that mirrors the emulated
  user-space buffer and asserts the transfer direction.
- **SOURCE**: ldd3 ch.3 (read/write implementations); kernel-driver-api.

## 4. Unload must not race open/read/write: module reference counting

- **RULE**: a driver that can be rmmod'd while the file is open must protect the
  module (and its data) with `try_module_get(THIS_MODULE)` in `open` and
  `module_put` in `release` (kref/refcount for per-device state). `rmmod` is
  then refused while references exist.
- **WHY AI GETS IT WRONG**: agents write open/release as no-ops and rely on the
  kernel "not allowing rmmod of a busy module" — but the busy check is the
  reference count; with no `try_module_get`, the count stays 0 and rmmod
  succeeds, after which the next open/read executes freed code.
- **CORRECT REASONING**: `module_refcount` increments in `try_module_get` and
  decrements in `module_put`; `delete_module` returns `-EBUSY` only if the count
  is nonzero. Each successful open must hold one reference; release releases it.
  If open fails, do not leak the reference.
- **EXAMPLE** (bad):
  ```c
  static int dev_open(struct inode *inode, struct file *filp) {
      filp->private_data = &dev_data;  /* no try_module_get */
      return 0;
  }
  /* rmmod succeeds; dev_read later dereferences freed dev_data and code */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static int dev_open(struct inode *inode, struct file *filp) {
      if (!try_module_get(THIS_MODULE))
          return -EBUSY;
      filp->private_data = &dev_data;
      return 0;
  }
  static int dev_release(struct inode *inode, struct file *filp) {
      module_put(THIS_MODULE);
      return 0;
  }
  ```
- **VERIFICATION**: open the device, attempt rmmod while open — it must fail
  with `-EBUSY`; close, rmmod — success. On host: stub counts refs and asserts
  rmmod-blocked-while-open.
- **SOURCE**: ldd3 ch.2 (module reference counting, `try_module_get`);
  kernel-driver-api.

## 5. Bound user-supplied sizes BEFORE the copy

- **RULE**: never copy a user-controlled length into a fixed-size buffer. Check
  the length against the real capacity first, or copy a capped amount, then
  verify the full amount was consumed.
- **WHY AI GETS IT WRONG**: the model trusts that `user_len` is sane because
  the API "looks" length-safe; an unbounded `copy_from_user` into a stack array
  is the exact ChatGPT-demo failure class.
- **CORRECT REASONING**: the size argument to `copy_from_user` is the MAXIMUM
  the kernel will write; the kernel decides it. Compute `n = min(user_len,
  sizeof(kbuf))` (or reject `user_len > sizeof(kbuf)`) BEFORE calling the copy,
  and validate `user_len` against protocol expectations (e.g. ioctl struct
  sizes via `_IOC_SIZE`).
- **EXAMPLE** (bad): `copy_from_user(kbuf, uptr, user_len)` with
  `char kbuf[32]`.
- **COUNTEREXAMPLE** (good):
  ```c
  if (user_len > sizeof(kbuf)) return -EINVAL;
  if (copy_from_user(kbuf, uptr, user_len)) return -EFAULT;
  ```
- **VERIFICATION**: host harness passes `user_len = 4096` against a 32-byte
  buffer and asserts `-EINVAL` (no copy attempted). Kernel: KASAN.
- **SOURCE**: kernel-driver-api (uaccess contract); ldd3 ch.3.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Teardown order | reverse of init; device_destroy → class_destroy → cdev_del → unregister |
| copy_from_user return | bytes NOT copied; 0 = success; nonzero must be handled |
| Direction | kernel read = copy_to_user; kernel write = copy_from_user |
| Unload safety | try_module_get in open, module_put in release |
| Sizes | bound user length against real capacity before the copy |
| Error returns | -EFAULT / -EINVAL / -EBUSY, not silent success |
