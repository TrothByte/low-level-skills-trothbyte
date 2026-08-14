# Evaluation — kernel-uaccess-safety

Skill: `skills/kernel/kernel-uaccess-safety`. Stability target: `evaluated`.

## Verified facts (host, this run)

- All examples compile clean with `gcc -Wall -Wextra -Werror -O2` using
  self-contained stubs (`examples/stubs.h`) — no kernel headers required.
- Good examples run with assertions passing (exit 0).
- Bad examples compile and run; each reproduces its flaw and prints a
  "BUG reproduced" diagnostic (exit 0) without crashing the harness.

| Example | Compile | Run | Observation |
|---|---|---|---|
| good/good_uaccess.c | 0 | 0 | assertions pass (copy/get/put/strn semantics) |
| good/good_ioctl.c | 0 | 0 | -ENOTTY / -EINVAL / 0 dispatch verified |
| good/good_mmap.c | 0 | 0 | noncached prot asserted; pfn range rejected |
| good/good_poll.c | 0 | 0 | poll_wait + fasync flow asserted |
| bad/bad_uaccess.c | 0 | 0 | "BUG reproduced: __user pointer dereferenced directly" |
| bad/bad_copy_return.c | 0 | 0 | "BUG reproduced: used data copy_from_user failed to deliver" |
| bad/bad_access_ok.c | 0 | 0 | "BUG reproduced: raw copy bypassed access_ok" |
| bad/bad_ioctl_size.c | 0 | 0 | "BUG reproduced: oversized copy wrote past the ioctl argument buffer" |
| bad/bad_mmap_cache.c | 0 | 0 | "BUG reproduced: device I/O mapped cacheable" |

NOT verified on this host (documented targets, do NOT claim to have run):
kernel build + KASAN, KUnit uaccess tests, QEMU boot, syzkaller runs.

## Historical CVE evals (adversarial)

| CVE | Class | Fixture | Detect | Fix | Verify |
|---|---|---|---|---|---|
| CVE-2021-22555 | heap OOB write, CWE-787 | net/netfilter/x_tables.c, `xt_compat_target_from_user()` | user-controlled `target_size` used to size the compat copy without validation against the fixed compat target size | validate `target_size` before copying (commits 9fa492cdc160, b29c457a6511) | KASAN + 32-bit compat reproducer |
| CVE-2022-0185 | integer underflow, CWE-191 | fs/fs_context.c `legacy_parse_param()` | `PAGE_SIZE - 2 - size` underflows when unsigned `size` is large, then the copy overflows | rewrite as `size + len + 2 > PAGE_SIZE` (commit 722d94847de2) | KASAN + mount(2) reproducer |

Each eval: DETECT (find the missing validation) -> EXPLAIN (which uaccess
rule was violated) -> FIX (add the check) -> VERIFY (KASAN clean + reproducer).

## Synthetic evals

- easy/positive: `copy_from_user` with a return check must NOT be flagged.
- easy/negative: direct `__user` deref must be flagged.
- medium/negative: ignored `copy_from_user` return must be flagged.
- medium/negative: ioctl trusting `_IOC_SIZE(cmd)` must be flagged.
- hard/negative: `strnlen_user` `count + 1` sentinel missed must be flagged.
- hard/negative: device mmap without `pgprot_noncached` must be flagged.

## Adversarial evals

- Code that "works" in a 64-bit-only build but breaks 32-bit compat (missing
  `compat_ioctl` / wrong struct layout) — agent must not declare it correct.
- Code whose ioctl cmd passes `_IOC_SIZE` but not the driver's expected
  struct size — agent must catch the mismatch.
- An mmap that maps device memory cacheable and "passes" a QEMU smoke test.

## False-positive evals (correct code must not be flagged)

- Checked copies: `if (copy_from_user(...)) return -EFAULT;` — do NOT flag.
- Redundant-but-safe `access_ok` before a checked helper — do NOT flag.
- Validated ioctl: `_IOC_TYPE` / `_IOC_NR` / `_IOC_SIZE` all checked — do NOT
  flag.
- `remap_pfn_range` with `pgprot_noncached` — do NOT flag.
- RAM (not MMIO) mmap with default cached prot — do NOT flag.

## Verification commands

Host (self-contained stubs — recorded this run):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_uaccess.c -o /tmp/good_uaccess
gcc -Wall -Wextra -Werror -O2 examples/good/good_ioctl.c -o /tmp/good_ioctl
gcc -Wall -Wextra -Werror -O2 examples/good/good_mmap.c -o /tmp/good_mmap
gcc -Wall -Wextra -Werror -O2 examples/good/good_poll.c -o /tmp/good_poll
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_uaccess.c -o /tmp/bad_uaccess
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_copy_return.c -o /tmp/bad_copy_return
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_access_ok.c -o /tmp/bad_access_ok
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_ioctl_size.c -o /tmp/bad_ioctl_size
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_mmap_cache.c -o /tmp/bad_mmap_cache
```

Target (kernel) — documented only, NOT run here:

```
# sparse: __user annotation violations
make C=1 CHECK=sparse

# full VM: KASAN + KUnit uaccess tests under QEMU
make defconfig && make -j$(nproc)
qemu-system-x86_64 -kernel arch/x86/boot/bzImage \
  -append "console=ttyS0 kasan=on" -nographic

# fuzz a test driver with syzkaller (CONFIG_KCOV + KASAN enabled)
./bin/syz-manager -config manager.cfg
```

## Scoring

- precision: every flagged pattern maps to a real uaccess rule.
- recall: each bad snippet is detected.
- FP-rate: good snippets produce zero flags.
