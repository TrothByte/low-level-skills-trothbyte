# Out-of-Tree Module Builds — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Out-of-tree modules must use the Kbuild two-file pattern

- **RULE**: an out-of-tree module is built by invoking the kernel's Kbuild with
  `make -C $(KDIR) M=$(PWD) modules`, where the module Makefile contains Kbuild
  syntax (`obj-m := name.o`), NOT a plain `cc file.c -o file.o` recipe.
- **WHY AI GETS IT WRONG**: agents write an ordinary Makefile with `cc -c` and
  a link line, treating the kernel as an ordinary library; the result has no
  vermagic, no modpost version processing, and fails `insmod` with
  "Invalid module format".
- **CORRECT REASONING**: Kbuild performs the actual compile with the kernel's
  flags (`-I$(srctree)/arch/...`, `-fno-strict-aliasing`, etc.), runs
  modpost to resolve symbols against `Module.symvers`, and stamps the vermagic
  tag. The module Makefile only *declares* targets (`obj-m`, `name-objs`,
  `ccflags-y`); the driver-side makefile is a one-liner that delegates to the
  kernel's build system.
- **EXAMPLE** (bad):
  ```make
  # module/Makefile
  all: mymod.ko
  mymod.o: mymod.c
      cc -I/usr/src/linux-headers/include -c mymod.c -o mymod.o
  ```
- **COUNTEREXAMPLE** (good):
  ```make
  # module/Makefile (Kbuild)
  obj-m := mymod.o
  ```
  ```make
  # module/build.sh (or a driver makefile)
  KDIR ?= /lib/modules/$(shell uname -r)/build
  all:
      $(MAKE) -C $(KDIR) M=$(PWD) modules
  clean:
      $(MAKE) -C $(KDIR) M=$(PWD) clean
  ```
- **VERIFICATION**: `make -C $(KDIR) M=$PWD modules` produces `mymod.ko`
  (and `modules.order`, `.mod` files); `modinfo mymod.ko` shows `vermagic`.
- **SOURCE**: kernel-kbuild (ch. "Building external modules").

## 2. The build tree must be the TARGET kernel's headers

- **RULE**: the `KDIR` used by `make -C` must point at the build tree of the
  kernel the module will LOAD on: `/lib/modules/<target-version>/build`. It is
  the headers + `Module.symvers` + generated `vmlinux.h`/`autoconf.h` of that
  kernel that bind the module to it.
- **WHY AI GETS IT WRONG**: the model builds against whatever headers exist
  locally (distro headers, a different version) and ships the .ko; the Codex
  macbook12 wifi PR #5 class of failure — the driver built on the dev kernel
  but the target ran another version and the module refused to load.
- **CORRECT REASONING**: the module's ABI (struct layouts, syscall numbers,
  `struct file_operations` signature) is defined by the target kernel's
  headers. `uname -r` in the Makefile resolves the *local* kernel, which is the
  right target ONLY when building for the local host. For a cross-target build,
  `KDIR` must point at the target kernel's build tree.
- **EXAMPLE** (bad): hardcoding `/usr/src/linux-headers-6.0.0` in the Makefile
  and loading the module on a 6.8 kernel.
- **COUNTEREXAMPLE** (good):
  ```make
  KDIR ?= /lib/modules/$(shell uname -r)/build
  # or, explicitly for another target:
  # KDIR ?= /path/to/kernel-6.6/build
  ```
- **VERIFICATION**: `modinfo *.ko | grep vermagic` must contain the target
  kernel's version string; loading on a mismatched kernel fails at insmod.
