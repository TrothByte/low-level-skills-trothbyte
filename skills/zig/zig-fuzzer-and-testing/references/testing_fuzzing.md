# Zig Fuzzer and Testing — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.
Version markers: KNOWN / INFERRED / UNVERIFIED.

## 1. Test declarations and std.testing helpers

- **RULE**: `test "name" { ... }` blocks (return type `anyerror!void`, fixed) are compiled
  only in test builds. Use `std.testing.expect`, `expectEqual`, `expectError`,
  `expectEqualStrings`, etc.; `--test-filter [text]` selects by name; `error.SkipZigTest`
  skips; `@import("builtin").is_test` detects a test build. The default test runner lives
  in lib/compiler/test_runner.zig (master).
- **WHY AI GETS IT WRONG**: writes test code that runs in production (no `is_test` guard),
  or asserts with plain `if (...) return error.Fail` losing the helpful diagnostics.
- **CORRECT REASONING**: tests are ordinary functions returning `!void`; the runner calls
  them and reports OK/SKIP/FAIL with the error trace. `expectEqual(expected, actual)`
  gives clear messages.
- **EXAMPLE** (bad):
  ```zig
  test "oops" {
      if (add(1, 2) != 3) return error.Wrong; // works but poor diagnostics
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  test "oops" {
      try std.testing.expectEqual(@as(i32, 3), add(1, 2));
  }
  ```
- **VERIFICATION**: `zig test examples/good/tests.zig`.
- **SOURCE**: zig-langref §Zig Test (Test Declarations, Test Failure, Skip Tests,
  Detecting Test Build, The Testing Namespace); zig-std-source (lib/compiler/test_runner.zig).

## 2. std.testing.allocator catches leaks

- **RULE**: allocating through `std.testing.allocator` inside tests makes the runner
  report leaks ("1 tests leaked memory", exit nonzero). `std.testing.FailingAllocator`
  simulates OOM.
- **WHY AI GETS IT WRONG**: uses `page_allocator` in tests (leaks invisible) or omits
  `defer list.deinit(gpa)`.
- **CORRECT REASONING**: the testing allocator is the leak detector; every allocation in a
  test gets a matching free. Unmanaged containers take the allocator per call.
- **EXAMPLE** (bad):
  ```zig
  test "leak" {
      var list: std.ArrayList(u32) = .empty;
      try list.append(std.testing.allocator, 1); // no defer deinit
      try std.testing.expectEqual(@as(usize, 1), list.items.len);
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  test "no leak" {
      const gpa = std.testing.allocator;
      var list: std.ArrayList(u32) = .empty;
      defer list.deinit(gpa);
      try list.append(gpa, 1);
      try std.testing.expectEqual(@as(usize, 1), list.items.len);
  }
  ```
- **VERIFICATION**: `zig test examples/bad/allocator_leak.zig` reports the leak and fails.
- **SOURCE**: zig-langref §Zig Test (Report Memory Leaks); zig-std-source (std/testing.zig).

## 3. Fuzz function signature: []const u8 (0.15) → *std.testing.Smith (0.16+)

- **RULE**: 0.16.0 replaced the fuzz parameter `[]const u8` with `*std.testing.Smith`.
  Smith generates typed values: `smith.value(T)`, `smith.bytes(&buf)`, `smith.slice(buf)`,
  `smith.eos()` (end-of-stream marker that is guaranteed to eventually return true);
  weighted variants (`valueWeighted`, `eosWeightedSimple`, `boolWeighted`) and
  `baselineWeights`; range helpers (`valueRangeAtMost`, `valueRangeLessThan`).
- **WHY AI GETS IT WRONG**: writes the 0.15 signature on 0.16+ (the function silently
  becomes an ordinary test that runs once), or feeds raw bytes into a `Smith` target.
- **CORRECT REASONING**: use `smith.eos()`/`eosWeightedSimple` in a `while` loop to let
  the fuzzer control input length, and `smith.value(u8)` for elements.
- **EXAMPLE** (bad, 0.16+):
  ```zig
  fn fuzzTest(_: void, input: []const u8) !void { // runs once, not fuzzed on 0.16
      var sum: u64 = 0;
      for (input) |b| sum += b;
      try std.testing.expect(sum != 1234);
  }
  ```
- **COUNTEREXAMPLE** (good, 0.16+):
  ```zig
  fn fuzzTest(_: void, smith: *std.testing.Smith) !void {
      var sum: u64 = 0;
      while (!smith.eosWeightedSimple(7, 1)) {
          sum += smith.value(u8);
      }
      try std.testing.expect(sum != 1234);
  }
  ```
- **VERIFICATION**: `zig build --fuzz=10K` (0.16+) fuzzes the Smith target and finds the
  `1234` sum; `zig test -fuzz examples/good/fuzz_015.zig` is the 0.15-era entry point.
- **SOURCE**: zig-release-notes 0.16.0 (Fuzzer → Smith, with the exact upgrade example).

