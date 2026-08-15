---
name: kernel-api-drift-migration
description: Use when porting or reviewing kernel code across kernel versions: sys_call_table unexport, DRM fbdev-to-client_setup, IIO migrations, symbol availability via kallsyms, and API signature changes. Prevents code that compiles but silently does nothing after an API disappears, and pins kernel versions before claiming compatibility.
---

# Kernel API Drift and Migration

## When to use

- Porting a driver or module to a newer kernel where an API may have changed
  name, signature, or export visibility.
- Reviewing code that hooks or patches kernel internals (e.g. `sys_call_table`,
  ftrace/BPF-based hooks, exported symbols) for the kernel version it targets.
- Checking whether a symbol is actually exported before using it
  (`kallsyms`, `Module.symvers`, `EXPORT_SYMBOL`).
- Migrating across a known breaking change: DRM fbdev emulation → DRM
  client-based setup, IIO driver API reshuffles, `vfs`/`proc` changes.
- Reviewing LLM-generated driver patches, which frequently target the wrong
  kernel API generation.

## When not to use

- Out-of-tree build mechanics (KDIR, vermagic) — use
  `kernel-module-build-out-of-tree`.
- Character device lifecycle contracts — use
  `kernel-driver-char-device-lifecycle`.
- Kernel bugs where the API exists and works but the driver misuses it —
  those are driver-logic skills, not drift.
- Userspace library API churn (glibc/ABI) — different rules, different
  verification.

## What the agent often gets wrong

- Assumes a symbol that existed in kernel 6.5 still exists in 6.9+
  (`sys_call_table` stopped being exported; a hook using it builds fine but
  silently does nothing at runtime). A 2024 StackOverflow report documents
  exactly this: the hook compiled, loaded, but never fired.
- Treats "compiles" as "works": `sys_call_table` removal is undetected by the
  compiler because the code accesses it via a declared `extern` table, so the
  failure is a NULL/zero table access or silent no-op, not an error.
- Applies migration recipes from memory without pinning the exact version:
  DRM fbdev→client_setup, IIO renames, `proc` handler changes are version-gated;
  a recipe for 6.10 can be wrong for 6.8 and 6.12.
- Ignores `EXPORT_SYMBOL_GPL` vs `EXPORT_SYMBOL` distinction when checking
  availability — a symbol exported GPL-only resolves only for GPL modules.
- Writes code against a future/older API than the pinned kernel and then
  "verifies" by compiling against the current dev headers, not the target.

## How to reason correctly

1. Pin the target kernel version FIRST: `uname -r` of the target, or the exact
   upstream tag. Every API question is answered relative to that version.
2. Check symbol availability, not just presence in headers: consult
   `System.map`/`/proc/kallsyms` of the target or the source tree's
   `Module.symvers`/`EXPORT_SYMBOL*` declarations. An `extern` declaration that
   compiles tells you nothing about runtime availability.
3. Classify the drift: (a) symbol unexported → code compiles, hook silently
   dead; (b) signature change → compile error or ABI mismatch; (c) semantic
   change → compiles AND runs but behaves differently. Each has a different
   detection method.
4. For the "silently dead" class, the worst outcome, add a runtime check: verify
   the resolved symbol address is non-NULL (kallsyms lookup) before registering
   a hook, or use `IS_ERR`/availability checks the API provides.
5. For migration tasks, diff against the actual upstream commit that changed the
   API (e.g. DRM's fbdev→client_setup conversion patches, IIO driver reworks)
   rather than reconstructing from memory.

## What to verify

- The target kernel version is pinned and recorded in the code/README.
- Every externally-referenced symbol is verified exported in the target:
  `grep <symbol> /lib/modules/$(uname -r)/build/Module.symvers` or
  `/proc/kallsyms`.
- `sys_call_table`-style hooks are either replaced with the supported mechanism
  (ftrace/BPF) or guarded by a runtime availability check; a silently-dead hook
  is unacceptable.
- Migration patches match the version-gated API of the target (check
  `#if LINUX_VERSION_CODE >= KERNEL_VERSION(...)` where needed).
- The module is compiled against the target kernel's headers (see
  `kernel-module-build-out-of-tree`), and loaded+exercised on the target.

## How to verify

```
# On the target Linux host (documented target commands, not this host):
grep sys_call_table /proc/kallsyms          # empty in 6.9+ → unexported
grep '^000.*' /lib/modules/$(uname -r)/build/Module.symvers | grep <symbol>
insmod myhook.ko && dmesg | tail            # verify the hook actually fires

# compile against the pinned kernel:
make -C /lib/modules/$(uname -r)/build M=$PWD modules
```

On this host the verification is done by static review + host-run checks of the
guarded-hook pattern (see `evals/README.md` for the honest status).

## Where the knowledge comes from

- `kernel-source` — the authoritative version-gated API definitions
  (`include/linux/syscall_table.h` history, DRM/IIO tree state per tag).
- `kernel-driver-api` — documented export/export-gpl conventions and available
  hooks per stable kernel.

## Related skills

- `kernel-module-build-out-of-tree` — building against the pinned kernel.
- `kernel-driver-char-device-lifecycle` — runtime contracts of the built module.
- `ebpf-verifier-reasoning` — the supported alternative for syscall hooks.

## Evaluation

- Synthetic: flag a `sys_call_table` hook without an availability check on a
  6.9+ target; flag unverified `EXPORT_SYMBOL_GPL` usage; approve the
  guarded-hook or ftrace/BPF pattern.
- False-positive: a correctly guarded kallsyms lookup with a non-NULL check and
  fallback must NOT be flagged; a driver whose target version is pinned and
  whose symbols are verified must be approved.
- Historical: the 2024 StackOverflow `sys_call_table` silent-hook report — the
  agent must recognize "compiles but never fires" and require a runtime check.
- Adversarial: a DRM fbdev→client_setup patch that compiles on both 6.8 and
  6.12 but behaves differently — the agent must pin the version and check the
  API against that tag.
- Verified facts and commands: `evals/README.md`.
