# Evaluation — kernel-module-build-out-of-tree

Skill: `skills/kernel/kernel-module-build-out-of-tree`.
Stability target: `evaluated`.

## Verified facts (host, recorded 2026-08-15)

This host is Windows/MSYS2: no `/lib/modules`, no kernel headers, no `insmod`.
The Kbuild syntax rules were validated in two ways that are honest to run here:

```
gcc -Wall -Wextra -Werror -O2 examples/good/version_binding_check.c -o vbc
  run: "mismatch: module built against /lib/modules/6.6.0/build must not be
        loaded on /lib/modules/6.8.0/build"   (exit 0)

make -f examples/good/Makefile --dry-run   (target kernel present? no —
  documented below as target command, NOT executed here)
```

`gcc -fsyntax-only` was NOT applied to the `.c` module sources: they include
`<linux/module.h>` which does not exist on this host — they are reviewed
rather than compiled, and that limitation is recorded honestly.

File-level review facts:

- `bad/Makefile.cc_recipe` — plain `cc` recipe, no Kbuild `obj-m`: the module
  would lack vermagic and fail `insmod` with "Invalid module format".
- `bad/Makefile.deprecated_flags` — `EXTRA_CFLAGS` (deprecated → `ccflags-y`)
  plus a hardcoded `/lib/modules/6.0.0...` `KDIR` that does not match the
  running kernel → version-magic mismatch at load.
- `bad/bad_no_license.c` — no `MODULE_LICENSE`, calls a GPL-only symbol →
  unresolved at load + taint.
- `good/Makefile` — canonical Kbuild two-file pattern (`obj-m`,
  `good_mod-objs`, `ccflags-y`, `make -C $(KDIR) M=$(PWD)`).
- `good/core.c` — `MODULE_LICENSE("GPL")`, correct init/exit, vermagic left
  intact.
- `good/version_binding_check.c` — host-run demonstration of the vermagic-match
  rule (output above).

## Synthetic evals

- easy/negative: `Makefile.cc_recipe` — hand-rolled compile must be flagged.
- easy/negative: `bad_no_license.c` — missing license + GPL-only symbol.
- medium/negative: `Makefile.deprecated_flags` — `EXTRA_CFLAGS` and wrong `KDIR`.
- easy/positive: `good/Makefile` — must be approved.
- medium/positive: `good/core.c` — license/init/exit/vermagic must be approved.

## False-positive evals (correct code must not be flagged)

- `obj-m := foo.o` + `make -C $(KDIR) M=$(PWD)` — the canonical pattern.
- `MODULE_LICENSE("GPL")` on a driver that only uses `EXPORT_SYMBOL` (not only
  GPL) symbols — still fine, not a false positive.
- `ccflags-y` with `-I$(src)/...` — correct, must NOT be flagged as deprecated.
- A multi-file module via `foo-objs := a.o b.o` — correct Kbuild.

## Historical evals (Codex macbook12 PR#5, 2025)

- Class: wifi driver built in CI against the dev kernel's headers, shipped to
  users whose kernel differed; load failure on target.
- Task: the agent must (a) detect the version binding problem, (b) pin
  `KDIR`/the target kernel release, (c) reject "build succeeded" as the success
  criterion, and (d) NOT "fix" by stripping vermagic or forcing load.
- Verify: build against the reported target kernel and `insmod` on it. Not
  runnable on this host — documented as target verification.

## Adversarial evals

- A module that "builds fine" locally but the Makefile hardcodes
  `/lib/modules/<old-version>/build` — the agent must flag the stale `KDIR`.
- A proposed "fix" that adds `-fno-pic` or `CONFIG_MODULE_FORCE_LOAD` to make a
  mismatched module load — must be rejected.
- A Makefile that sets `CROSS_COMPILE` but uses `uname -r` for `KDIR` while
  cross-building — must be flagged (target arch/kernel both wrong).

## Verification commands (target — Linux, documented, NOT run here)

```
# build for the RUNNING local kernel (host verification once Linux is available):
make -C /lib/modules/$(uname -r)/build M=$PWD modules
modinfo $PWD/good_mod.ko | grep vermagic
sudo insmod $PWD/good_mod.ko
sudo rmmod good_mod

# build for a specific other kernel:
make -C /path/to/linux-6.6.0 M=$PWD ARCH=x86_64 modules
```

## Scoring

- precision: every flagged file maps to a named reference rule.
- recall: all bad files detected.
- FP-rate: good files produce zero flags.
