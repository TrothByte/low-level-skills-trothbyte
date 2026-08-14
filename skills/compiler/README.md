# compiler — Skills

The compiler is the agent's least-trusted and least-understood colleague. This domain explains how compilers interpret undefined behavior — the single most common cause of 'works at -O0, breaks at -O2'.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `compiler-ub-assumptions` | Use when diagnosing why C/C++ code behaves differently across optimization levels or compilers, when a bounds/null check "disappears" at -O2, when the optimizer reorders or elides code, or when explaining assumption-based optimization. Teaches how compilers exploit undefined behavior and how to prove the behavior with disassembly. | source-backed | `skills/compiler/compiler-ub-assumptions` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
