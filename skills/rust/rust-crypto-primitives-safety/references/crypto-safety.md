# Rust Crypto Primitives Safety — Reference Rules

Knowledge layer for `rust-crypto-primitives-safety`. Format: RULE → WHY AI
GETS IT WRONG → CORRECT REASONING → EXAMPLE (bad) → COUNTEREXAMPLE (good) →
VERIFICATION → SOURCE. Uncertainty is marked KNOWN / INFERRED / UNVERIFIED.

All commands recorded against rustc/cargo 1.97.1, Windows. The ChaCha20
implementation in this skill is a self-written learning artifact, verified
against the published vector; production code must use rustcrypto/ring.

## 1. LLM crypto output is overwhelmingly broken — never invent primitives

- **RULE**: a study of LLM-generated crypto-Rust found only 23.3% of outputs
  compiled and 57% of the compiled outputs were vulnerable (nonce misuse,
  unauthenticated schemes, weak keys); chain-of-thought prompting made the
  failure 5× worse (arxiv-2604-27001). The correct primitives are maintained,
  audited crates: `aes-gcm` / `chacha20poly1305` from RustCrypto, or `ring`.
- **WHY AI GETS IT WRONG**: crypto correctness is non-negotiable and not
  plausible from prose: subtle requirements (nonce uniqueness, authentication,
  counter rules) dominate, and "it compiled and round-tripped" feels like
  success.
- **CORRECT REASONING**: treat any hand-rolled cipher as broken until proven
  otherwise against published vectors; default to an audited AEAD crate and
  only implement primitives as a test-vector exercise.
- **EXAMPLE** (bad): `examples/bad/invented_crypto.rs` — a stateless,
  key-only XOR keystream with no nonce and no authentication; identical
  plaintexts produce identical ciphertexts (recorded).
- **COUNTEREXAMPLE** (good): use `aes-gcm` / `chacha20poly1305` (rustcrypto)
  with a 96-bit nonce and associated data; verify the crate exists first:
  `cargo info aes-gcm` (target verification; network-dependent).
- **VERIFICATION**: the study figures are KNOWN from the abstract of
  arxiv-2604-27001; the invented-cipher example's determinism is demonstrated
  locally (`assert_eq!(c1, c2)` passes).
- **SOURCE**: arxiv-2604-27001; rustcrypto.

## 2. Nonce uniqueness is mandatory: reuse is catastrophic

- **RULE**: with a fixed key, every message must use a distinct nonce/IV.
  For ChaCha20 the 96-bit nonce differentiates messages and the counter
  differentiates blocks within a message (RFC 8439 §2.3); for AES-GCM the
  IV must be unique for each key (NIST SP 800-38D §8.3, §6.1.1 note 2). When
  a nonce is reused, the keystream is identical, so C1^C2 == P1^P2: the XOR
  of two ciphertexts leaks the XOR of the plaintexts.
- **WHY AI GETS IT WRONG**: agents treat the nonce as a decoration; code that
  resets the counter to 1 per message (or uses a fixed/derived nonce)
  compiles, runs, and round-trips, so the failure is invisible in tests.
- **CORRECT REASONING**: enumerate the (key, nonce, block-counter) space your
  code produces and prove each triple is used at most once; for many-message
  protocols prefer a monotonic counter over random nonces, and track used
  nonces in a set.
- **EXAMPLE** (bad): `examples/bad/nonce_reuse.rs` — two messages encrypted
  with the same key, same nonce, counter restarted at 1; the program prints
  `keystream reuse: C1^C2 == P1^P2 (46 bytes leaked)` and the embedded
  assertion holds (recorded).
- **COUNTEREXAMPLE** (good): `examples/good/nonce_catalogue.rs` — a
  `NonceCatalogue` tracks every issued nonce and rejects reuse:
  `reuse rejected: nonce 0x1122334455667788 already used` (recorded).
- **VERIFICATION**: both programs run above; the leak equation is a direct
  consequence of keystream identity (KNOWN — see RFC 8439 keystream model).
- **SOURCE**: nist-sp800-38d; rfc-8439; arxiv-2604-27001 (nonce reuse as the
  dominant LLM failure).

## 3. ChaCha20 counter semantics: message-level counter vs block counter

- **RULE**: ChaCha20's 32-bit block counter starts at 1 for the first block
  of each message (RFC 8439 §2.3, "the initial counter is 1"); the 96-bit
  nonce changes per message. Confusing the two — e.g. deriving the nonce from
  the counter, or resetting both per message — produces keystream reuse.
- **WHY AI GETS IT WRONG**: pseudocode from memory often shows counter=0 or
  omits the counter entirely; RFC 8439 explicitly uses initial counter 1.
- **CORRECT REASONING**: keep three separate values: constant key, per-message
  nonce (unique), per-block counter starting at 1. Any scheme that reuses
  (key, nonce, counter) collides.
- **EXAMPLE** (bad): encrypting every message with counter restart 1 and the
  same nonce — the entire keystream repeats (the `nonce_reuse` demo).