- **SOURCE**: kernel-kbuild (vermagic, M= builds); empirical (Codex macbook12
  PR#5 2025).

## 3. MODULE_LICENSE gates symbol availability (GPL-only exports)

- **RULE**: `MODULE_LICENSE("GPL")` (and GPL-compatible variants) must be
  present for drivers using `EXPORT_SYMBOL_GPL` symbols. The license tag also
  drives the taint flag and the modpost license checks.
- **WHY AI GETS IT WRONG**: agents omit the tag ("it's just a comment") or use
  `MODULE_LICENSE("Proprietary")` for a module that calls GPL-only APIs,
  producing unresolved symbols at link time or a tainted kernel at load.
- **CORRECT REASONING**: `EXPORT_SYMBOL_GPL(sym)` restricts resolution to
  modules whose license modpost deems GPL-compatible. An out-of-tree module
  that links `EXPORT_SYMBOL_GPL` symbols needs `MODULE_LICENSE("GPL")` or a
  dual-license string; omitting the tag makes the symbols unresolved
  ("unknown symbol in module" at load) even though the module "compiled".
- **EXAMPLE** (bad): a driver calling GPL-only functions with no
  `MODULE_LICENSE` at all.
- **COUNTEREXAMPLE** (good):
  ```c
  MODULE_LICENSE("GPL");
  MODULE_AUTHOR("...");
  MODULE_DESCRIPTION("...");
  ```
- **VERIFICATION**: `modinfo *.ko` lists the license; `insmod` with
  `dmesg | tail` shows no "kernel tainted" / unresolved GPL symbols.
- **SOURCE**: kernel-driver-api (module license); kernel-kbuild (modpost).

## 4. Version binding: never suppress the vermagic check

- **RULE**: the module's vermagic must match the target kernel's. When they
  differ, `insmod` fails with `version magic '6.6.0-...' should be '6.8.0-...'`.
  The correct fix is building against the target; suppressing the check is a
  latent misload (can boot with wrong ABI).
- **WHY AI GETS IT WRONG**: agents interpret the version-magic error as a
  build defect and "fix" it by stripping `MODULE_INFO(vermagic)` or adding
  `CONFIG_MODULE_FORCE_LOAD`, which hides the mismatch instead of removing it.
- **CORRECT REASONING**: vermagic is a *correctness signal*: it encodes
  SMP/preempt/config flags and the kernel version. A forced load of a
  mismatched module can reference structures at the wrong offsets. The only
  legitimate path is: point `KDIR` at the target kernel's build tree, rebuild,
  verify vermagic matches.
- **EXAMPLE** (bad): adding `make ... MODULE_FORCE_LOAD=1` or patching out the
  vermagic to silence the error.
- **COUNTEREXAMPLE** (good):
  ```
  make -C /lib/modules/$(target-release)/build M=$PWD modules
  modinfo *.ko | grep vermagic   # shows target-release
  ```
- **VERIFICATION**: `insmod` succeeds with a matching vermagic; `modinfo`
  comparison against `/proc/version` / `uname -r` of the target.
- **SOURCE**: kernel-kbuild (module versioning, vermagic).

## 5. Deprecated flags: ccflags-y, not EXTRA_CFLAGS

- **RULE**: per-file/per-module compiler flags use `ccflags-y`, `cflags-y`,
  or `CFLAGS_<obj>.o`. `EXTRA_CFLAGS` is a long-deprecated alias.
- **WHY AI GETS IT WRONG**: older documentation and examples still show
  `EXTRA_CFLAGS`; agents copy it and the build silently ignores intended
  defines/includes, or warns about the deprecation.
- **CORRECT REASONING**: Kbuild's flag variables are namespaced
  (`ccflags-y`, `ldflags-y`, `cppflags-y`, `asflags-y`) and per-file
  `CFLAGS_<name>.o`. Using a deprecated variable is at best a warning and at
  worst ignored entirely, so the compile does not get the flags the module
  needs.
- **EXAMPLE** (bad):
  ```make
  EXTRA_CFLAGS += -DDEBUG -I$(src)/include
  ```
- **COUNTEREXAMPLE** (good):
  ```make
  ccflags-y += -DDEBUG -I$(src)/include
  ```
- **VERIFICATION**: `make V=1` shows the flag in the compile line; no
  deprecation warning in the build log.
- **SOURCE**: kernel-kbuild (variables chapter).

## 6. Building for one kernel and shipping to another is the classic AI failure

- **RULE**: before shipping a built .ko, verify the target kernel version and
  the build tree it was compiled against. A "build succeeded" outcome is NOT a
  guarantee the module loads on the deployment kernel.
- **WHY AI GETS IT WRONG**: CI passes on the dev host, so the model reports
  success; the load failure happens only on the user's machine — the worst
  failure mode ("compiles, silently does nothing / refuses to load").
- **CORRECT REASONING**: treat "insmod succeeds on the target" as the only
  meaningful success criterion. Track the build provenance: which kernel
  version, which build dir, which config (`/proc/config.gz`). Pin
  `KDIR` explicitly rather than trusting `uname -r` on the build host.
- **EXAMPLE** (bad): CI builds with the distro's 6.0 headers; the user's host
  runs 6.8; insmod fails; the model cannot reproduce because it never builds
  against 6.8.
- **COUNTEREXAMPLE** (good): CI matrix builds against every supported kernel
  release and insmods (or at least `modinfo`-verifies) each artifact.
- **VERIFICATION**: build + `insmod` on the target, `dmesg` clean; or at
  minimum `modinfo` vermagic vs target `uname -r`.
- **SOURCE**: kernel-kbuild; empirical (Codex macbook12 PR#5 2025).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Build pattern | Kbuild two-file: `obj-m` Makefile + `make -C KDIR M=$PWD` |
| Headers | `KDIR` = target kernel's build tree, not arbitrary headers |
| License | `MODULE_LICENSE("GPL")` for GPL-only symbol resolution |
| Vermagic | never suppress; rebuild against the target instead |
| Flags | `ccflags-y` / `CFLAGS_<o>`, never `EXTRA_CFLAGS` |
| Shipping | load on target is the only valid success criterion |
