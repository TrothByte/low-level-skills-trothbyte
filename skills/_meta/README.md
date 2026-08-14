# _meta — Skills

Meta-skills govern agent behavior: routing to the minimal skill set, evidence discipline (KNOWN/INFERRED/UNVERIFIED), verification gates, surfacing assumptions, rejecting rationalizations, and honest completion. Also here: the flagship cross-layer skills safe-low-level-from-scratch, zeroize-constant-time, and wasm-runtime-from-scratch.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `meta-assumptions` | Use when code correctness depends on implicit assumptions — compiler, ABI, platform, memory model, optimization level, or endianness. Forces surfacing and documenting every assumption before concluding. | researched | `skills/_meta/meta-assumptions` |
| `meta-completion` | Use before declaring a low-level task complete. Enforces honest completion criteria: verifiable success, no hidden partial results, updated state files, and explicit uncertainty. | researched | `skills/_meta/meta-completion` |
| `meta-evidence` | Use whenever making a normative or factual claim about C/C++/Rust/asm/ABI/UB/compiler behavior. Enforces the KNOWN / INFERRED / UNVERIFIED classification and requires source-backed evidence for strong claims. | researched | `skills/_meta/meta-evidence` |
| `meta-rationalizations` | Use during code review or self-review to catch and reject rationalizations that excuse unsafe or incorrect low-level code. Contains the "Rationalizations to Reject" list derived from trailofbits and failure modes B1-B22. | researched | `skills/_meta/meta-rationalizations` |
| `meta-routing` | Use at the start of any low-level task to choose the minimal relevant skill set. Prevents "load everything" behavior, enables dependency expansion, and routes to the correct skill from the registry. | researched | `skills/_meta/meta-routing` |
| `meta-verification` | Use before concluding that low-level code is correct or that a bug is found. Enforces executable verification (compile+run, sanitizers, asm inspection, debugger) instead of "it compiles" or "tests pass". | researched | `skills/_meta/meta-verification` |
| `safe-low-level-from-scratch` | Use when writing NEW low-level code (C/C++/Rust/asm) from scratch that must be memory-safe and correct across optimization levels and platforms. Provides the positive writing process integrating UB semantics, layout/alignment, ownership, atomics, and FFI, with verification gates at each step. | source-backed | `skills/_meta/safe-low-level-from-scratch` |
| `wasm-runtime-from-scratch` | Use when writing, reviewing, or debugging a WebAssembly runtime, interpreter, loader, or validator in C — module binary parsing, validation, linear memory bounds, tables and call_indirect, traps vs undefined behavior, memory.grow, and host function imports. | source-backed | `skills/_meta/wasm-runtime-from-scratch` |
| `zeroize-constant-time` | Use when writing or reviewing code handling secrets (keys, passwords, nonces) that must be zeroized or compared in constant time. Triggers on memset-before-return, secret-dependent branches or indexing, memcmp on secrets, and claims that a secret is cleared. Teaches volatile-sink zeroization, explicit_bzero, ct_memcmp, and asm verification. | source-backed | `skills/_meta/zeroize-constant-time` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
