# rust — Skills

Rust makes memory safety the default, but unsafe blocks move the burden onto the author. These skills teach unsafe semantics (aliasing, validity, provenance), FFI boundaries, and panic safety — the three places where Rust code can still be wrong.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `rust-ffi-boundary` | Use when writing or reviewing Rust FFI code — repr(C) layout, enum discriminants, CString/CStr and Box::into_raw ownership, extern "C" callbacks, opaque handles, or panic/unwind at the boundary. Teaches Rust-specific rules for safe C interop and how to verify layout and ownership on both sides. | source-backed | `skills/rust/rust-ffi-boundary` |
| `rust-panic-safety` | Use when writing, reviewing, or debugging Rust code where a panic may be reachable — unwrap/expect on untrusted input, unwinding through extern "C" exports, catch_unwind boundaries, RefCell/Mutex poisoning, Drop during unwind, or panic=abort vs panic=unwind. Teaches panic reachability and unwind discipline as a single reasoning model. | source-backed | `skills/rust/rust-panic-safety` |
| `rust-unsafe-reasoning` | Use when writing, reviewing, or debugging Rust code that uses unsafe blocks — raw pointers, transmute, MaybeUninit, Box::from_raw/into_raw, unsafe impl Send/Sync, or FFI-adjacent code — to reason about validity invariants, aliasing, and pointer provenance, and to detect undefined behavior that compiles and runs but is wrong. | source-backed | `skills/rust/rust-unsafe-reasoning` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
