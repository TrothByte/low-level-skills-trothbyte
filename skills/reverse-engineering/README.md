# reverse-engineering — Skills

Reverse engineering covers Go and Rust binaries with their special metadata (pclntab, panic strings) and automated protocol reverse engineering with a verify-as-gate.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `auto-re-protocols-beyond-can` | Use when reverse-engineering a binary protocol from captured bytes — UART/serial frames, industrial fieldbus or CAN-style traffic — where the wire format is unknown and must be recovered from evidence. Applies the deterministic pipeline: capture, survey, correlate, bit/field search, schema, verify-as-gate, instead of guessing field boundaries. | source-backed | `skills/reverse-engineering/auto-re-protocols-beyond-can` |
| `go-rust-re` | Use when analyzing or reverse-engineering Go or Rust binaries — recovering function names from .gopclntab, decoding mangled Rust symbols (_ZN/_R), finding panic strings and core::fmt literals in .rodata/.rdata, distinguishing Rust from C/C++, or triaging stripped Go/Rust executables with GoReSym, redress, objdump, gdb, and Delve. | source-backed | `skills/reverse-engineering/go-rust-re` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
