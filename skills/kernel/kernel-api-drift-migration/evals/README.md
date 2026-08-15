# Evaluation — kernel-api-drift-migration

Skill: `skills/kernel/kernel-api-drift-migration`.
Stability target: `evaluated`.

## Verified facts (host, recorded 2026-08-15)

This host is Windows/MSYS2: no Linux kernel headers, no `/proc/kallsyms`, no
`Module.symvers`. The availability-guard rule was exercised with a host stub
that models the 6.9+ unexported-symbol outcome:

```
gcc -Wall -Wextra -Werror -O2 examples/good/availability_demo.c -o avail_demo
avail_demo 1
  "disabled: sys_call_table unavailable on this kernel"     (exit 0)

avail_demo 0
  "hook installed with verified table"                       (exit 0)
```

The kernel-module `.c` examples (`bad_syscall_table_hook.c`,
`bad_unverified_symbol.c`, `good_guarded_hook.c`) include `<linux/module.h>`,
which does not exist on this host — they were reviewed, NOT compiled. That
limitation is recorded honestly. They are reviewed against the reference rules:
- `bad_syscall_table_hook.c` — `extern void **sys_call_table` + unguarded use:
  the 2024 StackOverflow silent-hook class on 6.9+.
- `bad_unverified_symbol.c` — assumes `fbdev_setup_api` availability +
  `MODULE_LICENSE("Proprietary")` against an assumed GPL-only export.
- `good_guarded_hook.c` — resolver returns NULL → feature disabled loudly,
  `-ENOTSUPP`; the required pattern.

## Synthetic evals

- easy/negative: `bad_syscall_table_hook.c` — unguarded sys_call_table on 6.9+.
- medium/negative: `bad_unverified_symbol.c` — unverified export + license
  mismatch.
- easy/positive: `good_guarded_hook.c` — NULL-check + loud disable + fallback.
- easy/positive: `availability_demo.c` — host-run demonstration.

## False-positive evals (correct code must not be flagged)

- A guarded hook whose resolver result is NULL-checked and which returns
  `-ENOTSUPP` — do NOT flag.
- A driver that states its pinned kernel version and verifies its symbols in
  `Module.symvers` — do NOT flag as drifting.
- A migration patch that checks `LINUX_VERSION_CODE` and provides both paths
  — do NOT flag.
- ftrace/BPF-based hooks (the supported 6.9+ mechanism) — do NOT flag.

## Historical evals (StackOverflow 2024, sys_call_table)

- Class: symbol removed from export (6.9+); hook compiles, loads, silently does
  nothing. Reported on StackOverflow 2024.
- Task: the agent must (a) recognize the compile-success/dead-runtime
  mismatch, (b) require a runtime availability check, (c) recommend ftrace/BPF
  as the supported mechanism, and (d) NOT declare the module working because
  `insmod` returned 0.
- Verify: check `/proc/kallsyms` and observe the hook firing on the target.
  Not runnable on this host — documented as target verification.

## Adversarial evals

- A DRM fbdev→client_setup migration that compiles on both 6.8 and 6.12 but
  only initializes the client on one — the agent must pin the target and check
  the API at that exact tag.
- An IIO migration where the agent "fixes" a compile error by changing a field
  name to match a different kernel version's struct — must be caught as
  version-hopping.
- A proposed "solution" that adds a kallsyms lookup returning `0` and then
  continues to call through the NULL pointer — must be rejected.

## Verification commands (target — Linux, documented, NOT run here)

```
grep sys_call_table /proc/kallsyms
grep <symbol> /lib/modules/$(uname -r)/build/Module.symvers
make -C /lib/modules/$(uname -r)/build M=$PWD modules
insmod <mod>.ko && dmesg | tail     # verify the hook actually fires
```

## Scoring

- precision: every flagged case maps to a named reference rule.
- recall: all bad files detected.
- FP-rate: good files produce zero flags.