## 4. Make the assertion reachable — naive targets are the trap

- **RULE**: the fuzzer mutates and extends inputs; a target that validates only a prefix
  and ignores the rest (or truncates early) never exercises the buggy deep path. The
  assertion must be reachable from generated inputs.
- **WHY AI GETS IT WRONG**: writes an "IPv4 parser" that checks only the first octet; the
  fuzzer floods valid prefixes and the naive assertion passes — "input that passes naive
  tests".
- **CORRECT REASONING**: consume the whole generated input (loop until `smith.eos()`),
  validate then process, and assert invariants about the processed result — not just the
  first field.
- **EXAMPLE** (bad):
  ```zig
  fn fuzzTest(_: void, smith: *std.testing.Smith) !void {
      const first = smith.value(u8);
      try std.testing.expect(parse(first) == 0); // ignores the rest of the input
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn fuzzTest(_: void, smith: *std.testing.Smith) !void {
      var buf: [64]u8 = undefined;
      const len = smith.slice(&buf);
      const sum = checksum(buf[0..len]);       // consumes the full input
      try std.testing.expect(checksumInvariant(sum)); // assertion on processed data
  }
  ```
- **VERIFICATION**: `zig build --fuzz=10K` — the good target explores deep paths and finds
  failures; the bad target never fails (flagged by review).
- **SOURCE**: zig-release-notes 0.16.0 (Smith — values generated "from the fuzzer";
  Fuzzing Infinite Mode — prioritizes tests yielding new inputs).

## 5. Crash dumps and corpora

- **RULE**: 0.16 fuzzing saves crashing inputs to files (paths printed in the crash
  message); reproduce with `std.testing.FuzzInputOptions.corpus` and `@embedFile` of the
  saved input. `-j<N>` enables multiprocess fuzzing; `--fuzz=10K` bounds iterations.
- **WHY AI GETS IT WRONG**: declares a crash fixed without the reproducer; "fixes" by
  changing the assertion instead of the code.
- **CORRECT REASONING**: the dump is the evidence gate (see `fuzzing-harness-evidence-gate`):
  embed the input, reproduce, fix, re-run corpus + fuzzer.
- **EXAMPLE** (bad): deleting a crash dump "because the fuzzer found it" — no evidence.
- **COUNTEREXAMPLE** (good):
  ```zig
  // reproduce.zig
  const crash_input = @embedFile("crash.bin");
  test "crash repro" {
      try runParser(crash_input);
  }
  ```
- **VERIFICATION**: `zig test reproduce.zig` fails before the fix and passes after;
  `zig build --fuzz=10K` stays clean afterwards.
- **SOURCE**: zig-release-notes 0.16.0 (Fuzzer → Crash Dumps; "reproduce the crash using
  std.testing.FuzzInputOptions.corpus and @embedFile").

## 6. The fuzzer is part of the build runner (0.16+)

- **RULE**: `zig build --fuzz` runs the fuzz target(s) through the build runner
  (0.16 help: "--fuzz[=limit] Continuously search for unit test failures"), implying
  `--webui` in infinite mode; `-j<N>` limits concurrency. The 0.15-era CLI entry was
  `zig test -fuzz <file>`.
- **WHY AI GETS IT WRONG**: invokes `zig test --fuzz` on 0.16+ and gets an unknown-flag
  error, or runs the fuzzer without `zig build`.
- **CORRECT REASONING**: check `zig build --help` for the pinned version; the fuzzer is a
  build-runner integration in 0.16.
- **EXAMPLE** (bad): `zig test --fuzz foo.zig` on 0.16+.
- **COUNTEREXAMPLE** (good): `zig build --fuzz=10K`.
- **VERIFICATION**: `zig build --help` lists `--fuzz` on 0.16+ (KNOWN from the 0.16 help
  text in the build guide).
- **SOURCE**: zig-release-notes 0.16.0 (Fuzzer; build-runner integration);
  zig-build-guide (zig build --help output).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Tests | `test` blocks, `anyerror!void`; `expectEqual`/`expectError`; `--test-filter` |
| Skip | `error.SkipZigTest`; `builtin.is_test` guards test-only code |
| Leaks | allocate via `std.testing.allocator`; runner reports leaks |
| Fuzz 0.15 | `fn fuzzTest(_: void, input: []const u8) !void`; `zig test -fuzz` |
| Fuzz 0.16+ | `fn fuzzTest(_: void, smith: *std.testing.Smith) !void`; `zig build --fuzz` |
| Smith | `value(T)`/`bytes`/`slice`/`eos()`; weights; `valueRangeAtMost` |
| Reachability | consume the full input; assert on processed results — no prefix-only checks |
| Crashes | dump to file; reproduce with `FuzzInputOptions.corpus` + `@embedFile` |
| Parallel | `-j<N>` multiprocess; `--fuzz=10K` bounded; infinite mode without limit |
