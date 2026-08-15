# Rust Dependency Supply Chain — Reference Rules

Knowledge layer for `rust-dependency-supply-chain`. Format: RULE → WHY AI GETS
IT WRONG → CORRECT REASONING → EXAMPLE (bad) → COUNTEREXAMPLE (good) →
VERIFICATION → SOURCE. Uncertainty is marked KNOWN / INFERRED / UNVERIFIED.

Commands recorded against cargo 1.97.1, Python 3.11.9, Windows. Network was
available on 2026-08-15, so `cargo info` / `cargo add` outputs are real; the
python proxy is the offline stand-in.

## 1. Crate names are hallucinated at a measurable rate

- **RULE**: LLM-generated dependencies include nonexistent packages. A study
  of package hallucinations across ecosystems measured 5.2% for commercial
  and 21.7% for open-source generated dependencies (arxiv-2406-10279); a
  Rust-specific study found nonexistent crates that resemble real ones, at a
  model-independent rate (arxiv-2606-08444). The failure is structural, not a
  bug in one model.
- **WHY AI GETS IT WRONG**: a plausible-looking name is statistically likely
  to be remembered as real; the model has no existence oracle.
- **CORRECT REASONING**: existence is a machine-checkable fact. Never let a
  name pass from memory into `Cargo.toml` without an exact-name check.
- **EXAMPLE** (bad): `serde_jon = "1.0"` — a typo of `serde_json`; no such
  crate exists (recorded: `cargo info serde_jon` → error, exit 101).
- **COUNTEREXAMPLE** (good):
  ```
  cargo info serde_json       # exit 0 -> real crate, version shown
  ```
- **VERIFICATION**: `cargo info <name>` exit 101 = not in registry; exit 0 =
  exists. `cargo add <name>` reproduces the same verdict as the last gate
  (recorded: `cargo add serde_jon` → `error: the crate `serde_jon` could not
  be found in registry index.`, exit 101).
- **SOURCE**: arxiv-2606-08444; arxiv-2406-10279; crates-io-api.

## 2. `cargo search` is fuzzy; exact-name checks are `cargo info` / API

- **RULE**: `cargo search <name>` matches terms in names AND descriptions and
  always returns results for near-any query; it proves nothing about a
  specific name. `cargo info <name>` and the crates.io endpoint
  `GET /api/v1/crates/<name>` (200/404) are exact-name lookups.
- **WHY AI GETS IT WRONG**: agents run `cargo search`, see output, and
  conclude the crate exists. This is the classic false positive.
- **CORRECT REASONING**: use `cargo search` only to find real crates by
  keyword, then confirm the exact name with `cargo info` before adding.
- **EXAMPLE** (bad): `cargo search serde_jon --limit 3` returns
  `podcast-api`, `tobu`, `extrude-licenses` (description matches) — yet
  `serde_jon` does not exist (recorded).
- **COUNTEREXAMPLE** (good): `cargo info serde` prints
  `A generic serialization/deserialization framework, version: 1.0.229`
  (recorded); `cargo info serde_jon` prints `error: could not find
  `serde_jon` in registry`, exit 101 (recorded).
- **VERIFICATION**: the three commands above, all recorded 2026-08-15.
- **SOURCE**: crates-io-api; cargo docs (INFERRED mapping of `cargo search`
  match semantics to the API search endpoint).

## 3. Typosquat and near-miss detection (Levenshtein proxy)

- **RULE**: look-alike names differ by typo, hyphen/underscore, plural, or a
  prefix/suffix (`serde_jon`, `chacha20poly`, `cha-cha20-poly1305`,
  `tokio-utils-rs`). The offline proxy in this skill flags any candidate within
  Levenshtein distance ≤ 2 of a real name for human review; exact matches
  pass.
- **WHY AI GETS IT WRONG**: humans and models both read `chacha20poly` as
  "basically `chacha20poly1305`"; a package registry does not — every name is
  exact, and squatted near-misses are a documented attack pattern.
- **CORRECT REASONING**: treat every non-exact name as suspicious until
  verified; the closer the distance, the more suspicious (a squat must look
  real to work).
- **EXAMPLE** (bad):
  ```
  serde_jon            -> NEAR-MISS of 'serde_json' (distance 1)
  chacha20poly         -> NOT in list, closest 'chacha20poly1305' (distance 4)
  ```
- **COUNTEREXAMPLE** (good):
  ```
  serde                -> REAL (exact match in list)
  chacha20poly1305     -> REAL (exact match in list)
  ```
