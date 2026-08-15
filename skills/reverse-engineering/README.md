# reverse-engineering — Skills

Reverse engineering covers Go and Rust binaries with their special metadata (pclntab, panic strings) and automated protocol reverse engineering with a verify-as-gate.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `auto-re-protocols-beyond-can` | Use when reverse-engineering a binary protocol from captured bytes — UART/serial frames, industrial fieldbus or CAN-style traffic — where the wire format is unknown and must be recovered from evidence. Applies the deterministic pipeline: capture, survey, correlate, bit/field search, schema, verify-as-gate, instead of guessing field boundaries. | source-backed | `skills/reverse-engineering/auto-re-protocols-beyond-can` |
| `go-rust-re` | Use when analyzing or reverse-engineering Go or Rust binaries — recovering function names from .gopclntab, decoding mangled Rust symbols (_ZN/_R), finding panic strings and core::fmt literals in .rodata/.rdata, distinguishing Rust from C/C++, or triaging stripped Go/Rust executables with GoReSym, redress, objdump, gdb, and Delve. | source-backed | `skills/reverse-engineering/go-rust-re` |
| `reverse-engineering-can-signal-extraction` | Use when extracting CAN signal layouts from a DBC file or reverse-engineering unknown CAN signals: DBC bit numbering (Intel vs Motorola sawtooth), scale/offset, little/big endian, and the parked-vs-moving gate (SWEEP vs HOLDS) before certifying a signal as speed-like. | researched | `skills/reverse-engineering/reverse-engineering-can-signal-extraction` |
| `reverse-engineering-ghidra-agent-automation` | Use when an agent drives Ghidra (PyGhidra daemon/RPC or headless) in a triage-annotate-type-recovery-diff loop, or when an automated verdict about a binary is being trusted. Prevents confident-but-wrong identity claims, rebase errors ($0000 vs $A000), and unverified byte patches. | researched | `skills/reverse-engineering/reverse-engineering-ghidra-agent-automation` |
| `reverse-engineering-shellcode-analysis` | Use when reading, verifying, or extracting information from raw shellcode bytes (x86-64 Linux focus): syscall numbers, IP/port constants, instruction inventory. Prevents invented instructions, wrong syscall tables, and byte-order errors in IP/port extraction. | source-backed | `skills/reverse-engineering/reverse-engineering-shellcode-analysis` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
