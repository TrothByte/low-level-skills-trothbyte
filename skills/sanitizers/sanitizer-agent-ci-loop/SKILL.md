---
name: sanitizer-agent-ci-loop
description: Use when integrating sanitizers (ASan/UBSan/TSan/MSan) into an agent's build-and-test loop for C/C++/Rust — so every change is automatically checked, reports are parsed and deduplicated, and regressions are caught. Fills the gap where "how to use sanitizers" exists but the universal agent loop does not.
---

# Sanitizer Integration in the Agent CI Loop

## When to use

- Writing or modifying C/C++/Rust code where memory safety matters.
- Setting up a verification harness for an agent working on a codebase.
- Diagnosing why a sanitizer "found nothing" (it may not have run!).
- Adding regression tracking to sanitizer findings.

## When not to use

- Reading/explaining a single sanitizer report — use `sanitizer-report-reading`.
- Writing the buggy code in the first place — use `safe-low-level-from-scratch`.

## What the agent often gets wrong

- Runs sanitizers only at the end (or not at all) — B5/B22.
- Sanitizer flags inconsistent with the build (issue #181): ASan build but fuzzer without
  `-rss_limit_mb=0`/`-m none` → fuzzer silently dies → false "no bugs".
- Treats a "clean" run as proof when the sanitizer didn't actually exercise the code path.
- Doesn't deduplicate reports (same bug reported 50 times) — drowns the signal.
- No regression tracking: a finding fixed today silently returns.

## How to reason correctly (the loop)

1. **Build**: compile with the sanitizer flags in every build script (not ad-hoc).
2. **Run**: run the test suite under the sanitizer; run fuzzers with the correct
   `-m none`/`-rss_limit_mb=0` so they don't silently die (issue #181).
3. **Parse**: extract category/file/line from reports; deduplicate (Z3 pattern: SQLite
   findings DB with trend queries).
4. **Fix & re-verify**: fix the root cause, re-run, confirm the specific report disappears.
5. **Track**: store findings so regressions are detected (`--last 10` style trend query).

## What to verify

- The sanitizer ACTUALLY ran (exit status, coverage, no silent-death flags).
- Reports are deduplicated by (category, file, line).
- A fix removes the report AND the code path still executes (no test-suite regression).
- The loop is part of every change, not a one-off.

## How to verify

```
# build with sanitizers every time
clang -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -o prog prog.c
./prog                        # ASan catches on run
# fuzzing with correct flags (issue #181)
./fuzzer -rss_limit_mb=0 -max_total_time=60 corpus/   # must NOT silently die
# dedupe via tools/eval/sanitizer_parse.py
```

## Where the knowledge comes from

- ASan/UBSan/TSan/MSan documentation; trailofbits testing-handbook-skills;
  Z3Prover/z3 `.github/skills/memory-safety` (SQLite findings pattern);
  trailofbits issue #181 (cross-skill ASan flag omission).

## Related skills

- `sanitizer-report-reading` (require)
- `meta-verification` (require — the loop is the verification gate)
- `safe-low-level-from-scratch` (recommend)

## Evaluation

Adversarial AD-12: a fuzzer that silently dies because `-m none` is missing — the agent must
detect the silent death and fix the flags, not conclude "no bugs". Historical: run the
CVE-2022-3602 fixture under the loop — must surface the off-by-one.
