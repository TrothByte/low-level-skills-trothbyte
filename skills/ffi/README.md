# ffi — Skills

FFI is where one language's safety guarantees end. These skills cover cross-language boundaries: layout pinning, ownership transfer, error translation, and the no-unwind rule.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `ffi-boundary-cross-language` | Use when passing data or control across a language boundary — C to Rust, C++ to C, Zig to C, Rust to WASM — where layout, ownership, error translation, and unwind semantics must be pinned. Teaches the shared rules: repr(C) layout, who frees/drops, panic/unwind prohibition, and opaque handles. | source-backed | `skills/ffi/ffi-boundary-cross-language` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
