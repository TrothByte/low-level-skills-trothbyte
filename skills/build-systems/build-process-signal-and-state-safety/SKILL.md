---
name: build-process-signal-and-state-safety
description: Use when a build may not have actually run or succeeded: signals killing ninja/make mid-write, corrupted .ninja_deps, sandbox no-ops that exit 0, ignored build exit codes. Teaches exit-code checks, output-change verification, ninja state inspection/repair, and write-only-after-success discipline.
---

# Build Process Signal & State Safety

## When to use

- A build "succeeded" but the outputs are missing, stale, or untrustworthy.
- A build was interrupted (SIGINT/SIGTERM/kill) and the next run recompiles
  everything or behaves flakily.
- A wrapper or sandbox around cmake/ninja exits 0 without printing any work.
- You must prove a build actually ran and that its outputs changed.
- `.ninja_deps` / `.ninja_log` corruption is suspected after a crash.

## When not to use

- Configure-time CMake errors or link-stage failures — use
  `build-system-cmake-diagnostics` / `build-linker-error-diagnostics`.
- Toolchain or `-std=` drift — use `build-toolchain-version-drift`.
- General crash-consistency of application data files; this skill covers build
  state and artifacts only.

## What the agent often gets wrong

- Treats "command ran" as "command succeeded": the exit code is never captured,
  or a later `echo`/pipe overwrites `$?` before it is read.
- Reports success after a no-op: the sandbox returned 0, no compiler ran, no
  output changed (empirical claude-code#38211).
- Believes ninja state survives a hard kill. SIGTERM mid-`fwrite` can truncate
  `.ninja_deps`, so the next build recompiles affected edges (empirical
  claude-code#49233).
- On Windows reads PowerShell `$?` instead of the native `$LASTEXITCODE` and
  misses cmake/ninja failures (empirical codex#14453); Bazel fetch-403 gets
  masked as a retryable network error (empirical bazel#77610).
- "Repairs" corrupt state by deleting build dirs and re-configuring instead of
  inspecting (`-t deps`) and repairing (`-t recompact`) ninja's own state.
- Writes artifact/state files in place, so an interrupt mid-write leaves a
  truncated file that passes existence checks.

## How to reason correctly

1. Ground truth = the exit code of the last build command, captured in the same
   statement (`ninja; rc=$?`, PowerShell `$LASTEXITCODE`) before anything else.
2. Exit 0 is necessary, not sufficient. A real run printed `[N/M]` lines and
   changed outputs; a truthful up-to-date run printed `ninja: no work to do.`
   A silent exit 0 with unchanged outputs means the build never ran.
3. Ninja state = `build.ninja` (graph) + `.ninja_deps` (deps log) +
   `.ninja_log` (command log). Interruption can truncate the logs; ninja then
   treats deps as missing and recompiles affected edges.
4. Inspect before you trust, repair before you rebuild-all:
   `ninja -t deps` → `ninja -t graph` → `ninja -t commands` → `ninja -t
   recompact` → `ninja -n` (only truly dirty edges remain).
5. Write outputs only after the build succeeded, and write state files
   atomically (temp file + rename), never in place.

## What to verify

- The real exit code of the last build command, read immediately.
- Outputs exist AND changed (mtime/size/content) when the build claimed to
  compile something.
- A claimed no-op: `ninja -n` says `no work to do`, deps show
  `#deps N ... (VALID)`, outputs intact.
- `ninja -t deps <output>` reports healthy entries, not `deps not found`.
- `ninja -t recompact` exits 0 and deps stay VALID afterwards.
- Wrappers/sandboxes forward real exit codes — no `> /dev/null` on the build,
  no trailing `exit 0`, no swallowed stderr.

## How to verify

```
ninja                          # real run; watch [N/M] lines and exit code
echo $?                        # POSIX; PowerShell: $LASTEXITCODE — read fast
ninja -t deps main.o           # deps log state; "deps not found" = lost deps
ninja -t graph                 # graphviz edges — who really depends on what
ninja -t commands main.o       # exact command lines ninja would run
ninja -t recompact             # rewrite internal logs compactly; exit 0
ninja -n                       # dry run: only truly-dirty edges rebuild
stat -c '%y %s' main.o         # mtime and size (PowerShell: Get-Item)
cmp old.bin new.bin            # content changed, not just touch
```

For make: same exit-code and output-change checks; fatal-signal target deletion
and `.DELETE_ON_ERROR` are documented in make-manual, not replayed here (no
POSIX make on this host).

## Where the knowledge comes from

- `ninja-manual` — `.ninja_deps`/`.ninja_log`, `recompact`, `deps`, `graph`,
  `commands`, `deps = gcc` needing an explicit `depfile`.
- `make-manual` — fatal-signal target deletion, `.DELETE_ON_ERROR`.
- `empirical (claude-code#49233)` — killed build corrupts deps state.
- `empirical (claude-code#38211)` — no-op sandbox reported as success.
- `empirical (codex#14453)` — Windows build exit codes ignored.
- `empirical (bazel#77610)` — fetch-403 hidden by a wrapper.

## Related skills

- `build-system-cmake-diagnostics` — configure-time graph and target inspection
- `build-linker-error-diagnostics` — diagnosis once the build genuinely fails
- `build-toolchain-version-drift` — proving which compiler and flags actually ran

## Evaluation

- Synthetic: the four `bad/` fixtures must be caught (ignored exit code,
  no-op sandbox, kill-mid-write, in-place state write); the `good/` fixtures
  must pass the exit-code + output-change + state checks.
- False-positive: a truthful `ninja: no work to do.` with VALID deps and intact
  outputs is NOT a no-op sandbox; a VALID deps entry is health, not corruption.
- Historical: replay "I killed the build, now everything rebuilds" — the correct
  fix is `ninja -t deps` + `ninja -t recompact` + targeted rebuild, not wiping
  build state.
- Adversarial: a plausible wrapper that swallows cmake/ninja and exits 0 must be
  detected (no log, unchanged outputs, `>/dev/null` + `exit 0` shape) and any
  downstream claim rejected.
- Researched status, recorded ninja 1.13.2 runs, and verified facts:
  `evals/README.md`.
