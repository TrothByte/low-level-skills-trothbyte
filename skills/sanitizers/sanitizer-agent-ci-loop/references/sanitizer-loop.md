# Sanitizer Agent CI Loop — Reference

Sources: ASan/UBSan/TSan/MSan docs; Z3Prover/z3 memory-safety skill (SQLite findings);
trailofbits issue #181. Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. Sanitizers run on every change, in the build script

- **RULE**: the agent's build step compiles with sanitizer flags; the loop is not optional.
- **WHY AI GETS IT WRONG**: sanitizers are treated as an afterthought (B5).
- **CORRECT REASONING**: one build+test command that runs sanitizers every time catches bugs
  at the change, not at review.
- **EXAMPLE** (good): build script: `clang -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`.
- **COUNTEREXAMPLE** (bad): `gcc -O2` plain build; sanitizers never run.
- **VERIFICATION**: every CI/log line shows the sanitizer flags.
- **SOURCE**: ASan docs; Z3 memory-safety skill.

## 2. Fuzzer flags must prevent silent death (issue #181)

- **RULE**: ASan-instrumented fuzzers need `-rss_limit_mb=0` (libFuzzer) or `-m none` (AFL++)
  so they don't die quietly on ASan's memory usage; a silently dead fuzzer is a false "clean".
- **WHY AI GETS IT WRONG**: compiles with `-fsanitize=address` but runs the fuzzer without
  the memory-limit flags (the exact trailofbits issue #181 scenario).
- **CORRECT REASONING**: ASan inflates memory; libFuzzer's default RSS limit kills it early
  and quietly. The operator then concludes "no bugs" when no fuzzing occurred.
- **EXAMPLE** (good): `./fuzzer -rss_limit_mb=0 -max_total_time=60 corpus/`.
- **COUNTEREXAMPLE** (bad): `./fuzzer corpus/` — may die immediately, silently.
- **VERIFICATION**: check the fuzzer actually executed (coverage stats, corpus growth).
- **SOURCE**: trailofbits issue #181; libFuzzer docs.

## 3. Deduplicate reports by (category, file, line)

- **RULE**: parse sanitizer output into (category, file, line) and deduplicate; count distinct
  findings, not report lines.
- **WHY AI GETS IT WRONG**: pastes raw output; the same UAF reported 50 times "looks worse".
- **CORRECT REASONING**: a triage table of distinct findings guides fixing order (null-deref
  first, then UAF — Z3 prioritization).
- **EXAMPLE**: `tools/eval/sanitizer_parse.py` emits deduped findings.
- **VERIFICATION**: distinct-count == actual bug count.
- **SOURCE**: Z3 memory-safety skill (SQLite findings DB).

## 4. Regression tracking

- **RULE**: store findings (DB or file); re-check after fixes and on later changes so
  regressions are caught (Z3: `--last 10` trend queries).
- **WHY AI GETS IT WRONG**: fixes are verified once and forgotten; the bug silently returns.
- **CORRECT REASONING**: a findings ledger makes "fixed vs returned" measurable.
- **EXAMPLE**: `findings.db` with (category, file, line, status, date).
- **VERIFICATION**: a previously-fixed finding never reappears in fresh runs.
- **SOURCE**: Z3 memory-safety skill.

## 5. "Clean" means "ran and found nothing", provably

- **RULE**: a clean sanitizer run is only meaningful if the code path executed under the
  sanitizer with the right flags.
- **WHY AI GETS IT WRONG**: "ASan passed" reported without proof the sanitizer ran.
- **CORRECT REASONING**: record exit status, test names run, and (for fuzzers) coverage.
- **VERIFICATION**: logs contain the run evidence.
- **SOURCE**: meta-verification; registry/evals.yaml (AD-07).
