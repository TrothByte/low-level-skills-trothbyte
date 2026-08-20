# safety — Skills

Functional-safety and deterministic-systems skills: MISRA C/C++ compliance and hard real-time determinism for automotive, aerospace, and medical code.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `hard-real-time-determinism` | Use when writing or reviewing hard real-time code — bare-metal or RTOS tasks with deadlines, WCET analysis, or safety-critical control loops. Enforces no dynamic allocation, no recursion, no exceptions, bounded loops, and deterministic scheduling. | unique | researched | `skills/safety/hard-real-time-determinism` |
| `misra-c-compliance` | Use when writing or reviewing C/C++ for automotive, aerospace, medical or other safety-critical systems that must comply with MISRA C:2012 or MISRA C++:2023. Covers the Top-k most-violated rules LLMs break, essential-type casts, boolean control expressions, and verification with static analyzers. | unique | source-backed | `skills/safety/misra-c-compliance` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
