# Evaluation — post-quantum-crypto-mlkem

Skill: `skills/security/post-quantum-crypto-mlkem`.
Stability: `researched` (full native verification needs liboqs/PQClean/OpenSSL
+ OQS provider; the constant-time and implicit-rejection models are
host-verified with real python runs on this host).

## Verified facts (host, recorded 2026-08-20)

Host: Windows, python 3.11.9 (CSPRNG/SHAKE via stdlib `hashlib`). All six
fixtures were actually executed on this host; output below is verbatim.

Good fixtures (exit 0, expected PASS):

```
python examples/good/ct_rejection_sampling.py
  lucky:   iterations=256 accepted=256/256
  unlucky: iterations=256 accepted=0/256
  PASS — fixed iteration count for accepted vs rejected samples
  (exit 0)

python examples/good/implicit_rejection.py
  valid ct:   returns 32 bytes, type=bytes
  invalid ct: returns 32 bytes, type=bytes
  path trace valid:   ['decrypt', 'check_valid', 'derive_valid', 'derive_rejected', 'select']
  path trace invalid: ['decrypt', 'check_valid', 'derive_valid', 'derive_rejected', 'select']
  PASS — invalid ciphertext still returns a derived value on the same path
  (exit 0)

python examples/good/ct_compare.py
  same bytes:          equal=True, iterations=64
  diff at byte 0:      equal=False, iterations=64
  diff at byte 63:     equal=False, iterations=64
  PASS — fixed work for mismatch at first byte vs last byte
  (exit 0)
```

Bad fixtures (exit 1, expected FLAG):

```
python examples/bad/branchy_rejection.py
  lucky:   iterations=4973
  unlucky: iterations=9964
  FLAG: source: loop bound depends on the accepted-count state
        (`while len(coeffs) < n_coeffs`)
  FLAG: behavior: iteration count varies with input (4973 vs 9964) — timing leak
  FLAG — data-dependent control flow: iteration count varies with input
  (exit 1)

python examples/bad/early_exit_decaps.py
  valid ct:   returns 32 bytes
  invalid ct: returns None
  FLAG: source: early `return None` on the invalid-ciphertext path — API
        distinguishes valid from invalid
  FLAG: behavior: valid ciphertext yields a secret, invalid yields None —
        an oracle distinguishes them by return code (and by the shorter
        early-exit timing path)
  FLAG — early exit on invalid ciphertext: decapsulation oracle (breaks
        FIPS 203 implicit rejection)
  (exit 1)

python examples/bad/early_exit_compare.py
  diff at byte 0:  iterations=1
  diff at byte 63: iterations=64
  FLAG: source: loop returns at the first differing byte
        (`if x != y: return False`)
  FLAG: behavior: iteration count depends on mismatch position (1 vs 64) — timing leak
  FLAG — early-exit compare: variable work leaks the mismatch position
  (exit 1)
```

Interpretation: the good fixtures prove the properties (fixed sampling
iteration count 256 regardless of acceptance; identical decapsulation path
for valid and invalid ciphertexts; fixed 64-iteration compare). The bad
fixtures demonstrate the actual leaks with real numbers: the early-exit
sampler runs 4973 vs 9964 iterations depending on input, and the early-exit
compare runs 1 vs 64 iterations. These python models are property-correct
stand-ins; the equivalent C/assembly timing behavior is documented in
`side-channel-constant-time-verification` (gcc 16.1.0 timing demos).

## Synthetic evals

- easy/positive: `good/ct_rejection_sampling.py` — fixed-iteration rejection
  sampling must be approved; the identical iteration count (256 vs 256) is the
  evidence.
- medium/positive: `good/implicit_rejection.py` — invalid ciphertext still
  returns a 32-byte derived value on the identical path; must be approved.
- easy/positive: `good/ct_compare.py` — XOR-fold compare, fixed work, must be
  approved.
- easy/negative: `bad/branchy_rejection.py` — early-exit sampling (4973 vs
  9964 iterations) must be flagged as a timing leak.
