# debugging — Skills

Low-level engineering skills for this domain.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `debugging-crash-triage-discipline` | Use when debugging a crash, segfault, or access violation in C/C++ or low-level code. Guides reliable reproduction, capturing backtraces and registers, deciding whether the crash site is the bug or corruption surfaces later, and verifying the fix. Prevents merry-go-round hypothesis churn and false "fixed" verdicts. | source-backed | `skills/debugging/debugging-crash-triage-discipline` |
| `debugging-instrumentation-over-reasoning` | Use when a bug resists a debugger and pure reasoning: repeated full runs, truncated traces, or fixes aimed at the wrong object. Replace speculation with deterministic append-only file instrumentation, entry logging, checkpoints, counters, and before/after values, and keep the log as evidence. | source-backed | `skills/debugging/debugging-instrumentation-over-reasoning` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
