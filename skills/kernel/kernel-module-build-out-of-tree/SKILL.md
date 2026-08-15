---
name: kernel-module-build-out-of-tree
description: Use when building, packaging, or fixing Linux kernel modules outside the kernel tree: Kbuild Makefiles, Kconfig, kernel-headers dependency and version binding, MODULE_LICENSE/EXPORT_SYMBOL placement, and mismatch between a module built against one kernel and loaded on another. Prevents "unknown symbol", "disagrees about version", and silently-wrong module builds.
---

# Out-of-Tree Kernel Module Builds (Kbuild, headers, version binding)

## When to use

- Writing the `Makefile`/Kbuild file and module source for a driver built
  outside the kernel tree (`M=$PWD` builds).
- Fixing build/load errors: `unknown symbol`, `version magic`, `no symbol
  version`, `unable to handle kernel paging request`, or a module that builds
  on the dev machine but not on the target.
- Deciding whether the module needs `Kconfig` integration (in-tree) or a plain
  Kbuild `obj-m` file (out-of-tree).
- Adding `MODULE_LICENSE`, `MODULE_AUTHOR`, `MODULE_DESCRIPTION`, or deciding
  what may be `EXPORT_SYMBOL`'d.
- CI for driver development where the module must be rebuilt per kernel
  version.

## When not to use

- Modules built in-tree (`obj-$(CONFIG_X)` inside the kernel source) — the
  rules differ (Kconfig is required, version.h comes from the tree).
- Userspace development of driver logic (use the `kernel-driver-char-device-lifecycle`
  host-stub pattern instead).
- Kernel build debugging that is really about toolchain/compiler issues — use
  `toolchain-drift` skills.
- Cross-compiling for a target kernel: same rules apply, but the target
  kernel's `build` dir must be provided via `CROSS_COMPILE` + `ARCH`.

## What the agent often gets wrong

- Writes a plain `cc -c file.c` Makefile instead of the Kbuild `obj-m := ...`
  + `make -C $(KDIR) M=$(PWD) modules` pattern — the "module" is then not
  version-tagged and cannot be inserted (`Invalid module format`).
- Forgets the kernel-headers dependency: the build needs the *running kernel's*
  `lib/modules/$(uname -r)/build` (headers + Module.symvers + vmlinux.h), not
  the distro's arbitrary headers; a Codex PR (#5) for the macbook12 wifi
  driver built against a dev kernel and failed to load on the user's machine.
- Omits `MODULE_LICENSE("GPL")` (or picks a wrong license string), producing
  "kernel tainted" warnings and — critically — hidden symbols: GPL-only
  symbols (`EXPORT_SYMBOL_GPL`) become unusable by non-GPL modules.
- Ignores version binding: a module built for kernel A loaded on kernel B fails
  at `insmod` with `version magic 'x.y.z-...' should be 'x.y.z-...'`; agents
  often "fix" the build by disabling vermagic instead of building against the
  target.
- Assumes `EXTRA_CFLAGS` still works (it is `ccflags-y` since long ago) or that
  `CFLAGS_<name>.o` syntax is the same across versions.
- Uses `$(shell uname -r)` in the Makefile so every build targets the local
  kernel, even when the module is destined for another kernel.

## How to reason correctly

1. Decide where the module will LOAD (the running target kernel) — that exact
   kernel's build tree is the only valid source of headers/vermagic. For a
   local host: `KDIR = /lib/modules/$(shell uname -r)/build`.
2. Use the two-file convention: the module Makefile is Kbuild syntax
   (`obj-m := name.o`, `name-objs := a.o b.o` if multi-file), and the driver
   makefile just invokes it:
   `make -C $(KDIR) M=$(PWD) modules`. Never hand-roll the compile.
3. Set `MODULE_LICENSE("GPL")` for GPL drivers so GPL-only exports resolve;
   keep `MODULE_INFO(vermagic, ...)` intact — do not suppress the version check
   as a "fix".
4. Remember that out-of-tree modules get symbols from the build tree's
   `Module.symvers`; a missing exported symbol fails at link ("unknown symbol
   in module") if the build tree's Module.symvers lacks it — pin to the target's
   headers, and re-export if the symbol is GPL-only vs EXPORT_SYMBOL.
5. If the module is part of a real kernel source tree (drivers/staging style),
   it needs `Kconfig` (`tristate`/`bool`) and the Makefile uses
   `obj-$(CONFIG_FOO) += foo.o` — do not conflate the two worlds.

## What to verify

- `make -C <target-kdir>/build M=$PWD modules` completes with `exit 0` and
  produces `name.ko`.
- `modinfo name.ko` shows vermagic matching the target kernel
  (`vermagic: 6.x.y-... SMP mod_unload modversions`).
- `modprobe name.ko` / `insmod` on the target succeeds without "Invalid module
  format", "version magic", or "unknown symbol".
- License string resolves GPL-only symbols (check with `nm` for unresolved
  symbols or `dmesg` for taint warnings).
- No `ccflags-y`/`CFLAGS_*` deprecated flags; no hardcoded compiler
  optimization flags that break with the kernel's `-fno-strict-aliasing`
  defaults.

## How to verify

```
# On a Linux host with the target kernel's headers installed:
make -C /lib/modules/$(uname -r)/build M=$PWD modules
modinfo $PWD/<module>.ko | grep vermagic
sudo insmod $PWD/<module>.ko
dmesg | tail
sudo rmmod <module>

# cross-build against a specific kernel source tree:
make -C /path/to/linux-6.6.0 M=$PWD ARCH=<arch> CROSS_COMPILE=<prefix>- modules
```

On this host (Windows, no Linux kernel headers) the Kbuild syntax is validated
by a self-contained `Makefile` review plus a syntax check of the rules; the
Linux build/insert cycle is documented as the target verification command —
see `evals/README.md` for the honest status.

## Where the knowledge comes from

- `kernel-kbuild` — kernel.org Kbuild documentation: `obj-m`, `ccflags-y`,
  `M=`, `-C` semantics, module versioning, Module.symvers.
- `kernel-driver-api` — module init/exit conventions and license tagging.

## Related skills

- `kernel-driver-char-device-lifecycle` — what the built module must do
  correctly at runtime.
- `kernel-api-drift-migration` — symbols that disappeared from `EXPORT_SYMBOL`
  across versions (builds fine, links/loads against the wrong kernel).
- `kernel-source` — primary reference for Kbuild internals when debugging
  odd `make` failures.
- `toolchain-drift` — compiler flag mismatches that break module builds.

## Evaluation

- Synthetic: flag a `cc file.c -o file.ko` Makefile; flag missing
  `MODULE_LICENSE`; flag `EXTRA_CFLAGS`; flag `uname -r` used for a target that
  is not the local host. Approve the canonical two-file Kbuild pattern.
- False-positive: a correct `obj-m := name.o` + `make -C $(KDIR) M=$(PWD)`
  Makefile must NOT be flagged; `MODULE_LICENSE("GPL")` + GPL symbols must be
  approved.
- Historical: the macbook12 wifi Codex PR #5 class of failure — module built
  against dev headers, fails on the user kernel; the agent must pin the target
  kernel's headers.
- Adversarial: a "build succeeded" module that fails `insmod` with
  `version magic` mismatch — the agent must diagnose version binding, not
  suppress vermagic.
- Verified facts and commands: `evals/README.md`.
