# rust — Skills

Rust makes memory safety the default — unsafe blocks move the burden onto the author.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `rust-api-evolution-and-drift` | Use when writing Rust code that must compile against a specific toolchain: API signatures drift between Rust versions and editions, methods get deprecated or change meaning. Prevents generated code that references removed or renamed APIs, edition-2024 unsafe changes, and stale deprecations. | unique | source-backed | `skills/rust/rust-api-evolution-and-drift` |
| `rust-crypto-primitives-safety` | Use when writing, reviewing, or auditing Rust cryptography: selecting AEAD primitives, nonces, key handling, or hand-rolled ciphers. Prevents nonce reuse, invented algorithms, and API hallucination — 57% of LLM-compiled crypto is vulnerable. | unique | source-backed | `skills/rust/rust-crypto-primitives-safety` |
| `rust-dependency-supply-chain` | Use when choosing or adding a dependency: crate names are hallucinated at 5.2-21.7%, typosquats and near-misses abound. Teaches exact-name verification (cargo info, crates.io API), Levenshtein near-miss checks, cargo-deny/audit, and minimal version pinning. | unique | source-backed | `skills/rust/rust-dependency-supply-chain` |
| `rust-ffi-boundary` | Use when writing or reviewing Rust FFI code — repr(C) layout, enum discriminants, CString/CStr and Box::into_raw ownership, extern "C" callbacks, opaque handles, or panic/unwind at the boundary. Teaches Rust-specific rules for safe C interop and how to verify layout and ownership on both sides. | improved | source-backed | `skills/rust/rust-ffi-boundary` |
| `rust-panic-safety` | Use when writing, reviewing, or debugging Rust code where a panic may be reachable — unwrap/expect on untrusted input, unwinding through extern "C" exports, catch_unwind boundaries, RefCell/Mutex poisoning, Drop during unwind, or panic=abort vs panic=unwind. Teaches panic reachability and unwind discipline as a single reasoning model. | improved | source-backed | `skills/rust/rust-panic-safety` |
| `rust-unsafe-reasoning` | Use when writing, reviewing, or debugging Rust code that uses unsafe blocks — raw pointers, transmute, MaybeUninit, Box::from_raw/into_raw, unsafe impl Send/Sync, or FFI-adjacent code — to reason about validity invariants, aliasing, and pointer provenance, and to detect undefined behavior that compiles and runs but is wrong. | improved | source-backed | `skills/rust/rust-unsafe-reasoning` |
| `rust-unsafe-safety-contract-verification` | Use when auditing unsafe Rust: every unsafe block must carry a SAFETY comment whose preconditions are real and encoded in the type system. Prevents fabricated contracts — like Bun PathString.rs (2026) — that claim a 'caller guarantees' invariant no type enforces, silently permitting use-after-free. | improved | source-backed | `skills/rust/rust-unsafe-safety-contract-verification` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
