# Kernel API Drift — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Pin the target kernel version before any API question

- **RULE**: every kernel API question (does symbol X exist, what is its
  signature, is it exported) is answered relative to ONE kernel version. Pin it
  first, then answer.
- **WHY AI GETS IT WRONG**: agents answer "what is the signature of X?" from a
  blur of kernels they have seen, mixing 5.x and 6.x realities; a recipe that
  is right for 6.10 is applied to a 6.8 or 6.12 target.
- **CORRECT REASONING**: kernel APIs are version-gated by design
  (`#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,9,0)`, per-version headers).
  The target is `uname -r` of the deployment host or an exact upstream tag;
  record it in the code and in the review notes, and check every API against
  that version's source tree, not the current checkout.
- **EXAMPLE** (bad): reviewing a driver with "uses `drm_fb_helper` — that's
  fine" without stating the kernel version.
- **COUNTEREXAMPLE** (good): "Target: kernel 6.12. `drm_fb_helper` replaced by
  `drm_client_setup` in 6.12 — must migrate."
- **VERIFICATION**: state the tag; `git show <tag>:include/...` or
  `https://git.kernel.org/...?h=<tag>` to check the API at that exact tag.
- **SOURCE**: kernel-source (per-tag API state); kernel-driver-api.

## 2. An `extern` that compiles proves nothing about availability

- **RULE**: availability of a kernel symbol at runtime is determined by its
  EXPORT in the target kernel (`EXPORT_SYMBOL`/`EXPORT_SYMBOL_GPL`), visible in
  `Module.symvers` and `/proc/kallsyms`. A C `extern` declaration compiles
  regardless.
- **WHY AI GETS IT WRONG**: the model sees "it compiles" as evidence the API is
  available; for `sys_call_table` (removed from export in 6.9+) this is exactly
  the failure: the hook builds, loads, and does nothing.
- **CORRECT REASONING**: the compiler only checks that the declaration matches;
  the linker/module loader resolves against exported symbols. Check the target
  kernel's `Module.symvers` (source of truth for out-of-tree modules) and
  `/proc/kallsyms` (runtime truth). Distinguish `EXPORT_SYMBOL` from
  `EXPORT_SYMBOL_GPL`: the latter resolves only for GPL-licensed modules.
- **EXAMPLE** (bad): hooking `sys_call_table` via `extern void **sys_call_table`
  on 6.9+ — compiles, links, loads, never fires.
- **COUNTEREXAMPLE** (good): before using the symbol, resolve it with
  `kallsyms_lookup_name` (where available) or check
  `grep sys_call_table Module.symvers`; on 6.9+ it is absent — switch to
  ftrace/BPF trampolines.
- **VERIFICATION**: `grep <sym> /lib/modules/$(uname -r)/build/Module.symvers`;
  `cat /proc/kallsyms | grep <sym>` on the target; run the hook and observe it
  actually fire.
- **SOURCE**: kernel-source; empirical (StackOverflow 2024 sys_call_table).

## 3. Classify drift: unexported vs signature vs semantic

- **RULE**: three distinct failure classes with different detection:
  (a) symbol unexported — compiles, silently dead; (b) signature change —
  compile error or ABI mismatch; (c) semantic change — compiles and runs but
  behaves differently.
- **WHY AI GETS IT WRONG**: agents treat "compiles" as a single success signal
  and miss classes (a) and (c); the 2024 StackOverflow report is class (a), the
  worst because it is invisible.
- **CORRECT REASONING**: for (a) verify export state; for (b) compile against
  the pinned kernel's headers and compare the call's argument types to the new
  prototype; for (c) read the change notes/commit that altered behavior (e.g.
  DRM fbdev→client_setup is a semantic + API move, IIO renames are signature
  churn).
- **EXAMPLE** (bad): a 6.10-era DRM fbdev patch applied to 6.12 where the
  framebuffer path is gone — compiles via compat but no console.
- **COUNTEREXAMPLE** (good): migrate per the upstream conversion commit and
  test that the console/DRM client actually initializes on the target.
- **VERIFICATION**: compile against pinned headers; boot/test on target;
  runtime check that the expected side effect occurred (console appeared, IIO
  device enumerated).
- **SOURCE**: kernel-source (commit history); kernel-driver-api.

## 4. The "silently does nothing" outcome is the worst — require a runtime check

- **RULE**: any code that depends on a symbol/behavior whose availability is not
  guaranteed must carry a runtime availability check (non-NULL resolved address,
  feature flag, IS_ERR), and must fail loudly or disable itself when the API is
  missing.
- **WHY AI GETS IT WRONG**: models prefer "if it compiles it's done"; the
  silent hook is precisely the failure that ships to users with zero signal.
- **CORRECT REASONING**: the cost of a runtime check (one kallsyms lookup at
  init, one `if (ptr)` before use) is tiny compared to a dead feature that
  appears healthy. Design for "missing API" as a first-class state.
- **EXAMPLE** (bad): `hooks[SYSCALL_NR] = (void *)sys_call_table[nr];` with no
  NULL guard.
- **COUNTEREXAMPLE** (good):
  ```c
  void **tbl = resolve_syscall_table();   /* NULL when unexported */
  if (!tbl) {
      pr_warn("sys_call_table unavailable — hook disabled\n");
      return -ENOTSUPP;
  }
  ```
- **VERIFICATION**: run on a kernel without the API and confirm the module
  reports disabled rather than silently no-oping.
- **SOURCE**: kernel-driver-api; empirical (StackOverflow 2024).

## 5. Migrate by diffing the upstream conversion, not from memory

- **RULE**: for a known breaking migration (DRM fbdev→client_setup, IIO
  rework), port by diffing the upstream commit that converted other drivers;
  copy the conversion's structure, not a remembered recipe.
- **WHY AI GETS IT WRONG**: the model "remembers" a migration recipe that
  blends several kernel versions and produces a hybrid that compiles nowhere
  or runs wrong.
- **CORRECT REASONING**: upstream conversion commits show the exact new API
  usage, config symbol changes, and version gates for that driver generation.
  Diff your driver against the converted example and adapt names, then compile
  against the pinned version.
- **EXAMPLE** (bad): writing `drm_client_setup` code from memory on 6.10 where
  the call takes a different argument list than 6.12.
- **COUNTEREXAMPLE** (good): `git log --oneline -- drivers/gpu/drm/...` find the
  conversion commit, read its diff, port, compile at the pinned tag.
- **VERIFICATION**: compile + boot the migrated driver on the target; check the
  feature (console/framebuffer/IIO device) actually appears.
- **SOURCE**: kernel-source (commit history, stable tree).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Version pinning | answer every API question against ONE kernel version |
| Availability | export state (Module.symvers/kallsyms) decides, not `extern` |
| Drift classes | unexported / signature / semantic — different detection each |
| Silent failure | runtime check required; fail loudly when the API is gone |
| Migrations | port by diffing upstream conversion commits |
| Worst outcome | "compiles but silently does nothing" — always test the side effect |
