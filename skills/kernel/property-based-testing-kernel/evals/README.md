# Evaluation — property-based-testing-kernel

Skill: `skills/kernel/property-based-testing-kernel`. Stability target:
`evaluated`. PBT methodology KNOWN from quickcheck/proptest lineage.
Both fixtures EXECUTED on this host: C (gcc 16.1.0) and Rust
(rustc 1.97.1). proptest/quickcheck are PROPOSED NEW sources — claims
from them marked INFERRED until registered.

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/pbt_no_shrink.c` | fixed seed + no shrink + no universal property → flagged | structural |
| medium/positive | `good/pbt_checksum.c` | finds planted len=17 bug and shrinks to minimal case | executable |
| medium/positive | `good/pbt_rust.rs` | round-trip + in-bounds properties pass, boundaries covered | executable |

Detection rule: (1) the assertion is a universal property over generated
inputs; (2) generator covers boundary values; (3) shrinking is present
or documented; (4) seed is deterministic + logged; (5) C contract keeps
size within the buffer.

## False-positive evals (correct code must NOT be flagged)

- A plain fixed-input unit test that does NOT claim to be a property test
  — not a PBT failure.
- A property test that reports "sample, not proof" in its comment and
  runs bounded iterations — honest, not a defect.
- `good/pbt_rust.rs`'s `wrap_index` test correctly never generates
  `capacity == 0` (precondition respected) — correct, not a gap.

## Historical evals

- Off-by-one and wrap-around bugs in parsers/decoders caught by
  PBT-style campaigns are documented in the quickcheck/proptest
  ecosystems and kernel test-development lore. UNVERIFIED as named
  kernel incidents on this host.
- The "tautological property passes" class is documented in PBT
  literature. UNVERIFIED as a named incident here.

## Adversarial evals

- A generator that never reaches the failing region (e.g. only even
  lengths when the bug is at len 17) — the "passes" trap from
  `meta-verification-harness-validity`.
- A property that is true by construction (e.g. `x == x`) — passes 100%,
  proves nothing.
- A shrinking implementation that reduces to an invalid input (violating
  preconditions) and then "fails" spuriously.
- A fixed seed that hides the bug in CI but the bug reappears with a
  different seed — the fixture's seed-logging discipline catches this.

## Verification commands

```
gcc -Wall -Wextra -Werror -O2 examples/good/pbt_checksum.c -o pbt
pbt 12345
pbt 99999
pbt 7
gcc -Wall -Wextra -Werror -O2 examples/bad/pbt_no_shrink.c -o pbtnosh
pbtnosh
rustc --edition 2021 --test examples/good/pbt_rust.rs -o pbt_rust
pbt_rust
```

## Verified facts

- KNOWN: universal-property formulation; boundary-aware generation;
  shrinking; deterministic seed discipline; precondition-respecting
  generators; PBT-is-not-a-proof. Sources: proptest-docs (proposed NEW),
  quickcheck (proposed NEW), libfuzzer-docs, rust-miri.
- EXECUTED on this host: `pbt_checksum.c` finds and shrinks the planted
  bug to len=17; `pbt_rust.rs` passes all three tests; `pbt_no_shrink.c`
  never finds the bug (fixed seed 0) — recorded below.
- UNVERIFIED: proptest/quickcheck crates on this host (no cargo network
  run here), kernel-internal test framework runs.

## Scoring

- precision: every flagged issue maps to a reference rule (1–6).
- recall: the no-shrink fixture is flagged; the good fixtures pass.
- FP-rate: honest "sample, not proof" tests produce zero flags.
- Decisive test: "is there a universal property, a boundary-aware
  generator, shrinking, and a logged deterministic seed?"

### Executed output (2026-08-17, MSYS2 gcc 16.1.0 / rustc 1.97.1)

```
$ gcc -Wall -Wextra -Werror -O2 examples/good/pbt_checksum.c -o pbt && ./pbt 12345
seed=12345
COUNTEREXAMPLE len=17 (raw=17), shrunk to minimal: len=17 confirmed
PLANTED BUG FOUND by property test and shrunk
exit 1   (the property test correctly fails — its job)

$ ./pbt 99999
seed=99999
COUNTEREXAMPLE len=17 (raw=17), shrunk to minimal: len=17 confirmed
exit 1

$ ./pbt 7
seed=7
COUNTEREXAMPLE len=17 (raw=17), shrunk to minimal: len=17 confirmed
exit 1

$ gcc -Wall -Wextra -Werror -O2 examples/bad/pbt_no_shrink.c -o pbtnosh && ./pbtnosh
PASS: 1000 cases with fixed seed 0   (BUG HIDDEN — fixed seed never hits len 17)
exit 0   (flagged: hidden bug + no shrinking + biased generator)

$ rustc --edition 2021 --test examples/good/pbt_rust.rs -o pbt_rust && ./pbt_rust

running 3 tests
test roundtrip_encode_decode_is_identity ... ok
test wrap_index_always_in_bounds ... ok
test boundary_lengths_covered ... ok

test result: ok. 3 passed; 0 failed
exit 0
```
