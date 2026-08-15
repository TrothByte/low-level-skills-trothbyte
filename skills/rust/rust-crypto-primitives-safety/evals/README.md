# Evaluation — rust-crypto-primitives-safety

Skill: `skills/rust/rust-crypto-primitives-safety`. Stability target:
`evaluated`. Toolchain: rustc 1.97.1, Windows x86_64-msvc.
All outputs below were actually produced by running the examples in this
skill on 2026-08-15.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/nonce_reuse.rs` | keystream reuse detected | exit 0, prints leak |
| easy/negative | `bad/invented_crypto.rs` | invented primitive flagged | exit 0, must be flagged |
| medium/positive | `good/chacha20_vector.rs` | RFC 8439 §2.3.2 vector passes | exit 0, PASS |
| medium/positive | `good/nonce_catalogue.rs` | reuse rejected, distinct nonces ok | exit 0 |
| hard/negative | nonce-reuse reasoning | must explain C1^C2 == P1^P2 | assertion in bad example holds |

Detection rule: a scheme is unsafe if (a) it is a hand-rolled primitive
(invented), (b) any (key, nonce) pair is used for more than one message,
(c) the counter restarts to a reused value, or (d) the plaintext is not
authenticated. "It round-trips" is never a pass.

## False-positive evals (correct code must NOT be flagged)

- `good/chacha20_vector.rs` — a self-written ChaCha20 block function whose
  output equals the published vector. Not flagged: correct implementation
  backed by a known-answer test. (Production still must use rustcrypto/ring —
  the local implementation is a learning artifact only.)
- `good/nonce_catalogue.rs` — correct nonce management: distinct nonces
  issued, reuse rejected with an error. Not flagged.
- A counter that increments across blocks of ONE message (RFC 8439 model) is
  correct and must NOT be flagged as reuse.
- An AEAD usage where each message gets a fresh 96-bit nonce is correct.

## Historical evals

- arxiv-2604-27001 (KNOWN, abstract-level): 23.3% of LLM crypto-Rust
  compiled; 57% of compiled were vulnerable; chain-of-thought made it 5×
  worse. The dominant bug class is nonce misuse.
- The RFC 8439 §2.3.2 vector (KNOWN, normative): key 00..1f, nonce
  `00000009 0000004a 00000000`, counter 1 → keystream
  `10f1e7e4d13b5915...a2503c4e`. Reproduced byte-for-byte locally.
- NIST SP 800-38D (KNOWN, normative): GCM IV uniqueness requirement (§8.3).

## Adversarial evals

- `bad/nonce_reuse.rs` compiles, runs, and its ciphertexts "work" for
  decrypt-anyone — naive tests pass. Must be caught by keystream-reuse
  reasoning: identical (key, nonce, counter) → identical keystream →
  C1^C2 = P1^P2 (46 bytes leaked, asserted locally).
- `bad/invented_crypto.rs` asserts `c1 == c2` — a "stability" property that
  is actually the vulnerability signature. Must be recognized as such.
- Near-miss crate name `chacha20poly` (missing `1305`) is covered by
  `rust-dependency-supply-chain`; cross-check it before adding any crypto
  dependency.

## Verification commands (ACTUAL, recorded 2026-08-15)

```
rustc --edition 2021 examples/good/chacha20_vector.rs -o v.exe && ./v.exe
  exit 0
  keystream[0..8] = 10f1e7e4d13b5915
  RFC 8439 section 2.3.2 test vector: PASS

rustc --edition 2021 examples/good/nonce_catalogue.rs -o n.exe && ./n.exe
  exit 0
  issued: 0x1122334455667788 0x1122334455667789
  reuse rejected: nonce 0x1122334455667788 already used

rustc --edition 2021 examples/bad/nonce_reuse.rs -o r.exe && ./r.exe
  exit 0
  keystream reuse: C1^C2 == P1^P2 (46 bytes leaked)

rustc --edition 2021 examples/bad/invented_crypto.rs -o i.exe && ./i.exe
  exit 0
  no nonce, deterministic keystream: identical plaintexts give identical ciphertexts

cargo info chacha20poly1305
  exit 0: real crate, version 0.11.0 (recorded 2026-08-15)
cargo info chacha20poly
  exit 101: "error: could not find `chacha20poly` in registry"
```

## Verified facts

- The ChaCha20 implementation in `chacha20_vector.rs` reproduces the RFC 8439
  §2.3.2 keystream exactly — a real known-answer test, not a round-trip.
- Nonce reuse leaks the plaintext XOR deterministically: the embedded
  `assert_eq!(xor_pt, xor_ct)` passes.
- `chacha20poly1305` exists (0.11.0); `chacha20poly` does not — the missing
  suffix is a real near-miss vector.

## Target verification (absent, documented)

- rustcrypto crates (`aes-gcm`, `chacha20poly1305`) are not fetched/built
  locally (network-dependent); `cargo info` checks the names, and production
  code must use them (or `ring`) with their own test-vector suites.
- NIST SP 800-38D GCM test vectors: the AEAD-level check is target
  verification with the `aes-gcm` crate.

## Scoring (for routing eval)

- precision: every flagged scheme maps to a rule in
  `references/crypto-safety.md`.
- recall: nonce reuse, counter-reset reuse, invented primitives, and
  unauthenticated schemes detected.
- FP-rate: vector-passing implementations and correct nonce management
  produce zero flags.
