# debugging — Skills

Debugging is instrumentation over reasoning — measure before you conclude.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `debugging-crash-triage-discipline` | Use when debugging a crash, segfault, or access violation in C/C++ or low-level code. Guides reliable reproduction, capturing backtraces and registers, deciding whether the crash site is the bug or corruption surfaces later, and verifying the fix. Prevents merry-go-round hypothesis churn and false "fixed" verdicts. | unique | source-backed | `skills/debugging/debugging-crash-triage-discipline` |
| `debugging-instrumentation-over-reasoning` | Use when a bug resists a debugger and pure reasoning: repeated full runs, truncated traces, or fixes aimed at the wrong object. Replace speculation with deterministic append-only file instrumentation, entry logging, checkpoints, counters, and before/after values, and keep the log as evidence. | unique | source-backed | `skills/debugging/debugging-instrumentation-over-reasoning` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