- **VERIFICATION**: `python examples/good/verify_crate_names.py` flagged
  10/13 candidates for verification, 3 real ones passed — recorded. Distance
  values are deterministic for the hardcoded lists (INFERRED as representative
  of the typosquat class; exact distances are KNOWN for these strings).
- **SOURCE**: arxiv-2606-08444 (resemblance is the mechanism); crates-io-api.

## 4. Hyphen/underscore is part of the canonical name

- **RULE**: crates.io normalizes the *initial* name to lowercase and
  underscores, but afterwards the published name is what it is; `cargo`
  resolves dependencies by exact name. `serde-json` is not `serde_json`; a
  squatted `-`/`_` variant is a real supply-chain vector.
- **WHY AI GETS IT WRONG**: models transcribe names from prose where the
  separator was not quoted exactly.
- **CORRECT REASONING**: always copy the name from a machine source
  (`cargo search` result line, docs.rs URL, registry) and verify with
  `cargo info` before adding.
- **EXAMPLE** (bad): `serde-json = "1"` — near-miss (distance 1) flagged by
  the proxy.
- **COUNTEREXAMPLE** (good): `serde_json = "1"` — exact match.
- **VERIFICATION**: proxy output above; `cargo info serde-json` returns the
  real `serde_json` crate (unified crate names map `-`→`_`, so this specific
  name is KNOWN to exist — but only after verification, not from memory).
- **SOURCE**: crates-io-api; cargo docs (name normalization, INFERRED).

## 5. Version specifiers: exact pins and `--locked` CI

- **RULE**: `^1.0` allows any 1.x; for `0.x` crates, `^0.2` does NOT include
  `0.3` but `0.2` alone means `^0.2`. Reproducibility comes from the
  `Cargo.lock` and `--locked` builds (CI), or from exact pins
  (`serde = "=1.0.229"`). Behavior-changing minor updates are a documented
  Rust API-drift vector (arxiv-2503-16922).
- **WHY AI GETS IT WRONG**: models write bare `"1"` / `"0.1"` specs and
  assume the resolver freezes versions (it does not without a checked-in
  lockfile + `--locked`).
- **CORRECT REASONING**: for applications, commit `Cargo.lock` and build with
  `cargo build --locked`; for libraries, keep a minimal but explicit upper
  bound. Add `cargo update --precise` for deliberate bumps.
- **EXAMPLE** (bad): `tokio = "1"` in an app without a lockfile — next build
  may pull a different `1.x` with changed behavior.
- **COUNTEREXAMPLE** (good): `serde = "=1.0.229"` (exact) or `tokio = "1.48"`
  plus committed lockfile and `--locked` CI.
- **VERIFICATION**: minimal-versions testing (`cargo +nightly update -Z
  minimal-versions`) is documented target verification — nightly is installed
  but the flag is not exercised here (UNVERIFIED locally).
- **SOURCE**: arxiv-2503-16922 (behavioral drift); cargo-semver-checks.

## 6. Standing gates: cargo-deny and cargo audit

- **RULE**: `cargo deny check` (advisories from OSV, banned crates, license
  allowlists) and `cargo audit` (RustSec advisory DB) are the standing
  gates. `cargo-deny` also supports `bans` for known-malicious crate names
  and exact-version pins.
- **WHY AI GETS IT WRONG**: agents add `cargo-deny` to the config or skip it
  entirely; the value is in running it in CI on every change, not in the
  config file.
- **CORRECT REASONING**: `deny.toml` with `advisories`, `bans`, `licenses`
  sections, wired into CI as a hard gate; `cargo audit` on the lockfile.
- **EXAMPLE** (bad): no `deny.toml`, or `cargo deny check` never run.
- **COUNTEREXAMPLE** (good): CI step `cargo deny check` that fails the build
  on any vulnerability advisory or unlicensed dependency.
- **VERIFICATION**: neither binary is installed locally — documented target
  verification (UNVERIFIED locally).
- **SOURCE**: cargo-deny; arxiv-2505-05057 (dependency-aware hallucination
  mitigation context).

## Quick reference table

| Check | Command | Nonexistent name verdict (recorded) |
|---|---|---|
| Exact existence | `cargo info <name>` | `error: could not find ... in registry`, exit 101 |
| Last machine gate | `cargo add <name>` | `error: the crate ... could not be found`, exit 101 |
| Fuzzy keyword | `cargo search <name>` | returns unrelated crates (false positive) |
| API exact | `GET https://crates.io/api/v1/crates/<name>` | 404 |
| Offline proxy | `python examples/good/verify_crate_names.py` | Levenshtein-based flag |
| License/advisory | `cargo deny check` / `cargo audit` | clean report (target verification) |
