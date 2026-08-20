# ML-KEM / ML-DSA — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE (URL). Uncertainty is marked KNOWN / INFERRED /
UNVERIFIED per repository policy.

## 1. Know the parameter sets and the round-3 → FIPS renames

- **RULE**: ML-KEM uses modulus q=3329, ring degree n=256, and matrix width
  k in {2,3,4} for ML-KEM-512/768/1024 (ML-KEM-768 is the FIPS-203 default).
  ML-DSA has parameter sets 44/65/87. Function and API names changed from the
  CRYSTALS round-3 submissions (Kyber, Dilithium).
- **WHY AI GETS IT WRONG**: training data predates FIPS 203/204; agents reach
  for Kyber-512/768/1024 and Dilithium-2/3/5 names, or for the round-3
  `crypto_kem_*` / `crypto_sign_*` API, and copy parameters that no longer
  match the standard.
- **CORRECT REASONING**: map old → new: Kyber-768 → ML-KEM-768 (k=3),
  Dilithium-3 → ML-DSA-65. FIPS 203 Encaps generates its own randomness
  internally (the round-3 variant took it as input); FIPS 204 renames
  `ExpandA`, `MatrixExpand`, and the sampling functions. Use the FIPS names
  and the FIPS parameter table.
- **EXAMPLE** (bad): `mlkem768_encaps(seed, ek)` passing an external seed —
  the FIPS 203 API draws `r` from the CSPRNG inside Encaps.
- **COUNTEREXAMPLE** (good): `mlkem768_keygen(ek, dk)`,
  `mlkem768_encaps(ct, ss, ek)`, `mlkem768_decaps(ss, ct, dk)` with FIPS 203
  semantics.
- **VERIFICATION**: compare key/ciphertext lengths against FIPS 203 tables
  (ML-KEM-768: ek 1184 bytes, dk 2400 bytes, ct 1088 bytes); run the ACVP or
  PQClean known-answer tests.
- **SOURCE**: KNOWN — FIPS 203, FIPS 204 (URLs below); INFERRED — exact
  byte-length tables (verify against the standard PDF).

## 2. Rejection sampling must be constant-time

- **RULE**: conversion of SHAKE output into ring elements (values < q) must
  run a FIXED number of iterations. The loop must not exit early when enough
  coefficients are collected, and acceptance/rejection must not branch on the
  sampled value in a way that changes path length.
- **WHY AI GETS IT WRONG**: the natural implementation is
  `while len(coeffs) < n: ...` — correct output, but iteration count and
  execution time depend on the sampled values, which derive from secret
  seeds. A timing attacker learns how often rejection occurred.
- **CORRECT REASONING**: process a fixed number of candidate blocks; store
  accepted values with an arithmetic select (mask-based write) and advance a
  fill counter only on acceptance. Work per input is identical.
- **EXAMPLE** (bad): `bad/branchy_rejection.py` — `while len(coeffs) < n`.
- **COUNTEREXAMPLE** (good): `good/ct_rejection_sampling.py` — loop bound is
  fixed; iteration count identical for all-rejected and all-accepted streams.
- **VERIFICATION**: count iterations for a "lucky" (all accepted) and
  "unlucky" (mostly rejected) stream — must be identical. On the target:
  `objdump -d` of the sampling loop at `-O2` (see
  `side-channel-constant-time-verification`).
- **SOURCE**: FIPS 203 Section 4.3 (Sampling of Polynomials); Kyber round-3
  submission.

## 3. Decapsulation must implement implicit rejection

- **RULE**: FIPS 203 requires that decapsulation of an invalid ciphertext
  returns a pseudorandom shared secret derived from the ciphertext and a
  secret value (not an error). No API path, timing, or return value may reveal
  whether the ciphertext was valid.
- **WHY AI GETS IT WRONG**: agents "optimize" by checking validity first and
  returning an error or `None` — this is a decapsulation oracle: an attacker
  who queries decapsulation and observes success/failure (or timing) learns
  validity, enabling chosen-ciphertext attacks.
- **CORRECT REASONING**: always decrypt and always derive both candidate
  values; combine with arithmetic selection so every input takes the same
  path and returns 32 bytes. Re-encryption check still runs, but its result
  only selects which derivation is returned.
- **EXAMPLE** (bad): `bad/early_exit_decaps.py` — `if not check_valid(ct):
  return None`.
- **COUNTEREXAMPLE** (good): `good/implicit_rejection.py` — same operations
  for valid and invalid inputs, derived value returned either way.
- **VERIFICATION**: run `bad/early_exit_decaps.py` (must be flagged) and
  `good/implicit_rejection.py` (must pass); feed an invalid ciphertext to the
  implementation and assert a 32-byte secret is still returned.
- **SOURCE**: FIPS 203 Section 7.2 (Decapsulation, implicit rejection);
  Kyber round-3 "ImplicitReject".

## 4. ML-KEM is ephemeral — hybrid mode during migration

