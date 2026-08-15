---
name: fuzzing-harness-evidence-gate
description: Use when reporting or reviewing fuzzing results to decide whether a finding is evidence or noise. Enforces the proof standard: reproducible sanitizer report, minimized crashing input, demonstrated reachable path, before any bug or CVE claim. Covers libFuzzer and AFL++ harnesses.
---

# Fuzzing Harness Evidence Gate

## When to use

- Reporting a bug found by fuzzing and needing to know whether the evidence is
  sufficient.
- Writing or reviewing a libFuzzer/AFL++ harness and the build flags around it.
- Turning a raw crash artifact into a reproducible, minimized finding.
- Evaluating a "no findings" fuzzing result before claiming the target is clean.
- Deciding whether a CVE/security claim is defensible.

## When not to use

- Interpreting a sanitizer report you already hold — use `sanitizer-report-reading`.
- Integrating sanitizers into the build-and-test loop — use `sanitizer-agent-ci-loop`.
- Designing a structure-aware/grammar fuzzer or a custom mutator.
- Writing the fuzzing driver API in depth — this skill gates the OUTPUT of a
  fuzz run; the `LLVMFuzzerTestOneInput` contract is summarized here and detailed
  in `references/evidence-gate.md`.

## What the agent often gets wrong

- Claims "found a bug" from a single crash seen once, with no saved input and no
  reproducer command. An unreproducible crash is an observation, not a finding.
- Writes a harness that never reaches the vulnerable code: it fuzzes a wrapper,
  reads from stdin, or calls the wrong entry, so mutations never exercise the parser.
- Trusts length fields inside the fuzzed bytes instead of `Size`, so the crash
  fires in the harness, not in the target.
- Reports "no findings" without runtime, coverage, corpus, sanitizer set, or stated
  limitations — absence of evidence stated as proof.
- Ships a huge un-minimized crash file and calls it a reproducer.
- Treats OOM/timeout/SIGKILL tool artifacts as sanitizer findings.
- Uses only ASan for a bug class that needs UBSan or MSan, or runs MSan combined
  with ASan (MSan is then silently disabled).

## How to reason correctly

1. Map the target first: find the entry function that consumes untrusted bytes and
   its exact signature; the harness must call that function with (buffer, length).
2. Write `LLVMFuzzerTestOneInput` so `Size` is the only trusted length; guard the
   minimum size before parsing; keep the call pure (no hidden file or stdin reads).
3. Pick sanitizers by the suspected bug class and by what the codebase can be
   instrumented with; make UB fatal so it produces an artifact; never combine MSan
   with other sanitizers.
4. Run with artifact-saving and stats flags: a finding is (crash artifact + input)
   or it is nothing.
5. On a crash: reproduce with the saved artifact on the same binary, minimize, then
   confirm the crash still reproduces on the minimized input.
6. Demonstrate the reachable path: the report stack must run from the harness entry
   through the parser to the fault.
7. For "no findings", report runtime, exec rate, corpus growth, coverage, sanitizer
   set, entry point, and what was NOT fuzzed.

## What to verify

- A saved, minimized crashing input exists and reproduces the report with a
  one-command rerun on the pinned binary.
- The harness demonstrably reaches the target entry (coverage trace or report stack
  includes the parser frames).
- Sanitizer set is named and correct for the bug class (no MSan+ASan mix; UB is
  fatal).
- The finding report states: reproducer command, minimized input, reachable path,
  coverage, runtime, limitations.
- "No findings" claims include runtime, coverage, corpus, sanitizer set, and
  limitations.

## How to verify

```
clang -O1 -g -fsanitize=fuzzer,address -fno-omit-frame-pointer -o fuzz_pkt fuzz_pkt.c
./fuzz_pkt -artifact_prefix=./art/ -max_total_time=600 ./corpus
./fuzz_pkt -runs=1 ./art/crash-<hash>
./fuzz_pkt -minimize_crash=1 -runs=1 ./art/crash-<hash>
./fuzz_pkt -runs=1 ./art/minimized-from-<hash>
afl-tmin -i crashes/id:000000 -o crash.min -- ./target @@
afl-cmin -i in -o in.min -- ./target @@
```

Confirm the report stack passes through the target's parse functions
(`-print_pcs=1` or `-print_coverage=1` shows the parser frames). For a "no
findings" claim, re-run with `-print_final_stats=1` and include those numbers.

## Where the knowledge comes from

- `libfuzzer-docs` — `LLVMFuzzerTestOneInput` contract, artifact/minimization/stats
  flags.
- `aflpp-docs` — `afl-fuzz`, `afl-tmin`/`afl-cmin`, `AFL_USE_ASAN`/`AFL_USE_UBSAN`.
- `oss-fuzz` — reproduce, minimize, disclose discipline.
- `clang-docs` — sanitizer build flags and MSan isolation.
- The three-part proof standard (reproducible report + minimized input + reachable
  path) is an INFERRED adaptation, idea only, of a third-party fuzzing skill
  (0xazanul/fuzz-skill); it is not a registered source and is not claimed as new.

## Related skills

- `sanitizer-report-reading` (require) — turns the crash report into a triaged finding
- `sanitizer-agent-ci-loop` (require) — keeps the fuzz run reproducible and tracked
- `meta-verification` (recommend) — proves a "clean" run actually ran the code path
- `c-undefined-behavior` (recommend) — root-causes the UB that UBSan made fatal

## Evaluation

- Synthetic: bad fixtures (`examples/bad/`) must be rejected against the gate; good
  fixtures (`examples/good/`) must pass it.
- False-positive: a correct harness with the correct sanitizer flags must NOT be
  flagged; an honest report with stated limitations must not be rejected.
- Historical: the CVE-2022-3602 (OpenSSL, OSS-Fuzz) case — the agent must name the
  three required artifacts and the reproduce-minimize-disclose discipline, not just
  "the parser overflowed".
- Adversarial: a crash whose stack never reaches the target (harness-level bug, or
  OOM/timeout tool artifact) must be rejected as non-evidence.
- Verified facts and exact verification commands: `evals/README.md`. This
  environment has no clang/libFuzzer/AFL toolchain, so all fixture claims are
  researched (UNVERIFIED here), with the exact commands that would verify them.
