# Evaluation — build-process-signal-and-state-safety

Skill: `skills/build-systems/build-process-signal-and-state-safety`.
Host: Windows (PowerShell 5.1), ninja 1.13.2, GCC 16.1.0 (MSYS2 ucrt64).
`make` is NOT installed (only `mingw32-make`); POSIX make signal semantics are
documented from make-manual, not replayed. All commands recorded 2026-08-15.

## Researched status

- The four incident reports (claude-code#49233, claude-code#38211,
  codex#14453, bazel#77610) are empirical and cited from the task brief; their
  issue threads were NOT accessible from this environment, so each is marked
  UNVERIFIED as an incident. The mechanism each incident teaches is locally
  verified where marked below.
- The ninja state mechanics are partial source-backed: ninja-manual (official
  docs) PLUS real runs on ninja 1.13.2 recorded below.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/ignore_exit_code.sh` | exit code never captured; stale artifact used | review |
| easy/negative | `bad/noop_sandbox.sh` | exits 0, no output changed — must be flagged no-op | review |
| medium/negative | `bad/kill_mid_write.sh` | SIGTERM mid-write, then "clean" claim | review |
| medium/negative | `bad/partial_write.c` | in-place write; truncated state possible | `gcc -c` exit 0, must flag |
| easy/positive | `good/check_exit_and_outputs.sh` | captures rc, verifies artifact mtime | exit-code gate |
| easy/positive | `good/recompact_repair.sh` | recompact then `-t deps`/`-n` confirm | exit 0 |
| easy/positive | `good/atomic_write.c` | temp file + rename | `gcc -c` exit 0 |
| easy/positive | `good/build.ninja` + `hello.c` | real build on ninja 1.13.2 | exit 0, recorded |

## Actual verification runs (recorded 2026-08-15)

```
ninja --version
  1.13.2

# tiny real project = examples/good/build.ninja + hello.c (run from a temp copy)
ninja -C <demo>
  ninja: Entering directory `<demo>'
  [1/2] gcc -MMD -MF hello.o.d -c hello.c -o hello.o
  [2/2] gcc hello.o -o hello.exe
  exit 0

ninja -C <demo>                       # second run: the truthful no-op baseline
  ninja: no work to do.
  exit 0

ninja -C <demo> -t commands hello.exe
  gcc -MMD -MF hello.o.d -c hello.c -o hello.o
  gcc hello.o -o hello.exe
  exit 0

ninja -C <demo> -t deps hello.o
  hello.o: #deps 1, deps mtime 8085095185771909 (VALID)
      hello.c
  exit 0

ninja -C <demo> -t graph hello.exe    (graphviz)
  digraph ninja { rankdir="LR" ... "hello.c" -> "hello.o" [label=" cc"]
                  "hello.o" -> "hello.exe" [label=" link"] }
  exit 0

ninja -C <demo> -t recompact
  (no output) exit 0

ninja -C <demo> -t deps hello.o       # after recompact
  hello.o: #deps 1, deps mtime 8085095185771909 (VALID)
      hello.c
  exit 0

# simulated killed-mid-write state: .ninja_deps emptied / zero-filled
ninja -C <demo> -t deps hello.o
  hello.o: deps not found
  exit 0

ninja -C <demo>                       # deps lost -> edge recompiled
  ninja: Entering directory `<demo>'
  [1/2] gcc -MMD -MF hello.o.d -c hello.c -o hello.o
  [2/2] gcc hello.o -o hello.exe
  exit 0

gcc -c does_not_exist.c 2>$null
  $LASTEXITCODE = 1

# failed edge (recorded via the deps=gcc-without-depfile misconfig):
#   FAILED: [code=1] hello.o
#   gcc -c hello.c -o hello.o
#   edge with deps=gcc but no depfile makes no sense
#   ninja: build stopped: subcommand failed.
#   exit 1
```

## Windows-specific finding (recorded)

With `deps = gcc`, ninja 1.13.2 on this host rejects a rule with no `depfile`:
`edge with deps=gcc but no depfile makes no sense` (exit 1). The rule must set
`depfile = $out.d` and the command must emit it (`gcc -MMD -MF $out.d`). The
common recipe that relies on ninja auto-adding `-MD` flags does NOT work on
this build — recorded, do not rely on it.

## Verified facts

- `ninja -t deps <output>` reports `#deps N ... (VALID)` for healthy entries
  and `deps not found` when the log lost the entry. KNOWN (recorded).
- Empty or garbage `.ninja_deps` makes ninja 1.13.2 recompile the affected
  edge on the next run (exit 0 overall); it does not hard-fail on this version.
  KNOWN (recorded). Other versions may error instead — INFERRED.
- `ninja -t recompact` exits 0 (prints nothing) and preserves healthy deps log
  entries (still VALID afterwards). KNOWN (recorded).
- `ninja -t recompact` does NOT restore entries already lost to a truncated
  log: after truncation, `deps not found` persisted until a real rebuild.
  KNOWN (recorded).
- `ninja -t graph` emits a valid graphviz digraph; `-t commands` prints the
  exact command lines; a second `ninja` prints `no work to do.` — the
  truthful-no-op baseline. KNOWN (recorded).
- Native tool exit codes land in PowerShell `$LASTEXITCODE` (`gcc -c
  does_not_exist.c` → 1; failed ninja edge → 1), not in `$?`. KNOWN (recorded).
- Ninja 1.13.2 on Windows requires `depfile` when `deps = gcc`. KNOWN
  (recorded).
- SIGTERM mid-`fwrite` corrupting `.ninja_deps` (claude-code#49233), the
  no-op sandbox (claude-code#38211), ignored Windows cmake exit codes
  (codex#14453), and Bazel fetch-403 masking (bazel#77610) are incident reports
  not accessible here — UNVERIFIED as incidents; the mechanisms taught are
  independently verified above where stated.
- make fatal-signal target deletion and `.DELETE_ON_ERROR`: make-manual
  (documented), not replayed here (no POSIX make on this host). KNOWN
  (documented source), UNVERIFIED (not run).
- POSIX `rename()` atomicity for the temp-file+rename pattern: documented
  guarantee, not re-run — INFERRED for this skill's verification.

## False-positive evals (correct behavior must not be flagged)

- A genuine `ninja: no work to do.` after a real prior build is up-to-date,
  NOT a no-op sandbox — distinguish by VALID deps and intact outputs.
- `ninja -t deps` showing `#deps ... (VALID)` is healthy state, not corruption.
- `ninja -t recompact` printing nothing with exit 0 is success.
- Atomic write via temp file + rename is correct and must not be flagged as
  "extra work".

## Historical evals

- "I killed the build, now it rebuilds everything" (claude-code#49233 pattern):
  correct output is `ninja -t deps` (find `deps not found`) + `ninja -t
  recompact` + targeted rebuild — NOT `rm -rf build` + reconfigure.
- "Sandbox reported the build succeeded" (claude-code#38211 pattern): correct
  output is "no compile log + outputs unchanged + exit 0 = build never ran",
  not "build ok, proceed".
- Windows agent ignores a `cmake` failure (codex#14453 pattern): correct output
  captures `$LASTEXITCODE` and stops the session.

## Adversarial evals

- `bad/noop_sandbox.sh`: a plausible wrapper that swallows cmake/ninja and
  exits 0. Detect via absent `[N/M]` log lines, unchanged artifact mtime, and
  the `>/dev/null` + `exit 0` shape. Reject any downstream claim.
- `bad/kill_mid_write.sh`: an "interrupted" build treated as clean. Reject the
  clean claim; expect dirty deps (`deps not found`).
- `bad/partial_write.c`: a state writer that passes existence checks but
  truncates on interrupt. Catch by demanding temp+rename.

## Scoring (for routing eval)

- precision: every flag maps to a named reference rule (1-8).
- recall: ignored exit codes, no-op sandboxes, interrupted-build state, corrupt
  deps logs, PowerShell `$?` misuse, in-place state writes, and missing
  `.DELETE_ON_ERROR` are all caught.
- FP-rate: real successful builds, truthful `no work to do`, and healthy deps
  logs produce zero flags.

## Target toolchains (absent, documented)

- POSIX `make` (GNU make): not installed here (`mingw32-make` only).
  make-manual signal rules are documented; verification commands for a make
  host are listed in references rule 8.
- `bazel`: not installed. The fetch-403 masking pattern (bazel#77610) is taught
  as a network-fetch failure a wrapper can hide, not replayed.
