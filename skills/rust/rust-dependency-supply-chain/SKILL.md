---
name: rust-dependency-supply-chain
description: Use when choosing or adding a dependency: crate names are hallucinated at 5.2-21.7%, typosquats and near-misses abound. Teaches exact-name verification (cargo info, crates.io API), Levenshtein near-miss checks, cargo-deny/audit, and minimal version pinning.
---

# Rust Dependency Supply Chain

## When to use

- Adding a dependency to `Cargo.toml`, either by hand or from generated code.
- Reviewing a `Cargo.toml` written by a model or another engineer for
  nonexistent or look-alike crate names.
- Choosing a version specifier (exact pin vs caret) for reproducibility.
- Auditing the dependency tree before release (licenses, advisories, bans).

## When not to use

- API drift of an already-chosen crate — use `rust-api-evolution-and-drift`.
- Cryptography primitive selection — use `rust-crypto-primitives-safety`.
- Publishing / packaging your own crate — this skill is about consumption.

## What the agent often gets wrong

- "The crate name I remember must exist." Crate hallucinations are
  well-documented: 5.2% of commercial and 21.7% of open-source LLM-generated
  dependencies are nonexistent packages (arxiv-2406-10279); a dedicated Rust
  study confirmed nonexistent crates that resemble real ones, at a
  model-independent rate (arxiv-2606-08444).
- Hyphen vs underscore is treated as cosmetic — on crates.io it is part of
  the canonical name; `serde-json` and `serde_json` are different names, and
  the misspelled look-alike is exactly what a typosquat takes.
- "`cargo search` returned results, so the crate exists." `cargo search` is
  a fuzzy keyword search: it returns results for nonexistent names (e.g.
  `serde_jon` returns unrelated crates). Exact-name checks are `cargo info`
  or the crates.io exact API.
- "A version spec like `^1` is fine." It silently allows major-breaking
  (for `0.x`) and behavior-changing updates; reproducibility demands exact
  pins or a lockfile plus `--locked` CI.
- "cargo-deny/audit are optional extras." They are the standing gates
  against known-vulnerability and malicious-typosquat crates entering the
  tree.
- "If it compiles, the dependency is safe." A compiled dependency still may
  be the wrong (look-alike) crate with malicious or vulnerable code.

## How to reason correctly

1. Verify exact existence before writing the name anywhere:
   `cargo info <name>` (exit 101 = not found) or the crates.io API
   `GET /api/v1/crates/<name>` (200 = exists, 404 = does not).
   Treat `cargo search` output as a hint, never as existence proof.
2. For every name, run a near-miss comparison (Levenshtein ≤ 2) against the
   real crate list; hyphen/underscore variants and single-character typos
   are the typosquat signature.
3. Write the version spec deliberately: exact `=1.0.229` pins for releases,
   caret `1.2` with a lockfile + `--locked` for app builds; never leave the
   spec to the resolver.
4. Add `cargo-deny` to CI with `advisories` (OSV), `bans` (known bad crates),
   and `licenses` sections; run `cargo audit` on the lockfile.
5. Prefer small, maintained, popular crates; check download counts and
   last-update dates via the crates.io API before adopting an unknown crate.
6. When a crate is missing, do NOT improvise a close name — re-prompt with
   the verified name or use a std-only implementation.

## What to verify

- Every dependency name passes an exact-name check (`cargo info` exit 0).
- No candidate name is within Levenshtein distance ≤ 2 of a different real
  crate (or is flagged for review).
- `cargo add <name>` succeeds for every name (final machine gate).
- The lockfile is checked in and CI uses `--locked`.
- `cargo deny check` and `cargo audit` are clean (target verification).

## How to verify

```
python examples/good/verify_crate_names.py   # exact match + Levenshtein proxy
cargo search <name> --limit 3                # fuzzy — evidence only
cargo info <name>                            # exact-name existence, exit 101 = not found
cargo add <name>                             # final gate; error: could not be found
cargo deny check                             # target verification (not installed locally)
cargo audit                                  # target verification (not installed locally)
```

## Where the knowledge comes from

- arxiv-2606-08444 — nonexistent crates resembling real ones, model-independent
  hallucination rate.
- arxiv-2406-10279 — 5.2% commercial / 21.7% open-source package
  hallucinations.
- arxiv-2505-05057 — MiHN/MaHR API-hallucination metrics and
  dependency-aware decoding.
- crates-io-api — `/api/v1/crates/<name>` exact-name endpoint and
  `cargo search`/`cargo info` semantics.
- cargo-deny — advisories/bans/license gates.

## Related skills

- `rust-api-evolution-and-drift` — signature/version drift of real crates
  (recommend)
- `rust-crypto-primitives-safety` — crypto crates are a favorite typosquat
  target (recommend)
- `rust-unsafe-reasoning` — auditing a suspicious dependency's unsafe code
  (cross-link)

## Evaluation

- Synthetic: hallucinated and near-miss names in `examples/bad` must be
  flagged; real names in `examples/good` must pass.
- False-positive: real crates (`serde`, `tokio`, `chacha20poly1305`) and
  correctly-pinned versions must NOT be flagged.
- Historical: documented package-hallucination datasets (5.2%/21.7%) and the
  Rust-specific crate-hallucination study as ground truth for the problem.
- Adversarial: `chacha20poly` (missing suffix), `cha-cha20-poly1305`
  (inserted hyphens) and `serde_jon` (typo) must each be caught by the proxy
  and by `cargo info`/`cargo add`.
- Commands and recorded results: `evals/README.md`.
