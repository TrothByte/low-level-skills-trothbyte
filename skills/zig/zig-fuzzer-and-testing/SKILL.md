---
name: zig-fuzzer-and-testing
description: Use when writing Zig tests and fuzz targets: test declarations, std.testing.allocator leak detection, the built-in fuzzer and std.testing.Smith interface, corpora, and crash reproduction. Prevents naive assertions that pass all inputs, leaked allocations, and wrong-version fuzz signatures. Version-pinned to Zig 0.15-0.17.
---

# Zig Fuzzer and Testing

## When to use

- Writing `test` declarations and using `std.testing.*` helpers
  (`expectEqual`, `expectError`, `expectEqualStrings`, `std.testing.allocator`).
- Writing fuzz targets for the built-in fuzzer (`zig build --fuzz`), including the
  `std.testing.Smith` interface (0.16+).
- Reproducing fuzzer-found crashes from saved crash dumps and building corpora.
- Reviewing whether a test/fuzz assertion can actually fail (naive assertions).

## When not to use

- External fuzzers (AFL++, libFuzzer) and their harnesses — see `fuzzing-harness-evidence-gate`
  and `sanitizer-agent-ci-loop`.
- Test frameworks for other languages — this is Zig's std.testing + built-in fuzzer.
- General allocator leak/debugging — see `zig-allocators-and-memory-management`.

## What the agent often gets wrong

- Using the pre-0.16 fuzz signature `fn fuzzTest(_: void, input: []const u8)` on 0.16+,
  where the fuzzer feeds a `*std.testing.Smith` instead.
- Writing fuzz targets whose assertions cannot fail on adversarial input (e.g. parsing
  only the first byte, or asserting `sum != 1234` with inputs that never reach the deep
  code path) — "input that passes naive tests".
- Allocating inside a fuzz/test with `page_allocator` (or forgetting deinit) so leaks are
  invisible; `std.testing.allocator` must be used for leak detection.
- Confusing the fuzzer CLI: `zig test -fuzz file.zig` (0.15-era) vs `zig build --fuzz`
  (0.16+, from the build runner).
- Claiming a crash is "fixed" without a minimized input and a crash dump; the 0.16 fuzzer
  saves crash inputs to files for `@embedFile`-based reproduction.
- Forgetting `error.SkipZigTest` for platform-specific tests, or reading `builtin.is_test`
  wrong.

## How to reason correctly

1. Tests: write `test` blocks; assert with the specific helper; allocate through
   `std.testing.allocator` so the runner reports leaks; return `error.SkipZigTest` to
   skip; filter with `--test-filter`.
2. Fuzz targets (0.16+): declare `fn fuzzTest(_: void, smith: *std.testing.Smith) !void`
   and generate inputs with `smith.value(T)` / `smith.bytes` / `smith.slice` / `smith.eos`;
   end the loop when `smith.eos()` returns true so the fuzzer controls input length.
3. Make the assertion reachable: the fuzzer mutates and grows inputs; a target that reads
   only the first byte or truncates early never exercises the bug. Design the target so
   deep code is reachable from generated inputs.
4. Add value guidance: `valueRangeAtMost`/`valueRangeLessThan` constrain integers;
   weighted values (`eosWeightedSimple`, `valueWeighted` with `baselineWeights`) steer
   toward interesting inputs.
5. Reproduce: run with the crash file through `std.testing.FuzzInputOptions.corpus`
   (`@embedFile` the saved input); confirm the fix by re-running the fuzzer and the corpus.
6. Use `-j<N>` for multiprocess fuzzing; `zig build --fuzz=10K` for a bounded run.

## What to verify

- Fuzz function signatures match the pinned version (`*std.testing.Smith` on 0.16+,
  `[]const u8` on 0.15).
- Fuzz targets use `smith.eos()`/`eosWeightedSimple` to bound input length and consume
  the whole input; no early truncation that skips deep paths.
- Allocations inside tests/fuzz use `std.testing.allocator` with matching deinit.
- Assertions can actually fail on adversarial inputs (value ranges reach the buggy path).
- Crashes are reproduced from the saved crash dump, not just "fixed by eye".
- Test suite and fuzz run pass under the pinned Zig.

## How to verify

```
zig test examples/good/tests.zig
zig build --fuzz=10K          # 0.16+: fuzz all fuzz tests, bounded
zig build --fuzz              # infinite mode (Ctrl-C to stop)
zig test -fuzz examples/good/fuzz_015.zig   # 0.15-era entry point
zig test examples/bad/naive_fuzz_target.zig
zig test examples/bad/allocator_leak.zig
```

Researched — zig not installed on this host; commands are the recorded verification plan.

## Where the knowledge comes from

- zig-release-notes 0.16.0 (Fuzzer: Smith, Multiprocess Fuzzing, Fuzzing Infinite Mode,
  Crash Dumps; the AST Smith found 20 fmt bugs) and 0.15.1 (fuzzer mostly unchanged from
  0.14.0).
- zig-std-source (std/testing.zig — allocator, Smith, FuzzInputOptions;
  std/testing/smith.zig; lib/compiler/test_runner.zig).
- zig-langref §Zig Test (Test Declarations, Test Failure, Skip Tests, Report Memory
  Leaks, Detecting Test Build, The Testing Namespace), §Builtin Functions (@embedFile).

## Related skills

- `zig-allocators-and-memory-management` — testing allocator leak detection.
- `fuzzing-harness-evidence-gate` — no crash claim without reproducer+minimized input.
- `sanitizer-agent-ci-loop` — CI integration of sanitizer/fuzz evidence.
- `zig-error-model-and-defers` — error unions inside tests/fuzz targets.
- `zig-comptime-metaprogramming` — comptime assertions in tests.

## Evaluation

- Synthetic: wrong-version fuzz signature, non-leaking-but-wrong allocator use, fuzz
  targets that truncate input early, assertions that cannot fail — must be caught; good
  Smith fuzz target and test suite must pass.
- False-positive: `error.SkipZigTest` guards, `builtin.is_test` checks, `@embedFile`
  crash reproduction, `--fuzz=10K` bounded runs — must NOT be flagged.
- Historical: the 0.16.0 fuzz signature change (`[]const u8` → `*std.testing.Smith`) is
  the regression target.
- Adversarial: an input that passes naive tests (e.g. an "IPv4 parser" that validates only
  the first octet and silently ignores the rest) — the fuzzer must be shown to reach the
  guarded path, or the target is inadequate.
- Commands and recorded results: `evals/README.md`.
