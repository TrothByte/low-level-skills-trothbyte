---
name: rust-crypto-primitives-safety
description: Use when writing, reviewing, or auditing Rust cryptography: selecting AEAD primitives, nonces, key handling, or hand-rolled ciphers. Prevents nonce reuse, invented algorithms, and API hallucination — 57% of LLM-compiled crypto is vulnerable.
---

# Rust Crypto Primitives Safety

## When to use

- Writing or reviewing code that encrypts, authenticates, hashes, or
  key-derives: AEADs (ChaCha20-Poly1305, AES-GCM), stream ciphers, MACs.
- Choosing a nonce/IV scheme for a new protocol message format.
- Auditing generated or vendored crypto for nonce reuse and invented
  algorithms.
- Verifying a crypto implementation against published test vectors.

## When not to use

- Pure hashing for data structures (not security) — `HashMap` is fine.
- TLS/HTTPS plumbing — use a maintained client; this skill covers primitives.
- Generic supply-chain checks — use `rust-dependency-supply-chain`.
- Constant-time side-channel reasoning — see the side-channel domain.

## What the agent often gets wrong

- "Crypto is just XOR with a key." A study of LLM-generated crypto-Rust found
  only 23.3% of outputs compiled, and 57% of the compiled ones were vulnerable
  (arxiv-2604-27001); chain-of-thought prompting made it 5× worse. Crypto
  correctness is not plausible-looking code.
- "I invented a secure cipher." A stateless, key-only XOR/LCG keystream with
  no nonce and no authentication is broken by design; it compiles and runs.
- "Nonce reuse just hurts randomness." Nonce reuse in a stream/AEAD is
  catastrophic: identical keystreams mean C1^C2 == P1^P2 — the XOR of two
  ciphertexts leaks the XOR of the plaintexts (demonstrated in the bad
  example).
- "A counter that resets per message is fine." If the counter restarts at the
  same value with the same key, every message reuses the keystream; the nonce
  is the per-message differentiator, not the key.
- "The API call I remember is the right one." Crypto crate APIs hallucinate
  like any other (e.g. `aes_gcm::encrypt(key, iv)` signatures from memory);
  check `cargo info` and the docs before writing the call.
- "It passed my test, so it is secure." A round-trip test proves nothing about
  nonce handling, authentication, or side channels; only known-answer vectors
  plus a reviewed protocol matter.

## How to reason correctly

1. Never invent primitives: use maintained, audited crates
   (`aes-gcm`, `chacha20poly1305` from RustCrypto, or `ring`). Hand-rolled
   crypto is only acceptable as a learning exercise with a test vector.
2. State the nonce contract per primitive before writing code: ChaCha20 /
   ChaCha20-Poly1305 uses a 96-bit nonce, counter starting at 1 (RFC 8439
   §2.3); AES-GCM uses a 96-bit IV with a uniqueness requirement
   (NIST SP 800-38D §8.3). Reuse under the same key is a catastrophic failure.
3. Derive nonces from a counter or a CSPRNG, track them, and refuse reuse
   (single message per key+nonce pair); a persistent counter is safer than
   random for many-message protocols.
4. Encrypt-then-authenticate (or use an AEAD); authenticate the associated
   data (protocol version, message length).
5. Test against published vectors: RFC 8439 §2.3.2 keystream vector, NIST
   GCM test vectors; embed them as unit tests, never eyeball-test.
6. Treat key handling as part of the design: 256-bit keys from a CSPRNG,
   `zeroize` on drop, no keys in logs or in code.
7. Check the real API surface: `cargo info aes-gcm`, read the crate docs;
   never call functions that don't exist in the pinned version.

## What to verify

- The primitive is a real, maintained crate — `cargo info <name>` exit 0 and
  the version is current (target verification).
- Every message uses a distinct (key, nonce) pair; no counter resets to a
  reused value. A nonce-tracker test passes.
- Known-answer tests pass: ChaCha20 block matches the RFC 8439 §2.3.2 vector;
  AES-GCM matches NIST SP 800-38D vectors (via the crate's own tests).
- No hand-rolled cipher, PRNG-based keystream, or unauthenticated scheme is
  present.
- Key material is never printed, logged, or hardcoded; `zeroize` covers it.

## How to verify

```
rustc --edition 2021 examples/good/chacha20_vector.rs -o v.exe && ./v.exe
  # prints "RFC 8439 section 2.3.2 test vector: PASS"
rustc --edition 2021 examples/good/nonce_catalogue.rs -o n.exe && ./n.exe
  # nonce reuse rejected
rustc --edition 2021 examples/bad/nonce_reuse.rs -o r.exe && ./r.exe
  # prints the C1^C2 == P1^P2 leak — this is the attack
cargo info aes-gcm            # exact-name check of the primitive crate
cargo add aes-gcm             # target: add the real crate, then cargo test
```

## Where the knowledge comes from

- arxiv-2604-27001 — crypto-Rust codegen study: 23.3% compile, 57% of compiled
  vulnerable, nonce reuse as top bug class, CoT 5× worse.
- NIST SP 800-38D — GCM mode, IV/nonce uniqueness requirements (§8.3).
- RFC 8439 — ChaCha20 and Poly1305, AEAD construction, 96-bit nonce, counter
  from 1, test vectors in §2.3.2.
- rustcrypto — audited pure-Rust `aes-gcm`, `chacha20poly1305` crates;
  production code must use these or `ring`, not hand-rolled primitives.

## Related skills

- `rust-dependency-supply-chain` — crypto crate names are a typosquat target
  (recommend)
- `rust-api-evolution-and-drift` — pin the crate version; API drift in crypto
  is security-relevant (recommend)
- `rust-unsafe-reasoning` — vendored crypto often contains `unsafe` (cross-link)

## Evaluation

- Synthetic: nonce reuse and invented-crypto examples must be flagged; the
  RFC-vector check must pass.
- False-positive: correct nonce management and a vector-passing ChaCha20
  implementation must NOT be flagged.
- Historical: the 23.3%/57% study figures as the known problem baseline.
- Adversarial: nonce-reuse code that compiles, runs, and "works" in tests
  must still be caught by keystream-reuse reasoning.
- Commands and recorded results: `evals/README.md`.