- **RULE**: ML-KEM keys are for ephemeral key exchange. Static KEM keys and
  ciphertext/seed reuse break confidentiality. During migration, concatenate
  classical and PQ shared secrets (e.g., X25519 + ML-KEM-768) and feed both
  into a KDF so a break in one scheme does not compromise the handshake.
- **WHY AI GETS IT WRONG**: agents reuse a single encapsulation key across
  connections or re-encrypt the same message with the same seed, treating the
  KEM like a public-key encryption scheme. ML-KEM guarantees only
  key-encapsulation freshness, and the security of re-encryption is a
  separate, easily-misused property.
- **CORRECT REASONING**: generate ephemeral keypairs per session, keep
  ciphertexts single-use, and derive session keys as
  `KDF(ss_classical || ss_pq, transcript)`. Hybrid groups such as
  `X25519MLKEM768` (draft-ietf-tls-hybrid-design) standardize this.
- **EXAMPLE** (bad): a server that caches one ML-KEM-768 keypair and answers
  encapsulations with it for all clients.
- **COUNTEREXAMPLE** (good): fresh ephemeral ML-KEM keypair per handshake,
  hybrid-derived session key, secrets zeroized after use.
- **VERIFICATION**: openssl 3 + OQS provider serving the `X25519MLKEM768`
  TLS group; review the key schedule for the concatenation-and-KDF pattern.
- **SOURCE**: KNOWN — hybrid design drafts (draft-ietf-tls-hybrid-design),
  liboqs; INFERRED — exact group negotiation semantics on a target OpenSSL
  build (UNVERIFIED on this host).

## 5. Verify against known-answer tests

- **RULE**: an implementation is not trusted until it matches official test
  vectors: NIST ACVP vectors or the PQClean test vectors for the same
  parameter set.
- **WHY AI GETS IT WRONG**: agents generate implementations and trust that
  "it compiles and interoperates with itself" — self-consistent but wrong
  implementations (bad NTT, wrong q, wrong padding) pass their own round-trip
  and fail only against the vectors.
- **CORRECT REASONING**: run the ACVP test harness or PQClean's KAT tests
  (which use deterministic randomness and SHAKE of known seeds). A round-trip
  (keygen → encaps → decaps) is necessary but not sufficient.
- **EXAMPLE** (bad): shipping an ML-KEM implementation validated only by a
  self-round-trip.
- **COUNTEREXAMPLE** (good): `ctest` over liboqs KATs, or PQClean
  `make test` for `crypto_kem/ml-kem-768/m4clean`.
- **VERIFICATION**: see "Verification commands (target)" in `evals/README.md`.
- **SOURCE**: KNOWN — NIST ACVP; PQClean README and test harness.

## 6. Randomness and zeroization

- **RULE**: key generation and encapsulation randomness must come from the
  platform CSPRNG; private keys, ciphertexts' internal states, and derived
  secrets must be zeroized after use.
- **WHY AI GETS IT WRONG**: agents seed from `rand()`/`time()` or keep
  secrets in long-lived buffers; ML-KEM's security relies on fresh high-
  entropy randomness for every encapsulate.
- **CORRECT REASONING**: use the OS CSPRNG (`getrandom`, `rand_os`), never
  seed from the clock; wipe secrets with `zeroize`-style volatile writes (see
  `zeroize-constant-time`).
- **EXAMPLE** (bad): `srand(time(NULL))` before `mlkem768_keygen`.
- **COUNTEREXAMPLE** (good): platform CSPRNG for `d`, `z`, and encaps
  randomness; secret buffers zeroized.
- **VERIFICATION**: code review for seed sources; `zeroize-constant-time`
  checks on secret buffers.
- **SOURCE**: FIPS 203 Section 3 (Randomness); `zeroize-constant-time`
  skill; liboqs/PQClean example drivers.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Parameters | q=3329, n=256, k=2/3/4; ML-KEM-768 is the default; ML-DSA 44/65/87 |
| Names | use FIPS names and API (round-3 Kyber/Dilithium names are stale) |
| Sampling | rejection sampling: fixed iteration count, no early exit |
| Decaps | implicit rejection: always return a derived 32-byte value |
| Reuse | ephemeral keys; never reuse seeds; single-use ciphertexts |
| Hybrid | X25519/ECDHE + ML-KEM concatenated into a KDF during migration |
| Randomness | CSPRNG only; zeroize keys and secrets |
| Tests | ACVP or PQClean known-answer vectors before integration |

## Primary sources

- NIST FIPS 203 (ML-KEM) — https://csrc.nist.gov/pubs/fips/203/final
- NIST FIPS 204 (ML-DSA) — https://csrc.nisp.gov/pubs/fips/204/final
- Kyber round-3 submission / spec notes — https://pq-crystals.org/kyber/
- PQClean — https://github.com/PQClean/PQClean
- liboqs — https://github.com/open-quantum-safe/liboqs
- NIST ACVP test vectors (NIST ACVP site)
