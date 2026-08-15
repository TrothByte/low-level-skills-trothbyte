# Evaluation — kernel-driver-char-device-lifecycle

Skill: `skills/kernel/kernel-driver-char-device-lifecycle`.
Stability target: `evaluated`. Toolchain: gcc 16.1.0 (MSYS2 ucrt64, Windows).

## Verified facts (host, recorded 2026-08-15)

Self-contained host stubs (`examples/stubs.h`) model the uaccess contract
(bytes NOT copied), the module reference counter, and the class/device/cdev
lifecycle ordering, so the lifecycle logic is exercised without kernel headers.
Commands run from the skill directory:

```
gcc -Wall -Wextra -Werror -O2 -Iexamples -c examples/good/good_bounded_copy.c    exit 0
gcc -Wall -Wextra -Werror -O2 -Iexamples -c examples/good/good_rw_direction.c   exit 0
gcc -Wall -Wextra -Werror -O2 -Iexamples -c examples/good/good_teardown_order.c exit 0
gcc -Wall -Wextra -Werror -O2 -Iexamples examples/good/good_refcount.c -o refgood
  run: "refs while open: 1 (rmmod must refuse: -EBUSY)"
       "refs after close: 0 (rmmod allowed)"

gcc -Wall -Wextra -Werror -O2 -Iexamples -c examples/bad/bad_unbounded_copy.c    exit 0
gcc -Wall -Wextra -Werror -O2 -Iexamples -c examples/bad/bad_inverted_rw.c      exit 0
gcc -Wall -Wextra -Werror -O2 -Iexamples -c examples/bad/bad_teardown_order.c   exit 0
gcc -Wall -Wextra -Werror -O2 -Iexamples examples/bad/bad_no_refcount.c -o refbad
  run: "refs after open (expect 0 if unprotected): 0"   <-- bug reproduced
```

Bad files compile clean — they are silent, must be caught by review:

- `bad_unbounded_copy.c`: `copy_from_user_emu(stack_buf, user_ptr, user_len)`
  with `user_len` unbounded against `char stack_buf[32]`. In the stub,
  `user_len` between 33 and 4096 overflows the buffer silently (no size check);
  only `user_len > 4096` triggers the return check. The review-time catch: the
  copy target size must be validated against `sizeof(stack_buf)` BEFORE the call.
- `bad_inverted_rw.c`: `read` performs `copy_from_user_emu(device_regs, buf, n)`
  — direction inverted.
- `bad_teardown_order.c`: `class_destroy` before `device_destroy` + a second
  `class_destroy` reachable from the init error path.
- `bad_no_refcount.c`: refs stay 0 while the file is "open" — rmmod would
  succeed; the printed output confirms the unprotected state.

NOT verified on this host (documented targets, do NOT claim to have run):
Linux kernel build, `insmod`/`rmmod` on a real or QEMU Linux guest, KASAN
run, udev population. The lifecycle ordering was exercised only through the
host stubs.

## Synthetic evals

- easy/negative: `bad_unbounded_copy.c` — unchecked size into a stack buffer.
- easy/negative: `bad_inverted_rw.c` — inverted read direction.
- medium/negative: `bad_teardown_order.c` — wrong teardown order + double
  class_destroy.
- medium/negative: `bad_no_refcount.c` — no try_module_get in open.
- easy/positive: `good_bounded_copy.c` — bounded + checked copy must NOT be
  flagged.
- easy/positive: `good_rw_direction.c` — correct directions, -EFAULT on copy
  failure.
- medium/positive: `good_teardown_order.c` — reverse-of-init teardown,
  class_destroy exactly once.
- medium/positive: `good_refcount.c` — refs 1 while open, 0 after close.

## False-positive evals (correct code must not be flagged)

- `if (copy_from_user(...)) return -EFAULT;` on a properly bounded buffer.
- `copy_to_user` in `read` and `copy_from_user` in `write` — correct directions.
- A teardown sequence that is exactly the reverse of init — do NOT flag as
  "double cleanup" (the paired `device_destroy`/`class_destroy` is required).
- A single `class_destroy` call guarded by an `if (devices_created)` — correct.
- `try_module_get` in open with matching `module_put` in release — do NOT flag.

## Historical / adversarial evals

- Historical (2022 ChatGPT demo): an unbounded `copy_from_user` into a stack
  buffer that "compiles and works" for small inputs must be flagged even though
  it passes a happy-path smoke test.
- Adversarial: a driver where `rmmod` succeeds while the device node is open —
  the agent must identify the missing reference count and the resulting
  use-after-free, not "fix" the symptom by removing open/release.
- Adversarial: an ioctl that validates `_IOC_SIZE` but then copies
  user-controlled `size` into a fixed array — must be flagged as an unbounded
  copy (bridges to `kernel-uaccess-safety`).

## Verification commands (target — Linux, documented, NOT run here)

```
make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo insmod lifecycle_demo.ko
sudo rmmod lifecycle_demo.ko          # must not crash or leak
# open/read/write/ioctl/close loop from a userspace test program
# QEMU: qemu-system-x86_64 -kernel bzImage -initrd initramfs, then insmod/rmmod
```

## Scoring

- precision: every flagged case maps to a named reference rule.
- recall: all bad snippets detected.
- FP-rate: good snippets produce zero flags.
