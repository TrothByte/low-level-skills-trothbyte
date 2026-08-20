# Evaluation — vfs-file-operations-and-fops

Skill: `skills/kernel/vfs-file-operations-and-fops`. Stability target:
`evaluated`.

## Verified facts (host, this run)

- Examples compile clean with `gcc -Wall -Wextra -Werror -O2` (MinGW gcc
  16.1.0, Windows host, PowerShell) using self-contained stubs
  (`examples/stubs.h`) — no kernel headers required.
- `examples/good/good_fops.c` runs with all assertions passing and prints
  `ALL CHECKS PASSED` (exit 0).
- `examples/bad/bad_fops.c` compiles and runs; it reproduces three fops
  contract violations at runtime and prints a `BUG reproduced:` diagnostic
  for each (exit 0) without crashing the harness.

| Example | Compile | Run | Observation |
|---|---|---|---|
| good/good_fops.c | 0 | 0 | "ALL CHECKS PASSED"; read/write return semantics, llseek validation, ioctl dispatch, refcount balance asserted |
| bad/bad_fops.c | 0 | 0 | 3 bugs reproduced: module unloaded while open (.owner missing); write returned bytes NOT transferred; use-after-free of private_data |

Host quirk recorded: the native MinGW linker does not resolve `/tmp`, so the
`-o` target is an existing temp dir (`$env:TEMP\kilo`). Build artifacts are
never written into the repo.

NOT verified on this host (documented targets, do NOT claim to have run):
kernel build + KASAN, KUnit fops tests, QEMU boot, insmod/rmmod cycle,
sparse `make C=1 CHECK=sparse`, syzkaller.

## Historical CVE evals (adversarial)

| CVE | Class | Fixture | Detect | Fix | Verify |
|---|---|---|---|---|---|
| CVE-2022-0185 | unsigned underflow, CWE-191 | fs/fs_context.c `legacy_parse_param()` (mount-API path) | `PAGE_SIZE - 2 - size` underflows when unsigned `size` is large, then the copy overflows | rewrite as `size + len + 2 > PAGE_SIZE` | KASAN + mount(2) reproducer |
| CVE-2023-0386 | incorrect permission handling, CWE-269-class | overlayfs FUSE copy-up umask handling | copy-up did not apply the caller's umask, so unprivileged users could create upper-layer files with unexpected permissions (privilege escalation) | apply the umask correctly during copy-up | KASAN + overlayfs/FUSE reproducer as non-root |

Each eval: DETECT (find the missing validation) -> EXPLAIN (which fops/VFS
contract was violated) -> FIX (add the check) -> VERIFY (KASAN clean +
reproducer). Both CVEs are historical eval classes: they bound where
permission and length validation must live in VFS paths.

## Synthetic evals

- easy/positive: read/write returning correct byte counts must NOT be flagged.
- easy/negative: `read` returning 0 on a short read must be flagged.
- easy/negative: `write` returning bytes NOT transferred must be flagged.
- medium/negative: forgotten `.owner` must be flagged.
- medium/negative: `private_data` freed before the last fput must be flagged.
- hard/negative: a module-level alias of `private_data` read after close
  must be flagged (use-after-free).
- hard/negative: `.llseek` accepting negative SEEK_SET/SEEK_CUR offsets must
  be flagged.

## Adversarial evals

- A driver that "passes" a single-open smoke test but breaks when two opens
  share a module global (`last_ctx`-style state) — agent must flag the stale
  alias even though the first close is clean.
- Code that returns a positive errno (`return EFAULT;`), which the VFS treats
  as a byte count and advances `f_pos` by — agent must catch the missing
  minus sign.
- An ioctl handler trusting `_IOC_SIZE(cmd)` as a copy length — agent must
  flag it even when it "works" for well-formed commands.
- A driver that manages its own position counter and also lets the VFS
  advance `*pos` (doubled position).

## False-positive evals (correct code must not be flagged)

- A short positive read/write return for a device with less data — do NOT
  flag.
- `.llseek` returning `0` at EOF via SEEK_END with correct bounds — do NOT
  flag.
- `.owner = THIS_MODULE` with a balanced open/release refcount — do NOT flag.
- `compat_ioctl` sharing the native handler when the command layout is
  identical — do NOT flag.
- `private_data` allocated in open and freed once in release with no stale
  aliases — do NOT flag.

## Verification commands

Host (self-contained stubs — recorded this run, Windows/MinGW + PowerShell):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_fops.c -o "$env:TEMP\kilo\good_fops.exe"
& "$env:TEMP\kilo\good_fops.exe"      # exit 0, "ALL CHECKS PASSED"
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_fops.c -o "$env:TEMP\kilo\bad_fops.exe"
& "$env:TEMP\kilo\bad_fops.exe"       # exit 0, three "BUG reproduced:" lines
```

POSIX host variant (same sources, no changes):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_fops.c -o /tmp/good_fops
/tmp/good_fops
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_fops.c -o /tmp/bad_fops
/tmp/bad_fops
```

Target (kernel) — documented only, NOT run here:

```
# sparse: fops signature / __user annotation violations
make C=1 CHECK=sparse

# full VM: KASAN + KUnit fops tests under QEMU
make defconfig && make -j$(nproc)
qemu-system-x86_64 -kernel arch/x86/boot/bzImage \
  -append "console=ttyS0 kasan=on" -nographic

# in the guest: insmod the driver, open/read/write/llseek/ioctl/close in a
# loop, then rmmod with the device open — must fail with EBUSY, no oops
```

## Scoring

- precision: every flagged pattern maps to a real fops/VFS contract rule.
- recall: each bad snippet (missing .owner, wrong write return, UAF of
  private_data) is detected at runtime.
- FP-rate: good snippets produce zero flags.