- **COUNTEREXAMPLE** (good): the vector check fixes counter=1 and nonce
  `00000009 0000004a 00000000` and asserts the RFC keystream.
- **VERIFICATION**: the RFC 8439 §2.3.2 vector check passes locally
  (`keystream[0..8] = 10f1e7e4d13b5915`); the vector values are KNOWN from
  the RFC.
- **SOURCE**: rfc-8439.

## 4. Verify against published test vectors

- **RULE**: correctness is established by known-answer tests, not by
  round-trips. RFC 8439 §2.3.2 publishes a ChaCha20 keystream vector (key
  `00..1f`, nonce `00000009 0000004a 00000000`, counter 1); NIST SP 800-38D
  (and GCM test vectors) cover AES-GCM.
- **WHY AI GETS IT WRONG**: agents write the cipher, then test with
  encrypt→decrypt and declare success — the same bug on both sides passes.
- **CORRECT REASONING**: embed the published vector as a unit test with the
  expected hex; any deviation means the implementation (or your embed of the
  vector) is wrong.
- **EXAMPLE** (bad): checking `decrypt(encrypt(x)) == x` only.
- **COUNTEREXAMPLE** (good):
  ```rust
  let keystream = block(&key, 1, &nonce);
  assert_eq!(to_hex(&keystream), "10f1e7e4d13b5915500fdd1fa32071c4...");
  ```
- **VERIFICATION**: `rustc --edition 2021 examples/good/chacha20_vector.rs`
  exits 0 and prints `RFC 8439 section 2.3.2 test vector: PASS` (recorded).
- **SOURCE**: rfc-8439 (§2.3.2); nist-sp800-38d.

## 5. Authenticate the ciphertext (encrypt-then-authenticate / AEAD)

- **RULE**: confidentiality without integrity is malleable: an attacker can
  flip ciphertext bits and corrupt the plaintext undetectably. AEAD modes
  (ChaCha20-Poly1305, AES-GCM) authenticate the ciphertext and associated
  data in one primitive; NIST SP 800-38D specifies GCM's authentication tag.
- **WHY AI GETS IT WRONG**: agents implement stream-XOR encryption and call
  it done; integrity is never mentioned in the prompt they were trained on.
- **CORRECT REASONING**: use an AEAD; bind protocol context (version, sender,
  length) as associated data; verify the tag before using the plaintext.
- **EXAMPLE** (bad): `invented_crypto.rs` — XOR-only keystream with no tag.
- **COUNTEREXAMPLE** (good): `chacha20poly1305::ChaCha20Poly1305::encrypt`
  with `Aad` + the nonce and tag checked on decrypt.
- **VERIFICATION**: the malleability claim is KNOWN (AEAD definition in
  RFC 8439 and NIST SP 800-38D); local demonstration is the vector test on
  the ChaCha20 block only (the AEAD construction is target verification with
  the crate).
- **SOURCE**: rfc-8439; nist-sp800-38d; rustcrypto.

## 6. Crypto API usage must be verified, not recalled

- **RULE**: crypto crates are subject to the same API drift and hallucination
  as everything else (see `rust-api-evolution-and-drift`): a remembered
  `aes_gcm::encrypt(key, iv)` signature may not exist in the pinned version.
  Verify the name and version with `cargo info`, read the crate docs, and let
  `cargo check` confirm the call.
- **WHY AI GETS IT WRONG**: crypto API details are high-entropy and rarely in
  training data correctly; combined with arxiv-2604-27001's 23.3% compile
  rate, most recalled calls do not compile.
- **CORRECT REASONING**: `cargo info aes-gcm` (exit 0 + current version),
  then `cargo add aes-gcm` and let `cargo check` validate each call.
- **EXAMPLE** (bad): writing `chacha20poly::encrypt(key, nonce, plaintext)`
  from memory for a crate named `chacha20poly1305`.
- **COUNTEREXAMPLE** (good): `cargo info chacha20poly1305` → real crate
  (recorded, exit 0, version 0.11.0); use its documented `encrypt`/`decrypt`
  with a `Nonce` type.
- **VERIFICATION**: `cargo info chacha20poly1305` exit 0 recorded; the crate
  itself is not fetched/built locally (network-dependent target verification).
- **SOURCE**: rustcrypto; arxiv-2505-05057 (API-hallucination metrics).

## Quick reference table

| Primitive | Nonce size | Counter start | Reuse consequence | Normative source |
|---|---|---|---|---|
| ChaCha20 | 96-bit per message | 1 (per block) | Keystream reuse → C1^C2 = P1^P2 | RFC 8439 §2.3 |
| ChaCha20-Poly1305 | 96-bit | 1 | Total loss of confidentiality | RFC 8439 §2.8 |
| AES-GCM | 96-bit IV | n/a (IV unique per key) | Catastrophic (SP 800-38D §8.3) | NIST SP 800-38D |
| Hand-rolled XOR/LCG | none | n/a | Insecure by design | arxiv-2604-27001 |
