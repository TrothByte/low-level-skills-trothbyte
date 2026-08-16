# Property-Based Testing for Kernel-Adjacent Code — Reference Rules

Knowledge layer for `property-based-testing-kernel`. Format: RULE → WHY
AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE →
VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED / UNVERIFIED.
The C and Rust fixtures were executed on this host (gcc 16.1.0, rustc
1.97.1); output is in evals/README.md. Relative paths assume the skill
directory as CWD.

## 1. A property test asserts a universal claim, not a fixed expectation

- **RULE**: PBT checks "for all x in domain D: P(f(x))" against a stream
  of generated inputs. A loop over fixed inputs with fixed assertions is
  example testing, not property testing. Classic shapes: round-trip
  `decode(encode(x)) == x`, invariant `0 <= out <= bound`, order
  preservation, idempotence.
- **WHY AI GETS IT WRONG**: agents write `for (i...) { assert(f(inputs[i])
  == expected[i]) }` and label it a property test; it checks a handful of
  cases, not the property.
- **CORRECT REASONING**: formulate the universal sentence first; if the
  assertion depends on the specific input, it is not a property. The
  generator produces inputs; the assertion must hold for all of them.
- **EXAMPLE** (bad): asserting `checksum(buf) > 0` for random buffers
  (trivially true for most bytes — a tautological near-pass).
- **COUNTEREXAMPLE** (good): `examples/good/pbt_checksum.c` asserts
  `reparse(checksummed(x)) == x` — a round-trip property over generated
  buffers.
- **VERIFICATION**: the fixture runs 5000 generated cases; executed.
- **SOURCE**: proptest-docs (proposed NEW, INFERRED until registered);
  quickcheck (proposed NEW).

## 2. Generators must cover boundary values, not just rand()%n

- **RULE**: `rand() % n` is biased (a 256-boundary and 255-boundary are
  under-sampled) and never systematically hits 0, 1, MAX-1, MAX, or
  power-of-two boundaries where off-by-one and wrap bugs live. Structured
  generators mix uniform random values with boundary values: lengths in
  {0, 1, n/2, n-1, n, n+1, MAX}, bit patterns of all-ones/all-zeros.
- **WHY AI GETS IT WRONG**: agents use `rand()%size` for lengths and
  never exercise the exact boundaries where kernel-adjacent code wraps.
- **CORRECT REASONING**: the generator composes a set of boundary cases
  with random cases; the property checker samples both.
- **EXAMPLE** (bad): a generator that only ever produces even lengths —
  never triggers the odd-length off-by-one.
- **COUNTEREXAMPLE** (good): `examples/good/pbt_checksum.c` explicitly
  adds `len ∈ {0, 1, 2, 15, 16, 17, 255, 256}` cases to the random
  stream.
- **VERIFICATION**: executed on this host (boundary cases run).
- **SOURCE**: proptest-docs (strategy composition); quickcheck
  (generation loop).

## 3. Shrinking is mandatory: minimal failing input, not the raw random one

- **RULE**: on a property failure, the test must reduce the input to a
  minimal reproducer (shrink lengths toward 0, clear fields, try
  boundary values, binary-search the failing length). Reporting the raw
  1 MB random blob makes the bug untriagable.
- **WHY AI GETS IT WRONG**: agents `println!` the raw generated input on
  failure and call it done; the minimal case (e.g. `len == 17`) is what
  the kernel maintainer needs.
- **CORRECT REASONING**: implement a shrink loop: on failure, try
  progressively smaller/lower values and re-check the property until the
  failing input stops shrinking; report that.
- **EXAMPLE** (bad): `examples/bad/pbt_no_shrink.c` prints the raw
  failing buffer.
- **COUNTEREXAMPLE** (good): `examples/good/pbt_checksum.c` shrinks the
  failing length to the boundary value before reporting.
- **VERIFICATION**: `pbt 7` on the bad fixture prints the raw input; the
  good fixture prints the shrunk length (executed).
- **SOURCE**: proptest-docs (shrinking); quickcheck (shrink
  implementation).

## 4. Deterministic seed with cross-run variation; log the seed on failure

- **RULE**: the PRNG seed must be fixed for reproducible CI runs but
  varied across runs (default: derived from time/entropy; replay: fixed).
  On failure, print the seed so the case reproduces. Non-deterministic
  "random" tests that cannot replay are a debugging trap.
- **WHY AI GETS IT WRONG**: agents seed with `0` every run (never vary
  inputs, hide the bug) or with `time()` without logging (failure cannot
  be replayed).
- **CORRECT REASONING**: `seed = argc > 1 ? argv[1] : time(NULL)`, print
  the seed in the header line, and derive every PRNG draw from it.
- **EXAMPLE** (bad): fixed seed `0` with the property never failing.
- **COUNTEREXAMPLE** (good): `examples/good/pbt_checksum.c` takes the
  seed from argv and logs it.
- **VERIFICATION**: run with two seeds (`pbt 12345`, `pbt 99999`); both
  reproducible (executed).
- **SOURCE**: quickcheck (seeding model); libfuzzer-docs (repro
  discipline for random testing).

## 5. Preconditions and the C contract: size + pointer as one unit

- **RULE**: a C function's contract includes its preconditions (buffer
  non-NULL, `size <= capacity`, valid enum values). The generator must
  produce valid (contract-respecting) inputs — or the checker must
  filter invalid ones — otherwise failures are test-harness bugs, not
  code bugs. Pass `size` and pointer together; never generate a pointer
  without the length.
- **WHY AI GETS IT WRONG**: agents generate a random pointer/length pair
  that violates the contract (e.g. `size` larger than the buffer) and
  report the crash as a product bug.
- **CORRECT REASONING**: generate into a heap buffer, set `size` ≤
  allocated capacity, and let the property operate on
  `(buffer, size)`. If the function has preconditions, the test is only
  valid on inputs that satisfy them.
- **EXAMPLE** (bad): passing `size = 10^9` to a function whose buffer is
  256 bytes and calling the segfault a finding.
- **COUNTEREXAMPLE** (good): `examples/good/pbt_checksum.c` allocates a
  buffer and keeps `size` within it.
- **VERIFICATION**: executed on this host.
- **SOURCE**: rust-miri (unsafe/FFI contract checks); kernel-source
  (kernel APIs are size-parameterized).

## 6. PBT finds counterexamples; it does not prove correctness

- **RULE**: a green PBT run is evidence over the sampled input space, not
  a proof. Claiming "verified by property tests" without quantifying the
  sample (seed, iterations) and without complementary methods (sanitizers,
  Miri for unsafe code, model checking where warranted) overstates the
  result.
- **WHY AI GETS IT WRONG**: agents report "PBT passed → code correct",
  ignoring that the generator never reached the failing region.
- **CORRECT REASONING**: state the test budget (e.g. 5000 cases, seed
  logged), and run sanitizers/UB tools over the same generated inputs;
  treat green as "no counterexample found in this sample".
- **EXAMPLE** (bad): a tautological property that passes 100% of the
  time.
- **COUNTEREXAMPLE** (good): the fixture's header line states iterations
  and seed; the README states the proof-vs-sample boundary.
- **VERIFICATION**: README claims (documented).
- **SOURCE**: proptest-docs (statistical nature); rust-miri (UB checks
  complement).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| property shape | universal "for all x: P(f(x))", not fixed expectations |
| generator | boundary values (0,1,MAX,±1, pow2) + random, not just rand()%n |
| shrinking | reduce failing input to minimal reproducer |
| seed | deterministic + logged; vary across runs |
| C contract | generate (ptr, size) as one valid unit |
| scope | finds counterexamples, does not prove correctness |
