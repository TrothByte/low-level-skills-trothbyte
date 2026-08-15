# Evaluation — zig-fuzzer-and-testing

Skill: `skills/zig/zig-fuzzer-and-testing`.
Stability target: `researched`. Toolchain: zig is NOT installed on this host; the code
targets the 0.15–0.17 API surface (verified against the 0.16.0 release notes Fuzzer
section and the langref Zig Test section). Verification commands below are the recorded
plan, not run results.

## Synthetic evals

| Case | Fixture | Expected | Command |
|---|---|---|---|
| easy/negative | `bad/allocator_leak.zig` | runner reports leak (fails) | `zig test` |
| medium/negative | `bad/naive_fuzz_target.zig` | structurally can't fail — review flag | `zig build --fuzz=10K` + review |
| medium/negative | review | 0.15 `[]const u8` fuzz signature on 0.16+ — silently ordinary test | review |
| positive | `good/tests.zig` | passes; expect/skip/is_test/leaks | `zig test` |
| positive | `good/fuzz_smith.zig` | Smith target fuzzes and fails on `sum == 1234` | `zig build --fuzz=10K` |
| positive | `good/fuzz_015.zig` | 0.15-era entry (documented for that pin) | `zig test -fuzz fuzz_015.zig` |

## False-positive evals (correct code must not be flagged)

- `good/tests.zig` — `error.SkipZigTest` for non-Linux, `builtin.is_test` guard,
  `std.testing.allocator` + deinit — all correct.
- `good/fuzz_smith.zig` — `eosWeightedSimple` loop termination and
  `valueRangeAtMost(u32, 100)` constraints are the documented Smith idioms.
- `zig build --fuzz=10K` bounded runs and `-j<N>` multiprocessing — correct usage.

## Historical evals

- 0.16.0 fuzz signature change (`[]const u8` → `*std.testing.Smith`): the release-notes
  upgrade example is the regression target — the function name is arbitrary; the parameter
  type is what makes it a fuzz target.
- 0.16.0: multiprocess fuzzing (`-j`), infinite mode, crash dumps, and the AST Smith that
  found 20 `zig fmt` bugs — evidence that the fuzzer is a real tool, not a stub.
- 0.15.1: "fuzzer mostly unchanged from 0.14.0" — the `[]const u8` era.

## Adversarial evals

- The naive-target trap: an input validator that checks only the first octet passes every
  generated input; the fix is structural (consume the whole input and assert on processed
  results), not "more fuzzing time".
- A crash "fixed" by deleting the crash dump or loosening the assertion instead of fixing
  the code — the evidence gate (`FuzzInputOptions.corpus` + `@embedFile` repro).
- A fuzz target with unbounded `while (!smith.eos())` allocation loops — memory-pressure
  and timeout guards (`--test-timeout`, bounded lengths).

## Verified facts

- KNOWN (from 0.16.0 release notes and langref; not run on this host):
  - 0.16.0 replaced the fuzz parameter with `*std.testing.Smith`; Smith provides `value`,
    `eos` (guaranteed eventually true), `bytes`, `slice`, weighted variants
    (`eosWeightedSimple`, `baselineWeights`), and `valueRangeAtMost`/`valueRangeLessThan`.
  - 0.16.0 fuzzer: multiprocess (`-j`), infinite mode (prioritizes effective tests),
    crash dumps for `FuzzInputOptions.corpus` + `@embedFile` reproduction.
  - The default test runner reports `std.testing.allocator` leaks and exits nonzero.
  - `error.SkipZigTest` skips; `builtin.is_test` detects test builds.
  - The AST Smith found 20 unique bugs in `zig fmt` during the 0.16.0 cycle.
- INFERRED: the exact `--fuzz` flag interaction with `--webui` and the fuzz-entry
  discovery rules on 0.17-dev.
- UNVERIFIED (needs zig on this host): actual fuzz output, crash-dump paths, and the
  `sum == 1234` discovery time.

## Target toolchains (absent, documented)

- zig 0.15.2 / 0.16.0 / 0.17.0-dev: not installed. First execution plan: install zig,
  then run the commands in SKILL.md §How to verify.
