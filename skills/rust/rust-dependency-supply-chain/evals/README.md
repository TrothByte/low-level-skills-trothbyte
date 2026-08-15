# Evaluation — rust-dependency-supply-chain

Skill: `skills/rust/rust-dependency-supply-chain`. Stability target: `evaluated`.
Toolchain: cargo 1.97.1, Python 3.11.9, Windows. Network WAS available on
2026-08-15, so `cargo info`/`cargo add` outputs below are live registry
answers; the python proxy is the offline fallback.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/Cargo.toml` (`serde_jon`) | hallucinated name flagged | cargo info → exit 101 |
| easy/negative | `bad/Cargo.toml` (`chacha20poly`) | near-miss flagged | cargo info → exit 101 |
| medium/negative | `bad/Cargo.toml` (`tokio-utils-rs`) | near-miss of real `tokio-utils` flagged | cargo info → exit 101 |
| medium/positive | `good/verify_crate_names.py` | real names pass | 3 exact matches, exit 0 |
| hard/negative | `bad/hallucinated_deps.py` | generates Cargo.toml with no checks | exit 0, must still be flagged |
| hard/positive | `good/Cargo.toml` | real names, exact pins | `cargo info` exit 0 per name |

Detection rule: a name is accepted ONLY after an exact-name check
(`cargo info <name>` exit 0, or crates.io API 200). A name that only
"looks like" a real crate is a hallucination or typosquat until proven
otherwise.

## False-positive evals (correct code must NOT be flagged)

- Real crates `serde`, `tokio`, `chacha20poly1305`, `serde_json` — proxy
  reports exact match, not flagged.
- `good/Cargo.toml` exact pins (`=1.0.229`) — not flagged; exact pins are the
  recommended reproducibility pattern.
- `cargo search serde` returning real results — not an existence claim error.
- `tokio-utils` (real, obscure, 0.1.2) must NOT be called a hallucination —
  only verification decides; the proxy labels it "verify", and `cargo info`
  confirms existence (recorded).

## Historical evals

- arxiv-2406-10279: 5.2% commercial / 21.7% open-source generated
  dependencies are nonexistent packages — KNOWN dataset figures.
- arxiv-2606-08444: nonexistent crates resembling real ones, model-independent
  rate — KNOWN abstract-level figures; exact per-model rates UNVERIFIED here.
- The `tokio-utils` case (real but obscure) is the historical contrast: name
  plausibility is not existence, and popularity is not safety.

## Adversarial evals

- `chacha20poly` — missing `1305` suffix; `cargo info` → `error: could not
  find ... in registry` (exit 101) — must be caught.
- `cha-cha20-poly1305` — inserted hyphens, Levenshtein 2 from the real name —
  must be caught by the proxy and by `cargo info` (exit 101).
- `serde_jon` / `serde_jason` — one-character typos of `serde_json`; distance 1
  — must be caught by the proxy; `cargo search serde_jon` even returns fuzzy
  results, which is itself the trap: it must NOT be taken as existence.

## Verification commands (ACTUAL, recorded 2026-08-15)

```
python examples/good/verify_crate_names.py
  exit 0. 3 real names exact-match; 10/13 candidates flagged for verification.
  serde_jon -> NEAR-MISS of 'serde_json' (distance 1)
  serde-json -> NEAR-MISS of 'serde_json' (distance 1)
  cha-cha20-poly1305 -> NEAR-MISS of 'chacha20poly1305' (distance 2)
  chacha20poly -> NOT in list, closest 'chacha20poly1305' (distance 4)
  tokio-utils-rs -> NOT in list, closest 'tokio' (distance 9)

cargo search serde --limit 1
  exit 0: "serde = \"1.0.229\"    # A generic serialization/deserialization framework"

cargo search serde_jon --limit 3
  exit 0 BUT returns unrelated crates (podcast-api, tobu, extrude-licenses)
  -- fuzzy search, NOT existence proof

cargo info serde
  exit 0: "A generic serialization/deserialization framework", "version: 1.0.229"

cargo info serde_jon
  exit 101: "error: could not find `serde_jon` in registry"

cargo info chacha20poly
  exit 101: "error: could not find `chacha20poly` in registry"

cargo info tokio-utils
  exit 0: real crate 0.1.2 (github.com/wcygan/tokio-utils) -- exact check decides

cargo add serde_jon
  exit 101: "error: the crate `serde_jon` could not be found in registry index."

cargo add chacha20poly
  exit 101: "error: the crate `chacha20poly` could not be found in registry index."

python examples/bad/hallucinated_deps.py
  exit 0, prints a Cargo.toml containing serde_jon / chacha20poly /
  tokio-utils-rs with no verification -- flagged by review
```

## Verified facts

- `cargo search` is fuzzy: it returns results for nonexistent names
  (`serde_jon` → unrelated crates). Never treat it as existence proof.
- `cargo info <name>` and `cargo add <name>` are exact: both exit 101 with
  `could not be found` for nonexistent names.
- `tokio-utils` exists (0.1.2, real); `tokio-utils-rs` does not — a one-token
  suffix flips existence, confirming that name accuracy is binary.
- crates.io canonical-name normalization maps `serde-json` to the `serde_json`
  crate for lookup, but the canonical (underscore) name is what should be
  written.

## Target toolchains (absent, documented)

- cargo-deny (`cargo deny check`) and cargo-audit (`cargo audit`): not
  installed — documented target verification, network-dependent.
- Minimal-versions (`cargo +nightly update -Z minimal-versions`): nightly
  toolchain present but flag not exercised — UNVERIFIED locally.

## Scoring (for routing eval)

- precision: every flagged name maps to a real registry/API verdict or a
  Levenshtein near-miss rule in `references/supply-chain.md`.
- recall: all hallucinated names detected (exit 101, or proxy flag).
- FP-rate: real names and exact pins produce zero flags.
