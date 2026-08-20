---
name: post-quantum-crypto-mlkem
description: Use when selecting or implementing post-quantum cryptography — ML-KEM (Kyber), ML-DSA (Dilithium), hybrid key exchange, or reviewing constant-time rejection sampling and decapsulation-failure handling. Prevents misuse that opens chosen-ciphertext or side-channel holes.
---

# Post-Quantum Crypto: ML-KEM and ML-DSA

## When to use

- Selecting or migrating to NIST-standardized post-quantum algorithms: ML-KEM
  (FIPS 203, lattice KEM, CRYSTALS-Kyber lineage, ML-KEM-768 the default) and
  ML-DSA (FIPS 204, lattice signature, CRYSTALS-Dilithium lineage).
- Implementing or reviewing key generation, encapsulation, or decapsulation,
  including rejection sampling from SHAKE output and failure handling.
- Designing hybrid key exchange (X25519/ECDHE + ML-KEM concatenated) so a
  break in one scheme does not compromise the handshake during migration.
- Reviewing "quantum-safe" claims, key/ciphertext reuse, or PRNG usage in
  post-quantum code.
- Verifying an implementation against official known-answer tests (NIST ACVP,
  PQClean vectors) before integration.

## When not to use

- Purely classic crypto with no post-quantum or hybrid component (AES-GCM,
  standalone X25519) — out of scope; use the general crypto skills.
- TLS/SSH configuration where the stack's provider already negotiates PQ
  groups — verify the configuration instead of re-deriving the algorithm.
- Quantum key distribution (QKD) — a different discipline, not lattice-based.
- General timing-leak audits with no ML-KEM/ML-DSA component — use
  `side-channel-constant-time-verification`.
- Pure lattice theory (Module-LWE/SIS research) with no implementation concern.

## What the agent often gets wrong

1. Early-exit on invalid ciphertext in decapsulation — breaks the implicit
   rejection that FIPS 203 mandates and creates a decapsulation oracle
   (chosen-ciphertext attack); agents "optimize" by returning an error early.
2. Non-constant-time rejection sampling — an early-exit or branch on the
   sampled value leaks key material through timing (secret-dependent path
   length).
3. Non-constant-time comparisons or serialization — variable-time `memcmp` or
   early-exit equality checks on secret-dependent data.
4. Wrong parameters — q=3329, n=256, k=2/3/4 per parameter set (ML-KEM-768 is
   k=3); a wrong NTT or base-case multiplication gives silently wrong results.
5. Key/ciphertext reuse — ML-KEM is ephemeral by design; reusing an
   encapsulation key or re-encrypting the same message with the same seed
   leaks; KEMs must feed a key schedule (e.g., a TLS hybrid).
6. Non-CSPRNG randomness — using `rand()`/time seeds for keys or ephemerals.
7. Claiming "quantum-safe" without hybrid mode and known-answer-test
   verification.

## How to reason correctly

1. Use a vetted implementation (liboqs, PQClean, BoringSSL/OpenSSL providers)
   instead of writing ML-KEM from scratch; if you must implement, verify
   against official test vectors.
2. Implement decapsulation with implicit rejection: always compute a fallback
   value (derived from the ciphertext and a secret) and return it on
   mismatch — no branch that distinguishes valid/invalid at the API, no timing
   or return code that differs.
3. Make every secret-dependent operation constant-time: rejection sampling
   runs a fixed number of iterations (no early exit on accepted/rejected
   values); comparisons use XOR-accumulate folds.
4. Pair ML-KEM with an established scheme (X25519/ECDHE) in hybrid mode during
   migration; derive session keys from the combined secrets.
5. Generate keys and ephemerals with the platform CSPRNG; zeroize secrets
   after use.
6. Test with ACVP/PQClean known-answer vectors before integration.

## What to verify

- Rejection sampling is a fixed-iteration constant-time loop with no
  data-dependent control flow.
- Decapsulation never distinguishes an invalid ciphertext by timing or return
  code — it returns a derived value (implicit rejection).
- Parameters match the standard: q=3329, n=256, k per parameter set
  (ML-KEM-768: k=3); ML-DSA parameter sets 44/65/87.
- Implementation passes official known-answer tests (ACVP or PQClean vectors).
- Secrets are zeroized; all randomness comes from a CSPRNG.

## How to verify

Host-executable Python models (this repo, python 3.11):

```
python examples/good/ct_rejection_sampling.py
python examples/bad/branchy_rejection.py
python examples/good/implicit_rejection.py
python examples/bad/early_exit_decaps.py
python examples/good/ct_compare.py
python examples/bad/early_exit_compare.py
```

Recorded real output for this host is in `evals/README.md`. Documented target
commands (native libs not installed on this host):

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
```

## Where the knowledge comes from

- NIST FIPS 203 (ML-KEM) (https://csrc.nist.gov/pubs/fips/203/final)
- NIST FIPS 204 (ML-DSA) (https://csrc.nisp.gov/pubs/fips/204/final)
- ML-KEM spec notes & Kyber round-3 submission (https://pq-crystals.org/kyber/)
- PQClean implementations (https://github.com/PQClean/PQClean)
- liboqs (https://github.com/open-quantum-safe/liboqs)
- NIST ACVP test vectors

## Related skills

- `rust-crypto-primitives-safety` — constant-time claims and crypto
  implementation safety in Rust crates (recommend).
- `side-channel-constant-time-verification` — timing-leak audit, measurement,
  and assembly verification of constant-time code (require).
- `zeroize-constant-time` — safe wiping of keys and shared secrets (recommend).
- `side-channel-mitigation` — countermeasure selection for the channels
  post-quantum code can leak through (recommend).
- `rust-unsafe-reasoning` — safe wrappers around vetted C implementations such
  as liboqs/PQClean (recommend).

## Evaluation

- Synthetic: approve `good/ct_rejection_sampling.py` (fixed iterations),
  `good/implicit_rejection.py` (derived value on invalid ct),
  `good/ct_compare.py` (XOR-fold); flag `bad/branchy_rejection.py`
  (early-exit sampling) and `bad/early_exit_decaps.py` (decapsulation oracle).
- False-positive: correct implicit rejection must NOT be flagged as "returns a
  value on error"; a fixed-iteration sampling loop must NOT be flagged as
  "wasted iterations".
- Historical: Kyber/Dilithium round-3 to FIPS 203/204 changes — renamed
  functions, encapsulation randomness moved inside Encaps, decapsulation
  failure handling, parameter-set renames; agents trained on round-3
  specifications must update.
- Adversarial: a decapsulation that early-exits on mismatch to "save work"; a
  rejection sampler with an early-exit; static key reuse presented as
  quantum-safe; a "constant-time" claim never verified in assembly.
- Verified facts and real host output: `evals/README.md`.
