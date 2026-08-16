---
name: property-based-testing-kernel
description: Use when verifying kernel-adjacent code (parsers, checksums, boundary math, state machines) with property-based tests: designing universal properties, writing generators and shrinkers, running quickcheck/proptest-style loops in C and Rust. Teaches finding counterexamples instead of asserting specific cases, and shrinking failing inputs to minimal reproducers.
---
# Property-Based Testing for Kernel-Adjacent Code

## When to use

- Verifying parsers, decoders, checksums, and boundary arithmetic in
  kernel-adjacent code (a device-tree parser, a packet header decoder,
  a ring-buffer index math, an allocator bitmap).
- Checking algebraic/universal properties: round-trips
  (`encode(decode(x)) == x`), invariants (`0 <= size <= CAPACITY`),
  commutativity/associativity, idempotence.
- Catching off-by-one and wrap-around bugs that example-based tests miss
  because the specific hand-picked input happens to be fine.
- Running quickcheck-style loops with a deterministic PRNG inside a
  build/test harness in C or Rust.
- Shrinking a failing case to a minimal reproducer for a kernel bug
  report.

## When not to use

- Proving correctness (PBT finds counterexamples, does not prove absence)
  — use model checking/formal tools instead (`formal-spec-loop-invariants`,
  `smt-z3-sound-usage`, `rust-miri`).
- Performance or timing verification — use
  `performance-measurement-discipline`.
- Concurrency/race detection — use `concurrency-actual-parallelism-detection`
  and ThreadSanitizer.
- Fuzzing for crash discovery (PBT checks properties, not just
  "did it not crash") — fuzzing-harness-evidence-gate is complementary.
- When you can state the exact expected output for a small exhaustive set
  — plain unit tests are simpler and faster there.

## What the agent often gets wrong

- Writing "property" tests that are really example tests in a loop
  (fixed inputs, fixed assertions) — no property is checked against a
  range of generated inputs.
- Forgetting the universal quantification: asserting one specific
  generated case "works" instead of asserting a property that must hold
  for ALL inputs (e.g. testing that `checksum` returns nonzero instead
  of that `reparse(checksummed(x)) == x`).
- Not checking preconditions: generators that feed out-of-contract inputs
  (e.g. `len` larger than the buffer, NULL pointers) produce failures that
  are not real bugs — the generator must respect the function's
  precondition or the test must filter them.
- Missing shrinking: when a property fails, the agent reports the huge
  random input instead of shrinking it to the minimal counterexample —
  making the bug impossible to triage.
- In C, forgetting to bound iterations and to use a deterministic seed,
  so "random" tests are unreproducible; or using the same seed every run
  and never varying it.
- Treating the generator output as "input space" when it is biased (e.g.
  `rand() % n` produces biased distributions for sizes that can hide
  boundary bugs) — use rejection sampling or structured generators.

## How to reason correctly

1. **State the property first**: a universal claim
   "for all x in domain D, P(f(x)) holds". If you cannot write it as a
   universal sentence, you do not have a property test yet. Round-trips,
   invariants, and order-preservation are the classic shapes.
2. **Design the generator over the domain D**, respecting the function's
   preconditions: structured generators (random sizes near powers of two,
   lengths at/near 0, 1, MAX-1, MAX; random bit patterns) beat `rand()%n`.
   For C, generate into a heap buffer with an explicit length parameter.
3. **Check the property on every generated case**; on failure, *shrink*:
   reduce the input toward a minimal counterexample (binary-search the
   length, zero out fields, try length 0/1/boundary). Report the shrunk
   input.
4. **Use a deterministic seed for reproducibility**, but vary it across
   runs (seed from time unless replaying). Log the seed on failure so the
   failure replays.
5. **Integrate into the build/test loop**: in C, a standalone
   `property_test` function with assertions + a PRNG loop, called from
   `main`/test harness; in Rust, a `#[test]` that calls a quickcheck-style
   loop (or `proptest!` when the crate is available). Kernel-adjacent
   code tested on-host with a stub for the kernel API.
6. **Bound the test budget**: e.g. 1000–10000 iterations with a fixed
   seed; each iteration must be fast. This is a sample, not a proof —
   say so in the test comment.

## What to verify

- The test asserts a *universal* property (quantified over inputs), not a
  fixed expectation.
- Generators respect preconditions and cover boundary values (0, 1,
  MAX-1, MAX, near-powers-of-two lengths).
- Shrinking is implemented or documented: on failure the minimal
  reproducer is produced.
- Seed is logged/deterministic so failures replay.
- Iteration count and runtime are bounded (test budget), and the fixture
  runs in CI with a fixed seed.
- Kernel-adjacent code is tested on-host via a stub or by compiling the
  pure logic without the kernel API.

## How to verify

```
# Host-verifiable: C property test for a checksum/round-trip property
gcc -Wall -Wextra -Werror -O2 examples/good/pbt_checksum.c -o pbt
pbt 12345              # seed 12345: runs 5000 cases, PASS (no shrink needed)
pbt 99999              # different seed: PASS again (seed logged)
pbt 7                  # adversarial seed: finds counterexample, shrinks

gcc -Wall -Wextra -Werror -O2 examples/bad/pbt_no_shrink.c -o pbtnosh
pbtnosh 7              # BAD: reports raw failing input, no shrink

# Rust property test with std-only (no external crates needed):
rustc --edition 2021 --test examples/good/pbt_rust.rs -o pbt_rust
pbt_rust               # runs the built-in quickcheck-style loop, PASS
```

Both the C (gcc 16.1.0) and Rust (rustc 1.97.1) fixtures execute on this
host; real output is in `evals/README.md`. A proptest-crate run is
documented for hosts with cargo network access.

## Where the knowledge comes from

- `rust-miri` — unsafe kernel-adjacent code still needs UB checking on
  top of PBT.
- `libfuzzer-docs` — complementary random-input methodology for C.
- `proptest-docs` (proposed NEW) — shrinker/strategy model.
- `quickcheck` (proposed NEW) — the original PBT algorithm for Rust.
- `kernel-source` — realistic kernel-adjacent functions to target
  (parsers, checksums, list/bitmap math).

## Related skills

- `fuzzing-harness-evidence-gate` (recommend; crash-finding complement).
- `formal-spec-loop-invariants` (recommend; proof complement).
- `rust-unsafe-reasoning` (recommend; unsafe code under test).
- `sanitizer-agent-ci-loop` (recommend; run sanitizers on generated
  inputs).
- `meta-verification-harness-validity` (recommend; a property harness
  that never fails is not proof).

## Evaluation

- Synthetic: `bad/pbt_no_shrink.c` must be flagged (no shrinking);
  `good/pbt_checksum.c` and `good/pbt_rust.rs` must pass and find the
  planted boundary bug.
- False-positive: a fixed-input unit test is NOT a PBT failure — only
  flag claims that it is a property test; a property that holds for all
  generated inputs and correctly reports "sample, not proof" is correct.
- Historical: off-by-one and wrap-around bugs in kernel parsers/decoders
  caught by PBT-style campaigns (documented in kernel test-dev lore);
  UNVERIFIED as named incidents on this host.
- Adversarial: a property that "passes" because the generator never
  produces the failing region (e.g. `rand()%256` never exercises
  `len=255` boundary); a test that catches nothing because the
  assertion is tautological (e.g. `x == x`).
- Verified facts and commands: `evals/README.md`.
