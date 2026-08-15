# Build Process Signal & State Safety — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → STATUS → SOURCE. Source ids refer to registry/sources.yaml.
STATUS is one of KNOWN (documented and/or recorded here), INFERRED (reasoned,
not directly verified), UNVERIFIED (incident not accessible).

## 1. The exit code of the last build command is the ground truth

- **RULE**: capture the exit code of THE BUILD COMMAND itself, immediately, in
  the same statement. POSIX: `ninja; rc=$?`; PowerShell: the native code is
  `$LASTEXITCODE`, and `$?` is not it.
- **WHY AI GETS IT WRONG**: reads `$?` after a later `echo`/`cat`/pipe that
  always succeeds; reads PowerShell `$?` (a boolean, usually True) instead of
  `$LASTEXITCODE`; lets a wrapper with `>/dev/null` and a trailing `exit 0`
  hide rc 127/1. Windows: the cmake exit code is ignored entirely and the
  session moves on (codex#14453).
- **CORRECT REASONING**: a native tool reports its own code: ninja exits 1 on a
  failed edge, 4 on stale/missing build file, 127 when not found. Every later
  command redefines `$?`; PowerShell sets `$LASTEXITCODE` from the last native
  program only.
- **EXAMPLE** (bad):
  ```sh
  ninja -C build
  echo "build ok"            # overwrites $? with 0
  test -f app                # runs anyway even if ninja failed
  ```
- **COUNTEREXAMPLE** (good):
  ```sh
  ninja -C build
  rc=$?
  [ "$rc" -ne 0 ] && { echo "build failed rc=$rc" >&2; exit "$rc"; }
  ```
- **VERIFICATION**: `gcc -c does_not_exist.c; echo $?` prints 1 (recorded).
  A failing ninja edge prints `FAILED: [code=1] ...`, `ninja: build stopped:
  subcommand failed.` and exits 1 (recorded, ninja 1.13.2).
- **STATUS**: KNOWN (recorded locally).
- **SOURCE**: empirical (codex#14453); ninja-manual.

## 2. Exit 0 is necessary, not sufficient: outputs must change

- **RULE**: success = (exit 0) AND (real work that changed outputs, OR a
  truthful `no work to do` with intact outputs). Verify mtime/size/content of
  the claimed artifacts.
- **WHY AI GETS IT WRONG**: trusts a wrapper's 0; never checks an artifact was
  produced or updated; accepts "printed nothing but exited 0" as a valid build
  (claude-code#38211); a Bazel fetch failing with HTTP 403 gets retried and
  hidden by a wrapper that still exits 0 (bazel#77610).
- **CORRECT REASONING**: a real ninja run prints `[N/M]` lines and bumps
  output mtimes; an up-to-date run prints `ninja: no work to do.` (both
  recorded). A no-op wrapper prints nothing, changes nothing, returns 0 — the
  tell is outputs that are missing or stale for what was claimed built.
- **EXAMPLE** (bad):
  ```sh
  cmake --build build >/dev/null 2>&1   # log gone
  ninja -C build >/dev/null 2>&1        # no-op: nothing compiled
  exit 0                                # caller sees success
  ```
- **COUNTEREXAMPLE** (good):
  ```sh
  ninja -C build 2>&1 | tee build.log   # keep the log
  rc=$?
  [ "$rc" -ne 0 ] && exit "$rc"
  test -f build/app || { echo "no app produced" >&2; exit 1; }
  stat -c '%Y:%s' build/app             # mtime:size changed since before
  ```
- **VERIFICATION**: `ninja` twice on the demo project — second run prints
  `ninja: no work to do.` (recorded); `test -f`/`stat` on the artifact.
- **STATUS**: KNOWN (recorded); the sandbox/fetch incidents are UNVERIFIED.
- **SOURCE**: empirical (claude-code#38211); empirical (bazel#77610);
  ninja-manual.

## 3. Killing the build mid-write corrupts persistent state

- **RULE**: ninja persists `.ninja_deps` (deps log) and `.ninja_log` (command
  log). A hard termination (SIGKILL, crash) during a write can leave them
  truncated or garbage; ninja then treats deps as unknown and recompiles the
  affected edges.
- **WHY AI GETS IT WRONG**: assumes a terminated build leaves state intact;
  blames the toolchain or "changed headers" for the sudden recompile;
  "fixes" it by deleting build dirs.
- **CORRECT REASONING**: the logs are memory + flush-on-exit; an interrupted
  flush loses entries. Recorded on ninja 1.13.2: emptying or zero-filling
  `.ninja_deps` makes `ninja -t deps` report `deps not found` and the next
  `ninja` recompiles hello.o and relinks hello.exe.
- **EXAMPLE** (bad):
  ```sh
  ninja -C build &
  pid=$!
  sleep 2
  kill -TERM "$pid"          # ninja dies mid-write of .ninja_deps
  wait "$pid"
  ninja -C build             # "why is everything rebuilding?"
  ```
- **COUNTEREXAMPLE** (good):
  ```sh
  ninja -C build
  rc=$?
  [ "$rc" -ne 0 ] && exit "$rc"
  ninja -t deps main.o       # confirm entries still (VALID)
  ```
- **VERIFICATION**: recorded (ninja 1.13.2): truncated/garbage `.ninja_deps`
  → `ninja -t deps` prints `hello.o: deps not found`; next `ninja` recompiles.
  The exact SIGTERM-mid-`fwrite` corruption incident is UNVERIFIED.
- **STATUS**: partial — repair mechanics KNOWN (manual + recorded); the
  SIGTERM corruption mode UNVERIFIED.
- **SOURCE**: ninja-manual; empirical (claude-code#49233).

## 4. Repair with `ninja -t recompact`, don't wipe the tree

- **RULE**: `ninja -t recompact` rewrites ninja's internal logs (deps log,
  command log) into a compact, consistent form. Run it when an interrupted
  build leaves state suspect, then confirm with `ninja -t deps`.
- **WHY AI GETS IT WRONG**: wipes `build/` and re-runs configure as the first
  "repair"; does not know `recompact` exists; cannot tell "deps log corrupt"
  from "dependency graph wrong".
- **CORRECT REASONING**: `recompact` is the manual-named tool for internal
  state. It is cheap and safe; afterwards `ninja -t deps <output>` should still
  report `#deps N ... (VALID)`. It does NOT restore entries already lost to a
  truncated log — those edges need one real rebuild (recorded).
- **EXAMPLE** (bad):
  ```sh
  rm -rf build && cmake -B build   # throws away the graph AND the cache
  ```
- **COUNTEREXAMPLE** (good):
  ```sh
  ninja -t recompact
  rc=$?
  [ "$rc" -ne 0 ] && exit "$rc"
  ninja -t deps main.o             # still #deps ... (VALID)
  ninja -n                         # dry-run: only truly-dirty edges remain
  ```
- **VERIFICATION**: recorded on the demo project: `ninja -t recompact` exits 0
  (prints nothing); `ninja -t deps hello.o` still prints
  `hello.o: #deps 1, deps mtime ... (VALID)`.
- **STATUS**: KNOWN (recorded, ninja 1.13.2).
- **SOURCE**: ninja-manual.

## 5. Inspect state with `-t deps`, `-t graph`, `-t commands`

- **RULE**: `ninja -t deps [output]` dumps the deps log; `ninja -t graph`
  emits graphviz of the real graph; `ninja -t commands [target]` prints the
  exact command lines ninja would run.
- **WHY AI GETS IT WRONG**: reads `build.ninja` text and guesses; asks "why
  did it rebuild?" without consulting the deps log; claims a command ran
  without checking `-t commands` shows it.
- **CORRECT REASONING**: the tools print what ninja actually recorded.
  `deps not found` = the log lost the entry (rebuild pending).
  `#deps N ... (VALID)` = entry healthy. `-t graph` shows the exact edges for
  "why is X a dependency of Y".
- **EXAMPLE** (bad): assuming `hello.o` depends on a header because the source
  includes it, without verifying the deps log recorded it.
- **COUNTEREXAMPLE** (good):
  ```sh
  ninja -t deps hello.o        # hello.o: #deps 1 ... (VALID) \n  hello.c
  ninja -t commands hello.exe  # gcc -MMD -MF hello.o.d -c hello.c -o hello.o
  ```
- **VERIFICATION**: recorded outputs on the demo project (shown in
  `evals/README.md`).
- **STATUS**: KNOWN (recorded).
- **SOURCE**: ninja-manual.

## 6. Windows: PowerShell exit codes are `$LASTEXITCODE`, not `$?`

- **RULE**: PowerShell surfaces a native tool's code in `$LASTEXITCODE`; `$?`
  is a boolean success flag that an intervening `Write-Output` flips to True.
  `cmake`/`ninja` failures must be read from `$LASTEXITCODE`.
- **WHY AI GETS IT WRONG**: checks `$?` after the build in PowerShell (almost
  always True); ignores `cmake`/`ninja` exit codes on Windows and reports
  success after a failed build (codex#14453).
- **CORRECT REASONING**: in PowerShell,
  `ninja -C build; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }`; or invoke
  through `cmd /c "ninja -C build"` which returns the native code directly.
- **EXAMPLE** (bad):
  ```powershell
  ninja -C build
  Write-Output "done"        # $? is now True from Write-Output
  ```
- **COUNTEREXAMPLE** (good):
  ```powershell
  ninja -C build
  if ($LASTEXITCODE -ne 0) { Write-Error "build failed"; exit $LASTEXITCODE }
  ```
- **VERIFICATION**: recorded: `gcc -c does_not_exist.c` → `$LASTEXITCODE` = 1;
  failed ninja edge exits 1.
- **STATUS**: KNOWN (recorded locally); the cmake incident is UNVERIFIED.
- **SOURCE**: empirical (codex#14453).

## 7. Write outputs only after success; write state atomically

- **RULE**: a tool that writes artifact/state files must (a) not emit its
  output until the inputs actually built, and (b) write state files via
  temp-file + rename, never in place, so a reader never sees partial bytes and
  an interrupted write leaves the old file intact.
- **WHY AI GETS IT WRONG**: writes the report/artifact before checking the
  build result; appends/overwrites a state file in place; an interrupted write
  then leaves a file that passes existence checks but is truncated.
- **CORRECT REASONING**: same discipline ninja applies to its own logs. Write
  `<file>.tmp`, flush, close, then rename over `<file>`; rename is atomic
  (POSIX) and MoveFileEx-equivalent on Windows. A crash leaves the temp file,
  never a partial main file.
- **EXAMPLE** (bad):
  ```c
  FILE *f = fopen(state_path, "wb");
  fwrite(data, 1, size, f);   /* kill mid-write -> partial file */
  fclose(f);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  FILE *f = fopen(tmp_path, "wb");
  fwrite(data, 1, size, f);
  fclose(f);
  rename(tmp_path, state_path);   /* atomic swap */
  ```
- **VERIFICATION**: compile both with `gcc -c`; interrupt-point analysis is
  the POSIX `rename` atomicity guarantee.
- **STATUS**: INFERRED (engineering practice; POSIX rename atomicity
  documented, not re-run here).
- **SOURCE**: ninja-manual; make-manual.

## 8. make: signals, partial targets, and `.DELETE_ON_ERROR`

- **RULE**: GNU make, on a fatal signal while a recipe is running, deletes the
  target file that recipe was updating and terminates; `.DELETE_ON_ERROR`
  deletes the target when a recipe returns non-zero. An interrupted build
  leaves earlier targets valid and the current one removed, so the next `make`
  resumes those edges instead of restarting from scratch.
- **WHY AI GETS IT WRONG**: assumes an interrupted make leaves arbitrary
  partial state; treats `make` exit 130 (SIGINT) as "still fine"; forgets
  `.DELETE_ON_ERROR` gives deterministic cleanup on recipe failure.
- **CORRECT REASONING**: make's model is per-target atomicity by deletion:
  killed mid-recipe → that target is deleted → next `make` rebuilds only that
  edge. So a "resume" is cheap and correct; a full clean rebuild is usually
  the wrong reaction to an interrupted make.
- **EXAMPLE** (bad):
  ```sh
  make && make install        # SIGINT mid-build -> partial target used by install
  ```
- **COUNTEREXAMPLE** (good):
  ```make
  .DELETE_ON_ERROR:
  ```
- **VERIFICATION**: POSIX `make` is not installed on this host (only
  `mingw32-make`); documented semantics from make-manual, not replayed.
  On a make host: `kill -INT <make pid>; make -n` → only the interrupted edge
  is dirty.
- **STATUS**: KNOWN (documented source); NOT replayed on this host.
- **SOURCE**: make-manual.

## Quick reference

| Topic | Rule in one line |
|---|---|
| Exit code | capture the build command's code before any other command; PowerShell: `$LASTEXITCODE` |
| Exit 0 | necessary only — outputs must exist and change, or `no work to do` must be truthful |
| `.ninja_deps` | kill mid-write truncates it → `deps not found` → recompile of edges |
| Repair | `ninja -t recompact` rewrites logs; does not restore already-lost entries |
| Inspect | `-t deps` (log), `-t graph` (edges), `-t commands` (real command lines) |
| Windows | `$?` is a boolean; read `$LASTEXITCODE` |
| Atomicity | temp file + rename, never in-place fwrite |
| make | fatal signal deletes the recipe's target; `.DELETE_ON_ERROR` for failures |
