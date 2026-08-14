# Evaluation — rust-unsafe-reasoning

Skill: `skills/rust/rust-unsafe-reasoning`. Stability target: `evaluated`.

## Historical CVE evals

| CVE | Class | Detect | Fix | Verify |
|---|---|---|---|---|
| CVE-2020-36432 | uninitialized memory drop | unsafe `fill_with` writes `*ptr = value` into a raw region that is later read/dropped as initialized | minimal Rust fix: full initialization before use (or zero the buffer) | Miri |
| CVE-2021-32714 | usize overflow | length arithmetic overflows `usize`, producing a length mismatch / OOB | checked arithmetic | Miri + fuzz |

Details as recorded in `registry/evals.yaml` (evals.yaml is the source for these rows;
CVE specifics not re-derived locally — INFERRED where not reproducible on this machine).

Each eval: DETECT (find the unsafe violation) → EXPLAIN (name the rule in
`references/unsafe-semantics.md`) → FIX (remove UB at source) → VERIFY (Miri clean).

## Synthetic evals

- **easy/positive**: correct `MaybeUninit` + full init + `assume_init` — must NOT flag
  (see `examples/good/maybe_uninit.rs`).
- **easy/negative**: `Vec::set_len(4)` without writing elements, then read — must detect
  uninitialized-memory read (see `examples/bad/set_len_uninit.rs`).
- **medium/positive**: `u32` → `[u8; 4]` transmute with a size assertion — must NOT flag
  (see `examples/good/transmute.rs`).
- **medium/negative**: `u32` → `[u8; 3]` transmute — must name E0512
  (see `examples/bad/transmute_wrong_size.rs`).
- **hard/negative**: raw pointer cached across a `Vec::push` (retag/realloc), then
  dereferenced — Miri flags; detect stale-pointer misuse.
- **hard/negative**: `Box::into_raw` + early `Box::from_raw` + later deref — dangling/UAF,
  Miri flags (see `examples/bad/dangling_box_pointer.rs`).
- **adversarial**: two raw pointers derived from two `&mut` to the same location, both
  written — compiles and runs; Miri flags under Stacked Borrows (model-dependent under
  Tree Borrows: mark uncertainty, do not overclaim).
- **adversarial**: `unsafe impl Send` on an `Rc<Cell<u32>>` wrapper used from two threads —
  compiles, naive tests pass, but it is a data race (see
  `examples/bad/unsafe_send_sync.rs`).

## False-positive evals (correct code must NOT be flagged)

- FP-03 (registry evals.yaml): correct unsafe with proper SAFETY comments — not flagged.
- Correct `ptr::read`/`ptr::write` swap with paired moves — not flagged
  (see `examples/good/ptr_read_write.rs`).
- Correct scoped raw-pointer fill derived from a single `&mut` — not flagged
  (see `examples/good/raw_ptr_scoped.rs`).
- Correct `Box::from_raw`/`Box::into_raw` round-trip — not flagged
  (see `examples/good/box_from_raw.rs`).

## Verification commands

Locally executed (rustc 1.97.1, Windows):

```
rustc --edition 2021 examples/good/maybe_uninit.rs -o out_g1.exe && ./out_g1.exe
rustc --edition 2021 examples/good/transmute.rs       -o out_g2.exe && ./out_g2.exe
rustc --edition 2021 examples/good/raw_ptr_scoped.rs  -o out_g3.exe && ./out_g3.exe
rustc --edition 2021 examples/good/ptr_read_write.rs  -o out_g4.exe && ./out_g4.exe
rustc --edition 2021 examples/good/box_from_raw.rs    -o out_g5.exe && ./out_g5.exe
rustc --edition 2021 examples/bad/transmute_wrong_size.rs -o out_b1.exe   # expect E0512
rustc --edition 2021 examples/bad/two_mut_aliases.rs      -o out_b3.exe   # expect E0499
```

Bad files that DO compile (`dangling_box_pointer.rs`, `set_len_uninit.rs`,
`unsafe_send_sync.rs`) are UB by spec; verify them only with Miri, never by "it printed
something reasonable".

Target verification (Miri):

```
rustup component add --toolchain nightly miri
cargo +nightly miri run
cargo +nightly miri test
cargo +nightly miri run -Zmiri-strict-provenance   # provenance-losing casts
cargo +nightly miri run -Zmiri-tree-borrows        # compare aliasing models
```

## Miri / nightly availability (local, 2026-08-14)

- rustc 1.97.1 stable and cargo 1.97.1 are on PATH — used for the local compile/run gates.
- nightly `1.99.0-nightly` is installed via rustup, but the `miri` component is NOT
  installed: `cargo +nightly miri --version` → "cargo-miri.exe is not installed for the
  toolchain 'nightly-x86_64-pc-windows-msvc'".
- Until `rustup component add --toolchain nightly miri` runs, Miri commands are the
  documented target verification, not locally executed. The bad examples that compile were
  classified UB-by-spec from the Rust Reference list, not from a Miri run (marked INFERRED
  where the verdict depends on the aliasing model).

## Scoring (for routing eval)

- precision: every flagged construct maps to a real rule in `references/unsafe-semantics.md`.
- recall: every bad example detected (compile error recorded, or UB-by-spec with the reason).
- FP-rate: good examples produce zero flags.