- easy/negative: `bad/early_exit_decaps.py` — `return None` on invalid ct must
  be flagged as a decapsulation oracle.
- medium/negative: `bad/early_exit_compare.py` — memcmp-style compare (1 vs 64
  iterations) must be flagged.

## False-positive evals (correct code that must NOT be flagged)

- A correct implicit-rejection decapsulation "returns a value on an invalid
  ciphertext" — that is the FIPS 203 requirement, not a bug; do NOT flag
  "it did not return an error".
- A fixed-iteration rejection-sampling loop that processes more candidates
  than the minimum needed — do NOT flag as "wasted iterations"; the fixed
  bound is the security property.
- A final `acc == 0` branch AFTER the XOR-fold — the branch depends only on
  equality, not on mismatch position; do NOT flag.
- Checking ciphertext length before parsing — a length check on public
  metadata is not secret-dependent; do NOT flag as a timing leak.
- `ct_compare.py` reading all bytes "even when they differ early" — that is
  the correct constant-time behavior.

## Historical evals

- **Kyber round-3 → FIPS 203**: agents trained on the round-3 submission must
  update — `crypto_kem_keypair/enc/dec` became `mlkem*_keygen/encaps/decaps`;
  encapsulation randomness moved inside `Encaps` (no external seed); the
  decapsulation failure handling was clarified as implicit rejection with a
  fixed secret `z`; parameter-set names changed (Kyber-768 → ML-KEM-768).
- **Dilithium round-3 → FIPS 204**: name changes (Dilithium-2/3/5 → ML-DSA-44/
  65/87) and sampling/`MatrixExpand` renames; agents must use the FIPS names.
- **CVE-2026-22705 class (ML-DSA)**: value-dependent integer division timing
  in a signing path — division latency is not constant; relevant when
  reviewing ML-DSA signing implementations (see
  `side-channel-constant-time-verification`).
- Task: given round-3 code, produce the FIPS 203/204 equivalent and name the
  behavioral changes (Encaps randomness, implicit rejection, renames).

## Adversarial evals

- A decapsulation that early-exits on mismatch "to save work" — must be
  rejected as a decapsulation oracle, even though it returns correct results
  for valid ciphertexts.
- A rejection sampler that early-exits when the buffer is full — must be
  rejected; iteration count varies with input (the model shows 4973 vs 9964).
- A "quantum-safe" claim backed only by a self-round-trip test (no ACVP/PQClean
  vectors) — must be rejected as unverified.
- Static ML-KEM key reuse across connections presented as secure — must be
  rejected; ML-KEM is ephemeral by design.
- A "constant-time" implementation never checked at the assembly level — must
  be pushed to `objdump -d`/dudect evidence (see related skill).

## Verification commands (target)

Documented target commands (liboqs, PQClean, OpenSSL + OQS provider are NOT
installed on this host — recorded as targets, not executed here):

```
# liboqs: build and run the known-answer test suite
cmake -B build -DCMAKE_BUILD_TYPE=Release -DOQS_BUILD_ONLY_LIB=ON
cmake --build build
ctest --test-dir build

# PQClean: ML-KEM-768 known-answer test
make -C crypto_kem/ml-kem-768/m4clean test

# OpenSSL 3 + OQS provider: list PQ/hybrid algorithms and serve a hybrid group
openssl list -kem-algorithms -provider oqsprovider -provider default
openssl s_server -groups X25519MLKEM768 -accept 4433 -www

# NIST ACVP harness (per NIST ACVP documentation): run the ML-KEM-768 KAT
# against the implementation under test and compare digests.
```

## Scoring

- Precision: high on the modeled classes — the fixtures distinguish the
  property (fixed iterations, implicit rejection) from the violation with
  real iteration counts and return values. Recall: high for rejection
  sampling and decapsulation handling; parameter/API-drift recall depends on
  the reference notes. FP-rate: low — the FP cases above are behaviorally
  distinct from the flagged ones. Native-suite results (liboqs/PQClean KATs,
  openssl OQS hybrid handshake) remain target-UNVERIFIED on this host.
