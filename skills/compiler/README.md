# compiler — Skills

The compiler is the agent's least-trusted and least-understood colleague.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `compiler-ub-assumptions` | Use when diagnosing why C/C++ code behaves differently across optimization levels or compilers, when a bounds/null check "disappears" at -O2, when the optimizer reorders or elides code, or when explaining assumption-based optimization. Teaches how compilers exploit undefined behavior and how to prove the behavior with disassembly. | improved | source-backed | `skills/compiler/compiler-ub-assumptions` |
| `compiler-unstable-code-detection` | Use when code behaves differently across compilers or optimization levels, when sanitizers report nothing but behavior changes at -O2, or when reviewing generated code for optimization-sensitive bugs. Teaches differential testing between compilers and optimization levels to find undefined behavior that sanitizers miss. | unique | source-backed | `skills/compiler/compiler-unstable-code-detection` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
