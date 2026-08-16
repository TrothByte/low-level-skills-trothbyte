# llvm — Skills

LLVM is the reference compiler infrastructure.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `llvm-ir-reading` | Use when reading, reviewing, or debugging LLVM IR (.ll files or opt output) — understanding SSA form, GEP offsets, phi nodes, poison/undef/freeze semantics, opaque pointers, function attributes, or explaining why an optimization pass changed the IR. Covers clang -S -emit-llvm and opt workflows. | common | researched | `skills/llvm/llvm-ir-reading` |
| `llvm-pass-writing` | Use when writing, reviewing, or testing LLVM optimization passes in C++ — New Pass Manager structure, PassInfoMixin run methods, PreservedAnalyses correctness, analysis invalidation, IRBuilder usage, SSA-safe IR mutation, opt -passes= integration, and lit/FileCheck testing. | common | researched | `skills/llvm/llvm-pass-writing` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
